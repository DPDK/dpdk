/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015 Cavium, Inc
 */

#ifndef _RTE_CPUFLAGS_ARM64_H_
#define _RTE_CPUFLAGS_ARM64_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Enumeration of all CPU features supported
 */
enum rte_cpu_flag_t {
	RTE_CPUFLAG_FP = 0,
	RTE_CPUFLAG_NEON,
	RTE_CPUFLAG_EVTSTRM,
	RTE_CPUFLAG_AES,
	RTE_CPUFLAG_PMULL,
	RTE_CPUFLAG_SHA1,
	RTE_CPUFLAG_SHA2,
	RTE_CPUFLAG_CRC32,
	RTE_CPUFLAG_ATOMICS,
	RTE_CPUFLAG_AARCH64,
	/* The last item */
	RTE_CPUFLAG_NUMFLAGS,/**< This should always be the last! */
};

#include "generic/rte_cpuflags.h"

#ifdef __cplusplus
}
#endif

#endif /* _RTE_CPUFLAGS_ARM64_H_ */
