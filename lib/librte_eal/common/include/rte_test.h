/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015 Cavium, Inc
 */

#ifndef _RTE_TEST_H_
#define _RTE_TEST_H_

#include <rte_log.h>

/* Before including rte_test.h file you can define
 * RTE_TEST_TRACE_FAILURE(_file, _line, _func) macro to better trace/debug test
 * failures. Mostly useful in development phase.
 */
#ifndef RTE_TEST_TRACE_FAILURE
#define RTE_TEST_TRACE_FAILURE(_file, _line, _func)
#endif


#define RTE_TEST_ASSERT(cond, msg, ...) do {                                  \
	if (!(cond)) {                                                        \
		RTE_LOG(DEBUG, EAL, "Test assert %s line %d failed: "         \
				msg "\n", __func__, __LINE__, ##__VA_ARGS__); \
		RTE_TEST_TRACE_FAILURE(__FILE__, __LINE__, __func__);         \
		return -1;                                                    \
	}                                                                     \
} while (0)

#define RTE_TEST_ASSERT_EQUAL(a, b, msg, ...) \
	RTE_TEST_ASSERT(a == b, msg, ##__VA_ARGS__)

#define RTE_TEST_ASSERT_NOT_EQUAL(a, b, msg, ...) \
	RTE_TEST_ASSERT(a != b, msg, ##__VA_ARGS__)

#define RTE_TEST_ASSERT_SUCCESS(val, msg, ...) \
	RTE_TEST_ASSERT(val == 0, msg, ##__VA_ARGS__)

#define RTE_TEST_ASSERT_FAIL(val, msg, ...) \
	RTE_TEST_ASSERT(val != 0, msg, ##__VA_ARGS__)

#define RTE_TEST_ASSERT_NULL(val, msg, ...) \
	RTE_TEST_ASSERT(val == NULL, msg, ##__VA_ARGS__)

#define RTE_TEST_ASSERT_NOT_NULL(val, msg, ...) \
	RTE_TEST_ASSERT(val != NULL, msg, ##__VA_ARGS__)

#endif /* _RTE_TEST_H_ */
