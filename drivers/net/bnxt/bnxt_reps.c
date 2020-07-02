/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2020 Broadcom
 * All rights reserved.
 */

#include "bnxt.h"
#include "bnxt_ring.h"
#include "bnxt_reps.h"
#include "hsi_struct_def_dpdk.h"

static const struct eth_dev_ops bnxt_vf_rep_dev_ops = {
	.dev_infos_get = bnxt_vf_rep_dev_info_get_op,
	.dev_configure = bnxt_vf_rep_dev_configure_op,
	.dev_start = bnxt_vf_rep_dev_start_op,
	.rx_queue_setup = bnxt_vf_rep_rx_queue_setup_op,
	.tx_queue_setup = bnxt_vf_rep_tx_queue_setup_op,
	.link_update = bnxt_vf_rep_link_update_op,
	.dev_close = bnxt_vf_rep_dev_close_op,
	.dev_stop = bnxt_vf_rep_dev_stop_op
};

static uint16_t
bnxt_vf_rep_rx_burst(__rte_unused void *rx_queue,
		     __rte_unused struct rte_mbuf **rx_pkts,
		     __rte_unused uint16_t nb_pkts)
{
	return 0;
}

static uint16_t
bnxt_vf_rep_tx_burst(__rte_unused void *tx_queue,
		     __rte_unused struct rte_mbuf **tx_pkts,
		     __rte_unused uint16_t nb_pkts)
{
	return 0;
}

int bnxt_vf_representor_init(struct rte_eth_dev *eth_dev, void *params)
{
	struct bnxt_vf_representor *vf_rep_bp = eth_dev->data->dev_private;
	struct bnxt_vf_representor *rep_params =
				 (struct bnxt_vf_representor *)params;
	struct rte_eth_link *link;
	struct bnxt *parent_bp;

	vf_rep_bp->vf_id = rep_params->vf_id;
	vf_rep_bp->switch_domain_id = rep_params->switch_domain_id;
	vf_rep_bp->parent_priv = rep_params->parent_priv;

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
	parent_bp = vf_rep_bp->parent_priv;
	link = &parent_bp->eth_dev->data->dev_link;

	eth_dev->data->dev_link.link_speed = link->link_speed;
	eth_dev->data->dev_link.link_duplex = link->link_duplex;
	eth_dev->data->dev_link.link_status = link->link_status;
	eth_dev->data->dev_link.link_autoneg = link->link_autoneg;

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

	parent_bp = rep->parent_priv;
	if (parent_bp) {
		parent_bp->num_reps--;
		vf_id = rep->vf_id;
		memset(&parent_bp->rep_info[vf_id], 0,
		       sizeof(parent_bp->rep_info[vf_id]));
		/* mark that this representor has been freed */
	}
	eth_dev->dev_ops = NULL;
	return 0;
}

int bnxt_vf_rep_link_update_op(struct rte_eth_dev *eth_dev, int wait_to_compl)
{
	struct bnxt *parent_bp;
	struct bnxt_vf_representor *rep =
		(struct bnxt_vf_representor *)eth_dev->data->dev_private;
	struct rte_eth_link *link;
	int rc;

	parent_bp = rep->parent_priv;
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

int bnxt_vf_rep_dev_start_op(struct rte_eth_dev *eth_dev)
{
	bnxt_vf_rep_link_update_op(eth_dev, 1);

	return 0;
}

void bnxt_vf_rep_dev_stop_op(__rte_unused struct rte_eth_dev *eth_dev)
{
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
	uint16_t max_vnics, i, j, vpool, vrxq;
	unsigned int max_rx_rings;
	int rc = 0;

	/* MAC Specifics */
	parent_bp = rep_bp->parent_priv;
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
	max_vnics = parent_bp->max_vnics;

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

	/* *INDENT-OFF* */
	dev_info->default_rxconf = (struct rte_eth_rxconf) {
		.rx_thresh = {
			.pthresh = 8,
			.hthresh = 8,
			.wthresh = 0,
		},
		.rx_free_thresh = 32,
		/* If no descriptors available, pkts are dropped by default */
		.rx_drop_en = 1,
	};

	dev_info->default_txconf = (struct rte_eth_txconf) {
		.tx_thresh = {
			.pthresh = 32,
			.hthresh = 0,
			.wthresh = 0,
		},
		.tx_free_thresh = 32,
		.tx_rs_thresh = 32,
	};
	eth_dev->data->dev_conf.intr_conf.lsc = 1;

	eth_dev->data->dev_conf.intr_conf.rxq = 1;
	dev_info->rx_desc_lim.nb_min = BNXT_MIN_RING_DESC;
	dev_info->rx_desc_lim.nb_max = BNXT_MAX_RX_RING_DESC;
	dev_info->tx_desc_lim.nb_min = BNXT_MIN_RING_DESC;
	dev_info->tx_desc_lim.nb_max = BNXT_MAX_TX_RING_DESC;

	/* *INDENT-ON* */

	/*
	 * TODO: default_rxconf, default_txconf, rx_desc_lim, and tx_desc_lim
	 *       need further investigation.
	 */

	/* VMDq resources */
	vpool = 64; /* ETH_64_POOLS */
	vrxq = 128; /* ETH_VMDQ_DCB_NUM_QUEUES */
	for (i = 0; i < 4; vpool >>= 1, i++) {
		if (max_vnics > vpool) {
			for (j = 0; j < 5; vrxq >>= 1, j++) {
				if (dev_info->max_rx_queues > vrxq) {
					if (vpool > vrxq)
						vpool = vrxq;
					goto found;
				}
			}
			/* Not enough resources to support VMDq */
			break;
		}
	}
	/* Not enough resources to support VMDq */
	vpool = 0;
	vrxq = 0;
found:
	dev_info->max_vmdq_pools = vpool;
	dev_info->vmdq_queue_num = vrxq;

	dev_info->vmdq_pool_base = 0;
	dev_info->vmdq_queue_base = 0;

	return 0;
}

int bnxt_vf_rep_dev_configure_op(__rte_unused struct rte_eth_dev *eth_dev)
{
	PMD_DRV_LOG(DEBUG, "Representor dev_configure_op\n");
	return 0;
}

int bnxt_vf_rep_rx_queue_setup_op(__rte_unused struct rte_eth_dev *eth_dev,
				  __rte_unused uint16_t queue_idx,
				  __rte_unused uint16_t nb_desc,
				  __rte_unused unsigned int socket_id,
				  __rte_unused const struct rte_eth_rxconf *
				  rx_conf,
				  __rte_unused struct rte_mempool *mp)
{
	return 0;
}

int bnxt_vf_rep_tx_queue_setup_op(__rte_unused struct rte_eth_dev *eth_dev,
				  __rte_unused uint16_t queue_idx,
				  __rte_unused uint16_t nb_desc,
				  __rte_unused unsigned int socket_id,
				  __rte_unused const struct rte_eth_txconf *
				  tx_conf)
{
	return 0;
}
