/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 StarFive
 * Copyright(c) 2022 SiFive
 * Copyright(c) 2022 Semihalf
 */

#include "rte_cpuflags.h"

#include <elf.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#ifndef AT_HWCAP
#define AT_HWCAP 16
#endif

#ifndef AT_HWCAP2
#define AT_HWCAP2 26
#endif

#ifndef AT_PLATFORM
#define AT_PLATFORM 15
#endif

enum cpu_register_t {
	REG_NONE = 0,
	REG_HWCAP,
	REG_HWCAP2,
	REG_PLATFORM,
	REG_MAX
};

typedef uint32_t hwcap_registers_t[REG_MAX];

/**
 * Struct to hold a processor feature entry
 */
struct feature_entry {
	uint32_t reg;
	uint32_t bit;
#define CPU_FLAG_NAME_MAX_LEN 64
	char name[CPU_FLAG_NAME_MAX_LEN];
};

#define FEAT_DEF(name, reg, bit) \
	[RTE_CPUFLAG_##name] = {reg, bit, #name},

typedef Elf64_auxv_t _Elfx_auxv_t;

const struct feature_entry rte_cpu_feature_table[] = {
	FEAT_DEF(RISCV_ISA_A, REG_HWCAP,    0)
	FEAT_DEF(RISCV_ISA_B, REG_HWCAP,    1)
	FEAT_DEF(RISCV_ISA_C, REG_HWCAP,    2)
	FEAT_DEF(RISCV_ISA_D, REG_HWCAP,    3)
	FEAT_DEF(RISCV_ISA_E, REG_HWCAP,    4)
	FEAT_DEF(RISCV_ISA_F, REG_HWCAP,    5)
	FEAT_DEF(RISCV_ISA_G, REG_HWCAP,    6)
	FEAT_DEF(RISCV_ISA_H, REG_HWCAP,    7)
	FEAT_DEF(RISCV_ISA_I, REG_HWCAP,    8)
	FEAT_DEF(RISCV_ISA_J, REG_HWCAP,    9)
	FEAT_DEF(RISCV_ISA_K, REG_HWCAP,   10)
	FEAT_DEF(RISCV_ISA_L, REG_HWCAP,   11)
	FEAT_DEF(RISCV_ISA_M, REG_HWCAP,   12)
	FEAT_DEF(RISCV_ISA_N, REG_HWCAP,   13)
	FEAT_DEF(RISCV_ISA_O, REG_HWCAP,   14)
	FEAT_DEF(RISCV_ISA_P, REG_HWCAP,   15)
	FEAT_DEF(RISCV_ISA_Q, REG_HWCAP,   16)
	FEAT_DEF(RISCV_ISA_R, REG_HWCAP,   17)
	FEAT_DEF(RISCV_ISA_S, REG_HWCAP,   18)
	FEAT_DEF(RISCV_ISA_T, REG_HWCAP,   19)
	FEAT_DEF(RISCV_ISA_U, REG_HWCAP,   20)
	FEAT_DEF(RISCV_ISA_V, REG_HWCAP,   21)
	FEAT_DEF(RISCV_ISA_W, REG_HWCAP,   22)
	FEAT_DEF(RISCV_ISA_X, REG_HWCAP,   23)
	FEAT_DEF(RISCV_ISA_Y, REG_HWCAP,   24)
	FEAT_DEF(RISCV_ISA_Z, REG_HWCAP,   25)
};
/*
 * Read AUXV software register and get cpu features for ARM
 */
static void
rte_cpu_get_features(hwcap_registers_t out)
{
	out[REG_HWCAP] = rte_cpu_getauxval(AT_HWCAP);
	out[REG_HWCAP2] = rte_cpu_getauxval(AT_HWCAP2);
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
