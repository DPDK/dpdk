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
sfc_flow_tunnel_is_supported(struct sfc_adapter *sa)
{
	SFC_ASSERT(sfc_adapter_is_locked(sa));

	return ((sa->priv.dp_rx->features & SFC_DP_RX_FEAT_FLOW_MARK) != 0 &&
		sa->mae.status == SFC_MAE_STATUS_ADMIN);
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
		case RTE_FLOW_ACTION_TYPE_COUNT:
			break;
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

static int
sfc_flow_tunnel_attach(struct sfc_adapter *sa,
		       struct rte_flow_tunnel *tunnel,
		       struct sfc_flow_tunnel **ftp)
{
	struct sfc_flow_tunnel *ft;
	const char *ft_status;
	int ft_id_free = -1;
	sfc_ft_id_t ft_id;
	int rc;

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	rc = sfc_dp_ft_id_register();
	if (rc != 0)
		return rc;

	if (tunnel->type != RTE_FLOW_ITEM_TYPE_VXLAN) {
		sfc_err(sa, "tunnel offload: unsupported tunnel (encapsulation) type");
		return ENOTSUP;
	}

	for (ft_id = 0; ft_id < SFC_FT_MAX_NTUNNELS; ++ft_id) {
		ft = &sa->flow_tunnels[ft_id];

		if (ft->refcnt == 0) {
			if (ft_id_free == -1)
				ft_id_free = ft_id;

			continue;
		}

		if (memcmp(tunnel, &ft->rte_tunnel, sizeof(*tunnel)) == 0) {
			ft_status = "existing";
			goto attach;
		}
	}

	if (ft_id_free == -1) {
		sfc_err(sa, "tunnel offload: no free slot for the new tunnel");
		return ENOBUFS;
	}

	ft_id = ft_id_free;
	ft = &sa->flow_tunnels[ft_id];

	memcpy(&ft->rte_tunnel, tunnel, sizeof(*tunnel));

	ft->encap_type = EFX_TUNNEL_PROTOCOL_VXLAN;

	ft->action_mark.id = SFC_FT_ID_TO_MARK(ft_id_free);
	ft->action.type = RTE_FLOW_ACTION_TYPE_MARK;
	ft->action.conf = &ft->action_mark;

	ft->item.type = RTE_FLOW_ITEM_TYPE_MARK;
	ft->item_mark_v.id = ft->action_mark.id;
	ft->item.spec = &ft->item_mark_v;
	ft->item.mask = &ft->item_mark_m;
	ft->item_mark_m.id = UINT32_MAX;

	ft->jump_rule_is_set = B_FALSE;

	ft->refcnt = 0;

	ft_status = "newly added";

attach:
	sfc_dbg(sa, "tunnel offload: attaching to %s tunnel=%u",
		ft_status, ft_id);

	++(ft->refcnt);
	*ftp = ft;

	return 0;
}

static int
sfc_flow_tunnel_detach(struct sfc_adapter *sa,
		       uint32_t ft_mark)
{
	struct sfc_flow_tunnel *ft;

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	ft = sfc_flow_tunnel_pick(sa, ft_mark);
	if (ft == NULL) {
		sfc_err(sa, "tunnel offload: invalid tunnel");
		return EINVAL;
	}

	if (ft->refcnt == 0) {
		sfc_err(sa, "tunnel offload: tunnel=%u does not exist", ft->id);
		return ENOENT;
	}

	--(ft->refcnt);

	return 0;
}

int
sfc_flow_tunnel_decap_set(struct rte_eth_dev *dev,
			  struct rte_flow_tunnel *tunnel,
			  struct rte_flow_action **pmd_actions,
			  uint32_t *num_of_actions,
			  struct rte_flow_error *err)
{
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
	struct sfc_flow_tunnel *ft;
	int rc;

	sfc_adapter_lock(sa);

	if (!sfc_flow_tunnel_is_active(sa)) {
		rc = ENOTSUP;
		goto fail;
	}

	rc = sfc_flow_tunnel_attach(sa, tunnel, &ft);
	if (rc != 0)
		goto fail;

	*pmd_actions = &ft->action;
	*num_of_actions = 1;

	sfc_adapter_unlock(sa);

	return 0;

fail:
	sfc_adapter_unlock(sa);

	return rte_flow_error_set(err, rc,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				  "tunnel offload: decap_set failed");
}

int
sfc_flow_tunnel_match(struct rte_eth_dev *dev,
		      struct rte_flow_tunnel *tunnel,
		      struct rte_flow_item **pmd_items,
		      uint32_t *num_of_items,
		      struct rte_flow_error *err)
{
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
	struct sfc_flow_tunnel *ft;
	int rc;

	sfc_adapter_lock(sa);

	if (!sfc_flow_tunnel_is_active(sa)) {
		rc = ENOTSUP;
		goto fail;
	}

	rc = sfc_flow_tunnel_attach(sa, tunnel, &ft);
	if (rc != 0)
		goto fail;

	*pmd_items = &ft->item;
	*num_of_items = 1;

	sfc_adapter_unlock(sa);

	return 0;

fail:
	sfc_adapter_unlock(sa);

	return rte_flow_error_set(err, rc,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				  "tunnel offload: tunnel_match failed");
}

int
sfc_flow_tunnel_item_release(struct rte_eth_dev *dev,
			     struct rte_flow_item *pmd_items,
			     uint32_t num_items,
			     struct rte_flow_error *err)
{
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
	const struct rte_flow_item_mark *item_mark;
	struct rte_flow_item *item = pmd_items;
	int rc;

	sfc_adapter_lock(sa);

	if (!sfc_flow_tunnel_is_active(sa)) {
		rc = ENOTSUP;
		goto fail;
	}

	if (num_items != 1 || item == NULL || item->spec == NULL ||
	    item->type != RTE_FLOW_ITEM_TYPE_MARK) {
		sfc_err(sa, "tunnel offload: item_release: wrong input");
		rc = EINVAL;
		goto fail;
	}

	item_mark = item->spec;

	rc = sfc_flow_tunnel_detach(sa, item_mark->id);
	if (rc != 0)
		goto fail;

	sfc_adapter_unlock(sa);

	return 0;

fail:
	sfc_adapter_unlock(sa);

	return rte_flow_error_set(err, rc,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				  "tunnel offload: item_release failed");
}

int
sfc_flow_tunnel_action_decap_release(struct rte_eth_dev *dev,
				     struct rte_flow_action *pmd_actions,
				     uint32_t num_actions,
				     struct rte_flow_error *err)
{
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
	const struct rte_flow_action_mark *action_mark;
	struct rte_flow_action *action = pmd_actions;
	int rc;

	sfc_adapter_lock(sa);

	if (!sfc_flow_tunnel_is_active(sa)) {
		rc = ENOTSUP;
		goto fail;
	}

	if (num_actions != 1 || action == NULL || action->conf == NULL ||
	    action->type != RTE_FLOW_ACTION_TYPE_MARK) {
		sfc_err(sa, "tunnel offload: action_decap_release: wrong input");
		rc = EINVAL;
		goto fail;
	}

	action_mark = action->conf;

	rc = sfc_flow_tunnel_detach(sa, action_mark->id);
	if (rc != 0)
		goto fail;

	sfc_adapter_unlock(sa);

	return 0;

fail:
	sfc_adapter_unlock(sa);

	return rte_flow_error_set(err, rc,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				  "tunnel offload: item_release failed");
}

int
sfc_flow_tunnel_get_restore_info(struct rte_eth_dev *dev,
				 struct rte_mbuf *m,
				 struct rte_flow_restore_info *info,
				 struct rte_flow_error *err)
{
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
	const struct sfc_flow_tunnel *ft;
	sfc_ft_id_t ft_id;
	int rc;

	sfc_adapter_lock(sa);

	if ((m->ol_flags & sfc_dp_ft_id_valid) == 0) {
		sfc_dbg(sa, "tunnel offload: get_restore_info: no tunnel mark in the packet");
		rc = EINVAL;
		goto fail;
	}

	ft_id = *RTE_MBUF_DYNFIELD(m, sfc_dp_ft_id_offset, sfc_ft_id_t *);
	ft = &sa->flow_tunnels[ft_id];

	if (ft->refcnt == 0) {
		sfc_dbg(sa, "tunnel offload: get_restore_info: tunnel=%u does not exist",
			ft_id);
		rc = ENOENT;
		goto fail;
	}

	memcpy(&info->tunnel, &ft->rte_tunnel, sizeof(info->tunnel));

	/*
	 * The packet still has encapsulation header; JUMP rules never
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
				  "tunnel offload: get_restore_info failed");
}

void
sfc_flow_tunnel_reset_hit_counters(struct sfc_adapter *sa)
{
	unsigned int i;

	SFC_ASSERT(sfc_adapter_is_locked(sa));
	SFC_ASSERT(sa->state != SFC_ETHDEV_STARTED);

	for (i = 0; i < RTE_DIM(sa->flow_tunnels); ++i) {
		struct sfc_flow_tunnel *ft = &sa->flow_tunnels[i];

		ft->reset_jump_hit_counter = 0;
		ft->group_hit_counter = 0;
	}
}
