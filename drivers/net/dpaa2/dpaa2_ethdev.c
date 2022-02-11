/* * SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright (c) 2016 Freescale Semiconductor, Inc. All rights reserved.
 *   Copyright 2016-2021 NXP
 *
 */

#include <time.h>
#include <net/if.h>

#include <rte_mbuf.h>
#include <ethdev_driver.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_string_fns.h>
#include <rte_cycles.h>
#include <rte_kvargs.h>
#include <rte_dev.h>
#include <rte_fslmc.h>
#include <rte_flow_driver.h>
#include "rte_dpaa2_mempool.h"

#include "dpaa2_pmd_logs.h"
#include <fslmc_vfio.h>
#include <dpaa2_hw_pvt.h>
#include <dpaa2_hw_mempool.h>
#include <dpaa2_hw_dpio.h>
#include <mc/fsl_dpmng.h>
#include "dpaa2_ethdev.h"
#include "dpaa2_sparser.h"
#include <fsl_qbman_debug.h>

#define DRIVER_LOOPBACK_MODE "drv_loopback"
#define DRIVER_NO_PREFETCH_MODE "drv_no_prefetch"
#define DRIVER_TX_CONF "drv_tx_conf"
#define DRIVER_ERROR_QUEUE  "drv_err_queue"
#define CHECK_INTERVAL         100  /* 100ms */
#define MAX_REPEAT_TIME        90   /* 9s (90 * 100ms) in total */

/* Supported Rx offloads */
static uint64_t dev_rx_offloads_sup =
		RTE_ETH_RX_OFFLOAD_CHECKSUM |
		RTE_ETH_RX_OFFLOAD_SCTP_CKSUM |
		RTE_ETH_RX_OFFLOAD_OUTER_IPV4_CKSUM |
		RTE_ETH_RX_OFFLOAD_OUTER_UDP_CKSUM |
		RTE_ETH_RX_OFFLOAD_VLAN_STRIP |
		RTE_ETH_RX_OFFLOAD_VLAN_FILTER |
		RTE_ETH_RX_OFFLOAD_TIMESTAMP;

/* Rx offloads which cannot be disabled */
static uint64_t dev_rx_offloads_nodis =
		RTE_ETH_RX_OFFLOAD_RSS_HASH |
		RTE_ETH_RX_OFFLOAD_SCATTER;

/* Supported Tx offloads */
static uint64_t dev_tx_offloads_sup =
		RTE_ETH_TX_OFFLOAD_VLAN_INSERT |
		RTE_ETH_TX_OFFLOAD_IPV4_CKSUM |
		RTE_ETH_TX_OFFLOAD_UDP_CKSUM |
		RTE_ETH_TX_OFFLOAD_TCP_CKSUM |
		RTE_ETH_TX_OFFLOAD_SCTP_CKSUM |
		RTE_ETH_TX_OFFLOAD_OUTER_IPV4_CKSUM |
		RTE_ETH_TX_OFFLOAD_MT_LOCKFREE |
		RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;

/* Tx offloads which cannot be disabled */
static uint64_t dev_tx_offloads_nodis =
		RTE_ETH_TX_OFFLOAD_MULTI_SEGS;

/* enable timestamp in mbuf */
bool dpaa2_enable_ts[RTE_MAX_ETHPORTS];
uint64_t dpaa2_timestamp_rx_dynflag;
int dpaa2_timestamp_dynfield_offset = -1;

/* Enable error queue */
bool dpaa2_enable_err_queue;

#define MAX_NB_RX_DESC		11264
int total_nb_rx_desc;

struct rte_dpaa2_xstats_name_off {
	char name[RTE_ETH_XSTATS_NAME_SIZE];
	uint8_t page_id; /* dpni statistics page id */
	uint8_t stats_id; /* stats id in the given page */
};

static const struct rte_dpaa2_xstats_name_off dpaa2_xstats_strings[] = {
	{"ingress_multicast_frames", 0, 2},
	{"ingress_multicast_bytes", 0, 3},
	{"ingress_broadcast_frames", 0, 4},
	{"ingress_broadcast_bytes", 0, 5},
	{"egress_multicast_frames", 1, 2},
	{"egress_multicast_bytes", 1, 3},
	{"egress_broadcast_frames", 1, 4},
	{"egress_broadcast_bytes", 1, 5},
	{"ingress_filtered_frames", 2, 0},
	{"ingress_discarded_frames", 2, 1},
	{"ingress_nobuffer_discards", 2, 2},
	{"egress_discarded_frames", 2, 3},
	{"egress_confirmed_frames", 2, 4},
	{"cgr_reject_frames", 4, 0},
	{"cgr_reject_bytes", 4, 1},
};

static struct rte_dpaa2_driver rte_dpaa2_pmd;
static int dpaa2_dev_link_update(struct rte_eth_dev *dev,
				 int wait_to_complete);
static int dpaa2_dev_set_link_up(struct rte_eth_dev *dev);
static int dpaa2_dev_set_link_down(struct rte_eth_dev *dev);
static int dpaa2_dev_mtu_set(struct rte_eth_dev *dev, uint16_t mtu);

static int
dpaa2_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vlan_id, int on)
{
	int ret;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = dev->process_private;

	PMD_INIT_FUNC_TRACE();

	if (dpni == NULL) {
		DPAA2_PMD_ERR("dpni is NULL");
		return -1;
	}

	if (on)
		ret = dpni_add_vlan_id(dpni, CMD_PRI_LOW, priv->token,
				       vlan_id, 0, 0, 0);
	else
		ret = dpni_remove_vlan_id(dpni, CMD_PRI_LOW,
					  priv->token, vlan_id);

	if (ret < 0)
		DPAA2_PMD_ERR("ret = %d Unable to add/rem vlan %d hwid =%d",
			      ret, vlan_id, priv->hw_id);

	return ret;
}

static int
dpaa2_vlan_offload_set(struct rte_eth_dev *dev, int mask)
{
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = dev->process_private;
	int ret = 0;

	PMD_INIT_FUNC_TRACE();

	if (mask & RTE_ETH_VLAN_FILTER_MASK) {
		/* VLAN Filter not available */
		if (!priv->max_vlan_filters) {
			DPAA2_PMD_INFO("VLAN filter not available");
			return -ENOTSUP;
		}

		if (dev->data->dev_conf.rxmode.offloads &
			RTE_ETH_RX_OFFLOAD_VLAN_FILTER)
			ret = dpni_enable_vlan_filter(dpni, CMD_PRI_LOW,
						      priv->token, true);
		else
			ret = dpni_enable_vlan_filter(dpni, CMD_PRI_LOW,
						      priv->token, false);
		if (ret < 0)
			DPAA2_PMD_INFO("Unable to set vlan filter = %d", ret);
	}

	return ret;
}

static int
dpaa2_vlan_tpid_set(struct rte_eth_dev *dev,
		      enum rte_vlan_type vlan_type __rte_unused,
		      uint16_t tpid)
{
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = dev->process_private;
	int ret = -ENOTSUP;

	PMD_INIT_FUNC_TRACE();

	/* nothing to be done for standard vlan tpids */
	if (tpid == 0x8100 || tpid == 0x88A8)
		return 0;

	ret = dpni_add_custom_tpid(dpni, CMD_PRI_LOW,
				   priv->token, tpid);
	if (ret < 0)
		DPAA2_PMD_INFO("Unable to set vlan tpid = %d", ret);
	/* if already configured tpids, remove them first */
	if (ret == -EBUSY) {
		struct dpni_custom_tpid_cfg tpid_list = {0};

		ret = dpni_get_custom_tpid(dpni, CMD_PRI_LOW,
				   priv->token, &tpid_list);
		if (ret < 0)
			goto fail;
		ret = dpni_remove_custom_tpid(dpni, CMD_PRI_LOW,
				   priv->token, tpid_list.tpid1);
		if (ret < 0)
			goto fail;
		ret = dpni_add_custom_tpid(dpni, CMD_PRI_LOW,
					   priv->token, tpid);
	}
fail:
	return ret;
}

static int
dpaa2_fw_version_get(struct rte_eth_dev *dev,
		     char *fw_version,
		     size_t fw_size)
{
	int ret;
	struct fsl_mc_io *dpni = dev->process_private;
	struct mc_soc_version mc_plat_info = {0};
	struct mc_version mc_ver_info = {0};

	PMD_INIT_FUNC_TRACE();

	if (mc_get_soc_version(dpni, CMD_PRI_LOW, &mc_plat_info))
		DPAA2_PMD_WARN("\tmc_get_soc_version failed");

	if (mc_get_version(dpni, CMD_PRI_LOW, &mc_ver_info))
		DPAA2_PMD_WARN("\tmc_get_version failed");

	ret = snprintf(fw_version, fw_size,
		       "%x-%d.%d.%d",
		       mc_plat_info.svr,
		       mc_ver_info.major,
		       mc_ver_info.minor,
		       mc_ver_info.revision);
	if (ret < 0)
		return -EINVAL;

	ret += 1; /* add the size of '\0' */
	if (fw_size < (size_t)ret)
		return ret;
	else
		return 0;
}

static int
dpaa2_dev_info_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	struct dpaa2_dev_priv *priv = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();

	dev_info->max_mac_addrs = priv->max_mac_filters;
	dev_info->max_rx_pktlen = DPAA2_MAX_RX_PKT_LEN;
	dev_info->min_rx_bufsize = DPAA2_MIN_RX_BUF_SIZE;
	dev_info->max_rx_queues = (uint16_t)priv->nb_rx_queues;
	dev_info->max_tx_queues = (uint16_t)priv->nb_tx_queues;
	dev_info->rx_offload_capa = dev_rx_offloads_sup |
					dev_rx_offloads_nodis;
	dev_info->tx_offload_capa = dev_tx_offloads_sup |
					dev_tx_offloads_nodis;
	dev_info->speed_capa = RTE_ETH_LINK_SPEED_1G |
			RTE_ETH_LINK_SPEED_2_5G |
			RTE_ETH_LINK_SPEED_10G;
	dev_info->dev_capa &= ~RTE_ETH_DEV_CAPA_FLOW_RULE_KEEP;

	dev_info->max_hash_mac_addrs = 0;
	dev_info->max_vfs = 0;
	dev_info->max_vmdq_pools = RTE_ETH_16_POOLS;
	dev_info->flow_type_rss_offloads = DPAA2_RSS_OFFLOAD_ALL;

	dev_info->default_rxportconf.burst_size = dpaa2_dqrr_size;
	/* same is rx size for best perf */
	dev_info->default_txportconf.burst_size = dpaa2_dqrr_size;

	dev_info->default_rxportconf.nb_queues = 1;
	dev_info->default_txportconf.nb_queues = 1;
	dev_info->default_txportconf.ring_size = CONG_ENTER_TX_THRESHOLD;
	dev_info->default_rxportconf.ring_size = DPAA2_RX_DEFAULT_NBDESC;

	if (dpaa2_svr_family == SVR_LX2160A) {
		dev_info->speed_capa |= RTE_ETH_LINK_SPEED_25G |
				RTE_ETH_LINK_SPEED_40G |
				RTE_ETH_LINK_SPEED_50G |
				RTE_ETH_LINK_SPEED_100G;
	}

	return 0;
}

static int
dpaa2_dev_rx_burst_mode_get(struct rte_eth_dev *dev,
			__rte_unused uint16_t queue_id,
			struct rte_eth_burst_mode *mode)
{
	struct rte_eth_conf *eth_conf = &dev->data->dev_conf;
	int ret = -EINVAL;
	unsigned int i;
	const struct burst_info {
		uint64_t flags;
		const char *output;
	} rx_offload_map[] = {
			{RTE_ETH_RX_OFFLOAD_CHECKSUM, " Checksum,"},
			{RTE_ETH_RX_OFFLOAD_SCTP_CKSUM, " SCTP csum,"},
			{RTE_ETH_RX_OFFLOAD_OUTER_IPV4_CKSUM, " Outer IPV4 csum,"},
			{RTE_ETH_RX_OFFLOAD_OUTER_UDP_CKSUM, " Outer UDP csum,"},
			{RTE_ETH_RX_OFFLOAD_VLAN_STRIP, " VLAN strip,"},
			{RTE_ETH_RX_OFFLOAD_VLAN_FILTER, " VLAN filter,"},
			{RTE_ETH_RX_OFFLOAD_TIMESTAMP, " Timestamp,"},
			{RTE_ETH_RX_OFFLOAD_RSS_HASH, " RSS,"},
			{RTE_ETH_RX_OFFLOAD_SCATTER, " Scattered,"}
	};

	/* Update Rx offload info */
	for (i = 0; i < RTE_DIM(rx_offload_map); i++) {
		if (eth_conf->rxmode.offloads & rx_offload_map[i].flags) {
			snprintf(mode->info, sizeof(mode->info), "%s",
				rx_offload_map[i].output);
			ret = 0;
			break;
		}
	}
	return ret;
}

static int
dpaa2_dev_tx_burst_mode_get(struct rte_eth_dev *dev,
			__rte_unused uint16_t queue_id,
			struct rte_eth_burst_mode *mode)
{
	struct rte_eth_conf *eth_conf = &dev->data->dev_conf;
	int ret = -EINVAL;
	unsigned int i;
	const struct burst_info {
		uint64_t flags;
		const char *output;
	} tx_offload_map[] = {
			{RTE_ETH_TX_OFFLOAD_VLAN_INSERT, " VLAN Insert,"},
			{RTE_ETH_TX_OFFLOAD_IPV4_CKSUM, " IPV4 csum,"},
			{RTE_ETH_TX_OFFLOAD_UDP_CKSUM, " UDP csum,"},
			{RTE_ETH_TX_OFFLOAD_TCP_CKSUM, " TCP csum,"},
			{RTE_ETH_TX_OFFLOAD_SCTP_CKSUM, " SCTP csum,"},
			{RTE_ETH_TX_OFFLOAD_OUTER_IPV4_CKSUM, " Outer IPV4 csum,"},
			{RTE_ETH_TX_OFFLOAD_MT_LOCKFREE, " MT lockfree,"},
			{RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE, " MBUF free disable,"},
			{RTE_ETH_TX_OFFLOAD_MULTI_SEGS, " Scattered,"}
	};

	/* Update Tx offload info */
	for (i = 0; i < RTE_DIM(tx_offload_map); i++) {
		if (eth_conf->txmode.offloads & tx_offload_map[i].flags) {
			snprintf(mode->info, sizeof(mode->info), "%s",
				tx_offload_map[i].output);
			ret = 0;
			break;
		}
	}
	return ret;
}

static int
dpaa2_alloc_rx_tx_queues(struct rte_eth_dev *dev)
{
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	uint16_t dist_idx;
	uint32_t vq_id;
	uint8_t num_rxqueue_per_tc;
	struct dpaa2_queue *mc_q, *mcq;
	uint32_t tot_queues;
	int i;
	struct dpaa2_queue *dpaa2_q;

	PMD_INIT_FUNC_TRACE();

	num_rxqueue_per_tc = (priv->nb_rx_queues / priv->num_rx_tc);
	if (priv->flags & DPAA2_TX_CONF_ENABLE)
		tot_queues = priv->nb_rx_queues + 2 * priv->nb_tx_queues;
	else
		tot_queues = priv->nb_rx_queues + priv->nb_tx_queues;
	mc_q = rte_malloc(NULL, sizeof(struct dpaa2_queue) * tot_queues,
			  RTE_CACHE_LINE_SIZE);
	if (!mc_q) {
		DPAA2_PMD_ERR("Memory allocation failed for rx/tx queues");
		return -1;
	}

	for (i = 0; i < priv->nb_rx_queues; i++) {
		mc_q->eth_data = dev->data;
		priv->rx_vq[i] = mc_q++;
		dpaa2_q = (struct dpaa2_queue *)priv->rx_vq[i];
		dpaa2_q->q_storage = rte_malloc("dq_storage",
					sizeof(struct queue_storage_info_t),
					RTE_CACHE_LINE_SIZE);
		if (!dpaa2_q->q_storage)
			goto fail;

		memset(dpaa2_q->q_storage, 0,
		       sizeof(struct queue_storage_info_t));
		if (dpaa2_alloc_dq_storage(dpaa2_q->q_storage))
			goto fail;
	}

	if (dpaa2_enable_err_queue) {
		priv->rx_err_vq = rte_zmalloc("dpni_rx_err",
			sizeof(struct dpaa2_queue), 0);
		if (!priv->rx_err_vq)
			goto fail;

		dpaa2_q = (struct dpaa2_queue *)priv->rx_err_vq;
		dpaa2_q->q_storage = rte_malloc("err_dq_storage",
					sizeof(struct queue_storage_info_t) *
					RTE_MAX_LCORE,
					RTE_CACHE_LINE_SIZE);
		if (!dpaa2_q->q_storage)
			goto fail;

		memset(dpaa2_q->q_storage, 0,
		       sizeof(struct queue_storage_info_t));
		for (i = 0; i < RTE_MAX_LCORE; i++)
			if (dpaa2_alloc_dq_storage(&dpaa2_q->q_storage[i]))
				goto fail;
	}

	for (i = 0; i < priv->nb_tx_queues; i++) {
		mc_q->eth_data = dev->data;
		mc_q->flow_id = 0xffff;
		priv->tx_vq[i] = mc_q++;
		dpaa2_q = (struct dpaa2_queue *)priv->tx_vq[i];
		dpaa2_q->cscn = rte_malloc(NULL,
					   sizeof(struct qbman_result), 16);
		if (!dpaa2_q->cscn)
			goto fail_tx;
	}

	if (priv->flags & DPAA2_TX_CONF_ENABLE) {
		/*Setup tx confirmation queues*/
		for (i = 0; i < priv->nb_tx_queues; i++) {
			mc_q->eth_data = dev->data;
			mc_q->tc_index = i;
			mc_q->flow_id = 0;
			priv->tx_conf_vq[i] = mc_q++;
			dpaa2_q = (struct dpaa2_queue *)priv->tx_conf_vq[i];
			dpaa2_q->q_storage =
				rte_malloc("dq_storage",
					sizeof(struct queue_storage_info_t),
					RTE_CACHE_LINE_SIZE);
			if (!dpaa2_q->q_storage)
				goto fail_tx_conf;

			memset(dpaa2_q->q_storage, 0,
			       sizeof(struct queue_storage_info_t));
			if (dpaa2_alloc_dq_storage(dpaa2_q->q_storage))
				goto fail_tx_conf;
		}
	}

	vq_id = 0;
	for (dist_idx = 0; dist_idx < priv->nb_rx_queues; dist_idx++) {
		mcq = (struct dpaa2_queue *)priv->rx_vq[vq_id];
		mcq->tc_index = dist_idx / num_rxqueue_per_tc;
		mcq->flow_id = dist_idx % num_rxqueue_per_tc;
		vq_id++;
	}

	return 0;
fail_tx_conf:
	i -= 1;
	while (i >= 0) {
		dpaa2_q = (struct dpaa2_queue *)priv->tx_conf_vq[i];
		rte_free(dpaa2_q->q_storage);
		priv->tx_conf_vq[i--] = NULL;
	}
	i = priv->nb_tx_queues;
fail_tx:
	i -= 1;
	while (i >= 0) {
		dpaa2_q = (struct dpaa2_queue *)priv->tx_vq[i];
		rte_free(dpaa2_q->cscn);
		priv->tx_vq[i--] = NULL;
	}
	i = priv->nb_rx_queues;
fail:
	i -= 1;
	mc_q = priv->rx_vq[0];
	while (i >= 0) {
		dpaa2_q = (struct dpaa2_queue *)priv->rx_vq[i];
		dpaa2_free_dq_storage(dpaa2_q->q_storage);
		rte_free(dpaa2_q->q_storage);
		priv->rx_vq[i--] = NULL;
	}

	if (dpaa2_enable_err_queue) {
		dpaa2_q = (struct dpaa2_queue *)priv->rx_err_vq;
		if (dpaa2_q->q_storage)
			dpaa2_free_dq_storage(dpaa2_q->q_storage);
		rte_free(dpaa2_q->q_storage);
	}

	rte_free(mc_q);
	return -1;
}

static void
dpaa2_free_rx_tx_queues(struct rte_eth_dev *dev)
{
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct dpaa2_queue *dpaa2_q;
	int i;

	PMD_INIT_FUNC_TRACE();

	/* Queue allocation base */
	if (priv->rx_vq[0]) {
		/* cleaning up queue storage */
		for (i = 0; i < priv->nb_rx_queues; i++) {
			dpaa2_q = (struct dpaa2_queue *)priv->rx_vq[i];
			rte_free(dpaa2_q->q_storage);
		}
		/* cleanup tx queue cscn */
		for (i = 0; i < priv->nb_tx_queues; i++) {
			dpaa2_q = (struct dpaa2_queue *)priv->tx_vq[i];
			rte_free(dpaa2_q->cscn);
		}
		if (priv->flags & DPAA2_TX_CONF_ENABLE) {
			/* cleanup tx conf queue storage */
			for (i = 0; i < priv->nb_tx_queues; i++) {
				dpaa2_q = (struct dpaa2_queue *)
						priv->tx_conf_vq[i];
				rte_free(dpaa2_q->q_storage);
			}
		}
		/*free memory for all queues (RX+TX) */
		rte_free(priv->rx_vq[0]);
		priv->rx_vq[0] = NULL;
	}
}

static int
dpaa2_eth_dev_configure(struct rte_eth_dev *dev)
{
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = dev->process_private;
	struct rte_eth_conf *eth_conf = &dev->data->dev_conf;
	uint64_t rx_offloads = eth_conf->rxmode.offloads;
	uint64_t tx_offloads = eth_conf->txmode.offloads;
	int rx_l3_csum_offload = false;
	int rx_l4_csum_offload = false;
	int tx_l3_csum_offload = false;
	int tx_l4_csum_offload = false;
	int ret, tc_index;
	uint32_t max_rx_pktlen;

	PMD_INIT_FUNC_TRACE();

	/* Rx offloads which are enabled by default */
	if (dev_rx_offloads_nodis & ~rx_offloads) {
		DPAA2_PMD_INFO(
		"Some of rx offloads enabled by default - requested 0x%" PRIx64
		" fixed are 0x%" PRIx64,
		rx_offloads, dev_rx_offloads_nodis);
	}

	/* Tx offloads which are enabled by default */
	if (dev_tx_offloads_nodis & ~tx_offloads) {
		DPAA2_PMD_INFO(
		"Some of tx offloads enabled by default - requested 0x%" PRIx64
		" fixed are 0x%" PRIx64,
		tx_offloads, dev_tx_offloads_nodis);
	}

	max_rx_pktlen = eth_conf->rxmode.mtu + RTE_ETHER_HDR_LEN +
				RTE_ETHER_CRC_LEN + VLAN_TAG_SIZE;
	if (max_rx_pktlen <= DPAA2_MAX_RX_PKT_LEN) {
		ret = dpni_set_max_frame_length(dpni, CMD_PRI_LOW,
			priv->token, max_rx_pktlen - RTE_ETHER_CRC_LEN);
		if (ret != 0) {
			DPAA2_PMD_ERR("Unable to set mtu. check config");
			return ret;
		}
		DPAA2_PMD_INFO("MTU configured for the device: %d",
				dev->data->mtu);
	} else {
		return -1;
	}

	if (eth_conf->rxmode.mq_mode == RTE_ETH_MQ_RX_RSS) {
		for (tc_index = 0; tc_index < priv->num_rx_tc; tc_index++) {
			ret = dpaa2_setup_flow_dist(dev,
					eth_conf->rx_adv_conf.rss_conf.rss_hf,
					tc_index);
			if (ret) {
				DPAA2_PMD_ERR(
					"Unable to set flow distribution on tc%d."
					"Check queue config", tc_index);
				return ret;
			}
		}
	}

	if (rx_offloads & RTE_ETH_RX_OFFLOAD_IPV4_CKSUM)
		rx_l3_csum_offload = true;

	if ((rx_offloads & RTE_ETH_RX_OFFLOAD_UDP_CKSUM) ||
		(rx_offloads & RTE_ETH_RX_OFFLOAD_TCP_CKSUM) ||
		(rx_offloads & RTE_ETH_RX_OFFLOAD_SCTP_CKSUM))
		rx_l4_csum_offload = true;

	ret = dpni_set_offload(dpni, CMD_PRI_LOW, priv->token,
			       DPNI_OFF_RX_L3_CSUM, rx_l3_csum_offload);
	if (ret) {
		DPAA2_PMD_ERR("Error to set RX l3 csum:Error = %d", ret);
		return ret;
	}

	ret = dpni_set_offload(dpni, CMD_PRI_LOW, priv->token,
			       DPNI_OFF_RX_L4_CSUM, rx_l4_csum_offload);
	if (ret) {
		DPAA2_PMD_ERR("Error to get RX l4 csum:Error = %d", ret);
		return ret;
	}

#if !defined(RTE_LIBRTE_IEEE1588)
	if (rx_offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP)
#endif
	{
		ret = rte_mbuf_dyn_rx_timestamp_register(
				&dpaa2_timestamp_dynfield_offset,
				&dpaa2_timestamp_rx_dynflag);
		if (ret != 0) {
			DPAA2_PMD_ERR("Error to register timestamp field/flag");
			return -rte_errno;
		}
		dpaa2_enable_ts[dev->data->port_id] = true;
	}

	if (tx_offloads & RTE_ETH_TX_OFFLOAD_IPV4_CKSUM)
		tx_l3_csum_offload = true;

	if ((tx_offloads & RTE_ETH_TX_OFFLOAD_UDP_CKSUM) ||
		(tx_offloads & RTE_ETH_TX_OFFLOAD_TCP_CKSUM) ||
		(tx_offloads & RTE_ETH_TX_OFFLOAD_SCTP_CKSUM))
		tx_l4_csum_offload = true;

	ret = dpni_set_offload(dpni, CMD_PRI_LOW, priv->token,
			       DPNI_OFF_TX_L3_CSUM, tx_l3_csum_offload);
	if (ret) {
		DPAA2_PMD_ERR("Error to set TX l3 csum:Error = %d", ret);
		return ret;
	}

	ret = dpni_set_offload(dpni, CMD_PRI_LOW, priv->token,
			       DPNI_OFF_TX_L4_CSUM, tx_l4_csum_offload);
	if (ret) {
		DPAA2_PMD_ERR("Error to get TX l4 csum:Error = %d", ret);
		return ret;
	}

	/* Enabling hash results in FD requires setting DPNI_FLCTYPE_HASH in
	 * dpni_set_offload API. Setting this FLCTYPE for DPNI sets the FD[SC]
	 * to 0 for LS2 in the hardware thus disabling data/annotation
	 * stashing. For LX2 this is fixed in hardware and thus hash result and
	 * parse results can be received in FD using this option.
	 */
	if (dpaa2_svr_family == SVR_LX2160A) {
		ret = dpni_set_offload(dpni, CMD_PRI_LOW, priv->token,
				       DPNI_FLCTYPE_HASH, true);
		if (ret) {
			DPAA2_PMD_ERR("Error setting FLCTYPE: Err = %d", ret);
			return ret;
		}
	}

	if (rx_offloads & RTE_ETH_RX_OFFLOAD_VLAN_FILTER)
		dpaa2_vlan_offload_set(dev, RTE_ETH_VLAN_FILTER_MASK);

	if (eth_conf->lpbk_mode) {
		ret = dpaa2_dev_recycle_config(dev);
		if (ret) {
			DPAA2_PMD_ERR("Error to configure %s to recycle port.",
				dev->data->name);

			return ret;
		}
	} else {
		/** User may disable loopback mode by calling
		 * "dev_configure" with lpbk_mode cleared.
		 * No matter the port was configured recycle or not,
		 * recycle de-configure is called here.
		 * If port is not recycled, the de-configure will return directly.
		 */
		ret = dpaa2_dev_recycle_deconfig(dev);
		if (ret) {
			DPAA2_PMD_ERR("Error to de-configure recycle port %s.",
				dev->data->name);

			return ret;
		}
	}

	dpaa2_tm_init(dev);

	return 0;
}

/* Function to setup RX flow information. It contains traffic class ID,
 * flow ID, destination configuration etc.
 */
static int
dpaa2_dev_rx_queue_setup(struct rte_eth_dev *dev,
			 uint16_t rx_queue_id,
			 uint16_t nb_rx_desc,
			 unsigned int socket_id __rte_unused,
			 const struct rte_eth_rxconf *rx_conf,
			 struct rte_mempool *mb_pool)
{
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)dev->process_private;
	struct dpaa2_queue *dpaa2_q;
	struct dpni_queue cfg;
	uint8_t options = 0;
	uint8_t flow_id;
	uint32_t bpid;
	int i, ret;

	PMD_INIT_FUNC_TRACE();

	DPAA2_PMD_DEBUG("dev =%p, queue =%d, pool = %p, conf =%p",
			dev, rx_queue_id, mb_pool, rx_conf);

	total_nb_rx_desc += nb_rx_desc;
	if (total_nb_rx_desc > MAX_NB_RX_DESC) {
		DPAA2_PMD_WARN("\nTotal nb_rx_desc exceeds %d limit. Please use Normal buffers",
			       MAX_NB_RX_DESC);
		DPAA2_PMD_WARN("To use Normal buffers, run 'export DPNI_NORMAL_BUF=1' before running dynamic_dpl.sh script");
	}

	/* Rx deferred start is not supported */
	if (rx_conf->rx_deferred_start) {
		DPAA2_PMD_ERR("%p:Rx deferred start not supported",
				(void *)dev);
		return -EINVAL;
	}

	if (!priv->bp_list || priv->bp_list->mp != mb_pool) {
		if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
			ret = rte_dpaa2_bpid_info_init(mb_pool);
			if (ret)
				return ret;
		}
		bpid = mempool_to_bpid(mb_pool);
		ret = dpaa2_attach_bp_list(priv, dpni,
				rte_dpaa2_bpid_info[bpid].bp_list);
		if (ret)
			return ret;
	}
	dpaa2_q = (struct dpaa2_queue *)priv->rx_vq[rx_queue_id];
	dpaa2_q->mb_pool = mb_pool; /**< mbuf pool to populate RX ring. */
	dpaa2_q->bp_array = rte_dpaa2_bpid_info;
	dpaa2_q->nb_desc = UINT16_MAX;
	dpaa2_q->offloads = rx_conf->offloads;

	/*Get the flow id from given VQ id*/
	flow_id = dpaa2_q->flow_id;
	memset(&cfg, 0, sizeof(struct dpni_queue));

	options = options | DPNI_QUEUE_OPT_USER_CTX;
	cfg.user_context = (size_t)(dpaa2_q);

	/* check if a private cgr available. */
	for (i = 0; i < priv->max_cgs; i++) {
		if (!priv->cgid_in_use[i]) {
			priv->cgid_in_use[i] = 1;
			break;
		}
	}

	if (i < priv->max_cgs) {
		options |= DPNI_QUEUE_OPT_SET_CGID;
		cfg.cgid = i;
		dpaa2_q->cgid = cfg.cgid;
	} else {
		dpaa2_q->cgid = 0xff;
	}

	/*if ls2088 or rev2 device, enable the stashing */

	if ((dpaa2_svr_family & 0xffff0000) != SVR_LS2080A) {
		options |= DPNI_QUEUE_OPT_FLC;
		cfg.flc.stash_control = true;
		cfg.flc.value &= 0xFFFFFFFFFFFFFFC0;
		/* 00 00 00 - last 6 bit represent annotation, context stashing,
		 * data stashing setting 01 01 00 (0x14)
		 * (in following order ->DS AS CS)
		 * to enable 1 line data, 1 line annotation.
		 * For LX2, this setting should be 01 00 00 (0x10)
		 */
		if ((dpaa2_svr_family & 0xffff0000) == SVR_LX2160A)
			cfg.flc.value |= 0x10;
		else
			cfg.flc.value |= 0x14;
	}
	ret = dpni_set_queue(dpni, CMD_PRI_LOW, priv->token, DPNI_QUEUE_RX,
			     dpaa2_q->tc_index, flow_id, options, &cfg);
	if (ret) {
		DPAA2_PMD_ERR("Error in setting the rx flow: = %d", ret);
		return -1;
	}

	if (!(priv->flags & DPAA2_RX_TAILDROP_OFF)) {
		struct dpni_taildrop taildrop;

		taildrop.enable = 1;
		dpaa2_q->nb_desc = nb_rx_desc;
		/* Private CGR will use tail drop length as nb_rx_desc.
		 * for rest cases we can use standard byte based tail drop.
		 * There is no HW restriction, but number of CGRs are limited,
		 * hence this restriction is placed.
		 */
		if (dpaa2_q->cgid != 0xff) {
			/*enabling per rx queue congestion control */
			taildrop.threshold = nb_rx_desc;
			taildrop.units = DPNI_CONGESTION_UNIT_FRAMES;
			taildrop.oal = 0;
			DPAA2_PMD_DEBUG("Enabling CG Tail Drop on queue = %d",
					rx_queue_id);
			ret = dpni_set_taildrop(dpni, CMD_PRI_LOW, priv->token,
						DPNI_CP_CONGESTION_GROUP,
						DPNI_QUEUE_RX,
						dpaa2_q->tc_index,
						dpaa2_q->cgid, &taildrop);
		} else {
			/*enabling per rx queue congestion control */
			taildrop.threshold = CONG_THRESHOLD_RX_BYTES_Q;
			taildrop.units = DPNI_CONGESTION_UNIT_BYTES;
			taildrop.oal = CONG_RX_OAL;
			DPAA2_PMD_DEBUG("Enabling Byte based Drop on queue= %d",
					rx_queue_id);
			ret = dpni_set_taildrop(dpni, CMD_PRI_LOW, priv->token,
						DPNI_CP_QUEUE, DPNI_QUEUE_RX,
						dpaa2_q->tc_index, flow_id,
						&taildrop);
		}
		if (ret) {
			DPAA2_PMD_ERR("Error in setting taildrop. err=(%d)",
				      ret);
			return -1;
		}
	} else { /* Disable tail Drop */
		struct dpni_taildrop taildrop = {0};
		DPAA2_PMD_INFO("Tail drop is disabled on queue");

		taildrop.enable = 0;
		if (dpaa2_q->cgid != 0xff) {
			ret = dpni_set_taildrop(dpni, CMD_PRI_LOW, priv->token,
					DPNI_CP_CONGESTION_GROUP, DPNI_QUEUE_RX,
					dpaa2_q->tc_index,
					dpaa2_q->cgid, &taildrop);
		} else {
			ret = dpni_set_taildrop(dpni, CMD_PRI_LOW, priv->token,
					DPNI_CP_QUEUE, DPNI_QUEUE_RX,
					dpaa2_q->tc_index, flow_id, &taildrop);
		}
		if (ret) {
			DPAA2_PMD_ERR("Error in setting taildrop. err=(%d)",
				      ret);
			return -1;
		}
	}

	dev->data->rx_queues[rx_queue_id] = dpaa2_q;
	return 0;
}

static int
dpaa2_dev_tx_queue_setup(struct rte_eth_dev *dev,
			 uint16_t tx_queue_id,
			 uint16_t nb_tx_desc,
			 unsigned int socket_id __rte_unused,
			 const struct rte_eth_txconf *tx_conf)
{
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct dpaa2_queue *dpaa2_q = (struct dpaa2_queue *)
		priv->tx_vq[tx_queue_id];
	struct dpaa2_queue *dpaa2_tx_conf_q = (struct dpaa2_queue *)
		priv->tx_conf_vq[tx_queue_id];
	struct fsl_mc_io *dpni = dev->process_private;
	struct dpni_queue tx_conf_cfg;
	struct dpni_queue tx_flow_cfg;
	uint8_t options = 0, flow_id;
	uint16_t channel_id;
	struct dpni_queue_id qid;
	uint32_t tc_id;
	int ret;

	PMD_INIT_FUNC_TRACE();

	/* Tx deferred start is not supported */
	if (tx_conf->tx_deferred_start) {
		DPAA2_PMD_ERR("%p:Tx deferred start not supported",
				(void *)dev);
		return -EINVAL;
	}

	dpaa2_q->nb_desc = UINT16_MAX;
	dpaa2_q->offloads = tx_conf->offloads;

	/* Return if queue already configured */
	if (dpaa2_q->flow_id != 0xffff) {
		dev->data->tx_queues[tx_queue_id] = dpaa2_q;
		return 0;
	}

	memset(&tx_conf_cfg, 0, sizeof(struct dpni_queue));
	memset(&tx_flow_cfg, 0, sizeof(struct dpni_queue));

	if (tx_queue_id == 0) {
		/*Set tx-conf and error configuration*/
		if (priv->flags & DPAA2_TX_CONF_ENABLE)
			ret = dpni_set_tx_confirmation_mode(dpni, CMD_PRI_LOW,
							    priv->token,
							    DPNI_CONF_AFFINE);
		else
			ret = dpni_set_tx_confirmation_mode(dpni, CMD_PRI_LOW,
							    priv->token,
							    DPNI_CONF_DISABLE);
		if (ret) {
			DPAA2_PMD_ERR("Error in set tx conf mode settings: "
				      "err=%d", ret);
			return -1;
		}
	}

	tc_id = tx_queue_id % priv->num_tx_tc;
	channel_id = (uint8_t)(tx_queue_id / priv->num_tx_tc) % priv->num_channels;
	flow_id = 0;

	ret = dpni_set_queue(dpni, CMD_PRI_LOW, priv->token, DPNI_QUEUE_TX,
			((channel_id << 8) | tc_id), flow_id, options, &tx_flow_cfg);
	if (ret) {
		DPAA2_PMD_ERR("Error in setting the tx flow: "
			"tc_id=%d, flow=%d err=%d",
			tc_id, flow_id, ret);
			return -1;
	}

	dpaa2_q->flow_id = flow_id;

	dpaa2_q->tc_index = tc_id;

	ret = dpni_get_queue(dpni, CMD_PRI_LOW, priv->token,
			     DPNI_QUEUE_TX, ((channel_id << 8) | dpaa2_q->tc_index),
			     dpaa2_q->flow_id, &tx_flow_cfg, &qid);
	if (ret) {
		DPAA2_PMD_ERR("Error in getting LFQID err=%d", ret);
		return -1;
	}
	dpaa2_q->fqid = qid.fqid;

	if (!(priv->flags & DPAA2_TX_CGR_OFF)) {
		struct dpni_congestion_notification_cfg cong_notif_cfg = {0};

		dpaa2_q->nb_desc = nb_tx_desc;

		cong_notif_cfg.units = DPNI_CONGESTION_UNIT_FRAMES;
		cong_notif_cfg.threshold_entry = nb_tx_desc;
		/* Notify that the queue is not congested when the data in
		 * the queue is below this threshold.(90% of value)
		 */
		cong_notif_cfg.threshold_exit = (nb_tx_desc * 9) / 10;
		cong_notif_cfg.message_ctx = 0;
		cong_notif_cfg.message_iova =
				(size_t)DPAA2_VADDR_TO_IOVA(dpaa2_q->cscn);
		cong_notif_cfg.dest_cfg.dest_type = DPNI_DEST_NONE;
		cong_notif_cfg.notification_mode =
					 DPNI_CONG_OPT_WRITE_MEM_ON_ENTER |
					 DPNI_CONG_OPT_WRITE_MEM_ON_EXIT |
					 DPNI_CONG_OPT_COHERENT_WRITE;
		cong_notif_cfg.cg_point = DPNI_CP_QUEUE;

		ret = dpni_set_congestion_notification(dpni, CMD_PRI_LOW,
						       priv->token,
						       DPNI_QUEUE_TX,
						       ((channel_id << 8) | tc_id),
						       &cong_notif_cfg);
		if (ret) {
			DPAA2_PMD_ERR(
			   "Error in setting tx congestion notification: "
			   "err=%d", ret);
			return -ret;
		}
	}
	dpaa2_q->cb_eqresp_free = dpaa2_dev_free_eqresp_buf;
	dev->data->tx_queues[tx_queue_id] = dpaa2_q;

	if (priv->flags & DPAA2_TX_CONF_ENABLE) {
		dpaa2_q->tx_conf_queue = dpaa2_tx_conf_q;
		options = options | DPNI_QUEUE_OPT_USER_CTX;
		tx_conf_cfg.user_context = (size_t)(dpaa2_q);
		ret = dpni_set_queue(dpni, CMD_PRI_LOW, priv->token,
			     DPNI_QUEUE_TX_CONFIRM, ((channel_id << 8) | dpaa2_tx_conf_q->tc_index),
			     dpaa2_tx_conf_q->flow_id, options, &tx_conf_cfg);
		if (ret) {
			DPAA2_PMD_ERR("Error in setting the tx conf flow: "
			      "tc_index=%d, flow=%d err=%d",
			      dpaa2_tx_conf_q->tc_index,
			      dpaa2_tx_conf_q->flow_id, ret);
			return -1;
		}

		ret = dpni_get_queue(dpni, CMD_PRI_LOW, priv->token,
			     DPNI_QUEUE_TX_CONFIRM, ((channel_id << 8) | dpaa2_tx_conf_q->tc_index),
			     dpaa2_tx_conf_q->flow_id, &tx_conf_cfg, &qid);
		if (ret) {
			DPAA2_PMD_ERR("Error in getting LFQID err=%d", ret);
			return -1;
		}
		dpaa2_tx_conf_q->fqid = qid.fqid;
	}
	return 0;
}

static void
dpaa2_dev_rx_queue_release(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct dpaa2_queue *dpaa2_q = dev->data->rx_queues[rx_queue_id];
	struct dpaa2_dev_priv *priv = dpaa2_q->eth_data->dev_private;
	struct fsl_mc_io *dpni =
		(struct fsl_mc_io *)priv->eth_dev->process_private;
	uint8_t options = 0;
	int ret;
	struct dpni_queue cfg;

	memset(&cfg, 0, sizeof(struct dpni_queue));
	PMD_INIT_FUNC_TRACE();

	total_nb_rx_desc -= dpaa2_q->nb_desc;

	if (dpaa2_q->cgid != 0xff) {
		options = DPNI_QUEUE_OPT_CLEAR_CGID;
		cfg.cgid = dpaa2_q->cgid;

		ret = dpni_set_queue(dpni, CMD_PRI_LOW, priv->token,
				     DPNI_QUEUE_RX,
				     dpaa2_q->tc_index, dpaa2_q->flow_id,
				     options, &cfg);
		if (ret)
			DPAA2_PMD_ERR("Unable to clear CGR from q=%u err=%d",
					dpaa2_q->fqid, ret);
		priv->cgid_in_use[dpaa2_q->cgid] = 0;
		dpaa2_q->cgid = 0xff;
	}
}

static uint32_t
dpaa2_dev_rx_queue_count(void *rx_queue)
{
	int32_t ret;
	struct dpaa2_queue *dpaa2_q;
	struct qbman_swp *swp;
	struct qbman_fq_query_np_rslt state;
	uint32_t frame_cnt = 0;

	if (unlikely(!DPAA2_PER_LCORE_DPIO)) {
		ret = dpaa2_affine_qbman_swp();
		if (ret) {
			DPAA2_PMD_ERR(
				"Failed to allocate IO portal, tid: %d\n",
				rte_gettid());
			return -EINVAL;
		}
	}
	swp = DPAA2_PER_LCORE_PORTAL;

	dpaa2_q = rx_queue;

	if (qbman_fq_query_state(swp, dpaa2_q->fqid, &state) == 0) {
		frame_cnt = qbman_fq_state_frame_count(&state);
		DPAA2_PMD_DP_DEBUG("RX frame count for q(%p) is %u",
				rx_queue, frame_cnt);
	}
	return frame_cnt;
}

static const uint32_t *
dpaa2_supported_ptypes_get(struct rte_eth_dev *dev)
{
	static const uint32_t ptypes[] = {
		/*todo -= add more types */
		RTE_PTYPE_L2_ETHER,
		RTE_PTYPE_L3_IPV4,
		RTE_PTYPE_L3_IPV4_EXT,
		RTE_PTYPE_L3_IPV6,
		RTE_PTYPE_L3_IPV6_EXT,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_L4_SCTP,
		RTE_PTYPE_L4_ICMP,
		RTE_PTYPE_UNKNOWN
	};

	if (dev->rx_pkt_burst == dpaa2_dev_prefetch_rx ||
		dev->rx_pkt_burst == dpaa2_dev_rx ||
		dev->rx_pkt_burst == dpaa2_dev_loopback_rx)
		return ptypes;
	return NULL;
}

/**
 * Dpaa2 link Interrupt handler
 *
 * @param param
 *  The address of parameter (struct rte_eth_dev *) registered before.
 *
 * @return
 *  void
 */
static void
dpaa2_interrupt_handler(void *param)
{
	struct rte_eth_dev *dev = param;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)dev->process_private;
	int ret;
	int irq_index = DPNI_IRQ_INDEX;
	unsigned int status = 0, clear = 0;

	PMD_INIT_FUNC_TRACE();

	if (dpni == NULL) {
		DPAA2_PMD_ERR("dpni is NULL");
		return;
	}

	ret = dpni_get_irq_status(dpni, CMD_PRI_LOW, priv->token,
				  irq_index, &status);
	if (unlikely(ret)) {
		DPAA2_PMD_ERR("Can't get irq status (err %d)", ret);
		clear = 0xffffffff;
		goto out;
	}

	if (status & DPNI_IRQ_EVENT_LINK_CHANGED) {
		clear = DPNI_IRQ_EVENT_LINK_CHANGED;
		dpaa2_dev_link_update(dev, 0);
		/* calling all the apps registered for link status event */
		rte_eth_dev_callback_process(dev, RTE_ETH_EVENT_INTR_LSC, NULL);
	}
out:
	ret = dpni_clear_irq_status(dpni, CMD_PRI_LOW, priv->token,
				    irq_index, clear);
	if (unlikely(ret))
		DPAA2_PMD_ERR("Can't clear irq status (err %d)", ret);
}

static int
dpaa2_eth_setup_irqs(struct rte_eth_dev *dev, int enable)
{
	int err = 0;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)dev->process_private;
	int irq_index = DPNI_IRQ_INDEX;
	unsigned int mask = DPNI_IRQ_EVENT_LINK_CHANGED;

	PMD_INIT_FUNC_TRACE();

	err = dpni_set_irq_mask(dpni, CMD_PRI_LOW, priv->token,
				irq_index, mask);
	if (err < 0) {
		DPAA2_PMD_ERR("Error: dpni_set_irq_mask():%d (%s)", err,
			      strerror(-err));
		return err;
	}

	err = dpni_set_irq_enable(dpni, CMD_PRI_LOW, priv->token,
				  irq_index, enable);
	if (err < 0)
		DPAA2_PMD_ERR("Error: dpni_set_irq_enable():%d (%s)", err,
			      strerror(-err));

	return err;
}

static int
dpaa2_dev_start(struct rte_eth_dev *dev)
{
	struct rte_device *rdev = dev->device;
	struct rte_dpaa2_device *dpaa2_dev;
	struct rte_eth_dev_data *data = dev->data;
	struct dpaa2_dev_priv *priv = data->dev_private;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)dev->process_private;
	struct dpni_queue cfg;
	struct dpni_error_cfg	err_cfg;
	struct dpni_queue_id qid;
	struct dpaa2_queue *dpaa2_q;
	int ret, i;
	struct rte_intr_handle *intr_handle;

	dpaa2_dev = container_of(rdev, struct rte_dpaa2_device, device);
	intr_handle = dpaa2_dev->intr_handle;

	PMD_INIT_FUNC_TRACE();
	ret = dpni_enable(dpni, CMD_PRI_LOW, priv->token);
	if (ret) {
		DPAA2_PMD_ERR("Failure in enabling dpni %d device: err=%d",
			      priv->hw_id, ret);
		return ret;
	}

	/* Power up the phy. Needed to make the link go UP */
	dpaa2_dev_set_link_up(dev);

	for (i = 0; i < data->nb_rx_queues; i++) {
		dpaa2_q = (struct dpaa2_queue *)data->rx_queues[i];
		ret = dpni_get_queue(dpni, CMD_PRI_LOW, priv->token,
				     DPNI_QUEUE_RX, dpaa2_q->tc_index,
				       dpaa2_q->flow_id, &cfg, &qid);
		if (ret) {
			DPAA2_PMD_ERR("Error in getting flow information: "
				      "err=%d", ret);
			return ret;
		}
		dpaa2_q->fqid = qid.fqid;
	}

	if (dpaa2_enable_err_queue) {
		ret = dpni_get_queue(dpni, CMD_PRI_LOW, priv->token,
				     DPNI_QUEUE_RX_ERR, 0, 0, &cfg, &qid);
		if (ret) {
			DPAA2_PMD_ERR("Error getting rx err flow information: err=%d",
						ret);
			return ret;
		}
		dpaa2_q = (struct dpaa2_queue *)priv->rx_err_vq;
		dpaa2_q->fqid = qid.fqid;
		dpaa2_q->eth_data = dev->data;

		err_cfg.errors =  DPNI_ERROR_DISC;
		err_cfg.error_action = DPNI_ERROR_ACTION_SEND_TO_ERROR_QUEUE;
	} else {
		/* checksum errors, send them to normal path
		 * and set it in annotation
		 */
		err_cfg.errors = DPNI_ERROR_L3CE | DPNI_ERROR_L4CE;

		/* if packet with parse error are not to be dropped */
		err_cfg.errors |= DPNI_ERROR_PHE;

		err_cfg.error_action = DPNI_ERROR_ACTION_CONTINUE;
	}
	err_cfg.set_frame_annotation = true;

	ret = dpni_set_errors_behavior(dpni, CMD_PRI_LOW,
				       priv->token, &err_cfg);
	if (ret) {
		DPAA2_PMD_ERR("Error to dpni_set_errors_behavior: code = %d",
			      ret);
		return ret;
	}

	/* if the interrupts were configured on this devices*/
	if (intr_handle && rte_intr_fd_get(intr_handle) &&
	    dev->data->dev_conf.intr_conf.lsc != 0) {
		/* Registering LSC interrupt handler */
		rte_intr_callback_register(intr_handle,
					   dpaa2_interrupt_handler,
					   (void *)dev);

		/* enable vfio intr/eventfd mapping
		 * Interrupt index 0 is required, so we can not use
		 * rte_intr_enable.
		 */
		rte_dpaa2_intr_enable(intr_handle, DPNI_IRQ_INDEX);

		/* enable dpni_irqs */
		dpaa2_eth_setup_irqs(dev, 1);
	}

	/* Change the tx burst function if ordered queues are used */
	if (priv->en_ordered)
		dev->tx_pkt_burst = dpaa2_dev_tx_ordered;

	return 0;
}

/**
 *  This routine disables all traffic on the adapter by issuing a
 *  global reset on the MAC.
 */
static int
dpaa2_dev_stop(struct rte_eth_dev *dev)
{
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)dev->process_private;
	int ret;
	struct rte_eth_link link;
	struct rte_device *rdev = dev->device;
	struct rte_intr_handle *intr_handle;
	struct rte_dpaa2_device *dpaa2_dev;

	dpaa2_dev = container_of(rdev, struct rte_dpaa2_device, device);
	intr_handle = dpaa2_dev->intr_handle;

	PMD_INIT_FUNC_TRACE();

	/* reset interrupt callback  */
	if (intr_handle && rte_intr_fd_get(intr_handle) &&
	    dev->data->dev_conf.intr_conf.lsc != 0) {
		/*disable dpni irqs */
		dpaa2_eth_setup_irqs(dev, 0);

		/* disable vfio intr before callback unregister */
		rte_dpaa2_intr_disable(intr_handle, DPNI_IRQ_INDEX);

		/* Unregistering LSC interrupt handler */
		rte_intr_callback_unregister(intr_handle,
					     dpaa2_interrupt_handler,
					     (void *)dev);
	}

	dpaa2_dev_set_link_down(dev);

	ret = dpni_disable(dpni, CMD_PRI_LOW, priv->token);
	if (ret) {
		DPAA2_PMD_ERR("Failure (ret %d) in disabling dpni %d dev",
			      ret, priv->hw_id);
		return ret;
	}

	/* clear the recorded link status */
	memset(&link, 0, sizeof(link));
	rte_eth_linkstatus_set(dev, &link);

	return 0;
}

static int
dpaa2_dev_close(struct rte_eth_dev *dev)
{
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)dev->process_private;
	int i, ret;
	struct rte_eth_link link;

	PMD_INIT_FUNC_TRACE();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	if (!dpni) {
		DPAA2_PMD_WARN("Already closed or not started");
		return -1;
	}

	dpaa2_tm_deinit(dev);
	dpaa2_flow_clean(dev);
	/* Clean the device first */
	ret = dpni_reset(dpni, CMD_PRI_LOW, priv->token);
	if (ret) {
		DPAA2_PMD_ERR("Failure cleaning dpni device: err=%d", ret);
		return -1;
	}

	memset(&link, 0, sizeof(link));
	rte_eth_linkstatus_set(dev, &link);

	/* Free private queues memory */
	dpaa2_free_rx_tx_queues(dev);
	/* Close the device at underlying layer*/
	ret = dpni_close(dpni, CMD_PRI_LOW, priv->token);
	if (ret) {
		DPAA2_PMD_ERR("Failure closing dpni device with err code %d",
			      ret);
	}

	/* Free the allocated memory for ethernet private data and dpni*/
	priv->hw = NULL;
	dev->process_private = NULL;
	rte_free(dpni);

	for (i = 0; i < MAX_TCS; i++)
		rte_free((void *)(size_t)priv->extract.tc_extract_param[i]);

	if (priv->extract.qos_extract_param)
		rte_free((void *)(size_t)priv->extract.qos_extract_param);

	DPAA2_PMD_INFO("%s: netdev deleted", dev->data->name);
	return 0;
}

static int
dpaa2_dev_promiscuous_enable(
		struct rte_eth_dev *dev)
{
	int ret;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)dev->process_private;

	PMD_INIT_FUNC_TRACE();

	if (dpni == NULL) {
		DPAA2_PMD_ERR("dpni is NULL");
		return -ENODEV;
	}

	ret = dpni_set_unicast_promisc(dpni, CMD_PRI_LOW, priv->token, true);
	if (ret < 0)
		DPAA2_PMD_ERR("Unable to enable U promisc mode %d", ret);

	ret = dpni_set_multicast_promisc(dpni, CMD_PRI_LOW, priv->token, true);
	if (ret < 0)
		DPAA2_PMD_ERR("Unable to enable M promisc mode %d", ret);

	return ret;
}

static int
dpaa2_dev_promiscuous_disable(
		struct rte_eth_dev *dev)
{
	int ret;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)dev->process_private;

	PMD_INIT_FUNC_TRACE();

	if (dpni == NULL) {
		DPAA2_PMD_ERR("dpni is NULL");
		return -ENODEV;
	}

	ret = dpni_set_unicast_promisc(dpni, CMD_PRI_LOW, priv->token, false);
	if (ret < 0)
		DPAA2_PMD_ERR("Unable to disable U promisc mode %d", ret);

	if (dev->data->all_multicast == 0) {
		ret = dpni_set_multicast_promisc(dpni, CMD_PRI_LOW,
						 priv->token, false);
		if (ret < 0)
			DPAA2_PMD_ERR("Unable to disable M promisc mode %d",
				      ret);
	}

	return ret;
}

static int
dpaa2_dev_allmulticast_enable(
		struct rte_eth_dev *dev)
{
	int ret;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)dev->process_private;

	PMD_INIT_FUNC_TRACE();

	if (dpni == NULL) {
		DPAA2_PMD_ERR("dpni is NULL");
		return -ENODEV;
	}

	ret = dpni_set_multicast_promisc(dpni, CMD_PRI_LOW, priv->token, true);
	if (ret < 0)
		DPAA2_PMD_ERR("Unable to enable multicast mode %d", ret);

	return ret;
}

static int
dpaa2_dev_allmulticast_disable(struct rte_eth_dev *dev)
{
	int ret;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)dev->process_private;

	PMD_INIT_FUNC_TRACE();

	if (dpni == NULL) {
		DPAA2_PMD_ERR("dpni is NULL");
		return -ENODEV;
	}

	/* must remain on for all promiscuous */
	if (dev->data->promiscuous == 1)
		return 0;

	ret = dpni_set_multicast_promisc(dpni, CMD_PRI_LOW, priv->token, false);
	if (ret < 0)
		DPAA2_PMD_ERR("Unable to disable multicast mode %d", ret);

	return ret;
}

static int
dpaa2_dev_mtu_set(struct rte_eth_dev *dev, uint16_t mtu)
{
	int ret;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)dev->process_private;
	uint32_t frame_size = mtu + RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN
				+ VLAN_TAG_SIZE;

	PMD_INIT_FUNC_TRACE();

	if (dpni == NULL) {
		DPAA2_PMD_ERR("dpni is NULL");
		return -EINVAL;
	}

	/* Set the Max Rx frame length as 'mtu' +
	 * Maximum Ethernet header length
	 */
	ret = dpni_set_max_frame_length(dpni, CMD_PRI_LOW, priv->token,
					frame_size - RTE_ETHER_CRC_LEN);
	if (ret) {
		DPAA2_PMD_ERR("Setting the max frame length failed");
		return -1;
	}
	DPAA2_PMD_INFO("MTU configured for the device: %d", mtu);
	return 0;
}

static int
dpaa2_dev_add_mac_addr(struct rte_eth_dev *dev,
		       struct rte_ether_addr *addr,
		       __rte_unused uint32_t index,
		       __rte_unused uint32_t pool)
{
	int ret;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)dev->process_private;

	PMD_INIT_FUNC_TRACE();

	if (dpni == NULL) {
		DPAA2_PMD_ERR("dpni is NULL");
		return -1;
	}

	ret = dpni_add_mac_addr(dpni, CMD_PRI_LOW, priv->token,
				addr->addr_bytes, 0, 0, 0);
	if (ret)
		DPAA2_PMD_ERR(
			"error: Adding the MAC ADDR failed: err = %d", ret);
	return 0;
}

static void
dpaa2_dev_remove_mac_addr(struct rte_eth_dev *dev,
			  uint32_t index)
{
	int ret;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)dev->process_private;
	struct rte_eth_dev_data *data = dev->data;
	struct rte_ether_addr *macaddr;

	PMD_INIT_FUNC_TRACE();

	macaddr = &data->mac_addrs[index];

	if (dpni == NULL) {
		DPAA2_PMD_ERR("dpni is NULL");
		return;
	}

	ret = dpni_remove_mac_addr(dpni, CMD_PRI_LOW,
				   priv->token, macaddr->addr_bytes);
	if (ret)
		DPAA2_PMD_ERR(
			"error: Removing the MAC ADDR failed: err = %d", ret);
}

static int
dpaa2_dev_set_mac_addr(struct rte_eth_dev *dev,
		       struct rte_ether_addr *addr)
{
	int ret;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)dev->process_private;

	PMD_INIT_FUNC_TRACE();

	if (dpni == NULL) {
		DPAA2_PMD_ERR("dpni is NULL");
		return -EINVAL;
	}

	ret = dpni_set_primary_mac_addr(dpni, CMD_PRI_LOW,
					priv->token, addr->addr_bytes);

	if (ret)
		DPAA2_PMD_ERR(
			"error: Setting the MAC ADDR failed %d", ret);

	return ret;
}

static
int dpaa2_dev_stats_get(struct rte_eth_dev *dev,
			 struct rte_eth_stats *stats)
{
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)dev->process_private;
	int32_t  retcode;
	uint8_t page0 = 0, page1 = 1, page2 = 2;
	union dpni_statistics value;
	int i;
	struct dpaa2_queue *dpaa2_rxq, *dpaa2_txq;

	memset(&value, 0, sizeof(union dpni_statistics));

	PMD_INIT_FUNC_TRACE();

	if (!dpni) {
		DPAA2_PMD_ERR("dpni is NULL");
		return -EINVAL;
	}

	if (!stats) {
		DPAA2_PMD_ERR("stats is NULL");
		return -EINVAL;
	}

	/*Get Counters from page_0*/
	retcode = dpni_get_statistics(dpni, CMD_PRI_LOW, priv->token,
				      page0, 0, &value);
	if (retcode)
		goto err;

	stats->ipackets = value.page_0.ingress_all_frames;
	stats->ibytes = value.page_0.ingress_all_bytes;

	/*Get Counters from page_1*/
	retcode = dpni_get_statistics(dpni, CMD_PRI_LOW, priv->token,
				      page1, 0, &value);
	if (retcode)
		goto err;

	stats->opackets = value.page_1.egress_all_frames;
	stats->obytes = value.page_1.egress_all_bytes;

	/*Get Counters from page_2*/
	retcode = dpni_get_statistics(dpni, CMD_PRI_LOW, priv->token,
				      page2, 0, &value);
	if (retcode)
		goto err;

	/* Ingress drop frame count due to configured rules */
	stats->ierrors = value.page_2.ingress_filtered_frames;
	/* Ingress drop frame count due to error */
	stats->ierrors += value.page_2.ingress_discarded_frames;

	stats->oerrors = value.page_2.egress_discarded_frames;
	stats->imissed = value.page_2.ingress_nobuffer_discards;

	/* Fill in per queue stats */
	for (i = 0; (i < RTE_ETHDEV_QUEUE_STAT_CNTRS) &&
		(i < priv->nb_rx_queues || i < priv->nb_tx_queues); ++i) {
		dpaa2_rxq = (struct dpaa2_queue *)priv->rx_vq[i];
		dpaa2_txq = (struct dpaa2_queue *)priv->tx_vq[i];
		if (dpaa2_rxq)
			stats->q_ipackets[i] = dpaa2_rxq->rx_pkts;
		if (dpaa2_txq)
			stats->q_opackets[i] = dpaa2_txq->tx_pkts;

		/* Byte counting is not implemented */
		stats->q_ibytes[i]   = 0;
		stats->q_obytes[i]   = 0;
	}

	return 0;

err:
	DPAA2_PMD_ERR("Operation not completed:Error Code = %d", retcode);
	return retcode;
};

static int
dpaa2_dev_xstats_get(struct rte_eth_dev *dev, struct rte_eth_xstat *xstats,
		     unsigned int n)
{
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)dev->process_private;
	int32_t  retcode;
	union dpni_statistics value[5] = {};
	unsigned int i = 0, num = RTE_DIM(dpaa2_xstats_strings);

	if (n < num)
		return num;

	if (xstats == NULL)
		return 0;

	/* Get Counters from page_0*/
	retcode = dpni_get_statistics(dpni, CMD_PRI_LOW, priv->token,
				      0, 0, &value[0]);
	if (retcode)
		goto err;

	/* Get Counters from page_1*/
	retcode = dpni_get_statistics(dpni, CMD_PRI_LOW, priv->token,
				      1, 0, &value[1]);
	if (retcode)
		goto err;

	/* Get Counters from page_2*/
	retcode = dpni_get_statistics(dpni, CMD_PRI_LOW, priv->token,
				      2, 0, &value[2]);
	if (retcode)
		goto err;

	for (i = 0; i < priv->max_cgs; i++) {
		if (!priv->cgid_in_use[i]) {
			/* Get Counters from page_4*/
			retcode = dpni_get_statistics(dpni, CMD_PRI_LOW,
						      priv->token,
						      4, 0, &value[4]);
			if (retcode)
				goto err;
			break;
		}
	}

	for (i = 0; i < num; i++) {
		xstats[i].id = i;
		xstats[i].value = value[dpaa2_xstats_strings[i].page_id].
			raw.counter[dpaa2_xstats_strings[i].stats_id];
	}
	return i;
err:
	DPAA2_PMD_ERR("Error in obtaining extended stats (%d)", retcode);
	return retcode;
}

static int
dpaa2_xstats_get_names(__rte_unused struct rte_eth_dev *dev,
		       struct rte_eth_xstat_name *xstats_names,
		       unsigned int limit)
{
	unsigned int i, stat_cnt = RTE_DIM(dpaa2_xstats_strings);

	if (limit < stat_cnt)
		return stat_cnt;

	if (xstats_names != NULL)
		for (i = 0; i < stat_cnt; i++)
			strlcpy(xstats_names[i].name,
				dpaa2_xstats_strings[i].name,
				sizeof(xstats_names[i].name));

	return stat_cnt;
}

static int
dpaa2_xstats_get_by_id(struct rte_eth_dev *dev, const uint64_t *ids,
		       uint64_t *values, unsigned int n)
{
	unsigned int i, stat_cnt = RTE_DIM(dpaa2_xstats_strings);
	uint64_t values_copy[stat_cnt];

	if (!ids) {
		struct dpaa2_dev_priv *priv = dev->data->dev_private;
		struct fsl_mc_io *dpni =
			(struct fsl_mc_io *)dev->process_private;
		int32_t  retcode;
		union dpni_statistics value[5] = {};

		if (n < stat_cnt)
			return stat_cnt;

		if (!values)
			return 0;

		/* Get Counters from page_0*/
		retcode = dpni_get_statistics(dpni, CMD_PRI_LOW, priv->token,
					      0, 0, &value[0]);
		if (retcode)
			return 0;

		/* Get Counters from page_1*/
		retcode = dpni_get_statistics(dpni, CMD_PRI_LOW, priv->token,
					      1, 0, &value[1]);
		if (retcode)
			return 0;

		/* Get Counters from page_2*/
		retcode = dpni_get_statistics(dpni, CMD_PRI_LOW, priv->token,
					      2, 0, &value[2]);
		if (retcode)
			return 0;

		/* Get Counters from page_4*/
		retcode = dpni_get_statistics(dpni, CMD_PRI_LOW, priv->token,
					      4, 0, &value[4]);
		if (retcode)
			return 0;

		for (i = 0; i < stat_cnt; i++) {
			values[i] = value[dpaa2_xstats_strings[i].page_id].
				raw.counter[dpaa2_xstats_strings[i].stats_id];
		}
		return stat_cnt;
	}

	dpaa2_xstats_get_by_id(dev, NULL, values_copy, stat_cnt);

	for (i = 0; i < n; i++) {
		if (ids[i] >= stat_cnt) {
			DPAA2_PMD_ERR("xstats id value isn't valid");
			return -1;
		}
		values[i] = values_copy[ids[i]];
	}
	return n;
}

static int
dpaa2_xstats_get_names_by_id(
	struct rte_eth_dev *dev,
	const uint64_t *ids,
	struct rte_eth_xstat_name *xstats_names,
	unsigned int limit)
{
	unsigned int i, stat_cnt = RTE_DIM(dpaa2_xstats_strings);
	struct rte_eth_xstat_name xstats_names_copy[stat_cnt];

	if (!ids)
		return dpaa2_xstats_get_names(dev, xstats_names, limit);

	dpaa2_xstats_get_names(dev, xstats_names_copy, limit);

	for (i = 0; i < limit; i++) {
		if (ids[i] >= stat_cnt) {
			DPAA2_PMD_ERR("xstats id value isn't valid");
			return -1;
		}
		strcpy(xstats_names[i].name, xstats_names_copy[ids[i]].name);
	}
	return limit;
}

static int
dpaa2_dev_stats_reset(struct rte_eth_dev *dev)
{
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)dev->process_private;
	int retcode;
	int i;
	struct dpaa2_queue *dpaa2_q;

	PMD_INIT_FUNC_TRACE();

	if (dpni == NULL) {
		DPAA2_PMD_ERR("dpni is NULL");
		return -EINVAL;
	}

	retcode =  dpni_reset_statistics(dpni, CMD_PRI_LOW, priv->token);
	if (retcode)
		goto error;

	/* Reset the per queue stats in dpaa2_queue structure */
	for (i = 0; i < priv->nb_rx_queues; i++) {
		dpaa2_q = (struct dpaa2_queue *)priv->rx_vq[i];
		if (dpaa2_q)
			dpaa2_q->rx_pkts = 0;
	}

	for (i = 0; i < priv->nb_tx_queues; i++) {
		dpaa2_q = (struct dpaa2_queue *)priv->tx_vq[i];
		if (dpaa2_q)
			dpaa2_q->tx_pkts = 0;
	}

	return 0;

error:
	DPAA2_PMD_ERR("Operation not completed:Error Code = %d", retcode);
	return retcode;
};

/* return 0 means link status changed, -1 means not changed */
static int
dpaa2_dev_link_update(struct rte_eth_dev *dev,
		      int wait_to_complete)
{
	int ret;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)dev->process_private;
	struct rte_eth_link link;
	struct dpni_link_state state = {0};
	uint8_t count;

	if (dpni == NULL) {
		DPAA2_PMD_ERR("dpni is NULL");
		return 0;
	}

	for (count = 0; count <= MAX_REPEAT_TIME; count++) {
		ret = dpni_get_link_state(dpni, CMD_PRI_LOW, priv->token,
					  &state);
		if (ret < 0) {
			DPAA2_PMD_DEBUG("error: dpni_get_link_state %d", ret);
			return -1;
		}
		if (state.up == RTE_ETH_LINK_DOWN &&
		    wait_to_complete)
			rte_delay_ms(CHECK_INTERVAL);
		else
			break;
	}

	memset(&link, 0, sizeof(struct rte_eth_link));
	link.link_status = state.up;
	link.link_speed = state.rate;

	if (state.options & DPNI_LINK_OPT_HALF_DUPLEX)
		link.link_duplex = RTE_ETH_LINK_HALF_DUPLEX;
	else
		link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;

	ret = rte_eth_linkstatus_set(dev, &link);
	if (ret == -1)
		DPAA2_PMD_DEBUG("No change in status");
	else
		DPAA2_PMD_INFO("Port %d Link is %s\n", dev->data->port_id,
			       link.link_status ? "Up" : "Down");

	return ret;
}

/**
 * Toggle the DPNI to enable, if not already enabled.
 * This is not strictly PHY up/down - it is more of logical toggling.
 */
static int
dpaa2_dev_set_link_up(struct rte_eth_dev *dev)
{
	int ret = -EINVAL;
	struct dpaa2_dev_priv *priv;
	struct fsl_mc_io *dpni;
	int en = 0;
	struct dpni_link_state state = {0};

	priv = dev->data->dev_private;
	dpni = (struct fsl_mc_io *)dev->process_private;

	if (dpni == NULL) {
		DPAA2_PMD_ERR("dpni is NULL");
		return ret;
	}

	/* Check if DPNI is currently enabled */
	ret = dpni_is_enabled(dpni, CMD_PRI_LOW, priv->token, &en);
	if (ret) {
		/* Unable to obtain dpni status; Not continuing */
		DPAA2_PMD_ERR("Interface Link UP failed (%d)", ret);
		return -EINVAL;
	}

	/* Enable link if not already enabled */
	if (!en) {
		ret = dpni_enable(dpni, CMD_PRI_LOW, priv->token);
		if (ret) {
			DPAA2_PMD_ERR("Interface Link UP failed (%d)", ret);
			return -EINVAL;
		}
	}
	ret = dpni_get_link_state(dpni, CMD_PRI_LOW, priv->token, &state);
	if (ret < 0) {
		DPAA2_PMD_DEBUG("Unable to get link state (%d)", ret);
		return -1;
	}

	/* changing tx burst function to start enqueues */
	dev->tx_pkt_burst = dpaa2_dev_tx;
	dev->data->dev_link.link_status = state.up;
	dev->data->dev_link.link_speed = state.rate;

	if (state.up)
		DPAA2_PMD_INFO("Port %d Link is Up", dev->data->port_id);
	else
		DPAA2_PMD_INFO("Port %d Link is Down", dev->data->port_id);
	return ret;
}

/**
 * Toggle the DPNI to disable, if not already disabled.
 * This is not strictly PHY up/down - it is more of logical toggling.
 */
static int
dpaa2_dev_set_link_down(struct rte_eth_dev *dev)
{
	int ret = -EINVAL;
	struct dpaa2_dev_priv *priv;
	struct fsl_mc_io *dpni;
	int dpni_enabled = 0;
	int retries = 10;

	PMD_INIT_FUNC_TRACE();

	priv = dev->data->dev_private;
	dpni = (struct fsl_mc_io *)dev->process_private;

	if (dpni == NULL) {
		DPAA2_PMD_ERR("Device has not yet been configured");
		return ret;
	}

	/*changing  tx burst function to avoid any more enqueues */
	dev->tx_pkt_burst = rte_eth_pkt_burst_dummy;

	/* Loop while dpni_disable() attempts to drain the egress FQs
	 * and confirm them back to us.
	 */
	do {
		ret = dpni_disable(dpni, 0, priv->token);
		if (ret) {
			DPAA2_PMD_ERR("dpni disable failed (%d)", ret);
			return ret;
		}
		ret = dpni_is_enabled(dpni, 0, priv->token, &dpni_enabled);
		if (ret) {
			DPAA2_PMD_ERR("dpni enable check failed (%d)", ret);
			return ret;
		}
		if (dpni_enabled)
			/* Allow the MC some slack */
			rte_delay_us(100 * 1000);
	} while (dpni_enabled && --retries);

	if (!retries) {
		DPAA2_PMD_WARN("Retry count exceeded disabling dpni");
		/* todo- we may have to manually cleanup queues.
		 */
	} else {
		DPAA2_PMD_INFO("Port %d Link DOWN successful",
			       dev->data->port_id);
	}

	dev->data->dev_link.link_status = 0;

	return ret;
}

static int
dpaa2_flow_ctrl_get(struct rte_eth_dev *dev, struct rte_eth_fc_conf *fc_conf)
{
	int ret = -EINVAL;
	struct dpaa2_dev_priv *priv;
	struct fsl_mc_io *dpni;
	struct dpni_link_state state = {0};

	PMD_INIT_FUNC_TRACE();

	priv = dev->data->dev_private;
	dpni = (struct fsl_mc_io *)dev->process_private;

	if (dpni == NULL || fc_conf == NULL) {
		DPAA2_PMD_ERR("device not configured");
		return ret;
	}

	ret = dpni_get_link_state(dpni, CMD_PRI_LOW, priv->token, &state);
	if (ret) {
		DPAA2_PMD_ERR("error: dpni_get_link_state %d", ret);
		return ret;
	}

	memset(fc_conf, 0, sizeof(struct rte_eth_fc_conf));
	if (state.options & DPNI_LINK_OPT_PAUSE) {
		/* DPNI_LINK_OPT_PAUSE set
		 *  if ASYM_PAUSE not set,
		 *	RX Side flow control (handle received Pause frame)
		 *	TX side flow control (send Pause frame)
		 *  if ASYM_PAUSE set,
		 *	RX Side flow control (handle received Pause frame)
		 *	No TX side flow control (send Pause frame disabled)
		 */
		if (!(state.options & DPNI_LINK_OPT_ASYM_PAUSE))
			fc_conf->mode = RTE_ETH_FC_FULL;
		else
			fc_conf->mode = RTE_ETH_FC_RX_PAUSE;
	} else {
		/* DPNI_LINK_OPT_PAUSE not set
		 *  if ASYM_PAUSE set,
		 *	TX side flow control (send Pause frame)
		 *	No RX side flow control (No action on pause frame rx)
		 *  if ASYM_PAUSE not set,
		 *	Flow control disabled
		 */
		if (state.options & DPNI_LINK_OPT_ASYM_PAUSE)
			fc_conf->mode = RTE_ETH_FC_TX_PAUSE;
		else
			fc_conf->mode = RTE_ETH_FC_NONE;
	}

	return ret;
}

static int
dpaa2_flow_ctrl_set(struct rte_eth_dev *dev, struct rte_eth_fc_conf *fc_conf)
{
	int ret = -EINVAL;
	struct dpaa2_dev_priv *priv;
	struct fsl_mc_io *dpni;
	struct dpni_link_state state = {0};
	struct dpni_link_cfg cfg = {0};

	PMD_INIT_FUNC_TRACE();

	priv = dev->data->dev_private;
	dpni = (struct fsl_mc_io *)dev->process_private;

	if (dpni == NULL) {
		DPAA2_PMD_ERR("dpni is NULL");
		return ret;
	}

	/* It is necessary to obtain the current state before setting fc_conf
	 * as MC would return error in case rate, autoneg or duplex values are
	 * different.
	 */
	ret = dpni_get_link_state(dpni, CMD_PRI_LOW, priv->token, &state);
	if (ret) {
		DPAA2_PMD_ERR("Unable to get link state (err=%d)", ret);
		return -1;
	}

	/* Disable link before setting configuration */
	dpaa2_dev_set_link_down(dev);

	/* Based on fc_conf, update cfg */
	cfg.rate = state.rate;
	cfg.options = state.options;

	/* update cfg with fc_conf */
	switch (fc_conf->mode) {
	case RTE_ETH_FC_FULL:
		/* Full flow control;
		 * OPT_PAUSE set, ASYM_PAUSE not set
		 */
		cfg.options |= DPNI_LINK_OPT_PAUSE;
		cfg.options &= ~DPNI_LINK_OPT_ASYM_PAUSE;
		break;
	case RTE_ETH_FC_TX_PAUSE:
		/* Enable RX flow control
		 * OPT_PAUSE not set;
		 * ASYM_PAUSE set;
		 */
		cfg.options |= DPNI_LINK_OPT_ASYM_PAUSE;
		cfg.options &= ~DPNI_LINK_OPT_PAUSE;
		break;
	case RTE_ETH_FC_RX_PAUSE:
		/* Enable TX Flow control
		 * OPT_PAUSE set
		 * ASYM_PAUSE set
		 */
		cfg.options |= DPNI_LINK_OPT_PAUSE;
		cfg.options |= DPNI_LINK_OPT_ASYM_PAUSE;
		break;
	case RTE_ETH_FC_NONE:
		/* Disable Flow control
		 * OPT_PAUSE not set
		 * ASYM_PAUSE not set
		 */
		cfg.options &= ~DPNI_LINK_OPT_PAUSE;
		cfg.options &= ~DPNI_LINK_OPT_ASYM_PAUSE;
		break;
	default:
		DPAA2_PMD_ERR("Incorrect Flow control flag (%d)",
			      fc_conf->mode);
		return -1;
	}

	ret = dpni_set_link_cfg(dpni, CMD_PRI_LOW, priv->token, &cfg);
	if (ret)
		DPAA2_PMD_ERR("Unable to set Link configuration (err=%d)",
			      ret);

	/* Enable link */
	dpaa2_dev_set_link_up(dev);

	return ret;
}

static int
dpaa2_dev_rss_hash_update(struct rte_eth_dev *dev,
			  struct rte_eth_rss_conf *rss_conf)
{
	struct rte_eth_dev_data *data = dev->data;
	struct dpaa2_dev_priv *priv = data->dev_private;
	struct rte_eth_conf *eth_conf = &data->dev_conf;
	int ret, tc_index;

	PMD_INIT_FUNC_TRACE();

	if (rss_conf->rss_hf) {
		for (tc_index = 0; tc_index < priv->num_rx_tc; tc_index++) {
			ret = dpaa2_setup_flow_dist(dev, rss_conf->rss_hf,
				tc_index);
			if (ret) {
				DPAA2_PMD_ERR("Unable to set flow dist on tc%d",
					tc_index);
				return ret;
			}
		}
	} else {
		for (tc_index = 0; tc_index < priv->num_rx_tc; tc_index++) {
			ret = dpaa2_remove_flow_dist(dev, tc_index);
			if (ret) {
				DPAA2_PMD_ERR(
					"Unable to remove flow dist on tc%d",
					tc_index);
				return ret;
			}
		}
	}
	eth_conf->rx_adv_conf.rss_conf.rss_hf = rss_conf->rss_hf;
	return 0;
}

static int
dpaa2_dev_rss_hash_conf_get(struct rte_eth_dev *dev,
			    struct rte_eth_rss_conf *rss_conf)
{
	struct rte_eth_dev_data *data = dev->data;
	struct rte_eth_conf *eth_conf = &data->dev_conf;

	/* dpaa2 does not support rss_key, so length should be 0*/
	rss_conf->rss_key_len = 0;
	rss_conf->rss_hf = eth_conf->rx_adv_conf.rss_conf.rss_hf;
	return 0;
}

int dpaa2_eth_eventq_attach(const struct rte_eth_dev *dev,
		int eth_rx_queue_id,
		struct dpaa2_dpcon_dev *dpcon,
		const struct rte_event_eth_rx_adapter_queue_conf *queue_conf)
{
	struct dpaa2_dev_priv *eth_priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)dev->process_private;
	struct dpaa2_queue *dpaa2_ethq = eth_priv->rx_vq[eth_rx_queue_id];
	uint8_t flow_id = dpaa2_ethq->flow_id;
	struct dpni_queue cfg;
	uint8_t options, priority;
	int ret;

	if (queue_conf->ev.sched_type == RTE_SCHED_TYPE_PARALLEL)
		dpaa2_ethq->cb = dpaa2_dev_process_parallel_event;
	else if (queue_conf->ev.sched_type == RTE_SCHED_TYPE_ATOMIC)
		dpaa2_ethq->cb = dpaa2_dev_process_atomic_event;
	else if (queue_conf->ev.sched_type == RTE_SCHED_TYPE_ORDERED)
		dpaa2_ethq->cb = dpaa2_dev_process_ordered_event;
	else
		return -EINVAL;

	priority = (RTE_EVENT_DEV_PRIORITY_LOWEST / queue_conf->ev.priority) *
		   (dpcon->num_priorities - 1);

	memset(&cfg, 0, sizeof(struct dpni_queue));
	options = DPNI_QUEUE_OPT_DEST;
	cfg.destination.type = DPNI_DEST_DPCON;
	cfg.destination.id = dpcon->dpcon_id;
	cfg.destination.priority = priority;

	if (queue_conf->ev.sched_type == RTE_SCHED_TYPE_ATOMIC) {
		options |= DPNI_QUEUE_OPT_HOLD_ACTIVE;
		cfg.destination.hold_active = 1;
	}

	if (queue_conf->ev.sched_type == RTE_SCHED_TYPE_ORDERED &&
			!eth_priv->en_ordered) {
		struct opr_cfg ocfg;

		/* Restoration window size = 256 frames */
		ocfg.oprrws = 3;
		/* Restoration window size = 512 frames for LX2 */
		if (dpaa2_svr_family == SVR_LX2160A)
			ocfg.oprrws = 4;
		/* Auto advance NESN window enabled */
		ocfg.oa = 1;
		/* Late arrival window size disabled */
		ocfg.olws = 0;
		/* ORL resource exhaustion advance NESN disabled */
		ocfg.oeane = 0;
		/* Loose ordering enabled */
		ocfg.oloe = 1;
		eth_priv->en_loose_ordered = 1;
		/* Strict ordering enabled if explicitly set */
		if (getenv("DPAA2_STRICT_ORDERING_ENABLE")) {
			ocfg.oloe = 0;
			eth_priv->en_loose_ordered = 0;
		}

		ret = dpni_set_opr(dpni, CMD_PRI_LOW, eth_priv->token,
				   dpaa2_ethq->tc_index, flow_id,
				   OPR_OPT_CREATE, &ocfg, 0);
		if (ret) {
			DPAA2_PMD_ERR("Error setting opr: ret: %d\n", ret);
			return ret;
		}

		eth_priv->en_ordered = 1;
	}

	options |= DPNI_QUEUE_OPT_USER_CTX;
	cfg.user_context = (size_t)(dpaa2_ethq);

	ret = dpni_set_queue(dpni, CMD_PRI_LOW, eth_priv->token, DPNI_QUEUE_RX,
			     dpaa2_ethq->tc_index, flow_id, options, &cfg);
	if (ret) {
		DPAA2_PMD_ERR("Error in dpni_set_queue: ret: %d", ret);
		return ret;
	}

	memcpy(&dpaa2_ethq->ev, &queue_conf->ev, sizeof(struct rte_event));

	return 0;
}

int dpaa2_eth_eventq_detach(const struct rte_eth_dev *dev,
		int eth_rx_queue_id)
{
	struct dpaa2_dev_priv *eth_priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)dev->process_private;
	struct dpaa2_queue *dpaa2_ethq = eth_priv->rx_vq[eth_rx_queue_id];
	uint8_t flow_id = dpaa2_ethq->flow_id;
	struct dpni_queue cfg;
	uint8_t options;
	int ret;

	memset(&cfg, 0, sizeof(struct dpni_queue));
	options = DPNI_QUEUE_OPT_DEST;
	cfg.destination.type = DPNI_DEST_NONE;

	ret = dpni_set_queue(dpni, CMD_PRI_LOW, eth_priv->token, DPNI_QUEUE_RX,
			     dpaa2_ethq->tc_index, flow_id, options, &cfg);
	if (ret)
		DPAA2_PMD_ERR("Error in dpni_set_queue: ret: %d", ret);

	return ret;
}

static int
dpaa2_dev_flow_ops_get(struct rte_eth_dev *dev,
		       const struct rte_flow_ops **ops)
{
	if (!dev)
		return -ENODEV;

	*ops = &dpaa2_flow_ops;
	return 0;
}

static void
dpaa2_rxq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
	struct rte_eth_rxq_info *qinfo)
{
	struct dpaa2_queue *rxq;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)dev->process_private;
	uint16_t max_frame_length;

	rxq = (struct dpaa2_queue *)dev->data->rx_queues[queue_id];

	qinfo->mp = rxq->mb_pool;
	qinfo->scattered_rx = dev->data->scattered_rx;
	qinfo->nb_desc = rxq->nb_desc;
	if (dpni_get_max_frame_length(dpni, CMD_PRI_LOW, priv->token,
				&max_frame_length) == 0)
		qinfo->rx_buf_size = max_frame_length;

	qinfo->conf.rx_free_thresh = 1;
	qinfo->conf.rx_drop_en = 1;
	qinfo->conf.rx_deferred_start = 0;
	qinfo->conf.offloads = rxq->offloads;
}

static void
dpaa2_txq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
	struct rte_eth_txq_info *qinfo)
{
	struct dpaa2_queue *txq;

	txq = dev->data->tx_queues[queue_id];

	qinfo->nb_desc = txq->nb_desc;
	qinfo->conf.tx_thresh.pthresh = 0;
	qinfo->conf.tx_thresh.hthresh = 0;
	qinfo->conf.tx_thresh.wthresh = 0;

	qinfo->conf.tx_free_thresh = 0;
	qinfo->conf.tx_rs_thresh = 0;
	qinfo->conf.offloads = txq->offloads;
	qinfo->conf.tx_deferred_start = 0;
}

static int
dpaa2_tm_ops_get(struct rte_eth_dev *dev __rte_unused, void *ops)
{
	*(const void **)ops = &dpaa2_tm_ops;

	return 0;
}

void
rte_pmd_dpaa2_thread_init(void)
{
	int ret;

	if (unlikely(!DPAA2_PER_LCORE_DPIO)) {
		ret = dpaa2_affine_qbman_swp();
		if (ret) {
			DPAA2_PMD_ERR(
				"Failed to allocate IO portal, tid: %d\n",
				rte_gettid());
			return;
		}
	}
}

static struct eth_dev_ops dpaa2_ethdev_ops = {
	.dev_configure	  = dpaa2_eth_dev_configure,
	.dev_start	      = dpaa2_dev_start,
	.dev_stop	      = dpaa2_dev_stop,
	.dev_close	      = dpaa2_dev_close,
	.promiscuous_enable   = dpaa2_dev_promiscuous_enable,
	.promiscuous_disable  = dpaa2_dev_promiscuous_disable,
	.allmulticast_enable  = dpaa2_dev_allmulticast_enable,
	.allmulticast_disable = dpaa2_dev_allmulticast_disable,
	.dev_set_link_up      = dpaa2_dev_set_link_up,
	.dev_set_link_down    = dpaa2_dev_set_link_down,
	.link_update	   = dpaa2_dev_link_update,
	.stats_get	       = dpaa2_dev_stats_get,
	.xstats_get	       = dpaa2_dev_xstats_get,
	.xstats_get_by_id     = dpaa2_xstats_get_by_id,
	.xstats_get_names_by_id = dpaa2_xstats_get_names_by_id,
	.xstats_get_names      = dpaa2_xstats_get_names,
	.stats_reset	   = dpaa2_dev_stats_reset,
	.xstats_reset	      = dpaa2_dev_stats_reset,
	.fw_version_get	   = dpaa2_fw_version_get,
	.dev_infos_get	   = dpaa2_dev_info_get,
	.dev_supported_ptypes_get = dpaa2_supported_ptypes_get,
	.mtu_set           = dpaa2_dev_mtu_set,
	.vlan_filter_set      = dpaa2_vlan_filter_set,
	.vlan_offload_set     = dpaa2_vlan_offload_set,
	.vlan_tpid_set	      = dpaa2_vlan_tpid_set,
	.rx_queue_setup    = dpaa2_dev_rx_queue_setup,
	.rx_queue_release  = dpaa2_dev_rx_queue_release,
	.tx_queue_setup    = dpaa2_dev_tx_queue_setup,
	.rx_burst_mode_get = dpaa2_dev_rx_burst_mode_get,
	.tx_burst_mode_get = dpaa2_dev_tx_burst_mode_get,
	.flow_ctrl_get	      = dpaa2_flow_ctrl_get,
	.flow_ctrl_set	      = dpaa2_flow_ctrl_set,
	.mac_addr_add         = dpaa2_dev_add_mac_addr,
	.mac_addr_remove      = dpaa2_dev_remove_mac_addr,
	.mac_addr_set         = dpaa2_dev_set_mac_addr,
	.rss_hash_update      = dpaa2_dev_rss_hash_update,
	.rss_hash_conf_get    = dpaa2_dev_rss_hash_conf_get,
	.flow_ops_get         = dpaa2_dev_flow_ops_get,
	.rxq_info_get	      = dpaa2_rxq_info_get,
	.txq_info_get	      = dpaa2_txq_info_get,
	.tm_ops_get	      = dpaa2_tm_ops_get,
#if defined(RTE_LIBRTE_IEEE1588)
	.timesync_enable      = dpaa2_timesync_enable,
	.timesync_disable     = dpaa2_timesync_disable,
	.timesync_read_time   = dpaa2_timesync_read_time,
	.timesync_write_time  = dpaa2_timesync_write_time,
	.timesync_adjust_time = dpaa2_timesync_adjust_time,
	.timesync_read_rx_timestamp = dpaa2_timesync_read_rx_timestamp,
	.timesync_read_tx_timestamp = dpaa2_timesync_read_tx_timestamp,
#endif
};

/* Populate the mac address from physically available (u-boot/firmware) and/or
 * one set by higher layers like MC (restool) etc.
 * Returns the table of MAC entries (multiple entries)
 */
static int
populate_mac_addr(struct fsl_mc_io *dpni_dev, struct dpaa2_dev_priv *priv,
		  struct rte_ether_addr *mac_entry)
{
	int ret;
	struct rte_ether_addr phy_mac, prime_mac;

	memset(&phy_mac, 0, sizeof(struct rte_ether_addr));
	memset(&prime_mac, 0, sizeof(struct rte_ether_addr));

	/* Get the physical device MAC address */
	ret = dpni_get_port_mac_addr(dpni_dev, CMD_PRI_LOW, priv->token,
				     phy_mac.addr_bytes);
	if (ret) {
		DPAA2_PMD_ERR("DPNI get physical port MAC failed: %d", ret);
		goto cleanup;
	}

	ret = dpni_get_primary_mac_addr(dpni_dev, CMD_PRI_LOW, priv->token,
					prime_mac.addr_bytes);
	if (ret) {
		DPAA2_PMD_ERR("DPNI get Prime port MAC failed: %d", ret);
		goto cleanup;
	}

	/* Now that both MAC have been obtained, do:
	 *  if not_empty_mac(phy) && phy != Prime, overwrite prime with Phy
	 *     and return phy
	 *  If empty_mac(phy), return prime.
	 *  if both are empty, create random MAC, set as prime and return
	 */
	if (!rte_is_zero_ether_addr(&phy_mac)) {
		/* If the addresses are not same, overwrite prime */
		if (!rte_is_same_ether_addr(&phy_mac, &prime_mac)) {
			ret = dpni_set_primary_mac_addr(dpni_dev, CMD_PRI_LOW,
							priv->token,
							phy_mac.addr_bytes);
			if (ret) {
				DPAA2_PMD_ERR("Unable to set MAC Address: %d",
					      ret);
				goto cleanup;
			}
			memcpy(&prime_mac, &phy_mac,
				sizeof(struct rte_ether_addr));
		}
	} else if (rte_is_zero_ether_addr(&prime_mac)) {
		/* In case phys and prime, both are zero, create random MAC */
		rte_eth_random_addr(prime_mac.addr_bytes);
		ret = dpni_set_primary_mac_addr(dpni_dev, CMD_PRI_LOW,
						priv->token,
						prime_mac.addr_bytes);
		if (ret) {
			DPAA2_PMD_ERR("Unable to set MAC Address: %d", ret);
			goto cleanup;
		}
	}

	/* prime_mac the final MAC address */
	memcpy(mac_entry, &prime_mac, sizeof(struct rte_ether_addr));
	return 0;

cleanup:
	return -1;
}

static int
check_devargs_handler(__rte_unused const char *key, const char *value,
		      __rte_unused void *opaque)
{
	if (strcmp(value, "1"))
		return -1;

	return 0;
}

static int
dpaa2_get_devargs(struct rte_devargs *devargs, const char *key)
{
	struct rte_kvargs *kvlist;

	if (!devargs)
		return 0;

	kvlist = rte_kvargs_parse(devargs->args, NULL);
	if (!kvlist)
		return 0;

	if (!rte_kvargs_count(kvlist, key)) {
		rte_kvargs_free(kvlist);
		return 0;
	}

	if (rte_kvargs_process(kvlist, key,
			       check_devargs_handler, NULL) < 0) {
		rte_kvargs_free(kvlist);
		return 0;
	}
	rte_kvargs_free(kvlist);

	return 1;
}

static int
dpaa2_dev_init(struct rte_eth_dev *eth_dev)
{
	struct rte_device *dev = eth_dev->device;
	struct rte_dpaa2_device *dpaa2_dev;
	struct fsl_mc_io *dpni_dev;
	struct dpni_attr attr;
	struct dpaa2_dev_priv *priv = eth_dev->data->dev_private;
	struct dpni_buffer_layout layout;
	int ret, hw_id, i;

	PMD_INIT_FUNC_TRACE();

	dpni_dev = rte_malloc(NULL, sizeof(struct fsl_mc_io), 0);
	if (!dpni_dev) {
		DPAA2_PMD_ERR("Memory allocation failed for dpni device");
		return -1;
	}
	dpni_dev->regs = dpaa2_get_mcp_ptr(MC_PORTAL_INDEX);
	eth_dev->process_private = (void *)dpni_dev;

	/* For secondary processes, the primary has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		/* In case of secondary, only burst and ops API need to be
		 * plugged.
		 */
		eth_dev->dev_ops = &dpaa2_ethdev_ops;
		eth_dev->rx_queue_count = dpaa2_dev_rx_queue_count;
		if (dpaa2_get_devargs(dev->devargs, DRIVER_LOOPBACK_MODE))
			eth_dev->rx_pkt_burst = dpaa2_dev_loopback_rx;
		else if (dpaa2_get_devargs(dev->devargs,
					DRIVER_NO_PREFETCH_MODE))
			eth_dev->rx_pkt_burst = dpaa2_dev_rx;
		else
			eth_dev->rx_pkt_burst = dpaa2_dev_prefetch_rx;
		eth_dev->tx_pkt_burst = dpaa2_dev_tx;
		return 0;
	}

	dpaa2_dev = container_of(dev, struct rte_dpaa2_device, device);

	hw_id = dpaa2_dev->object_id;
	ret = dpni_open(dpni_dev, CMD_PRI_LOW, hw_id, &priv->token);
	if (ret) {
		DPAA2_PMD_ERR(
			     "Failure in opening dpni@%d with err code %d",
			     hw_id, ret);
		rte_free(dpni_dev);
		return -1;
	}

	if (eth_dev->data->dev_conf.lpbk_mode)
		dpaa2_dev_recycle_deconfig(eth_dev);

	/* Clean the device first */
	ret = dpni_reset(dpni_dev, CMD_PRI_LOW, priv->token);
	if (ret) {
		DPAA2_PMD_ERR("Failure cleaning dpni@%d with err code %d",
			      hw_id, ret);
		goto init_err;
	}

	ret = dpni_get_attributes(dpni_dev, CMD_PRI_LOW, priv->token, &attr);
	if (ret) {
		DPAA2_PMD_ERR(
			     "Failure in get dpni@%d attribute, err code %d",
			     hw_id, ret);
		goto init_err;
	}

	priv->num_rx_tc = attr.num_rx_tcs;
	priv->num_tx_tc = attr.num_tx_tcs;
	priv->qos_entries = attr.qos_entries;
	priv->fs_entries = attr.fs_entries;
	priv->dist_queues = attr.num_queues;
	priv->num_channels = attr.num_channels;
	priv->channel_inuse = 0;
	rte_spinlock_init(&priv->lpbk_qp_lock);

	/* only if the custom CG is enabled */
	if (attr.options & DPNI_OPT_CUSTOM_CG)
		priv->max_cgs = attr.num_cgs;
	else
		priv->max_cgs = 0;

	for (i = 0; i < priv->max_cgs; i++)
		priv->cgid_in_use[i] = 0;

	for (i = 0; i < attr.num_rx_tcs; i++)
		priv->nb_rx_queues += attr.num_queues;

	priv->nb_tx_queues = attr.num_tx_tcs * attr.num_channels;

	DPAA2_PMD_DEBUG("RX-TC= %d, rx_queues= %d, tx_queues=%d, max_cgs=%d",
			priv->num_rx_tc, priv->nb_rx_queues,
			priv->nb_tx_queues, priv->max_cgs);

	priv->hw = dpni_dev;
	priv->hw_id = hw_id;
	priv->options = attr.options;
	priv->max_mac_filters = attr.mac_filter_entries;
	priv->max_vlan_filters = attr.vlan_filter_entries;
	priv->flags = 0;
#if defined(RTE_LIBRTE_IEEE1588)
	printf("DPDK IEEE1588 is enabled\n");
	priv->flags |= DPAA2_TX_CONF_ENABLE;
#endif
	/* Used with ``fslmc:dpni.1,drv_tx_conf=1`` */
	if (dpaa2_get_devargs(dev->devargs, DRIVER_TX_CONF)) {
		priv->flags |= DPAA2_TX_CONF_ENABLE;
		DPAA2_PMD_INFO("TX_CONF Enabled");
	}

	if (dpaa2_get_devargs(dev->devargs, DRIVER_ERROR_QUEUE)) {
		dpaa2_enable_err_queue = 1;
		DPAA2_PMD_INFO("Enable error queue");
	}

	/* Allocate memory for hardware structure for queues */
	ret = dpaa2_alloc_rx_tx_queues(eth_dev);
	if (ret) {
		DPAA2_PMD_ERR("Queue allocation Failed");
		goto init_err;
	}

	/* Allocate memory for storing MAC addresses.
	 * Table of mac_filter_entries size is allocated so that RTE ether lib
	 * can add MAC entries when rte_eth_dev_mac_addr_add is called.
	 */
	eth_dev->data->mac_addrs = rte_zmalloc("dpni",
		RTE_ETHER_ADDR_LEN * attr.mac_filter_entries, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		DPAA2_PMD_ERR(
		   "Failed to allocate %d bytes needed to store MAC addresses",
		   RTE_ETHER_ADDR_LEN * attr.mac_filter_entries);
		ret = -ENOMEM;
		goto init_err;
	}

	ret = populate_mac_addr(dpni_dev, priv, &eth_dev->data->mac_addrs[0]);
	if (ret) {
		DPAA2_PMD_ERR("Unable to fetch MAC Address for device");
		rte_free(eth_dev->data->mac_addrs);
		eth_dev->data->mac_addrs = NULL;
		goto init_err;
	}

	/* ... tx buffer layout ... */
	memset(&layout, 0, sizeof(struct dpni_buffer_layout));
	if (priv->flags & DPAA2_TX_CONF_ENABLE) {
		layout.options = DPNI_BUF_LAYOUT_OPT_FRAME_STATUS |
				 DPNI_BUF_LAYOUT_OPT_TIMESTAMP;
		layout.pass_timestamp = true;
	} else {
		layout.options = DPNI_BUF_LAYOUT_OPT_FRAME_STATUS;
	}
	layout.pass_frame_status = 1;
	ret = dpni_set_buffer_layout(dpni_dev, CMD_PRI_LOW, priv->token,
				     DPNI_QUEUE_TX, &layout);
	if (ret) {
		DPAA2_PMD_ERR("Error (%d) in setting tx buffer layout", ret);
		goto init_err;
	}

	/* ... tx-conf and error buffer layout ... */
	memset(&layout, 0, sizeof(struct dpni_buffer_layout));
	if (priv->flags & DPAA2_TX_CONF_ENABLE) {
		layout.options = DPNI_BUF_LAYOUT_OPT_TIMESTAMP;
		layout.pass_timestamp = true;
	}
	layout.options |= DPNI_BUF_LAYOUT_OPT_FRAME_STATUS;
	layout.pass_frame_status = 1;
	ret = dpni_set_buffer_layout(dpni_dev, CMD_PRI_LOW, priv->token,
				     DPNI_QUEUE_TX_CONFIRM, &layout);
	if (ret) {
		DPAA2_PMD_ERR("Error (%d) in setting tx-conf buffer layout",
			     ret);
		goto init_err;
	}

	eth_dev->dev_ops = &dpaa2_ethdev_ops;

	if (dpaa2_get_devargs(dev->devargs, DRIVER_LOOPBACK_MODE)) {
		eth_dev->rx_pkt_burst = dpaa2_dev_loopback_rx;
		DPAA2_PMD_INFO("Loopback mode");
	} else if (dpaa2_get_devargs(dev->devargs, DRIVER_NO_PREFETCH_MODE)) {
		eth_dev->rx_pkt_burst = dpaa2_dev_rx;
		DPAA2_PMD_INFO("No Prefetch mode");
	} else {
		eth_dev->rx_pkt_burst = dpaa2_dev_prefetch_rx;
	}
	eth_dev->tx_pkt_burst = dpaa2_dev_tx;

	/* Init fields w.r.t. classification */
	memset(&priv->extract.qos_key_extract, 0,
		sizeof(struct dpaa2_key_extract));
	priv->extract.qos_extract_param = (size_t)rte_malloc(NULL, 256, 64);
	if (!priv->extract.qos_extract_param) {
		DPAA2_PMD_ERR(" Error(%d) in allocation resources for flow "
			    " classification ", ret);
		goto init_err;
	}
	priv->extract.qos_key_extract.key_info.ipv4_src_offset =
		IP_ADDRESS_OFFSET_INVALID;
	priv->extract.qos_key_extract.key_info.ipv4_dst_offset =
		IP_ADDRESS_OFFSET_INVALID;
	priv->extract.qos_key_extract.key_info.ipv6_src_offset =
		IP_ADDRESS_OFFSET_INVALID;
	priv->extract.qos_key_extract.key_info.ipv6_dst_offset =
		IP_ADDRESS_OFFSET_INVALID;

	for (i = 0; i < MAX_TCS; i++) {
		memset(&priv->extract.tc_key_extract[i], 0,
			sizeof(struct dpaa2_key_extract));
		priv->extract.tc_extract_param[i] =
			(size_t)rte_malloc(NULL, 256, 64);
		if (!priv->extract.tc_extract_param[i]) {
			DPAA2_PMD_ERR(" Error(%d) in allocation resources for flow classification",
				     ret);
			goto init_err;
		}
		priv->extract.tc_key_extract[i].key_info.ipv4_src_offset =
			IP_ADDRESS_OFFSET_INVALID;
		priv->extract.tc_key_extract[i].key_info.ipv4_dst_offset =
			IP_ADDRESS_OFFSET_INVALID;
		priv->extract.tc_key_extract[i].key_info.ipv6_src_offset =
			IP_ADDRESS_OFFSET_INVALID;
		priv->extract.tc_key_extract[i].key_info.ipv6_dst_offset =
			IP_ADDRESS_OFFSET_INVALID;
	}

	ret = dpni_set_max_frame_length(dpni_dev, CMD_PRI_LOW, priv->token,
					RTE_ETHER_MAX_LEN - RTE_ETHER_CRC_LEN
					+ VLAN_TAG_SIZE);
	if (ret) {
		DPAA2_PMD_ERR("Unable to set mtu. check config");
		goto init_err;
	}

	/*TODO To enable soft parser support DPAA2 driver needs to integrate
	 * with external entity to receive byte code for software sequence
	 * and same will be offload to the H/W using MC interface.
	 * Currently it is assumed that DPAA2 driver has byte code by some
	 * mean and same if offloaded to H/W.
	 */
	if (getenv("DPAA2_ENABLE_SOFT_PARSER")) {
		WRIOP_SS_INITIALIZER(priv);
		ret = dpaa2_eth_load_wriop_soft_parser(priv, DPNI_SS_INGRESS);
		if (ret < 0) {
			DPAA2_PMD_ERR(" Error(%d) in loading softparser\n",
				      ret);
			return ret;
		}

		ret = dpaa2_eth_enable_wriop_soft_parser(priv,
							 DPNI_SS_INGRESS);
		if (ret < 0) {
			DPAA2_PMD_ERR(" Error(%d) in enabling softparser\n",
				      ret);
			return ret;
		}
	}
	RTE_LOG(INFO, PMD, "%s: netdev created, connected to %s\n",
		eth_dev->data->name, dpaa2_dev->ep_name);

	return 0;
init_err:
	dpaa2_dev_close(eth_dev);

	return ret;
}

int dpaa2_dev_is_dpaa2(struct rte_eth_dev *dev)
{
	return dev->device->driver == &rte_dpaa2_pmd.driver;
}

static int
rte_dpaa2_probe(struct rte_dpaa2_driver *dpaa2_drv,
		struct rte_dpaa2_device *dpaa2_dev)
{
	struct rte_eth_dev *eth_dev;
	struct dpaa2_dev_priv *dev_priv;
	int diag;

	if ((DPAA2_MBUF_HW_ANNOTATION + DPAA2_FD_PTA_SIZE) >
		RTE_PKTMBUF_HEADROOM) {
		DPAA2_PMD_ERR(
		"RTE_PKTMBUF_HEADROOM(%d) shall be > DPAA2 Annotation req(%d)",
		RTE_PKTMBUF_HEADROOM,
		DPAA2_MBUF_HW_ANNOTATION + DPAA2_FD_PTA_SIZE);

		return -1;
	}

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		eth_dev = rte_eth_dev_allocate(dpaa2_dev->device.name);
		if (!eth_dev)
			return -ENODEV;
		dev_priv = rte_zmalloc("ethdev private structure",
				       sizeof(struct dpaa2_dev_priv),
				       RTE_CACHE_LINE_SIZE);
		if (dev_priv == NULL) {
			DPAA2_PMD_CRIT(
				"Unable to allocate memory for private data");
			rte_eth_dev_release_port(eth_dev);
			return -ENOMEM;
		}
		eth_dev->data->dev_private = (void *)dev_priv;
		/* Store a pointer to eth_dev in dev_private */
		dev_priv->eth_dev = eth_dev;
	} else {
		eth_dev = rte_eth_dev_attach_secondary(dpaa2_dev->device.name);
		if (!eth_dev) {
			DPAA2_PMD_DEBUG("returning enodev");
			return -ENODEV;
		}
	}

	eth_dev->device = &dpaa2_dev->device;

	dpaa2_dev->eth_dev = eth_dev;
	eth_dev->data->rx_mbuf_alloc_failed = 0;

	if (dpaa2_drv->drv_flags & RTE_DPAA2_DRV_INTR_LSC)
		eth_dev->data->dev_flags |= RTE_ETH_DEV_INTR_LSC;

	eth_dev->data->dev_flags |= RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS;

	/* Invoke PMD device initialization function */
	diag = dpaa2_dev_init(eth_dev);
	if (diag == 0) {
		rte_eth_dev_probing_finish(eth_dev);
		return 0;
	}

	rte_eth_dev_release_port(eth_dev);
	return diag;
}

static int
rte_dpaa2_remove(struct rte_dpaa2_device *dpaa2_dev)
{
	struct rte_eth_dev *eth_dev;
	int ret;

	eth_dev = dpaa2_dev->eth_dev;
	dpaa2_dev_close(eth_dev);
	ret = rte_eth_dev_release_port(eth_dev);

	return ret;
}

static struct rte_dpaa2_driver rte_dpaa2_pmd = {
	.drv_flags = RTE_DPAA2_DRV_INTR_LSC | RTE_DPAA2_DRV_IOVA_AS_VA,
	.drv_type = DPAA2_ETH,
	.probe = rte_dpaa2_probe,
	.remove = rte_dpaa2_remove,
};

RTE_PMD_REGISTER_DPAA2(NET_DPAA2_PMD_DRIVER_NAME, rte_dpaa2_pmd);
RTE_PMD_REGISTER_PARAM_STRING(NET_DPAA2_PMD_DRIVER_NAME,
		DRIVER_LOOPBACK_MODE "=<int> "
		DRIVER_NO_PREFETCH_MODE "=<int>"
		DRIVER_TX_CONF "=<int>"
		DRIVER_ERROR_QUEUE "=<int>");
RTE_LOG_REGISTER_DEFAULT(dpaa2_logtype_pmd, NOTICE);
