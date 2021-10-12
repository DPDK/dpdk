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
