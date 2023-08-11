/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Loongson Technology Corporation Limited
 */

#include "rte_cpuflags.h"

#include <elf.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

/* Symbolic values for the entries in the auxiliary table */
#define AT_HWCAP  16

/* software based registers */
enum cpu_register_t {
	REG_NONE = 0,
	REG_HWCAP,
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
	FEAT_DEF(CPUCFG,             REG_HWCAP,   0)
	FEAT_DEF(LAM,                REG_HWCAP,   1)
	FEAT_DEF(UAL,                REG_HWCAP,   2)
	FEAT_DEF(FPU,                REG_HWCAP,   3)
	FEAT_DEF(LSX,                REG_HWCAP,   4)
	FEAT_DEF(LASX,               REG_HWCAP,   5)
	FEAT_DEF(CRC32,              REG_HWCAP,   6)
	FEAT_DEF(COMPLEX,            REG_HWCAP,   7)
	FEAT_DEF(CRYPTO,             REG_HWCAP,   8)
	FEAT_DEF(LVZ,                REG_HWCAP,   9)
	FEAT_DEF(LBT_X86,            REG_HWCAP,  10)
	FEAT_DEF(LBT_ARM,            REG_HWCAP,  11)
	FEAT_DEF(LBT_MIPS,           REG_HWCAP,  12)
};

/*
 * Read AUXV software register and get cpu features for LoongArch
 */
static void
rte_cpu_get_features(hwcap_registers_t out)
{
	out[REG_HWCAP] = rte_cpu_getauxval(AT_HWCAP);
}

/*
 * Checks if a particular flag is available on current machine.
 */
int
rte_cpu_get_flag_enabled(enum rte_cpu_flag_t feature)
{
	const struct feature_entry *feat;
	hwcap_registers_t regs = {0};

	if ((unsigned int)feature >= RTE_DIM(rte_cpu_feature_table))
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
	if ((unsigned int)feature >= RTE_DIM(rte_cpu_feature_table))
		return NULL;
	return rte_cpu_feature_table[feature].name;
}

void
rte_cpu_get_intrinsics_support(struct rte_cpu_intrinsics *intrinsics)
{
	memset(intrinsics, 0, sizeof(*intrinsics));
}
