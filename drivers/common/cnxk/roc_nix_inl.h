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

static inline struct roc_onf_ipsec_inb_sa *
roc_nix_inl_onf_ipsec_inb_sa(uintptr_t base, uint64_t idx)
{
	uint64_t off = idx << ROC_NIX_INL_ONF_IPSEC_INB_SA_SZ_LOG2;

	return PLT_PTR_ADD(base, off);
}

static inline struct roc_onf_ipsec_outb_sa *
roc_nix_inl_onf_ipsec_outb_sa(uintptr_t base, uint64_t idx)
{
	uint64_t off = idx << ROC_NIX_INL_ONF_IPSEC_OUTB_SA_SZ_LOG2;

	return PLT_PTR_ADD(base, off);
}

static inline void *
roc_nix_inl_onf_ipsec_inb_sa_sw_rsvd(void *sa)
{
	return PLT_PTR_ADD(sa, ROC_NIX_INL_ONF_IPSEC_INB_HW_SZ);
}

static inline void *
roc_nix_inl_onf_ipsec_outb_sa_sw_rsvd(void *sa)
{
	return PLT_PTR_ADD(sa, ROC_NIX_INL_ONF_IPSEC_OUTB_HW_SZ);
}

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

/* Inline device SSO Work callback */
typedef void (*roc_nix_inl_sso_work_cb_t)(uint64_t *gw, void *args);

struct roc_nix_inl_dev {
	/* Input parameters */
	struct plt_pci_device *pci_dev;
	uint16_t ipsec_in_max_spi;
	bool selftest;
	bool is_multi_channel;
	uint16_t channel;
	uint16_t chan_mask;
	bool attach_cptlf;
	/* End of input parameters */

#define ROC_NIX_INL_MEM_SZ (1280)
	uint8_t reserved[ROC_NIX_INL_MEM_SZ] __plt_cache_aligned;
} __plt_cache_aligned;

/* NIX Inline Device API */
int __roc_api roc_nix_inl_dev_init(struct roc_nix_inl_dev *roc_inl_dev);
int __roc_api roc_nix_inl_dev_fini(struct roc_nix_inl_dev *roc_inl_dev);
void __roc_api roc_nix_inl_dev_dump(struct roc_nix_inl_dev *roc_inl_dev);
bool __roc_api roc_nix_inl_dev_is_probed(void);
void __roc_api roc_nix_inl_dev_lock(void);
void __roc_api roc_nix_inl_dev_unlock(void);

/* NIX Inline Inbound API */
int __roc_api roc_nix_inl_inb_init(struct roc_nix *roc_nix);
int __roc_api roc_nix_inl_inb_fini(struct roc_nix *roc_nix);
bool __roc_api roc_nix_inl_inb_is_enabled(struct roc_nix *roc_nix);
uintptr_t __roc_api roc_nix_inl_inb_sa_base_get(struct roc_nix *roc_nix,
						bool inl_dev_sa);
uint32_t __roc_api roc_nix_inl_inb_sa_max_spi(struct roc_nix *roc_nix,
					      bool inl_dev_sa);
uint32_t __roc_api roc_nix_inl_inb_sa_sz(struct roc_nix *roc_nix,
					 bool inl_dev_sa);
uintptr_t __roc_api roc_nix_inl_inb_sa_get(struct roc_nix *roc_nix,
					   bool inl_dev_sa, uint32_t spi);
void __roc_api roc_nix_inb_mode_set(struct roc_nix *roc_nix, bool use_inl_dev);
int __roc_api roc_nix_inl_dev_rq_get(struct roc_nix_rq *rq);
int __roc_api roc_nix_inl_dev_rq_put(struct roc_nix_rq *rq);
bool __roc_api roc_nix_inb_is_with_inl_dev(struct roc_nix *roc_nix);
struct roc_nix_rq *__roc_api roc_nix_inl_dev_rq(void);
int __roc_api roc_nix_inl_inb_tag_update(struct roc_nix *roc_nix,
					 uint32_t tag_const, uint8_t tt);
uint64_t __roc_api roc_nix_inl_dev_rq_limit_get(void);

/* NIX Inline Outbound API */
int __roc_api roc_nix_inl_outb_init(struct roc_nix *roc_nix);
int __roc_api roc_nix_inl_outb_fini(struct roc_nix *roc_nix);
bool __roc_api roc_nix_inl_outb_is_enabled(struct roc_nix *roc_nix);
uintptr_t __roc_api roc_nix_inl_outb_sa_base_get(struct roc_nix *roc_nix);
struct roc_cpt_lf *__roc_api
roc_nix_inl_outb_lf_base_get(struct roc_nix *roc_nix);
uint16_t __roc_api roc_nix_inl_outb_sso_pffunc_get(struct roc_nix *roc_nix);
int __roc_api roc_nix_inl_cb_register(roc_nix_inl_sso_work_cb_t cb, void *args);
int __roc_api roc_nix_inl_cb_unregister(roc_nix_inl_sso_work_cb_t cb,
					void *args);
/* NIX Inline/Outbound API */
enum roc_nix_inl_sa_sync_op {
	ROC_NIX_INL_SA_OP_FLUSH,
	ROC_NIX_INL_SA_OP_FLUSH_INVAL,
	ROC_NIX_INL_SA_OP_RELOAD,
};

int __roc_api roc_nix_inl_sa_sync(struct roc_nix *roc_nix, void *sa, bool inb,
				  enum roc_nix_inl_sa_sync_op op);
int __roc_api roc_nix_inl_ctx_write(struct roc_nix *roc_nix, void *sa_dptr,
				    void *sa_cptr, bool inb, uint16_t sa_len);
void __roc_api roc_nix_inl_inb_sa_init(struct roc_ot_ipsec_inb_sa *sa);
void __roc_api roc_nix_inl_outb_sa_init(struct roc_ot_ipsec_outb_sa *sa);

#endif /* _ROC_NIX_INL_H_ */
