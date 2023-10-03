/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_SSO_PRIV_H_
#define _ROC_SSO_PRIV_H_

struct sso_rsrc {
	uint16_t rsrc_id;
	uint64_t base;
};

struct sso {
	struct plt_pci_device *pci_dev;
	struct dev dev;
	/* Interrupt handler args. */
	struct sso_rsrc hws_rsrc[MAX_RVU_BLKLF_CNT];
	struct sso_rsrc hwgrp_rsrc[MAX_RVU_BLKLF_CNT];
	/* MSIX offsets */
	uint16_t hws_msix_offset[MAX_RVU_BLKLF_CNT];
	uint16_t hwgrp_msix_offset[MAX_RVU_BLKLF_CNT];
	/* SSO link mapping. */
	struct plt_bitmap **link_map;
	void *link_map_mem;
} __plt_cache_aligned;

enum sso_err_status {
	SSO_ERR_PARAM = -4096,
	SSO_ERR_DEVICE_NOT_BOUNDED = -4097,
};

enum sso_lf_type {
	SSO_LF_TYPE_HWS,
	SSO_LF_TYPE_HWGRP,
};

static inline struct sso *
roc_sso_to_sso_priv(struct roc_sso *roc_sso)
{
	return (struct sso *)&roc_sso->reserved[0];
}

/* SSO LF ops */
int sso_lf_alloc(struct dev *dev, enum sso_lf_type lf_type, uint16_t nb_lf,
		 void **rsp);
int sso_lf_free(struct dev *dev, enum sso_lf_type lf_type, uint16_t nb_lf);
void sso_hws_link_modify(uint8_t hws, uintptr_t base, struct plt_bitmap *bmp, uint16_t hwgrp[],
			 uint16_t n, uint8_t set, uint16_t enable);
int sso_hwgrp_alloc_xaq(struct dev *dev, uint32_t npa_aura_id, uint16_t hwgrps);
int sso_hwgrp_release_xaq(struct dev *dev, uint16_t hwgrps);
int sso_hwgrp_init_xaq_aura(struct dev *dev, struct roc_sso_xaq_data *xaq,
			    uint32_t nb_xae, uint32_t xae_waes,
			    uint32_t xaq_buf_size, uint16_t nb_hwgrp);
int sso_hwgrp_free_xaq_aura(struct dev *dev, struct roc_sso_xaq_data *xaq,
			    uint16_t nb_hwgrp);

/* SSO IRQ */
int sso_register_irqs_priv(struct roc_sso *roc_sso,
			   struct plt_intr_handle *handle, uint16_t nb_hws,
			   uint16_t nb_hwgrp);
void sso_unregister_irqs_priv(struct roc_sso *roc_sso,
			      struct plt_intr_handle *handle, uint16_t nb_hws,
			      uint16_t nb_hwgrp);

#endif /* _ROC_SSO_PRIV_H_ */
