/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#include <stdio.h>
#include <inttypes.h>

#include <rte_common.h>
#include <rte_bitmap.h>
#include <rte_malloc.h>

#include "test.h"

#define MAX_BITS 1000

static int
test_bitmap_scan_operations(struct rte_bitmap *bmp)
{
	uint32_t pos = 0;
	uint64_t slab1_magic = 0xBADC0FFEEBADF00D;
	uint64_t slab2_magic = 0xFEEDDEADDEADF00D;
	uint64_t out_slab = 0;

	rte_bitmap_reset(bmp);

	rte_bitmap_set_slab(bmp, pos, slab1_magic);
	rte_bitmap_set_slab(bmp, pos + RTE_BITMAP_SLAB_BIT_SIZE, slab2_magic);

	if (!rte_bitmap_scan(bmp, &pos, &out_slab)) {
		printf("Failed to get slab from bitmap.\n");
		return TEST_FAILED;
	}

	if (slab1_magic != out_slab) {
		printf("Scan operation sanity failed.\n");
		return TEST_FAILED;
	}

	if (!rte_bitmap_scan(bmp, &pos, &out_slab)) {
		printf("Failed to get slab from bitmap.\n");
		return TEST_FAILED;
	}

	if (slab2_magic != out_slab) {
		printf("Scan operation sanity failed.\n");
		return TEST_FAILED;
	}

	/* Wrap around */
	if (!rte_bitmap_scan(bmp, &pos, &out_slab)) {
		printf("Failed to get slab from bitmap.\n");
		return TEST_FAILED;
	}

	if (slab1_magic != out_slab) {
		printf("Scan operation wrap around failed.\n");
		return TEST_FAILED;
	}

	/* Scan reset check. */
	__rte_bitmap_scan_init(bmp);

	if (!rte_bitmap_scan(bmp, &pos, &out_slab)) {
		printf("Failed to get slab from bitmap.\n");
		return TEST_FAILED;
	}

	if (slab1_magic != out_slab) {
		printf("Scan reset operation failed.\n");
		return TEST_FAILED;
	}

	return TEST_SUCCESS;
}

static int
test_bitmap_slab_set_get(struct rte_bitmap *bmp)
{
	uint32_t pos = 0;
	uint64_t slab_magic = 0xBADC0FFEEBADF00D;
	uint64_t out_slab = 0;

	rte_bitmap_reset(bmp);
	rte_bitmap_set_slab(bmp, pos, slab_magic);

	if (!rte_bitmap_scan(bmp, &pos, &out_slab)) {
		printf("Failed to get slab from bitmap.\n");
		return TEST_FAILED;
	}


	if (slab_magic != out_slab) {
		printf("Invalid slab in bitmap.\n");
		return TEST_FAILED;
	}


	return TEST_SUCCESS;
}

static int
test_bitmap_set_get_clear(struct rte_bitmap *bmp)
{
	uint64_t val;
	int i;

	rte_bitmap_reset(bmp);
	for (i = 0; i < MAX_BITS; i++)
		rte_bitmap_set(bmp, i);

	for (i = 0; i < MAX_BITS; i++) {
		if (!rte_bitmap_get(bmp, i)) {
			printf("Failed to get set bit.\n");
			return TEST_FAILED;
		}
	}

	for (i = 0; i < MAX_BITS; i++)
		rte_bitmap_clear(bmp, i);

	for (i = 0; i < MAX_BITS; i++) {
		if (rte_bitmap_get(bmp, i)) {
			printf("Failed to clear set bit.\n");
			return TEST_FAILED;
		}
	}

	rte_bitmap_reset(bmp);

	/* Alternate slab set test */
	for (i = 0; i < MAX_BITS; i++) {
		if (i % RTE_BITMAP_SLAB_BIT_SIZE)
			rte_bitmap_set(bmp, i);
	}

	for (i = 0; i < MAX_BITS; i++) {
		val = rte_bitmap_get(bmp, i);
		if (((i % RTE_BITMAP_SLAB_BIT_SIZE) && !val) ||
		    (!(i % RTE_BITMAP_SLAB_BIT_SIZE) && val)) {
			printf("Failed to get set bit.\n");
			return TEST_FAILED;
		}
	}

	return TEST_SUCCESS;
}

static int
test_bitmap_all_clear(void)
{
	void *mem;
	uint32_t bmp_size;
	struct rte_bitmap *bmp;

	bmp_size =
		rte_bitmap_get_memory_footprint(MAX_BITS);

	mem = rte_zmalloc("test_bmap", bmp_size, RTE_CACHE_LINE_SIZE);
	if (mem == NULL) {
		printf("Failed to allocate memory for bitmap\n");
		return TEST_FAILED;
	}

	bmp = rte_bitmap_init(MAX_BITS, mem, bmp_size);
	if (bmp == NULL) {
		printf("Failed to init bitmap\n");
		return TEST_FAILED;
	}

	if (test_bitmap_set_get_clear(bmp) < 0)
		return TEST_FAILED;

	if (test_bitmap_slab_set_get(bmp) < 0)
		return TEST_FAILED;

	if (test_bitmap_scan_operations(bmp) < 0)
		return TEST_FAILED;

	rte_bitmap_free(bmp);
	rte_free(mem);

	return TEST_SUCCESS;
}

static int
test_bitmap_all_set(void)
{
	void *mem;
	uint32_t i;
	uint64_t slab;
	uint32_t pos;
	uint32_t bmp_size;
	struct rte_bitmap *bmp;

	bmp_size =
		rte_bitmap_get_memory_footprint(MAX_BITS);

	mem = rte_zmalloc("test_bmap", bmp_size, RTE_CACHE_LINE_SIZE);
	if (mem == NULL) {
		printf("Failed to allocate memory for bitmap\n");
		return TEST_FAILED;
	}

	bmp = rte_bitmap_init_with_all_set(MAX_BITS, mem, bmp_size);
	if (bmp == NULL) {
		printf("Failed to init bitmap\n");
		return TEST_FAILED;
	}

	for (i = 0; i < MAX_BITS; i++) {
		pos = slab = 0;
		if (!rte_bitmap_scan(bmp, &pos, &slab)) {
			printf("Failed with init bitmap.\n");
			return TEST_FAILED;
		}
		pos += (slab ? __builtin_ctzll(slab) : 0);
		rte_bitmap_clear(bmp, pos);
	}

	if (rte_bitmap_scan(bmp, &pos, &slab)) {
		printf("Too much bits set.\n");
		return TEST_FAILED;
	}

	rte_bitmap_free(bmp);
	rte_free(mem);

	return TEST_SUCCESS;

}

static int
test_bitmap(void)
{
	if (test_bitmap_all_clear() != TEST_SUCCESS)
		return TEST_FAILED;
	return test_bitmap_all_set();
}

REGISTER_TEST_COMMAND(bitmap_test, test_bitmap);
