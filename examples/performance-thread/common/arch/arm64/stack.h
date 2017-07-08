/*
 *   BSD LICENSE
 *
 *   Copyright (C) Cavium, Inc. 2017.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Cavium, Inc nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
