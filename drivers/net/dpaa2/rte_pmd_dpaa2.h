/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 NXP
 */

#ifndef _RTE_PMD_DPAA2_H
#define _RTE_PMD_DPAA2_H

/**
 * @file rte_pmd_dpaa2.h
 *
 * NXP dpaa2 PMD specific functions.
 *
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 */

#include <rte_flow.h>

enum pmd_dpaa2_ts {
	PMD_DPAA2_DISABLE_TS,
	PMD_DPAA2_ENABLE_TS
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Enable/Disable timestamping update in mbuf for LX2160 kind of devices.
 * For LS2088/LS1088 devices, timestamping will be updated in mbuf without
 * calling this API.
 *
 * @param pmd_dpaa2_ts
 *    Enum to enable/disable timestamp update in mbuf for LX2160 devices.
 */
__rte_experimental
void rte_pmd_dpaa2_set_timestamp(enum pmd_dpaa2_ts);

#endif /* _RTE_PMD_DPAA2_H */
