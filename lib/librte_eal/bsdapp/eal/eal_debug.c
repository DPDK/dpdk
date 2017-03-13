/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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

#ifdef RTE_BACKTRACE
#include <execinfo.h>
#endif
#include <stdarg.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <rte_log.h>
#include <rte_debug.h>
#include <rte_common.h>

#define BACKTRACE_SIZE 256

/* dump the stack of the calling core */
void rte_dump_stack(void)
{
#ifdef RTE_BACKTRACE
	void *func[BACKTRACE_SIZE];
	char **symb = NULL;
	int size;

	size = backtrace(func, BACKTRACE_SIZE);
	symb = backtrace_symbols(func, size);

	if (symb == NULL)
		return;

	while (size > 0) {
		rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL,
			"%d: [%s]\n", size, symb[size - 1]);
		size --;
	}

	free(symb);
#endif /* RTE_BACKTRACE */
}

/* not implemented in this environment */
void rte_dump_registers(void)
{
	return;
}

/* call abort(), it will generate a coredump if enabled */
void __rte_panic(const char *funcname, const char *format, ...)
{
	va_list ap;

	rte_log(RTE_LOG_CRIT, RTE_LOGTYPE_EAL, "PANIC in %s():\n", funcname);
	va_start(ap, format);
	rte_vlog(RTE_LOG_CRIT, RTE_LOGTYPE_EAL, format, ap);
	va_end(ap);
	rte_dump_stack();
	rte_dump_registers();
	abort();
}

/*
 * Like rte_panic this terminates the application. However, no traceback is
 * provided and no core-dump is generated.
 */
void
rte_exit(int exit_code, const char *format, ...)
{
	va_list ap;

	if (exit_code != 0)
		RTE_LOG(CRIT, EAL, "Error - exiting with code: %d\n"
				"  Cause: ", exit_code);

	va_start(ap, format);
	rte_vlog(RTE_LOG_CRIT, RTE_LOGTYPE_EAL, format, ap);
	va_end(ap);

#ifndef RTE_EAL_ALWAYS_PANIC_ON_ERROR
	exit(exit_code);
#else
	rte_dump_stack();
	rte_dump_registers();
	abort();
#endif
}
