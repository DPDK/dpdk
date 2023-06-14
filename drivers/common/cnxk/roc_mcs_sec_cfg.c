/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

int
roc_mcs_rsrc_alloc(struct roc_mcs *mcs, struct roc_mcs_alloc_rsrc_req *req,
		   struct roc_mcs_alloc_rsrc_rsp *rsp)
{
	struct mcs_priv *priv = roc_mcs_to_mcs_priv(mcs);
	struct mcs_alloc_rsrc_req *rsrc_req;
	struct mcs_alloc_rsrc_rsp *rsrc_rsp;
	int rc, i;

	MCS_SUPPORT_CHECK;

	if (req == NULL || rsp == NULL)
		return -EINVAL;

	rsrc_req = mbox_alloc_msg_mcs_alloc_resources(mcs->mbox);
	if (rsrc_req == NULL)
		return -ENOMEM;

	rsrc_req->rsrc_type = req->rsrc_type;
	rsrc_req->rsrc_cnt = req->rsrc_cnt;
	rsrc_req->mcs_id = mcs->idx;
	rsrc_req->dir = req->dir;
	rsrc_req->all = req->all;

	rc = mbox_process_msg(mcs->mbox, (void *)&rsrc_rsp);
	if (rc)
		return rc;

	if (rsrc_rsp->all) {
		rsrc_rsp->rsrc_cnt = 1;
		rsrc_rsp->rsrc_type = 0xFF;
	}

	for (i = 0; i < rsrc_rsp->rsrc_cnt; i++) {
		switch (rsrc_rsp->rsrc_type) {
		case MCS_RSRC_TYPE_FLOWID:
			rsp->flow_ids[i] = rsrc_rsp->flow_ids[i];
			plt_bitmap_set(priv->dev_rsrc.tcam_bmap,
				       rsp->flow_ids[i] +
					       ((req->dir == MCS_TX) ? priv->tcam_entries : 0));
			break;
		case MCS_RSRC_TYPE_SECY:
			rsp->secy_ids[i] = rsrc_rsp->secy_ids[i];
			plt_bitmap_set(priv->dev_rsrc.secy_bmap,
				       rsp->secy_ids[i] +
					       ((req->dir == MCS_TX) ? priv->secy_entries : 0));
			break;
		case MCS_RSRC_TYPE_SC:
			rsp->sc_ids[i] = rsrc_rsp->sc_ids[i];
			plt_bitmap_set(priv->dev_rsrc.sc_bmap,
				       rsp->sc_ids[i] +
					       ((req->dir == MCS_TX) ? priv->sc_entries : 0));
			break;
		case MCS_RSRC_TYPE_SA:
			rsp->sa_ids[i] = rsrc_rsp->sa_ids[i];
			plt_bitmap_set(priv->dev_rsrc.sa_bmap,
				       rsp->sa_ids[i] +
					       ((req->dir == MCS_TX) ? priv->sa_entries : 0));
			break;
		default:
			rsp->flow_ids[i] = rsrc_rsp->flow_ids[i];
			rsp->secy_ids[i] = rsrc_rsp->secy_ids[i];
			rsp->sc_ids[i] = rsrc_rsp->sc_ids[i];
			rsp->sa_ids[i] = rsrc_rsp->sa_ids[i];
			plt_bitmap_set(priv->dev_rsrc.tcam_bmap,
				       rsp->flow_ids[i] +
					       ((req->dir == MCS_TX) ? priv->tcam_entries : 0));
			plt_bitmap_set(priv->dev_rsrc.secy_bmap,
				       rsp->secy_ids[i] +
					       ((req->dir == MCS_TX) ? priv->secy_entries : 0));
			plt_bitmap_set(priv->dev_rsrc.sc_bmap,
				       rsp->sc_ids[i] +
					       ((req->dir == MCS_TX) ? priv->sc_entries : 0));
			plt_bitmap_set(priv->dev_rsrc.sa_bmap,
				       rsp->sa_ids[i] +
					       ((req->dir == MCS_TX) ? priv->sa_entries : 0));
			break;
		}
	}
	rsp->rsrc_type = rsrc_rsp->rsrc_type;
	rsp->rsrc_cnt = rsrc_rsp->rsrc_cnt;
	rsp->dir = rsrc_rsp->dir;
	rsp->all = rsrc_rsp->all;

	return 0;
}

int
roc_mcs_rsrc_free(struct roc_mcs *mcs, struct roc_mcs_free_rsrc_req *free_req)
{
	struct mcs_priv *priv = roc_mcs_to_mcs_priv(mcs);
	struct mcs_free_rsrc_req *req;
	struct msg_rsp *rsp;
	uint32_t pos;
	int i, rc;

	MCS_SUPPORT_CHECK;

	if (free_req == NULL)
		return -EINVAL;

	req = mbox_alloc_msg_mcs_free_resources(mcs->mbox);
	if (req == NULL)
		return -ENOMEM;

	req->rsrc_id = free_req->rsrc_id;
	req->rsrc_type = free_req->rsrc_type;
	req->mcs_id = mcs->idx;
	req->dir = free_req->dir;
	req->all = free_req->all;

	rc = mbox_process_msg(mcs->mbox, (void *)&rsp);
	if (rc)
		return rc;

	switch (free_req->rsrc_type) {
	case MCS_RSRC_TYPE_FLOWID:
		pos = free_req->rsrc_id + ((req->dir == MCS_TX) ? priv->tcam_entries : 0);
		plt_bitmap_clear(priv->dev_rsrc.tcam_bmap, pos);
		for (i = 0; i < MAX_PORTS_PER_MCS; i++) {
			uint32_t set = plt_bitmap_get(priv->port_rsrc[i].tcam_bmap, pos);

			if (set) {
				plt_bitmap_clear(priv->port_rsrc[i].tcam_bmap, pos);
				break;
			}
		}
		break;
	case MCS_RSRC_TYPE_SECY:
		pos = free_req->rsrc_id + ((req->dir == MCS_TX) ? priv->secy_entries : 0);
		plt_bitmap_clear(priv->dev_rsrc.secy_bmap, pos);
		for (i = 0; i < MAX_PORTS_PER_MCS; i++) {
			uint32_t set = plt_bitmap_get(priv->port_rsrc[i].secy_bmap, pos);

			if (set) {
				plt_bitmap_clear(priv->port_rsrc[i].secy_bmap, pos);
				break;
			}
		}
		break;
	case MCS_RSRC_TYPE_SC:
		pos = free_req->rsrc_id + ((req->dir == MCS_TX) ? priv->sc_entries : 0);
		plt_bitmap_clear(priv->dev_rsrc.sc_bmap, pos);
		for (i = 0; i < MAX_PORTS_PER_MCS; i++) {
			uint32_t set = plt_bitmap_get(priv->port_rsrc[i].sc_bmap, pos);

			if (set) {
				plt_bitmap_clear(priv->port_rsrc[i].sc_bmap, pos);
				break;
			}
		}
		break;
	case MCS_RSRC_TYPE_SA:
		pos = free_req->rsrc_id + ((req->dir == MCS_TX) ? priv->sa_entries : 0);
		plt_bitmap_clear(priv->dev_rsrc.sa_bmap, pos);
		for (i = 0; i < MAX_PORTS_PER_MCS; i++) {
			uint32_t set = plt_bitmap_get(priv->port_rsrc[i].sa_bmap, pos);

			if (set) {
				plt_bitmap_clear(priv->port_rsrc[i].sa_bmap, pos);
				break;
			}
		}
		break;
	default:
		break;
	}

	return rc;
}

int
roc_mcs_sa_policy_write(struct roc_mcs *mcs, struct roc_mcs_sa_plcy_write_req *sa_plcy)
{
	struct mcs_sa_plcy_write_req *sa;
	struct msg_rsp *rsp;

	MCS_SUPPORT_CHECK;

	if (sa_plcy == NULL)
		return -EINVAL;

	sa = mbox_alloc_msg_mcs_sa_plcy_write(mcs->mbox);
	if (sa == NULL)
		return -ENOMEM;

	mbox_memcpy(sa->plcy, sa_plcy->plcy, sizeof(uint64_t) * 2 * 9);
	sa->sa_index[0] = sa_plcy->sa_index[0];
	sa->sa_index[1] = sa_plcy->sa_index[1];
	sa->sa_cnt = sa_plcy->sa_cnt;
	sa->mcs_id = mcs->idx;
	sa->dir = sa_plcy->dir;

	return mbox_process_msg(mcs->mbox, (void *)&rsp);
}

int
roc_mcs_sa_policy_read(struct roc_mcs *mcs __plt_unused,
		       struct roc_mcs_sa_plcy_write_req *sa __plt_unused)
{
	MCS_SUPPORT_CHECK;

	return -ENOTSUP;
}
