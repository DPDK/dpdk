/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include "otx2_ethdev.h"

int
otx2_nix_rxchan_bpid_cfg(struct rte_eth_dev *eth_dev, bool enb)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_fc_info *fc = &dev->fc_info;
	struct otx2_mbox *mbox = dev->mbox;
	struct nix_bp_cfg_req *req;
	struct nix_bp_cfg_rsp *rsp;
	int rc;

	if (otx2_dev_is_sdp(dev))
		return 0;

	if (enb) {
		req = otx2_mbox_alloc_msg_nix_bp_enable(mbox);
		req->chan_base = 0;
		req->chan_cnt = 1;
		req->bpid_per_chan = 0;

		rc = otx2_mbox_process_msg(mbox, (void *)&rsp);
		if (rc || req->chan_cnt != rsp->chan_cnt) {
			otx2_err("Insufficient BPIDs, alloc=%u < req=%u rc=%d",
				 rsp->chan_cnt, req->chan_cnt, rc);
			return rc;
		}

		fc->bpid[0] = rsp->chan_bpid[0];
	} else {
		req = otx2_mbox_alloc_msg_nix_bp_disable(mbox);
		req->chan_base = 0;
		req->chan_cnt = 1;

		rc = otx2_mbox_process(mbox);

		memset(fc->bpid, 0, sizeof(uint16_t) * NIX_MAX_CHAN);
	}

	return rc;
}

int
otx2_nix_flow_ctrl_get(struct rte_eth_dev *eth_dev,
		       struct rte_eth_fc_conf *fc_conf)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct cgx_pause_frm_cfg *req, *rsp;
	struct otx2_mbox *mbox = dev->mbox;
	int rc;

	if (otx2_dev_is_lbk(dev)) {
		fc_conf->mode = RTE_FC_NONE;
		return 0;
	}

	req = otx2_mbox_alloc_msg_cgx_cfg_pause_frm(mbox);
	req->set = 0;

	rc = otx2_mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		goto done;

	if (rsp->rx_pause && rsp->tx_pause)
		fc_conf->mode = RTE_FC_FULL;
	else if (rsp->rx_pause)
		fc_conf->mode = RTE_FC_RX_PAUSE;
	else if (rsp->tx_pause)
		fc_conf->mode = RTE_FC_TX_PAUSE;
	else
		fc_conf->mode = RTE_FC_NONE;

done:
	return rc;
}

static int
otx2_nix_cq_bp_cfg(struct rte_eth_dev *eth_dev, bool enb)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_fc_info *fc = &dev->fc_info;
	struct otx2_mbox *mbox = dev->mbox;
	struct nix_aq_enq_req *aq;
	struct otx2_eth_rxq *rxq;
	int i, rc;

	for (i = 0; i < eth_dev->data->nb_rx_queues; i++) {
		rxq = eth_dev->data->rx_queues[i];

		aq = otx2_mbox_alloc_msg_nix_aq_enq(mbox);
		if (!aq) {
			/* The shared memory buffer can be full.
			 * flush it and retry
			 */
			otx2_mbox_msg_send(mbox, 0);
			rc = otx2_mbox_wait_for_rsp(mbox, 0);
			if (rc < 0)
				return rc;

			aq = otx2_mbox_alloc_msg_nix_aq_enq(mbox);
			if (!aq)
				return -ENOMEM;
		}
		aq->qidx = rxq->rq;
		aq->ctype = NIX_AQ_CTYPE_CQ;
		aq->op = NIX_AQ_INSTOP_WRITE;

		if (enb) {
			aq->cq.bpid = fc->bpid[0];
			aq->cq_mask.bpid = ~(aq->cq_mask.bpid);
			aq->cq.bp = rxq->cq_drop;
			aq->cq_mask.bp = ~(aq->cq_mask.bp);
		}

		aq->cq.bp_ena = !!enb;
		aq->cq_mask.bp_ena = ~(aq->cq_mask.bp_ena);
	}

	otx2_mbox_msg_send(mbox, 0);
	rc = otx2_mbox_wait_for_rsp(mbox, 0);
	if (rc < 0)
		return rc;

	return 0;
}

static int
otx2_nix_rx_fc_cfg(struct rte_eth_dev *eth_dev, bool enb)
{
	return otx2_nix_cq_bp_cfg(eth_dev, enb);
}

int
otx2_nix_flow_ctrl_set(struct rte_eth_dev *eth_dev,
		       struct rte_eth_fc_conf *fc_conf)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_fc_info *fc = &dev->fc_info;
	struct otx2_mbox *mbox = dev->mbox;
	struct cgx_pause_frm_cfg *req;
	uint8_t tx_pause, rx_pause;
	int rc = 0;

	if (otx2_dev_is_lbk(dev)) {
		otx2_info("No flow control support for LBK bound ethports");
		return -ENOTSUP;
	}

	if (fc_conf->high_water || fc_conf->low_water || fc_conf->pause_time ||
	    fc_conf->mac_ctrl_frame_fwd || fc_conf->autoneg) {
		otx2_info("Flowctrl parameter is not supported");
		return -EINVAL;
	}

	if (fc_conf->mode == fc->mode)
		return 0;

	rx_pause = (fc_conf->mode == RTE_FC_FULL) ||
		    (fc_conf->mode == RTE_FC_RX_PAUSE);
	tx_pause = (fc_conf->mode == RTE_FC_FULL) ||
		    (fc_conf->mode == RTE_FC_TX_PAUSE);

	/* Check if TX pause frame is already enabled or not */
	if (fc->tx_pause ^ tx_pause) {
		if (otx2_dev_is_Ax(dev) && eth_dev->data->dev_started) {
			/* on Ax, CQ should be in disabled state
			 * while setting flow control configuration.
			 */
			otx2_info("Stop the port=%d for setting flow control\n",
				  eth_dev->data->port_id);
				return 0;
		}
		/* TX pause frames, enable/disable flowctrl on RX side. */
		rc = otx2_nix_rx_fc_cfg(eth_dev, tx_pause);
		if (rc)
			return rc;
	}

	req = otx2_mbox_alloc_msg_cgx_cfg_pause_frm(mbox);
	req->set = 1;
	req->rx_pause = rx_pause;
	req->tx_pause = tx_pause;

	rc = otx2_mbox_process(mbox);
	if (rc)
		return rc;

	fc->tx_pause = tx_pause;
	fc->rx_pause = rx_pause;
	fc->mode = fc_conf->mode;

	return rc;
}

int
otx2_nix_update_flow_ctrl_mode(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_fc_info *fc = &dev->fc_info;
	struct rte_eth_fc_conf fc_conf;

	if (otx2_dev_is_lbk(dev) || otx2_dev_is_sdp(dev))
		return 0;

	memset(&fc_conf, 0, sizeof(struct rte_eth_fc_conf));
	fc_conf.mode = fc->mode;

	/* To avoid Link credit deadlock on Ax, disable Tx FC if it's enabled */
	if (otx2_dev_is_Ax(dev) &&
	    (dev->npc_flow.switch_header_type != OTX2_PRIV_FLAGS_HIGIG) &&
	    (fc_conf.mode == RTE_FC_FULL || fc_conf.mode == RTE_FC_RX_PAUSE)) {
		fc_conf.mode =
				(fc_conf.mode == RTE_FC_FULL ||
				fc_conf.mode == RTE_FC_TX_PAUSE) ?
				RTE_FC_TX_PAUSE : RTE_FC_NONE;
	}

	return otx2_nix_flow_ctrl_set(eth_dev, &fc_conf);
}

int
otx2_nix_flow_ctrl_init(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_fc_info *fc = &dev->fc_info;
	struct rte_eth_fc_conf fc_conf;
	int rc;

	if (otx2_dev_is_lbk(dev) || otx2_dev_is_sdp(dev))
		return 0;

	memset(&fc_conf, 0, sizeof(struct rte_eth_fc_conf));
	/* Both Rx & Tx flow ctrl get enabled(RTE_FC_FULL) in HW
	 * by AF driver, update those info in PMD structure.
	 */
	rc = otx2_nix_flow_ctrl_get(eth_dev, &fc_conf);
	if (rc)
		goto exit;

	fc->mode = fc_conf.mode;
	fc->rx_pause = (fc_conf.mode == RTE_FC_FULL) ||
			(fc_conf.mode == RTE_FC_RX_PAUSE);
	fc->tx_pause = (fc_conf.mode == RTE_FC_FULL) ||
			(fc_conf.mode == RTE_FC_TX_PAUSE);

exit:
	return rc;
}
