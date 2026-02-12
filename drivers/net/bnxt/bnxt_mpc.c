/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2020 Broadcom
 * All rights reserved.
 */

#include <inttypes.h>
#include <rte_malloc.h>
#include <unistd.h>

#include "bnxt.h"
#include "bnxt_ring.h"
#include "bnxt_mpc.h"
#include "bnxt_hwrm.h"
#include "hsi_struct_def_dpdk.h"

/*#define MPC_DEBUG 1*/

#define BNXT_MPC_BP_SIZE 16

static int bnxt_mpc_chnls_enable(struct bnxt *bp)
{
	struct bnxt_mpc *mpc = bp->mpc;
	uint8_t mpc_chnl_msk = 0;
	int i, rc;

	if (!mpc)
		return -EINVAL;

	for (i = 0; i < BNXT_MPC_CHNL_MAX; i++) {
		if (!(mpc->mpc_chnls_cap & (1 << i)))
			continue;
		mpc_chnl_msk |= (1 << i);
	}
	mpc->mpc_chnls_en = mpc_chnl_msk;

	if (!BNXT_PF(bp))
		return 0;

	rc = bnxt_hwrm_func_cfg_mpc(bp, mpc_chnl_msk, true);
	if (rc != 0) {
		mpc->mpc_chnls_en = 0;
		PMD_DRV_LOG_LINE(ERR, "MPC chnls enabling failed rc:%d", rc);
	}

	return rc;
}

static int bnxt_mpc_chnls_disable(struct bnxt *bp)
{
	struct bnxt_mpc *mpc = bp->mpc;
	uint8_t mpc_chnl_msk = 0;
	int i, rc;

	if (!mpc)
		return -EINVAL;
	mpc->mpc_chnls_en = 0;

	if (!BNXT_PF(bp))
		return 0;

	for (i = 0; i < BNXT_MPC_CHNL_MAX; i++) {
		if (!(mpc->mpc_chnls_en & (1 << i)))
			continue;
		mpc_chnl_msk |= (1 << i);
	}
	rc = bnxt_hwrm_func_cfg_mpc(bp, mpc_chnl_msk, false);
	if (rc != 0)
		PMD_DRV_LOG_LINE(ERR, "MPC chnls disabling failed rc:%d", rc);

	return rc;
}

static void bnxt_mpc_queue_release_mbufs(struct bnxt_mpc_txq *mpc_queue)
{
	struct bnxt_sw_mpc_bd *sw_ring;
	uint16_t i;

	if (!mpc_queue)
		return;

	sw_ring = mpc_queue->mpc_ring->mpc_buf_ring;
	if (!sw_ring)
		return;

	for (i = 0; i < mpc_queue->mpc_ring->mpc_ring_struct->ring_size; i++) {
		if (sw_ring[i].mpc_mbuf) {
			rte_free(sw_ring[i].mpc_mbuf);
			sw_ring[i].mpc_mbuf = NULL;
		}
	}
}

static void bnxt_mpc_queue_release_one(struct bnxt_mpc_txq *mpc_queue)
{
	if (!mpc_queue)
		return;

	if (is_bnxt_in_error(mpc_queue->bp))
		return;
	/* Free MPC ring HW descriptors */
	bnxt_mpc_queue_release_mbufs(mpc_queue);
	bnxt_free_ring(mpc_queue->mpc_ring->mpc_ring_struct);
	/* Free MPC completion ring HW descriptors */
	bnxt_free_ring(mpc_queue->cp_ring->cp_ring_struct);

	rte_memzone_free(mpc_queue->mz);
	mpc_queue->mz = NULL;

	rte_free(mpc_queue->free);
	rte_free(mpc_queue);
}

static void bnxt_mpc_ring_free_one(struct bnxt_mpc_txq *mpc_queue)
{
	struct bnxt_cp_ring_info *cpr;
	struct bnxt_mpc_ring_info *mpr;
	struct bnxt_ring *ring;

	if (!mpc_queue)
		return;

	if (is_bnxt_in_error(mpc_queue->bp))
		return;

	mpr = mpc_queue->mpc_ring;
	ring = mpr->mpc_ring_struct;
	if (ring->fw_ring_id == INVALID_HW_RING_ID)
		return;

	cpr = mpc_queue->cp_ring;
	bnxt_hwrm_ring_free(mpc_queue->bp, ring,
			    HWRM_RING_FREE_INPUT_RING_TYPE_TX,
			    cpr->cp_ring_struct->fw_ring_id);
	ring->fw_ring_id = INVALID_HW_RING_ID;
	memset(mpr->mpc_desc_ring, 0,
	       mpr->mpc_ring_struct->ring_size * sizeof(*mpr->mpc_desc_ring));
	memset(mpr->mpc_buf_ring, 0,
	       mpr->mpc_ring_struct->ring_size * sizeof(*mpr->mpc_buf_ring));
	mpr->raw_prod = 0;
	mpr->raw_cons = 0;

	bnxt_free_cp_ring(mpc_queue->bp, cpr);
	bnxt_hwrm_stat_ctx_free(mpc_queue->bp, cpr);
}

int bnxt_mpc_close(struct bnxt *bp)
{
	int i, rc = 0;
	struct bnxt_mpc_txq *mpc_queue;
	struct bnxt_mpc *mpc;

	rc = is_bnxt_in_error(bp);
	if (rc)
		return rc;

	if (!bp->mpc)
		return 0;

	mpc = bp->mpc;
	/* free the MPC TX ring for each channel. */
	for (i = 0; i < BNXT_MPC_CHNL_MAX; i++) {
		if (!(mpc->mpc_chnls_en & (1 << i)))
			continue;
		mpc_queue = mpc->mpc_txq[i];
		if (!mpc_queue)
			continue;
		bnxt_mpc_ring_free_one(mpc_queue);
		bnxt_mpc_queue_release_one(mpc_queue);
		mpc->mpc_txq[i] = NULL;
	}

	rc = bnxt_mpc_chnls_disable(bp);
	if (rc)
		PMD_DRV_LOG_LINE(ERR, "MPC channels disable failed rc:%d", rc);

	return rc;
}

static int bnxt_init_mpc_ring_struct(struct bnxt_mpc_txq *mpc_queue,
				     unsigned int socket_id)
{
	struct bnxt_cp_ring_info *cpr;
	struct bnxt_mpc_ring_info *mpr;
	struct bnxt_ring *ring;
	int rc = 0;

	mpr = rte_zmalloc_socket("bnxt_mpc_ring",
				 sizeof(struct bnxt_mpc_ring_info),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (mpr == NULL)
		return -ENOMEM;
	mpc_queue->mpc_ring = mpr;

	ring = rte_zmalloc_socket("bnxt_mpc_ring_struct",
				  sizeof(struct bnxt_ring),
				  RTE_CACHE_LINE_SIZE, socket_id);
	if (ring == NULL) {
		PMD_DRV_LOG_LINE(ERR, "MPC ring struct alloc failed rc:%d", rc);
		rc = -ENOMEM;
		goto bnxt_init_mpc_ring_struct_err;
	}

	mpr->mpc_ring_struct = ring;
	ring->ring_size = rte_align32pow2(mpc_queue->nb_mpc_desc);
	ring->ring_mask = ring->ring_size - 1;
	ring->bd = (void *)mpr->mpc_desc_ring;
	ring->bd_dma = mpr->mpc_desc_mapping;
	ring->vmem_size = ring->ring_size * sizeof(struct bnxt_sw_mpc_bd);
	ring->vmem = (void **)&mpr->mpc_buf_ring;
	ring->fw_ring_id = INVALID_HW_RING_ID;

	cpr = rte_zmalloc_socket("bnxt_mpc_ring",
				 sizeof(struct bnxt_cp_ring_info),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (cpr == NULL) {
		PMD_DRV_LOG_LINE(ERR, "MPC cp ring alloc failed rc:%d", rc);
		rc = -ENOMEM;
		goto bnxt_init_mpc_ring_struct_err1;
	}
	mpc_queue->cp_ring = cpr;

	ring = rte_zmalloc_socket("bnxt_mpc_ring_struct",
				  sizeof(struct bnxt_ring),
				  RTE_CACHE_LINE_SIZE, socket_id);
	if (ring == NULL) {
		PMD_DRV_LOG_LINE(ERR, "MPC cp ring struct alloc failed rc:%d", rc);
		rc = -ENOMEM;
		goto bnxt_init_mpc_ring_struct_err2;
	}
	cpr->cp_ring_struct = ring;
	ring->ring_size = mpr->mpc_ring_struct->ring_size;
	ring->ring_mask = ring->ring_size - 1;
	ring->bd = (void *)cpr->cp_desc_ring;
	ring->bd_dma = cpr->cp_desc_mapping;
	ring->vmem_size = 0;
	ring->vmem = NULL;
	ring->fw_ring_id = INVALID_HW_RING_ID;

	return 0;

bnxt_init_mpc_ring_struct_err2:
	rte_free(cpr);
bnxt_init_mpc_ring_struct_err1:
	rte_free(ring);
bnxt_init_mpc_ring_struct_err:
	rte_free(mpr);
	mpc_queue->mpc_ring = NULL;
	return rc;
}

/*
 * For a MPC queue, allocates a completion ring with vmem and bd ring,
 * stats mem, a TX ring with vmem and bd ring.
 *
 * Order in the allocation is:
 * stats - Always non-zero length
 * cp vmem - Always zero-length, supported for the bnxt_ring abstraction
 * tx vmem - Only non-zero length
 * cp bd ring - Always non-zero length
 * tx bd ring - Only non-zero length
 */

static int bnxt_alloc_mpc_rings(struct bnxt_mpc_txq *mpc_queue,
				const char *suffix)
{
	struct bnxt_ring *cp_ring;
	struct bnxt_cp_ring_info *cp_ring_info;
	struct bnxt_mpc_ring_info *mpc_ring_info;
	struct bnxt_ring *ring;
	struct rte_pci_device *pdev;
	const struct rte_memzone *mz = NULL;
	char mz_name[RTE_MEMZONE_NAMESIZE];
	rte_iova_t mz_phys_addr;

	if (!mpc_queue)
		return -EINVAL;

	pdev = mpc_queue->bp->pdev;
	mpc_ring_info = mpc_queue->mpc_ring;
	cp_ring = mpc_queue->cp_ring->cp_ring_struct;
	cp_ring_info = mpc_queue->cp_ring;

	int stats_len = BNXT_HWRM_CTX_GET_SIZE(mpc_queue->bp);
	stats_len = RTE_CACHE_LINE_ROUNDUP(stats_len);
	stats_len = RTE_ALIGN(stats_len, 128);

	int cp_vmem_start = stats_len;
	int cp_vmem_len = RTE_CACHE_LINE_ROUNDUP(cp_ring->vmem_size);
	cp_vmem_len = RTE_ALIGN(cp_vmem_len, 128);

	int nq_vmem_len = RTE_CACHE_LINE_ROUNDUP(cp_ring->vmem_size);
	nq_vmem_len = RTE_ALIGN(nq_vmem_len, 128);

	int nq_vmem_start = cp_vmem_start + cp_vmem_len;

	int mpc_vmem_start = nq_vmem_start + nq_vmem_len;
	int mpc_vmem_len =
	RTE_CACHE_LINE_ROUNDUP(mpc_ring_info->mpc_ring_struct->vmem_size);
	mpc_vmem_len = RTE_ALIGN(mpc_vmem_len, 128);

	int cp_ring_start = mpc_vmem_start + mpc_vmem_len;
	cp_ring_start = RTE_ALIGN(cp_ring_start, 4096);

	int cp_ring_len = RTE_CACHE_LINE_ROUNDUP(cp_ring->ring_size *
						 sizeof(struct cmpl_base));
	cp_ring_len = RTE_ALIGN(cp_ring_len, 128);

	int mpc_ring_start = cp_ring_start + cp_ring_len;
	mpc_ring_start = RTE_ALIGN(mpc_ring_start, 4096);
	int mpc_ring_len =
	RTE_CACHE_LINE_ROUNDUP(mpc_ring_info->mpc_ring_struct->ring_size *
			       sizeof(struct tx_bd_mp_cmd));
	mpc_ring_len = RTE_ALIGN(mpc_ring_len, 4096);

	int total_alloc_len = mpc_ring_start + mpc_ring_len;
	snprintf(mz_name, RTE_MEMZONE_NAMESIZE,
		 "bnxt_" PCI_PRI_FMT "-%04x_%s", pdev->addr.domain,
		 pdev->addr.bus, pdev->addr.devid, pdev->addr.function,
		 mpc_queue->chnl_id, suffix);
	mz_name[RTE_MEMZONE_NAMESIZE - 1] = 0;
	mz = rte_memzone_lookup(mz_name);
	if (!mz) {
		mz = rte_memzone_reserve_aligned(mz_name, total_alloc_len,
				SOCKET_ID_ANY,
				RTE_MEMZONE_2MB |
				RTE_MEMZONE_SIZE_HINT_ONLY |
				RTE_MEMZONE_IOVA_CONTIG,
				getpagesize());
		if (mz == NULL || !mz->addr)
			return -ENOMEM;
	}
	memset(mz->addr, 0, mz->len);
	mz_phys_addr = mz->iova;

	mpc_queue->mz = mz;
	ring = mpc_ring_info->mpc_ring_struct;

	ring->bd = ((char *)mz->addr + mpc_ring_start);
	mpc_ring_info->mpc_desc_ring = (struct tx_bd_mp_cmd *)ring->bd;
	ring->bd_dma = mz_phys_addr + mpc_ring_start;
	mpc_ring_info->mpc_desc_mapping = ring->bd_dma;
	ring->mem_zone = (const void *)mz;

	if (ring->vmem_size) {
		ring->vmem = (void **)((char *)mz->addr + mpc_vmem_start);
		mpc_ring_info->mpc_buf_ring =
			(struct bnxt_sw_mpc_bd *)ring->vmem;
	}

	cp_ring->bd = ((char *)mz->addr + cp_ring_start);
	cp_ring->bd_dma = mz_phys_addr + cp_ring_start;
	cp_ring_info->cp_desc_ring = cp_ring->bd;
	cp_ring_info->cp_desc_mapping = cp_ring->bd_dma;
	cp_ring->mem_zone = (const void *)mz;

	if (cp_ring->vmem_size)
		*cp_ring->vmem = (char *)mz->addr + stats_len;

	cp_ring_info->hw_stats = mz->addr;
	cp_ring_info->hw_stats_map = mz_phys_addr;
	cp_ring_info->hw_stats_ctx_id = HWRM_NA_SIGNATURE;

	return 0;
}

static void bnxt_init_one_mpc_ring(struct bnxt_mpc_txq *mpc_queue)
{
	struct bnxt_mpc_ring_info *mpr = mpc_queue->mpc_ring;
	struct bnxt_cp_ring_info *cpr = mpc_queue->cp_ring;
	struct bnxt_ring *ring = mpr->mpc_ring_struct;

	mpc_queue->wake_thresh = ring->ring_size / 2;
	ring->fw_ring_id = INVALID_HW_RING_ID;
	mpr->epoch = 0;
	cpr->epoch = 0;
}

static uint16_t get_mpc_ring_logical_id(uint8_t mpc_cap,
					enum bnxt_mpc_chnl chnl_id,
					uint16_t offset)
{
	unsigned int i;
	uint8_t logical_id = 0;

	for (i = 0; i < BNXT_MPC_CHNL_MAX; i++) {
		if (!(mpc_cap & (1 << i)))
			continue;

		if (i == chnl_id)
			return logical_id + offset;

		logical_id++;
	}

	return INVALID_HW_RING_ID;
}

static int bnxt_mpc_queue_setup_one(struct bnxt *bp, enum bnxt_mpc_chnl chnl_id,
				    uint16_t nb_desc, unsigned int socket_id)
{
	int rc = 0;
	struct bnxt_mpc *mpc;
	struct bnxt_mpc_txq *mpc_queue;

	if (!bp || !bp->mpc)
		return 0;

	mpc = bp->mpc;
	mpc_queue = rte_zmalloc_socket("bnxt_mpc_queue",
				       sizeof(struct bnxt_mpc_txq),
				       RTE_CACHE_LINE_SIZE, socket_id);
	if (!mpc_queue) {
		PMD_DRV_LOG_LINE(ERR, "bnxt_mpc_queue allocation failed!");
		return -ENOMEM;
	}

	mpc_queue->free =
		rte_zmalloc_socket(NULL,
				   sizeof(struct bnxt_mpc_mbuf *) * nb_desc,
				   RTE_CACHE_LINE_SIZE, socket_id);
	if (!mpc_queue->free) {
		PMD_DRV_LOG_LINE(ERR, "allocation of mpc mbuf free array failed!");
		rc = -ENOMEM;
		goto bnxt_mpc_queue_setup_one_err;
	}
	mpc_queue->bp = bp;
	mpc_queue->nb_mpc_desc = nb_desc;
	/* TBD: hardcoded to 1 for now and should be tuned later for perf */
	mpc_queue->free_thresh = BNXT_MPC_DESC_THRESH;

	rc = bnxt_init_mpc_ring_struct(mpc_queue, socket_id);
	if (rc)
		goto bnxt_mpc_queue_setup_one_err1;

	mpc_queue->chnl_id = chnl_id;

	/* allocate MPC TX ring hardware descriptors */
	rc = bnxt_alloc_mpc_rings(mpc_queue, "mpc");
	if (rc) {
		PMD_DRV_LOG_LINE(ERR, "ring_dma_zone_reserve for mpc_ring failed!");
		rc = -ENOMEM;
		goto bnxt_mpc_queue_setup_one_err1;
	}
	bnxt_init_one_mpc_ring(mpc_queue);
	mpc_queue->queue_idx = get_mpc_ring_logical_id(bp->mpc->mpc_chnls_cap,
						       chnl_id,
						       bp->tx_cp_nr_rings);
	mpc_queue->started = true;
	mpc->mpc_txq[chnl_id] = mpc_queue;

	return 0;

bnxt_mpc_queue_setup_one_err1:
	rte_free(mpc_queue->free);
bnxt_mpc_queue_setup_one_err:
	rte_free(mpc_queue);
	return rc;
}

static int bnxt_mpc_ring_alloc_one(struct bnxt *bp, enum bnxt_mpc_chnl chnl_id)
{
	int rc = 0;
	struct bnxt_mpc_txq *mpc_queue;
	struct bnxt_cp_ring_info *cpr;
	struct bnxt_ring *cp_ring;
	struct bnxt_mpc_ring_info *mpr;
	struct bnxt_ring *ring;
	struct bnxt_coal coal;
	uint32_t map_index;

	if (!bp || !bp->mpc)
		return 0;

	mpc_queue = bp->mpc->mpc_txq[chnl_id];
	if (!mpc_queue)
		return -EINVAL;

	bnxt_init_dflt_coal(&coal);
	cpr = mpc_queue->cp_ring;
	cp_ring = cpr->cp_ring_struct;
	map_index = mpc_queue->queue_idx;

	rc = bnxt_hwrm_stat_ctx_alloc(bp, cpr);
	if (rc) {
		PMD_DRV_LOG_LINE(ERR, "mpc ring %d stats alloc failed rc:%d!",
			    chnl_id, rc);
		return rc;
	}
	rc = bnxt_alloc_cmpl_ring(bp, map_index, cpr);
	if (rc) {
		PMD_DRV_LOG_LINE(ERR, "mpc ring %d cmpl ring alloc failed rc:%d!",
			    chnl_id, rc);
		goto bnxt_mpc_ring_alloc_one_err;
	}
	mpr = mpc_queue->mpc_ring;
	ring = mpr->mpc_ring_struct;
	map_index = BNXT_MPC_MAP_INDEX(chnl_id, mpc_queue->queue_idx);

	rc = bnxt_hwrm_ring_alloc(bp,
				  ring,
				  HWRM_RING_ALLOC_INPUT_RING_TYPE_TX,
				  map_index,
				  cpr->hw_stats_ctx_id,
				  cp_ring->fw_ring_id,
				  MPC_HW_COS_ID);
	if (rc) {
		PMD_DRV_LOG_LINE(ERR, "mpc ring %d tx ring alloc failed rc:%d!",
			    chnl_id, rc);
		goto bnxt_mpc_ring_alloc_one_err1;
	}

	bnxt_set_db(bp, &mpr->db, HWRM_RING_ALLOC_INPUT_RING_TYPE_TX, chnl_id,
		    ring->fw_ring_id, ring->ring_mask);

	bnxt_hwrm_set_ring_coal(bp, &coal, cp_ring->fw_ring_id);

	return rc;

bnxt_mpc_ring_alloc_one_err1:
	bnxt_free_cp_ring(bp, cpr);
bnxt_mpc_ring_alloc_one_err:
	bnxt_hwrm_stat_ctx_free(bp, cpr);
	return rc;
}

int bnxt_mpc_open(struct bnxt *bp)
{
	int rc = 0;
	enum bnxt_mpc_chnl i;
	struct bnxt_mpc *mpc;
	unsigned int socket_id;

	rc = is_bnxt_in_error(bp);
	if (rc)
		return rc;

	if (!bp->mpc)
		return 0;

	/* enable the MPC channels first */
	rc = bnxt_mpc_chnls_enable(bp);
	if (rc) {
		PMD_DRV_LOG_LINE(ERR, "MPC channels enable failed rc:%d", rc);
		return rc;
	}
	socket_id = rte_lcore_to_socket_id(rte_get_main_lcore());
	mpc = bp->mpc;

	/* Limit to MPC TE_CFA and RE_CFA */
	mpc->mpc_chnls_cap &= (1 << HWRM_RING_ALLOC_INPUT_MPC_CHNLS_TYPE_TE_CFA) |
		(1 << HWRM_RING_ALLOC_INPUT_MPC_CHNLS_TYPE_RE_CFA);

	/* allocate one MPC TX ring for each channel. */
	for (i = 0; i < BNXT_MPC_CHNL_MAX; i++) {
		if (!(mpc->mpc_chnls_cap & (1 << i)))
			continue;
		rc = bnxt_mpc_queue_setup_one(bp, i, BNXT_MPC_NB_DESC, socket_id);
		if (rc) {
			PMD_DRV_LOG_LINE(ERR, "MPC queue %d setup failed rc:%d",
				    i, rc);
			goto bnxt_mpc_open_err;
		}
		rc = bnxt_mpc_ring_alloc_one(bp, i);
		if (rc) {
			PMD_DRV_LOG_LINE(ERR, "MPC ring %d alloc failed rc:%d",
				    i, rc);
			goto bnxt_mpc_open_err;
		}
	}

	return rc;

bnxt_mpc_open_err:
	bnxt_mpc_close(bp);
	return rc;
}

int bnxt_mpc_cmd_cmpl(struct bnxt_mpc_txq *mpc_queue,
		      struct bnxt_mpc_mbuf *out_msg,
		      uint16_t *opaque)
{
	struct bnxt_cp_ring_info *cpr = mpc_queue->cp_ring;
	uint32_t raw_cons = cpr->cp_raw_cons;
	uint32_t cons;
	struct cmpl_base *mpc_cmpl;
	uint32_t nb_mpc_cmds = 0;
	struct cmpl_base *cp_desc_ring = cpr->cp_desc_ring;
	struct bnxt_ring *cp_ring_struct = cpr->cp_ring_struct;
	uint32_t ring_mask = cp_ring_struct->ring_mask;
	uint32_t idx = raw_cons;
	uint32_t num_bds;
	bool is_long =
		(out_msg->cmp_type == CMPL_BASE_TYPE_MID_PATH_LONG ? true : false);

	do {
		cons = RING_CMPL(ring_mask, raw_cons);
		mpc_cmpl = &cpr->cp_desc_ring[cons];

		rte_prefetch_non_temporal(&cp_desc_ring[(cons + 2) &
					  ring_mask]);

		if (!CMPL_VALID(mpc_cmpl, cpr->valid)) {
			break;
		} else if (is_long) {
			uint32_t cons_tmp = cons + 1;
			uint32_t valid;
			struct cmpl_base *tmp_mpc_cmpl = &cp_desc_ring[cons_tmp & ring_mask];

			if ((cons_tmp & ring_mask) < (cons & ring_mask))
				valid = !cpr->valid;
			else
				valid = cpr->valid;

			if (!CMPL_VALID(tmp_mpc_cmpl, valid))
				break;
		}

		NEXT_CMPL(cpr,
			  cons,
			  cpr->valid,
			  (is_long ? 2 : 1));

		rte_prefetch0(&cp_desc_ring[cons]);

		if (likely(CMP_TYPE(mpc_cmpl) == out_msg->cmp_type)) {
			nb_mpc_cmds++;
			idx = raw_cons;
			raw_cons = cons;
			break;
		} else {
			PMD_DRV_LOG_LINE(DEBUG, "Unhandled CMP type %02x",
					 CMP_TYPE(mpc_cmpl));
		}

		raw_cons = cons;
	} while (nb_mpc_cmds < ring_mask);

	if (nb_mpc_cmds) {
		memcpy(out_msg->msg_data,
		       &cpr->cp_desc_ring[idx],
		       BNXT_MPC_BP_SIZE);

		if (is_long) {
			uint32_t tidx = idx + 1;

			if (tidx >= BNXT_MPC_NB_DESC)
				tidx = 0;

			memcpy(out_msg->msg_data + BNXT_MPC_BP_SIZE,
			       &cpr->cp_desc_ring[tidx],
			       BNXT_MPC_BP_SIZE);
		}

		if (is_long)
			num_bds = 2;
		else
			num_bds = 1;

		cpr->cp_raw_cons = idx + num_bds;

		/* Handle the wrap */
		if (cpr->cp_raw_cons >= BNXT_MPC_NB_DESC) {
			cpr->epoch = (cpr->epoch == 0 ? 1 : 0);
			cpr->cp_raw_cons -= BNXT_MPC_NB_DESC;
		}

		bnxt_db_mpc_cq(cpr);
	}

	*opaque = (uint16_t)mpc_cmpl->info2;
	return nb_mpc_cmds;
}

static uint16_t bnxt_mpc_xmit(struct bnxt_mpc_mbuf *mpc_cmd,
			      struct bnxt_mpc_txq *mpc_queue,
			      uint16_t *opaque)
{
	struct bnxt_mpc_ring_info *mpr = mpc_queue->mpc_ring;
	struct bnxt_ring *ring = mpr->mpc_ring_struct;
	unsigned short nr_bds = 0;
	uint16_t prod;
	struct bnxt_sw_mpc_bd *mpc_buf;
	struct tx_bd_mp_cmd *mpc_bd;
	uint8_t *msg_buf;
	int i;

	if (unlikely(is_bnxt_in_error(mpc_queue->bp)))
		return -EIO;

	nr_bds = (mpc_cmd->msg_size + sizeof(struct tx_bd_mp_cmd) - 1)
		/ sizeof(struct tx_bd_mp_cmd) + 1;

	prod = RING_IDX(ring, mpr->raw_prod);
	mpc_buf = &mpr->mpc_buf_ring[prod];
	mpc_buf->mpc_mbuf = mpc_cmd;
	mpc_buf->nr_bds = nr_bds;

	mpc_bd = &mpr->mpc_desc_ring[prod];
	memset(mpc_bd, 0, sizeof(struct tx_bd_mp_cmd));
	mpc_bd->opaque = *opaque;
	mpc_bd->flags_type = nr_bds << TX_BD_MP_CMD_FLAGS_BD_CNT_SFT;
	mpc_bd->flags_type |= TX_BD_MP_CMD_TYPE_TX_BD_MP_CMD;
	mpc_bd->len = mpc_cmd->msg_size;

	/* copy the messages to the subsequent inline bds */
	for (i = 0; i < nr_bds - 1; i++) {
		mpr->raw_prod = RING_NEXT(mpr->raw_prod) % BNXT_MPC_NB_DESC;
		prod = RING_IDX(ring, mpr->raw_prod);
		mpc_bd = &mpr->mpc_desc_ring[prod];
		msg_buf = mpc_cmd->msg_data + i * sizeof(struct tx_bd_mp_cmd);
		memcpy(mpc_bd, msg_buf, sizeof(struct tx_bd_mp_cmd));
	}

	mpr->raw_prod = RING_NEXT(mpr->raw_prod)  % BNXT_MPC_NB_DESC;
	return 0;
}

int bnxt_mpc_send(struct bnxt *bp,
		  struct bnxt_mpc_mbuf *in_msg,
		  struct bnxt_mpc_mbuf *out_msg,
		  uint16_t *opaque,
		  bool batch)
{
	int rc;
	struct bnxt_mpc_txq *mpc_queue = bp->mpc->mpc_txq[in_msg->chnl_id];
	int retry = BNXT_MPC_RX_RETRY;
	uint32_t pi = 0;
	uint16_t rx_opaque;

	if (out_msg->cmp_type != CMPL_BASE_TYPE_MID_PATH_SHORT &&
	    out_msg->cmp_type != CMPL_BASE_TYPE_MID_PATH_LONG)
		return -1;

#ifdef MPC_DEBUG
	if (mpc_queue == NULL || mpc_queue->mpc_ring == NULL)
		return -1;
#endif

	/*
	 * Save the producer index so that if wrapping occurs
	 * it can be detected.
	 */
	pi = mpc_queue->mpc_ring->raw_prod;
	*opaque = mpc_queue->seq_num;
	mpc_queue->seq_num++;
	rc = bnxt_mpc_xmit(in_msg, mpc_queue, opaque);

	if (unlikely(rc))
		return -1;
	/*
	 * If the producer index wraps then toggle the epoch.
	 */
	if (mpc_queue->mpc_ring->raw_prod < pi)
		mpc_queue->mpc_ring->epoch = (mpc_queue->mpc_ring->epoch == 0 ? 1 : 0);

	/*
	 * Ring the Tx doorbell.
	 */
	bnxt_db_mpc_write(&mpc_queue->mpc_ring->db,
			  mpc_queue->mpc_ring->raw_prod,
			  mpc_queue->mpc_ring->epoch);

	if (batch)
		return 0;

	/* Wait for response */
	do {
		rte_delay_us_block(BNXT_MPC_RX_US_DELAY);

		rc =  bnxt_mpc_cmd_cmpl(mpc_queue, out_msg, &rx_opaque);

		if (rc == 1) {
			if (rx_opaque != *opaque)
				PMD_DRV_LOG_LINE(ERR,
					    "%s: Out of order completion. Opaque Expected:%d Got:%d",
					    __func__,
					    *opaque,
					    rx_opaque);
			return 0;
		}
#ifdef MPC_DEBUG
		PMD_DRV_LOG_LINE("Received zero or more than one completion:%d", rc);
#endif
		retry--;
	} while (retry);

	return -1;
}
