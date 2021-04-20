/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#ifndef _RTE_PAUSE_PPC64_H_
#define _RTE_PAUSE_PPC64_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "rte_atomic.h"

#include "generic/rte_pause.h"

static inline void rte_pause(void)
{
	/* Set hardware multi-threading low priority */
	asm volatile("or 1,1,1");
	/* Set hardware multi-threading medium priority */
	asm volatile("or 2,2,2");
	rte_compiler_barrier();
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_PAUSE_PPC64_H_ */
