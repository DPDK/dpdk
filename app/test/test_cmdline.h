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
