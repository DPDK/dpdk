/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_IDEV_H_
#define _ROC_IDEV_H_

uint32_t __roc_api roc_idev_npa_maxpools_get(void);
void __roc_api roc_idev_npa_maxpools_set(uint32_t max_pools);

int __roc_api roc_idev_npa_halo_ena_get(void);
void __roc_api roc_idev_npa_halo_ena_set(int halo_ena);

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

uint8_t __roc_api roc_idev_nix_rx_inject_get(uint16_t port);
void __roc_api roc_idev_nix_rx_inject_set(uint16_t port, uint8_t enable);
uint16_t *__roc_api roc_idev_nix_rx_chan_base_get(void);
void __roc_api roc_idev_nix_rx_chan_set(uint16_t port, uint16_t chan);

uint16_t __roc_api roc_idev_nix_inl_dev_pffunc_get(void);

struct roc_rvu_lf *__roc_api roc_idev_rvu_lf_get(uint8_t rvu_lf_idx);
void __roc_api roc_idev_rvu_lf_set(struct roc_rvu_lf *rvu);
void __roc_api roc_idev_rvu_lf_free(struct roc_rvu_lf *rvu);
#endif /* _ROC_IDEV_H_ */
