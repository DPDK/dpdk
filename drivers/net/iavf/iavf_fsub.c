/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#include <sys/queue.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_eth_ctrl.h>
#include <rte_tailq.h>
#include <rte_flow_driver.h>
#include <rte_flow.h>
#include <iavf.h>
#include "iavf_generic_flow.h"

#define MAX_QGRP_NUM_TYPE      7
#define IAVF_IPV6_ADDR_LENGTH  16
#define MAX_INPUT_SET_BYTE     32

#define IAVF_SW_INSET_ETHER ( \
	IAVF_INSET_DMAC | IAVF_INSET_SMAC | IAVF_INSET_ETHERTYPE)
#define IAVF_SW_INSET_MAC_IPV4 ( \
	IAVF_INSET_DMAC | IAVF_INSET_IPV4_DST | IAVF_INSET_IPV4_SRC | \
	IAVF_INSET_IPV4_PROTO | IAVF_INSET_IPV4_TTL | IAVF_INSET_IPV4_TOS)
#define IAVF_SW_INSET_MAC_VLAN_IPV4 ( \
	IAVF_SW_INSET_MAC_IPV4 | IAVF_INSET_VLAN_OUTER)
#define IAVF_SW_INSET_MAC_IPV4_TCP ( \
	IAVF_INSET_DMAC | IAVF_INSET_IPV4_DST | IAVF_INSET_IPV4_SRC | \
	IAVF_INSET_IPV4_TTL | IAVF_INSET_IPV4_TOS | \
	IAVF_INSET_TCP_DST_PORT | IAVF_INSET_TCP_SRC_PORT)
#define IAVF_SW_INSET_MAC_IPV4_UDP ( \
	IAVF_INSET_DMAC | IAVF_INSET_IPV4_DST | IAVF_INSET_IPV4_SRC | \
	IAVF_INSET_IPV4_TTL | IAVF_INSET_IPV4_TOS | \
	IAVF_INSET_UDP_DST_PORT | IAVF_INSET_UDP_SRC_PORT)
#define IAVF_SW_INSET_MAC_IPV6 ( \
	IAVF_INSET_DMAC | IAVF_INSET_IPV6_DST | IAVF_INSET_IPV6_SRC | \
	IAVF_INSET_IPV6_TC | IAVF_INSET_IPV6_HOP_LIMIT | \
	IAVF_INSET_IPV6_NEXT_HDR)
#define IAVF_SW_INSET_MAC_IPV6_TCP ( \
	IAVF_INSET_DMAC | IAVF_INSET_IPV6_DST | IAVF_INSET_IPV6_SRC | \
	IAVF_INSET_IPV6_HOP_LIMIT | IAVF_INSET_IPV6_TC | \
	IAVF_INSET_TCP_DST_PORT | IAVF_INSET_TCP_SRC_PORT)
#define IAVF_SW_INSET_MAC_IPV6_UDP ( \
	IAVF_INSET_DMAC | IAVF_INSET_IPV6_DST | IAVF_INSET_IPV6_SRC | \
	IAVF_INSET_IPV6_HOP_LIMIT | IAVF_INSET_IPV6_TC | \
	IAVF_INSET_UDP_DST_PORT | IAVF_INSET_UDP_SRC_PORT)

static struct iavf_flow_parser iavf_fsub_parser;

static struct
iavf_pattern_match_item iavf_fsub_pattern_list[] = {
	{iavf_pattern_raw,				IAVF_INSET_NONE,			IAVF_INSET_NONE},
	{iavf_pattern_ethertype,			IAVF_SW_INSET_ETHER,			IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4,				IAVF_SW_INSET_MAC_IPV4,			IAVF_INSET_NONE},
	{iavf_pattern_eth_vlan_ipv4,			IAVF_SW_INSET_MAC_VLAN_IPV4,		IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_udp,			IAVF_SW_INSET_MAC_IPV4_UDP,		IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_tcp,			IAVF_SW_INSET_MAC_IPV4_TCP,		IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6,				IAVF_SW_INSET_MAC_IPV6,			IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_udp,			IAVF_SW_INSET_MAC_IPV6_UDP,		IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_tcp,			IAVF_SW_INSET_MAC_IPV6_TCP,		IAVF_INSET_NONE},
};

static int
iavf_fsub_create(struct iavf_adapter *ad, struct rte_flow *flow,
		 void *meta, struct rte_flow_error *error)
{
	struct iavf_fsub_conf *filter = meta;
	struct iavf_fsub_conf *rule;
	int ret;

	rule = rte_zmalloc("fsub_entry", sizeof(*rule), 0);
	if (!rule) {
		rte_flow_error_set(error, ENOMEM,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Failed to allocate memory for fsub rule");
		return -rte_errno;
	}

	ret = iavf_flow_sub(ad, filter);
	if (ret) {
		rte_flow_error_set(error, -ret,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to subscribe flow rule.");
		goto free_entry;
	}

	rte_memcpy(rule, filter, sizeof(*rule));
	flow->rule = rule;

	rte_free(meta);
	return ret;

free_entry:
	rte_free(rule);
	return -rte_errno;
}

static int
iavf_fsub_destroy(struct iavf_adapter *ad, struct rte_flow *flow,
		  struct rte_flow_error *error)
{
	struct iavf_fsub_conf *filter;
	int ret;

	filter = (struct iavf_fsub_conf *)flow->rule;

	ret = iavf_flow_unsub(ad, filter);
	if (ret) {
		rte_flow_error_set(error, -ret,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to unsubscribe flow rule.");
		return -rte_errno;
	}

	flow->rule = NULL;
	rte_free(filter);

	return ret;
}

static int
iavf_fsub_validation(struct iavf_adapter *ad,
		     __rte_unused struct rte_flow *flow,
		     void *meta,
		     struct rte_flow_error *error)
{
	struct iavf_fsub_conf *filter = meta;
	int ret;

	ret = iavf_flow_sub_check(ad, filter);
	if (ret) {
		rte_flow_error_set(error, -ret,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to validate filter rule.");
		return -rte_errno;
	}

	return ret;
};

static int
iavf_fsub_parse_pattern(const struct rte_flow_item pattern[],
			const uint64_t input_set_mask,
			struct rte_flow_error *error,
			struct iavf_fsub_conf *filter)
{
	struct virtchnl_proto_hdrs *hdrs = &filter->sub_fltr.proto_hdrs;
	enum rte_flow_item_type item_type;
	const struct rte_flow_item_raw *raw_spec, *raw_mask;
	const struct rte_flow_item_eth *eth_spec, *eth_mask;
	const struct rte_flow_item_ipv4 *ipv4_spec, *ipv4_mask;
	const struct rte_flow_item_ipv6 *ipv6_spec, *ipv6_mask;
	const struct rte_flow_item_tcp *tcp_spec, *tcp_mask;
	const struct rte_flow_item_udp *udp_spec, *udp_mask;
	const struct rte_flow_item_vlan *vlan_spec, *vlan_mask;
	const struct rte_flow_item *item = pattern;
	struct virtchnl_proto_hdr_w_msk *hdr, *hdr1 = NULL;
	uint64_t outer_input_set = IAVF_INSET_NONE;
	uint64_t *input = NULL;
	uint16_t input_set_byte = 0;
	uint8_t item_num = 0;
	uint32_t layer = 0;

	for (item = pattern; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		if (item->last) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   item, "Not support range");
			return -rte_errno;
		}

		item_type = item->type;
		item_num++;

		switch (item_type) {
		case RTE_FLOW_ITEM_TYPE_RAW: {
			raw_spec = item->spec;
			raw_mask = item->mask;

			if (item_num != 1)
				return -rte_errno;

			if (raw_spec->length != raw_mask->length)
				return -rte_errno;

			uint16_t pkt_len = 0;
			uint16_t tmp_val = 0;
			uint8_t tmp = 0;
			int i, j;

			pkt_len = raw_spec->length;

			for (i = 0, j = 0; i < pkt_len; i += 2, j++) {
				tmp = raw_spec->pattern[i];
				if (tmp >= 'a' && tmp <= 'f')
					tmp_val = tmp - 'a' + 10;
				if (tmp >= 'A' && tmp <= 'F')
					tmp_val = tmp - 'A' + 10;
				if (tmp >= '0' && tmp <= '9')
					tmp_val = tmp - '0';

				tmp_val *= 16;
				tmp = raw_spec->pattern[i + 1];
				if (tmp >= 'a' && tmp <= 'f')
					tmp_val += (tmp - 'a' + 10);
				if (tmp >= 'A' && tmp <= 'F')
					tmp_val += (tmp - 'A' + 10);
				if (tmp >= '0' && tmp <= '9')
					tmp_val += (tmp - '0');

				hdrs->raw.spec[j] = tmp_val;

				tmp = raw_mask->pattern[i];
				if (tmp >= 'a' && tmp <= 'f')
					tmp_val = tmp - 'a' + 10;
				if (tmp >= 'A' && tmp <= 'F')
					tmp_val = tmp - 'A' + 10;
				if (tmp >= '0' && tmp <= '9')
					tmp_val = tmp - '0';

				tmp_val *= 16;
				tmp = raw_mask->pattern[i + 1];
				if (tmp >= 'a' && tmp <= 'f')
					tmp_val += (tmp - 'a' + 10);
				if (tmp >= 'A' && tmp <= 'F')
					tmp_val += (tmp - 'A' + 10);
				if (tmp >= '0' && tmp <= '9')
					tmp_val += (tmp - '0');

				hdrs->raw.mask[j] = tmp_val;
			}

			hdrs->raw.pkt_len = pkt_len / 2;
			hdrs->tunnel_level = 0;
			hdrs->count = 0;
			return 0;
		}
		case RTE_FLOW_ITEM_TYPE_ETH:
			eth_spec = item->spec;
			eth_mask = item->mask;

			hdr1 = &hdrs->proto_hdr_w_msk[layer];

			VIRTCHNL_SET_PROTO_HDR_TYPE(hdr1, ETH);

			if (eth_spec && eth_mask) {
				input = &outer_input_set;

				if (!rte_is_zero_ether_addr(&eth_mask->hdr.dst_addr)) {
					*input |= IAVF_INSET_DMAC;
					input_set_byte += 6;
				} else {
					/* flow subscribe filter will add dst mac in kernel */
					input_set_byte += 6;
				}

				if (!rte_is_zero_ether_addr(&eth_mask->hdr.src_addr)) {
					*input |= IAVF_INSET_SMAC;
					input_set_byte += 6;
				}

				if (eth_mask->hdr.ether_type) {
					*input |= IAVF_INSET_ETHERTYPE;
					input_set_byte += 2;
				}

				rte_memcpy(hdr1->buffer_spec, eth_spec,
					   sizeof(struct rte_ether_hdr));
				rte_memcpy(hdr1->buffer_mask, eth_mask,
					   sizeof(struct rte_ether_hdr));
			} else {
				/* flow subscribe filter will add dst mac in kernel */
				input_set_byte += 6;
			}

			hdrs->count = ++layer;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			ipv4_spec = item->spec;
			ipv4_mask = item->mask;

			hdr = &hdrs->proto_hdr_w_msk[layer];

			VIRTCHNL_SET_PROTO_HDR_TYPE(hdr, IPV4);

			if (ipv4_spec && ipv4_mask) {
				input = &outer_input_set;
				/* Check IPv4 mask and update input set */
				if (ipv4_mask->hdr.version_ihl ||
					ipv4_mask->hdr.total_length ||
					ipv4_mask->hdr.packet_id ||
					ipv4_mask->hdr.hdr_checksum) {
					rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ITEM,
						item, "Invalid IPv4 mask.");
					return -rte_errno;
				}

				if (ipv4_mask->hdr.src_addr) {
					*input |= IAVF_INSET_IPV4_SRC;
					input_set_byte += 2;
				}
				if (ipv4_mask->hdr.dst_addr) {
					*input |= IAVF_INSET_IPV4_DST;
					input_set_byte += 2;
				}
				if (ipv4_mask->hdr.time_to_live) {
					*input |= IAVF_INSET_IPV4_TTL;
					input_set_byte++;
				}
				if (ipv4_mask->hdr.next_proto_id) {
					*input |= IAVF_INSET_IPV4_PROTO;
					input_set_byte++;
				}
				if (ipv4_mask->hdr.type_of_service) {
					*input |= IAVF_INSET_IPV4_TOS;
					input_set_byte++;
				}

				rte_memcpy(hdr->buffer_spec, &ipv4_spec->hdr,
					   sizeof(ipv4_spec->hdr));
				rte_memcpy(hdr->buffer_mask, &ipv4_mask->hdr,
					   sizeof(ipv4_spec->hdr));
			}

			hdrs->count = ++layer;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6: {
			int j;

			ipv6_spec = item->spec;
			ipv6_mask = item->mask;

			hdr = &hdrs->proto_hdr_w_msk[layer];

			VIRTCHNL_SET_PROTO_HDR_TYPE(hdr, IPV6);

			if (ipv6_spec && ipv6_mask) {
				input = &outer_input_set;

				if (ipv6_mask->hdr.payload_len) {
					rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ITEM,
						item, "Invalid IPv6 mask");
					return -rte_errno;
				}

				for (j = 0; j < IAVF_IPV6_ADDR_LENGTH; j++) {
					if (ipv6_mask->hdr.src_addr[j]) {
						*input |= IAVF_INSET_IPV6_SRC;
						break;
					}
				}
				for (j = 0; j < IAVF_IPV6_ADDR_LENGTH; j++) {
					if (ipv6_mask->hdr.dst_addr[j]) {
						*input |= IAVF_INSET_IPV6_DST;
						break;
					}
				}

				for (j = 0; j < IAVF_IPV6_ADDR_LENGTH; j++) {
					if (ipv6_mask->hdr.src_addr[j])
						input_set_byte++;

					if (ipv6_mask->hdr.dst_addr[j])
						input_set_byte++;
				}

				if (ipv6_mask->hdr.proto) {
					*input |= IAVF_INSET_IPV6_NEXT_HDR;
					input_set_byte++;
				}
				if (ipv6_mask->hdr.hop_limits) {
					*input |= IAVF_INSET_IPV6_HOP_LIMIT;
					input_set_byte++;
				}
				if (ipv6_mask->hdr.vtc_flow &
				    rte_cpu_to_be_32(RTE_IPV6_HDR_TC_MASK)) {
					*input |= IAVF_INSET_IPV6_TC;
					input_set_byte += 4;
				}

				rte_memcpy(hdr->buffer_spec, &ipv6_spec->hdr,
					   sizeof(ipv6_spec->hdr));
				rte_memcpy(hdr->buffer_mask, &ipv6_mask->hdr,
					   sizeof(ipv6_spec->hdr));
			}

			hdrs->count = ++layer;
			break;
		}
		case RTE_FLOW_ITEM_TYPE_UDP:
			udp_spec = item->spec;
			udp_mask = item->mask;

			hdr = &hdrs->proto_hdr_w_msk[layer];

			VIRTCHNL_SET_PROTO_HDR_TYPE(hdr, UDP);

			if (udp_spec && udp_mask) {
				input = &outer_input_set;
				/* Check UDP mask and update input set*/
				if (udp_mask->hdr.dgram_len ||
				    udp_mask->hdr.dgram_cksum) {
					rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ITEM,
						item, "Invalid UDP mask");
					return -rte_errno;
				}

				if (udp_mask->hdr.src_port) {
					*input |= IAVF_INSET_UDP_SRC_PORT;
					input_set_byte += 2;
				}
				if (udp_mask->hdr.dst_port) {
					*input |= IAVF_INSET_UDP_DST_PORT;
					input_set_byte += 2;
				}

				rte_memcpy(hdr->buffer_spec, &udp_spec->hdr,
					   sizeof(udp_spec->hdr));
				rte_memcpy(hdr->buffer_mask, &udp_mask->hdr,
					   sizeof(udp_mask->hdr));
			}

			hdrs->count = ++layer;
			break;
		case RTE_FLOW_ITEM_TYPE_TCP:
			tcp_spec = item->spec;
			tcp_mask = item->mask;

			hdr = &hdrs->proto_hdr_w_msk[layer];

			VIRTCHNL_SET_PROTO_HDR_TYPE(hdr, TCP);

			if (tcp_spec && tcp_mask) {
				input = &outer_input_set;
				/* Check TCP mask and update input set */
				if (tcp_mask->hdr.sent_seq ||
					tcp_mask->hdr.recv_ack ||
					tcp_mask->hdr.data_off ||
					tcp_mask->hdr.tcp_flags ||
					tcp_mask->hdr.rx_win ||
					tcp_mask->hdr.cksum ||
					tcp_mask->hdr.tcp_urp) {
					rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ITEM,
						item, "Invalid TCP mask");
					return -rte_errno;
				}

				if (tcp_mask->hdr.src_port) {
					*input |= IAVF_INSET_TCP_SRC_PORT;
					input_set_byte += 2;
				}
				if (tcp_mask->hdr.dst_port) {
					*input |= IAVF_INSET_TCP_DST_PORT;
					input_set_byte += 2;
				}

				rte_memcpy(hdr->buffer_spec, &tcp_spec->hdr,
					   sizeof(tcp_spec->hdr));
				rte_memcpy(hdr->buffer_mask, &tcp_mask->hdr,
					   sizeof(tcp_mask->hdr));
			}

			hdrs->count = ++layer;
			break;
		case RTE_FLOW_ITEM_TYPE_VLAN:
			vlan_spec = item->spec;
			vlan_mask = item->mask;

			hdr = &hdrs->proto_hdr_w_msk[layer];

			VIRTCHNL_SET_PROTO_HDR_TYPE(hdr, S_VLAN);

			if (vlan_spec && vlan_mask) {
				input = &outer_input_set;

				*input |= IAVF_INSET_VLAN_OUTER;

				if (vlan_mask->hdr.vlan_tci)
					input_set_byte += 2;

				if (vlan_mask->hdr.eth_proto) {
					rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ITEM,
						item,
						"Invalid VLAN input set.");
					return -rte_errno;
				}

				rte_memcpy(hdr->buffer_spec, &vlan_spec->hdr,
					   sizeof(vlan_spec->hdr));
				rte_memcpy(hdr->buffer_mask, &vlan_mask->hdr,
					   sizeof(vlan_mask->hdr));
			}

			hdrs->count = ++layer;
			break;
		case RTE_FLOW_ITEM_TYPE_VOID:
			break;
		default:
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM, pattern,
					   "Invalid pattern item.");
			return -rte_errno;
		}
	}

	hdrs->count += VIRTCHNL_MAX_NUM_PROTO_HDRS;

	if (input_set_byte > MAX_INPUT_SET_BYTE) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
				   item, "too much input set");
		return -rte_errno;
	}

	if (!outer_input_set || (outer_input_set & ~input_set_mask))
		return -rte_errno;

	return 0;
}

static int
iavf_fsub_parse_action(struct iavf_adapter *ad,
		       const struct rte_flow_action *actions,
		       uint32_t priority,
		       struct rte_flow_error *error,
		       struct iavf_fsub_conf *filter)
{
	const struct rte_flow_action *action;
	const struct rte_flow_action_ethdev *act_ethdev;
	const struct rte_flow_action_queue *act_q;
	const struct rte_flow_action_rss *act_qgrop;
	struct virtchnl_filter_action *filter_action;
	uint16_t valid_qgrop_number[MAX_QGRP_NUM_TYPE] = {
		2, 4, 8, 16, 32, 64, 128};
	uint16_t i, num = 0, dest_num = 0, vf_num = 0;
	uint16_t rule_port_id;

	for (action = actions; action->type !=
				RTE_FLOW_ACTION_TYPE_END; action++) {
		switch (action->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;

		case RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR:
			vf_num++;
			filter_action = &filter->sub_fltr.actions.actions[num];

			act_ethdev = action->conf;
			rule_port_id = ad->dev_data->port_id;
			if (rule_port_id != act_ethdev->port_id)
				goto error1;

			filter->sub_fltr.actions.count = ++num;
			break;
		case RTE_FLOW_ACTION_TYPE_QUEUE:
			dest_num++;
			filter_action = &filter->sub_fltr.actions.actions[num];

			act_q = action->conf;
			if (act_q->index >= ad->dev_data->nb_rx_queues)
				goto error2;

			filter_action->type = VIRTCHNL_ACTION_QUEUE;
			filter_action->act_conf.queue.index = act_q->index;
			filter->sub_fltr.actions.count = ++num;
			break;
		case RTE_FLOW_ACTION_TYPE_RSS:
			dest_num++;
			filter_action = &filter->sub_fltr.actions.actions[num];

			act_qgrop = action->conf;
			if (act_qgrop->queue_num <= 1)
				goto error2;

			filter_action->type = VIRTCHNL_ACTION_Q_REGION;
			filter_action->act_conf.queue.index =
							act_qgrop->queue[0];
			for (i = 0; i < MAX_QGRP_NUM_TYPE; i++) {
				if (act_qgrop->queue_num ==
				    valid_qgrop_number[i])
					break;
			}

			if (i == MAX_QGRP_NUM_TYPE)
				goto error2;

			if ((act_qgrop->queue[0] + act_qgrop->queue_num) >
			    ad->dev_data->nb_rx_queues)
				goto error3;

			for (i = 0; i < act_qgrop->queue_num - 1; i++)
				if (act_qgrop->queue[i + 1] !=
				    act_qgrop->queue[i] + 1)
					goto error4;

			filter_action->act_conf.queue.region = act_qgrop->queue_num;
			filter->sub_fltr.actions.count = ++num;
			break;
		default:
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ACTION,
					   actions, "Invalid action type");
			return -rte_errno;
		}
	}

	/* 0 denotes lowest priority of recipe and highest priority
	 * of rte_flow. Change rte_flow priority into recipe priority.
	 */
	filter->sub_fltr.priority = priority;

	if (num > VIRTCHNL_MAX_NUM_ACTIONS) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION, actions,
				   "Action numbers exceed the maximum value");
		return -rte_errno;
	}

	if (vf_num == 0) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION, actions,
				   "Invalid action, vf action must be added");
		return -rte_errno;
	}

	if (dest_num >= 2) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION, actions,
				   "Unsupported action combination");
		return -rte_errno;
	}

	return 0;

error1:
	rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION, actions,
			   "Invalid port id");
	return -rte_errno;

error2:
	rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION, actions,
			   "Invalid action type or queue number");
	return -rte_errno;

error3:
	rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION, actions,
			   "Invalid queue region indexes");
	return -rte_errno;

error4:
	rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION, actions,
			   "Discontinuous queue region");
	return -rte_errno;
}

static int
iavf_fsub_check_action(const struct rte_flow_action *actions,
		       struct rte_flow_error *error)
{
	const struct rte_flow_action *action;
	enum rte_flow_action_type action_type;
	uint16_t actions_num = 0;
	bool vf_valid = false;
	bool queue_valid = false;

	for (action = actions; action->type !=
				RTE_FLOW_ACTION_TYPE_END; action++) {
		action_type = action->type;
		switch (action_type) {
		case RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR:
			vf_valid = true;
			actions_num++;
			break;
		case RTE_FLOW_ACTION_TYPE_RSS:
		case RTE_FLOW_ACTION_TYPE_QUEUE:
			queue_valid = true;
			actions_num++;
			break;
		case RTE_FLOW_ACTION_TYPE_DROP:
			actions_num++;
			break;
		case RTE_FLOW_ACTION_TYPE_VOID:
			continue;
		default:
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ACTION,
					   actions, "Invalid action type");
			return -rte_errno;
		}
	}

	if (!((actions_num == 1 && !queue_valid) ||
	      (actions_num == 2 && vf_valid && queue_valid))) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
				   actions, "Invalid action number");
		return -rte_errno;
	}

	return 0;
}

static int
iavf_fsub_parse(struct iavf_adapter *ad,
		struct iavf_pattern_match_item *array,
		uint32_t array_len,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		uint32_t priority,
		void **meta,
		struct rte_flow_error *error)
{
	struct iavf_fsub_conf *filter;
	struct iavf_pattern_match_item *pattern_match_item = NULL;
	int ret = 0;

	filter = rte_zmalloc(NULL, sizeof(*filter), 0);
	if (!filter) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "No memory for iavf_fsub_conf_ptr");
		return -ENOMEM;
	}

	/* search flow subscribe pattern */
	pattern_match_item = iavf_search_pattern_match_item(pattern, array,
							    array_len, error);
	if (!pattern_match_item) {
		ret = -rte_errno;
		goto error;
	}

	/* parse flow subscribe pattern */
	ret = iavf_fsub_parse_pattern(pattern,
				      pattern_match_item->input_set_mask,
				      error, filter);
	if (ret)
		goto error;

	/* check flow subscribe pattern action */
	ret = iavf_fsub_check_action(actions, error);
	if (ret)
		goto error;

	/* parse flow subscribe pattern action */
	ret = iavf_fsub_parse_action((void *)ad, actions, priority,
				     error, filter);

error:
	if (!ret && meta)
		*meta = filter;
	else
		rte_free(filter);

	rte_free(pattern_match_item);
	return ret;
}

static int
iavf_fsub_init(struct iavf_adapter *ad)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(ad);
	struct iavf_flow_parser *parser;

	if (!vf->vf_res)
		return -EINVAL;

	if (vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_FSUB_PF)
		parser = &iavf_fsub_parser;
	else
		return -ENOTSUP;

	return iavf_register_parser(parser, ad);
}

static void
iavf_fsub_uninit(struct iavf_adapter *ad)
{
	iavf_unregister_parser(&iavf_fsub_parser, ad);
}

static struct
iavf_flow_engine iavf_fsub_engine = {
	.init = iavf_fsub_init,
	.uninit = iavf_fsub_uninit,
	.create = iavf_fsub_create,
	.destroy = iavf_fsub_destroy,
	.validation = iavf_fsub_validation,
	.type = IAVF_FLOW_ENGINE_FSUB,
};

static struct
iavf_flow_parser iavf_fsub_parser = {
	.engine = &iavf_fsub_engine,
	.array = iavf_fsub_pattern_list,
	.array_len = RTE_DIM(iavf_fsub_pattern_list),
	.parse_pattern_action = iavf_fsub_parse,
	.stage = IAVF_FLOW_STAGE_DISTRIBUTOR,
};

RTE_INIT(iavf_fsub_engine_init)
{
	iavf_register_flow_engine(&iavf_fsub_engine);
}
