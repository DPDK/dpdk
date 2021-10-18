/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021 Intel Corporation
 */

#ifndef _IOAT_INTERNAL_H_
#define _IOAT_INTERNAL_H_

#include "ioat_hw_defs.h"

struct ioat_dmadev {
	struct rte_dma_dev *dmadev;
	struct rte_dma_vchan_conf qcfg;
	struct rte_dma_stats stats;

	volatile uint16_t *doorbell __rte_cache_aligned;
	phys_addr_t status_addr;
	phys_addr_t ring_addr;

	struct ioat_dma_hw_desc *desc_ring;

	unsigned short next_read;
	unsigned short next_write;
	unsigned short last_write; /* Used to compute submitted count. */
	unsigned short offset; /* Used after a device recovery when counts -> 0. */
	unsigned int failure; /* Used to store chanerr for error handling. */

	/* To report completions, the device will write status back here. */
	volatile uint64_t status __rte_cache_aligned;

	/* Pointer to the register bar. */
	volatile struct ioat_registers *regs;

	/* Store the IOAT version. */
	uint8_t version;
};

extern int ioat_pmd_logtype;

#define IOAT_PMD_LOG(level, fmt, args...) rte_log(RTE_LOG_ ## level, \
		ioat_pmd_logtype, "IOAT: %s(): " fmt "\n", __func__, ##args)

#define IOAT_PMD_DEBUG(fmt, args...)  IOAT_PMD_LOG(DEBUG, fmt, ## args)
#define IOAT_PMD_INFO(fmt, args...)   IOAT_PMD_LOG(INFO, fmt, ## args)
#define IOAT_PMD_ERR(fmt, args...)    IOAT_PMD_LOG(ERR, fmt, ## args)
#define IOAT_PMD_WARN(fmt, args...)   IOAT_PMD_LOG(WARNING, fmt, ## args)

#endif /* _IOAT_INTERNAL_H_ */
