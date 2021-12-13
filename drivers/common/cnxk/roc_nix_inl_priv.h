/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#ifndef _ROC_NIX_INL_PRIV_H_
#define _ROC_NIX_INL_PRIV_H_

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
	uint64_t xaq_aura;
	void *xaq_mem;
	roc_nix_inl_sso_work_cb_t work_cb;
	void *cb_args;

	/* NIX data */
	uint8_t lf_tx_stats;
	uint8_t lf_rx_stats;
	uint16_t vwqe_interval;
	uint16_t cints;
	uint16_t qints;
	struct roc_nix_rq rq;
	uint16_t rq_refs;
	bool is_nix1;

	/* NIX/CPT data */
	void *inb_sa_base;
	uint16_t inb_sa_sz;

	/* CPT data */
	struct roc_cpt_lf cpt_lf;

	/* Device arguments */
	uint8_t selftest;
	uint16_t channel;
	uint16_t chan_mask;
	bool is_multi_channel;
	uint16_t ipsec_in_max_spi;
	bool attach_cptlf;
};

int nix_inl_sso_register_irqs(struct nix_inl_dev *inl_dev);
void nix_inl_sso_unregister_irqs(struct nix_inl_dev *inl_dev);

int nix_inl_nix_register_irqs(struct nix_inl_dev *inl_dev);
void nix_inl_nix_unregister_irqs(struct nix_inl_dev *inl_dev);

uint16_t nix_inl_dev_pffunc_get(void);

#endif /* _ROC_NIX_INL_PRIV_H_ */
