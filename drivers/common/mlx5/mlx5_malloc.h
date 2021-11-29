/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#ifndef MLX5_MALLOC_H_
#define MLX5_MALLOC_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MLX5_MALLOC_ALIGNMENT
#ifndef RTE_ARCH_64
#define MLX5_MALLOC_ALIGNMENT 8
#else
#define MLX5_MALLOC_ALIGNMENT 16
#endif
#endif

enum mlx5_mem_flags {
	MLX5_MEM_ANY = 0,
	/* Memory will be allocated depends on sys_mem_en. */
	MLX5_MEM_SYS = 1 << 0,
	/* Memory should be allocated from system. */
	MLX5_MEM_RTE = 1 << 1,
	/* Memory should be allocated from rte hugepage. */
	MLX5_MEM_ZERO = 1 << 2,
	/* Memory should be cleared to zero. */
};

/**
 * Select the PMD memory allocate preference.
 *
 * Once sys_mem_en is set, the default memory allocate will from
 * system only if an explicitly flag is set to order the memory
 * from rte hugepage memory.
 *
 * @param sys_mem_en
 *   Use system memory or not.
 */
void mlx5_malloc_mem_select(uint32_t sys_mem_en);

/**
 * Dump the PMD memory usage statistic.
 */
__rte_internal
void mlx5_memory_stat_dump(void);

/**
 * Memory allocate function.
 *
 * @param flags
 *   The bits as enum mlx5_mem_flags defined.
 * @param size
 *   Memory size to be allocated.
 * @param align
 *   Memory alignment.
 * @param socket
 *   The socket memory should allocated.
 *   Valid only when allocate the memory from rte hugepage.
 *
 * @return
 *   Pointer of the allocated memory, NULL otherwise.
 */
__rte_internal
void *mlx5_malloc(uint32_t flags, size_t size, unsigned int align, int socket);

/**
 * Memory reallocate function.
 *
 *
 *
 * @param addr
 *   The memory to be reallocated.
 * @param flags
 *   The bits as enum mlx5_mem_flags defined.
 * @param size
 *   Memory size to be allocated.
 * @param align
 *   Memory alignment.
 * @param socket
 *   The socket memory should allocated.
 *   Valid only when allocate the memory from rte hugepage.
 *
 * @return
 *   Pointer of the allocated memory, NULL otherwise.
 */

__rte_internal
void *mlx5_realloc(void *addr, uint32_t flags, size_t size, unsigned int align,
		   int socket);

/**
 * Memory free function.
 *
 * @param addr
 *   The memory address to be freed..
 */
__rte_internal
void mlx5_free(void *addr);

#ifdef __cplusplus
}
#endif

#endif
