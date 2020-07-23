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

static const struct eth_dev_ops bnxt_vf_rep_dev_ops = {
	.dev_infos_get = bnxt_vf_rep_dev_info_get_op,
	.dev_configure = bnxt_vf_rep_dev_configure_op,
	.dev_start = bnxt_vf_rep_dev_start_op,
	.rx_queue_setup = bnxt_vf_rep_rx_queue_setup_op,
	.rx_queue_release = bnxt_vf_rep_rx_queue_release_op,
	.tx_queue_setup = bnxt_vf_rep_tx_queue_setup_op,
	.tx_queue_release = bnxt_vf_rep_tx_queue_release_op,
	.link_update = bnxt_vf_rep_link_update_op,
	.dev_close = bnxt_vf_rep_dev_close_op,
	.dev_stop = bnxt_vf_rep_dev_stop_op,
	.stats_get = bnxt_vf_rep_stats_get_op,
	.stats_reset = bnxt_vf_rep_stats_reset_op,
	.filter_ctrl = bnxt_filter_ctrl_op
};

uint16_t
bnxt_vfr_recv(uint16_t port_id, uint16_t queue_id, struct rte_mbuf *mbuf)
{
	struct bnxt_sw_rx_bd *prod_rx_buf;
	struct bnxt_rx_ring_info *rep_rxr;
	struct bnxt_rx_queue *rep_rxq;
	struct rte_eth_dev *vfr_eth_dev;
	struct bnxt_vf_representor *vfr_bp;
	uint16_t mask;
	uint8_t que;

	vfr_eth_dev = &rte_eth_devices[port_id];
	if (!vfr_eth_dev)
		return 1;
	vfr_bp = vfr_eth_dev->data->dev_private;
	/* If rxq_id happens to be > max rep_queue, use rxq0 */
	que = queue_id < BNXT_MAX_VF_REP_RINGS ? queue_id : 0;
	rep_rxq = vfr_bp->rx_queues[que];
	rep_rxr = rep_rxq->rx_ring;
	mask = rep_rxr->rx_ring_struct->ring_mask;

	/* Put this mbuf on the RxQ of the Representor */
	prod_rx_buf =
		&rep_rxr->rx_buf_ring[rep_rxr->rx_prod++ & mask];
	if (!prod_rx_buf->mbuf) {
		prod_rx_buf->mbuf = mbuf;
		vfr_bp->rx_bytes[que] += mbuf->pkt_len;
		vfr_bp->rx_pkts[que]++;
	} else {
		vfr_bp->rx_drop_bytes[que] += mbuf->pkt_len;
		vfr_bp->rx_drop_pkts[que]++;
		rte_free(mbuf); /* Representor Rx ring full, drop pkt */
	}

	return 0;
}

static uint16_t
bnxt_vf_rep_rx_burst(void *rx_queue,
		     struct rte_mbuf **rx_pkts,
		     uint16_t nb_pkts)
{
	struct bnxt_rx_queue *rxq = rx_queue;
	struct bnxt_sw_rx_bd *cons_rx_buf;
	struct bnxt_rx_ring_info *rxr;
	uint16_t nb_rx_pkts = 0;
	uint16_t mask, i;

	if (!rxq)
		return 0;

	rxr = rxq->rx_ring;
	mask = rxr->rx_ring_struct->ring_mask;
	for (i = 0; i < nb_pkts; i++) {
		cons_rx_buf = &rxr->rx_buf_ring[rxr->rx_cons & mask];
		if (!cons_rx_buf->mbuf)
			return nb_rx_pkts;
		rx_pkts[nb_rx_pkts] = cons_rx_buf->mbuf;
		rx_pkts[nb_rx_pkts]->port = rxq->port_id;
		cons_rx_buf->mbuf = NULL;
		nb_rx_pkts++;
		rxr->rx_cons++;
	}

	return nb_rx_pkts;
}

static uint16_t
bnxt_vf_rep_tx_burst(void *tx_queue,
		     struct rte_mbuf **tx_pkts,
		     __rte_unused uint16_t nb_pkts)
{
	struct bnxt_vf_rep_tx_queue *vfr_txq = tx_queue;
	struct bnxt_tx_queue *ptxq;
	struct bnxt *parent;
	struct  bnxt_vf_representor *vf_rep_bp;
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

int bnxt_vf_representor_init(struct rte_eth_dev *eth_dev, void *params)
{
	struct bnxt_vf_representor *vf_rep_bp = eth_dev->data->dev_private;
	struct bnxt_vf_representor *rep_params =
				 (struct bnxt_vf_representor *)params;
	struct rte_eth_link *link;
	struct bnxt *parent_bp;
	int rc = 0;

	vf_rep_bp->vf_id = rep_params->vf_id;
	vf_rep_bp->switch_domain_id = rep_params->switch_domain_id;
	vf_rep_bp->parent_dev = rep_params->parent_dev;

	eth_dev->data->dev_flags |= RTE_ETH_DEV_REPRESENTOR;
	eth_dev->data->representor_id = rep_params->vf_id;

	rte_eth_random_addr(vf_rep_bp->dflt_mac_addr);
	memcpy(vf_rep_bp->mac_addr, vf_rep_bp->dflt_mac_addr,
	       sizeof(vf_rep_bp->mac_addr));
	eth_dev->data->mac_addrs =
		(struct rte_ether_addr *)&vf_rep_bp->mac_addr;
	eth_dev->dev_ops = &bnxt_vf_rep_dev_ops;

	/* No data-path, but need stub Rx/Tx functions to avoid crash
	 * when testing with ovs-dpdk
	 */
	eth_dev->rx_pkt_burst = bnxt_vf_rep_rx_burst;
	eth_dev->tx_pkt_burst = bnxt_vf_rep_tx_burst;
	/* Link state. Inherited from PF or trusted VF */
	parent_bp = vf_rep_bp->parent_dev->data->dev_private;
	link = &parent_bp->eth_dev->data->dev_link;

	eth_dev->data->dev_link.link_speed = link->link_speed;
	eth_dev->data->dev_link.link_duplex = link->link_duplex;
	eth_dev->data->dev_link.link_status = link->link_status;
	eth_dev->data->dev_link.link_autoneg = link->link_autoneg;

	vf_rep_bp->fw_fid = rep_params->vf_id + parent_bp->first_vf_id;
	PMD_DRV_LOG(INFO, "vf_rep->fw_fid = %d\n", vf_rep_bp->fw_fid);
	rc = bnxt_hwrm_get_dflt_vnic_svif(parent_bp, vf_rep_bp->fw_fid,
					  &vf_rep_bp->dflt_vnic_id,
					  &vf_rep_bp->svif);
	if (rc)
		PMD_DRV_LOG(ERR, "Failed to get default vnic id of VF\n");
	else
		PMD_DRV_LOG(INFO, "vf_rep->dflt_vnic_id = %d\n",
			    vf_rep_bp->dflt_vnic_id);

	PMD_DRV_LOG(INFO, "calling bnxt_print_link_info\n");
	bnxt_print_link_info(eth_dev);

	/* Pass the information to the rte_eth_dev_close() that it should also
	 * release the private port resources.
	 */
	eth_dev->data->dev_flags |= RTE_ETH_DEV_CLOSE_REMOVE;
	PMD_DRV_LOG(INFO,
		    "Switch domain id %d: Representor Device %d init done\n",
		    vf_rep_bp->switch_domain_id, vf_rep_bp->vf_id);

	return 0;
}

int bnxt_vf_representor_uninit(struct rte_eth_dev *eth_dev)
{
	struct bnxt *parent_bp;
	struct bnxt_vf_representor *rep =
		(struct bnxt_vf_representor *)eth_dev->data->dev_private;

	uint16_t vf_id;

	eth_dev->data->mac_addrs = NULL;
	eth_dev->dev_ops = NULL;

	parent_bp = rep->parent_dev->data->dev_private;
	if (!parent_bp)
		return 0;

	parent_bp->num_reps--;
	vf_id = rep->vf_id;
	if (parent_bp->rep_info)
		memset(&parent_bp->rep_info[vf_id], 0,
		       sizeof(parent_bp->rep_info[vf_id]));
		/* mark that this representor has been freed */
	return 0;
}

int bnxt_vf_rep_link_update_op(struct rte_eth_dev *eth_dev, int wait_to_compl)
{
	struct bnxt *parent_bp;
	struct bnxt_vf_representor *rep =
		(struct bnxt_vf_representor *)eth_dev->data->dev_private;
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
	struct bnxt_vf_representor *vfr = vfr_ethdev->data->dev_private;
	struct rte_eth_dev *parent_dev = vfr->parent_dev;
	struct bnxt *parent_bp = parent_dev->data->dev_private;
	uint16_t vfr_port_id = vfr_ethdev->data->port_id;
	struct ulp_tlv_param param_list[] = {
		{
			.type = BNXT_ULP_DF_PARAM_TYPE_DEV_PORT_ID,
			.length = 2,
			.value = {(vfr_port_id >> 8) & 0xff, vfr_port_id & 0xff}
		},
		{
			.type = BNXT_ULP_DF_PARAM_TYPE_LAST,
			.length = 0,
			.value = {0}
		}
	};

	ulp_port_db_dev_port_intf_update(parent_bp->ulp_ctx, vfr_ethdev);

	rc = ulp_default_flow_create(parent_dev, param_list,
				     BNXT_ULP_DF_TPL_VFREP_TO_VF,
				     &vfr->rep2vf_flow_id);
	if (rc) {
		BNXT_TF_DBG(DEBUG,
			    "Default flow rule creation for VFR->VF failed!\n");
		goto err;
	}

	BNXT_TF_DBG(DEBUG, "*** Default flow rule created for VFR->VF! ***\n");
	BNXT_TF_DBG(DEBUG, "rep2vf_flow_id = %d\n", vfr->rep2vf_flow_id);
	rc = ulp_default_flow_db_cfa_action_get(parent_bp->ulp_ctx,
						vfr->rep2vf_flow_id,
						&vfr->vfr_tx_cfa_action);
	if (rc) {
		BNXT_TF_DBG(DEBUG,
			    "Failed to get action_ptr for VFR->VF dflt rule\n");
		goto rep2vf_free;
	}
	BNXT_TF_DBG(DEBUG, "tx_cfa_action = %d\n", vfr->vfr_tx_cfa_action);
	rc = ulp_default_flow_create(parent_dev, param_list,
				     BNXT_ULP_DF_TPL_VF_TO_VFREP,
				     &vfr->vf2rep_flow_id);
	if (rc) {
		BNXT_TF_DBG(DEBUG,
			    "Default flow rule creation for VF->VFR failed!\n");
		goto rep2vf_free;
	}

	BNXT_TF_DBG(DEBUG, "*** Default flow rule created for VF->VFR! ***\n");
	BNXT_TF_DBG(DEBUG, "vfr2rep_flow_id = %d\n", vfr->vf2rep_flow_id);

	rc = bnxt_hwrm_cfa_vfr_alloc(parent_bp, vfr->vf_id);
	if (rc)
		goto vf2rep_free;

	return 0;

vf2rep_free:
	ulp_default_flow_destroy(vfr->parent_dev, vfr->vf2rep_flow_id);
rep2vf_free:
	ulp_default_flow_destroy(vfr->parent_dev, vfr->rep2vf_flow_id);
err:
	return -EIO;
}

static int bnxt_vfr_alloc(struct rte_eth_dev *vfr_ethdev)
{
	int rc = 0;
	struct bnxt_vf_representor *vfr = vfr_ethdev->data->dev_private;

	if (!vfr || !vfr->parent_dev) {
		PMD_DRV_LOG(ERR,
			    "No memory allocated for representor\n");
		return -ENOMEM;
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

static void bnxt_vf_rep_free_rx_mbufs(struct bnxt_vf_representor *rep_bp)
{
	struct bnxt_rx_queue *rxq;
	unsigned int i;

	for (i = 0; i < rep_bp->rx_nr_rings; i++) {
		rxq = rep_bp->rx_queues[i];
		bnxt_rx_queue_release_mbufs(rxq);
	}
}

int bnxt_vf_rep_dev_start_op(struct rte_eth_dev *eth_dev)
{
	struct bnxt_vf_representor *rep_bp = eth_dev->data->dev_private;
	int rc;

	rc = bnxt_vfr_alloc(eth_dev);

	if (!rc) {
		eth_dev->rx_pkt_burst = &bnxt_vf_rep_rx_burst;
		eth_dev->tx_pkt_burst = &bnxt_vf_rep_tx_burst;

		bnxt_vf_rep_link_update_op(eth_dev, 1);
	} else {
		eth_dev->data->dev_link.link_status = 0;
		bnxt_vf_rep_free_rx_mbufs(rep_bp);
	}

	return rc;
}

static int bnxt_tf_vfr_free(struct bnxt_vf_representor *vfr)
{
	int rc = 0;

	rc = ulp_default_flow_destroy(vfr->parent_dev,
				      vfr->rep2vf_flow_id);
	if (rc)
		PMD_DRV_LOG(ERR,
			    "default flow destroy failed rep2vf flowid: %d\n",
			    vfr->rep2vf_flow_id);
	rc = ulp_default_flow_destroy(vfr->parent_dev,
				      vfr->vf2rep_flow_id);
	if (rc)
		PMD_DRV_LOG(ERR,
			    "default flow destroy failed vf2rep flowid: %d\n",
			    vfr->vf2rep_flow_id);
	return 0;
}

static int bnxt_vfr_free(struct bnxt_vf_representor *vfr)
{
	int rc = 0;
	struct bnxt *parent_bp;

	if (!vfr || !vfr->parent_dev) {
		PMD_DRV_LOG(ERR,
			    "No memory allocated for representor\n");
		return -ENOMEM;
	}

	parent_bp = vfr->parent_dev->data->dev_private;
	if (!parent_bp)
		return 0;

	/* Check if representor has been already freed in FW */
	if (!vfr->vfr_tx_cfa_action)
		return 0;

	rc = bnxt_tf_vfr_free(vfr);
	if (rc) {
		PMD_DRV_LOG(ERR,
			    "Failed to free representor %d in FW\n",
			    vfr->vf_id);
		return rc;
	}

	PMD_DRV_LOG(DEBUG, "freed representor %d in FW\n",
		    vfr->vf_id);
	vfr->vfr_tx_cfa_action = 0;

	rc = bnxt_hwrm_cfa_vfr_free(parent_bp, vfr->vf_id);

	return rc;
}

void bnxt_vf_rep_dev_stop_op(struct rte_eth_dev *eth_dev)
{
	struct bnxt_vf_representor *vfr_bp = eth_dev->data->dev_private;

	/* Avoid crashes as we are about to free queues */
	eth_dev->rx_pkt_burst = &bnxt_dummy_recv_pkts;
	eth_dev->tx_pkt_burst = &bnxt_dummy_xmit_pkts;

	bnxt_vfr_free(vfr_bp);

	if (eth_dev->data->dev_started)
		eth_dev->data->dev_link.link_status = 0;

	bnxt_vf_rep_free_rx_mbufs(vfr_bp);
}

void bnxt_vf_rep_dev_close_op(struct rte_eth_dev *eth_dev)
{
	bnxt_vf_representor_uninit(eth_dev);
}

int bnxt_vf_rep_dev_info_get_op(struct rte_eth_dev *eth_dev,
				struct rte_eth_dev_info *dev_info)
{
	struct bnxt_vf_representor *rep_bp = eth_dev->data->dev_private;
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

	return 0;
}

int bnxt_vf_rep_dev_configure_op(__rte_unused struct rte_eth_dev *eth_dev)
{
	struct bnxt_vf_representor *rep_bp = eth_dev->data->dev_private;

	PMD_DRV_LOG(DEBUG, "Representor dev_configure_op\n");
	rep_bp->rx_queues = (void *)eth_dev->data->rx_queues;
	rep_bp->tx_nr_rings = eth_dev->data->nb_tx_queues;
	rep_bp->rx_nr_rings = eth_dev->data->nb_rx_queues;

	return 0;
}

int bnxt_vf_rep_rx_queue_setup_op(struct rte_eth_dev *eth_dev,
			  uint16_t queue_idx,
			  uint16_t nb_desc,
			  unsigned int socket_id,
			  __rte_unused const struct rte_eth_rxconf *rx_conf,
			  __rte_unused struct rte_mempool *mp)
{
	struct bnxt_vf_representor *rep_bp = eth_dev->data->dev_private;
	struct bnxt *parent_bp = rep_bp->parent_dev->data->dev_private;
	struct bnxt_rx_queue *parent_rxq;
	struct bnxt_rx_queue *rxq;
	struct bnxt_sw_rx_bd *buf_ring;
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

	rc = bnxt_init_rx_ring_struct(rxq, socket_id);
	if (rc)
		goto out;

	buf_ring = rte_zmalloc_socket("bnxt_rx_vfr_buf_ring",
				      sizeof(struct bnxt_sw_rx_bd) *
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
		bnxt_rx_queue_release_op(rxq);

	return rc;
}

void bnxt_vf_rep_rx_queue_release_op(void *rx_queue)
{
	struct bnxt_rx_queue *rxq = (struct bnxt_rx_queue *)rx_queue;

	if (!rxq)
		return;

	bnxt_rx_queue_release_mbufs(rxq);

	bnxt_free_ring(rxq->rx_ring->rx_ring_struct);
	bnxt_free_ring(rxq->rx_ring->ag_ring_struct);
	bnxt_free_ring(rxq->cp_ring->cp_ring_struct);

	rte_free(rxq);
}

int bnxt_vf_rep_tx_queue_setup_op(struct rte_eth_dev *eth_dev,
			  uint16_t queue_idx,
			  uint16_t nb_desc,
			  unsigned int socket_id,
			  __rte_unused const struct rte_eth_txconf *tx_conf)
{
	struct bnxt_vf_representor *rep_bp = eth_dev->data->dev_private;
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
		bnxt_vf_rep_tx_queue_release_op(vfr_txq);
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

void bnxt_vf_rep_tx_queue_release_op(void *tx_queue)
{
	struct bnxt_vf_rep_tx_queue *vfr_txq = tx_queue;

	if (!vfr_txq)
		return;

	rte_free(vfr_txq->txq);
	rte_free(vfr_txq);
}

int bnxt_vf_rep_stats_get_op(struct rte_eth_dev *eth_dev,
			     struct rte_eth_stats *stats)
{
	struct bnxt_vf_representor *rep_bp = eth_dev->data->dev_private;
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

int bnxt_vf_rep_stats_reset_op(struct rte_eth_dev *eth_dev)
{
	struct bnxt_vf_representor *rep_bp = eth_dev->data->dev_private;
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
