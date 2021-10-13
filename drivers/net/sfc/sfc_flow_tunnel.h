/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Xilinx, Inc.
 */

#ifndef _SFC_FLOW_TUNNEL_H
#define _SFC_FLOW_TUNNEL_H

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Flow Tunnel (FT) SW entry ID */
typedef uint8_t sfc_ft_id_t;

#define SFC_FT_TUNNEL_MARK_BITS \
	(sizeof(sfc_ft_id_t) * CHAR_BIT)

#define SFC_FT_USER_MARK_BITS \
	(sizeof(uint32_t) * CHAR_BIT - SFC_FT_TUNNEL_MARK_BITS)

#define SFC_FT_USER_MARK_MASK \
	RTE_LEN2MASK(SFC_FT_USER_MARK_BITS, uint32_t)

struct sfc_adapter;

bool sfc_flow_tunnel_is_supported(struct sfc_adapter *sa);

bool sfc_flow_tunnel_is_active(struct sfc_adapter *sa);

#ifdef __cplusplus
}
#endif
#endif /* _SFC_FLOW_TUNNEL_H */
