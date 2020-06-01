/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019 Mellanox Technologies, Ltd
 */

#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include <rte_errno.h>

#include "mlx5_common.h"
#include "mlx5_common_utils.h"

int mlx5_common_logtype;

#ifdef MLX5_GLUE
const struct mlx5_glue *mlx5_glue;
#endif

uint8_t haswell_broadwell_cpu;

static int
mlx5_class_check_handler(__rte_unused const char *key, const char *value,
			 void *opaque)
{
	enum mlx5_class *ret = opaque;

	if (strcmp(value, "vdpa") == 0) {
		*ret = MLX5_CLASS_VDPA;
	} else if (strcmp(value, "net") == 0) {
		*ret = MLX5_CLASS_NET;
	} else {
		DRV_LOG(ERR, "Invalid mlx5 class %s. Maybe typo in device"
			" class argument setting?", value);
		*ret = MLX5_CLASS_INVALID;
	}
	return 0;
}

enum mlx5_class
mlx5_class_get(struct rte_devargs *devargs)
{
	struct rte_kvargs *kvlist;
	const char *key = MLX5_CLASS_ARG_NAME;
	enum mlx5_class ret = MLX5_CLASS_NET;

	if (devargs == NULL)
		return ret;
	kvlist = rte_kvargs_parse(devargs->args, NULL);
	if (kvlist == NULL)
		return ret;
	if (rte_kvargs_count(kvlist, key))
		rte_kvargs_process(kvlist, key, mlx5_class_check_handler, &ret);
	rte_kvargs_free(kvlist);
	return ret;
}


/* In case this is an x86_64 intel processor to check if
 * we should use relaxed ordering.
 */
#ifdef RTE_ARCH_X86_64
/**
 * This function returns processor identification and feature information
 * into the registers.
 *
 * @param eax, ebx, ecx, edx
 *		Pointers to the registers that will hold cpu information.
 * @param level
 *		The main category of information returned.
 */
static inline void mlx5_cpu_id(unsigned int level,
				unsigned int *eax, unsigned int *ebx,
				unsigned int *ecx, unsigned int *edx)
{
	__asm__("cpuid\n\t"
		: "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
		: "0" (level));
}
#endif

RTE_INIT_PRIO(mlx5_log_init, LOG)
{
	mlx5_common_logtype = rte_log_register("pmd.common.mlx5");
	if (mlx5_common_logtype >= 0)
		rte_log_set_level(mlx5_common_logtype, RTE_LOG_NOTICE);
}

/**
 * Initialization routine for run-time dependency on glue library.
 */
RTE_INIT_PRIO(mlx5_glue_init, CLASS)
{
	mlx5_glue_constructor();
}

/**
 * This function is responsible of initializing the variable
 *  haswell_broadwell_cpu by checking if the cpu is intel
 *  and reading the data returned from mlx5_cpu_id().
 *  since haswell and broadwell cpus don't have improved performance
 *  when using relaxed ordering we want to check the cpu type before
 *  before deciding whether to enable RO or not.
 *  if the cpu is haswell or broadwell the variable will be set to 1
 *  otherwise it will be 0.
 */
RTE_INIT_PRIO(mlx5_is_haswell_broadwell_cpu, LOG)
{
#ifdef RTE_ARCH_X86_64
	unsigned int broadwell_models[4] = {0x3d, 0x47, 0x4F, 0x56};
	unsigned int haswell_models[4] = {0x3c, 0x3f, 0x45, 0x46};
	unsigned int i, model, family, brand_id, vendor;
	unsigned int signature_intel_ebx = 0x756e6547;
	unsigned int extended_model;
	unsigned int eax = 0;
	unsigned int ebx = 0;
	unsigned int ecx = 0;
	unsigned int edx = 0;
	int max_level;

	mlx5_cpu_id(0, &eax, &ebx, &ecx, &edx);
	vendor = ebx;
	max_level = eax;
	if (max_level < 1) {
		haswell_broadwell_cpu = 0;
		return;
	}
	mlx5_cpu_id(1, &eax, &ebx, &ecx, &edx);
	model = (eax >> 4) & 0x0f;
	family = (eax >> 8) & 0x0f;
	brand_id = ebx & 0xff;
	extended_model = (eax >> 12) & 0xf0;
	/* Check if the processor is Haswell or Broadwell */
	if (vendor == signature_intel_ebx) {
		if (family == 0x06)
			model += extended_model;
		if (brand_id == 0 && family == 0x6) {
			for (i = 0; i < RTE_DIM(broadwell_models); i++)
				if (model == broadwell_models[i]) {
					haswell_broadwell_cpu = 1;
					return;
				}
			for (i = 0; i < RTE_DIM(haswell_models); i++)
				if (model == haswell_models[i]) {
					haswell_broadwell_cpu = 1;
					return;
				}
		}
	}
#endif
	haswell_broadwell_cpu = 0;
}
