/*-
 *   BSD LICENSE
 *
 *   Copyright 2017 6WIND S.A.
 *   Copyright 2017 Mellanox.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of 6WIND S.A. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdint.h>

#include <rte_debug.h>
#include <rte_atomic.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_flow.h>

#include "failsafe_private.h"

static struct rte_eth_dev_info default_infos = {
	/* Max possible number of elements */
	.max_rx_pktlen = UINT32_MAX,
	.max_rx_queues = RTE_MAX_QUEUES_PER_PORT,
	.max_tx_queues = RTE_MAX_QUEUES_PER_PORT,
	.max_mac_addrs = FAILSAFE_MAX_ETHADDR,
	.max_hash_mac_addrs = UINT32_MAX,
	.max_vfs = UINT16_MAX,
	.max_vmdq_pools = UINT16_MAX,
	.rx_desc_lim = {
		.nb_max = UINT16_MAX,
		.nb_min = 0,
		.nb_align = 1,
		.nb_seg_max = UINT16_MAX,
		.nb_mtu_seg_max = UINT16_MAX,
	},
	.tx_desc_lim = {
		.nb_max = UINT16_MAX,
		.nb_min = 0,
		.nb_align = 1,
		.nb_seg_max = UINT16_MAX,
		.nb_mtu_seg_max = UINT16_MAX,
	},
	/*
	 * Set of capabilities that can be verified upon
	 * configuring a sub-device.
	 */
	.rx_offload_capa =
		DEV_RX_OFFLOAD_VLAN_STRIP |
		DEV_RX_OFFLOAD_QINQ_STRIP |
		DEV_RX_OFFLOAD_IPV4_CKSUM |
		DEV_RX_OFFLOAD_UDP_CKSUM |
		DEV_RX_OFFLOAD_TCP_CKSUM |
		DEV_RX_OFFLOAD_TCP_LRO,
	.tx_offload_capa = 0x0,
	.flow_type_rss_offloads = 0x0,
};

/**
 * Check whether a specific offloading capability
 * is supported by a sub_device.
 *
 * @return
 *   0: all requested capabilities are supported by the sub_device
 *   positive value: This flag at least is not supported by the sub_device
 */
static int
fs_port_offload_validate(struct rte_eth_dev *dev,
			 struct sub_device *sdev)
{
	struct rte_eth_dev_info infos = {0};
	struct rte_eth_conf *cf;
	uint32_t cap;

	cf = &dev->data->dev_conf;
	SUBOPS(sdev, dev_infos_get)(ETH(sdev), &infos);
	/* RX capabilities */
	cap = infos.rx_offload_capa;
	if (cf->rxmode.hw_vlan_strip &&
	    ((cap & DEV_RX_OFFLOAD_VLAN_STRIP) == 0)) {
		WARN("VLAN stripping offload requested but not supported by sub_device %d",
		      SUB_ID(sdev));
		return DEV_RX_OFFLOAD_VLAN_STRIP;
	}
	if (cf->rxmode.hw_ip_checksum &&
	    ((cap & (DEV_RX_OFFLOAD_IPV4_CKSUM |
		     DEV_RX_OFFLOAD_UDP_CKSUM |
		     DEV_RX_OFFLOAD_TCP_CKSUM)) !=
	     (DEV_RX_OFFLOAD_IPV4_CKSUM |
	      DEV_RX_OFFLOAD_UDP_CKSUM |
	      DEV_RX_OFFLOAD_TCP_CKSUM))) {
		WARN("IP checksum offload requested but not supported by sub_device %d",
		      SUB_ID(sdev));
		return DEV_RX_OFFLOAD_IPV4_CKSUM |
		       DEV_RX_OFFLOAD_UDP_CKSUM |
		       DEV_RX_OFFLOAD_TCP_CKSUM;
	}
	if (cf->rxmode.enable_lro &&
	    ((cap & DEV_RX_OFFLOAD_TCP_LRO) == 0)) {
		WARN("TCP LRO offload requested but not supported by sub_device %d",
		      SUB_ID(sdev));
		return DEV_RX_OFFLOAD_TCP_LRO;
	}
	if (cf->rxmode.hw_vlan_extend &&
	    ((cap & DEV_RX_OFFLOAD_QINQ_STRIP) == 0)) {
		WARN("Stacked VLAN stripping offload requested but not supported by sub_device %d",
		      SUB_ID(sdev));
		return DEV_RX_OFFLOAD_QINQ_STRIP;
	}
	/* TX capabilities */
	/* Nothing to do, no tx capa supported */
	return 0;
}

/*
 * Disable the dev_conf flag related to an offload capability flag
 * within an ethdev configuration.
 */
static int
fs_port_disable_offload(struct rte_eth_conf *cf,
			uint32_t ol_cap)
{
	switch (ol_cap) {
	case DEV_RX_OFFLOAD_VLAN_STRIP:
		INFO("Disabling VLAN stripping offload");
		cf->rxmode.hw_vlan_strip = 0;
		break;
	case DEV_RX_OFFLOAD_IPV4_CKSUM:
	case DEV_RX_OFFLOAD_UDP_CKSUM:
	case DEV_RX_OFFLOAD_TCP_CKSUM:
	case (DEV_RX_OFFLOAD_IPV4_CKSUM |
	      DEV_RX_OFFLOAD_UDP_CKSUM |
	      DEV_RX_OFFLOAD_TCP_CKSUM):
		INFO("Disabling IP checksum offload");
		cf->rxmode.hw_ip_checksum = 0;
		break;
	case DEV_RX_OFFLOAD_TCP_LRO:
		INFO("Disabling TCP LRO offload");
		cf->rxmode.enable_lro = 0;
		break;
	case DEV_RX_OFFLOAD_QINQ_STRIP:
		INFO("Disabling stacked VLAN stripping offload");
		cf->rxmode.hw_vlan_extend = 0;
		break;
	default:
		DEBUG("Unable to disable offload capability: %" PRIx32,
		      ol_cap);
		return -1;
	}
	return 0;
}

static int
fs_dev_configure(struct rte_eth_dev *dev)
{
	struct sub_device *sdev;
	uint8_t i;
	int capa_flag;
	int ret;

	FOREACH_SUBDEV(sdev, i, dev) {
		if (sdev->state != DEV_PROBED)
			continue;
		DEBUG("Checking capabilities for sub_device %d", i);
		while ((capa_flag = fs_port_offload_validate(dev, sdev))) {
			/*
			 * Refuse to change configuration if multiple devices
			 * are present and we already have configured at least
			 * some of them.
			 */
			if (PRIV(dev)->state >= DEV_ACTIVE &&
			    PRIV(dev)->subs_tail > 1) {
				ERROR("device already configured, cannot fix live configuration");
				return -1;
			}
			ret = fs_port_disable_offload(&dev->data->dev_conf,
						      capa_flag);
			if (ret) {
				ERROR("Unable to disable offload capability");
				return ret;
			}
		}
	}
	FOREACH_SUBDEV(sdev, i, dev) {
		int rmv_interrupt = 0;
		int lsc_interrupt = 0;
		int lsc_enabled;

		if (sdev->state != DEV_PROBED)
			continue;

		rmv_interrupt = ETH(sdev)->data->dev_flags &
				RTE_ETH_DEV_INTR_RMV;
		if (rmv_interrupt) {
			DEBUG("Enabling RMV interrupts for sub_device %d", i);
			dev->data->dev_conf.intr_conf.rmv = 1;
		} else {
			DEBUG("sub_device %d does not support RMV event", i);
		}
		lsc_enabled = dev->data->dev_conf.intr_conf.lsc;
		lsc_interrupt = lsc_enabled &&
				(ETH(sdev)->data->dev_flags &
				 RTE_ETH_DEV_INTR_LSC);
		if (lsc_interrupt) {
			DEBUG("Enabling LSC interrupts for sub_device %d", i);
			dev->data->dev_conf.intr_conf.lsc = 1;
		} else if (lsc_enabled && !lsc_interrupt) {
			DEBUG("Disabling LSC interrupts for sub_device %d", i);
			dev->data->dev_conf.intr_conf.lsc = 0;
		}
		DEBUG("Configuring sub-device %d", i);
		sdev->remove = 0;
		ret = rte_eth_dev_configure(PORT_ID(sdev),
					dev->data->nb_rx_queues,
					dev->data->nb_tx_queues,
					&dev->data->dev_conf);
		if (ret) {
			ERROR("Could not configure sub_device %d", i);
			return ret;
		}
		if (rmv_interrupt) {
			ret = rte_eth_dev_callback_register(PORT_ID(sdev),
					RTE_ETH_EVENT_INTR_RMV,
					failsafe_eth_rmv_event_callback,
					sdev);
			if (ret)
				WARN("Failed to register RMV callback for sub_device %d",
				     SUB_ID(sdev));
		}
		dev->data->dev_conf.intr_conf.rmv = 0;
		if (lsc_interrupt) {
			ret = rte_eth_dev_callback_register(PORT_ID(sdev),
						RTE_ETH_EVENT_INTR_LSC,
						failsafe_eth_lsc_event_callback,
						dev);
			if (ret)
				WARN("Failed to register LSC callback for sub_device %d",
				     SUB_ID(sdev));
		}
		dev->data->dev_conf.intr_conf.lsc = lsc_enabled;
		sdev->state = DEV_ACTIVE;
	}
	if (PRIV(dev)->state < DEV_ACTIVE)
		PRIV(dev)->state = DEV_ACTIVE;
	return 0;
}

static int
fs_dev_start(struct rte_eth_dev *dev)
{
	struct sub_device *sdev;
	uint8_t i;
	int ret;

	FOREACH_SUBDEV(sdev, i, dev) {
		if (sdev->state != DEV_ACTIVE)
			continue;
		DEBUG("Starting sub_device %d", i);
		ret = rte_eth_dev_start(PORT_ID(sdev));
		if (ret)
			return ret;
		sdev->state = DEV_STARTED;
	}
	if (PRIV(dev)->state < DEV_STARTED)
		PRIV(dev)->state = DEV_STARTED;
	fs_switch_dev(dev, NULL);
	return 0;
}

static void
fs_dev_stop(struct rte_eth_dev *dev)
{
	struct sub_device *sdev;
	uint8_t i;

	PRIV(dev)->state = DEV_STARTED - 1;
	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_STARTED) {
		rte_eth_dev_stop(PORT_ID(sdev));
		sdev->state = DEV_STARTED - 1;
	}
}

static int
fs_dev_set_link_up(struct rte_eth_dev *dev)
{
	struct sub_device *sdev;
	uint8_t i;
	int ret;

	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE) {
		DEBUG("Calling rte_eth_dev_set_link_up on sub_device %d", i);
		ret = rte_eth_dev_set_link_up(PORT_ID(sdev));
		if (ret) {
			ERROR("Operation rte_eth_dev_set_link_up failed for sub_device %d"
			      " with error %d", i, ret);
			return ret;
		}
	}
	return 0;
}

static int
fs_dev_set_link_down(struct rte_eth_dev *dev)
{
	struct sub_device *sdev;
	uint8_t i;
	int ret;

	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE) {
		DEBUG("Calling rte_eth_dev_set_link_down on sub_device %d", i);
		ret = rte_eth_dev_set_link_down(PORT_ID(sdev));
		if (ret) {
			ERROR("Operation rte_eth_dev_set_link_down failed for sub_device %d"
			      " with error %d", i, ret);
			return ret;
		}
	}
	return 0;
}

static void fs_dev_free_queues(struct rte_eth_dev *dev);
static void
fs_dev_close(struct rte_eth_dev *dev)
{
	struct sub_device *sdev;
	uint8_t i;

	failsafe_hotplug_alarm_cancel(dev);
	if (PRIV(dev)->state == DEV_STARTED)
		dev->dev_ops->dev_stop(dev);
	PRIV(dev)->state = DEV_ACTIVE - 1;
	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE) {
		DEBUG("Closing sub_device %d", i);
		rte_eth_dev_close(PORT_ID(sdev));
		sdev->state = DEV_ACTIVE - 1;
	}
	fs_dev_free_queues(dev);
}

static void
fs_rx_queue_release(void *queue)
{
	struct rte_eth_dev *dev;
	struct sub_device *sdev;
	uint8_t i;
	struct rxq *rxq;

	if (queue == NULL)
		return;
	rxq = queue;
	dev = rxq->priv->dev;
	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE)
		SUBOPS(sdev, rx_queue_release)
			(ETH(sdev)->data->rx_queues[rxq->qid]);
	dev->data->rx_queues[rxq->qid] = NULL;
	rte_free(rxq);
}

static int
fs_rx_queue_setup(struct rte_eth_dev *dev,
		uint16_t rx_queue_id,
		uint16_t nb_rx_desc,
		unsigned int socket_id,
		const struct rte_eth_rxconf *rx_conf,
		struct rte_mempool *mb_pool)
{
	struct sub_device *sdev;
	struct rxq *rxq;
	uint8_t i;
	int ret;

	rxq = dev->data->rx_queues[rx_queue_id];
	if (rxq != NULL) {
		fs_rx_queue_release(rxq);
		dev->data->rx_queues[rx_queue_id] = NULL;
	}
	rxq = rte_zmalloc(NULL,
			  sizeof(*rxq) +
			  sizeof(rte_atomic64_t) * PRIV(dev)->subs_tail,
			  RTE_CACHE_LINE_SIZE);
	if (rxq == NULL)
		return -ENOMEM;
	FOREACH_SUBDEV(sdev, i, dev)
		rte_atomic64_init(&rxq->refcnt[i]);
	rxq->qid = rx_queue_id;
	rxq->socket_id = socket_id;
	rxq->info.mp = mb_pool;
	rxq->info.conf = *rx_conf;
	rxq->info.nb_desc = nb_rx_desc;
	rxq->priv = PRIV(dev);
	dev->data->rx_queues[rx_queue_id] = rxq;
	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE) {
		ret = rte_eth_rx_queue_setup(PORT_ID(sdev),
				rx_queue_id,
				nb_rx_desc, socket_id,
				rx_conf, mb_pool);
		if (ret) {
			ERROR("RX queue setup failed for sub_device %d", i);
			goto free_rxq;
		}
	}
	return 0;
free_rxq:
	fs_rx_queue_release(rxq);
	return ret;
}

static void
fs_tx_queue_release(void *queue)
{
	struct rte_eth_dev *dev;
	struct sub_device *sdev;
	uint8_t i;
	struct txq *txq;

	if (queue == NULL)
		return;
	txq = queue;
	dev = txq->priv->dev;
	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE)
		SUBOPS(sdev, tx_queue_release)
			(ETH(sdev)->data->tx_queues[txq->qid]);
	dev->data->tx_queues[txq->qid] = NULL;
	rte_free(txq);
}

static int
fs_tx_queue_setup(struct rte_eth_dev *dev,
		uint16_t tx_queue_id,
		uint16_t nb_tx_desc,
		unsigned int socket_id,
		const struct rte_eth_txconf *tx_conf)
{
	struct sub_device *sdev;
	struct txq *txq;
	uint8_t i;
	int ret;

	txq = dev->data->tx_queues[tx_queue_id];
	if (txq != NULL) {
		fs_tx_queue_release(txq);
		dev->data->tx_queues[tx_queue_id] = NULL;
	}
	txq = rte_zmalloc("ethdev TX queue",
			  sizeof(*txq) +
			  sizeof(rte_atomic64_t) * PRIV(dev)->subs_tail,
			  RTE_CACHE_LINE_SIZE);
	if (txq == NULL)
		return -ENOMEM;
	FOREACH_SUBDEV(sdev, i, dev)
		rte_atomic64_init(&txq->refcnt[i]);
	txq->qid = tx_queue_id;
	txq->socket_id = socket_id;
	txq->info.conf = *tx_conf;
	txq->info.nb_desc = nb_tx_desc;
	txq->priv = PRIV(dev);
	dev->data->tx_queues[tx_queue_id] = txq;
	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE) {
		ret = rte_eth_tx_queue_setup(PORT_ID(sdev),
				tx_queue_id,
				nb_tx_desc, socket_id,
				tx_conf);
		if (ret) {
			ERROR("TX queue setup failed for sub_device %d", i);
			goto free_txq;
		}
	}
	return 0;
free_txq:
	fs_tx_queue_release(txq);
	return ret;
}

static void
fs_dev_free_queues(struct rte_eth_dev *dev)
{
	uint16_t i;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		fs_rx_queue_release(dev->data->rx_queues[i]);
		dev->data->rx_queues[i] = NULL;
	}
	dev->data->nb_rx_queues = 0;
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		fs_tx_queue_release(dev->data->tx_queues[i]);
		dev->data->tx_queues[i] = NULL;
	}
	dev->data->nb_tx_queues = 0;
}

static void
fs_promiscuous_enable(struct rte_eth_dev *dev)
{
	struct sub_device *sdev;
	uint8_t i;

	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE)
		rte_eth_promiscuous_enable(PORT_ID(sdev));
}

static void
fs_promiscuous_disable(struct rte_eth_dev *dev)
{
	struct sub_device *sdev;
	uint8_t i;

	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE)
		rte_eth_promiscuous_disable(PORT_ID(sdev));
}

static void
fs_allmulticast_enable(struct rte_eth_dev *dev)
{
	struct sub_device *sdev;
	uint8_t i;

	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE)
		rte_eth_allmulticast_enable(PORT_ID(sdev));
}

static void
fs_allmulticast_disable(struct rte_eth_dev *dev)
{
	struct sub_device *sdev;
	uint8_t i;

	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE)
		rte_eth_allmulticast_disable(PORT_ID(sdev));
}

static int
fs_link_update(struct rte_eth_dev *dev,
		int wait_to_complete)
{
	struct sub_device *sdev;
	uint8_t i;
	int ret;

	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE) {
		DEBUG("Calling link_update on sub_device %d", i);
		ret = (SUBOPS(sdev, link_update))(ETH(sdev), wait_to_complete);
		if (ret && ret != -1) {
			ERROR("Link update failed for sub_device %d with error %d",
			      i, ret);
			return ret;
		}
	}
	if (TX_SUBDEV(dev)) {
		struct rte_eth_link *l1;
		struct rte_eth_link *l2;

		l1 = &dev->data->dev_link;
		l2 = &ETH(TX_SUBDEV(dev))->data->dev_link;
		if (memcmp(l1, l2, sizeof(*l1))) {
			*l1 = *l2;
			return 0;
		}
	}
	return -1;
}

static void
fs_stats_get(struct rte_eth_dev *dev,
	     struct rte_eth_stats *stats)
{
	if (TX_SUBDEV(dev) == NULL)
		return;
	rte_eth_stats_get(PORT_ID(TX_SUBDEV(dev)), stats);
}

static void
fs_stats_reset(struct rte_eth_dev *dev)
{
	struct sub_device *sdev;
	uint8_t i;

	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE)
		rte_eth_stats_reset(PORT_ID(sdev));
}

/**
 * Fail-safe dev_infos_get rules:
 *
 * No sub_device:
 *   Numerables:
 *      Use the maximum possible values for any field, so as not
 *      to impede any further configuration effort.
 *   Capabilities:
 *      Limits capabilities to those that are understood by the
 *      fail-safe PMD. This understanding stems from the fail-safe
 *      being capable of verifying that the related capability is
 *      expressed within the device configuration (struct rte_eth_conf).
 *
 * At least one probed sub_device:
 *   Numerables:
 *      Uses values from the active probed sub_device
 *      The rationale here is that if any sub_device is less capable
 *      (for example concerning the number of queues) than the active
 *      sub_device, then its subsequent configuration will fail.
 *      It is impossible to foresee this failure when the failing sub_device
 *      is supposed to be plugged-in later on, so the configuration process
 *      is the single point of failure and error reporting.
 *   Capabilities:
 *      Uses a logical AND of RX capabilities among
 *      all sub_devices and the default capabilities.
 *      Uses a logical AND of TX capabilities among
 *      the active probed sub_device and the default capabilities.
 *
 */
static void
fs_dev_infos_get(struct rte_eth_dev *dev,
		  struct rte_eth_dev_info *infos)
{
	struct sub_device *sdev;
	uint8_t i;

	sdev = TX_SUBDEV(dev);
	if (sdev == NULL) {
		DEBUG("No probed device, using default infos");
		rte_memcpy(&PRIV(dev)->infos, &default_infos,
			   sizeof(default_infos));
	} else {
		uint32_t rx_offload_capa;

		rx_offload_capa = default_infos.rx_offload_capa;
		FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_PROBED) {
			rte_eth_dev_info_get(PORT_ID(sdev),
					&PRIV(dev)->infos);
			rx_offload_capa &= PRIV(dev)->infos.rx_offload_capa;
		}
		sdev = TX_SUBDEV(dev);
		rte_eth_dev_info_get(PORT_ID(sdev), &PRIV(dev)->infos);
		PRIV(dev)->infos.rx_offload_capa = rx_offload_capa;
		PRIV(dev)->infos.tx_offload_capa &=
					default_infos.tx_offload_capa;
		PRIV(dev)->infos.flow_type_rss_offloads &=
					default_infos.flow_type_rss_offloads;
	}
	rte_memcpy(infos, &PRIV(dev)->infos, sizeof(*infos));
}

static const uint32_t *
fs_dev_supported_ptypes_get(struct rte_eth_dev *dev)
{
	struct sub_device *sdev;
	struct rte_eth_dev *edev;

	sdev = TX_SUBDEV(dev);
	if (sdev == NULL)
		return NULL;
	edev = ETH(sdev);
	/* ENOTSUP: counts as no supported ptypes */
	if (SUBOPS(sdev, dev_supported_ptypes_get) == NULL)
		return NULL;
	/*
	 * The API does not permit to do a clean AND of all ptypes,
	 * It is also incomplete by design and we do not really care
	 * to have a best possible value in this context.
	 * We just return the ptypes of the device of highest
	 * priority, usually the PREFERRED device.
	 */
	return SUBOPS(sdev, dev_supported_ptypes_get)(edev);
}

static int
fs_mtu_set(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct sub_device *sdev;
	uint8_t i;
	int ret;

	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE) {
		DEBUG("Calling rte_eth_dev_set_mtu on sub_device %d", i);
		ret = rte_eth_dev_set_mtu(PORT_ID(sdev), mtu);
		if (ret) {
			ERROR("Operation rte_eth_dev_set_mtu failed for sub_device %d with error %d",
			      i, ret);
			return ret;
		}
	}
	return 0;
}

static int
fs_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vlan_id, int on)
{
	struct sub_device *sdev;
	uint8_t i;
	int ret;

	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE) {
		DEBUG("Calling rte_eth_dev_vlan_filter on sub_device %d", i);
		ret = rte_eth_dev_vlan_filter(PORT_ID(sdev), vlan_id, on);
		if (ret) {
			ERROR("Operation rte_eth_dev_vlan_filter failed for sub_device %d"
			      " with error %d", i, ret);
			return ret;
		}
	}
	return 0;
}

static int
fs_flow_ctrl_get(struct rte_eth_dev *dev,
		struct rte_eth_fc_conf *fc_conf)
{
	struct sub_device *sdev;

	sdev = TX_SUBDEV(dev);
	if (sdev == NULL)
		return 0;
	if (SUBOPS(sdev, flow_ctrl_get) == NULL)
		return -ENOTSUP;
	return SUBOPS(sdev, flow_ctrl_get)(ETH(sdev), fc_conf);
}

static int
fs_flow_ctrl_set(struct rte_eth_dev *dev,
		struct rte_eth_fc_conf *fc_conf)
{
	struct sub_device *sdev;
	uint8_t i;
	int ret;

	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE) {
		DEBUG("Calling rte_eth_dev_flow_ctrl_set on sub_device %d", i);
		ret = rte_eth_dev_flow_ctrl_set(PORT_ID(sdev), fc_conf);
		if (ret) {
			ERROR("Operation rte_eth_dev_flow_ctrl_set failed for sub_device %d"
			      " with error %d", i, ret);
			return ret;
		}
	}
	return 0;
}

static void
fs_mac_addr_remove(struct rte_eth_dev *dev, uint32_t index)
{
	struct sub_device *sdev;
	uint8_t i;

	/* No check: already done within the rte_eth_dev_mac_addr_remove
	 * call for the fail-safe device.
	 */
	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE)
		rte_eth_dev_mac_addr_remove(PORT_ID(sdev),
				&dev->data->mac_addrs[index]);
	PRIV(dev)->mac_addr_pool[index] = 0;
}

static int
fs_mac_addr_add(struct rte_eth_dev *dev,
		struct ether_addr *mac_addr,
		uint32_t index,
		uint32_t vmdq)
{
	struct sub_device *sdev;
	int ret;
	uint8_t i;

	RTE_ASSERT(index < FAILSAFE_MAX_ETHADDR);
	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE) {
		ret = rte_eth_dev_mac_addr_add(PORT_ID(sdev), mac_addr, vmdq);
		if (ret) {
			ERROR("Operation rte_eth_dev_mac_addr_add failed for sub_device %"
			      PRIu8 " with error %d", i, ret);
			return ret;
		}
	}
	if (index >= PRIV(dev)->nb_mac_addr) {
		DEBUG("Growing mac_addrs array");
		PRIV(dev)->nb_mac_addr = index;
	}
	PRIV(dev)->mac_addr_pool[index] = vmdq;
	return 0;
}

static void
fs_mac_addr_set(struct rte_eth_dev *dev, struct ether_addr *mac_addr)
{
	struct sub_device *sdev;
	uint8_t i;

	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE)
		rte_eth_dev_default_mac_addr_set(PORT_ID(sdev), mac_addr);
}

static int
fs_filter_ctrl(struct rte_eth_dev *dev,
		enum rte_filter_type type,
		enum rte_filter_op op,
		void *arg)
{
	struct sub_device *sdev;
	uint8_t i;
	int ret;

	if (type == RTE_ETH_FILTER_GENERIC &&
	    op == RTE_ETH_FILTER_GET) {
		*(const void **)arg = &fs_flow_ops;
		return 0;
	}
	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE) {
		DEBUG("Calling rte_eth_dev_filter_ctrl on sub_device %d", i);
		ret = rte_eth_dev_filter_ctrl(PORT_ID(sdev), type, op, arg);
		if (ret) {
			ERROR("Operation rte_eth_dev_filter_ctrl failed for sub_device %d"
			      " with error %d", i, ret);
			return ret;
		}
	}
	return 0;
}

const struct eth_dev_ops failsafe_ops = {
	.dev_configure = fs_dev_configure,
	.dev_start = fs_dev_start,
	.dev_stop = fs_dev_stop,
	.dev_set_link_down = fs_dev_set_link_down,
	.dev_set_link_up = fs_dev_set_link_up,
	.dev_close = fs_dev_close,
	.promiscuous_enable = fs_promiscuous_enable,
	.promiscuous_disable = fs_promiscuous_disable,
	.allmulticast_enable = fs_allmulticast_enable,
	.allmulticast_disable = fs_allmulticast_disable,
	.link_update = fs_link_update,
	.stats_get = fs_stats_get,
	.stats_reset = fs_stats_reset,
	.dev_infos_get = fs_dev_infos_get,
	.dev_supported_ptypes_get = fs_dev_supported_ptypes_get,
	.mtu_set = fs_mtu_set,
	.vlan_filter_set = fs_vlan_filter_set,
	.rx_queue_setup = fs_rx_queue_setup,
	.tx_queue_setup = fs_tx_queue_setup,
	.rx_queue_release = fs_rx_queue_release,
	.tx_queue_release = fs_tx_queue_release,
	.flow_ctrl_get = fs_flow_ctrl_get,
	.flow_ctrl_set = fs_flow_ctrl_set,
	.mac_addr_remove = fs_mac_addr_remove,
	.mac_addr_add = fs_mac_addr_add,
	.mac_addr_set = fs_mac_addr_set,
	.filter_ctrl = fs_filter_ctrl,
};
