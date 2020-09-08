/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2015-2018 Atomic Rules LLC
 */

#ifndef _ARK_DEBUG_H_
#define _ARK_DEBUG_H_

#include <inttypes.h>
#include <rte_log.h>

/* system camel case definition changed to upper case */
#define PRIU32 PRIu32
#define PRIU64 PRIu64

/* Format specifiers for string data pairs */
#define ARK_SU32  "\n\t%-20s    %'20" PRIU32
#define ARK_SU64  "\n\t%-20s    %'20" PRIU64
#define ARK_SU64X "\n\t%-20s    %#20" PRIx64
#define ARK_SPTR  "\n\t%-20s    %20p"

extern int ark_logtype;

#define ARK_PMD_LOG(level, fmt, args...)	\
	rte_log(RTE_LOG_ ##level, ark_logtype, "ARK: " fmt, ## args)


/* Debug macro to enable core debug code */
#ifdef RTE_LIBRTE_ETHDEV_DEBUG
#define ARK_DEBUG_CORE 1
#else
#define ARK_DEBUG_CORE 0
#endif

#endif
