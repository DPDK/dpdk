/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

#include <rte_bitmap.h>
#include <rte_memzone.h>
#include <rte_malloc.h>
#include <unistd.h>

#include "bnxt.h"
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

static void bnxt_init_ring_grps(struct bnxt *bp)
{
	unsigned int i;

	for (i = 0; i < bp->max_ring_grps; i++)
		memset(&bp->grp_info[i], (uint8_t)HWRM_NA_SIGNATURE,
		       sizeof(struct bnxt_ring_grp_info));
}

int bnxt_alloc_ring_grps(struct bnxt *bp)
{
	if (bp->max_tx_rings == 0) {
		PMD_DRV_LOG(ERR, "No TX rings available!\n");
		return -EBUSY;
	}

	/* THOR does not support ring groups.
	 * But we will use the array to save RSS context IDs.
	 */
	if (BNXT_CHIP_P5(bp)) {
		bp->max_ring_grps = BNXT_MAX_RSS_CTXTS_P5;
	} else if (bp->max_ring_grps < bp->rx_cp_nr_rings) {
		/* 1 ring is for default completion ring */
		PMD_DRV_LOG(ERR, "Insufficient resource: Ring Group\n");
		return -ENOSPC;
	}

	if (BNXT_HAS_RING_GRPS(bp)) {
		bp->grp_info = rte_zmalloc("bnxt_grp_info",
					   sizeof(*bp->grp_info) *
					   bp->max_ring_grps, 0);
		if (!bp->grp_info) {
			PMD_DRV_LOG(ERR,
				    "Failed to alloc grp info tbl.\n");
			return -ENOMEM;
		}
		bnxt_init_ring_grps(bp);
	}

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
int bnxt_alloc_rings(struct bnxt *bp, unsigned int socket_id, uint16_t qidx,
			    struct bnxt_tx_queue *txq,
			    struct bnxt_rx_queue *rxq,
			    struct bnxt_cp_ring_info *cp_ring_info,
			    struct bnxt_cp_ring_info *nq_ring_info,
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

	int stats_len = (tx_ring_info || rx_ring_info) ?
	    RTE_CACHE_LINE_ROUNDUP(sizeof(struct hwrm_stat_ctx_query_output) -
				   sizeof (struct hwrm_resp_hdr)) : 0;
	stats_len = RTE_ALIGN(stats_len, 128);

	int cp_vmem_start = stats_len;
	int cp_vmem_len = RTE_CACHE_LINE_ROUNDUP(cp_ring->vmem_size);
	cp_vmem_len = RTE_ALIGN(cp_vmem_len, 128);

	int nq_vmem_len = nq_ring_info ?
		RTE_CACHE_LINE_ROUNDUP(cp_ring->vmem_size) : 0;
	nq_vmem_len = RTE_ALIGN(nq_vmem_len, 128);

	int nq_vmem_start = cp_vmem_start + cp_vmem_len;

	int tx_vmem_start = nq_vmem_start + nq_vmem_len;
	int tx_vmem_len =
	    tx_ring_info ? RTE_CACHE_LINE_ROUNDUP(tx_ring_info->
						tx_ring_struct->vmem_size) : 0;
	tx_vmem_len = RTE_ALIGN(tx_vmem_len, 128);

	int rx_vmem_start = tx_vmem_start + tx_vmem_len;
	int rx_vmem_len = rx_ring_info ?
		RTE_CACHE_LINE_ROUNDUP(rx_ring_info->
						rx_ring_struct->vmem_size) : 0;
	rx_vmem_len = RTE_ALIGN(rx_vmem_len, 128);
	int ag_vmem_start = 0;
	int ag_vmem_len = 0;
	int cp_ring_start =  0;
	int nq_ring_start = 0;

	ag_vmem_start = rx_vmem_start + rx_vmem_len;
	ag_vmem_len = rx_ring_info ? RTE_CACHE_LINE_ROUNDUP(
				rx_ring_info->ag_ring_struct->vmem_size) : 0;
	cp_ring_start = ag_vmem_start + ag_vmem_len;
	cp_ring_start = RTE_ALIGN(cp_ring_start, 4096);

	int cp_ring_len = RTE_CACHE_LINE_ROUNDUP(cp_ring->ring_size *
						 sizeof(struct cmpl_base));
	cp_ring_len = RTE_ALIGN(cp_ring_len, 128);
	nq_ring_start = cp_ring_start + cp_ring_len;
	nq_ring_start = RTE_ALIGN(nq_ring_start, 4096);

	int nq_ring_len = nq_ring_info ? cp_ring_len : 0;

	int tx_ring_start = nq_ring_start + nq_ring_len;
	tx_ring_start = RTE_ALIGN(tx_ring_start, 4096);
	int tx_ring_len = tx_ring_info ?
	    RTE_CACHE_LINE_ROUNDUP(tx_ring_info->tx_ring_struct->ring_size *
				   sizeof(struct tx_bd_long)) : 0;
	tx_ring_len = RTE_ALIGN(tx_ring_len, 4096);

	int rx_ring_start = tx_ring_start + tx_ring_len;
	rx_ring_start = RTE_ALIGN(rx_ring_start, 4096);
	int rx_ring_len =  rx_ring_info ?
		RTE_CACHE_LINE_ROUNDUP(rx_ring_info->rx_ring_struct->ring_size *
		sizeof(struct rx_prod_pkt_bd)) : 0;
	rx_ring_len = RTE_ALIGN(rx_ring_len, 4096);

	int ag_ring_start = rx_ring_start + rx_ring_len;
	ag_ring_start = RTE_ALIGN(ag_ring_start, 4096);
	int ag_ring_len = rx_ring_len * AGG_RING_SIZE_FACTOR;
	ag_ring_len = RTE_ALIGN(ag_ring_len, 4096);

	int ag_bitmap_start = ag_ring_start + ag_ring_len;
	int ag_bitmap_len =  rx_ring_info ?
		RTE_CACHE_LINE_ROUNDUP(rte_bitmap_get_memory_footprint(
			rx_ring_info->rx_ring_struct->ring_size *
			AGG_RING_SIZE_FACTOR)) : 0;

	int tpa_info_start = ag_bitmap_start + ag_bitmap_len;
	int tpa_info_len = 0;

	if (rx_ring_info && (rx_offloads & DEV_RX_OFFLOAD_TCP_LRO)) {
		int tpa_max = BNXT_TPA_MAX_AGGS(bp);

		tpa_info_len = tpa_max * sizeof(struct bnxt_tpa_info);
		tpa_info_len = RTE_CACHE_LINE_ROUNDUP(tpa_info_len);
	}

	int total_alloc_len = tpa_info_start;
	total_alloc_len += tpa_info_len;

	snprintf(mz_name, RTE_MEMZONE_NAMESIZE,
		 "bnxt_" PCI_PRI_FMT "-%04x_%s", pdev->addr.domain,
		 pdev->addr.bus, pdev->addr.devid, pdev->addr.function, qidx,
		 suffix);
	mz_name[RTE_MEMZONE_NAMESIZE - 1] = 0;
	mz = rte_memzone_lookup(mz_name);
	if (!mz) {
		mz = rte_memzone_reserve_aligned(mz_name, total_alloc_len,
				socket_id,
				RTE_MEMZONE_2MB |
				RTE_MEMZONE_SIZE_HINT_ONLY |
				RTE_MEMZONE_IOVA_CONTIG,
				getpagesize());
		if (mz == NULL)
			return -ENOMEM;
	}
	memset(mz->addr, 0, mz->len);
	mz_phys_addr = mz->iova;

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
			    (struct rte_mbuf **)tx_ring->vmem;
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
			    (struct rte_mbuf **)rx_ring->vmem;
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
			    (struct rte_mbuf **)rx_ring->vmem;
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

	if (nq_ring_info) {
		struct bnxt_ring *nq_ring = nq_ring_info->cp_ring_struct;

		nq_ring->bd = (char *)mz->addr + nq_ring_start;
		nq_ring->bd_dma = mz_phys_addr + nq_ring_start;
		nq_ring_info->cp_desc_ring = nq_ring->bd;
		nq_ring_info->cp_desc_mapping = nq_ring->bd_dma;
		nq_ring->mem_zone = (const void *)mz;

		if (!nq_ring->bd)
			return -ENOMEM;
		if (nq_ring->vmem_size)
			*nq_ring->vmem = (char *)mz->addr + nq_vmem_start;

		nq_ring_info->hw_stats_ctx_id = HWRM_NA_SIGNATURE;
	}

	return 0;
}

static void bnxt_init_dflt_coal(struct bnxt_coal *coal)
{
	/* Tick values in micro seconds.
	 * 1 coal_buf x bufs_per_record = 1 completion record.
	 */
	coal->num_cmpl_aggr_int = BNXT_NUM_CMPL_AGGR_INT;
	/* This is a 6-bit value and must not be 0, or we'll get non stop IRQ */
	coal->num_cmpl_dma_aggr = BNXT_NUM_CMPL_DMA_AGGR;
	/* This is a 6-bit value and must not be 0, or we'll get non stop IRQ */
	coal->num_cmpl_dma_aggr_during_int = BNXT_NUM_CMPL_DMA_AGGR_DURING_INT;
	coal->int_lat_tmr_max = BNXT_INT_LAT_TMR_MAX;
	/* min timer set to 1/2 of interrupt timer */
	coal->int_lat_tmr_min = BNXT_INT_LAT_TMR_MIN;
	/* buf timer set to 1/4 of interrupt timer */
	coal->cmpl_aggr_dma_tmr = BNXT_CMPL_AGGR_DMA_TMR;
	coal->cmpl_aggr_dma_tmr_during_int = BNXT_CMPL_AGGR_DMA_TMR_DURING_INT;
}

static void bnxt_set_db(struct bnxt *bp,
			struct bnxt_db_info *db,
			uint32_t ring_type,
			uint32_t map_idx,
			uint32_t fid,
			uint32_t ring_mask)
{
	if (BNXT_CHIP_P5(bp)) {
		int db_offset = DB_PF_OFFSET;
		switch (ring_type) {
		case HWRM_RING_ALLOC_INPUT_RING_TYPE_TX:
			db->db_key64 = DBR_PATH_L2 | DBR_TYPE_SQ;
			break;
		case HWRM_RING_ALLOC_INPUT_RING_TYPE_RX:
		case HWRM_RING_ALLOC_INPUT_RING_TYPE_RX_AGG:
			db->db_key64 = DBR_PATH_L2 | DBR_TYPE_SRQ;
			break;
		case HWRM_RING_ALLOC_INPUT_RING_TYPE_L2_CMPL:
			db->db_key64 = DBR_PATH_L2 | DBR_TYPE_CQ;
			break;
		case HWRM_RING_ALLOC_INPUT_RING_TYPE_NQ:
			db->db_key64 = DBR_PATH_L2;
			break;
		}
		if (BNXT_CHIP_SR2(bp)) {
			db->db_key64 |= DBR_VALID;
			db_offset = bp->legacy_db_size;
		} else if (BNXT_VF(bp)) {
			db_offset = DB_VF_OFFSET;
		}

		db->doorbell = (char *)bp->doorbell_base + db_offset;
		db->db_key64 |= (uint64_t)fid << DBR_XID_SFT;
		db->db_64 = true;
	} else {
		db->doorbell = (char *)bp->doorbell_base + map_idx * 0x80;
		switch (ring_type) {
		case HWRM_RING_ALLOC_INPUT_RING_TYPE_TX:
			db->db_key32 = DB_KEY_TX;
			break;
		case HWRM_RING_ALLOC_INPUT_RING_TYPE_RX:
			db->db_key32 = DB_KEY_RX;
			break;
		case HWRM_RING_ALLOC_INPUT_RING_TYPE_L2_CMPL:
			db->db_key32 = DB_KEY_CP;
			break;
		}
		db->db_64 = false;
	}
	db->db_ring_mask = ring_mask;

	if (BNXT_CHIP_SR2(bp)) {
		db->db_epoch_mask = db->db_ring_mask + 1;
		db->db_epoch_shift = DBR_EPOCH_SFT -
					rte_log2_u32(db->db_epoch_mask);
	}
}

static int bnxt_alloc_cmpl_ring(struct bnxt *bp, int queue_index,
				struct bnxt_cp_ring_info *cpr)
{
	struct bnxt_ring *cp_ring = cpr->cp_ring_struct;
	uint32_t nq_ring_id = HWRM_NA_SIGNATURE;
	int cp_ring_index = queue_index + BNXT_RX_VEC_START;
	struct bnxt_cp_ring_info *nqr = bp->rxtx_nq_ring;
	uint8_t ring_type;
	int rc = 0;

	ring_type = HWRM_RING_ALLOC_INPUT_RING_TYPE_L2_CMPL;

	if (BNXT_HAS_NQ(bp)) {
		if (nqr) {
			nq_ring_id = nqr->cp_ring_struct->fw_ring_id;
		} else {
			PMD_DRV_LOG(ERR, "NQ ring is NULL\n");
			return -EINVAL;
		}
	}

	rc = bnxt_hwrm_ring_alloc(bp, cp_ring, ring_type, cp_ring_index,
				  HWRM_NA_SIGNATURE, nq_ring_id, 0);
	if (rc)
		return rc;

	cpr->cp_raw_cons = 0;
	bnxt_set_db(bp, &cpr->cp_db, ring_type, cp_ring_index,
		    cp_ring->fw_ring_id, cp_ring->ring_mask);
	bnxt_db_cq(cpr);

	return 0;
}

int bnxt_alloc_rxtx_nq_ring(struct bnxt *bp)
{
	struct bnxt_cp_ring_info *nqr;
	struct bnxt_ring *ring;
	int ring_index = BNXT_NUM_ASYNC_CPR(bp);
	uint8_t ring_type;
	int rc = 0;

	if (!BNXT_HAS_NQ(bp) || bp->rxtx_nq_ring)
		return 0;

	nqr = rte_zmalloc_socket("nqr",
				 sizeof(struct bnxt_cp_ring_info),
				 RTE_CACHE_LINE_SIZE,
				 bp->eth_dev->device->numa_node);
	if (nqr == NULL)
		return -ENOMEM;

	ring = rte_zmalloc_socket("bnxt_cp_ring_struct",
				  sizeof(struct bnxt_ring),
				  RTE_CACHE_LINE_SIZE,
				  bp->eth_dev->device->numa_node);
	if (ring == NULL) {
		rte_free(nqr);
		return -ENOMEM;
	}

	ring->bd = (void *)nqr->cp_desc_ring;
	ring->bd_dma = nqr->cp_desc_mapping;
	ring->ring_size = rte_align32pow2(DEFAULT_CP_RING_SIZE);
	ring->ring_mask = ring->ring_size - 1;
	ring->vmem_size = 0;
	ring->vmem = NULL;
	ring->fw_ring_id = INVALID_HW_RING_ID;

	nqr->cp_ring_struct = ring;
	rc = bnxt_alloc_rings(bp, bp->eth_dev->device->numa_node, 0, NULL,
			      NULL, nqr, NULL, "l2_nqr");
	if (rc) {
		rte_free(ring);
		rte_free(nqr);
		return -ENOMEM;
	}

	ring_type = HWRM_RING_ALLOC_INPUT_RING_TYPE_NQ;

	rc = bnxt_hwrm_ring_alloc(bp, ring, ring_type, ring_index,
				  HWRM_NA_SIGNATURE, HWRM_NA_SIGNATURE, 0);
	if (rc) {
		rte_free(ring);
		rte_free(nqr);
		return rc;
	}

	bnxt_set_db(bp, &nqr->cp_db, ring_type, ring_index,
		    ring->fw_ring_id, ring->ring_mask);
	bnxt_db_nq(nqr);

	bp->rxtx_nq_ring = nqr;

	return 0;
}

/* Free RX/TX NQ ring.  */
void bnxt_free_rxtx_nq_ring(struct bnxt *bp)
{
	struct bnxt_cp_ring_info *nqr = bp->rxtx_nq_ring;

	if (!nqr)
		return;

	bnxt_free_nq_ring(bp, nqr);

	bnxt_free_ring(nqr->cp_ring_struct);
	rte_free(nqr->cp_ring_struct);
	nqr->cp_ring_struct = NULL;
	rte_free(nqr);
	bp->rxtx_nq_ring = NULL;
}

static int bnxt_alloc_rx_ring(struct bnxt *bp, int queue_index)
{
	struct bnxt_rx_queue *rxq = bp->rx_queues[queue_index];
	struct bnxt_cp_ring_info *cpr = rxq->cp_ring;
	struct bnxt_ring *cp_ring = cpr->cp_ring_struct;
	struct bnxt_rx_ring_info *rxr = rxq->rx_ring;
	struct bnxt_ring *ring = rxr->rx_ring_struct;
	uint8_t ring_type;
	int rc = 0;

	ring_type = HWRM_RING_ALLOC_INPUT_RING_TYPE_RX;

	rc = bnxt_hwrm_ring_alloc(bp, ring, ring_type,
				  queue_index, cpr->hw_stats_ctx_id,
				  cp_ring->fw_ring_id, 0);
	if (rc)
		return rc;

	rxr->rx_raw_prod = 0;
	if (BNXT_HAS_RING_GRPS(bp))
		bp->grp_info[queue_index].rx_fw_ring_id = ring->fw_ring_id;
	bnxt_set_db(bp, &rxr->rx_db, ring_type, queue_index, ring->fw_ring_id,
		    ring->ring_mask);
	bnxt_db_write(&rxr->rx_db, rxr->rx_raw_prod);

	return 0;
}

static int bnxt_alloc_rx_agg_ring(struct bnxt *bp, int queue_index)
{
	unsigned int map_idx = queue_index + bp->rx_cp_nr_rings;
	struct bnxt_rx_queue *rxq = bp->rx_queues[queue_index];
	struct bnxt_cp_ring_info *cpr = rxq->cp_ring;
	struct bnxt_ring *cp_ring = cpr->cp_ring_struct;
	struct bnxt_rx_ring_info *rxr = rxq->rx_ring;
	struct bnxt_ring *ring = rxr->ag_ring_struct;
	uint32_t hw_stats_ctx_id = HWRM_NA_SIGNATURE;
	uint8_t ring_type;
	int rc = 0;

	ring->fw_rx_ring_id = rxr->rx_ring_struct->fw_ring_id;

	if (BNXT_CHIP_P5(bp)) {
		ring_type = HWRM_RING_ALLOC_INPUT_RING_TYPE_RX_AGG;
		hw_stats_ctx_id = cpr->hw_stats_ctx_id;
	} else {
		ring_type = HWRM_RING_ALLOC_INPUT_RING_TYPE_RX;
	}

	rc = bnxt_hwrm_ring_alloc(bp, ring, ring_type, map_idx,
				  hw_stats_ctx_id, cp_ring->fw_ring_id, 0);

	if (rc)
		return rc;

	rxr->ag_raw_prod = 0;
	if (BNXT_HAS_RING_GRPS(bp))
		bp->grp_info[queue_index].ag_fw_ring_id = ring->fw_ring_id;
	bnxt_set_db(bp, &rxr->ag_db, ring_type, map_idx, ring->fw_ring_id,
		    ring->ring_mask);
	bnxt_db_write(&rxr->ag_db, rxr->ag_raw_prod);

	return 0;
}

int bnxt_alloc_hwrm_rx_ring(struct bnxt *bp, int queue_index)
{
	struct bnxt_rx_queue *rxq = bp->rx_queues[queue_index];
	struct bnxt_cp_ring_info *cpr = rxq->cp_ring;
	struct bnxt_ring *cp_ring = cpr->cp_ring_struct;
	struct bnxt_rx_ring_info *rxr = rxq->rx_ring;
	struct bnxt_coal coal;
	int rc;

	/*
	 * Storage for the cp ring is allocated based on worst-case
	 * usage, the actual size to be used by hw is computed here.
	 */
	cp_ring->ring_size = rxr->rx_ring_struct->ring_size * 2;

	if (bp->eth_dev->data->scattered_rx)
		cp_ring->ring_size *= AGG_RING_SIZE_FACTOR;

	cp_ring->ring_mask = cp_ring->ring_size - 1;

	rc = bnxt_alloc_cmpl_ring(bp, queue_index, cpr);
	if (rc)
		goto err_out;

	rc = bnxt_hwrm_stat_ctx_alloc(bp, cpr);
	if (rc)
		goto err_out;

	if (BNXT_HAS_RING_GRPS(bp)) {
		bp->grp_info[queue_index].fw_stats_ctx = cpr->hw_stats_ctx_id;
		bp->grp_info[queue_index].cp_fw_ring_id = cp_ring->fw_ring_id;
	}

	bnxt_init_dflt_coal(&coal);
	bnxt_hwrm_set_ring_coal(bp, &coal, cp_ring->fw_ring_id);

	if (!BNXT_NUM_ASYNC_CPR(bp) && !queue_index) {
		/*
		 * If a dedicated async event completion ring is not enabled,
		 * use the first completion ring from PF or VF as the default
		 * completion ring for async event handling.
		 */
		bp->async_cp_ring = cpr;
		rc = bnxt_hwrm_set_async_event_cr(bp);
		if (rc)
			goto err_out;
	}

	rc = bnxt_alloc_rx_ring(bp, queue_index);
	if (rc)
		goto err_out;

	rc = bnxt_alloc_rx_agg_ring(bp, queue_index);
	if (rc)
		goto err_out;

	if (BNXT_HAS_RING_GRPS(bp)) {
		rc = bnxt_hwrm_ring_grp_alloc(bp, queue_index);
		if (rc)
			goto err_out;
	}

	if (rxq->rx_started) {
		if (bnxt_init_one_rx_ring(rxq)) {
			PMD_DRV_LOG(ERR, "bnxt_init_one_rx_ring failed!\n");
			bnxt_rx_queue_release_op(rxq);
			rc = -ENOMEM;
			goto err_out;
		}
		bnxt_db_write(&rxr->rx_db, rxr->rx_raw_prod);
		bnxt_db_write(&rxr->ag_db, rxr->ag_raw_prod);
	}
	rxq->index = queue_index;
#if defined(RTE_ARCH_X86) || defined(RTE_ARCH_ARM64)
	bnxt_rxq_vec_setup(rxq);
#endif

	return 0;

err_out:
	PMD_DRV_LOG(ERR,
		    "Failed to allocate receive queue %d, rc %d.\n",
		    queue_index, rc);
	return rc;
}

/* Initialise all rings to -1, its used to free rings later if allocation
 * of few rings fails.
 */
static void bnxt_init_all_rings(struct bnxt *bp)
{
	unsigned int i = 0;
	struct bnxt_rx_queue *rxq;
	struct bnxt_ring *cp_ring;
	struct bnxt_ring *ring;
	struct bnxt_rx_ring_info *rxr;
	struct bnxt_tx_queue *txq;

	for (i = 0; i < bp->rx_cp_nr_rings; i++) {
		rxq = bp->rx_queues[i];
		/* Rx-compl */
		cp_ring = rxq->cp_ring->cp_ring_struct;
		cp_ring->fw_ring_id = INVALID_HW_RING_ID;
		/* Rx-Reg */
		rxr = rxq->rx_ring;
		ring = rxr->rx_ring_struct;
		ring->fw_ring_id = INVALID_HW_RING_ID;
		/* Rx-AGG */
		ring = rxr->ag_ring_struct;
		ring->fw_ring_id = INVALID_HW_RING_ID;
	}
	for (i = 0; i < bp->tx_cp_nr_rings; i++) {
		txq = bp->tx_queues[i];
		/* Tx cmpl */
		cp_ring = txq->cp_ring->cp_ring_struct;
		cp_ring->fw_ring_id = INVALID_HW_RING_ID;
		/*Tx Ring */
		ring = txq->tx_ring->tx_ring_struct;
		ring->fw_ring_id = INVALID_HW_RING_ID;
	}
}

/* ring_grp usage:
 * [0] = default completion ring
 * [1 -> +rx_cp_nr_rings] = rx_cp, rx rings
 * [1+rx_cp_nr_rings + 1 -> +tx_cp_nr_rings] = tx_cp, tx rings
 */
int bnxt_alloc_hwrm_rings(struct bnxt *bp)
{
	struct bnxt_coal coal;
	unsigned int i;
	int rc = 0;

	bnxt_init_dflt_coal(&coal);
	bnxt_init_all_rings(bp);

	for (i = 0; i < bp->rx_cp_nr_rings; i++) {
		rc = bnxt_alloc_hwrm_rx_ring(bp, i);
		if (rc)
			goto err_out;
	}

	/* If something is wrong with Rx ring alloc, skip Tx ring alloc */
	for (i = 0; i < bp->tx_cp_nr_rings; i++) {
		rc = bnxt_alloc_hwrm_tx_ring(bp, i);
		if (rc)
			goto err_out;
	}

err_out:
	return rc;
}

/* Allocate dedicated async completion ring. */
int bnxt_alloc_async_cp_ring(struct bnxt *bp)
{
	struct bnxt_cp_ring_info *cpr = bp->async_cp_ring;
	struct bnxt_ring *cp_ring;
	uint8_t ring_type;
	int rc;

	if (BNXT_NUM_ASYNC_CPR(bp) == 0 || cpr == NULL)
		return 0;

	cp_ring = cpr->cp_ring_struct;

	if (BNXT_HAS_NQ(bp))
		ring_type = HWRM_RING_ALLOC_INPUT_RING_TYPE_NQ;
	else
		ring_type = HWRM_RING_ALLOC_INPUT_RING_TYPE_L2_CMPL;

	rc = bnxt_hwrm_ring_alloc(bp, cp_ring, ring_type, 0,
				  HWRM_NA_SIGNATURE, HWRM_NA_SIGNATURE, 0);

	if (rc)
		return rc;

	cpr->cp_raw_cons = 0;
	bnxt_set_db(bp, &cpr->cp_db, ring_type, 0,
		    cp_ring->fw_ring_id, cp_ring->ring_mask);

	if (BNXT_HAS_NQ(bp))
		bnxt_db_nq(cpr);
	else
		bnxt_db_cq(cpr);

	return bnxt_hwrm_set_async_event_cr(bp);
}

/* Free dedicated async completion ring. */
void bnxt_free_async_cp_ring(struct bnxt *bp)
{
	struct bnxt_cp_ring_info *cpr = bp->async_cp_ring;

	if (BNXT_NUM_ASYNC_CPR(bp) == 0 || cpr == NULL)
		return;

	if (BNXT_HAS_NQ(bp))
		bnxt_free_nq_ring(bp, cpr);
	else
		bnxt_free_cp_ring(bp, cpr);

	bnxt_free_ring(cpr->cp_ring_struct);
	rte_free(cpr->cp_ring_struct);
	cpr->cp_ring_struct = NULL;
	rte_free(cpr);
	bp->async_cp_ring = NULL;
}

int bnxt_alloc_async_ring_struct(struct bnxt *bp)
{
	struct bnxt_cp_ring_info *cpr = NULL;
	struct bnxt_ring *ring = NULL;

	if (BNXT_NUM_ASYNC_CPR(bp) == 0)
		return 0;

	cpr = rte_zmalloc_socket("cpr",
				 sizeof(struct bnxt_cp_ring_info),
				 RTE_CACHE_LINE_SIZE,
				 bp->eth_dev->device->numa_node);
	if (cpr == NULL)
		return -ENOMEM;

	ring = rte_zmalloc_socket("bnxt_cp_ring_struct",
				  sizeof(struct bnxt_ring),
				  RTE_CACHE_LINE_SIZE,
				  bp->eth_dev->device->numa_node);
	if (ring == NULL) {
		rte_free(cpr);
		return -ENOMEM;
	}

	ring->bd = (void *)cpr->cp_desc_ring;
	ring->bd_dma = cpr->cp_desc_mapping;
	ring->ring_size = rte_align32pow2(DEFAULT_CP_RING_SIZE);
	ring->ring_mask = ring->ring_size - 1;
	ring->vmem_size = 0;
	ring->vmem = NULL;

	bp->async_cp_ring = cpr;
	cpr->cp_ring_struct = ring;

	return bnxt_alloc_rings(bp, bp->eth_dev->device->numa_node, 0, NULL,
				NULL, bp->async_cp_ring, NULL, "def_cp");
}

int bnxt_alloc_hwrm_tx_ring(struct bnxt *bp, int queue_index)
{
	struct bnxt_tx_queue *txq = bp->tx_queues[queue_index];
	struct bnxt_cp_ring_info *cpr = txq->cp_ring;
	struct bnxt_ring *cp_ring = cpr->cp_ring_struct;
	struct bnxt_tx_ring_info *txr = txq->tx_ring;
	struct bnxt_ring *ring = txr->tx_ring_struct;
	unsigned int idx = queue_index + bp->rx_cp_nr_rings;
	uint16_t tx_cosq_id = 0;
	struct bnxt_coal coal;
	int rc = 0;

	rc = bnxt_alloc_cmpl_ring(bp, idx, cpr);
	if (rc)
		goto err_out;

	bnxt_init_dflt_coal(&coal);
	bnxt_hwrm_set_ring_coal(bp, &coal, cp_ring->fw_ring_id);

	rc = bnxt_hwrm_stat_ctx_alloc(bp, cpr);
	if (rc)
		goto err_out;

	if (bp->vnic_cap_flags & BNXT_VNIC_CAP_COS_CLASSIFY)
		tx_cosq_id = bp->tx_cosq_id[queue_index < bp->max_lltc ? queue_index : 0];
	else
		tx_cosq_id = bp->tx_cosq_id[0];

	rc = bnxt_hwrm_ring_alloc(bp, ring,
				  HWRM_RING_ALLOC_INPUT_RING_TYPE_TX,
				  queue_index, cpr->hw_stats_ctx_id,
				  cp_ring->fw_ring_id,
				  tx_cosq_id);
	if (rc)
		goto err_out;

	bnxt_set_db(bp, &txr->tx_db, HWRM_RING_ALLOC_INPUT_RING_TYPE_TX,
		    queue_index, ring->fw_ring_id,
		    ring->ring_mask);
	txq->index = idx;

	return rc;
err_out:
	bnxt_free_hwrm_tx_ring(bp, queue_index);
	return rc;
}
