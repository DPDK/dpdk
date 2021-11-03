/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020-2021 Xilinx, Inc.
 */

#ifndef _SFC_VDPA_DEBUG_H_
#define _SFC_VDPA_DEBUG_H_

#include <rte_debug.h>

#ifdef RTE_LIBRTE_SFC_VDPA_DEBUG
/* Avoid dependency from RTE_LOG_DP_LEVEL to be able to enable debug check
 * in the driver only.
 */
#define SFC_VDPA_ASSERT(exp)			RTE_VERIFY(exp)
#else
/* If the driver debug is not enabled, follow DPDK debug/non-debug */
#define SFC_VDPA_ASSERT(exp)			RTE_ASSERT(exp)
#endif

#endif /* _SFC_VDPA_DEBUG_H_ */
