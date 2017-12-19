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
test_string_fns(void)
{
	if (test_rte_strsplit() < 0)
		return -1;
	return 0;
}

REGISTER_TEST_COMMAND(string_autotest, test_string_fns);
