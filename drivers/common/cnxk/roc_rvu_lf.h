/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#ifndef _ROC_RVU_LF_H_
#define _ROC_RVU_LF_H_

#include "roc_platform.h"

struct roc_rvu_lf {
	TAILQ_ENTRY(roc_rvu_lf) next;
	struct plt_pci_device *pci_dev;
	uint8_t idx;
#define ROC_RVU_MEM_SZ (6 * 1024)
	uint8_t reserved[ROC_RVU_MEM_SZ] __plt_cache_aligned;
};

TAILQ_HEAD(roc_rvu_lf_head, roc_rvu_lf);

/* Dev */
int __roc_api roc_rvu_lf_dev_init(struct roc_rvu_lf *roc_rvu_lf);
int __roc_api roc_rvu_lf_dev_fini(struct roc_rvu_lf *roc_rvu_lf);

uint16_t __roc_api roc_rvu_lf_pf_func_get(struct roc_rvu_lf *roc_rvu_lf);

int __roc_api roc_rvu_lf_msg_process(struct roc_rvu_lf *roc_rvu_lf,
				     uint16_t vf, uint16_t msg_id,
				     void *req_data, uint16_t req_len,
				     void *rsp_data, uint16_t rsp_len);

int __roc_api roc_rvu_lf_msg_id_range_set(struct roc_rvu_lf *roc_rvu_lf,
					  uint16_t from, uint16_t to);
bool  __roc_api roc_rvu_lf_msg_id_range_check(struct roc_rvu_lf *roc_rvu_lf, uint16_t msg_id);
typedef void (*roc_rvu_lf_intr_cb_fn)(void *cb_arg);
typedef int (*roc_rvu_lf_msg_handler_cb_fn)(uint16_t vf, uint16_t msg_id,
					    void *req, uint16_t req_len,
					    void **rsp, uint16_t *rsp_len);

int __roc_api roc_rvu_lf_irq_register(struct roc_rvu_lf *roc_rvu_lf, unsigned int irq,
				      roc_rvu_lf_intr_cb_fn cb, void *cb_arg);
int __roc_api roc_rvu_lf_irq_unregister(struct roc_rvu_lf *roc_rvu_lf, unsigned int irq,
					roc_rvu_lf_intr_cb_fn cb, void *cb_arg);
int __roc_api roc_rvu_lf_msg_handler_register(struct roc_rvu_lf *roc_rvu_lf,
					      roc_rvu_lf_msg_handler_cb_fn cb);
int __roc_api roc_rvu_lf_msg_handler_unregister(struct roc_rvu_lf *roc_rvu_lf);
#endif /* _ROC_RVU_LF_H_ */
