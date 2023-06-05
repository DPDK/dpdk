/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <sys/queue.h>

#include <rte_random.h>
#include <rte_cycles.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_common.h>
#include <rte_string_fns.h>
#include <rte_errno.h>
#include <rte_malloc.h>

#include "malloc_elem.h"

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

#define TEST_MEMZONE_NAME(suffix) "MZ_TEST_" suffix

/* Test if memory overlaps: return 1 if true, or 0 if false. */
static int
is_memory_overlap(rte_iova_t ptr1, size_t len1, rte_iova_t ptr2, size_t len2)
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

	mz = rte_memzone_lookup(TEST_MEMZONE_NAME("invalid_alignment"));
	if (mz != NULL) {
		printf("Zone with invalid alignment has been reserved\n");
		return -1;
	}

	mz = rte_memzone_reserve_aligned(TEST_MEMZONE_NAME("invalid_alignment"),
					 100, SOCKET_ID_ANY, 0, 100);
	if (mz != NULL) {
		printf("Zone with invalid alignment has been reserved\n");
		return -1;
	}
	return 0;
}

static int
test_memzone_invalid_flags(void)
{
	const struct rte_memzone *mz;

	mz = rte_memzone_lookup(TEST_MEMZONE_NAME("invalid_flags"));
	if (mz != NULL) {
		printf("Zone with invalid flags has been reserved\n");
		return -1;
	}

	mz = rte_memzone_reserve(TEST_MEMZONE_NAME("invalid_flags"),
		100, SOCKET_ID_ANY, RTE_MEMZONE_IOVA_CONTIG << 1);
	if (mz != NULL) {
		printf("Zone with invalid flags has been reserved\n");
		return -1;
	}
	return 0;
}

static int
test_memzone_reserving_zone_size_bigger_than_the_maximum(void)
{
	const struct rte_memzone * mz;

	mz = rte_memzone_lookup(
			TEST_MEMZONE_NAME("zone_size_bigger_than_the_maximum"));
	if (mz != NULL) {
		printf("zone_size_bigger_than_the_maximum has been reserved\n");
		return -1;
	}

	mz = rte_memzone_reserve(
			TEST_MEMZONE_NAME("zone_size_bigger_than_the_maximum"),
			(size_t)-1, SOCKET_ID_ANY, 0);
	if (mz != NULL) {
		printf("It is impossible to reserve such big a memzone\n");
		return -1;
	}

	return 0;
}

struct walk_arg {
	int hugepage_2MB_avail;
	int hugepage_1GB_avail;
	int hugepage_16MB_avail;
	int hugepage_16GB_avail;
};
static int
find_available_pagesz(const struct rte_memseg_list *msl, void *arg)
{
	struct walk_arg *wa = arg;

	if (msl->external)
		return 0;

	if (msl->page_sz == RTE_PGSIZE_2M)
		wa->hugepage_2MB_avail = 1;
	if (msl->page_sz == RTE_PGSIZE_1G)
		wa->hugepage_1GB_avail = 1;
	if (msl->page_sz == RTE_PGSIZE_16M)
		wa->hugepage_16MB_avail = 1;
	if (msl->page_sz == RTE_PGSIZE_16G)
		wa->hugepage_16GB_avail = 1;

	return 0;
}

static int
test_memzone_reserve_flags(void)
{
	const struct rte_memzone *mz;
	struct walk_arg wa;
	int hugepage_2MB_avail, hugepage_1GB_avail;
	int hugepage_16MB_avail, hugepage_16GB_avail;
	const size_t size = 100;

	memset(&wa, 0, sizeof(wa));

	rte_memseg_list_walk(find_available_pagesz, &wa);

	hugepage_2MB_avail = wa.hugepage_2MB_avail;
	hugepage_1GB_avail = wa.hugepage_1GB_avail;
	hugepage_16MB_avail = wa.hugepage_16MB_avail;
	hugepage_16GB_avail = wa.hugepage_16GB_avail;

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
		mz = rte_memzone_reserve(TEST_MEMZONE_NAME("flag_zone_2M"),
				size, SOCKET_ID_ANY, RTE_MEMZONE_2MB);
		if (mz == NULL) {
			printf("MEMZONE FLAG 2MB\n");
			return -1;
		}
		if (mz->hugepage_sz != RTE_PGSIZE_2M) {
			printf("hugepage_sz not equal 2M\n");
			return -1;
		}
		if (rte_memzone_free(mz)) {
			printf("Fail memzone free\n");
			return -1;
		}

		mz = rte_memzone_reserve(TEST_MEMZONE_NAME("flag_zone_2M_HINT"),
				size, SOCKET_ID_ANY,
				RTE_MEMZONE_2MB|RTE_MEMZONE_SIZE_HINT_ONLY);
		if (mz == NULL) {
			printf("MEMZONE FLAG 2MB\n");
			return -1;
		}
		if (mz->hugepage_sz != RTE_PGSIZE_2M) {
			printf("hugepage_sz not equal 2M\n");
			return -1;
		}
		if (rte_memzone_free(mz)) {
			printf("Fail memzone free\n");
			return -1;
		}

		/* Check if 1GB huge pages are unavailable, that function fails unless
		 * HINT flag is indicated
		 */
		if (!hugepage_1GB_avail) {
			mz = rte_memzone_reserve(
					TEST_MEMZONE_NAME("flag_zone_1G_HINT"),
					size, SOCKET_ID_ANY,
					RTE_MEMZONE_1GB|RTE_MEMZONE_SIZE_HINT_ONLY);
			if (mz == NULL) {
				printf("MEMZONE FLAG 1GB & HINT\n");
				return -1;
			}
			if (mz->hugepage_sz != RTE_PGSIZE_2M) {
				printf("hugepage_sz not equal 2M\n");
				return -1;
			}
			if (rte_memzone_free(mz)) {
				printf("Fail memzone free\n");
				return -1;
			}

			mz = rte_memzone_reserve(
					TEST_MEMZONE_NAME("flag_zone_1G"), size,
					SOCKET_ID_ANY, RTE_MEMZONE_1GB);
			if (mz != NULL) {
				printf("MEMZONE FLAG 1GB\n");
				return -1;
			}
		}
	}

	/*As with 2MB tests above for 1GB huge page requests*/
	if (hugepage_1GB_avail) {
		mz = rte_memzone_reserve(TEST_MEMZONE_NAME("flag_zone_1G"),
				size, SOCKET_ID_ANY, RTE_MEMZONE_1GB);
		if (mz == NULL) {
			printf("MEMZONE FLAG 1GB\n");
			return -1;
		}
		if (mz->hugepage_sz != RTE_PGSIZE_1G) {
			printf("hugepage_sz not equal 1G\n");
			return -1;
		}
		if (rte_memzone_free(mz)) {
			printf("Fail memzone free\n");
			return -1;
		}

		mz = rte_memzone_reserve(TEST_MEMZONE_NAME("flag_zone_1G_HINT"),
				size, SOCKET_ID_ANY,
				RTE_MEMZONE_1GB|RTE_MEMZONE_SIZE_HINT_ONLY);
		if (mz == NULL) {
			printf("MEMZONE FLAG 1GB\n");
			return -1;
		}
		if (mz->hugepage_sz != RTE_PGSIZE_1G) {
			printf("hugepage_sz not equal 1G\n");
			return -1;
		}
		if (rte_memzone_free(mz)) {
			printf("Fail memzone free\n");
			return -1;
		}

		/* Check if 1GB huge pages are unavailable, that function fails unless
		 * HINT flag is indicated
		 */
		if (!hugepage_2MB_avail) {
			mz = rte_memzone_reserve(
					TEST_MEMZONE_NAME("flag_zone_2M_HINT"),
					size, SOCKET_ID_ANY,
					RTE_MEMZONE_2MB|RTE_MEMZONE_SIZE_HINT_ONLY);
			if (mz == NULL){
				printf("MEMZONE FLAG 2MB & HINT\n");
				return -1;
			}
			if (mz->hugepage_sz != RTE_PGSIZE_1G) {
				printf("hugepage_sz not equal 1G\n");
				return -1;
			}
			if (rte_memzone_free(mz)) {
				printf("Fail memzone free\n");
				return -1;
			}
			mz = rte_memzone_reserve(
					TEST_MEMZONE_NAME("flag_zone_2M"), size,
					SOCKET_ID_ANY, RTE_MEMZONE_2MB);
			if (mz != NULL) {
				printf("MEMZONE FLAG 2MB\n");
				return -1;
			}
		}

		if (hugepage_2MB_avail && hugepage_1GB_avail) {
			mz = rte_memzone_reserve(
					TEST_MEMZONE_NAME("flag_zone_2M_HINT"),
					size, SOCKET_ID_ANY,
					RTE_MEMZONE_2MB|RTE_MEMZONE_1GB);
			if (mz == NULL) {
				printf("BOTH SIZES SET\n");
				return -1;
			}
			if (mz->hugepage_sz != RTE_PGSIZE_1G &&
					mz->hugepage_sz != RTE_PGSIZE_2M) {
				printf("Wrong size when both sizes set\n");
				return -1;
			}
			if (rte_memzone_free(mz)) {
				printf("Fail memzone free\n");
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
		mz = rte_memzone_reserve(TEST_MEMZONE_NAME("flag_zone_16M"),
				size, SOCKET_ID_ANY, RTE_MEMZONE_16MB);
		if (mz == NULL) {
			printf("MEMZONE FLAG 16MB\n");
			return -1;
		}
		if (mz->hugepage_sz != RTE_PGSIZE_16M) {
			printf("hugepage_sz not equal 16M\n");
			return -1;
		}
		if (rte_memzone_free(mz)) {
			printf("Fail memzone free\n");
			return -1;
		}

		mz = rte_memzone_reserve(
				TEST_MEMZONE_NAME("flag_zone_16M_HINT"), size,
				SOCKET_ID_ANY,
				RTE_MEMZONE_16MB|RTE_MEMZONE_SIZE_HINT_ONLY);
		if (mz == NULL) {
			printf("MEMZONE FLAG 16MB\n");
			return -1;
		}
		if (mz->hugepage_sz != RTE_PGSIZE_16M) {
			printf("hugepage_sz not equal 16M\n");
			return -1;
		}
		if (rte_memzone_free(mz)) {
			printf("Fail memzone free\n");
			return -1;
		}

		/* Check if 1GB huge pages are unavailable, that function fails
		 * unless HINT flag is indicated
		 */
		if (!hugepage_16GB_avail) {
			mz = rte_memzone_reserve(
					TEST_MEMZONE_NAME("flag_zone_16G_HINT"),
					size, SOCKET_ID_ANY,
					RTE_MEMZONE_16GB |
					RTE_MEMZONE_SIZE_HINT_ONLY);
			if (mz == NULL) {
				printf("MEMZONE FLAG 16GB & HINT\n");
				return -1;
			}
			if (mz->hugepage_sz != RTE_PGSIZE_16M) {
				printf("hugepage_sz not equal 16M\n");
				return -1;
			}
			if (rte_memzone_free(mz)) {
				printf("Fail memzone free\n");
				return -1;
			}

			mz = rte_memzone_reserve(
					TEST_MEMZONE_NAME("flag_zone_16G"),
					size,
					SOCKET_ID_ANY, RTE_MEMZONE_16GB);
			if (mz != NULL) {
				printf("MEMZONE FLAG 16GB\n");
				return -1;
			}
		}
	}
	/*As with 16MB tests above for 16GB huge page requests*/
	if (hugepage_16GB_avail) {
		mz = rte_memzone_reserve(TEST_MEMZONE_NAME("flag_zone_16G"),
				size, SOCKET_ID_ANY, RTE_MEMZONE_16GB);
		if (mz == NULL) {
			printf("MEMZONE FLAG 16GB\n");
			return -1;
		}
		if (mz->hugepage_sz != RTE_PGSIZE_16G) {
			printf("hugepage_sz not equal 16G\n");
			return -1;
		}
		if (rte_memzone_free(mz)) {
			printf("Fail memzone free\n");
			return -1;
		}

		mz = rte_memzone_reserve(
				TEST_MEMZONE_NAME("flag_zone_16G_HINT"), size,
				SOCKET_ID_ANY,
				RTE_MEMZONE_16GB|RTE_MEMZONE_SIZE_HINT_ONLY);
		if (mz == NULL) {
			printf("MEMZONE FLAG 16GB\n");
			return -1;
		}
		if (mz->hugepage_sz != RTE_PGSIZE_16G) {
			printf("hugepage_sz not equal 16G\n");
			return -1;
		}
		if (rte_memzone_free(mz)) {
			printf("Fail memzone free\n");
			return -1;
		}

		/* Check if 1GB huge pages are unavailable, that function fails
		 * unless HINT flag is indicated
		 */
		if (!hugepage_16MB_avail) {
			mz = rte_memzone_reserve(
					TEST_MEMZONE_NAME("flag_zone_16M_HINT"),
					size, SOCKET_ID_ANY,
					RTE_MEMZONE_16MB |
					RTE_MEMZONE_SIZE_HINT_ONLY);
			if (mz == NULL) {
				printf("MEMZONE FLAG 16MB & HINT\n");
				return -1;
			}
			if (mz->hugepage_sz != RTE_PGSIZE_16G) {
				printf("hugepage_sz not equal 16G\n");
				return -1;
			}
			if (rte_memzone_free(mz)) {
				printf("Fail memzone free\n");
				return -1;
			}
			mz = rte_memzone_reserve(
					TEST_MEMZONE_NAME("flag_zone_16M"),
					size, SOCKET_ID_ANY, RTE_MEMZONE_16MB);
			if (mz != NULL) {
				printf("MEMZONE FLAG 16MB\n");
				return -1;
			}
		}

		if (hugepage_16MB_avail && hugepage_16GB_avail) {
			mz = rte_memzone_reserve(
					TEST_MEMZONE_NAME("flag_zone_16M_HINT"),
					size, SOCKET_ID_ANY,
					RTE_MEMZONE_16MB|RTE_MEMZONE_16GB);
			if (mz == NULL) {
				printf("BOTH SIZES SET\n");
				return -1;
			}
			if (mz->hugepage_sz != RTE_PGSIZE_16G &&
					mz->hugepage_sz != RTE_PGSIZE_16M) {
				printf("Wrong size when both sizes set\n");
				return -1;
			}
			if (rte_memzone_free(mz)) {
				printf("Fail memzone free\n");
				return -1;
			}
		}
	}
	return 0;
}


/* Find the heap with the greatest free block size */
static size_t
find_max_block_free_size(unsigned int align, unsigned int socket_id)
{
	struct rte_malloc_socket_stats stats;
	size_t len, overhead;

	if (rte_malloc_get_socket_stats(socket_id, &stats) < 0)
		return 0;

	len = stats.greatest_free_size;
	overhead = MALLOC_ELEM_OVERHEAD;

	if (len == 0)
		return 0;

	align = RTE_CACHE_LINE_ROUNDUP(align);
	overhead += align;

	if (len < overhead)
		return 0;

	return len - overhead;
}

static int
test_memzone_reserve_max(void)
{
	unsigned int i;

	for (i = 0; i < rte_socket_count(); i++) {
		const struct rte_memzone *mz;
		size_t maxlen;
		int socket;

		socket = rte_socket_id_by_idx(i);
		maxlen = find_max_block_free_size(0, socket);

		if (maxlen == 0) {
			printf("There is no space left!\n");
			return 0;
		}

		mz = rte_memzone_reserve(TEST_MEMZONE_NAME("max_zone"), 0,
				socket, 0);
		if (mz == NULL) {
			printf("Failed to reserve a big chunk of memory - %s\n",
					rte_strerror(rte_errno));
			rte_dump_physmem_layout(stdout);
			rte_memzone_dump(stdout);
			return -1;
		}

		if (mz->len != maxlen) {
			printf("Memzone reserve with 0 size did not return biggest block\n");
			printf("Expected size = %zu, actual size = %zu\n",
					maxlen, mz->len);
			rte_dump_physmem_layout(stdout);
			rte_memzone_dump(stdout);
			return -1;
		}

		if (rte_memzone_free(mz)) {
			printf("Fail memzone free\n");
			return -1;
		}
	}

	return 0;
}

static int
test_memzone_reserve_max_aligned(void)
{
	unsigned int i;

	for (i = 0; i < rte_socket_count(); i++) {
		const struct rte_memzone *mz;
		size_t maxlen, minlen = 0;
		int socket;

		socket = rte_socket_id_by_idx(i);

		/* random alignment */
		rte_srand((unsigned int)rte_rdtsc());
		const unsigned int align = 1 << ((rte_rand() % 8) + 5); /* from 128 up to 4k alignment */

		/* memzone size may be between size and size - align */
		minlen = find_max_block_free_size(align, socket);
		maxlen = find_max_block_free_size(0, socket);

		if (minlen == 0 || maxlen == 0) {
			printf("There is no space left for biggest %u-aligned memzone!\n",
					align);
			return 0;
		}

		mz = rte_memzone_reserve_aligned(
				TEST_MEMZONE_NAME("max_zone_aligned"),
				0, socket, 0, align);
		if (mz == NULL) {
			printf("Failed to reserve a big chunk of memory - %s\n",
					rte_strerror(rte_errno));
			rte_dump_physmem_layout(stdout);
			rte_memzone_dump(stdout);
			return -1;
		}
		if (mz->addr != RTE_PTR_ALIGN(mz->addr, align)) {
			printf("Memzone reserve with 0 size and alignment %u did not return aligned block\n",
					align);
			rte_dump_physmem_layout(stdout);
			rte_memzone_dump(stdout);
			return -1;
		}

		if (mz->len < minlen || mz->len > maxlen) {
			printf("Memzone reserve with 0 size and alignment %u did not return"
					" biggest block\n", align);
			printf("Expected size = %zu-%zu, actual size = %zu\n",
					minlen, maxlen, mz->len);
			rte_dump_physmem_layout(stdout);
			rte_memzone_dump(stdout);
			return -1;
		}

		if (rte_memzone_free(mz)) {
			printf("Fail memzone free\n");
			return -1;
		}
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
	memzone_aligned_32 = rte_memzone_reserve_aligned(
			TEST_MEMZONE_NAME("aligned_32"), 100, SOCKET_ID_ANY, 0,
			32);

	/* memzone that is supposed to be aligned on a 128 byte boundary */
	memzone_aligned_128 = rte_memzone_reserve_aligned(
			TEST_MEMZONE_NAME("aligned_128"), 100, SOCKET_ID_ANY, 0,
			128);

	/* memzone that is supposed to be aligned on a 256 byte boundary */
	memzone_aligned_256 = rte_memzone_reserve_aligned(
			TEST_MEMZONE_NAME("aligned_256"), 100, SOCKET_ID_ANY, 0,
			256);

	/* memzone that is supposed to be aligned on a 512 byte boundary */
	memzone_aligned_512 = rte_memzone_reserve_aligned(
			TEST_MEMZONE_NAME("aligned_512"), 100, SOCKET_ID_ANY, 0,
			512);

	/* memzone that is supposed to be aligned on a 1024 byte boundary */
	memzone_aligned_1024 = rte_memzone_reserve_aligned(
			TEST_MEMZONE_NAME("aligned_1024"), 100, SOCKET_ID_ANY,
			0, 1024);

	printf("check alignments and lengths\n");
	if (memzone_aligned_32 == NULL) {
		printf("Unable to reserve 64-byte aligned memzone!\n");
		return -1;
	}
	if ((memzone_aligned_32->iova & RTE_CACHE_LINE_MASK) != 0)
		return -1;
	if (((uintptr_t) memzone_aligned_32->addr & RTE_CACHE_LINE_MASK) != 0)
		return -1;
	if ((memzone_aligned_32->len & RTE_CACHE_LINE_MASK) != 0)
		return -1;

	if (memzone_aligned_128 == NULL) {
		printf("Unable to reserve 128-byte aligned memzone!\n");
		return -1;
	}
	if ((memzone_aligned_128->iova & 127) != 0)
		return -1;
	if (((uintptr_t) memzone_aligned_128->addr & 127) != 0)
		return -1;
	if ((memzone_aligned_128->len & RTE_CACHE_LINE_MASK) != 0)
		return -1;

	if (memzone_aligned_256 == NULL) {
		printf("Unable to reserve 256-byte aligned memzone!\n");
		return -1;
	}
	if ((memzone_aligned_256->iova & 255) != 0)
		return -1;
	if (((uintptr_t) memzone_aligned_256->addr & 255) != 0)
		return -1;
	if ((memzone_aligned_256->len & RTE_CACHE_LINE_MASK) != 0)
		return -1;

	if (memzone_aligned_512 == NULL) {
		printf("Unable to reserve 512-byte aligned memzone!\n");
		return -1;
	}
	if ((memzone_aligned_512->iova & 511) != 0)
		return -1;
	if (((uintptr_t) memzone_aligned_512->addr & 511) != 0)
		return -1;
	if ((memzone_aligned_512->len & RTE_CACHE_LINE_MASK) != 0)
		return -1;

	if (memzone_aligned_1024 == NULL) {
		printf("Unable to reserve 1024-byte aligned memzone!\n");
		return -1;
	}
	if ((memzone_aligned_1024->iova & 1023) != 0)
		return -1;
	if (((uintptr_t) memzone_aligned_1024->addr & 1023) != 0)
		return -1;
	if ((memzone_aligned_1024->len & RTE_CACHE_LINE_MASK) != 0)
		return -1;

	/* check that zones don't overlap */
	printf("check overlapping\n");
	if (is_memory_overlap(memzone_aligned_32->iova, memzone_aligned_32->len,
					memzone_aligned_128->iova, memzone_aligned_128->len))
		return -1;
	if (is_memory_overlap(memzone_aligned_32->iova, memzone_aligned_32->len,
					memzone_aligned_256->iova, memzone_aligned_256->len))
		return -1;
	if (is_memory_overlap(memzone_aligned_32->iova, memzone_aligned_32->len,
					memzone_aligned_512->iova, memzone_aligned_512->len))
		return -1;
	if (is_memory_overlap(memzone_aligned_32->iova, memzone_aligned_32->len,
					memzone_aligned_1024->iova, memzone_aligned_1024->len))
		return -1;
	if (is_memory_overlap(memzone_aligned_128->iova, memzone_aligned_128->len,
					memzone_aligned_256->iova, memzone_aligned_256->len))
		return -1;
	if (is_memory_overlap(memzone_aligned_128->iova, memzone_aligned_128->len,
					memzone_aligned_512->iova, memzone_aligned_512->len))
		return -1;
	if (is_memory_overlap(memzone_aligned_128->iova, memzone_aligned_128->len,
					memzone_aligned_1024->iova, memzone_aligned_1024->len))
		return -1;
	if (is_memory_overlap(memzone_aligned_256->iova, memzone_aligned_256->len,
					memzone_aligned_512->iova, memzone_aligned_512->len))
		return -1;
	if (is_memory_overlap(memzone_aligned_256->iova, memzone_aligned_256->len,
					memzone_aligned_1024->iova, memzone_aligned_1024->len))
		return -1;
	if (is_memory_overlap(memzone_aligned_512->iova, memzone_aligned_512->len,
					memzone_aligned_1024->iova, memzone_aligned_1024->len))
		return -1;

	/* free all used zones */
	if (rte_memzone_free(memzone_aligned_32)) {
		printf("Fail memzone free\n");
		return -1;
	}
	if (rte_memzone_free(memzone_aligned_128)) {
		printf("Fail memzone free\n");
		return -1;
	}
	if (rte_memzone_free(memzone_aligned_256)) {
		printf("Fail memzone free\n");
		return -1;
	}
	if (rte_memzone_free(memzone_aligned_512)) {
		printf("Fail memzone free\n");
		return -1;
	}
	if (rte_memzone_free(memzone_aligned_1024)) {
		printf("Fail memzone free\n");
		return -1;
	}
	return 0;
}

static int
check_memzone_bounded(const char *name, uint32_t len,  uint32_t align,
	uint32_t bound)
{
	const struct rte_memzone *mz;
	rte_iova_t bmask;

	bmask = ~((rte_iova_t)bound - 1);

	if ((mz = rte_memzone_reserve_bounded(name, len, SOCKET_ID_ANY, 0,
			align, bound)) == NULL) {
		printf("%s(%s): memzone creation failed\n",
			__func__, name);
		return -1;
	}

	if ((mz->iova & ((rte_iova_t)align - 1)) != 0) {
		printf("%s(%s): invalid phys addr alignment\n",
			__func__, mz->name);
		return -1;
	}

	if (((uintptr_t) mz->addr & ((uintptr_t)align - 1)) != 0) {
		printf("%s(%s): invalid virtual addr alignment\n",
			__func__, mz->name);
		return -1;
	}

	if ((mz->len & RTE_CACHE_LINE_MASK) != 0 || mz->len < len ||
			mz->len < RTE_CACHE_LINE_SIZE) {
		printf("%s(%s): invalid length\n",
			__func__, mz->name);
		return -1;
	}

	if ((mz->iova & bmask) !=
			((mz->iova + mz->len - 1) & bmask)) {
		printf("%s(%s): invalid memzone boundary %u crossed\n",
			__func__, mz->name, bound);
		return -1;
	}

	if (rte_memzone_free(mz)) {
		printf("Fail memzone free\n");
		return -1;
	}

	return 0;
}

static int
test_memzone_bounded(void)
{
	const struct rte_memzone *memzone_err;
	int rc;

	/* should fail as boundary is not power of two */
	memzone_err = rte_memzone_reserve_bounded(
			TEST_MEMZONE_NAME("bounded_error_31"), 100,
			SOCKET_ID_ANY, 0, 32, UINT32_MAX);
	if (memzone_err != NULL) {
		printf("%s(%s)created a memzone with invalid boundary "
			"conditions\n", __func__, memzone_err->name);
		return -1;
	}

	/* should fail as len is greater then boundary */
	memzone_err = rte_memzone_reserve_bounded(
			TEST_MEMZONE_NAME("bounded_error_32"), 100,
			SOCKET_ID_ANY, 0, 32, 32);
	if (memzone_err != NULL) {
		printf("%s(%s)created a memzone with invalid boundary "
			"conditions\n", __func__, memzone_err->name);
		return -1;
	}

	rc = check_memzone_bounded(TEST_MEMZONE_NAME("bounded_128"), 100, 128,
			128);
	if (rc != 0)
		return rc;

	rc = check_memzone_bounded(TEST_MEMZONE_NAME("bounded_256"), 100, 256,
			128);
	if (rc != 0)
		return rc;

	rc = check_memzone_bounded(TEST_MEMZONE_NAME("bounded_1K"), 100, 64,
			1024);
	if (rc != 0)
		return rc;

	rc = check_memzone_bounded(TEST_MEMZONE_NAME("bounded_1K_MAX"), 0, 64,
			1024);
	if (rc != 0)
		return rc;

	return 0;
}

static int
test_memzone_free(void)
{
	const struct rte_memzone **mz;
	int i;
	char name[20];
	int rc = -1;

	mz = rte_calloc("memzone_test", rte_memzone_max_get() + 1,
			sizeof(struct rte_memzone *), 0);
	if (!mz) {
		printf("Fail allocating memzone test array\n");
		return rc;
	}

	mz[0] = rte_memzone_reserve(TEST_MEMZONE_NAME("tempzone0"), 2000,
			SOCKET_ID_ANY, 0);
	mz[1] = rte_memzone_reserve(TEST_MEMZONE_NAME("tempzone1"), 4000,
			SOCKET_ID_ANY, 0);

	if (mz[0] > mz[1])
		goto exit_test;
	if (!rte_memzone_lookup(TEST_MEMZONE_NAME("tempzone0")))
		goto exit_test;
	if (!rte_memzone_lookup(TEST_MEMZONE_NAME("tempzone1")))
		goto exit_test;

	if (rte_memzone_free(mz[0])) {
		printf("Fail memzone free - tempzone0\n");
		goto exit_test;
	}
	if (rte_memzone_lookup(TEST_MEMZONE_NAME("tempzone0"))) {
		printf("Found previously free memzone - tempzone0\n");
		goto exit_test;
	}
	mz[2] = rte_memzone_reserve(TEST_MEMZONE_NAME("tempzone2"), 2000,
			SOCKET_ID_ANY, 0);

	if (mz[2] > mz[1]) {
		printf("tempzone2 should have gotten the free entry from tempzone0\n");
		goto exit_test;
	}
	if (rte_memzone_free(mz[2])) {
		printf("Fail memzone free - tempzone2\n");
		goto exit_test;
	}
	if (rte_memzone_lookup(TEST_MEMZONE_NAME("tempzone2"))) {
		printf("Found previously free memzone - tempzone2\n");
		goto exit_test;
	}
	if (rte_memzone_free(mz[1])) {
		printf("Fail memzone free - tempzone1\n");
		goto exit_test;
	}
	if (rte_memzone_lookup(TEST_MEMZONE_NAME("tempzone1"))) {
		printf("Found previously free memzone - tempzone1\n");
		goto exit_test;
	}

	i = 0;
	do {
		snprintf(name, sizeof(name), TEST_MEMZONE_NAME("tempzone%u"),
				i);
		mz[i] = rte_memzone_reserve(name, 1, SOCKET_ID_ANY, 0);
	} while (mz[i++] != NULL);

	if (rte_memzone_free(mz[0])) {
		printf("Fail memzone free - tempzone0\n");
		goto exit_test;
	}
	mz[0] = rte_memzone_reserve(TEST_MEMZONE_NAME("tempzone0new"), 0,
			SOCKET_ID_ANY, 0);

	if (mz[0] == NULL) {
		printf("Fail to create memzone - tempzone0new - when MAX memzones were "
				"created and one was free\n");
		goto exit_test;
	}

	for (i = i - 2; i >= 0; i--) {
		if (rte_memzone_free(mz[i])) {
			printf("Fail memzone free - tempzone%d\n", i);
			goto exit_test;
		}
	}

	rc = 0;

exit_test:
	rte_free(mz);
	return rc;
}

static int test_memzones_left;
static int memzone_walk_cnt;
static void memzone_walk_clb(const struct rte_memzone *mz,
			     void *arg __rte_unused)
{
	memzone_walk_cnt++;
	if (!strncmp(TEST_MEMZONE_NAME(""), mz->name, RTE_MEMZONE_NAMESIZE))
		test_memzones_left++;
}

static int
test_memzone_basic(void)
{
	const struct rte_memzone *memzone1;
	const struct rte_memzone *memzone2;
	const struct rte_memzone *memzone3;
	const struct rte_memzone *memzone4;
	const struct rte_memzone *mz;
	int memzone_cnt_after, memzone_cnt_expected;
	int memzone_cnt_before;

	memzone_walk_cnt = 0;
	test_memzones_left = 0;
	rte_memzone_walk(memzone_walk_clb, NULL);
	memzone_cnt_before = memzone_walk_cnt;

	memzone1 = rte_memzone_reserve(TEST_MEMZONE_NAME("testzone1"), 100,
				SOCKET_ID_ANY, 0);

	memzone2 = rte_memzone_reserve(TEST_MEMZONE_NAME("testzone2"), 1000,
				0, 0);

	memzone3 = rte_memzone_reserve(TEST_MEMZONE_NAME("testzone3"), 1000,
				1, 0);

	memzone4 = rte_memzone_reserve(TEST_MEMZONE_NAME("testzone4"), 1024,
				SOCKET_ID_ANY, 0);

	/* memzone3 may be NULL if we don't have NUMA */
	if (memzone1 == NULL || memzone2 == NULL || memzone4 == NULL)
		return -1;

	/* check how many memzones we are expecting */
	memzone_cnt_expected = memzone_cnt_before +
			(memzone1 != NULL) + (memzone2 != NULL) +
			(memzone3 != NULL) + (memzone4 != NULL);

	memzone_walk_cnt = 0;
	test_memzones_left = 0;
	rte_memzone_walk(memzone_walk_clb, NULL);
	memzone_cnt_after = memzone_walk_cnt;

	if (memzone_cnt_after != memzone_cnt_expected)
		return -1;


	rte_memzone_dump(stdout);

	/* check cache-line alignments */
	printf("check alignments and lengths\n");

	if ((memzone1->iova & RTE_CACHE_LINE_MASK) != 0)
		return -1;
	if ((memzone2->iova & RTE_CACHE_LINE_MASK) != 0)
		return -1;
	if (memzone3 != NULL && (memzone3->iova & RTE_CACHE_LINE_MASK) != 0)
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

	if (is_memory_overlap(memzone1->iova, memzone1->len,
			memzone2->iova, memzone2->len))
		return -1;
	if (memzone3 != NULL &&
			is_memory_overlap(memzone1->iova, memzone1->len,
					memzone3->iova, memzone3->len))
		return -1;
	if (memzone3 != NULL &&
			is_memory_overlap(memzone2->iova, memzone2->len,
					memzone3->iova, memzone3->len))
		return -1;

	printf("check socket ID\n");

	/* memzone2 must be on socket id 0 and memzone3 on socket 1 */
	if (memzone2->socket_id != 0)
		return -1;
	if (memzone3 != NULL && memzone3->socket_id != 1)
		return -1;

	printf("test zone lookup\n");
	mz = rte_memzone_lookup(TEST_MEMZONE_NAME("testzone1"));
	if (mz != memzone1)
		return -1;

	printf("test duplicate zone name\n");
	mz = rte_memzone_reserve(TEST_MEMZONE_NAME("testzone1"), 100,
			SOCKET_ID_ANY, 0);
	if (mz != NULL)
		return -1;

	if (rte_memzone_free(memzone1)) {
		printf("Fail memzone free - memzone1\n");
		return -1;
	}
	if (rte_memzone_free(memzone2)) {
		printf("Fail memzone free - memzone2\n");
		return -1;
	}
	if (memzone3 && rte_memzone_free(memzone3)) {
		printf("Fail memzone free - memzone3\n");
		return -1;
	}
	if (rte_memzone_free(memzone4)) {
		printf("Fail memzone free - memzone4\n");
		return -1;
	}

	memzone_walk_cnt = 0;
	test_memzones_left = 0;
	rte_memzone_walk(memzone_walk_clb, NULL);
	memzone_cnt_after = memzone_walk_cnt;
	if (memzone_cnt_after != memzone_cnt_before)
		return -1;

	return 0;
}

static int
test_memzone(void)
{
	/* take note of how many memzones were allocated before running */
	int memzone_cnt;

	memzone_walk_cnt = 0;
	test_memzones_left = 0;
	rte_memzone_walk(memzone_walk_clb, NULL);
	memzone_cnt = memzone_walk_cnt;

	printf("test basic memzone API\n");
	if (test_memzone_basic() < 0)
		return -1;

	printf("test free memzone\n");
	if (test_memzone_free() < 0)
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

	printf("test boundary alignment for memzone_reserve\n");
	if (test_memzone_bounded() < 0)
		return -1;

	printf("test invalid alignment for memzone_reserve\n");
	if (test_memzone_invalid_alignment() < 0)
		return -1;

	printf("test invalid flags for memzone_reserve\n");
	if (test_memzone_invalid_flags() < 0)
		return -1;

	printf("test reserving the largest size memzone possible\n");
	if (test_memzone_reserve_max() < 0)
		return -1;

	printf("test reserving the largest size aligned memzone possible\n");
	if (test_memzone_reserve_max_aligned() < 0)
		return -1;

	printf("check memzone cleanup\n");
	memzone_walk_cnt = 0;
	test_memzones_left = 0;
	rte_memzone_walk(memzone_walk_clb, NULL);
	if (memzone_walk_cnt != memzone_cnt || test_memzones_left > 0) {
		printf("there are some memzones left after test\n");
		rte_memzone_dump(stdout);
		return -1;
	}

	return 0;
}

REGISTER_TEST_COMMAND(memzone_autotest, test_memzone);
