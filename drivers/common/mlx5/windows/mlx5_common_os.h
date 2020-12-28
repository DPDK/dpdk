/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_COMMON_OS_H_
#define RTE_PMD_MLX5_COMMON_OS_H_

#include <stdio.h>

#include "mlx5_autoconf.h"
#include "mlx5_glue.h"
#include "mlx5_malloc.h"

/**
 * This API allocates aligned or non-aligned memory.  The free can be on either
 * aligned or nonaligned memory.  To be protected - even though there may be no
 * alignment - in Windows this API will unconditioanlly call _aligned_malloc()
 * with at least a minimal alignment size.
 *
 * @param[in] align
 *    The alignment value, which must be an integer power of 2 (or 0 for
 *    non-alignment)
 * @param[in] size
 *    Size in bytes to allocate
 *
 * @return
 *    Valid pointer to allocated memory, NULL in case of failure
 */
static inline void *
mlx5_os_malloc(size_t align, size_t size)
{
	if (align < MLX5_MALLOC_ALIGNMENT)
		align = MLX5_MALLOC_ALIGNMENT;
	return _aligned_malloc(size, align);
}

/**
 * This API de-allocates a memory that originally could have been allocated
 * aligned or non-aligned. In Windows since the allocation was with
 * _aligned_malloc() - it is safe to always call _aligned_free().
 *
 * @param[in] addr
 *    Pointer to address to free
 *
 */
static inline void
mlx5_os_free(void *addr)
{
	_aligned_free(addr);
}
#endif /* RTE_PMD_MLX5_COMMON_OS_H_ */
