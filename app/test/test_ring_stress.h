/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */


#include <inttypes.h>
#include <stddef.h>
#include <stdalign.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include <rte_ring.h>
#include <rte_cycles.h>
#include <rte_launch.h>
#include <rte_pause.h>
#include <rte_random.h>
#include <rte_malloc.h>
#include <rte_spinlock.h>

#include "test.h"

struct test_case {
	const char *name;
	int (*func)(int (*)(void *));
	int (*wfunc)(void *arg);
};

struct test {
	const char *name;
	uint32_t nb_case;
	const struct test_case *cases;
};

extern const struct test test_ring_mpmc_stress;
extern const struct test test_ring_rts_stress;
extern const struct test test_ring_hts_stress;
extern const struct test test_ring_mt_peek_stress;
extern const struct test test_ring_mt_peek_stress_zc;
extern const struct test test_ring_st_peek_stress;
extern const struct test test_ring_st_peek_stress_zc;
