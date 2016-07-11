/*-
 *   BSD LICENSE
 *
 *   Copyright(c) Broadcom Limited.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Broadcom Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <inttypes.h>
#include <stdbool.h>

#include <rte_dev.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_cycles.h>

#include "bnxt.h"
#include "bnxt_cpr.h"
#include "bnxt_filter.h"
#include "bnxt_hwrm.h"
#include "bnxt_ring.h"
#include "bnxt_rxq.h"
#include "bnxt_rxr.h"
#include "bnxt_stats.h"
#include "bnxt_txq.h"
#include "bnxt_txr.h"
#include "bnxt_vnic.h"
#include "hsi_struct_def_dpdk.h"

#define DRV_MODULE_NAME		"bnxt"
static const char bnxt_version[] =
	"Broadcom Cumulus driver " DRV_MODULE_NAME "\n";

#define PCI_VENDOR_ID_BROADCOM 0x14E4

#define BROADCOM_DEV_ID_57301 0x16c8
#define BROADCOM_DEV_ID_57302 0x16c9
#define BROADCOM_DEV_ID_57304_PF 0x16ca
#define BROADCOM_DEV_ID_57304_VF 0x16cb
#define BROADCOM_DEV_ID_57402 0x16d0
#define BROADCOM_DEV_ID_57404 0x16d1
#define BROADCOM_DEV_ID_57406_PF 0x16d2
#define BROADCOM_DEV_ID_57406_VF 0x16d3
#define BROADCOM_DEV_ID_57406_MF 0x16d4
#define BROADCOM_DEV_ID_57314 0x16df

static struct rte_pci_id bnxt_pci_id_map[] = {
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_57301) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_57302) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_57304_PF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_57304_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_57402) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_57404) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_57406_PF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_57406_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_57406_MF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_57314) },
	{ .vendor_id = 0, /* sentinel */ },
};

#define BNXT_ETH_RSS_SUPPORT (	\
	ETH_RSS_IPV4 |		\
	ETH_RSS_NONFRAG_IPV4_TCP |	\
	ETH_RSS_NONFRAG_IPV4_UDP |	\
	ETH_RSS_IPV6 |		\
	ETH_RSS_NONFRAG_IPV6_TCP |	\
	ETH_RSS_NONFRAG_IPV6_UDP)

/***********************/

/*
 * High level utility functions
 */

static void bnxt_free_mem(struct bnxt *bp)
{
	bnxt_free_filter_mem(bp);
	bnxt_free_vnic_attributes(bp);
	bnxt_free_vnic_mem(bp);

	bnxt_free_stats(bp);
	bnxt_free_tx_rings(bp);
	bnxt_free_rx_rings(bp);
	bnxt_free_def_cp_ring(bp);
}

static int bnxt_alloc_mem(struct bnxt *bp)
{
	int rc;

	/* Default completion ring */
	rc = bnxt_init_def_ring_struct(bp, SOCKET_ID_ANY);
	if (rc)
		goto alloc_mem_err;

	rc = bnxt_alloc_rings(bp, 0, NULL, NULL,
			      bp->def_cp_ring, "def_cp");
	if (rc)
		goto alloc_mem_err;

	rc = bnxt_alloc_vnic_mem(bp);
	if (rc)
		goto alloc_mem_err;

	rc = bnxt_alloc_vnic_attributes(bp);
	if (rc)
		goto alloc_mem_err;

	rc = bnxt_alloc_filter_mem(bp);
	if (rc)
		goto alloc_mem_err;

	return 0;

alloc_mem_err:
	bnxt_free_mem(bp);
	return rc;
}

static int bnxt_init_chip(struct bnxt *bp)
{
	unsigned int i, rss_idx, fw_idx;
	int rc;

	rc = bnxt_alloc_all_hwrm_stat_ctxs(bp);
	if (rc) {
		RTE_LOG(ERR, PMD, "HWRM stat ctx alloc failure rc: %x\n", rc);
		goto err_out;
	}

	rc = bnxt_alloc_hwrm_rings(bp);
	if (rc) {
		RTE_LOG(ERR, PMD, "HWRM ring alloc failure rc: %x\n", rc);
		goto err_out;
	}

	rc = bnxt_alloc_all_hwrm_ring_grps(bp);
	if (rc) {
		RTE_LOG(ERR, PMD, "HWRM ring grp alloc failure: %x\n", rc);
		goto err_out;
	}

	rc = bnxt_mq_rx_configure(bp);
	if (rc) {
		RTE_LOG(ERR, PMD, "MQ mode configure failure rc: %x\n", rc);
		goto err_out;
	}

	/* VNIC configuration */
	for (i = 0; i < bp->nr_vnics; i++) {
		struct bnxt_vnic_info *vnic = &bp->vnic_info[i];

		rc = bnxt_hwrm_vnic_alloc(bp, vnic);
		if (rc) {
			RTE_LOG(ERR, PMD, "HWRM vnic alloc failure rc: %x\n",
				rc);
			goto err_out;
		}

		rc = bnxt_hwrm_vnic_ctx_alloc(bp, vnic);
		if (rc) {
			RTE_LOG(ERR, PMD,
				"HWRM vnic ctx alloc failure rc: %x\n", rc);
			goto err_out;
		}

		rc = bnxt_hwrm_vnic_cfg(bp, vnic);
		if (rc) {
			RTE_LOG(ERR, PMD, "HWRM vnic cfg failure rc: %x\n", rc);
			goto err_out;
		}

		rc = bnxt_set_hwrm_vnic_filters(bp, vnic);
		if (rc) {
			RTE_LOG(ERR, PMD, "HWRM vnic filter failure rc: %x\n",
				rc);
			goto err_out;
		}
		if (vnic->rss_table && vnic->hash_type) {
			/*
			 * Fill the RSS hash & redirection table with
			 * ring group ids for all VNICs
			 */
			for (rss_idx = 0, fw_idx = 0;
			     rss_idx < HW_HASH_INDEX_SIZE;
			     rss_idx++, fw_idx++) {
				if (vnic->fw_grp_ids[fw_idx] ==
				    INVALID_HW_RING_ID)
					fw_idx = 0;
				vnic->rss_table[rss_idx] =
						vnic->fw_grp_ids[fw_idx];
			}
			rc = bnxt_hwrm_vnic_rss_cfg(bp, vnic);
			if (rc) {
				RTE_LOG(ERR, PMD,
					"HWRM vnic set RSS failure rc: %x\n",
					rc);
				goto err_out;
			}
		}
	}
	rc = bnxt_hwrm_cfa_l2_set_rx_mask(bp, &bp->vnic_info[0]);
	if (rc) {
		RTE_LOG(ERR, PMD,
			"HWRM cfa l2 rx mask failure rc: %x\n", rc);
		goto err_out;
	}

	return 0;

err_out:
	bnxt_free_all_hwrm_resources(bp);

	return rc;
}

static int bnxt_shutdown_nic(struct bnxt *bp)
{
	bnxt_free_all_hwrm_resources(bp);
	bnxt_free_all_filters(bp);
	bnxt_free_all_vnics(bp);
	return 0;
}

static int bnxt_init_nic(struct bnxt *bp)
{
	int rc;

	bnxt_init_ring_grps(bp);
	bnxt_init_vnics(bp);
	bnxt_init_filters(bp);

	rc = bnxt_init_chip(bp);
	if (rc)
		return rc;

	return 0;
}

/*
 * Device configuration and status function
 */

static void bnxt_dev_info_get_op(struct rte_eth_dev *eth_dev,
				  struct rte_eth_dev_info *dev_info)
{
	struct bnxt *bp = (struct bnxt *)eth_dev->data->dev_private;
	uint16_t max_vnics, i, j, vpool, vrxq;

	/* MAC Specifics */
	dev_info->max_mac_addrs = MAX_NUM_MAC_ADDR;
	dev_info->max_hash_mac_addrs = 0;

	/* PF/VF specifics */
	if (BNXT_PF(bp)) {
		dev_info->max_rx_queues = bp->pf.max_rx_rings;
		dev_info->max_tx_queues = bp->pf.max_tx_rings;
		dev_info->max_vfs = bp->pf.active_vfs;
		dev_info->reta_size = bp->pf.max_rsscos_ctx;
		max_vnics = bp->pf.max_vnics;
	} else {
		dev_info->max_rx_queues = bp->vf.max_rx_rings;
		dev_info->max_tx_queues = bp->vf.max_tx_rings;
		dev_info->reta_size = bp->vf.max_rsscos_ctx;
		max_vnics = bp->vf.max_vnics;
	}

	/* Fast path specifics */
	dev_info->min_rx_bufsize = 1;
	dev_info->max_rx_pktlen = BNXT_MAX_MTU + ETHER_HDR_LEN + ETHER_CRC_LEN
				  + VLAN_TAG_SIZE;
	dev_info->rx_offload_capa = 0;
	dev_info->tx_offload_capa = DEV_TX_OFFLOAD_IPV4_CKSUM |
					DEV_TX_OFFLOAD_TCP_CKSUM |
					DEV_TX_OFFLOAD_UDP_CKSUM |
					DEV_TX_OFFLOAD_TCP_TSO;

	/* *INDENT-OFF* */
	dev_info->default_rxconf = (struct rte_eth_rxconf) {
		.rx_thresh = {
			.pthresh = 8,
			.hthresh = 8,
			.wthresh = 0,
		},
		.rx_free_thresh = 32,
		.rx_drop_en = 0,
	};

	dev_info->default_txconf = (struct rte_eth_txconf) {
		.tx_thresh = {
			.pthresh = 32,
			.hthresh = 0,
			.wthresh = 0,
		},
		.tx_free_thresh = 32,
		.tx_rs_thresh = 32,
		.txq_flags = ETH_TXQ_FLAGS_NOMULTSEGS |
			     ETH_TXQ_FLAGS_NOOFFLOADS,
	};
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
}

/* Configure the device based on the configuration provided */
static int bnxt_dev_configure_op(struct rte_eth_dev *eth_dev)
{
	struct bnxt *bp = (struct bnxt *)eth_dev->data->dev_private;
	int rc;

	bp->rx_queues = (void *)eth_dev->data->rx_queues;
	bp->tx_queues = (void *)eth_dev->data->tx_queues;

	/* Inherit new configurations */
	bp->rx_nr_rings = eth_dev->data->nb_rx_queues;
	bp->tx_nr_rings = eth_dev->data->nb_tx_queues;
	bp->rx_cp_nr_rings = bp->rx_nr_rings;
	bp->tx_cp_nr_rings = bp->tx_nr_rings;

	if (eth_dev->data->dev_conf.rxmode.jumbo_frame)
		eth_dev->data->mtu =
				eth_dev->data->dev_conf.rxmode.max_rx_pkt_len -
				ETHER_HDR_LEN - ETHER_CRC_LEN - VLAN_TAG_SIZE;
	rc = bnxt_set_hwrm_link_config(bp, true);
	return rc;
}

static int bnxt_dev_start_op(struct rte_eth_dev *eth_dev)
{
	struct bnxt *bp = (struct bnxt *)eth_dev->data->dev_private;
	int rc;

	rc = bnxt_hwrm_func_reset(bp);
	if (rc) {
		RTE_LOG(ERR, PMD, "hwrm chip reset failure rc: %x\n", rc);
		rc = -1;
		goto error;
	}

	rc = bnxt_alloc_mem(bp);
	if (rc)
		goto error;

	rc = bnxt_init_nic(bp);
	if (rc)
		goto error;

	return 0;

error:
	bnxt_shutdown_nic(bp);
	bnxt_free_tx_mbufs(bp);
	bnxt_free_rx_mbufs(bp);
	bnxt_free_mem(bp);
	return rc;
}

static int bnxt_dev_set_link_up_op(struct rte_eth_dev *eth_dev)
{
	struct bnxt *bp = (struct bnxt *)eth_dev->data->dev_private;

	eth_dev->data->dev_link.link_status = 1;
	bnxt_set_hwrm_link_config(bp, true);
	return 0;
}

static int bnxt_dev_set_link_down_op(struct rte_eth_dev *eth_dev)
{
	struct bnxt *bp = (struct bnxt *)eth_dev->data->dev_private;

	eth_dev->data->dev_link.link_status = 0;
	bnxt_set_hwrm_link_config(bp, false);
	return 0;
}

static void bnxt_dev_close_op(struct rte_eth_dev *eth_dev)
{
	struct bnxt *bp = (struct bnxt *)eth_dev->data->dev_private;

	bnxt_free_tx_mbufs(bp);
	bnxt_free_rx_mbufs(bp);
	bnxt_free_mem(bp);
	rte_free(eth_dev->data->mac_addrs);
}

/* Unload the driver, release resources */
static void bnxt_dev_stop_op(struct rte_eth_dev *eth_dev)
{
	struct bnxt *bp = (struct bnxt *)eth_dev->data->dev_private;

	if (bp->eth_dev->data->dev_started) {
		/* TBD: STOP HW queues DMA */
		eth_dev->data->dev_link.link_status = 0;
	}
	bnxt_shutdown_nic(bp);
}

static void bnxt_mac_addr_remove_op(struct rte_eth_dev *eth_dev,
				    uint32_t index)
{
	struct bnxt *bp = (struct bnxt *)eth_dev->data->dev_private;
	uint64_t pool_mask = eth_dev->data->mac_pool_sel[index];
	struct bnxt_vnic_info *vnic;
	struct bnxt_filter_info *filter, *temp_filter;
	int i;

	/*
	 * Loop through all VNICs from the specified filter flow pools to
	 * remove the corresponding MAC addr filter
	 */
	for (i = 0; i < MAX_FF_POOLS; i++) {
		if (!(pool_mask & (1 << i)))
			continue;

		STAILQ_FOREACH(vnic, &bp->ff_pool[i], next) {
			filter = STAILQ_FIRST(&vnic->filter);
			while (filter) {
				temp_filter = STAILQ_NEXT(filter, next);
				if (filter->mac_index == index) {
					STAILQ_REMOVE(&vnic->filter, filter,
						      bnxt_filter_info, next);
					bnxt_hwrm_clear_filter(bp, filter);
					filter->mac_index = INVALID_MAC_INDEX;
					memset(&filter->l2_addr, 0,
					       ETHER_ADDR_LEN);
					STAILQ_INSERT_TAIL(
							&bp->free_filter_list,
							filter, next);
				}
				filter = temp_filter;
			}
		}
	}
}

static void bnxt_mac_addr_add_op(struct rte_eth_dev *eth_dev,
				 struct ether_addr *mac_addr,
				 uint32_t index, uint32_t pool)
{
	struct bnxt *bp = (struct bnxt *)eth_dev->data->dev_private;
	struct bnxt_vnic_info *vnic = STAILQ_FIRST(&bp->ff_pool[pool]);
	struct bnxt_filter_info *filter;

	if (!vnic) {
		RTE_LOG(ERR, PMD, "VNIC not found for pool %d!\n", pool);
		return;
	}
	/* Attach requested MAC address to the new l2_filter */
	STAILQ_FOREACH(filter, &vnic->filter, next) {
		if (filter->mac_index == index) {
			RTE_LOG(ERR, PMD,
				"MAC addr already existed for pool %d\n", pool);
			return;
		}
	}
	filter = bnxt_alloc_filter(bp);
	if (!filter) {
		RTE_LOG(ERR, PMD, "L2 filter alloc failed\n");
		return;
	}
	STAILQ_INSERT_TAIL(&vnic->filter, filter, next);
	filter->mac_index = index;
	memcpy(filter->l2_addr, mac_addr, ETHER_ADDR_LEN);
	bnxt_hwrm_set_filter(bp, vnic, filter);
}

static int bnxt_link_update_op(struct rte_eth_dev *eth_dev,
			       int wait_to_complete)
{
	int rc = 0;
	struct bnxt *bp = (struct bnxt *)eth_dev->data->dev_private;
	struct rte_eth_link new;
	unsigned int cnt = BNXT_LINK_WAIT_CNT;

	memset(&new, 0, sizeof(new));
	do {
		/* Retrieve link info from hardware */
		rc = bnxt_get_hwrm_link_config(bp, &new);
		if (rc) {
			new.link_speed = ETH_LINK_SPEED_100M;
			new.link_duplex = ETH_LINK_FULL_DUPLEX;
			RTE_LOG(ERR, PMD,
				"Failed to retrieve link rc = 0x%x!", rc);
			goto out;
		}
		if (!wait_to_complete)
			break;

		rte_delay_ms(BNXT_LINK_WAIT_INTERVAL);

	} while (!new.link_status && cnt--);

	/* Timed out or success */
	if (new.link_status) {
		/* Update only if success */
		eth_dev->data->dev_link.link_duplex = new.link_duplex;
		eth_dev->data->dev_link.link_speed = new.link_speed;
	}
	eth_dev->data->dev_link.link_status = new.link_status;
out:
	return rc;
}

static void bnxt_promiscuous_enable_op(struct rte_eth_dev *eth_dev)
{
	struct bnxt *bp = (struct bnxt *)eth_dev->data->dev_private;
	struct bnxt_vnic_info *vnic;

	if (bp->vnic_info == NULL)
		return;

	vnic = &bp->vnic_info[0];

	vnic->flags |= BNXT_VNIC_INFO_PROMISC;
	bnxt_hwrm_cfa_l2_set_rx_mask(bp, vnic);
}

static void bnxt_promiscuous_disable_op(struct rte_eth_dev *eth_dev)
{
	struct bnxt *bp = (struct bnxt *)eth_dev->data->dev_private;
	struct bnxt_vnic_info *vnic;

	if (bp->vnic_info == NULL)
		return;

	vnic = &bp->vnic_info[0];

	vnic->flags &= ~BNXT_VNIC_INFO_PROMISC;
	bnxt_hwrm_cfa_l2_set_rx_mask(bp, vnic);
}

static void bnxt_allmulticast_enable_op(struct rte_eth_dev *eth_dev)
{
	struct bnxt *bp = (struct bnxt *)eth_dev->data->dev_private;
	struct bnxt_vnic_info *vnic;

	if (bp->vnic_info == NULL)
		return;

	vnic = &bp->vnic_info[0];

	vnic->flags |= BNXT_VNIC_INFO_ALLMULTI;
	bnxt_hwrm_cfa_l2_set_rx_mask(bp, vnic);
}

static void bnxt_allmulticast_disable_op(struct rte_eth_dev *eth_dev)
{
	struct bnxt *bp = (struct bnxt *)eth_dev->data->dev_private;
	struct bnxt_vnic_info *vnic;

	if (bp->vnic_info == NULL)
		return;

	vnic = &bp->vnic_info[0];

	vnic->flags &= ~BNXT_VNIC_INFO_ALLMULTI;
	bnxt_hwrm_cfa_l2_set_rx_mask(bp, vnic);
}

static int bnxt_reta_update_op(struct rte_eth_dev *eth_dev,
			    struct rte_eth_rss_reta_entry64 *reta_conf,
			    uint16_t reta_size)
{
	struct bnxt *bp = (struct bnxt *)eth_dev->data->dev_private;
	struct rte_eth_conf *dev_conf = &bp->eth_dev->data->dev_conf;
	struct bnxt_vnic_info *vnic;
	int i;

	if (!(dev_conf->rxmode.mq_mode & ETH_MQ_RX_RSS_FLAG))
		return -EINVAL;

	if (reta_size != HW_HASH_INDEX_SIZE) {
		RTE_LOG(ERR, PMD, "The configured hash table lookup size "
			"(%d) must equal the size supported by the hardware "
			"(%d)\n", reta_size, HW_HASH_INDEX_SIZE);
		return -EINVAL;
	}
	/* Update the RSS VNIC(s) */
	for (i = 0; i < MAX_FF_POOLS; i++) {
		STAILQ_FOREACH(vnic, &bp->ff_pool[i], next) {
			memcpy(vnic->rss_table, reta_conf, reta_size);

			bnxt_hwrm_vnic_rss_cfg(bp, vnic);
		}
	}
	return 0;
}

static int bnxt_reta_query_op(struct rte_eth_dev *eth_dev,
			      struct rte_eth_rss_reta_entry64 *reta_conf,
			      uint16_t reta_size)
{
	struct bnxt *bp = (struct bnxt *)eth_dev->data->dev_private;
	struct bnxt_vnic_info *vnic = &bp->vnic_info[0];

	/* Retrieve from the default VNIC */
	if (!vnic)
		return -EINVAL;
	if (!vnic->rss_table)
		return -EINVAL;

	if (reta_size != HW_HASH_INDEX_SIZE) {
		RTE_LOG(ERR, PMD, "The configured hash table lookup size "
			"(%d) must equal the size supported by the hardware "
			"(%d)\n", reta_size, HW_HASH_INDEX_SIZE);
		return -EINVAL;
	}
	/* EW - need to revisit here copying from u64 to u16 */
	memcpy(reta_conf, vnic->rss_table, reta_size);

	return 0;
}

static int bnxt_rss_hash_update_op(struct rte_eth_dev *eth_dev,
				   struct rte_eth_rss_conf *rss_conf)
{
	struct bnxt *bp = (struct bnxt *)eth_dev->data->dev_private;
	struct rte_eth_conf *dev_conf = &bp->eth_dev->data->dev_conf;
	struct bnxt_vnic_info *vnic;
	uint16_t hash_type = 0;
	int i;

	/*
	 * If RSS enablement were different than dev_configure,
	 * then return -EINVAL
	 */
	if (dev_conf->rxmode.mq_mode & ETH_MQ_RX_RSS_FLAG) {
		if (!rss_conf->rss_hf)
			return -EINVAL;
	} else {
		if (rss_conf->rss_hf & BNXT_ETH_RSS_SUPPORT)
			return -EINVAL;
	}
	if (rss_conf->rss_hf & ETH_RSS_IPV4)
		hash_type |= HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_IPV4;
	if (rss_conf->rss_hf & ETH_RSS_NONFRAG_IPV4_TCP)
		hash_type |= HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_TCP_IPV4;
	if (rss_conf->rss_hf & ETH_RSS_NONFRAG_IPV4_UDP)
		hash_type |= HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_UDP_IPV4;
	if (rss_conf->rss_hf & ETH_RSS_IPV6)
		hash_type |= HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_IPV6;
	if (rss_conf->rss_hf & ETH_RSS_NONFRAG_IPV6_TCP)
		hash_type |= HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_TCP_IPV6;
	if (rss_conf->rss_hf & ETH_RSS_NONFRAG_IPV6_UDP)
		hash_type |= HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_UDP_IPV6;

	/* Update the RSS VNIC(s) */
	for (i = 0; i < MAX_FF_POOLS; i++) {
		STAILQ_FOREACH(vnic, &bp->ff_pool[i], next) {
			vnic->hash_type = hash_type;

			/*
			 * Use the supplied key if the key length is
			 * acceptable and the rss_key is not NULL
			 */
			if (rss_conf->rss_key &&
			    rss_conf->rss_key_len <= HW_HASH_KEY_SIZE)
				memcpy(vnic->rss_hash_key, rss_conf->rss_key,
				       rss_conf->rss_key_len);

			bnxt_hwrm_vnic_rss_cfg(bp, vnic);
		}
	}
	return 0;
}

static int bnxt_rss_hash_conf_get_op(struct rte_eth_dev *eth_dev,
				     struct rte_eth_rss_conf *rss_conf)
{
	struct bnxt *bp = (struct bnxt *)eth_dev->data->dev_private;
	struct bnxt_vnic_info *vnic = &bp->vnic_info[0];
	int len;
	uint32_t hash_types;

	/* RSS configuration is the same for all VNICs */
	if (vnic && vnic->rss_hash_key) {
		if (rss_conf->rss_key) {
			len = rss_conf->rss_key_len <= HW_HASH_KEY_SIZE ?
			      rss_conf->rss_key_len : HW_HASH_KEY_SIZE;
			memcpy(rss_conf->rss_key, vnic->rss_hash_key, len);
		}

		hash_types = vnic->hash_type;
		rss_conf->rss_hf = 0;
		if (hash_types & HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_IPV4) {
			rss_conf->rss_hf |= ETH_RSS_IPV4;
			hash_types &= ~HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_IPV4;
		}
		if (hash_types & HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_TCP_IPV4) {
			rss_conf->rss_hf |= ETH_RSS_NONFRAG_IPV4_TCP;
			hash_types &=
				~HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_TCP_IPV4;
		}
		if (hash_types & HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_UDP_IPV4) {
			rss_conf->rss_hf |= ETH_RSS_NONFRAG_IPV4_UDP;
			hash_types &=
				~HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_UDP_IPV4;
		}
		if (hash_types & HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_IPV6) {
			rss_conf->rss_hf |= ETH_RSS_IPV6;
			hash_types &= ~HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_IPV6;
		}
		if (hash_types & HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_TCP_IPV6) {
			rss_conf->rss_hf |= ETH_RSS_NONFRAG_IPV6_TCP;
			hash_types &=
				~HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_TCP_IPV6;
		}
		if (hash_types & HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_UDP_IPV6) {
			rss_conf->rss_hf |= ETH_RSS_NONFRAG_IPV6_UDP;
			hash_types &=
				~HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_UDP_IPV6;
		}
		if (hash_types) {
			RTE_LOG(ERR, PMD,
				"Unknwon RSS config from firmware (%08x), RSS disabled",
				vnic->hash_type);
			return -ENOTSUP;
		}
	} else {
		rss_conf->rss_hf = 0;
	}
	return 0;
}

static int bnxt_flow_ctrl_get_op(struct rte_eth_dev *dev,
			       struct rte_eth_fc_conf *fc_conf __rte_unused)
{
	struct bnxt *bp = (struct bnxt *)dev->data->dev_private;
	struct rte_eth_link link_info;
	int rc;

	rc = bnxt_get_hwrm_link_config(bp, &link_info);
	if (rc)
		return rc;

	memset(fc_conf, 0, sizeof(*fc_conf));
	if (bp->link_info.auto_pause)
		fc_conf->autoneg = 1;
	switch (bp->link_info.pause) {
	case 0:
		fc_conf->mode = RTE_FC_NONE;
		break;
	case HWRM_PORT_PHY_QCFG_OUTPUT_PAUSE_TX:
		fc_conf->mode = RTE_FC_TX_PAUSE;
		break;
	case HWRM_PORT_PHY_QCFG_OUTPUT_PAUSE_RX:
		fc_conf->mode = RTE_FC_RX_PAUSE;
		break;
	case (HWRM_PORT_PHY_QCFG_OUTPUT_PAUSE_TX |
			HWRM_PORT_PHY_QCFG_OUTPUT_PAUSE_RX):
		fc_conf->mode = RTE_FC_FULL;
		break;
	}
	return 0;
}

static int bnxt_flow_ctrl_set_op(struct rte_eth_dev *dev,
			       struct rte_eth_fc_conf *fc_conf)
{
	struct bnxt *bp = (struct bnxt *)dev->data->dev_private;

	switch (fc_conf->mode) {
	case RTE_FC_NONE:
		bp->link_info.auto_pause = 0;
		bp->link_info.force_pause = 0;
		break;
	case RTE_FC_RX_PAUSE:
		if (fc_conf->autoneg) {
			bp->link_info.auto_pause =
					HWRM_PORT_PHY_CFG_INPUT_AUTO_PAUSE_RX;
			bp->link_info.force_pause = 0;
		} else {
			bp->link_info.auto_pause = 0;
			bp->link_info.force_pause =
					HWRM_PORT_PHY_CFG_INPUT_FORCE_PAUSE_RX;
		}
		break;
	case RTE_FC_TX_PAUSE:
		if (fc_conf->autoneg) {
			bp->link_info.auto_pause =
					HWRM_PORT_PHY_CFG_INPUT_AUTO_PAUSE_TX;
			bp->link_info.force_pause = 0;
		} else {
			bp->link_info.auto_pause = 0;
			bp->link_info.force_pause =
					HWRM_PORT_PHY_CFG_INPUT_FORCE_PAUSE_TX;
		}
		break;
	case RTE_FC_FULL:
		if (fc_conf->autoneg) {
			bp->link_info.auto_pause =
					HWRM_PORT_PHY_CFG_INPUT_AUTO_PAUSE_TX |
					HWRM_PORT_PHY_CFG_INPUT_AUTO_PAUSE_RX;
			bp->link_info.force_pause = 0;
		} else {
			bp->link_info.auto_pause = 0;
			bp->link_info.force_pause =
					HWRM_PORT_PHY_CFG_INPUT_FORCE_PAUSE_TX |
					HWRM_PORT_PHY_CFG_INPUT_FORCE_PAUSE_RX;
		}
		break;
	}
	return bnxt_set_hwrm_link_config(bp, true);
}

/*
 * Initialization
 */

static struct eth_dev_ops bnxt_dev_ops = {
	.dev_infos_get = bnxt_dev_info_get_op,
	.dev_close = bnxt_dev_close_op,
	.dev_configure = bnxt_dev_configure_op,
	.dev_start = bnxt_dev_start_op,
	.dev_stop = bnxt_dev_stop_op,
	.dev_set_link_up = bnxt_dev_set_link_up_op,
	.dev_set_link_down = bnxt_dev_set_link_down_op,
	.stats_get = bnxt_stats_get_op,
	.stats_reset = bnxt_stats_reset_op,
	.rx_queue_setup = bnxt_rx_queue_setup_op,
	.rx_queue_release = bnxt_rx_queue_release_op,
	.tx_queue_setup = bnxt_tx_queue_setup_op,
	.tx_queue_release = bnxt_tx_queue_release_op,
	.reta_update = bnxt_reta_update_op,
	.reta_query = bnxt_reta_query_op,
	.rss_hash_update = bnxt_rss_hash_update_op,
	.rss_hash_conf_get = bnxt_rss_hash_conf_get_op,
	.link_update = bnxt_link_update_op,
	.promiscuous_enable = bnxt_promiscuous_enable_op,
	.promiscuous_disable = bnxt_promiscuous_disable_op,
	.allmulticast_enable = bnxt_allmulticast_enable_op,
	.allmulticast_disable = bnxt_allmulticast_disable_op,
	.mac_addr_add = bnxt_mac_addr_add_op,
	.mac_addr_remove = bnxt_mac_addr_remove_op,
	.flow_ctrl_get = bnxt_flow_ctrl_get_op,
	.flow_ctrl_set = bnxt_flow_ctrl_set_op,
};

static bool bnxt_vf_pciid(uint16_t id)
{
	if (id == BROADCOM_DEV_ID_57304_VF ||
	    id == BROADCOM_DEV_ID_57406_VF)
		return true;
	return false;
}

static int bnxt_init_board(struct rte_eth_dev *eth_dev)
{
	int rc;
	struct bnxt *bp = eth_dev->data->dev_private;

	/* enable device (incl. PCI PM wakeup), and bus-mastering */
	if (!eth_dev->pci_dev->mem_resource[0].addr) {
		RTE_LOG(ERR, PMD,
			"Cannot find PCI device base address, aborting\n");
		rc = -ENODEV;
		goto init_err_disable;
	}

	bp->eth_dev = eth_dev;
	bp->pdev = eth_dev->pci_dev;

	bp->bar0 = (void *)eth_dev->pci_dev->mem_resource[0].addr;
	if (!bp->bar0) {
		RTE_LOG(ERR, PMD, "Cannot map device registers, aborting\n");
		rc = -ENOMEM;
		goto init_err_release;
	}
	return 0;

init_err_release:
	if (bp->bar0)
		bp->bar0 = NULL;

init_err_disable:

	return rc;
}

static int
bnxt_dev_init(struct rte_eth_dev *eth_dev)
{
	static int version_printed;
	struct bnxt *bp;
	int rc;

	if (version_printed++ == 0)
		RTE_LOG(INFO, PMD, "%s", bnxt_version);

	if (eth_dev->pci_dev->addr.function >= 2 &&
			eth_dev->pci_dev->addr.function < 4) {
		RTE_LOG(ERR, PMD, "Function not enabled %x:\n",
			eth_dev->pci_dev->addr.function);
		rc = -ENOMEM;
		goto error;
	}

	rte_eth_copy_pci_info(eth_dev, eth_dev->pci_dev);
	bp = eth_dev->data->dev_private;

	if (bnxt_vf_pciid(eth_dev->pci_dev->id.device_id))
		bp->flags |= BNXT_FLAG_VF;

	rc = bnxt_init_board(eth_dev);
	if (rc) {
		RTE_LOG(ERR, PMD,
			"Board initialization failed rc: %x\n", rc);
		goto error;
	}
	eth_dev->dev_ops = &bnxt_dev_ops;
	eth_dev->rx_pkt_burst = &bnxt_recv_pkts;
	eth_dev->tx_pkt_burst = &bnxt_xmit_pkts;

	rc = bnxt_alloc_hwrm_resources(bp);
	if (rc) {
		RTE_LOG(ERR, PMD,
			"hwrm resource allocation failure rc: %x\n", rc);
		goto error_free;
	}
	rc = bnxt_hwrm_ver_get(bp);
	if (rc)
		goto error_free;
	bnxt_hwrm_queue_qportcfg(bp);

	/* Get the MAX capabilities for this function */
	rc = bnxt_hwrm_func_qcaps(bp);
	if (rc) {
		RTE_LOG(ERR, PMD, "hwrm query capability failure rc: %x\n", rc);
		goto error_free;
	}
	eth_dev->data->mac_addrs = rte_zmalloc("bnxt_mac_addr_tbl",
					ETHER_ADDR_LEN * MAX_NUM_MAC_ADDR, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		RTE_LOG(ERR, PMD,
			"Failed to alloc %u bytes needed to store MAC addr tbl",
			ETHER_ADDR_LEN * MAX_NUM_MAC_ADDR);
		rc = -ENOMEM;
		goto error_free;
	}
	/* Copy the permanent MAC from the qcap response address now. */
	if (BNXT_PF(bp))
		memcpy(bp->mac_addr, bp->pf.mac_addr, sizeof(bp->mac_addr));
	else
		memcpy(bp->mac_addr, bp->vf.mac_addr, sizeof(bp->mac_addr));
	memcpy(&eth_dev->data->mac_addrs[0], bp->mac_addr, ETHER_ADDR_LEN);
	bp->grp_info = rte_zmalloc("bnxt_grp_info",
				sizeof(*bp->grp_info) * bp->max_ring_grps, 0);
	if (!bp->grp_info) {
		RTE_LOG(ERR, PMD,
			"Failed to alloc %zu bytes needed to store group info table\n",
			sizeof(*bp->grp_info) * bp->max_ring_grps);
		rc = -ENOMEM;
		goto error_free;
	}

	rc = bnxt_hwrm_func_driver_register(bp, 0,
					    bp->pf.vf_req_fwd);
	if (rc) {
		RTE_LOG(ERR, PMD,
			"Failed to register driver");
		rc = -EBUSY;
		goto error_free;
	}

	RTE_LOG(INFO, PMD,
		DRV_MODULE_NAME " found at mem %" PRIx64 ", node addr %pM\n",
		eth_dev->pci_dev->mem_resource[0].phys_addr,
		eth_dev->pci_dev->mem_resource[0].addr);

	return 0;

error_free:
	eth_dev->driver->eth_dev_uninit(eth_dev);
error:
	return rc;
}

static int
bnxt_dev_uninit(struct rte_eth_dev *eth_dev) {
	struct bnxt *bp = eth_dev->data->dev_private;
	int rc;

	if (eth_dev->data->mac_addrs)
		rte_free(eth_dev->data->mac_addrs);
	if (bp->grp_info)
		rte_free(bp->grp_info);
	rc = bnxt_hwrm_func_driver_unregister(bp, 0);
	bnxt_free_hwrm_resources(bp);
	return rc;
}

static struct eth_driver bnxt_rte_pmd = {
	.pci_drv = {
		    .name = "rte_" DRV_MODULE_NAME "_pmd",
		    .id_table = bnxt_pci_id_map,
		    .drv_flags = RTE_PCI_DRV_NEED_MAPPING,
		    },
	.eth_dev_init = bnxt_dev_init,
	.eth_dev_uninit = bnxt_dev_uninit,
	.dev_private_size = sizeof(struct bnxt),
};

static int bnxt_rte_pmd_init(const char *name, const char *params __rte_unused)
{
	RTE_LOG(INFO, PMD, "bnxt_rte_pmd_init() called for %s\n", name);
	rte_eth_driver_register(&bnxt_rte_pmd);
	return 0;
}

static struct rte_driver bnxt_pmd_drv = {
	.type = PMD_PDEV,
	.init = bnxt_rte_pmd_init,
};

PMD_REGISTER_DRIVER(bnxt_pmd_drv, bnxt);
DRIVER_REGISTER_PCI_TABLE(bnxt, bnxt_pci_id_map);
