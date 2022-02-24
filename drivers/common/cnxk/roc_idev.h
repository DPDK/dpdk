/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_IDEV_H_
#define _ROC_IDEV_H_

uint32_t __roc_api roc_idev_npa_maxpools_get(void);
void __roc_api roc_idev_npa_maxpools_set(uint32_t max_pools);

/* LMT */
uint64_t __roc_api roc_idev_lmt_base_addr_get(void);
uint16_t __roc_api roc_idev_num_lmtlines_get(void);

struct roc_cpt *__roc_api roc_idev_cpt_get(void);
void __roc_api roc_idev_cpt_set(struct roc_cpt *cpt);

struct roc_nix *__roc_api roc_idev_npa_nix_get(void);

uint64_t *__roc_api roc_nix_inl_outb_ring_base_get(struct roc_nix *roc_nix);

#endif /* _ROC_IDEV_H_ */
