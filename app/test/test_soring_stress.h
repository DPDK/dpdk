/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Huawei Technologies Co., Ltd
 */

#include <inttypes.h>
#include <stddef.h>
#include <stdalign.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include <rte_soring.h>
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

extern const struct test test_soring_mt_stress;
