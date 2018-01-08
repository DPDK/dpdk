/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
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
	void **stack_top = (void *)((char *)(lt->stack) + lt->stack_size);

	/*
	 * Align stack_top to 16 bytes. Arm64 has the constraint that the
	 * stack pointer must always be quad-word aligned.
	 */
	stack_top = (void **)(((unsigned long)(stack_top)) & ~0xfUL);

	/*
	 * First Stack Frame
	 */
	stack_top[0] = NULL;
	stack_top[-1] = NULL;

	/*
	 * Initialize the context
	 */
	lt->ctx.fp = &stack_top[-1];
	lt->ctx.sp = &stack_top[-2];

	/*
	 * Here only the address of _lthread_exec is saved as the link
	 * register value. The argument to _lthread_exec i.e the address of
	 * the lthread struct is not saved. This is because the first
	 * argument to ctx_switch is the address of the new context,
	 * which also happens to be the address of required lthread struct.
	 * So while returning from ctx_switch into _thread_exec, parameter
	 * register x0 will always contain the required value.
	 */
	lt->ctx.lr = func;
}

#ifdef __cplusplus
}
#endif

#endif /* STACK_H_ */
