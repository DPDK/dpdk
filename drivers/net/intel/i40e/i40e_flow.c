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

#define I40E_FLOW_DUMP_CHUNK_BYTES 32

static const char *
i40e_flow_rule_name(enum rte_filter_type filter_type)
{
	switch (filter_type) {
	case RTE_ETH_FILTER_HASH:
		return "hash";
	default:
		return "unknown";
	}
}

static size_t
i40e_flow_rule_size(enum rte_filter_type filter_type)
{
	switch (filter_type) {
	case RTE_ETH_FILTER_HASH:
		return sizeof(struct i40e_rss_filter);
	default:
		return 0;
	}
}

static void
i40e_flow_dump_blob(FILE *file, const char *engine,
		    const void *data, size_t data_len)
{
	const uint8_t *raw = (const uint8_t *)data;
	const size_t nchunks =
		(data_len + I40E_FLOW_DUMP_CHUNK_BYTES - 1) /
		I40E_FLOW_DUMP_CHUNK_BYTES;
	char title[64];
	size_t ci;

	fprintf(file, "FLOW DUMP: driver=i40e engine=%s\n", engine);
	fprintf(file, "FLOW DUMP: DATA size=%zu chunks=%zu chunk_bytes=%d\n",
		data_len, nchunks, I40E_FLOW_DUMP_CHUNK_BYTES);

	for (ci = 0; ci < nchunks; ci++) {
		const size_t off = ci * I40E_FLOW_DUMP_CHUNK_BYTES;
		const size_t clen =
			RTE_MIN((size_t)I40E_FLOW_DUMP_CHUNK_BYTES, data_len - off);

		snprintf(title, sizeof(title), "FLOW DUMP: chunk %03zu/%03zu",
			 ci + 1, nchunks);
		rte_memdump(file, title, raw + off, clen);
	}
}

static int
i40e_flow_dev_dump(struct rte_eth_dev *dev,
		   struct rte_flow *flow,
		   FILE *file,
		   struct rte_flow_error *error)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct rte_flow *p_flow;
	bool found = false;
	int ret;

	/* try the new flow engine first */
	ret = ci_flow_dump(&pf->flow_engine_conf, flow, file, error);

	/*
	 * There are multiple possible situations here:
	 *
	 * - User requested to dump all flows
	 * - User requested to dump a specific flow
	 *
	 * For the first case, we keep going because legacy engines might still
	 * have flows we want to dump.
	 *
	 * For the second case, we only keep going if the flow we were asked to
	 * dump was not found in the new engines.
	 */
	if (flow != NULL && ret == 0)
		return 0;

	TAILQ_FOREACH(p_flow, &pf->flow_list, node) {
		size_t rule_size = 0;
		const void *rule_data = NULL;

		if (flow != NULL && p_flow != flow)
			continue;

		/* should not happen */
		if (p_flow->rule == NULL) {
			PMD_DRV_LOG(DEBUG, "Invalid flow rule");
			continue;
		}

		rule_size = i40e_flow_rule_size(p_flow->filter_type);
		/* should not happen either */
		if (rule_size == 0)
			continue;

		found = true;
		rule_data = p_flow->rule;
		i40e_flow_dump_blob(file,
			i40e_flow_rule_name(p_flow->filter_type),
			rule_data, rule_size);
	}

	if (flow != NULL && !found)
		return rte_flow_error_set(error, ENOENT,
			RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
			"Flow not found");

	return 0;
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
i40e_flow_check(struct rte_eth_dev *dev,
		   const struct rte_flow_attr *attr,
		   const struct rte_flow_item pattern[],
		   const struct rte_flow_action actions[],
		   struct i40e_filter_ctx *filter_ctx,
		   struct rte_flow_error *error)
{
	int ret;

	ret = ci_flow_check_attr(attr, NULL, error);
	if (ret) {
		return ret;
	}
	/* action and pattern validation will happen in each respective engine */

	if (!pattern) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM_NUM,
				   NULL, "NULL pattern.");
		return -rte_errno;
	}

	if (!actions) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION_NUM,
				   NULL, "NULL action.");
		return -rte_errno;
	}

	/* try parsing as RSS */
	filter_ctx->type = RTE_ETH_FILTER_HASH;

	return i40e_hash_parse(dev, pattern, actions, &filter_ctx->rss_conf, error);
}

static int
i40e_flow_validate(struct rte_eth_dev *dev,
		   const struct rte_flow_attr *attr,
		   const struct rte_flow_item pattern[],
		   const struct rte_flow_action actions[],
		   struct rte_flow_error *error)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	/* creates dummy context */
	struct i40e_filter_ctx filter_ctx = {0};
	int ret;

	/* try the new engine first */
	ret = ci_flow_validate(&pf->flow_engine_conf, attr, pattern, actions, error);
	if (ret == 0)
		return 0;

	return i40e_flow_check(dev, attr, pattern, actions, &filter_ctx, error);
}

static struct rte_flow *
i40e_flow_create(struct rte_eth_dev *dev,
		 const struct rte_flow_attr *attr,
		 const struct rte_flow_item pattern[],
		 const struct rte_flow_action actions[],
		 struct rte_flow_error *error)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_filter_ctx filter_ctx = {0};
	struct rte_flow *flow = NULL;
	int ret;

	/* try the new engine first */
	flow = ci_flow_create(&pf->flow_engine_conf, attr, pattern, actions, error);
	if (flow != NULL)
		return flow;

	ret = i40e_flow_check(dev, attr, pattern, actions, &filter_ctx, error);
	if (ret < 0)
		return NULL;

	flow = rte_zmalloc("i40e_flow", sizeof(struct rte_flow), 0);
	if (!flow) {
		rte_flow_error_set(error, ENOMEM,
					RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					"Failed to allocate memory");
		return flow;
	}

	switch (filter_ctx.type) {
	case RTE_ETH_FILTER_HASH:
		ret = i40e_hash_filter_create(pf, &filter_ctx.rss_conf);
		if (ret)
			goto free_flow;
		flow->rule = TAILQ_LAST(&pf->rss_config_list,
					i40e_rss_conf_list);
		break;
	default:
		goto free_flow;
	}

	flow->filter_type = filter_ctx.type;
	TAILQ_INSERT_TAIL(&pf->flow_list, flow, node);
	return flow;

free_flow:
	rte_flow_error_set(error, -ret,
			   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
			   "Failed to create flow.");

	rte_free(flow);

	return NULL;
}

static int
i40e_flow_destroy(struct rte_eth_dev *dev,
		  struct rte_flow *flow,
		  struct rte_flow_error *error)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	enum rte_filter_type filter_type = flow->filter_type;
	int ret = 0;

	/* try the new engine first */
	ret = ci_flow_destroy(&pf->flow_engine_conf, flow, error);
	if (ret == 0)
		return 0;

	switch (filter_type) {
	case RTE_ETH_FILTER_HASH:
		ret = i40e_hash_filter_destroy(pf, flow->rule);
		break;
	default:
		PMD_DRV_LOG(WARNING, "Filter type (%d) not supported",
			    filter_type);
		ret = -EINVAL;
		break;
	}

	if (!ret) {
		TAILQ_REMOVE(&pf->flow_list, flow, node);
		rte_free(flow);

	} else
		rte_flow_error_set(error, -ret,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to destroy flow.");

	return ret;
}

static int
i40e_flow_flush(struct rte_eth_dev *dev, struct rte_flow_error *error)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	int ret;

	/* flush the new engine first */
	ret = ci_flow_flush(&pf->flow_engine_conf, error);
	if (ret != 0)
		return ret;

	ret = i40e_hash_filter_flush(pf);
	if (ret)
		rte_flow_error_set(error, -ret,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to flush RSS flows.");
	return ret;
}

static int
i40e_flow_query(struct rte_eth_dev *dev,
		struct rte_flow *flow,
		const struct rte_flow_action *actions,
		void *data, struct rte_flow_error *error)
{
	struct i40e_pf *pf = dev->data->dev_private;
	struct i40e_rss_filter *rss_rule = (struct i40e_rss_filter *)flow->rule;
	enum rte_filter_type filter_type = flow->filter_type;
	struct rte_flow_action_rss *rss_conf = data;
	int ret;

	/* try the new engine first */
	ret = ci_flow_query(&pf->flow_engine_conf, flow, actions, data, error);
	if (ret == 0)
		return 0;

	if (!rss_rule) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "Invalid rule");
		return -rte_errno;
	}

	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_RSS:
			if (filter_type != RTE_ETH_FILTER_HASH) {
				rte_flow_error_set(error, ENOTSUP,
						   RTE_FLOW_ERROR_TYPE_ACTION,
						   actions,
						   "action not supported");
				return -rte_errno;
			}
			*rss_conf = (struct rte_flow_action_rss){
				.func = rss_rule->rss_filter_info.func,
				.types = rss_rule->rss_filter_info.types,
				.key_len = rss_rule->rss_filter_info.key_len,
				.queue_num = rss_rule->rss_filter_info.queue_num,
				.key = rss_rule->rss_filter_info.key_len ?
					rss_rule->rss_filter_info.key : NULL,
				.queue = rss_rule->rss_filter_info.queue_num ?
					rss_rule->rss_filter_info.queue : NULL,
			};
			break;
		default:
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  actions,
						  "action not supported");
		}
	}

	return 0;
}
