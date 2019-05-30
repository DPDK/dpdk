/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef __OTX2_RX_H__
#define __OTX2_RX_H__

#define PTYPE_WIDTH 12
#define PTYPE_NON_TUNNEL_ARRAY_SZ	BIT(PTYPE_WIDTH)
#define PTYPE_TUNNEL_ARRAY_SZ		BIT(PTYPE_WIDTH)
#define PTYPE_ARRAY_SZ			((PTYPE_NON_TUNNEL_ARRAY_SZ +\
					 PTYPE_TUNNEL_ARRAY_SZ) *\
					 sizeof(uint16_t))

#define NIX_RX_OFFLOAD_PTYPE_F         BIT(1)

#endif /* __OTX2_RX_H__ */
