/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 */

/* Test prototypes */
int test_table_stub_combined(void);
int test_table_lpm_combined(void);
int test_table_lpm_ipv6_combined(void);
#ifdef RTE_LIB_ACL
int test_table_acl(void);
#endif
int test_table_hash8unoptimized(void);
int test_table_hash8lru(void);
int test_table_hash8ext(void);
int test_table_hash16unoptimized(void);
int test_table_hash16lru(void);
int test_table_hash16ext(void);
int test_table_hash32unoptimized(void);
int test_table_hash32lru(void);
int test_table_hash32ext(void);
int test_table_hash_cuckoo_combined(void);

/* Extern variables */
typedef int (*combined_table_test)(void);

extern combined_table_test table_tests_combined[];
extern unsigned n_table_tests_combined;
