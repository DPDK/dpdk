/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Huawei Technologies Co., Ltd
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
#include <rte_random.h>
#include <rte_errno.h>
#include <rte_hexdump.h>

#include <rte_soring.h>

#include "test.h"

#define MAX_ACQUIRED 20

#define SORING_TEST_ASSERT(val, expected) do { \
	RTE_TEST_ASSERT(expected == val, \
			"%s: expected %u got %u\n", #val, expected, val); \
} while (0)

static void
set_soring_init_param(struct rte_soring_param *prm,
		const char *name, uint32_t esize, uint32_t elems,
		uint32_t stages, uint32_t stsize,
		enum rte_ring_sync_type rst_prod,
		enum rte_ring_sync_type rst_cons)
{
	prm->name = name;
	prm->elem_size = esize;
	prm->elems = elems;
	prm->stages = stages;
	prm->meta_size = stsize;
	prm->prod_synt = rst_prod;
	prm->cons_synt = rst_cons;
}

static int
move_forward_stage(struct rte_soring *sor,
		uint32_t num_packets, uint32_t stage)
{
	uint32_t acquired;
	uint32_t ftoken;
	uint32_t *acquired_objs[MAX_ACQUIRED];

	acquired = rte_soring_acquire_bulk(sor, acquired_objs, stage,
			num_packets, &ftoken, NULL);
	SORING_TEST_ASSERT(acquired, num_packets);
	rte_soring_release(sor, NULL, stage, num_packets,
			ftoken);

	return 0;
}

/*
 * struct rte_soring_param param checking.
 */
static int
test_soring_init(void)
{
	struct rte_soring *sor = NULL;
	struct rte_soring_param prm;
	int rc;
	size_t sz;
	memset(&prm, 0, sizeof(prm));

	/* init memory */
	set_soring_init_param(&prm, "alloc_memory", sizeof(uintptr_t),
			4, 1, 4, RTE_RING_SYNC_MT, RTE_RING_SYNC_MT);
	sz = rte_soring_get_memsize(&prm);
	sor = rte_zmalloc(NULL, sz, RTE_CACHE_LINE_SIZE);
	RTE_TEST_ASSERT_NOT_NULL(sor, "could not allocate memory for soring");

	set_soring_init_param(&prm, "test_invalid_stages", sizeof(uintptr_t),
			4, 0, 4, RTE_RING_SYNC_MT, RTE_RING_SYNC_MT);
	rc = rte_soring_init(sor, &prm);
	RTE_TEST_ASSERT_FAIL(rc, "initted soring with invalid num stages");

	set_soring_init_param(&prm, "test_invalid_esize", 0,
			4, 1, 4, RTE_RING_SYNC_MT, RTE_RING_SYNC_MT);
	rc = rte_soring_init(sor, &prm);
	RTE_TEST_ASSERT_FAIL(rc, "initted soring with 0 esize");

	set_soring_init_param(&prm, "test_invalid_esize", 9,
			4, 1, 4, RTE_RING_SYNC_MT, RTE_RING_SYNC_MT);
	rc = rte_soring_init(sor, &prm);
	RTE_TEST_ASSERT_FAIL(rc, "initted soring with esize not multiple of 4");

	set_soring_init_param(&prm, "test_invalid_rsize", sizeof(uintptr_t),
			4, 1, 3, RTE_RING_SYNC_MT, RTE_RING_SYNC_MT);
	rc = rte_soring_init(sor, &prm);
	RTE_TEST_ASSERT_FAIL(rc, "initted soring with rcsize not multiple of 4");

	set_soring_init_param(&prm, "test_invalid_elems", sizeof(uintptr_t),
			RTE_SORING_ELEM_MAX + 1, 1, 4, RTE_RING_SYNC_MT,
			RTE_RING_SYNC_MT);
	rc = rte_soring_init(sor, &prm);
	RTE_TEST_ASSERT_FAIL(rc, "initted soring with invalid num elements");

	rte_free(sor);
	return 0;
}

static int
test_soring_get_memsize(void)
{

	struct rte_soring_param prm;
	ssize_t sz;

	set_soring_init_param(&prm, "memsize", sizeof(uint32_t *),
			10, 1, 0, RTE_RING_SYNC_MT, RTE_RING_SYNC_MT);
	sz = rte_soring_get_memsize(&prm);
	RTE_TEST_ASSERT(sz > 0, "failed to calculate size");

	set_soring_init_param(&prm, "memsize", sizeof(uint8_t),
			16, UINT32_MAX, sizeof(uint32_t), RTE_RING_SYNC_MT, RTE_RING_SYNC_MT);
	sz = rte_soring_get_memsize(&prm);
	RTE_TEST_ASSERT_EQUAL(sz, -EINVAL, "calculated size incorrect");

	set_soring_init_param(&prm, "memsize", 0,
			16, UINT32_MAX, sizeof(uint32_t), RTE_RING_SYNC_MT, RTE_RING_SYNC_MT);
	sz = rte_soring_get_memsize(&prm);
	RTE_TEST_ASSERT_EQUAL(sz, -EINVAL, "calculated size incorrect");

	return 0;

}

static int
test_soring_stages(void)
{
	struct rte_soring *sor = NULL;
	struct rte_soring_param prm;
	uint32_t objs[32];
	uint32_t rcs[32];
	uint32_t acquired_objs[32];
	uint32_t acquired_rcs[32];
	uint32_t dequeued_rcs[32];
	uint32_t dequeued_objs[32];
	size_t ssz;
	uint32_t stage, enqueued, dequeued, acquired;
	uint32_t i, ftoken;

	memset(&prm, 0, sizeof(prm));
	set_soring_init_param(&prm, "stages", sizeof(uint32_t), 32,
			10000, sizeof(uint32_t), RTE_RING_SYNC_MT, RTE_RING_SYNC_MT);
	ssz = rte_soring_get_memsize(&prm);
	RTE_TEST_ASSERT(ssz > 0, "parameter error calculating ring size");
	sor = rte_zmalloc(NULL, ssz, RTE_CACHE_LINE_SIZE);
	RTE_TEST_ASSERT_NOT_NULL(sor, "couldn't allocate memory for soring");
	rte_soring_init(sor, &prm);

	for (i = 0; i < 32; i++) {
		rcs[i] = i;
		objs[i] = i + 32;
	}

	enqueued = rte_soring_enqueux_burst(sor, objs, rcs, 32, NULL);
	SORING_TEST_ASSERT(enqueued, 32);

	for (stage = 0; stage < 10000; stage++) {
		int j;
		dequeued = rte_soring_dequeue_bulk(sor, dequeued_objs,
				32, NULL);
		/* check none at end stage */
		SORING_TEST_ASSERT(dequeued, 0);

		acquired = rte_soring_acquirx_bulk(sor, acquired_objs,
				acquired_rcs, stage, 32, &ftoken, NULL);
		SORING_TEST_ASSERT(acquired, 32);

		for (j = 0; j < 32; j++) {
			SORING_TEST_ASSERT(acquired_rcs[j], j + stage);
			SORING_TEST_ASSERT(acquired_objs[j], j + stage + 32);
			/* modify both queue object and rc */
			acquired_objs[j]++;
			acquired_rcs[j]++;
		}

		rte_soring_releasx(sor, acquired_objs,
				acquired_rcs, stage, 32,
				ftoken);
	}

	dequeued = rte_soring_dequeux_bulk(sor, dequeued_objs, dequeued_rcs,
			32, NULL);
	SORING_TEST_ASSERT(dequeued, 32);
		for (i = 0; i < 32; i++) {
			/* ensure we ended up with the expected values in order */
			SORING_TEST_ASSERT(dequeued_rcs[i], i + 10000);
			SORING_TEST_ASSERT(dequeued_objs[i], i + 32 + 10000);
		}
	rte_free(sor);
	return 0;
}

static int
test_soring_enqueue_dequeue(void)
{
	struct rte_soring *sor = NULL;
	struct rte_soring_param prm;
	int rc;
	uint32_t i;
	size_t sz;
	uint32_t queue_objs[10];
	uint32_t *queue_objs_p[10];
	uint32_t free_space;
	uint32_t enqueued, dequeued;
	uint32_t *dequeued_objs[10];

	memset(&prm, 0, sizeof(prm));
	for (i = 0; i < 10; i++) {
		queue_objs[i] = i + 1;
		queue_objs_p[i] = &queue_objs[i];
	}

	/* init memory */
	set_soring_init_param(&prm, "enqueue/dequeue", sizeof(uint32_t *),
			10, 1, 0, RTE_RING_SYNC_MT, RTE_RING_SYNC_MT);
	sz = rte_soring_get_memsize(&prm);
	sor = rte_zmalloc(NULL, sz, RTE_CACHE_LINE_SIZE);
	RTE_TEST_ASSERT_NOT_NULL(sor, "alloc failed for soring");
	rc = rte_soring_init(sor, &prm);
	RTE_TEST_ASSERT_SUCCESS(rc, "Failed to init soring");

	free_space = 0;

	enqueued = rte_soring_enqueue_burst(sor, queue_objs_p, 5, &free_space);

	SORING_TEST_ASSERT(free_space, 5);
	SORING_TEST_ASSERT(enqueued, 5);

	/* fixed amount enqueue */
	enqueued = rte_soring_enqueue_bulk(sor, queue_objs_p, 7, &free_space);

	SORING_TEST_ASSERT(free_space, 5);
	SORING_TEST_ASSERT(enqueued, 0);

	/* variable amount enqueue */
	enqueued = rte_soring_enqueue_burst(sor, queue_objs_p + 5, 7,
				&free_space);
	SORING_TEST_ASSERT(free_space, 0);
	SORING_TEST_ASSERT(enqueued, 5);

	/* test no dequeue while stage 0 has not completed */
	dequeued = rte_soring_dequeue_bulk(sor, dequeued_objs, 10, NULL);
	SORING_TEST_ASSERT(dequeued, 0);
	dequeued = rte_soring_dequeue_burst(sor, dequeued_objs, 10, NULL);
	SORING_TEST_ASSERT(dequeued, 0);

	move_forward_stage(sor, 8, 0);

	/* can only dequeue as many as have completed stage */
	dequeued = rte_soring_dequeue_bulk(sor, dequeued_objs, 10, NULL);
	SORING_TEST_ASSERT(dequeued, 0);
	dequeued = rte_soring_dequeue_burst(sor, dequeued_objs, 10, NULL);
	SORING_TEST_ASSERT(dequeued, 8);
	/* packets remain in order */
	for (i = 0; i < dequeued; i++) {
		RTE_TEST_ASSERT_EQUAL(dequeued_objs[i],
				queue_objs_p[i], "dequeued != enqueued");
	}

	dequeued = rte_soring_dequeue_burst(sor, dequeued_objs, 1, NULL);
	SORING_TEST_ASSERT(dequeued, 0);

	move_forward_stage(sor, 2, 0);
	dequeued = rte_soring_dequeue_burst(sor, dequeued_objs, 2, NULL);
	SORING_TEST_ASSERT(dequeued, 2);
	/* packets remain in order */
	for (i = 0; i < dequeued; i++) {
		RTE_TEST_ASSERT_EQUAL(dequeued_objs[i],
				queue_objs_p[i + 8], "dequeued != enqueued");
	}

	rte_soring_dump(stdout, sor);
	rte_free(sor);
	return 0;
}

static int
test_soring_acquire_release(void)
{

	struct rte_soring *sor = NULL;
	struct rte_soring_param prm;
	int rc, i;
	size_t sz;

	memset(&prm, 0, sizeof(prm));
	uint32_t queue_objs[10];
	uint32_t rc_objs[10];
	uint32_t acquired_objs[10];
	uint32_t dequeued_objs[10];
	uint32_t dequeued_rcs[10];
	uint32_t s1_acquired_rcs[10];
	uint32_t free_space, enqueued, ftoken, acquired, dequeued;

	for (i = 0; i < 10; i++) {
		queue_objs[i] = i + 5;
		rc_objs[i] = i + 10;
	}

/*init memory*/
	set_soring_init_param(&prm, "test_acquire_release", sizeof(uint32_t),
			20, 2, sizeof(uint32_t), RTE_RING_SYNC_MT, RTE_RING_SYNC_MT);
	sz = rte_soring_get_memsize(&prm);
	sor = rte_zmalloc(NULL, sz, RTE_CACHE_LINE_SIZE);
	if (sor == NULL) {
		printf("%s: alloc(%zu) for FIFO with %u elems failed",
			__func__, sz, prm.elems);
		return -ENOMEM;
	}

	/* init ring */
	rc = rte_soring_init(sor, &prm);
	RTE_TEST_ASSERT_SUCCESS(rc, "failed to init soring");

	/* enqueue with associated rc */
	enqueued = rte_soring_enqueux_burst(sor, queue_objs, rc_objs, 5,
			&free_space);
	SORING_TEST_ASSERT(enqueued, 5);
	/* enqueue without associated rc */
	enqueued = rte_soring_enqueue_burst(sor, queue_objs + 5, 5,
			&free_space);
	SORING_TEST_ASSERT(enqueued, 5);

	/* acquire the objects with rc's and ensure they are as expected */
	acquired = rte_soring_acquirx_burst(sor, acquired_objs,
			s1_acquired_rcs, 0, 5, &ftoken, NULL);
	SORING_TEST_ASSERT(acquired, 5);
	for (i = 0; i < 5; i++) {
		RTE_TEST_ASSERT_EQUAL(s1_acquired_rcs[i], rc_objs[i],
				"acquired rc[%d]: %u != enqueued rc: %u",
				i, s1_acquired_rcs[i], rc_objs[i]);
		RTE_TEST_ASSERT_EQUAL(acquired_objs[i], queue_objs[i],
				"acquired obj[%d]: %u != enqueued obj %u",
				i, acquired_objs[i], queue_objs[i]);
	}
	rte_soring_release(sor, NULL, 0, 5, ftoken);

	/* acquire the objects without rc's and ensure they are as expected */
	acquired = rte_soring_acquirx_burst(sor, acquired_objs,
			s1_acquired_rcs, 0, 5, &ftoken, NULL);
	SORING_TEST_ASSERT(acquired, 5);
	for (i = 0; i < 5; i++) {
		/* as the rc area of memory is zero'd at init this is true
		 * but this is a detail of implementation rather than
		 * a guarantee.
		 */
		RTE_TEST_ASSERT_EQUAL(s1_acquired_rcs[i], 0,
				"acquired rc not empty");
		RTE_TEST_ASSERT_EQUAL(acquired_objs[i], queue_objs[i + 5],
				"acquired obj[%d]: %u != enqueued obj %u",
				i, acquired_objs[i], queue_objs[i + 5]);
	}
	/*release the objects, adding rc's */
	rte_soring_releasx(sor, NULL, rc_objs + 5, 0, 5,
			ftoken);

	acquired = rte_soring_acquirx_burst(sor, acquired_objs,
			s1_acquired_rcs, 1, 10, &ftoken, NULL);
	SORING_TEST_ASSERT(acquired, 10);
	for (i = 0; i < 10; i++) {
		/* ensure the associated rc's are the ones added at release */
		RTE_TEST_ASSERT_EQUAL(s1_acquired_rcs[i], rc_objs[i],
				"acquired rc[%d]: %u != enqueued rc: %u",
				i, s1_acquired_rcs[i], rc_objs[i]);
		RTE_TEST_ASSERT_EQUAL(acquired_objs[i], queue_objs[i],
				"acquired obj[%d]: %u != enqueued obj %u",
				i, acquired_objs[i], queue_objs[i]);
	}
	/* release the objects, with rc's set to NULL */
	rte_soring_release(sor, NULL, 1, 10, ftoken);

	dequeued = rte_soring_dequeux_burst(sor, dequeued_objs, dequeued_rcs,
			10, NULL);
	SORING_TEST_ASSERT(dequeued, 10);
	for (i = 0; i < 10; i++) {
		/* ensure the associated rc's are the ones added at release */
		RTE_TEST_ASSERT_EQUAL(dequeued_rcs[i], rc_objs[i],
				"dequeued rc[%d]: %u != enqueued rc: %u",
				i, dequeued_rcs[i], rc_objs[i]);
		RTE_TEST_ASSERT_EQUAL(acquired_objs[i], queue_objs[i],
				"acquired obj[%d]: %u != enqueued obj %u",
				i, acquired_objs[i], queue_objs[i]);
	}
	rte_soring_dump(stdout, sor);
	rte_free(sor);
	return 0;
}

static int
test_soring(void)
{

	/* Negative test cases */
	if (test_soring_init() < 0)
		goto test_fail;

	/* Memory calculations */
	if (test_soring_get_memsize() < 0)
		goto test_fail;

	/* Basic enqueue/dequeue operations */
	if (test_soring_enqueue_dequeue() < 0)
		goto test_fail;

	/* Acquire/release */
	if (test_soring_acquire_release() < 0)
		goto test_fail;

	/* Test large number of stages */
	if (test_soring_stages() < 0)
		goto test_fail;

	return 0;

test_fail:
	return -1;
}

REGISTER_FAST_TEST(soring_autotest, NOHUGE_OK, ASAN_OK, test_soring);
