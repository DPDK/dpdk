/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>

#include <rte_eal.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_errno.h>

#include <eal_export.h>
#include "eal_private.h"

RTE_EXPORT_SYMBOL(__rte_panic)
void
__rte_panic(const char *funcname, const char *format, ...)
{
	va_list ap;

	EAL_LOG(CRIT, "PANIC in %s():", funcname);
	va_start(ap, format);
	rte_vlog(RTE_LOG_CRIT, RTE_LOGTYPE_EAL, format, ap);
	va_end(ap);
	rte_dump_stack();
	abort(); /* generate a coredump if enabled */
}

/*
 * Like rte_panic this terminates the application. However, no traceback is
 * provided and no core-dump is generated.
 */
RTE_EXPORT_SYMBOL(rte_exit)
void
rte_exit(int exit_code, const char *format, ...)
{
	va_list ap;

	if (exit_code != 0)
		EAL_LOG(CRIT, "Error - exiting with code: %d", exit_code);

	va_start(ap, format);
	rte_vlog(RTE_LOG_CRIT, RTE_LOGTYPE_EAL, format, ap);
	va_end(ap);

	if (rte_eal_cleanup() != 0 && rte_errno != EALREADY)
		EAL_LOG(CRIT, "EAL could not release all resources");
	exit(exit_code);
}
