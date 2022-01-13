/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

#define NIX_MAX_BPF_COUNT_LEAF_LAYER 64
#define NIX_MAX_BPF_COUNT_MID_LAYER  8
#define NIX_MAX_BPF_COUNT_TOP_LAYER  1

#define NIX_BPF_PRECOLOR_GEN_TABLE_SIZE	 16
#define NIX_BPF_PRECOLOR_VLAN_TABLE_SIZE 16
#define NIX_BPF_PRECOLOR_DSCP_TABLE_SIZE 64

#define NIX_BPF_LEVEL_F_MASK                                                   \
	(ROC_NIX_BPF_LEVEL_F_LEAF | ROC_NIX_BPF_LEVEL_F_MID |                  \
	 ROC_NIX_BPF_LEVEL_F_TOP)

#define NIX_RD_STATS(val)  plt_read64(nix->base + NIX_LF_RX_STATX(val))
#define NIX_RST_STATS(val) plt_write64(0, nix->base + NIX_LF_RX_STATX(val))

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
		  uint64_t *div_exp_p, uint32_t timeunit_p)
{
	uint64_t div_exp, exponent, mantissa;
	uint32_t time_ns = timeunit_p;

	/* Boundary checks */
	if (value < NIX_BPF_RATE(time_ns, 0, 0, 0) ||
	    value > NIX_BPF_RATE(time_ns, NIX_BPF_MAX_RATE_EXPONENT,
				 NIX_BPF_MAX_RATE_MANTISSA, 0))
		return 0;

	div_exp = 0;
	exponent = NIX_BPF_MAX_RATE_EXPONENT;
	mantissa = NIX_BPF_MAX_RATE_MANTISSA;

	while (value < (NIX_BPF_RATE(time_ns, exponent, 0, 0)))
		exponent -= 1;

	while (value < (NIX_BPF_RATE(time_ns, exponent, mantissa, 0)))
		mantissa -= 1;

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
	return NIX_BPF_RATE(time_ns, exponent, mantissa, div_exp);
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

static inline void
nix_lf_bpf_dump(__io struct nix_band_prof_s *bpf)
{
	plt_dump("W0: cir_mantissa  \t\t\t%d\nW0: pebs_mantissa \t\t\t0x%03x",
		 bpf->cir_mantissa, bpf->pebs_mantissa);
	plt_dump("W0: peir_mantissa \t\t\t\t%d\nW0: cbs_exponent \t\t\t%d",
		 bpf->peir_mantissa, bpf->cbs_exponent);
	plt_dump("W0: cir_exponent \t\t\t%d\nW0: pebs_exponent \t\t\t%d",
		 bpf->cir_exponent, bpf->pebs_exponent);
	plt_dump("W0: peir_exponent \t\t\t%d\n", bpf->peir_exponent);
	plt_dump("W0: tnl_ena \t\t\t%d\n", bpf->tnl_ena);
	plt_dump("W0: icolor \t\t\t%d\n", bpf->icolor);
	plt_dump("W0: pc_mode \t\t\t%d\n", bpf->pc_mode);
	plt_dump("W1: hl_en \t\t%d\nW1: band_prof_id \t\t%d", bpf->hl_en,
		 bpf->band_prof_id);
	plt_dump("W1: meter_algo \t\t%d\nW1: rc_action \t\t%d", bpf->meter_algo,
		 bpf->rc_action);
	plt_dump("W1: yc_action \t\t\t%d\nW1: gc_action \t\t\t%d",
		 bpf->yc_action, bpf->gc_action);
	plt_dump("W1: adjust_mantissa\t\t\t%d\nW1: adjust_exponent \t\t\t%d",
		 bpf->adjust_mantissa, bpf->adjust_exponent);
	plt_dump("W1: rdiv \t\t\t%d\n", bpf->rdiv);
	plt_dump("W1: l_select \t\t%d\nW2: lmode \t\t%d", bpf->l_sellect,
		 bpf->lmode);
	plt_dump("W1: cbs_mantissa \t\t\t%d\n", bpf->cbs_mantissa);
	plt_dump("W2: tsa \t\t\t0x%" PRIx64 "\n", (uint64_t)bpf->ts);
	plt_dump("W3: c_accum \t\t%d\nW3: pe_accum \t\t%d", bpf->c_accum,
		 bpf->pe_accum);
	plt_dump("W4: green_pkt_pass \t\t\t0x%" PRIx64 "",
		 (uint64_t)bpf->green_pkt_pass);
	plt_dump("W5: yellow_pkt_pass \t\t\t0x%" PRIx64 "",
		 (uint64_t)bpf->yellow_pkt_pass);
	plt_dump("W6: red_pkt_pass \t\t\t0x%" PRIx64 "",
		 (uint64_t)bpf->red_pkt_pass);
	plt_dump("W7: green_octs_pass \t\t\t0x%" PRIx64 "",
		 (uint64_t)bpf->green_octs_pass);
	plt_dump("W8: yellow_octs_pass \t\t\t0x%" PRIx64 "",
		 (uint64_t)bpf->yellow_octs_pass);
	plt_dump("W9: red_octs_pass \t\t\t0x%" PRIx64 "",
		 (uint64_t)bpf->red_octs_pass);
	plt_dump("W10: green_pkt_drop \t\t\t0x%" PRIx64 "",
		 (uint64_t)bpf->green_pkt_drop);
	plt_dump("W11: yellow_pkt_drop \t\t\t0x%" PRIx64 "",
		 (uint64_t)bpf->yellow_pkt_drop);
	plt_dump("W12: red_pkt_drop \t\t\t0x%" PRIx64 "",
		 (uint64_t)bpf->red_pkt_drop);
	plt_dump("W13: green_octs_drop \t\t\t0x%" PRIx64 "",
		 (uint64_t)bpf->green_octs_drop);
	plt_dump("W14: yellow_octs_drop \t\t\t0x%" PRIx64 "",
		 (uint64_t)bpf->yellow_octs_drop);
	plt_dump("W15: red_octs_drop \t\t\t0x%" PRIx64 "",
		 (uint64_t)bpf->red_octs_drop);
}

static inline void
nix_precolor_conv_table_write(struct roc_nix *roc_nix, uint64_t val,
			      uint32_t off)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	int64_t *addr;

	addr = PLT_PTR_ADD(nix->base, off);
	plt_write64(val, addr);
}

static uint8_t
nix_precolor_vlan_table_update(struct roc_nix *roc_nix,
			       struct roc_nix_bpf_precolor *tbl)
{
	uint64_t val = 0, i;
	uint8_t tn_ena;
	uint32_t off;

	for (i = 0; i < tbl->count; i++)
		val |= (((uint64_t)tbl->color[i]) << (2 * i));

	if (tbl->mode == ROC_NIX_BPF_PC_MODE_VLAN_INNER) {
		off = NIX_LF_RX_VLAN1_COLOR_CONV;
		tn_ena = true;
	} else {
		off = NIX_LF_RX_VLAN0_COLOR_CONV;
		tn_ena = false;
	}

	nix_precolor_conv_table_write(roc_nix, val, off);
	return tn_ena;
}

static uint8_t
nix_precolor_inner_dscp_table_update(struct roc_nix *roc_nix,
				     struct roc_nix_bpf_precolor *tbl)
{
	uint64_t val_lo = 0, val_hi = 0, i, j;

	for (i = 0, j = 0; i < (tbl->count / 2); i++, j++)
		val_lo |= (((uint64_t)tbl->color[i]) << (2 * j));

	for (j = 0; i < tbl->count; i++, j++)
		val_hi |= (((uint64_t)tbl->color[i]) << (2 * j));

	nix_precolor_conv_table_write(roc_nix, val_lo,
				      NIX_LF_RX_IIP_COLOR_CONV_LO);
	nix_precolor_conv_table_write(roc_nix, val_hi,
				      NIX_LF_RX_IIP_COLOR_CONV_HI);

	return true;
}

static uint8_t
nix_precolor_outer_dscp_table_update(struct roc_nix *roc_nix,
				     struct roc_nix_bpf_precolor *tbl)
{
	uint64_t val_lo = 0, val_hi = 0, i, j;

	for (i = 0, j = 0; i < (tbl->count / 2); i++, j++)
		val_lo |= (((uint64_t)tbl->color[i]) << (2 * j));

	for (j = 0; i < tbl->count; i++, j++)
		val_hi |= (((uint64_t)tbl->color[i]) << (2 * j));

	nix_precolor_conv_table_write(roc_nix, val_lo,
				      NIX_LF_RX_OIP_COLOR_CONV_LO);
	nix_precolor_conv_table_write(roc_nix, val_hi,
				      NIX_LF_RX_OIP_COLOR_CONV_HI);

	return false;
}

static uint8_t
nix_precolor_gen_table_update(struct roc_nix *roc_nix,
			      struct roc_nix_bpf_precolor *tbl)
{
	uint64_t val = 0, i;
	uint8_t tn_ena;
	uint32_t off;

	for (i = 0; i < tbl->count; i++)
		val |= (((uint64_t)tbl->color[i]) << (2 * i));

	if (tbl->mode == ROC_NIX_BPF_PC_MODE_GEN_INNER) {
		off = NIX_LF_RX_GEN_COLOR_CONVX(1);
		tn_ena = true;
	} else {
		off = NIX_LF_RX_GEN_COLOR_CONVX(0);
		tn_ena = false;
	}

	nix_precolor_conv_table_write(roc_nix, val, off);
	return tn_ena;
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

uint8_t
roc_nix_bpf_stats_to_idx(enum roc_nix_bpf_stats level_f)
{
	uint8_t idx;

	if (level_f & ROC_NIX_BPF_GREEN_PKT_F_PASS)
		idx = 0;
	else if (level_f & ROC_NIX_BPF_GREEN_OCTS_F_PASS)
		idx = 1;
	else if (level_f & ROC_NIX_BPF_GREEN_PKT_F_DROP)
		idx = 2;
	else if (level_f & ROC_NIX_BPF_GREEN_OCTS_F_DROP)
		idx = 3;
	else if (level_f & ROC_NIX_BPF_YELLOW_PKT_F_PASS)
		idx = 4;
	else if (level_f & ROC_NIX_BPF_YELLOW_OCTS_F_PASS)
		idx = 5;
	else if (level_f & ROC_NIX_BPF_YELLOW_PKT_F_DROP)
		idx = 6;
	else if (level_f & ROC_NIX_BPF_YELLOW_OCTS_F_DROP)
		idx = 7;
	else if (level_f & ROC_NIX_BPF_RED_PKT_F_PASS)
		idx = 8;
	else if (level_f & ROC_NIX_BPF_RED_OCTS_F_PASS)
		idx = 9;
	else if (level_f & ROC_NIX_BPF_RED_PKT_F_DROP)
		idx = 10;
	else if (level_f & ROC_NIX_BPF_RED_OCTS_F_DROP)
		idx = 11;
	else
		idx = ROC_NIX_BPF_STATS_MAX;
	return idx;
}

int
roc_nix_bpf_timeunit_get(struct roc_nix *roc_nix, uint32_t *time_unit)
{
	struct nix_bandprof_get_hwinfo_rsp *rsp;
	struct mbox *mbox = get_mbox(roc_nix);
	struct msg_req *req;
	int rc = -ENOSPC;

	if (roc_model_is_cn9k())
		return NIX_ERR_HW_NOTSUP;

	req = mbox_alloc_msg_nix_bandprof_get_hwinfo(mbox);
	if (req == NULL)
		goto exit;

	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		goto exit;

	*time_unit = rsp->policer_timeunit;

exit:
	return rc;
}

int
roc_nix_bpf_count_get(struct roc_nix *roc_nix, uint8_t lvl_mask,
		      uint16_t count[ROC_NIX_BPF_LEVEL_MAX])
{
	uint8_t mask = lvl_mask & NIX_BPF_LEVEL_F_MASK;
	struct nix_bandprof_get_hwinfo_rsp *rsp;
	struct mbox *mbox = get_mbox(roc_nix);
	uint8_t leaf_idx, mid_idx, top_idx;
	struct msg_req *req;
	int rc = -ENOSPC;

	if (roc_model_is_cn9k())
		return NIX_ERR_HW_NOTSUP;

	if (!mask)
		return NIX_ERR_PARAM;

	req = mbox_alloc_msg_nix_bandprof_get_hwinfo(mbox);
	if (req == NULL)
		goto exit;

	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		goto exit;

	leaf_idx = roc_nix_bpf_level_to_idx(mask & ROC_NIX_BPF_LEVEL_F_LEAF);
	mid_idx = roc_nix_bpf_level_to_idx(mask & ROC_NIX_BPF_LEVEL_F_MID);
	top_idx = roc_nix_bpf_level_to_idx(mask & ROC_NIX_BPF_LEVEL_F_TOP);

	if (leaf_idx != ROC_NIX_BPF_LEVEL_IDX_INVALID)
		count[leaf_idx] = rsp->prof_count[sw_to_hw_lvl_map[leaf_idx]];

	if (mid_idx != ROC_NIX_BPF_LEVEL_IDX_INVALID)
		count[mid_idx] = rsp->prof_count[sw_to_hw_lvl_map[mid_idx]];

	if (top_idx != ROC_NIX_BPF_LEVEL_IDX_INVALID)
		count[top_idx] = rsp->prof_count[sw_to_hw_lvl_map[top_idx]];

exit:
	return rc;
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
	uint32_t policer_timeunit;
	uint8_t level_idx;
	int rc;

	if (roc_model_is_cn9k())
		return NIX_ERR_HW_NOTSUP;

	if (!cfg)
		return NIX_ERR_PARAM;

	rc = roc_nix_bpf_timeunit_get(roc_nix, &policer_timeunit);
	if (rc)
		return rc;

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
				  &div_exp_p, policer_timeunit);
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
				  &div_exp_p, policer_timeunit);
		aq->prof.cir_mantissa = mantissa_p;
		aq->prof.cir_exponent = exponent_p;

		meter_rate_to_nix(cfg->algo2698.pir, &exponent_p, &mantissa_p,
				  &div_exp_p, policer_timeunit);
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
				  &div_exp_p, policer_timeunit);
		aq->prof.cir_mantissa = mantissa_p;
		aq->prof.cir_exponent = exponent_p;

		meter_rate_to_nix(cfg->algo4115.eir, &exponent_p, &mantissa_p,
				  &div_exp_p, policer_timeunit);
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
	aq->prof.meter_algo = cfg->alg;
	aq->prof.pc_mode = cfg->pc_mode;
	aq->prof.tnl_ena = cfg->tnl_ena;
	aq->prof.gc_action = cfg->action[ROC_NIX_BPF_COLOR_GREEN];
	aq->prof.yc_action = cfg->action[ROC_NIX_BPF_COLOR_YELLOW];
	aq->prof.rc_action = cfg->action[ROC_NIX_BPF_COLOR_RED];

	aq->prof_mask.lmode = ~(aq->prof_mask.lmode);
	aq->prof_mask.icolor = ~(aq->prof_mask.icolor);
	aq->prof_mask.meter_algo = ~(aq->prof_mask.meter_algo);
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

int
roc_nix_bpf_dump(struct roc_nix *roc_nix, uint16_t id,
		 enum roc_nix_bpf_level_flag lvl_flag)
{
	struct mbox *mbox = get_mbox(roc_nix);
	struct nix_cn10k_aq_enq_rsp *rsp;
	struct nix_cn10k_aq_enq_req *aq;
	uint8_t level_idx;
	int rc;

	if (roc_model_is_cn9k())
		return NIX_ERR_HW_NOTSUP;

	level_idx = roc_nix_bpf_level_to_idx(lvl_flag);
	if (level_idx == ROC_NIX_BPF_LEVEL_IDX_INVALID)
		return NIX_ERR_PARAM;

	aq = mbox_alloc_msg_nix_cn10k_aq_enq(mbox);
	if (aq == NULL)
		return -ENOSPC;
	aq->qidx = (sw_to_hw_lvl_map[level_idx] << 14 | id);
	aq->ctype = NIX_AQ_CTYPE_BAND_PROF;
	aq->op = NIX_AQ_INSTOP_READ;
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (!rc) {
		plt_dump("============= band prof id =%d ===============", id);
		nix_lf_bpf_dump(&rsp->prof);
	}

	return rc;
}

int
roc_nix_bpf_pre_color_tbl_setup(struct roc_nix *roc_nix, uint16_t id,
				enum roc_nix_bpf_level_flag lvl_flag,
				struct roc_nix_bpf_precolor *tbl)
{
	struct mbox *mbox = get_mbox(roc_nix);
	struct nix_cn10k_aq_enq_req *aq;
	uint8_t pc_mode, tn_ena;
	uint8_t level_idx;
	int rc;

	if (!tbl || !tbl->count)
		return NIX_ERR_PARAM;

	if (roc_model_is_cn9k())
		return NIX_ERR_HW_NOTSUP;

	level_idx = roc_nix_bpf_level_to_idx(lvl_flag);
	if (level_idx == ROC_NIX_BPF_LEVEL_IDX_INVALID)
		return NIX_ERR_PARAM;

	switch (tbl->mode) {
	case ROC_NIX_BPF_PC_MODE_VLAN_INNER:
	case ROC_NIX_BPF_PC_MODE_VLAN_OUTER:
		if (tbl->count != NIX_BPF_PRECOLOR_VLAN_TABLE_SIZE) {
			plt_err("Table size must be %d",
				NIX_BPF_PRECOLOR_VLAN_TABLE_SIZE);
			rc = NIX_ERR_PARAM;
			goto exit;
		}
		tn_ena = nix_precolor_vlan_table_update(roc_nix, tbl);
		pc_mode = NIX_RX_BAND_PROF_PC_MODE_VLAN;
		break;
	case ROC_NIX_BPF_PC_MODE_DSCP_INNER:
		if (tbl->count != NIX_BPF_PRECOLOR_DSCP_TABLE_SIZE) {
			plt_err("Table size must be %d",
				NIX_BPF_PRECOLOR_DSCP_TABLE_SIZE);
			rc = NIX_ERR_PARAM;
			goto exit;
		}
		tn_ena = nix_precolor_inner_dscp_table_update(roc_nix, tbl);
		pc_mode = NIX_RX_BAND_PROF_PC_MODE_DSCP;
		break;
	case ROC_NIX_BPF_PC_MODE_DSCP_OUTER:
		if (tbl->count != NIX_BPF_PRECOLOR_DSCP_TABLE_SIZE) {
			plt_err("Table size must be %d",
				NIX_BPF_PRECOLOR_DSCP_TABLE_SIZE);
			rc = NIX_ERR_PARAM;
			goto exit;
		}
		tn_ena = nix_precolor_outer_dscp_table_update(roc_nix, tbl);
		pc_mode = NIX_RX_BAND_PROF_PC_MODE_DSCP;
		break;
	case ROC_NIX_BPF_PC_MODE_GEN_INNER:
	case ROC_NIX_BPF_PC_MODE_GEN_OUTER:
		if (tbl->count != NIX_BPF_PRECOLOR_GEN_TABLE_SIZE) {
			plt_err("Table size must be %d",
				NIX_BPF_PRECOLOR_GEN_TABLE_SIZE);
			rc = NIX_ERR_PARAM;
			goto exit;
		}

		tn_ena = nix_precolor_gen_table_update(roc_nix, tbl);
		pc_mode = NIX_RX_BAND_PROF_PC_MODE_GEN;
		break;
	default:
		rc = NIX_ERR_PARAM;
		goto exit;
	}

	/* Update corresponding bandwidth profile too */
	aq = mbox_alloc_msg_nix_cn10k_aq_enq(mbox);
	if (aq == NULL)
		return -ENOSPC;
	aq->qidx = (sw_to_hw_lvl_map[level_idx] << 14) | id;
	aq->ctype = NIX_AQ_CTYPE_BAND_PROF;
	aq->op = NIX_AQ_INSTOP_WRITE;
	aq->prof.pc_mode = pc_mode;
	aq->prof.tnl_ena = tn_ena;
	aq->prof_mask.pc_mode = ~(aq->prof_mask.pc_mode);
	aq->prof_mask.tnl_ena = ~(aq->prof_mask.tnl_ena);

	return mbox_process(mbox);

exit:
	return rc;
}

int
roc_nix_bpf_connect(struct roc_nix *roc_nix,
		    enum roc_nix_bpf_level_flag lvl_flag, uint16_t src_id,
		    uint16_t dst_id)
{
	struct mbox *mbox = get_mbox(roc_nix);
	struct nix_cn10k_aq_enq_req *aq;
	uint8_t level_idx;

	if (roc_model_is_cn9k())
		return NIX_ERR_HW_NOTSUP;

	level_idx = roc_nix_bpf_level_to_idx(lvl_flag);
	if (level_idx == ROC_NIX_BPF_LEVEL_IDX_INVALID)
		return NIX_ERR_PARAM;

	aq = mbox_alloc_msg_nix_cn10k_aq_enq(mbox);
	if (aq == NULL)
		return -ENOSPC;
	aq->qidx = (sw_to_hw_lvl_map[level_idx] << 14) | src_id;
	aq->ctype = NIX_AQ_CTYPE_BAND_PROF;
	aq->op = NIX_AQ_INSTOP_WRITE;

	if (dst_id == ROC_NIX_BPF_ID_INVALID) {
		aq->prof.hl_en = false;
		aq->prof_mask.hl_en = ~(aq->prof_mask.hl_en);
	} else {
		aq->prof.hl_en = true;
		aq->prof.band_prof_id = dst_id;
		aq->prof_mask.hl_en = ~(aq->prof_mask.hl_en);
		aq->prof_mask.band_prof_id = ~(aq->prof_mask.band_prof_id);
	}

	return mbox_process(mbox);
}

int
roc_nix_bpf_stats_read(struct roc_nix *roc_nix, uint16_t id, uint64_t mask,
		       enum roc_nix_bpf_level_flag lvl_flag,
		       uint64_t stats[ROC_NIX_BPF_STATS_MAX])
{
	uint8_t yellow_pkt_pass, yellow_octs_pass, yellow_pkt_drop;
	uint8_t green_octs_drop, yellow_octs_drop, red_octs_drop;
	uint8_t green_pkt_pass, green_octs_pass, green_pkt_drop;
	uint8_t red_pkt_pass, red_octs_pass, red_pkt_drop;
	struct mbox *mbox = get_mbox(roc_nix);
	struct nix_cn10k_aq_enq_rsp *rsp;
	struct nix_cn10k_aq_enq_req *aq;
	uint8_t level_idx;
	int rc;

	if (roc_model_is_cn9k())
		return NIX_ERR_HW_NOTSUP;

	level_idx = roc_nix_bpf_level_to_idx(lvl_flag);
	if (level_idx == ROC_NIX_BPF_LEVEL_IDX_INVALID)
		return NIX_ERR_PARAM;

	aq = mbox_alloc_msg_nix_cn10k_aq_enq(mbox);
	if (aq == NULL)
		return -ENOSPC;
	aq->qidx = (sw_to_hw_lvl_map[level_idx] << 14 | id);
	aq->ctype = NIX_AQ_CTYPE_BAND_PROF;
	aq->op = NIX_AQ_INSTOP_READ;
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	green_pkt_pass =
		roc_nix_bpf_stats_to_idx(mask & ROC_NIX_BPF_GREEN_PKT_F_PASS);
	green_octs_pass =
		roc_nix_bpf_stats_to_idx(mask & ROC_NIX_BPF_GREEN_OCTS_F_PASS);
	green_pkt_drop =
		roc_nix_bpf_stats_to_idx(mask & ROC_NIX_BPF_GREEN_PKT_F_DROP);
	green_octs_drop =
		roc_nix_bpf_stats_to_idx(mask & ROC_NIX_BPF_GREEN_OCTS_F_DROP);
	yellow_pkt_pass =
		roc_nix_bpf_stats_to_idx(mask & ROC_NIX_BPF_YELLOW_PKT_F_PASS);
	yellow_octs_pass =
		roc_nix_bpf_stats_to_idx(mask & ROC_NIX_BPF_YELLOW_OCTS_F_PASS);
	yellow_pkt_drop =
		roc_nix_bpf_stats_to_idx(mask & ROC_NIX_BPF_YELLOW_PKT_F_DROP);
	yellow_octs_drop =
		roc_nix_bpf_stats_to_idx(mask & ROC_NIX_BPF_YELLOW_OCTS_F_DROP);
	red_pkt_pass =
		roc_nix_bpf_stats_to_idx(mask & ROC_NIX_BPF_RED_PKT_F_PASS);
	red_octs_pass =
		roc_nix_bpf_stats_to_idx(mask & ROC_NIX_BPF_RED_OCTS_F_PASS);
	red_pkt_drop =
		roc_nix_bpf_stats_to_idx(mask & ROC_NIX_BPF_RED_PKT_F_DROP);
	red_octs_drop =
		roc_nix_bpf_stats_to_idx(mask & ROC_NIX_BPF_RED_OCTS_F_DROP);

	if (green_pkt_pass != ROC_NIX_BPF_STATS_MAX)
		stats[green_pkt_pass] = rsp->prof.green_pkt_pass;

	if (green_octs_pass != ROC_NIX_BPF_STATS_MAX)
		stats[green_octs_pass] = rsp->prof.green_octs_pass;

	if (green_pkt_drop != ROC_NIX_BPF_STATS_MAX)
		stats[green_pkt_drop] = rsp->prof.green_pkt_drop;

	if (green_octs_drop != ROC_NIX_BPF_STATS_MAX)
		stats[green_octs_drop] = rsp->prof.green_octs_pass;

	if (yellow_pkt_pass != ROC_NIX_BPF_STATS_MAX)
		stats[yellow_pkt_pass] = rsp->prof.yellow_pkt_pass;

	if (yellow_octs_pass != ROC_NIX_BPF_STATS_MAX)
		stats[yellow_octs_pass] = rsp->prof.yellow_octs_pass;

	if (yellow_pkt_drop != ROC_NIX_BPF_STATS_MAX)
		stats[yellow_pkt_drop] = rsp->prof.yellow_pkt_drop;

	if (yellow_octs_drop != ROC_NIX_BPF_STATS_MAX)
		stats[yellow_octs_drop] = rsp->prof.yellow_octs_drop;

	if (red_pkt_pass != ROC_NIX_BPF_STATS_MAX)
		stats[red_pkt_pass] = rsp->prof.red_pkt_pass;

	if (red_octs_pass != ROC_NIX_BPF_STATS_MAX)
		stats[red_octs_pass] = rsp->prof.red_octs_pass;

	if (red_pkt_drop != ROC_NIX_BPF_STATS_MAX)
		stats[red_pkt_drop] = rsp->prof.red_pkt_drop;

	if (red_octs_drop != ROC_NIX_BPF_STATS_MAX)
		stats[red_octs_drop] = rsp->prof.red_octs_drop;

	return 0;
}

int
roc_nix_bpf_stats_reset(struct roc_nix *roc_nix, uint16_t id, uint64_t mask,
			enum roc_nix_bpf_level_flag lvl_flag)
{
	struct mbox *mbox = get_mbox(roc_nix);
	struct nix_cn10k_aq_enq_req *aq;
	uint8_t level_idx;

	if (roc_model_is_cn9k())
		return NIX_ERR_HW_NOTSUP;

	level_idx = roc_nix_bpf_level_to_idx(lvl_flag);
	if (level_idx == ROC_NIX_BPF_LEVEL_IDX_INVALID)
		return NIX_ERR_PARAM;

	aq = mbox_alloc_msg_nix_cn10k_aq_enq(mbox);
	if (aq == NULL)
		return -ENOSPC;
	aq->qidx = (sw_to_hw_lvl_map[level_idx] << 14 | id);
	aq->ctype = NIX_AQ_CTYPE_BAND_PROF;
	aq->op = NIX_AQ_INSTOP_WRITE;

	if (mask & ROC_NIX_BPF_GREEN_PKT_F_PASS) {
		aq->prof.green_pkt_pass = 0;
		aq->prof_mask.green_pkt_pass = ~(aq->prof_mask.green_pkt_pass);
	}
	if (mask & ROC_NIX_BPF_GREEN_OCTS_F_PASS) {
		aq->prof.green_octs_pass = 0;
		aq->prof_mask.green_octs_pass =
			~(aq->prof_mask.green_octs_pass);
	}
	if (mask & ROC_NIX_BPF_GREEN_PKT_F_DROP) {
		aq->prof.green_pkt_drop = 0;
		aq->prof_mask.green_pkt_drop = ~(aq->prof_mask.green_pkt_drop);
	}
	if (mask & ROC_NIX_BPF_GREEN_OCTS_F_DROP) {
		aq->prof.green_octs_drop = 0;
		aq->prof_mask.green_octs_drop =
			~(aq->prof_mask.green_octs_drop);
	}
	if (mask & ROC_NIX_BPF_YELLOW_PKT_F_PASS) {
		aq->prof.yellow_pkt_pass = 0;
		aq->prof_mask.yellow_pkt_pass =
			~(aq->prof_mask.yellow_pkt_pass);
	}
	if (mask & ROC_NIX_BPF_YELLOW_OCTS_F_PASS) {
		aq->prof.yellow_octs_pass = 0;
		aq->prof_mask.yellow_octs_pass =
			~(aq->prof_mask.yellow_octs_pass);
	}
	if (mask & ROC_NIX_BPF_YELLOW_PKT_F_DROP) {
		aq->prof.yellow_pkt_drop = 0;
		aq->prof_mask.yellow_pkt_drop =
			~(aq->prof_mask.yellow_pkt_drop);
	}
	if (mask & ROC_NIX_BPF_YELLOW_OCTS_F_DROP) {
		aq->prof.yellow_octs_drop = 0;
		aq->prof_mask.yellow_octs_drop =
			~(aq->prof_mask.yellow_octs_drop);
	}
	if (mask & ROC_NIX_BPF_RED_PKT_F_PASS) {
		aq->prof.red_pkt_pass = 0;
		aq->prof_mask.red_pkt_pass = ~(aq->prof_mask.red_pkt_pass);
	}
	if (mask & ROC_NIX_BPF_RED_OCTS_F_PASS) {
		aq->prof.red_octs_pass = 0;
		aq->prof_mask.red_octs_pass = ~(aq->prof_mask.red_octs_pass);
	}
	if (mask & ROC_NIX_BPF_RED_PKT_F_DROP) {
		aq->prof.red_pkt_drop = 0;
		aq->prof_mask.red_pkt_drop = ~(aq->prof_mask.red_pkt_drop);
	}
	if (mask & ROC_NIX_BPF_RED_OCTS_F_DROP) {
		aq->prof.red_octs_drop = 0;
		aq->prof_mask.red_octs_drop = ~(aq->prof_mask.red_octs_drop);
	}

	return mbox_process(mbox);
}

int
roc_nix_bpf_lf_stats_read(struct roc_nix *roc_nix, uint64_t mask,
			  uint64_t stats[ROC_NIX_BPF_STATS_MAX])
{
	uint8_t yellow_pkt_pass, yellow_octs_pass, yellow_pkt_drop;
	uint8_t green_octs_drop, yellow_octs_drop, red_octs_drop;
	uint8_t green_pkt_pass, green_octs_pass, green_pkt_drop;
	uint8_t red_pkt_pass, red_octs_pass, red_pkt_drop;
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);

	green_pkt_pass =
		roc_nix_bpf_stats_to_idx(mask & ROC_NIX_BPF_GREEN_PKT_F_PASS);
	green_octs_pass =
		roc_nix_bpf_stats_to_idx(mask & ROC_NIX_BPF_GREEN_OCTS_F_PASS);
	green_pkt_drop =
		roc_nix_bpf_stats_to_idx(mask & ROC_NIX_BPF_GREEN_PKT_F_DROP);
	green_octs_drop =
		roc_nix_bpf_stats_to_idx(mask & ROC_NIX_BPF_GREEN_OCTS_F_DROP);
	yellow_pkt_pass =
		roc_nix_bpf_stats_to_idx(mask & ROC_NIX_BPF_YELLOW_PKT_F_PASS);
	yellow_octs_pass =
		roc_nix_bpf_stats_to_idx(mask & ROC_NIX_BPF_YELLOW_OCTS_F_PASS);
	yellow_pkt_drop =
		roc_nix_bpf_stats_to_idx(mask & ROC_NIX_BPF_YELLOW_PKT_F_DROP);
	yellow_octs_drop =
		roc_nix_bpf_stats_to_idx(mask & ROC_NIX_BPF_YELLOW_OCTS_F_DROP);
	red_pkt_pass =
		roc_nix_bpf_stats_to_idx(mask & ROC_NIX_BPF_RED_PKT_F_PASS);
	red_octs_pass =
		roc_nix_bpf_stats_to_idx(mask & ROC_NIX_BPF_RED_OCTS_F_PASS);
	red_pkt_drop =
		roc_nix_bpf_stats_to_idx(mask & ROC_NIX_BPF_RED_PKT_F_DROP);
	red_octs_drop =
		roc_nix_bpf_stats_to_idx(mask & ROC_NIX_BPF_RED_OCTS_F_DROP);

	if (green_pkt_pass != ROC_NIX_BPF_STATS_MAX) {
		stats[green_pkt_pass] =
			NIX_RD_STATS(NIX_STAT_LF_RX_RX_GC_OCTS_PASSED);
	}

	if (green_octs_pass != ROC_NIX_BPF_STATS_MAX) {
		stats[green_octs_pass] =
			NIX_RD_STATS(NIX_STAT_LF_RX_RX_YC_PKTS_PASSED);
	}

	if (green_pkt_drop != ROC_NIX_BPF_STATS_MAX) {
		stats[green_pkt_drop] =
			NIX_RD_STATS(NIX_STAT_LF_RX_RX_GC_OCTS_DROP);
	}

	if (green_octs_drop != ROC_NIX_BPF_STATS_MAX) {
		stats[green_octs_drop] =
			NIX_RD_STATS(NIX_STAT_LF_RX_RX_YC_PKTS_DROP);
	}

	if (yellow_pkt_pass != ROC_NIX_BPF_STATS_MAX) {
		stats[yellow_pkt_pass] =
			NIX_RD_STATS(NIX_STAT_LF_RX_RX_GC_PKTS_PASSED);
	}

	if (yellow_octs_pass != ROC_NIX_BPF_STATS_MAX) {
		stats[yellow_octs_pass] =
			NIX_RD_STATS(NIX_STAT_LF_RX_RX_RC_OCTS_PASSED);
	}

	if (yellow_pkt_drop != ROC_NIX_BPF_STATS_MAX) {
		stats[yellow_pkt_drop] =
			NIX_RD_STATS(NIX_STAT_LF_RX_RX_GC_PKTS_DROP);
	}

	if (yellow_octs_drop != ROC_NIX_BPF_STATS_MAX) {
		stats[yellow_octs_drop] =
			NIX_RD_STATS(NIX_STAT_LF_RX_RX_RC_OCTS_DROP);
	}

	if (red_pkt_pass != ROC_NIX_BPF_STATS_MAX) {
		stats[red_pkt_pass] =
			NIX_RD_STATS(NIX_STAT_LF_RX_RX_YC_OCTS_PASSED);
	}

	if (red_octs_pass != ROC_NIX_BPF_STATS_MAX) {
		stats[red_octs_pass] =
			NIX_RD_STATS(NIX_STAT_LF_RX_RX_RC_PKTS_PASSED);
	}

	if (red_pkt_drop != ROC_NIX_BPF_STATS_MAX) {
		stats[red_pkt_drop] =
			NIX_RD_STATS(NIX_STAT_LF_RX_RX_YC_OCTS_DROP);
	}

	if (red_octs_drop != ROC_NIX_BPF_STATS_MAX) {
		stats[red_octs_drop] =
			NIX_RD_STATS(NIX_STAT_LF_RX_RX_RC_PKTS_DROP);
	}

	return 0;
}

int
roc_nix_bpf_lf_stats_reset(struct roc_nix *roc_nix, uint64_t mask)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);

	if (mask & ROC_NIX_BPF_GREEN_PKT_F_PASS)
		NIX_RST_STATS(ROC_NIX_BPF_GREEN_PKT_F_PASS);
	if (mask & ROC_NIX_BPF_GREEN_OCTS_F_PASS)
		NIX_RST_STATS(ROC_NIX_BPF_GREEN_OCTS_F_PASS);
	if (mask & ROC_NIX_BPF_GREEN_PKT_F_DROP)
		NIX_RST_STATS(ROC_NIX_BPF_GREEN_PKT_F_DROP);
	if (mask & ROC_NIX_BPF_GREEN_OCTS_F_DROP)
		NIX_RST_STATS(ROC_NIX_BPF_GREEN_OCTS_F_DROP);
	if (mask & ROC_NIX_BPF_YELLOW_PKT_F_PASS)
		NIX_RST_STATS(ROC_NIX_BPF_YELLOW_PKT_F_PASS);
	if (mask & ROC_NIX_BPF_YELLOW_OCTS_F_PASS)
		NIX_RST_STATS(ROC_NIX_BPF_YELLOW_OCTS_F_PASS);
	if (mask & ROC_NIX_BPF_YELLOW_PKT_F_DROP)
		NIX_RST_STATS(ROC_NIX_BPF_YELLOW_PKT_F_DROP);
	if (mask & ROC_NIX_BPF_YELLOW_OCTS_F_DROP)
		NIX_RST_STATS(ROC_NIX_BPF_YELLOW_OCTS_F_DROP);
	if (mask & ROC_NIX_BPF_RED_PKT_F_PASS)
		NIX_RST_STATS(ROC_NIX_BPF_RED_PKT_F_PASS);
	if (mask & ROC_NIX_BPF_RED_OCTS_F_PASS)
		NIX_RST_STATS(ROC_NIX_BPF_RED_OCTS_F_PASS);
	if (mask & ROC_NIX_BPF_RED_PKT_F_DROP)
		NIX_RST_STATS(ROC_NIX_BPF_RED_PKT_F_DROP);
	if (mask & ROC_NIX_BPF_RED_OCTS_F_DROP)
		NIX_RST_STATS(ROC_NIX_BPF_RED_OCTS_F_DROP);

	return 0;
}
