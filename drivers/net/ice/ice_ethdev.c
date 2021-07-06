/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <rte_string_fns.h>
#include <rte_ethdev_pci.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <rte_tailq.h>

#include "base/ice_sched.h"
#include "base/ice_flow.h"
#include "base/ice_dcb.h"
#include "base/ice_common.h"

#include "rte_pmd_ice.h"
#include "ice_ethdev.h"
#include "ice_rxtx.h"
#include "ice_generic_flow.h"

/* devargs */
#define ICE_SAFE_MODE_SUPPORT_ARG "safe-mode-support"
#define ICE_PIPELINE_MODE_SUPPORT_ARG  "pipeline-mode-support"
#define ICE_PROTO_XTR_ARG         "proto_xtr"

static const char * const ice_valid_args[] = {
	ICE_SAFE_MODE_SUPPORT_ARG,
	ICE_PIPELINE_MODE_SUPPORT_ARG,
	ICE_PROTO_XTR_ARG,
	NULL
};

static const struct rte_mbuf_dynfield ice_proto_xtr_metadata_param = {
	.name = "intel_pmd_dynfield_proto_xtr_metadata",
	.size = sizeof(uint32_t),
	.align = __alignof__(uint32_t),
	.flags = 0,
};

struct proto_xtr_ol_flag {
	const struct rte_mbuf_dynflag param;
	uint64_t *ol_flag;
	bool required;
};

static bool ice_proto_xtr_hw_support[PROTO_XTR_MAX];

static struct proto_xtr_ol_flag ice_proto_xtr_ol_flag_params[] = {
	[PROTO_XTR_VLAN] = {
		.param = { .name = "intel_pmd_dynflag_proto_xtr_vlan" },
		.ol_flag = &rte_net_ice_dynflag_proto_xtr_vlan_mask },
	[PROTO_XTR_IPV4] = {
		.param = { .name = "intel_pmd_dynflag_proto_xtr_ipv4" },
		.ol_flag = &rte_net_ice_dynflag_proto_xtr_ipv4_mask },
	[PROTO_XTR_IPV6] = {
		.param = { .name = "intel_pmd_dynflag_proto_xtr_ipv6" },
		.ol_flag = &rte_net_ice_dynflag_proto_xtr_ipv6_mask },
	[PROTO_XTR_IPV6_FLOW] = {
		.param = { .name = "intel_pmd_dynflag_proto_xtr_ipv6_flow" },
		.ol_flag = &rte_net_ice_dynflag_proto_xtr_ipv6_flow_mask },
	[PROTO_XTR_TCP] = {
		.param = { .name = "intel_pmd_dynflag_proto_xtr_tcp" },
		.ol_flag = &rte_net_ice_dynflag_proto_xtr_tcp_mask },
	[PROTO_XTR_IP_OFFSET] = {
		.param = { .name = "intel_pmd_dynflag_proto_xtr_ip_offset" },
		.ol_flag = &rte_net_ice_dynflag_proto_xtr_ip_offset_mask },
};

#define ICE_DFLT_OUTER_TAG_TYPE ICE_AQ_VSI_OUTER_TAG_VLAN_9100

#define ICE_OS_DEFAULT_PKG_NAME		"ICE OS Default Package"
#define ICE_COMMS_PKG_NAME			"ICE COMMS Package"
#define ICE_MAX_RES_DESC_NUM        1024

static int ice_dev_configure(struct rte_eth_dev *dev);
static int ice_dev_start(struct rte_eth_dev *dev);
static int ice_dev_stop(struct rte_eth_dev *dev);
static int ice_dev_close(struct rte_eth_dev *dev);
static int ice_dev_reset(struct rte_eth_dev *dev);
static int ice_dev_info_get(struct rte_eth_dev *dev,
			    struct rte_eth_dev_info *dev_info);
static int ice_link_update(struct rte_eth_dev *dev,
			   int wait_to_complete);
static int ice_dev_set_link_up(struct rte_eth_dev *dev);
static int ice_dev_set_link_down(struct rte_eth_dev *dev);

static int ice_mtu_set(struct rte_eth_dev *dev, uint16_t mtu);
static int ice_vlan_offload_set(struct rte_eth_dev *dev, int mask);
static int ice_rss_reta_update(struct rte_eth_dev *dev,
			       struct rte_eth_rss_reta_entry64 *reta_conf,
			       uint16_t reta_size);
static int ice_rss_reta_query(struct rte_eth_dev *dev,
			      struct rte_eth_rss_reta_entry64 *reta_conf,
			      uint16_t reta_size);
static int ice_rss_hash_update(struct rte_eth_dev *dev,
			       struct rte_eth_rss_conf *rss_conf);
static int ice_rss_hash_conf_get(struct rte_eth_dev *dev,
				 struct rte_eth_rss_conf *rss_conf);
static int ice_promisc_enable(struct rte_eth_dev *dev);
static int ice_promisc_disable(struct rte_eth_dev *dev);
static int ice_allmulti_enable(struct rte_eth_dev *dev);
static int ice_allmulti_disable(struct rte_eth_dev *dev);
static int ice_vlan_filter_set(struct rte_eth_dev *dev,
			       uint16_t vlan_id,
			       int on);
static int ice_macaddr_set(struct rte_eth_dev *dev,
			   struct rte_ether_addr *mac_addr);
static int ice_macaddr_add(struct rte_eth_dev *dev,
			   struct rte_ether_addr *mac_addr,
			   __rte_unused uint32_t index,
			   uint32_t pool);
static void ice_macaddr_remove(struct rte_eth_dev *dev, uint32_t index);
static int ice_rx_queue_intr_enable(struct rte_eth_dev *dev,
				    uint16_t queue_id);
static int ice_rx_queue_intr_disable(struct rte_eth_dev *dev,
				     uint16_t queue_id);
static int ice_fw_version_get(struct rte_eth_dev *dev, char *fw_version,
			      size_t fw_size);
static int ice_vlan_pvid_set(struct rte_eth_dev *dev,
			     uint16_t pvid, int on);
static int ice_get_eeprom_length(struct rte_eth_dev *dev);
static int ice_get_eeprom(struct rte_eth_dev *dev,
			  struct rte_dev_eeprom_info *eeprom);
static int ice_stats_get(struct rte_eth_dev *dev,
			 struct rte_eth_stats *stats);
static int ice_stats_reset(struct rte_eth_dev *dev);
static int ice_xstats_get(struct rte_eth_dev *dev,
			  struct rte_eth_xstat *xstats, unsigned int n);
static int ice_xstats_get_names(struct rte_eth_dev *dev,
				struct rte_eth_xstat_name *xstats_names,
				unsigned int limit);
static int ice_dev_filter_ctrl(struct rte_eth_dev *dev,
			enum rte_filter_type filter_type,
			enum rte_filter_op filter_op,
			void *arg);
static int ice_dev_udp_tunnel_port_add(struct rte_eth_dev *dev,
			struct rte_eth_udp_tunnel *udp_tunnel);
static int ice_dev_udp_tunnel_port_del(struct rte_eth_dev *dev,
			struct rte_eth_udp_tunnel *udp_tunnel);

static const struct rte_pci_id pci_id_ice_map[] = {
	{ RTE_PCI_DEVICE(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E823L_BACKPLANE) },
	{ RTE_PCI_DEVICE(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E823L_SFP) },
	{ RTE_PCI_DEVICE(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E823L_10G_BASE_T) },
	{ RTE_PCI_DEVICE(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E823L_1GBE) },
	{ RTE_PCI_DEVICE(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E823L_QSFP) },
	{ RTE_PCI_DEVICE(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E810C_BACKPLANE) },
	{ RTE_PCI_DEVICE(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E810C_QSFP) },
	{ RTE_PCI_DEVICE(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E810C_SFP) },
	{ RTE_PCI_DEVICE(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E810_XXV_BACKPLANE) },
	{ RTE_PCI_DEVICE(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E810_XXV_QSFP) },
	{ RTE_PCI_DEVICE(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E810_XXV_SFP) },
	{ RTE_PCI_DEVICE(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E822C_BACKPLANE) },
	{ RTE_PCI_DEVICE(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E822C_QSFP) },
	{ RTE_PCI_DEVICE(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E822C_SFP) },
	{ RTE_PCI_DEVICE(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E822C_10G_BASE_T) },
	{ RTE_PCI_DEVICE(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E822C_SGMII) },
	{ RTE_PCI_DEVICE(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E822L_BACKPLANE) },
	{ RTE_PCI_DEVICE(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E822L_SFP) },
	{ RTE_PCI_DEVICE(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E822L_10G_BASE_T) },
	{ RTE_PCI_DEVICE(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E822L_SGMII) },
	{ .vendor_id = 0, /* sentinel */ },
};

static const struct eth_dev_ops ice_eth_dev_ops = {
	.dev_configure                = ice_dev_configure,
	.dev_start                    = ice_dev_start,
	.dev_stop                     = ice_dev_stop,
	.dev_close                    = ice_dev_close,
	.dev_reset                    = ice_dev_reset,
	.dev_set_link_up              = ice_dev_set_link_up,
	.dev_set_link_down            = ice_dev_set_link_down,
	.rx_queue_start               = ice_rx_queue_start,
	.rx_queue_stop                = ice_rx_queue_stop,
	.tx_queue_start               = ice_tx_queue_start,
	.tx_queue_stop                = ice_tx_queue_stop,
	.rx_queue_setup               = ice_rx_queue_setup,
	.rx_queue_release             = ice_rx_queue_release,
	.tx_queue_setup               = ice_tx_queue_setup,
	.tx_queue_release             = ice_tx_queue_release,
	.dev_infos_get                = ice_dev_info_get,
	.dev_supported_ptypes_get     = ice_dev_supported_ptypes_get,
	.link_update                  = ice_link_update,
	.mtu_set                      = ice_mtu_set,
	.mac_addr_set                 = ice_macaddr_set,
	.mac_addr_add                 = ice_macaddr_add,
	.mac_addr_remove              = ice_macaddr_remove,
	.vlan_filter_set              = ice_vlan_filter_set,
	.vlan_offload_set             = ice_vlan_offload_set,
	.reta_update                  = ice_rss_reta_update,
	.reta_query                   = ice_rss_reta_query,
	.rss_hash_update              = ice_rss_hash_update,
	.rss_hash_conf_get            = ice_rss_hash_conf_get,
	.promiscuous_enable           = ice_promisc_enable,
	.promiscuous_disable          = ice_promisc_disable,
	.allmulticast_enable          = ice_allmulti_enable,
	.allmulticast_disable         = ice_allmulti_disable,
	.rx_queue_intr_enable         = ice_rx_queue_intr_enable,
	.rx_queue_intr_disable        = ice_rx_queue_intr_disable,
	.fw_version_get               = ice_fw_version_get,
	.vlan_pvid_set                = ice_vlan_pvid_set,
	.rxq_info_get                 = ice_rxq_info_get,
	.txq_info_get                 = ice_txq_info_get,
	.rx_burst_mode_get            = ice_rx_burst_mode_get,
	.tx_burst_mode_get            = ice_tx_burst_mode_get,
	.get_eeprom_length            = ice_get_eeprom_length,
	.get_eeprom                   = ice_get_eeprom,
	.stats_get                    = ice_stats_get,
	.stats_reset                  = ice_stats_reset,
	.xstats_get                   = ice_xstats_get,
	.xstats_get_names             = ice_xstats_get_names,
	.xstats_reset                 = ice_stats_reset,
	.filter_ctrl                  = ice_dev_filter_ctrl,
	.udp_tunnel_port_add          = ice_dev_udp_tunnel_port_add,
	.udp_tunnel_port_del          = ice_dev_udp_tunnel_port_del,
	.tx_done_cleanup              = ice_tx_done_cleanup,
};

/* store statistics names and its offset in stats structure */
struct ice_xstats_name_off {
	char name[RTE_ETH_XSTATS_NAME_SIZE];
	unsigned int offset;
};

static const struct ice_xstats_name_off ice_stats_strings[] = {
	{"rx_unicast_packets", offsetof(struct ice_eth_stats, rx_unicast)},
	{"rx_multicast_packets", offsetof(struct ice_eth_stats, rx_multicast)},
	{"rx_broadcast_packets", offsetof(struct ice_eth_stats, rx_broadcast)},
	{"rx_dropped_packets", offsetof(struct ice_eth_stats, rx_discards)},
	{"rx_unknown_protocol_packets", offsetof(struct ice_eth_stats,
		rx_unknown_protocol)},
	{"tx_unicast_packets", offsetof(struct ice_eth_stats, tx_unicast)},
	{"tx_multicast_packets", offsetof(struct ice_eth_stats, tx_multicast)},
	{"tx_broadcast_packets", offsetof(struct ice_eth_stats, tx_broadcast)},
	{"tx_dropped_packets", offsetof(struct ice_eth_stats, tx_discards)},
};

#define ICE_NB_ETH_XSTATS (sizeof(ice_stats_strings) / \
		sizeof(ice_stats_strings[0]))

static const struct ice_xstats_name_off ice_hw_port_strings[] = {
	{"tx_link_down_dropped", offsetof(struct ice_hw_port_stats,
		tx_dropped_link_down)},
	{"rx_crc_errors", offsetof(struct ice_hw_port_stats, crc_errors)},
	{"rx_illegal_byte_errors", offsetof(struct ice_hw_port_stats,
		illegal_bytes)},
	{"rx_error_bytes", offsetof(struct ice_hw_port_stats, error_bytes)},
	{"mac_local_errors", offsetof(struct ice_hw_port_stats,
		mac_local_faults)},
	{"mac_remote_errors", offsetof(struct ice_hw_port_stats,
		mac_remote_faults)},
	{"rx_len_errors", offsetof(struct ice_hw_port_stats,
		rx_len_errors)},
	{"tx_xon_packets", offsetof(struct ice_hw_port_stats, link_xon_tx)},
	{"rx_xon_packets", offsetof(struct ice_hw_port_stats, link_xon_rx)},
	{"tx_xoff_packets", offsetof(struct ice_hw_port_stats, link_xoff_tx)},
	{"rx_xoff_packets", offsetof(struct ice_hw_port_stats, link_xoff_rx)},
	{"rx_size_64_packets", offsetof(struct ice_hw_port_stats, rx_size_64)},
	{"rx_size_65_to_127_packets", offsetof(struct ice_hw_port_stats,
		rx_size_127)},
	{"rx_size_128_to_255_packets", offsetof(struct ice_hw_port_stats,
		rx_size_255)},
	{"rx_size_256_to_511_packets", offsetof(struct ice_hw_port_stats,
		rx_size_511)},
	{"rx_size_512_to_1023_packets", offsetof(struct ice_hw_port_stats,
		rx_size_1023)},
	{"rx_size_1024_to_1522_packets", offsetof(struct ice_hw_port_stats,
		rx_size_1522)},
	{"rx_size_1523_to_max_packets", offsetof(struct ice_hw_port_stats,
		rx_size_big)},
	{"rx_undersized_errors", offsetof(struct ice_hw_port_stats,
		rx_undersize)},
	{"rx_oversize_errors", offsetof(struct ice_hw_port_stats,
		rx_oversize)},
	{"rx_mac_short_pkt_dropped", offsetof(struct ice_hw_port_stats,
		mac_short_pkt_dropped)},
	{"rx_fragmented_errors", offsetof(struct ice_hw_port_stats,
		rx_fragments)},
	{"rx_jabber_errors", offsetof(struct ice_hw_port_stats, rx_jabber)},
	{"tx_size_64_packets", offsetof(struct ice_hw_port_stats, tx_size_64)},
	{"tx_size_65_to_127_packets", offsetof(struct ice_hw_port_stats,
		tx_size_127)},
	{"tx_size_128_to_255_packets", offsetof(struct ice_hw_port_stats,
		tx_size_255)},
	{"tx_size_256_to_511_packets", offsetof(struct ice_hw_port_stats,
		tx_size_511)},
	{"tx_size_512_to_1023_packets", offsetof(struct ice_hw_port_stats,
		tx_size_1023)},
	{"tx_size_1024_to_1522_packets", offsetof(struct ice_hw_port_stats,
		tx_size_1522)},
	{"tx_size_1523_to_max_packets", offsetof(struct ice_hw_port_stats,
		tx_size_big)},
};

#define ICE_NB_HW_PORT_XSTATS (sizeof(ice_hw_port_strings) / \
		sizeof(ice_hw_port_strings[0]))

static void
ice_init_controlq_parameter(struct ice_hw *hw)
{
	/* fields for adminq */
	hw->adminq.num_rq_entries = ICE_ADMINQ_LEN;
	hw->adminq.num_sq_entries = ICE_ADMINQ_LEN;
	hw->adminq.rq_buf_size = ICE_ADMINQ_BUF_SZ;
	hw->adminq.sq_buf_size = ICE_ADMINQ_BUF_SZ;

	/* fields for mailboxq, DPDK used as PF host */
	hw->mailboxq.num_rq_entries = ICE_MAILBOXQ_LEN;
	hw->mailboxq.num_sq_entries = ICE_MAILBOXQ_LEN;
	hw->mailboxq.rq_buf_size = ICE_MAILBOXQ_BUF_SZ;
	hw->mailboxq.sq_buf_size = ICE_MAILBOXQ_BUF_SZ;
}

static int
lookup_proto_xtr_type(const char *xtr_name)
{
	static struct {
		const char *name;
		enum proto_xtr_type type;
	} xtr_type_map[] = {
		{ "vlan",      PROTO_XTR_VLAN      },
		{ "ipv4",      PROTO_XTR_IPV4      },
		{ "ipv6",      PROTO_XTR_IPV6      },
		{ "ipv6_flow", PROTO_XTR_IPV6_FLOW },
		{ "tcp",       PROTO_XTR_TCP       },
		{ "ip_offset", PROTO_XTR_IP_OFFSET },
	};
	uint32_t i;

	for (i = 0; i < RTE_DIM(xtr_type_map); i++) {
		if (strcmp(xtr_name, xtr_type_map[i].name) == 0)
			return xtr_type_map[i].type;
	}

	return -1;
}

/*
 * Parse elem, the elem could be single number/range or '(' ')' group
 * 1) A single number elem, it's just a simple digit. e.g. 9
 * 2) A single range elem, two digits with a '-' between. e.g. 2-6
 * 3) A group elem, combines multiple 1) or 2) with '( )'. e.g (0,2-4,6)
 *    Within group elem, '-' used for a range separator;
 *                       ',' used for a single number.
 */
static int
parse_queue_set(const char *input, int xtr_type, struct ice_devargs *devargs)
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
		if (errno || end == NULL || idx >= ICE_MAX_QUEUE_NUM)
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
			if (errno || end == NULL || idx >= ICE_MAX_QUEUE_NUM)
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

	min = ICE_MAX_QUEUE_NUM;
	do {
		/* go ahead to the first digit */
		while (isblank(*str))
			str++;
		if (!isdigit(*str))
			return -1;

		/* get the digit value */
		errno = 0;
		idx = strtoul(str, &end, 10);
		if (errno || end == NULL || idx >= ICE_MAX_QUEUE_NUM)
			return -1;

		/* go ahead to separator '-',',' and ')' */
		while (isblank(*end))
			end++;
		if (*end == '-') {
			if (min == ICE_MAX_QUEUE_NUM)
				min = idx;
			else /* avoid continuous '-' */
				return -1;
		} else if (*end == ',' || *end == ')') {
			max = idx;
			if (min == ICE_MAX_QUEUE_NUM)
				min = idx;

			for (idx = RTE_MIN(min, max);
			     idx <= RTE_MAX(min, max); idx++)
				devargs->proto_xtr[idx] = xtr_type;

			min = ICE_MAX_QUEUE_NUM;
		} else {
			return -1;
		}

		str = end + 1;
	} while (*end != ')' && *end != '\0');

	return 0;
}

static int
parse_queue_proto_xtr(const char *queues, struct ice_devargs *devargs)
{
	const char *queue_start;
	uint32_t idx;
	int xtr_type;
	char xtr_name[32];

	while (isblank(*queues))
		queues++;

	if (*queues != '[') {
		xtr_type = lookup_proto_xtr_type(queues);
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

			if (idx > sizeof(xtr_name) - 2)
				return -1;

			xtr_name[idx] = queues[idx];
		}
		xtr_name[idx] = '\0';
		xtr_type = lookup_proto_xtr_type(xtr_name);
		if (xtr_type < 0)
			return -1;

		queues += idx;

		while (isblank(*queues) || *queues == ',' || *queues == ']')
			queues++;

		if (parse_queue_set(queue_start, xtr_type, devargs) < 0)
			return -1;
	} while (*queues != '\0');

	return 0;
}

static int
handle_proto_xtr_arg(__rte_unused const char *key, const char *value,
		     void *extra_args)
{
	struct ice_devargs *devargs = extra_args;

	if (value == NULL || extra_args == NULL)
		return -EINVAL;

	if (parse_queue_proto_xtr(value, devargs) < 0) {
		PMD_DRV_LOG(ERR,
			    "The protocol extraction parameter is wrong : '%s'",
			    value);
		return -1;
	}

	return 0;
}

static void
ice_check_proto_xtr_support(struct ice_hw *hw)
{
#define FLX_REG(val, fld, idx) \
	(((val) & GLFLXP_RXDID_FLX_WRD_##idx##_##fld##_M) >> \
	 GLFLXP_RXDID_FLX_WRD_##idx##_##fld##_S)
	static struct {
		uint32_t rxdid;
		uint8_t opcode;
		uint8_t protid_0;
		uint8_t protid_1;
	} xtr_sets[] = {
		[PROTO_XTR_VLAN] = { ICE_RXDID_COMMS_AUX_VLAN,
				     ICE_RX_OPC_EXTRACT,
				     ICE_PROT_EVLAN_O, ICE_PROT_VLAN_O},
		[PROTO_XTR_IPV4] = { ICE_RXDID_COMMS_AUX_IPV4,
				     ICE_RX_OPC_EXTRACT,
				     ICE_PROT_IPV4_OF_OR_S,
				     ICE_PROT_IPV4_OF_OR_S },
		[PROTO_XTR_IPV6] = { ICE_RXDID_COMMS_AUX_IPV6,
				     ICE_RX_OPC_EXTRACT,
				     ICE_PROT_IPV6_OF_OR_S,
				     ICE_PROT_IPV6_OF_OR_S },
		[PROTO_XTR_IPV6_FLOW] = { ICE_RXDID_COMMS_AUX_IPV6_FLOW,
					  ICE_RX_OPC_EXTRACT,
					  ICE_PROT_IPV6_OF_OR_S,
					  ICE_PROT_IPV6_OF_OR_S },
		[PROTO_XTR_TCP] = { ICE_RXDID_COMMS_AUX_TCP,
				    ICE_RX_OPC_EXTRACT,
				    ICE_PROT_TCP_IL, ICE_PROT_ID_INVAL },
		[PROTO_XTR_IP_OFFSET] = { ICE_RXDID_COMMS_AUX_IP_OFFSET,
					  ICE_RX_OPC_PROTID,
					  ICE_PROT_IPV4_OF_OR_S,
					  ICE_PROT_IPV6_OF_OR_S },
	};
	uint32_t i;

	for (i = 0; i < RTE_DIM(xtr_sets); i++) {
		uint32_t rxdid = xtr_sets[i].rxdid;
		uint32_t v;

		if (xtr_sets[i].protid_0 != ICE_PROT_ID_INVAL) {
			v = ICE_READ_REG(hw, GLFLXP_RXDID_FLX_WRD_4(rxdid));

			if (FLX_REG(v, PROT_MDID, 4) == xtr_sets[i].protid_0 &&
			    FLX_REG(v, RXDID_OPCODE, 4) == xtr_sets[i].opcode)
				ice_proto_xtr_hw_support[i] = true;
		}

		if (xtr_sets[i].protid_1 != ICE_PROT_ID_INVAL) {
			v = ICE_READ_REG(hw, GLFLXP_RXDID_FLX_WRD_5(rxdid));

			if (FLX_REG(v, PROT_MDID, 5) == xtr_sets[i].protid_1 &&
			    FLX_REG(v, RXDID_OPCODE, 5) == xtr_sets[i].opcode)
				ice_proto_xtr_hw_support[i] = true;
		}
	}
}

static int
ice_res_pool_init(struct ice_res_pool_info *pool, uint32_t base,
		  uint32_t num)
{
	struct pool_entry *entry;

	if (!pool || !num)
		return -EINVAL;

	entry = rte_zmalloc(NULL, sizeof(*entry), 0);
	if (!entry) {
		PMD_INIT_LOG(ERR,
			     "Failed to allocate memory for resource pool");
		return -ENOMEM;
	}

	/* queue heap initialize */
	pool->num_free = num;
	pool->num_alloc = 0;
	pool->base = base;
	LIST_INIT(&pool->alloc_list);
	LIST_INIT(&pool->free_list);

	/* Initialize element  */
	entry->base = 0;
	entry->len = num;

	LIST_INSERT_HEAD(&pool->free_list, entry, next);
	return 0;
}

static int
ice_res_pool_alloc(struct ice_res_pool_info *pool,
		   uint16_t num)
{
	struct pool_entry *entry, *valid_entry;

	if (!pool || !num) {
		PMD_INIT_LOG(ERR, "Invalid parameter");
		return -EINVAL;
	}

	if (pool->num_free < num) {
		PMD_INIT_LOG(ERR, "No resource. ask:%u, available:%u",
			     num, pool->num_free);
		return -ENOMEM;
	}

	valid_entry = NULL;
	/* Lookup  in free list and find most fit one */
	LIST_FOREACH(entry, &pool->free_list, next) {
		if (entry->len >= num) {
			/* Find best one */
			if (entry->len == num) {
				valid_entry = entry;
				break;
			}
			if (!valid_entry ||
			    valid_entry->len > entry->len)
				valid_entry = entry;
		}
	}

	/* Not find one to satisfy the request, return */
	if (!valid_entry) {
		PMD_INIT_LOG(ERR, "No valid entry found");
		return -ENOMEM;
	}
	/**
	 * The entry have equal queue number as requested,
	 * remove it from alloc_list.
	 */
	if (valid_entry->len == num) {
		LIST_REMOVE(valid_entry, next);
	} else {
		/**
		 * The entry have more numbers than requested,
		 * create a new entry for alloc_list and minus its
		 * queue base and number in free_list.
		 */
		entry = rte_zmalloc(NULL, sizeof(*entry), 0);
		if (!entry) {
			PMD_INIT_LOG(ERR,
				     "Failed to allocate memory for "
				     "resource pool");
			return -ENOMEM;
		}
		entry->base = valid_entry->base;
		entry->len = num;
		valid_entry->base += num;
		valid_entry->len -= num;
		valid_entry = entry;
	}

	/* Insert it into alloc list, not sorted */
	LIST_INSERT_HEAD(&pool->alloc_list, valid_entry, next);

	pool->num_free -= valid_entry->len;
	pool->num_alloc += valid_entry->len;

	return valid_entry->base + pool->base;
}

static void
ice_res_pool_destroy(struct ice_res_pool_info *pool)
{
	struct pool_entry *entry, *next_entry;

	if (!pool)
		return;

	for (entry = LIST_FIRST(&pool->alloc_list);
	     entry && (next_entry = LIST_NEXT(entry, next), 1);
	     entry = next_entry) {
		LIST_REMOVE(entry, next);
		rte_free(entry);
	}

	for (entry = LIST_FIRST(&pool->free_list);
	     entry && (next_entry = LIST_NEXT(entry, next), 1);
	     entry = next_entry) {
		LIST_REMOVE(entry, next);
		rte_free(entry);
	}

	pool->num_free = 0;
	pool->num_alloc = 0;
	pool->base = 0;
	LIST_INIT(&pool->alloc_list);
	LIST_INIT(&pool->free_list);
}

static void
ice_vsi_config_default_rss(struct ice_aqc_vsi_props *info)
{
	/* Set VSI LUT selection */
	info->q_opt_rss = ICE_AQ_VSI_Q_OPT_RSS_LUT_VSI &
			  ICE_AQ_VSI_Q_OPT_RSS_LUT_M;
	/* Set Hash scheme */
	info->q_opt_rss |= ICE_AQ_VSI_Q_OPT_RSS_TPLZ &
			   ICE_AQ_VSI_Q_OPT_RSS_HASH_M;
	/* enable TC */
	info->q_opt_tc = ICE_AQ_VSI_Q_OPT_TC_OVR_M;
}

static enum ice_status
ice_vsi_config_tc_queue_mapping(struct ice_vsi *vsi,
				struct ice_aqc_vsi_props *info,
				uint8_t enabled_tcmap)
{
	uint16_t bsf, qp_idx;

	/* default tc 0 now. Multi-TC supporting need to be done later.
	 * Configure TC and queue mapping parameters, for enabled TC,
	 * allocate qpnum_per_tc queues to this traffic.
	 */
	if (enabled_tcmap != 0x01) {
		PMD_INIT_LOG(ERR, "only TC0 is supported");
		return -ENOTSUP;
	}

	vsi->nb_qps = RTE_MIN(vsi->nb_qps, ICE_MAX_Q_PER_TC);
	bsf = rte_bsf32(vsi->nb_qps);
	/* Adjust the queue number to actual queues that can be applied */
	vsi->nb_qps = 0x1 << bsf;

	qp_idx = 0;
	/* Set tc and queue mapping with VSI */
	info->tc_mapping[0] = rte_cpu_to_le_16((qp_idx <<
						ICE_AQ_VSI_TC_Q_OFFSET_S) |
					       (bsf << ICE_AQ_VSI_TC_Q_NUM_S));

	/* Associate queue number with VSI */
	info->mapping_flags |= rte_cpu_to_le_16(ICE_AQ_VSI_Q_MAP_CONTIG);
	info->q_mapping[0] = rte_cpu_to_le_16(vsi->base_queue);
	info->q_mapping[1] = rte_cpu_to_le_16(vsi->nb_qps);
	info->valid_sections |=
		rte_cpu_to_le_16(ICE_AQ_VSI_PROP_RXQ_MAP_VALID);
	/* Set the info.ingress_table and info.egress_table
	 * for UP translate table. Now just set it to 1:1 map by default
	 * -- 0b 111 110 101 100 011 010 001 000 == 0xFAC688
	 */
#define ICE_TC_QUEUE_TABLE_DFLT 0x00FAC688
	info->ingress_table  = rte_cpu_to_le_32(ICE_TC_QUEUE_TABLE_DFLT);
	info->egress_table   = rte_cpu_to_le_32(ICE_TC_QUEUE_TABLE_DFLT);
	info->outer_up_table = rte_cpu_to_le_32(ICE_TC_QUEUE_TABLE_DFLT);
	return 0;
}

static int
ice_init_mac_address(struct rte_eth_dev *dev)
{
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (!rte_is_unicast_ether_addr
		((struct rte_ether_addr *)hw->port_info[0].mac.lan_addr)) {
		PMD_INIT_LOG(ERR, "Invalid MAC address");
		return -EINVAL;
	}

	rte_ether_addr_copy(
		(struct rte_ether_addr *)hw->port_info[0].mac.lan_addr,
		(struct rte_ether_addr *)hw->port_info[0].mac.perm_addr);

	dev->data->mac_addrs =
		rte_zmalloc(NULL, sizeof(struct rte_ether_addr) * ICE_NUM_MACADDR_MAX, 0);
	if (!dev->data->mac_addrs) {
		PMD_INIT_LOG(ERR,
			     "Failed to allocate memory to store mac address");
		return -ENOMEM;
	}
	/* store it to dev data */
	rte_ether_addr_copy(
		(struct rte_ether_addr *)hw->port_info[0].mac.perm_addr,
		&dev->data->mac_addrs[0]);
	return 0;
}

/* Find out specific MAC filter */
static struct ice_mac_filter *
ice_find_mac_filter(struct ice_vsi *vsi, struct rte_ether_addr *macaddr)
{
	struct ice_mac_filter *f;

	TAILQ_FOREACH(f, &vsi->mac_list, next) {
		if (rte_is_same_ether_addr(macaddr, &f->mac_info.mac_addr))
			return f;
	}

	return NULL;
}

static int
ice_add_mac_filter(struct ice_vsi *vsi, struct rte_ether_addr *mac_addr)
{
	struct ice_fltr_list_entry *m_list_itr = NULL;
	struct ice_mac_filter *f;
	struct LIST_HEAD_TYPE list_head;
	struct ice_hw *hw = ICE_VSI_TO_HW(vsi);
	int ret = 0;

	/* If it's added and configured, return */
	f = ice_find_mac_filter(vsi, mac_addr);
	if (f) {
		PMD_DRV_LOG(INFO, "This MAC filter already exists.");
		return 0;
	}

	INIT_LIST_HEAD(&list_head);

	m_list_itr = (struct ice_fltr_list_entry *)
		ice_malloc(hw, sizeof(*m_list_itr));
	if (!m_list_itr) {
		ret = -ENOMEM;
		goto DONE;
	}
	ice_memcpy(m_list_itr->fltr_info.l_data.mac.mac_addr,
		   mac_addr, ETH_ALEN, ICE_NONDMA_TO_NONDMA);
	m_list_itr->fltr_info.src_id = ICE_SRC_ID_VSI;
	m_list_itr->fltr_info.fltr_act = ICE_FWD_TO_VSI;
	m_list_itr->fltr_info.lkup_type = ICE_SW_LKUP_MAC;
	m_list_itr->fltr_info.flag = ICE_FLTR_TX;
	m_list_itr->fltr_info.vsi_handle = vsi->idx;

	LIST_ADD(&m_list_itr->list_entry, &list_head);

	/* Add the mac */
	ret = ice_add_mac(hw, &list_head);
	if (ret != ICE_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to add MAC filter");
		ret = -EINVAL;
		goto DONE;
	}
	/* Add the mac addr into mac list */
	f = rte_zmalloc(NULL, sizeof(*f), 0);
	if (!f) {
		PMD_DRV_LOG(ERR, "failed to allocate memory");
		ret = -ENOMEM;
		goto DONE;
	}
	rte_ether_addr_copy(mac_addr, &f->mac_info.mac_addr);
	TAILQ_INSERT_TAIL(&vsi->mac_list, f, next);
	vsi->mac_num++;

	ret = 0;

DONE:
	rte_free(m_list_itr);
	return ret;
}

static int
ice_remove_mac_filter(struct ice_vsi *vsi, struct rte_ether_addr *mac_addr)
{
	struct ice_fltr_list_entry *m_list_itr = NULL;
	struct ice_mac_filter *f;
	struct LIST_HEAD_TYPE list_head;
	struct ice_hw *hw = ICE_VSI_TO_HW(vsi);
	int ret = 0;

	/* Can't find it, return an error */
	f = ice_find_mac_filter(vsi, mac_addr);
	if (!f)
		return -EINVAL;

	INIT_LIST_HEAD(&list_head);

	m_list_itr = (struct ice_fltr_list_entry *)
		ice_malloc(hw, sizeof(*m_list_itr));
	if (!m_list_itr) {
		ret = -ENOMEM;
		goto DONE;
	}
	ice_memcpy(m_list_itr->fltr_info.l_data.mac.mac_addr,
		   mac_addr, ETH_ALEN, ICE_NONDMA_TO_NONDMA);
	m_list_itr->fltr_info.src_id = ICE_SRC_ID_VSI;
	m_list_itr->fltr_info.fltr_act = ICE_FWD_TO_VSI;
	m_list_itr->fltr_info.lkup_type = ICE_SW_LKUP_MAC;
	m_list_itr->fltr_info.flag = ICE_FLTR_TX;
	m_list_itr->fltr_info.vsi_handle = vsi->idx;

	LIST_ADD(&m_list_itr->list_entry, &list_head);

	/* remove the mac filter */
	ret = ice_remove_mac(hw, &list_head);
	if (ret != ICE_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to remove MAC filter");
		ret = -EINVAL;
		goto DONE;
	}

	/* Remove the mac addr from mac list */
	TAILQ_REMOVE(&vsi->mac_list, f, next);
	rte_free(f);
	vsi->mac_num--;

	ret = 0;
DONE:
	rte_free(m_list_itr);
	return ret;
}

/* Find out specific VLAN filter */
static struct ice_vlan_filter *
ice_find_vlan_filter(struct ice_vsi *vsi, uint16_t vlan_id)
{
	struct ice_vlan_filter *f;

	TAILQ_FOREACH(f, &vsi->vlan_list, next) {
		if (vlan_id == f->vlan_info.vlan_id)
			return f;
	}

	return NULL;
}

static int
ice_add_vlan_filter(struct ice_vsi *vsi, uint16_t vlan_id)
{
	struct ice_fltr_list_entry *v_list_itr = NULL;
	struct ice_vlan_filter *f;
	struct LIST_HEAD_TYPE list_head;
	struct ice_hw *hw;
	int ret = 0;

	if (!vsi || vlan_id > RTE_ETHER_MAX_VLAN_ID)
		return -EINVAL;

	hw = ICE_VSI_TO_HW(vsi);

	/* If it's added and configured, return. */
	f = ice_find_vlan_filter(vsi, vlan_id);
	if (f) {
		PMD_DRV_LOG(INFO, "This VLAN filter already exists.");
		return 0;
	}

	if (!vsi->vlan_anti_spoof_on && !vsi->vlan_filter_on)
		return 0;

	INIT_LIST_HEAD(&list_head);

	v_list_itr = (struct ice_fltr_list_entry *)
		      ice_malloc(hw, sizeof(*v_list_itr));
	if (!v_list_itr) {
		ret = -ENOMEM;
		goto DONE;
	}
	v_list_itr->fltr_info.l_data.vlan.vlan_id = vlan_id;
	v_list_itr->fltr_info.src_id = ICE_SRC_ID_VSI;
	v_list_itr->fltr_info.fltr_act = ICE_FWD_TO_VSI;
	v_list_itr->fltr_info.lkup_type = ICE_SW_LKUP_VLAN;
	v_list_itr->fltr_info.flag = ICE_FLTR_TX;
	v_list_itr->fltr_info.vsi_handle = vsi->idx;

	LIST_ADD(&v_list_itr->list_entry, &list_head);

	/* Add the vlan */
	ret = ice_add_vlan(hw, &list_head);
	if (ret != ICE_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to add VLAN filter");
		ret = -EINVAL;
		goto DONE;
	}

	/* Add vlan into vlan list */
	f = rte_zmalloc(NULL, sizeof(*f), 0);
	if (!f) {
		PMD_DRV_LOG(ERR, "failed to allocate memory");
		ret = -ENOMEM;
		goto DONE;
	}
	f->vlan_info.vlan_id = vlan_id;
	TAILQ_INSERT_TAIL(&vsi->vlan_list, f, next);
	vsi->vlan_num++;

	ret = 0;

DONE:
	rte_free(v_list_itr);
	return ret;
}

static int
ice_remove_vlan_filter(struct ice_vsi *vsi, uint16_t vlan_id)
{
	struct ice_fltr_list_entry *v_list_itr = NULL;
	struct ice_vlan_filter *f;
	struct LIST_HEAD_TYPE list_head;
	struct ice_hw *hw;
	int ret = 0;

	/**
	 * Vlan 0 is the generic filter for untagged packets
	 * and can't be removed.
	 */
	if (!vsi || vlan_id == 0 || vlan_id > RTE_ETHER_MAX_VLAN_ID)
		return -EINVAL;

	hw = ICE_VSI_TO_HW(vsi);

	/* Can't find it, return an error */
	f = ice_find_vlan_filter(vsi, vlan_id);
	if (!f)
		return -EINVAL;

	INIT_LIST_HEAD(&list_head);

	v_list_itr = (struct ice_fltr_list_entry *)
		      ice_malloc(hw, sizeof(*v_list_itr));
	if (!v_list_itr) {
		ret = -ENOMEM;
		goto DONE;
	}

	v_list_itr->fltr_info.l_data.vlan.vlan_id = vlan_id;
	v_list_itr->fltr_info.src_id = ICE_SRC_ID_VSI;
	v_list_itr->fltr_info.fltr_act = ICE_FWD_TO_VSI;
	v_list_itr->fltr_info.lkup_type = ICE_SW_LKUP_VLAN;
	v_list_itr->fltr_info.flag = ICE_FLTR_TX;
	v_list_itr->fltr_info.vsi_handle = vsi->idx;

	LIST_ADD(&v_list_itr->list_entry, &list_head);

	/* remove the vlan filter */
	ret = ice_remove_vlan(hw, &list_head);
	if (ret != ICE_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to remove VLAN filter");
		ret = -EINVAL;
		goto DONE;
	}

	/* Remove the vlan id from vlan list */
	TAILQ_REMOVE(&vsi->vlan_list, f, next);
	rte_free(f);
	vsi->vlan_num--;

	ret = 0;
DONE:
	rte_free(v_list_itr);
	return ret;
}

static int
ice_remove_all_mac_vlan_filters(struct ice_vsi *vsi)
{
	struct ice_mac_filter *m_f;
	struct ice_vlan_filter *v_f;
	void *temp;
	int ret = 0;

	if (!vsi || !vsi->mac_num)
		return -EINVAL;

	TAILQ_FOREACH_SAFE(m_f, &vsi->mac_list, next, temp) {
		ret = ice_remove_mac_filter(vsi, &m_f->mac_info.mac_addr);
		if (ret != ICE_SUCCESS) {
			ret = -EINVAL;
			goto DONE;
		}
	}

	if (vsi->vlan_num == 0)
		return 0;

	TAILQ_FOREACH_SAFE(v_f, &vsi->vlan_list, next, temp) {
		ret = ice_remove_vlan_filter(vsi, v_f->vlan_info.vlan_id);
		if (ret != ICE_SUCCESS) {
			ret = -EINVAL;
			goto DONE;
		}
	}

DONE:
	return ret;
}

static int
ice_vsi_config_qinq_insertion(struct ice_vsi *vsi, bool on)
{
	struct ice_hw *hw = ICE_VSI_TO_HW(vsi);
	struct ice_vsi_ctx ctxt;
	uint8_t qinq_flags;
	int ret = 0;

	/* Check if it has been already on or off */
	if (vsi->info.valid_sections &
		rte_cpu_to_le_16(ICE_AQ_VSI_PROP_OUTER_TAG_VALID)) {
		if (on) {
			if ((vsi->info.outer_tag_flags &
			     ICE_AQ_VSI_OUTER_TAG_ACCEPT_HOST) ==
			    ICE_AQ_VSI_OUTER_TAG_ACCEPT_HOST)
				return 0; /* already on */
		} else {
			if (!(vsi->info.outer_tag_flags &
			      ICE_AQ_VSI_OUTER_TAG_ACCEPT_HOST))
				return 0; /* already off */
		}
	}

	if (on)
		qinq_flags = ICE_AQ_VSI_OUTER_TAG_ACCEPT_HOST;
	else
		qinq_flags = 0;
	/* clear global insertion and use per packet insertion */
	vsi->info.outer_tag_flags &= ~(ICE_AQ_VSI_OUTER_TAG_INSERT);
	vsi->info.outer_tag_flags &= ~(ICE_AQ_VSI_OUTER_TAG_ACCEPT_HOST);
	vsi->info.outer_tag_flags |= qinq_flags;
	/* use default vlan type 0x8100 */
	vsi->info.outer_tag_flags &= ~(ICE_AQ_VSI_OUTER_TAG_TYPE_M);
	vsi->info.outer_tag_flags |= ICE_DFLT_OUTER_TAG_TYPE <<
				     ICE_AQ_VSI_OUTER_TAG_TYPE_S;
	(void)rte_memcpy(&ctxt.info, &vsi->info, sizeof(vsi->info));
	ctxt.info.valid_sections =
			rte_cpu_to_le_16(ICE_AQ_VSI_PROP_OUTER_TAG_VALID);
	ctxt.vsi_num = vsi->vsi_id;
	ret = ice_update_vsi(hw, vsi->idx, &ctxt, NULL);
	if (ret) {
		PMD_DRV_LOG(INFO,
			    "Update VSI failed to %s qinq stripping",
			    on ? "enable" : "disable");
		return -EINVAL;
	}

	vsi->info.valid_sections |=
		rte_cpu_to_le_16(ICE_AQ_VSI_PROP_OUTER_TAG_VALID);

	return ret;
}

static int
ice_vsi_config_qinq_stripping(struct ice_vsi *vsi, bool on)
{
	struct ice_hw *hw = ICE_VSI_TO_HW(vsi);
	struct ice_vsi_ctx ctxt;
	uint8_t qinq_flags;
	int ret = 0;

	/* Check if it has been already on or off */
	if (vsi->info.valid_sections &
		rte_cpu_to_le_16(ICE_AQ_VSI_PROP_OUTER_TAG_VALID)) {
		if (on) {
			if ((vsi->info.outer_tag_flags &
			     ICE_AQ_VSI_OUTER_TAG_MODE_M) ==
			    ICE_AQ_VSI_OUTER_TAG_COPY)
				return 0; /* already on */
		} else {
			if ((vsi->info.outer_tag_flags &
			     ICE_AQ_VSI_OUTER_TAG_MODE_M) ==
			    ICE_AQ_VSI_OUTER_TAG_NOTHING)
				return 0; /* already off */
		}
	}

	if (on)
		qinq_flags = ICE_AQ_VSI_OUTER_TAG_COPY;
	else
		qinq_flags = ICE_AQ_VSI_OUTER_TAG_NOTHING;
	vsi->info.outer_tag_flags &= ~(ICE_AQ_VSI_OUTER_TAG_MODE_M);
	vsi->info.outer_tag_flags |= qinq_flags;
	/* use default vlan type 0x8100 */
	vsi->info.outer_tag_flags &= ~(ICE_AQ_VSI_OUTER_TAG_TYPE_M);
	vsi->info.outer_tag_flags |= ICE_DFLT_OUTER_TAG_TYPE <<
				     ICE_AQ_VSI_OUTER_TAG_TYPE_S;
	(void)rte_memcpy(&ctxt.info, &vsi->info, sizeof(vsi->info));
	ctxt.info.valid_sections =
			rte_cpu_to_le_16(ICE_AQ_VSI_PROP_OUTER_TAG_VALID);
	ctxt.vsi_num = vsi->vsi_id;
	ret = ice_update_vsi(hw, vsi->idx, &ctxt, NULL);
	if (ret) {
		PMD_DRV_LOG(INFO,
			    "Update VSI failed to %s qinq stripping",
			    on ? "enable" : "disable");
		return -EINVAL;
	}

	vsi->info.valid_sections |=
		rte_cpu_to_le_16(ICE_AQ_VSI_PROP_OUTER_TAG_VALID);

	return ret;
}

static int
ice_vsi_config_double_vlan(struct ice_vsi *vsi, int on)
{
	int ret;

	ret = ice_vsi_config_qinq_stripping(vsi, on);
	if (ret)
		PMD_DRV_LOG(ERR, "Fail to set qinq stripping - %d", ret);

	ret = ice_vsi_config_qinq_insertion(vsi, on);
	if (ret)
		PMD_DRV_LOG(ERR, "Fail to set qinq insertion - %d", ret);

	return ret;
}

/* Enable IRQ0 */
static void
ice_pf_enable_irq0(struct ice_hw *hw)
{
	/* reset the registers */
	ICE_WRITE_REG(hw, PFINT_OICR_ENA, 0);
	ICE_READ_REG(hw, PFINT_OICR);

#ifdef ICE_LSE_SPT
	ICE_WRITE_REG(hw, PFINT_OICR_ENA,
		      (uint32_t)(PFINT_OICR_ENA_INT_ENA_M &
				 (~PFINT_OICR_LINK_STAT_CHANGE_M)));

	ICE_WRITE_REG(hw, PFINT_OICR_CTL,
		      (0 & PFINT_OICR_CTL_MSIX_INDX_M) |
		      ((0 << PFINT_OICR_CTL_ITR_INDX_S) &
		       PFINT_OICR_CTL_ITR_INDX_M) |
		      PFINT_OICR_CTL_CAUSE_ENA_M);

	ICE_WRITE_REG(hw, PFINT_FW_CTL,
		      (0 & PFINT_FW_CTL_MSIX_INDX_M) |
		      ((0 << PFINT_FW_CTL_ITR_INDX_S) &
		       PFINT_FW_CTL_ITR_INDX_M) |
		      PFINT_FW_CTL_CAUSE_ENA_M);
#else
	ICE_WRITE_REG(hw, PFINT_OICR_ENA, PFINT_OICR_ENA_INT_ENA_M);
#endif

	ICE_WRITE_REG(hw, GLINT_DYN_CTL(0),
		      GLINT_DYN_CTL_INTENA_M |
		      GLINT_DYN_CTL_CLEARPBA_M |
		      GLINT_DYN_CTL_ITR_INDX_M);

	ice_flush(hw);
}

/* Disable IRQ0 */
static void
ice_pf_disable_irq0(struct ice_hw *hw)
{
	/* Disable all interrupt types */
	ICE_WRITE_REG(hw, GLINT_DYN_CTL(0), GLINT_DYN_CTL_WB_ON_ITR_M);
	ice_flush(hw);
}

#ifdef ICE_LSE_SPT
static void
ice_handle_aq_msg(struct rte_eth_dev *dev)
{
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ice_ctl_q_info *cq = &hw->adminq;
	struct ice_rq_event_info event;
	uint16_t pending, opcode;
	int ret;

	event.buf_len = ICE_AQ_MAX_BUF_LEN;
	event.msg_buf = rte_zmalloc(NULL, event.buf_len, 0);
	if (!event.msg_buf) {
		PMD_DRV_LOG(ERR, "Failed to allocate mem");
		return;
	}

	pending = 1;
	while (pending) {
		ret = ice_clean_rq_elem(hw, cq, &event, &pending);

		if (ret != ICE_SUCCESS) {
			PMD_DRV_LOG(INFO,
				    "Failed to read msg from AdminQ, "
				    "adminq_err: %u",
				    hw->adminq.sq_last_status);
			break;
		}
		opcode = rte_le_to_cpu_16(event.desc.opcode);

		switch (opcode) {
		case ice_aqc_opc_get_link_status:
			ret = ice_link_update(dev, 0);
			if (!ret)
				rte_eth_dev_callback_process
					(dev, RTE_ETH_EVENT_INTR_LSC, NULL);
			break;
		default:
			PMD_DRV_LOG(DEBUG, "Request %u is not supported yet",
				    opcode);
			break;
		}
	}
	rte_free(event.msg_buf);
}
#endif

/**
 * Interrupt handler triggered by NIC for handling
 * specific interrupt.
 *
 * @param handle
 *  Pointer to interrupt handle.
 * @param param
 *  The address of parameter (struct rte_eth_dev *) regsitered before.
 *
 * @return
 *  void
 */
static void
ice_interrupt_handler(void *param)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t oicr;
	uint32_t reg;
	uint8_t pf_num;
	uint8_t event;
	uint16_t queue;
	int ret;
#ifdef ICE_LSE_SPT
	uint32_t int_fw_ctl;
#endif

	/* Disable interrupt */
	ice_pf_disable_irq0(hw);

	/* read out interrupt causes */
	oicr = ICE_READ_REG(hw, PFINT_OICR);
#ifdef ICE_LSE_SPT
	int_fw_ctl = ICE_READ_REG(hw, PFINT_FW_CTL);
#endif

	/* No interrupt event indicated */
	if (!(oicr & PFINT_OICR_INTEVENT_M)) {
		PMD_DRV_LOG(INFO, "No interrupt event");
		goto done;
	}

#ifdef ICE_LSE_SPT
	if (int_fw_ctl & PFINT_FW_CTL_INTEVENT_M) {
		PMD_DRV_LOG(INFO, "FW_CTL: link state change event");
		ice_handle_aq_msg(dev);
	}
#else
	if (oicr & PFINT_OICR_LINK_STAT_CHANGE_M) {
		PMD_DRV_LOG(INFO, "OICR: link state change event");
		ret = ice_link_update(dev, 0);
		if (!ret)
			rte_eth_dev_callback_process
				(dev, RTE_ETH_EVENT_INTR_LSC, NULL);
	}
#endif

	if (oicr & PFINT_OICR_MAL_DETECT_M) {
		PMD_DRV_LOG(WARNING, "OICR: MDD event");
		reg = ICE_READ_REG(hw, GL_MDET_TX_PQM);
		if (reg & GL_MDET_TX_PQM_VALID_M) {
			pf_num = (reg & GL_MDET_TX_PQM_PF_NUM_M) >>
				 GL_MDET_TX_PQM_PF_NUM_S;
			event = (reg & GL_MDET_TX_PQM_MAL_TYPE_M) >>
				GL_MDET_TX_PQM_MAL_TYPE_S;
			queue = (reg & GL_MDET_TX_PQM_QNUM_M) >>
				GL_MDET_TX_PQM_QNUM_S;

			PMD_DRV_LOG(WARNING, "Malicious Driver Detection event "
				    "%d by PQM on TX queue %d PF# %d",
				    event, queue, pf_num);
		}

		reg = ICE_READ_REG(hw, GL_MDET_TX_TCLAN);
		if (reg & GL_MDET_TX_TCLAN_VALID_M) {
			pf_num = (reg & GL_MDET_TX_TCLAN_PF_NUM_M) >>
				 GL_MDET_TX_TCLAN_PF_NUM_S;
			event = (reg & GL_MDET_TX_TCLAN_MAL_TYPE_M) >>
				GL_MDET_TX_TCLAN_MAL_TYPE_S;
			queue = (reg & GL_MDET_TX_TCLAN_QNUM_M) >>
				GL_MDET_TX_TCLAN_QNUM_S;

			PMD_DRV_LOG(WARNING, "Malicious Driver Detection event "
				    "%d by TCLAN on TX queue %d PF# %d",
				    event, queue, pf_num);
		}
	}
done:
	/* Enable interrupt */
	ice_pf_enable_irq0(hw);
	rte_intr_ack(dev->intr_handle);
}

static void
ice_init_proto_xtr(struct rte_eth_dev *dev)
{
	struct ice_adapter *ad =
			ICE_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	const struct proto_xtr_ol_flag *ol_flag;
	bool proto_xtr_enable = false;
	int offset;
	uint16_t i;

	pf->proto_xtr = rte_zmalloc(NULL, pf->lan_nb_qps, 0);
	if (unlikely(pf->proto_xtr == NULL)) {
		PMD_DRV_LOG(ERR, "No memory for setting up protocol extraction table");
		return;
	}

	for (i = 0; i < pf->lan_nb_qps; i++) {
		pf->proto_xtr[i] = ad->devargs.proto_xtr[i] != PROTO_XTR_NONE ?
				   ad->devargs.proto_xtr[i] :
				   ad->devargs.proto_xtr_dflt;

		if (pf->proto_xtr[i] != PROTO_XTR_NONE) {
			uint8_t type = pf->proto_xtr[i];

			ice_proto_xtr_ol_flag_params[type].required = true;
			proto_xtr_enable = true;
		}
	}

	if (likely(!proto_xtr_enable))
		return;

	ice_check_proto_xtr_support(hw);

	offset = rte_mbuf_dynfield_register(&ice_proto_xtr_metadata_param);
	if (unlikely(offset == -1)) {
		PMD_DRV_LOG(ERR,
			    "Protocol extraction metadata is disabled in mbuf with error %d",
			    -rte_errno);
		return;
	}

	PMD_DRV_LOG(DEBUG,
		    "Protocol extraction metadata offset in mbuf is : %d",
		    offset);
	rte_net_ice_dynfield_proto_xtr_metadata_offs = offset;

	for (i = 0; i < RTE_DIM(ice_proto_xtr_ol_flag_params); i++) {
		ol_flag = &ice_proto_xtr_ol_flag_params[i];

		if (!ol_flag->required)
			continue;

		if (!ice_proto_xtr_hw_support[i]) {
			PMD_DRV_LOG(ERR,
				    "Protocol extraction type %u is not supported in hardware",
				    i);
			rte_net_ice_dynfield_proto_xtr_metadata_offs = -1;
			break;
		}

		offset = rte_mbuf_dynflag_register(&ol_flag->param);
		if (unlikely(offset == -1)) {
			PMD_DRV_LOG(ERR,
				    "Protocol extraction offload '%s' failed to register with error %d",
				    ol_flag->param.name, -rte_errno);

			rte_net_ice_dynfield_proto_xtr_metadata_offs = -1;
			break;
		}

		PMD_DRV_LOG(DEBUG,
			    "Protocol extraction offload '%s' offset in mbuf is : %d",
			    ol_flag->param.name, offset);
		*ol_flag->ol_flag = 1ULL << offset;
	}
}

/*  Initialize SW parameters of PF */
static int
ice_pf_sw_init(struct rte_eth_dev *dev)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_hw *hw = ICE_PF_TO_HW(pf);

	pf->lan_nb_qp_max =
		(uint16_t)RTE_MIN(hw->func_caps.common_cap.num_txq,
				  hw->func_caps.common_cap.num_rxq);

	pf->lan_nb_qps = pf->lan_nb_qp_max;

	ice_init_proto_xtr(dev);

	if (hw->func_caps.fd_fltr_guar > 0 ||
	    hw->func_caps.fd_fltr_best_effort > 0) {
		pf->flags |= ICE_FLAG_FDIR;
		pf->fdir_nb_qps = ICE_DEFAULT_QP_NUM_FDIR;
		pf->lan_nb_qps = pf->lan_nb_qp_max - pf->fdir_nb_qps;
	} else {
		pf->fdir_nb_qps = 0;
	}
	pf->fdir_qp_offset = 0;

	return 0;
}

struct ice_vsi *
ice_setup_vsi(struct ice_pf *pf, enum ice_vsi_type type)
{
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	struct ice_vsi *vsi = NULL;
	struct ice_vsi_ctx vsi_ctx;
	int ret;
	struct rte_ether_addr broadcast = {
		.addr_bytes = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff} };
	struct rte_ether_addr mac_addr;
	uint16_t max_txqs[ICE_MAX_TRAFFIC_CLASS] = { 0 };
	uint8_t tc_bitmap = 0x1;
	uint16_t cfg;

	/* hw->num_lports = 1 in NIC mode */
	vsi = rte_zmalloc(NULL, sizeof(struct ice_vsi), 0);
	if (!vsi)
		return NULL;

	vsi->idx = pf->next_vsi_idx;
	pf->next_vsi_idx++;
	vsi->type = type;
	vsi->adapter = ICE_PF_TO_ADAPTER(pf);
	vsi->max_macaddrs = ICE_NUM_MACADDR_MAX;
	vsi->vlan_anti_spoof_on = 0;
	vsi->vlan_filter_on = 1;
	TAILQ_INIT(&vsi->mac_list);
	TAILQ_INIT(&vsi->vlan_list);

	/* Be sync with ETH_RSS_RETA_SIZE_x maximum value definition */
	pf->hash_lut_size = hw->func_caps.common_cap.rss_table_size >
			ETH_RSS_RETA_SIZE_512 ? ETH_RSS_RETA_SIZE_512 :
			hw->func_caps.common_cap.rss_table_size;
	pf->flags |= ICE_FLAG_RSS_AQ_CAPABLE;

	memset(&vsi_ctx, 0, sizeof(vsi_ctx));
	switch (type) {
	case ICE_VSI_PF:
		vsi->nb_qps = pf->lan_nb_qps;
		vsi->base_queue = 1;
		ice_vsi_config_default_rss(&vsi_ctx.info);
		vsi_ctx.alloc_from_pool = true;
		vsi_ctx.flags = ICE_AQ_VSI_TYPE_PF;
		/* switch_id is queried by get_switch_config aq, which is done
		 * by ice_init_hw
		 */
		vsi_ctx.info.sw_id = hw->port_info->sw_id;
		vsi_ctx.info.sw_flags2 = ICE_AQ_VSI_SW_FLAG_LAN_ENA;
		/* Allow all untagged or tagged packets */
		vsi_ctx.info.vlan_flags = ICE_AQ_VSI_VLAN_MODE_ALL;
		vsi_ctx.info.vlan_flags |= ICE_AQ_VSI_VLAN_EMOD_NOTHING;
		vsi_ctx.info.q_opt_rss = ICE_AQ_VSI_Q_OPT_RSS_LUT_PF |
					 ICE_AQ_VSI_Q_OPT_RSS_TPLZ;

		/* FDIR */
		cfg = ICE_AQ_VSI_PROP_SECURITY_VALID |
			ICE_AQ_VSI_PROP_FLOW_DIR_VALID;
		vsi_ctx.info.valid_sections |= rte_cpu_to_le_16(cfg);
		cfg = ICE_AQ_VSI_FD_ENABLE;
		vsi_ctx.info.fd_options = rte_cpu_to_le_16(cfg);
		vsi_ctx.info.max_fd_fltr_dedicated =
			rte_cpu_to_le_16(hw->func_caps.fd_fltr_guar);
		vsi_ctx.info.max_fd_fltr_shared =
			rte_cpu_to_le_16(hw->func_caps.fd_fltr_best_effort);

		/* Enable VLAN/UP trip */
		ret = ice_vsi_config_tc_queue_mapping(vsi,
						      &vsi_ctx.info,
						      ICE_DEFAULT_TCMAP);
		if (ret) {
			PMD_INIT_LOG(ERR,
				     "tc queue mapping with vsi failed, "
				     "err = %d",
				     ret);
			goto fail_mem;
		}

		break;
	case ICE_VSI_CTRL:
		vsi->nb_qps = pf->fdir_nb_qps;
		vsi->base_queue = ICE_FDIR_QUEUE_ID;
		vsi_ctx.alloc_from_pool = true;
		vsi_ctx.flags = ICE_AQ_VSI_TYPE_PF;

		cfg = ICE_AQ_VSI_PROP_FLOW_DIR_VALID;
		vsi_ctx.info.valid_sections |= rte_cpu_to_le_16(cfg);
		cfg = ICE_AQ_VSI_FD_PROG_ENABLE;
		vsi_ctx.info.fd_options = rte_cpu_to_le_16(cfg);
		vsi_ctx.info.sw_id = hw->port_info->sw_id;
		vsi_ctx.info.sw_flags2 = ICE_AQ_VSI_SW_FLAG_LAN_ENA;
		ret = ice_vsi_config_tc_queue_mapping(vsi,
						      &vsi_ctx.info,
						      ICE_DEFAULT_TCMAP);
		if (ret) {
			PMD_INIT_LOG(ERR,
				     "tc queue mapping with vsi failed, "
				     "err = %d",
				     ret);
			goto fail_mem;
		}
		break;
	default:
		/* for other types of VSI */
		PMD_INIT_LOG(ERR, "other types of VSI not supported");
		goto fail_mem;
	}

	/* VF has MSIX interrupt in VF range, don't allocate here */
	if (type == ICE_VSI_PF) {
		ret = ice_res_pool_alloc(&pf->msix_pool,
					 RTE_MIN(vsi->nb_qps,
						 RTE_MAX_RXTX_INTR_VEC_ID));
		if (ret < 0) {
			PMD_INIT_LOG(ERR, "VSI MAIN %d get heap failed %d",
				     vsi->vsi_id, ret);
		}
		vsi->msix_intr = ret;
		vsi->nb_msix = RTE_MIN(vsi->nb_qps, RTE_MAX_RXTX_INTR_VEC_ID);
	} else if (type == ICE_VSI_CTRL) {
		ret = ice_res_pool_alloc(&pf->msix_pool, 1);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "VSI %d get heap failed %d",
				    vsi->vsi_id, ret);
		}
		vsi->msix_intr = ret;
		vsi->nb_msix = 1;
	} else {
		vsi->msix_intr = 0;
		vsi->nb_msix = 0;
	}
	ret = ice_add_vsi(hw, vsi->idx, &vsi_ctx, NULL);
	if (ret != ICE_SUCCESS) {
		PMD_INIT_LOG(ERR, "add vsi failed, err = %d", ret);
		goto fail_mem;
	}
	/* store vsi information is SW structure */
	vsi->vsi_id = vsi_ctx.vsi_num;
	vsi->info = vsi_ctx.info;
	pf->vsis_allocated = vsi_ctx.vsis_allocd;
	pf->vsis_unallocated = vsi_ctx.vsis_unallocated;

	if (type == ICE_VSI_PF) {
		/* MAC configuration */
		rte_ether_addr_copy((struct rte_ether_addr *)
					hw->port_info->mac.perm_addr,
				    &pf->dev_addr);

		rte_ether_addr_copy(&pf->dev_addr, &mac_addr);
		ret = ice_add_mac_filter(vsi, &mac_addr);
		if (ret != ICE_SUCCESS)
			PMD_INIT_LOG(ERR, "Failed to add dflt MAC filter");

		rte_ether_addr_copy(&broadcast, &mac_addr);
		ret = ice_add_mac_filter(vsi, &mac_addr);
		if (ret != ICE_SUCCESS)
			PMD_INIT_LOG(ERR, "Failed to add MAC filter");
	}

	/* At the beginning, only TC0. */
	/* What we need here is the maximam number of the TX queues.
	 * Currently vsi->nb_qps means it.
	 * Correct it if any change.
	 */
	max_txqs[0] = vsi->nb_qps;
	ret = ice_cfg_vsi_lan(hw->port_info, vsi->idx,
			      tc_bitmap, max_txqs);
	if (ret != ICE_SUCCESS)
		PMD_INIT_LOG(ERR, "Failed to config vsi sched");

	return vsi;
fail_mem:
	rte_free(vsi);
	pf->next_vsi_idx--;
	return NULL;
}

static int
ice_send_driver_ver(struct ice_hw *hw)
{
	struct ice_driver_ver dv;

	/* we don't have driver version use 0 for dummy */
	dv.major_ver = 0;
	dv.minor_ver = 0;
	dv.build_ver = 0;
	dv.subbuild_ver = 0;
	strncpy((char *)dv.driver_string, "dpdk", sizeof(dv.driver_string));

	return ice_aq_send_driver_ver(hw, &dv, NULL);
}

static int
ice_pf_setup(struct ice_pf *pf)
{
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	struct ice_vsi *vsi;
	uint16_t unused;

	/* Clear all stats counters */
	pf->offset_loaded = false;
	memset(&pf->stats, 0, sizeof(struct ice_hw_port_stats));
	memset(&pf->stats_offset, 0, sizeof(struct ice_hw_port_stats));
	memset(&pf->internal_stats, 0, sizeof(struct ice_eth_stats));
	memset(&pf->internal_stats_offset, 0, sizeof(struct ice_eth_stats));

	/* force guaranteed filter pool for PF */
	ice_alloc_fd_guar_item(hw, &unused,
			       hw->func_caps.fd_fltr_guar);
	/* force shared filter pool for PF */
	ice_alloc_fd_shrd_item(hw, &unused,
			       hw->func_caps.fd_fltr_best_effort);

	vsi = ice_setup_vsi(pf, ICE_VSI_PF);
	if (!vsi) {
		PMD_INIT_LOG(ERR, "Failed to add vsi for PF");
		return -EINVAL;
	}

	pf->main_vsi = vsi;

	return 0;
}

/*
 * Extract device serial number from PCIe Configuration Space and
 * determine the pkg file path according to the DSN.
 */
static int
ice_pkg_file_search_path(struct rte_pci_device *pci_dev, char *pkg_file)
{
	off_t pos;
	char opt_ddp_filename[ICE_MAX_PKG_FILENAME_SIZE];
	uint32_t dsn_low, dsn_high;
	memset(opt_ddp_filename, 0, ICE_MAX_PKG_FILENAME_SIZE);

	pos = rte_pci_find_ext_capability(pci_dev, RTE_PCI_EXT_CAP_ID_DSN);

	if (pos) {
		if (rte_pci_read_config(pci_dev, &dsn_low, 4, pos + 4) < 0) {
			PMD_INIT_LOG(ERR, "Failed to read pci config space\n");
			return -1;
		}
		if (rte_pci_read_config(pci_dev, &dsn_high, 4, pos + 8) < 0) {
			PMD_INIT_LOG(ERR, "Failed to read pci config space\n");
			return -1;
		}
		snprintf(opt_ddp_filename, ICE_MAX_PKG_FILENAME_SIZE,
			 "ice-%08x%08x.pkg", dsn_high, dsn_low);
	} else {
		PMD_INIT_LOG(ERR, "Failed to read device serial number\n");
		goto fail_dsn;
	}

	strncpy(pkg_file, ICE_PKG_FILE_SEARCH_PATH_UPDATES,
		ICE_MAX_PKG_FILENAME_SIZE);
	if (!access(strcat(pkg_file, opt_ddp_filename), 0))
		return 0;

	strncpy(pkg_file, ICE_PKG_FILE_SEARCH_PATH_DEFAULT,
		ICE_MAX_PKG_FILENAME_SIZE);
	if (!access(strcat(pkg_file, opt_ddp_filename), 0))
		return 0;

fail_dsn:
	strncpy(pkg_file, ICE_PKG_FILE_UPDATES, ICE_MAX_PKG_FILENAME_SIZE);
	if (!access(pkg_file, 0))
		return 0;
	strncpy(pkg_file, ICE_PKG_FILE_DEFAULT, ICE_MAX_PKG_FILENAME_SIZE);
	return 0;
}

enum ice_pkg_type
ice_load_pkg_type(struct ice_hw *hw)
{
	enum ice_pkg_type package_type;

	/* store the activated package type (OS default or Comms) */
	if (!strncmp((char *)hw->active_pkg_name, ICE_OS_DEFAULT_PKG_NAME,
		ICE_PKG_NAME_SIZE))
		package_type = ICE_PKG_TYPE_OS_DEFAULT;
	else if (!strncmp((char *)hw->active_pkg_name, ICE_COMMS_PKG_NAME,
		ICE_PKG_NAME_SIZE))
		package_type = ICE_PKG_TYPE_COMMS;
	else
		package_type = ICE_PKG_TYPE_UNKNOWN;

	PMD_INIT_LOG(NOTICE, "Active package is: %d.%d.%d.%d, %s",
		hw->active_pkg_ver.major, hw->active_pkg_ver.minor,
		hw->active_pkg_ver.update, hw->active_pkg_ver.draft,
		hw->active_pkg_name);

	return package_type;
}

static int ice_load_pkg(struct rte_eth_dev *dev)
{
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	char pkg_file[ICE_MAX_PKG_FILENAME_SIZE];
	int err;
	uint8_t *buf;
	int buf_len;
	FILE *file;
	struct stat fstat;
	struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI(dev->device);
	struct ice_adapter *ad =
		ICE_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);

	err = ice_pkg_file_search_path(pci_dev, pkg_file);
	if (err) {
		PMD_INIT_LOG(ERR, "failed to search file path\n");
		return err;
	}

	file = fopen(pkg_file, "rb");
	if (!file)  {
		PMD_INIT_LOG(ERR, "failed to open file: %s\n", pkg_file);
		return -1;
	}

	err = stat(pkg_file, &fstat);
	if (err) {
		PMD_INIT_LOG(ERR, "failed to get file stats\n");
		fclose(file);
		return err;
	}

	buf_len = fstat.st_size;
	buf = rte_malloc(NULL, buf_len, 0);

	if (!buf) {
		PMD_INIT_LOG(ERR, "failed to allocate buf of size %d for package\n",
				buf_len);
		fclose(file);
		return -1;
	}

	err = fread(buf, buf_len, 1, file);
	if (err != 1) {
		PMD_INIT_LOG(ERR, "failed to read package data\n");
		fclose(file);
		err = -1;
		goto fail_exit;
	}

	fclose(file);

	err = ice_copy_and_init_pkg(hw, buf, buf_len);
	if (err) {
		PMD_INIT_LOG(ERR, "ice_copy_and_init_hw failed: %d\n", err);
		goto fail_exit;
	}

	/* store the loaded pkg type info */
	ad->active_pkg_type = ice_load_pkg_type(hw);

	err = ice_init_hw_tbls(hw);
	if (err) {
		PMD_INIT_LOG(ERR, "ice_init_hw_tbls failed: %d\n", err);
		goto fail_init_tbls;
	}

	return 0;

fail_init_tbls:
	rte_free(hw->pkg_copy);
fail_exit:
	rte_free(buf);
	return err;
}

static void
ice_base_queue_get(struct ice_pf *pf)
{
	uint32_t reg;
	struct ice_hw *hw = ICE_PF_TO_HW(pf);

	reg = ICE_READ_REG(hw, PFLAN_RX_QALLOC);
	if (reg & PFLAN_RX_QALLOC_VALID_M) {
		pf->base_queue = reg & PFLAN_RX_QALLOC_FIRSTQ_M;
	} else {
		PMD_INIT_LOG(WARNING, "Failed to get Rx base queue"
					" index");
	}
}

static int
parse_bool(const char *key, const char *value, void *args)
{
	int *i = (int *)args;
	char *end;
	int num;

	num = strtoul(value, &end, 10);

	if (num != 0 && num != 1) {
		PMD_DRV_LOG(WARNING, "invalid value:\"%s\" for key:\"%s\", "
			"value must be 0 or 1",
			value, key);
		return -1;
	}

	*i = num;
	return 0;
}

static int ice_parse_devargs(struct rte_eth_dev *dev)
{
	struct ice_adapter *ad =
		ICE_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct rte_devargs *devargs = dev->device->devargs;
	struct rte_kvargs *kvlist;
	int ret;

	if (devargs == NULL)
		return 0;

	kvlist = rte_kvargs_parse(devargs->args, ice_valid_args);
	if (kvlist == NULL) {
		PMD_INIT_LOG(ERR, "Invalid kvargs key\n");
		return -EINVAL;
	}

	ad->devargs.proto_xtr_dflt = PROTO_XTR_NONE;
	memset(ad->devargs.proto_xtr, PROTO_XTR_NONE,
	       sizeof(ad->devargs.proto_xtr));

	ret = rte_kvargs_process(kvlist, ICE_PROTO_XTR_ARG,
				 &handle_proto_xtr_arg, &ad->devargs);
	if (ret)
		goto bail;

	ret = rte_kvargs_process(kvlist, ICE_SAFE_MODE_SUPPORT_ARG,
				 &parse_bool, &ad->devargs.safe_mode_support);
	if (ret)
		goto bail;

	ret = rte_kvargs_process(kvlist, ICE_PIPELINE_MODE_SUPPORT_ARG,
				 &parse_bool, &ad->devargs.pipe_mode_support);
	if (ret)
		goto bail;

bail:
	rte_kvargs_free(kvlist);
	return ret;
}

/* Forward LLDP packets to default VSI by set switch rules */
static int
ice_vsi_config_sw_lldp(struct ice_vsi *vsi,  bool on)
{
	struct ice_hw *hw = ICE_VSI_TO_HW(vsi);
	struct ice_fltr_list_entry *s_list_itr = NULL;
	struct LIST_HEAD_TYPE list_head;
	int ret = 0;

	INIT_LIST_HEAD(&list_head);

	s_list_itr = (struct ice_fltr_list_entry *)
			ice_malloc(hw, sizeof(*s_list_itr));
	if (!s_list_itr)
		return -ENOMEM;
	s_list_itr->fltr_info.lkup_type = ICE_SW_LKUP_ETHERTYPE;
	s_list_itr->fltr_info.vsi_handle = vsi->idx;
	s_list_itr->fltr_info.l_data.ethertype_mac.ethertype =
			RTE_ETHER_TYPE_LLDP;
	s_list_itr->fltr_info.fltr_act = ICE_FWD_TO_VSI;
	s_list_itr->fltr_info.flag = ICE_FLTR_RX;
	s_list_itr->fltr_info.src_id = ICE_SRC_ID_LPORT;
	LIST_ADD(&s_list_itr->list_entry, &list_head);
	if (on)
		ret = ice_add_eth_mac(hw, &list_head);
	else
		ret = ice_remove_eth_mac(hw, &list_head);

	rte_free(s_list_itr);
	return ret;
}

static enum ice_status
ice_get_hw_res(struct ice_hw *hw, uint16_t res_type,
		uint16_t num, uint16_t desc_id,
		uint16_t *prof_buf, uint16_t *num_prof)
{
	struct ice_aqc_res_elem *resp_buf;
	int ret;
	uint16_t buf_len;
	bool res_shared = 1;
	struct ice_aq_desc aq_desc;
	struct ice_sq_cd *cd = NULL;
	struct ice_aqc_get_allocd_res_desc *cmd =
			&aq_desc.params.get_res_desc;

	buf_len = sizeof(*resp_buf) * num;
	resp_buf = ice_malloc(hw, buf_len);
	if (!resp_buf)
		return -ENOMEM;

	ice_fill_dflt_direct_cmd_desc(&aq_desc,
			ice_aqc_opc_get_allocd_res_desc);

	cmd->ops.cmd.res = CPU_TO_LE16(((res_type << ICE_AQC_RES_TYPE_S) &
				ICE_AQC_RES_TYPE_M) | (res_shared ?
				ICE_AQC_RES_TYPE_FLAG_SHARED : 0));
	cmd->ops.cmd.first_desc = CPU_TO_LE16(desc_id);

	ret = ice_aq_send_cmd(hw, &aq_desc, resp_buf, buf_len, cd);
	if (!ret)
		*num_prof = LE16_TO_CPU(cmd->ops.resp.num_desc);
	else
		goto exit;

	ice_memcpy(prof_buf, resp_buf, sizeof(*resp_buf) *
			(*num_prof), ICE_NONDMA_TO_NONDMA);

exit:
	rte_free(resp_buf);
	return ret;
}
static int
ice_cleanup_resource(struct ice_hw *hw, uint16_t res_type)
{
	int ret;
	uint16_t prof_id;
	uint16_t prof_buf[ICE_MAX_RES_DESC_NUM];
	uint16_t first_desc = 1;
	uint16_t num_prof = 0;

	ret = ice_get_hw_res(hw, res_type, ICE_MAX_RES_DESC_NUM,
			first_desc, prof_buf, &num_prof);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to get fxp resource");
		return ret;
	}

	for (prof_id = 0; prof_id < num_prof; prof_id++) {
		ret = ice_free_hw_res(hw, res_type, 1, &prof_buf[prof_id]);
		if (ret) {
			PMD_INIT_LOG(ERR, "Failed to free fxp resource");
			return ret;
		}
	}
	return 0;
}

static int
ice_reset_fxp_resource(struct ice_hw *hw)
{
	int ret;

	ret = ice_cleanup_resource(hw, ICE_AQC_RES_TYPE_FD_PROF_BLDR_PROFID);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to clearup fdir resource");
		return ret;
	}

	ret = ice_cleanup_resource(hw, ICE_AQC_RES_TYPE_HASH_PROF_BLDR_PROFID);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to clearup rss resource");
		return ret;
	}

	return 0;
}

static void
ice_rss_ctx_init(struct ice_pf *pf)
{
	memset(&pf->hash_ctx, 0, sizeof(pf->hash_ctx));
}

static uint64_t
ice_get_supported_rxdid(struct ice_hw *hw)
{
	uint64_t supported_rxdid = 0; /* bitmap for supported RXDID */
	uint32_t regval;
	int i;

	supported_rxdid |= BIT(ICE_RXDID_LEGACY_1);

	for (i = ICE_RXDID_FLEX_NIC; i < ICE_FLEX_DESC_RXDID_MAX_NUM; i++) {
		regval = ICE_READ_REG(hw, GLFLXP_RXDID_FLAGS(i, 0));
		if ((regval >> GLFLXP_RXDID_FLAGS_FLEXIFLAG_4N_S)
			& GLFLXP_RXDID_FLAGS_FLEXIFLAG_4N_M)
			supported_rxdid |= BIT(i);
	}
	return supported_rxdid;
}

static int
ice_dev_init(struct rte_eth_dev *dev)
{
	struct rte_pci_device *pci_dev;
	struct rte_intr_handle *intr_handle;
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_adapter *ad =
		ICE_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct ice_vsi *vsi;
	int ret;

	dev->dev_ops = &ice_eth_dev_ops;
	dev->rx_queue_count = ice_rx_queue_count;
	dev->rx_descriptor_status = ice_rx_descriptor_status;
	dev->tx_descriptor_status = ice_tx_descriptor_status;
	dev->rx_pkt_burst = ice_recv_pkts;
	dev->tx_pkt_burst = ice_xmit_pkts;
	dev->tx_pkt_prepare = ice_prep_pkts;

	/* for secondary processes, we don't initialise any further as primary
	 * has already done this work.
	 */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		ice_set_rx_function(dev);
		ice_set_tx_function(dev);
		return 0;
	}

	dev->data->dev_flags |= RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS;

	ice_set_default_ptype_table(dev);
	pci_dev = RTE_DEV_TO_PCI(dev->device);
	intr_handle = &pci_dev->intr_handle;

	pf->adapter = ICE_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	pf->dev_data = dev->data;
	hw->back = pf->adapter;
	hw->hw_addr = (uint8_t *)pci_dev->mem_resource[0].addr;
	hw->vendor_id = pci_dev->id.vendor_id;
	hw->device_id = pci_dev->id.device_id;
	hw->subsystem_vendor_id = pci_dev->id.subsystem_vendor_id;
	hw->subsystem_device_id = pci_dev->id.subsystem_device_id;
	hw->bus.device = pci_dev->addr.devid;
	hw->bus.func = pci_dev->addr.function;

	ret = ice_parse_devargs(dev);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to parse devargs");
		return -EINVAL;
	}

	ice_init_controlq_parameter(hw);

	ret = ice_init_hw(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to initialize HW");
		return -EINVAL;
	}

	ret = ice_load_pkg(dev);
	if (ret) {
		if (ad->devargs.safe_mode_support == 0) {
			PMD_INIT_LOG(ERR, "Failed to load the DDP package,"
					"Use safe-mode-support=1 to enter Safe Mode");
			goto err_init_fw;
		}

		PMD_INIT_LOG(WARNING, "Failed to load the DDP package,"
					"Entering Safe Mode");
		ad->is_safe_mode = 1;
	}

	PMD_INIT_LOG(INFO, "FW %d.%d.%05d API %d.%d",
		     hw->fw_maj_ver, hw->fw_min_ver, hw->fw_build,
		     hw->api_maj_ver, hw->api_min_ver);

	ice_pf_sw_init(dev);
	ret = ice_init_mac_address(dev);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to initialize mac address");
		goto err_init_mac;
	}

	ret = ice_res_pool_init(&pf->msix_pool, 1,
				hw->func_caps.common_cap.num_msix_vectors - 1);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to init MSIX pool");
		goto err_msix_pool_init;
	}

	ret = ice_pf_setup(pf);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to setup PF");
		goto err_pf_setup;
	}

	ret = ice_send_driver_ver(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to send driver version");
		goto err_pf_setup;
	}

	vsi = pf->main_vsi;

	/* Disable double vlan by default */
	ice_vsi_config_double_vlan(vsi, false);

	ret = ice_aq_stop_lldp(hw, true, false, NULL);
	if (ret != ICE_SUCCESS)
		PMD_INIT_LOG(DEBUG, "lldp has already stopped\n");
	ret = ice_init_dcb(hw, true);
	if (ret != ICE_SUCCESS)
		PMD_INIT_LOG(DEBUG, "Failed to init DCB\n");
	/* Forward LLDP packets to default VSI */
	ret = ice_vsi_config_sw_lldp(vsi, true);
	if (ret != ICE_SUCCESS)
		PMD_INIT_LOG(DEBUG, "Failed to cfg lldp\n");
	/* register callback func to eal lib */
	rte_intr_callback_register(intr_handle,
				   ice_interrupt_handler, dev);

	ice_pf_enable_irq0(hw);

	/* enable uio intr after callback register */
	rte_intr_enable(intr_handle);

	/* get base queue pairs index  in the device */
	ice_base_queue_get(pf);

	/* Initialize RSS context for gtpu_eh */
	ice_rss_ctx_init(pf);

	if (!ad->is_safe_mode) {
		ret = ice_flow_init(ad);
		if (ret) {
			PMD_INIT_LOG(ERR, "Failed to initialize flow");
			return ret;
		}
	}

	ret = ice_reset_fxp_resource(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to reset fxp resource");
		return ret;
	}

	pf->supported_rxdid = ice_get_supported_rxdid(hw);

	return 0;

err_pf_setup:
	ice_res_pool_destroy(&pf->msix_pool);
err_msix_pool_init:
	rte_free(dev->data->mac_addrs);
	dev->data->mac_addrs = NULL;
err_init_mac:
	rte_free(pf->proto_xtr);
#ifndef RTE_EXEC_ENV_WINDOWS
err_init_fw:
#endif
	ice_deinit_hw(hw);

	return ret;
}

int
ice_release_vsi(struct ice_vsi *vsi)
{
	struct ice_hw *hw;
	struct ice_vsi_ctx vsi_ctx;
	enum ice_status ret;
	int error = 0;

	if (!vsi)
		return error;

	hw = ICE_VSI_TO_HW(vsi);

	ice_remove_all_mac_vlan_filters(vsi);

	memset(&vsi_ctx, 0, sizeof(vsi_ctx));

	vsi_ctx.vsi_num = vsi->vsi_id;
	vsi_ctx.info = vsi->info;
	ret = ice_free_vsi(hw, vsi->idx, &vsi_ctx, false, NULL);
	if (ret != ICE_SUCCESS) {
		PMD_INIT_LOG(ERR, "Failed to free vsi by aq, %u", vsi->vsi_id);
		error = -1;
	}

	rte_free(vsi->rss_lut);
	rte_free(vsi->rss_key);
	rte_free(vsi);
	return error;
}

void
ice_vsi_disable_queues_intr(struct ice_vsi *vsi)
{
	struct rte_eth_dev *dev = &rte_eth_devices[vsi->adapter->pf.dev_data->port_id];
	struct rte_pci_device *pci_dev = ICE_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	struct ice_hw *hw = ICE_VSI_TO_HW(vsi);
	uint16_t msix_intr, i;

	/* disable interrupt and also clear all the exist config */
	for (i = 0; i < vsi->nb_qps; i++) {
		ICE_WRITE_REG(hw, QINT_TQCTL(vsi->base_queue + i), 0);
		ICE_WRITE_REG(hw, QINT_RQCTL(vsi->base_queue + i), 0);
		rte_wmb();
	}

	if (rte_intr_allow_others(intr_handle))
		/* vfio-pci */
		for (i = 0; i < vsi->nb_msix; i++) {
			msix_intr = vsi->msix_intr + i;
			ICE_WRITE_REG(hw, GLINT_DYN_CTL(msix_intr),
				      GLINT_DYN_CTL_WB_ON_ITR_M);
		}
	else
		/* igb_uio */
		ICE_WRITE_REG(hw, GLINT_DYN_CTL(0), GLINT_DYN_CTL_WB_ON_ITR_M);
}

static int
ice_dev_stop(struct rte_eth_dev *dev)
{
	struct rte_eth_dev_data *data = dev->data;
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_vsi *main_vsi = pf->main_vsi;
	struct rte_pci_device *pci_dev = ICE_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	uint16_t i;

	/* avoid stopping again */
	if (pf->adapter_stopped)
		return 0;

	/* stop and clear all Rx queues */
	for (i = 0; i < data->nb_rx_queues; i++)
		ice_rx_queue_stop(dev, i);

	/* stop and clear all Tx queues */
	for (i = 0; i < data->nb_tx_queues; i++)
		ice_tx_queue_stop(dev, i);

	/* disable all queue interrupts */
	ice_vsi_disable_queues_intr(main_vsi);

	if (pf->init_link_up)
		ice_dev_set_link_up(dev);
	else
		ice_dev_set_link_down(dev);

	/* Clean datapath event and queue/vec mapping */
	rte_intr_efd_disable(intr_handle);
	if (intr_handle->intr_vec) {
		rte_free(intr_handle->intr_vec);
		intr_handle->intr_vec = NULL;
	}

	pf->adapter_stopped = true;
	dev->data->dev_started = 0;

	return 0;
}

static int
ice_dev_close(struct rte_eth_dev *dev)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	struct ice_adapter *ad =
		ICE_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	int ret;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	/* Since stop will make link down, then the link event will be
	 * triggered, disable the irq firstly to avoid the port_infoe etc
	 * resources deallocation causing the interrupt service thread
	 * crash.
	 */
	ice_pf_disable_irq0(hw);

	ret = ice_dev_stop(dev);

	if (!ad->is_safe_mode)
		ice_flow_uninit(ad);

	/* release all queue resource */
	ice_free_queues(dev);

	ice_res_pool_destroy(&pf->msix_pool);
	ice_release_vsi(pf->main_vsi);
	ice_sched_cleanup_all(hw);
	ice_free_hw_tbls(hw);
	rte_free(hw->port_info);
	hw->port_info = NULL;
	ice_shutdown_all_ctrlq(hw);
	rte_free(pf->proto_xtr);
	pf->proto_xtr = NULL;

	/* disable uio intr before callback unregister */
	rte_intr_disable(intr_handle);

	/* unregister callback func from eal lib */
	rte_intr_callback_unregister(intr_handle,
				     ice_interrupt_handler, dev);

	return ret;
}

static int
ice_dev_uninit(struct rte_eth_dev *dev)
{
	ice_dev_close(dev);

	return 0;
}

static bool
is_hash_cfg_valid(struct ice_rss_hash_cfg *cfg)
{
	return (cfg->hash_flds != 0 && cfg->addl_hdrs != 0) ? true : false;
}

static void
hash_cfg_reset(struct ice_rss_hash_cfg *cfg)
{
	cfg->hash_flds = 0;
	cfg->addl_hdrs = 0;
	cfg->symm = 0;
	cfg->hdr_type = ICE_RSS_ANY_HEADERS;
}

static int
ice_hash_moveout(struct ice_pf *pf, struct ice_rss_hash_cfg *cfg)
{
	enum ice_status status = ICE_SUCCESS;
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	struct ice_vsi *vsi = pf->main_vsi;

	if (!is_hash_cfg_valid(cfg))
		return -ENOENT;

	status = ice_rem_rss_cfg(hw, vsi->idx, cfg);
	if (status && status != ICE_ERR_DOES_NOT_EXIST) {
		PMD_DRV_LOG(ERR,
			    "ice_rem_rss_cfg failed for VSI:%d, error:%d\n",
			    vsi->idx, status);
		return -EBUSY;
	}

	return 0;
}

static int
ice_hash_moveback(struct ice_pf *pf, struct ice_rss_hash_cfg *cfg)
{
	enum ice_status status = ICE_SUCCESS;
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	struct ice_vsi *vsi = pf->main_vsi;

	if (!is_hash_cfg_valid(cfg))
		return -ENOENT;

	status = ice_add_rss_cfg(hw, vsi->idx, cfg);
	if (status) {
		PMD_DRV_LOG(ERR,
			    "ice_add_rss_cfg failed for VSI:%d, error:%d\n",
			    vsi->idx, status);
		return -EBUSY;
	}

	return 0;
}

static int
ice_hash_remove(struct ice_pf *pf, struct ice_rss_hash_cfg *cfg)
{
	int ret;

	ret = ice_hash_moveout(pf, cfg);
	if (ret && (ret != -ENOENT))
		return ret;

	hash_cfg_reset(cfg);

	return 0;
}

static int
ice_add_rss_cfg_pre_gtpu(struct ice_pf *pf, struct ice_hash_gtpu_ctx *ctx,
			 u8 ctx_idx)
{
	int ret;

	switch (ctx_idx) {
	case ICE_HASH_GTPU_CTX_EH_IP:
		ret = ice_hash_remove(pf,
				      &ctx->ctx[ICE_HASH_GTPU_CTX_EH_IP_UDP]);
		if (ret && (ret != -ENOENT))
			return ret;

		ret = ice_hash_remove(pf,
				      &ctx->ctx[ICE_HASH_GTPU_CTX_EH_IP_TCP]);
		if (ret && (ret != -ENOENT))
			return ret;

		ret = ice_hash_remove(pf,
				      &ctx->ctx[ICE_HASH_GTPU_CTX_UP_IP]);
		if (ret && (ret != -ENOENT))
			return ret;

		ret = ice_hash_remove(pf,
				      &ctx->ctx[ICE_HASH_GTPU_CTX_UP_IP_UDP]);
		if (ret && (ret != -ENOENT))
			return ret;

		ret = ice_hash_remove(pf,
				      &ctx->ctx[ICE_HASH_GTPU_CTX_UP_IP_TCP]);
		if (ret && (ret != -ENOENT))
			return ret;

		ret = ice_hash_remove(pf,
				      &ctx->ctx[ICE_HASH_GTPU_CTX_DW_IP]);
		if (ret && (ret != -ENOENT))
			return ret;

		ret = ice_hash_remove(pf,
				      &ctx->ctx[ICE_HASH_GTPU_CTX_DW_IP_UDP]);
		if (ret && (ret != -ENOENT))
			return ret;

		ret = ice_hash_remove(pf,
				      &ctx->ctx[ICE_HASH_GTPU_CTX_DW_IP_TCP]);
		if (ret && (ret != -ENOENT))
			return ret;

		break;
	case ICE_HASH_GTPU_CTX_EH_IP_UDP:
		ret = ice_hash_remove(pf,
				      &ctx->ctx[ICE_HASH_GTPU_CTX_UP_IP_UDP]);
		if (ret && (ret != -ENOENT))
			return ret;

		ret = ice_hash_remove(pf,
				      &ctx->ctx[ICE_HASH_GTPU_CTX_DW_IP_UDP]);
		if (ret && (ret != -ENOENT))
			return ret;

		ret = ice_hash_moveout(pf,
				       &ctx->ctx[ICE_HASH_GTPU_CTX_UP_IP]);
		if (ret && (ret != -ENOENT))
			return ret;

		ret = ice_hash_moveout(pf,
				       &ctx->ctx[ICE_HASH_GTPU_CTX_UP_IP_TCP]);
		if (ret && (ret != -ENOENT))
			return ret;

		ret = ice_hash_moveout(pf,
				       &ctx->ctx[ICE_HASH_GTPU_CTX_DW_IP]);
		if (ret && (ret != -ENOENT))
			return ret;

		ret = ice_hash_moveout(pf,
				       &ctx->ctx[ICE_HASH_GTPU_CTX_DW_IP_TCP]);
		if (ret && (ret != -ENOENT))
			return ret;

		break;
	case ICE_HASH_GTPU_CTX_EH_IP_TCP:
		ret = ice_hash_remove(pf,
				      &ctx->ctx[ICE_HASH_GTPU_CTX_UP_IP_TCP]);
		if (ret && (ret != -ENOENT))
			return ret;

		ret = ice_hash_remove(pf,
				      &ctx->ctx[ICE_HASH_GTPU_CTX_DW_IP_TCP]);
		if (ret && (ret != -ENOENT))
			return ret;

		ret = ice_hash_moveout(pf,
				       &ctx->ctx[ICE_HASH_GTPU_CTX_UP_IP]);
		if (ret && (ret != -ENOENT))
			return ret;

		ret = ice_hash_moveout(pf,
				       &ctx->ctx[ICE_HASH_GTPU_CTX_UP_IP_UDP]);
		if (ret && (ret != -ENOENT))
			return ret;

		ret = ice_hash_moveout(pf,
				       &ctx->ctx[ICE_HASH_GTPU_CTX_DW_IP]);
		if (ret && (ret != -ENOENT))
			return ret;

		ret = ice_hash_moveout(pf,
				       &ctx->ctx[ICE_HASH_GTPU_CTX_DW_IP_UDP]);
		if (ret && (ret != -ENOENT))
			return ret;

		break;
	case ICE_HASH_GTPU_CTX_UP_IP:
		ret = ice_hash_remove(pf,
				      &ctx->ctx[ICE_HASH_GTPU_CTX_UP_IP_UDP]);
		if (ret && (ret != -ENOENT))
			return ret;

		ret = ice_hash_remove(pf,
				      &ctx->ctx[ICE_HASH_GTPU_CTX_UP_IP_TCP]);
		if (ret && (ret != -ENOENT))
			return ret;

		ret = ice_hash_moveout(pf,
				       &ctx->ctx[ICE_HASH_GTPU_CTX_EH_IP]);
		if (ret && (ret != -ENOENT))
			return ret;

		ret = ice_hash_moveout(pf,
				       &ctx->ctx[ICE_HASH_GTPU_CTX_EH_IP_UDP]);
		if (ret && (ret != -ENOENT))
			return ret;

		ret = ice_hash_moveout(pf,
				       &ctx->ctx[ICE_HASH_GTPU_CTX_EH_IP_TCP]);
		if (ret && (ret != -ENOENT))
			return ret;

		break;
	case ICE_HASH_GTPU_CTX_UP_IP_UDP:
	case ICE_HASH_GTPU_CTX_UP_IP_TCP:
		ret = ice_hash_moveout(pf,
				       &ctx->ctx[ICE_HASH_GTPU_CTX_EH_IP]);
		if (ret && (ret != -ENOENT))
			return ret;

		ret = ice_hash_moveout(pf,
				       &ctx->ctx[ICE_HASH_GTPU_CTX_EH_IP_UDP]);
		if (ret && (ret != -ENOENT))
			return ret;

		ret = ice_hash_moveout(pf,
				       &ctx->ctx[ICE_HASH_GTPU_CTX_EH_IP_TCP]);
		if (ret && (ret != -ENOENT))
			return ret;

		break;
	case ICE_HASH_GTPU_CTX_DW_IP:
		ret = ice_hash_remove(pf,
				      &ctx->ctx[ICE_HASH_GTPU_CTX_DW_IP_UDP]);
		if (ret && (ret != -ENOENT))
			return ret;

		ret = ice_hash_remove(pf,
				      &ctx->ctx[ICE_HASH_GTPU_CTX_DW_IP_TCP]);
		if (ret && (ret != -ENOENT))
			return ret;

		ret = ice_hash_moveout(pf,
				       &ctx->ctx[ICE_HASH_GTPU_CTX_EH_IP]);
		if (ret && (ret != -ENOENT))
			return ret;

		ret = ice_hash_moveout(pf,
				       &ctx->ctx[ICE_HASH_GTPU_CTX_EH_IP_UDP]);
		if (ret && (ret != -ENOENT))
			return ret;

		ret = ice_hash_moveout(pf,
				       &ctx->ctx[ICE_HASH_GTPU_CTX_EH_IP_TCP]);
		if (ret && (ret != -ENOENT))
			return ret;

		break;
	case ICE_HASH_GTPU_CTX_DW_IP_UDP:
	case ICE_HASH_GTPU_CTX_DW_IP_TCP:
		ret = ice_hash_moveout(pf,
				       &ctx->ctx[ICE_HASH_GTPU_CTX_EH_IP]);
		if (ret && (ret != -ENOENT))
			return ret;

		ret = ice_hash_moveout(pf,
				       &ctx->ctx[ICE_HASH_GTPU_CTX_EH_IP_UDP]);
		if (ret && (ret != -ENOENT))
			return ret;

		ret = ice_hash_moveout(pf,
				       &ctx->ctx[ICE_HASH_GTPU_CTX_EH_IP_TCP]);
		if (ret && (ret != -ENOENT))
			return ret;

		break;
	default:
		break;
	}

	return 0;
}

static u8 calc_gtpu_ctx_idx(uint32_t hdr)
{
	u8 eh_idx, ip_idx;

	if (hdr & ICE_FLOW_SEG_HDR_GTPU_EH)
		eh_idx = 0;
	else if (hdr & ICE_FLOW_SEG_HDR_GTPU_UP)
		eh_idx = 1;
	else if (hdr & ICE_FLOW_SEG_HDR_GTPU_DWN)
		eh_idx = 2;
	else
		return ICE_HASH_GTPU_CTX_MAX;

	ip_idx = 0;
	if (hdr & ICE_FLOW_SEG_HDR_UDP)
		ip_idx = 1;
	else if (hdr & ICE_FLOW_SEG_HDR_TCP)
		ip_idx = 2;

	if (hdr & (ICE_FLOW_SEG_HDR_IPV4 | ICE_FLOW_SEG_HDR_IPV6))
		return eh_idx * 3 + ip_idx;
	else
		return ICE_HASH_GTPU_CTX_MAX;
}

static int
ice_add_rss_cfg_pre(struct ice_pf *pf, uint32_t hdr)
{
	u8 gtpu_ctx_idx = calc_gtpu_ctx_idx(hdr);

	if (hdr & ICE_FLOW_SEG_HDR_IPV4)
		return ice_add_rss_cfg_pre_gtpu(pf, &pf->hash_ctx.gtpu4,
						gtpu_ctx_idx);
	else if (hdr & ICE_FLOW_SEG_HDR_IPV6)
		return ice_add_rss_cfg_pre_gtpu(pf, &pf->hash_ctx.gtpu6,
						gtpu_ctx_idx);

	return 0;
}

static int
ice_add_rss_cfg_post_gtpu(struct ice_pf *pf, struct ice_hash_gtpu_ctx *ctx,
			  u8 ctx_idx, struct ice_rss_hash_cfg *cfg)
{
	int ret;

	if (ctx_idx < ICE_HASH_GTPU_CTX_MAX)
		ctx->ctx[ctx_idx] = *cfg;

	switch (ctx_idx) {
	case ICE_HASH_GTPU_CTX_EH_IP:
		break;
	case ICE_HASH_GTPU_CTX_EH_IP_UDP:
		ret = ice_hash_moveback(pf,
					&ctx->ctx[ICE_HASH_GTPU_CTX_UP_IP]);
		if (ret && (ret != -ENOENT))
			return ret;

		ret = ice_hash_moveback(pf,
					&ctx->ctx[ICE_HASH_GTPU_CTX_UP_IP_TCP]);
		if (ret && (ret != -ENOENT))
			return ret;

		ret = ice_hash_moveback(pf,
					&ctx->ctx[ICE_HASH_GTPU_CTX_DW_IP]);
		if (ret && (ret != -ENOENT))
			return ret;

		ret = ice_hash_moveback(pf,
					&ctx->ctx[ICE_HASH_GTPU_CTX_DW_IP_TCP]);
		if (ret && (ret != -ENOENT))
			return ret;

		break;
	case ICE_HASH_GTPU_CTX_EH_IP_TCP:
		ret = ice_hash_moveback(pf,
					&ctx->ctx[ICE_HASH_GTPU_CTX_UP_IP]);
		if (ret && (ret != -ENOENT))
			return ret;

		ret = ice_hash_moveback(pf,
					&ctx->ctx[ICE_HASH_GTPU_CTX_UP_IP_UDP]);
		if (ret && (ret != -ENOENT))
			return ret;

		ret = ice_hash_moveback(pf,
					&ctx->ctx[ICE_HASH_GTPU_CTX_DW_IP]);
		if (ret && (ret != -ENOENT))
			return ret;

		ret = ice_hash_moveback(pf,
					&ctx->ctx[ICE_HASH_GTPU_CTX_DW_IP_UDP]);
		if (ret && (ret != -ENOENT))
			return ret;

		break;
	case ICE_HASH_GTPU_CTX_UP_IP:
	case ICE_HASH_GTPU_CTX_UP_IP_UDP:
	case ICE_HASH_GTPU_CTX_UP_IP_TCP:
	case ICE_HASH_GTPU_CTX_DW_IP:
	case ICE_HASH_GTPU_CTX_DW_IP_UDP:
	case ICE_HASH_GTPU_CTX_DW_IP_TCP:
		ret = ice_hash_moveback(pf,
					&ctx->ctx[ICE_HASH_GTPU_CTX_EH_IP]);
		if (ret && (ret != -ENOENT))
			return ret;

		ret = ice_hash_moveback(pf,
					&ctx->ctx[ICE_HASH_GTPU_CTX_EH_IP_UDP]);
		if (ret && (ret != -ENOENT))
			return ret;

		ret = ice_hash_moveback(pf,
					&ctx->ctx[ICE_HASH_GTPU_CTX_EH_IP_TCP]);
		if (ret && (ret != -ENOENT))
			return ret;

		break;
	default:
		break;
	}

	return 0;
}

static int
ice_add_rss_cfg_post(struct ice_pf *pf, struct ice_rss_hash_cfg *cfg)
{
	u8 gtpu_ctx_idx = calc_gtpu_ctx_idx(cfg->addl_hdrs);

	if (cfg->addl_hdrs & ICE_FLOW_SEG_HDR_IPV4)
		return ice_add_rss_cfg_post_gtpu(pf, &pf->hash_ctx.gtpu4,
						 gtpu_ctx_idx, cfg);
	else if (cfg->addl_hdrs & ICE_FLOW_SEG_HDR_IPV6)
		return ice_add_rss_cfg_post_gtpu(pf, &pf->hash_ctx.gtpu6,
						 gtpu_ctx_idx, cfg);

	return 0;
}

static void
ice_rem_rss_cfg_post(struct ice_pf *pf, uint32_t hdr)
{
	u8 gtpu_ctx_idx = calc_gtpu_ctx_idx(hdr);

	if (gtpu_ctx_idx >= ICE_HASH_GTPU_CTX_MAX)
		return;

	if (hdr & ICE_FLOW_SEG_HDR_IPV4)
		hash_cfg_reset(&pf->hash_ctx.gtpu4.ctx[gtpu_ctx_idx]);
	else if (hdr & ICE_FLOW_SEG_HDR_IPV6)
		hash_cfg_reset(&pf->hash_ctx.gtpu6.ctx[gtpu_ctx_idx]);
}

int
ice_rem_rss_cfg_wrap(struct ice_pf *pf, uint16_t vsi_id,
		     struct ice_rss_hash_cfg *cfg)
{
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	int ret;

	ret = ice_rem_rss_cfg(hw, vsi_id, cfg);
	if (ret && ret != ICE_ERR_DOES_NOT_EXIST)
		PMD_DRV_LOG(ERR, "remove rss cfg failed\n");

	ice_rem_rss_cfg_post(pf, cfg->addl_hdrs);

	return 0;
}

int
ice_add_rss_cfg_wrap(struct ice_pf *pf, uint16_t vsi_id,
		     struct ice_rss_hash_cfg *cfg)
{
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	int ret;

	ret = ice_add_rss_cfg_pre(pf, cfg->addl_hdrs);
	if (ret)
		PMD_DRV_LOG(ERR, "add rss cfg pre failed\n");

	ret = ice_add_rss_cfg(hw, vsi_id, cfg);
	if (ret)
		PMD_DRV_LOG(ERR, "add rss cfg failed\n");

	ret = ice_add_rss_cfg_post(pf, cfg);
	if (ret)
		PMD_DRV_LOG(ERR, "add rss cfg post failed\n");

	return 0;
}

static void
ice_rss_hash_set(struct ice_pf *pf, uint64_t rss_hf)
{
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	struct ice_vsi *vsi = pf->main_vsi;
	struct ice_rss_hash_cfg cfg;
	int ret;

#define ICE_RSS_HF_ALL ( \
	ETH_RSS_IPV4 | \
	ETH_RSS_IPV6 | \
	ETH_RSS_NONFRAG_IPV4_UDP | \
	ETH_RSS_NONFRAG_IPV6_UDP | \
	ETH_RSS_NONFRAG_IPV4_TCP | \
	ETH_RSS_NONFRAG_IPV6_TCP | \
	ETH_RSS_NONFRAG_IPV4_SCTP | \
	ETH_RSS_NONFRAG_IPV6_SCTP)

	ret = ice_rem_vsi_rss_cfg(hw, vsi->idx);
	if (ret)
		PMD_DRV_LOG(ERR, "%s Remove rss vsi fail %d",
			    __func__, ret);

	cfg.symm = 0;
	cfg.hdr_type = ICE_RSS_ANY_HEADERS;
	/* Configure RSS for IPv4 with src/dst addr as input set */
	if (rss_hf & ETH_RSS_IPV4) {
		cfg.addl_hdrs = ICE_FLOW_SEG_HDR_IPV4 | ICE_FLOW_SEG_HDR_IPV_OTHER;
		cfg.hash_flds = ICE_FLOW_HASH_IPV4;
		ret = ice_add_rss_cfg_wrap(pf, vsi->idx, &cfg);
		if (ret)
			PMD_DRV_LOG(ERR, "%s IPV4 rss flow fail %d",
				    __func__, ret);
	}

	/* Configure RSS for IPv6 with src/dst addr as input set */
	if (rss_hf & ETH_RSS_IPV6) {
		cfg.addl_hdrs = ICE_FLOW_SEG_HDR_IPV6 | ICE_FLOW_SEG_HDR_IPV_OTHER;
		cfg.hash_flds = ICE_FLOW_HASH_IPV6;
		ret = ice_add_rss_cfg_wrap(pf, vsi->idx, &cfg);
		if (ret)
			PMD_DRV_LOG(ERR, "%s IPV6 rss flow fail %d",
				    __func__, ret);
	}

	/* Configure RSS for udp4 with src/dst addr and port as input set */
	if (rss_hf & ETH_RSS_NONFRAG_IPV4_UDP) {
		cfg.addl_hdrs = ICE_FLOW_SEG_HDR_UDP | ICE_FLOW_SEG_HDR_IPV4 |
				ICE_FLOW_SEG_HDR_IPV_OTHER;
		cfg.hash_flds = ICE_HASH_UDP_IPV4;
		ret = ice_add_rss_cfg_wrap(pf, vsi->idx, &cfg);
		if (ret)
			PMD_DRV_LOG(ERR, "%s UDP_IPV4 rss flow fail %d",
				    __func__, ret);
	}

	/* Configure RSS for udp6 with src/dst addr and port as input set */
	if (rss_hf & ETH_RSS_NONFRAG_IPV6_UDP) {
		cfg.addl_hdrs = ICE_FLOW_SEG_HDR_UDP | ICE_FLOW_SEG_HDR_IPV6 |
				ICE_FLOW_SEG_HDR_IPV_OTHER;
		cfg.hash_flds = ICE_HASH_UDP_IPV6;
		ret = ice_add_rss_cfg_wrap(pf, vsi->idx, &cfg);
		if (ret)
			PMD_DRV_LOG(ERR, "%s UDP_IPV6 rss flow fail %d",
				    __func__, ret);
	}

	/* Configure RSS for tcp4 with src/dst addr and port as input set */
	if (rss_hf & ETH_RSS_NONFRAG_IPV4_TCP) {
		cfg.addl_hdrs = ICE_FLOW_SEG_HDR_TCP | ICE_FLOW_SEG_HDR_IPV4 |
				ICE_FLOW_SEG_HDR_IPV_OTHER;
		cfg.hash_flds = ICE_HASH_TCP_IPV4;
		ret = ice_add_rss_cfg_wrap(pf, vsi->idx, &cfg);
		if (ret)
			PMD_DRV_LOG(ERR, "%s TCP_IPV4 rss flow fail %d",
				    __func__, ret);
	}

	/* Configure RSS for tcp6 with src/dst addr and port as input set */
	if (rss_hf & ETH_RSS_NONFRAG_IPV6_TCP) {
		cfg.addl_hdrs = ICE_FLOW_SEG_HDR_TCP | ICE_FLOW_SEG_HDR_IPV6 |
				ICE_FLOW_SEG_HDR_IPV_OTHER;
		cfg.hash_flds = ICE_HASH_TCP_IPV6;
		ret = ice_add_rss_cfg_wrap(pf, vsi->idx, &cfg);
		if (ret)
			PMD_DRV_LOG(ERR, "%s TCP_IPV6 rss flow fail %d",
				    __func__, ret);
	}

	/* Configure RSS for sctp4 with src/dst addr and port as input set */
	if (rss_hf & ETH_RSS_NONFRAG_IPV4_SCTP) {
		cfg.addl_hdrs = ICE_FLOW_SEG_HDR_SCTP | ICE_FLOW_SEG_HDR_IPV4 |
				ICE_FLOW_SEG_HDR_IPV_OTHER;
		cfg.hash_flds = ICE_HASH_SCTP_IPV4;
		ret = ice_add_rss_cfg_wrap(pf, vsi->idx, &cfg);
		if (ret)
			PMD_DRV_LOG(ERR, "%s SCTP_IPV4 rss flow fail %d",
				    __func__, ret);
	}

	/* Configure RSS for sctp6 with src/dst addr and port as input set */
	if (rss_hf & ETH_RSS_NONFRAG_IPV6_SCTP) {
		cfg.addl_hdrs = ICE_FLOW_SEG_HDR_SCTP | ICE_FLOW_SEG_HDR_IPV6 |
				ICE_FLOW_SEG_HDR_IPV_OTHER;
		cfg.hash_flds = ICE_HASH_SCTP_IPV6;
		ret = ice_add_rss_cfg_wrap(pf, vsi->idx, &cfg);
		if (ret)
			PMD_DRV_LOG(ERR, "%s SCTP_IPV6 rss flow fail %d",
				    __func__, ret);
	}

	if (rss_hf & ETH_RSS_IPV4) {
		cfg.addl_hdrs = ICE_FLOW_SEG_HDR_GTPU_IP | ICE_FLOW_SEG_HDR_IPV4 |
				ICE_FLOW_SEG_HDR_IPV_OTHER;
		cfg.hash_flds = ICE_FLOW_HASH_IPV4;
		ret = ice_add_rss_cfg_wrap(pf, vsi->idx, &cfg);
		if (ret)
			PMD_DRV_LOG(ERR, "%s GTPU_IPV4 rss flow fail %d",
				    __func__, ret);

		cfg.addl_hdrs = ICE_FLOW_SEG_HDR_GTPU_EH | ICE_FLOW_SEG_HDR_IPV4 |
				ICE_FLOW_SEG_HDR_IPV_OTHER;
		ret = ice_add_rss_cfg_wrap(pf, vsi->idx, &cfg);
		if (ret)
			PMD_DRV_LOG(ERR, "%s GTPU_EH_IPV4 rss flow fail %d",
				    __func__, ret);

		cfg.addl_hdrs = ICE_FLOW_SEG_HDR_PPPOE | ICE_FLOW_SEG_HDR_IPV4 |
				ICE_FLOW_SEG_HDR_IPV_OTHER;
		ret = ice_add_rss_cfg_wrap(pf, vsi->idx, &cfg);
		if (ret)
			PMD_DRV_LOG(ERR, "%s PPPoE_IPV4 rss flow fail %d",
				    __func__, ret);
	}

	if (rss_hf & ETH_RSS_IPV6) {
		cfg.addl_hdrs = ICE_FLOW_SEG_HDR_GTPU_IP | ICE_FLOW_SEG_HDR_IPV6 |
				ICE_FLOW_SEG_HDR_IPV_OTHER;
		cfg.hash_flds = ICE_FLOW_HASH_IPV6;
		ret = ice_add_rss_cfg_wrap(pf, vsi->idx, &cfg);
		if (ret)
			PMD_DRV_LOG(ERR, "%s GTPU_IPV6 rss flow fail %d",
				    __func__, ret);

		cfg.addl_hdrs = ICE_FLOW_SEG_HDR_GTPU_EH | ICE_FLOW_SEG_HDR_IPV6 |
				ICE_FLOW_SEG_HDR_IPV_OTHER;
		ret = ice_add_rss_cfg_wrap(pf, vsi->idx, &cfg);
		if (ret)
			PMD_DRV_LOG(ERR, "%s GTPU_EH_IPV6 rss flow fail %d",
				    __func__, ret);

		cfg.addl_hdrs = ICE_FLOW_SEG_HDR_PPPOE | ICE_FLOW_SEG_HDR_IPV6 |
				ICE_FLOW_SEG_HDR_IPV_OTHER;
		ret = ice_add_rss_cfg_wrap(pf, vsi->idx, &cfg);
		if (ret)
			PMD_DRV_LOG(ERR, "%s PPPoE_IPV6 rss flow fail %d",
				    __func__, ret);
	}

	if (rss_hf & ETH_RSS_NONFRAG_IPV4_UDP) {
		cfg.addl_hdrs = ICE_FLOW_SEG_HDR_GTPU_IP | ICE_FLOW_SEG_HDR_UDP |
				ICE_FLOW_SEG_HDR_IPV4 | ICE_FLOW_SEG_HDR_IPV_OTHER;
		cfg.hash_flds = ICE_HASH_UDP_IPV4;
		ret = ice_add_rss_cfg_wrap(pf, vsi->idx, &cfg);
		if (ret)
			PMD_DRV_LOG(ERR, "%s GTPU_IPV4_UDP rss flow fail %d",
				    __func__, ret);

		cfg.addl_hdrs = ICE_FLOW_SEG_HDR_GTPU_EH | ICE_FLOW_SEG_HDR_UDP |
				ICE_FLOW_SEG_HDR_IPV4 | ICE_FLOW_SEG_HDR_IPV_OTHER;
		ret = ice_add_rss_cfg_wrap(pf, vsi->idx, &cfg);
		if (ret)
			PMD_DRV_LOG(ERR, "%s GTPU_EH_IPV4_UDP rss flow fail %d",
				    __func__, ret);

		cfg.addl_hdrs = ICE_FLOW_SEG_HDR_PPPOE | ICE_FLOW_SEG_HDR_UDP |
				ICE_FLOW_SEG_HDR_IPV4 | ICE_FLOW_SEG_HDR_IPV_OTHER;
		ret = ice_add_rss_cfg_wrap(pf, vsi->idx, &cfg);
		if (ret)
			PMD_DRV_LOG(ERR, "%s PPPoE_IPV4_UDP rss flow fail %d",
				    __func__, ret);
	}

	if (rss_hf & ETH_RSS_NONFRAG_IPV6_UDP) {
		cfg.addl_hdrs = ICE_FLOW_SEG_HDR_GTPU_IP | ICE_FLOW_SEG_HDR_UDP |
				ICE_FLOW_SEG_HDR_IPV6 | ICE_FLOW_SEG_HDR_IPV_OTHER;
		cfg.hash_flds = ICE_HASH_UDP_IPV6;
		ret = ice_add_rss_cfg_wrap(pf, vsi->idx, &cfg);
		if (ret)
			PMD_DRV_LOG(ERR, "%s GTPU_IPV6_UDP rss flow fail %d",
				    __func__, ret);

		cfg.addl_hdrs = ICE_FLOW_SEG_HDR_GTPU_EH | ICE_FLOW_SEG_HDR_UDP |
				ICE_FLOW_SEG_HDR_IPV6 | ICE_FLOW_SEG_HDR_IPV_OTHER;
		ret = ice_add_rss_cfg_wrap(pf, vsi->idx, &cfg);
		if (ret)
			PMD_DRV_LOG(ERR, "%s GTPU_EH_IPV6_UDP rss flow fail %d",
				    __func__, ret);

		cfg.addl_hdrs = ICE_FLOW_SEG_HDR_PPPOE | ICE_FLOW_SEG_HDR_UDP |
				ICE_FLOW_SEG_HDR_IPV6 | ICE_FLOW_SEG_HDR_IPV_OTHER;
		ret = ice_add_rss_cfg_wrap(pf, vsi->idx, &cfg);
		if (ret)
			PMD_DRV_LOG(ERR, "%s PPPoE_IPV6_UDP rss flow fail %d",
				    __func__, ret);
	}

	if (rss_hf & ETH_RSS_NONFRAG_IPV4_TCP) {
		cfg.addl_hdrs = ICE_FLOW_SEG_HDR_GTPU_IP | ICE_FLOW_SEG_HDR_TCP |
				ICE_FLOW_SEG_HDR_IPV4 | ICE_FLOW_SEG_HDR_IPV_OTHER;
		cfg.hash_flds = ICE_HASH_TCP_IPV4;
		ret = ice_add_rss_cfg_wrap(pf, vsi->idx, &cfg);
		if (ret)
			PMD_DRV_LOG(ERR, "%s GTPU_IPV4_TCP rss flow fail %d",
				    __func__, ret);

		cfg.addl_hdrs = ICE_FLOW_SEG_HDR_GTPU_EH | ICE_FLOW_SEG_HDR_TCP |
				ICE_FLOW_SEG_HDR_IPV4 | ICE_FLOW_SEG_HDR_IPV_OTHER;
		ret = ice_add_rss_cfg_wrap(pf, vsi->idx, &cfg);
		if (ret)
			PMD_DRV_LOG(ERR, "%s GTPU_EH_IPV4_TCP rss flow fail %d",
				    __func__, ret);

		cfg.addl_hdrs = ICE_FLOW_SEG_HDR_PPPOE | ICE_FLOW_SEG_HDR_TCP |
				ICE_FLOW_SEG_HDR_IPV4 | ICE_FLOW_SEG_HDR_IPV_OTHER;
		ret = ice_add_rss_cfg_wrap(pf, vsi->idx, &cfg);
		if (ret)
			PMD_DRV_LOG(ERR, "%s PPPoE_IPV4_TCP rss flow fail %d",
				    __func__, ret);
	}

	if (rss_hf & ETH_RSS_NONFRAG_IPV6_TCP) {
		cfg.addl_hdrs = ICE_FLOW_SEG_HDR_GTPU_IP | ICE_FLOW_SEG_HDR_TCP |
				ICE_FLOW_SEG_HDR_IPV6 | ICE_FLOW_SEG_HDR_IPV_OTHER;
		cfg.hash_flds = ICE_HASH_TCP_IPV6;
		ret = ice_add_rss_cfg_wrap(pf, vsi->idx, &cfg);
		if (ret)
			PMD_DRV_LOG(ERR, "%s GTPU_IPV6_TCP rss flow fail %d",
				    __func__, ret);

		cfg.addl_hdrs = ICE_FLOW_SEG_HDR_GTPU_EH | ICE_FLOW_SEG_HDR_TCP |
				ICE_FLOW_SEG_HDR_IPV6 | ICE_FLOW_SEG_HDR_IPV_OTHER;
		ret = ice_add_rss_cfg_wrap(pf, vsi->idx, &cfg);
		if (ret)
			PMD_DRV_LOG(ERR, "%s GTPU_EH_IPV6_TCP rss flow fail %d",
				    __func__, ret);

		cfg.addl_hdrs = ICE_FLOW_SEG_HDR_PPPOE | ICE_FLOW_SEG_HDR_TCP |
				ICE_FLOW_SEG_HDR_IPV6 | ICE_FLOW_SEG_HDR_IPV_OTHER;
		ret = ice_add_rss_cfg_wrap(pf, vsi->idx, &cfg);
		if (ret)
			PMD_DRV_LOG(ERR, "%s PPPoE_IPV6_TCP rss flow fail %d",
				    __func__, ret);
	}

	pf->rss_hf = rss_hf & ICE_RSS_HF_ALL;
}

static void
ice_get_default_rss_key(uint8_t *rss_key, uint32_t rss_key_size)
{
	static struct ice_aqc_get_set_rss_keys default_key;
	static bool default_key_done;
	uint8_t *key = (uint8_t *)&default_key;
	size_t i;

	if (rss_key_size > sizeof(default_key)) {
		PMD_DRV_LOG(WARNING,
			    "requested size %u is larger than default %zu, "
			    "only %zu bytes are gotten for key\n",
			    rss_key_size, sizeof(default_key),
			    sizeof(default_key));
	}

	if (!default_key_done) {
		/* Calculate the default hash key */
		for (i = 0; i < sizeof(default_key); i++)
			key[i] = (uint8_t)rte_rand();
		default_key_done = true;
	}
	rte_memcpy(rss_key, key, RTE_MIN(rss_key_size, sizeof(default_key)));
}

static int ice_init_rss(struct ice_pf *pf)
{
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	struct ice_vsi *vsi = pf->main_vsi;
	struct rte_eth_dev_data *dev_data = pf->dev_data;
	struct ice_aq_get_set_rss_lut_params lut_params;
	struct rte_eth_rss_conf *rss_conf;
	struct ice_aqc_get_set_rss_keys key;
	uint16_t i, nb_q;
	int ret = 0;
	bool is_safe_mode = pf->adapter->is_safe_mode;
	uint32_t reg;

	rss_conf = &dev_data->dev_conf.rx_adv_conf.rss_conf;
	nb_q = dev_data->nb_rx_queues;
	vsi->rss_key_size = ICE_AQC_GET_SET_RSS_KEY_DATA_RSS_KEY_SIZE;
	vsi->rss_lut_size = pf->hash_lut_size;

	if (nb_q == 0) {
		PMD_DRV_LOG(WARNING,
			"RSS is not supported as rx queues number is zero\n");
		return 0;
	}

	if (is_safe_mode) {
		PMD_DRV_LOG(WARNING, "RSS is not supported in safe mode\n");
		return 0;
	}

	if (!vsi->rss_key) {
		vsi->rss_key = rte_zmalloc(NULL,
					   vsi->rss_key_size, 0);
		if (vsi->rss_key == NULL) {
			PMD_DRV_LOG(ERR, "Failed to allocate memory for rss_key");
			return -ENOMEM;
		}
	}
	if (!vsi->rss_lut) {
		vsi->rss_lut = rte_zmalloc(NULL,
					   vsi->rss_lut_size, 0);
		if (vsi->rss_lut == NULL) {
			PMD_DRV_LOG(ERR, "Failed to allocate memory for rss_key");
			rte_free(vsi->rss_key);
			vsi->rss_key = NULL;
			return -ENOMEM;
		}
	}
	/* configure RSS key */
	if (!rss_conf->rss_key)
		ice_get_default_rss_key(vsi->rss_key, vsi->rss_key_size);
	else
		rte_memcpy(vsi->rss_key, rss_conf->rss_key,
			   RTE_MIN(rss_conf->rss_key_len,
				   vsi->rss_key_size));

	rte_memcpy(key.standard_rss_key, vsi->rss_key, vsi->rss_key_size);
	ret = ice_aq_set_rss_key(hw, vsi->idx, &key);
	if (ret)
		goto out;

	/* init RSS LUT table */
	for (i = 0; i < vsi->rss_lut_size; i++)
		vsi->rss_lut[i] = i % nb_q;

	lut_params.vsi_handle = vsi->idx;
	lut_params.lut_size = vsi->rss_lut_size;
	lut_params.lut_type = ICE_AQC_GSET_RSS_LUT_TABLE_TYPE_PF;
	lut_params.lut = vsi->rss_lut;
	lut_params.global_lut_id = 0;
	ret = ice_aq_set_rss_lut(hw, &lut_params);
	if (ret)
		goto out;

	/* Enable registers for symmetric_toeplitz function. */
	reg = ICE_READ_REG(hw, VSIQF_HASH_CTL(vsi->vsi_id));
	reg = (reg & (~VSIQF_HASH_CTL_HASH_SCHEME_M)) |
		(1 << VSIQF_HASH_CTL_HASH_SCHEME_S);
	ICE_WRITE_REG(hw, VSIQF_HASH_CTL(vsi->vsi_id), reg);

	/* RSS hash configuration */
	ice_rss_hash_set(pf, rss_conf->rss_hf);

	return 0;
out:
	rte_free(vsi->rss_key);
	vsi->rss_key = NULL;
	rte_free(vsi->rss_lut);
	vsi->rss_lut = NULL;
	return -EINVAL;
}

static int
ice_dev_configure(struct rte_eth_dev *dev)
{
	struct ice_adapter *ad =
		ICE_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	int ret;

	/* Initialize to TRUE. If any of Rx queues doesn't meet the
	 * bulk allocation or vector Rx preconditions we will reset it.
	 */
	ad->rx_bulk_alloc_allowed = true;
	ad->tx_simple_allowed = true;

	if (dev->data->dev_conf.rxmode.mq_mode & ETH_MQ_RX_RSS_FLAG)
		dev->data->dev_conf.rxmode.offloads |= DEV_RX_OFFLOAD_RSS_HASH;

	if (dev->data->nb_rx_queues) {
		ret = ice_init_rss(pf);
		if (ret) {
			PMD_DRV_LOG(ERR, "Failed to enable rss for PF");
			return ret;
		}
	}

	return 0;
}

static void
__vsi_queues_bind_intr(struct ice_vsi *vsi, uint16_t msix_vect,
		       int base_queue, int nb_queue)
{
	struct ice_hw *hw = ICE_VSI_TO_HW(vsi);
	uint32_t val, val_tx;
	int i;

	for (i = 0; i < nb_queue; i++) {
		/*do actual bind*/
		val = (msix_vect & QINT_RQCTL_MSIX_INDX_M) |
		      (0 << QINT_RQCTL_ITR_INDX_S) | QINT_RQCTL_CAUSE_ENA_M;
		val_tx = (msix_vect & QINT_TQCTL_MSIX_INDX_M) |
			 (0 << QINT_TQCTL_ITR_INDX_S) | QINT_TQCTL_CAUSE_ENA_M;

		PMD_DRV_LOG(INFO, "queue %d is binding to vect %d",
			    base_queue + i, msix_vect);
		/* set ITR0 value */
		ICE_WRITE_REG(hw, GLINT_ITR(0, msix_vect), 0x2);
		ICE_WRITE_REG(hw, QINT_RQCTL(base_queue + i), val);
		ICE_WRITE_REG(hw, QINT_TQCTL(base_queue + i), val_tx);
	}
}

void
ice_vsi_queues_bind_intr(struct ice_vsi *vsi)
{
	struct rte_eth_dev *dev = &rte_eth_devices[vsi->adapter->pf.dev_data->port_id];
	struct rte_pci_device *pci_dev = ICE_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	struct ice_hw *hw = ICE_VSI_TO_HW(vsi);
	uint16_t msix_vect = vsi->msix_intr;
	uint16_t nb_msix = RTE_MIN(vsi->nb_msix, intr_handle->nb_efd);
	uint16_t queue_idx = 0;
	int record = 0;
	int i;

	/* clear Rx/Tx queue interrupt */
	for (i = 0; i < vsi->nb_used_qps; i++) {
		ICE_WRITE_REG(hw, QINT_TQCTL(vsi->base_queue + i), 0);
		ICE_WRITE_REG(hw, QINT_RQCTL(vsi->base_queue + i), 0);
	}

	/* PF bind interrupt */
	if (rte_intr_dp_is_en(intr_handle)) {
		queue_idx = 0;
		record = 1;
	}

	for (i = 0; i < vsi->nb_used_qps; i++) {
		if (nb_msix <= 1) {
			if (!rte_intr_allow_others(intr_handle))
				msix_vect = ICE_MISC_VEC_ID;

			/* uio mapping all queue to one msix_vect */
			__vsi_queues_bind_intr(vsi, msix_vect,
					       vsi->base_queue + i,
					       vsi->nb_used_qps - i);

			for (; !!record && i < vsi->nb_used_qps; i++)
				intr_handle->intr_vec[queue_idx + i] =
					msix_vect;
			break;
		}

		/* vfio 1:1 queue/msix_vect mapping */
		__vsi_queues_bind_intr(vsi, msix_vect,
				       vsi->base_queue + i, 1);

		if (!!record)
			intr_handle->intr_vec[queue_idx + i] = msix_vect;

		msix_vect++;
		nb_msix--;
	}
}

void
ice_vsi_enable_queues_intr(struct ice_vsi *vsi)
{
	struct rte_eth_dev *dev = &rte_eth_devices[vsi->adapter->pf.dev_data->port_id];
	struct rte_pci_device *pci_dev = ICE_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	struct ice_hw *hw = ICE_VSI_TO_HW(vsi);
	uint16_t msix_intr, i;

	if (rte_intr_allow_others(intr_handle))
		for (i = 0; i < vsi->nb_used_qps; i++) {
			msix_intr = vsi->msix_intr + i;
			ICE_WRITE_REG(hw, GLINT_DYN_CTL(msix_intr),
				      GLINT_DYN_CTL_INTENA_M |
				      GLINT_DYN_CTL_CLEARPBA_M |
				      GLINT_DYN_CTL_ITR_INDX_M |
				      GLINT_DYN_CTL_WB_ON_ITR_M);
		}
	else
		ICE_WRITE_REG(hw, GLINT_DYN_CTL(0),
			      GLINT_DYN_CTL_INTENA_M |
			      GLINT_DYN_CTL_CLEARPBA_M |
			      GLINT_DYN_CTL_ITR_INDX_M |
			      GLINT_DYN_CTL_WB_ON_ITR_M);
}

static int
ice_rxq_intr_setup(struct rte_eth_dev *dev)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct rte_pci_device *pci_dev = ICE_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	struct ice_vsi *vsi = pf->main_vsi;
	uint32_t intr_vector = 0;

	rte_intr_disable(intr_handle);

	/* check and configure queue intr-vector mapping */
	if ((rte_intr_cap_multiple(intr_handle) ||
	     !RTE_ETH_DEV_SRIOV(dev).active) &&
	    dev->data->dev_conf.intr_conf.rxq != 0) {
		intr_vector = dev->data->nb_rx_queues;
		if (intr_vector > ICE_MAX_INTR_QUEUE_NUM) {
			PMD_DRV_LOG(ERR, "At most %d intr queues supported",
				    ICE_MAX_INTR_QUEUE_NUM);
			return -ENOTSUP;
		}
		if (rte_intr_efd_enable(intr_handle, intr_vector))
			return -1;
	}

	if (rte_intr_dp_is_en(intr_handle) && !intr_handle->intr_vec) {
		intr_handle->intr_vec =
		rte_zmalloc(NULL, dev->data->nb_rx_queues * sizeof(int),
			    0);
		if (!intr_handle->intr_vec) {
			PMD_DRV_LOG(ERR,
				    "Failed to allocate %d rx_queues intr_vec",
				    dev->data->nb_rx_queues);
			return -ENOMEM;
		}
	}

	/* Map queues with MSIX interrupt */
	vsi->nb_used_qps = dev->data->nb_rx_queues;
	ice_vsi_queues_bind_intr(vsi);

	/* Enable interrupts for all the queues */
	ice_vsi_enable_queues_intr(vsi);

	rte_intr_enable(intr_handle);

	return 0;
}

static void
ice_get_init_link_status(struct rte_eth_dev *dev)
{
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	bool enable_lse = dev->data->dev_conf.intr_conf.lsc ? true : false;
	struct ice_link_status link_status;
	int ret;

	ret = ice_aq_get_link_info(hw->port_info, enable_lse,
				   &link_status, NULL);
	if (ret != ICE_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to get link info");
		pf->init_link_up = false;
		return;
	}

	if (link_status.link_info & ICE_AQ_LINK_UP)
		pf->init_link_up = true;
}

static int
ice_dev_start(struct rte_eth_dev *dev)
{
	struct rte_eth_dev_data *data = dev->data;
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_vsi *vsi = pf->main_vsi;
	uint16_t nb_rxq = 0;
	uint16_t nb_txq, i;
	uint16_t max_frame_size;
	int mask, ret;

	/* program Tx queues' context in hardware */
	for (nb_txq = 0; nb_txq < data->nb_tx_queues; nb_txq++) {
		ret = ice_tx_queue_start(dev, nb_txq);
		if (ret) {
			PMD_DRV_LOG(ERR, "fail to start Tx queue %u", nb_txq);
			goto tx_err;
		}
	}

	/* program Rx queues' context in hardware*/
	for (nb_rxq = 0; nb_rxq < data->nb_rx_queues; nb_rxq++) {
		ret = ice_rx_queue_start(dev, nb_rxq);
		if (ret) {
			PMD_DRV_LOG(ERR, "fail to start Rx queue %u", nb_rxq);
			goto rx_err;
		}
	}

	ice_set_rx_function(dev);
	ice_set_tx_function(dev);

	mask = ETH_VLAN_STRIP_MASK | ETH_VLAN_FILTER_MASK |
			ETH_VLAN_EXTEND_MASK;
	ret = ice_vlan_offload_set(dev, mask);
	if (ret) {
		PMD_INIT_LOG(ERR, "Unable to set VLAN offload");
		goto rx_err;
	}

	/* enable Rx interrput and mapping Rx queue to interrupt vector */
	if (ice_rxq_intr_setup(dev))
		return -EIO;

	/* Enable receiving broadcast packets and transmitting packets */
	ret = ice_set_vsi_promisc(hw, vsi->idx,
				  ICE_PROMISC_BCAST_RX | ICE_PROMISC_BCAST_TX |
				  ICE_PROMISC_UCAST_TX | ICE_PROMISC_MCAST_TX,
				  0);
	if (ret != ICE_SUCCESS)
		PMD_DRV_LOG(INFO, "fail to set vsi broadcast");

	ret = ice_aq_set_event_mask(hw, hw->port_info->lport,
				    ((u16)(ICE_AQ_LINK_EVENT_LINK_FAULT |
				     ICE_AQ_LINK_EVENT_PHY_TEMP_ALARM |
				     ICE_AQ_LINK_EVENT_EXCESSIVE_ERRORS |
				     ICE_AQ_LINK_EVENT_SIGNAL_DETECT |
				     ICE_AQ_LINK_EVENT_AN_COMPLETED |
				     ICE_AQ_LINK_EVENT_PORT_TX_SUSPENDED)),
				     NULL);
	if (ret != ICE_SUCCESS)
		PMD_DRV_LOG(WARNING, "Fail to set phy mask");

	ice_get_init_link_status(dev);

	ice_dev_set_link_up(dev);

	/* Call get_link_info aq commond to enable/disable LSE */
	ice_link_update(dev, 0);

	pf->adapter_stopped = false;

	/* Set the max frame size to default value*/
	max_frame_size = pf->dev_data->dev_conf.rxmode.max_rx_pkt_len ?
		pf->dev_data->dev_conf.rxmode.max_rx_pkt_len :
		ICE_FRAME_SIZE_MAX;

	/* Set the max frame size to HW*/
	ice_aq_set_mac_cfg(hw, max_frame_size, NULL);

	return 0;

	/* stop the started queues if failed to start all queues */
rx_err:
	for (i = 0; i < nb_rxq; i++)
		ice_rx_queue_stop(dev, i);
tx_err:
	for (i = 0; i < nb_txq; i++)
		ice_tx_queue_stop(dev, i);

	return -EIO;
}

static int
ice_dev_reset(struct rte_eth_dev *dev)
{
	int ret;

	if (dev->data->sriov.active)
		return -ENOTSUP;

	ret = ice_dev_uninit(dev);
	if (ret) {
		PMD_INIT_LOG(ERR, "failed to uninit device, status = %d", ret);
		return -ENXIO;
	}

	ret = ice_dev_init(dev);
	if (ret) {
		PMD_INIT_LOG(ERR, "failed to init device, status = %d", ret);
		return -ENXIO;
	}

	return 0;
}

static int
ice_dev_info_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ice_vsi *vsi = pf->main_vsi;
	struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI(dev->device);
	bool is_safe_mode = pf->adapter->is_safe_mode;
	u64 phy_type_low;
	u64 phy_type_high;

	dev_info->min_rx_bufsize = ICE_BUF_SIZE_MIN;
	dev_info->max_rx_pktlen = ICE_FRAME_SIZE_MAX;
	dev_info->max_rx_queues = vsi->nb_qps;
	dev_info->max_tx_queues = vsi->nb_qps;
	dev_info->max_mac_addrs = vsi->max_macaddrs;
	dev_info->max_vfs = pci_dev->max_vfs;
	dev_info->max_mtu = dev_info->max_rx_pktlen - ICE_ETH_OVERHEAD;
	dev_info->min_mtu = RTE_ETHER_MIN_MTU;

	dev_info->rx_offload_capa =
		DEV_RX_OFFLOAD_VLAN_STRIP |
		DEV_RX_OFFLOAD_JUMBO_FRAME |
		DEV_RX_OFFLOAD_KEEP_CRC |
		DEV_RX_OFFLOAD_SCATTER |
		DEV_RX_OFFLOAD_VLAN_FILTER;
	dev_info->tx_offload_capa =
		DEV_TX_OFFLOAD_VLAN_INSERT |
		DEV_TX_OFFLOAD_TCP_TSO |
		DEV_TX_OFFLOAD_MULTI_SEGS |
		DEV_TX_OFFLOAD_MBUF_FAST_FREE;
	dev_info->flow_type_rss_offloads = 0;

	if (!is_safe_mode) {
		dev_info->rx_offload_capa |=
			DEV_RX_OFFLOAD_IPV4_CKSUM |
			DEV_RX_OFFLOAD_UDP_CKSUM |
			DEV_RX_OFFLOAD_TCP_CKSUM |
			DEV_RX_OFFLOAD_QINQ_STRIP |
			DEV_RX_OFFLOAD_OUTER_IPV4_CKSUM |
			DEV_RX_OFFLOAD_VLAN_EXTEND |
			DEV_RX_OFFLOAD_RSS_HASH;
		dev_info->tx_offload_capa |=
			DEV_TX_OFFLOAD_QINQ_INSERT |
			DEV_TX_OFFLOAD_IPV4_CKSUM |
			DEV_TX_OFFLOAD_UDP_CKSUM |
			DEV_TX_OFFLOAD_TCP_CKSUM |
			DEV_TX_OFFLOAD_SCTP_CKSUM |
			DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM |
			DEV_TX_OFFLOAD_OUTER_UDP_CKSUM;
		dev_info->flow_type_rss_offloads |= ICE_RSS_OFFLOAD_ALL;
	}

	dev_info->rx_queue_offload_capa = 0;
	dev_info->tx_queue_offload_capa = DEV_TX_OFFLOAD_MBUF_FAST_FREE;

	dev_info->reta_size = pf->hash_lut_size;
	dev_info->hash_key_size = (VSIQF_HKEY_MAX_INDEX + 1) * sizeof(uint32_t);

	dev_info->default_rxconf = (struct rte_eth_rxconf) {
		.rx_thresh = {
			.pthresh = ICE_DEFAULT_RX_PTHRESH,
			.hthresh = ICE_DEFAULT_RX_HTHRESH,
			.wthresh = ICE_DEFAULT_RX_WTHRESH,
		},
		.rx_free_thresh = ICE_DEFAULT_RX_FREE_THRESH,
		.rx_drop_en = 0,
		.offloads = 0,
	};

	dev_info->default_txconf = (struct rte_eth_txconf) {
		.tx_thresh = {
			.pthresh = ICE_DEFAULT_TX_PTHRESH,
			.hthresh = ICE_DEFAULT_TX_HTHRESH,
			.wthresh = ICE_DEFAULT_TX_WTHRESH,
		},
		.tx_free_thresh = ICE_DEFAULT_TX_FREE_THRESH,
		.tx_rs_thresh = ICE_DEFAULT_TX_RSBIT_THRESH,
		.offloads = 0,
	};

	dev_info->rx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = ICE_MAX_RING_DESC,
		.nb_min = ICE_MIN_RING_DESC,
		.nb_align = ICE_ALIGN_RING_DESC,
	};

	dev_info->tx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = ICE_MAX_RING_DESC,
		.nb_min = ICE_MIN_RING_DESC,
		.nb_align = ICE_ALIGN_RING_DESC,
	};

	dev_info->speed_capa = ETH_LINK_SPEED_10M |
			       ETH_LINK_SPEED_100M |
			       ETH_LINK_SPEED_1G |
			       ETH_LINK_SPEED_2_5G |
			       ETH_LINK_SPEED_5G |
			       ETH_LINK_SPEED_10G |
			       ETH_LINK_SPEED_20G |
			       ETH_LINK_SPEED_25G;

	phy_type_low = hw->port_info->phy.phy_type_low;
	phy_type_high = hw->port_info->phy.phy_type_high;

	if (ICE_PHY_TYPE_SUPPORT_50G(phy_type_low))
		dev_info->speed_capa |= ETH_LINK_SPEED_50G;

	if (ICE_PHY_TYPE_SUPPORT_100G_LOW(phy_type_low) ||
			ICE_PHY_TYPE_SUPPORT_100G_HIGH(phy_type_high))
		dev_info->speed_capa |= ETH_LINK_SPEED_100G;

	dev_info->nb_rx_queues = dev->data->nb_rx_queues;
	dev_info->nb_tx_queues = dev->data->nb_tx_queues;

	dev_info->default_rxportconf.burst_size = ICE_RX_MAX_BURST;
	dev_info->default_txportconf.burst_size = ICE_TX_MAX_BURST;
	dev_info->default_rxportconf.nb_queues = 1;
	dev_info->default_txportconf.nb_queues = 1;
	dev_info->default_rxportconf.ring_size = ICE_BUF_SIZE_MIN;
	dev_info->default_txportconf.ring_size = ICE_BUF_SIZE_MIN;

	return 0;
}

static inline int
ice_atomic_read_link_status(struct rte_eth_dev *dev,
			    struct rte_eth_link *link)
{
	struct rte_eth_link *dst = link;
	struct rte_eth_link *src = &dev->data->dev_link;

	if (rte_atomic64_cmpset((uint64_t *)dst, *(uint64_t *)dst,
				*(uint64_t *)src) == 0)
		return -1;

	return 0;
}

static inline int
ice_atomic_write_link_status(struct rte_eth_dev *dev,
			     struct rte_eth_link *link)
{
	struct rte_eth_link *dst = &dev->data->dev_link;
	struct rte_eth_link *src = link;

	if (rte_atomic64_cmpset((uint64_t *)dst, *(uint64_t *)dst,
				*(uint64_t *)src) == 0)
		return -1;

	return 0;
}

static int
ice_link_update(struct rte_eth_dev *dev, int wait_to_complete)
{
#define CHECK_INTERVAL 100  /* 100ms */
#define MAX_REPEAT_TIME 10  /* 1s (10 * 100ms) in total */
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ice_link_status link_status;
	struct rte_eth_link link, old;
	int status;
	unsigned int rep_cnt = MAX_REPEAT_TIME;
	bool enable_lse = dev->data->dev_conf.intr_conf.lsc ? true : false;

	memset(&link, 0, sizeof(link));
	memset(&old, 0, sizeof(old));
	memset(&link_status, 0, sizeof(link_status));
	ice_atomic_read_link_status(dev, &old);

	do {
		/* Get link status information from hardware */
		status = ice_aq_get_link_info(hw->port_info, enable_lse,
					      &link_status, NULL);
		if (status != ICE_SUCCESS) {
			link.link_speed = ETH_SPEED_NUM_100M;
			link.link_duplex = ETH_LINK_FULL_DUPLEX;
			PMD_DRV_LOG(ERR, "Failed to get link info");
			goto out;
		}

		link.link_status = link_status.link_info & ICE_AQ_LINK_UP;
		if (!wait_to_complete || link.link_status)
			break;

		rte_delay_ms(CHECK_INTERVAL);
	} while (--rep_cnt);

	if (!link.link_status)
		goto out;

	/* Full-duplex operation at all supported speeds */
	link.link_duplex = ETH_LINK_FULL_DUPLEX;

	/* Parse the link status */
	switch (link_status.link_speed) {
	case ICE_AQ_LINK_SPEED_10MB:
		link.link_speed = ETH_SPEED_NUM_10M;
		break;
	case ICE_AQ_LINK_SPEED_100MB:
		link.link_speed = ETH_SPEED_NUM_100M;
		break;
	case ICE_AQ_LINK_SPEED_1000MB:
		link.link_speed = ETH_SPEED_NUM_1G;
		break;
	case ICE_AQ_LINK_SPEED_2500MB:
		link.link_speed = ETH_SPEED_NUM_2_5G;
		break;
	case ICE_AQ_LINK_SPEED_5GB:
		link.link_speed = ETH_SPEED_NUM_5G;
		break;
	case ICE_AQ_LINK_SPEED_10GB:
		link.link_speed = ETH_SPEED_NUM_10G;
		break;
	case ICE_AQ_LINK_SPEED_20GB:
		link.link_speed = ETH_SPEED_NUM_20G;
		break;
	case ICE_AQ_LINK_SPEED_25GB:
		link.link_speed = ETH_SPEED_NUM_25G;
		break;
	case ICE_AQ_LINK_SPEED_40GB:
		link.link_speed = ETH_SPEED_NUM_40G;
		break;
	case ICE_AQ_LINK_SPEED_50GB:
		link.link_speed = ETH_SPEED_NUM_50G;
		break;
	case ICE_AQ_LINK_SPEED_100GB:
		link.link_speed = ETH_SPEED_NUM_100G;
		break;
	case ICE_AQ_LINK_SPEED_UNKNOWN:
		PMD_DRV_LOG(ERR, "Unknown link speed");
		link.link_speed = ETH_SPEED_NUM_UNKNOWN;
		break;
	default:
		PMD_DRV_LOG(ERR, "None link speed");
		link.link_speed = ETH_SPEED_NUM_NONE;
		break;
	}

	link.link_autoneg = !(dev->data->dev_conf.link_speeds &
			      ETH_LINK_SPEED_FIXED);

out:
	ice_atomic_write_link_status(dev, &link);
	if (link.link_status == old.link_status)
		return -1;

	return 0;
}

/* Force the physical link state by getting the current PHY capabilities from
 * hardware and setting the PHY config based on the determined capabilities. If
 * link changes, link event will be triggered because both the Enable Automatic
 * Link Update and LESM Enable bits are set when setting the PHY capabilities.
 */
static enum ice_status
ice_force_phys_link_state(struct ice_hw *hw, bool link_up)
{
	struct ice_aqc_set_phy_cfg_data cfg = { 0 };
	struct ice_aqc_get_phy_caps_data *pcaps;
	struct ice_port_info *pi;
	enum ice_status status;

	if (!hw || !hw->port_info)
		return ICE_ERR_PARAM;

	pi = hw->port_info;

	pcaps = (struct ice_aqc_get_phy_caps_data *)
		ice_malloc(hw, sizeof(*pcaps));
	if (!pcaps)
		return ICE_ERR_NO_MEMORY;

	status = ice_aq_get_phy_caps(pi, false, ICE_AQC_REPORT_SW_CFG, pcaps,
				     NULL);
	if (status)
		goto out;

	/* No change in link */
	if (link_up == !!(pcaps->caps & ICE_AQC_PHY_EN_LINK) &&
	    link_up == !!(pi->phy.link_info.link_info & ICE_AQ_LINK_UP))
		goto out;

	cfg.phy_type_low = pcaps->phy_type_low;
	cfg.phy_type_high = pcaps->phy_type_high;
	cfg.caps = pcaps->caps | ICE_AQ_PHY_ENA_AUTO_LINK_UPDT;
	cfg.low_power_ctrl_an = pcaps->low_power_ctrl_an;
	cfg.eee_cap = pcaps->eee_cap;
	cfg.eeer_value = pcaps->eeer_value;
	cfg.link_fec_opt = pcaps->link_fec_options;
	if (link_up)
		cfg.caps |= ICE_AQ_PHY_ENA_LINK;
	else
		cfg.caps &= ~ICE_AQ_PHY_ENA_LINK;

	status = ice_aq_set_phy_cfg(hw, pi, &cfg, NULL);

out:
	ice_free(hw, pcaps);
	return status;
}

static int
ice_dev_set_link_up(struct rte_eth_dev *dev)
{
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	return ice_force_phys_link_state(hw, true);
}

static int
ice_dev_set_link_down(struct rte_eth_dev *dev)
{
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	return ice_force_phys_link_state(hw, false);
}

static int
ice_mtu_set(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct rte_eth_dev_data *dev_data = pf->dev_data;
	uint32_t frame_size = mtu + ICE_ETH_OVERHEAD;

	/* check if mtu is within the allowed range */
	if (mtu < RTE_ETHER_MIN_MTU || frame_size > ICE_FRAME_SIZE_MAX)
		return -EINVAL;

	/* mtu setting is forbidden if port is start */
	if (dev_data->dev_started) {
		PMD_DRV_LOG(ERR,
			    "port %d must be stopped before configuration",
			    dev_data->port_id);
		return -EBUSY;
	}

	if (frame_size > ICE_ETH_MAX_LEN)
		dev_data->dev_conf.rxmode.offloads |=
			DEV_RX_OFFLOAD_JUMBO_FRAME;
	else
		dev_data->dev_conf.rxmode.offloads &=
			~DEV_RX_OFFLOAD_JUMBO_FRAME;

	dev_data->dev_conf.rxmode.max_rx_pkt_len = frame_size;

	return 0;
}

static int ice_macaddr_set(struct rte_eth_dev *dev,
			   struct rte_ether_addr *mac_addr)
{
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_vsi *vsi = pf->main_vsi;
	struct ice_mac_filter *f;
	uint8_t flags = 0;
	int ret;

	if (!rte_is_valid_assigned_ether_addr(mac_addr)) {
		PMD_DRV_LOG(ERR, "Tried to set invalid MAC address.");
		return -EINVAL;
	}

	TAILQ_FOREACH(f, &vsi->mac_list, next) {
		if (rte_is_same_ether_addr(&pf->dev_addr, &f->mac_info.mac_addr))
			break;
	}

	if (!f) {
		PMD_DRV_LOG(ERR, "Failed to find filter for default mac");
		return -EIO;
	}

	ret = ice_remove_mac_filter(vsi, &f->mac_info.mac_addr);
	if (ret != ICE_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to delete mac filter");
		return -EIO;
	}
	ret = ice_add_mac_filter(vsi, mac_addr);
	if (ret != ICE_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to add mac filter");
		return -EIO;
	}
	rte_ether_addr_copy(mac_addr, &pf->dev_addr);

	flags = ICE_AQC_MAN_MAC_UPDATE_LAA_WOL;
	ret = ice_aq_manage_mac_write(hw, mac_addr->addr_bytes, flags, NULL);
	if (ret != ICE_SUCCESS)
		PMD_DRV_LOG(ERR, "Failed to set manage mac");

	return 0;
}

/* Add a MAC address, and update filters */
static int
ice_macaddr_add(struct rte_eth_dev *dev,
		struct rte_ether_addr *mac_addr,
		__rte_unused uint32_t index,
		__rte_unused uint32_t pool)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_vsi *vsi = pf->main_vsi;
	int ret;

	ret = ice_add_mac_filter(vsi, mac_addr);
	if (ret != ICE_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to add MAC filter");
		return -EINVAL;
	}

	return ICE_SUCCESS;
}

/* Remove a MAC address, and update filters */
static void
ice_macaddr_remove(struct rte_eth_dev *dev, uint32_t index)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_vsi *vsi = pf->main_vsi;
	struct rte_eth_dev_data *data = dev->data;
	struct rte_ether_addr *macaddr;
	int ret;

	macaddr = &data->mac_addrs[index];
	ret = ice_remove_mac_filter(vsi, macaddr);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to remove MAC filter");
		return;
	}
}

static int
ice_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vlan_id, int on)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_vsi *vsi = pf->main_vsi;
	int ret;

	PMD_INIT_FUNC_TRACE();

	if (on) {
		ret = ice_add_vlan_filter(vsi, vlan_id);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "Failed to add vlan filter");
			return -EINVAL;
		}
	} else {
		ret = ice_remove_vlan_filter(vsi, vlan_id);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "Failed to remove vlan filter");
			return -EINVAL;
		}
	}

	return 0;
}

/* Configure vlan filter on or off */
static int
ice_vsi_config_vlan_filter(struct ice_vsi *vsi, bool on)
{
	struct ice_hw *hw = ICE_VSI_TO_HW(vsi);
	struct ice_vsi_ctx ctxt;
	uint8_t sw_flags2;
	int ret = 0;

	sw_flags2 = ICE_AQ_VSI_SW_FLAG_RX_VLAN_PRUNE_ENA;

	if (on)
		vsi->info.sw_flags2 |= sw_flags2;
	else
		vsi->info.sw_flags2 &= ~sw_flags2;

	vsi->info.sw_id = hw->port_info->sw_id;
	(void)rte_memcpy(&ctxt.info, &vsi->info, sizeof(vsi->info));
	ctxt.info.valid_sections =
		rte_cpu_to_le_16(ICE_AQ_VSI_PROP_SW_VALID |
				 ICE_AQ_VSI_PROP_SECURITY_VALID);
	ctxt.vsi_num = vsi->vsi_id;

	ret = ice_update_vsi(hw, vsi->idx, &ctxt, NULL);
	if (ret) {
		PMD_DRV_LOG(INFO, "Update VSI failed to %s vlan rx pruning",
			    on ? "enable" : "disable");
		return -EINVAL;
	} else {
		vsi->info.valid_sections |=
			rte_cpu_to_le_16(ICE_AQ_VSI_PROP_SW_VALID |
					 ICE_AQ_VSI_PROP_SECURITY_VALID);
	}

	/* consist with other drivers, allow untagged packet when vlan filter on */
	if (on)
		ret = ice_add_vlan_filter(vsi, 0);
	else
		ret = ice_remove_vlan_filter(vsi, 0);

	return 0;
}

static int
ice_vsi_config_vlan_stripping(struct ice_vsi *vsi, bool on)
{
	struct ice_hw *hw = ICE_VSI_TO_HW(vsi);
	struct ice_vsi_ctx ctxt;
	uint8_t vlan_flags;
	int ret = 0;

	/* Check if it has been already on or off */
	if (vsi->info.valid_sections &
		rte_cpu_to_le_16(ICE_AQ_VSI_PROP_VLAN_VALID)) {
		if (on) {
			if ((vsi->info.vlan_flags &
			     ICE_AQ_VSI_VLAN_EMOD_M) ==
			    ICE_AQ_VSI_VLAN_EMOD_STR_BOTH)
				return 0; /* already on */
		} else {
			if ((vsi->info.vlan_flags &
			     ICE_AQ_VSI_VLAN_EMOD_M) ==
			    ICE_AQ_VSI_VLAN_EMOD_NOTHING)
				return 0; /* already off */
		}
	}

	if (on)
		vlan_flags = ICE_AQ_VSI_VLAN_EMOD_STR_BOTH;
	else
		vlan_flags = ICE_AQ_VSI_VLAN_EMOD_NOTHING;
	vsi->info.vlan_flags &= ~(ICE_AQ_VSI_VLAN_EMOD_M);
	vsi->info.vlan_flags |= vlan_flags;
	(void)rte_memcpy(&ctxt.info, &vsi->info, sizeof(vsi->info));
	ctxt.info.valid_sections =
		rte_cpu_to_le_16(ICE_AQ_VSI_PROP_VLAN_VALID);
	ctxt.vsi_num = vsi->vsi_id;
	ret = ice_update_vsi(hw, vsi->idx, &ctxt, NULL);
	if (ret) {
		PMD_DRV_LOG(INFO, "Update VSI failed to %s vlan stripping",
			    on ? "enable" : "disable");
		return -EINVAL;
	}

	vsi->info.valid_sections |=
		rte_cpu_to_le_16(ICE_AQ_VSI_PROP_VLAN_VALID);

	return ret;
}

static int
ice_vlan_offload_set(struct rte_eth_dev *dev, int mask)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_vsi *vsi = pf->main_vsi;
	struct rte_eth_rxmode *rxmode;

	rxmode = &dev->data->dev_conf.rxmode;
	if (mask & ETH_VLAN_FILTER_MASK) {
		if (rxmode->offloads & DEV_RX_OFFLOAD_VLAN_FILTER)
			ice_vsi_config_vlan_filter(vsi, true);
		else
			ice_vsi_config_vlan_filter(vsi, false);
	}

	if (mask & ETH_VLAN_STRIP_MASK) {
		if (rxmode->offloads & DEV_RX_OFFLOAD_VLAN_STRIP)
			ice_vsi_config_vlan_stripping(vsi, true);
		else
			ice_vsi_config_vlan_stripping(vsi, false);
	}

	if (mask & ETH_VLAN_EXTEND_MASK) {
		if (rxmode->offloads & DEV_RX_OFFLOAD_VLAN_EXTEND)
			ice_vsi_config_double_vlan(vsi, true);
		else
			ice_vsi_config_double_vlan(vsi, false);
	}

	return 0;
}

static int
ice_get_rss_lut(struct ice_vsi *vsi, uint8_t *lut, uint16_t lut_size)
{
	struct ice_aq_get_set_rss_lut_params lut_params;
	struct ice_pf *pf = ICE_VSI_TO_PF(vsi);
	struct ice_hw *hw = ICE_VSI_TO_HW(vsi);
	int ret;

	if (!lut)
		return -EINVAL;

	if (pf->flags & ICE_FLAG_RSS_AQ_CAPABLE) {
		lut_params.vsi_handle = vsi->idx;
		lut_params.lut_size = lut_size;
		lut_params.lut_type = ICE_AQC_GSET_RSS_LUT_TABLE_TYPE_PF;
		lut_params.lut = lut;
		lut_params.global_lut_id = 0;
		ret = ice_aq_get_rss_lut(hw, &lut_params);
		if (ret) {
			PMD_DRV_LOG(ERR, "Failed to get RSS lookup table");
			return -EINVAL;
		}
	} else {
		uint64_t *lut_dw = (uint64_t *)lut;
		uint16_t i, lut_size_dw = lut_size / 4;

		for (i = 0; i < lut_size_dw; i++)
			lut_dw[i] = ICE_READ_REG(hw, PFQF_HLUT(i));
	}

	return 0;
}

static int
ice_set_rss_lut(struct ice_vsi *vsi, uint8_t *lut, uint16_t lut_size)
{
	struct ice_aq_get_set_rss_lut_params lut_params;
	struct ice_pf *pf;
	struct ice_hw *hw;
	int ret;

	if (!vsi || !lut)
		return -EINVAL;

	pf = ICE_VSI_TO_PF(vsi);
	hw = ICE_VSI_TO_HW(vsi);

	if (pf->flags & ICE_FLAG_RSS_AQ_CAPABLE) {
		lut_params.vsi_handle = vsi->idx;
		lut_params.lut_size = lut_size;
		lut_params.lut_type = ICE_AQC_GSET_RSS_LUT_TABLE_TYPE_PF;
		lut_params.lut = lut;
		lut_params.global_lut_id = 0;
		ret = ice_aq_set_rss_lut(hw, &lut_params);
		if (ret) {
			PMD_DRV_LOG(ERR, "Failed to set RSS lookup table");
			return -EINVAL;
		}
	} else {
		uint64_t *lut_dw = (uint64_t *)lut;
		uint16_t i, lut_size_dw = lut_size / 4;

		for (i = 0; i < lut_size_dw; i++)
			ICE_WRITE_REG(hw, PFQF_HLUT(i), lut_dw[i]);

		ice_flush(hw);
	}

	return 0;
}

static int
ice_rss_reta_update(struct rte_eth_dev *dev,
		    struct rte_eth_rss_reta_entry64 *reta_conf,
		    uint16_t reta_size)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	uint16_t i, lut_size = pf->hash_lut_size;
	uint16_t idx, shift;
	uint8_t *lut;
	int ret;

	if (reta_size != ICE_AQC_GSET_RSS_LUT_TABLE_SIZE_128 &&
	    reta_size != ICE_AQC_GSET_RSS_LUT_TABLE_SIZE_512 &&
	    reta_size != ICE_AQC_GSET_RSS_LUT_TABLE_SIZE_2K) {
		PMD_DRV_LOG(ERR,
			    "The size of hash lookup table configured (%d)"
			    "doesn't match the number hardware can "
			    "supported (128, 512, 2048)",
			    reta_size);
		return -EINVAL;
	}

	/* It MUST use the current LUT size to get the RSS lookup table,
	 * otherwise if will fail with -100 error code.
	 */
	lut = rte_zmalloc(NULL,  RTE_MAX(reta_size, lut_size), 0);
	if (!lut) {
		PMD_DRV_LOG(ERR, "No memory can be allocated");
		return -ENOMEM;
	}
	ret = ice_get_rss_lut(pf->main_vsi, lut, lut_size);
	if (ret)
		goto out;

	for (i = 0; i < reta_size; i++) {
		idx = i / RTE_RETA_GROUP_SIZE;
		shift = i % RTE_RETA_GROUP_SIZE;
		if (reta_conf[idx].mask & (1ULL << shift))
			lut[i] = reta_conf[idx].reta[shift];
	}
	ret = ice_set_rss_lut(pf->main_vsi, lut, reta_size);
	if (ret == 0 && lut_size != reta_size) {
		PMD_DRV_LOG(INFO,
			    "The size of hash lookup table is changed from (%d) to (%d)",
			    lut_size, reta_size);
		pf->hash_lut_size = reta_size;
	}

out:
	rte_free(lut);

	return ret;
}

static int
ice_rss_reta_query(struct rte_eth_dev *dev,
		   struct rte_eth_rss_reta_entry64 *reta_conf,
		   uint16_t reta_size)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	uint16_t i, lut_size = pf->hash_lut_size;
	uint16_t idx, shift;
	uint8_t *lut;
	int ret;

	if (reta_size != lut_size) {
		PMD_DRV_LOG(ERR,
			    "The size of hash lookup table configured (%d)"
			    "doesn't match the number hardware can "
			    "supported (%d)",
			    reta_size, lut_size);
		return -EINVAL;
	}

	lut = rte_zmalloc(NULL, reta_size, 0);
	if (!lut) {
		PMD_DRV_LOG(ERR, "No memory can be allocated");
		return -ENOMEM;
	}

	ret = ice_get_rss_lut(pf->main_vsi, lut, reta_size);
	if (ret)
		goto out;

	for (i = 0; i < reta_size; i++) {
		idx = i / RTE_RETA_GROUP_SIZE;
		shift = i % RTE_RETA_GROUP_SIZE;
		if (reta_conf[idx].mask & (1ULL << shift))
			reta_conf[idx].reta[shift] = lut[i];
	}

out:
	rte_free(lut);

	return ret;
}

static int
ice_set_rss_key(struct ice_vsi *vsi, uint8_t *key, uint8_t key_len)
{
	struct ice_hw *hw = ICE_VSI_TO_HW(vsi);
	int ret = 0;

	if (!key || key_len == 0) {
		PMD_DRV_LOG(DEBUG, "No key to be configured");
		return 0;
	} else if (key_len != (VSIQF_HKEY_MAX_INDEX + 1) *
		   sizeof(uint32_t)) {
		PMD_DRV_LOG(ERR, "Invalid key length %u", key_len);
		return -EINVAL;
	}

	struct ice_aqc_get_set_rss_keys *key_dw =
		(struct ice_aqc_get_set_rss_keys *)key;

	ret = ice_aq_set_rss_key(hw, vsi->idx, key_dw);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to configure RSS key via AQ");
		ret = -EINVAL;
	}

	return ret;
}

static int
ice_get_rss_key(struct ice_vsi *vsi, uint8_t *key, uint8_t *key_len)
{
	struct ice_hw *hw = ICE_VSI_TO_HW(vsi);
	int ret;

	if (!key || !key_len)
		return -EINVAL;

	ret = ice_aq_get_rss_key
		(hw, vsi->idx,
		 (struct ice_aqc_get_set_rss_keys *)key);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to get RSS key via AQ");
		return -EINVAL;
	}
	*key_len = (VSIQF_HKEY_MAX_INDEX + 1) * sizeof(uint32_t);

	return 0;
}

static int
ice_rss_hash_update(struct rte_eth_dev *dev,
		    struct rte_eth_rss_conf *rss_conf)
{
	enum ice_status status = ICE_SUCCESS;
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_vsi *vsi = pf->main_vsi;

	/* set hash key */
	status = ice_set_rss_key(vsi, rss_conf->rss_key, rss_conf->rss_key_len);
	if (status)
		return status;

	if (rss_conf->rss_hf == 0) {
		pf->rss_hf = 0;
		return 0;
	}

	/* RSS hash configuration */
	ice_rss_hash_set(pf, rss_conf->rss_hf);

	return 0;
}

static int
ice_rss_hash_conf_get(struct rte_eth_dev *dev,
		      struct rte_eth_rss_conf *rss_conf)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_vsi *vsi = pf->main_vsi;

	ice_get_rss_key(vsi, rss_conf->rss_key,
			&rss_conf->rss_key_len);

	rss_conf->rss_hf = pf->rss_hf;
	return 0;
}

static int
ice_promisc_enable(struct rte_eth_dev *dev)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ice_vsi *vsi = pf->main_vsi;
	enum ice_status status;
	uint8_t pmask;
	int ret = 0;

	pmask = ICE_PROMISC_UCAST_RX | ICE_PROMISC_UCAST_TX |
		ICE_PROMISC_MCAST_RX | ICE_PROMISC_MCAST_TX;

	status = ice_set_vsi_promisc(hw, vsi->idx, pmask, 0);
	switch (status) {
	case ICE_ERR_ALREADY_EXISTS:
		PMD_DRV_LOG(DEBUG, "Promisc mode has already been enabled");
	case ICE_SUCCESS:
		break;
	default:
		PMD_DRV_LOG(ERR, "Failed to enable promisc, err=%d", status);
		ret = -EAGAIN;
	}

	return ret;
}

static int
ice_promisc_disable(struct rte_eth_dev *dev)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ice_vsi *vsi = pf->main_vsi;
	enum ice_status status;
	uint8_t pmask;
	int ret = 0;

	if (dev->data->all_multicast == 1)
		pmask = ICE_PROMISC_UCAST_RX | ICE_PROMISC_UCAST_TX;
	else
		pmask = ICE_PROMISC_UCAST_RX | ICE_PROMISC_UCAST_TX |
			ICE_PROMISC_MCAST_RX | ICE_PROMISC_MCAST_TX;

	status = ice_clear_vsi_promisc(hw, vsi->idx, pmask, 0);
	if (status != ICE_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to clear promisc, err=%d", status);
		ret = -EAGAIN;
	}

	return ret;
}

static int
ice_allmulti_enable(struct rte_eth_dev *dev)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ice_vsi *vsi = pf->main_vsi;
	enum ice_status status;
	uint8_t pmask;
	int ret = 0;

	pmask = ICE_PROMISC_MCAST_RX | ICE_PROMISC_MCAST_TX;

	status = ice_set_vsi_promisc(hw, vsi->idx, pmask, 0);

	switch (status) {
	case ICE_ERR_ALREADY_EXISTS:
		PMD_DRV_LOG(DEBUG, "Allmulti has already been enabled");
	case ICE_SUCCESS:
		break;
	default:
		PMD_DRV_LOG(ERR, "Failed to enable allmulti, err=%d", status);
		ret = -EAGAIN;
	}

	return ret;
}

static int
ice_allmulti_disable(struct rte_eth_dev *dev)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ice_vsi *vsi = pf->main_vsi;
	enum ice_status status;
	uint8_t pmask;
	int ret = 0;

	if (dev->data->promiscuous == 1)
		return 0; /* must remain in all_multicast mode */

	pmask = ICE_PROMISC_MCAST_RX | ICE_PROMISC_MCAST_TX;

	status = ice_clear_vsi_promisc(hw, vsi->idx, pmask, 0);
	if (status != ICE_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to clear allmulti, err=%d", status);
		ret = -EAGAIN;
	}

	return ret;
}

static int ice_rx_queue_intr_enable(struct rte_eth_dev *dev,
				    uint16_t queue_id)
{
	struct rte_pci_device *pci_dev = ICE_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t val;
	uint16_t msix_intr;

	msix_intr = intr_handle->intr_vec[queue_id];

	val = GLINT_DYN_CTL_INTENA_M | GLINT_DYN_CTL_CLEARPBA_M |
	      GLINT_DYN_CTL_ITR_INDX_M;
	val &= ~GLINT_DYN_CTL_WB_ON_ITR_M;

	ICE_WRITE_REG(hw, GLINT_DYN_CTL(msix_intr), val);
	rte_intr_ack(&pci_dev->intr_handle);

	return 0;
}

static int ice_rx_queue_intr_disable(struct rte_eth_dev *dev,
				     uint16_t queue_id)
{
	struct rte_pci_device *pci_dev = ICE_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint16_t msix_intr;

	msix_intr = intr_handle->intr_vec[queue_id];

	ICE_WRITE_REG(hw, GLINT_DYN_CTL(msix_intr), GLINT_DYN_CTL_WB_ON_ITR_M);

	return 0;
}

static int
ice_fw_version_get(struct rte_eth_dev *dev, char *fw_version, size_t fw_size)
{
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	u8 ver, patch;
	u16 build;
	int ret;

	ver = hw->flash.orom.major;
	patch = hw->flash.orom.patch;
	build = hw->flash.orom.build;

	ret = snprintf(fw_version, fw_size,
			"%x.%02x 0x%08x %d.%d.%d",
			hw->flash.nvm.major,
			hw->flash.nvm.minor,
			hw->flash.nvm.eetrack,
			ver, build, patch);
	if (ret < 0)
		return -EINVAL;

	/* add the size of '\0' */
	ret += 1;
	if (fw_size < (size_t)ret)
		return ret;
	else
		return 0;
}

static int
ice_vsi_vlan_pvid_set(struct ice_vsi *vsi, struct ice_vsi_vlan_pvid_info *info)
{
	struct ice_hw *hw;
	struct ice_vsi_ctx ctxt;
	uint8_t vlan_flags = 0;
	int ret;

	if (!vsi || !info) {
		PMD_DRV_LOG(ERR, "invalid parameters");
		return -EINVAL;
	}

	if (info->on) {
		vsi->info.pvid = info->config.pvid;
		/**
		 * If insert pvid is enabled, only tagged pkts are
		 * allowed to be sent out.
		 */
		vlan_flags = ICE_AQ_VSI_PVLAN_INSERT_PVID |
			     ICE_AQ_VSI_VLAN_MODE_UNTAGGED;
	} else {
		vsi->info.pvid = 0;
		if (info->config.reject.tagged == 0)
			vlan_flags |= ICE_AQ_VSI_VLAN_MODE_TAGGED;

		if (info->config.reject.untagged == 0)
			vlan_flags |= ICE_AQ_VSI_VLAN_MODE_UNTAGGED;
	}
	vsi->info.vlan_flags &= ~(ICE_AQ_VSI_PVLAN_INSERT_PVID |
				  ICE_AQ_VSI_VLAN_MODE_M);
	vsi->info.vlan_flags |= vlan_flags;
	memset(&ctxt, 0, sizeof(ctxt));
	rte_memcpy(&ctxt.info, &vsi->info, sizeof(vsi->info));
	ctxt.info.valid_sections =
		rte_cpu_to_le_16(ICE_AQ_VSI_PROP_VLAN_VALID);
	ctxt.vsi_num = vsi->vsi_id;

	hw = ICE_VSI_TO_HW(vsi);
	ret = ice_update_vsi(hw, vsi->idx, &ctxt, NULL);
	if (ret != ICE_SUCCESS) {
		PMD_DRV_LOG(ERR,
			    "update VSI for VLAN insert failed, err %d",
			    ret);
		return -EINVAL;
	}

	vsi->info.valid_sections |=
		rte_cpu_to_le_16(ICE_AQ_VSI_PROP_VLAN_VALID);

	return ret;
}

static int
ice_vlan_pvid_set(struct rte_eth_dev *dev, uint16_t pvid, int on)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_vsi *vsi = pf->main_vsi;
	struct rte_eth_dev_data *data = pf->dev_data;
	struct ice_vsi_vlan_pvid_info info;
	int ret;

	memset(&info, 0, sizeof(info));
	info.on = on;
	if (info.on) {
		info.config.pvid = pvid;
	} else {
		info.config.reject.tagged =
			data->dev_conf.txmode.hw_vlan_reject_tagged;
		info.config.reject.untagged =
			data->dev_conf.txmode.hw_vlan_reject_untagged;
	}

	ret = ice_vsi_vlan_pvid_set(vsi, &info);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to set pvid.");
		return -EINVAL;
	}

	return 0;
}

static int
ice_get_eeprom_length(struct rte_eth_dev *dev)
{
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	return hw->flash.flash_size;
}

static int
ice_get_eeprom(struct rte_eth_dev *dev,
	       struct rte_dev_eeprom_info *eeprom)
{
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	enum ice_status status = ICE_SUCCESS;
	uint8_t *data = eeprom->data;

	eeprom->magic = hw->vendor_id | (hw->device_id << 16);

	status = ice_acquire_nvm(hw, ICE_RES_READ);
	if (status) {
		PMD_DRV_LOG(ERR, "acquire nvm failed.");
		return -EIO;
	}

	status = ice_read_flat_nvm(hw, eeprom->offset, &eeprom->length,
				   data, false);

	ice_release_nvm(hw);

	if (status) {
		PMD_DRV_LOG(ERR, "EEPROM read failed.");
		return -EIO;
	}

	return 0;
}

static void
ice_stat_update_32(struct ice_hw *hw,
		   uint32_t reg,
		   bool offset_loaded,
		   uint64_t *offset,
		   uint64_t *stat)
{
	uint64_t new_data;

	new_data = (uint64_t)ICE_READ_REG(hw, reg);
	if (!offset_loaded)
		*offset = new_data;

	if (new_data >= *offset)
		*stat = (uint64_t)(new_data - *offset);
	else
		*stat = (uint64_t)((new_data +
				    ((uint64_t)1 << ICE_32_BIT_WIDTH))
				   - *offset);
}

static void
ice_stat_update_40(struct ice_hw *hw,
		   uint32_t hireg,
		   uint32_t loreg,
		   bool offset_loaded,
		   uint64_t *offset,
		   uint64_t *stat)
{
	uint64_t new_data;

	new_data = (uint64_t)ICE_READ_REG(hw, loreg);
	new_data |= (uint64_t)(ICE_READ_REG(hw, hireg) & ICE_8_BIT_MASK) <<
		    ICE_32_BIT_WIDTH;

	if (!offset_loaded)
		*offset = new_data;

	if (new_data >= *offset)
		*stat = new_data - *offset;
	else
		*stat = (uint64_t)((new_data +
				    ((uint64_t)1 << ICE_40_BIT_WIDTH)) -
				   *offset);

	*stat &= ICE_40_BIT_MASK;
}

/* Get all the statistics of a VSI */
static void
ice_update_vsi_stats(struct ice_vsi *vsi)
{
	struct ice_eth_stats *oes = &vsi->eth_stats_offset;
	struct ice_eth_stats *nes = &vsi->eth_stats;
	struct ice_hw *hw = ICE_VSI_TO_HW(vsi);
	int idx = rte_le_to_cpu_16(vsi->vsi_id);

	ice_stat_update_40(hw, GLV_GORCH(idx), GLV_GORCL(idx),
			   vsi->offset_loaded, &oes->rx_bytes,
			   &nes->rx_bytes);
	ice_stat_update_40(hw, GLV_UPRCH(idx), GLV_UPRCL(idx),
			   vsi->offset_loaded, &oes->rx_unicast,
			   &nes->rx_unicast);
	ice_stat_update_40(hw, GLV_MPRCH(idx), GLV_MPRCL(idx),
			   vsi->offset_loaded, &oes->rx_multicast,
			   &nes->rx_multicast);
	ice_stat_update_40(hw, GLV_BPRCH(idx), GLV_BPRCL(idx),
			   vsi->offset_loaded, &oes->rx_broadcast,
			   &nes->rx_broadcast);
	/* enlarge the limitation when rx_bytes overflowed */
	if (vsi->offset_loaded) {
		if (ICE_RXTX_BYTES_LOW(vsi->old_rx_bytes) > nes->rx_bytes)
			nes->rx_bytes += (uint64_t)1 << ICE_40_BIT_WIDTH;
		nes->rx_bytes += ICE_RXTX_BYTES_HIGH(vsi->old_rx_bytes);
	}
	vsi->old_rx_bytes = nes->rx_bytes;
	/* exclude CRC bytes */
	nes->rx_bytes -= (nes->rx_unicast + nes->rx_multicast +
			  nes->rx_broadcast) * RTE_ETHER_CRC_LEN;

	ice_stat_update_32(hw, GLV_RDPC(idx), vsi->offset_loaded,
			   &oes->rx_discards, &nes->rx_discards);
	/* GLV_REPC not supported */
	/* GLV_RMPC not supported */
	ice_stat_update_32(hw, GLSWID_RUPP(idx), vsi->offset_loaded,
			   &oes->rx_unknown_protocol,
			   &nes->rx_unknown_protocol);
	ice_stat_update_40(hw, GLV_GOTCH(idx), GLV_GOTCL(idx),
			   vsi->offset_loaded, &oes->tx_bytes,
			   &nes->tx_bytes);
	ice_stat_update_40(hw, GLV_UPTCH(idx), GLV_UPTCL(idx),
			   vsi->offset_loaded, &oes->tx_unicast,
			   &nes->tx_unicast);
	ice_stat_update_40(hw, GLV_MPTCH(idx), GLV_MPTCL(idx),
			   vsi->offset_loaded, &oes->tx_multicast,
			   &nes->tx_multicast);
	ice_stat_update_40(hw, GLV_BPTCH(idx), GLV_BPTCL(idx),
			   vsi->offset_loaded,  &oes->tx_broadcast,
			   &nes->tx_broadcast);
	/* GLV_TDPC not supported */
	ice_stat_update_32(hw, GLV_TEPC(idx), vsi->offset_loaded,
			   &oes->tx_errors, &nes->tx_errors);
	/* enlarge the limitation when tx_bytes overflowed */
	if (vsi->offset_loaded) {
		if (ICE_RXTX_BYTES_LOW(vsi->old_tx_bytes) > nes->tx_bytes)
			nes->tx_bytes += (uint64_t)1 << ICE_40_BIT_WIDTH;
		nes->tx_bytes += ICE_RXTX_BYTES_HIGH(vsi->old_tx_bytes);
	}
	vsi->old_tx_bytes = nes->tx_bytes;
	vsi->offset_loaded = true;

	PMD_DRV_LOG(DEBUG, "************** VSI[%u] stats start **************",
		    vsi->vsi_id);
	PMD_DRV_LOG(DEBUG, "rx_bytes:            %"PRIu64"", nes->rx_bytes);
	PMD_DRV_LOG(DEBUG, "rx_unicast:          %"PRIu64"", nes->rx_unicast);
	PMD_DRV_LOG(DEBUG, "rx_multicast:        %"PRIu64"", nes->rx_multicast);
	PMD_DRV_LOG(DEBUG, "rx_broadcast:        %"PRIu64"", nes->rx_broadcast);
	PMD_DRV_LOG(DEBUG, "rx_discards:         %"PRIu64"", nes->rx_discards);
	PMD_DRV_LOG(DEBUG, "rx_unknown_protocol: %"PRIu64"",
		    nes->rx_unknown_protocol);
	PMD_DRV_LOG(DEBUG, "tx_bytes:            %"PRIu64"", nes->tx_bytes);
	PMD_DRV_LOG(DEBUG, "tx_unicast:          %"PRIu64"", nes->tx_unicast);
	PMD_DRV_LOG(DEBUG, "tx_multicast:        %"PRIu64"", nes->tx_multicast);
	PMD_DRV_LOG(DEBUG, "tx_broadcast:        %"PRIu64"", nes->tx_broadcast);
	PMD_DRV_LOG(DEBUG, "tx_discards:         %"PRIu64"", nes->tx_discards);
	PMD_DRV_LOG(DEBUG, "tx_errors:           %"PRIu64"", nes->tx_errors);
	PMD_DRV_LOG(DEBUG, "************** VSI[%u] stats end ****************",
		    vsi->vsi_id);
}

static void
ice_read_stats_registers(struct ice_pf *pf, struct ice_hw *hw)
{
	struct ice_hw_port_stats *ns = &pf->stats; /* new stats */
	struct ice_hw_port_stats *os = &pf->stats_offset; /* old stats */

	/* Get statistics of struct ice_eth_stats */
	ice_stat_update_40(hw, GLPRT_GORCH(hw->port_info->lport),
			   GLPRT_GORCL(hw->port_info->lport),
			   pf->offset_loaded, &os->eth.rx_bytes,
			   &ns->eth.rx_bytes);
	ice_stat_update_40(hw, GLPRT_UPRCH(hw->port_info->lport),
			   GLPRT_UPRCL(hw->port_info->lport),
			   pf->offset_loaded, &os->eth.rx_unicast,
			   &ns->eth.rx_unicast);
	ice_stat_update_40(hw, GLPRT_MPRCH(hw->port_info->lport),
			   GLPRT_MPRCL(hw->port_info->lport),
			   pf->offset_loaded, &os->eth.rx_multicast,
			   &ns->eth.rx_multicast);
	ice_stat_update_40(hw, GLPRT_BPRCH(hw->port_info->lport),
			   GLPRT_BPRCL(hw->port_info->lport),
			   pf->offset_loaded, &os->eth.rx_broadcast,
			   &ns->eth.rx_broadcast);
	ice_stat_update_32(hw, PRTRPB_RDPC,
			   pf->offset_loaded, &os->eth.rx_discards,
			   &ns->eth.rx_discards);
	/* enlarge the limitation when rx_bytes overflowed */
	if (pf->offset_loaded) {
		if (ICE_RXTX_BYTES_LOW(pf->old_rx_bytes) > ns->eth.rx_bytes)
			ns->eth.rx_bytes += (uint64_t)1 << ICE_40_BIT_WIDTH;
		ns->eth.rx_bytes += ICE_RXTX_BYTES_HIGH(pf->old_rx_bytes);
	}
	pf->old_rx_bytes = ns->eth.rx_bytes;

	/* Workaround: CRC size should not be included in byte statistics,
	 * so subtract RTE_ETHER_CRC_LEN from the byte counter for each rx
	 * packet.
	 */
	ns->eth.rx_bytes -= (ns->eth.rx_unicast + ns->eth.rx_multicast +
			     ns->eth.rx_broadcast) * RTE_ETHER_CRC_LEN;

	/* GLPRT_REPC not supported */
	/* GLPRT_RMPC not supported */
	ice_stat_update_32(hw, GLSWID_RUPP(hw->port_info->lport),
			   pf->offset_loaded,
			   &os->eth.rx_unknown_protocol,
			   &ns->eth.rx_unknown_protocol);
	ice_stat_update_40(hw, GLPRT_GOTCH(hw->port_info->lport),
			   GLPRT_GOTCL(hw->port_info->lport),
			   pf->offset_loaded, &os->eth.tx_bytes,
			   &ns->eth.tx_bytes);
	ice_stat_update_40(hw, GLPRT_UPTCH(hw->port_info->lport),
			   GLPRT_UPTCL(hw->port_info->lport),
			   pf->offset_loaded, &os->eth.tx_unicast,
			   &ns->eth.tx_unicast);
	ice_stat_update_40(hw, GLPRT_MPTCH(hw->port_info->lport),
			   GLPRT_MPTCL(hw->port_info->lport),
			   pf->offset_loaded, &os->eth.tx_multicast,
			   &ns->eth.tx_multicast);
	ice_stat_update_40(hw, GLPRT_BPTCH(hw->port_info->lport),
			   GLPRT_BPTCL(hw->port_info->lport),
			   pf->offset_loaded, &os->eth.tx_broadcast,
			   &ns->eth.tx_broadcast);
	/* enlarge the limitation when tx_bytes overflowed */
	if (pf->offset_loaded) {
		if (ICE_RXTX_BYTES_LOW(pf->old_tx_bytes) > ns->eth.tx_bytes)
			ns->eth.tx_bytes += (uint64_t)1 << ICE_40_BIT_WIDTH;
		ns->eth.tx_bytes += ICE_RXTX_BYTES_HIGH(pf->old_tx_bytes);
	}
	pf->old_tx_bytes = ns->eth.tx_bytes;
	ns->eth.tx_bytes -= (ns->eth.tx_unicast + ns->eth.tx_multicast +
			     ns->eth.tx_broadcast) * RTE_ETHER_CRC_LEN;

	/* GLPRT_TEPC not supported */

	/* additional port specific stats */
	ice_stat_update_32(hw, GLPRT_TDOLD(hw->port_info->lport),
			   pf->offset_loaded, &os->tx_dropped_link_down,
			   &ns->tx_dropped_link_down);
	ice_stat_update_32(hw, GLPRT_CRCERRS(hw->port_info->lport),
			   pf->offset_loaded, &os->crc_errors,
			   &ns->crc_errors);
	ice_stat_update_32(hw, GLPRT_ILLERRC(hw->port_info->lport),
			   pf->offset_loaded, &os->illegal_bytes,
			   &ns->illegal_bytes);
	/* GLPRT_ERRBC not supported */
	ice_stat_update_32(hw, GLPRT_MLFC(hw->port_info->lport),
			   pf->offset_loaded, &os->mac_local_faults,
			   &ns->mac_local_faults);
	ice_stat_update_32(hw, GLPRT_MRFC(hw->port_info->lport),
			   pf->offset_loaded, &os->mac_remote_faults,
			   &ns->mac_remote_faults);

	ice_stat_update_32(hw, GLPRT_RLEC(hw->port_info->lport),
			   pf->offset_loaded, &os->rx_len_errors,
			   &ns->rx_len_errors);

	ice_stat_update_32(hw, GLPRT_LXONRXC(hw->port_info->lport),
			   pf->offset_loaded, &os->link_xon_rx,
			   &ns->link_xon_rx);
	ice_stat_update_32(hw, GLPRT_LXOFFRXC(hw->port_info->lport),
			   pf->offset_loaded, &os->link_xoff_rx,
			   &ns->link_xoff_rx);
	ice_stat_update_32(hw, GLPRT_LXONTXC(hw->port_info->lport),
			   pf->offset_loaded, &os->link_xon_tx,
			   &ns->link_xon_tx);
	ice_stat_update_32(hw, GLPRT_LXOFFTXC(hw->port_info->lport),
			   pf->offset_loaded, &os->link_xoff_tx,
			   &ns->link_xoff_tx);
	ice_stat_update_40(hw, GLPRT_PRC64H(hw->port_info->lport),
			   GLPRT_PRC64L(hw->port_info->lport),
			   pf->offset_loaded, &os->rx_size_64,
			   &ns->rx_size_64);
	ice_stat_update_40(hw, GLPRT_PRC127H(hw->port_info->lport),
			   GLPRT_PRC127L(hw->port_info->lport),
			   pf->offset_loaded, &os->rx_size_127,
			   &ns->rx_size_127);
	ice_stat_update_40(hw, GLPRT_PRC255H(hw->port_info->lport),
			   GLPRT_PRC255L(hw->port_info->lport),
			   pf->offset_loaded, &os->rx_size_255,
			   &ns->rx_size_255);
	ice_stat_update_40(hw, GLPRT_PRC511H(hw->port_info->lport),
			   GLPRT_PRC511L(hw->port_info->lport),
			   pf->offset_loaded, &os->rx_size_511,
			   &ns->rx_size_511);
	ice_stat_update_40(hw, GLPRT_PRC1023H(hw->port_info->lport),
			   GLPRT_PRC1023L(hw->port_info->lport),
			   pf->offset_loaded, &os->rx_size_1023,
			   &ns->rx_size_1023);
	ice_stat_update_40(hw, GLPRT_PRC1522H(hw->port_info->lport),
			   GLPRT_PRC1522L(hw->port_info->lport),
			   pf->offset_loaded, &os->rx_size_1522,
			   &ns->rx_size_1522);
	ice_stat_update_40(hw, GLPRT_PRC9522H(hw->port_info->lport),
			   GLPRT_PRC9522L(hw->port_info->lport),
			   pf->offset_loaded, &os->rx_size_big,
			   &ns->rx_size_big);
	ice_stat_update_32(hw, GLPRT_RUC(hw->port_info->lport),
			   pf->offset_loaded, &os->rx_undersize,
			   &ns->rx_undersize);
	ice_stat_update_32(hw, GLPRT_RFC(hw->port_info->lport),
			   pf->offset_loaded, &os->rx_fragments,
			   &ns->rx_fragments);
	ice_stat_update_32(hw, GLPRT_ROC(hw->port_info->lport),
			   pf->offset_loaded, &os->rx_oversize,
			   &ns->rx_oversize);
	ice_stat_update_32(hw, GLPRT_RJC(hw->port_info->lport),
			   pf->offset_loaded, &os->rx_jabber,
			   &ns->rx_jabber);
	ice_stat_update_40(hw, GLPRT_PTC64H(hw->port_info->lport),
			   GLPRT_PTC64L(hw->port_info->lport),
			   pf->offset_loaded, &os->tx_size_64,
			   &ns->tx_size_64);
	ice_stat_update_40(hw, GLPRT_PTC127H(hw->port_info->lport),
			   GLPRT_PTC127L(hw->port_info->lport),
			   pf->offset_loaded, &os->tx_size_127,
			   &ns->tx_size_127);
	ice_stat_update_40(hw, GLPRT_PTC255H(hw->port_info->lport),
			   GLPRT_PTC255L(hw->port_info->lport),
			   pf->offset_loaded, &os->tx_size_255,
			   &ns->tx_size_255);
	ice_stat_update_40(hw, GLPRT_PTC511H(hw->port_info->lport),
			   GLPRT_PTC511L(hw->port_info->lport),
			   pf->offset_loaded, &os->tx_size_511,
			   &ns->tx_size_511);
	ice_stat_update_40(hw, GLPRT_PTC1023H(hw->port_info->lport),
			   GLPRT_PTC1023L(hw->port_info->lport),
			   pf->offset_loaded, &os->tx_size_1023,
			   &ns->tx_size_1023);
	ice_stat_update_40(hw, GLPRT_PTC1522H(hw->port_info->lport),
			   GLPRT_PTC1522L(hw->port_info->lport),
			   pf->offset_loaded, &os->tx_size_1522,
			   &ns->tx_size_1522);
	ice_stat_update_40(hw, GLPRT_PTC9522H(hw->port_info->lport),
			   GLPRT_PTC9522L(hw->port_info->lport),
			   pf->offset_loaded, &os->tx_size_big,
			   &ns->tx_size_big);

	/* GLPRT_MSPDC not supported */
	/* GLPRT_XEC not supported */

	pf->offset_loaded = true;

	if (pf->main_vsi)
		ice_update_vsi_stats(pf->main_vsi);
}

/* Get all statistics of a port */
static int
ice_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ice_hw_port_stats *ns = &pf->stats; /* new stats */

	/* call read registers - updates values, now write them to struct */
	ice_read_stats_registers(pf, hw);

	stats->ipackets = pf->main_vsi->eth_stats.rx_unicast +
			  pf->main_vsi->eth_stats.rx_multicast +
			  pf->main_vsi->eth_stats.rx_broadcast -
			  pf->main_vsi->eth_stats.rx_discards;
	stats->opackets = ns->eth.tx_unicast +
			  ns->eth.tx_multicast +
			  ns->eth.tx_broadcast;
	stats->ibytes   = pf->main_vsi->eth_stats.rx_bytes;
	stats->obytes   = ns->eth.tx_bytes;
	stats->oerrors  = ns->eth.tx_errors +
			  pf->main_vsi->eth_stats.tx_errors;

	/* Rx Errors */
	stats->imissed  = ns->eth.rx_discards +
			  pf->main_vsi->eth_stats.rx_discards;
	stats->ierrors  = ns->crc_errors +
			  ns->rx_undersize +
			  ns->rx_oversize + ns->rx_fragments + ns->rx_jabber;

	PMD_DRV_LOG(DEBUG, "*************** PF stats start *****************");
	PMD_DRV_LOG(DEBUG, "rx_bytes:	%"PRIu64"", ns->eth.rx_bytes);
	PMD_DRV_LOG(DEBUG, "rx_unicast:	%"PRIu64"", ns->eth.rx_unicast);
	PMD_DRV_LOG(DEBUG, "rx_multicast:%"PRIu64"", ns->eth.rx_multicast);
	PMD_DRV_LOG(DEBUG, "rx_broadcast:%"PRIu64"", ns->eth.rx_broadcast);
	PMD_DRV_LOG(DEBUG, "rx_discards:%"PRIu64"", ns->eth.rx_discards);
	PMD_DRV_LOG(DEBUG, "vsi rx_discards:%"PRIu64"",
		    pf->main_vsi->eth_stats.rx_discards);
	PMD_DRV_LOG(DEBUG, "rx_unknown_protocol:  %"PRIu64"",
		    ns->eth.rx_unknown_protocol);
	PMD_DRV_LOG(DEBUG, "tx_bytes:	%"PRIu64"", ns->eth.tx_bytes);
	PMD_DRV_LOG(DEBUG, "tx_unicast:	%"PRIu64"", ns->eth.tx_unicast);
	PMD_DRV_LOG(DEBUG, "tx_multicast:%"PRIu64"", ns->eth.tx_multicast);
	PMD_DRV_LOG(DEBUG, "tx_broadcast:%"PRIu64"", ns->eth.tx_broadcast);
	PMD_DRV_LOG(DEBUG, "tx_discards:%"PRIu64"", ns->eth.tx_discards);
	PMD_DRV_LOG(DEBUG, "vsi tx_discards:%"PRIu64"",
		    pf->main_vsi->eth_stats.tx_discards);
	PMD_DRV_LOG(DEBUG, "tx_errors:		%"PRIu64"", ns->eth.tx_errors);

	PMD_DRV_LOG(DEBUG, "tx_dropped_link_down:	%"PRIu64"",
		    ns->tx_dropped_link_down);
	PMD_DRV_LOG(DEBUG, "crc_errors:	%"PRIu64"", ns->crc_errors);
	PMD_DRV_LOG(DEBUG, "illegal_bytes:	%"PRIu64"",
		    ns->illegal_bytes);
	PMD_DRV_LOG(DEBUG, "error_bytes:	%"PRIu64"", ns->error_bytes);
	PMD_DRV_LOG(DEBUG, "mac_local_faults:	%"PRIu64"",
		    ns->mac_local_faults);
	PMD_DRV_LOG(DEBUG, "mac_remote_faults:	%"PRIu64"",
		    ns->mac_remote_faults);
	PMD_DRV_LOG(DEBUG, "link_xon_rx:	%"PRIu64"", ns->link_xon_rx);
	PMD_DRV_LOG(DEBUG, "link_xoff_rx:	%"PRIu64"", ns->link_xoff_rx);
	PMD_DRV_LOG(DEBUG, "link_xon_tx:	%"PRIu64"", ns->link_xon_tx);
	PMD_DRV_LOG(DEBUG, "link_xoff_tx:	%"PRIu64"", ns->link_xoff_tx);
	PMD_DRV_LOG(DEBUG, "rx_size_64:		%"PRIu64"", ns->rx_size_64);
	PMD_DRV_LOG(DEBUG, "rx_size_127:	%"PRIu64"", ns->rx_size_127);
	PMD_DRV_LOG(DEBUG, "rx_size_255:	%"PRIu64"", ns->rx_size_255);
	PMD_DRV_LOG(DEBUG, "rx_size_511:	%"PRIu64"", ns->rx_size_511);
	PMD_DRV_LOG(DEBUG, "rx_size_1023:	%"PRIu64"", ns->rx_size_1023);
	PMD_DRV_LOG(DEBUG, "rx_size_1522:	%"PRIu64"", ns->rx_size_1522);
	PMD_DRV_LOG(DEBUG, "rx_size_big:	%"PRIu64"", ns->rx_size_big);
	PMD_DRV_LOG(DEBUG, "rx_undersize:	%"PRIu64"", ns->rx_undersize);
	PMD_DRV_LOG(DEBUG, "rx_fragments:	%"PRIu64"", ns->rx_fragments);
	PMD_DRV_LOG(DEBUG, "rx_oversize:	%"PRIu64"", ns->rx_oversize);
	PMD_DRV_LOG(DEBUG, "rx_jabber:		%"PRIu64"", ns->rx_jabber);
	PMD_DRV_LOG(DEBUG, "tx_size_64:		%"PRIu64"", ns->tx_size_64);
	PMD_DRV_LOG(DEBUG, "tx_size_127:	%"PRIu64"", ns->tx_size_127);
	PMD_DRV_LOG(DEBUG, "tx_size_255:	%"PRIu64"", ns->tx_size_255);
	PMD_DRV_LOG(DEBUG, "tx_size_511:	%"PRIu64"", ns->tx_size_511);
	PMD_DRV_LOG(DEBUG, "tx_size_1023:	%"PRIu64"", ns->tx_size_1023);
	PMD_DRV_LOG(DEBUG, "tx_size_1522:	%"PRIu64"", ns->tx_size_1522);
	PMD_DRV_LOG(DEBUG, "tx_size_big:	%"PRIu64"", ns->tx_size_big);
	PMD_DRV_LOG(DEBUG, "rx_len_errors:	%"PRIu64"", ns->rx_len_errors);
	PMD_DRV_LOG(DEBUG, "************* PF stats end ****************");
	return 0;
}

/* Reset the statistics */
static int
ice_stats_reset(struct rte_eth_dev *dev)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/* Mark PF and VSI stats to update the offset, aka "reset" */
	pf->offset_loaded = false;
	if (pf->main_vsi)
		pf->main_vsi->offset_loaded = false;

	/* read the stats, reading current register values into offset */
	ice_read_stats_registers(pf, hw);

	return 0;
}

static uint32_t
ice_xstats_calc_num(void)
{
	uint32_t num;

	num = ICE_NB_ETH_XSTATS + ICE_NB_HW_PORT_XSTATS;

	return num;
}

static int
ice_xstats_get(struct rte_eth_dev *dev, struct rte_eth_xstat *xstats,
	       unsigned int n)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	unsigned int i;
	unsigned int count;
	struct ice_hw_port_stats *hw_stats = &pf->stats;

	count = ice_xstats_calc_num();
	if (n < count)
		return count;

	ice_read_stats_registers(pf, hw);

	if (!xstats)
		return 0;

	count = 0;

	/* Get stats from ice_eth_stats struct */
	for (i = 0; i < ICE_NB_ETH_XSTATS; i++) {
		xstats[count].value =
			*(uint64_t *)((char *)&hw_stats->eth +
				      ice_stats_strings[i].offset);
		xstats[count].id = count;
		count++;
	}

	/* Get individiual stats from ice_hw_port struct */
	for (i = 0; i < ICE_NB_HW_PORT_XSTATS; i++) {
		xstats[count].value =
			*(uint64_t *)((char *)hw_stats +
				      ice_hw_port_strings[i].offset);
		xstats[count].id = count;
		count++;
	}

	return count;
}

static int ice_xstats_get_names(__rte_unused struct rte_eth_dev *dev,
				struct rte_eth_xstat_name *xstats_names,
				__rte_unused unsigned int limit)
{
	unsigned int count = 0;
	unsigned int i;

	if (!xstats_names)
		return ice_xstats_calc_num();

	/* Note: limit checked in rte_eth_xstats_names() */

	/* Get stats from ice_eth_stats struct */
	for (i = 0; i < ICE_NB_ETH_XSTATS; i++) {
		strlcpy(xstats_names[count].name, ice_stats_strings[i].name,
			sizeof(xstats_names[count].name));
		count++;
	}

	/* Get individiual stats from ice_hw_port struct */
	for (i = 0; i < ICE_NB_HW_PORT_XSTATS; i++) {
		strlcpy(xstats_names[count].name, ice_hw_port_strings[i].name,
			sizeof(xstats_names[count].name));
		count++;
	}

	return count;
}

static int
ice_dev_filter_ctrl(struct rte_eth_dev *dev,
		     enum rte_filter_type filter_type,
		     enum rte_filter_op filter_op,
		     void *arg)
{
	int ret = 0;

	if (!dev)
		return -EINVAL;

	switch (filter_type) {
	case RTE_ETH_FILTER_GENERIC:
		if (filter_op != RTE_ETH_FILTER_GET)
			return -EINVAL;
		*(const void **)arg = &ice_flow_ops;
		break;
	default:
		PMD_DRV_LOG(WARNING, "Filter type (%d) not supported",
					filter_type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

/* Add UDP tunneling port */
static int
ice_dev_udp_tunnel_port_add(struct rte_eth_dev *dev,
			     struct rte_eth_udp_tunnel *udp_tunnel)
{
	int ret = 0;
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (udp_tunnel == NULL)
		return -EINVAL;

	switch (udp_tunnel->prot_type) {
	case RTE_TUNNEL_TYPE_VXLAN:
		ret = ice_create_tunnel(hw, TNL_VXLAN, udp_tunnel->udp_port);
		break;
	default:
		PMD_DRV_LOG(ERR, "Invalid tunnel type");
		ret = -EINVAL;
		break;
	}

	return ret;
}

/* Delete UDP tunneling port */
static int
ice_dev_udp_tunnel_port_del(struct rte_eth_dev *dev,
			     struct rte_eth_udp_tunnel *udp_tunnel)
{
	int ret = 0;
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (udp_tunnel == NULL)
		return -EINVAL;

	switch (udp_tunnel->prot_type) {
	case RTE_TUNNEL_TYPE_VXLAN:
		ret = ice_destroy_tunnel(hw, udp_tunnel->udp_port, 0);
		break;
	default:
		PMD_DRV_LOG(ERR, "Invalid tunnel type");
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int
ice_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	      struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
					     sizeof(struct ice_adapter),
					     ice_dev_init);
}

static int
ice_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, ice_dev_uninit);
}

static struct rte_pci_driver rte_ice_pmd = {
	.id_table = pci_id_ice_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
	.probe = ice_pci_probe,
	.remove = ice_pci_remove,
};

/**
 * Driver initialization routine.
 * Invoked once at EAL init time.
 * Register itself as the [Poll Mode] Driver of PCI devices.
 */
RTE_PMD_REGISTER_PCI(net_ice, rte_ice_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_ice, pci_id_ice_map);
RTE_PMD_REGISTER_KMOD_DEP(net_ice, "* igb_uio | uio_pci_generic | vfio-pci");
RTE_PMD_REGISTER_PARAM_STRING(net_ice,
			      ICE_PROTO_XTR_ARG "=[queue:]<vlan|ipv4|ipv6|ipv6_flow|tcp|ip_offset>"
			      ICE_SAFE_MODE_SUPPORT_ARG "=<0|1>"
			      ICE_PIPELINE_MODE_SUPPORT_ARG "=<0|1>");

RTE_LOG_REGISTER(ice_logtype_init, pmd.net.ice.init, NOTICE);
RTE_LOG_REGISTER(ice_logtype_driver, pmd.net.ice.driver, NOTICE);
#ifdef RTE_LIBRTE_ICE_DEBUG_RX
RTE_LOG_REGISTER(ice_logtype_rx, pmd.net.ice.rx, DEBUG);
#endif
#ifdef RTE_LIBRTE_ICE_DEBUG_TX
RTE_LOG_REGISTER(ice_logtype_tx, pmd.net.ice.tx, DEBUG);
#endif
#ifdef RTE_LIBRTE_ICE_DEBUG_TX_FREE
RTE_LOG_REGISTER(ice_logtype_tx_free, pmd.net.ice.tx_free, DEBUG);
#endif
