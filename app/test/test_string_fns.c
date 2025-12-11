/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>

#include <rte_string_fns.h>

#include "test.h"

#define LOG(...) do {\
	fprintf(stderr, "%s() ln %d: ", __func__, __LINE__); \
	fprintf(stderr, __VA_ARGS__); \
} while(0)

#define DATA_BYTE 'a'

static int
test_rte_strsplit(void)
{
	int i;
	do {
		/* =======================================================
		 * split a mac address correct number of splits requested
		 * =======================================================*/
		char test_string[] = "54:65:76:87:98:90";
		char *splits[6];

		LOG("Source string: '%s', to split on ':'\n", test_string);
		if (rte_strsplit(test_string, sizeof(test_string),
				splits, 6, ':') != 6) {
			LOG("Error splitting mac address\n");
			return -1;
		}
		for (i = 0; i < 6; i++)
			LOG("Token %d = %s\n", i + 1, splits[i]);
	} while (0);


	do {
		/* =======================================================
		 * split on spaces smaller number of splits requested
		 * =======================================================*/
		char test_string[] = "54 65 76 87 98 90";
		char *splits[6];

		LOG("Source string: '%s', to split on ' '\n", test_string);
		if (rte_strsplit(test_string, sizeof(test_string),
				splits, 3, ' ') != 3) {
			LOG("Error splitting mac address for max 2 splits\n");
			return -1;
		}
		for (i = 0; i < 3; i++)
			LOG("Token %d = %s\n", i + 1, splits[i]);
	} while (0);

	do {
		/* =======================================================
		 * split on commas - more splits than commas requested
		 * =======================================================*/
		char test_string[] = "a,b,c,d";
		char *splits[6];

		LOG("Source string: '%s', to split on ','\n", test_string);
		if (rte_strsplit(test_string, sizeof(test_string),
				splits, 6, ',') != 4) {
			LOG("Error splitting %s on ','\n", test_string);
			return -1;
		}
		for (i = 0; i < 4; i++)
			LOG("Token %d = %s\n", i + 1, splits[i]);
	} while(0);

	do {
		/* =======================================================
		 * Try splitting on non-existent character.
		 * =======================================================*/
		char test_string[] = "a,b,c,d";
		char *splits[6];

		LOG("Source string: '%s', to split on ' '\n", test_string);
		if (rte_strsplit(test_string, sizeof(test_string),
				splits, 6, ' ') != 1) {
			LOG("Error splitting %s on ' '\n", test_string);
			return -1;
		}
		LOG("String not split\n");
	} while(0);

	do {
		/* =======================================================
		 * Invalid / edge case parameter checks
		 * =======================================================*/
		char test_string[] = "a,b,c,d";
		char *splits[6];

		if (rte_strsplit(NULL, 0, splits, 6, ',') >= 0
				|| errno != EINVAL){
			LOG("Error: rte_strsplit accepted NULL string parameter\n");
			return -1;
		}

		if (rte_strsplit(test_string, sizeof(test_string), NULL, 0, ',') >= 0
				|| errno != EINVAL){
			LOG("Error: rte_strsplit accepted NULL array parameter\n");
			return -1;
		}

		errno = 0;
		if (rte_strsplit(test_string, 0, splits, 6, ',') != 0 || errno != 0) {
			LOG("Error: rte_strsplit did not accept 0 length string\n");
			return -1;
		}

		if (rte_strsplit(test_string, sizeof(test_string), splits, 0, ',') != 0
				|| errno != 0) {
			LOG("Error: rte_strsplit did not accept 0 length array\n");
			return -1;
		}

		LOG("Parameter test cases passed\n");
	} while(0);

	LOG("%s - PASSED\n", __func__);
	return 0;
}

static int
test_rte_strlcat(void)
{
	/* only run actual unit tests if we have system-provided strlcat */
#if defined(__BSD_VISIBLE) || defined(RTE_USE_LIBBSD)
#define BUF_LEN 32
	const char dst[BUF_LEN] = "Test string";
	const char src[] = " appended";
	char bsd_dst[BUF_LEN];
	char rte_dst[BUF_LEN];
	size_t i, bsd_ret, rte_ret;

	LOG("dst = '%s', strlen(dst) = %zu\n", dst, strlen(dst));
	LOG("src = '%s', strlen(src) = %zu\n", src, strlen(src));
	LOG("---\n");

	for (i = 0; i < BUF_LEN; i++) {
		/* initialize destination buffers */
		memcpy(bsd_dst, dst, BUF_LEN);
		memcpy(rte_dst, dst, BUF_LEN);
		/* compare implementations */
		bsd_ret = strlcat(bsd_dst, src, i);
		rte_ret = rte_strlcat(rte_dst, src, i);
		if (bsd_ret != rte_ret) {
			LOG("Incorrect retval for buf length = %zu\n", i);
			LOG("BSD: '%zu', rte: '%zu'\n", bsd_ret, rte_ret);
			return -1;
		}
		if (memcmp(bsd_dst, rte_dst, BUF_LEN) != 0) {
			LOG("Resulting buffers don't match\n");
			LOG("BSD: '%s', rte: '%s'\n", bsd_dst, rte_dst);
			return -1;
		}
		LOG("buffer size = %zu: dst = '%s', ret = %zu\n",
			i, rte_dst, rte_ret);
	}
	LOG("Checked %zu combinations\n", i);
#undef BUF_LEN
#endif /* defined(__BSD_VISIBLE) || defined(RTE_USE_LIBBSD) */

	return 0;
}

static int
test_rte_str_skip_leading_spaces(void)
{
	static const char empty[] = "";
	static const char nowhitespace[] = "Thereisreallynowhitespace";
	static const char somewhitespaces[] = " \f\n\r\t\vThere are some whitespaces";
	const char *p;

	LOG("Checking '%s'\n", empty);
	p = rte_str_skip_leading_spaces(empty);
	if (p != empty) {
		LOG("Returned address '%s' does not match expected result\n", p);
		return -1;
	}
	LOG("Got expected '%s'\n", p);
	LOG("Checking '%s'\n", nowhitespace);
	p = rte_str_skip_leading_spaces(nowhitespace);
	if (p != nowhitespace) {
		LOG("Returned address '%s' does not match expected result\n", p);
		return -1;
	}
	LOG("Got expected '%s'\n", p);
	LOG("Checking '%s'\n", somewhitespaces);
	p = rte_str_skip_leading_spaces(somewhitespaces);
	if (p != strchr(somewhitespaces, 'T')) {
		LOG("Returned address '%s' does not match expected result\n", p);
		return -1;
	}
	LOG("Got expected '%s'\n", p);

	return 0;
}

static int
test_rte_basename(void)
{
	/* Test case structure for positive cases */
	struct {
		const char *input_path;    /* Input path string */
		const char *expected;      /* Expected result */
	} test_cases[] = {
		/* Test cases from man 3 basename */
		{"/usr/lib", "lib"},
		{"/usr/", "usr"},
		{"usr", "usr"},
		{"/", "/"},
		{".", "."},
		{"..", ".."},

		/* Additional requested test cases */
		{"/////", "/"},
		{"/path/to/file.txt", "file.txt"},

		/* Additional edge cases with trailing slashes */
		{"///usr///", "usr"},
		{"/a/b/c/", "c"},

		/* Empty string case */
		{"", "."},
		{NULL, "."}  /* NULL path should return "." */
	};

	char buf[256];
	size_t result;

	/* Run positive test cases from table */
	for (size_t i = 0; i < RTE_DIM(test_cases); i++) {
		result = rte_basename(test_cases[i].input_path, buf, sizeof(buf));

		if (strcmp(buf, test_cases[i].expected) != 0) {
			LOG("FAIL [%zu]: '%s' - buf contains '%s', expected '%s'\n",
			    i, test_cases[i].input_path, buf, test_cases[i].expected);
			return -1;
		}

		/* Check that the return value matches the expected string length */
		if (result != strlen(test_cases[i].expected)) {
			LOG("FAIL [%zu]: '%s' - returned length %zu, expected %zu\n",
			    i, test_cases[i].input_path, result, strlen(test_cases[i].expected));
			return -1;
		}

		LOG("PASS [%zu]: '%s' -> '%s' (len=%zu)\n",
				i, test_cases[i].input_path, buf, result);
	}

	/* re-run the table above verifying that for a NULL buffer, or zero length, we get
	 * correct length returned.
	 */
	for (size_t i = 0; i < RTE_DIM(test_cases); i++) {
		result = rte_basename(test_cases[i].input_path, NULL, 0);
		if (result != strlen(test_cases[i].expected)) {
			LOG("FAIL [%zu]: '%s' - returned length %zu, expected %zu\n",
			    i, test_cases[i].input_path, result, strlen(test_cases[i].expected));
			return -1;
		}
		LOG("PASS [%zu]: '%s' -> length %zu (NULL buffer case)\n",
		    i, test_cases[i].input_path, result);
	}

	/* Test case: buffer too small for result should truncate and return full length */
	const size_t small_size = 5;
	result = rte_basename("/path/to/very_long_filename.txt", buf, small_size);
	/* Should be truncated to fit in 5 bytes (4 chars + null terminator) */
	if (strlen(buf) >= small_size) {
		LOG("FAIL: small buffer test - result '%s' not properly truncated (len=%zu, buflen=%zu)\n",
		    buf, strlen(buf), small_size);
		return -1;
	}
	/* Return value should indicate truncation occurred (>= buflen) */
	if (result != strlen("very_long_filename.txt")) {
		LOG("FAIL: small buffer test - return value %zu doesn't indicate truncation (buflen=%zu)\n",
		    result, small_size);
		return -1;
	}
	LOG("PASS: small buffer truncation -> '%s' (returned len=%zu, actual len=%zu)\n",
	    buf, result, strlen(buf));

	/* extreme length test case -  check that even with paths longer than PATH_MAX we still
	 * return the last component correctly. Use "/zzz...zzz/abc.txt" and check we get "abc.txt"
	 */
	char basename_val[] = "abc.txt";
	char long_path[PATH_MAX + 50];
	for (int i = 0; i < PATH_MAX + 20; i++)
		long_path[i] = (i == 0) ? '/' : 'z';
	sprintf(long_path + PATH_MAX + 20, "/%s", basename_val);

	result = rte_basename(long_path, buf, sizeof(buf));
	if (strcmp(buf, basename_val) != 0) {
		LOG("FAIL: long path test - expected '%s', got '%s'\n",
		    basename_val, buf);
		return -1;
	}
	if (result != strlen(basename_val)) {
		LOG("FAIL: long path test - expected length %zu, got %zu\n",
		    strlen(basename_val), result);
		return -1;
	}
	LOG("PASS: long path test -> '%s' (len=%zu)\n", buf, result);
	return 0;
}

static int
test_string_fns(void)
{
	if (test_rte_strsplit() < 0)
		return -1;
	if (test_rte_strlcat() < 0)
		return -1;
	if (test_rte_str_skip_leading_spaces() < 0)
		return -1;
	if (test_rte_basename() < 0)
		return -1;
	return 0;
}

REGISTER_FAST_TEST(string_autotest, NOHUGE_OK, ASAN_OK, test_string_fns);
