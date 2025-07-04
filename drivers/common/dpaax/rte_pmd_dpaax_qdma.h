/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021-2024 NXP
 */

#ifndef RTE_PMD_DPAAX_QDMA_H
#define RTE_PMD_DPAAX_QDMA_H

#include <rte_compat.h>

#define RTE_DPAAX_QDMA_COPY_IDX_OFFSET 8
#define RTE_DPAAX_QDMA_SG_IDX_ADDR_ALIGN \
	RTE_BIT64(RTE_DPAAX_QDMA_COPY_IDX_OFFSET)
#define RTE_DPAAX_QDMA_SG_IDX_ADDR_MASK \
	(RTE_DPAAX_QDMA_SG_IDX_ADDR_ALIGN - 1)
#define RTE_DPAAX_QDMA_SG_SUBMIT(idx_addr, flag) \
	(((uint64_t)idx_addr) | (flag))

#define RTE_DPAAX_QDMA_COPY_SUBMIT(idx, flag) \
	((idx << RTE_DPAAX_QDMA_COPY_IDX_OFFSET) | (flag))

#define RTE_DPAAX_QDMA_JOB_SUBMIT_MAX 64
#define RTE_DMA_CAPA_DPAAX_QDMA_FLAGS_INDEX RTE_BIT64(63)

#endif /* RTE_PMD_DPAAX_QDMA_H */
