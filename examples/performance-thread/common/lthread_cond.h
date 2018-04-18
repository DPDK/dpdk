/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 Intel Corporation.
 * Copyright 2012 Hasan Alayli <halayli@gmail.com>
 */

#ifndef LTHREAD_COND_H_
#define LTHREAD_COND_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "lthread_queue.h"

#define MAX_COND_NAME_SIZE 64

struct lthread_cond {
	struct lthread_queue *blocked;
	struct lthread_sched *root_sched;
	int count;
	char name[MAX_COND_NAME_SIZE];
	uint64_t diag_ref;	/* optional ref to user diag data */
} __rte_cache_aligned;

#ifdef __cplusplus
}
#endif

#endif				/* LTHREAD_COND_H_ */
