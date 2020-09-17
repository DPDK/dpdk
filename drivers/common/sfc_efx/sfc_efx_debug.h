/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2020 Xilinx, Inc.
 * Copyright(c) 2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#ifndef _SFC_EFX_DEBUG_H_
#define _SFC_EFX_DEBUG_H_

#include <rte_debug.h>

#ifndef RTE_DEBUG_COMMON_SFC_EFX
#define RTE_DEBUG_COMMON_SFC_EFX	0
#endif

#ifdef RTE_DEBUG_COMMON_SFC_EFX
/* Avoid dependency from RTE_LOG_DP_LEVEL to be able to enable debug check
 * in the driver only.
 */
#define SFC_EFX_ASSERT(exp)		RTE_VERIFY(exp)
#else
/* If the driver debug is not enabled, follow DPDK debug/non-debug */
#define SFC_EFX_ASSERT(exp)		RTE_ASSERT(exp)
#endif

#endif /* _SFC_EFX_DEBUG_H_ */
