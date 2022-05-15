/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Xilinx, Inc.
 */

#include <stdbool.h>
#include <stdint.h>

#include <rte_flow.h>

#include "sfc.h"
#include "sfc_dp.h"
#include "sfc_flow.h"
#include "sfc_dp_rx.h"
#include "sfc_flow_tunnel.h"
#include "sfc_mae.h"

bool
sfc_ft_is_supported(struct sfc_adapter *sa)
{
	SFC_ASSERT(sfc_adapter_is_locked(sa));

	return ((sa->priv.dp_rx->features & SFC_DP_RX_FEAT_FLOW_MARK) != 0 &&
		sa->mae.status == SFC_MAE_STATUS_ADMIN);
}

bool
sfc_ft_is_active(struct sfc_adapter *sa)
{
	SFC_ASSERT(sfc_adapter_is_locked(sa));

	return ((sa->negotiated_rx_metadata &
		 RTE_ETH_RX_METADATA_TUNNEL_ID) != 0);
}

struct sfc_ft_ctx *
sfc_ft_ctx_pick(struct sfc_adapter *sa, uint32_t flow_mark)
{
	uint8_t ft_ctx_mark = SFC_FT_FLOW_MARK_TO_CTX_MARK(flow_mark);

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	if (ft_ctx_mark != SFC_FT_CTX_MARK_INVALID) {
		sfc_ft_ctx_id_t ft_ctx_id = SFC_FT_CTX_MARK_TO_CTX_ID(ft_ctx_mark);
		struct sfc_ft_ctx *ft_ctx = &sa->ft_ctx_pool[ft_ctx_id];

		ft_ctx->id = ft_ctx_id;

		return ft_ctx;
	}

	return NULL;
}

int
sfc_ft_tunnel_rule_detect(struct sfc_adapter *sa,
			  const struct rte_flow_action *actions,
			  struct sfc_flow_spec_mae *spec,
			  struct rte_flow_error *error)
{
	const struct rte_flow_action_mark *action_mark = NULL;
	const struct rte_flow_action_jump *action_jump = NULL;
	struct sfc_ft_ctx *ft_ctx;
	uint32_t flow_mark = 0;
	int rc = 0;

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	if (!sfc_ft_is_active(sa)) {
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
		case RTE_FLOW_ACTION_TYPE_COUNT:
			break;
		case RTE_FLOW_ACTION_TYPE_MARK:
			if (action_mark == NULL) {
				action_mark = actions->conf;
				flow_mark = action_mark->id;
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

	ft_ctx = sfc_ft_ctx_pick(sa, flow_mark);
	if (ft_ctx != NULL && action_jump != 0) {
		sfc_dbg(sa, "FT: TUNNEL: detected");

		if (rc != 0) {
			/* The loop above might have spotted wrong actions. */
			sfc_err(sa, "FT: TUNNEL: invalid actions: %s",
				strerror(rc));
			goto fail;
		}

		if (ft_ctx->refcnt == 0) {
			sfc_err(sa, "FT: TUNNEL: inactive context (ID=%u)",
				ft_ctx->id);
			rc = ENOENT;
			goto fail;
		}

		if (ft_ctx->tunnel_rule_is_set) {
			sfc_err(sa, "FT: TUNNEL: already setup context (ID=%u)",
				ft_ctx->id);
			rc = EEXIST;
			goto fail;
		}

		spec->ft_rule_type = SFC_FT_RULE_TUNNEL;
		spec->ft_ctx = ft_ctx;
	}

	return 0;

fail:
	return rte_flow_error_set(error, rc,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				  "FT: TUNNEL: preparsing failed");
}

static int
sfc_ft_ctx_attach(struct sfc_adapter *sa, const struct rte_flow_tunnel *tunnel,
		  struct sfc_ft_ctx **ft_ctxp)
{
	sfc_ft_ctx_id_t ft_ctx_id;
	struct sfc_ft_ctx *ft_ctx;
	const char *ft_ctx_status;
	int ft_ctx_id_free = -1;
	int rc;

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	rc = sfc_dp_ft_ctx_id_register();
	if (rc != 0)
		return rc;

	if (tunnel->type != RTE_FLOW_ITEM_TYPE_VXLAN) {
		sfc_err(sa, "FT: unsupported tunnel (encapsulation) type");
		return ENOTSUP;
	}

	for (ft_ctx_id = 0; ft_ctx_id < SFC_FT_MAX_NTUNNELS; ++ft_ctx_id) {
		ft_ctx = &sa->ft_ctx_pool[ft_ctx_id];

		if (ft_ctx->refcnt == 0) {
			if (ft_ctx_id_free == -1)
				ft_ctx_id_free = ft_ctx_id;

			continue;
		}

		if (memcmp(tunnel, &ft_ctx->tunnel, sizeof(*tunnel)) == 0) {
			ft_ctx_status = "existing";
			goto attach;
		}
	}

	if (ft_ctx_id_free == -1) {
		sfc_err(sa, "FT: no free slot for the new context");
		return ENOBUFS;
	}

	ft_ctx_id = ft_ctx_id_free;
	ft_ctx = &sa->ft_ctx_pool[ft_ctx_id];

	memcpy(&ft_ctx->tunnel, tunnel, sizeof(*tunnel));

	ft_ctx->encap_type = EFX_TUNNEL_PROTOCOL_VXLAN;

	ft_ctx->action_mark.id = SFC_FT_CTX_ID_TO_FLOW_MARK(ft_ctx_id);
	ft_ctx->action.type = RTE_FLOW_ACTION_TYPE_MARK;
	ft_ctx->action.conf = &ft_ctx->action_mark;

	ft_ctx->item_mark_v.id = ft_ctx->action_mark.id;
	ft_ctx->item.type = RTE_FLOW_ITEM_TYPE_MARK;
	ft_ctx->item.spec = &ft_ctx->item_mark_v;
	ft_ctx->item.mask = &ft_ctx->item_mark_m;
	ft_ctx->item_mark_m.id = UINT32_MAX;

	ft_ctx->tunnel_rule_is_set = B_FALSE;

	ft_ctx->refcnt = 0;

	ft_ctx_status = "newly added";

attach:
	sfc_dbg(sa, "FT: attaching to %s context (ID=%u)",
		ft_ctx_status, ft_ctx_id);

	++(ft_ctx->refcnt);
	*ft_ctxp = ft_ctx;

	return 0;
}

static int
sfc_ft_ctx_detach(struct sfc_adapter *sa, uint32_t flow_mark)
{
	struct sfc_ft_ctx *ft_ctx;

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	ft_ctx = sfc_ft_ctx_pick(sa, flow_mark);
	if (ft_ctx == NULL) {
		sfc_err(sa, "FT: invalid context");
		return EINVAL;
	}

	if (ft_ctx->refcnt == 0) {
		sfc_err(sa, "FT: inactive context (ID=%u)", ft_ctx->id);
		return ENOENT;
	}

	--(ft_ctx->refcnt);

	return 0;
}

int
sfc_ft_decap_set(struct rte_eth_dev *dev, struct rte_flow_tunnel *tunnel,
		 struct rte_flow_action **pmd_actions, uint32_t *num_of_actions,
		 struct rte_flow_error *err)
{
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
	struct sfc_ft_ctx *ft_ctx;
	int rc;

	sfc_adapter_lock(sa);

	if (!sfc_ft_is_active(sa)) {
		rc = ENOTSUP;
		goto fail;
	}

	rc = sfc_ft_ctx_attach(sa, tunnel, &ft_ctx);
	if (rc != 0)
		goto fail;

	*pmd_actions = &ft_ctx->action;
	*num_of_actions = 1;

	sfc_adapter_unlock(sa);

	return 0;

fail:
	sfc_adapter_unlock(sa);

	return rte_flow_error_set(err, rc,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				  "FT: decap_set failed");
}

int
sfc_ft_match(struct rte_eth_dev *dev, struct rte_flow_tunnel *tunnel,
	     struct rte_flow_item **pmd_items, uint32_t *num_of_items,
	     struct rte_flow_error *err)
{
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
	struct sfc_ft_ctx *ft_ctx;
	int rc;

	sfc_adapter_lock(sa);

	if (!sfc_ft_is_active(sa)) {
		rc = ENOTSUP;
		goto fail;
	}

	rc = sfc_ft_ctx_attach(sa, tunnel, &ft_ctx);
	if (rc != 0)
		goto fail;

	*pmd_items = &ft_ctx->item;
	*num_of_items = 1;

	sfc_adapter_unlock(sa);

	return 0;

fail:
	sfc_adapter_unlock(sa);

	return rte_flow_error_set(err, rc,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				  "FT: tunnel_match failed");
}

int
sfc_ft_item_release(struct rte_eth_dev *dev, struct rte_flow_item *pmd_items,
		    uint32_t num_items, struct rte_flow_error *err)
{
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
	const struct rte_flow_item_mark *item_mark;
	struct rte_flow_item *item = pmd_items;
	int rc;

	sfc_adapter_lock(sa);

	if (!sfc_ft_is_active(sa)) {
		rc = ENOTSUP;
		goto fail;
	}

	if (num_items != 1 || item == NULL || item->spec == NULL ||
	    item->type != RTE_FLOW_ITEM_TYPE_MARK) {
		sfc_err(sa, "FT: item_release: wrong input");
		rc = EINVAL;
		goto fail;
	}

	item_mark = item->spec;

	rc = sfc_ft_ctx_detach(sa, item_mark->id);
	if (rc != 0)
		goto fail;

	sfc_adapter_unlock(sa);

	return 0;

fail:
	sfc_adapter_unlock(sa);

	return rte_flow_error_set(err, rc,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				  "FT: item_release failed");
}

int
sfc_ft_action_decap_release(struct rte_eth_dev *dev,
			    struct rte_flow_action *pmd_actions,
			    uint32_t num_actions, struct rte_flow_error *err)
{
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
	const struct rte_flow_action_mark *action_mark;
	struct rte_flow_action *action = pmd_actions;
	int rc;

	sfc_adapter_lock(sa);

	if (!sfc_ft_is_active(sa)) {
		rc = ENOTSUP;
		goto fail;
	}

	if (num_actions != 1 || action == NULL || action->conf == NULL ||
	    action->type != RTE_FLOW_ACTION_TYPE_MARK) {
		sfc_err(sa, "FT: action_decap_release: wrong input");
		rc = EINVAL;
		goto fail;
	}

	action_mark = action->conf;

	rc = sfc_ft_ctx_detach(sa, action_mark->id);
	if (rc != 0)
		goto fail;

	sfc_adapter_unlock(sa);

	return 0;

fail:
	sfc_adapter_unlock(sa);

	return rte_flow_error_set(err, rc,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				  "FT: item_release failed");
}

int
sfc_ft_get_restore_info(struct rte_eth_dev *dev, struct rte_mbuf *m,
			struct rte_flow_restore_info *info,
			struct rte_flow_error *err)
{
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
	const struct sfc_ft_ctx *ft_ctx;
	sfc_ft_ctx_id_t ft_ctx_id;
	int rc;

	sfc_adapter_lock(sa);

	if ((m->ol_flags & sfc_dp_ft_ctx_id_valid) == 0) {
		sfc_dbg(sa, "FT: get_restore_info: no FT context mark in the packet");
		rc = EINVAL;
		goto fail;
	}

	ft_ctx_id = *RTE_MBUF_DYNFIELD(m, sfc_dp_ft_ctx_id_offset,
				    sfc_ft_ctx_id_t *);
	ft_ctx = &sa->ft_ctx_pool[ft_ctx_id];

	if (ft_ctx->refcnt == 0) {
		sfc_dbg(sa, "FT: get_restore_info: inactive context (ID=%u)",
			ft_ctx_id);
		rc = ENOENT;
		goto fail;
	}

	memcpy(&info->tunnel, &ft_ctx->tunnel, sizeof(info->tunnel));

	/*
	 * The packet still has encapsulation header; TUNNEL rules never
	 * strip it. Therefore, set RTE_FLOW_RESTORE_INFO_ENCAPSULATED.
	 */
	info->flags = RTE_FLOW_RESTORE_INFO_ENCAPSULATED |
		      RTE_FLOW_RESTORE_INFO_GROUP_ID |
		      RTE_FLOW_RESTORE_INFO_TUNNEL;

	info->group_id = 0;

	sfc_adapter_unlock(sa);

	return 0;

fail:
	sfc_adapter_unlock(sa);

	return rte_flow_error_set(err, rc,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				  "FT: get_restore_info failed");
}

void
sfc_ft_counters_reset(struct sfc_adapter *sa)
{
	unsigned int i;

	SFC_ASSERT(sfc_adapter_is_locked(sa));
	SFC_ASSERT(sa->state != SFC_ETHDEV_STARTED);

	for (i = 0; i < RTE_DIM(sa->ft_ctx_pool); ++i) {
		struct sfc_ft_ctx *ft_ctx = &sa->ft_ctx_pool[i];

		ft_ctx->reset_tunnel_hit_counter = 0;
		ft_ctx->switch_hit_counter = 0;
	}
}
