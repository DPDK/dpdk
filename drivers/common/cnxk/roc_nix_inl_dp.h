/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Marvell.
 */
#ifndef _ROC_NIX_INL_DP_H_
#define _ROC_NIX_INL_DP_H_

/* OT INB HW area */
#define ROC_NIX_INL_OT_IPSEC_INB_HW_SZ                                         \
	PLT_ALIGN(sizeof(struct roc_ot_ipsec_inb_sa), ROC_ALIGN)
/* OT INB SW reserved area */
#define ROC_NIX_INL_OT_IPSEC_INB_SW_RSVD 128
#define ROC_NIX_INL_OT_IPSEC_INB_SA_SZ                                         \
	(ROC_NIX_INL_OT_IPSEC_INB_HW_SZ + ROC_NIX_INL_OT_IPSEC_INB_SW_RSVD)
#define ROC_NIX_INL_OT_IPSEC_INB_SA_SZ_LOG2 10

/* OT OUTB HW area */
#define ROC_NIX_INL_OT_IPSEC_OUTB_HW_SZ                                        \
	PLT_ALIGN(sizeof(struct roc_ot_ipsec_outb_sa), ROC_ALIGN)

/* OT OUTB SW reserved area */
#define ROC_NIX_INL_OT_IPSEC_OUTB_SW_RSVD 128
#define ROC_NIX_INL_OT_IPSEC_OUTB_SA_SZ                                        \
	(ROC_NIX_INL_OT_IPSEC_OUTB_HW_SZ + ROC_NIX_INL_OT_IPSEC_OUTB_SW_RSVD)
#define ROC_NIX_INL_OT_IPSEC_OUTB_SA_SZ_LOG2 9

/* Alignment of SA Base */
#define ROC_NIX_INL_SA_BASE_ALIGN BIT_ULL(16)

static inline struct roc_ot_ipsec_inb_sa *
roc_nix_inl_ot_ipsec_inb_sa(uintptr_t base, uint64_t idx)
{
	uint64_t off = idx << ROC_NIX_INL_OT_IPSEC_INB_SA_SZ_LOG2;

	return PLT_PTR_ADD(base, off);
}

static inline struct roc_ot_ipsec_outb_sa *
roc_nix_inl_ot_ipsec_outb_sa(uintptr_t base, uint64_t idx)
{
	uint64_t off = idx << ROC_NIX_INL_OT_IPSEC_OUTB_SA_SZ_LOG2;

	return PLT_PTR_ADD(base, off);
}

static inline void *
roc_nix_inl_ot_ipsec_inb_sa_sw_rsvd(void *sa)
{
	return PLT_PTR_ADD(sa, ROC_NIX_INL_OT_IPSEC_INB_HW_SZ);
}

static inline void *
roc_nix_inl_ot_ipsec_outb_sa_sw_rsvd(void *sa)
{
	return PLT_PTR_ADD(sa, ROC_NIX_INL_OT_IPSEC_OUTB_HW_SZ);
}

#endif /* _ROC_NIX_INL_DP_H_ */
