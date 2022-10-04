/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Loongson Technology Corporation Limited
 */

#include "eal_private.h"

#define LOONGARCH_CPUCFG4	0x4
#define CPUCFG4_CCFREQ_MASK	0xFFFFFFFF
#define CPUCFG4_CCFREQ_SHIFT	0

#define LOONGARCH_CPUCFG5	0x5
#define CPUCFG5_CCMUL_MASK	0xFFFF
#define CPUCFG5_CCMUL_SHIFT	0

#define CPUCFG5_CCDIV_MASK	0xFFFF0000
#define CPUCFG5_CCDIV_SHIFT	16

static __rte_noinline uint32_t
read_cpucfg(int arg)
{
	int ret = 0;

	__asm__ __volatile__ (
		"cpucfg %[var], %[index]\n"
		: [var]"=r"(ret)
		: [index]"r"(arg)
		:
		);

	return ret;
}

uint64_t
get_tsc_freq_arch(void)
{
	uint32_t base_freq, mul_factor, div_factor;

	base_freq = read_cpucfg(LOONGARCH_CPUCFG4);
	mul_factor = (read_cpucfg(LOONGARCH_CPUCFG5) & CPUCFG5_CCMUL_MASK) >>
		CPUCFG5_CCMUL_SHIFT;
	div_factor = (read_cpucfg(LOONGARCH_CPUCFG5) & CPUCFG5_CCDIV_MASK) >>
		CPUCFG5_CCDIV_SHIFT;

	return base_freq * mul_factor / div_factor;
}
