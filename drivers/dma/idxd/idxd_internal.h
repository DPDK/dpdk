/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021 Intel Corporation
 */

#ifndef _IDXD_INTERNAL_H_
#define _IDXD_INTERNAL_H_

#include <rte_dmadev_pmd.h>
#include <rte_spinlock.h>

#include "idxd_hw_defs.h"

/**
 * @file idxd_internal.h
 *
 * Internal data structures for the idxd/DSA driver for dmadev
 *
 * @warning
 * @b EXPERIMENTAL: these structures and APIs may change without prior notice
 */

extern int idxd_pmd_logtype;

#define IDXD_PMD_LOG(level, fmt, args...) rte_log(RTE_LOG_ ## level, \
		idxd_pmd_logtype, "IDXD: %s(): " fmt "\n", __func__, ##args)

#define IDXD_PMD_DEBUG(fmt, args...)  IDXD_PMD_LOG(DEBUG, fmt, ## args)
#define IDXD_PMD_INFO(fmt, args...)   IDXD_PMD_LOG(INFO, fmt, ## args)
#define IDXD_PMD_ERR(fmt, args...)    IDXD_PMD_LOG(ERR, fmt, ## args)
#define IDXD_PMD_WARN(fmt, args...)   IDXD_PMD_LOG(WARNING, fmt, ## args)

struct idxd_pci_common {
	rte_spinlock_t lk;

	uint8_t wq_cfg_sz;
	volatile struct rte_idxd_bar0 *regs;
	volatile uint32_t *wq_regs_base;
	volatile struct rte_idxd_grpcfg *grp_regs;
	volatile void *portals;
};

struct idxd_dmadev {
	/* counters to track the batches */
	unsigned short max_batches;
	unsigned short batch_idx_read;
	unsigned short batch_idx_write;

	/* track descriptors and handles */
	unsigned short desc_ring_mask;
	unsigned short ids_avail; /* handles for ops completed */
	unsigned short ids_returned; /* the read pointer for hdls/desc rings */
	unsigned short batch_start; /* start+size == write pointer for hdls/desc */
	unsigned short batch_size;

	void *portal; /* address to write the batch descriptor */

	struct idxd_completion *batch_comp_ring;
	unsigned short *batch_idx_ring; /* store where each batch ends */

	rte_iova_t batch_iova; /* base address of the batch comp ring */
	rte_iova_t desc_iova; /* base address of desc ring, needed for completions */

	unsigned short max_batch_size;

	struct rte_dma_dev *dmadev;
	uint8_t sva_support;
	uint8_t qid;

	union {
		struct {
			unsigned int dsa_id;
		} bus;

		struct idxd_pci_common *pci;
	} u;
};

int idxd_dmadev_create(const char *name, struct rte_device *dev,
		const struct idxd_dmadev *base_idxd, const struct rte_dma_dev_ops *ops);

#endif /* _IDXD_INTERNAL_H_ */
