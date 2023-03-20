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

#include "cpfl_ethdev.h"
#include "cpfl_rxtx.h"

#define CPFL_TX_SINGLE_Q	"tx_single"
#define CPFL_RX_SINGLE_Q	"rx_single"
#define CPFL_VPORT		"vport"

rte_spinlock_t cpfl_adapter_lock;
/* A list for all adapters, one adapter matches one PCI device */
struct cpfl_adapter_list cpfl_adapter_list;
bool cpfl_adapter_list_init;

static const char * const cpfl_valid_args[] = {
	CPFL_TX_SINGLE_Q,
	CPFL_RX_SINGLE_Q,
	CPFL_VPORT,
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
	struct idpf_vport *vport = dev->data->dev_private;
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
cpfl_dev_info_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	struct idpf_vport *vport = dev->data->dev_private;
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
	struct idpf_vport *vport = dev->data->dev_private;

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
	struct idpf_rx_queue *rxq;
	int i = 0;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];
		mbuf_alloc_failed += __atomic_load_n(&rxq->rx_stats.mbuf_alloc_failed,
						     __ATOMIC_RELAXED);
	}

	return mbuf_alloc_failed;
}

static int
cpfl_dev_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	struct idpf_vport *vport =
		(struct idpf_vport *)dev->data->dev_private;
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
	struct idpf_rx_queue *rxq;
	int i;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];
		__atomic_store_n(&rxq->rx_stats.mbuf_alloc_failed, 0, __ATOMIC_RELAXED);
	}
}

static int
cpfl_dev_stats_reset(struct rte_eth_dev *dev)
{
	struct idpf_vport *vport =
		(struct idpf_vport *)dev->data->dev_private;
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
	struct idpf_vport *vport =
		(struct idpf_vport *)dev->data->dev_private;
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
	struct idpf_vport *vport = dev->data->dev_private;
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
	struct idpf_vport *vport = dev->data->dev_private;
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
	struct idpf_vport *vport = dev->data->dev_private;
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
	struct idpf_vport *vport = dev->data->dev_private;
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
	struct idpf_vport *vport = dev->data->dev_private;
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

	if (base->caps.rss_caps != 0 && dev->data->nb_rx_queues != 0 &&
		conf->rxmode.mq_mode == RTE_ETH_MQ_RX_RSS) {
		ret = cpfl_init_rss(vport);
		if (ret != 0) {
			PMD_INIT_LOG(ERR, "Failed to init rss");
			return ret;
		}
	} else {
		PMD_INIT_LOG(ERR, "RSS is not supported.");
		if (conf->rxmode.mq_mode == RTE_ETH_MQ_RX_RSS)
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
	struct idpf_vport *vport = dev->data->dev_private;
	uint16_t nb_rx_queues = dev->data->nb_rx_queues;

	return idpf_vport_irq_map_config(vport, nb_rx_queues);
}

static int
cpfl_start_queues(struct rte_eth_dev *dev)
{
	struct idpf_rx_queue *rxq;
	struct idpf_tx_queue *txq;
	int err = 0;
	int i;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		if (txq == NULL || txq->tx_deferred_start)
			continue;
		err = cpfl_tx_queue_start(dev, i);
		if (err != 0) {
			PMD_DRV_LOG(ERR, "Fail to start Tx queue %u", i);
			return err;
		}
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];
		if (rxq == NULL || rxq->rx_deferred_start)
			continue;
		err = cpfl_rx_queue_start(dev, i);
		if (err != 0) {
			PMD_DRV_LOG(ERR, "Fail to start Rx queue %u", i);
			return err;
		}
	}

	return err;
}

static int
cpfl_dev_start(struct rte_eth_dev *dev)
{
	struct idpf_vport *vport = dev->data->dev_private;
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

	vport->stopped = 0;

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
	struct idpf_vport *vport = dev->data->dev_private;

	if (vport->stopped == 1)
		return 0;

	idpf_vc_vport_ena_dis(vport, false);

	cpfl_stop_queues(dev);

	idpf_vport_irq_unmap_config(vport, dev->data->nb_rx_queues);

	idpf_vc_vectors_dealloc(vport);

	vport->stopped = 1;

	return 0;
}

static int
cpfl_dev_close(struct rte_eth_dev *dev)
{
	struct idpf_vport *vport = dev->data->dev_private;
	struct cpfl_adapter_ext *adapter = CPFL_ADAPTER_TO_EXT(vport->adapter);

	cpfl_dev_stop(dev);
	idpf_vport_deinit(vport);

	adapter->cur_vports &= ~RTE_BIT32(vport->devarg_id);
	adapter->cur_vport_nb--;
	dev->data->dev_private = NULL;
	adapter->vports[vport->sw_idx] = NULL;
	rte_free(vport);

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
cpfl_parse_devargs(struct rte_pci_device *pci_dev, struct cpfl_adapter_ext *adapter,
		   struct cpfl_devargs *cpfl_args)
{
	struct rte_devargs *devargs = pci_dev->device.devargs;
	struct rte_kvargs *kvlist;
	int i, ret;

	cpfl_args->req_vport_nb = 0;

	if (devargs == NULL)
		return 0;

	kvlist = rte_kvargs_parse(devargs->args, cpfl_valid_args);
	if (kvlist == NULL) {
		PMD_INIT_LOG(ERR, "invalid kvargs key");
		return -EINVAL;
	}

	if (rte_kvargs_count(kvlist, CPFL_VPORT) > 1) {
		PMD_INIT_LOG(ERR, "devarg vport is duplicated.");
		return -EINVAL;
	}

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

	/* check parsed devargs */
	if (adapter->cur_vport_nb + cpfl_args->req_vport_nb >
	    adapter->max_vport_nb) {
		PMD_INIT_LOG(ERR, "Total vport number can't be > %d",
			     adapter->max_vport_nb);
		ret = -EINVAL;
		goto fail;
	}

	for (i = 0; i < cpfl_args->req_vport_nb; i++) {
		if (cpfl_args->req_vports[i] > adapter->max_vport_nb - 1) {
			PMD_INIT_LOG(ERR, "Invalid vport id %d, it should be 0 ~ %d",
				     cpfl_args->req_vports[i], adapter->max_vport_nb - 1);
			ret = -EINVAL;
			goto fail;
		}

		if (adapter->cur_vports & RTE_BIT32(cpfl_args->req_vports[i])) {
			PMD_INIT_LOG(ERR, "Vport %d has been requested",
				     cpfl_args->req_vports[i]);
			ret = -EINVAL;
			goto fail;
		}
	}

fail:
	rte_kvargs_free(kvlist);
	return ret;
}

static struct idpf_vport *
cpfl_find_vport(struct cpfl_adapter_ext *adapter, uint32_t vport_id)
{
	struct idpf_vport *vport = NULL;
	int i;

	for (i = 0; i < adapter->cur_vport_nb; i++) {
		vport = adapter->vports[i];
		if (vport->vport_id != vport_id)
			continue;
		else
			return vport;
	}

	return vport;
}

static void
cpfl_handle_event_msg(struct idpf_vport *vport, uint8_t *msg, uint16_t msglen)
{
	struct virtchnl2_event *vc_event = (struct virtchnl2_event *)msg;
	struct rte_eth_dev *dev = (struct rte_eth_dev *)vport->dev;

	if (msglen < sizeof(struct virtchnl2_event)) {
		PMD_DRV_LOG(ERR, "Error event");
		return;
	}

	switch (vc_event->event) {
	case VIRTCHNL2_EVENT_LINK_CHANGE:
		PMD_DRV_LOG(DEBUG, "VIRTCHNL2_EVENT_LINK_CHANGE");
		vport->link_up = !!(vc_event->link_status);
		vport->link_speed = vc_event->link_speed;
		cpfl_dev_link_update(dev, 0);
		break;
	default:
		PMD_DRV_LOG(ERR, " unknown event received %u", vc_event->event);
		break;
	}
}

static void
cpfl_handle_virtchnl_msg(struct cpfl_adapter_ext *adapter)
{
	struct idpf_adapter *base = &adapter->base;
	struct idpf_dma_mem *dma_mem = NULL;
	struct idpf_hw *hw = &base->hw;
	struct virtchnl2_event *vc_event;
	struct idpf_ctlq_msg ctlq_msg;
	enum idpf_mbx_opc mbx_op;
	struct idpf_vport *vport;
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
				if (ctlq_msg.data_len < sizeof(struct virtchnl2_event)) {
					PMD_DRV_LOG(ERR, "Error event");
					return;
				}
				vc_event = (struct virtchnl2_event *)base->mbx_resp;
				vport = cpfl_find_vport(adapter, vc_event->vport_id);
				if (!vport) {
					PMD_DRV_LOG(ERR, "Can't find vport.");
					return;
				}
				cpfl_handle_event_msg(vport, base->mbx_resp,
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

	ret = idpf_adapter_init(base);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Failed to init adapter");
		goto err_adapter_init;
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

	adapter->cur_vports = 0;
	adapter->cur_vport_nb = 0;

	adapter->used_vecs_num = 0;

	return ret;

err_vports_alloc:
	rte_eal_alarm_cancel(cpfl_dev_alarm_handler, adapter);
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
cpfl_dev_vport_init(struct rte_eth_dev *dev, void *init_params)
{
	struct idpf_vport *vport = dev->data->dev_private;
	struct cpfl_vport_param *param = init_params;
	struct cpfl_adapter_ext *adapter = param->adapter;
	/* for sending create vport virtchnl msg prepare */
	struct virtchnl2_create_vport create_vport_info;
	int ret = 0;

	dev->dev_ops = &cpfl_eth_dev_ops;
	vport->adapter = &adapter->base;
	vport->sw_idx = param->idx;
	vport->devarg_id = param->devarg_id;
	vport->dev = dev;

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

	adapter->vports[param->idx] = vport;
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
	rte_eal_alarm_cancel(cpfl_dev_alarm_handler, adapter);
	idpf_adapter_deinit(&adapter->base);

	rte_free(adapter->vports);
	adapter->vports = NULL;
}

static int
cpfl_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	       struct rte_pci_device *pci_dev)
{
	struct cpfl_vport_param vport_param;
	struct cpfl_adapter_ext *adapter;
	struct cpfl_devargs devargs;
	char name[RTE_ETH_NAME_MAX_LEN];
	int i, retval;

	if (!cpfl_adapter_list_init) {
		rte_spinlock_init(&cpfl_adapter_lock);
		TAILQ_INIT(&cpfl_adapter_list);
		cpfl_adapter_list_init = true;
	}

	adapter = rte_zmalloc("cpfl_adapter_ext",
			      sizeof(struct cpfl_adapter_ext), 0);
	if (adapter == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate adapter.");
		return -ENOMEM;
	}

	retval = cpfl_adapter_ext_init(pci_dev, adapter);
	if (retval != 0) {
		PMD_INIT_LOG(ERR, "Failed to init adapter.");
		return retval;
	}

	rte_spinlock_lock(&cpfl_adapter_lock);
	TAILQ_INSERT_TAIL(&cpfl_adapter_list, adapter, next);
	rte_spinlock_unlock(&cpfl_adapter_lock);

	retval = cpfl_parse_devargs(pci_dev, adapter, &devargs);
	if (retval != 0) {
		PMD_INIT_LOG(ERR, "Failed to parse private devargs");
		goto err;
	}

	if (devargs.req_vport_nb == 0) {
		/* If no vport devarg, create vport 0 by default. */
		vport_param.adapter = adapter;
		vport_param.devarg_id = 0;
		vport_param.idx = cpfl_vport_idx_alloc(adapter);
		if (vport_param.idx == CPFL_INVALID_VPORT_IDX) {
			PMD_INIT_LOG(ERR, "No space for vport %u", vport_param.devarg_id);
			return 0;
		}
		snprintf(name, sizeof(name), "cpfl_%s_vport_0",
			 pci_dev->device.name);
		retval = rte_eth_dev_create(&pci_dev->device, name,
					    sizeof(struct idpf_vport),
					    NULL, NULL, cpfl_dev_vport_init,
					    &vport_param);
		if (retval != 0)
			PMD_DRV_LOG(ERR, "Failed to create default vport 0");
	} else {
		for (i = 0; i < devargs.req_vport_nb; i++) {
			vport_param.adapter = adapter;
			vport_param.devarg_id = devargs.req_vports[i];
			vport_param.idx = cpfl_vport_idx_alloc(adapter);
			if (vport_param.idx == CPFL_INVALID_VPORT_IDX) {
				PMD_INIT_LOG(ERR, "No space for vport %u", vport_param.devarg_id);
				break;
			}
			snprintf(name, sizeof(name), "cpfl_%s_vport_%d",
				 pci_dev->device.name,
				 devargs.req_vports[i]);
			retval = rte_eth_dev_create(&pci_dev->device, name,
						    sizeof(struct idpf_vport),
						    NULL, NULL, cpfl_dev_vport_init,
						    &vport_param);
			if (retval != 0)
				PMD_DRV_LOG(ERR, "Failed to create vport %d",
					    vport_param.devarg_id);
		}
	}

	return 0;

err:
	rte_spinlock_lock(&cpfl_adapter_lock);
	TAILQ_REMOVE(&cpfl_adapter_list, adapter, next);
	rte_spinlock_unlock(&cpfl_adapter_lock);
	cpfl_adapter_ext_deinit(adapter);
	rte_free(adapter);
	return retval;
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
	.drv_flags	= RTE_PCI_DRV_NEED_MAPPING,
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
