/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Xilinx, Inc.
 */

#ifndef _SFC_FLOW_TUNNEL_H
#define _SFC_FLOW_TUNNEL_H

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#include "efx.h"

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

#define SFC_FT_GET_TUNNEL_MARK(_mark) \
	((_mark) >> SFC_FT_USER_MARK_BITS)

#define SFC_FT_TUNNEL_MARK_INVALID	(0)

#define SFC_FT_TUNNEL_MARK_TO_ID(_tunnel_mark) \
	((_tunnel_mark) - 1)

#define SFC_FT_ID_TO_TUNNEL_MARK(_id) \
	((_id) + 1)

#define SFC_FT_MAX_NTUNNELS \
	(RTE_LEN2MASK(SFC_FT_TUNNEL_MARK_BITS, uint8_t) - 1)

struct sfc_flow_tunnel {
	bool				jump_rule_is_set;
	efx_tunnel_protocol_t		encap_type;
	unsigned int			refcnt;
	sfc_ft_id_t			id;
};

struct sfc_adapter;

bool sfc_flow_tunnel_is_supported(struct sfc_adapter *sa);

bool sfc_flow_tunnel_is_active(struct sfc_adapter *sa);

struct sfc_flow_tunnel *sfc_flow_tunnel_pick(struct sfc_adapter *sa,
					     uint32_t ft_mark);

int sfc_flow_tunnel_detect_jump_rule(struct sfc_adapter *sa,
				     const struct rte_flow_action *actions,
				     struct sfc_flow_spec_mae *spec,
				     struct rte_flow_error *error);

#ifdef __cplusplus
}
#endif
#endif /* _SFC_FLOW_TUNNEL_H */
