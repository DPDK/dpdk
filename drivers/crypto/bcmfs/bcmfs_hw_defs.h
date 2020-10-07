/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Broadcom
 * All rights reserved.
 */

#ifndef _BCMFS_HW_DEFS_H_
#define _BCMFS_HW_DEFS_H_

#include <rte_atomic.h>
#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_io.h>

#ifndef BIT
#define BIT(nr)         (1UL << (nr))
#endif

#define FS_RING_REGS_SIZE		0x10000
#define FS_RING_DESC_SIZE		8
#define FS_RING_BD_ALIGN_ORDER		12
#define FS_RING_BD_DESC_PER_REQ		32
#define FS_RING_CMPL_ALIGN_ORDER	13
#define FS_RING_CMPL_SIZE		(1024 * FS_RING_DESC_SIZE)
#define FS_RING_MAX_REQ_COUNT		1024
#define FS_RING_PAGE_SHFT		12
#define FS_RING_PAGE_SIZE		BIT(FS_RING_PAGE_SHFT)

/* Minimum and maximum number of requests supported */
#define FS_RM_MAX_REQS			4096
#define FS_RM_MIN_REQS			32

#endif /* BCMFS_HW_DEFS_H_ */
