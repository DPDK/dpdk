/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

#define NIX_MAX_BPF_COUNT_LEAF_LAYER 64
#define NIX_MAX_BPF_COUNT_MID_LAYER  8
#define NIX_MAX_BPF_COUNT_TOP_LAYER  1

#define NIX_BPF_LEVEL_F_MASK                                                   \
	(ROC_NIX_BPF_LEVEL_F_LEAF | ROC_NIX_BPF_LEVEL_F_MID |                  \
	 ROC_NIX_BPF_LEVEL_F_TOP)

static uint8_t sw_to_hw_lvl_map[] = {NIX_RX_BAND_PROF_LAYER_LEAF,
				     NIX_RX_BAND_PROF_LAYER_MIDDLE,
				     NIX_RX_BAND_PROF_LAYER_TOP};

static inline struct mbox *
get_mbox(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;

	return dev->mbox;
}

static inline uint64_t
meter_rate_to_nix(uint64_t value, uint64_t *exponent_p, uint64_t *mantissa_p,
		  uint64_t *div_exp_p)
{
	uint64_t div_exp, exponent, mantissa;

	/* Boundary checks */
	if (value < NIX_BPF_RATE_MIN || value > NIX_BPF_RATE_MAX)
		return 0;

	if (value <= NIX_BPF_RATE(0, 0, 0)) {
		/* Calculate rate div_exp and mantissa using
		 * the following formula:
		 *
		 * value = (2E6 * (256 + mantissa)
		 *              / ((1 << div_exp) * 256))
		 */
		div_exp = 0;
		exponent = 0;
		mantissa = NIX_BPF_MAX_RATE_MANTISSA;

		while (value < (NIX_BPF_RATE_CONST / (1 << div_exp)))
			div_exp += 1;

		while (value < ((NIX_BPF_RATE_CONST * (256 + mantissa)) /
				((1 << div_exp) * 256)))
			mantissa -= 1;
	} else {
		/* Calculate rate exponent and mantissa using
		 * the following formula:
		 *
		 * value = (2E6 * ((256 + mantissa) << exponent)) / 256
		 *
		 */
		div_exp = 0;
		exponent = NIX_BPF_MAX_RATE_EXPONENT;
		mantissa = NIX_BPF_MAX_RATE_MANTISSA;

		while (value < (NIX_BPF_RATE_CONST * (1 << exponent)))
			exponent -= 1;

		while (value <
		       ((NIX_BPF_RATE_CONST * ((256 + mantissa) << exponent)) /
			256))
			mantissa -= 1;
	}

	if (div_exp > NIX_BPF_MAX_RATE_DIV_EXP ||
	    exponent > NIX_BPF_MAX_RATE_EXPONENT ||
	    mantissa > NIX_BPF_MAX_RATE_MANTISSA)
		return 0;

	if (div_exp_p)
		*div_exp_p = div_exp;
	if (exponent_p)
		*exponent_p = exponent;
	if (mantissa_p)
		*mantissa_p = mantissa;

	/* Calculate real rate value */
	return NIX_BPF_RATE(exponent, mantissa, div_exp);
}

static inline uint64_t
meter_burst_to_nix(uint64_t value, uint64_t *exponent_p, uint64_t *mantissa_p)
{
	uint64_t exponent, mantissa;

	if (value < NIX_BPF_BURST_MIN || value > NIX_BPF_BURST_MAX)
		return 0;

	/* Calculate burst exponent and mantissa using
	 * the following formula:
	 *
	 * value = (((256 + mantissa) << (exponent + 1)
	 / 256)
	 *
	 */
	exponent = NIX_BPF_MAX_BURST_EXPONENT;
	mantissa = NIX_BPF_MAX_BURST_MANTISSA;

	while (value < (1ull << (exponent + 1)))
		exponent -= 1;

	while (value < ((256 + mantissa) << (exponent + 1)) / 256)
		mantissa -= 1;

	if (exponent > NIX_BPF_MAX_BURST_EXPONENT ||
	    mantissa > NIX_BPF_MAX_BURST_MANTISSA)
		return 0;

	if (exponent_p)
		*exponent_p = exponent;
	if (mantissa_p)
		*mantissa_p = mantissa;

	return NIX_BPF_BURST(exponent, mantissa);
}

uint8_t
roc_nix_bpf_level_to_idx(enum roc_nix_bpf_level_flag level_f)
{
	uint8_t idx;

	if (level_f & ROC_NIX_BPF_LEVEL_F_LEAF)
		idx = 0;
	else if (level_f & ROC_NIX_BPF_LEVEL_F_MID)
		idx = 1;
	else if (level_f & ROC_NIX_BPF_LEVEL_F_TOP)
		idx = 2;
	else
		idx = ROC_NIX_BPF_LEVEL_IDX_INVALID;
	return idx;
}

int
roc_nix_bpf_count_get(struct roc_nix *roc_nix, uint8_t lvl_mask,
		      uint16_t count[ROC_NIX_BPF_LEVEL_MAX])
{
	uint8_t mask = lvl_mask & NIX_BPF_LEVEL_F_MASK;
	uint8_t leaf_idx, mid_idx, top_idx;

	PLT_SET_USED(roc_nix);

	if (roc_model_is_cn9k())
		return NIX_ERR_HW_NOTSUP;

	if (!mask)
		return NIX_ERR_PARAM;

	/* Currently No MBOX interface is available to get number
	 * of bandwidth profiles. So numbers per level are hard coded,
	 * considering 3 RPM blocks and each block has 4 LMAC's.
	 * So total 12 physical interfaces are in system. Each interface
	 * supports following bandwidth profiles.
	 */

	leaf_idx = roc_nix_bpf_level_to_idx(mask & ROC_NIX_BPF_LEVEL_F_LEAF);
	mid_idx = roc_nix_bpf_level_to_idx(mask & ROC_NIX_BPF_LEVEL_F_MID);
	top_idx = roc_nix_bpf_level_to_idx(mask & ROC_NIX_BPF_LEVEL_F_TOP);

	if (leaf_idx != ROC_NIX_BPF_LEVEL_IDX_INVALID)
		count[leaf_idx] = NIX_MAX_BPF_COUNT_LEAF_LAYER;

	if (mid_idx != ROC_NIX_BPF_LEVEL_IDX_INVALID)
		count[mid_idx] = NIX_MAX_BPF_COUNT_MID_LAYER;

	if (top_idx != ROC_NIX_BPF_LEVEL_IDX_INVALID)
		count[top_idx] = NIX_MAX_BPF_COUNT_TOP_LAYER;

	return 0;
}

int
roc_nix_bpf_alloc(struct roc_nix *roc_nix, uint8_t lvl_mask,
		  uint16_t per_lvl_cnt[ROC_NIX_BPF_LEVEL_MAX],
		  struct roc_nix_bpf_objs *profs)
{
	uint8_t mask = lvl_mask & NIX_BPF_LEVEL_F_MASK;
	struct mbox *mbox = get_mbox(roc_nix);
	struct nix_bandprof_alloc_req *req;
	struct nix_bandprof_alloc_rsp *rsp;
	uint8_t leaf_idx, mid_idx, top_idx;
	int rc = -ENOSPC, i;

	if (roc_model_is_cn9k())
		return NIX_ERR_HW_NOTSUP;

	if (!mask)
		return NIX_ERR_PARAM;

	leaf_idx = roc_nix_bpf_level_to_idx(mask & ROC_NIX_BPF_LEVEL_F_LEAF);
	mid_idx = roc_nix_bpf_level_to_idx(mask & ROC_NIX_BPF_LEVEL_F_MID);
	top_idx = roc_nix_bpf_level_to_idx(mask & ROC_NIX_BPF_LEVEL_F_TOP);

	if ((leaf_idx != ROC_NIX_BPF_LEVEL_IDX_INVALID) &&
	    (per_lvl_cnt[leaf_idx] > NIX_MAX_BPF_COUNT_LEAF_LAYER))
		return NIX_ERR_INVALID_RANGE;

	if ((mid_idx != ROC_NIX_BPF_LEVEL_IDX_INVALID) &&
	    (per_lvl_cnt[mid_idx] > NIX_MAX_BPF_COUNT_MID_LAYER))
		return NIX_ERR_INVALID_RANGE;

	if ((top_idx != ROC_NIX_BPF_LEVEL_IDX_INVALID) &&
	    (per_lvl_cnt[top_idx] > NIX_MAX_BPF_COUNT_TOP_LAYER))
		return NIX_ERR_INVALID_RANGE;

	req = mbox_alloc_msg_nix_bandprof_alloc(mbox);
	if (req == NULL)
		goto exit;

	if (leaf_idx != ROC_NIX_BPF_LEVEL_IDX_INVALID) {
		req->prof_count[sw_to_hw_lvl_map[leaf_idx]] =
			per_lvl_cnt[leaf_idx];
	}

	if (mid_idx != ROC_NIX_BPF_LEVEL_IDX_INVALID) {
		req->prof_count[sw_to_hw_lvl_map[mid_idx]] =
			per_lvl_cnt[mid_idx];
	}

	if (top_idx != ROC_NIX_BPF_LEVEL_IDX_INVALID) {
		req->prof_count[sw_to_hw_lvl_map[top_idx]] =
			per_lvl_cnt[top_idx];
	}

	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		goto exit;

	if (leaf_idx != ROC_NIX_BPF_LEVEL_IDX_INVALID) {
		profs[leaf_idx].level = leaf_idx;
		profs[leaf_idx].count =
			rsp->prof_count[sw_to_hw_lvl_map[leaf_idx]];
		for (i = 0; i < profs[leaf_idx].count; i++) {
			profs[leaf_idx].ids[i] =
				rsp->prof_idx[sw_to_hw_lvl_map[leaf_idx]][i];
		}
	}

	if (mid_idx != ROC_NIX_BPF_LEVEL_IDX_INVALID) {
		profs[mid_idx].level = mid_idx;
		profs[mid_idx].count =
			rsp->prof_count[sw_to_hw_lvl_map[mid_idx]];
		for (i = 0; i < profs[mid_idx].count; i++) {
			profs[mid_idx].ids[i] =
				rsp->prof_idx[sw_to_hw_lvl_map[mid_idx]][i];
		}
	}

	if (top_idx != ROC_NIX_BPF_LEVEL_IDX_INVALID) {
		profs[top_idx].level = top_idx;
		profs[top_idx].count =
			rsp->prof_count[sw_to_hw_lvl_map[top_idx]];
		for (i = 0; i < profs[top_idx].count; i++) {
			profs[top_idx].ids[i] =
				rsp->prof_idx[sw_to_hw_lvl_map[top_idx]][i];
		}
	}

exit:
	return rc;
}

int
roc_nix_bpf_free(struct roc_nix *roc_nix, struct roc_nix_bpf_objs *profs,
		 uint8_t num_prof)
{
	struct mbox *mbox = get_mbox(roc_nix);
	struct nix_bandprof_free_req *req;
	uint8_t level;
	int i, j;

	if (num_prof >= NIX_RX_BAND_PROF_LAYER_MAX)
		return NIX_ERR_INVALID_RANGE;

	req = mbox_alloc_msg_nix_bandprof_free(mbox);
	if (req == NULL)
		return -ENOSPC;

	for (i = 0; i < num_prof; i++) {
		level = sw_to_hw_lvl_map[profs[i].level];
		req->prof_count[level] = profs[i].count;
		for (j = 0; j < profs[i].count; j++)
			req->prof_idx[level][j] = profs[i].ids[j];
	}

	return mbox_process(mbox);
}

int
roc_nix_bpf_free_all(struct roc_nix *roc_nix)
{
	struct mbox *mbox = get_mbox(roc_nix);
	struct nix_bandprof_free_req *req;

	req = mbox_alloc_msg_nix_bandprof_free(mbox);
	if (req == NULL)
		return -ENOSPC;

	req->free_all = true;
	return mbox_process(mbox);
}

int
roc_nix_bpf_config(struct roc_nix *roc_nix, uint16_t id,
		   enum roc_nix_bpf_level_flag lvl_flag,
		   struct roc_nix_bpf_cfg *cfg)
{
	uint64_t exponent_p = 0, mantissa_p = 0, div_exp_p = 0;
	struct mbox *mbox = get_mbox(roc_nix);
	struct nix_cn10k_aq_enq_req *aq;
	uint8_t level_idx;

	if (roc_model_is_cn9k())
		return NIX_ERR_HW_NOTSUP;

	if (!cfg)
		return NIX_ERR_PARAM;

	level_idx = roc_nix_bpf_level_to_idx(lvl_flag);
	if (level_idx == ROC_NIX_BPF_LEVEL_IDX_INVALID)
		return NIX_ERR_PARAM;

	aq = mbox_alloc_msg_nix_cn10k_aq_enq(mbox);
	if (aq == NULL)
		return -ENOSPC;
	aq->qidx = (sw_to_hw_lvl_map[level_idx] << 14) | id;
	aq->ctype = NIX_AQ_CTYPE_BAND_PROF;
	aq->op = NIX_AQ_INSTOP_WRITE;

	aq->prof.adjust_exponent = NIX_BPF_DEFAULT_ADJUST_EXPONENT;
	aq->prof.adjust_mantissa = NIX_BPF_DEFAULT_ADJUST_MANTISSA;
	if (cfg->lmode == ROC_NIX_BPF_LMODE_BYTE)
		aq->prof.adjust_mantissa = NIX_BPF_DEFAULT_ADJUST_MANTISSA / 2;

	aq->prof_mask.adjust_exponent = ~(aq->prof_mask.adjust_exponent);
	aq->prof_mask.adjust_mantissa = ~(aq->prof_mask.adjust_mantissa);

	switch (cfg->alg) {
	case ROC_NIX_BPF_ALGO_2697:
		meter_rate_to_nix(cfg->algo2697.cir, &exponent_p, &mantissa_p,
				  &div_exp_p);
		aq->prof.cir_mantissa = mantissa_p;
		aq->prof.cir_exponent = exponent_p;

		meter_burst_to_nix(cfg->algo2697.cbs, &exponent_p, &mantissa_p);
		aq->prof.cbs_mantissa = mantissa_p;
		aq->prof.cbs_exponent = exponent_p;

		meter_burst_to_nix(cfg->algo2697.ebs, &exponent_p, &mantissa_p);
		aq->prof.pebs_mantissa = mantissa_p;
		aq->prof.pebs_exponent = exponent_p;

		aq->prof_mask.cir_mantissa = ~(aq->prof_mask.cir_mantissa);
		aq->prof_mask.cbs_mantissa = ~(aq->prof_mask.cbs_mantissa);
		aq->prof_mask.pebs_mantissa = ~(aq->prof_mask.pebs_mantissa);
		aq->prof_mask.cir_exponent = ~(aq->prof_mask.cir_exponent);
		aq->prof_mask.cbs_exponent = ~(aq->prof_mask.cbs_exponent);
		aq->prof_mask.pebs_exponent = ~(aq->prof_mask.pebs_exponent);
		break;

	case ROC_NIX_BPF_ALGO_2698:
		meter_rate_to_nix(cfg->algo2698.cir, &exponent_p, &mantissa_p,
				  &div_exp_p);
		aq->prof.cir_mantissa = mantissa_p;
		aq->prof.cir_exponent = exponent_p;

		meter_rate_to_nix(cfg->algo2698.pir, &exponent_p, &mantissa_p,
				  &div_exp_p);
		aq->prof.peir_mantissa = mantissa_p;
		aq->prof.peir_exponent = exponent_p;

		meter_burst_to_nix(cfg->algo2698.cbs, &exponent_p, &mantissa_p);
		aq->prof.cbs_mantissa = mantissa_p;
		aq->prof.cbs_exponent = exponent_p;

		meter_burst_to_nix(cfg->algo2698.pbs, &exponent_p, &mantissa_p);
		aq->prof.pebs_mantissa = mantissa_p;
		aq->prof.pebs_exponent = exponent_p;

		aq->prof_mask.cir_mantissa = ~(aq->prof_mask.cir_mantissa);
		aq->prof_mask.peir_mantissa = ~(aq->prof_mask.peir_mantissa);
		aq->prof_mask.cbs_mantissa = ~(aq->prof_mask.cbs_mantissa);
		aq->prof_mask.pebs_mantissa = ~(aq->prof_mask.pebs_mantissa);
		aq->prof_mask.cir_exponent = ~(aq->prof_mask.cir_exponent);
		aq->prof_mask.peir_exponent = ~(aq->prof_mask.peir_exponent);
		aq->prof_mask.cbs_exponent = ~(aq->prof_mask.cbs_exponent);
		aq->prof_mask.pebs_exponent = ~(aq->prof_mask.pebs_exponent);
		break;

	case ROC_NIX_BPF_ALGO_4115:
		meter_rate_to_nix(cfg->algo4115.cir, &exponent_p, &mantissa_p,
				  &div_exp_p);
		aq->prof.cir_mantissa = mantissa_p;
		aq->prof.cir_exponent = exponent_p;

		meter_rate_to_nix(cfg->algo4115.eir, &exponent_p, &mantissa_p,
				  &div_exp_p);
		aq->prof.peir_mantissa = mantissa_p;
		aq->prof.peir_exponent = exponent_p;

		meter_burst_to_nix(cfg->algo4115.cbs, &exponent_p, &mantissa_p);
		aq->prof.cbs_mantissa = mantissa_p;
		aq->prof.cbs_exponent = exponent_p;

		meter_burst_to_nix(cfg->algo4115.ebs, &exponent_p, &mantissa_p);
		aq->prof.pebs_mantissa = mantissa_p;
		aq->prof.pebs_exponent = exponent_p;

		aq->prof_mask.cir_mantissa = ~(aq->prof_mask.cir_mantissa);
		aq->prof_mask.peir_mantissa = ~(aq->prof_mask.peir_mantissa);
		aq->prof_mask.cbs_mantissa = ~(aq->prof_mask.cbs_mantissa);
		aq->prof_mask.pebs_mantissa = ~(aq->prof_mask.pebs_mantissa);

		aq->prof_mask.cir_exponent = ~(aq->prof_mask.cir_exponent);
		aq->prof_mask.peir_exponent = ~(aq->prof_mask.peir_exponent);
		aq->prof_mask.cbs_exponent = ~(aq->prof_mask.cbs_exponent);
		aq->prof_mask.pebs_exponent = ~(aq->prof_mask.pebs_exponent);
		break;

	default:
		return NIX_ERR_PARAM;
	}

	aq->prof.lmode = cfg->lmode;
	aq->prof.icolor = cfg->icolor;
	aq->prof.pc_mode = cfg->pc_mode;
	aq->prof.tnl_ena = cfg->tnl_ena;
	aq->prof.gc_action = cfg->action[ROC_NIX_BPF_COLOR_GREEN];
	aq->prof.yc_action = cfg->action[ROC_NIX_BPF_COLOR_YELLOW];
	aq->prof.rc_action = cfg->action[ROC_NIX_BPF_COLOR_RED];

	aq->prof_mask.lmode = ~(aq->prof_mask.lmode);
	aq->prof_mask.icolor = ~(aq->prof_mask.icolor);
	aq->prof_mask.pc_mode = ~(aq->prof_mask.pc_mode);
	aq->prof_mask.tnl_ena = ~(aq->prof_mask.tnl_ena);
	aq->prof_mask.gc_action = ~(aq->prof_mask.gc_action);
	aq->prof_mask.yc_action = ~(aq->prof_mask.yc_action);
	aq->prof_mask.rc_action = ~(aq->prof_mask.rc_action);

	return mbox_process(mbox);
}

int
roc_nix_bpf_ena_dis(struct roc_nix *roc_nix, uint16_t id, struct roc_nix_rq *rq,
		    bool enable)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct mbox *mbox = get_mbox(roc_nix);
	struct nix_cn10k_aq_enq_req *aq;
	int rc;

	if (roc_model_is_cn9k())
		return NIX_ERR_HW_NOTSUP;

	if (rq->qid >= nix->nb_rx_queues)
		return NIX_ERR_QUEUE_INVALID_RANGE;

	aq = mbox_alloc_msg_nix_cn10k_aq_enq(mbox);
	if (aq == NULL)
		return -ENOSPC;
	aq->qidx = rq->qid;
	aq->ctype = NIX_AQ_CTYPE_RQ;
	aq->op = NIX_AQ_INSTOP_WRITE;

	aq->rq.policer_ena = enable;
	aq->rq_mask.policer_ena = ~(aq->rq_mask.policer_ena);
	if (enable) {
		aq->rq.band_prof_id = id;
		aq->rq_mask.band_prof_id = ~(aq->rq_mask.band_prof_id);
	}

	rc = mbox_process(mbox);
	if (rc)
		goto exit;

	rq->bpf_id = id;

exit:
	return rc;
}
