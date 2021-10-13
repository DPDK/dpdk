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

#define COPY_LEN 1024

static struct rte_mempool *pool;
static uint16_t id_count;

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

static int
runtest(const char *printable, int (*test_fn)(int16_t dev_id, uint16_t vchan), int iterations,
		int16_t dev_id, uint16_t vchan, bool check_err_stats)
{
	struct rte_dma_stats stats;
	int i;

	rte_dma_stats_reset(dev_id, vchan);
	printf("DMA Dev %d: Running %s Tests %s\n", dev_id, printable,
			check_err_stats ? " " : "(errors expected)");
	for (i = 0; i < iterations; i++) {
		if (test_fn(dev_id, vchan) < 0)
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

static int
test_enqueue_copies(int16_t dev_id, uint16_t vchan)
{
	unsigned int i;
	uint16_t id;

	/* test doing a single copy */
	do {
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
		if (rte_dma_completed(dev_id, vchan, 1, &id, NULL) != 1)
			ERR_RETURN("Error with rte_dma_completed\n");

		if (id != id_count)
			ERR_RETURN("Error:incorrect job id received, %u [expected %u]\n",
					id, id_count);

		rte_pktmbuf_free(src);
		rte_pktmbuf_free(dst);

		/* now check completion returns nothing more */
		if (rte_dma_completed(dev_id, 0, 1, NULL, NULL) != 0)
			ERR_RETURN("Error with rte_dma_completed in empty check\n");

		id_count++;

	} while (0);

	/* test doing a multiple single copies */
	do {
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

	return 0;
}

static int
test_dmadev_instance(int16_t dev_id)
{
#define TEST_RINGSIZE 512
#define CHECK_ERRS    true
	struct rte_dma_stats stats;
	struct rte_dma_info info;
	const struct rte_dma_conf conf = { .nb_vchans = 1};
	const struct rte_dma_vchan_conf qconf = {
			.direction = RTE_DMA_DIR_MEM_TO_MEM,
			.nb_desc = TEST_RINGSIZE,
	};
	const int vchan = 0;

	printf("\n### Test dmadev instance %u [%s]\n",
			dev_id, rte_dma_devices[dev_id].data->dev_name);

	rte_dma_info_get(dev_id, &info);
	if (info.max_vchans < 1)
		ERR_RETURN("Error, no channels available on device id %u\n", dev_id);

	if (rte_dma_configure(dev_id, &conf) != 0)
		ERR_RETURN("Error with rte_dma_configure()\n");

	if (rte_dma_vchan_setup(dev_id, vchan, &qconf) < 0)
		ERR_RETURN("Error with queue configuration\n");

	rte_dma_info_get(dev_id, &info);
	if (info.nb_vchans != 1)
		ERR_RETURN("Error, no configured queues reported on device id %u\n", dev_id);

	if (rte_dma_start(dev_id) != 0)
		ERR_RETURN("Error with rte_dma_start()\n");

	if (rte_dma_stats_get(dev_id, vchan, &stats) != 0)
		ERR_RETURN("Error with rte_dma_stats_get()\n");

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
			2048, /* data room size */
			info.numa_node);
	if (pool == NULL)
		ERR_RETURN("Error with mempool creation\n");

	/* run the test cases, use many iterations to ensure UINT16_MAX id wraparound */
	if (runtest("copy", test_enqueue_copies, 640, dev_id, vchan, CHECK_ERRS) < 0)
		goto err;

	rte_mempool_free(pool);
	rte_dma_stop(dev_id);
	rte_dma_stats_reset(dev_id, vchan);
	return 0;

err:
	rte_mempool_free(pool);
	rte_dma_stop(dev_id);
	return -1;
}

static int
test_apis(void)
{
	const char *pmd = "dma_skeleton";
	int id;
	int ret;

	if (rte_vdev_init(pmd, NULL) < 0)
		return TEST_SKIPPED;
	id = rte_dma_get_dev_id_by_name(pmd);
	if (id < 0)
		return TEST_SKIPPED;
	printf("\n### Test dmadev infrastructure using skeleton driver\n");
	ret = test_dma_api(id);
	rte_vdev_uninit(pmd);

	return ret;
}

static int
test_dma(void)
{
	int i;

	/* basic sanity on dmadev infrastructure */
	if (test_apis() < 0)
		ERR_RETURN("Error performing API tests\n");

	if (rte_dma_count_avail() == 0)
		return TEST_SKIPPED;

	RTE_DMA_FOREACH_DEV(i)
		if (test_dmadev_instance(i) < 0)
			ERR_RETURN("Error, test failure for device %d\n", i);

	return 0;
}

REGISTER_TEST_COMMAND(dmadev_autotest, test_dma);
