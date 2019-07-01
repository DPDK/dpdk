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

#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev_driver.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_eth_ctrl.h>
#include <rte_tailq.h>
#include <rte_flow_driver.h>

#include "ice_logs.h"
#include "base/ice_type.h"
#include "ice_switch_filter.h"

static int
ice_parse_switch_filter(const struct rte_flow_item pattern[],
			const struct rte_flow_action actions[],
			struct rte_flow_error *error,
			struct ice_adv_lkup_elem *list,
			uint16_t *lkups_num,
			enum ice_sw_tunnel_type tun_type)
{
	const struct rte_flow_item *item = pattern;
	enum rte_flow_item_type item_type;
	const struct rte_flow_item_eth *eth_spec, *eth_mask;
	const struct rte_flow_item_ipv4 *ipv4_spec, *ipv4_mask;
	const struct rte_flow_item_ipv6 *ipv6_spec, *ipv6_mask;
	const struct rte_flow_item_tcp *tcp_spec, *tcp_mask;
	const struct rte_flow_item_udp *udp_spec, *udp_mask;
	const struct rte_flow_item_sctp *sctp_spec, *sctp_mask;
	const struct rte_flow_item_nvgre  *nvgre_spec, *nvgre_mask;
	const struct rte_flow_item_vxlan  *vxlan_spec, *vxlan_mask;
	uint16_t j, t = 0;
	uint16_t tunnel_valid = 0;

	for (item = pattern; item->type !=
			RTE_FLOW_ITEM_TYPE_END; item++) {
		item_type = item->type;

		switch (item_type) {
		case RTE_FLOW_ITEM_TYPE_ETH:
			eth_spec = item->spec;
			eth_mask = item->mask;
			if (eth_spec && eth_mask) {
				list[t].type = (tun_type == ICE_NON_TUN) ?
					ICE_MAC_OFOS : ICE_MAC_IL;
				struct ice_ether_hdr *h;
				struct ice_ether_hdr *m;
				uint16_t i = 0;
				h = &list[t].h_u.eth_hdr;
				m = &list[t].m_u.eth_hdr;
				for (j = 0; j < RTE_ETHER_ADDR_LEN; j++) {
					if (eth_mask->src.addr_bytes[j] ==
								UINT8_MAX) {
						h->src_addr[j] =
						eth_spec->src.addr_bytes[j];
						m->src_addr[j] =
						eth_mask->src.addr_bytes[j];
						i = 1;
					}
					if (eth_mask->dst.addr_bytes[j] ==
								UINT8_MAX) {
						h->dst_addr[j] =
						eth_spec->dst.addr_bytes[j];
						m->dst_addr[j] =
						eth_mask->dst.addr_bytes[j];
						i = 1;
					}
				}
				if (i)
					t++;
				if (eth_mask->type == UINT16_MAX) {
					list[t].type = ICE_ETYPE_OL;
					list[t].h_u.ethertype.ethtype_id =
						eth_spec->type;
					list[t].m_u.ethertype.ethtype_id =
						UINT16_MAX;
					t++;
				}
			} else if (!eth_spec && !eth_mask) {
				list[t].type = (tun_type == ICE_NON_TUN) ?
					ICE_MAC_OFOS : ICE_MAC_IL;
			}
			break;

		case RTE_FLOW_ITEM_TYPE_IPV4:
			ipv4_spec = item->spec;
			ipv4_mask = item->mask;
			if (ipv4_spec && ipv4_mask) {
				list[t].type = (tun_type == ICE_NON_TUN) ?
					ICE_IPV4_OFOS : ICE_IPV4_IL;
				if (ipv4_mask->hdr.src_addr == UINT32_MAX) {
					list[t].h_u.ipv4_hdr.src_addr =
						ipv4_spec->hdr.src_addr;
					list[t].m_u.ipv4_hdr.src_addr =
						UINT32_MAX;
				}
				if (ipv4_mask->hdr.dst_addr == UINT32_MAX) {
					list[t].h_u.ipv4_hdr.dst_addr =
						ipv4_spec->hdr.dst_addr;
					list[t].m_u.ipv4_hdr.dst_addr =
						UINT32_MAX;
				}
				if (ipv4_mask->hdr.time_to_live == UINT8_MAX) {
					list[t].h_u.ipv4_hdr.time_to_live =
						ipv4_spec->hdr.time_to_live;
					list[t].m_u.ipv4_hdr.time_to_live =
						UINT8_MAX;
				}
				if (ipv4_mask->hdr.next_proto_id == UINT8_MAX) {
					list[t].h_u.ipv4_hdr.protocol =
						ipv4_spec->hdr.next_proto_id;
					list[t].m_u.ipv4_hdr.protocol =
						UINT8_MAX;
				}
				if (ipv4_mask->hdr.type_of_service ==
						UINT8_MAX) {
					list[t].h_u.ipv4_hdr.tos =
						ipv4_spec->hdr.type_of_service;
					list[t].m_u.ipv4_hdr.tos = UINT8_MAX;
				}
				t++;
			} else if (!ipv4_spec && !ipv4_mask) {
				list[t].type = (tun_type == ICE_NON_TUN) ?
					ICE_IPV4_OFOS : ICE_IPV4_IL;
			}
			break;

		case RTE_FLOW_ITEM_TYPE_IPV6:
			ipv6_spec = item->spec;
			ipv6_mask = item->mask;
			if (ipv6_spec && ipv6_mask) {
				list[t].type = (tun_type == ICE_NON_TUN) ?
					ICE_IPV6_OFOS : ICE_IPV6_IL;
				struct ice_ipv6_hdr *f;
				struct ice_ipv6_hdr *s;
				f = &list[t].h_u.ipv6_hdr;
				s = &list[t].m_u.ipv6_hdr;
				for (j = 0; j < ICE_IPV6_ADDR_LENGTH; j++) {
					if (ipv6_mask->hdr.src_addr[j] ==
								UINT8_MAX) {
						f->src_addr[j] =
						ipv6_spec->hdr.src_addr[j];
						s->src_addr[j] =
						ipv6_mask->hdr.src_addr[j];
					}
					if (ipv6_mask->hdr.dst_addr[j] ==
								UINT8_MAX) {
						f->dst_addr[j] =
						ipv6_spec->hdr.dst_addr[j];
						s->dst_addr[j] =
						ipv6_mask->hdr.dst_addr[j];
					}
				}
				if (ipv6_mask->hdr.proto == UINT8_MAX) {
					f->next_hdr =
						ipv6_spec->hdr.proto;
					s->next_hdr = UINT8_MAX;
				}
				if (ipv6_mask->hdr.hop_limits == UINT8_MAX) {
					f->hop_limit =
						ipv6_spec->hdr.hop_limits;
					s->hop_limit = UINT8_MAX;
				}
				t++;
			} else if (!ipv6_spec && !ipv6_mask) {
				list[t].type = (tun_type == ICE_NON_TUN) ?
					ICE_IPV4_OFOS : ICE_IPV4_IL;
			}
			break;

		case RTE_FLOW_ITEM_TYPE_UDP:
			udp_spec = item->spec;
			udp_mask = item->mask;
			if (udp_spec && udp_mask) {
				if (tun_type == ICE_SW_TUN_VXLAN &&
						tunnel_valid == 0)
					list[t].type = ICE_UDP_OF;
				else
					list[t].type = ICE_UDP_ILOS;
				if (udp_mask->hdr.src_port == UINT16_MAX) {
					list[t].h_u.l4_hdr.src_port =
						udp_spec->hdr.src_port;
					list[t].m_u.l4_hdr.src_port =
						udp_mask->hdr.src_port;
				}
				if (udp_mask->hdr.dst_port == UINT16_MAX) {
					list[t].h_u.l4_hdr.dst_port =
						udp_spec->hdr.dst_port;
					list[t].m_u.l4_hdr.dst_port =
						udp_mask->hdr.dst_port;
				}
				t++;
			} else if (!udp_spec && !udp_mask) {
				list[t].type = ICE_UDP_ILOS;
			}
			break;

		case RTE_FLOW_ITEM_TYPE_TCP:
			tcp_spec = item->spec;
			tcp_mask = item->mask;
			if (tcp_spec && tcp_mask) {
				list[t].type = ICE_TCP_IL;
				if (tcp_mask->hdr.src_port == UINT16_MAX) {
					list[t].h_u.l4_hdr.src_port =
						tcp_spec->hdr.src_port;
					list[t].m_u.l4_hdr.src_port =
						tcp_mask->hdr.src_port;
				}
				if (tcp_mask->hdr.dst_port == UINT16_MAX) {
					list[t].h_u.l4_hdr.dst_port =
						tcp_spec->hdr.dst_port;
					list[t].m_u.l4_hdr.dst_port =
						tcp_mask->hdr.dst_port;
				}
				t++;
			} else if (!tcp_spec && !tcp_mask) {
				list[t].type = ICE_TCP_IL;
			}
			break;

		case RTE_FLOW_ITEM_TYPE_SCTP:
			sctp_spec = item->spec;
			sctp_mask = item->mask;
			if (sctp_spec && sctp_mask) {
				list[t].type = ICE_SCTP_IL;
				if (sctp_mask->hdr.src_port == UINT16_MAX) {
					list[t].h_u.sctp_hdr.src_port =
						sctp_spec->hdr.src_port;
					list[t].m_u.sctp_hdr.src_port =
						sctp_mask->hdr.src_port;
				}
				if (sctp_mask->hdr.dst_port == UINT16_MAX) {
					list[t].h_u.sctp_hdr.dst_port =
						sctp_spec->hdr.dst_port;
					list[t].m_u.sctp_hdr.dst_port =
						sctp_mask->hdr.dst_port;
				}
				t++;
			} else if (!sctp_spec && !sctp_mask) {
				list[t].type = ICE_SCTP_IL;
			}
			break;

		case RTE_FLOW_ITEM_TYPE_VXLAN:
			vxlan_spec = item->spec;
			vxlan_mask = item->mask;
			tunnel_valid = 1;
			if (vxlan_spec && vxlan_mask) {
				list[t].type = ICE_VXLAN;
				if (vxlan_mask->vni[0] == UINT8_MAX &&
					vxlan_mask->vni[1] == UINT8_MAX &&
					vxlan_mask->vni[2] == UINT8_MAX) {
					list[t].h_u.tnl_hdr.vni =
						(vxlan_spec->vni[2] << 16) |
						(vxlan_spec->vni[1] << 8) |
						vxlan_spec->vni[0];
					list[t].m_u.tnl_hdr.vni =
						UINT32_MAX;
				}
				t++;
			} else if (!vxlan_spec && !vxlan_mask) {
				list[t].type = ICE_VXLAN;
			}
			break;

		case RTE_FLOW_ITEM_TYPE_NVGRE:
			nvgre_spec = item->spec;
			nvgre_mask = item->mask;
			tunnel_valid = 1;
			if (nvgre_spec && nvgre_mask) {
				list[t].type = ICE_NVGRE;
				if (nvgre_mask->tni[0] == UINT8_MAX &&
					nvgre_mask->tni[1] == UINT8_MAX &&
					nvgre_mask->tni[2] == UINT8_MAX) {
					list[t].h_u.nvgre_hdr.tni_flow =
						(nvgre_spec->tni[2] << 16) |
						(nvgre_spec->tni[1] << 8) |
						nvgre_spec->tni[0];
					list[t].m_u.nvgre_hdr.tni_flow =
						UINT32_MAX;
				}
				t++;
			} else if (!nvgre_spec && !nvgre_mask) {
				list[t].type = ICE_NVGRE;
			}
			break;

		case RTE_FLOW_ITEM_TYPE_VOID:
		case RTE_FLOW_ITEM_TYPE_END:
			break;

		default:
			rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM, actions,
				   "Invalid pattern item.");
			goto out;
		}
	}

	*lkups_num = t;

	return 0;
out:
	return -rte_errno;
}

/* By now ice switch filter action code implement only
 * supports QUEUE or DROP.
 */
static int
ice_parse_switch_action(struct ice_pf *pf,
				 const struct rte_flow_action *actions,
				 struct rte_flow_error *error,
				 struct ice_adv_rule_info *rule_info)
{
	struct ice_vsi *vsi = pf->main_vsi;
	const struct rte_flow_action_queue *act_q;
	uint16_t base_queue;
	const struct rte_flow_action *action;
	enum rte_flow_action_type action_type;

	base_queue = pf->base_queue;
	for (action = actions; action->type !=
			RTE_FLOW_ACTION_TYPE_END; action++) {
		action_type = action->type;
		switch (action_type) {
		case RTE_FLOW_ACTION_TYPE_QUEUE:
			act_q = action->conf;
			rule_info->sw_act.fltr_act =
				ICE_FWD_TO_Q;
			rule_info->sw_act.fwd_id.q_id =
				base_queue + act_q->index;
			break;

		case RTE_FLOW_ACTION_TYPE_DROP:
			rule_info->sw_act.fltr_act =
				ICE_DROP_PACKET;
			break;

		case RTE_FLOW_ACTION_TYPE_VOID:
			break;

		default:
			rte_flow_error_set(error,
				EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				actions,
				"Invalid action type");
			return -rte_errno;
		}
	}

	rule_info->sw_act.vsi_handle = vsi->idx;
	rule_info->rx = 1;
	rule_info->sw_act.src = vsi->idx;
	rule_info->priority = 5;

	return 0;
}

static int
ice_switch_rule_set(struct ice_pf *pf,
			struct ice_adv_lkup_elem *list,
			uint16_t lkups_cnt,
			struct ice_adv_rule_info *rule_info,
			struct rte_flow *flow,
			struct rte_flow_error *error)
{
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	int ret;
	struct ice_rule_query_data rule_added = {0};
	struct ice_rule_query_data *filter_ptr;

	if (lkups_cnt > ICE_MAX_CHAIN_WORDS) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM, NULL,
			"item number too large for rule");
		return -rte_errno;
	}
	if (!list) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM, NULL,
			"lookup list should not be NULL");
		return -rte_errno;
	}

	ret = ice_add_adv_rule(hw, list, lkups_cnt, rule_info, &rule_added);

	if (!ret) {
		filter_ptr = rte_zmalloc("ice_switch_filter",
			sizeof(struct ice_rule_query_data), 0);
		if (!filter_ptr) {
			PMD_DRV_LOG(ERR, "failed to allocate memory");
			return -EINVAL;
		}
		flow->rule = filter_ptr;
		rte_memcpy(filter_ptr,
			&rule_added,
			sizeof(struct ice_rule_query_data));
	}

	return ret;
}

int
ice_create_switch_filter(struct ice_pf *pf,
			const struct rte_flow_item pattern[],
			const struct rte_flow_action actions[],
			struct rte_flow *flow,
			struct rte_flow_error *error)
{
	int ret = 0;
	struct ice_adv_rule_info rule_info = {0};
	struct ice_adv_lkup_elem *list = NULL;
	uint16_t lkups_num = 0;
	const struct rte_flow_item *item = pattern;
	uint16_t item_num = 0;
	enum ice_sw_tunnel_type tun_type = ICE_NON_TUN;

	for (; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		item_num++;
		if (item->type == RTE_FLOW_ITEM_TYPE_VXLAN)
			tun_type = ICE_SW_TUN_VXLAN;
		if (item->type == RTE_FLOW_ITEM_TYPE_NVGRE)
			tun_type = ICE_SW_TUN_NVGRE;
	}
	rule_info.tun_type = tun_type;

	list = rte_zmalloc(NULL, item_num * sizeof(*list), 0);
	if (!list) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "No memory for PMD internal items");
		return -rte_errno;
	}

	ret = ice_parse_switch_filter(pattern, actions, error,
			list, &lkups_num, tun_type);
	if (ret)
		goto error;

	ret = ice_parse_switch_action(pf, actions, error, &rule_info);
	if (ret)
		goto error;

	ret = ice_switch_rule_set(pf, list, lkups_num, &rule_info, flow, error);
	if (ret)
		goto error;

	rte_free(list);
	return 0;

error:
	rte_free(list);

	return -rte_errno;
}

int
ice_destroy_switch_filter(struct ice_pf *pf,
			struct rte_flow *flow,
			struct rte_flow_error *error)
{
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	int ret;
	struct ice_rule_query_data *filter_ptr;

	filter_ptr = (struct ice_rule_query_data *)
			flow->rule;

	if (!filter_ptr) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
			"no such flow"
			" create by switch filter");
		return -rte_errno;
	}

	ret = ice_rem_adv_rule_by_id(hw, filter_ptr);
	if (ret) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
			"fail to destroy switch filter rule");
		return -rte_errno;
	}

	rte_free(filter_ptr);
	return ret;
}

void
ice_free_switch_filter_rule(void *rule)
{
	struct ice_rule_query_data *filter_ptr;

	filter_ptr = (struct ice_rule_query_data *)rule;

	rte_free(filter_ptr);
}
