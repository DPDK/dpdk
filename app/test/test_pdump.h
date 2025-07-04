/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef _TEST_PDUMP_H_
#define _TEST_PDUMP_H_

#define QUEUE_ID 0
#define NUM_ITR 3

/* sample test to send packets to the pdump client recursively */
uint32_t send_pkts(void *empty);

/* Sample test to create setup for the pdump server tests */
int test_pdump_init(void);

/* Sample test to teardown the pdump server setup */
int test_pdump_uninit(void);

/* Sample test to run the pdump client tests */
int run_pdump_client_tests(void);

/* Sample test to run the pdump server tests */
int run_pdump_server_tests(void);

/* Sample test to run the pdump client and server tests based on
 * the process type
 */
int test_pdump(void);

#endif /* _TEST_PDUMP_H_ */
