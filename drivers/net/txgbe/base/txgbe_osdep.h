/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020
 */

#ifndef _TXGBE_OS_H_
#define _TXGBE_OS_H_

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <rte_version.h>
#include <rte_common.h>
#include <rte_debug.h>
#include <rte_cycles.h>
#include <rte_log.h>
#include <rte_byteorder.h>
#include <rte_config.h>
#include <rte_io.h>

#include "../txgbe_logs.h"

#define RTE_LIBRTE_TXGBE_TM        DCPV(1, 0)
#define TMZ_PADDR(mz)  ((mz)->iova)
#define TMZ_VADDR(mz)  ((mz)->addr)
#define TDEV_NAME(eth_dev)  ((eth_dev)->device->name)

#define ASSERT(x) do {			\
	if (!(x))			\
		PMD_DRV_LOG(ERR, "TXGBE: %d", x);	\
} while (0)

#define txgbe_unused __rte_unused

#define usec_delay(x) rte_delay_us(x)
#define msec_delay(x) rte_delay_ms(x)
#define usleep(x)     rte_delay_us(x)
#define msleep(x)     rte_delay_ms(x)

#define FALSE               0
#define TRUE                1

#define false               0
#define true                1
#define min(a, b)	RTE_MIN(a, b)
#define max(a, b)	RTE_MAX(a, b)

/* Bunch of defines for shared code bogosity */

static inline void UNREFERENCED(const char *a __rte_unused, ...) {}
#define UNREFERENCED_PARAMETER(args...) UNREFERENCED("", ##args)

#define STATIC static

typedef uint8_t		u8;
typedef int8_t		s8;
typedef uint16_t	u16;
typedef int16_t		s16;
typedef uint32_t	u32;
typedef int32_t		s32;
typedef uint64_t	u64;
typedef int64_t		s64;

/* Little Endian defines */
#ifndef __le16
#define __le16  u16
#define __le32  u32
#define __le64  u64
#endif
#ifndef __be16
#define __be16  u16
#define __be32  u32
#define __be64  u64
#endif

/* Bit shift and mask */
#define BIT_MASK4                 (0x0000000FU)
#define BIT_MASK8                 (0x000000FFU)
#define BIT_MASK16                (0x0000FFFFU)
#define BIT_MASK32                (0xFFFFFFFFU)
#define BIT_MASK64                (0xFFFFFFFFFFFFFFFFUL)

#ifndef cpu_to_le32
#define cpu_to_le16(v)          rte_cpu_to_le_16((u16)(v))
#define cpu_to_le32(v)          rte_cpu_to_le_32((u32)(v))
#define cpu_to_le64(v)          rte_cpu_to_le_64((u64)(v))
#define le_to_cpu16(v)          rte_le_to_cpu_16((u16)(v))
#define le_to_cpu32(v)          rte_le_to_cpu_32((u32)(v))
#define le_to_cpu64(v)          rte_le_to_cpu_64((u64)(v))

#define cpu_to_be16(v)          rte_cpu_to_be_16((u16)(v))
#define cpu_to_be32(v)          rte_cpu_to_be_32((u32)(v))
#define cpu_to_be64(v)          rte_cpu_to_be_64((u64)(v))
#define be_to_cpu16(v)          rte_be_to_cpu_16((u16)(v))
#define be_to_cpu32(v)          rte_be_to_cpu_32((u32)(v))
#define be_to_cpu64(v)          rte_be_to_cpu_64((u64)(v))

#define le_to_be16(v)           rte_bswap16((u16)(v))
#define le_to_be32(v)           rte_bswap32((u32)(v))
#define le_to_be64(v)           rte_bswap64((u64)(v))
#define be_to_le16(v)           rte_bswap16((u16)(v))
#define be_to_le32(v)           rte_bswap32((u32)(v))
#define be_to_le64(v)           rte_bswap64((u64)(v))

#define npu_to_le16(v)          (v)
#define npu_to_le32(v)          (v)
#define npu_to_le64(v)          (v)
#define le_to_npu16(v)          (v)
#define le_to_npu32(v)          (v)
#define le_to_npu64(v)          (v)

#define npu_to_be16(v)          le_to_be16((u16)(v))
#define npu_to_be32(v)          le_to_be32((u32)(v))
#define npu_to_be64(v)          le_to_be64((u64)(v))
#define be_to_npu16(v)          be_to_le16((u16)(v))
#define be_to_npu32(v)          be_to_le32((u32)(v))
#define be_to_npu64(v)          be_to_le64((u64)(v))
#endif /* !cpu_to_le32 */

static inline u16 REVERT_BIT_MASK16(u16 mask)
{
	mask = ((mask & 0x5555) << 1) | ((mask & 0xAAAA) >> 1);
	mask = ((mask & 0x3333) << 2) | ((mask & 0xCCCC) >> 2);
	mask = ((mask & 0x0F0F) << 4) | ((mask & 0xF0F0) >> 4);
	return ((mask & 0x00FF) << 8) | ((mask & 0xFF00) >> 8);
}

static inline u32 REVERT_BIT_MASK32(u32 mask)
{
	mask = ((mask & 0x55555555) << 1) | ((mask & 0xAAAAAAAA) >> 1);
	mask = ((mask & 0x33333333) << 2) | ((mask & 0xCCCCCCCC) >> 2);
	mask = ((mask & 0x0F0F0F0F) << 4) | ((mask & 0xF0F0F0F0) >> 4);
	mask = ((mask & 0x00FF00FF) << 8) | ((mask & 0xFF00FF00) >> 8);
	return ((mask & 0x0000FFFF) << 16) | ((mask & 0xFFFF0000) >> 16);
}

static inline u64 REVERT_BIT_MASK64(u64 mask)
{
	mask = ((mask & 0x5555555555555555) << 1) |
	       ((mask & 0xAAAAAAAAAAAAAAAA) >> 1);
	mask = ((mask & 0x3333333333333333) << 2) |
	       ((mask & 0xCCCCCCCCCCCCCCCC) >> 2);
	mask = ((mask & 0x0F0F0F0F0F0F0F0F) << 4) |
	       ((mask & 0xF0F0F0F0F0F0F0F0) >> 4);
	mask = ((mask & 0x00FF00FF00FF00FF) << 8) |
	       ((mask & 0xFF00FF00FF00FF00) >> 8);
	mask = ((mask & 0x0000FFFF0000FFFF) << 16) |
	       ((mask & 0xFFFF0000FFFF0000) >> 16);
	return ((mask & 0x00000000FFFFFFFF) << 32) |
	       ((mask & 0xFFFFFFFF00000000) >> 32);
}

#define IOMEM

#define prefetch(x) rte_prefetch0(x)

#define ARRAY_SIZE(x) ((int32_t)RTE_DIM(x))

#ifndef MAX_UDELAY_MS
#define MAX_UDELAY_MS 5
#endif

#define ETH_ADDR_LEN	6
#define ETH_FCS_LEN	4

/* Check whether address is multicast. This is little-endian specific check.*/
#define TXGBE_IS_MULTICAST(address) \
		(bool)(((u8 *)(address))[0] & ((u8)0x01))

/* Check whether an address is broadcast. */
#define TXGBE_IS_BROADCAST(address) \
		({typeof(address)addr = (address); \
		(((u8 *)(addr))[0] == ((u8)0xff)) && \
		(((u8 *)(addr))[1] == ((u8)0xff)); })

#define ETH_P_8021Q      0x8100
#define ETH_P_8021AD     0x88A8

#endif /* _TXGBE_OS_H_ */
