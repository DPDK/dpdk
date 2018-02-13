/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015 Intel Corporation.
 * Copyright(c) Cavium, Inc. 2017.
 * All rights reserved
 * Copyright (C) 2012, Hasan Alayli <halayli@gmail.com>
 * Portions derived from: https://github.com/halayli/lthread
 * With permissions from Hasan Alayli to use them as BSD-3-Clause
 */

#ifndef STACK_H
#define STACK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lthread_int.h"

/*
 * Sets up the initial stack for the lthread.
 */
static inline void
arch_set_stack(struct lthread *lt, void *func)
{
	char *stack_top = (char *)(lt->stack) + lt->stack_size;
	void **s = (void **)stack_top;

	/* set initial context */
	s[-3] = NULL;
	s[-2] = (void *)lt;
	lt->ctx.rsp = (void *)(stack_top - (4 * sizeof(void *)));
	lt->ctx.rbp = (void *)(stack_top - (3 * sizeof(void *)));
	lt->ctx.rip = func;
}

#ifdef __cplusplus
}
#endif

#endif /* STACK_H_ */
