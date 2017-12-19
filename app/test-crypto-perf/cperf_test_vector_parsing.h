/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */

#ifndef APP_CRYPTO_PERF_CPERF_TEST_VECTOR_PARSING_H_
#define APP_CRYPTO_PERF_CPERF_TEST_VECTOR_PARSING_H_

#define CPERF_VALUE_DELIMITER ","
#define CPERF_ENTRY_DELIMITER "="

/**
 * Frees the allocated memory for test vector
 *
 * @param vector
 *   Destination vector test to release
 * @param opts
 *   Test options
 * @return
 *   0 on success, (-1) on error.
 */
int
free_test_vector(struct cperf_test_vector *vector, struct cperf_options *opts);

/**
 * Displays data in test vector
 *
 * @param vector
 *   Vector to display
 */
void
show_test_vector(struct cperf_test_vector *test_vector);

/**
 * Completes test vector with data from file
 *
 * @param opts
 *   Test options
 * @return
 *   NULL on error.
 *   Test vector pointer on successful.
 */
struct cperf_test_vector*
cperf_test_vector_get_from_file(struct cperf_options *opts);

#endif /* APP_CRYPTO_PERF_CPERF_TEST_VECTOR_PARSING_H_ */
