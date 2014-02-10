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

#include <cmdline_parse.h>

#include <rte_random.h>
#include <rte_cycles.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_eal_memconfig.h>
#include <rte_common.h>

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
	const int size = 100;
	int i = 0;
	ms = rte_eal_get_physmem_layout();
	for (i = 0; i < RTE_MAX_MEMSEG; i++) {
		if (ms[i].hugepage_sz == RTE_PGSIZE_2M)
			hugepage_2MB_avail = 1;
		if (ms[i].hugepage_sz == RTE_PGSIZE_1G)
			hugepage_1GB_avail = 1;
	}
	/* Display the availability of 2MB and 1GB pages */
	if (hugepage_2MB_avail)
		printf("2MB Huge pages available\n");
	if (hugepage_1GB_avail)
		printf("1GB Huge pages available\n");
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
		last_addr = RTE_PTR_ALIGN_CEIL(ms[memseg_idx].addr, CACHE_LINE_SIZE);
		len = ms[memseg_idx].len - RTE_PTR_DIFF(last_addr, ms[memseg_idx].addr);
		len &= ~((size_t) CACHE_LINE_MASK);

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
		rte_dump_physmem_layout();
		rte_memzone_dump();
		return -1;
	}

	if (mz->len != maxlen) {
		printf("Memzone reserve with 0 size did not return bigest block\n");
		printf("Expected size = %zu, actual size = %zu\n",
				maxlen, mz->len);
		rte_dump_physmem_layout();
		rte_memzone_dump();

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
		last_addr = RTE_PTR_ALIGN_CEIL(ms[memseg_idx].addr, CACHE_LINE_SIZE);
		len = ms[memseg_idx].len - RTE_PTR_DIFF(last_addr, ms[memseg_idx].addr);
		len &= ~((size_t) CACHE_LINE_MASK);

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
		rte_dump_physmem_layout();
		rte_memzone_dump();
		return -1;
	}

	if (mz->len != maxlen) {
		printf("Memzone reserve with 0 size and alignment %u did not return"
				" bigest block\n", align);
		printf("Expected size = %zu, actual size = %zu\n",
				maxlen, mz->len);
		rte_dump_physmem_layout();
		rte_memzone_dump();

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
	memzone_aligned_32 = rte_memzone_lookup("aligned_32");
	if (memzone_aligned_32 == NULL)
		memzone_aligned_32 = rte_memzone_reserve_aligned("aligned_32", 100,
				SOCKET_ID_ANY, 0, 32);

	/* memzone that is supposed to be aligned on a 128 byte boundary */
	memzone_aligned_128 = rte_memzone_lookup("aligned_128");
	if (memzone_aligned_128 == NULL)
		memzone_aligned_128 = rte_memzone_reserve_aligned("aligned_128", 100,
				SOCKET_ID_ANY, 0, 128);

	/* memzone that is supposed to be aligned on a 256 byte boundary */
	memzone_aligned_256 = rte_memzone_lookup("aligned_256");
	if (memzone_aligned_256 == NULL)
		memzone_aligned_256 = rte_memzone_reserve_aligned("aligned_256", 100,
				SOCKET_ID_ANY, 0, 256);

	/* memzone that is supposed to be aligned on a 512 byte boundary */
	memzone_aligned_512 = rte_memzone_lookup("aligned_512");
	if (memzone_aligned_512 == NULL)
		memzone_aligned_512 = rte_memzone_reserve_aligned("aligned_512", 100,
				SOCKET_ID_ANY, 0, 512);

	/* memzone that is supposed to be aligned on a 1024 byte boundary */
	memzone_aligned_1024 = rte_memzone_lookup("aligned_1024");
	if (memzone_aligned_1024 == NULL)
		memzone_aligned_1024 = rte_memzone_reserve_aligned("aligned_1024", 100,
				SOCKET_ID_ANY, 0, 1024);

	printf("check alignments and lengths\n");
	if (memzone_aligned_32 == NULL) {
		printf("Unable to reserve 64-byte aligned memzone!\n");
		return -1;
	}
	if ((memzone_aligned_32->phys_addr & CACHE_LINE_MASK) != 0)
		return -1;
	if (((uintptr_t) memzone_aligned_32->addr & CACHE_LINE_MASK) != 0)
		return -1;
	if ((memzone_aligned_32->len & CACHE_LINE_MASK) != 0)
		return -1;
	if (memzone_aligned_128 == NULL) {
		printf("Unable to reserve 128-byte aligned memzone!\n");
		return -1;
	}
	if ((memzone_aligned_128->phys_addr & 127) != 0)
		return -1;
	if (((uintptr_t) memzone_aligned_128->addr & 127) != 0)
		return -1;
	if ((memzone_aligned_128->len & CACHE_LINE_MASK) != 0)
		return -1;
	if (memzone_aligned_256 == NULL) {
		printf("Unable to reserve 256-byte aligned memzone!\n");
		return -1;
	}
	if ((memzone_aligned_256->phys_addr & 255) != 0)
		return -1;
	if (((uintptr_t) memzone_aligned_256->addr & 255) != 0)
		return -1;
	if ((memzone_aligned_256->len & CACHE_LINE_MASK) != 0)
		return -1;
	if (memzone_aligned_512 == NULL) {
		printf("Unable to reserve 512-byte aligned memzone!\n");
		return -1;
	}
	if ((memzone_aligned_512->phys_addr & 511) != 0)
		return -1;
	if (((uintptr_t) memzone_aligned_512->addr & 511) != 0)
		return -1;
	if ((memzone_aligned_512->len & CACHE_LINE_MASK) != 0)
		return -1;
	if (memzone_aligned_1024 == NULL) {
		printf("Unable to reserve 1024-byte aligned memzone!\n");
		return -1;
	}
	if ((memzone_aligned_1024->phys_addr & 1023) != 0)
		return -1;
	if (((uintptr_t) memzone_aligned_1024->addr & 1023) != 0)
		return -1;
	if ((memzone_aligned_1024->len & CACHE_LINE_MASK) != 0)
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

int
test_memzone(void)
{
	const struct rte_memzone *memzone1;
	const struct rte_memzone *memzone2;
	const struct rte_memzone *memzone3;
	const struct rte_memzone *mz;

	memzone1 = rte_memzone_lookup("testzone1");
	if (memzone1 == NULL)
		memzone1 = rte_memzone_reserve("testzone1", 100,
				SOCKET_ID_ANY, 0);

	memzone2 = rte_memzone_lookup("testzone2");
	if (memzone2 == NULL)
		memzone2 = rte_memzone_reserve("testzone2", 1000,
				0, 0);

	memzone3 = rte_memzone_lookup("testzone3");
	if (memzone3 == NULL)
		memzone3 = rte_memzone_reserve("testzone3", 1000,
				1, 0);

	/* memzone3 may be NULL if we don't have NUMA */
	if (memzone1 == NULL || memzone2 == NULL)
		return -1;

	rte_memzone_dump();

	/* check cache-line alignments */
	printf("check alignments and lengths\n");

	if ((memzone1->phys_addr & CACHE_LINE_MASK) != 0)
		return -1;
	if ((memzone2->phys_addr & CACHE_LINE_MASK) != 0)
		return -1;
	if (memzone3 != NULL && (memzone3->phys_addr & CACHE_LINE_MASK) != 0)
		return -1;
	if ((memzone1->len & CACHE_LINE_MASK) != 0 || memzone1->len == 0)
		return -1;
	if ((memzone2->len & CACHE_LINE_MASK) != 0 || memzone2->len == 0)
		return -1;
	if (memzone3 != NULL && ((memzone3->len & CACHE_LINE_MASK) != 0 ||
			memzone3->len == 0))
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

	printf("test memzone_reserve flags\n");
	if (test_memzone_reserve_flags() < 0)
		return -1;

	printf("test alignment for memzone_reserve\n");
	if (test_memzone_aligned() < 0)
		return -1;

	printf("test invalid alignment for memzone_reserve\n");
	if (test_memzone_invalid_alignment() < 0)
		return -1;

	printf("test reserving the largest size memzone possible\n");
	if (test_memzone_reserve_max() < 0)
		return -1;

	printf("test reserving the largest size aligned memzone possible\n");
	if (test_memzone_reserve_max_aligned() < 0)
		return -1;

	return 0;
}
