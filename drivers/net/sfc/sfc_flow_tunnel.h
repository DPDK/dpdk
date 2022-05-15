/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Xilinx, Inc.
 */

#ifndef _SFC_FLOW_TUNNEL_H
#define _SFC_FLOW_TUNNEL_H

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#include <rte_flow.h>

#include "efx.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Flow Tunnel (FT) SW entry ID */
typedef uint8_t sfc_ft_ctx_id_t;

#define SFC_FT_CTX_MARK_BITS \
	(sizeof(sfc_ft_ctx_id_t) * CHAR_BIT)

#define SFC_FT_USER_MARK_BITS \
	(sizeof(uint32_t) * CHAR_BIT - SFC_FT_CTX_MARK_BITS)

#define SFC_FT_USER_MARK_MASK \
	RTE_LEN2MASK(SFC_FT_USER_MARK_BITS, uint32_t)

#define SFC_FT_FLOW_MARK_TO_CTX_MARK(_flow_mark) \
	((_flow_mark) >> SFC_FT_USER_MARK_BITS)

#define SFC_FT_CTX_MARK_INVALID	(0)

#define SFC_FT_CTX_MARK_TO_CTX_ID(_ctx_mark) \
	((_ctx_mark) - 1)

#define SFC_FT_CTX_ID_TO_CTX_MARK(_ctx_id) \
	((_ctx_id) + 1)

#define SFC_FT_CTX_ID_TO_FLOW_MARK(_ctx_id) \
	(SFC_FT_CTX_ID_TO_CTX_MARK(_ctx_id) << SFC_FT_USER_MARK_BITS)

#define SFC_FT_FLOW_MARK_TO_USER_MARK(_flow_mark) \
	((_flow_mark) & SFC_FT_USER_MARK_MASK)

#define SFC_FT_MAX_NTUNNELS \
	(RTE_LEN2MASK(SFC_FT_CTX_MARK_BITS, uint8_t) - 1)

struct sfc_ft_ctx {
	bool				tunnel_rule_is_set;
	efx_tunnel_protocol_t		encap_type;
	struct rte_flow_tunnel		tunnel;
	unsigned int			refcnt;
	sfc_ft_ctx_id_t			id;

	struct rte_flow_action_mark	action_mark;
	struct rte_flow_action		action;

	struct rte_flow_item_mark	item_mark_v;
	struct rte_flow_item_mark	item_mark_m;
	struct rte_flow_item		item;

	uint64_t			reset_tunnel_hit_counter;
	uint64_t			switch_hit_counter;
};

struct sfc_adapter;

bool sfc_ft_is_supported(struct sfc_adapter *sa);

bool sfc_ft_is_active(struct sfc_adapter *sa);

struct sfc_ft_ctx *sfc_ft_ctx_pick(struct sfc_adapter *sa, uint32_t flow_mark);

int sfc_ft_tunnel_rule_detect(struct sfc_adapter *sa,
			      const struct rte_flow_action *actions,
			      struct sfc_flow_spec_mae *spec,
			      struct rte_flow_error *error);

int sfc_ft_decap_set(struct rte_eth_dev *dev, struct rte_flow_tunnel *tunnel,
		     struct rte_flow_action **pmd_actions,
		     uint32_t *num_of_actions, struct rte_flow_error *err);

int sfc_ft_match(struct rte_eth_dev *dev, struct rte_flow_tunnel *tunnel,
		 struct rte_flow_item **pmd_items, uint32_t *num_of_items,
		 struct rte_flow_error *err);

int sfc_ft_item_release(struct rte_eth_dev *dev,
			struct rte_flow_item *pmd_items, uint32_t num_items,
			struct rte_flow_error *err);

int sfc_ft_action_decap_release(struct rte_eth_dev *dev,
				struct rte_flow_action *pmd_actions,
				uint32_t num_actions,
				struct rte_flow_error *err);

int sfc_ft_get_restore_info(struct rte_eth_dev *dev, struct rte_mbuf *m,
			    struct rte_flow_restore_info *info,
			    struct rte_flow_error *err);

void sfc_ft_counters_reset(struct sfc_adapter *sa);

#ifdef __cplusplus
}
#endif
#endif /* _SFC_FLOW_TUNNEL_H */
