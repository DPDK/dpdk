/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2018 Broadcom
 * All rights reserved.
 */

#include <inttypes.h>
#include <stdbool.h>

#include <rte_dev.h>
#include <rte_ethdev_driver.h>
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
#include "bnxt_nvm_defs.h"

#define DRV_MODULE_NAME		"bnxt"
static const char bnxt_version[] =
	"Broadcom NetXtreme driver " DRV_MODULE_NAME "\n";
int bnxt_logtype_driver;

#define PCI_VENDOR_ID_BROADCOM 0x14E4

#define BROADCOM_DEV_ID_STRATUS_NIC_VF1 0x1606
#define BROADCOM_DEV_ID_STRATUS_NIC_VF2 0x1609
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
#define BROADCOM_DEV_ID_58802 0xd802
#define BROADCOM_DEV_ID_58804 0xd804
#define BROADCOM_DEV_ID_58808 0x16f0

static const struct rte_pci_id bnxt_pci_id_map[] = {
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM,
			 BROADCOM_DEV_ID_STRATUS_NIC_VF1) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM,
			 BROADCOM_DEV_ID_STRATUS_NIC_VF2) },
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
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_58802) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_58804) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, BROADCOM_DEV_ID_58808) },
	{ .vendor_id = 0, /* sentinel */ },
};

#define BNXT_ETH_RSS_SUPPORT (	\
	ETH_RSS_IPV4 |		\
	ETH_RSS_NONFRAG_IPV4_TCP |	\
	ETH_RSS_NONFRAG_IPV4_UDP |	\
	ETH_RSS_IPV6 |		\
	ETH_RSS_NONFRAG_IPV6_TCP |	\
	ETH_RSS_NONFRAG_IPV6_UDP)

#define BNXT_DEV_TX_OFFLOAD_SUPPORT (DEV_TX_OFFLOAD_VLAN_INSERT | \
				     DEV_TX_OFFLOAD_IPV4_CKSUM | \
				     DEV_TX_OFFLOAD_TCP_CKSUM | \
				     DEV_TX_OFFLOAD_UDP_CKSUM | \
				     DEV_TX_OFFLOAD_TCP_TSO | \
				     DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM | \
				     DEV_TX_OFFLOAD_VXLAN_TNL_TSO | \
				     DEV_TX_OFFLOAD_GRE_TNL_TSO | \
				     DEV_TX_OFFLOAD_IPIP_TNL_TSO | \
				     DEV_TX_OFFLOAD_GENEVE_TNL_TSO | \
				     DEV_TX_OFFLOAD_MULTI_SEGS)

#define BNXT_DEV_RX_OFFLOAD_SUPPORT (DEV_RX_OFFLOAD_VLAN_FILTER | \
				     DEV_RX_OFFLOAD_VLAN_STRIP | \
				     DEV_RX_OFFLOAD_IPV4_CKSUM | \
				     DEV_RX_OFFLOAD_UDP_CKSUM | \
				     DEV_RX_OFFLOAD_TCP_CKSUM | \
				     DEV_RX_OFFLOAD_OUTER_IPV4_CKSUM | \
				     DEV_RX_OFFLOAD_JUMBO_FRAME | \
				     DEV_RX_OFFLOAD_CRC_STRIP | \
				     DEV_RX_OFFLOAD_TCP_LRO)

static int bnxt_vlan_offload_set_op(struct rte_eth_dev *dev, int mask);
static void bnxt_print_link_info(struct rte_eth_dev *eth_dev);
static int bnxt_mtu_set_op(struct rte_eth_dev *eth_dev, uint16_t new_mtu);

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
}

static int bnxt_alloc_mem(struct bnxt *bp)
{
	int rc;

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
	unsigned int i;
	struct rte_eth_link new;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(bp->eth_dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	uint32_t intr_vector = 0;
	uint32_t queue_id, base = BNXT_MISC_VEC_ID;
	uint32_t vec = BNXT_MISC_VEC_ID;
	int rc;

	/* disable uio/vfio intr/eventfd mapping */
	rte_intr_disable(intr_handle);

	if (bp->eth_dev->data->mtu > ETHER_MTU) {
		bp->eth_dev->data->dev_conf.rxmode.offloads |=
			DEV_RX_OFFLOAD_JUMBO_FRAME;
		bp->flags |= BNXT_FLAG_JUMBO;
	} else {
		bp->eth_dev->data->dev_conf.rxmode.offloads &=
			~DEV_RX_OFFLOAD_JUMBO_FRAME;
		bp->flags &= ~BNXT_FLAG_JUMBO;
	}

	rc = bnxt_alloc_all_hwrm_stat_ctxs(bp);
	if (rc) {
		PMD_DRV_LOG(ERR, "HWRM stat ctx alloc failure rc: %x\n", rc);
		goto err_out;
	}

	rc = bnxt_alloc_hwrm_rings(bp);
	if (rc) {
		PMD_DRV_LOG(ERR, "HWRM ring alloc failure rc: %x\n", rc);
		goto err_out;
	}

	rc = bnxt_alloc_all_hwrm_ring_grps(bp);
	if (rc) {
		PMD_DRV_LOG(ERR, "HWRM ring grp alloc failure: %x\n", rc);
		goto err_out;
	}

	rc = bnxt_mq_rx_configure(bp);
	if (rc) {
		PMD_DRV_LOG(ERR, "MQ mode configure failure rc: %x\n", rc);
		goto err_out;
	}

	/* VNIC configuration */
	for (i = 0; i < bp->nr_vnics; i++) {
		struct bnxt_vnic_info *vnic = &bp->vnic_info[i];

		rc = bnxt_hwrm_vnic_alloc(bp, vnic);
		if (rc) {
			PMD_DRV_LOG(ERR, "HWRM vnic %d alloc failure rc: %x\n",
				i, rc);
			goto err_out;
		}

		rc = bnxt_hwrm_vnic_ctx_alloc(bp, vnic);
		if (rc) {
			PMD_DRV_LOG(ERR,
				"HWRM vnic %d ctx alloc failure rc: %x\n",
				i, rc);
			goto err_out;
		}

		rc = bnxt_hwrm_vnic_cfg(bp, vnic);
		if (rc) {
			PMD_DRV_LOG(ERR, "HWRM vnic %d cfg failure rc: %x\n",
				i, rc);
			goto err_out;
		}

		rc = bnxt_set_hwrm_vnic_filters(bp, vnic);
		if (rc) {
			PMD_DRV_LOG(ERR,
				"HWRM vnic %d filter failure rc: %x\n",
				i, rc);
			goto err_out;
		}

		rc = bnxt_vnic_rss_configure(bp, vnic);
		if (rc) {
			PMD_DRV_LOG(ERR,
				    "HWRM vnic set RSS failure rc: %x\n", rc);
			goto err_out;
		}

		bnxt_hwrm_vnic_plcmode_cfg(bp, vnic);

		if (bp->eth_dev->data->dev_conf.rxmode.offloads &
		    DEV_RX_OFFLOAD_TCP_LRO)
			bnxt_hwrm_vnic_tpa_cfg(bp, vnic, 1);
		else
			bnxt_hwrm_vnic_tpa_cfg(bp, vnic, 0);
	}
	rc = bnxt_hwrm_cfa_l2_set_rx_mask(bp, &bp->vnic_info[0], 0, NULL);
	if (rc) {
		PMD_DRV_LOG(ERR,
			"HWRM cfa l2 rx mask failure rc: %x\n", rc);
		goto err_out;
	}

	/* check and configure queue intr-vector mapping */
	if ((rte_intr_cap_multiple(intr_handle) ||
	     !RTE_ETH_DEV_SRIOV(bp->eth_dev).active) &&
	    bp->eth_dev->data->dev_conf.intr_conf.rxq != 0) {
		intr_vector = bp->eth_dev->data->nb_rx_queues;
		PMD_DRV_LOG(DEBUG, "intr_vector = %d\n", intr_vector);
		if (intr_vector > bp->rx_cp_nr_rings) {
			PMD_DRV_LOG(ERR, "At most %d intr queues supported",
					bp->rx_cp_nr_rings);
			return -ENOTSUP;
		}
		if (rte_intr_efd_enable(intr_handle, intr_vector))
			return -1;
	}

	if (rte_intr_dp_is_en(intr_handle) && !intr_handle->intr_vec) {
		intr_handle->intr_vec =
			rte_zmalloc("intr_vec",
				    bp->eth_dev->data->nb_rx_queues *
				    sizeof(int), 0);
		if (intr_handle->intr_vec == NULL) {
			PMD_DRV_LOG(ERR, "Failed to allocate %d rx_queues"
				" intr_vec", bp->eth_dev->data->nb_rx_queues);
			return -ENOMEM;
		}
		PMD_DRV_LOG(DEBUG, "intr_handle->intr_vec = %p "
			"intr_handle->nb_efd = %d intr_handle->max_intr = %d\n",
			 intr_handle->intr_vec, intr_handle->nb_efd,
			intr_handle->max_intr);
	}

	for (queue_id = 0; queue_id < bp->eth_dev->data->nb_rx_queues;
	     queue_id++) {
		intr_handle->intr_vec[queue_id] = vec;
		if (vec < base + intr_handle->nb_efd - 1)
			vec++;
	}

	/* enable uio/vfio intr/eventfd mapping */
	rte_intr_enable(intr_handle);

	rc = bnxt_get_hwrm_link_config(bp, &new);
	if (rc) {
		PMD_DRV_LOG(ERR, "HWRM Get link config failure rc: %x\n", rc);
		goto err_out;
	}

	if (!bp->link_info.link_up) {
		rc = bnxt_set_hwrm_link_config(bp, true);
		if (rc) {
			PMD_DRV_LOG(ERR,
				"HWRM link config failure rc: %x\n", rc);
			goto err_out;
		}
	}
	bnxt_print_link_info(bp->eth_dev);

	return 0;

err_out:
	bnxt_free_all_hwrm_resources(bp);

	/* Some of the error status returned by FW may not be from errno.h */
	if (rc > 0)
		rc = -EIO;

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

	rc = bnxt_init_ring_grps(bp);
	if (rc)
		return rc;

	bnxt_init_vnics(bp);
	bnxt_init_filters(bp);

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
	unsigned int max_rx_rings;

	/* MAC Specifics */
	dev_info->max_mac_addrs = bp->max_l2_ctx;
	dev_info->max_hash_mac_addrs = 0;

	/* PF/VF specifics */
	if (BNXT_PF(bp))
		dev_info->max_vfs = bp->pdev->max_vfs;
	max_rx_rings = RTE_MIN(bp->max_vnics, bp->max_stat_ctx);
	/* For the sake of symmetry, max_rx_queues = max_tx_queues */
	dev_info->max_rx_queues = max_rx_rings;
	dev_info->max_tx_queues = max_rx_rings;
	dev_info->reta_size = bp->max_rsscos_ctx;
	dev_info->hash_key_size = 40;
	max_vnics = bp->max_vnics;

	/* Fast path specifics */
	dev_info->min_rx_bufsize = 1;
	dev_info->max_rx_pktlen = BNXT_MAX_MTU + ETHER_HDR_LEN + ETHER_CRC_LEN
				  + VLAN_TAG_SIZE;

	dev_info->rx_offload_capa = BNXT_DEV_RX_OFFLOAD_SUPPORT;
	if (bp->flags & BNXT_FLAG_PTP_SUPPORTED)
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
	uint64_t rx_offloads = eth_dev->data->dev_conf.rxmode.offloads;

	bp->rx_queues = (void *)eth_dev->data->rx_queues;
	bp->tx_queues = (void *)eth_dev->data->tx_queues;
	bp->tx_nr_rings = eth_dev->data->nb_tx_queues;
	bp->rx_nr_rings = eth_dev->data->nb_rx_queues;

	if (BNXT_VF(bp) && (bp->flags & BNXT_FLAG_NEW_RM)) {
		int rc;

		rc = bnxt_hwrm_func_reserve_vf_resc(bp);
		if (rc) {
			PMD_DRV_LOG(ERR, "HWRM resource alloc fail:%x\n", rc);
			return -ENOSPC;
		}

		/* legacy driver needs to get updated values */
		rc = bnxt_hwrm_func_qcaps(bp);
		if (rc) {
			PMD_DRV_LOG(ERR, "hwrm func qcaps fail:%d\n", rc);
			return -ENOSPC;
		}
	}

	/* Inherit new configurations */
	if (eth_dev->data->nb_rx_queues > bp->max_rx_rings ||
	    eth_dev->data->nb_tx_queues > bp->max_tx_rings ||
	    eth_dev->data->nb_rx_queues + eth_dev->data->nb_tx_queues >
	    bp->max_cp_rings ||
	    eth_dev->data->nb_rx_queues + eth_dev->data->nb_tx_queues >
	    bp->max_stat_ctx ||
	    (uint32_t)(eth_dev->data->nb_rx_queues) > bp->max_ring_grps) {
		PMD_DRV_LOG(ERR,
			"Insufficient resources to support requested config\n");
		PMD_DRV_LOG(ERR,
			"Num Queues Requested: Tx %d, Rx %d\n",
			eth_dev->data->nb_tx_queues,
			eth_dev->data->nb_rx_queues);
		PMD_DRV_LOG(ERR,
			"Res available: TxQ %d, RxQ %d, CQ %d Stat %d, Grp %d\n",
			bp->max_tx_rings, bp->max_rx_rings, bp->max_cp_rings,
			bp->max_stat_ctx, bp->max_ring_grps);
		return -ENOSPC;
	}

	bp->rx_cp_nr_rings = bp->rx_nr_rings;
	bp->tx_cp_nr_rings = bp->tx_nr_rings;

	if (rx_offloads & DEV_RX_OFFLOAD_JUMBO_FRAME) {
		eth_dev->data->mtu =
				eth_dev->data->dev_conf.rxmode.max_rx_pkt_len -
				ETHER_HDR_LEN - ETHER_CRC_LEN - VLAN_TAG_SIZE *
				BNXT_NUM_VLANS;
		bnxt_mtu_set_op(eth_dev, eth_dev->data->mtu);
	}
	return 0;
}

static void bnxt_print_link_info(struct rte_eth_dev *eth_dev)
{
	struct rte_eth_link *link = &eth_dev->data->dev_link;

	if (link->link_status)
		PMD_DRV_LOG(INFO, "Port %d Link Up - speed %u Mbps - %s\n",
			eth_dev->data->port_id,
			(uint32_t)link->link_speed,
			(link->link_duplex == ETH_LINK_FULL_DUPLEX) ?
			("full-duplex") : ("half-duplex\n"));
	else
		PMD_DRV_LOG(INFO, "Port %d Link Down\n",
			eth_dev->data->port_id);
}

static int bnxt_dev_lsc_intr_setup(struct rte_eth_dev *eth_dev)
{
	bnxt_print_link_info(eth_dev);
	return 0;
}

static int bnxt_dev_start_op(struct rte_eth_dev *eth_dev)
{
	struct bnxt *bp = (struct bnxt *)eth_dev->data->dev_private;
	uint64_t rx_offloads = eth_dev->data->dev_conf.rxmode.offloads;
	int vlan_mask = 0;
	int rc;

	if (bp->rx_cp_nr_rings > RTE_ETHDEV_QUEUE_STAT_CNTRS) {
		PMD_DRV_LOG(ERR,
			"RxQ cnt %d > CONFIG_RTE_ETHDEV_QUEUE_STAT_CNTRS %d\n",
			bp->rx_cp_nr_rings, RTE_ETHDEV_QUEUE_STAT_CNTRS);
	}
	bp->dev_stopped = 0;

	rc = bnxt_init_chip(bp);
	if (rc)
		goto error;

	bnxt_link_update_op(eth_dev, 1);

	if (rx_offloads & DEV_RX_OFFLOAD_VLAN_FILTER)
		vlan_mask |= ETH_VLAN_FILTER_MASK;
	if (rx_offloads & DEV_RX_OFFLOAD_VLAN_STRIP)
		vlan_mask |= ETH_VLAN_STRIP_MASK;
	rc = bnxt_vlan_offload_set_op(eth_dev, vlan_mask);
	if (rc)
		goto error;

	bp->flags |= BNXT_FLAG_INIT_DONE;
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
	int rc = 0;

	if (!bp->link_info.link_up)
		rc = bnxt_set_hwrm_link_config(bp, true);
	if (!rc)
		eth_dev->data->dev_link.link_status = 1;

	bnxt_print_link_info(eth_dev);
	return 0;
}

static int bnxt_dev_set_link_down_op(struct rte_eth_dev *eth_dev)
{
	struct bnxt *bp = (struct bnxt *)eth_dev->data->dev_private;

	eth_dev->data->dev_link.link_status = 0;
	bnxt_set_hwrm_link_config(bp, false);
	bp->link_info.link_up = 0;

	return 0;
}

/* Unload the driver, release resources */
static void bnxt_dev_stop_op(struct rte_eth_dev *eth_dev)
{
	struct bnxt *bp = (struct bnxt *)eth_dev->data->dev_private;

	bp->flags &= ~BNXT_FLAG_INIT_DONE;
	if (bp->eth_dev->data->dev_started) {
		/* TBD: STOP HW queues DMA */
		eth_dev->data->dev_link.link_status = 0;
	}
	bnxt_set_hwrm_link_config(bp, false);
	bnxt_hwrm_port_clr_stats(bp);
	bnxt_free_tx_mbufs(bp);
	bnxt_free_rx_mbufs(bp);
	bnxt_shutdown_nic(bp);
	bp->dev_stopped = 1;
}

static void bnxt_dev_close_op(struct rte_eth_dev *eth_dev)
{
	struct bnxt *bp = (struct bnxt *)eth_dev->data->dev_private;

	if (bp->dev_stopped == 0)
		bnxt_dev_stop_op(eth_dev);

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
	uint32_t pool = RTE_MIN(MAX_FF_POOLS, ETH_64_POOLS);
	uint32_t i;

	/*
	 * Loop through all VNICs from the specified filter flow pools to
	 * remove the corresponding MAC addr filter
	 */
	for (i = 0; i < pool; i++) {
		if (!(pool_mask & (1ULL << i)))
			continue;

		STAILQ_FOREACH(vnic, &bp->ff_pool[i], next) {
			filter = STAILQ_FIRST(&vnic->filter);
			while (filter) {
				temp_filter = STAILQ_NEXT(filter, next);
				if (filter->mac_index == index) {
					STAILQ_REMOVE(&vnic->filter, filter,
						      bnxt_filter_info, next);
					bnxt_hwrm_clear_l2_filter(bp, filter);
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
		PMD_DRV_LOG(ERR, "Cannot add MAC address to a VF interface\n");
		return -ENOTSUP;
	}

	if (!vnic) {
		PMD_DRV_LOG(ERR, "VNIC not found for pool %d!\n", pool);
		return -EINVAL;
	}
	/* Attach requested MAC address to the new l2_filter */
	STAILQ_FOREACH(filter, &vnic->filter, next) {
		if (filter->mac_index == index) {
			PMD_DRV_LOG(ERR,
				"MAC addr already existed for pool %d\n", pool);
			return 0;
		}
	}
	filter = bnxt_alloc_filter(bp);
	if (!filter) {
		PMD_DRV_LOG(ERR, "L2 filter alloc failed\n");
		return -ENODEV;
	}
	STAILQ_INSERT_TAIL(&vnic->filter, filter, next);
	filter->mac_index = index;
	memcpy(filter->l2_addr, mac_addr, ETHER_ADDR_LEN);
	return bnxt_hwrm_set_l2_filter(bp, vnic->fw_vnic_id, filter);
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
			PMD_DRV_LOG(ERR,
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
		memcpy(&eth_dev->data->dev_link, &new,
			sizeof(struct rte_eth_link));

		_rte_eth_dev_callback_process(eth_dev,
					      RTE_ETH_EVENT_INTR_LSC,
					      NULL);

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
		PMD_DRV_LOG(ERR, "The configured hash table lookup size "
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
		PMD_DRV_LOG(ERR, "The configured hash table lookup size "
			"(%d) must equal the size supported by the hardware "
			"(%d)\n", reta_size, HW_HASH_INDEX_SIZE);
		return -EINVAL;
	}
	/* EW - need to revisit here copying from uint64_t to uint16_t */
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
			PMD_DRV_LOG(ERR, "Hash type NONE\n");
	} else {
		if (rss_conf->rss_hf & BNXT_ETH_RSS_SUPPORT)
			return -EINVAL;
	}

	bp->flags |= BNXT_FLAG_UPDATE_HASH;
	memcpy(&bp->rss_conf, rss_conf, sizeof(*rss_conf));

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
			PMD_DRV_LOG(ERR,
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

	if (!BNXT_SINGLE_PF(bp) || BNXT_VF(bp)) {
		PMD_DRV_LOG(ERR, "Flow Control Settings cannot be modified\n");
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
			PMD_DRV_LOG(ERR, "Tunnel Port %d already programmed\n",
				udp_tunnel->udp_port);
			if (bp->vxlan_port != udp_tunnel->udp_port) {
				PMD_DRV_LOG(ERR, "Only one port allowed\n");
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
			PMD_DRV_LOG(ERR, "Tunnel Port %d already programmed\n",
				udp_tunnel->udp_port);
			if (bp->geneve_port != udp_tunnel->udp_port) {
				PMD_DRV_LOG(ERR, "Only one port allowed\n");
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
		PMD_DRV_LOG(ERR, "Tunnel type is not supported\n");
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
			PMD_DRV_LOG(ERR, "No Tunnel port configured yet\n");
			return -EINVAL;
		}
		if (bp->vxlan_port != udp_tunnel->udp_port) {
			PMD_DRV_LOG(ERR, "Req Port: %d. Configured port: %d\n",
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
			PMD_DRV_LOG(ERR, "No Tunnel port configured yet\n");
			return -EINVAL;
		}
		if (bp->geneve_port != udp_tunnel->udp_port) {
			PMD_DRV_LOG(ERR, "Req Port: %d. Configured port: %d\n",
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
		PMD_DRV_LOG(ERR, "Tunnel type is not supported\n");
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
					bnxt_hwrm_clear_l2_filter(bp, filter);
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
						PMD_DRV_LOG(ERR,
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
					rc = bnxt_hwrm_set_l2_filter(bp,
							vnic->fw_vnic_id,
							new_filter);
					if (rc)
						goto exit;
					PMD_DRV_LOG(INFO,
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
					bnxt_hwrm_clear_l2_filter(bp, filter);
					filter->l2_ovlan = 0;
					STAILQ_INSERT_TAIL(
							&bp->free_filter_list,
							filter, next);
				}
				new_filter = bnxt_alloc_filter(bp);
				if (!new_filter) {
					PMD_DRV_LOG(ERR,
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
				rc = bnxt_hwrm_set_l2_filter(bp,
							     vnic->fw_vnic_id,
							     new_filter);
				if (rc)
					goto exit;
				PMD_DRV_LOG(INFO,
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

static int
bnxt_vlan_offload_set_op(struct rte_eth_dev *dev, int mask)
{
	struct bnxt *bp = (struct bnxt *)dev->data->dev_private;
	uint64_t rx_offloads = dev->data->dev_conf.rxmode.offloads;
	unsigned int i;

	if (mask & ETH_VLAN_FILTER_MASK) {
		if (!(rx_offloads & DEV_RX_OFFLOAD_VLAN_FILTER)) {
			/* Remove any VLAN filters programmed */
			for (i = 0; i < 4095; i++)
				bnxt_del_vlan_filter(bp, i);
		}
		PMD_DRV_LOG(DEBUG, "VLAN Filtering: %d\n",
			!!(rx_offloads & DEV_RX_OFFLOAD_VLAN_FILTER));
	}

	if (mask & ETH_VLAN_STRIP_MASK) {
		/* Enable or disable VLAN stripping */
		for (i = 0; i < bp->nr_vnics; i++) {
			struct bnxt_vnic_info *vnic = &bp->vnic_info[i];
			if (rx_offloads & DEV_RX_OFFLOAD_VLAN_STRIP)
				vnic->vlan_strip = true;
			else
				vnic->vlan_strip = false;
			bnxt_hwrm_vnic_cfg(bp, vnic);
		}
		PMD_DRV_LOG(DEBUG, "VLAN Strip Offload: %d\n",
			!!(rx_offloads & DEV_RX_OFFLOAD_VLAN_STRIP));
	}

	if (mask & ETH_VLAN_EXTEND_MASK)
		PMD_DRV_LOG(ERR, "Extend VLAN Not supported\n");

	return 0;
}

static int
bnxt_set_default_mac_addr_op(struct rte_eth_dev *dev, struct ether_addr *addr)
{
	struct bnxt *bp = (struct bnxt *)dev->data->dev_private;
	/* Default Filter is tied to VNIC 0 */
	struct bnxt_vnic_info *vnic = &bp->vnic_info[0];
	struct bnxt_filter_info *filter;
	int rc;

	if (BNXT_VF(bp))
		return -EPERM;

	memcpy(bp->mac_addr, addr, sizeof(bp->mac_addr));

	STAILQ_FOREACH(filter, &vnic->filter, next) {
		/* Default Filter is at Index 0 */
		if (filter->mac_index != 0)
			continue;
		rc = bnxt_hwrm_clear_l2_filter(bp, filter);
		if (rc)
			return rc;
		memcpy(filter->l2_addr, bp->mac_addr, ETHER_ADDR_LEN);
		memset(filter->l2_addr_mask, 0xff, ETHER_ADDR_LEN);
		filter->flags |= HWRM_CFA_L2_FILTER_ALLOC_INPUT_FLAGS_PATH_RX;
		filter->enables |=
			HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_ADDR |
			HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_ADDR_MASK;
		rc = bnxt_hwrm_set_l2_filter(bp, vnic->fw_vnic_id, filter);
		if (rc)
			return rc;
		filter->mac_index = 0;
		PMD_DRV_LOG(DEBUG, "Set MAC addr\n");
	}

	return 0;
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
		PMD_DRV_LOG(ERR, "MTU requested must be within (%d, %d)\n",
			ETHER_MIN_MTU, max_dev_mtu);
		return -EINVAL;
	}


	if (new_mtu > ETHER_MTU) {
		bp->flags |= BNXT_FLAG_JUMBO;
		bp->eth_dev->data->dev_conf.rxmode.offloads |=
			DEV_RX_OFFLOAD_JUMBO_FRAME;
	} else {
		bp->eth_dev->data->dev_conf.rxmode.offloads &=
			~DEV_RX_OFFLOAD_JUMBO_FRAME;
		bp->flags &= ~BNXT_FLAG_JUMBO;
	}

	eth_dev->data->dev_conf.rxmode.max_rx_pkt_len =
		new_mtu + ETHER_HDR_LEN + ETHER_CRC_LEN + VLAN_TAG_SIZE * 2;

	eth_dev->data->mtu = new_mtu;
	PMD_DRV_LOG(INFO, "New MTU is %d\n", eth_dev->data->mtu);

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

	if (!BNXT_SINGLE_PF(bp) || BNXT_VF(bp)) {
		PMD_DRV_LOG(ERR,
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

static uint32_t
bnxt_rx_queue_count_op(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	uint32_t desc = 0, raw_cons = 0, cons;
	struct bnxt_cp_ring_info *cpr;
	struct bnxt_rx_queue *rxq;
	struct rx_pkt_cmpl *rxcmp;
	uint16_t cmp_type;
	uint8_t cmp = 1;
	bool valid;

	rxq = dev->data->rx_queues[rx_queue_id];
	cpr = rxq->cp_ring;
	valid = cpr->valid;

	while (raw_cons < rxq->nb_rx_desc) {
		cons = RING_CMP(cpr->cp_ring_struct, raw_cons);
		rxcmp = (struct rx_pkt_cmpl *)&cpr->cp_desc_ring[cons];

		if (!CMPL_VALID(rxcmp, valid))
			goto nothing_to_do;
		valid = FLIP_VALID(cons, cpr->cp_ring_struct->ring_mask, valid);
		cmp_type = CMP_TYPE(rxcmp);
		if (cmp_type == RX_TPA_END_CMPL_TYPE_RX_TPA_END) {
			cmp = (rte_le_to_cpu_32(
					((struct rx_tpa_end_cmpl *)
					 (rxcmp))->agg_bufs_v1) &
			       RX_TPA_END_CMPL_AGG_BUFS_MASK) >>
				RX_TPA_END_CMPL_AGG_BUFS_SFT;
			desc++;
		} else if (cmp_type == 0x11) {
			desc++;
			cmp = (rxcmp->agg_bufs_v1 &
				   RX_PKT_CMPL_AGG_BUFS_MASK) >>
				RX_PKT_CMPL_AGG_BUFS_SFT;
		} else {
			cmp = 1;
		}
nothing_to_do:
		raw_cons += cmp ? cmp : 2;
	}

	return desc;
}

static int
bnxt_rx_descriptor_status_op(void *rx_queue, uint16_t offset)
{
	struct bnxt_rx_queue *rxq = (struct bnxt_rx_queue *)rx_queue;
	struct bnxt_rx_ring_info *rxr;
	struct bnxt_cp_ring_info *cpr;
	struct bnxt_sw_rx_bd *rx_buf;
	struct rx_pkt_cmpl *rxcmp;
	uint32_t cons, cp_cons;

	if (!rxq)
		return -EINVAL;

	cpr = rxq->cp_ring;
	rxr = rxq->rx_ring;

	if (offset >= rxq->nb_rx_desc)
		return -EINVAL;

	cons = RING_CMP(cpr->cp_ring_struct, offset);
	cp_cons = cpr->cp_raw_cons;
	rxcmp = (struct rx_pkt_cmpl *)&cpr->cp_desc_ring[cons];

	if (cons > cp_cons) {
		if (CMPL_VALID(rxcmp, cpr->valid))
			return RTE_ETH_RX_DESC_DONE;
	} else {
		if (CMPL_VALID(rxcmp, !cpr->valid))
			return RTE_ETH_RX_DESC_DONE;
	}
	rx_buf = &rxr->rx_buf_ring[cons];
	if (rx_buf->mbuf == NULL)
		return RTE_ETH_RX_DESC_UNAVAIL;


	return RTE_ETH_RX_DESC_AVAIL;
}

static int
bnxt_tx_descriptor_status_op(void *tx_queue, uint16_t offset)
{
	struct bnxt_tx_queue *txq = (struct bnxt_tx_queue *)tx_queue;
	struct bnxt_tx_ring_info *txr;
	struct bnxt_cp_ring_info *cpr;
	struct bnxt_sw_tx_bd *tx_buf;
	struct tx_pkt_cmpl *txcmp;
	uint32_t cons, cp_cons;

	if (!txq)
		return -EINVAL;

	cpr = txq->cp_ring;
	txr = txq->tx_ring;

	if (offset >= txq->nb_tx_desc)
		return -EINVAL;

	cons = RING_CMP(cpr->cp_ring_struct, offset);
	txcmp = (struct tx_pkt_cmpl *)&cpr->cp_desc_ring[cons];
	cp_cons = cpr->cp_raw_cons;

	if (cons > cp_cons) {
		if (CMPL_VALID(txcmp, cpr->valid))
			return RTE_ETH_TX_DESC_UNAVAIL;
	} else {
		if (CMPL_VALID(txcmp, !cpr->valid))
			return RTE_ETH_TX_DESC_UNAVAIL;
	}
	tx_buf = &txr->tx_buf_ring[cons];
	if (tx_buf->mbuf == NULL)
		return RTE_ETH_TX_DESC_DONE;

	return RTE_ETH_TX_DESC_FULL;
}

static struct bnxt_filter_info *
bnxt_match_and_validate_ether_filter(struct bnxt *bp,
				struct rte_eth_ethertype_filter *efilter,
				struct bnxt_vnic_info *vnic0,
				struct bnxt_vnic_info *vnic,
				int *ret)
{
	struct bnxt_filter_info *mfilter = NULL;
	int match = 0;
	*ret = 0;

	if (efilter->ether_type == ETHER_TYPE_IPv4 ||
		efilter->ether_type == ETHER_TYPE_IPv6) {
		PMD_DRV_LOG(ERR, "invalid ether_type(0x%04x) in"
			" ethertype filter.", efilter->ether_type);
		*ret = -EINVAL;
		goto exit;
	}
	if (efilter->queue >= bp->rx_nr_rings) {
		PMD_DRV_LOG(ERR, "Invalid queue %d\n", efilter->queue);
		*ret = -EINVAL;
		goto exit;
	}

	vnic0 = STAILQ_FIRST(&bp->ff_pool[0]);
	vnic = STAILQ_FIRST(&bp->ff_pool[efilter->queue]);
	if (vnic == NULL) {
		PMD_DRV_LOG(ERR, "Invalid queue %d\n", efilter->queue);
		*ret = -EINVAL;
		goto exit;
	}

	if (efilter->flags & RTE_ETHTYPE_FLAGS_DROP) {
		STAILQ_FOREACH(mfilter, &vnic0->filter, next) {
			if ((!memcmp(efilter->mac_addr.addr_bytes,
				     mfilter->l2_addr, ETHER_ADDR_LEN) &&
			     mfilter->flags ==
			     HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_FLAGS_DROP &&
			     mfilter->ethertype == efilter->ether_type)) {
				match = 1;
				break;
			}
		}
	} else {
		STAILQ_FOREACH(mfilter, &vnic->filter, next)
			if ((!memcmp(efilter->mac_addr.addr_bytes,
				     mfilter->l2_addr, ETHER_ADDR_LEN) &&
			     mfilter->ethertype == efilter->ether_type &&
			     mfilter->flags ==
			     HWRM_CFA_L2_FILTER_CFG_INPUT_FLAGS_PATH_RX)) {
				match = 1;
				break;
			}
	}

	if (match)
		*ret = -EEXIST;

exit:
	return mfilter;
}

static int
bnxt_ethertype_filter(struct rte_eth_dev *dev,
			enum rte_filter_op filter_op,
			void *arg)
{
	struct bnxt *bp = (struct bnxt *)dev->data->dev_private;
	struct rte_eth_ethertype_filter *efilter =
			(struct rte_eth_ethertype_filter *)arg;
	struct bnxt_filter_info *bfilter, *filter1;
	struct bnxt_vnic_info *vnic, *vnic0;
	int ret;

	if (filter_op == RTE_ETH_FILTER_NOP)
		return 0;

	if (arg == NULL) {
		PMD_DRV_LOG(ERR, "arg shouldn't be NULL for operation %u.",
			    filter_op);
		return -EINVAL;
	}

	vnic0 = STAILQ_FIRST(&bp->ff_pool[0]);
	vnic = STAILQ_FIRST(&bp->ff_pool[efilter->queue]);

	switch (filter_op) {
	case RTE_ETH_FILTER_ADD:
		bnxt_match_and_validate_ether_filter(bp, efilter,
							vnic0, vnic, &ret);
		if (ret < 0)
			return ret;

		bfilter = bnxt_get_unused_filter(bp);
		if (bfilter == NULL) {
			PMD_DRV_LOG(ERR,
				"Not enough resources for a new filter.\n");
			return -ENOMEM;
		}
		bfilter->filter_type = HWRM_CFA_NTUPLE_FILTER;
		memcpy(bfilter->l2_addr, efilter->mac_addr.addr_bytes,
		       ETHER_ADDR_LEN);
		memcpy(bfilter->dst_macaddr, efilter->mac_addr.addr_bytes,
		       ETHER_ADDR_LEN);
		bfilter->enables |= NTUPLE_FLTR_ALLOC_INPUT_EN_DST_MACADDR;
		bfilter->ethertype = efilter->ether_type;
		bfilter->enables |= NTUPLE_FLTR_ALLOC_INPUT_EN_ETHERTYPE;

		filter1 = bnxt_get_l2_filter(bp, bfilter, vnic0);
		if (filter1 == NULL) {
			ret = -1;
			goto cleanup;
		}
		bfilter->enables |=
			HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_L2_FILTER_ID;
		bfilter->fw_l2_filter_id = filter1->fw_l2_filter_id;

		bfilter->dst_id = vnic->fw_vnic_id;

		if (efilter->flags & RTE_ETHTYPE_FLAGS_DROP) {
			bfilter->flags =
				HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_FLAGS_DROP;
		}

		ret = bnxt_hwrm_set_ntuple_filter(bp, bfilter->dst_id, bfilter);
		if (ret)
			goto cleanup;
		STAILQ_INSERT_TAIL(&vnic->filter, bfilter, next);
		break;
	case RTE_ETH_FILTER_DELETE:
		filter1 = bnxt_match_and_validate_ether_filter(bp, efilter,
							vnic0, vnic, &ret);
		if (ret == -EEXIST) {
			ret = bnxt_hwrm_clear_ntuple_filter(bp, filter1);

			STAILQ_REMOVE(&vnic->filter, filter1, bnxt_filter_info,
				      next);
			bnxt_free_filter(bp, filter1);
		} else if (ret == 0) {
			PMD_DRV_LOG(ERR, "No matching filter found\n");
		}
		break;
	default:
		PMD_DRV_LOG(ERR, "unsupported operation %u.", filter_op);
		ret = -EINVAL;
		goto error;
	}
	return ret;
cleanup:
	bnxt_free_filter(bp, bfilter);
error:
	return ret;
}

static inline int
parse_ntuple_filter(struct bnxt *bp,
		    struct rte_eth_ntuple_filter *nfilter,
		    struct bnxt_filter_info *bfilter)
{
	uint32_t en = 0;

	if (nfilter->queue >= bp->rx_nr_rings) {
		PMD_DRV_LOG(ERR, "Invalid queue %d\n", nfilter->queue);
		return -EINVAL;
	}

	switch (nfilter->dst_port_mask) {
	case UINT16_MAX:
		bfilter->dst_port_mask = -1;
		bfilter->dst_port = nfilter->dst_port;
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_DST_PORT |
			NTUPLE_FLTR_ALLOC_INPUT_EN_DST_PORT_MASK;
		break;
	default:
		PMD_DRV_LOG(ERR, "invalid dst_port mask.");
		return -EINVAL;
	}

	bfilter->ip_addr_type = NTUPLE_FLTR_ALLOC_INPUT_IP_ADDR_TYPE_IPV4;
	en |= NTUPLE_FLTR_ALLOC_IN_EN_IP_PROTO;

	switch (nfilter->proto_mask) {
	case UINT8_MAX:
		if (nfilter->proto == 17) /* IPPROTO_UDP */
			bfilter->ip_protocol = 17;
		else if (nfilter->proto == 6) /* IPPROTO_TCP */
			bfilter->ip_protocol = 6;
		else
			return -EINVAL;
		en |= NTUPLE_FLTR_ALLOC_IN_EN_IP_PROTO;
		break;
	default:
		PMD_DRV_LOG(ERR, "invalid protocol mask.");
		return -EINVAL;
	}

	switch (nfilter->dst_ip_mask) {
	case UINT32_MAX:
		bfilter->dst_ipaddr_mask[0] = -1;
		bfilter->dst_ipaddr[0] = nfilter->dst_ip;
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_DST_IPADDR |
			NTUPLE_FLTR_ALLOC_INPUT_EN_DST_IPADDR_MASK;
		break;
	default:
		PMD_DRV_LOG(ERR, "invalid dst_ip mask.");
		return -EINVAL;
	}

	switch (nfilter->src_ip_mask) {
	case UINT32_MAX:
		bfilter->src_ipaddr_mask[0] = -1;
		bfilter->src_ipaddr[0] = nfilter->src_ip;
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_SRC_IPADDR |
			NTUPLE_FLTR_ALLOC_INPUT_EN_SRC_IPADDR_MASK;
		break;
	default:
		PMD_DRV_LOG(ERR, "invalid src_ip mask.");
		return -EINVAL;
	}

	switch (nfilter->src_port_mask) {
	case UINT16_MAX:
		bfilter->src_port_mask = -1;
		bfilter->src_port = nfilter->src_port;
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_SRC_PORT |
			NTUPLE_FLTR_ALLOC_INPUT_EN_SRC_PORT_MASK;
		break;
	default:
		PMD_DRV_LOG(ERR, "invalid src_port mask.");
		return -EINVAL;
	}

	//TODO Priority
	//nfilter->priority = (uint8_t)filter->priority;

	bfilter->enables = en;
	return 0;
}

static struct bnxt_filter_info*
bnxt_match_ntuple_filter(struct bnxt *bp,
			 struct bnxt_filter_info *bfilter,
			 struct bnxt_vnic_info **mvnic)
{
	struct bnxt_filter_info *mfilter = NULL;
	int i;

	for (i = bp->nr_vnics - 1; i >= 0; i--) {
		struct bnxt_vnic_info *vnic = &bp->vnic_info[i];
		STAILQ_FOREACH(mfilter, &vnic->filter, next) {
			if (bfilter->src_ipaddr[0] == mfilter->src_ipaddr[0] &&
			    bfilter->src_ipaddr_mask[0] ==
			    mfilter->src_ipaddr_mask[0] &&
			    bfilter->src_port == mfilter->src_port &&
			    bfilter->src_port_mask == mfilter->src_port_mask &&
			    bfilter->dst_ipaddr[0] == mfilter->dst_ipaddr[0] &&
			    bfilter->dst_ipaddr_mask[0] ==
			    mfilter->dst_ipaddr_mask[0] &&
			    bfilter->dst_port == mfilter->dst_port &&
			    bfilter->dst_port_mask == mfilter->dst_port_mask &&
			    bfilter->flags == mfilter->flags &&
			    bfilter->enables == mfilter->enables) {
				if (mvnic)
					*mvnic = vnic;
				return mfilter;
			}
		}
	}
	return NULL;
}

static int
bnxt_cfg_ntuple_filter(struct bnxt *bp,
		       struct rte_eth_ntuple_filter *nfilter,
		       enum rte_filter_op filter_op)
{
	struct bnxt_filter_info *bfilter, *mfilter, *filter1;
	struct bnxt_vnic_info *vnic, *vnic0, *mvnic;
	int ret;

	if (nfilter->flags != RTE_5TUPLE_FLAGS) {
		PMD_DRV_LOG(ERR, "only 5tuple is supported.");
		return -EINVAL;
	}

	if (nfilter->flags & RTE_NTUPLE_FLAGS_TCP_FLAG) {
		PMD_DRV_LOG(ERR, "Ntuple filter: TCP flags not supported\n");
		return -EINVAL;
	}

	bfilter = bnxt_get_unused_filter(bp);
	if (bfilter == NULL) {
		PMD_DRV_LOG(ERR,
			"Not enough resources for a new filter.\n");
		return -ENOMEM;
	}
	ret = parse_ntuple_filter(bp, nfilter, bfilter);
	if (ret < 0)
		goto free_filter;

	vnic = STAILQ_FIRST(&bp->ff_pool[nfilter->queue]);
	vnic0 = STAILQ_FIRST(&bp->ff_pool[0]);
	filter1 = STAILQ_FIRST(&vnic0->filter);
	if (filter1 == NULL) {
		ret = -1;
		goto free_filter;
	}

	bfilter->dst_id = vnic->fw_vnic_id;
	bfilter->fw_l2_filter_id = filter1->fw_l2_filter_id;
	bfilter->enables |=
		HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_L2_FILTER_ID;
	bfilter->ethertype = 0x800;
	bfilter->enables |= NTUPLE_FLTR_ALLOC_INPUT_EN_ETHERTYPE;

	mfilter = bnxt_match_ntuple_filter(bp, bfilter, &mvnic);

	if (mfilter != NULL && filter_op == RTE_ETH_FILTER_ADD &&
	    bfilter->dst_id == mfilter->dst_id) {
		PMD_DRV_LOG(ERR, "filter exists.\n");
		ret = -EEXIST;
		goto free_filter;
	} else if (mfilter != NULL && filter_op == RTE_ETH_FILTER_ADD &&
		   bfilter->dst_id != mfilter->dst_id) {
		mfilter->dst_id = vnic->fw_vnic_id;
		ret = bnxt_hwrm_set_ntuple_filter(bp, mfilter->dst_id, mfilter);
		STAILQ_REMOVE(&mvnic->filter, mfilter, bnxt_filter_info, next);
		STAILQ_INSERT_TAIL(&vnic->filter, mfilter, next);
		PMD_DRV_LOG(ERR, "filter with matching pattern exists.\n");
		PMD_DRV_LOG(ERR, " Updated it to the new destination queue\n");
		goto free_filter;
	}
	if (mfilter == NULL && filter_op == RTE_ETH_FILTER_DELETE) {
		PMD_DRV_LOG(ERR, "filter doesn't exist.");
		ret = -ENOENT;
		goto free_filter;
	}

	if (filter_op == RTE_ETH_FILTER_ADD) {
		bfilter->filter_type = HWRM_CFA_NTUPLE_FILTER;
		ret = bnxt_hwrm_set_ntuple_filter(bp, bfilter->dst_id, bfilter);
		if (ret)
			goto free_filter;
		STAILQ_INSERT_TAIL(&vnic->filter, bfilter, next);
	} else {
		if (mfilter == NULL) {
			/* This should not happen. But for Coverity! */
			ret = -ENOENT;
			goto free_filter;
		}
		ret = bnxt_hwrm_clear_ntuple_filter(bp, mfilter);

		STAILQ_REMOVE(&vnic->filter, mfilter, bnxt_filter_info, next);
		bnxt_free_filter(bp, mfilter);
		mfilter->fw_l2_filter_id = -1;
		bnxt_free_filter(bp, bfilter);
		bfilter->fw_l2_filter_id = -1;
	}

	return 0;
free_filter:
	bfilter->fw_l2_filter_id = -1;
	bnxt_free_filter(bp, bfilter);
	return ret;
}

static int
bnxt_ntuple_filter(struct rte_eth_dev *dev,
			enum rte_filter_op filter_op,
			void *arg)
{
	struct bnxt *bp = (struct bnxt *)dev->data->dev_private;
	int ret;

	if (filter_op == RTE_ETH_FILTER_NOP)
		return 0;

	if (arg == NULL) {
		PMD_DRV_LOG(ERR, "arg shouldn't be NULL for operation %u.",
			    filter_op);
		return -EINVAL;
	}

	switch (filter_op) {
	case RTE_ETH_FILTER_ADD:
		ret = bnxt_cfg_ntuple_filter(bp,
			(struct rte_eth_ntuple_filter *)arg,
			filter_op);
		break;
	case RTE_ETH_FILTER_DELETE:
		ret = bnxt_cfg_ntuple_filter(bp,
			(struct rte_eth_ntuple_filter *)arg,
			filter_op);
		break;
	default:
		PMD_DRV_LOG(ERR, "unsupported operation %u.", filter_op);
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int
bnxt_parse_fdir_filter(struct bnxt *bp,
		       struct rte_eth_fdir_filter *fdir,
		       struct bnxt_filter_info *filter)
{
	enum rte_fdir_mode fdir_mode =
		bp->eth_dev->data->dev_conf.fdir_conf.mode;
	struct bnxt_vnic_info *vnic0, *vnic;
	struct bnxt_filter_info *filter1;
	uint32_t en = 0;
	int i;

	if (fdir_mode == RTE_FDIR_MODE_PERFECT_TUNNEL)
		return -EINVAL;

	filter->l2_ovlan = fdir->input.flow_ext.vlan_tci;
	en |= EM_FLOW_ALLOC_INPUT_EN_OVLAN_VID;

	switch (fdir->input.flow_type) {
	case RTE_ETH_FLOW_IPV4:
	case RTE_ETH_FLOW_NONFRAG_IPV4_OTHER:
		/* FALLTHROUGH */
		filter->src_ipaddr[0] = fdir->input.flow.ip4_flow.src_ip;
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_SRC_IPADDR;
		filter->dst_ipaddr[0] = fdir->input.flow.ip4_flow.dst_ip;
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_DST_IPADDR;
		filter->ip_protocol = fdir->input.flow.ip4_flow.proto;
		en |= NTUPLE_FLTR_ALLOC_IN_EN_IP_PROTO;
		filter->ip_addr_type =
			NTUPLE_FLTR_ALLOC_INPUT_IP_ADDR_TYPE_IPV4;
		filter->src_ipaddr_mask[0] = 0xffffffff;
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_SRC_IPADDR_MASK;
		filter->dst_ipaddr_mask[0] = 0xffffffff;
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_DST_IPADDR_MASK;
		filter->ethertype = 0x800;
		filter->enables |= NTUPLE_FLTR_ALLOC_INPUT_EN_ETHERTYPE;
		break;
	case RTE_ETH_FLOW_NONFRAG_IPV4_TCP:
		filter->src_port = fdir->input.flow.tcp4_flow.src_port;
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_SRC_PORT;
		filter->dst_port = fdir->input.flow.tcp4_flow.dst_port;
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_DST_PORT;
		filter->dst_port_mask = 0xffff;
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_DST_PORT_MASK;
		filter->src_port_mask = 0xffff;
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_SRC_PORT_MASK;
		filter->src_ipaddr[0] = fdir->input.flow.tcp4_flow.ip.src_ip;
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_SRC_IPADDR;
		filter->dst_ipaddr[0] = fdir->input.flow.tcp4_flow.ip.dst_ip;
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_DST_IPADDR;
		filter->ip_protocol = 6;
		en |= NTUPLE_FLTR_ALLOC_IN_EN_IP_PROTO;
		filter->ip_addr_type =
			NTUPLE_FLTR_ALLOC_INPUT_IP_ADDR_TYPE_IPV4;
		filter->src_ipaddr_mask[0] = 0xffffffff;
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_SRC_IPADDR_MASK;
		filter->dst_ipaddr_mask[0] = 0xffffffff;
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_DST_IPADDR_MASK;
		filter->ethertype = 0x800;
		filter->enables |= NTUPLE_FLTR_ALLOC_INPUT_EN_ETHERTYPE;
		break;
	case RTE_ETH_FLOW_NONFRAG_IPV4_UDP:
		filter->src_port = fdir->input.flow.udp4_flow.src_port;
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_SRC_PORT;
		filter->dst_port = fdir->input.flow.udp4_flow.dst_port;
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_DST_PORT;
		filter->dst_port_mask = 0xffff;
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_DST_PORT_MASK;
		filter->src_port_mask = 0xffff;
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_SRC_PORT_MASK;
		filter->src_ipaddr[0] = fdir->input.flow.udp4_flow.ip.src_ip;
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_SRC_IPADDR;
		filter->dst_ipaddr[0] = fdir->input.flow.udp4_flow.ip.dst_ip;
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_DST_IPADDR;
		filter->ip_protocol = 17;
		en |= NTUPLE_FLTR_ALLOC_IN_EN_IP_PROTO;
		filter->ip_addr_type =
			NTUPLE_FLTR_ALLOC_INPUT_IP_ADDR_TYPE_IPV4;
		filter->src_ipaddr_mask[0] = 0xffffffff;
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_SRC_IPADDR_MASK;
		filter->dst_ipaddr_mask[0] = 0xffffffff;
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_DST_IPADDR_MASK;
		filter->ethertype = 0x800;
		filter->enables |= NTUPLE_FLTR_ALLOC_INPUT_EN_ETHERTYPE;
		break;
	case RTE_ETH_FLOW_IPV6:
	case RTE_ETH_FLOW_NONFRAG_IPV6_OTHER:
		/* FALLTHROUGH */
		filter->ip_addr_type =
			NTUPLE_FLTR_ALLOC_INPUT_IP_ADDR_TYPE_IPV6;
		filter->ip_protocol = fdir->input.flow.ipv6_flow.proto;
		en |= NTUPLE_FLTR_ALLOC_IN_EN_IP_PROTO;
		rte_memcpy(filter->src_ipaddr,
			   fdir->input.flow.ipv6_flow.src_ip, 16);
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_SRC_IPADDR;
		rte_memcpy(filter->dst_ipaddr,
			   fdir->input.flow.ipv6_flow.dst_ip, 16);
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_DST_IPADDR;
		memset(filter->dst_ipaddr_mask, 0xff, 16);
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_DST_IPADDR_MASK;
		memset(filter->src_ipaddr_mask, 0xff, 16);
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_SRC_IPADDR_MASK;
		filter->ethertype = 0x86dd;
		filter->enables |= NTUPLE_FLTR_ALLOC_INPUT_EN_ETHERTYPE;
		break;
	case RTE_ETH_FLOW_NONFRAG_IPV6_TCP:
		filter->src_port = fdir->input.flow.tcp6_flow.src_port;
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_SRC_PORT;
		filter->dst_port = fdir->input.flow.tcp6_flow.dst_port;
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_DST_PORT;
		filter->dst_port_mask = 0xffff;
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_DST_PORT_MASK;
		filter->src_port_mask = 0xffff;
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_SRC_PORT_MASK;
		filter->ip_addr_type =
			NTUPLE_FLTR_ALLOC_INPUT_IP_ADDR_TYPE_IPV6;
		filter->ip_protocol = fdir->input.flow.tcp6_flow.ip.proto;
		en |= NTUPLE_FLTR_ALLOC_IN_EN_IP_PROTO;
		rte_memcpy(filter->src_ipaddr,
			   fdir->input.flow.tcp6_flow.ip.src_ip, 16);
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_SRC_IPADDR;
		rte_memcpy(filter->dst_ipaddr,
			   fdir->input.flow.tcp6_flow.ip.dst_ip, 16);
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_DST_IPADDR;
		memset(filter->dst_ipaddr_mask, 0xff, 16);
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_DST_IPADDR_MASK;
		memset(filter->src_ipaddr_mask, 0xff, 16);
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_SRC_IPADDR_MASK;
		filter->ethertype = 0x86dd;
		filter->enables |= NTUPLE_FLTR_ALLOC_INPUT_EN_ETHERTYPE;
		break;
	case RTE_ETH_FLOW_NONFRAG_IPV6_UDP:
		filter->src_port = fdir->input.flow.udp6_flow.src_port;
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_SRC_PORT;
		filter->dst_port = fdir->input.flow.udp6_flow.dst_port;
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_DST_PORT;
		filter->dst_port_mask = 0xffff;
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_DST_PORT_MASK;
		filter->src_port_mask = 0xffff;
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_SRC_PORT_MASK;
		filter->ip_addr_type =
			NTUPLE_FLTR_ALLOC_INPUT_IP_ADDR_TYPE_IPV6;
		filter->ip_protocol = fdir->input.flow.udp6_flow.ip.proto;
		en |= NTUPLE_FLTR_ALLOC_IN_EN_IP_PROTO;
		rte_memcpy(filter->src_ipaddr,
			   fdir->input.flow.udp6_flow.ip.src_ip, 16);
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_SRC_IPADDR;
		rte_memcpy(filter->dst_ipaddr,
			   fdir->input.flow.udp6_flow.ip.dst_ip, 16);
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_DST_IPADDR;
		memset(filter->dst_ipaddr_mask, 0xff, 16);
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_DST_IPADDR_MASK;
		memset(filter->src_ipaddr_mask, 0xff, 16);
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_SRC_IPADDR_MASK;
		filter->ethertype = 0x86dd;
		filter->enables |= NTUPLE_FLTR_ALLOC_INPUT_EN_ETHERTYPE;
		break;
	case RTE_ETH_FLOW_L2_PAYLOAD:
		filter->ethertype = fdir->input.flow.l2_flow.ether_type;
		en |= NTUPLE_FLTR_ALLOC_INPUT_EN_ETHERTYPE;
		break;
	case RTE_ETH_FLOW_VXLAN:
		if (fdir->action.behavior == RTE_ETH_FDIR_REJECT)
			return -EINVAL;
		filter->vni = fdir->input.flow.tunnel_flow.tunnel_id;
		filter->tunnel_type =
			CFA_NTUPLE_FILTER_ALLOC_REQ_TUNNEL_TYPE_VXLAN;
		en |= HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_TUNNEL_TYPE;
		break;
	case RTE_ETH_FLOW_NVGRE:
		if (fdir->action.behavior == RTE_ETH_FDIR_REJECT)
			return -EINVAL;
		filter->vni = fdir->input.flow.tunnel_flow.tunnel_id;
		filter->tunnel_type =
			CFA_NTUPLE_FILTER_ALLOC_REQ_TUNNEL_TYPE_NVGRE;
		en |= HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_TUNNEL_TYPE;
		break;
	case RTE_ETH_FLOW_UNKNOWN:
	case RTE_ETH_FLOW_RAW:
	case RTE_ETH_FLOW_FRAG_IPV4:
	case RTE_ETH_FLOW_NONFRAG_IPV4_SCTP:
	case RTE_ETH_FLOW_FRAG_IPV6:
	case RTE_ETH_FLOW_NONFRAG_IPV6_SCTP:
	case RTE_ETH_FLOW_IPV6_EX:
	case RTE_ETH_FLOW_IPV6_TCP_EX:
	case RTE_ETH_FLOW_IPV6_UDP_EX:
	case RTE_ETH_FLOW_GENEVE:
		/* FALLTHROUGH */
	default:
		return -EINVAL;
	}

	vnic0 = STAILQ_FIRST(&bp->ff_pool[0]);
	vnic = STAILQ_FIRST(&bp->ff_pool[fdir->action.rx_queue]);
	if (vnic == NULL) {
		PMD_DRV_LOG(ERR, "Invalid queue %d\n", fdir->action.rx_queue);
		return -EINVAL;
	}


	if (fdir_mode == RTE_FDIR_MODE_PERFECT_MAC_VLAN) {
		rte_memcpy(filter->dst_macaddr,
			fdir->input.flow.mac_vlan_flow.mac_addr.addr_bytes, 6);
			en |= NTUPLE_FLTR_ALLOC_INPUT_EN_DST_MACADDR;
	}

	if (fdir->action.behavior == RTE_ETH_FDIR_REJECT) {
		filter->flags = HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_FLAGS_DROP;
		filter1 = STAILQ_FIRST(&vnic0->filter);
		//filter1 = bnxt_get_l2_filter(bp, filter, vnic0);
	} else {
		filter->dst_id = vnic->fw_vnic_id;
		for (i = 0; i < ETHER_ADDR_LEN; i++)
			if (filter->dst_macaddr[i] == 0x00)
				filter1 = STAILQ_FIRST(&vnic0->filter);
			else
				filter1 = bnxt_get_l2_filter(bp, filter, vnic);
	}

	if (filter1 == NULL)
		return -EINVAL;

	en |= HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_L2_FILTER_ID;
	filter->fw_l2_filter_id = filter1->fw_l2_filter_id;

	filter->enables = en;

	return 0;
}

static struct bnxt_filter_info *
bnxt_match_fdir(struct bnxt *bp, struct bnxt_filter_info *nf,
		struct bnxt_vnic_info **mvnic)
{
	struct bnxt_filter_info *mf = NULL;
	int i;

	for (i = bp->nr_vnics - 1; i >= 0; i--) {
		struct bnxt_vnic_info *vnic = &bp->vnic_info[i];

		STAILQ_FOREACH(mf, &vnic->filter, next) {
			if (mf->filter_type == nf->filter_type &&
			    mf->flags == nf->flags &&
			    mf->src_port == nf->src_port &&
			    mf->src_port_mask == nf->src_port_mask &&
			    mf->dst_port == nf->dst_port &&
			    mf->dst_port_mask == nf->dst_port_mask &&
			    mf->ip_protocol == nf->ip_protocol &&
			    mf->ip_addr_type == nf->ip_addr_type &&
			    mf->ethertype == nf->ethertype &&
			    mf->vni == nf->vni &&
			    mf->tunnel_type == nf->tunnel_type &&
			    mf->l2_ovlan == nf->l2_ovlan &&
			    mf->l2_ovlan_mask == nf->l2_ovlan_mask &&
			    mf->l2_ivlan == nf->l2_ivlan &&
			    mf->l2_ivlan_mask == nf->l2_ivlan_mask &&
			    !memcmp(mf->l2_addr, nf->l2_addr, ETHER_ADDR_LEN) &&
			    !memcmp(mf->l2_addr_mask, nf->l2_addr_mask,
				    ETHER_ADDR_LEN) &&
			    !memcmp(mf->src_macaddr, nf->src_macaddr,
				    ETHER_ADDR_LEN) &&
			    !memcmp(mf->dst_macaddr, nf->dst_macaddr,
				    ETHER_ADDR_LEN) &&
			    !memcmp(mf->src_ipaddr, nf->src_ipaddr,
				    sizeof(nf->src_ipaddr)) &&
			    !memcmp(mf->src_ipaddr_mask, nf->src_ipaddr_mask,
				    sizeof(nf->src_ipaddr_mask)) &&
			    !memcmp(mf->dst_ipaddr, nf->dst_ipaddr,
				    sizeof(nf->dst_ipaddr)) &&
			    !memcmp(mf->dst_ipaddr_mask, nf->dst_ipaddr_mask,
				    sizeof(nf->dst_ipaddr_mask))) {
				if (mvnic)
					*mvnic = vnic;
				return mf;
			}
		}
	}
	return NULL;
}

static int
bnxt_fdir_filter(struct rte_eth_dev *dev,
		 enum rte_filter_op filter_op,
		 void *arg)
{
	struct bnxt *bp = (struct bnxt *)dev->data->dev_private;
	struct rte_eth_fdir_filter *fdir  = (struct rte_eth_fdir_filter *)arg;
	struct bnxt_filter_info *filter, *match;
	struct bnxt_vnic_info *vnic, *mvnic;
	int ret = 0, i;

	if (filter_op == RTE_ETH_FILTER_NOP)
		return 0;

	if (arg == NULL && filter_op != RTE_ETH_FILTER_FLUSH)
		return -EINVAL;

	switch (filter_op) {
	case RTE_ETH_FILTER_ADD:
	case RTE_ETH_FILTER_DELETE:
		/* FALLTHROUGH */
		filter = bnxt_get_unused_filter(bp);
		if (filter == NULL) {
			PMD_DRV_LOG(ERR,
				"Not enough resources for a new flow.\n");
			return -ENOMEM;
		}

		ret = bnxt_parse_fdir_filter(bp, fdir, filter);
		if (ret != 0)
			goto free_filter;
		filter->filter_type = HWRM_CFA_NTUPLE_FILTER;

		if (fdir->action.behavior == RTE_ETH_FDIR_REJECT)
			vnic = STAILQ_FIRST(&bp->ff_pool[0]);
		else
			vnic = STAILQ_FIRST(&bp->ff_pool[fdir->action.rx_queue]);

		match = bnxt_match_fdir(bp, filter, &mvnic);
		if (match != NULL && filter_op == RTE_ETH_FILTER_ADD) {
			if (match->dst_id == vnic->fw_vnic_id) {
				PMD_DRV_LOG(ERR, "Flow already exists.\n");
				ret = -EEXIST;
				goto free_filter;
			} else {
				match->dst_id = vnic->fw_vnic_id;
				ret = bnxt_hwrm_set_ntuple_filter(bp,
								  match->dst_id,
								  match);
				STAILQ_REMOVE(&mvnic->filter, match,
					      bnxt_filter_info, next);
				STAILQ_INSERT_TAIL(&vnic->filter, match, next);
				PMD_DRV_LOG(ERR,
					"Filter with matching pattern exist\n");
				PMD_DRV_LOG(ERR,
					"Updated it to new destination q\n");
				goto free_filter;
			}
		}
		if (match == NULL && filter_op == RTE_ETH_FILTER_DELETE) {
			PMD_DRV_LOG(ERR, "Flow does not exist.\n");
			ret = -ENOENT;
			goto free_filter;
		}

		if (filter_op == RTE_ETH_FILTER_ADD) {
			ret = bnxt_hwrm_set_ntuple_filter(bp,
							  filter->dst_id,
							  filter);
			if (ret)
				goto free_filter;
			STAILQ_INSERT_TAIL(&vnic->filter, filter, next);
		} else {
			ret = bnxt_hwrm_clear_ntuple_filter(bp, match);
			STAILQ_REMOVE(&vnic->filter, match,
				      bnxt_filter_info, next);
			bnxt_free_filter(bp, match);
			filter->fw_l2_filter_id = -1;
			bnxt_free_filter(bp, filter);
		}
		break;
	case RTE_ETH_FILTER_FLUSH:
		for (i = bp->nr_vnics - 1; i >= 0; i--) {
			struct bnxt_vnic_info *vnic = &bp->vnic_info[i];

			STAILQ_FOREACH(filter, &vnic->filter, next) {
				if (filter->filter_type ==
				    HWRM_CFA_NTUPLE_FILTER) {
					ret =
					bnxt_hwrm_clear_ntuple_filter(bp,
								      filter);
					STAILQ_REMOVE(&vnic->filter, filter,
						      bnxt_filter_info, next);
				}
			}
		}
		return ret;
	case RTE_ETH_FILTER_UPDATE:
	case RTE_ETH_FILTER_STATS:
	case RTE_ETH_FILTER_INFO:
		PMD_DRV_LOG(ERR, "operation %u not implemented", filter_op);
		break;
	default:
		PMD_DRV_LOG(ERR, "unknown operation %u", filter_op);
		ret = -EINVAL;
		break;
	}
	return ret;

free_filter:
	filter->fw_l2_filter_id = -1;
	bnxt_free_filter(bp, filter);
	return ret;
}

static int
bnxt_filter_ctrl_op(struct rte_eth_dev *dev __rte_unused,
		    enum rte_filter_type filter_type,
		    enum rte_filter_op filter_op, void *arg)
{
	int ret = 0;

	switch (filter_type) {
	case RTE_ETH_FILTER_TUNNEL:
		PMD_DRV_LOG(ERR,
			"filter type: %d: To be implemented\n", filter_type);
		break;
	case RTE_ETH_FILTER_FDIR:
		ret = bnxt_fdir_filter(dev, filter_op, arg);
		break;
	case RTE_ETH_FILTER_NTUPLE:
		ret = bnxt_ntuple_filter(dev, filter_op, arg);
		break;
	case RTE_ETH_FILTER_ETHERTYPE:
		ret = bnxt_ethertype_filter(dev, filter_op, arg);
		break;
	case RTE_ETH_FILTER_GENERIC:
		if (filter_op != RTE_ETH_FILTER_GET)
			return -EINVAL;
		*(const void **)arg = &bnxt_flow_ops;
		break;
	default:
		PMD_DRV_LOG(ERR,
			"Filter type (%d) not supported", filter_type);
		ret = -EINVAL;
		break;
	}
	return ret;
}

static const uint32_t *
bnxt_dev_supported_ptypes_get_op(struct rte_eth_dev *dev)
{
	static const uint32_t ptypes[] = {
		RTE_PTYPE_L2_ETHER_VLAN,
		RTE_PTYPE_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_L4_ICMP,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_INNER_L4_ICMP,
		RTE_PTYPE_INNER_L4_TCP,
		RTE_PTYPE_INNER_L4_UDP,
		RTE_PTYPE_UNKNOWN
	};

	if (dev->rx_pkt_burst == bnxt_recv_pkts)
		return ptypes;
	return NULL;
}

static int bnxt_map_regs(struct bnxt *bp, uint32_t *reg_arr, int count,
			 int reg_win)
{
	uint32_t reg_base = *reg_arr & 0xfffff000;
	uint32_t win_off;
	int i;

	for (i = 0; i < count; i++) {
		if ((reg_arr[i] & 0xfffff000) != reg_base)
			return -ERANGE;
	}
	win_off = BNXT_GRCPF_REG_WINDOW_BASE_OUT + (reg_win - 1) * 4;
	rte_cpu_to_le_32(rte_write32(reg_base, (uint8_t *)bp->bar0 + win_off));
	return 0;
}

static int bnxt_map_ptp_regs(struct bnxt *bp)
{
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;
	uint32_t *reg_arr;
	int rc, i;

	reg_arr = ptp->rx_regs;
	rc = bnxt_map_regs(bp, reg_arr, BNXT_PTP_RX_REGS, 5);
	if (rc)
		return rc;

	reg_arr = ptp->tx_regs;
	rc = bnxt_map_regs(bp, reg_arr, BNXT_PTP_TX_REGS, 6);
	if (rc)
		return rc;

	for (i = 0; i < BNXT_PTP_RX_REGS; i++)
		ptp->rx_mapped_regs[i] = 0x5000 + (ptp->rx_regs[i] & 0xfff);

	for (i = 0; i < BNXT_PTP_TX_REGS; i++)
		ptp->tx_mapped_regs[i] = 0x6000 + (ptp->tx_regs[i] & 0xfff);

	return 0;
}

static void bnxt_unmap_ptp_regs(struct bnxt *bp)
{
	rte_cpu_to_le_32(rte_write32(0, (uint8_t *)bp->bar0 +
			 BNXT_GRCPF_REG_WINDOW_BASE_OUT + 16));
	rte_cpu_to_le_32(rte_write32(0, (uint8_t *)bp->bar0 +
			 BNXT_GRCPF_REG_WINDOW_BASE_OUT + 20));
}

static uint64_t bnxt_cc_read(struct bnxt *bp)
{
	uint64_t ns;

	ns = rte_le_to_cpu_32(rte_read32((uint8_t *)bp->bar0 +
			      BNXT_GRCPF_REG_SYNC_TIME));
	ns |= (uint64_t)(rte_le_to_cpu_32(rte_read32((uint8_t *)bp->bar0 +
					  BNXT_GRCPF_REG_SYNC_TIME + 4))) << 32;
	return ns;
}

static int bnxt_get_tx_ts(struct bnxt *bp, uint64_t *ts)
{
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;
	uint32_t fifo;

	fifo = rte_le_to_cpu_32(rte_read32((uint8_t *)bp->bar0 +
				ptp->tx_mapped_regs[BNXT_PTP_TX_FIFO]));
	if (fifo & BNXT_PTP_TX_FIFO_EMPTY)
		return -EAGAIN;

	fifo = rte_le_to_cpu_32(rte_read32((uint8_t *)bp->bar0 +
				ptp->tx_mapped_regs[BNXT_PTP_TX_FIFO]));
	*ts = rte_le_to_cpu_32(rte_read32((uint8_t *)bp->bar0 +
				ptp->tx_mapped_regs[BNXT_PTP_TX_TS_L]));
	*ts |= (uint64_t)rte_le_to_cpu_32(rte_read32((uint8_t *)bp->bar0 +
				ptp->tx_mapped_regs[BNXT_PTP_TX_TS_H])) << 32;

	return 0;
}

static int bnxt_get_rx_ts(struct bnxt *bp, uint64_t *ts)
{
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;
	struct bnxt_pf_info *pf = &bp->pf;
	uint16_t port_id;
	uint32_t fifo;

	if (!ptp)
		return -ENODEV;

	fifo = rte_le_to_cpu_32(rte_read32((uint8_t *)bp->bar0 +
				ptp->rx_mapped_regs[BNXT_PTP_RX_FIFO]));
	if (!(fifo & BNXT_PTP_RX_FIFO_PENDING))
		return -EAGAIN;

	port_id = pf->port_id;
	rte_cpu_to_le_32(rte_write32(1 << port_id, (uint8_t *)bp->bar0 +
	       ptp->rx_mapped_regs[BNXT_PTP_RX_FIFO_ADV]));

	fifo = rte_le_to_cpu_32(rte_read32((uint8_t *)bp->bar0 +
				   ptp->rx_mapped_regs[BNXT_PTP_RX_FIFO]));
	if (fifo & BNXT_PTP_RX_FIFO_PENDING) {
/*		bnxt_clr_rx_ts(bp);	  TBD  */
		return -EBUSY;
	}

	*ts = rte_le_to_cpu_32(rte_read32((uint8_t *)bp->bar0 +
				ptp->rx_mapped_regs[BNXT_PTP_RX_TS_L]));
	*ts |= (uint64_t)rte_le_to_cpu_32(rte_read32((uint8_t *)bp->bar0 +
				ptp->rx_mapped_regs[BNXT_PTP_RX_TS_H])) << 32;

	return 0;
}

static int
bnxt_timesync_write_time(struct rte_eth_dev *dev, const struct timespec *ts)
{
	uint64_t ns;
	struct bnxt *bp = (struct bnxt *)dev->data->dev_private;
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;

	if (!ptp)
		return 0;

	ns = rte_timespec_to_ns(ts);
	/* Set the timecounters to a new value. */
	ptp->tc.nsec = ns;

	return 0;
}

static int
bnxt_timesync_read_time(struct rte_eth_dev *dev, struct timespec *ts)
{
	uint64_t ns, systime_cycles;
	struct bnxt *bp = (struct bnxt *)dev->data->dev_private;
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;

	if (!ptp)
		return 0;

	systime_cycles = bnxt_cc_read(bp);
	ns = rte_timecounter_update(&ptp->tc, systime_cycles);
	*ts = rte_ns_to_timespec(ns);

	return 0;
}
static int
bnxt_timesync_enable(struct rte_eth_dev *dev)
{
	struct bnxt *bp = (struct bnxt *)dev->data->dev_private;
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;
	uint32_t shift = 0;

	if (!ptp)
		return 0;

	ptp->rx_filter = 1;
	ptp->tx_tstamp_en = 1;
	ptp->rxctl = BNXT_PTP_MSG_EVENTS;

	if (!bnxt_hwrm_ptp_cfg(bp))
		bnxt_map_ptp_regs(bp);

	memset(&ptp->tc, 0, sizeof(struct rte_timecounter));
	memset(&ptp->rx_tstamp_tc, 0, sizeof(struct rte_timecounter));
	memset(&ptp->tx_tstamp_tc, 0, sizeof(struct rte_timecounter));

	ptp->tc.cc_mask = BNXT_CYCLECOUNTER_MASK;
	ptp->tc.cc_shift = shift;
	ptp->tc.nsec_mask = (1ULL << shift) - 1;

	ptp->rx_tstamp_tc.cc_mask = BNXT_CYCLECOUNTER_MASK;
	ptp->rx_tstamp_tc.cc_shift = shift;
	ptp->rx_tstamp_tc.nsec_mask = (1ULL << shift) - 1;

	ptp->tx_tstamp_tc.cc_mask = BNXT_CYCLECOUNTER_MASK;
	ptp->tx_tstamp_tc.cc_shift = shift;
	ptp->tx_tstamp_tc.nsec_mask = (1ULL << shift) - 1;

	return 0;
}

static int
bnxt_timesync_disable(struct rte_eth_dev *dev)
{
	struct bnxt *bp = (struct bnxt *)dev->data->dev_private;
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;

	if (!ptp)
		return 0;

	ptp->rx_filter = 0;
	ptp->tx_tstamp_en = 0;
	ptp->rxctl = 0;

	bnxt_hwrm_ptp_cfg(bp);

	bnxt_unmap_ptp_regs(bp);

	return 0;
}

static int
bnxt_timesync_read_rx_timestamp(struct rte_eth_dev *dev,
				 struct timespec *timestamp,
				 uint32_t flags __rte_unused)
{
	struct bnxt *bp = (struct bnxt *)dev->data->dev_private;
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;
	uint64_t rx_tstamp_cycles = 0;
	uint64_t ns;

	if (!ptp)
		return 0;

	bnxt_get_rx_ts(bp, &rx_tstamp_cycles);
	ns = rte_timecounter_update(&ptp->rx_tstamp_tc, rx_tstamp_cycles);
	*timestamp = rte_ns_to_timespec(ns);
	return  0;
}

static int
bnxt_timesync_read_tx_timestamp(struct rte_eth_dev *dev,
				 struct timespec *timestamp)
{
	struct bnxt *bp = (struct bnxt *)dev->data->dev_private;
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;
	uint64_t tx_tstamp_cycles = 0;
	uint64_t ns;

	if (!ptp)
		return 0;

	bnxt_get_tx_ts(bp, &tx_tstamp_cycles);
	ns = rte_timecounter_update(&ptp->tx_tstamp_tc, tx_tstamp_cycles);
	*timestamp = rte_ns_to_timespec(ns);

	return 0;
}

static int
bnxt_timesync_adjust_time(struct rte_eth_dev *dev, int64_t delta)
{
	struct bnxt *bp = (struct bnxt *)dev->data->dev_private;
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;

	if (!ptp)
		return 0;

	ptp->tc.nsec += delta;

	return 0;
}

static int
bnxt_get_eeprom_length_op(struct rte_eth_dev *dev)
{
	struct bnxt *bp = (struct bnxt *)dev->data->dev_private;
	int rc;
	uint32_t dir_entries;
	uint32_t entry_length;

	PMD_DRV_LOG(INFO, "%04x:%02x:%02x:%02x\n",
		bp->pdev->addr.domain, bp->pdev->addr.bus,
		bp->pdev->addr.devid, bp->pdev->addr.function);

	rc = bnxt_hwrm_nvm_get_dir_info(bp, &dir_entries, &entry_length);
	if (rc != 0)
		return rc;

	return dir_entries * entry_length;
}

static int
bnxt_get_eeprom_op(struct rte_eth_dev *dev,
		struct rte_dev_eeprom_info *in_eeprom)
{
	struct bnxt *bp = (struct bnxt *)dev->data->dev_private;
	uint32_t index;
	uint32_t offset;

	PMD_DRV_LOG(INFO, "%04x:%02x:%02x:%02x in_eeprom->offset = %d "
		"len = %d\n", bp->pdev->addr.domain,
		bp->pdev->addr.bus, bp->pdev->addr.devid,
		bp->pdev->addr.function, in_eeprom->offset, in_eeprom->length);

	if (in_eeprom->offset == 0) /* special offset value to get directory */
		return bnxt_get_nvram_directory(bp, in_eeprom->length,
						in_eeprom->data);

	index = in_eeprom->offset >> 24;
	offset = in_eeprom->offset & 0xffffff;

	if (index != 0)
		return bnxt_hwrm_get_nvram_item(bp, index - 1, offset,
					   in_eeprom->length, in_eeprom->data);

	return 0;
}

static bool bnxt_dir_type_is_ape_bin_format(uint16_t dir_type)
{
	switch (dir_type) {
	case BNX_DIR_TYPE_CHIMP_PATCH:
	case BNX_DIR_TYPE_BOOTCODE:
	case BNX_DIR_TYPE_BOOTCODE_2:
	case BNX_DIR_TYPE_APE_FW:
	case BNX_DIR_TYPE_APE_PATCH:
	case BNX_DIR_TYPE_KONG_FW:
	case BNX_DIR_TYPE_KONG_PATCH:
	case BNX_DIR_TYPE_BONO_FW:
	case BNX_DIR_TYPE_BONO_PATCH:
		/* FALLTHROUGH */
		return true;
	}

	return false;
}

static bool bnxt_dir_type_is_other_exec_format(uint16_t dir_type)
{
	switch (dir_type) {
	case BNX_DIR_TYPE_AVS:
	case BNX_DIR_TYPE_EXP_ROM_MBA:
	case BNX_DIR_TYPE_PCIE:
	case BNX_DIR_TYPE_TSCF_UCODE:
	case BNX_DIR_TYPE_EXT_PHY:
	case BNX_DIR_TYPE_CCM:
	case BNX_DIR_TYPE_ISCSI_BOOT:
	case BNX_DIR_TYPE_ISCSI_BOOT_IPV6:
	case BNX_DIR_TYPE_ISCSI_BOOT_IPV4N6:
		/* FALLTHROUGH */
		return true;
	}

	return false;
}

static bool bnxt_dir_type_is_executable(uint16_t dir_type)
{
	return bnxt_dir_type_is_ape_bin_format(dir_type) ||
		bnxt_dir_type_is_other_exec_format(dir_type);
}

static int
bnxt_set_eeprom_op(struct rte_eth_dev *dev,
		struct rte_dev_eeprom_info *in_eeprom)
{
	struct bnxt *bp = (struct bnxt *)dev->data->dev_private;
	uint8_t index, dir_op;
	uint16_t type, ext, ordinal, attr;

	PMD_DRV_LOG(INFO, "%04x:%02x:%02x:%02x in_eeprom->offset = %d "
		"len = %d\n", bp->pdev->addr.domain,
		bp->pdev->addr.bus, bp->pdev->addr.devid,
		bp->pdev->addr.function, in_eeprom->offset, in_eeprom->length);

	if (!BNXT_PF(bp)) {
		PMD_DRV_LOG(ERR, "NVM write not supported from a VF\n");
		return -EINVAL;
	}

	type = in_eeprom->magic >> 16;

	if (type == 0xffff) { /* special value for directory operations */
		index = in_eeprom->magic & 0xff;
		dir_op = in_eeprom->magic >> 8;
		if (index == 0)
			return -EINVAL;
		switch (dir_op) {
		case 0x0e: /* erase */
			if (in_eeprom->offset != ~in_eeprom->magic)
				return -EINVAL;
			return bnxt_hwrm_erase_nvram_directory(bp, index - 1);
		default:
			return -EINVAL;
		}
	}

	/* Create or re-write an NVM item: */
	if (bnxt_dir_type_is_executable(type) == true)
		return -EOPNOTSUPP;
	ext = in_eeprom->magic & 0xffff;
	ordinal = in_eeprom->offset >> 16;
	attr = in_eeprom->offset & 0xffff;

	return bnxt_hwrm_flash_nvram(bp, type, ordinal, ext, attr,
				     in_eeprom->data, in_eeprom->length);
	return 0;
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
	.rx_queue_intr_enable = bnxt_rx_queue_intr_enable_op,
	.rx_queue_intr_disable = bnxt_rx_queue_intr_disable_op,
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
	.xstats_get_by_id = bnxt_dev_xstats_get_by_id_op,
	.xstats_get_names_by_id = bnxt_dev_xstats_get_names_by_id_op,
	.rx_queue_count = bnxt_rx_queue_count_op,
	.rx_descriptor_status = bnxt_rx_descriptor_status_op,
	.tx_descriptor_status = bnxt_tx_descriptor_status_op,
	.rx_queue_start = bnxt_rx_queue_start,
	.rx_queue_stop = bnxt_rx_queue_stop,
	.tx_queue_start = bnxt_tx_queue_start,
	.tx_queue_stop = bnxt_tx_queue_stop,
	.filter_ctrl = bnxt_filter_ctrl_op,
	.dev_supported_ptypes_get = bnxt_dev_supported_ptypes_get_op,
	.get_eeprom_length    = bnxt_get_eeprom_length_op,
	.get_eeprom           = bnxt_get_eeprom_op,
	.set_eeprom           = bnxt_set_eeprom_op,
	.timesync_enable      = bnxt_timesync_enable,
	.timesync_disable     = bnxt_timesync_disable,
	.timesync_read_time   = bnxt_timesync_read_time,
	.timesync_write_time   = bnxt_timesync_write_time,
	.timesync_adjust_time = bnxt_timesync_adjust_time,
	.timesync_read_rx_timestamp = bnxt_timesync_read_rx_timestamp,
	.timesync_read_tx_timestamp = bnxt_timesync_read_tx_timestamp,
};

static bool bnxt_vf_pciid(uint16_t id)
{
	if (id == BROADCOM_DEV_ID_57304_VF ||
	    id == BROADCOM_DEV_ID_57406_VF ||
	    id == BROADCOM_DEV_ID_5731X_VF ||
	    id == BROADCOM_DEV_ID_5741X_VF ||
	    id == BROADCOM_DEV_ID_57414_VF ||
	    id == BROADCOM_DEV_ID_STRATUS_NIC_VF1 ||
	    id == BROADCOM_DEV_ID_STRATUS_NIC_VF2)
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
		PMD_DRV_LOG(ERR,
			"Cannot find PCI device base address, aborting\n");
		rc = -ENODEV;
		goto init_err_disable;
	}

	bp->eth_dev = eth_dev;
	bp->pdev = pci_dev;

	bp->bar0 = (void *)pci_dev->mem_resource[0].addr;
	if (!bp->bar0) {
		PMD_DRV_LOG(ERR, "Cannot map device registers, aborting\n");
		rc = -ENOMEM;
		goto init_err_release;
	}

	if (!pci_dev->mem_resource[2].addr) {
		PMD_DRV_LOG(ERR,
			    "Cannot find PCI device BAR 2 address, aborting\n");
		rc = -ENODEV;
		goto init_err_release;
	} else {
		bp->doorbell_base = (void *)pci_dev->mem_resource[2].addr;
	}

	return 0;

init_err_release:
	if (bp->bar0)
		bp->bar0 = NULL;
	if (bp->doorbell_base)
		bp->doorbell_base = NULL;

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
	rte_iova_t mz_phys_addr;
	struct bnxt *bp;
	int rc;

	if (version_printed++ == 0)
		PMD_DRV_LOG(INFO, "%s\n", bnxt_version);

	rte_eth_copy_pci_info(eth_dev, pci_dev);

	bp = eth_dev->data->dev_private;

	bp->dev_stopped = 1;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		goto skip_init;

	if (bnxt_vf_pciid(pci_dev->id.device_id))
		bp->flags |= BNXT_FLAG_VF;

	rc = bnxt_init_board(eth_dev);
	if (rc) {
		PMD_DRV_LOG(ERR,
			"Board initialization failed rc: %x\n", rc);
		goto error;
	}
skip_init:
	eth_dev->dev_ops = &bnxt_dev_ops;
	eth_dev->rx_pkt_burst = &bnxt_recv_pkts;
	eth_dev->tx_pkt_burst = &bnxt_xmit_pkts;
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	if (pci_dev->id.device_id != BROADCOM_DEV_ID_NS2) {
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
					RTE_MEMZONE_SIZE_HINT_ONLY |
					RTE_MEMZONE_IOVA_CONTIG);
			if (mz == NULL)
				return -ENOMEM;
		}
		memset(mz->addr, 0, mz->len);
		mz_phys_addr = mz->iova;
		if ((unsigned long)mz->addr == mz_phys_addr) {
			PMD_DRV_LOG(WARNING,
				"Memzone physical address same as virtual.\n");
			PMD_DRV_LOG(WARNING,
				"Using rte_mem_virt2iova()\n");
			mz_phys_addr = rte_mem_virt2iova(mz->addr);
			if (mz_phys_addr == 0) {
				PMD_DRV_LOG(ERR,
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
			mz = rte_memzone_reserve(mz_name,
					total_alloc_len,
					SOCKET_ID_ANY,
					RTE_MEMZONE_2MB |
					RTE_MEMZONE_SIZE_HINT_ONLY |
					RTE_MEMZONE_IOVA_CONTIG);
			if (mz == NULL)
				return -ENOMEM;
		}
		memset(mz->addr, 0, mz->len);
		mz_phys_addr = mz->iova;
		if ((unsigned long)mz->addr == mz_phys_addr) {
			PMD_DRV_LOG(WARNING,
				"Memzone physical address same as virtual.\n");
			PMD_DRV_LOG(WARNING,
				"Using rte_mem_virt2iova()\n");
			mz_phys_addr = rte_mem_virt2iova(mz->addr);
			if (mz_phys_addr == 0) {
				PMD_DRV_LOG(ERR,
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
		PMD_DRV_LOG(ERR,
			"hwrm resource allocation failure rc: %x\n", rc);
		goto error_free;
	}
	rc = bnxt_hwrm_ver_get(bp);
	if (rc)
		goto error_free;
	rc = bnxt_hwrm_queue_qportcfg(bp);
	if (rc) {
		PMD_DRV_LOG(ERR, "hwrm queue qportcfg failed\n");
		goto error_free;
	}

	rc = bnxt_hwrm_func_qcfg(bp);
	if (rc) {
		PMD_DRV_LOG(ERR, "hwrm func qcfg failed\n");
		goto error_free;
	}

	/* Get the MAX capabilities for this function */
	rc = bnxt_hwrm_func_qcaps(bp);
	if (rc) {
		PMD_DRV_LOG(ERR, "hwrm query capability failure rc: %x\n", rc);
		goto error_free;
	}
	if (bp->max_tx_rings == 0) {
		PMD_DRV_LOG(ERR, "No TX rings available!\n");
		rc = -EBUSY;
		goto error_free;
	}
	eth_dev->data->mac_addrs = rte_zmalloc("bnxt_mac_addr_tbl",
					ETHER_ADDR_LEN * bp->max_l2_ctx, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		PMD_DRV_LOG(ERR,
			"Failed to alloc %u bytes needed to store MAC addr tbl",
			ETHER_ADDR_LEN * bp->max_l2_ctx);
		rc = -ENOMEM;
		goto error_free;
	}

	if (bnxt_check_zero_bytes(bp->dflt_mac_addr, ETHER_ADDR_LEN)) {
		PMD_DRV_LOG(ERR,
			    "Invalid MAC addr %02X:%02X:%02X:%02X:%02X:%02X\n",
			    bp->dflt_mac_addr[0], bp->dflt_mac_addr[1],
			    bp->dflt_mac_addr[2], bp->dflt_mac_addr[3],
			    bp->dflt_mac_addr[4], bp->dflt_mac_addr[5]);
		rc = -EINVAL;
		goto error_free;
	}
	/* Copy the permanent MAC from the qcap response address now. */
	memcpy(bp->mac_addr, bp->dflt_mac_addr, sizeof(bp->mac_addr));
	memcpy(&eth_dev->data->mac_addrs[0], bp->mac_addr, ETHER_ADDR_LEN);

	if (bp->max_ring_grps < bp->rx_cp_nr_rings) {
		/* 1 ring is for default completion ring */
		PMD_DRV_LOG(ERR, "Insufficient resource: Ring Group\n");
		rc = -ENOSPC;
		goto error_free;
	}

	bp->grp_info = rte_zmalloc("bnxt_grp_info",
				sizeof(*bp->grp_info) * bp->max_ring_grps, 0);
	if (!bp->grp_info) {
		PMD_DRV_LOG(ERR,
			"Failed to alloc %zu bytes to store group info table\n",
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
		PMD_DRV_LOG(WARNING,
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
	ALLOW_FUNC(HWRM_PORT_PHY_QCFG);
	ALLOW_FUNC(HWRM_VNIC_TPA_CFG);
	rc = bnxt_hwrm_func_driver_register(bp);
	if (rc) {
		PMD_DRV_LOG(ERR,
			"Failed to register driver");
		rc = -EBUSY;
		goto error_free;
	}

	PMD_DRV_LOG(INFO,
		DRV_MODULE_NAME " found at mem %" PRIx64 ", node addr %pM\n",
		pci_dev->mem_resource[0].phys_addr,
		pci_dev->mem_resource[0].addr);

	rc = bnxt_hwrm_func_reset(bp);
	if (rc) {
		PMD_DRV_LOG(ERR, "hwrm chip reset failure rc: %x\n", rc);
		rc = -EIO;
		goto error_free;
	}

	if (BNXT_PF(bp)) {
		//if (bp->pf.active_vfs) {
			// TODO: Deallocate VF resources?
		//}
		if (bp->pdev->max_vfs) {
			rc = bnxt_hwrm_allocate_vfs(bp, bp->pdev->max_vfs);
			if (rc) {
				PMD_DRV_LOG(ERR, "Failed to allocate VFs\n");
				goto error_free;
			}
		} else {
			rc = bnxt_hwrm_allocate_pf_only(bp);
			if (rc) {
				PMD_DRV_LOG(ERR,
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

	bnxt_enable_int(bp);
	bnxt_init_nic(bp);

	return 0;

error_free_int:
	bnxt_disable_int(bp);
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

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -EPERM;

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

RTE_INIT(bnxt_init_log);
static void
bnxt_init_log(void)
{
	bnxt_logtype_driver = rte_log_register("pmd.bnxt.driver");
	if (bnxt_logtype_driver >= 0)
		rte_log_set_level(bnxt_logtype_driver, RTE_LOG_INFO);
}

RTE_PMD_REGISTER_PCI(net_bnxt, bnxt_rte_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_bnxt, bnxt_pci_id_map);
RTE_PMD_REGISTER_KMOD_DEP(net_bnxt, "* igb_uio | uio_pci_generic | vfio-pci");
