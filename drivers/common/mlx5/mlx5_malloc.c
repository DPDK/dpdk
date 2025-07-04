/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#include <errno.h>
#include <eal_export.h>
#include <rte_malloc.h>
#include <malloc.h>
#include <stdbool.h>
#include <string.h>

#include "mlx5_common_log.h"
#include "mlx5_common_os.h"
#include "mlx5_malloc.h"

struct mlx5_sys_mem {
	uint32_t init:1; /* Memory allocator initialized. */
	uint32_t enable:1; /* System memory select. */
	uint32_t reserve:30; /* Reserve. */
	RTE_ATOMIC(struct rte_memseg_list *) last_msl;
	/* last allocated rte memory memseg list. */
#ifdef RTE_LIBRTE_MLX5_DEBUG
	uint64_t malloc_sys;
	/* Memory allocated from system count. */
	uint64_t malloc_rte;
	/* Memory allocated from hugepage count. */
	uint64_t realloc_sys;
	/* Memory reallocate from system count. */
	uint64_t realloc_rte;
	/* Memory reallocate from hugepage count. */
	uint64_t free_sys;
	/* Memory free to system count. */
	uint64_t free_rte;
	/* Memory free to hugepage count. */
	uint64_t msl_miss;
	/* MSL miss count. */
	uint64_t msl_update;
	/* MSL update count. */
#endif
};

/* Initialize default as not */
static struct mlx5_sys_mem mlx5_sys_mem = {
	.init = 0,
	.enable = 0,
#ifdef RTE_LIBRTE_MLX5_DEBUG
	.malloc_sys = 0,
	.malloc_rte = 0,
	.realloc_sys = 0,
	.realloc_rte = 0,
	.free_sys = 0,
	.free_rte = 0,
	.msl_miss = 0,
	.msl_update = 0,
#endif
};

/**
 * Check if the address belongs to memory seg list.
 *
 * @param addr
 *   Memory address to be checked.
 * @param msl
 *   Memory seg list.
 *
 * @return
 *   True if it belongs, false otherwise.
 */
static bool
mlx5_mem_check_msl(void *addr, struct rte_memseg_list *msl)
{
	void *start, *end;

	if (!msl)
		return false;
	start = msl->base_va;
	end = RTE_PTR_ADD(start, msl->len);
	if (addr >= start && addr < end)
		return true;
	return false;
}

/**
 * Update the msl if memory belongs to new msl.
 *
 * @param addr
 *   Memory address.
 */
static void
mlx5_mem_update_msl(void *addr)
{
	/*
	 * Update the cache msl if the new addr comes from the new msl
	 * different with the cached msl.
	 */
	if (addr && !mlx5_mem_check_msl(addr,
	    (struct rte_memseg_list *)rte_atomic_load_explicit
	    (&mlx5_sys_mem.last_msl, rte_memory_order_relaxed))) {
		rte_atomic_store_explicit(&mlx5_sys_mem.last_msl,
			rte_mem_virt2memseg_list(addr),
			rte_memory_order_relaxed);
#ifdef RTE_LIBRTE_MLX5_DEBUG
		rte_atomic_fetch_add_explicit(&mlx5_sys_mem.msl_update, 1,
				   rte_memory_order_relaxed);
#endif
	}
}

/**
 * Check if the address belongs to rte memory.
 *
 * @param addr
 *   Memory address to be checked.
 *
 * @return
 *   True if it belongs, false otherwise.
 */
static bool
mlx5_mem_is_rte(void *addr)
{
	/*
	 * Check if the last cache msl matches. Drop to slow path
	 * to check if the memory belongs to rte memory.
	 */
	if (!mlx5_mem_check_msl(addr, (struct rte_memseg_list *)
	    rte_atomic_load_explicit(&mlx5_sys_mem.last_msl, rte_memory_order_relaxed))) {
		if (!rte_mem_virt2memseg_list(addr))
			return false;
#ifdef RTE_LIBRTE_MLX5_DEBUG
		rte_atomic_fetch_add_explicit(&mlx5_sys_mem.msl_miss, 1, rte_memory_order_relaxed);
#endif
	}
	return true;
}

/**
 * Allocate memory with alignment.
 *
 * @param size
 *   Memory size to be allocated.
 * @param align
 *   Memory alignment.
 * @param zero
 *   Clear the allocated memory or not.
 *
 * @return
 *   Pointer of the allocated memory, NULL otherwise.
 */
static void *
mlx5_alloc_align(size_t size, unsigned int align, unsigned int zero)
{
	void *buf;

	buf = mlx5_os_malloc(align, size);
	if (!buf) {
		DRV_LOG(ERR, "Couldn't allocate buf size=%zu align=%u.",
			size, align);
		return NULL;
	}
	if (zero)
		memset(buf, 0, size);
	return buf;
}

static void *
mlx5_malloc_socket_internal(size_t size, unsigned int align, int socket, bool zero)
{
	return zero ? rte_zmalloc_socket(NULL, size, align, socket) :
		      rte_malloc_socket(NULL, size, align, socket);
}

RTE_EXPORT_INTERNAL_SYMBOL(mlx5_malloc)
void *
mlx5_malloc(uint32_t flags, size_t size, unsigned int align, int socket)
{
	void *addr;
	bool rte_mem;

	/*
	 * If neither system memory nor rte memory is required, allocate
	 * memory according to mlx5_sys_mem.enable.
	 */
	if (flags & MLX5_MEM_RTE)
		rte_mem = true;
	else if (flags & MLX5_MEM_SYS)
		rte_mem = false;
	else
		rte_mem = mlx5_sys_mem.enable ? false : true;
	if (rte_mem) {
		addr = mlx5_malloc_socket_internal(size, align, socket, !!(flags & MLX5_MEM_ZERO));
		if (addr == NULL && socket != SOCKET_ID_ANY && (flags & MLX5_NUMA_TOLERANT)) {
			size_t alloc_size = size;
			addr = mlx5_malloc_socket_internal(size, align, SOCKET_ID_ANY,
				!!(flags & MLX5_MEM_ZERO));
			if (addr) {
				DRV_LOG(WARNING, "Allocated %p (size %zu socket %d) through NUMA tolerant fallback",
					(addr), (alloc_size), (socket));
			}
		}
		mlx5_mem_update_msl(addr);
#ifdef RTE_LIBRTE_MLX5_DEBUG
		if (addr)
			rte_atomic_fetch_add_explicit(&mlx5_sys_mem.malloc_rte, 1,
					   rte_memory_order_relaxed);
#endif
		return addr;
	}
	/* The memory will be allocated from system. */
	if (align > MLX5_MALLOC_ALIGNMENT)
		addr = mlx5_alloc_align(size, align, !!(flags & MLX5_MEM_ZERO));
	else if (flags & MLX5_MEM_ZERO)
		addr = calloc(1, size);
	else
		addr = malloc(size);
#ifdef RTE_LIBRTE_MLX5_DEBUG
	if (addr)
		rte_atomic_fetch_add_explicit(&mlx5_sys_mem.malloc_sys, 1,
				   rte_memory_order_relaxed);
#endif
	return addr;
}

RTE_EXPORT_INTERNAL_SYMBOL(mlx5_realloc)
void *
mlx5_realloc(void *addr, uint32_t flags, size_t size, unsigned int align,
	     int socket)
{
	void *new_addr;
	bool rte_mem;

	/* Allocate directly if old memory address is NULL. */
	if (!addr)
		return mlx5_malloc(flags, size, align, socket);
	/* Get the memory type. */
	if (flags & MLX5_MEM_RTE)
		rte_mem = true;
	else if (flags & MLX5_MEM_SYS)
		rte_mem = false;
	else
		rte_mem = mlx5_sys_mem.enable ? false : true;
	/* Check if old memory and to be allocated memory are the same type. */
	if (rte_mem != mlx5_mem_is_rte(addr)) {
		DRV_LOG(ERR, "Couldn't reallocate to different memory type.");
		return NULL;
	}
	/* Allocate memory from rte memory. */
	if (rte_mem) {
		new_addr = rte_realloc_socket(addr, size, align, socket);
		mlx5_mem_update_msl(new_addr);
#ifdef RTE_LIBRTE_MLX5_DEBUG
		if (new_addr)
			rte_atomic_fetch_add_explicit(&mlx5_sys_mem.realloc_rte, 1,
					   rte_memory_order_relaxed);
#endif
		return new_addr;
	}
	/* Align is not supported for system memory. */
	if (align) {
		DRV_LOG(ERR, "Couldn't reallocate with alignment");
		return NULL;
	}
	new_addr = realloc(addr, size);
#ifdef RTE_LIBRTE_MLX5_DEBUG
	if (new_addr)
		rte_atomic_fetch_add_explicit(&mlx5_sys_mem.realloc_sys, 1,
				   rte_memory_order_relaxed);
#endif
	return new_addr;
}

RTE_EXPORT_INTERNAL_SYMBOL(mlx5_free)
void
mlx5_free(void *addr)
{
	if (addr == NULL)
		return;
	if (!mlx5_mem_is_rte(addr)) {
#ifdef RTE_LIBRTE_MLX5_DEBUG
		rte_atomic_fetch_add_explicit(&mlx5_sys_mem.free_sys, 1,
				   rte_memory_order_relaxed);
#endif
		mlx5_os_free(addr);
	} else {
#ifdef RTE_LIBRTE_MLX5_DEBUG
		rte_atomic_fetch_add_explicit(&mlx5_sys_mem.free_rte, 1,
				   rte_memory_order_relaxed);
#endif
		rte_free(addr);
	}
}

RTE_EXPORT_INTERNAL_SYMBOL(mlx5_memory_stat_dump)
void
mlx5_memory_stat_dump(void)
{
#ifdef RTE_LIBRTE_MLX5_DEBUG
	DRV_LOG(INFO, "System memory malloc:%"PRIi64", realloc:%"PRIi64","
		" free:%"PRIi64"\nRTE memory malloc:%"PRIi64","
		" realloc:%"PRIi64", free:%"PRIi64"\nMSL miss:%"PRIi64","
		" update:%"PRIi64"",
		rte_atomic_load_explicit(&mlx5_sys_mem.malloc_sys, rte_memory_order_relaxed),
		rte_atomic_load_explicit(&mlx5_sys_mem.realloc_sys, rte_memory_order_relaxed),
		rte_atomic_load_explicit(&mlx5_sys_mem.free_sys, rte_memory_order_relaxed),
		rte_atomic_load_explicit(&mlx5_sys_mem.malloc_rte, rte_memory_order_relaxed),
		rte_atomic_load_explicit(&mlx5_sys_mem.realloc_rte, rte_memory_order_relaxed),
		rte_atomic_load_explicit(&mlx5_sys_mem.free_rte, rte_memory_order_relaxed),
		rte_atomic_load_explicit(&mlx5_sys_mem.msl_miss, rte_memory_order_relaxed),
		rte_atomic_load_explicit(&mlx5_sys_mem.msl_update, rte_memory_order_relaxed));
#endif
}

void
mlx5_malloc_mem_select(uint32_t sys_mem_en)
{
	/*
	 * The initialization should be called only once and all devices
	 * should use the same memory type. Otherwise, when new device is
	 * being attached with some different memory allocation configuration,
	 * the memory will get wrong behavior or a failure will be raised.
	 */
	if (!mlx5_sys_mem.init) {
		if (sys_mem_en)
			mlx5_sys_mem.enable = 1;
		mlx5_sys_mem.init = 1;
		DRV_LOG(INFO, "%s is selected.", sys_mem_en ? "SYS_MEM" : "RTE_MEM");
	} else if (mlx5_sys_mem.enable != sys_mem_en) {
		DRV_LOG(WARNING, "%s is already selected.",
			mlx5_sys_mem.enable ? "SYS_MEM" : "RTE_MEM");
	}
}
