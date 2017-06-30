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
#include <rte_ethdev_pci.h>
#include <rte_malloc.h>
#include <rte_cycles.h>

#include "bnxt.h"
#include "bnxt_cpr.h"
#include "bnxt_filter.h"
#include "bnxt_hwrm.h"
#include "bnxt_irq.h"
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

#define BROADCOM_DEV_ID_STRATUS_NIC_VF 0x1609
#define BROADCOM_DEV_ID_STRATUS_NIC 0x1614
#define BROADCOM_DEV_ID_57414_VF 0x16c1
#define BROADCOM_DEV_ID_57301 0x16c8
#define BROADCOM_DEV_ID_57302 0x16c9
#define BROADCOM_DEV_ID_57304_PF 0x16ca
#define BROADCOM_DEV_ID_57304_VF 0x16cb
#define BROADCOM_DEV_ID_57417_MF 0x16cc
#define BROADCOM_DEV_ID_NS2 0x16cd
#define BROADCOM_DEV_ID_57311 0x16ce
#define BROADCOM_DEV_ID_57312 0x16cf
#define BROADCOM_DEV_ID_57402 0x16d0
#define BROADCOM_DEV_ID_57404 0x16d1
#define BROADCOM_DEV_ID_57406_PF 0x16d2
#define BROADCOM_DEV_ID_57406_VF 0x16d3
#define BROADCOM_DEV_ID_57402_MF 0x16d4
#define BROADCOM_DEV_ID_57407_RJ45 0x16d5
#define BROADCOM_DEV_ID_57412 0x16d6
#define BROADCOM_DEV_ID_57414 0x16d7
#define BROADCOM_DEV_ID_57416_RJ45 0x16d8
#define BROADCOM_DEV_ID_57417_RJ45 0x16d9
#define BROADCOM_DEV_ID_5741X_VF 0x16dc
#define BROADCOM_DEV_ID_57412_MF 0x16de
#define BROADCOM_DEV_ID_57314 0x16df
#define BROADCOM_DEV_ID_57317_RJ45 0x16e0
#define BROADCOM_DEV_ID_5731X_VF 0x16e1
#define BROADCOM_DEV_ID_57417_SFP 0x16e2
#define BROADCOM_DEV_ID_57416_SFP 0x16e3
#define BROADCOM_DEV_ID_57317_SFP 0x16e4
#define BROADCOM_DEV_ID_57404_MF 0x16e7
#define BROADCOM_DEV_ID_57406_MF 0x16e8
#define BROADCOM_DEV_ID_57407_SFP 0x16e9
#define BROADCOM_DEV_ID_57407_MF 0x16ea
#define BROADCOM_DEV_ID_57414_MF 0x16ec
#define BROADCOM_DEV_ID_57416_MF 0x16ee

static const struct rte_pci_id bnxt_pci_id_map[] = {
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM,
			 BROADCOM_DEV_ID_STRATUS_NIC_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_STRATUS_NIC) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_57414_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_57301) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_57302) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_57304_PF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_57304_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_NS2) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_57402) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_57404) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_57406_PF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_57406_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_57402_MF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_57407_RJ45) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_57404_MF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_57406_MF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_57407_SFP) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_57407_MF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_5741X_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_5731X_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_57314) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_57417_MF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_57311) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_57312) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_57412) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_57414) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_57416_RJ45) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_57417_RJ45) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_57412_MF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_57317_RJ45) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_57417_SFP) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_57416_SFP) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_57317_SFP) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_57414_MF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_57416_MF) },
	{ .vendor_id = 0, /* sentinel */ },
};

#define BNXT_ETH_RSS_SUPPORT (	\
	ETH_RSS_IPV4 |		\
	ETH_RSS_NONFRAG_IPV4_TCP |	\
	ETH_RSS_NONFRAG_IPV4_UDP |	\
	ETH_RSS_IPV6 |		\
	ETH_RSS_NONFRAG_IPV6_TCP |	\
	ETH_RSS_NONFRAG_IPV6_UDP)

static void bnxt_vlan_offload_set_op(struct rte_eth_dev *dev, int mask);

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
	struct rte_eth_link new;
	int rc;

	if (bp->eth_dev->data->mtu > ETHER_MTU) {
		bp->eth_dev->data->dev_conf.rxmode.jumbo_frame = 1;
		bp->flags |= BNXT_FLAG_JUMBO;
	} else {
		bp->eth_dev->data->dev_conf.rxmode.jumbo_frame = 0;
		bp->flags &= ~BNXT_FLAG_JUMBO;
	}

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
			RTE_LOG(ERR, PMD, "HWRM vnic %d alloc failure rc: %x\n",
				i, rc);
			goto err_out;
		}

		rc = bnxt_hwrm_vnic_ctx_alloc(bp, vnic);
		if (rc) {
			RTE_LOG(ERR, PMD,
				"HWRM vnic %d ctx alloc failure rc: %x\n",
				i, rc);
			goto err_out;
		}

		rc = bnxt_hwrm_vnic_cfg(bp, vnic);
		if (rc) {
			RTE_LOG(ERR, PMD, "HWRM vnic %d cfg failure rc: %x\n",
				i, rc);
			goto err_out;
		}

		rc = bnxt_set_hwrm_vnic_filters(bp, vnic);
		if (rc) {
			RTE_LOG(ERR, PMD,
				"HWRM vnic %d filter failure rc: %x\n",
				i, rc);
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
					"HWRM vnic %d set RSS failure rc: %x\n",
					i, rc);
				goto err_out;
			}
		}

		bnxt_hwrm_vnic_plcmode_cfg(bp, vnic);

		if (bp->eth_dev->data->dev_conf.rxmode.enable_lro)
			bnxt_hwrm_vnic_tpa_cfg(bp, vnic, 1);
		else
			bnxt_hwrm_vnic_tpa_cfg(bp, vnic, 0);
	}
	rc = bnxt_hwrm_cfa_l2_set_rx_mask(bp, &bp->vnic_info[0], 0, NULL);
	if (rc) {
		RTE_LOG(ERR, PMD,
			"HWRM cfa l2 rx mask failure rc: %x\n", rc);
		goto err_out;
	}

	rc = bnxt_get_hwrm_link_config(bp, &new);
	if (rc) {
		RTE_LOG(ERR, PMD, "HWRM Get link config failure rc: %x\n", rc);
		goto err_out;
	}

	if (!bp->link_info.link_up) {
		rc = bnxt_set_hwrm_link_config(bp, true);
		if (rc) {
			RTE_LOG(ERR, PMD,
				"HWRM link config failure rc: %x\n", rc);
			goto err_out;
		}
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

	dev_info->pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);

	/* MAC Specifics */
	dev_info->max_mac_addrs = MAX_NUM_MAC_ADDR;
	dev_info->max_hash_mac_addrs = 0;

	/* PF/VF specifics */
	if (BNXT_PF(bp))
		dev_info->max_vfs = bp->pdev->max_vfs;
	dev_info->max_rx_queues = bp->max_rx_rings;
	dev_info->max_tx_queues = bp->max_tx_rings;
	dev_info->reta_size = bp->max_rsscos_ctx;
	max_vnics = bp->max_vnics;

	/* Fast path specifics */
	dev_info->min_rx_bufsize = 1;
	dev_info->max_rx_pktlen = BNXT_MAX_MTU + ETHER_HDR_LEN + ETHER_CRC_LEN
				  + VLAN_TAG_SIZE;
	dev_info->rx_offload_capa = 0;
	dev_info->tx_offload_capa = DEV_TX_OFFLOAD_IPV4_CKSUM |
					DEV_TX_OFFLOAD_TCP_CKSUM |
					DEV_TX_OFFLOAD_UDP_CKSUM |
					DEV_TX_OFFLOAD_TCP_TSO |
					DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM |
					DEV_TX_OFFLOAD_VXLAN_TNL_TSO |
					DEV_TX_OFFLOAD_GRE_TNL_TSO |
					DEV_TX_OFFLOAD_IPIP_TNL_TSO |
					DEV_TX_OFFLOAD_GENEVE_TNL_TSO;

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
	eth_dev->data->dev_conf.intr_conf.lsc = 1;

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
	return 0;
}

static inline int
rte_bnxt_atomic_write_link_status(struct rte_eth_dev *eth_dev,
				struct rte_eth_link *link)
{
	struct rte_eth_link *dst = &eth_dev->data->dev_link;
	struct rte_eth_link *src = link;

	if (rte_atomic64_cmpset((uint64_t *)dst, *(uint64_t *)dst,
					*(uint64_t *)src) == 0)
		return 1;

	return 0;
}

static void bnxt_print_link_info(struct rte_eth_dev *eth_dev)
{
	struct rte_eth_link *link = &eth_dev->data->dev_link;

	if (link->link_status)
		RTE_LOG(INFO, PMD, "Port %d Link Up - speed %u Mbps - %s\n",
			(uint8_t)(eth_dev->data->port_id),
			(uint32_t)link->link_speed,
			(link->link_duplex == ETH_LINK_FULL_DUPLEX) ?
			("full-duplex") : ("half-duplex\n"));
	else
		RTE_LOG(INFO, PMD, "Port %d Link Down\n",
			(uint8_t)(eth_dev->data->port_id));
}

static int bnxt_dev_lsc_intr_setup(struct rte_eth_dev *eth_dev)
{
	bnxt_print_link_info(eth_dev);
	return 0;
}

static int bnxt_dev_start_op(struct rte_eth_dev *eth_dev)
{
	struct bnxt *bp = (struct bnxt *)eth_dev->data->dev_private;
	int vlan_mask = 0;
	int rc;

	bp->dev_stopped = 0;

	rc = bnxt_init_nic(bp);
	if (rc)
		goto error;

	bnxt_link_update_op(eth_dev, 0);

	if (eth_dev->data->dev_conf.rxmode.hw_vlan_filter)
		vlan_mask |= ETH_VLAN_FILTER_MASK;
	if (eth_dev->data->dev_conf.rxmode.hw_vlan_strip)
		vlan_mask |= ETH_VLAN_STRIP_MASK;
	bnxt_vlan_offload_set_op(eth_dev, vlan_mask);

	return 0;

error:
	bnxt_shutdown_nic(bp);
	bnxt_free_tx_mbufs(bp);
	bnxt_free_rx_mbufs(bp);
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

/* Unload the driver, release resources */
static void bnxt_dev_stop_op(struct rte_eth_dev *eth_dev)
{
	struct bnxt *bp = (struct bnxt *)eth_dev->data->dev_private;

	if (bp->eth_dev->data->dev_started) {
		/* TBD: STOP HW queues DMA */
		eth_dev->data->dev_link.link_status = 0;
	}
	bnxt_set_hwrm_link_config(bp, false);
	bnxt_hwrm_port_clr_stats(bp);
	bnxt_shutdown_nic(bp);
	bp->dev_stopped = 1;
}

static void bnxt_dev_close_op(struct rte_eth_dev *eth_dev)
{
	struct bnxt *bp = (struct bnxt *)eth_dev->data->dev_private;

	if (bp->dev_stopped == 0)
		bnxt_dev_stop_op(eth_dev);

	bnxt_free_tx_mbufs(bp);
	bnxt_free_rx_mbufs(bp);
	bnxt_free_mem(bp);
	if (eth_dev->data->mac_addrs != NULL) {
		rte_free(eth_dev->data->mac_addrs);
		eth_dev->data->mac_addrs = NULL;
	}
	if (bp->grp_info != NULL) {
		rte_free(bp->grp_info);
		bp->grp_info = NULL;
	}
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
		if (!(pool_mask & (1ULL << i)))
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

static int bnxt_mac_addr_add_op(struct rte_eth_dev *eth_dev,
				struct ether_addr *mac_addr,
				uint32_t index, uint32_t pool)
{
	struct bnxt *bp = (struct bnxt *)eth_dev->data->dev_private;
	struct bnxt_vnic_info *vnic = STAILQ_FIRST(&bp->ff_pool[pool]);
	struct bnxt_filter_info *filter;

	if (BNXT_VF(bp)) {
		RTE_LOG(ERR, PMD, "Cannot add MAC address to a VF interface\n");
		return -ENOTSUP;
	}

	if (!vnic) {
		RTE_LOG(ERR, PMD, "VNIC not found for pool %d!\n", pool);
		return -EINVAL;
	}
	/* Attach requested MAC address to the new l2_filter */
	STAILQ_FOREACH(filter, &vnic->filter, next) {
		if (filter->mac_index == index) {
			RTE_LOG(ERR, PMD,
				"MAC addr already existed for pool %d\n", pool);
			return -EINVAL;
		}
	}
	filter = bnxt_alloc_filter(bp);
	if (!filter) {
		RTE_LOG(ERR, PMD, "L2 filter alloc failed\n");
		return -ENODEV;
	}
	STAILQ_INSERT_TAIL(&vnic->filter, filter, next);
	filter->mac_index = index;
	memcpy(filter->l2_addr, mac_addr, ETHER_ADDR_LEN);
	return bnxt_hwrm_set_filter(bp, vnic->fw_vnic_id, filter);
}

int bnxt_link_update_op(struct rte_eth_dev *eth_dev, int wait_to_complete)
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
				"Failed to retrieve link rc = 0x%x!\n", rc);
			goto out;
		}
		rte_delay_ms(BNXT_LINK_WAIT_INTERVAL);

		if (!wait_to_complete)
			break;
	} while (!new.link_status && cnt--);

out:
	/* Timed out or success */
	if (new.link_status != eth_dev->data->dev_link.link_status ||
	new.link_speed != eth_dev->data->dev_link.link_speed) {
		rte_bnxt_atomic_write_link_status(eth_dev, &new);
		bnxt_print_link_info(eth_dev);
	}

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
	bnxt_hwrm_cfa_l2_set_rx_mask(bp, vnic, 0, NULL);
}

static void bnxt_promiscuous_disable_op(struct rte_eth_dev *eth_dev)
{
	struct bnxt *bp = (struct bnxt *)eth_dev->data->dev_private;
	struct bnxt_vnic_info *vnic;

	if (bp->vnic_info == NULL)
		return;

	vnic = &bp->vnic_info[0];

	vnic->flags &= ~BNXT_VNIC_INFO_PROMISC;
	bnxt_hwrm_cfa_l2_set_rx_mask(bp, vnic, 0, NULL);
}

static void bnxt_allmulticast_enable_op(struct rte_eth_dev *eth_dev)
{
	struct bnxt *bp = (struct bnxt *)eth_dev->data->dev_private;
	struct bnxt_vnic_info *vnic;

	if (bp->vnic_info == NULL)
		return;

	vnic = &bp->vnic_info[0];

	vnic->flags |= BNXT_VNIC_INFO_ALLMULTI;
	bnxt_hwrm_cfa_l2_set_rx_mask(bp, vnic, 0, NULL);
}

static void bnxt_allmulticast_disable_op(struct rte_eth_dev *eth_dev)
{
	struct bnxt *bp = (struct bnxt *)eth_dev->data->dev_private;
	struct bnxt_vnic_info *vnic;

	if (bp->vnic_info == NULL)
		return;

	vnic = &bp->vnic_info[0];

	vnic->flags &= ~BNXT_VNIC_INFO_ALLMULTI;
	bnxt_hwrm_cfa_l2_set_rx_mask(bp, vnic, 0, NULL);
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
	struct rte_intr_handle *intr_handle
		= &bp->pdev->intr_handle;

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

	if (rte_intr_allow_others(intr_handle)) {
		if (eth_dev->data->dev_conf.intr_conf.lsc != 0)
			bnxt_dev_lsc_intr_setup(eth_dev);
	}

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
			       struct rte_eth_fc_conf *fc_conf)
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

	if (BNXT_NPAR_PF(bp) || BNXT_VF(bp)) {
		RTE_LOG(ERR, PMD, "Flow Control Settings cannot be modified\n");
		return -ENOTSUP;
	}

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

/* Add UDP tunneling port */
static int
bnxt_udp_tunnel_port_add_op(struct rte_eth_dev *eth_dev,
			 struct rte_eth_udp_tunnel *udp_tunnel)
{
	struct bnxt *bp = (struct bnxt *)eth_dev->data->dev_private;
	uint16_t tunnel_type = 0;
	int rc = 0;

	switch (udp_tunnel->prot_type) {
	case RTE_TUNNEL_TYPE_VXLAN:
		if (bp->vxlan_port_cnt) {
			RTE_LOG(ERR, PMD, "Tunnel Port %d already programmed\n",
				udp_tunnel->udp_port);
			if (bp->vxlan_port != udp_tunnel->udp_port) {
				RTE_LOG(ERR, PMD, "Only one port allowed\n");
				return -ENOSPC;
			}
			bp->vxlan_port_cnt++;
			return 0;
		}
		tunnel_type =
			HWRM_TUNNEL_DST_PORT_ALLOC_INPUT_TUNNEL_TYPE_VXLAN;
		bp->vxlan_port_cnt++;
		break;
	case RTE_TUNNEL_TYPE_GENEVE:
		if (bp->geneve_port_cnt) {
			RTE_LOG(ERR, PMD, "Tunnel Port %d already programmed\n",
				udp_tunnel->udp_port);
			if (bp->geneve_port != udp_tunnel->udp_port) {
				RTE_LOG(ERR, PMD, "Only one port allowed\n");
				return -ENOSPC;
			}
			bp->geneve_port_cnt++;
			return 0;
		}
		tunnel_type =
			HWRM_TUNNEL_DST_PORT_ALLOC_INPUT_TUNNEL_TYPE_GENEVE;
		bp->geneve_port_cnt++;
		break;
	default:
		RTE_LOG(ERR, PMD, "Tunnel type is not supported\n");
		return -ENOTSUP;
	}
	rc = bnxt_hwrm_tunnel_dst_port_alloc(bp, udp_tunnel->udp_port,
					     tunnel_type);
	return rc;
}

static int
bnxt_udp_tunnel_port_del_op(struct rte_eth_dev *eth_dev,
			 struct rte_eth_udp_tunnel *udp_tunnel)
{
	struct bnxt *bp = (struct bnxt *)eth_dev->data->dev_private;
	uint16_t tunnel_type = 0;
	uint16_t port = 0;
	int rc = 0;

	switch (udp_tunnel->prot_type) {
	case RTE_TUNNEL_TYPE_VXLAN:
		if (!bp->vxlan_port_cnt) {
			RTE_LOG(ERR, PMD, "No Tunnel port configured yet\n");
			return -EINVAL;
		}
		if (bp->vxlan_port != udp_tunnel->udp_port) {
			RTE_LOG(ERR, PMD, "Req Port: %d. Configured port: %d\n",
				udp_tunnel->udp_port, bp->vxlan_port);
			return -EINVAL;
		}
		if (--bp->vxlan_port_cnt)
			return 0;

		tunnel_type =
			HWRM_TUNNEL_DST_PORT_FREE_INPUT_TUNNEL_TYPE_VXLAN;
		port = bp->vxlan_fw_dst_port_id;
		break;
	case RTE_TUNNEL_TYPE_GENEVE:
		if (!bp->geneve_port_cnt) {
			RTE_LOG(ERR, PMD, "No Tunnel port configured yet\n");
			return -EINVAL;
		}
		if (bp->geneve_port != udp_tunnel->udp_port) {
			RTE_LOG(ERR, PMD, "Req Port: %d. Configured port: %d\n",
				udp_tunnel->udp_port, bp->geneve_port);
			return -EINVAL;
		}
		if (--bp->geneve_port_cnt)
			return 0;

		tunnel_type =
			HWRM_TUNNEL_DST_PORT_FREE_INPUT_TUNNEL_TYPE_GENEVE;
		port = bp->geneve_fw_dst_port_id;
		break;
	default:
		RTE_LOG(ERR, PMD, "Tunnel type is not supported\n");
		return -ENOTSUP;
	}

	rc = bnxt_hwrm_tunnel_dst_port_free(bp, port, tunnel_type);
	if (!rc) {
		if (tunnel_type ==
		    HWRM_TUNNEL_DST_PORT_FREE_INPUT_TUNNEL_TYPE_VXLAN)
			bp->vxlan_port = 0;
		if (tunnel_type ==
		    HWRM_TUNNEL_DST_PORT_FREE_INPUT_TUNNEL_TYPE_GENEVE)
			bp->geneve_port = 0;
	}
	return rc;
}

static int bnxt_del_vlan_filter(struct bnxt *bp, uint16_t vlan_id)
{
	struct bnxt_filter_info *filter, *temp_filter, *new_filter;
	struct bnxt_vnic_info *vnic;
	unsigned int i;
	int rc = 0;
	uint32_t chk = HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_OVLAN;

	/* Cycle through all VNICs */
	for (i = 0; i < bp->nr_vnics; i++) {
		/*
		 * For each VNIC and each associated filter(s)
		 * if VLAN exists && VLAN matches vlan_id
		 *      remove the MAC+VLAN filter
		 *      add a new MAC only filter
		 * else
		 *      VLAN filter doesn't exist, just skip and continue
		 */
		STAILQ_FOREACH(vnic, &bp->ff_pool[i], next) {
			filter = STAILQ_FIRST(&vnic->filter);
			while (filter) {
				temp_filter = STAILQ_NEXT(filter, next);

				if (filter->enables & chk &&
				    filter->l2_ovlan == vlan_id) {
					/* Must delete the filter */
					STAILQ_REMOVE(&vnic->filter, filter,
						      bnxt_filter_info, next);
					bnxt_hwrm_clear_filter(bp, filter);
					STAILQ_INSERT_TAIL(
							&bp->free_filter_list,
							filter, next);

					/*
					 * Need to examine to see if the MAC
					 * filter already existed or not before
					 * allocating a new one
					 */

					new_filter = bnxt_alloc_filter(bp);
					if (!new_filter) {
						RTE_LOG(ERR, PMD,
							"MAC/VLAN filter alloc failed\n");
						rc = -ENOMEM;
						goto exit;
					}
					STAILQ_INSERT_TAIL(&vnic->filter,
							   new_filter, next);
					/* Inherit MAC from previous filter */
					new_filter->mac_index =
							filter->mac_index;
					memcpy(new_filter->l2_addr,
					       filter->l2_addr, ETHER_ADDR_LEN);
					/* MAC only filter */
					rc = bnxt_hwrm_set_filter(bp,
							vnic->fw_vnic_id,
							new_filter);
					if (rc)
						goto exit;
					RTE_LOG(INFO, PMD,
						"Del Vlan filter for %d\n",
						vlan_id);
				}
				filter = temp_filter;
			}
		}
	}
exit:
	return rc;
}

static int bnxt_add_vlan_filter(struct bnxt *bp, uint16_t vlan_id)
{
	struct bnxt_filter_info *filter, *temp_filter, *new_filter;
	struct bnxt_vnic_info *vnic;
	unsigned int i;
	int rc = 0;
	uint32_t en = HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_OVLAN |
		HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_OVLAN_MASK;
	uint32_t chk = HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_OVLAN;

	/* Cycle through all VNICs */
	for (i = 0; i < bp->nr_vnics; i++) {
		/*
		 * For each VNIC and each associated filter(s)
		 * if VLAN exists:
		 *   if VLAN matches vlan_id
		 *      VLAN filter already exists, just skip and continue
		 *   else
		 *      add a new MAC+VLAN filter
		 * else
		 *   Remove the old MAC only filter
		 *    Add a new MAC+VLAN filter
		 */
		STAILQ_FOREACH(vnic, &bp->ff_pool[i], next) {
			filter = STAILQ_FIRST(&vnic->filter);
			while (filter) {
				temp_filter = STAILQ_NEXT(filter, next);

				if (filter->enables & chk) {
					if (filter->l2_ovlan == vlan_id)
						goto cont;
				} else {
					/* Must delete the MAC filter */
					STAILQ_REMOVE(&vnic->filter, filter,
						      bnxt_filter_info, next);
					bnxt_hwrm_clear_filter(bp, filter);
					filter->l2_ovlan = 0;
					STAILQ_INSERT_TAIL(
							&bp->free_filter_list,
							filter, next);
				}
				new_filter = bnxt_alloc_filter(bp);
				if (!new_filter) {
					RTE_LOG(ERR, PMD,
						"MAC/VLAN filter alloc failed\n");
					rc = -ENOMEM;
					goto exit;
				}
				STAILQ_INSERT_TAIL(&vnic->filter, new_filter,
						   next);
				/* Inherit MAC from the previous filter */
				new_filter->mac_index = filter->mac_index;
				memcpy(new_filter->l2_addr, filter->l2_addr,
				       ETHER_ADDR_LEN);
				/* MAC + VLAN ID filter */
				new_filter->l2_ovlan = vlan_id;
				new_filter->l2_ovlan_mask = 0xF000;
				new_filter->enables |= en;
				rc = bnxt_hwrm_set_filter(bp, vnic->fw_vnic_id,
							  new_filter);
				if (rc)
					goto exit;
				RTE_LOG(INFO, PMD,
					"Added Vlan filter for %d\n", vlan_id);
cont:
				filter = temp_filter;
			}
		}
	}
exit:
	return rc;
}

static int bnxt_vlan_filter_set_op(struct rte_eth_dev *eth_dev,
				   uint16_t vlan_id, int on)
{
	struct bnxt *bp = (struct bnxt *)eth_dev->data->dev_private;

	/* These operations apply to ALL existing MAC/VLAN filters */
	if (on)
		return bnxt_add_vlan_filter(bp, vlan_id);
	else
		return bnxt_del_vlan_filter(bp, vlan_id);
}

static void
bnxt_vlan_offload_set_op(struct rte_eth_dev *dev, int mask)
{
	struct bnxt *bp = (struct bnxt *)dev->data->dev_private;
	unsigned int i;

	if (mask & ETH_VLAN_FILTER_MASK) {
		if (!dev->data->dev_conf.rxmode.hw_vlan_filter) {
			/* Remove any VLAN filters programmed */
			for (i = 0; i < 4095; i++)
				bnxt_del_vlan_filter(bp, i);
		}
		RTE_LOG(INFO, PMD, "VLAN Filtering: %d\n",
			dev->data->dev_conf.rxmode.hw_vlan_filter);
	}

	if (mask & ETH_VLAN_STRIP_MASK) {
		/* Enable or disable VLAN stripping */
		for (i = 0; i < bp->nr_vnics; i++) {
			struct bnxt_vnic_info *vnic = &bp->vnic_info[i];
			if (dev->data->dev_conf.rxmode.hw_vlan_strip)
				vnic->vlan_strip = true;
			else
				vnic->vlan_strip = false;
			bnxt_hwrm_vnic_cfg(bp, vnic);
		}
		RTE_LOG(INFO, PMD, "VLAN Strip Offload: %d\n",
			dev->data->dev_conf.rxmode.hw_vlan_strip);
	}

	if (mask & ETH_VLAN_EXTEND_MASK)
		RTE_LOG(ERR, PMD, "Extend VLAN Not supported\n");
}

static void
bnxt_set_default_mac_addr_op(struct rte_eth_dev *dev, struct ether_addr *addr)
{
	struct bnxt *bp = (struct bnxt *)dev->data->dev_private;
	/* Default Filter is tied to VNIC 0 */
	struct bnxt_vnic_info *vnic = &bp->vnic_info[0];
	struct bnxt_filter_info *filter;
	int rc;

	if (BNXT_VF(bp))
		return;

	memcpy(bp->mac_addr, addr, sizeof(bp->mac_addr));
	memcpy(&dev->data->mac_addrs[0], bp->mac_addr, ETHER_ADDR_LEN);

	STAILQ_FOREACH(filter, &vnic->filter, next) {
		/* Default Filter is at Index 0 */
		if (filter->mac_index != 0)
			continue;
		rc = bnxt_hwrm_clear_filter(bp, filter);
		if (rc)
			break;
		memcpy(filter->l2_addr, bp->mac_addr, ETHER_ADDR_LEN);
		memset(filter->l2_addr_mask, 0xff, ETHER_ADDR_LEN);
		filter->flags |= HWRM_CFA_L2_FILTER_ALLOC_INPUT_FLAGS_PATH_RX;
		filter->enables |=
			HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_ADDR |
			HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_ADDR_MASK;
		rc = bnxt_hwrm_set_filter(bp, vnic->fw_vnic_id, filter);
		if (rc)
			break;
		filter->mac_index = 0;
		RTE_LOG(DEBUG, PMD, "Set MAC addr\n");
	}
}

static int
bnxt_dev_set_mc_addr_list_op(struct rte_eth_dev *eth_dev,
			  struct ether_addr *mc_addr_set,
			  uint32_t nb_mc_addr)
{
	struct bnxt *bp = (struct bnxt *)eth_dev->data->dev_private;
	char *mc_addr_list = (char *)mc_addr_set;
	struct bnxt_vnic_info *vnic;
	uint32_t off = 0, i = 0;

	vnic = &bp->vnic_info[0];

	if (nb_mc_addr > BNXT_MAX_MC_ADDRS) {
		vnic->flags |= BNXT_VNIC_INFO_ALLMULTI;
		goto allmulti;
	}

	/* TODO Check for Duplicate mcast addresses */
	vnic->flags &= ~BNXT_VNIC_INFO_ALLMULTI;
	for (i = 0; i < nb_mc_addr; i++) {
		memcpy(vnic->mc_list + off, &mc_addr_list[i], ETHER_ADDR_LEN);
		off += ETHER_ADDR_LEN;
	}

	vnic->mc_addr_cnt = i;

allmulti:
	return bnxt_hwrm_cfa_l2_set_rx_mask(bp, vnic, 0, NULL);
}

static int
bnxt_fw_version_get(struct rte_eth_dev *dev, char *fw_version, size_t fw_size)
{
	struct bnxt *bp = (struct bnxt *)dev->data->dev_private;
	uint8_t fw_major = (bp->fw_ver >> 24) & 0xff;
	uint8_t fw_minor = (bp->fw_ver >> 16) & 0xff;
	uint8_t fw_updt = (bp->fw_ver >> 8) & 0xff;
	int ret;

	ret = snprintf(fw_version, fw_size, "%d.%d.%d",
			fw_major, fw_minor, fw_updt);

	ret += 1; /* add the size of '\0' */
	if (fw_size < (uint32_t)ret)
		return ret;
	else
		return 0;
}

static void
bnxt_rxq_info_get_op(struct rte_eth_dev *dev, uint16_t queue_id,
	struct rte_eth_rxq_info *qinfo)
{
	struct bnxt_rx_queue *rxq;

	rxq = dev->data->rx_queues[queue_id];

	qinfo->mp = rxq->mb_pool;
	qinfo->scattered_rx = dev->data->scattered_rx;
	qinfo->nb_desc = rxq->nb_rx_desc;

	qinfo->conf.rx_free_thresh = rxq->rx_free_thresh;
	qinfo->conf.rx_drop_en = 0;
	qinfo->conf.rx_deferred_start = 0;
}

static void
bnxt_txq_info_get_op(struct rte_eth_dev *dev, uint16_t queue_id,
	struct rte_eth_txq_info *qinfo)
{
	struct bnxt_tx_queue *txq;

	txq = dev->data->tx_queues[queue_id];

	qinfo->nb_desc = txq->nb_tx_desc;

	qinfo->conf.tx_thresh.pthresh = txq->pthresh;
	qinfo->conf.tx_thresh.hthresh = txq->hthresh;
	qinfo->conf.tx_thresh.wthresh = txq->wthresh;

	qinfo->conf.tx_free_thresh = txq->tx_free_thresh;
	qinfo->conf.tx_rs_thresh = 0;
	qinfo->conf.txq_flags = txq->txq_flags;
	qinfo->conf.tx_deferred_start = txq->tx_deferred_start;
}

static int bnxt_mtu_set_op(struct rte_eth_dev *eth_dev, uint16_t new_mtu)
{
	struct bnxt *bp = eth_dev->data->dev_private;
	struct rte_eth_dev_info dev_info;
	uint32_t max_dev_mtu;
	uint32_t rc = 0;
	uint32_t i;

	bnxt_dev_info_get_op(eth_dev, &dev_info);
	max_dev_mtu = dev_info.max_rx_pktlen -
		      ETHER_HDR_LEN - ETHER_CRC_LEN - VLAN_TAG_SIZE * 2;

	if (new_mtu < ETHER_MIN_MTU || new_mtu > max_dev_mtu) {
		RTE_LOG(ERR, PMD, "MTU requested must be within (%d, %d)\n",
			ETHER_MIN_MTU, max_dev_mtu);
		return -EINVAL;
	}


	if (new_mtu > ETHER_MTU) {
		bp->flags |= BNXT_FLAG_JUMBO;
		eth_dev->data->dev_conf.rxmode.jumbo_frame = 1;
	} else {
		eth_dev->data->dev_conf.rxmode.jumbo_frame = 0;
		bp->flags &= ~BNXT_FLAG_JUMBO;
	}

	eth_dev->data->dev_conf.rxmode.max_rx_pkt_len =
		new_mtu + ETHER_HDR_LEN + ETHER_CRC_LEN + VLAN_TAG_SIZE * 2;

	eth_dev->data->mtu = new_mtu;
	RTE_LOG(INFO, PMD, "New MTU is %d\n", eth_dev->data->mtu);

	for (i = 0; i < bp->nr_vnics; i++) {
		struct bnxt_vnic_info *vnic = &bp->vnic_info[i];

		vnic->mru = bp->eth_dev->data->mtu + ETHER_HDR_LEN +
					ETHER_CRC_LEN + VLAN_TAG_SIZE * 2;
		rc = bnxt_hwrm_vnic_cfg(bp, vnic);
		if (rc)
			break;

		rc = bnxt_hwrm_vnic_plcmode_cfg(bp, vnic);
		if (rc)
			return rc;
	}

	return rc;
}

static int
bnxt_vlan_pvid_set_op(struct rte_eth_dev *dev, uint16_t pvid, int on)
{
	struct bnxt *bp = (struct bnxt *)dev->data->dev_private;
	uint16_t vlan = bp->vlan;
	int rc;

	if (BNXT_NPAR_PF(bp) || BNXT_VF(bp)) {
		RTE_LOG(ERR, PMD,
			"PVID cannot be modified for this function\n");
		return -ENOTSUP;
	}
	bp->vlan = on ? pvid : 0;

	rc = bnxt_hwrm_set_default_vlan(bp, 0, 0);
	if (rc)
		bp->vlan = vlan;
	return rc;
}

static int
bnxt_dev_led_on_op(struct rte_eth_dev *dev)
{
	struct bnxt *bp = (struct bnxt *)dev->data->dev_private;

	return bnxt_hwrm_port_led_cfg(bp, true);
}

static int
bnxt_dev_led_off_op(struct rte_eth_dev *dev)
{
	struct bnxt *bp = (struct bnxt *)dev->data->dev_private;

	return bnxt_hwrm_port_led_cfg(bp, false);
}

/*
 * Initialization
 */

static const struct eth_dev_ops bnxt_dev_ops = {
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
	.udp_tunnel_port_add  = bnxt_udp_tunnel_port_add_op,
	.udp_tunnel_port_del  = bnxt_udp_tunnel_port_del_op,
	.vlan_filter_set = bnxt_vlan_filter_set_op,
	.vlan_offload_set = bnxt_vlan_offload_set_op,
	.vlan_pvid_set = bnxt_vlan_pvid_set_op,
	.mtu_set = bnxt_mtu_set_op,
	.mac_addr_set = bnxt_set_default_mac_addr_op,
	.xstats_get = bnxt_dev_xstats_get_op,
	.xstats_get_names = bnxt_dev_xstats_get_names_op,
	.xstats_reset = bnxt_dev_xstats_reset_op,
	.fw_version_get = bnxt_fw_version_get,
	.set_mc_addr_list = bnxt_dev_set_mc_addr_list_op,
	.rxq_info_get = bnxt_rxq_info_get_op,
	.txq_info_get = bnxt_txq_info_get_op,
	.dev_led_on = bnxt_dev_led_on_op,
	.dev_led_off = bnxt_dev_led_off_op,
};

static bool bnxt_vf_pciid(uint16_t id)
{
	if (id == BROADCOM_DEV_ID_57304_VF ||
	    id == BROADCOM_DEV_ID_57406_VF ||
	    id == BROADCOM_DEV_ID_5731X_VF ||
	    id == BROADCOM_DEV_ID_5741X_VF ||
	    id == BROADCOM_DEV_ID_57414_VF ||
	    id == BROADCOM_DEV_ID_STRATUS_NIC_VF)
		return true;
	return false;
}

static int bnxt_init_board(struct rte_eth_dev *eth_dev)
{
	struct bnxt *bp = eth_dev->data->dev_private;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	int rc;

	/* enable device (incl. PCI PM wakeup), and bus-mastering */
	if (!pci_dev->mem_resource[0].addr) {
		RTE_LOG(ERR, PMD,
			"Cannot find PCI device base address, aborting\n");
		rc = -ENODEV;
		goto init_err_disable;
	}

	bp->eth_dev = eth_dev;
	bp->pdev = pci_dev;

	bp->bar0 = (void *)pci_dev->mem_resource[0].addr;
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

static int bnxt_dev_uninit(struct rte_eth_dev *eth_dev);

#define ALLOW_FUNC(x)	\
	{ \
		typeof(x) arg = (x); \
		bp->pf.vf_req_fwd[((arg) >> 5)] &= \
		~rte_cpu_to_le_32(1 << ((arg) & 0x1f)); \
	}
static int
bnxt_dev_init(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	char mz_name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *mz = NULL;
	static int version_printed;
	uint32_t total_alloc_len;
	phys_addr_t mz_phys_addr;
	struct bnxt *bp;
	int rc;

	if (version_printed++ == 0)
		RTE_LOG(INFO, PMD, "%s\n", bnxt_version);

	rte_eth_copy_pci_info(eth_dev, pci_dev);
	eth_dev->data->dev_flags |= RTE_ETH_DEV_DETACHABLE;

	bp = eth_dev->data->dev_private;

	rte_atomic64_init(&bp->rx_mbuf_alloc_fail);
	bp->dev_stopped = 1;

	if (bnxt_vf_pciid(pci_dev->id.device_id))
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

	if (BNXT_PF(bp) && pci_dev->id.device_id != BROADCOM_DEV_ID_NS2) {
		snprintf(mz_name, RTE_MEMZONE_NAMESIZE,
			 "bnxt_%04x:%02x:%02x:%02x-%s", pci_dev->addr.domain,
			 pci_dev->addr.bus, pci_dev->addr.devid,
			 pci_dev->addr.function, "rx_port_stats");
		mz_name[RTE_MEMZONE_NAMESIZE - 1] = 0;
		mz = rte_memzone_lookup(mz_name);
		total_alloc_len = RTE_CACHE_LINE_ROUNDUP(
				sizeof(struct rx_port_stats) + 512);
		if (!mz) {
			mz = rte_memzone_reserve(mz_name, total_alloc_len,
						 SOCKET_ID_ANY,
						 RTE_MEMZONE_2MB |
						 RTE_MEMZONE_SIZE_HINT_ONLY);
			if (mz == NULL)
				return -ENOMEM;
		}
		memset(mz->addr, 0, mz->len);
		mz_phys_addr = mz->phys_addr;
		if ((unsigned long)mz->addr == mz_phys_addr) {
			RTE_LOG(WARNING, PMD,
				"Memzone physical address same as virtual.\n");
			RTE_LOG(WARNING, PMD,
				"Using rte_mem_virt2phy()\n");
			mz_phys_addr = rte_mem_virt2phy(mz->addr);
			if (mz_phys_addr == 0) {
				RTE_LOG(ERR, PMD,
				"unable to map address to physical memory\n");
				return -ENOMEM;
			}
		}

		bp->rx_mem_zone = (const void *)mz;
		bp->hw_rx_port_stats = mz->addr;
		bp->hw_rx_port_stats_map = mz_phys_addr;

		snprintf(mz_name, RTE_MEMZONE_NAMESIZE,
			 "bnxt_%04x:%02x:%02x:%02x-%s", pci_dev->addr.domain,
			 pci_dev->addr.bus, pci_dev->addr.devid,
			 pci_dev->addr.function, "tx_port_stats");
		mz_name[RTE_MEMZONE_NAMESIZE - 1] = 0;
		mz = rte_memzone_lookup(mz_name);
		total_alloc_len = RTE_CACHE_LINE_ROUNDUP(
				sizeof(struct tx_port_stats) + 512);
		if (!mz) {
			mz = rte_memzone_reserve(mz_name, total_alloc_len,
						 SOCKET_ID_ANY,
						 RTE_MEMZONE_2MB |
						 RTE_MEMZONE_SIZE_HINT_ONLY);
			if (mz == NULL)
				return -ENOMEM;
		}
		memset(mz->addr, 0, mz->len);
		mz_phys_addr = mz->phys_addr;
		if ((unsigned long)mz->addr == mz_phys_addr) {
			RTE_LOG(WARNING, PMD,
				"Memzone physical address same as virtual.\n");
			RTE_LOG(WARNING, PMD,
				"Using rte_mem_virt2phy()\n");
			mz_phys_addr = rte_mem_virt2phy(mz->addr);
			if (mz_phys_addr == 0) {
				RTE_LOG(ERR, PMD,
				"unable to map address to physical memory\n");
				return -ENOMEM;
			}
		}

		bp->tx_mem_zone = (const void *)mz;
		bp->hw_tx_port_stats = mz->addr;
		bp->hw_tx_port_stats_map = mz_phys_addr;

		bp->flags |= BNXT_FLAG_PORT_STATS;
	}

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

	bnxt_hwrm_func_qcfg(bp);

	/* Get the MAX capabilities for this function */
	rc = bnxt_hwrm_func_qcaps(bp);
	if (rc) {
		RTE_LOG(ERR, PMD, "hwrm query capability failure rc: %x\n", rc);
		goto error_free;
	}
	if (bp->max_tx_rings == 0) {
		RTE_LOG(ERR, PMD, "No TX rings available!\n");
		rc = -EBUSY;
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
	memcpy(bp->mac_addr, bp->dflt_mac_addr, sizeof(bp->mac_addr));
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

	/* Forward all requests if firmware is new enough */
	if (((bp->fw_ver >= ((20 << 24) | (6 << 16) | (100 << 8))) &&
	    (bp->fw_ver < ((20 << 24) | (7 << 16)))) ||
	    ((bp->fw_ver >= ((20 << 24) | (8 << 16))))) {
		memset(bp->pf.vf_req_fwd, 0xff, sizeof(bp->pf.vf_req_fwd));
	} else {
		RTE_LOG(WARNING, PMD,
			"Firmware too old for VF mailbox functionality\n");
		memset(bp->pf.vf_req_fwd, 0, sizeof(bp->pf.vf_req_fwd));
	}

	/*
	 * The following are used for driver cleanup.  If we disallow these,
	 * VF drivers can't clean up cleanly.
	 */
	ALLOW_FUNC(HWRM_FUNC_DRV_UNRGTR);
	ALLOW_FUNC(HWRM_VNIC_FREE);
	ALLOW_FUNC(HWRM_RING_FREE);
	ALLOW_FUNC(HWRM_RING_GRP_FREE);
	ALLOW_FUNC(HWRM_VNIC_RSS_COS_LB_CTX_FREE);
	ALLOW_FUNC(HWRM_CFA_L2_FILTER_FREE);
	ALLOW_FUNC(HWRM_STAT_CTX_FREE);
	rc = bnxt_hwrm_func_driver_register(bp);
	if (rc) {
		RTE_LOG(ERR, PMD,
			"Failed to register driver");
		rc = -EBUSY;
		goto error_free;
	}

	RTE_LOG(INFO, PMD,
		DRV_MODULE_NAME " found at mem %" PRIx64 ", node addr %pM\n",
		pci_dev->mem_resource[0].phys_addr,
		pci_dev->mem_resource[0].addr);

	rc = bnxt_hwrm_func_reset(bp);
	if (rc) {
		RTE_LOG(ERR, PMD, "hwrm chip reset failure rc: %x\n", rc);
		rc = -1;
		goto error_free;
	}

	if (BNXT_PF(bp)) {
		//if (bp->pf.active_vfs) {
			// TODO: Deallocate VF resources?
		//}
		if (bp->pdev->max_vfs) {
			rc = bnxt_hwrm_allocate_vfs(bp, bp->pdev->max_vfs);
			if (rc) {
				RTE_LOG(ERR, PMD, "Failed to allocate VFs\n");
				goto error_free;
			}
		} else {
			rc = bnxt_hwrm_allocate_pf_only(bp);
			if (rc) {
				RTE_LOG(ERR, PMD,
					"Failed to allocate PF resources\n");
				goto error_free;
			}
		}
	}

	bnxt_hwrm_port_led_qcaps(bp);

	rc = bnxt_setup_int(bp);
	if (rc)
		goto error_free;

	rc = bnxt_alloc_mem(bp);
	if (rc)
		goto error_free_int;

	rc = bnxt_request_int(bp);
	if (rc)
		goto error_free_int;

	rc = bnxt_alloc_def_cp_ring(bp);
	if (rc)
		goto error_free_int;

	bnxt_enable_int(bp);

	return 0;

error_free_int:
	bnxt_disable_int(bp);
	bnxt_free_def_cp_ring(bp);
	bnxt_hwrm_func_buf_unrgtr(bp);
	bnxt_free_int(bp);
	bnxt_free_mem(bp);
error_free:
	bnxt_dev_uninit(eth_dev);
error:
	return rc;
}

static int
bnxt_dev_uninit(struct rte_eth_dev *eth_dev) {
	struct bnxt *bp = eth_dev->data->dev_private;
	int rc;

	bnxt_disable_int(bp);
	bnxt_free_int(bp);
	bnxt_free_mem(bp);
	if (eth_dev->data->mac_addrs != NULL) {
		rte_free(eth_dev->data->mac_addrs);
		eth_dev->data->mac_addrs = NULL;
	}
	if (bp->grp_info != NULL) {
		rte_free(bp->grp_info);
		bp->grp_info = NULL;
	}
	rc = bnxt_hwrm_func_driver_unregister(bp, 0);
	bnxt_free_hwrm_resources(bp);
	rte_memzone_free((const struct rte_memzone *)bp->tx_mem_zone);
	rte_memzone_free((const struct rte_memzone *)bp->rx_mem_zone);
	if (bp->dev_stopped == 0)
		bnxt_dev_close_op(eth_dev);
	if (bp->pf.vf_info)
		rte_free(bp->pf.vf_info);
	eth_dev->dev_ops = NULL;
	eth_dev->rx_pkt_burst = NULL;
	eth_dev->tx_pkt_burst = NULL;

	return rc;
}

static int bnxt_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev, sizeof(struct bnxt),
		bnxt_dev_init);
}

static int bnxt_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, bnxt_dev_uninit);
}

static struct rte_pci_driver bnxt_rte_pmd = {
	.id_table = bnxt_pci_id_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING |
		RTE_PCI_DRV_INTR_LSC,
	.probe = bnxt_pci_probe,
	.remove = bnxt_pci_remove,
};

static bool
is_device_supported(struct rte_eth_dev *dev, struct rte_pci_driver *drv)
{
	if (strcmp(dev->device->driver->name, drv->driver.name))
		return false;

	return true;
}

bool is_bnxt_supported(struct rte_eth_dev *dev)
{
	return is_device_supported(dev, &bnxt_rte_pmd);
}

RTE_PMD_REGISTER_PCI(net_bnxt, bnxt_rte_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_bnxt, bnxt_pci_id_map);
RTE_PMD_REGISTER_KMOD_DEP(net_bnxt, "* igb_uio | uio_pci_generic | vfio-pci");
