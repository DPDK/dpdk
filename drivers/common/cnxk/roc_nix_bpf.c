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
