/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015 RehiveTech. All rights reserved.
 */

#ifndef _RTE_BYTEORDER_ARM_H_
#define _RTE_BYTEORDER_ARM_H_

#ifndef RTE_FORCE_INTRINSICS
#  error Platform must be built with RTE_FORCE_INTRINSICS
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <rte_common.h>
#include "generic/rte_byteorder.h"

/* ARM architecture is bi-endian (both big and little). */
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN

#define rte_cpu_to_le_16(x) (x)
#define rte_cpu_to_le_32(x) (x)
#define rte_cpu_to_le_64(x) (x)

#define rte_cpu_to_be_16(x) rte_bswap16(x)
#define rte_cpu_to_be_32(x) rte_bswap32(x)
#define rte_cpu_to_be_64(x) rte_bswap64(x)

#define rte_le_to_cpu_16(x) (x)
#define rte_le_to_cpu_32(x) (x)
#define rte_le_to_cpu_64(x) (x)

#define rte_be_to_cpu_16(x) rte_bswap16(x)
#define rte_be_to_cpu_32(x) rte_bswap32(x)
#define rte_be_to_cpu_64(x) rte_bswap64(x)

#else /* RTE_BIG_ENDIAN */

#define rte_cpu_to_le_16(x) rte_bswap16(x)
#define rte_cpu_to_le_32(x) rte_bswap32(x)
#define rte_cpu_to_le_64(x) rte_bswap64(x)

#define rte_cpu_to_be_16(x) (x)
#define rte_cpu_to_be_32(x) (x)
#define rte_cpu_to_be_64(x) (x)

#define rte_le_to_cpu_16(x) rte_bswap16(x)
#define rte_le_to_cpu_32(x) rte_bswap32(x)
#define rte_le_to_cpu_64(x) rte_bswap64(x)

#define rte_be_to_cpu_16(x) (x)
#define rte_be_to_cpu_32(x) (x)
#define rte_be_to_cpu_64(x) (x)
#endif

#ifdef __cplusplus
}
#endif

#endif /* _RTE_BYTEORDER_ARM_H_ */
