/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include <rte_common.h>

#include <cmdline_parse.h>
#include <cmdline_parse_portlist.h>

#include "test_cmdline.h"

struct portlist_str {
	const char * str;
	uint32_t portmap;
};

/* valid strings */
const struct portlist_str portlist_valid_strs[] = {
		{"0", 0x1U },
		{"0-10", 0x7FFU},
		{"10-20", 0x1FFC00U},
		{"all", UINT32_MAX},
		{"0,1,2,3", 0xFU},
		{"0,1-5", 0x3FU},
		{"0,0,0", 0x1U},
		{"31,0-10,15", 0x800087FFU},
		{"0000", 0x1U},
		{"00,01,02,03", 0xFU},
		{"000,001,002,003", 0xFU},
};

/* valid strings but with garbage at the end.
 * these strings should still be valid because parser checks
 * for end of token, which is either a space/tab, a newline/return,
 * or a hash sign.
 */

const char * portlist_garbage_strs[] = {
		"0-31 garbage",
		"0-31#garbage",
		"0-31\0garbage",
		"0-31\ngarbage",
		"0-31\rgarbage",
		"0-31\tgarbage",
		"0,1,2,3-31 garbage",
		"0,1,2,3-31#garbage",
		"0,1,2,3-31\0garbage",
		"0,1,2,3-31\ngarbage",
		"0,1,2,3-31\rgarbage",
		"0,1,2,3-31\tgarbage",
		"all garbage",
		"all#garbage",
		"all\0garbage",
		"all\ngarbage",
		"all\rgarbage",
		"all\tgarbage",
};

/* invalid strings */
const char * portlist_invalid_strs[] = {
		/* valid syntax, invalid chars */
		"A-B",
		"0-S",
		"1,2,3,4,Q",
		"A-4,3-15",
		"0-31invalid",
		/* valid chars, invalid syntax */
		"1, 2",
		"1- 4",
		",2",
		",2 ",
		"-1, 4",
		"5-1",
		"2-",
		/* misc */
		"-"
		"a",
		"A",
		",",
		"#",
		" ",
		"\0",
		"",
		/* too long */
		"0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,"
		"0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,2",
};

/* test invalid parameters */
int
test_parse_portlist_invalid_param(void)
{
	cmdline_portlist_t result;
	char buf[CMDLINE_TEST_BUFSIZE];
	int ret;

	memset(&buf, 0, sizeof(buf));
	memset(&result, 0, sizeof(cmdline_portlist_t));

	/* try all null */
	ret = cmdline_parse_portlist(NULL, NULL, NULL, 0);
	if (ret != -1) {
		printf("Error: parser accepted null parameters!\n");
		return -1;
	}

	/* try null buf */
	ret = cmdline_parse_portlist(NULL, NULL, (void*)&result,
		sizeof(result));
	if (ret != -1) {
		printf("Error: parser accepted null string!\n");
		return -1;
	}

	/* try null result */
	ret = cmdline_parse_portlist(NULL, portlist_valid_strs[0].str, NULL, 0);
	if (ret == -1) {
		printf("Error: parser rejected null result!\n");
		return -1;
	}

	/* token is not used in ether_parse anyway so there's no point in
	 * testing it */

	/* test help function */

	/* coverage! */
	ret = cmdline_get_help_portlist(NULL, buf, sizeof(buf));
	if (ret < 0) {
		printf("Error: help function failed with valid parameters!\n");
		return -1;
	}

	return 0;
}

/* test valid parameters but invalid data */
int
test_parse_portlist_invalid_data(void)
{
	int ret = 0;
	unsigned i;
	cmdline_portlist_t result;

	/* test invalid strings */
	for (i = 0; i < RTE_DIM(portlist_invalid_strs); i++) {

		memset(&result, 0, sizeof(cmdline_portlist_t));

		ret = cmdline_parse_portlist(NULL, portlist_invalid_strs[i],
			(void*)&result, sizeof(result));
		if (ret != -1) {
			printf("Error: parsing %s succeeded!\n",
					portlist_invalid_strs[i]);
			return -1;
		}
	}

	return 0;
}

/* test valid parameters and data */
int
test_parse_portlist_valid(void)
{
	int ret = 0;
	unsigned i;
	cmdline_portlist_t result;

	/* test full strings */
	for (i = 0; i < RTE_DIM(portlist_valid_strs); i++) {

		memset(&result, 0, sizeof(cmdline_portlist_t));

		ret = cmdline_parse_portlist(NULL, portlist_valid_strs[i].str,
			(void*)&result, sizeof(result));
		if (ret < 0) {
			printf("Error: parsing %s failed!\n",
					portlist_valid_strs[i].str);
			return -1;
		}
		if (result.map != portlist_valid_strs[i].portmap) {
			printf("Error: parsing %s failed: map mismatch!\n",
					portlist_valid_strs[i].str);
			return -1;
		}
	}

	/* test garbage strings */
	for (i = 0; i < RTE_DIM(portlist_garbage_strs); i++) {

		memset(&result, 0, sizeof(cmdline_portlist_t));

		ret = cmdline_parse_portlist(NULL, portlist_garbage_strs[i],
			(void*)&result, sizeof(result));
		if (ret < 0) {
			printf("Error: parsing %s failed!\n",
					portlist_garbage_strs[i]);
			return -1;
		}
		if (result.map != UINT32_MAX) {
			printf("Error: parsing %s failed: map mismatch!\n",
					portlist_garbage_strs[i]);
			return -1;
		}
	}

	return 0;
}
