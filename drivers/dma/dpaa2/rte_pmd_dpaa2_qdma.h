/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021-2023 NXP
 */

#ifndef _RTE_PMD_DPAA2_QDMA_H_
#define _RTE_PMD_DPAA2_QDMA_H_

#include <rte_compat.h>

#define RTE_DPAA2_QDMA_COPY_IDX_OFFSET 8
#define RTE_DPAA2_QDMA_SG_IDX_ADDR_ALIGN \
	RTE_BIT64(RTE_DPAA2_QDMA_COPY_IDX_OFFSET)
#define RTE_DPAA2_QDMA_SG_IDX_ADDR_MASK \
	(RTE_DPAA2_QDMA_SG_IDX_ADDR_ALIGN - 1)
#define RTE_DPAA2_QDMA_SG_SUBMIT(idx_addr, flag) \
	(((uint64_t)idx_addr) | (flag))

#define RTE_DPAA2_QDMA_COPY_SUBMIT(idx, flag) \
	((idx << RTE_DPAA2_QDMA_COPY_IDX_OFFSET) | (flag))

#define RTE_DPAA2_QDMA_JOB_SUBMIT_MAX (32 + 8)
#define RTE_DMA_CAPA_DPAA2_QDMA_FLAGS_INDEX RTE_BIT64(63)
#endif /* _RTE_PMD_DPAA2_QDMA_H_ */
