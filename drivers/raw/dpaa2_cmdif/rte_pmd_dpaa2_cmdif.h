/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 NXP
 */

#ifndef __RTE_PMD_DPAA2_CMDIF_H__
#define __RTE_PMD_DPAA2_CMDIF_H__

/**
 * @file
 *
 * NXP dpaa2 AIOP CMDIF PMD specific structures.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

/** The context required in the I/O path for DPAA2 AIOP Command Interface */
struct rte_dpaa2_cmdif_context {
	/** Size to populate in QBMAN FD */
	uint32_t size;
	/** FRC to populate in QBMAN FD */
	uint32_t frc;
	/** FLC to populate in QBMAN FD */
	uint64_t flc;
	/** Priority of the command. This priority determines DPCI Queue*/
	uint8_t priority;
};

#ifdef __cplusplus
}
#endif

#endif /* __RTE_PMD_DPAA2_CMDIF_H__ */
