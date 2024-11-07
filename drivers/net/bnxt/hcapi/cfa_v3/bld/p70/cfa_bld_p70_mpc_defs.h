/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023-2024 Broadcom
 * All rights reserved.
 */

#ifndef _CFA_BLD_P70_MPC_DEFS_H_
#define _CFA_BLD_P70_MPC_DEFS_H_

/*
 * CFA phase 7.0 Action/Lookup cache option values for various accesses
 * From EAS
 */
#define CACHE_READ_OPTION_NORMAL 0x0
#define CACHE_READ_OPTION_EVICT 0x1
#define CACHE_READ_OPTION_FAST_EVICT 0x2
#define CACHE_READ_OPTION_DEBUG_LINE 0x4
#define CACHE_READ_OPTION_DEBUG_TAG  0x5

/*
 * Cache read and clear command expects the cache option bit 3
 * to be set, failing which the clear is not done.
 */
#define CACHE_READ_CLR_MASK (0x1U << 3)
#define CACHE_READ_CLR_OPTION_NORMAL                                           \
	(CACHE_READ_CLR_MASK | CACHE_READ_OPTION_NORMAL)
#define CACHE_READ_CLR_OPTION_EVICT                                            \
	(CACHE_READ_CLR_MASK | CACHE_READ_OPTION_EVICT)
#define CACHE_READ_CLR_OPTION_FAST_EVICT                                       \
	(CACHE_READ_CLR_MASK | CACHE_READ_OPTION_FAST_EVICT)

#define CACHE_WRITE_OPTION_WRITE_BACK 0x0
#define CACHE_WRITE_OPTION_WRITE_THRU 0x1

#define CACHE_EVICT_OPTION_CLEAN_LINES 0x1
#define CACHE_EVICT_OPTION_CLEAN_FAST_LINES 0x2
#define CACHE_EVICT_OPTION_CLEAN_AND_CLEAN_FAST_EVICT_LINES 0x3
#define CACHE_EVICT_OPTION_LINE 0x4
#define CACHE_EVICT_OPTION_SCOPE_ADDRESS 0x5

/* EM/action cache access unit size in bytes */
#define MPC_CFA_CACHE_ACCESS_UNIT_SIZE CFA_P70_CACHE_LINE_BYTES

#endif /* _CFA_BLD_P70_MPC_DEFS_H_ */
