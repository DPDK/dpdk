/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include <sys/queue.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#include <rte_io.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <ethdev_driver.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_eth_ctrl.h>
#include <rte_tailq.h>
#include <rte_rawdev.h>
#include <rte_rawdev_pmd.h>
#include <rte_bus_ifpga.h>
#include <ifpga_common.h>
#include <ifpga_logs.h>
#include <ifpga_rawdev.h>

#include "ipn3ke_rawdev_api.h"
#include "ipn3ke_flow.h"
#include "ipn3ke_logs.h"
#include "ipn3ke_ethdev.h"

/** Static initializer for items. */
#define FLOW_PATTERNS(...) \
	((const enum rte_flow_item_type []) { \
		__VA_ARGS__, RTE_FLOW_ITEM_TYPE_END, \
	})

enum IPN3KE_HASH_KEY_TYPE {
	IPN3KE_HASH_KEY_VXLAN,
	IPN3KE_HASH_KEY_MAC,
	IPN3KE_HASH_KEY_QINQ,
	IPN3KE_HASH_KEY_MPLS,
	IPN3KE_HASH_KEY_IP_TCP,
	IPN3KE_HASH_KEY_IP_UDP,
	IPN3KE_HASH_KEY_IP_NVGRE,
	IPN3KE_HASH_KEY_VXLAN_IP_UDP,
};

struct ipn3ke_flow_parse {
	uint32_t mark:1; /**< Set if the flow is marked. */
	uint32_t drop:1; /**< ACL drop. */
	uint32_t key_type:IPN3KE_FLOW_KEY_ID_BITS;
	uint32_t mark_id:IPN3KE_FLOW_RESULT_UID_BITS; /**< Mark identifier. */
	uint8_t key_len; /**< Length in bit. */
	uint8_t key[BITS_TO_BYTES(IPN3KE_FLOW_KEY_DATA_BITS)];
		/**< key1, key2 */
};

typedef int (*pattern_filter_t)(const struct rte_flow_item patterns[],
	struct rte_flow_error *error, struct ipn3ke_flow_parse *parser);


struct ipn3ke_flow_pattern {
	const enum rte_flow_item_type *const items;

	pattern_filter_t filter;
};

/*
 * @ RTL definition:
 * typedef struct packed {
 * logic [47:0]    vxlan_inner_mac;
 * logic [23:0]    vxlan_vni;
 * } Hash_Key_Vxlan_t;
 *
 * @ flow items:
 * RTE_FLOW_ITEM_TYPE_VXLAN
 * RTE_FLOW_ITEM_TYPE_ETH
 */
static int
ipn3ke_pattern_vxlan(const struct rte_flow_item patterns[],
	struct rte_flow_error *error, struct ipn3ke_flow_parse *parser)
{
	const struct rte_flow_item_vxlan *vxlan = NULL;
	const struct rte_flow_item_eth *eth = NULL;
	const struct rte_flow_item *item;

	for (item = patterns; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		if (/*!item->spec || item->mask || */item->last) {
			rte_flow_error_set(error,
					EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM,
					item,
					"Only support item with 'spec'");
			return -rte_errno;
		}

		switch (item->type) {
		case RTE_FLOW_ITEM_TYPE_ETH:
			eth = item->spec;

			rte_memcpy(&parser->key[0],
					eth->src.addr_bytes,
					RTE_ETHER_ADDR_LEN);
			break;

		case RTE_FLOW_ITEM_TYPE_VXLAN:
			vxlan = item->spec;

			rte_memcpy(&parser->key[6], vxlan->vni, 3);
			break;

		default:
			rte_flow_error_set(error,
					EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM,
					item,
					"Not support item type");
			return -rte_errno;
		}
	}

	if (vxlan != NULL && eth != NULL) {
		parser->key_len = 48 + 24;
		return 0;
	}

	rte_flow_error_set(error,
			EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			patterns,
			"Missed some patterns");
	return -rte_errno;
}

/*
 * @ RTL definition:
 * typedef struct packed {
 * logic [47:0]    eth_smac;
 * } Hash_Key_Mac_t;
 *
 * @ flow items:
 * RTE_FLOW_ITEM_TYPE_ETH
 */
static int
ipn3ke_pattern_mac(const struct rte_flow_item patterns[],
	struct rte_flow_error *error, struct ipn3ke_flow_parse *parser)
{
	const struct rte_flow_item_eth *eth = NULL;
	const struct rte_flow_item *item;

	for (item = patterns; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		if (!item->spec || item->mask || item->last) {
			rte_flow_error_set(error,
					EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM,
					item,
					"Only support item with 'spec'");
			return -rte_errno;
		}

		switch (item->type) {
		case RTE_FLOW_ITEM_TYPE_ETH:
			eth = item->spec;

			rte_memcpy(parser->key,
					eth->src.addr_bytes,
					RTE_ETHER_ADDR_LEN);
			break;

		default:
			rte_flow_error_set(error,
					EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM,
					item,
					"Not support item type");
			return -rte_errno;
		}
	}

	if (eth != NULL) {
		parser->key_len = 48;
		return 0;
	}

	rte_flow_error_set(error,
			EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			patterns,
			"Missed some patterns");
	return -rte_errno;
}

/*
 * @ RTL definition:
 * typedef struct packed {
 * logic [11:0]    outer_vlan_id;
 * logic [11:0]    inner_vlan_id;
 * } Hash_Key_QinQ_t;
 *
 * @ flow items:
 * RTE_FLOW_ITEM_TYPE_VLAN
 * RTE_FLOW_ITEM_TYPE_VLAN
 */
static int
ipn3ke_pattern_qinq(const struct rte_flow_item patterns[],
	struct rte_flow_error *error, struct ipn3ke_flow_parse *parser)
{
	const struct rte_flow_item_vlan *outer_vlan = NULL;
	const struct rte_flow_item_vlan *inner_vlan = NULL;
	const struct rte_flow_item *item;
	uint16_t tci;

	for (item = patterns; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		if (!item->spec || item->mask || item->last) {
			rte_flow_error_set(error,
					EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM,
					item,
					"Only support item with 'spec'");
			return -rte_errno;
		}

		switch (item->type) {
		case RTE_FLOW_ITEM_TYPE_VLAN:
			if (!outer_vlan) {
				outer_vlan = item->spec;

				tci = rte_be_to_cpu_16(outer_vlan->tci);
				parser->key[0]  = (tci & 0xff0) >> 4;
				parser->key[1] |= (tci & 0x00f) << 4;
			} else {
				inner_vlan = item->spec;

				tci = rte_be_to_cpu_16(inner_vlan->tci);
				parser->key[1] |= (tci & 0xf00) >> 8;
				parser->key[2]  = (tci & 0x0ff);
			}
			break;

		default:
			rte_flow_error_set(error,
					EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM,
					item,
					"Not support item type");
			return -rte_errno;
		}
	}

	if (outer_vlan != NULL && inner_vlan != NULL) {
		parser->key_len = 12 + 12;
		return 0;
	}

	rte_flow_error_set(error,
			EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			patterns,
			"Missed some patterns");
	return -rte_errno;
}

/*
 * @ RTL definition:
 * typedef struct packed {
 * logic [19:0]    mpls_label1;
 * logic [19:0]    mpls_label2;
 * } Hash_Key_Mpls_t;
 *
 * @ flow items:
 * RTE_FLOW_ITEM_TYPE_MPLS
 * RTE_FLOW_ITEM_TYPE_MPLS
 */
static int
ipn3ke_pattern_mpls(const struct rte_flow_item patterns[],
	struct rte_flow_error *error, struct ipn3ke_flow_parse *parser)
{
	const struct rte_flow_item_mpls *mpls1 = NULL;
	const struct rte_flow_item_mpls *mpls2 = NULL;
	const struct rte_flow_item *item;

	for (item = patterns; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		if (!item->spec || item->mask || item->last) {
			rte_flow_error_set(error,
					EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM,
					item,
					"Only support item with 'spec'");
			return -rte_errno;
		}

		switch (item->type) {
		case RTE_FLOW_ITEM_TYPE_MPLS:
			if (!mpls1) {
				mpls1 = item->spec;

				parser->key[0] = mpls1->label_tc_s[0];
				parser->key[1] = mpls1->label_tc_s[1];
				parser->key[2] = mpls1->label_tc_s[2] & 0xf0;
			} else {
				mpls2 = item->spec;

				parser->key[2] |=
					((mpls2->label_tc_s[0] & 0xf0) >> 4);
				parser->key[3] =
					((mpls2->label_tc_s[0] & 0xf) << 4) |
					((mpls2->label_tc_s[1] & 0xf0) >> 4);
				parser->key[4] =
					((mpls2->label_tc_s[1] & 0xf) << 4) |
					((mpls2->label_tc_s[2] & 0xf0) >> 4);
			}
			break;

		default:
			rte_flow_error_set(error,
					EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM,
					item,
					"Not support item type");
			return -rte_errno;
		}
	}

	if (mpls1 != NULL && mpls2 != NULL) {
		parser->key_len = 20 + 20;
		return 0;
	}

	rte_flow_error_set(error,
			EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			patterns,
			"Missed some patterns");
	return -rte_errno;
}

/*
 * @ RTL definition:
 * typedef struct packed {
 * logic [31:0]    ip_sa;
 * logic [15:0]    tcp_sport;
 * } Hash_Key_Ip_Tcp_t;
 *
 * @ flow items:
 * RTE_FLOW_ITEM_TYPE_IPV4
 * RTE_FLOW_ITEM_TYPE_TCP
 */
static int
ipn3ke_pattern_ip_tcp(const struct rte_flow_item patterns[],
	struct rte_flow_error *error, struct ipn3ke_flow_parse *parser)
{
	const struct rte_flow_item_ipv4 *ipv4 = NULL;
	const struct rte_flow_item_tcp *tcp = NULL;
	const struct rte_flow_item *item;

	for (item = patterns; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		if (!item->spec || item->mask || item->last) {
			rte_flow_error_set(error,
					EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM,
					item,
					"Only support item with 'spec'");
			return -rte_errno;
		}

		switch (item->type) {
		case RTE_FLOW_ITEM_TYPE_IPV4:
			ipv4 = item->spec;

			rte_memcpy(&parser->key[0], &ipv4->hdr.src_addr, 4);
			break;

		case RTE_FLOW_ITEM_TYPE_TCP:
			tcp = item->spec;

			rte_memcpy(&parser->key[4], &tcp->hdr.src_port, 2);
			break;

		default:
			rte_flow_error_set(error,
					EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM,
					item,
					"Not support item type");
			return -rte_errno;
		}
	}

	if (ipv4 != NULL && tcp != NULL) {
		parser->key_len = 32 + 16;
		return 0;
	}

	rte_flow_error_set(error,
			EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			patterns,
			"Missed some patterns");
	return -rte_errno;
}

/*
 * @ RTL definition:
 * typedef struct packed {
 * logic [31:0]    ip_sa;
 * logic [15:0]    udp_sport;
 * } Hash_Key_Ip_Udp_t;
 *
 * @ flow items:
 * RTE_FLOW_ITEM_TYPE_IPV4
 * RTE_FLOW_ITEM_TYPE_UDP
 */
static int
ipn3ke_pattern_ip_udp(const struct rte_flow_item patterns[],
	struct rte_flow_error *error, struct ipn3ke_flow_parse *parser)
{
	const struct rte_flow_item_ipv4 *ipv4 = NULL;
	const struct rte_flow_item_udp *udp = NULL;
	const struct rte_flow_item *item;

	for (item = patterns; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		if (!item->spec || item->mask || item->last) {
			rte_flow_error_set(error,
					EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM,
					item,
					"Only support item with 'spec'");
			return -rte_errno;
		}

		switch (item->type) {
		case RTE_FLOW_ITEM_TYPE_IPV4:
			ipv4 = item->spec;

			rte_memcpy(&parser->key[0], &ipv4->hdr.src_addr, 4);
			break;

		case RTE_FLOW_ITEM_TYPE_UDP:
			udp = item->spec;

			rte_memcpy(&parser->key[4], &udp->hdr.src_port, 2);
			break;

		default:
			rte_flow_error_set(error,
					EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM,
					item,
					"Not support item type");
			return -rte_errno;
		}
	}

	if (ipv4 != NULL && udp != NULL) {
		parser->key_len = 32 + 16;
		return 0;
	}

	rte_flow_error_set(error,
			EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			patterns,
			"Missed some patterns");
	return -rte_errno;
}

/*
 * @ RTL definition:
 * typedef struct packed {
 * logic [31:0]    ip_sa;
 * logic [15:0]    udp_sport;
 * logic [23:0]    vsid;
 * } Hash_Key_Ip_Nvgre_t;
 *
 * @ flow items:
 * RTE_FLOW_ITEM_TYPE_IPV4
 * RTE_FLOW_ITEM_TYPE_UDP
 * RTE_FLOW_ITEM_TYPE_NVGRE
 */
static int
ipn3ke_pattern_ip_nvgre(const struct rte_flow_item patterns[],
	struct rte_flow_error *error, struct ipn3ke_flow_parse *parser)
{
	const struct rte_flow_item_nvgre *nvgre = NULL;
	const struct rte_flow_item_ipv4 *ipv4 = NULL;
	const struct rte_flow_item_udp *udp = NULL;
	const struct rte_flow_item *item;

	for (item = patterns; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		if (!item->spec || item->mask || item->last) {
			rte_flow_error_set(error,
					EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM,
					item,
					"Only support item with 'spec'");
			return -rte_errno;
		}

		switch (item->type) {
		case RTE_FLOW_ITEM_TYPE_IPV4:
			ipv4 = item->spec;

			rte_memcpy(&parser->key[0], &ipv4->hdr.src_addr, 4);
			break;

		case RTE_FLOW_ITEM_TYPE_UDP:
			udp = item->spec;

			rte_memcpy(&parser->key[4], &udp->hdr.src_port, 2);
			break;

		case RTE_FLOW_ITEM_TYPE_NVGRE:
			nvgre = item->spec;

			rte_memcpy(&parser->key[6], nvgre->tni, 3);
			break;

		default:
			rte_flow_error_set(error,
					EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM,
					item,
					"Not support item type");
			return -rte_errno;
		}
	}

	if (ipv4 != NULL && udp != NULL && nvgre != NULL) {
		parser->key_len = 32 + 16 + 24;
		return 0;
	}

	rte_flow_error_set(error,
			EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			patterns,
			"Missed some patterns");
	return -rte_errno;
}

/*
 * @ RTL definition:
 * typedef struct packed{
 * logic [23:0]    vxlan_vni;
 * logic [31:0]    ip_sa;
 * logic [15:0]    udp_sport;
 * } Hash_Key_Vxlan_Ip_Udp_t;
 *
 * @ flow items:
 * RTE_FLOW_ITEM_TYPE_VXLAN
 * RTE_FLOW_ITEM_TYPE_IPV4
 * RTE_FLOW_ITEM_TYPE_UDP
 */
static int
ipn3ke_pattern_vxlan_ip_udp(const struct rte_flow_item patterns[],
	struct rte_flow_error *error, struct ipn3ke_flow_parse *parser)
{
	const struct rte_flow_item_vxlan *vxlan = NULL;
	const struct rte_flow_item_ipv4 *ipv4 = NULL;
	const struct rte_flow_item_udp *udp = NULL;
	const struct rte_flow_item *item;

	for (item = patterns; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		if (!item->spec || item->mask || item->last) {
			rte_flow_error_set(error,
					EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM,
					item,
					"Only support item with 'spec'");
			return -rte_errno;
		}

		switch (item->type) {
		case RTE_FLOW_ITEM_TYPE_VXLAN:
			vxlan = item->spec;

			rte_memcpy(&parser->key[0], vxlan->vni, 3);
			break;

		case RTE_FLOW_ITEM_TYPE_IPV4:
			ipv4 = item->spec;

			rte_memcpy(&parser->key[3], &ipv4->hdr.src_addr, 4);
			break;

		case RTE_FLOW_ITEM_TYPE_UDP:
			udp = item->spec;

			rte_memcpy(&parser->key[7], &udp->hdr.src_port, 2);
			break;

		default:
			rte_flow_error_set(error,
					EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM,
					item,
					"Not support item type");
			return -rte_errno;
		}
	}

	if (vxlan != NULL && ipv4 != NULL && udp != NULL) {
		parser->key_len = 24 + 32 + 16;
		return 0;
	}

	rte_flow_error_set(error,
			EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			patterns,
			"Missed some patterns");
	return -rte_errno;
}

static const struct ipn3ke_flow_pattern ipn3ke_supported_patterns[] = {
	[IPN3KE_HASH_KEY_VXLAN] = {
		.items = FLOW_PATTERNS(RTE_FLOW_ITEM_TYPE_VXLAN,
					RTE_FLOW_ITEM_TYPE_ETH),
		.filter = ipn3ke_pattern_vxlan,
	},

	[IPN3KE_HASH_KEY_MAC] = {
		.items = FLOW_PATTERNS(RTE_FLOW_ITEM_TYPE_ETH),
		.filter = ipn3ke_pattern_mac,
	},

	[IPN3KE_HASH_KEY_QINQ] = {
		.items = FLOW_PATTERNS(RTE_FLOW_ITEM_TYPE_VLAN,
					RTE_FLOW_ITEM_TYPE_VLAN),
		.filter = ipn3ke_pattern_qinq,
	},

	[IPN3KE_HASH_KEY_MPLS] = {
		.items = FLOW_PATTERNS(RTE_FLOW_ITEM_TYPE_MPLS,
					RTE_FLOW_ITEM_TYPE_MPLS),
		.filter = ipn3ke_pattern_mpls,
	},

	[IPN3KE_HASH_KEY_IP_TCP] = {
		.items = FLOW_PATTERNS(RTE_FLOW_ITEM_TYPE_IPV4,
					RTE_FLOW_ITEM_TYPE_TCP),
		.filter = ipn3ke_pattern_ip_tcp,
	},

	[IPN3KE_HASH_KEY_IP_UDP] = {
		.items = FLOW_PATTERNS(RTE_FLOW_ITEM_TYPE_IPV4,
					RTE_FLOW_ITEM_TYPE_UDP),
		.filter = ipn3ke_pattern_ip_udp,
	},

	[IPN3KE_HASH_KEY_IP_NVGRE] = {
		.items = FLOW_PATTERNS(RTE_FLOW_ITEM_TYPE_IPV4,
					RTE_FLOW_ITEM_TYPE_UDP,
					RTE_FLOW_ITEM_TYPE_NVGRE),
		.filter = ipn3ke_pattern_ip_nvgre,
	},

	[IPN3KE_HASH_KEY_VXLAN_IP_UDP] = {
		.items = FLOW_PATTERNS(RTE_FLOW_ITEM_TYPE_VXLAN,
					RTE_FLOW_ITEM_TYPE_IPV4,
					RTE_FLOW_ITEM_TYPE_UDP),
		.filter = ipn3ke_pattern_vxlan_ip_udp,
	},
};

static int
ipn3ke_flow_convert_attributes(const struct rte_flow_attr *attr,
				struct rte_flow_error *error)
{
	if (!attr) {
		rte_flow_error_set(error,
				EINVAL,
				RTE_FLOW_ERROR_TYPE_ATTR,
				NULL,
				"NULL attribute.");
		return -rte_errno;
	}

	if (attr->group) {
		rte_flow_error_set(error,
				ENOTSUP,
				RTE_FLOW_ERROR_TYPE_ATTR_GROUP,
				NULL,
				"groups are not supported");
		return -rte_errno;
	}

	if (attr->egress) {
		rte_flow_error_set(error,
				ENOTSUP,
				RTE_FLOW_ERROR_TYPE_ATTR_EGRESS,
				NULL,
				"egress is not supported");
		return -rte_errno;
	}

	if (attr->transfer) {
		rte_flow_error_set(error,
				ENOTSUP,
				RTE_FLOW_ERROR_TYPE_ATTR_TRANSFER,
				NULL,
				"transfer is not supported");
		return -rte_errno;
	}

	if (!attr->ingress) {
		rte_flow_error_set(error,
				ENOTSUP,
				RTE_FLOW_ERROR_TYPE_ATTR_INGRESS,
				NULL,
				"only ingress is supported");
		return -rte_errno;
	}

	return 0;
}

static int
ipn3ke_flow_convert_actions(const struct rte_flow_action actions[],
	struct rte_flow_error *error, struct ipn3ke_flow_parse *parser)
{
	const struct rte_flow_action_mark *mark = NULL;

	if (!actions) {
		rte_flow_error_set(error,
				EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION_NUM,
				NULL,
				"NULL action.");
		return -rte_errno;
	}

	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; ++actions) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;

		case RTE_FLOW_ACTION_TYPE_MARK:
			if (mark) {
				rte_flow_error_set(error,
						ENOTSUP,
						RTE_FLOW_ERROR_TYPE_ACTION,
						actions,
						"duplicated mark");
				return -rte_errno;
			}

			mark = actions->conf;
			if (!mark) {
				rte_flow_error_set(error,
						EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION,
						actions,
						"mark must be defined");
				return -rte_errno;
			} else if (mark->id > IPN3KE_FLOW_RESULT_UID_MAX) {
				rte_flow_error_set(error,
						ENOTSUP,
						RTE_FLOW_ERROR_TYPE_ACTION,
						actions,
						"mark id is out of range");
				return -rte_errno;
			}

			parser->mark = 1;
			parser->mark_id = mark->id;
			break;

		case RTE_FLOW_ACTION_TYPE_DROP:
			parser->drop = 1;
			break;

		default:
			rte_flow_error_set(error,
					ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION,
					actions,
					"invalid action");
			return -rte_errno;
		}
	}

	if (!parser->drop && !parser->mark) {
		rte_flow_error_set(error,
				EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION,
				actions,
				"no valid actions");
		return -rte_errno;
	}

	return 0;
}

static bool
ipn3ke_match_pattern(const enum rte_flow_item_type *patterns,
				const struct rte_flow_item *input)
{
	const struct rte_flow_item *item = input;

	while ((*patterns == item->type) &&
		(*patterns != RTE_FLOW_ITEM_TYPE_END)) {
		patterns++;
		item++;
	}

	return (*patterns == RTE_FLOW_ITEM_TYPE_END &&
		item->type == RTE_FLOW_ITEM_TYPE_END);
}

static pattern_filter_t
ipn3ke_find_filter_func(const struct rte_flow_item *input,
				uint32_t *idx)
{
	pattern_filter_t filter = NULL;
	uint32_t i;

	for (i = 0; i < RTE_DIM(ipn3ke_supported_patterns); i++) {
		if (ipn3ke_match_pattern(ipn3ke_supported_patterns[i].items,
					input)) {
			filter = ipn3ke_supported_patterns[i].filter;
			*idx = i;
			break;
		}
	}

	return filter;
}

static int
ipn3ke_flow_convert_items(const struct rte_flow_item items[],
	struct rte_flow_error *error, struct ipn3ke_flow_parse *parser)
{
	pattern_filter_t filter = NULL;
	uint32_t idx;

	if (!items) {
		rte_flow_error_set(error,
				EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM_NUM,
				NULL,
				"NULL pattern.");
		return -rte_errno;
	}

	filter = ipn3ke_find_filter_func(items, &idx);

	if (!filter) {
		rte_flow_error_set(error,
				EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				items,
				"Unsupported pattern");
		return -rte_errno;
	}

	parser->key_type = idx;

	return filter(items, error, parser);
}

/* Put the least @nbits of @data into @offset of @dst bits stream, and
 * the @offset starts from MSB to LSB in each byte.
 *
 * MSB    LSB
 *  +------+------+------+------+
 *  |      |      |      |      |
 *  +------+------+------+------+
 *       ^                 ^
 *       |<- data: nbits ->|
 *       |
 *     offset
 */
static void
copy_data_bits(uint8_t *dst, uint64_t data,
		uint32_t offset, uint8_t nbits)
{
	uint8_t set, *p = &dst[offset / BITS_PER_BYTE];
	uint8_t bits_to_set = BITS_PER_BYTE - (offset % BITS_PER_BYTE);
	uint8_t mask_to_set = 0xff >> (offset % BITS_PER_BYTE);
	uint32_t size = offset + nbits;

	if (nbits > (sizeof(data) * BITS_PER_BYTE)) {
		IPN3KE_AFU_PMD_ERR("nbits is out of range");
		return;
	}

	while (nbits - bits_to_set >= 0) {
		set = data >> (nbits - bits_to_set);

		*p &= ~mask_to_set;
		*p |= (set & mask_to_set);

		nbits -= bits_to_set;
		bits_to_set = BITS_PER_BYTE;
		mask_to_set = 0xff;
		p++;
	}

	if (nbits) {
		uint8_t shift = BITS_PER_BYTE - (size % BITS_PER_BYTE);

		set = data << shift;
		mask_to_set = 0xff << shift;

		*p &= ~mask_to_set;
		*p |= (set & mask_to_set);
	}
}

static void
ipn3ke_flow_key_generation(struct ipn3ke_flow_parse *parser,
				struct rte_flow *flow)
{
	uint32_t i, shift_bytes, len_in_bytes, offset;
	uint64_t key;
	uint8_t *dst;

	dst = flow->rule.key;

	copy_data_bits(dst,
			parser->key_type,
			IPN3KE_FLOW_KEY_ID_OFFSET,
			IPN3KE_FLOW_KEY_ID_BITS);

	/* The MSb of key is filled to 0 when it is less than
	 * IPN3KE_FLOW_KEY_DATA_BITS bit. And the parsed key data is
	 * save as MSB byte first in the array, it needs to move
	 * the bits before formatting them.
	 */
	key = 0;
	shift_bytes = 0;
	len_in_bytes = BITS_TO_BYTES(parser->key_len);
	offset = (IPN3KE_FLOW_KEY_DATA_OFFSET +
		IPN3KE_FLOW_KEY_DATA_BITS -
		parser->key_len);

	for (i = 0; i < len_in_bytes; i++) {
		key = (key << 8) | parser->key[i];

		if (++shift_bytes == sizeof(key)) {
			shift_bytes = 0;

			copy_data_bits(dst, key, offset,
					sizeof(key) * BITS_PER_BYTE);
			offset += sizeof(key) * BITS_PER_BYTE;
			key = 0;
		}
	}

	if (shift_bytes != 0) {
		uint32_t rem_bits;

		rem_bits = parser->key_len % (sizeof(key) * BITS_PER_BYTE);
		key >>= (shift_bytes * 8 - rem_bits);
		copy_data_bits(dst, key, offset, rem_bits);
	}
}

static void
ipn3ke_flow_result_generation(struct ipn3ke_flow_parse *parser,
				struct rte_flow *flow)
{
	uint8_t *dst;

	if (parser->drop)
		return;

	dst = flow->rule.result;

	copy_data_bits(dst,
			1,
			IPN3KE_FLOW_RESULT_ACL_OFFSET,
			IPN3KE_FLOW_RESULT_ACL_BITS);

	copy_data_bits(dst,
			parser->mark_id,
			IPN3KE_FLOW_RESULT_UID_OFFSET,
			IPN3KE_FLOW_RESULT_UID_BITS);
}

#define MHL_COMMAND_TIME_COUNT        0xFFFF
#define MHL_COMMAND_TIME_INTERVAL_US  10

static int
ipn3ke_flow_hw_update(struct ipn3ke_hw *hw,
			struct rte_flow *flow, uint32_t is_add)
{
	uint32_t *pdata = NULL;
	uint32_t data;
	uint32_t time_out = MHL_COMMAND_TIME_COUNT;
	uint32_t i;

	IPN3KE_AFU_PMD_DEBUG("IPN3KE flow dump start\n");

	pdata = (uint32_t *)flow->rule.key;
	IPN3KE_AFU_PMD_DEBUG(" - key   :");

	for (i = 0; i < RTE_DIM(flow->rule.key); i++)
		IPN3KE_AFU_PMD_DEBUG(" %02x", flow->rule.key[i]);

	for (i = 0; i < 4; i++)
		IPN3KE_AFU_PMD_DEBUG(" %02x", ipn3ke_swap32(pdata[3 - i]));
	IPN3KE_AFU_PMD_DEBUG("\n");

	pdata = (uint32_t *)flow->rule.result;
	IPN3KE_AFU_PMD_DEBUG(" - result:");

	for (i = 0; i < RTE_DIM(flow->rule.result); i++)
		IPN3KE_AFU_PMD_DEBUG(" %02x", flow->rule.result[i]);

	for (i = 0; i < 1; i++)
		IPN3KE_AFU_PMD_DEBUG(" %02x", pdata[i]);
	IPN3KE_AFU_PMD_DEBUG("IPN3KE flow dump end\n");

	pdata = (uint32_t *)flow->rule.key;

	IPN3KE_MASK_WRITE_REG(hw,
			IPN3KE_CLF_MHL_KEY_0,
			0,
			ipn3ke_swap32(pdata[3]),
			IPN3KE_CLF_MHL_KEY_MASK);

	IPN3KE_MASK_WRITE_REG(hw,
			IPN3KE_CLF_MHL_KEY_1,
			0,
			ipn3ke_swap32(pdata[2]),
			IPN3KE_CLF_MHL_KEY_MASK);

	IPN3KE_MASK_WRITE_REG(hw,
			IPN3KE_CLF_MHL_KEY_2,
			0,
			ipn3ke_swap32(pdata[1]),
			IPN3KE_CLF_MHL_KEY_MASK);

	IPN3KE_MASK_WRITE_REG(hw,
			IPN3KE_CLF_MHL_KEY_3,
			0,
			ipn3ke_swap32(pdata[0]),
			IPN3KE_CLF_MHL_KEY_MASK);

	pdata = (uint32_t *)flow->rule.result;
	IPN3KE_MASK_WRITE_REG(hw,
			IPN3KE_CLF_MHL_RES,
			0,
			ipn3ke_swap32(pdata[0]),
			IPN3KE_CLF_MHL_RES_MASK);

	/* insert/delete the key and result */
	data = 0;
	data = IPN3KE_MASK_READ_REG(hw,
				IPN3KE_CLF_MHL_MGMT_CTRL,
				0,
				0x80000000);
	time_out = MHL_COMMAND_TIME_COUNT;
	while (IPN3KE_BIT_ISSET(data, IPN3KE_CLF_MHL_MGMT_CTRL_BIT_BUSY) &&
		(time_out > 0)) {
		data = IPN3KE_MASK_READ_REG(hw,
					IPN3KE_CLF_MHL_MGMT_CTRL,
					0,
					0x80000000);
		time_out--;
		rte_delay_us(MHL_COMMAND_TIME_INTERVAL_US);
	}
	if (!time_out)
		return -1;
	if (is_add)
		IPN3KE_MASK_WRITE_REG(hw,
				IPN3KE_CLF_MHL_MGMT_CTRL,
				0,
				IPN3KE_CLF_MHL_MGMT_CTRL_INSERT,
				0x3);
	else
		IPN3KE_MASK_WRITE_REG(hw,
				IPN3KE_CLF_MHL_MGMT_CTRL,
				0,
				IPN3KE_CLF_MHL_MGMT_CTRL_DELETE,
				0x3);

	return 0;
}

static int
ipn3ke_flow_hw_flush(struct ipn3ke_hw *hw)
{
	uint32_t data;
	uint32_t time_out = MHL_COMMAND_TIME_COUNT;

	/* flush the MHL lookup table */
	data = 0;
	data = IPN3KE_MASK_READ_REG(hw,
				IPN3KE_CLF_MHL_MGMT_CTRL,
				0,
				0x80000000);
	time_out = MHL_COMMAND_TIME_COUNT;
	while (IPN3KE_BIT_ISSET(data, IPN3KE_CLF_MHL_MGMT_CTRL_BIT_BUSY) &&
		(time_out > 0)) {
		data = IPN3KE_MASK_READ_REG(hw,
					IPN3KE_CLF_MHL_MGMT_CTRL,
					0,
					0x80000000);
		time_out--;
		rte_delay_us(MHL_COMMAND_TIME_INTERVAL_US);
	}
	if (!time_out)
		return -1;
	IPN3KE_MASK_WRITE_REG(hw,
			IPN3KE_CLF_MHL_MGMT_CTRL,
			0,
			IPN3KE_CLF_MHL_MGMT_CTRL_FLUSH,
			0x3);

	return 0;
}

static void
ipn3ke_flow_convert_finalise(struct ipn3ke_hw *hw,
	struct ipn3ke_flow_parse *parser, struct rte_flow *flow)
{
	ipn3ke_flow_key_generation(parser, flow);
	ipn3ke_flow_result_generation(parser, flow);
	ipn3ke_flow_hw_update(hw, flow, 1);
}

static int
ipn3ke_flow_convert(const struct rte_flow_attr *attr,
	const struct rte_flow_item items[],
	const struct rte_flow_action actions[], struct rte_flow_error *error,
	struct ipn3ke_flow_parse *parser)
{
	int ret;

	ret = ipn3ke_flow_convert_attributes(attr, error);
	if (ret)
		return ret;

	ret = ipn3ke_flow_convert_actions(actions, error, parser);
	if (ret)
		return ret;

	ret = ipn3ke_flow_convert_items(items, error, parser);
	if (ret)
		return ret;

	return 0;
}

static int
ipn3ke_flow_validate(__rte_unused struct rte_eth_dev *dev,
	const struct rte_flow_attr *attr, const struct rte_flow_item pattern[],
	const struct rte_flow_action actions[], struct rte_flow_error *error)
{
	struct ipn3ke_flow_parse parser = {0};
	return ipn3ke_flow_convert(attr, pattern, actions, error, &parser);
}

static struct rte_flow *
ipn3ke_flow_create(struct rte_eth_dev *dev,
	const struct rte_flow_attr *attr, const struct rte_flow_item pattern[],
	const struct rte_flow_action actions[], struct rte_flow_error *error)
{
	struct ipn3ke_hw *hw = IPN3KE_DEV_PRIVATE_TO_HW(dev);
	struct ipn3ke_flow_parse parser = {0};
	struct rte_flow *flow;
	int ret;

	if (hw->flow_num_entries == hw->flow_max_entries) {
		rte_flow_error_set(error,
				ENOBUFS,
				RTE_FLOW_ERROR_TYPE_HANDLE,
				NULL,
				"The flow table is full.");
		return NULL;
	}

	ret = ipn3ke_flow_convert(attr, pattern, actions, error, &parser);
	if (ret < 0) {
		rte_flow_error_set(error,
				-ret,
				RTE_FLOW_ERROR_TYPE_HANDLE,
				NULL,
				"Failed to create flow.");
		return NULL;
	}

	flow = rte_zmalloc("ipn3ke_flow", sizeof(struct rte_flow), 0);
	if (!flow) {
		rte_flow_error_set(error,
				ENOMEM,
				RTE_FLOW_ERROR_TYPE_HANDLE,
				NULL,
				"Failed to allocate memory");
		return flow;
	}

	ipn3ke_flow_convert_finalise(hw, &parser, flow);

	TAILQ_INSERT_TAIL(&hw->flow_list, flow, next);

	return flow;
}

static int
ipn3ke_flow_destroy(struct rte_eth_dev *dev,
	struct rte_flow *flow, struct rte_flow_error *error)
{
	struct ipn3ke_hw *hw = IPN3KE_DEV_PRIVATE_TO_HW(dev);
	int ret = 0;

	ret = ipn3ke_flow_hw_update(hw, flow, 0);
	if (!ret) {
		TAILQ_REMOVE(&hw->flow_list, flow, next);
		rte_free(flow);
	} else {
		rte_flow_error_set(error,
				-ret,
				RTE_FLOW_ERROR_TYPE_HANDLE,
				NULL,
				"Failed to destroy flow.");
	}

	return ret;
}

static int
ipn3ke_flow_flush(struct rte_eth_dev *dev,
		__rte_unused struct rte_flow_error *error)
{
	struct ipn3ke_hw *hw = IPN3KE_DEV_PRIVATE_TO_HW(dev);
	struct rte_flow *flow, *temp;

	RTE_TAILQ_FOREACH_SAFE(flow, &hw->flow_list, next, temp) {
		TAILQ_REMOVE(&hw->flow_list, flow, next);
		rte_free(flow);
	}

	return ipn3ke_flow_hw_flush(hw);
}

int ipn3ke_flow_init(void *dev)
{
	struct ipn3ke_hw *hw = (struct ipn3ke_hw *)dev;
	uint32_t data;

	/* disable rx classifier bypass */
	IPN3KE_MASK_WRITE_REG(hw,
			IPN3KE_CLF_RX_TEST,
			0, 0, 0x1);

	data = 0;
	data = IPN3KE_MASK_READ_REG(hw,
				IPN3KE_CLF_RX_TEST,
				0,
				0x1);
	IPN3KE_AFU_PMD_DEBUG("IPN3KE_CLF_RX_TEST: %x\n", data);

	/* configure base mac address */
	IPN3KE_MASK_WRITE_REG(hw,
			IPN3KE_CLF_BASE_DST_MAC_ADDR_HI,
			0,
			0x2457,
			0xFFFF);

	data = 0;
	data = IPN3KE_MASK_READ_REG(hw,
				IPN3KE_CLF_BASE_DST_MAC_ADDR_HI,
				0,
				0xFFFF);
	IPN3KE_AFU_PMD_DEBUG("IPN3KE_CLF_BASE_DST_MAC_ADDR_HI: %x\n", data);

	IPN3KE_MASK_WRITE_REG(hw,
			IPN3KE_CLF_BASE_DST_MAC_ADDR_LOW,
			0,
			0x9bdf1000,
			0xFFFFFFFF);

	data = 0;
	data = IPN3KE_MASK_READ_REG(hw,
				IPN3KE_CLF_BASE_DST_MAC_ADDR_LOW,
				0,
				0xFFFFFFFF);
	IPN3KE_AFU_PMD_DEBUG("IPN3KE_CLF_BASE_DST_MAC_ADDR_LOW: %x\n", data);


	/* configure hash lookup rules enable */
	IPN3KE_MASK_WRITE_REG(hw,
			IPN3KE_CLF_LKUP_ENABLE,
			0,
			0xFD,
			0xFF);

	data = 0;
	data = IPN3KE_MASK_READ_REG(hw,
				IPN3KE_CLF_LKUP_ENABLE,
				0,
				0xFF);
	IPN3KE_AFU_PMD_DEBUG("IPN3KE_CLF_LKUP_ENABLE: %x\n", data);


	/* configure rx parse config, settings associatied with VxLAN */
	IPN3KE_MASK_WRITE_REG(hw,
			IPN3KE_CLF_RX_PARSE_CFG,
			0,
			0x212b5,
			0x3FFFF);

	data = 0;
	data = IPN3KE_MASK_READ_REG(hw,
				IPN3KE_CLF_RX_PARSE_CFG,
				0,
				0x3FFFF);
	IPN3KE_AFU_PMD_DEBUG("IPN3KE_CLF_RX_PARSE_CFG: %x\n", data);


	/* configure QinQ S-Tag */
	IPN3KE_MASK_WRITE_REG(hw,
			IPN3KE_CLF_QINQ_STAG,
			0,
			0x88a8,
			0xFFFF);

	data = 0;
	data = IPN3KE_MASK_READ_REG(hw,
				IPN3KE_CLF_QINQ_STAG,
				0,
				0xFFFF);
	IPN3KE_AFU_PMD_DEBUG("IPN3KE_CLF_QINQ_STAG: %x\n", data);


	/* configure gen ctrl */
	IPN3KE_MASK_WRITE_REG(hw,
			IPN3KE_CLF_MHL_GEN_CTRL,
			0,
			0x3,
			0x3);

	data = 0;
	data = IPN3KE_MASK_READ_REG(hw,
				IPN3KE_CLF_MHL_GEN_CTRL,
				0,
				0x1F);
	IPN3KE_AFU_PMD_DEBUG("IPN3KE_CLF_MHL_GEN_CTRL: %x\n", data);


	/* clear monitoring register */
	IPN3KE_MASK_WRITE_REG(hw,
			IPN3KE_CLF_MHL_MON_0,
			0,
			0xFFFFFFFF,
			0xFFFFFFFF);

	data = 0;
	data = IPN3KE_MASK_READ_REG(hw,
				IPN3KE_CLF_MHL_MON_0,
				0,
				0xFFFFFFFF);
	IPN3KE_AFU_PMD_DEBUG("IPN3KE_CLF_MHL_MON_0: %x\n", data);


	ipn3ke_flow_hw_flush(hw);

	TAILQ_INIT(&hw->flow_list);
	hw->flow_max_entries = IPN3KE_MASK_READ_REG(hw,
						IPN3KE_CLF_EM_NUM,
						0,
						0xFFFFFFFF);
	IPN3KE_AFU_PMD_DEBUG("IPN3KE_CLF_EN_NUM: %x\n", hw->flow_max_entries);
	hw->flow_num_entries = 0;

	return 0;
}

const struct rte_flow_ops ipn3ke_flow_ops = {
	.validate = ipn3ke_flow_validate,
	.create = ipn3ke_flow_create,
	.destroy = ipn3ke_flow_destroy,
	.flush = ipn3ke_flow_flush,
};
