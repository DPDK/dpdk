/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_NPA_PRIV_H_
#define _ROC_NPA_PRIV_H_

enum npa_error_status {
	NPA_ERR_PARAM = -512,
	NPA_ERR_ALLOC = -513,
	NPA_ERR_INVALID_BLOCK_SZ = -514,
	NPA_ERR_AURA_ID_ALLOC = -515,
	NPA_ERR_AURA_POOL_INIT = -516,
	NPA_ERR_AURA_POOL_FINI = -517,
	NPA_ERR_BASE_INVALID = -518,
	NPA_ERR_DEVICE_NOT_BOUNDED = -519,
};

struct npa_lf {
	struct plt_intr_handle *intr_handle;
	struct npa_aura_attr *aura_attr;
	struct npa_aura_lim *aura_lim;
	struct plt_pci_device *pci_dev;
	struct plt_bitmap *npa_bmp;
	struct mbox *mbox;
	uint32_t stack_pg_ptrs;
	uint32_t stack_pg_bytes;
	uint16_t npa_msixoff;
	bool zero_aura_rsvd;
	void *npa_qint_mem;
	void *npa_bmp_mem;
	uint32_t nr_pools;
	uint16_t pf_func;
	uint8_t aura_sz;
	uint32_t qints;
	uintptr_t base;

};

struct npa_qint {
	struct npa_lf *lf;
	uint8_t qintx;
};

struct npa_aura_lim {
	uint64_t ptr_start;
	uint64_t ptr_end;
};

struct npa_aura_attr {
	int buf_type[ROC_NPA_BUF_TYPE_END];
	uint16_t ref_count;
	uint64_t nix0_bpid;
	uint64_t nix1_bpid;
	uint64_t shift;
	uint64_t limit;
	uint8_t bp_ena;
	uint8_t bp;
};

struct dev;

static inline struct npa *
roc_npa_to_npa_priv(struct roc_npa *roc_npa)
{
	return (struct npa *)&roc_npa->reserved[0];
}

/* NPA lf */
int npa_lf_init(struct dev *dev, struct plt_pci_device *pci_dev);
int npa_lf_fini(void);

/* IRQ */
int npa_register_irqs(struct npa_lf *lf);
void npa_unregister_irqs(struct npa_lf *lf);

#endif /* _ROC_NPA_PRIV_H_ */
