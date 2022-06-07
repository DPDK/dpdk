/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 StarFive
 * Copyright(c) 2022 SiFive
 * Copyright(c) 2022 Semihalf
 */

#ifndef RTE_PAUSE_RISCV_H
#define RTE_PAUSE_RISCV_H

#ifdef __cplusplus
extern "C" {
#endif

#include "rte_atomic.h"

#include "generic/rte_pause.h"

static inline void rte_pause(void)
{
	/* Insert pause hint directly to be compatible with old compilers.
	 * This will work even on platforms without Zihintpause extension
	 * because this is a FENCE hint instruction which evaluates to NOP.
	 */
	asm volatile(".int 0x0100000F" : : : "memory");
}

#ifdef __cplusplus
}
#endif

#endif /* RTE_PAUSE_RISCV_H */
