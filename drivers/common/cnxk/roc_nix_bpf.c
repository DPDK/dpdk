/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

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
