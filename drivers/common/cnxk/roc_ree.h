/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_REE_H_
#define _ROC_REE_H_

#include "roc_api.h"

#define REE_MAX_LFS	       36
#define REE_MAX_QUEUES_PER_VF  36
#define REE_MAX_MATCHES_PER_VF 254

#define REE_MAX_PAYLOAD_SIZE (1 << 14)

#define REE_NON_INC_PROG 0
#define REE_INC_PROG	 1

#define REE_MOD_INC(i, l) ((i) == (l - 1) ? (i) = 0 : (i)++)

/**
 * Device vf data
 */
struct roc_ree_vf {
	struct plt_pci_device *pci_dev;
	struct dev *dev;
	/**< Base class */
	uint16_t max_queues;
	/**< Max queues supported */
	uint8_t nb_queues;
	/**< Number of regex queues attached */
	uint16_t max_matches;
	/**<  Max matches supported*/
	uint16_t lf_msixoff[REE_MAX_LFS];
	/**< MSI-X offsets */
	uint8_t block_address;
	/**< REE Block Address */
	uint8_t err_intr_registered : 1;
	/**< Are error interrupts registered? */

#define ROC_REE_MEM_SZ (6 * 1024)
	uint8_t reserved[ROC_REE_MEM_SZ] __plt_cache_aligned;
} __plt_cache_aligned;

struct roc_ree_rid {
	uintptr_t rid;
	/** Request id of a ree operation */
	uint64_t user_id;
	/* Client data */
	/**< IOVA address of the pattern to be matched. */
};

struct roc_ree_pending_queue {
	uint64_t pending_count;
	/** Pending requests count */
	struct roc_ree_rid *rid_queue;
	/** Array of pending requests */
	uint16_t enq_tail;
	/** Tail of queue to be used for enqueue */
	uint16_t deq_head;
	/** Head of queue to be used for dequeue */
};

struct roc_ree_qp {
	uint32_t id;
	/**< Queue pair id */
	uintptr_t base;
	/**< Base address where BAR is mapped */
	struct roc_ree_pending_queue pend_q;
	/**< Pending queue */
	plt_iova_t iq_dma_addr;
	/**< Instruction queue address */
	uint32_t roc_regexdev_jobid;
	/**< Job ID */
	uint32_t write_offset;
	/**< write offset */
};

union roc_ree_inst {
	uint64_t u[8];
	struct {
		uint64_t doneint : 1;
		uint64_t reserved_1_3 : 3;
		uint64_t dg : 1;
		uint64_t reserved_5_7 : 3;
		uint64_t ooj : 1;
		uint64_t reserved_9_15 : 7;
		uint64_t reserved_16_63 : 48;
		uint64_t inp_ptr_addr : 64;
		uint64_t inp_ptr_ctl : 64;
		uint64_t res_ptr_addr : 64;
		uint64_t wq_ptr : 64;
		uint64_t tag : 32;
		uint64_t tt : 2;
		uint64_t ggrp : 10;
		uint64_t reserved_364_383 : 20;
		uint64_t reserved_384_391 : 8;
		uint64_t ree_job_id : 24;
		uint64_t ree_job_ctrl : 16;
		uint64_t ree_job_length : 15;
		uint64_t reserved_447_447 : 1;
		uint64_t ree_job_subset_id_0 : 16;
		uint64_t ree_job_subset_id_1 : 16;
		uint64_t ree_job_subset_id_2 : 16;
		uint64_t ree_job_subset_id_3 : 16;
	} cn98xx;
};

int __roc_api roc_ree_dev_init(struct roc_ree_vf *vf);
int __roc_api roc_ree_dev_fini(struct roc_ree_vf *vf);
int __roc_api roc_ree_queues_attach(struct roc_ree_vf *vf, uint8_t nb_queues);
int __roc_api roc_ree_queues_detach(struct roc_ree_vf *vf);
int __roc_api roc_ree_msix_offsets_get(struct roc_ree_vf *vf);
int __roc_api roc_ree_config_lf(struct roc_ree_vf *vf, uint8_t lf, uint8_t pri,
				uint32_t size);
int __roc_api roc_ree_af_reg_read(struct roc_ree_vf *vf, uint64_t reg,
				  uint64_t *val);
int __roc_api roc_ree_af_reg_write(struct roc_ree_vf *vf, uint64_t reg,
				   uint64_t val);
int __roc_api roc_ree_rule_db_get(struct roc_ree_vf *vf, char *rule_db,
				  uint32_t rule_db_len, char *rule_dbi,
				  uint32_t rule_dbi_len);
int __roc_api roc_ree_rule_db_len_get(struct roc_ree_vf *vf,
				      uint32_t *rule_db_len,
				      uint32_t *rule_dbi_len);
int __roc_api roc_ree_rule_db_prog(struct roc_ree_vf *vf, const char *rule_db,
				   uint32_t rule_db_len, const char *rule_dbi,
				   uint32_t rule_dbi_len);
uintptr_t __roc_api roc_ree_qp_get_base(struct roc_ree_vf *vf, uint16_t qp_id);
void __roc_api roc_ree_err_intr_unregister(struct roc_ree_vf *vf);
int __roc_api roc_ree_err_intr_register(struct roc_ree_vf *vf);
int __roc_api roc_ree_iq_enable(struct roc_ree_vf *vf,
				const struct roc_ree_qp *qp, uint8_t pri,
				uint32_t size_div128);
void __roc_api roc_ree_iq_disable(struct roc_ree_qp *qp);

#endif /* _ROC_REE_H_ */
