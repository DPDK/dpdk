/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <ctype.h>
#include <sys/queue.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <inttypes.h>
#include <rte_byteorder.h>
#include <rte_common.h>

#include <rte_interrupts.h>
#include <rte_debug.h>
#include <rte_pci.h>
#include <rte_alarm.h>
#include <rte_atomic.h>
#include <rte_eal.h>
#include <rte_ether.h>
#include <ethdev_driver.h>
#include <ethdev_pci.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <dev_driver.h>

#include "iavf.h"
#include "iavf_rxtx.h"
#include "iavf_generic_flow.h"
#include "rte_pmd_iavf.h"
#include "iavf_ipsec_crypto.h"

/* devargs */
#define IAVF_PROTO_XTR_ARG         "proto_xtr"
#define IAVF_QUANTA_SIZE_ARG       "quanta_size"

uint64_t iavf_timestamp_dynflag;
int iavf_timestamp_dynfield_offset = -1;

static const char * const iavf_valid_args[] = {
	IAVF_PROTO_XTR_ARG,
	IAVF_QUANTA_SIZE_ARG,
	NULL
};

static const struct rte_mbuf_dynfield iavf_proto_xtr_metadata_param = {
	.name = "intel_pmd_dynfield_proto_xtr_metadata",
	.size = sizeof(uint32_t),
	.align = __alignof__(uint32_t),
	.flags = 0,
};

struct iavf_proto_xtr_ol {
	const struct rte_mbuf_dynflag param;
	uint64_t *ol_flag;
	bool required;
};

static struct iavf_proto_xtr_ol iavf_proto_xtr_params[] = {
	[IAVF_PROTO_XTR_VLAN] = {
		.param = { .name = "intel_pmd_dynflag_proto_xtr_vlan" },
		.ol_flag = &rte_pmd_ifd_dynflag_proto_xtr_vlan_mask },
	[IAVF_PROTO_XTR_IPV4] = {
		.param = { .name = "intel_pmd_dynflag_proto_xtr_ipv4" },
		.ol_flag = &rte_pmd_ifd_dynflag_proto_xtr_ipv4_mask },
	[IAVF_PROTO_XTR_IPV6] = {
		.param = { .name = "intel_pmd_dynflag_proto_xtr_ipv6" },
		.ol_flag = &rte_pmd_ifd_dynflag_proto_xtr_ipv6_mask },
	[IAVF_PROTO_XTR_IPV6_FLOW] = {
		.param = { .name = "intel_pmd_dynflag_proto_xtr_ipv6_flow" },
		.ol_flag = &rte_pmd_ifd_dynflag_proto_xtr_ipv6_flow_mask },
	[IAVF_PROTO_XTR_TCP] = {
		.param = { .name = "intel_pmd_dynflag_proto_xtr_tcp" },
		.ol_flag = &rte_pmd_ifd_dynflag_proto_xtr_tcp_mask },
	[IAVF_PROTO_XTR_IP_OFFSET] = {
		.param = { .name = "intel_pmd_dynflag_proto_xtr_ip_offset" },
		.ol_flag = &rte_pmd_ifd_dynflag_proto_xtr_ip_offset_mask },
	[IAVF_PROTO_XTR_IPSEC_CRYPTO_SAID] = {
		.param = {
		.name = "intel_pmd_dynflag_proto_xtr_ipsec_crypto_said" },
		.ol_flag =
			&rte_pmd_ifd_dynflag_proto_xtr_ipsec_crypto_said_mask },
};

static int iavf_dev_configure(struct rte_eth_dev *dev);
static int iavf_dev_start(struct rte_eth_dev *dev);
static int iavf_dev_stop(struct rte_eth_dev *dev);
static int iavf_dev_close(struct rte_eth_dev *dev);
static int iavf_dev_reset(struct rte_eth_dev *dev);
static int iavf_dev_info_get(struct rte_eth_dev *dev,
			     struct rte_eth_dev_info *dev_info);
static const uint32_t *iavf_dev_supported_ptypes_get(struct rte_eth_dev *dev);
static int iavf_dev_stats_get(struct rte_eth_dev *dev,
			     struct rte_eth_stats *stats);
static int iavf_dev_stats_reset(struct rte_eth_dev *dev);
static int iavf_dev_xstats_reset(struct rte_eth_dev *dev);
static int iavf_dev_xstats_get(struct rte_eth_dev *dev,
				 struct rte_eth_xstat *xstats, unsigned int n);
static int iavf_dev_xstats_get_names(struct rte_eth_dev *dev,
				       struct rte_eth_xstat_name *xstats_names,
				       unsigned int limit);
static int iavf_dev_promiscuous_enable(struct rte_eth_dev *dev);
static int iavf_dev_promiscuous_disable(struct rte_eth_dev *dev);
static int iavf_dev_allmulticast_enable(struct rte_eth_dev *dev);
static int iavf_dev_allmulticast_disable(struct rte_eth_dev *dev);
static int iavf_dev_add_mac_addr(struct rte_eth_dev *dev,
				struct rte_ether_addr *addr,
				uint32_t index,
				uint32_t pool);
static void iavf_dev_del_mac_addr(struct rte_eth_dev *dev, uint32_t index);
static int iavf_dev_vlan_filter_set(struct rte_eth_dev *dev,
				   uint16_t vlan_id, int on);
static int iavf_dev_vlan_offload_set(struct rte_eth_dev *dev, int mask);
static int iavf_dev_rss_reta_update(struct rte_eth_dev *dev,
				   struct rte_eth_rss_reta_entry64 *reta_conf,
				   uint16_t reta_size);
static int iavf_dev_rss_reta_query(struct rte_eth_dev *dev,
				  struct rte_eth_rss_reta_entry64 *reta_conf,
				  uint16_t reta_size);
static int iavf_dev_rss_hash_update(struct rte_eth_dev *dev,
				   struct rte_eth_rss_conf *rss_conf);
static int iavf_dev_rss_hash_conf_get(struct rte_eth_dev *dev,
				     struct rte_eth_rss_conf *rss_conf);
static int iavf_dev_mtu_set(struct rte_eth_dev *dev, uint16_t mtu);
static int iavf_dev_set_default_mac_addr(struct rte_eth_dev *dev,
					 struct rte_ether_addr *mac_addr);
static int iavf_dev_rx_queue_intr_enable(struct rte_eth_dev *dev,
					uint16_t queue_id);
static int iavf_dev_rx_queue_intr_disable(struct rte_eth_dev *dev,
					 uint16_t queue_id);
static int iavf_dev_flow_ops_get(struct rte_eth_dev *dev,
				 const struct rte_flow_ops **ops);
static int iavf_set_mc_addr_list(struct rte_eth_dev *dev,
			struct rte_ether_addr *mc_addrs,
			uint32_t mc_addrs_num);
static int iavf_tm_ops_get(struct rte_eth_dev *dev __rte_unused, void *arg);

static const struct rte_pci_id pci_id_iavf_map[] = {
	{ RTE_PCI_DEVICE(IAVF_INTEL_VENDOR_ID, IAVF_DEV_ID_ADAPTIVE_VF) },
	{ RTE_PCI_DEVICE(IAVF_INTEL_VENDOR_ID, IAVF_DEV_ID_VF) },
	{ RTE_PCI_DEVICE(IAVF_INTEL_VENDOR_ID, IAVF_DEV_ID_VF_HV) },
	{ RTE_PCI_DEVICE(IAVF_INTEL_VENDOR_ID, IAVF_DEV_ID_X722_VF) },
	{ RTE_PCI_DEVICE(IAVF_INTEL_VENDOR_ID, IAVF_DEV_ID_X722_A0_VF) },
	{ .vendor_id = 0, /* sentinel */ },
};

struct rte_iavf_xstats_name_off {
	char name[RTE_ETH_XSTATS_NAME_SIZE];
	unsigned int offset;
};

#define _OFF_OF(a) offsetof(struct iavf_eth_xstats, a)
static const struct rte_iavf_xstats_name_off rte_iavf_stats_strings[] = {
	{"rx_bytes", _OFF_OF(eth_stats.rx_bytes)},
	{"rx_unicast_packets", _OFF_OF(eth_stats.rx_unicast)},
	{"rx_multicast_packets", _OFF_OF(eth_stats.rx_multicast)},
	{"rx_broadcast_packets", _OFF_OF(eth_stats.rx_broadcast)},
	{"rx_dropped_packets", _OFF_OF(eth_stats.rx_discards)},
	{"rx_unknown_protocol_packets", offsetof(struct iavf_eth_stats,
		rx_unknown_protocol)},
	{"tx_bytes", _OFF_OF(eth_stats.tx_bytes)},
	{"tx_unicast_packets", _OFF_OF(eth_stats.tx_unicast)},
	{"tx_multicast_packets", _OFF_OF(eth_stats.tx_multicast)},
	{"tx_broadcast_packets", _OFF_OF(eth_stats.tx_broadcast)},
	{"tx_dropped_packets", _OFF_OF(eth_stats.tx_discards)},
	{"tx_error_packets", _OFF_OF(eth_stats.tx_errors)},

	{"inline_ipsec_crypto_ipackets", _OFF_OF(ips_stats.icount)},
	{"inline_ipsec_crypto_ibytes", _OFF_OF(ips_stats.ibytes)},
	{"inline_ipsec_crypto_ierrors", _OFF_OF(ips_stats.ierrors.count)},
	{"inline_ipsec_crypto_ierrors_sad_lookup",
			_OFF_OF(ips_stats.ierrors.sad_miss)},
	{"inline_ipsec_crypto_ierrors_not_processed",
			_OFF_OF(ips_stats.ierrors.not_processed)},
	{"inline_ipsec_crypto_ierrors_icv_fail",
			_OFF_OF(ips_stats.ierrors.icv_check)},
	{"inline_ipsec_crypto_ierrors_length",
			_OFF_OF(ips_stats.ierrors.ipsec_length)},
	{"inline_ipsec_crypto_ierrors_misc",
			_OFF_OF(ips_stats.ierrors.misc)},
};
#undef _OFF_OF

#define IAVF_NB_XSTATS (sizeof(rte_iavf_stats_strings) / \
		sizeof(rte_iavf_stats_strings[0]))

static const struct eth_dev_ops iavf_eth_dev_ops = {
	.dev_configure              = iavf_dev_configure,
	.dev_start                  = iavf_dev_start,
	.dev_stop                   = iavf_dev_stop,
	.dev_close                  = iavf_dev_close,
	.dev_reset                  = iavf_dev_reset,
	.dev_infos_get              = iavf_dev_info_get,
	.dev_supported_ptypes_get   = iavf_dev_supported_ptypes_get,
	.link_update                = iavf_dev_link_update,
	.stats_get                  = iavf_dev_stats_get,
	.stats_reset                = iavf_dev_stats_reset,
	.xstats_get                 = iavf_dev_xstats_get,
	.xstats_get_names           = iavf_dev_xstats_get_names,
	.xstats_reset               = iavf_dev_xstats_reset,
	.promiscuous_enable         = iavf_dev_promiscuous_enable,
	.promiscuous_disable        = iavf_dev_promiscuous_disable,
	.allmulticast_enable        = iavf_dev_allmulticast_enable,
	.allmulticast_disable       = iavf_dev_allmulticast_disable,
	.mac_addr_add               = iavf_dev_add_mac_addr,
	.mac_addr_remove            = iavf_dev_del_mac_addr,
	.set_mc_addr_list			= iavf_set_mc_addr_list,
	.vlan_filter_set            = iavf_dev_vlan_filter_set,
	.vlan_offload_set           = iavf_dev_vlan_offload_set,
	.rx_queue_start             = iavf_dev_rx_queue_start,
	.rx_queue_stop              = iavf_dev_rx_queue_stop,
	.tx_queue_start             = iavf_dev_tx_queue_start,
	.tx_queue_stop              = iavf_dev_tx_queue_stop,
	.rx_queue_setup             = iavf_dev_rx_queue_setup,
	.rx_queue_release           = iavf_dev_rx_queue_release,
	.tx_queue_setup             = iavf_dev_tx_queue_setup,
	.tx_queue_release           = iavf_dev_tx_queue_release,
	.mac_addr_set               = iavf_dev_set_default_mac_addr,
	.reta_update                = iavf_dev_rss_reta_update,
	.reta_query                 = iavf_dev_rss_reta_query,
	.rss_hash_update            = iavf_dev_rss_hash_update,
	.rss_hash_conf_get          = iavf_dev_rss_hash_conf_get,
	.rxq_info_get               = iavf_dev_rxq_info_get,
	.txq_info_get               = iavf_dev_txq_info_get,
	.mtu_set                    = iavf_dev_mtu_set,
	.rx_queue_intr_enable       = iavf_dev_rx_queue_intr_enable,
	.rx_queue_intr_disable      = iavf_dev_rx_queue_intr_disable,
	.flow_ops_get               = iavf_dev_flow_ops_get,
	.tx_done_cleanup	    = iavf_dev_tx_done_cleanup,
	.get_monitor_addr           = iavf_get_monitor_addr,
	.tm_ops_get                 = iavf_tm_ops_get,
};

static int
iavf_tm_ops_get(struct rte_eth_dev *dev,
			void *arg)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);

	if (adapter->closed)
		return -EIO;

	if (!arg)
		return -EINVAL;

	*(const void **)arg = &iavf_tm_ops;

	return 0;
}

__rte_unused
static int
iavf_vfr_inprogress(struct iavf_hw *hw)
{
	int inprogress = 0;

	if ((IAVF_READ_REG(hw, IAVF_VFGEN_RSTAT) &
		IAVF_VFGEN_RSTAT_VFR_STATE_MASK) ==
		VIRTCHNL_VFR_INPROGRESS)
		inprogress = 1;

	if (inprogress)
		PMD_DRV_LOG(INFO, "Watchdog detected VFR in progress");

	return inprogress;
}

__rte_unused
static void
iavf_dev_watchdog(void *cb_arg)
{
	struct iavf_adapter *adapter = cb_arg;
	struct iavf_hw *hw = IAVF_DEV_PRIVATE_TO_HW(adapter);
	int vfr_inprogress = 0, rc = 0;

	/* check if watchdog has been disabled since last call */
	if (!adapter->vf.watchdog_enabled)
		return;

	/* If in reset then poll vfr_inprogress register for completion */
	if (adapter->vf.vf_reset) {
		vfr_inprogress = iavf_vfr_inprogress(hw);

		if (!vfr_inprogress) {
			PMD_DRV_LOG(INFO, "VF \"%s\" reset has completed",
				adapter->vf.eth_dev->data->name);
			adapter->vf.vf_reset = false;
		}
	/* If not in reset then poll vfr_inprogress register for VFLR event */
	} else {
		vfr_inprogress = iavf_vfr_inprogress(hw);

		if (vfr_inprogress) {
			PMD_DRV_LOG(INFO,
				"VF \"%s\" reset event detected by watchdog",
				adapter->vf.eth_dev->data->name);

			/* enter reset state with VFLR event */
			adapter->vf.vf_reset = true;

			rte_eth_dev_callback_process(adapter->vf.eth_dev,
				RTE_ETH_EVENT_INTR_RESET, NULL);
		}
	}

	/* re-alarm watchdog */
	rc = rte_eal_alarm_set(IAVF_DEV_WATCHDOG_PERIOD,
			&iavf_dev_watchdog, cb_arg);

	if (rc)
		PMD_DRV_LOG(ERR, "Failed \"%s\" to reset device watchdog alarm",
			adapter->vf.eth_dev->data->name);
}

static void
iavf_dev_watchdog_enable(struct iavf_adapter *adapter __rte_unused)
{
#if (IAVF_DEV_WATCHDOG_PERIOD > 0)
	PMD_DRV_LOG(INFO, "Enabling device watchdog");
	adapter->vf.watchdog_enabled = true;
	if (rte_eal_alarm_set(IAVF_DEV_WATCHDOG_PERIOD,
			&iavf_dev_watchdog, (void *)adapter))
		PMD_DRV_LOG(ERR, "Failed to enabled device watchdog");
#endif
}

static void
iavf_dev_watchdog_disable(struct iavf_adapter *adapter __rte_unused)
{
#if (IAVF_DEV_WATCHDOG_PERIOD > 0)
	PMD_DRV_LOG(INFO, "Disabling device watchdog");
	adapter->vf.watchdog_enabled = false;
#endif
}

static int
iavf_set_mc_addr_list(struct rte_eth_dev *dev,
			struct rte_ether_addr *mc_addrs,
			uint32_t mc_addrs_num)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	int err, ret;

	if (mc_addrs_num > IAVF_NUM_MACADDR_MAX) {
		PMD_DRV_LOG(ERR,
			    "can't add more than a limited number (%u) of addresses.",
			    (uint32_t)IAVF_NUM_MACADDR_MAX);
		return -EINVAL;
	}

	if (adapter->closed)
		return -EIO;

	/* flush previous addresses */
	err = iavf_add_del_mc_addr_list(adapter, vf->mc_addrs, vf->mc_addrs_num,
					false);
	if (err)
		return err;

	/* add new ones */
	err = iavf_add_del_mc_addr_list(adapter, mc_addrs, mc_addrs_num, true);

	if (err) {
		/* if adding mac address list fails, should add the previous
		 * addresses back.
		 */
		ret = iavf_add_del_mc_addr_list(adapter, vf->mc_addrs,
						vf->mc_addrs_num, true);
		if (ret)
			return ret;
	} else {
		vf->mc_addrs_num = mc_addrs_num;
		memcpy(vf->mc_addrs,
		       mc_addrs, mc_addrs_num * sizeof(*mc_addrs));
	}

	return err;
}

static void
iavf_config_rss_hf(struct iavf_adapter *adapter, uint64_t rss_hf)
{
	static const uint64_t map_hena_rss[] = {
		/* IPv4 */
		[IAVF_FILTER_PCTYPE_NONF_UNICAST_IPV4_UDP] =
				RTE_ETH_RSS_NONFRAG_IPV4_UDP,
		[IAVF_FILTER_PCTYPE_NONF_MULTICAST_IPV4_UDP] =
				RTE_ETH_RSS_NONFRAG_IPV4_UDP,
		[IAVF_FILTER_PCTYPE_NONF_IPV4_UDP] =
				RTE_ETH_RSS_NONFRAG_IPV4_UDP,
		[IAVF_FILTER_PCTYPE_NONF_IPV4_TCP_SYN_NO_ACK] =
				RTE_ETH_RSS_NONFRAG_IPV4_TCP,
		[IAVF_FILTER_PCTYPE_NONF_IPV4_TCP] =
				RTE_ETH_RSS_NONFRAG_IPV4_TCP,
		[IAVF_FILTER_PCTYPE_NONF_IPV4_SCTP] =
				RTE_ETH_RSS_NONFRAG_IPV4_SCTP,
		[IAVF_FILTER_PCTYPE_NONF_IPV4_OTHER] =
				RTE_ETH_RSS_NONFRAG_IPV4_OTHER,
		[IAVF_FILTER_PCTYPE_FRAG_IPV4] = RTE_ETH_RSS_FRAG_IPV4,

		/* IPv6 */
		[IAVF_FILTER_PCTYPE_NONF_UNICAST_IPV6_UDP] =
				RTE_ETH_RSS_NONFRAG_IPV6_UDP,
		[IAVF_FILTER_PCTYPE_NONF_MULTICAST_IPV6_UDP] =
				RTE_ETH_RSS_NONFRAG_IPV6_UDP,
		[IAVF_FILTER_PCTYPE_NONF_IPV6_UDP] =
				RTE_ETH_RSS_NONFRAG_IPV6_UDP,
		[IAVF_FILTER_PCTYPE_NONF_IPV6_TCP_SYN_NO_ACK] =
				RTE_ETH_RSS_NONFRAG_IPV6_TCP,
		[IAVF_FILTER_PCTYPE_NONF_IPV6_TCP] =
				RTE_ETH_RSS_NONFRAG_IPV6_TCP,
		[IAVF_FILTER_PCTYPE_NONF_IPV6_SCTP] =
				RTE_ETH_RSS_NONFRAG_IPV6_SCTP,
		[IAVF_FILTER_PCTYPE_NONF_IPV6_OTHER] =
				RTE_ETH_RSS_NONFRAG_IPV6_OTHER,
		[IAVF_FILTER_PCTYPE_FRAG_IPV6] = RTE_ETH_RSS_FRAG_IPV6,

		/* L2 Payload */
		[IAVF_FILTER_PCTYPE_L2_PAYLOAD] = RTE_ETH_RSS_L2_PAYLOAD
	};

	const uint64_t ipv4_rss = RTE_ETH_RSS_NONFRAG_IPV4_UDP |
				  RTE_ETH_RSS_NONFRAG_IPV4_TCP |
				  RTE_ETH_RSS_NONFRAG_IPV4_SCTP |
				  RTE_ETH_RSS_NONFRAG_IPV4_OTHER |
				  RTE_ETH_RSS_FRAG_IPV4;

	const uint64_t ipv6_rss = RTE_ETH_RSS_NONFRAG_IPV6_UDP |
				  RTE_ETH_RSS_NONFRAG_IPV6_TCP |
				  RTE_ETH_RSS_NONFRAG_IPV6_SCTP |
				  RTE_ETH_RSS_NONFRAG_IPV6_OTHER |
				  RTE_ETH_RSS_FRAG_IPV6;

	struct iavf_info *vf =  IAVF_DEV_PRIVATE_TO_VF(adapter);
	uint64_t caps = 0, hena = 0, valid_rss_hf = 0;
	uint32_t i;
	int ret;

	ret = iavf_get_hena_caps(adapter, &caps);
	if (ret) {
		/**
		 * RSS offload type configuration is not a necessary feature
		 * for VF, so here just print a warning and return.
		 */
		PMD_DRV_LOG(WARNING,
			    "fail to get RSS offload type caps, ret: %d", ret);
		return;
	}

	/**
	 * RTE_ETH_RSS_IPV4 and RTE_ETH_RSS_IPV6 can be considered as 2
	 * generalizations of all other IPv4 and IPv6 RSS types.
	 */
	if (rss_hf & RTE_ETH_RSS_IPV4)
		rss_hf |= ipv4_rss;

	if (rss_hf & RTE_ETH_RSS_IPV6)
		rss_hf |= ipv6_rss;

	RTE_BUILD_BUG_ON(RTE_DIM(map_hena_rss) > sizeof(uint64_t) * CHAR_BIT);

	for (i = 0; i < RTE_DIM(map_hena_rss); i++) {
		uint64_t bit = BIT_ULL(i);

		if ((caps & bit) && (map_hena_rss[i] & rss_hf)) {
			valid_rss_hf |= map_hena_rss[i];
			hena |= bit;
		}
	}

	ret = iavf_set_hena(adapter, hena);
	if (ret) {
		/**
		 * RSS offload type configuration is not a necessary feature
		 * for VF, so here just print a warning and return.
		 */
		PMD_DRV_LOG(WARNING,
			    "fail to set RSS offload types, ret: %d", ret);
		return;
	}

	if (valid_rss_hf & ipv4_rss)
		valid_rss_hf |= rss_hf & RTE_ETH_RSS_IPV4;

	if (valid_rss_hf & ipv6_rss)
		valid_rss_hf |= rss_hf & RTE_ETH_RSS_IPV6;

	if (rss_hf & ~valid_rss_hf)
		PMD_DRV_LOG(WARNING, "Unsupported rss_hf 0x%" PRIx64,
			    rss_hf & ~valid_rss_hf);

	vf->rss_hf = valid_rss_hf;
}

static int
iavf_init_rss(struct iavf_adapter *adapter)
{
	struct iavf_info *vf =  IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct rte_eth_rss_conf *rss_conf;
	uint16_t i, j, nb_q;
	int ret;

	rss_conf = &adapter->dev_data->dev_conf.rx_adv_conf.rss_conf;
	nb_q = RTE_MIN(adapter->dev_data->nb_rx_queues,
		       vf->max_rss_qregion);

	if (!(vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_RSS_PF)) {
		PMD_DRV_LOG(DEBUG, "RSS is not supported");
		return -ENOTSUP;
	}

	/* configure RSS key */
	if (!rss_conf->rss_key) {
		/* Calculate the default hash key */
		for (i = 0; i < vf->vf_res->rss_key_size; i++)
			vf->rss_key[i] = (uint8_t)rte_rand();
	} else
		rte_memcpy(vf->rss_key, rss_conf->rss_key,
			   RTE_MIN(rss_conf->rss_key_len,
				   vf->vf_res->rss_key_size));

	/* init RSS LUT table */
	for (i = 0, j = 0; i < vf->vf_res->rss_lut_size; i++, j++) {
		if (j >= nb_q)
			j = 0;
		vf->rss_lut[i] = j;
	}
	/* send virtchnl ops to configure RSS */
	ret = iavf_configure_rss_lut(adapter);
	if (ret)
		return ret;
	ret = iavf_configure_rss_key(adapter);
	if (ret)
		return ret;

	if (vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_ADV_RSS_PF) {
		/* Set RSS hash configuration based on rss_conf->rss_hf. */
		ret = iavf_rss_hash_set(adapter, rss_conf->rss_hf, true);
		if (ret) {
			PMD_DRV_LOG(ERR, "fail to set default RSS");
			return ret;
		}
	} else {
		iavf_config_rss_hf(adapter, rss_conf->rss_hf);
	}

	return 0;
}

static int
iavf_queues_req_reset(struct rte_eth_dev *dev, uint16_t num)
{
	struct iavf_adapter *ad =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf =  IAVF_DEV_PRIVATE_TO_VF(ad);
	int ret;

	ret = iavf_request_queues(dev, num);
	if (ret) {
		PMD_DRV_LOG(ERR, "request queues from PF failed");
		return ret;
	}
	PMD_DRV_LOG(INFO, "change queue pairs from %u to %u",
			vf->vsi_res->num_queue_pairs, num);

	ret = iavf_dev_reset(dev);
	if (ret) {
		PMD_DRV_LOG(ERR, "vf reset failed");
		return ret;
	}

	return 0;
}

static int
iavf_dev_vlan_insert_set(struct rte_eth_dev *dev)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	bool enable;

	if (!(vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_VLAN_V2))
		return 0;

	enable = !!(dev->data->dev_conf.txmode.offloads &
		    RTE_ETH_TX_OFFLOAD_VLAN_INSERT);
	iavf_config_vlan_insert_v2(adapter, enable);

	return 0;
}

static int
iavf_dev_init_vlan(struct rte_eth_dev *dev)
{
	int err;

	err = iavf_dev_vlan_offload_set(dev,
					RTE_ETH_VLAN_STRIP_MASK |
					RTE_ETH_QINQ_STRIP_MASK |
					RTE_ETH_VLAN_FILTER_MASK |
					RTE_ETH_VLAN_EXTEND_MASK);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to update vlan offload");
		return err;
	}

	err = iavf_dev_vlan_insert_set(dev);
	if (err)
		PMD_DRV_LOG(ERR, "Failed to update vlan insertion");

	return err;
}

static int
iavf_dev_configure(struct rte_eth_dev *dev)
{
	struct iavf_adapter *ad =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf =  IAVF_DEV_PRIVATE_TO_VF(ad);
	uint16_t num_queue_pairs = RTE_MAX(dev->data->nb_rx_queues,
		dev->data->nb_tx_queues);
	int ret;

	if (ad->closed)
		return -EIO;

	ad->rx_bulk_alloc_allowed = true;
	/* Initialize to TRUE. If any of Rx queues doesn't meet the
	 * vector Rx/Tx preconditions, it will be reset.
	 */
	ad->rx_vec_allowed = true;
	ad->tx_vec_allowed = true;

	if (dev->data->dev_conf.rxmode.mq_mode & RTE_ETH_MQ_RX_RSS_FLAG)
		dev->data->dev_conf.rxmode.offloads |= RTE_ETH_RX_OFFLOAD_RSS_HASH;

	/* Large VF setting */
	if (num_queue_pairs > IAVF_MAX_NUM_QUEUES_DFLT) {
		if (!(vf->vf_res->vf_cap_flags &
				VIRTCHNL_VF_LARGE_NUM_QPAIRS)) {
			PMD_DRV_LOG(ERR, "large VF is not supported");
			return -1;
		}

		if (num_queue_pairs > IAVF_MAX_NUM_QUEUES_LV) {
			PMD_DRV_LOG(ERR, "queue pairs number cannot be larger than %u",
				IAVF_MAX_NUM_QUEUES_LV);
			return -1;
		}

		ret = iavf_queues_req_reset(dev, num_queue_pairs);
		if (ret)
			return ret;

		ret = iavf_get_max_rss_queue_region(ad);
		if (ret) {
			PMD_INIT_LOG(ERR, "get max rss queue region failed");
			return ret;
		}

		vf->lv_enabled = true;
	} else {
		/* Check if large VF is already enabled. If so, disable and
		 * release redundant queue resource.
		 * Or check if enough queue pairs. If not, request them from PF.
		 */
		if (vf->lv_enabled ||
		    num_queue_pairs > vf->vsi_res->num_queue_pairs) {
			ret = iavf_queues_req_reset(dev, num_queue_pairs);
			if (ret)
				return ret;

			vf->lv_enabled = false;
		}
		/* if large VF is not required, use default rss queue region */
		vf->max_rss_qregion = IAVF_MAX_NUM_QUEUES_DFLT;
	}

	ret = iavf_dev_init_vlan(dev);
	if (ret)
		PMD_DRV_LOG(ERR, "configure VLAN failed: %d", ret);

	if (vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_RSS_PF) {
		if (iavf_init_rss(ad) != 0) {
			PMD_DRV_LOG(ERR, "configure rss failed");
			return -1;
		}
	}
	return 0;
}

static int
iavf_init_rxq(struct rte_eth_dev *dev, struct iavf_rx_queue *rxq)
{
	struct iavf_hw *hw = IAVF_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct rte_eth_dev_data *dev_data = dev->data;
	uint16_t buf_size, max_pkt_len;
	uint32_t frame_size = dev->data->mtu + IAVF_ETH_OVERHEAD;
	enum iavf_status err;

	buf_size = rte_pktmbuf_data_room_size(rxq->mp) - RTE_PKTMBUF_HEADROOM;

	/* Calculate the maximum packet length allowed */
	max_pkt_len = RTE_MIN((uint32_t)
			rxq->rx_buf_len * IAVF_MAX_CHAINED_RX_BUFFERS,
			frame_size);

	/* Check if maximum packet length is set correctly.  */
	if (max_pkt_len <= RTE_ETHER_MIN_LEN ||
	    max_pkt_len > IAVF_FRAME_SIZE_MAX) {
		PMD_DRV_LOG(ERR, "maximum packet length must be "
			    "larger than %u and smaller than %u",
			    (uint32_t)IAVF_ETH_MAX_LEN,
			    (uint32_t)IAVF_FRAME_SIZE_MAX);
		return -EINVAL;
	}

	if (rxq->offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP) {
		/* Register mbuf field and flag for Rx timestamp */
		err = rte_mbuf_dyn_rx_timestamp_register(
			&iavf_timestamp_dynfield_offset,
			&iavf_timestamp_dynflag);
		if (err) {
			PMD_DRV_LOG(ERR,
				    "Cannot register mbuf field/flag for timestamp");
			return -EINVAL;
		}
	}

	rxq->max_pkt_len = max_pkt_len;
	if ((dev_data->dev_conf.rxmode.offloads & RTE_ETH_RX_OFFLOAD_SCATTER) ||
	    rxq->max_pkt_len > buf_size) {
		dev_data->scattered_rx = 1;
	}
	IAVF_PCI_REG_WRITE(rxq->qrx_tail, rxq->nb_rx_desc - 1);
	IAVF_WRITE_FLUSH(hw);

	return 0;
}

static int
iavf_init_queues(struct rte_eth_dev *dev)
{
	struct iavf_rx_queue **rxq =
		(struct iavf_rx_queue **)dev->data->rx_queues;
	int i, ret = IAVF_SUCCESS;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		if (!rxq[i] || !rxq[i]->q_set)
			continue;
		ret = iavf_init_rxq(dev, rxq[i]);
		if (ret != IAVF_SUCCESS)
			break;
	}
	/* set rx/tx function to vector/scatter/single-segment
	 * according to parameters
	 */
	iavf_set_rx_function(dev);
	iavf_set_tx_function(dev);

	return ret;
}

static int iavf_config_rx_queues_irqs(struct rte_eth_dev *dev,
				     struct rte_intr_handle *intr_handle)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct iavf_hw *hw = IAVF_DEV_PRIVATE_TO_HW(adapter);
	struct iavf_qv_map *qv_map;
	uint16_t interval, i;
	int vec;

	if (rte_intr_cap_multiple(intr_handle) &&
	    dev->data->dev_conf.intr_conf.rxq) {
		if (rte_intr_efd_enable(intr_handle, dev->data->nb_rx_queues))
			return -1;
	}

	if (rte_intr_dp_is_en(intr_handle)) {
		if (rte_intr_vec_list_alloc(intr_handle, "intr_vec",
						    dev->data->nb_rx_queues)) {
			PMD_DRV_LOG(ERR, "Failed to allocate %d rx intr_vec",
				    dev->data->nb_rx_queues);
			return -1;
		}
	}


	qv_map = rte_zmalloc("qv_map",
		dev->data->nb_rx_queues * sizeof(struct iavf_qv_map), 0);
	if (!qv_map) {
		PMD_DRV_LOG(ERR, "Failed to allocate %d queue-vector map",
				dev->data->nb_rx_queues);
		goto qv_map_alloc_err;
	}

	if (!dev->data->dev_conf.intr_conf.rxq ||
	    !rte_intr_dp_is_en(intr_handle)) {
		/* Rx interrupt disabled, Map interrupt only for writeback */
		vf->nb_msix = 1;
		if (vf->vf_res->vf_cap_flags &
		    VIRTCHNL_VF_OFFLOAD_WB_ON_ITR) {
			/* If WB_ON_ITR supports, enable it */
			vf->msix_base = IAVF_RX_VEC_START;
			/* Set the ITR for index zero, to 2us to make sure that
			 * we leave time for aggregation to occur, but don't
			 * increase latency dramatically.
			 */
			IAVF_WRITE_REG(hw,
				       IAVF_VFINT_DYN_CTLN1(vf->msix_base - 1),
				       (0 << IAVF_VFINT_DYN_CTLN1_ITR_INDX_SHIFT) |
				       IAVF_VFINT_DYN_CTLN1_WB_ON_ITR_MASK |
				       (2UL << IAVF_VFINT_DYN_CTLN1_INTERVAL_SHIFT));
			/* debug - check for success! the return value
			 * should be 2, offset is 0x2800
			 */
			/* IAVF_READ_REG(hw, IAVF_VFINT_ITRN1(0, 0)); */
		} else {
			/* If no WB_ON_ITR offload flags, need to set
			 * interrupt for descriptor write back.
			 */
			vf->msix_base = IAVF_MISC_VEC_ID;

			/* set ITR to default */
			interval = iavf_calc_itr_interval(
					IAVF_QUEUE_ITR_INTERVAL_DEFAULT);
			IAVF_WRITE_REG(hw, IAVF_VFINT_DYN_CTL01,
				       IAVF_VFINT_DYN_CTL01_INTENA_MASK |
				       (IAVF_ITR_INDEX_DEFAULT <<
					IAVF_VFINT_DYN_CTL01_ITR_INDX_SHIFT) |
				       (interval <<
					IAVF_VFINT_DYN_CTL01_INTERVAL_SHIFT));
		}
		IAVF_WRITE_FLUSH(hw);
		/* map all queues to the same interrupt */
		for (i = 0; i < dev->data->nb_rx_queues; i++) {
			qv_map[i].queue_id = i;
			qv_map[i].vector_id = vf->msix_base;
		}
		vf->qv_map = qv_map;
	} else {
		if (!rte_intr_allow_others(intr_handle)) {
			vf->nb_msix = 1;
			vf->msix_base = IAVF_MISC_VEC_ID;
			for (i = 0; i < dev->data->nb_rx_queues; i++) {
				qv_map[i].queue_id = i;
				qv_map[i].vector_id = vf->msix_base;
				rte_intr_vec_list_index_set(intr_handle,
							i, IAVF_MISC_VEC_ID);
			}
			vf->qv_map = qv_map;
			PMD_DRV_LOG(DEBUG,
				    "vector %u are mapping to all Rx queues",
				    vf->msix_base);
		} else {
			/* If Rx interrupt is required, and we can use
			 * multi interrupts, then the vec is from 1
			 */
			vf->nb_msix =
				RTE_MIN(rte_intr_nb_efd_get(intr_handle),
				(uint16_t)(vf->vf_res->max_vectors - 1));
			vf->msix_base = IAVF_RX_VEC_START;
			vec = IAVF_RX_VEC_START;
			for (i = 0; i < dev->data->nb_rx_queues; i++) {
				qv_map[i].queue_id = i;
				qv_map[i].vector_id = vec;
				rte_intr_vec_list_index_set(intr_handle,
								   i, vec++);
				if (vec >= vf->nb_msix + IAVF_RX_VEC_START)
					vec = IAVF_RX_VEC_START;
			}
			vf->qv_map = qv_map;
			PMD_DRV_LOG(DEBUG,
				    "%u vectors are mapping to %u Rx queues",
				    vf->nb_msix, dev->data->nb_rx_queues);
		}
	}

	if (!vf->lv_enabled) {
		if (iavf_config_irq_map(adapter)) {
			PMD_DRV_LOG(ERR, "config interrupt mapping failed");
			goto config_irq_map_err;
		}
	} else {
		uint16_t num_qv_maps = dev->data->nb_rx_queues;
		uint16_t index = 0;

		while (num_qv_maps > IAVF_IRQ_MAP_NUM_PER_BUF) {
			if (iavf_config_irq_map_lv(adapter,
					IAVF_IRQ_MAP_NUM_PER_BUF, index)) {
				PMD_DRV_LOG(ERR, "config interrupt mapping for large VF failed");
				goto config_irq_map_err;
			}
			num_qv_maps -= IAVF_IRQ_MAP_NUM_PER_BUF;
			index += IAVF_IRQ_MAP_NUM_PER_BUF;
		}

		if (iavf_config_irq_map_lv(adapter, num_qv_maps, index)) {
			PMD_DRV_LOG(ERR, "config interrupt mapping for large VF failed");
			goto config_irq_map_err;
		}
	}
	return 0;

config_irq_map_err:
	rte_free(vf->qv_map);
	vf->qv_map = NULL;

qv_map_alloc_err:
	rte_intr_vec_list_free(intr_handle);

	return -1;
}

static int
iavf_start_queues(struct rte_eth_dev *dev)
{
	struct iavf_rx_queue *rxq;
	struct iavf_tx_queue *txq;
	int i;
	uint16_t nb_txq, nb_rxq;

	for (nb_txq = 0; nb_txq < dev->data->nb_tx_queues; nb_txq++) {
		txq = dev->data->tx_queues[nb_txq];
		if (txq->tx_deferred_start)
			continue;
		if (iavf_dev_tx_queue_start(dev, nb_txq) != 0) {
			PMD_DRV_LOG(ERR, "Fail to start tx queue %u", nb_txq);
			goto tx_err;
		}
	}

	for (nb_rxq = 0; nb_rxq < dev->data->nb_rx_queues; nb_rxq++) {
		rxq = dev->data->rx_queues[nb_rxq];
		if (rxq->rx_deferred_start)
			continue;
		if (iavf_dev_rx_queue_start(dev, nb_rxq) != 0) {
			PMD_DRV_LOG(ERR, "Fail to start rx queue %u", nb_rxq);
			goto rx_err;
		}
	}

	return 0;

rx_err:
	for (i = 0; i < nb_rxq; i++)
		iavf_dev_rx_queue_stop(dev, i);
tx_err:
	for (i = 0; i < nb_txq; i++)
		iavf_dev_tx_queue_stop(dev, i);

	return -1;
}

static int
iavf_dev_start(struct rte_eth_dev *dev)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	struct rte_intr_handle *intr_handle = dev->intr_handle;
	uint16_t num_queue_pairs;
	uint16_t index = 0;

	PMD_INIT_FUNC_TRACE();

	if (adapter->closed)
		return -1;

	adapter->stopped = 0;

	vf->max_pkt_len = dev->data->mtu + IAVF_ETH_OVERHEAD;
	vf->num_queue_pairs = RTE_MAX(dev->data->nb_rx_queues,
				      dev->data->nb_tx_queues);
	num_queue_pairs = vf->num_queue_pairs;

	if (vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_QOS)
		if (iavf_get_qos_cap(adapter)) {
			PMD_INIT_LOG(ERR, "Failed to get qos capability");
			return -1;
		}

	if (vf->vf_res->vf_cap_flags & VIRTCHNL_VF_CAP_PTP) {
		if (iavf_get_ptp_cap(adapter)) {
			PMD_INIT_LOG(ERR, "Failed to get ptp capability");
			return -1;
		}
	}

	if (iavf_init_queues(dev) != 0) {
		PMD_DRV_LOG(ERR, "failed to do Queue init");
		return -1;
	}

	if (iavf_set_vf_quanta_size(adapter, index, num_queue_pairs) != 0)
		PMD_DRV_LOG(WARNING, "configure quanta size failed");

	/* If needed, send configure queues msg multiple times to make the
	 * adminq buffer length smaller than the 4K limitation.
	 */
	while (num_queue_pairs > IAVF_CFG_Q_NUM_PER_BUF) {
		if (iavf_configure_queues(adapter,
				IAVF_CFG_Q_NUM_PER_BUF, index) != 0) {
			PMD_DRV_LOG(ERR, "configure queues failed");
			goto err_queue;
		}
		num_queue_pairs -= IAVF_CFG_Q_NUM_PER_BUF;
		index += IAVF_CFG_Q_NUM_PER_BUF;
	}

	if (iavf_configure_queues(adapter, num_queue_pairs, index) != 0) {
		PMD_DRV_LOG(ERR, "configure queues failed");
		goto err_queue;
	}

	if (iavf_config_rx_queues_irqs(dev, intr_handle) != 0) {
		PMD_DRV_LOG(ERR, "configure irq failed");
		goto err_queue;
	}
	/* re-enable intr again, because efd assign may change */
	if (dev->data->dev_conf.intr_conf.rxq != 0) {
		if (vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_WB_ON_ITR)
			rte_intr_disable(intr_handle);
		rte_intr_enable(intr_handle);
	}

	/* Set all mac addrs */
	iavf_add_del_all_mac_addr(adapter, true);

	/* Set all multicast addresses */
	iavf_add_del_mc_addr_list(adapter, vf->mc_addrs, vf->mc_addrs_num,
				  true);

	rte_spinlock_init(&vf->phc_time_aq_lock);

	if (iavf_start_queues(dev) != 0) {
		PMD_DRV_LOG(ERR, "enable queues failed");
		goto err_mac;
	}

	return 0;

err_mac:
	iavf_add_del_all_mac_addr(adapter, false);
err_queue:
	return -1;
}

static int
iavf_dev_stop(struct rte_eth_dev *dev)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct rte_intr_handle *intr_handle = dev->intr_handle;

	PMD_INIT_FUNC_TRACE();

	if (vf->vf_reset)
		return 0;

	if (adapter->closed)
		return -1;

	if (!(vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_WB_ON_ITR) &&
	    dev->data->dev_conf.intr_conf.rxq != 0)
		rte_intr_disable(intr_handle);

	if (adapter->stopped == 1)
		return 0;

	iavf_stop_queues(dev);

	/* Disable the interrupt for Rx */
	rte_intr_efd_disable(intr_handle);
	/* Rx interrupt vector mapping free */
	rte_intr_vec_list_free(intr_handle);

	/* remove all mac addrs */
	iavf_add_del_all_mac_addr(adapter, false);

	/* remove all multicast addresses */
	iavf_add_del_mc_addr_list(adapter, vf->mc_addrs, vf->mc_addrs_num,
				  false);

	adapter->stopped = 1;
	dev->data->dev_started = 0;

	return 0;
}

static int
iavf_dev_info_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = &adapter->vf;

	if (adapter->closed)
		return -EIO;

	dev_info->max_rx_queues = IAVF_MAX_NUM_QUEUES_LV;
	dev_info->max_tx_queues = IAVF_MAX_NUM_QUEUES_LV;
	dev_info->min_rx_bufsize = IAVF_BUF_SIZE_MIN;
	dev_info->max_rx_pktlen = IAVF_FRAME_SIZE_MAX;
	dev_info->max_mtu = dev_info->max_rx_pktlen - IAVF_ETH_OVERHEAD;
	dev_info->min_mtu = RTE_ETHER_MIN_MTU;
	dev_info->hash_key_size = vf->vf_res->rss_key_size;
	dev_info->reta_size = vf->vf_res->rss_lut_size;
	dev_info->flow_type_rss_offloads = IAVF_RSS_OFFLOAD_ALL;
	dev_info->max_mac_addrs = IAVF_NUM_MACADDR_MAX;
	dev_info->dev_capa &= ~RTE_ETH_DEV_CAPA_FLOW_RULE_KEEP;
	dev_info->rx_offload_capa =
		RTE_ETH_RX_OFFLOAD_VLAN_STRIP |
		RTE_ETH_RX_OFFLOAD_QINQ_STRIP |
		RTE_ETH_RX_OFFLOAD_IPV4_CKSUM |
		RTE_ETH_RX_OFFLOAD_UDP_CKSUM |
		RTE_ETH_RX_OFFLOAD_TCP_CKSUM |
		RTE_ETH_RX_OFFLOAD_OUTER_IPV4_CKSUM |
		RTE_ETH_RX_OFFLOAD_SCATTER |
		RTE_ETH_RX_OFFLOAD_VLAN_FILTER |
		RTE_ETH_RX_OFFLOAD_VLAN_EXTEND |
		RTE_ETH_RX_OFFLOAD_RSS_HASH;

	dev_info->tx_offload_capa =
		RTE_ETH_TX_OFFLOAD_VLAN_INSERT |
		RTE_ETH_TX_OFFLOAD_QINQ_INSERT |
		RTE_ETH_TX_OFFLOAD_IPV4_CKSUM |
		RTE_ETH_TX_OFFLOAD_UDP_CKSUM |
		RTE_ETH_TX_OFFLOAD_TCP_CKSUM |
		RTE_ETH_TX_OFFLOAD_SCTP_CKSUM |
		RTE_ETH_TX_OFFLOAD_OUTER_IPV4_CKSUM |
		RTE_ETH_TX_OFFLOAD_OUTER_UDP_CKSUM |
		RTE_ETH_TX_OFFLOAD_TCP_TSO |
		RTE_ETH_TX_OFFLOAD_VXLAN_TNL_TSO |
		RTE_ETH_TX_OFFLOAD_GRE_TNL_TSO |
		RTE_ETH_TX_OFFLOAD_IPIP_TNL_TSO |
		RTE_ETH_TX_OFFLOAD_GENEVE_TNL_TSO |
		RTE_ETH_TX_OFFLOAD_MULTI_SEGS |
		RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;

	if (vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_CRC)
		dev_info->rx_offload_capa |= RTE_ETH_RX_OFFLOAD_KEEP_CRC;

	if (vf->vf_res->vf_cap_flags & VIRTCHNL_VF_CAP_PTP)
		dev_info->rx_offload_capa |= RTE_ETH_RX_OFFLOAD_TIMESTAMP;

	if (iavf_ipsec_crypto_supported(adapter)) {
		dev_info->rx_offload_capa |= RTE_ETH_RX_OFFLOAD_SECURITY;
		dev_info->tx_offload_capa |= RTE_ETH_TX_OFFLOAD_SECURITY;
	}

	dev_info->default_rxconf = (struct rte_eth_rxconf) {
		.rx_free_thresh = IAVF_DEFAULT_RX_FREE_THRESH,
		.rx_drop_en = 0,
		.offloads = 0,
	};

	dev_info->default_txconf = (struct rte_eth_txconf) {
		.tx_free_thresh = IAVF_DEFAULT_TX_FREE_THRESH,
		.tx_rs_thresh = IAVF_DEFAULT_TX_RS_THRESH,
		.offloads = 0,
	};

	dev_info->rx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = IAVF_MAX_RING_DESC,
		.nb_min = IAVF_MIN_RING_DESC,
		.nb_align = IAVF_ALIGN_RING_DESC,
	};

	dev_info->tx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = IAVF_MAX_RING_DESC,
		.nb_min = IAVF_MIN_RING_DESC,
		.nb_align = IAVF_ALIGN_RING_DESC,
	};

	dev_info->err_handle_mode = RTE_ETH_ERROR_HANDLE_MODE_PASSIVE;

	return 0;
}

static const uint32_t *
iavf_dev_supported_ptypes_get(struct rte_eth_dev *dev __rte_unused)
{
	static const uint32_t ptypes[] = {
		RTE_PTYPE_L2_ETHER,
		RTE_PTYPE_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_L4_FRAG,
		RTE_PTYPE_L4_ICMP,
		RTE_PTYPE_L4_NONFRAG,
		RTE_PTYPE_L4_SCTP,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_UNKNOWN
	};
	return ptypes;
}

int
iavf_dev_link_update(struct rte_eth_dev *dev,
		    __rte_unused int wait_to_complete)
{
	struct rte_eth_link new_link;
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);

	memset(&new_link, 0, sizeof(new_link));

	/* Only read status info stored in VF, and the info is updated
	 *  when receive LINK_CHANGE evnet from PF by Virtchnnl.
	 */
	switch (vf->link_speed) {
	case 10:
		new_link.link_speed = RTE_ETH_SPEED_NUM_10M;
		break;
	case 100:
		new_link.link_speed = RTE_ETH_SPEED_NUM_100M;
		break;
	case 1000:
		new_link.link_speed = RTE_ETH_SPEED_NUM_1G;
		break;
	case 10000:
		new_link.link_speed = RTE_ETH_SPEED_NUM_10G;
		break;
	case 20000:
		new_link.link_speed = RTE_ETH_SPEED_NUM_20G;
		break;
	case 25000:
		new_link.link_speed = RTE_ETH_SPEED_NUM_25G;
		break;
	case 40000:
		new_link.link_speed = RTE_ETH_SPEED_NUM_40G;
		break;
	case 50000:
		new_link.link_speed = RTE_ETH_SPEED_NUM_50G;
		break;
	case 100000:
		new_link.link_speed = RTE_ETH_SPEED_NUM_100G;
		break;
	default:
		new_link.link_speed = RTE_ETH_SPEED_NUM_NONE;
		break;
	}

	new_link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
	new_link.link_status = vf->link_up ? RTE_ETH_LINK_UP :
					     RTE_ETH_LINK_DOWN;
	new_link.link_autoneg = !(dev->data->dev_conf.link_speeds &
				RTE_ETH_LINK_SPEED_FIXED);

	return rte_eth_linkstatus_set(dev, &new_link);
}

static int
iavf_dev_promiscuous_enable(struct rte_eth_dev *dev)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);

	return iavf_config_promisc(adapter,
				  true, vf->promisc_multicast_enabled);
}

static int
iavf_dev_promiscuous_disable(struct rte_eth_dev *dev)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);

	return iavf_config_promisc(adapter,
				  false, vf->promisc_multicast_enabled);
}

static int
iavf_dev_allmulticast_enable(struct rte_eth_dev *dev)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);

	return iavf_config_promisc(adapter,
				  vf->promisc_unicast_enabled, true);
}

static int
iavf_dev_allmulticast_disable(struct rte_eth_dev *dev)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);

	return iavf_config_promisc(adapter,
				  vf->promisc_unicast_enabled, false);
}

static int
iavf_dev_add_mac_addr(struct rte_eth_dev *dev, struct rte_ether_addr *addr,
		     __rte_unused uint32_t index,
		     __rte_unused uint32_t pool)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	int err;

	if (rte_is_zero_ether_addr(addr)) {
		PMD_DRV_LOG(ERR, "Invalid Ethernet Address");
		return -EINVAL;
	}

	err = iavf_add_del_eth_addr(adapter, addr, true, VIRTCHNL_ETHER_ADDR_EXTRA);
	if (err) {
		PMD_DRV_LOG(ERR, "fail to add MAC address");
		return -EIO;
	}

	vf->mac_num++;

	return 0;
}

static void
iavf_dev_del_mac_addr(struct rte_eth_dev *dev, uint32_t index)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct rte_ether_addr *addr;
	int err;

	addr = &dev->data->mac_addrs[index];

	err = iavf_add_del_eth_addr(adapter, addr, false, VIRTCHNL_ETHER_ADDR_EXTRA);
	if (err)
		PMD_DRV_LOG(ERR, "fail to delete MAC address");

	vf->mac_num--;
}

static int
iavf_dev_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vlan_id, int on)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	int err;

	if (adapter->closed)
		return -EIO;

	if (vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_VLAN_V2) {
		err = iavf_add_del_vlan_v2(adapter, vlan_id, on);
		if (err)
			return -EIO;
		return 0;
	}

	if (!(vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_VLAN))
		return -ENOTSUP;

	err = iavf_add_del_vlan(adapter, vlan_id, on);
	if (err)
		return -EIO;
	return 0;
}

static void
iavf_iterate_vlan_filters_v2(struct rte_eth_dev *dev, bool enable)
{
	struct rte_vlan_filter_conf *vfc = &dev->data->vlan_filter_conf;
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	uint32_t i, j;
	uint64_t ids;

	for (i = 0; i < RTE_DIM(vfc->ids); i++) {
		if (vfc->ids[i] == 0)
			continue;

		ids = vfc->ids[i];
		for (j = 0; ids != 0 && j < 64; j++, ids >>= 1) {
			if (ids & 1)
				iavf_add_del_vlan_v2(adapter,
						     64 * i + j, enable);
		}
	}
}

static int
iavf_dev_vlan_offload_set_v2(struct rte_eth_dev *dev, int mask)
{
	struct rte_eth_rxmode *rxmode = &dev->data->dev_conf.rxmode;
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	bool enable;
	int err;

	if (mask & RTE_ETH_VLAN_FILTER_MASK) {
		enable = !!(rxmode->offloads & RTE_ETH_RX_OFFLOAD_VLAN_FILTER);

		iavf_iterate_vlan_filters_v2(dev, enable);
	}

	if (mask & RTE_ETH_VLAN_STRIP_MASK) {
		enable = !!(rxmode->offloads & RTE_ETH_RX_OFFLOAD_VLAN_STRIP);

		err = iavf_config_vlan_strip_v2(adapter, enable);
		/* If not support, the stripping is already disabled by PF */
		if (err == -ENOTSUP && !enable)
			err = 0;
		if (err)
			return -EIO;
	}

	return 0;
}

static int
iavf_dev_vlan_offload_set(struct rte_eth_dev *dev, int mask)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct rte_eth_conf *dev_conf = &dev->data->dev_conf;
	int err;

	if (adapter->closed)
		return -EIO;

	if (vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_VLAN_V2)
		return iavf_dev_vlan_offload_set_v2(dev, mask);

	if (!(vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_VLAN))
		return -ENOTSUP;

	/* Vlan stripping setting */
	if (mask & RTE_ETH_VLAN_STRIP_MASK) {
		/* Enable or disable VLAN stripping */
		if (dev_conf->rxmode.offloads & RTE_ETH_RX_OFFLOAD_VLAN_STRIP)
			err = iavf_enable_vlan_strip(adapter);
		else
			err = iavf_disable_vlan_strip(adapter);

		if (err)
			return -EIO;
	}
	return 0;
}

static int
iavf_dev_rss_reta_update(struct rte_eth_dev *dev,
			struct rte_eth_rss_reta_entry64 *reta_conf,
			uint16_t reta_size)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	uint8_t *lut;
	uint16_t i, idx, shift;
	int ret;

	if (adapter->closed)
		return -EIO;

	if (!(vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_RSS_PF))
		return -ENOTSUP;

	if (reta_size != vf->vf_res->rss_lut_size) {
		PMD_DRV_LOG(ERR, "The size of hash lookup table configured "
			"(%d) doesn't match the number of hardware can "
			"support (%d)", reta_size, vf->vf_res->rss_lut_size);
		return -EINVAL;
	}

	lut = rte_zmalloc("rss_lut", reta_size, 0);
	if (!lut) {
		PMD_DRV_LOG(ERR, "No memory can be allocated");
		return -ENOMEM;
	}
	/* store the old lut table temporarily */
	rte_memcpy(lut, vf->rss_lut, reta_size);

	for (i = 0; i < reta_size; i++) {
		idx = i / RTE_ETH_RETA_GROUP_SIZE;
		shift = i % RTE_ETH_RETA_GROUP_SIZE;
		if (reta_conf[idx].mask & (1ULL << shift))
			lut[i] = reta_conf[idx].reta[shift];
	}

	rte_memcpy(vf->rss_lut, lut, reta_size);
	/* send virtchnl ops to configure RSS */
	ret = iavf_configure_rss_lut(adapter);
	if (ret) /* revert back */
		rte_memcpy(vf->rss_lut, lut, reta_size);
	rte_free(lut);

	return ret;
}

static int
iavf_dev_rss_reta_query(struct rte_eth_dev *dev,
		       struct rte_eth_rss_reta_entry64 *reta_conf,
		       uint16_t reta_size)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	uint16_t i, idx, shift;

	if (adapter->closed)
		return -EIO;

	if (!(vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_RSS_PF))
		return -ENOTSUP;

	if (reta_size != vf->vf_res->rss_lut_size) {
		PMD_DRV_LOG(ERR, "The size of hash lookup table configured "
			"(%d) doesn't match the number of hardware can "
			"support (%d)", reta_size, vf->vf_res->rss_lut_size);
		return -EINVAL;
	}

	for (i = 0; i < reta_size; i++) {
		idx = i / RTE_ETH_RETA_GROUP_SIZE;
		shift = i % RTE_ETH_RETA_GROUP_SIZE;
		if (reta_conf[idx].mask & (1ULL << shift))
			reta_conf[idx].reta[shift] = vf->rss_lut[i];
	}

	return 0;
}

static int
iavf_set_rss_key(struct iavf_adapter *adapter, uint8_t *key, uint8_t key_len)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);

	/* HENA setting, it is enabled by default, no change */
	if (!key || key_len == 0) {
		PMD_DRV_LOG(DEBUG, "No key to be configured");
		return 0;
	} else if (key_len != vf->vf_res->rss_key_size) {
		PMD_DRV_LOG(ERR, "The size of hash key configured "
			"(%d) doesn't match the size of hardware can "
			"support (%d)", key_len,
			vf->vf_res->rss_key_size);
		return -EINVAL;
	}

	rte_memcpy(vf->rss_key, key, key_len);

	return iavf_configure_rss_key(adapter);
}

static int
iavf_dev_rss_hash_update(struct rte_eth_dev *dev,
			struct rte_eth_rss_conf *rss_conf)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	int ret;

	adapter->dev_data->dev_conf.rx_adv_conf.rss_conf = *rss_conf;

	if (adapter->closed)
		return -EIO;

	if (!(vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_RSS_PF))
		return -ENOTSUP;

	/* Set hash key. */
	ret = iavf_set_rss_key(adapter, rss_conf->rss_key,
			       rss_conf->rss_key_len);
	if (ret)
		return ret;

	if (rss_conf->rss_hf == 0) {
		vf->rss_hf = 0;
		ret = iavf_set_hena(adapter, 0);

		/* It is a workaround, temporarily allow error to be returned
		 * due to possible lack of PF handling for hena = 0.
		 */
		if (ret)
			PMD_DRV_LOG(WARNING, "fail to clean existing RSS, lack PF support");
		return 0;
	}

	if (vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_ADV_RSS_PF) {
		/* Clear existing RSS. */
		ret = iavf_set_hena(adapter, 0);

		/* It is a workaround, temporarily allow error to be returned
		 * due to possible lack of PF handling for hena = 0.
		 */
		if (ret)
			PMD_DRV_LOG(WARNING, "fail to clean existing RSS,"
				    "lack PF support");

		/* Set new RSS configuration. */
		ret = iavf_rss_hash_set(adapter, rss_conf->rss_hf, true);
		if (ret) {
			PMD_DRV_LOG(ERR, "fail to set new RSS");
			return ret;
		}
	} else {
		iavf_config_rss_hf(adapter, rss_conf->rss_hf);
	}

	return 0;
}

static int
iavf_dev_rss_hash_conf_get(struct rte_eth_dev *dev,
			  struct rte_eth_rss_conf *rss_conf)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);

	if (adapter->closed)
		return -EIO;

	if (!(vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_RSS_PF))
		return -ENOTSUP;

	rss_conf->rss_hf = vf->rss_hf;

	if (!rss_conf->rss_key)
		return 0;

	rss_conf->rss_key_len = vf->vf_res->rss_key_size;
	rte_memcpy(rss_conf->rss_key, vf->rss_key, rss_conf->rss_key_len);

	return 0;
}

static int
iavf_dev_mtu_set(struct rte_eth_dev *dev, uint16_t mtu __rte_unused)
{
	/* mtu setting is forbidden if port is start */
	if (dev->data->dev_started) {
		PMD_DRV_LOG(ERR, "port must be stopped before configuration");
		return -EBUSY;
	}

	return 0;
}

static int
iavf_dev_set_default_mac_addr(struct rte_eth_dev *dev,
			     struct rte_ether_addr *mac_addr)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_hw *hw = IAVF_DEV_PRIVATE_TO_HW(adapter);
	struct rte_ether_addr *old_addr;
	int ret;

	old_addr = (struct rte_ether_addr *)hw->mac.addr;

	if (rte_is_same_ether_addr(old_addr, mac_addr))
		return 0;

	ret = iavf_add_del_eth_addr(adapter, old_addr, false, VIRTCHNL_ETHER_ADDR_PRIMARY);
	if (ret)
		PMD_DRV_LOG(ERR, "Fail to delete old MAC:"
			    RTE_ETHER_ADDR_PRT_FMT,
				RTE_ETHER_ADDR_BYTES(old_addr));

	ret = iavf_add_del_eth_addr(adapter, mac_addr, true, VIRTCHNL_ETHER_ADDR_PRIMARY);
	if (ret)
		PMD_DRV_LOG(ERR, "Fail to add new MAC:"
			    RTE_ETHER_ADDR_PRT_FMT,
				RTE_ETHER_ADDR_BYTES(mac_addr));

	if (ret)
		return -EIO;

	rte_ether_addr_copy(mac_addr, (struct rte_ether_addr *)hw->mac.addr);
	return 0;
}

static void
iavf_stat_update_48(uint64_t *offset, uint64_t *stat)
{
	if (*stat >= *offset)
		*stat = *stat - *offset;
	else
		*stat = (uint64_t)((*stat +
			((uint64_t)1 << IAVF_48_BIT_WIDTH)) - *offset);

	*stat &= IAVF_48_BIT_MASK;
}

static void
iavf_stat_update_32(uint64_t *offset, uint64_t *stat)
{
	if (*stat >= *offset)
		*stat = (uint64_t)(*stat - *offset);
	else
		*stat = (uint64_t)((*stat +
			((uint64_t)1 << IAVF_32_BIT_WIDTH)) - *offset);
}

static void
iavf_update_stats(struct iavf_vsi *vsi, struct virtchnl_eth_stats *nes)
{
	struct virtchnl_eth_stats *oes = &vsi->eth_stats_offset.eth_stats;

	iavf_stat_update_48(&oes->rx_bytes, &nes->rx_bytes);
	iavf_stat_update_48(&oes->rx_unicast, &nes->rx_unicast);
	iavf_stat_update_48(&oes->rx_multicast, &nes->rx_multicast);
	iavf_stat_update_48(&oes->rx_broadcast, &nes->rx_broadcast);
	iavf_stat_update_32(&oes->rx_discards, &nes->rx_discards);
	iavf_stat_update_48(&oes->tx_bytes, &nes->tx_bytes);
	iavf_stat_update_48(&oes->tx_unicast, &nes->tx_unicast);
	iavf_stat_update_48(&oes->tx_multicast, &nes->tx_multicast);
	iavf_stat_update_48(&oes->tx_broadcast, &nes->tx_broadcast);
	iavf_stat_update_32(&oes->tx_errors, &nes->tx_errors);
	iavf_stat_update_32(&oes->tx_discards, &nes->tx_discards);
}

static int
iavf_dev_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	struct iavf_vsi *vsi = &vf->vsi;
	struct virtchnl_eth_stats *pstats = NULL;
	int ret;

	ret = iavf_query_stats(adapter, &pstats);
	if (ret == 0) {
		uint8_t crc_stats_len = (dev->data->dev_conf.rxmode.offloads &
					 RTE_ETH_RX_OFFLOAD_KEEP_CRC) ? 0 :
					 RTE_ETHER_CRC_LEN;
		iavf_update_stats(vsi, pstats);
		stats->ipackets = pstats->rx_unicast + pstats->rx_multicast +
				pstats->rx_broadcast - pstats->rx_discards;
		stats->opackets = pstats->tx_broadcast + pstats->tx_multicast +
						pstats->tx_unicast;
		stats->imissed = pstats->rx_discards;
		stats->oerrors = pstats->tx_errors + pstats->tx_discards;
		stats->ibytes = pstats->rx_bytes;
		stats->ibytes -= stats->ipackets * crc_stats_len;
		stats->obytes = pstats->tx_bytes;
	} else {
		PMD_DRV_LOG(ERR, "Get statistics failed");
	}
	return ret;
}

static int
iavf_dev_stats_reset(struct rte_eth_dev *dev)
{
	int ret;
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	struct iavf_vsi *vsi = &vf->vsi;
	struct virtchnl_eth_stats *pstats = NULL;

	/* read stat values to clear hardware registers */
	ret = iavf_query_stats(adapter, &pstats);
	if (ret != 0)
		return ret;

	/* set stats offset base on current values */
	vsi->eth_stats_offset.eth_stats = *pstats;

	return 0;
}

static int
iavf_dev_xstats_reset(struct rte_eth_dev *dev)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	iavf_dev_stats_reset(dev);
	memset(&vf->vsi.eth_stats_offset.ips_stats, 0,
			sizeof(struct iavf_ipsec_crypto_stats));
	return 0;
}

static int iavf_dev_xstats_get_names(__rte_unused struct rte_eth_dev *dev,
				      struct rte_eth_xstat_name *xstats_names,
				      __rte_unused unsigned int limit)
{
	unsigned int i;

	if (xstats_names != NULL)
		for (i = 0; i < IAVF_NB_XSTATS; i++) {
			snprintf(xstats_names[i].name,
				sizeof(xstats_names[i].name),
				"%s", rte_iavf_stats_strings[i].name);
		}
	return IAVF_NB_XSTATS;
}

static void
iavf_dev_update_ipsec_xstats(struct rte_eth_dev *ethdev,
		struct iavf_ipsec_crypto_stats *ips)
{
	uint16_t idx;
	for (idx = 0; idx < ethdev->data->nb_rx_queues; idx++) {
		struct iavf_rx_queue *rxq;
		struct iavf_ipsec_crypto_stats *stats;
		rxq = (struct iavf_rx_queue *)ethdev->data->rx_queues[idx];
		stats = &rxq->stats.ipsec_crypto;
		ips->icount += stats->icount;
		ips->ibytes += stats->ibytes;
		ips->ierrors.count += stats->ierrors.count;
		ips->ierrors.sad_miss += stats->ierrors.sad_miss;
		ips->ierrors.not_processed += stats->ierrors.not_processed;
		ips->ierrors.icv_check += stats->ierrors.icv_check;
		ips->ierrors.ipsec_length += stats->ierrors.ipsec_length;
		ips->ierrors.misc += stats->ierrors.misc;
	}
}

static int iavf_dev_xstats_get(struct rte_eth_dev *dev,
				 struct rte_eth_xstat *xstats, unsigned int n)
{
	int ret;
	unsigned int i;
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	struct iavf_vsi *vsi = &vf->vsi;
	struct virtchnl_eth_stats *pstats = NULL;
	struct iavf_eth_xstats iavf_xtats = {{0}};

	if (n < IAVF_NB_XSTATS)
		return IAVF_NB_XSTATS;

	ret = iavf_query_stats(adapter, &pstats);
	if (ret != 0)
		return 0;

	if (!xstats)
		return 0;

	iavf_update_stats(vsi, pstats);
	iavf_xtats.eth_stats = *pstats;

	if (iavf_ipsec_crypto_supported(adapter))
		iavf_dev_update_ipsec_xstats(dev, &iavf_xtats.ips_stats);

	/* loop over xstats array and values from pstats */
	for (i = 0; i < IAVF_NB_XSTATS; i++) {
		xstats[i].id = i;
		xstats[i].value = *(uint64_t *)(((char *)&iavf_xtats) +
			rte_iavf_stats_strings[i].offset);
	}

	return IAVF_NB_XSTATS;
}


static int
iavf_dev_rx_queue_intr_enable(struct rte_eth_dev *dev, uint16_t queue_id)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct iavf_hw *hw = IAVF_DEV_PRIVATE_TO_HW(adapter);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	uint16_t msix_intr;

	if (adapter->closed)
		return -EIO;

	msix_intr = rte_intr_vec_list_index_get(pci_dev->intr_handle,
						       queue_id);
	if (msix_intr == IAVF_MISC_VEC_ID) {
		PMD_DRV_LOG(INFO, "MISC is also enabled for control");
		IAVF_WRITE_REG(hw, IAVF_VFINT_DYN_CTL01,
			       IAVF_VFINT_DYN_CTL01_INTENA_MASK |
			       IAVF_VFINT_DYN_CTL01_CLEARPBA_MASK |
			       IAVF_VFINT_DYN_CTL01_ITR_INDX_MASK);
	} else {
		IAVF_WRITE_REG(hw,
			       IAVF_VFINT_DYN_CTLN1
				(msix_intr - IAVF_RX_VEC_START),
			       IAVF_VFINT_DYN_CTLN1_INTENA_MASK |
			       IAVF_VFINT_DYN_CTL01_CLEARPBA_MASK |
			       IAVF_VFINT_DYN_CTLN1_ITR_INDX_MASK);
	}

	IAVF_WRITE_FLUSH(hw);

	if (vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_WB_ON_ITR)
		rte_intr_ack(pci_dev->intr_handle);

	return 0;
}

static int
iavf_dev_rx_queue_intr_disable(struct rte_eth_dev *dev, uint16_t queue_id)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct iavf_hw *hw = IAVF_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint16_t msix_intr;

	msix_intr = rte_intr_vec_list_index_get(pci_dev->intr_handle,
						       queue_id);
	if (msix_intr == IAVF_MISC_VEC_ID) {
		PMD_DRV_LOG(ERR, "MISC is used for control, cannot disable it");
		return -EIO;
	}

	IAVF_WRITE_REG(hw,
		      IAVF_VFINT_DYN_CTLN1(msix_intr - IAVF_RX_VEC_START),
		      IAVF_VFINT_DYN_CTLN1_WB_ON_ITR_MASK);

	IAVF_WRITE_FLUSH(hw);
	return 0;
}

static int
iavf_check_vf_reset_done(struct iavf_hw *hw)
{
	int i, reset;

	for (i = 0; i < IAVF_RESET_WAIT_CNT; i++) {
		reset = IAVF_READ_REG(hw, IAVF_VFGEN_RSTAT) &
			IAVF_VFGEN_RSTAT_VFR_STATE_MASK;
		reset = reset >> IAVF_VFGEN_RSTAT_VFR_STATE_SHIFT;
		if (reset == VIRTCHNL_VFR_VFACTIVE ||
		    reset == VIRTCHNL_VFR_COMPLETED)
			break;
		rte_delay_ms(20);
	}

	if (i >= IAVF_RESET_WAIT_CNT)
		return -1;

	return 0;
}

static int
iavf_lookup_proto_xtr_type(const char *flex_name)
{
	static struct {
		const char *name;
		enum iavf_proto_xtr_type type;
	} xtr_type_map[] = {
		{ "vlan",      IAVF_PROTO_XTR_VLAN      },
		{ "ipv4",      IAVF_PROTO_XTR_IPV4      },
		{ "ipv6",      IAVF_PROTO_XTR_IPV6      },
		{ "ipv6_flow", IAVF_PROTO_XTR_IPV6_FLOW },
		{ "tcp",       IAVF_PROTO_XTR_TCP       },
		{ "ip_offset", IAVF_PROTO_XTR_IP_OFFSET },
		{ "ipsec_crypto_said", IAVF_PROTO_XTR_IPSEC_CRYPTO_SAID },
	};
	uint32_t i;

	for (i = 0; i < RTE_DIM(xtr_type_map); i++) {
		if (strcmp(flex_name, xtr_type_map[i].name) == 0)
			return xtr_type_map[i].type;
	}

	PMD_DRV_LOG(ERR, "wrong proto_xtr type, it should be: "
			"vlan|ipv4|ipv6|ipv6_flow|tcp|ip_offset|ipsec_crypto_said");

	return -1;
}

/**
 * Parse elem, the elem could be single number/range or '(' ')' group
 * 1) A single number elem, it's just a simple digit. e.g. 9
 * 2) A single range elem, two digits with a '-' between. e.g. 2-6
 * 3) A group elem, combines multiple 1) or 2) with '( )'. e.g (0,2-4,6)
 *    Within group elem, '-' used for a range separator;
 *                       ',' used for a single number.
 */
static int
iavf_parse_queue_set(const char *input, int xtr_type,
		     struct iavf_devargs *devargs)
{
	const char *str = input;
	char *end = NULL;
	uint32_t min, max;
	uint32_t idx;

	while (isblank(*str))
		str++;

	if (!isdigit(*str) && *str != '(')
		return -1;

	/* process single number or single range of number */
	if (*str != '(') {
		errno = 0;
		idx = strtoul(str, &end, 10);
		if (errno || !end || idx >= IAVF_MAX_QUEUE_NUM)
			return -1;

		while (isblank(*end))
			end++;

		min = idx;
		max = idx;

		/* process single <number>-<number> */
		if (*end == '-') {
			end++;
			while (isblank(*end))
				end++;
			if (!isdigit(*end))
				return -1;

			errno = 0;
			idx = strtoul(end, &end, 10);
			if (errno || !end || idx >= IAVF_MAX_QUEUE_NUM)
				return -1;

			max = idx;
			while (isblank(*end))
				end++;
		}

		if (*end != ':')
			return -1;

		for (idx = RTE_MIN(min, max);
		     idx <= RTE_MAX(min, max); idx++)
			devargs->proto_xtr[idx] = xtr_type;

		return 0;
	}

	/* process set within bracket */
	str++;
	while (isblank(*str))
		str++;
	if (*str == '\0')
		return -1;

	min = IAVF_MAX_QUEUE_NUM;
	do {
		/* go ahead to the first digit */
		while (isblank(*str))
			str++;
		if (!isdigit(*str))
			return -1;

		/* get the digit value */
		errno = 0;
		idx = strtoul(str, &end, 10);
		if (errno || !end || idx >= IAVF_MAX_QUEUE_NUM)
			return -1;

		/* go ahead to separator '-',',' and ')' */
		while (isblank(*end))
			end++;
		if (*end == '-') {
			if (min == IAVF_MAX_QUEUE_NUM)
				min = idx;
			else /* avoid continuous '-' */
				return -1;
		} else if (*end == ',' || *end == ')') {
			max = idx;
			if (min == IAVF_MAX_QUEUE_NUM)
				min = idx;

			for (idx = RTE_MIN(min, max);
			     idx <= RTE_MAX(min, max); idx++)
				devargs->proto_xtr[idx] = xtr_type;

			min = IAVF_MAX_QUEUE_NUM;
		} else {
			return -1;
		}

		str = end + 1;
	} while (*end != ')' && *end != '\0');

	return 0;
}

static int
iavf_parse_queue_proto_xtr(const char *queues, struct iavf_devargs *devargs)
{
	const char *queue_start;
	uint32_t idx;
	int xtr_type;
	char flex_name[32];

	while (isblank(*queues))
		queues++;

	if (*queues != '[') {
		xtr_type = iavf_lookup_proto_xtr_type(queues);
		if (xtr_type < 0)
			return -1;

		devargs->proto_xtr_dflt = xtr_type;

		return 0;
	}

	queues++;
	do {
		while (isblank(*queues))
			queues++;
		if (*queues == '\0')
			return -1;

		queue_start = queues;

		/* go across a complete bracket */
		if (*queue_start == '(') {
			queues += strcspn(queues, ")");
			if (*queues != ')')
				return -1;
		}

		/* scan the separator ':' */
		queues += strcspn(queues, ":");
		if (*queues++ != ':')
			return -1;
		while (isblank(*queues))
			queues++;

		for (idx = 0; ; idx++) {
			if (isblank(queues[idx]) ||
			    queues[idx] == ',' ||
			    queues[idx] == ']' ||
			    queues[idx] == '\0')
				break;

			if (idx > sizeof(flex_name) - 2)
				return -1;

			flex_name[idx] = queues[idx];
		}
		flex_name[idx] = '\0';
		xtr_type = iavf_lookup_proto_xtr_type(flex_name);
		if (xtr_type < 0)
			return -1;

		queues += idx;

		while (isblank(*queues) || *queues == ',' || *queues == ']')
			queues++;

		if (iavf_parse_queue_set(queue_start, xtr_type, devargs) < 0)
			return -1;
	} while (*queues != '\0');

	return 0;
}

static int
iavf_handle_proto_xtr_arg(__rte_unused const char *key, const char *value,
			  void *extra_args)
{
	struct iavf_devargs *devargs = extra_args;

	if (!value || !extra_args)
		return -EINVAL;

	if (iavf_parse_queue_proto_xtr(value, devargs) < 0) {
		PMD_DRV_LOG(ERR, "the proto_xtr's parameter is wrong : '%s'",
			    value);
		return -1;
	}

	return 0;
}

static int
parse_u16(__rte_unused const char *key, const char *value, void *args)
{
	u16 *num = (u16 *)args;
	u16 tmp;

	errno = 0;
	tmp = strtoull(value, NULL, 10);
	if (errno || !tmp) {
		PMD_DRV_LOG(WARNING, "%s: \"%s\" is not a valid u16",
			    key, value);
		return -1;
	}

	*num = tmp;

	return 0;
}

static int iavf_parse_devargs(struct rte_eth_dev *dev)
{
	struct iavf_adapter *ad =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct rte_devargs *devargs = dev->device->devargs;
	struct rte_kvargs *kvlist;
	int ret;

	if (!devargs)
		return 0;

	kvlist = rte_kvargs_parse(devargs->args, iavf_valid_args);
	if (!kvlist) {
		PMD_INIT_LOG(ERR, "invalid kvargs key\n");
		return -EINVAL;
	}

	ad->devargs.proto_xtr_dflt = IAVF_PROTO_XTR_NONE;
	memset(ad->devargs.proto_xtr, IAVF_PROTO_XTR_NONE,
	       sizeof(ad->devargs.proto_xtr));

	ret = rte_kvargs_process(kvlist, IAVF_PROTO_XTR_ARG,
				 &iavf_handle_proto_xtr_arg, &ad->devargs);
	if (ret)
		goto bail;

	ret = rte_kvargs_process(kvlist, IAVF_QUANTA_SIZE_ARG,
				 &parse_u16, &ad->devargs.quanta_size);
	if (ret)
		goto bail;

	if (ad->devargs.quanta_size != 0 &&
	    (ad->devargs.quanta_size < 256 || ad->devargs.quanta_size > 4096 ||
	     ad->devargs.quanta_size & 0x40)) {
		PMD_INIT_LOG(ERR, "invalid quanta size\n");
		ret = -EINVAL;
		goto bail;
	}

bail:
	rte_kvargs_free(kvlist);
	return ret;
}

static void
iavf_init_proto_xtr(struct rte_eth_dev *dev)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	struct iavf_adapter *ad =
			IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	const struct iavf_proto_xtr_ol *xtr_ol;
	bool proto_xtr_enable = false;
	int offset;
	uint16_t i;

	vf->proto_xtr = rte_zmalloc("vf proto xtr",
				    vf->vsi_res->num_queue_pairs, 0);
	if (unlikely(!(vf->proto_xtr))) {
		PMD_DRV_LOG(ERR, "no memory for setting up proto_xtr's table");
		return;
	}

	for (i = 0; i < vf->vsi_res->num_queue_pairs; i++) {
		vf->proto_xtr[i] = ad->devargs.proto_xtr[i] !=
					IAVF_PROTO_XTR_NONE ?
					ad->devargs.proto_xtr[i] :
					ad->devargs.proto_xtr_dflt;

		if (vf->proto_xtr[i] != IAVF_PROTO_XTR_NONE) {
			uint8_t type = vf->proto_xtr[i];

			iavf_proto_xtr_params[type].required = true;
			proto_xtr_enable = true;
		}
	}

	if (likely(!proto_xtr_enable))
		return;

	offset = rte_mbuf_dynfield_register(&iavf_proto_xtr_metadata_param);
	if (unlikely(offset == -1)) {
		PMD_DRV_LOG(ERR,
			    "failed to extract protocol metadata, error %d",
			    -rte_errno);
		return;
	}

	PMD_DRV_LOG(DEBUG,
		    "proto_xtr metadata offset in mbuf is : %d",
		    offset);
	rte_pmd_ifd_dynfield_proto_xtr_metadata_offs = offset;

	for (i = 0; i < RTE_DIM(iavf_proto_xtr_params); i++) {
		xtr_ol = &iavf_proto_xtr_params[i];

		uint8_t rxdid = iavf_proto_xtr_type_to_rxdid((uint8_t)i);

		if (!xtr_ol->required)
			continue;

		if (!(vf->supported_rxdid & BIT(rxdid))) {
			PMD_DRV_LOG(ERR,
				    "rxdid[%u] is not supported in hardware",
				    rxdid);
			rte_pmd_ifd_dynfield_proto_xtr_metadata_offs = -1;
			break;
		}

		offset = rte_mbuf_dynflag_register(&xtr_ol->param);
		if (unlikely(offset == -1)) {
			PMD_DRV_LOG(ERR,
				    "failed to register proto_xtr offload '%s', error %d",
				    xtr_ol->param.name, -rte_errno);

			rte_pmd_ifd_dynfield_proto_xtr_metadata_offs = -1;
			break;
		}

		PMD_DRV_LOG(DEBUG,
			    "proto_xtr offload '%s' offset in mbuf is : %d",
			    xtr_ol->param.name, offset);
		*xtr_ol->ol_flag = 1ULL << offset;
	}
}

static int
iavf_init_vf(struct rte_eth_dev *dev)
{
	int err, bufsz;
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_hw *hw = IAVF_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);

	vf->eth_dev = dev;

	err = iavf_parse_devargs(dev);
	if (err) {
		PMD_INIT_LOG(ERR, "Failed to parse devargs");
		goto err;
	}

	err = iavf_set_mac_type(hw);
	if (err) {
		PMD_INIT_LOG(ERR, "set_mac_type failed: %d", err);
		goto err;
	}

	err = iavf_check_vf_reset_done(hw);
	if (err) {
		PMD_INIT_LOG(ERR, "VF is still resetting");
		goto err;
	}

	iavf_init_adminq_parameter(hw);
	err = iavf_init_adminq(hw);
	if (err) {
		PMD_INIT_LOG(ERR, "init_adminq failed: %d", err);
		goto err;
	}

	vf->aq_resp = rte_zmalloc("vf_aq_resp", IAVF_AQ_BUF_SZ, 0);
	if (!vf->aq_resp) {
		PMD_INIT_LOG(ERR, "unable to allocate vf_aq_resp memory");
		goto err_aq;
	}
	if (iavf_check_api_version(adapter) != 0) {
		PMD_INIT_LOG(ERR, "check_api version failed");
		goto err_api;
	}

	bufsz = sizeof(struct virtchnl_vf_resource) +
		(IAVF_MAX_VF_VSI * sizeof(struct virtchnl_vsi_resource));
	vf->vf_res = rte_zmalloc("vf_res", bufsz, 0);
	if (!vf->vf_res) {
		PMD_INIT_LOG(ERR, "unable to allocate vf_res memory");
		goto err_api;
	}

	if (iavf_get_vf_resource(adapter) != 0) {
		PMD_INIT_LOG(ERR, "iavf_get_vf_config failed");
		goto err_alloc;
	}
	/* Allocate memort for RSS info */
	if (vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_RSS_PF) {
		vf->rss_key = rte_zmalloc("rss_key",
					  vf->vf_res->rss_key_size, 0);
		if (!vf->rss_key) {
			PMD_INIT_LOG(ERR, "unable to allocate rss_key memory");
			goto err_rss;
		}
		vf->rss_lut = rte_zmalloc("rss_lut",
					  vf->vf_res->rss_lut_size, 0);
		if (!vf->rss_lut) {
			PMD_INIT_LOG(ERR, "unable to allocate rss_lut memory");
			goto err_rss;
		}
	}

	if (vf->vsi_res->num_queue_pairs > IAVF_MAX_NUM_QUEUES_DFLT)
		vf->lv_enabled = true;

	if (vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_RX_FLEX_DESC) {
		if (iavf_get_supported_rxdid(adapter) != 0) {
			PMD_INIT_LOG(ERR, "failed to do get supported rxdid");
			goto err_rss;
		}
	}

	if (vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_VLAN_V2) {
		if (iavf_get_vlan_offload_caps_v2(adapter) != 0) {
			PMD_INIT_LOG(ERR, "failed to do get VLAN offload v2 capabilities");
			goto err_rss;
		}
	}

	if (vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_QOS) {
		bufsz = sizeof(struct virtchnl_qos_cap_list) +
			IAVF_MAX_TRAFFIC_CLASS *
			sizeof(struct virtchnl_qos_cap_elem);
		vf->qos_cap = rte_zmalloc("qos_cap", bufsz, 0);
		if (!vf->qos_cap) {
			PMD_INIT_LOG(ERR, "unable to allocate qos_cap memory");
			goto err_rss;
		}
		iavf_tm_conf_init(dev);
	}

	iavf_init_proto_xtr(dev);

	return 0;
err_rss:
	rte_free(vf->rss_key);
	rte_free(vf->rss_lut);
err_alloc:
	rte_free(vf->qos_cap);
	rte_free(vf->vf_res);
	vf->vsi_res = NULL;
err_api:
	rte_free(vf->aq_resp);
err_aq:
	iavf_shutdown_adminq(hw);
err:
	return -1;
}

static void
iavf_uninit_vf(struct rte_eth_dev *dev)
{
	struct iavf_hw *hw = IAVF_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);

	iavf_shutdown_adminq(hw);

	rte_free(vf->vf_res);
	vf->vsi_res = NULL;
	vf->vf_res = NULL;

	rte_free(vf->aq_resp);
	vf->aq_resp = NULL;

	rte_free(vf->qos_cap);
	vf->qos_cap = NULL;

	rte_free(vf->rss_lut);
	vf->rss_lut = NULL;
	rte_free(vf->rss_key);
	vf->rss_key = NULL;
}

/* Enable default admin queue interrupt setting */
static inline void
iavf_enable_irq0(struct iavf_hw *hw)
{
	/* Enable admin queue interrupt trigger */
	IAVF_WRITE_REG(hw, IAVF_VFINT_ICR0_ENA1,
		       IAVF_VFINT_ICR0_ENA1_ADMINQ_MASK);

	IAVF_WRITE_REG(hw, IAVF_VFINT_DYN_CTL01,
		       IAVF_VFINT_DYN_CTL01_INTENA_MASK |
		       IAVF_VFINT_DYN_CTL01_CLEARPBA_MASK |
		       IAVF_VFINT_DYN_CTL01_ITR_INDX_MASK);

	IAVF_WRITE_FLUSH(hw);
}

static inline void
iavf_disable_irq0(struct iavf_hw *hw)
{
	/* Disable all interrupt types */
	IAVF_WRITE_REG(hw, IAVF_VFINT_ICR0_ENA1, 0);
	IAVF_WRITE_REG(hw, IAVF_VFINT_DYN_CTL01,
		       IAVF_VFINT_DYN_CTL01_ITR_INDX_MASK);
	IAVF_WRITE_FLUSH(hw);
}

static void
iavf_dev_interrupt_handler(void *param)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;
	struct iavf_hw *hw = IAVF_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	iavf_disable_irq0(hw);

	iavf_handle_virtchnl_msg(dev);

	iavf_enable_irq0(hw);
}

void
iavf_dev_alarm_handler(void *param)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;
	struct iavf_hw *hw = IAVF_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t icr0;

	iavf_disable_irq0(hw);

	/* read out interrupt causes */
	icr0 = IAVF_READ_REG(hw, IAVF_VFINT_ICR01);

	if (icr0 & IAVF_VFINT_ICR01_ADMINQ_MASK) {
		PMD_DRV_LOG(DEBUG, "ICR01_ADMINQ is reported");
		iavf_handle_virtchnl_msg(dev);
	}

	iavf_enable_irq0(hw);

	rte_eal_alarm_set(IAVF_ALARM_INTERVAL,
			  iavf_dev_alarm_handler, dev);
}

static int
iavf_dev_flow_ops_get(struct rte_eth_dev *dev,
		      const struct rte_flow_ops **ops)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);

	if (adapter->closed)
		return -EIO;

	*ops = &iavf_flow_ops;
	return 0;
}

static void
iavf_default_rss_disable(struct iavf_adapter *adapter)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	int ret = 0;

	if (vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_RSS_PF) {
		/* Set hena = 0 to ask PF to cleanup all existing RSS. */
		ret = iavf_set_hena(adapter, 0);
		if (ret)
			/* It is a workaround, temporarily allow error to be
			 * returned due to possible lack of PF handling for
			 * hena = 0.
			 */
			PMD_INIT_LOG(WARNING, "fail to disable default RSS,"
				    "lack PF support");
	}
}

static int
iavf_dev_init(struct rte_eth_dev *eth_dev)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(eth_dev->data->dev_private);
	struct iavf_hw *hw = IAVF_DEV_PRIVATE_TO_HW(adapter);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	int ret = 0;

	PMD_INIT_FUNC_TRACE();

	/* assign ops func pointer */
	eth_dev->dev_ops = &iavf_eth_dev_ops;
	eth_dev->rx_queue_count = iavf_dev_rxq_count;
	eth_dev->rx_descriptor_status = iavf_dev_rx_desc_status;
	eth_dev->tx_descriptor_status = iavf_dev_tx_desc_status;
	eth_dev->rx_pkt_burst = &iavf_recv_pkts;
	eth_dev->tx_pkt_burst = &iavf_xmit_pkts;
	eth_dev->tx_pkt_prepare = &iavf_prep_pkts;

	/* For secondary processes, we don't initialise any further as primary
	 * has already done this work. Only check if we need a different RX
	 * and TX function.
	 */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		iavf_set_rx_function(eth_dev);
		iavf_set_tx_function(eth_dev);
		return 0;
	}
	rte_eth_copy_pci_info(eth_dev, pci_dev);

	hw->vendor_id = pci_dev->id.vendor_id;
	hw->device_id = pci_dev->id.device_id;
	hw->subsystem_vendor_id = pci_dev->id.subsystem_vendor_id;
	hw->subsystem_device_id = pci_dev->id.subsystem_device_id;
	hw->bus.bus_id = pci_dev->addr.bus;
	hw->bus.device = pci_dev->addr.devid;
	hw->bus.func = pci_dev->addr.function;
	hw->hw_addr = (void *)pci_dev->mem_resource[0].addr;
	hw->back = IAVF_DEV_PRIVATE_TO_ADAPTER(eth_dev->data->dev_private);
	adapter->dev_data = eth_dev->data;
	adapter->stopped = 1;

	if (iavf_init_vf(eth_dev) != 0) {
		PMD_INIT_LOG(ERR, "Init vf failed");
		return -1;
	}

	/* set default ptype table */
	iavf_set_default_ptype_table(eth_dev);

	/* copy mac addr */
	eth_dev->data->mac_addrs = rte_zmalloc(
		"iavf_mac", RTE_ETHER_ADDR_LEN * IAVF_NUM_MACADDR_MAX, 0);
	if (!eth_dev->data->mac_addrs) {
		PMD_INIT_LOG(ERR, "Failed to allocate %d bytes needed to"
			     " store MAC addresses",
			     RTE_ETHER_ADDR_LEN * IAVF_NUM_MACADDR_MAX);
		ret = -ENOMEM;
		goto init_vf_err;
	}
	/* If the MAC address is not configured by host,
	 * generate a random one.
	 */
	if (!rte_is_valid_assigned_ether_addr(
			(struct rte_ether_addr *)hw->mac.addr))
		rte_eth_random_addr(hw->mac.addr);
	rte_ether_addr_copy((struct rte_ether_addr *)hw->mac.addr,
			&eth_dev->data->mac_addrs[0]);

	if (iavf_dev_event_handler_init())
		goto init_vf_err;

	if (vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_WB_ON_ITR) {
		/* register callback func to eal lib */
		rte_intr_callback_register(pci_dev->intr_handle,
					   iavf_dev_interrupt_handler,
					   (void *)eth_dev);

		/* enable uio intr after callback register */
		rte_intr_enable(pci_dev->intr_handle);
	} else {
		rte_eal_alarm_set(IAVF_ALARM_INTERVAL,
				  iavf_dev_alarm_handler, eth_dev);
	}

	/* configure and enable device interrupt */
	iavf_enable_irq0(hw);

	ret = iavf_flow_init(adapter);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to initialize flow");
		goto flow_init_err;
	}

	/** Check if the IPsec Crypto offload is supported and create
	 *  security_ctx if it is.
	 */
	if (iavf_ipsec_crypto_supported(adapter)) {
		/* Initialize security_ctx only for primary process*/
		ret = iavf_security_ctx_create(adapter);
		if (ret) {
			PMD_INIT_LOG(ERR, "failed to create ipsec crypto security instance");
			return ret;
		}

		ret = iavf_security_init(adapter);
		if (ret) {
			PMD_INIT_LOG(ERR, "failed to initialized ipsec crypto resources");
			return ret;
		}
	}

	iavf_default_rss_disable(adapter);


	/* Start device watchdog */
	iavf_dev_watchdog_enable(adapter);
	adapter->closed = false;

	return 0;

flow_init_err:
	rte_free(eth_dev->data->mac_addrs);
	eth_dev->data->mac_addrs = NULL;

init_vf_err:
	iavf_uninit_vf(eth_dev);

	return ret;
}

static int
iavf_dev_close(struct rte_eth_dev *dev)
{
	struct iavf_hw *hw = IAVF_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	int ret;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	if (adapter->closed) {
		ret = 0;
		goto out;
	}

	ret = iavf_dev_stop(dev);
	adapter->closed = true;

	/* free iAVF security device context all related resources */
	iavf_security_ctx_destroy(adapter);

	iavf_flow_flush(dev, NULL);
	iavf_flow_uninit(adapter);

	/*
	 * disable promiscuous mode before reset vf
	 * it is a workaround solution when work with kernel driver
	 * and it is not the normal way
	 */
	if (vf->promisc_unicast_enabled || vf->promisc_multicast_enabled)
		iavf_config_promisc(adapter, false, false);

	iavf_shutdown_adminq(hw);
	if (vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_WB_ON_ITR) {
		/* disable uio intr before callback unregister */
		rte_intr_disable(intr_handle);

		/* unregister callback func from eal lib */
		rte_intr_callback_unregister(intr_handle,
					     iavf_dev_interrupt_handler, dev);
	} else {
		rte_eal_alarm_cancel(iavf_dev_alarm_handler, dev);
	}
	iavf_disable_irq0(hw);

	if (vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_QOS)
		iavf_tm_conf_uninit(dev);

	if (vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_RSS_PF) {
		if (vf->rss_lut) {
			rte_free(vf->rss_lut);
			vf->rss_lut = NULL;
		}
		if (vf->rss_key) {
			rte_free(vf->rss_key);
			vf->rss_key = NULL;
		}
	}

	rte_free(vf->vf_res);
	vf->vsi_res = NULL;
	vf->vf_res = NULL;

	rte_free(vf->aq_resp);
	vf->aq_resp = NULL;

	/*
	 * If the VF is reset via VFLR, the device will be knocked out of bus
	 * master mode, and the driver will fail to recover from the reset. Fix
	 * this by enabling bus mastering after every reset. In a non-VFLR case,
	 * the bus master bit will not be disabled, and this call will have no
	 * effect.
	 */
out:
	if (vf->vf_reset && !rte_pci_set_bus_master(pci_dev, true))
		vf->vf_reset = false;

	/* disable watchdog */
	iavf_dev_watchdog_disable(adapter);

	return ret;
}

static int
iavf_dev_uninit(struct rte_eth_dev *dev)
{
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -EPERM;

	iavf_dev_close(dev);

	iavf_dev_event_handler_fini();

	return 0;
}

/*
 * Reset VF device only to re-initialize resources in PMD layer
 */
static int
iavf_dev_reset(struct rte_eth_dev *dev)
{
	int ret;

	ret = iavf_dev_uninit(dev);
	if (ret)
		return ret;

	return iavf_dev_init(dev);
}

static int
iavf_dcf_cap_check_handler(__rte_unused const char *key,
			   const char *value, __rte_unused void *opaque)
{
	if (strcmp(value, "dcf"))
		return -1;

	return 0;
}

static int
iavf_dcf_cap_selected(struct rte_devargs *devargs)
{
	struct rte_kvargs *kvlist;
	const char *key = "cap";
	int ret = 0;

	if (devargs == NULL)
		return 0;

	kvlist = rte_kvargs_parse(devargs->args, NULL);
	if (kvlist == NULL)
		return 0;

	if (!rte_kvargs_count(kvlist, key))
		goto exit;

	/* dcf capability selected when there's a key-value pair: cap=dcf */
	if (rte_kvargs_process(kvlist, key,
			       iavf_dcf_cap_check_handler, NULL) < 0)
		goto exit;

	ret = 1;

exit:
	rte_kvargs_free(kvlist);
	return ret;
}

static int eth_iavf_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
			     struct rte_pci_device *pci_dev)
{
	if (iavf_dcf_cap_selected(pci_dev->device.devargs))
		return 1;

	return rte_eth_dev_pci_generic_probe(pci_dev,
		sizeof(struct iavf_adapter), iavf_dev_init);
}

static int eth_iavf_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, iavf_dev_uninit);
}

/* Adaptive virtual function driver struct */
static struct rte_pci_driver rte_iavf_pmd = {
	.id_table = pci_id_iavf_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
	.probe = eth_iavf_pci_probe,
	.remove = eth_iavf_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_iavf, rte_iavf_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_iavf, pci_id_iavf_map);
RTE_PMD_REGISTER_KMOD_DEP(net_iavf, "* igb_uio | vfio-pci");
RTE_PMD_REGISTER_PARAM_STRING(net_iavf, "cap=dcf");
RTE_LOG_REGISTER_SUFFIX(iavf_logtype_init, init, NOTICE);
RTE_LOG_REGISTER_SUFFIX(iavf_logtype_driver, driver, NOTICE);
#ifdef RTE_ETHDEV_DEBUG_RX
RTE_LOG_REGISTER_SUFFIX(iavf_logtype_rx, rx, DEBUG);
#endif
#ifdef RTE_ETHDEV_DEBUG_TX
RTE_LOG_REGISTER_SUFFIX(iavf_logtype_tx, tx, DEBUG);
#endif
