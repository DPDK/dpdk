/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Nebulamatrix Technology Co., Ltd.
 */

#ifndef _NBL_TXRX_H_
#define _NBL_TXRX_H_

#include "nbl_resource.h"

#define NBL_PACKED_DESC_F_AVAIL			(7)
#define NBL_PACKED_DESC_F_USED			(15)
#define NBL_VRING_DESC_F_NEXT			(1 << 0)
#define NBL_VRING_DESC_F_WRITE			(1 << 1)

#define NBL_TX_RS_THRESH			(32)
#define NBL_TX_HEADER_LEN			(32)
#define NBL_VQ_HDR_NAME_MAXSIZE			(32)

#define NBL_DESC_PER_LOOP_VEC_MAX		(8)

#endif
