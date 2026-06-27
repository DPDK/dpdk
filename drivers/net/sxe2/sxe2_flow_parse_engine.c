/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#include "sxe2_flow_parse_engine.h"
#include "sxe2_ethdev.h"
#include "sxe2_flow_public.h"
#include "sxe2_flow_parse_action.h"
#include "sxe2_common_log.h"

static int32_t sxe2_flow_parse_engine_chk(struct sxe2_flow *flow,
		struct rte_flow_error *error)
{
	int32_t ret = 0;

	if (flow->engine_type == SXE2_FLOW_ENGINE_FNAV) {
		if (flow->has_mask) {
			ret = -EINVAL;
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM_MASK, NULL,
				"FNAV flow doesn't support mask");
			PMD_LOG_ERR(DRV, "FNAV flow doesn't support mask");
			goto l_end;
		}
		if (sxe2_test_bit(SXE2_FLOW_FLD_ID_S_VID,
				flow->pattern_outer.map_spec) &&
			sxe2_test_bit(SXE2_FLOW_FLD_ID_C_VID,
				flow->pattern_outer.map_spec)) {
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				"Can't set double vid,please use tci.");
			PMD_LOG_ERR(DRV,
				"Can't set double vid,please use tci.");
			ret = -EINVAL;
			goto l_end;
		}
	}
l_end:
	return ret;
}

int32_t sxe2_flow_parse_engine(struct rte_eth_dev *dev,
		const struct rte_flow_attr *attr,
		const struct rte_flow_action actions[],
		struct rte_flow_error *error,
		struct sxe2_flow *flow)
{
	int32_t ret = 0;
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	const struct rte_flow_action *action;

	if (flow->has_mask == 0 && flow->has_spec == 0) {
		flow->engine_type = SXE2_FLOW_ENGINE_RSS;
		goto l_end;
	}

	if (attr->group == 1) {
		flow->engine_type = SXE2_FLOW_ENGINE_SWITCH;
		goto l_end;
	}
	if (attr->group == 2) {
		flow->engine_type = SXE2_FLOW_ENGINE_ACL;
		goto l_end;
	}
	if (attr->group == 3) {
		flow->engine_type = SXE2_FLOW_ENGINE_FNAV;
		goto l_end;
	}

	if (adapter->is_dev_repr) {
		flow->engine_type = SXE2_FLOW_ENGINE_SWITCH;
		goto l_end;
	}

	if (adapter->switchdev_info.is_switchdev &&
	    adapter->dev_type == SXE2_DEV_T_VF) {
		flow->engine_type = SXE2_FLOW_ENGINE_FNAV;
		goto l_end;
	}

	for (action = actions; action->type != RTE_FLOW_ACTION_TYPE_END; action++) {
		switch (action->type) {
		case RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT:
		case RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR:
		case RTE_FLOW_ACTION_TYPE_PORT_ID:
			flow->engine_type = SXE2_FLOW_ENGINE_SWITCH;
			goto l_end;
		default:
			break;
		}
	}

	if (adapter->switchdev_info.is_switchdev) {
		flow->engine_type = SXE2_FLOW_ENGINE_FNAV;
		goto l_end;
	}

	if (adapter->flow_isolated)
		flow->engine_type = SXE2_FLOW_ENGINE_SWITCH;
	else
		flow->engine_type = SXE2_FLOW_ENGINE_FNAV;

l_end:
	ret = sxe2_flow_parse_engine_chk(flow, error);
	return ret;
}
