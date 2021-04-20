/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) IBM Corporation 2019.
 */

#include <sys/platform/ppc.h>

#include "eal_private.h"

uint64_t
get_tsc_freq_arch(void)
{
	return __ppc_get_timebase_freq();
}
