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
uint64_t __roc_api roc_idev_nix_inl_meta_aura_get(void);
struct roc_nix_list *__roc_api roc_idev_nix_list_get(void);

struct roc_mcs *__roc_api roc_idev_mcs_get(uint8_t mcs_idx);
void __roc_api roc_idev_mcs_set(struct roc_mcs *mcs);
void __roc_api roc_idev_mcs_free(struct roc_mcs *mcs);
#endif /* _ROC_IDEV_H_ */
