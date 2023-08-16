/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#include <rte_eal_trace.h>
#include <rte_lcore.h>
#include <rte_random.h>
#include <rte_trace.h>

#include "test.h"
#include "test_trace.h"

int app_dpdk_test_tp_count;

#ifdef RTE_EXEC_ENV_WINDOWS

static int
test_trace(void)
{
	printf("trace not supported on Windows, skipping test\n");
	return TEST_SKIPPED;
}

#else

static int32_t
test_trace_point_globbing(void)
{
	int rc;

	rc = rte_trace_pattern("app.dpdk.test*", false);
	if (rc != 1)
		goto failed;

	if (rte_trace_point_is_enabled(&__app_dpdk_test_tp))
		goto failed;

	rc = rte_trace_pattern("app.dpdk.test*", true);
	if (rc != 1)
		goto failed;

	if (!rte_trace_point_is_enabled(&__app_dpdk_test_tp))
		goto failed;

	rc = rte_trace_pattern("invalid_testpoint.*", true);
	if (rc != 0)
		goto failed;

	return TEST_SUCCESS;

failed:
	return TEST_FAILED;
}

static int32_t
test_trace_point_regex(void)
{
	int rc;

	rc = rte_trace_regexp("app.dpdk.test*", false);
	if (rc != 1)
		goto failed;

	if (rte_trace_point_is_enabled(&__app_dpdk_test_tp))
		goto failed;

	rc = rte_trace_regexp("app.dpdk.test*", true);
	if (rc != 1)
		goto failed;

	if (!rte_trace_point_is_enabled(&__app_dpdk_test_tp))
		goto failed;

	rc = rte_trace_regexp("invalid_testpoint.*", true);
	if (rc != 0)
		goto failed;

	return TEST_SUCCESS;

failed:
	return TEST_FAILED;
}

static int32_t
test_trace_point_disable_enable(void)
{
	int expected;
	int rc;

	/* At tp registration, the associated counter increases once. */
	expected = 1;
	TEST_ASSERT_EQUAL(app_dpdk_test_tp_count, expected,
		"Expecting %d, but got %d for app_dpdk_test_tp_count",
		expected, app_dpdk_test_tp_count);

	rc = rte_trace_point_disable(&__app_dpdk_test_tp);
	if (rc < 0)
		goto failed;

	if (rte_trace_point_is_enabled(&__app_dpdk_test_tp))
		goto failed;

	/* No emission expected */
	app_dpdk_test_tp("app.dpdk.test.tp");
	TEST_ASSERT_EQUAL(app_dpdk_test_tp_count, expected,
		"Expecting %d, but got %d for app_dpdk_test_tp_count",
		expected, app_dpdk_test_tp_count);

	rc = rte_trace_point_enable(&__app_dpdk_test_tp);
	if (rc < 0)
		goto failed;

	if (!rte_trace_point_is_enabled(&__app_dpdk_test_tp))
		goto failed;

	/* Emit the trace */
	app_dpdk_test_tp("app.dpdk.test.tp");
	expected++;
	TEST_ASSERT_EQUAL(app_dpdk_test_tp_count, expected,
		"Expecting %d, but got %d for app_dpdk_test_tp_count",
		expected, app_dpdk_test_tp_count);

	return TEST_SUCCESS;

failed:
	return TEST_FAILED;
}

static int
test_trace_mode(void)
{
	enum rte_trace_mode current;

	current = rte_trace_mode_get();

	rte_trace_mode_set(RTE_TRACE_MODE_DISCARD);
	if (rte_trace_mode_get() != RTE_TRACE_MODE_DISCARD)
		goto failed;

	rte_trace_mode_set(RTE_TRACE_MODE_OVERWRITE);
	if (rte_trace_mode_get() != RTE_TRACE_MODE_OVERWRITE)
		goto failed;

	rte_trace_mode_set(current);
	return TEST_SUCCESS;

failed:
	return TEST_FAILED;

}

static int
test_trace_points_lookup(void)
{
	rte_trace_point_t *trace;

	trace =  rte_trace_point_lookup("app.dpdk.test.tp");
	if (trace == NULL)
		goto fail;
	trace = rte_trace_point_lookup("this_trace_point_does_not_exist");
	if (trace != NULL)
		goto fail;

	return TEST_SUCCESS;
fail:
	return TEST_FAILED;
}

static int
test_fp_trace_points(void)
{
	/* Emit the FP trace */
	app_dpdk_test_fp();

	return TEST_SUCCESS;
}

static int
test_generic_trace_points(void)
{
	uint8_t arr[RTE_TRACE_BLOB_LEN_MAX];
	int tmp;
	int i;

	for (i = 0; i < RTE_TRACE_BLOB_LEN_MAX; i++)
		arr[i] = i;

	rte_eal_trace_generic_void();
	rte_eal_trace_generic_u64(0x10000000000000);
	rte_eal_trace_generic_u32(0x10000000);
	rte_eal_trace_generic_u16(0xffee);
	rte_eal_trace_generic_u8(0xc);
	rte_eal_trace_generic_i64(-1234);
	rte_eal_trace_generic_i32(-1234567);
	rte_eal_trace_generic_i16(12);
	rte_eal_trace_generic_i8(-3);
	rte_eal_trace_generic_int(3333333);
	rte_eal_trace_generic_long(333);
	rte_eal_trace_generic_float(20.45);
	rte_eal_trace_generic_double(20000.5000004);
	rte_eal_trace_generic_ptr(&tmp);
	rte_eal_trace_generic_str("my string");
	rte_eal_trace_generic_size_t(sizeof(void *));
	rte_eal_trace_generic_blob(arr, 0);
	rte_eal_trace_generic_blob(arr, 17);
	rte_eal_trace_generic_blob(arr, RTE_TRACE_BLOB_LEN_MAX);
	rte_eal_trace_generic_blob(arr, rte_rand() %
					RTE_TRACE_BLOB_LEN_MAX);
	RTE_EAL_TRACE_GENERIC_FUNC;

	return TEST_SUCCESS;
}

static int
test_trace_dump(void)
{
	rte_trace_dump(stdout);
	return 0;
}

static int
test_trace_metadata_dump(void)
{
	return rte_trace_metadata_dump(stdout);
}

static struct unit_test_suite trace_tests = {
	.suite_name = "trace autotest",
	.setup = NULL,
	.teardown = NULL,
	.unit_test_cases = {
		TEST_CASE(test_trace_mode),
		TEST_CASE(test_generic_trace_points),
		TEST_CASE(test_fp_trace_points),
		TEST_CASE(test_trace_point_disable_enable),
		TEST_CASE(test_trace_point_globbing),
		TEST_CASE(test_trace_point_regex),
		TEST_CASE(test_trace_points_lookup),
		TEST_CASE(test_trace_dump),
		TEST_CASE(test_trace_metadata_dump),
		TEST_CASES_END()
	}
};

static int
test_trace(void)
{
	return unit_test_suite_runner(&trace_tests);
}

#endif /* !RTE_EXEC_ENV_WINDOWS */

REGISTER_FAST_TEST(trace_autotest, true, true, test_trace);
