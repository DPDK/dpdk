/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
 * Get the layout of the available physical memory.
 *
 * It can be useful for an application to have the full physical
 * memory layout to decide the size of a memory zone to reserve. This
 * table is stored in rte_config (see rte_eal_get_configuration()).
 *
 * @return
 *  - On success, return a pointer to a read-only table of struct
 *    rte_physmem_desc elements, containing the layout of all
 *    addressable physical memory. The last element of the table
 *    contains a NULL address.
 *  - On error, return NULL. This should not happen since it is a fatal
 *    error that will probably cause the entire system to panic.
 */
const struct rte_memseg *rte_eal_get_physmem_layout(void);

/**
 * Dump the physical memory layout to a file.
 *
 * @param f
 *   A pointer to a file for output
 */
void rte_dump_physmem_layout(FILE *f);

/**
 * Get the total amount of available physical memory.
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

#ifdef __cplusplus
}
#endif

#endif /* _RTE_MEMORY_H_ */
