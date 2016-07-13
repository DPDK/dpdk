/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _TEST_H_
#define _TEST_H_
#include <stddef.h>
#include <sys/queue.h>
#include "rte_log.h"

#define TEST_SUCCESS  (0)
#define TEST_FAILED  (-1)

/* Before including test.h file you can define
 * TEST_TRACE_FAILURE(_file, _line, _func) macro to better trace/debug test
 * failures. Mostly useful in test development phase. */
#ifndef TEST_TRACE_FAILURE
# define TEST_TRACE_FAILURE(_file, _line, _func)
#endif

#define TEST_ASSERT(cond, msg, ...) do {                         \
		if (!(cond)) {                                           \
			printf("TestCase %s() line %d failed: "              \
				msg "\n", __func__, __LINE__, ##__VA_ARGS__);    \
			TEST_TRACE_FAILURE(__FILE__, __LINE__, __func__);    \
			return TEST_FAILED;                                  \
		}                                                        \
} while (0)

#define TEST_ASSERT_EQUAL(a, b, msg, ...) do {                   \
		if (!(a == b)) {                                         \
			printf("TestCase %s() line %d failed: "              \
				msg "\n", __func__, __LINE__, ##__VA_ARGS__);    \
			TEST_TRACE_FAILURE(__FILE__, __LINE__, __func__);    \
			return TEST_FAILED;                                  \
		}                                                        \
} while (0)

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

#define TEST_ASSERT_NOT_EQUAL(a, b, msg, ...) do {               \
		if (!(a != b)) {                                         \
			printf("TestCase %s() line %d failed: "              \
				msg "\n", __func__, __LINE__, ##__VA_ARGS__);    \
			TEST_TRACE_FAILURE(__FILE__, __LINE__, __func__);    \
			return TEST_FAILED;                                  \
		}                                                        \
} while (0)

#define TEST_ASSERT_SUCCESS(val, msg, ...) do {                  \
		typeof(val) _val = (val);                                \
		if (!(_val == 0)) {                                      \
			printf("TestCase %s() line %d failed (err %d): "     \
				msg "\n", __func__, __LINE__, _val,              \
				##__VA_ARGS__);                                  \
			TEST_TRACE_FAILURE(__FILE__, __LINE__, __func__);    \
			return TEST_FAILED;                                  \
		}                                                        \
} while (0)

#define TEST_ASSERT_FAIL(val, msg, ...) do {                     \
		if (!(val != 0)) {                                       \
			printf("TestCase %s() line %d failed: "              \
				msg "\n", __func__, __LINE__, ##__VA_ARGS__);    \
			TEST_TRACE_FAILURE(__FILE__, __LINE__, __func__);    \
			return TEST_FAILED;                                  \
		}                                                        \
} while (0)

#define TEST_ASSERT_NULL(val, msg, ...) do {                     \
		if (!(val == NULL)) {                                    \
			printf("TestCase %s() line %d failed: "              \
				msg "\n", __func__, __LINE__, ##__VA_ARGS__);    \
			TEST_TRACE_FAILURE(__FILE__, __LINE__, __func__);    \
			return TEST_FAILED;                                  \
		}                                                        \
} while (0)

#define TEST_ASSERT_NOT_NULL(val, msg, ...) do {                 \
		if (!(val != NULL)) {                                    \
			printf("TestCase %s() line %d failed: "              \
				msg "\n", __func__, __LINE__, ##__VA_ARGS__);    \
			TEST_TRACE_FAILURE(__FILE__, __LINE__, __func__);    \
			return TEST_FAILED;                                  \
		}                                                        \
} while (0)

struct unit_test_case {
	int (*setup)(void);
	void (*teardown)(void);
	int (*testcase)(void);
	const char *success_msg;
	const char *fail_msg;
	unsigned enabled;
};

#define TEST_CASE(fn) { NULL, NULL, fn, #fn " succeeded", #fn " failed", 1 }

#define TEST_CASE_NAMED(name, fn) { NULL, NULL, fn, name " succeeded", \
		name " failed", 1 }

#define TEST_CASE_ST(setup, teardown, testcase)         \
		{ setup, teardown, testcase, #testcase " succeeded",    \
		#testcase " failed ", 1 }


#define TEST_CASE_DISABLED(fn) { NULL, NULL, fn, #fn " succeeded", \
	#fn " failed", 0 }

#define TEST_CASE_ST_DISABLED(setup, teardown, testcase)         \
		{ setup, teardown, testcase, #testcase " succeeded",    \
		#testcase " failed ", 0 }

#define TEST_CASES_END() { NULL, NULL, NULL, NULL, NULL, 0 }

#if RTE_LOG_LEVEL >= RTE_LOG_DEBUG
#define TEST_HEXDUMP(file, title, buf, len) rte_hexdump(file, title, buf, len)
#else
#define TEST_HEXDUMP(file, title, buf, len) do {} while (0)
#endif

struct unit_test_suite {
	const char *suite_name;
	int (*setup)(void);
	void (*teardown)(void);
	struct unit_test_case unit_test_cases[];
};

int unit_test_suite_runner(struct unit_test_suite *suite);

#define RECURSIVE_ENV_VAR "RTE_TEST_RECURSIVE"

#include <cmdline_parse.h>
#include <cmdline_parse_string.h>

extern const char *prgname;

int commands_init(void);

int test_pci(void);
int test_pci_run;

int test_mp_secondary(void);

int test_ivshmem(void);
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

/* Register a test function with its command string */
#define REGISTER_TEST_COMMAND(cmd, func) \
	static struct test_command test_struct_##cmd = { \
		.command = RTE_STR(cmd), \
		.callback = func, \
	}; \
	static void __attribute__((constructor, used)) \
	test_register_##cmd(void) \
	{ \
		add_test_command(&test_struct_##cmd); \
	}

#endif
