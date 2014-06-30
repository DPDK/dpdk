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
#include <string.h>
#include <inttypes.h>

#include <rte_ether.h>
#include <rte_string_fns.h>

#include <cmdline_parse.h>
#include <cmdline_parse_etheraddr.h>

#include "test_cmdline.h"

struct ether_addr_str {
	const char * str;
	uint64_t address;
};

/* valid strings */
const struct ether_addr_str ether_addr_valid_strs[] = {
		{"01:23:45:67:89:AB", 0xAB8967452301ULL},
		{"4567:89AB:CDEF", 0xEFCDAB896745ULL},
};

/* valid strings with various garbage at the end.
 * these strings are still valid because parser checks for
 * end of token, which is either space chars, null char or
 * a hash sign.
 */
const char * ether_addr_garbage_strs[] = {
		"00:11:22:33:44:55\0garbage",
		"00:11:22:33:44:55#garbage",
		"00:11:22:33:44:55 garbage",
		"00:11:22:33:44:55\tgarbage",
		"00:11:22:33:44:55\ngarbage",
		"00:11:22:33:44:55\rgarbage",
		"00:11:22:33:44:55#",
		"00:11:22:33:44:55 ",
		"00:11:22:33:44:55\t",
		"00:11:22:33:44:55\n",
		"00:11:22:33:44:55\r",
};
#define GARBAGE_ETHERADDR 0x554433221100ULL /* corresponding address */


const char * ether_addr_invalid_strs[] = {
		/* valid chars, invalid syntax */
		"0123:45:67:89:AB",
		"01:23:4567:89:AB",
		"01:23:45:67:89AB",
		"012:345:678:9AB",
		"01:23:45:67:89:ABC",
		"01:23:45:67:89:A",
		"01:23:45:67:89",
		"01:23:45:67:89:AB:CD",
		/* invalid chars, valid syntax */
		"IN:VA:LI:DC:HA:RS",
		"INVA:LIDC:HARS",
		/* misc */
		"01 23 45 67 89 AB",
		"01.23.45.67.89.AB",
		"01,23,45,67,89,AB",
		"01:23:45\0:67:89:AB",
		"01:23:45#:67:89:AB",
		"random invalid text",
		"random text",
		"",
		"\0",
		" ",
};

#define ETHERADDR_VALID_STRS_SIZE \
	(sizeof(ether_addr_valid_strs) / sizeof(ether_addr_valid_strs[0]))
#define ETHERADDR_GARBAGE_STRS_SIZE \
	(sizeof(ether_addr_garbage_strs) / sizeof(ether_addr_garbage_strs[0]))
#define ETHERADDR_INVALID_STRS_SIZE \
	(sizeof(ether_addr_invalid_strs) / sizeof(ether_addr_invalid_strs[0]))



static int
is_addr_different(const struct ether_addr addr, uint64_t num)
{
	int i;
	for (i = 0; i < ETHER_ADDR_LEN; i++, num >>= 8)
		if (addr.addr_bytes[i] != (num & 0xFF)) {
			return 1;
		}
	return 0;
}

/* test invalid parameters */
int
test_parse_etheraddr_invalid_param(void)
{
	char buf[CMDLINE_TEST_BUFSIZE];
	struct ether_addr result;
	int ret = 0;

	/* try all null */
	ret = cmdline_parse_etheraddr(NULL, NULL, NULL);
	if (ret != -1) {
		printf("Error: parser accepted null parameters!\n");
		return -1;
	}

	/* try null buf */
	ret = cmdline_parse_etheraddr(NULL, NULL, (void*)&result);
	if (ret != -1) {
		printf("Error: parser accepted null string!\n");
		return -1;
	}

	/* try null result */

	/* copy string to buffer */
	snprintf(buf, sizeof(buf), "%s",
			ether_addr_valid_strs[0].str);

	ret = cmdline_parse_etheraddr(NULL, buf, NULL);
	if (ret == -1) {
		printf("Error: parser rejected null result!\n");
		return -1;
	}

	/* token is not used in ether_parse anyway so there's no point in
	 * testing it */

	/* test help function */
	memset(&buf, 0, sizeof(buf));

	/* coverage! */
	ret = cmdline_get_help_etheraddr(NULL, buf, sizeof(buf));
	if (ret < 0) {
		printf("Error: help function failed with valid parameters!\n");
		return -1;
	}

	return 0;
}

/* test valid parameters but invalid data */
int
test_parse_etheraddr_invalid_data(void)
{
	int ret = 0;
	unsigned i;
	struct ether_addr result;

	/* test full strings */
	for (i = 0; i < ETHERADDR_INVALID_STRS_SIZE; i++) {

		memset(&result, 0, sizeof(struct ether_addr));

		ret = cmdline_parse_etheraddr(NULL, ether_addr_invalid_strs[i],
				(void*)&result);
		if (ret != -1) {
			printf("Error: parsing %s succeeded!\n",
					ether_addr_invalid_strs[i]);
			return -1;
		}
	}

	return 0;
}

/* test valid parameters and data */
int
test_parse_etheraddr_valid(void)
{
	int ret = 0;
	unsigned i;
	struct ether_addr result;

	/* test full strings */
	for (i = 0; i < ETHERADDR_VALID_STRS_SIZE; i++) {

		memset(&result, 0, sizeof(struct ether_addr));

		ret = cmdline_parse_etheraddr(NULL, ether_addr_valid_strs[i].str,
				(void*)&result);
		if (ret < 0) {
			printf("Error: parsing %s failed!\n",
					ether_addr_valid_strs[i].str);
			return -1;
		}
		if (is_addr_different(result, ether_addr_valid_strs[i].address)) {
			printf("Error: parsing %s failed: address mismatch!\n",
					ether_addr_valid_strs[i].str);
			return -1;
		}
	}

	/* test garbage strings */
	for (i = 0; i < ETHERADDR_GARBAGE_STRS_SIZE; i++) {

		memset(&result, 0, sizeof(struct ether_addr));

		ret = cmdline_parse_etheraddr(NULL, ether_addr_garbage_strs[i],
				(void*)&result);
		if (ret < 0) {
			printf("Error: parsing %s failed!\n",
					ether_addr_garbage_strs[i]);
			return -1;
		}
		if (is_addr_different(result, GARBAGE_ETHERADDR)) {
			printf("Error: parsing %s failed: address mismatch!\n",
					ether_addr_garbage_strs[i]);
			return -1;
		}
	}

	return 0;
}
