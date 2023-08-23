/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _TEST_H_
#define _TEST_H_

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/queue.h>

#include <rte_hexdump.h>
#include <rte_common.h>
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

/* Compare two buffers (length in bytes) */
#define TEST_ASSERT_BUFFERS_ARE_EQUAL(a, b, len,  msg, ...) do {	\
	if (memcmp(a, b, len)) {                                        \
		printf("TestCase %s() line %d failed: "              \
			msg "\n", __func__, __LINE__, ##__VA_ARGS__);    \
		TEST_TRACE_FAILURE(__FILE__, __LINE__, __func__);    \
		return TEST_FAILED;                                  \
	}                                                        \
} while (0)

/* Compare two buffers with offset (length and offset in bytes) */
#define TEST_ASSERT_BUFFERS_ARE_EQUAL_OFFSET(a, b, len, off, msg, ...) do { \
	const uint8_t *_a_with_off = (const uint8_t *)a + off;              \
	const uint8_t *_b_with_off = (const uint8_t *)b + off;              \
	TEST_ASSERT_BUFFERS_ARE_EQUAL(_a_with_off, _b_with_off, len, msg);  \
} while (0)

/* Compare two buffers (length in bits) */
#define TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(a, b, len, msg, ...) do {	\
	uint8_t _last_byte_a, _last_byte_b;                       \
	uint8_t _last_byte_mask, _last_byte_bits;                  \
	TEST_ASSERT_BUFFERS_ARE_EQUAL(a, b, (len >> 3), msg);     \
	if (len % 8) {                                              \
		_last_byte_bits = len % 8;                   \
		_last_byte_mask = ~((1 << (8 - _last_byte_bits)) - 1); \
		_last_byte_a = ((const uint8_t *)a)[len >> 3];            \
		_last_byte_b = ((const uint8_t *)b)[len >> 3];            \
		_last_byte_a &= _last_byte_mask;                     \
		_last_byte_b &= _last_byte_mask;                    \
		if (_last_byte_a != _last_byte_b) {                  \
			printf("TestCase %s() line %d failed: "              \
				msg "\n", __func__, __LINE__, ##__VA_ARGS__);\
			TEST_TRACE_FAILURE(__FILE__, __LINE__, __func__);    \
			return TEST_FAILED;                                  \
		}                                                        \
	}                                                            \
} while (0)

/* Compare two buffers with offset (length and offset in bits) */
#define TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT_OFFSET(a, b, len, off, msg, ...) do {	\
	uint8_t _first_byte_a, _first_byte_b;                                 \
	uint8_t _first_byte_mask, _first_byte_bits;                           \
	uint32_t _len_without_first_byte = (off % 8) ?                       \
				len - (8 - (off % 8)) :                       \
				len;                                          \
	uint32_t _off_in_bytes = (off % 8) ? (off >> 3) + 1 : (off >> 3);     \
	const uint8_t *_a_with_off = (const uint8_t *)a + _off_in_bytes;      \
	const uint8_t *_b_with_off = (const uint8_t *)b + _off_in_bytes;      \
	TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(_a_with_off, _b_with_off,           \
				_len_without_first_byte, msg);                \
	if (off % 8) {                                                        \
		_first_byte_bits = 8 - (off % 8);                             \
		_first_byte_mask = (1 << _first_byte_bits) - 1;               \
		_first_byte_a = *(_a_with_off - 1);                           \
		_first_byte_b = *(_b_with_off - 1);                           \
		_first_byte_a &= _first_byte_mask;                            \
		_first_byte_b &= _first_byte_mask;                            \
		if (_first_byte_a != _first_byte_b) {                         \
			printf("TestCase %s() line %d failed: "               \
				msg "\n", __func__, __LINE__, ##__VA_ARGS__); \
			TEST_TRACE_FAILURE(__FILE__, __LINE__, __func__);     \
			return TEST_FAILED;                                   \
		}                                                             \
	}                                                                     \
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

int test_mp_secondary(void);
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
#define REGISTER_FAST_TEST(cmd, no_huge, ASan, func)  REGISTER_TEST_COMMAND(cmd, func)
#define REGISTER_PERF_TEST REGISTER_TEST_COMMAND
#define REGISTER_DRIVER_TEST REGISTER_TEST_COMMAND

#endif
