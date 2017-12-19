/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

/* Test prototypes */
int test_port_ring_reader(void);
int test_port_ring_writer(void);

/* Extern variables */
typedef int (*port_test)(void);

extern port_test port_tests[];
extern unsigned n_port_tests;
