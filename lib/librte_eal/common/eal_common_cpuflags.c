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

/**
 * Parameters for CPUID instruction
 */
struct cpuid_parameters_t {
	uint32_t eax;
	uint32_t ebx;
	uint32_t ecx;
	uint32_t edx;
	enum cpu_register_t return_register;
};

#define CPU_FLAG_NAME_MAX_LEN 64

/**
 * Struct to hold a processor feature entry
 */
struct feature_entry {
	enum rte_cpu_flag_t feature;            /**< feature name */
	char name[CPU_FLAG_NAME_MAX_LEN];       /**< String for printing */
	struct cpuid_parameters_t params;       /**< cpuid parameters */
	uint32_t feature_mask;                  /**< bitmask for feature */
};

#define FEAT_DEF(f) RTE_CPUFLAG_##f, #f

/**
 * An array that holds feature entries
 */
static const struct feature_entry cpu_feature_table[] = {
	{FEAT_DEF(SSE3),              {0x1, 0, 0, 0, REG_ECX}, 0x00000001},
	{FEAT_DEF(PCLMULQDQ),         {0x1, 0, 0, 0, REG_ECX}, 0x00000002},
	{FEAT_DEF(DTES64),            {0x1, 0, 0, 0, REG_ECX}, 0x00000004},
	{FEAT_DEF(MONITOR),           {0x1, 0, 0, 0, REG_ECX}, 0x00000008},
	{FEAT_DEF(DS_CPL),            {0x1, 0, 0, 0, REG_ECX}, 0x00000010},
	{FEAT_DEF(VMX),               {0x1, 0, 0, 0, REG_ECX}, 0x00000020},
	{FEAT_DEF(SMX),               {0x1, 0, 0, 0, REG_ECX}, 0x00000040},
	{FEAT_DEF(EIST),              {0x1, 0, 0, 0, REG_ECX}, 0x00000080},
	{FEAT_DEF(TM2),               {0x1, 0, 0, 0, REG_ECX}, 0x00000100},
	{FEAT_DEF(SSSE3),             {0x1, 0, 0, 0, REG_ECX}, 0x00000200},
	{FEAT_DEF(CNXT_ID),           {0x1, 0, 0, 0, REG_ECX}, 0x00000400},
	{FEAT_DEF(FMA),               {0x1, 0, 0, 0, REG_ECX}, 0x00001000},
	{FEAT_DEF(CMPXCHG16B),        {0x1, 0, 0, 0, REG_ECX}, 0x00002000},
	{FEAT_DEF(XTPR),              {0x1, 0, 0, 0, REG_ECX}, 0x00004000},
	{FEAT_DEF(PDCM),              {0x1, 0, 0, 0, REG_ECX}, 0x00008000},
	{FEAT_DEF(PCID),              {0x1, 0, 0, 0, REG_ECX}, 0x00020000},
	{FEAT_DEF(DCA),               {0x1, 0, 0, 0, REG_ECX}, 0x00040000},
	{FEAT_DEF(SSE4_1),            {0x1, 0, 0, 0, REG_ECX}, 0x00080000},
	{FEAT_DEF(SSE4_2),            {0x1, 0, 0, 0, REG_ECX}, 0x00100000},
	{FEAT_DEF(X2APIC),            {0x1, 0, 0, 0, REG_ECX}, 0x00200000},
	{FEAT_DEF(MOVBE),             {0x1, 0, 0, 0, REG_ECX}, 0x00400000},
	{FEAT_DEF(POPCNT),            {0x1, 0, 0, 0, REG_ECX}, 0x00800000},
	{FEAT_DEF(TSC_DEADLINE),      {0x1, 0, 0, 0, REG_ECX}, 0x01000000},
	{FEAT_DEF(AES),               {0x1, 0, 0, 0, REG_ECX}, 0x02000000},
	{FEAT_DEF(XSAVE),             {0x1, 0, 0, 0, REG_ECX}, 0x04000000},
	{FEAT_DEF(OSXSAVE),           {0x1, 0, 0, 0, REG_ECX}, 0x08000000},
	{FEAT_DEF(AVX),               {0x1, 0, 0, 0, REG_ECX}, 0x10000000},
	{FEAT_DEF(F16C),              {0x1, 0, 0, 0, REG_ECX}, 0x20000000},
	{FEAT_DEF(RDRAND),            {0x1, 0, 0, 0, REG_ECX}, 0x40000000},

	{FEAT_DEF(FPU),               {0x1, 0, 0, 0, REG_EDX}, 0x00000001},
	{FEAT_DEF(VME),               {0x1, 0, 0, 0, REG_EDX}, 0x00000002},
	{FEAT_DEF(DE),                {0x1, 0, 0, 0, REG_EDX}, 0x00000004},
	{FEAT_DEF(PSE),               {0x1, 0, 0, 0, REG_EDX}, 0x00000008},
	{FEAT_DEF(TSC),               {0x1, 0, 0, 0, REG_EDX}, 0x00000010},
	{FEAT_DEF(MSR),               {0x1, 0, 0, 0, REG_EDX}, 0x00000020},
	{FEAT_DEF(PAE),               {0x1, 0, 0, 0, REG_EDX}, 0x00000040},
	{FEAT_DEF(MCE),               {0x1, 0, 0, 0, REG_EDX}, 0x00000080},
	{FEAT_DEF(CX8),               {0x1, 0, 0, 0, REG_EDX}, 0x00000100},
	{FEAT_DEF(APIC),              {0x1, 0, 0, 0, REG_EDX}, 0x00000200},
	{FEAT_DEF(SEP),               {0x1, 0, 0, 0, REG_EDX}, 0x00000800},
	{FEAT_DEF(MTRR),              {0x1, 0, 0, 0, REG_EDX}, 0x00001000},
	{FEAT_DEF(PGE),               {0x1, 0, 0, 0, REG_EDX}, 0x00002000},
	{FEAT_DEF(MCA),               {0x1, 0, 0, 0, REG_EDX}, 0x00004000},
	{FEAT_DEF(CMOV),              {0x1, 0, 0, 0, REG_EDX}, 0x00008000},
	{FEAT_DEF(PAT),               {0x1, 0, 0, 0, REG_EDX}, 0x00010000},
	{FEAT_DEF(PSE36),             {0x1, 0, 0, 0, REG_EDX}, 0x00020000},
	{FEAT_DEF(PSN),               {0x1, 0, 0, 0, REG_EDX}, 0x00040000},
	{FEAT_DEF(CLFSH),             {0x1, 0, 0, 0, REG_EDX}, 0x00080000},
	{FEAT_DEF(DS),                {0x1, 0, 0, 0, REG_EDX}, 0x00200000},
	{FEAT_DEF(ACPI),              {0x1, 0, 0, 0, REG_EDX}, 0x00400000},
	{FEAT_DEF(MMX),               {0x1, 0, 0, 0, REG_EDX}, 0x00800000},
	{FEAT_DEF(FXSR),              {0x1, 0, 0, 0, REG_EDX}, 0x01000000},
	{FEAT_DEF(SSE),               {0x1, 0, 0, 0, REG_EDX}, 0x02000000},
	{FEAT_DEF(SSE2),              {0x1, 0, 0, 0, REG_EDX}, 0x04000000},
	{FEAT_DEF(SS),                {0x1, 0, 0, 0, REG_EDX}, 0x08000000},
	{FEAT_DEF(HTT),               {0x1, 0, 0, 0, REG_EDX}, 0x10000000},
	{FEAT_DEF(TM),                {0x1, 0, 0, 0, REG_EDX}, 0x20000000},
	{FEAT_DEF(PBE),               {0x1, 0, 0, 0, REG_EDX}, 0x80000000},

	{FEAT_DEF(DIGTEMP),           {0x6, 0, 0, 0, REG_EAX}, 0x00000001},
	{FEAT_DEF(TRBOBST),           {0x6, 0, 0, 0, REG_EAX}, 0x00000002},
	{FEAT_DEF(ARAT),              {0x6, 0, 0, 0, REG_EAX}, 0x00000004},
	{FEAT_DEF(PLN),               {0x6, 0, 0, 0, REG_EAX}, 0x00000010},
	{FEAT_DEF(ECMD),              {0x6, 0, 0, 0, REG_EAX}, 0x00000020},
	{FEAT_DEF(PTM),               {0x6, 0, 0, 0, REG_EAX}, 0x00000040},

	{FEAT_DEF(MPERF_APERF_MSR),   {0x6, 0, 0, 0, REG_ECX}, 0x00000001},
	{FEAT_DEF(ACNT2),             {0x6, 0, 0, 0, REG_ECX}, 0x00000002},
	{FEAT_DEF(ENERGY_EFF),        {0x6, 0, 0, 0, REG_ECX}, 0x00000008},

	{FEAT_DEF(FSGSBASE),          {0x7, 0, 0, 0, REG_EBX}, 0x00000001},
	{FEAT_DEF(BMI1),              {0x7, 0, 0, 0, REG_EBX}, 0x00000004},
	{FEAT_DEF(HLE),               {0x7, 0, 0, 0, REG_EBX}, 0x00000010},
	{FEAT_DEF(AVX2),              {0x7, 0, 0, 0, REG_EBX}, 0x00000020},
	{FEAT_DEF(SMEP),              {0x7, 0, 0, 0, REG_EBX}, 0x00000040},
	{FEAT_DEF(BMI2),              {0x7, 0, 0, 0, REG_EBX}, 0x00000080},
	{FEAT_DEF(ERMS),              {0x7, 0, 0, 0, REG_EBX}, 0x00000100},
	{FEAT_DEF(INVPCID),           {0x7, 0, 0, 0, REG_EBX}, 0x00000400},
	{FEAT_DEF(RTM),               {0x7, 0, 0, 0, REG_EBX}, 0x00000800},

	{FEAT_DEF(LAHF_SAHF),  {0x80000001, 0, 0, 0, REG_ECX}, 0x00000001},
	{FEAT_DEF(LZCNT),      {0x80000001, 0, 0, 0, REG_ECX}, 0x00000010},

	{FEAT_DEF(SYSCALL),    {0x80000001, 0, 0, 0, REG_EDX}, 0x00000800},
	{FEAT_DEF(XD),         {0x80000001, 0, 0, 0, REG_EDX}, 0x00100000},
	{FEAT_DEF(1GB_PG),     {0x80000001, 0, 0, 0, REG_EDX}, 0x04000000},
	{FEAT_DEF(RDTSCP),     {0x80000001, 0, 0, 0, REG_EDX}, 0x08000000},
	{FEAT_DEF(EM64T),      {0x80000001, 0, 0, 0, REG_EDX}, 0x20000000},

	{FEAT_DEF(INVTSC),     {0x80000007, 0, 0, 0, REG_EDX}, 0x00000100},
};

/*
 * Execute CPUID instruction and get contents of a specific register
 *
 * This function, when compiled with GCC, will generate architecture-neutral
 * code, as per GCC manual.
 */
static inline int
rte_cpu_get_features(struct cpuid_parameters_t params)
{
	int eax, ebx, ecx, edx;            /* registers */

#ifndef __PIC__
   asm volatile ("cpuid"
                 /* output */
                 : "=a" (eax),
                   "=b" (ebx),
                   "=c" (ecx),
                   "=d" (edx)
                 /* input */
                 : "a" (params.eax),
                   "b" (params.ebx),
                   "c" (params.ecx),
                   "d" (params.edx));
#else
	asm volatile ( 
            "mov %%ebx, %%edi\n"
            "cpuid\n"
            "xchgl %%ebx, %%edi;\n"
            : "=a" (eax),
              "=D" (ebx),
              "=c" (ecx),
              "=d" (edx)
            /* input */
            : "a" (params.eax),
              "D" (params.ebx),
              "c" (params.ecx),
              "d" (params.edx));
#endif

	switch (params.return_register) {
	case REG_EAX:
		return eax;
	case REG_EBX:
		return ebx;
	case REG_ECX:
		return ecx;
	case REG_EDX:
		return edx;
	default:
		return 0;
	}
}

/*
 * Checks if a particular flag is available on current machine.
 */
int
rte_cpu_get_flag_enabled(enum rte_cpu_flag_t feature)
{
	int value;

	if (feature >= RTE_CPUFLAG_NUMFLAGS)
		/* Flag does not match anything in the feature tables */
		return -ENOENT;

	/* get value of the register containing the desired feature */
	value = rte_cpu_get_features(cpu_feature_table[feature].params);

	/* check if the feature is enabled */
	return (cpu_feature_table[feature].feature_mask & value) > 0;
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

	for (i = 0; i < sizeof(compile_time_flags)/sizeof(compile_time_flags[0]); i++)
		if (rte_cpu_get_flag_enabled(compile_time_flags[i]) < 1) {
			fprintf(stderr,
			        "ERROR: This system does not support \"%s\".\n"
			        "Please check that RTE_MACHINE is set correctly.\n",
			        cpu_feature_table[compile_time_flags[i]].name);
			exit(1);
		}
}
