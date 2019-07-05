/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Arm Limited
 */

#ifndef _RTE_MCSLOCK_ARM_H_
#define _RTE_MCSLOCK_ARM_H_

#ifndef RTE_FORCE_INTRINSICS
#  error Platform must be built with CONFIG_RTE_FORCE_INTRINSICS
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include "generic/rte_mcslock.h"

#ifdef __cplusplus
}
#endif

#endif /* _RTE_MCSLOCK_ARM_H_ */
