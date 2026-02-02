/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016 6WIND S.A.
 * Copyright 2016 Mellanox Technologies, Ltd
 * Copyright 2026 DynaNIC Semiconductors, Ltd.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>

#include <sys/queue.h>

#include <rte_string_fns.h>
#include <rte_common.h>
#include <rte_ethdev.h>
#include <rte_byteorder.h>
#include <cmdline_parse.h>
#include <cmdline_parse_etheraddr.h>
#include <cmdline_parse_string.h>
#include <cmdline_parse_num.h>
#include <rte_flow.h>
#include <rte_hexdump.h>
#include <rte_vxlan.h>
#include <rte_gre.h>
#include <rte_mpls.h>
#include <rte_gtp.h>
#include <rte_geneve.h>
#include <rte_ip.h>
#include <rte_os_shim.h>

#include <rte_per_lcore.h>
#include <rte_log.h>

#include <eal_export.h>
#include "rte_flow_parser_private.h"

#ifndef RTE_PORT_ALL
#define RTE_PORT_ALL (~(uint16_t)0x0)
#endif

typedef uint16_t portid_t;
typedef uint16_t queueid_t;

#ifndef RSS_HASH_KEY_LENGTH
#define RSS_HASH_KEY_LENGTH 64
#endif

struct flow_parser_simple_parse_buf {
	uint8_t buf[4096];
};

static RTE_DEFINE_PER_LCORE(struct flow_parser_simple_parse_buf,
		flow_parser_simple_parse_buf);

/** Storage for struct rte_flow_action_raw_encap. */
struct action_raw_encap_data {
	struct rte_flow_action_raw_encap conf;
	uint8_t data[ACTION_RAW_ENCAP_MAX_DATA];
	uint8_t preserve[ACTION_RAW_ENCAP_MAX_DATA];
	uint16_t idx;
};

/** Storage for struct rte_flow_action_raw_decap including external data. */
struct action_raw_decap_data {
	struct rte_flow_action_raw_decap conf;
	uint8_t data[ACTION_RAW_ENCAP_MAX_DATA];
	uint16_t idx;
};

/** Storage for struct rte_flow_action_ipv6_ext_push including external data. */
struct action_ipv6_ext_push_data {
	struct rte_flow_action_ipv6_ext_push conf;
	uint8_t data[ACTION_IPV6_EXT_PUSH_MAX_DATA];
	uint8_t type;
	uint16_t idx;
};

/** Storage for struct rte_flow_action_ipv6_ext_remove including external data. */
struct action_ipv6_ext_remove_data {
	struct rte_flow_action_ipv6_ext_remove conf;
	uint8_t type;
	uint16_t idx;
};

const struct rte_flow_parser_l2_encap_conf
rte_flow_parser_default_l2_encap_conf = { 0 };

const struct rte_flow_parser_l2_decap_conf
rte_flow_parser_default_l2_decap_conf = { 0 };

const struct rte_flow_parser_mplsogre_encap_conf
rte_flow_parser_default_mplsogre_encap_conf = { 0 };

const struct rte_flow_parser_mplsogre_decap_conf
rte_flow_parser_default_mplsogre_decap_conf = { 0 };

const struct rte_flow_parser_mplsoudp_encap_conf
rte_flow_parser_default_mplsoudp_encap_conf = { 0 };

const struct rte_flow_parser_mplsoudp_decap_conf
rte_flow_parser_default_mplsoudp_decap_conf = { 0 };

const struct rte_flow_parser_vxlan_encap_conf
rte_flow_parser_default_vxlan_encap_conf = {
	.select_ipv4 = 1,
	.select_vlan = 0,
	.select_tos_ttl = 0,
	.vni = { 0x00, 0x00, 0x00 },
	.udp_src = 0,
	.udp_dst = RTE_BE16(RTE_VXLAN_DEFAULT_PORT),
	.ipv4_src = RTE_IPV4(127, 0, 0, 1),
	.ipv4_dst = RTE_IPV4(255, 255, 255, 255),
	.ipv6_src = RTE_IPV6_ADDR_LOOPBACK,
	.ipv6_dst = RTE_IPV6(0, 0, 0, 0, 0, 0, 0, 0x1111),
	.vlan_tci = 0,
	.ip_tos = 0,
	.ip_ttl = 255,
	.eth_src = { .addr_bytes = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	.eth_dst = { .addr_bytes = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff } },
};

/** Storage for struct rte_flow_action_vxlan_encap including external data. */
struct action_vxlan_encap_data {
	struct rte_flow_action_vxlan_encap conf;
	struct rte_flow_item items[ACTION_VXLAN_ENCAP_ITEMS_NUM];
	struct rte_flow_item_eth item_eth;
	struct rte_flow_item_vlan item_vlan;
	union {
		struct rte_flow_item_ipv4 item_ipv4;
		struct rte_flow_item_ipv6 item_ipv6;
	};
	struct rte_flow_item_udp item_udp;
	struct rte_flow_item_vxlan item_vxlan;
};

const struct rte_flow_parser_nvgre_encap_conf
rte_flow_parser_default_nvgre_encap_conf = {
	.select_ipv4 = 1,
	.select_vlan = 0,
	.tni = { 0x00, 0x00, 0x00 },
	.ipv4_src = RTE_IPV4(127, 0, 0, 1),
	.ipv4_dst = RTE_IPV4(255, 255, 255, 255),
	.ipv6_src = RTE_IPV6_ADDR_LOOPBACK,
	.ipv6_dst = RTE_IPV6(0, 0, 0, 0, 0, 0, 0, 0x1111),
	.vlan_tci = 0,
	.eth_src = { .addr_bytes = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	.eth_dst = { .addr_bytes = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff } },
};

/** Storage for struct rte_flow_action_nvgre_encap including external data. */
struct action_nvgre_encap_data {
	struct rte_flow_action_nvgre_encap conf;
	struct rte_flow_item items[ACTION_NVGRE_ENCAP_ITEMS_NUM];
	struct rte_flow_item_eth item_eth;
	struct rte_flow_item_vlan item_vlan;
	union {
		struct rte_flow_item_ipv4 item_ipv4;
		struct rte_flow_item_ipv6 item_ipv6;
	};
	struct rte_flow_item_nvgre item_nvgre;
};

struct raw_encap_conf {
	uint8_t data[ACTION_RAW_ENCAP_MAX_DATA];
	uint8_t preserve[ACTION_RAW_ENCAP_MAX_DATA];
	size_t size;
};

struct raw_decap_conf {
	uint8_t data[ACTION_RAW_ENCAP_MAX_DATA];
	size_t size;
};

struct ipv6_ext_push_conf {
	uint8_t data[ACTION_IPV6_EXT_PUSH_MAX_DATA];
	size_t size;
	uint8_t type;
};

struct ipv6_ext_remove_conf {
	uint8_t type;
};

struct raw_sample_conf {
	struct rte_flow_action data[ACTION_SAMPLE_ACTIONS_NUM];
};

struct action_rss_data {
	struct rte_flow_action_rss conf;
	uint8_t key[RSS_HASH_KEY_LENGTH];
	uint16_t queue[ACTION_RSS_QUEUE_NUM];
};

struct rte_flow_parser_ctx {
	struct rte_flow_parser_vxlan_encap_conf vxlan_encap_conf;
	struct rte_flow_parser_nvgre_encap_conf nvgre_encap_conf;
	struct rte_flow_parser_l2_encap_conf l2_encap_conf;
	struct rte_flow_parser_l2_decap_conf l2_decap_conf;
	struct rte_flow_parser_mplsogre_encap_conf mplsogre_encap_conf;
	struct rte_flow_parser_mplsogre_decap_conf mplsogre_decap_conf;
	struct rte_flow_parser_mplsoudp_encap_conf mplsoudp_encap_conf;
	struct rte_flow_parser_mplsoudp_decap_conf mplsoudp_decap_conf;
	struct rte_flow_action_conntrack conntrack_context;
	struct raw_encap_conf raw_encap_confs[RAW_ENCAP_CONFS_MAX_NUM];
	struct raw_decap_conf raw_decap_confs[RAW_ENCAP_CONFS_MAX_NUM];
	struct ipv6_ext_push_conf ipv6_ext_push_confs[IPV6_EXT_PUSH_CONFS_MAX_NUM];
	struct ipv6_ext_remove_conf ipv6_ext_remove_confs[IPV6_EXT_PUSH_CONFS_MAX_NUM];
	struct rte_flow_action_raw_encap raw_encap_conf_cache[RAW_ENCAP_CONFS_MAX_NUM];
	struct rte_flow_action_raw_decap raw_decap_conf_cache[RAW_ENCAP_CONFS_MAX_NUM];
	struct rte_flow_action_ipv6_ext_push
		ipv6_ext_push_action_cache[IPV6_EXT_PUSH_CONFS_MAX_NUM];
	struct rte_flow_action_ipv6_ext_remove
		ipv6_ext_remove_action_cache[IPV6_EXT_PUSH_CONFS_MAX_NUM];
	struct raw_sample_conf raw_sample_confs[RAW_SAMPLE_CONFS_MAX_NUM];
	struct rte_flow_action_mark sample_mark[RAW_SAMPLE_CONFS_MAX_NUM];
	struct rte_flow_action_queue sample_queue[RAW_SAMPLE_CONFS_MAX_NUM];
	struct rte_flow_action_count sample_count[RAW_SAMPLE_CONFS_MAX_NUM];
	struct rte_flow_action_port_id sample_port_id[RAW_SAMPLE_CONFS_MAX_NUM];
	struct rte_flow_action_raw_encap sample_encap[RAW_SAMPLE_CONFS_MAX_NUM];
	struct action_vxlan_encap_data sample_vxlan_encap[RAW_SAMPLE_CONFS_MAX_NUM];
	struct action_nvgre_encap_data sample_nvgre_encap[RAW_SAMPLE_CONFS_MAX_NUM];
	struct action_rss_data sample_rss_data[RAW_SAMPLE_CONFS_MAX_NUM];
	struct rte_flow_action_vf sample_vf[RAW_SAMPLE_CONFS_MAX_NUM];
	struct rte_flow_action_ethdev sample_port_representor[RAW_SAMPLE_CONFS_MAX_NUM];
	struct rte_flow_action_ethdev sample_represented_port[RAW_SAMPLE_CONFS_MAX_NUM];
};

struct rte_flow_parser {
	const struct rte_flow_parser_ops *ops;
	struct rte_flow_parser_ctx ctx;
	bool initialized;
};

static struct rte_flow_parser parser;

static void
parser_ctx_update_fields(uint8_t *buf, struct rte_flow_item *item,
			   uint16_t next_proto)
{
	struct rte_ipv4_hdr *ipv4;
	struct rte_ether_hdr *eth;
	struct rte_ipv6_hdr *ipv6;
	struct rte_vxlan_hdr *vxlan;
	struct rte_vxlan_gpe_hdr *gpe;
	struct rte_flow_item_nvgre *nvgre;
	uint32_t ipv6_vtc_flow;

	switch (item->type) {
	case RTE_FLOW_ITEM_TYPE_ETH:
		eth = (struct rte_ether_hdr *)buf;
		if (next_proto != 0)
			eth->ether_type = rte_cpu_to_be_16(next_proto);
		break;
	case RTE_FLOW_ITEM_TYPE_IPV4:
		ipv4 = (struct rte_ipv4_hdr *)buf;
		if (ipv4->version_ihl == 0)
			ipv4->version_ihl = RTE_IPV4_VHL_DEF;
		if (next_proto && ipv4->next_proto_id == 0)
			ipv4->next_proto_id = (uint8_t)next_proto;
		break;
	case RTE_FLOW_ITEM_TYPE_IPV6:
		ipv6 = (struct rte_ipv6_hdr *)buf;
		if (next_proto && ipv6->proto == 0)
			ipv6->proto = (uint8_t)next_proto;
		ipv6_vtc_flow = rte_be_to_cpu_32(ipv6->vtc_flow);
		ipv6_vtc_flow &= 0x0FFFFFFF;
		ipv6_vtc_flow |= 0x60000000;
		ipv6->vtc_flow = rte_cpu_to_be_32(ipv6_vtc_flow);
		break;
	case RTE_FLOW_ITEM_TYPE_VXLAN:
		vxlan = (struct rte_vxlan_hdr *)buf;
		if (vxlan->flags == 0)
			vxlan->flags = 0x08;
		break;
	case RTE_FLOW_ITEM_TYPE_VXLAN_GPE:
		gpe = (struct rte_vxlan_gpe_hdr *)buf;
		gpe->vx_flags = 0x0C;
		break;
	case RTE_FLOW_ITEM_TYPE_NVGRE:
		nvgre = (struct rte_flow_item_nvgre *)buf;
		nvgre->protocol = rte_cpu_to_be_16(0x6558);
		nvgre->c_k_s_rsvd0_ver = rte_cpu_to_be_16(0x2000);
		break;
	default:
		break;
	}
}

static const void *
parser_ctx_item_default_mask(const struct rte_flow_item *item)
{
	const void *mask = NULL;
	static rte_be32_t gre_key_default_mask = RTE_BE32(UINT32_MAX);
	static struct rte_flow_item_ipv6_routing_ext ipv6_routing_ext_default_mask = {
		.hdr = {
			.next_hdr = 0xff,
			.type = 0xff,
			.segments_left = 0xff,
		},
	};

	switch (item->type) {
	case RTE_FLOW_ITEM_TYPE_ANY:
		mask = &rte_flow_item_any_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_PORT_ID:
		mask = &rte_flow_item_port_id_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_RAW:
		mask = &rte_flow_item_raw_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_ETH:
		mask = &rte_flow_item_eth_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_VLAN:
		mask = &rte_flow_item_vlan_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_IPV4:
		mask = &rte_flow_item_ipv4_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_IPV6:
		mask = &rte_flow_item_ipv6_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_ICMP:
		mask = &rte_flow_item_icmp_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_UDP:
		mask = &rte_flow_item_udp_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_TCP:
		mask = &rte_flow_item_tcp_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_SCTP:
		mask = &rte_flow_item_sctp_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_VXLAN:
		mask = &rte_flow_item_vxlan_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_VXLAN_GPE:
		mask = &rte_flow_item_vxlan_gpe_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_E_TAG:
		mask = &rte_flow_item_e_tag_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_NVGRE:
		mask = &rte_flow_item_nvgre_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_MPLS:
		mask = &rte_flow_item_mpls_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_GRE:
		mask = &rte_flow_item_gre_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_GRE_KEY:
		mask = &gre_key_default_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_META:
		mask = &rte_flow_item_meta_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_RANDOM:
		mask = &rte_flow_item_random_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_FUZZY:
		mask = &rte_flow_item_fuzzy_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_GTP:
		mask = &rte_flow_item_gtp_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_GTP_PSC:
		mask = &rte_flow_item_gtp_psc_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_GENEVE:
		mask = &rte_flow_item_geneve_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_GENEVE_OPT:
		mask = &rte_flow_item_geneve_opt_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_PPPOE_PROTO_ID:
		mask = &rte_flow_item_pppoe_proto_id_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_L2TPV3OIP:
		mask = &rte_flow_item_l2tpv3oip_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_ESP:
		mask = &rte_flow_item_esp_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_AH:
		mask = &rte_flow_item_ah_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_PFCP:
		mask = &rte_flow_item_pfcp_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_PORT_REPRESENTOR:
	case RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT:
		mask = &rte_flow_item_ethdev_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_L2TPV2:
		mask = &rte_flow_item_l2tpv2_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_PPP:
		mask = &rte_flow_item_ppp_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_METER_COLOR:
		mask = &rte_flow_item_meter_color_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_IPV6_ROUTING_EXT:
		mask = &ipv6_routing_ext_default_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_AGGR_AFFINITY:
		mask = &rte_flow_item_aggr_affinity_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_TX_QUEUE:
		mask = &rte_flow_item_tx_queue_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_IB_BTH:
		mask = &rte_flow_item_ib_bth_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_PTYPE:
		mask = &rte_flow_item_ptype_mask;
		break;
	default:
		break;
	}
	return mask;
}

static int
parser_ctx_setup_vxlan_encap_data(struct action_vxlan_encap_data *data)
{
	struct rte_flow_parser_vxlan_encap_conf *conf;

	if (data == NULL)
		return -EINVAL;
	conf = &parser.ctx.vxlan_encap_conf;
	*data = (struct action_vxlan_encap_data){
		.conf = (struct rte_flow_action_vxlan_encap){
			.definition = data->items,
		},
		.items = {
			{
				.type = RTE_FLOW_ITEM_TYPE_ETH,
				.spec = &data->item_eth,
				.mask = &rte_flow_item_eth_mask,
			},
			{
				.type = RTE_FLOW_ITEM_TYPE_VLAN,
				.spec = &data->item_vlan,
				.mask = &rte_flow_item_vlan_mask,
			},
			{
				.type = RTE_FLOW_ITEM_TYPE_IPV4,
				.spec = &data->item_ipv4,
				.mask = &rte_flow_item_ipv4_mask,
			},
			{
				.type = RTE_FLOW_ITEM_TYPE_UDP,
				.spec = &data->item_udp,
				.mask = &rte_flow_item_udp_mask,
			},
			{
				.type = RTE_FLOW_ITEM_TYPE_VXLAN,
				.spec = &data->item_vxlan,
				.mask = &rte_flow_item_vxlan_mask,
			},
			{
				.type = RTE_FLOW_ITEM_TYPE_END,
			},
		},
		.item_eth.hdr.ether_type = 0,
		.item_vlan = {
			.hdr.vlan_tci = conf->vlan_tci,
			.hdr.eth_proto = 0,
		},
		.item_ipv4.hdr = {
			.src_addr = conf->ipv4_src,
			.dst_addr = conf->ipv4_dst,
		},
		.item_udp.hdr = {
			.src_port = conf->udp_src,
			.dst_port = conf->udp_dst,
		},
		.item_vxlan.hdr.flags = 0,
	};
	rte_ether_addr_copy(&conf->eth_dst, &data->item_eth.hdr.dst_addr);
	rte_ether_addr_copy(&conf->eth_src, &data->item_eth.hdr.src_addr);
	if (conf->select_ipv4 == 0) {
		memcpy(&data->item_ipv6.hdr.src_addr,
		       &conf->ipv6_src,
		       sizeof(conf->ipv6_src));
		memcpy(&data->item_ipv6.hdr.dst_addr,
		       &conf->ipv6_dst,
		       sizeof(conf->ipv6_dst));
		data->items[2] = (struct rte_flow_item){
			.type = RTE_FLOW_ITEM_TYPE_IPV6,
			.spec = &data->item_ipv6,
			.mask = &rte_flow_item_ipv6_mask,
		};
	}
	if (conf->select_vlan == 0)
		data->items[1].type = RTE_FLOW_ITEM_TYPE_VOID;
	if (conf->select_tos_ttl != 0) {
		if (conf->select_ipv4 != 0) {
			static struct rte_flow_item_ipv4 ipv4_mask_tos;

			memcpy(&ipv4_mask_tos, &rte_flow_item_ipv4_mask,
			       sizeof(ipv4_mask_tos));
			ipv4_mask_tos.hdr.type_of_service = 0xff;
			ipv4_mask_tos.hdr.time_to_live = 0xff;
			data->item_ipv4.hdr.type_of_service =
					conf->ip_tos;
			data->item_ipv4.hdr.time_to_live =
					conf->ip_ttl;
			data->items[2].mask = &ipv4_mask_tos;
		} else {
			static struct rte_flow_item_ipv6 ipv6_mask_tos;

			memcpy(&ipv6_mask_tos, &rte_flow_item_ipv6_mask,
			       sizeof(ipv6_mask_tos));
			ipv6_mask_tos.hdr.vtc_flow |=
				RTE_BE32(0xfful << RTE_IPV6_HDR_TC_SHIFT);
			ipv6_mask_tos.hdr.hop_limits = 0xff;
			data->item_ipv6.hdr.vtc_flow |=
				rte_cpu_to_be_32((uint32_t)conf->ip_tos <<
					RTE_IPV6_HDR_TC_SHIFT);
			data->item_ipv6.hdr.hop_limits =
					conf->ip_ttl;
			data->items[2].mask = &ipv6_mask_tos;
		}
	}
	memcpy(data->item_vxlan.hdr.vni, conf->vni,
	       RTE_DIM(conf->vni));
	return 0;
}

static int
parser_ctx_setup_nvgre_encap_data(struct action_nvgre_encap_data *data)
{
	struct rte_flow_parser_nvgre_encap_conf *conf;

	if (data == NULL)
		return -EINVAL;
	conf = &parser.ctx.nvgre_encap_conf;
	memset(data, 0, sizeof(*data));
	data->conf.definition = data->items;
	data->items[0] = (struct rte_flow_item){
		.type = RTE_FLOW_ITEM_TYPE_ETH,
		.spec = &data->item_eth,
		.mask = &rte_flow_item_eth_mask,
	};
	data->items[1] = (struct rte_flow_item){
		.type = RTE_FLOW_ITEM_TYPE_VLAN,
		.spec = &data->item_vlan,
		.mask = &rte_flow_item_vlan_mask,
	};
	data->items[2] = (struct rte_flow_item){
		.type = RTE_FLOW_ITEM_TYPE_IPV4,
		.spec = &data->item_ipv4,
		.mask = &rte_flow_item_ipv4_mask,
	};
	data->items[3] = (struct rte_flow_item){
		.type = RTE_FLOW_ITEM_TYPE_NVGRE,
		.spec = &data->item_nvgre,
		.mask = &rte_flow_item_nvgre_mask,
	};
	data->items[4] = (struct rte_flow_item){
		.type = RTE_FLOW_ITEM_TYPE_END,
	};
	data->item_eth.hdr.ether_type = 0;
	data->item_vlan.hdr.vlan_tci = conf->vlan_tci;
	data->item_vlan.hdr.eth_proto = 0;
	data->item_ipv4.hdr.src_addr = conf->ipv4_src;
	data->item_ipv4.hdr.dst_addr = conf->ipv4_dst;
	memset(&data->item_nvgre, 0, sizeof(data->item_nvgre));
	rte_ether_addr_copy(&conf->eth_dst, &data->item_eth.hdr.dst_addr);
	rte_ether_addr_copy(&conf->eth_src, &data->item_eth.hdr.src_addr);
	if (conf->select_ipv4 == 0) {
		memcpy(&data->item_ipv6.hdr.src_addr,
		       &conf->ipv6_src,
		       sizeof(conf->ipv6_src));
		memcpy(&data->item_ipv6.hdr.dst_addr,
		       &conf->ipv6_dst,
		       sizeof(conf->ipv6_dst));
		data->items[2] = (struct rte_flow_item){
			.type = RTE_FLOW_ITEM_TYPE_IPV6,
			.spec = &data->item_ipv6,
			.mask = &rte_flow_item_ipv6_mask,
		};
	}
	if (conf->select_vlan == 0)
		data->items[1].type = RTE_FLOW_ITEM_TYPE_VOID;
	memcpy(data->item_nvgre.tni, conf->tni, RTE_DIM(conf->tni));
	return 0;
}

static inline int
parser_ctx_raw_append(uint8_t *data_tail, size_t *total_size,
		      const void *src, size_t len)
{
	if (len == 0)
		return 0;
	if (len > ACTION_RAW_ENCAP_MAX_DATA - *total_size)
		return -EINVAL;
	*total_size += len;
	memcpy(data_tail - (*total_size), src, len);
	return 0;
}

static int
parser_ctx_set_raw_common(bool encap, uint16_t idx,
			  const struct rte_flow_item pattern[],
			  uint32_t pattern_n)
{
	uint32_t n = pattern_n;
	int i = 0;
	struct rte_flow_item *item = NULL;
	size_t size = 0;
	uint8_t *data = NULL;
	uint8_t *data_tail = NULL;
	size_t *total_size = NULL;
	uint16_t upper_layer = 0;
	uint16_t proto = 0;
	int gtp_psc = -1;
	const void *src_spec;
	struct rte_flow_item *items = (struct rte_flow_item *)(uintptr_t)pattern;

	if (idx >= RAW_ENCAP_CONFS_MAX_NUM)
		return -EINVAL;
	if (encap != 0) {
		total_size = &parser.ctx.raw_encap_confs[idx].size;
		data = (uint8_t *)&parser.ctx.raw_encap_confs[idx].data;
	} else {
		total_size = &parser.ctx.raw_decap_confs[idx].size;
		data = (uint8_t *)&parser.ctx.raw_decap_confs[idx].data;
	}
	*total_size = 0;
	memset(data, 0x00, ACTION_RAW_ENCAP_MAX_DATA);
	data_tail = data + ACTION_RAW_ENCAP_MAX_DATA;
	for (i = n - 1; i >= 0; --i) {
		const struct rte_flow_item_gtp *gtp;
		const struct rte_flow_item_geneve_opt *geneve_opt;
		struct rte_flow_item_ipv6_routing_ext *ext;

		item = items + i;
		if (item->spec == NULL)
			item->spec = parser_ctx_item_default_mask(item);
		src_spec = item->spec;
		switch (item->type) {
		case RTE_FLOW_ITEM_TYPE_ETH:
			size = sizeof(struct rte_ether_hdr);
			break;
		case RTE_FLOW_ITEM_TYPE_VLAN:
			size = sizeof(struct rte_vlan_hdr);
			proto = RTE_ETHER_TYPE_VLAN;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			size = sizeof(struct rte_ipv4_hdr);
			proto = RTE_ETHER_TYPE_IPV4;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			size = sizeof(struct rte_ipv6_hdr);
			proto = RTE_ETHER_TYPE_IPV6;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6_ROUTING_EXT:
			ext = (struct rte_flow_item_ipv6_routing_ext *)(uintptr_t)item->spec;
			if (ext->hdr.hdr_len == 0) {
				size = sizeof(struct rte_ipv6_routing_ext) +
					(ext->hdr.segments_left << 4);
				ext->hdr.hdr_len = ext->hdr.segments_left << 1;
				if (ext->hdr.type == RTE_IPV6_SRCRT_TYPE_4)
					ext->hdr.last_entry =
						ext->hdr.segments_left - 1;
			} else {
				size = sizeof(struct rte_ipv6_routing_ext) +
					(ext->hdr.hdr_len << 3);
			}
			proto = IPPROTO_ROUTING;
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			size = sizeof(struct rte_udp_hdr);
			proto = 0x11;
			break;
		case RTE_FLOW_ITEM_TYPE_TCP:
			size = sizeof(struct rte_tcp_hdr);
			proto = 0x06;
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN:
			size = sizeof(struct rte_vxlan_hdr);
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN_GPE:
			size = sizeof(struct rte_vxlan_gpe_hdr);
			break;
		case RTE_FLOW_ITEM_TYPE_GRE:
			size = sizeof(struct rte_gre_hdr);
			proto = 0x2F;
			break;
		case RTE_FLOW_ITEM_TYPE_GRE_KEY:
			size = sizeof(rte_be32_t);
			proto = 0x0;
			break;
		case RTE_FLOW_ITEM_TYPE_MPLS:
			size = sizeof(struct rte_mpls_hdr);
			proto = 0x0;
			break;
		case RTE_FLOW_ITEM_TYPE_NVGRE:
			size = sizeof(struct rte_flow_item_nvgre);
			proto = 0x2F;
			break;
		case RTE_FLOW_ITEM_TYPE_GENEVE:
			size = sizeof(struct rte_geneve_hdr);
			break;
		case RTE_FLOW_ITEM_TYPE_GENEVE_OPT:
			geneve_opt = (const struct rte_flow_item_geneve_opt *)item->spec;
			size = offsetof(struct rte_flow_item_geneve_opt, option_len) +
				sizeof(uint8_t);
			if (geneve_opt->option_len != 0 && geneve_opt->data != NULL) {
				if (parser_ctx_raw_append(data_tail, total_size,
							  geneve_opt->data,
							  geneve_opt->option_len * sizeof(uint32_t)) != 0)
					goto error;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_L2TPV3OIP:
			size = sizeof(rte_be32_t);
			proto = 0x73;
			break;
		case RTE_FLOW_ITEM_TYPE_ESP:
			size = sizeof(struct rte_esp_hdr);
			proto = 0x32;
			break;
		case RTE_FLOW_ITEM_TYPE_AH:
			size = sizeof(struct rte_flow_item_ah);
			proto = 0x33;
			break;
		case RTE_FLOW_ITEM_TYPE_GTP:
			if (gtp_psc < 0) {
				size = sizeof(struct rte_gtp_hdr);
				break;
			}
			if (gtp_psc != i + 1)
				goto error;
			gtp = item->spec;
			if (gtp->hdr.s == 1 || gtp->hdr.pn == 1)
				goto error;
			else {
				struct rte_gtp_hdr_ext_word ext_word = {
					.next_ext = 0x85
				};
				if (parser_ctx_raw_append(data_tail, total_size,
							  &ext_word,
							  sizeof(ext_word)) != 0)
					goto error;
			}
			size = sizeof(struct rte_gtp_hdr);
			break;
		case RTE_FLOW_ITEM_TYPE_GTP_PSC:
			if (gtp_psc >= 0)
				goto error;
			else {
				const struct rte_flow_item_gtp_psc *gtp_opt = item->spec;
				struct rte_gtp_psc_generic_hdr *hdr;
				size_t hdr_size = RTE_ALIGN(sizeof(*hdr),
						 sizeof(int32_t));

				if (hdr_size > ACTION_RAW_ENCAP_MAX_DATA - *total_size)
					goto error;
				*total_size += hdr_size;
				hdr = (typeof(hdr))(data_tail - (*total_size));
				memset(hdr, 0, hdr_size);
				if (gtp_opt != NULL)
					*hdr = gtp_opt->hdr;
				hdr->ext_hdr_len = 1;
				gtp_psc = i;
				size = 0;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_PFCP:
			size = sizeof(struct rte_flow_item_pfcp);
			break;
		case RTE_FLOW_ITEM_TYPE_FLEX:
			if (item->spec != NULL) {
				size = ((const struct rte_flow_item_flex *)item->spec)->length;
				src_spec = ((const struct rte_flow_item_flex *)item->spec)->pattern;
			} else {
				size = 0;
				src_spec = NULL;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_GRE_OPTION:
			size = 0;
			if (item->spec != NULL) {
				const struct rte_flow_item_gre_opt *gre_opt = item->spec;
				if (gre_opt->checksum_rsvd.checksum != 0) {
					if (parser_ctx_raw_append(data_tail, total_size,
								  &gre_opt->checksum_rsvd,
								  sizeof(gre_opt->checksum_rsvd)) != 0)
						goto error;
				}
				if (gre_opt->key.key != 0) {
					if (parser_ctx_raw_append(data_tail, total_size,
								  &gre_opt->key.key,
								  sizeof(gre_opt->key.key)) != 0)
						goto error;
				}
				if (gre_opt->sequence.sequence != 0) {
					if (parser_ctx_raw_append(data_tail, total_size,
								  &gre_opt->sequence.sequence,
								  sizeof(gre_opt->sequence.sequence)) != 0)
						goto error;
				}
			}
			proto = 0x2F;
			break;
		default:
			goto error;
		}
		if (size != 0) {
			if (src_spec == NULL)
				goto error;
			if (parser_ctx_raw_append(data_tail, total_size,
						  src_spec, size) != 0)
				goto error;
			parser_ctx_update_fields((data_tail - (*total_size)), item,
					      upper_layer);
			upper_layer = proto;
		}
	}
	RTE_ASSERT((*total_size) <= ACTION_RAW_ENCAP_MAX_DATA);
	memmove(data, (data_tail - (*total_size)), *total_size);
	return 0;

error:
	*total_size = 0;
	memset(data, 0x00, ACTION_RAW_ENCAP_MAX_DATA);
	return -EINVAL;
}

const struct rte_flow_action_raw_encap *
rte_flow_parser_raw_encap_conf_get(uint16_t index)
{
	if (index >= RAW_ENCAP_CONFS_MAX_NUM)
		return NULL;
	parser.ctx.raw_encap_conf_cache[index] = (struct rte_flow_action_raw_encap){
		.data = parser.ctx.raw_encap_confs[index].data,
		.size = parser.ctx.raw_encap_confs[index].size,
		.preserve = parser.ctx.raw_encap_confs[index].preserve,
	};
	return &parser.ctx.raw_encap_conf_cache[index];
}

const struct rte_flow_action_raw_decap *
rte_flow_parser_raw_decap_conf_get(uint16_t index)
{
	if (index >= RAW_ENCAP_CONFS_MAX_NUM)
		return NULL;
	parser.ctx.raw_decap_conf_cache[index] = (struct rte_flow_action_raw_decap){
		.data = parser.ctx.raw_decap_confs[index].data,
		.size = parser.ctx.raw_decap_confs[index].size,
	};
	return &parser.ctx.raw_decap_conf_cache[index];
}

static int
parser_ctx_set_ipv6_ext_push(uint16_t idx,
			     const struct rte_flow_item pattern[],
			     uint32_t pattern_n)
{
	uint32_t i = 0;
	const struct rte_flow_item *item;
	size_t size = 0;
	uint8_t *data;
	uint8_t *type;
	size_t *total_size;

	if (idx >= IPV6_EXT_PUSH_CONFS_MAX_NUM)
		return -EINVAL;
	data = (uint8_t *)&parser.ctx.ipv6_ext_push_confs[idx].data;
	type = (uint8_t *)&parser.ctx.ipv6_ext_push_confs[idx].type;
	total_size = &parser.ctx.ipv6_ext_push_confs[idx].size;
	*total_size = 0;
	memset(data, 0x00, ACTION_IPV6_EXT_PUSH_MAX_DATA);
	for (i = pattern_n; i > 0; --i) {
		item = &pattern[i - 1];
		switch (item->type) {
		case RTE_FLOW_ITEM_TYPE_IPV6_EXT:
			if (item->spec == NULL)
				goto error;
			*type = ((const struct rte_flow_item_ipv6_ext *)item->spec)->next_hdr;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6_ROUTING_EXT:
		{
			struct rte_flow_item_ipv6_routing_ext ext;
			const struct rte_flow_item_ipv6_routing_ext *spec = item->spec;

			if (spec == NULL)
				goto error;
			memcpy(&ext, spec, sizeof(ext));
			if (ext.hdr.hdr_len == 0) {
				size = sizeof(struct rte_ipv6_routing_ext) +
					(ext.hdr.segments_left << 4);
				ext.hdr.hdr_len = ext.hdr.segments_left << 1;
				if (ext.hdr.type == 4)
					ext.hdr.last_entry =
						ext.hdr.segments_left - 1;
			} else {
				size = sizeof(struct rte_ipv6_routing_ext) +
					(ext.hdr.hdr_len << 3);
			}
			*total_size += size;
			memcpy(data, &ext, size);
			break;
		}
		default:
			goto error;
		}
	}
	RTE_ASSERT((*total_size) <= ACTION_IPV6_EXT_PUSH_MAX_DATA);
	return 0;
error:
	*total_size = 0;
	memset(data, 0x00, ACTION_IPV6_EXT_PUSH_MAX_DATA);
	return -EINVAL;
}

static int
parser_ctx_set_ipv6_ext_remove(uint16_t idx,
			       const struct rte_flow_item pattern[],
			       uint32_t pattern_n)
{
	const struct rte_flow_item_ipv6_ext *ipv6_ext;

	if (pattern_n != 1 ||
	    pattern[0].type != RTE_FLOW_ITEM_TYPE_IPV6_EXT ||
	    pattern[0].spec == NULL || idx >= IPV6_EXT_PUSH_CONFS_MAX_NUM)
		return -EINVAL;
	ipv6_ext = pattern[0].spec;
	parser.ctx.ipv6_ext_remove_confs[idx].type = ipv6_ext->next_hdr;
	return 0;
}

static const struct rte_flow_action_ipv6_ext_push *
parser_ctx_ipv6_ext_push_conf_get(uint16_t index)
{
	if (index >= IPV6_EXT_PUSH_CONFS_MAX_NUM)
		return NULL;
	parser.ctx.ipv6_ext_push_action_cache[index] =
		(struct rte_flow_action_ipv6_ext_push){
			.data = parser.ctx.ipv6_ext_push_confs[index].data,
			.size = parser.ctx.ipv6_ext_push_confs[index].size,
			.type = parser.ctx.ipv6_ext_push_confs[index].type,
		};
	return &parser.ctx.ipv6_ext_push_action_cache[index];
}

static const struct rte_flow_action_ipv6_ext_remove *
parser_ctx_ipv6_ext_remove_conf_get(uint16_t index)
{
	if (index >= IPV6_EXT_PUSH_CONFS_MAX_NUM)
		return NULL;
	parser.ctx.ipv6_ext_remove_action_cache[index] =
		(struct rte_flow_action_ipv6_ext_remove){
			.type = parser.ctx.ipv6_ext_remove_confs[index].type,
		};
	return &parser.ctx.ipv6_ext_remove_action_cache[index];
}

static int
parser_ctx_set_sample_actions(uint16_t idx,
			      const struct rte_flow_action actions[],
			      uint32_t actions_n)
{
	uint32_t i;
	struct rte_flow_action *data;
	size_t size;
	uint32_t max_size = sizeof(struct rte_flow_action) *
		ACTION_SAMPLE_ACTIONS_NUM;

	if (idx >= RAW_SAMPLE_CONFS_MAX_NUM)
		return -EINVAL;
	if (actions_n > ACTION_SAMPLE_ACTIONS_NUM)
		return -EINVAL;
	data = (struct rte_flow_action *)&parser.ctx.raw_sample_confs[idx].data;
	memset(data, 0x00, max_size);
	for (i = 0; i < actions_n; i++) {
		struct rte_flow_action action = actions[i];
		const struct rte_flow_action_rss *rss;

		if (action.type == RTE_FLOW_ACTION_TYPE_END)
			break;
		switch (action.type) {
		case RTE_FLOW_ACTION_TYPE_MARK:
			size = sizeof(struct rte_flow_action_mark);
			memcpy(&parser.ctx.sample_mark[idx], action.conf, size);
			action.conf = &parser.ctx.sample_mark[idx];
			break;
		case RTE_FLOW_ACTION_TYPE_COUNT:
			size = sizeof(struct rte_flow_action_count);
			memcpy(&parser.ctx.sample_count[idx], action.conf, size);
			action.conf = &parser.ctx.sample_count[idx];
			break;
		case RTE_FLOW_ACTION_TYPE_QUEUE:
			size = sizeof(struct rte_flow_action_queue);
			memcpy(&parser.ctx.sample_queue[idx], action.conf, size);
			action.conf = &parser.ctx.sample_queue[idx];
			break;
		case RTE_FLOW_ACTION_TYPE_RSS:
			size = sizeof(struct rte_flow_action_rss);
			rss = action.conf;
			if (rss == NULL)
				return -EINVAL;
			memcpy(&parser.ctx.sample_rss_data[idx].conf, rss, size);
			if (rss->key_len != 0 && rss->key != NULL) {
				parser.ctx.sample_rss_data[idx].conf.key =
					parser.ctx.sample_rss_data[idx].key;
				memcpy((void *)(uintptr_t)
				       parser.ctx.sample_rss_data[idx].conf.key,
				       rss->key,
				       sizeof(uint8_t) * rss->key_len);
			}
			if (rss->queue_num != 0 && rss->queue != NULL) {
				parser.ctx.sample_rss_data[idx].conf.queue =
					parser.ctx.sample_rss_data[idx].queue;
				memcpy((void *)(uintptr_t)
				       parser.ctx.sample_rss_data[idx].conf.queue,
				       rss->queue,
				       sizeof(uint16_t) * rss->queue_num);
			}
			action.conf = &parser.ctx.sample_rss_data[idx].conf;
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_ENCAP:
		{
			const struct rte_flow_action_raw_encap *raw = action.conf;

			if (raw == NULL)
				return -EINVAL;
			memcpy(&parser.ctx.sample_encap[idx], raw, sizeof(*raw));
			action.conf = &parser.ctx.sample_encap[idx];
			break;
		}
		case RTE_FLOW_ACTION_TYPE_PORT_ID:
			size = sizeof(struct rte_flow_action_port_id);
			memcpy(&parser.ctx.sample_port_id[idx], action.conf, size);
			action.conf = &parser.ctx.sample_port_id[idx];
			break;
		case RTE_FLOW_ACTION_TYPE_PF:
			break;
		case RTE_FLOW_ACTION_TYPE_VF:
			size = sizeof(struct rte_flow_action_vf);
			memcpy(&parser.ctx.sample_vf[idx], action.conf, size);
			action.conf = &parser.ctx.sample_vf[idx];
			break;
		case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP:
			size = sizeof(struct rte_flow_action_vxlan_encap);
			parser_ctx_setup_vxlan_encap_data(&parser.ctx.sample_vxlan_encap[idx]);
			action.conf = &parser.ctx.sample_vxlan_encap[idx].conf;
			break;
		case RTE_FLOW_ACTION_TYPE_NVGRE_ENCAP:
			size = sizeof(struct rte_flow_action_nvgre_encap);
			parser_ctx_setup_nvgre_encap_data(&parser.ctx.sample_nvgre_encap[idx]);
			action.conf = &parser.ctx.sample_nvgre_encap[idx];
			break;
		case RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR:
			size = sizeof(struct rte_flow_action_ethdev);
			memcpy(&parser.ctx.sample_port_representor[idx],
			       action.conf, size);
			action.conf = &parser.ctx.sample_port_representor[idx];
			break;
		case RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT:
			size = sizeof(struct rte_flow_action_ethdev);
			memcpy(&parser.ctx.sample_represented_port[idx],
			       action.conf, size);
			action.conf = &parser.ctx.sample_represented_port[idx];
			break;
		default:
			return -EINVAL;
		}
		*data = action;
		data++;
	}
	return 0;
}
void
rte_flow_parser_reset_defaults(void)
{
	memset(&parser.ctx, 0, sizeof(parser.ctx));
	parser.ctx.vxlan_encap_conf = rte_flow_parser_default_vxlan_encap_conf;
	parser.ctx.nvgre_encap_conf = rte_flow_parser_default_nvgre_encap_conf;
	parser.ctx.l2_encap_conf = rte_flow_parser_default_l2_encap_conf;
	parser.ctx.l2_decap_conf = rte_flow_parser_default_l2_decap_conf;
	parser.ctx.mplsogre_encap_conf = rte_flow_parser_default_mplsogre_encap_conf;
	parser.ctx.mplsogre_decap_conf = rte_flow_parser_default_mplsogre_decap_conf;
	parser.ctx.mplsoudp_encap_conf = rte_flow_parser_default_mplsoudp_encap_conf;
	parser.ctx.mplsoudp_decap_conf = rte_flow_parser_default_mplsoudp_decap_conf;
}

int
rte_flow_parser_init(const struct rte_flow_parser_ops *ops)
{
	if (parser.initialized)
		return -EALREADY;
	parser.ops = ops;
	rte_flow_parser_reset_defaults();
	parser.initialized = true;
	return 0;
}

struct rte_flow_parser_vxlan_encap_conf *
rte_flow_parser_vxlan_encap_conf(void)
{
	return &parser.ctx.vxlan_encap_conf;
}

struct rte_flow_parser_nvgre_encap_conf *
rte_flow_parser_nvgre_encap_conf(void)
{
	return &parser.ctx.nvgre_encap_conf;
}

struct rte_flow_parser_l2_encap_conf *
rte_flow_parser_l2_encap_conf(void)
{
	return &parser.ctx.l2_encap_conf;
}

struct rte_flow_parser_l2_decap_conf *
rte_flow_parser_l2_decap_conf(void)
{
	return &parser.ctx.l2_decap_conf;
}

struct rte_flow_parser_mplsogre_encap_conf *
rte_flow_parser_mplsogre_encap_conf(void)
{
	return &parser.ctx.mplsogre_encap_conf;
}

struct rte_flow_parser_mplsogre_decap_conf *
rte_flow_parser_mplsogre_decap_conf(void)
{
	return &parser.ctx.mplsogre_decap_conf;
}

struct rte_flow_parser_mplsoudp_encap_conf *
rte_flow_parser_mplsoudp_encap_conf(void)
{
	return &parser.ctx.mplsoudp_encap_conf;
}

struct rte_flow_parser_mplsoudp_decap_conf *
rte_flow_parser_mplsoudp_decap_conf(void)
{
	return &parser.ctx.mplsoudp_decap_conf;
}

struct rte_flow_action_conntrack *
rte_flow_parser_conntrack_context(void)
{
	return &parser.ctx.conntrack_context;
}

static inline const struct rte_flow_parser_ops_query *
parser_query_ops(void)
{
	return parser.ops ? parser.ops->query : NULL;
}

static inline const struct rte_flow_parser_ops_command *
parser_command_ops(void)
{
	return parser.ops ? parser.ops->command : NULL;
}

static inline int
parser_port_id_is_invalid(uint16_t port_id)
{
	const struct rte_flow_parser_ops_query *ops = parser_query_ops();

	if (ops == NULL || ops->port_validate == NULL)
		return 0;
	return ops->port_validate(port_id);
}

static inline uint16_t
parser_flow_rule_count(uint16_t port_id)
{
	const struct rte_flow_parser_ops_query *ops = parser_query_ops();

	if (ops == NULL || ops->flow_rule_count == NULL)
		return 0;
	return ops->flow_rule_count(port_id);
}

static inline int
parser_flow_rule_id_get(uint16_t port_id, unsigned int index, uint64_t *rule_id)
{
	const struct rte_flow_parser_ops_query *ops = parser_query_ops();

	if (ops == NULL || ops->flow_rule_id_get == NULL)
		return -1;
	return ops->flow_rule_id_get(port_id, index, rule_id);
}

static inline uint16_t
parser_pattern_template_count(uint16_t port_id)
{
	const struct rte_flow_parser_ops_query *ops = parser_query_ops();

	if (ops == NULL || ops->pattern_template_count == NULL)
		return 0;
	return ops->pattern_template_count(port_id);
}

static inline int
parser_pattern_template_id_get(uint16_t port_id, unsigned int index,
			       uint32_t *template_id)
{
	const struct rte_flow_parser_ops_query *ops = parser_query_ops();

	if (ops == NULL || ops->pattern_template_id_get == NULL)
		return -1;
	return ops->pattern_template_id_get(port_id, index, template_id);
}

static inline uint16_t
parser_actions_template_count(uint16_t port_id)
{
	const struct rte_flow_parser_ops_query *ops = parser_query_ops();

	if (ops == NULL || ops->actions_template_count == NULL)
		return 0;
	return ops->actions_template_count(port_id);
}

static inline int
parser_actions_template_id_get(uint16_t port_id, unsigned int index,
			       uint32_t *template_id)
{
	const struct rte_flow_parser_ops_query *ops = parser_query_ops();

	if (ops == NULL || ops->actions_template_id_get == NULL)
		return -1;
	return ops->actions_template_id_get(port_id, index, template_id);
}

static inline uint16_t
parser_table_count(uint16_t port_id)
{
	const struct rte_flow_parser_ops_query *ops = parser_query_ops();

	if (ops == NULL || ops->table_count == NULL)
		return 0;
	return ops->table_count(port_id);
}

static inline int
parser_table_id_get(uint16_t port_id, unsigned int index, uint32_t *table_id)
{
	const struct rte_flow_parser_ops_query *ops = parser_query_ops();

	if (ops == NULL || ops->table_id_get == NULL)
		return -1;
	return ops->table_id_get(port_id, index, table_id);
}

static inline uint16_t
parser_queue_count(uint16_t port_id)
{
	const struct rte_flow_parser_ops_query *ops = parser_query_ops();

	if (ops == NULL || ops->queue_count == NULL)
		return 0;
	return ops->queue_count(port_id);
}

static inline uint16_t
parser_rss_queue_count(uint16_t port_id)
{
	const struct rte_flow_parser_ops_query *ops = parser_query_ops();

	if (ops == NULL || ops->rss_queue_count == NULL)
		return 0;
	return ops->rss_queue_count(port_id);
}

static inline struct rte_flow_template_table *
parser_table_get(uint16_t port_id, uint32_t table_id)
{
	const struct rte_flow_parser_ops_query *ops = parser_query_ops();

	if (ops == NULL || ops->table_get == NULL)
		return NULL;
	return ops->table_get(port_id, table_id);
}

static inline struct rte_flow_action_handle *
parser_action_handle_get(uint16_t port_id, uint32_t action_id)
{
	const struct rte_flow_parser_ops_query *ops = parser_query_ops();

	if (ops == NULL || ops->action_handle_get == NULL)
		return NULL;
	return ops->action_handle_get(port_id, action_id);
}

static inline struct rte_flow_meter_profile *
parser_meter_profile_get(uint16_t port_id, uint32_t profile_id)
{
	const struct rte_flow_parser_ops_query *ops = parser_query_ops();

	if (ops == NULL || ops->meter_profile_get == NULL)
		return NULL;
	return ops->meter_profile_get(port_id, profile_id);
}

static inline struct rte_flow_meter_policy *
parser_meter_policy_get(uint16_t port_id, uint32_t policy_id)
{
	const struct rte_flow_parser_ops_query *ops = parser_query_ops();

	if (ops == NULL || ops->meter_policy_get == NULL)
		return NULL;
	return ops->meter_policy_get(port_id, policy_id);
}

static inline struct rte_flow_item_flex_handle *
parser_flex_handle_get(uint16_t port_id, uint16_t flex_id)
{
	const struct rte_flow_parser_ops_query *ops = parser_query_ops();

	if (ops == NULL || ops->flex_handle_get == NULL)
		return NULL;
	return ops->flex_handle_get(port_id, flex_id);
}

static inline int
parser_flex_pattern_get(uint16_t pattern_id,
			const struct rte_flow_item_flex **spec,
			const struct rte_flow_item_flex **mask)
{
	const struct rte_flow_parser_ops_query *ops = parser_query_ops();

	if (ops == NULL || ops->flex_pattern_get == NULL)
		return -1;
	return ops->flex_pattern_get(pattern_id, spec, mask);
}

static inline int
parser_command_flow_get_info(portid_t port_id)
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->flow_get_info != NULL)
		return ops->flow_get_info(port_id);
	return -ENOTSUP;
}

static inline int
parser_command_flow_configure(portid_t port_id,
			      const struct rte_flow_port_attr *port_attr,
			      uint16_t nb_queue,
			      const struct rte_flow_queue_attr *queue_attr)
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->flow_configure != NULL)
		return ops->flow_configure(port_id, port_attr, nb_queue, queue_attr);
	return -ENOTSUP;
}

static inline int
parser_command_flow_pattern_template_create(portid_t port_id, uint32_t id,
					    const struct rte_flow_pattern_template_attr *attr,
					    const struct rte_flow_item pattern[])
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->flow_pattern_template_create != NULL)
		return ops->flow_pattern_template_create(port_id, id, attr, pattern);
	return -ENOTSUP;
}

static inline int
parser_command_flow_pattern_template_destroy(portid_t port_id,
					     uint32_t nb_id,
					     const uint32_t id[])
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->flow_pattern_template_destroy != NULL)
		return ops->flow_pattern_template_destroy(port_id, nb_id, id);
	return -ENOTSUP;
}

static inline int
parser_command_flow_actions_template_create(portid_t port_id, uint32_t id,
					    const struct rte_flow_actions_template_attr *attr,
					    const struct rte_flow_action actions[],
					    const struct rte_flow_action masks[])
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->flow_actions_template_create != NULL)
		return ops->flow_actions_template_create(port_id, id, attr, actions,
						  masks);
	return -ENOTSUP;
}

static inline int
parser_command_flow_actions_template_destroy(portid_t port_id,
					     uint32_t nb_id,
					     const uint32_t id[])
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->flow_actions_template_destroy != NULL)
		return ops->flow_actions_template_destroy(port_id, nb_id, id);
	return -ENOTSUP;
}

static inline int
parser_command_flow_template_table_create(portid_t port_id, uint32_t table_id,
					  const struct rte_flow_template_table_attr *attr,
					  uint32_t nb_pattern,
					  uint32_t pattern_id[],
					  uint32_t nb_action,
					  uint32_t action_id[])
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->flow_template_table_create != NULL)
		return ops->flow_template_table_create(port_id, table_id, attr,
							nb_pattern, pattern_id,
							nb_action, action_id);
	return -ENOTSUP;
}

static inline int
parser_command_flow_template_table_destroy(portid_t port_id,
					   uint32_t nb_id,
					   const uint32_t id[])
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->flow_template_table_destroy != NULL)
		return ops->flow_template_table_destroy(port_id, nb_id, id);
	return -ENOTSUP;
}

static inline int
parser_command_flow_template_table_resize_complete(portid_t port_id,
						   uint32_t table_id)
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->flow_template_table_resize_complete != NULL)
		return ops->flow_template_table_resize_complete(port_id, table_id);
	return -ENOTSUP;
}

static inline int
parser_command_queue_group_set_miss_actions(portid_t port_id,
					    const struct rte_flow_attr *attr,
					    const struct rte_flow_action actions[])
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->queue_group_set_miss_actions != NULL)
		return ops->queue_group_set_miss_actions(port_id, attr, actions);
	return -ENOTSUP;
}

static inline int
parser_command_flow_template_table_resize(portid_t port_id, uint32_t table_id,
					  uint32_t nb_rules)
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->flow_template_table_resize != NULL)
		return ops->flow_template_table_resize(port_id, table_id, nb_rules);
	return -ENOTSUP;
}

static inline int
parser_command_queue_flow_create(portid_t port_id, queueid_t queue,
				 bool postpone, uint32_t table_id,
				 uint32_t rule_id, uint32_t pattern_templ_id,
				 uint32_t actions_templ_id,
				 const struct rte_flow_item pattern[],
				 const struct rte_flow_action actions[])
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->queue_flow_create != NULL)
		return ops->queue_flow_create(port_id, queue, postpone, table_id,
				       rule_id, pattern_templ_id,
				       actions_templ_id, pattern, actions);
	return -ENOTSUP;
}

static inline int
parser_command_queue_flow_destroy(portid_t port_id, queueid_t queue,
				  bool postpone, uint32_t rule_n,
				  const uint64_t rule[])
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->queue_flow_destroy != NULL)
		return ops->queue_flow_destroy(port_id, queue, postpone, rule_n,
					rule);
	return -ENOTSUP;
}

static inline int
parser_command_queue_flow_update_resized(portid_t port_id, queueid_t queue,
					 bool postpone, uint32_t flow_id)
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->queue_flow_update_resized != NULL)
		return ops->queue_flow_update_resized(port_id, queue, postpone,
				       flow_id);
	return -ENOTSUP;
}

static inline int
parser_command_queue_flow_update(portid_t port_id, queueid_t queue,
				 bool postpone, uint32_t rule_id,
				 uint32_t action_templ_id,
				 const struct rte_flow_action actions[])
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->queue_flow_update != NULL)
		return ops->queue_flow_update(port_id, queue, postpone, rule_id,
				       action_templ_id, actions);
	return -ENOTSUP;
}

static inline int
parser_command_queue_flow_push(portid_t port_id, queueid_t queue)
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->queue_flow_push != NULL)
		return ops->queue_flow_push(port_id, queue);
	return -ENOTSUP;
}

static inline int
parser_command_queue_flow_pull(portid_t port_id, queueid_t queue)
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->queue_flow_pull != NULL)
		return ops->queue_flow_pull(port_id, queue);
	return -ENOTSUP;
}

static inline int
parser_command_flow_hash_calc(portid_t port_id, uint32_t table_id,
			      uint32_t pattern_templ_id,
			      const struct rte_flow_item pattern[])
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->flow_hash_calc != NULL)
		return ops->flow_hash_calc(port_id, table_id,
				    (uint8_t)pattern_templ_id, pattern);
	return -ENOTSUP;
}

static inline int
parser_command_flow_hash_calc_encap(portid_t port_id,
				    enum rte_flow_encap_hash_field field,
				    const struct rte_flow_item pattern[])
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->flow_hash_calc_encap != NULL)
		return ops->flow_hash_calc_encap(port_id, field, pattern);
	return -ENOTSUP;
}

static inline void
parser_command_queue_flow_aged(portid_t port_id, queueid_t queue, bool destroy)
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->queue_flow_aged != NULL)
		ops->queue_flow_aged(port_id, (uint32_t)queue, destroy ? 1 : 0);
}

static inline int
parser_command_queue_action_handle_create(portid_t port_id, queueid_t queue,
					  bool postpone, uint32_t group,
					  bool is_list,
					  const struct rte_flow_indir_action_conf *conf,
					  const struct rte_flow_action actions[])
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->queue_action_handle_create != NULL)
		return ops->queue_action_handle_create(port_id, (uint32_t)queue,
						postpone, group, is_list, conf, actions);
	return -ENOTSUP;
}

static inline int
parser_command_queue_action_handle_destroy(portid_t port_id, queueid_t queue,
					   bool postpone, uint32_t nb_id,
					   const uint32_t id[])
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->queue_action_handle_destroy != NULL)
		return ops->queue_action_handle_destroy(port_id, (uint32_t)queue,
						 postpone, nb_id, id);
	return -ENOTSUP;
}

static inline int
parser_command_queue_action_handle_update(portid_t port_id, queueid_t queue,
					  bool postpone, uint32_t group,
					  const struct rte_flow_action actions[])
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->queue_action_handle_update != NULL)
		return ops->queue_action_handle_update(port_id, (uint32_t)queue,
						postpone, group, actions);
	return -ENOTSUP;
}

static inline int
parser_command_queue_action_handle_query(portid_t port_id, queueid_t queue,
					 bool postpone, uint32_t action_id)
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->queue_action_handle_query != NULL)
		return ops->queue_action_handle_query(port_id, (uint32_t)queue,
					       postpone, action_id);
	return -ENOTSUP;
}

static inline void
parser_command_queue_action_handle_query_update(portid_t port_id,
						queueid_t queue, bool postpone,
						uint32_t action_id,
						enum rte_flow_query_update_mode qu_mode,
						const struct rte_flow_action actions[])
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->queue_action_handle_query_update != NULL)
		ops->queue_action_handle_query_update(port_id, (uint32_t)queue,
						      postpone, action_id, qu_mode,
						      actions);
}

static inline int
parser_command_action_handle_create(portid_t port_id, uint32_t group,
				    bool is_list,
				    const struct rte_flow_indir_action_conf *conf,
				    const struct rte_flow_action actions[])
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->action_handle_create != NULL)
		return ops->action_handle_create(port_id, group, is_list, conf,
					  actions);
	return -ENOTSUP;
}

static inline int
parser_command_action_handle_destroy(portid_t port_id, uint32_t nb_id,
				     const uint32_t id[])
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->action_handle_destroy != NULL)
		return ops->action_handle_destroy(port_id, nb_id, id);
	return -ENOTSUP;
}

static inline int
parser_command_action_handle_update(portid_t port_id, uint32_t group,
				    const struct rte_flow_action actions[])
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->action_handle_update != NULL)
		return ops->action_handle_update(port_id, group, actions);
	return -ENOTSUP;
}

static inline int
parser_command_action_handle_query(portid_t port_id, uint32_t action_id)
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->action_handle_query != NULL)
		return ops->action_handle_query(port_id, action_id);
	return -ENOTSUP;
}

static inline void
parser_command_action_handle_query_update(portid_t port_id,
					  uint32_t action_id,
					  enum rte_flow_query_update_mode qu_mode,
					  const struct rte_flow_action actions[])
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->action_handle_query_update != NULL)
		ops->action_handle_query_update(port_id, action_id, qu_mode,
						actions);
}

static inline int
parser_command_flow_validate(portid_t port_id,
			     const struct rte_flow_attr *attr,
			     const struct rte_flow_item pattern[],
			     const struct rte_flow_action actions[],
			     const struct rte_flow_parser_tunnel_ops *tunnel_ops)
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->flow_validate != NULL)
		return ops->flow_validate(port_id, attr, pattern, actions,
					   tunnel_ops);
	return -ENOTSUP;
}

static inline int
parser_command_flow_create(portid_t port_id,
			   const struct rte_flow_attr *attr,
			   const struct rte_flow_item pattern[],
			   const struct rte_flow_action actions[],
			   const struct rte_flow_parser_tunnel_ops *tunnel_ops,
			   uintptr_t user_id)
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->flow_create != NULL)
		return ops->flow_create(port_id, attr, pattern, actions, tunnel_ops,
					 user_id);
	return -ENOTSUP;
}

static inline int
parser_command_flow_destroy(portid_t port_id, uint32_t nb_rule,
			    const uint64_t rule[], bool is_user_id)
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->flow_destroy != NULL)
		return ops->flow_destroy(port_id, nb_rule, rule, is_user_id);
	return -ENOTSUP;
}

static inline int
parser_command_flow_update(portid_t port_id, uint32_t rule_id,
			   const struct rte_flow_action actions[],
			   bool is_user_id)
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->flow_update != NULL)
		return ops->flow_update(port_id, rule_id, actions, is_user_id);
	return -ENOTSUP;
}

static inline int
parser_command_flow_flush(portid_t port_id)
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->flow_flush != NULL)
		return ops->flow_flush(port_id);
	return -ENOTSUP;
}

static inline int
parser_command_flow_dump(portid_t port_id, bool all, uint64_t rule,
			 const char *file, bool is_user_id)
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->flow_dump != NULL)
		return ops->flow_dump(port_id, all, rule, file, is_user_id);
	return -ENOTSUP;
}

static inline int
parser_command_flow_query(portid_t port_id, uint64_t rule,
			  const struct rte_flow_action *action, bool is_user_id)
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->flow_query != NULL)
		return ops->flow_query(port_id, rule, action, is_user_id);
	return -ENOTSUP;
}

static inline void
parser_command_flow_list(portid_t port_id, uint32_t group_n,
			 const uint32_t group[])
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->flow_list != NULL)
		ops->flow_list(port_id, group_n, group);
}

static inline int
parser_command_flow_isolate(portid_t port_id, int set)
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->flow_isolate != NULL)
		return ops->flow_isolate(port_id, set);
	return -ENOTSUP;
}

static inline void
parser_command_flow_aged(portid_t port_id, int destroy)
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->flow_aged != NULL)
		ops->flow_aged(port_id, destroy);
}

static inline void
parser_command_flow_tunnel_create(portid_t port_id,
				  const struct rte_flow_parser_tunnel_ops *ops_conf)
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->flow_tunnel_create != NULL)
		ops->flow_tunnel_create(port_id, ops_conf);
}

static inline void
parser_command_flow_tunnel_destroy(portid_t port_id, uint32_t id)
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->flow_tunnel_destroy != NULL)
		ops->flow_tunnel_destroy(port_id, id);
}

static inline void
parser_command_flow_tunnel_list(portid_t port_id)
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->flow_tunnel_list != NULL)
		ops->flow_tunnel_list(port_id);
}

static inline int
parser_command_meter_policy_add(portid_t port_id, uint32_t policy_id,
				const struct rte_flow_action actions[])
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->meter_policy_add != NULL)
		return ops->meter_policy_add(port_id, policy_id, actions);
	return -ENOTSUP;
}

static inline void
parser_command_flex_item_create(portid_t port_id, uint16_t token,
				const char *filename)
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->flex_item_create != NULL)
		ops->flex_item_create(port_id, token, filename);
}

static inline void
parser_command_flex_item_destroy(portid_t port_id, uint16_t token)
{
	const struct rte_flow_parser_ops_command *ops = parser_command_ops();

	if (ops != NULL && ops->flex_item_destroy != NULL)
		ops->flex_item_destroy(port_id, token);
}

static inline void
parser_command_set_raw_encap(uint16_t index,
			     const struct rte_flow_item pattern[],
			     uint32_t pattern_n)
{
	if (parser_ctx_set_raw_common(true, index, pattern, pattern_n) == 0)
		return;
	RTE_LOG_LINE(ERR, EAL, "set_raw_encap failed for index %u", index);
}

static inline void
parser_command_set_raw_decap(uint16_t index,
			     const struct rte_flow_item pattern[],
			     uint32_t pattern_n)
{
	if (parser_ctx_set_raw_common(false, index, pattern, pattern_n) == 0)
		return;
	RTE_LOG_LINE(ERR, EAL, "set_raw_decap failed for index %u", index);
}

static inline void
parser_command_set_sample_actions(uint16_t index,
				  const struct rte_flow_action actions[],
				  uint32_t actions_n)
{
	if (parser_ctx_set_sample_actions(index, actions, actions_n) == 0)
		return;
	RTE_LOG_LINE(ERR, EAL, "set_sample_actions failed for index %u", index);
}

static inline void
parser_command_set_ipv6_ext_push(uint16_t index,
				 const struct rte_flow_item pattern[],
				 uint32_t pattern_n)
{
	if (parser_ctx_set_ipv6_ext_push(index, pattern, pattern_n) == 0)
		return;
	RTE_LOG_LINE(ERR, EAL, "set_ipv6_ext_push failed for index %u", index);
}

static inline void
parser_command_set_ipv6_ext_remove(uint16_t index,
				   const struct rte_flow_item pattern[],
				   uint32_t pattern_n)
{
	if (parser_ctx_set_ipv6_ext_remove(index, pattern, pattern_n) == 0)
		return;
	RTE_LOG_LINE(ERR, EAL, "set_ipv6_ext_remove failed for index %u", index);
}

/** Maximum size for pattern in struct rte_flow_item_raw. */
#define ITEM_RAW_PATTERN_SIZE 512

/** Maximum size for GENEVE option data pattern in bytes. */
#define ITEM_GENEVE_OPT_DATA_SIZE 124

/** Storage size for struct rte_flow_item_raw including pattern. */
#define ITEM_RAW_SIZE \
	(sizeof(struct rte_flow_item_raw) + ITEM_RAW_PATTERN_SIZE)

static const char *const compare_ops[] = {
	"eq", "ne", "lt", "le", "gt", "ge", NULL
};

/** Maximum size for external pattern in struct rte_flow_field_data. */
#define FLOW_FIELD_PATTERN_SIZE 32

/** Storage size for struct rte_flow_action_modify_field including pattern. */
#define ACTION_MODIFY_SIZE \
	(sizeof(struct rte_flow_action_modify_field) + \
	FLOW_FIELD_PATTERN_SIZE)

/** Storage for struct rte_flow_action_sample including external data. */
struct action_sample_data {
	struct rte_flow_action_sample conf;
	uint32_t idx;
};
static const char *const modify_field_ops[] = {
	"set", "add", "sub", NULL
};

static const char *const flow_field_ids[] = {
	"start", "mac_dst", "mac_src",
	"vlan_type", "vlan_id", "mac_type",
	"ipv4_dscp", "ipv4_ttl", "ipv4_src", "ipv4_dst",
	"ipv6_dscp", "ipv6_hoplimit", "ipv6_src", "ipv6_dst",
	"tcp_port_src", "tcp_port_dst",
	"tcp_seq_num", "tcp_ack_num", "tcp_flags",
	"udp_port_src", "udp_port_dst",
	"vxlan_vni", "geneve_vni", "gtp_teid",
	"tag", "mark", "meta", "pointer", "value",
	"ipv4_ecn", "ipv6_ecn", "gtp_psc_qfi", "meter_color",
	"ipv6_proto",
	"flex_item",
	"hash_result",
	"geneve_opt_type", "geneve_opt_class", "geneve_opt_data", "mpls",
	"tcp_data_off", "ipv4_ihl", "ipv4_total_len", "ipv6_payload_len",
	"ipv4_proto",
	"ipv6_flow_label", "ipv6_traffic_class",
	"esp_spi", "esp_seq_num", "esp_proto",
	"random",
	"vxlan_last_rsvd",
	NULL
};

static const char *const meter_colors[] = {
	"green", "yellow", "red", "all", NULL
};

static const char *const table_insertion_types[] = {
	"pattern", "index", "index_with_pattern", NULL
};

static const char *const table_hash_funcs[] = {
	"default", "linear", "crc32", "crc16", NULL
};

#define RAW_IPSEC_CONFS_MAX_NUM 8

/** Maximum number of subsequent tokens and arguments on the stack. */
#define CTX_STACK_SIZE 16

/** Parser context. */
struct context {
	/** Stack of subsequent token lists to process. */
	const enum rte_flow_parser_command_index *next[CTX_STACK_SIZE];
	/** Arguments for stacked tokens. */
	const void *args[CTX_STACK_SIZE];
	enum rte_flow_parser_command_index curr; /**< Current token index. */
	enum rte_flow_parser_command_index prev; /**< Index of the last token seen. */
	int next_num; /**< Number of entries in next[]. */
	int args_num; /**< Number of entries in args[]. */
	uint32_t eol:1; /**< EOL has been detected. */
	uint32_t last:1; /**< No more arguments. */
	portid_t port; /**< Current port ID (for completions). */
	uint32_t objdata; /**< Object-specific data. */
	void *object; /**< Address of current object for relative offsets. */
	void *objmask; /**< Object a full mask must be written to. */
};

/** Token argument. */
struct arg {
	uint32_t hton:1; /**< Use network byte ordering. */
	uint32_t sign:1; /**< Value is signed. */
	uint32_t bounded:1; /**< Value is bounded. */
	uintmax_t min; /**< Minimum value if bounded. */
	uintmax_t max; /**< Maximum value if bounded. */
	uint32_t offset; /**< Relative offset from ctx->object. */
	uint32_t size; /**< Field size. */
	const uint8_t *mask; /**< Bit-mask to use instead of offset/size. */
};

/** Parser token definition. */
struct token {
	/** Type displayed during completion (defaults to "TOKEN"). */
	const char *type;
	/** Help displayed during completion (defaults to token name). */
	const char *help;
	/** Private data used by parser functions. */
	const void *priv;
	/**
	 * Lists of subsequent tokens to push on the stack. Each call to the
	 * parser consumes the last entry of that stack.
	 */
	const enum rte_flow_parser_command_index *const *next;
	/** Arguments stack for subsequent tokens that need them. */
	const struct arg *const *args;
	/**
	 * Token-processing callback, returns -1 in case of error, the
	 * length of the matched string otherwise. If NULL, attempts to
	 * match the token name.
	 *
	 * If buf is not NULL, the result should be stored in it according
	 * to context. An error is returned if not large enough.
	 */
	int (*call)(struct context *ctx, const struct token *token,
		    const char *str, unsigned int len,
		    void *buf, unsigned int size);
	/**
	 * Callback that provides possible values for this token, used for
	 * completion. Returns -1 in case of error, the number of possible
	 * values otherwise. If NULL, the token name is used.
	 *
	 * If buf is not NULL, entry index ent is written to buf and the
	 * full length of the entry is returned (same behavior as
	 * snprintf()).
	 */
	int (*comp)(struct context *ctx, const struct token *token,
		    unsigned int ent, char *buf, unsigned int size);
	/** Mandatory token name, no default value. */
	const char *name;
};

static struct context default_parser_context;

static inline struct context *
parser_cmd_context(void)
{
	return &default_parser_context;
}

/** Static initializer for the next field. */
#define NEXT(...) (const enum rte_flow_parser_command_index *const []){ __VA_ARGS__, NULL, }

/** Static initializer for a NEXT() entry. */
#define NEXT_ENTRY(...) \
	(const enum rte_flow_parser_command_index []){ __VA_ARGS__, RTE_FLOW_PARSER_CMD_ZERO, }

/** Static initializer for the args field. */
#define ARGS(...) (const struct arg *const []){ __VA_ARGS__, NULL, }

/** Static initializer for ARGS() to target a field. */
#define ARGS_ENTRY(s, f) \
	(&(const struct arg){ \
		.offset = offsetof(s, f), \
		.size = sizeof(((s *)0)->f), \
	})

/** Static initializer for ARGS() to target a bit-field. */
#define ARGS_ENTRY_BF(s, f, b) \
	(&(const struct arg){ \
		.size = sizeof(s), \
		.mask = (const void *)&(const s){ .f = (1 << (b)) - 1 }, \
	})

/** Static initializer for ARGS() to target a field with limits. */
#define ARGS_ENTRY_BOUNDED(s, f, i, a) \
	(&(const struct arg){ \
		.bounded = 1, \
		.min = (i), \
		.max = (a), \
		.offset = offsetof(s, f), \
		.size = sizeof(((s *)0)->f), \
	})

/** Static initializer for ARGS() to target an arbitrary bit-mask. */
#define ARGS_ENTRY_MASK(s, f, m) \
	(&(const struct arg){ \
		.offset = offsetof(s, f), \
		.size = sizeof(((s *)0)->f), \
		.mask = (const void *)(m), \
	})

/** Same as ARGS_ENTRY_MASK() using network byte ordering for the value. */
#define ARGS_ENTRY_MASK_HTON(s, f, m) \
	(&(const struct arg){ \
		.hton = 1, \
		.offset = offsetof(s, f), \
		.size = sizeof(((s *)0)->f), \
		.mask = (const void *)(m), \
	})

/** Static initializer for ARGS() to target a pointer. */
#define ARGS_ENTRY_PTR(s, f) \
	(&(const struct arg){ \
		.size = sizeof(*((s *)0)->f), \
	})

/** Static initializer for ARGS() with arbitrary offset and size. */
#define ARGS_ENTRY_ARB(o, s) \
	(&(const struct arg){ \
		.offset = (o), \
		.size = (s), \
	})

/** Same as ARGS_ENTRY_ARB() with bounded values. */
#define ARGS_ENTRY_ARB_BOUNDED(o, s, i, a) \
	(&(const struct arg){ \
		.bounded = 1, \
		.min = (i), \
		.max = (a), \
		.offset = (o), \
		.size = (s), \
	})

/** Same as ARGS_ENTRY() using network byte ordering. */
#define ARGS_ENTRY_HTON(s, f) \
	(&(const struct arg){ \
		.hton = 1, \
		.offset = offsetof(s, f), \
		.size = sizeof(((s *)0)->f), \
	})

/** Same as ARGS_ENTRY_HTON() for a single argument, without structure. */
#define ARG_ENTRY_HTON(s) \
	(&(const struct arg){ \
		.hton = 1, \
		.offset = 0, \
		.size = sizeof(s), \
	})

/** Private data for pattern items. */
struct parse_item_priv {
	enum rte_flow_item_type type; /**< Item type. */
	uint32_t size; /**< Size of item specification structure. */
};

#define PRIV_ITEM(t, s) \
	(&(const struct parse_item_priv){ \
		.type = RTE_FLOW_ITEM_TYPE_ ## t, \
		.size = s, \
	})

/** Private data for actions. */
struct parse_action_priv {
	enum rte_flow_action_type type; /**< Action type. */
	uint32_t size; /**< Size of action configuration structure. */
};

#define PRIV_ACTION(t, s) \
	(&(const struct parse_action_priv){ \
		.type = RTE_FLOW_ACTION_TYPE_ ## t, \
		.size = s, \
	})

static const enum rte_flow_parser_command_index next_flex_item[] = {
	RTE_FLOW_PARSER_CMD_FLEX_ITEM_CREATE,
	RTE_FLOW_PARSER_CMD_FLEX_ITEM_DESTROY,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index next_config_attr[] = {
	RTE_FLOW_PARSER_CMD_CONFIG_QUEUES_NUMBER,
	RTE_FLOW_PARSER_CMD_CONFIG_QUEUES_SIZE,
	RTE_FLOW_PARSER_CMD_CONFIG_COUNTERS_NUMBER,
	RTE_FLOW_PARSER_CMD_CONFIG_AGING_OBJECTS_NUMBER,
	RTE_FLOW_PARSER_CMD_CONFIG_METERS_NUMBER,
	RTE_FLOW_PARSER_CMD_CONFIG_CONN_TRACK_NUMBER,
	RTE_FLOW_PARSER_CMD_CONFIG_QUOTAS_NUMBER,
	RTE_FLOW_PARSER_CMD_CONFIG_FLAGS,
	RTE_FLOW_PARSER_CMD_CONFIG_HOST_PORT,
	RTE_FLOW_PARSER_CMD_END,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index next_pt_subcmd[] = {
	RTE_FLOW_PARSER_CMD_PATTERN_TEMPLATE_CREATE,
	RTE_FLOW_PARSER_CMD_PATTERN_TEMPLATE_DESTROY,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index next_pt_attr[] = {
	RTE_FLOW_PARSER_CMD_PATTERN_TEMPLATE_CREATE_ID,
	RTE_FLOW_PARSER_CMD_PATTERN_TEMPLATE_RELAXED_MATCHING,
	RTE_FLOW_PARSER_CMD_PATTERN_TEMPLATE_INGRESS,
	RTE_FLOW_PARSER_CMD_PATTERN_TEMPLATE_EGRESS,
	RTE_FLOW_PARSER_CMD_PATTERN_TEMPLATE_TRANSFER,
	RTE_FLOW_PARSER_CMD_PATTERN_TEMPLATE_SPEC,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index next_pt_destroy_attr[] = {
	RTE_FLOW_PARSER_CMD_PATTERN_TEMPLATE_DESTROY_ID,
	RTE_FLOW_PARSER_CMD_END,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index next_at_subcmd[] = {
	RTE_FLOW_PARSER_CMD_ACTIONS_TEMPLATE_CREATE,
	RTE_FLOW_PARSER_CMD_ACTIONS_TEMPLATE_DESTROY,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index next_at_attr[] = {
	RTE_FLOW_PARSER_CMD_ACTIONS_TEMPLATE_CREATE_ID,
	RTE_FLOW_PARSER_CMD_ACTIONS_TEMPLATE_INGRESS,
	RTE_FLOW_PARSER_CMD_ACTIONS_TEMPLATE_EGRESS,
	RTE_FLOW_PARSER_CMD_ACTIONS_TEMPLATE_TRANSFER,
	RTE_FLOW_PARSER_CMD_ACTIONS_TEMPLATE_SPEC,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index next_at_destroy_attr[] = {
	RTE_FLOW_PARSER_CMD_ACTIONS_TEMPLATE_DESTROY_ID,
	RTE_FLOW_PARSER_CMD_END,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index next_group_attr[] = {
	RTE_FLOW_PARSER_CMD_GROUP_INGRESS,
	RTE_FLOW_PARSER_CMD_GROUP_EGRESS,
	RTE_FLOW_PARSER_CMD_GROUP_TRANSFER,
	RTE_FLOW_PARSER_CMD_GROUP_SET_MISS_ACTIONS,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index next_table_subcmd[] = {
	RTE_FLOW_PARSER_CMD_TABLE_CREATE,
	RTE_FLOW_PARSER_CMD_TABLE_DESTROY,
	RTE_FLOW_PARSER_CMD_TABLE_RESIZE,
	RTE_FLOW_PARSER_CMD_TABLE_RESIZE_COMPLETE,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index next_table_attr[] = {
	RTE_FLOW_PARSER_CMD_TABLE_CREATE_ID,
	RTE_FLOW_PARSER_CMD_TABLE_GROUP,
	RTE_FLOW_PARSER_CMD_TABLE_INSERTION_TYPE,
	RTE_FLOW_PARSER_CMD_TABLE_HASH_FUNC,
	RTE_FLOW_PARSER_CMD_TABLE_PRIORITY,
	RTE_FLOW_PARSER_CMD_TABLE_INGRESS,
	RTE_FLOW_PARSER_CMD_TABLE_EGRESS,
	RTE_FLOW_PARSER_CMD_TABLE_TRANSFER,
	RTE_FLOW_PARSER_CMD_TABLE_TRANSFER_WIRE_ORIG,
	RTE_FLOW_PARSER_CMD_TABLE_TRANSFER_VPORT_ORIG,
	RTE_FLOW_PARSER_CMD_TABLE_RESIZABLE,
	RTE_FLOW_PARSER_CMD_TABLE_RULES_NUMBER,
	RTE_FLOW_PARSER_CMD_TABLE_PATTERN_TEMPLATE,
	RTE_FLOW_PARSER_CMD_TABLE_ACTIONS_TEMPLATE,
	RTE_FLOW_PARSER_CMD_END,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index next_table_destroy_attr[] = {
	RTE_FLOW_PARSER_CMD_TABLE_DESTROY_ID,
	RTE_FLOW_PARSER_CMD_END,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index next_queue_subcmd[] = {
	RTE_FLOW_PARSER_CMD_QUEUE_CREATE,
	RTE_FLOW_PARSER_CMD_QUEUE_DESTROY,
	RTE_FLOW_PARSER_CMD_QUEUE_FLOW_UPDATE_RESIZED,
	RTE_FLOW_PARSER_CMD_QUEUE_UPDATE,
	RTE_FLOW_PARSER_CMD_QUEUE_AGED,
	RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index next_queue_destroy_attr[] = {
	RTE_FLOW_PARSER_CMD_QUEUE_DESTROY_ID,
	RTE_FLOW_PARSER_CMD_END,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index next_qia_subcmd[] = {
	RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_CREATE,
	RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_UPDATE,
	RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_DESTROY,
	RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_QUERY,
	RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_QUERY_UPDATE,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index next_qia_create_attr[] = {
	RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_CREATE_ID,
	RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_INGRESS,
	RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_EGRESS,
	RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_TRANSFER,
	RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_CREATE_POSTPONE,
	RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_SPEC,
	RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_LIST,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index next_qia_update_attr[] = {
	RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_UPDATE_POSTPONE,
	RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_SPEC,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index next_qia_destroy_attr[] = {
	RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_DESTROY_POSTPONE,
	RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_DESTROY_ID,
	RTE_FLOW_PARSER_CMD_END,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index next_qia_query_attr[] = {
	RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_QUERY_POSTPONE,
	RTE_FLOW_PARSER_CMD_END,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index next_ia_create_attr[] = {
	RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_CREATE_ID,
	RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_INGRESS,
	RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_EGRESS,
	RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_TRANSFER,
	RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_SPEC,
	RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_LIST,
	RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_FLOW_CONF,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index next_ia[] = {
	RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_ID2PTR,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO
};

static const enum rte_flow_parser_command_index next_ial[] = {
	RTE_FLOW_PARSER_CMD_ACTION_INDIRECT_LIST_HANDLE,
	RTE_FLOW_PARSER_CMD_ACTION_INDIRECT_LIST_CONF,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO
};

static const enum rte_flow_parser_command_index next_qia_qu_attr[] = {
	RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_QU_MODE,
	RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_UPDATE_POSTPONE,
	RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_SPEC,
	RTE_FLOW_PARSER_CMD_ZERO
};

static const enum rte_flow_parser_command_index next_ia_qu_attr[] = {
	RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_QU_MODE,
	RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_SPEC,
	RTE_FLOW_PARSER_CMD_ZERO
};

static const enum rte_flow_parser_command_index next_dump_subcmd[] = {
	RTE_FLOW_PARSER_CMD_DUMP_ALL,
	RTE_FLOW_PARSER_CMD_DUMP_ONE,
	RTE_FLOW_PARSER_CMD_DUMP_IS_USER_ID,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index next_ia_subcmd[] = {
	RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_CREATE,
	RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_UPDATE,
	RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_DESTROY,
	RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_QUERY,
	RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_QUERY_UPDATE,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index next_vc_attr[] = {
	RTE_FLOW_PARSER_CMD_VC_GROUP,
	RTE_FLOW_PARSER_CMD_VC_PRIORITY,
	RTE_FLOW_PARSER_CMD_VC_INGRESS,
	RTE_FLOW_PARSER_CMD_VC_EGRESS,
	RTE_FLOW_PARSER_CMD_VC_TRANSFER,
	RTE_FLOW_PARSER_CMD_VC_TUNNEL_SET,
	RTE_FLOW_PARSER_CMD_VC_TUNNEL_MATCH,
	RTE_FLOW_PARSER_CMD_VC_USER_ID,
	RTE_FLOW_PARSER_CMD_ITEM_PATTERN,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index next_destroy_attr[] = {
	RTE_FLOW_PARSER_CMD_DESTROY_RULE,
	RTE_FLOW_PARSER_CMD_DESTROY_IS_USER_ID,
	RTE_FLOW_PARSER_CMD_END,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index next_dump_attr[] = {
	RTE_FLOW_PARSER_CMD_COMMON_FILE_PATH,
	RTE_FLOW_PARSER_CMD_END,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index next_query_attr[] = {
	RTE_FLOW_PARSER_CMD_QUERY_IS_USER_ID,
	RTE_FLOW_PARSER_CMD_END,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index next_list_attr[] = {
	RTE_FLOW_PARSER_CMD_LIST_GROUP,
	RTE_FLOW_PARSER_CMD_END,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index next_aged_attr[] = {
	RTE_FLOW_PARSER_CMD_AGED_DESTROY,
	RTE_FLOW_PARSER_CMD_END,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index next_ia_destroy_attr[] = {
	RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_DESTROY_ID,
	RTE_FLOW_PARSER_CMD_END,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index next_async_insert_subcmd[] = {
	RTE_FLOW_PARSER_CMD_QUEUE_PATTERN_TEMPLATE,
	RTE_FLOW_PARSER_CMD_QUEUE_RULE_ID,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index next_async_pattern_subcmd[] = {
	RTE_FLOW_PARSER_CMD_QUEUE_PATTERN_TEMPLATE,
	RTE_FLOW_PARSER_CMD_QUEUE_ACTIONS_TEMPLATE,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_param[] = {
	RTE_FLOW_PARSER_CMD_ITEM_PARAM_IS,
	RTE_FLOW_PARSER_CMD_ITEM_PARAM_SPEC,
	RTE_FLOW_PARSER_CMD_ITEM_PARAM_LAST,
	RTE_FLOW_PARSER_CMD_ITEM_PARAM_MASK,
	RTE_FLOW_PARSER_CMD_ITEM_PARAM_PREFIX,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index next_item[] = {
	RTE_FLOW_PARSER_CMD_ITEM_END,
	RTE_FLOW_PARSER_CMD_ITEM_VOID,
	RTE_FLOW_PARSER_CMD_ITEM_INVERT,
	RTE_FLOW_PARSER_CMD_ITEM_ANY,
	RTE_FLOW_PARSER_CMD_ITEM_PORT_ID,
	RTE_FLOW_PARSER_CMD_ITEM_MARK,
	RTE_FLOW_PARSER_CMD_ITEM_RAW,
	RTE_FLOW_PARSER_CMD_ITEM_ETH,
	RTE_FLOW_PARSER_CMD_ITEM_VLAN,
	RTE_FLOW_PARSER_CMD_ITEM_IPV4,
	RTE_FLOW_PARSER_CMD_ITEM_IPV6,
	RTE_FLOW_PARSER_CMD_ITEM_ICMP,
	RTE_FLOW_PARSER_CMD_ITEM_UDP,
	RTE_FLOW_PARSER_CMD_ITEM_TCP,
	RTE_FLOW_PARSER_CMD_ITEM_SCTP,
	RTE_FLOW_PARSER_CMD_ITEM_VXLAN,
	RTE_FLOW_PARSER_CMD_ITEM_E_TAG,
	RTE_FLOW_PARSER_CMD_ITEM_NVGRE,
	RTE_FLOW_PARSER_CMD_ITEM_MPLS,
	RTE_FLOW_PARSER_CMD_ITEM_GRE,
	RTE_FLOW_PARSER_CMD_ITEM_FUZZY,
	RTE_FLOW_PARSER_CMD_ITEM_GTP,
	RTE_FLOW_PARSER_CMD_ITEM_GTPC,
	RTE_FLOW_PARSER_CMD_ITEM_GTPU,
	RTE_FLOW_PARSER_CMD_ITEM_GENEVE,
	RTE_FLOW_PARSER_CMD_ITEM_VXLAN_GPE,
	RTE_FLOW_PARSER_CMD_ITEM_ARP_ETH_IPV4,
	RTE_FLOW_PARSER_CMD_ITEM_IPV6_EXT,
	RTE_FLOW_PARSER_CMD_ITEM_IPV6_FRAG_EXT,
	RTE_FLOW_PARSER_CMD_ITEM_IPV6_ROUTING_EXT,
	RTE_FLOW_PARSER_CMD_ITEM_ICMP6,
	RTE_FLOW_PARSER_CMD_ITEM_ICMP6_ECHO_REQUEST,
	RTE_FLOW_PARSER_CMD_ITEM_ICMP6_ECHO_REPLY,
	RTE_FLOW_PARSER_CMD_ITEM_ICMP6_ND_NS,
	RTE_FLOW_PARSER_CMD_ITEM_ICMP6_ND_NA,
	RTE_FLOW_PARSER_CMD_ITEM_ICMP6_ND_OPT,
	RTE_FLOW_PARSER_CMD_ITEM_ICMP6_ND_OPT_SLA_ETH,
	RTE_FLOW_PARSER_CMD_ITEM_ICMP6_ND_OPT_TLA_ETH,
	RTE_FLOW_PARSER_CMD_ITEM_META,
	RTE_FLOW_PARSER_CMD_ITEM_RANDOM,
	RTE_FLOW_PARSER_CMD_ITEM_GRE_KEY,
	RTE_FLOW_PARSER_CMD_ITEM_GRE_OPTION,
	RTE_FLOW_PARSER_CMD_ITEM_GTP_PSC,
	RTE_FLOW_PARSER_CMD_ITEM_PPPOES,
	RTE_FLOW_PARSER_CMD_ITEM_PPPOED,
	RTE_FLOW_PARSER_CMD_ITEM_PPPOE_PROTO_ID,
	RTE_FLOW_PARSER_CMD_ITEM_HIGIG2,
	RTE_FLOW_PARSER_CMD_ITEM_TAG,
	RTE_FLOW_PARSER_CMD_ITEM_L2TPV3OIP,
	RTE_FLOW_PARSER_CMD_ITEM_ESP,
	RTE_FLOW_PARSER_CMD_ITEM_AH,
	RTE_FLOW_PARSER_CMD_ITEM_PFCP,
	RTE_FLOW_PARSER_CMD_ITEM_ECPRI,
	RTE_FLOW_PARSER_CMD_ITEM_GENEVE_OPT,
	RTE_FLOW_PARSER_CMD_ITEM_INTEGRITY,
	RTE_FLOW_PARSER_CMD_ITEM_CONNTRACK,
	RTE_FLOW_PARSER_CMD_ITEM_PORT_REPRESENTOR,
	RTE_FLOW_PARSER_CMD_ITEM_REPRESENTED_PORT,
	RTE_FLOW_PARSER_CMD_ITEM_FLEX,
	RTE_FLOW_PARSER_CMD_ITEM_L2TPV2,
	RTE_FLOW_PARSER_CMD_ITEM_PPP,
	RTE_FLOW_PARSER_CMD_ITEM_METER,
	RTE_FLOW_PARSER_CMD_ITEM_QUOTA,
	RTE_FLOW_PARSER_CMD_ITEM_AGGR_AFFINITY,
	RTE_FLOW_PARSER_CMD_ITEM_TX_QUEUE,
	RTE_FLOW_PARSER_CMD_ITEM_IB_BTH,
	RTE_FLOW_PARSER_CMD_ITEM_PTYPE,
	RTE_FLOW_PARSER_CMD_ITEM_NSH,
	RTE_FLOW_PARSER_CMD_ITEM_COMPARE,
	RTE_FLOW_PARSER_CMD_END_SET,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_fuzzy[] = {
	RTE_FLOW_PARSER_CMD_ITEM_FUZZY_THRESH,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_any[] = {
	RTE_FLOW_PARSER_CMD_ITEM_ANY_NUM,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_port_id[] = {
	RTE_FLOW_PARSER_CMD_ITEM_PORT_ID_ID,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_mark[] = {
	RTE_FLOW_PARSER_CMD_ITEM_MARK_ID,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_raw[] = {
	RTE_FLOW_PARSER_CMD_ITEM_RAW_RELATIVE,
	RTE_FLOW_PARSER_CMD_ITEM_RAW_SEARCH,
	RTE_FLOW_PARSER_CMD_ITEM_RAW_OFFSET,
	RTE_FLOW_PARSER_CMD_ITEM_RAW_LIMIT,
	RTE_FLOW_PARSER_CMD_ITEM_RAW_PATTERN,
	RTE_FLOW_PARSER_CMD_ITEM_RAW_PATTERN_HEX,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_eth[] = {
	RTE_FLOW_PARSER_CMD_ITEM_ETH_DST,
	RTE_FLOW_PARSER_CMD_ITEM_ETH_SRC,
	RTE_FLOW_PARSER_CMD_ITEM_ETH_TYPE,
	RTE_FLOW_PARSER_CMD_ITEM_ETH_HAS_VLAN,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_vlan[] = {
	RTE_FLOW_PARSER_CMD_ITEM_VLAN_TCI,
	RTE_FLOW_PARSER_CMD_ITEM_VLAN_PCP,
	RTE_FLOW_PARSER_CMD_ITEM_VLAN_DEI,
	RTE_FLOW_PARSER_CMD_ITEM_VLAN_VID,
	RTE_FLOW_PARSER_CMD_ITEM_VLAN_INNER_TYPE,
	RTE_FLOW_PARSER_CMD_ITEM_VLAN_HAS_MORE_VLAN,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_ipv4[] = {
	RTE_FLOW_PARSER_CMD_ITEM_IPV4_VER_IHL,
	RTE_FLOW_PARSER_CMD_ITEM_IPV4_TOS,
	RTE_FLOW_PARSER_CMD_ITEM_IPV4_LENGTH,
	RTE_FLOW_PARSER_CMD_ITEM_IPV4_ID,
	RTE_FLOW_PARSER_CMD_ITEM_IPV4_FRAGMENT_OFFSET,
	RTE_FLOW_PARSER_CMD_ITEM_IPV4_TTL,
	RTE_FLOW_PARSER_CMD_ITEM_IPV4_PROTO,
	RTE_FLOW_PARSER_CMD_ITEM_IPV4_SRC,
	RTE_FLOW_PARSER_CMD_ITEM_IPV4_DST,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_ipv6[] = {
	RTE_FLOW_PARSER_CMD_ITEM_IPV6_TC,
	RTE_FLOW_PARSER_CMD_ITEM_IPV6_FLOW,
	RTE_FLOW_PARSER_CMD_ITEM_IPV6_LEN,
	RTE_FLOW_PARSER_CMD_ITEM_IPV6_PROTO,
	RTE_FLOW_PARSER_CMD_ITEM_IPV6_HOP,
	RTE_FLOW_PARSER_CMD_ITEM_IPV6_SRC,
	RTE_FLOW_PARSER_CMD_ITEM_IPV6_DST,
	RTE_FLOW_PARSER_CMD_ITEM_IPV6_HAS_FRAG_EXT,
	RTE_FLOW_PARSER_CMD_ITEM_IPV6_ROUTING_EXT,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_ipv6_routing_ext[] = {
	RTE_FLOW_PARSER_CMD_ITEM_IPV6_ROUTING_EXT_TYPE,
	RTE_FLOW_PARSER_CMD_ITEM_IPV6_ROUTING_EXT_NEXT_HDR,
	RTE_FLOW_PARSER_CMD_ITEM_IPV6_ROUTING_EXT_SEG_LEFT,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_icmp[] = {
	RTE_FLOW_PARSER_CMD_ITEM_ICMP_TYPE,
	RTE_FLOW_PARSER_CMD_ITEM_ICMP_CODE,
	RTE_FLOW_PARSER_CMD_ITEM_ICMP_IDENT,
	RTE_FLOW_PARSER_CMD_ITEM_ICMP_SEQ,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_udp[] = {
	RTE_FLOW_PARSER_CMD_ITEM_UDP_SRC,
	RTE_FLOW_PARSER_CMD_ITEM_UDP_DST,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_tcp[] = {
	RTE_FLOW_PARSER_CMD_ITEM_TCP_SRC,
	RTE_FLOW_PARSER_CMD_ITEM_TCP_DST,
	RTE_FLOW_PARSER_CMD_ITEM_TCP_FLAGS,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_sctp[] = {
	RTE_FLOW_PARSER_CMD_ITEM_SCTP_SRC,
	RTE_FLOW_PARSER_CMD_ITEM_SCTP_DST,
	RTE_FLOW_PARSER_CMD_ITEM_SCTP_TAG,
	RTE_FLOW_PARSER_CMD_ITEM_SCTP_CKSUM,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_vxlan[] = {
	RTE_FLOW_PARSER_CMD_ITEM_VXLAN_VNI,
	RTE_FLOW_PARSER_CMD_ITEM_VXLAN_FLAG_G,
	RTE_FLOW_PARSER_CMD_ITEM_VXLAN_FLAG_VER,
	RTE_FLOW_PARSER_CMD_ITEM_VXLAN_FLAG_I,
	RTE_FLOW_PARSER_CMD_ITEM_VXLAN_FLAG_P,
	RTE_FLOW_PARSER_CMD_ITEM_VXLAN_FLAG_B,
	RTE_FLOW_PARSER_CMD_ITEM_VXLAN_FLAG_O,
	RTE_FLOW_PARSER_CMD_ITEM_VXLAN_FLAG_D,
	RTE_FLOW_PARSER_CMD_ITEM_VXLAN_FLAG_A,
	RTE_FLOW_PARSER_CMD_ITEM_VXLAN_GBP_ID,
	RTE_FLOW_PARSER_CMD_ITEM_VXLAN_GPE_PROTO,
	RTE_FLOW_PARSER_CMD_ITEM_VXLAN_FIRST_RSVD,
	RTE_FLOW_PARSER_CMD_ITEM_VXLAN_SECND_RSVD,
	RTE_FLOW_PARSER_CMD_ITEM_VXLAN_THIRD_RSVD,
	RTE_FLOW_PARSER_CMD_ITEM_VXLAN_LAST_RSVD,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_e_tag[] = {
	RTE_FLOW_PARSER_CMD_ITEM_E_TAG_GRP_ECID_B,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_nvgre[] = {
	RTE_FLOW_PARSER_CMD_ITEM_NVGRE_TNI,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_mpls[] = {
	RTE_FLOW_PARSER_CMD_ITEM_MPLS_LABEL,
	RTE_FLOW_PARSER_CMD_ITEM_MPLS_TC,
	RTE_FLOW_PARSER_CMD_ITEM_MPLS_S,
	RTE_FLOW_PARSER_CMD_ITEM_MPLS_TTL,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_gre[] = {
	RTE_FLOW_PARSER_CMD_ITEM_GRE_PROTO,
	RTE_FLOW_PARSER_CMD_ITEM_GRE_C_RSVD0_VER,
	RTE_FLOW_PARSER_CMD_ITEM_GRE_C_BIT,
	RTE_FLOW_PARSER_CMD_ITEM_GRE_K_BIT,
	RTE_FLOW_PARSER_CMD_ITEM_GRE_S_BIT,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_gre_key[] = {
	RTE_FLOW_PARSER_CMD_ITEM_GRE_KEY_VALUE,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_gre_option[] = {
	RTE_FLOW_PARSER_CMD_ITEM_GRE_OPTION_CHECKSUM,
	RTE_FLOW_PARSER_CMD_ITEM_GRE_OPTION_KEY,
	RTE_FLOW_PARSER_CMD_ITEM_GRE_OPTION_SEQUENCE,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_gtp[] = {
	RTE_FLOW_PARSER_CMD_ITEM_GTP_FLAGS,
	RTE_FLOW_PARSER_CMD_ITEM_GTP_MSG_TYPE,
	RTE_FLOW_PARSER_CMD_ITEM_GTP_TEID,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_geneve[] = {
	RTE_FLOW_PARSER_CMD_ITEM_GENEVE_VNI,
	RTE_FLOW_PARSER_CMD_ITEM_GENEVE_PROTO,
	RTE_FLOW_PARSER_CMD_ITEM_GENEVE_OPTLEN,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_vxlan_gpe[] = {
	RTE_FLOW_PARSER_CMD_ITEM_VXLAN_GPE_VNI,
	RTE_FLOW_PARSER_CMD_ITEM_VXLAN_GPE_PROTO_IN_DEPRECATED_VXLAN_GPE_HDR,
	RTE_FLOW_PARSER_CMD_ITEM_VXLAN_GPE_FLAGS,
	RTE_FLOW_PARSER_CMD_ITEM_VXLAN_GPE_RSVD0,
	RTE_FLOW_PARSER_CMD_ITEM_VXLAN_GPE_RSVD1,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_arp_eth_ipv4[] = {
	RTE_FLOW_PARSER_CMD_ITEM_ARP_ETH_IPV4_SHA,
	RTE_FLOW_PARSER_CMD_ITEM_ARP_ETH_IPV4_SPA,
	RTE_FLOW_PARSER_CMD_ITEM_ARP_ETH_IPV4_THA,
	RTE_FLOW_PARSER_CMD_ITEM_ARP_ETH_IPV4_TPA,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_ipv6_ext[] = {
	RTE_FLOW_PARSER_CMD_ITEM_IPV6_EXT_NEXT_HDR,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_ipv6_frag_ext[] = {
	RTE_FLOW_PARSER_CMD_ITEM_IPV6_FRAG_EXT_NEXT_HDR,
	RTE_FLOW_PARSER_CMD_ITEM_IPV6_FRAG_EXT_FRAG_DATA,
	RTE_FLOW_PARSER_CMD_ITEM_IPV6_FRAG_EXT_ID,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_icmp6[] = {
	RTE_FLOW_PARSER_CMD_ITEM_ICMP6_TYPE,
	RTE_FLOW_PARSER_CMD_ITEM_ICMP6_CODE,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_icmp6_echo_request[] = {
	RTE_FLOW_PARSER_CMD_ITEM_ICMP6_ECHO_REQUEST_ID,
	RTE_FLOW_PARSER_CMD_ITEM_ICMP6_ECHO_REQUEST_SEQ,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_icmp6_echo_reply[] = {
	RTE_FLOW_PARSER_CMD_ITEM_ICMP6_ECHO_REPLY_ID,
	RTE_FLOW_PARSER_CMD_ITEM_ICMP6_ECHO_REPLY_SEQ,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_icmp6_nd_ns[] = {
	RTE_FLOW_PARSER_CMD_ITEM_ICMP6_ND_NS_TARGET_ADDR,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_icmp6_nd_na[] = {
	RTE_FLOW_PARSER_CMD_ITEM_ICMP6_ND_NA_TARGET_ADDR,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_icmp6_nd_opt[] = {
	RTE_FLOW_PARSER_CMD_ITEM_ICMP6_ND_OPT_TYPE,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_icmp6_nd_opt_sla_eth[] = {
	RTE_FLOW_PARSER_CMD_ITEM_ICMP6_ND_OPT_SLA_ETH_SLA,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_icmp6_nd_opt_tla_eth[] = {
	RTE_FLOW_PARSER_CMD_ITEM_ICMP6_ND_OPT_TLA_ETH_TLA,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_meta[] = {
	RTE_FLOW_PARSER_CMD_ITEM_META_DATA,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_random[] = {
	RTE_FLOW_PARSER_CMD_ITEM_RANDOM_VALUE,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_gtp_psc[] = {
	RTE_FLOW_PARSER_CMD_ITEM_GTP_PSC_QFI,
	RTE_FLOW_PARSER_CMD_ITEM_GTP_PSC_PDU_T,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_pppoed[] = {
	RTE_FLOW_PARSER_CMD_ITEM_PPPOE_SEID,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_pppoes[] = {
	RTE_FLOW_PARSER_CMD_ITEM_PPPOE_SEID,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_pppoe_proto_id[] = {
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_higig2[] = {
	RTE_FLOW_PARSER_CMD_ITEM_HIGIG2_CLASSIFICATION,
	RTE_FLOW_PARSER_CMD_ITEM_HIGIG2_VID,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_esp[] = {
	RTE_FLOW_PARSER_CMD_ITEM_ESP_SPI,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_ah[] = {
	RTE_FLOW_PARSER_CMD_ITEM_AH_SPI,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_pfcp[] = {
	RTE_FLOW_PARSER_CMD_ITEM_PFCP_S_FIELD,
	RTE_FLOW_PARSER_CMD_ITEM_PFCP_SEID,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index next_set_raw[] = {
	RTE_FLOW_PARSER_CMD_SET_RAW_INDEX,
	RTE_FLOW_PARSER_CMD_ITEM_ETH,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_tag[] = {
	RTE_FLOW_PARSER_CMD_ITEM_TAG_DATA,
	RTE_FLOW_PARSER_CMD_ITEM_TAG_INDEX,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_l2tpv3oip[] = {
	RTE_FLOW_PARSER_CMD_ITEM_L2TPV3OIP_SESSION_ID,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_ecpri[] = {
	RTE_FLOW_PARSER_CMD_ITEM_ECPRI_COMMON,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_ecpri_common[] = {
	RTE_FLOW_PARSER_CMD_ITEM_ECPRI_COMMON_TYPE,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_ecpri_common_type[] = {
	RTE_FLOW_PARSER_CMD_ITEM_ECPRI_COMMON_TYPE_IQ_DATA,
	RTE_FLOW_PARSER_CMD_ITEM_ECPRI_COMMON_TYPE_RTC_CTRL,
	RTE_FLOW_PARSER_CMD_ITEM_ECPRI_COMMON_TYPE_DLY_MSR,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_geneve_opt[] = {
	RTE_FLOW_PARSER_CMD_ITEM_GENEVE_OPT_CLASS,
	RTE_FLOW_PARSER_CMD_ITEM_GENEVE_OPT_TYPE,
	RTE_FLOW_PARSER_CMD_ITEM_GENEVE_OPT_LENGTH,
	RTE_FLOW_PARSER_CMD_ITEM_GENEVE_OPT_DATA,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_integrity[] = {
	RTE_FLOW_PARSER_CMD_ITEM_INTEGRITY_LEVEL,
	RTE_FLOW_PARSER_CMD_ITEM_INTEGRITY_VALUE,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_integrity_lv[] = {
	RTE_FLOW_PARSER_CMD_ITEM_INTEGRITY_LEVEL,
	RTE_FLOW_PARSER_CMD_ITEM_INTEGRITY_VALUE,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_port_representor[] = {
	RTE_FLOW_PARSER_CMD_ITEM_PORT_REPRESENTOR_PORT_ID,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_represented_port[] = {
	RTE_FLOW_PARSER_CMD_ITEM_REPRESENTED_PORT_ETHDEV_PORT_ID,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_flex[] = {
	RTE_FLOW_PARSER_CMD_ITEM_FLEX_PATTERN_HANDLE,
	RTE_FLOW_PARSER_CMD_ITEM_FLEX_ITEM_HANDLE,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_l2tpv2[] = {
	RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_TYPE,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_l2tpv2_type[] = {
	RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_TYPE_DATA,
	RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_TYPE_DATA_L,
	RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_TYPE_DATA_S,
	RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_TYPE_DATA_O,
	RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_TYPE_DATA_L_S,
	RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_TYPE_CTRL,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_l2tpv2_type_data[] = {
	RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_DATA_TUNNEL_ID,
	RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_DATA_SESSION_ID,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_l2tpv2_type_data_l[] = {
	RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_DATA_L_LENGTH,
	RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_DATA_L_TUNNEL_ID,
	RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_DATA_L_SESSION_ID,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_l2tpv2_type_data_s[] = {
	RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_DATA_S_TUNNEL_ID,
	RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_DATA_S_SESSION_ID,
	RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_DATA_S_NS,
	RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_DATA_S_NR,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_l2tpv2_type_data_o[] = {
	RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_DATA_O_TUNNEL_ID,
	RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_DATA_O_SESSION_ID,
	RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_DATA_O_OFFSET,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_l2tpv2_type_data_l_s[] = {
	RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_DATA_L_S_LENGTH,
	RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_DATA_L_S_TUNNEL_ID,
	RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_DATA_L_S_SESSION_ID,
	RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_DATA_L_S_NS,
	RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_DATA_L_S_NR,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_l2tpv2_type_ctrl[] = {
	RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_CTRL_LENGTH,
	RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_CTRL_TUNNEL_ID,
	RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_CTRL_SESSION_ID,
	RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_CTRL_NS,
	RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_CTRL_NR,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_ppp[] = {
	RTE_FLOW_PARSER_CMD_ITEM_PPP_ADDR,
	RTE_FLOW_PARSER_CMD_ITEM_PPP_CTRL,
	RTE_FLOW_PARSER_CMD_ITEM_PPP_PROTO_ID,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_meter[] = {
	RTE_FLOW_PARSER_CMD_ITEM_METER_COLOR,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_quota[] = {
	RTE_FLOW_PARSER_CMD_ITEM_QUOTA_STATE,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_aggr_affinity[] = {
	RTE_FLOW_PARSER_CMD_ITEM_AGGR_AFFINITY_VALUE,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_tx_queue[] = {
	RTE_FLOW_PARSER_CMD_ITEM_TX_QUEUE_VALUE,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_ib_bth[] = {
	RTE_FLOW_PARSER_CMD_ITEM_IB_BTH_OPCODE,
	RTE_FLOW_PARSER_CMD_ITEM_IB_BTH_PKEY,
	RTE_FLOW_PARSER_CMD_ITEM_IB_BTH_DST_QPN,
	RTE_FLOW_PARSER_CMD_ITEM_IB_BTH_PSN,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_ptype[] = {
	RTE_FLOW_PARSER_CMD_ITEM_PTYPE_VALUE,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_nsh[] = {
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_compare_field[] = {
	RTE_FLOW_PARSER_CMD_ITEM_COMPARE_OP,
	RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_A_TYPE,
	RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_B_TYPE,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index compare_field_a[] = {
	RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_A_TYPE,
	RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_A_LEVEL,
	RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_A_TAG_INDEX,
	RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_A_TYPE_ID,
	RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_A_CLASS_ID,
	RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_A_OFFSET,
	RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_B_TYPE,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index compare_field_b[] = {
	RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_B_TYPE,
	RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_B_LEVEL,
	RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_B_TAG_INDEX,
	RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_B_TYPE_ID,
	RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_B_CLASS_ID,
	RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_B_OFFSET,
	RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_B_VALUE,
	RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_B_POINTER,
	RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_WIDTH,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index next_action[] = {
	RTE_FLOW_PARSER_CMD_ACTION_END,
	RTE_FLOW_PARSER_CMD_ACTION_VOID,
	RTE_FLOW_PARSER_CMD_ACTION_PASSTHRU,
	RTE_FLOW_PARSER_CMD_ACTION_SKIP_CMAN,
	RTE_FLOW_PARSER_CMD_ACTION_JUMP,
	RTE_FLOW_PARSER_CMD_ACTION_MARK,
	RTE_FLOW_PARSER_CMD_ACTION_FLAG,
	RTE_FLOW_PARSER_CMD_ACTION_QUEUE,
	RTE_FLOW_PARSER_CMD_ACTION_DROP,
	RTE_FLOW_PARSER_CMD_ACTION_COUNT,
	RTE_FLOW_PARSER_CMD_ACTION_RSS,
	RTE_FLOW_PARSER_CMD_ACTION_PF,
	RTE_FLOW_PARSER_CMD_ACTION_VF,
	RTE_FLOW_PARSER_CMD_ACTION_PORT_ID,
	RTE_FLOW_PARSER_CMD_ACTION_METER,
	RTE_FLOW_PARSER_CMD_ACTION_METER_COLOR,
	RTE_FLOW_PARSER_CMD_ACTION_METER_MARK,
	RTE_FLOW_PARSER_CMD_ACTION_METER_MARK_CONF,
	RTE_FLOW_PARSER_CMD_ACTION_OF_DEC_NW_TTL,
	RTE_FLOW_PARSER_CMD_ACTION_OF_POP_VLAN,
	RTE_FLOW_PARSER_CMD_ACTION_OF_PUSH_VLAN,
	RTE_FLOW_PARSER_CMD_ACTION_OF_SET_VLAN_VID,
	RTE_FLOW_PARSER_CMD_ACTION_OF_SET_VLAN_PCP,
	RTE_FLOW_PARSER_CMD_ACTION_OF_POP_MPLS,
	RTE_FLOW_PARSER_CMD_ACTION_OF_PUSH_MPLS,
	RTE_FLOW_PARSER_CMD_ACTION_VXLAN_ENCAP,
	RTE_FLOW_PARSER_CMD_ACTION_VXLAN_DECAP,
	RTE_FLOW_PARSER_CMD_ACTION_NVGRE_ENCAP,
	RTE_FLOW_PARSER_CMD_ACTION_NVGRE_DECAP,
	RTE_FLOW_PARSER_CMD_ACTION_L2_ENCAP,
	RTE_FLOW_PARSER_CMD_ACTION_L2_DECAP,
	RTE_FLOW_PARSER_CMD_ACTION_MPLSOGRE_ENCAP,
	RTE_FLOW_PARSER_CMD_ACTION_MPLSOGRE_DECAP,
	RTE_FLOW_PARSER_CMD_ACTION_MPLSOUDP_ENCAP,
	RTE_FLOW_PARSER_CMD_ACTION_MPLSOUDP_DECAP,
	RTE_FLOW_PARSER_CMD_ACTION_SET_IPV4_SRC,
	RTE_FLOW_PARSER_CMD_ACTION_SET_IPV4_DST,
	RTE_FLOW_PARSER_CMD_ACTION_SET_IPV6_SRC,
	RTE_FLOW_PARSER_CMD_ACTION_SET_IPV6_DST,
	RTE_FLOW_PARSER_CMD_ACTION_SET_TP_SRC,
	RTE_FLOW_PARSER_CMD_ACTION_SET_TP_DST,
	RTE_FLOW_PARSER_CMD_ACTION_MAC_SWAP,
	RTE_FLOW_PARSER_CMD_ACTION_DEC_TTL,
	RTE_FLOW_PARSER_CMD_ACTION_SET_TTL,
	RTE_FLOW_PARSER_CMD_ACTION_SET_MAC_SRC,
	RTE_FLOW_PARSER_CMD_ACTION_SET_MAC_DST,
	RTE_FLOW_PARSER_CMD_ACTION_INC_TCP_SEQ,
	RTE_FLOW_PARSER_CMD_ACTION_DEC_TCP_SEQ,
	RTE_FLOW_PARSER_CMD_ACTION_INC_TCP_ACK,
	RTE_FLOW_PARSER_CMD_ACTION_DEC_TCP_ACK,
	RTE_FLOW_PARSER_CMD_ACTION_RAW_ENCAP,
	RTE_FLOW_PARSER_CMD_ACTION_RAW_DECAP,
	RTE_FLOW_PARSER_CMD_ACTION_SET_TAG,
	RTE_FLOW_PARSER_CMD_ACTION_SET_META,
	RTE_FLOW_PARSER_CMD_ACTION_SET_IPV4_DSCP,
	RTE_FLOW_PARSER_CMD_ACTION_SET_IPV6_DSCP,
	RTE_FLOW_PARSER_CMD_ACTION_AGE,
	RTE_FLOW_PARSER_CMD_ACTION_AGE_UPDATE,
	RTE_FLOW_PARSER_CMD_ACTION_SAMPLE,
	RTE_FLOW_PARSER_CMD_ACTION_INDIRECT,
	RTE_FLOW_PARSER_CMD_ACTION_INDIRECT_LIST,
	RTE_FLOW_PARSER_CMD_ACTION_SHARED_INDIRECT,
	RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD,
	RTE_FLOW_PARSER_CMD_ACTION_CONNTRACK,
	RTE_FLOW_PARSER_CMD_ACTION_CONNTRACK_UPDATE,
	RTE_FLOW_PARSER_CMD_ACTION_PORT_REPRESENTOR,
	RTE_FLOW_PARSER_CMD_ACTION_REPRESENTED_PORT,
	RTE_FLOW_PARSER_CMD_ACTION_SEND_TO_KERNEL,
	RTE_FLOW_PARSER_CMD_ACTION_QUOTA_CREATE,
	RTE_FLOW_PARSER_CMD_ACTION_QUOTA_QU,
	RTE_FLOW_PARSER_CMD_ACTION_IPV6_EXT_REMOVE,
	RTE_FLOW_PARSER_CMD_ACTION_IPV6_EXT_PUSH,
	RTE_FLOW_PARSER_CMD_ACTION_NAT64,
	RTE_FLOW_PARSER_CMD_ACTION_JUMP_TO_TABLE_INDEX,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_quota_create[] = {
	RTE_FLOW_PARSER_CMD_ACTION_QUOTA_CREATE_LIMIT,
	RTE_FLOW_PARSER_CMD_ACTION_QUOTA_CREATE_MODE,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO
};

static const enum rte_flow_parser_command_index action_quota_update[] = {
	RTE_FLOW_PARSER_CMD_ACTION_QUOTA_QU_LIMIT,
	RTE_FLOW_PARSER_CMD_ACTION_QUOTA_QU_UPDATE_OP,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO
};

static const enum rte_flow_parser_command_index action_mark[] = {
	RTE_FLOW_PARSER_CMD_ACTION_MARK_ID,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_queue[] = {
	RTE_FLOW_PARSER_CMD_ACTION_QUEUE_INDEX,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_count[] = {
	RTE_FLOW_PARSER_CMD_ACTION_COUNT_ID,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_rss[] = {
	RTE_FLOW_PARSER_CMD_ACTION_RSS_FUNC,
	RTE_FLOW_PARSER_CMD_ACTION_RSS_LEVEL,
	RTE_FLOW_PARSER_CMD_ACTION_RSS_TYPES,
	RTE_FLOW_PARSER_CMD_ACTION_RSS_KEY,
	RTE_FLOW_PARSER_CMD_ACTION_RSS_KEY_LEN,
	RTE_FLOW_PARSER_CMD_ACTION_RSS_QUEUES,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_vf[] = {
	RTE_FLOW_PARSER_CMD_ACTION_VF_ORIGINAL,
	RTE_FLOW_PARSER_CMD_ACTION_VF_ID,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_port_id[] = {
	RTE_FLOW_PARSER_CMD_ACTION_PORT_ID_ORIGINAL,
	RTE_FLOW_PARSER_CMD_ACTION_PORT_ID_ID,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_meter[] = {
	RTE_FLOW_PARSER_CMD_ACTION_METER_ID,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_meter_color[] = {
	RTE_FLOW_PARSER_CMD_ACTION_METER_COLOR_TYPE,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_meter_mark[] = {
	RTE_FLOW_PARSER_CMD_ACTION_METER_PROFILE,
	RTE_FLOW_PARSER_CMD_ACTION_METER_POLICY,
	RTE_FLOW_PARSER_CMD_ACTION_METER_COLOR_MODE,
	RTE_FLOW_PARSER_CMD_ACTION_METER_STATE,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_of_push_vlan[] = {
	RTE_FLOW_PARSER_CMD_ACTION_OF_PUSH_VLAN_ETHERTYPE,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_of_set_vlan_vid[] = {
	RTE_FLOW_PARSER_CMD_ACTION_OF_SET_VLAN_VID_VLAN_VID,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_of_set_vlan_pcp[] = {
	RTE_FLOW_PARSER_CMD_ACTION_OF_SET_VLAN_PCP_VLAN_PCP,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_of_pop_mpls[] = {
	RTE_FLOW_PARSER_CMD_ACTION_OF_POP_MPLS_ETHERTYPE,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_of_push_mpls[] = {
	RTE_FLOW_PARSER_CMD_ACTION_OF_PUSH_MPLS_ETHERTYPE,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_set_ipv4_src[] = {
	RTE_FLOW_PARSER_CMD_ACTION_SET_IPV4_SRC_IPV4_SRC,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_set_mac_src[] = {
	RTE_FLOW_PARSER_CMD_ACTION_SET_MAC_SRC_MAC_SRC,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_set_ipv4_dst[] = {
	RTE_FLOW_PARSER_CMD_ACTION_SET_IPV4_DST_IPV4_DST,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_set_ipv6_src[] = {
	RTE_FLOW_PARSER_CMD_ACTION_SET_IPV6_SRC_IPV6_SRC,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_set_ipv6_dst[] = {
	RTE_FLOW_PARSER_CMD_ACTION_SET_IPV6_DST_IPV6_DST,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_set_tp_src[] = {
	RTE_FLOW_PARSER_CMD_ACTION_SET_TP_SRC_TP_SRC,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_set_tp_dst[] = {
	RTE_FLOW_PARSER_CMD_ACTION_SET_TP_DST_TP_DST,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_set_ttl[] = {
	RTE_FLOW_PARSER_CMD_ACTION_SET_TTL_TTL,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_jump[] = {
	RTE_FLOW_PARSER_CMD_ACTION_JUMP_GROUP,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_set_mac_dst[] = {
	RTE_FLOW_PARSER_CMD_ACTION_SET_MAC_DST_MAC_DST,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_inc_tcp_seq[] = {
	RTE_FLOW_PARSER_CMD_ACTION_INC_TCP_SEQ_VALUE,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_dec_tcp_seq[] = {
	RTE_FLOW_PARSER_CMD_ACTION_DEC_TCP_SEQ_VALUE,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_inc_tcp_ack[] = {
	RTE_FLOW_PARSER_CMD_ACTION_INC_TCP_ACK_VALUE,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_dec_tcp_ack[] = {
	RTE_FLOW_PARSER_CMD_ACTION_DEC_TCP_ACK_VALUE,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_raw_encap[] = {
	RTE_FLOW_PARSER_CMD_ACTION_RAW_ENCAP_SIZE,
	RTE_FLOW_PARSER_CMD_ACTION_RAW_ENCAP_INDEX,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_raw_decap[] = {
	RTE_FLOW_PARSER_CMD_ACTION_RAW_DECAP_INDEX,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_ipv6_ext_remove[] = {
	RTE_FLOW_PARSER_CMD_ACTION_IPV6_EXT_REMOVE_INDEX,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_ipv6_ext_push[] = {
	RTE_FLOW_PARSER_CMD_ACTION_IPV6_EXT_PUSH_INDEX,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_set_tag[] = {
	RTE_FLOW_PARSER_CMD_ACTION_SET_TAG_DATA,
	RTE_FLOW_PARSER_CMD_ACTION_SET_TAG_INDEX,
	RTE_FLOW_PARSER_CMD_ACTION_SET_TAG_MASK,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_set_meta[] = {
	RTE_FLOW_PARSER_CMD_ACTION_SET_META_DATA,
	RTE_FLOW_PARSER_CMD_ACTION_SET_META_MASK,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_set_ipv4_dscp[] = {
	RTE_FLOW_PARSER_CMD_ACTION_SET_IPV4_DSCP_VALUE,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_set_ipv6_dscp[] = {
	RTE_FLOW_PARSER_CMD_ACTION_SET_IPV6_DSCP_VALUE,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_age[] = {
	RTE_FLOW_PARSER_CMD_ACTION_AGE,
	RTE_FLOW_PARSER_CMD_ACTION_AGE_TIMEOUT,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_age_update[] = {
	RTE_FLOW_PARSER_CMD_ACTION_AGE_UPDATE,
	RTE_FLOW_PARSER_CMD_ACTION_AGE_UPDATE_TIMEOUT,
	RTE_FLOW_PARSER_CMD_ACTION_AGE_UPDATE_TOUCH,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_sample[] = {
	RTE_FLOW_PARSER_CMD_ACTION_SAMPLE,
	RTE_FLOW_PARSER_CMD_ACTION_SAMPLE_RATIO,
	RTE_FLOW_PARSER_CMD_ACTION_SAMPLE_INDEX,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index next_action_sample[] = {
	RTE_FLOW_PARSER_CMD_ACTION_QUEUE,
	RTE_FLOW_PARSER_CMD_ACTION_RSS,
	RTE_FLOW_PARSER_CMD_ACTION_MARK,
	RTE_FLOW_PARSER_CMD_ACTION_COUNT,
	RTE_FLOW_PARSER_CMD_ACTION_PORT_ID,
	RTE_FLOW_PARSER_CMD_ACTION_RAW_ENCAP,
	RTE_FLOW_PARSER_CMD_ACTION_VXLAN_ENCAP,
	RTE_FLOW_PARSER_CMD_ACTION_NVGRE_ENCAP,
	RTE_FLOW_PARSER_CMD_ACTION_REPRESENTED_PORT,
	RTE_FLOW_PARSER_CMD_ACTION_PORT_REPRESENTOR,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_ipv6_push_ext[] = {
	RTE_FLOW_PARSER_CMD_ITEM_IPV6_PUSH_REMOVE_EXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_ipv6_push_ext_type[] = {
	RTE_FLOW_PARSER_CMD_ITEM_IPV6_PUSH_REMOVE_EXT_TYPE,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index item_ipv6_push_ext_header[] = {
	RTE_FLOW_PARSER_CMD_ITEM_IPV6_ROUTING_EXT,
	RTE_FLOW_PARSER_CMD_ITEM_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_modify_field_dst[] = {
	RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_DST_LEVEL,
	RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_DST_TAG_INDEX,
	RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_DST_TYPE_ID,
	RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_DST_CLASS_ID,
	RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_DST_OFFSET,
	RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_SRC_TYPE,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_modify_field_src[] = {
	RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_SRC_LEVEL,
	RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_SRC_TAG_INDEX,
	RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_SRC_TYPE_ID,
	RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_SRC_CLASS_ID,
	RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_SRC_OFFSET,
	RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_SRC_VALUE,
	RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_SRC_POINTER,
	RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_WIDTH,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_update_conntrack[] = {
	RTE_FLOW_PARSER_CMD_ACTION_CONNTRACK_UPDATE_DIR,
	RTE_FLOW_PARSER_CMD_ACTION_CONNTRACK_UPDATE_CTX,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_port_representor[] = {
	RTE_FLOW_PARSER_CMD_ACTION_PORT_REPRESENTOR_PORT_ID,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_represented_port[] = {
	RTE_FLOW_PARSER_CMD_ACTION_REPRESENTED_PORT_ETHDEV_PORT_ID,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_nat64[] = {
	RTE_FLOW_PARSER_CMD_ACTION_NAT64_MODE,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index next_hash_subcmd[] = {
	RTE_FLOW_PARSER_CMD_HASH_CALC_TABLE,
	RTE_FLOW_PARSER_CMD_HASH_CALC_ENCAP,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index next_hash_encap_dest_subcmd[] = {
	RTE_FLOW_PARSER_CMD_ENCAP_HASH_FIELD_SRC_PORT,
	RTE_FLOW_PARSER_CMD_ENCAP_HASH_FIELD_GRE_FLOW_ID,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static const enum rte_flow_parser_command_index action_jump_to_table_index[] = {
	RTE_FLOW_PARSER_CMD_ACTION_JUMP_TO_TABLE_INDEX_TABLE,
	RTE_FLOW_PARSER_CMD_ACTION_JUMP_TO_TABLE_INDEX_INDEX,
	RTE_FLOW_PARSER_CMD_ACTION_NEXT,
	RTE_FLOW_PARSER_CMD_ZERO,
};

static int parse_set_raw_encap_decap(struct context *, const struct token *,
				     const char *, unsigned int,
				     void *, unsigned int);
static int parse_set_sample_action(struct context *, const struct token *,
				   const char *, unsigned int,
				   void *, unsigned int);
static int parse_set_ipv6_ext_action(struct context *, const struct token *,
				     const char *, unsigned int,
				     void *, unsigned int);
static int parse_set_init(struct context *, const struct token *,
			  const char *, unsigned int,
			  void *, unsigned int);
static int
parse_flex_handle(struct context *, const struct token *,
		  const char *, unsigned int, void *, unsigned int);
static int parse_init(struct context *, const struct token *,
		      const char *, unsigned int,
		      void *, unsigned int);
static int parse_vc(struct context *, const struct token *,
		    const char *, unsigned int,
		    void *, unsigned int);
static int parse_vc_spec(struct context *, const struct token *,
			 const char *, unsigned int, void *, unsigned int);
static int parse_vc_conf(struct context *, const struct token *,
			 const char *, unsigned int, void *, unsigned int);
static int parse_vc_conf_timeout(struct context *, const struct token *,
				 const char *, unsigned int, void *,
				 unsigned int);
static int parse_vc_item_ecpri_type(struct context *, const struct token *,
				    const char *, unsigned int,
				    void *, unsigned int);
static int parse_vc_item_l2tpv2_type(struct context *, const struct token *,
				    const char *, unsigned int,
				    void *, unsigned int);
static int parse_vc_action_meter_color_type(struct context *,
					const struct token *,
					const char *, unsigned int, void *,
					unsigned int);
static int parse_vc_action_rss(struct context *, const struct token *,
			       const char *, unsigned int, void *,
			       unsigned int);
static int parse_vc_action_rss_func(struct context *, const struct token *,
				    const char *, unsigned int, void *,
				    unsigned int);
static int parse_vc_action_rss_type(struct context *, const struct token *,
				    const char *, unsigned int, void *,
				    unsigned int);
static int parse_vc_action_rss_queue(struct context *, const struct token *,
				     const char *, unsigned int, void *,
				     unsigned int);
static int parse_vc_action_vxlan_encap(struct context *, const struct token *,
				       const char *, unsigned int, void *,
				       unsigned int);
static int parse_vc_action_nvgre_encap(struct context *, const struct token *,
				       const char *, unsigned int, void *,
				       unsigned int);
static int parse_vc_action_l2_encap(struct context *, const struct token *,
				    const char *, unsigned int, void *,
				    unsigned int);
static int parse_vc_action_l2_decap(struct context *, const struct token *,
				    const char *, unsigned int, void *,
				    unsigned int);
static int parse_vc_action_mplsogre_encap(struct context *,
					  const struct token *, const char *,
					  unsigned int, void *, unsigned int);
static int parse_vc_action_mplsogre_decap(struct context *,
					  const struct token *, const char *,
					  unsigned int, void *, unsigned int);
static int parse_vc_action_mplsoudp_encap(struct context *,
					  const struct token *, const char *,
					  unsigned int, void *, unsigned int);
static int parse_vc_action_mplsoudp_decap(struct context *,
					  const struct token *, const char *,
					  unsigned int, void *, unsigned int);
static int parse_vc_action_raw_encap(struct context *,
				     const struct token *, const char *,
				     unsigned int, void *, unsigned int);
static int parse_vc_action_raw_decap(struct context *,
				     const struct token *, const char *,
				     unsigned int, void *, unsigned int);
static int parse_vc_action_raw_encap_index(struct context *,
					   const struct token *, const char *,
					   unsigned int, void *, unsigned int);
static int parse_vc_action_raw_decap_index(struct context *,
					   const struct token *, const char *,
					   unsigned int, void *, unsigned int);
static int parse_vc_action_ipv6_ext_remove(struct context *ctx, const struct token *token,
					   const char *str, unsigned int len, void *buf,
					   unsigned int size);
static int parse_vc_action_ipv6_ext_remove_index(struct context *ctx,
						 const struct token *token,
						 const char *str, unsigned int len,
						 void *buf,
						 unsigned int size);
static int parse_vc_action_ipv6_ext_push(struct context *ctx, const struct token *token,
					 const char *str, unsigned int len, void *buf,
					 unsigned int size);
static int parse_vc_action_ipv6_ext_push_index(struct context *ctx,
					       const struct token *token,
					       const char *str, unsigned int len,
					       void *buf,
					       unsigned int size);
static int parse_vc_action_set_meta(struct context *ctx,
				    const struct token *token, const char *str,
				    unsigned int len, void *buf,
					unsigned int size);
static int parse_vc_action_sample(struct context *ctx,
				    const struct token *token, const char *str,
				    unsigned int len, void *buf,
				    unsigned int size);
static int
parse_vc_action_sample_index(struct context *ctx, const struct token *token,
				const char *str, unsigned int len, void *buf,
				unsigned int size);
static int
parse_vc_modify_field_op(struct context *ctx, const struct token *token,
				const char *str, unsigned int len, void *buf,
				unsigned int size);
static int
parse_vc_modify_field_id(struct context *ctx, const struct token *token,
				const char *str, unsigned int len, void *buf,
				unsigned int size);
static int
parse_vc_modify_field_level(struct context *ctx, const struct token *token,
				const char *str, unsigned int len, void *buf,
				unsigned int size);
static int
parse_vc_action_conntrack_update(struct context *ctx, const struct token *token,
			 const char *str, unsigned int len, void *buf,
			 unsigned int size);
static int parse_destroy(struct context *, const struct token *,
			 const char *, unsigned int,
			 void *, unsigned int);
static int parse_flush(struct context *, const struct token *,
		       const char *, unsigned int,
		       void *, unsigned int);
static int parse_dump(struct context *, const struct token *,
		      const char *, unsigned int,
		      void *, unsigned int);
static int parse_query(struct context *, const struct token *,
		       const char *, unsigned int,
		       void *, unsigned int);
static int parse_action(struct context *, const struct token *,
			const char *, unsigned int,
			void *, unsigned int);
static int parse_list(struct context *, const struct token *,
		      const char *, unsigned int,
		      void *, unsigned int);
static int parse_aged(struct context *, const struct token *,
		      const char *, unsigned int,
		      void *, unsigned int);
static int parse_isolate(struct context *, const struct token *,
			 const char *, unsigned int,
			 void *, unsigned int);
static int parse_configure(struct context *, const struct token *,
			   const char *, unsigned int,
			   void *, unsigned int);
static int parse_template(struct context *, const struct token *,
			  const char *, unsigned int,
			  void *, unsigned int);
static int parse_template_destroy(struct context *, const struct token *,
				  const char *, unsigned int,
				  void *, unsigned int);
static int parse_table(struct context *, const struct token *,
		       const char *, unsigned int, void *, unsigned int);
static int parse_table_destroy(struct context *, const struct token *,
			       const char *, unsigned int,
			       void *, unsigned int);
static int parse_jump_table_id(struct context *, const struct token *,
			       const char *, unsigned int,
			       void *, unsigned int);
static int parse_qo(struct context *, const struct token *,
		    const char *, unsigned int,
		    void *, unsigned int);
static int parse_qo_destroy(struct context *, const struct token *,
			    const char *, unsigned int,
			    void *, unsigned int);
static int parse_qia(struct context *, const struct token *,
		     const char *, unsigned int,
		     void *, unsigned int);
static int parse_qia_destroy(struct context *, const struct token *,
			     const char *, unsigned int,
			     void *, unsigned int);
static int parse_push(struct context *, const struct token *,
		      const char *, unsigned int,
		      void *, unsigned int);
static int parse_pull(struct context *, const struct token *,
		      const char *, unsigned int,
		      void *, unsigned int);
static int parse_group(struct context *, const struct token *,
		       const char *, unsigned int,
		       void *, unsigned int);
static int parse_hash(struct context *, const struct token *,
		      const char *, unsigned int,
		      void *, unsigned int);
static int parse_tunnel(struct context *, const struct token *,
			const char *, unsigned int,
			void *, unsigned int);
static int parse_flex(struct context *, const struct token *,
		      const char *, unsigned int, void *, unsigned int);
static int parse_int(struct context *, const struct token *,
		     const char *, unsigned int,
		     void *, unsigned int);
static int parse_prefix(struct context *, const struct token *,
			const char *, unsigned int,
			void *, unsigned int);
static int parse_boolean(struct context *, const struct token *,
			 const char *, unsigned int,
			 void *, unsigned int);
static int parse_string(struct context *, const struct token *,
			const char *, unsigned int,
			void *, unsigned int);
static int parse_hex(struct context *ctx, const struct token *token,
			const char *str, unsigned int len,
			void *buf, unsigned int size);
static int parse_string0(struct context *, const struct token *,
			const char *, unsigned int,
			void *, unsigned int);
static int parse_mac_addr(struct context *, const struct token *,
			  const char *, unsigned int,
			  void *, unsigned int);
static int parse_ipv4_addr(struct context *, const struct token *,
			   const char *, unsigned int,
			   void *, unsigned int);
static int parse_ipv6_addr(struct context *, const struct token *,
			   const char *, unsigned int,
			   void *, unsigned int);
static int parse_port(struct context *, const struct token *,
		      const char *, unsigned int,
		      void *, unsigned int);
static int parse_ia(struct context *, const struct token *,
		    const char *, unsigned int,
		    void *, unsigned int);
static int parse_ia_destroy(struct context *ctx, const struct token *token,
			    const char *str, unsigned int len,
			    void *buf, unsigned int size);
static int parse_ia_id2ptr(struct context *ctx, const struct token *token,
			   const char *str, unsigned int len, void *buf,
			   unsigned int size);

static int parse_indlst_id2ptr(struct context *ctx, const struct token *token,
			       const char *str, unsigned int len, void *buf,
			       unsigned int size);
static int parse_ia_port(struct context *ctx, const struct token *token,
			 const char *str, unsigned int len, void *buf,
			 unsigned int size);
static int parse_mp(struct context *, const struct token *,
		    const char *, unsigned int,
		    void *, unsigned int);
static int parse_meter_profile_id2ptr(struct context *ctx,
				      const struct token *token,
				      const char *str, unsigned int len,
				      void *buf, unsigned int size);
static int parse_meter_policy_id2ptr(struct context *ctx,
				     const struct token *token,
				     const char *str, unsigned int len,
				     void *buf, unsigned int size);
static int parse_meter_color(struct context *ctx, const struct token *token,
			     const char *str, unsigned int len, void *buf,
			     unsigned int size);
static int parse_insertion_table_type(struct context *ctx, const struct token *token,
				      const char *str, unsigned int len, void *buf,
				      unsigned int size);
static int parse_hash_table_type(struct context *ctx, const struct token *token,
				 const char *str, unsigned int len, void *buf,
				 unsigned int size);
static int
parse_quota_state_name(struct context *ctx, const struct token *token,
		       const char *str, unsigned int len, void *buf,
		       unsigned int size);
static int
parse_quota_mode_name(struct context *ctx, const struct token *token,
		      const char *str, unsigned int len, void *buf,
		      unsigned int size);
static int
parse_quota_update_name(struct context *ctx, const struct token *token,
			const char *str, unsigned int len, void *buf,
			unsigned int size);
static int
parse_qu_mode_name(struct context *ctx, const struct token *token,
		   const char *str, unsigned int len, void *buf,
		   unsigned int size);
static int comp_none(struct context *, const struct token *,
		     unsigned int, char *, unsigned int);
static int comp_boolean(struct context *, const struct token *,
			unsigned int, char *, unsigned int);
static int comp_action(struct context *, const struct token *,
		       unsigned int, char *, unsigned int);
static int comp_port(struct context *, const struct token *,
		     unsigned int, char *, unsigned int);
static int comp_rule_id(struct context *, const struct token *,
			unsigned int, char *, unsigned int);
static int comp_vc_action_rss_type(struct context *, const struct token *,
				   unsigned int, char *, unsigned int);
static int comp_vc_action_rss_queue(struct context *, const struct token *,
				    unsigned int, char *, unsigned int);
static int comp_set_raw_index(struct context *, const struct token *,
			      unsigned int, char *, unsigned int);
static int comp_set_sample_index(struct context *, const struct token *,
			      unsigned int, char *, unsigned int);
static int comp_set_ipv6_ext_index(struct context *ctx, const struct token *token,
				   unsigned int ent, char *buf, unsigned int size);
static int comp_set_modify_field_op(struct context *, const struct token *,
			      unsigned int, char *, unsigned int);
static int comp_set_modify_field_id(struct context *, const struct token *,
			      unsigned int, char *, unsigned int);
static int comp_pattern_template_id(struct context *, const struct token *,
				    unsigned int, char *, unsigned int);
static int comp_actions_template_id(struct context *, const struct token *,
				    unsigned int, char *, unsigned int);
static int comp_table_id(struct context *, const struct token *,
			 unsigned int, char *, unsigned int);
static int comp_queue_id(struct context *, const struct token *,
			 unsigned int, char *, unsigned int);
static int comp_meter_color(struct context *, const struct token *,
			    unsigned int, char *, unsigned int);
static int comp_insertion_table_type(struct context *, const struct token *,
				     unsigned int, char *, unsigned int);
static int comp_hash_table_type(struct context *, const struct token *,
				unsigned int, char *, unsigned int);
static int
comp_quota_state_name(struct context *ctx, const struct token *token,
		      unsigned int ent, char *buf, unsigned int size);
static int
comp_quota_mode_name(struct context *ctx, const struct token *token,
		     unsigned int ent, char *buf, unsigned int size);
static int
comp_quota_update_name(struct context *ctx, const struct token *token,
		       unsigned int ent, char *buf, unsigned int size);
static int
comp_qu_mode_name(struct context *ctx, const struct token *token,
		  unsigned int ent, char *buf, unsigned int size);
static int
comp_set_compare_field_id(struct context *ctx, const struct token *token,
			  unsigned int ent, char *buf, unsigned int size);
static int
comp_set_compare_op(struct context *ctx, const struct token *token,
		    unsigned int ent, char *buf, unsigned int size);
static int
parse_vc_compare_op(struct context *ctx, const struct token *token,
			 const char *str, unsigned int len, void *buf,
			 unsigned int size);
static int
parse_vc_compare_field_id(struct context *ctx, const struct token *token,
			  const char *str, unsigned int len, void *buf,
			  unsigned int size);
static int
parse_vc_compare_field_level(struct context *ctx, const struct token *token,
			     const char *str, unsigned int len, void *buf,
			     unsigned int size);

struct indlst_conf {
	uint32_t id;
	uint32_t conf_num;
	struct rte_flow_action *actions;
	const void **conf;
	SLIST_ENTRY(indlst_conf) next;
};

static const struct indlst_conf *indirect_action_list_conf_get(uint32_t conf_id);

/** Token definitions. */
static const struct token token_list[] = {
	/* Special tokens. */
	[RTE_FLOW_PARSER_CMD_ZERO] = {
		.name = "RTE_FLOW_PARSER_CMD_ZERO",
		.help = "null entry, abused as the entry point",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_FLOW,
					RTE_FLOW_PARSER_CMD_ADD)),
	},
	[RTE_FLOW_PARSER_CMD_END] = {
		.name = "",
		.type = "RETURN",
		.help = "command may end here",
	},
	[RTE_FLOW_PARSER_CMD_START_SET] = {
		.name = "RTE_FLOW_PARSER_CMD_START_SET",
		.help = "null entry, abused as the entry point for set",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_SET)),
	},
	[RTE_FLOW_PARSER_CMD_END_SET] = {
		.name = "end_set",
		.type = "RETURN",
		.help = "set command may end here",
	},
	/* Common tokens. */
	[RTE_FLOW_PARSER_CMD_COMMON_INTEGER] = {
		.name = "{int}",
		.type = "INTEGER",
		.help = "integer value",
		.call = parse_int,
		.comp = comp_none,
	},
	[RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED] = {
		.name = "{unsigned}",
		.type = "UNSIGNED",
		.help = "unsigned integer value",
		.call = parse_int,
		.comp = comp_none,
	},
	[RTE_FLOW_PARSER_CMD_COMMON_PREFIX] = {
		.name = "{prefix}",
		.type = "PREFIX",
		.help = "prefix length for bit-mask",
		.call = parse_prefix,
		.comp = comp_none,
	},
	[RTE_FLOW_PARSER_CMD_COMMON_BOOLEAN] = {
		.name = "{boolean}",
		.type = "BOOLEAN",
		.help = "any boolean value",
		.call = parse_boolean,
		.comp = comp_boolean,
	},
	[RTE_FLOW_PARSER_CMD_COMMON_STRING] = {
		.name = "{string}",
		.type = "STRING",
		.help = "fixed string",
		.call = parse_string,
		.comp = comp_none,
	},
	[RTE_FLOW_PARSER_CMD_COMMON_HEX] = {
		.name = "{hex}",
		.type = "HEX",
		.help = "fixed string",
		.call = parse_hex,
	},
	[RTE_FLOW_PARSER_CMD_COMMON_FILE_PATH] = {
		.name = "{file path}",
		.type = "STRING",
		.help = "file path",
		.call = parse_string0,
		.comp = comp_none,
	},
	[RTE_FLOW_PARSER_CMD_COMMON_MAC_ADDR] = {
		.name = "{MAC address}",
		.type = "MAC-48",
		.help = "standard MAC address notation",
		.call = parse_mac_addr,
		.comp = comp_none,
	},
	[RTE_FLOW_PARSER_CMD_COMMON_IPV4_ADDR] = {
		.name = "{IPv4 address}",
		.type = "IPV4 ADDRESS",
		.help = "standard IPv4 address notation",
		.call = parse_ipv4_addr,
		.comp = comp_none,
	},
	[RTE_FLOW_PARSER_CMD_COMMON_IPV6_ADDR] = {
		.name = "{IPv6 address}",
		.type = "IPV6 ADDRESS",
		.help = "standard IPv6 address notation",
		.call = parse_ipv6_addr,
		.comp = comp_none,
	},
	[RTE_FLOW_PARSER_CMD_COMMON_RULE_ID] = {
		.name = "{rule id}",
		.type = "RULE ID",
		.help = "rule identifier",
		.call = parse_int,
		.comp = comp_rule_id,
	},
	[RTE_FLOW_PARSER_CMD_COMMON_PORT_ID] = {
		.name = "{port_id}",
		.type = "PORT ID",
		.help = "port identifier",
		.call = parse_port,
		.comp = comp_port,
	},
	[RTE_FLOW_PARSER_CMD_COMMON_GROUP_ID] = {
		.name = "{group_id}",
		.type = "GROUP ID",
		.help = "group identifier",
		.call = parse_int,
		.comp = comp_none,
	},
	[RTE_FLOW_PARSER_CMD_COMMON_PRIORITY_LEVEL] = {
		.name = "{level}",
		.type = "PRIORITY",
		.help = "priority level",
		.call = parse_int,
		.comp = comp_none,
	},
	[RTE_FLOW_PARSER_CMD_COMMON_INDIRECT_ACTION_ID] = {
		.name = "{indirect_action_id}",
		.type = "INDIRECT_ACTION_ID",
		.help = "indirect action id",
		.call = parse_int,
		.comp = comp_none,
	},
	[RTE_FLOW_PARSER_CMD_COMMON_PROFILE_ID] = {
		.name = "{profile_id}",
		.type = "PROFILE_ID",
		.help = "profile id",
		.call = parse_int,
		.comp = comp_none,
	},
	[RTE_FLOW_PARSER_CMD_COMMON_POLICY_ID] = {
		.name = "{policy_id}",
		.type = "POLICY_ID",
		.help = "policy id",
		.call = parse_int,
		.comp = comp_none,
	},
	[RTE_FLOW_PARSER_CMD_COMMON_FLEX_TOKEN] = {
		.name = "{flex token}",
		.type = "flex token",
		.help = "flex token",
		.call = parse_int,
		.comp = comp_none,
	},
	[RTE_FLOW_PARSER_CMD_COMMON_FLEX_HANDLE] = {
		.name = "{flex handle}",
		.type = "RTE_FLOW_PARSER_CMD_FLEX HANDLE",
		.help = "fill flex item data",
		.call = parse_flex_handle,
		.comp = comp_none,
	},
	[RTE_FLOW_PARSER_CMD_COMMON_PATTERN_TEMPLATE_ID] = {
		.name = "{pattern_template_id}",
		.type = "PATTERN_TEMPLATE_ID",
		.help = "pattern template id",
		.call = parse_int,
		.comp = comp_pattern_template_id,
	},
	[RTE_FLOW_PARSER_CMD_COMMON_ACTIONS_TEMPLATE_ID] = {
		.name = "{actions_template_id}",
		.type = "ACTIONS_TEMPLATE_ID",
		.help = "actions template id",
		.call = parse_int,
		.comp = comp_actions_template_id,
	},
	[RTE_FLOW_PARSER_CMD_COMMON_TABLE_ID] = {
		.name = "{table_id}",
		.type = "TABLE_ID",
		.help = "table id",
		.call = parse_int,
		.comp = comp_table_id,
	},
	[RTE_FLOW_PARSER_CMD_COMMON_QUEUE_ID] = {
		.name = "{queue_id}",
		.type = "QUEUE_ID",
		.help = "queue id",
		.call = parse_int,
		.comp = comp_queue_id,
	},
	[RTE_FLOW_PARSER_CMD_COMMON_METER_COLOR_NAME] = {
		.name = "color_name",
		.help = "meter color name",
		.call = parse_meter_color,
		.comp = comp_meter_color,
	},
	/* Top-level command. */
	[RTE_FLOW_PARSER_CMD_FLOW] = {
		.name = "flow",
		.type = "{command} {port_id} [{arg} [...]]",
		.help = "manage ingress/egress flow rules",
		.next = NEXT(NEXT_ENTRY
			     (RTE_FLOW_PARSER_CMD_INFO,
			      RTE_FLOW_PARSER_CMD_CONFIGURE,
			      RTE_FLOW_PARSER_CMD_PATTERN_TEMPLATE,
			      RTE_FLOW_PARSER_CMD_ACTIONS_TEMPLATE,
			      RTE_FLOW_PARSER_CMD_TABLE,
			      RTE_FLOW_PARSER_CMD_FLOW_GROUP,
			      RTE_FLOW_PARSER_CMD_INDIRECT_ACTION,
			      RTE_FLOW_PARSER_CMD_VALIDATE,
			      RTE_FLOW_PARSER_CMD_CREATE,
			      RTE_FLOW_PARSER_CMD_DESTROY,
			      RTE_FLOW_PARSER_CMD_UPDATE,
			      RTE_FLOW_PARSER_CMD_FLUSH,
			      RTE_FLOW_PARSER_CMD_DUMP,
			      RTE_FLOW_PARSER_CMD_LIST,
			      RTE_FLOW_PARSER_CMD_AGED,
			      RTE_FLOW_PARSER_CMD_QUERY,
			      RTE_FLOW_PARSER_CMD_ISOLATE,
			      RTE_FLOW_PARSER_CMD_TUNNEL,
			      RTE_FLOW_PARSER_CMD_FLEX,
			      RTE_FLOW_PARSER_CMD_QUEUE,
			      RTE_FLOW_PARSER_CMD_PUSH,
			      RTE_FLOW_PARSER_CMD_PULL,
			      RTE_FLOW_PARSER_CMD_HASH)),
		.call = parse_init,
	},
	/* Top-level command. */
	[RTE_FLOW_PARSER_CMD_INFO] = {
		.name = "info",
		.help = "get information about flow engine",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_END),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, port)),
		.call = parse_configure,
	},
	/* Top-level command. */
	[RTE_FLOW_PARSER_CMD_CONFIGURE] = {
		.name = "configure",
		.help = "configure flow engine",
		.next = NEXT(next_config_attr,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, port)),
		.call = parse_configure,
	},
	/* Configure arguments. */
	[RTE_FLOW_PARSER_CMD_CONFIG_QUEUES_NUMBER] = {
		.name = "queues_number",
		.help = "number of queues",
		.next = NEXT(next_config_attr,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output,
					args.configure.nb_queue)),
	},
	[RTE_FLOW_PARSER_CMD_CONFIG_QUEUES_SIZE] = {
		.name = "queues_size",
		.help = "number of elements in queues",
		.next = NEXT(next_config_attr,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output,
					args.configure.queue_attr.size)),
	},
	[RTE_FLOW_PARSER_CMD_CONFIG_COUNTERS_NUMBER] = {
		.name = "counters_number",
		.help = "number of counters",
		.next = NEXT(next_config_attr,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output,
					args.configure.port_attr.nb_counters)),
	},
	[RTE_FLOW_PARSER_CMD_CONFIG_AGING_OBJECTS_NUMBER] = {
		.name = "aging_counters_number",
		.help = "number of aging objects",
		.next = NEXT(next_config_attr,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output,
					args.configure.port_attr.nb_aging_objects)),
	},
	[RTE_FLOW_PARSER_CMD_CONFIG_QUOTAS_NUMBER] = {
		.name = "quotas_number",
		.help = "number of quotas",
		.next = NEXT(next_config_attr,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output,
				     args.configure.port_attr.nb_quotas)),
	},
	[RTE_FLOW_PARSER_CMD_CONFIG_METERS_NUMBER] = {
		.name = "meters_number",
		.help = "number of meters",
		.next = NEXT(next_config_attr,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output,
					args.configure.port_attr.nb_meters)),
	},
	[RTE_FLOW_PARSER_CMD_CONFIG_CONN_TRACK_NUMBER] = {
		.name = "conn_tracks_number",
		.help = "number of connection trackings",
		.next = NEXT(next_config_attr,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output,
					args.configure.port_attr.nb_conn_tracks)),
	},
	[RTE_FLOW_PARSER_CMD_CONFIG_FLAGS] = {
		.name = "flags",
		.help = "configuration flags",
		.next = NEXT(next_config_attr,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output,
					args.configure.port_attr.flags)),
	},
	[RTE_FLOW_PARSER_CMD_CONFIG_HOST_PORT] = {
		.name = "host_port",
		.help = "host port for shared objects",
		.next = NEXT(next_config_attr,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output,
					args.configure.port_attr.host_port_id)),
	},
	/* Top-level command. */
	[RTE_FLOW_PARSER_CMD_PATTERN_TEMPLATE] = {
		.name = "pattern_template",
		.type = "{command} {port_id} [{arg} [...]]",
		.help = "manage pattern templates",
		.next = NEXT(next_pt_subcmd, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, port)),
		.call = parse_template,
	},
	/* Sub-level commands. */
	[RTE_FLOW_PARSER_CMD_PATTERN_TEMPLATE_CREATE] = {
		.name = "create",
		.help = "create pattern template",
		.next = NEXT(next_pt_attr),
		.call = parse_template,
	},
	[RTE_FLOW_PARSER_CMD_PATTERN_TEMPLATE_DESTROY] = {
		.name = "destroy",
		.help = "destroy pattern template",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_PATTERN_TEMPLATE_DESTROY_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, port)),
		.call = parse_template_destroy,
	},
	/* Pattern template arguments. */
	[RTE_FLOW_PARSER_CMD_PATTERN_TEMPLATE_CREATE_ID] = {
		.name = "pattern_template_id",
		.help = "specify a pattern template id to create",
		.next = NEXT(next_pt_attr,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_PATTERN_TEMPLATE_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, args.vc.pat_templ_id)),
	},
	[RTE_FLOW_PARSER_CMD_PATTERN_TEMPLATE_DESTROY_ID] = {
		.name = "pattern_template",
		.help = "specify a pattern template id to destroy",
		.next = NEXT(next_pt_destroy_attr,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_PATTERN_TEMPLATE_ID)),
		.args = ARGS(ARGS_ENTRY_PTR(struct rte_flow_parser_output,
					    args.templ_destroy.template_id)),
		.call = parse_template_destroy,
	},
	[RTE_FLOW_PARSER_CMD_PATTERN_TEMPLATE_RELAXED_MATCHING] = {
		.name = "relaxed",
		.help = "is matching relaxed",
		.next = NEXT(next_pt_attr,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_BOOLEAN)),
		.args = ARGS(ARGS_ENTRY_BF(struct rte_flow_parser_output,
			     args.vc.attr.reserved, 1)),
	},
	[RTE_FLOW_PARSER_CMD_PATTERN_TEMPLATE_INGRESS] = {
		.name = "ingress",
		.help = "attribute pattern to ingress",
		.next = NEXT(next_pt_attr),
		.call = parse_template,
	},
	[RTE_FLOW_PARSER_CMD_PATTERN_TEMPLATE_EGRESS] = {
		.name = "egress",
		.help = "attribute pattern to egress",
		.next = NEXT(next_pt_attr),
		.call = parse_template,
	},
	[RTE_FLOW_PARSER_CMD_PATTERN_TEMPLATE_TRANSFER] = {
		.name = "transfer",
		.help = "attribute pattern to transfer",
		.next = NEXT(next_pt_attr),
		.call = parse_template,
	},
	[RTE_FLOW_PARSER_CMD_PATTERN_TEMPLATE_SPEC] = {
		.name = "template",
		.help = "specify item to create pattern template",
		.next = NEXT(next_item),
	},
	/* Top-level command. */
	[RTE_FLOW_PARSER_CMD_ACTIONS_TEMPLATE] = {
		.name = "actions_template",
		.type = "{command} {port_id} [{arg} [...]]",
		.help = "manage actions templates",
		.next = NEXT(next_at_subcmd, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, port)),
		.call = parse_template,
	},
	/* Sub-level commands. */
	[RTE_FLOW_PARSER_CMD_ACTIONS_TEMPLATE_CREATE] = {
		.name = "create",
		.help = "create actions template",
		.next = NEXT(next_at_attr),
		.call = parse_template,
	},
	[RTE_FLOW_PARSER_CMD_ACTIONS_TEMPLATE_DESTROY] = {
		.name = "destroy",
		.help = "destroy actions template",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTIONS_TEMPLATE_DESTROY_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, port)),
		.call = parse_template_destroy,
	},
	/* Actions template arguments. */
	[RTE_FLOW_PARSER_CMD_ACTIONS_TEMPLATE_CREATE_ID] = {
		.name = "actions_template_id",
		.help = "specify an actions template id to create",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTIONS_TEMPLATE_MASK),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTIONS_TEMPLATE_SPEC),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_ACTIONS_TEMPLATE_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, args.vc.act_templ_id)),
	},
	[RTE_FLOW_PARSER_CMD_ACTIONS_TEMPLATE_DESTROY_ID] = {
		.name = "actions_template",
		.help = "specify an actions template id to destroy",
		.next = NEXT(next_at_destroy_attr,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_ACTIONS_TEMPLATE_ID)),
		.args = ARGS(ARGS_ENTRY_PTR(struct rte_flow_parser_output,
					    args.templ_destroy.template_id)),
		.call = parse_template_destroy,
	},
	[RTE_FLOW_PARSER_CMD_ACTIONS_TEMPLATE_INGRESS] = {
		.name = "ingress",
		.help = "attribute actions to ingress",
		.next = NEXT(next_at_attr),
		.call = parse_template,
	},
	[RTE_FLOW_PARSER_CMD_ACTIONS_TEMPLATE_EGRESS] = {
		.name = "egress",
		.help = "attribute actions to egress",
		.next = NEXT(next_at_attr),
		.call = parse_template,
	},
	[RTE_FLOW_PARSER_CMD_ACTIONS_TEMPLATE_TRANSFER] = {
		.name = "transfer",
		.help = "attribute actions to transfer",
		.next = NEXT(next_at_attr),
		.call = parse_template,
	},
	[RTE_FLOW_PARSER_CMD_ACTIONS_TEMPLATE_SPEC] = {
		.name = "template",
		.help = "specify action to create actions template",
		.next = NEXT(next_action),
		.call = parse_template,
	},
	[RTE_FLOW_PARSER_CMD_ACTIONS_TEMPLATE_MASK] = {
		.name = "mask",
		.help = "specify action mask to create actions template",
		.next = NEXT(next_action),
		.call = parse_template,
	},
	/* Top-level command. */
	[RTE_FLOW_PARSER_CMD_TABLE] = {
		.name = "template_table",
		.type = "{command} {port_id} [{arg} [...]]",
		.help = "manage template tables",
		.next = NEXT(next_table_subcmd, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, port)),
		.call = parse_table,
	},
	/* Sub-level commands. */
	[RTE_FLOW_PARSER_CMD_TABLE_CREATE] = {
		.name = "create",
		.help = "create template table",
		.next = NEXT(next_table_attr),
		.call = parse_table,
	},
	[RTE_FLOW_PARSER_CMD_TABLE_DESTROY] = {
		.name = "destroy",
		.help = "destroy template table",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_TABLE_DESTROY_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, port)),
		.call = parse_table_destroy,
	},
	[RTE_FLOW_PARSER_CMD_TABLE_RESIZE] = {
		.name = "resize",
		.help = "resize template table",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_TABLE_RESIZE_ID)),
		.call = parse_table
	},
	[RTE_FLOW_PARSER_CMD_TABLE_RESIZE_COMPLETE] = {
		.name = "resize_complete",
		.help = "complete table resize",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_TABLE_DESTROY_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, port)),
		.call = parse_table_destroy,
	},
	/* Table  arguments. */
	[RTE_FLOW_PARSER_CMD_TABLE_CREATE_ID] = {
		.name = "table_id",
		.help = "specify table id to create",
		.next = NEXT(next_table_attr,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_TABLE_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, args.table.id)),
	},
	[RTE_FLOW_PARSER_CMD_TABLE_DESTROY_ID] = {
		.name = "table",
		.help = "table id",
		.next = NEXT(next_table_destroy_attr,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_TABLE_ID)),
		.args = ARGS(ARGS_ENTRY_PTR(struct rte_flow_parser_output,
					    args.table_destroy.table_id)),
		.call = parse_table_destroy,
	},
	[RTE_FLOW_PARSER_CMD_TABLE_RESIZE_ID] = {
		.name = "table_resize_id",
		.help = "table resize id",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_TABLE_RESIZE_RULES_NUMBER),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_TABLE_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, args.table.id)),
		.call = parse_table
	},
	[RTE_FLOW_PARSER_CMD_TABLE_RESIZE_RULES_NUMBER] = {
		.name = "table_resize_rules_num",
		.help = "table resize rules number",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_END),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output,
					args.table.attr.nb_flows)),
		.call = parse_table
	},
	[RTE_FLOW_PARSER_CMD_TABLE_INSERTION_TYPE] = {
		.name = "insertion_type",
		.help = "specify insertion type",
		.next = NEXT(next_table_attr,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_TABLE_INSERTION_TYPE_NAME)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output,
					args.table.attr.insertion_type)),
	},
	[RTE_FLOW_PARSER_CMD_TABLE_INSERTION_TYPE_NAME] = {
		.name = "insertion_type_name",
		.help = "insertion type name",
		.call = parse_insertion_table_type,
		.comp = comp_insertion_table_type,
	},
	[RTE_FLOW_PARSER_CMD_TABLE_HASH_FUNC] = {
		.name = "hash_func",
		.help = "specify hash calculation function",
		.next = NEXT(next_table_attr,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_TABLE_HASH_FUNC_NAME)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output,
					args.table.attr.hash_func)),
	},
	[RTE_FLOW_PARSER_CMD_TABLE_HASH_FUNC_NAME] = {
		.name = "hash_func_name",
		.help = "hash calculation function name",
		.call = parse_hash_table_type,
		.comp = comp_hash_table_type,
	},
	[RTE_FLOW_PARSER_CMD_TABLE_GROUP] = {
		.name = "group",
		.help = "specify a group",
		.next = NEXT(next_table_attr, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_GROUP_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output,
					args.table.attr.flow_attr.group)),
	},
	[RTE_FLOW_PARSER_CMD_TABLE_PRIORITY] = {
		.name = "priority",
		.help = "specify a priority level",
		.next = NEXT(next_table_attr, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_PRIORITY_LEVEL)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output,
					args.table.attr.flow_attr.priority)),
	},
	[RTE_FLOW_PARSER_CMD_TABLE_EGRESS] = {
		.name = "egress",
		.help = "affect rule to egress",
		.next = NEXT(next_table_attr),
		.call = parse_table,
	},
	[RTE_FLOW_PARSER_CMD_TABLE_INGRESS] = {
		.name = "ingress",
		.help = "affect rule to ingress",
		.next = NEXT(next_table_attr),
		.call = parse_table,
	},
	[RTE_FLOW_PARSER_CMD_TABLE_TRANSFER] = {
		.name = "transfer",
		.help = "affect rule to transfer",
		.next = NEXT(next_table_attr),
		.call = parse_table,
	},
	[RTE_FLOW_PARSER_CMD_TABLE_TRANSFER_WIRE_ORIG] = {
		.name = "wire_orig",
		.help = "affect rule direction to transfer",
		.next = NEXT(next_table_attr),
		.call = parse_table,
	},
	[RTE_FLOW_PARSER_CMD_TABLE_TRANSFER_VPORT_ORIG] = {
		.name = "vport_orig",
		.help = "affect rule direction to transfer",
		.next = NEXT(next_table_attr),
		.call = parse_table,
	},
	[RTE_FLOW_PARSER_CMD_TABLE_RESIZABLE] = {
		.name = "resizable",
		.help = "set resizable attribute",
		.next = NEXT(next_table_attr),
		.call = parse_table,
	},
	[RTE_FLOW_PARSER_CMD_TABLE_RULES_NUMBER] = {
		.name = "rules_number",
		.help = "number of rules in table",
		.next = NEXT(next_table_attr,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output,
					args.table.attr.nb_flows)),
		.call = parse_table,
	},
	[RTE_FLOW_PARSER_CMD_TABLE_PATTERN_TEMPLATE] = {
		.name = "pattern_template",
		.help = "specify pattern template id",
		.next = NEXT(next_table_attr,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_PATTERN_TEMPLATE_ID)),
		.args = ARGS(ARGS_ENTRY_PTR(struct rte_flow_parser_output,
					    args.table.pat_templ_id)),
		.call = parse_table,
	},
	[RTE_FLOW_PARSER_CMD_TABLE_ACTIONS_TEMPLATE] = {
		.name = "actions_template",
		.help = "specify actions template id",
		.next = NEXT(next_table_attr,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_ACTIONS_TEMPLATE_ID)),
		.args = ARGS(ARGS_ENTRY_PTR(struct rte_flow_parser_output,
					    args.table.act_templ_id)),
		.call = parse_table,
	},
	/* Top-level command. */
	[RTE_FLOW_PARSER_CMD_FLOW_GROUP] = {
		.name = "group",
		.help = "manage flow groups",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_GROUP_ID),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, port)),
		.call = parse_group,
	},
	/* Sub-level commands. */
	[RTE_FLOW_PARSER_CMD_GROUP_SET_MISS_ACTIONS] = {
		.name = "set_miss_actions",
		.help = "set group miss actions",
		.next = NEXT(next_action),
		.call = parse_group,
	},
	/* Group arguments */
	[RTE_FLOW_PARSER_CMD_GROUP_ID]	= {
		.name = "group_id",
		.help = "group id",
		.next = NEXT(next_group_attr, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_GROUP_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, args.vc.attr.group)),
	},
	[RTE_FLOW_PARSER_CMD_GROUP_INGRESS] = {
		.name = "ingress",
		.help = "group ingress attr",
		.next = NEXT(next_group_attr),
		.call = parse_group,
	},
	[RTE_FLOW_PARSER_CMD_GROUP_EGRESS] = {
		.name = "egress",
		.help = "group egress attr",
		.next = NEXT(next_group_attr),
		.call = parse_group,
	},
	[RTE_FLOW_PARSER_CMD_GROUP_TRANSFER] = {
		.name = "transfer",
		.help = "group transfer attr",
		.next = NEXT(next_group_attr),
		.call = parse_group,
	},
	/* Top-level command. */
	[RTE_FLOW_PARSER_CMD_QUEUE] = {
		.name = "queue",
		.help = "queue a flow rule operation",
		.next = NEXT(next_queue_subcmd, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, port)),
		.call = parse_qo,
	},
	/* Sub-level commands. */
	[RTE_FLOW_PARSER_CMD_QUEUE_CREATE] = {
		.name = "create",
		.help = "create a flow rule",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_QUEUE_TEMPLATE_TABLE),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_QUEUE_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, queue)),
		.call = parse_qo,
	},
	[RTE_FLOW_PARSER_CMD_QUEUE_DESTROY] = {
		.name = "destroy",
		.help = "destroy a flow rule",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_QUEUE_DESTROY_POSTPONE),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_QUEUE_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, queue)),
		.call = parse_qo_destroy,
	},
	[RTE_FLOW_PARSER_CMD_QUEUE_FLOW_UPDATE_RESIZED] = {
		.name = "update_resized",
		.help = "update a flow after table resize",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_QUEUE_DESTROY_ID),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_QUEUE_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, queue)),
		.call = parse_qo_destroy,
	},
	[RTE_FLOW_PARSER_CMD_QUEUE_UPDATE] = {
		.name = "update",
		.help = "update a flow rule",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_QUEUE_UPDATE_ID),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_QUEUE_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, queue)),
		.call = parse_qo,
	},
	[RTE_FLOW_PARSER_CMD_QUEUE_AGED] = {
		.name = "aged",
		.help = "list and destroy aged flows",
		.next = NEXT(next_aged_attr, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_QUEUE_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, queue)),
		.call = parse_aged,
	},
	[RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION] = {
		.name = "indirect_action",
		.help = "queue indirect actions",
		.next = NEXT(next_qia_subcmd, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_QUEUE_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, queue)),
		.call = parse_qia,
	},
	/* Queue  arguments. */
	[RTE_FLOW_PARSER_CMD_QUEUE_TEMPLATE_TABLE] = {
		.name = "template_table",
		.help = "specify table id",
		.next = NEXT(next_async_insert_subcmd,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_TABLE_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output,
					args.vc.table_id)),
		.call = parse_qo,
	},
	[RTE_FLOW_PARSER_CMD_QUEUE_PATTERN_TEMPLATE] = {
		.name = "pattern_template",
		.help = "specify pattern template index",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_QUEUE_ACTIONS_TEMPLATE),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output,
					args.vc.pat_templ_id)),
		.call = parse_qo,
	},
	[RTE_FLOW_PARSER_CMD_QUEUE_ACTIONS_TEMPLATE] = {
		.name = "actions_template",
		.help = "specify actions template index",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_QUEUE_CREATE_POSTPONE),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output,
					args.vc.act_templ_id)),
		.call = parse_qo,
	},
	[RTE_FLOW_PARSER_CMD_QUEUE_RULE_ID] = {
		.name = "rule_index",
		.help = "specify flow rule index",
		.next = NEXT(next_async_pattern_subcmd,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output,
					args.vc.rule_id)),
		.call = parse_qo,
	},
	[RTE_FLOW_PARSER_CMD_QUEUE_CREATE_POSTPONE] = {
		.name = "postpone",
		.help = "postpone create operation",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ITEM_PATTERN),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_BOOLEAN)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, postpone)),
		.call = parse_qo,
	},
	[RTE_FLOW_PARSER_CMD_QUEUE_DESTROY_POSTPONE] = {
		.name = "postpone",
		.help = "postpone destroy operation",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_QUEUE_DESTROY_ID),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_BOOLEAN)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, postpone)),
		.call = parse_qo_destroy,
	},
	[RTE_FLOW_PARSER_CMD_QUEUE_DESTROY_ID] = {
		.name = "rule",
		.help = "specify rule id to destroy",
		.next = NEXT(next_queue_destroy_attr,
			NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY_PTR(struct rte_flow_parser_output,
					    args.destroy.rule)),
		.call = parse_qo_destroy,
	},
	[RTE_FLOW_PARSER_CMD_QUEUE_UPDATE_ID] = {
		.name = "rule",
		.help = "specify rule id to update",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_QUEUE_ACTIONS_TEMPLATE),
			NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output,
				     args.vc.rule_id)),
		.call = parse_qo,
	},
	/* Queue indirect action arguments */
	[RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_CREATE] = {
		.name = "create",
		.help = "create indirect action",
		.next = NEXT(next_qia_create_attr),
		.call = parse_qia,
	},
	[RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_UPDATE] = {
		.name = "update",
		.help = "update indirect action",
		.next = NEXT(next_qia_update_attr,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_INDIRECT_ACTION_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, args.vc.attr.group)),
		.call = parse_qia,
	},
	[RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_DESTROY] = {
		.name = "destroy",
		.help = "destroy indirect action",
		.next = NEXT(next_qia_destroy_attr),
		.call = parse_qia_destroy,
	},
	[RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_QUERY] = {
		.name = "query",
		.help = "query indirect action",
		.next = NEXT(next_qia_query_attr,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_INDIRECT_ACTION_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, args.ia.action_id)),
		.call = parse_qia,
	},
	/* Indirect action destroy arguments. */
	[RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_DESTROY_POSTPONE] = {
		.name = "postpone",
		.help = "postpone destroy operation",
		.next = NEXT(next_qia_destroy_attr,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_BOOLEAN)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, postpone)),
	},
	[RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_DESTROY_ID] = {
		.name = "action_id",
		.help = "specify a indirect action id to destroy",
		.next = NEXT(next_qia_destroy_attr,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_INDIRECT_ACTION_ID)),
		.args = ARGS(ARGS_ENTRY_PTR(struct rte_flow_parser_output,
					    args.ia_destroy.action_id)),
		.call = parse_qia_destroy,
	},
	[RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_QUERY_UPDATE] = {
		.name = "query_update",
		.help = "indirect query [and|or] update action",
		.next = NEXT(next_qia_qu_attr, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_INDIRECT_ACTION_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, args.ia.action_id)),
		.call = parse_qia
	},
	[RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_QU_MODE] = {
		.name = "mode",
		.help = "indirect query [and|or] update action",
		.next = NEXT(next_qia_qu_attr,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_QU_MODE_NAME)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, args.ia.qu_mode)),
		.call = parse_qia
	},
	/* Indirect action update arguments. */
	[RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_UPDATE_POSTPONE] = {
		.name = "postpone",
		.help = "postpone update operation",
		.next = NEXT(next_qia_update_attr,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_BOOLEAN)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, postpone)),
	},
	/* Indirect action update arguments. */
	[RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_QUERY_POSTPONE] = {
		.name = "postpone",
		.help = "postpone query operation",
		.next = NEXT(next_qia_query_attr,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_BOOLEAN)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, postpone)),
	},
	/* Indirect action create arguments. */
	[RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_CREATE_ID] = {
		.name = "action_id",
		.help = "specify a indirect action id to create",
		.next = NEXT(next_qia_create_attr,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_INDIRECT_ACTION_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, args.vc.attr.group)),
	},
	[RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_INGRESS] = {
		.name = "ingress",
		.help = "affect rule to ingress",
		.next = NEXT(next_qia_create_attr),
		.call = parse_qia,
	},
	[RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_EGRESS] = {
		.name = "egress",
		.help = "affect rule to egress",
		.next = NEXT(next_qia_create_attr),
		.call = parse_qia,
	},
	[RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_TRANSFER] = {
		.name = "transfer",
		.help = "affect rule to transfer",
		.next = NEXT(next_qia_create_attr),
		.call = parse_qia,
	},
	[RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_CREATE_POSTPONE] = {
		.name = "postpone",
		.help = "postpone create operation",
		.next = NEXT(next_qia_create_attr,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_BOOLEAN)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, postpone)),
	},
	[RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_SPEC] = {
		.name = "action",
		.help = "specify action to create indirect handle",
		.next = NEXT(next_action),
	},
	[RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_LIST] = {
		.name = "list",
		.help = "specify actions for indirect handle list",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTIONS, RTE_FLOW_PARSER_CMD_END)),
		.call = parse_qia,
	},
	/* Top-level command. */
	[RTE_FLOW_PARSER_CMD_PUSH] = {
		.name = "push",
		.help = "push enqueued operations",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_PUSH_QUEUE),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, port)),
		.call = parse_push,
	},
	/* Sub-level commands. */
	[RTE_FLOW_PARSER_CMD_PUSH_QUEUE] = {
		.name = "queue",
		.help = "specify queue id",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_END),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_QUEUE_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, queue)),
	},
	/* Top-level command. */
	[RTE_FLOW_PARSER_CMD_PULL] = {
		.name = "pull",
		.help = "pull flow operations results",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_PULL_QUEUE),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, port)),
		.call = parse_pull,
	},
	/* Sub-level commands. */
	[RTE_FLOW_PARSER_CMD_PULL_QUEUE] = {
		.name = "queue",
		.help = "specify queue id",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_END),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_QUEUE_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, queue)),
	},
	/* Top-level command. */
	[RTE_FLOW_PARSER_CMD_HASH] = {
		.name = "hash",
		.help = "calculate hash for a given pattern in a given template table",
		.next = NEXT(next_hash_subcmd, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, port)),
		.call = parse_hash,
	},
	/* Sub-level commands. */
	[RTE_FLOW_PARSER_CMD_HASH_CALC_TABLE] = {
		.name = "template_table",
		.help = "specify table id",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_HASH_CALC_PATTERN_INDEX),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_TABLE_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output,
					args.vc.table_id)),
		.call = parse_hash,
	},
	[RTE_FLOW_PARSER_CMD_HASH_CALC_ENCAP] = {
		.name = "encap",
		.help = "calculates encap hash",
		.next = NEXT(next_hash_encap_dest_subcmd),
		.call = parse_hash,
	},
	[RTE_FLOW_PARSER_CMD_HASH_CALC_PATTERN_INDEX] = {
		.name = "pattern_template",
		.help = "specify pattern template id",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ITEM_PATTERN),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output,
					args.vc.pat_templ_id)),
		.call = parse_hash,
	},
	[RTE_FLOW_PARSER_CMD_ENCAP_HASH_FIELD_SRC_PORT] = {
		.name = "hash_field_sport",
		.help = "the encap hash field is src port",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ITEM_PATTERN)),
		.call = parse_hash,
	},
	[RTE_FLOW_PARSER_CMD_ENCAP_HASH_FIELD_GRE_FLOW_ID] = {
		.name = "hash_field_flow_id",
		.help = "the encap hash field is NVGRE flow id",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ITEM_PATTERN)),
		.call = parse_hash,
	},
	/* Top-level command. */
	[RTE_FLOW_PARSER_CMD_INDIRECT_ACTION] = {
		.name = "indirect_action",
		.type = "{command} {port_id} [{arg} [...]]",
		.help = "manage indirect actions",
		.next = NEXT(next_ia_subcmd, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, port)),
		.call = parse_ia,
	},
	/* Sub-level commands. */
	[RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_CREATE] = {
		.name = "create",
		.help = "create indirect action",
		.next = NEXT(next_ia_create_attr),
		.call = parse_ia,
	},
	[RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_UPDATE] = {
		.name = "update",
		.help = "update indirect action",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_SPEC),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_INDIRECT_ACTION_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, args.vc.attr.group)),
		.call = parse_ia,
	},
	[RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_DESTROY] = {
		.name = "destroy",
		.help = "destroy indirect action",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_DESTROY_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, port)),
		.call = parse_ia_destroy,
	},
	[RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_QUERY] = {
		.name = "query",
		.help = "query indirect action",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_END),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_INDIRECT_ACTION_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, args.ia.action_id)),
		.call = parse_ia,
	},
	[RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_QUERY_UPDATE] = {
		.name = "query_update",
		.help = "query [and|or] update",
		.next = NEXT(next_ia_qu_attr, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_INDIRECT_ACTION_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, args.ia.action_id)),
		.call = parse_ia
	},
	[RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_QU_MODE] = {
		.name = "mode",
		.help = "query_update mode",
		.next = NEXT(next_ia_qu_attr,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_QU_MODE_NAME)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, args.ia.qu_mode)),
		.call = parse_ia,
	},
	[RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_QU_MODE_NAME] = {
		.name = "mode_name",
		.help = "query-update mode name",
		.call = parse_qu_mode_name,
		.comp = comp_qu_mode_name,
	},
	[RTE_FLOW_PARSER_CMD_VALIDATE] = {
		.name = "validate",
		.help = "check whether a flow rule can be created",
		.next = NEXT(next_vc_attr, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, port)),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_CREATE] = {
		.name = "create",
		.help = "create a flow rule",
		.next = NEXT(next_vc_attr, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, port)),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_DESTROY] = {
		.name = "destroy",
		.help = "destroy specific flow rules",
		.next = NEXT(next_destroy_attr,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, port)),
		.call = parse_destroy,
	},
	[RTE_FLOW_PARSER_CMD_UPDATE] = {
		.name = "update",
		.help = "update a flow rule with new actions",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_VC_IS_USER_ID, RTE_FLOW_PARSER_CMD_END),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTIONS),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_RULE_ID),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, args.vc.rule_id),
			     ARGS_ENTRY(struct rte_flow_parser_output, port)),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_FLUSH] = {
		.name = "flush",
		.help = "destroy all flow rules",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, port)),
		.call = parse_flush,
	},
	[RTE_FLOW_PARSER_CMD_DUMP] = {
		.name = "dump",
		.help = "dump single/all flow rules to file",
		.next = NEXT(next_dump_subcmd, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, port)),
		.call = parse_dump,
	},
	[RTE_FLOW_PARSER_CMD_QUERY] = {
		.name = "query",
		.help = "query an existing flow rule",
		.next = NEXT(next_query_attr, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_QUERY_ACTION),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_RULE_ID),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, args.query.action.type),
			     ARGS_ENTRY(struct rte_flow_parser_output, args.query.rule),
			     ARGS_ENTRY(struct rte_flow_parser_output, port)),
		.call = parse_query,
	},
	[RTE_FLOW_PARSER_CMD_LIST] = {
		.name = "list",
		.help = "list existing flow rules",
		.next = NEXT(next_list_attr, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, port)),
		.call = parse_list,
	},
	[RTE_FLOW_PARSER_CMD_AGED] = {
		.name = "aged",
		.help = "list and destroy aged flows",
		.next = NEXT(next_aged_attr, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, port)),
		.call = parse_aged,
	},
	[RTE_FLOW_PARSER_CMD_ISOLATE] = {
		.name = "isolate",
		.help = "restrict ingress traffic to the defined flow rules",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_BOOLEAN),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, args.isolate.set),
			     ARGS_ENTRY(struct rte_flow_parser_output, port)),
		.call = parse_isolate,
	},
	[RTE_FLOW_PARSER_CMD_FLEX] = {
		.name = "flex_item",
		.help = "flex item API",
		.next = NEXT(next_flex_item),
		.call = parse_flex,
	},
	[RTE_FLOW_PARSER_CMD_FLEX_ITEM_CREATE] = {
		.name = "create",
		.help = "flex item create",
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, args.flex.filename),
			     ARGS_ENTRY(struct rte_flow_parser_output, args.flex.token),
			     ARGS_ENTRY(struct rte_flow_parser_output, port)),
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_FILE_PATH),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_FLEX_TOKEN),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_PORT_ID)),
		.call = parse_flex
	},
	[RTE_FLOW_PARSER_CMD_FLEX_ITEM_DESTROY] = {
		.name = "destroy",
		.help = "flex item destroy",
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, args.flex.token),
			     ARGS_ENTRY(struct rte_flow_parser_output, port)),
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_FLEX_TOKEN),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_PORT_ID)),
		.call = parse_flex
	},
	[RTE_FLOW_PARSER_CMD_TUNNEL] = {
		.name = "tunnel",
		.help = "new tunnel API",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_TUNNEL_CREATE,
					RTE_FLOW_PARSER_CMD_TUNNEL_LIST,
					RTE_FLOW_PARSER_CMD_TUNNEL_DESTROY)),
		.call = parse_tunnel,
	},
	/* Tunnel arguments. */
	[RTE_FLOW_PARSER_CMD_TUNNEL_CREATE] = {
		.name = "create",
		.help = "create new tunnel object",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_TUNNEL_CREATE_TYPE),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, port)),
		.call = parse_tunnel,
	},
	[RTE_FLOW_PARSER_CMD_TUNNEL_CREATE_TYPE] = {
		.name = "type",
		.help = "create new tunnel",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_FILE_PATH)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_tunnel_ops, type)),
		.call = parse_tunnel,
	},
	[RTE_FLOW_PARSER_CMD_TUNNEL_DESTROY] = {
		.name = "destroy",
		.help = "destroy tunnel",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_TUNNEL_DESTROY_ID),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, port)),
		.call = parse_tunnel,
	},
	[RTE_FLOW_PARSER_CMD_TUNNEL_DESTROY_ID] = {
		.name = "id",
		.help = "tunnel identifier to destroy",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_tunnel_ops, id)),
		.call = parse_tunnel,
	},
	[RTE_FLOW_PARSER_CMD_TUNNEL_LIST] = {
		.name = "list",
		.help = "list existing tunnels",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, port)),
		.call = parse_tunnel,
	},
	/* Destroy arguments. */
	[RTE_FLOW_PARSER_CMD_DESTROY_RULE] = {
		.name = "rule",
		.help = "specify a rule identifier",
		.next = NEXT(next_destroy_attr, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_RULE_ID)),
		.args = ARGS(ARGS_ENTRY_PTR(struct rte_flow_parser_output, args.destroy.rule)),
		.call = parse_destroy,
	},
	[RTE_FLOW_PARSER_CMD_DESTROY_IS_USER_ID] = {
		.name = "user_id",
		.help = "rule identifier is user-id",
		.next = NEXT(next_destroy_attr),
		.call = parse_destroy,
	},
	/* Dump arguments. */
	[RTE_FLOW_PARSER_CMD_DUMP_ALL] = {
		.name = "all",
		.help = "dump all",
		.next = NEXT(next_dump_attr),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, args.dump.file)),
		.call = parse_dump,
	},
	[RTE_FLOW_PARSER_CMD_DUMP_ONE] = {
		.name = "rule",
		.help = "dump one rule",
		.next = NEXT(next_dump_attr, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_RULE_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, args.dump.file),
				ARGS_ENTRY(struct rte_flow_parser_output, args.dump.rule)),
		.call = parse_dump,
	},
	[RTE_FLOW_PARSER_CMD_DUMP_IS_USER_ID] = {
		.name = "user_id",
		.help = "rule identifier is user-id",
		.next = NEXT(next_dump_subcmd),
		.call = parse_dump,
	},
	/* Query arguments. */
	[RTE_FLOW_PARSER_CMD_QUERY_ACTION] = {
		.name = "{action}",
		.type = "ACTION",
		.help = "action to query, must be part of the rule",
		.call = parse_action,
		.comp = comp_action,
	},
	[RTE_FLOW_PARSER_CMD_QUERY_IS_USER_ID] = {
		.name = "user_id",
		.help = "rule identifier is user-id",
		.next = NEXT(next_query_attr),
		.call = parse_query,
	},
	/* List arguments. */
	[RTE_FLOW_PARSER_CMD_LIST_GROUP] = {
		.name = "group",
		.help = "specify a group",
		.next = NEXT(next_list_attr, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_GROUP_ID)),
		.args = ARGS(ARGS_ENTRY_PTR(struct rte_flow_parser_output, args.list.group)),
		.call = parse_list,
	},
	[RTE_FLOW_PARSER_CMD_AGED_DESTROY] = {
		.name = "destroy",
		.help = "specify aged flows need be destroyed",
		.call = parse_aged,
		.comp = comp_none,
	},
	/* Validate/create attributes. */
	[RTE_FLOW_PARSER_CMD_VC_GROUP] = {
		.name = "group",
		.help = "specify a group",
		.next = NEXT(next_vc_attr, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_GROUP_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_attr, group)),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_VC_PRIORITY] = {
		.name = "priority",
		.help = "specify a priority level",
		.next = NEXT(next_vc_attr, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_PRIORITY_LEVEL)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_attr, priority)),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_VC_INGRESS] = {
		.name = "ingress",
		.help = "affect rule to ingress",
		.next = NEXT(next_vc_attr),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_VC_EGRESS] = {
		.name = "egress",
		.help = "affect rule to egress",
		.next = NEXT(next_vc_attr),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_VC_TRANSFER] = {
		.name = "transfer",
		.help = "apply rule directly to endpoints found in pattern",
		.next = NEXT(next_vc_attr),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_VC_TUNNEL_SET] = {
		.name = "tunnel_set",
		.help = "tunnel steer rule",
		.next = NEXT(next_vc_attr, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_tunnel_ops, id)),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_VC_TUNNEL_MATCH] = {
		.name = "tunnel_match",
		.help = "tunnel match rule",
		.next = NEXT(next_vc_attr, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_tunnel_ops, id)),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_VC_USER_ID] = {
		.name = "user_id",
		.help = "specify a user id to create",
		.next = NEXT(next_vc_attr, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, args.vc.user_id)),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_VC_IS_USER_ID] = {
		.name = "user_id",
		.help = "rule identifier is user-id",
		.call = parse_vc,
	},
	/* Validate/create pattern. */
	[RTE_FLOW_PARSER_CMD_ITEM_PATTERN] = {
		.name = "pattern",
		.help = "submit a list of pattern items",
		.next = NEXT(next_item),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_PARAM_IS] = {
		.name = "is",
		.help = "match value perfectly (with full bit-mask)",
		.call = parse_vc_spec,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_PARAM_SPEC] = {
		.name = "spec",
		.help = "match value according to configured bit-mask",
		.call = parse_vc_spec,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_PARAM_LAST] = {
		.name = "last",
		.help = "specify upper bound to establish a range",
		.call = parse_vc_spec,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_PARAM_MASK] = {
		.name = "mask",
		.help = "specify bit-mask with relevant bits set to one",
		.call = parse_vc_spec,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_PARAM_PREFIX] = {
		.name = "prefix",
		.help = "generate bit-mask from a prefix length",
		.call = parse_vc_spec,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_NEXT] = {
		.name = "/",
		.help = "specify next pattern item",
		.next = NEXT(next_item),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_END] = {
		.name = "end",
		.help = "end list of pattern items",
		.priv = PRIV_ITEM(END, 0),
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTIONS, RTE_FLOW_PARSER_CMD_END)),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_VOID] = {
		.name = "void",
		.help = "no-op pattern item",
		.priv = PRIV_ITEM(VOID, 0),
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ITEM_NEXT)),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_INVERT] = {
		.name = "invert",
		.help = "perform actions when pattern does not match",
		.priv = PRIV_ITEM(INVERT, 0),
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ITEM_NEXT)),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ANY] = {
		.name = "any",
		.help = "match any protocol for the current layer",
		.priv = PRIV_ITEM(ANY, sizeof(struct rte_flow_item_any)),
		.next = NEXT(item_any),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ANY_NUM] = {
		.name = "num",
		.help = "number of layers covered",
		.next = NEXT(item_any, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_any, num)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_PORT_ID] = {
		.name = "port_id",
		.help = "match traffic from/to a given DPDK port ID",
		.priv = PRIV_ITEM(PORT_ID,
				  sizeof(struct rte_flow_item_port_id)),
		.next = NEXT(item_port_id),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_PORT_ID_ID] = {
		.name = "id",
		.help = "DPDK port ID",
		.next = NEXT(item_port_id, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_port_id, id)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_MARK] = {
		.name = "mark",
		.help = "match traffic against value set in previously matched rule",
		.priv = PRIV_ITEM(MARK, sizeof(struct rte_flow_item_mark)),
		.next = NEXT(item_mark),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_MARK_ID] = {
		.name = "id",
		.help = "Integer value to match against",
		.next = NEXT(item_mark, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_mark, id)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_RAW] = {
		.name = "raw",
		.help = "match an arbitrary byte string",
		.priv = PRIV_ITEM(RAW, ITEM_RAW_SIZE),
		.next = NEXT(item_raw),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_RAW_RELATIVE] = {
		.name = "relative",
		.help = "look for pattern after the previous item",
		.next = NEXT(item_raw, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_BOOLEAN), item_param),
		.args = ARGS(ARGS_ENTRY_BF(struct rte_flow_item_raw,
					   relative, 1)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_RAW_SEARCH] = {
		.name = "search",
		.help = "search pattern from offset (see also limit)",
		.next = NEXT(item_raw, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_BOOLEAN), item_param),
		.args = ARGS(ARGS_ENTRY_BF(struct rte_flow_item_raw,
					   search, 1)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_RAW_OFFSET] = {
		.name = "offset",
		.help = "absolute or relative offset for pattern",
		.next = NEXT(item_raw, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_INTEGER), item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_raw, offset)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_RAW_LIMIT] = {
		.name = "limit",
		.help = "search area limit for start of pattern",
		.next = NEXT(item_raw, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_raw, limit)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_RAW_PATTERN] = {
		.name = "pattern",
		.help = "byte string to look for",
		.next = NEXT(item_raw,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_STRING),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ITEM_PARAM_IS,
					RTE_FLOW_PARSER_CMD_ITEM_PARAM_SPEC,
					RTE_FLOW_PARSER_CMD_ITEM_PARAM_MASK)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_raw, pattern),
			     ARGS_ENTRY(struct rte_flow_item_raw, length),
			     ARGS_ENTRY_ARB(sizeof(struct rte_flow_item_raw),
					    ITEM_RAW_PATTERN_SIZE)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_RAW_PATTERN_HEX] = {
		.name = "pattern_hex",
		.help = "hex string to look for",
		.next = NEXT(item_raw,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_HEX),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ITEM_PARAM_IS,
					RTE_FLOW_PARSER_CMD_ITEM_PARAM_SPEC,
					RTE_FLOW_PARSER_CMD_ITEM_PARAM_MASK)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_raw, pattern),
			     ARGS_ENTRY(struct rte_flow_item_raw, length),
			     ARGS_ENTRY_ARB(sizeof(struct rte_flow_item_raw),
					    ITEM_RAW_PATTERN_SIZE)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ETH] = {
		.name = "eth",
		.help = "match Ethernet header",
		.priv = PRIV_ITEM(ETH, sizeof(struct rte_flow_item_eth)),
		.next = NEXT(item_eth),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ETH_DST] = {
		.name = "dst",
		.help = "destination MAC",
		.next = NEXT(item_eth, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_MAC_ADDR), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_eth, hdr.dst_addr)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ETH_SRC] = {
		.name = "src",
		.help = "source MAC",
		.next = NEXT(item_eth, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_MAC_ADDR), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_eth, hdr.src_addr)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ETH_TYPE] = {
		.name = "type",
		.help = "EtherType",
		.next = NEXT(item_eth, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_eth, hdr.ether_type)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ETH_HAS_VLAN] = {
		.name = "has_vlan",
		.help = "packet header contains VLAN",
		.next = NEXT(item_eth, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_BF(struct rte_flow_item_eth,
					   has_vlan, 1)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_VLAN] = {
		.name = "vlan",
		.help = "match 802.1Q/ad VLAN tag",
		.priv = PRIV_ITEM(VLAN, sizeof(struct rte_flow_item_vlan)),
		.next = NEXT(item_vlan),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_VLAN_TCI] = {
		.name = "tci",
		.help = "tag control information",
		.next = NEXT(item_vlan, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_vlan, hdr.vlan_tci)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_VLAN_PCP] = {
		.name = "pcp",
		.help = "priority code point",
		.next = NEXT(item_vlan, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_MASK_HTON(struct rte_flow_item_vlan,
						  hdr.vlan_tci, "\xe0\x00")),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_VLAN_DEI] = {
		.name = "dei",
		.help = "drop eligible indicator",
		.next = NEXT(item_vlan, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_MASK_HTON(struct rte_flow_item_vlan,
						  hdr.vlan_tci, "\x10\x00")),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_VLAN_VID] = {
		.name = "vid",
		.help = "VLAN identifier",
		.next = NEXT(item_vlan, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_MASK_HTON(struct rte_flow_item_vlan,
						  hdr.vlan_tci, "\x0f\xff")),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_VLAN_INNER_TYPE] = {
		.name = "inner_type",
		.help = "inner EtherType",
		.next = NEXT(item_vlan, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_vlan,
					     hdr.eth_proto)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_VLAN_HAS_MORE_VLAN] = {
		.name = "has_more_vlan",
		.help = "packet header contains another VLAN",
		.next = NEXT(item_vlan, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_BF(struct rte_flow_item_vlan,
					   has_more_vlan, 1)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_IPV4] = {
		.name = "ipv4",
		.help = "match IPv4 header",
		.priv = PRIV_ITEM(IPV4, sizeof(struct rte_flow_item_ipv4)),
		.next = NEXT(item_ipv4),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_IPV4_VER_IHL] = {
		.name = "version_ihl",
		.help = "match header length",
		.next = NEXT(item_ipv4, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_ipv4,
				     hdr.version_ihl)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_IPV4_TOS] = {
		.name = "tos",
		.help = "type of service",
		.next = NEXT(item_ipv4, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv4,
					     hdr.type_of_service)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_IPV4_LENGTH] = {
		.name = "length",
		.help = "total length",
		.next = NEXT(item_ipv4, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv4,
					     hdr.total_length)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_IPV4_ID] = {
		.name = "packet_id",
		.help = "fragment packet id",
		.next = NEXT(item_ipv4, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv4,
					     hdr.packet_id)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_IPV4_FRAGMENT_OFFSET] = {
		.name = "fragment_offset",
		.help = "fragmentation flags and fragment offset",
		.next = NEXT(item_ipv4, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv4,
					     hdr.fragment_offset)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_IPV4_TTL] = {
		.name = "ttl",
		.help = "time to live",
		.next = NEXT(item_ipv4, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv4,
					     hdr.time_to_live)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_IPV4_PROTO] = {
		.name = "proto",
		.help = "next protocol ID",
		.next = NEXT(item_ipv4, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv4,
					     hdr.next_proto_id)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_IPV4_SRC] = {
		.name = "src",
		.help = "source address",
		.next = NEXT(item_ipv4, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_IPV4_ADDR),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv4,
					     hdr.src_addr)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_IPV4_DST] = {
		.name = "dst",
		.help = "destination address",
		.next = NEXT(item_ipv4, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_IPV4_ADDR),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv4,
					     hdr.dst_addr)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_IPV6] = {
		.name = "ipv6",
		.help = "match IPv6 header",
		.priv = PRIV_ITEM(IPV6, sizeof(struct rte_flow_item_ipv6)),
		.next = NEXT(item_ipv6),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_IPV6_TC] = {
		.name = "tc",
		.help = "traffic class",
		.next = NEXT(item_ipv6, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_MASK_HTON(struct rte_flow_item_ipv6,
						  hdr.vtc_flow,
						  "\x0f\xf0\x00\x00")),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_IPV6_FLOW] = {
		.name = "flow",
		.help = "flow label",
		.next = NEXT(item_ipv6, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_MASK_HTON(struct rte_flow_item_ipv6,
						  hdr.vtc_flow,
						  "\x00\x0f\xff\xff")),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_IPV6_LEN] = {
		.name = "length",
		.help = "payload length",
		.next = NEXT(item_ipv6, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv6,
					     hdr.payload_len)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_IPV6_PROTO] = {
		.name = "proto",
		.help = "protocol (next header)",
		.next = NEXT(item_ipv6, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv6,
					     hdr.proto)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_IPV6_HOP] = {
		.name = "hop",
		.help = "hop limit",
		.next = NEXT(item_ipv6, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv6,
					     hdr.hop_limits)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_IPV6_SRC] = {
		.name = "src",
		.help = "source address",
		.next = NEXT(item_ipv6, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_IPV6_ADDR),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv6,
					     hdr.src_addr)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_IPV6_DST] = {
		.name = "dst",
		.help = "destination address",
		.next = NEXT(item_ipv6, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_IPV6_ADDR),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv6,
					     hdr.dst_addr)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_IPV6_HAS_FRAG_EXT] = {
		.name = "has_frag_ext",
		.help = "fragment packet attribute",
		.next = NEXT(item_ipv6, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_BF(struct rte_flow_item_ipv6,
					   has_frag_ext, 1)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_IPV6_ROUTING_EXT] = {
		.name = "ipv6_routing_ext",
		.help = "match IPv6 routing extension header",
		.priv = PRIV_ITEM(IPV6_ROUTING_EXT,
				  sizeof(struct rte_flow_item_ipv6_routing_ext)),
		.next = NEXT(item_ipv6_routing_ext),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_IPV6_ROUTING_EXT_TYPE] = {
		.name = "ext_type",
		.help = "match IPv6 routing extension header type",
		.next = NEXT(item_ipv6_routing_ext, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv6_routing_ext,
					     hdr.type)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_IPV6_ROUTING_EXT_NEXT_HDR] = {
		.name = "ext_next_hdr",
		.help = "match IPv6 routing extension header next header type",
		.next = NEXT(item_ipv6_routing_ext, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv6_routing_ext,
					     hdr.next_hdr)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_IPV6_ROUTING_EXT_SEG_LEFT] = {
		.name = "ext_seg_left",
		.help = "match IPv6 routing extension header segment left",
		.next = NEXT(item_ipv6_routing_ext, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv6_routing_ext,
					     hdr.segments_left)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ICMP] = {
		.name = "icmp",
		.help = "match ICMP header",
		.priv = PRIV_ITEM(ICMP, sizeof(struct rte_flow_item_icmp)),
		.next = NEXT(item_icmp),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ICMP_TYPE] = {
		.name = "type",
		.help = "ICMP packet type",
		.next = NEXT(item_icmp, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_icmp,
					     hdr.icmp_type)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ICMP_CODE] = {
		.name = "code",
		.help = "ICMP packet code",
		.next = NEXT(item_icmp, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_icmp,
					     hdr.icmp_code)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ICMP_IDENT] = {
		.name = "ident",
		.help = "ICMP packet identifier",
		.next = NEXT(item_icmp, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_icmp,
					     hdr.icmp_ident)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ICMP_SEQ] = {
		.name = "seq",
		.help = "ICMP packet sequence number",
		.next = NEXT(item_icmp, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_icmp,
					     hdr.icmp_seq_nb)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_UDP] = {
		.name = "udp",
		.help = "match UDP header",
		.priv = PRIV_ITEM(UDP, sizeof(struct rte_flow_item_udp)),
		.next = NEXT(item_udp),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_UDP_SRC] = {
		.name = "src",
		.help = "UDP source port",
		.next = NEXT(item_udp, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_udp,
					     hdr.src_port)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_UDP_DST] = {
		.name = "dst",
		.help = "UDP destination port",
		.next = NEXT(item_udp, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_udp,
					     hdr.dst_port)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_TCP] = {
		.name = "tcp",
		.help = "match TCP header",
		.priv = PRIV_ITEM(TCP, sizeof(struct rte_flow_item_tcp)),
		.next = NEXT(item_tcp),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_TCP_SRC] = {
		.name = "src",
		.help = "TCP source port",
		.next = NEXT(item_tcp, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_tcp,
					     hdr.src_port)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_TCP_DST] = {
		.name = "dst",
		.help = "TCP destination port",
		.next = NEXT(item_tcp, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_tcp,
					     hdr.dst_port)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_TCP_FLAGS] = {
		.name = "flags",
		.help = "TCP flags",
		.next = NEXT(item_tcp, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_tcp,
					     hdr.tcp_flags)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_SCTP] = {
		.name = "sctp",
		.help = "match SCTP header",
		.priv = PRIV_ITEM(SCTP, sizeof(struct rte_flow_item_sctp)),
		.next = NEXT(item_sctp),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_SCTP_SRC] = {
		.name = "src",
		.help = "SCTP source port",
		.next = NEXT(item_sctp, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_sctp,
					     hdr.src_port)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_SCTP_DST] = {
		.name = "dst",
		.help = "SCTP destination port",
		.next = NEXT(item_sctp, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_sctp,
					     hdr.dst_port)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_SCTP_TAG] = {
		.name = "tag",
		.help = "validation tag",
		.next = NEXT(item_sctp, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_sctp,
					     hdr.tag)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_SCTP_CKSUM] = {
		.name = "cksum",
		.help = "checksum",
		.next = NEXT(item_sctp, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_sctp,
					     hdr.cksum)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_VXLAN] = {
		.name = "vxlan",
		.help = "match VXLAN header",
		.priv = PRIV_ITEM(VXLAN, sizeof(struct rte_flow_item_vxlan)),
		.next = NEXT(item_vxlan),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_VXLAN_VNI] = {
		.name = "vni",
		.help = "VXLAN identifier",
		.next = NEXT(item_vxlan, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_vxlan, hdr.vni)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_VXLAN_FLAG_G] = {
		.name = "flag_g",
		.help = "VXLAN GBP bit",
		.next = NEXT(item_vxlan, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_BF(struct rte_flow_item_vxlan,
					   hdr.flag_g, 1)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_VXLAN_FLAG_VER] = {
		.name = "flag_ver",
		.help = "VXLAN GPE version",
		.next = NEXT(item_vxlan, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_BF(struct rte_flow_item_vxlan,
					   hdr.flag_ver, 2)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_VXLAN_FLAG_I] = {
		.name = "flag_i",
		.help = "VXLAN Instance bit",
		.next = NEXT(item_vxlan, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_BF(struct rte_flow_item_vxlan,
					   hdr.flag_i, 1)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_VXLAN_FLAG_P] = {
		.name = "flag_p",
		.help = "VXLAN GPE Next Protocol bit",
		.next = NEXT(item_vxlan, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_BF(struct rte_flow_item_vxlan,
					   hdr.flag_p, 1)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_VXLAN_FLAG_B] = {
		.name = "flag_b",
		.help = "VXLAN GPE Ingress-Replicated BUM",
		.next = NEXT(item_vxlan, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_BF(struct rte_flow_item_vxlan,
					   hdr.flag_b, 1)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_VXLAN_FLAG_O] = {
		.name = "flag_o",
		.help = "VXLAN GPE OAM Packet bit",
		.next = NEXT(item_vxlan, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_BF(struct rte_flow_item_vxlan,
					   hdr.flag_o, 1)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_VXLAN_FLAG_D] = {
		.name = "flag_d",
		.help = "VXLAN GBP Don't Learn bit",
		.next = NEXT(item_vxlan, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_BF(struct rte_flow_item_vxlan,
					   hdr.flag_d, 1)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_VXLAN_FLAG_A] = {
		.name = "flag_a",
		.help = "VXLAN GBP Applied bit",
		.next = NEXT(item_vxlan, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_BF(struct rte_flow_item_vxlan,
					   hdr.flag_a, 1)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_VXLAN_GBP_ID] = {
		.name = "group_policy_id",
		.help = "VXLAN GBP ID",
		.next = NEXT(item_vxlan, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_vxlan,
					     hdr.policy_id)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_VXLAN_GPE_PROTO] = {
		.name = "protocol",
		.help = "VXLAN GPE next protocol",
		.next = NEXT(item_vxlan, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_vxlan,
					     hdr.proto)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_VXLAN_FIRST_RSVD] = {
		.name = "first_rsvd",
		.help = "VXLAN rsvd0 first byte",
		.next = NEXT(item_vxlan, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_vxlan,
					     hdr.rsvd0[0])),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_VXLAN_SECND_RSVD] = {
		.name = "second_rsvd",
		.help = "VXLAN rsvd0 second byte",
		.next = NEXT(item_vxlan, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_vxlan,
					     hdr.rsvd0[1])),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_VXLAN_THIRD_RSVD] = {
		.name = "third_rsvd",
		.help = "VXLAN rsvd0 third byte",
		.next = NEXT(item_vxlan, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_vxlan,
					     hdr.rsvd0[2])),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_VXLAN_LAST_RSVD] = {
		.name = "last_rsvd",
		.help = "VXLAN last reserved byte",
		.next = NEXT(item_vxlan, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_vxlan,
					     hdr.last_rsvd)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_E_TAG] = {
		.name = "e_tag",
		.help = "match E-Tag header",
		.priv = PRIV_ITEM(E_TAG, sizeof(struct rte_flow_item_e_tag)),
		.next = NEXT(item_e_tag),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_E_TAG_GRP_ECID_B] = {
		.name = "grp_ecid_b",
		.help = "GRP and E-CID base",
		.next = NEXT(item_e_tag, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_MASK_HTON(struct rte_flow_item_e_tag,
						  rsvd_grp_ecid_b,
						  "\x3f\xff")),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_NVGRE] = {
		.name = "nvgre",
		.help = "match NVGRE header",
		.priv = PRIV_ITEM(NVGRE, sizeof(struct rte_flow_item_nvgre)),
		.next = NEXT(item_nvgre),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_NVGRE_TNI] = {
		.name = "tni",
		.help = "virtual subnet ID",
		.next = NEXT(item_nvgre, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_nvgre, tni)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_MPLS] = {
		.name = "mpls",
		.help = "match MPLS header",
		.priv = PRIV_ITEM(MPLS, sizeof(struct rte_flow_item_mpls)),
		.next = NEXT(item_mpls),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_MPLS_LABEL] = {
		.name = "label",
		.help = "MPLS label",
		.next = NEXT(item_mpls, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_MASK_HTON(struct rte_flow_item_mpls,
						  label_tc_s,
						  "\xff\xff\xf0")),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_MPLS_TC] = {
		.name = "tc",
		.help = "MPLS Traffic Class",
		.next = NEXT(item_mpls, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_MASK_HTON(struct rte_flow_item_mpls,
						  label_tc_s,
						  "\x00\x00\x0e")),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_MPLS_S] = {
		.name = "s",
		.help = "MPLS Bottom-of-Stack",
		.next = NEXT(item_mpls, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_MASK_HTON(struct rte_flow_item_mpls,
						  label_tc_s,
						  "\x00\x00\x01")),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_MPLS_TTL] = {
		.name = "ttl",
		.help = "MPLS Time-to-Live",
		.next = NEXT(item_mpls, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_mpls, ttl)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_GRE] = {
		.name = "gre",
		.help = "match GRE header",
		.priv = PRIV_ITEM(GRE, sizeof(struct rte_flow_item_gre)),
		.next = NEXT(item_gre),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_GRE_PROTO] = {
		.name = "protocol",
		.help = "GRE protocol type",
		.next = NEXT(item_gre, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_gre,
					     protocol)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_GRE_C_RSVD0_VER] = {
		.name = "c_rsvd0_ver",
		.help =
			"checksum (1b), undefined (1b), key bit (1b),"
			" sequence number (1b), reserved 0 (9b),"
			" version (3b)",
		.next = NEXT(item_gre, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_gre,
					     c_rsvd0_ver)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_GRE_C_BIT] = {
		.name = "c_bit",
		.help = "checksum bit (C)",
		.next = NEXT(item_gre, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_BOOLEAN),
			     item_param),
		.args = ARGS(ARGS_ENTRY_MASK_HTON(struct rte_flow_item_gre,
						  c_rsvd0_ver,
						  "\x80\x00\x00\x00")),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_GRE_S_BIT] = {
		.name = "s_bit",
		.help = "sequence number bit (S)",
		.next = NEXT(item_gre, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_BOOLEAN), item_param),
		.args = ARGS(ARGS_ENTRY_MASK_HTON(struct rte_flow_item_gre,
						  c_rsvd0_ver,
						  "\x10\x00\x00\x00")),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_GRE_K_BIT] = {
		.name = "k_bit",
		.help = "key bit (K)",
		.next = NEXT(item_gre, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_BOOLEAN), item_param),
		.args = ARGS(ARGS_ENTRY_MASK_HTON(struct rte_flow_item_gre,
						  c_rsvd0_ver,
						  "\x20\x00\x00\x00")),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_FUZZY] = {
		.name = "fuzzy",
		.help = "fuzzy pattern match, expect faster than default",
		.priv = PRIV_ITEM(FUZZY,
				sizeof(struct rte_flow_item_fuzzy)),
		.next = NEXT(item_fuzzy),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_FUZZY_THRESH] = {
		.name = "thresh",
		.help = "match accuracy threshold",
		.next = NEXT(item_fuzzy, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_fuzzy,
					thresh)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_GTP] = {
		.name = "gtp",
		.help = "match GTP header",
		.priv = PRIV_ITEM(GTP, sizeof(struct rte_flow_item_gtp)),
		.next = NEXT(item_gtp),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_GTP_FLAGS] = {
		.name = "v_pt_rsv_flags",
		.help = "GTP flags",
		.next = NEXT(item_gtp, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_gtp,
					hdr.gtp_hdr_info)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_GTP_MSG_TYPE] = {
		.name = "msg_type",
		.help = "GTP message type",
		.next = NEXT(item_gtp, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_gtp, hdr.msg_type)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_GTP_TEID] = {
		.name = "teid",
		.help = "tunnel endpoint identifier",
		.next = NEXT(item_gtp, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_gtp, hdr.teid)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_GTPC] = {
		.name = "gtpc",
		.help = "match GTP header",
		.priv = PRIV_ITEM(GTPC, sizeof(struct rte_flow_item_gtp)),
		.next = NEXT(item_gtp),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_GTPU] = {
		.name = "gtpu",
		.help = "match GTP header",
		.priv = PRIV_ITEM(GTPU, sizeof(struct rte_flow_item_gtp)),
		.next = NEXT(item_gtp),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_GENEVE] = {
		.name = "geneve",
		.help = "match GENEVE header",
		.priv = PRIV_ITEM(GENEVE, sizeof(struct rte_flow_item_geneve)),
		.next = NEXT(item_geneve),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_GENEVE_VNI] = {
		.name = "vni",
		.help = "virtual network identifier",
		.next = NEXT(item_geneve, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_geneve, vni)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_GENEVE_PROTO] = {
		.name = "protocol",
		.help = "GENEVE protocol type",
		.next = NEXT(item_geneve, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_geneve,
					     protocol)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_GENEVE_OPTLEN] = {
		.name = "optlen",
		.help = "GENEVE options length in dwords",
		.next = NEXT(item_geneve, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_MASK_HTON(struct rte_flow_item_geneve,
						  ver_opt_len_o_c_rsvd0,
						  "\x3f\x00")),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_VXLAN_GPE] = {
		.name = "vxlan-gpe",
		.help = "match VXLAN-GPE header",
		.priv = PRIV_ITEM(VXLAN_GPE,
				  sizeof(struct rte_flow_item_vxlan_gpe)),
		.next = NEXT(item_vxlan_gpe),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_VXLAN_GPE_VNI] = {
		.name = "vni",
		.help = "VXLAN-GPE identifier",
		.next = NEXT(item_vxlan_gpe, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_vxlan_gpe,
					     hdr.vni)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_VXLAN_GPE_PROTO_IN_DEPRECATED_VXLAN_GPE_HDR] = {
		.name = "protocol",
		.help = "VXLAN-GPE next protocol",
		.next = NEXT(item_vxlan_gpe, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_vxlan_gpe,
					     protocol)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_VXLAN_GPE_FLAGS] = {
		.name = "flags",
		.help = "VXLAN-GPE flags",
		.next = NEXT(item_vxlan_gpe, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_vxlan_gpe,
					     flags)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_VXLAN_GPE_RSVD0] = {
		.name = "rsvd0",
		.help = "VXLAN-GPE rsvd0",
		.next = NEXT(item_vxlan_gpe, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_vxlan_gpe,
					     rsvd0)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_VXLAN_GPE_RSVD1] = {
		.name = "rsvd1",
		.help = "VXLAN-GPE rsvd1",
		.next = NEXT(item_vxlan_gpe, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_vxlan_gpe,
					     rsvd1)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ARP_ETH_IPV4] = {
		.name = "arp_eth_ipv4",
		.help = "match ARP header for Ethernet/IPv4",
		.priv = PRIV_ITEM(ARP_ETH_IPV4,
				  sizeof(struct rte_flow_item_arp_eth_ipv4)),
		.next = NEXT(item_arp_eth_ipv4),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ARP_ETH_IPV4_SHA] = {
		.name = "sha",
		.help = "sender hardware address",
		.next = NEXT(item_arp_eth_ipv4, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_MAC_ADDR),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_arp_eth_ipv4,
					     hdr.arp_data.arp_sha)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ARP_ETH_IPV4_SPA] = {
		.name = "spa",
		.help = "sender IPv4 address",
		.next = NEXT(item_arp_eth_ipv4, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_IPV4_ADDR),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_arp_eth_ipv4,
					     hdr.arp_data.arp_sip)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ARP_ETH_IPV4_THA] = {
		.name = "tha",
		.help = "target hardware address",
		.next = NEXT(item_arp_eth_ipv4, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_MAC_ADDR),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_arp_eth_ipv4,
					     hdr.arp_data.arp_tha)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ARP_ETH_IPV4_TPA] = {
		.name = "tpa",
		.help = "target IPv4 address",
		.next = NEXT(item_arp_eth_ipv4, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_IPV4_ADDR),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_arp_eth_ipv4,
					     hdr.arp_data.arp_tip)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_IPV6_EXT] = {
		.name = "ipv6_ext",
		.help = "match presence of any IPv6 extension header",
		.priv = PRIV_ITEM(IPV6_EXT,
				  sizeof(struct rte_flow_item_ipv6_ext)),
		.next = NEXT(item_ipv6_ext),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_IPV6_EXT_NEXT_HDR] = {
		.name = "next_hdr",
		.help = "next header",
		.next = NEXT(item_ipv6_ext, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv6_ext,
					     next_hdr)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_IPV6_FRAG_EXT] = {
		.name = "ipv6_frag_ext",
		.help = "match presence of IPv6 fragment extension header",
		.priv = PRIV_ITEM(IPV6_FRAG_EXT,
				sizeof(struct rte_flow_item_ipv6_frag_ext)),
		.next = NEXT(item_ipv6_frag_ext),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_IPV6_FRAG_EXT_NEXT_HDR] = {
		.name = "next_hdr",
		.help = "next header",
		.next = NEXT(item_ipv6_frag_ext, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_ipv6_frag_ext,
					hdr.next_header)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_IPV6_FRAG_EXT_FRAG_DATA] = {
		.name = "frag_data",
		.help = "fragment flags and offset",
		.next = NEXT(item_ipv6_frag_ext, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv6_frag_ext,
					     hdr.frag_data)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_IPV6_FRAG_EXT_ID] = {
		.name = "packet_id",
		.help = "fragment packet id",
		.next = NEXT(item_ipv6_frag_ext, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv6_frag_ext,
					     hdr.id)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ICMP6] = {
		.name = "icmp6",
		.help = "match any ICMPv6 header",
		.priv = PRIV_ITEM(ICMP6, sizeof(struct rte_flow_item_icmp6)),
		.next = NEXT(item_icmp6),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ICMP6_TYPE] = {
		.name = "type",
		.help = "ICMPv6 type",
		.next = NEXT(item_icmp6, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_icmp6,
					     type)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ICMP6_CODE] = {
		.name = "code",
		.help = "ICMPv6 code",
		.next = NEXT(item_icmp6, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_icmp6,
					     code)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ICMP6_ECHO_REQUEST] = {
		.name = "icmp6_echo_request",
		.help = "match ICMPv6 echo request",
		.priv = PRIV_ITEM(ICMP6_ECHO_REQUEST,
				  sizeof(struct rte_flow_item_icmp6_echo)),
		.next = NEXT(item_icmp6_echo_request),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ICMP6_ECHO_REQUEST_ID] = {
		.name = "ident",
		.help = "ICMPv6 echo request identifier",
		.next = NEXT(item_icmp6_echo_request, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_icmp6_echo,
					     hdr.identifier)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ICMP6_ECHO_REQUEST_SEQ] = {
		.name = "seq",
		.help = "ICMPv6 echo request sequence",
		.next = NEXT(item_icmp6_echo_request, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_icmp6_echo,
					     hdr.sequence)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ICMP6_ECHO_REPLY] = {
		.name = "icmp6_echo_reply",
		.help = "match ICMPv6 echo reply",
		.priv = PRIV_ITEM(ICMP6_ECHO_REPLY,
				  sizeof(struct rte_flow_item_icmp6_echo)),
		.next = NEXT(item_icmp6_echo_reply),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ICMP6_ECHO_REPLY_ID] = {
		.name = "ident",
		.help = "ICMPv6 echo reply identifier",
		.next = NEXT(item_icmp6_echo_reply, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_icmp6_echo,
					     hdr.identifier)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ICMP6_ECHO_REPLY_SEQ] = {
		.name = "seq",
		.help = "ICMPv6 echo reply sequence",
		.next = NEXT(item_icmp6_echo_reply, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_icmp6_echo,
					     hdr.sequence)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ICMP6_ND_NS] = {
		.name = "icmp6_nd_ns",
		.help = "match ICMPv6 neighbor discovery solicitation",
		.priv = PRIV_ITEM(ICMP6_ND_NS,
				  sizeof(struct rte_flow_item_icmp6_nd_ns)),
		.next = NEXT(item_icmp6_nd_ns),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ICMP6_ND_NS_TARGET_ADDR] = {
		.name = "target_addr",
		.help = "target address",
		.next = NEXT(item_icmp6_nd_ns, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_IPV6_ADDR),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_icmp6_nd_ns,
					     target_addr)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ICMP6_ND_NA] = {
		.name = "icmp6_nd_na",
		.help = "match ICMPv6 neighbor discovery advertisement",
		.priv = PRIV_ITEM(ICMP6_ND_NA,
				  sizeof(struct rte_flow_item_icmp6_nd_na)),
		.next = NEXT(item_icmp6_nd_na),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ICMP6_ND_NA_TARGET_ADDR] = {
		.name = "target_addr",
		.help = "target address",
		.next = NEXT(item_icmp6_nd_na, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_IPV6_ADDR),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_icmp6_nd_na,
					     target_addr)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ICMP6_ND_OPT] = {
		.name = "icmp6_nd_opt",
		.help = "match presence of any ICMPv6 neighbor discovery"
			" option",
		.priv = PRIV_ITEM(ICMP6_ND_OPT,
				  sizeof(struct rte_flow_item_icmp6_nd_opt)),
		.next = NEXT(item_icmp6_nd_opt),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ICMP6_ND_OPT_TYPE] = {
		.name = "type",
		.help = "ND option type",
		.next = NEXT(item_icmp6_nd_opt, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_icmp6_nd_opt,
					     type)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ICMP6_ND_OPT_SLA_ETH] = {
		.name = "icmp6_nd_opt_sla_eth",
		.help = "match ICMPv6 neighbor discovery source Ethernet"
			" link-layer address option",
		.priv = PRIV_ITEM
			(ICMP6_ND_OPT_SLA_ETH,
			 sizeof(struct rte_flow_item_icmp6_nd_opt_sla_eth)),
		.next = NEXT(item_icmp6_nd_opt_sla_eth),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ICMP6_ND_OPT_SLA_ETH_SLA] = {
		.name = "sla",
		.help = "source Ethernet LLA",
		.next = NEXT(item_icmp6_nd_opt_sla_eth,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_MAC_ADDR), item_param),
		.args = ARGS(ARGS_ENTRY_HTON
			     (struct rte_flow_item_icmp6_nd_opt_sla_eth, sla)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ICMP6_ND_OPT_TLA_ETH] = {
		.name = "icmp6_nd_opt_tla_eth",
		.help = "match ICMPv6 neighbor discovery target Ethernet"
			" link-layer address option",
		.priv = PRIV_ITEM
			(ICMP6_ND_OPT_TLA_ETH,
			 sizeof(struct rte_flow_item_icmp6_nd_opt_tla_eth)),
		.next = NEXT(item_icmp6_nd_opt_tla_eth),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ICMP6_ND_OPT_TLA_ETH_TLA] = {
		.name = "tla",
		.help = "target Ethernet LLA",
		.next = NEXT(item_icmp6_nd_opt_tla_eth,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_MAC_ADDR), item_param),
		.args = ARGS(ARGS_ENTRY_HTON
			     (struct rte_flow_item_icmp6_nd_opt_tla_eth, tla)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_META] = {
		.name = "meta",
		.help = "match metadata header",
		.priv = PRIV_ITEM(META, sizeof(struct rte_flow_item_meta)),
		.next = NEXT(item_meta),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_META_DATA] = {
		.name = "data",
		.help = "metadata value",
		.next = NEXT(item_meta, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_MASK(struct rte_flow_item_meta,
					     data, "\xff\xff\xff\xff")),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_RANDOM] = {
		.name = "random",
		.help = "match random value",
		.priv = PRIV_ITEM(RANDOM, sizeof(struct rte_flow_item_random)),
		.next = NEXT(item_random),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_RANDOM_VALUE] = {
		.name = "value",
		.help = "random value",
		.next = NEXT(item_random, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_MASK(struct rte_flow_item_random,
					     value, "\xff\xff\xff\xff")),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_GRE_KEY] = {
		.name = "gre_key",
		.help = "match GRE key",
		.priv = PRIV_ITEM(GRE_KEY, sizeof(rte_be32_t)),
		.next = NEXT(item_gre_key),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_GRE_KEY_VALUE] = {
		.name = "value",
		.help = "key value",
		.next = NEXT(item_gre_key, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARG_ENTRY_HTON(rte_be32_t)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_GRE_OPTION] = {
		.name = "gre_option",
		.help = "match GRE optional fields",
		.priv = PRIV_ITEM(GRE_OPTION,
				  sizeof(struct rte_flow_item_gre_opt)),
		.next = NEXT(item_gre_option),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_GRE_OPTION_CHECKSUM] = {
		.name = "checksum",
		.help = "match GRE checksum",
		.next = NEXT(item_gre_option, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_gre_opt,
					     checksum_rsvd.checksum)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_GRE_OPTION_KEY] = {
		.name = "key",
		.help = "match GRE key",
		.next = NEXT(item_gre_option, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_gre_opt,
					     key.key)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_GRE_OPTION_SEQUENCE] = {
		.name = "sequence",
		.help = "match GRE sequence",
		.next = NEXT(item_gre_option, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_gre_opt,
					     sequence.sequence)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_GTP_PSC] = {
		.name = "gtp_psc",
		.help = "match GTP extension header with type 0x85",
		.priv = PRIV_ITEM(GTP_PSC,
				sizeof(struct rte_flow_item_gtp_psc)),
		.next = NEXT(item_gtp_psc),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_GTP_PSC_QFI] = {
		.name = "qfi",
		.help = "QoS flow identifier",
		.next = NEXT(item_gtp_psc, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_BF(struct rte_flow_item_gtp_psc,
					hdr.qfi, 6)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_GTP_PSC_PDU_T] = {
		.name = "pdu_t",
		.help = "PDU type",
		.next = NEXT(item_gtp_psc, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_BF(struct rte_flow_item_gtp_psc,
					hdr.type, 4)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_PPPOES] = {
		.name = "pppoes",
		.help = "match PPPoE session header",
		.priv = PRIV_ITEM(PPPOES, sizeof(struct rte_flow_item_pppoe)),
		.next = NEXT(item_pppoes),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_PPPOED] = {
		.name = "pppoed",
		.help = "match PPPoE discovery header",
		.priv = PRIV_ITEM(PPPOED, sizeof(struct rte_flow_item_pppoe)),
		.next = NEXT(item_pppoed),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_PPPOE_SEID] = {
		.name = "seid",
		.help = "session identifier",
		.next = NEXT(item_pppoes, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_pppoe,
					session_id)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_PPPOE_PROTO_ID] = {
		.name = "pppoe_proto_id",
		.help = "match PPPoE session protocol identifier",
		.priv = PRIV_ITEM(PPPOE_PROTO_ID,
				sizeof(struct rte_flow_item_pppoe_proto_id)),
		.next = NEXT(item_pppoe_proto_id, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON
			     (struct rte_flow_item_pppoe_proto_id, proto_id)),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_HIGIG2] = {
		.name = "higig2",
		.help = "matches higig2 header",
		.priv = PRIV_ITEM(HIGIG2,
				sizeof(struct rte_flow_item_higig2_hdr)),
		.next = NEXT(item_higig2),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_HIGIG2_CLASSIFICATION] = {
		.name = "classification",
		.help = "matches classification of higig2 header",
		.next = NEXT(item_higig2, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_higig2_hdr,
					hdr.ppt1.classification)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_HIGIG2_VID] = {
		.name = "vid",
		.help = "matches vid of higig2 header",
		.next = NEXT(item_higig2, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_higig2_hdr,
					hdr.ppt1.vid)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_TAG] = {
		.name = "tag",
		.help = "match tag value",
		.priv = PRIV_ITEM(TAG, sizeof(struct rte_flow_item_tag)),
		.next = NEXT(item_tag),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_TAG_DATA] = {
		.name = "data",
		.help = "tag value to match",
		.next = NEXT(item_tag, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_tag, data)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_TAG_INDEX] = {
		.name = "index",
		.help = "index of tag array to match",
		.next = NEXT(item_tag, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ITEM_PARAM_IS)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_tag, index)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_L2TPV3OIP] = {
		.name = "l2tpv3oip",
		.help = "match L2TPv3 over IP header",
		.priv = PRIV_ITEM(L2TPV3OIP,
				  sizeof(struct rte_flow_item_l2tpv3oip)),
		.next = NEXT(item_l2tpv3oip),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_L2TPV3OIP_SESSION_ID] = {
		.name = "session_id",
		.help = "session identifier",
		.next = NEXT(item_l2tpv3oip, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv3oip,
					     session_id)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ESP] = {
		.name = "esp",
		.help = "match ESP header",
		.priv = PRIV_ITEM(ESP, sizeof(struct rte_flow_item_esp)),
		.next = NEXT(item_esp),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ESP_SPI] = {
		.name = "spi",
		.help = "security policy index",
		.next = NEXT(item_esp, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_esp,
				hdr.spi)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_AH] = {
		.name = "ah",
		.help = "match AH header",
		.priv = PRIV_ITEM(AH, sizeof(struct rte_flow_item_ah)),
		.next = NEXT(item_ah),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_AH_SPI] = {
		.name = "spi",
		.help = "security parameters index",
		.next = NEXT(item_ah, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ah, spi)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_PFCP] = {
		.name = "pfcp",
		.help = "match pfcp header",
		.priv = PRIV_ITEM(PFCP, sizeof(struct rte_flow_item_pfcp)),
		.next = NEXT(item_pfcp),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_PFCP_S_FIELD] = {
		.name = "s_field",
		.help = "S field",
		.next = NEXT(item_pfcp, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_pfcp,
				s_field)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_PFCP_SEID] = {
		.name = "seid",
		.help = "session endpoint identifier",
		.next = NEXT(item_pfcp, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_pfcp, seid)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ECPRI] = {
		.name = "ecpri",
		.help = "match eCPRI header",
		.priv = PRIV_ITEM(ECPRI, sizeof(struct rte_flow_item_ecpri)),
		.next = NEXT(item_ecpri),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ECPRI_COMMON] = {
		.name = "common",
		.help = "eCPRI common header",
		.next = NEXT(item_ecpri_common),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ECPRI_COMMON_TYPE] = {
		.name = "type",
		.help = "type of common header",
		.next = NEXT(item_ecpri_common_type),
		.args = ARGS(ARG_ENTRY_HTON(struct rte_flow_item_ecpri)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ECPRI_COMMON_TYPE_IQ_DATA] = {
		.name = "iq_data",
		.help = "Type #0: IQ Data",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ITEM_ECPRI_MSG_IQ_DATA_PCID,
					RTE_FLOW_PARSER_CMD_ITEM_NEXT)),
		.call = parse_vc_item_ecpri_type,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ECPRI_MSG_IQ_DATA_PCID] = {
		.name = "pc_id",
		.help = "Physical Channel ID",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ITEM_ECPRI_MSG_IQ_DATA_PCID,
				RTE_FLOW_PARSER_CMD_ITEM_ECPRI_COMMON, RTE_FLOW_PARSER_CMD_ITEM_NEXT),
				NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ecpri,
				hdr.type0.pc_id)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ECPRI_COMMON_TYPE_RTC_CTRL] = {
		.name = "rtc_ctrl",
		.help = "Type #2: Real-Time Control Data",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ITEM_ECPRI_MSG_RTC_CTRL_RTCID,
					RTE_FLOW_PARSER_CMD_ITEM_NEXT)),
		.call = parse_vc_item_ecpri_type,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ECPRI_MSG_RTC_CTRL_RTCID] = {
		.name = "rtc_id",
		.help = "Real-Time Control Data ID",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ITEM_ECPRI_MSG_RTC_CTRL_RTCID,
				RTE_FLOW_PARSER_CMD_ITEM_ECPRI_COMMON, RTE_FLOW_PARSER_CMD_ITEM_NEXT),
				NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ecpri,
				hdr.type2.rtc_id)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ECPRI_COMMON_TYPE_DLY_MSR] = {
		.name = "delay_measure",
		.help = "Type #5: One-Way Delay Measurement",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ITEM_ECPRI_MSG_DLY_MSR_MSRID,
					RTE_FLOW_PARSER_CMD_ITEM_NEXT)),
		.call = parse_vc_item_ecpri_type,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_ECPRI_MSG_DLY_MSR_MSRID] = {
		.name = "msr_id",
		.help = "Measurement ID",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ITEM_ECPRI_MSG_DLY_MSR_MSRID,
				RTE_FLOW_PARSER_CMD_ITEM_ECPRI_COMMON, RTE_FLOW_PARSER_CMD_ITEM_NEXT),
				NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ecpri,
				hdr.type5.msr_id)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_GENEVE_OPT] = {
		.name = "geneve-opt",
		.help = "GENEVE header option",
		.priv = PRIV_ITEM(GENEVE_OPT,
				  sizeof(struct rte_flow_item_geneve_opt) +
				  ITEM_GENEVE_OPT_DATA_SIZE),
		.next = NEXT(item_geneve_opt),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_GENEVE_OPT_CLASS]	= {
		.name = "class",
		.help = "GENEVE option class",
		.next = NEXT(item_geneve_opt, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_geneve_opt,
					     option_class)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_GENEVE_OPT_TYPE] = {
		.name = "type",
		.help = "GENEVE option type",
		.next = NEXT(item_geneve_opt, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_geneve_opt,
					option_type)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_GENEVE_OPT_LENGTH] = {
		.name = "length",
		.help = "GENEVE option data length (in 32b words)",
		.next = NEXT(item_geneve_opt, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_BOUNDED(
				struct rte_flow_item_geneve_opt, option_len,
				0, 31)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_GENEVE_OPT_DATA] = {
		.name = "data",
		.help = "GENEVE option data pattern",
		.next = NEXT(item_geneve_opt, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_HEX),
			     item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_geneve_opt, data),
			     ARGS_ENTRY_ARB(0, 0),
			     ARGS_ENTRY_ARB
				(sizeof(struct rte_flow_item_geneve_opt),
				ITEM_GENEVE_OPT_DATA_SIZE)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_INTEGRITY] = {
		.name = "integrity",
		.help = "match packet integrity",
		.priv = PRIV_ITEM(INTEGRITY,
				  sizeof(struct rte_flow_item_integrity)),
		.next = NEXT(item_integrity),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_INTEGRITY_LEVEL] = {
		.name = "level",
		.help = "integrity level",
		.next = NEXT(item_integrity_lv, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_integrity, level)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_INTEGRITY_VALUE] = {
		.name = "value",
		.help = "integrity value",
		.next = NEXT(item_integrity_lv, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_integrity, value)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_CONNTRACK] = {
		.name = "conntrack",
		.help = "conntrack state",
		.priv = PRIV_ITEM(CONNTRACK,
				  sizeof(struct rte_flow_item_conntrack)),
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ITEM_NEXT),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_conntrack, flags)),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_PORT_REPRESENTOR] = {
		.name = "port_representor",
		.help = "match traffic entering the embedded switch from the given ethdev",
		.priv = PRIV_ITEM(PORT_REPRESENTOR,
				  sizeof(struct rte_flow_item_ethdev)),
		.next = NEXT(item_port_representor),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_PORT_REPRESENTOR_PORT_ID] = {
		.name = "port_id",
		.help = "ethdev port ID",
		.next = NEXT(item_port_representor, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_ethdev, port_id)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_REPRESENTED_PORT] = {
		.name = "represented_port",
		.help = "match traffic entering the embedded switch from "
			"the entity represented by the given ethdev",
		.priv = PRIV_ITEM(REPRESENTED_PORT,
				  sizeof(struct rte_flow_item_ethdev)),
		.next = NEXT(item_represented_port),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_REPRESENTED_PORT_ETHDEV_PORT_ID] = {
		.name = "ethdev_port_id",
		.help = "ethdev port ID",
		.next = NEXT(item_represented_port, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_ethdev, port_id)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_FLEX] = {
		.name = "flex",
		.help = "match flex header",
		.priv = PRIV_ITEM(FLEX, sizeof(struct rte_flow_item_flex)),
		.next = NEXT(item_flex),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_FLEX_ITEM_HANDLE] = {
		.name = "item",
		.help = "flex item handle",
		.next = NEXT(item_flex, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_FLEX_HANDLE),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ITEM_PARAM_IS)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_flex, handle)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_FLEX_PATTERN_HANDLE] = {
		.name = "pattern",
		.help = "flex pattern handle",
		.next = NEXT(item_flex, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_FLEX_HANDLE),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ITEM_PARAM_IS)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_flex, pattern)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_L2TPV2] = {
		.name = "l2tpv2",
		.help = "match L2TPv2 header",
		.priv = PRIV_ITEM(L2TPV2, sizeof(struct rte_flow_item_l2tpv2)),
		.next = NEXT(item_l2tpv2),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_TYPE] = {
		.name = "type",
		.help = "type of l2tpv2",
		.next = NEXT(item_l2tpv2_type),
		.args = ARGS(ARG_ENTRY_HTON(struct rte_flow_item_l2tpv2)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_TYPE_DATA] = {
		.name = "data",
		.help = "Type #7: data message without any options",
		.next = NEXT(item_l2tpv2_type_data),
		.call = parse_vc_item_l2tpv2_type,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_DATA_TUNNEL_ID] = {
		.name = "tunnel_id",
		.help = "tunnel identifier",
		.next = NEXT(item_l2tpv2_type_data,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type7.tunnel_id)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_DATA_SESSION_ID] = {
		.name = "session_id",
		.help = "session identifier",
		.next = NEXT(item_l2tpv2_type_data,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type7.session_id)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_TYPE_DATA_L] = {
		.name = "data_l",
		.help = "Type #6: data message with length option",
		.next = NEXT(item_l2tpv2_type_data_l),
		.call = parse_vc_item_l2tpv2_type,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_DATA_L_LENGTH] = {
		.name = "length",
		.help = "message length",
		.next = NEXT(item_l2tpv2_type_data_l,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type6.length)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_DATA_L_TUNNEL_ID] = {
		.name = "tunnel_id",
		.help = "tunnel identifier",
		.next = NEXT(item_l2tpv2_type_data_l,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type6.tunnel_id)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_DATA_L_SESSION_ID] = {
		.name = "session_id",
		.help = "session identifier",
		.next = NEXT(item_l2tpv2_type_data_l,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type6.session_id)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_TYPE_DATA_S] = {
		.name = "data_s",
		.help = "Type #5: data message with ns, nr option",
		.next = NEXT(item_l2tpv2_type_data_s),
		.call = parse_vc_item_l2tpv2_type,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_DATA_S_TUNNEL_ID] = {
		.name = "tunnel_id",
		.help = "tunnel identifier",
		.next = NEXT(item_l2tpv2_type_data_s,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type5.tunnel_id)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_DATA_S_SESSION_ID] = {
		.name = "session_id",
		.help = "session identifier",
		.next = NEXT(item_l2tpv2_type_data_s,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type5.session_id)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_DATA_S_NS] = {
		.name = "ns",
		.help = "sequence number for message",
		.next = NEXT(item_l2tpv2_type_data_s,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type5.ns)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_DATA_S_NR] = {
		.name = "nr",
		.help = "sequence number for next receive message",
		.next = NEXT(item_l2tpv2_type_data_s,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type5.nr)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_TYPE_DATA_O] = {
		.name = "data_o",
		.help = "Type #4: data message with offset option",
		.next = NEXT(item_l2tpv2_type_data_o),
		.call = parse_vc_item_l2tpv2_type,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_DATA_O_TUNNEL_ID] = {
		.name = "tunnel_id",
		.help = "tunnel identifier",
		.next = NEXT(item_l2tpv2_type_data_o,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type4.tunnel_id)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_DATA_O_SESSION_ID] = {
		.name = "session_id",
		.help = "session identifier",
		.next = NEXT(item_l2tpv2_type_data_o,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type5.session_id)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_DATA_O_OFFSET] = {
		.name = "offset_size",
		.help = "the size of offset padding",
		.next = NEXT(item_l2tpv2_type_data_o,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type4.offset_size)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_TYPE_DATA_L_S] = {
		.name = "data_l_s",
		.help = "Type #3: data message contains length, ns, nr "
			"options",
		.next = NEXT(item_l2tpv2_type_data_l_s),
		.call = parse_vc_item_l2tpv2_type,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_DATA_L_S_LENGTH] = {
		.name = "length",
		.help = "message length",
		.next = NEXT(item_l2tpv2_type_data_l_s,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type3.length)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_DATA_L_S_TUNNEL_ID] = {
		.name = "tunnel_id",
		.help = "tunnel identifier",
		.next = NEXT(item_l2tpv2_type_data_l_s,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type3.tunnel_id)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_DATA_L_S_SESSION_ID] = {
		.name = "session_id",
		.help = "session identifier",
		.next = NEXT(item_l2tpv2_type_data_l_s,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type3.session_id)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_DATA_L_S_NS] = {
		.name = "ns",
		.help = "sequence number for message",
		.next = NEXT(item_l2tpv2_type_data_l_s,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type3.ns)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_DATA_L_S_NR] = {
		.name = "nr",
		.help = "sequence number for next receive message",
		.next = NEXT(item_l2tpv2_type_data_l_s,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type3.nr)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_TYPE_CTRL] = {
		.name = "control",
		.help = "Type #3: conrtol message contains length, ns, nr "
			"options",
		.next = NEXT(item_l2tpv2_type_ctrl),
		.call = parse_vc_item_l2tpv2_type,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_CTRL_LENGTH] = {
		.name = "length",
		.help = "message length",
		.next = NEXT(item_l2tpv2_type_ctrl,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type3.length)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_CTRL_TUNNEL_ID] = {
		.name = "tunnel_id",
		.help = "tunnel identifier",
		.next = NEXT(item_l2tpv2_type_ctrl,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type3.tunnel_id)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_CTRL_SESSION_ID] = {
		.name = "session_id",
		.help = "session identifier",
		.next = NEXT(item_l2tpv2_type_ctrl,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type3.session_id)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_CTRL_NS] = {
		.name = "ns",
		.help = "sequence number for message",
		.next = NEXT(item_l2tpv2_type_ctrl,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type3.ns)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_MSG_CTRL_NR] = {
		.name = "nr",
		.help = "sequence number for next receive message",
		.next = NEXT(item_l2tpv2_type_ctrl,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type3.nr)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_PPP] = {
		.name = "ppp",
		.help = "match PPP header",
		.priv = PRIV_ITEM(PPP, sizeof(struct rte_flow_item_ppp)),
		.next = NEXT(item_ppp),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_PPP_ADDR] = {
		.name = "addr",
		.help = "PPP address",
		.next = NEXT(item_ppp, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_ppp, hdr.addr)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_PPP_CTRL] = {
		.name = "ctrl",
		.help = "PPP control",
		.next = NEXT(item_ppp, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_ppp, hdr.ctrl)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_PPP_PROTO_ID] = {
		.name = "proto_id",
		.help = "PPP protocol identifier",
		.next = NEXT(item_ppp, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_ppp,
					hdr.proto_id)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_METER] = {
		.name = "meter",
		.help = "match meter color",
		.priv = PRIV_ITEM(METER_COLOR,
				  sizeof(struct rte_flow_item_meter_color)),
		.next = NEXT(item_meter),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_METER_COLOR] = {
		.name = "color",
		.help = "meter color",
		.next = NEXT(item_meter,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_METER_COLOR_NAME),
			     item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_meter_color,
					color)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_QUOTA] = {
		.name = "quota",
		.help = "match quota",
		.priv = PRIV_ITEM(QUOTA, sizeof(struct rte_flow_item_quota)),
		.next = NEXT(item_quota),
		.call = parse_vc
	},
	[RTE_FLOW_PARSER_CMD_ITEM_QUOTA_STATE] = {
		.name = "quota_state",
		.help = "quota state",
		.next = NEXT(item_quota, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ITEM_QUOTA_STATE_NAME),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ITEM_PARAM_SPEC, RTE_FLOW_PARSER_CMD_ITEM_PARAM_MASK)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_quota, state))
	},
	[RTE_FLOW_PARSER_CMD_ITEM_QUOTA_STATE_NAME] = {
		.name = "state_name",
		.help = "quota state name",
		.call = parse_quota_state_name,
		.comp = comp_quota_state_name
	},
	[RTE_FLOW_PARSER_CMD_ITEM_IB_BTH] = {
		.name = "ib_bth",
		.help = "match ib bth fields",
		.priv = PRIV_ITEM(IB_BTH,
				  sizeof(struct rte_flow_item_ib_bth)),
		.next = NEXT(item_ib_bth),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_IB_BTH_OPCODE] = {
		.name = "opcode",
		.help = "match ib bth opcode",
		.next = NEXT(item_ib_bth, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
				 item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ib_bth,
						 hdr.opcode)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_IB_BTH_PKEY] = {
		.name = "pkey",
		.help = "partition key",
		.next = NEXT(item_ib_bth, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
				 item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ib_bth,
						 hdr.pkey)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_IB_BTH_DST_QPN] = {
		.name = "dst_qp",
		.help = "destination qp",
		.next = NEXT(item_ib_bth, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
				 item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ib_bth,
						 hdr.dst_qp)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_IB_BTH_PSN] = {
		.name = "psn",
		.help = "packet sequence number",
		.next = NEXT(item_ib_bth, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
				 item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ib_bth,
						 hdr.psn)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_PTYPE] = {
		.name = "ptype",
		.help = "match L2/L3/L4 and tunnel information",
		.priv = PRIV_ITEM(PTYPE,
				  sizeof(struct rte_flow_item_ptype)),
		.next = NEXT(item_ptype),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_PTYPE_VALUE] = {
		.name = "packet_type",
		.help = "packet type as defined in rte_mbuf_ptype",
		.next = NEXT(item_ptype, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_ptype, packet_type)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_NSH] = {
		.name = "nsh",
		.help = "match NSH header",
		.priv = PRIV_ITEM(NSH,
				  sizeof(struct rte_flow_item_nsh)),
		.next = NEXT(item_nsh),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_COMPARE] = {
		.name = "compare",
		.help = "match with the comparison result",
		.priv = PRIV_ITEM(COMPARE, sizeof(struct rte_flow_item_compare)),
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ITEM_COMPARE_OP)),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_COMPARE_OP] = {
		.name = "op",
		.help = "operation type",
		.next = NEXT(item_compare_field,
			NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ITEM_COMPARE_OP_VALUE), item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_compare, operation)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_COMPARE_OP_VALUE] = {
		.name = "{operation}",
		.help = "operation type value",
		.call = parse_vc_compare_op,
		.comp = comp_set_compare_op,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_A_TYPE] = {
		.name = "a_type",
		.help = "compared field type",
		.next = NEXT(compare_field_a,
			NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_A_TYPE_VALUE), item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_compare, a.field)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_A_TYPE_VALUE] = {
		.name = "{a_type}",
		.help = "compared field type value",
		.call = parse_vc_compare_field_id,
		.comp = comp_set_compare_field_id,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_A_LEVEL] = {
		.name = "a_level",
		.help = "compared field level",
		.next = NEXT(compare_field_a,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_A_LEVEL_VALUE), item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_compare, a.level)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_A_LEVEL_VALUE] = {
		.name = "{a_level}",
		.help = "compared field level value",
		.call = parse_vc_compare_field_level,
		.comp = comp_none,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_A_TAG_INDEX] = {
		.name = "a_tag_index",
		.help = "compared field tag array",
		.next = NEXT(compare_field_a,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_compare,
					a.tag_index)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_A_TYPE_ID] = {
		.name = "a_type_id",
		.help = "compared field type ID",
		.next = NEXT(compare_field_a,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_compare,
					a.type)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_A_CLASS_ID] = {
		.name = "a_class",
		.help = "compared field class ID",
		.next = NEXT(compare_field_a,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_compare,
					     a.class_id)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_A_OFFSET] = {
		.name = "a_offset",
		.help = "compared field bit offset",
		.next = NEXT(compare_field_a,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_compare,
					a.offset)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_B_TYPE] = {
		.name = "b_type",
		.help = "comparator field type",
		.next = NEXT(compare_field_b,
			NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_B_TYPE_VALUE), item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_compare,
					b.field)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_B_TYPE_VALUE] = {
		.name = "{b_type}",
		.help = "comparator field type value",
		.call = parse_vc_compare_field_id,
		.comp = comp_set_compare_field_id,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_B_LEVEL] = {
		.name = "b_level",
		.help = "comparator field level",
		.next = NEXT(compare_field_b,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_B_LEVEL_VALUE), item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_compare,
					b.level)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_B_LEVEL_VALUE] = {
		.name = "{b_level}",
		.help = "comparator field level value",
		.call = parse_vc_compare_field_level,
		.comp = comp_none,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_B_TAG_INDEX] = {
		.name = "b_tag_index",
		.help = "comparator field tag array",
		.next = NEXT(compare_field_b,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_compare,
					b.tag_index)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_B_TYPE_ID] = {
		.name = "b_type_id",
		.help = "comparator field type ID",
		.next = NEXT(compare_field_b,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_compare,
					b.type)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_B_CLASS_ID] = {
		.name = "b_class",
		.help = "comparator field class ID",
		.next = NEXT(compare_field_b,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_compare,
					     b.class_id)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_B_OFFSET] = {
		.name = "b_offset",
		.help = "comparator field bit offset",
		.next = NEXT(compare_field_b,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_compare,
					b.offset)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_B_VALUE] = {
		.name = "b_value",
		.help = "comparator immediate value",
		.next = NEXT(compare_field_b,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_HEX), item_param),
		.args = ARGS(ARGS_ENTRY_ARB(0, 0),
			     ARGS_ENTRY_ARB(0, 0),
			     ARGS_ENTRY(struct rte_flow_item_compare,
					b.value)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_B_POINTER] = {
		.name = "b_ptr",
		.help = "pointer to comparator immediate value",
		.next = NEXT(compare_field_b,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_HEX), item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_compare,
					b.pvalue),
			     ARGS_ENTRY_ARB(0, 0),
			     ARGS_ENTRY_ARB
				(sizeof(struct rte_flow_item_compare),
				 FLOW_FIELD_PATTERN_SIZE)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_WIDTH] = {
		.name = "width",
		.help = "number of bits to compare",
		.next = NEXT(item_compare_field,
			NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_compare,
					width)),
	},

	/* Validate/create actions. */
	[RTE_FLOW_PARSER_CMD_ACTIONS] = {
		.name = "actions",
		.help = "submit a list of associated actions",
		.next = NEXT(next_action),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_NEXT] = {
		.name = "/",
		.help = "specify next action",
		.next = NEXT(next_action),
	},
	[RTE_FLOW_PARSER_CMD_ACTION_END] = {
		.name = "end",
		.help = "end list of actions",
		.priv = PRIV_ACTION(END, 0),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_VOID] = {
		.name = "void",
		.help = "no-op action",
		.priv = PRIV_ACTION(VOID, 0),
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_NEXT)),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_PASSTHRU] = {
		.name = "passthru",
		.help = "let subsequent rule process matched packets",
		.priv = PRIV_ACTION(PASSTHRU, 0),
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_NEXT)),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_SKIP_CMAN] = {
		.name = "skip_cman",
		.help = "bypass cman on received packets",
		.priv = PRIV_ACTION(SKIP_CMAN, 0),
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_NEXT)),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_JUMP] = {
		.name = "jump",
		.help = "redirect traffic to a given group",
		.priv = PRIV_ACTION(JUMP, sizeof(struct rte_flow_action_jump)),
		.next = NEXT(action_jump),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_JUMP_GROUP] = {
		.name = "group",
		.help = "group to redirect traffic to",
		.next = NEXT(action_jump, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_jump, group)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_MARK] = {
		.name = "mark",
		.help = "attach 32 bit value to packets",
		.priv = PRIV_ACTION(MARK, sizeof(struct rte_flow_action_mark)),
		.next = NEXT(action_mark),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_MARK_ID] = {
		.name = "id",
		.help = "32 bit value to return with packets",
		.next = NEXT(action_mark, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_mark, id)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_FLAG] = {
		.name = "flag",
		.help = "flag packets",
		.priv = PRIV_ACTION(FLAG, 0),
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_NEXT)),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_QUEUE] = {
		.name = "queue",
		.help = "assign packets to a given queue index",
		.priv = PRIV_ACTION(QUEUE,
				    sizeof(struct rte_flow_action_queue)),
		.next = NEXT(action_queue),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_QUEUE_INDEX] = {
		.name = "index",
		.help = "queue index to use",
		.next = NEXT(action_queue, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_queue, index)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_DROP] = {
		.name = "drop",
		.help = "drop packets (note: passthru has priority)",
		.priv = PRIV_ACTION(DROP, 0),
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_NEXT)),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_COUNT] = {
		.name = "count",
		.help = "enable counters for this rule",
		.priv = PRIV_ACTION(COUNT,
				    sizeof(struct rte_flow_action_count)),
		.next = NEXT(action_count),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_COUNT_ID] = {
		.name = "identifier",
		.help = "counter identifier to use",
		.next = NEXT(action_count, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_count, id)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_RSS] = {
		.name = "rss",
		.help = "spread packets among several queues",
		.priv = PRIV_ACTION(RSS, sizeof(struct action_rss_data)),
		.next = NEXT(action_rss),
		.call = parse_vc_action_rss,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_RSS_FUNC] = {
		.name = "func",
		.help = "RSS hash function to apply",
		.next = NEXT(action_rss,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_RSS_FUNC_DEFAULT,
					RTE_FLOW_PARSER_CMD_ACTION_RSS_FUNC_TOEPLITZ,
					RTE_FLOW_PARSER_CMD_ACTION_RSS_FUNC_SIMPLE_XOR,
					RTE_FLOW_PARSER_CMD_ACTION_RSS_FUNC_SYMMETRIC_TOEPLITZ)),
	},
	[RTE_FLOW_PARSER_CMD_ACTION_RSS_FUNC_DEFAULT] = {
		.name = "default",
		.help = "default hash function",
		.call = parse_vc_action_rss_func,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_RSS_FUNC_TOEPLITZ] = {
		.name = "toeplitz",
		.help = "Toeplitz hash function",
		.call = parse_vc_action_rss_func,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_RSS_FUNC_SIMPLE_XOR] = {
		.name = "simple_xor",
		.help = "simple XOR hash function",
		.call = parse_vc_action_rss_func,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_RSS_FUNC_SYMMETRIC_TOEPLITZ] = {
		.name = "symmetric_toeplitz",
		.help = "Symmetric Toeplitz hash function",
		.call = parse_vc_action_rss_func,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_RSS_LEVEL] = {
		.name = "level",
		.help = "encapsulation level for \"types\"",
		.next = NEXT(action_rss, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY_ARB
			     (offsetof(struct action_rss_data, conf) +
			      offsetof(struct rte_flow_action_rss, level),
			      sizeof(((struct rte_flow_action_rss *)0)->
				     level))),
	},
	[RTE_FLOW_PARSER_CMD_ACTION_RSS_TYPES] = {
		.name = "types",
		.help = "specific RSS hash types",
		.next = NEXT(action_rss, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_RSS_TYPE)),
	},
	[RTE_FLOW_PARSER_CMD_ACTION_RSS_TYPE] = {
		.name = "{type}",
		.help = "RSS hash type",
		.call = parse_vc_action_rss_type,
		.comp = comp_vc_action_rss_type,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_RSS_KEY] = {
		.name = "key",
		.help = "RSS hash key",
		.next = NEXT(action_rss, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_HEX)),
		.args = ARGS(ARGS_ENTRY_ARB
			     (offsetof(struct action_rss_data, conf) +
			      offsetof(struct rte_flow_action_rss, key),
			      sizeof(((struct rte_flow_action_rss *)0)->key)),
			     ARGS_ENTRY_ARB
			     (offsetof(struct action_rss_data, conf) +
			      offsetof(struct rte_flow_action_rss, key_len),
			      sizeof(((struct rte_flow_action_rss *)0)->
				     key_len)),
			     ARGS_ENTRY(struct action_rss_data, key)),
	},
	[RTE_FLOW_PARSER_CMD_ACTION_RSS_KEY_LEN] = {
		.name = "key_len",
		.help = "RSS hash key length in bytes",
		.next = NEXT(action_rss, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY_ARB_BOUNDED
			     (offsetof(struct action_rss_data, conf) +
			      offsetof(struct rte_flow_action_rss, key_len),
			      sizeof(((struct rte_flow_action_rss *)0)->
				     key_len),
			      0,
			      RSS_HASH_KEY_LENGTH)),
	},
	[RTE_FLOW_PARSER_CMD_ACTION_RSS_QUEUES] = {
		.name = "queues",
		.help = "queue indices to use",
		.next = NEXT(action_rss, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_RSS_QUEUE)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_RSS_QUEUE] = {
		.name = "{queue}",
		.help = "queue index",
		.call = parse_vc_action_rss_queue,
		.comp = comp_vc_action_rss_queue,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_PF] = {
		.name = "pf",
		.help = "direct traffic to physical function",
		.priv = PRIV_ACTION(PF, 0),
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_NEXT)),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_VF] = {
		.name = "vf",
		.help = "direct traffic to a virtual function ID",
		.priv = PRIV_ACTION(VF, sizeof(struct rte_flow_action_vf)),
		.next = NEXT(action_vf),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_VF_ORIGINAL] = {
		.name = "original",
		.help = "use original VF ID if possible",
		.next = NEXT(action_vf, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_BOOLEAN)),
		.args = ARGS(ARGS_ENTRY_BF(struct rte_flow_action_vf,
					   original, 1)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_VF_ID] = {
		.name = "id",
		.help = "VF ID",
		.next = NEXT(action_vf, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_vf, id)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_PORT_ID] = {
		.name = "port_id",
		.help = "direct matching traffic to a given DPDK port ID",
		.priv = PRIV_ACTION(PORT_ID,
				    sizeof(struct rte_flow_action_port_id)),
		.next = NEXT(action_port_id),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_PORT_ID_ORIGINAL] = {
		.name = "original",
		.help = "use original DPDK port ID if possible",
		.next = NEXT(action_port_id, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_BOOLEAN)),
		.args = ARGS(ARGS_ENTRY_BF(struct rte_flow_action_port_id,
					   original, 1)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_PORT_ID_ID] = {
		.name = "id",
		.help = "DPDK port ID",
		.next = NEXT(action_port_id, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_port_id, id)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_METER] = {
		.name = "meter",
		.help = "meter the directed packets at given id",
		.priv = PRIV_ACTION(METER,
				    sizeof(struct rte_flow_action_meter)),
		.next = NEXT(action_meter),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_METER_COLOR] = {
		.name = "color",
		.help = "meter color for the packets",
		.priv = PRIV_ACTION(METER_COLOR,
				sizeof(struct rte_flow_action_meter_color)),
		.next = NEXT(action_meter_color),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_METER_COLOR_TYPE] = {
		.name = "type",
		.help = "specific meter color",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_NEXT),
				NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_METER_COLOR_GREEN,
					RTE_FLOW_PARSER_CMD_ACTION_METER_COLOR_YELLOW,
					RTE_FLOW_PARSER_CMD_ACTION_METER_COLOR_RED)),
	},
	[RTE_FLOW_PARSER_CMD_ACTION_METER_COLOR_GREEN] = {
		.name = "green",
		.help = "meter color green",
		.call = parse_vc_action_meter_color_type,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_METER_COLOR_YELLOW] = {
		.name = "yellow",
		.help = "meter color yellow",
		.call = parse_vc_action_meter_color_type,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_METER_COLOR_RED] = {
		.name = "red",
		.help = "meter color red",
		.call = parse_vc_action_meter_color_type,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_METER_ID] = {
		.name = "mtr_id",
		.help = "meter id to use",
		.next = NEXT(action_meter, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_meter, mtr_id)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_METER_MARK] = {
		.name = "meter_mark",
		.help = "meter the directed packets using profile and policy",
		.priv = PRIV_ACTION(METER_MARK,
				    sizeof(struct rte_flow_action_meter_mark)),
		.next = NEXT(action_meter_mark),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_METER_MARK_CONF] = {
		.name = "meter_mark_conf",
		.help = "meter mark configuration",
		.priv = PRIV_ACTION(METER_MARK,
				    sizeof(struct rte_flow_action_meter_mark)),
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_METER_MARK_CONF_COLOR)),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_METER_MARK_CONF_COLOR] = {
		.name = "mtr_update_init_color",
		.help = "meter update init color",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_NEXT),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_METER_COLOR_NAME)),
		.args = ARGS(ARGS_ENTRY
			     (struct rte_flow_indirect_update_flow_meter_mark,
			      init_color)),
	},
	[RTE_FLOW_PARSER_CMD_ACTION_METER_PROFILE] = {
		.name = "mtr_profile",
		.help = "meter profile id to use",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_METER_PROFILE_ID2PTR)),
		.args = ARGS(ARGS_ENTRY_ARB(0, sizeof(uint32_t))),
	},
	[RTE_FLOW_PARSER_CMD_ACTION_METER_PROFILE_ID2PTR] = {
		.name = "{mtr_profile_id}",
		.type = "PROFILE_ID",
		.help = "meter profile id",
		.next = NEXT(action_meter_mark),
		.call = parse_meter_profile_id2ptr,
		.comp = comp_none,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_METER_POLICY] = {
		.name = "mtr_policy",
		.help = "meter policy id to use",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_METER_POLICY_ID2PTR)),
		ARGS(ARGS_ENTRY_ARB(0, sizeof(uint32_t))),
	},
	[RTE_FLOW_PARSER_CMD_ACTION_METER_POLICY_ID2PTR] = {
		.name = "{mtr_policy_id}",
		.type = "POLICY_ID",
		.help = "meter policy id",
		.next = NEXT(action_meter_mark),
		.call = parse_meter_policy_id2ptr,
		.comp = comp_none,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_METER_COLOR_MODE] = {
		.name = "mtr_color_mode",
		.help = "meter color awareness mode",
		.next = NEXT(action_meter_mark, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_meter_mark, color_mode)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_METER_STATE] = {
		.name = "mtr_state",
		.help = "meter state",
		.next = NEXT(action_meter_mark, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_meter_mark, state)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_OF_DEC_NW_TTL] = {
		.name = "of_dec_nw_ttl",
		.help = "OpenFlow's OFPAT_DEC_NW_TTL",
		.priv = PRIV_ACTION(OF_DEC_NW_TTL, 0),
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_NEXT)),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_OF_POP_VLAN] = {
		.name = "of_pop_vlan",
		.help = "OpenFlow's OFPAT_POP_VLAN",
		.priv = PRIV_ACTION(OF_POP_VLAN, 0),
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_NEXT)),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_OF_PUSH_VLAN] = {
		.name = "of_push_vlan",
		.help = "OpenFlow's OFPAT_PUSH_VLAN",
		.priv = PRIV_ACTION
			(OF_PUSH_VLAN,
			 sizeof(struct rte_flow_action_of_push_vlan)),
		.next = NEXT(action_of_push_vlan),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_OF_PUSH_VLAN_ETHERTYPE] = {
		.name = "ethertype",
		.help = "EtherType",
		.next = NEXT(action_of_push_vlan, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY_HTON
			     (struct rte_flow_action_of_push_vlan,
			      ethertype)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_OF_SET_VLAN_VID] = {
		.name = "of_set_vlan_vid",
		.help = "OpenFlow's OFPAT_SET_VLAN_VID",
		.priv = PRIV_ACTION
			(OF_SET_VLAN_VID,
			 sizeof(struct rte_flow_action_of_set_vlan_vid)),
		.next = NEXT(action_of_set_vlan_vid),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_OF_SET_VLAN_VID_VLAN_VID] = {
		.name = "vlan_vid",
		.help = "VLAN id",
		.next = NEXT(action_of_set_vlan_vid,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY_HTON
			     (struct rte_flow_action_of_set_vlan_vid,
			      vlan_vid)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_OF_SET_VLAN_PCP] = {
		.name = "of_set_vlan_pcp",
		.help = "OpenFlow's OFPAT_SET_VLAN_PCP",
		.priv = PRIV_ACTION
			(OF_SET_VLAN_PCP,
			 sizeof(struct rte_flow_action_of_set_vlan_pcp)),
		.next = NEXT(action_of_set_vlan_pcp),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_OF_SET_VLAN_PCP_VLAN_PCP] = {
		.name = "vlan_pcp",
		.help = "VLAN priority",
		.next = NEXT(action_of_set_vlan_pcp,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY_HTON
			     (struct rte_flow_action_of_set_vlan_pcp,
			      vlan_pcp)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_OF_POP_MPLS] = {
		.name = "of_pop_mpls",
		.help = "OpenFlow's OFPAT_POP_MPLS",
		.priv = PRIV_ACTION(OF_POP_MPLS,
				    sizeof(struct rte_flow_action_of_pop_mpls)),
		.next = NEXT(action_of_pop_mpls),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_OF_POP_MPLS_ETHERTYPE] = {
		.name = "ethertype",
		.help = "EtherType",
		.next = NEXT(action_of_pop_mpls, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY_HTON
			     (struct rte_flow_action_of_pop_mpls,
			      ethertype)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_OF_PUSH_MPLS] = {
		.name = "of_push_mpls",
		.help = "OpenFlow's OFPAT_PUSH_MPLS",
		.priv = PRIV_ACTION
			(OF_PUSH_MPLS,
			 sizeof(struct rte_flow_action_of_push_mpls)),
		.next = NEXT(action_of_push_mpls),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_OF_PUSH_MPLS_ETHERTYPE] = {
		.name = "ethertype",
		.help = "EtherType",
		.next = NEXT(action_of_push_mpls, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY_HTON
			     (struct rte_flow_action_of_push_mpls,
			      ethertype)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_VXLAN_ENCAP] = {
		.name = "vxlan_encap",
		.help = "VXLAN encapsulation, uses configuration set by \"set"
			" vxlan\"",
		.priv = PRIV_ACTION(VXLAN_ENCAP,
				    sizeof(struct action_vxlan_encap_data)),
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_NEXT)),
		.call = parse_vc_action_vxlan_encap,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_VXLAN_DECAP] = {
		.name = "vxlan_decap",
		.help = "Performs a decapsulation action by stripping all"
			" headers of the VXLAN tunnel network overlay from the"
			" matched flow.",
		.priv = PRIV_ACTION(VXLAN_DECAP, 0),
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_NEXT)),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_NVGRE_ENCAP] = {
		.name = "nvgre_encap",
		.help = "NVGRE encapsulation, uses configuration set by \"set"
			" nvgre\"",
		.priv = PRIV_ACTION(NVGRE_ENCAP,
				    sizeof(struct action_nvgre_encap_data)),
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_NEXT)),
		.call = parse_vc_action_nvgre_encap,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_NVGRE_DECAP] = {
		.name = "nvgre_decap",
		.help = "Performs a decapsulation action by stripping all"
			" headers of the NVGRE tunnel network overlay from the"
			" matched flow.",
		.priv = PRIV_ACTION(NVGRE_DECAP, 0),
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_NEXT)),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_L2_ENCAP] = {
		.name = "l2_encap",
		.help = "l2 encap, uses configuration set by"
			" \"set l2_encap\"",
		.priv = PRIV_ACTION(RAW_ENCAP,
				    sizeof(struct action_raw_encap_data)),
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_NEXT)),
		.call = parse_vc_action_l2_encap,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_L2_DECAP] = {
		.name = "l2_decap",
		.help = "l2 decap, uses configuration set by"
			" \"set l2_decap\"",
		.priv = PRIV_ACTION(RAW_DECAP,
				    sizeof(struct action_raw_decap_data)),
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_NEXT)),
		.call = parse_vc_action_l2_decap,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_MPLSOGRE_ENCAP] = {
		.name = "mplsogre_encap",
		.help = "mplsogre encapsulation, uses configuration set by"
			" \"set mplsogre_encap\"",
		.priv = PRIV_ACTION(RAW_ENCAP,
				    sizeof(struct action_raw_encap_data)),
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_NEXT)),
		.call = parse_vc_action_mplsogre_encap,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_MPLSOGRE_DECAP] = {
		.name = "mplsogre_decap",
		.help = "mplsogre decapsulation, uses configuration set by"
			" \"set mplsogre_decap\"",
		.priv = PRIV_ACTION(RAW_DECAP,
				    sizeof(struct action_raw_decap_data)),
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_NEXT)),
		.call = parse_vc_action_mplsogre_decap,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_MPLSOUDP_ENCAP] = {
		.name = "mplsoudp_encap",
		.help = "mplsoudp encapsulation, uses configuration set by"
			" \"set mplsoudp_encap\"",
		.priv = PRIV_ACTION(RAW_ENCAP,
				    sizeof(struct action_raw_encap_data)),
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_NEXT)),
		.call = parse_vc_action_mplsoudp_encap,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_MPLSOUDP_DECAP] = {
		.name = "mplsoudp_decap",
		.help = "mplsoudp decapsulation, uses configuration set by"
			" \"set mplsoudp_decap\"",
		.priv = PRIV_ACTION(RAW_DECAP,
				    sizeof(struct action_raw_decap_data)),
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_NEXT)),
		.call = parse_vc_action_mplsoudp_decap,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_SET_IPV4_SRC] = {
		.name = "set_ipv4_src",
		.help = "Set a new IPv4 source address in the outermost"
			" IPv4 header",
		.priv = PRIV_ACTION(SET_IPV4_SRC,
			sizeof(struct rte_flow_action_set_ipv4)),
		.next = NEXT(action_set_ipv4_src),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_SET_IPV4_SRC_IPV4_SRC] = {
		.name = "ipv4_addr",
		.help = "new IPv4 source address to set",
		.next = NEXT(action_set_ipv4_src, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_IPV4_ADDR)),
		.args = ARGS(ARGS_ENTRY_HTON
			(struct rte_flow_action_set_ipv4, ipv4_addr)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_SET_IPV4_DST] = {
		.name = "set_ipv4_dst",
		.help = "Set a new IPv4 destination address in the outermost"
			" IPv4 header",
		.priv = PRIV_ACTION(SET_IPV4_DST,
			sizeof(struct rte_flow_action_set_ipv4)),
		.next = NEXT(action_set_ipv4_dst),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_SET_IPV4_DST_IPV4_DST] = {
		.name = "ipv4_addr",
		.help = "new IPv4 destination address to set",
		.next = NEXT(action_set_ipv4_dst, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_IPV4_ADDR)),
		.args = ARGS(ARGS_ENTRY_HTON
			(struct rte_flow_action_set_ipv4, ipv4_addr)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_SET_IPV6_SRC] = {
		.name = "set_ipv6_src",
		.help = "Set a new IPv6 source address in the outermost"
			" IPv6 header",
		.priv = PRIV_ACTION(SET_IPV6_SRC,
			sizeof(struct rte_flow_action_set_ipv6)),
		.next = NEXT(action_set_ipv6_src),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_SET_IPV6_SRC_IPV6_SRC] = {
		.name = "ipv6_addr",
		.help = "new IPv6 source address to set",
		.next = NEXT(action_set_ipv6_src, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_IPV6_ADDR)),
		.args = ARGS(ARGS_ENTRY_HTON
			(struct rte_flow_action_set_ipv6, ipv6_addr)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_SET_IPV6_DST] = {
		.name = "set_ipv6_dst",
		.help = "Set a new IPv6 destination address in the outermost"
			" IPv6 header",
		.priv = PRIV_ACTION(SET_IPV6_DST,
			sizeof(struct rte_flow_action_set_ipv6)),
		.next = NEXT(action_set_ipv6_dst),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_SET_IPV6_DST_IPV6_DST] = {
		.name = "ipv6_addr",
		.help = "new IPv6 destination address to set",
		.next = NEXT(action_set_ipv6_dst, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_IPV6_ADDR)),
		.args = ARGS(ARGS_ENTRY_HTON
			(struct rte_flow_action_set_ipv6, ipv6_addr)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_SET_TP_SRC] = {
		.name = "set_tp_src",
		.help = "set a new source port number in the outermost"
			" TCP/UDP header",
		.priv = PRIV_ACTION(SET_TP_SRC,
			sizeof(struct rte_flow_action_set_tp)),
		.next = NEXT(action_set_tp_src),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_SET_TP_SRC_TP_SRC] = {
		.name = "port",
		.help = "new source port number to set",
		.next = NEXT(action_set_tp_src, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY_HTON
			     (struct rte_flow_action_set_tp, port)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_SET_TP_DST] = {
		.name = "set_tp_dst",
		.help = "set a new destination port number in the outermost"
			" TCP/UDP header",
		.priv = PRIV_ACTION(SET_TP_DST,
			sizeof(struct rte_flow_action_set_tp)),
		.next = NEXT(action_set_tp_dst),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_SET_TP_DST_TP_DST] = {
		.name = "port",
		.help = "new destination port number to set",
		.next = NEXT(action_set_tp_dst, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY_HTON
			     (struct rte_flow_action_set_tp, port)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_MAC_SWAP] = {
		.name = "mac_swap",
		.help = "Swap the source and destination MAC addresses"
			" in the outermost Ethernet header",
		.priv = PRIV_ACTION(MAC_SWAP, 0),
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_NEXT)),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_DEC_TTL] = {
		.name = "dec_ttl",
		.help = "decrease network TTL if available",
		.priv = PRIV_ACTION(DEC_TTL, 0),
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_NEXT)),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_SET_TTL] = {
		.name = "set_ttl",
		.help = "set ttl value",
		.priv = PRIV_ACTION(SET_TTL,
			sizeof(struct rte_flow_action_set_ttl)),
		.next = NEXT(action_set_ttl),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_SET_TTL_TTL] = {
		.name = "ttl_value",
		.help = "new ttl value to set",
		.next = NEXT(action_set_ttl, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY_HTON
			     (struct rte_flow_action_set_ttl, ttl_value)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_SET_MAC_SRC] = {
		.name = "set_mac_src",
		.help = "set source mac address",
		.priv = PRIV_ACTION(SET_MAC_SRC,
			sizeof(struct rte_flow_action_set_mac)),
		.next = NEXT(action_set_mac_src),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_SET_MAC_SRC_MAC_SRC] = {
		.name = "mac_addr",
		.help = "new source mac address",
		.next = NEXT(action_set_mac_src, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_MAC_ADDR)),
		.args = ARGS(ARGS_ENTRY_HTON
			     (struct rte_flow_action_set_mac, mac_addr)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_SET_MAC_DST] = {
		.name = "set_mac_dst",
		.help = "set destination mac address",
		.priv = PRIV_ACTION(SET_MAC_DST,
			sizeof(struct rte_flow_action_set_mac)),
		.next = NEXT(action_set_mac_dst),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_SET_MAC_DST_MAC_DST] = {
		.name = "mac_addr",
		.help = "new destination mac address to set",
		.next = NEXT(action_set_mac_dst, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_MAC_ADDR)),
		.args = ARGS(ARGS_ENTRY_HTON
			     (struct rte_flow_action_set_mac, mac_addr)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_INC_TCP_SEQ] = {
		.name = "inc_tcp_seq",
		.help = "increase TCP sequence number",
		.priv = PRIV_ACTION(INC_TCP_SEQ, sizeof(rte_be32_t)),
		.next = NEXT(action_inc_tcp_seq),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_INC_TCP_SEQ_VALUE] = {
		.name = "value",
		.help = "the value to increase TCP sequence number by",
		.next = NEXT(action_inc_tcp_seq, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARG_ENTRY_HTON(rte_be32_t)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_DEC_TCP_SEQ] = {
		.name = "dec_tcp_seq",
		.help = "decrease TCP sequence number",
		.priv = PRIV_ACTION(DEC_TCP_SEQ, sizeof(rte_be32_t)),
		.next = NEXT(action_dec_tcp_seq),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_DEC_TCP_SEQ_VALUE] = {
		.name = "value",
		.help = "the value to decrease TCP sequence number by",
		.next = NEXT(action_dec_tcp_seq, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARG_ENTRY_HTON(rte_be32_t)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_INC_TCP_ACK] = {
		.name = "inc_tcp_ack",
		.help = "increase TCP acknowledgment number",
		.priv = PRIV_ACTION(INC_TCP_ACK, sizeof(rte_be32_t)),
		.next = NEXT(action_inc_tcp_ack),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_INC_TCP_ACK_VALUE] = {
		.name = "value",
		.help = "the value to increase TCP acknowledgment number by",
		.next = NEXT(action_inc_tcp_ack, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARG_ENTRY_HTON(rte_be32_t)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_DEC_TCP_ACK] = {
		.name = "dec_tcp_ack",
		.help = "decrease TCP acknowledgment number",
		.priv = PRIV_ACTION(DEC_TCP_ACK, sizeof(rte_be32_t)),
		.next = NEXT(action_dec_tcp_ack),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_DEC_TCP_ACK_VALUE] = {
		.name = "value",
		.help = "the value to decrease TCP acknowledgment number by",
		.next = NEXT(action_dec_tcp_ack, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARG_ENTRY_HTON(rte_be32_t)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_RAW_ENCAP] = {
		.name = "raw_encap",
		.help = "encapsulation data, defined by set raw_encap",
		.priv = PRIV_ACTION(RAW_ENCAP,
			sizeof(struct action_raw_encap_data)),
		.next = NEXT(action_raw_encap),
		.call = parse_vc_action_raw_encap,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_RAW_ENCAP_SIZE] = {
		.name = "size",
		.help = "raw encap size",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_NEXT),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_raw_encap, size)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_RAW_ENCAP_INDEX] = {
		.name = "index",
		.help = "the index of raw_encap_confs",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_RAW_ENCAP_INDEX_VALUE)),
	},
	[RTE_FLOW_PARSER_CMD_ACTION_RAW_ENCAP_INDEX_VALUE] = {
		.name = "{index}",
		.type = "UNSIGNED",
		.help = "unsigned integer value",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_NEXT)),
		.call = parse_vc_action_raw_encap_index,
		.comp = comp_set_raw_index,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_RAW_DECAP] = {
		.name = "raw_decap",
		.help = "decapsulation data, defined by set raw_encap",
		.priv = PRIV_ACTION(RAW_DECAP,
			sizeof(struct action_raw_decap_data)),
		.next = NEXT(action_raw_decap),
		.call = parse_vc_action_raw_decap,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_RAW_DECAP_INDEX] = {
		.name = "index",
		.help = "the index of raw_encap_confs",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_RAW_DECAP_INDEX_VALUE)),
	},
	[RTE_FLOW_PARSER_CMD_ACTION_RAW_DECAP_INDEX_VALUE] = {
		.name = "{index}",
		.type = "UNSIGNED",
		.help = "unsigned integer value",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_NEXT)),
		.call = parse_vc_action_raw_decap_index,
		.comp = comp_set_raw_index,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD] = {
		.name = "modify_field",
		.help = "modify destination field with data from source field",
		.priv = PRIV_ACTION(MODIFY_FIELD, ACTION_MODIFY_SIZE),
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_OP)),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_OP] = {
		.name = "op",
		.help = "operation type",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_DST_TYPE),
			NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_OP_VALUE)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_OP_VALUE] = {
		.name = "{operation}",
		.help = "operation type value",
		.call = parse_vc_modify_field_op,
		.comp = comp_set_modify_field_op,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_DST_TYPE] = {
		.name = "dst_type",
		.help = "destination field type",
		.next = NEXT(action_modify_field_dst,
			NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_DST_TYPE_VALUE)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_DST_TYPE_VALUE] = {
		.name = "{dst_type}",
		.help = "destination field type value",
		.call = parse_vc_modify_field_id,
		.comp = comp_set_modify_field_id,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_DST_LEVEL] = {
		.name = "dst_level",
		.help = "destination field level",
		.next = NEXT(action_modify_field_dst,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_DST_LEVEL_VALUE)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_DST_LEVEL_VALUE] = {
		.name = "{dst_level}",
		.help = "destination field level value",
		.call = parse_vc_modify_field_level,
		.comp = comp_none,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_DST_TAG_INDEX] = {
		.name = "dst_tag_index",
		.help = "destination field tag array",
		.next = NEXT(action_modify_field_dst,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_modify_field,
					dst.tag_index)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_DST_TYPE_ID] = {
		.name = "dst_type_id",
		.help = "destination field type ID",
		.next = NEXT(action_modify_field_dst,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_modify_field,
					dst.type)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_DST_CLASS_ID] = {
		.name = "dst_class",
		.help = "destination field class ID",
		.next = NEXT(action_modify_field_dst,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_action_modify_field,
					     dst.class_id)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_DST_OFFSET] = {
		.name = "dst_offset",
		.help = "destination field bit offset",
		.next = NEXT(action_modify_field_dst,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_modify_field,
					dst.offset)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_SRC_TYPE] = {
		.name = "src_type",
		.help = "source field type",
		.next = NEXT(action_modify_field_src,
			NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_SRC_TYPE_VALUE)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_SRC_TYPE_VALUE] = {
		.name = "{src_type}",
		.help = "source field type value",
		.call = parse_vc_modify_field_id,
		.comp = comp_set_modify_field_id,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_SRC_LEVEL] = {
		.name = "src_level",
		.help = "source field level",
		.next = NEXT(action_modify_field_src,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_SRC_LEVEL_VALUE)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_SRC_LEVEL_VALUE] = {
		.name = "{src_level}",
		.help = "source field level value",
		.call = parse_vc_modify_field_level,
		.comp = comp_none,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_SRC_TAG_INDEX] = {
		.name = "src_tag_index",
		.help = "source field tag array",
		.next = NEXT(action_modify_field_src,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_modify_field,
					src.tag_index)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_SRC_TYPE_ID] = {
		.name = "src_type_id",
		.help = "source field type ID",
		.next = NEXT(action_modify_field_src,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_modify_field,
					src.type)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_SRC_CLASS_ID] = {
		.name = "src_class",
		.help = "source field class ID",
		.next = NEXT(action_modify_field_src,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_action_modify_field,
					     src.class_id)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_SRC_OFFSET] = {
		.name = "src_offset",
		.help = "source field bit offset",
		.next = NEXT(action_modify_field_src,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_modify_field,
					src.offset)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_SRC_VALUE] = {
		.name = "src_value",
		.help = "source immediate value",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_WIDTH),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_HEX)),
		.args = ARGS(ARGS_ENTRY_ARB(0, 0),
			     ARGS_ENTRY_ARB(0, 0),
			     ARGS_ENTRY(struct rte_flow_action_modify_field,
					src.value)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_SRC_POINTER] = {
		.name = "src_ptr",
		.help = "pointer to source immediate value",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_WIDTH),
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_HEX)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_modify_field,
					src.pvalue),
			     ARGS_ENTRY_ARB(0, 0),
			     ARGS_ENTRY_ARB
				(sizeof(struct rte_flow_action_modify_field),
				 FLOW_FIELD_PATTERN_SIZE)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_WIDTH] = {
		.name = "width",
		.help = "number of bits to copy",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_NEXT),
			NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_modify_field,
					width)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_SEND_TO_KERNEL] = {
		.name = "send_to_kernel",
		.help = "send packets to kernel",
		.priv = PRIV_ACTION(SEND_TO_KERNEL, 0),
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_NEXT)),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_IPV6_EXT_REMOVE] = {
		.name = "ipv6_ext_remove",
		.help = "IPv6 extension type, defined by set ipv6_ext_remove",
		.priv = PRIV_ACTION(IPV6_EXT_REMOVE,
			sizeof(struct action_ipv6_ext_remove_data)),
		.next = NEXT(action_ipv6_ext_remove),
		.call = parse_vc_action_ipv6_ext_remove,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_IPV6_EXT_REMOVE_INDEX] = {
		.name = "index",
		.help = "the index of ipv6_ext_remove",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_IPV6_EXT_REMOVE_INDEX_VALUE)),
	},
	[RTE_FLOW_PARSER_CMD_ACTION_IPV6_EXT_REMOVE_INDEX_VALUE] = {
		.name = "{index}",
		.type = "UNSIGNED",
		.help = "unsigned integer value",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_NEXT)),
		.call = parse_vc_action_ipv6_ext_remove_index,
		.comp = comp_set_ipv6_ext_index,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_IPV6_EXT_PUSH] = {
		.name = "ipv6_ext_push",
		.help = "IPv6 extension data, defined by set ipv6_ext_push",
		.priv = PRIV_ACTION(IPV6_EXT_PUSH,
			sizeof(struct action_ipv6_ext_push_data)),
		.next = NEXT(action_ipv6_ext_push),
		.call = parse_vc_action_ipv6_ext_push,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_IPV6_EXT_PUSH_INDEX] = {
		.name = "index",
		.help = "the index of ipv6_ext_push",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_IPV6_EXT_PUSH_INDEX_VALUE)),
	},
	[RTE_FLOW_PARSER_CMD_ACTION_IPV6_EXT_PUSH_INDEX_VALUE] = {
		.name = "{index}",
		.type = "UNSIGNED",
		.help = "unsigned integer value",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_NEXT)),
		.call = parse_vc_action_ipv6_ext_push_index,
		.comp = comp_set_ipv6_ext_index,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_NAT64] = {
		.name = "nat64",
		.help = "NAT64 IP headers translation",
		.priv = PRIV_ACTION(NAT64, sizeof(struct rte_flow_action_nat64)),
		.next = NEXT(action_nat64),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_NAT64_MODE] = {
		.name = "type",
		.help = "NAT64 translation type",
		.next = NEXT(action_nat64, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_nat64, type)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_JUMP_TO_TABLE_INDEX] = {
		.name = "jump_to_table_index",
		.help = "Jump to table index",
		.priv = PRIV_ACTION(JUMP_TO_TABLE_INDEX,
				    sizeof(struct rte_flow_action_jump_to_table_index)),
		.next = NEXT(action_jump_to_table_index),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_JUMP_TO_TABLE_INDEX_TABLE] = {
		.name = "table",
		.help = "table id to redirect traffic to",
		.next = NEXT(action_jump_to_table_index,
			NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_JUMP_TO_TABLE_INDEX_TABLE_VALUE)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_jump_to_table_index, table)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_JUMP_TO_TABLE_INDEX_TABLE_VALUE] = {
		.name = "{table_id}",
		.type = "TABLE_ID",
		.help = "table id for jump action",
		.call = parse_jump_table_id,
		.comp = comp_table_id,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_JUMP_TO_TABLE_INDEX_INDEX] = {
		.name = "index",
		.help = "rule index to redirect traffic to",
		.next = NEXT(action_jump_to_table_index, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_jump_to_table_index, index)),
		.call = parse_vc_conf,
	},

	/* Top level command. */
	[RTE_FLOW_PARSER_CMD_SET] = {
		.name = "set",
		.help = "set raw encap/decap/sample data",
		.type = "set raw_encap|raw_decap <index> <pattern>"
				" or set sample_actions <index> <action>",
		.next = NEXT(NEXT_ENTRY
			     (RTE_FLOW_PARSER_CMD_SET_RAW_ENCAP,
			      RTE_FLOW_PARSER_CMD_SET_RAW_DECAP,
			      RTE_FLOW_PARSER_CMD_SET_SAMPLE_ACTIONS,
			      RTE_FLOW_PARSER_CMD_SET_IPV6_EXT_REMOVE,
			      RTE_FLOW_PARSER_CMD_SET_IPV6_EXT_PUSH)),
		.call = parse_set_init,
	},
	/* Sub-level commands. */
	[RTE_FLOW_PARSER_CMD_SET_RAW_ENCAP] = {
		.name = "raw_encap",
		.help = "set raw encap data",
		.next = NEXT(next_set_raw),
		.args = ARGS(ARGS_ENTRY_ARB_BOUNDED
				(offsetof(struct rte_flow_parser_output, port),
				 sizeof(((struct rte_flow_parser_output *)0)->port),
				 0, RAW_ENCAP_CONFS_MAX_NUM - 1)),
		.call = parse_set_raw_encap_decap,
	},
	[RTE_FLOW_PARSER_CMD_SET_RAW_DECAP] = {
		.name = "raw_decap",
		.help = "set raw decap data",
		.next = NEXT(next_set_raw),
		.args = ARGS(ARGS_ENTRY_ARB_BOUNDED
				(offsetof(struct rte_flow_parser_output, port),
				 sizeof(((struct rte_flow_parser_output *)0)->port),
				 0, RAW_ENCAP_CONFS_MAX_NUM - 1)),
		.call = parse_set_raw_encap_decap,
	},
	[RTE_FLOW_PARSER_CMD_SET_RAW_INDEX] = {
		.name = "{index}",
		.type = "RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED",
		.help = "index of raw_encap/raw_decap data",
		.next = NEXT(next_item),
		.call = parse_port,
	},
	[RTE_FLOW_PARSER_CMD_SET_SAMPLE_INDEX] = {
		.name = "{index}",
		.type = "UNSIGNED",
		.help = "index of sample actions",
		.next = NEXT(next_action_sample),
		.call = parse_port,
	},
	[RTE_FLOW_PARSER_CMD_SET_SAMPLE_ACTIONS] = {
		.name = "sample_actions",
		.help = "set sample actions list",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_SET_SAMPLE_INDEX)),
		.args = ARGS(ARGS_ENTRY_ARB_BOUNDED
				(offsetof(struct rte_flow_parser_output, port),
				 sizeof(((struct rte_flow_parser_output *)0)->port),
				 0, RAW_SAMPLE_CONFS_MAX_NUM - 1)),
		.call = parse_set_sample_action,
	},
	[RTE_FLOW_PARSER_CMD_SET_IPV6_EXT_PUSH] = {
		.name = "ipv6_ext_push",
		.help = "set IPv6 extension header",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_SET_IPV6_EXT_INDEX)),
		.args = ARGS(ARGS_ENTRY_ARB_BOUNDED
				(offsetof(struct rte_flow_parser_output, port),
				 sizeof(((struct rte_flow_parser_output *)0)->port),
				 0, IPV6_EXT_PUSH_CONFS_MAX_NUM - 1)),
		.call = parse_set_ipv6_ext_action,
	},
	[RTE_FLOW_PARSER_CMD_SET_IPV6_EXT_REMOVE] = {
		.name = "ipv6_ext_remove",
		.help = "set IPv6 extension header",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_SET_IPV6_EXT_INDEX)),
		.args = ARGS(ARGS_ENTRY_ARB_BOUNDED
				(offsetof(struct rte_flow_parser_output, port),
				 sizeof(((struct rte_flow_parser_output *)0)->port),
				 0, IPV6_EXT_PUSH_CONFS_MAX_NUM - 1)),
		.call = parse_set_ipv6_ext_action,
	},
	[RTE_FLOW_PARSER_CMD_SET_IPV6_EXT_INDEX] = {
		.name = "{index}",
		.type = "UNSIGNED",
		.help = "index of ipv6 extension push/remove actions",
		.next = NEXT(item_ipv6_push_ext),
		.call = parse_port,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_IPV6_PUSH_REMOVE_EXT] = {
		.name = "ipv6_ext",
		.help = "set IPv6 extension header",
		.priv = PRIV_ITEM(IPV6_EXT,
				  sizeof(struct rte_flow_item_ipv6_ext)),
		.next = NEXT(item_ipv6_push_ext_type),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_IPV6_PUSH_REMOVE_EXT_TYPE] = {
		.name = "type",
		.help = "set IPv6 extension type",
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv6_ext,
					     next_hdr)),
		.next = NEXT(item_ipv6_push_ext_header, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
	},
	[RTE_FLOW_PARSER_CMD_ACTION_SET_TAG] = {
		.name = "set_tag",
		.help = "set tag",
		.priv = PRIV_ACTION(SET_TAG,
			sizeof(struct rte_flow_action_set_tag)),
		.next = NEXT(action_set_tag),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_SET_TAG_INDEX] = {
		.name = "index",
		.help = "index of tag array",
		.next = NEXT(action_set_tag, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_set_tag, index)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_SET_TAG_DATA] = {
		.name = "data",
		.help = "tag value",
		.next = NEXT(action_set_tag, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY
			     (struct rte_flow_action_set_tag, data)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_SET_TAG_MASK] = {
		.name = "mask",
		.help = "mask for tag value",
		.next = NEXT(action_set_tag, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY
			     (struct rte_flow_action_set_tag, mask)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_SET_META] = {
		.name = "set_meta",
		.help = "set metadata",
		.priv = PRIV_ACTION(SET_META,
			sizeof(struct rte_flow_action_set_meta)),
		.next = NEXT(action_set_meta),
		.call = parse_vc_action_set_meta,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_SET_META_DATA] = {
		.name = "data",
		.help = "metadata value",
		.next = NEXT(action_set_meta, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY
			     (struct rte_flow_action_set_meta, data)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_SET_META_MASK] = {
		.name = "mask",
		.help = "mask for metadata value",
		.next = NEXT(action_set_meta, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY
			     (struct rte_flow_action_set_meta, mask)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_SET_IPV4_DSCP] = {
		.name = "set_ipv4_dscp",
		.help = "set DSCP value",
		.priv = PRIV_ACTION(SET_IPV4_DSCP,
			sizeof(struct rte_flow_action_set_dscp)),
		.next = NEXT(action_set_ipv4_dscp),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_SET_IPV4_DSCP_VALUE] = {
		.name = "dscp_value",
		.help = "new IPv4 DSCP value to set",
		.next = NEXT(action_set_ipv4_dscp, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY
			     (struct rte_flow_action_set_dscp, dscp)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_SET_IPV6_DSCP] = {
		.name = "set_ipv6_dscp",
		.help = "set DSCP value",
		.priv = PRIV_ACTION(SET_IPV6_DSCP,
			sizeof(struct rte_flow_action_set_dscp)),
		.next = NEXT(action_set_ipv6_dscp),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_SET_IPV6_DSCP_VALUE] = {
		.name = "dscp_value",
		.help = "new IPv6 DSCP value to set",
		.next = NEXT(action_set_ipv6_dscp, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY
			     (struct rte_flow_action_set_dscp, dscp)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_AGE] = {
		.name = "age",
		.help = "set a specific metadata header",
		.next = NEXT(action_age),
		.priv = PRIV_ACTION(AGE,
			sizeof(struct rte_flow_action_age)),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_AGE_TIMEOUT] = {
		.name = "timeout",
		.help = "flow age timeout value",
		.args = ARGS(ARGS_ENTRY_BF(struct rte_flow_action_age,
					   timeout, 24)),
		.next = NEXT(action_age, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_AGE_UPDATE] = {
		.name = "age_update",
		.help = "update aging parameter",
		.next = NEXT(action_age_update),
		.priv = PRIV_ACTION(AGE,
				    sizeof(struct rte_flow_update_age)),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_AGE_UPDATE_TIMEOUT] = {
		.name = "timeout",
		.help = "age timeout update value",
		.args = ARGS(ARGS_ENTRY_BF(struct rte_flow_update_age,
					   timeout, 24)),
		.next = NEXT(action_age_update, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.call = parse_vc_conf_timeout,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_AGE_UPDATE_TOUCH] = {
		.name = "touch",
		.help = "this flow is touched",
		.next = NEXT(action_age_update, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_BOOLEAN)),
		.args = ARGS(ARGS_ENTRY_BF(struct rte_flow_update_age,
					   touch, 1)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_SAMPLE] = {
		.name = "sample",
		.help = "set a sample action",
		.next = NEXT(action_sample),
		.priv = PRIV_ACTION(SAMPLE,
			sizeof(struct action_sample_data)),
		.call = parse_vc_action_sample,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_SAMPLE_RATIO] = {
		.name = "ratio",
		.help = "flow sample ratio value",
		.next = NEXT(action_sample, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY_ARB
			     (offsetof(struct action_sample_data, conf) +
			      offsetof(struct rte_flow_action_sample, ratio),
			      sizeof(((struct rte_flow_action_sample *)0)->
				     ratio))),
	},
	[RTE_FLOW_PARSER_CMD_ACTION_SAMPLE_INDEX] = {
		.name = "index",
		.help = "the index of sample actions list",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_SAMPLE_INDEX_VALUE)),
	},
	[RTE_FLOW_PARSER_CMD_ACTION_SAMPLE_INDEX_VALUE] = {
		.name = "{index}",
		.type = "RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED",
		.help = "unsigned integer value",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_NEXT)),
		.call = parse_vc_action_sample_index,
		.comp = comp_set_sample_index,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_CONNTRACK] = {
		.name = "conntrack",
		.help = "create a conntrack object",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_NEXT)),
		.priv = PRIV_ACTION(CONNTRACK,
				    sizeof(struct rte_flow_action_conntrack)),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_CONNTRACK_UPDATE] = {
		.name = "conntrack_update",
		.help = "update a conntrack object",
		.next = NEXT(action_update_conntrack),
		.priv = PRIV_ACTION(CONNTRACK,
				    sizeof(struct rte_flow_modify_conntrack)),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_CONNTRACK_UPDATE_DIR] = {
		.name = "dir",
		.help = "update a conntrack object direction",
		.next = NEXT(action_update_conntrack),
		.call = parse_vc_action_conntrack_update,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_CONNTRACK_UPDATE_CTX] = {
		.name = "ctx",
		.help = "update a conntrack object context",
		.next = NEXT(action_update_conntrack),
		.call = parse_vc_action_conntrack_update,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_PORT_REPRESENTOR] = {
		.name = "port_representor",
		.help = "at embedded switch level, send matching traffic to the given ethdev",
		.priv = PRIV_ACTION(PORT_REPRESENTOR,
				    sizeof(struct rte_flow_action_ethdev)),
		.next = NEXT(action_port_representor),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_PORT_REPRESENTOR_PORT_ID] = {
		.name = "port_id",
		.help = "ethdev port ID",
		.next = NEXT(action_port_representor,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_ethdev,
					port_id)),
		.call = parse_vc_conf,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_REPRESENTED_PORT] = {
		.name = "represented_port",
		.help = "at embedded switch level, send matching traffic "
			"to the entity represented by the given ethdev",
		.priv = PRIV_ACTION(REPRESENTED_PORT,
				sizeof(struct rte_flow_action_ethdev)),
		.next = NEXT(action_represented_port),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_REPRESENTED_PORT_ETHDEV_PORT_ID] = {
		.name = "ethdev_port_id",
		.help = "ethdev port ID",
		.next = NEXT(action_represented_port,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_ethdev,
					port_id)),
		.call = parse_vc_conf,
	},
	/* Indirect action destroy arguments. */
	[RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_DESTROY_ID] = {
		.name = "action_id",
		.help = "specify a indirect action id to destroy",
		.next = NEXT(next_ia_destroy_attr,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_INDIRECT_ACTION_ID)),
		.args = ARGS(ARGS_ENTRY_PTR(struct rte_flow_parser_output,
					    args.ia_destroy.action_id)),
		.call = parse_ia_destroy,
	},
	/* Indirect action create arguments. */
	[RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_CREATE_ID] = {
		.name = "action_id",
		.help = "specify a indirect action id to create",
		.next = NEXT(next_ia_create_attr,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_INDIRECT_ACTION_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, args.vc.attr.group)),
	},
	[RTE_FLOW_PARSER_CMD_ACTION_INDIRECT] = {
		.name = "indirect",
		.help = "apply indirect action by id",
		.priv = PRIV_ACTION(INDIRECT, 0),
		.next = NEXT(next_ia),
		.args = ARGS(ARGS_ENTRY_ARB(0, sizeof(uint32_t))),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_INDIRECT_LIST] = {
		.name = "indirect_list",
		.help = "apply indirect list action by id",
		.priv = PRIV_ACTION(INDIRECT_LIST,
				    sizeof(struct
					   rte_flow_action_indirect_list)),
		.next = NEXT(next_ial),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_INDIRECT_LIST_HANDLE] = {
		.name = "handle",
		.help = "indirect list handle",
		.next = NEXT(next_ial, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_INDIRECT_LIST_ACTION_ID2PTR_HANDLE)),
		.args = ARGS(ARGS_ENTRY_ARB(0, sizeof(uint32_t))),
	},
	[RTE_FLOW_PARSER_CMD_ACTION_INDIRECT_LIST_CONF] = {
		.name = "conf",
		.help = "indirect list configuration",
		.next = NEXT(next_ial, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_INDIRECT_LIST_ACTION_ID2PTR_CONF)),
		.args = ARGS(ARGS_ENTRY_ARB(0, sizeof(uint32_t))),
	},
	[RTE_FLOW_PARSER_CMD_INDIRECT_LIST_ACTION_ID2PTR_HANDLE] = {
		.type = "UNSIGNED",
		.help = "unsigned integer value",
		.call = parse_indlst_id2ptr,
		.comp = comp_none,
	},
	[RTE_FLOW_PARSER_CMD_INDIRECT_LIST_ACTION_ID2PTR_CONF] = {
		.type = "UNSIGNED",
		.help = "unsigned integer value",
		.call = parse_indlst_id2ptr,
		.comp = comp_none,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_SHARED_INDIRECT] = {
		.name = "shared_indirect",
		.help = "apply indirect action by id and port",
		.priv = PRIV_ACTION(INDIRECT, 0),
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_PORT)),
		.args = ARGS(ARGS_ENTRY_ARB(0, sizeof(uint32_t)),
			     ARGS_ENTRY_ARB(0, sizeof(uint32_t))),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_PORT] = {
		.name = "{indirect_action_port}",
		.type = "RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_PORT",
		.help = "indirect action port",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_ID2PTR)),
		.call = parse_ia_port,
		.comp = comp_none,
	},
	[RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_ID2PTR] = {
		.name = "{action_id}",
		.type = "INDIRECT_ACTION_ID",
		.help = "indirect action id",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_NEXT)),
		.call = parse_ia_id2ptr,
		.comp = comp_none,
	},
	[RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_INGRESS] = {
		.name = "ingress",
		.help = "affect rule to ingress",
		.next = NEXT(next_ia_create_attr),
		.call = parse_ia,
	},
	[RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_EGRESS] = {
		.name = "egress",
		.help = "affect rule to egress",
		.next = NEXT(next_ia_create_attr),
		.call = parse_ia,
	},
	[RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_TRANSFER] = {
		.name = "transfer",
		.help = "affect rule to transfer",
		.next = NEXT(next_ia_create_attr),
		.call = parse_ia,
	},
	[RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_SPEC] = {
		.name = "action",
		.help = "specify action to create indirect handle",
		.next = NEXT(next_action),
	},
	[RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_LIST] = {
		.name = "list",
		.help = "specify actions for indirect handle list",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTIONS, RTE_FLOW_PARSER_CMD_END)),
		.call = parse_ia,
	},
	[RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_FLOW_CONF] = {
		.name = "flow_conf",
		.help = "specify actions configuration for indirect handle list",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTIONS, RTE_FLOW_PARSER_CMD_END)),
		.call = parse_ia,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_POL_G] = {
		.name = "g_actions",
		.help = "submit a list of associated actions for green",
		.next = NEXT(next_action),
		.call = parse_mp,
	},
	[RTE_FLOW_PARSER_CMD_ACTION_POL_Y] = {
		.name = "y_actions",
		.help = "submit a list of associated actions for yellow",
		.next = NEXT(next_action),
	},
	[RTE_FLOW_PARSER_CMD_ACTION_POL_R] = {
		.name = "r_actions",
		.help = "submit a list of associated actions for red",
		.next = NEXT(next_action),
	},
	[RTE_FLOW_PARSER_CMD_ACTION_QUOTA_CREATE] = {
		.name = "quota_create",
		.help = "create quota action",
		.priv = PRIV_ACTION(QUOTA,
				    sizeof(struct rte_flow_action_quota)),
		.next = NEXT(action_quota_create),
		.call = parse_vc
	},
	[RTE_FLOW_PARSER_CMD_ACTION_QUOTA_CREATE_LIMIT] = {
		.name = "limit",
		.help = "quota limit",
		.next = NEXT(action_quota_create, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_quota, quota)),
		.call = parse_vc_conf
	},
	[RTE_FLOW_PARSER_CMD_ACTION_QUOTA_CREATE_MODE] = {
		.name = "mode",
		.help = "quota mode",
		.next = NEXT(action_quota_create,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_QUOTA_CREATE_MODE_NAME)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_quota, mode)),
		.call = parse_vc_conf
	},
	[RTE_FLOW_PARSER_CMD_ACTION_QUOTA_CREATE_MODE_NAME] = {
		.name = "mode_name",
		.help = "quota mode name",
		.call = parse_quota_mode_name,
		.comp = comp_quota_mode_name
	},
	[RTE_FLOW_PARSER_CMD_ACTION_QUOTA_QU] = {
		.name = "quota_update",
		.help = "update quota action",
		.priv = PRIV_ACTION(QUOTA,
				    sizeof(struct rte_flow_update_quota)),
		.next = NEXT(action_quota_update),
		.call = parse_vc
	},
	[RTE_FLOW_PARSER_CMD_ACTION_QUOTA_QU_LIMIT] = {
		.name = "limit",
		.help = "quota limit",
		.next = NEXT(action_quota_update, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_update_quota, quota)),
		.call = parse_vc_conf
	},
	[RTE_FLOW_PARSER_CMD_ACTION_QUOTA_QU_UPDATE_OP] = {
		.name = "update_op",
		.help = "query update op RTE_FLOW_PARSER_CMD_SET|RTE_FLOW_PARSER_CMD_ADD",
		.next = NEXT(action_quota_update,
			     NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_QUOTA_QU_UPDATE_OP_NAME)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_update_quota, op)),
		.call = parse_vc_conf
	},
	[RTE_FLOW_PARSER_CMD_ACTION_QUOTA_QU_UPDATE_OP_NAME] = {
		.name = "update_op_name",
		.help = "quota update op name",
		.call = parse_quota_update_name,
		.comp = comp_quota_update_name
	},

	/* Top-level command. */
	[RTE_FLOW_PARSER_CMD_ADD] = {
		.name = "add",
		.type = "port meter policy {port_id} {arg}",
		.help = "add port meter policy",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ITEM_POL_PORT)),
		.call = parse_init,
	},
	/* Sub-level commands. */
	[RTE_FLOW_PARSER_CMD_ITEM_POL_PORT] = {
		.name = "port",
		.help = "add port meter policy",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ITEM_POL_METER)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_POL_METER] = {
		.name = "meter",
		.help = "add port meter policy",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ITEM_POL_POLICY)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_POL_POLICY] = {
		.name = "policy",
		.help = "add port meter policy",
		.next = NEXT(NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_POL_R),
				NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_POL_Y),
				NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_POL_G),
				NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_POLICY_ID),
				NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_parser_output, args.policy.policy_id),
				ARGS_ENTRY(struct rte_flow_parser_output, port)),
		.call = parse_mp,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_AGGR_AFFINITY] = {
		.name = "aggr_affinity",
		.help = "match on the aggregated port receiving the packets",
		.priv = PRIV_ITEM(AGGR_AFFINITY,
				  sizeof(struct rte_flow_item_aggr_affinity)),
		.next = NEXT(item_aggr_affinity),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_AGGR_AFFINITY_VALUE] = {
		.name = "affinity",
		.help = "aggregated affinity value",
		.next = NEXT(item_aggr_affinity, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_aggr_affinity,
					affinity)),
	},
	[RTE_FLOW_PARSER_CMD_ITEM_TX_QUEUE] = {
		.name = "tx_queue",
		.help = "match on the tx queue of send packet",
		.priv = PRIV_ITEM(TX_QUEUE,
				  sizeof(struct rte_flow_item_tx_queue)),
		.next = NEXT(item_tx_queue),
		.call = parse_vc,
	},
	[RTE_FLOW_PARSER_CMD_ITEM_TX_QUEUE_VALUE] = {
		.name = "tx_queue_value",
		.help = "tx queue value",
		.next = NEXT(item_tx_queue, NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_tx_queue,
					tx_queue)),
	},
};

/** Remove and return last entry from argument stack. */
static const struct arg *
pop_args(struct context *ctx)
{
	return ctx->args_num ? ctx->args[--ctx->args_num] : NULL;
}

/** Add entry on top of the argument stack. */
static int
push_args(struct context *ctx, const struct arg *arg)
{
	if (ctx->args_num == CTX_STACK_SIZE)
		return -1;
	ctx->args[ctx->args_num++] = arg;
	return 0;
}

/** Spread value into buffer according to bit-mask. */
static size_t
arg_entry_bf_fill(void *dst, uintmax_t val, const struct arg *arg)
{
	uint32_t i = arg->size;
	uint32_t end = 0;
	int sub = 1;
	int add = 0;
	size_t len = 0;

	if (arg->mask == NULL)
		return 0;
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	if (arg->hton == 0) {
		i = 0;
		end = arg->size;
		sub = 0;
		add = 1;
	}
#endif
	while (i != end) {
		unsigned int shift = 0;
		uint8_t *buf = (uint8_t *)dst + arg->offset + (i -= sub);

		for (shift = 0; arg->mask[i] >> shift; ++shift) {
			if ((arg->mask[i] & (1 << shift)) == 0)
				continue;
			++len;
			if (dst == NULL)
				continue;
			*buf &= ~(1 << shift);
			*buf |= (val & 1) << shift;
			val >>= 1;
		}
		i += add;
	}
	return len;
}

/** Compare a string with a partial one of a given length. */
static int
strcmp_partial(const char *full, const char *partial, size_t partial_len)
{
	int r = strncmp(full, partial, partial_len);

	if (r != 0)
		return r;
	if (strlen(full) <= partial_len)
		return 0;
	return full[partial_len];
}

/**
 * Parse a prefix length and generate a bit-mask.
 *
 * Last argument (ctx->args) is retrieved to determine mask size, storage
 * location and whether the result must use network byte ordering.
 */
static int
parse_prefix(struct context *ctx, const struct token *token,
	     const char *str, unsigned int len,
	     void *buf, unsigned int size)
{
	const struct arg *arg = pop_args(ctx);
	static const uint8_t conv[] = { 0x00, 0x80, 0xc0, 0xe0, 0xf0,
					0xf8, 0xfc, 0xfe, 0xff };
	char *end;
	uintmax_t u;
	unsigned int bytes;
	unsigned int extra;

	(void)token;
	/* Argument is expected. */
	if (arg == NULL)
		return -1;
	errno = 0;
	u = strtoumax(str, &end, 0);
	if (errno || (size_t)(end - str) != len)
		goto error;
	if (arg->mask != NULL) {
		uintmax_t v = 0;

		extra = arg_entry_bf_fill(NULL, 0, arg);
		if (u > extra)
			goto error;
		if (ctx->object == NULL)
			return len;
		extra -= u;
		while (u--) {
			v <<= 1;
			v |= 1;
		}
		v <<= extra;
		if (arg_entry_bf_fill(ctx->object, v, arg) == 0 ||
		    arg_entry_bf_fill(ctx->objmask, -1, arg) == 0)
			goto error;
		return len;
	}
	bytes = u / 8;
	extra = u % 8;
	size = arg->size;
	if (bytes > size || bytes + !!extra > size)
		goto error;
	if (ctx->object == NULL)
		return len;
	buf = (uint8_t *)ctx->object + arg->offset;
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	if (arg->hton == 0) {
		memset((uint8_t *)buf + size - bytes, 0xff, bytes);
		memset(buf, 0x00, size - bytes);
		if (extra != 0)
			((uint8_t *)buf)[size - bytes - 1] = conv[extra];
	} else
#endif
	{
		memset(buf, 0xff, bytes);
		memset((uint8_t *)buf + bytes, 0x00, size - bytes);
		if (extra != 0)
			((uint8_t *)buf)[bytes] = conv[extra];
	}
	if (ctx->objmask != NULL)
		memset((uint8_t *)ctx->objmask + arg->offset, 0xff, size);
	return len;
error:
	push_args(ctx, arg);
	return -1;
}

/** Default parsing function for token name matching. */
static int
parse_default(struct context *ctx, const struct token *token,
	      const char *str, unsigned int len,
	      void *buf, unsigned int size)
{
	(void)ctx;
	(void)buf;
	(void)size;
	if (strcmp_partial(token->name, str, len) != 0)
		return -1;
	return len;
}

/** Parse flow command, initialize output buffer for subsequent tokens. */
static int
parse_init(struct context *ctx, const struct token *token,
	   const char *str, unsigned int len,
	   void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return len;
	/* Make sure buffer is large enough. */
	if (size < sizeof(*out))
		return -1;
	/* Initialize buffer. */
	memset(out, 0x00, sizeof(*out));
	memset((uint8_t *)out + sizeof(*out), 0x22, size - sizeof(*out));
	ctx->objdata = 0;
	ctx->object = out;
	ctx->objmask = NULL;
	return len;
}

/** Parse tokens for indirect action commands. */
static int
parse_ia(struct context *ctx, const struct token *token,
	 const char *str, unsigned int len,
	 void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return len;
	if (out->command == 0) {
		if (ctx->curr != RTE_FLOW_PARSER_CMD_INDIRECT_ACTION)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		out->args.vc.data = (uint8_t *)out + size;
		return len;
	}
	switch (ctx->curr) {
	case RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_CREATE:
	case RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_UPDATE:
	case RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_QUERY_UPDATE:
		out->args.vc.actions =
			(void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
					       sizeof(double));
		out->args.vc.attr.group = UINT32_MAX;
		/* fallthrough */
	case RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_QUERY:
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		return len;
	case RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_EGRESS:
		out->args.vc.attr.egress = 1;
		return len;
	case RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_INGRESS:
		out->args.vc.attr.ingress = 1;
		return len;
	case RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_TRANSFER:
		out->args.vc.attr.transfer = 1;
		return len;
	case RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_QU_MODE:
		return len;
	case RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_LIST:
		out->command = RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_LIST_CREATE;
		return len;
	case RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_FLOW_CONF:
		out->command = RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_FLOW_CONF_CREATE;
		return len;
	default:
		return -1;
	}
}

/** Parse tokens for indirect action destroy command. */
static int
parse_ia_destroy(struct context *ctx, const struct token *token,
		 const char *str, unsigned int len,
		 void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;
	uint32_t *action_id;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return len;
	if (out->command == 0 ||
	    out->command == RTE_FLOW_PARSER_CMD_INDIRECT_ACTION) {
		if (ctx->curr != RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_DESTROY)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		out->args.ia_destroy.action_id =
			(void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
					       sizeof(double));
		return len;
	}
	action_id = out->args.ia_destroy.action_id
		    + out->args.ia_destroy.action_id_n++;
	if ((uint8_t *)action_id > (uint8_t *)out + size)
		return -1;
	ctx->objdata = 0;
	ctx->object = action_id;
	ctx->objmask = NULL;
	return len;
}

/** Parse tokens for indirect action commands. */
static int
parse_qia(struct context *ctx, const struct token *token,
	  const char *str, unsigned int len,
	  void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return len;
	if (out->command == 0) {
		if (ctx->curr != RTE_FLOW_PARSER_CMD_QUEUE)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->args.vc.data = (uint8_t *)out + size;
		return len;
	}
	switch (ctx->curr) {
	case RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION:
		return len;
	case RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_CREATE:
	case RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_UPDATE:
	case RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_QUERY_UPDATE:
		out->args.vc.actions =
			(void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
					       sizeof(double));
		out->args.vc.attr.group = UINT32_MAX;
		/* fallthrough */
	case RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_QUERY:
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		return len;
	case RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_EGRESS:
		out->args.vc.attr.egress = 1;
		return len;
	case RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_INGRESS:
		out->args.vc.attr.ingress = 1;
		return len;
	case RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_TRANSFER:
		out->args.vc.attr.transfer = 1;
		return len;
	case RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_CREATE_POSTPONE:
		return len;
	case RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_QU_MODE:
		return len;
	case RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_LIST:
		out->command = RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_LIST_CREATE;
		return len;
	default:
		return -1;
	}
}

/** Parse tokens for indirect action destroy command. */
static int
parse_qia_destroy(struct context *ctx, const struct token *token,
		  const char *str, unsigned int len,
		  void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;
	uint32_t *action_id;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return len;
	if (out->command == 0 || out->command == RTE_FLOW_PARSER_CMD_QUEUE) {
		if (ctx->curr != RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_DESTROY)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		out->args.ia_destroy.action_id =
			(void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
					       sizeof(double));
		return len;
	}
	switch (ctx->curr) {
	case RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION:
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		return len;
	case RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_DESTROY_ID:
		action_id = out->args.ia_destroy.action_id
				+ out->args.ia_destroy.action_id_n++;
		if ((uint8_t *)action_id > (uint8_t *)out + size)
			return -1;
		ctx->objdata = 0;
		ctx->object = action_id;
		ctx->objmask = NULL;
		return len;
	case RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_DESTROY_POSTPONE:
		return len;
	default:
		return -1;
	}
}

/** Parse tokens for meter policy action commands. */
static int
parse_mp(struct context *ctx, const struct token *token,
	const char *str, unsigned int len,
	void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return len;
	if (out->command == 0) {
		if (ctx->curr != RTE_FLOW_PARSER_CMD_ITEM_POL_POLICY)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		out->args.vc.data = (uint8_t *)out + size;
		return len;
	}
	switch (ctx->curr) {
	case RTE_FLOW_PARSER_CMD_ACTION_POL_G:
		out->args.vc.actions =
			(void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
					sizeof(double));
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		return len;
	default:
		return -1;
	}
}

/** Parse tokens for validate/create commands. */
static int
parse_vc(struct context *ctx, const struct token *token,
	 const char *str, unsigned int len,
	 void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;
	uint8_t *data;
	uint32_t data_size;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return len;
	if (out->command == 0) {
		if (ctx->curr != RTE_FLOW_PARSER_CMD_VALIDATE && ctx->curr != RTE_FLOW_PARSER_CMD_CREATE &&
		    ctx->curr != RTE_FLOW_PARSER_CMD_PATTERN_TEMPLATE_CREATE &&
		    ctx->curr != RTE_FLOW_PARSER_CMD_ACTIONS_TEMPLATE_CREATE &&
		    ctx->curr != RTE_FLOW_PARSER_CMD_UPDATE)
			return -1;
		if (ctx->curr == RTE_FLOW_PARSER_CMD_UPDATE)
			out->args.vc.pattern =
				(void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
						       sizeof(double));
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		out->args.vc.data = (uint8_t *)out + size;
		return len;
	}
	ctx->objdata = 0;
	switch (ctx->curr) {
	default:
		ctx->object = &out->args.vc.attr;
		break;
	case RTE_FLOW_PARSER_CMD_VC_TUNNEL_SET:
	case RTE_FLOW_PARSER_CMD_VC_TUNNEL_MATCH:
		ctx->object = &out->args.vc.tunnel_ops;
		break;
	case RTE_FLOW_PARSER_CMD_VC_USER_ID:
		ctx->object = out;
		break;
	}
	ctx->objmask = NULL;
	switch (ctx->curr) {
	case RTE_FLOW_PARSER_CMD_VC_GROUP:
	case RTE_FLOW_PARSER_CMD_VC_PRIORITY:
	case RTE_FLOW_PARSER_CMD_VC_USER_ID:
		return len;
	case RTE_FLOW_PARSER_CMD_VC_TUNNEL_SET:
		out->args.vc.tunnel_ops.enabled = 1;
		out->args.vc.tunnel_ops.actions = 1;
		return len;
	case RTE_FLOW_PARSER_CMD_VC_TUNNEL_MATCH:
		out->args.vc.tunnel_ops.enabled = 1;
		out->args.vc.tunnel_ops.items = 1;
		return len;
	case RTE_FLOW_PARSER_CMD_VC_INGRESS:
		out->args.vc.attr.ingress = 1;
		return len;
	case RTE_FLOW_PARSER_CMD_VC_EGRESS:
		out->args.vc.attr.egress = 1;
		return len;
	case RTE_FLOW_PARSER_CMD_VC_TRANSFER:
		out->args.vc.attr.transfer = 1;
		return len;
	case RTE_FLOW_PARSER_CMD_ITEM_PATTERN:
		out->args.vc.pattern =
			(void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
					       sizeof(double));
		ctx->object = out->args.vc.pattern;
		ctx->objmask = NULL;
		return len;
	case RTE_FLOW_PARSER_CMD_ITEM_END:
		if ((out->command == RTE_FLOW_PARSER_CMD_VALIDATE ||
		     out->command == RTE_FLOW_PARSER_CMD_CREATE) && ctx->last)
			return -1;
		if (out->command == RTE_FLOW_PARSER_CMD_PATTERN_TEMPLATE_CREATE &&
		    !ctx->last)
			return -1;
		break;
	case RTE_FLOW_PARSER_CMD_ACTIONS:
		out->args.vc.actions = out->args.vc.pattern ?
			(void *)RTE_ALIGN_CEIL((uintptr_t)
					       (out->args.vc.pattern +
						out->args.vc.pattern_n),
					       sizeof(double)) :
			(void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
					       sizeof(double));
		ctx->object = out->args.vc.actions;
		ctx->objmask = NULL;
		return len;
	case RTE_FLOW_PARSER_CMD_VC_IS_USER_ID:
		out->args.vc.is_user_id = true;
		return len;
	default:
		if (token->priv == NULL)
			return -1;
		break;
	}
	if (out->args.vc.actions == NULL) {
		const struct parse_item_priv *priv = token->priv;
		struct rte_flow_item *item =
			out->args.vc.pattern + out->args.vc.pattern_n;

		data_size = priv->size * 3; /* spec, last, mask */
		data = (void *)RTE_ALIGN_FLOOR((uintptr_t)
					       (out->args.vc.data - data_size),
					       sizeof(double));
		if ((uint8_t *)item + sizeof(*item) > data)
			return -1;
		*item = (struct rte_flow_item){
			.type = priv->type,
		};
		++out->args.vc.pattern_n;
		ctx->object = item;
		ctx->objmask = NULL;
	} else {
		const struct parse_action_priv *priv = token->priv;
		struct rte_flow_action *action =
			out->args.vc.actions + out->args.vc.actions_n;

		data_size = priv->size; /* configuration */
		data = (void *)RTE_ALIGN_FLOOR((uintptr_t)
					       (out->args.vc.data - data_size),
					       sizeof(double));
		if ((uint8_t *)action + sizeof(*action) > data)
			return -1;
		*action = (struct rte_flow_action){
			.type = priv->type,
			.conf = data_size ? data : NULL,
		};
		++out->args.vc.actions_n;
		ctx->object = action;
		ctx->objmask = NULL;
	}
	memset(data, 0, data_size);
	out->args.vc.data = data;
	ctx->objdata = data_size;
	return len;
}

/** Parse pattern item parameter type. */
static int
parse_vc_spec(struct context *ctx, const struct token *token,
	      const char *str, unsigned int len,
	      void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;
	struct rte_flow_item *item;
	uint32_t data_size;
	int index;
	int objmask = 0;

	(void)size;
	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Parse parameter types. */
	switch (ctx->curr) {
	case RTE_FLOW_PARSER_CMD_ITEM_PARAM_IS:
		index = 0;
		objmask = 1;
		break;
	case RTE_FLOW_PARSER_CMD_ITEM_PARAM_SPEC:
		index = 0;
		break;
	case RTE_FLOW_PARSER_CMD_ITEM_PARAM_LAST:
		index = 1;
		break;
	case RTE_FLOW_PARSER_CMD_ITEM_PARAM_PREFIX:
		/* Modify next token to expect a prefix. */
		if (ctx->next_num < 2)
			return -1;
		ctx->next[ctx->next_num - 2] = NEXT_ENTRY(RTE_FLOW_PARSER_CMD_COMMON_PREFIX);
		/* Fall through. */
	case RTE_FLOW_PARSER_CMD_ITEM_PARAM_MASK:
		index = 2;
		break;
	default:
		return -1;
	}
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return len;
	if (out->args.vc.pattern_n == 0)
		return -1;
	item = &out->args.vc.pattern[out->args.vc.pattern_n - 1];
	data_size = ctx->objdata / 3; /* spec, last, mask */
	/* Point to selected object. */
	ctx->object = out->args.vc.data + (data_size * index);
	if (objmask != 0) {
		ctx->objmask = out->args.vc.data + (data_size * 2); /* mask */
		item->mask = ctx->objmask;
	} else
		ctx->objmask = NULL;
	/* Update relevant item pointer. */
	*((const void **[]){ &item->spec, &item->last, &item->mask })[index] =
		ctx->object;
	return len;
}

/** Parse action configuration field. */
static int
parse_vc_conf(struct context *ctx, const struct token *token,
	      const char *str, unsigned int len,
	      void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;

	(void)size;
	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return len;
	/* Point to selected object. */
	ctx->object = out->args.vc.data;
	ctx->objmask = NULL;
	return len;
}

/** Parse action configuration field. */
static int
parse_vc_conf_timeout(struct context *ctx, const struct token *token,
		      const char *str, unsigned int len,
		      void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;
	struct rte_flow_update_age *update;

	(void)size;
	if (ctx->curr != RTE_FLOW_PARSER_CMD_ACTION_AGE_UPDATE_TIMEOUT)
		return -1;
	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return len;
	/* Point to selected object. */
	ctx->object = out->args.vc.data;
	ctx->objmask = NULL;
	/* Update the timeout is valid. */
	update = (struct rte_flow_update_age *)out->args.vc.data;
	update->timeout_valid = 1;
	return len;
}

/** Parse eCPRI common header type field. */
static int
parse_vc_item_ecpri_type(struct context *ctx, const struct token *token,
			 const char *str, unsigned int len,
			 void *buf, unsigned int size)
{
	struct rte_flow_item_ecpri *ecpri;
	struct rte_flow_item_ecpri *ecpri_mask;
	struct rte_flow_item *item;
	uint32_t data_size;
	uint8_t msg_type;
	struct rte_flow_parser_output *out = buf;
	const struct arg *arg;

	(void)size;
	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	switch (ctx->curr) {
	case RTE_FLOW_PARSER_CMD_ITEM_ECPRI_COMMON_TYPE_IQ_DATA:
		msg_type = RTE_ECPRI_MSG_TYPE_IQ_DATA;
		break;
	case RTE_FLOW_PARSER_CMD_ITEM_ECPRI_COMMON_TYPE_RTC_CTRL:
		msg_type = RTE_ECPRI_MSG_TYPE_RTC_CTRL;
		break;
	case RTE_FLOW_PARSER_CMD_ITEM_ECPRI_COMMON_TYPE_DLY_MSR:
		msg_type = RTE_ECPRI_MSG_TYPE_DLY_MSR;
		break;
	default:
		return -1;
	}
	if (ctx->object == NULL)
		return len;
	arg = pop_args(ctx);
	if (arg == NULL)
		return -1;
	ecpri = (struct rte_flow_item_ecpri *)out->args.vc.data;
	ecpri->hdr.common.type = msg_type;
	data_size = ctx->objdata / 3; /* spec, last, mask */
	ecpri_mask = (struct rte_flow_item_ecpri *)(out->args.vc.data +
						    (data_size * 2));
	ecpri_mask->hdr.common.type = 0xFF;
	if (arg->hton != 0) {
		ecpri->hdr.common.u32 = rte_cpu_to_be_32(ecpri->hdr.common.u32);
		ecpri_mask->hdr.common.u32 =
				rte_cpu_to_be_32(ecpri_mask->hdr.common.u32);
	}
	item = &out->args.vc.pattern[out->args.vc.pattern_n - 1];
	item->spec = ecpri;
	item->mask = ecpri_mask;
	return len;
}

/** Parse L2TPv2 common header type field. */
static int
parse_vc_item_l2tpv2_type(struct context *ctx, const struct token *token,
			 const char *str, unsigned int len,
			 void *buf, unsigned int size)
{
	struct rte_flow_item_l2tpv2 *l2tpv2;
	struct rte_flow_item_l2tpv2 *l2tpv2_mask;
	struct rte_flow_item *item;
	uint32_t data_size;
	uint16_t msg_type = 0;
	struct rte_flow_parser_output *out = buf;
	const struct arg *arg;

	(void)size;
	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	switch (ctx->curr) {
	case RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_TYPE_DATA:
		msg_type |= RTE_L2TPV2_MSG_TYPE_DATA;
		break;
	case RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_TYPE_DATA_L:
		msg_type |= RTE_L2TPV2_MSG_TYPE_DATA_L;
		break;
	case RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_TYPE_DATA_S:
		msg_type |= RTE_L2TPV2_MSG_TYPE_DATA_S;
		break;
	case RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_TYPE_DATA_O:
		msg_type |= RTE_L2TPV2_MSG_TYPE_DATA_O;
		break;
	case RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_TYPE_DATA_L_S:
		msg_type |= RTE_L2TPV2_MSG_TYPE_DATA_L_S;
		break;
	case RTE_FLOW_PARSER_CMD_ITEM_L2TPV2_TYPE_CTRL:
		msg_type |= RTE_L2TPV2_MSG_TYPE_CONTROL;
		break;
	default:
		return -1;
	}
	if (ctx->object == NULL)
		return len;
	arg = pop_args(ctx);
	if (arg == NULL)
		return -1;
	l2tpv2 = (struct rte_flow_item_l2tpv2 *)out->args.vc.data;
	l2tpv2->hdr.common.flags_version |= msg_type;
	data_size = ctx->objdata / 3; /* spec, last, mask */
	l2tpv2_mask = (struct rte_flow_item_l2tpv2 *)(out->args.vc.data +
						    (data_size * 2));
	l2tpv2_mask->hdr.common.flags_version = 0xFFFF;
	if (arg->hton != 0) {
		l2tpv2->hdr.common.flags_version =
			rte_cpu_to_be_16(l2tpv2->hdr.common.flags_version);
		l2tpv2_mask->hdr.common.flags_version =
		    rte_cpu_to_be_16(l2tpv2_mask->hdr.common.flags_version);
	}
	item = &out->args.vc.pattern[out->args.vc.pattern_n - 1];
	item->spec = l2tpv2;
	item->mask = l2tpv2_mask;
	return len;
}

/** Parse operation for compare match item. */
static int
parse_vc_compare_op(struct context *ctx, const struct token *token,
			 const char *str, unsigned int len, void *buf,
			 unsigned int size)
{
	struct rte_flow_item_compare *compare_item;
	unsigned int i;

	(void)token;
	(void)buf;
	(void)size;
	if (ctx->curr != RTE_FLOW_PARSER_CMD_ITEM_COMPARE_OP_VALUE)
		return -1;
	for (i = 0; compare_ops[i]; ++i)
		if (strcmp_partial(compare_ops[i], str, len) == 0)
			break;
	if (compare_ops[i] == NULL)
		return -1;
	if (ctx->object == NULL)
		return len;
	compare_item = ctx->object;
	compare_item->operation = (enum rte_flow_item_compare_op)i;
	return len;
}

/** Parse id for compare match item. */
static int
parse_vc_compare_field_id(struct context *ctx, const struct token *token,
			  const char *str, unsigned int len, void *buf,
			  unsigned int size)
{
	struct rte_flow_item_compare *compare_item;
	unsigned int i;

	(void)token;
	(void)buf;
	(void)size;
	if (ctx->curr != RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_A_TYPE_VALUE &&
		ctx->curr != RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_B_TYPE_VALUE)
		return -1;
	for (i = 0; flow_field_ids[i]; ++i)
		if (strcmp_partial(flow_field_ids[i], str, len) == 0)
			break;
	if (flow_field_ids[i] == NULL)
		return -1;
	if (ctx->object == NULL)
		return len;
	compare_item = ctx->object;
	if (ctx->curr == RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_A_TYPE_VALUE)
		compare_item->a.field = (enum rte_flow_field_id)i;
	else
		compare_item->b.field = (enum rte_flow_field_id)i;
	return len;
}

/** Parse level for compare match item. */
static int
parse_vc_compare_field_level(struct context *ctx, const struct token *token,
			     const char *str, unsigned int len, void *buf,
			     unsigned int size)
{
	struct rte_flow_item_compare *compare_item;
	struct rte_flow_item_flex_handle *flex_handle = NULL;
	uint32_t val;
	struct rte_flow_parser_output *out = buf;
	char *end;

	(void)token;
	(void)size;
	if (ctx->curr != RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_A_LEVEL_VALUE &&
		ctx->curr != RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_B_LEVEL_VALUE)
		return -1;
	if (ctx->object == NULL)
		return len;
	compare_item = ctx->object;
	errno = 0;
	val = strtoumax(str, &end, 0);
	if (errno || (size_t)(end - str) != len)
		return -1;
	/* No need to validate action template mask value */
	if (out->args.vc.masks != NULL) {
		if (ctx->curr == RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_A_LEVEL_VALUE)
			compare_item->a.level = val;
		else
			compare_item->b.level = val;
		return len;
	}
	if ((ctx->curr == RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_A_LEVEL_VALUE &&
	     compare_item->a.field == RTE_FLOW_FIELD_FLEX_ITEM) ||
	    (ctx->curr == RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_B_LEVEL_VALUE &&
	     compare_item->b.field == RTE_FLOW_FIELD_FLEX_ITEM)) {
		flex_handle = parser_flex_handle_get(ctx->port, val);
		if (flex_handle == NULL)
			return -1;
	}
	if (ctx->curr == RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_A_LEVEL_VALUE) {
		if (compare_item->a.field != RTE_FLOW_FIELD_FLEX_ITEM)
			compare_item->a.level = val;
		else
			compare_item->a.flex_handle = flex_handle;
	} else if (ctx->curr == RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_B_LEVEL_VALUE) {
		if (compare_item->b.field != RTE_FLOW_FIELD_FLEX_ITEM)
			compare_item->b.level = val;
		else
			compare_item->b.flex_handle = flex_handle;
	}
	return len;
}

/** Parse meter color action type. */
static int
parse_vc_action_meter_color_type(struct context *ctx, const struct token *token,
				const char *str, unsigned int len,
				void *buf, unsigned int size)
{
	struct rte_flow_action *action_data;
	struct rte_flow_action_meter_color *conf;
	enum rte_color color;

	(void)buf;
	(void)size;
	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	switch (ctx->curr) {
	case RTE_FLOW_PARSER_CMD_ACTION_METER_COLOR_GREEN:
		color = RTE_COLOR_GREEN;
	break;
	case RTE_FLOW_PARSER_CMD_ACTION_METER_COLOR_YELLOW:
		color = RTE_COLOR_YELLOW;
	break;
	case RTE_FLOW_PARSER_CMD_ACTION_METER_COLOR_RED:
		color = RTE_COLOR_RED;
	break;
	default:
		return -1;
	}

	if (ctx->object == NULL)
		return len;
	action_data = ctx->object;
	conf = (struct rte_flow_action_meter_color *)
					(uintptr_t)(action_data->conf);
	conf->color = color;
	return len;
}

/** Parse RSS action. */
static int
parse_vc_action_rss(struct context *ctx, const struct token *token,
		    const char *str, unsigned int len,
		    void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;
	struct rte_flow_action *action;
	struct action_rss_data *action_rss_data;
	unsigned int i;
	uint16_t rss_queue_n;
	int ret;

	ret = parse_vc(ctx, token, str, len, buf, size);
	if (ret < 0)
		return ret;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return ret;
	if (out->args.vc.actions_n == 0)
		return -1;
	action = &out->args.vc.actions[out->args.vc.actions_n - 1];
	/* Point to selected object. */
	ctx->object = out->args.vc.data;
	ctx->objmask = NULL;
	/* Set up default configuration. */
	action_rss_data = ctx->object;
	rss_queue_n = parser_rss_queue_count(ctx->port);
	if (rss_queue_n == 0)
		rss_queue_n = ACTION_RSS_QUEUE_NUM;
	*action_rss_data = (struct action_rss_data){
		.conf = (struct rte_flow_action_rss){
			.func = RTE_ETH_HASH_FUNCTION_DEFAULT,
			.level = 0,
			.types = RTE_ETH_RSS_IP,
			.key_len = 0,
			.queue_num = RTE_MIN(rss_queue_n, ACTION_RSS_QUEUE_NUM),
			.key = NULL,
			.queue = action_rss_data->queue,
		},
		.queue = { 0 },
	};
	for (i = 0; i < action_rss_data->conf.queue_num; ++i)
		action_rss_data->queue[i] = i;
	action->conf = &action_rss_data->conf;
	return ret;
}

/**
 * Parse func field for RSS action.
 *
 * The RTE_ETH_HASH_FUNCTION_* value to assign is derived from the
 * ACTION_RSS_FUNC_* index that called this function.
 */
static int
parse_vc_action_rss_func(struct context *ctx, const struct token *token,
			 const char *str, unsigned int len,
			 void *buf, unsigned int size)
{
	struct action_rss_data *action_rss_data;
	enum rte_eth_hash_function func;

	(void)buf;
	(void)size;
	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	switch (ctx->curr) {
	case RTE_FLOW_PARSER_CMD_ACTION_RSS_FUNC_DEFAULT:
		func = RTE_ETH_HASH_FUNCTION_DEFAULT;
		break;
	case RTE_FLOW_PARSER_CMD_ACTION_RSS_FUNC_TOEPLITZ:
		func = RTE_ETH_HASH_FUNCTION_TOEPLITZ;
		break;
	case RTE_FLOW_PARSER_CMD_ACTION_RSS_FUNC_SIMPLE_XOR:
		func = RTE_ETH_HASH_FUNCTION_SIMPLE_XOR;
		break;
	case RTE_FLOW_PARSER_CMD_ACTION_RSS_FUNC_SYMMETRIC_TOEPLITZ:
		func = RTE_ETH_HASH_FUNCTION_SYMMETRIC_TOEPLITZ;
		break;
	default:
		return -1;
	}
	if (ctx->object == NULL)
		return len;
	action_rss_data = ctx->object;
	action_rss_data->conf.func = func;
	return len;
}

/**
 * Parse type field for RSS action.
 *
 * Valid tokens are type field names and the "end" token.
 */
static int
parse_vc_action_rss_type(struct context *ctx, const struct token *token,
			  const char *str, unsigned int len,
			  void *buf, unsigned int size)
{
	struct action_rss_data *action_rss_data;
	const struct rte_eth_rss_type_info *tbl;
	unsigned int i;

	(void)token;
	(void)buf;
	(void)size;
	if (ctx->curr != RTE_FLOW_PARSER_CMD_ACTION_RSS_TYPE)
		return -1;
	if ((ctx->objdata >> 16) == 0 && ctx->object != NULL) {
		action_rss_data = ctx->object;
		action_rss_data->conf.types = 0;
	}
	if (strcmp_partial("end", str, len) == 0) {
		ctx->objdata &= 0xffff;
		return len;
	}
	tbl = rte_eth_rss_type_info_get();
	for (i = 0; tbl[i].str; ++i)
		if (strcmp_partial(tbl[i].str, str, len) == 0)
			break;
	if (tbl[i].str == NULL)
		return -1;
	ctx->objdata = 1 << 16 | (ctx->objdata & 0xffff);
	/* Repeat token. */
	if (ctx->next_num == RTE_DIM(ctx->next))
		return -1;
	ctx->next[ctx->next_num++] = NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_RSS_TYPE);
	if (ctx->object == NULL)
		return len;
	action_rss_data = ctx->object;
	action_rss_data->conf.types |= tbl[i].rss_type;
	return len;
}

/**
 * Parse queue field for RSS action.
 *
 * Valid tokens are queue indices and the "end" token.
 */
static int
parse_vc_action_rss_queue(struct context *ctx, const struct token *token,
			  const char *str, unsigned int len,
			  void *buf, unsigned int size)
{
	struct action_rss_data *action_rss_data;
	const struct arg *arg;
	int ret;
	int i;

	(void)token;
	(void)buf;
	(void)size;
	if (ctx->curr != RTE_FLOW_PARSER_CMD_ACTION_RSS_QUEUE)
		return -1;
	i = ctx->objdata >> 16;
	if (strcmp_partial("end", str, len) == 0) {
		ctx->objdata &= 0xffff;
		goto end;
	}
	if (i >= ACTION_RSS_QUEUE_NUM)
		return -1;
	arg = ARGS_ENTRY_ARB(offsetof(struct action_rss_data, queue) +
			     i * sizeof(action_rss_data->queue[i]),
			     sizeof(action_rss_data->queue[i]));
	if (push_args(ctx, arg) != 0)
		return -1;
	ret = parse_int(ctx, token, str, len, NULL, 0);
	if (ret < 0) {
		pop_args(ctx);
		return -1;
	}
	++i;
	ctx->objdata = i << 16 | (ctx->objdata & 0xffff);
	/* Repeat token. */
	if (ctx->next_num == RTE_DIM(ctx->next))
		return -1;
	ctx->next[ctx->next_num++] = NEXT_ENTRY(RTE_FLOW_PARSER_CMD_ACTION_RSS_QUEUE);
end:
	if (ctx->object == NULL)
		return len;
	action_rss_data = ctx->object;
	action_rss_data->conf.queue_num = i;
	action_rss_data->conf.queue = i ? action_rss_data->queue : NULL;
	return len;
}

/** Setup VXLAN encap configuration. */
static int
parse_setup_vxlan_encap_data(struct action_vxlan_encap_data *action_vxlan_encap_data)
{
	const struct rte_flow_parser_vxlan_encap_conf *conf =
		rte_flow_parser_vxlan_encap_conf();

	/* Set up default configuration. */
	*action_vxlan_encap_data = (struct action_vxlan_encap_data){
		.conf = (struct rte_flow_action_vxlan_encap){
			.definition = action_vxlan_encap_data->items,
		},
		.items = {
			{
				.type = RTE_FLOW_ITEM_TYPE_ETH,
				.spec = &action_vxlan_encap_data->item_eth,
				.mask = &rte_flow_item_eth_mask,
			},
			{
				.type = RTE_FLOW_ITEM_TYPE_VLAN,
				.spec = &action_vxlan_encap_data->item_vlan,
				.mask = &rte_flow_item_vlan_mask,
			},
			{
				.type = RTE_FLOW_ITEM_TYPE_IPV4,
				.spec = &action_vxlan_encap_data->item_ipv4,
				.mask = &rte_flow_item_ipv4_mask,
			},
			{
				.type = RTE_FLOW_ITEM_TYPE_UDP,
				.spec = &action_vxlan_encap_data->item_udp,
				.mask = &rte_flow_item_udp_mask,
			},
			{
				.type = RTE_FLOW_ITEM_TYPE_VXLAN,
				.spec = &action_vxlan_encap_data->item_vxlan,
				.mask = &rte_flow_item_vxlan_mask,
			},
			{
				.type = RTE_FLOW_ITEM_TYPE_END,
			},
		},
		.item_eth.hdr.ether_type = 0,
		.item_vlan = {
			.hdr.vlan_tci = conf->vlan_tci,
			.hdr.eth_proto = 0,
		},
		.item_ipv4.hdr = {
			.src_addr = conf->ipv4_src,
			.dst_addr = conf->ipv4_dst,
		},
		.item_udp.hdr = {
			.src_port = conf->udp_src,
			.dst_port = conf->udp_dst,
		},
		.item_vxlan.hdr.flags = 0,
	};
	rte_ether_addr_copy(&conf->eth_dst,
			    &action_vxlan_encap_data->item_eth.hdr.dst_addr);
	rte_ether_addr_copy(&conf->eth_src,
			    &action_vxlan_encap_data->item_eth.hdr.src_addr);
	if (conf->select_ipv4 == 0) {
		memcpy(&action_vxlan_encap_data->item_ipv6.hdr.src_addr,
		       &conf->ipv6_src,
		       sizeof(conf->ipv6_src));
		memcpy(&action_vxlan_encap_data->item_ipv6.hdr.dst_addr,
		       &conf->ipv6_dst,
		       sizeof(conf->ipv6_dst));
		action_vxlan_encap_data->items[2] = (struct rte_flow_item){
			.type = RTE_FLOW_ITEM_TYPE_IPV6,
			.spec = &action_vxlan_encap_data->item_ipv6,
			.mask = &rte_flow_item_ipv6_mask,
		};
	}
	if (conf->select_vlan == 0)
		action_vxlan_encap_data->items[1].type =
			RTE_FLOW_ITEM_TYPE_VOID;
	if (conf->select_tos_ttl != 0) {
		if (conf->select_ipv4 != 0) {
			static struct rte_flow_item_ipv4 ipv4_mask_tos;

			memcpy(&ipv4_mask_tos, &rte_flow_item_ipv4_mask,
			       sizeof(ipv4_mask_tos));
			ipv4_mask_tos.hdr.type_of_service = 0xff;
			ipv4_mask_tos.hdr.time_to_live = 0xff;
			action_vxlan_encap_data->item_ipv4.hdr.type_of_service =
					conf->ip_tos;
			action_vxlan_encap_data->item_ipv4.hdr.time_to_live =
					conf->ip_ttl;
			action_vxlan_encap_data->items[2].mask =
							&ipv4_mask_tos;
		} else {
			static struct rte_flow_item_ipv6 ipv6_mask_tos;

			memcpy(&ipv6_mask_tos, &rte_flow_item_ipv6_mask,
			       sizeof(ipv6_mask_tos));
			ipv6_mask_tos.hdr.vtc_flow |=
				RTE_BE32(0xfful << RTE_IPV6_HDR_TC_SHIFT);
			ipv6_mask_tos.hdr.hop_limits = 0xff;
			action_vxlan_encap_data->item_ipv6.hdr.vtc_flow |=
				rte_cpu_to_be_32
					((uint32_t)conf->ip_tos <<
					 RTE_IPV6_HDR_TC_SHIFT);
			action_vxlan_encap_data->item_ipv6.hdr.hop_limits =
					conf->ip_ttl;
			action_vxlan_encap_data->items[2].mask =
					&ipv6_mask_tos;
		}
	}
	memcpy(action_vxlan_encap_data->item_vxlan.hdr.vni, conf->vni,
	       RTE_DIM(conf->vni));
	return 0;
}

/** Parse VXLAN encap action. */
static int
parse_vc_action_vxlan_encap(struct context *ctx, const struct token *token,
			    const char *str, unsigned int len,
			    void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;
	struct rte_flow_action *action;
	struct action_vxlan_encap_data *action_vxlan_encap_data;
	int ret;

	ret = parse_vc(ctx, token, str, len, buf, size);
	if (ret < 0)
		return ret;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return ret;
	if (out->args.vc.actions_n == 0)
		return -1;
	action = &out->args.vc.actions[out->args.vc.actions_n - 1];
	/* Point to selected object. */
	ctx->object = out->args.vc.data;
	ctx->objmask = NULL;
	action_vxlan_encap_data = ctx->object;
	parse_setup_vxlan_encap_data(action_vxlan_encap_data);
	action->conf = &action_vxlan_encap_data->conf;
	return ret;
}

/** Setup NVGRE encap configuration. */
static int
parse_setup_nvgre_encap_data(struct action_nvgre_encap_data *action_nvgre_encap_data)
{
	const struct rte_flow_parser_nvgre_encap_conf *conf =
		rte_flow_parser_nvgre_encap_conf();

	/* Set up default configuration. */
	*action_nvgre_encap_data = (struct action_nvgre_encap_data){
		.conf = (struct rte_flow_action_nvgre_encap){
			.definition = action_nvgre_encap_data->items,
		},
		.items = {
			{
				.type = RTE_FLOW_ITEM_TYPE_ETH,
				.spec = &action_nvgre_encap_data->item_eth,
				.mask = &rte_flow_item_eth_mask,
			},
			{
				.type = RTE_FLOW_ITEM_TYPE_VLAN,
				.spec = &action_nvgre_encap_data->item_vlan,
				.mask = &rte_flow_item_vlan_mask,
			},
			{
				.type = RTE_FLOW_ITEM_TYPE_IPV4,
				.spec = &action_nvgre_encap_data->item_ipv4,
				.mask = &rte_flow_item_ipv4_mask,
			},
			{
				.type = RTE_FLOW_ITEM_TYPE_NVGRE,
				.spec = &action_nvgre_encap_data->item_nvgre,
				.mask = &rte_flow_item_nvgre_mask,
			},
			{
				.type = RTE_FLOW_ITEM_TYPE_END,
			},
		},
		.item_eth.hdr.ether_type = 0,
		.item_vlan = {
			.hdr.vlan_tci = conf->vlan_tci,
			.hdr.eth_proto = 0,
		},
		.item_ipv4.hdr = {
		       .src_addr = conf->ipv4_src,
		       .dst_addr = conf->ipv4_dst,
		},
		.item_nvgre.c_k_s_rsvd0_ver = RTE_BE16(0x2000),
		.item_nvgre.protocol = RTE_BE16(RTE_ETHER_TYPE_TEB),
		.item_nvgre.flow_id = 0,
	};
	rte_ether_addr_copy(&conf->eth_dst,
			    &action_nvgre_encap_data->item_eth.hdr.dst_addr);
	rte_ether_addr_copy(&conf->eth_src,
			    &action_nvgre_encap_data->item_eth.hdr.src_addr);
	if (conf->select_ipv4 == 0) {
		memcpy(&action_nvgre_encap_data->item_ipv6.hdr.src_addr,
		       &conf->ipv6_src,
		       sizeof(conf->ipv6_src));
		memcpy(&action_nvgre_encap_data->item_ipv6.hdr.dst_addr,
		       &conf->ipv6_dst,
		       sizeof(conf->ipv6_dst));
		action_nvgre_encap_data->items[2] = (struct rte_flow_item){
			.type = RTE_FLOW_ITEM_TYPE_IPV6,
			.spec = &action_nvgre_encap_data->item_ipv6,
			.mask = &rte_flow_item_ipv6_mask,
		};
	}
	if (conf->select_vlan == 0)
		action_nvgre_encap_data->items[1].type =
			RTE_FLOW_ITEM_TYPE_VOID;
	memcpy(action_nvgre_encap_data->item_nvgre.tni, conf->tni,
	       RTE_DIM(conf->tni));
	return 0;
}

/** Parse NVGRE encap action. */
static int
parse_vc_action_nvgre_encap(struct context *ctx, const struct token *token,
			    const char *str, unsigned int len,
			    void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;
	struct rte_flow_action *action;
	struct action_nvgre_encap_data *action_nvgre_encap_data;
	int ret;

	ret = parse_vc(ctx, token, str, len, buf, size);
	if (ret < 0)
		return ret;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return ret;
	if (out->args.vc.actions_n == 0)
		return -1;
	action = &out->args.vc.actions[out->args.vc.actions_n - 1];
	/* Point to selected object. */
	ctx->object = out->args.vc.data;
	ctx->objmask = NULL;
	action_nvgre_encap_data = ctx->object;
	parse_setup_nvgre_encap_data(action_nvgre_encap_data);
	action->conf = &action_nvgre_encap_data->conf;
	return ret;
}

/** Parse l2 encap action. */
static int
parse_vc_action_l2_encap(struct context *ctx, const struct token *token,
			 const char *str, unsigned int len,
			 void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;
	struct rte_flow_action *action;
	struct action_raw_encap_data *action_encap_data;
	const struct rte_flow_parser_l2_encap_conf *conf =
		rte_flow_parser_l2_encap_conf();
	struct rte_flow_item_eth eth = { .hdr.ether_type = 0, };
	struct rte_flow_item_vlan vlan = {
		.hdr.vlan_tci = conf->vlan_tci,
		.hdr.eth_proto = 0,
	};
	uint8_t *header;
	int ret;

	ret = parse_vc(ctx, token, str, len, buf, size);
	if (ret < 0)
		return ret;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return ret;
	if (out->args.vc.actions_n == 0)
		return -1;
	action = &out->args.vc.actions[out->args.vc.actions_n - 1];
	/* Point to selected object. */
	ctx->object = out->args.vc.data;
	ctx->objmask = NULL;
	/* Copy the headers to the buffer. */
	action_encap_data = ctx->object;
	*action_encap_data = (struct action_raw_encap_data) {
		.conf = (struct rte_flow_action_raw_encap){
			.data = action_encap_data->data,
		},
		.data = {},
	};
	header = action_encap_data->data;
	if (conf->select_vlan != 0)
		eth.hdr.ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN);
	else if (conf->select_ipv4 != 0)
		eth.hdr.ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
	else
		eth.hdr.ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6);
	rte_ether_addr_copy(&conf->eth_dst, &eth.hdr.dst_addr);
	rte_ether_addr_copy(&conf->eth_src, &eth.hdr.src_addr);
	memcpy(header, &eth.hdr, sizeof(struct rte_ether_hdr));
	header += sizeof(struct rte_ether_hdr);
	if (conf->select_vlan != 0) {
		if (conf->select_ipv4 != 0)
			vlan.hdr.eth_proto = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
		else
			vlan.hdr.eth_proto = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6);
		memcpy(header, &vlan.hdr, sizeof(struct rte_vlan_hdr));
		header += sizeof(struct rte_vlan_hdr);
	}
	action_encap_data->conf.size = header -
		action_encap_data->data;
	action->conf = &action_encap_data->conf;
	return ret;
}

/** Parse l2 decap action. */
static int
parse_vc_action_l2_decap(struct context *ctx, const struct token *token,
			 const char *str, unsigned int len,
			 void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;
	struct rte_flow_action *action;
	struct action_raw_decap_data *action_decap_data;
	const struct rte_flow_parser_l2_decap_conf *conf =
		rte_flow_parser_l2_decap_conf();
	const struct rte_flow_parser_mplsoudp_encap_conf *mpls_conf =
		rte_flow_parser_mplsoudp_encap_conf();
	struct rte_flow_item_eth eth = { .hdr.ether_type = 0, };
	struct rte_flow_item_vlan vlan = {
		.hdr.vlan_tci = mpls_conf->vlan_tci,
		.hdr.eth_proto = 0,
	};
	uint8_t *header;
	int ret;

	ret = parse_vc(ctx, token, str, len, buf, size);
	if (ret < 0)
		return ret;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return ret;
	if (out->args.vc.actions_n == 0)
		return -1;
	action = &out->args.vc.actions[out->args.vc.actions_n - 1];
	/* Point to selected object. */
	ctx->object = out->args.vc.data;
	ctx->objmask = NULL;
	/* Copy the headers to the buffer. */
	action_decap_data = ctx->object;
	*action_decap_data = (struct action_raw_decap_data) {
		.conf = (struct rte_flow_action_raw_decap){
			.data = action_decap_data->data,
		},
		.data = {},
	};
	header = action_decap_data->data;
	if (conf->select_vlan != 0)
		eth.hdr.ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN);
	memcpy(header, &eth.hdr, sizeof(struct rte_ether_hdr));
	header += sizeof(struct rte_ether_hdr);
	if (conf->select_vlan != 0) {
		memcpy(header, &vlan.hdr, sizeof(struct rte_vlan_hdr));
		header += sizeof(struct rte_vlan_hdr);
	}
	action_decap_data->conf.size = header -
		action_decap_data->data;
	action->conf = &action_decap_data->conf;
	return ret;
}

#define ETHER_TYPE_MPLS_UNICAST 0x8847

/** Parse MPLSOGRE encap action. */
static int
parse_vc_action_mplsogre_encap(struct context *ctx, const struct token *token,
			       const char *str, unsigned int len,
			       void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;
	struct rte_flow_action *action;
	struct action_raw_encap_data *action_encap_data;
	const struct rte_flow_parser_mplsogre_encap_conf *conf =
		rte_flow_parser_mplsogre_encap_conf();
	struct rte_flow_item_eth eth = { .hdr.ether_type = 0, };
	struct rte_flow_item_vlan vlan = {
		.hdr.vlan_tci = conf->vlan_tci,
		.hdr.eth_proto = 0,
	};
	struct rte_flow_item_ipv4 ipv4 = {
		.hdr =  {
			.src_addr = conf->ipv4_src,
			.dst_addr = conf->ipv4_dst,
			.next_proto_id = IPPROTO_GRE,
			.version_ihl = RTE_IPV4_VHL_DEF,
			.time_to_live = IPDEFTTL,
		},
	};
	struct rte_flow_item_ipv6 ipv6 = {
		.hdr =  {
			.proto = IPPROTO_GRE,
			.hop_limits = IPDEFTTL,
		},
	};
	struct rte_flow_item_gre gre = {
		.protocol = rte_cpu_to_be_16(ETHER_TYPE_MPLS_UNICAST),
	};
	struct rte_flow_item_mpls mpls = {
		.ttl = 0,
	};
	uint8_t *header;
	int ret;

	ret = parse_vc(ctx, token, str, len, buf, size);
	if (ret < 0)
		return ret;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return ret;
	if (out->args.vc.actions_n == 0)
		return -1;
	action = &out->args.vc.actions[out->args.vc.actions_n - 1];
	/* Point to selected object. */
	ctx->object = out->args.vc.data;
	ctx->objmask = NULL;
	/* Copy the headers to the buffer. */
	action_encap_data = ctx->object;
	*action_encap_data = (struct action_raw_encap_data) {
		.conf = (struct rte_flow_action_raw_encap){
			.data = action_encap_data->data,
		},
		.data = {},
		.preserve = {},
	};
	header = action_encap_data->data;
	if (conf->select_vlan != 0)
		eth.hdr.ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN);
	else if (conf->select_ipv4 != 0)
		eth.hdr.ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
	else
		eth.hdr.ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6);
	memcpy(eth.hdr.dst_addr.addr_bytes,
	       conf->eth_dst, RTE_ETHER_ADDR_LEN);
	memcpy(eth.hdr.src_addr.addr_bytes,
	       conf->eth_src, RTE_ETHER_ADDR_LEN);
	memcpy(header, &eth.hdr, sizeof(struct rte_ether_hdr));
	header += sizeof(struct rte_ether_hdr);
	if (conf->select_vlan != 0) {
		if (conf->select_ipv4 != 0)
			vlan.hdr.eth_proto = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
		else
			vlan.hdr.eth_proto = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6);
		memcpy(header, &vlan.hdr, sizeof(struct rte_vlan_hdr));
		header += sizeof(struct rte_vlan_hdr);
	}
	if (conf->select_ipv4 != 0) {
		memcpy(header, &ipv4, sizeof(ipv4));
		header += sizeof(ipv4);
	} else {
		memcpy(&ipv6.hdr.src_addr,
		       &conf->ipv6_src,
		       sizeof(conf->ipv6_src));
		memcpy(&ipv6.hdr.dst_addr,
		       &conf->ipv6_dst,
		       sizeof(conf->ipv6_dst));
		memcpy(header, &ipv6, sizeof(ipv6));
		header += sizeof(ipv6);
	}
	memcpy(header, &gre, sizeof(gre));
	header += sizeof(gre);
	memcpy(mpls.label_tc_s, conf->label, RTE_DIM(conf->label));
	mpls.label_tc_s[2] |= 0x1;
	memcpy(header, &mpls, sizeof(mpls));
	header += sizeof(mpls);
	action_encap_data->conf.size = header -
		action_encap_data->data;
	action->conf = &action_encap_data->conf;
	return ret;
}

/** Parse MPLSOGRE decap action. */
static int
parse_vc_action_mplsogre_decap(struct context *ctx, const struct token *token,
			       const char *str, unsigned int len,
			       void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;
	struct rte_flow_action *action;
	struct action_raw_decap_data *action_decap_data;
	const struct rte_flow_parser_mplsogre_decap_conf *conf =
		rte_flow_parser_mplsogre_decap_conf();
	const struct rte_flow_parser_mplsogre_encap_conf *enc_conf =
		rte_flow_parser_mplsogre_encap_conf();
	struct rte_flow_item_eth eth = { .hdr.ether_type = 0, };
	struct rte_flow_item_vlan vlan = {.hdr.vlan_tci = 0};
	struct rte_flow_item_ipv4 ipv4 = {
		.hdr =  {
			.next_proto_id = IPPROTO_GRE,
		},
	};
	struct rte_flow_item_ipv6 ipv6 = {
		.hdr =  {
			.proto = IPPROTO_GRE,
		},
	};
	struct rte_flow_item_gre gre = {
		.protocol = rte_cpu_to_be_16(ETHER_TYPE_MPLS_UNICAST),
	};
	struct rte_flow_item_mpls mpls;
	uint8_t *header;
	int ret;

	ret = parse_vc(ctx, token, str, len, buf, size);
	if (ret < 0)
		return ret;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return ret;
	if (out->args.vc.actions_n == 0)
		return -1;
	action = &out->args.vc.actions[out->args.vc.actions_n - 1];
	/* Point to selected object. */
	ctx->object = out->args.vc.data;
	ctx->objmask = NULL;
	/* Copy the headers to the buffer. */
	action_decap_data = ctx->object;
	*action_decap_data = (struct action_raw_decap_data) {
		.conf = (struct rte_flow_action_raw_decap){
			.data = action_decap_data->data,
		},
		.data = {},
	};
	header = action_decap_data->data;
	if (conf->select_vlan != 0)
		eth.hdr.ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN);
	else if (enc_conf->select_ipv4 != 0)
		eth.hdr.ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
	else
		eth.hdr.ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6);
	memcpy(eth.hdr.dst_addr.addr_bytes,
	       enc_conf->eth_dst, RTE_ETHER_ADDR_LEN);
	memcpy(eth.hdr.src_addr.addr_bytes,
	       enc_conf->eth_src, RTE_ETHER_ADDR_LEN);
	memcpy(header, &eth.hdr, sizeof(struct rte_ether_hdr));
	header += sizeof(struct rte_ether_hdr);
	if (enc_conf->select_vlan != 0) {
		if (enc_conf->select_ipv4 != 0)
			vlan.hdr.eth_proto = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
		else
			vlan.hdr.eth_proto = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6);
		memcpy(header, &vlan.hdr, sizeof(struct rte_vlan_hdr));
		header += sizeof(struct rte_vlan_hdr);
	}
	if (enc_conf->select_ipv4 != 0) {
		memcpy(header, &ipv4, sizeof(ipv4));
		header += sizeof(ipv4);
	} else {
		memcpy(header, &ipv6, sizeof(ipv6));
		header += sizeof(ipv6);
	}
	memcpy(header, &gre, sizeof(gre));
	header += sizeof(gre);
	memset(&mpls, 0, sizeof(mpls));
	memcpy(header, &mpls, sizeof(mpls));
	header += sizeof(mpls);
	action_decap_data->conf.size = header -
		action_decap_data->data;
	action->conf = &action_decap_data->conf;
	return ret;
}

/** Parse MPLSOUDP encap action. */
static int
parse_vc_action_mplsoudp_encap(struct context *ctx, const struct token *token,
			       const char *str, unsigned int len,
			       void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;
	struct rte_flow_action *action;
	struct action_raw_encap_data *action_encap_data;
	const struct rte_flow_parser_mplsoudp_encap_conf *conf =
		rte_flow_parser_mplsoudp_encap_conf();
	struct rte_flow_item_eth eth = { .hdr.ether_type = 0, };
	struct rte_flow_item_vlan vlan = {
		.hdr.vlan_tci = conf->vlan_tci,
		.hdr.eth_proto = 0,
	};
	struct rte_flow_item_ipv4 ipv4 = {
		.hdr =  {
			.src_addr = conf->ipv4_src,
			.dst_addr = conf->ipv4_dst,
			.next_proto_id = IPPROTO_UDP,
			.version_ihl = RTE_IPV4_VHL_DEF,
			.time_to_live = IPDEFTTL,
		},
	};
	struct rte_flow_item_ipv6 ipv6 = {
		.hdr =  {
			.proto = IPPROTO_UDP,
			.hop_limits = IPDEFTTL,
		},
	};
	struct rte_flow_item_udp udp = {
		.hdr = {
			.src_port = conf->udp_src,
			.dst_port = conf->udp_dst,
		},
	};
	struct rte_flow_item_mpls mpls;
	uint8_t *header;
	int ret;

	ret = parse_vc(ctx, token, str, len, buf, size);
	if (ret < 0)
		return ret;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return ret;
	if (out->args.vc.actions_n == 0)
		return -1;
	action = &out->args.vc.actions[out->args.vc.actions_n - 1];
	/* Point to selected object. */
	ctx->object = out->args.vc.data;
	ctx->objmask = NULL;
	/* Copy the headers to the buffer. */
	action_encap_data = ctx->object;
	*action_encap_data = (struct action_raw_encap_data) {
		.conf = (struct rte_flow_action_raw_encap){
			.data = action_encap_data->data,
		},
		.data = {},
		.preserve = {},
	};
	header = action_encap_data->data;
	if (conf->select_vlan != 0)
		eth.hdr.ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN);
	else if (conf->select_ipv4 != 0)
		eth.hdr.ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
	else
		eth.hdr.ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6);
	memcpy(eth.hdr.dst_addr.addr_bytes,
	       conf->eth_dst, RTE_ETHER_ADDR_LEN);
	memcpy(eth.hdr.src_addr.addr_bytes,
	       conf->eth_src, RTE_ETHER_ADDR_LEN);
	memcpy(header, &eth.hdr, sizeof(struct rte_ether_hdr));
	header += sizeof(struct rte_ether_hdr);
	if (conf->select_vlan != 0) {
		if (conf->select_ipv4 != 0)
			vlan.hdr.eth_proto = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
		else
			vlan.hdr.eth_proto = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6);
		memcpy(header, &vlan.hdr, sizeof(struct rte_vlan_hdr));
		header += sizeof(struct rte_vlan_hdr);
	}
	if (conf->select_ipv4 != 0) {
		memcpy(header, &ipv4, sizeof(ipv4));
		header += sizeof(ipv4);
	} else {
		memcpy(&ipv6.hdr.src_addr,
		       &conf->ipv6_src,
		       sizeof(conf->ipv6_src));
		memcpy(&ipv6.hdr.dst_addr,
		       &conf->ipv6_dst,
		       sizeof(conf->ipv6_dst));
		memcpy(header, &ipv6, sizeof(ipv6));
		header += sizeof(ipv6);
	}
	memcpy(header, &udp, sizeof(udp));
	header += sizeof(udp);
	memcpy(mpls.label_tc_s, conf->label, RTE_DIM(conf->label));
	mpls.label_tc_s[2] |= 0x1;
	memcpy(header, &mpls, sizeof(mpls));
	header += sizeof(mpls);
	action_encap_data->conf.size = header -
		action_encap_data->data;
	action->conf = &action_encap_data->conf;
	return ret;
}

/** Parse MPLSOUDP decap action. */
static int
parse_vc_action_mplsoudp_decap(struct context *ctx, const struct token *token,
			       const char *str, unsigned int len,
			       void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;
	struct rte_flow_action *action;
	struct action_raw_decap_data *action_decap_data;
	const struct rte_flow_parser_mplsoudp_decap_conf *conf =
		rte_flow_parser_mplsoudp_decap_conf();
	const struct rte_flow_parser_mplsoudp_encap_conf *enc_conf =
		rte_flow_parser_mplsoudp_encap_conf();
	struct rte_flow_item_eth eth = { .hdr.ether_type = 0, };
	struct rte_flow_item_vlan vlan = {.hdr.vlan_tci = 0};
	struct rte_flow_item_ipv4 ipv4 = {
		.hdr =  {
			.next_proto_id = IPPROTO_UDP,
		},
	};
	struct rte_flow_item_ipv6 ipv6 = {
		.hdr =  {
			.proto = IPPROTO_UDP,
		},
	};
	struct rte_flow_item_udp udp = {
		.hdr = {
			.dst_port = rte_cpu_to_be_16(6635),
		},
	};
	struct rte_flow_item_mpls mpls;
	uint8_t *header;
	int ret;

	ret = parse_vc(ctx, token, str, len, buf, size);
	if (ret < 0)
		return ret;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return ret;
	if (out->args.vc.actions_n == 0)
		return -1;
	action = &out->args.vc.actions[out->args.vc.actions_n - 1];
	/* Point to selected object. */
	ctx->object = out->args.vc.data;
	ctx->objmask = NULL;
	/* Copy the headers to the buffer. */
	action_decap_data = ctx->object;
	*action_decap_data = (struct action_raw_decap_data) {
		.conf = (struct rte_flow_action_raw_decap){
			.data = action_decap_data->data,
		},
		.data = {},
	};
	header = action_decap_data->data;
	if (conf->select_vlan != 0)
		eth.hdr.ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN);
	else if (enc_conf->select_ipv4 != 0)
		eth.hdr.ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
	else
		eth.hdr.ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6);
	memcpy(eth.hdr.dst_addr.addr_bytes,
	       enc_conf->eth_dst, RTE_ETHER_ADDR_LEN);
	memcpy(eth.hdr.src_addr.addr_bytes,
	       enc_conf->eth_src, RTE_ETHER_ADDR_LEN);
	memcpy(header, &eth.hdr, sizeof(struct rte_ether_hdr));
	header += sizeof(struct rte_ether_hdr);
	if (enc_conf->select_vlan != 0) {
		if (enc_conf->select_ipv4 != 0)
			vlan.hdr.eth_proto = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
		else
			vlan.hdr.eth_proto = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6);
		memcpy(header, &vlan.hdr, sizeof(struct rte_vlan_hdr));
		header += sizeof(struct rte_vlan_hdr);
	}
	if (enc_conf->select_ipv4 != 0) {
		memcpy(header, &ipv4, sizeof(ipv4));
		header += sizeof(ipv4);
	} else {
		memcpy(header, &ipv6, sizeof(ipv6));
		header += sizeof(ipv6);
	}
	memcpy(header, &udp, sizeof(udp));
	header += sizeof(udp);
	memset(&mpls, 0, sizeof(mpls));
	memcpy(header, &mpls, sizeof(mpls));
	header += sizeof(mpls);
	action_decap_data->conf.size = header -
		action_decap_data->data;
	action->conf = &action_decap_data->conf;
	return ret;
}

static int
parse_vc_action_raw_decap_index(struct context *ctx, const struct token *token,
				const char *str, unsigned int len, void *buf,
				unsigned int size)
{
	struct action_raw_decap_data *action_raw_decap_data;
	struct rte_flow_action *action;
	const struct arg *arg;
	struct rte_flow_parser_output *out = buf;
	int ret;
	uint16_t idx;

	RTE_SET_USED(token);
	RTE_SET_USED(buf);
	RTE_SET_USED(size);
	arg = ARGS_ENTRY_ARB_BOUNDED
		(offsetof(struct action_raw_decap_data, idx),
		 sizeof(((struct action_raw_decap_data *)0)->idx),
		 0, RAW_ENCAP_CONFS_MAX_NUM - 1);
	if (push_args(ctx, arg) != 0)
		return -1;
	ret = parse_int(ctx, token, str, len, NULL, 0);
	if (ret < 0) {
		pop_args(ctx);
		return -1;
	}
	if (ctx->object == NULL)
		return len;
	action = &out->args.vc.actions[out->args.vc.actions_n - 1];
	action_raw_decap_data = ctx->object;
	idx = action_raw_decap_data->idx;
	const struct rte_flow_action_raw_decap *conf =
		rte_flow_parser_raw_decap_conf_get(idx);

	if (conf == NULL)
		return -1;
	action_raw_decap_data->conf = *conf;
	action->conf = &action_raw_decap_data->conf;
	return len;
}

static int
parse_vc_action_raw_encap_index(struct context *ctx, const struct token *token,
				const char *str, unsigned int len, void *buf,
				unsigned int size)
{
	struct action_raw_encap_data *action_raw_encap_data;
	struct rte_flow_action *action;
	const struct arg *arg;
	struct rte_flow_parser_output *out = buf;
	int ret;
	uint16_t idx;

	RTE_SET_USED(token);
	RTE_SET_USED(buf);
	RTE_SET_USED(size);
	if (ctx->curr != RTE_FLOW_PARSER_CMD_ACTION_RAW_ENCAP_INDEX_VALUE)
		return -1;
	arg = ARGS_ENTRY_ARB_BOUNDED
		(offsetof(struct action_raw_encap_data, idx),
		 sizeof(((struct action_raw_encap_data *)0)->idx),
		 0, RAW_ENCAP_CONFS_MAX_NUM - 1);
	if (push_args(ctx, arg) != 0)
		return -1;
	ret = parse_int(ctx, token, str, len, NULL, 0);
	if (ret < 0) {
		pop_args(ctx);
		return -1;
	}
	if (ctx->object == NULL)
		return len;
	action = &out->args.vc.actions[out->args.vc.actions_n - 1];
	action_raw_encap_data = ctx->object;
	idx = action_raw_encap_data->idx;
	const struct rte_flow_action_raw_encap *conf =
		rte_flow_parser_raw_encap_conf_get(idx);

	if (conf == NULL)
		return -1;
	action_raw_encap_data->conf = *conf;
	action->conf = &action_raw_encap_data->conf;
	return len;
}

static int
parse_vc_action_raw_encap(struct context *ctx, const struct token *token,
			  const char *str, unsigned int len, void *buf,
			  unsigned int size)
{
	struct rte_flow_parser_output *out = buf;
	int ret;

	ret = parse_vc(ctx, token, str, len, buf, size);
	if (ret < 0)
		return ret;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return ret;
	if (out->args.vc.actions_n == 0)
		return -1;
	/* Point to selected object. */
	ctx->object = out->args.vc.data;
	ctx->objmask = NULL;
	return ret;
}

static int
parse_vc_action_raw_decap(struct context *ctx, const struct token *token,
			  const char *str, unsigned int len, void *buf,
			  unsigned int size)
{
	struct rte_flow_parser_output *out = buf;
	struct rte_flow_action *action;
	struct action_raw_decap_data *action_raw_decap_data = NULL;
	int ret;

	ret = parse_vc(ctx, token, str, len, buf, size);
	if (ret < 0)
		return ret;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return ret;
	if (out->args.vc.actions_n == 0)
		return -1;
	action = &out->args.vc.actions[out->args.vc.actions_n - 1];
	/* Point to selected object. */
	ctx->object = out->args.vc.data;
	ctx->objmask = NULL;
	/* Copy the headers to the buffer. */
	action_raw_decap_data = ctx->object;
	const struct rte_flow_action_raw_decap *conf =
		rte_flow_parser_raw_decap_conf_get(0);

	if (conf == NULL || conf->data == NULL || conf->size == 0)
		return -1;
	action_raw_decap_data->conf = *conf;
	action->conf = &action_raw_decap_data->conf;
	return ret;
}

static int
parse_vc_action_ipv6_ext_remove(struct context *ctx, const struct token *token,
				const char *str, unsigned int len, void *buf,
				unsigned int size)
{
	struct rte_flow_parser_output *out = buf;
	struct rte_flow_action *action;
	struct action_ipv6_ext_remove_data *ipv6_ext_remove_data = NULL;
	int ret;

	ret = parse_vc(ctx, token, str, len, buf, size);
	if (ret < 0)
		return ret;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return ret;
	if (out->args.vc.actions_n == 0)
		return -1;
	action = &out->args.vc.actions[out->args.vc.actions_n - 1];
	/* Point to selected object. */
	ctx->object = out->args.vc.data;
	ctx->objmask = NULL;
	/* Copy the headers to the buffer. */
	ipv6_ext_remove_data = ctx->object;
	const struct rte_flow_action_ipv6_ext_remove *conf =
		parser_ctx_ipv6_ext_remove_conf_get(0);

	if (conf == NULL)
		return -1;
	ipv6_ext_remove_data->conf = *conf;
	action->conf = &ipv6_ext_remove_data->conf;
	return ret;
}

static int
parse_vc_action_ipv6_ext_remove_index(struct context *ctx, const struct token *token,
				      const char *str, unsigned int len, void *buf,
				      unsigned int size)
{
	struct action_ipv6_ext_remove_data *action_ipv6_ext_remove_data;
	struct rte_flow_action *action;
	const struct arg *arg;
	struct rte_flow_parser_output *out = buf;
	int ret;
	uint16_t idx;

	RTE_SET_USED(token);
	RTE_SET_USED(buf);
	RTE_SET_USED(size);
	arg = ARGS_ENTRY_ARB_BOUNDED
		(offsetof(struct action_ipv6_ext_remove_data, idx),
		 sizeof(((struct action_ipv6_ext_remove_data *)0)->idx),
		 0, IPV6_EXT_PUSH_CONFS_MAX_NUM - 1);
	if (push_args(ctx, arg) != 0)
		return -1;
	ret = parse_int(ctx, token, str, len, NULL, 0);
	if (ret < 0) {
		pop_args(ctx);
		return -1;
	}
	if (ctx->object == NULL)
		return len;
	action = &out->args.vc.actions[out->args.vc.actions_n - 1];
	action_ipv6_ext_remove_data = ctx->object;
	idx = action_ipv6_ext_remove_data->idx;
	const struct rte_flow_action_ipv6_ext_remove *conf =
		parser_ctx_ipv6_ext_remove_conf_get(idx);

	if (conf == NULL)
		return -1;
	action_ipv6_ext_remove_data->conf = *conf;
	action->conf = &action_ipv6_ext_remove_data->conf;
	return len;
}

static int
parse_vc_action_ipv6_ext_push(struct context *ctx, const struct token *token,
			      const char *str, unsigned int len, void *buf,
			      unsigned int size)
{
	struct rte_flow_parser_output *out = buf;
	struct rte_flow_action *action;
	struct action_ipv6_ext_push_data *ipv6_ext_push_data = NULL;
	int ret;

	ret = parse_vc(ctx, token, str, len, buf, size);
	if (ret < 0)
		return ret;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return ret;
	if (out->args.vc.actions_n == 0)
		return -1;
	action = &out->args.vc.actions[out->args.vc.actions_n - 1];
	/* Point to selected object. */
	ctx->object = out->args.vc.data;
	ctx->objmask = NULL;
	/* Copy the headers to the buffer. */
	ipv6_ext_push_data = ctx->object;
	const struct rte_flow_action_ipv6_ext_push *conf =
		parser_ctx_ipv6_ext_push_conf_get(0);

	if (conf == NULL || conf->data == NULL || conf->size == 0)
		return -1;
	ipv6_ext_push_data->conf = *conf;
	action->conf = &ipv6_ext_push_data->conf;
	return ret;
}

static int
parse_vc_action_ipv6_ext_push_index(struct context *ctx, const struct token *token,
				    const char *str, unsigned int len, void *buf,
				    unsigned int size)
{
	struct action_ipv6_ext_push_data *action_ipv6_ext_push_data;
	struct rte_flow_action *action;
	const struct arg *arg;
	struct rte_flow_parser_output *out = buf;
	int ret;
	uint16_t idx;

	RTE_SET_USED(token);
	RTE_SET_USED(buf);
	RTE_SET_USED(size);
	arg = ARGS_ENTRY_ARB_BOUNDED
		(offsetof(struct action_ipv6_ext_push_data, idx),
		 sizeof(((struct action_ipv6_ext_push_data *)0)->idx),
		 0, IPV6_EXT_PUSH_CONFS_MAX_NUM - 1);
	if (push_args(ctx, arg) != 0)
		return -1;
	ret = parse_int(ctx, token, str, len, NULL, 0);
	if (ret < 0) {
		pop_args(ctx);
		return -1;
	}
	if (ctx->object == NULL)
		return len;
	action = &out->args.vc.actions[out->args.vc.actions_n - 1];
	action_ipv6_ext_push_data = ctx->object;
	idx = action_ipv6_ext_push_data->idx;
	const struct rte_flow_action_ipv6_ext_push *conf =
		parser_ctx_ipv6_ext_push_conf_get(idx);

	if (conf == NULL)
		return -1;
	action_ipv6_ext_push_data->conf = *conf;
	action->conf = &action_ipv6_ext_push_data->conf;
	return len;
}

static int
parse_vc_action_set_meta(struct context *ctx, const struct token *token,
			 const char *str, unsigned int len, void *buf,
			 unsigned int size)
{
	int ret;

	ret = parse_vc(ctx, token, str, len, buf, size);
	if (ret < 0)
		return ret;
	ret = rte_flow_dynf_metadata_register();
	if (ret < 0)
		return -1;
	return len;
}

static int
parse_vc_action_sample(struct context *ctx, const struct token *token,
			 const char *str, unsigned int len, void *buf,
			 unsigned int size)
{
	struct rte_flow_parser_output *out = buf;
	struct rte_flow_action *action;
	struct action_sample_data *action_sample_data = NULL;
	static struct rte_flow_action end_action = {
		RTE_FLOW_ACTION_TYPE_END, 0
	};
	int ret;

	ret = parse_vc(ctx, token, str, len, buf, size);
	if (ret < 0)
		return ret;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return ret;
	if (out->args.vc.actions_n == 0)
		return -1;
	action = &out->args.vc.actions[out->args.vc.actions_n - 1];
	/* Point to selected object. */
	ctx->object = out->args.vc.data;
	ctx->objmask = NULL;
	/* Copy the headers to the buffer. */
	action_sample_data = ctx->object;
	action_sample_data->conf.actions = &end_action;
	action->conf = &action_sample_data->conf;
	return ret;
}

static int
parse_vc_action_sample_index(struct context *ctx, const struct token *token,
				const char *str, unsigned int len, void *buf,
				unsigned int size)
{
	struct action_sample_data *action_sample_data;
	struct rte_flow_action *action;
	const struct rte_flow_action *actions;
	const struct arg *arg;
	struct rte_flow_parser_output *out = buf;
	int ret;
	uint16_t idx;

	RTE_SET_USED(token);
	RTE_SET_USED(buf);
	RTE_SET_USED(size);
	if (ctx->curr != RTE_FLOW_PARSER_CMD_ACTION_SAMPLE_INDEX_VALUE)
		return -1;
	arg = ARGS_ENTRY_ARB_BOUNDED
		(offsetof(struct action_sample_data, idx),
		 sizeof(((struct action_sample_data *)0)->idx),
		 0, RAW_SAMPLE_CONFS_MAX_NUM - 1);
	if (push_args(ctx, arg) != 0)
		return -1;
	ret = parse_int(ctx, token, str, len, NULL, 0);
	if (ret < 0) {
		pop_args(ctx);
		return -1;
	}
	if (ctx->object == NULL)
		return len;
	action = &out->args.vc.actions[out->args.vc.actions_n - 1];
	action_sample_data = ctx->object;
	idx = action_sample_data->idx;
	if (idx >= RAW_SAMPLE_CONFS_MAX_NUM)
		actions = NULL;
	else
		actions = parser.ctx.raw_sample_confs[idx].data;
	if (actions == NULL)
		return -1;
	action_sample_data->conf.actions = actions;
	action->conf = &action_sample_data->conf;
	return len;
}

/** Parse operation for modify_field command. */
static int
parse_vc_modify_field_op(struct context *ctx, const struct token *token,
			 const char *str, unsigned int len, void *buf,
			 unsigned int size)
{
	struct rte_flow_action_modify_field *action_modify_field;
	unsigned int i;

	(void)token;
	(void)buf;
	(void)size;
	if (ctx->curr != RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_OP_VALUE)
		return -1;
	for (i = 0; modify_field_ops[i]; ++i)
		if (strcmp_partial(modify_field_ops[i], str, len) == 0)
			break;
	if (modify_field_ops[i] == NULL)
		return -1;
	if (ctx->object == NULL)
		return len;
	action_modify_field = ctx->object;
	action_modify_field->operation = (enum rte_flow_modify_op)i;
	return len;
}

/** Parse id for modify_field command. */
static int
parse_vc_modify_field_id(struct context *ctx, const struct token *token,
			 const char *str, unsigned int len, void *buf,
			 unsigned int size)
{
	struct rte_flow_action_modify_field *action_modify_field;
	unsigned int i;

	(void)token;
	(void)buf;
	(void)size;
	if (ctx->curr != RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_DST_TYPE_VALUE &&
		ctx->curr != RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_SRC_TYPE_VALUE)
		return -1;
	for (i = 0; flow_field_ids[i]; ++i)
		if (strcmp_partial(flow_field_ids[i], str, len) == 0)
			break;
	if (flow_field_ids[i] == NULL)
		return -1;
	if (ctx->object == NULL)
		return len;
	action_modify_field = ctx->object;
	if (ctx->curr == RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_DST_TYPE_VALUE)
		action_modify_field->dst.field = (enum rte_flow_field_id)i;
	else
		action_modify_field->src.field = (enum rte_flow_field_id)i;
	return len;
}

/** Parse level for modify_field command. */
static int
parse_vc_modify_field_level(struct context *ctx, const struct token *token,
			 const char *str, unsigned int len, void *buf,
			 unsigned int size)
{
	struct rte_flow_action_modify_field *action;
	struct rte_flow_item_flex_handle *flex_handle = NULL;
	uint32_t val;
	struct rte_flow_parser_output *out = buf;
	char *end;

	(void)token;
	(void)size;
	if (ctx->curr != RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_DST_LEVEL_VALUE &&
		ctx->curr != RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_SRC_LEVEL_VALUE)
		return -1;
	if (ctx->object == NULL)
		return len;
	action = ctx->object;
	errno = 0;
	val = strtoumax(str, &end, 0);
	if (errno || (size_t)(end - str) != len)
		return -1;
	/* No need to validate action template mask value */
	if (out->args.vc.masks != NULL) {
		if (ctx->curr == RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_DST_LEVEL_VALUE)
			action->dst.level = val;
		else
			action->src.level = val;
		return len;
	}
	if ((ctx->curr == RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_DST_LEVEL_VALUE &&
	     action->dst.field == RTE_FLOW_FIELD_FLEX_ITEM) ||
	    (ctx->curr == RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_SRC_LEVEL_VALUE &&
	     action->src.field == RTE_FLOW_FIELD_FLEX_ITEM)) {
		flex_handle = parser_flex_handle_get(ctx->port, val);
		if (flex_handle == NULL)
			return -1;
	}
	if (ctx->curr == RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_DST_LEVEL_VALUE) {
		if (action->dst.field != RTE_FLOW_FIELD_FLEX_ITEM)
			action->dst.level = val;
		else
			action->dst.flex_handle = flex_handle;
	} else if (ctx->curr == RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_SRC_LEVEL_VALUE) {
		if (action->src.field != RTE_FLOW_FIELD_FLEX_ITEM)
			action->src.level = val;
		else
			action->src.flex_handle = flex_handle;
	}
	return len;
}

/** Parse the conntrack update, not a rte_flow_action. */
static int
parse_vc_action_conntrack_update(struct context *ctx, const struct token *token,
			 const char *str, unsigned int len, void *buf,
			 unsigned int size)
{
	struct rte_flow_parser_output *out = buf;
	struct rte_flow_modify_conntrack *ct_modify = NULL;

	(void)size;
	if (ctx->curr != RTE_FLOW_PARSER_CMD_ACTION_CONNTRACK_UPDATE_CTX &&
	    ctx->curr != RTE_FLOW_PARSER_CMD_ACTION_CONNTRACK_UPDATE_DIR)
		return -1;
	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return len;
	ct_modify = (struct rte_flow_modify_conntrack *)out->args.vc.data;
	if (ctx->curr == RTE_FLOW_PARSER_CMD_ACTION_CONNTRACK_UPDATE_DIR) {
		ct_modify->new_ct.is_original_dir =
				parser.ctx.conntrack_context.is_original_dir;
		ct_modify->direction = 1;
	} else {
		uint32_t old_dir;

		old_dir = ct_modify->new_ct.is_original_dir;
		memcpy(&ct_modify->new_ct, &parser.ctx.conntrack_context,
		       sizeof(parser.ctx.conntrack_context));
		ct_modify->new_ct.is_original_dir = old_dir;
		ct_modify->state = 1;
	}
	return len;
}

/** Parse tokens for destroy command. */
static int
parse_destroy(struct context *ctx, const struct token *token,
	      const char *str, unsigned int len,
	      void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return len;
	if (out->command == 0) {
		if (ctx->curr != RTE_FLOW_PARSER_CMD_DESTROY)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		out->args.destroy.rule =
			(void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
					       sizeof(double));
		return len;
	}
	if (ctx->curr == RTE_FLOW_PARSER_CMD_DESTROY_IS_USER_ID) {
		out->args.destroy.is_user_id = true;
		return len;
	}
	if (((uint8_t *)(out->args.destroy.rule + out->args.destroy.rule_n) +
	     sizeof(*out->args.destroy.rule)) > (uint8_t *)out + size)
		return -1;
	ctx->objdata = 0;
	ctx->object = out->args.destroy.rule + out->args.destroy.rule_n++;
	ctx->objmask = NULL;
	return len;
}

/** Parse tokens for flush command. */
static int
parse_flush(struct context *ctx, const struct token *token,
	    const char *str, unsigned int len,
	    void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return len;
	if (out->command == 0) {
		if (ctx->curr != RTE_FLOW_PARSER_CMD_FLUSH)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
	}
	return len;
}

/** Parse tokens for dump command. */
static int
parse_dump(struct context *ctx, const struct token *token,
	    const char *str, unsigned int len,
	    void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return len;
	if (out->command == 0) {
		if (ctx->curr != RTE_FLOW_PARSER_CMD_DUMP)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		return len;
	}
	switch (ctx->curr) {
	case RTE_FLOW_PARSER_CMD_DUMP_ALL:
	case RTE_FLOW_PARSER_CMD_DUMP_ONE:
		out->args.dump.mode = (ctx->curr == RTE_FLOW_PARSER_CMD_DUMP_ALL) ? true : false;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		return len;
	case RTE_FLOW_PARSER_CMD_DUMP_IS_USER_ID:
		out->args.dump.is_user_id = true;
		return len;
	default:
		return -1;
	}
}

/** Parse tokens for query command. */
static int
parse_query(struct context *ctx, const struct token *token,
	    const char *str, unsigned int len,
	    void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return len;
	if (out->command == 0) {
		if (ctx->curr != RTE_FLOW_PARSER_CMD_QUERY)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
	}
	if (ctx->curr == RTE_FLOW_PARSER_CMD_QUERY_IS_USER_ID) {
		out->args.query.is_user_id = true;
		return len;
	}
	return len;
}

/** Parse action names. */
static int
parse_action(struct context *ctx, const struct token *token,
	     const char *str, unsigned int len,
	     void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;
	const struct arg *arg = pop_args(ctx);
	unsigned int i;

	(void)size;
	/* Argument is expected. */
	if (arg == NULL)
		return -1;
	/* Parse action name. */
	for (i = 0; next_action[i]; ++i) {
		const struct parse_action_priv *priv;

		token = &token_list[next_action[i]];
		if (strcmp_partial(token->name, str, len) != 0)
			continue;
		priv = token->priv;
		if (priv == NULL)
			goto error;
		if (out != NULL)
			memcpy((uint8_t *)ctx->object + arg->offset,
			       &priv->type,
			       arg->size);
		return len;
	}
error:
	push_args(ctx, arg);
	return -1;
}

/** Parse tokens for list command. */
static int
parse_list(struct context *ctx, const struct token *token,
	   const char *str, unsigned int len,
	   void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return len;
	if (out->command == 0) {
		if (ctx->curr != RTE_FLOW_PARSER_CMD_LIST)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		out->args.list.group =
			(void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
					       sizeof(double));
		return len;
	}
	if (((uint8_t *)(out->args.list.group + out->args.list.group_n) +
	     sizeof(*out->args.list.group)) > (uint8_t *)out + size)
		return -1;
	ctx->objdata = 0;
	ctx->object = out->args.list.group + out->args.list.group_n++;
	ctx->objmask = NULL;
	return len;
}

/** Parse tokens for list all aged flows command. */
static int
parse_aged(struct context *ctx, const struct token *token,
	   const char *str, unsigned int len,
	   void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return len;
	if (out->command == 0 || out->command == RTE_FLOW_PARSER_CMD_QUEUE) {
		if (ctx->curr != RTE_FLOW_PARSER_CMD_AGED && ctx->curr != RTE_FLOW_PARSER_CMD_QUEUE_AGED)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
	}
	if (ctx->curr == RTE_FLOW_PARSER_CMD_AGED_DESTROY)
		out->args.aged.destroy = 1;
	return len;
}

/** Parse tokens for isolate command. */
static int
parse_isolate(struct context *ctx, const struct token *token,
	      const char *str, unsigned int len,
	      void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return len;
	if (out->command == 0) {
		if (ctx->curr != RTE_FLOW_PARSER_CMD_ISOLATE)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
	}
	return len;
}

/** Parse tokens for info/configure command. */
static int
parse_configure(struct context *ctx, const struct token *token,
		const char *str, unsigned int len,
		void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return len;
	if (out->command == 0) {
		if (ctx->curr != RTE_FLOW_PARSER_CMD_INFO && ctx->curr != RTE_FLOW_PARSER_CMD_CONFIGURE)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
	}
	return len;
}

/** Parse tokens for template create command. */
static int
parse_template(struct context *ctx, const struct token *token,
	       const char *str, unsigned int len,
	       void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return len;
	if (out->command == 0) {
		if (ctx->curr != RTE_FLOW_PARSER_CMD_PATTERN_TEMPLATE &&
		    ctx->curr != RTE_FLOW_PARSER_CMD_ACTIONS_TEMPLATE)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		out->args.vc.data = (uint8_t *)out + size;
		return len;
	}
	switch (ctx->curr) {
	case RTE_FLOW_PARSER_CMD_PATTERN_TEMPLATE_CREATE:
		out->args.vc.pattern =
			(void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
					       sizeof(double));
		out->args.vc.pat_templ_id = UINT32_MAX;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		return len;
	case RTE_FLOW_PARSER_CMD_PATTERN_TEMPLATE_EGRESS:
		out->args.vc.attr.egress = 1;
		return len;
	case RTE_FLOW_PARSER_CMD_PATTERN_TEMPLATE_INGRESS:
		out->args.vc.attr.ingress = 1;
		return len;
	case RTE_FLOW_PARSER_CMD_PATTERN_TEMPLATE_TRANSFER:
		out->args.vc.attr.transfer = 1;
		return len;
	case RTE_FLOW_PARSER_CMD_ACTIONS_TEMPLATE_CREATE:
		out->args.vc.act_templ_id = UINT32_MAX;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		return len;
	case RTE_FLOW_PARSER_CMD_ACTIONS_TEMPLATE_SPEC:
		out->args.vc.actions =
			(void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
					       sizeof(double));
		ctx->object = out->args.vc.actions;
		ctx->objmask = NULL;
		return len;
	case RTE_FLOW_PARSER_CMD_ACTIONS_TEMPLATE_MASK:
		out->args.vc.masks =
			(void *)RTE_ALIGN_CEIL((uintptr_t)
					       (out->args.vc.actions +
						out->args.vc.actions_n),
					       sizeof(double));
		ctx->object = out->args.vc.masks;
		ctx->objmask = NULL;
		return len;
	case RTE_FLOW_PARSER_CMD_ACTIONS_TEMPLATE_EGRESS:
		out->args.vc.attr.egress = 1;
		return len;
	case RTE_FLOW_PARSER_CMD_ACTIONS_TEMPLATE_INGRESS:
		out->args.vc.attr.ingress = 1;
		return len;
	case RTE_FLOW_PARSER_CMD_ACTIONS_TEMPLATE_TRANSFER:
		out->args.vc.attr.transfer = 1;
		return len;
	default:
		return -1;
	}
}

/** Parse tokens for template destroy command. */
static int
parse_template_destroy(struct context *ctx, const struct token *token,
		       const char *str, unsigned int len,
		       void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;
	uint32_t *template_id;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return len;
	if (out->command == 0 ||
		out->command == RTE_FLOW_PARSER_CMD_PATTERN_TEMPLATE ||
		out->command == RTE_FLOW_PARSER_CMD_ACTIONS_TEMPLATE) {
		if (ctx->curr != RTE_FLOW_PARSER_CMD_PATTERN_TEMPLATE_DESTROY &&
			ctx->curr != RTE_FLOW_PARSER_CMD_ACTIONS_TEMPLATE_DESTROY)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		out->args.templ_destroy.template_id =
			(void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
					       sizeof(double));
		return len;
	}
	template_id = out->args.templ_destroy.template_id
		    + out->args.templ_destroy.template_id_n++;
	if ((uint8_t *)template_id > (uint8_t *)out + size)
		return -1;
	ctx->objdata = 0;
	ctx->object = template_id;
	ctx->objmask = NULL;
	return len;
}

/** Parse tokens for table create command. */
static int
parse_table(struct context *ctx, const struct token *token,
	    const char *str, unsigned int len,
	    void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;
	uint32_t *template_id;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return len;
	if (out->command == 0) {
		if (ctx->curr != RTE_FLOW_PARSER_CMD_TABLE)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		return len;
	}
	switch (ctx->curr) {
	case RTE_FLOW_PARSER_CMD_TABLE_CREATE:
	case RTE_FLOW_PARSER_CMD_TABLE_RESIZE:
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		out->args.table.id = UINT32_MAX;
		return len;
	case RTE_FLOW_PARSER_CMD_TABLE_PATTERN_TEMPLATE:
		out->args.table.pat_templ_id =
			(void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
					       sizeof(double));
		template_id = out->args.table.pat_templ_id
				+ out->args.table.pat_templ_id_n++;
		if ((uint8_t *)template_id > (uint8_t *)out + size)
			return -1;
		ctx->objdata = 0;
		ctx->object = template_id;
		ctx->objmask = NULL;
		return len;
	case RTE_FLOW_PARSER_CMD_TABLE_ACTIONS_TEMPLATE:
		out->args.table.act_templ_id =
			(void *)RTE_ALIGN_CEIL((uintptr_t)
					       (out->args.table.pat_templ_id +
						out->args.table.pat_templ_id_n),
					       sizeof(double));
		template_id = out->args.table.act_templ_id
				+ out->args.table.act_templ_id_n++;
		if ((uint8_t *)template_id > (uint8_t *)out + size)
			return -1;
		ctx->objdata = 0;
		ctx->object = template_id;
		ctx->objmask = NULL;
		return len;
	case RTE_FLOW_PARSER_CMD_TABLE_INGRESS:
		out->args.table.attr.flow_attr.ingress = 1;
		return len;
	case RTE_FLOW_PARSER_CMD_TABLE_EGRESS:
		out->args.table.attr.flow_attr.egress = 1;
		return len;
	case RTE_FLOW_PARSER_CMD_TABLE_TRANSFER:
		out->args.table.attr.flow_attr.transfer = 1;
		return len;
	case RTE_FLOW_PARSER_CMD_TABLE_TRANSFER_WIRE_ORIG:
		if (out->args.table.attr.flow_attr.transfer == 0)
			return -1;
		out->args.table.attr.specialize |= RTE_FLOW_TABLE_SPECIALIZE_TRANSFER_WIRE_ORIG;
		return len;
	case RTE_FLOW_PARSER_CMD_TABLE_TRANSFER_VPORT_ORIG:
		if (out->args.table.attr.flow_attr.transfer == 0)
			return -1;
		out->args.table.attr.specialize |= RTE_FLOW_TABLE_SPECIALIZE_TRANSFER_VPORT_ORIG;
		return len;
	case RTE_FLOW_PARSER_CMD_TABLE_RESIZABLE:
		out->args.table.attr.specialize |=
			RTE_FLOW_TABLE_SPECIALIZE_RESIZABLE;
		return len;
	case RTE_FLOW_PARSER_CMD_TABLE_RULES_NUMBER:
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		return len;
	case RTE_FLOW_PARSER_CMD_TABLE_RESIZE_ID:
	case RTE_FLOW_PARSER_CMD_TABLE_RESIZE_RULES_NUMBER:
		return len;
	default:
		return -1;
	}
}

/** Parse tokens for table destroy command. */
static int
parse_table_destroy(struct context *ctx, const struct token *token,
		    const char *str, unsigned int len,
		    void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;
	uint32_t *table_id;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return len;
	if (out->command == 0 || out->command == RTE_FLOW_PARSER_CMD_TABLE) {
		if (ctx->curr != RTE_FLOW_PARSER_CMD_TABLE_DESTROY &&
		    ctx->curr != RTE_FLOW_PARSER_CMD_TABLE_RESIZE_COMPLETE)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		out->args.table_destroy.table_id =
			(void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
					       sizeof(double));
		return len;
	}
	table_id = out->args.table_destroy.table_id
		    + out->args.table_destroy.table_id_n++;
	if ((uint8_t *)table_id > (uint8_t *)out + size)
		return -1;
	ctx->objdata = 0;
	ctx->object = table_id;
	ctx->objmask = NULL;
	return len;
}

/** Parse table id and convert to table pointer for jump_to_table_index action. */
static int
parse_jump_table_id(struct context *ctx, const struct token *token,
	    const char *str, unsigned int len,
	    void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;
	uint32_t table_id;
	const struct arg *arg;
	void *entry_ptr;
	struct rte_flow_template_table *table;

	/* Get the arg before parse_int consumes it */
	arg = pop_args(ctx);
	if (arg == NULL)
		return -1;
	/* Push it back and do the standard integer parsing */
	if (push_args(ctx, arg) < 0)
		return -1;
	if (parse_int(ctx, token, str, len, buf, size) < 0)
		return -1;
	/* Nothing else to do if there is no buffer */
	if (out == NULL || ctx->object == NULL)
		return len;
	/* Get the parsed table ID from where parse_int stored it */
	entry_ptr = (uint8_t *)ctx->object + arg->offset;
	memcpy(&table_id, entry_ptr, sizeof(uint32_t));
	/* Look up the table using table ID */
	table = parser_table_get(ctx->port, table_id);
	if (table == NULL)
		return -1;
	/* Replace the table ID with the table pointer */
	memcpy(entry_ptr, &table, sizeof(struct rte_flow_template_table *));
	return len;
}

/** Parse tokens for queue create commands. */
static int
parse_qo(struct context *ctx, const struct token *token,
	 const char *str, unsigned int len,
	 void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return len;
	if (out->command == 0) {
		if (ctx->curr != RTE_FLOW_PARSER_CMD_QUEUE)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		out->args.vc.data = (uint8_t *)out + size;
		return len;
	}
	switch (ctx->curr) {
	case RTE_FLOW_PARSER_CMD_QUEUE_CREATE:
	case RTE_FLOW_PARSER_CMD_QUEUE_UPDATE:
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		out->args.vc.rule_id = UINT32_MAX;
		return len;
	case RTE_FLOW_PARSER_CMD_QUEUE_TEMPLATE_TABLE:
	case RTE_FLOW_PARSER_CMD_QUEUE_PATTERN_TEMPLATE:
	case RTE_FLOW_PARSER_CMD_QUEUE_ACTIONS_TEMPLATE:
	case RTE_FLOW_PARSER_CMD_QUEUE_CREATE_POSTPONE:
	case RTE_FLOW_PARSER_CMD_QUEUE_RULE_ID:
	case RTE_FLOW_PARSER_CMD_QUEUE_UPDATE_ID:
		return len;
	case RTE_FLOW_PARSER_CMD_ITEM_PATTERN:
		out->args.vc.pattern =
			(void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
					       sizeof(double));
		ctx->object = out->args.vc.pattern;
		ctx->objmask = NULL;
		return len;
	case RTE_FLOW_PARSER_CMD_ACTIONS:
		out->args.vc.actions =
			(void *)RTE_ALIGN_CEIL((uintptr_t)
					       (out->args.vc.pattern +
						out->args.vc.pattern_n),
					       sizeof(double));
		ctx->object = out->args.vc.actions;
		ctx->objmask = NULL;
		return len;
	default:
		return -1;
	}
}

/** Parse tokens for queue destroy command. */
static int
parse_qo_destroy(struct context *ctx, const struct token *token,
		 const char *str, unsigned int len,
		 void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;
	uint64_t *flow_id;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return len;
	if (out->command == 0 || out->command == RTE_FLOW_PARSER_CMD_QUEUE) {
		if (ctx->curr != RTE_FLOW_PARSER_CMD_QUEUE_DESTROY &&
		    ctx->curr != RTE_FLOW_PARSER_CMD_QUEUE_FLOW_UPDATE_RESIZED)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		out->args.destroy.rule =
			(void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
					       sizeof(double));
		return len;
	}
	switch (ctx->curr) {
	case RTE_FLOW_PARSER_CMD_QUEUE_DESTROY_ID:
		flow_id = out->args.destroy.rule
				+ out->args.destroy.rule_n++;
		if ((uint8_t *)flow_id > (uint8_t *)out + size)
			return -1;
		ctx->objdata = 0;
		ctx->object = flow_id;
		ctx->objmask = NULL;
		return len;
	case RTE_FLOW_PARSER_CMD_QUEUE_DESTROY_POSTPONE:
		return len;
	default:
		return -1;
	}
}

/** Parse tokens for push queue command. */
static int
parse_push(struct context *ctx, const struct token *token,
	   const char *str, unsigned int len,
	   void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return len;
	if (out->command == 0) {
		if (ctx->curr != RTE_FLOW_PARSER_CMD_PUSH)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		out->args.vc.data = (uint8_t *)out + size;
	}
	return len;
}

/** Parse tokens for pull command. */
static int
parse_pull(struct context *ctx, const struct token *token,
	   const char *str, unsigned int len,
	   void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return len;
	if (out->command == 0) {
		if (ctx->curr != RTE_FLOW_PARSER_CMD_PULL)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		out->args.vc.data = (uint8_t *)out + size;
	}
	return len;
}

/** Parse tokens for hash calculation commands. */
static int
parse_hash(struct context *ctx, const struct token *token,
	 const char *str, unsigned int len,
	 void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return len;
	if (out->command == 0) {
		if (ctx->curr != RTE_FLOW_PARSER_CMD_HASH)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		out->args.vc.data = (uint8_t *)out + size;
		return len;
	}
	switch (ctx->curr) {
	case RTE_FLOW_PARSER_CMD_HASH_CALC_TABLE:
	case RTE_FLOW_PARSER_CMD_HASH_CALC_PATTERN_INDEX:
		return len;
	case RTE_FLOW_PARSER_CMD_ITEM_PATTERN:
		out->args.vc.pattern =
			(void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
					       sizeof(double));
		ctx->object = out->args.vc.pattern;
		ctx->objmask = NULL;
		return len;
	case RTE_FLOW_PARSER_CMD_HASH_CALC_ENCAP:
		out->args.vc.encap_hash = 1;
		return len;
	case RTE_FLOW_PARSER_CMD_ENCAP_HASH_FIELD_SRC_PORT:
		out->args.vc.field = RTE_FLOW_ENCAP_HASH_FIELD_SRC_PORT;
		return len;
	case RTE_FLOW_PARSER_CMD_ENCAP_HASH_FIELD_GRE_FLOW_ID:
		out->args.vc.field = RTE_FLOW_ENCAP_HASH_FIELD_NVGRE_FLOW_ID;
		return len;
	default:
		return -1;
	}
}

static int
parse_group(struct context *ctx, const struct token *token,
	    const char *str, unsigned int len,
	    void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return len;
	if (out->command == 0) {
		if (ctx->curr != RTE_FLOW_PARSER_CMD_FLOW_GROUP)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		out->args.vc.data = (uint8_t *)out + size;
		return len;
	}
	switch (ctx->curr) {
	case RTE_FLOW_PARSER_CMD_GROUP_INGRESS:
		out->args.vc.attr.ingress = 1;
		return len;
	case RTE_FLOW_PARSER_CMD_GROUP_EGRESS:
		out->args.vc.attr.egress = 1;
		return len;
	case RTE_FLOW_PARSER_CMD_GROUP_TRANSFER:
		out->args.vc.attr.transfer = 1;
		return len;
	case RTE_FLOW_PARSER_CMD_GROUP_SET_MISS_ACTIONS:
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		out->args.vc.actions = (void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
							       sizeof(double));
		return len;
	default:
		return -1;
	}
}

static int
parse_flex(struct context *ctx, const struct token *token,
	     const char *str, unsigned int len,
	     void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return len;
	if (out->command == RTE_FLOW_PARSER_CMD_ZERO) {
		if (ctx->curr != RTE_FLOW_PARSER_CMD_FLEX)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
	} else {
		switch (ctx->curr) {
		default:
			break;
		case RTE_FLOW_PARSER_CMD_FLEX_ITEM_CREATE:
		case RTE_FLOW_PARSER_CMD_FLEX_ITEM_DESTROY:
			out->command = ctx->curr;
			break;
		}
	}

	return len;
}

static int
parse_tunnel(struct context *ctx, const struct token *token,
	     const char *str, unsigned int len,
	     void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return len;
	if (out->command == 0) {
		if (ctx->curr != RTE_FLOW_PARSER_CMD_TUNNEL)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
	} else {
		switch (ctx->curr) {
		default:
			break;
		case RTE_FLOW_PARSER_CMD_TUNNEL_CREATE:
		case RTE_FLOW_PARSER_CMD_TUNNEL_DESTROY:
		case RTE_FLOW_PARSER_CMD_TUNNEL_LIST:
			out->command = ctx->curr;
			break;
		case RTE_FLOW_PARSER_CMD_TUNNEL_CREATE_TYPE:
		case RTE_FLOW_PARSER_CMD_TUNNEL_DESTROY_ID:
			ctx->object = &out->args.vc.tunnel_ops;
			break;
		}
	}

	return len;
}

/**
 * Parse signed/unsigned integers 8 to 64-bit long.
 *
 * Last argument (ctx->args) is retrieved to determine integer type and
 * storage location.
 */
static int
parse_int(struct context *ctx, const struct token *token,
	  const char *str, unsigned int len,
	  void *buf, unsigned int size)
{
	const struct arg *arg = pop_args(ctx);
	uintmax_t u;
	char *end;

	(void)token;
	/* Argument is expected. */
	if (arg == NULL)
		return -1;
	errno = 0;
	u = arg->sign ?
		(uintmax_t)strtoimax(str, &end, 0) :
		strtoumax(str, &end, 0);
	if (errno || (size_t)(end - str) != len)
		goto error;
	if (arg->bounded &&
	    ((arg->sign && ((intmax_t)u < (intmax_t)arg->min ||
			    (intmax_t)u > (intmax_t)arg->max)) ||
	     (!arg->sign && (u < arg->min || u > arg->max))))
		goto error;
	if (ctx->object == NULL)
		return len;
	if (arg->mask != NULL) {
		if (arg_entry_bf_fill(ctx->object, u, arg) == 0 ||
		    !arg_entry_bf_fill(ctx->objmask, -1, arg))
			goto error;
		return len;
	}
	buf = (uint8_t *)ctx->object + arg->offset;
	size = arg->size;
	if (u > RTE_LEN2MASK(size * CHAR_BIT, uint64_t))
		return -1;
objmask:
	switch (size) {
	case sizeof(uint8_t):
		*(uint8_t *)buf = u;
		break;
	case sizeof(uint16_t):
		*(uint16_t *)buf = arg->hton ? rte_cpu_to_be_16(u) : u;
		break;
	case sizeof(uint8_t [3]):
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
		if (arg->hton == 0) {
			((uint8_t *)buf)[0] = u;
			((uint8_t *)buf)[1] = u >> 8;
			((uint8_t *)buf)[2] = u >> 16;
			break;
		}
#endif
		((uint8_t *)buf)[0] = u >> 16;
		((uint8_t *)buf)[1] = u >> 8;
		((uint8_t *)buf)[2] = u;
		break;
	case sizeof(uint32_t):
		*(uint32_t *)buf = arg->hton ? rte_cpu_to_be_32(u) : u;
		break;
	case sizeof(uint64_t):
		*(uint64_t *)buf = arg->hton ? rte_cpu_to_be_64(u) : u;
		break;
	default:
		goto error;
	}
	if (ctx->objmask && buf != (uint8_t *)ctx->objmask + arg->offset) {
		u = -1;
		buf = (uint8_t *)ctx->objmask + arg->offset;
		goto objmask;
	}
	return len;
error:
	push_args(ctx, arg);
	return -1;
}

/**
 * Parse a string.
 *
 * Three arguments (ctx->args) are retrieved from the stack to store data,
 * its actual length and address (in that order).
 */
static int
parse_string(struct context *ctx, const struct token *token,
	     const char *str, unsigned int len,
	     void *buf, unsigned int size)
{
	const struct arg *arg_data = pop_args(ctx);
	const struct arg *arg_len = pop_args(ctx);
	const struct arg *arg_addr = pop_args(ctx);
	char tmp[16]; /* Ought to be enough. */
	int ret;

	/* Arguments are expected. */
	if (arg_data == NULL)
		return -1;
	if (arg_len == NULL) {
		push_args(ctx, arg_data);
		return -1;
	}
	if (arg_addr == NULL) {
		push_args(ctx, arg_len);
		push_args(ctx, arg_data);
		return -1;
	}
	size = arg_data->size;
	/* Bit-mask fill is not supported. */
	if (arg_data->mask != NULL || size < len)
		goto error;
	if (ctx->object == NULL)
		return len;
	/* Let parse_int() fill length information first. */
	ret = snprintf(tmp, sizeof(tmp), "%u", len);
	if (ret < 0)
		goto error;
	push_args(ctx, arg_len);
	ret = parse_int(ctx, token, tmp, ret, NULL, 0);
	if (ret < 0) {
		pop_args(ctx);
		goto error;
	}
	buf = (uint8_t *)ctx->object + arg_data->offset;
	/* Output buffer is not necessarily NUL-terminated. */
	memcpy(buf, str, len);
	memset((uint8_t *)buf + len, 0x00, size - len);
	if (ctx->objmask != NULL)
		memset((uint8_t *)ctx->objmask + arg_data->offset, 0xff, len);
	/* Save address if requested. */
	if (arg_addr->size != 0) {
		memcpy((uint8_t *)ctx->object + arg_addr->offset,
		       (void *[]){
			(uint8_t *)ctx->object + arg_data->offset
		       },
		       arg_addr->size);
		if (ctx->objmask != NULL)
			memcpy((uint8_t *)ctx->objmask + arg_addr->offset,
			       (void *[]){
				(uint8_t *)ctx->objmask + arg_data->offset
			       },
			       arg_addr->size);
	}
	return len;
error:
	push_args(ctx, arg_addr);
	push_args(ctx, arg_len);
	push_args(ctx, arg_data);
	return -1;
}

static int
parse_hex_string(const char *src, uint8_t *dst, uint32_t *size)
{
	const uint8_t *head = dst;
	uint32_t left;

	if (*size == 0)
		return -1;

	left = *size;

	/* Convert chars to bytes */
	while (left != 0) {
		char tmp[3], *end = tmp;
		uint32_t read_lim = left & 1 ? 1 : 2;

		snprintf(tmp, read_lim + 1, "%s", src);
		*dst = strtoul(tmp, &end, 16);
		if (*end != '\0') {
			*dst = 0;
			*size = (uint32_t)(dst - head);
			return -1;
		}
		left -= read_lim;
		src += read_lim;
		dst++;
	}
	*dst = 0;
	*size = (uint32_t)(dst - head);
	return 0;
}

static int
parse_hex(struct context *ctx, const struct token *token,
		const char *str, unsigned int len,
		void *buf, unsigned int size)
{
	const struct arg *arg_data = pop_args(ctx);
	const struct arg *arg_len = pop_args(ctx);
	const struct arg *arg_addr = pop_args(ctx);
	char tmp[16]; /* Ought to be enough. */
	int ret;
	unsigned int hexlen = len;
	uint8_t hex_tmp[256];

	/* Arguments are expected. */
	if (arg_data == NULL)
		return -1;
	if (arg_len == NULL) {
		push_args(ctx, arg_data);
		return -1;
	}
	if (arg_addr == NULL) {
		push_args(ctx, arg_len);
		push_args(ctx, arg_data);
		return -1;
	}
	size = arg_data->size;
	/* Bit-mask fill is not supported. */
	if (arg_data->mask != NULL)
		goto error;
	if (ctx->object == NULL)
		return len;

	/* translate bytes string to array. */
	if (str[0] == '0' && ((str[1] == 'x') ||
			(str[1] == 'X'))) {
		str += 2;
		hexlen -= 2;
	}
	if (hexlen > RTE_DIM(hex_tmp))
		goto error;
	ret = parse_hex_string(str, hex_tmp, &hexlen);
	if (ret < 0)
		goto error;
	/* Check the converted binary fits into data buffer. */
	if (hexlen > size)
		goto error;
	/* Let parse_int() fill length information first. */
	ret = snprintf(tmp, sizeof(tmp), "%u", hexlen);
	if (ret < 0)
		goto error;
	/* Save length if requested. */
	if (arg_len->size != 0) {
		push_args(ctx, arg_len);
		ret = parse_int(ctx, token, tmp, ret, NULL, 0);
		if (ret < 0) {
			pop_args(ctx);
			goto error;
		}
	}
	buf = (uint8_t *)ctx->object + arg_data->offset;
	/* Output buffer is not necessarily NUL-terminated. */
	memcpy(buf, hex_tmp, hexlen);
	memset((uint8_t *)buf + hexlen, 0x00, size - hexlen);
	if (ctx->objmask != NULL)
		memset((uint8_t *)ctx->objmask + arg_data->offset,
					0xff, hexlen);
	/* Save address if requested. */
	if (arg_addr->size != 0) {
		memcpy((uint8_t *)ctx->object + arg_addr->offset,
		       (void *[]){
			(uint8_t *)ctx->object + arg_data->offset
		       },
		       arg_addr->size);
		if (ctx->objmask != NULL)
			memcpy((uint8_t *)ctx->objmask + arg_addr->offset,
			       (void *[]){
				(uint8_t *)ctx->objmask + arg_data->offset
			       },
			       arg_addr->size);
	}
	return len;
error:
	push_args(ctx, arg_addr);
	push_args(ctx, arg_len);
	push_args(ctx, arg_data);
	return -1;

}

/**
 * Parse a zero-ended string.
 */
static int
parse_string0(struct context *ctx, const struct token *token __rte_unused,
	     const char *str, unsigned int len,
	     void *buf, unsigned int size)
{
	const struct arg *arg_data = pop_args(ctx);

	/* Arguments are expected. */
	if (arg_data == NULL)
		return -1;
	size = arg_data->size;
	/* Bit-mask fill is not supported. */
	if (arg_data->mask != NULL || size < len + 1)
		goto error;
	if (ctx->object == NULL)
		return len;
	buf = (uint8_t *)ctx->object + arg_data->offset;
	strncpy(buf, str, len);
	if (ctx->objmask != NULL)
		memset((uint8_t *)ctx->objmask + arg_data->offset, 0xff, len);
	return len;
error:
	push_args(ctx, arg_data);
	return -1;
}

/**
 * Parse a MAC address.
 *
 * Last argument (ctx->args) is retrieved to determine storage size and
 * location.
 */
static int
parse_mac_addr(struct context *ctx, const struct token *token,
	       const char *str, unsigned int len,
	       void *buf, unsigned int size)
{
	const struct arg *arg = pop_args(ctx);
	struct rte_ether_addr tmp;
	int ret;

	(void)token;
	/* Argument is expected. */
	if (arg == NULL)
		return -1;
	size = arg->size;
	/* Bit-mask fill is not supported. */
	if (arg->mask || size != sizeof(tmp))
		goto error;
	/* Only network endian is supported. */
	if (arg->hton == 0)
		goto error;
	ret = cmdline_parse_etheraddr(NULL, str, &tmp, size);
	if (ret < 0 || (unsigned int)ret != len)
		goto error;
	if (ctx->object == NULL)
		return len;
	buf = (uint8_t *)ctx->object + arg->offset;
	memcpy(buf, &tmp, size);
	if (ctx->objmask != NULL)
		memset((uint8_t *)ctx->objmask + arg->offset, 0xff, size);
	return len;
error:
	push_args(ctx, arg);
	return -1;
}

/**
 * Parse an IPv4 address.
 *
 * Last argument (ctx->args) is retrieved to determine storage size and
 * location.
 */
static int
parse_ipv4_addr(struct context *ctx, const struct token *token,
		const char *str, unsigned int len,
		void *buf, unsigned int size)
{
	const struct arg *arg = pop_args(ctx);
	char str2[INET_ADDRSTRLEN];
	struct in_addr tmp;
	int ret;

	/* Length is longer than the max length an IPv4 address can have. */
	if (len >= INET_ADDRSTRLEN)
		return -1;
	/* Argument is expected. */
	if (arg == NULL)
		return -1;
	size = arg->size;
	/* Bit-mask fill is not supported. */
	if (arg->mask || size != sizeof(tmp))
		goto error;
	/* Only network endian is supported. */
	if (arg->hton == 0)
		goto error;
	memcpy(str2, str, len);
	str2[len] = '\0';
	ret = inet_pton(AF_INET, str2, &tmp);
	if (ret != 1) {
		/* Attempt integer parsing. */
		push_args(ctx, arg);
		return parse_int(ctx, token, str, len, buf, size);
	}
	if (ctx->object == NULL)
		return len;
	buf = (uint8_t *)ctx->object + arg->offset;
	memcpy(buf, &tmp, size);
	if (ctx->objmask != NULL)
		memset((uint8_t *)ctx->objmask + arg->offset, 0xff, size);
	return len;
error:
	push_args(ctx, arg);
	return -1;
}

/**
 * Parse an IPv6 address.
 *
 * Last argument (ctx->args) is retrieved to determine storage size and
 * location.
 */
static int
parse_ipv6_addr(struct context *ctx, const struct token *token,
		const char *str, unsigned int len,
		void *buf, unsigned int size)
{
	const struct arg *arg = pop_args(ctx);
	char str2[INET6_ADDRSTRLEN];
	struct rte_ipv6_addr tmp;
	int ret;

	(void)token;
	/* Length is longer than the max length an IPv6 address can have. */
	if (len >= INET6_ADDRSTRLEN)
		return -1;
	/* Argument is expected. */
	if (arg == NULL)
		return -1;
	size = arg->size;
	/* Bit-mask fill is not supported. */
	if (arg->mask || size != sizeof(tmp))
		goto error;
	/* Only network endian is supported. */
	if (arg->hton == 0)
		goto error;
	memcpy(str2, str, len);
	str2[len] = '\0';
	ret = inet_pton(AF_INET6, str2, &tmp);
	if (ret != 1)
		goto error;
	if (ctx->object == NULL)
		return len;
	buf = (uint8_t *)ctx->object + arg->offset;
	memcpy(buf, &tmp, size);
	if (ctx->objmask != NULL)
		memset((uint8_t *)ctx->objmask + arg->offset, 0xff, size);
	return len;
error:
	push_args(ctx, arg);
	return -1;
}

/** Boolean values (even indices stand for false). */
static const char *const boolean_name[] = {
	"0", "1",
	"false", "true",
	"no", "yes",
	"N", "Y",
	"off", "on",
	NULL,
};

/**
 * Parse a boolean value.
 *
 * Last argument (ctx->args) is retrieved to determine storage size and
 * location.
 */
static int
parse_boolean(struct context *ctx, const struct token *token,
	      const char *str, unsigned int len,
	      void *buf, unsigned int size)
{
	const struct arg *arg = pop_args(ctx);
	unsigned int i;
	int ret;

	/* Argument is expected. */
	if (arg == NULL)
		return -1;
	for (i = 0; boolean_name[i]; ++i)
		if (strcmp_partial(boolean_name[i], str, len) == 0)
			break;
	/* Process token as integer. */
	if (boolean_name[i] != NULL)
		str = i & 1 ? "1" : "0";
	push_args(ctx, arg);
	ret = parse_int(ctx, token, str, strlen(str), buf, size);
	return ret > 0 ? (int)len : ret;
}

/** Parse port and update context. */
static int
parse_port(struct context *ctx, const struct token *token,
	   const char *str, unsigned int len,
	   void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = &(struct rte_flow_parser_output){ .port = 0 };
	int ret;

	if (buf != NULL)
		out = buf;
	else {
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		size = sizeof(*out);
	}
	ret = parse_int(ctx, token, str, len, out, size);
	if (ret >= 0)
		ctx->port = out->port;
	if (buf == NULL)
		ctx->object = NULL;
	return ret;
}

/** Parse tokens for shared indirect actions. */
static int
parse_ia_port(struct context *ctx, const struct token *token,
	      const char *str, unsigned int len,
	      void *buf, unsigned int size)
{
	struct rte_flow_action *action = ctx->object;
	uint32_t id;
	int ret;

	(void)buf;
	(void)size;
	ctx->objdata = 0;
	ctx->object = &id;
	ctx->objmask = NULL;
	ret = parse_int(ctx, token, str, len, ctx->object, sizeof(id));
	ctx->object = action;
	if (ret != (int)len)
		return ret;
	/* set indirect action */
	if (action != NULL)
		action->conf = (void *)(uintptr_t)id;
	return ret;
}

static int
parse_ia_id2ptr(struct context *ctx, const struct token *token,
		const char *str, unsigned int len,
		void *buf, unsigned int size)
{
	struct rte_flow_action *action = ctx->object;
	uint32_t id;
	int ret;

	(void)buf;
	(void)size;
	ctx->objdata = 0;
	ctx->object = &id;
	ctx->objmask = NULL;
	ret = parse_int(ctx, token, str, len, ctx->object, sizeof(id));
	ctx->object = action;
	if (ret != (int)len)
		return ret;
	/* set indirect action */
	if (action != NULL) {
		portid_t port_id = ctx->port;
		if (ctx->prev == RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_PORT)
			port_id = (portid_t)(uintptr_t)action->conf;
		action->conf = parser_action_handle_get(port_id, id);
		ret = (action->conf) ? ret : -1;
	}
	return ret;
}

static int
parse_indlst_id2ptr(struct context *ctx, const struct token *token,
		    const char *str, unsigned int len,
		    __rte_unused void *buf, __rte_unused unsigned int size)
{
	struct rte_flow_action *action = ctx->object;
	struct rte_flow_action_indirect_list *action_conf;
	const struct indlst_conf *indlst_conf;
	uint32_t id;
	int ret;

	ctx->objdata = 0;
	ctx->object = &id;
	ctx->objmask = NULL;
	ret = parse_int(ctx, token, str, len, ctx->object, sizeof(id));
	ctx->object = action;
	if (ret != (int)len)
		return ret;

	/* set handle and conf */
	if (action != NULL) {
		action_conf = (void *)(uintptr_t)action->conf;
		action_conf->conf = NULL;
		switch (ctx->curr) {
		case RTE_FLOW_PARSER_CMD_INDIRECT_LIST_ACTION_ID2PTR_HANDLE:
		action_conf->handle = (typeof(action_conf->handle))
				parser_action_handle_get(ctx->port, id);
			if (action_conf->handle == NULL)
				return -1;
			break;
		case RTE_FLOW_PARSER_CMD_INDIRECT_LIST_ACTION_ID2PTR_CONF:
			indlst_conf = indirect_action_list_conf_get(id);
			if (indlst_conf == NULL)
				return -1;
			action_conf->conf = (const void **)indlst_conf->conf;
			break;
		default:
			break;
		}
	}
	return ret;
}

static int
parse_meter_profile_id2ptr(struct context *ctx, const struct token *token,
		const char *str, unsigned int len,
		void *buf, unsigned int size)
{
	struct rte_flow_action *action = ctx->object;
	struct rte_flow_action_meter_mark *meter;
	struct rte_flow_meter_profile *profile = NULL;
	uint32_t id = 0;
	int ret;

	(void)buf;
	(void)size;
	ctx->objdata = 0;
	ctx->object = &id;
	ctx->objmask = NULL;
	ret = parse_int(ctx, token, str, len, ctx->object, sizeof(id));
	ctx->object = action;
	if (ret != (int)len)
		return ret;
	/* set meter profile */
	if (action != NULL) {
		meter = (struct rte_flow_action_meter_mark *)
			(uintptr_t)(action->conf);
		profile = parser_meter_profile_get(ctx->port, id);
		meter->profile = profile;
		ret = (profile) ? ret : -1;
	}
	return ret;
}

static int
parse_meter_policy_id2ptr(struct context *ctx, const struct token *token,
		const char *str, unsigned int len,
		void *buf, unsigned int size)
{
	struct rte_flow_action *action = ctx->object;
	struct rte_flow_action_meter_mark *meter;
	struct rte_flow_meter_policy *policy = NULL;
	uint32_t id = 0;
	int ret;

	(void)buf;
	(void)size;
	ctx->objdata = 0;
	ctx->object = &id;
	ctx->objmask = NULL;
	ret = parse_int(ctx, token, str, len, ctx->object, sizeof(id));
	ctx->object = action;
	if (ret != (int)len)
		return ret;
	/* set meter policy */
	if (action != NULL) {
		meter = (struct rte_flow_action_meter_mark *)
			(uintptr_t)(action->conf);
		policy = parser_meter_policy_get(ctx->port, id);
		meter->policy = policy;
		ret = (policy) ? ret : -1;
	}
	return ret;
}

/** Parse set command, initialize output buffer for subsequent tokens. */
static int
parse_set_raw_encap_decap(struct context *ctx, const struct token *token,
			  const char *str, unsigned int len,
			  void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return len;
	/* Make sure buffer is large enough. */
	if (size < sizeof(*out))
		return -1;
	ctx->objdata = 0;
	ctx->objmask = NULL;
	ctx->object = out;
	if (out->command == 0)
		return -1;
	out->command = ctx->curr;
	/* For encap/decap we need is pattern */
	out->args.vc.pattern = (void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
						       sizeof(double));
	return len;
}

/** Parse set command, initialize output buffer for subsequent tokens. */
static int
parse_set_sample_action(struct context *ctx, const struct token *token,
			  const char *str, unsigned int len,
			  void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return len;
	/* Make sure buffer is large enough. */
	if (size < sizeof(*out))
		return -1;
	ctx->objdata = 0;
	ctx->objmask = NULL;
	ctx->object = out;
	if (out->command == 0)
		return -1;
	out->command = ctx->curr;
	/* For sampler we need is actions */
	out->args.vc.actions = (void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
						       sizeof(double));
	return len;
}

/** Parse set command, initialize output buffer for subsequent tokens. */
static int
parse_set_ipv6_ext_action(struct context *ctx, const struct token *token,
			  const char *str, unsigned int len,
			  void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return len;
	/* Make sure buffer is large enough. */
	if (size < sizeof(*out))
		return -1;
	ctx->objdata = 0;
	ctx->objmask = NULL;
	ctx->object = out;
	if (out->command == 0)
		return -1;
	out->command = ctx->curr;
	/* For ipv6_ext_push/remove we need is pattern */
	out->args.vc.pattern = (void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
						       sizeof(double));
	return len;
}

/**
 * Parse set raw_encap/raw_decap command,
 * initialize output buffer for subsequent tokens.
 */
static int
parse_set_init(struct context *ctx, const struct token *token,
	       const char *str, unsigned int len,
	       void *buf, unsigned int size)
{
	struct rte_flow_parser_output *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (out == NULL)
		return len;
	/* Make sure buffer is large enough. */
	if (size < sizeof(*out))
		return -1;
	/* Initialize buffer. */
	memset(out, 0x00, sizeof(*out));
	memset((uint8_t *)out + sizeof(*out), 0x22, size - sizeof(*out));
	ctx->objdata = 0;
	ctx->object = out;
	ctx->objmask = NULL;
	if (out->command == 0) {
		if (ctx->curr != RTE_FLOW_PARSER_CMD_SET)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		out->args.vc.data = (uint8_t *)out + size;
		ctx->object  = (void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
						       sizeof(double));
	}
	return len;
}

/*
 * Replace testpmd handles in a flex flow item with real values.
 */
static int
parse_flex_handle(struct context *ctx, const struct token *token,
		  const char *str, unsigned int len,
		  void *buf, unsigned int size)
{
	struct rte_flow_item_flex *spec, *mask;
	const struct rte_flow_item_flex *src_spec, *src_mask;
	struct rte_flow_item_flex_handle *flex_handle;
	const struct arg *arg = pop_args(ctx);
	uint32_t offset;
	uint16_t handle;
	int ret;

	if (arg == NULL)
		return -1;
	offset = arg->offset;
	push_args(ctx, arg);
	ret = parse_int(ctx, token, str, len, buf, size);
	if (ret <= 0 || ctx->object == NULL)
		return ret;
	if (ctx->port >= RTE_MAX_ETHPORTS)
		return -1;
	if (offset == offsetof(struct rte_flow_item_flex, handle)) {
		spec = ctx->object;
		handle = (uint16_t)(uintptr_t)spec->handle;
		flex_handle = parser_flex_handle_get(ctx->port, handle);
		if (flex_handle == NULL)
			return -1;
		spec->handle = flex_handle;
		mask = spec + 2; /* spec, last, mask */
		mask->handle = flex_handle;
	} else if (offset == offsetof(struct rte_flow_item_flex, pattern)) {
		handle = (uint16_t)(uintptr_t)
			((struct rte_flow_item_flex *)ctx->object)->pattern;
		if (parser_flex_pattern_get(handle, &src_spec, &src_mask) != 0 ||
		    src_spec == NULL || src_mask == NULL)
			return -1;
		spec = ctx->object;
		mask = spec + 2; /* spec, last, mask */
		/* fill flow rule spec and mask parameters */
		spec->length = src_spec->length;
		spec->pattern = src_spec->pattern;
		mask->length = src_mask->length;
		mask->pattern = src_mask->pattern;
	} else {
		return -1;
	}
	return ret;
}

/** Parse Meter color name */
static int
parse_meter_color(struct context *ctx, const struct token *token,
		  const char *str, unsigned int len, void *buf,
		  unsigned int size)
{
	unsigned int i;
	struct rte_flow_parser_output *out = buf;

	(void)token;
	(void)buf;
	(void)size;
	for (i = 0; meter_colors[i]; ++i)
		if (strcmp_partial(meter_colors[i], str, len) == 0)
			break;
	if (meter_colors[i] == NULL)
		return -1;
	if (ctx->object == NULL)
		return len;
	if (ctx->prev == RTE_FLOW_PARSER_CMD_ACTION_METER_MARK_CONF_COLOR) {
		struct rte_flow_action *action =
			out->args.vc.actions + out->args.vc.actions_n - 1;
		const struct arg *arg = pop_args(ctx);

		if (arg == NULL)
			return -1;
		*(int *)RTE_PTR_ADD(action->conf, arg->offset) = i;
	} else {
		((struct rte_flow_item_meter_color *)
			ctx->object)->color = (enum rte_color)i;
	}
	return len;
}

/** Parse Insertion Table Type name */
static int
parse_insertion_table_type(struct context *ctx, const struct token *token,
			   const char *str, unsigned int len, void *buf,
			   unsigned int size)
{
	const struct arg *arg = pop_args(ctx);
	unsigned int i;
	char tmp[2];
	int ret;

	(void)size;
	/* Argument is expected. */
	if (arg == NULL)
		return -1;
	for (i = 0; table_insertion_types[i]; ++i)
		if (strcmp_partial(table_insertion_types[i], str, len) == 0)
			break;
	if (table_insertion_types[i] == NULL)
		return -1;
	push_args(ctx, arg);
	snprintf(tmp, sizeof(tmp), "%u", i);
	ret = parse_int(ctx, token, tmp, strlen(tmp), buf, sizeof(i));
	return ret > 0 ? (int)len : ret;
}

/** Parse Hash Calculation Table Type name */
static int
parse_hash_table_type(struct context *ctx, const struct token *token,
		      const char *str, unsigned int len, void *buf,
		      unsigned int size)
{
	const struct arg *arg = pop_args(ctx);
	unsigned int i;
	char tmp[2];
	int ret;

	(void)size;
	/* Argument is expected. */
	if (arg == NULL)
		return -1;
	for (i = 0; table_hash_funcs[i]; ++i)
		if (strcmp_partial(table_hash_funcs[i], str, len) == 0)
			break;
	if (table_hash_funcs[i] == NULL)
		return -1;
	push_args(ctx, arg);
	snprintf(tmp, sizeof(tmp), "%u", i);
	ret = parse_int(ctx, token, tmp, strlen(tmp), buf, sizeof(i));
	return ret > 0 ? (int)len : ret;
}

static int
parse_name_to_index(struct context *ctx, const struct token *token,
		    const char *str, unsigned int len, void *buf,
		    unsigned int size,
		    const char *const names[], size_t names_size, uint32_t *dst)
{
	int ret;
	uint32_t i;

	RTE_SET_USED(token);
	RTE_SET_USED(buf);
	RTE_SET_USED(size);
	if (ctx->object == NULL)
		return len;
	for (i = 0; i < names_size; i++) {
		if (names[i] == NULL)
			continue;
		ret = strcmp_partial(names[i], str,
				     RTE_MIN(len, strlen(names[i])));
		if (ret == 0) {
			*dst = i;
			return len;
		}
	}
	return -1;
}

static const char *const quota_mode_names[] = {
	NULL,
	[RTE_FLOW_QUOTA_MODE_PACKET] = "packet",
	[RTE_FLOW_QUOTA_MODE_L2] = "l2",
	[RTE_FLOW_QUOTA_MODE_L3] = "l3"
};

static const char *const quota_state_names[] = {
	[RTE_FLOW_QUOTA_STATE_PASS] = "pass",
	[RTE_FLOW_QUOTA_STATE_BLOCK] = "block"
};

static const char *const quota_update_names[] = {
	[RTE_FLOW_UPDATE_QUOTA_SET] = "set",
	[RTE_FLOW_UPDATE_QUOTA_ADD] = "add"
};

static const char *const query_update_mode_names[] = {
	[RTE_FLOW_QU_QUERY_FIRST] = "query_first",
	[RTE_FLOW_QU_UPDATE_FIRST] = "update_first"
};

static int
parse_quota_state_name(struct context *ctx, const struct token *token,
		       const char *str, unsigned int len, void *buf,
		       unsigned int size)
{
	struct rte_flow_item_quota *quota = ctx->object;

	return parse_name_to_index(ctx, token, str, len, buf, size,
				   quota_state_names,
				   RTE_DIM(quota_state_names),
				   (uint32_t *)&quota->state);
}

static int
parse_quota_mode_name(struct context *ctx, const struct token *token,
		      const char *str, unsigned int len, void *buf,
		      unsigned int size)
{
	struct rte_flow_action_quota *quota = ctx->object;

	return parse_name_to_index(ctx, token, str, len, buf, size,
				   quota_mode_names,
				   RTE_DIM(quota_mode_names),
				   (uint32_t *)&quota->mode);
}

static int
parse_quota_update_name(struct context *ctx, const struct token *token,
			const char *str, unsigned int len, void *buf,
			unsigned int size)
{
	struct rte_flow_update_quota *update = ctx->object;

	return parse_name_to_index(ctx, token, str, len, buf, size,
				   quota_update_names,
				   RTE_DIM(quota_update_names),
				   (uint32_t *)&update->op);
}

static int
parse_qu_mode_name(struct context *ctx, const struct token *token,
		   const char *str, unsigned int len, void *buf,
		   unsigned int size)
{
	struct rte_flow_parser_output *out = ctx->object;

	return parse_name_to_index(ctx, token, str, len, buf, size,
				   query_update_mode_names,
				   RTE_DIM(query_update_mode_names),
				   (uint32_t *)&out->args.ia.qu_mode);
}

/** No completion. */
static int
comp_none(struct context *ctx, const struct token *token,
	  unsigned int ent, char *buf, unsigned int size)
{
	(void)ctx;
	(void)token;
	(void)ent;
	(void)buf;
	(void)size;
	return 0;
}

/** Complete boolean values. */
static int
comp_boolean(struct context *ctx, const struct token *token,
	     unsigned int ent, char *buf, unsigned int size)
{
	unsigned int i;

	(void)ctx;
	(void)token;
	for (i = 0; boolean_name[i]; ++i)
		if (buf && i == ent)
			return strlcpy(buf, boolean_name[i], size);
	if (buf != NULL)
		return -1;
	return i;
}

/** Complete action names. */
static int
comp_action(struct context *ctx, const struct token *token,
	    unsigned int ent, char *buf, unsigned int size)
{
	unsigned int i;

	(void)ctx;
	(void)token;
	for (i = 0; next_action[i]; ++i)
		if (buf && i == ent)
			return strlcpy(buf, token_list[next_action[i]].name,
				       size);
	if (buf != NULL)
		return -1;
	return i;
}

/** Complete available ports. */
static int
comp_port(struct context *ctx, const struct token *token,
	  unsigned int ent, char *buf, unsigned int size)
{
	unsigned int i = 0;
	portid_t p;

	(void)ctx;
	(void)token;
	RTE_ETH_FOREACH_DEV(p) {
		if (buf && i == ent)
			return snprintf(buf, size, "%u", p);
		++i;
	}
	if (buf != NULL)
		return -1;
	return i;
}

/** Complete available rule IDs. */
static int
comp_rule_id(struct context *ctx, const struct token *token,
	     unsigned int ent, char *buf, unsigned int size)
{
	uint16_t count;
	uint64_t rule_id;

	(void)token;
	if (parser_port_id_is_invalid(ctx->port) != 0 ||
	    ctx->port == (portid_t)RTE_PORT_ALL)
		return -1;
	count = parser_flow_rule_count(ctx->port);
	if (buf == NULL)
		return count;
	if (ent >= count)
		return -1;
	if (parser_flow_rule_id_get(ctx->port, ent, &rule_id) < 0)
		return -1;
	return snprintf(buf, size, "%" PRIu64, rule_id);
}

/** Complete operation for compare match item. */
static int
comp_set_compare_op(struct context *ctx, const struct token *token,
		    unsigned int ent, char *buf, unsigned int size)
{
	RTE_SET_USED(ctx);
	RTE_SET_USED(token);
	if (buf == NULL)
		return RTE_DIM(compare_ops);
	if (ent < RTE_DIM(compare_ops) - 1)
		return strlcpy(buf, compare_ops[ent], size);
	return -1;
}

/** Complete field id for compare match item. */
static int
comp_set_compare_field_id(struct context *ctx, const struct token *token,
			  unsigned int ent, char *buf, unsigned int size)
{
	const char *name;

	RTE_SET_USED(token);
	if (buf == NULL)
		return RTE_DIM(flow_field_ids);
	if (ent >= RTE_DIM(flow_field_ids) - 1)
		return -1;
	name = flow_field_ids[ent];
	if (ctx->curr == RTE_FLOW_PARSER_CMD_ITEM_COMPARE_FIELD_B_TYPE ||
	    (strcmp(name, "pointer") && strcmp(name, "value")))
		return strlcpy(buf, name, size);
	return -1;
}

/** Complete type field for RSS action. */
static int
comp_vc_action_rss_type(struct context *ctx, const struct token *token,
			unsigned int ent, char *buf, unsigned int size)
{
	const struct rte_eth_rss_type_info *tbl;
	unsigned int i;

	(void)ctx;
	(void)token;
	tbl = rte_eth_rss_type_info_get();
	for (i = 0; tbl[i].str; ++i)
		;
	if (buf == NULL)
		return i + 1;
	if (ent < i)
		return strlcpy(buf, tbl[ent].str, size);
	if (ent == i)
		return snprintf(buf, size, "end");
	return -1;
}

/** Complete queue field for RSS action. */
static int
comp_vc_action_rss_queue(struct context *ctx, const struct token *token,
		 unsigned int ent, char *buf, unsigned int size)
{
	uint16_t count;

	(void)token;
	if (parser_port_id_is_invalid(ctx->port) != 0 ||
	    ctx->port == (portid_t)RTE_PORT_ALL)
		return -1;
	count = parser_rss_queue_count(ctx->port);
	if (buf == NULL)
		return count + 1;
	if (ent < count)
		return snprintf(buf, size, "%u", ent);
	if (ent == count)
		return snprintf(buf, size, "end");
	return -1;
}

/** Complete index number for set raw_encap/raw_decap commands. */
static int
comp_set_raw_index(struct context *ctx, const struct token *token,
		   unsigned int ent, char *buf, unsigned int size)
{
	uint16_t idx = 0;
	uint16_t nb = 0;

	RTE_SET_USED(ctx);
	RTE_SET_USED(token);
	for (idx = 0; idx < RAW_ENCAP_CONFS_MAX_NUM; ++idx) {
		if (buf && idx == ent)
			return snprintf(buf, size, "%u", idx);
		++nb;
	}
	return nb;
}

/** Complete index number for set raw_ipv6_ext_push/ipv6_ext_remove commands. */
static int
comp_set_ipv6_ext_index(struct context *ctx, const struct token *token,
			unsigned int ent, char *buf, unsigned int size)
{
	uint16_t idx = 0;
	uint16_t nb = 0;

	RTE_SET_USED(ctx);
	RTE_SET_USED(token);
	for (idx = 0; idx < IPV6_EXT_PUSH_CONFS_MAX_NUM; ++idx) {
		if (buf && idx == ent)
			return snprintf(buf, size, "%u", idx);
		++nb;
	}
	return nb;
}

/** Complete index number for set raw_encap/raw_decap commands. */
static int
comp_set_sample_index(struct context *ctx, const struct token *token,
		   unsigned int ent, char *buf, unsigned int size)
{
	uint16_t idx = 0;
	uint16_t nb = 0;

	RTE_SET_USED(ctx);
	RTE_SET_USED(token);
	for (idx = 0; idx < RAW_SAMPLE_CONFS_MAX_NUM; ++idx) {
		if (buf && idx == ent)
			return snprintf(buf, size, "%u", idx);
		++nb;
	}
	return nb;
}

/** Complete operation for modify_field command. */
static int
comp_set_modify_field_op(struct context *ctx, const struct token *token,
		   unsigned int ent, char *buf, unsigned int size)
{
	RTE_SET_USED(ctx);
	RTE_SET_USED(token);
	if (buf == NULL)
		return RTE_DIM(modify_field_ops);
	if (ent < RTE_DIM(modify_field_ops) - 1)
		return strlcpy(buf, modify_field_ops[ent], size);
	return -1;
}

/** Complete field id for modify_field command. */
static int
comp_set_modify_field_id(struct context *ctx, const struct token *token,
		   unsigned int ent, char *buf, unsigned int size)
{
	const char *name;

	RTE_SET_USED(token);
	if (buf == NULL)
		return RTE_DIM(flow_field_ids);
	if (ent >= RTE_DIM(flow_field_ids) - 1)
		return -1;
	name = flow_field_ids[ent];
	if (ctx->curr == RTE_FLOW_PARSER_CMD_ACTION_MODIFY_FIELD_SRC_TYPE ||
	    (strcmp(name, "pointer") && strcmp(name, "value")))
		return strlcpy(buf, name, size);
	return -1;
}

/** Complete available pattern template IDs. */
static int
comp_pattern_template_id(struct context *ctx, const struct token *token,
		 unsigned int ent, char *buf, unsigned int size)
{
	uint16_t count;
	uint32_t template_id;

	(void)token;
	if (parser_port_id_is_invalid(ctx->port) != 0 ||
	    ctx->port == (portid_t)RTE_PORT_ALL)
		return -1;
	count = parser_pattern_template_count(ctx->port);
	if (buf == NULL)
		return count;
	if (ent >= count)
		return -1;
	if (parser_pattern_template_id_get(ctx->port, ent, &template_id) < 0)
		return -1;
	return snprintf(buf, size, "%u", template_id);
}

/** Complete available actions template IDs. */
static int
comp_actions_template_id(struct context *ctx, const struct token *token,
		 unsigned int ent, char *buf, unsigned int size)
{
	uint16_t count;
	uint32_t template_id;

	(void)token;
	if (parser_port_id_is_invalid(ctx->port) != 0 ||
	    ctx->port == (portid_t)RTE_PORT_ALL)
		return -1;
	count = parser_actions_template_count(ctx->port);
	if (buf == NULL)
		return count;
	if (ent >= count)
		return -1;
	if (parser_actions_template_id_get(ctx->port, ent, &template_id) < 0)
		return -1;
	return snprintf(buf, size, "%u", template_id);
}

/** Complete available table IDs. */
static int
comp_table_id(struct context *ctx, const struct token *token,
	      unsigned int ent, char *buf, unsigned int size)
{
	uint16_t count;
	uint32_t table_id;

	(void)token;
	if (parser_port_id_is_invalid(ctx->port) != 0 ||
	    ctx->port == (portid_t)RTE_PORT_ALL)
		return -1;
	count = parser_table_count(ctx->port);
	if (buf == NULL)
		return count;
	if (ent >= count)
		return -1;
	if (parser_table_id_get(ctx->port, ent, &table_id) < 0)
		return -1;
	return snprintf(buf, size, "%u", table_id);
}

/** Complete available queue IDs. */
static int
comp_queue_id(struct context *ctx, const struct token *token,
	      unsigned int ent, char *buf, unsigned int size)
{
	uint16_t count;

	(void)token;
	if (parser_port_id_is_invalid(ctx->port) != 0 ||
	    ctx->port == (portid_t)RTE_PORT_ALL)
		return -1;
	count = parser_queue_count(ctx->port);
	if (buf == NULL)
		return count;
	if (ent >= count)
		return -1;
	return snprintf(buf, size, "%u", ent);
}

static int
comp_names_to_index(struct context *ctx, const struct token *token,
		    unsigned int ent, char *buf, unsigned int size,
		    const char *const names[], size_t names_size)
{
	RTE_SET_USED(ctx);
	RTE_SET_USED(token);
	if (buf == NULL)
		return names_size;
	if (ent < names_size && names[ent] != NULL)
		return rte_strscpy(buf, names[ent], size);
	return -1;

}

/** Complete available Meter colors. */
static int
comp_meter_color(struct context *ctx, const struct token *token,
		 unsigned int ent, char *buf, unsigned int size)
{
	RTE_SET_USED(ctx);
	RTE_SET_USED(token);
	if (buf == NULL)
		return RTE_DIM(meter_colors);
	if (ent < RTE_DIM(meter_colors) - 1)
		return strlcpy(buf, meter_colors[ent], size);
	return -1;
}

/** Complete available Insertion Table types. */
static int
comp_insertion_table_type(struct context *ctx, const struct token *token,
			  unsigned int ent, char *buf, unsigned int size)
{
	RTE_SET_USED(ctx);
	RTE_SET_USED(token);
	if (buf == NULL)
		return RTE_DIM(table_insertion_types);
	if (ent < RTE_DIM(table_insertion_types) - 1)
		return rte_strscpy(buf, table_insertion_types[ent], size);
	return -1;
}

/** Complete available Hash Calculation Table types. */
static int
comp_hash_table_type(struct context *ctx, const struct token *token,
		     unsigned int ent, char *buf, unsigned int size)
{
	RTE_SET_USED(ctx);
	RTE_SET_USED(token);
	if (buf == NULL)
		return RTE_DIM(table_hash_funcs);
	if (ent < RTE_DIM(table_hash_funcs) - 1)
		return rte_strscpy(buf, table_hash_funcs[ent], size);
	return -1;
}

static int
comp_quota_state_name(struct context *ctx, const struct token *token,
		      unsigned int ent, char *buf, unsigned int size)
{
	return comp_names_to_index(ctx, token, ent, buf, size,
				   quota_state_names,
				   RTE_DIM(quota_state_names));
}

static int
comp_quota_mode_name(struct context *ctx, const struct token *token,
		     unsigned int ent, char *buf, unsigned int size)
{
	return comp_names_to_index(ctx, token, ent, buf, size,
				   quota_mode_names,
				   RTE_DIM(quota_mode_names));
}

static int
comp_quota_update_name(struct context *ctx, const struct token *token,
		       unsigned int ent, char *buf, unsigned int size)
{
	return comp_names_to_index(ctx, token, ent, buf, size,
				   quota_update_names,
				   RTE_DIM(quota_update_names));
}

static int
comp_qu_mode_name(struct context *ctx, const struct token *token,
		  unsigned int ent, char *buf, unsigned int size)
{
	return comp_names_to_index(ctx, token, ent, buf, size,
				   query_update_mode_names,
				   RTE_DIM(query_update_mode_names));
}

/** Global parser instances (cmdline API). */
/** Initialize context. */
static void
cmd_flow_context_init(struct context *ctx)
{
	/* A full memset() is not necessary. */
	ctx->curr = RTE_FLOW_PARSER_CMD_ZERO;
	ctx->prev = RTE_FLOW_PARSER_CMD_ZERO;
	ctx->next_num = 0;
	ctx->args_num = 0;
	ctx->eol = 0;
	ctx->last = 0;
	ctx->port = 0;
	ctx->objdata = 0;
	ctx->object = NULL;
	ctx->objmask = NULL;
}

/** Parse a token (cmdline API). */
static int
cmd_flow_parse(cmdline_parse_token_hdr_t *hdr, const char *src, void *result,
	       unsigned int size)
{
	struct context *ctx = parser_cmd_context();
	const struct token *token;
	const enum rte_flow_parser_command_index *list;
	int len;
	int i;

	(void)hdr;
	token = &token_list[ctx->curr];
	/* Check argument length. */
	ctx->eol = 0;
	ctx->last = 1;
	for (len = 0; src[len]; ++len)
		if (src[len] == '#' || isspace(src[len]))
			break;
	if (len == 0)
		return -1;
	/* Last argument and EOL detection. */
	for (i = len; src[i]; ++i)
		if (src[i] == '#' || src[i] == '\r' || src[i] == '\n')
			break;
		else if (isspace(src[i]) == 0) {
			ctx->last = 0;
			break;
		}
	for (; src[i]; ++i)
		if (src[i] == '\r' || src[i] == '\n') {
			ctx->eol = 1;
			break;
		}
	/* Initialize context if necessary. */
	if (ctx->next_num == 0) {
		if (token->next == NULL)
			return 0;
		ctx->next[ctx->next_num++] = token->next[0];
	}
	/* Process argument through candidates. */
	ctx->prev = ctx->curr;
	list = ctx->next[ctx->next_num - 1];
	for (i = 0; list[i]; ++i) {
		const struct token *next = &token_list[list[i]];
		int tmp;

		ctx->curr = list[i];
		if (next->call != NULL)
			tmp = next->call(ctx, next, src, len, result, size);
		else
			tmp = parse_default(ctx, next, src, len, result, size);
		if (tmp == -1 || tmp != len)
			continue;
		token = next;
		break;
	}
	if (list[i] == RTE_FLOW_PARSER_CMD_ZERO)
		return -1;
	--ctx->next_num;
	/* Push subsequent tokens if any. */
	if (token->next != NULL)
		for (i = 0; token->next[i]; ++i) {
			if (ctx->next_num == RTE_DIM(ctx->next))
				return -1;
			ctx->next[ctx->next_num++] = token->next[i];
		}
	/* Push arguments if any. */
	if (token->args != NULL)
		for (i = 0; token->args[i]; ++i) {
			if (ctx->args_num == RTE_DIM(ctx->args))
				return -1;
			ctx->args[ctx->args_num++] = token->args[i];
	}
	return len;
}

static SLIST_HEAD(, indlst_conf) indlst_conf_head =
	SLIST_HEAD_INITIALIZER();

static void
indirect_action_flow_conf_create(const struct rte_flow_parser_output *in)
{
	int len, ret;
	uint32_t i;
	struct indlst_conf *indlst_conf = NULL;
	size_t base = RTE_ALIGN(sizeof(*indlst_conf), 8);
	struct rte_flow_action *src = in->args.vc.actions;

	if (in->args.vc.actions_n == 0)
		goto end;
	len = rte_flow_conv(RTE_FLOW_CONV_OP_ACTIONS, NULL, 0, src, NULL);
	if (len <= 0)
		goto end;
	len = RTE_ALIGN(len, 16);

	indlst_conf = calloc(1, base + len +
			     in->args.vc.actions_n * sizeof(uintptr_t));
	if (indlst_conf == NULL)
		goto end;
	indlst_conf->id = in->args.vc.attr.group;
	indlst_conf->conf_num = in->args.vc.actions_n - 1;
	indlst_conf->actions = RTE_PTR_ADD(indlst_conf, base);
	ret = rte_flow_conv(RTE_FLOW_CONV_OP_ACTIONS, indlst_conf->actions,
			    len, src, NULL);
	if (ret <= 0) {
		free(indlst_conf);
		indlst_conf = NULL;
		goto end;
	}
	indlst_conf->conf = RTE_PTR_ADD(indlst_conf, base + len);
	for (i = 0; i < indlst_conf->conf_num; i++)
		indlst_conf->conf[i] = indlst_conf->actions[i].conf;
	SLIST_INSERT_HEAD(&indlst_conf_head, indlst_conf, next);
end:
	if (indlst_conf != NULL)
		RTE_LOG_LINE(DEBUG, EAL,
			"created indirect action list configuration %u",
			in->args.vc.attr.group);
	else
		RTE_LOG_LINE(ERR, EAL,
			"cannot create indirect action list configuration %u",
			in->args.vc.attr.group);
}

static const struct indlst_conf *
indirect_action_list_conf_get(uint32_t conf_id)
{
	const struct indlst_conf *conf;

	SLIST_FOREACH(conf, &indlst_conf_head, next) {
		if (conf->id == conf_id)
			return conf;
	}
	return NULL;
}

/** Dispatch parsed buffer to function calls. */
static int
cmd_flow_parsed(struct rte_flow_parser_output *in)
{
	int ret = 0;

	switch (in->command) {
	case RTE_FLOW_PARSER_CMD_INFO:
		ret = parser_command_flow_get_info(in->port);
		break;
	case RTE_FLOW_PARSER_CMD_CONFIGURE:
		ret = parser_command_flow_configure(in->port,
					      &in->args.configure.port_attr,
					      in->args.configure.nb_queue,
					      &in->args.configure.queue_attr);
		break;
	case RTE_FLOW_PARSER_CMD_PATTERN_TEMPLATE_CREATE:
		ret = parser_command_flow_pattern_template_create(in->port,
				in->args.vc.pat_templ_id,
				&((const struct rte_flow_pattern_template_attr) {
					.relaxed_matching = in->args.vc.attr.reserved,
					.ingress = in->args.vc.attr.ingress,
					.egress = in->args.vc.attr.egress,
					.transfer = in->args.vc.attr.transfer,
				}),
				in->args.vc.pattern);
		break;
	case RTE_FLOW_PARSER_CMD_PATTERN_TEMPLATE_DESTROY:
		ret = parser_command_flow_pattern_template_destroy(in->port,
				in->args.templ_destroy.template_id_n,
				in->args.templ_destroy.template_id);
		break;
	case RTE_FLOW_PARSER_CMD_ACTIONS_TEMPLATE_CREATE:
		ret = parser_command_flow_actions_template_create(in->port,
				in->args.vc.act_templ_id,
				&((const struct rte_flow_actions_template_attr) {
					.ingress = in->args.vc.attr.ingress,
					.egress = in->args.vc.attr.egress,
					.transfer = in->args.vc.attr.transfer,
				}),
				in->args.vc.actions,
				in->args.vc.masks);
		break;
	case RTE_FLOW_PARSER_CMD_ACTIONS_TEMPLATE_DESTROY:
		ret = parser_command_flow_actions_template_destroy(in->port,
				in->args.templ_destroy.template_id_n,
				in->args.templ_destroy.template_id);
		break;
	case RTE_FLOW_PARSER_CMD_TABLE_CREATE:
		ret = parser_command_flow_template_table_create(in->port,
			in->args.table.id, &in->args.table.attr,
			in->args.table.pat_templ_id_n,
			in->args.table.pat_templ_id,
			in->args.table.act_templ_id_n,
			in->args.table.act_templ_id);
		break;
	case RTE_FLOW_PARSER_CMD_TABLE_DESTROY:
		ret = parser_command_flow_template_table_destroy(in->port,
					in->args.table_destroy.table_id_n,
					in->args.table_destroy.table_id);
		break;
	case RTE_FLOW_PARSER_CMD_TABLE_RESIZE_COMPLETE:
		ret = parser_command_flow_template_table_resize_complete
			(in->port, in->args.table_destroy.table_id[0]);
		break;
	case RTE_FLOW_PARSER_CMD_GROUP_SET_MISS_ACTIONS:
		ret = parser_command_queue_group_set_miss_actions(in->port,
							    &in->args.vc.attr,
							    in->args.vc.actions);
		break;
	case RTE_FLOW_PARSER_CMD_TABLE_RESIZE:
		ret = parser_command_flow_template_table_resize(in->port,
				in->args.table.id,
				in->args.table.attr.nb_flows);
		break;
	case RTE_FLOW_PARSER_CMD_QUEUE_CREATE:
		ret = parser_command_queue_flow_create(in->port, in->queue,
			in->postpone, in->args.vc.table_id, in->args.vc.rule_id,
			in->args.vc.pat_templ_id, in->args.vc.act_templ_id,
			in->args.vc.pattern, in->args.vc.actions);
		break;
	case RTE_FLOW_PARSER_CMD_QUEUE_DESTROY:
		ret = parser_command_queue_flow_destroy(in->port, in->queue,
				in->postpone, in->args.destroy.rule_n,
				in->args.destroy.rule);
		break;
	case RTE_FLOW_PARSER_CMD_QUEUE_FLOW_UPDATE_RESIZED:
		ret = parser_command_queue_flow_update_resized(in->port, in->queue,
					       in->postpone,
					       (uint32_t)in->args.destroy.rule[0]);
		break;
	case RTE_FLOW_PARSER_CMD_QUEUE_UPDATE:
		ret = parser_command_queue_flow_update(in->port, in->queue,
				in->postpone, in->args.vc.rule_id,
				in->args.vc.act_templ_id, in->args.vc.actions);
		break;
	case RTE_FLOW_PARSER_CMD_PUSH:
		ret = parser_command_queue_flow_push(in->port, in->queue);
		break;
	case RTE_FLOW_PARSER_CMD_PULL:
		ret = parser_command_queue_flow_pull(in->port, in->queue);
		break;
	case RTE_FLOW_PARSER_CMD_HASH:
		if (in->args.vc.encap_hash == 0)
			ret = parser_command_flow_hash_calc(in->port,
					in->args.vc.table_id,
					in->args.vc.pat_templ_id,
					in->args.vc.pattern);
		else
			ret = parser_command_flow_hash_calc_encap(in->port,
					in->args.vc.field,
					in->args.vc.pattern);
		break;
	case RTE_FLOW_PARSER_CMD_QUEUE_AGED:
		parser_command_queue_flow_aged(in->port, in->queue,
					       in->args.aged.destroy);
		break;
	case RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_CREATE:
	case RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_LIST_CREATE:
		ret = parser_command_queue_action_handle_create(
				in->port, in->queue, in->postpone,
				in->args.vc.attr.group,
				in->command == RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_LIST_CREATE,
				&((const struct rte_flow_indir_action_conf) {
					.ingress = in->args.vc.attr.ingress,
					.egress = in->args.vc.attr.egress,
					.transfer = in->args.vc.attr.transfer,
				}),
				in->args.vc.actions);
		break;
	case RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_DESTROY:
		ret = parser_command_queue_action_handle_destroy(in->port,
				in->queue, in->postpone,
				in->args.ia_destroy.action_id_n,
				in->args.ia_destroy.action_id);
		break;
	case RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_UPDATE:
		ret = parser_command_queue_action_handle_update(in->port,
				in->queue, in->postpone,
				in->args.vc.attr.group,
				in->args.vc.actions);
		break;
	case RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_QUERY:
		ret = parser_command_queue_action_handle_query(in->port,
				in->queue, in->postpone,
				in->args.ia.action_id);
		break;
	case RTE_FLOW_PARSER_CMD_QUEUE_INDIRECT_ACTION_QUERY_UPDATE:
		parser_command_queue_action_handle_query_update(in->port,
				in->queue, in->postpone,
				in->args.ia.action_id,
				in->args.ia.qu_mode,
				in->args.vc.actions);
		break;
	case RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_CREATE:
	case RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_LIST_CREATE:
		ret = parser_command_action_handle_create(
				in->port, in->args.vc.attr.group,
				in->command == RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_LIST_CREATE,
				&((const struct rte_flow_indir_action_conf) {
					.ingress = in->args.vc.attr.ingress,
					.egress = in->args.vc.attr.egress,
					.transfer = in->args.vc.attr.transfer,
				}),
				in->args.vc.actions);
		break;
	case RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_FLOW_CONF_CREATE:
		indirect_action_flow_conf_create(in);
		break;
	case RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_DESTROY:
		ret = parser_command_action_handle_destroy(in->port,
				in->args.ia_destroy.action_id_n,
				in->args.ia_destroy.action_id);
		break;
	case RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_UPDATE:
		ret = parser_command_action_handle_update(in->port,
				in->args.vc.attr.group, in->args.vc.actions);
		break;
	case RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_QUERY:
		ret = parser_command_action_handle_query(in->port,
				in->args.ia.action_id);
		break;
	case RTE_FLOW_PARSER_CMD_INDIRECT_ACTION_QUERY_UPDATE:
		parser_command_action_handle_query_update(in->port,
				in->args.ia.action_id, in->args.ia.qu_mode,
				in->args.vc.actions);
		break;
	case RTE_FLOW_PARSER_CMD_VALIDATE:
		ret = parser_command_flow_validate(in->port, &in->args.vc.attr,
				in->args.vc.pattern, in->args.vc.actions,
				(const struct rte_flow_parser_tunnel_ops *)
				&in->args.vc.tunnel_ops);
		break;
	case RTE_FLOW_PARSER_CMD_CREATE:
		ret = parser_command_flow_create(in->port, &in->args.vc.attr,
				in->args.vc.pattern, in->args.vc.actions,
				(const struct rte_flow_parser_tunnel_ops *)
				&in->args.vc.tunnel_ops, in->args.vc.user_id);
		break;
	case RTE_FLOW_PARSER_CMD_DESTROY:
		ret = parser_command_flow_destroy(in->port,
				in->args.destroy.rule_n,
				in->args.destroy.rule,
				in->args.destroy.is_user_id);
		break;
	case RTE_FLOW_PARSER_CMD_UPDATE:
		ret = parser_command_flow_update(in->port, in->args.vc.rule_id,
				in->args.vc.actions, in->args.vc.is_user_id);
		break;
	case RTE_FLOW_PARSER_CMD_FLUSH:
		ret = parser_command_flow_flush(in->port);
		break;
	case RTE_FLOW_PARSER_CMD_DUMP_ONE:
	case RTE_FLOW_PARSER_CMD_DUMP_ALL:
		ret = parser_command_flow_dump(in->port, in->args.dump.mode,
				in->args.dump.rule, in->args.dump.file,
				in->args.dump.is_user_id);
		break;
	case RTE_FLOW_PARSER_CMD_QUERY:
		ret = parser_command_flow_query(in->port, in->args.query.rule,
				&in->args.query.action,
				in->args.query.is_user_id);
		break;
	case RTE_FLOW_PARSER_CMD_LIST:
		parser_command_flow_list(in->port, in->args.list.group_n,
					 in->args.list.group);
		break;
	case RTE_FLOW_PARSER_CMD_ISOLATE:
		ret = parser_command_flow_isolate(in->port, in->args.isolate.set);
		break;
	case RTE_FLOW_PARSER_CMD_AGED:
		parser_command_flow_aged(in->port, in->args.aged.destroy);
		break;
	case RTE_FLOW_PARSER_CMD_TUNNEL_CREATE:
		parser_command_flow_tunnel_create(in->port,
				(const struct rte_flow_parser_tunnel_ops *)
				&in->args.vc.tunnel_ops);
		break;
	case RTE_FLOW_PARSER_CMD_TUNNEL_DESTROY:
		parser_command_flow_tunnel_destroy(in->port,
				in->args.vc.tunnel_ops.id);
		break;
	case RTE_FLOW_PARSER_CMD_TUNNEL_LIST:
		parser_command_flow_tunnel_list(in->port);
		break;
	case RTE_FLOW_PARSER_CMD_ACTION_POL_G:
		ret = parser_command_meter_policy_add(in->port,
				in->args.policy.policy_id,
				in->args.vc.actions);
		break;
	case RTE_FLOW_PARSER_CMD_FLEX_ITEM_CREATE:
		parser_command_flex_item_create(in->port,
				in->args.flex.token, in->args.flex.filename);
		break;
	case RTE_FLOW_PARSER_CMD_FLEX_ITEM_DESTROY:
		parser_command_flex_item_destroy(in->port,
				in->args.flex.token);
		break;
	default:
		break;
	}
	fflush(stdout);
	return ret;
}

/** Dispatch parsed buffer to function calls. */
static void
cmd_set_raw_parsed(const struct rte_flow_parser_output *in)
{
	uint16_t idx = in->port; /* We borrow port field as index */

	switch (in->command) {
	case RTE_FLOW_PARSER_CMD_SET_SAMPLE_ACTIONS:
		parser_command_set_sample_actions(idx, in->args.vc.actions,
						  in->args.vc.actions_n);
		break;
	case RTE_FLOW_PARSER_CMD_SET_IPV6_EXT_PUSH:
		parser_command_set_ipv6_ext_push(idx, in->args.vc.pattern,
						 in->args.vc.pattern_n);
		break;
	case RTE_FLOW_PARSER_CMD_SET_IPV6_EXT_REMOVE:
		parser_command_set_ipv6_ext_remove(idx, in->args.vc.pattern,
						   in->args.vc.pattern_n);
		break;
	case RTE_FLOW_PARSER_CMD_SET_RAW_ENCAP:
		parser_command_set_raw_encap(idx, in->args.vc.pattern,
					     in->args.vc.pattern_n);
		break;
	case RTE_FLOW_PARSER_CMD_SET_RAW_DECAP:
		parser_command_set_raw_decap(idx, in->args.vc.pattern,
					     in->args.vc.pattern_n);
		break;
	default:
		break;
	}
}

/* Cmdline instances are owned by applications; keep weak references for help. */
static cmdline_parse_inst_t *cmd_flow_inst;
static cmdline_parse_inst_t *cmd_set_raw_inst;

/** Return number of completion entries (cmdline API). */
static int
cmd_flow_complete_get_nb(cmdline_parse_token_hdr_t *hdr)
{
	struct context *ctx = parser_cmd_context();
	const struct token *token = &token_list[ctx->curr];
	const enum rte_flow_parser_command_index *list;
	int i;

	RTE_SET_USED(hdr);
	/* Count number of tokens in current list. */
	if (ctx->next_num != 0)
		list = ctx->next[ctx->next_num - 1];
	else if (token->next != NULL)
		list = token->next[0];
	else
		return 0;
	for (i = 0; list[i]; ++i)
		;
	if (i == 0)
		return 0;
	/*
	 * If there is a single token, use its completion callback, otherwise
	 * return the number of entries.
	 */
	token = &token_list[list[0]];
	if (i == 1 && token->comp) {
		/* Save index for cmd_flow_get_help(). */
		ctx->prev = list[0];
		return token->comp(ctx, token, 0, NULL, 0);
	}
	return i;
}

/** Return a completion entry (cmdline API). */
static int
cmd_flow_complete_get_elt(cmdline_parse_token_hdr_t *hdr, int index,
			  char *dst, unsigned int size)
{
	struct context *ctx = parser_cmd_context();
	const struct token *token = &token_list[ctx->curr];
	const enum rte_flow_parser_command_index *list;
	int i;

	RTE_SET_USED(hdr);
	/* Count number of tokens in current list. */
	if (ctx->next_num != 0)
		list = ctx->next[ctx->next_num - 1];
	else if (token->next != NULL)
		list = token->next[0];
	else
		return -1;
	for (i = 0; list[i]; ++i)
		;
	if (i == 0)
		return -1;
	/* If there is a single token, use its completion callback. */
	token = &token_list[list[0]];
	if (i == 1 && token->comp) {
		/* Save index for cmd_flow_get_help(). */
		ctx->prev = list[0];
		return token->comp(ctx, token, (unsigned int)index, dst, size) <
		       0 ? -1 : 0;
	}
	/* Otherwise make sure the index is valid and use defaults. */
	if (index >= i)
		return -1;
	token = &token_list[list[index]];
	strlcpy(dst, token->name, size);
	/* Save index for cmd_flow_get_help(). */
	ctx->prev = list[index];
	return 0;
}

/** Populate help strings for current token (cmdline API). */
static int
cmd_flow_get_help(cmdline_parse_token_hdr_t *hdr, char *dst, unsigned int size)
{
	struct context *ctx = parser_cmd_context();
	const struct token *token = &token_list[ctx->prev];

	RTE_SET_USED(hdr);
	if (size == 0)
		return -1;
	/* Set token type and update global help with details. */
	strlcpy(dst, (token->type ? token->type : "TOKEN"), size);
	if (cmd_flow_inst != NULL) {
		if (token->help != NULL)
			cmd_flow_inst->help_str = token->help;
		else
			cmd_flow_inst->help_str = token->name;
	}
	return 0;
}

/** Token definition template (cmdline API). */
static struct cmdline_token_hdr cmd_flow_token_hdr = {
	.ops = &(struct cmdline_token_ops){
		.parse = cmd_flow_parse,
		.complete_get_nb = cmd_flow_complete_get_nb,
		.complete_get_elt = cmd_flow_complete_get_elt,
		.get_help = cmd_flow_get_help,
	},
	.offset = 0,
};

/** Populate the next dynamic token. */
static void
cmd_flow_tok(cmdline_parse_token_hdr_t **hdr,
	     cmdline_parse_token_hdr_t **hdr_inst)
{
	struct context *ctx = parser_cmd_context();
	cmdline_parse_token_hdr_t **tokens;

	tokens = cmd_flow_inst ? cmd_flow_inst->tokens : NULL;
	if (tokens == NULL) {
		*hdr = NULL;
		return;
	}
	/* Always reinitialize context before requesting the first token. */
	if ((hdr_inst - tokens) == 0)
		cmd_flow_context_init(ctx);
	/* Return NULL when no more tokens are expected. */
	if (ctx->next_num == 0 && ctx->curr != 0) {
		*hdr = NULL;
		return;
	}
	/* Determine if command should end here. */
	if (ctx->eol != 0 && ctx->last != 0 && ctx->next_num != 0) {
		const enum rte_flow_parser_command_index *list = ctx->next[ctx->next_num - 1];
		int i;

		for (i = 0; list[i]; ++i) {
			if (list[i] != RTE_FLOW_PARSER_CMD_END)
				continue;
			*hdr = NULL;
			return;
		}
	}
	*hdr = &cmd_flow_token_hdr;
}

static int
cmd_set_raw_get_help(cmdline_parse_token_hdr_t *hdr, char *dst,
		     unsigned int size)
{
	struct context *ctx = parser_cmd_context();
	const struct token *token = &token_list[ctx->prev];

	RTE_SET_USED(hdr);
	if (size == 0)
		return -1;
	/* Set token type and update global help with details. */
	snprintf(dst, size, "%s", (token->type ? token->type : "TOKEN"));
	if (cmd_set_raw_inst != NULL) {
		if (token->help != NULL)
			cmd_set_raw_inst->help_str = token->help;
		else
			cmd_set_raw_inst->help_str = token->name;
	}
	return 0;
}

/** Token definition template for set command (cmdline API). */
static struct cmdline_token_hdr cmd_set_raw_token_hdr = {
	.ops = &(struct cmdline_token_ops){
		.parse = cmd_flow_parse,
		.complete_get_nb = cmd_flow_complete_get_nb,
		.complete_get_elt = cmd_flow_complete_get_elt,
		.get_help = cmd_set_raw_get_help,
	},
	.offset = 0,
};

/** Populate the next dynamic token for set command. */
static void
cmd_set_raw_tok(cmdline_parse_token_hdr_t **hdr,
		cmdline_parse_token_hdr_t **hdr_inst)
{
	struct context *ctx = parser_cmd_context();
	cmdline_parse_token_hdr_t **tokens;

	tokens = cmd_set_raw_inst ? cmd_set_raw_inst->tokens : NULL;
	if (tokens == NULL) {
		*hdr = NULL;
		return;
	}
	/* Always reinitialize context before requesting the first token. */
	if ((hdr_inst - tokens) == 0) {
		cmd_flow_context_init(ctx);
		ctx->curr = RTE_FLOW_PARSER_CMD_START_SET;
	}
	/* Return NULL when no more tokens are expected. */
	if (ctx->next_num == 0 && (ctx->curr != RTE_FLOW_PARSER_CMD_START_SET)) {
		*hdr = NULL;
		return;
	}
	/* Determine if command should end here. */
	if (ctx->eol != 0 && ctx->last != 0 && ctx->next_num != 0) {
		const enum rte_flow_parser_command_index *list = ctx->next[ctx->next_num - 1];
		int i;

		for (i = 0; list[i]; ++i) {
			if (list[i] != RTE_FLOW_PARSER_CMD_END_SET)
				continue;
			*hdr = NULL;
			return;
		}
	}
	*hdr = &cmd_set_raw_token_hdr;
}

void
rte_flow_parser_cmdline_register(cmdline_parse_inst_t *flow,
				 cmdline_parse_inst_t *set_raw)
{
	cmd_flow_inst = flow;
	cmd_set_raw_inst = set_raw;
}

void
rte_flow_parser_cmd_flow_tok(cmdline_parse_token_hdr_t **hdr,
			     cmdline_parse_token_hdr_t **hdr_inst)
{
	cmd_flow_tok(hdr, hdr_inst);
}

void
rte_flow_parser_cmd_set_raw_tok(cmdline_parse_token_hdr_t **hdr,
				cmdline_parse_token_hdr_t **hdr_inst)
{
	cmd_set_raw_tok(hdr, hdr_inst);
}

void
rte_flow_parser_cmd_flow_dispatch(struct rte_flow_parser_output *out)
{
	if (out == NULL)
		return;
	(void)cmd_flow_parsed((struct rte_flow_parser_output *)out);
}

void
rte_flow_parser_cmd_set_raw_dispatch(struct rte_flow_parser_output *out)
{
	if (out == NULL)
		return;
	cmd_set_raw_parsed((struct rte_flow_parser_output *)out);
}

int
rte_flow_parser_parse(const char *src,
		      struct rte_flow_parser_output *result,
		      size_t result_size)
{
	struct context *ctx;
	const char *pos;
	const char *start;
	size_t tok_len;
	int ret;

	if (src == NULL || result == NULL)
		return -EINVAL;
	if (result_size < sizeof(struct rte_flow_parser_output))
		return -ENOBUFS;
	ctx = parser_cmd_context();
	cmd_flow_context_init(ctx);
	start = src;
	while (isspace((unsigned char)*start))
		start++;
	tok_len = 0;
	while (start[tok_len] != '\0' &&
	       !isspace((unsigned char)start[tok_len]) &&
	       start[tok_len] != '#')
		tok_len++;
	if (tok_len == 3 && strncmp(start, "set", tok_len) == 0)
		ctx->curr = RTE_FLOW_PARSER_CMD_START_SET;
	pos = src;
	do {
		ret = cmd_flow_parse(NULL, pos, result,
				     (unsigned int)result_size);
		if (ret > 0) {
			pos += ret;
			while (isspace((unsigned char)*pos))
				pos++;
		}
	} while (ret > 0 && *pos);
	if (ret < 0)
		return ret;
	if (*pos != '\0')
		return -EINVAL;
	return 0;
}

int
rte_flow_parser_run(const char *src)
{
	uint8_t buf[4096];
	struct rte_flow_parser_output *out = (struct rte_flow_parser_output *)buf;
	int ret;

	ret = rte_flow_parser_parse(src,
				    (struct rte_flow_parser_output *)buf,
				    sizeof(buf));
	if (ret < 0)
		return ret;
	switch (out->command) {
	case RTE_FLOW_PARSER_CMD_SET_SAMPLE_ACTIONS:
	case RTE_FLOW_PARSER_CMD_SET_IPV6_EXT_PUSH:
	case RTE_FLOW_PARSER_CMD_SET_IPV6_EXT_REMOVE:
	case RTE_FLOW_PARSER_CMD_SET_RAW_ENCAP:
	case RTE_FLOW_PARSER_CMD_SET_RAW_DECAP:
		cmd_set_raw_parsed(out);
		break;
	default:
		return cmd_flow_parsed(out);
		break;
	}
	return 0;
}

static int
parser_simple_parse(const char *cmd, struct rte_flow_parser_output **out)
{
	uint8_t *buf = RTE_PER_LCORE(flow_parser_simple_parse_buf).buf;
	int ret;

	if (cmd == NULL || out == NULL)
		return -EINVAL;
	memset(buf, 0, sizeof(RTE_PER_LCORE(flow_parser_simple_parse_buf).buf));
	ret = rte_flow_parser_parse(cmd, (struct rte_flow_parser_output *)buf,
				    sizeof(RTE_PER_LCORE(flow_parser_simple_parse_buf).buf));
	if (ret < 0)
		return ret;
	*out = (struct rte_flow_parser_output *)buf;
	return 0;
}

static int
parser_format_cmd(char **dst, const char *prefix, const char *body,
		  const char *suffix)
{
	size_t len;

	if (dst == NULL || prefix == NULL || body == NULL || suffix == NULL)
		return -EINVAL;
	len = strlen(prefix) + strlen(body) + strlen(suffix) + 1;
	*dst = malloc(len);
	if (*dst == NULL)
		return -ENOMEM;
	snprintf(*dst, len, "%s%s%s", prefix, body, suffix);
	return 0;
}

static bool
parser_str_has_trailing_end(const char *src)
{
	const char *p;
	const char *q;

	if (src == NULL)
		return false;
	p = src + strlen(src);
	while (p > src && isspace((unsigned char)p[-1]))
		p--;
	if (p - src < 3)
		return false;
	if (strncmp(p - 3, "end", 3) != 0)
		return false;
	q = p - 3;
	while (q > src && isspace((unsigned char)q[-1]))
		q--;
	if (q <= src || q[-1] != '/')
		return false;
	return true;
}

int
rte_flow_parser_parse_attr_str(const char *src, struct rte_flow_attr *attr)
{
	struct rte_flow_parser_output *out;
	char *cmd = NULL;
	int ret;

	if (src == NULL || attr == NULL)
		return -EINVAL;
	ret = parser_format_cmd(&cmd, "flow validate 0 ",
				src, " pattern eth / end actions drop / end");
	if (ret != 0)
		return ret;
	ret = parser_simple_parse(cmd, &out);
	free(cmd);
	if (ret != 0)
		return ret;
	*attr = out->args.vc.attr;
	return 0;
}

int
rte_flow_parser_parse_pattern_str(const char *src,
				  const struct rte_flow_item **pattern,
				  uint32_t *pattern_n)
{
	struct rte_flow_parser_output *out;
	char *cmd = NULL;
	int ret;

	if (src == NULL || pattern == NULL || pattern_n == NULL)
		return -EINVAL;
	ret = parser_format_cmd(&cmd, "flow validate 0 ingress pattern ",
				src, " actions drop / end");
	if (ret != 0)
		return ret;
	ret = parser_simple_parse(cmd, &out);
	free(cmd);
	if (ret != 0)
		return ret;
	*pattern = out->args.vc.pattern;
	*pattern_n = out->args.vc.pattern_n;
	return 0;
}

int
rte_flow_parser_parse_actions_str(const char *src,
				  const struct rte_flow_action **actions,
				  uint32_t *actions_n)
{
	struct rte_flow_parser_output *out;
	char *cmd = NULL;
	const char *suffix;
	int ret;

	if (src == NULL || actions == NULL || actions_n == NULL)
		return -EINVAL;
	suffix = parser_str_has_trailing_end(src) ? "" : " / end";
	ret = parser_format_cmd(&cmd, "flow validate 0 ingress pattern eth / end actions ",
				src, suffix);
	if (ret != 0)
		return ret;
	ret = parser_simple_parse(cmd, &out);
	free(cmd);
	if (ret != 0)
		return ret;
	*actions = out->args.vc.actions;
	*actions_n = out->args.vc.actions_n;
	return 0;
}

/* Experimental API exports */
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_parser_init, 26.03);
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_parser_parse_attr_str, 26.03);
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_parser_parse_pattern_str, 26.03);
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_parser_parse_actions_str, 26.03);
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_parser_reset_defaults, 26.03);
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_parser_vxlan_encap_conf, 26.03);
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_parser_nvgre_encap_conf, 26.03);
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_parser_l2_encap_conf, 26.03);
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_parser_l2_decap_conf, 26.03);
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_parser_mplsogre_encap_conf, 26.03);
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_parser_mplsogre_decap_conf, 26.03);
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_parser_mplsoudp_encap_conf, 26.03);
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_parser_mplsoudp_decap_conf, 26.03);
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_parser_raw_encap_conf_get, 26.03);
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_parser_raw_decap_conf_get, 26.03);
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_parser_conntrack_context, 26.03);
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_parser_cmdline_register, 26.03);
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_parser_cmd_flow_tok, 26.03);
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_parser_cmd_set_raw_tok, 26.03);
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_parser_cmd_flow_dispatch, 26.03);
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_parser_cmd_set_raw_dispatch, 26.03);
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_parser_parse, 26.03);
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_flow_parser_run, 26.03);
