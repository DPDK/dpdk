/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#include <eal_export.h>
#include "rte_cpuflags.h"

#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "rte_cpuid.h"
#include "rte_atomic.h"

#ifdef RTE_TOOLCHAIN_MSVC
#include <immintrin.h>
#endif

/*
 * XCR0 state components that the OS must enable before
 * the related instructions can execute without faulting.
 */
#define XSTATE_SSE      (UINT64_C(1) << 1)
#define XSTATE_YMM      (UINT64_C(1) << 2)
#define XSTATE_OPMASK   (UINT64_C(1) << 5)
#define XSTATE_ZMM_H256 (UINT64_C(1) << 6)
#define XSTATE_HI16_ZMM (UINT64_C(1) << 7)

#define XSTATE_AVX    (XSTATE_SSE | XSTATE_YMM)
#define XSTATE_AVX512 (XSTATE_AVX | XSTATE_OPMASK | XSTATE_ZMM_H256 | XSTATE_HI16_ZMM)

/**
 * Struct to hold a processor feature entry
 */
struct feature_entry {
	uint32_t leaf;				/**< cpuid leaf */
	uint32_t subleaf;			/**< cpuid subleaf */
	uint32_t reg;				/**< cpuid register */
	uint32_t bit;				/**< cpuid register bit */
#define CPU_FLAG_NAME_MAX_LEN 64
	char name[CPU_FLAG_NAME_MAX_LEN];       /**< String for printing */
	bool has_value;
	bool value;
	uint64_t xstate;			/**< XCR0 bits the OS must enable */
};

#define FEAT_DEF(name, leaf, subleaf, reg, bit) \
	[RTE_CPUFLAG_##name] = {leaf, subleaf, reg, bit, #name },

/*
 * A VEX or EVEX encoded feature also needs OS support for its register state.
 * Use FEAT_DEF_XSTATE for such a feature, with the XCR0 bits that it needs.
 */
#define FEAT_DEF_XSTATE(name, leaf, subleaf, reg, bit, xs) \
	[RTE_CPUFLAG_##name] = {leaf, subleaf, reg, bit, #name, .xstate = xs },

struct feature_entry rte_cpu_feature_table[] = {
	FEAT_DEF(SSE3, 0x00000001, 0, RTE_REG_ECX,  0)
	FEAT_DEF(PCLMULQDQ, 0x00000001, 0, RTE_REG_ECX,  1)
	FEAT_DEF(DTES64, 0x00000001, 0, RTE_REG_ECX,  2)
	FEAT_DEF(MONITOR, 0x00000001, 0, RTE_REG_ECX,  3)
	FEAT_DEF(DS_CPL, 0x00000001, 0, RTE_REG_ECX,  4)
	FEAT_DEF(VMX, 0x00000001, 0, RTE_REG_ECX,  5)
	FEAT_DEF(SMX, 0x00000001, 0, RTE_REG_ECX,  6)
	FEAT_DEF(EIST, 0x00000001, 0, RTE_REG_ECX,  7)
	FEAT_DEF(TM2, 0x00000001, 0, RTE_REG_ECX,  8)
	FEAT_DEF(SSSE3, 0x00000001, 0, RTE_REG_ECX,  9)
	FEAT_DEF(CNXT_ID, 0x00000001, 0, RTE_REG_ECX, 10)
	FEAT_DEF_XSTATE(FMA, 0x00000001, 0, RTE_REG_ECX, 12, XSTATE_AVX)
	FEAT_DEF(CMPXCHG16B, 0x00000001, 0, RTE_REG_ECX, 13)
	FEAT_DEF(XTPR, 0x00000001, 0, RTE_REG_ECX, 14)
	FEAT_DEF(PDCM, 0x00000001, 0, RTE_REG_ECX, 15)
	FEAT_DEF(PCID, 0x00000001, 0, RTE_REG_ECX, 17)
	FEAT_DEF(DCA, 0x00000001, 0, RTE_REG_ECX, 18)
	FEAT_DEF(SSE4_1, 0x00000001, 0, RTE_REG_ECX, 19)
	FEAT_DEF(SSE4_2, 0x00000001, 0, RTE_REG_ECX, 20)
	FEAT_DEF(X2APIC, 0x00000001, 0, RTE_REG_ECX, 21)
	FEAT_DEF(MOVBE, 0x00000001, 0, RTE_REG_ECX, 22)
	FEAT_DEF(POPCNT, 0x00000001, 0, RTE_REG_ECX, 23)
	FEAT_DEF(TSC_DEADLINE, 0x00000001, 0, RTE_REG_ECX, 24)
	FEAT_DEF(AES, 0x00000001, 0, RTE_REG_ECX, 25)
	FEAT_DEF(XSAVE, 0x00000001, 0, RTE_REG_ECX, 26)
	FEAT_DEF(OSXSAVE, 0x00000001, 0, RTE_REG_ECX, 27)
	FEAT_DEF_XSTATE(AVX, 0x00000001, 0, RTE_REG_ECX, 28, XSTATE_AVX)
	FEAT_DEF_XSTATE(F16C, 0x00000001, 0, RTE_REG_ECX, 29, XSTATE_AVX)
	FEAT_DEF(RDRAND, 0x00000001, 0, RTE_REG_ECX, 30)
	FEAT_DEF(HYPERVISOR, 0x00000001, 0, RTE_REG_ECX, 31)

	FEAT_DEF(FPU, 0x00000001, 0, RTE_REG_EDX,  0)
	FEAT_DEF(VME, 0x00000001, 0, RTE_REG_EDX,  1)
	FEAT_DEF(DE, 0x00000001, 0, RTE_REG_EDX,  2)
	FEAT_DEF(PSE, 0x00000001, 0, RTE_REG_EDX,  3)
	FEAT_DEF(TSC, 0x00000001, 0, RTE_REG_EDX,  4)
	FEAT_DEF(MSR, 0x00000001, 0, RTE_REG_EDX,  5)
	FEAT_DEF(PAE, 0x00000001, 0, RTE_REG_EDX,  6)
	FEAT_DEF(MCE, 0x00000001, 0, RTE_REG_EDX,  7)
	FEAT_DEF(CX8, 0x00000001, 0, RTE_REG_EDX,  8)
	FEAT_DEF(APIC, 0x00000001, 0, RTE_REG_EDX,  9)
	FEAT_DEF(SEP, 0x00000001, 0, RTE_REG_EDX, 11)
	FEAT_DEF(MTRR, 0x00000001, 0, RTE_REG_EDX, 12)
	FEAT_DEF(PGE, 0x00000001, 0, RTE_REG_EDX, 13)
	FEAT_DEF(MCA, 0x00000001, 0, RTE_REG_EDX, 14)
	FEAT_DEF(CMOV, 0x00000001, 0, RTE_REG_EDX, 15)
	FEAT_DEF(PAT, 0x00000001, 0, RTE_REG_EDX, 16)
	FEAT_DEF(PSE36, 0x00000001, 0, RTE_REG_EDX, 17)
	FEAT_DEF(PSN, 0x00000001, 0, RTE_REG_EDX, 18)
	FEAT_DEF(CLFSH, 0x00000001, 0, RTE_REG_EDX, 19)
	FEAT_DEF(DS, 0x00000001, 0, RTE_REG_EDX, 21)
	FEAT_DEF(ACPI, 0x00000001, 0, RTE_REG_EDX, 22)
	FEAT_DEF(MMX, 0x00000001, 0, RTE_REG_EDX, 23)
	FEAT_DEF(FXSR, 0x00000001, 0, RTE_REG_EDX, 24)
	FEAT_DEF(SSE, 0x00000001, 0, RTE_REG_EDX, 25)
	FEAT_DEF(SSE2, 0x00000001, 0, RTE_REG_EDX, 26)
	FEAT_DEF(SS, 0x00000001, 0, RTE_REG_EDX, 27)
	FEAT_DEF(HTT, 0x00000001, 0, RTE_REG_EDX, 28)
	FEAT_DEF(TM, 0x00000001, 0, RTE_REG_EDX, 29)
	FEAT_DEF(PBE, 0x00000001, 0, RTE_REG_EDX, 31)

	FEAT_DEF(DIGTEMP, 0x00000006, 0, RTE_REG_EAX,  0)
	FEAT_DEF(TRBOBST, 0x00000006, 0, RTE_REG_EAX,  1)
	FEAT_DEF(ARAT, 0x00000006, 0, RTE_REG_EAX,  2)
	FEAT_DEF(PLN, 0x00000006, 0, RTE_REG_EAX,  4)
	FEAT_DEF(ECMD, 0x00000006, 0, RTE_REG_EAX,  5)
	FEAT_DEF(PTM, 0x00000006, 0, RTE_REG_EAX,  6)

	FEAT_DEF(MPERF_APERF_MSR, 0x00000006, 0, RTE_REG_ECX,  0)
	FEAT_DEF(ACNT2, 0x00000006, 0, RTE_REG_ECX,  1)
	FEAT_DEF(ENERGY_EFF, 0x00000006, 0, RTE_REG_ECX,  3)

	FEAT_DEF(FSGSBASE, 0x00000007, 0, RTE_REG_EBX,  0)
	FEAT_DEF(BMI1, 0x00000007, 0, RTE_REG_EBX,  3)
	FEAT_DEF(HLE, 0x00000007, 0, RTE_REG_EBX,  4)
	FEAT_DEF_XSTATE(AVX2, 0x00000007, 0, RTE_REG_EBX,  5, XSTATE_AVX)
	FEAT_DEF(SMEP, 0x00000007, 0, RTE_REG_EBX,  7)
	FEAT_DEF(BMI2, 0x00000007, 0, RTE_REG_EBX,  8)
	FEAT_DEF(ERMS, 0x00000007, 0, RTE_REG_EBX,  9)
	FEAT_DEF(INVPCID, 0x00000007, 0, RTE_REG_EBX, 10)
	FEAT_DEF(RTM, 0x00000007, 0, RTE_REG_EBX, 11)
	FEAT_DEF_XSTATE(AVX512F, 0x00000007, 0, RTE_REG_EBX, 16, XSTATE_AVX512)
	FEAT_DEF_XSTATE(AVX512DQ, 0x00000007, 0, RTE_REG_EBX, 17, XSTATE_AVX512)
	FEAT_DEF(RDSEED, 0x00000007, 0, RTE_REG_EBX, 18)
	FEAT_DEF_XSTATE(AVX512IFMA, 0x00000007, 0, RTE_REG_EBX, 21, XSTATE_AVX512)
	FEAT_DEF_XSTATE(AVX512CD, 0x00000007, 0, RTE_REG_EBX, 28, XSTATE_AVX512)
	FEAT_DEF_XSTATE(AVX512BW, 0x00000007, 0, RTE_REG_EBX, 30, XSTATE_AVX512)
	FEAT_DEF_XSTATE(AVX512VL, 0x00000007, 0, RTE_REG_EBX, 31, XSTATE_AVX512)

	FEAT_DEF_XSTATE(AVX512VBMI, 0x00000007, 0, RTE_REG_ECX,  1, XSTATE_AVX512)
	FEAT_DEF(WAITPKG, 0x00000007, 0, RTE_REG_ECX,  5)
	FEAT_DEF_XSTATE(AVX512VBMI2, 0x00000007, 0, RTE_REG_ECX,  6, XSTATE_AVX512)
	FEAT_DEF(GFNI, 0x00000007, 0, RTE_REG_ECX,  8)
	FEAT_DEF_XSTATE(VAES, 0x00000007, 0, RTE_REG_ECX,  9, XSTATE_AVX)
	FEAT_DEF_XSTATE(VPCLMULQDQ, 0x00000007, 0, RTE_REG_ECX, 10, XSTATE_AVX)
	FEAT_DEF_XSTATE(AVX512VNNI, 0x00000007, 0, RTE_REG_ECX, 11, XSTATE_AVX512)
	FEAT_DEF_XSTATE(AVX512BITALG, 0x00000007, 0, RTE_REG_ECX, 12, XSTATE_AVX512)
	FEAT_DEF_XSTATE(AVX512VPOPCNTDQ, 0x00000007, 0, RTE_REG_ECX, 14, XSTATE_AVX512)
	FEAT_DEF(CLDEMOTE, 0x00000007, 0, RTE_REG_ECX, 25)
	FEAT_DEF(MOVDIRI, 0x00000007, 0, RTE_REG_ECX, 27)
	FEAT_DEF(MOVDIR64B, 0x00000007, 0, RTE_REG_ECX, 28)

	FEAT_DEF_XSTATE(AVX512VP2INTERSECT, 0x00000007, 0, RTE_REG_EDX,  8, XSTATE_AVX512)

	FEAT_DEF(LAHF_SAHF, 0x80000001, 0, RTE_REG_ECX,  0)
	FEAT_DEF(LZCNT, 0x80000001, 0, RTE_REG_ECX,  5)
	FEAT_DEF(MONITORX, 0x80000001, 0, RTE_REG_ECX,  29)

	FEAT_DEF(SYSCALL, 0x80000001, 0, RTE_REG_EDX, 11)
	FEAT_DEF(XD, 0x80000001, 0, RTE_REG_EDX, 20)
	FEAT_DEF(1GB_PG, 0x80000001, 0, RTE_REG_EDX, 26)
	FEAT_DEF(RDTSCP, 0x80000001, 0, RTE_REG_EDX, 27)
	FEAT_DEF(EM64T, 0x80000001, 0, RTE_REG_EDX, 29)

	FEAT_DEF(INVTSC, 0x80000007, 0, RTE_REG_EDX,  8)
};

static uint64_t
xcr0_read(void)
{
#ifdef RTE_TOOLCHAIN_MSVC
	return _xgetbv(0);
#else
	uint32_t eax, edx;

	/* use the raw mnemonic: _xgetbv() would need -mxsave */
	asm volatile("xgetbv" : "=a" (eax), "=d" (edx) : "c" (0));
	return ((uint64_t)edx << 32) | eax;
#endif
}

/*
 * CPUID reports what the CPU implements, not what the OS enables.
 * Check that the OS saves the register state that the feature uses.
 */
static bool
xstate_enabled(uint64_t xstate)
{
	/*
	 * XGETBV faults unless the OS has set CR4.OSXSAVE.
	 * The OSXSAVE entry must not have an xstate mask,
	 * else this call recurses without end.
	 */
	if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_OSXSAVE) != 1)
		return false;

	return (xcr0_read() & xstate) == xstate;
}

RTE_EXPORT_SYMBOL(rte_cpu_get_flag_enabled)
int
rte_cpu_get_flag_enabled(enum rte_cpu_flag_t feature)
{
	struct feature_entry *feat;
	cpuid_registers_t regs;
	unsigned int maxleaf;
	bool value;

	if ((unsigned int)feature >= RTE_DIM(rte_cpu_feature_table))
		/* Flag does not match anything in the feature tables */
		return -ENOENT;

	feat = &rte_cpu_feature_table[feature];
	if (feat->has_value)
		return feat->value;

	if (!feat->leaf)
		/* This entry in the table wasn't filled out! */
		return -EFAULT;

	maxleaf = __get_cpuid_max(feat->leaf & 0x80000000, NULL);

	if (maxleaf < feat->leaf) {
		feat->value = 0;
		goto out;
	}

#ifdef RTE_TOOLCHAIN_MSVC
	__cpuidex(regs, feat->leaf, feat->subleaf);
#else
	__cpuid_count(feat->leaf, feat->subleaf,
			 regs[RTE_REG_EAX], regs[RTE_REG_EBX],
			 regs[RTE_REG_ECX], regs[RTE_REG_EDX]);
#endif

	/* check if the feature is enabled */
	value = (regs[feat->reg] >> feat->bit) & 1;

	/* check if the OS enabled the register state for the feature */
	if (value && feat->xstate != 0)
		value = xstate_enabled(feat->xstate);

	feat->value = value;
out:
	rte_compiler_barrier();
	feat->has_value = true;
	return feat->value;
}

RTE_EXPORT_SYMBOL(rte_cpu_get_flag_name)
const char *
rte_cpu_get_flag_name(enum rte_cpu_flag_t feature)
{
	if ((unsigned int)feature >= RTE_DIM(rte_cpu_feature_table))
		return NULL;
	return rte_cpu_feature_table[feature].name;
}

RTE_EXPORT_SYMBOL(rte_cpu_get_intrinsics_support)
void
rte_cpu_get_intrinsics_support(struct rte_cpu_intrinsics *intrinsics)
{
	memset(intrinsics, 0, sizeof(*intrinsics));

	if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_WAITPKG)) {
		intrinsics->power_monitor = 1;
		intrinsics->power_pause = 1;
		if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_RTM))
			intrinsics->power_monitor_multi = 1;
	} else if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_MONITORX)) {
		intrinsics->power_monitor = 1;
	}
}
