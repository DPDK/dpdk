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

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/queue.h>

#include <rte_random.h>
#include <rte_cycles.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_eal_memconfig.h>
#include <rte_common.h>
#include <rte_string_fns.h>

#include "test.h"

/*
 * Memzone
 * =======
 *
 * - Search for three reserved zones or reserve them if they do not exist:
 *
 *   - One is on any socket id.
 *   - The second is on socket 0.
 *   - The last one is on socket 1 (if socket 1 exists).
 *
 * - Check that the zones exist.
 *
 * - Check that the zones are cache-aligned.
 *
 * - Check that zones do not overlap.
 *
 * - Check that the zones are on the correct socket id.
 *
 * - Check that a lookup of the first zone returns the same pointer.
 *
 * - Check that it is not possible to create another zone with the
 *   same name as an existing zone.
 *
 * - Check flags for specific huge page size reservation
 */

/* Test if memory overlaps: return 1 if true, or 0 if false. */
static int
is_memory_overlap(phys_addr_t ptr1, size_t len1, phys_addr_t ptr2, size_t len2)
{
	if (ptr2 >= ptr1 && (ptr2 - ptr1) < len1)
		return 1;
	else if (ptr2 < ptr1 && (ptr1 - ptr2) < len2)
		return 1;
	return 0;
}

static int
test_memzone_invalid_alignment(void)
{
	const struct rte_memzone * mz;

	mz = rte_memzone_lookup("invalid_alignment");
	if (mz != NULL) {
		printf("Zone with invalid alignment has been reserved\n");
		return -1;
	}

	mz = rte_memzone_reserve_aligned("invalid_alignment", 100,
			SOCKET_ID_ANY, 0, 100);
	if (mz != NULL) {
		printf("Zone with invalid alignment has been reserved\n");
		return -1;
	}
	return 0;
}

static int
test_memzone_reserving_zone_size_bigger_than_the_maximum(void)
{
	const struct rte_memzone * mz;

	mz = rte_memzone_lookup("zone_size_bigger_than_the_maximum");
	if (mz != NULL) {
		printf("zone_size_bigger_than_the_maximum has been reserved\n");
		return -1;
	}

	mz = rte_memzone_reserve("zone_size_bigger_than_the_maximum", (size_t)-1,
			SOCKET_ID_ANY, 0);
	if (mz != NULL) {
		printf("It is impossible to reserve such big a memzone\n");
		return -1;
	}

	return 0;
}

static int
test_memzone_reserve_flags(void)
{
	const struct rte_memzone *mz;
	const struct rte_memseg *ms;
	int hugepage_2MB_avail = 0;
	int hugepage_1GB_avail = 0;
	int hugepage_16MB_avail = 0;
	int hugepage_16GB_avail = 0;
	const size_t size = 100;
	int i = 0;
	ms = rte_eal_get_physmem_layout();
	for (i = 0; i < RTE_MAX_MEMSEG; i++) {
		if (ms[i].hugepage_sz == RTE_PGSIZE_2M)
			hugepage_2MB_avail = 1;
		if (ms[i].hugepage_sz == RTE_PGSIZE_1G)
			hugepage_1GB_avail = 1;
		if (ms[i].hugepage_sz == RTE_PGSIZE_16M)
			hugepage_16MB_avail = 1;
		if (ms[i].hugepage_sz == RTE_PGSIZE_16G)
			hugepage_16GB_avail = 1;
	}
	/* Display the availability of 2MB ,1GB, 16MB, 16GB pages */
	if (hugepage_2MB_avail)
		printf("2MB Huge pages available\n");
	if (hugepage_1GB_avail)
		printf("1GB Huge pages available\n");
	if (hugepage_16MB_avail)
		printf("16MB Huge pages available\n");
	if (hugepage_16GB_avail)
		printf("16GB Huge pages available\n");
	/*
	 * If 2MB pages available, check that a small memzone is correctly
	 * reserved from 2MB huge pages when requested by the RTE_MEMZONE_2MB flag.
	 * Also check that RTE_MEMZONE_SIZE_HINT_ONLY flag only defaults to an
	 * available page size (i.e 1GB ) when 2MB pages are unavailable.
	 */
	if (hugepage_2MB_avail) {
		mz = rte_memzone_reserve("flag_zone_2M", size, SOCKET_ID_ANY,
				RTE_MEMZONE_2MB);
		if (mz == NULL) {
			printf("MEMZONE FLAG 2MB\n");
			return -1;
		}
		if (mz->hugepage_sz != RTE_PGSIZE_2M) {
			printf("hugepage_sz not equal 2M\n");
			return -1;
		}

		mz = rte_memzone_reserve("flag_zone_2M_HINT", size, SOCKET_ID_ANY,
				RTE_MEMZONE_2MB|RTE_MEMZONE_SIZE_HINT_ONLY);
		if (mz == NULL) {
			printf("MEMZONE FLAG 2MB\n");
			return -1;
		}
		if (mz->hugepage_sz != RTE_PGSIZE_2M) {
			printf("hugepage_sz not equal 2M\n");
			return -1;
		}

		/* Check if 1GB huge pages are unavailable, that function fails unless
		 * HINT flag is indicated
		 */
		if (!hugepage_1GB_avail) {
			mz = rte_memzone_reserve("flag_zone_1G_HINT", size, SOCKET_ID_ANY,
					RTE_MEMZONE_1GB|RTE_MEMZONE_SIZE_HINT_ONLY);
			if (mz == NULL) {
				printf("MEMZONE FLAG 1GB & HINT\n");
				return -1;
			}
			if (mz->hugepage_sz != RTE_PGSIZE_2M) {
				printf("hugepage_sz not equal 2M\n");
				return -1;
			}

			mz = rte_memzone_reserve("flag_zone_1G", size, SOCKET_ID_ANY,
					RTE_MEMZONE_1GB);
			if (mz != NULL) {
				printf("MEMZONE FLAG 1GB\n");
				return -1;
			}
		}
	}

	/*As with 2MB tests above for 1GB huge page requests*/
	if (hugepage_1GB_avail) {
		mz = rte_memzone_reserve("flag_zone_1G", size, SOCKET_ID_ANY,
				RTE_MEMZONE_1GB);
		if (mz == NULL) {
			printf("MEMZONE FLAG 1GB\n");
			return -1;
		}
		if (mz->hugepage_sz != RTE_PGSIZE_1G) {
			printf("hugepage_sz not equal 1G\n");
			return -1;
		}

		mz = rte_memzone_reserve("flag_zone_1G_HINT", size, SOCKET_ID_ANY,
				RTE_MEMZONE_1GB|RTE_MEMZONE_SIZE_HINT_ONLY);
		if (mz == NULL) {
			printf("MEMZONE FLAG 1GB\n");
			return -1;
		}
		if (mz->hugepage_sz != RTE_PGSIZE_1G) {
			printf("hugepage_sz not equal 1G\n");
			return -1;
		}

		/* Check if 1GB huge pages are unavailable, that function fails unless
		 * HINT flag is indicated
		 */
		if (!hugepage_2MB_avail) {
			mz = rte_memzone_reserve("flag_zone_2M_HINT", size, SOCKET_ID_ANY,
					RTE_MEMZONE_2MB|RTE_MEMZONE_SIZE_HINT_ONLY);
			if (mz == NULL){
				printf("MEMZONE FLAG 2MB & HINT\n");
				return -1;
			}
			if (mz->hugepage_sz != RTE_PGSIZE_1G) {
				printf("hugepage_sz not equal 1G\n");
				return -1;
			}
			mz = rte_memzone_reserve("flag_zone_2M", size, SOCKET_ID_ANY,
					RTE_MEMZONE_2MB);
			if (mz != NULL) {
				printf("MEMZONE FLAG 2MB\n");
				return -1;
			}
		}

		if (hugepage_2MB_avail && hugepage_1GB_avail) {
			mz = rte_memzone_reserve("flag_zone_2M_HINT", size, SOCKET_ID_ANY,
								RTE_MEMZONE_2MB|RTE_MEMZONE_1GB);
			if (mz != NULL) {
				printf("BOTH SIZES SET\n");
				return -1;
			}
		}
	}
	/*
	 * This option is for IBM Power. If 16MB pages available, check
	 * that a small memzone is correctly reserved from 16MB huge pages
	 * when requested by the RTE_MEMZONE_16MB flag. Also check that
	 * RTE_MEMZONE_SIZE_HINT_ONLY flag only defaults to an available
	 * page size (i.e 16GB ) when 16MB pages are unavailable.
	 */
	if (hugepage_16MB_avail) {
		mz = rte_memzone_reserve("flag_zone_16M", size, SOCKET_ID_ANY,
				RTE_MEMZONE_16MB);
		if (mz == NULL) {
			printf("MEMZONE FLAG 16MB\n");
			return -1;
		}
		if (mz->hugepage_sz != RTE_PGSIZE_16M) {
			printf("hugepage_sz not equal 16M\n");
			return -1;
		}

		mz = rte_memzone_reserve("flag_zone_16M_HINT", size,
		SOCKET_ID_ANY, RTE_MEMZONE_16MB|RTE_MEMZONE_SIZE_HINT_ONLY);
		if (mz == NULL) {
			printf("MEMZONE FLAG 2MB\n");
			return -1;
		}
		if (mz->hugepage_sz != RTE_PGSIZE_16M) {
			printf("hugepage_sz not equal 16M\n");
			return -1;
		}

		/* Check if 1GB huge pages are unavailable, that function fails
		 * unless HINT flag is indicated
		 */
		if (!hugepage_16GB_avail) {
			mz = rte_memzone_reserve("flag_zone_16G_HINT", size,
				SOCKET_ID_ANY,
				RTE_MEMZONE_16GB|RTE_MEMZONE_SIZE_HINT_ONLY);
			if (mz == NULL) {
				printf("MEMZONE FLAG 16GB & HINT\n");
				return -1;
			}
			if (mz->hugepage_sz != RTE_PGSIZE_16M) {
				printf("hugepage_sz not equal 16M\n");
				return -1;
			}

			mz = rte_memzone_reserve("flag_zone_16G", size,
				SOCKET_ID_ANY, RTE_MEMZONE_16GB);
			if (mz != NULL) {
				printf("MEMZONE FLAG 16GB\n");
				return -1;
			}
		}
	}
	/*As with 16MB tests above for 16GB huge page requests*/
	if (hugepage_16GB_avail) {
		mz = rte_memzone_reserve("flag_zone_16G", size, SOCKET_ID_ANY,
				RTE_MEMZONE_16GB);
		if (mz == NULL) {
			printf("MEMZONE FLAG 16GB\n");
			return -1;
		}
		if (mz->hugepage_sz != RTE_PGSIZE_16G) {
			printf("hugepage_sz not equal 16G\n");
			return -1;
		}

		mz = rte_memzone_reserve("flag_zone_16G_HINT", size,
		SOCKET_ID_ANY, RTE_MEMZONE_16GB|RTE_MEMZONE_SIZE_HINT_ONLY);
		if (mz == NULL) {
			printf("MEMZONE FLAG 16GB\n");
			return -1;
		}
		if (mz->hugepage_sz != RTE_PGSIZE_16G) {
			printf("hugepage_sz not equal 16G\n");
			return -1;
		}

		/* Check if 1GB huge pages are unavailable, that function fails
		 * unless HINT flag is indicated
		 */
		if (!hugepage_16MB_avail) {
			mz = rte_memzone_reserve("flag_zone_16M_HINT", size,
				SOCKET_ID_ANY,
				RTE_MEMZONE_16MB|RTE_MEMZONE_SIZE_HINT_ONLY);
			if (mz == NULL) {
				printf("MEMZONE FLAG 16MB & HINT\n");
				return -1;
			}
			if (mz->hugepage_sz != RTE_PGSIZE_16G) {
				printf("hugepage_sz not equal 16G\n");
				return -1;
			}
			mz = rte_memzone_reserve("flag_zone_16M", size,
				SOCKET_ID_ANY, RTE_MEMZONE_16MB);
			if (mz != NULL) {
				printf("MEMZONE FLAG 16MB\n");
				return -1;
			}
		}

		if (hugepage_16MB_avail && hugepage_16GB_avail) {
			mz = rte_memzone_reserve("flag_zone_16M_HINT", size,
				SOCKET_ID_ANY,
				RTE_MEMZONE_16MB|RTE_MEMZONE_16GB);
			if (mz != NULL) {
				printf("BOTH SIZES SET\n");
				return -1;
			}
		}
	}
	return 0;
}

static int
test_memzone_reserve_max(void)
{
	const struct rte_memzone *mz;
	const struct rte_config *config;
	const struct rte_memseg *ms;
	int memseg_idx = 0;
	int memzone_idx = 0;
	size_t len = 0;
	void* last_addr;
	size_t maxlen = 0;

	/* get pointer to global configuration */
	config = rte_eal_get_configuration();

	ms = rte_eal_get_physmem_layout();

	for (memseg_idx = 0; memseg_idx < RTE_MAX_MEMSEG; memseg_idx++){
		/* ignore smaller memsegs as they can only get smaller */
		if (ms[memseg_idx].len < maxlen)
			continue;

		/* align everything */
		last_addr = RTE_PTR_ALIGN_CEIL(ms[memseg_idx].addr, RTE_CACHE_LINE_SIZE);
		len = ms[memseg_idx].len - RTE_PTR_DIFF(last_addr, ms[memseg_idx].addr);
		len &= ~((size_t) RTE_CACHE_LINE_MASK);

		/* cycle through all memzones */
		for (memzone_idx = 0; memzone_idx < RTE_MAX_MEMZONE; memzone_idx++) {

			/* stop when reaching last allocated memzone */
			if (config->mem_config->memzone[memzone_idx].addr == NULL)
				break;

			/* check if the memzone is in our memseg and subtract length */
			if ((config->mem_config->memzone[memzone_idx].addr >=
			     ms[memseg_idx].addr) &&
			    (config->mem_config->memzone[memzone_idx].addr <
			     (RTE_PTR_ADD(ms[memseg_idx].addr, ms[memseg_idx].len)))) {
				/* since the zones can now be aligned and occasionally skip
				 * some space, we should calculate the length based on
				 * reported length and start addresses difference. Addresses
				 * are allocated sequentially so we don't need to worry about
				 * them being in the right order.
				 */
				len -= RTE_PTR_DIFF(
						    config->mem_config->memzone[memzone_idx].addr,
						    last_addr);
				len -= config->mem_config->memzone[memzone_idx].len;
				last_addr = RTE_PTR_ADD(config->mem_config->memzone[memzone_idx].addr,
							(size_t) config->mem_config->memzone[memzone_idx].len);
			}
		}

		/* we don't need to calculate offset here since length
		 * is always cache-aligned */
		if (len > maxlen)
			maxlen = len;
	}

	if (maxlen == 0) {
		printf("There is no space left!\n");
		return 0;
	}

	mz = rte_memzone_reserve("max_zone", 0, SOCKET_ID_ANY, 0);
	if (mz == NULL){
		printf("Failed to reserve a big chunk of memory\n");
		rte_dump_physmem_layout(stdout);
		rte_memzone_dump(stdout);
		return -1;
	}

	if (mz->len != maxlen) {
		printf("Memzone reserve with 0 size did not return bigest block\n");
		printf("Expected size = %zu, actual size = %zu\n",
		       maxlen, mz->len);
		rte_dump_physmem_layout(stdout);
		rte_memzone_dump(stdout);

		return -1;
	}
	return 0;
}

static int
test_memzone_reserve_max_aligned(void)
{
	const struct rte_memzone *mz;
	const struct rte_config *config;
	const struct rte_memseg *ms;
	int memseg_idx = 0;
	int memzone_idx = 0;
	uintptr_t addr_offset;
	size_t len = 0;
	void* last_addr;
	size_t maxlen = 0;

	/* random alignment */
	rte_srand((unsigned)rte_rdtsc());
	const unsigned align = 1 << ((rte_rand() % 8) + 5); /* from 128 up to 4k alignment */

	/* get pointer to global configuration */
	config = rte_eal_get_configuration();

	ms = rte_eal_get_physmem_layout();

	addr_offset = 0;

	for (memseg_idx = 0; memseg_idx < RTE_MAX_MEMSEG; memseg_idx++){

		/* ignore smaller memsegs as they can only get smaller */
		if (ms[memseg_idx].len < maxlen)
			continue;

		/* align everything */
		last_addr = RTE_PTR_ALIGN_CEIL(ms[memseg_idx].addr, RTE_CACHE_LINE_SIZE);
		len = ms[memseg_idx].len - RTE_PTR_DIFF(last_addr, ms[memseg_idx].addr);
		len &= ~((size_t) RTE_CACHE_LINE_MASK);

		/* cycle through all memzones */
		for (memzone_idx = 0; memzone_idx < RTE_MAX_MEMZONE; memzone_idx++) {

			/* stop when reaching last allocated memzone */
			if (config->mem_config->memzone[memzone_idx].addr == NULL)
				break;

			/* check if the memzone is in our memseg and subtract length */
			if ((config->mem_config->memzone[memzone_idx].addr >=
					ms[memseg_idx].addr) &&
					(config->mem_config->memzone[memzone_idx].addr <
					(RTE_PTR_ADD(ms[memseg_idx].addr, ms[memseg_idx].len)))) {
				/* since the zones can now be aligned and occasionally skip
				 * some space, we should calculate the length based on
				 * reported length and start addresses difference.
				 */
				len -= (uintptr_t) RTE_PTR_SUB(
						config->mem_config->memzone[memzone_idx].addr,
						(uintptr_t) last_addr);
				len -= config->mem_config->memzone[memzone_idx].len;
				last_addr =
						RTE_PTR_ADD(config->mem_config->memzone[memzone_idx].addr,
						(size_t) config->mem_config->memzone[memzone_idx].len);
			}
		}

		/* make sure we get the alignment offset */
		if (len > maxlen) {
			addr_offset = RTE_PTR_ALIGN_CEIL((uintptr_t) last_addr, align) - (uintptr_t) last_addr;
			maxlen = len;
		}
	}

	if (maxlen == 0 || maxlen == addr_offset) {
		printf("There is no space left for biggest %u-aligned memzone!\n", align);
		return 0;
	}

	maxlen -= addr_offset;

	mz = rte_memzone_reserve_aligned("max_zone_aligned", 0,
			SOCKET_ID_ANY, 0, align);
	if (mz == NULL){
		printf("Failed to reserve a big chunk of memory\n");
		rte_dump_physmem_layout(stdout);
		rte_memzone_dump(stdout);
		return -1;
	}

	if (mz->len != maxlen) {
		printf("Memzone reserve with 0 size and alignment %u did not return"
				" bigest block\n", align);
		printf("Expected size = %zu, actual size = %zu\n",
				maxlen, mz->len);
		rte_dump_physmem_layout(stdout);
		rte_memzone_dump(stdout);

		return -1;
	}
	return 0;
}

static int
test_memzone_aligned(void)
{
	const struct rte_memzone *memzone_aligned_32;
	const struct rte_memzone *memzone_aligned_128;
	const struct rte_memzone *memzone_aligned_256;
	const struct rte_memzone *memzone_aligned_512;
	const struct rte_memzone *memzone_aligned_1024;

	/* memzone that should automatically be adjusted to align on 64 bytes */
	memzone_aligned_32 = rte_memzone_reserve_aligned("aligned_32", 100,
				SOCKET_ID_ANY, 0, 32);

	/* memzone that is supposed to be aligned on a 128 byte boundary */
	memzone_aligned_128 = rte_memzone_reserve_aligned("aligned_128", 100,
				SOCKET_ID_ANY, 0, 128);

	/* memzone that is supposed to be aligned on a 256 byte boundary */
	memzone_aligned_256 = rte_memzone_reserve_aligned("aligned_256", 100,
				SOCKET_ID_ANY, 0, 256);

	/* memzone that is supposed to be aligned on a 512 byte boundary */
	memzone_aligned_512 = rte_memzone_reserve_aligned("aligned_512", 100,
				SOCKET_ID_ANY, 0, 512);

	/* memzone that is supposed to be aligned on a 1024 byte boundary */
	memzone_aligned_1024 = rte_memzone_reserve_aligned("aligned_1024", 100,
				SOCKET_ID_ANY, 0, 1024);

	printf("check alignments and lengths\n");
	if (memzone_aligned_32 == NULL) {
		printf("Unable to reserve 64-byte aligned memzone!\n");
		return -1;
	}
	if ((memzone_aligned_32->phys_addr & RTE_CACHE_LINE_MASK) != 0)
		return -1;
	if (((uintptr_t) memzone_aligned_32->addr & RTE_CACHE_LINE_MASK) != 0)
		return -1;
	if ((memzone_aligned_32->len & RTE_CACHE_LINE_MASK) != 0)
		return -1;

	if (memzone_aligned_128 == NULL) {
		printf("Unable to reserve 128-byte aligned memzone!\n");
		return -1;
	}
	if ((memzone_aligned_128->phys_addr & 127) != 0)
		return -1;
	if (((uintptr_t) memzone_aligned_128->addr & 127) != 0)
		return -1;
	if ((memzone_aligned_128->len & RTE_CACHE_LINE_MASK) != 0)
		return -1;

	if (memzone_aligned_256 == NULL) {
		printf("Unable to reserve 256-byte aligned memzone!\n");
		return -1;
	}
	if ((memzone_aligned_256->phys_addr & 255) != 0)
		return -1;
	if (((uintptr_t) memzone_aligned_256->addr & 255) != 0)
		return -1;
	if ((memzone_aligned_256->len & RTE_CACHE_LINE_MASK) != 0)
		return -1;

	if (memzone_aligned_512 == NULL) {
		printf("Unable to reserve 512-byte aligned memzone!\n");
		return -1;
	}
	if ((memzone_aligned_512->phys_addr & 511) != 0)
		return -1;
	if (((uintptr_t) memzone_aligned_512->addr & 511) != 0)
		return -1;
	if ((memzone_aligned_512->len & RTE_CACHE_LINE_MASK) != 0)
		return -1;

	if (memzone_aligned_1024 == NULL) {
		printf("Unable to reserve 1024-byte aligned memzone!\n");
		return -1;
	}
	if ((memzone_aligned_1024->phys_addr & 1023) != 0)
		return -1;
	if (((uintptr_t) memzone_aligned_1024->addr & 1023) != 0)
		return -1;
	if ((memzone_aligned_1024->len & RTE_CACHE_LINE_MASK) != 0)
		return -1;

	/* check that zones don't overlap */
	printf("check overlapping\n");
	if (is_memory_overlap(memzone_aligned_32->phys_addr, memzone_aligned_32->len,
					memzone_aligned_128->phys_addr, memzone_aligned_128->len))
		return -1;
	if (is_memory_overlap(memzone_aligned_32->phys_addr, memzone_aligned_32->len,
					memzone_aligned_256->phys_addr, memzone_aligned_256->len))
		return -1;
	if (is_memory_overlap(memzone_aligned_32->phys_addr, memzone_aligned_32->len,
					memzone_aligned_512->phys_addr, memzone_aligned_512->len))
		return -1;
	if (is_memory_overlap(memzone_aligned_32->phys_addr, memzone_aligned_32->len,
					memzone_aligned_1024->phys_addr, memzone_aligned_1024->len))
		return -1;
	if (is_memory_overlap(memzone_aligned_128->phys_addr, memzone_aligned_128->len,
					memzone_aligned_256->phys_addr, memzone_aligned_256->len))
		return -1;
	if (is_memory_overlap(memzone_aligned_128->phys_addr, memzone_aligned_128->len,
					memzone_aligned_512->phys_addr, memzone_aligned_512->len))
		return -1;
	if (is_memory_overlap(memzone_aligned_128->phys_addr, memzone_aligned_128->len,
					memzone_aligned_1024->phys_addr, memzone_aligned_1024->len))
		return -1;
	if (is_memory_overlap(memzone_aligned_256->phys_addr, memzone_aligned_256->len,
					memzone_aligned_512->phys_addr, memzone_aligned_512->len))
		return -1;
	if (is_memory_overlap(memzone_aligned_256->phys_addr, memzone_aligned_256->len,
					memzone_aligned_1024->phys_addr, memzone_aligned_1024->len))
		return -1;
	if (is_memory_overlap(memzone_aligned_512->phys_addr, memzone_aligned_512->len,
					memzone_aligned_1024->phys_addr, memzone_aligned_1024->len))
		return -1;
	return 0;
}

static int
check_memzone_bounded(const char *name, uint32_t len,  uint32_t align,
	uint32_t bound)
{
	const struct rte_memzone *mz;
	phys_addr_t bmask;

	bmask = ~((phys_addr_t)bound - 1);

	if ((mz = rte_memzone_reserve_bounded(name, len, SOCKET_ID_ANY, 0,
			align, bound)) == NULL) {
		printf("%s(%s): memzone creation failed\n",
			__func__, name);
		return (-1);
	}

	if ((mz->phys_addr & ((phys_addr_t)align - 1)) != 0) {
		printf("%s(%s): invalid phys addr alignment\n",
			__func__, mz->name);
		return (-1);
	}

	if (((uintptr_t) mz->addr & ((uintptr_t)align - 1)) != 0) {
		printf("%s(%s): invalid virtual addr alignment\n",
			__func__, mz->name);
		return (-1);
	}

	if ((mz->len & RTE_CACHE_LINE_MASK) != 0 || mz->len < len ||
			mz->len < RTE_CACHE_LINE_SIZE) {
		printf("%s(%s): invalid length\n",
			__func__, mz->name);
		return (-1);
	}

	if ((mz->phys_addr & bmask) !=
			((mz->phys_addr + mz->len - 1) & bmask)) {
		printf("%s(%s): invalid memzone boundary %u crossed\n",
			__func__, mz->name, bound);
		return (-1);
	}

	return (0);
}

static int
test_memzone_bounded(void)
{
	const struct rte_memzone *memzone_err;
	const char *name;
	int rc;

	/* should fail as boundary is not power of two */
	name = "bounded_error_31";
	if ((memzone_err = rte_memzone_reserve_bounded(name,
			100, SOCKET_ID_ANY, 0, 32, UINT32_MAX)) != NULL) {
		printf("%s(%s)created a memzone with invalid boundary "
			"conditions\n", __func__, memzone_err->name);
		return (-1);
	}

	/* should fail as len is greater then boundary */
	name = "bounded_error_32";
	if ((memzone_err = rte_memzone_reserve_bounded(name,
			100, SOCKET_ID_ANY, 0, 32, 32)) != NULL) {
		printf("%s(%s)created a memzone with invalid boundary "
			"conditions\n", __func__, memzone_err->name);
		return (-1);
	}

	if ((rc = check_memzone_bounded("bounded_128", 100, 128, 128)) != 0)
		return (rc);

	if ((rc = check_memzone_bounded("bounded_256", 100, 256, 128)) != 0)
		return (rc);

	if ((rc = check_memzone_bounded("bounded_1K", 100, 64, 1024)) != 0)
		return (rc);

	if ((rc = check_memzone_bounded("bounded_1K_MAX", 0, 64, 1024)) != 0)
		return (rc);

	return (0);
}

static int
test_memzone_reserve_memory_in_smallest_segment(void)
{
	const struct rte_memzone *mz;
	const struct rte_memseg *ms, *min_ms, *prev_min_ms;
	size_t min_len, prev_min_len;
	const struct rte_config *config;
	int i;

	config = rte_eal_get_configuration();

	min_ms = NULL;  /*< smallest segment */
	prev_min_ms = NULL; /*< second smallest segment */

	/* find two smallest segments */
	for (i = 0; i < RTE_MAX_MEMSEG; i++) {
		ms = &config->mem_config->free_memseg[i];

		if (ms->addr == NULL)
			break;
		if (ms->len == 0)
			continue;

		if (min_ms == NULL)
			min_ms = ms;
		else if (min_ms->len > ms->len) {
			/* set last smallest to second last */
			prev_min_ms = min_ms;

			/* set new smallest */
			min_ms = ms;
		} else if ((prev_min_ms == NULL)
			|| (prev_min_ms->len > ms->len))
			prev_min_ms = ms;
	}

	if (min_ms == NULL || prev_min_ms == NULL) {
		printf("Smallest segments not found!\n");
		return -1;
	}

	min_len = min_ms->len;
	prev_min_len = prev_min_ms->len;

	/* try reserving a memzone in the smallest memseg */
	mz = rte_memzone_reserve("smallest_mz", RTE_CACHE_LINE_SIZE,
			SOCKET_ID_ANY, 0);
	if (mz == NULL) {
		printf("Failed to reserve memory from smallest memseg!\n");
		return -1;
	}
	if (prev_min_ms->len != prev_min_len &&
			min_ms->len != min_len - RTE_CACHE_LINE_SIZE) {
		printf("Reserved memory from wrong memseg!\n");
		return -1;
	}

	return 0;
}

/* this test is a bit  tricky, and thus warrants explanation.
 *
 * first, we find two smallest memsegs to conduct our experiments on.
 *
 * then, we bring them within alignment from each other: if second segment is
 * twice+ as big as the first, reserve memory from that segment; if second
 * segment is comparable in length to the first, then cut the first segment
 * down until it becomes less than half of second segment, and then cut down
 * the second segment to be within alignment of the first.
 *
 * then, we have to pass the following test: if segments are within alignment
 * of each other (that is, the difference is less than 256 bytes, which is what
 * our alignment will be), segment with smallest offset should be picked.
 *
 * we know that min_ms will be our smallest segment, so we need to make sure
 * that we adjust the alignments so that the bigger segment has smallest
 * alignment (in our case, smallest segment will have 64-byte alignment, while
 * bigger segment will have 128-byte alignment).
 */
static int
test_memzone_reserve_memory_with_smallest_offset(void)
{
	const struct rte_memseg *ms, *min_ms, *prev_min_ms;
	size_t len, min_len, prev_min_len;
	const struct rte_config *config;
	int i, align;

	config = rte_eal_get_configuration();

	min_ms = NULL;  /*< smallest segment */
	prev_min_ms = NULL; /*< second smallest segment */
	align = RTE_CACHE_LINE_SIZE * 4;

	/* find two smallest segments */
	for (i = 0; i < RTE_MAX_MEMSEG; i++) {
		ms = &config->mem_config->free_memseg[i];

		if (ms->addr == NULL)
			break;
		if (ms->len == 0)
			continue;

		if (min_ms == NULL)
			min_ms = ms;
		else if (min_ms->len > ms->len) {
			/* set last smallest to second last */
			prev_min_ms = min_ms;

			/* set new smallest */
			min_ms = ms;
		} else if ((prev_min_ms == NULL)
			|| (prev_min_ms->len > ms->len)) {
			prev_min_ms = ms;
		}
	}

	if (min_ms == NULL || prev_min_ms == NULL) {
		printf("Smallest segments not found!\n");
		return -1;
	}

	prev_min_len = prev_min_ms->len;
	min_len = min_ms->len;

	/* if smallest segment is bigger than half of bigger segment */
	if (prev_min_ms->len - min_ms->len <= min_ms->len) {

		len = (min_ms->len * 2) - prev_min_ms->len;

		/* make sure final length is *not* aligned */
		while (((min_ms->addr_64 + len) & (align-1)) == 0)
			len += RTE_CACHE_LINE_SIZE;

		if (rte_memzone_reserve("dummy_mz1", len, SOCKET_ID_ANY, 0) == NULL) {
			printf("Cannot reserve memory!\n");
			return -1;
		}

		/* check if we got memory from correct segment */
		if (min_ms->len != min_len - len) {
			printf("Reserved memory from wrong segment!\n");
			return -1;
		}
	}
    /* if we don't need to touch smallest segment but it's aligned */
    else if ((min_ms->addr_64 & (align-1)) == 0) {
            if (rte_memzone_reserve("align_mz1", RTE_CACHE_LINE_SIZE,
                    SOCKET_ID_ANY, 0) == NULL) {
                            printf("Cannot reserve memory!\n");
                            return -1;
            }
            if (min_ms->len != min_len - RTE_CACHE_LINE_SIZE) {
                    printf("Reserved memory from wrong segment!\n");
                    return -1;
            }
    }

	/* if smallest segment is less than half of bigger segment */
	if (prev_min_ms->len - min_ms->len > min_ms->len) {
		len = prev_min_ms->len - min_ms->len - align;

		/* make sure final length is aligned */
		while (((prev_min_ms->addr_64 + len) & (align-1)) != 0)
			len += RTE_CACHE_LINE_SIZE;

		if (rte_memzone_reserve("dummy_mz2", len, SOCKET_ID_ANY, 0) == NULL) {
			printf("Cannot reserve memory!\n");
			return -1;
		}

		/* check if we got memory from correct segment */
		if (prev_min_ms->len != prev_min_len - len) {
			printf("Reserved memory from wrong segment!\n");
			return -1;
		}
	}
	len = RTE_CACHE_LINE_SIZE;



	prev_min_len = prev_min_ms->len;
	min_len = min_ms->len;

	if (min_len >= prev_min_len || prev_min_len - min_len > (unsigned) align) {
		printf("Segments are of wrong lengths!\n");
		return -1;
	}

	/* try reserving from a bigger segment */
	if (rte_memzone_reserve_aligned("smallest_offset", len, SOCKET_ID_ANY, 0, align) ==
			NULL) {
		printf("Cannot reserve memory!\n");
		return -1;
	}

	/* check if we got memory from correct segment */
	if (min_ms->len != min_len && prev_min_ms->len != (prev_min_len - len)) {
		printf("Reserved memory from segment with smaller offset!\n");
		return -1;
	}

	return 0;
}

static int
test_memzone_reserve_remainder(void)
{
	const struct rte_memzone *mz1, *mz2;
	const struct rte_memseg *ms, *min_ms = NULL;
	size_t min_len;
	const struct rte_config *config;
	int i, align;

	min_len = 0;
	align = RTE_CACHE_LINE_SIZE;

	config = rte_eal_get_configuration();

	/* find minimum free contiguous length */
	for (i = 0; i < RTE_MAX_MEMSEG; i++) {
		ms = &config->mem_config->free_memseg[i];

		if (ms->addr == NULL)
			break;
		if (ms->len == 0)
			continue;

		if (min_len == 0 || ms->len < min_len) {
			min_len = ms->len;
			min_ms = ms;

			/* find maximum alignment this segment is able to hold */
			align = RTE_CACHE_LINE_SIZE;
			while ((ms->addr_64 & (align-1)) == 0) {
				align <<= 1;
			}
		}
	}

	if (min_ms == NULL) {
		printf("Minimal sized segment not found!\n");
		return -1;
	}

	/* try reserving min_len bytes with alignment - this should not affect our
	 * memseg, the memory will be taken from a different one.
	 */
	mz1 = rte_memzone_reserve_aligned("reserve_remainder_1", min_len,
			SOCKET_ID_ANY, 0, align);
	if (mz1 == NULL) {
		printf("Failed to reserve %zu bytes aligned on %i bytes\n", min_len,
				align);
		return -1;
	}
	if (min_ms->len != min_len) {
		printf("Memseg memory should not have been reserved!\n");
		return -1;
	}

	/* try reserving min_len bytes with less alignment - this should fill up
	 * the segment.
	 */
	mz2 = rte_memzone_reserve("reserve_remainder_2", min_len,
			SOCKET_ID_ANY, 0);
	if (mz2 == NULL) {
		printf("Failed to reserve %zu bytes\n", min_len);
		return -1;
	}
	if (min_ms->len != 0) {
		printf("Memseg memory should have been reserved!\n");
		return -1;
	}

	return 0;
}

static int
test_memzone(void)
{
	const struct rte_memzone *memzone1;
	const struct rte_memzone *memzone2;
	const struct rte_memzone *memzone3;
	const struct rte_memzone *memzone4;
	const struct rte_memzone *mz;

	memzone1 = rte_memzone_reserve("testzone1", 100,
				SOCKET_ID_ANY, 0);

	memzone2 = rte_memzone_reserve("testzone2", 1000,
				0, 0);

	memzone3 = rte_memzone_reserve("testzone3", 1000,
				1, 0);

	memzone4 = rte_memzone_reserve("testzone4", 1024,
				SOCKET_ID_ANY, 0);

	/* memzone3 may be NULL if we don't have NUMA */
	if (memzone1 == NULL || memzone2 == NULL || memzone4 == NULL)
		return -1;

	rte_memzone_dump(stdout);

	/* check cache-line alignments */
	printf("check alignments and lengths\n");

	if ((memzone1->phys_addr & RTE_CACHE_LINE_MASK) != 0)
		return -1;
	if ((memzone2->phys_addr & RTE_CACHE_LINE_MASK) != 0)
		return -1;
	if (memzone3 != NULL && (memzone3->phys_addr & RTE_CACHE_LINE_MASK) != 0)
		return -1;
	if ((memzone1->len & RTE_CACHE_LINE_MASK) != 0 || memzone1->len == 0)
		return -1;
	if ((memzone2->len & RTE_CACHE_LINE_MASK) != 0 || memzone2->len == 0)
		return -1;
	if (memzone3 != NULL && ((memzone3->len & RTE_CACHE_LINE_MASK) != 0 ||
			memzone3->len == 0))
		return -1;
	if (memzone4->len != 1024)
		return -1;

	/* check that zones don't overlap */
	printf("check overlapping\n");

	if (is_memory_overlap(memzone1->phys_addr, memzone1->len,
			memzone2->phys_addr, memzone2->len))
		return -1;
	if (memzone3 != NULL &&
			is_memory_overlap(memzone1->phys_addr, memzone1->len,
					memzone3->phys_addr, memzone3->len))
		return -1;
	if (memzone3 != NULL &&
			is_memory_overlap(memzone2->phys_addr, memzone2->len,
					memzone3->phys_addr, memzone3->len))
		return -1;

	printf("check socket ID\n");

	/* memzone2 must be on socket id 0 and memzone3 on socket 1 */
	if (memzone2->socket_id != 0)
		return -1;
	if (memzone3 != NULL && memzone3->socket_id != 1)
		return -1;

	printf("test zone lookup\n");
	mz = rte_memzone_lookup("testzone1");
	if (mz != memzone1)
		return -1;

	printf("test duplcate zone name\n");
	mz = rte_memzone_reserve("testzone1", 100,
			SOCKET_ID_ANY, 0);
	if (mz != NULL)
		return -1;

	printf("test reserving memzone with bigger size than the maximum\n");
	if (test_memzone_reserving_zone_size_bigger_than_the_maximum() < 0)
		return -1;

	printf("test reserving memory in smallest segments\n");
	if (test_memzone_reserve_memory_in_smallest_segment() < 0)
		return -1;

	printf("test reserving memory in segments with smallest offsets\n");
	if (test_memzone_reserve_memory_with_smallest_offset() < 0)
		return -1;

	printf("test memzone_reserve flags\n");
	if (test_memzone_reserve_flags() < 0)
		return -1;

	printf("test alignment for memzone_reserve\n");
	if (test_memzone_aligned() < 0)
		return -1;

	printf("test boundary alignment for memzone_reserve\n");
	if (test_memzone_bounded() < 0)
		return -1;

	printf("test invalid alignment for memzone_reserve\n");
	if (test_memzone_invalid_alignment() < 0)
		return -1;

	printf("test reserving amounts of memory equal to segment's length\n");
	if (test_memzone_reserve_remainder() < 0)
		return -1;

	printf("test reserving the largest size memzone possible\n");
	if (test_memzone_reserve_max() < 0)
		return -1;

	printf("test reserving the largest size aligned memzone possible\n");
	if (test_memzone_reserve_max_aligned() < 0)
		return -1;

	return 0;
}

static struct test_command memzone_cmd = {
	.command = "memzone_autotest",
	.callback = test_memzone,
};
REGISTER_TEST_COMMAND(memzone_cmd);
