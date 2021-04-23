/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2020 Broadcom
 * All rights reserved.
 */

#include "bnxt.h"
#include "bnxt_ring.h"
#include "bnxt_reps.h"
#include "bnxt_rxq.h"
#include "bnxt_rxr.h"
#include "bnxt_txq.h"
#include "bnxt_txr.h"
#include "bnxt_hwrm.h"
#include "hsi_struct_def_dpdk.h"
#include "bnxt_tf_common.h"
#include "ulp_port_db.h"
#include "ulp_flow_db.h"

static const struct eth_dev_ops bnxt_rep_dev_ops = {
	.dev_infos_get = bnxt_rep_dev_info_get_op,
	.dev_configure = bnxt_rep_dev_configure_op,
	.dev_start = bnxt_rep_dev_start_op,
	.rx_queue_setup = bnxt_rep_rx_queue_setup_op,
	.rx_queue_release = bnxt_rep_rx_queue_release_op,
	.tx_queue_setup = bnxt_rep_tx_queue_setup_op,
	.tx_queue_release = bnxt_rep_tx_queue_release_op,
	.link_update = bnxt_rep_link_update_op,
	.dev_close = bnxt_rep_dev_close_op,
	.dev_stop = bnxt_rep_dev_stop_op,
	.stats_get = bnxt_rep_stats_get_op,
	.stats_reset = bnxt_rep_stats_reset_op,
	.filter_ctrl = bnxt_filter_ctrl_op
};

uint16_t
bnxt_vfr_recv(uint16_t port_id, uint16_t queue_id, struct rte_mbuf *mbuf)
{
	struct rte_mbuf **prod_rx_buf;
	struct bnxt_rx_ring_info *rep_rxr;
	struct bnxt_rx_queue *rep_rxq;
	struct rte_eth_dev *vfr_eth_dev;
	struct bnxt_representor *vfr_bp;
	uint16_t mask;
	uint8_t que;

	vfr_eth_dev = &rte_eth_devices[port_id];
	vfr_bp = vfr_eth_dev->data->dev_private;
	/* If rxq_id happens to be > nr_rings, use ring 0 */
	que = queue_id < vfr_bp->rx_nr_rings ? queue_id : 0;
	rep_rxq = vfr_bp->rx_queues[que];
	/* Ideally should not happen now, paranoid check */
	if (!rep_rxq)
		return 1;
	rep_rxr = rep_rxq->rx_ring;
	mask = rep_rxr->rx_ring_struct->ring_mask;

	/* Put this mbuf on the RxQ of the Representor */
	prod_rx_buf = &rep_rxr->rx_buf_ring[rep_rxr->rx_prod & mask];
	if (!*prod_rx_buf) {
		*prod_rx_buf = mbuf;
		vfr_bp->rx_bytes[que] += mbuf->pkt_len;
		vfr_bp->rx_pkts[que]++;
		rep_rxr->rx_prod++;
	} else {
		/* Representor Rx ring full, drop pkt */
		vfr_bp->rx_drop_bytes[que] += mbuf->pkt_len;
		vfr_bp->rx_drop_pkts[que]++;
		rte_mbuf_raw_free(mbuf);
	}

	return 0;
}

static uint16_t
bnxt_rep_rx_burst(void *rx_queue,
		     struct rte_mbuf **rx_pkts,
		     uint16_t nb_pkts)
{
	struct bnxt_rx_queue *rxq = rx_queue;
	struct rte_mbuf **cons_rx_buf;
	struct bnxt_rx_ring_info *rxr;
	uint16_t nb_rx_pkts = 0;
	uint16_t mask, i;

	if (!rxq)
		return 0;

	rxr = rxq->rx_ring;
	mask = rxr->rx_ring_struct->ring_mask;
	for (i = 0; i < nb_pkts; i++) {
		cons_rx_buf = &rxr->rx_buf_ring[rxr->rx_cons & mask];
		if (*cons_rx_buf == NULL)
			return nb_rx_pkts;
		rx_pkts[nb_rx_pkts] = *cons_rx_buf;
		rx_pkts[nb_rx_pkts]->port = rxq->port_id;
		*cons_rx_buf = NULL;
		nb_rx_pkts++;
		rxr->rx_cons++;
	}

	return nb_rx_pkts;
}

static uint16_t
bnxt_rep_tx_burst(void *tx_queue,
		     struct rte_mbuf **tx_pkts,
		     uint16_t nb_pkts)
{
	struct bnxt_vf_rep_tx_queue *vfr_txq = tx_queue;
	struct bnxt_tx_queue *ptxq;
	struct bnxt *parent;
	struct  bnxt_representor *vf_rep_bp;
	int qid;
	int rc;
	int i;

	if (!vfr_txq)
		return 0;

	qid = vfr_txq->txq->queue_id;
	vf_rep_bp = vfr_txq->bp;
	parent = vf_rep_bp->parent_dev->data->dev_private;
	pthread_mutex_lock(&parent->rep_info->vfr_lock);
	ptxq = parent->tx_queues[qid];

	ptxq->vfr_tx_cfa_action = vf_rep_bp->vfr_tx_cfa_action;

	for (i = 0; i < nb_pkts; i++) {
		vf_rep_bp->tx_bytes[qid] += tx_pkts[i]->pkt_len;
		vf_rep_bp->tx_pkts[qid]++;
	}

	rc = bnxt_xmit_pkts(ptxq, tx_pkts, nb_pkts);
	ptxq->vfr_tx_cfa_action = 0;
	pthread_mutex_unlock(&parent->rep_info->vfr_lock);

	return rc;
}

static int
bnxt_get_dflt_vnic_svif(struct bnxt *bp, struct bnxt_representor *vf_rep_bp)
{
	struct bnxt_rep_info *rep_info;
	int rc;

	rc = bnxt_hwrm_get_dflt_vnic_svif(bp, vf_rep_bp->fw_fid,
					  &vf_rep_bp->dflt_vnic_id,
					  &vf_rep_bp->svif);
	if (rc) {
		PMD_DRV_LOG(ERR, "Failed to get default vnic id of VF\n");
		vf_rep_bp->dflt_vnic_id = BNXT_DFLT_VNIC_ID_INVALID;
		vf_rep_bp->svif = BNXT_SVIF_INVALID;
	} else {
		PMD_DRV_LOG(INFO, "vf_rep->dflt_vnic_id = %d\n",
				vf_rep_bp->dflt_vnic_id);
	}
	if (vf_rep_bp->dflt_vnic_id != BNXT_DFLT_VNIC_ID_INVALID &&
	    vf_rep_bp->svif != BNXT_SVIF_INVALID) {
		rep_info = &bp->rep_info[vf_rep_bp->vf_id];
		rep_info->conduit_valid = true;
	}

	return rc;
}

int bnxt_representor_init(struct rte_eth_dev *eth_dev, void *params)
{
	struct bnxt_representor *vf_rep_bp = eth_dev->data->dev_private;
	struct bnxt_representor *rep_params =
				 (struct bnxt_representor *)params;
	struct rte_eth_link *link;
	struct bnxt *parent_bp;
	uint16_t first_vf_id;
	int rc = 0;

	PMD_DRV_LOG(DEBUG, "BNXT Port:%d VFR init\n", eth_dev->data->port_id);
	vf_rep_bp->vf_id = rep_params->vf_id;
	vf_rep_bp->switch_domain_id = rep_params->switch_domain_id;
	vf_rep_bp->parent_dev = rep_params->parent_dev;
	vf_rep_bp->rep_based_pf = rep_params->rep_based_pf;
	vf_rep_bp->flags = rep_params->flags;
	vf_rep_bp->rep_q_r2f = rep_params->rep_q_r2f;
	vf_rep_bp->rep_q_f2r = rep_params->rep_q_f2r;
	vf_rep_bp->rep_fc_r2f = rep_params->rep_fc_r2f;
	vf_rep_bp->rep_fc_f2r = rep_params->rep_fc_f2r;

	eth_dev->data->dev_flags |= RTE_ETH_DEV_REPRESENTOR |
					RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS;
	eth_dev->data->representor_id = rep_params->vf_id;

	rte_eth_random_addr(vf_rep_bp->dflt_mac_addr);
	memcpy(vf_rep_bp->mac_addr, vf_rep_bp->dflt_mac_addr,
	       sizeof(vf_rep_bp->mac_addr));
	eth_dev->data->mac_addrs =
		(struct rte_ether_addr *)&vf_rep_bp->mac_addr;
	eth_dev->dev_ops = &bnxt_rep_dev_ops;

	/* No data-path, but need stub Rx/Tx functions to avoid crash
	 * when testing with ovs-dpdk
	 */
	eth_dev->rx_pkt_burst = bnxt_rep_rx_burst;
	eth_dev->tx_pkt_burst = bnxt_rep_tx_burst;
	/* Link state. Inherited from PF or trusted VF */
	parent_bp = vf_rep_bp->parent_dev->data->dev_private;
	link = &parent_bp->eth_dev->data->dev_link;

	eth_dev->data->dev_link.link_speed = link->link_speed;
	eth_dev->data->dev_link.link_duplex = link->link_duplex;
	eth_dev->data->dev_link.link_status = link->link_status;
	eth_dev->data->dev_link.link_autoneg = link->link_autoneg;

	PMD_DRV_LOG(INFO, "calling bnxt_print_link_info\n");
	bnxt_print_link_info(eth_dev);

	PMD_DRV_LOG(INFO,
		    "Switch domain id %d: Representor Device %d init done\n",
		    vf_rep_bp->switch_domain_id, vf_rep_bp->vf_id);

	if (BNXT_REP_BASED_PF(vf_rep_bp)) {
		vf_rep_bp->fw_fid = vf_rep_bp->rep_based_pf + 1;
		vf_rep_bp->parent_pf_idx = vf_rep_bp->rep_based_pf;
		if (!(BNXT_REP_PF(vf_rep_bp))) {
			/* VF representor for the remote PF,get first_vf_id */
			rc = bnxt_hwrm_first_vf_id_query(parent_bp,
							 vf_rep_bp->fw_fid,
							 &first_vf_id);
			if (rc)
				return rc;
			if (first_vf_id == 0xffff) {
				PMD_DRV_LOG(ERR,
					    "Invalid first_vf_id fid:%x\n",
					    vf_rep_bp->fw_fid);
				return -EINVAL;
			}
			PMD_DRV_LOG(INFO, "first_vf_id = %x parent_fid:%x\n",
				    first_vf_id, vf_rep_bp->fw_fid);
			vf_rep_bp->fw_fid = rep_params->vf_id + first_vf_id;
		}
	}  else {
		vf_rep_bp->fw_fid = rep_params->vf_id + parent_bp->first_vf_id;
		if (BNXT_VF_IS_TRUSTED(parent_bp))
			vf_rep_bp->parent_pf_idx = parent_bp->parent->fid - 1;
		else
			vf_rep_bp->parent_pf_idx = parent_bp->fw_fid - 1;
	}

	PMD_DRV_LOG(INFO, "vf_rep->fw_fid = %d\n", vf_rep_bp->fw_fid);

	return 0;
}

int bnxt_representor_uninit(struct rte_eth_dev *eth_dev)
{
	struct bnxt *parent_bp;
	struct bnxt_representor *rep =
		(struct bnxt_representor *)eth_dev->data->dev_private;
	uint16_t vf_id;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	PMD_DRV_LOG(DEBUG, "BNXT Port:%d VFR uninit\n", eth_dev->data->port_id);
	eth_dev->data->mac_addrs = NULL;

	parent_bp = rep->parent_dev->data->dev_private;
	if (!parent_bp) {
		PMD_DRV_LOG(DEBUG, "BNXT Port:%d already freed\n",
			    eth_dev->data->port_id);
		return 0;
	}

	parent_bp->num_reps--;
	vf_id = rep->vf_id;
	if (parent_bp->rep_info)
		memset(&parent_bp->rep_info[vf_id], 0,
		       sizeof(parent_bp->rep_info[vf_id]));
		/* mark that this representor has been freed */
	return 0;
}

int bnxt_rep_link_update_op(struct rte_eth_dev *eth_dev, int wait_to_compl)
{
	struct bnxt *parent_bp;
	struct bnxt_representor *rep =
		(struct bnxt_representor *)eth_dev->data->dev_private;
	struct rte_eth_link *link;
	int rc;

	parent_bp = rep->parent_dev->data->dev_private;
	if (!parent_bp)
		return 0;

	rc = bnxt_link_update_op(parent_bp->eth_dev, wait_to_compl);

	/* Link state. Inherited from PF or trusted VF */
	link = &parent_bp->eth_dev->data->dev_link;

	eth_dev->data->dev_link.link_speed = link->link_speed;
	eth_dev->data->dev_link.link_duplex = link->link_duplex;
	eth_dev->data->dev_link.link_status = link->link_status;
	eth_dev->data->dev_link.link_autoneg = link->link_autoneg;
	bnxt_print_link_info(eth_dev);

	return rc;
}

static int bnxt_tf_vfr_alloc(struct rte_eth_dev *vfr_ethdev)
{
	int rc;
	struct bnxt_representor *vfr = vfr_ethdev->data->dev_private;
	struct rte_eth_dev *parent_dev = vfr->parent_dev;
	struct bnxt *parent_bp = parent_dev->data->dev_private;

	if (!parent_bp || !parent_bp->ulp_ctx) {
		BNXT_TF_DBG(ERR, "Invalid arguments\n");
		return 0;
	}

	/* Update the ULP portdata base with the new VFR interface */
	rc = ulp_port_db_dev_port_intf_update(parent_bp->ulp_ctx, vfr_ethdev);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to update ulp port details vfr:%u\n",
			    vfr->vf_id);
		return rc;
	}

	/* Create the default rules for the VFR */
	rc = bnxt_ulp_create_vfr_default_rules(vfr_ethdev);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to create VFR default rules vfr:%u\n",
			    vfr->vf_id);
		return rc;
	}
	/* update the port id so you can backtrack to ethdev */
	vfr->dpdk_port_id = vfr_ethdev->data->port_id;

	rc = bnxt_hwrm_cfa_pair_alloc(parent_bp, vfr);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed in hwrm vfr alloc vfr:%u rc=%d\n",
			    vfr->vf_id, rc);
		(void)bnxt_ulp_delete_vfr_default_rules(vfr);
	}
	BNXT_TF_DBG(DEBUG, "BNXT Port:%d VFR created and initialized\n",
		    vfr->dpdk_port_id);
	return rc;
}

static int bnxt_vfr_alloc(struct rte_eth_dev *vfr_ethdev)
{
	int rc = 0;
	struct bnxt_representor *vfr = vfr_ethdev->data->dev_private;
	struct bnxt *parent_bp;

	if (!vfr || !vfr->parent_dev) {
		PMD_DRV_LOG(ERR,
				"No memory allocated for representor\n");
		return -ENOMEM;
	}

	parent_bp = vfr->parent_dev->data->dev_private;
	if (parent_bp && !parent_bp->ulp_ctx) {
		PMD_DRV_LOG(ERR,
			    "ulp context not allocated for parent\n");
		return -EIO;
	}

	/* Check if representor has been already allocated in FW */
	if (vfr->vfr_tx_cfa_action)
		return 0;

	/*
	 * Alloc VF rep rules in CFA after default VNIC is created.
	 * Otherwise the FW will create the VF-rep rules with
	 * default drop action.
	 */
	rc = bnxt_tf_vfr_alloc(vfr_ethdev);
	if (!rc)
		PMD_DRV_LOG(DEBUG, "allocated representor %d in FW\n",
			    vfr->vf_id);
	else
		PMD_DRV_LOG(ERR,
			    "Failed to alloc representor %d in FW\n",
			    vfr->vf_id);

	return rc;
}

static void bnxt_rep_free_rx_mbufs(struct bnxt_representor *rep_bp)
{
	struct bnxt_rx_queue *rxq;
	unsigned int i;

	for (i = 0; i < rep_bp->rx_nr_rings; i++) {
		rxq = rep_bp->rx_queues[i];
		bnxt_rx_queue_release_mbufs(rxq);
	}
}

int bnxt_rep_dev_start_op(struct rte_eth_dev *eth_dev)
{
	struct bnxt_representor *rep_bp = eth_dev->data->dev_private;
	struct bnxt_rep_info *rep_info;
	struct bnxt *parent_bp;
	int rc;

	parent_bp = rep_bp->parent_dev->data->dev_private;
	rep_info = &parent_bp->rep_info[rep_bp->vf_id];

	BNXT_TF_DBG(DEBUG, "BNXT Port:%d VFR start\n", eth_dev->data->port_id);
	pthread_mutex_lock(&rep_info->vfr_start_lock);
	if (!rep_info->conduit_valid) {
		rc = bnxt_get_dflt_vnic_svif(parent_bp, rep_bp);
		if (rc || !rep_info->conduit_valid) {
			pthread_mutex_unlock(&rep_info->vfr_start_lock);
			return rc;
		}
	}
	pthread_mutex_unlock(&rep_info->vfr_start_lock);

	rc = bnxt_vfr_alloc(eth_dev);
	if (rc) {
		eth_dev->data->dev_link.link_status = 0;
		bnxt_rep_free_rx_mbufs(rep_bp);
		return rc;
	}
	eth_dev->rx_pkt_burst = &bnxt_rep_rx_burst;
	eth_dev->tx_pkt_burst = &bnxt_rep_tx_burst;
	bnxt_rep_link_update_op(eth_dev, 1);

	return 0;
}

static int bnxt_tf_vfr_free(struct bnxt_representor *vfr)
{
	BNXT_TF_DBG(DEBUG, "BNXT Port:%d VFR ulp free\n", vfr->dpdk_port_id);
	return bnxt_ulp_delete_vfr_default_rules(vfr);
}

static int bnxt_vfr_free(struct bnxt_representor *vfr)
{
	int rc = 0;
	struct bnxt *parent_bp;

	if (!vfr || !vfr->parent_dev) {
		PMD_DRV_LOG(ERR,
			    "No memory allocated for representor\n");
		return -ENOMEM;
	}

	parent_bp = vfr->parent_dev->data->dev_private;
	if (!parent_bp) {
		PMD_DRV_LOG(DEBUG, "BNXT Port:%d VFR already freed\n",
			    vfr->dpdk_port_id);
		return 0;
	}

	/* Check if representor has been already freed in FW */
	if (!vfr->vfr_tx_cfa_action)
		return 0;

	rc = bnxt_tf_vfr_free(vfr);
	if (rc) {
		PMD_DRV_LOG(ERR,
			    "Failed to free representor %d in FW\n",
			    vfr->vf_id);
	}

	PMD_DRV_LOG(DEBUG, "freed representor %d in FW\n",
		    vfr->vf_id);
	vfr->vfr_tx_cfa_action = 0;

	rc = bnxt_hwrm_cfa_pair_free(parent_bp, vfr);

	return rc;
}

int bnxt_rep_dev_stop_op(struct rte_eth_dev *eth_dev)
{
	struct bnxt_representor *vfr_bp = eth_dev->data->dev_private;

	/* Avoid crashes as we are about to free queues */
	eth_dev->rx_pkt_burst = &bnxt_dummy_recv_pkts;
	eth_dev->tx_pkt_burst = &bnxt_dummy_xmit_pkts;

	BNXT_TF_DBG(DEBUG, "BNXT Port:%d VFR stop\n", eth_dev->data->port_id);

	bnxt_vfr_free(vfr_bp);

	if (eth_dev->data->dev_started)
		eth_dev->data->dev_link.link_status = 0;

	bnxt_rep_free_rx_mbufs(vfr_bp);

	return 0;
}

int bnxt_rep_dev_close_op(struct rte_eth_dev *eth_dev)
{
	BNXT_TF_DBG(DEBUG, "BNXT Port:%d VFR close\n", eth_dev->data->port_id);
	bnxt_representor_uninit(eth_dev);
	return 0;
}

int bnxt_rep_dev_info_get_op(struct rte_eth_dev *eth_dev,
				struct rte_eth_dev_info *dev_info)
{
	struct bnxt_representor *rep_bp = eth_dev->data->dev_private;
	struct bnxt *parent_bp;
	unsigned int max_rx_rings;
	int rc = 0;

	/* MAC Specifics */
	parent_bp = rep_bp->parent_dev->data->dev_private;
	if (!parent_bp) {
		PMD_DRV_LOG(ERR, "Rep parent NULL!\n");
		return rc;
	}
	PMD_DRV_LOG(DEBUG, "Representor dev_info_get_op\n");
	dev_info->max_mac_addrs = parent_bp->max_l2_ctx;
	dev_info->max_hash_mac_addrs = 0;

	max_rx_rings = BNXT_MAX_VF_REP_RINGS;
	/* For the sake of symmetry, max_rx_queues = max_tx_queues */
	dev_info->max_rx_queues = max_rx_rings;
	dev_info->max_tx_queues = max_rx_rings;
	dev_info->reta_size = bnxt_rss_hash_tbl_size(parent_bp);
	dev_info->hash_key_size = 40;

	/* MTU specifics */
	dev_info->min_mtu = RTE_ETHER_MIN_MTU;
	dev_info->max_mtu = BNXT_MAX_MTU;

	/* Fast path specifics */
	dev_info->min_rx_bufsize = 1;
	dev_info->max_rx_pktlen = BNXT_MAX_PKT_LEN;

	dev_info->rx_offload_capa = BNXT_DEV_RX_OFFLOAD_SUPPORT;
	if (parent_bp->flags & BNXT_FLAG_PTP_SUPPORTED)
		dev_info->rx_offload_capa |= DEV_RX_OFFLOAD_TIMESTAMP;
	dev_info->tx_offload_capa = BNXT_DEV_TX_OFFLOAD_SUPPORT;
	dev_info->flow_type_rss_offloads = BNXT_ETH_RSS_SUPPORT;

	dev_info->switch_info.name = eth_dev->device->name;
	dev_info->switch_info.domain_id = rep_bp->switch_domain_id;
	dev_info->switch_info.port_id =
			rep_bp->vf_id & BNXT_SWITCH_PORT_ID_VF_MASK;

	return 0;
}

int bnxt_rep_dev_configure_op(struct rte_eth_dev *eth_dev)
{
	struct bnxt_representor *rep_bp = eth_dev->data->dev_private;

	PMD_DRV_LOG(DEBUG, "Representor dev_configure_op\n");
	rep_bp->rx_queues = (void *)eth_dev->data->rx_queues;
	rep_bp->tx_nr_rings = eth_dev->data->nb_tx_queues;
	rep_bp->rx_nr_rings = eth_dev->data->nb_rx_queues;

	return 0;
}

static int bnxt_init_rep_rx_ring(struct bnxt_rx_queue *rxq,
				 unsigned int socket_id)
{
	struct bnxt_rx_ring_info *rxr;
	struct bnxt_ring *ring;

	rxr = rte_zmalloc_socket("bnxt_rep_rx_ring",
				 sizeof(struct bnxt_rx_ring_info),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (rxr == NULL)
		return -ENOMEM;
	rxq->rx_ring = rxr;

	ring = rte_zmalloc_socket("bnxt_rep_rx_ring_struct",
				  sizeof(struct bnxt_ring),
				  RTE_CACHE_LINE_SIZE, socket_id);
	if (ring == NULL)
		return -ENOMEM;
	rxr->rx_ring_struct = ring;
	ring->ring_size = rte_align32pow2(rxq->nb_rx_desc);
	ring->ring_mask = ring->ring_size - 1;

	return 0;
}

int bnxt_rep_rx_queue_setup_op(struct rte_eth_dev *eth_dev,
			       uint16_t queue_idx,
			       uint16_t nb_desc,
			       unsigned int socket_id,
			       __rte_unused const struct rte_eth_rxconf *rx_conf,
			       __rte_unused struct rte_mempool *mp)
{
	struct bnxt_representor *rep_bp = eth_dev->data->dev_private;
	struct bnxt *parent_bp = rep_bp->parent_dev->data->dev_private;
	struct bnxt_rx_queue *parent_rxq;
	struct bnxt_rx_queue *rxq;
	struct rte_mbuf **buf_ring;
	int rc = 0;

	if (queue_idx >= BNXT_MAX_VF_REP_RINGS) {
		PMD_DRV_LOG(ERR,
			    "Cannot create Rx ring %d. %d rings available\n",
			    queue_idx, BNXT_MAX_VF_REP_RINGS);
		return -EINVAL;
	}

	if (!nb_desc || nb_desc > MAX_RX_DESC_CNT) {
		PMD_DRV_LOG(ERR, "nb_desc %d is invalid\n", nb_desc);
		return -EINVAL;
	}

	if (!parent_bp->rx_queues) {
		PMD_DRV_LOG(ERR, "Parent Rx qs not configured yet\n");
		return -EINVAL;
	}

	parent_rxq = parent_bp->rx_queues[queue_idx];
	if (!parent_rxq) {
		PMD_DRV_LOG(ERR, "Parent RxQ has not been configured yet\n");
		return -EINVAL;
	}

	if (nb_desc != parent_rxq->nb_rx_desc) {
		PMD_DRV_LOG(ERR, "nb_desc %d do not match parent rxq", nb_desc);
		return -EINVAL;
	}

	if (eth_dev->data->rx_queues) {
		rxq = eth_dev->data->rx_queues[queue_idx];
		if (rxq)
			bnxt_rx_queue_release_op(rxq);
	}

	rxq = rte_zmalloc_socket("bnxt_vfr_rx_queue",
				 sizeof(struct bnxt_rx_queue),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (!rxq) {
		PMD_DRV_LOG(ERR, "bnxt_vfr_rx_queue allocation failed!\n");
		return -ENOMEM;
	}

	rxq->nb_rx_desc = nb_desc;

	rc = bnxt_init_rep_rx_ring(rxq, socket_id);
	if (rc)
		goto out;

	buf_ring = rte_zmalloc_socket("bnxt_rx_vfr_buf_ring",
				      sizeof(struct rte_mbuf *) *
				      rxq->rx_ring->rx_ring_struct->ring_size,
				      RTE_CACHE_LINE_SIZE, socket_id);
	if (!buf_ring) {
		PMD_DRV_LOG(ERR, "bnxt_rx_vfr_buf_ring allocation failed!\n");
		rc = -ENOMEM;
		goto out;
	}

	rxq->rx_ring->rx_buf_ring = buf_ring;
	rxq->queue_id = queue_idx;
	rxq->port_id = eth_dev->data->port_id;
	eth_dev->data->rx_queues[queue_idx] = rxq;

	return 0;

out:
	if (rxq)
		bnxt_rep_rx_queue_release_op(rxq);

	return rc;
}

void bnxt_rep_rx_queue_release_op(void *rx_queue)
{
	struct bnxt_rx_queue *rxq = (struct bnxt_rx_queue *)rx_queue;

	if (!rxq)
		return;

	bnxt_rx_queue_release_mbufs(rxq);

	bnxt_free_ring(rxq->rx_ring->rx_ring_struct);
	rte_free(rxq->rx_ring->rx_ring_struct);
	rte_free(rxq->rx_ring);

	rte_free(rxq);
}

int bnxt_rep_tx_queue_setup_op(struct rte_eth_dev *eth_dev,
			       uint16_t queue_idx,
			       uint16_t nb_desc,
			       unsigned int socket_id,
			       __rte_unused const struct rte_eth_txconf *tx_conf)
{
	struct bnxt_representor *rep_bp = eth_dev->data->dev_private;
	struct bnxt *parent_bp = rep_bp->parent_dev->data->dev_private;
	struct bnxt_tx_queue *parent_txq, *txq;
	struct bnxt_vf_rep_tx_queue *vfr_txq;

	if (queue_idx >= BNXT_MAX_VF_REP_RINGS) {
		PMD_DRV_LOG(ERR,
			    "Cannot create Tx rings %d. %d rings available\n",
			    queue_idx, BNXT_MAX_VF_REP_RINGS);
		return -EINVAL;
	}

	if (!nb_desc || nb_desc > MAX_TX_DESC_CNT) {
		PMD_DRV_LOG(ERR, "nb_desc %d is invalid", nb_desc);
		return -EINVAL;
	}

	if (!parent_bp->tx_queues) {
		PMD_DRV_LOG(ERR, "Parent Tx qs not configured yet\n");
		return -EINVAL;
	}

	parent_txq = parent_bp->tx_queues[queue_idx];
	if (!parent_txq) {
		PMD_DRV_LOG(ERR, "Parent TxQ has not been configured yet\n");
		return -EINVAL;
	}

	if (nb_desc != parent_txq->nb_tx_desc) {
		PMD_DRV_LOG(ERR, "nb_desc %d do not match parent txq", nb_desc);
		return -EINVAL;
	}

	if (eth_dev->data->tx_queues) {
		vfr_txq = eth_dev->data->tx_queues[queue_idx];
		bnxt_rep_tx_queue_release_op(vfr_txq);
		vfr_txq = NULL;
	}

	vfr_txq = rte_zmalloc_socket("bnxt_vfr_tx_queue",
				     sizeof(struct bnxt_vf_rep_tx_queue),
				     RTE_CACHE_LINE_SIZE, socket_id);
	if (!vfr_txq) {
		PMD_DRV_LOG(ERR, "bnxt_vfr_tx_queue allocation failed!");
		return -ENOMEM;
	}
	txq = rte_zmalloc_socket("bnxt_tx_queue",
				 sizeof(struct bnxt_tx_queue),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (!txq) {
		PMD_DRV_LOG(ERR, "bnxt_tx_queue allocation failed!");
		rte_free(vfr_txq);
		return -ENOMEM;
	}

	txq->nb_tx_desc = nb_desc;
	txq->queue_id = queue_idx;
	txq->port_id = eth_dev->data->port_id;
	vfr_txq->txq = txq;
	vfr_txq->bp = rep_bp;
	eth_dev->data->tx_queues[queue_idx] = vfr_txq;

	return 0;
}

void bnxt_rep_tx_queue_release_op(void *tx_queue)
{
	struct bnxt_vf_rep_tx_queue *vfr_txq = tx_queue;

	if (!vfr_txq)
		return;

	rte_free(vfr_txq->txq);
	rte_free(vfr_txq);
}

int bnxt_rep_stats_get_op(struct rte_eth_dev *eth_dev,
			     struct rte_eth_stats *stats)
{
	struct bnxt_representor *rep_bp = eth_dev->data->dev_private;
	int i;

	memset(stats, 0, sizeof(*stats));
	for (i = 0; i < BNXT_MAX_VF_REP_RINGS; i++) {
		stats->obytes += rep_bp->tx_bytes[i];
		stats->opackets += rep_bp->tx_pkts[i];
		stats->ibytes += rep_bp->rx_bytes[i];
		stats->ipackets += rep_bp->rx_pkts[i];
		stats->imissed += rep_bp->rx_drop_pkts[i];

		stats->q_ipackets[i] = rep_bp->rx_pkts[i];
		stats->q_ibytes[i] = rep_bp->rx_bytes[i];
		stats->q_opackets[i] = rep_bp->tx_pkts[i];
		stats->q_obytes[i] = rep_bp->tx_bytes[i];
		stats->q_errors[i] = rep_bp->rx_drop_pkts[i];
	}

	return 0;
}

int bnxt_rep_stats_reset_op(struct rte_eth_dev *eth_dev)
{
	struct bnxt_representor *rep_bp = eth_dev->data->dev_private;
	int i;

	for (i = 0; i < BNXT_MAX_VF_REP_RINGS; i++) {
		rep_bp->tx_pkts[i] = 0;
		rep_bp->tx_bytes[i] = 0;
		rep_bp->rx_pkts[i] = 0;
		rep_bp->rx_bytes[i] = 0;
		rep_bp->rx_drop_pkts[i] = 0;
	}
	return 0;
}

int bnxt_rep_stop_all(struct bnxt *bp)
{
	uint16_t vf_id;
	struct rte_eth_dev *rep_eth_dev;
	int ret;

	/* No vfrep ports just exit */
	if (!bp->rep_info)
		return 0;

	for (vf_id = 0; vf_id < BNXT_MAX_VF_REPS; vf_id++) {
		rep_eth_dev = bp->rep_info[vf_id].vfr_eth_dev;
		if (!rep_eth_dev)
			continue;
		ret = bnxt_rep_dev_stop_op(rep_eth_dev);
		if (ret != 0)
			return ret;
	}

	return 0;
}
