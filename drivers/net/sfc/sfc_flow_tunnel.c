/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Xilinx, Inc.
 */

#include <stdbool.h>
#include <stdint.h>

#include "sfc.h"
#include "sfc_flow.h"
#include "sfc_dp_rx.h"
#include "sfc_flow_tunnel.h"
#include "sfc_mae.h"

bool
sfc_flow_tunnel_is_supported(struct sfc_adapter *sa)
{
	SFC_ASSERT(sfc_adapter_is_locked(sa));

	return ((sa->priv.dp_rx->features & SFC_DP_RX_FEAT_FLOW_MARK) != 0 &&
		sa->mae.status == SFC_MAE_STATUS_SUPPORTED);
}

bool
sfc_flow_tunnel_is_active(struct sfc_adapter *sa)
{
	SFC_ASSERT(sfc_adapter_is_locked(sa));

	return ((sa->negotiated_rx_metadata &
		 RTE_ETH_RX_METADATA_TUNNEL_ID) != 0);
}

struct sfc_flow_tunnel *
sfc_flow_tunnel_pick(struct sfc_adapter *sa, uint32_t ft_mark)
{
	uint32_t tunnel_mark = SFC_FT_GET_TUNNEL_MARK(ft_mark);

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	if (tunnel_mark != SFC_FT_TUNNEL_MARK_INVALID) {
		sfc_ft_id_t ft_id = SFC_FT_TUNNEL_MARK_TO_ID(tunnel_mark);
		struct sfc_flow_tunnel *ft = &sa->flow_tunnels[ft_id];

		ft->id = ft_id;

		return ft;
	}

	return NULL;
}

int
sfc_flow_tunnel_detect_jump_rule(struct sfc_adapter *sa,
				 const struct rte_flow_action *actions,
				 struct sfc_flow_spec_mae *spec,
				 struct rte_flow_error *error)
{
	const struct rte_flow_action_mark *action_mark = NULL;
	const struct rte_flow_action_jump *action_jump = NULL;
	struct sfc_flow_tunnel *ft;
	uint32_t ft_mark = 0;
	int rc = 0;

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	if (!sfc_flow_tunnel_is_active(sa)) {
		/* Tunnel-related actions (if any) will be turned down later. */
		return 0;
	}

	if (actions == NULL) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION_NUM, NULL,
				   "NULL actions");
		return -rte_errno;
	}

	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; ++actions) {
		if (actions->type == RTE_FLOW_ACTION_TYPE_VOID)
			continue;

		if (actions->conf == NULL) {
			rc = EINVAL;
			continue;
		}

		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_MARK:
			if (action_mark == NULL) {
				action_mark = actions->conf;
				ft_mark = action_mark->id;
			} else {
				rc = EINVAL;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_JUMP:
			if (action_jump == NULL) {
				action_jump = actions->conf;
				if (action_jump->group != 0)
					rc = EINVAL;
			} else {
				rc = EINVAL;
			}
			break;
		default:
			rc = ENOTSUP;
			break;
		}
	}

	ft = sfc_flow_tunnel_pick(sa, ft_mark);
	if (ft != NULL && action_jump != 0) {
		sfc_dbg(sa, "tunnel offload: JUMP: detected");

		if (rc != 0) {
			/* The loop above might have spotted wrong actions. */
			sfc_err(sa, "tunnel offload: JUMP: invalid actions: %s",
				strerror(rc));
			goto fail;
		}

		if (ft->refcnt == 0) {
			sfc_err(sa, "tunnel offload: JUMP: tunnel=%u does not exist",
				ft->id);
			rc = ENOENT;
			goto fail;
		}

		if (ft->jump_rule_is_set) {
			sfc_err(sa, "tunnel offload: JUMP: already exists in tunnel=%u",
				ft->id);
			rc = EEXIST;
			goto fail;
		}

		spec->ft_rule_type = SFC_FT_RULE_JUMP;
		spec->ft = ft;
	}

	return 0;

fail:
	return rte_flow_error_set(error, rc,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				  "tunnel offload: JUMP: preparsing failed");
}
