/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>

#include <errno.h>
#include <stdint.h>
#include <rte_cpuflags.h>
#include <rte_debug.h>

#include "test.h"


/* convenience define */
#define CHECK_FOR_FLAG(x) \
			result = rte_cpu_get_flag_enabled(x);    \
			printf("%s\n", cpu_flag_result(result)); \
			if (result == -ENOENT)                   \
				return -1;

/*
 * Helper function to display result
 */
static inline const char *
cpu_flag_result(int result)
{
	switch (result) {
	case 0:
		return "NOT PRESENT";
	case 1:
		return "OK";
	default:
		return "ERROR";
	}
}



/*
 * CPUID test
 * ===========
 *
 * - Check flags from different registers with rte_cpu_get_flag_enabled()
 * - Check if register and CPUID functions fail properly
 */

static int
test_cpuflags(void)
{
	int result;
	printf("\nChecking for flags from different registers...\n");

#ifdef RTE_ARCH_PPC_64
	printf("Check for PPC64:\t\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_PPC64);

	printf("Check for PPC32:\t\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_PPC32);

	printf("Check for VSX:\t\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_VSX);

	printf("Check for DFP:\t\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_DFP);

	printf("Check for FPU:\t\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_FPU);

	printf("Check for SMT:\t\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_SMT);

	printf("Check for MMU:\t\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_MMU);

	printf("Check for ALTIVEC:\t\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_ALTIVEC);

	printf("Check for ARCH_2_06:\t\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_ARCH_2_06);

	printf("Check for ARCH_2_07:\t\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_ARCH_2_07);

	printf("Check for ICACHE_SNOOP:\t\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_ICACHE_SNOOP);
#endif

#if defined(RTE_ARCH_ARM)
	printf("Check for NEON:\t\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_NEON);
#endif

#if defined(RTE_ARCH_ARM64)
	printf("Check for FP:\t\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_FP);

	printf("Check for ASIMD:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_NEON);

	printf("Check for EVTSTRM:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_EVTSTRM);

	printf("Check for AES:\t\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_AES);

	printf("Check for PMULL:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_PMULL);

	printf("Check for SHA1:\t\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_SHA1);

	printf("Check for SHA2:\t\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_SHA2);

	printf("Check for CRC32:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_CRC32);

	printf("Check for ATOMICS:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_ATOMICS);
#endif

#if defined(RTE_ARCH_X86_64) || defined(RTE_ARCH_I686)
	printf("Check for SSE:\t\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_SSE);

	printf("Check for SSE2:\t\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_SSE2);

	printf("Check for SSE3:\t\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_SSE3);

	printf("Check for SSE4.1:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_SSE4_1);

	printf("Check for SSE4.2:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_SSE4_2);

	printf("Check for AVX:\t\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_AVX);

	printf("Check for AVX2:\t\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_AVX2);

	printf("Check for AVX512F:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_AVX512F);

	printf("Check for TRBOBST:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_TRBOBST);

	printf("Check for ENERGY_EFF:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_ENERGY_EFF);

	printf("Check for LAHF_SAHF:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_LAHF_SAHF);

	printf("Check for 1GB_PG:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_1GB_PG);

	printf("Check for INVTSC:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_INVTSC);
#endif

	/*
	 * Check if invalid data is handled properly
	 */
	printf("\nCheck for invalid flag:\t");
	result = rte_cpu_get_flag_enabled(RTE_CPUFLAG_NUMFLAGS);
	printf("%s\n", cpu_flag_result(result));
	if (result != -ENOENT)
		return -1;

	return 0;
}

REGISTER_TEST_COMMAND(cpuflags_autotest, test_cpuflags);
