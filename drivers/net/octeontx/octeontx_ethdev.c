/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <eventdev_pmd.h>
#include <rte_alarm.h>
#include <rte_branch_prediction.h>
#include <rte_bus_vdev.h>
#include <rte_cycles.h>
#include <rte_debug.h>
#include <rte_dev.h>
#include <rte_devargs.h>
#include <rte_kvargs.h>
#include <rte_malloc.h>
#include <rte_mbuf_pool_ops.h>
#include <rte_prefetch.h>

#include "octeontx_ethdev.h"
#include "octeontx_rxtx.h"
#include "octeontx_logs.h"

/* Useful in stopping/closing event device if no of
 * eth ports are using it.
 */
uint16_t evdev_refcnt;

struct evdev_priv_data {
	OFFLOAD_FLAGS; /*Sequence should not be changed */
} __rte_cache_aligned;

struct octeontx_vdev_init_params {
	uint8_t	nr_port;
};

uint16_t
rte_octeontx_pchan_map[OCTEONTX_MAX_BGX_PORTS][OCTEONTX_MAX_LMAC_PER_BGX];

enum octeontx_link_speed {
	OCTEONTX_LINK_SPEED_SGMII,
	OCTEONTX_LINK_SPEED_XAUI,
	OCTEONTX_LINK_SPEED_RXAUI,
	OCTEONTX_LINK_SPEED_10G_R,
	OCTEONTX_LINK_SPEED_40G_R,
	OCTEONTX_LINK_SPEED_RESERVE1,
	OCTEONTX_LINK_SPEED_QSGMII,
	OCTEONTX_LINK_SPEED_RESERVE2
};

RTE_LOG_REGISTER_SUFFIX(otx_net_logtype_mbox, mbox, NOTICE);
RTE_LOG_REGISTER_SUFFIX(otx_net_logtype_init, init, NOTICE);
RTE_LOG_REGISTER_SUFFIX(otx_net_logtype_driver, driver, NOTICE);

/* Parse integer from integer argument */
static int
parse_integer_arg(const char *key __rte_unused,
		const char *value, void *extra_args)
{
	int *i = (int *)extra_args;

	*i = atoi(value);
	if (*i < 0) {
		octeontx_log_err("argument has to be positive.");
		return -1;
	}

	return 0;
}

static int
octeontx_parse_vdev_init_params(struct octeontx_vdev_init_params *params,
				struct rte_vdev_device *dev)
{
	struct rte_kvargs *kvlist = NULL;
	int ret = 0;

	static const char * const octeontx_vdev_valid_params[] = {
		OCTEONTX_VDEV_NR_PORT_ARG,
		NULL
	};

	const char *input_args = rte_vdev_device_args(dev);
	if (params == NULL)
		return -EINVAL;


	if (input_args) {
		kvlist = rte_kvargs_parse(input_args,
				octeontx_vdev_valid_params);
		if (kvlist == NULL)
			return -1;

		ret = rte_kvargs_process(kvlist,
					OCTEONTX_VDEV_NR_PORT_ARG,
					&parse_integer_arg,
					&params->nr_port);
		if (ret < 0)
			goto free_kvlist;
	}

free_kvlist:
	rte_kvargs_free(kvlist);
	return ret;
}

static int
octeontx_port_open(struct octeontx_nic *nic)
{
	octeontx_mbox_bgx_port_conf_t bgx_port_conf;
	octeontx_mbox_bgx_port_fifo_cfg_t fifo_cfg;
	int res;

	res = 0;
	memset(&bgx_port_conf, 0x0, sizeof(bgx_port_conf));
	PMD_INIT_FUNC_TRACE();

	res = octeontx_bgx_port_open(nic->port_id, &bgx_port_conf);
	if (res < 0) {
		octeontx_log_err("failed to open port %d", res);
		return res;
	}

	nic->node = bgx_port_conf.node;
	nic->port_ena = bgx_port_conf.enable;
	nic->base_ichan = bgx_port_conf.base_chan;
	nic->base_ochan = bgx_port_conf.base_chan;
	nic->num_ichans = bgx_port_conf.num_chans;
	nic->num_ochans = bgx_port_conf.num_chans;
	nic->bgx_mtu = bgx_port_conf.mtu;
	nic->bpen = bgx_port_conf.bpen;
	nic->fcs_strip = bgx_port_conf.fcs_strip;
	nic->bcast_mode = bgx_port_conf.bcast_mode;
	nic->mcast_mode = bgx_port_conf.mcast_mode;
	nic->speed	= bgx_port_conf.mode;

	memset(&fifo_cfg, 0x0, sizeof(fifo_cfg));

	res = octeontx_bgx_port_get_fifo_cfg(nic->port_id, &fifo_cfg);
	if (res < 0) {
		octeontx_log_err("failed to get port %d fifo cfg", res);
		return res;
	}

	nic->fc.rx_fifosz = fifo_cfg.rx_fifosz;

	memcpy(&nic->mac_addr[0], &bgx_port_conf.macaddr[0],
		RTE_ETHER_ADDR_LEN);

	octeontx_log_dbg("port opened %d", nic->port_id);
	return res;
}

static void
octeontx_link_status_print(struct rte_eth_dev *eth_dev,
			   struct rte_eth_link *link)
{
	if (link && link->link_status)
		octeontx_log_info("Port %u: Link Up - speed %u Mbps - %s",
			  (eth_dev->data->port_id),
			  link->link_speed,
			  link->link_duplex == RTE_ETH_LINK_FULL_DUPLEX ?
			  "full-duplex" : "half-duplex");
	else
		octeontx_log_info("Port %d: Link Down",
				  (int)(eth_dev->data->port_id));
}

static void
octeontx_link_status_update(struct octeontx_nic *nic,
			 struct rte_eth_link *link)
{
	memset(link, 0, sizeof(*link));

	link->link_status = nic->link_up ? RTE_ETH_LINK_UP : RTE_ETH_LINK_DOWN;

	switch (nic->speed) {
	case OCTEONTX_LINK_SPEED_SGMII:
		link->link_speed = RTE_ETH_SPEED_NUM_1G;
		break;

	case OCTEONTX_LINK_SPEED_XAUI:
		link->link_speed = RTE_ETH_SPEED_NUM_10G;
		break;

	case OCTEONTX_LINK_SPEED_RXAUI:
	case OCTEONTX_LINK_SPEED_10G_R:
		link->link_speed = RTE_ETH_SPEED_NUM_10G;
		break;
	case OCTEONTX_LINK_SPEED_QSGMII:
		link->link_speed = RTE_ETH_SPEED_NUM_5G;
		break;
	case OCTEONTX_LINK_SPEED_40G_R:
		link->link_speed = RTE_ETH_SPEED_NUM_40G;
		break;

	case OCTEONTX_LINK_SPEED_RESERVE1:
	case OCTEONTX_LINK_SPEED_RESERVE2:
	default:
		link->link_speed = RTE_ETH_SPEED_NUM_NONE;
		octeontx_log_err("incorrect link speed %d", nic->speed);
		break;
	}

	link->link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
	link->link_autoneg = RTE_ETH_LINK_AUTONEG;
}

static void
octeontx_link_status_poll(void *arg)
{
	struct octeontx_nic *nic = arg;
	struct rte_eth_link link;
	struct rte_eth_dev *dev;
	int res;

	PMD_INIT_FUNC_TRACE();

	dev = nic->dev;

	res = octeontx_bgx_port_link_status(nic->port_id);
	if (res < 0) {
		octeontx_log_err("Failed to get port %d link status",
				nic->port_id);
	} else {
		if (nic->link_up != (uint8_t)res) {
			nic->link_up = (uint8_t)res;
			octeontx_link_status_update(nic, &link);
			octeontx_link_status_print(dev, &link);
			rte_eth_linkstatus_set(dev, &link);
			rte_eth_dev_callback_process(dev,
						     RTE_ETH_EVENT_INTR_LSC,
						     NULL);
		}
	}

	res = rte_eal_alarm_set(OCCTX_INTR_POLL_INTERVAL_MS * 1000,
				octeontx_link_status_poll, nic);
	if (res < 0)
		octeontx_log_err("Failed to restart alarm for port %d, err: %d",
				nic->port_id, res);
}

static void
octeontx_port_close(struct octeontx_nic *nic)
{
	PMD_INIT_FUNC_TRACE();

	rte_eal_alarm_cancel(octeontx_link_status_poll, nic);
	octeontx_bgx_port_close(nic->port_id);
	octeontx_log_dbg("port closed %d", nic->port_id);
}

static int
octeontx_port_start(struct octeontx_nic *nic)
{
	PMD_INIT_FUNC_TRACE();

	return octeontx_bgx_port_start(nic->port_id);
}

static int
octeontx_port_stop(struct octeontx_nic *nic)
{
	PMD_INIT_FUNC_TRACE();

	return octeontx_bgx_port_stop(nic->port_id);
}

static int
octeontx_port_promisc_set(struct octeontx_nic *nic, int en)
{
	struct rte_eth_dev *dev;
	int res;

	res = 0;
	PMD_INIT_FUNC_TRACE();
	dev = nic->dev;

	res = octeontx_bgx_port_promisc_set(nic->port_id, en);
	if (res < 0) {
		octeontx_log_err("failed to set promiscuous mode %d",
				nic->port_id);
		return res;
	}

	/* Set proper flag for the mode */
	dev->data->promiscuous = (en != 0) ? 1 : 0;

	octeontx_log_dbg("port %d : promiscuous mode %s",
			nic->port_id, en ? "set" : "unset");

	return 0;
}

static int
octeontx_port_stats(struct octeontx_nic *nic, struct rte_eth_stats *stats)
{
	octeontx_mbox_bgx_port_stats_t bgx_stats;
	int res;

	PMD_INIT_FUNC_TRACE();

	res = octeontx_bgx_port_stats(nic->port_id, &bgx_stats);
	if (res < 0) {
		octeontx_log_err("failed to get port stats %d", nic->port_id);
		return res;
	}

	stats->ipackets = bgx_stats.rx_packets;
	stats->ibytes = bgx_stats.rx_bytes;
	stats->imissed = bgx_stats.rx_dropped;
	stats->ierrors = bgx_stats.rx_errors;
	stats->opackets = bgx_stats.tx_packets;
	stats->obytes = bgx_stats.tx_bytes;
	stats->oerrors = bgx_stats.tx_errors;

	octeontx_log_dbg("port%d stats inpkts=%" PRIx64 " outpkts=%" PRIx64 "",
			nic->port_id, stats->ipackets, stats->opackets);

	return 0;
}

static int
octeontx_port_stats_clr(struct octeontx_nic *nic)
{
	PMD_INIT_FUNC_TRACE();

	return octeontx_bgx_port_stats_clr(nic->port_id);
}

static inline void
devconf_set_default_sane_values(struct rte_event_dev_config *dev_conf,
				struct rte_event_dev_info *info)
{
	memset(dev_conf, 0, sizeof(struct rte_event_dev_config));
	dev_conf->dequeue_timeout_ns = info->min_dequeue_timeout_ns;

	dev_conf->nb_event_ports = info->max_event_ports;
	dev_conf->nb_event_queues = info->max_event_queues;

	dev_conf->nb_event_queue_flows = info->max_event_queue_flows;
	dev_conf->nb_event_port_dequeue_depth =
			info->max_event_port_dequeue_depth;
	dev_conf->nb_event_port_enqueue_depth =
			info->max_event_port_enqueue_depth;
	dev_conf->nb_event_port_enqueue_depth =
			info->max_event_port_enqueue_depth;
	dev_conf->nb_events_limit =
			info->max_num_events;
}

static uint16_t
octeontx_tx_offload_flags(struct rte_eth_dev *eth_dev)
{
	struct octeontx_nic *nic = octeontx_pmd_priv(eth_dev);
	uint16_t flags = 0;

	if (nic->tx_offloads & RTE_ETH_TX_OFFLOAD_OUTER_IPV4_CKSUM ||
	    nic->tx_offloads & RTE_ETH_TX_OFFLOAD_OUTER_UDP_CKSUM)
		flags |= OCCTX_TX_OFFLOAD_OL3_OL4_CSUM_F;

	if (nic->tx_offloads & RTE_ETH_TX_OFFLOAD_IPV4_CKSUM ||
	    nic->tx_offloads & RTE_ETH_TX_OFFLOAD_TCP_CKSUM ||
	    nic->tx_offloads & RTE_ETH_TX_OFFLOAD_UDP_CKSUM ||
	    nic->tx_offloads & RTE_ETH_TX_OFFLOAD_SCTP_CKSUM)
		flags |= OCCTX_TX_OFFLOAD_L3_L4_CSUM_F;

	if (!(nic->tx_offloads & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE))
		flags |= OCCTX_TX_OFFLOAD_MBUF_NOFF_F;

	if (nic->tx_offloads & RTE_ETH_TX_OFFLOAD_MULTI_SEGS)
		flags |= OCCTX_TX_MULTI_SEG_F;

	return flags;
}

static uint16_t
octeontx_rx_offload_flags(struct rte_eth_dev *eth_dev)
{
	struct octeontx_nic *nic = octeontx_pmd_priv(eth_dev);
	uint16_t flags = 0;

	if (nic->rx_offloads & (RTE_ETH_RX_OFFLOAD_TCP_CKSUM |
			 RTE_ETH_RX_OFFLOAD_UDP_CKSUM))
		flags |= OCCTX_RX_OFFLOAD_CSUM_F;

	if (nic->rx_offloads & (RTE_ETH_RX_OFFLOAD_IPV4_CKSUM |
				RTE_ETH_RX_OFFLOAD_OUTER_IPV4_CKSUM))
		flags |= OCCTX_RX_OFFLOAD_CSUM_F;

	if (nic->rx_offloads & RTE_ETH_RX_OFFLOAD_SCATTER) {
		flags |= OCCTX_RX_MULTI_SEG_F;
		eth_dev->data->scattered_rx = 1;
		/* If scatter mode is enabled, TX should also be in multi
		 * seg mode, else memory leak will occur
		 */
		nic->tx_offloads |= RTE_ETH_TX_OFFLOAD_MULTI_SEGS;
	}

	return flags;
}

static int
octeontx_dev_configure(struct rte_eth_dev *dev)
{
	struct rte_eth_dev_data *data = dev->data;
	struct rte_eth_conf *conf = &data->dev_conf;
	struct rte_eth_rxmode *rxmode = &conf->rxmode;
	struct rte_eth_txmode *txmode = &conf->txmode;
	struct octeontx_nic *nic = octeontx_pmd_priv(dev);
	int ret;

	PMD_INIT_FUNC_TRACE();
	RTE_SET_USED(conf);

	if (!rte_eal_has_hugepages()) {
		octeontx_log_err("huge page is not configured");
		return -EINVAL;
	}

	if (txmode->mq_mode) {
		octeontx_log_err("tx mq_mode DCB or VMDq not supported");
		return -EINVAL;
	}

	if (rxmode->mq_mode != RTE_ETH_MQ_RX_NONE &&
		rxmode->mq_mode != RTE_ETH_MQ_RX_RSS) {
		octeontx_log_err("unsupported rx qmode %d", rxmode->mq_mode);
		return -EINVAL;
	}

	if (!(txmode->offloads & RTE_ETH_TX_OFFLOAD_MT_LOCKFREE)) {
		PMD_INIT_LOG(NOTICE, "cant disable lockfree tx");
		txmode->offloads |= RTE_ETH_TX_OFFLOAD_MT_LOCKFREE;
	}

	if (conf->link_speeds & RTE_ETH_LINK_SPEED_FIXED) {
		octeontx_log_err("setting link speed/duplex not supported");
		return -EINVAL;
	}

	if (conf->dcb_capability_en) {
		octeontx_log_err("DCB enable not supported");
		return -EINVAL;
	}

	if (conf->fdir_conf.mode != RTE_FDIR_MODE_NONE) {
		octeontx_log_err("flow director not supported");
		return -EINVAL;
	}

	nic->num_tx_queues = dev->data->nb_tx_queues;

	ret = octeontx_pko_channel_open(nic->pko_vfid * PKO_VF_NUM_DQ,
					nic->num_tx_queues,
					nic->base_ochan);
	if (ret) {
		octeontx_log_err("failed to open channel %d no-of-txq %d",
			   nic->base_ochan, nic->num_tx_queues);
		return -EFAULT;
	}

	ret = octeontx_dev_vlan_offload_init(dev);
	if (ret) {
		octeontx_log_err("failed to initialize vlan offload");
		return -EFAULT;
	}

	nic->pki.classifier_enable = false;
	nic->pki.hash_enable = true;
	nic->pki.initialized = false;

	nic->rx_offloads |= rxmode->offloads;
	nic->tx_offloads |= txmode->offloads;
	nic->rx_offload_flags |= octeontx_rx_offload_flags(dev);
	nic->tx_offload_flags |= octeontx_tx_offload_flags(dev);

	return 0;
}

static int
octeontx_dev_close(struct rte_eth_dev *dev)
{
	struct octeontx_txq *txq = NULL;
	struct octeontx_nic *nic = octeontx_pmd_priv(dev);
	unsigned int i;
	int ret;

	PMD_INIT_FUNC_TRACE();
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	/* Stopping/closing event device once all eth ports are closed. */
	if (__atomic_sub_fetch(&evdev_refcnt, 1, __ATOMIC_ACQUIRE) == 0) {
		rte_event_dev_stop(nic->evdev);
		rte_event_dev_close(nic->evdev);
	}

	octeontx_dev_flow_ctrl_fini(dev);

	octeontx_dev_vlan_offload_fini(dev);

	ret = octeontx_pko_channel_close(nic->base_ochan);
	if (ret < 0) {
		octeontx_log_err("failed to close channel %d VF%d %d %d",
			     nic->base_ochan, nic->port_id, nic->num_tx_queues,
			     ret);
	}
	/* Free txq resources for this port */
	for (i = 0; i < nic->num_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		if (!txq)
			continue;

		rte_free(txq);
	}

	octeontx_port_close(nic);

	return 0;
}

static int
octeontx_dev_mtu_set(struct rte_eth_dev *eth_dev, uint16_t mtu)
{
	uint32_t buffsz, frame_size = mtu + OCCTX_L2_OVERHEAD;
	struct octeontx_nic *nic = octeontx_pmd_priv(eth_dev);
	struct rte_eth_dev_data *data = eth_dev->data;
	int rc = 0;

	buffsz = data->min_rx_buf_size - RTE_PKTMBUF_HEADROOM;

	/* Refuse MTU that requires the support of scattered packets
	 * when this feature has not been enabled before.
	 */
	if (data->dev_started && frame_size > buffsz &&
	    !(nic->rx_offloads & RTE_ETH_RX_OFFLOAD_SCATTER)) {
		octeontx_log_err("Scatter mode is disabled");
		return -EINVAL;
	}

	/* Check <seg size> * <max_seg>  >= max_frame */
	if ((nic->rx_offloads & RTE_ETH_RX_OFFLOAD_SCATTER)	&&
	    (frame_size > buffsz * OCCTX_RX_NB_SEG_MAX))
		return -EINVAL;

	rc = octeontx_pko_send_mtu(nic->port_id, frame_size);
	if (rc)
		return rc;

	rc = octeontx_bgx_port_mtu_set(nic->port_id, frame_size);
	if (rc)
		return rc;

	octeontx_log_info("Received pkt beyond  maxlen %d will be dropped",
			  frame_size);

	return rc;
}

static int
octeontx_recheck_rx_offloads(struct octeontx_rxq *rxq)
{
	struct rte_eth_dev *eth_dev = rxq->eth_dev;
	struct octeontx_nic *nic = octeontx_pmd_priv(eth_dev);
	struct rte_eth_dev_data *data = eth_dev->data;
	struct rte_pktmbuf_pool_private *mbp_priv;
	struct evdev_priv_data *evdev_priv;
	struct rte_eventdev *dev;
	uint32_t buffsz;

	/* Get rx buffer size */
	mbp_priv = rte_mempool_get_priv(rxq->pool);
	buffsz = mbp_priv->mbuf_data_room_size - RTE_PKTMBUF_HEADROOM;

	/* Setup scatter mode if needed by jumbo */
	if (data->mtu > buffsz) {
		nic->rx_offloads |= RTE_ETH_RX_OFFLOAD_SCATTER;
		nic->rx_offload_flags |= octeontx_rx_offload_flags(eth_dev);
		nic->tx_offload_flags |= octeontx_tx_offload_flags(eth_dev);
	}

	/* Sharing offload flags via eventdev priv region */
	dev = &rte_eventdevs[rxq->evdev];
	evdev_priv = dev->data->dev_private;
	evdev_priv->rx_offload_flags = nic->rx_offload_flags;
	evdev_priv->tx_offload_flags = nic->tx_offload_flags;

	/* Setup MTU */
	nic->mtu = data->mtu;

	return 0;
}

static int
octeontx_dev_start(struct rte_eth_dev *dev)
{
	struct octeontx_nic *nic = octeontx_pmd_priv(dev);
	struct octeontx_rxq *rxq;
	int ret, i;

	PMD_INIT_FUNC_TRACE();
	/* Rechecking if any new offload set to update
	 * rx/tx burst function pointer accordingly.
	 */
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];
		octeontx_recheck_rx_offloads(rxq);
	}

	/* Setting up the mtu */
	ret = octeontx_dev_mtu_set(dev, nic->mtu);
	if (ret) {
		octeontx_log_err("Failed to set default MTU size %d", ret);
		goto error;
	}

	/*
	 * Tx start
	 */
	octeontx_set_tx_function(dev);
	ret = octeontx_pko_channel_start(nic->base_ochan);
	if (ret < 0) {
		octeontx_log_err("fail to conf VF%d no. txq %d chan %d ret %d",
			   nic->port_id, nic->num_tx_queues, nic->base_ochan,
			   ret);
		goto error;
	}

	/*
	 * Rx start
	 */
	dev->rx_pkt_burst = octeontx_recv_pkts;
	ret = octeontx_pki_port_start(nic->port_id);
	if (ret < 0) {
		octeontx_log_err("fail to start Rx on port %d", nic->port_id);
		goto channel_stop_error;
	}

	/*
	 * Start port
	 */
	ret = octeontx_port_start(nic);
	if (ret < 0) {
		octeontx_log_err("failed start port %d", ret);
		goto pki_port_stop_error;
	}

	PMD_TX_LOG(DEBUG, "pko: start channel %d no.of txq %d port %d",
			nic->base_ochan, nic->num_tx_queues, nic->port_id);

	ret = rte_event_dev_start(nic->evdev);
	if (ret < 0) {
		octeontx_log_err("failed to start evdev: ret (%d)", ret);
		goto pki_port_stop_error;
	}

	/* Success */
	return ret;

pki_port_stop_error:
	octeontx_pki_port_stop(nic->port_id);
channel_stop_error:
	octeontx_pko_channel_stop(nic->base_ochan);
error:
	return ret;
}

static int
octeontx_dev_stop(struct rte_eth_dev *dev)
{
	struct octeontx_nic *nic = octeontx_pmd_priv(dev);
	int ret;

	PMD_INIT_FUNC_TRACE();

	ret = octeontx_port_stop(nic);
	if (ret < 0) {
		octeontx_log_err("failed to req stop port %d res=%d",
					nic->port_id, ret);
		return ret;
	}

	ret = octeontx_pki_port_stop(nic->port_id);
	if (ret < 0) {
		octeontx_log_err("failed to stop pki port %d res=%d",
					nic->port_id, ret);
		return ret;
	}

	ret = octeontx_pko_channel_stop(nic->base_ochan);
	if (ret < 0) {
		octeontx_log_err("failed to stop channel %d VF%d %d %d",
			     nic->base_ochan, nic->port_id, nic->num_tx_queues,
			     ret);
		return ret;
	}

	return 0;
}

static int
octeontx_dev_promisc_enable(struct rte_eth_dev *dev)
{
	struct octeontx_nic *nic = octeontx_pmd_priv(dev);

	PMD_INIT_FUNC_TRACE();
	return octeontx_port_promisc_set(nic, 1);
}

static int
octeontx_dev_promisc_disable(struct rte_eth_dev *dev)
{
	struct octeontx_nic *nic = octeontx_pmd_priv(dev);

	PMD_INIT_FUNC_TRACE();
	return octeontx_port_promisc_set(nic, 0);
}

static int
octeontx_port_link_status(struct octeontx_nic *nic)
{
	int res;

	PMD_INIT_FUNC_TRACE();
	res = octeontx_bgx_port_link_status(nic->port_id);
	if (res < 0) {
		octeontx_log_err("failed to get port %d link status",
				nic->port_id);
		return res;
	}

	if (nic->link_up != (uint8_t)res || nic->print_flag == -1) {
		nic->link_up = (uint8_t)res;
		nic->print_flag = 1;
	}
	octeontx_log_dbg("port %d link status %d", nic->port_id, nic->link_up);

	return res;
}

/*
 * Return 0 means link status changed, -1 means not changed
 */
static int
octeontx_dev_link_update(struct rte_eth_dev *dev,
			 int wait_to_complete __rte_unused)
{
	struct octeontx_nic *nic = octeontx_pmd_priv(dev);
	struct rte_eth_link link;
	int res;

	PMD_INIT_FUNC_TRACE();

	res = octeontx_port_link_status(nic);
	if (res < 0) {
		octeontx_log_err("failed to request link status %d", res);
		return res;
	}

	octeontx_link_status_update(nic, &link);
	if (nic->print_flag) {
		octeontx_link_status_print(nic->dev, &link);
		nic->print_flag = 0;
	}

	return rte_eth_linkstatus_set(dev, &link);
}

static int
octeontx_dev_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	struct octeontx_nic *nic = octeontx_pmd_priv(dev);

	PMD_INIT_FUNC_TRACE();
	return octeontx_port_stats(nic, stats);
}

static int
octeontx_dev_stats_reset(struct rte_eth_dev *dev)
{
	struct octeontx_nic *nic = octeontx_pmd_priv(dev);

	PMD_INIT_FUNC_TRACE();
	return octeontx_port_stats_clr(nic);
}

static void
octeontx_dev_mac_addr_del(struct rte_eth_dev *dev, uint32_t index)
{
	struct octeontx_nic *nic = octeontx_pmd_priv(dev);
	int ret;

	ret = octeontx_bgx_port_mac_del(nic->port_id, index);
	if (ret != 0)
		octeontx_log_err("failed to del MAC address filter on port %d",
				 nic->port_id);
}

static int
octeontx_dev_mac_addr_add(struct rte_eth_dev *dev,
			  struct rte_ether_addr *mac_addr,
			  uint32_t index,
			  __rte_unused uint32_t vmdq)
{
	struct octeontx_nic *nic = octeontx_pmd_priv(dev);
	int ret;

	ret = octeontx_bgx_port_mac_add(nic->port_id, mac_addr->addr_bytes,
					index);
	if (ret < 0) {
		octeontx_log_err("failed to add MAC address filter on port %d",
				 nic->port_id);
		return ret;
	}

	return 0;
}

static int
octeontx_dev_default_mac_addr_set(struct rte_eth_dev *dev,
					struct rte_ether_addr *addr)
{
	struct octeontx_nic *nic = octeontx_pmd_priv(dev);
	int ret;

	ret = octeontx_bgx_port_mac_set(nic->port_id, addr->addr_bytes);
	if (ret == 0) {
		/* Update same mac address to BGX CAM table */
		ret = octeontx_bgx_port_mac_add(nic->port_id, addr->addr_bytes,
						0);
	}
	if (ret < 0) {
		octeontx_log_err("failed to set MAC address on port %d",
				 nic->port_id);
	}

	return ret;
}

static int
octeontx_dev_info(struct rte_eth_dev *dev,
		struct rte_eth_dev_info *dev_info)
{
	struct octeontx_nic *nic = octeontx_pmd_priv(dev);

	/* Autonegotiation may be disabled */
	dev_info->speed_capa = RTE_ETH_LINK_SPEED_FIXED;
	dev_info->speed_capa |= RTE_ETH_LINK_SPEED_10M | RTE_ETH_LINK_SPEED_100M |
			RTE_ETH_LINK_SPEED_1G | RTE_ETH_LINK_SPEED_10G |
			RTE_ETH_LINK_SPEED_40G;

	/* Min/Max MTU supported */
	dev_info->min_rx_bufsize = OCCTX_MIN_FRS;
	dev_info->max_rx_pktlen = OCCTX_MAX_FRS;
	dev_info->max_mtu = dev_info->max_rx_pktlen - OCCTX_L2_OVERHEAD;
	dev_info->min_mtu = dev_info->min_rx_bufsize - OCCTX_L2_OVERHEAD;

	dev_info->max_mac_addrs =
				octeontx_bgx_port_mac_entries_get(nic->port_id);
	dev_info->max_rx_queues = 1;
	dev_info->max_tx_queues = PKO_MAX_NUM_DQ;
	dev_info->min_rx_bufsize = 0;

	dev_info->default_rxconf = (struct rte_eth_rxconf) {
		.rx_free_thresh = 0,
		.rx_drop_en = 0,
		.offloads = OCTEONTX_RX_OFFLOADS,
	};

	dev_info->default_txconf = (struct rte_eth_txconf) {
		.tx_free_thresh = 0,
		.offloads = OCTEONTX_TX_OFFLOADS,
	};

	dev_info->rx_offload_capa = OCTEONTX_RX_OFFLOADS;
	dev_info->tx_offload_capa = OCTEONTX_TX_OFFLOADS;
	dev_info->rx_queue_offload_capa = OCTEONTX_RX_OFFLOADS;
	dev_info->tx_queue_offload_capa = OCTEONTX_TX_OFFLOADS;

	return 0;
}

static void
octeontx_dq_info_getter(octeontx_dq_t *dq, void *out)
{
	((octeontx_dq_t *)out)->lmtline_va = dq->lmtline_va;
	((octeontx_dq_t *)out)->ioreg_va = dq->ioreg_va;
	((octeontx_dq_t *)out)->fc_status_va = dq->fc_status_va;
}

static int
octeontx_vf_start_tx_queue(struct rte_eth_dev *dev, struct octeontx_nic *nic,
				uint16_t qidx)
{
	struct octeontx_txq *txq;
	int res;

	PMD_INIT_FUNC_TRACE();

	if (dev->data->tx_queue_state[qidx] == RTE_ETH_QUEUE_STATE_STARTED)
		return 0;

	txq = dev->data->tx_queues[qidx];

	res = octeontx_pko_channel_query_dqs(nic->base_ochan,
						&txq->dq,
						sizeof(octeontx_dq_t),
						txq->queue_id,
						octeontx_dq_info_getter);
	if (res < 0) {
		res = -EFAULT;
		goto close_port;
	}

	dev->data->tx_queue_state[qidx] = RTE_ETH_QUEUE_STATE_STARTED;
	return res;

close_port:
	(void)octeontx_port_stop(nic);
	octeontx_pko_channel_stop(nic->base_ochan);
	octeontx_pko_channel_close(nic->base_ochan);
	dev->data->tx_queue_state[qidx] = RTE_ETH_QUEUE_STATE_STOPPED;
	return res;
}

int
octeontx_dev_tx_queue_start(struct rte_eth_dev *dev, uint16_t qidx)
{
	struct octeontx_nic *nic = octeontx_pmd_priv(dev);

	PMD_INIT_FUNC_TRACE();
	qidx = qidx % PKO_VF_NUM_DQ;
	return octeontx_vf_start_tx_queue(dev, nic, qidx);
}

static inline int
octeontx_vf_stop_tx_queue(struct rte_eth_dev *dev, struct octeontx_nic *nic,
			  uint16_t qidx)
{
	int ret = 0;

	RTE_SET_USED(nic);
	PMD_INIT_FUNC_TRACE();

	if (dev->data->tx_queue_state[qidx] == RTE_ETH_QUEUE_STATE_STOPPED)
		return 0;

	dev->data->tx_queue_state[qidx] = RTE_ETH_QUEUE_STATE_STOPPED;
	return ret;
}

int
octeontx_dev_tx_queue_stop(struct rte_eth_dev *dev, uint16_t qidx)
{
	struct octeontx_nic *nic = octeontx_pmd_priv(dev);

	PMD_INIT_FUNC_TRACE();
	qidx = qidx % PKO_VF_NUM_DQ;

	return octeontx_vf_stop_tx_queue(dev, nic, qidx);
}

static void
octeontx_dev_tx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	int res;

	PMD_INIT_FUNC_TRACE();

	if (dev->data->tx_queues[qid]) {
		res = octeontx_dev_tx_queue_stop(dev, qid);
		if (res < 0)
			octeontx_log_err("failed stop tx_queue(%d)\n", qid);

		rte_free(dev->data->tx_queues[qid]);
	}
}

static int
octeontx_dev_tx_queue_setup(struct rte_eth_dev *dev, uint16_t qidx,
			    uint16_t nb_desc, unsigned int socket_id,
			    const struct rte_eth_txconf *tx_conf __rte_unused)
{
	struct octeontx_nic *nic = octeontx_pmd_priv(dev);
	struct octeontx_txq *txq = NULL;
	uint16_t dq_num;
	int res = 0;

	RTE_SET_USED(nb_desc);
	RTE_SET_USED(socket_id);

	dq_num = (nic->pko_vfid * PKO_VF_NUM_DQ) + qidx;

	/* Socket id check */
	if (socket_id != (unsigned int)SOCKET_ID_ANY &&
			socket_id != (unsigned int)nic->node)
		PMD_TX_LOG(INFO, "socket_id expected %d, configured %d",
						socket_id, nic->node);

	/* Free memory prior to re-allocation if needed. */
	if (dev->data->tx_queues[qidx] != NULL) {
		PMD_TX_LOG(DEBUG, "freeing memory prior to re-allocation %d",
				qidx);
		octeontx_dev_tx_queue_release(dev, qidx);
		dev->data->tx_queues[qidx] = NULL;
	}

	/* Allocating tx queue data structure */
	txq = rte_zmalloc_socket("ethdev TX queue", sizeof(struct octeontx_txq),
				 RTE_CACHE_LINE_SIZE, nic->node);
	if (txq == NULL) {
		octeontx_log_err("failed to allocate txq=%d", qidx);
		res = -ENOMEM;
		goto err;
	}

	txq->eth_dev = dev;
	txq->queue_id = dq_num;
	dev->data->tx_queues[qidx] = txq;
	dev->data->tx_queue_state[qidx] = RTE_ETH_QUEUE_STATE_STOPPED;

	res = octeontx_pko_channel_query_dqs(nic->base_ochan,
						&txq->dq,
						sizeof(octeontx_dq_t),
						txq->queue_id,
						octeontx_dq_info_getter);
	if (res < 0) {
		res = -EFAULT;
		goto err;
	}

	PMD_TX_LOG(DEBUG, "[%d]:[%d] txq=%p nb_desc=%d lmtline=%p ioreg_va=%p fc_status_va=%p",
			qidx, txq->queue_id, txq, nb_desc, txq->dq.lmtline_va,
			txq->dq.ioreg_va,
			txq->dq.fc_status_va);

	return res;

err:
	if (txq)
		rte_free(txq);

	return res;
}

static int
octeontx_dev_rx_queue_setup(struct rte_eth_dev *dev, uint16_t qidx,
				uint16_t nb_desc, unsigned int socket_id,
				const struct rte_eth_rxconf *rx_conf,
				struct rte_mempool *mb_pool)
{
	struct octeontx_nic *nic = octeontx_pmd_priv(dev);
	struct rte_mempool_ops *mp_ops = NULL;
	struct octeontx_rxq *rxq = NULL;
	pki_pktbuf_cfg_t pktbuf_conf;
	pki_hash_cfg_t pki_hash;
	pki_qos_cfg_t pki_qos;
	uintptr_t pool;
	int ret, port;
	uint16_t gaura;
	unsigned int ev_queues = (nic->ev_queues * nic->port_id) + qidx;
	unsigned int ev_ports = (nic->ev_ports * nic->port_id) + qidx;

	RTE_SET_USED(nb_desc);

	memset(&pktbuf_conf, 0, sizeof(pktbuf_conf));
	memset(&pki_hash, 0, sizeof(pki_hash));
	memset(&pki_qos, 0, sizeof(pki_qos));

	mp_ops = rte_mempool_get_ops(mb_pool->ops_index);
	if (strcmp(mp_ops->name, "octeontx_fpavf")) {
		octeontx_log_err("failed to find octeontx_fpavf mempool");
		return -ENOTSUP;
	}

	/* Handle forbidden configurations */
	if (nic->pki.classifier_enable) {
		octeontx_log_err("cannot setup queue %d. "
					"Classifier option unsupported", qidx);
		return -EINVAL;
	}

	port = nic->port_id;

	/* Rx deferred start is not supported */
	if (rx_conf->rx_deferred_start) {
		octeontx_log_err("rx deferred start not supported");
		return -EINVAL;
	}

	/* Verify queue index */
	if (qidx >= dev->data->nb_rx_queues) {
		octeontx_log_err("QID %d not supported (0 - %d available)\n",
				qidx, (dev->data->nb_rx_queues - 1));
		return -ENOTSUP;
	}

	/* Socket id check */
	if (socket_id != (unsigned int)SOCKET_ID_ANY &&
			socket_id != (unsigned int)nic->node)
		PMD_RX_LOG(INFO, "socket_id expected %d, configured %d",
						socket_id, nic->node);

	/* Allocating rx queue data structure */
	rxq = rte_zmalloc_socket("ethdev RX queue", sizeof(struct octeontx_rxq),
				 RTE_CACHE_LINE_SIZE, nic->node);
	if (rxq == NULL) {
		octeontx_log_err("failed to allocate rxq=%d", qidx);
		return -ENOMEM;
	}

	if (!nic->pki.initialized) {
		pktbuf_conf.port_type = 0;
		pki_hash.port_type = 0;
		pki_qos.port_type = 0;

		pktbuf_conf.mmask.f_wqe_skip = 1;
		pktbuf_conf.mmask.f_first_skip = 1;
		pktbuf_conf.mmask.f_later_skip = 1;
		pktbuf_conf.mmask.f_mbuff_size = 1;
		pktbuf_conf.mmask.f_cache_mode = 1;

		pktbuf_conf.wqe_skip = OCTTX_PACKET_WQE_SKIP;
		pktbuf_conf.first_skip = OCTTX_PACKET_FIRST_SKIP(mb_pool);
		pktbuf_conf.later_skip = OCTTX_PACKET_LATER_SKIP;
		pktbuf_conf.mbuff_size = (mb_pool->elt_size -
					RTE_PKTMBUF_HEADROOM -
					rte_pktmbuf_priv_size(mb_pool) -
					sizeof(struct rte_mbuf));

		pktbuf_conf.cache_mode = PKI_OPC_MODE_STF2_STT;

		ret = octeontx_pki_port_pktbuf_config(port, &pktbuf_conf);
		if (ret != 0) {
			octeontx_log_err("fail to configure pktbuf for port %d",
					port);
			rte_free(rxq);
			return ret;
		}
		PMD_RX_LOG(DEBUG, "Port %d Rx pktbuf configured:\n"
				"\tmbuf_size:\t0x%0x\n"
				"\twqe_skip:\t0x%0x\n"
				"\tfirst_skip:\t0x%0x\n"
				"\tlater_skip:\t0x%0x\n"
				"\tcache_mode:\t%s\n",
				port,
				pktbuf_conf.mbuff_size,
				pktbuf_conf.wqe_skip,
				pktbuf_conf.first_skip,
				pktbuf_conf.later_skip,
				(pktbuf_conf.cache_mode ==
						PKI_OPC_MODE_STT) ?
				"STT" :
				(pktbuf_conf.cache_mode ==
						PKI_OPC_MODE_STF) ?
				"STF" :
				(pktbuf_conf.cache_mode ==
						PKI_OPC_MODE_STF1_STT) ?
				"STF1_STT" : "STF2_STT");

		if (nic->pki.hash_enable) {
			pki_hash.tag_dlc = 1;
			pki_hash.tag_slc = 1;
			pki_hash.tag_dlf = 1;
			pki_hash.tag_slf = 1;
			pki_hash.tag_prt = 1;
			octeontx_pki_port_hash_config(port, &pki_hash);
		}

		pool = (uintptr_t)mb_pool->pool_id;

		/* Get the gaura Id */
		gaura = octeontx_fpa_bufpool_gaura(pool);

		pki_qos.qpg_qos = PKI_QPG_QOS_NONE;
		pki_qos.num_entry = 1;
		pki_qos.drop_policy = 0;
		pki_qos.tag_type = 0L;
		pki_qos.qos_entry[0].port_add = 0;
		pki_qos.qos_entry[0].gaura = gaura;
		pki_qos.qos_entry[0].ggrp_ok = ev_queues;
		pki_qos.qos_entry[0].ggrp_bad = ev_queues;
		pki_qos.qos_entry[0].grptag_bad = 0;
		pki_qos.qos_entry[0].grptag_ok = 0;

		ret = octeontx_pki_port_create_qos(port, &pki_qos);
		if (ret < 0) {
			octeontx_log_err("failed to create QOS port=%d, q=%d",
					port, qidx);
			rte_free(rxq);
			return ret;
		}
		nic->pki.initialized = true;
	}

	rxq->port_id = nic->port_id;
	rxq->eth_dev = dev;
	rxq->queue_id = qidx;
	rxq->evdev = nic->evdev;
	rxq->ev_queues = ev_queues;
	rxq->ev_ports = ev_ports;
	rxq->pool = mb_pool;

	octeontx_recheck_rx_offloads(rxq);
	dev->data->rx_queues[qidx] = rxq;
	dev->data->rx_queue_state[qidx] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

static void
octeontx_dev_rx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	rte_free(dev->data->rx_queues[qid]);
}

static const uint32_t *
octeontx_dev_supported_ptypes_get(struct rte_eth_dev *dev)
{
	static const uint32_t ptypes[] = {
		RTE_PTYPE_L3_IPV4,
		RTE_PTYPE_L3_IPV4_EXT,
		RTE_PTYPE_L3_IPV6,
		RTE_PTYPE_L3_IPV6_EXT,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_L4_FRAG,
		RTE_PTYPE_UNKNOWN
	};

	if (dev->rx_pkt_burst == octeontx_recv_pkts)
		return ptypes;

	return NULL;
}

static int
octeontx_pool_ops(struct rte_eth_dev *dev, const char *pool)
{
	RTE_SET_USED(dev);

	if (!strcmp(pool, "octeontx_fpavf"))
		return 0;

	return -ENOTSUP;
}

/* Initialize and register driver with DPDK Application */
static const struct eth_dev_ops octeontx_dev_ops = {
	.dev_configure		 = octeontx_dev_configure,
	.dev_infos_get		 = octeontx_dev_info,
	.dev_close		 = octeontx_dev_close,
	.dev_start		 = octeontx_dev_start,
	.dev_stop		 = octeontx_dev_stop,
	.promiscuous_enable	 = octeontx_dev_promisc_enable,
	.promiscuous_disable	 = octeontx_dev_promisc_disable,
	.link_update		 = octeontx_dev_link_update,
	.stats_get		 = octeontx_dev_stats_get,
	.stats_reset		 = octeontx_dev_stats_reset,
	.mac_addr_remove	 = octeontx_dev_mac_addr_del,
	.mac_addr_add		 = octeontx_dev_mac_addr_add,
	.mac_addr_set		 = octeontx_dev_default_mac_addr_set,
	.vlan_offload_set	 = octeontx_dev_vlan_offload_set,
	.vlan_filter_set	 = octeontx_dev_vlan_filter_set,
	.tx_queue_start		 = octeontx_dev_tx_queue_start,
	.tx_queue_stop		 = octeontx_dev_tx_queue_stop,
	.tx_queue_setup		 = octeontx_dev_tx_queue_setup,
	.tx_queue_release	 = octeontx_dev_tx_queue_release,
	.rx_queue_setup		 = octeontx_dev_rx_queue_setup,
	.rx_queue_release	 = octeontx_dev_rx_queue_release,
	.dev_set_link_up          = octeontx_dev_set_link_up,
	.dev_set_link_down        = octeontx_dev_set_link_down,
	.dev_supported_ptypes_get = octeontx_dev_supported_ptypes_get,
	.mtu_set                 = octeontx_dev_mtu_set,
	.pool_ops_supported      = octeontx_pool_ops,
	.flow_ctrl_get           = octeontx_dev_flow_ctrl_get,
	.flow_ctrl_set           = octeontx_dev_flow_ctrl_set,
};

/* Create Ethdev interface per BGX LMAC ports */
static int
octeontx_create(struct rte_vdev_device *dev, int port, uint8_t evdev,
			int socket_id)
{
	int res;
	size_t pko_vfid;
	char octtx_name[OCTEONTX_MAX_NAME_LEN];
	struct octeontx_nic *nic = NULL;
	struct rte_eth_dev *eth_dev = NULL;
	struct rte_eth_dev_data *data;
	const char *name = rte_vdev_device_name(dev);
	int max_entries;

	PMD_INIT_FUNC_TRACE();

	sprintf(octtx_name, "%s_%d", name, port);
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		eth_dev = rte_eth_dev_attach_secondary(octtx_name);
		if (eth_dev == NULL)
			return -ENODEV;

		eth_dev->dev_ops = &octeontx_dev_ops;
		eth_dev->device = &dev->device;
		octeontx_set_tx_function(eth_dev);
		eth_dev->rx_pkt_burst = octeontx_recv_pkts;
		rte_eth_dev_probing_finish(eth_dev);
		return 0;
	}

	/* Reserve an ethdev entry */
	eth_dev = rte_eth_dev_allocate(octtx_name);
	if (eth_dev == NULL) {
		octeontx_log_err("failed to allocate rte_eth_dev");
		res = -ENOMEM;
		goto err;
	}
	data = eth_dev->data;

	nic = rte_zmalloc_socket(octtx_name, sizeof(*nic), 0, socket_id);
	if (nic == NULL) {
		octeontx_log_err("failed to allocate nic structure");
		res = -ENOMEM;
		goto err;
	}
	data->dev_private = nic;
	pko_vfid = octeontx_pko_get_vfid();

	if (pko_vfid == SIZE_MAX) {
		octeontx_log_err("failed to get pko vfid");
		res = -ENODEV;
		goto err;
	}

	nic->pko_vfid = pko_vfid;
	nic->port_id = port;
	nic->evdev = evdev;
	__atomic_add_fetch(&evdev_refcnt, 1, __ATOMIC_ACQUIRE);

	res = octeontx_port_open(nic);
	if (res < 0)
		goto err;

	/* Rx side port configuration */
	res = octeontx_pki_port_open(port);
	if (res != 0) {
		octeontx_log_err("failed to open PKI port %d", port);
		res = -ENODEV;
		goto err;
	}

	eth_dev->device = &dev->device;
	eth_dev->intr_handle = NULL;
	eth_dev->data->numa_node = dev->device.numa_node;

	data->port_id = eth_dev->data->port_id;

	nic->ev_queues = 1;
	nic->ev_ports = 1;
	nic->print_flag = -1;

	data->dev_link.link_status = RTE_ETH_LINK_DOWN;
	data->dev_started = 0;
	data->promiscuous = 0;
	data->all_multicast = 0;
	data->scattered_rx = 0;

	/* Get maximum number of supported MAC entries */
	max_entries = octeontx_bgx_port_mac_entries_get(nic->port_id);
	if (max_entries < 0) {
		octeontx_log_err("Failed to get max entries for mac addr");
		res = -ENOTSUP;
		goto err;
	}

	data->mac_addrs = rte_zmalloc_socket(octtx_name, max_entries *
					     RTE_ETHER_ADDR_LEN, 0,
							socket_id);
	if (data->mac_addrs == NULL) {
		octeontx_log_err("failed to allocate memory for mac_addrs");
		res = -ENOMEM;
		goto err;
	}

	eth_dev->dev_ops = &octeontx_dev_ops;

	/* Finally save ethdev pointer to the NIC structure */
	nic->dev = eth_dev;

	if (nic->port_id != data->port_id) {
		octeontx_log_err("eth_dev->port_id (%d) is diff to orig (%d)",
				data->port_id, nic->port_id);
		res = -EINVAL;
		goto free_mac_addrs;
	}

	res = rte_eal_alarm_set(OCCTX_INTR_POLL_INTERVAL_MS * 1000,
				octeontx_link_status_poll, nic);
	if (res) {
		octeontx_log_err("Failed to start link polling alarm");
		goto err;
	}

	/* Update port_id mac to eth_dev */
	memcpy(data->mac_addrs, nic->mac_addr, RTE_ETHER_ADDR_LEN);

	/* Update same mac address to BGX CAM table at index 0 */
	octeontx_bgx_port_mac_add(nic->port_id, nic->mac_addr, 0);

	res = octeontx_dev_flow_ctrl_init(eth_dev);
	if (res < 0)
		goto err;

	PMD_INIT_LOG(DEBUG, "ethdev info: ");
	PMD_INIT_LOG(DEBUG, "port %d, port_ena %d ochan %d num_ochan %d tx_q %d",
				nic->port_id, nic->port_ena,
				nic->base_ochan, nic->num_ochans,
				nic->num_tx_queues);
	PMD_INIT_LOG(DEBUG, "speed %d mtu %d", nic->speed, nic->bgx_mtu);

	rte_octeontx_pchan_map[(nic->base_ochan >> 8) & 0x7]
		[(nic->base_ochan >> 4) & 0xF] = data->port_id;

	rte_eth_dev_probing_finish(eth_dev);
	return data->port_id;

free_mac_addrs:
	rte_free(data->mac_addrs);
	data->mac_addrs = NULL;
err:
	if (nic)
		octeontx_port_close(nic);

	rte_eth_dev_release_port(eth_dev);

	return res;
}

/* Un initialize octeontx device */
static int
octeontx_remove(struct rte_vdev_device *dev)
{
	char octtx_name[OCTEONTX_MAX_NAME_LEN];
	struct rte_eth_dev *eth_dev = NULL;
	struct octeontx_nic *nic = NULL;
	int i;

	if (dev == NULL)
		return -EINVAL;

	for (i = 0; i < OCTEONTX_VDEV_DEFAULT_MAX_NR_PORT; i++) {
		sprintf(octtx_name, "eth_octeontx_%d", i);

		eth_dev = rte_eth_dev_allocated(octtx_name);
		if (eth_dev == NULL)
			continue; /* port already released */

		if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
			rte_eth_dev_release_port(eth_dev);
			continue;
		}

		nic = octeontx_pmd_priv(eth_dev);
		rte_event_dev_stop(nic->evdev);
		PMD_INIT_LOG(INFO, "Closing octeontx device %s", octtx_name);
		octeontx_dev_close(eth_dev);
		rte_eth_dev_release_port(eth_dev);
	}

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	/* Free FC resource */
	octeontx_pko_fc_free();

	return 0;
}

/* Initialize octeontx device */
static int
octeontx_probe(struct rte_vdev_device *dev)
{
	const char *dev_name;
	static int probe_once;
	uint8_t socket_id, qlist;
	int tx_vfcnt, port_id, evdev, qnum, pnum, res, i;
	struct rte_event_dev_config dev_conf;
	const char *eventdev_name = "event_octeontx";
	struct rte_event_dev_info info;
	struct rte_eth_dev *eth_dev;

	struct octeontx_vdev_init_params init_params = {
		OCTEONTX_VDEV_DEFAULT_MAX_NR_PORT
	};

	dev_name = rte_vdev_device_name(dev);

	if (rte_eal_process_type() == RTE_PROC_SECONDARY &&
	    strlen(rte_vdev_device_args(dev)) == 0) {
		eth_dev = rte_eth_dev_attach_secondary(dev_name);
		if (!eth_dev) {
			PMD_INIT_LOG(ERR, "Failed to probe %s", dev_name);
			return -1;
		}
		/* TODO: request info from primary to set up Rx and Tx */
		eth_dev->dev_ops = &octeontx_dev_ops;
		eth_dev->device = &dev->device;
		rte_eth_dev_probing_finish(eth_dev);
		return 0;
	}

	res = octeontx_parse_vdev_init_params(&init_params, dev);
	if (res < 0)
		return -EINVAL;

	if (init_params.nr_port > OCTEONTX_VDEV_DEFAULT_MAX_NR_PORT) {
		octeontx_log_err("nr_port (%d) > max (%d)", init_params.nr_port,
				OCTEONTX_VDEV_DEFAULT_MAX_NR_PORT);
		return -ENOTSUP;
	}

	PMD_INIT_LOG(DEBUG, "initializing %s pmd", dev_name);

	socket_id = rte_socket_id();

	tx_vfcnt = octeontx_pko_vf_count();

	if (tx_vfcnt < init_params.nr_port) {
		octeontx_log_err("not enough PKO (%d) for port number (%d)",
				tx_vfcnt, init_params.nr_port);
		return -EINVAL;
	}
	evdev = rte_event_dev_get_dev_id(eventdev_name);
	if (evdev < 0) {
		octeontx_log_err("eventdev %s not found", eventdev_name);
		return -ENODEV;
	}

	res = rte_event_dev_info_get(evdev, &info);
	if (res < 0) {
		octeontx_log_err("failed to eventdev info %d", res);
		return -EINVAL;
	}

	PMD_INIT_LOG(DEBUG, "max_queue %d max_port %d",
			info.max_event_queues, info.max_event_ports);

	if (octeontx_pko_init_fc(tx_vfcnt))
		return -ENOMEM;

	devconf_set_default_sane_values(&dev_conf, &info);
	res = rte_event_dev_configure(evdev, &dev_conf);
	if (res < 0)
		goto parse_error;

	rte_event_dev_attr_get(evdev, RTE_EVENT_DEV_ATTR_PORT_COUNT,
			(uint32_t *)&pnum);
	rte_event_dev_attr_get(evdev, RTE_EVENT_DEV_ATTR_QUEUE_COUNT,
			(uint32_t *)&qnum);
	if (pnum < qnum) {
		octeontx_log_err("too few event ports (%d) for event_q(%d)",
				pnum, qnum);
		res = -EINVAL;
		goto parse_error;
	}

	/* Enable all queues available */
	for (i = 0; i < qnum; i++) {
		res = rte_event_queue_setup(evdev, i, NULL);
		if (res < 0) {
			octeontx_log_err("failed to setup event_q(%d): res %d",
					i, res);
			goto parse_error;
		}
	}

	/* Enable all ports available */
	for (i = 0; i < pnum; i++) {
		res = rte_event_port_setup(evdev, i, NULL);
		if (res < 0) {
			res = -ENODEV;
			octeontx_log_err("failed to setup ev port(%d) res=%d",
						i, res);
			goto parse_error;
		}
	}

	__atomic_store_n(&evdev_refcnt, 0, __ATOMIC_RELEASE);
	/*
	 * Do 1:1 links for ports & queues. All queues would be mapped to
	 * one port. If there are more ports than queues, then some ports
	 * won't be linked to any queue.
	 */
	for (i = 0; i < qnum; i++) {
		/* Link one queue to one event port */
		qlist = i;
		res = rte_event_port_link(evdev, i, &qlist, NULL, 1);
		if (res < 0) {
			res = -ENODEV;
			octeontx_log_err("failed to link port (%d): res=%d",
					i, res);
			goto parse_error;
		}
	}

	/* Create ethdev interface */
	for (i = 0; i < init_params.nr_port; i++) {
		port_id = octeontx_create(dev, i, evdev, socket_id);
		if (port_id < 0) {
			octeontx_log_err("failed to create device %s",
					dev_name);
			res = -ENODEV;
			goto parse_error;
		}

		PMD_INIT_LOG(INFO, "created ethdev %s for port %d", dev_name,
					port_id);
	}

	if (probe_once) {
		octeontx_log_err("interface %s not supported", dev_name);
		octeontx_remove(dev);
		res = -ENOTSUP;
		goto parse_error;
	}
	rte_mbuf_set_platform_mempool_ops("octeontx_fpavf");
	probe_once = 1;

	return 0;

parse_error:
	octeontx_pko_fc_free();
	return res;
}

static struct rte_vdev_driver octeontx_pmd_drv = {
	.probe = octeontx_probe,
	.remove = octeontx_remove,
};

RTE_PMD_REGISTER_VDEV(OCTEONTX_PMD, octeontx_pmd_drv);
RTE_PMD_REGISTER_ALIAS(OCTEONTX_PMD, eth_octeontx);
RTE_PMD_REGISTER_PARAM_STRING(OCTEONTX_PMD, "nr_port=<int> ");
