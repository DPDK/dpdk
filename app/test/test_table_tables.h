/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 */

/* Test prototypes */
int test_table_hash_cuckoo(void);
int test_table_lpm(void);
int test_table_lpm_ipv6(void);
int test_table_array(void);
#ifdef RTE_LIB_ACL
int test_table_acl(void);
#endif
int test_table_hash_unoptimized(void);
int test_table_hash_lru(void);
int test_table_hash_ext(void);
int test_table_stub(void);

/* Extern variables */
typedef int (*table_test)(void);

extern table_test table_tests[];
extern unsigned n_table_tests;
