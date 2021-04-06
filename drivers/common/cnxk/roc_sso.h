/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_SSO_H_
#define _ROC_SSO_H_

struct roc_sso {
	struct plt_pci_device *pci_dev;
	/* Public data. */
	uint16_t max_hwgrp;
	uint16_t max_hws;
	uint16_t nb_hwgrp;
	uint8_t nb_hws;
	uintptr_t lmt_base;
	/* HW Const. */
	uint32_t xae_waes;
	uint32_t xaq_buf_size;
	uint32_t iue;
	/* Private data. */
#define ROC_SSO_MEM_SZ (16 * 1024)
	uint8_t reserved[ROC_SSO_MEM_SZ] __plt_cache_aligned;
} __plt_cache_aligned;

/* SSO device initialization */
int __roc_api roc_sso_dev_init(struct roc_sso *roc_sso);
int __roc_api roc_sso_dev_fini(struct roc_sso *roc_sso);

/* SSO device configuration */
int __roc_api roc_sso_rsrc_init(struct roc_sso *roc_sso, uint8_t nb_hws,
				uint16_t nb_hwgrp);
void __roc_api roc_sso_rsrc_fini(struct roc_sso *roc_sso);
uint64_t __roc_api roc_sso_ns_to_gw(struct roc_sso *roc_sso, uint64_t ns);
int __roc_api roc_sso_hws_link(struct roc_sso *roc_sso, uint8_t hws,
			       uint16_t hwgrp[], uint16_t nb_hwgrp);
int __roc_api roc_sso_hws_unlink(struct roc_sso *roc_sso, uint8_t hws,
				 uint16_t hwgrp[], uint16_t nb_hwgrp);
uintptr_t __roc_api roc_sso_hws_base_get(struct roc_sso *roc_sso, uint8_t hws);

#endif /* _ROC_SSOW_H_ */
