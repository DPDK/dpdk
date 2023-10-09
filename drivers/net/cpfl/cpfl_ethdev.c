/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#include <rte_atomic.h>
#include <rte_eal.h>
#include <rte_ether.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_dev.h>
#include <errno.h>
#include <rte_alarm.h>
#include <rte_hash_crc.h>

#include "cpfl_ethdev.h"
#include <ethdev_private.h>
#include "cpfl_rxtx.h"
#include "cpfl_flow.h"
#include "cpfl_rules.h"

#define CPFL_REPRESENTOR	"representor"
#define CPFL_TX_SINGLE_Q	"tx_single"
#define CPFL_RX_SINGLE_Q	"rx_single"
#define CPFL_VPORT		"vport"

#ifdef RTE_HAS_JANSSON
#define CPFL_FLOW_PARSER	"flow_parser"
#endif

rte_spinlock_t cpfl_adapter_lock;
/* A list for all adapters, one adapter matches one PCI device */
struct cpfl_adapter_list cpfl_adapter_list;
bool cpfl_adapter_list_init;

static const char * const cpfl_valid_args_first[] = {
	CPFL_REPRESENTOR,
	CPFL_TX_SINGLE_Q,
	CPFL_RX_SINGLE_Q,
	CPFL_VPORT,
#ifdef RTE_HAS_JANSSON
	CPFL_FLOW_PARSER,
#endif
	NULL
};

static const char * const cpfl_valid_args_again[] = {
	CPFL_REPRESENTOR,
	NULL
};

uint32_t cpfl_supported_speeds[] = {
	RTE_ETH_SPEED_NUM_NONE,
	RTE_ETH_SPEED_NUM_10M,
	RTE_ETH_SPEED_NUM_100M,
	RTE_ETH_SPEED_NUM_1G,
	RTE_ETH_SPEED_NUM_2_5G,
	RTE_ETH_SPEED_NUM_5G,
	RTE_ETH_SPEED_NUM_10G,
	RTE_ETH_SPEED_NUM_20G,
	RTE_ETH_SPEED_NUM_25G,
	RTE_ETH_SPEED_NUM_40G,
	RTE_ETH_SPEED_NUM_50G,
	RTE_ETH_SPEED_NUM_56G,
	RTE_ETH_SPEED_NUM_100G,
	RTE_ETH_SPEED_NUM_200G
};

static const uint64_t cpfl_map_hena_rss[] = {
	[IDPF_HASH_NONF_UNICAST_IPV4_UDP] =
			RTE_ETH_RSS_NONFRAG_IPV4_UDP,
	[IDPF_HASH_NONF_MULTICAST_IPV4_UDP] =
			RTE_ETH_RSS_NONFRAG_IPV4_UDP,
	[IDPF_HASH_NONF_IPV4_UDP] =
			RTE_ETH_RSS_NONFRAG_IPV4_UDP,
	[IDPF_HASH_NONF_IPV4_TCP_SYN_NO_ACK] =
			RTE_ETH_RSS_NONFRAG_IPV4_TCP,
	[IDPF_HASH_NONF_IPV4_TCP] =
			RTE_ETH_RSS_NONFRAG_IPV4_TCP,
	[IDPF_HASH_NONF_IPV4_SCTP] =
			RTE_ETH_RSS_NONFRAG_IPV4_SCTP,
	[IDPF_HASH_NONF_IPV4_OTHER] =
			RTE_ETH_RSS_NONFRAG_IPV4_OTHER,
	[IDPF_HASH_FRAG_IPV4] = RTE_ETH_RSS_FRAG_IPV4,

	/* IPv6 */
	[IDPF_HASH_NONF_UNICAST_IPV6_UDP] =
			RTE_ETH_RSS_NONFRAG_IPV6_UDP,
	[IDPF_HASH_NONF_MULTICAST_IPV6_UDP] =
			RTE_ETH_RSS_NONFRAG_IPV6_UDP,
	[IDPF_HASH_NONF_IPV6_UDP] =
			RTE_ETH_RSS_NONFRAG_IPV6_UDP,
	[IDPF_HASH_NONF_IPV6_TCP_SYN_NO_ACK] =
			RTE_ETH_RSS_NONFRAG_IPV6_TCP,
	[IDPF_HASH_NONF_IPV6_TCP] =
			RTE_ETH_RSS_NONFRAG_IPV6_TCP,
	[IDPF_HASH_NONF_IPV6_SCTP] =
			RTE_ETH_RSS_NONFRAG_IPV6_SCTP,
	[IDPF_HASH_NONF_IPV6_OTHER] =
			RTE_ETH_RSS_NONFRAG_IPV6_OTHER,
	[IDPF_HASH_FRAG_IPV6] = RTE_ETH_RSS_FRAG_IPV6,

	/* L2 Payload */
	[IDPF_HASH_L2_PAYLOAD] = RTE_ETH_RSS_L2_PAYLOAD
};

static const uint64_t cpfl_ipv4_rss = RTE_ETH_RSS_NONFRAG_IPV4_UDP |
			  RTE_ETH_RSS_NONFRAG_IPV4_TCP |
			  RTE_ETH_RSS_NONFRAG_IPV4_SCTP |
			  RTE_ETH_RSS_NONFRAG_IPV4_OTHER |
			  RTE_ETH_RSS_FRAG_IPV4;

static const uint64_t cpfl_ipv6_rss = RTE_ETH_RSS_NONFRAG_IPV6_UDP |
			  RTE_ETH_RSS_NONFRAG_IPV6_TCP |
			  RTE_ETH_RSS_NONFRAG_IPV6_SCTP |
			  RTE_ETH_RSS_NONFRAG_IPV6_OTHER |
			  RTE_ETH_RSS_FRAG_IPV6;

struct rte_cpfl_xstats_name_off {
	char name[RTE_ETH_XSTATS_NAME_SIZE];
	unsigned int offset;
};

static const struct rte_cpfl_xstats_name_off rte_cpfl_stats_strings[] = {
	{"rx_bytes", offsetof(struct virtchnl2_vport_stats, rx_bytes)},
	{"rx_unicast_packets", offsetof(struct virtchnl2_vport_stats, rx_unicast)},
	{"rx_multicast_packets", offsetof(struct virtchnl2_vport_stats, rx_multicast)},
	{"rx_broadcast_packets", offsetof(struct virtchnl2_vport_stats, rx_broadcast)},
	{"rx_dropped_packets", offsetof(struct virtchnl2_vport_stats, rx_discards)},
	{"rx_errors", offsetof(struct virtchnl2_vport_stats, rx_errors)},
	{"rx_unknown_protocol_packets", offsetof(struct virtchnl2_vport_stats,
						 rx_unknown_protocol)},
	{"tx_bytes", offsetof(struct virtchnl2_vport_stats, tx_bytes)},
	{"tx_unicast_packets", offsetof(struct virtchnl2_vport_stats, tx_unicast)},
	{"tx_multicast_packets", offsetof(struct virtchnl2_vport_stats, tx_multicast)},
	{"tx_broadcast_packets", offsetof(struct virtchnl2_vport_stats, tx_broadcast)},
	{"tx_dropped_packets", offsetof(struct virtchnl2_vport_stats, tx_discards)},
	{"tx_error_packets", offsetof(struct virtchnl2_vport_stats, tx_errors)}};

#define CPFL_NB_XSTATS			RTE_DIM(rte_cpfl_stats_strings)

static int
cpfl_dev_link_update(struct rte_eth_dev *dev,
		     __rte_unused int wait_to_complete)
{
	struct cpfl_vport *cpfl_vport = dev->data->dev_private;
	struct idpf_vport *vport = &cpfl_vport->base;
	struct rte_eth_link new_link;
	unsigned int i;

	memset(&new_link, 0, sizeof(new_link));

	for (i = 0; i < RTE_DIM(cpfl_supported_speeds); i++) {
		if (vport->link_speed == cpfl_supported_speeds[i]) {
			new_link.link_speed = vport->link_speed;
			break;
		}
	}

	if (i == RTE_DIM(cpfl_supported_speeds)) {
		if (vport->link_up)
			new_link.link_speed = RTE_ETH_SPEED_NUM_UNKNOWN;
		else
			new_link.link_speed = RTE_ETH_SPEED_NUM_NONE;
	}

	new_link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
	new_link.link_status = vport->link_up ? RTE_ETH_LINK_UP :
		RTE_ETH_LINK_DOWN;
	new_link.link_autoneg = (dev->data->dev_conf.link_speeds & RTE_ETH_LINK_SPEED_FIXED) ?
				 RTE_ETH_LINK_FIXED : RTE_ETH_LINK_AUTONEG;

	return rte_eth_linkstatus_set(dev, &new_link);
}

static int
cpfl_hairpin_cap_get(struct rte_eth_dev *dev,
		     struct rte_eth_hairpin_cap *cap)
{
	struct cpfl_vport *cpfl_vport = dev->data->dev_private;

	if (cpfl_vport->p2p_q_chunks_info == NULL)
		return -ENOTSUP;

	cap->max_nb_queues = CPFL_MAX_P2P_NB_QUEUES;
	cap->max_rx_2_tx = CPFL_MAX_HAIRPINQ_RX_2_TX;
	cap->max_tx_2_rx = CPFL_MAX_HAIRPINQ_TX_2_RX;
	cap->max_nb_desc = CPFL_MAX_HAIRPINQ_NB_DESC;

	return 0;
}

static int
cpfl_dev_info_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	struct cpfl_vport *cpfl_vport = dev->data->dev_private;
	struct idpf_vport *vport = &cpfl_vport->base;
	struct idpf_adapter *base = vport->adapter;

	dev_info->max_rx_queues = base->caps.max_rx_q;
	dev_info->max_tx_queues = base->caps.max_tx_q;
	dev_info->min_rx_bufsize = CPFL_MIN_BUF_SIZE;
	dev_info->max_rx_pktlen = vport->max_mtu + CPFL_ETH_OVERHEAD;

	dev_info->max_mtu = vport->max_mtu;
	dev_info->min_mtu = RTE_ETHER_MIN_MTU;

	dev_info->hash_key_size = vport->rss_key_size;
	dev_info->reta_size = vport->rss_lut_size;

	dev_info->flow_type_rss_offloads = CPFL_RSS_OFFLOAD_ALL;

	dev_info->rx_offload_capa =
		RTE_ETH_RX_OFFLOAD_IPV4_CKSUM           |
		RTE_ETH_RX_OFFLOAD_UDP_CKSUM            |
		RTE_ETH_RX_OFFLOAD_TCP_CKSUM            |
		RTE_ETH_RX_OFFLOAD_OUTER_IPV4_CKSUM     |
		RTE_ETH_RX_OFFLOAD_TIMESTAMP		|
		RTE_ETH_RX_OFFLOAD_SCATTER;

	dev_info->tx_offload_capa =
		RTE_ETH_TX_OFFLOAD_IPV4_CKSUM		|
		RTE_ETH_TX_OFFLOAD_UDP_CKSUM		|
		RTE_ETH_TX_OFFLOAD_TCP_CKSUM		|
		RTE_ETH_TX_OFFLOAD_SCTP_CKSUM		|
		RTE_ETH_TX_OFFLOAD_TCP_TSO		|
		RTE_ETH_TX_OFFLOAD_MULTI_SEGS		|
		RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;

	dev_info->default_txconf = (struct rte_eth_txconf) {
		.tx_free_thresh = CPFL_DEFAULT_TX_FREE_THRESH,
		.tx_rs_thresh = CPFL_DEFAULT_TX_RS_THRESH,
	};

	dev_info->default_rxconf = (struct rte_eth_rxconf) {
		.rx_free_thresh = CPFL_DEFAULT_RX_FREE_THRESH,
	};

	dev_info->tx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = CPFL_MAX_RING_DESC,
		.nb_min = CPFL_MIN_RING_DESC,
		.nb_align = CPFL_ALIGN_RING_DESC,
	};

	dev_info->rx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = CPFL_MAX_RING_DESC,
		.nb_min = CPFL_MIN_RING_DESC,
		.nb_align = CPFL_ALIGN_RING_DESC,
	};

	return 0;
}

static int
cpfl_dev_mtu_set(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct cpfl_vport *cpfl_vport = dev->data->dev_private;
	struct idpf_vport *vport = &cpfl_vport->base;

	/* mtu setting is forbidden if port is start */
	if (dev->data->dev_started) {
		PMD_DRV_LOG(ERR, "port must be stopped before configuration");
		return -EBUSY;
	}

	if (mtu > vport->max_mtu) {
		PMD_DRV_LOG(ERR, "MTU should be less than %d", vport->max_mtu);
		return -EINVAL;
	}

	vport->max_pkt_len = mtu + CPFL_ETH_OVERHEAD;

	return 0;
}

static const uint32_t *
cpfl_dev_supported_ptypes_get(struct rte_eth_dev *dev __rte_unused)
{
	static const uint32_t ptypes[] = {
		RTE_PTYPE_L2_ETHER,
		RTE_PTYPE_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_L4_FRAG,
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_SCTP,
		RTE_PTYPE_L4_ICMP,
		RTE_PTYPE_UNKNOWN
	};

	return ptypes;
}

static uint64_t
cpfl_get_mbuf_alloc_failed_stats(struct rte_eth_dev *dev)
{
	uint64_t mbuf_alloc_failed = 0;
	struct cpfl_rx_queue *cpfl_rxq;
	int i = 0;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		cpfl_rxq = dev->data->rx_queues[i];
		mbuf_alloc_failed += __atomic_load_n(&cpfl_rxq->base.rx_stats.mbuf_alloc_failed,
						     __ATOMIC_RELAXED);
	}

	return mbuf_alloc_failed;
}

static int
cpfl_dev_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	struct cpfl_vport *cpfl_vport = dev->data->dev_private;
	struct idpf_vport *vport = &cpfl_vport->base;
	struct virtchnl2_vport_stats *pstats = NULL;
	int ret;

	ret = idpf_vc_stats_query(vport, &pstats);
	if (ret == 0) {
		uint8_t crc_stats_len = (dev->data->dev_conf.rxmode.offloads &
					 RTE_ETH_RX_OFFLOAD_KEEP_CRC) ? 0 :
					 RTE_ETHER_CRC_LEN;

		idpf_vport_stats_update(&vport->eth_stats_offset, pstats);
		stats->ipackets = pstats->rx_unicast + pstats->rx_multicast +
				pstats->rx_broadcast - pstats->rx_discards;
		stats->opackets = pstats->tx_broadcast + pstats->tx_multicast +
						pstats->tx_unicast;
		stats->imissed = pstats->rx_discards;
		stats->ierrors = pstats->rx_errors;
		stats->oerrors = pstats->tx_errors + pstats->tx_discards;
		stats->ibytes = pstats->rx_bytes;
		stats->ibytes -= stats->ipackets * crc_stats_len;
		stats->obytes = pstats->tx_bytes;

		dev->data->rx_mbuf_alloc_failed = cpfl_get_mbuf_alloc_failed_stats(dev);
		stats->rx_nombuf = dev->data->rx_mbuf_alloc_failed;
	} else {
		PMD_DRV_LOG(ERR, "Get statistics failed");
	}
	return ret;
}

static void
cpfl_reset_mbuf_alloc_failed_stats(struct rte_eth_dev *dev)
{
	struct cpfl_rx_queue *cpfl_rxq;
	int i;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		cpfl_rxq = dev->data->rx_queues[i];
		__atomic_store_n(&cpfl_rxq->base.rx_stats.mbuf_alloc_failed, 0, __ATOMIC_RELAXED);
	}
}

static int
cpfl_dev_stats_reset(struct rte_eth_dev *dev)
{
	struct cpfl_vport *cpfl_vport = dev->data->dev_private;
	struct idpf_vport *vport = &cpfl_vport->base;
	struct virtchnl2_vport_stats *pstats = NULL;
	int ret;

	ret = idpf_vc_stats_query(vport, &pstats);
	if (ret != 0)
		return ret;

	/* set stats offset base on current values */
	vport->eth_stats_offset = *pstats;

	cpfl_reset_mbuf_alloc_failed_stats(dev);

	return 0;
}

static int cpfl_dev_xstats_reset(struct rte_eth_dev *dev)
{
	cpfl_dev_stats_reset(dev);
	return 0;
}

static int cpfl_dev_xstats_get(struct rte_eth_dev *dev,
			       struct rte_eth_xstat *xstats, unsigned int n)
{
	struct cpfl_vport *cpfl_vport = dev->data->dev_private;
	struct idpf_vport *vport = &cpfl_vport->base;
	struct virtchnl2_vport_stats *pstats = NULL;
	unsigned int i;
	int ret;

	if (n < CPFL_NB_XSTATS)
		return CPFL_NB_XSTATS;

	if (!xstats)
		return CPFL_NB_XSTATS;

	ret = idpf_vc_stats_query(vport, &pstats);
	if (ret) {
		PMD_DRV_LOG(ERR, "Get statistics failed");
		return 0;
	}

	idpf_vport_stats_update(&vport->eth_stats_offset, pstats);

	/* loop over xstats array and values from pstats */
	for (i = 0; i < CPFL_NB_XSTATS; i++) {
		xstats[i].id = i;
		xstats[i].value = *(uint64_t *)(((char *)pstats) +
			rte_cpfl_stats_strings[i].offset);
	}
	return CPFL_NB_XSTATS;
}

static int cpfl_dev_xstats_get_names(__rte_unused struct rte_eth_dev *dev,
				     struct rte_eth_xstat_name *xstats_names,
				     __rte_unused unsigned int limit)
{
	unsigned int i;

	if (xstats_names) {
		for (i = 0; i < CPFL_NB_XSTATS; i++) {
			snprintf(xstats_names[i].name,
				 sizeof(xstats_names[i].name),
				 "%s", rte_cpfl_stats_strings[i].name);
		}
	}
	return CPFL_NB_XSTATS;
}

static int cpfl_config_rss_hf(struct idpf_vport *vport, uint64_t rss_hf)
{
	uint64_t hena = 0;
	uint16_t i;

	/**
	 * RTE_ETH_RSS_IPV4 and RTE_ETH_RSS_IPV6 can be considered as 2
	 * generalizations of all other IPv4 and IPv6 RSS types.
	 */
	if (rss_hf & RTE_ETH_RSS_IPV4)
		rss_hf |= cpfl_ipv4_rss;

	if (rss_hf & RTE_ETH_RSS_IPV6)
		rss_hf |= cpfl_ipv6_rss;

	for (i = 0; i < RTE_DIM(cpfl_map_hena_rss); i++) {
		if (cpfl_map_hena_rss[i] & rss_hf)
			hena |= BIT_ULL(i);
	}

	/**
	 * At present, cp doesn't process the virtual channel msg of rss_hf configuration,
	 * tips are given below.
	 */
	if (hena != vport->rss_hf)
		PMD_DRV_LOG(WARNING, "Updating RSS Hash Function is not supported at present.");

	return 0;
}

static int
cpfl_init_rss(struct idpf_vport *vport)
{
	struct rte_eth_rss_conf *rss_conf;
	struct rte_eth_dev_data *dev_data;
	uint16_t i, nb_q;
	int ret = 0;

	dev_data = vport->dev_data;
	rss_conf = &dev_data->dev_conf.rx_adv_conf.rss_conf;
	nb_q = dev_data->nb_rx_queues;

	if (rss_conf->rss_key == NULL) {
		for (i = 0; i < vport->rss_key_size; i++)
			vport->rss_key[i] = (uint8_t)rte_rand();
	} else if (rss_conf->rss_key_len != vport->rss_key_size) {
		PMD_INIT_LOG(ERR, "Invalid RSS key length in RSS configuration, should be %d",
			     vport->rss_key_size);
		return -EINVAL;
	} else {
		memcpy(vport->rss_key, rss_conf->rss_key,
			   vport->rss_key_size);
	}

	for (i = 0; i < vport->rss_lut_size; i++)
		vport->rss_lut[i] = i % nb_q;

	vport->rss_hf = IDPF_DEFAULT_RSS_HASH_EXPANDED;

	ret = idpf_vport_rss_config(vport);
	if (ret != 0)
		PMD_INIT_LOG(ERR, "Failed to configure RSS");

	return ret;
}

static int
cpfl_rss_reta_update(struct rte_eth_dev *dev,
		     struct rte_eth_rss_reta_entry64 *reta_conf,
		     uint16_t reta_size)
{
	struct cpfl_vport *cpfl_vport = dev->data->dev_private;
	struct idpf_vport *vport = &cpfl_vport->base;
	struct idpf_adapter *base = vport->adapter;
	uint16_t idx, shift;
	int ret = 0;
	uint16_t i;

	if (base->caps.rss_caps == 0 || dev->data->nb_rx_queues == 0) {
		PMD_DRV_LOG(DEBUG, "RSS is not supported");
		return -ENOTSUP;
	}

	if (reta_size != vport->rss_lut_size) {
		PMD_DRV_LOG(ERR, "The size of hash lookup table configured "
				 "(%d) doesn't match the number of hardware can "
				 "support (%d)",
			    reta_size, vport->rss_lut_size);
		return -EINVAL;
	}

	for (i = 0; i < reta_size; i++) {
		idx = i / RTE_ETH_RETA_GROUP_SIZE;
		shift = i % RTE_ETH_RETA_GROUP_SIZE;
		if (reta_conf[idx].mask & (1ULL << shift))
			vport->rss_lut[i] = reta_conf[idx].reta[shift];
	}

	/* send virtchnl ops to configure RSS */
	ret = idpf_vc_rss_lut_set(vport);
	if (ret)
		PMD_INIT_LOG(ERR, "Failed to configure RSS lut");

	return ret;
}

static int
cpfl_rss_reta_query(struct rte_eth_dev *dev,
		    struct rte_eth_rss_reta_entry64 *reta_conf,
		    uint16_t reta_size)
{
	struct cpfl_vport *cpfl_vport = dev->data->dev_private;
	struct idpf_vport *vport = &cpfl_vport->base;
	struct idpf_adapter *base = vport->adapter;
	uint16_t idx, shift;
	int ret = 0;
	uint16_t i;

	if (base->caps.rss_caps == 0 || dev->data->nb_rx_queues == 0) {
		PMD_DRV_LOG(DEBUG, "RSS is not supported");
		return -ENOTSUP;
	}

	if (reta_size != vport->rss_lut_size) {
		PMD_DRV_LOG(ERR, "The size of hash lookup table configured "
			"(%d) doesn't match the number of hardware can "
			"support (%d)", reta_size, vport->rss_lut_size);
		return -EINVAL;
	}

	ret = idpf_vc_rss_lut_get(vport);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to get RSS LUT");
		return ret;
	}

	for (i = 0; i < reta_size; i++) {
		idx = i / RTE_ETH_RETA_GROUP_SIZE;
		shift = i % RTE_ETH_RETA_GROUP_SIZE;
		if (reta_conf[idx].mask & (1ULL << shift))
			reta_conf[idx].reta[shift] = vport->rss_lut[i];
	}

	return 0;
}

static int
cpfl_rss_hash_update(struct rte_eth_dev *dev,
		     struct rte_eth_rss_conf *rss_conf)
{
	struct cpfl_vport *cpfl_vport = dev->data->dev_private;
	struct idpf_vport *vport = &cpfl_vport->base;
	struct idpf_adapter *base = vport->adapter;
	int ret = 0;

	if (base->caps.rss_caps == 0 || dev->data->nb_rx_queues == 0) {
		PMD_DRV_LOG(DEBUG, "RSS is not supported");
		return -ENOTSUP;
	}

	if (!rss_conf->rss_key || rss_conf->rss_key_len == 0) {
		PMD_DRV_LOG(DEBUG, "No key to be configured");
		goto skip_rss_key;
	} else if (rss_conf->rss_key_len != vport->rss_key_size) {
		PMD_DRV_LOG(ERR, "The size of hash key configured "
				 "(%d) doesn't match the size of hardware can "
				 "support (%d)",
			    rss_conf->rss_key_len,
			    vport->rss_key_size);
		return -EINVAL;
	}

	memcpy(vport->rss_key, rss_conf->rss_key,
		   vport->rss_key_size);
	ret = idpf_vc_rss_key_set(vport);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Failed to configure RSS key");
		return ret;
	}

skip_rss_key:
	ret = cpfl_config_rss_hf(vport, rss_conf->rss_hf);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Failed to configure RSS hash");
		return ret;
	}

	return 0;
}

static uint64_t
cpfl_map_general_rss_hf(uint64_t config_rss_hf, uint64_t last_general_rss_hf)
{
	uint64_t valid_rss_hf = 0;
	uint16_t i;

	for (i = 0; i < RTE_DIM(cpfl_map_hena_rss); i++) {
		uint64_t bit = BIT_ULL(i);

		if (bit & config_rss_hf)
			valid_rss_hf |= cpfl_map_hena_rss[i];
	}

	if (valid_rss_hf & cpfl_ipv4_rss)
		valid_rss_hf |= last_general_rss_hf & RTE_ETH_RSS_IPV4;

	if (valid_rss_hf & cpfl_ipv6_rss)
		valid_rss_hf |= last_general_rss_hf & RTE_ETH_RSS_IPV6;

	return valid_rss_hf;
}

static int
cpfl_rss_hash_conf_get(struct rte_eth_dev *dev,
		       struct rte_eth_rss_conf *rss_conf)
{
	struct cpfl_vport *cpfl_vport = dev->data->dev_private;
	struct idpf_vport *vport = &cpfl_vport->base;
	struct idpf_adapter *base = vport->adapter;
	int ret = 0;

	if (base->caps.rss_caps == 0 || dev->data->nb_rx_queues == 0) {
		PMD_DRV_LOG(DEBUG, "RSS is not supported");
		return -ENOTSUP;
	}

	ret = idpf_vc_rss_hash_get(vport);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to get RSS hf");
		return ret;
	}

	rss_conf->rss_hf = cpfl_map_general_rss_hf(vport->rss_hf, vport->last_general_rss_hf);

	if (!rss_conf->rss_key)
		return 0;

	ret = idpf_vc_rss_key_get(vport);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to get RSS key");
		return ret;
	}

	if (rss_conf->rss_key_len > vport->rss_key_size)
		rss_conf->rss_key_len = vport->rss_key_size;

	memcpy(rss_conf->rss_key, vport->rss_key, rss_conf->rss_key_len);

	return 0;
}

static int
cpfl_dev_configure(struct rte_eth_dev *dev)
{
	struct cpfl_vport *cpfl_vport = dev->data->dev_private;
	struct idpf_vport *vport = &cpfl_vport->base;
	struct rte_eth_conf *conf = &dev->data->dev_conf;
	struct idpf_adapter *base = vport->adapter;
	int ret;

	if (conf->link_speeds & RTE_ETH_LINK_SPEED_FIXED) {
		PMD_INIT_LOG(ERR, "Setting link speed is not supported");
		return -ENOTSUP;
	}

	if (conf->txmode.mq_mode != RTE_ETH_MQ_TX_NONE) {
		PMD_INIT_LOG(ERR, "Multi-queue TX mode %d is not supported",
			     conf->txmode.mq_mode);
		return -ENOTSUP;
	}

	if (conf->lpbk_mode != 0) {
		PMD_INIT_LOG(ERR, "Loopback operation mode %d is not supported",
			     conf->lpbk_mode);
		return -ENOTSUP;
	}

	if (conf->dcb_capability_en != 0) {
		PMD_INIT_LOG(ERR, "Priority Flow Control(PFC) if not supported");
		return -ENOTSUP;
	}

	if (conf->intr_conf.lsc != 0) {
		PMD_INIT_LOG(ERR, "LSC interrupt is not supported");
		return -ENOTSUP;
	}

	if (conf->intr_conf.rxq != 0) {
		PMD_INIT_LOG(ERR, "RXQ interrupt is not supported");
		return -ENOTSUP;
	}

	if (conf->intr_conf.rmv != 0) {
		PMD_INIT_LOG(ERR, "RMV interrupt is not supported");
		return -ENOTSUP;
	}

	if (conf->rxmode.mq_mode != RTE_ETH_MQ_RX_RSS &&
	    conf->rxmode.mq_mode != RTE_ETH_MQ_RX_NONE) {
		PMD_INIT_LOG(ERR, "RX mode %d is not supported.",
			     conf->rxmode.mq_mode);
		return -EINVAL;
	}

	if (base->caps.rss_caps != 0 && dev->data->nb_rx_queues != 0) {
		ret = cpfl_init_rss(vport);
		if (ret != 0) {
			PMD_INIT_LOG(ERR, "Failed to init rss");
			return ret;
		}
	} else if (conf->rxmode.mq_mode == RTE_ETH_MQ_RX_RSS) {
		PMD_INIT_LOG(ERR, "RSS is not supported.");
		return -ENOTSUP;
	}

	vport->max_pkt_len =
		(dev->data->mtu == 0) ? CPFL_DEFAULT_MTU : dev->data->mtu +
		CPFL_ETH_OVERHEAD;

	return 0;
}

static int
cpfl_config_rx_queues_irqs(struct rte_eth_dev *dev)
{
	uint32_t qids[CPFL_MAX_P2P_NB_QUEUES + IDPF_DEFAULT_RXQ_NUM] = {0};
	struct cpfl_vport *cpfl_vport = dev->data->dev_private;
	struct idpf_vport *vport = &cpfl_vport->base;
	uint16_t nb_rx_queues = dev->data->nb_rx_queues;
	struct cpfl_rx_queue *cpfl_rxq;
	int i;

	for (i = 0; i < nb_rx_queues; i++) {
		cpfl_rxq = dev->data->rx_queues[i];
		if (cpfl_rxq->hairpin_info.hairpin_q)
			qids[i] = cpfl_hw_qid_get(cpfl_vport->p2p_q_chunks_info->rx_start_qid,
						  (i - cpfl_vport->nb_data_rxq));
		else
			qids[i] = cpfl_hw_qid_get(vport->chunks_info.rx_start_qid, i);
	}
	return idpf_vport_irq_map_config_by_qids(vport, qids, nb_rx_queues);
}

/* Update hairpin_info for dev's tx hairpin queue */
static int
cpfl_txq_hairpin_info_update(struct rte_eth_dev *dev, uint16_t rx_port)
{
	struct cpfl_vport *cpfl_tx_vport = dev->data->dev_private;
	struct rte_eth_dev *peer_dev = &rte_eth_devices[rx_port];
	struct cpfl_vport *cpfl_rx_vport = peer_dev->data->dev_private;
	struct cpfl_txq_hairpin_info *hairpin_info;
	struct cpfl_tx_queue *cpfl_txq;
	int i;

	for (i = cpfl_tx_vport->nb_data_txq; i < dev->data->nb_tx_queues; i++) {
		cpfl_txq = dev->data->tx_queues[i];
		hairpin_info = &cpfl_txq->hairpin_info;
		if (hairpin_info->peer_rxp != rx_port) {
			PMD_DRV_LOG(ERR, "port %d is not the peer port", rx_port);
			return -EINVAL;
		}
		hairpin_info->peer_rxq_id =
			cpfl_hw_qid_get(cpfl_rx_vport->p2p_q_chunks_info->rx_start_qid,
					hairpin_info->peer_rxq_id - cpfl_rx_vport->nb_data_rxq);
	}

	return 0;
}

/* Bind Rx hairpin queue's memory zone to peer Tx hairpin queue's memory zone */
static void
cpfl_rxq_hairpin_mz_bind(struct rte_eth_dev *dev)
{
	struct cpfl_vport *cpfl_rx_vport = dev->data->dev_private;
	struct idpf_vport *vport = &cpfl_rx_vport->base;
	struct idpf_adapter *adapter = vport->adapter;
	struct idpf_hw *hw = &adapter->hw;
	struct cpfl_rx_queue *cpfl_rxq;
	struct cpfl_tx_queue *cpfl_txq;
	struct rte_eth_dev *peer_dev;
	const struct rte_memzone *mz;
	uint16_t peer_tx_port;
	uint16_t peer_tx_qid;
	int i;

	for (i = cpfl_rx_vport->nb_data_rxq; i < dev->data->nb_rx_queues; i++) {
		cpfl_rxq = dev->data->rx_queues[i];
		peer_tx_port = cpfl_rxq->hairpin_info.peer_txp;
		peer_tx_qid = cpfl_rxq->hairpin_info.peer_txq_id;
		peer_dev = &rte_eth_devices[peer_tx_port];
		cpfl_txq = peer_dev->data->tx_queues[peer_tx_qid];

		/* bind rx queue */
		mz = cpfl_txq->base.mz;
		cpfl_rxq->base.rx_ring_phys_addr = mz->iova;
		cpfl_rxq->base.rx_ring = mz->addr;
		cpfl_rxq->base.mz = mz;

		/* bind rx buffer queue */
		mz = cpfl_txq->base.complq->mz;
		cpfl_rxq->base.bufq1->rx_ring_phys_addr = mz->iova;
		cpfl_rxq->base.bufq1->rx_ring = mz->addr;
		cpfl_rxq->base.bufq1->mz = mz;
		cpfl_rxq->base.bufq1->qrx_tail = hw->hw_addr +
			cpfl_hw_qtail_get(cpfl_rx_vport->p2p_q_chunks_info->rx_buf_qtail_start,
					0, cpfl_rx_vport->p2p_q_chunks_info->rx_buf_qtail_spacing);
	}
}

static int
cpfl_rss_lut_config(struct cpfl_vport *cpfl_vport, uint16_t nb_q)
{
	struct idpf_vport *vport = &cpfl_vport->base;
	uint16_t lut_size = vport->rss_lut_size;
	uint16_t i;
	int ret;

	for (i = 0; i < lut_size; i++)
		vport->rss_lut[i] = i % nb_q;

	ret = idpf_vc_rss_lut_set(vport);
	if (ret)
		PMD_INIT_LOG(ERR, "Failed to configure RSS lut");

	return ret;
}

static int
cpfl_start_queues(struct rte_eth_dev *dev)
{
	struct cpfl_vport *cpfl_vport = dev->data->dev_private;
	struct idpf_vport *vport = &cpfl_vport->base;
	struct cpfl_rx_queue *cpfl_rxq;
	struct cpfl_tx_queue *cpfl_txq;
	int update_flag = 0;
	int err = 0;
	int i;

	/* For normal data queues, configure, init and enale Txq.
	 * For non-manual bind hairpin queues, configure Txq.
	 */
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		cpfl_txq = dev->data->tx_queues[i];
		if (cpfl_txq == NULL || cpfl_txq->base.tx_deferred_start)
			continue;
		if (!cpfl_txq->hairpin_info.hairpin_q) {
			err = cpfl_tx_queue_start(dev, i);
			if (err != 0) {
				PMD_DRV_LOG(ERR, "Fail to start Tx queue %u", i);
				return err;
			}
		} else if (!cpfl_vport->p2p_manual_bind) {
			if (update_flag == 0) {
				err = cpfl_txq_hairpin_info_update(dev,
								   cpfl_txq->hairpin_info.peer_rxp);
				if (err != 0) {
					PMD_DRV_LOG(ERR, "Fail to update Tx hairpin queue info");
					return err;
				}
				update_flag = 1;
			}
			err = cpfl_hairpin_txq_config(vport, cpfl_txq);
			if (err != 0) {
				PMD_DRV_LOG(ERR, "Fail to configure hairpin Tx queue %u", i);
				return err;
			}
		}
	}

	/* For non-manual bind hairpin queues, configure Tx completion queue first.*/
	if (!cpfl_vport->p2p_manual_bind && cpfl_vport->p2p_tx_complq != NULL) {
		err = cpfl_hairpin_tx_complq_config(cpfl_vport);
		if (err != 0) {
			PMD_DRV_LOG(ERR, "Fail to config Tx completion queue");
			return err;
		}
	}

	/* For non-manual bind hairpin queues, configure Rx buffer queue.*/
	if (!cpfl_vport->p2p_manual_bind && cpfl_vport->p2p_rx_bufq != NULL) {
		cpfl_rxq_hairpin_mz_bind(dev);
		err = cpfl_hairpin_rx_bufq_config(cpfl_vport);
		if (err != 0) {
			PMD_DRV_LOG(ERR, "Fail to config Rx buffer queue");
			return err;
		}
	}

	/* For normal data queues, configure, init and enale Rxq.
	 * For non-manual bind hairpin queues, configure Rxq, and then init Rxq.
	 */
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		cpfl_rxq = dev->data->rx_queues[i];
		if (cpfl_rxq == NULL || cpfl_rxq->base.rx_deferred_start)
			continue;
		if (!cpfl_rxq->hairpin_info.hairpin_q) {
			err = cpfl_rx_queue_start(dev, i);
			if (err != 0) {
				PMD_DRV_LOG(ERR, "Fail to start Rx queue %u", i);
				return err;
			}
		} else if (!cpfl_vport->p2p_manual_bind) {
			err = cpfl_hairpin_rxq_config(vport, cpfl_rxq);
			if (err != 0) {
				PMD_DRV_LOG(ERR, "Fail to configure hairpin Rx queue %u", i);
				return err;
			}
			err = cpfl_rx_queue_init(dev, i);
			if (err != 0) {
				PMD_DRV_LOG(ERR, "Fail to init hairpin Rx queue %u", i);
				return err;
			}
		}
	}

	/* For non-manual bind hairpin queues, enable Tx queue and Rx queue,
	 * then enable Tx completion queue and Rx buffer queue.
	 */
	for (i = cpfl_vport->nb_data_txq; i < dev->data->nb_tx_queues; i++) {
		cpfl_txq = dev->data->tx_queues[i];
		if (cpfl_txq->hairpin_info.hairpin_q && !cpfl_vport->p2p_manual_bind) {
			err = cpfl_switch_hairpin_rxtx_queue(cpfl_vport,
							     i - cpfl_vport->nb_data_txq,
							     false, true);
			if (err)
				PMD_DRV_LOG(ERR, "Failed to switch hairpin TX queue %u on",
					    i);
			else
				cpfl_txq->base.q_started = true;
		}
	}

	for (i = cpfl_vport->nb_data_rxq; i < dev->data->nb_rx_queues; i++) {
		cpfl_rxq = dev->data->rx_queues[i];
		if (cpfl_rxq->hairpin_info.hairpin_q && !cpfl_vport->p2p_manual_bind) {
			err = cpfl_switch_hairpin_rxtx_queue(cpfl_vport,
							     i - cpfl_vport->nb_data_rxq,
							     true, true);
			if (err)
				PMD_DRV_LOG(ERR, "Failed to switch hairpin RX queue %u on",
					    i);
			else
				cpfl_rxq->base.q_started = true;
		}
	}

	if (!cpfl_vport->p2p_manual_bind &&
	    cpfl_vport->p2p_tx_complq != NULL &&
	    cpfl_vport->p2p_rx_bufq != NULL) {
		err = cpfl_switch_hairpin_complq(cpfl_vport, true);
		if (err != 0) {
			PMD_DRV_LOG(ERR, "Failed to switch hairpin Tx complq");
			return err;
		}
		err = cpfl_switch_hairpin_bufq(cpfl_vport, true);
		if (err != 0) {
			PMD_DRV_LOG(ERR, "Failed to switch hairpin Rx bufq");
			return err;
		}
	}

	/* re-configure RSS lut if there's hairpin queue */
	if (cpfl_vport->nb_p2p_rxq > 0)
		err = cpfl_rss_lut_config(cpfl_vport, cpfl_vport->nb_data_rxq);

	return err;
}

static int
cpfl_dev_start(struct rte_eth_dev *dev)
{
	struct cpfl_vport *cpfl_vport = dev->data->dev_private;
	struct idpf_vport *vport = &cpfl_vport->base;
	struct idpf_adapter *base = vport->adapter;
	struct cpfl_adapter_ext *adapter = CPFL_ADAPTER_TO_EXT(base);
	uint16_t num_allocated_vectors = base->caps.num_allocated_vectors;
	uint16_t req_vecs_num;
	int ret;

	req_vecs_num = CPFL_DFLT_Q_VEC_NUM;
	if (req_vecs_num + adapter->used_vecs_num > num_allocated_vectors) {
		PMD_DRV_LOG(ERR, "The accumulated request vectors' number should be less than %d",
			    num_allocated_vectors);
		ret = -EINVAL;
		goto err_vec;
	}

	ret = idpf_vc_vectors_alloc(vport, req_vecs_num);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Failed to allocate interrupt vectors");
		goto err_vec;
	}
	adapter->used_vecs_num += req_vecs_num;

	ret = cpfl_config_rx_queues_irqs(dev);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Failed to configure irqs");
		goto err_irq;
	}

	ret = cpfl_start_queues(dev);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Failed to start queues");
		goto err_startq;
	}

	cpfl_set_rx_function(dev);
	cpfl_set_tx_function(dev);

	ret = idpf_vc_vport_ena_dis(vport, true);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Failed to enable vport");
		goto err_vport;
	}

	if (cpfl_dev_stats_reset(dev))
		PMD_DRV_LOG(ERR, "Failed to reset stats");

	return 0;

err_vport:
	cpfl_stop_queues(dev);
err_startq:
	idpf_vport_irq_unmap_config(vport, dev->data->nb_rx_queues);
err_irq:
	idpf_vc_vectors_dealloc(vport);
err_vec:
	return ret;
}

static int
cpfl_dev_stop(struct rte_eth_dev *dev)
{
	struct cpfl_vport *cpfl_vport = dev->data->dev_private;
	struct idpf_vport *vport = &cpfl_vport->base;

	if (dev->data->dev_started == 0)
		return 0;

	idpf_vc_vport_ena_dis(vport, false);

	cpfl_stop_queues(dev);

	idpf_vport_irq_unmap_config(vport, dev->data->nb_rx_queues);

	idpf_vc_vectors_dealloc(vport);

	return 0;
}

static void
cpfl_flow_free(struct cpfl_vport *vport)
{
	struct rte_flow *p_flow;

	while ((p_flow = TAILQ_FIRST(&vport->itf.flow_list))) {
		TAILQ_REMOVE(&vport->itf.flow_list, p_flow, next);
		if (p_flow->engine->free)
			p_flow->engine->free(p_flow);
		rte_free(p_flow);
	}
}

static int
cpfl_p2p_queue_grps_del(struct idpf_vport *vport)
{
	struct virtchnl2_queue_group_id qg_ids;
	int ret = 0;

	memset(&qg_ids, 0, sizeof(qg_ids));
	qg_ids.queue_group_id = CPFL_P2P_QUEUE_GRP_ID;
	qg_ids.queue_group_type = VIRTCHNL2_QUEUE_GROUP_P2P;
	ret = idpf_vc_queue_grps_del(vport, CPFL_P2P_NB_QUEUE_GRPS, &qg_ids);
	if (ret)
		PMD_DRV_LOG(ERR, "Failed to delete p2p queue groups");
	return ret;
}

static int
cpfl_dev_close(struct rte_eth_dev *dev)
{
	struct cpfl_vport *cpfl_vport = dev->data->dev_private;
	struct idpf_vport *vport = &cpfl_vport->base;
	struct cpfl_adapter_ext *adapter = CPFL_ADAPTER_TO_EXT(vport->adapter);

	cpfl_dev_stop(dev);
	if (cpfl_vport->p2p_mp) {
		rte_mempool_free(cpfl_vport->p2p_mp);
		cpfl_vport->p2p_mp = NULL;
	}

	if (!adapter->base.is_rx_singleq && !adapter->base.is_tx_singleq)
		cpfl_p2p_queue_grps_del(vport);

	cpfl_flow_free(cpfl_vport);
	idpf_vport_deinit(vport);
	rte_free(cpfl_vport->p2p_q_chunks_info);

	adapter->cur_vports &= ~RTE_BIT32(vport->devarg_id);
	adapter->cur_vport_nb--;
	dev->data->dev_private = NULL;
	adapter->vports[vport->sw_idx] = NULL;
	idpf_free_dma_mem(NULL, &cpfl_vport->itf.flow_dma);
	rte_free(cpfl_vport);

	return 0;
}

static int
cpfl_dev_flow_ops_get(struct rte_eth_dev *dev,
		      const struct rte_flow_ops **ops)
{
	struct cpfl_itf *itf;

	if (!dev)
		return -EINVAL;

	itf = CPFL_DEV_TO_ITF(dev);

	/* only vport support rte_flow */
	if (itf->type != CPFL_ITF_TYPE_VPORT)
		return -ENOTSUP;
#ifdef RTE_HAS_JANSSON
	*ops = &cpfl_flow_ops;
#else
	*ops = NULL;
	PMD_DRV_LOG(NOTICE, "not support rte_flow, please install json-c library.");
#endif
	return 0;
}

static int
cpfl_hairpin_get_peer_ports(struct rte_eth_dev *dev, uint16_t *peer_ports,
			    size_t len, uint32_t tx)
{
	struct cpfl_vport *cpfl_vport =
		(struct cpfl_vport *)dev->data->dev_private;
	struct idpf_tx_queue *txq;
	struct idpf_rx_queue *rxq;
	struct cpfl_tx_queue *cpfl_txq;
	struct cpfl_rx_queue *cpfl_rxq;
	uint16_t i;
	uint16_t j = 0;

	if (len <= 0)
		return -EINVAL;

	if (cpfl_vport->p2p_q_chunks_info == NULL)
		return -ENOTSUP;

	if (tx > 0) {
		for (i = cpfl_vport->nb_data_txq, j = 0; i < dev->data->nb_tx_queues; i++, j++) {
			txq = dev->data->tx_queues[i];
			if (txq == NULL || j >= len)
				return -EINVAL;
			cpfl_txq = (struct cpfl_tx_queue *)txq;
			peer_ports[j] = cpfl_txq->hairpin_info.peer_rxp;
		}
	} else if (tx == 0) {
		for (i = cpfl_vport->nb_data_rxq, j = 0; i < dev->data->nb_rx_queues; i++, j++) {
			rxq = dev->data->rx_queues[i];
			if (rxq == NULL || j >= len)
				return -EINVAL;
			cpfl_rxq = (struct cpfl_rx_queue *)rxq;
			peer_ports[j] = cpfl_rxq->hairpin_info.peer_txp;
		}
	}

	return j;
}

static int
cpfl_hairpin_bind(struct rte_eth_dev *dev, uint16_t rx_port)
{
	struct cpfl_vport *cpfl_tx_vport = dev->data->dev_private;
	struct idpf_vport *tx_vport = &cpfl_tx_vport->base;
	struct cpfl_vport *cpfl_rx_vport;
	struct cpfl_tx_queue *cpfl_txq;
	struct cpfl_rx_queue *cpfl_rxq;
	struct rte_eth_dev *peer_dev;
	struct idpf_vport *rx_vport;
	int err = 0;
	int i;

	err = cpfl_txq_hairpin_info_update(dev, rx_port);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Fail to update Tx hairpin queue info.");
		return err;
	}

	/* configure hairpin queues */
	for (i = cpfl_tx_vport->nb_data_txq; i < dev->data->nb_tx_queues; i++) {
		cpfl_txq = dev->data->tx_queues[i];
		err = cpfl_hairpin_txq_config(tx_vport, cpfl_txq);
		if (err != 0) {
			PMD_DRV_LOG(ERR, "Fail to configure hairpin Tx queue %u", i);
			return err;
		}
	}

	err = cpfl_hairpin_tx_complq_config(cpfl_tx_vport);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Fail to config Tx completion queue");
		return err;
	}

	peer_dev = &rte_eth_devices[rx_port];
	cpfl_rx_vport = (struct cpfl_vport *)peer_dev->data->dev_private;
	rx_vport = &cpfl_rx_vport->base;
	cpfl_rxq_hairpin_mz_bind(peer_dev);

	err = cpfl_hairpin_rx_bufq_config(cpfl_rx_vport);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Fail to config Rx buffer queue");
		return err;
	}

	for (i = cpfl_rx_vport->nb_data_rxq; i < peer_dev->data->nb_rx_queues; i++) {
		cpfl_rxq = peer_dev->data->rx_queues[i];
		err = cpfl_hairpin_rxq_config(rx_vport, cpfl_rxq);
		if (err != 0) {
			PMD_DRV_LOG(ERR, "Fail to configure hairpin Rx queue %u", i);
			return err;
		}
		err = cpfl_rx_queue_init(peer_dev, i);
		if (err != 0) {
			PMD_DRV_LOG(ERR, "Fail to init hairpin Rx queue %u", i);
			return err;
		}
	}

	/* enable hairpin queues */
	for (i = cpfl_tx_vport->nb_data_txq; i < dev->data->nb_tx_queues; i++) {
		cpfl_txq = dev->data->tx_queues[i];
		err = cpfl_switch_hairpin_rxtx_queue(cpfl_tx_vport,
						     i - cpfl_tx_vport->nb_data_txq,
						     false, true);
		if (err != 0) {
			PMD_DRV_LOG(ERR, "Failed to switch hairpin TX queue %u on",
				    i);
			return err;
		}
		cpfl_txq->base.q_started = true;
	}

	err = cpfl_switch_hairpin_complq(cpfl_tx_vport, true);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Failed to switch hairpin Tx complq");
		return err;
	}

	for (i = cpfl_rx_vport->nb_data_rxq; i < peer_dev->data->nb_rx_queues; i++) {
		cpfl_rxq = peer_dev->data->rx_queues[i];
		err = cpfl_switch_hairpin_rxtx_queue(cpfl_rx_vport,
						     i - cpfl_rx_vport->nb_data_rxq,
						     true, true);
		if (err != 0) {
			PMD_DRV_LOG(ERR, "Failed to switch hairpin RX queue %u on",
				    i);
		}
		cpfl_rxq->base.q_started = true;
	}

	err = cpfl_switch_hairpin_bufq(cpfl_rx_vport, true);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Failed to switch hairpin Rx buffer queue");
		return err;
	}

	return 0;
}

static int
cpfl_hairpin_unbind(struct rte_eth_dev *dev, uint16_t rx_port)
{
	struct cpfl_vport *cpfl_tx_vport = dev->data->dev_private;
	struct rte_eth_dev *peer_dev = &rte_eth_devices[rx_port];
	struct cpfl_vport *cpfl_rx_vport = peer_dev->data->dev_private;
	struct cpfl_tx_queue *cpfl_txq;
	struct cpfl_rx_queue *cpfl_rxq;
	int i;

	/* disable hairpin queues */
	for (i = cpfl_tx_vport->nb_data_txq; i < dev->data->nb_tx_queues; i++) {
		cpfl_txq = dev->data->tx_queues[i];
		cpfl_switch_hairpin_rxtx_queue(cpfl_tx_vport,
					       i - cpfl_tx_vport->nb_data_txq,
					       false, false);
		cpfl_txq->base.q_started = false;
	}

	cpfl_switch_hairpin_complq(cpfl_tx_vport, false);

	for (i = cpfl_rx_vport->nb_data_rxq; i < peer_dev->data->nb_rx_queues; i++) {
		cpfl_rxq = peer_dev->data->rx_queues[i];
		cpfl_switch_hairpin_rxtx_queue(cpfl_rx_vport,
					       i - cpfl_rx_vport->nb_data_rxq,
					       true, false);
		cpfl_rxq->base.q_started = false;
	}

	cpfl_switch_hairpin_bufq(cpfl_rx_vport, false);

	return 0;
}

static const struct eth_dev_ops cpfl_eth_dev_ops = {
	.dev_configure			= cpfl_dev_configure,
	.dev_close			= cpfl_dev_close,
	.rx_queue_setup			= cpfl_rx_queue_setup,
	.tx_queue_setup			= cpfl_tx_queue_setup,
	.dev_infos_get			= cpfl_dev_info_get,
	.dev_start			= cpfl_dev_start,
	.dev_stop			= cpfl_dev_stop,
	.link_update			= cpfl_dev_link_update,
	.rx_queue_start			= cpfl_rx_queue_start,
	.tx_queue_start			= cpfl_tx_queue_start,
	.rx_queue_stop			= cpfl_rx_queue_stop,
	.tx_queue_stop			= cpfl_tx_queue_stop,
	.rx_queue_release		= cpfl_dev_rx_queue_release,
	.tx_queue_release		= cpfl_dev_tx_queue_release,
	.mtu_set			= cpfl_dev_mtu_set,
	.dev_supported_ptypes_get	= cpfl_dev_supported_ptypes_get,
	.stats_get			= cpfl_dev_stats_get,
	.stats_reset			= cpfl_dev_stats_reset,
	.reta_update			= cpfl_rss_reta_update,
	.reta_query			= cpfl_rss_reta_query,
	.rss_hash_update		= cpfl_rss_hash_update,
	.rss_hash_conf_get		= cpfl_rss_hash_conf_get,
	.xstats_get			= cpfl_dev_xstats_get,
	.xstats_get_names		= cpfl_dev_xstats_get_names,
	.xstats_reset			= cpfl_dev_xstats_reset,
	.flow_ops_get			= cpfl_dev_flow_ops_get,
	.hairpin_cap_get		= cpfl_hairpin_cap_get,
	.rx_hairpin_queue_setup		= cpfl_rx_hairpin_queue_setup,
	.tx_hairpin_queue_setup		= cpfl_tx_hairpin_queue_setup,
	.hairpin_get_peer_ports         = cpfl_hairpin_get_peer_ports,
	.hairpin_bind                   = cpfl_hairpin_bind,
	.hairpin_unbind                 = cpfl_hairpin_unbind,
};

static int
insert_value(struct cpfl_devargs *devargs, uint16_t id)
{
	uint16_t i;

	/* ignore duplicate */
	for (i = 0; i < devargs->req_vport_nb; i++) {
		if (devargs->req_vports[i] == id)
			return 0;
	}

	devargs->req_vports[devargs->req_vport_nb] = id;
	devargs->req_vport_nb++;

	return 0;
}

static const char *
parse_range(const char *value, struct cpfl_devargs *devargs)
{
	uint16_t lo, hi, i;
	int n = 0;
	int result;
	const char *pos = value;

	result = sscanf(value, "%hu%n-%hu%n", &lo, &n, &hi, &n);
	if (result == 1) {
		if (insert_value(devargs, lo) != 0)
			return NULL;
	} else if (result == 2) {
		if (lo > hi)
			return NULL;
		for (i = lo; i <= hi; i++) {
			if (insert_value(devargs, i) != 0)
				return NULL;
		}
	} else {
		return NULL;
	}

	return pos + n;
}

static int
parse_vport(const char *key, const char *value, void *args)
{
	struct cpfl_devargs *devargs = args;
	const char *pos = value;

	devargs->req_vport_nb = 0;

	if (*pos == '[')
		pos++;

	while (1) {
		pos = parse_range(pos, devargs);
		if (pos == NULL) {
			PMD_INIT_LOG(ERR, "invalid value:\"%s\" for key:\"%s\", ",
				     value, key);
			return -EINVAL;
		}
		if (*pos != ',')
			break;
		pos++;
	}

	if (*value == '[' && *pos != ']') {
		PMD_INIT_LOG(ERR, "invalid value:\"%s\" for key:\"%s\", ",
			     value, key);
		return -EINVAL;
	}

	return 0;
}

static int
parse_bool(const char *key, const char *value, void *args)
{
	int *i = args;
	char *end;
	int num;

	errno = 0;

	num = strtoul(value, &end, 10);

	if (errno == ERANGE || (num != 0 && num != 1)) {
		PMD_INIT_LOG(ERR, "invalid value:\"%s\" for key:\"%s\", value must be 0 or 1",
			value, key);
		return -EINVAL;
	}

	*i = num;
	return 0;
}

static int
enlist(uint16_t *list, uint16_t *len_list, const uint16_t max_list, uint16_t val)
{
	uint16_t i;

	for (i = 0; i < *len_list; i++) {
		if (list[i] == val)
			return 0;
	}
	if (*len_list >= max_list)
		return -1;
	list[(*len_list)++] = val;
	return 0;
}

static const char *
process_range(const char *str, uint16_t *list, uint16_t *len_list,
	const uint16_t max_list)
{
	uint16_t lo, hi, val;
	int result, n = 0;
	const char *pos = str;

	result = sscanf(str, "%hu%n-%hu%n", &lo, &n, &hi, &n);
	if (result == 1) {
		if (enlist(list, len_list, max_list, lo) != 0)
			return NULL;
	} else if (result == 2) {
		if (lo > hi)
			return NULL;
		for (val = lo; val <= hi; val++) {
			if (enlist(list, len_list, max_list, val) != 0)
				return NULL;
		}
	} else {
		return NULL;
	}
	return pos + n;
}

static const char *
process_list(const char *str, uint16_t *list, uint16_t *len_list, const uint16_t max_list)
{
	const char *pos = str;

	if (*pos == '[')
		pos++;
	while (1) {
		pos = process_range(pos, list, len_list, max_list);
		if (pos == NULL)
			return NULL;
		if (*pos != ',') /* end of list */
			break;
		pos++;
	}
	if (*str == '[' && *pos != ']')
		return NULL;
	if (*pos == ']')
		pos++;
	return pos;
}

static int
parse_repr(const char *key __rte_unused, const char *value, void *args)
{
	struct cpfl_devargs *devargs = args;
	struct rte_eth_devargs *eth_da;
	const char *str = value;

	if (devargs->repr_args_num == CPFL_REPR_ARG_NUM_MAX)
		return -EINVAL;

	eth_da = &devargs->repr_args[devargs->repr_args_num];

	if (str[0] == 'c') {
		str += 1;
		str = process_list(str, eth_da->mh_controllers,
				&eth_da->nb_mh_controllers,
				RTE_DIM(eth_da->mh_controllers));
		if (str == NULL)
			goto done;
	}
	if (str[0] == 'p' && str[1] == 'f') {
		eth_da->type = RTE_ETH_REPRESENTOR_PF;
		str += 2;
		str = process_list(str, eth_da->ports,
				&eth_da->nb_ports, RTE_DIM(eth_da->ports));
		if (str == NULL || str[0] == '\0')
			goto done;
	} else if (eth_da->nb_mh_controllers > 0) {
		/* 'c' must followed by 'pf'. */
		str = NULL;
		goto done;
	}
	if (str[0] == 'v' && str[1] == 'f') {
		eth_da->type = RTE_ETH_REPRESENTOR_VF;
		str += 2;
	} else if (str[0] == 's' && str[1] == 'f') {
		eth_da->type = RTE_ETH_REPRESENTOR_SF;
		str += 2;
	} else {
		/* 'pf' must followed by 'vf' or 'sf'. */
		if (eth_da->type == RTE_ETH_REPRESENTOR_PF) {
			str = NULL;
			goto done;
		}
		eth_da->type = RTE_ETH_REPRESENTOR_VF;
	}
	str = process_list(str, eth_da->representor_ports,
		&eth_da->nb_representor_ports,
		RTE_DIM(eth_da->representor_ports));
done:
	if (str == NULL) {
		RTE_LOG(ERR, EAL, "wrong representor format: %s\n", str);
		return -1;
	}

	devargs->repr_args_num++;

	return 0;
}

#ifdef RTE_HAS_JANSSON
static int
parse_file(const char *key, const char *value, void *args)
{
	char *name = args;

	if (strlen(value) > CPFL_FLOW_FILE_LEN - 1) {
		PMD_DRV_LOG(ERR, "file path(%s) is too long.", value);
		return -1;
	}

	PMD_DRV_LOG(DEBUG, "value:\"%s\" for key:\"%s\"", value, key);
	strlcpy(name, value, CPFL_FLOW_FILE_LEN);

	return 0;
}
#endif

static int
cpfl_parse_devargs(struct rte_pci_device *pci_dev, struct cpfl_adapter_ext *adapter, bool first)
{
	struct rte_devargs *devargs = pci_dev->device.devargs;
	struct cpfl_devargs *cpfl_args = &adapter->devargs;
	struct rte_kvargs *kvlist;
	int ret;

	if (devargs == NULL)
		return 0;

	if (first)
		memset(cpfl_args, 0, sizeof(struct cpfl_devargs));

	kvlist = rte_kvargs_parse(devargs->args,
			first ? cpfl_valid_args_first : cpfl_valid_args_again);
	if (kvlist == NULL) {
		PMD_INIT_LOG(ERR, "invalid kvargs key");
		return -EINVAL;
	}

	if (rte_kvargs_count(kvlist, CPFL_VPORT) > 1) {
		PMD_INIT_LOG(ERR, "devarg vport is duplicated.");
		return -EINVAL;
	}

	ret = rte_kvargs_process(kvlist, CPFL_REPRESENTOR, &parse_repr, cpfl_args);

	if (ret != 0)
		goto fail;

	if (!first)
		return 0;

	ret = rte_kvargs_process(kvlist, CPFL_VPORT, &parse_vport,
				 cpfl_args);
	if (ret != 0)
		goto fail;

	ret = rte_kvargs_process(kvlist, CPFL_TX_SINGLE_Q, &parse_bool,
				 &adapter->base.is_tx_singleq);
	if (ret != 0)
		goto fail;

	ret = rte_kvargs_process(kvlist, CPFL_RX_SINGLE_Q, &parse_bool,
				 &adapter->base.is_rx_singleq);
	if (ret != 0)
		goto fail;
#ifdef RTE_HAS_JANSSON
	if (rte_kvargs_get(kvlist, CPFL_FLOW_PARSER)) {
		ret = rte_kvargs_process(kvlist, CPFL_FLOW_PARSER,
					 &parse_file, cpfl_args->flow_parser);
		if (ret) {
			PMD_DRV_LOG(ERR, "Failed to parser flow_parser, ret: %d", ret);
			goto fail;
		}
	} else {
		cpfl_args->flow_parser[0] = '\0';
	}
#endif
fail:
	rte_kvargs_free(kvlist);
	return ret;
}

static struct cpfl_vport *
cpfl_find_vport(struct cpfl_adapter_ext *adapter, uint32_t vport_id)
{
	struct cpfl_vport *vport = NULL;
	int i;

	for (i = 0; i < adapter->cur_vport_nb; i++) {
		vport = adapter->vports[i];
		if (vport == NULL)
			continue;
		if (vport->base.vport_id != vport_id)
			continue;
		else
			return vport;
	}

	return NULL;
}

static void
cpfl_handle_vchnl_event_msg(struct cpfl_adapter_ext *adapter, uint8_t *msg, uint16_t msglen)
{
	struct virtchnl2_event *vc_event = (struct virtchnl2_event *)msg;
	struct cpfl_vport *vport;
	struct rte_eth_dev_data *data;
	struct rte_eth_dev *dev;

	if (msglen < sizeof(struct virtchnl2_event)) {
		PMD_DRV_LOG(ERR, "Error event");
		return;
	}

	/* ignore if it is ctrl vport */
	if (adapter->ctrl_vport.base.vport_id == vc_event->vport_id)
		return;

	vport = cpfl_find_vport(adapter, vc_event->vport_id);
	if (!vport) {
		PMD_DRV_LOG(ERR, "Can't find vport.");
		return;
	}

	data = vport->itf.data;
	dev = &rte_eth_devices[data->port_id];

	switch (vc_event->event) {
	case VIRTCHNL2_EVENT_LINK_CHANGE:
		PMD_DRV_LOG(DEBUG, "VIRTCHNL2_EVENT_LINK_CHANGE");
		vport->base.link_up = !!(vc_event->link_status);
		vport->base.link_speed = vc_event->link_speed;
		cpfl_dev_link_update(dev, 0);
		break;
	default:
		PMD_DRV_LOG(ERR, " unknown event received %u", vc_event->event);
		break;
	}
}

int
cpfl_vport_info_create(struct cpfl_adapter_ext *adapter,
		       struct cpfl_vport_id *vport_identity,
		       struct cpchnl2_event_vport_created *vport_created)
{
	struct cpfl_vport_info *info = NULL;
	int ret;

	rte_spinlock_lock(&adapter->vport_map_lock);
	ret = rte_hash_lookup_data(adapter->vport_map_hash, vport_identity, (void **)&info);
	if (ret >= 0) {
		PMD_DRV_LOG(WARNING, "vport already exist, overwrite info anyway");
		/* overwrite info */
		if (info)
			info->vport = *vport_created;
		goto fini;
	}

	info = rte_zmalloc(NULL, sizeof(*info), 0);
	if (info == NULL) {
		PMD_DRV_LOG(ERR, "Failed to alloc memory for vport map info");
		ret = -ENOMEM;
		goto err;
	}

	info->vport = *vport_created;

	ret = rte_hash_add_key_data(adapter->vport_map_hash, vport_identity, info);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to add vport map into hash");
		rte_free(info);
		goto err;
	}

fini:
	rte_spinlock_unlock(&adapter->vport_map_lock);
	return 0;
err:
	rte_spinlock_unlock(&adapter->vport_map_lock);
	return ret;
}

static int
cpfl_vport_info_destroy(struct cpfl_adapter_ext *adapter, struct cpfl_vport_id *vport_identity)
{
	struct cpfl_vport_info *info;
	int ret;

	rte_spinlock_lock(&adapter->vport_map_lock);
	ret = rte_hash_lookup_data(adapter->vport_map_hash, vport_identity, (void **)&info);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "vport id doesn't exist");
		goto err;
	}

	rte_hash_del_key(adapter->vport_map_hash, vport_identity);
	rte_spinlock_unlock(&adapter->vport_map_lock);
	rte_free(info);

	return 0;

err:
	rte_spinlock_unlock(&adapter->vport_map_lock);
	return ret;
}

static void
cpfl_handle_cpchnl_event_msg(struct cpfl_adapter_ext *adapter, uint8_t *msg, uint16_t msglen)
{
	struct cpchnl2_event_info *cpchnl2_event = (struct cpchnl2_event_info *)msg;
	struct cpchnl2_event_vport_created *vport_created;
	struct cpfl_vport_id vport_identity = { 0 };

	if (msglen < sizeof(struct cpchnl2_event_info)) {
		PMD_DRV_LOG(ERR, "Error event");
		return;
	}

	switch (cpchnl2_event->header.type) {
	case CPCHNL2_EVENT_VPORT_CREATED:
		vport_identity.vport_id = cpchnl2_event->data.vport_created.vport.vport_id;
		vport_created = &cpchnl2_event->data.vport_created;
		vport_identity.func_type = vport_created->info.func_type;
		vport_identity.pf_id = vport_created->info.pf_id;
		vport_identity.vf_id = vport_created->info.vf_id;
		if (cpfl_vport_info_create(adapter, &vport_identity, vport_created))
			PMD_DRV_LOG(WARNING, "Failed to handle CPCHNL2_EVENT_VPORT_CREATED");
		break;
	case CPCHNL2_EVENT_VPORT_DESTROYED:
		vport_identity.vport_id = cpchnl2_event->data.vport_destroyed.vport.vport_id;
		vport_identity.func_type = cpchnl2_event->data.vport_destroyed.func.func_type;
		vport_identity.pf_id = cpchnl2_event->data.vport_destroyed.func.pf_id;
		vport_identity.vf_id = cpchnl2_event->data.vport_destroyed.func.vf_id;
		if (cpfl_vport_info_destroy(adapter, &vport_identity))
			PMD_DRV_LOG(WARNING, "Failed to handle CPCHNL2_EVENT_VPORT_DESTROY");
		break;
	default:
		PMD_DRV_LOG(ERR, " unknown event received %u", cpchnl2_event->header.type);
		break;
	}
}

static void
cpfl_handle_virtchnl_msg(struct cpfl_adapter_ext *adapter)
{
	struct idpf_adapter *base = &adapter->base;
	struct idpf_dma_mem *dma_mem = NULL;
	struct idpf_hw *hw = &base->hw;
	struct idpf_ctlq_msg ctlq_msg;
	enum idpf_mbx_opc mbx_op;
	uint16_t pending = 1;
	uint32_t vc_op;
	int ret;

	while (pending) {
		ret = idpf_vc_ctlq_recv(hw->arq, &pending, &ctlq_msg);
		if (ret) {
			PMD_DRV_LOG(INFO, "Failed to read msg from virtual channel, ret: %d", ret);
			return;
		}

		memcpy(base->mbx_resp, ctlq_msg.ctx.indirect.payload->va,
			   IDPF_DFLT_MBX_BUF_SIZE);

		mbx_op = rte_le_to_cpu_16(ctlq_msg.opcode);
		vc_op = rte_le_to_cpu_32(ctlq_msg.cookie.mbx.chnl_opcode);
		base->cmd_retval = rte_le_to_cpu_32(ctlq_msg.cookie.mbx.chnl_retval);

		switch (mbx_op) {
		case idpf_mbq_opc_send_msg_to_peer_pf:
			if (vc_op == VIRTCHNL2_OP_EVENT) {
				cpfl_handle_vchnl_event_msg(adapter, adapter->base.mbx_resp,
							    ctlq_msg.data_len);
			} else if (vc_op == CPCHNL2_OP_EVENT) {
				cpfl_handle_cpchnl_event_msg(adapter, adapter->base.mbx_resp,
							     ctlq_msg.data_len);
			} else {
				if (vc_op == base->pend_cmd)
					notify_cmd(base, base->cmd_retval);
				else
					PMD_DRV_LOG(ERR, "command mismatch, expect %u, get %u",
						    base->pend_cmd, vc_op);

				PMD_DRV_LOG(DEBUG, " Virtual channel response is received,"
					    "opcode = %d", vc_op);
			}
			goto post_buf;
		default:
			PMD_DRV_LOG(DEBUG, "Request %u is not supported yet", mbx_op);
		}
	}

post_buf:
	if (ctlq_msg.data_len)
		dma_mem = ctlq_msg.ctx.indirect.payload;
	else
		pending = 0;

	ret = idpf_vc_ctlq_post_rx_buffs(hw, hw->arq, &pending, &dma_mem);
	if (ret && dma_mem)
		idpf_free_dma_mem(hw, dma_mem);
}

static void
cpfl_dev_alarm_handler(void *param)
{
	struct cpfl_adapter_ext *adapter = param;

	cpfl_handle_virtchnl_msg(adapter);

	rte_eal_alarm_set(CPFL_ALARM_INTERVAL, cpfl_dev_alarm_handler, adapter);
}

static int
cpfl_stop_cfgqs(struct cpfl_adapter_ext *adapter)
{
	int i, ret;

	for (i = 0; i < CPFL_TX_CFGQ_NUM; i++) {
		ret = idpf_vc_queue_switch(&adapter->ctrl_vport.base, i, false, false);
		if (ret) {
			PMD_DRV_LOG(ERR, "Fail to disable Tx config queue.");
			return ret;
		}
	}

	for (i = 0; i < CPFL_RX_CFGQ_NUM; i++) {
		ret = idpf_vc_queue_switch(&adapter->ctrl_vport.base, i, true, false);
		if (ret) {
			PMD_DRV_LOG(ERR, "Fail to disable Rx config queue.");
			return ret;
		}
	}

	return 0;
}

static int
cpfl_start_cfgqs(struct cpfl_adapter_ext *adapter)
{
	int i, ret;

	ret = cpfl_config_ctlq_tx(adapter);
	if (ret) {
		PMD_DRV_LOG(ERR, "Fail to configure Tx config queue.");
		return ret;
	}

	ret = cpfl_config_ctlq_rx(adapter);
	if (ret) {
		PMD_DRV_LOG(ERR, "Fail to configure Rx config queue.");
		return ret;
	}

	for (i = 0; i < CPFL_TX_CFGQ_NUM; i++) {
		ret = idpf_vc_queue_switch(&adapter->ctrl_vport.base, i, false, true);
		if (ret) {
			PMD_DRV_LOG(ERR, "Fail to enable Tx config queue.");
			return ret;
		}
	}

	for (i = 0; i < CPFL_RX_CFGQ_NUM; i++) {
		ret = idpf_vc_queue_switch(&adapter->ctrl_vport.base, i, true, true);
		if (ret) {
			PMD_DRV_LOG(ERR, "Fail to enable Rx config queue.");
			return ret;
		}
	}

	return 0;
}

static void
cpfl_remove_cfgqs(struct cpfl_adapter_ext *adapter)
{
	struct idpf_hw *hw = (struct idpf_hw *)(&adapter->base.hw);
	struct cpfl_ctlq_create_info *create_cfgq_info;
	int i;

	create_cfgq_info = adapter->cfgq_info;

	for (i = 0; i < CPFL_CFGQ_NUM; i++) {
		if (adapter->ctlqp[i])
			cpfl_vport_ctlq_remove(hw, adapter->ctlqp[i]);
		if (create_cfgq_info[i].ring_mem.va)
			idpf_free_dma_mem(&adapter->base.hw, &create_cfgq_info[i].ring_mem);
		if (create_cfgq_info[i].buf_mem.va)
			idpf_free_dma_mem(&adapter->base.hw, &create_cfgq_info[i].buf_mem);
	}
}

static int
cpfl_add_cfgqs(struct cpfl_adapter_ext *adapter)
{
	struct idpf_ctlq_info *cfg_cq;
	int ret = 0;
	int i = 0;

	for (i = 0; i < CPFL_CFGQ_NUM; i++) {
		cfg_cq = NULL;
		ret = cpfl_vport_ctlq_add((struct idpf_hw *)(&adapter->base.hw),
					  &adapter->cfgq_info[i],
					  &cfg_cq);
		if (ret || !cfg_cq) {
			PMD_DRV_LOG(ERR, "ctlq add failed for queue id: %d",
				    adapter->cfgq_info[i].id);
			cpfl_remove_cfgqs(adapter);
			return ret;
		}
		PMD_DRV_LOG(INFO, "added cfgq to hw. queue id: %d",
			    adapter->cfgq_info[i].id);
		adapter->ctlqp[i] = cfg_cq;
	}

	return ret;
}

#define CPFL_CFGQ_RING_LEN		512
#define CPFL_CFGQ_DESCRIPTOR_SIZE	32
#define CPFL_CFGQ_BUFFER_SIZE		256
#define CPFL_CFGQ_RING_SIZE		512

static int
cpfl_cfgq_setup(struct cpfl_adapter_ext *adapter)
{
	struct cpfl_ctlq_create_info *create_cfgq_info;
	struct cpfl_vport *vport;
	int i, err;
	uint32_t ring_size = CPFL_CFGQ_RING_SIZE * sizeof(struct idpf_ctlq_desc);
	uint32_t buf_size = CPFL_CFGQ_RING_SIZE * CPFL_CFGQ_BUFFER_SIZE;

	vport = &adapter->ctrl_vport;
	create_cfgq_info = adapter->cfgq_info;

	for (i = 0; i < CPFL_CFGQ_NUM; i++) {
		if (i % 2 == 0) {
			/* Setup Tx config queue */
			create_cfgq_info[i].id = vport->base.chunks_info.tx_start_qid + i / 2;
			create_cfgq_info[i].type = IDPF_CTLQ_TYPE_CONFIG_TX;
			create_cfgq_info[i].len = CPFL_CFGQ_RING_SIZE;
			create_cfgq_info[i].buf_size = CPFL_CFGQ_BUFFER_SIZE;
			memset(&create_cfgq_info[i].reg, 0, sizeof(struct idpf_ctlq_reg));
			create_cfgq_info[i].reg.tail = vport->base.chunks_info.tx_qtail_start +
				i / 2 * vport->base.chunks_info.tx_qtail_spacing;
		} else {
			/* Setup Rx config queue */
			create_cfgq_info[i].id = vport->base.chunks_info.rx_start_qid + i / 2;
			create_cfgq_info[i].type = IDPF_CTLQ_TYPE_CONFIG_RX;
			create_cfgq_info[i].len = CPFL_CFGQ_RING_SIZE;
			create_cfgq_info[i].buf_size = CPFL_CFGQ_BUFFER_SIZE;
			memset(&create_cfgq_info[i].reg, 0, sizeof(struct idpf_ctlq_reg));
			create_cfgq_info[i].reg.tail = vport->base.chunks_info.rx_qtail_start +
				i / 2 * vport->base.chunks_info.rx_qtail_spacing;
			if (!idpf_alloc_dma_mem(&adapter->base.hw, &create_cfgq_info[i].buf_mem,
						buf_size)) {
				err = -ENOMEM;
				goto free_mem;
			}
		}
		if (!idpf_alloc_dma_mem(&adapter->base.hw, &create_cfgq_info[i].ring_mem,
					ring_size)) {
			err = -ENOMEM;
			goto free_mem;
		}
	}
	return 0;
free_mem:
	for (i = 0; i < CPFL_CFGQ_NUM; i++) {
		if (create_cfgq_info[i].ring_mem.va)
			idpf_free_dma_mem(&adapter->base.hw, &create_cfgq_info[i].ring_mem);
		if (create_cfgq_info[i].buf_mem.va)
			idpf_free_dma_mem(&adapter->base.hw, &create_cfgq_info[i].buf_mem);
	}
	return err;
}

static int
cpfl_init_ctrl_vport(struct cpfl_adapter_ext *adapter)
{
	struct cpfl_vport *vport = &adapter->ctrl_vport;
	struct virtchnl2_create_vport *vport_info =
		(struct virtchnl2_create_vport *)adapter->ctrl_vport_recv_info;
	int i;

	vport->itf.adapter = adapter;
	vport->base.adapter = &adapter->base;
	vport->base.vport_id = vport_info->vport_id;

	for (i = 0; i < vport_info->chunks.num_chunks; i++) {
		if (vport_info->chunks.chunks[i].type == VIRTCHNL2_QUEUE_TYPE_TX) {
			vport->base.chunks_info.tx_start_qid =
				vport_info->chunks.chunks[i].start_queue_id;
			vport->base.chunks_info.tx_qtail_start =
			vport_info->chunks.chunks[i].qtail_reg_start;
			vport->base.chunks_info.tx_qtail_spacing =
			vport_info->chunks.chunks[i].qtail_reg_spacing;
		} else if (vport_info->chunks.chunks[i].type == VIRTCHNL2_QUEUE_TYPE_RX) {
			vport->base.chunks_info.rx_start_qid =
				vport_info->chunks.chunks[i].start_queue_id;
			vport->base.chunks_info.rx_qtail_start =
			vport_info->chunks.chunks[i].qtail_reg_start;
			vport->base.chunks_info.rx_qtail_spacing =
			vport_info->chunks.chunks[i].qtail_reg_spacing;
		} else {
			PMD_INIT_LOG(ERR, "Unsupported chunk type");
			return -EINVAL;
		}
	}

	return 0;
}

static void
cpfl_ctrl_path_close(struct cpfl_adapter_ext *adapter)
{
	cpfl_stop_cfgqs(adapter);
	cpfl_remove_cfgqs(adapter);
	idpf_vc_vport_destroy(&adapter->ctrl_vport.base);
}

static int
cpfl_ctrl_path_open(struct cpfl_adapter_ext *adapter)
{
	int ret;

	ret = cpfl_vc_create_ctrl_vport(adapter);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to create control vport");
		return ret;
	}

	ret = cpfl_init_ctrl_vport(adapter);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to init control vport");
		goto err_init_ctrl_vport;
	}

	ret = cpfl_cfgq_setup(adapter);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to setup control queues");
		goto err_cfgq_setup;
	}

	ret = cpfl_add_cfgqs(adapter);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to add control queues");
		goto err_add_cfgq;
	}

	ret = cpfl_start_cfgqs(adapter);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to start control queues");
		goto err_start_cfgqs;
	}

	return 0;

err_start_cfgqs:
	cpfl_stop_cfgqs(adapter);
err_add_cfgq:
	cpfl_remove_cfgqs(adapter);
err_cfgq_setup:
err_init_ctrl_vport:
	idpf_vc_vport_destroy(&adapter->ctrl_vport.base);

	return ret;
}

static struct virtchnl2_get_capabilities req_caps = {
	.csum_caps =
	VIRTCHNL2_CAP_TX_CSUM_L3_IPV4          |
	VIRTCHNL2_CAP_TX_CSUM_L4_IPV4_TCP      |
	VIRTCHNL2_CAP_TX_CSUM_L4_IPV4_UDP      |
	VIRTCHNL2_CAP_TX_CSUM_L4_IPV4_SCTP     |
	VIRTCHNL2_CAP_TX_CSUM_L4_IPV6_TCP      |
	VIRTCHNL2_CAP_TX_CSUM_L4_IPV6_UDP      |
	VIRTCHNL2_CAP_TX_CSUM_L4_IPV6_SCTP     |
	VIRTCHNL2_CAP_TX_CSUM_GENERIC          |
	VIRTCHNL2_CAP_RX_CSUM_L3_IPV4          |
	VIRTCHNL2_CAP_RX_CSUM_L4_IPV4_TCP      |
	VIRTCHNL2_CAP_RX_CSUM_L4_IPV4_UDP      |
	VIRTCHNL2_CAP_RX_CSUM_L4_IPV4_SCTP     |
	VIRTCHNL2_CAP_RX_CSUM_L4_IPV6_TCP      |
	VIRTCHNL2_CAP_RX_CSUM_L4_IPV6_UDP      |
	VIRTCHNL2_CAP_RX_CSUM_L4_IPV6_SCTP     |
	VIRTCHNL2_CAP_RX_CSUM_GENERIC,

	.rss_caps =
	VIRTCHNL2_CAP_RSS_IPV4_TCP             |
	VIRTCHNL2_CAP_RSS_IPV4_UDP             |
	VIRTCHNL2_CAP_RSS_IPV4_SCTP            |
	VIRTCHNL2_CAP_RSS_IPV4_OTHER           |
	VIRTCHNL2_CAP_RSS_IPV6_TCP             |
	VIRTCHNL2_CAP_RSS_IPV6_UDP             |
	VIRTCHNL2_CAP_RSS_IPV6_SCTP            |
	VIRTCHNL2_CAP_RSS_IPV6_OTHER           |
	VIRTCHNL2_CAP_RSS_IPV4_AH              |
	VIRTCHNL2_CAP_RSS_IPV4_ESP             |
	VIRTCHNL2_CAP_RSS_IPV4_AH_ESP          |
	VIRTCHNL2_CAP_RSS_IPV6_AH              |
	VIRTCHNL2_CAP_RSS_IPV6_ESP             |
	VIRTCHNL2_CAP_RSS_IPV6_AH_ESP,

	.other_caps = VIRTCHNL2_CAP_WB_ON_ITR
};

static int
cpfl_vport_map_init(struct cpfl_adapter_ext *adapter)
{
	char hname[32];

	snprintf(hname, 32, "%s-vport", adapter->name);

	rte_spinlock_init(&adapter->vport_map_lock);

#define CPFL_VPORT_MAP_HASH_ENTRY_NUM 2048

	struct rte_hash_parameters params = {
		.name = adapter->name,
		.entries = CPFL_VPORT_MAP_HASH_ENTRY_NUM,
		.key_len = sizeof(struct cpfl_vport_id),
		.hash_func = rte_hash_crc,
		.socket_id = SOCKET_ID_ANY,
	};

	adapter->vport_map_hash = rte_hash_create(&params);

	if (adapter->vport_map_hash == NULL) {
		PMD_INIT_LOG(ERR, "Failed to create vport map hash");
		return -EINVAL;
	}

	return 0;
}

static void
cpfl_vport_map_uninit(struct cpfl_adapter_ext *adapter)
{
	const void *key = NULL;
	struct cpfl_vport_map_info *info;
	uint32_t iter = 0;

	while (rte_hash_iterate(adapter->vport_map_hash, &key, (void **)&info, &iter) >= 0)
		rte_free(info);

	rte_hash_free(adapter->vport_map_hash);
}

static int
cpfl_repr_allowlist_init(struct cpfl_adapter_ext *adapter)
{
	char hname[32];

	snprintf(hname, 32, "%s-repr_al", adapter->name);

	rte_spinlock_init(&adapter->repr_lock);

#define CPFL_REPR_HASH_ENTRY_NUM 2048

	struct rte_hash_parameters params = {
		.name = hname,
		.entries = CPFL_REPR_HASH_ENTRY_NUM,
		.key_len = sizeof(struct cpfl_repr_id),
		.hash_func = rte_hash_crc,
		.socket_id = SOCKET_ID_ANY,
	};

	adapter->repr_allowlist_hash = rte_hash_create(&params);

	if (adapter->repr_allowlist_hash == NULL) {
		PMD_INIT_LOG(ERR, "Failed to create repr allowlist hash");
		return -EINVAL;
	}

	return 0;
}

static void
cpfl_repr_allowlist_uninit(struct cpfl_adapter_ext *adapter)
{
	rte_hash_free(adapter->repr_allowlist_hash);
}


static int
cpfl_adapter_ext_init(struct rte_pci_device *pci_dev, struct cpfl_adapter_ext *adapter)
{
	struct idpf_adapter *base = &adapter->base;
	struct idpf_hw *hw = &base->hw;
	int ret = 0;

	hw->hw_addr = (void *)pci_dev->mem_resource[0].addr;
	hw->hw_addr_len = pci_dev->mem_resource[0].len;
	hw->back = base;
	hw->vendor_id = pci_dev->id.vendor_id;
	hw->device_id = pci_dev->id.device_id;
	hw->subsystem_vendor_id = pci_dev->id.subsystem_vendor_id;

	strncpy(adapter->name, pci_dev->device.name, PCI_PRI_STR_SIZE);

	rte_memcpy(&base->caps, &req_caps, sizeof(struct virtchnl2_get_capabilities));

	ret = idpf_adapter_init(base);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Failed to init adapter");
		goto err_adapter_init;
	}

	ret = cpfl_vport_map_init(adapter);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to init vport map");
		goto err_vport_map_init;
	}

	ret = cpfl_repr_allowlist_init(adapter);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to init representor allowlist");
		goto err_repr_allowlist_init;
	}

	rte_eal_alarm_set(CPFL_ALARM_INTERVAL, cpfl_dev_alarm_handler, adapter);

	adapter->max_vport_nb = adapter->base.caps.max_vports > CPFL_MAX_VPORT_NUM ?
				CPFL_MAX_VPORT_NUM : adapter->base.caps.max_vports;

	adapter->vports = rte_zmalloc("vports",
				      adapter->max_vport_nb *
				      sizeof(*adapter->vports),
				      0);
	if (adapter->vports == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate vports memory");
		ret = -ENOMEM;
		goto err_vports_alloc;
	}

	ret = cpfl_ctrl_path_open(adapter);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to setup control path");
		goto err_create_ctrl_vport;
	}

#ifdef RTE_HAS_JANSSON
	ret = cpfl_flow_init(adapter);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to init flow module");
		goto err_flow_init;
	}
#endif
	adapter->cur_vports = 0;
	adapter->cur_vport_nb = 0;

	adapter->used_vecs_num = 0;

	return ret;

#ifdef RTE_HAS_JANSSON
err_flow_init:
	cpfl_ctrl_path_close(adapter);
#endif
err_create_ctrl_vport:
	rte_free(adapter->vports);
err_vports_alloc:
	rte_eal_alarm_cancel(cpfl_dev_alarm_handler, adapter);
	cpfl_repr_allowlist_uninit(adapter);
err_repr_allowlist_init:
	cpfl_vport_map_uninit(adapter);
err_vport_map_init:
	idpf_adapter_deinit(base);
err_adapter_init:
	return ret;
}

static uint16_t
cpfl_vport_idx_alloc(struct cpfl_adapter_ext *adapter)
{
	uint16_t vport_idx;
	uint16_t i;

	for (i = 0; i < adapter->max_vport_nb; i++) {
		if (adapter->vports[i] == NULL)
			break;
	}

	if (i == adapter->max_vport_nb)
		vport_idx = CPFL_INVALID_VPORT_IDX;
	else
		vport_idx = i;

	return vport_idx;
}

static int
cpfl_p2p_q_grps_add(struct idpf_vport *vport,
		    struct virtchnl2_add_queue_groups *p2p_queue_grps_info,
		    uint8_t *p2p_q_vc_out_info)
{
	int ret;

	p2p_queue_grps_info->vport_id = vport->vport_id;
	p2p_queue_grps_info->qg_info.num_queue_groups = CPFL_P2P_NB_QUEUE_GRPS;
	p2p_queue_grps_info->qg_info.groups[0].num_rx_q = CPFL_MAX_P2P_NB_QUEUES;
	p2p_queue_grps_info->qg_info.groups[0].num_rx_bufq = CPFL_P2P_NB_RX_BUFQ;
	p2p_queue_grps_info->qg_info.groups[0].num_tx_q = CPFL_MAX_P2P_NB_QUEUES;
	p2p_queue_grps_info->qg_info.groups[0].num_tx_complq = CPFL_P2P_NB_TX_COMPLQ;
	p2p_queue_grps_info->qg_info.groups[0].qg_id.queue_group_id = CPFL_P2P_QUEUE_GRP_ID;
	p2p_queue_grps_info->qg_info.groups[0].qg_id.queue_group_type = VIRTCHNL2_QUEUE_GROUP_P2P;
	p2p_queue_grps_info->qg_info.groups[0].rx_q_grp_info.rss_lut_size = 0;
	p2p_queue_grps_info->qg_info.groups[0].tx_q_grp_info.tx_tc = 0;
	p2p_queue_grps_info->qg_info.groups[0].tx_q_grp_info.priority = 0;
	p2p_queue_grps_info->qg_info.groups[0].tx_q_grp_info.is_sp = 0;
	p2p_queue_grps_info->qg_info.groups[0].tx_q_grp_info.pir_weight = 0;

	ret = idpf_vc_queue_grps_add(vport, p2p_queue_grps_info, p2p_q_vc_out_info);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Failed to add p2p queue groups.");
		return ret;
	}

	return ret;
}

static int
cpfl_p2p_queue_info_init(struct cpfl_vport *cpfl_vport,
			 struct virtchnl2_add_queue_groups *p2p_q_vc_out_info)
{
	struct p2p_queue_chunks_info *p2p_q_chunks_info = cpfl_vport->p2p_q_chunks_info;
	struct virtchnl2_queue_reg_chunks *vc_chunks_out;
	int i, type;

	if (p2p_q_vc_out_info->qg_info.groups[0].qg_id.queue_group_type !=
	    VIRTCHNL2_QUEUE_GROUP_P2P) {
		PMD_DRV_LOG(ERR, "Add queue group response mismatch.");
		return -EINVAL;
	}

	vc_chunks_out = &p2p_q_vc_out_info->qg_info.groups[0].chunks;

	for (i = 0; i < vc_chunks_out->num_chunks; i++) {
		type = vc_chunks_out->chunks[i].type;
		switch (type) {
		case VIRTCHNL2_QUEUE_TYPE_TX:
			p2p_q_chunks_info->tx_start_qid =
				vc_chunks_out->chunks[i].start_queue_id;
			p2p_q_chunks_info->tx_qtail_start =
				vc_chunks_out->chunks[i].qtail_reg_start;
			p2p_q_chunks_info->tx_qtail_spacing =
				vc_chunks_out->chunks[i].qtail_reg_spacing;
			break;
		case VIRTCHNL2_QUEUE_TYPE_RX:
			p2p_q_chunks_info->rx_start_qid =
				vc_chunks_out->chunks[i].start_queue_id;
			p2p_q_chunks_info->rx_qtail_start =
				vc_chunks_out->chunks[i].qtail_reg_start;
			p2p_q_chunks_info->rx_qtail_spacing =
				vc_chunks_out->chunks[i].qtail_reg_spacing;
			break;
		case VIRTCHNL2_QUEUE_TYPE_TX_COMPLETION:
			p2p_q_chunks_info->tx_compl_start_qid =
				vc_chunks_out->chunks[i].start_queue_id;
			p2p_q_chunks_info->tx_compl_qtail_start =
				vc_chunks_out->chunks[i].qtail_reg_start;
			p2p_q_chunks_info->tx_compl_qtail_spacing =
				vc_chunks_out->chunks[i].qtail_reg_spacing;
			break;
		case VIRTCHNL2_QUEUE_TYPE_RX_BUFFER:
			p2p_q_chunks_info->rx_buf_start_qid =
				vc_chunks_out->chunks[i].start_queue_id;
			p2p_q_chunks_info->rx_buf_qtail_start =
				vc_chunks_out->chunks[i].qtail_reg_start;
			p2p_q_chunks_info->rx_buf_qtail_spacing =
				vc_chunks_out->chunks[i].qtail_reg_spacing;
			break;
		default:
			PMD_DRV_LOG(ERR, "Unsupported queue type");
			break;
		}
	}

	return 0;
}

int
cpfl_alloc_dma_mem_batch(struct idpf_dma_mem *orig_dma, struct idpf_dma_mem *dma, uint32_t size,
			 int batch_size)
{
	int i;

	if (!idpf_alloc_dma_mem(NULL, orig_dma, size * (1 + batch_size))) {
		PMD_INIT_LOG(ERR, "Could not alloc dma memory");
		return -ENOMEM;
	}

	for (i = 0; i < batch_size; i++) {
		dma[i].va = (void *)((char *)orig_dma->va + size * (i + 1));
		dma[i].pa = orig_dma->pa + size * (i + 1);
		dma[i].size = size;
		dma[i].zone = NULL;
	}
	return 0;
}

static int
cpfl_dev_vport_init(struct rte_eth_dev *dev, void *init_params)
{
	struct cpfl_vport *cpfl_vport = dev->data->dev_private;
	struct idpf_vport *vport = &cpfl_vport->base;
	struct cpfl_vport_param *param = init_params;
	struct cpfl_adapter_ext *adapter = param->adapter;
	/* for sending create vport virtchnl msg prepare */
	struct virtchnl2_create_vport create_vport_info;
	struct virtchnl2_add_queue_groups p2p_queue_grps_info;
	uint8_t p2p_q_vc_out_info[IDPF_DFLT_MBX_BUF_SIZE] = {0};
	int ret = 0;

	dev->dev_ops = &cpfl_eth_dev_ops;
	vport->adapter = &adapter->base;
	vport->sw_idx = param->idx;
	vport->devarg_id = param->devarg_id;

	memset(&create_vport_info, 0, sizeof(create_vport_info));
	ret = idpf_vport_info_init(vport, &create_vport_info);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Failed to init vport req_info.");
		goto err;
	}

	ret = idpf_vport_init(vport, &create_vport_info, dev->data);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Failed to init vports.");
		goto err;
	}

	cpfl_vport->itf.type = CPFL_ITF_TYPE_VPORT;
	cpfl_vport->itf.adapter = adapter;
	cpfl_vport->itf.data = dev->data;
	TAILQ_INIT(&cpfl_vport->itf.flow_list);
	adapter->vports[param->idx] = cpfl_vport;
	adapter->cur_vports |= RTE_BIT32(param->devarg_id);
	adapter->cur_vport_nb++;

	dev->data->mac_addrs = rte_zmalloc(NULL, RTE_ETHER_ADDR_LEN, 0);
	if (dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR, "Cannot allocate mac_addr memory.");
		ret = -ENOMEM;
		goto err_mac_addrs;
	}

	rte_ether_addr_copy((struct rte_ether_addr *)vport->default_mac_addr,
			    &dev->data->mac_addrs[0]);

	memset(cpfl_vport->itf.dma, 0, sizeof(cpfl_vport->itf.dma));
	memset(cpfl_vport->itf.msg, 0, sizeof(cpfl_vport->itf.msg));
	ret = cpfl_alloc_dma_mem_batch(&cpfl_vport->itf.flow_dma,
				       cpfl_vport->itf.dma,
				       sizeof(union cpfl_rule_cfg_pkt_record),
				       CPFL_FLOW_BATCH_SIZE);
	if (ret < 0)
		goto err_mac_addrs;

	if (!adapter->base.is_rx_singleq && !adapter->base.is_tx_singleq) {
		memset(&p2p_queue_grps_info, 0, sizeof(p2p_queue_grps_info));
		ret = cpfl_p2p_q_grps_add(vport, &p2p_queue_grps_info, p2p_q_vc_out_info);
		if (ret != 0) {
			PMD_INIT_LOG(WARNING, "Failed to add p2p queue group.");
			return 0;
		}
		cpfl_vport->p2p_q_chunks_info = rte_zmalloc(NULL,
						    sizeof(struct p2p_queue_chunks_info), 0);
		if (cpfl_vport->p2p_q_chunks_info == NULL) {
			PMD_INIT_LOG(WARNING, "Failed to allocate p2p queue info.");
			cpfl_p2p_queue_grps_del(vport);
			return 0;
		}
		ret = cpfl_p2p_queue_info_init(cpfl_vport,
				       (struct virtchnl2_add_queue_groups *)p2p_q_vc_out_info);
		if (ret != 0) {
			PMD_INIT_LOG(WARNING, "Failed to init p2p queue info.");
			rte_free(cpfl_vport->p2p_q_chunks_info);
			cpfl_p2p_queue_grps_del(vport);
		}
	}

	return 0;

err_mac_addrs:
	adapter->vports[param->idx] = NULL;  /* reset */
	idpf_vport_deinit(vport);
	adapter->cur_vports &= ~RTE_BIT32(param->devarg_id);
	adapter->cur_vport_nb--;
err:
	return ret;
}

static const struct rte_pci_id pci_id_cpfl_map[] = {
	{ RTE_PCI_DEVICE(IDPF_INTEL_VENDOR_ID, IDPF_DEV_ID_CPF) },
	{ .vendor_id = 0, /* sentinel */ },
};

static struct cpfl_adapter_ext *
cpfl_find_adapter_ext(struct rte_pci_device *pci_dev)
{
	struct cpfl_adapter_ext *adapter;
	int found = 0;

	if (pci_dev == NULL)
		return NULL;

	rte_spinlock_lock(&cpfl_adapter_lock);
	TAILQ_FOREACH(adapter, &cpfl_adapter_list, next) {
		if (strncmp(adapter->name, pci_dev->device.name, PCI_PRI_STR_SIZE) == 0) {
			found = 1;
			break;
		}
	}
	rte_spinlock_unlock(&cpfl_adapter_lock);

	if (found == 0)
		return NULL;

	return adapter;
}

static void
cpfl_adapter_ext_deinit(struct cpfl_adapter_ext *adapter)
{
#ifdef RTE_HAS_JANSSON
	cpfl_flow_uninit(adapter);
#endif
	cpfl_ctrl_path_close(adapter);
	rte_eal_alarm_cancel(cpfl_dev_alarm_handler, adapter);
	cpfl_vport_map_uninit(adapter);
	idpf_adapter_deinit(&adapter->base);

	rte_free(adapter->vports);
	adapter->vports = NULL;
}

static int
cpfl_vport_devargs_process(struct cpfl_adapter_ext *adapter)
{
	struct cpfl_devargs *devargs = &adapter->devargs;
	int i;

	/* refine vport number, at least 1 vport */
	if (devargs->req_vport_nb == 0) {
		devargs->req_vport_nb = 1;
		devargs->req_vports[0] = 0;
	}

	/* check parsed devargs */
	if (adapter->cur_vport_nb + devargs->req_vport_nb >
	    adapter->max_vport_nb) {
		PMD_INIT_LOG(ERR, "Total vport number can't be > %d",
			     adapter->max_vport_nb);
		return -EINVAL;
	}

	for (i = 0; i < devargs->req_vport_nb; i++) {
		if (devargs->req_vports[i] > adapter->max_vport_nb - 1) {
			PMD_INIT_LOG(ERR, "Invalid vport id %d, it should be 0 ~ %d",
				     devargs->req_vports[i], adapter->max_vport_nb - 1);
			return -EINVAL;
		}

		if (adapter->cur_vports & RTE_BIT32(devargs->req_vports[i])) {
			PMD_INIT_LOG(ERR, "Vport %d has been requested",
				     devargs->req_vports[i]);
			return -EINVAL;
		}
	}

	return 0;
}

static int
cpfl_vport_create(struct rte_pci_device *pci_dev, struct cpfl_adapter_ext *adapter)
{
	struct cpfl_vport_param vport_param;
	char name[RTE_ETH_NAME_MAX_LEN];
	int ret, i;

	for (i = 0; i < adapter->devargs.req_vport_nb; i++) {
		vport_param.adapter = adapter;
		vport_param.devarg_id = adapter->devargs.req_vports[i];
		vport_param.idx = cpfl_vport_idx_alloc(adapter);
		if (vport_param.idx == CPFL_INVALID_VPORT_IDX) {
			PMD_INIT_LOG(ERR, "No space for vport %u", vport_param.devarg_id);
			break;
		}
		snprintf(name, sizeof(name), "net_%s_vport_%d",
			 pci_dev->device.name,
			 adapter->devargs.req_vports[i]);
		ret = rte_eth_dev_create(&pci_dev->device, name,
					    sizeof(struct cpfl_vport),
					    NULL, NULL, cpfl_dev_vport_init,
					    &vport_param);
		if (ret != 0)
			PMD_DRV_LOG(ERR, "Failed to create vport %d",
				    vport_param.devarg_id);
	}

	return 0;
}

static int
cpfl_pci_probe_first(struct rte_pci_device *pci_dev)
{
	struct cpfl_adapter_ext *adapter;
	int retval;
	uint16_t port_id;

	adapter = rte_zmalloc("cpfl_adapter_ext",
			      sizeof(struct cpfl_adapter_ext), 0);
	if (adapter == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate adapter.");
		return -ENOMEM;
	}

	retval = cpfl_parse_devargs(pci_dev, adapter, true);
	if (retval != 0) {
		PMD_INIT_LOG(ERR, "Failed to parse private devargs");
		return retval;
	}

	retval = cpfl_adapter_ext_init(pci_dev, adapter);
	if (retval != 0) {
		PMD_INIT_LOG(ERR, "Failed to init adapter.");
		return retval;
	}

	rte_spinlock_lock(&cpfl_adapter_lock);
	TAILQ_INSERT_TAIL(&cpfl_adapter_list, adapter, next);
	rte_spinlock_unlock(&cpfl_adapter_lock);

	retval = cpfl_vport_devargs_process(adapter);
	if (retval != 0) {
		PMD_INIT_LOG(ERR, "Failed to process vport devargs");
		goto err;
	}

	retval = cpfl_vport_create(pci_dev, adapter);
	if (retval != 0) {
		PMD_INIT_LOG(ERR, "Failed to create vports.");
		goto err;
	}

	retval = cpfl_repr_devargs_process(adapter);
	if (retval != 0) {
		PMD_INIT_LOG(ERR, "Failed to process repr devargs");
		goto close_ethdev;
	}

	retval = cpfl_repr_create(pci_dev, adapter);
	if (retval != 0) {
		PMD_INIT_LOG(ERR, "Failed to create representors ");
		goto close_ethdev;
	}


	return 0;

close_ethdev:
	/* Ethdev created can be found RTE_ETH_FOREACH_DEV_OF through rte_device */
	RTE_ETH_FOREACH_DEV_OF(port_id, &pci_dev->device) {
		rte_eth_dev_close(port_id);
	}
err:
	rte_spinlock_lock(&cpfl_adapter_lock);
	TAILQ_REMOVE(&cpfl_adapter_list, adapter, next);
	rte_spinlock_unlock(&cpfl_adapter_lock);
	cpfl_adapter_ext_deinit(adapter);
	rte_free(adapter);
	return retval;
}

static int
cpfl_pci_probe_again(struct rte_pci_device *pci_dev, struct cpfl_adapter_ext *adapter)
{
	int ret;

	ret = cpfl_parse_devargs(pci_dev, adapter, false);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Failed to parse private devargs");
		return ret;
	}

	ret = cpfl_repr_devargs_process(adapter);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Failed to process reprenstor devargs");
		return ret;
	}

	ret = cpfl_repr_create(pci_dev, adapter);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Failed to create representors ");
		return ret;
	}

	return 0;
}

static int
cpfl_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	       struct rte_pci_device *pci_dev)
{
	struct cpfl_adapter_ext *adapter;

	if (!cpfl_adapter_list_init) {
		rte_spinlock_init(&cpfl_adapter_lock);
		TAILQ_INIT(&cpfl_adapter_list);
		cpfl_adapter_list_init = true;
	}

	adapter = cpfl_find_adapter_ext(pci_dev);

	if (adapter == NULL)
		return cpfl_pci_probe_first(pci_dev);
	else
		return cpfl_pci_probe_again(pci_dev, adapter);
}

static int
cpfl_pci_remove(struct rte_pci_device *pci_dev)
{
	struct cpfl_adapter_ext *adapter = cpfl_find_adapter_ext(pci_dev);
	uint16_t port_id;

	/* Ethdev created can be found RTE_ETH_FOREACH_DEV_OF through rte_device */
	RTE_ETH_FOREACH_DEV_OF(port_id, &pci_dev->device) {
			rte_eth_dev_close(port_id);
	}

	rte_spinlock_lock(&cpfl_adapter_lock);
	TAILQ_REMOVE(&cpfl_adapter_list, adapter, next);
	rte_spinlock_unlock(&cpfl_adapter_lock);
	cpfl_adapter_ext_deinit(adapter);
	rte_free(adapter);

	return 0;
}

static struct rte_pci_driver rte_cpfl_pmd = {
	.id_table	= pci_id_cpfl_map,
	.drv_flags	= RTE_PCI_DRV_NEED_MAPPING |
			  RTE_PCI_DRV_PROBE_AGAIN,
	.probe		= cpfl_pci_probe,
	.remove		= cpfl_pci_remove,
};

/**
 * Driver initialization routine.
 * Invoked once at EAL init time.
 * Register itself as the [Poll Mode] Driver of PCI devices.
 */
RTE_PMD_REGISTER_PCI(net_cpfl, rte_cpfl_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_cpfl, pci_id_cpfl_map);
RTE_PMD_REGISTER_KMOD_DEP(net_cpfl, "* igb_uio | vfio-pci");
RTE_PMD_REGISTER_PARAM_STRING(net_cpfl,
	CPFL_TX_SINGLE_Q "=<0|1> "
	CPFL_RX_SINGLE_Q "=<0|1> "
	CPFL_VPORT "=[vport0_begin[-vport0_end][,vport1_begin[-vport1_end]][,..]]");

RTE_LOG_REGISTER_SUFFIX(cpfl_logtype_init, init, NOTICE);
RTE_LOG_REGISTER_SUFFIX(cpfl_logtype_driver, driver, NOTICE);
