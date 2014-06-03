/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <rte_cpuflags.h>

/*
 * This should prevent use of advanced instruction sets in this file. Otherwise
 * the check function itself could cause a crash.
 */
#ifdef __INTEL_COMPILER
#pragma optimize ("", off)
#else
#define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#if GCC_VERSION > 404000
#pragma GCC optimize ("O0")
#endif
#endif

/**
 * Enumeration of CPU registers
 */
enum cpu_register_t {
	REG_EAX = 0,
	REG_EBX,
	REG_ECX,
	REG_EDX,
};

typedef uint32_t cpuid_registers_t[4];

#define CPU_FLAG_NAME_MAX_LEN 64

/**
 * Struct to hold a processor feature entry
 */
struct feature_entry {
	uint32_t leaf;				/**< cpuid leaf */
	uint32_t subleaf;			/**< cpuid subleaf */
	uint32_t reg;				/**< cpuid register */
	uint32_t bit;				/**< cpuid register bit */
	char name[CPU_FLAG_NAME_MAX_LEN];       /**< String for printing */
};

#define FEAT_DEF(name, leaf, subleaf, reg, bit) \
	[RTE_CPUFLAG_##name] = {leaf, subleaf, reg, bit, #name },

/**
 * An array that holds feature entries
 */
static const struct feature_entry cpu_feature_table[] = {
	FEAT_DEF(SSE3, 0x00000001, 0, REG_ECX,  0)
	FEAT_DEF(PCLMULQDQ, 0x00000001, 0, REG_ECX,  1)
	FEAT_DEF(DTES64, 0x00000001, 0, REG_ECX,  2)
	FEAT_DEF(MONITOR, 0x00000001, 0, REG_ECX,  3)
	FEAT_DEF(DS_CPL, 0x00000001, 0, REG_ECX,  4)
	FEAT_DEF(VMX, 0x00000001, 0, REG_ECX,  5)
	FEAT_DEF(SMX, 0x00000001, 0, REG_ECX,  6)
	FEAT_DEF(EIST, 0x00000001, 0, REG_ECX,  7)
	FEAT_DEF(TM2, 0x00000001, 0, REG_ECX,  8)
	FEAT_DEF(SSSE3, 0x00000001, 0, REG_ECX,  9)
	FEAT_DEF(CNXT_ID, 0x00000001, 0, REG_ECX, 10)
	FEAT_DEF(FMA, 0x00000001, 0, REG_ECX, 12)
	FEAT_DEF(CMPXCHG16B, 0x00000001, 0, REG_ECX, 13)
	FEAT_DEF(XTPR, 0x00000001, 0, REG_ECX, 14)
	FEAT_DEF(PDCM, 0x00000001, 0, REG_ECX, 15)
	FEAT_DEF(PCID, 0x00000001, 0, REG_ECX, 17)
	FEAT_DEF(DCA, 0x00000001, 0, REG_ECX, 18)
	FEAT_DEF(SSE4_1, 0x00000001, 0, REG_ECX, 19)
	FEAT_DEF(SSE4_2, 0x00000001, 0, REG_ECX, 20)
	FEAT_DEF(X2APIC, 0x00000001, 0, REG_ECX, 21)
	FEAT_DEF(MOVBE, 0x00000001, 0, REG_ECX, 22)
	FEAT_DEF(POPCNT, 0x00000001, 0, REG_ECX, 23)
	FEAT_DEF(TSC_DEADLINE, 0x00000001, 0, REG_ECX, 24)
	FEAT_DEF(AES, 0x00000001, 0, REG_ECX, 25)
	FEAT_DEF(XSAVE, 0x00000001, 0, REG_ECX, 26)
	FEAT_DEF(OSXSAVE, 0x00000001, 0, REG_ECX, 27)
	FEAT_DEF(AVX, 0x00000001, 0, REG_ECX, 28)
	FEAT_DEF(F16C, 0x00000001, 0, REG_ECX, 29)
	FEAT_DEF(RDRAND, 0x00000001, 0, REG_ECX, 30)

	FEAT_DEF(FPU, 0x00000001, 0, REG_EDX,  0)
	FEAT_DEF(VME, 0x00000001, 0, REG_EDX,  1)
	FEAT_DEF(DE, 0x00000001, 0, REG_EDX,  2)
	FEAT_DEF(PSE, 0x00000001, 0, REG_EDX,  3)
	FEAT_DEF(TSC, 0x00000001, 0, REG_EDX,  4)
	FEAT_DEF(MSR, 0x00000001, 0, REG_EDX,  5)
	FEAT_DEF(PAE, 0x00000001, 0, REG_EDX,  6)
	FEAT_DEF(MCE, 0x00000001, 0, REG_EDX,  7)
	FEAT_DEF(CX8, 0x00000001, 0, REG_EDX,  8)
	FEAT_DEF(APIC, 0x00000001, 0, REG_EDX,  9)
	FEAT_DEF(SEP, 0x00000001, 0, REG_EDX, 11)
	FEAT_DEF(MTRR, 0x00000001, 0, REG_EDX, 12)
	FEAT_DEF(PGE, 0x00000001, 0, REG_EDX, 13)
	FEAT_DEF(MCA, 0x00000001, 0, REG_EDX, 14)
	FEAT_DEF(CMOV, 0x00000001, 0, REG_EDX, 15)
	FEAT_DEF(PAT, 0x00000001, 0, REG_EDX, 16)
	FEAT_DEF(PSE36, 0x00000001, 0, REG_EDX, 17)
	FEAT_DEF(PSN, 0x00000001, 0, REG_EDX, 18)
	FEAT_DEF(CLFSH, 0x00000001, 0, REG_EDX, 19)
	FEAT_DEF(DS, 0x00000001, 0, REG_EDX, 21)
	FEAT_DEF(ACPI, 0x00000001, 0, REG_EDX, 22)
	FEAT_DEF(MMX, 0x00000001, 0, REG_EDX, 23)
	FEAT_DEF(FXSR, 0x00000001, 0, REG_EDX, 24)
	FEAT_DEF(SSE, 0x00000001, 0, REG_EDX, 25)
	FEAT_DEF(SSE2, 0x00000001, 0, REG_EDX, 26)
	FEAT_DEF(SS, 0x00000001, 0, REG_EDX, 27)
	FEAT_DEF(HTT, 0x00000001, 0, REG_EDX, 28)
	FEAT_DEF(TM, 0x00000001, 0, REG_EDX, 29)
	FEAT_DEF(PBE, 0x00000001, 0, REG_EDX, 31)

	FEAT_DEF(DIGTEMP, 0x00000006, 0, REG_EAX,  0)
	FEAT_DEF(TRBOBST, 0x00000006, 0, REG_EAX,  1)
	FEAT_DEF(ARAT, 0x00000006, 0, REG_EAX,  2)
	FEAT_DEF(PLN, 0x00000006, 0, REG_EAX,  4)
	FEAT_DEF(ECMD, 0x00000006, 0, REG_EAX,  5)
	FEAT_DEF(PTM, 0x00000006, 0, REG_EAX,  6)

	FEAT_DEF(MPERF_APERF_MSR, 0x00000006, 0, REG_ECX,  0)
	FEAT_DEF(ACNT2, 0x00000006, 0, REG_ECX,  1)
	FEAT_DEF(ENERGY_EFF, 0x00000006, 0, REG_ECX,  3)

	FEAT_DEF(FSGSBASE, 0x00000007, 0, REG_EBX,  0)
	FEAT_DEF(BMI1, 0x00000007, 0, REG_EBX,  2)
	FEAT_DEF(HLE, 0x00000007, 0, REG_EBX,  4)
	FEAT_DEF(AVX2, 0x00000007, 0, REG_EBX,  5)
	FEAT_DEF(SMEP, 0x00000007, 0, REG_EBX,  6)
	FEAT_DEF(BMI2, 0x00000007, 0, REG_EBX,  7)
	FEAT_DEF(ERMS, 0x00000007, 0, REG_EBX,  8)
	FEAT_DEF(INVPCID, 0x00000007, 0, REG_EBX, 10)
	FEAT_DEF(RTM, 0x00000007, 0, REG_EBX, 11)

	FEAT_DEF(LAHF_SAHF, 0x80000001, 0, REG_ECX,  0)
	FEAT_DEF(LZCNT, 0x80000001, 0, REG_ECX,  4)

	FEAT_DEF(SYSCALL, 0x80000001, 0, REG_EDX, 11)
	FEAT_DEF(XD, 0x80000001, 0, REG_EDX, 20)
	FEAT_DEF(1GB_PG, 0x80000001, 0, REG_EDX, 26)
	FEAT_DEF(RDTSCP, 0x80000001, 0, REG_EDX, 27)
	FEAT_DEF(EM64T, 0x80000001, 0, REG_EDX, 29)

	FEAT_DEF(INVTSC, 0x80000007, 0, REG_EDX,  8)
};

/*
 * Execute CPUID instruction and get contents of a specific register
 *
 * This function, when compiled with GCC, will generate architecture-neutral
 * code, as per GCC manual.
 */
static inline void
rte_cpu_get_features(uint32_t leaf, uint32_t subleaf, cpuid_registers_t out)
{
#if defined(__i386__) && defined(__PIC__)
    /* %ebx is a forbidden register if we compile with -fPIC or -fPIE */
    asm volatile("movl %%ebx,%0 ; cpuid ; xchgl %%ebx,%0"
		 : "=r" (out[REG_EBX]),
		   "=a" (out[REG_EAX]),
		   "=c" (out[REG_ECX]),
		   "=d" (out[REG_EDX])
		 : "a" (leaf), "c" (subleaf));
#else

    asm volatile("cpuid"
		 : "=a" (out[REG_EAX]),
		   "=b" (out[REG_EBX]),
		   "=c" (out[REG_ECX]),
		   "=d" (out[REG_EDX])
		 : "a" (leaf), "c" (subleaf));

#endif
}

/*
 * Checks if a particular flag is available on current machine.
 */
int
rte_cpu_get_flag_enabled(enum rte_cpu_flag_t feature)
{
	const struct feature_entry *feat;
	cpuid_registers_t regs;


	if (feature >= RTE_CPUFLAG_NUMFLAGS)
		/* Flag does not match anything in the feature tables */
		return -ENOENT;

	feat = &cpu_feature_table[feature];

	if (!feat->leaf)
		/* This entry in the table wasn't filled out! */
		return -EFAULT;

	rte_cpu_get_features(feat->leaf & 0xffff0000, 0, regs);
	if (((regs[REG_EAX] ^ feat->leaf) & 0xffff0000) ||
	      regs[REG_EAX] < feat->leaf)
		return 0;

	/* get the cpuid leaf containing the desired feature */
	rte_cpu_get_features(feat->leaf, feat->subleaf, regs);

	/* check if the feature is enabled */
	return (regs[feat->reg] >> feat->bit) & 1;
}

/**
 * Checks if the machine is adequate for running the binary. If it is not, the
 * program exits with status 1.
 * The function attribute forces this function to be called before main(). But
 * with ICC, the check is generated by the compiler.
 */
#ifndef __INTEL_COMPILER
void __attribute__ ((__constructor__))
#else
void
#endif
rte_cpu_check_supported(void)
{
	/* This is generated at compile-time by the build system */
	static const enum rte_cpu_flag_t compile_time_flags[] = {
			RTE_COMPILE_TIME_CPUFLAGS
	};
	unsigned i;
	int ret;

	for (i = 0; i < sizeof(compile_time_flags)/sizeof(compile_time_flags[0]); i++) {
		ret = rte_cpu_get_flag_enabled(compile_time_flags[i]);

		if (ret < 0) {
			fprintf(stderr,
				"ERROR: CPU feature flag lookup failed with error %d\n",
				ret);
			exit(1);
		}
		if (!ret) {
			fprintf(stderr,
			        "ERROR: This system does not support \"%s\".\n"
			        "Please check that RTE_MACHINE is set correctly.\n",
			        cpu_feature_table[compile_time_flags[i]].name);
			exit(1);
		}
	}
}
