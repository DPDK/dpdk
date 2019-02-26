/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef TEST_CMDLINE_H_
#define TEST_CMDLINE_H_

#define CMDLINE_TEST_BUFSIZE 64

/* cmdline_parse_num tests */
int test_parse_num_valid(void);
int test_parse_num_invalid_data(void);
int test_parse_num_invalid_param(void);

/* cmdline_parse_etheraddr tests */
int test_parse_etheraddr_valid(void);
int test_parse_etheraddr_invalid_data(void);
int test_parse_etheraddr_invalid_param(void);

/* cmdline_parse_portlist tests */
int test_parse_portlist_valid(void);
int test_parse_portlist_invalid_data(void);
int test_parse_portlist_invalid_param(void);

/* cmdline_parse_ipaddr tests */
int test_parse_ipaddr_valid(void);
int test_parse_ipaddr_invalid_data(void);
int test_parse_ipaddr_invalid_param(void);

/* cmdline_parse_string tests */
int test_parse_string_valid(void);
int test_parse_string_invalid_data(void);
int test_parse_string_invalid_param(void);

/* cmdline_cirbuf tests */
int test_cirbuf_invalid_param(void);
int test_cirbuf_char(void);
int test_cirbuf_string(void);
int test_cirbuf_align(void);

/* test the rest of the library */
int test_cmdline_lib(void);

#endif /* TEST_CMDLINE_H_ */
