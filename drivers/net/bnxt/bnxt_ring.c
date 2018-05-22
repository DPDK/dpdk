/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2018 Broadcom
 * All rights reserved.
 */

#include <rte_bitmap.h>
#include <rte_memzone.h>
#include <unistd.h>

#include "bnxt.h"
#include "bnxt_cpr.h"
#include "bnxt_hwrm.h"
#include "bnxt_ring.h"
#include "bnxt_rxq.h"
#include "bnxt_rxr.h"
#include "bnxt_txq.h"
#include "bnxt_txr.h"

#include "hsi_struct_def_dpdk.h"

/*
 * Generic ring handling
 */

void bnxt_free_ring(struct bnxt_ring *ring)
{
	if (!ring)
		return;

	if (ring->vmem_size && *ring->vmem) {
		memset((char *)*ring->vmem, 0, ring->vmem_size);
		*ring->vmem = NULL;
	}
	ring->mem_zone = NULL;
}

/*
 * Ring groups
 */

int bnxt_init_ring_grps(struct bnxt *bp)
{
	unsigned int i;

	for (i = 0; i < bp->max_ring_grps; i++)
		memset(&bp->grp_info[i], (uint8_t)HWRM_NA_SIGNATURE,
		       sizeof(struct bnxt_ring_grp_info));

	return 0;
}

/*
 * Allocates a completion ring with vmem and stats optionally also allocating
 * a TX and/or RX ring.  Passing NULL as tx_ring_info and/or rx_ring_info
 * to not allocate them.
 *
 * Order in the allocation is:
 * stats - Always non-zero length
 * cp vmem - Always zero-length, supported for the bnxt_ring abstraction
 * tx vmem - Only non-zero length if tx_ring_info is not NULL
 * rx vmem - Only non-zero length if rx_ring_info is not NULL
 * cp bd ring - Always non-zero length
 * tx bd ring - Only non-zero length if tx_ring_info is not NULL
 * rx bd ring - Only non-zero length if rx_ring_info is not NULL
 */
int bnxt_alloc_rings(struct bnxt *bp, uint16_t qidx,
			    struct bnxt_tx_queue *txq,
			    struct bnxt_rx_queue *rxq,
			    struct bnxt_cp_ring_info *cp_ring_info,
			    const char *suffix)
{
	struct bnxt_ring *cp_ring = cp_ring_info->cp_ring_struct;
	struct bnxt_rx_ring_info *rx_ring_info = rxq ? rxq->rx_ring : NULL;
	struct bnxt_tx_ring_info *tx_ring_info = txq ? txq->tx_ring : NULL;
	struct bnxt_ring *tx_ring;
	struct bnxt_ring *rx_ring;
	struct rte_pci_device *pdev = bp->pdev;
	uint64_t rx_offloads = bp->eth_dev->data->dev_conf.rxmode.offloads;
	const struct rte_memzone *mz = NULL;
	char mz_name[RTE_MEMZONE_NAMESIZE];
	rte_iova_t mz_phys_addr;
	int sz;

	int stats_len = (tx_ring_info || rx_ring_info) ?
	    RTE_CACHE_LINE_ROUNDUP(sizeof(struct hwrm_stat_ctx_query_output) -
				   sizeof (struct hwrm_resp_hdr)) : 0;

	int cp_vmem_start = stats_len;
	int cp_vmem_len = RTE_CACHE_LINE_ROUNDUP(cp_ring->vmem_size);

	int tx_vmem_start = cp_vmem_start + cp_vmem_len;
	int tx_vmem_len =
	    tx_ring_info ? RTE_CACHE_LINE_ROUNDUP(tx_ring_info->
						tx_ring_struct->vmem_size) : 0;

	int rx_vmem_start = tx_vmem_start + tx_vmem_len;
	int rx_vmem_len = rx_ring_info ?
		RTE_CACHE_LINE_ROUNDUP(rx_ring_info->
						rx_ring_struct->vmem_size) : 0;
	int ag_vmem_start = 0;
	int ag_vmem_len = 0;
	int cp_ring_start =  0;

	ag_vmem_start = rx_vmem_start + rx_vmem_len;
	ag_vmem_len = rx_ring_info ? RTE_CACHE_LINE_ROUNDUP(
				rx_ring_info->ag_ring_struct->vmem_size) : 0;
	cp_ring_start = ag_vmem_start + ag_vmem_len;

	int cp_ring_len = RTE_CACHE_LINE_ROUNDUP(cp_ring->ring_size *
						 sizeof(struct cmpl_base));

	int tx_ring_start = cp_ring_start + cp_ring_len;
	int tx_ring_len = tx_ring_info ?
	    RTE_CACHE_LINE_ROUNDUP(tx_ring_info->tx_ring_struct->ring_size *
				   sizeof(struct tx_bd_long)) : 0;

	int rx_ring_start = tx_ring_start + tx_ring_len;
	int rx_ring_len =  rx_ring_info ?
		RTE_CACHE_LINE_ROUNDUP(rx_ring_info->rx_ring_struct->ring_size *
		sizeof(struct rx_prod_pkt_bd)) : 0;

	int ag_ring_start = rx_ring_start + rx_ring_len;
	int ag_ring_len = rx_ring_len * AGG_RING_SIZE_FACTOR;

	int ag_bitmap_start = ag_ring_start + ag_ring_len;
	int ag_bitmap_len =  rx_ring_info ?
		RTE_CACHE_LINE_ROUNDUP(rte_bitmap_get_memory_footprint(
			rx_ring_info->rx_ring_struct->ring_size *
			AGG_RING_SIZE_FACTOR)) : 0;

	int tpa_info_start = ag_bitmap_start + ag_bitmap_len;
	int tpa_info_len = rx_ring_info ?
		RTE_CACHE_LINE_ROUNDUP(BNXT_TPA_MAX *
				       sizeof(struct bnxt_tpa_info)) : 0;

	int total_alloc_len = tpa_info_start;
	if (rx_offloads & DEV_RX_OFFLOAD_TCP_LRO)
		total_alloc_len += tpa_info_len;

	snprintf(mz_name, RTE_MEMZONE_NAMESIZE,
		 "bnxt_%04x:%02x:%02x:%02x-%04x_%s", pdev->addr.domain,
		 pdev->addr.bus, pdev->addr.devid, pdev->addr.function, qidx,
		 suffix);
	mz_name[RTE_MEMZONE_NAMESIZE - 1] = 0;
	mz = rte_memzone_lookup(mz_name);
	if (!mz) {
		mz = rte_memzone_reserve_aligned(mz_name, total_alloc_len,
				SOCKET_ID_ANY,
				RTE_MEMZONE_2MB |
				RTE_MEMZONE_SIZE_HINT_ONLY |
				RTE_MEMZONE_IOVA_CONTIG,
				getpagesize());
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
		for (sz = 0; sz < total_alloc_len; sz += getpagesize())
			rte_mem_lock_page(((char *)mz->addr) + sz);
		mz_phys_addr = rte_mem_virt2iova(mz->addr);
		if (mz_phys_addr == 0) {
			PMD_DRV_LOG(ERR,
			"unable to map ring address to physical memory\n");
			return -ENOMEM;
		}
	}

	if (tx_ring_info) {
		txq->mz = mz;
		tx_ring = tx_ring_info->tx_ring_struct;

		tx_ring->bd = ((char *)mz->addr + tx_ring_start);
		tx_ring_info->tx_desc_ring = (struct tx_bd_long *)tx_ring->bd;
		tx_ring->bd_dma = mz_phys_addr + tx_ring_start;
		tx_ring_info->tx_desc_mapping = tx_ring->bd_dma;
		tx_ring->mem_zone = (const void *)mz;

		if (!tx_ring->bd)
			return -ENOMEM;
		if (tx_ring->vmem_size) {
			tx_ring->vmem =
			    (void **)((char *)mz->addr + tx_vmem_start);
			tx_ring_info->tx_buf_ring =
			    (struct bnxt_sw_tx_bd *)tx_ring->vmem;
		}
	}

	if (rx_ring_info) {
		rxq->mz = mz;
		rx_ring = rx_ring_info->rx_ring_struct;

		rx_ring->bd = ((char *)mz->addr + rx_ring_start);
		rx_ring_info->rx_desc_ring =
		    (struct rx_prod_pkt_bd *)rx_ring->bd;
		rx_ring->bd_dma = mz_phys_addr + rx_ring_start;
		rx_ring_info->rx_desc_mapping = rx_ring->bd_dma;
		rx_ring->mem_zone = (const void *)mz;

		if (!rx_ring->bd)
			return -ENOMEM;
		if (rx_ring->vmem_size) {
			rx_ring->vmem =
			    (void **)((char *)mz->addr + rx_vmem_start);
			rx_ring_info->rx_buf_ring =
			    (struct bnxt_sw_rx_bd *)rx_ring->vmem;
		}

		rx_ring = rx_ring_info->ag_ring_struct;

		rx_ring->bd = ((char *)mz->addr + ag_ring_start);
		rx_ring_info->ag_desc_ring =
		    (struct rx_prod_pkt_bd *)rx_ring->bd;
		rx_ring->bd_dma = mz->iova + ag_ring_start;
		rx_ring_info->ag_desc_mapping = rx_ring->bd_dma;
		rx_ring->mem_zone = (const void *)mz;

		if (!rx_ring->bd)
			return -ENOMEM;
		if (rx_ring->vmem_size) {
			rx_ring->vmem =
			    (void **)((char *)mz->addr + ag_vmem_start);
			rx_ring_info->ag_buf_ring =
			    (struct bnxt_sw_rx_bd *)rx_ring->vmem;
		}

		rx_ring_info->ag_bitmap =
		    rte_bitmap_init(rx_ring_info->rx_ring_struct->ring_size *
				    AGG_RING_SIZE_FACTOR, (uint8_t *)mz->addr +
				    ag_bitmap_start, ag_bitmap_len);

		/* TPA info */
		if (rx_offloads & DEV_RX_OFFLOAD_TCP_LRO)
			rx_ring_info->tpa_info =
				((struct bnxt_tpa_info *)((char *)mz->addr +
							  tpa_info_start));
	}

	cp_ring->bd = ((char *)mz->addr + cp_ring_start);
	cp_ring->bd_dma = mz_phys_addr + cp_ring_start;
	cp_ring_info->cp_desc_ring = cp_ring->bd;
	cp_ring_info->cp_desc_mapping = cp_ring->bd_dma;
	cp_ring->mem_zone = (const void *)mz;

	if (!cp_ring->bd)
		return -ENOMEM;
	if (cp_ring->vmem_size)
		*cp_ring->vmem = ((char *)mz->addr + stats_len);
	if (stats_len) {
		cp_ring_info->hw_stats = mz->addr;
		cp_ring_info->hw_stats_map = mz_phys_addr;
	}
	cp_ring_info->hw_stats_ctx_id = HWRM_NA_SIGNATURE;
	return 0;
}

/* ring_grp usage:
 * [0] = default completion ring
 * [1 -> +rx_cp_nr_rings] = rx_cp, rx rings
 * [1+rx_cp_nr_rings + 1 -> +tx_cp_nr_rings] = tx_cp, tx rings
 */
int bnxt_alloc_hwrm_rings(struct bnxt *bp)
{
	unsigned int i;
	int rc = 0;

	for (i = 0; i < bp->rx_cp_nr_rings; i++) {
		struct bnxt_rx_queue *rxq = bp->rx_queues[i];
		struct bnxt_cp_ring_info *cpr = rxq->cp_ring;
		struct bnxt_ring *cp_ring = cpr->cp_ring_struct;
		struct bnxt_rx_ring_info *rxr = rxq->rx_ring;
		struct bnxt_ring *ring = rxr->rx_ring_struct;
		unsigned int map_idx = i + bp->rx_cp_nr_rings;

		bp->grp_info[i].fw_stats_ctx = cpr->hw_stats_ctx_id;

		/* Rx cmpl */
		rc = bnxt_hwrm_ring_alloc
			(bp,
			 cp_ring,
			 HWRM_RING_ALLOC_INPUT_RING_TYPE_L2_CMPL,
			 i,
			 HWRM_NA_SIGNATURE,
			 HWRM_NA_SIGNATURE);
		if (rc)
			goto err_out;
		cpr->cp_doorbell = (char *)bp->doorbell_base + i * 0x80;
		bp->grp_info[i].cp_fw_ring_id = cp_ring->fw_ring_id;
		B_CP_DIS_DB(cpr, cpr->cp_raw_cons);

		if (!i) {
			/*
			 * In order to save completion resource, use the first
			 * completion ring from PF or VF as the default
			 * completion ring for async event & HWRM
			 * forward response handling.
			 */
			bp->def_cp_ring = cpr;
			rc = bnxt_hwrm_set_async_event_cr(bp);
			if (rc)
				goto err_out;
		}

		/* Rx ring */
		rc = bnxt_hwrm_ring_alloc(bp,
					  ring,
					  HWRM_RING_ALLOC_INPUT_RING_TYPE_RX,
					  i,
					  cpr->hw_stats_ctx_id,
					  cp_ring->fw_ring_id);
		if (rc)
			goto err_out;
		rxr->rx_prod = 0;
		rxr->rx_doorbell = (char *)bp->doorbell_base + i * 0x80;
		bp->grp_info[i].rx_fw_ring_id = ring->fw_ring_id;
		B_RX_DB(rxr->rx_doorbell, rxr->rx_prod);

		ring = rxr->ag_ring_struct;
		/* Agg ring */
		if (ring == NULL) {
			PMD_DRV_LOG(ERR, "Alloc AGG Ring is NULL!\n");
			goto err_out;
		}

		rc = bnxt_hwrm_ring_alloc(bp, ring,
				HWRM_RING_ALLOC_INPUT_RING_TYPE_RX,
				map_idx, HWRM_NA_SIGNATURE,
				cp_ring->fw_ring_id);
		if (rc)
			goto err_out;
		PMD_DRV_LOG(DEBUG, "Alloc AGG Done!\n");
		rxr->ag_prod = 0;
		rxr->ag_doorbell = (char *)bp->doorbell_base + map_idx * 0x80;
		bp->grp_info[i].ag_fw_ring_id = ring->fw_ring_id;
		B_RX_DB(rxr->ag_doorbell, rxr->ag_prod);

		rxq->rx_buf_use_size = BNXT_MAX_MTU + ETHER_HDR_LEN +
					ETHER_CRC_LEN + (2 * VLAN_TAG_SIZE);
		if (bnxt_init_one_rx_ring(rxq)) {
			PMD_DRV_LOG(ERR, "bnxt_init_one_rx_ring failed!\n");
			bnxt_rx_queue_release_op(rxq);
			return -ENOMEM;
		}
		B_RX_DB(rxr->rx_doorbell, rxr->rx_prod);
		B_RX_DB(rxr->ag_doorbell, rxr->ag_prod);
		rxq->index = i;
	}

	for (i = 0; i < bp->tx_cp_nr_rings; i++) {
		struct bnxt_tx_queue *txq = bp->tx_queues[i];
		struct bnxt_cp_ring_info *cpr = txq->cp_ring;
		struct bnxt_ring *cp_ring = cpr->cp_ring_struct;
		struct bnxt_tx_ring_info *txr = txq->tx_ring;
		struct bnxt_ring *ring = txr->tx_ring_struct;
		unsigned int idx = i + bp->rx_cp_nr_rings;

		/* Tx cmpl */
		rc = bnxt_hwrm_ring_alloc(bp, cp_ring,
					HWRM_RING_ALLOC_INPUT_RING_TYPE_L2_CMPL,
					idx, HWRM_NA_SIGNATURE,
					HWRM_NA_SIGNATURE);
		if (rc)
			goto err_out;

		cpr->cp_doorbell = (char *)bp->doorbell_base + idx * 0x80;
		B_CP_DIS_DB(cpr, cpr->cp_raw_cons);

		/* Tx ring */
		rc = bnxt_hwrm_ring_alloc(bp, ring,
					HWRM_RING_ALLOC_INPUT_RING_TYPE_TX,
					idx, cpr->hw_stats_ctx_id,
					cp_ring->fw_ring_id);
		if (rc)
			goto err_out;

		txr->tx_doorbell = (char *)bp->doorbell_base + idx * 0x80;
		txq->index = idx;
	}

err_out:
	return rc;
}
