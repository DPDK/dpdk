/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Broadcom
 * All rights reserved.
 */

#ifndef _BCMFS_RM_COMMON_H_
#define _BCMFS_RM_COMMON_H_

#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_io.h>

/* 32-bit MMIO register write */
#define FS_MMIO_WRITE32(value, addr) rte_write32_relaxed((value), (addr))
/* 32-bit MMIO register read */
#define FS_MMIO_READ32(addr) rte_read32_relaxed((addr))

/* Descriptor helper macros */
#define FS_DESC_DEC(d, s, m)			(((d) >> (s)) & (m))

#define FS_RING_BD_ALIGN_CHECK(addr)			\
			(!((addr) & ((0x1 << FS_RING_BD_ALIGN_ORDER) - 1)))

#define cpu_to_le64     rte_cpu_to_le_64
#define cpu_to_le32     rte_cpu_to_le_32
#define cpu_to_le16     rte_cpu_to_le_16

#define le64_to_cpu     rte_le_to_cpu_64
#define le32_to_cpu     rte_le_to_cpu_32
#define le16_to_cpu     rte_le_to_cpu_16

#define lower_32_bits(x) ((uint32_t)(x))
#define upper_32_bits(x) ((uint32_t)(((x) >> 16) >> 16))

uint64_t
rm_build_desc(uint64_t val, uint32_t shift,
	   uint64_t mask);
uint64_t
rm_read_desc(void *desc_ptr);

void
rm_write_desc(void *desc_ptr, uint64_t desc);

uint32_t
rm_cmpl_desc_to_reqid(uint64_t cmpl_desc);

int
rm_cmpl_desc_to_error(uint64_t cmpl_desc);

#endif /* _BCMFS_RM_COMMON_H_ */
