/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

struct mcs_event_cb {
	TAILQ_ENTRY(mcs_event_cb) next;
	enum roc_mcs_event_type event;
	roc_mcs_dev_cb_fn cb_fn;
	void *cb_arg;
	void *userdata;
	void *ret_param;
	uint32_t active;
};
TAILQ_HEAD(mcs_event_cb_list, mcs_event_cb);

PLT_STATIC_ASSERT(ROC_MCS_MEM_SZ >= (sizeof(struct mcs_priv) + sizeof(struct mcs_event_cb_list)));

int
roc_mcs_hw_info_get(struct roc_mcs_hw_info *hw_info)
{
	struct mcs_hw_info *hw;
	struct npa_lf *npa;
	int rc;

	MCS_SUPPORT_CHECK;

	if (hw_info == NULL)
		return -EINVAL;

	/* Use mbox handler of first probed pci_func for
	 * initial mcs mbox communication.
	 */
	npa = idev_npa_obj_get();
	if (!npa)
		return MCS_ERR_DEVICE_NOT_FOUND;

	mbox_alloc_msg_mcs_get_hw_info(npa->mbox);
	rc = mbox_process_msg(npa->mbox, (void *)&hw);
	if (rc)
		return rc;

	hw_info->num_mcs_blks = hw->num_mcs_blks;
	hw_info->tcam_entries = hw->tcam_entries;
	hw_info->secy_entries = hw->secy_entries;
	hw_info->sc_entries = hw->sc_entries;
	hw_info->sa_entries = hw->sa_entries;

	return rc;
}

int
roc_mcs_active_lmac_set(struct roc_mcs *mcs, struct roc_mcs_set_active_lmac *lmac)
{
	struct mcs_set_active_lmac *req;
	struct msg_rsp *rsp;

	/* Only needed for 105N */
	if (!roc_model_is_cnf10kb())
		return 0;

	if (lmac == NULL)
		return -EINVAL;

	MCS_SUPPORT_CHECK;

	req = mbox_alloc_msg_mcs_set_active_lmac(mcs->mbox);
	if (req == NULL)
		return -ENOMEM;

	req->lmac_bmap = lmac->lmac_bmap;
	req->channel_base = lmac->channel_base;
	req->mcs_id = mcs->idx;

	return mbox_process_msg(mcs->mbox, (void *)&rsp);
}

static int
mcs_port_reset_set(struct roc_mcs *mcs, struct roc_mcs_port_reset_req *port, uint8_t reset)
{
	struct mcs_port_reset_req *req;
	struct msg_rsp *rsp;

	MCS_SUPPORT_CHECK;

	req = mbox_alloc_msg_mcs_port_reset(mcs->mbox);
	if (req == NULL)
		return -ENOMEM;

	req->reset = reset;
	req->lmac_id = port->port_id;
	req->mcs_id = mcs->idx;

	return mbox_process_msg(mcs->mbox, (void *)&rsp);
}

int
roc_mcs_lmac_mode_set(struct roc_mcs *mcs, struct roc_mcs_set_lmac_mode *port)
{
	struct mcs_set_lmac_mode *req;
	struct msg_rsp *rsp;

	if (port == NULL)
		return -EINVAL;

	MCS_SUPPORT_CHECK;

	req = mbox_alloc_msg_mcs_set_lmac_mode(mcs->mbox);
	if (req == NULL)
		return -ENOMEM;

	req->lmac_id = port->lmac_id;
	req->mcs_id = mcs->idx;
	req->mode = port->mode;

	return mbox_process_msg(mcs->mbox, (void *)&rsp);
}

int
roc_mcs_pn_threshold_set(struct roc_mcs *mcs, struct roc_mcs_set_pn_threshold *pn)
{
	struct mcs_set_pn_threshold *req;
	struct msg_rsp *rsp;

	if (pn == NULL)
		return -EINVAL;

	MCS_SUPPORT_CHECK;

	req = mbox_alloc_msg_mcs_set_pn_threshold(mcs->mbox);
	if (req == NULL)
		return -ENOMEM;

	req->threshold = pn->threshold;
	req->mcs_id = mcs->idx;
	req->dir = pn->dir;
	req->xpn = pn->xpn;

	return mbox_process_msg(mcs->mbox, (void *)&rsp);
}

int
roc_mcs_ctrl_pkt_rule_alloc(struct roc_mcs *mcs, struct roc_mcs_alloc_ctrl_pkt_rule_req *req,
			    struct roc_mcs_alloc_ctrl_pkt_rule_rsp *rsp)
{
	struct mcs_alloc_ctrl_pkt_rule_req *rule_req;
	struct mcs_alloc_ctrl_pkt_rule_rsp *rule_rsp;
	int rc;

	MCS_SUPPORT_CHECK;

	if (req == NULL || rsp == NULL)
		return -EINVAL;

	rule_req = mbox_alloc_msg_mcs_alloc_ctrl_pkt_rule(mcs->mbox);
	if (rule_req == NULL)
		return -ENOMEM;

	rule_req->rule_type = req->rule_type;
	rule_req->mcs_id = mcs->idx;
	rule_req->dir = req->dir;

	rc = mbox_process_msg(mcs->mbox, (void *)&rule_rsp);
	if (rc)
		return rc;

	rsp->rule_type = rule_rsp->rule_type;
	rsp->rule_idx = rule_rsp->rule_idx;
	rsp->dir = rule_rsp->dir;

	return 0;
}

int
roc_mcs_ctrl_pkt_rule_free(struct roc_mcs *mcs, struct roc_mcs_free_ctrl_pkt_rule_req *req)
{
	struct mcs_free_ctrl_pkt_rule_req *rule_req;
	struct msg_rsp *rsp;

	MCS_SUPPORT_CHECK;

	if (req == NULL)
		return -EINVAL;

	rule_req = mbox_alloc_msg_mcs_free_ctrl_pkt_rule(mcs->mbox);
	if (rule_req == NULL)
		return -ENOMEM;

	rule_req->rule_type = req->rule_type;
	rule_req->rule_idx = req->rule_idx;
	rule_req->mcs_id = mcs->idx;
	rule_req->dir = req->dir;
	rule_req->all = req->all;

	return mbox_process_msg(mcs->mbox, (void *)&rsp);
}

int
roc_mcs_ctrl_pkt_rule_write(struct roc_mcs *mcs, struct roc_mcs_ctrl_pkt_rule_write_req *req)
{
	struct mcs_ctrl_pkt_rule_write_req *rule_req;
	struct msg_rsp *rsp;

	MCS_SUPPORT_CHECK;

	if (req == NULL)
		return -EINVAL;

	rule_req = mbox_alloc_msg_mcs_ctrl_pkt_rule_write(mcs->mbox);
	if (rule_req == NULL)
		return -ENOMEM;

	rule_req->rule_type = req->rule_type;
	rule_req->rule_idx = req->rule_idx;
	rule_req->mcs_id = mcs->idx;
	rule_req->dir = req->dir;
	rule_req->data0 = req->data0;
	rule_req->data1 = req->data1;
	rule_req->data2 = req->data2;

	return mbox_process_msg(mcs->mbox, (void *)&rsp);
}

int
roc_mcs_port_cfg_set(struct roc_mcs *mcs, struct roc_mcs_port_cfg_set_req *req)
{
	struct mcs_port_cfg_set_req *set_req;
	struct msg_rsp *rsp;

	MCS_SUPPORT_CHECK;

	if (req == NULL)
		return -EINVAL;

	set_req = mbox_alloc_msg_mcs_port_cfg_set(mcs->mbox);
	if (set_req == NULL)
		return -ENOMEM;

	set_req->cstm_tag_rel_mode_sel = req->cstm_tag_rel_mode_sel;
	set_req->custom_hdr_enb = req->custom_hdr_enb;
	set_req->fifo_skid = req->fifo_skid;
	set_req->lmac_mode = req->port_mode;
	set_req->lmac_id = req->port_id;
	set_req->mcs_id = mcs->idx;

	return mbox_process_msg(mcs->mbox, (void *)&rsp);
}

int
roc_mcs_port_cfg_get(struct roc_mcs *mcs, struct roc_mcs_port_cfg_get_req *req,
		     struct roc_mcs_port_cfg_get_rsp *rsp)
{
	struct mcs_port_cfg_get_req *get_req;
	struct mcs_port_cfg_get_rsp *get_rsp;
	int rc;

	MCS_SUPPORT_CHECK;

	if (req == NULL)
		return -EINVAL;

	get_req = mbox_alloc_msg_mcs_port_cfg_get(mcs->mbox);
	if (get_req == NULL)
		return -ENOMEM;

	get_req->lmac_id = req->port_id;
	get_req->mcs_id = mcs->idx;

	rc = mbox_process_msg(mcs->mbox, (void *)&get_rsp);
	if (rc)
		return rc;

	rsp->cstm_tag_rel_mode_sel = get_rsp->cstm_tag_rel_mode_sel;
	rsp->custom_hdr_enb = get_rsp->custom_hdr_enb;
	rsp->fifo_skid = get_rsp->fifo_skid;
	rsp->port_mode = get_rsp->lmac_mode;
	rsp->port_id = get_rsp->lmac_id;

	return 0;
}

int
roc_mcs_custom_tag_cfg_get(struct roc_mcs *mcs, struct roc_mcs_custom_tag_cfg_get_req *req,
			   struct roc_mcs_custom_tag_cfg_get_rsp *rsp)
{
	struct mcs_custom_tag_cfg_get_req *get_req;
	struct mcs_custom_tag_cfg_get_rsp *get_rsp;
	int i, rc;

	MCS_SUPPORT_CHECK;

	if (req == NULL)
		return -EINVAL;

	get_req = mbox_alloc_msg_mcs_custom_tag_cfg_get(mcs->mbox);
	if (get_req == NULL)
		return -ENOMEM;

	get_req->dir = req->dir;
	get_req->mcs_id = mcs->idx;

	rc = mbox_process_msg(mcs->mbox, (void *)&get_rsp);
	if (rc)
		return rc;

	for (i = 0; i < 8; i++) {
		rsp->cstm_etype[i] = get_rsp->cstm_etype[i];
		rsp->cstm_indx[i] = get_rsp->cstm_indx[i];
	}

	rsp->cstm_etype_en = get_rsp->cstm_etype_en;
	rsp->dir = get_rsp->dir;

	return 0;
}

int
roc_mcs_intr_configure(struct roc_mcs *mcs, struct roc_mcs_intr_cfg *config)
{
	struct mcs_intr_cfg *req;
	struct msg_rsp *rsp;
	int rc;

	if (config == NULL)
		return -EINVAL;

	MCS_SUPPORT_CHECK;

	if (mcs->intr_cfg_once)
		return 0;

	req = mbox_alloc_msg_mcs_intr_cfg(mcs->mbox);
	if (req == NULL)
		return -ENOMEM;

	req->intr_mask = config->intr_mask;
	req->mcs_id = mcs->idx;

	rc = mbox_process_msg(mcs->mbox, (void *)&rsp);
	if (rc == 0)
		mcs->intr_cfg_once = true;

	return rc;
}

int
roc_mcs_port_recovery(struct roc_mcs *mcs, union roc_mcs_event_data *mdata, uint8_t port_id)
{
	struct mcs_priv *priv = roc_mcs_to_mcs_priv(mcs);
	struct roc_mcs_pn_table_write_req pn_table = {0};
	struct roc_mcs_rx_sc_sa_map rx_map = {0};
	struct roc_mcs_tx_sc_sa_map tx_map = {0};
	struct roc_mcs_port_reset_req port = {0};
	struct roc_mcs_clear_stats stats = {0};
	int tx_cnt = 0, rx_cnt = 0, rc = 0;
	uint64_t set;
	int i;

	port.port_id = port_id;
	rc = mcs_port_reset_set(mcs, &port, 1);

	/* Reset TX/RX PN tables */
	for (i = 0; i < (priv->sa_entries << 1); i++) {
		set = plt_bitmap_get(priv->port_rsrc[port_id].sa_bmap, i);
		if (set) {
			pn_table.pn_id = i;
			pn_table.next_pn = 1;
			pn_table.dir = MCS_RX;
			if (i >= priv->sa_entries) {
				pn_table.dir = MCS_TX;
				pn_table.pn_id -= priv->sa_entries;
			}
			rc = roc_mcs_pn_table_write(mcs, &pn_table);
			if (rc)
				return rc;

			if (i >= priv->sa_entries)
				tx_cnt++;
			else
				rx_cnt++;
		}
	}

	if (tx_cnt || rx_cnt) {
		mdata->tx_sa_array = plt_zmalloc(tx_cnt * sizeof(uint16_t), 0);
		if (tx_cnt && (mdata->tx_sa_array == NULL)) {
			rc = -ENOMEM;
			goto exit;
		}
		mdata->rx_sa_array = plt_zmalloc(rx_cnt * sizeof(uint16_t), 0);
		if (rx_cnt && (mdata->rx_sa_array == NULL)) {
			rc = -ENOMEM;
			goto exit;
		}

		mdata->num_tx_sa = tx_cnt;
		mdata->num_rx_sa = rx_cnt;
		for (i = 0; i < (priv->sa_entries << 1); i++) {
			set = plt_bitmap_get(priv->port_rsrc[port_id].sa_bmap, i);
			if (set) {
				if (i >= priv->sa_entries)
					mdata->tx_sa_array[--tx_cnt] = i - priv->sa_entries;
				else
					mdata->rx_sa_array[--rx_cnt] = i;
			}
		}
	}
	tx_cnt = 0;
	rx_cnt = 0;

	/* Reset Tx active SA to index:0 */
	for (i = priv->sc_entries; i < (priv->sc_entries << 1); i++) {
		set = plt_bitmap_get(priv->port_rsrc[port_id].sc_bmap, i);
		if (set) {
			uint16_t sc_id = i - priv->sc_entries;

			tx_map.sa_index0 = priv->port_rsrc[port_id].sc_conf[sc_id].tx.sa_idx0;
			tx_map.sa_index1 = priv->port_rsrc[port_id].sc_conf[sc_id].tx.sa_idx1;
			tx_map.rekey_ena = priv->port_rsrc[port_id].sc_conf[sc_id].tx.rekey_enb;
			tx_map.sectag_sci = priv->port_rsrc[port_id].sc_conf[sc_id].tx.sci;
			tx_map.sa_index0_vld = 1;
			tx_map.sa_index1_vld = 0;
			tx_map.tx_sa_active = 0;
			tx_map.sc_id = sc_id;
			rc = roc_mcs_tx_sc_sa_map_write(mcs, &tx_map);
			if (rc)
				return rc;

			tx_cnt++;
		}
	}

	if (tx_cnt) {
		mdata->tx_sc_array = plt_zmalloc(tx_cnt * sizeof(uint16_t), 0);
		if (tx_cnt && (mdata->tx_sc_array == NULL)) {
			rc = -ENOMEM;
			goto exit;
		}

		mdata->num_tx_sc = tx_cnt;
		for (i = priv->sc_entries; i < (priv->sc_entries << 1); i++) {
			set = plt_bitmap_get(priv->port_rsrc[port_id].sc_bmap, i);
			if (set)
				mdata->tx_sc_array[--tx_cnt] = i - priv->sc_entries;
		}
	}

	/* Clear SA_IN_USE for active ANs in RX CPM */
	for (i = 0; i < priv->sc_entries; i++) {
		set = plt_bitmap_get(priv->port_rsrc[port_id].sc_bmap, i);
		if (set) {
			rx_map.sa_index = priv->port_rsrc[port_id].sc_conf[i].rx.sa_idx;
			rx_map.an = priv->port_rsrc[port_id].sc_conf[i].rx.an;
			rx_map.sa_in_use = 0;
			rx_map.sc_id = i;
			rc = roc_mcs_rx_sc_sa_map_write(mcs, &rx_map);
			if (rc)
				return rc;

			rx_cnt++;
		}
	}

	/* Reset flow(flow/secy/sc/sa) stats mapped to this PORT */
	for (i = 0; i < (priv->tcam_entries << 1); i++) {
		set = plt_bitmap_get(priv->port_rsrc[port_id].tcam_bmap, i);
		if (set) {
			stats.type = MCS_FLOWID_STATS;
			stats.id = i;
			stats.dir = MCS_RX;
			if (i >= priv->sa_entries) {
				stats.dir = MCS_TX;
				stats.id -= priv->tcam_entries;
			}
			rc = roc_mcs_stats_clear(mcs, &stats);
			if (rc)
				return rc;
		}
	}
	for (i = 0; i < (priv->secy_entries << 1); i++) {
		set = plt_bitmap_get(priv->port_rsrc[port_id].secy_bmap, i);
		if (set) {
			stats.type = MCS_SECY_STATS;
			stats.id = i;
			stats.dir = MCS_RX;
			if (i >= priv->sa_entries) {
				stats.dir = MCS_TX;
				stats.id -= priv->secy_entries;
			}
			rc = roc_mcs_stats_clear(mcs, &stats);
			if (rc)
				return rc;
		}
	}
	for (i = 0; i < (priv->sc_entries << 1); i++) {
		set = plt_bitmap_get(priv->port_rsrc[port_id].sc_bmap, i);
		if (set) {
			stats.type = MCS_SC_STATS;
			stats.id = i;
			stats.dir = MCS_RX;
			if (i >= priv->sa_entries) {
				stats.dir = MCS_TX;
				stats.id -= priv->sc_entries;
			}
			rc = roc_mcs_stats_clear(mcs, &stats);
			if (rc)
				return rc;
		}
	}
	if (roc_model_is_cn10kb_a0()) {
		for (i = 0; i < (priv->sa_entries << 1); i++) {
			set = plt_bitmap_get(priv->port_rsrc[port_id].sa_bmap, i);
			if (set) {
				stats.type = MCS_SA_STATS;
				stats.id = i;
				stats.dir = MCS_RX;
				if (i >= priv->sa_entries) {
					stats.dir = MCS_TX;
					stats.id -= priv->sa_entries;
				}
				rc = roc_mcs_stats_clear(mcs, &stats);
				if (rc)
					return rc;
			}
		}
	}
	{
		stats.type = MCS_PORT_STATS;
		stats.id = port_id;
		rc = roc_mcs_stats_clear(mcs, &stats);
		if (rc)
			return rc;
	}

	if (rx_cnt) {
		mdata->rx_sc_array = plt_zmalloc(rx_cnt * sizeof(uint16_t), 0);
		if (mdata->rx_sc_array == NULL) {
			rc = -ENOMEM;
			goto exit;
		}
		mdata->sc_an_array = plt_zmalloc(rx_cnt * sizeof(uint8_t), 0);
		if (mdata->sc_an_array == NULL) {
			rc = -ENOMEM;
			goto exit;
		}

		mdata->num_rx_sc = rx_cnt;
	}

	/* Reactivate in-use ANs for active SCs in RX CPM */
	for (i = 0; i < priv->sc_entries; i++) {
		set = plt_bitmap_get(priv->port_rsrc[port_id].sc_bmap, i);
		if (set) {
			rx_map.sa_index = priv->port_rsrc[port_id].sc_conf[i].rx.sa_idx;
			rx_map.an = priv->port_rsrc[port_id].sc_conf[i].rx.an;
			rx_map.sa_in_use = 1;
			rx_map.sc_id = i;
			rc = roc_mcs_rx_sc_sa_map_write(mcs, &rx_map);
			if (rc)
				return rc;

			mdata->rx_sc_array[--rx_cnt] = i;
			mdata->sc_an_array[rx_cnt] = priv->port_rsrc[port_id].sc_conf[i].rx.an;
		}
	}

	port.port_id = port_id;
	rc = mcs_port_reset_set(mcs, &port, 0);

	return rc;
exit:
	if (mdata->num_tx_sa)
		plt_free(mdata->tx_sa_array);
	if (mdata->num_rx_sa)
		plt_free(mdata->rx_sa_array);
	if (mdata->num_tx_sc)
		plt_free(mdata->tx_sc_array);
	if (mdata->num_rx_sc) {
		plt_free(mdata->rx_sc_array);
		plt_free(mdata->sc_an_array);
	}
	return rc;
}

int
roc_mcs_port_reset(struct roc_mcs *mcs, struct roc_mcs_port_reset_req *port)
{
	struct roc_mcs_event_desc desc = {0};
	int rc;

	/* Initiate port reset and software recovery */
	rc = roc_mcs_port_recovery(mcs, &desc.metadata, port->port_id);
	if (rc)
		goto exit;

	desc.type = ROC_MCS_EVENT_PORT_RESET_RECOVERY;
	/* Notify the entity details to the application which are recovered */
	mcs_event_cb_process(mcs, &desc);

exit:
	if (desc.metadata.num_tx_sa)
		plt_free(desc.metadata.tx_sa_array);
	if (desc.metadata.num_rx_sa)
		plt_free(desc.metadata.rx_sa_array);
	if (desc.metadata.num_tx_sc)
		plt_free(desc.metadata.tx_sc_array);
	if (desc.metadata.num_rx_sc) {
		plt_free(desc.metadata.rx_sc_array);
		plt_free(desc.metadata.sc_an_array);
	}

	return rc;
}

int
roc_mcs_event_cb_register(struct roc_mcs *mcs, enum roc_mcs_event_type event,
			  roc_mcs_dev_cb_fn cb_fn, void *cb_arg, void *userdata)
{
	struct mcs_event_cb_list *cb_list = (struct mcs_event_cb_list *)roc_mcs_to_mcs_cb_list(mcs);
	struct mcs_event_cb *cb;

	if (cb_fn == NULL || cb_arg == NULL || userdata == NULL)
		return -EINVAL;

	MCS_SUPPORT_CHECK;

	TAILQ_FOREACH(cb, cb_list, next) {
		if (cb->cb_fn == cb_fn && cb->cb_arg == cb_arg && cb->event == event)
			break;
	}

	if (cb == NULL) {
		cb = plt_zmalloc(sizeof(struct mcs_event_cb), 0);
		if (!cb)
			return -ENOMEM;

		cb->cb_fn = cb_fn;
		cb->cb_arg = cb_arg;
		cb->event = event;
		cb->userdata = userdata;
		TAILQ_INSERT_TAIL(cb_list, cb, next);
	}

	return 0;
}

int
roc_mcs_event_cb_unregister(struct roc_mcs *mcs, enum roc_mcs_event_type event)
{
	struct mcs_event_cb_list *cb_list = (struct mcs_event_cb_list *)roc_mcs_to_mcs_cb_list(mcs);
	struct mcs_event_cb *cb, *next;

	MCS_SUPPORT_CHECK;

	for (cb = TAILQ_FIRST(cb_list); cb != NULL; cb = next) {
		next = TAILQ_NEXT(cb, next);

		if (cb->event != event)
			continue;

		if (cb->active == 0) {
			TAILQ_REMOVE(cb_list, cb, next);
			plt_free(cb);
		} else {
			return -EAGAIN;
		}
	}

	return 0;
}

int
mcs_event_cb_process(struct roc_mcs *mcs, struct roc_mcs_event_desc *desc)
{
	struct mcs_event_cb_list *cb_list = (struct mcs_event_cb_list *)roc_mcs_to_mcs_cb_list(mcs);
	struct mcs_event_cb mcs_cb;
	struct mcs_event_cb *cb;
	int rc = 0;

	TAILQ_FOREACH(cb, cb_list, next) {
		if (cb->cb_fn == NULL || cb->event != desc->type)
			continue;

		mcs_cb = *cb;
		cb->active = 1;
		mcs_cb.ret_param = desc;

		rc = mcs_cb.cb_fn(mcs_cb.userdata, mcs_cb.ret_param, mcs_cb.cb_arg,
				  mcs->sa_port_map[desc->metadata.sa_idx]);
		cb->active = 0;
	}

	return rc;
}

static int
mcs_alloc_bmap(uint16_t entries, void **mem, struct plt_bitmap **bmap)
{
	size_t bmap_sz;
	int rc = 0;

	bmap_sz = plt_bitmap_get_memory_footprint(entries);
	*mem = plt_zmalloc(bmap_sz, PLT_CACHE_LINE_SIZE);
	if (*mem == NULL)
		rc = -ENOMEM;

	*bmap = plt_bitmap_init(entries, *mem, bmap_sz);
	if (!*bmap) {
		plt_free(*mem);
		*mem = NULL;
		rc = -ENOMEM;
	}

	return rc;
}

static void
rsrc_bmap_free(struct mcs_rsrc *rsrc)
{
	plt_bitmap_free(rsrc->tcam_bmap);
	plt_free(rsrc->tcam_bmap_mem);
	plt_bitmap_free(rsrc->secy_bmap);
	plt_free(rsrc->secy_bmap_mem);
	plt_bitmap_free(rsrc->sc_bmap);
	plt_free(rsrc->sc_bmap_mem);
	plt_bitmap_free(rsrc->sa_bmap);
	plt_free(rsrc->sa_bmap_mem);
}

static int
rsrc_bmap_alloc(struct mcs_priv *priv, struct mcs_rsrc *rsrc)
{
	int rc;

	rc = mcs_alloc_bmap(priv->tcam_entries << 1, &rsrc->tcam_bmap_mem, &rsrc->tcam_bmap);
	if (rc)
		goto exit;

	rc = mcs_alloc_bmap(priv->secy_entries << 1, &rsrc->secy_bmap_mem, &rsrc->secy_bmap);
	if (rc)
		goto exit;

	rc = mcs_alloc_bmap(priv->sc_entries << 1, &rsrc->sc_bmap_mem, &rsrc->sc_bmap);
	if (rc)
		goto exit;

	rc = mcs_alloc_bmap(priv->sa_entries << 1, &rsrc->sa_bmap_mem, &rsrc->sa_bmap);
	if (rc)
		goto exit;

	return rc;
exit:
	rsrc_bmap_free(rsrc);

	return rc;
}

static int
mcs_alloc_rsrc_bmap(struct roc_mcs *mcs)
{
	struct mcs_priv *priv = roc_mcs_to_mcs_priv(mcs);
	struct mcs_hw_info *hw;
	int i, rc;

	mbox_alloc_msg_mcs_get_hw_info(mcs->mbox);
	rc = mbox_process_msg(mcs->mbox, (void *)&hw);
	if (rc)
		return rc;

	priv->num_mcs_blks = hw->num_mcs_blks;
	priv->tcam_entries = hw->tcam_entries;
	priv->secy_entries = hw->secy_entries;
	priv->sc_entries = hw->sc_entries;
	priv->sa_entries = hw->sa_entries;

	rc = rsrc_bmap_alloc(priv, &priv->dev_rsrc);
	if (rc)
		return rc;

	priv->port_rsrc = plt_zmalloc(sizeof(struct mcs_rsrc) * 4, 0);
	if (priv->port_rsrc == NULL) {
		rsrc_bmap_free(&priv->dev_rsrc);
		return -ENOMEM;
	}

	for (i = 0; i < MAX_PORTS_PER_MCS; i++) {
		rc = rsrc_bmap_alloc(priv, &priv->port_rsrc[i]);
		if (rc)
			goto exit;

		priv->port_rsrc[i].sc_conf =
			plt_zmalloc(priv->sc_entries * sizeof(struct mcs_sc_conf), 0);
		if (priv->port_rsrc[i].sc_conf == NULL) {
			rsrc_bmap_free(&priv->port_rsrc[i]);
			goto exit;
		}
	}

	mcs->sa_port_map = plt_zmalloc(sizeof(uint8_t) * hw->sa_entries, 0);
	if (mcs->sa_port_map == NULL)
		goto exit;

	return rc;

exit:
	while (i--) {
		rsrc_bmap_free(&priv->port_rsrc[i]);
		plt_free(priv->port_rsrc[i].sc_conf);
	}
	plt_free(priv->port_rsrc);

	return -ENOMEM;
}

struct roc_mcs *
roc_mcs_dev_init(uint8_t mcs_idx)
{
	struct mcs_event_cb_list *cb_list;
	struct roc_mcs *mcs;
	struct npa_lf *npa;

	if (!(roc_feature_bphy_has_macsec() || roc_feature_nix_has_macsec()))
		return NULL;

	mcs = roc_idev_mcs_get(mcs_idx);
	if (mcs) {
		plt_info("Skipping device, mcs device already probed");
		mcs->refcount++;
		return mcs;
	}

	mcs = plt_zmalloc(sizeof(struct roc_mcs), PLT_CACHE_LINE_SIZE);
	if (!mcs)
		return NULL;

	npa = idev_npa_obj_get();
	if (!npa)
		goto exit;

	mcs->mbox = npa->mbox;
	mcs->idx = mcs_idx;

	/* Add any per mcsv initialization */
	if (mcs_alloc_rsrc_bmap(mcs))
		goto exit;

	cb_list = (struct mcs_event_cb_list *)roc_mcs_to_mcs_cb_list(mcs);
	TAILQ_INIT(cb_list);

	roc_idev_mcs_set(mcs);
	mcs->refcount++;

	return mcs;
exit:
	plt_free(mcs);
	return NULL;
}

void
roc_mcs_dev_fini(struct roc_mcs *mcs)
{
	struct mcs_priv *priv;
	int i;

	mcs->refcount--;
	if (mcs->refcount > 0)
		return;

	priv = roc_mcs_to_mcs_priv(mcs);

	rsrc_bmap_free(&priv->dev_rsrc);

	for (i = 0; i < MAX_PORTS_PER_MCS; i++) {
		rsrc_bmap_free(&priv->port_rsrc[i]);
		plt_free(priv->port_rsrc[i].sc_conf);
	}

	plt_free(priv->port_rsrc);

	plt_free(mcs->sa_port_map);

	roc_idev_mcs_free(mcs);

	plt_free(mcs);
}
