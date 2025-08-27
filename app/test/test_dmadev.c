/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 HiSilicon Limited
 * Copyright(c) 2021 Intel Corporation
 */

#include <inttypes.h>

#include <rte_dmadev.h>
#include <rte_mbuf.h>
#include <rte_pause.h>
#include <rte_cycles.h>
#include <rte_random.h>
#include <rte_bus_vdev.h>
#include <rte_dmadev_pmd.h>

#include "test.h"
#include "test_dmadev_api.h"

#define ERR_RETURN(...) do { print_err(__func__, __LINE__, __VA_ARGS__); return -1; } while (0)

#define TEST_NAME_MAX_LEN 80
#define TEST_RINGSIZE 512
#define COPY_LEN 2048

static struct rte_dma_info info;
static struct rte_mempool *pool;
static bool check_err_stats;
static int16_t test_dev_id;
static uint16_t id_count;
static uint16_t vchan;

enum {
	TEST_PARAM_REMOTE_ADDR = 0,
	TEST_PARAM_MAX,
};

static const char * const dma_test_param[] = {
	[TEST_PARAM_REMOTE_ADDR] = "remote_addr",
};

static uint64_t env_test_param[TEST_PARAM_MAX];

enum {
	TEST_M2D_AUTO_FREE = 0,
	TEST_MAX,
};

struct dma_add_test {
	const char *name;
	bool enabled;
};

struct dma_add_test dma_add_test[] = {
	[TEST_M2D_AUTO_FREE] = {.name = "m2d_auto_free", .enabled = false},
};

static void
__rte_format_printf(3, 4)
print_err(const char *func, int lineno, const char *format, ...)
{
	va_list ap;

	fprintf(stderr, "In %s:%d - ", func, lineno);
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
}

struct runtest_param {
	const char name[TEST_NAME_MAX_LEN];
	int (*test_fn)(int16_t dev_id, uint16_t vchan);
	int iterations;
};

static int
runtest(const void *args)
{
	int (*test_fn)(int16_t dev_id, uint16_t vchan);
	const struct runtest_param *param = args;
	struct rte_dma_stats stats;
	const char *printable;
	int iterations;
	int16_t dev_id;
	int i;

	printable = param->name;
	iterations = param->iterations;
	test_fn = param->test_fn;
	dev_id = test_dev_id;

	rte_dma_stats_reset(dev_id, vchan);
	printf("DMA Dev %d: Running %s Tests %s\n", dev_id, printable,
			check_err_stats ? " " : "(errors expected)");
	for (i = 0; i < iterations; i++) {
		if (test_fn(dev_id, vchan) != 0)
			return -1;

		rte_dma_stats_get(dev_id, 0, &stats);
		printf("Ops submitted: %"PRIu64"\t", stats.submitted);
		printf("Ops completed: %"PRIu64"\t", stats.completed);
		printf("Errors: %"PRIu64"\r", stats.errors);

		if (stats.completed != stats.submitted)
			ERR_RETURN("\nError, not all submitted jobs are reported as completed\n");
		if (check_err_stats && stats.errors != 0)
			ERR_RETURN("\nErrors reported during op processing, aborting tests\n");
	}
	printf("\n");
	return 0;
}

static void
await_hw(int16_t dev_id, uint16_t vchan)
{
	enum rte_dma_vchan_status st;

	if (rte_dma_vchan_status(dev_id, vchan, &st) < 0) {
		/* for drivers that don't support this op, just sleep for 1 millisecond */
		rte_delay_us_sleep(1000);
		return;
	}

	/* for those that do, *max* end time is one second from now, but all should be faster */
	const uint64_t end_cycles = rte_get_timer_cycles() + rte_get_timer_hz();
	while (st == RTE_DMA_VCHAN_ACTIVE && rte_get_timer_cycles() < end_cycles) {
		rte_pause();
		rte_dma_vchan_status(dev_id, vchan, &st);
	}
}

/* run a series of copy tests just using some different options for enqueues and completions */
static int
do_multi_copies(int16_t dev_id, uint16_t vchan,
		int split_batches,     /* submit 2 x 16 or 1 x 32 burst */
		int split_completions, /* gather 2 x 16 or 1 x 32 completions */
		int use_completed_status) /* use completed or completed_status function */
{
	struct rte_mbuf *srcs[32], *dsts[32];
	enum rte_dma_status_code sc[32];
	unsigned int i, j;
	bool dma_err = false;

	/* Enqueue burst of copies and hit doorbell */
	for (i = 0; i < RTE_DIM(srcs); i++) {
		uint64_t *src_data;

		if (split_batches && i == RTE_DIM(srcs) / 2)
			rte_dma_submit(dev_id, vchan);

		srcs[i] = rte_pktmbuf_alloc(pool);
		dsts[i] = rte_pktmbuf_alloc(pool);
		if (srcs[i] == NULL || dsts[i] == NULL)
			ERR_RETURN("Error allocating buffers\n");

		src_data = rte_pktmbuf_mtod(srcs[i], uint64_t *);
		for (j = 0; j < COPY_LEN/sizeof(uint64_t); j++)
			src_data[j] = rte_rand();

		if (rte_dma_copy(dev_id, vchan, rte_mbuf_data_iova(srcs[i]),
				 rte_mbuf_data_iova(dsts[i]), COPY_LEN, 0) != id_count++)
			ERR_RETURN("Error with rte_dma_copy for buffer %u\n", i);
	}
	rte_dma_submit(dev_id, vchan);

	await_hw(dev_id, vchan);

	if (split_completions) {
		/* gather completions in two halves */
		uint16_t half_len = RTE_DIM(srcs) / 2;
		int ret = rte_dma_completed(dev_id, vchan, half_len, NULL, &dma_err);
		if (ret != half_len || dma_err)
			ERR_RETURN("Error with rte_dma_completed - first half. ret = %d, expected ret = %u, dma_err = %d\n",
					ret, half_len, dma_err);

		ret = rte_dma_completed(dev_id, vchan, half_len, NULL, &dma_err);
		if (ret != half_len || dma_err)
			ERR_RETURN("Error with rte_dma_completed - second half. ret = %d, expected ret = %u, dma_err = %d\n",
					ret, half_len, dma_err);
	} else {
		/* gather all completions in one go, using either
		 * completed or completed_status fns
		 */
		if (!use_completed_status) {
			int n = rte_dma_completed(dev_id, vchan, RTE_DIM(srcs), NULL, &dma_err);
			if (n != RTE_DIM(srcs) || dma_err)
				ERR_RETURN("Error with rte_dma_completed, %u [expected: %zu], dma_err = %d\n",
						n, RTE_DIM(srcs), dma_err);
		} else {
			int n = rte_dma_completed_status(dev_id, vchan, RTE_DIM(srcs), NULL, sc);
			if (n != RTE_DIM(srcs))
				ERR_RETURN("Error with rte_dma_completed_status, %u [expected: %zu]\n",
						n, RTE_DIM(srcs));

			for (j = 0; j < (uint16_t)n; j++)
				if (sc[j] != RTE_DMA_STATUS_SUCCESSFUL)
					ERR_RETURN("Error with rte_dma_completed_status, job %u reports failure [code %u]\n",
							j, sc[j]);
		}
	}

	/* check for empty */
	int ret = use_completed_status ?
			rte_dma_completed_status(dev_id, vchan, RTE_DIM(srcs), NULL, sc) :
			rte_dma_completed(dev_id, vchan, RTE_DIM(srcs), NULL, &dma_err);
	if (ret != 0)
		ERR_RETURN("Error with completion check - ops unexpectedly returned\n");

	for (i = 0; i < RTE_DIM(srcs); i++) {
		char *src_data, *dst_data;

		src_data = rte_pktmbuf_mtod(srcs[i], char *);
		dst_data = rte_pktmbuf_mtod(dsts[i], char *);
		for (j = 0; j < COPY_LEN; j++)
			if (src_data[j] != dst_data[j])
				ERR_RETURN("Error with copy of packet %u, byte %u\n", i, j);

		rte_pktmbuf_free(srcs[i]);
		rte_pktmbuf_free(dsts[i]);
	}
	return 0;
}

static int
test_single_copy(int16_t dev_id, uint16_t vchan)
{
	uint16_t i;
	uint16_t id;
	enum rte_dma_status_code status;
	struct rte_mbuf *src, *dst;
	char *src_data, *dst_data;

	src = rte_pktmbuf_alloc(pool);
	dst = rte_pktmbuf_alloc(pool);
	src_data = rte_pktmbuf_mtod(src, char *);
	dst_data = rte_pktmbuf_mtod(dst, char *);

	for (i = 0; i < COPY_LEN; i++)
		src_data[i] = rte_rand() & 0xFF;

	id = rte_dma_copy(dev_id, vchan, rte_pktmbuf_iova(src), rte_pktmbuf_iova(dst),
			COPY_LEN, RTE_DMA_OP_FLAG_SUBMIT);
	if (id != id_count)
		ERR_RETURN("Error with rte_dma_copy, got %u, expected %u\n",
				id, id_count);

	/* give time for copy to finish, then check it was done */
	await_hw(dev_id, vchan);

	for (i = 0; i < COPY_LEN; i++)
		if (dst_data[i] != src_data[i])
			ERR_RETURN("Data mismatch at char %u [Got %02x not %02x]\n", i,
					dst_data[i], src_data[i]);

	/* now check completion works */
	id = ~id;
	if (rte_dma_completed(dev_id, vchan, 1, &id, NULL) != 1)
		ERR_RETURN("Error with rte_dma_completed\n");

	if (id != id_count)
		ERR_RETURN("Error:incorrect job id received, %u [expected %u]\n",
				id, id_count);

	/* check for completed and id when no job done */
	id = ~id;
	if (rte_dma_completed(dev_id, vchan, 1, &id, NULL) != 0)
		ERR_RETURN("Error with rte_dma_completed when no job done\n");
	if (id != id_count)
		ERR_RETURN("Error:incorrect job id received when no job done, %u [expected %u]\n",
				id, id_count);

	/* check for completed_status and id when no job done */
	id = ~id;
	if (rte_dma_completed_status(dev_id, vchan, 1, &id, &status) != 0)
		ERR_RETURN("Error with rte_dma_completed_status when no job done\n");
	if (id != id_count)
		ERR_RETURN("Error:incorrect job id received when no job done, %u [expected %u]\n",
				id, id_count);

	rte_pktmbuf_free(src);
	rte_pktmbuf_free(dst);

	/* now check completion returns nothing more */
	if (rte_dma_completed(dev_id, 0, 1, NULL, NULL) != 0)
		ERR_RETURN("Error with rte_dma_completed in empty check\n");

	id_count++;

	return 0;
}

static int
test_enqueue_copies(int16_t dev_id, uint16_t vchan)
{
	unsigned int i;

	/* test doing a single copy */
	if (test_single_copy(dev_id, vchan) < 0)
		return -1;

	/* test doing a multiple single copies */
	do {
		uint16_t id;
		const uint16_t max_ops = 4;
		struct rte_mbuf *src, *dst;
		char *src_data, *dst_data;
		uint16_t count;

		src = rte_pktmbuf_alloc(pool);
		dst = rte_pktmbuf_alloc(pool);
		src_data = rte_pktmbuf_mtod(src, char *);
		dst_data = rte_pktmbuf_mtod(dst, char *);

		for (i = 0; i < COPY_LEN; i++)
			src_data[i] = rte_rand() & 0xFF;

		/* perform the same copy <max_ops> times */
		for (i = 0; i < max_ops; i++)
			if (rte_dma_copy(dev_id, vchan,
					rte_pktmbuf_iova(src),
					rte_pktmbuf_iova(dst),
					COPY_LEN, RTE_DMA_OP_FLAG_SUBMIT) != id_count++)
				ERR_RETURN("Error with rte_dma_copy\n");

		await_hw(dev_id, vchan);

		count = rte_dma_completed(dev_id, vchan, max_ops * 2, &id, NULL);
		if (count != max_ops)
			ERR_RETURN("Error with rte_dma_completed, got %u not %u\n",
					count, max_ops);

		if (id != id_count - 1)
			ERR_RETURN("Error, incorrect job id returned: got %u not %u\n",
					id, id_count - 1);

		for (i = 0; i < COPY_LEN; i++)
			if (dst_data[i] != src_data[i])
				ERR_RETURN("Data mismatch at char %u\n", i);

		rte_pktmbuf_free(src);
		rte_pktmbuf_free(dst);
	} while (0);

	/* test doing multiple copies */
	return do_multi_copies(dev_id, vchan, 0, 0, 0) /* enqueue and complete 1 batch at a time */
			/* enqueue 2 batches and then complete both */
			|| do_multi_copies(dev_id, vchan, 1, 0, 0)
			/* enqueue 1 batch, then complete in two halves */
			|| do_multi_copies(dev_id, vchan, 0, 1, 0)
			/* test using completed_status in place of regular completed API */
			|| do_multi_copies(dev_id, vchan, 0, 0, 1);
}

static int
test_stop_start(int16_t dev_id, uint16_t vchan)
{
	/* device is already started on input, should be (re)started on output */

	uint16_t id = 0;
	enum rte_dma_status_code status = RTE_DMA_STATUS_SUCCESSFUL;

	/* - test stopping a device works ok,
	 * - then do a start-stop without doing a copy
	 * - finally restart the device
	 * checking for errors at each stage, and validating we can still copy at the end.
	 */
	if (rte_dma_stop(dev_id) < 0)
		ERR_RETURN("Error stopping device\n");

	if (rte_dma_start(dev_id) < 0)
		ERR_RETURN("Error restarting device\n");
	if (rte_dma_stop(dev_id) < 0)
		ERR_RETURN("Error stopping device after restart (no jobs executed)\n");

	if (rte_dma_start(dev_id) < 0)
		ERR_RETURN("Error restarting device after multiple stop-starts\n");

	/* before doing a copy, we need to know what the next id will be it should
	 * either be:
	 * - the last completed job before start if driver does not reset id on stop
	 * - or -1 i.e. next job is 0, if driver does reset the job ids on stop
	 */
	if (rte_dma_completed_status(dev_id, vchan, 1, &id, &status) != 0)
		ERR_RETURN("Error with rte_dma_completed_status when no job done\n");
	id += 1; /* id_count is next job id */
	if (id != id_count && id != 0)
		ERR_RETURN("Unexpected next id from device after stop-start. Got %u, expected %u or 0\n",
				id, id_count);

	id_count = id;
	if (test_single_copy(dev_id, vchan) < 0)
		ERR_RETURN("Error performing copy after device restart\n");
	return 0;
}

static int
test_enqueue_sg_copies(int16_t dev_id, uint16_t vchan)
{
	unsigned int src_len, dst_len, n_sge, len, i, j, k;
	char orig_src[COPY_LEN], orig_dst[COPY_LEN];
	struct rte_dma_info info = { 0 };
	enum rte_dma_status_code status;
	uint16_t id, n_src, n_dst;

	if (rte_dma_info_get(dev_id, &info) < 0)
		ERR_RETURN("Failed to get dev info");

	if (info.max_sges < 2)
		ERR_RETURN("Test needs minimum 2 SG pointers");

	n_sge = info.max_sges;

	for (n_src = 1; n_src <= n_sge; n_src++) {
		for (n_dst = 1; n_dst <= n_sge; n_dst++) {
			/* Normalize SG buffer lengths */
			len = COPY_LEN;
			len -= (len % (n_src * n_dst));
			dst_len = len / n_dst;
			src_len = len / n_src;

			struct rte_dma_sge *sg_src = alloca(sizeof(struct rte_dma_sge) * n_sge);
			struct rte_dma_sge *sg_dst = alloca(sizeof(struct rte_dma_sge) * n_sge);
			struct rte_mbuf **src = alloca(sizeof(struct rte_mbuf *) * n_sge);
			struct rte_mbuf **dst = alloca(sizeof(struct rte_mbuf *) * n_sge);
			char **src_data = alloca(sizeof(char *) * n_sge);
			char **dst_data = alloca(sizeof(char *) * n_sge);

			for (i = 0 ; i < len; i++)
				orig_src[i] = rte_rand() & 0xFF;

			memset(orig_dst, 0, len);

			for (i = 0; i < n_src; i++) {
				src[i] = rte_pktmbuf_alloc(pool);
				RTE_ASSERT(src[i] != NULL);
				sg_src[i].addr = rte_pktmbuf_iova(src[i]);
				sg_src[i].length = src_len;
				src_data[i] = rte_pktmbuf_mtod(src[i], char *);
			}

			for (k = 0; k < n_dst; k++) {
				dst[k] = rte_pktmbuf_alloc(pool);
				RTE_ASSERT(dst[k] != NULL);
				sg_dst[k].addr = rte_pktmbuf_iova(dst[k]);
				sg_dst[k].length = dst_len;
				dst_data[k] = rte_pktmbuf_mtod(dst[k], char *);
			}

			for (i = 0; i < n_src; i++) {
				for (j = 0; j < src_len; j++)
					src_data[i][j] = orig_src[i * src_len + j];
			}

			for (k = 0; k < n_dst; k++)
				memset(dst_data[k], 0, dst_len);

			printf("\tsrc segs: %2d [seg len: %4d] - dst segs: %2d [seg len : %4d]\n",
				n_src, src_len, n_dst, dst_len);

			id = rte_dma_copy_sg(dev_id, vchan, sg_src, sg_dst, n_src, n_dst,
					     RTE_DMA_OP_FLAG_SUBMIT);

			if (id != id_count)
				ERR_RETURN("Error with rte_dma_copy_sg, got %u, expected %u\n",
					id, id_count);

			/* Give time for copy to finish, then check it was done */
			await_hw(dev_id, vchan);

			for (k = 0; k < n_dst; k++)
				memcpy((&orig_dst[0] + k * dst_len), dst_data[k], dst_len);

			if (memcmp(orig_src, orig_dst, COPY_LEN))
				ERR_RETURN("Data mismatch");

			/* Verify completion */
			id = ~id;
			if (rte_dma_completed(dev_id, vchan, 1, &id, NULL) != 1)
				ERR_RETURN("Error with rte_dma_completed\n");

			/* Verify expected index(id_count) */
			if (id != id_count)
				ERR_RETURN("Error:incorrect job id received, %u [expected %u]\n",
						id, id_count);

			/* Check for completed and id when no job done */
			id = ~id;
			if (rte_dma_completed(dev_id, vchan, 1, &id, NULL) != 0)
				ERR_RETURN("Error with rte_dma_completed when no job done\n");

			if (id != id_count)
				ERR_RETURN("Error:incorrect job id received when no job done, %u [expected %u]\n",
					   id, id_count);

			/* Check for completed_status and id when no job done */
			id = ~id;
			if (rte_dma_completed_status(dev_id, vchan, 1, &id, &status) != 0)
				ERR_RETURN("Error with rte_dma_completed_status when no job done\n");
			if (id != id_count)
				ERR_RETURN("Error:incorrect job id received when no job done, %u [expected %u]\n",
						id, 0);

			for (i = 0; i < n_src; i++)
				rte_pktmbuf_free(src[i]);
			for (i = 0; i < n_dst; i++)
				rte_pktmbuf_free(dst[i]);

			/* Verify that completion returns nothing more */
			if (rte_dma_completed(dev_id, 0, 1, NULL, NULL) != 0)
				ERR_RETURN("Error with rte_dma_completed in empty check\n");

			id_count++;
		}
	}
	return 0;
}

/* Failure handling test cases - global macros and variables for those tests*/
#define COMP_BURST_SZ	16
#define OPT_FENCE(idx) ((fence && idx == 8) ? RTE_DMA_OP_FLAG_FENCE : 0)

static int
test_failure_in_full_burst(int16_t dev_id, uint16_t vchan, bool fence,
		struct rte_mbuf **srcs, struct rte_mbuf **dsts, unsigned int fail_idx)
{
	/* Test single full batch statuses with failures */
	enum rte_dma_status_code status[COMP_BURST_SZ];
	struct rte_dma_stats baseline, stats;
	uint16_t invalid_addr_id = 0;
	uint16_t idx;
	uint16_t count, status_count;
	unsigned int i;
	bool error = false;
	int err_count = 0;

	rte_dma_stats_get(dev_id, vchan, &baseline); /* get a baseline set of stats */
	for (i = 0; i < COMP_BURST_SZ; i++) {
		int id = rte_dma_copy(dev_id, vchan,
				      (i == fail_idx ? 0 : rte_mbuf_data_iova(srcs[i])),
				      rte_mbuf_data_iova(dsts[i]), COPY_LEN, OPT_FENCE(i));
		if (id < 0)
			ERR_RETURN("Error with rte_dma_copy for buffer %u\n", i);
		if (i == fail_idx)
			invalid_addr_id = id;
	}
	rte_dma_submit(dev_id, vchan);
	rte_dma_stats_get(dev_id, vchan, &stats);
	if (stats.submitted != baseline.submitted + COMP_BURST_SZ)
		ERR_RETURN("Submitted stats value not as expected, %"PRIu64" not %"PRIu64"\n",
				stats.submitted, baseline.submitted + COMP_BURST_SZ);

	await_hw(dev_id, vchan);

	count = rte_dma_completed(dev_id, vchan, COMP_BURST_SZ, &idx, &error);
	if (count != fail_idx)
		ERR_RETURN("Error with rte_dma_completed for failure test. Got returned %u not %u.\n",
				count, fail_idx);
	if (!error)
		ERR_RETURN("Error, missing expected failed copy, %u. has_error is not set\n",
				fail_idx);
	if (idx != invalid_addr_id - 1)
		ERR_RETURN("Error, missing expected failed copy, %u. Got last idx %u, not %u\n",
				fail_idx, idx, invalid_addr_id - 1);

	/* all checks ok, now verify calling completed() again always returns 0 */
	for (i = 0; i < 10; i++)
		if (rte_dma_completed(dev_id, vchan, COMP_BURST_SZ, &idx, &error) != 0
				|| error == false || idx != (invalid_addr_id - 1))
			ERR_RETURN("Error with follow-up completed calls for fail idx %u\n",
					fail_idx);

	status_count = rte_dma_completed_status(dev_id, vchan, COMP_BURST_SZ,
			&idx, status);
	/* some HW may stop on error and be restarted after getting error status for single value
	 * To handle this case, if we get just one error back, wait for more completions and get
	 * status for rest of the burst
	 */
	if (status_count == 1) {
		await_hw(dev_id, vchan);
		status_count += rte_dma_completed_status(dev_id, vchan, COMP_BURST_SZ - 1,
					&idx, &status[1]);
	}
	/* check that at this point we have all status values */
	if (status_count != COMP_BURST_SZ - count)
		ERR_RETURN("Error with completed_status calls for fail idx %u. Got %u not %u\n",
				fail_idx, status_count, COMP_BURST_SZ - count);
	/* now verify just one failure followed by multiple successful or skipped entries */
	if (status[0] == RTE_DMA_STATUS_SUCCESSFUL)
		ERR_RETURN("Error with status returned for fail idx %u. First status was not failure\n",
				fail_idx);
	for (i = 1; i < status_count; i++)
		/* after a failure in a burst, depending on ordering/fencing,
		 * operations may be successful or skipped because of previous error.
		 */
		if (status[i] != RTE_DMA_STATUS_SUCCESSFUL
				&& status[i] != RTE_DMA_STATUS_NOT_ATTEMPTED)
			ERR_RETURN("Error with status calls for fail idx %u. Status for job %u (of %u) is not successful\n",
					fail_idx, count + i, COMP_BURST_SZ);

	/* check the completed + errors stats are as expected */
	rte_dma_stats_get(dev_id, vchan, &stats);
	if (stats.completed != baseline.completed + COMP_BURST_SZ)
		ERR_RETURN("Completed stats value not as expected, %"PRIu64" not %"PRIu64"\n",
				stats.completed, baseline.completed + COMP_BURST_SZ);
	for (i = 0; i < status_count; i++)
		err_count += (status[i] != RTE_DMA_STATUS_SUCCESSFUL);
	if (stats.errors != baseline.errors + err_count)
		ERR_RETURN("'Errors' stats value not as expected, %"PRIu64" not %"PRIu64"\n",
				stats.errors, baseline.errors + err_count);

	return 0;
}

static int
test_individual_status_query_with_failure(int16_t dev_id, uint16_t vchan, bool fence,
		struct rte_mbuf **srcs, struct rte_mbuf **dsts, unsigned int fail_idx)
{
	/* Test gathering batch statuses one at a time */
	enum rte_dma_status_code status[COMP_BURST_SZ];
	uint16_t invalid_addr_id = 0;
	uint16_t idx;
	uint16_t count = 0, status_count = 0;
	unsigned int j;
	bool error = false;

	for (j = 0; j < COMP_BURST_SZ; j++) {
		int id = rte_dma_copy(dev_id, vchan,
				      (j == fail_idx ? 0 : rte_mbuf_data_iova(srcs[j])),
				      rte_mbuf_data_iova(dsts[j]), COPY_LEN, OPT_FENCE(j));
		if (id < 0)
			ERR_RETURN("Error with rte_dma_copy for buffer %u\n", j);
		if (j == fail_idx)
			invalid_addr_id = id;
	}
	rte_dma_submit(dev_id, vchan);
	await_hw(dev_id, vchan);

	/* use regular "completed" until we hit error */
	while (!error) {
		uint16_t n = rte_dma_completed(dev_id, vchan, 1, &idx, &error);
		count += n;
		if (n > 1 || count >= COMP_BURST_SZ)
			ERR_RETURN("Error - too many completions got\n");
		if (n == 0 && !error)
			ERR_RETURN("Error, unexpectedly got zero completions after %u completed\n",
					count);
	}
	if (idx != invalid_addr_id - 1)
		ERR_RETURN("Error, last successful index not as expected, got %u, expected %u\n",
				idx, invalid_addr_id - 1);

	/* use completed_status until we hit end of burst */
	while (count + status_count < COMP_BURST_SZ) {
		uint16_t n = rte_dma_completed_status(dev_id, vchan, 1, &idx,
				&status[status_count]);
		await_hw(dev_id, vchan); /* allow delay to ensure jobs are completed */
		status_count += n;
		if (n != 1)
			ERR_RETURN("Error: unexpected number of completions received, %u, not 1\n",
					n);
	}

	/* check for single failure */
	if (status[0] == RTE_DMA_STATUS_SUCCESSFUL)
		ERR_RETURN("Error, unexpected successful DMA transaction\n");
	for (j = 1; j < status_count; j++)
		if (status[j] != RTE_DMA_STATUS_SUCCESSFUL
				&& status[j] != RTE_DMA_STATUS_NOT_ATTEMPTED)
			ERR_RETURN("Error, unexpected DMA error reported\n");

	return 0;
}

static int
test_single_item_status_query_with_failure(int16_t dev_id, uint16_t vchan,
		struct rte_mbuf **srcs, struct rte_mbuf **dsts, unsigned int fail_idx)
{
	/* When error occurs just collect a single error using "completed_status()"
	 * before going to back to completed() calls
	 */
	enum rte_dma_status_code status;
	uint16_t invalid_addr_id = 0;
	uint16_t idx;
	uint16_t count, status_count, count2;
	unsigned int j;
	bool error = false;

	for (j = 0; j < COMP_BURST_SZ; j++) {
		int id = rte_dma_copy(dev_id, vchan,
				      (j == fail_idx ? 0 : rte_mbuf_data_iova(srcs[j])),
				      rte_mbuf_data_iova(dsts[j]), COPY_LEN, 0);
		if (id < 0)
			ERR_RETURN("Error with rte_dma_copy for buffer %u\n", j);
		if (j == fail_idx)
			invalid_addr_id = id;
	}
	rte_dma_submit(dev_id, vchan);
	await_hw(dev_id, vchan);

	/* get up to the error point */
	count = rte_dma_completed(dev_id, vchan, COMP_BURST_SZ, &idx, &error);
	if (count != fail_idx)
		ERR_RETURN("Error with rte_dma_completed for failure test. Got returned %u not %u.\n",
				count, fail_idx);
	if (!error)
		ERR_RETURN("Error, missing expected failed copy, %u. has_error is not set\n",
				fail_idx);
	if (idx != invalid_addr_id - 1)
		ERR_RETURN("Error, missing expected failed copy, %u. Got last idx %u, not %u\n",
				fail_idx, idx, invalid_addr_id - 1);

	/* get the error code */
	status_count = rte_dma_completed_status(dev_id, vchan, 1, &idx, &status);
	if (status_count != 1)
		ERR_RETURN("Error with completed_status calls for fail idx %u. Got %u not %u\n",
				fail_idx, status_count, COMP_BURST_SZ - count);
	if (status == RTE_DMA_STATUS_SUCCESSFUL)
		ERR_RETURN("Error with status returned for fail idx %u. First status was not failure\n",
				fail_idx);

	/* delay in case time needed after err handled to complete other jobs */
	await_hw(dev_id, vchan);

	/* get the rest of the completions without status */
	count2 = rte_dma_completed(dev_id, vchan, COMP_BURST_SZ, &idx, &error);
	if (error == true)
		ERR_RETURN("Error, got further errors post completed_status() call, for failure case %u.\n",
				fail_idx);
	if (count + status_count + count2 != COMP_BURST_SZ)
		ERR_RETURN("Error, incorrect number of completions received, got %u not %u\n",
				count + status_count + count2, COMP_BURST_SZ);

	return 0;
}

static int
test_multi_failure(int16_t dev_id, uint16_t vchan, struct rte_mbuf **srcs, struct rte_mbuf **dsts,
		const unsigned int *fail, size_t num_fail)
{
	/* test having multiple errors in one go */
	enum rte_dma_status_code status[COMP_BURST_SZ];
	unsigned int i, j;
	uint16_t count, err_count = 0;
	bool error = false;

	/* enqueue and gather completions in one go */
	for (j = 0; j < COMP_BURST_SZ; j++) {
		uintptr_t src = rte_mbuf_data_iova(srcs[j]);
		/* set up for failure if the current index is anywhere is the fails array */
		for (i = 0; i < num_fail; i++)
			if (j == fail[i])
				src = 0;

		int id = rte_dma_copy(dev_id, vchan, src, rte_mbuf_data_iova(dsts[j]),
				      COPY_LEN, 0);
		if (id < 0)
			ERR_RETURN("Error with rte_dma_copy for buffer %u\n", j);
	}
	rte_dma_submit(dev_id, vchan);
	await_hw(dev_id, vchan);

	count = rte_dma_completed_status(dev_id, vchan, COMP_BURST_SZ, NULL, status);
	while (count < COMP_BURST_SZ) {
		await_hw(dev_id, vchan);

		uint16_t ret = rte_dma_completed_status(dev_id, vchan, COMP_BURST_SZ - count,
				NULL, &status[count]);
		if (ret == 0)
			ERR_RETURN("Error getting all completions for jobs. Got %u of %u\n",
					count, COMP_BURST_SZ);
		count += ret;
	}
	for (i = 0; i < count; i++)
		if (status[i] != RTE_DMA_STATUS_SUCCESSFUL)
			err_count++;

	if (err_count != num_fail)
		ERR_RETURN("Error: Invalid number of failed completions returned, %u; expected %zu\n",
			err_count, num_fail);

	/* enqueue and gather completions in bursts, but getting errors one at a time */
	for (j = 0; j < COMP_BURST_SZ; j++) {
		uintptr_t src = rte_mbuf_data_iova(srcs[j]);
		/* set up for failure if the current index is anywhere is the fails array */
		for (i = 0; i < num_fail; i++)
			if (j == fail[i])
				src = 0;

		int id = rte_dma_copy(dev_id, vchan, src, rte_mbuf_data_iova(dsts[j]),
				      COPY_LEN, 0);
		if (id < 0)
			ERR_RETURN("Error with rte_dma_copy for buffer %u\n", j);
	}
	rte_dma_submit(dev_id, vchan);
	await_hw(dev_id, vchan);

	count = 0;
	err_count = 0;
	while (count + err_count < COMP_BURST_SZ) {
		count += rte_dma_completed(dev_id, vchan, COMP_BURST_SZ, NULL, &error);
		if (error) {
			uint16_t ret = rte_dma_completed_status(dev_id, vchan, 1,
					NULL, status);
			if (ret != 1)
				ERR_RETURN("Error getting error-status for completions\n");
			err_count += ret;
			await_hw(dev_id, vchan);
		}
	}
	if (err_count != num_fail)
		ERR_RETURN("Error: Incorrect number of failed completions received, got %u not %zu\n",
				err_count, num_fail);

	return 0;
}

static int
test_completion_status(int16_t dev_id, uint16_t vchan, bool fence)
{
	const unsigned int fail[] = {0, 7, 14, 15};
	struct rte_mbuf *srcs[COMP_BURST_SZ], *dsts[COMP_BURST_SZ];
	unsigned int i;

	for (i = 0; i < COMP_BURST_SZ; i++) {
		srcs[i] = rte_pktmbuf_alloc(pool);
		dsts[i] = rte_pktmbuf_alloc(pool);
	}

	for (i = 0; i < RTE_DIM(fail); i++) {
		if (test_failure_in_full_burst(dev_id, vchan, fence, srcs, dsts, fail[i]) < 0)
			return -1;

		if (test_individual_status_query_with_failure(dev_id, vchan, fence,
				srcs, dsts, fail[i]) < 0)
			return -1;

		/* test is run the same fenced, or unfenced, but no harm in running it twice */
		if (test_single_item_status_query_with_failure(dev_id, vchan,
				srcs, dsts, fail[i]) < 0)
			return -1;
	}

	if (test_multi_failure(dev_id, vchan, srcs, dsts, fail, RTE_DIM(fail)) < 0)
		return -1;

	for (i = 0; i < COMP_BURST_SZ; i++) {
		rte_pktmbuf_free(srcs[i]);
		rte_pktmbuf_free(dsts[i]);
	}
	return 0;
}

static int
test_completion_handling(int16_t dev_id, uint16_t vchan)
{
	return test_completion_status(dev_id, vchan, false)              /* without fences */
			|| test_completion_status(dev_id, vchan, true);  /* with fences */
}

static int
test_enqueue_fill(int16_t dev_id, uint16_t vchan)
{
	const unsigned int lengths[] = {8, 64, 1024, 50, 100, 89};
	struct rte_mbuf *dst;
	char *dst_data;
	uint64_t pattern = 0xfedcba9876543210;
	unsigned int i, j;

	dst = rte_pktmbuf_alloc(pool);
	if (dst == NULL)
		ERR_RETURN("Failed to allocate mbuf\n");
	dst_data = rte_pktmbuf_mtod(dst, char *);

	for (i = 0; i < RTE_DIM(lengths); i++) {
		/* reset dst_data */
		memset(dst_data, 0, rte_pktmbuf_data_len(dst));

		/* perform the fill operation */
		int id = rte_dma_fill(dev_id, vchan, pattern,
				rte_pktmbuf_iova(dst), lengths[i], RTE_DMA_OP_FLAG_SUBMIT);
		if (id < 0)
			ERR_RETURN("Error with rte_dma_fill\n");
		await_hw(dev_id, vchan);

		if (rte_dma_completed(dev_id, vchan, 1, NULL, NULL) != 1)
			ERR_RETURN("Error: fill operation failed (length: %u)\n", lengths[i]);
		/* check the data from the fill operation is correct */
		for (j = 0; j < lengths[i]; j++) {
			char pat_byte = ((char *)&pattern)[j % 8];
			if (dst_data[j] != pat_byte)
				ERR_RETURN("Error with fill operation (lengths = %u): got (%x), not (%x)\n",
						lengths[i], dst_data[j], pat_byte);
		}
		/* check that the data after the fill operation was not written to */
		for (; j < rte_pktmbuf_data_len(dst); j++)
			if (dst_data[j] != 0)
				ERR_RETURN("Error, fill operation wrote too far (lengths = %u): got (%x), not (%x)\n",
						lengths[i], dst_data[j], 0);
	}

	rte_pktmbuf_free(dst);
	return 0;
}

static int
test_burst_capacity(int16_t dev_id, uint16_t vchan)
{
#define CAP_TEST_BURST_SIZE	64
	const int ring_space = rte_dma_burst_capacity(dev_id, vchan);
	struct rte_mbuf *src, *dst;
	int i, j, iter;
	int cap, ret;
	bool dma_err;

	src = rte_pktmbuf_alloc(pool);
	dst = rte_pktmbuf_alloc(pool);

	/* to test capacity, we enqueue elements and check capacity is reduced
	 * by one each time - rebaselining the expected value after each burst
	 * as the capacity is only for a burst. We enqueue multiple bursts to
	 * fill up half the ring, before emptying it again. We do this multiple
	 * times to ensure that we get to test scenarios where we get ring
	 * wrap-around and wrap-around of the ids returned (at UINT16_MAX).
	 */
	for (iter = 0; iter < 2 * (((int)UINT16_MAX + 1) / ring_space); iter++) {
		for (i = 0; i < (ring_space / (2 * CAP_TEST_BURST_SIZE)) + 1; i++) {
			cap = rte_dma_burst_capacity(dev_id, vchan);

			for (j = 0; j < CAP_TEST_BURST_SIZE; j++) {
				ret = rte_dma_copy(dev_id, vchan, rte_pktmbuf_iova(src),
						rte_pktmbuf_iova(dst), COPY_LEN, 0);
				if (ret < 0)
					ERR_RETURN("Error with rte_dmadev_copy\n");

				if (rte_dma_burst_capacity(dev_id, vchan) != cap - (j + 1))
					ERR_RETURN("Error, ring capacity did not change as expected\n");
			}
			if (rte_dma_submit(dev_id, vchan) < 0)
				ERR_RETURN("Error, failed to submit burst\n");

			if (cap < rte_dma_burst_capacity(dev_id, vchan))
				ERR_RETURN("Error, avail ring capacity has gone up, not down\n");
		}
		await_hw(dev_id, vchan);

		for (i = 0; i < (ring_space / (2 * CAP_TEST_BURST_SIZE)) + 1; i++) {
			ret = rte_dma_completed(dev_id, vchan,
					CAP_TEST_BURST_SIZE, NULL, &dma_err);
			if (ret != CAP_TEST_BURST_SIZE || dma_err) {
				enum rte_dma_status_code status;

				rte_dma_completed_status(dev_id, vchan, 1, NULL, &status);
				ERR_RETURN("Error with rte_dmadev_completed, %u [expected: %u], dma_err = %d, i = %u, iter = %u, status = %u\n",
						ret, CAP_TEST_BURST_SIZE, dma_err, i, iter, status);
			}
		}
		cap = rte_dma_burst_capacity(dev_id, vchan);
		if (cap != ring_space)
			ERR_RETURN("Error, ring capacity has not reset to original value, got %u, expected %u\n",
					cap, ring_space);
	}

	rte_pktmbuf_free(src);
	rte_pktmbuf_free(dst);

	return 0;
}

static int
test_m2d_auto_free(int16_t dev_id, uint16_t vchan)
{
#define NR_MBUF 256
	struct rte_mempool_cache *cache;
	struct rte_mbuf *src[NR_MBUF];
	uint32_t buf_cnt1, buf_cnt2;
	struct rte_mempool_ops *ops;
	uint16_t nb_done = 0;
	bool dma_err = false;
	int retry = 100;
	int i, ret = 0;
	rte_iova_t dst;

	dst = (rte_iova_t)env_test_param[TEST_PARAM_REMOTE_ADDR];

	/* Capture buffer count before allocating source buffer. */
	cache = rte_mempool_default_cache(pool, rte_lcore_id());
	ops = rte_mempool_get_ops(pool->ops_index);
	buf_cnt1 = ops->get_count(pool) + cache->len;

	if (rte_pktmbuf_alloc_bulk(pool, src, NR_MBUF) != 0)
		ERR_RETURN("alloc src mbufs failed.\n");

	if ((buf_cnt1 - NR_MBUF) != (ops->get_count(pool) + cache->len)) {
		printf("Buffer count check failed.\n");
		ret = -1;
		goto done;
	}

	for (i = 0; i < NR_MBUF; i++) {
		ret = rte_dma_copy(dev_id, vchan, rte_mbuf_data_iova(src[i]), dst,
				   COPY_LEN, RTE_DMA_OP_FLAG_AUTO_FREE);

		if (ret < 0) {
			printf("rte_dma_copy returned error.\n");
			goto done;
		}
	}

	rte_dma_submit(dev_id, vchan);
	do {
		nb_done += rte_dma_completed(dev_id, vchan, (NR_MBUF - nb_done), NULL, &dma_err);
		if (dma_err)
			break;
		/* Sleep for 1 millisecond */
		rte_delay_us_sleep(1000);
	} while (retry-- && (nb_done < NR_MBUF));

	buf_cnt2 = ops->get_count(pool) + cache->len;
	if ((buf_cnt1 != buf_cnt2) || dma_err) {
		printf("Free mem to dev buffer test failed.\n");
		ret = -1;
	}

done:
	/* If the test passes source buffer will be freed in hardware. */
	if (ret < 0)
		rte_pktmbuf_free_bulk(&src[nb_done], (NR_MBUF - nb_done));

	return ret;
}

static int
prepare_m2d_auto_free(int16_t dev_id, uint16_t vchan)
{
	const struct rte_dma_vchan_conf qconf = {
		.direction = RTE_DMA_DIR_MEM_TO_DEV,
		.nb_desc = TEST_RINGSIZE,
		.auto_free.m2d.pool = pool,
		.dst_port.port_type = RTE_DMA_PORT_PCIE,
		.dst_port.pcie.coreid = 0,
	};

	/* Stop the device to reconfigure vchan. */
	if (rte_dma_stop(dev_id) < 0)
		ERR_RETURN("Error stopping device %u\n", dev_id);

	if (rte_dma_vchan_setup(dev_id, vchan, &qconf) < 0)
		ERR_RETURN("Error with queue configuration\n");

	if (rte_dma_start(dev_id) != 0)
		ERR_RETURN("Error with rte_dma_start()\n");

	return 0;
}

static int
test_enq_deq_ops(int16_t dev_id, uint16_t vchan)
{
#define BURST_SIZE 16
#define ROUNDS	   2E7
#define CPY_LEN	   64
	struct rte_mempool *ops_pool, *pkt_pool;
	struct rte_mbuf *mbufs[BURST_SIZE * 2];
	struct rte_dma_op *ops[BURST_SIZE];
	uint64_t enq_lat, deq_lat, start;
	int ret, i, j, enq, deq, n, max;
	struct rte_dma_sge ssg, dsg;
	struct rte_dma_info info;
	uint64_t tenq, tdeq;

	memset(&info, 0, sizeof(info));
	ret = rte_dma_info_get(dev_id, &info);
	if (ret != 0)
		ERR_RETURN("Error with rte_dma_info_get()\n");

	pkt_pool = rte_pktmbuf_pool_create("pkt_pool", info.max_desc * 2, 0, 0,
					   CPY_LEN + RTE_PKTMBUF_HEADROOM, rte_socket_id());
	if (pkt_pool == NULL)
		ERR_RETURN("Error creating pkt pool\n");

	ops_pool = rte_mempool_create("ops_pool", info.max_desc,
				      sizeof(struct rte_dma_op) + (sizeof(struct rte_dma_sge) * 2),
				      0, 0, NULL, NULL, NULL, NULL, rte_socket_id(), 0);
	if (ops_pool == NULL)
		ERR_RETURN("Error creating ops pool\n");

	max = info.max_desc - BURST_SIZE;
	tenq = 0;
	tdeq = 0;
	enq_lat = 0;
	deq_lat = 0;

	for (i = 0; i < ROUNDS / max; i++) {
		n = 0;
		while (n != max) {
			if (rte_mempool_get_bulk(ops_pool, (void **)ops, BURST_SIZE) != 0)
				continue;

			if (rte_pktmbuf_alloc_bulk(pkt_pool, mbufs, BURST_SIZE * 2) != 0)
				ERR_RETURN("Error allocating mbufs %d\n", n);

			for (j = 0; j < BURST_SIZE; j++) {
				ops[j]->src_dst_seg[0].addr = rte_pktmbuf_iova(mbufs[j]);
				ops[j]->src_dst_seg[1].addr =
					rte_pktmbuf_iova(mbufs[j + BURST_SIZE]);
				ops[j]->src_dst_seg[0].length = CPY_LEN;
				ops[j]->src_dst_seg[1].length = CPY_LEN;

				ops[j]->nb_src = 1;
				ops[j]->nb_dst = 1;
				ops[j]->user_meta = (uint64_t)mbufs[j];
				ops[j]->event_meta = (uint64_t)mbufs[j + BURST_SIZE];

				memset((void *)(uintptr_t)ops[j]->src_dst_seg[0].addr,
				       rte_rand() & 0xFF, CPY_LEN);
				memset((void *)(uintptr_t)ops[j]->src_dst_seg[1].addr, 0, CPY_LEN);
			}

			start = rte_rdtsc_precise();
			enq = rte_dma_enqueue_ops(dev_id, vchan, ops, BURST_SIZE);
			while (enq != BURST_SIZE) {
				enq += rte_dma_enqueue_ops(dev_id, vchan, ops + enq,
							   BURST_SIZE - enq);
			}

			enq_lat += rte_rdtsc_precise() - start;
			n += enq;
		}
		tenq += n;

		memset(ops, 0, sizeof(ops));
		n = 0;
		while (n != max) {
			start = rte_rdtsc_precise();
			deq = rte_dma_dequeue_ops(dev_id, vchan, ops, BURST_SIZE);
			while (deq != BURST_SIZE) {
				deq += rte_dma_dequeue_ops(dev_id, vchan, ops + deq,
							   BURST_SIZE - deq);
			}
			n += deq;
			deq_lat += rte_rdtsc_precise() - start;

			for (j = 0; j < deq; j++) {
				/* check the data is correct */
				ssg = ops[j]->src_dst_seg[0];
				dsg = ops[j]->src_dst_seg[1];
				if (memcmp((void *)(uintptr_t)ssg.addr, (void *)(uintptr_t)dsg.addr,
					   ssg.length) != 0)
					ERR_RETURN("Error with copy operation\n");
				rte_pktmbuf_free((struct rte_mbuf *)(uintptr_t)ops[j]->user_meta);
				rte_pktmbuf_free((struct rte_mbuf *)(uintptr_t)ops[j]->event_meta);
			}
			rte_mempool_put_bulk(ops_pool, (void **)ops, BURST_SIZE);
		}
		tdeq += n;

		printf("\rEnqueued %" PRIu64 " Latency %.3f Dequeued %" PRIu64 " Latency %.3f",
		       tenq, (double)enq_lat / tenq, tdeq, (double)deq_lat / tdeq);
	}
	printf("\n");

	rte_mempool_free(pkt_pool);
	rte_mempool_free(ops_pool);

	return 0;
}

static int
prepare_enq_deq_ops(int16_t dev_id, uint16_t vchan)
{
	const struct rte_dma_conf conf = {.nb_vchans = 1, .flags = RTE_DMA_CFG_FLAG_ENQ_DEQ};
	struct rte_dma_vchan_conf qconf;
	struct rte_dma_info info;

	memset(&qconf, 0, sizeof(qconf));
	memset(&info, 0, sizeof(info));

	int ret = rte_dma_info_get(dev_id, &info);
	if (ret != 0)
		ERR_RETURN("Error with rte_dma_info_get()\n");

	qconf.direction = RTE_DMA_DIR_MEM_TO_MEM;
	qconf.nb_desc = info.max_desc;

	if (rte_dma_stop(dev_id) < 0)
		ERR_RETURN("Error stopping device %u\n", dev_id);
	if (rte_dma_configure(dev_id, &conf) != 0)
		ERR_RETURN("Error with rte_dma_configure()\n");
	if (rte_dma_vchan_setup(dev_id, vchan, &qconf) < 0)
		ERR_RETURN("Error with queue configuration\n");
	if (rte_dma_start(dev_id) != 0)
		ERR_RETURN("Error with rte_dma_start()\n");

	return 0;
}

static int
test_dmadev_sg_copy_setup(void)
{
	int ret = TEST_SUCCESS;

	if ((info.dev_capa & RTE_DMA_CAPA_OPS_COPY_SG) == 0)
		return TEST_SKIPPED;

	return ret;
}

static int
test_dmadev_burst_setup(void)
{
	if (rte_dma_burst_capacity(test_dev_id, vchan) < 64) {
		RTE_LOG(ERR, USER1,
			"DMA Dev %u: insufficient burst capacity (64 required), skipping tests\n",
			test_dev_id);
		return TEST_SKIPPED;
	}

	return TEST_SUCCESS;
}

static int
test_dmadev_err_handling_setup(void)
{
	int ret = TEST_SKIPPED;

	/* to test error handling we can provide null pointers for source or dest in copies. This
	 * requires VA mode in DPDK, since NULL(0) is a valid physical address.
	 * We also need hardware that can report errors back.
	 */
	if (rte_eal_iova_mode() != RTE_IOVA_VA)
		RTE_LOG(ERR, USER1,
			"DMA Dev %u: DPDK not in VA mode, skipping error handling tests\n",
			test_dev_id);
	else if ((info.dev_capa & RTE_DMA_CAPA_HANDLES_ERRORS) == 0)
		RTE_LOG(ERR, USER1,
			"DMA Dev %u: device does not report errors, skipping error handling tests\n",
			test_dev_id);
	else
		ret = TEST_SUCCESS;

	return ret;
}

static int
test_dmadev_fill_setup(void)
{
	int ret = TEST_SUCCESS;

	if ((info.dev_capa & RTE_DMA_CAPA_OPS_FILL) == 0) {
		RTE_LOG(ERR, USER1,
			"DMA Dev %u: No device fill support, skipping fill tests\n", test_dev_id);
		ret = TEST_SKIPPED;
	}

	return ret;
}

static int
test_dmadev_autofree_setup(void)
{
	int ret = TEST_SKIPPED;

	if ((info.dev_capa & RTE_DMA_CAPA_M2D_AUTO_FREE) &&
	    dma_add_test[TEST_M2D_AUTO_FREE].enabled == true) {
		if (prepare_m2d_auto_free(test_dev_id, vchan) != 0)
			return ret;

		ret = TEST_SUCCESS;
	}

	return ret;
}

static int
test_dmadev_enq_deq_setup(void)
{
	int ret = TEST_SKIPPED;

	if ((info.dev_capa & RTE_DMA_CAPA_OPS_ENQ_DEQ)) {
		if (prepare_enq_deq_ops(test_dev_id, vchan) != 0)
			return ret;
		ret = TEST_SUCCESS;
	}

	return ret;
}

static int
test_dmadev_setup(void)
{
	int16_t dev_id = test_dev_id;
	struct rte_dma_stats stats;
	const struct rte_dma_conf conf = { .nb_vchans = 1};
	struct rte_dma_vchan_conf qconf = {
			.direction = RTE_DMA_DIR_MEM_TO_MEM,
			.nb_desc = TEST_RINGSIZE,
	};
	int ret;

	ret = rte_dma_info_get(dev_id, &info);
	if (ret != 0)
		ERR_RETURN("Error with rte_dma_info_get()\n");

	if (info.max_vchans < 1)
		ERR_RETURN("Error, no channels available on device id %u\n", dev_id);

	if (rte_dma_configure(dev_id, &conf) != 0)
		ERR_RETURN("Error with rte_dma_configure()\n");

	if (qconf.nb_desc < info.min_desc)
		qconf.nb_desc = info.min_desc;
	if (qconf.nb_desc > info.max_desc)
		qconf.nb_desc = info.max_desc;
	if (rte_dma_vchan_setup(dev_id, vchan, &qconf) < 0)
		ERR_RETURN("Error with queue configuration\n");

	ret = rte_dma_info_get(dev_id, &info);
	if (ret != 0 || info.nb_vchans != 1)
		ERR_RETURN("Error, no configured queues reported on device id %u\n", dev_id);

	if (rte_dma_start(dev_id) != 0)
		ERR_RETURN("Error with rte_dma_start()\n");

	if (rte_dma_stats_get(dev_id, vchan, &stats) != 0)
		ERR_RETURN("Error with rte_dma_stats_get()\n");

	if (rte_dma_burst_capacity(dev_id, vchan) < 32)
		ERR_RETURN("Error: Device does not have sufficient burst capacity to run tests");

	if (stats.completed != 0 || stats.submitted != 0 || stats.errors != 0)
		ERR_RETURN("Error device stats are not all zero: completed = %"PRIu64", "
				"submitted = %"PRIu64", errors = %"PRIu64"\n",
				stats.completed, stats.submitted, stats.errors);
	id_count = 0;

	/* create a mempool for running tests */
	pool = rte_pktmbuf_pool_create("TEST_DMADEV_POOL",
			TEST_RINGSIZE * 2, /* n == num elements */
			32,  /* cache size */
			0,   /* priv size */
			COPY_LEN + RTE_PKTMBUF_HEADROOM, /* data room size */
			info.numa_node);
	if (pool == NULL)
		ERR_RETURN("Error with mempool creation\n");

	check_err_stats = false;
	vchan = 0;

	return 0;
}

static void
test_dmadev_teardown(void)
{
	rte_mempool_free(pool);
	rte_dma_stop(test_dev_id);
	rte_dma_stats_reset(test_dev_id, vchan);
	test_dev_id = -EINVAL;
}

static int
test_dmadev_instance(int16_t dev_id)
{
	struct rte_dma_info dev_info;
	enum {
		  TEST_COPY = 0,
		  TEST_COPY_SG,
		  TEST_START,
		  TEST_BURST,
		  TEST_ERR,
		  TEST_FILL,
		  TEST_M2D,
		  TEST_ENQ_DEQ,
		  TEST_END
	};

	static struct runtest_param param[] = {
		{"copy", test_enqueue_copies, 640},
		{"sg_copy", test_enqueue_sg_copies, 1},
		{"stop_start", test_stop_start, 1},
		{"burst_capacity", test_burst_capacity, 1},
		{"error_handling", test_completion_handling, 1},
		{"fill", test_enqueue_fill, 1},
		{"m2d_auto_free", test_m2d_auto_free, 128},
		{"dma_enq_deq", test_enq_deq_ops, 1},
	};

	static struct unit_test_suite ts = {
		.suite_name = "DMA dev instance testsuite",
		.setup = test_dmadev_setup,
		.teardown = test_dmadev_teardown,
		.unit_test_cases = {
			TEST_CASE_NAMED_WITH_DATA("copy",
				NULL, NULL,
				runtest, &param[TEST_COPY]),
			TEST_CASE_NAMED_WITH_DATA("sg_copy",
				test_dmadev_sg_copy_setup, NULL,
				runtest, &param[TEST_COPY_SG]),
			TEST_CASE_NAMED_WITH_DATA("stop_start",
				NULL, NULL,
				runtest, &param[TEST_START]),
			TEST_CASE_NAMED_WITH_DATA("burst_capacity",
				test_dmadev_burst_setup, NULL,
				runtest, &param[TEST_BURST]),
			TEST_CASE_NAMED_WITH_DATA("error_handling",
				test_dmadev_err_handling_setup, NULL,
				runtest, &param[TEST_ERR]),
			TEST_CASE_NAMED_WITH_DATA("fill",
				test_dmadev_fill_setup, NULL,
				runtest, &param[TEST_FILL]),
			TEST_CASE_NAMED_WITH_DATA("m2d_autofree",
				test_dmadev_autofree_setup, NULL,
				runtest, &param[TEST_M2D]),
			TEST_CASE_NAMED_WITH_DATA("dma_enq_deq",
				test_dmadev_enq_deq_setup, NULL,
				runtest, &param[TEST_ENQ_DEQ]),
			TEST_CASES_END()
		}
	};

	int ret;

	if (rte_dma_info_get(dev_id, &dev_info) < 0)
		return TEST_SKIPPED;

	test_dev_id = dev_id;
	printf("\n### Test dmadev instance %u [%s]\n",
		   test_dev_id, dev_info.dev_name);

	ret = unit_test_suite_runner(&ts);
	test_dev_id = -EINVAL;

	return ret;
}

static void
parse_dma_env_var(void)
{
	char *dma_env_param_str = getenv("DPDK_ADD_DMA_TEST_PARAM");
	char *dma_env_test_str = getenv("DPDK_ADD_DMA_TEST");
	char *params[32] = {0};
	char *tests[32] = {0};
	char *var[2] = {0};
	int n_var = 0;
	int i, j;

	/* Additional test from commandline. */
	if (dma_env_test_str && strlen(dma_env_test_str) > 0) {
		n_var = rte_strsplit(dma_env_test_str, strlen(dma_env_test_str), tests,
				RTE_DIM(tests), ',');
		for (i = 0; i < n_var; i++) {
			for (j = 0; j < TEST_MAX; j++) {
				if (!strcmp(tests[i], dma_add_test[j].name))
					dma_add_test[j].enabled = true;
			}
		}
	}

	/* Commandline variables for test */
	if (dma_env_param_str && strlen(dma_env_param_str) > 0) {
		n_var = rte_strsplit(dma_env_param_str, strlen(dma_env_param_str), params,
				       RTE_DIM(params), ',');
		for (i = 0; i < n_var; i++) {
			rte_strsplit(params[i], strlen(params[i]), var,	RTE_DIM(var), '=');
			for (j = 0; j < TEST_PARAM_MAX; j++) {
				if (!strcmp(var[0], dma_test_param[j]))
					env_test_param[j] = strtoul(var[1], NULL, 16);
			}
		}
	}
}

static int
test_dma(void)
{
	const char *pmd = "dma_skeleton";
	int i;

	parse_dma_env_var();

	/* attempt to create skeleton instance - ignore errors due to one being already present*/
	rte_vdev_init(pmd, NULL);

	if (rte_dma_count_avail() == 0)
		return TEST_SKIPPED;

	RTE_DMA_FOREACH_DEV(i) {
		if (test_dma_api(i) < 0)
			ERR_RETURN("Error performing API tests\n");

		if (test_dmadev_instance(i) < 0)
			ERR_RETURN("Error, test failure for device %d\n", i);
	}

	return 0;
}

REGISTER_DRIVER_TEST(dmadev_autotest, test_dma);
