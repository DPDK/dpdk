/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 * Copyright(c) 2020 Arm Limited
 */

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/queue.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_launch.h>
#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_malloc.h>
#include <rte_ring.h>
#include <rte_ring_elem.h>
#include <rte_random.h>
#include <rte_errno.h>
#include <rte_hexdump.h>

#include "test.h"
#include "test_ring.h"

/*
 * Ring
 * ====
 *
 * #. Functional tests. Tests single/bulk/burst, default/SPSC/MPMC,
 *    legacy/custom element size (4B, 8B, 16B, 20B) APIs.
 *    Some tests incorporate unaligned addresses for objects.
 *    The enqueued/dequeued data is validated for correctness.
 *
 * #. Performance tests are in test_ring_perf.c
 */

#define RING_SIZE 4096
#define MAX_BULK 32

/*
 * Validate the return value of test cases and print details of the
 * ring if validation fails
 *
 * @param exp
 *   Expression to validate return value.
 * @param r
 *   A pointer to the ring structure.
 */
#define TEST_RING_VERIFY(exp, r, errst) do {				\
	if (!(exp)) {							\
		printf("error at %s:%d\tcondition " #exp " failed\n",	\
		    __func__, __LINE__);				\
		rte_ring_dump(stdout, (r));				\
		errst;							\
	}								\
} while (0)

#define TEST_RING_FULL_EMPTY_ITER	8

static const int esize[] = {-1, 4, 8, 16, 20};

/* Wrappers around the zero-copy APIs. The wrappers match
 * the normal enqueue/dequeue API declarations.
 */
static unsigned int
test_ring_enqueue_zc_bulk(struct rte_ring *r, void * const *obj_table,
	unsigned int n, unsigned int *free_space)
{
	uint32_t ret;
	struct rte_ring_zc_data zcd;

	ret = rte_ring_enqueue_zc_bulk_start(r, n, &zcd, free_space);
	if (ret != 0) {
		/* Copy the data to the ring */
		test_ring_copy_to(&zcd, obj_table, sizeof(void *), ret);
		rte_ring_enqueue_zc_finish(r, ret);
	}

	return ret;
}

static unsigned int
test_ring_enqueue_zc_bulk_elem(struct rte_ring *r, const void *obj_table,
	unsigned int esize, unsigned int n, unsigned int *free_space)
{
	unsigned int ret;
	struct rte_ring_zc_data zcd;

	ret = rte_ring_enqueue_zc_bulk_elem_start(r, esize, n,
				&zcd, free_space);
	if (ret != 0) {
		/* Copy the data to the ring */
		test_ring_copy_to(&zcd, obj_table, esize, ret);
		rte_ring_enqueue_zc_finish(r, ret);
	}

	return ret;
}

static unsigned int
test_ring_enqueue_zc_burst(struct rte_ring *r, void * const *obj_table,
	unsigned int n, unsigned int *free_space)
{
	unsigned int ret;
	struct rte_ring_zc_data zcd;

	ret = rte_ring_enqueue_zc_burst_start(r, n, &zcd, free_space);
	if (ret != 0) {
		/* Copy the data to the ring */
		test_ring_copy_to(&zcd, obj_table, sizeof(void *), ret);
		rte_ring_enqueue_zc_finish(r, ret);
	}

	return ret;
}

static unsigned int
test_ring_enqueue_zc_burst_elem(struct rte_ring *r, const void *obj_table,
	unsigned int esize, unsigned int n, unsigned int *free_space)
{
	unsigned int ret;
	struct rte_ring_zc_data zcd;

	ret = rte_ring_enqueue_zc_burst_elem_start(r, esize, n,
				&zcd, free_space);
	if (ret != 0) {
		/* Copy the data to the ring */
		test_ring_copy_to(&zcd, obj_table, esize, ret);
		rte_ring_enqueue_zc_finish(r, ret);
	}

	return ret;
}

static unsigned int
test_ring_dequeue_zc_bulk(struct rte_ring *r, void **obj_table,
	unsigned int n, unsigned int *available)
{
	unsigned int ret;
	struct rte_ring_zc_data zcd;

	ret = rte_ring_dequeue_zc_bulk_start(r, n, &zcd, available);
	if (ret != 0) {
		/* Copy the data from the ring */
		test_ring_copy_from(&zcd, obj_table, sizeof(void *), ret);
		rte_ring_dequeue_zc_finish(r, ret);
	}

	return ret;
}

static unsigned int
test_ring_dequeue_zc_bulk_elem(struct rte_ring *r, void *obj_table,
	unsigned int esize, unsigned int n, unsigned int *available)
{
	unsigned int ret;
	struct rte_ring_zc_data zcd;

	ret = rte_ring_dequeue_zc_bulk_elem_start(r, esize, n,
				&zcd, available);
	if (ret != 0) {
		/* Copy the data from the ring */
		test_ring_copy_from(&zcd, obj_table, esize, ret);
		rte_ring_dequeue_zc_finish(r, ret);
	}

	return ret;
}

static unsigned int
test_ring_dequeue_zc_burst(struct rte_ring *r, void **obj_table,
	unsigned int n, unsigned int *available)
{
	unsigned int ret;
	struct rte_ring_zc_data zcd;

	ret = rte_ring_dequeue_zc_burst_start(r, n, &zcd, available);
	if (ret != 0) {
		/* Copy the data from the ring */
		test_ring_copy_from(&zcd, obj_table, sizeof(void *), ret);
		rte_ring_dequeue_zc_finish(r, ret);
	}

	return ret;
}

static unsigned int
test_ring_dequeue_zc_burst_elem(struct rte_ring *r, void *obj_table,
	unsigned int esize, unsigned int n, unsigned int *available)
{
	unsigned int ret;
	struct rte_ring_zc_data zcd;

	ret = rte_ring_dequeue_zc_burst_elem_start(r, esize, n,
				&zcd, available);
	if (ret != 0) {
		/* Copy the data from the ring */
		test_ring_copy_from(&zcd, obj_table, esize, ret);
		rte_ring_dequeue_zc_finish(r, ret);
	}

	return ret;
}

static const struct {
	const char *desc;
	uint32_t api_type;
	uint32_t create_flags;
	struct {
		unsigned int (*flegacy)(struct rte_ring *r,
			void * const *obj_table, unsigned int n,
			unsigned int *free_space);
		unsigned int (*felem)(struct rte_ring *r, const void *obj_table,
			unsigned int esize, unsigned int n,
			unsigned int *free_space);
	} enq;
	struct {
		unsigned int (*flegacy)(struct rte_ring *r,
			void **obj_table, unsigned int n,
			unsigned int *available);
		unsigned int (*felem)(struct rte_ring *r, void *obj_table,
			unsigned int esize, unsigned int n,
			unsigned int *available);
	} deq;
} test_enqdeq_impl[] = {
	{
		.desc = "MP/MC sync mode",
		.api_type = TEST_RING_ELEM_BULK | TEST_RING_THREAD_DEF,
		.create_flags = 0,
		.enq = {
			.flegacy = rte_ring_enqueue_bulk,
			.felem = rte_ring_enqueue_bulk_elem,
		},
		.deq = {
			.flegacy = rte_ring_dequeue_bulk,
			.felem = rte_ring_dequeue_bulk_elem,
		},
	},
	{
		.desc = "SP/SC sync mode",
		.api_type = TEST_RING_ELEM_BULK | TEST_RING_THREAD_SPSC,
		.create_flags = RING_F_SP_ENQ | RING_F_SC_DEQ,
		.enq = {
			.flegacy = rte_ring_sp_enqueue_bulk,
			.felem = rte_ring_sp_enqueue_bulk_elem,
		},
		.deq = {
			.flegacy = rte_ring_sc_dequeue_bulk,
			.felem = rte_ring_sc_dequeue_bulk_elem,
		},
	},
	{
		.desc = "MP/MC sync mode",
		.api_type = TEST_RING_ELEM_BULK | TEST_RING_THREAD_MPMC,
		.create_flags = 0,
		.enq = {
			.flegacy = rte_ring_mp_enqueue_bulk,
			.felem = rte_ring_mp_enqueue_bulk_elem,
		},
		.deq = {
			.flegacy = rte_ring_mc_dequeue_bulk,
			.felem = rte_ring_mc_dequeue_bulk_elem,
		},
	},
	{
		.desc = "MP_RTS/MC_RTS sync mode",
		.api_type = TEST_RING_ELEM_BULK | TEST_RING_THREAD_DEF,
		.create_flags = RING_F_MP_RTS_ENQ | RING_F_MC_RTS_DEQ,
		.enq = {
			.flegacy = rte_ring_enqueue_bulk,
			.felem = rte_ring_enqueue_bulk_elem,
		},
		.deq = {
			.flegacy = rte_ring_dequeue_bulk,
			.felem = rte_ring_dequeue_bulk_elem,
		},
	},
	{
		.desc = "MP_HTS/MC_HTS sync mode",
		.api_type = TEST_RING_ELEM_BULK | TEST_RING_THREAD_DEF,
		.create_flags = RING_F_MP_HTS_ENQ | RING_F_MC_HTS_DEQ,
		.enq = {
			.flegacy = rte_ring_enqueue_bulk,
			.felem = rte_ring_enqueue_bulk_elem,
		},
		.deq = {
			.flegacy = rte_ring_dequeue_bulk,
			.felem = rte_ring_dequeue_bulk_elem,
		},
	},
	{
		.desc = "MP/MC sync mode",
		.api_type = TEST_RING_ELEM_BURST | TEST_RING_THREAD_DEF,
		.create_flags = 0,
		.enq = {
			.flegacy = rte_ring_enqueue_burst,
			.felem = rte_ring_enqueue_burst_elem,
		},
		.deq = {
			.flegacy = rte_ring_dequeue_burst,
			.felem = rte_ring_dequeue_burst_elem,
		},
	},
	{
		.desc = "SP/SC sync mode",
		.api_type = TEST_RING_ELEM_BURST | TEST_RING_THREAD_SPSC,
		.create_flags = RING_F_SP_ENQ | RING_F_SC_DEQ,
		.enq = {
			.flegacy = rte_ring_sp_enqueue_burst,
			.felem = rte_ring_sp_enqueue_burst_elem,
		},
		.deq = {
			.flegacy = rte_ring_sc_dequeue_burst,
			.felem = rte_ring_sc_dequeue_burst_elem,
		},
	},
	{
		.desc = "MP/MC sync mode",
		.api_type = TEST_RING_ELEM_BURST | TEST_RING_THREAD_MPMC,
		.create_flags = 0,
		.enq = {
			.flegacy = rte_ring_mp_enqueue_burst,
			.felem = rte_ring_mp_enqueue_burst_elem,
		},
		.deq = {
			.flegacy = rte_ring_mc_dequeue_burst,
			.felem = rte_ring_mc_dequeue_burst_elem,
		},
	},
	{
		.desc = "MP_RTS/MC_RTS sync mode",
		.api_type = TEST_RING_ELEM_BURST | TEST_RING_THREAD_DEF,
		.create_flags = RING_F_MP_RTS_ENQ | RING_F_MC_RTS_DEQ,
		.enq = {
			.flegacy = rte_ring_enqueue_burst,
			.felem = rte_ring_enqueue_burst_elem,
		},
		.deq = {
			.flegacy = rte_ring_dequeue_burst,
			.felem = rte_ring_dequeue_burst_elem,
		},
	},
	{
		.desc = "MP_HTS/MC_HTS sync mode",
		.api_type = TEST_RING_ELEM_BURST | TEST_RING_THREAD_DEF,
		.create_flags = RING_F_MP_HTS_ENQ | RING_F_MC_HTS_DEQ,
		.enq = {
			.flegacy = rte_ring_enqueue_burst,
			.felem = rte_ring_enqueue_burst_elem,
		},
		.deq = {
			.flegacy = rte_ring_dequeue_burst,
			.felem = rte_ring_dequeue_burst_elem,
		},
	},
	{
		.desc = "SP/SC sync mode (ZC)",
		.api_type = TEST_RING_ELEM_BULK | TEST_RING_THREAD_SPSC,
		.create_flags = RING_F_SP_ENQ | RING_F_SC_DEQ,
		.enq = {
			.flegacy = test_ring_enqueue_zc_bulk,
			.felem = test_ring_enqueue_zc_bulk_elem,
		},
		.deq = {
			.flegacy = test_ring_dequeue_zc_bulk,
			.felem = test_ring_dequeue_zc_bulk_elem,
		},
	},
	{
		.desc = "MP_HTS/MC_HTS sync mode (ZC)",
		.api_type = TEST_RING_ELEM_BULK | TEST_RING_THREAD_DEF,
		.create_flags = RING_F_MP_HTS_ENQ | RING_F_MC_HTS_DEQ,
		.enq = {
			.flegacy = test_ring_enqueue_zc_bulk,
			.felem = test_ring_enqueue_zc_bulk_elem,
		},
		.deq = {
			.flegacy = test_ring_dequeue_zc_bulk,
			.felem = test_ring_dequeue_zc_bulk_elem,
		},
	},
	{
		.desc = "SP/SC sync mode (ZC)",
		.api_type = TEST_RING_ELEM_BURST | TEST_RING_THREAD_SPSC,
		.create_flags = RING_F_SP_ENQ | RING_F_SC_DEQ,
		.enq = {
			.flegacy = test_ring_enqueue_zc_burst,
			.felem = test_ring_enqueue_zc_burst_elem,
		},
		.deq = {
			.flegacy = test_ring_dequeue_zc_burst,
			.felem = test_ring_dequeue_zc_burst_elem,
		},
	},
	{
		.desc = "MP_HTS/MC_HTS sync mode (ZC)",
		.api_type = TEST_RING_ELEM_BURST | TEST_RING_THREAD_DEF,
		.create_flags = RING_F_MP_HTS_ENQ | RING_F_MC_HTS_DEQ,
		.enq = {
			.flegacy = test_ring_enqueue_zc_burst,
			.felem = test_ring_enqueue_zc_burst_elem,
		},
		.deq = {
			.flegacy = test_ring_dequeue_zc_burst,
			.felem = test_ring_dequeue_zc_burst_elem,
		},
	}
};

static unsigned int
test_ring_enq_impl(struct rte_ring *r, void **obj, int esize, unsigned int n,
	unsigned int test_idx)
{
	if (esize == -1)
		return test_enqdeq_impl[test_idx].enq.flegacy(r, obj, n, NULL);
	else
		return test_enqdeq_impl[test_idx].enq.felem(r, obj, esize, n,
			NULL);
}

static unsigned int
test_ring_deq_impl(struct rte_ring *r, void **obj, int esize, unsigned int n,
	unsigned int test_idx)
{
	if (esize == -1)
		return test_enqdeq_impl[test_idx].deq.flegacy(r, obj, n, NULL);
	else
		return test_enqdeq_impl[test_idx].deq.felem(r, obj, esize, n,
			NULL);
}

static void
test_ring_mem_init(void *obj, unsigned int count, int esize)
{
	unsigned int i;

	/* Legacy queue APIs? */
	if (esize == -1)
		for (i = 0; i < count; i++)
			((void **)obj)[i] = (void *)(uintptr_t)i;
	else
		for (i = 0; i < (count * esize / sizeof(uint32_t)); i++)
			((uint32_t *)obj)[i] = i;
}

static int
test_ring_mem_cmp(void *src, void *dst, unsigned int size)
{
	int ret;

	ret = memcmp(src, dst, size);
	if (ret) {
		rte_hexdump(stdout, "src", src, size);
		rte_hexdump(stdout, "dst", dst, size);
		printf("data after dequeue is not the same\n");
	}

	return ret;
}

static void
test_ring_print_test_string(const char *istr, unsigned int api_type, int esize)
{
	printf("\n%s: ", istr);

	if (esize == -1)
		printf("legacy APIs: ");
	else
		printf("elem APIs: element size %dB ", esize);

	if (api_type == TEST_RING_IGNORE_API_TYPE)
		return;

	if (api_type & TEST_RING_THREAD_DEF)
		printf(": default enqueue/dequeue: ");
	else if (api_type & TEST_RING_THREAD_SPSC)
		printf(": SP/SC: ");
	else if (api_type & TEST_RING_THREAD_MPMC)
		printf(": MP/MC: ");

	if (api_type & TEST_RING_ELEM_SINGLE)
		printf("single\n");
	else if (api_type & TEST_RING_ELEM_BULK)
		printf("bulk\n");
	else if (api_type & TEST_RING_ELEM_BURST)
		printf("burst\n");
}

/*
 * Various negative test cases.
 */
static int
test_ring_negative_tests(void)
{
	struct rte_ring *rp = NULL;
	struct rte_ring *rt = NULL;
	unsigned int i;

	/* Test with esize not a multiple of 4 */
	rp = test_ring_create("test_bad_element_size", 23,
				RING_SIZE + 1, SOCKET_ID_ANY, 0);
	if (rp != NULL) {
		printf("Test failed to detect invalid element size\n");
		goto test_fail;
	}


	for (i = 0; i < RTE_DIM(esize); i++) {
		/* Test if ring size is not power of 2 */
		rp = test_ring_create("test_bad_ring_size", esize[i],
					RING_SIZE + 1, SOCKET_ID_ANY, 0);
		if (rp != NULL) {
			printf("Test failed to detect odd count\n");
			goto test_fail;
		}

		/* Test if ring size is exceeding the limit */
		rp = test_ring_create("test_bad_ring_size", esize[i],
					RTE_RING_SZ_MASK + 1, SOCKET_ID_ANY, 0);
		if (rp != NULL) {
			printf("Test failed to detect limits\n");
			goto test_fail;
		}

		/* Tests if lookup returns NULL on non-existing ring */
		rp = rte_ring_lookup("ring_not_found");
		if (rp != NULL && rte_errno != ENOENT) {
			printf("Test failed to detect NULL ring lookup\n");
			goto test_fail;
		}

		/* Test to if a non-power of 2 count causes the create
		 * function to fail correctly
		 */
		rp = test_ring_create("test_ring_count", esize[i], 4097,
					SOCKET_ID_ANY, 0);
		if (rp != NULL)
			goto test_fail;

		rp = test_ring_create("test_ring_negative", esize[i], RING_SIZE,
					SOCKET_ID_ANY,
					RING_F_SP_ENQ | RING_F_SC_DEQ);
		if (rp == NULL) {
			printf("test_ring_negative fail to create ring\n");
			goto test_fail;
		}

		TEST_RING_VERIFY(rte_ring_lookup("test_ring_negative") == rp,
					rp, goto test_fail);

		TEST_RING_VERIFY(rte_ring_empty(rp) == 1, rp, goto test_fail);

		/* Tests if it would always fail to create ring with an used
		 * ring name.
		 */
		rt = test_ring_create("test_ring_negative", esize[i], RING_SIZE,
					SOCKET_ID_ANY, 0);
		if (rt != NULL)
			goto test_fail;

		rte_ring_free(rp);
		rp = NULL;
	}

	return 0;

test_fail:

	rte_ring_free(rp);
	return -1;
}

/*
 * Burst and bulk operations with sp/sc, mp/mc and default (during creation)
 * Random number of elements are enqueued and dequeued.
 */
static int
test_ring_burst_bulk_tests1(unsigned int test_idx)
{
	struct rte_ring *r;
	void **src = NULL, **cur_src = NULL, **dst = NULL, **cur_dst = NULL;
	int ret;
	unsigned int i, j, temp_sz;
	int rand;
	const unsigned int rsz = RING_SIZE - 1;

	for (i = 0; i < RTE_DIM(esize); i++) {
		test_ring_print_test_string(test_enqdeq_impl[test_idx].desc,
			test_enqdeq_impl[test_idx].api_type, esize[i]);

		/* Create the ring */
		r = test_ring_create("test_ring_burst_bulk_tests", esize[i],
				RING_SIZE, SOCKET_ID_ANY,
				test_enqdeq_impl[test_idx].create_flags);

		/* alloc dummy object pointers */
		src = test_ring_calloc(RING_SIZE * 2, esize[i]);
		if (src == NULL)
			goto fail;
		test_ring_mem_init(src, RING_SIZE * 2, esize[i]);
		cur_src = src;

		/* alloc some room for copied objects */
		dst = test_ring_calloc(RING_SIZE * 2, esize[i]);
		if (dst == NULL)
			goto fail;
		cur_dst = dst;

		printf("Random full/empty test\n");

		for (j = 0; j != TEST_RING_FULL_EMPTY_ITER; j++) {
			/* random shift in the ring */
			rand = RTE_MAX(rte_rand() % RING_SIZE, 1UL);
			printf("%s: iteration %u, random shift: %u;\n",
			    __func__, i, rand);
			ret = test_ring_enq_impl(r, cur_src, esize[i], rand,
							test_idx);
			TEST_RING_VERIFY(ret != 0, r, goto fail);

			ret = test_ring_deq_impl(r, cur_dst, esize[i], rand,
							test_idx);
			TEST_RING_VERIFY(ret == rand, r, goto fail);

			/* fill the ring */
			ret = test_ring_enq_impl(r, cur_src, esize[i], rsz,
							test_idx);
			TEST_RING_VERIFY(ret != 0, r, goto fail);

			TEST_RING_VERIFY(rte_ring_free_count(r) == 0, r, goto fail);
			TEST_RING_VERIFY(rsz == rte_ring_count(r), r, goto fail);
			TEST_RING_VERIFY(rte_ring_full(r), r, goto fail);
			TEST_RING_VERIFY(rte_ring_empty(r) == 0, r, goto fail);

			/* empty the ring */
			ret = test_ring_deq_impl(r, cur_dst, esize[i], rsz,
							test_idx);
			TEST_RING_VERIFY(ret == (int)rsz, r, goto fail);

			TEST_RING_VERIFY(rsz == rte_ring_free_count(r), r, goto fail);
			TEST_RING_VERIFY(rte_ring_count(r) == 0, r, goto fail);
			TEST_RING_VERIFY(rte_ring_full(r) == 0, r, goto fail);
			TEST_RING_VERIFY(rte_ring_empty(r), r, goto fail);

			/* check data */
			temp_sz = rsz * sizeof(void *);
			if (esize[i] != -1)
				temp_sz = rsz * esize[i];
			TEST_RING_VERIFY(test_ring_mem_cmp(src, dst,
						temp_sz) == 0, r, goto fail);
		}

		/* Free memory before test completed */
		rte_ring_free(r);
		rte_free(src);
		rte_free(dst);
		r = NULL;
		src = NULL;
		dst = NULL;
	}

	return 0;
fail:
	rte_ring_free(r);
	rte_free(src);
	rte_free(dst);
	return -1;
}

/*
 * Burst and bulk operations with sp/sc, mp/mc and default (during creation)
 * Sequence of simple enqueues/dequeues and validate the enqueued and
 * dequeued data.
 */
static int
test_ring_burst_bulk_tests2(unsigned int test_idx)
{
	struct rte_ring *r;
	void **src = NULL, **cur_src = NULL, **dst = NULL, **cur_dst = NULL;
	int ret;
	unsigned int i;

	for (i = 0; i < RTE_DIM(esize); i++) {
		test_ring_print_test_string(test_enqdeq_impl[test_idx].desc,
			test_enqdeq_impl[test_idx].api_type, esize[i]);

		/* Create the ring */
		r = test_ring_create("test_ring_burst_bulk_tests", esize[i],
				RING_SIZE, SOCKET_ID_ANY,
				test_enqdeq_impl[test_idx].create_flags);

		/* alloc dummy object pointers */
		src = test_ring_calloc(RING_SIZE * 2, esize[i]);
		if (src == NULL)
			goto fail;
		test_ring_mem_init(src, RING_SIZE * 2, esize[i]);
		cur_src = src;

		/* alloc some room for copied objects */
		dst = test_ring_calloc(RING_SIZE * 2, esize[i]);
		if (dst == NULL)
			goto fail;
		cur_dst = dst;

		printf("enqueue 1 obj\n");
		ret = test_ring_enq_impl(r, cur_src, esize[i], 1, test_idx);
		TEST_RING_VERIFY(ret == 1, r, goto fail);
		cur_src = test_ring_inc_ptr(cur_src, esize[i], 1);

		printf("enqueue 2 objs\n");
		ret = test_ring_enq_impl(r, cur_src, esize[i], 2, test_idx);
		TEST_RING_VERIFY(ret == 2, r, goto fail);
		cur_src = test_ring_inc_ptr(cur_src, esize[i], 2);

		printf("enqueue MAX_BULK objs\n");
		ret = test_ring_enq_impl(r, cur_src, esize[i], MAX_BULK,
						test_idx);
		TEST_RING_VERIFY(ret == MAX_BULK, r, goto fail);

		printf("dequeue 1 obj\n");
		ret = test_ring_deq_impl(r, cur_dst, esize[i], 1, test_idx);
		TEST_RING_VERIFY(ret == 1, r, goto fail);
		cur_dst = test_ring_inc_ptr(cur_dst, esize[i], 1);

		printf("dequeue 2 objs\n");
		ret = test_ring_deq_impl(r, cur_dst, esize[i], 2, test_idx);
		TEST_RING_VERIFY(ret == 2, r, goto fail);
		cur_dst = test_ring_inc_ptr(cur_dst, esize[i], 2);

		printf("dequeue MAX_BULK objs\n");
		ret = test_ring_deq_impl(r, cur_dst, esize[i], MAX_BULK,
						test_idx);
		TEST_RING_VERIFY(ret == MAX_BULK, r, goto fail);
		cur_dst = test_ring_inc_ptr(cur_dst, esize[i], MAX_BULK);

		/* check data */
		TEST_RING_VERIFY(test_ring_mem_cmp(src, dst,
					RTE_PTR_DIFF(cur_dst, dst)) == 0,
					r, goto fail);

		/* Free memory before test completed */
		rte_ring_free(r);
		rte_free(src);
		rte_free(dst);
		r = NULL;
		src = NULL;
		dst = NULL;
	}

	return 0;
fail:
	rte_ring_free(r);
	rte_free(src);
	rte_free(dst);
	return -1;
}

/*
 * Burst and bulk operations with sp/sc, mp/mc and default (during creation)
 * Enqueue and dequeue to cover the entire ring length.
 */
static int
test_ring_burst_bulk_tests3(unsigned int test_idx)
{
	struct rte_ring *r;
	void **src = NULL, **cur_src = NULL, **dst = NULL, **cur_dst = NULL;
	int ret;
	unsigned int i, j;

	for (i = 0; i < RTE_DIM(esize); i++) {
		test_ring_print_test_string(test_enqdeq_impl[test_idx].desc,
			test_enqdeq_impl[test_idx].api_type, esize[i]);

		/* Create the ring */
		r = test_ring_create("test_ring_burst_bulk_tests", esize[i],
				RING_SIZE, SOCKET_ID_ANY,
				test_enqdeq_impl[test_idx].create_flags);

		/* alloc dummy object pointers */
		src = test_ring_calloc(RING_SIZE * 2, esize[i]);
		if (src == NULL)
			goto fail;
		test_ring_mem_init(src, RING_SIZE * 2, esize[i]);
		cur_src = src;

		/* alloc some room for copied objects */
		dst = test_ring_calloc(RING_SIZE * 2, esize[i]);
		if (dst == NULL)
			goto fail;
		cur_dst = dst;

		printf("fill and empty the ring\n");
		for (j = 0; j < RING_SIZE / MAX_BULK; j++) {
			ret = test_ring_enq_impl(r, cur_src, esize[i], MAX_BULK,
							test_idx);
			TEST_RING_VERIFY(ret == MAX_BULK, r, goto fail);
			cur_src = test_ring_inc_ptr(cur_src, esize[i],
								MAX_BULK);

			ret = test_ring_deq_impl(r, cur_dst, esize[i], MAX_BULK,
							test_idx);
			TEST_RING_VERIFY(ret == MAX_BULK, r, goto fail);
			cur_dst = test_ring_inc_ptr(cur_dst, esize[i],
								MAX_BULK);
		}

		/* check data */
		TEST_RING_VERIFY(test_ring_mem_cmp(src, dst,
					RTE_PTR_DIFF(cur_dst, dst)) == 0,
					r, goto fail);

		/* Free memory before test completed */
		rte_ring_free(r);
		rte_free(src);
		rte_free(dst);
		r = NULL;
		src = NULL;
		dst = NULL;
	}

	return 0;
fail:
	rte_ring_free(r);
	rte_free(src);
	rte_free(dst);
	return -1;
}

/*
 * Burst and bulk operations with sp/sc, mp/mc and default (during creation)
 * Enqueue till the ring is full and dequeue till the ring becomes empty.
 */
static int
test_ring_burst_bulk_tests4(unsigned int test_idx)
{
	struct rte_ring *r;
	void **src = NULL, **cur_src = NULL, **dst = NULL, **cur_dst = NULL;
	int ret;
	unsigned int i, j;
	unsigned int api_type, num_elems;

	api_type = test_enqdeq_impl[test_idx].api_type;

	for (i = 0; i < RTE_DIM(esize); i++) {
		test_ring_print_test_string(test_enqdeq_impl[test_idx].desc,
			test_enqdeq_impl[test_idx].api_type, esize[i]);

		/* Create the ring */
		r = test_ring_create("test_ring_burst_bulk_tests", esize[i],
				RING_SIZE, SOCKET_ID_ANY,
				test_enqdeq_impl[test_idx].create_flags);

		/* alloc dummy object pointers */
		src = test_ring_calloc(RING_SIZE * 2, esize[i]);
		if (src == NULL)
			goto fail;
		test_ring_mem_init(src, RING_SIZE * 2, esize[i]);
		cur_src = src;

		/* alloc some room for copied objects */
		dst = test_ring_calloc(RING_SIZE * 2, esize[i]);
		if (dst == NULL)
			goto fail;
		cur_dst = dst;

		printf("Test enqueue without enough memory space\n");
		for (j = 0; j < (RING_SIZE/MAX_BULK - 1); j++) {
			ret = test_ring_enq_impl(r, cur_src, esize[i], MAX_BULK,
							test_idx);
			TEST_RING_VERIFY(ret == MAX_BULK, r, goto fail);
			cur_src = test_ring_inc_ptr(cur_src, esize[i],
								MAX_BULK);
		}

		printf("Enqueue 2 objects, free entries = MAX_BULK - 2\n");
		ret = test_ring_enq_impl(r, cur_src, esize[i], 2, test_idx);
		TEST_RING_VERIFY(ret == 2, r, goto fail);
		cur_src = test_ring_inc_ptr(cur_src, esize[i], 2);

		printf("Enqueue the remaining entries = MAX_BULK - 3\n");
		/* Bulk APIs enqueue exact number of elements */
		if ((api_type & TEST_RING_ELEM_BULK) == TEST_RING_ELEM_BULK)
			num_elems = MAX_BULK - 3;
		else
			num_elems = MAX_BULK;
		/* Always one free entry left */
		ret = test_ring_enq_impl(r, cur_src, esize[i], num_elems,
						test_idx);
		TEST_RING_VERIFY(ret == MAX_BULK - 3, r, goto fail);
		cur_src = test_ring_inc_ptr(cur_src, esize[i], MAX_BULK - 3);

		printf("Test if ring is full\n");
		TEST_RING_VERIFY(rte_ring_full(r) == 1, r, goto fail);

		printf("Test enqueue for a full entry\n");
		ret = test_ring_enq_impl(r, cur_src, esize[i], MAX_BULK,
						test_idx);
		TEST_RING_VERIFY(ret == 0, r, goto fail);

		printf("Test dequeue without enough objects\n");
		for (j = 0; j < RING_SIZE / MAX_BULK - 1; j++) {
			ret = test_ring_deq_impl(r, cur_dst, esize[i], MAX_BULK,
							test_idx);
			TEST_RING_VERIFY(ret == MAX_BULK, r, goto fail);
			cur_dst = test_ring_inc_ptr(cur_dst, esize[i],
								MAX_BULK);
		}

		/* Available memory space for the exact MAX_BULK entries */
		ret = test_ring_deq_impl(r, cur_dst, esize[i], 2, test_idx);
		TEST_RING_VERIFY(ret == 2, r, goto fail);
		cur_dst = test_ring_inc_ptr(cur_dst, esize[i], 2);

		/* Bulk APIs enqueue exact number of elements */
		if ((api_type & TEST_RING_ELEM_BULK) == TEST_RING_ELEM_BULK)
			num_elems = MAX_BULK - 3;
		else
			num_elems = MAX_BULK;
		ret = test_ring_deq_impl(r, cur_dst, esize[i], num_elems,
						test_idx);
		TEST_RING_VERIFY(ret == MAX_BULK - 3, r, goto fail);
		cur_dst = test_ring_inc_ptr(cur_dst, esize[i], MAX_BULK - 3);

		printf("Test if ring is empty\n");
		/* Check if ring is empty */
		TEST_RING_VERIFY(rte_ring_empty(r) == 1, r, goto fail);

		/* check data */
		TEST_RING_VERIFY(test_ring_mem_cmp(src, dst,
					RTE_PTR_DIFF(cur_dst, dst)) == 0,
					r, goto fail);

		/* Free memory before test completed */
		rte_ring_free(r);
		rte_free(src);
		rte_free(dst);
		r = NULL;
		src = NULL;
		dst = NULL;
	}

	return 0;
fail:
	rte_ring_free(r);
	rte_free(src);
	rte_free(dst);
	return -1;
}

/*
 * Test default, single element, bulk and burst APIs
 */
static int
test_ring_basic_ex(void)
{
	int ret = -1;
	unsigned int i, j;
	struct rte_ring *rp = NULL;
	void **src = NULL, **cur_src = NULL, **dst = NULL, **cur_dst = NULL;

	for (i = 0; i < RTE_DIM(esize); i++) {
		rp = test_ring_create("test_ring_basic_ex", esize[i], RING_SIZE,
					SOCKET_ID_ANY,
					RING_F_SP_ENQ | RING_F_SC_DEQ);
		if (rp == NULL) {
			printf("%s: failed to create ring\n", __func__);
			goto fail_test;
		}

		/* alloc dummy object pointers */
		src = test_ring_calloc(RING_SIZE, esize[i]);
		if (src == NULL) {
			printf("%s: failed to alloc src memory\n", __func__);
			goto fail_test;
		}
		test_ring_mem_init(src, RING_SIZE, esize[i]);
		cur_src = src;

		/* alloc some room for copied objects */
		dst = test_ring_calloc(RING_SIZE, esize[i]);
		if (dst == NULL) {
			printf("%s: failed to alloc dst memory\n", __func__);
			goto fail_test;
		}
		cur_dst = dst;

		TEST_RING_VERIFY(rte_ring_lookup("test_ring_basic_ex") == rp,
					rp, goto fail_test);

		TEST_RING_VERIFY(rte_ring_empty(rp) == 1, rp, goto fail_test);

		printf("%u ring entries are now free\n",
			rte_ring_free_count(rp));

		for (j = 0; j < RING_SIZE - 1; j++) {
			ret = test_ring_enqueue(rp, cur_src, esize[i], 1,
				TEST_RING_THREAD_DEF | TEST_RING_ELEM_SINGLE);
			TEST_RING_VERIFY(ret == 0, rp, goto fail_test);
			cur_src = test_ring_inc_ptr(cur_src, esize[i], 1);
		}

		TEST_RING_VERIFY(rte_ring_full(rp) == 1, rp, goto fail_test);

		for (j = 0; j < RING_SIZE - 1; j++) {
			ret = test_ring_dequeue(rp, cur_dst, esize[i], 1,
				TEST_RING_THREAD_DEF | TEST_RING_ELEM_SINGLE);
			TEST_RING_VERIFY(ret == 0, rp, goto fail_test);
			cur_dst = test_ring_inc_ptr(cur_dst, esize[i], 1);
		}

		TEST_RING_VERIFY(rte_ring_empty(rp) == 1, rp, goto fail_test);

		/* check data */
		TEST_RING_VERIFY(test_ring_mem_cmp(src, dst,
					RTE_PTR_DIFF(cur_dst, dst)) == 0,
					rp, goto fail_test);

		/* Following tests use the configured flags to decide
		 * SP/SC or MP/MC.
		 */
		/* reset memory of dst */
		memset(dst, 0, RTE_PTR_DIFF(cur_dst, dst));

		/* reset cur_src and cur_dst */
		cur_src = src;
		cur_dst = dst;

		/* Covering the ring burst operation */
		ret = test_ring_enqueue(rp, cur_src, esize[i], 2,
				TEST_RING_THREAD_DEF | TEST_RING_ELEM_BURST);
		TEST_RING_VERIFY(ret == 2, rp, goto fail_test);
		cur_src = test_ring_inc_ptr(cur_src, esize[i], 2);

		ret = test_ring_dequeue(rp, cur_dst, esize[i], 2,
				TEST_RING_THREAD_DEF | TEST_RING_ELEM_BURST);
		TEST_RING_VERIFY(ret == 2, rp, goto fail_test);
		cur_dst = test_ring_inc_ptr(cur_dst, esize[i], 2);

		/* Covering the ring bulk operation */
		ret = test_ring_enqueue(rp, cur_src, esize[i], 2,
				TEST_RING_THREAD_DEF | TEST_RING_ELEM_BULK);
		TEST_RING_VERIFY(ret == 2, rp, goto fail_test);

		ret = test_ring_dequeue(rp, cur_dst, esize[i], 2,
				TEST_RING_THREAD_DEF | TEST_RING_ELEM_BULK);
		TEST_RING_VERIFY(ret == 2, rp, goto fail_test);
		cur_dst = test_ring_inc_ptr(cur_dst, esize[i], 2);

		/* check data */
		TEST_RING_VERIFY(test_ring_mem_cmp(src, dst,
					RTE_PTR_DIFF(cur_dst, dst)) == 0,
					rp, goto fail_test);

		rte_ring_free(rp);
		rte_free(src);
		rte_free(dst);
		rp = NULL;
		src = NULL;
		dst = NULL;
	}

	return 0;

fail_test:
	rte_ring_free(rp);
	rte_free(src);
	rte_free(dst);
	return -1;
}

/*
 * Basic test cases with exact size ring.
 */
static int
test_ring_with_exact_size(void)
{
	struct rte_ring *std_r = NULL, *exact_sz_r = NULL;
	void **src_orig = NULL, **dst_orig = NULL;
	void **src = NULL, **cur_src = NULL, **dst = NULL, **cur_dst = NULL;
	const unsigned int ring_sz = 16;
	unsigned int i, j;
	int ret = -1;

	for (i = 0; i < RTE_DIM(esize); i++) {
		test_ring_print_test_string("Test exact size ring",
				TEST_RING_IGNORE_API_TYPE,
				esize[i]);

		std_r = test_ring_create("std", esize[i], ring_sz,
					rte_socket_id(),
					RING_F_SP_ENQ | RING_F_SC_DEQ);
		if (std_r == NULL) {
			printf("%s: error, can't create std ring\n", __func__);
			goto test_fail;
		}
		exact_sz_r = test_ring_create("exact sz", esize[i], ring_sz,
				rte_socket_id(),
				RING_F_SP_ENQ | RING_F_SC_DEQ |
				RING_F_EXACT_SZ);
		if (exact_sz_r == NULL) {
			printf("%s: error, can't create exact size ring\n",
					__func__);
			goto test_fail;
		}

		/* alloc object pointers. Allocate one extra object
		 * and create an unaligned address.
		 */
		src_orig = test_ring_calloc(17, esize[i]);
		if (src_orig == NULL)
			goto test_fail;
		test_ring_mem_init(src_orig, 17, esize[i]);
		src = (void **)((uintptr_t)src_orig + 1);
		cur_src = src;

		dst_orig = test_ring_calloc(17, esize[i]);
		if (dst_orig == NULL)
			goto test_fail;
		dst = (void **)((uintptr_t)dst_orig + 1);
		cur_dst = dst;

		/*
		 * Check that the exact size ring is bigger than the
		 * standard ring
		 */
		TEST_RING_VERIFY(rte_ring_get_size(std_r) <=
				rte_ring_get_size(exact_sz_r),
				std_r, goto test_fail);

		/*
		 * check that the exact_sz_ring can hold one more element
		 * than the standard ring. (16 vs 15 elements)
		 */
		for (j = 0; j < ring_sz - 1; j++) {
			ret = test_ring_enqueue(std_r, cur_src, esize[i], 1,
				TEST_RING_THREAD_DEF | TEST_RING_ELEM_SINGLE);
			TEST_RING_VERIFY(ret == 0, std_r, goto test_fail);
			ret = test_ring_enqueue(exact_sz_r, cur_src, esize[i], 1,
				TEST_RING_THREAD_DEF | TEST_RING_ELEM_SINGLE);
			TEST_RING_VERIFY(ret == 0, exact_sz_r, goto test_fail);
			cur_src = test_ring_inc_ptr(cur_src, esize[i], 1);
		}
		ret = test_ring_enqueue(std_r, cur_src, esize[i], 1,
				TEST_RING_THREAD_DEF | TEST_RING_ELEM_SINGLE);
		TEST_RING_VERIFY(ret == -ENOBUFS, std_r, goto test_fail);
		ret = test_ring_enqueue(exact_sz_r, cur_src, esize[i], 1,
				TEST_RING_THREAD_DEF | TEST_RING_ELEM_SINGLE);
		TEST_RING_VERIFY(ret != -ENOBUFS, exact_sz_r, goto test_fail);

		/* check that dequeue returns the expected number of elements */
		ret = test_ring_dequeue(exact_sz_r, cur_dst, esize[i], ring_sz,
				TEST_RING_THREAD_DEF | TEST_RING_ELEM_BURST);
		TEST_RING_VERIFY(ret == (int)ring_sz, exact_sz_r, goto test_fail);
		cur_dst = test_ring_inc_ptr(cur_dst, esize[i], ring_sz);

		/* check that the capacity function returns expected value */
		TEST_RING_VERIFY(rte_ring_get_capacity(exact_sz_r) == ring_sz,
					exact_sz_r, goto test_fail);

		/* check data */
		TEST_RING_VERIFY(test_ring_mem_cmp(src, dst,
					RTE_PTR_DIFF(cur_dst, dst)) == 0,
					exact_sz_r, goto test_fail);

		rte_free(src_orig);
		rte_free(dst_orig);
		rte_ring_free(std_r);
		rte_ring_free(exact_sz_r);
		src_orig = NULL;
		dst_orig = NULL;
		std_r = NULL;
		exact_sz_r = NULL;
	}

	return 0;

test_fail:
	rte_free(src_orig);
	rte_free(dst_orig);
	rte_ring_free(std_r);
	rte_ring_free(exact_sz_r);
	return -1;
}

static int
test_ring(void)
{
	int32_t rc;
	unsigned int i;

	/* Negative test cases */
	if (test_ring_negative_tests() < 0)
		goto test_fail;

	/* Some basic operations */
	if (test_ring_basic_ex() < 0)
		goto test_fail;

	if (test_ring_with_exact_size() < 0)
		goto test_fail;

	/* Burst and bulk operations with sp/sc, mp/mc and default.
	 * The test cases are split into smaller test cases to
	 * help clang compile faster.
	 */
	for (i = 0; i != RTE_DIM(test_enqdeq_impl); i++) {


		rc = test_ring_burst_bulk_tests1(i);
		if (rc < 0)
			goto test_fail;

		rc = test_ring_burst_bulk_tests2(i);
		if (rc < 0)
			goto test_fail;

		rc = test_ring_burst_bulk_tests3(i);
		if (rc < 0)
			goto test_fail;

		rc = test_ring_burst_bulk_tests4(i);
		if (rc < 0)
			goto test_fail;
	}

	/* dump the ring status */
	rte_ring_list_dump(stdout);

	return 0;

test_fail:

	return -1;
}

REGISTER_FAST_TEST(ring_autotest, true, true, test_ring);
