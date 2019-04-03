/* SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright 2016 Freescale Semiconductor, Inc. All rights reserved.
 *   Copyright 2017 NXP
 *
 */
/* System headers */
#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>
#include <limits.h>
#include <sched.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/syscall.h>

#include <rte_string_fns.h>
#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_interrupts.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_pci.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_memory.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_alarm.h>
#include <rte_ether.h>
#include <rte_ethdev_driver.h>
#include <rte_malloc.h>
#include <rte_ring.h>

#include <rte_dpaa_bus.h>
#include <rte_dpaa_logs.h>
#include <dpaa_mempool.h>

#include <dpaa_ethdev.h>
#include <dpaa_rxtx.h>
#include <rte_pmd_dpaa.h>

#include <fsl_usd.h>
#include <fsl_qman.h>
#include <fsl_bman.h>
#include <fsl_fman.h>

/* Supported Rx offloads */
static uint64_t dev_rx_offloads_sup =
		DEV_RX_OFFLOAD_JUMBO_FRAME |
		DEV_RX_OFFLOAD_SCATTER;

/* Rx offloads which cannot be disabled */
static uint64_t dev_rx_offloads_nodis =
		DEV_RX_OFFLOAD_IPV4_CKSUM |
		DEV_RX_OFFLOAD_UDP_CKSUM |
		DEV_RX_OFFLOAD_TCP_CKSUM |
		DEV_RX_OFFLOAD_OUTER_IPV4_CKSUM;

/* Supported Tx offloads */
static uint64_t dev_tx_offloads_sup;

/* Tx offloads which cannot be disabled */
static uint64_t dev_tx_offloads_nodis =
		DEV_TX_OFFLOAD_IPV4_CKSUM |
		DEV_TX_OFFLOAD_UDP_CKSUM |
		DEV_TX_OFFLOAD_TCP_CKSUM |
		DEV_TX_OFFLOAD_SCTP_CKSUM |
		DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM |
		DEV_TX_OFFLOAD_MULTI_SEGS |
		DEV_TX_OFFLOAD_MT_LOCKFREE |
		DEV_TX_OFFLOAD_MBUF_FAST_FREE;

/* Keep track of whether QMAN and BMAN have been globally initialized */
static int is_global_init;
static int default_q;	/* use default queue - FMC is not executed*/
/* At present we only allow up to 4 push mode queues as default - as each of
 * this queue need dedicated portal and we are short of portals.
 */
#define DPAA_MAX_PUSH_MODE_QUEUE       8
#define DPAA_DEFAULT_PUSH_MODE_QUEUE   4

static int dpaa_push_mode_max_queue = DPAA_DEFAULT_PUSH_MODE_QUEUE;
static int dpaa_push_queue_idx; /* Queue index which are in push mode*/


/* Per FQ Taildrop in frame count */
static unsigned int td_threshold = CGR_RX_PERFQ_THRESH;

struct rte_dpaa_xstats_name_off {
	char name[RTE_ETH_XSTATS_NAME_SIZE];
	uint32_t offset;
};

static const struct rte_dpaa_xstats_name_off dpaa_xstats_strings[] = {
	{"rx_align_err",
		offsetof(struct dpaa_if_stats, raln)},
	{"rx_valid_pause",
		offsetof(struct dpaa_if_stats, rxpf)},
	{"rx_fcs_err",
		offsetof(struct dpaa_if_stats, rfcs)},
	{"rx_vlan_frame",
		offsetof(struct dpaa_if_stats, rvlan)},
	{"rx_frame_err",
		offsetof(struct dpaa_if_stats, rerr)},
	{"rx_drop_err",
		offsetof(struct dpaa_if_stats, rdrp)},
	{"rx_undersized",
		offsetof(struct dpaa_if_stats, rund)},
	{"rx_oversize_err",
		offsetof(struct dpaa_if_stats, rovr)},
	{"rx_fragment_pkt",
		offsetof(struct dpaa_if_stats, rfrg)},
	{"tx_valid_pause",
		offsetof(struct dpaa_if_stats, txpf)},
	{"tx_fcs_err",
		offsetof(struct dpaa_if_stats, terr)},
	{"tx_vlan_frame",
		offsetof(struct dpaa_if_stats, tvlan)},
	{"rx_undersized",
		offsetof(struct dpaa_if_stats, tund)},
};

static struct rte_dpaa_driver rte_dpaa_pmd;

static void
dpaa_eth_dev_info(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info);

static inline void
dpaa_poll_queue_default_config(struct qm_mcc_initfq *opts)
{
	memset(opts, 0, sizeof(struct qm_mcc_initfq));
	opts->we_mask = QM_INITFQ_WE_FQCTRL | QM_INITFQ_WE_CONTEXTA;
	opts->fqd.fq_ctrl = QM_FQCTRL_AVOIDBLOCK | QM_FQCTRL_CTXASTASHING |
			   QM_FQCTRL_PREFERINCACHE;
	opts->fqd.context_a.stashing.exclusive = 0;
	if (dpaa_svr_family != SVR_LS1046A_FAMILY)
		opts->fqd.context_a.stashing.annotation_cl =
						DPAA_IF_RX_ANNOTATION_STASH;
	opts->fqd.context_a.stashing.data_cl = DPAA_IF_RX_DATA_STASH;
	opts->fqd.context_a.stashing.context_cl = DPAA_IF_RX_CONTEXT_STASH;
}

static int
dpaa_mtu_set(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct dpaa_if *dpaa_intf = dev->data->dev_private;
	uint32_t frame_size = mtu + ETHER_HDR_LEN + ETHER_CRC_LEN
				+ VLAN_TAG_SIZE;
	uint32_t buffsz = dev->data->min_rx_buf_size - RTE_PKTMBUF_HEADROOM;

	PMD_INIT_FUNC_TRACE();

	if (mtu < ETHER_MIN_MTU || frame_size > DPAA_MAX_RX_PKT_LEN)
		return -EINVAL;
	/*
	 * Refuse mtu that requires the support of scattered packets
	 * when this feature has not been enabled before.
	 */
	if (dev->data->min_rx_buf_size &&
		!dev->data->scattered_rx && frame_size > buffsz) {
		DPAA_PMD_ERR("SG not enabled, will not fit in one buffer");
		return -EINVAL;
	}

	/* check <seg size> * <max_seg>  >= max_frame */
	if (dev->data->min_rx_buf_size && dev->data->scattered_rx &&
		(frame_size > buffsz * DPAA_SGT_MAX_ENTRIES)) {
		DPAA_PMD_ERR("Too big to fit for Max SG list %d",
				buffsz * DPAA_SGT_MAX_ENTRIES);
		return -EINVAL;
	}

	if (frame_size > ETHER_MAX_LEN)
		dev->data->dev_conf.rxmode.offloads &=
						DEV_RX_OFFLOAD_JUMBO_FRAME;
	else
		dev->data->dev_conf.rxmode.offloads &=
						~DEV_RX_OFFLOAD_JUMBO_FRAME;

	dev->data->dev_conf.rxmode.max_rx_pkt_len = frame_size;

	fman_if_set_maxfrm(dpaa_intf->fif, frame_size);

	return 0;
}

static int
dpaa_eth_dev_configure(struct rte_eth_dev *dev)
{
	struct dpaa_if *dpaa_intf = dev->data->dev_private;
	struct rte_eth_conf *eth_conf = &dev->data->dev_conf;
	uint64_t rx_offloads = eth_conf->rxmode.offloads;
	uint64_t tx_offloads = eth_conf->txmode.offloads;

	PMD_INIT_FUNC_TRACE();

	/* Rx offloads validation */
	if (dev_rx_offloads_nodis & ~rx_offloads) {
		DPAA_PMD_WARN(
		"Rx offloads non configurable - requested 0x%" PRIx64
		" ignored 0x%" PRIx64,
			rx_offloads, dev_rx_offloads_nodis);
	}

	/* Tx offloads validation */
	if (dev_tx_offloads_nodis & ~tx_offloads) {
		DPAA_PMD_WARN(
		"Tx offloads non configurable - requested 0x%" PRIx64
		" ignored 0x%" PRIx64,
			tx_offloads, dev_tx_offloads_nodis);
	}

	if (rx_offloads & DEV_RX_OFFLOAD_JUMBO_FRAME) {
		uint32_t max_len;

		DPAA_PMD_DEBUG("enabling jumbo");

		if (dev->data->dev_conf.rxmode.max_rx_pkt_len <=
		    DPAA_MAX_RX_PKT_LEN)
			max_len = dev->data->dev_conf.rxmode.max_rx_pkt_len;
		else {
			DPAA_PMD_INFO("enabling jumbo override conf max len=%d "
				"supported is %d",
				dev->data->dev_conf.rxmode.max_rx_pkt_len,
				DPAA_MAX_RX_PKT_LEN);
			max_len = DPAA_MAX_RX_PKT_LEN;
		}

		fman_if_set_maxfrm(dpaa_intf->fif, max_len);
		dev->data->mtu = max_len
				- ETHER_HDR_LEN - ETHER_CRC_LEN - VLAN_TAG_SIZE;
	}

	if (rx_offloads & DEV_RX_OFFLOAD_SCATTER) {
		DPAA_PMD_DEBUG("enabling scatter mode");
		fman_if_set_sg(dpaa_intf->fif, 1);
		dev->data->scattered_rx = 1;
	}

	return 0;
}

static const uint32_t *
dpaa_supported_ptypes_get(struct rte_eth_dev *dev)
{
	static const uint32_t ptypes[] = {
		RTE_PTYPE_L2_ETHER,
		RTE_PTYPE_L2_ETHER_VLAN,
		RTE_PTYPE_L2_ETHER_ARP,
		RTE_PTYPE_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_L4_ICMP,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_L4_FRAG,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_L4_SCTP
	};

	PMD_INIT_FUNC_TRACE();

	if (dev->rx_pkt_burst == dpaa_eth_queue_rx)
		return ptypes;
	return NULL;
}

static int dpaa_eth_dev_start(struct rte_eth_dev *dev)
{
	struct dpaa_if *dpaa_intf = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();

	/* Change tx callback to the real one */
	dev->tx_pkt_burst = dpaa_eth_queue_tx;
	fman_if_enable_rx(dpaa_intf->fif);

	return 0;
}

static void dpaa_eth_dev_stop(struct rte_eth_dev *dev)
{
	struct dpaa_if *dpaa_intf = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();

	fman_if_disable_rx(dpaa_intf->fif);
	dev->tx_pkt_burst = dpaa_eth_tx_drop_all;
}

static void dpaa_eth_dev_close(struct rte_eth_dev *dev)
{
	PMD_INIT_FUNC_TRACE();

	dpaa_eth_dev_stop(dev);
}

static int
dpaa_fw_version_get(struct rte_eth_dev *dev __rte_unused,
		     char *fw_version,
		     size_t fw_size)
{
	int ret;
	FILE *svr_file = NULL;
	unsigned int svr_ver = 0;

	PMD_INIT_FUNC_TRACE();

	svr_file = fopen(DPAA_SOC_ID_FILE, "r");
	if (!svr_file) {
		DPAA_PMD_ERR("Unable to open SoC device");
		return -ENOTSUP; /* Not supported on this infra */
	}
	if (fscanf(svr_file, "svr:%x", &svr_ver) > 0)
		dpaa_svr_family = svr_ver & SVR_MASK;
	else
		DPAA_PMD_ERR("Unable to read SoC device");

	fclose(svr_file);

	ret = snprintf(fw_version, fw_size, "SVR:%x-fman-v%x",
		       svr_ver, fman_ip_rev);
	ret += 1; /* add the size of '\0' */

	if (fw_size < (uint32_t)ret)
		return ret;
	else
		return 0;
}

static void dpaa_eth_dev_info(struct rte_eth_dev *dev,
			      struct rte_eth_dev_info *dev_info)
{
	struct dpaa_if *dpaa_intf = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();

	dev_info->max_rx_queues = dpaa_intf->nb_rx_queues;
	dev_info->max_tx_queues = dpaa_intf->nb_tx_queues;
	dev_info->max_rx_pktlen = DPAA_MAX_RX_PKT_LEN;
	dev_info->max_mac_addrs = DPAA_MAX_MAC_FILTER;
	dev_info->max_hash_mac_addrs = 0;
	dev_info->max_vfs = 0;
	dev_info->max_vmdq_pools = ETH_16_POOLS;
	dev_info->flow_type_rss_offloads = DPAA_RSS_OFFLOAD_ALL;

	if (dpaa_intf->fif->mac_type == fman_mac_1g)
		dev_info->speed_capa = ETH_LINK_SPEED_1G;
	else if (dpaa_intf->fif->mac_type == fman_mac_10g)
		dev_info->speed_capa = (ETH_LINK_SPEED_1G | ETH_LINK_SPEED_10G);
	else
		DPAA_PMD_ERR("invalid link_speed: %s, %d",
			     dpaa_intf->name, dpaa_intf->fif->mac_type);

	dev_info->rx_offload_capa = dev_rx_offloads_sup |
					dev_rx_offloads_nodis;
	dev_info->tx_offload_capa = dev_tx_offloads_sup |
					dev_tx_offloads_nodis;
	dev_info->default_rxportconf.burst_size = DPAA_DEF_RX_BURST_SIZE;
	dev_info->default_txportconf.burst_size = DPAA_DEF_TX_BURST_SIZE;
}

static int dpaa_eth_link_update(struct rte_eth_dev *dev,
				int wait_to_complete __rte_unused)
{
	struct dpaa_if *dpaa_intf = dev->data->dev_private;
	struct rte_eth_link *link = &dev->data->dev_link;

	PMD_INIT_FUNC_TRACE();

	if (dpaa_intf->fif->mac_type == fman_mac_1g)
		link->link_speed = ETH_SPEED_NUM_1G;
	else if (dpaa_intf->fif->mac_type == fman_mac_10g)
		link->link_speed = ETH_SPEED_NUM_10G;
	else
		DPAA_PMD_ERR("invalid link_speed: %s, %d",
			     dpaa_intf->name, dpaa_intf->fif->mac_type);

	link->link_status = dpaa_intf->valid;
	link->link_duplex = ETH_LINK_FULL_DUPLEX;
	link->link_autoneg = ETH_LINK_AUTONEG;
	return 0;
}

static int dpaa_eth_stats_get(struct rte_eth_dev *dev,
			       struct rte_eth_stats *stats)
{
	struct dpaa_if *dpaa_intf = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();

	fman_if_stats_get(dpaa_intf->fif, stats);
	return 0;
}

static void dpaa_eth_stats_reset(struct rte_eth_dev *dev)
{
	struct dpaa_if *dpaa_intf = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();

	fman_if_stats_reset(dpaa_intf->fif);
}

static int
dpaa_dev_xstats_get(struct rte_eth_dev *dev, struct rte_eth_xstat *xstats,
		    unsigned int n)
{
	struct dpaa_if *dpaa_intf = dev->data->dev_private;
	unsigned int i = 0, num = RTE_DIM(dpaa_xstats_strings);
	uint64_t values[sizeof(struct dpaa_if_stats) / 8];

	if (n < num)
		return num;

	if (xstats == NULL)
		return 0;

	fman_if_stats_get_all(dpaa_intf->fif, values,
			      sizeof(struct dpaa_if_stats) / 8);

	for (i = 0; i < num; i++) {
		xstats[i].id = i;
		xstats[i].value = values[dpaa_xstats_strings[i].offset / 8];
	}
	return i;
}

static int
dpaa_xstats_get_names(__rte_unused struct rte_eth_dev *dev,
		      struct rte_eth_xstat_name *xstats_names,
		      unsigned int limit)
{
	unsigned int i, stat_cnt = RTE_DIM(dpaa_xstats_strings);

	if (limit < stat_cnt)
		return stat_cnt;

	if (xstats_names != NULL)
		for (i = 0; i < stat_cnt; i++)
			strlcpy(xstats_names[i].name,
				dpaa_xstats_strings[i].name,
				sizeof(xstats_names[i].name));

	return stat_cnt;
}

static int
dpaa_xstats_get_by_id(struct rte_eth_dev *dev, const uint64_t *ids,
		      uint64_t *values, unsigned int n)
{
	unsigned int i, stat_cnt = RTE_DIM(dpaa_xstats_strings);
	uint64_t values_copy[sizeof(struct dpaa_if_stats) / 8];

	if (!ids) {
		struct dpaa_if *dpaa_intf = dev->data->dev_private;

		if (n < stat_cnt)
			return stat_cnt;

		if (!values)
			return 0;

		fman_if_stats_get_all(dpaa_intf->fif, values_copy,
				      sizeof(struct dpaa_if_stats) / 8);

		for (i = 0; i < stat_cnt; i++)
			values[i] =
				values_copy[dpaa_xstats_strings[i].offset / 8];

		return stat_cnt;
	}

	dpaa_xstats_get_by_id(dev, NULL, values_copy, stat_cnt);

	for (i = 0; i < n; i++) {
		if (ids[i] >= stat_cnt) {
			DPAA_PMD_ERR("id value isn't valid");
			return -1;
		}
		values[i] = values_copy[ids[i]];
	}
	return n;
}

static int
dpaa_xstats_get_names_by_id(
	struct rte_eth_dev *dev,
	struct rte_eth_xstat_name *xstats_names,
	const uint64_t *ids,
	unsigned int limit)
{
	unsigned int i, stat_cnt = RTE_DIM(dpaa_xstats_strings);
	struct rte_eth_xstat_name xstats_names_copy[stat_cnt];

	if (!ids)
		return dpaa_xstats_get_names(dev, xstats_names, limit);

	dpaa_xstats_get_names(dev, xstats_names_copy, limit);

	for (i = 0; i < limit; i++) {
		if (ids[i] >= stat_cnt) {
			DPAA_PMD_ERR("id value isn't valid");
			return -1;
		}
		strcpy(xstats_names[i].name, xstats_names_copy[ids[i]].name);
	}
	return limit;
}

static void dpaa_eth_promiscuous_enable(struct rte_eth_dev *dev)
{
	struct dpaa_if *dpaa_intf = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();

	fman_if_promiscuous_enable(dpaa_intf->fif);
}

static void dpaa_eth_promiscuous_disable(struct rte_eth_dev *dev)
{
	struct dpaa_if *dpaa_intf = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();

	fman_if_promiscuous_disable(dpaa_intf->fif);
}

static void dpaa_eth_multicast_enable(struct rte_eth_dev *dev)
{
	struct dpaa_if *dpaa_intf = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();

	fman_if_set_mcast_filter_table(dpaa_intf->fif);
}

static void dpaa_eth_multicast_disable(struct rte_eth_dev *dev)
{
	struct dpaa_if *dpaa_intf = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();

	fman_if_reset_mcast_filter_table(dpaa_intf->fif);
}

static
int dpaa_eth_rx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
			    uint16_t nb_desc,
			    unsigned int socket_id __rte_unused,
			    const struct rte_eth_rxconf *rx_conf __rte_unused,
			    struct rte_mempool *mp)
{
	struct dpaa_if *dpaa_intf = dev->data->dev_private;
	struct qman_fq *rxq = &dpaa_intf->rx_queues[queue_idx];
	struct qm_mcc_initfq opts = {0};
	u32 flags = 0;
	int ret;
	u32 buffsz = rte_pktmbuf_data_room_size(mp) - RTE_PKTMBUF_HEADROOM;

	PMD_INIT_FUNC_TRACE();

	if (queue_idx >= dev->data->nb_rx_queues) {
		rte_errno = EOVERFLOW;
		DPAA_PMD_ERR("%p: queue index out of range (%u >= %u)",
		      (void *)dev, queue_idx, dev->data->nb_rx_queues);
		return -rte_errno;
	}

	DPAA_PMD_INFO("Rx queue setup for queue index: %d fq_id (0x%x)",
			queue_idx, rxq->fqid);

	/* Max packet can fit in single buffer */
	if (dev->data->dev_conf.rxmode.max_rx_pkt_len <= buffsz) {
		;
	} else if (dev->data->dev_conf.rxmode.offloads &
			DEV_RX_OFFLOAD_SCATTER) {
		if (dev->data->dev_conf.rxmode.max_rx_pkt_len >
			buffsz * DPAA_SGT_MAX_ENTRIES) {
			DPAA_PMD_ERR("max RxPkt size %d too big to fit "
				"MaxSGlist %d",
				dev->data->dev_conf.rxmode.max_rx_pkt_len,
				buffsz * DPAA_SGT_MAX_ENTRIES);
			rte_errno = EOVERFLOW;
			return -rte_errno;
		}
	} else {
		DPAA_PMD_WARN("The requested maximum Rx packet size (%u) is"
		     " larger than a single mbuf (%u) and scattered"
		     " mode has not been requested",
		     dev->data->dev_conf.rxmode.max_rx_pkt_len,
		     buffsz - RTE_PKTMBUF_HEADROOM);
	}

	if (!dpaa_intf->bp_info || dpaa_intf->bp_info->mp != mp) {
		struct fman_if_ic_params icp;
		uint32_t fd_offset;
		uint32_t bp_size;

		if (!mp->pool_data) {
			DPAA_PMD_ERR("Not an offloaded buffer pool!");
			return -1;
		}
		dpaa_intf->bp_info = DPAA_MEMPOOL_TO_POOL_INFO(mp);

		memset(&icp, 0, sizeof(icp));
		/* set ICEOF for to the default value , which is 0*/
		icp.iciof = DEFAULT_ICIOF;
		icp.iceof = DEFAULT_RX_ICEOF;
		icp.icsz = DEFAULT_ICSZ;
		fman_if_set_ic_params(dpaa_intf->fif, &icp);

		fd_offset = RTE_PKTMBUF_HEADROOM + DPAA_HW_BUF_RESERVE;
		fman_if_set_fdoff(dpaa_intf->fif, fd_offset);

		/* Buffer pool size should be equal to Dataroom Size*/
		bp_size = rte_pktmbuf_data_room_size(mp);
		fman_if_set_bp(dpaa_intf->fif, mp->size,
			       dpaa_intf->bp_info->bpid, bp_size);
		dpaa_intf->valid = 1;
		DPAA_PMD_DEBUG("if:%s fd_offset = %d offset = %d",
				dpaa_intf->name, fd_offset,
				fman_if_get_fdoff(dpaa_intf->fif));
	}
	DPAA_PMD_DEBUG("if:%s sg_on = %d, max_frm =%d", dpaa_intf->name,
		fman_if_get_sg_enable(dpaa_intf->fif),
		dev->data->dev_conf.rxmode.max_rx_pkt_len);
	/* checking if push mode only, no error check for now */
	if (dpaa_push_mode_max_queue > dpaa_push_queue_idx) {
		dpaa_push_queue_idx++;
		opts.we_mask = QM_INITFQ_WE_FQCTRL | QM_INITFQ_WE_CONTEXTA;
		opts.fqd.fq_ctrl = QM_FQCTRL_AVOIDBLOCK |
				   QM_FQCTRL_CTXASTASHING |
				   QM_FQCTRL_PREFERINCACHE;
		opts.fqd.context_a.stashing.exclusive = 0;
		/* In muticore scenario stashing becomes a bottleneck on LS1046.
		 * So do not enable stashing in this case
		 */
		if (dpaa_svr_family != SVR_LS1046A_FAMILY)
			opts.fqd.context_a.stashing.annotation_cl =
						DPAA_IF_RX_ANNOTATION_STASH;
		opts.fqd.context_a.stashing.data_cl = DPAA_IF_RX_DATA_STASH;
		opts.fqd.context_a.stashing.context_cl =
						DPAA_IF_RX_CONTEXT_STASH;

		/*Create a channel and associate given queue with the channel*/
		qman_alloc_pool_range((u32 *)&rxq->ch_id, 1, 1, 0);
		opts.we_mask = opts.we_mask | QM_INITFQ_WE_DESTWQ;
		opts.fqd.dest.channel = rxq->ch_id;
		opts.fqd.dest.wq = DPAA_IF_RX_PRIORITY;
		flags = QMAN_INITFQ_FLAG_SCHED;

		/* Configure tail drop */
		if (dpaa_intf->cgr_rx) {
			opts.we_mask |= QM_INITFQ_WE_CGID;
			opts.fqd.cgid = dpaa_intf->cgr_rx[queue_idx].cgrid;
			opts.fqd.fq_ctrl |= QM_FQCTRL_CGE;
		}
		ret = qman_init_fq(rxq, flags, &opts);
		if (ret) {
			DPAA_PMD_ERR("Channel/Q association failed. fqid 0x%x "
				"ret:%d(%s)", rxq->fqid, ret, strerror(ret));
			return ret;
		}
		if (dpaa_svr_family == SVR_LS1043A_FAMILY) {
			rxq->cb.dqrr_dpdk_pull_cb = dpaa_rx_cb_no_prefetch;
		} else {
			rxq->cb.dqrr_dpdk_pull_cb = dpaa_rx_cb;
			rxq->cb.dqrr_prepare = dpaa_rx_cb_prepare;
		}

		rxq->is_static = true;
	}
	rxq->bp_array = rte_dpaa_bpid_info;
	dev->data->rx_queues[queue_idx] = rxq;

	/* configure the CGR size as per the desc size */
	if (dpaa_intf->cgr_rx) {
		struct qm_mcc_initcgr cgr_opts = {0};

		/* Enable tail drop with cgr on this queue */
		qm_cgr_cs_thres_set64(&cgr_opts.cgr.cs_thres, nb_desc, 0);
		ret = qman_modify_cgr(dpaa_intf->cgr_rx, 0, &cgr_opts);
		if (ret) {
			DPAA_PMD_WARN(
				"rx taildrop modify fail on fqid %d (ret=%d)",
				rxq->fqid, ret);
		}
	}

	return 0;
}

int
dpaa_eth_eventq_attach(const struct rte_eth_dev *dev,
		int eth_rx_queue_id,
		u16 ch_id,
		const struct rte_event_eth_rx_adapter_queue_conf *queue_conf)
{
	int ret;
	u32 flags = 0;
	struct dpaa_if *dpaa_intf = dev->data->dev_private;
	struct qman_fq *rxq = &dpaa_intf->rx_queues[eth_rx_queue_id];
	struct qm_mcc_initfq opts = {0};

	if (dpaa_push_mode_max_queue)
		DPAA_PMD_WARN("PUSH mode q and EVENTDEV are not compatible\n"
			      "PUSH mode already enabled for first %d queues.\n"
			      "To disable set DPAA_PUSH_QUEUES_NUMBER to 0\n",
			      dpaa_push_mode_max_queue);

	dpaa_poll_queue_default_config(&opts);

	switch (queue_conf->ev.sched_type) {
	case RTE_SCHED_TYPE_ATOMIC:
		opts.fqd.fq_ctrl |= QM_FQCTRL_HOLDACTIVE;
		/* Reset FQCTRL_AVOIDBLOCK bit as it is unnecessary
		 * configuration with HOLD_ACTIVE setting
		 */
		opts.fqd.fq_ctrl &= (~QM_FQCTRL_AVOIDBLOCK);
		rxq->cb.dqrr_dpdk_cb = dpaa_rx_cb_atomic;
		break;
	case RTE_SCHED_TYPE_ORDERED:
		DPAA_PMD_ERR("Ordered queue schedule type is not supported\n");
		return -1;
	default:
		opts.fqd.fq_ctrl |= QM_FQCTRL_AVOIDBLOCK;
		rxq->cb.dqrr_dpdk_cb = dpaa_rx_cb_parallel;
		break;
	}

	opts.we_mask = opts.we_mask | QM_INITFQ_WE_DESTWQ;
	opts.fqd.dest.channel = ch_id;
	opts.fqd.dest.wq = queue_conf->ev.priority;

	if (dpaa_intf->cgr_rx) {
		opts.we_mask |= QM_INITFQ_WE_CGID;
		opts.fqd.cgid = dpaa_intf->cgr_rx[eth_rx_queue_id].cgrid;
		opts.fqd.fq_ctrl |= QM_FQCTRL_CGE;
	}

	flags = QMAN_INITFQ_FLAG_SCHED;

	ret = qman_init_fq(rxq, flags, &opts);
	if (ret) {
		DPAA_PMD_ERR("Ev-Channel/Q association failed. fqid 0x%x "
				"ret:%d(%s)", rxq->fqid, ret, strerror(ret));
		return ret;
	}

	/* copy configuration which needs to be filled during dequeue */
	memcpy(&rxq->ev, &queue_conf->ev, sizeof(struct rte_event));
	dev->data->rx_queues[eth_rx_queue_id] = rxq;

	return ret;
}

int
dpaa_eth_eventq_detach(const struct rte_eth_dev *dev,
		int eth_rx_queue_id)
{
	struct qm_mcc_initfq opts;
	int ret;
	u32 flags = 0;
	struct dpaa_if *dpaa_intf = dev->data->dev_private;
	struct qman_fq *rxq = &dpaa_intf->rx_queues[eth_rx_queue_id];

	dpaa_poll_queue_default_config(&opts);

	if (dpaa_intf->cgr_rx) {
		opts.we_mask |= QM_INITFQ_WE_CGID;
		opts.fqd.cgid = dpaa_intf->cgr_rx[eth_rx_queue_id].cgrid;
		opts.fqd.fq_ctrl |= QM_FQCTRL_CGE;
	}

	ret = qman_init_fq(rxq, flags, &opts);
	if (ret) {
		DPAA_PMD_ERR("init rx fqid %d failed with ret: %d",
			     rxq->fqid, ret);
	}

	rxq->cb.dqrr_dpdk_cb = NULL;
	dev->data->rx_queues[eth_rx_queue_id] = NULL;

	return 0;
}

static
void dpaa_eth_rx_queue_release(void *rxq __rte_unused)
{
	PMD_INIT_FUNC_TRACE();
}

static
int dpaa_eth_tx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
			    uint16_t nb_desc __rte_unused,
		unsigned int socket_id __rte_unused,
		const struct rte_eth_txconf *tx_conf __rte_unused)
{
	struct dpaa_if *dpaa_intf = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();

	if (queue_idx >= dev->data->nb_tx_queues) {
		rte_errno = EOVERFLOW;
		DPAA_PMD_ERR("%p: queue index out of range (%u >= %u)",
		      (void *)dev, queue_idx, dev->data->nb_tx_queues);
		return -rte_errno;
	}

	DPAA_PMD_INFO("Tx queue setup for queue index: %d fq_id (0x%x)",
			queue_idx, dpaa_intf->tx_queues[queue_idx].fqid);
	dev->data->tx_queues[queue_idx] = &dpaa_intf->tx_queues[queue_idx];
	return 0;
}

static void dpaa_eth_tx_queue_release(void *txq __rte_unused)
{
	PMD_INIT_FUNC_TRACE();
}

static uint32_t
dpaa_dev_rx_queue_count(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct dpaa_if *dpaa_intf = dev->data->dev_private;
	struct qman_fq *rxq = &dpaa_intf->rx_queues[rx_queue_id];
	u32 frm_cnt = 0;

	PMD_INIT_FUNC_TRACE();

	if (qman_query_fq_frm_cnt(rxq, &frm_cnt) == 0) {
		RTE_LOG(DEBUG, PMD, "RX frame count for q(%d) is %u\n",
			rx_queue_id, frm_cnt);
	}
	return frm_cnt;
}

static int dpaa_link_down(struct rte_eth_dev *dev)
{
	PMD_INIT_FUNC_TRACE();

	dpaa_eth_dev_stop(dev);
	return 0;
}

static int dpaa_link_up(struct rte_eth_dev *dev)
{
	PMD_INIT_FUNC_TRACE();

	dpaa_eth_dev_start(dev);
	return 0;
}

static int
dpaa_flow_ctrl_set(struct rte_eth_dev *dev,
		   struct rte_eth_fc_conf *fc_conf)
{
	struct dpaa_if *dpaa_intf = dev->data->dev_private;
	struct rte_eth_fc_conf *net_fc;

	PMD_INIT_FUNC_TRACE();

	if (!(dpaa_intf->fc_conf)) {
		dpaa_intf->fc_conf = rte_zmalloc(NULL,
			sizeof(struct rte_eth_fc_conf), MAX_CACHELINE);
		if (!dpaa_intf->fc_conf) {
			DPAA_PMD_ERR("unable to save flow control info");
			return -ENOMEM;
		}
	}
	net_fc = dpaa_intf->fc_conf;

	if (fc_conf->high_water < fc_conf->low_water) {
		DPAA_PMD_ERR("Incorrect Flow Control Configuration");
		return -EINVAL;
	}

	if (fc_conf->mode == RTE_FC_NONE) {
		return 0;
	} else if (fc_conf->mode == RTE_FC_TX_PAUSE ||
		 fc_conf->mode == RTE_FC_FULL) {
		fman_if_set_fc_threshold(dpaa_intf->fif, fc_conf->high_water,
					 fc_conf->low_water,
				dpaa_intf->bp_info->bpid);
		if (fc_conf->pause_time)
			fman_if_set_fc_quanta(dpaa_intf->fif,
					      fc_conf->pause_time);
	}

	/* Save the information in dpaa device */
	net_fc->pause_time = fc_conf->pause_time;
	net_fc->high_water = fc_conf->high_water;
	net_fc->low_water = fc_conf->low_water;
	net_fc->send_xon = fc_conf->send_xon;
	net_fc->mac_ctrl_frame_fwd = fc_conf->mac_ctrl_frame_fwd;
	net_fc->mode = fc_conf->mode;
	net_fc->autoneg = fc_conf->autoneg;

	return 0;
}

static int
dpaa_flow_ctrl_get(struct rte_eth_dev *dev,
		   struct rte_eth_fc_conf *fc_conf)
{
	struct dpaa_if *dpaa_intf = dev->data->dev_private;
	struct rte_eth_fc_conf *net_fc = dpaa_intf->fc_conf;
	int ret;

	PMD_INIT_FUNC_TRACE();

	if (net_fc) {
		fc_conf->pause_time = net_fc->pause_time;
		fc_conf->high_water = net_fc->high_water;
		fc_conf->low_water = net_fc->low_water;
		fc_conf->send_xon = net_fc->send_xon;
		fc_conf->mac_ctrl_frame_fwd = net_fc->mac_ctrl_frame_fwd;
		fc_conf->mode = net_fc->mode;
		fc_conf->autoneg = net_fc->autoneg;
		return 0;
	}
	ret = fman_if_get_fc_threshold(dpaa_intf->fif);
	if (ret) {
		fc_conf->mode = RTE_FC_TX_PAUSE;
		fc_conf->pause_time = fman_if_get_fc_quanta(dpaa_intf->fif);
	} else {
		fc_conf->mode = RTE_FC_NONE;
	}

	return 0;
}

static int
dpaa_dev_add_mac_addr(struct rte_eth_dev *dev,
			     struct ether_addr *addr,
			     uint32_t index,
			     __rte_unused uint32_t pool)
{
	int ret;
	struct dpaa_if *dpaa_intf = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();

	ret = fman_if_add_mac_addr(dpaa_intf->fif, addr->addr_bytes, index);

	if (ret)
		RTE_LOG(ERR, PMD, "error: Adding the MAC ADDR failed:"
			" err = %d", ret);
	return 0;
}

static void
dpaa_dev_remove_mac_addr(struct rte_eth_dev *dev,
			  uint32_t index)
{
	struct dpaa_if *dpaa_intf = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();

	fman_if_clear_mac_addr(dpaa_intf->fif, index);
}

static int
dpaa_dev_set_mac_addr(struct rte_eth_dev *dev,
		       struct ether_addr *addr)
{
	int ret;
	struct dpaa_if *dpaa_intf = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();

	ret = fman_if_add_mac_addr(dpaa_intf->fif, addr->addr_bytes, 0);
	if (ret)
		RTE_LOG(ERR, PMD, "error: Setting the MAC ADDR failed %d", ret);

	return ret;
}

static struct eth_dev_ops dpaa_devops = {
	.dev_configure		  = dpaa_eth_dev_configure,
	.dev_start		  = dpaa_eth_dev_start,
	.dev_stop		  = dpaa_eth_dev_stop,
	.dev_close		  = dpaa_eth_dev_close,
	.dev_infos_get		  = dpaa_eth_dev_info,
	.dev_supported_ptypes_get = dpaa_supported_ptypes_get,

	.rx_queue_setup		  = dpaa_eth_rx_queue_setup,
	.tx_queue_setup		  = dpaa_eth_tx_queue_setup,
	.rx_queue_release	  = dpaa_eth_rx_queue_release,
	.tx_queue_release	  = dpaa_eth_tx_queue_release,
	.rx_queue_count		  = dpaa_dev_rx_queue_count,

	.flow_ctrl_get		  = dpaa_flow_ctrl_get,
	.flow_ctrl_set		  = dpaa_flow_ctrl_set,

	.link_update		  = dpaa_eth_link_update,
	.stats_get		  = dpaa_eth_stats_get,
	.xstats_get		  = dpaa_dev_xstats_get,
	.xstats_get_by_id	  = dpaa_xstats_get_by_id,
	.xstats_get_names_by_id	  = dpaa_xstats_get_names_by_id,
	.xstats_get_names	  = dpaa_xstats_get_names,
	.xstats_reset		  = dpaa_eth_stats_reset,
	.stats_reset		  = dpaa_eth_stats_reset,
	.promiscuous_enable	  = dpaa_eth_promiscuous_enable,
	.promiscuous_disable	  = dpaa_eth_promiscuous_disable,
	.allmulticast_enable	  = dpaa_eth_multicast_enable,
	.allmulticast_disable	  = dpaa_eth_multicast_disable,
	.mtu_set		  = dpaa_mtu_set,
	.dev_set_link_down	  = dpaa_link_down,
	.dev_set_link_up	  = dpaa_link_up,
	.mac_addr_add		  = dpaa_dev_add_mac_addr,
	.mac_addr_remove	  = dpaa_dev_remove_mac_addr,
	.mac_addr_set		  = dpaa_dev_set_mac_addr,

	.fw_version_get		  = dpaa_fw_version_get,
};

static bool
is_device_supported(struct rte_eth_dev *dev, struct rte_dpaa_driver *drv)
{
	if (strcmp(dev->device->driver->name,
		   drv->driver.name))
		return false;

	return true;
}

static bool
is_dpaa_supported(struct rte_eth_dev *dev)
{
	return is_device_supported(dev, &rte_dpaa_pmd);
}

int
rte_pmd_dpaa_set_tx_loopback(uint8_t port, uint8_t on)
{
	struct rte_eth_dev *dev;
	struct dpaa_if *dpaa_intf;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_dpaa_supported(dev))
		return -ENOTSUP;

	dpaa_intf = dev->data->dev_private;

	if (on)
		fman_if_loopback_enable(dpaa_intf->fif);
	else
		fman_if_loopback_disable(dpaa_intf->fif);

	return 0;
}

static int dpaa_fc_set_default(struct dpaa_if *dpaa_intf)
{
	struct rte_eth_fc_conf *fc_conf;
	int ret;

	PMD_INIT_FUNC_TRACE();

	if (!(dpaa_intf->fc_conf)) {
		dpaa_intf->fc_conf = rte_zmalloc(NULL,
			sizeof(struct rte_eth_fc_conf), MAX_CACHELINE);
		if (!dpaa_intf->fc_conf) {
			DPAA_PMD_ERR("unable to save flow control info");
			return -ENOMEM;
		}
	}
	fc_conf = dpaa_intf->fc_conf;
	ret = fman_if_get_fc_threshold(dpaa_intf->fif);
	if (ret) {
		fc_conf->mode = RTE_FC_TX_PAUSE;
		fc_conf->pause_time = fman_if_get_fc_quanta(dpaa_intf->fif);
	} else {
		fc_conf->mode = RTE_FC_NONE;
	}

	return 0;
}

/* Initialise an Rx FQ */
static int dpaa_rx_queue_init(struct qman_fq *fq, struct qman_cgr *cgr_rx,
			      uint32_t fqid)
{
	struct qm_mcc_initfq opts = {0};
	int ret;
	u32 flags = QMAN_FQ_FLAG_NO_ENQUEUE;
	struct qm_mcc_initcgr cgr_opts = {
		.we_mask = QM_CGR_WE_CS_THRES |
				QM_CGR_WE_CSTD_EN |
				QM_CGR_WE_MODE,
		.cgr = {
			.cstd_en = QM_CGR_EN,
			.mode = QMAN_CGR_MODE_FRAME
		}
	};

	PMD_INIT_FUNC_TRACE();

	if (fqid) {
		ret = qman_reserve_fqid(fqid);
		if (ret) {
			DPAA_PMD_ERR("reserve rx fqid 0x%x failed with ret: %d",
				     fqid, ret);
			return -EINVAL;
		}
	} else {
		flags |= QMAN_FQ_FLAG_DYNAMIC_FQID;
	}
	DPAA_PMD_DEBUG("creating rx fq %p, fqid 0x%x", fq, fqid);
	ret = qman_create_fq(fqid, flags, fq);
	if (ret) {
		DPAA_PMD_ERR("create rx fqid 0x%x failed with ret: %d",
			fqid, ret);
		return ret;
	}
	fq->is_static = false;

	dpaa_poll_queue_default_config(&opts);

	if (cgr_rx) {
		/* Enable tail drop with cgr on this queue */
		qm_cgr_cs_thres_set64(&cgr_opts.cgr.cs_thres, td_threshold, 0);
		cgr_rx->cb = NULL;
		ret = qman_create_cgr(cgr_rx, QMAN_CGR_FLAG_USE_INIT,
				      &cgr_opts);
		if (ret) {
			DPAA_PMD_WARN(
				"rx taildrop init fail on rx fqid 0x%x(ret=%d)",
				fq->fqid, ret);
			goto without_cgr;
		}
		opts.we_mask |= QM_INITFQ_WE_CGID;
		opts.fqd.cgid = cgr_rx->cgrid;
		opts.fqd.fq_ctrl |= QM_FQCTRL_CGE;
	}
without_cgr:
	ret = qman_init_fq(fq, 0, &opts);
	if (ret)
		DPAA_PMD_ERR("init rx fqid 0x%x failed with ret:%d", fqid, ret);
	return ret;
}

/* Initialise a Tx FQ */
static int dpaa_tx_queue_init(struct qman_fq *fq,
			      struct fman_if *fman_intf)
{
	struct qm_mcc_initfq opts = {0};
	int ret;

	PMD_INIT_FUNC_TRACE();

	ret = qman_create_fq(0, QMAN_FQ_FLAG_DYNAMIC_FQID |
			     QMAN_FQ_FLAG_TO_DCPORTAL, fq);
	if (ret) {
		DPAA_PMD_ERR("create tx fq failed with ret: %d", ret);
		return ret;
	}
	opts.we_mask = QM_INITFQ_WE_DESTWQ | QM_INITFQ_WE_FQCTRL |
		       QM_INITFQ_WE_CONTEXTB | QM_INITFQ_WE_CONTEXTA;
	opts.fqd.dest.channel = fman_intf->tx_channel_id;
	opts.fqd.dest.wq = DPAA_IF_TX_PRIORITY;
	opts.fqd.fq_ctrl = QM_FQCTRL_PREFERINCACHE;
	opts.fqd.context_b = 0;
	/* no tx-confirmation */
	opts.fqd.context_a.hi = 0x80000000 | fman_dealloc_bufs_mask_hi;
	opts.fqd.context_a.lo = 0 | fman_dealloc_bufs_mask_lo;
	DPAA_PMD_DEBUG("init tx fq %p, fqid 0x%x", fq, fq->fqid);
	ret = qman_init_fq(fq, QMAN_INITFQ_FLAG_SCHED, &opts);
	if (ret)
		DPAA_PMD_ERR("init tx fqid 0x%x failed %d", fq->fqid, ret);
	return ret;
}

#ifdef RTE_LIBRTE_DPAA_DEBUG_DRIVER
/* Initialise a DEBUG FQ ([rt]x_error, rx_default). */
static int dpaa_debug_queue_init(struct qman_fq *fq, uint32_t fqid)
{
	struct qm_mcc_initfq opts = {0};
	int ret;

	PMD_INIT_FUNC_TRACE();

	ret = qman_reserve_fqid(fqid);
	if (ret) {
		DPAA_PMD_ERR("Reserve debug fqid %d failed with ret: %d",
			fqid, ret);
		return -EINVAL;
	}
	/* "map" this Rx FQ to one of the interfaces Tx FQID */
	DPAA_PMD_DEBUG("Creating debug fq %p, fqid %d", fq, fqid);
	ret = qman_create_fq(fqid, QMAN_FQ_FLAG_NO_ENQUEUE, fq);
	if (ret) {
		DPAA_PMD_ERR("create debug fqid %d failed with ret: %d",
			fqid, ret);
		return ret;
	}
	opts.we_mask = QM_INITFQ_WE_DESTWQ | QM_INITFQ_WE_FQCTRL;
	opts.fqd.dest.wq = DPAA_IF_DEBUG_PRIORITY;
	ret = qman_init_fq(fq, 0, &opts);
	if (ret)
		DPAA_PMD_ERR("init debug fqid %d failed with ret: %d",
			    fqid, ret);
	return ret;
}
#endif

/* Initialise a network interface */
static int
dpaa_dev_init(struct rte_eth_dev *eth_dev)
{
	int num_cores, num_rx_fqs, fqid;
	int loop, ret = 0;
	int dev_id;
	struct rte_dpaa_device *dpaa_device;
	struct dpaa_if *dpaa_intf;
	struct fm_eth_port_cfg *cfg;
	struct fman_if *fman_intf;
	struct fman_if_bpool *bp, *tmp_bp;
	uint32_t cgrid[DPAA_MAX_NUM_PCD_QUEUES];

	PMD_INIT_FUNC_TRACE();

	dpaa_intf = eth_dev->data->dev_private;
	/* For secondary processes, the primary has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		eth_dev->dev_ops = &dpaa_devops;
		/* Plugging of UCODE burst API not supported in Secondary */
		eth_dev->rx_pkt_burst = dpaa_eth_queue_rx;
		eth_dev->tx_pkt_burst = dpaa_eth_queue_tx;
#ifdef CONFIG_FSL_QMAN_FQ_LOOKUP
		qman_set_fq_lookup_table(
				dpaa_intf->rx_queues->qman_fq_lookup_table);
#endif
		return 0;
	}

	dpaa_device = DEV_TO_DPAA_DEVICE(eth_dev->device);
	dev_id = dpaa_device->id.dev_id;
	dpaa_intf = eth_dev->data->dev_private;
	cfg = &dpaa_netcfg->port_cfg[dev_id];
	fman_intf = cfg->fman_if;

	dpaa_intf->name = dpaa_device->name;

	/* save fman_if & cfg in the interface struture */
	dpaa_intf->fif = fman_intf;
	dpaa_intf->ifid = dev_id;
	dpaa_intf->cfg = cfg;

	/* Initialize Rx FQ's */
	if (default_q) {
		num_rx_fqs = DPAA_DEFAULT_NUM_PCD_QUEUES;
	} else {
		if (getenv("DPAA_NUM_RX_QUEUES"))
			num_rx_fqs = atoi(getenv("DPAA_NUM_RX_QUEUES"));
		else
			num_rx_fqs = DPAA_DEFAULT_NUM_PCD_QUEUES;
	}


	/* Each device can not have more than DPAA_MAX_NUM_PCD_QUEUES RX
	 * queues.
	 */
	if (num_rx_fqs <= 0 || num_rx_fqs > DPAA_MAX_NUM_PCD_QUEUES) {
		DPAA_PMD_ERR("Invalid number of RX queues\n");
		return -EINVAL;
	}

	dpaa_intf->rx_queues = rte_zmalloc(NULL,
		sizeof(struct qman_fq) * num_rx_fqs, MAX_CACHELINE);
	if (!dpaa_intf->rx_queues) {
		DPAA_PMD_ERR("Failed to alloc mem for RX queues\n");
		return -ENOMEM;
	}

	/* If congestion control is enabled globally*/
	if (td_threshold) {
		dpaa_intf->cgr_rx = rte_zmalloc(NULL,
			sizeof(struct qman_cgr) * num_rx_fqs, MAX_CACHELINE);
		if (!dpaa_intf->cgr_rx) {
			DPAA_PMD_ERR("Failed to alloc mem for cgr_rx\n");
			ret = -ENOMEM;
			goto free_rx;
		}

		ret = qman_alloc_cgrid_range(&cgrid[0], num_rx_fqs, 1, 0);
		if (ret != num_rx_fqs) {
			DPAA_PMD_WARN("insufficient CGRIDs available");
			ret = -EINVAL;
			goto free_rx;
		}
	} else {
		dpaa_intf->cgr_rx = NULL;
	}

	for (loop = 0; loop < num_rx_fqs; loop++) {
		if (default_q)
			fqid = cfg->rx_def;
		else
			fqid = DPAA_PCD_FQID_START + dpaa_intf->fif->mac_idx *
				DPAA_PCD_FQID_MULTIPLIER + loop;

		if (dpaa_intf->cgr_rx)
			dpaa_intf->cgr_rx[loop].cgrid = cgrid[loop];

		ret = dpaa_rx_queue_init(&dpaa_intf->rx_queues[loop],
			dpaa_intf->cgr_rx ? &dpaa_intf->cgr_rx[loop] : NULL,
			fqid);
		if (ret)
			goto free_rx;
		dpaa_intf->rx_queues[loop].dpaa_intf = dpaa_intf;
	}
	dpaa_intf->nb_rx_queues = num_rx_fqs;

	/* Initialise Tx FQs.free_rx Have as many Tx FQ's as number of cores */
	num_cores = rte_lcore_count();
	dpaa_intf->tx_queues = rte_zmalloc(NULL, sizeof(struct qman_fq) *
		num_cores, MAX_CACHELINE);
	if (!dpaa_intf->tx_queues) {
		DPAA_PMD_ERR("Failed to alloc mem for TX queues\n");
		ret = -ENOMEM;
		goto free_rx;
	}

	for (loop = 0; loop < num_cores; loop++) {
		ret = dpaa_tx_queue_init(&dpaa_intf->tx_queues[loop],
					 fman_intf);
		if (ret)
			goto free_tx;
		dpaa_intf->tx_queues[loop].dpaa_intf = dpaa_intf;
	}
	dpaa_intf->nb_tx_queues = num_cores;

#ifdef RTE_LIBRTE_DPAA_DEBUG_DRIVER
	dpaa_debug_queue_init(&dpaa_intf->debug_queues[
		DPAA_DEBUG_FQ_RX_ERROR], fman_intf->fqid_rx_err);
	dpaa_intf->debug_queues[DPAA_DEBUG_FQ_RX_ERROR].dpaa_intf = dpaa_intf;
	dpaa_debug_queue_init(&dpaa_intf->debug_queues[
		DPAA_DEBUG_FQ_TX_ERROR], fman_intf->fqid_tx_err);
	dpaa_intf->debug_queues[DPAA_DEBUG_FQ_TX_ERROR].dpaa_intf = dpaa_intf;
#endif

	DPAA_PMD_DEBUG("All frame queues created");

	/* Get the initial configuration for flow control */
	dpaa_fc_set_default(dpaa_intf);

	/* reset bpool list, initialize bpool dynamically */
	list_for_each_entry_safe(bp, tmp_bp, &cfg->fman_if->bpool_list, node) {
		list_del(&bp->node);
		rte_free(bp);
	}

	/* Populate ethdev structure */
	eth_dev->dev_ops = &dpaa_devops;
	eth_dev->rx_pkt_burst = dpaa_eth_queue_rx;
	eth_dev->tx_pkt_burst = dpaa_eth_tx_drop_all;

	/* Allocate memory for storing MAC addresses */
	eth_dev->data->mac_addrs = rte_zmalloc("mac_addr",
		ETHER_ADDR_LEN * DPAA_MAX_MAC_FILTER, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		DPAA_PMD_ERR("Failed to allocate %d bytes needed to "
						"store MAC addresses",
				ETHER_ADDR_LEN * DPAA_MAX_MAC_FILTER);
		ret = -ENOMEM;
		goto free_tx;
	}

	/* copy the primary mac address */
	ether_addr_copy(&fman_intf->mac_addr, &eth_dev->data->mac_addrs[0]);

	RTE_LOG(INFO, PMD, "net: dpaa: %s: %02x:%02x:%02x:%02x:%02x:%02x\n",
		dpaa_device->name,
		fman_intf->mac_addr.addr_bytes[0],
		fman_intf->mac_addr.addr_bytes[1],
		fman_intf->mac_addr.addr_bytes[2],
		fman_intf->mac_addr.addr_bytes[3],
		fman_intf->mac_addr.addr_bytes[4],
		fman_intf->mac_addr.addr_bytes[5]);

	/* Disable RX mode */
	fman_if_discard_rx_errors(fman_intf);
	fman_if_disable_rx(fman_intf);
	/* Disable promiscuous mode */
	fman_if_promiscuous_disable(fman_intf);
	/* Disable multicast */
	fman_if_reset_mcast_filter_table(fman_intf);
	/* Reset interface statistics */
	fman_if_stats_reset(fman_intf);
	/* Disable SG by default */
	fman_if_set_sg(fman_intf, 0);
	fman_if_set_maxfrm(fman_intf, ETHER_MAX_LEN + VLAN_TAG_SIZE);

	return 0;

free_tx:
	rte_free(dpaa_intf->tx_queues);
	dpaa_intf->tx_queues = NULL;
	dpaa_intf->nb_tx_queues = 0;

free_rx:
	rte_free(dpaa_intf->cgr_rx);
	rte_free(dpaa_intf->rx_queues);
	dpaa_intf->rx_queues = NULL;
	dpaa_intf->nb_rx_queues = 0;
	return ret;
}

static int
dpaa_dev_uninit(struct rte_eth_dev *dev)
{
	struct dpaa_if *dpaa_intf = dev->data->dev_private;
	int loop;

	PMD_INIT_FUNC_TRACE();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -EPERM;

	if (!dpaa_intf) {
		DPAA_PMD_WARN("Already closed or not started");
		return -1;
	}

	dpaa_eth_dev_close(dev);

	/* release configuration memory */
	if (dpaa_intf->fc_conf)
		rte_free(dpaa_intf->fc_conf);

	/* Release RX congestion Groups */
	if (dpaa_intf->cgr_rx) {
		for (loop = 0; loop < dpaa_intf->nb_rx_queues; loop++)
			qman_delete_cgr(&dpaa_intf->cgr_rx[loop]);

		qman_release_cgrid_range(dpaa_intf->cgr_rx[loop].cgrid,
					 dpaa_intf->nb_rx_queues);
	}

	rte_free(dpaa_intf->cgr_rx);
	dpaa_intf->cgr_rx = NULL;

	rte_free(dpaa_intf->rx_queues);
	dpaa_intf->rx_queues = NULL;

	rte_free(dpaa_intf->tx_queues);
	dpaa_intf->tx_queues = NULL;

	dev->dev_ops = NULL;
	dev->rx_pkt_burst = NULL;
	dev->tx_pkt_burst = NULL;

	return 0;
}

static int
rte_dpaa_probe(struct rte_dpaa_driver *dpaa_drv __rte_unused,
	       struct rte_dpaa_device *dpaa_dev)
{
	int diag;
	int ret;
	struct rte_eth_dev *eth_dev;

	PMD_INIT_FUNC_TRACE();

	/* In case of secondary process, the device is already configured
	 * and no further action is required, except portal initialization
	 * and verifying secondary attachment to port name.
	 */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		eth_dev = rte_eth_dev_attach_secondary(dpaa_dev->name);
		if (!eth_dev)
			return -ENOMEM;
		eth_dev->device = &dpaa_dev->device;
		eth_dev->dev_ops = &dpaa_devops;
		rte_eth_dev_probing_finish(eth_dev);
		return 0;
	}

	if (!is_global_init) {
		/* One time load of Qman/Bman drivers */
		ret = qman_global_init();
		if (ret) {
			DPAA_PMD_ERR("QMAN initialization failed: %d",
				     ret);
			return ret;
		}
		ret = bman_global_init();
		if (ret) {
			DPAA_PMD_ERR("BMAN initialization failed: %d",
				     ret);
			return ret;
		}

		if (access("/tmp/fmc.bin", F_OK) == -1) {
			RTE_LOG(INFO, PMD,
				"* FMC not configured.Enabling default mode\n");
			default_q = 1;
		}

		/* disabling the default push mode for LS1043 */
		if (dpaa_svr_family == SVR_LS1043A_FAMILY)
			dpaa_push_mode_max_queue = 0;

		/* if push mode queues to be enabled. Currenly we are allowing
		 * only one queue per thread.
		 */
		if (getenv("DPAA_PUSH_QUEUES_NUMBER")) {
			dpaa_push_mode_max_queue =
					atoi(getenv("DPAA_PUSH_QUEUES_NUMBER"));
			if (dpaa_push_mode_max_queue > DPAA_MAX_PUSH_MODE_QUEUE)
			    dpaa_push_mode_max_queue = DPAA_MAX_PUSH_MODE_QUEUE;
		}

		is_global_init = 1;
	}

	if (unlikely(!RTE_PER_LCORE(dpaa_io))) {
		ret = rte_dpaa_portal_init((void *)1);
		if (ret) {
			DPAA_PMD_ERR("Unable to initialize portal");
			return ret;
		}
	}

	eth_dev = rte_eth_dev_allocate(dpaa_dev->name);
	if (eth_dev == NULL)
		return -ENOMEM;

	eth_dev->data->dev_private = rte_zmalloc(
					"ethdev private structure",
					sizeof(struct dpaa_if),
					RTE_CACHE_LINE_SIZE);
	if (!eth_dev->data->dev_private) {
		DPAA_PMD_ERR("Cannot allocate memzone for port data");
		rte_eth_dev_release_port(eth_dev);
		return -ENOMEM;
	}

	eth_dev->device = &dpaa_dev->device;
	dpaa_dev->eth_dev = eth_dev;

	/* Invoke PMD device initialization function */
	diag = dpaa_dev_init(eth_dev);
	if (diag == 0) {
		rte_eth_dev_probing_finish(eth_dev);
		return 0;
	}

	rte_eth_dev_release_port(eth_dev);
	return diag;
}

static int
rte_dpaa_remove(struct rte_dpaa_device *dpaa_dev)
{
	struct rte_eth_dev *eth_dev;

	PMD_INIT_FUNC_TRACE();

	eth_dev = dpaa_dev->eth_dev;
	dpaa_dev_uninit(eth_dev);

	rte_eth_dev_release_port(eth_dev);

	return 0;
}

static struct rte_dpaa_driver rte_dpaa_pmd = {
	.drv_type = FSL_DPAA_ETH,
	.probe = rte_dpaa_probe,
	.remove = rte_dpaa_remove,
};

RTE_PMD_REGISTER_DPAA(net_dpaa, rte_dpaa_pmd);
