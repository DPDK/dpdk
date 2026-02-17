/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_SSO_H_
#define _ROC_SSO_H_

#include "hw/ssow.h"

#define ROC_SSO_AW_PER_LMT_LINE_LOG2 3
#define ROC_SSO_MAX_HWGRP_PER_PF     256

struct roc_sso_hwgrp_qos {
	uint16_t hwgrp;
	uint8_t xaq_prcnt;
	uint8_t iaq_prcnt;
	uint8_t taq_prcnt;
};

struct roc_sso_hwgrp_stash {
	uint16_t hwgrp;
	uint8_t stash_offset;
	uint8_t stash_count;
};

struct roc_sso_hws_stats {
	uint64_t arbitration;
};

struct roc_sso_hwgrp_stats {
	uint64_t ws_pc;
	uint64_t ext_pc;
	uint64_t wa_pc;
	uint64_t ts_pc;
	uint64_t ds_pc;
	uint64_t dq_pc;
	uint64_t aw_status;
	uint64_t page_cnt;
};

struct roc_sso_xaq_data {
	uint32_t nb_xaq;
	uint32_t nb_xae;
	uint32_t xaq_lmt;
	uint64_t aura_handle;
	int halo_ena;
	void *fc;
	void *mem;
};

struct roc_sso_agq_data {
	uint8_t tt;
	uint8_t cnt_ena;
	uint8_t xqe_type;
	uint16_t stag;
	uint32_t tag;
	uint32_t vwqe_max_sz_exp;
	uint64_t vwqe_wait_tmo;
	uint64_t vwqe_aura;
};

struct roc_sso {
	struct plt_pci_device *pci_dev;
	/* Public data. */
	uint16_t max_hwgrp;
	uint16_t max_hws;
	uint16_t nb_hwgrp;
	uint8_t nb_hws;
	uintptr_t lmt_base;
	struct roc_sso_xaq_data xaq;
	/* HW Const. */
	struct sso_feat_info feat;
	/* Private data. */
#define ROC_SSO_MEM_SZ (16 * 1024)
	uint8_t reserved[ROC_SSO_MEM_SZ] __plt_cache_aligned;
} __plt_cache_aligned;

/* SSO device initialization */
int __roc_api roc_sso_dev_init(struct roc_sso *roc_sso);
int __roc_api roc_sso_dev_fini(struct roc_sso *roc_sso);

/* SSO device configuration */
int __roc_api roc_sso_rsrc_init(struct roc_sso *roc_sso, uint8_t nb_hws, uint16_t nb_hwgrp,
				uint16_t nb_tim_lfs);
void __roc_api roc_sso_rsrc_fini(struct roc_sso *roc_sso);
int __roc_api roc_sso_hwgrp_qos_config(struct roc_sso *roc_sso,
				       struct roc_sso_hwgrp_qos *qos,
				       uint16_t nb_qos);
int __roc_api roc_sso_hwgrp_alloc_xaq(struct roc_sso *roc_sso,
				      uint32_t npa_aura_id, uint16_t hwgrps);
int __roc_api roc_sso_hwgrp_release_xaq(struct roc_sso *roc_sso,
					uint16_t hwgrps);
int __roc_api roc_sso_hwgrp_set_priority(struct roc_sso *roc_sso,
					 uint16_t hwgrp, uint8_t weight,
					 uint8_t affinity, uint8_t priority);
uint64_t __roc_api roc_sso_ns_to_gw(uint64_t base, uint64_t ns);
int __roc_api roc_sso_hws_link(struct roc_sso *roc_sso, uint8_t hws, uint16_t hwgrp[],
			       uint16_t nb_hwgrp, uint8_t set, bool use_mbox);
int __roc_api roc_sso_hws_unlink(struct roc_sso *roc_sso, uint8_t hws, uint16_t hwgrp[],
				 uint16_t nb_hwgrp, uint8_t set, bool use_mbox);
int __roc_api roc_sso_hwgrp_hws_link_status(struct roc_sso *roc_sso,
					    uint8_t hws, uint16_t hwgrp);
uintptr_t __roc_api roc_sso_hws_base_get(struct roc_sso *roc_sso, uint8_t hws);
uintptr_t __roc_api roc_sso_hwgrp_base_get(struct roc_sso *roc_sso,
					   uint16_t hwgrp);
int __roc_api roc_sso_hwgrp_init_xaq_aura(struct roc_sso *roc_sso,
					  uint32_t nb_xae);
int __roc_api roc_sso_hwgrp_free_xaq_aura(struct roc_sso *roc_sso,
					  uint16_t nb_hwgrp);
int __roc_api roc_sso_hwgrp_stash_config(struct roc_sso *roc_sso,
					 struct roc_sso_hwgrp_stash *stash,
					 uint16_t nb_stash);
void __roc_api roc_sso_hws_gwc_invalidate(struct roc_sso *roc_sso, uint8_t *hws,
					  uint8_t nb_hws);
int __roc_api roc_sso_hwgrp_agq_alloc(struct roc_sso *roc_sso, uint16_t hwgrp,
				      struct roc_sso_agq_data *data, uint32_t *agq_id);
void __roc_api roc_sso_hwgrp_agq_free(struct roc_sso *roc_sso, uint16_t hwgrp, uint32_t agq_id);
void __roc_api roc_sso_hwgrp_agq_release(struct roc_sso *roc_sso, uint16_t hwgrp);
uint32_t __roc_api roc_sso_hwgrp_agq_from_tag(struct roc_sso *roc_sso, uint16_t hwgrp, uint32_t tag,
					      uint8_t xqe_type);

/* Utility function */
uint16_t __roc_api roc_sso_pf_func_get(void);

/* Debug */
void __roc_api roc_sso_dump(struct roc_sso *roc_sso, uint8_t nb_hws,
			    uint16_t hwgrp, FILE *f);
int __roc_api roc_sso_hwgrp_stats_get(struct roc_sso *roc_sso, uint16_t hwgrp,
				      struct roc_sso_hwgrp_stats *stats);
int __roc_api roc_sso_hws_stats_get(struct roc_sso *roc_sso, uint8_t hws,
				    struct roc_sso_hws_stats *stats);

#endif /* _ROC_SSOW_H_ */
