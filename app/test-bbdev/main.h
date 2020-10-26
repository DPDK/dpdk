/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _MAIN_H_
#define _MAIN_H_

#include <stddef.h>
#include <sys/queue.h>

#include <rte_common.h>
#include <rte_hexdump.h>
#include <rte_log.h>

#define TEST_SUCCESS    0
#define TEST_FAILED     -1
#define TEST_SKIPPED    1

#define MAX_BURST 512U
#define MAX_OPS 1024U
#define DEFAULT_BURST 32U
#define DEFAULT_OPS 64U
#define DEFAULT_ITER 6U



#define TEST_ASSERT(cond, msg, ...) do {  \
		if (!(cond)) {  \
			printf("TestCase %s() line %d failed: " \
				msg "\n", __func__, __LINE__, ##__VA_ARGS__); \
			return TEST_FAILED;  \
		} \
} while (0)

/* Compare two buffers (length in bytes) */
#define TEST_ASSERT_BUFFERS_ARE_EQUAL(a, b, len, msg, ...) do { \
	if (memcmp((a), (b), len)) { \
		printf("TestCase %s() line %d failed: " \
			msg "\n", __func__, __LINE__, ##__VA_ARGS__); \
		rte_memdump(stdout, "Buffer A", (a), len); \
		rte_memdump(stdout, "Buffer B", (b), len); \
		return TEST_FAILED; \
	} \
} while (0)

#define TEST_ASSERT_SUCCESS(val, msg, ...) do { \
		typeof(val) _val = (val); \
		if (!(_val == 0)) { \
			printf("TestCase %s() line %d failed (err %d): " \
				msg "\n", __func__, __LINE__, _val, \
				##__VA_ARGS__); \
			return TEST_FAILED; \
		} \
} while (0)

#define TEST_ASSERT_FAIL(val, msg, ...) \
	TEST_ASSERT_SUCCESS(!(val), msg, ##__VA_ARGS__)

#define TEST_ASSERT_NOT_NULL(val, msg, ...) do { \
		if ((val) == NULL) { \
			printf("TestCase %s() line %d failed (null): " \
				msg "\n", __func__, __LINE__, ##__VA_ARGS__); \
			return TEST_FAILED;  \
		} \
} while (0)

struct unit_test_case {
	int (*setup)(void);
	void (*teardown)(void);
	int (*testcase)(void);
	const char *name;
};

#define TEST_CASE(testcase) {NULL, NULL, testcase, #testcase}

#define TEST_CASE_ST(setup, teardown, testcase) \
		{setup, teardown, testcase, #testcase}

#define TEST_CASES_END() {NULL, NULL, NULL, NULL}

struct unit_test_suite {
	const char *suite_name;
	int (*setup)(void);
	void (*teardown)(void);
	struct unit_test_case unit_test_cases[];
};

int unit_test_suite_runner(struct unit_test_suite *suite);

typedef int (test_callback)(void);
TAILQ_HEAD(test_commands_list, test_command);
struct test_command {
	TAILQ_ENTRY(test_command) next;
	const char *command;
	test_callback *callback;
};

void add_test_command(struct test_command *t);

/* Register a test function */
#define REGISTER_TEST_COMMAND(name, testsuite) \
	static int test_func_##name(void) \
	{ \
		return unit_test_suite_runner(&testsuite); \
	} \
	static struct test_command test_struct_##name = { \
		.command = RTE_STR(name), \
		.callback = test_func_##name, \
	}; \
	RTE_INIT(test_register_##name) \
	{ \
		add_test_command(&test_struct_##name); \
	}

const char *get_vector_filename(void);

unsigned int get_num_ops(void);

unsigned int get_burst_sz(void);

unsigned int get_num_lcores(void);

double get_snr(void);

unsigned int get_iter_max(void);

bool get_init_device(void);

#endif
