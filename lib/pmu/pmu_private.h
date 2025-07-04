/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Marvell
 */

#ifndef PMU_PRIVATE_H
#define PMU_PRIVATE_H

/**
 * Architecture-specific PMU init callback.
 *
 * @return
 *   0 in case of success, negative value otherwise.
 */
int
pmu_arch_init(void);

/**
 * Architecture-specific PMU cleanup callback.
 */
void
pmu_arch_fini(void);

/**
 * Apply architecture-specific settings to config before passing it to syscall.
 *
 * @param config
 *   Architecture-specific event configuration.
 *   Consult kernel sources for available options.
 */
void
pmu_arch_fixup_config(uint64_t config[3]);

#endif /* PMU_PRIVATE_H */
