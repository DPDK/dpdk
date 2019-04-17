/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 6WIND S.A.
 * Copyright 2017 Mellanox Technologies, Ltd
 */

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

#include <rte_debug.h>
#include <rte_atomic.h>
#include <rte_ethdev_driver.h>
#include <rte_malloc.h>
#include <rte_flow.h>
#include <rte_cycles.h>
#include <rte_ethdev.h>

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
		DEV_RX_OFFLOAD_IPV4_CKSUM |
		DEV_RX_OFFLOAD_UDP_CKSUM |
		DEV_RX_OFFLOAD_TCP_CKSUM |
		DEV_RX_OFFLOAD_TCP_LRO |
		DEV_RX_OFFLOAD_QINQ_STRIP |
		DEV_RX_OFFLOAD_OUTER_IPV4_CKSUM |
		DEV_RX_OFFLOAD_MACSEC_STRIP |
		DEV_RX_OFFLOAD_HEADER_SPLIT |
		DEV_RX_OFFLOAD_VLAN_FILTER |
		DEV_RX_OFFLOAD_VLAN_EXTEND |
		DEV_RX_OFFLOAD_JUMBO_FRAME |
		DEV_RX_OFFLOAD_SCATTER |
		DEV_RX_OFFLOAD_TIMESTAMP |
		DEV_RX_OFFLOAD_SECURITY,
	.rx_queue_offload_capa =
		DEV_RX_OFFLOAD_VLAN_STRIP |
		DEV_RX_OFFLOAD_IPV4_CKSUM |
		DEV_RX_OFFLOAD_UDP_CKSUM |
		DEV_RX_OFFLOAD_TCP_CKSUM |
		DEV_RX_OFFLOAD_TCP_LRO |
		DEV_RX_OFFLOAD_QINQ_STRIP |
		DEV_RX_OFFLOAD_OUTER_IPV4_CKSUM |
		DEV_RX_OFFLOAD_MACSEC_STRIP |
		DEV_RX_OFFLOAD_HEADER_SPLIT |
		DEV_RX_OFFLOAD_VLAN_FILTER |
		DEV_RX_OFFLOAD_VLAN_EXTEND |
		DEV_RX_OFFLOAD_JUMBO_FRAME |
		DEV_RX_OFFLOAD_SCATTER |
		DEV_RX_OFFLOAD_TIMESTAMP |
		DEV_RX_OFFLOAD_SECURITY,
	.tx_offload_capa =
		DEV_TX_OFFLOAD_MULTI_SEGS |
		DEV_TX_OFFLOAD_MBUF_FAST_FREE |
		DEV_TX_OFFLOAD_IPV4_CKSUM |
		DEV_TX_OFFLOAD_UDP_CKSUM |
		DEV_TX_OFFLOAD_TCP_CKSUM |
		DEV_TX_OFFLOAD_TCP_TSO,
	.flow_type_rss_offloads =
			ETH_RSS_IP |
			ETH_RSS_UDP |
			ETH_RSS_TCP,
	.dev_capa =
		RTE_ETH_DEV_CAPA_RUNTIME_RX_QUEUE_SETUP |
		RTE_ETH_DEV_CAPA_RUNTIME_TX_QUEUE_SETUP,
};

static int
fs_dev_configure(struct rte_eth_dev *dev)
{
	struct sub_device *sdev;
	uint8_t i;
	int ret;

	fs_lock(dev, 0);
	FOREACH_SUBDEV(sdev, i, dev) {
		int rmv_interrupt = 0;
		int lsc_interrupt = 0;
		int lsc_enabled;

		if (sdev->state != DEV_PROBED &&
		    !(PRIV(dev)->alarm_lock == 0 && sdev->state == DEV_ACTIVE))
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
		ret = rte_eth_dev_configure(PORT_ID(sdev),
					dev->data->nb_rx_queues,
					dev->data->nb_tx_queues,
					&dev->data->dev_conf);
		if (ret) {
			if (!fs_err(sdev, ret))
				continue;
			ERROR("Could not configure sub_device %d", i);
			fs_unlock(dev, 0);
			return ret;
		}
		if (rmv_interrupt && sdev->rmv_callback == 0) {
			ret = rte_eth_dev_callback_register(PORT_ID(sdev),
					RTE_ETH_EVENT_INTR_RMV,
					failsafe_eth_rmv_event_callback,
					sdev);
			if (ret)
				WARN("Failed to register RMV callback for sub_device %d",
				     SUB_ID(sdev));
			else
				sdev->rmv_callback = 1;
		}
		dev->data->dev_conf.intr_conf.rmv = 0;
		if (lsc_interrupt && sdev->lsc_callback == 0) {
			ret = rte_eth_dev_callback_register(PORT_ID(sdev),
						RTE_ETH_EVENT_INTR_LSC,
						failsafe_eth_lsc_event_callback,
						dev);
			if (ret)
				WARN("Failed to register LSC callback for sub_device %d",
				     SUB_ID(sdev));
			else
				sdev->lsc_callback = 1;
		}
		dev->data->dev_conf.intr_conf.lsc = lsc_enabled;
		sdev->state = DEV_ACTIVE;
	}
	if (PRIV(dev)->state < DEV_ACTIVE)
		PRIV(dev)->state = DEV_ACTIVE;
	fs_unlock(dev, 0);
	return 0;
}

static void
fs_set_queues_state_start(struct rte_eth_dev *dev)
{
	struct rxq *rxq;
	struct txq *txq;
	uint16_t i;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];
		if (rxq != NULL && !rxq->info.conf.rx_deferred_start)
			dev->data->rx_queue_state[i] =
						RTE_ETH_QUEUE_STATE_STARTED;
	}
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		if (txq != NULL && !txq->info.conf.tx_deferred_start)
			dev->data->tx_queue_state[i] =
						RTE_ETH_QUEUE_STATE_STARTED;
	}
}

static int
fs_dev_start(struct rte_eth_dev *dev)
{
	struct sub_device *sdev;
	uint8_t i;
	int ret;

	fs_lock(dev, 0);
	ret = failsafe_rx_intr_install(dev);
	if (ret) {
		fs_unlock(dev, 0);
		return ret;
	}
	FOREACH_SUBDEV(sdev, i, dev) {
		if (sdev->state != DEV_ACTIVE)
			continue;
		DEBUG("Starting sub_device %d", i);
		ret = rte_eth_dev_start(PORT_ID(sdev));
		if (ret) {
			if (!fs_err(sdev, ret))
				continue;
			fs_unlock(dev, 0);
			return ret;
		}
		ret = failsafe_rx_intr_install_subdevice(sdev);
		if (ret) {
			if (!fs_err(sdev, ret))
				continue;
			rte_eth_dev_stop(PORT_ID(sdev));
			fs_unlock(dev, 0);
			return ret;
		}
		sdev->state = DEV_STARTED;
	}
	if (PRIV(dev)->state < DEV_STARTED) {
		PRIV(dev)->state = DEV_STARTED;
		fs_set_queues_state_start(dev);
	}
	fs_switch_dev(dev, NULL);
	fs_unlock(dev, 0);
	return 0;
}

static void
fs_set_queues_state_stop(struct rte_eth_dev *dev)
{
	uint16_t i;

	for (i = 0; i < dev->data->nb_rx_queues; i++)
		if (dev->data->rx_queues[i] != NULL)
			dev->data->rx_queue_state[i] =
						RTE_ETH_QUEUE_STATE_STOPPED;
	for (i = 0; i < dev->data->nb_tx_queues; i++)
		if (dev->data->tx_queues[i] != NULL)
			dev->data->tx_queue_state[i] =
						RTE_ETH_QUEUE_STATE_STOPPED;
}

static void
fs_dev_stop(struct rte_eth_dev *dev)
{
	struct sub_device *sdev;
	uint8_t i;

	fs_lock(dev, 0);
	PRIV(dev)->state = DEV_STARTED - 1;
	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_STARTED) {
		rte_eth_dev_stop(PORT_ID(sdev));
		failsafe_rx_intr_uninstall_subdevice(sdev);
		sdev->state = DEV_STARTED - 1;
	}
	failsafe_rx_intr_uninstall(dev);
	fs_set_queues_state_stop(dev);
	fs_unlock(dev, 0);
}

static int
fs_dev_set_link_up(struct rte_eth_dev *dev)
{
	struct sub_device *sdev;
	uint8_t i;
	int ret;

	fs_lock(dev, 0);
	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE) {
		DEBUG("Calling rte_eth_dev_set_link_up on sub_device %d", i);
		ret = rte_eth_dev_set_link_up(PORT_ID(sdev));
		if ((ret = fs_err(sdev, ret))) {
			ERROR("Operation rte_eth_dev_set_link_up failed for sub_device %d"
			      " with error %d", i, ret);
			fs_unlock(dev, 0);
			return ret;
		}
	}
	fs_unlock(dev, 0);
	return 0;
}

static int
fs_dev_set_link_down(struct rte_eth_dev *dev)
{
	struct sub_device *sdev;
	uint8_t i;
	int ret;

	fs_lock(dev, 0);
	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE) {
		DEBUG("Calling rte_eth_dev_set_link_down on sub_device %d", i);
		ret = rte_eth_dev_set_link_down(PORT_ID(sdev));
		if ((ret = fs_err(sdev, ret))) {
			ERROR("Operation rte_eth_dev_set_link_down failed for sub_device %d"
			      " with error %d", i, ret);
			fs_unlock(dev, 0);
			return ret;
		}
	}
	fs_unlock(dev, 0);
	return 0;
}

static void fs_dev_free_queues(struct rte_eth_dev *dev);
static void
fs_dev_close(struct rte_eth_dev *dev)
{
	struct sub_device *sdev;
	uint8_t i;

	fs_lock(dev, 0);
	failsafe_hotplug_alarm_cancel(dev);
	if (PRIV(dev)->state == DEV_STARTED)
		dev->dev_ops->dev_stop(dev);
	PRIV(dev)->state = DEV_ACTIVE - 1;
	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE) {
		DEBUG("Closing sub_device %d", i);
		failsafe_eth_dev_unregister_callbacks(sdev);
		rte_eth_dev_close(PORT_ID(sdev));
		sdev->state = DEV_ACTIVE - 1;
	}
	fs_dev_free_queues(dev);
	fs_unlock(dev, 0);
}

static int
fs_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct sub_device *sdev;
	uint8_t i;
	int ret;
	int err = 0;
	bool failure = true;

	fs_lock(dev, 0);
	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE) {
		uint16_t port_id = ETH(sdev)->data->port_id;

		ret = rte_eth_dev_rx_queue_stop(port_id, rx_queue_id);
		ret = fs_err(sdev, ret);
		if (ret) {
			ERROR("Rx queue stop failed for subdevice %d", i);
			err = ret;
		} else {
			failure = false;
		}
	}
	dev->data->rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;
	fs_unlock(dev, 0);
	/* Return 0 in case of at least one successful queue stop */
	return (failure) ? err : 0;
}

static int
fs_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct sub_device *sdev;
	uint8_t i;
	int ret;

	fs_lock(dev, 0);
	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE) {
		uint16_t port_id = ETH(sdev)->data->port_id;

		ret = rte_eth_dev_rx_queue_start(port_id, rx_queue_id);
		ret = fs_err(sdev, ret);
		if (ret) {
			ERROR("Rx queue start failed for subdevice %d", i);
			fs_rx_queue_stop(dev, rx_queue_id);
			fs_unlock(dev, 0);
			return ret;
		}
	}
	dev->data->rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STARTED;
	fs_unlock(dev, 0);
	return 0;
}

static int
fs_tx_queue_stop(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct sub_device *sdev;
	uint8_t i;
	int ret;
	int err = 0;
	bool failure = true;

	fs_lock(dev, 0);
	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE) {
		uint16_t port_id = ETH(sdev)->data->port_id;

		ret = rte_eth_dev_tx_queue_stop(port_id, tx_queue_id);
		ret = fs_err(sdev, ret);
		if (ret) {
			ERROR("Tx queue stop failed for subdevice %d", i);
			err = ret;
		} else {
			failure = false;
		}
	}
	dev->data->tx_queue_state[tx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;
	fs_unlock(dev, 0);
	/* Return 0 in case of at least one successful queue stop */
	return (failure) ? err : 0;
}

static int
fs_tx_queue_start(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct sub_device *sdev;
	uint8_t i;
	int ret;

	fs_lock(dev, 0);
	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE) {
		uint16_t port_id = ETH(sdev)->data->port_id;

		ret = rte_eth_dev_tx_queue_start(port_id, tx_queue_id);
		ret = fs_err(sdev, ret);
		if (ret) {
			ERROR("Tx queue start failed for subdevice %d", i);
			fs_tx_queue_stop(dev, tx_queue_id);
			fs_unlock(dev, 0);
			return ret;
		}
	}
	dev->data->tx_queue_state[tx_queue_id] = RTE_ETH_QUEUE_STATE_STARTED;
	fs_unlock(dev, 0);
	return 0;
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
	dev = &rte_eth_devices[rxq->priv->data->port_id];
	fs_lock(dev, 0);
	if (rxq->event_fd > 0)
		close(rxq->event_fd);
	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE) {
		if (ETH(sdev)->data->rx_queues != NULL &&
		    ETH(sdev)->data->rx_queues[rxq->qid] != NULL) {
			SUBOPS(sdev, rx_queue_release)
				(ETH(sdev)->data->rx_queues[rxq->qid]);
		}
	}
	dev->data->rx_queues[rxq->qid] = NULL;
	rte_free(rxq);
	fs_unlock(dev, 0);
}

static int
fs_rx_queue_setup(struct rte_eth_dev *dev,
		uint16_t rx_queue_id,
		uint16_t nb_rx_desc,
		unsigned int socket_id,
		const struct rte_eth_rxconf *rx_conf,
		struct rte_mempool *mb_pool)
{
	/*
	 * FIXME: Add a proper interface in rte_eal_interrupts for
	 * allocating eventfd as an interrupt vector.
	 * For the time being, fake as if we are using MSIX interrupts,
	 * this will cause rte_intr_efd_enable to allocate an eventfd for us.
	 */
	struct rte_intr_handle intr_handle = {
		.type = RTE_INTR_HANDLE_VFIO_MSIX,
		.efds = { -1, },
	};
	struct sub_device *sdev;
	struct rxq *rxq;
	uint8_t i;
	int ret;

	fs_lock(dev, 0);
	if (rx_conf->rx_deferred_start) {
		FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_PROBED) {
			if (SUBOPS(sdev, rx_queue_start) == NULL) {
				ERROR("Rx queue deferred start is not "
					"supported for subdevice %d", i);
				fs_unlock(dev, 0);
				return -EINVAL;
			}
		}
	}
	rxq = dev->data->rx_queues[rx_queue_id];
	if (rxq != NULL) {
		fs_rx_queue_release(rxq);
		dev->data->rx_queues[rx_queue_id] = NULL;
	}
	rxq = rte_zmalloc(NULL,
			  sizeof(*rxq) +
			  sizeof(rte_atomic64_t) * PRIV(dev)->subs_tail,
			  RTE_CACHE_LINE_SIZE);
	if (rxq == NULL) {
		fs_unlock(dev, 0);
		return -ENOMEM;
	}
	FOREACH_SUBDEV(sdev, i, dev)
		rte_atomic64_init(&rxq->refcnt[i]);
	rxq->qid = rx_queue_id;
	rxq->socket_id = socket_id;
	rxq->info.mp = mb_pool;
	rxq->info.conf = *rx_conf;
	rxq->info.nb_desc = nb_rx_desc;
	rxq->priv = PRIV(dev);
	rxq->sdev = PRIV(dev)->subs;
	ret = rte_intr_efd_enable(&intr_handle, 1);
	if (ret < 0) {
		fs_unlock(dev, 0);
		return ret;
	}
	rxq->event_fd = intr_handle.efds[0];
	dev->data->rx_queues[rx_queue_id] = rxq;
	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE) {
		ret = rte_eth_rx_queue_setup(PORT_ID(sdev),
				rx_queue_id,
				nb_rx_desc, socket_id,
				rx_conf, mb_pool);
		if ((ret = fs_err(sdev, ret))) {
			ERROR("RX queue setup failed for sub_device %d", i);
			goto free_rxq;
		}
	}
	fs_unlock(dev, 0);
	return 0;
free_rxq:
	fs_rx_queue_release(rxq);
	fs_unlock(dev, 0);
	return ret;
}

static int
fs_rx_intr_enable(struct rte_eth_dev *dev, uint16_t idx)
{
	struct rxq *rxq;
	struct sub_device *sdev;
	uint8_t i;
	int ret;
	int rc = 0;

	fs_lock(dev, 0);
	if (idx >= dev->data->nb_rx_queues) {
		rc = -EINVAL;
		goto unlock;
	}
	rxq = dev->data->rx_queues[idx];
	if (rxq == NULL || rxq->event_fd <= 0) {
		rc = -EINVAL;
		goto unlock;
	}
	/* Fail if proxy service is nor running. */
	if (PRIV(dev)->rxp.sstate != SS_RUNNING) {
		ERROR("failsafe interrupt services are not running");
		rc = -EAGAIN;
		goto unlock;
	}
	rxq->enable_events = 1;
	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE) {
		ret = rte_eth_dev_rx_intr_enable(PORT_ID(sdev), idx);
		ret = fs_err(sdev, ret);
		if (ret)
			rc = ret;
	}
unlock:
	fs_unlock(dev, 0);
	if (rc)
		rte_errno = -rc;
	return rc;
}

static int
fs_rx_intr_disable(struct rte_eth_dev *dev, uint16_t idx)
{
	struct rxq *rxq;
	struct sub_device *sdev;
	uint64_t u64;
	uint8_t i;
	int rc = 0;
	int ret;

	fs_lock(dev, 0);
	if (idx >= dev->data->nb_rx_queues) {
		rc = -EINVAL;
		goto unlock;
	}
	rxq = dev->data->rx_queues[idx];
	if (rxq == NULL || rxq->event_fd <= 0) {
		rc = -EINVAL;
		goto unlock;
	}
	rxq->enable_events = 0;
	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE) {
		ret = rte_eth_dev_rx_intr_disable(PORT_ID(sdev), idx);
		ret = fs_err(sdev, ret);
		if (ret)
			rc = ret;
	}
	/* Clear pending events */
	while (read(rxq->event_fd, &u64, sizeof(uint64_t)) >  0)
		;
unlock:
	fs_unlock(dev, 0);
	if (rc)
		rte_errno = -rc;
	return rc;
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
	dev = &rte_eth_devices[txq->priv->data->port_id];
	fs_lock(dev, 0);
	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE) {
		if (ETH(sdev)->data->tx_queues != NULL &&
		    ETH(sdev)->data->tx_queues[txq->qid] != NULL) {
			SUBOPS(sdev, tx_queue_release)
				(ETH(sdev)->data->tx_queues[txq->qid]);
		}
	}
	dev->data->tx_queues[txq->qid] = NULL;
	rte_free(txq);
	fs_unlock(dev, 0);
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

	fs_lock(dev, 0);
	if (tx_conf->tx_deferred_start) {
		FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_PROBED) {
			if (SUBOPS(sdev, tx_queue_start) == NULL) {
				ERROR("Tx queue deferred start is not "
					"supported for subdevice %d", i);
				fs_unlock(dev, 0);
				return -EINVAL;
			}
		}
	}
	txq = dev->data->tx_queues[tx_queue_id];
	if (txq != NULL) {
		fs_tx_queue_release(txq);
		dev->data->tx_queues[tx_queue_id] = NULL;
	}
	txq = rte_zmalloc("ethdev TX queue",
			  sizeof(*txq) +
			  sizeof(rte_atomic64_t) * PRIV(dev)->subs_tail,
			  RTE_CACHE_LINE_SIZE);
	if (txq == NULL) {
		fs_unlock(dev, 0);
		return -ENOMEM;
	}
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
		if ((ret = fs_err(sdev, ret))) {
			ERROR("TX queue setup failed for sub_device %d", i);
			goto free_txq;
		}
	}
	fs_unlock(dev, 0);
	return 0;
free_txq:
	fs_tx_queue_release(txq);
	fs_unlock(dev, 0);
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

	fs_lock(dev, 0);
	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE)
		rte_eth_promiscuous_enable(PORT_ID(sdev));
	fs_unlock(dev, 0);
}

static void
fs_promiscuous_disable(struct rte_eth_dev *dev)
{
	struct sub_device *sdev;
	uint8_t i;

	fs_lock(dev, 0);
	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE)
		rte_eth_promiscuous_disable(PORT_ID(sdev));
	fs_unlock(dev, 0);
}

static void
fs_allmulticast_enable(struct rte_eth_dev *dev)
{
	struct sub_device *sdev;
	uint8_t i;

	fs_lock(dev, 0);
	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE)
		rte_eth_allmulticast_enable(PORT_ID(sdev));
	fs_unlock(dev, 0);
}

static void
fs_allmulticast_disable(struct rte_eth_dev *dev)
{
	struct sub_device *sdev;
	uint8_t i;

	fs_lock(dev, 0);
	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE)
		rte_eth_allmulticast_disable(PORT_ID(sdev));
	fs_unlock(dev, 0);
}

static int
fs_link_update(struct rte_eth_dev *dev,
		int wait_to_complete)
{
	struct sub_device *sdev;
	uint8_t i;
	int ret;

	fs_lock(dev, 0);
	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE) {
		DEBUG("Calling link_update on sub_device %d", i);
		ret = (SUBOPS(sdev, link_update))(ETH(sdev), wait_to_complete);
		if (ret && ret != -1 && sdev->remove == 0 &&
		    rte_eth_dev_is_removed(PORT_ID(sdev)) == 0) {
			ERROR("Link update failed for sub_device %d with error %d",
			      i, ret);
			fs_unlock(dev, 0);
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
			fs_unlock(dev, 0);
			return 0;
		}
	}
	fs_unlock(dev, 0);
	return -1;
}

static int
fs_stats_get(struct rte_eth_dev *dev,
	     struct rte_eth_stats *stats)
{
	struct rte_eth_stats backup;
	struct sub_device *sdev;
	uint8_t i;
	int ret;

	fs_lock(dev, 0);
	rte_memcpy(stats, &PRIV(dev)->stats_accumulator, sizeof(*stats));
	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE) {
		struct rte_eth_stats *snapshot = &sdev->stats_snapshot.stats;
		uint64_t *timestamp = &sdev->stats_snapshot.timestamp;

		rte_memcpy(&backup, snapshot, sizeof(backup));
		ret = rte_eth_stats_get(PORT_ID(sdev), snapshot);
		if (ret) {
			if (!fs_err(sdev, ret)) {
				rte_memcpy(snapshot, &backup, sizeof(backup));
				goto inc;
			}
			ERROR("Operation rte_eth_stats_get failed for sub_device %d with error %d",
				  i, ret);
			*timestamp = 0;
			fs_unlock(dev, 0);
			return ret;
		}
		*timestamp = rte_rdtsc();
inc:
		failsafe_stats_increment(stats, snapshot);
	}
	fs_unlock(dev, 0);
	return 0;
}

static void
fs_stats_reset(struct rte_eth_dev *dev)
{
	struct sub_device *sdev;
	uint8_t i;

	fs_lock(dev, 0);
	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE) {
		rte_eth_stats_reset(PORT_ID(sdev));
		memset(&sdev->stats_snapshot, 0, sizeof(struct rte_eth_stats));
	}
	memset(&PRIV(dev)->stats_accumulator, 0, sizeof(struct rte_eth_stats));
	fs_unlock(dev, 0);
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
 *      Uses a logical AND of device capabilities among
 *      all sub_devices and the default capabilities.
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
		uint64_t rx_offload_capa;
		uint64_t rxq_offload_capa;
		uint64_t rss_hf_offload_capa;
		uint64_t dev_capa;

		rx_offload_capa = default_infos.rx_offload_capa;
		rxq_offload_capa = default_infos.rx_queue_offload_capa;
		rss_hf_offload_capa = default_infos.flow_type_rss_offloads;
		dev_capa = default_infos.dev_capa;
		FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_PROBED) {
			rte_eth_dev_info_get(PORT_ID(sdev),
					&PRIV(dev)->infos);
			rx_offload_capa &= PRIV(dev)->infos.rx_offload_capa;
			rxq_offload_capa &=
					PRIV(dev)->infos.rx_queue_offload_capa;
			rss_hf_offload_capa &=
					PRIV(dev)->infos.flow_type_rss_offloads;
			dev_capa &= PRIV(dev)->infos.dev_capa;
		}
		sdev = TX_SUBDEV(dev);
		rte_eth_dev_info_get(PORT_ID(sdev), &PRIV(dev)->infos);
		PRIV(dev)->infos.rx_offload_capa = rx_offload_capa;
		PRIV(dev)->infos.rx_queue_offload_capa = rxq_offload_capa;
		PRIV(dev)->infos.flow_type_rss_offloads = rss_hf_offload_capa;
		PRIV(dev)->infos.dev_capa = dev_capa;
		PRIV(dev)->infos.tx_offload_capa &=
					default_infos.tx_offload_capa;
		PRIV(dev)->infos.tx_queue_offload_capa &=
					default_infos.tx_queue_offload_capa;
	}
	rte_memcpy(infos, &PRIV(dev)->infos, sizeof(*infos));
}

static const uint32_t *
fs_dev_supported_ptypes_get(struct rte_eth_dev *dev)
{
	struct sub_device *sdev;
	struct rte_eth_dev *edev;
	const uint32_t *ret;

	fs_lock(dev, 0);
	sdev = TX_SUBDEV(dev);
	if (sdev == NULL) {
		ret = NULL;
		goto unlock;
	}
	edev = ETH(sdev);
	/* ENOTSUP: counts as no supported ptypes */
	if (SUBOPS(sdev, dev_supported_ptypes_get) == NULL) {
		ret = NULL;
		goto unlock;
	}
	/*
	 * The API does not permit to do a clean AND of all ptypes,
	 * It is also incomplete by design and we do not really care
	 * to have a best possible value in this context.
	 * We just return the ptypes of the device of highest
	 * priority, usually the PREFERRED device.
	 */
	ret = SUBOPS(sdev, dev_supported_ptypes_get)(edev);
unlock:
	fs_unlock(dev, 0);
	return ret;
}

static int
fs_mtu_set(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct sub_device *sdev;
	uint8_t i;
	int ret;

	fs_lock(dev, 0);
	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE) {
		DEBUG("Calling rte_eth_dev_set_mtu on sub_device %d", i);
		ret = rte_eth_dev_set_mtu(PORT_ID(sdev), mtu);
		if ((ret = fs_err(sdev, ret))) {
			ERROR("Operation rte_eth_dev_set_mtu failed for sub_device %d with error %d",
			      i, ret);
			fs_unlock(dev, 0);
			return ret;
		}
	}
	fs_unlock(dev, 0);
	return 0;
}

static int
fs_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vlan_id, int on)
{
	struct sub_device *sdev;
	uint8_t i;
	int ret;

	fs_lock(dev, 0);
	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE) {
		DEBUG("Calling rte_eth_dev_vlan_filter on sub_device %d", i);
		ret = rte_eth_dev_vlan_filter(PORT_ID(sdev), vlan_id, on);
		if ((ret = fs_err(sdev, ret))) {
			ERROR("Operation rte_eth_dev_vlan_filter failed for sub_device %d"
			      " with error %d", i, ret);
			fs_unlock(dev, 0);
			return ret;
		}
	}
	fs_unlock(dev, 0);
	return 0;
}

static int
fs_flow_ctrl_get(struct rte_eth_dev *dev,
		struct rte_eth_fc_conf *fc_conf)
{
	struct sub_device *sdev;
	int ret;

	fs_lock(dev, 0);
	sdev = TX_SUBDEV(dev);
	if (sdev == NULL) {
		ret = 0;
		goto unlock;
	}
	if (SUBOPS(sdev, flow_ctrl_get) == NULL) {
		ret = -ENOTSUP;
		goto unlock;
	}
	ret = SUBOPS(sdev, flow_ctrl_get)(ETH(sdev), fc_conf);
unlock:
	fs_unlock(dev, 0);
	return ret;
}

static int
fs_flow_ctrl_set(struct rte_eth_dev *dev,
		struct rte_eth_fc_conf *fc_conf)
{
	struct sub_device *sdev;
	uint8_t i;
	int ret;

	fs_lock(dev, 0);
	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE) {
		DEBUG("Calling rte_eth_dev_flow_ctrl_set on sub_device %d", i);
		ret = rte_eth_dev_flow_ctrl_set(PORT_ID(sdev), fc_conf);
		if ((ret = fs_err(sdev, ret))) {
			ERROR("Operation rte_eth_dev_flow_ctrl_set failed for sub_device %d"
			      " with error %d", i, ret);
			fs_unlock(dev, 0);
			return ret;
		}
	}
	fs_unlock(dev, 0);
	return 0;
}

static void
fs_mac_addr_remove(struct rte_eth_dev *dev, uint32_t index)
{
	struct sub_device *sdev;
	uint8_t i;

	fs_lock(dev, 0);
	/* No check: already done within the rte_eth_dev_mac_addr_remove
	 * call for the fail-safe device.
	 */
	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE)
		rte_eth_dev_mac_addr_remove(PORT_ID(sdev),
				&dev->data->mac_addrs[index]);
	PRIV(dev)->mac_addr_pool[index] = 0;
	fs_unlock(dev, 0);
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
	fs_lock(dev, 0);
	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE) {
		ret = rte_eth_dev_mac_addr_add(PORT_ID(sdev), mac_addr, vmdq);
		if ((ret = fs_err(sdev, ret))) {
			ERROR("Operation rte_eth_dev_mac_addr_add failed for sub_device %"
			      PRIu8 " with error %d", i, ret);
			fs_unlock(dev, 0);
			return ret;
		}
	}
	if (index >= PRIV(dev)->nb_mac_addr) {
		DEBUG("Growing mac_addrs array");
		PRIV(dev)->nb_mac_addr = index;
	}
	PRIV(dev)->mac_addr_pool[index] = vmdq;
	fs_unlock(dev, 0);
	return 0;
}

static int
fs_mac_addr_set(struct rte_eth_dev *dev, struct ether_addr *mac_addr)
{
	struct sub_device *sdev;
	uint8_t i;
	int ret;

	fs_lock(dev, 0);
	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE) {
		ret = rte_eth_dev_default_mac_addr_set(PORT_ID(sdev), mac_addr);
		ret = fs_err(sdev, ret);
		if (ret) {
			ERROR("Operation rte_eth_dev_mac_addr_set failed for sub_device %d with error %d",
				i, ret);
			fs_unlock(dev, 0);
			return ret;
		}
	}
	fs_unlock(dev, 0);

	return 0;
}

static int
fs_set_mc_addr_list(struct rte_eth_dev *dev,
		    struct ether_addr *mc_addr_set, uint32_t nb_mc_addr)
{
	struct sub_device *sdev;
	uint8_t i;
	int ret;
	void *mcast_addrs;

	fs_lock(dev, 0);

	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE) {
		ret = rte_eth_dev_set_mc_addr_list(PORT_ID(sdev),
						   mc_addr_set, nb_mc_addr);
		if (ret != 0) {
			ERROR("Operation rte_eth_dev_set_mc_addr_list failed for sub_device %d with error %d",
			      i, ret);
			goto rollback;
		}
	}

	mcast_addrs = rte_realloc(PRIV(dev)->mcast_addrs,
		nb_mc_addr * sizeof(PRIV(dev)->mcast_addrs[0]), 0);
	if (mcast_addrs == NULL && nb_mc_addr > 0) {
		ret = -ENOMEM;
		goto rollback;
	}
	rte_memcpy(mcast_addrs, mc_addr_set,
		   nb_mc_addr * sizeof(PRIV(dev)->mcast_addrs[0]));
	PRIV(dev)->nb_mcast_addr = nb_mc_addr;
	PRIV(dev)->mcast_addrs = mcast_addrs;

	fs_unlock(dev, 0);
	return 0;

rollback:
	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE) {
		int rc = rte_eth_dev_set_mc_addr_list(PORT_ID(sdev),
			PRIV(dev)->mcast_addrs,	PRIV(dev)->nb_mcast_addr);
		if (rc != 0) {
			ERROR("Multicast MAC address list rollback for sub_device %d failed with error %d",
			      i, rc);
		}
	}

	fs_unlock(dev, 0);
	return ret;
}

static int
fs_rss_hash_update(struct rte_eth_dev *dev,
			struct rte_eth_rss_conf *rss_conf)
{
	struct sub_device *sdev;
	uint8_t i;
	int ret;

	fs_lock(dev, 0);
	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_ACTIVE) {
		ret = rte_eth_dev_rss_hash_update(PORT_ID(sdev), rss_conf);
		ret = fs_err(sdev, ret);
		if (ret) {
			ERROR("Operation rte_eth_dev_rss_hash_update"
				" failed for sub_device %d with error %d",
				i, ret);
			fs_unlock(dev, 0);
			return ret;
		}
	}
	fs_unlock(dev, 0);

	return 0;
}

static int
fs_filter_ctrl(struct rte_eth_dev *dev __rte_unused,
		enum rte_filter_type type,
		enum rte_filter_op op,
		void *arg)
{
	if (type == RTE_ETH_FILTER_GENERIC &&
	    op == RTE_ETH_FILTER_GET) {
		*(const void **)arg = &fs_flow_ops;
		return 0;
	}
	return -ENOTSUP;
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
	.rx_queue_start = fs_rx_queue_start,
	.rx_queue_stop = fs_rx_queue_stop,
	.tx_queue_start = fs_tx_queue_start,
	.tx_queue_stop = fs_tx_queue_stop,
	.rx_queue_setup = fs_rx_queue_setup,
	.tx_queue_setup = fs_tx_queue_setup,
	.rx_queue_release = fs_rx_queue_release,
	.tx_queue_release = fs_tx_queue_release,
	.rx_queue_intr_enable = fs_rx_intr_enable,
	.rx_queue_intr_disable = fs_rx_intr_disable,
	.flow_ctrl_get = fs_flow_ctrl_get,
	.flow_ctrl_set = fs_flow_ctrl_set,
	.mac_addr_remove = fs_mac_addr_remove,
	.mac_addr_add = fs_mac_addr_add,
	.mac_addr_set = fs_mac_addr_set,
	.set_mc_addr_list = fs_set_mc_addr_list,
	.rss_hash_update = fs_rss_hash_update,
	.filter_ctrl = fs_filter_ctrl,
};
