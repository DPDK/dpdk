/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#ifndef _ROC_NIX_INL_H_
#define _ROC_NIX_INL_H_

/* ONF INB HW area */
#define ROC_NIX_INL_ONF_IPSEC_INB_HW_SZ                                        \
	PLT_ALIGN(sizeof(struct roc_onf_ipsec_inb_sa), ROC_ALIGN)
/* ONF INB SW reserved area */
#define ROC_NIX_INL_ONF_IPSEC_INB_SW_RSVD 384
#define ROC_NIX_INL_ONF_IPSEC_INB_SA_SZ                                        \
	(ROC_NIX_INL_ONF_IPSEC_INB_HW_SZ + ROC_NIX_INL_ONF_IPSEC_INB_SW_RSVD)
#define ROC_NIX_INL_ONF_IPSEC_INB_SA_SZ_LOG2 9

/* ONF OUTB HW area */
#define ROC_NIX_INL_ONF_IPSEC_OUTB_HW_SZ                                       \
	PLT_ALIGN(sizeof(struct roc_onf_ipsec_outb_sa), ROC_ALIGN)
/* ONF OUTB SW reserved area */
#define ROC_NIX_INL_ONF_IPSEC_OUTB_SW_RSVD 128
#define ROC_NIX_INL_ONF_IPSEC_OUTB_SA_SZ                                       \
	(ROC_NIX_INL_ONF_IPSEC_OUTB_HW_SZ + ROC_NIX_INL_ONF_IPSEC_OUTB_SW_RSVD)
#define ROC_NIX_INL_ONF_IPSEC_OUTB_SA_SZ_LOG2 8

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

/* Inline device SSO Work callback */
typedef void (*roc_nix_inl_sso_work_cb_t)(uint64_t *gw, void *args);

struct roc_nix_inl_dev {
	/* Input parameters */
	struct plt_pci_device *pci_dev;
	uint16_t ipsec_in_max_spi;
	bool selftest;
	bool attach_cptlf;
	/* End of input parameters */

#define ROC_NIX_INL_MEM_SZ (1280)
	uint8_t reserved[ROC_NIX_INL_MEM_SZ] __plt_cache_aligned;
} __plt_cache_aligned;

/* NIX Inline Device API */
int __roc_api roc_nix_inl_dev_init(struct roc_nix_inl_dev *roc_inl_dev);
int __roc_api roc_nix_inl_dev_fini(struct roc_nix_inl_dev *roc_inl_dev);
void __roc_api roc_nix_inl_dev_dump(struct roc_nix_inl_dev *roc_inl_dev);

#endif /* _ROC_NIX_INL_H_ */
