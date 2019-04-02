/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include <stdarg.h>
#include <rte_log.h>

 /* call abort(), it will generate a coredump if enabled */
void
__rte_panic(const char *funcname, const char *format, ...)
{
	va_list ap;

	rte_log(RTE_LOG_CRIT, RTE_LOGTYPE_EAL, "PANIC in %s():\n", funcname);
	va_start(ap, format);
	rte_vlog(RTE_LOG_CRIT, RTE_LOGTYPE_EAL, format, ap);
	va_end(ap);
	abort();
}
