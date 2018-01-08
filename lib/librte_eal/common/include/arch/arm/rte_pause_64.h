/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#ifndef _RTE_PAUSE_ARM64_H_
#define _RTE_PAUSE_ARM64_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_common.h>
#include "generic/rte_pause.h"

static inline void rte_pause(void)
{
	asm volatile("yield" ::: "memory");
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_PAUSE_ARM64_H_ */
