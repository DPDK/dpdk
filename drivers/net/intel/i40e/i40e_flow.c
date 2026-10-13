/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */

#include <sys/queue.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>

#include <rte_debug.h>
#include <rte_ether.h>
#include <ethdev_driver.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_tailq.h>
#include <rte_hexdump.h>
#include <rte_flow_driver.h>
#include <rte_bitmap.h>

#include "i40e_logs.h"
#include "base/i40e_type.h"
#include "base/i40e_prototype.h"
#include "i40e_ethdev.h"
#include "i40e_hash.h"
#include "i40e_flow.h"

#include "../common/flow_check.h"

const struct ci_flow_engine_list i40e_flow_engine_list = {
	{
		&i40e_flow_engine_ethertype,
		&i40e_flow_engine_fdir,
		&i40e_flow_engine_tunnel_qinq,
		&i40e_flow_engine_tunnel_vxlan,
		&i40e_flow_engine_tunnel_nvgre,
		&i40e_flow_engine_tunnel_mpls,
		&i40e_flow_engine_tunnel_gtp,
		&i40e_flow_engine_tunnel_l4,
		&i40e_flow_engine_hash_pattern,
		&i40e_flow_engine_hash_vlan,
		&i40e_flow_engine_hash_empty,
	}
};

static int i40e_flow_validate(struct rte_eth_dev *dev,
			      const struct rte_flow_attr *attr,
			      const struct rte_flow_item pattern[],
			      const struct rte_flow_action actions[],
			      struct rte_flow_error *error);
static struct rte_flow *i40e_flow_create(struct rte_eth_dev *dev,
					 const struct rte_flow_attr *attr,
					 const struct rte_flow_item pattern[],
					 const struct rte_flow_action actions[],
					 struct rte_flow_error *error);
static int i40e_flow_destroy(struct rte_eth_dev *dev,
			     struct rte_flow *flow,
			     struct rte_flow_error *error);
static int i40e_flow_flush(struct rte_eth_dev *dev,
			   struct rte_flow_error *error);
static int i40e_flow_query(struct rte_eth_dev *dev,
			   struct rte_flow *flow,
			   const struct rte_flow_action *actions,
			   void *data, struct rte_flow_error *error);
static int i40e_flow_dev_dump(struct rte_eth_dev *dev,
			      struct rte_flow *flow,
			      FILE *file,
			      struct rte_flow_error *error);

const struct rte_flow_ops i40e_flow_ops = {
	.validate = i40e_flow_validate,
	.create = i40e_flow_create,
	.destroy = i40e_flow_destroy,
	.flush = i40e_flow_flush,
	.query = i40e_flow_query,
	.dev_dump = i40e_flow_dev_dump,
};

static int
i40e_flow_dev_dump(struct rte_eth_dev *dev,
		   struct rte_flow *flow,
		   FILE *file,
		   struct rte_flow_error *error)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);

	return ci_flow_dump(&pf->flow_engine_conf, flow, file, error);
}

int
i40e_get_outer_vlan(struct i40e_pf *pf, uint16_t *tpid)
{
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	int qinq = pf->dev_data->dev_conf.rxmode.offloads &
		RTE_ETH_RX_OFFLOAD_VLAN_EXTEND;
	uint64_t reg_r = 0;
	uint16_t reg_id;
	int ret;

	if (qinq)
		reg_id = 2;
	else
		reg_id = 3;

	ret = i40e_aq_debug_read_register(hw, I40E_GL_SWT_L2TAGCTRL(reg_id),
				    &reg_r, NULL);
	if (ret != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to read from L2 tag ctrl register [%d]", reg_id);
		return -EIO;
	}

	*tpid = (reg_r >> I40E_GL_SWT_L2TAGCTRL_ETHERTYPE_SHIFT) & 0xFFFF;

	return 0;
}

uint8_t
i40e_flow_fdir_get_pctype_value(struct i40e_pf *pf,
				enum rte_flow_item_type item_type,
				struct i40e_fdir_filter *filter)
{
	struct i40e_customized_pctype *cus_pctype = NULL;

	switch (item_type) {
	case RTE_FLOW_ITEM_TYPE_GTPC:
		cus_pctype = i40e_find_customized_pctype(pf,
							 I40E_CUSTOMIZED_GTPC);
		break;
	case RTE_FLOW_ITEM_TYPE_GTPU:
		if (!filter->input.flow_ext.inner_ip)
			cus_pctype = i40e_find_customized_pctype(pf,
							 I40E_CUSTOMIZED_GTPU);
		else if (filter->input.flow_ext.iip_type ==
			 I40E_FDIR_IPTYPE_IPV4)
			cus_pctype = i40e_find_customized_pctype(pf,
						 I40E_CUSTOMIZED_GTPU_IPV4);
		else if (filter->input.flow_ext.iip_type ==
			 I40E_FDIR_IPTYPE_IPV6)
			cus_pctype = i40e_find_customized_pctype(pf,
						 I40E_CUSTOMIZED_GTPU_IPV6);
		break;
	case RTE_FLOW_ITEM_TYPE_L2TPV3OIP:
		if (filter->input.flow_ext.oip_type == I40E_FDIR_IPTYPE_IPV4)
			cus_pctype = i40e_find_customized_pctype(pf,
						I40E_CUSTOMIZED_IPV4_L2TPV3);
		else if (filter->input.flow_ext.oip_type ==
			 I40E_FDIR_IPTYPE_IPV6)
			cus_pctype = i40e_find_customized_pctype(pf,
						I40E_CUSTOMIZED_IPV6_L2TPV3);
		break;
	case RTE_FLOW_ITEM_TYPE_ESP:
		if (!filter->input.flow_ext.is_udp) {
			if (filter->input.flow_ext.oip_type ==
				I40E_FDIR_IPTYPE_IPV4)
				cus_pctype = i40e_find_customized_pctype(pf,
						I40E_CUSTOMIZED_ESP_IPV4);
			else if (filter->input.flow_ext.oip_type ==
				I40E_FDIR_IPTYPE_IPV6)
				cus_pctype = i40e_find_customized_pctype(pf,
						I40E_CUSTOMIZED_ESP_IPV6);
		} else {
			if (filter->input.flow_ext.oip_type ==
				I40E_FDIR_IPTYPE_IPV4)
				cus_pctype = i40e_find_customized_pctype(pf,
						I40E_CUSTOMIZED_ESP_IPV4_UDP);
			else if (filter->input.flow_ext.oip_type ==
					I40E_FDIR_IPTYPE_IPV6)
				cus_pctype = i40e_find_customized_pctype(pf,
						I40E_CUSTOMIZED_ESP_IPV6_UDP);
			filter->input.flow_ext.is_udp = false;
		}
		break;
	default:
		PMD_DRV_LOG(ERR, "Unsupported item type");
		break;
	}

	if (cus_pctype && cus_pctype->valid)
		return cus_pctype->pctype;

	return I40E_FILTER_PCTYPE_INVALID;
}

static int
i40e_flow_validate(struct rte_eth_dev *dev,
		   const struct rte_flow_attr *attr,
		   const struct rte_flow_item pattern[],
		   const struct rte_flow_action actions[],
		   struct rte_flow_error *error)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);

	return ci_flow_validate(&pf->flow_engine_conf, attr, pattern, actions, error);
}

static struct rte_flow *
i40e_flow_create(struct rte_eth_dev *dev,
		 const struct rte_flow_attr *attr,
		 const struct rte_flow_item pattern[],
		 const struct rte_flow_action actions[],
		 struct rte_flow_error *error)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);

	return ci_flow_create(&pf->flow_engine_conf, attr, pattern, actions, error);
}

static int
i40e_flow_destroy(struct rte_eth_dev *dev,
		  struct rte_flow *flow,
		  struct rte_flow_error *error)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);

	return ci_flow_destroy(&pf->flow_engine_conf, flow, error);
}

static int
i40e_flow_flush(struct rte_eth_dev *dev, struct rte_flow_error *error)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);

	return ci_flow_flush(&pf->flow_engine_conf, error);
}

static int
i40e_flow_query(struct rte_eth_dev *dev,
		struct rte_flow *flow,
		const struct rte_flow_action *actions,
		void *data, struct rte_flow_error *error)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);

	return ci_flow_query(&pf->flow_engine_conf, flow, actions, data, error);
}
