/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 Intel Corporation.
 * Copyright 2012 Hasan Alayli <halayli@gmail.com>
 */
#ifndef LTHREAD_H_
#define LTHREAD_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_per_lcore.h>

#include "lthread_api.h"
#include "lthread_diag.h"

struct lthread;
struct lthread_sched;

/* function to be called when a context function returns */
typedef void (*lthread_exit_func) (struct lthread *);

void _lthread_exit_handler(struct lthread *lt);

void lthread_set_funcname(const char *f);

void _lthread_sched_busy_sleep(struct lthread *lt, uint64_t nsecs);

int _lthread_desched_sleep(struct lthread *lt);

void _lthread_free(struct lthread *lt);

struct lthread_sched *_lthread_sched_get(unsigned int lcore_id);

struct lthread_stack *_stack_alloc(void);

struct
lthread_sched *_lthread_sched_create(size_t stack_size);

void
_lthread_init(struct lthread *lt,
	      lthread_func_t fun, void *arg, lthread_exit_func exit_handler);

void _lthread_set_stack(struct lthread *lt, void *stack, size_t stack_size);

#ifdef __cplusplus
}
#endif

#endif				/* LTHREAD_H_ */
