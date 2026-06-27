/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#include "sxe2_flow_parse_action.h"
#include "sxe2_common_log.h"
#include "sxe2_flow_public.h"
#include "sxe2_ethdev.h"
#include "sxe2_vsi.h"


static int32_t sxe2_flow_check_rss_action_attr(const struct rte_flow_action_rss *rss,
					   struct rte_flow_error *error)
{
	int32_t ret = ENOTSUP;
	switch (rss->func) {
	case RTE_ETH_HASH_FUNCTION_DEFAULT:
	case RTE_ETH_HASH_FUNCTION_TOEPLITZ:
	case RTE_ETH_HASH_FUNCTION_SYMMETRIC_TOEPLITZ:
	case RTE_ETH_HASH_FUNCTION_SIMPLE_XOR:
		break;
	default:
		PMD_LOG_ERR(DRV, "RSS hash function[%d] not support.", rss->func);
		ret = -EINVAL;
		goto l_end;
	}

	if (rss->level > 2)
		rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION, NULL,
			"RSS  level is could not be greater than 2");
	if (rss->key_len)
		rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION, NULL,
			"a nonzero RSS key_len is not supported");
	if (rss->queue_num)
		rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION, NULL,
			"a non-NULL RSS queue is not supported");
	ret = 0;
l_end:
	return ret;
}


static int32_t sxe2_flow_set_rss_action_func(enum rte_eth_hash_function rss_func,
					 uint64_t rss_type,
					 struct sxe2_flow *flow,
					 struct rte_flow_error *error)
{
	int32_t ret = 0;
	if (rss_func == RTE_ETH_HASH_FUNCTION_SIMPLE_XOR) {
		if (flow->has_hdr) {
			ret = -EINVAL;
			rte_flow_error_set(error, -EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, NULL,
				"Failed to cfg Simple XOR hash with not empty pattern");
			PMD_LOG_ERR(DRV, "Failed to cfg Simple XOR hash with not empty pattern.");
			goto l_end;
		}
	} else {
		if (!flow->has_hdr) {
			ret = -EINVAL;
			rte_flow_error_set(error, -EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, NULL,
				"Failed to cfg Simple hash with empty pattern");
			PMD_LOG_ERR(DRV, "Failed to cfg Simple hash with empty pattern.");
			goto l_end;
		}
	}

	if (rss_func == RTE_ETH_HASH_FUNCTION_SYMMETRIC_TOEPLITZ) {
		if (rss_type & (RTE_ETH_RSS_L3_SRC_ONLY |
					RTE_ETH_RSS_L3_DST_ONLY |
					RTE_ETH_RSS_L4_SRC_ONLY |
					RTE_ETH_RSS_L4_DST_ONLY)) {
			ret = -EINVAL;
			rte_flow_error_set(error, -EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, NULL,
				"Failed to cfg symm func rss_type l3/l4 only.");
			PMD_LOG_ERR(DRV, "Failed to cfg symm func rss_type l3/l4 only.");
			goto l_end;
		}

		if (!(rss_type & (RTE_ETH_RSS_IPV4 | RTE_ETH_RSS_IPV6 |
					RTE_ETH_RSS_FRAG_IPV4 | RTE_ETH_RSS_FRAG_IPV6 |
					RTE_ETH_RSS_NONFRAG_IPV4_UDP |
					RTE_ETH_RSS_NONFRAG_IPV6_UDP |
					RTE_ETH_RSS_NONFRAG_IPV4_TCP |
					RTE_ETH_RSS_NONFRAG_IPV6_TCP |
					RTE_ETH_RSS_NONFRAG_IPV4_SCTP |
					RTE_ETH_RSS_NONFRAG_IPV6_SCTP))) {
			ret = -EINVAL;
			rte_flow_error_set(error, -EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, NULL,
				"Failed to cfg symm func unsupported rss_type.");
			PMD_LOG_ERR(DRV, "Failed to cfg symm func unsupported rss_type.");
			goto l_end;
		}
		flow->action.rss.func = SXE2_RSS_HASH_FUNC_SYM_TOEPLITZ;
	}
	if (rss_func == RTE_ETH_HASH_FUNCTION_SIMPLE_XOR)
		flow->action.rss.func = SXE2_RSS_HASH_FUNC_XOR;
	if (rss_func == RTE_ETH_HASH_FUNCTION_DEFAULT)
		flow->action.rss.func = SXE2_RSS_HASH_FUNC_TOEPLITZ;
	if (rss_func == RTE_ETH_HASH_FUNCTION_TOEPLITZ)
		flow->action.rss.func = SXE2_RSS_HASH_FUNC_TOEPLITZ;
l_end:
	return ret;
}


static uint64_t sxe2_hash_invalid_comb[] = {
	RTE_ETH_RSS_IPV4 | RTE_ETH_RSS_NONFRAG_IPV4_UDP,
	RTE_ETH_RSS_IPV4 | RTE_ETH_RSS_NONFRAG_IPV4_TCP,
	RTE_ETH_RSS_IPV4 | RTE_ETH_RSS_NONFRAG_IPV4_SCTP,
	RTE_ETH_RSS_IPV6 | RTE_ETH_RSS_NONFRAG_IPV6_UDP,
	RTE_ETH_RSS_IPV6 | RTE_ETH_RSS_NONFRAG_IPV6_TCP,
	RTE_ETH_RSS_IPV6 | RTE_ETH_RSS_NONFRAG_IPV6_SCTP,
	RTE_ETH_RSS_FRAG_IPV4 | RTE_ETH_RSS_NONFRAG_IPV4_OTHER,
	RTE_ETH_RSS_FRAG_IPV6 | RTE_ETH_RSS_NONFRAG_IPV6_OTHER,
	RTE_ETH_RSS_L3_PRE32 | RTE_ETH_RSS_L3_PRE48 | RTE_ETH_RSS_L3_PRE64,
};

struct sxe2_rss_attr_type {
	uint64_t attr;
	uint64_t type;
};

static struct sxe2_rss_attr_type sxe2_rss_attr_valid_type[] = {
	{RTE_ETH_RSS_L2_SRC_ONLY | RTE_ETH_RSS_L2_DST_ONLY, RTE_ETH_RSS_ETH},
	{RTE_ETH_RSS_L3_SRC_ONLY | RTE_ETH_RSS_L3_DST_ONLY, SXE2_VALID_RSS_L3},
	{RTE_ETH_RSS_L4_SRC_ONLY | RTE_ETH_RSS_L4_DST_ONLY, SXE2_VALID_RSS_L4},

	{RTE_ETH_RSS_L3_PRE32, SXE2_VALID_RSS_IPV6},
	{RTE_ETH_RSS_L3_PRE48, SXE2_VALID_RSS_IPV6},
	{RTE_ETH_RSS_L3_PRE64, SXE2_VALID_RSS_IPV6},
	{SXE2_INVALID_RSS_ATTR, 0}
};


static void sxe2_flow_action_pre(struct sxe2_flow *flow)
{
	flow->action.vsi.vsi_index = UINT16_MAX;
	flow->action.vsi_list.vsi_cnt = 0;
	sxe2_bitmap_zero(flow->action.vsi_list.vsi_list_map, SXE2_VSI_MAX);
}

static void sxe2_flow_action_post(struct sxe2_flow *flow, uint8_t action_num[])
{
	if (sxe2_test_bit(SXE2_FLOW_ACTION_TO_VSI, flow->action.act_types))
		action_num[SXE2_FLOW_ACTION_TO_VSI] = 1;

	if (sxe2_test_bit(SXE2_FLOW_ACTION_TO_VSI_LIST, flow->action.act_types))
		action_num[SXE2_FLOW_ACTION_TO_VSI_LIST] = 1;
}


static void sxe2_flow_action_vsi_merge(struct sxe2_flow *flow, uint16_t add_vsi_id)
{
	if (flow->action.vsi_list.vsi_cnt == 0) {
		if (flow->action.vsi.vsi_index == UINT16_MAX) {
			flow->action.vsi.vsi_index = add_vsi_id;
			sxe2_set_bit(SXE2_FLOW_ACTION_TO_VSI, flow->action.act_types);
			goto l_end;
		}

		if (flow->action.vsi.vsi_index == add_vsi_id)
			goto l_end;

		sxe2_set_bit(flow->action.vsi.vsi_index, flow->action.vsi_list.vsi_list_map);
		sxe2_set_bit(add_vsi_id, flow->action.vsi_list.vsi_list_map);
		flow->action.vsi_list.vsi_cnt = 2;
		flow->action.vsi.vsi_index = UINT16_MAX;
		sxe2_clear_bit(SXE2_FLOW_ACTION_TO_VSI, flow->action.act_types);
		sxe2_set_bit(SXE2_FLOW_ACTION_TO_VSI_LIST, flow->action.act_types);
	}

	if (sxe2_test_bit(add_vsi_id, flow->action.vsi_list.vsi_list_map))
		goto l_end;

	sxe2_set_bit(add_vsi_id, flow->action.vsi_list.vsi_list_map);
	flow->action.vsi_list.vsi_cnt++;

l_end:
	return;
}


static int32_t sxe2_flow_vsi_get_ethdev(struct rte_eth_dev *dev,
				uint16_t dev_port_id, uint16_t *vsi_index)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct rte_eth_dev *dst_dev;
	struct sxe2_adapter *dst_adapter;
	int32_t ret = 0;

	dst_dev = &rte_eth_devices[dev_port_id];
	if (!dst_dev->data) {
		ret = -EINVAL;
		goto l_end;
	}

	if (!sxe2_ethdev_check(dst_dev)) {
		PMD_DEV_LOG_ERR(adapter, DRV, "dst dev is not sxe2 ethdev.");
		ret = -EINVAL;
		goto l_end;
	}

	dst_adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dst_dev);
	if (!dst_adapter) {
		PMD_DEV_LOG_ERR(adapter, DRV, "dst dev adapter is null.");
		ret = -EINVAL;
		goto l_end;
	}

	if (adapter->dev_info.pci.serial_number != dst_adapter->dev_info.pci.serial_number) {
		PMD_DEV_LOG_ERR(adapter, DRV, "dst dev sn is miss match current dev.");
		ret = -EINVAL;
		goto l_end;
	}

	if (adapter->dev_type != SXE2_DEV_T_PF_BOND) {
		if (adapter->pf_idx != dst_adapter->pf_idx) {
			PMD_DEV_LOG_ERR(adapter, DRV, "dst dev pf id is miss match current dev.");
			ret = -EINVAL;
			goto l_end;
		}
	}

	if (dst_adapter->is_dev_repr) {
		if (dst_adapter->repr_priv_data == NULL) {
			PMD_DEV_LOG_ERR(adapter, DRV, "dst dev repr data is null.");
			ret = -EINVAL;
			goto l_end;
		}
		*vsi_index = dst_adapter->repr_priv_data->repr_vf_vsi_id;
	} else {
		*vsi_index = dst_adapter->vsi_ctxt.dpdk_vsi_id;
	}

l_end:
	return ret;
}
static int32_t sxe2_flow_check_rss_action_type_with_pattern(struct sxe2_flow_pattern *pattern,
							uint64_t rss_type)
{
	uint64_t rss_type_allow = pattern->rss_type_allow;
	int32_t ret = -EINVAL;

	if ((rss_type & rss_type_allow) != rss_type)
		goto l_end;

	if (sxe2_test_bit(SXE2_FLOW_HDR_VLAN, pattern->hdrs) &&
		!sxe2_test_bit(SXE2_FLOW_HDR_QINQ, pattern->hdrs)) {
		if ((rss_type & RTE_ETH_RSS_C_VLAN) != 0 &&
			(rss_type & RTE_ETH_RSS_S_VLAN) == 0)
			goto l_end;
	}
	ret = 0;
l_end:
	return ret;
}

static int32_t sxe2_flow_check_rss_action_type_valid(uint64_t rss_type)
{
	struct sxe2_rss_attr_type *attr_type;
	uint32_t i;
	int32_t ret = -EINVAL;

	for (i = 0; i < RTE_DIM(sxe2_hash_invalid_comb); i++) {
		if (rte_popcount64(rss_type & sxe2_hash_invalid_comb[i]) > 1) {
			PMD_LOG_ERR(DRV, "Error rss_type invalid comb[%d].", i);
			goto l_end;
		}
	}

	for (i = 0; i < RTE_DIM(sxe2_rss_attr_valid_type); i++) {
		attr_type = &sxe2_rss_attr_valid_type[i];
		if ((attr_type->attr & rss_type) &&
				!(attr_type->type & rss_type)) {
			PMD_LOG_ERR(DRV, "Rss_type valid_comb[%d] check error.", i);
			goto l_end;
		}
	}

	ret = 0;
l_end:
	return ret;
}


static void sxe2_flow_set_rss_action_type_l234(BITMAP_TYPE *hdr,
					       BITMAP_TYPE *fld,
					       uint64_t rss_type)
{
	if (rss_type & RTE_ETH_RSS_ETH) {
		if (rss_type & RTE_ETH_RSS_L2_SRC_ONLY) {
			sxe2_set_bit(SXE2_FLOW_FLD_ID_ETH_SA, fld);
		} else if (rss_type & RTE_ETH_RSS_L2_DST_ONLY) {
			sxe2_set_bit(SXE2_FLOW_FLD_ID_ETH_DA, fld);
		} else {
			sxe2_set_bit(SXE2_FLOW_FLD_ID_ETH_SA, fld);
			sxe2_set_bit(SXE2_FLOW_FLD_ID_ETH_DA, fld);
		}
	}
	if (rss_type & RTE_ETH_RSS_L2_PAYLOAD) {
		sxe2_set_bit(SXE2_FLOW_FLD_ID_ETH_TYPE, fld);
		sxe2_set_bit(SXE2_FLOW_HDR_ETH_NON_IP, hdr);
	}

	if (sxe2_test_bit(SXE2_FLOW_HDR_VLAN, hdr)) {
		if (rss_type & RTE_ETH_RSS_S_VLAN)
			sxe2_set_bit(SXE2_FLOW_FLD_ID_S_TCI, fld);
	}
	if (sxe2_test_bit(SXE2_FLOW_HDR_QINQ, hdr)) {
		if (rss_type & RTE_ETH_RSS_C_VLAN)
			sxe2_set_bit(SXE2_FLOW_FLD_ID_C_TCI, fld);
	}

	if (rss_type & (RTE_ETH_RSS_IPV4 |
				RTE_ETH_RSS_NONFRAG_IPV4_OTHER |
				RTE_ETH_RSS_NONFRAG_IPV4_UDP |
				RTE_ETH_RSS_NONFRAG_IPV4_TCP |
				RTE_ETH_RSS_NONFRAG_IPV4_SCTP |
				RTE_ETH_RSS_FRAG_IPV4 |
				RTE_ETH_RSS_IPV4_CHKSUM)) {
		if (rss_type & RTE_ETH_RSS_L3_SRC_ONLY) {
			sxe2_set_bit(SXE2_FLOW_FLD_ID_IPV4_SA, fld);
		} else if (rss_type & RTE_ETH_RSS_L3_DST_ONLY) {
			sxe2_set_bit(SXE2_FLOW_FLD_ID_IPV4_DA, fld);
		} else {
			sxe2_set_bit(SXE2_FLOW_FLD_ID_IPV4_SA, fld);
			sxe2_set_bit(SXE2_FLOW_FLD_ID_IPV4_DA, fld);
		}

		if (rss_type & RTE_ETH_RSS_NONFRAG_IPV4_OTHER)
			sxe2_set_bit(SXE2_FLOW_HDR_IPV_OTHER, hdr);
		if (rss_type & RTE_ETH_RSS_FRAG_IPV4) {
			sxe2_set_bit(SXE2_FLOW_HDR_IPV_FRAG, hdr);
			sxe2_set_bit(SXE2_FLOW_FLD_ID_IPV4_ID, fld);
		}

		if (rss_type & RTE_ETH_RSS_IPV4_CHKSUM)
			sxe2_set_bit(SXE2_FLOW_FLD_ID_IPV4_CHKSUM, fld);
	}

	if (rss_type & (RTE_ETH_RSS_IPV6 |
				RTE_ETH_RSS_FRAG_IPV6 |
				RTE_ETH_RSS_NONFRAG_IPV6_OTHER |
				RTE_ETH_RSS_NONFRAG_IPV6_UDP |
				RTE_ETH_RSS_NONFRAG_IPV6_TCP |
				RTE_ETH_RSS_NONFRAG_IPV6_SCTP)) {
		if (rss_type & RTE_ETH_RSS_L3_PRE32) {
			if (rss_type & RTE_ETH_RSS_L3_SRC_ONLY) {
				sxe2_set_bit(SXE2_FLOW_FLD_ID_IPV6_PRE32_SA, fld);
			} else if (rss_type & RTE_ETH_RSS_L3_DST_ONLY) {
				sxe2_set_bit(SXE2_FLOW_FLD_ID_IPV6_PRE32_DA, fld);
			} else {
				sxe2_set_bit(SXE2_FLOW_FLD_ID_IPV6_PRE32_SA, fld);
				sxe2_set_bit(SXE2_FLOW_FLD_ID_IPV6_PRE32_DA, fld);
			}
		} else if (rss_type & RTE_ETH_RSS_L3_PRE48) {
			if (rss_type & RTE_ETH_RSS_L3_SRC_ONLY) {
				sxe2_set_bit(SXE2_FLOW_FLD_ID_IPV6_PRE48_SA, fld);
			} else if (rss_type & RTE_ETH_RSS_L3_DST_ONLY) {
				sxe2_set_bit(SXE2_FLOW_FLD_ID_IPV6_PRE48_DA, fld);
			} else {
				sxe2_set_bit(SXE2_FLOW_FLD_ID_IPV6_PRE48_SA, fld);
				sxe2_set_bit(SXE2_FLOW_FLD_ID_IPV6_PRE48_DA, fld);
			}
		} else if (rss_type & RTE_ETH_RSS_L3_PRE64) {
			if (rss_type & RTE_ETH_RSS_L3_SRC_ONLY) {
				sxe2_set_bit(SXE2_FLOW_FLD_ID_IPV6_PRE64_SA, fld);
			} else if (rss_type & RTE_ETH_RSS_L3_DST_ONLY) {
				sxe2_set_bit(SXE2_FLOW_FLD_ID_IPV6_PRE64_DA, fld);
			} else {
				sxe2_set_bit(SXE2_FLOW_FLD_ID_IPV6_PRE64_SA, fld);
				sxe2_set_bit(SXE2_FLOW_FLD_ID_IPV6_PRE64_DA, fld);
			}
		} else {
			if (rss_type & RTE_ETH_RSS_L3_SRC_ONLY) {
				sxe2_set_bit(SXE2_FLOW_FLD_ID_IPV6_SA, fld);
			} else if (rss_type & RTE_ETH_RSS_L3_DST_ONLY) {
				sxe2_set_bit(SXE2_FLOW_FLD_ID_IPV6_DA, fld);
			} else {
				sxe2_set_bit(SXE2_FLOW_FLD_ID_IPV6_SA, fld);
				sxe2_set_bit(SXE2_FLOW_FLD_ID_IPV6_DA, fld);
			}
		}
		if (rss_type & RTE_ETH_RSS_FRAG_IPV6) {
			sxe2_set_bit(SXE2_FLOW_HDR_IPV_FRAG, hdr);
			sxe2_set_bit(SXE2_FLOW_FLD_ID_IPV6_ID, fld);
		}
		if (rss_type & RTE_ETH_RSS_NONFRAG_IPV6_OTHER)
			sxe2_set_bit(SXE2_FLOW_HDR_IPV_OTHER, hdr);
	}

	if (rss_type & (RTE_ETH_RSS_NONFRAG_IPV4_TCP |
				RTE_ETH_RSS_NONFRAG_IPV6_TCP)) {
		if (rss_type & RTE_ETH_RSS_L4_SRC_ONLY) {
			sxe2_set_bit(SXE2_FLOW_FLD_ID_TCP_SRC_PORT, fld);
		} else if (rss_type & RTE_ETH_RSS_L4_DST_ONLY) {
			sxe2_set_bit(SXE2_FLOW_FLD_ID_TCP_DST_PORT, fld);
		} else {
			sxe2_set_bit(SXE2_FLOW_FLD_ID_TCP_SRC_PORT, fld);
			sxe2_set_bit(SXE2_FLOW_FLD_ID_TCP_DST_PORT, fld);
		}
		if (rss_type & RTE_ETH_RSS_L4_CHKSUM)
			sxe2_set_bit(SXE2_FLOW_FLD_ID_TCP_CHKSUM, fld);
	}

	if (rss_type & (RTE_ETH_RSS_NONFRAG_IPV4_UDP |
				RTE_ETH_RSS_NONFRAG_IPV6_UDP)) {
		if (rss_type & RTE_ETH_RSS_L4_SRC_ONLY) {
			sxe2_set_bit(SXE2_FLOW_FLD_ID_UDP_SRC_PORT, fld);
		} else if (rss_type & RTE_ETH_RSS_L4_DST_ONLY) {
			sxe2_set_bit(SXE2_FLOW_FLD_ID_UDP_DST_PORT, fld);
		} else {
			sxe2_set_bit(SXE2_FLOW_FLD_ID_UDP_SRC_PORT, fld);
			sxe2_set_bit(SXE2_FLOW_FLD_ID_UDP_DST_PORT, fld);
		}
		if (rss_type & RTE_ETH_RSS_L4_CHKSUM)
			sxe2_set_bit(SXE2_FLOW_FLD_ID_UDP_CHKSUM, fld);
	}

	if (rss_type & (RTE_ETH_RSS_NONFRAG_IPV4_SCTP |
				RTE_ETH_RSS_NONFRAG_IPV6_SCTP)) {
		if (rss_type & RTE_ETH_RSS_L4_SRC_ONLY) {
			sxe2_set_bit(SXE2_FLOW_FLD_ID_SCTP_SRC_PORT, fld);
		} else if (rss_type & RTE_ETH_RSS_L4_DST_ONLY) {
			sxe2_set_bit(SXE2_FLOW_FLD_ID_SCTP_DST_PORT, fld);
		} else {
			sxe2_set_bit(SXE2_FLOW_FLD_ID_SCTP_SRC_PORT, fld);
			sxe2_set_bit(SXE2_FLOW_FLD_ID_SCTP_DST_PORT, fld);
		}
		if (rss_type & RTE_ETH_RSS_L4_CHKSUM)
			sxe2_set_bit(SXE2_FLOW_FLD_ID_SCTP_CHKSUM, fld);
	}
}


static int32_t sxe2_flow_set_rss_action_hdr_type(struct sxe2_flow *flow,
		bool is_inner)
{
	struct sxe2_flow_action_rss *rss = &flow->action.rss;
	BITMAP_TYPE *hdr = rss->hdr_out;
	int32_t ret = 0;

	rss->hdr_type = SXE2_RSS_ANY_HEADERS;
	if (!is_inner) {
		rss->hdr_type = SXE2_RSS_OUTER_HEADERS;
		goto l_end;
	}
	if (sxe2_test_bit(SXE2_FLOW_HDR_IPV4, hdr)) {
		if (sxe2_test_bit(SXE2_FLOW_HDR_UDP, hdr)) {
			if (sxe2_test_bit(SXE2_FLOW_HDR_GRE, hdr)) {
				rss->hdr_type =
					SXE2_RSS_INNER_HEADERS_WITH_OUTER_IPV4_UDP_GRE;
			} else if (sxe2_test_bit(SXE2_FLOW_HDR_GENEVE, hdr)) {
				rss->hdr_type =
					SXE2_RSS_INNER_HEADERS_WITH_OUTER_IPV4_UDP_GENEVE;
			} else if (sxe2_test_bit(SXE2_FLOW_HDR_VXLAN, hdr)) {
				rss->hdr_type =
					SXE2_RSS_INNER_HEADERS_WITH_OUTER_IPV4_UDP_VXLAN;
			} else if (sxe2_test_bit(SXE2_FLOW_HDR_GTPU, hdr)) {
				rss->hdr_type =
					SXE2_RSS_INNER_HEADERS_WITH_OUTER_IPV4_UDP_GTPU;
			}
		} else {
			if (sxe2_test_bit(SXE2_FLOW_HDR_GRE, hdr)) {
				rss->hdr_type =
					SXE2_RSS_INNER_HEADERS_WITH_OUTER_IPV4_GRE;
			} else {
				rss->hdr_type = SXE2_RSS_INNER_HEADERS_WITH_OUTER_IPV4;
			}
		}
	} else if (sxe2_test_bit(SXE2_FLOW_HDR_IPV6, hdr)) {
		if (sxe2_test_bit(SXE2_FLOW_HDR_UDP, hdr)) {
			if (sxe2_test_bit(SXE2_FLOW_HDR_GRE, hdr)) {
				rss->hdr_type =
					SXE2_RSS_INNER_HEADERS_WITH_OUTER_IPV6_UDP_GRE;
			} else if (sxe2_test_bit(SXE2_FLOW_HDR_GENEVE, hdr)) {
				rss->hdr_type =
					SXE2_RSS_INNER_HEADERS_WITH_OUTER_IPV6_UDP_GENEVE;
			} else if (sxe2_test_bit(SXE2_FLOW_HDR_VXLAN, hdr)) {
				rss->hdr_type =
					SXE2_RSS_INNER_HEADERS_WITH_OUTER_IPV6_UDP_VXLAN;
			} else if (sxe2_test_bit(SXE2_FLOW_HDR_GTPU, hdr)) {
				rss->hdr_type =
					SXE2_RSS_INNER_HEADERS_WITH_OUTER_IPV6_UDP_GTPU;
			}
		} else {
			if (sxe2_test_bit(SXE2_FLOW_HDR_GRE, hdr)) {
				rss->hdr_type =
					SXE2_RSS_INNER_HEADERS_WITH_OUTER_IPV6_GRE;
			} else {
				rss->hdr_type = SXE2_RSS_INNER_HEADERS_WITH_OUTER_IPV6;
			}
		}
	}

l_end:
	if (rss->hdr_type == SXE2_RSS_ANY_HEADERS) {
		ret = -EINVAL;
		PMD_LOG_ERR(DRV, "Unsupported rss hdr type.");
	}
	return ret;
}

static int32_t sxe2_flow_set_rss_action_level(uint32_t level,
		struct sxe2_flow *flow, struct rte_flow_error *error)
{
	bool is_inner = false;
	struct sxe2_flow_action_rss *rss = &flow->action.rss;
	int32_t ret = 0;

	if (flow->meta.tunnel_type != SXE2_FLOW_TUNNEL_TYPE_NONE) {
		if (level == 0 || level == 2)
			is_inner = true;
		else if (level == 1)
			is_inner = false;
	} else {
		if (level == 2) {
			ret = -EINVAL;
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, NULL,
				"RSS hash level 2 is not allowed no tunnel flow.");
			PMD_LOG_ERR(DRV, "RSS hash level 2 is not allowed no tunnel flow.");
			goto l_end;
		}
		is_inner = false;
	}
	rss->is_inner = is_inner;
l_end:
	return ret;
}

static int32_t sxe2_flow_set_rss_action_type(uint64_t rss_type,
		struct sxe2_flow *flow, struct rte_flow_error *error)
{
	int32_t ret = 0;
	struct sxe2_flow_pattern *pattern = NULL;
	struct sxe2_flow_action_rss *rss = &flow->action.rss;
	BITMAP_TYPE *hdr;
	BITMAP_TYPE *fld;
	bool is_inner = rss->is_inner;

	ret = sxe2_flow_check_rss_action_type_valid(rss_type);
	if (ret) {
		rte_flow_error_set(error, ENOTSUP,
			RTE_FLOW_ERROR_TYPE_ACTION, NULL,
			"RSS hash type has invalid combination.");
		PMD_LOG_ERR(DRV, "RSS hash type has invalid combination.");
		goto l_end;
	}

	pattern = is_inner ? &flow->pattern_inner : &flow->pattern_outer;
	ret = sxe2_flow_check_rss_action_type_with_pattern(pattern,
			rss_type);
	if (ret) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ACTION, NULL,
			"RSS hash type is not allowed by pattern.");
		PMD_LOG_ERR(DRV, "RSS hash type is not allowed by pattern.");
		goto l_end;
	}

	sxe2_bitmap_copy(rss->hdr_out, flow->pattern_outer.hdrs,
			SXE2_FLOW_HDR_MAX);
	sxe2_bitmap_copy(rss->hdr_in, flow->pattern_inner.hdrs,
			SXE2_FLOW_HDR_MAX);
	hdr = is_inner ? rss->hdr_in : rss->hdr_out;
	fld = rss->fld;
	sxe2_flow_set_rss_action_type_l234(hdr, fld, rss_type);

	ret = sxe2_flow_set_rss_action_hdr_type(flow, is_inner);
	if (ret) {
		rte_flow_error_set(error, ENOTSUP,
			RTE_FLOW_ERROR_TYPE_ACTION, NULL,
			"Unsupported rss hdr type.");
		PMD_LOG_ERR(DRV, "Unsupported rss hdr type.");
		goto l_end;
	}
l_end:
	return ret;
}

static int32_t sxe2_flow_parse_action_rss(const struct rte_flow_action *action,
				      struct rte_flow_error *error,
				      struct sxe2_flow *flow)
{
	const struct rte_flow_action_rss *rss = action->conf;
	int32_t ret = 0;
	uint64_t rss_type = rss->types;
	enum rte_eth_hash_function rss_func = rss->func;
	uint32_t level = rss->level;

	rss_type = rte_eth_rss_hf_refine(rss_type);

	ret = sxe2_flow_check_rss_action_attr(rss, error);
	if (ret)
		goto l_end;

	ret = sxe2_flow_set_rss_action_func(rss_func, rss_type, flow, error);
	if (ret)
		goto l_end;

	ret = sxe2_flow_set_rss_action_level(level, flow, error);
	if (ret)
		goto l_end;

	ret = sxe2_flow_set_rss_action_type(rss_type, flow, error);
	if (ret)
		goto l_end;
	sxe2_set_bit(SXE2_FLOW_ACTION_RSS, flow->action.act_types);
l_end:
	return ret;
}

static int32_t sxe2_flow_parse_action_qregion(struct rte_eth_dev *dev,
		const struct rte_flow_action *action,
		struct rte_flow_error *error, struct sxe2_flow *flow)
{
	int32_t ret = 0;
	uint8_t i = 0;
	const struct rte_flow_action_rss *rss = action->conf;
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);

	if (rss->types != 0 || rss->key_len != 0) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
				action, "Queue region not support rss types or key.");
		ret = -EINVAL;
		goto l_end;
	}
	if (rss->queue_num <= 1) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
				action, "Queue region size can't be 0 or 1.");
		ret = -EINVAL;
		goto l_end;
	}

	for (i = 0; i < rss->queue_num - 1; i++) {
		if (rss->queue[i + 1] != rss->queue[i] + 1) {
			rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
					action, "Queue index for queue region is not continuous.");
			ret = -EINVAL;
			goto l_end;
		}
	}
	if (rss->queue[rss->queue_num - 1] >= adapter->dev_info.dev_data->nb_rx_queues) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
				action, "Queue index for queue region is out of range.");
		ret = -EINVAL;
		goto l_end;
	}

	if (!(rte_is_power_of_2(rss->queue_num))) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
				action, "Queue region size must be power of 2.");
		ret = -EINVAL;
		goto l_end;
	}
	flow->action.q_region.vsi_index = adapter->vsi_ctxt.dpdk_vsi_id;
	flow->action.q_region.q_index = rss->queue[0];
	flow->action.q_region.region = (uint8_t)rte_log2_u32(rss->queue_num);
	sxe2_set_bit(SXE2_FLOW_ACTION_Q_REGION, flow->action.act_types);
l_end:
	return ret;
}


static int32_t sxe2_flow_parse_action_queue(struct rte_eth_dev *dev,
					const struct rte_flow_action *action,
					struct rte_flow_error *error,
					struct sxe2_flow *flow)
{
	int32_t ret = 0;
	const struct rte_flow_action_queue *queue = action->conf;
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);

	if (queue->index >= adapter->dev_info.dev_data->nb_rx_queues) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
			action, "Invalid queue index.");
		ret = -EINVAL;
		goto l_end;
	}
	flow->action.queue.vsi_index = adapter->vsi_ctxt.dpdk_vsi_id;
	flow->action.queue.q_index = queue->index;
	sxe2_set_bit(SXE2_FLOW_ACTION_QUEUE, flow->action.act_types);
l_end:
	return ret;
}


static int32_t sxe2_flow_parse_action_represented_port(struct rte_eth_dev *dev,
						   const struct rte_flow_action *action,
						   struct rte_flow_error *error,
						   struct sxe2_flow *flow)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	const struct rte_flow_action_ethdev *action_ethdev_conf;
	const struct rte_eth_dev *dst_repr_dev;
	uint16_t dst_repr_vsi_id;
	uint16_t dst_backer_port_id;
	uint16_t src_backer_port_id;
	int32_t ret = 0;

	if (!adapter->switchdev_info.is_switchdev) {
		rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_ACTION, NULL,
				"Represented port action only support in switchdev mode.");
		ret = -EINVAL;
		goto l_end;
	}

	if (adapter->dev_type == SXE2_DEV_T_VF) {
		rte_flow_error_set(error, ENOTSUP,
			RTE_FLOW_ERROR_TYPE_ACTION, NULL,
			"Failed to cfg vf dev type.");
		ret = -EINVAL;
		goto l_end;
	}

	action_ethdev_conf = action->conf;
	if (!rte_eth_dev_is_valid_port(action_ethdev_conf->port_id)) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
				action, "Invalid port for represented port action.");
		ret = -EINVAL;
		goto l_end;
	}

	dst_repr_dev = &rte_eth_devices[action_ethdev_conf->port_id];
	if (!dst_repr_dev->data) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
				action, "Invalid port for represented port action.");
		ret = -EINVAL;
		goto l_end;
	}

	dst_backer_port_id = dst_repr_dev->data->backer_port_id;
	if (adapter->is_dev_repr)
		src_backer_port_id = dev->data->backer_port_id;
	else
		src_backer_port_id = adapter->dev_port_id;

	if (src_backer_port_id != dst_backer_port_id) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
				action, "Represented port action only support to cfg port in same device.");
		ret = -EINVAL;
		goto l_end;
	}

	ret = sxe2_flow_vsi_get_ethdev(dev, action_ethdev_conf->port_id, &dst_repr_vsi_id);
	if (ret != 0) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ACTION,
			action, "Port representor action port dev invalid.");
		ret = -EINVAL;
		goto l_end;
	}

	sxe2_flow_action_vsi_merge(flow, dst_repr_vsi_id);
l_end:
	return ret;
}


static int32_t sxe2_flow_parse_action_port_representor(struct rte_eth_dev *dev,
						   const struct rte_flow_action *action,
						   struct rte_flow_error *error,
						   struct sxe2_flow *flow)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	const struct rte_flow_action_ethdev *action_ethdev_conf;
	uint16_t dst_vsi_id;
	int32_t ret = 0;

	if (!adapter->switchdev_info.is_switchdev) {
		rte_flow_error_set(error, ENOTSUP,
			RTE_FLOW_ERROR_TYPE_ACTION, NULL,
			"Port representor action only support in switchdev mode.");
		ret = -EINVAL;
		goto l_end;
	}

	if (adapter->dev_type == SXE2_DEV_T_VF) {
		rte_flow_error_set(error, ENOTSUP,
			RTE_FLOW_ERROR_TYPE_ACTION, NULL,
			"Cfg rule dev type is vf.");
		ret = -EINVAL;
		goto l_end;
	}

	if (!adapter->is_dev_repr) {
		rte_flow_error_set(error, ENOTSUP,
			RTE_FLOW_ERROR_TYPE_ACTION, NULL,
			"Cfg rule dev type is not repr.");
		ret = -EINVAL;
		goto l_end;
	}

	action_ethdev_conf = action->conf;
	if (!action_ethdev_conf || !rte_eth_dev_is_valid_port(action_ethdev_conf->port_id)) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
				action, "Invalid port for port representor action.");
		ret = -EINVAL;
		goto l_end;
	}

	if (dev->data->backer_port_id != action_ethdev_conf->port_id) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
				action, "Invalid port for port representor.");
		ret = -EINVAL;
		goto l_end;
	}

	ret = sxe2_flow_vsi_get_ethdev(dev, action_ethdev_conf->port_id, &dst_vsi_id);
	if (ret != 0) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ACTION,
			action, "Port representor action port dev invalid.");
		ret = -EINVAL;
		goto l_end;
	}

	sxe2_flow_action_vsi_merge(flow, dst_vsi_id);
l_end:
	return ret;
}

int32_t sxe2_flow_parse_action_port_id(struct rte_eth_dev *dev,
				   const struct rte_flow_action *action,
				   struct rte_flow_error *error,
				   struct sxe2_flow *flow)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	const struct rte_flow_action_port_id *action_port_id_conf;
	uint16_t dst_port_id;
	uint16_t dst_vsi_id;
	int32_t ret = 0;

	action_port_id_conf = (const struct rte_flow_action_port_id *)action->conf;
	dst_port_id = action_port_id_conf->original ?
			adapter->dev_port_id : action_port_id_conf->id;

	if (!rte_eth_dev_is_valid_port(dst_port_id)) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
				action, "Invalid port id.");
		ret = -EINVAL;
		goto l_end;
	}

	ret = sxe2_flow_vsi_get_ethdev(dev, dst_port_id, &dst_vsi_id);
	if (ret != 0) {
		rte_flow_error_set(error, ENOTSUP,
			RTE_FLOW_ERROR_TYPE_ACTION, action,
			"Failed to cfg port dev invalid.");
		goto l_end;
	}

	sxe2_flow_action_vsi_merge(flow, dst_vsi_id);

l_end:
	return ret;
}

static int32_t sxe2_flow_parse_action_send_to_kernel(struct rte_eth_dev *dev,
			const struct rte_flow_action *action, struct rte_flow_error *error,
			struct sxe2_flow *flow)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	int32_t ret = 0;

	if (adapter->vsi_ctxt.kernel_vsi_id == UINT16_MAX) {
		rte_flow_error_set(error, ENOTSUP,
			RTE_FLOW_ERROR_TYPE_ACTION, action,
			"Failed to cfg send to kernel action without kernel vsi.");
		ret = -EINVAL;
		goto l_end;
	}

	sxe2_flow_action_vsi_merge(flow, adapter->vsi_ctxt.kernel_vsi_id);
l_end:
	return ret;
}

static int32_t sxe2_flow_check_actions(struct rte_eth_dev *dev __rte_unused, struct sxe2_flow *flow,
				   uint8_t action_num[], struct rte_flow_error *error)
{
	enum sxe2_flow_engine_type engine_type = flow->engine_type;
	int32_t ret = 0;
	int32_t dest_num = action_num[SXE2_FLOW_ACTION_Q_REGION] +
			   action_num[SXE2_FLOW_ACTION_QUEUE];
	int32_t vsi_num = action_num[SXE2_FLOW_ACTION_TO_VSI];
	int32_t vsi_list_num = action_num[SXE2_FLOW_ACTION_TO_VSI_LIST];
	int32_t pass_num = action_num[SXE2_FLOW_ACTION_PASSTHRU];
	int32_t drop_num = action_num[SXE2_FLOW_ACTION_DROP];
	int32_t mark_num = action_num[SXE2_FLOW_ACTION_MARK];
	int32_t count_num = action_num[SXE2_FLOW_ACTION_COUNT];
	int32_t rss_num = action_num[SXE2_FLOW_ACTION_RSS];
	int32_t fwd_num = dest_num + vsi_num + vsi_list_num;
	int32_t total_num = dest_num + vsi_num + vsi_list_num + pass_num +
			drop_num + mark_num + count_num + rss_num;

	if (pass_num > 1 || drop_num > 1 || mark_num > 1 ||
			count_num > 1 || rss_num > 1 || dest_num > 1 ||
			vsi_num > 1 || vsi_list_num > 1) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, NULL,
				"ecah action can only be used once.");
		PMD_LOG_ERR(DRV, "ecah action can only be used once.");
		ret = -EINVAL;
		goto l_end;
	}

	if (vsi_list_num && engine_type != SXE2_FLOW_ENGINE_SWITCH) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ACTION, NULL,
			"VSI_LIST action is only supported for switch engine.");
		PMD_LOG_ERR(DRV, "VSI_LIST action is only supported for switch engine.");
		ret = -EINVAL;
		goto l_end;
	}

	if (drop_num) {
		if (total_num > drop_num + count_num) {
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, NULL,
				"Drop action can't be used with other actions unless count.");
			PMD_LOG_ERR(DRV,
				"Drop action can't be used with other actions unless count.");
			ret = -EINVAL;
			goto l_end;
		}
	}

	if (fwd_num > 1) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ACTION, NULL,
			"Only supports one type of forwarding action.");
		PMD_LOG_ERR(DRV, "Only supports one type of forwarding action.");
		ret = -EINVAL;
		goto l_end;
	}

	if (vsi_list_num) {
		if (total_num > vsi_list_num) {
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, NULL,
				"VSI_LIST action can't be used with other actions.");
			PMD_LOG_ERR(DRV,
				"VSI_LIST action can't be used with other actions.");
			ret = -EINVAL;
			goto l_end;
		}
	}

	if (engine_type == SXE2_FLOW_ENGINE_FNAV) {
		if (vsi_num) {
			flow->action.q_region.q_index = 0;
			flow->action.q_region.region = 7;
			flow->action.q_region.vsi_index = flow->action.vsi.vsi_index;
			sxe2_set_bit(SXE2_FLOW_ACTION_Q_REGION, flow->action.act_types);
			dest_num++;
		}
	}

l_end:
	return ret;
}


int32_t sxe2_flow_parse_action(struct rte_eth_dev *dev,
			const struct rte_flow_action actions[],
			struct rte_flow_error *error,
			struct sxe2_flow *flow)
{
	int32_t ret = 0;
	const struct rte_flow_action *action;
	const struct rte_flow_action_count *act_count;
	const struct rte_flow_action_mark *act_mark;
	uint8_t action_num[SXE2_FLOW_ACTION_MAX] = {0};
	enum sxe2_flow_engine_type engine_type = flow->engine_type;
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);

	sxe2_flow_action_pre(flow);

	for (action = actions; action->type != RTE_FLOW_ACTION_TYPE_END; action++) {
		switch (action->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_PASSTHRU:
			if (engine_type == SXE2_FLOW_ENGINE_FNAV) {
				sxe2_set_bit(SXE2_FLOW_ACTION_PASSTHRU, flow->action.act_types);
				action_num[SXE2_FLOW_ACTION_PASSTHRU]++;
			} else {
				rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION, action,
					"Passthru action is not supported for this flow.");
				PMD_LOG_ERR(DRV,
					"Passthru action is not supported for this flow.");
				ret = -EINVAL;
				goto l_end;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_DROP:
			if (engine_type == SXE2_FLOW_ENGINE_ACL ||
				engine_type == SXE2_FLOW_ENGINE_SWITCH ||
				engine_type == SXE2_FLOW_ENGINE_FNAV) {
				sxe2_set_bit(SXE2_FLOW_ACTION_DROP, flow->action.act_types);
				action_num[SXE2_FLOW_ACTION_DROP]++;
			} else {
				rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION, action,
					"Drop action is not supported for this flow.");
				PMD_LOG_ERR(DRV,
					"Drop action is not supported for this flow.");
				ret = -EINVAL;
				goto l_end;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_MARK:
			if (engine_type == SXE2_FLOW_ENGINE_FNAV) {
				sxe2_set_bit(SXE2_FLOW_ACTION_MARK, flow->action.act_types);
				act_mark = action->conf;
				flow->action.mark.mark_id = act_mark->id;
				action_num[SXE2_FLOW_ACTION_MARK]++;
			} else {
				rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION, action,
					"Mark action is not supported for this flow.");
				PMD_LOG_ERR(DRV,
					"Mark action is not supported for this flow.");
				ret = -EINVAL;
				goto l_end;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_COUNT:
			if (engine_type == SXE2_FLOW_ENGINE_FNAV) {
				sxe2_set_bit(SXE2_FLOW_ACTION_COUNT, flow->action.act_types);
				act_count = action->conf;
				flow->action.count.user_id = act_count->id;
				flow->action.count.driver_id = 0;
				if (flow->action.count.user_id == 0)
					flow->action.count.driver_id =
						++adapter->flow_ctxt.hw_res.global_index;
				action_num[SXE2_FLOW_ACTION_COUNT]++;
			} else {
				rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION, action,
					"Count action is not supported for this flow.");
				PMD_LOG_ERR(DRV,
					"Count action is not supported for this flow.");
				ret = -EINVAL;
				goto l_end;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_RSS:
			if (engine_type == SXE2_FLOW_ENGINE_RSS) {
				ret = sxe2_flow_parse_action_rss(action, error, flow);
				if (ret != 0)
					goto l_end;
				action_num[SXE2_FLOW_ACTION_RSS]++;
			} else if (engine_type == SXE2_FLOW_ENGINE_ACL ||
				engine_type == SXE2_FLOW_ENGINE_SWITCH ||
				engine_type == SXE2_FLOW_ENGINE_FNAV) {
				ret = sxe2_flow_parse_action_qregion(dev, action, error, flow);
				if (ret != 0)
					goto l_end;
				action_num[SXE2_FLOW_ACTION_Q_REGION]++;
			} else {
				rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION, action,
					"RSS action is only supported for RSS flow.");
				PMD_LOG_ERR(DRV,
					"RSS action is only supported for RSS flow.");
				ret = -EINVAL;
				goto l_end;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_QUEUE:
			if (engine_type == SXE2_FLOW_ENGINE_ACL ||
				engine_type == SXE2_FLOW_ENGINE_SWITCH ||
				engine_type == SXE2_FLOW_ENGINE_FNAV) {
				ret = sxe2_flow_parse_action_queue(dev, action, error, flow);
				if (ret != 0)
					goto l_end;
				action_num[SXE2_FLOW_ACTION_QUEUE]++;
			} else {
				rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION, action,
					"Queue action is not supported for this flow.");
				PMD_LOG_ERR(DRV,
					"Queue action is not supported for this flow.");
				ret = -EINVAL;
				goto l_end;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT:
			if (engine_type == SXE2_FLOW_ENGINE_SWITCH) {
				ret = sxe2_flow_parse_action_represented_port(dev,
					action, error, flow);
				if (ret != 0)
					goto l_end;
			} else {
				rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION, action,
					"REPRESENTED PORT action is only supported for SWITCH flow.");
				PMD_LOG_ERR(DRV,
					"REPRESENTED PORT action is only supported for SWITCH flow.");
				ret = -EINVAL;
				goto l_end;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR:
			if (engine_type == SXE2_FLOW_ENGINE_SWITCH) {
				ret = sxe2_flow_parse_action_port_representor(dev,
					action, error, flow);
				if (ret != 0)
					goto l_end;
			} else {
				rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION, action,
					"PORT REPRESENTOR action is only supported for SWITCH flow.");
				PMD_LOG_ERR(DRV,
					"PORT REPRESENTOR action is only supported for SWITCH flow.");
				ret = -EINVAL;
				goto l_end;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_PORT_ID:
			if (engine_type == SXE2_FLOW_ENGINE_SWITCH ||
				engine_type == SXE2_FLOW_ENGINE_ACL ||
				engine_type == SXE2_FLOW_ENGINE_FNAV) {
				ret = sxe2_flow_parse_action_port_id(dev, action,
						error, flow);
				if (ret != 0)
					goto l_end;
			} else {
				rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION, action,
					"PORT ID action is only supported for this flow.");
				PMD_LOG_ERR(DRV,
					"PORT ID action is only supported for this flow.");
				ret = -EINVAL;
				goto l_end;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_SEND_TO_KERNEL:
			if (engine_type == SXE2_FLOW_ENGINE_ACL ||
				engine_type == SXE2_FLOW_ENGINE_FNAV ||
				engine_type == SXE2_FLOW_ENGINE_SWITCH) {
				ret = sxe2_flow_parse_action_send_to_kernel(dev,
						action, error, flow);
				if (ret != 0)
					goto l_end;
			} else {
				rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION, action,
					"SEND TO KERNEL action is only supported for this flow.");
				PMD_LOG_ERR(DRV,
					"SEND TO KERNEL action is only supported for this flow.");
				ret = -EINVAL;
				goto l_end;
			}
			break;
		default:
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, actions,
				"Invalid action.");
			PMD_LOG_ERR(DRV, "Invalid action type:%d", actions->type);
			ret = -EINVAL;
			goto l_end;
		}
	}

	sxe2_flow_action_post(flow, action_num);

	ret = sxe2_flow_check_actions(dev, flow, action_num, error);
	if (ret != 0)
		goto l_end;
l_end:
	return ret;
}
