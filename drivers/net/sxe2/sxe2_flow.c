/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#include <sys/queue.h>
#include <unistd.h>
#include "sxe2_ethdev.h"
#include "sxe2_flow.h"
#include "sxe2_flow_parse_pattern.h"
#include "sxe2_flow_parse_action.h"
#include "sxe2_flow_parse_engine.h"
#include "sxe2_cmd_chnl.h"
#include "sxe2_flow_public.h"
#include "sxe2_common_log.h"

static int32_t sxe2_check_para(const struct rte_flow_attr *attr,
			   const struct rte_flow_item pattern[],
			   const struct rte_flow_action actions[],
			   struct rte_flow_error *error)
{
	int32_t ret = 0;
	if (!pattern) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM_NUM,
				NULL, "NULL pattern.");
		ret = -rte_errno;
		goto l_end;
	}

	if (!actions) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION_NUM,
				NULL, "NULL action.");
		ret = -rte_errno;
		goto l_end;
	}

	if (!attr) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ATTR,
				NULL, "NULL attribute.");
		ret = -rte_errno;
		goto l_end;
	}

l_end:
	return ret;
}

static int32_t sxe2_flow_valid_attr(const struct rte_flow_attr *attr, struct rte_flow_error *error)
{
	int32_t ret = 0;

	if (!attr->ingress) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ATTR_INGRESS,
				attr, "Only support ingress.");
		ret = -rte_errno;
		goto l_end;
	}

	if (attr->egress) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ATTR_EGRESS,
				attr, "Not support egress.");
		ret = -rte_errno;
		goto l_end;
	}

	if (attr->group >= 4) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ATTR_GROUP,
				attr, "Not support group >= 4.");
		ret = -rte_errno;
		goto l_end;
	}
l_end:
	return ret;
}

static int32_t sxe2_flow_check_hdr_duplicate(struct sxe2_flow_item *item_new,
					 struct sxe2_flow_item *item_exist)
{
	int32_t ret = 0;
	uint16_t i = 0;
	uint16_t size = sizeof(struct sxe2_flow_item);
	union sxe2_flow_item_raw item_raw_new;
	union sxe2_flow_item_raw item_raw_exist;
	rte_memcpy(&item_raw_new.item, item_new, size);
	rte_memcpy(&item_raw_exist.item, item_exist, size);

	for (i = 0; i < size; i++) {
		if (item_raw_new.raw[i] != item_raw_exist.raw[i])
			goto l_end;
	}
	ret = -EEXIST;
l_end:
	return ret;
}

static int32_t sxe2_flow_check_flow_duplicate(struct sxe2_flow *flow_new,
					      struct sxe2_flow *flow_exist)
{
	int32_t ret = 0;
	int32_t ret_mask1 = 0;
	int32_t ret_mask2 = 0;
	int32_t ret_spec1 = 0;
	int32_t ret_spec2 = 0;

	if (flow_new->engine_type != flow_exist->engine_type)
		goto l_end;
	if (flow_new->meta.flow_type != flow_exist->meta.flow_type)
		goto l_end;
	if (!sxe2_bitmap_equal(flow_new->flow_type, flow_exist->flow_type,
			SXE2_EXPANSION_MAX))
		goto l_end;
	if (flow_new->meta.flow_prio != flow_exist->meta.flow_prio)
		goto l_end;

	ret_mask1 = sxe2_flow_check_hdr_duplicate(&flow_new->pattern_outer.item_mask,
						  &flow_exist->pattern_outer.item_mask);
	ret_mask2 = sxe2_flow_check_hdr_duplicate(&flow_new->pattern_inner.item_mask,
						  &flow_exist->pattern_inner.item_mask);

	ret_spec1 = sxe2_flow_check_hdr_duplicate(&flow_new->pattern_outer.item_spec,
						  &flow_exist->pattern_outer.item_spec);
	ret_spec2 = sxe2_flow_check_hdr_duplicate(&flow_new->pattern_inner.item_spec,
						  &flow_exist->pattern_inner.item_spec);

	if (flow_new->engine_type == SXE2_FLOW_ENGINE_FNAV) {
		if (ret_mask1 == 0 || ret_mask2 == 0) {
			ret = -EEXIST;
			goto l_end;
		}

		if (ret_spec1 == -EEXIST && ret_spec2 == -EEXIST) {
			ret = -EEXIST;
			goto l_end;
		}
	} else {
		if (ret_mask1 == -EEXIST && ret_mask2 == -EEXIST &&
			ret_spec1 == -EEXIST && ret_spec2 == -EEXIST) {
			ret = -EEXIST;
			goto l_end;
		}
	}

l_end:
	return ret;
}

static int32_t sxe2_flow_check_flow_list_duplicate(struct rte_eth_dev *dev,
						   struct rte_flow *flow_list)
{
	int32_t ret = 0;
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_flow *sxe2_flow_new = NULL;
	struct rte_flow *rte_flow_exist = NULL;
	struct sxe2_flow *sxe2_flow_exist = NULL;
	TAILQ_FOREACH(sxe2_flow_new, &flow_list->sxe2_flow_list, next) {
		TAILQ_FOREACH(rte_flow_exist, &adapter->flow_ctxt.rte_flow_list, next) {
			TAILQ_FOREACH(sxe2_flow_exist, &rte_flow_exist->sxe2_flow_list, next) {
				ret = sxe2_flow_check_flow_duplicate(sxe2_flow_new,
								     sxe2_flow_exist);
				if (ret != 0)
					goto l_end;
			}
		}
	}
l_end:
	return ret;
}

static int32_t sxe2_flow_check_function(struct rte_eth_dev *dev,
				    struct rte_flow *flow_list,
				    struct rte_flow_error *error)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_flow_list_t *sxe2_flow_list = &flow_list->sxe2_flow_list;
	struct sxe2_flow *flow = TAILQ_FIRST(sxe2_flow_list);
	int32_t ret = 0;

	uint16_t flow_dst_vsi = UINT16_MAX;

	if (adapter->dev_type == SXE2_DEV_T_VF) {
		if (sxe2_test_bit(SXE2_FLOW_ACTION_TO_VSI, flow->action.act_types)) {
			flow_dst_vsi = flow->action.vsi.vsi_index;

			if (adapter->vsi_ctxt.dpdk_vsi_id != flow_dst_vsi &&
				adapter->vsi_ctxt.kernel_vsi_id != flow_dst_vsi) {
				PMD_LOG_ERR(DRV, "Failed to redirect other function");
				rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					"Failed to redirect other function");
				ret = -ENOTSUP;
				goto l_end;
			}
		}

		if (sxe2_test_bit(SXE2_FLOW_ACTION_TO_VSI_LIST, flow->action.act_types)) {
			PMD_LOG_ERR(DRV,
				"Failed to redirect multiple driver or function");
				rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					"Failed to redirect multiple driver or function");
				ret = -ENOTSUP;
				goto l_end;
		}

		if (!adapter->flow_isolated &&
			flow->engine_type == SXE2_FLOW_ENGINE_SWITCH) {
			PMD_LOG_ERR(DRV,
				"Failed to switch engine rules in a non-flow-isolated state");
				rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					"Failed to switch engine rules in a non-flow-isolated state");
				ret = -ENOTSUP;
				goto l_end;
		}

		if (adapter->switchdev_info.is_switchdev &&
			flow->engine_type == SXE2_FLOW_ENGINE_SWITCH) {
			PMD_LOG_ERR(DRV,
				"Failed to switch engine rules in a switchdev mode state");
				rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					"Failed to switch engine rules in a switchdev mode state");
				ret = -ENOTSUP;
				goto l_end;
		}
	}

	if (adapter->is_dev_repr) {
		if (flow->engine_type != SXE2_FLOW_ENGINE_SWITCH) {
			PMD_LOG_ERR(DRV,
				"Failed to config non switch engine rules in representor dev");
				rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					"Failed to config non switch engine rules in representor dev");
				ret = -ENOTSUP;
				goto l_end;
		}

		if (sxe2_test_bit(SXE2_FLOW_ACTION_QUEUE, flow->action.act_types) ||
			sxe2_test_bit(SXE2_FLOW_ACTION_Q_REGION, flow->action.act_types)) {
			PMD_LOG_ERR(DRV,
				"Failed to config queue rules in representor dev");
				rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					"Failed to config queue rules in representor dev");
				ret = -ENOTSUP;
				goto l_end;
		}
	}

	if (adapter->switchdev_info.is_switchdev &&
		adapter->dev_type == SXE2_DEV_T_PF &&
		!adapter->is_dev_repr &&
		!adapter->flow_isolated) {
		if (sxe2_test_bit(SXE2_FLOW_ACTION_TO_VSI, flow->action.act_types)) {
			if (flow->action.vsi.vsi_index == adapter->vsi_ctxt.dpdk_vsi_id) {
				PMD_LOG_ERR(DRV,
					"Failed to config rx fwd rule to current uplink dev");
					rte_flow_error_set(error, ENOTSUP,
						RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
						"Failed to config rx fwd rule to current uplink dev");
					ret = -ENOTSUP;
					goto l_end;
			}
		}
	}

l_end:
	return ret;
}

static int32_t sxe2_flow_meta_proc(struct rte_eth_dev *dev,
			       const struct rte_flow_attr *attr,
			       struct rte_flow *flow_list,
			       struct rte_flow_error *error)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_flow_list_t *sxe2_flow_list = &flow_list->sxe2_flow_list;
	struct sxe2_flow *flow = TAILQ_FIRST(sxe2_flow_list);
	int32_t ret = 0;

	if (attr->priority >= 1) {
		if (flow->engine_type != SXE2_FLOW_ENGINE_SWITCH) {
			PMD_LOG_ERR(DRV, "Only support priority 0.");
			rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY,
					attr, "Only support priority 0.");
			ret = -rte_errno;
			goto l_end;
		} else if (!adapter->switchdev_info.is_switchdev) {
			PMD_LOG_ERR(DRV, "Legacy mode only support priority 0.");
			rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY,
					attr, "Legacy mode only priority 0.");
			ret = -rte_errno;
			goto l_end;
		} else {
			flow->meta.flow_prio = attr->priority;
		}
	}

	flow->meta.flow_src_vsi = adapter->vsi_ctxt.dpdk_vsi_id;

	if (adapter->is_dev_repr && adapter->repr_priv_data &&
		adapter->repr_priv_data->parent_adapter) {
		flow->meta.flow_rule_vsi =
			adapter->repr_priv_data->parent_adapter->vsi_ctxt.dpdk_vsi_id;
	} else {
		flow->meta.flow_rule_vsi = adapter->vsi_ctxt.dpdk_vsi_id;
	}

	if (flow->engine_type == SXE2_FLOW_ENGINE_SWITCH) {
		flow->meta.switch_pattern_dup_allow =
			adapter->devargs.flow_dup_pattern_mode;

		flow->meta.switch_src_direct = SXE2_FLOW_SW_DIRECT_RX;

		if (adapter->switchdev_info.is_switchdev && adapter->is_dev_repr) {
			flow->meta.switch_src_direct = SXE2_FLOW_SW_DIRECT_TX;
			flow->meta.flow_src_vsi = adapter->repr_priv_data->repr_vf_vsi_id;
		}
	}

l_end:
	return ret;
}

static int32_t sxe2_flow_src_split_proc(struct rte_eth_dev *dev,
				    struct rte_flow *flow_list,
				    struct rte_flow_error *error)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_flow_list_t *sxe2_flow_list = &flow_list->sxe2_flow_list;
	struct sxe2_flow *flow = TAILQ_FIRST(sxe2_flow_list);
	int32_t ret = 0;

	int32_t idx = 0;
	uint8_t flow_cnt = 0;
	uint8_t flow_create_cnt = 0;
	uint8_t flow_bond_num = 1;
	uint16_t flow_src_vsi[SXE2_MAX_DRV_TYPE_CNT][SXE2_MAX_BOND_MEMBER_CNT];
	uint16_t flow_dst_vsi = UINT16_MAX;
	struct sxe2_flow *flow_new = NULL;

	if (sxe2_test_bit(SXE2_FLOW_ACTION_TO_VSI, flow->action.act_types))
		flow_dst_vsi = flow->action.vsi.vsi_index;

	for (idx = 0; idx < SXE2_MAX_BOND_MEMBER_CNT; idx++) {
		flow_src_vsi[SXE2_MAX_DRV_TYPE_DPDK][idx] = UINT16_MAX;
		flow_src_vsi[SXE2_MAX_DRV_TYPE_KERNEL][idx] = UINT16_MAX;
	}

	flow_src_vsi[SXE2_MAX_DRV_TYPE_DPDK][0] = adapter->vsi_ctxt.dpdk_vsi_id;
	flow_src_vsi[SXE2_MAX_DRV_TYPE_KERNEL][0] = adapter->vsi_ctxt.kernel_vsi_id;
	if (flow->engine_type == SXE2_FLOW_ENGINE_FNAV ||
		flow->engine_type == SXE2_FLOW_ENGINE_ACL) {
		if (!adapter->devargs.func_flow_direct_en &&
			adapter->dev_type != SXE2_DEV_T_PF_BOND) {
			if (adapter->flow_isolated) {
				for (idx = 0; idx < flow_bond_num; idx++) {
					if (flow_src_vsi[SXE2_MAX_DRV_TYPE_KERNEL][idx] !=
								UINT16_MAX)
						flow_src_vsi[SXE2_MAX_DRV_TYPE_DPDK][idx] =
										UINT16_MAX;
				}
			} else {
				for (idx = 0; idx < flow_bond_num; idx++)
					flow_src_vsi[SXE2_MAX_DRV_TYPE_KERNEL][idx] = UINT16_MAX;
			}
		}

		for (idx = 0; idx < flow_bond_num; idx++) {
			if (flow_dst_vsi == flow_src_vsi[SXE2_MAX_DRV_TYPE_KERNEL][idx])
				flow_src_vsi[SXE2_MAX_DRV_TYPE_KERNEL][idx] = UINT16_MAX;
			if (flow_dst_vsi == flow_src_vsi[SXE2_MAX_DRV_TYPE_DPDK][idx])
				flow_src_vsi[SXE2_MAX_DRV_TYPE_DPDK][idx] = UINT16_MAX;
		}
	} else {
		for (idx = 0; idx < flow_bond_num; idx++)
			flow_src_vsi[SXE2_MAX_DRV_TYPE_KERNEL][idx] = UINT16_MAX;
	}

	if (adapter->switchdev_info.is_switchdev && adapter->is_dev_repr) {
		flow_bond_num = 1;
		flow_src_vsi[SXE2_MAX_DRV_TYPE_DPDK][0] =
			adapter->repr_priv_data->repr_vf_u_vsi_id;
		flow_src_vsi[SXE2_MAX_DRV_TYPE_KERNEL][0] =
			adapter->repr_priv_data->repr_vf_k_vsi_id;
	}

	for (idx = 0; idx < flow_bond_num; idx++) {
		if (flow_src_vsi[SXE2_MAX_DRV_TYPE_KERNEL][idx] != UINT16_MAX)
			flow_cnt++;
		if (flow_src_vsi[SXE2_MAX_DRV_TYPE_DPDK][idx] != UINT16_MAX)
			flow_cnt++;
	}

	if (flow_cnt == 0) {
		PMD_LOG_ERR(DRV, "Failed to redirect same device.");
		rte_flow_error_set(error, ENOTSUP,
			RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
			"Failed to redirect same device");
		ret = -EINVAL;
		goto l_end;
	}

	for (idx = 0; idx < flow_bond_num; idx++) {
		if (flow_src_vsi[SXE2_MAX_DRV_TYPE_DPDK][idx] != UINT16_MAX) {
			if (flow_create_cnt == 0) {
				flow->meta.flow_src_vsi =
					flow_src_vsi[SXE2_MAX_DRV_TYPE_DPDK][idx];
				flow_create_cnt++;
			} else {
				flow_new = rte_zmalloc("sxe2_flow", sizeof(*flow), 0);
				if (!flow_new) {
					rte_flow_error_set(error, ENOMEM,
						   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
						   "Failed to alloc memory for flow rule");
					ret = -ENOMEM;
					goto l_end;
				}
				rte_memcpy(flow_new, flow, sizeof(struct sxe2_flow));
				TAILQ_INSERT_TAIL(sxe2_flow_list, flow_new, next);
				flow_new->meta.flow_src_vsi =
						flow_src_vsi[SXE2_MAX_DRV_TYPE_DPDK][idx];
				flow_create_cnt++;
			}
		}
		if (flow_src_vsi[SXE2_MAX_DRV_TYPE_KERNEL][idx] != UINT16_MAX) {
			if (flow_create_cnt == 0) {
				flow->meta.flow_src_vsi =
					flow_src_vsi[SXE2_MAX_DRV_TYPE_KERNEL][idx];
				flow_create_cnt++;
			} else {
				flow_new = rte_zmalloc("sxe2_flow", sizeof(*flow), 0);
				if (!flow_new) {
					rte_flow_error_set(error, ENOMEM,
						RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
						"Failed to alloc memory for flow rule");
					ret = -ENOMEM;
					goto l_end;
				}
				rte_memcpy(flow_new, flow, sizeof(struct sxe2_flow));
				TAILQ_INSERT_TAIL(sxe2_flow_list, flow_new, next);
				flow_new->meta.flow_src_vsi =
					flow_src_vsi[SXE2_MAX_DRV_TYPE_KERNEL][idx];
				flow_create_cnt++;
			}
		}
	}

l_end:
	return ret;
}

static int32_t sxe2_flow_adjust_action(struct rte_eth_dev *dev __rte_unused,
				   struct rte_flow *flow_list,
				   struct rte_flow_error *error __rte_unused)
{
	struct sxe2_flow_list_t *sxe2_flow_list = &flow_list->sxe2_flow_list;
	struct sxe2_flow *flow = NULL;
	int32_t ret = 0;
	int32_t dest_num = 0;
	int32_t pass_num = 0;
	int32_t mark_num = 0;
	int32_t count_num = 0;
	int32_t drop_num = 0;

	TAILQ_FOREACH(flow, sxe2_flow_list, next) {
		if (flow->engine_type == SXE2_FLOW_ENGINE_FNAV) {
			dest_num = sxe2_test_bit(SXE2_FLOW_ACTION_Q_REGION,
					    flow->action.act_types) +
				   sxe2_test_bit(SXE2_FLOW_ACTION_QUEUE,
					    flow->action.act_types);
			pass_num = sxe2_test_bit(SXE2_FLOW_ACTION_PASSTHRU,
					    flow->action.act_types);
			mark_num = sxe2_test_bit(SXE2_FLOW_ACTION_MARK,
					    flow->action.act_types);
			count_num = sxe2_test_bit(SXE2_FLOW_ACTION_COUNT,
					     flow->action.act_types);
			drop_num = sxe2_test_bit(SXE2_FLOW_ACTION_DROP,
					    flow->action.act_types);

			if (dest_num) {
				sxe2_clear_bit(SXE2_FLOW_ACTION_PASSTHRU,
					  flow->action.act_types);
				pass_num = 0;
				sxe2_clear_bit(SXE2_FLOW_ACTION_TO_VSI,
					  flow->action.act_types);
			}

			if (pass_num)
				flow->action.passthru.vsi_index = flow->meta.flow_src_vsi;

			if (mark_num) {
				if (dest_num == 0) {
					flow->action.q_region.q_index = 0;
					flow->action.q_region.region = 7;
					flow->action.q_region.vsi_index = flow->meta.flow_src_vsi;
					sxe2_set_bit(SXE2_FLOW_ACTION_Q_REGION,
						flow->action.act_types);
					dest_num++;
				}
				sxe2_clear_bit(SXE2_FLOW_ACTION_PASSTHRU,
					flow->action.act_types);
				pass_num = 0;
			}
			if (count_num) {
				if (dest_num == 0 && drop_num == 0) {
					if (pass_num == 0) {
						sxe2_set_bit(SXE2_FLOW_ACTION_PASSTHRU,
							flow->action.act_types);
						flow->action.passthru.vsi_index =
									flow->meta.flow_src_vsi;
						pass_num++;
					}
				}
			}
			PMD_LOG_DEBUG(DRV, "dest_num: %d, pass_num: %d, mark_num: %d, count_num: "
				"%d, drop_num: %d", dest_num, pass_num, mark_num, count_num,
				drop_num);
			PMD_LOG_DEBUG(DRV, "src_vsi: %d", flow->meta.flow_src_vsi);
		}
	}

	return ret;
}

int32_t sxe2_flow_init_udp_tunnel_port(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	int32_t ret = 0;
	uint16_t i = 0;
	uint16_t *flow_udp_tunnel_port = NULL;

	memset(adapter->flow_ctxt.tunnel_port_list, 0,
	       sizeof(adapter->flow_ctxt.tunnel_port_list));

	flow_udp_tunnel_port = adapter->flow_ctxt.tunnel_port_list;
	for (i = 0; i < SXE2_FLOW_UDP_TUNNEL_MAX; i++) {
		if (flow_udp_tunnel_port[i] == 0) {
			ret = sxe2_drv_get_udp_tunnel_port(adapter, i,
							   &flow_udp_tunnel_port[i]);
			if (ret != 0) {
				PMD_LOG_ERR(DRV, "Failed to get udp tunnel port, proto: %d,"
					    "ret: %d", i, ret);
				goto l_end;
			}
		}
	}

l_end:
	return ret;
}

static int32_t sxe2_flowlist_add_tunnel_port(struct rte_eth_dev *dev,
			struct rte_flow *flow_list,
			struct rte_flow_error *error)
{
	struct sxe2_flow_list_t *sxe2_flow_list = &flow_list->sxe2_flow_list;
	struct sxe2_flow *flow = TAILQ_FIRST(sxe2_flow_list);
	enum sxe2_flow_tunnel_type tunnel_type = flow->meta.tunnel_type;
	DECLARE_BITMAP(flow_type, SXE2_EXPANSION_MAX);
	sxe2_bitmap_zero(flow_type, SXE2_EXPANSION_MAX);
	sxe2_bitmap_copy(flow_type, flow->flow_type, SXE2_EXPANSION_MAX);
	int32_t ret = 0;

	if (flow->engine_type == SXE2_FLOW_ENGINE_FNAV)
		return sxe2_flow_add_tunnel_port(dev, error, flow, flow_type, tunnel_type);

	return ret;
}

static int32_t sxe2_flow_check_item_empty(uint8_t *item, uint16_t size)
{
	uint16_t i = 0;

	for (i = 0; i < size; i++) {
		if (item[i] != 0)
			return -1;
	}
	return 0;
}

static int32_t sxe2_flowlist_add_proto_type(struct rte_eth_dev *dev __rte_unused,
					struct rte_flow *flow_list,
					struct rte_flow_error *error)
{
	struct sxe2_flow_list_t *sxe2_flow_list = &flow_list->sxe2_flow_list;
	struct sxe2_flow *flow = TAILQ_FIRST(sxe2_flow_list);
	struct sxe2_flow_pattern *pattern = &flow->pattern_outer;
	int32_t ret = 0;

	if (flow->engine_type == SXE2_FLOW_ENGINE_SWITCH) {
		if (sxe2_test_bit(SXE2_EXPANSION_OUTER_IPV4, flow->flow_type) &&
		    sxe2_flow_check_item_empty((uint8_t *)&pattern->item_mask.ipv4,
			sizeof(pattern->item_mask.ipv4)) == 0) {
			sxe2_set_bit(SXE2_FLOW_FLD_ID_ETH_TYPE, pattern->map_spec);
			pattern->item_spec.eth.ether_type =
				rte_cpu_to_be_16(SXE2_FLOW_ETH_TYPE_IPV4);
			pattern->item_mask.eth.ether_type = 0xffff;
		}
		if (sxe2_test_bit(SXE2_EXPANSION_OUTER_IPV6, flow->flow_type) &&
		    sxe2_flow_check_item_empty((uint8_t *)&pattern->item_mask.ipv6,
			sizeof(pattern->item_mask.ipv6)) == 0) {
			sxe2_set_bit(SXE2_FLOW_FLD_ID_ETH_TYPE, pattern->map_spec);
			pattern->item_spec.eth.ether_type =
				rte_cpu_to_be_16(SXE2_FLOW_ETH_TYPE_IPV6);
			pattern->item_mask.eth.ether_type = 0xffff;
		}

		if (flow->meta.tunnel_type == SXE2_FLOW_TUNNEL_TYPE_NONE) {
			if (sxe2_test_bit(SXE2_EXPANSION_OUTER_UDP, flow->flow_type) &&
			    sxe2_flow_check_item_empty((uint8_t *)&pattern->item_mask.udp,
				sizeof(pattern->item_mask.udp)) == 0) {
				if (sxe2_test_bit(SXE2_EXPANSION_OUTER_IPV4,
					     flow->flow_type)) {
					sxe2_set_bit(SXE2_FLOW_FLD_ID_IPV4_PROT, pattern->map_spec);
					pattern->item_spec.ipv4.protocol =
						SXE2_FLOW_IP_PROTOCOL_UDP;
					pattern->item_mask.ipv4.protocol = 0xff;
				}
				if (sxe2_test_bit(SXE2_EXPANSION_OUTER_IPV6,
					     flow->flow_type)) {
					ret = -EINVAL;
					rte_flow_error_set(error, ENOENT,
						RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
						"UDP after IPv6 must has pattern item.");
					PMD_LOG_ERR(DRV,
						"UDP after IPv6 must has pattern item.");
					goto l_end;
				}
			}
			if (sxe2_test_bit(SXE2_EXPANSION_OUTER_TCP, flow->flow_type) &&
			    sxe2_flow_check_item_empty((uint8_t *)&pattern->item_mask.tcp,
				sizeof(pattern->item_mask.tcp)) == 0) {
				if (sxe2_test_bit(SXE2_EXPANSION_OUTER_IPV4,
					     flow->flow_type)) {
					sxe2_set_bit(SXE2_FLOW_FLD_ID_IPV4_PROT,
						pattern->map_spec);
					pattern->item_spec.ipv4.protocol =
						SXE2_FLOW_IP_PROTOCOL_TCP;
					pattern->item_mask.ipv4.protocol = 0xff;
				}
				if (sxe2_test_bit(SXE2_EXPANSION_OUTER_IPV6,
					     flow->flow_type)) {
					ret = -EINVAL;
					rte_flow_error_set(error, ENOENT,
						RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
						"TCP after IPv6 must has pattern item.");
					PMD_LOG_ERR(DRV,
						"TCP after IPv6 must has pattern item.");
					goto l_end;
				}
			}
			if (sxe2_test_bit(SXE2_EXPANSION_OUTER_SCTP,
				     flow->flow_type)) {
				ret = -EINVAL;
				rte_flow_error_set(error, ENOENT,
					RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					"SWITCH not support SCTP.");
				PMD_LOG_ERR(DRV, "SWITCH not support SCTP.");
				goto l_end;
			}
		}
	}
l_end:
	return ret;
}

static int32_t sxe2_flow_tunnel_split_proc(struct rte_eth_dev *dev __rte_unused,
				       struct rte_flow *flow_list,
				       struct rte_flow_error *error)
{
	struct sxe2_flow_list_t *sxe2_flow_list = &flow_list->sxe2_flow_list;
	struct sxe2_flow_list_t tunnel_flow_list;
	struct sxe2_flow *sxe2_flow_exist = NULL;
	struct sxe2_flow *sxe2_flow_new = NULL;
	struct sxe2_flow_pattern *pattern = NULL;
	int32_t ret = 0;

	TAILQ_INIT(&tunnel_flow_list);

	TAILQ_FOREACH(sxe2_flow_exist, sxe2_flow_list, next) {
		if (sxe2_flow_exist->engine_type != SXE2_FLOW_ENGINE_SWITCH)
			continue;
		if (sxe2_test_bit(SXE2_FLOW_FLD_ID_IPV4_PROT,
				sxe2_flow_exist->pattern_outer.map_spec)) {
			pattern = &sxe2_flow_exist->pattern_outer;
			if ((pattern->item_spec.ipv4.protocol &
				pattern->item_mask.ipv4.protocol) ==
				(SXE2_FLOW_IP_PROTOCOL_GRE &
				pattern->item_mask.ipv4.protocol)) {
				sxe2_flow_new = rte_zmalloc("sxe2_flow",
					sizeof(struct sxe2_flow), 0);
				if (!sxe2_flow_new) {
					rte_flow_error_set(error, ENOMEM,
						RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
						"Failed to alloc memory for flow rule");
					ret = -ENOMEM;
					goto l_end;
				}
				rte_memcpy(sxe2_flow_new, sxe2_flow_exist,
					sizeof(struct sxe2_flow));
				pattern = &sxe2_flow_new->pattern_outer;
				sxe2_flow_new->meta.tunnel_type =
					SXE2_FLOW_TUNNEL_TYPE_GRE;
				sxe2_set_bit(SXE2_FLOW_HDR_GRE, pattern->hdrs);
				pattern->item_spec.ipv4.protocol =
					SXE2_FLOW_IP_PROTOCOL_GRE;
				pattern->item_mask.ipv4.protocol = 0xff;
				TAILQ_INSERT_TAIL(&tunnel_flow_list, sxe2_flow_new, next);
			}
		}
	}
	TAILQ_FOREACH(sxe2_flow_exist, &tunnel_flow_list, next)
		TAILQ_INSERT_TAIL(sxe2_flow_list, sxe2_flow_exist, next);

l_end:
	return ret;
}

static int32_t sxe2_flow_post_proc(struct rte_eth_dev *dev,
			       const struct rte_flow_attr *attr,
			       struct rte_flow *flow_list,
			       struct rte_flow_error *error)
{
	int32_t ret = 0;

	ret = sxe2_flowlist_add_tunnel_port(dev, flow_list, error);
	if (ret)
		goto l_end;

	ret = sxe2_flowlist_add_proto_type(dev, flow_list, error);
	if (ret)
		goto l_end;

	ret = sxe2_flow_check_function(dev, flow_list, error);
	if (ret)
		goto l_end;

	ret = sxe2_flow_meta_proc(dev, attr, flow_list, error);
	if (ret)
		goto l_end;

	ret = sxe2_flow_src_split_proc(dev, flow_list, error);
	if (ret)
		goto l_end;

	ret = sxe2_flow_adjust_action(dev, flow_list, error);
	if (ret)
		goto l_end;

	ret = sxe2_flow_tunnel_split_proc(dev, flow_list, error);
	if (ret)
		goto l_end;
l_end:
	return ret;
}

static int32_t sxe2_flow_validate_with_flow(struct rte_eth_dev *dev,
					struct rte_flow *flow_list,
					const struct rte_flow_attr *attr,
					const struct rte_flow_item pattern[],
					const struct rte_flow_action actions[],
					struct rte_flow_error *error)
{
	int32_t ret = 0;
	struct sxe2_flow *flow = NULL;

	ret = sxe2_check_para(attr, pattern, actions, error);
	if (ret != 0)
		goto l_end;

	ret = sxe2_flow_valid_attr(attr, error);
	if (ret != 0)
		goto l_end;

	flow = rte_zmalloc("sxe2_flow", sizeof(*flow), 0);
	if (!flow) {
		rte_flow_error_set(error, ENOMEM,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Failed to alloc memory for flow rule");
		PMD_LOG_ERR(DRV, "Failed to alloc memory for flow rule.");
		ret = -ENOMEM;
		goto l_end;
	}

	TAILQ_INSERT_TAIL(&flow_list->sxe2_flow_list, flow, next);
	flow->create_err = -1;

	ret = sxe2_flow_parse_pattern(dev, pattern, error, flow);
	if (ret != 0)
		goto l_end;

	ret = sxe2_flow_parse_engine(dev, attr, actions, error, flow);
	if (ret != 0)
		goto l_end;

	ret = sxe2_flow_parse_action(dev, actions, error, flow);
	if (ret != 0)
		goto l_end;

	ret = sxe2_flow_post_proc(dev, attr, flow_list, error);
	if (ret != 0)
		goto l_end;

	ret = sxe2_flow_check_flow_list_duplicate(dev, flow_list);
	if (ret != 0) {
		rte_flow_error_set(error, EEXIST, RTE_FLOW_ERROR_TYPE_ITEM,
				NULL, "Duplicate flow.");
		PMD_LOG_ERR(DRV, "Duplicate flow.");
		goto l_end;
	}
l_end:
	return ret;
}

static const char *sxe2_flow_convert_ret_to_flow_msg(int32_t ret)
{
	const char *msg = NULL;
	if (ret > 0)
		ret = -ret;
	switch (ret) {
	case -ENOMEM:
		msg = "no memory";
		break;
	case -ENOTSUP:
		msg = "not support";
		break;
	case -EEXIST:
		msg = "rule already exist";
		break;
	case -ETIMEDOUT:
		msg = "timeout";
		break;
	case -EINVAL:
		msg = "invalid parameter";
		break;
	case -ENOSPC:
		msg = "no space";
		break;
	case -ENOENT:
		msg = "no such rule";
		break;
	default:
		msg = "unknown error";
		break;
	}
	return msg;
}

static int32_t sxe2_flow_rte_list_free(struct sxe2_adapter *adapter,
				   struct rte_flow **flow_ptr,
				   struct rte_flow_error *error)
{
	int32_t ret = 0;
	int32_t ret1 = 0;
	struct rte_flow *flow = *flow_ptr;
	struct rte_flow *flow_temp = NULL;
	struct sxe2_flow *hw_flow = NULL;
	struct sxe2_flow *hw_flow_temp = NULL;
	struct sxe2_fnav_cid_mgr *mgr = NULL;
	rte_spinlock_lock(&adapter->flow_ctxt.flow_list_lock);
	TAILQ_FOREACH(flow_temp, &adapter->flow_ctxt.rte_flow_list, next) {
		if (flow_temp == flow)
			TAILQ_REMOVE(&adapter->flow_ctxt.rte_flow_list, flow, next);
	}

	TAILQ_FOREACH_SAFE(hw_flow, &flow->sxe2_flow_list, next, hw_flow_temp) {
		if (hw_flow->create_err == 0) {
			if (sxe2_test_bit(SXE2_FLOW_ACTION_COUNT, hw_flow->action.act_types)) {
				ret = sxe2_flow_query_mgr(adapter, hw_flow, &mgr, error);
				if (ret) {
					PMD_LOG_ERR(DRV,
						"Failed to query flow count, flow id: %u, ret: %d.",
						hw_flow->flow_id, ret);
					rte_flow_error_set(error, EIO,
						RTE_FLOW_ERROR_TYPE_ACTION, NULL,
						"Failed to query flow count");
					ret1 = ret;
				}
			}

			ret = sxe2_drv_flow_filter_del(adapter, hw_flow);
			if (ret) {
				PMD_LOG_ERR(DRV,
					"Failed to delete flow filter, ret: %d:%s",
					ret, sxe2_flow_convert_ret_to_flow_msg(ret));
				rte_flow_error_set(error, EIO,
					RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					"Failed to delete flow filter");
				ret1 = ret;
			}

			if (sxe2_test_bit(SXE2_FLOW_ACTION_COUNT, hw_flow->action.act_types)) {
				ret = sxe2_flow_free_mgr(adapter, hw_flow,
							 &mgr, error);
				if (ret)
					ret1 = ret;
			}
		}

		TAILQ_REMOVE(&flow->sxe2_flow_list, hw_flow, next);
		rte_free(hw_flow);
	}
	rte_free(flow);
	*flow_ptr = NULL;
	rte_spinlock_unlock(&adapter->flow_ctxt.flow_list_lock);

	return ret1;
}

static int32_t sxe2_flow_validate(struct rte_eth_dev *dev,
			      const struct rte_flow_attr *attr,
			      const struct rte_flow_item pattern[],
			      const struct rte_flow_action actions[],
			      struct rte_flow_error *error)
{
	int32_t ret = 0;
	struct rte_flow *flow_list = NULL;
	struct sxe2_flow *hw_flow = NULL;
	struct sxe2_flow *hw_flow_temp = NULL;
	flow_list = rte_zmalloc("rte_flow_va", sizeof(*flow_list), 0);
	if (!flow_list) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to alloc memory for flow rule");
		PMD_LOG_ERR(DRV, "Failed to alloc memory for flow rule.");
		ret = -ENOMEM;
		goto l_end;
	}
	TAILQ_INIT(&flow_list->sxe2_flow_list);

	ret = sxe2_flow_validate_with_flow(dev, flow_list, attr, pattern, actions, error);
	if (ret != 0)
		goto l_free;
l_free:

	TAILQ_FOREACH_SAFE(hw_flow, &flow_list->sxe2_flow_list, next, hw_flow_temp) {
		TAILQ_REMOVE(&flow_list->sxe2_flow_list, hw_flow, next);
		rte_free(hw_flow);
	}
	rte_free(flow_list);
l_end:
	return ret;
}

static int32_t sxe2_flow_isolate(struct rte_eth_dev *dev,
				 int32_t enable,
				 struct rte_flow_error *error)
{
	int32_t ret = 0;
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);

	if (dev->data->dev_started) {
		rte_flow_error_set(error, EBUSY,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "port must be stopped first");
		ret = -EBUSY;
		goto l_end;
	}

	if (adapter->is_dev_repr) {
		rte_flow_error_set(error, ENOTSUP,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			"representor dev cannot change isolated mode ");
		ret = -EINVAL;
		goto l_end;
	}

	if (enable == adapter->flow_isolated)
		goto l_end;

	if (adapter->dev_type == SXE2_DEV_T_VF &&
		adapter->switchdev_info.is_switchdev) {
		rte_flow_error_set(error, ENOTSUP,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			"isolated mode cannot be change when port in switch dev mode");
		ret = -EINVAL;
		goto l_end;
	}

	rte_spinlock_lock(&adapter->flow_ctxt.flow_list_lock);
	if (!TAILQ_EMPTY(&adapter->flow_ctxt.rte_flow_list))
		PMD_DEV_LOG_WARN(adapter, DRV,
			"The configured flow item may not take effect.");
	rte_spinlock_unlock(&adapter->flow_ctxt.flow_list_lock);

	adapter->flow_isolated = !!enable;

	ret = sxe2_l2_rule_update(adapter);
	if (ret != 0)
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to update l2 rule");

	ret = sxe2_switchdev_rule_update(adapter);
	if (ret != 0)
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to update switchdev rule");

l_end:
	if (ret == 0)
		adapter->flow_isolate_cfg = !!enable;
	return ret;
}

static struct rte_flow *sxe2_flow_create(struct rte_eth_dev *dev,
					 const struct rte_flow_attr *attr,
					 const struct rte_flow_item pattern[],
					 const struct rte_flow_action action[],
					 struct rte_flow_error *error)
{
	int32_t ret = 0;
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct rte_flow *flow_list = NULL;
	struct sxe2_flow *flow = NULL;

	flow_list = rte_zmalloc("sxe2_flow_create", sizeof(*flow_list), 0);
	if (!flow_list) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to alloc memory for flow rule");
		PMD_LOG_ERR(DRV, "Failed to alloc memory for flow rule.");
		ret = -ENOMEM;
		goto l_end;
	}
	TAILQ_INIT(&flow_list->sxe2_flow_list);

	ret = sxe2_flow_validate_with_flow(dev, flow_list, attr, pattern, action, error);
	if (ret != 0)
		goto l_free_flow;

	TAILQ_FOREACH(flow, &flow_list->sxe2_flow_list, next) {
		ret = sxe2_fnav_get_filter_cid(adapter, flow);
		if (ret != 0) {
			PMD_LOG_ERR(DRV, "fnav get stats id failed, ret:%d", ret);
			rte_flow_error_set(error, EIO,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to add fnav rule:alloc cid failed.");
			goto l_free_flow;
		}
		ret = sxe2_drv_flow_filter_add(adapter, flow);
		if (ret) {
			rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				   "Failed to add flow filter to hw.");
			PMD_LOG_ERR(DRV, "Failed to add flow filter to hw.ret:%d:%s",
				ret, sxe2_flow_convert_ret_to_flow_msg(ret));
			goto l_free_flow;
		}
	}

	TAILQ_INSERT_TAIL(&adapter->flow_ctxt.rte_flow_list, flow_list, next);
	goto l_end;
l_free_flow:
	(void)sxe2_flow_rte_list_free(adapter, &flow_list, error);
l_end:
	return flow_list;
}

static int32_t sxe2_flow_destroy(struct rte_eth_dev *dev,
			     struct rte_flow *flow,
			     struct rte_flow_error *error)
{
	int32_t ret = 0;
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	ret = sxe2_flow_rte_list_free(adapter, &flow, error);
	if (ret)
		PMD_LOG_ERR(DRV, "Failed to destroy flow.ret:%d.", ret);
	return ret;
}

static int32_t sxe2_flow_flush(struct rte_eth_dev *dev, struct rte_flow_error *error)
{
	int32_t ret = 0;
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct rte_flow *flow = NULL;
	struct rte_flow *tmp_flow = NULL;
	struct rte_flow_list_t *flow_list = &adapter->flow_ctxt.rte_flow_list;
	TAILQ_FOREACH_SAFE(flow, flow_list, next, tmp_flow) {
		ret = sxe2_flow_destroy(dev, flow, error);
		if (ret) {
			PMD_LOG_ERR(DRV, "Failed to flush flows.ret:%d.", ret);

			if (ret != -EAGAIN)
				ret = -EINVAL;
			goto l_end;
		}
	}
l_end:
	return ret;
}

int32_t sxe2_fnav_get_filter_cid(struct sxe2_adapter *adapter, struct sxe2_flow *flow)
{
	int32_t ret = 0;
	struct sxe2_fnav_cid_mgr_list_t *cid_mgr_list =
				&adapter->flow_ctxt.hw_res.fnav_cid_mgr_list;
	uint32_t stat_index;
	uint32_t user_id;
	uint32_t driver_id;
	struct sxe2_fnav_cid_mgr *temp = NULL;
	struct sxe2_fnav_cid_mgr *mgr = NULL;

	if (sxe2_test_bit(SXE2_FLOW_ACTION_COUNT, flow->action.act_types)) {
		user_id = flow->action.count.user_id;
		driver_id = flow->action.count.driver_id;

		TAILQ_FOREACH(temp, cid_mgr_list, next) {
			if (temp->user_id == user_id &&
				temp->driver_id == driver_id) {
				mgr = temp;
				break;
			}
		}
		if (mgr == NULL) {
			mgr = rte_zmalloc("sxe2_fnav_cid_mgr",
				sizeof(struct sxe2_fnav_cid_mgr), 0);
			if (!mgr) {
				PMD_LOG_ERR(DRV,
					"Failed to alloc sxe2vf_fnav_cid_mgr memory.");
				ret = -ENOMEM;
				goto l_end;
			}

			ret = sxe2_drv_flow_fnav_get_stat_id(adapter, &stat_index);
			if (ret) {
				PMD_LOG_ERR(DRV, "Failed to alloc fw count id.");
				rte_free(mgr);
				goto l_end;
			}

			TAILQ_INSERT_TAIL(cid_mgr_list, mgr, next);
			mgr->user_id = user_id;
			mgr->driver_id = driver_id;
			mgr->stat_index = stat_index;
			mgr->count_type = adapter->flow_ctxt.hw_res.count_type;
		}
		flow->action.count.stat_index = mgr->stat_index;
		flow->action.count.stat_ctrl = mgr->count_type;
	}

l_end:
	return ret;
}

int32_t sxe2_flow_free_mgr(struct sxe2_adapter *adapter,
		       struct sxe2_flow *flow,
		       struct sxe2_fnav_cid_mgr **mgr_ptr,
		       struct rte_flow_error *error)
{
	int32_t ret = 0;
	struct sxe2_fnav_cid_mgr_list_t *cid_mgr_list =
				&adapter->flow_ctxt.hw_res.fnav_cid_mgr_list;
	struct sxe2_fnav_cid_mgr *mgr = *mgr_ptr;
	uint32_t user_id = flow->action.count.user_id;
	if (user_id == 0) {
		TAILQ_REMOVE(cid_mgr_list, mgr, next);
		ret = sxe2_drv_flow_fnav_free_stat(adapter, mgr->stat_index);
		if (ret) {
			rte_flow_error_set(error, EIO,
				RTE_FLOW_ERROR_TYPE_ACTION, NULL,
				"Failed to free flow count.");
			PMD_LOG_ERR(DRV,
				"Failed to free flow count, flow id: %u, ret: %d.",
				flow->flow_id, ret);
		}
		rte_free(mgr);
		*mgr_ptr = NULL;
	}
	return ret;
}

int32_t sxe2_flow_query_mgr(struct sxe2_adapter *adapter,
			struct sxe2_flow *flow,
			struct sxe2_fnav_cid_mgr **mgr_ptr,
			struct rte_flow_error *error)
{
	int32_t ret = 0;
	struct sxe2_fnav_cid_mgr_list_t *cid_mgr_list =
				&adapter->flow_ctxt.hw_res.fnav_cid_mgr_list;
	struct sxe2_fnav_cid_mgr *temp = NULL;
	struct sxe2_fnav_cid_mgr *mgr = NULL;
	uint32_t user_id = flow->action.count.user_id;
	uint32_t driver_id = flow->action.count.driver_id;

	TAILQ_FOREACH(temp, cid_mgr_list, next) {
		if (temp->user_id == user_id &&
			temp->driver_id == driver_id) {
			mgr = temp;
			break;
		}
	}
	if (!mgr) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
			"fnav flow query count invalid user_id or driver_id.");
		PMD_LOG_ERR(DRV,
			"fnav flow query count invalid user_id or driver_id.");
		ret = -EINVAL;
		goto l_end;
	}
	ret = sxe2_drv_flow_fnav_query_stat(adapter, mgr);
	if (ret) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM, NULL,
			"Failed to query flow count.");
		PMD_LOG_ERR(DRV,
			"Failed to query flow count, flow id: %u, ret: %d.",
			flow->flow_id, ret);
		goto l_end;
	}
	*mgr_ptr = mgr;
l_end:
	return ret;
}

static int32_t sxe2_flow_query_count(struct sxe2_adapter *adapter,
				 struct sxe2_flow *flow,
				 struct rte_flow_query_count *count,
				 struct rte_flow_error *error)
{
	int32_t ret = 0;
	struct sxe2_fnav_cid_mgr *mgr = NULL;
	switch (flow->action.count.stat_ctrl) {
	case SXE2_FNAV_STAT_ENA_NONE:
		count->hits_set = 0;
		count->bytes_set = 0;
		break;
	case SXE2_FNAV_STAT_ENA_PKTS:
		count->hits_set = 1;
		count->bytes_set = 0;
		break;
	case SXE2_FNAV_STAT_ENA_BYTES:
		count->hits_set = 0;
		count->bytes_set = 1;
		break;
	case SXE2_FNAV_STAT_ENA_ALL:
		count->hits_set = 1;
		count->bytes_set = 1;
		break;
	default:
		ret = -EINVAL;
		goto l_end;
	}
	if (!sxe2_test_bit(SXE2_FLOW_ACTION_COUNT, flow->action.act_types)) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
			"this flow don't have count action.");
		PMD_LOG_ERR(DRV, "this flow don't have count action.");
		ret = -EINVAL;
		goto l_end;
	}
	ret = sxe2_flow_query_mgr(adapter, flow, &mgr, error);
	if (ret) {
		PMD_LOG_ERR(DRV,
			"Failed to query flow count, flow id: %u, ret: %d.",
			flow->flow_id, ret);
		goto l_end;
	}
	count->hits = mgr->hits;
	count->bytes = mgr->bytes;
	if (count->reset) {
		mgr->hits = 0;
		mgr->bytes = 0;
	}
l_end:
	return ret;
}

static int32_t sxe2_flow_query(struct rte_eth_dev *dev,
			   struct rte_flow *flow_list,
			   const struct rte_flow_action *actions,
			   void *data,
			   struct rte_flow_error *error)
{
	int32_t ret = 0;
	struct rte_flow_query_count *count = data;
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_flow *flow = NULL;

	if (!flow_list) {
		ret = -EINVAL;
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_HANDLE,
				NULL, "Invalid flow");
		PMD_LOG_ERR(DRV, "Invalid flow to query flow.");
		goto l_end;
	}

	rte_spinlock_lock(&adapter->flow_ctxt.flow_list_lock);
	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_COUNT:
			flow = TAILQ_FIRST(&flow_list->sxe2_flow_list);
			ret = sxe2_flow_query_count(adapter, flow, count, error);
			if (ret) {
				PMD_LOG_ERR(DRV,
					"Failed to query flow count, flow id: %u, ret: %d.",
					flow->flow_id, ret);
				goto l_end_unlock;
			}
			break;
		default:
			rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION,
					actions,
					"action not supported");
			PMD_LOG_ERR(DRV,
					"Failed to query flow action type:%d.",
					actions->type);
			ret = -ENOTSUP;
			goto l_end_unlock;
		}
	}

l_end_unlock:
	rte_spinlock_unlock(&adapter->flow_ctxt.flow_list_lock);

l_end:
	return ret;
}

const struct rte_flow_ops sxe2_flow_ops = {
	.validate = sxe2_flow_validate,
	.create = sxe2_flow_create,
	.destroy = sxe2_flow_destroy,
	.flush = sxe2_flow_flush,
	.query = sxe2_flow_query,
	.isolate = sxe2_flow_isolate,
};

int32_t sxe2_flow_ops_get(struct rte_eth_dev *dev, const struct rte_flow_ops **ops)
{
	int32_t ret = 0;

	if (dev == NULL) {
		ret = -EINVAL;
		goto l_end;
	}

	*ops = &sxe2_flow_ops;

l_end:
	return ret;
}

int32_t sxe2_flow_init(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	int32_t ret = 0;
	TAILQ_INIT(&adapter->flow_ctxt.rte_flow_list);
	TAILQ_INIT(&adapter->flow_ctxt.hw_res.fnav_cid_mgr_list);
	if (adapter->devargs.fnav_stat_type)
		adapter->flow_ctxt.hw_res.count_type =
			adapter->devargs.fnav_stat_type;
	else
		adapter->flow_ctxt.hw_res.count_type = SXE2_FNAV_STAT_ENA_ALL;

	adapter->flow_ctxt.fnav_inited = 1;
	rte_spinlock_init(&adapter->flow_ctxt.flow_list_lock);

	ret = sxe2_flow_init_udp_tunnel_port(dev);
	if (ret)
		PMD_LOG_ERR(DRV, "Failed to init udp tunnel port, ret: %d.", ret);

	return ret;
}

int32_t sxe2_flow_uninit(struct rte_eth_dev *dev)
{
	int32_t ret = 0;
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct rte_flow_error error;
	struct sxe2_fnav_cid_mgr *mgr = NULL;
	struct sxe2_fnav_cid_mgr *temp = NULL;
	struct sxe2_fnav_cid_mgr_list_t *cid_mgr_list =
						&adapter->flow_ctxt.hw_res.fnav_cid_mgr_list;

	ret = sxe2_flow_flush(dev, &error);
	if (ret)
		PMD_LOG_ERR(DRV, "Failed to flush flow, ret: %d.", ret);

	TAILQ_FOREACH_SAFE(mgr, cid_mgr_list, next, temp) {
		TAILQ_REMOVE(cid_mgr_list, mgr, next);
		ret = sxe2_drv_flow_fnav_free_stat(adapter, mgr->stat_index);
		if (ret)
			PMD_LOG_ERR(DRV,
				"Failed to free fnav stat id, ret: %d.", ret);
		rte_free(mgr);
	}
	return ret;
}
