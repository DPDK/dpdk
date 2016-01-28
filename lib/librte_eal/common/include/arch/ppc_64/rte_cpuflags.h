/*
 *   BSD LICENSE
 *
 *   Copyright (C) IBM Corporation 2014.
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
 *     * Neither the name of IBM Corporation nor the names of its
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

#ifndef _RTE_CPUFLAGS_PPC_64_H_
#define _RTE_CPUFLAGS_PPC_64_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <elf.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>

#include "generic/rte_cpuflags.h"

extern const struct feature_entry rte_cpu_feature_table[];

/* Symbolic values for the entries in the auxiliary table */
#define AT_HWCAP  16
#define AT_HWCAP2 26

/* software based registers */
enum cpu_register_t {
	REG_HWCAP = 0,
	REG_HWCAP2,
};

/**
 * Enumeration of all CPU features supported
 */
enum rte_cpu_flag_t {
	RTE_CPUFLAG_PPC_LE = 0,
	RTE_CPUFLAG_TRUE_LE,
	RTE_CPUFLAG_PSERIES_PERFMON_COMPAT,
	RTE_CPUFLAG_VSX,
	RTE_CPUFLAG_ARCH_2_06,
	RTE_CPUFLAG_POWER6_EXT,
	RTE_CPUFLAG_DFP,
	RTE_CPUFLAG_PA6T,
	RTE_CPUFLAG_ARCH_2_05,
	RTE_CPUFLAG_ICACHE_SNOOP,
	RTE_CPUFLAG_SMT,
	RTE_CPUFLAG_BOOKE,
	RTE_CPUFLAG_CELLBE,
	RTE_CPUFLAG_POWER5_PLUS,
	RTE_CPUFLAG_POWER5,
	RTE_CPUFLAG_POWER4,
	RTE_CPUFLAG_NOTB,
	RTE_CPUFLAG_EFP_DOUBLE,
	RTE_CPUFLAG_EFP_SINGLE,
	RTE_CPUFLAG_SPE,
	RTE_CPUFLAG_UNIFIED_CACHE,
	RTE_CPUFLAG_4xxMAC,
	RTE_CPUFLAG_MMU,
	RTE_CPUFLAG_FPU,
	RTE_CPUFLAG_ALTIVEC,
	RTE_CPUFLAG_PPC601,
	RTE_CPUFLAG_PPC64,
	RTE_CPUFLAG_PPC32,
	RTE_CPUFLAG_TAR,
	RTE_CPUFLAG_LSEL,
	RTE_CPUFLAG_EBB,
	RTE_CPUFLAG_DSCR,
	RTE_CPUFLAG_HTM,
	RTE_CPUFLAG_ARCH_2_07,
	/* The last item */
	RTE_CPUFLAG_NUMFLAGS,/**< This should always be the last! */
};

/*
 * Read AUXV software register and get cpu features for Power
 */
static inline void
rte_cpu_get_features(__attribute__((unused)) uint32_t leaf,
	__attribute__((unused)) uint32_t subleaf, cpuid_registers_t out)
{
	int auxv_fd;
	Elf64_auxv_t auxv;

	auxv_fd = open("/proc/self/auxv", O_RDONLY);
	assert(auxv_fd);
	while (read(auxv_fd, &auxv,
		sizeof(Elf64_auxv_t)) == sizeof(Elf64_auxv_t)) {
		if (auxv.a_type == AT_HWCAP)
			out[REG_HWCAP] = auxv.a_un.a_val;
		else if (auxv.a_type == AT_HWCAP2)
			out[REG_HWCAP2] = auxv.a_un.a_val;
	}
}

/*
 * Checks if a particular flag is available on current machine.
 */
static inline int
rte_cpu_get_flag_enabled(enum rte_cpu_flag_t feature)
{
	const struct feature_entry *feat;
	cpuid_registers_t regs = {0};

	if (feature >= RTE_CPUFLAG_NUMFLAGS)
		/* Flag does not match anything in the feature tables */
		return -ENOENT;

	feat = &rte_cpu_feature_table[feature];

	if (!feat->leaf)
		/* This entry in the table wasn't filled out! */
		return -EFAULT;

	/* get the cpuid leaf containing the desired feature */
	rte_cpu_get_features(feat->leaf, feat->subleaf, regs);

	/* check if the feature is enabled */
	return (regs[feat->reg] >> feat->bit) & 1;
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_CPUFLAGS_PPC_64_H_ */
