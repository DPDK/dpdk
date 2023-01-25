/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Marvell.
 */

#ifndef _ROC_NPA_DP_H_
#define _ROC_NPA_DP_H_

#define ROC_AURA_ID_MASK       (BIT_ULL(16) - 1)

static inline uint64_t
roc_npa_aura_handle_to_aura(uint64_t aura_handle)
{
	return aura_handle & ROC_AURA_ID_MASK;
}

static inline uintptr_t
roc_npa_aura_handle_to_base(uint64_t aura_handle)
{
	return (uintptr_t)(aura_handle & ~ROC_AURA_ID_MASK);
}

static inline void
roc_npa_aura_op_free(uint64_t aura_handle, const int fabs, uint64_t iova)
{
	uint64_t reg = roc_npa_aura_handle_to_aura(aura_handle);
	const uint64_t addr =
		roc_npa_aura_handle_to_base(aura_handle) + NPA_LF_AURA_OP_FREE0;
	if (fabs)
		reg |= BIT_ULL(63); /* FABS */

	roc_store_pair(iova, reg, addr);
}

#endif /* _ROC_NPA_DP_H_ */
