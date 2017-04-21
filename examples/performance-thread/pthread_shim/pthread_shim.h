/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2015 Intel Corporation. All rights reserved.
 *   All rights reserved.
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
 *     * Neither the name of Intel Corporation nor the names of its
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

#ifndef _PTHREAD_SHIM_H_
#define _PTHREAD_SHIM_H_

#include <rte_lcore.h>

/*
 * This pthread shim is an example that demonstrates how legacy code
 * that makes use of POSIX pthread services can make use of lthreads
 * with reduced porting effort.
 *
 * N.B. The example is not a complete implementation, only a subset of
 * pthread APIs sufficient to demonstrate the principle of operation
 * are implemented.
 *
 * In general pthread attribute objects do not have equivalent functions
 * in lthreads, and are ignored.
 *
 * There is one exception and that is the use of attr to specify a
 * core affinity in calls to pthread_create.
 *
 * The shim operates as follows:-
 *
 * On initialisation a constructor function uses dlsym to obtain and
 * save the loaded address of the full set of pthread APIs that will
 * be overridden.
 *
 * For each function there is a stub provided that will invoke either
 * the genuine pthread library function saved saved by the constructor,
 * or else the corresponding equivalent lthread function.
 *
 * The stub functions are implemented in pthread_shim.c
 *
 * The stub will take care of adapting parameters, and any police
 * any constraints where lthread functionality differs.
 *
 * The initial thread must always be a pure lthread.
 *
 * The decision whether to invoke the real library function or the lthread
 * function is controlled by a per pthread flag that can be switched
 * on of off by the pthread_override_set() API described below. Typcially
 * this should be done as the first action of the initial lthread.
 *
 * N.B In general it would be poor practice to revert to invoke a real
 * pthread function when running as an lthread, since these may block and
 * effectively stall the lthread scheduler.
 *
 */


/*
 * An exiting lthread must not terminate the pthread it is running in
 * since this would mean terminating the lthread scheduler.
 * We override pthread_exit() with a macro because it is typically declared with
 * __attribute__((noreturn))
 */
void pthread_exit_override(void *v);

#define pthread_exit(v) do { \
	pthread_exit_override((v));	\
	return NULL;	\
} while (0)

/*
 * Enable/Disable pthread override
 * state
 * 0 disable
 * 1 enable
 */
void pthread_override_set(int state);


/*
 * Return pthread override state
 * return
 * 0 disable
 * 1 enable
 */
int pthread_override_get(void);


#endif /* _PTHREAD_SHIM_H_ */
