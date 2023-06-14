/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#include <cnxk_ethdev.h>
#include <cnxk_ethdev_mcs.h>
#include <roc_mcs.h>

static int
mcs_resource_alloc(struct cnxk_mcs_dev *mcs_dev, enum mcs_direction dir, uint8_t rsrc_id[],
		   uint8_t rsrc_cnt, enum cnxk_mcs_rsrc_type type)
{
	struct roc_mcs_alloc_rsrc_req req = {0};
	struct roc_mcs_alloc_rsrc_rsp rsp;
	int i;

	req.rsrc_type = type;
	req.rsrc_cnt = rsrc_cnt;
	req.dir = dir;

	memset(&rsp, 0, sizeof(struct roc_mcs_alloc_rsrc_rsp));

	if (roc_mcs_rsrc_alloc(mcs_dev->mdev, &req, &rsp)) {
		plt_err("Cannot allocate mcs resource.");
		return -1;
	}

	for (i = 0; i < rsrc_cnt; i++) {
		switch (rsp.rsrc_type) {
		case CNXK_MCS_RSRC_TYPE_FLOWID:
			rsrc_id[i] = rsp.flow_ids[i];
			break;
		case CNXK_MCS_RSRC_TYPE_SECY:
			rsrc_id[i] = rsp.secy_ids[i];
			break;
		case CNXK_MCS_RSRC_TYPE_SC:
			rsrc_id[i] = rsp.sc_ids[i];
			break;
		case CNXK_MCS_RSRC_TYPE_SA:
			rsrc_id[i] = rsp.sa_ids[i];
			break;
		default:
			plt_err("Invalid mcs resource allocated.");
			return -1;
		}
	}
	return 0;
}

int
cnxk_eth_macsec_sa_create(void *device, struct rte_security_macsec_sa *conf)
{
	struct rte_eth_dev *eth_dev = (struct rte_eth_dev *)device;
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	uint8_t salt[RTE_SECURITY_MACSEC_SALT_LEN] = {0};
	struct roc_mcs_pn_table_write_req pn_req = {0};
	uint8_t hash_key_rev[CNXK_MACSEC_HASH_KEY] = {0};
	uint8_t hash_key[CNXK_MACSEC_HASH_KEY] = {0};
	struct cnxk_mcs_dev *mcs_dev = dev->mcs_dev;
	struct roc_mcs_sa_plcy_write_req req;
	uint8_t ciph_key[32] = {0};
	enum mcs_direction dir;
	uint8_t sa_id = 0;
	int i, ret = 0;

	if (!roc_feature_nix_has_macsec())
		return -ENOTSUP;

	dir = (conf->dir == RTE_SECURITY_MACSEC_DIR_TX) ? MCS_TX : MCS_RX;
	ret = mcs_resource_alloc(mcs_dev, dir, &sa_id, 1, CNXK_MCS_RSRC_TYPE_SA);
	if (ret) {
		plt_err("Failed to allocate SA id.");
		return -ENOMEM;
	}
	memset(&req, 0, sizeof(struct roc_mcs_sa_plcy_write_req));
	req.sa_index[0] = sa_id;
	req.sa_cnt = 1;
	req.dir = dir;

	if (conf->key.length != 16 && conf->key.length != 32)
		return -EINVAL;

	for (i = 0; i < conf->key.length; i++)
		ciph_key[i] = conf->key.data[conf->key.length - 1 - i];

	memcpy(&req.plcy[0][0], ciph_key, conf->key.length);

	roc_aes_hash_key_derive(conf->key.data, conf->key.length, hash_key);
	for (i = 0; i < CNXK_MACSEC_HASH_KEY; i++)
		hash_key_rev[i] = hash_key[CNXK_MACSEC_HASH_KEY - 1 - i];

	memcpy(&req.plcy[0][4], hash_key_rev, CNXK_MACSEC_HASH_KEY);

	for (i = 0; i < RTE_SECURITY_MACSEC_SALT_LEN; i++)
		salt[i] = conf->salt[RTE_SECURITY_MACSEC_SALT_LEN - 1 - i];
	memcpy(&req.plcy[0][6], salt, RTE_SECURITY_MACSEC_SALT_LEN);

	req.plcy[0][7] |= (uint64_t)conf->ssci << 32;
	req.plcy[0][8] = (conf->dir == RTE_SECURITY_MACSEC_DIR_TX) ? (conf->an & 0x3) : 0;

	ret = roc_mcs_sa_policy_write(mcs_dev->mdev, &req);
	if (ret) {
		plt_err("Failed to write SA policy.");
		return -EINVAL;
	}
	pn_req.next_pn = ((uint64_t)conf->xpn << 32) | rte_be_to_cpu_32(conf->next_pn);
	pn_req.pn_id = sa_id;
	pn_req.dir = dir;

	ret = roc_mcs_pn_table_write(mcs_dev->mdev, &pn_req);
	if (ret) {
		plt_err("Failed to write PN table.");
		return -EINVAL;
	}

	return sa_id;
}

int
cnxk_eth_macsec_sa_destroy(void *device, uint16_t sa_id, enum rte_security_macsec_direction dir)
{
	struct rte_eth_dev *eth_dev = (struct rte_eth_dev *)device;
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct cnxk_mcs_dev *mcs_dev = dev->mcs_dev;
	struct roc_mcs_clear_stats stats_req = {0};
	struct roc_mcs_free_rsrc_req req = {0};
	int ret = 0;

	if (!roc_feature_nix_has_macsec())
		return -ENOTSUP;

	stats_req.type = CNXK_MCS_RSRC_TYPE_SA;
	stats_req.id = sa_id;
	stats_req.dir = (dir == RTE_SECURITY_MACSEC_DIR_TX) ? MCS_TX : MCS_RX;
	stats_req.all = 0;

	ret = roc_mcs_stats_clear(mcs_dev->mdev, &stats_req);
	if (ret)
		plt_err("Failed to clear stats for SA id %u, dir %u.", sa_id, dir);

	req.rsrc_id = sa_id;
	req.dir = (dir == RTE_SECURITY_MACSEC_DIR_TX) ? MCS_TX : MCS_RX;
	req.rsrc_type = CNXK_MCS_RSRC_TYPE_SA;

	ret = roc_mcs_rsrc_free(mcs_dev->mdev, &req);
	if (ret)
		plt_err("Failed to free SA id %u, dir %u.", sa_id, dir);

	return ret;
}

int
cnxk_eth_macsec_sc_create(void *device, struct rte_security_macsec_sc *conf)
{
	struct rte_eth_dev *eth_dev = (struct rte_eth_dev *)device;
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_mcs_set_pn_threshold pn_thresh = {0};
	struct cnxk_mcs_dev *mcs_dev = dev->mcs_dev;
	enum mcs_direction dir;
	uint8_t sc_id = 0;
	int i, ret = 0;

	if (!roc_feature_nix_has_macsec())
		return -ENOTSUP;

	dir = (conf->dir == RTE_SECURITY_MACSEC_DIR_TX) ? MCS_TX : MCS_RX;
	ret = mcs_resource_alloc(mcs_dev, dir, &sc_id, 1, CNXK_MCS_RSRC_TYPE_SC);
	if (ret) {
		plt_err("Failed to allocate SC id.");
		return -ENOMEM;
	}

	if (conf->dir == RTE_SECURITY_MACSEC_DIR_TX) {
		struct roc_mcs_tx_sc_sa_map req = {0};

		req.sa_index0 = conf->sc_tx.sa_id & 0xFF;
		req.sa_index1 = conf->sc_tx.sa_id_rekey & 0xFF;
		req.rekey_ena = conf->sc_tx.re_key_en;
		req.sa_index0_vld = conf->sc_tx.active;
		req.sa_index1_vld = conf->sc_tx.re_key_en && conf->sc_tx.active;
		req.tx_sa_active = 0;
		req.sectag_sci = conf->sc_tx.sci;
		req.sc_id = sc_id;

		ret = roc_mcs_tx_sc_sa_map_write(mcs_dev->mdev, &req);
		if (ret) {
			plt_err("Failed to map TX SC-SA");
			return -EINVAL;
		}
		pn_thresh.xpn = conf->sc_tx.is_xpn;
	} else {
		for (i = 0; i < RTE_SECURITY_MACSEC_NUM_AN; i++) {
			struct roc_mcs_rx_sc_sa_map req = {0};

			req.sa_index = conf->sc_rx.sa_id[i] & 0x7F;
			req.sc_id = sc_id;
			req.an = i & 0x3;
			req.sa_in_use = 0;
			/* Clearing the sa_in_use bit automatically clears
			 * the corresponding pn_thresh_reached bit
			 */
			ret = roc_mcs_rx_sc_sa_map_write(mcs_dev->mdev, &req);
			if (ret) {
				plt_err("Failed to map RX SC-SA");
				return -EINVAL;
			}
			req.sa_in_use = conf->sc_rx.sa_in_use[i];
			ret = roc_mcs_rx_sc_sa_map_write(mcs_dev->mdev, &req);
			if (ret) {
				plt_err("Failed to map RX SC-SA");
				return -EINVAL;
			}
		}
		pn_thresh.xpn = conf->sc_rx.is_xpn;
	}

	pn_thresh.threshold = conf->pn_threshold;
	pn_thresh.dir = dir;

	ret = roc_mcs_pn_threshold_set(mcs_dev->mdev, &pn_thresh);
	if (ret) {
		plt_err("Failed to write PN threshold.");
		return -EINVAL;
	}

	return sc_id;
}

int
cnxk_eth_macsec_sc_destroy(void *device, uint16_t sc_id, enum rte_security_macsec_direction dir)
{
	struct rte_eth_dev *eth_dev = (struct rte_eth_dev *)device;
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct cnxk_mcs_dev *mcs_dev = dev->mcs_dev;
	struct roc_mcs_clear_stats stats_req = {0};
	struct roc_mcs_free_rsrc_req req = {0};
	int ret = 0;

	if (!roc_feature_nix_has_macsec())
		return -ENOTSUP;

	stats_req.type = CNXK_MCS_RSRC_TYPE_SC;
	stats_req.id = sc_id;
	stats_req.dir = (dir == RTE_SECURITY_MACSEC_DIR_TX) ? MCS_TX : MCS_RX;
	stats_req.all = 0;

	ret = roc_mcs_stats_clear(mcs_dev->mdev, &stats_req);
	if (ret)
		plt_err("Failed to clear stats for SC id %u, dir %u.", sc_id, dir);

	req.rsrc_id = sc_id;
	req.dir = (dir == RTE_SECURITY_MACSEC_DIR_TX) ? MCS_TX : MCS_RX;
	req.rsrc_type = CNXK_MCS_RSRC_TYPE_SC;

	ret = roc_mcs_rsrc_free(mcs_dev->mdev, &req);
	if (ret)
		plt_err("Failed to free SC id.");

	return ret;
}

static int
cnxk_mcs_event_cb(void *userdata, struct roc_mcs_event_desc *desc, void *cb_arg)
{
	struct rte_eth_event_macsec_desc d = {0};

	d.metadata = (uint64_t)userdata;

	switch (desc->type) {
	case ROC_MCS_EVENT_SECTAG_VAL_ERR:
		d.type = RTE_ETH_EVENT_MACSEC_SECTAG_VAL_ERR;
		switch (desc->subtype) {
		case ROC_MCS_EVENT_RX_SECTAG_V_EQ1:
			d.subtype = RTE_ETH_SUBEVENT_MACSEC_RX_SECTAG_V_EQ1;
			break;
		case ROC_MCS_EVENT_RX_SECTAG_E_EQ0_C_EQ1:
			d.subtype = RTE_ETH_SUBEVENT_MACSEC_RX_SECTAG_E_EQ0_C_EQ1;
			break;
		case ROC_MCS_EVENT_RX_SECTAG_SL_GTE48:
			d.subtype = RTE_ETH_SUBEVENT_MACSEC_RX_SECTAG_SL_GTE48;
			break;
		case ROC_MCS_EVENT_RX_SECTAG_ES_EQ1_SC_EQ1:
			d.subtype = RTE_ETH_SUBEVENT_MACSEC_RX_SECTAG_ES_EQ1_SC_EQ1;
			break;
		case ROC_MCS_EVENT_RX_SECTAG_SC_EQ1_SCB_EQ1:
			d.subtype = RTE_ETH_SUBEVENT_MACSEC_RX_SECTAG_SC_EQ1_SCB_EQ1;
			break;
		default:
			plt_err("Unknown MACsec sub event : %d", desc->subtype);
		}
		break;
	case ROC_MCS_EVENT_RX_SA_PN_HARD_EXP:
		d.type = RTE_ETH_EVENT_MACSEC_RX_SA_PN_HARD_EXP;
		break;
	case ROC_MCS_EVENT_RX_SA_PN_SOFT_EXP:
		d.type = RTE_ETH_EVENT_MACSEC_RX_SA_PN_SOFT_EXP;
		break;
	case ROC_MCS_EVENT_TX_SA_PN_HARD_EXP:
		d.type = RTE_ETH_EVENT_MACSEC_TX_SA_PN_HARD_EXP;
		break;
	case ROC_MCS_EVENT_TX_SA_PN_SOFT_EXP:
		d.type = RTE_ETH_EVENT_MACSEC_TX_SA_PN_SOFT_EXP;
		break;
	default:
		plt_err("Unknown MACsec event type: %d", desc->type);
	}

	rte_eth_dev_callback_process(cb_arg, RTE_ETH_EVENT_MACSEC, &d);

	return 0;
}

void
cnxk_mcs_dev_fini(struct cnxk_eth_dev *dev)
{
	struct cnxk_mcs_dev *mcs_dev = dev->mcs_dev;
	int rc;

	rc = roc_mcs_event_cb_unregister(mcs_dev->mdev, ROC_MCS_EVENT_SECTAG_VAL_ERR);
	if (rc)
		plt_err("Failed to unregister MCS event callback: rc: %d", rc);

	rc = roc_mcs_event_cb_unregister(mcs_dev->mdev, ROC_MCS_EVENT_TX_SA_PN_SOFT_EXP);
	if (rc)
		plt_err("Failed to unregister MCS event callback: rc: %d", rc);

	rc = roc_mcs_event_cb_unregister(mcs_dev->mdev, ROC_MCS_EVENT_RX_SA_PN_SOFT_EXP);
	if (rc)
		plt_err("Failed to unregister MCS event callback: rc: %d", rc);

	/* Cleanup MACsec dev */
	roc_mcs_dev_fini(mcs_dev->mdev);

	plt_free(mcs_dev);
}

int
cnxk_mcs_dev_init(struct cnxk_eth_dev *dev, uint8_t mcs_idx)
{
	struct roc_mcs_intr_cfg intr_cfg = {0};
	struct roc_mcs_hw_info hw_info = {0};
	struct cnxk_mcs_dev *mcs_dev;
	int rc;

	rc = roc_mcs_hw_info_get(&hw_info);
	if (rc) {
		plt_err("MCS HW info get failed: rc: %d ", rc);
		return rc;
	}

	mcs_dev = plt_zmalloc(sizeof(struct cnxk_mcs_dev), PLT_CACHE_LINE_SIZE);
	if (!mcs_dev)
		return -ENOMEM;

	mcs_dev->idx = mcs_idx;
	mcs_dev->mdev = roc_mcs_dev_init(mcs_dev->idx);
	if (!mcs_dev->mdev) {
		plt_free(mcs_dev);
		return rc;
	}
	mcs_dev->port_id = dev->eth_dev->data->port_id;

	intr_cfg.intr_mask =
		ROC_MCS_CPM_RX_SECTAG_V_EQ1_INT | ROC_MCS_CPM_RX_SECTAG_E_EQ0_C_EQ1_INT |
		ROC_MCS_CPM_RX_SECTAG_SL_GTE48_INT | ROC_MCS_CPM_RX_SECTAG_ES_EQ1_SC_EQ1_INT |
		ROC_MCS_CPM_RX_SECTAG_SC_EQ1_SCB_EQ1_INT | ROC_MCS_CPM_RX_PACKET_XPN_EQ0_INT |
		ROC_MCS_CPM_RX_PN_THRESH_REACHED_INT | ROC_MCS_CPM_TX_PACKET_XPN_EQ0_INT |
		ROC_MCS_CPM_TX_PN_THRESH_REACHED_INT | ROC_MCS_CPM_TX_SA_NOT_VALID_INT |
		ROC_MCS_BBE_RX_DFIFO_OVERFLOW_INT | ROC_MCS_BBE_RX_PLFIFO_OVERFLOW_INT |
		ROC_MCS_BBE_TX_DFIFO_OVERFLOW_INT | ROC_MCS_BBE_TX_PLFIFO_OVERFLOW_INT |
		ROC_MCS_PAB_RX_CHAN_OVERFLOW_INT | ROC_MCS_PAB_TX_CHAN_OVERFLOW_INT;

	rc = roc_mcs_intr_configure(mcs_dev->mdev, &intr_cfg);
	if (rc) {
		plt_err("Failed to configure MCS interrupts: rc: %d", rc);
		plt_free(mcs_dev);
		return rc;
	}

	rc = roc_mcs_event_cb_register(mcs_dev->mdev, ROC_MCS_EVENT_SECTAG_VAL_ERR,
				       cnxk_mcs_event_cb, dev->eth_dev, mcs_dev);
	if (rc) {
		plt_err("Failed to register MCS event callback: rc: %d", rc);
		plt_free(mcs_dev);
		return rc;
	}
	rc = roc_mcs_event_cb_register(mcs_dev->mdev, ROC_MCS_EVENT_TX_SA_PN_SOFT_EXP,
				       cnxk_mcs_event_cb, dev->eth_dev, mcs_dev);
	if (rc) {
		plt_err("Failed to register MCS event callback: rc: %d", rc);
		plt_free(mcs_dev);
		return rc;
	}
	rc = roc_mcs_event_cb_register(mcs_dev->mdev, ROC_MCS_EVENT_RX_SA_PN_SOFT_EXP,
				       cnxk_mcs_event_cb, dev->eth_dev, mcs_dev);
	if (rc) {
		plt_err("Failed to register MCS event callback: rc: %d", rc);
		plt_free(mcs_dev);
		return rc;
	}
	dev->mcs_dev = mcs_dev;

	return 0;
}
