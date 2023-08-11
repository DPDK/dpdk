/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Loongson Technology Corporation Limited
 */

#ifndef RTE_CPUFLAGS_LOONGARCH_H
#define RTE_CPUFLAGS_LOONGARCH_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Enumeration of all CPU features supported
 */
enum rte_cpu_flag_t {
	RTE_CPUFLAG_CPUCFG = 0,
	RTE_CPUFLAG_LAM,
	RTE_CPUFLAG_UAL,
	RTE_CPUFLAG_FPU,
	RTE_CPUFLAG_LSX,
	RTE_CPUFLAG_LASX,
	RTE_CPUFLAG_CRC32,
	RTE_CPUFLAG_COMPLEX,
	RTE_CPUFLAG_CRYPTO,
	RTE_CPUFLAG_LVZ,
	RTE_CPUFLAG_LBT_X86,
	RTE_CPUFLAG_LBT_ARM,
	RTE_CPUFLAG_LBT_MIPS,
};

#include "generic/rte_cpuflags.h"

#ifdef __cplusplus
}
#endif

#endif /* RTE_CPUFLAGS_LOONGARCH_H */
