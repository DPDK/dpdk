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
#include <math.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_eth_ctrl.h>
#include <rte_tailq.h>
#include <rte_flow_driver.h>
#include <rte_flow.h>
#include <rte_bitmap.h>
#include <ethdev_driver.h>
#include "cpfl_rules.h"
#include "cpfl_logs.h"
#include "cpfl_ethdev.h"
#include "cpfl_flow.h"
#include "cpfl_fxp_rule.h"
#include "cpfl_flow_parser.h"

#define CPFL_COOKIE_DEF		0x1000
#define CPFL_MOD_COOKIE_DEF	0x1237561
#define CPFL_PREC_DEF		1
#define CPFL_PREC_SET		5
#define CPFL_TYPE_ID		3
#define CPFL_OFFSET		0x0a
#define CPFL_HOST_ID_DEF	0
#define CPFL_PF_NUM_DEF		0
#define CPFL_PORT_NUM_DEF	0
#define CPFL_RESP_REQ_DEF	2
#define CPFL_PIN_TO_CACHE_DEF	0
#define CPFL_CLEAR_MIRROR_1ST_STATE_DEF	0
#define CPFL_FIXED_FETCH_DEF	0
#define CPFL_PTI_DEF		0
#define CPFL_MOD_OBJ_SIZE_DEF	0
#define CPFL_PIN_MOD_CONTENT_DEF	0

#define CPFL_MAX_MOD_CONTENT_INDEX	256
#define CPFL_MAX_MR_ACTION_NUM	8

/* Struct used when parse detailed rule information with json file */
struct cpfl_rule_info_meta {
	struct cpfl_flow_pr_action pr_action;	/* json action field of pattern rule */
	uint32_t pr_num;			/* number of pattern rules */
	uint32_t mr_num;			/* number of modification rules */
	uint32_t rule_num;			/* number of all rules */
	struct cpfl_rule_info rules[];
};

static uint32_t cpfl_fxp_mod_idx_alloc(struct cpfl_adapter_ext *ad);
static void cpfl_fxp_mod_idx_free(struct cpfl_adapter_ext *ad, uint32_t idx);
uint64_t cpfl_rule_cookie = CPFL_COOKIE_DEF;

static int
cpfl_fxp_create(struct rte_eth_dev *dev,
		struct rte_flow *flow,
		void *meta,
		struct rte_flow_error *error)
{
	int ret = 0;
	uint32_t cpq_id = 0;
	struct cpfl_itf *itf = CPFL_DEV_TO_ITF(dev);
	struct cpfl_adapter_ext *ad = itf->adapter;
	struct cpfl_rule_info_meta *rim = meta;
	struct cpfl_vport *vport;
	struct cpfl_repr *repr;

	if (!rim)
		return ret;

	if (itf->type == CPFL_ITF_TYPE_VPORT) {
		vport = (struct cpfl_vport *)itf;
		/* Every vport has one pair control queues configured to handle message.
		 * Even index is tx queue and odd index is rx queue.
		 */
		cpq_id = vport->base.devarg_id * 2;
	} else if (itf->type == CPFL_ITF_TYPE_REPRESENTOR) {
		repr = (struct cpfl_repr *)itf;
		cpq_id = ((repr->repr_id.pf_id  + repr->repr_id.vf_id) &
			  (CPFL_TX_CFGQ_NUM - 1)) * 2;
	} else {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "fail to find correct control queue");
		return -rte_errno;
	}

	ret = cpfl_rule_process(itf, ad->ctlqp[cpq_id], ad->ctlqp[cpq_id + 1],
				rim->rules, rim->rule_num, true);
	if (ret < 0) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "cpfl filter create flow fail");
		rte_free(rim);
		return ret;
	}

	flow->rule = rim;

	return ret;
}

static int
cpfl_fxp_destroy(struct rte_eth_dev *dev,
		 struct rte_flow *flow,
		 struct rte_flow_error *error)
{
	int ret = 0;
	uint32_t cpq_id = 0;
	struct cpfl_itf *itf = CPFL_DEV_TO_ITF(dev);
	struct cpfl_adapter_ext *ad = itf->adapter;
	struct cpfl_rule_info_meta *rim;
	uint32_t i;
	struct cpfl_vport *vport;
	struct cpfl_repr *repr;

	rim = (struct cpfl_rule_info_meta *)flow->rule;
	if (!rim) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "no such flow create by cpfl filter");

		return -rte_errno;
	}

	if (itf->type == CPFL_ITF_TYPE_VPORT) {
		vport = (struct cpfl_vport *)itf;
		cpq_id = vport->base.devarg_id * 2;
	} else if (itf->type == CPFL_ITF_TYPE_REPRESENTOR) {
		repr = (struct cpfl_repr *)itf;
		cpq_id = ((repr->repr_id.pf_id  + repr->repr_id.vf_id) &
			  (CPFL_TX_CFGQ_NUM - 1)) * 2;
	} else {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "fail to find correct control queue");
		ret = -rte_errno;
		goto err;
	}

	ret = cpfl_rule_process(itf, ad->ctlqp[cpq_id], ad->ctlqp[cpq_id + 1], rim->rules,
				rim->rule_num, false);
	if (ret < 0) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "fail to destroy cpfl filter rule");
		goto err;
	}

	/* free mod index */
	for (i = rim->pr_num; i < rim->rule_num; i++)
		cpfl_fxp_mod_idx_free(ad, rim->rules[i].mod.mod_index);
err:
	rte_free(rim);
	flow->rule = NULL;
	return ret;
}

static bool
cpfl_fxp_parse_pattern(const struct cpfl_flow_pr_action *pr_action,
		       struct cpfl_rule_info_meta *rim,
		       int i)
{
	if (pr_action->type == CPFL_JS_PR_ACTION_TYPE_SEM) {
		struct cpfl_rule_info *rinfo = &rim->rules[i];

		rinfo->type = CPFL_RULE_TYPE_SEM;
		rinfo->sem.prof_id = pr_action->sem.prof;
		rinfo->sem.sub_prof_id = pr_action->sem.subprof;
		rinfo->sem.key_byte_len = pr_action->sem.keysize;
		memcpy(rinfo->sem.key, pr_action->sem.cpfl_flow_pr_fv, rinfo->sem.key_byte_len);
		rinfo->sem.pin_to_cache = CPFL_PIN_TO_CACHE_DEF;
		rinfo->sem.fixed_fetch = CPFL_FIXED_FETCH_DEF;
	} else {
		PMD_DRV_LOG(ERR, "Invalid pattern item.");
		return false;
	}

	return true;
}

static int
cpfl_parse_mod_content(struct cpfl_adapter_ext *adapter,
		       struct cpfl_rule_info *match_rinfo,
		       struct cpfl_rule_info *mod_rinfo,
		       const struct cpfl_flow_mr_action *mr_action)
{
	struct cpfl_mod_rule_info *minfo = &mod_rinfo->mod;
	uint32_t mod_idx;
	int i;
	int next = match_rinfo->act_byte_len / (sizeof(union cpfl_action_set));
	union cpfl_action_set *act_set =
		&((union cpfl_action_set *)match_rinfo->act_bytes)[next];

	if (!mr_action || mr_action->type != CPFL_JS_MR_ACTION_TYPE_MOD)
		return -EINVAL;

	*act_set = cpfl_act_mod_profile(CPFL_PREC_DEF,
					mr_action->mod.prof,
					CPFL_PTI_DEF,
					0, /* append */
					0, /* prepend */
					CPFL_ACT_MOD_PROFILE_PREFETCH_256B);

	act_set++;
	match_rinfo->act_byte_len += sizeof(union cpfl_action_set);

	mod_idx = cpfl_fxp_mod_idx_alloc(adapter);
	if (mod_idx == CPFL_MAX_MOD_CONTENT_INDEX) {
		PMD_DRV_LOG(ERR, "Out of Mod Index.");
		return -ENOMEM;
	}

	*act_set = cpfl_act_mod_addr(CPFL_PREC_DEF, mod_idx);

	act_set++;
	match_rinfo->act_byte_len += sizeof(union cpfl_action_set);

	mod_rinfo->type = CPFL_RULE_TYPE_MOD;
	minfo->mod_obj_size = CPFL_MOD_OBJ_SIZE_DEF;
	minfo->pin_mod_content = CPFL_PIN_MOD_CONTENT_DEF;
	minfo->mod_index = mod_idx;
	mod_rinfo->cookie = CPFL_MOD_COOKIE_DEF;
	mod_rinfo->port_num = CPFL_PORT_NUM_DEF;
	mod_rinfo->resp_req = CPFL_RESP_REQ_DEF;

	minfo->mod_content_byte_len = mr_action->mod.byte_len + 2;
	for (i = 0; i < minfo->mod_content_byte_len; i++)
		minfo->mod_content[i] = mr_action->mod.data[i];

	return 0;
}

#define CPFL_FXP_MAX_QREGION_SIZE 128
#define CPFL_INVALID_QUEUE_ID -2
static int
cpfl_fxp_parse_action(struct cpfl_itf *itf,
		      const struct rte_flow_action *actions,
		      const struct cpfl_flow_mr_action *mr_action,
		      struct cpfl_rule_info_meta *rim,
		      int priority,
		      int index)
{
	const struct rte_flow_action_ethdev *act_ethdev;
	const struct rte_flow_action *action;
	const struct rte_flow_action_queue *act_q;
	const struct rte_flow_action_rss *rss;
	struct rte_eth_dev_data *data;
	enum rte_flow_action_type action_type;
	struct cpfl_vport *vport;
	/* used when action is PORT_REPRESENTOR type */
	struct cpfl_itf *dst_itf;
	uint16_t dev_id; /* vsi id */
	int queue_id = -1;
	bool fwd_vsi = false;
	bool fwd_q = false;
	bool is_vsi;
	uint32_t i;
	struct cpfl_rule_info *rinfo = &rim->rules[index];
	union cpfl_action_set *act_set = (void *)rinfo->act_bytes;

	priority = CPFL_PREC_MAX - priority;
	for (action = actions; action->type !=
	     RTE_FLOW_ACTION_TYPE_END; action++) {
		action_type = action->type;
		switch (action_type) {
		case RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR:
		case RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT:
			if (!fwd_vsi)
				fwd_vsi = true;
			else
				goto err;

			act_ethdev = action->conf;
			dst_itf = cpfl_get_itf_by_port_id(act_ethdev->port_id);

			if (!dst_itf)
				goto err;

			if (dst_itf->type == CPFL_ITF_TYPE_VPORT) {
				vport = (struct cpfl_vport *)dst_itf;
				queue_id = vport->base.chunks_info.rx_start_qid;
			} else {
				queue_id = CPFL_INVALID_QUEUE_ID;
			}

			is_vsi = (action_type == RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR ||
				  dst_itf->type == CPFL_ITF_TYPE_REPRESENTOR);
			if (is_vsi)
				dev_id = cpfl_get_vsi_id(dst_itf);
			else
				dev_id = cpfl_get_port_id(dst_itf);

			if (dev_id == CPFL_INVALID_HW_ID)
				goto err;

			if (is_vsi)
				*act_set = cpfl_act_fwd_vsi(0, priority, 0, dev_id);
			else
				*act_set = cpfl_act_fwd_port(0, priority, 0, dev_id);
			act_set++;
			rinfo->act_byte_len += sizeof(union cpfl_action_set);
			break;
		case RTE_FLOW_ACTION_TYPE_QUEUE:
			if (!fwd_q)
				fwd_q = true;
			else
				goto err;
			if (queue_id == CPFL_INVALID_QUEUE_ID)
				goto err;
			act_q = action->conf;
			data = itf->data;
			if (act_q->index >= data->nb_rx_queues)
				goto err;

			vport = (struct cpfl_vport *)itf;
			if (queue_id < 0)
				queue_id = vport->base.chunks_info.rx_start_qid;
			queue_id += act_q->index;
			*act_set = cpfl_act_set_hash_queue(priority, 0, queue_id, 0);
			act_set++;
			rinfo->act_byte_len += sizeof(union cpfl_action_set);
			break;
		case RTE_FLOW_ACTION_TYPE_RSS:
			rss = action->conf;
			if (rss->queue_num <= 1)
				goto err;
			for (i = 0; i < rss->queue_num - 1; i++) {
				if (rss->queue[i + 1] != rss->queue[i] + 1)
					goto err;
			}
			data = itf->data;
			if (rss->queue[rss->queue_num - 1] >= data->nb_rx_queues)
				goto err;
			if (!(rte_is_power_of_2(rss->queue_num) &&
			      rss->queue_num <= CPFL_FXP_MAX_QREGION_SIZE))
				goto err;

			if (!fwd_q)
				fwd_q = true;
			else
				goto err;
			if (queue_id == CPFL_INVALID_QUEUE_ID)
				goto err;
			vport = (struct cpfl_vport *)itf;
			if (queue_id < 0)
				queue_id = vport->base.chunks_info.rx_start_qid;
			queue_id += rss->queue[0];
			*act_set = cpfl_act_set_hash_queue_region(priority, 0, queue_id,
								  log(rss->queue_num) / log(2), 0);
			act_set++;
			rinfo->act_byte_len += sizeof(union cpfl_action_set);
			break;
		case RTE_FLOW_ACTION_TYPE_DROP:
			(*act_set).data = cpfl_act_drop(priority).data;
			act_set++;
			rinfo->act_byte_len += sizeof(union cpfl_action_set);
			(*act_set).data = cpfl_act_set_commit_mode(priority, 0).data;
			act_set++;
			rinfo->act_byte_len += sizeof(union cpfl_action_set);
			break;
		case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP:
		case RTE_FLOW_ACTION_TYPE_VXLAN_DECAP:
		case RTE_FLOW_ACTION_TYPE_PROG:
			break;
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		default:
			goto err;
		}
	}

	if (mr_action) {
		uint32_t i;

		for (i = 0; i < rim->mr_num; i++)
			if (cpfl_parse_mod_content(itf->adapter, rinfo,
						   &rim->rules[rim->pr_num + i],
						   &mr_action[i]))
				goto err;
	}

	return 0;

err:
	PMD_DRV_LOG(ERR, "Invalid action type");
	return -EINVAL;
}

static void
cpfl_fill_rinfo_default_value(struct cpfl_rule_info *rinfo)
{
	if (cpfl_rule_cookie == ~0llu)
		cpfl_rule_cookie = CPFL_COOKIE_DEF;
	rinfo->cookie = cpfl_rule_cookie++;
	rinfo->host_id = CPFL_HOST_ID_DEF;
	rinfo->port_num = CPFL_PORT_NUM_DEF;
	rinfo->resp_req = CPFL_RESP_REQ_DEF;
	rinfo->clear_mirror_1st_state = CPFL_CLEAR_MIRROR_1ST_STATE_DEF;
}

static bool
cpfl_is_mod_action(const struct rte_flow_action actions[])
{
	const struct rte_flow_action *action;
	enum rte_flow_action_type action_type;

	if (!actions || actions->type == RTE_FLOW_ACTION_TYPE_END)
		return false;

	for (action = actions; action->type !=
			RTE_FLOW_ACTION_TYPE_END; action++) {
		action_type = action->type;
		switch (action_type) {
		case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP:
		case RTE_FLOW_ACTION_TYPE_VXLAN_DECAP:
		case RTE_FLOW_ACTION_TYPE_PROG:
			return true;
		default:
			continue;
		}
	}
	return false;
}

static bool
cpfl_fxp_get_metadata_port(struct cpfl_itf *itf,
			   const struct rte_flow_action actions[])
{
	const struct rte_flow_action *action;
	enum rte_flow_action_type action_type;
	const struct rte_flow_action_ethdev *ethdev;
	struct cpfl_itf *target_itf;
	bool ret;

	if (itf->type == CPFL_ITF_TYPE_VPORT) {
		ret = cpfl_metadata_write_port_id(itf);
		if (!ret) {
			PMD_DRV_LOG(ERR, "fail to write port id");
			return false;
		}
	}

	ret = cpfl_metadata_write_sourcevsi(itf);
	if (!ret) {
		PMD_DRV_LOG(ERR, "fail to write source vsi id");
		return false;
	}

	ret = cpfl_metadata_write_vsi(itf);
	if (!ret) {
		PMD_DRV_LOG(ERR, "fail to write vsi id");
		return false;
	}

	if (!actions || actions->type == RTE_FLOW_ACTION_TYPE_END)
		return false;

	for (action = actions; action->type != RTE_FLOW_ACTION_TYPE_END; action++) {
		action_type = action->type;
		switch (action_type) {
		case RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT:
		case RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR:
			ethdev = (const struct rte_flow_action_ethdev *)action->conf;
			target_itf = cpfl_get_itf_by_port_id(ethdev->port_id);
			if (!target_itf) {
				PMD_DRV_LOG(ERR, "fail to get target_itf by port id");
				return false;
			}
			ret = cpfl_metadata_write_targetvsi(target_itf);
			if (!ret) {
				PMD_DRV_LOG(ERR, "fail to write target vsi id");
				return false;
			}
			break;
		default:
			continue;
		}
	}

	return true;
}

static int
cpfl_fxp_parse_pattern_action(struct rte_eth_dev *dev,
			      const struct rte_flow_attr *attr,
			      const struct rte_flow_item pattern[],
			      const struct rte_flow_action actions[],
			      void **meta)
{
	struct cpfl_itf *itf = CPFL_DEV_TO_ITF(dev);
	struct cpfl_flow_pr_action pr_action = { 0 };
	struct cpfl_adapter_ext *adapter = itf->adapter;
	struct cpfl_flow_mr_action mr_action[CPFL_MAX_MR_ACTION_NUM] = { 0 };
	uint32_t pr_num = 0;
	uint32_t mr_num = 0;
	struct cpfl_rule_info_meta *rim;
	int ret;

	ret = cpfl_fxp_get_metadata_port(itf, actions);
	if (!ret) {
		PMD_DRV_LOG(ERR, "Fail to save metadata.");
		return -EINVAL;
	}

	ret = cpfl_flow_parse_items(itf, adapter->flow_parser, pattern, attr, &pr_action);
	if (ret) {
		PMD_DRV_LOG(ERR, "No Match pattern support.");
		return -EINVAL;
	}

	if (cpfl_is_mod_action(actions)) {
		ret = cpfl_flow_parse_actions(adapter->flow_parser, actions, mr_action);
		if (ret) {
			PMD_DRV_LOG(ERR, "action parse fails.");
			return -EINVAL;
		}
		mr_num++;
	}

	pr_num = 1;
	rim = rte_zmalloc(NULL,
			  sizeof(struct cpfl_rule_info_meta) +
			  (pr_num + mr_num) * sizeof(struct cpfl_rule_info),
			  0);
	if (!rim)
		return -ENOMEM;

	rim->pr_action = pr_action;
	rim->pr_num = pr_num;
	rim->mr_num = mr_num;
	rim->rule_num = pr_num + mr_num;

	if (!cpfl_fxp_parse_pattern(&pr_action, rim, 0)) {
		PMD_DRV_LOG(ERR, "Invalid pattern");
		rte_free(rim);
		return -rte_errno;
	}

	if (cpfl_fxp_parse_action(itf, actions, mr_action, rim, attr->priority, 0)) {
		PMD_DRV_LOG(ERR, "Invalid action");
		rte_free(rim);
		return -rte_errno;
	}

	cpfl_fill_rinfo_default_value(&rim->rules[0]);

	if (!meta)
		rte_free(rim);
	else
		*meta = rim;

	return 0;
}

static int
cpfl_fxp_mod_init(struct cpfl_adapter_ext *ad)
{
	uint32_t size = rte_bitmap_get_memory_footprint(CPFL_MAX_MOD_CONTENT_INDEX);
	void *mem = rte_zmalloc(NULL, size, RTE_CACHE_LINE_SIZE);

	if (!mem)
		return -ENOMEM;

	/* a set bit represent a free slot */
	ad->mod_bm = rte_bitmap_init_with_all_set(CPFL_MAX_MOD_CONTENT_INDEX, mem, size);
	if (!ad->mod_bm) {
		rte_free(mem);
		return -EINVAL;
	}

	ad->mod_bm_mem = mem;

	return 0;
}

static void
cpfl_fxp_mod_uninit(struct cpfl_adapter_ext *ad)
{
	rte_free(ad->mod_bm_mem);
	ad->mod_bm_mem = NULL;
	ad->mod_bm = NULL;
}

static uint32_t
cpfl_fxp_mod_idx_alloc(struct cpfl_adapter_ext *ad)
{
	uint64_t slab = 0;
	uint32_t pos = 0;

	if (!rte_bitmap_scan(ad->mod_bm, &pos, &slab))
		return CPFL_MAX_MOD_CONTENT_INDEX;

	pos += __builtin_ffsll(slab) - 1;
	rte_bitmap_clear(ad->mod_bm, pos);

	return pos;
}

static void
cpfl_fxp_mod_idx_free(struct cpfl_adapter_ext *ad, uint32_t idx)
{
	rte_bitmap_set(ad->mod_bm, idx);
}

static int
cpfl_fxp_query(struct rte_eth_dev *dev __rte_unused,
	       struct rte_flow *flow __rte_unused,
	       struct rte_flow_query_count *count __rte_unused,
	       struct rte_flow_error *error)
{
	rte_flow_error_set(error, EINVAL,
			   RTE_FLOW_ERROR_TYPE_HANDLE,
			   NULL,
			   "count action not supported by this module");

	return -rte_errno;
}

static void
cpfl_fxp_uninit(struct cpfl_adapter_ext *ad)
{
	cpfl_fxp_mod_uninit(ad);
}

static int
cpfl_fxp_init(struct cpfl_adapter_ext *ad)
{
	int ret = 0;

	ret = cpfl_fxp_mod_init(ad);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to init mod content bitmap.");
		return ret;
	}

	return ret;
}

static struct
cpfl_flow_engine cpfl_fxp_engine = {
	.type = CPFL_FLOW_ENGINE_FXP,
	.init = cpfl_fxp_init,
	.uninit = cpfl_fxp_uninit,
	.create = cpfl_fxp_create,
	.destroy = cpfl_fxp_destroy,
	.query_count = cpfl_fxp_query,
	.parse_pattern_action = cpfl_fxp_parse_pattern_action,
};

RTE_INIT(cpfl_sw_engine_init)
{
	struct cpfl_flow_engine *engine = &cpfl_fxp_engine;

	cpfl_flow_engine_register(engine);
}
