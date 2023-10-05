/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Netronome Systems, Inc.
 * All rights reserved.
 */

#ifndef __NFP_TARGET_H__
#define __NFP_TARGET_H__

#include <stdint.h>

/* CPP Target IDs */
#define NFP_CPP_TARGET_INVALID          0
#define NFP_CPP_TARGET_NBI              1
#define NFP_CPP_TARGET_QDR              2
#define NFP_CPP_TARGET_ILA              6
#define NFP_CPP_TARGET_MU               7
#define NFP_CPP_TARGET_PCIE             9
#define NFP_CPP_TARGET_ARM              10
#define NFP_CPP_TARGET_CRYPTO           12
#define NFP_CPP_TARGET_ISLAND_XPB       14      /* Shared with CAP */
#define NFP_CPP_TARGET_ISLAND_CAP       14      /* Shared with XPB */
#define NFP_CPP_TARGET_CT_XPB           14
#define NFP_CPP_TARGET_LOCAL_SCRATCH    15
#define NFP_CPP_TARGET_CLS              NFP_CPP_TARGET_LOCAL_SCRATCH

int nfp_target_pushpull(uint32_t cpp_id, uint64_t address);
int nfp_target_cpp(uint32_t cpp_island_id, uint64_t cpp_island_address,
		uint32_t *cpp_target_id, uint64_t *cpp_target_address,
		const uint32_t *imb_table);

#endif /* __NFP_TARGET_H__ */
