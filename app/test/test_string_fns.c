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
test_rte_snprintf(void)
{
	/* =================================================
	 * First test with a string that will fit in buffer
	 * =================================================*/
	do {
		int retval;
		const char source[] = "This is a string that will fit in buffer";
		char buf[sizeof(source)+2]; /* make buffer big enough to fit string */

		/* initialise buffer with characters so it can contain no nulls */
		memset(buf, DATA_BYTE, sizeof(buf));

		/* run rte_snprintf and check results */
		retval = rte_snprintf(buf, sizeof(buf), "%s", source);
		if (retval != sizeof(source) - 1) {
			LOG("Error, retval = %d, expected = %u\n",
					retval, (unsigned)sizeof(source));
			return -1;
		}
		if (buf[retval] != '\0') {
			LOG("Error, resultant is not null-terminated\n");
			return -1;
		}
		if (memcmp(source, buf, sizeof(source)-1) != 0){
			LOG("Error, corrupt data in buffer\n");
			return -1;
		}
	} while (0);

	do {
		/* =================================================
		 * Test with a string that will get truncated
		 * =================================================*/
		int retval;
		const char source[] = "This is a long string that won't fit in buffer";
		char buf[sizeof(source)/2]; /* make buffer half the size */

		/* initialise buffer with characters so it can contain no nulls */
		memset(buf, DATA_BYTE, sizeof(buf));

		/* run rte_snprintf and check results */
		retval = rte_snprintf(buf, sizeof(buf), "%s", source);
		if (retval != sizeof(source) - 1) {
			LOG("Error, retval = %d, expected = %u\n",
					retval, (unsigned)sizeof(source));
			return -1;
		}
		if (buf[sizeof(buf)-1] != '\0') {
			LOG("Error, buffer is not null-terminated\n");
			return -1;
		}
		if (memcmp(source, buf, sizeof(buf)-1) != 0){
			LOG("Error, corrupt data in buffer\n");
			return -1;
		}
	} while (0);

	do {
		/* ===========================================================
		 * Test using zero-size buf to check how long a buffer we need
		 * ===========================================================*/
		int retval;
		const char source[] = "This is a string";
		char buf[10];

		/* call with a zero-sized non-NULL buffer, should tell how big a buffer
		 * we need */
		retval = rte_snprintf(buf, 0, "%s", source);
		if (retval != sizeof(source) - 1) {
			LOG("Call with 0-length buffer does not return correct size."
					"Expected: %zu, got: %d\n", sizeof(source), retval);
			return -1;
		}

		/* call with a zero-sized NULL buffer, should tell how big a buffer
		 * we need */
		retval = rte_snprintf(NULL, 0, "%s", source);
		if (retval != sizeof(source) - 1) {
			LOG("Call with 0-length buffer does not return correct size."
					"Expected: %zu, got: %d\n", sizeof(source), retval);
			return -1;
		}

	} while (0);

	do {
		/* =================================================
		 * Test with invalid parameter values
		 * =================================================*/
		const char source[] = "This is a string";
		char buf[10];

		/* call with buffer value set to NULL is EINVAL */
		if (rte_snprintf(NULL, sizeof(buf), "%s\n", source) != -1 ||
				errno != EINVAL) {
			LOG("Failed to get suitable error when passing NULL buffer\n");
			return -1;
		}

		memset(buf, DATA_BYTE, sizeof(buf));
		/* call with a NULL format and zero-size should return error
		 * without affecting the buffer */
		if (rte_snprintf(buf, 0, NULL) != -1 ||
				errno != EINVAL) {
			LOG("Failed to get suitable error when passing NULL buffer\n");
			return -1;
		}
		if (buf[0] != DATA_BYTE) {
			LOG("Error, zero-length buffer modified after call with NULL"
					" format string\n");
			return -1;
		}

		/* call with a NULL format should return error but also null-terminate
		 *  the buffer */
		if (rte_snprintf(buf, sizeof(buf), NULL) != -1 ||
				errno != EINVAL) {
			LOG("Failed to get suitable error when passing NULL buffer\n");
			return -1;
		}
		if (buf[0] != '\0') {
			LOG("Error, buffer not null-terminated after call with NULL"
					" format string\n");
			return -1;
		}
	} while (0);

	LOG("%s - PASSED\n", __func__);
	return 0;
}

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

int
test_string_fns(void)
{
	if (test_rte_snprintf() < 0 ||
			test_rte_strsplit() < 0)
		return -1;
	return 0;
}
