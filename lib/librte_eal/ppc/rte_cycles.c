/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) IBM Corporation 2019.
 */

#ifdef linux
#include <features.h>
#endif

#ifdef __GLIBC__
#include <sys/platform/ppc.h>
#endif

#include "eal_private.h"

uint64_t
get_tsc_freq_arch(void)
{
#ifdef __GLIBC__
	return __ppc_get_timebase_freq();
#else
	uint64_t __tb;
	__asm__ volatile ("mfspr %0, 268" : "=r" (__tb));
	return __tb;
#endif
}
