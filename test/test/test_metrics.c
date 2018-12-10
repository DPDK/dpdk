/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#include <rte_lcore.h>
#include <rte_metrics.h>

#include "test.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define	REG_METRIC_COUNT	6
#define	METRIC_LESSER_COUNT	3
#define	KEY	1
#define	VALUE	1

/* Initializes metric module. This function must be called
 * from a primary process before metrics are used
 */
static int
test_metrics_init(void)
{
	rte_metrics_init(rte_socket_id());
	return TEST_SUCCESS;
}

 /* Test Case to check failures when memzone init is not done */
static int
test_metrics_without_init(void)
{
	int err = 0;
	const uint64_t  value[REG_METRIC_COUNT] = {0};
	const char * const mnames[] = {
		"mean_bits_in", "mean_bits_out",
		"peak_bits_in", "peak_bits_out",
	};

	/* Failure Test: Checking for memzone initialization */
	err = rte_metrics_reg_name("peak_bits_in");
	TEST_ASSERT(err == -EIO, "%s, %d", __func__, __LINE__);

	err = rte_metrics_reg_names(&mnames[0], 1);
	TEST_ASSERT(err == -EIO, "%s, %d", __func__, __LINE__);

	err = rte_metrics_update_value(RTE_METRICS_GLOBAL, KEY, VALUE);
	TEST_ASSERT(err == -EIO, "%s, %d", __func__, __LINE__);

	err = rte_metrics_update_values(RTE_METRICS_GLOBAL, KEY, &value[0], 4);
	TEST_ASSERT(err == -EIO, "%s, %d", __func__, __LINE__);

	err = rte_metrics_get_names(NULL, 0);
	TEST_ASSERT(err == -EIO, "%s, %d", __func__, __LINE__);

	err = rte_metrics_get_values(RTE_METRICS_GLOBAL, NULL, 0);
	TEST_ASSERT(err == -EIO, "%s, %d", __func__, __LINE__);

	return TEST_SUCCESS;
}

/* Test Case to validate registering a single metric */
static int
test_metrics_reg_name_with_validname(void)
{
	int err = 0;

	/* Test to register the new metric name */
	err = rte_metrics_reg_name("peak_bits_out");
	TEST_ASSERT(err >= 0, "%s, %d", __func__, __LINE__);

	/* Test to register the same metric name */
	err = rte_metrics_reg_name("peak_bits_out");
	TEST_ASSERT(err >= 0, "%s, %d", __func__, __LINE__);

	/* Test case to validate registering a invalid metric */
	err = rte_metrics_reg_name(NULL);
	TEST_ASSERT(err == -EINVAL, "%s, %d", __func__, __LINE__);

	return TEST_SUCCESS;
}

/* Test case to validate registering a list of valid  metric names */
static int
test_metrics_reg_names(void)
{
	int err = 0;
	const char * const mnames[] = {
		"mean_bits_in", "mean_bits_out",
		"peak_bits_in", "peak_bits_out",
		};

	/* Success Test: valid array and count size */
	err = rte_metrics_reg_names(&mnames[0], ARRAY_SIZE(mnames));
	TEST_ASSERT(err >= 0, "%s, %d", __func__, __LINE__);

	return TEST_SUCCESS;
}

/* Test case to validate update a metric */
static int
test_metrics_update_value(void)
{
	int err = 0;

	/* Successful Test: Valid port_id, key and value */
	err = rte_metrics_update_value(RTE_METRICS_GLOBAL, KEY, VALUE);
	TEST_ASSERT(err >= 0, "%s, %d", __func__, __LINE__);

	/* Successful Test: Valid port_id otherthan RTE_METRICS_GLOBAL, key
	 * and value
	 */
	err = rte_metrics_update_value(9, KEY, VALUE);
	TEST_ASSERT(err >= 0, "%s, %d", __func__, __LINE__);

	/* Failed Test: Invalid port_id with lower value */
	err = rte_metrics_update_value(-2, KEY, VALUE);
	TEST_ASSERT(err == -EINVAL, "%s, %d", __func__, __LINE__);

	/* Failed Test: Invalid port_id with higher value */
	err = rte_metrics_update_value(39, KEY, VALUE);
	TEST_ASSERT(err == -EINVAL, "%s, %d", __func__, __LINE__);

	/* Failed Test: valid port id, value with invalid key */
	err = rte_metrics_update_value(RTE_METRICS_GLOBAL, KEY+12, VALUE);
	TEST_ASSERT(err < 0, "%s, %d", __func__, __LINE__);

	return TEST_SUCCESS;
}

/* Test case to validate update a list of  metrics  */
static int
test_metrics_update_values(void)
{
	int err = 0;
	const uint64_t  value[REG_METRIC_COUNT] = {1, 2, 3, 4, 5, 6};

	/* Successful Test: update metrics with first set */
	err = rte_metrics_update_values(RTE_METRICS_GLOBAL, 0,
			&value[0], 1);
	TEST_ASSERT(err >= 0, "%s, %d", __func__, __LINE__);

	/* Successful Test: update metrics with second set */
	err = rte_metrics_update_values(RTE_METRICS_GLOBAL, 1,
			&value[1], 1);
	TEST_ASSERT(err >= 0, "%s, %d", __func__, __LINE__);

	/* Successful Test: update metrics with third set */
	err = rte_metrics_update_values(RTE_METRICS_GLOBAL, 2,
			&value[2], 4);
	TEST_ASSERT(err >= 0, "%s, %d", __func__, __LINE__);

	/* Failed Test: Invalid count size */
	err = rte_metrics_update_values(RTE_METRICS_GLOBAL,
			 KEY, &value[0], ARRAY_SIZE(value));
	TEST_ASSERT(err < 0, "%s, %d", __func__, __LINE__);

	/* Failed Test: Invalid port_id(lower value) and valid data */
	err = rte_metrics_update_values(-2, KEY, &value[0], ARRAY_SIZE(value));
	TEST_ASSERT(err == -EINVAL, "%s, %d", __func__, __LINE__);

	/* Failed Test: Invalid port_id(higher value) and valid data */
	err = rte_metrics_update_values(39, 1, &value[0], ARRAY_SIZE(value));
	TEST_ASSERT(err == -EINVAL, "%s, %d", __func__, __LINE__);

	/* Failed Test: Invalid array */
	err = rte_metrics_update_values(RTE_METRICS_GLOBAL,
			 KEY, NULL, ARRAY_SIZE(value));
	TEST_ASSERT(err == -EINVAL, "%s, %d", __func__, __LINE__);

	return TEST_SUCCESS;
}

/* Test to validate get metric name-key lookup table */
static int
test_metrics_get_names(void)
{
	int err = 0;
	struct rte_metric_name metrics[METRIC_LESSER_COUNT];
	struct rte_metric_name success_metrics[REG_METRIC_COUNT];

	/* Successful Test: Invalid array list */
	err = rte_metrics_get_names(NULL, 0);
	TEST_ASSERT(err >= 0, "%s, %d", __func__, __LINE__);

	/* Successful Test: Valid array list, Correct Count Stats same
	 * as memzone stats
	 */
	err = rte_metrics_get_names(success_metrics, REG_METRIC_COUNT);
	TEST_ASSERT(err >= 0, "%s, %d", __func__, __LINE__);

	/* Successful Test: Valid array list, Increase Count Stats than
	 * memzone stats
	 */
	err = rte_metrics_get_names(success_metrics, REG_METRIC_COUNT+5);
	TEST_ASSERT(err >= 0, "%s, %d", __func__, __LINE__);

	/* Successful Test, Not update results:
	 * Invalid array list, Lesser Count Stats than allocated stats
	 */
	err = rte_metrics_get_names(metrics, METRIC_LESSER_COUNT);
	TEST_ASSERT(err >= 0, "%s, %d", __func__, __LINE__);

	return TEST_SUCCESS;
}

/* Test to validate get list of metric values  */
static int
test_metrics_get_values(void)
{
	int i = 0;
	int err = 0;
	struct rte_metric_value getvalues[REG_METRIC_COUNT];

	size_t m_size = sizeof(struct rte_metric_value);
	for (i = 0; i < REG_METRIC_COUNT; i++)
		memset(&getvalues[i], 0, m_size);

	/* Successful Test, Not update results: valid arguments
	 * count lessthan the memzone stats
	 */
	err = rte_metrics_get_values(RTE_METRICS_GLOBAL, getvalues,
			 METRIC_LESSER_COUNT);
	TEST_ASSERT(err >= 0, "%s, %d", __func__, __LINE__);

	/* Successful Test, update results: valid arguments */
	err = rte_metrics_get_values(RTE_METRICS_GLOBAL, getvalues,
			 REG_METRIC_COUNT);
	TEST_ASSERT(err >= 0, "%s, %d", __func__, __LINE__);

	/* Successful Test : valid arguments count greaterthan the
	 * memzone stats
	 */
	err = rte_metrics_get_values(RTE_METRICS_GLOBAL, getvalues,
			REG_METRIC_COUNT+2);
	TEST_ASSERT(err >= 0, "%s, %d", __func__, __LINE__);

	/* Failure Test: Invalid port_id(lower value) with correct values
	 * and  Capacity
	 */
	err = rte_metrics_get_values(-2, getvalues, REG_METRIC_COUNT);
	TEST_ASSERT(err == -EINVAL, "%s, %d", __func__, __LINE__);

	/* Failure Test: Invalid port_id(higher value) with correct values
	 * and  Capacity
	 */
	err = rte_metrics_get_values(33, getvalues, REG_METRIC_COUNT);
	TEST_ASSERT(err == -EINVAL, "%s, %d", __func__, __LINE__);

	/* Successful Test: valid port_id with incorrect values and  valid
	 * capacity
	 */
	err = rte_metrics_get_values(RTE_METRICS_GLOBAL, NULL,
			 REG_METRIC_COUNT);
	TEST_ASSERT(err >= 0, "%s, %d", __func__, __LINE__);

	return TEST_SUCCESS;
}

static struct unit_test_suite metrics_testsuite  = {
	.suite_name = "Metrics Unit Test Suite",
	.setup = NULL,
	.teardown = NULL,
	.unit_test_cases = {
		/* Test Case 1: Test to check all metric APIs without
		 * metrics init
		 */
		TEST_CASE(test_metrics_without_init),

		/* TEST CASE 2: Test to register valid metrics*/
		TEST_CASE_ST(test_metrics_init, NULL,
				test_metrics_reg_name_with_validname),

		/* TEST CASE 3: Test to register list of metrics with valid
		 * names and valid count size, invalid names and invalid
		 * count size
		 */
		TEST_CASE(test_metrics_reg_names),

		/* TEST CASE 4: Test to register a update value with valid port
		 * id and invalid port id
		 */
		TEST_CASE(test_metrics_update_value),

		/* TEST CASE 5: Test to register update list of values with
		 * valid port id, key, value, count size and invalid port id,
		 * key, value, count size
		 */
		TEST_CASE(test_metrics_update_values),

		/* TEST CASE 6: Test to get metric names-key with valid
		 * array list, count size and invalid array list, count size
		 */
		TEST_CASE(test_metrics_get_names),

		/* TEST CASE 7: Test to get list of metric values with valid
		 * port id, array list, count size and invalid port id,
		 * arraylist, count size
		 */
		TEST_CASE(test_metrics_get_values),
		TEST_CASES_END()
	}
};

static int
test_metrics(void)
{
	return unit_test_suite_runner(&metrics_testsuite);
}

REGISTER_TEST_COMMAND(metrics_autotest, test_metrics);
