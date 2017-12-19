/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>

#include "test.h"
#include "test_cmdline.h"

static int
test_cmdline(void)
{
	printf("Testind parsing ethernet addresses...\n");
	if (test_parse_etheraddr_valid() < 0)
		return -1;
	if (test_parse_etheraddr_invalid_data() < 0)
		return -1;
	if (test_parse_etheraddr_invalid_param() < 0)
		return -1;
	printf("Testind parsing port lists...\n");
	if (test_parse_portlist_valid() < 0)
		return -1;
	if (test_parse_portlist_invalid_data() < 0)
		return -1;
	if (test_parse_portlist_invalid_param() < 0)
		return -1;
	printf("Testind parsing numbers...\n");
	if (test_parse_num_valid() < 0)
		return -1;
	if (test_parse_num_invalid_data() < 0)
		return -1;
	if (test_parse_num_invalid_param() < 0)
		return -1;
	printf("Testing parsing IP addresses...\n");
	if (test_parse_ipaddr_valid() < 0)
		return -1;
	if (test_parse_ipaddr_invalid_data() < 0)
		return -1;
	if (test_parse_ipaddr_invalid_param() < 0)
		return -1;
	printf("Testing parsing strings...\n");
	if (test_parse_string_valid() < 0)
		return -1;
	if (test_parse_string_invalid_data() < 0)
		return -1;
	if (test_parse_string_invalid_param() < 0)
		return -1;
	printf("Testing circular buffer...\n");
	if (test_cirbuf_char() < 0)
		return -1;
	if (test_cirbuf_string() < 0)
		return -1;
	if (test_cirbuf_align() < 0)
		return -1;
	if (test_cirbuf_invalid_param() < 0)
		return -1;
	printf("Testing library functions...\n");
	if (test_cmdline_lib() < 0)
		return -1;
	return 0;
}

REGISTER_TEST_COMMAND(cmdline_autotest, test_cmdline);
