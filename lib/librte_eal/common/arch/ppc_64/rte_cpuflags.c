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

#include "rte_cpuflags.h"

#include <elf.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>

/* Symbolic values for the entries in the auxiliary table */
#define AT_HWCAP  16
#define AT_HWCAP2 26

/* software based registers */
enum cpu_register_t {
	REG_NONE = 0,
	REG_HWCAP,
	REG_HWCAP2,
	REG_MAX
};

typedef uint32_t hwcap_registers_t[REG_MAX];

struct feature_entry {
	uint32_t reg;
	uint32_t bit;
#define CPU_FLAG_NAME_MAX_LEN 64
	char name[CPU_FLAG_NAME_MAX_LEN];
};

#define FEAT_DEF(name, reg, bit) \
	[RTE_CPUFLAG_##name] = {reg, bit, #name},

const struct feature_entry rte_cpu_feature_table[] = {
	FEAT_DEF(PPC_LE,                 REG_HWCAP,   0)
	FEAT_DEF(TRUE_LE,                REG_HWCAP,   1)
	FEAT_DEF(PSERIES_PERFMON_COMPAT, REG_HWCAP,   6)
	FEAT_DEF(VSX,                    REG_HWCAP,   7)
	FEAT_DEF(ARCH_2_06,              REG_HWCAP,   8)
	FEAT_DEF(POWER6_EXT,             REG_HWCAP,   9)
	FEAT_DEF(DFP,                    REG_HWCAP,  10)
	FEAT_DEF(PA6T,                   REG_HWCAP,  11)
	FEAT_DEF(ARCH_2_05,              REG_HWCAP,  12)
	FEAT_DEF(ICACHE_SNOOP,           REG_HWCAP,  13)
	FEAT_DEF(SMT,                    REG_HWCAP,  14)
	FEAT_DEF(BOOKE,                  REG_HWCAP,  15)
	FEAT_DEF(CELLBE,                 REG_HWCAP,  16)
	FEAT_DEF(POWER5_PLUS,            REG_HWCAP,  17)
	FEAT_DEF(POWER5,                 REG_HWCAP,  18)
	FEAT_DEF(POWER4,                 REG_HWCAP,  19)
	FEAT_DEF(NOTB,                   REG_HWCAP,  20)
	FEAT_DEF(EFP_DOUBLE,             REG_HWCAP,  21)
	FEAT_DEF(EFP_SINGLE,             REG_HWCAP,  22)
	FEAT_DEF(SPE,                    REG_HWCAP,  23)
	FEAT_DEF(UNIFIED_CACHE,          REG_HWCAP,  24)
	FEAT_DEF(4xxMAC,                 REG_HWCAP,  25)
	FEAT_DEF(MMU,                    REG_HWCAP,  26)
	FEAT_DEF(FPU,                    REG_HWCAP,  27)
	FEAT_DEF(ALTIVEC,                REG_HWCAP,  28)
	FEAT_DEF(PPC601,                 REG_HWCAP,  29)
	FEAT_DEF(PPC64,                  REG_HWCAP,  30)
	FEAT_DEF(PPC32,                  REG_HWCAP,  31)
	FEAT_DEF(TAR,                    REG_HWCAP2, 26)
	FEAT_DEF(LSEL,                   REG_HWCAP2, 27)
	FEAT_DEF(EBB,                    REG_HWCAP2, 28)
	FEAT_DEF(DSCR,                   REG_HWCAP2, 29)
	FEAT_DEF(HTM,                    REG_HWCAP2, 30)
	FEAT_DEF(ARCH_2_07,              REG_HWCAP2, 31)
};

/*
 * Read AUXV software register and get cpu features for Power
 */
static void
rte_cpu_get_features(hwcap_registers_t out)
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
	close(auxv_fd);
}

/*
 * Checks if a particular flag is available on current machine.
 */
int
rte_cpu_get_flag_enabled(enum rte_cpu_flag_t feature)
{
	const struct feature_entry *feat;
	hwcap_registers_t regs = {0};

	if (feature >= RTE_CPUFLAG_NUMFLAGS)
		return -ENOENT;

	feat = &rte_cpu_feature_table[feature];
	if (feat->reg == REG_NONE)
		return -EFAULT;

	rte_cpu_get_features(regs);
	return (regs[feat->reg] >> feat->bit) & 1;
}

const char *
rte_cpu_get_flag_name(enum rte_cpu_flag_t feature)
{
	if (feature >= RTE_CPUFLAG_NUMFLAGS)
		return NULL;
	return rte_cpu_feature_table[feature].name;
}
