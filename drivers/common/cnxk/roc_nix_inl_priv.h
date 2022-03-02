/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#ifndef _ROC_NIX_INL_PRIV_H_
#define _ROC_NIX_INL_PRIV_H_
#include <pthread.h>
#include <sys/types.h>

struct nix_inl_dev {
	/* Base device object */
	struct dev dev;

	/* PCI device */
	struct plt_pci_device *pci_dev;

	/* LF specific BAR2 regions */
	uintptr_t nix_base;
	uintptr_t ssow_base;
	uintptr_t sso_base;
	uintptr_t cpt_base;

	/* MSIX vector offsets */
	uint16_t nix_msixoff;
	uint16_t ssow_msixoff;
	uint16_t sso_msixoff;
	uint16_t cpt_msixoff;

	/* SSO data */
	uint32_t xaq_buf_size;
	uint32_t xae_waes;
	uint32_t iue;
	uint32_t nb_xae;
	struct roc_sso_xaq_data xaq;
	roc_nix_inl_sso_work_cb_t work_cb;
	void *cb_args;
	uint64_t *pkt_pools;
	uint16_t pkt_pools_cnt;

	/* NIX data */
	uint8_t lf_tx_stats;
	uint8_t lf_rx_stats;
	uint16_t vwqe_interval;
	uint16_t cints;
	uint16_t qints;
	struct roc_nix_rq rq;
	uint16_t rq_refs;
	bool is_nix1;
	uint8_t spb_drop_pc;
	uint8_t lpb_drop_pc;

	/* NIX/CPT data */
	void *inb_sa_base;
	uint16_t inb_sa_sz;

	/* CPT data */
	struct roc_cpt_lf cpt_lf;

	/* OUTB soft expiry poll thread */
	pthread_t soft_exp_poll_thread;
	uint32_t soft_exp_poll_freq;
	uint64_t *sa_soft_exp_ring;

	/* Soft expiry ring bitmap */
	struct plt_bitmap *soft_exp_ring_bmap;

	/* bitmap memory */
	void *soft_exp_ring_bmap_mem;

	/* Device arguments */
	uint8_t selftest;
	uint16_t channel;
	uint16_t chan_mask;
	bool is_multi_channel;
	uint32_t ipsec_in_min_spi;
	uint32_t ipsec_in_max_spi;
	uint32_t inb_spi_mask;
	bool attach_cptlf;
	bool wqe_skip;
};

int nix_inl_sso_register_irqs(struct nix_inl_dev *inl_dev);
void nix_inl_sso_unregister_irqs(struct nix_inl_dev *inl_dev);

int nix_inl_nix_register_irqs(struct nix_inl_dev *inl_dev);
void nix_inl_nix_unregister_irqs(struct nix_inl_dev *inl_dev);

uint16_t nix_inl_dev_pffunc_get(void);

#endif /* _ROC_NIX_INL_PRIV_H_ */
