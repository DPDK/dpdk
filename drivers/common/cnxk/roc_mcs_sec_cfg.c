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


int
roc_mcs_rx_sc_cam_write(struct roc_mcs *mcs, struct roc_mcs_rx_sc_cam_write_req *rx_sc_cam)
{
	struct mcs_priv *priv = roc_mcs_to_mcs_priv(mcs);
	struct mcs_rx_sc_cam_write_req *rx_sc;
	struct msg_rsp *rsp;
	int i, rc;

	MCS_SUPPORT_CHECK;

	if (rx_sc_cam == NULL)
		return -EINVAL;

	rx_sc = mbox_alloc_msg_mcs_rx_sc_cam_write(mcs->mbox);
	if (rx_sc == NULL)
		return -ENOMEM;

	rx_sc->sci = rx_sc_cam->sci;
	rx_sc->secy_id = rx_sc_cam->secy_id;
	rx_sc->sc_id = rx_sc_cam->sc_id;
	rx_sc->mcs_id = mcs->idx;

	rc = mbox_process_msg(mcs->mbox, (void *)&rsp);
	if (rc)
		return rc;

	for (i = 0; i < MAX_PORTS_PER_MCS; i++) {
		uint32_t set = plt_bitmap_get(priv->port_rsrc[i].secy_bmap, rx_sc_cam->secy_id);

		if (set) {
			plt_bitmap_set(priv->port_rsrc[i].sc_bmap, rx_sc_cam->sc_id);
			break;
		}
	}

	return 0;
}

int
roc_mcs_rx_sc_cam_read(struct roc_mcs *mcs __plt_unused,
		       struct roc_mcs_rx_sc_cam_write_req *rx_sc_cam __plt_unused)
{
	MCS_SUPPORT_CHECK;

	return -ENOTSUP;
}

int
roc_mcs_rx_sc_cam_enable(struct roc_mcs *mcs __plt_unused,
			 struct roc_mcs_rx_sc_cam_write_req *rx_sc_cam __plt_unused)
{
	MCS_SUPPORT_CHECK;

	return -ENOTSUP;
}

int
roc_mcs_rx_sc_sa_map_write(struct roc_mcs *mcs, struct roc_mcs_rx_sc_sa_map *rx_sc_sa_map)
{
	struct mcs_priv *priv = roc_mcs_to_mcs_priv(mcs);
	struct mcs_rx_sc_sa_map *sa_map;
	struct msg_rsp *rsp;
	uint16_t sc_id;
	int i, rc;

	MCS_SUPPORT_CHECK;

	if (rx_sc_sa_map == NULL)
		return -EINVAL;

	sc_id = rx_sc_sa_map->sc_id;
	sa_map = mbox_alloc_msg_mcs_rx_sc_sa_map_write(mcs->mbox);
	if (sa_map == NULL)
		return -ENOMEM;

	sa_map->sa_index = rx_sc_sa_map->sa_index;
	sa_map->sa_in_use = rx_sc_sa_map->sa_in_use;
	sa_map->sc_id = rx_sc_sa_map->sc_id;
	sa_map->an = rx_sc_sa_map->an;
	sa_map->mcs_id = mcs->idx;

	rc = mbox_process_msg(mcs->mbox, (void *)&rsp);
	if (rc)
		return rc;

	for (i = 0; i < MAX_PORTS_PER_MCS; i++) {
		uint32_t set = plt_bitmap_get(priv->port_rsrc[i].sc_bmap, sc_id);

		if (set) {
			plt_bitmap_set(priv->port_rsrc[i].sa_bmap, rx_sc_sa_map->sa_index);
			priv->port_rsrc[i].sc_conf[sc_id].rx.sa_idx = rx_sc_sa_map->sa_index;
			priv->port_rsrc[i].sc_conf[sc_id].rx.an = rx_sc_sa_map->an;
			break;
		}
	}

	return 0;
}

int
roc_mcs_rx_sc_sa_map_read(struct roc_mcs *mcs __plt_unused,
			  struct roc_mcs_rx_sc_sa_map *rx_sc_sa_map __plt_unused)
{
	MCS_SUPPORT_CHECK;

	return -ENOTSUP;
}

int
roc_mcs_tx_sc_sa_map_write(struct roc_mcs *mcs, struct roc_mcs_tx_sc_sa_map *tx_sc_sa_map)
{
	struct mcs_priv *priv = roc_mcs_to_mcs_priv(mcs);
	struct mcs_tx_sc_sa_map *sa_map;
	struct msg_rsp *rsp;
	uint16_t sc_id;
	int i, rc;

	MCS_SUPPORT_CHECK;

	if (tx_sc_sa_map == NULL)
		return -EINVAL;

	sa_map = mbox_alloc_msg_mcs_tx_sc_sa_map_write(mcs->mbox);
	if (sa_map == NULL)
		return -ENOMEM;

	sa_map->sa_index0 = tx_sc_sa_map->sa_index0;
	sa_map->sa_index1 = tx_sc_sa_map->sa_index1;
	sa_map->rekey_ena = tx_sc_sa_map->rekey_ena;
	sa_map->sa_index0_vld = tx_sc_sa_map->sa_index0_vld;
	sa_map->sa_index1_vld = tx_sc_sa_map->sa_index1_vld;
	sa_map->tx_sa_active = tx_sc_sa_map->tx_sa_active;
	sa_map->sectag_sci = tx_sc_sa_map->sectag_sci;
	sa_map->sc_id = tx_sc_sa_map->sc_id;
	sa_map->mcs_id = mcs->idx;

	rc = mbox_process_msg(mcs->mbox, (void *)&rsp);
	if (rc)
		return rc;

	sc_id = tx_sc_sa_map->sc_id;
	for (i = 0; i < MAX_PORTS_PER_MCS; i++) {
		uint32_t set = plt_bitmap_get(priv->port_rsrc[i].sc_bmap, sc_id + priv->sc_entries);

		if (set) {
			uint32_t pos = priv->sa_entries + tx_sc_sa_map->sa_index0;

			plt_bitmap_set(priv->port_rsrc[i].sa_bmap, pos);
			priv->port_rsrc[i].sc_conf[sc_id].tx.sa_idx0 = tx_sc_sa_map->sa_index0;
			pos = priv->sa_entries + tx_sc_sa_map->sa_index1;
			plt_bitmap_set(priv->port_rsrc[i].sa_bmap, pos);
			priv->port_rsrc[i].sc_conf[sc_id].tx.sa_idx1 = tx_sc_sa_map->sa_index1;
			priv->port_rsrc[i].sc_conf[sc_id].tx.sci = tx_sc_sa_map->sectag_sci;
			priv->port_rsrc[i].sc_conf[sc_id].tx.rekey_enb = tx_sc_sa_map->rekey_ena;
			break;
		}
	}

	return 0;
}

int
roc_mcs_tx_sc_sa_map_read(struct roc_mcs *mcs __plt_unused,
			  struct roc_mcs_tx_sc_sa_map *tx_sc_sa_map __plt_unused)
{
	MCS_SUPPORT_CHECK;

	return -ENOTSUP;
}
