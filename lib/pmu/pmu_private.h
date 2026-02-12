/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Marvell
 */

#ifndef PMU_PRIVATE_H
#define PMU_PRIVATE_H

#include <rte_log.h>

extern int rte_pmu_logtype;
#define RTE_LOGTYPE_PMU rte_pmu_logtype

#define PMU_LOG(level, ...) \
	RTE_LOG_LINE(level, PMU, ## __VA_ARGS__)

/**
 * Structure describing architecture specific PMU operations.
 */
struct pmu_arch_ops {
	int (*init)(void);
	void (*fini)(void);
	void (*fixup_config)(uint64_t config[3]);
};

extern const struct pmu_arch_ops *arch_ops;

#define PMU_SET_ARCH_OPS(ops) \
	RTE_INIT(rte_pmu_set_arch_ops) \
	{ \
		arch_ops = &(ops); \
	}

/**
 * Architecture-specific PMU init callback.
 *
 * @return
 *   0 in case of success, negative value otherwise.
 */
static inline int
pmu_arch_init(void)
{
	if (arch_ops != NULL && arch_ops->init != NULL)
		return arch_ops->init();

	return 0;
}

/**
 * Architecture-specific PMU cleanup callback.
 */
static inline void
pmu_arch_fini(void)
{
	if (arch_ops != NULL && arch_ops->fini != NULL)
		arch_ops->fini();
}

/**
 * Apply architecture-specific settings to config before passing it to syscall.
 *
 * @param config
 *   Architecture-specific event configuration.
 *   Consult kernel sources for available options.
 */
static inline void
pmu_arch_fixup_config(uint64_t config[3])
{
	if (arch_ops != NULL && arch_ops->fixup_config != NULL)
		arch_ops->fixup_config(config);
}

#endif /* PMU_PRIVATE_H */
