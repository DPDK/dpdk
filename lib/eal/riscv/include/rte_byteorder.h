/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Inspired from FreeBSD src/sys/powerpc/include/endian.h
 * Copyright(c) 1987, 1991, 1993
 * The Regents of the University of California.  All rights reserved.
 */

#ifndef RTE_BYTEORDER_RISCV_H
#define RTE_BYTEORDER_RISCV_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <rte_common.h>
#include "generic/rte_byteorder.h"

#ifndef RTE_BYTE_ORDER
#define RTE_BYTE_ORDER RTE_LITTLE_ENDIAN
#endif

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

#ifdef __cplusplus
}
#endif

#endif /* RTE_BYTEORDER_RISCV_H */
