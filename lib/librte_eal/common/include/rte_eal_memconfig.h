/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _RTE_EAL_MEMCONFIG_H_
#define _RTE_EAL_MEMCONFIG_H_

#include <rte_config.h>
#include <rte_tailq.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_malloc_heap.h>
#include <rte_rwlock.h>
#include <rte_pause.h>
#include <rte_fbarray.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * memseg list is a special case as we need to store a bunch of other data
 * together with the array itself.
 */
struct rte_memseg_list {
	RTE_STD_C11
	union {
		void *base_va;
		/**< Base virtual address for this memseg list. */
		uint64_t addr_64;
		/**< Makes sure addr is always 64-bits */
	};
	uint64_t page_sz; /**< Page size for all memsegs in this list. */
	int socket_id; /**< Socket ID for all memsegs in this list. */
	volatile uint32_t version; /**< version number for multiprocess sync. */
	size_t len; /**< Length of memory area covered by this memseg list. */
	unsigned int external; /**< 1 if this list points to external memory */
	struct rte_fbarray memseg_arr;
};

/**
 * the structure for the memory configuration for the RTE.
 * Used by the rte_config structure. It is separated out, as for multi-process
 * support, the memory details should be shared across instances
 */
struct rte_mem_config {
	volatile uint32_t magic;   /**< Magic number - Sanity check. */

	/* memory topology */
	uint32_t nchannel;    /**< Number of channels (0 if unknown). */
	uint32_t nrank;       /**< Number of ranks (0 if unknown). */

	/**
	 * current lock nest order
	 *  - qlock->mlock (ring/hash/lpm)
	 *  - mplock->qlock->mlock (mempool)
	 * Notice:
	 *  *ALWAYS* obtain qlock first if having to obtain both qlock and mlock
	 */
	rte_rwlock_t mlock;   /**< only used by memzone LIB for thread-safe. */
	rte_rwlock_t qlock;   /**< used for tailq operation for thread safe. */
	rte_rwlock_t mplock;  /**< only used by mempool LIB for thread-safe. */

	rte_rwlock_t memory_hotplug_lock;
	/**< indicates whether memory hotplug request is in progress. */

	/* memory segments and zones */
	struct rte_fbarray memzones; /**< Memzone descriptors. */

	struct rte_memseg_list memsegs[RTE_MAX_MEMSEG_LISTS];
	/**< list of dynamic arrays holding memsegs */

	struct rte_tailq_head tailq_head[RTE_MAX_TAILQ]; /**< Tailqs for objects */

	/* Heaps of Malloc */
	struct malloc_heap malloc_heaps[RTE_MAX_HEAPS];

	/* next socket ID for external malloc heap */
	int next_socket_id;

	/* address of mem_config in primary process. used to map shared config into
	 * exact same address the primary process maps it.
	 */
	uint64_t mem_cfg_addr;

	/* legacy mem and single file segments options are shared */
	uint32_t legacy_mem;
	uint32_t single_file_segments;

	/* keeps the more restricted dma mask */
	uint8_t dma_maskbits;
} __attribute__((__packed__));


inline static void
rte_eal_mcfg_wait_complete(struct rte_mem_config* mcfg)
{
	/* wait until shared mem_config finish initialising */
	while(mcfg->magic != RTE_MAGIC)
		rte_pause();
}

#ifdef __cplusplus
}
#endif

#endif /*__RTE_EAL_MEMCONFIG_H_*/
