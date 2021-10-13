/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 HiSilicon Limited
 */

#include <string.h>

#include <rte_cycles.h>
#include <rte_malloc.h>
#include <rte_test.h>
#include <rte_dmadev.h>

extern int test_dma_api(uint16_t dev_id);

#define DMA_TEST_API_RUN(test) \
	testsuite_run_test(test, #test)

#define TEST_MEMCPY_SIZE	1024
#define TEST_WAIT_US_VAL	50000

#define TEST_SUCCESS 0
#define TEST_FAILED  -1

static int16_t test_dev_id;
static int16_t invalid_dev_id;

static char *src;
static char *dst;

static int total;
static int passed;
static int failed;

static int
testsuite_setup(int16_t dev_id)
{
	test_dev_id = dev_id;
	invalid_dev_id = -1;

	src = rte_malloc("dmadev_test_src", TEST_MEMCPY_SIZE, 0);
	if (src == NULL)
		return -ENOMEM;
	dst = rte_malloc("dmadev_test_dst", TEST_MEMCPY_SIZE, 0);
	if (dst == NULL) {
		rte_free(src);
		src = NULL;
		return -ENOMEM;
	}

	total = 0;
	passed = 0;
	failed = 0;

	/* Set dmadev log level to critical to suppress unnecessary output
	 * during API tests.
	 */
	rte_log_set_level_pattern("lib.dmadev", RTE_LOG_CRIT);

	return 0;
}

static void
testsuite_teardown(void)
{
	rte_free(src);
	src = NULL;
	rte_free(dst);
	dst = NULL;
	/* Ensure the dmadev is stopped. */
	rte_dma_stop(test_dev_id);

	rte_log_set_level_pattern("lib.dmadev", RTE_LOG_INFO);
}

static void
testsuite_run_test(int (*test)(void), const char *name)
{
	int ret = 0;

	if (test) {
		ret = test();
		if (ret < 0) {
			failed++;
			printf("%s Failed\n", name);
		} else {
			passed++;
			printf("%s Passed\n", name);
		}
	}

	total++;
}

static int
test_dma_get_dev_id_by_name(void)
{
	int ret = rte_dma_get_dev_id_by_name("invalid_dmadev_device");
	RTE_TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);
	return TEST_SUCCESS;
}

static int
test_dma_is_valid_dev(void)
{
	int ret;
	ret = rte_dma_is_valid(invalid_dev_id);
	RTE_TEST_ASSERT(ret == false, "Expected false for invalid dev id");
	ret = rte_dma_is_valid(test_dev_id);
	RTE_TEST_ASSERT(ret == true, "Expected true for valid dev id");
	return TEST_SUCCESS;
}

static int
test_dma_count(void)
{
	uint16_t count = rte_dma_count_avail();
	RTE_TEST_ASSERT(count > 0, "Invalid dmadev count %u", count);
	return TEST_SUCCESS;
}

static int
test_dma_info_get(void)
{
	struct rte_dma_info info =  { 0 };
	int ret;

	ret = rte_dma_info_get(invalid_dev_id, &info);
	RTE_TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);
	ret = rte_dma_info_get(test_dev_id, NULL);
	RTE_TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);
	ret = rte_dma_info_get(test_dev_id, &info);
	RTE_TEST_ASSERT_SUCCESS(ret, "Failed to obtain device info");

	return TEST_SUCCESS;
}

static int
test_dma_configure(void)
{
	struct rte_dma_conf conf = { 0 };
	struct rte_dma_info info = { 0 };
	int ret;

	/* Check for invalid parameters */
	ret = rte_dma_configure(invalid_dev_id, &conf);
	RTE_TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);
	ret = rte_dma_configure(test_dev_id, NULL);
	RTE_TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);

	/* Check for nb_vchans == 0 */
	memset(&conf, 0, sizeof(conf));
	ret = rte_dma_configure(test_dev_id, &conf);
	RTE_TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);

	/* Check for conf.nb_vchans > info.max_vchans */
	ret = rte_dma_info_get(test_dev_id, &info);
	RTE_TEST_ASSERT_SUCCESS(ret, "Failed to obtain device info");
	memset(&conf, 0, sizeof(conf));
	conf.nb_vchans = info.max_vchans + 1;
	ret = rte_dma_configure(test_dev_id, &conf);
	RTE_TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);

	/* Check enable silent mode */
	memset(&conf, 0, sizeof(conf));
	conf.nb_vchans = info.max_vchans;
	conf.enable_silent = true;
	ret = rte_dma_configure(test_dev_id, &conf);
	RTE_TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);

	/* Configure success */
	memset(&conf, 0, sizeof(conf));
	conf.nb_vchans = info.max_vchans;
	ret = rte_dma_configure(test_dev_id, &conf);
	RTE_TEST_ASSERT_SUCCESS(ret, "Failed to configure dmadev, %d", ret);

	/* Check configure success */
	ret = rte_dma_info_get(test_dev_id, &info);
	RTE_TEST_ASSERT_SUCCESS(ret, "Failed to obtain device info");
	RTE_TEST_ASSERT_EQUAL(conf.nb_vchans, info.nb_vchans,
			      "Configure nb_vchans not match");

	return TEST_SUCCESS;
}

static int
check_direction(void)
{
	struct rte_dma_vchan_conf vchan_conf;
	int ret;

	/* Check for direction */
	memset(&vchan_conf, 0, sizeof(vchan_conf));
	vchan_conf.direction = RTE_DMA_DIR_DEV_TO_DEV + 1;
	ret = rte_dma_vchan_setup(test_dev_id, 0, &vchan_conf);
	RTE_TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);
	vchan_conf.direction = RTE_DMA_DIR_MEM_TO_MEM - 1;
	ret = rte_dma_vchan_setup(test_dev_id, 0, &vchan_conf);
	RTE_TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);

	/* Check for direction and dev_capa combination */
	memset(&vchan_conf, 0, sizeof(vchan_conf));
	vchan_conf.direction = RTE_DMA_DIR_MEM_TO_DEV;
	ret = rte_dma_vchan_setup(test_dev_id, 0, &vchan_conf);
	RTE_TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);
	vchan_conf.direction = RTE_DMA_DIR_DEV_TO_MEM;
	ret = rte_dma_vchan_setup(test_dev_id, 0, &vchan_conf);
	RTE_TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);
	vchan_conf.direction = RTE_DMA_DIR_DEV_TO_DEV;
	ret = rte_dma_vchan_setup(test_dev_id, 0, &vchan_conf);
	RTE_TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);

	return 0;
}

static int
check_port_type(struct rte_dma_info *dev_info)
{
	struct rte_dma_vchan_conf vchan_conf;
	int ret;

	/* Check src port type validation */
	memset(&vchan_conf, 0, sizeof(vchan_conf));
	vchan_conf.direction = RTE_DMA_DIR_MEM_TO_MEM;
	vchan_conf.nb_desc = dev_info->min_desc;
	vchan_conf.src_port.port_type = RTE_DMA_PORT_PCIE;
	ret = rte_dma_vchan_setup(test_dev_id, 0, &vchan_conf);
	RTE_TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);

	/* Check dst port type validation */
	memset(&vchan_conf, 0, sizeof(vchan_conf));
	vchan_conf.direction = RTE_DMA_DIR_MEM_TO_MEM;
	vchan_conf.nb_desc = dev_info->min_desc;
	vchan_conf.dst_port.port_type = RTE_DMA_PORT_PCIE;
	ret = rte_dma_vchan_setup(test_dev_id, 0, &vchan_conf);
	RTE_TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);

	return 0;
}

static int
test_dma_vchan_setup(void)
{
	struct rte_dma_vchan_conf vchan_conf = { 0 };
	struct rte_dma_conf dev_conf = { 0 };
	struct rte_dma_info dev_info = { 0 };
	int ret;

	/* Check for invalid parameters */
	ret = rte_dma_vchan_setup(invalid_dev_id, 0, &vchan_conf);
	RTE_TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);
	ret = rte_dma_vchan_setup(test_dev_id, 0, NULL);
	RTE_TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);
	ret = rte_dma_vchan_setup(test_dev_id, 0, &vchan_conf);
	RTE_TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);

	/* Make sure configure success */
	ret = rte_dma_info_get(test_dev_id, &dev_info);
	RTE_TEST_ASSERT_SUCCESS(ret, "Failed to obtain device info");
	dev_conf.nb_vchans = dev_info.max_vchans;
	ret = rte_dma_configure(test_dev_id, &dev_conf);
	RTE_TEST_ASSERT_SUCCESS(ret, "Failed to configure dmadev, %d", ret);

	/* Check for invalid vchan */
	ret = rte_dma_vchan_setup(test_dev_id, dev_conf.nb_vchans, &vchan_conf);
	RTE_TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);

	/* Check for direction */
	ret = check_direction();
	RTE_TEST_ASSERT_SUCCESS(ret, "Failed to check direction");

	/* Check for nb_desc validation */
	memset(&vchan_conf, 0, sizeof(vchan_conf));
	vchan_conf.direction = RTE_DMA_DIR_MEM_TO_MEM;
	vchan_conf.nb_desc = dev_info.min_desc - 1;
	ret = rte_dma_vchan_setup(test_dev_id, 0, &vchan_conf);
	RTE_TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);
	vchan_conf.nb_desc = dev_info.max_desc + 1;
	ret = rte_dma_vchan_setup(test_dev_id, 0, &vchan_conf);
	RTE_TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);

	/* Check port type */
	ret = check_port_type(&dev_info);
	RTE_TEST_ASSERT_SUCCESS(ret, "Failed to check port type");

	/* Check vchan setup success */
	memset(&vchan_conf, 0, sizeof(vchan_conf));
	vchan_conf.direction = RTE_DMA_DIR_MEM_TO_MEM;
	vchan_conf.nb_desc = dev_info.min_desc;
	ret = rte_dma_vchan_setup(test_dev_id, 0, &vchan_conf);
	RTE_TEST_ASSERT_SUCCESS(ret, "Failed to setup vchan, %d", ret);

	return TEST_SUCCESS;
}

static int
setup_one_vchan(void)
{
	struct rte_dma_vchan_conf vchan_conf = { 0 };
	struct rte_dma_info dev_info = { 0 };
	struct rte_dma_conf dev_conf = { 0 };
	int ret;

	ret = rte_dma_info_get(test_dev_id, &dev_info);
	RTE_TEST_ASSERT_SUCCESS(ret, "Failed to obtain device info, %d", ret);
	dev_conf.nb_vchans = dev_info.max_vchans;
	ret = rte_dma_configure(test_dev_id, &dev_conf);
	RTE_TEST_ASSERT_SUCCESS(ret, "Failed to configure, %d", ret);
	vchan_conf.direction = RTE_DMA_DIR_MEM_TO_MEM;
	vchan_conf.nb_desc = dev_info.min_desc;
	ret = rte_dma_vchan_setup(test_dev_id, 0, &vchan_conf);
	RTE_TEST_ASSERT_SUCCESS(ret, "Failed to setup vchan, %d", ret);

	return TEST_SUCCESS;
}

static int
test_dma_start_stop(void)
{
	struct rte_dma_vchan_conf vchan_conf = { 0 };
	struct rte_dma_conf dev_conf = { 0 };
	int ret;

	/* Check for invalid parameters */
	ret = rte_dma_start(invalid_dev_id);
	RTE_TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);
	ret = rte_dma_stop(invalid_dev_id);
	RTE_TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);

	/* Setup one vchan for later test */
	ret = setup_one_vchan();
	RTE_TEST_ASSERT_SUCCESS(ret, "Failed to setup one vchan, %d", ret);

	ret = rte_dma_start(test_dev_id);
	RTE_TEST_ASSERT_SUCCESS(ret, "Failed to start, %d", ret);

	/* Check reconfigure and vchan setup when device started */
	ret = rte_dma_configure(test_dev_id, &dev_conf);
	RTE_TEST_ASSERT(ret == -EBUSY, "Failed to configure, %d", ret);
	ret = rte_dma_vchan_setup(test_dev_id, 0, &vchan_conf);
	RTE_TEST_ASSERT(ret == -EBUSY, "Failed to setup vchan, %d", ret);

	ret = rte_dma_stop(test_dev_id);
	RTE_TEST_ASSERT_SUCCESS(ret, "Failed to stop, %d", ret);

	return TEST_SUCCESS;
}

static int
test_dma_stats(void)
{
	struct rte_dma_info dev_info = { 0 };
	struct rte_dma_stats stats = { 0 };
	int ret;

	/* Check for invalid parameters */
	ret = rte_dma_stats_get(invalid_dev_id, 0, &stats);
	RTE_TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);
	ret = rte_dma_stats_get(invalid_dev_id, 0, NULL);
	RTE_TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);
	ret = rte_dma_stats_reset(invalid_dev_id, 0);
	RTE_TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);

	/* Setup one vchan for later test */
	ret = setup_one_vchan();
	RTE_TEST_ASSERT_SUCCESS(ret, "Failed to setup one vchan, %d", ret);

	/* Check for invalid vchan */
	ret = rte_dma_info_get(test_dev_id, &dev_info);
	RTE_TEST_ASSERT_SUCCESS(ret, "Failed to obtain device info, %d", ret);
	ret = rte_dma_stats_get(test_dev_id, dev_info.max_vchans, &stats);
	RTE_TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);
	ret = rte_dma_stats_reset(test_dev_id, dev_info.max_vchans);
	RTE_TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);

	/* Check for valid vchan */
	ret = rte_dma_stats_get(test_dev_id, 0, &stats);
	RTE_TEST_ASSERT_SUCCESS(ret, "Failed to get stats, %d", ret);
	ret = rte_dma_stats_get(test_dev_id, RTE_DMA_ALL_VCHAN, &stats);
	RTE_TEST_ASSERT_SUCCESS(ret, "Failed to get all stats, %d", ret);
	ret = rte_dma_stats_reset(test_dev_id, 0);
	RTE_TEST_ASSERT_SUCCESS(ret, "Failed to reset stats, %d", ret);
	ret = rte_dma_stats_reset(test_dev_id, RTE_DMA_ALL_VCHAN);
	RTE_TEST_ASSERT_SUCCESS(ret, "Failed to reset all stats, %d", ret);

	return TEST_SUCCESS;
}

static int
test_dma_dump(void)
{
	int ret;

	/* Check for invalid parameters */
	ret = rte_dma_dump(invalid_dev_id, stderr);
	RTE_TEST_ASSERT(ret == -EINVAL, "Excepted -EINVAL, %d", ret);
	ret = rte_dma_dump(test_dev_id, NULL);
	RTE_TEST_ASSERT(ret == -EINVAL, "Excepted -EINVAL, %d", ret);

	return TEST_SUCCESS;
}

static void
setup_memory(void)
{
	int i;

	for (i = 0; i < TEST_MEMCPY_SIZE; i++)
		src[i] = (char)i;
	memset(dst, 0, TEST_MEMCPY_SIZE);
}

static int
verify_memory(void)
{
	int i;

	for (i = 0; i < TEST_MEMCPY_SIZE; i++) {
		if (src[i] == dst[i])
			continue;
		RTE_TEST_ASSERT_EQUAL(src[i], dst[i],
			"Failed to copy memory, %d %d", src[i], dst[i]);
	}

	return 0;
}

static int
test_dma_completed(void)
{
	uint16_t last_idx = 1;
	bool has_error = true;
	uint16_t cpl_ret;
	int ret;

	/* Setup one vchan for later test */
	ret = setup_one_vchan();
	RTE_TEST_ASSERT_SUCCESS(ret, "Failed to setup one vchan, %d", ret);

	ret = rte_dma_start(test_dev_id);
	RTE_TEST_ASSERT_SUCCESS(ret, "Failed to start, %d", ret);

	setup_memory();

	/* Check enqueue without submit */
	ret = rte_dma_copy(test_dev_id, 0, (rte_iova_t)src, (rte_iova_t)dst,
			   TEST_MEMCPY_SIZE, 0);
	RTE_TEST_ASSERT_EQUAL(ret, 0, "Failed to enqueue copy, %d", ret);
	rte_delay_us_sleep(TEST_WAIT_US_VAL);
	cpl_ret = rte_dma_completed(test_dev_id, 0, 1, &last_idx, &has_error);
	RTE_TEST_ASSERT_EQUAL(cpl_ret, 0, "Failed to get completed");

	/* Check add submit */
	ret = rte_dma_submit(test_dev_id, 0);
	RTE_TEST_ASSERT_SUCCESS(ret, "Failed to submit, %d", ret);
	rte_delay_us_sleep(TEST_WAIT_US_VAL);
	cpl_ret = rte_dma_completed(test_dev_id, 0, 1, &last_idx, &has_error);
	RTE_TEST_ASSERT_EQUAL(cpl_ret, 1, "Failed to get completed");
	RTE_TEST_ASSERT_EQUAL(last_idx, 0, "Last idx should be zero, %u",
				last_idx);
	RTE_TEST_ASSERT_EQUAL(has_error, false, "Should have no error");
	ret = verify_memory();
	RTE_TEST_ASSERT_SUCCESS(ret, "Failed to verify memory");

	setup_memory();

	/* Check for enqueue with submit */
	ret = rte_dma_copy(test_dev_id, 0, (rte_iova_t)src, (rte_iova_t)dst,
			   TEST_MEMCPY_SIZE, RTE_DMA_OP_FLAG_SUBMIT);
	RTE_TEST_ASSERT_EQUAL(ret, 1, "Failed to enqueue copy, %d", ret);
	rte_delay_us_sleep(TEST_WAIT_US_VAL);
	cpl_ret = rte_dma_completed(test_dev_id, 0, 1, &last_idx, &has_error);
	RTE_TEST_ASSERT_EQUAL(cpl_ret, 1, "Failed to get completed");
	RTE_TEST_ASSERT_EQUAL(last_idx, 1, "Last idx should be 1, %u",
				last_idx);
	RTE_TEST_ASSERT_EQUAL(has_error, false, "Should have no error");
	ret = verify_memory();
	RTE_TEST_ASSERT_SUCCESS(ret, "Failed to verify memory");

	/* Stop dmadev to make sure dmadev to a known state */
	ret = rte_dma_stop(test_dev_id);
	RTE_TEST_ASSERT_SUCCESS(ret, "Failed to stop, %d", ret);

	return TEST_SUCCESS;
}

static int
test_dma_completed_status(void)
{
	enum rte_dma_status_code status[1] = { 1 };
	uint16_t last_idx = 1;
	uint16_t cpl_ret, i;
	int ret;

	/* Setup one vchan for later test */
	ret = setup_one_vchan();
	RTE_TEST_ASSERT_SUCCESS(ret, "Failed to setup one vchan, %d", ret);

	ret = rte_dma_start(test_dev_id);
	RTE_TEST_ASSERT_SUCCESS(ret, "Failed to start, %d", ret);

	/* Check for enqueue with submit */
	ret = rte_dma_copy(test_dev_id, 0, (rte_iova_t)src, (rte_iova_t)dst,
			   TEST_MEMCPY_SIZE, RTE_DMA_OP_FLAG_SUBMIT);
	RTE_TEST_ASSERT_EQUAL(ret, 0, "Failed to enqueue copy, %d", ret);
	rte_delay_us_sleep(TEST_WAIT_US_VAL);
	cpl_ret = rte_dma_completed_status(test_dev_id, 0, 1, &last_idx,
					   status);
	RTE_TEST_ASSERT_EQUAL(cpl_ret, 1, "Failed to completed status");
	RTE_TEST_ASSERT_EQUAL(last_idx, 0, "Last idx should be zero, %u",
				last_idx);
	for (i = 0; i < RTE_DIM(status); i++)
		RTE_TEST_ASSERT_EQUAL(status[i], 0,
				"Failed to completed status, %d", status[i]);

	/* Check do completed status again */
	cpl_ret = rte_dma_completed_status(test_dev_id, 0, 1, &last_idx,
					   status);
	RTE_TEST_ASSERT_EQUAL(cpl_ret, 0, "Failed to completed status");

	/* Check for enqueue with submit again */
	ret = rte_dma_copy(test_dev_id, 0, (rte_iova_t)src, (rte_iova_t)dst,
			   TEST_MEMCPY_SIZE, RTE_DMA_OP_FLAG_SUBMIT);
	RTE_TEST_ASSERT_EQUAL(ret, 1, "Failed to enqueue copy, %d", ret);
	rte_delay_us_sleep(TEST_WAIT_US_VAL);
	cpl_ret = rte_dma_completed_status(test_dev_id, 0, 1, &last_idx,
					   status);
	RTE_TEST_ASSERT_EQUAL(cpl_ret, 1, "Failed to completed status");
	RTE_TEST_ASSERT_EQUAL(last_idx, 1, "Last idx should be 1, %u",
				last_idx);
	for (i = 0; i < RTE_DIM(status); i++)
		RTE_TEST_ASSERT_EQUAL(status[i], 0,
				"Failed to completed status, %d", status[i]);

	/* Stop dmadev to make sure dmadev to a known state */
	ret = rte_dma_stop(test_dev_id);
	RTE_TEST_ASSERT_SUCCESS(ret, "Failed to stop, %d", ret);

	return TEST_SUCCESS;
}

int
test_dma_api(uint16_t dev_id)
{
	int ret = testsuite_setup(dev_id);
	if (ret) {
		printf("testsuite setup fail!\n");
		return -1;
	}

	/* If the testcase exit successfully, ensure that the test dmadev exist
	 * and the dmadev is in the stopped state.
	 */
	DMA_TEST_API_RUN(test_dma_get_dev_id_by_name);
	DMA_TEST_API_RUN(test_dma_is_valid_dev);
	DMA_TEST_API_RUN(test_dma_count);
	DMA_TEST_API_RUN(test_dma_info_get);
	DMA_TEST_API_RUN(test_dma_configure);
	DMA_TEST_API_RUN(test_dma_vchan_setup);
	DMA_TEST_API_RUN(test_dma_start_stop);
	DMA_TEST_API_RUN(test_dma_stats);
	DMA_TEST_API_RUN(test_dma_dump);
	DMA_TEST_API_RUN(test_dma_completed);
	DMA_TEST_API_RUN(test_dma_completed_status);

	testsuite_teardown();

	printf("Total tests   : %d\n", total);
	printf("Passed        : %d\n", passed);
	printf("Failed        : %d\n", failed);

	if (failed)
		return -1;

	return 0;
};
