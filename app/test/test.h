/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _TEST_H_
#define _TEST_H_

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>

#include <rte_bitops.h>
#include <rte_common.h>
#include <rte_debug.h>
#include <rte_hexdump.h>
#include <rte_lcore.h>
#include <rte_os_shim.h>

#define TEST_SUCCESS EXIT_SUCCESS
#define TEST_FAILED  -1
#define TEST_SKIPPED  77

/* Before including test.h file you can define
 * TEST_TRACE_FAILURE(_file, _line, _func) macro to better trace/debug test
 * failures. Mostly useful in test development phase. */
#ifndef TEST_TRACE_FAILURE
# define TEST_TRACE_FAILURE(_file, _line, _func)
#endif

#include <rte_test.h>

#define TEST_ASSERT RTE_TEST_ASSERT

#define TEST_ASSERT_EQUAL RTE_TEST_ASSERT_EQUAL

/*
 * Helpers backing the TEST_ASSERT_BUFFERS_ARE_EQUAL* macros.
 *
 * Keeping the comparison logic in inline functions ensures each macro
 * argument is evaluated exactly once and gives the compiler real types
 * to check against, which the original all-macro implementation could
 * not provide.
 */
static inline bool
test_buffers_equal_offset(const void *a, const void *b,
		size_t len, size_t off)
{
	const uint8_t *pa = (const uint8_t *)a + off;
	const uint8_t *pb = (const uint8_t *)b + off;

	return memcmp(pa, pb, len) == 0;
}

static inline bool
test_buffers_equal_bit(const void *a, const void *b, size_t len)
{
	const uint8_t *pa = a;
	const uint8_t *pb = b;
	size_t len_bytes = len >> 3;
	size_t len_bits = len & 7;

	if (memcmp(pa, pb, len_bytes) != 0)
		return false;
	if (len_bits != 0) {
		uint8_t mask = (uint8_t)RTE_GENMASK32(7, 8 - len_bits);

		if ((pa[len_bytes] & mask) != (pb[len_bytes] & mask))
			return false;
	}
	return true;
}

static inline bool
test_buffers_equal_bit_offset(const void *a, const void *b,
		size_t len, size_t off)
{
	const uint8_t *pa = a;
	const uint8_t *pb = b;
	size_t off_bits = off & 7;
	size_t off_bytes = off >> 3;

	if (off_bits != 0) {
		uint8_t first_bits = 8 - off_bits;
		uint8_t mask = (uint8_t)RTE_GENMASK32(first_bits - 1, 0);

		if ((pa[off_bytes] & mask) != (pb[off_bytes] & mask))
			return false;
		RTE_ASSERT(len >= first_bits);
		off_bytes++;
		len -= first_bits;
	}
	return test_buffers_equal_bit(pa + off_bytes, pb + off_bytes, len);
}

/* Compare two buffers (length in bytes) */
#define TEST_ASSERT_BUFFERS_ARE_EQUAL(a, b, len, msg, ...) do {		\
	if (memcmp((a), (b), (len)) != 0) {				\
		printf("TestCase %s() line %d failed: " msg "\n",	\
			__func__, __LINE__, ##__VA_ARGS__);		\
		TEST_TRACE_FAILURE(__FILE__, __LINE__, __func__);	\
		return TEST_FAILED;					\
	}								\
} while (0)

/* Compare two buffers with offset (length and offset in bytes) */
#define TEST_ASSERT_BUFFERS_ARE_EQUAL_OFFSET(a, b, len, off, msg, ...) do { \
	if (!test_buffers_equal_offset((a), (b), (len), (off))) {	\
		printf("TestCase %s() line %d failed: " msg "\n",	\
			__func__, __LINE__, ##__VA_ARGS__);		\
		TEST_TRACE_FAILURE(__FILE__, __LINE__, __func__);	\
		return TEST_FAILED;					\
	}								\
} while (0)

/* Compare two buffers (length in bits) */
#define TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(a, b, len, msg, ...) do {	\
	if (!test_buffers_equal_bit((a), (b), (len))) {			\
		printf("TestCase %s() line %d failed: " msg "\n",	\
			__func__, __LINE__, ##__VA_ARGS__);		\
		TEST_TRACE_FAILURE(__FILE__, __LINE__, __func__);	\
		return TEST_FAILED;					\
	}								\
} while (0)

/* Compare two buffers with offset (length and offset in bits) */
#define TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT_OFFSET(a, b, len, off, msg, ...) \
do {									\
	if (!test_buffers_equal_bit_offset((a), (b), (len), (off))) {	\
		printf("TestCase %s() line %d failed: " msg "\n",	\
			__func__, __LINE__, ##__VA_ARGS__);		\
		TEST_TRACE_FAILURE(__FILE__, __LINE__, __func__);	\
		return TEST_FAILED;					\
	}								\
} while (0)

#define TEST_ASSERT_NOT_EQUAL RTE_TEST_ASSERT_NOT_EQUAL

#define TEST_ASSERT_SUCCESS RTE_TEST_ASSERT_SUCCESS

#define TEST_ASSERT_FAIL RTE_TEST_ASSERT_FAIL

#define TEST_ASSERT_NULL RTE_TEST_ASSERT_NULL

#define TEST_ASSERT_NOT_NULL RTE_TEST_ASSERT_NOT_NULL

struct unit_test_case {
	int (*setup)(void);
	void (*teardown)(void);
	int (*testcase)(void);
	int (*testcase_with_data)(const void *data);
	const char *name;
	unsigned enabled;
	const void *data;
};

#define TEST_CASE(fn) { NULL, NULL, fn, NULL, #fn, 1, NULL }

#define TEST_CASE_NAMED(name, fn) { NULL, NULL, fn, NULL, name, 1, NULL }

#define TEST_CASE_ST(setup, teardown, testcase) \
		{ setup, teardown, testcase, NULL, #testcase, 1, NULL }

#define TEST_CASE_WITH_DATA(setup, teardown, testcase, data) \
		{ setup, teardown, NULL, testcase, #testcase, 1, data }

#define TEST_CASE_NAMED_ST(name, setup, teardown, testcase) \
		{ setup, teardown, testcase, NULL, name, 1, NULL }

#define TEST_CASE_NAMED_WITH_DATA(name, setup, teardown, testcase, data) \
		{ setup, teardown, NULL, testcase, name, 1, data }

#define TEST_CASE_DISABLED(fn) { NULL, NULL, fn, NULL, #fn, 0, NULL }

#define TEST_CASE_ST_DISABLED(setup, teardown, testcase) \
		{ setup, teardown, testcase, NULL, #testcase, 0, NULL }

#define TEST_CASES_END() { NULL, NULL, NULL, NULL, NULL, 0, NULL }

static inline void
debug_hexdump(FILE *file, const char *title, const void *buf, size_t len)
{
	if (rte_log_get_global_level() == RTE_LOG_DEBUG)
		rte_hexdump(file, title, buf, len);
}

struct unit_test_suite {
	const char *suite_name;
	int (*setup)(void);
	void (*teardown)(void);
	unsigned int total;
	unsigned int executed;
	unsigned int succeeded;
	unsigned int skipped;
	unsigned int failed;
	unsigned int unsupported;
	struct unit_test_suite **unit_test_suites;
	struct unit_test_case unit_test_cases[];
};

int unit_test_suite_runner(struct unit_test_suite *suite);
extern int last_test_result;

#define RECURSIVE_ENV_VAR "RTE_TEST_RECURSIVE"

#include <cmdline_parse.h>
#include <cmdline_parse_string.h>

extern const char *prgname;

int commands_init(void);
int command_valid(const char *cmd);

int test_exit(void);
int test_mp_secondary(void);
int test_panic(void);
int test_timer_secondary(void);

int test_set_rxtx_conf(cmdline_fixed_string_t mode);
int test_set_rxtx_anchor(cmdline_fixed_string_t type);
int test_set_rxtx_sc(cmdline_fixed_string_t type);

typedef int (test_callback)(void);
TAILQ_HEAD(test_commands_list, test_command);
struct test_command {
	TAILQ_ENTRY(test_command) next;
	const char *command;
	test_callback *callback;
};

void add_test_command(struct test_command *t);

/* Register a test function with its command string. Should not be used directly */
#define REGISTER_TEST_COMMAND(cmd, func) \
	static struct test_command test_struct_##cmd = { \
		.command = RTE_STR(cmd), \
		.callback = func, \
	}; \
	RTE_INIT(test_register_##cmd) \
	{ \
		add_test_command(&test_struct_##cmd); \
	}

/* Register a test function as a particular type.
 * These can be used to build up test suites automatically
 */
#define REGISTER_PERF_TEST REGISTER_TEST_COMMAND
#define REGISTER_DRIVER_TEST REGISTER_TEST_COMMAND
#define REGISTER_STRESS_TEST REGISTER_TEST_COMMAND

/* fast tests are a bit special. They can be specified as supporting running without
 * hugepages and/or under ASan.
 * - The "no_huge" options should be passed as either "NOHUGE_OK" or "NOHUGE_SKIP"
 * - The "ASan" options should be passed as either "ASAN_OK" or "ASAN_SKIP"
 */
#define REGISTER_FAST_TEST(cmd, no_huge, ASan, func)  REGISTER_TEST_COMMAND(cmd, func)

/* For unstable or experimental tests cases which need work or which may be removed
 * in the future.
 */
#define REGISTER_ATTIC_TEST REGISTER_TEST_COMMAND

/**
 * Scale test iterations inversely with core count.
 *
 * On high core count systems, tests with per-core work can exceed
 * timeout limits due to increased lock contention and scheduling
 * overhead. This helper scales iterations to keep total test time
 * roughly constant regardless of core count.
 *
 * @param base  Base iteration count (used on single-core systems)
 * @param min   Minimum iterations (floor to ensure meaningful testing)
 * @return      Scaled iteration count
 */
static inline unsigned int
test_scale_iterations(unsigned int base, unsigned int min)
{
	return RTE_MAX(base / rte_lcore_count(), min);
}

#endif
