/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _RTE_MEMORY_H_
#define _RTE_MEMORY_H_

/**
 * @file
 *
 * Memory-related RTE API.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_common.h>
#include <rte_compat.h>
#include <rte_config.h>

/* forward declaration for pointers */
struct rte_memseg_list;

__extension__
enum rte_page_sizes {
	RTE_PGSIZE_4K    = 1ULL << 12,
	RTE_PGSIZE_64K   = 1ULL << 16,
	RTE_PGSIZE_256K  = 1ULL << 18,
	RTE_PGSIZE_2M    = 1ULL << 21,
	RTE_PGSIZE_16M   = 1ULL << 24,
	RTE_PGSIZE_256M  = 1ULL << 28,
	RTE_PGSIZE_512M  = 1ULL << 29,
	RTE_PGSIZE_1G    = 1ULL << 30,
	RTE_PGSIZE_4G    = 1ULL << 32,
	RTE_PGSIZE_16G   = 1ULL << 34,
};

#define SOCKET_ID_ANY -1                    /**< Any NUMA socket. */
#define RTE_CACHE_LINE_MASK (RTE_CACHE_LINE_SIZE-1) /**< Cache line mask. */

#define RTE_CACHE_LINE_ROUNDUP(size) \
	(RTE_CACHE_LINE_SIZE * ((size + RTE_CACHE_LINE_SIZE - 1) / RTE_CACHE_LINE_SIZE))
/**< Return the first cache-aligned value greater or equal to size. */

/**< Cache line size in terms of log2 */
#if RTE_CACHE_LINE_SIZE == 64
#define RTE_CACHE_LINE_SIZE_LOG2 6
#elif RTE_CACHE_LINE_SIZE == 128
#define RTE_CACHE_LINE_SIZE_LOG2 7
#else
#error "Unsupported cache line size"
#endif

#define RTE_CACHE_LINE_MIN_SIZE 64	/**< Minimum Cache line size. */

/**
 * Force alignment to cache line.
 */
#define __rte_cache_aligned __rte_aligned(RTE_CACHE_LINE_SIZE)

/**
 * Force minimum cache line alignment.
 */
#define __rte_cache_min_aligned __rte_aligned(RTE_CACHE_LINE_MIN_SIZE)

typedef uint64_t phys_addr_t; /**< Physical address. */
#define RTE_BAD_PHYS_ADDR ((phys_addr_t)-1)
/**
 * IO virtual address type.
 * When the physical addressing mode (IOVA as PA) is in use,
 * the translation from an IO virtual address (IOVA) to a physical address
 * is a direct mapping, i.e. the same value.
 * Otherwise, in virtual mode (IOVA as VA), an IOMMU may do the translation.
 */
typedef uint64_t rte_iova_t;
#define RTE_BAD_IOVA ((rte_iova_t)-1)

/**
 * Physical memory segment descriptor.
 */
#define RTE_MEMSEG_FLAG_DO_NOT_FREE (1 << 0)
/**< Prevent this segment from being freed back to the OS. */
struct rte_memseg {
	RTE_STD_C11
	union {
		phys_addr_t phys_addr;  /**< deprecated - Start physical address. */
		rte_iova_t iova;        /**< Start IO address. */
	};
	RTE_STD_C11
	union {
		void *addr;         /**< Start virtual address. */
		uint64_t addr_64;   /**< Makes sure addr is always 64 bits */
	};
	size_t len;               /**< Length of the segment. */
	uint64_t hugepage_sz;       /**< The pagesize of underlying memory */
	int32_t socket_id;          /**< NUMA socket ID. */
	uint32_t nchannel;          /**< Number of channels. */
	uint32_t nrank;             /**< Number of ranks. */
	uint32_t flags;             /**< Memseg-specific flags */
} __rte_packed;

/**
 * Lock page in physical memory and prevent from swapping.
 *
 * @param virt
 *   The virtual address.
 * @return
 *   0 on success, negative on error.
 */
int rte_mem_lock_page(const void *virt);

/**
 * Get physical address of any mapped virtual address in the current process.
 * It is found by browsing the /proc/self/pagemap special file.
 * The page must be locked.
 *
 * @param virt
 *   The virtual address.
 * @return
 *   The physical address or RTE_BAD_IOVA on error.
 */
phys_addr_t rte_mem_virt2phy(const void *virt);

/**
 * Get IO virtual address of any mapped virtual address in the current process.
 *
 * @param virt
 *   The virtual address.
 * @return
 *   The IO address or RTE_BAD_IOVA on error.
 */
rte_iova_t rte_mem_virt2iova(const void *virt);

/**
 * Get virtual memory address corresponding to iova address.
 *
 * @note This function read-locks the memory hotplug subsystem, and thus cannot
 *       be used within memory-related callback functions.
 *
 * @param iova
 *   The iova address.
 * @return
 *   Virtual address corresponding to iova address (or NULL if address does not
 *   exist within DPDK memory map).
 */
__rte_experimental void *
rte_mem_iova2virt(rte_iova_t iova);

/**
 * Get memseg to which a particular virtual address belongs.
 *
 * @param virt
 *   The virtual address.
 * @param msl
 *   The memseg list in which to look up based on ``virt`` address
 *   (can be NULL).
 * @return
 *   Memseg pointer on success, or NULL on error.
 */
__rte_experimental struct rte_memseg *
rte_mem_virt2memseg(const void *virt, const struct rte_memseg_list *msl);

/**
 * Get memseg list corresponding to virtual memory address.
 *
 * @param virt
 *   The virtual address.
 * @return
 *   Memseg list to which this virtual address belongs to.
 */
__rte_experimental struct rte_memseg_list *
rte_mem_virt2memseg_list(const void *virt);

/**
 * Memseg walk function prototype.
 *
 * Returning 0 will continue walk
 * Returning 1 will stop the walk
 * Returning -1 will stop the walk and report error
 */
typedef int (*rte_memseg_walk_t)(const struct rte_memseg_list *msl,
		const struct rte_memseg *ms, void *arg);

/**
 * Memseg contig walk function prototype. This will trigger a callback on every
 * VA-contiguous are starting at memseg ``ms``, so total valid VA space at each
 * callback call will be [``ms->addr``, ``ms->addr + len``).
 *
 * Returning 0 will continue walk
 * Returning 1 will stop the walk
 * Returning -1 will stop the walk and report error
 */
typedef int (*rte_memseg_contig_walk_t)(const struct rte_memseg_list *msl,
		const struct rte_memseg *ms, size_t len, void *arg);

/**
 * Memseg list walk function prototype. This will trigger a callback on every
 * allocated memseg list.
 *
 * Returning 0 will continue walk
 * Returning 1 will stop the walk
 * Returning -1 will stop the walk and report error
 */
typedef int (*rte_memseg_list_walk_t)(const struct rte_memseg_list *msl,
		void *arg);

/**
 * Walk list of all memsegs.
 *
 * @note This function read-locks the memory hotplug subsystem, and thus cannot
 *       be used within memory-related callback functions.
 *
 * @param func
 *   Iterator function
 * @param arg
 *   Argument passed to iterator
 * @return
 *   0 if walked over the entire list
 *   1 if stopped by the user
 *   -1 if user function reported error
 */
int __rte_experimental
rte_memseg_walk(rte_memseg_walk_t func, void *arg);

/**
 * Walk each VA-contiguous area.
 *
 * @note This function read-locks the memory hotplug subsystem, and thus cannot
 *       be used within memory-related callback functions.
 *
 * @param func
 *   Iterator function
 * @param arg
 *   Argument passed to iterator
 * @return
 *   0 if walked over the entire list
 *   1 if stopped by the user
 *   -1 if user function reported error
 */
int __rte_experimental
rte_memseg_contig_walk(rte_memseg_contig_walk_t func, void *arg);

/**
 * Walk each allocated memseg list.
 *
 * @note This function read-locks the memory hotplug subsystem, and thus cannot
 *       be used within memory-related callback functions.
 *
 * @param func
 *   Iterator function
 * @param arg
 *   Argument passed to iterator
 * @return
 *   0 if walked over the entire list
 *   1 if stopped by the user
 *   -1 if user function reported error
 */
int __rte_experimental
rte_memseg_list_walk(rte_memseg_list_walk_t func, void *arg);

/**
 * Dump the physical memory layout to a file.
 *
 * @note This function read-locks the memory hotplug subsystem, and thus cannot
 *       be used within memory-related callback functions.
 *
 * @param f
 *   A pointer to a file for output
 */
void rte_dump_physmem_layout(FILE *f);

/**
 * Get the total amount of available physical memory.
 *
 * @note This function read-locks the memory hotplug subsystem, and thus cannot
 *       be used within memory-related callback functions.
 *
 * @return
 *    The total amount of available physical memory in bytes.
 */
uint64_t rte_eal_get_physmem_size(void);

/**
 * Get the number of memory channels.
 *
 * @return
 *   The number of memory channels on the system. The value is 0 if unknown
 *   or not the same on all devices.
 */
unsigned rte_memory_get_nchannel(void);

/**
 * Get the number of memory ranks.
 *
 * @return
 *   The number of memory ranks on the system. The value is 0 if unknown or
 *   not the same on all devices.
 */
unsigned rte_memory_get_nrank(void);

/**
 * Drivers based on uio will not load unless physical
 * addresses are obtainable. It is only possible to get
 * physical addresses when running as a privileged user.
 *
 * @return
 *   1 if the system is able to obtain physical addresses.
 *   0 if using DMA addresses through an IOMMU.
 */
int rte_eal_using_phys_addrs(void);


/**
 * Enum indicating which kind of memory event has happened. Used by callbacks to
 * distinguish between memory allocations and deallocations.
 */
enum rte_mem_event {
	RTE_MEM_EVENT_ALLOC = 0, /**< Allocation event. */
	RTE_MEM_EVENT_FREE,      /**< Deallocation event. */
};
#define RTE_MEM_EVENT_CALLBACK_NAME_LEN 64
/**< maximum length of callback name */

/**
 * Function typedef used to register callbacks for memory events.
 */
typedef void (*rte_mem_event_callback_t)(enum rte_mem_event event_type,
		const void *addr, size_t len, void *arg);

/**
 * Function used to register callbacks for memory events.
 *
 * @note callbacks will happen while memory hotplug subsystem is write-locked,
 *       therefore some functions (e.g. `rte_memseg_walk()`) will cause a
 *       deadlock when called from within such callbacks.
 *
 * @note mem event callbacks not being supported is an expected error condition,
 *       so user code needs to handle this situation. In these cases, return
 *       value will be -1, and rte_errno will be set to ENOTSUP.
 *
 * @param name
 *   Name associated with specified callback to be added to the list.
 *
 * @param clb
 *   Callback function pointer.
 *
 * @param arg
 *   Argument to pass to the callback.
 *
 * @return
 *   0 on successful callback register
 *   -1 on unsuccessful callback register, with rte_errno value indicating
 *   reason for failure.
 */
int __rte_experimental
rte_mem_event_callback_register(const char *name, rte_mem_event_callback_t clb,
		void *arg);

/**
 * Function used to unregister callbacks for memory events.
 *
 * @param name
 *   Name associated with specified callback to be removed from the list.
 *
 * @param arg
 *   Argument to look for among callbacks with specified callback name.
 *
 * @return
 *   0 on successful callback unregister
 *   -1 on unsuccessful callback unregister, with rte_errno value indicating
 *   reason for failure.
 */
int __rte_experimental
rte_mem_event_callback_unregister(const char *name, void *arg);


#define RTE_MEM_ALLOC_VALIDATOR_NAME_LEN 64
/**< maximum length of alloc validator name */
/**
 * Function typedef used to register memory allocation validation callbacks.
 *
 * Returning 0 will allow allocation attempt to continue. Returning -1 will
 * prevent allocation from succeeding.
 */
typedef int (*rte_mem_alloc_validator_t)(int socket_id,
		size_t cur_limit, size_t new_len);

/**
 * @brief Register validator callback for memory allocations.
 *
 * Callbacks registered by this function will be called right before memory
 * allocator is about to trigger allocation of more pages from the system if
 * said allocation will bring total memory usage above specified limit on
 * specified socket. User will be able to cancel pending allocation if callback
 * returns -1.
 *
 * @note callbacks will happen while memory hotplug subsystem is write-locked,
 *       therefore some functions (e.g. `rte_memseg_walk()`) will cause a
 *       deadlock when called from within such callbacks.
 *
 * @note validator callbacks not being supported is an expected error condition,
 *       so user code needs to handle this situation. In these cases, return
 *       value will be -1, and rte_errno will be set to ENOTSUP.
 *
 * @param name
 *   Name associated with specified callback to be added to the list.
 *
 * @param clb
 *   Callback function pointer.
 *
 * @param socket_id
 *   Socket ID on which to watch for allocations.
 *
 * @param limit
 *   Limit above which to trigger callbacks.
 *
 * @return
 *   0 on successful callback register
 *   -1 on unsuccessful callback register, with rte_errno value indicating
 *   reason for failure.
 */
int __rte_experimental
rte_mem_alloc_validator_register(const char *name,
		rte_mem_alloc_validator_t clb, int socket_id, size_t limit);

/**
 * @brief Unregister validator callback for memory allocations.
 *
 * @param name
 *   Name associated with specified callback to be removed from the list.
 *
 * @param socket_id
 *   Socket ID on which to watch for allocations.
 *
 * @return
 *   0 on successful callback unregister
 *   -1 on unsuccessful callback unregister, with rte_errno value indicating
 *   reason for failure.
 */
int __rte_experimental
rte_mem_alloc_validator_unregister(const char *name, int socket_id);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_MEMORY_H_ */
