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

struct cnxk_macsec_sess *
cnxk_eth_macsec_sess_get_by_sess(struct cnxk_eth_dev *dev, const struct rte_security_session *sess)
{
	struct cnxk_macsec_sess *macsec_sess = NULL;

	TAILQ_FOREACH(macsec_sess, &dev->mcs_list, entry) {
		if (macsec_sess->sess == sess)
			return macsec_sess;
	}

	return NULL;
}

int
cnxk_eth_macsec_session_create(struct cnxk_eth_dev *dev, struct rte_security_session_conf *conf,
			       struct rte_security_session *sess)
{
	struct cnxk_macsec_sess *macsec_sess_priv = SECURITY_GET_SESS_PRIV(sess);
	struct rte_security_macsec_xform *xform = &conf->macsec;
	struct cnxk_mcs_dev *mcs_dev = dev->mcs_dev;
	struct roc_mcs_secy_plcy_write_req req;
	enum mcs_direction dir;
	uint8_t secy_id = 0;
	uint8_t sectag_tci = 0;
	int ret = 0;

	if (!roc_feature_nix_has_macsec())
		return -ENOTSUP;

	dir = (xform->dir == RTE_SECURITY_MACSEC_DIR_TX) ? MCS_TX : MCS_RX;
	ret = mcs_resource_alloc(mcs_dev, dir, &secy_id, 1, CNXK_MCS_RSRC_TYPE_SECY);
	if (ret) {
		plt_err("Failed to allocate SECY id.");
		return -ENOMEM;
	}

	req.secy_id = secy_id;
	req.dir = dir;
	req.plcy = 0L;

	if (xform->dir == RTE_SECURITY_MACSEC_DIR_TX) {
		sectag_tci = ((uint8_t)xform->tx_secy.sectag_version << 5) |
			     ((uint8_t)xform->tx_secy.end_station << 4) |
			     ((uint8_t)xform->tx_secy.send_sci << 3) |
			     ((uint8_t)xform->tx_secy.scb << 2) |
			     ((uint8_t)xform->tx_secy.encrypt << 1) |
			     (uint8_t)xform->tx_secy.encrypt;
		req.plcy = (((uint64_t)xform->tx_secy.mtu & 0xFFFF) << 28) |
			   (((uint64_t)sectag_tci & 0x3F) << 22) |
			   (((uint64_t)xform->tx_secy.sectag_off & 0x7F) << 15) |
			   ((uint64_t)xform->tx_secy.sectag_insert_mode << 14) |
			   ((uint64_t)xform->tx_secy.icv_include_da_sa << 13) |
			   (((uint64_t)xform->cipher_off & 0x7F) << 6) |
			   ((uint64_t)xform->alg << 2) |
			   ((uint64_t)xform->tx_secy.protect_frames << 1) |
			   (uint64_t)xform->tx_secy.ctrl_port_enable;
	} else {
		req.plcy = ((uint64_t)xform->rx_secy.replay_win_sz << 18) |
			   ((uint64_t)xform->rx_secy.replay_protect << 17) |
			   ((uint64_t)xform->rx_secy.icv_include_da_sa << 16) |
			   (((uint64_t)xform->cipher_off & 0x7F) << 9) |
			   ((uint64_t)xform->alg << 5) |
			   ((uint64_t)xform->rx_secy.preserve_sectag << 4) |
			   ((uint64_t)xform->rx_secy.preserve_icv << 3) |
			   ((uint64_t)xform->rx_secy.validate_frames << 1) |
			   (uint64_t)xform->rx_secy.ctrl_port_enable;
	}

	ret = roc_mcs_secy_policy_write(mcs_dev->mdev, &req);
	if (ret) {
		plt_err(" Failed to configure Tx SECY");
		return -EINVAL;
	}

	if (xform->dir == RTE_SECURITY_MACSEC_DIR_RX) {
		struct roc_mcs_rx_sc_cam_write_req rx_sc_cam = {0};

		rx_sc_cam.sci = xform->sci;
		rx_sc_cam.secy_id = secy_id & 0x3F;
		rx_sc_cam.sc_id = xform->sc_id;
		ret = roc_mcs_rx_sc_cam_write(mcs_dev->mdev, &rx_sc_cam);
		if (ret) {
			plt_err(" Failed to write rx_sc_cam");
			return -EINVAL;
		}
	}
	macsec_sess_priv->sci = xform->sci;
	macsec_sess_priv->sc_id = xform->sc_id;
	macsec_sess_priv->secy_id = secy_id;
	macsec_sess_priv->dir = dir;
	macsec_sess_priv->sess = sess;

	TAILQ_INSERT_TAIL(&dev->mcs_list, macsec_sess_priv, entry);

	return 0;
}

int
cnxk_eth_macsec_session_destroy(struct cnxk_eth_dev *dev, struct rte_security_session *sess)
{
	struct cnxk_mcs_dev *mcs_dev = dev->mcs_dev;
	struct roc_mcs_clear_stats stats_req = {0};
	struct roc_mcs_free_rsrc_req req = {0};
	struct cnxk_macsec_sess *s;
	int ret = 0;

	if (!roc_feature_nix_has_macsec())
		return -ENOTSUP;

	s = SECURITY_GET_SESS_PRIV(sess);

	stats_req.type = CNXK_MCS_RSRC_TYPE_SECY;
	stats_req.id = s->secy_id;
	stats_req.dir = s->dir;
	stats_req.all = 0;

	ret = roc_mcs_stats_clear(mcs_dev->mdev, &stats_req);
	if (ret)
		plt_err("Failed to clear stats for SECY id %u, dir %u.", s->secy_id, s->dir);

	req.rsrc_id = s->secy_id;
	req.dir = s->dir;
	req.rsrc_type = CNXK_MCS_RSRC_TYPE_SECY;

	ret = roc_mcs_rsrc_free(mcs_dev->mdev, &req);
	if (ret)
		plt_err("Failed to free SC id.");

	TAILQ_REMOVE(&dev->mcs_list, s, entry);

	return ret;
}

int
cnxk_mcs_flow_configure(struct rte_eth_dev *eth_dev, const struct rte_flow_attr *attr __rte_unused,
			 const struct rte_flow_item pattern[],
			 const struct rte_flow_action actions[],
			 struct rte_flow_error *error __rte_unused, void **mcs_flow)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	const struct rte_flow_item_eth *eth_item = NULL;
	struct cnxk_mcs_dev *mcs_dev = dev->mcs_dev;
	struct roc_mcs_flowid_entry_write_req req;
	struct cnxk_mcs_flow_opts opts = {0};
	struct cnxk_macsec_sess *sess;
	struct rte_ether_addr src;
	struct rte_ether_addr dst;
	int ret;
	int i = 0;

	if (!roc_feature_nix_has_macsec())
		return -ENOTSUP;

	sess = cnxk_eth_macsec_sess_get_by_sess(dev,
			(const struct rte_security_session *)actions->conf);
	if (sess == NULL)
		return -EINVAL;

	ret = mcs_resource_alloc(mcs_dev, sess->dir, &sess->flow_id, 1,
				 CNXK_MCS_RSRC_TYPE_FLOWID);
	if (ret) {
		plt_err("Failed to allocate FLow id.");
		return -ENOMEM;
	}
	memset(&req, 0, sizeof(struct roc_mcs_flowid_entry_write_req));
	req.sci = sess->sci;
	req.flow_id = sess->flow_id;
	req.secy_id = sess->secy_id;
	req.sc_id = sess->sc_id;
	req.ena = 1;
	req.ctr_pkt = 0;
	req.dir = sess->dir;

	while (pattern[i].type != RTE_FLOW_ITEM_TYPE_END) {
		if (pattern[i].type == RTE_FLOW_ITEM_TYPE_ETH)
			eth_item = pattern[i].spec;
		else
			plt_err("Unhandled flow item : %d", pattern[i].type);
		i++;
	}
	if (eth_item) {
		dst = eth_item->hdr.dst_addr;
		src = eth_item->hdr.src_addr;

		/* Find ways to fill opts */

		req.data[0] =
			(uint64_t)dst.addr_bytes[0] << 40 | (uint64_t)dst.addr_bytes[1] << 32 |
			(uint64_t)dst.addr_bytes[2] << 24 | (uint64_t)dst.addr_bytes[3] << 16 |
			(uint64_t)dst.addr_bytes[4] << 8 | (uint64_t)dst.addr_bytes[5] |
			(uint64_t)src.addr_bytes[5] << 48 | (uint64_t)src.addr_bytes[4] << 56;
		req.data[1] = (uint64_t)src.addr_bytes[3] | (uint64_t)src.addr_bytes[2] << 8 |
			      (uint64_t)src.addr_bytes[1] << 16 |
			      (uint64_t)src.addr_bytes[0] << 24 |
			      (uint64_t)eth_item->hdr.ether_type << 32 |
			      ((uint64_t)opts.outer_tag_id & 0xFFFF) << 48;
		req.data[2] = ((uint64_t)opts.outer_tag_id & 0xF0000) |
			      ((uint64_t)opts.outer_priority & 0xF) << 4 |
			      ((uint64_t)opts.second_outer_tag_id & 0xFFFFF) << 8 |
			      ((uint64_t)opts.second_outer_priority & 0xF) << 28 |
			      ((uint64_t)opts.bonus_data << 32) |
			      ((uint64_t)opts.tag_match_bitmap << 48) |
			      ((uint64_t)opts.packet_type & 0xF) << 56 |
			      ((uint64_t)opts.outer_vlan_type & 0x7) << 60 |
			      ((uint64_t)opts.inner_vlan_type & 0x1) << 63;
		req.data[3] = ((uint64_t)opts.inner_vlan_type & 0x6) >> 1 |
			      ((uint64_t)opts.num_tags & 0x7F) << 2 |
			      ((uint64_t)opts.flowid_user & 0x1F) << 9 |
			      ((uint64_t)opts.express & 1) << 14 |
			      ((uint64_t)opts.lmac_id & 0x1F) << 15;

		req.mask[0] = 0x0;
		req.mask[1] = 0xFFFFFFFF00000000;
		req.mask[2] = 0xFFFFFFFFFFFFFFFF;
		req.mask[3] = 0xFFFFFFFFFFFFFFFF;

		ret = roc_mcs_flowid_entry_write(mcs_dev->mdev, &req);
		if (ret)
			return ret;
		*mcs_flow = (void *)(uintptr_t)actions->conf;
	} else {
		plt_err("Flow not confirured");
		return -EINVAL;
	}
	return 0;
}

int
cnxk_mcs_flow_destroy(struct cnxk_eth_dev *dev, void *flow)
{
	const struct cnxk_macsec_sess *s = cnxk_eth_macsec_sess_get_by_sess(dev, flow);
	struct cnxk_mcs_dev *mcs_dev = dev->mcs_dev;
	struct roc_mcs_clear_stats stats_req = {0};
	struct roc_mcs_free_rsrc_req req = {0};
	int ret = 0;

	if (!roc_feature_nix_has_macsec())
		return -ENOTSUP;

	if (s == NULL)
		return 0;

	stats_req.type = CNXK_MCS_RSRC_TYPE_FLOWID;
	stats_req.id = s->flow_id;
	stats_req.dir = s->dir;
	stats_req.all = 0;

	ret = roc_mcs_stats_clear(mcs_dev->mdev, &stats_req);
	if (ret)
		plt_err("Failed to clear stats for Flow id %u, dir %u.", s->flow_id, s->dir);

	req.rsrc_id = s->flow_id;
	req.dir = s->dir;
	req.rsrc_type = CNXK_MCS_RSRC_TYPE_FLOWID;

	ret = roc_mcs_rsrc_free(mcs_dev->mdev, &req);
	if (ret)
		plt_err("Failed to free flow_id: %d.", s->flow_id);

	return ret;
}

int
cnxk_eth_macsec_sa_stats_get(void *device, uint16_t sa_id, enum rte_security_macsec_direction dir,
			     struct rte_security_macsec_sa_stats *stats)
{
	RTE_SET_USED(device);
	RTE_SET_USED(sa_id);
	RTE_SET_USED(dir);
	RTE_SET_USED(stats);

	return 0;
}

int
cnxk_eth_macsec_sc_stats_get(void *device, uint16_t sc_id, enum rte_security_macsec_direction dir,
			     struct rte_security_macsec_sc_stats *stats)
{
	struct rte_eth_dev *eth_dev = (struct rte_eth_dev *)device;
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct cnxk_mcs_dev *mcs_dev = dev->mcs_dev;
	struct roc_mcs_stats_req req = {0};

	if (!roc_feature_nix_has_macsec())
		return -ENOTSUP;

	req.id = sc_id;
	req.dir = (dir == RTE_SECURITY_MACSEC_DIR_RX) ? MCS_RX : MCS_TX;

	return roc_mcs_sc_stats_get(mcs_dev->mdev, &req, (struct roc_mcs_sc_stats *)stats);
}

int
cnxk_eth_macsec_session_stats_get(struct cnxk_eth_dev *dev, struct cnxk_macsec_sess *sess,
				  struct rte_security_stats *stats)
{
	struct cnxk_mcs_dev *mcs_dev = dev->mcs_dev;
	struct roc_mcs_flowid_stats flow_stats = {0};
	struct roc_mcs_port_stats port_stats = {0};
	struct roc_mcs_stats_req req = {0};

	if (!roc_feature_nix_has_macsec())
		return -ENOTSUP;

	req.id = sess->flow_id;
	req.dir = sess->dir;
	roc_mcs_flowid_stats_get(mcs_dev->mdev, &req, &flow_stats);
	plt_nix_dbg("\n******* FLOW_ID IDX[%u] STATS dir: %u********\n", sess->flow_id, sess->dir);
	plt_nix_dbg("TX: tcam_hit_cnt: 0x%" PRIx64 "\n", flow_stats.tcam_hit_cnt);

	req.id = mcs_dev->port_id;
	req.dir = sess->dir;
	roc_mcs_port_stats_get(mcs_dev->mdev, &req, &port_stats);
	plt_nix_dbg("\n********** PORT[0] STATS ****************\n");
	plt_nix_dbg("RX tcam_miss_cnt: 0x%" PRIx64 "\n", port_stats.tcam_miss_cnt);
	plt_nix_dbg("RX parser_err_cnt: 0x%" PRIx64 "\n", port_stats.parser_err_cnt);
	plt_nix_dbg("RX preempt_err_cnt: 0x%" PRIx64 "\n", port_stats.preempt_err_cnt);
	plt_nix_dbg("RX sectag_insert_err_cnt: 0x%" PRIx64 "\n", port_stats.sectag_insert_err_cnt);

	req.id = sess->secy_id;
	req.dir = sess->dir;

	return roc_mcs_secy_stats_get(mcs_dev->mdev, &req,
				      (struct roc_mcs_secy_stats *)(&stats->macsec));
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
