/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021 6WIND S.A.
 * Copyright 2021 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_COMMON_DEFS_H_
#define RTE_PMD_MLX5_COMMON_DEFS_H_

#include "mlx5_autoconf.h"

/* Size of per-queue MR cache array for linear search. */
#define MLX5_MR_CACHE_N 8

/* Size of MR cache table for binary search. */
#define MLX5_MR_BTREE_CACHE_N 256

/*
 * Defines the amount of retries to allocate the first UAR in the page.
 * OFED 5.0.x and Upstream rdma_core before v29 returned the NULL as
 * UAR base address if UAR was not the first object in the UAR page.
 * It caused the PMD failure and we should try to get another UAR
 * till we get the first one with non-NULL base address returned.
 */
#define MLX5_ALLOC_UAR_RETRY 32

/* Environment variable to control the doorbell register mapping. */
#define MLX5_SHUT_UP_BF "MLX5_SHUT_UP_BF"
#if defined(RTE_ARCH_ARM64)
#define MLX5_SHUT_UP_BF_DEFAULT "0"
#else
#define MLX5_SHUT_UP_BF_DEFAULT "1"
#endif

/* Default PMD specific parameter value. */
#define MLX5_ARG_UNSET (-1)

/* MLX5_SQ_DB_NC supported values. */
#define MLX5_SQ_DB_CACHED 0
#define MLX5_SQ_DB_NCACHED 1
#define MLX5_SQ_DB_HEURISTIC 2

/* Fields of memory mapping type in offset parameter of mmap() */
#define MLX5_UAR_MMAP_CMD_SHIFT 8
#define MLX5_UAR_MMAP_CMD_MASK 0xff

#ifndef HAVE_MLX5DV_MMAP_GET_NC_PAGES_CMD
#define MLX5_MMAP_GET_NC_PAGES_CMD 3
#endif

#define MLX5_VDPA_MAX_RETRIES 20
#define MLX5_VDPA_USEC 1000

#endif /* RTE_PMD_MLX5_COMMON_DEFS_H_ */
