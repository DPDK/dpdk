/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

/* Use XSI-compliant portable version of strerror_r() */
#undef _GNU_SOURCE

#include <stdio.h>
#include <string.h>

#include <rte_per_lcore.h>
#include <rte_errno.h>

#ifdef RTE_EXEC_ENV_WINDOWS
#define strerror_r(errnum, buf, buflen) strerror_s(buf, buflen, errnum)
#endif

RTE_DEFINE_PER_LCORE(int, _rte_errno);

const char *
rte_strerror(int errnum)
{
	/* BSD puts a colon in the "unknown error" messages, Linux doesn't */
#ifdef RTE_EXEC_ENV_FREEBSD
	static const char *sep = ":";
#else
	static const char *sep = "";
#endif
#define RETVAL_SZ 256
	static RTE_DEFINE_PER_LCORE(char[RETVAL_SZ], retval);
	char *ret = RTE_PER_LCORE(retval);

	/* since some implementations of strerror_r throw an error
	 * themselves if errnum is too big, we handle that case here */
	if (errnum >= RTE_MAX_ERRNO)
#ifdef RTE_EXEC_ENV_WINDOWS
		snprintf(ret, RETVAL_SZ, "Unknown error");
#else
		snprintf(ret, RETVAL_SZ, "Unknown error%s %d", sep, errnum);
#endif
	else
		switch (errnum){
		case E_RTE_SECONDARY:
			return "Invalid call in secondary process";
		case E_RTE_NO_CONFIG:
			return "Missing rte_config structure";
		default:
			if (strerror_r(errnum, ret, RETVAL_SZ) != 0)
				snprintf(ret, RETVAL_SZ, "Unknown error%s %d",
						sep, errnum);
		}

	return ret;
}
