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

#if defined(RTE_ARCH_ARM) && defined(RTE_ARCH_32)
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

	printf("Check for SVE:\t\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_SVE);

	printf("Check for SVE2:\t\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_SVE2);

	printf("Check for SVEAES:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_SVEAES);

	printf("Check for SVEPMULL:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_SVEPMULL);

	printf("Check for SVEBITPERM:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_SVEBITPERM);

	printf("Check for SVESHA3:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_SVESHA3);

	printf("Check for SVESM4:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_SVESM4);

	printf("Check for FLAGM2:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_FLAGM2);

	printf("Check for FRINT:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_FRINT);

	printf("Check for SVEI8MM:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_SVEI8MM);

	printf("Check for SVEF32MM:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_SVEF32MM);

	printf("Check for SVEF64MM:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_SVEF64MM);

	printf("Check for SVEBF16:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_SVEBF16);
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

#if defined(RTE_ARCH_RISCV)

	printf("Check for RISCV_ISA_A:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_RISCV_ISA_A);

	printf("Check for RISCV_ISA_B:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_RISCV_ISA_B);

	printf("Check for RISCV_ISA_C:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_RISCV_ISA_C);

	printf("Check for RISCV_ISA_D:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_RISCV_ISA_D);

	printf("Check for RISCV_ISA_E:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_RISCV_ISA_E);

	printf("Check for RISCV_ISA_F:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_RISCV_ISA_F);

	printf("Check for RISCV_ISA_G:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_RISCV_ISA_G);

	printf("Check for RISCV_ISA_H:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_RISCV_ISA_H);

	printf("Check for RISCV_ISA_I:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_RISCV_ISA_I);

	printf("Check for RISCV_ISA_J:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_RISCV_ISA_J);

	printf("Check for RISCV_ISA_K:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_RISCV_ISA_K);

	printf("Check for RISCV_ISA_L:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_RISCV_ISA_L);

	printf("Check for RISCV_ISA_M:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_RISCV_ISA_M);

	printf("Check for RISCV_ISA_N:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_RISCV_ISA_N);

	printf("Check for RISCV_ISA_O:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_RISCV_ISA_O);

	printf("Check for RISCV_ISA_P:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_RISCV_ISA_P);

	printf("Check for RISCV_ISA_Q:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_RISCV_ISA_Q);

	printf("Check for RISCV_ISA_R:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_RISCV_ISA_R);

	printf("Check for RISCV_ISA_S:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_RISCV_ISA_S);

	printf("Check for RISCV_ISA_T:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_RISCV_ISA_T);

	printf("Check for RISCV_ISA_U:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_RISCV_ISA_U);

	printf("Check for RISCV_ISA_V:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_RISCV_ISA_V);

	printf("Check for RISCV_ISA_W:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_RISCV_ISA_W);

	printf("Check for RISCV_ISA_X:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_RISCV_ISA_X);

	printf("Check for RISCV_ISA_Y:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_RISCV_ISA_Y);

	printf("Check for RISCV_ISA_Z:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_RISCV_ISA_Z);
#endif

#if defined(RTE_ARCH_LOONGARCH)
	printf("Check for CPUCFG:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_CPUCFG);

	printf("Check for LAM:\t\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_LAM);

	printf("Check for UAL:\t\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_UAL);

	printf("Check for FPU:\t\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_FPU);

	printf("Check for LSX:\t\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_LSX);

	printf("Check for LASX:\t\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_LASX);

	printf("Check for CRC32:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_CRC32);

	printf("Check for COMPLEX:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_COMPLEX);

	printf("Check for CRYPTO:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_CRYPTO);

	printf("Check for LVZ:\t\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_LVZ);

	printf("Check for LBT_X86:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_LBT_X86);

	printf("Check for LBT_ARM:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_LBT_ARM);

	printf("Check for LBT_MIPS:\t");
	CHECK_FOR_FLAG(RTE_CPUFLAG_LBT_MIPS);
#endif

	return 0;
}

REGISTER_FAST_TEST(cpuflags_autotest, true, true, test_cpuflags);
