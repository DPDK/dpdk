/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include <unistd.h>
#include <inttypes.h>
#include <rte_mbuf.h>
#include "rte_rawdev.h"
#include "rte_ioat_rawdev.h"
#include "ioat_private.h"

#define MAX_SUPPORTED_RAWDEVS 64
#define TEST_SKIPPED 77
#define COPY_LEN 1024

int ioat_rawdev_test(uint16_t dev_id); /* pre-define to keep compiler happy */

static struct rte_mempool *pool;
static unsigned short expected_ring_size[MAX_SUPPORTED_RAWDEVS];

#define PRINT_ERR(...) print_err(__func__, __LINE__, __VA_ARGS__)

static inline int
__rte_format_printf(3, 4)
print_err(const char *func, int lineno, const char *format, ...)
{
	va_list ap;
	int ret;

	ret = fprintf(stderr, "In %s:%d - ", func, lineno);
	va_start(ap, format);
	ret += vfprintf(stderr, format, ap);
	va_end(ap);

	return ret;
}

static int
do_multi_copies(int dev_id, int split_batches, int split_completions)
{
	struct rte_mbuf *srcs[32], *dsts[32];
	struct rte_mbuf *completed_src[64];
	struct rte_mbuf *completed_dst[64];
	unsigned int i, j;

	for (i = 0; i < RTE_DIM(srcs); i++) {
		char *src_data;

		if (split_batches && i == RTE_DIM(srcs) / 2)
			rte_ioat_perform_ops(dev_id);

		srcs[i] = rte_pktmbuf_alloc(pool);
		dsts[i] = rte_pktmbuf_alloc(pool);
		src_data = rte_pktmbuf_mtod(srcs[i], char *);

		for (j = 0; j < COPY_LEN; j++)
			src_data[j] = rand() & 0xFF;

		if (rte_ioat_enqueue_copy(dev_id,
				srcs[i]->buf_iova + srcs[i]->data_off,
				dsts[i]->buf_iova + dsts[i]->data_off,
				COPY_LEN,
				(uintptr_t)srcs[i],
				(uintptr_t)dsts[i]) != 1) {
			PRINT_ERR("Error with rte_ioat_enqueue_copy for buffer %u\n",
					i);
			return -1;
		}
	}
	rte_ioat_perform_ops(dev_id);
	usleep(100);

	if (split_completions) {
		/* gather completions in two halves */
		uint16_t half_len = RTE_DIM(srcs) / 2;
		if (rte_ioat_completed_ops(dev_id, half_len, NULL, NULL,
				(void *)completed_src,
				(void *)completed_dst) != half_len) {
			PRINT_ERR("Error with rte_ioat_completed_ops - first half request\n");
			rte_rawdev_dump(dev_id, stdout);
			return -1;
		}
		if (rte_ioat_completed_ops(dev_id, half_len, NULL, NULL,
				(void *)&completed_src[half_len],
				(void *)&completed_dst[half_len]) != half_len) {
			PRINT_ERR("Error with rte_ioat_completed_ops - second half request\n");
			rte_rawdev_dump(dev_id, stdout);
			return -1;
		}
	} else {
		/* gather all completions in one go */
		if (rte_ioat_completed_ops(dev_id, RTE_DIM(completed_src), NULL, NULL,
				(void *)completed_src,
				(void *)completed_dst) != RTE_DIM(srcs)) {
			PRINT_ERR("Error with rte_ioat_completed_ops\n");
			rte_rawdev_dump(dev_id, stdout);
			return -1;
		}
	}
	for (i = 0; i < RTE_DIM(srcs); i++) {
		char *src_data, *dst_data;

		if (completed_src[i] != srcs[i]) {
			PRINT_ERR("Error with source pointer %u\n", i);
			return -1;
		}
		if (completed_dst[i] != dsts[i]) {
			PRINT_ERR("Error with dest pointer %u\n", i);
			return -1;
		}

		src_data = rte_pktmbuf_mtod(srcs[i], char *);
		dst_data = rte_pktmbuf_mtod(dsts[i], char *);
		for (j = 0; j < COPY_LEN; j++)
			if (src_data[j] != dst_data[j]) {
				PRINT_ERR("Error with copy of packet %u, byte %u\n",
						i, j);
				return -1;
			}
		rte_pktmbuf_free(srcs[i]);
		rte_pktmbuf_free(dsts[i]);
	}
	return 0;
}

static int
test_enqueue_copies(int dev_id)
{
	unsigned int i;

	/* test doing a single copy */
	do {
		struct rte_mbuf *src, *dst;
		char *src_data, *dst_data;
		struct rte_mbuf *completed[2] = {0};

		src = rte_pktmbuf_alloc(pool);
		dst = rte_pktmbuf_alloc(pool);
		src_data = rte_pktmbuf_mtod(src, char *);
		dst_data = rte_pktmbuf_mtod(dst, char *);

		for (i = 0; i < COPY_LEN; i++)
			src_data[i] = rand() & 0xFF;

		if (rte_ioat_enqueue_copy(dev_id,
				src->buf_iova + src->data_off,
				dst->buf_iova + dst->data_off,
				COPY_LEN,
				(uintptr_t)src,
				(uintptr_t)dst) != 1) {
			PRINT_ERR("Error with rte_ioat_enqueue_copy\n");
			return -1;
		}
		rte_ioat_perform_ops(dev_id);
		usleep(10);

		if (rte_ioat_completed_ops(dev_id, 1, NULL, NULL, (void *)&completed[0],
				(void *)&completed[1]) != 1) {
			PRINT_ERR("Error with rte_ioat_completed_ops\n");
			return -1;
		}
		if (completed[0] != src || completed[1] != dst) {
			PRINT_ERR("Error with completions: got (%p, %p), not (%p,%p)\n",
					completed[0], completed[1], src, dst);
			return -1;
		}

		for (i = 0; i < COPY_LEN; i++)
			if (dst_data[i] != src_data[i]) {
				PRINT_ERR("Data mismatch at char %u [Got %02x not %02x]\n",
						i, dst_data[i], src_data[i]);
				return -1;
			}
		rte_pktmbuf_free(src);
		rte_pktmbuf_free(dst);

		/* check ring is now empty */
		if (rte_ioat_completed_ops(dev_id, 1, NULL, NULL, (void *)&completed[0],
				(void *)&completed[1]) != 0) {
			PRINT_ERR("Error: got unexpected returned handles from rte_ioat_completed_ops\n");
			return -1;
		}
	} while (0);

	/* test doing a multiple single copies */
	do {
		const uint16_t max_ops = 4;
		struct rte_mbuf *src, *dst;
		char *src_data, *dst_data;
		struct rte_mbuf *completed[32] = {0};
		const uint16_t max_completions = RTE_DIM(completed) / 2;

		src = rte_pktmbuf_alloc(pool);
		dst = rte_pktmbuf_alloc(pool);
		src_data = rte_pktmbuf_mtod(src, char *);
		dst_data = rte_pktmbuf_mtod(dst, char *);

		for (i = 0; i < COPY_LEN; i++)
			src_data[i] = rand() & 0xFF;

		/* perform the same copy <max_ops> times */
		for (i = 0; i < max_ops; i++) {
			if (rte_ioat_enqueue_copy(dev_id,
					src->buf_iova + src->data_off,
					dst->buf_iova + dst->data_off,
					COPY_LEN,
					(uintptr_t)src,
					(uintptr_t)dst) != 1) {
				PRINT_ERR("Error with rte_ioat_enqueue_copy\n");
				return -1;
			}
			rte_ioat_perform_ops(dev_id);
		}
		usleep(10);

		if (rte_ioat_completed_ops(dev_id, max_completions, NULL, NULL,
				(void *)&completed[0],
				(void *)&completed[max_completions]) != max_ops) {
			PRINT_ERR("Error with rte_ioat_completed_ops\n");
			rte_rawdev_dump(dev_id, stdout);
			return -1;
		}
		if (completed[0] != src || completed[max_completions] != dst) {
			PRINT_ERR("Error with completions: got (%p, %p), not (%p,%p)\n",
					completed[0], completed[max_completions], src, dst);
			return -1;
		}

		for (i = 0; i < COPY_LEN; i++)
			if (dst_data[i] != src_data[i]) {
				PRINT_ERR("Data mismatch at char %u\n", i);
				return -1;
			}
		rte_pktmbuf_free(src);
		rte_pktmbuf_free(dst);
	} while (0);

	/* test doing multiple copies */
	do_multi_copies(dev_id, 0, 0); /* enqueue and complete one batch at a time */
	do_multi_copies(dev_id, 1, 0); /* enqueue 2 batches and then complete both */
	do_multi_copies(dev_id, 0, 1); /* enqueue 1 batch, then complete in two halves */
	return 0;
}

static int
test_enqueue_fill(int dev_id)
{
	const unsigned int lengths[] = {8, 64, 1024, 50, 100, 89};
	struct rte_mbuf *dst = rte_pktmbuf_alloc(pool);
	char *dst_data = rte_pktmbuf_mtod(dst, char *);
	struct rte_mbuf *completed[2] = {0};
	uint64_t pattern = 0xfedcba9876543210;
	unsigned int i, j;

	for (i = 0; i < RTE_DIM(lengths); i++) {
		/* reset dst_data */
		memset(dst_data, 0, lengths[i]);

		/* perform the fill operation */
		if (rte_ioat_enqueue_fill(dev_id, pattern,
				dst->buf_iova + dst->data_off, lengths[i],
				(uintptr_t)dst) != 1) {
			PRINT_ERR("Error with rte_ioat_enqueue_fill\n");
			return -1;
		}

		rte_ioat_perform_ops(dev_id);
		usleep(100);

		if (rte_ioat_completed_ops(dev_id, 1, NULL, NULL, (void *)&completed[0],
			(void *)&completed[1]) != 1) {
			PRINT_ERR("Error with completed ops\n");
			return -1;
		}
		/* check the result */
		for (j = 0; j < lengths[i]; j++) {
			char pat_byte = ((char *)&pattern)[j % 8];
			if (dst_data[j] != pat_byte) {
				PRINT_ERR("Error with fill operation (lengths = %u): got (%x), not (%x)\n",
						lengths[i], dst_data[j], pat_byte);
				return -1;
			}
		}
	}

	rte_pktmbuf_free(dst);
	return 0;
}

static int
test_burst_capacity(int dev_id)
{
#define BURST_SIZE			64
	const unsigned int ring_space = rte_ioat_burst_capacity(dev_id);
	struct rte_mbuf *src, *dst;
	unsigned int length = 1024;
	unsigned int i, j, iter;
	unsigned int old_cap, cap;
	uintptr_t completions[BURST_SIZE];

	src = rte_pktmbuf_alloc(pool);
	dst = rte_pktmbuf_alloc(pool);

	old_cap = ring_space;
	/* to test capacity, we enqueue elements and check capacity is reduced
	 * by one each time - rebaselining the expected value after each burst
	 * as the capacity is only for a burst. We enqueue multiple bursts to
	 * fill up half the ring, before emptying it again. We do this twice to
	 * ensure that we get to test scenarios where we get ring wrap-around
	 */
	for (iter = 0; iter < 2; iter++) {
		for (i = 0; i < ring_space / (2 * BURST_SIZE); i++) {
			cap = rte_ioat_burst_capacity(dev_id);
			if (cap > old_cap) {
				PRINT_ERR("Error, avail ring capacity has gone up, not down\n");
				return -1;
			}
			old_cap = cap;

			for (j = 0; j < BURST_SIZE; j++) {
				if (rte_ioat_enqueue_copy(dev_id, rte_pktmbuf_iova(src),
						rte_pktmbuf_iova(dst), length, 0, 0) != 1) {
					PRINT_ERR("Error with rte_ioat_enqueue_copy\n");
					return -1;
				}
				if (cap - rte_ioat_burst_capacity(dev_id) != j + 1) {
					PRINT_ERR("Error, ring capacity did not change as expected\n");
					return -1;
				}
			}
			rte_ioat_perform_ops(dev_id);
		}
		usleep(100);
		for (i = 0; i < ring_space / (2 * BURST_SIZE); i++) {
			if (rte_ioat_completed_ops(dev_id, BURST_SIZE,
					NULL, NULL,
					completions, completions) != BURST_SIZE) {
				PRINT_ERR("Error with completions\n");
				return -1;
			}
		}
		if (rte_ioat_burst_capacity(dev_id) != ring_space) {
			PRINT_ERR("Error, ring capacity has not reset to original value\n");
			return -1;
		}
		old_cap = ring_space;
	}

	rte_pktmbuf_free(src);
	rte_pktmbuf_free(dst);

	return 0;
}

static int
test_completion_status(int dev_id)
{
#define COMP_BURST_SZ	16
	const unsigned int fail_copy[] = {0, 7, 15};
	struct rte_mbuf *srcs[COMP_BURST_SZ], *dsts[COMP_BURST_SZ];
	struct rte_mbuf *completed_src[COMP_BURST_SZ * 2];
	struct rte_mbuf *completed_dst[COMP_BURST_SZ * 2];
	unsigned int length = 1024;
	unsigned int i;
	uint8_t not_ok = 0;

	/* Test single full batch statuses */
	for (i = 0; i < RTE_DIM(fail_copy); i++) {
		uint32_t status[COMP_BURST_SZ] = {0};
		unsigned int j;

		for (j = 0; j < COMP_BURST_SZ; j++) {
			srcs[j] = rte_pktmbuf_alloc(pool);
			dsts[j] = rte_pktmbuf_alloc(pool);

			if (rte_ioat_enqueue_copy(dev_id,
					(j == fail_copy[i] ? (phys_addr_t)NULL :
							(srcs[j]->buf_iova + srcs[j]->data_off)),
					dsts[j]->buf_iova + dsts[j]->data_off,
					length,
					(uintptr_t)srcs[j],
					(uintptr_t)dsts[j]) != 1) {
				PRINT_ERR("Error with rte_ioat_enqueue_copy for buffer %u\n", j);
				return -1;
			}
		}
		rte_ioat_perform_ops(dev_id);
		usleep(100);

		if (rte_ioat_completed_ops(dev_id, COMP_BURST_SZ, status, &not_ok,
				(void *)completed_src, (void *)completed_dst) != COMP_BURST_SZ) {
			PRINT_ERR("Error with rte_ioat_completed_ops\n");
			rte_rawdev_dump(dev_id, stdout);
			return -1;
		}
		if (not_ok != 1 || status[fail_copy[i]] == RTE_IOAT_OP_SUCCESS) {
			unsigned int j;
			PRINT_ERR("Error, missing expected failed copy, %u\n", fail_copy[i]);
			for (j = 0; j < COMP_BURST_SZ; j++)
				printf("%u ", status[j]);
			printf("<-- Statuses\n");
			return -1;
		}
		for (j = 0; j < COMP_BURST_SZ; j++) {
			rte_pktmbuf_free(completed_src[j]);
			rte_pktmbuf_free(completed_dst[j]);
		}
	}

	/* Test gathering status for two batches at once */
	for (i = 0; i < RTE_DIM(fail_copy); i++) {
		uint32_t status[COMP_BURST_SZ] = {0};
		unsigned int batch, j;
		unsigned int expected_failures = 0;

		for (batch = 0; batch < 2; batch++) {
			for (j = 0; j < COMP_BURST_SZ/2; j++) {
				srcs[j] = rte_pktmbuf_alloc(pool);
				dsts[j] = rte_pktmbuf_alloc(pool);

				if (j == fail_copy[i])
					expected_failures++;
				if (rte_ioat_enqueue_copy(dev_id,
						(j == fail_copy[i] ? (phys_addr_t)NULL :
							(srcs[j]->buf_iova + srcs[j]->data_off)),
						dsts[j]->buf_iova + dsts[j]->data_off,
						length,
						(uintptr_t)srcs[j],
						(uintptr_t)dsts[j]) != 1) {
					PRINT_ERR("Error with rte_ioat_enqueue_copy for buffer %u\n",
							j);
					return -1;
				}
			}
			rte_ioat_perform_ops(dev_id);
		}
		usleep(100);

		if (rte_ioat_completed_ops(dev_id, COMP_BURST_SZ, status, &not_ok,
				(void *)completed_src, (void *)completed_dst) != COMP_BURST_SZ) {
			PRINT_ERR("Error with rte_ioat_completed_ops\n");
			rte_rawdev_dump(dev_id, stdout);
			return -1;
		}
		if (not_ok != expected_failures) {
			unsigned int j;
			PRINT_ERR("Error, missing expected failed copy, got %u, not %u\n",
					not_ok, expected_failures);
			for (j = 0; j < COMP_BURST_SZ; j++)
				printf("%u ", status[j]);
			printf("<-- Statuses\n");
			return -1;
		}
		for (j = 0; j < COMP_BURST_SZ; j++) {
			rte_pktmbuf_free(completed_src[j]);
			rte_pktmbuf_free(completed_dst[j]);
		}
	}

	/* Test gathering status for half batch at a time */
	for (i = 0; i < RTE_DIM(fail_copy); i++) {
		uint32_t status[COMP_BURST_SZ] = {0};
		unsigned int j;

		for (j = 0; j < COMP_BURST_SZ; j++) {
			srcs[j] = rte_pktmbuf_alloc(pool);
			dsts[j] = rte_pktmbuf_alloc(pool);

			if (rte_ioat_enqueue_copy(dev_id,
					(j == fail_copy[i] ? (phys_addr_t)NULL :
							(srcs[j]->buf_iova + srcs[j]->data_off)),
					dsts[j]->buf_iova + dsts[j]->data_off,
					length,
					(uintptr_t)srcs[j],
					(uintptr_t)dsts[j]) != 1) {
				PRINT_ERR("Error with rte_ioat_enqueue_copy for buffer %u\n", j);
				return -1;
			}
		}
		rte_ioat_perform_ops(dev_id);
		usleep(100);

		if (rte_ioat_completed_ops(dev_id, COMP_BURST_SZ / 2, status, &not_ok,
				(void *)completed_src,
				(void *)completed_dst) != (COMP_BURST_SZ / 2)) {
			PRINT_ERR("Error with rte_ioat_completed_ops\n");
			rte_rawdev_dump(dev_id, stdout);
			return -1;
		}
		if (fail_copy[i] < COMP_BURST_SZ / 2 &&
				(not_ok != 1 || status[fail_copy[i]] == RTE_IOAT_OP_SUCCESS)) {
			PRINT_ERR("Missing expected failure in first half-batch\n");
			rte_rawdev_dump(dev_id, stdout);
			return -1;
		}
		if (rte_ioat_completed_ops(dev_id, COMP_BURST_SZ / 2, status, &not_ok,
				(void *)&completed_src[COMP_BURST_SZ / 2],
				(void *)&completed_dst[COMP_BURST_SZ / 2]) != (COMP_BURST_SZ / 2)) {
			PRINT_ERR("Error with rte_ioat_completed_ops\n");
			rte_rawdev_dump(dev_id, stdout);
			return -1;
		}
		if (fail_copy[i] >= COMP_BURST_SZ / 2 && (not_ok != 1 ||
				status[fail_copy[i] - (COMP_BURST_SZ / 2)]
					== RTE_IOAT_OP_SUCCESS)) {
			PRINT_ERR("Missing expected failure in second half-batch\n");
			rte_rawdev_dump(dev_id, stdout);
			return -1;
		}

		for (j = 0; j < COMP_BURST_SZ; j++) {
			rte_pktmbuf_free(completed_src[j]);
			rte_pktmbuf_free(completed_dst[j]);
		}
	}

	/* Test gathering statuses with fence */
	for (i = 1; i < RTE_DIM(fail_copy); i++) {
		uint32_t status[COMP_BURST_SZ * 2] = {0};
		unsigned int j;
		uint16_t count;

		for (j = 0; j < COMP_BURST_SZ; j++) {
			srcs[j] = rte_pktmbuf_alloc(pool);
			dsts[j] = rte_pktmbuf_alloc(pool);

			/* always fail the first copy */
			if (rte_ioat_enqueue_copy(dev_id,
					(j == 0 ? (phys_addr_t)NULL :
						(srcs[j]->buf_iova + srcs[j]->data_off)),
					dsts[j]->buf_iova + dsts[j]->data_off,
					length,
					(uintptr_t)srcs[j],
					(uintptr_t)dsts[j]) != 1) {
				PRINT_ERR("Error with rte_ioat_enqueue_copy for buffer %u\n", j);
				return -1;
			}
			/* put in a fence which will stop any further transactions
			 * because we had a previous failure.
			 */
			if (j == fail_copy[i])
				rte_ioat_fence(dev_id);
		}
		rte_ioat_perform_ops(dev_id);
		usleep(100);

		count = rte_ioat_completed_ops(dev_id, COMP_BURST_SZ * 2, status, &not_ok,
				(void *)completed_src, (void *)completed_dst);
		if (count != COMP_BURST_SZ) {
			PRINT_ERR("Error with rte_ioat_completed_ops, got %u not %u\n",
					count, COMP_BURST_SZ);
			for (j = 0; j < count; j++)
				printf("%u ", status[j]);
			printf("<-- Statuses\n");
			return -1;
		}
		if (not_ok != COMP_BURST_SZ - fail_copy[i]) {
			PRINT_ERR("Unexpected failed copy count, got %u, expected %u\n",
					not_ok, COMP_BURST_SZ - fail_copy[i]);
			for (j = 0; j < COMP_BURST_SZ; j++)
				printf("%u ", status[j]);
			printf("<-- Statuses\n");
			return -1;
		}
		if (status[0] == RTE_IOAT_OP_SUCCESS || status[0] == RTE_IOAT_OP_SKIPPED) {
			PRINT_ERR("Error, op 0 unexpectedly did not fail.\n");
			return -1;
		}
		for (j = 1; j <= fail_copy[i]; j++) {
			if (status[j] != RTE_IOAT_OP_SUCCESS) {
				PRINT_ERR("Error, op %u unexpectedly failed\n", j);
				return -1;
			}
		}
		for (j = fail_copy[i] + 1; j < COMP_BURST_SZ; j++) {
			if (status[j] != RTE_IOAT_OP_SKIPPED) {
				PRINT_ERR("Error, all descriptors after fence should be invalid\n");
				return -1;
			}
		}
		for (j = 0; j < COMP_BURST_SZ; j++) {
			rte_pktmbuf_free(completed_src[j]);
			rte_pktmbuf_free(completed_dst[j]);
		}
	}

	return 0;
}

int
ioat_rawdev_test(uint16_t dev_id)
{
#define IOAT_TEST_RINGSIZE 512
	const struct rte_idxd_rawdev *idxd =
			(struct rte_idxd_rawdev *)rte_rawdevs[dev_id].dev_private;
	const enum rte_ioat_dev_type ioat_type = idxd->type;
	struct rte_ioat_rawdev_config p = { .ring_size = -1 };
	struct rte_rawdev_info info = { .dev_private = &p };
	struct rte_rawdev_xstats_name *snames = NULL;
	uint64_t *stats = NULL;
	unsigned int *ids = NULL;
	unsigned int nb_xstats;
	unsigned int i;

	if (dev_id >= MAX_SUPPORTED_RAWDEVS) {
		printf("Skipping test. Cannot test rawdevs with id's greater than %d\n",
				MAX_SUPPORTED_RAWDEVS);
		return TEST_SKIPPED;
	}

	rte_rawdev_info_get(dev_id, &info, sizeof(p));
	if (p.ring_size != expected_ring_size[dev_id]) {
		PRINT_ERR("Error, initial ring size is not as expected (Actual: %d, Expected: %d)\n",
				(int)p.ring_size, expected_ring_size[dev_id]);
		return -1;
	}

	p.ring_size = IOAT_TEST_RINGSIZE;
	if (rte_rawdev_configure(dev_id, &info, sizeof(p)) != 0) {
		PRINT_ERR("Error with rte_rawdev_configure()\n");
		return -1;
	}
	rte_rawdev_info_get(dev_id, &info, sizeof(p));
	if (p.ring_size != IOAT_TEST_RINGSIZE) {
		PRINT_ERR("Error, ring size is not %d (%d)\n",
				IOAT_TEST_RINGSIZE, (int)p.ring_size);
		return -1;
	}
	expected_ring_size[dev_id] = p.ring_size;

	if (rte_rawdev_start(dev_id) != 0) {
		PRINT_ERR("Error with rte_rawdev_start()\n");
		return -1;
	}

	pool = rte_pktmbuf_pool_create("TEST_IOAT_POOL",
			p.ring_size * 2, /* n == num elements */
			32,  /* cache size */
			0,   /* priv size */
			2048, /* data room size */
			info.socket_id);
	if (pool == NULL) {
		PRINT_ERR("Error with mempool creation\n");
		return -1;
	}

	/* allocate memory for xstats names and values */
	nb_xstats = rte_rawdev_xstats_names_get(dev_id, NULL, 0);

	snames = malloc(sizeof(*snames) * nb_xstats);
	if (snames == NULL) {
		PRINT_ERR("Error allocating xstat names memory\n");
		goto err;
	}
	rte_rawdev_xstats_names_get(dev_id, snames, nb_xstats);

	ids = malloc(sizeof(*ids) * nb_xstats);
	if (ids == NULL) {
		PRINT_ERR("Error allocating xstat ids memory\n");
		goto err;
	}
	for (i = 0; i < nb_xstats; i++)
		ids[i] = i;

	stats = malloc(sizeof(*stats) * nb_xstats);
	if (stats == NULL) {
		PRINT_ERR("Error allocating xstat memory\n");
		goto err;
	}

	/* run the test cases */
	printf("Running Copy Tests\n");
	for (i = 0; i < 100; i++) {
		unsigned int j;

		if (test_enqueue_copies(dev_id) != 0)
			goto err;

		rte_rawdev_xstats_get(dev_id, ids, stats, nb_xstats);
		for (j = 0; j < nb_xstats; j++)
			printf("%s: %"PRIu64"   ", snames[j].name, stats[j]);
		printf("\r");
	}
	printf("\n");

	/* test enqueue fill operation */
	printf("Running Fill Tests\n");
	for (i = 0; i < 100; i++) {
		unsigned int j;

		if (test_enqueue_fill(dev_id) != 0)
			goto err;

		rte_rawdev_xstats_get(dev_id, ids, stats, nb_xstats);
		for (j = 0; j < nb_xstats; j++)
			printf("%s: %"PRIu64"   ", snames[j].name, stats[j]);
		printf("\r");
	}
	printf("\n");

	printf("Running Burst Capacity Test\n");
	if (test_burst_capacity(dev_id) != 0)
		goto err;

	/* only DSA devices report address errors, and we can only use null pointers
	 * to generate those errors when DPDK is in VA mode.
	 */
	if (rte_eal_iova_mode() == RTE_IOVA_VA && ioat_type == RTE_IDXD_DEV) {
		printf("Running Completions Status Test\n");
		if (test_completion_status(dev_id) != 0)
			goto err;
	}

	rte_rawdev_stop(dev_id);
	if (rte_rawdev_xstats_reset(dev_id, NULL, 0) != 0) {
		PRINT_ERR("Error resetting xstat values\n");
		goto err;
	}

	rte_mempool_free(pool);
	free(snames);
	free(stats);
	free(ids);
	return 0;

err:
	rte_rawdev_stop(dev_id);
	rte_rawdev_xstats_reset(dev_id, NULL, 0);
	rte_mempool_free(pool);
	free(snames);
	free(stats);
	free(ids);
	return -1;
}
