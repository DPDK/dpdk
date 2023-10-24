/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2023 Broadcom
 * All rights reserved.
 */

#include <inttypes.h>

#include <rte_memzone.h>
#include <rte_malloc.h>

#include "bnxt.h"
#include "bnxt_rxq.h"
#include "bnxt_rxr.h"
#include "bnxt_ring.h"
#include "bnxt_vnic.h"
#include "hsi_struct_def_dpdk.h"
#include "bnxt_hwrm.h"

/* Macros to manipulate vnic bitmaps*/
#define BNXT_VNIC_BITMAP_SIZE	64
#define BNXT_VNIC_BITMAP_SET(b, i)	((b[(i) / BNXT_VNIC_BITMAP_SIZE]) |= \
			(1UL << ((BNXT_VNIC_BITMAP_SIZE - 1) - \
			((i) % BNXT_VNIC_BITMAP_SIZE))))

#define BNXT_VNIC_BITMAP_RESET(b, i)	((b[(i) / BNXT_VNIC_BITMAP_SIZE]) &= \
			(~(1UL << ((BNXT_VNIC_BITMAP_SIZE - 1) - \
			((i) % BNXT_VNIC_BITMAP_SIZE)))))

#define BNXT_VNIC_BITMAP_GET(b, i)	(((b[(i) / BNXT_VNIC_BITMAP_SIZE]) >> \
			((BNXT_VNIC_BITMAP_SIZE - 1) - \
			((i) % BNXT_VNIC_BITMAP_SIZE))) & 1)

/*
 * VNIC Functions
 */

void bnxt_prandom_bytes(void *dest_ptr, size_t len)
{
	char *dest = (char *)dest_ptr;
	uint64_t rb;

	while (len) {
		rb = rte_rand();
		if (len >= 8) {
			memcpy(dest, &rb, 8);
			len -= 8;
			dest += 8;
		} else {
			memcpy(dest, &rb, len);
			dest += len;
			len = 0;
		}
	}
}

static void bnxt_init_vnics(struct bnxt *bp)
{
	struct bnxt_vnic_info *vnic;
	uint16_t max_vnics;
	int i;

	max_vnics = bp->max_vnics;
	STAILQ_INIT(&bp->free_vnic_list);
	for (i = 0; i < max_vnics; i++) {
		vnic = &bp->vnic_info[i];
		vnic->fw_vnic_id = (uint16_t)HWRM_NA_SIGNATURE;
		vnic->rss_rule = (uint16_t)HWRM_NA_SIGNATURE;
		vnic->cos_rule = (uint16_t)HWRM_NA_SIGNATURE;
		vnic->lb_rule = (uint16_t)HWRM_NA_SIGNATURE;
		vnic->hash_mode =
			HWRM_VNIC_RSS_CFG_INPUT_HASH_MODE_FLAGS_DEFAULT;
		vnic->prev_hash_mode =
			HWRM_VNIC_RSS_CFG_INPUT_HASH_MODE_FLAGS_DEFAULT;
		vnic->rx_queue_cnt = 0;

		STAILQ_INIT(&vnic->filter);
		STAILQ_INIT(&vnic->flow_list);
		STAILQ_INSERT_TAIL(&bp->free_vnic_list, vnic, next);
	}
}

struct bnxt_vnic_info *bnxt_alloc_vnic(struct bnxt *bp)
{
	struct bnxt_vnic_info *vnic;

	/* Find the 1st unused vnic from the free_vnic_list pool*/
	vnic = STAILQ_FIRST(&bp->free_vnic_list);
	if (!vnic) {
		PMD_DRV_LOG(ERR, "No more free VNIC resources\n");
		return NULL;
	}
	STAILQ_REMOVE_HEAD(&bp->free_vnic_list, next);
	return vnic;
}

void bnxt_free_all_vnics(struct bnxt *bp)
{
	struct bnxt_vnic_info *vnic;
	unsigned int i;

	if (bp->vnic_info == NULL)
		return;

	for (i = 0; i < bp->max_vnics; i++) {
		vnic = &bp->vnic_info[i];
		STAILQ_INSERT_TAIL(&bp->free_vnic_list, vnic, next);
		if (vnic->ref_cnt) {
			/* clean up the default vnic details */
			bnxt_vnic_rss_action_free(bp, i);
		}

		vnic->rx_queue_cnt = 0;
	}
}

void bnxt_free_vnic_attributes(struct bnxt *bp)
{
	struct bnxt_vnic_info *vnic;
	unsigned int i;

	if (bp->vnic_info == NULL)
		return;

	for (i = 0; i < bp->max_vnics; i++) {
		vnic = &bp->vnic_info[i];
		if (vnic->rss_mz != NULL) {
			rte_memzone_free(vnic->rss_mz);
			vnic->rss_mz = NULL;
			vnic->rss_hash_key = NULL;
			vnic->rss_table = NULL;
		}
	}
}

int bnxt_alloc_vnic_attributes(struct bnxt *bp, bool reconfig)
{
	struct bnxt_vnic_info *vnic;
	struct rte_pci_device *pdev = bp->pdev;
	const struct rte_memzone *mz;
	char mz_name[RTE_MEMZONE_NAMESIZE];
	uint32_t entry_length;
	size_t rss_table_size;
	int i;
	rte_iova_t mz_phys_addr;

	entry_length = HW_HASH_KEY_SIZE;

	if (BNXT_CHIP_P5(bp))
		rss_table_size = BNXT_RSS_TBL_SIZE_P5 *
				 2 * sizeof(*vnic->rss_table);
	else
		rss_table_size = HW_HASH_INDEX_SIZE * sizeof(*vnic->rss_table);

	entry_length = RTE_CACHE_LINE_ROUNDUP(entry_length + rss_table_size);

	for (i = 0; i < bp->max_vnics; i++) {
		vnic = &bp->vnic_info[i];

		snprintf(mz_name, RTE_MEMZONE_NAMESIZE,
			 "bnxt_" PCI_PRI_FMT "_vnicattr_%d", pdev->addr.domain,
			 pdev->addr.bus, pdev->addr.devid, pdev->addr.function, i);
		mz_name[RTE_MEMZONE_NAMESIZE - 1] = 0;
		mz = rte_memzone_lookup(mz_name);
		if (mz == NULL) {
			mz = rte_memzone_reserve(mz_name,
						 entry_length,
						 bp->eth_dev->device->numa_node,
						 RTE_MEMZONE_2MB |
						 RTE_MEMZONE_SIZE_HINT_ONLY |
						 RTE_MEMZONE_IOVA_CONTIG);
			if (mz == NULL) {
				PMD_DRV_LOG(ERR, "Cannot allocate bnxt vnic_attributes memory\n");
				return -ENOMEM;
			}
		}
		vnic->rss_mz = mz;
		mz_phys_addr = mz->iova;

		/* Allocate rss table and hash key */
		vnic->rss_table = (void *)((char *)mz->addr);
		vnic->rss_table_dma_addr = mz_phys_addr;
		memset(vnic->rss_table, -1, entry_length);

		vnic->rss_hash_key = (void *)((char *)vnic->rss_table + rss_table_size);
		vnic->rss_hash_key_dma_addr = vnic->rss_table_dma_addr + rss_table_size;
		if (!reconfig) {
			bnxt_prandom_bytes(vnic->rss_hash_key, HW_HASH_KEY_SIZE);
			memcpy(bp->rss_conf.rss_key, vnic->rss_hash_key, HW_HASH_KEY_SIZE);
		} else {
			memcpy(vnic->rss_hash_key, bp->rss_conf.rss_key, HW_HASH_KEY_SIZE);
		}
	}

	return 0;
}

void bnxt_free_vnic_mem(struct bnxt *bp)
{
	struct bnxt_vnic_info *vnic;
	uint16_t max_vnics, i;

	if (bp->vnic_info == NULL)
		return;

	max_vnics = bp->max_vnics;
	for (i = 0; i < max_vnics; i++) {
		vnic = &bp->vnic_info[i];
		if (vnic->fw_vnic_id != (uint16_t)HWRM_NA_SIGNATURE) {
			PMD_DRV_LOG(ERR, "VNIC is not freed yet!\n");
			/* TODO Call HWRM to free VNIC */
		}
	}

	rte_free(bp->vnic_info);
	bp->vnic_info = NULL;
}

int bnxt_alloc_vnic_mem(struct bnxt *bp)
{
	struct bnxt_vnic_info *vnic_mem;
	uint16_t max_vnics;

	max_vnics = bp->max_vnics;
	/* Allocate memory for VNIC pool and filter pool */
	vnic_mem = rte_zmalloc("bnxt_vnic_info",
			       max_vnics * sizeof(struct bnxt_vnic_info), 0);
	if (vnic_mem == NULL) {
		PMD_DRV_LOG(ERR, "Failed to alloc memory for %d VNICs",
			max_vnics);
		return -ENOMEM;
	}
	bp->vnic_info = vnic_mem;
	bnxt_init_vnics(bp);
	return 0;
}

int bnxt_vnic_grp_alloc(struct bnxt *bp, struct bnxt_vnic_info *vnic)
{
	uint32_t size = sizeof(*vnic->fw_grp_ids) * bp->max_ring_grps;
	uint32_t i;

	vnic->fw_grp_ids = rte_zmalloc("vnic_fw_grp_ids", size, 0);
	if (!vnic->fw_grp_ids) {
		PMD_DRV_LOG(ERR,
			    "Failed to alloc %d bytes for group ids\n",
			    size);
		return -ENOMEM;
	}

	/* Initialize to invalid ring id */
	for (i = 0; i < bp->max_ring_grps; i++)
		vnic->fw_grp_ids[i] = INVALID_HW_RING_ID;

	return 0;
}

uint16_t bnxt_rte_to_hwrm_hash_types(uint64_t rte_type)
{
	uint16_t hwrm_type = 0;

	if ((rte_type & RTE_ETH_RSS_IPV4) ||
	    (rte_type & RTE_ETH_RSS_ECPRI))
		hwrm_type |= HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_IPV4;
	if (rte_type & RTE_ETH_RSS_NONFRAG_IPV4_TCP)
		hwrm_type |= HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_TCP_IPV4;
	if (rte_type & RTE_ETH_RSS_NONFRAG_IPV4_UDP)
		hwrm_type |= HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_UDP_IPV4;
	if (rte_type & RTE_ETH_RSS_IPV6)
		hwrm_type |= HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_IPV6;
	if (rte_type & RTE_ETH_RSS_NONFRAG_IPV6_TCP)
		hwrm_type |= HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_TCP_IPV6;
	if (rte_type & RTE_ETH_RSS_NONFRAG_IPV6_UDP)
		hwrm_type |= HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_UDP_IPV6;

	return hwrm_type;
}

int bnxt_rte_to_hwrm_hash_level(struct bnxt *bp, uint64_t hash_f, uint32_t lvl)
{
	uint32_t mode = HWRM_VNIC_RSS_CFG_INPUT_HASH_MODE_FLAGS_DEFAULT;
	bool l3 = (hash_f & (RTE_ETH_RSS_IPV4 | RTE_ETH_RSS_IPV6 | RTE_ETH_RSS_ECPRI));
	bool l4 = (hash_f & (RTE_ETH_RSS_NONFRAG_IPV4_UDP |
			     RTE_ETH_RSS_NONFRAG_IPV6_UDP |
			     RTE_ETH_RSS_NONFRAG_IPV4_TCP |
			     RTE_ETH_RSS_NONFRAG_IPV6_TCP));
	bool l3_only = l3 && !l4;
	bool l3_and_l4 = l3 && l4;

	/* If FW has not advertised capability to configure outer/inner
	 * RSS hashing , just log a message. HW will work in default RSS mode.
	 */
	if ((BNXT_CHIP_P5(bp) && BNXT_VNIC_OUTER_RSS_UNSUPPORTED(bp)) ||
	    (!BNXT_CHIP_P5(bp) && !(bp->vnic_cap_flags & BNXT_VNIC_CAP_OUTER_RSS))) {
		if (lvl)
			PMD_DRV_LOG(INFO,
				    "Given RSS level is unsupported, using default RSS level\n");
		return mode;
	}

	switch (lvl) {
	case BNXT_RSS_LEVEL_INNERMOST:
		/* Irrespective of what RTE says, FW always does 4 tuple */
		if (l3_and_l4 || l4 || l3_only)
			mode = BNXT_HASH_MODE_INNERMOST;
		break;
	case BNXT_RSS_LEVEL_OUTERMOST:
		/* Irrespective of what RTE says, FW always does 4 tuple */
		if (l3_and_l4 || l4 || l3_only)
			mode = BNXT_HASH_MODE_OUTERMOST;
		break;
	default:
		mode = BNXT_HASH_MODE_DEFAULT;
		break;
	}

	return mode;
}

uint64_t bnxt_hwrm_to_rte_rss_level(struct bnxt *bp, uint32_t mode)
{
	uint64_t rss_level = 0;

	/* If FW has not advertised capability to configure inner/outer RSS
	 * return default hash mode.
	 */
	if ((BNXT_CHIP_P5(bp) && BNXT_VNIC_OUTER_RSS_UNSUPPORTED(bp)) ||
	    (!BNXT_CHIP_P5(bp) && !(bp->vnic_cap_flags & BNXT_VNIC_CAP_OUTER_RSS)))
		return RTE_ETH_RSS_LEVEL_PMD_DEFAULT;

	if (mode == HWRM_VNIC_RSS_CFG_INPUT_HASH_MODE_FLAGS_OUTERMOST_2 ||
	    mode == HWRM_VNIC_RSS_CFG_INPUT_HASH_MODE_FLAGS_OUTERMOST_4)
		rss_level |= RTE_ETH_RSS_LEVEL_OUTERMOST;
	else if (mode == HWRM_VNIC_RSS_CFG_INPUT_HASH_MODE_FLAGS_INNERMOST_2 ||
		 mode == HWRM_VNIC_RSS_CFG_INPUT_HASH_MODE_FLAGS_INNERMOST_4)
		rss_level |= RTE_ETH_RSS_LEVEL_INNERMOST;
	else
		rss_level |= RTE_ETH_RSS_LEVEL_PMD_DEFAULT;

	return rss_level;
}

static
int32_t bnxt_vnic_populate_rss_table_p5(struct bnxt *bp,
					struct bnxt_vnic_info *vnic)
{
	uint32_t ctx_idx = 0, rss_idx = 0, cnt = 0;
	uint32_t q_id = -1;
	struct bnxt_rx_queue *rxq;
	uint16_t *ring_tbl = vnic->rss_table;
	uint8_t *rx_queue_state = bp->eth_dev->data->rx_queue_state;
	uint16_t ring_id;

	/* For P5 platform */
	for (ctx_idx = 0; ctx_idx < vnic->num_lb_ctxts; ctx_idx++) {
		for (rss_idx = 0; rss_idx < BNXT_RSS_ENTRIES_PER_CTX_P5;
		      rss_idx++) {
			/* Find next active ring. */
			for (cnt = 0; cnt < BNXT_VNIC_MAX_QUEUE_SIZE; cnt++) {
				if (++q_id == bp->rx_nr_rings)
					q_id = 0; /* reset the q_id */
				if (BNXT_VNIC_BITMAP_GET(vnic->queue_bitmap,
							 q_id) &&
				    rx_queue_state[q_id] !=
						RTE_ETH_QUEUE_STATE_STOPPED)
					break;
			}

			/* no active queues exit */
			if (cnt == BNXT_VNIC_MAX_QUEUE_SIZE)
				return 0;

			rxq = bp->rx_queues[q_id];
			ring_id = rxq->rx_ring->rx_ring_struct->fw_ring_id;
			*ring_tbl++ = rte_cpu_to_le_16(ring_id);
			ring_id = rxq->cp_ring->cp_ring_struct->fw_ring_id;
			*ring_tbl++ = rte_cpu_to_le_16(ring_id);
		}
	}
	return 0;
}

static
int32_t bnxt_vnic_populate_rss_table_p4(struct bnxt *bp,
					struct bnxt_vnic_info *vnic)
{
	uint32_t rss_idx = 0, cnt = 0;
	uint32_t q_id = -1;
	uint16_t *ring_tbl = vnic->rss_table;
	uint8_t *rx_queue_state = bp->eth_dev->data->rx_queue_state;
	uint16_t ring_id;

	/* For Wh+ platform */
	for (rss_idx = 0; rss_idx < bnxt_rss_hash_tbl_size(bp); rss_idx++) {
		/* Find next active ring. */
		for (cnt = 0; cnt < BNXT_VNIC_MAX_QUEUE_SIZE; cnt++) {
			if (++q_id == bp->rx_nr_rings)
				q_id = 0; /* reset the q_id */
			if (BNXT_VNIC_BITMAP_GET(vnic->queue_bitmap,
						 q_id) &&
			    rx_queue_state[q_id] !=
					RTE_ETH_QUEUE_STATE_STOPPED)
				break;
		}

		/* no active queues exit */
		if (cnt == BNXT_VNIC_MAX_QUEUE_SIZE)
			return 0;

		ring_id = vnic->fw_grp_ids[q_id];
		*ring_tbl++ = rte_cpu_to_le_16(ring_id);
	}
	return 0;
}

static
int32_t bnxt_vnic_populate_rss_table(struct bnxt *bp,
				     struct bnxt_vnic_info *vnic)
{
	/* RSS table population is different for p4 and p5 platforms */
	if (BNXT_CHIP_P5(bp))
		return bnxt_vnic_populate_rss_table_p5(bp, vnic);

	return bnxt_vnic_populate_rss_table_p4(bp, vnic);
}

static void
bnxt_vnic_queue_delete(struct bnxt *bp, uint16_t vnic_idx)
{
	struct bnxt_vnic_info *vnic = &bp->vnic_info[vnic_idx];

	if (bnxt_hwrm_vnic_free(bp, vnic))
		PMD_DRV_LOG(ERR, "Failed to delete queue\n");

	if (vnic->fw_grp_ids) {
		rte_free(vnic->fw_grp_ids);
		vnic->fw_grp_ids = NULL;
	}

	vnic->rx_queue_cnt = 0;
	if (bp->nr_vnics)
		bp->nr_vnics--;

	/* reset the queue_bitmap */
	memset(vnic->queue_bitmap, 0, sizeof(vnic->queue_bitmap));
}

static struct bnxt_vnic_info*
bnxt_vnic_queue_create(struct bnxt *bp, int32_t vnic_id, uint16_t q_index)
{
	uint8_t *rx_queue_state = bp->eth_dev->data->rx_queue_state;
	struct bnxt_vnic_info *vnic;
	struct bnxt_rx_queue *rxq = NULL;
	int32_t rc = -EINVAL;
	uint16_t saved_mru = 0;

	vnic = &bp->vnic_info[vnic_id];
	if (vnic->rx_queue_cnt) {
		PMD_DRV_LOG(ERR, "invalid queue configuration %d\n", vnic_id);
		return NULL;
	}

	/* set the queue_bitmap */
	BNXT_VNIC_BITMAP_SET(vnic->queue_bitmap, q_index);

	rxq = bp->rx_queues[q_index];
	if (rx_queue_state[q_index] == RTE_ETH_QUEUE_STATE_STOPPED)
		rxq->rx_started = 0;
	else
		rxq->rx_started = 1;

	vnic->rx_queue_cnt++;
	vnic->start_grp_id = q_index;
	vnic->end_grp_id = q_index + 1;
	vnic->func_default = 0;	/* This is not a default VNIC. */
	bp->nr_vnics++;

	/* Allocate vnic group for p4 platform */
	rc = bnxt_vnic_grp_alloc(bp, vnic);
	if (rc) {
		PMD_DRV_LOG(DEBUG, "Failed to allocate vnic groups\n");
		goto cleanup;
	}

	/* populate the fw group table */
	bnxt_vnic_ring_grp_populate(bp, vnic);
	bnxt_vnic_rules_init(vnic);

	rc = bnxt_hwrm_vnic_alloc(bp, vnic);
	if (rc) {
		PMD_DRV_LOG(DEBUG, "Failed to allocate vnic %d\n", q_index);
		goto cleanup;
	}

	/* store the mru so we can set it to zero in hw */
	if (rxq->rx_started == 0) {
		saved_mru = vnic->mru;
		vnic->mru = 0;
	}

	rc = bnxt_hwrm_vnic_cfg(bp, vnic);
	if (rxq->rx_started == 0)
		vnic->mru = saved_mru;

	if (rc) {
		PMD_DRV_LOG(DEBUG, "Failed to configure vnic %d\n", q_index);
		goto cleanup;
	}

	rc = bnxt_hwrm_vnic_plcmode_cfg(bp, vnic);
	if (rc) {
		PMD_DRV_LOG(DEBUG, "Failed to configure vnic plcmode %d\n",
			    q_index);
		goto cleanup;
	}

	vnic->ref_cnt++;
	return vnic;

cleanup:
	bnxt_vnic_queue_delete(bp, vnic_id);
	return NULL;
}

static inline int32_t
bnxt_vnic_queue_db_lookup(struct bnxt *bp, uint64_t *q_list)
{
	/* lookup in the database to check if it is in use */
	return rte_hash_lookup(bp->vnic_queue_db.rss_q_db,
			       (const void *)q_list);
}

static inline int32_t
bnxt_vnic_queue_db_del(struct bnxt *bp, uint64_t *q_list)
{
	return rte_hash_del_key(bp->vnic_queue_db.rss_q_db,
				(const void *)q_list);
}

static int32_t
bnxt_vnic_queue_db_add(struct bnxt *bp, uint64_t *q_list)
{
	struct bnxt_vnic_info *vnic_info;
	int32_t vnic_id, rc = -1;

	vnic_id = rte_hash_add_key(bp->vnic_queue_db.rss_q_db,
				   (const void *)q_list);

	if (vnic_id < 0 || vnic_id >= bp->max_vnics) {
		PMD_DRV_LOG(DEBUG, "unable to assign vnic index %d\n",
			    vnic_id);
		return rc;
	}

	vnic_info = &bp->vnic_info[vnic_id];
	if (vnic_info->fw_vnic_id != INVALID_HW_RING_ID) {
		PMD_DRV_LOG(DEBUG, "Invalid ring id for %d.\n", vnic_id);
		return rc;
	}
	return vnic_id;
}

/* Function to validate the incoming rss configuration */
static
int32_t bnxt_vnic_queue_db_rss_validate(struct bnxt *bp,
					struct bnxt_vnic_rss_info *rss_info,
					int32_t *vnic_idx)
{
	struct rte_eth_conf *dev_conf = &bp->eth_dev->data->dev_conf;
	int32_t rc = -EINVAL;
	uint32_t idx = 0;
	int32_t out_idx;

	if (!(dev_conf->rxmode.mq_mode & RTE_ETH_MQ_RX_RSS)) {
		PMD_DRV_LOG(ERR, "Error Rss is not supported on this port\n");
		return rc;
	}

	/* rss queue is zero then use the default vnic */
	if (rss_info->queue_num == 0) {
		*vnic_idx = 0;
		return 0;
	}

	/* Check to see if the queues id are in supported range */
	if (rss_info->queue_num > bp->rx_nr_rings) {
		PMD_DRV_LOG(ERR, "Error unsupported queue num.\n");
		return rc;
	}

	/* validate the queue ids are in correct range */
	for (idx = 0; idx < BNXT_VNIC_MAX_QUEUE_SIZE; idx++) {
		if (BNXT_VNIC_BITMAP_GET(rss_info->queue_list, idx)) {
			if (idx >= bp->rx_nr_rings) {
				PMD_DRV_LOG(ERR,
					    "Error %d beyond support size %u\n",
					    idx, bp->rx_nr_rings);
				return rc;
			}
		}
	}

	/* check if the vnic already exist */
	out_idx = bnxt_vnic_queue_db_lookup(bp, rss_info->queue_list);
	if (out_idx < 0 || out_idx >= bp->max_vnics)
		return -ENOENT; /* entry not found */

	/* found an entry */
	*vnic_idx = out_idx;
	return 0;
}

static void
bnxt_vnic_rss_delete(struct bnxt *bp, uint16_t q_index)
{
	struct bnxt_vnic_info *vnic;

	vnic = &bp->vnic_info[q_index];
	if (vnic->rx_queue_cnt >= 1)
		bnxt_hwrm_vnic_ctx_free(bp, vnic);

	if (vnic->fw_vnic_id != INVALID_HW_RING_ID)
		bnxt_hwrm_vnic_free(bp, vnic);

	if (vnic->fw_grp_ids) {
		rte_free(vnic->fw_grp_ids);
		vnic->fw_grp_ids = NULL;
	}

	/* Update the vnic details for all the rx queues */
	vnic->rx_queue_cnt = 0;
	memset(vnic->queue_bitmap, 0, sizeof(vnic->queue_bitmap));

	if (bp->nr_vnics)
		bp->nr_vnics--;
}

/* The validation of the rss_info should be done before calling this function*/

static struct bnxt_vnic_info *
bnxt_vnic_rss_create(struct bnxt *bp,
		     struct bnxt_vnic_rss_info *rss_info,
		     uint16_t vnic_id)
{
	uint8_t *rx_queue_state = bp->eth_dev->data->rx_queue_state;
	struct bnxt_vnic_info *vnic;
	struct bnxt_rx_queue *rxq = NULL;
	uint32_t idx, nr_ctxs, config_rss = 0;
	uint16_t saved_mru = 0;
	uint16_t active_q_cnt = 0;
	int16_t first_q = -1;
	int16_t end_q = -1;
	int32_t rc = 0;

	/* Assign the vnic to be used for this rss configuration */
	vnic = &bp->vnic_info[vnic_id];

	/* Update the vnic details for all the rx queues */
	for (idx = 0; idx < BNXT_VNIC_MAX_QUEUE_SIZE; idx++) {
		if (BNXT_VNIC_BITMAP_GET(rss_info->queue_list, idx)) {
			rxq = bp->rx_queues[idx];
			if (rx_queue_state[idx] ==
			    RTE_ETH_QUEUE_STATE_STOPPED) {
				rxq->rx_started = 0;
			} else {
				rxq->rx_started = 1;
				active_q_cnt++;
			}
			vnic->rx_queue_cnt++;

			/* Update the queue list */
			BNXT_VNIC_BITMAP_SET(vnic->queue_bitmap, idx);
			if (first_q == -1)
				first_q = idx;
			end_q = idx;
		}
	}
	vnic->start_grp_id = first_q;
	vnic->end_grp_id = end_q + 1;
	vnic->func_default = 0;	/* This is not a default VNIC. */
	bp->nr_vnics++;

	/* Allocate vnic group for p4 platform */
	rc = bnxt_vnic_grp_alloc(bp, vnic);
	if (rc) {
		PMD_DRV_LOG(ERR, "Failed to allocate vnic groups\n");
		goto fail_cleanup;
	}

	/* populate the fw group table */
	bnxt_vnic_ring_grp_populate(bp, vnic);
	bnxt_vnic_rules_init(vnic);

	/* Allocate the vnic in the firmware */
	rc = bnxt_hwrm_vnic_alloc(bp, vnic);
	if (rc) {
		PMD_DRV_LOG(ERR, "Failed to allocate vnic %d\n", idx);
		goto fail_cleanup;
	}

	/* Allocate the vnic rss context */
	/* RSS table size in P5 is 512. Cap max Rx rings to same value */
	nr_ctxs = bnxt_rss_ctxts(bp);
	for (idx = 0; idx < nr_ctxs; idx++) {
		rc = bnxt_hwrm_vnic_ctx_alloc(bp, vnic, idx);
		if (rc)
			break;
	}
	if (rc) {
		PMD_DRV_LOG(ERR,
			    "HWRM ctx %d alloc failure rc: %x\n", idx, rc);
		goto fail_cleanup;
	}
	vnic->num_lb_ctxts = nr_ctxs;

	saved_mru = vnic->mru;
	if (!active_q_cnt)
		vnic->mru = 0;

	/* configure the vnic details in firmware */
	rc = bnxt_hwrm_vnic_cfg(bp, vnic);
	vnic->mru = saved_mru;
	if (rc) {
		PMD_DRV_LOG(ERR, "Failed to configure vnic %d\n", idx);
		goto fail_cleanup;
	}

	rc = bnxt_hwrm_vnic_plcmode_cfg(bp, vnic);
	if (rc) {
		PMD_DRV_LOG(ERR, "Failed to configure vnic plcmode %d\n",
			    idx);
		goto fail_cleanup;
	}

	/* hwrm_type conversion */
	vnic->hash_type = bnxt_rte_to_hwrm_hash_types(rss_info->rss_types);
	vnic->hash_mode = bnxt_rte_to_hwrm_hash_level(bp, rss_info->rss_types,
						      rss_info->rss_level);

	/* configure the key */
	if (!rss_info->key_len)
		/* If hash key has not been specified, use random hash key.*/
		bnxt_prandom_bytes(vnic->rss_hash_key, HW_HASH_KEY_SIZE);
	else
		memcpy(vnic->rss_hash_key, rss_info->key, rss_info->key_len);

	/* Prepare the indirection table */
	bnxt_vnic_populate_rss_table(bp, vnic);

	/* check to see if there is at least one queue that is active */
	for (idx = vnic->start_grp_id; idx < vnic->end_grp_id; idx++) {
		if (bnxt_vnic_queue_id_is_valid(vnic, idx) &&
		    bp->rx_queues[idx]->rx_started) {
			config_rss = 1;
			break;
		}
	}

	/* configure the rss table */
	if (config_rss) {
		rc = bnxt_hwrm_vnic_rss_cfg(bp, vnic);
		if (rc) {
			memset(vnic->rss_hash_key, 0, HW_HASH_KEY_SIZE);
			PMD_DRV_LOG(ERR,
				    "Failed to configure vnic rss details %d\n",
				    idx);
			goto fail_cleanup;
		}
	}

	vnic->ref_cnt++;
	return vnic;

fail_cleanup:
	bnxt_vnic_rss_delete(bp, idx);
	return NULL;
}

int32_t
bnxt_vnic_rss_queue_status_update(struct bnxt *bp, struct bnxt_vnic_info *vnic)
{
	if (vnic->fw_vnic_id == INVALID_HW_RING_ID)
		return 0;

	if (!(vnic->rss_table && vnic->hash_type))
		return 0;

	/* Prepare the indirection table */
	bnxt_vnic_populate_rss_table(bp, vnic);

	/* configure the rss table */
	if (bnxt_hwrm_vnic_rss_cfg(bp, vnic)) {
		PMD_DRV_LOG(DEBUG, "Failed to update vnic rss details\n");
		return -EINVAL;
	}
	return 0;
}

static int32_t
bnxt_vnic_rss_hash_algo_update(struct bnxt *bp,
			       struct bnxt_vnic_info *vnic,
			       struct bnxt_vnic_rss_info *rss_info)
{
	uint8_t old_rss_hash_key[HW_HASH_KEY_SIZE] = { 0 };
	uint16_t	hash_type;
	uint8_t		hash_mode;
	uint32_t apply = 0;

	/* validate key length */
	if (rss_info->key_len != 0 && rss_info->key_len != HW_HASH_KEY_SIZE) {
		PMD_DRV_LOG(ERR,
			    "Invalid hashkey length, should be %d bytes\n",
			    HW_HASH_KEY_SIZE);
		return -EINVAL;
	}

	/* hwrm_type conversion */
	hash_type = bnxt_rte_to_hwrm_hash_types(rss_info->rss_types);
	hash_mode = bnxt_rte_to_hwrm_hash_level(bp, rss_info->rss_types,
						rss_info->rss_level);
	if (vnic->hash_mode != hash_mode ||
	    vnic->hash_type != hash_type) {
		apply = 1;
		vnic->hash_mode = hash_mode;
		vnic->hash_type = hash_type;
	}
	/* Store the old hash key before programming the new one. It will
	 * be used to restore the old hash key when HWRM_VNIC_RSS_CFG
	 * fails.
	 */
	memcpy(old_rss_hash_key, vnic->rss_hash_key, HW_HASH_KEY_SIZE);
	if (rss_info->key_len != 0 && memcmp(rss_info->key, vnic->rss_hash_key,
					     HW_HASH_KEY_SIZE)) {
		apply = 1;
		memcpy(vnic->rss_hash_key, rss_info->key, HW_HASH_KEY_SIZE);
	}

	if (apply) {
		if (bnxt_hwrm_vnic_rss_cfg(bp, vnic)) {
			memcpy(vnic->rss_hash_key, old_rss_hash_key, HW_HASH_KEY_SIZE);
			BNXT_TF_DBG(ERR, "Error configuring vnic RSS config\n");
			return -EINVAL;
		}
		BNXT_TF_DBG(INFO, "Rss config successfully applied\n");
	}
	return 0;
}

int32_t bnxt_vnic_queue_db_deinit(struct bnxt *bp)
{
	rte_hash_free(bp->vnic_queue_db.rss_q_db);
	return 0;
}

int32_t bnxt_vnic_queue_db_init(struct bnxt *bp)
{
	struct rte_hash_parameters hash_tbl_params = {0};
	char hash_tbl_name[64] = {0};

	/* choose the least supported value */
	if (bp->rx_nr_rings > BNXT_VNIC_MAX_QUEUE_SIZE)
		bp->vnic_queue_db.num_queues = BNXT_VNIC_MAX_QUEUE_SIZE;
	else
		bp->vnic_queue_db.num_queues = bp->rx_nr_rings;

	/* create the hash table for the rss hash entries */
	snprintf(hash_tbl_name, sizeof(hash_tbl_name),
		 "bnxt_rss_hash_%d", bp->eth_dev->data->port_id);
	hash_tbl_params.name = hash_tbl_name;
	hash_tbl_params.entries = (bp->max_vnics > BNXT_VNIC_MAX_SUPPORTED_ID) ?
		BNXT_VNIC_MAX_SUPPORTED_ID : bp->max_vnics;
	hash_tbl_params.key_len = BNXT_VNIC_MAX_QUEUE_SZ_IN_8BITS;
	hash_tbl_params.socket_id = rte_socket_id();
	bp->vnic_queue_db.rss_q_db = rte_hash_create(&hash_tbl_params);
	if (bp->vnic_queue_db.rss_q_db == NULL) {
		PMD_DRV_LOG(ERR, "Failed to create rss hash tbl\n");
		return -ENOMEM;
	}
	return 0;
}

void bnxt_vnic_queue_db_update_dlft_vnic(struct bnxt *bp)
{
	struct bnxt_vnic_info *dflt_vnic;
	uint64_t bitmap[BNXT_VNIC_MAX_QUEUE_SZ_IN_64BITS];
	uint32_t idx;
	int32_t vnic_id;

	/* populate all the queue ids in the default vnic */
	memset(bitmap, 0, sizeof(bitmap));
	for (idx = 0; idx < bp->vnic_queue_db.num_queues; idx++)
		BNXT_VNIC_BITMAP_SET(bitmap, idx);

	vnic_id  = bnxt_vnic_queue_db_add(bp, bitmap);
	if (vnic_id < 0) {
		PMD_DRV_LOG(ERR, "Unable to alloc vnic for default rss\n");
		return;
	}

	dflt_vnic  = bnxt_vnic_queue_db_get_vnic(bp, vnic_id);
	if (dflt_vnic == NULL) {
		PMD_DRV_LOG(ERR, "Invalid vnic for default rss %d\n", vnic_id);
		return;
	}
	/* Update the default vnic structure */
	bp->vnic_queue_db.dflt_vnic_id = vnic_id;
	memcpy(dflt_vnic->queue_bitmap, bitmap, sizeof(bitmap));
	dflt_vnic->rx_queue_cnt = bp->vnic_queue_db.num_queues;
	dflt_vnic->ref_cnt++;
}

int32_t bnxt_vnic_queue_action_alloc(struct bnxt *bp,
				     uint16_t q_index,
				     uint16_t *vnic_idx,
				     uint16_t *vnicid)
{
	uint64_t queue_list[BNXT_VNIC_MAX_QUEUE_SZ_IN_64BITS] = {0};
	struct bnxt_vnic_info *vnic_info;
	int32_t idx;
	int32_t rc = -EINVAL;

	/* validate the given queue id */
	if (q_index >= bp->rx_nr_rings || q_index >= BNXT_VNIC_MAX_QUEUE_SIZE) {
		PMD_DRV_LOG(ERR, "invalid queue id should be less than %d\n",
			    bp->rx_nr_rings);
		return rc;
	}

	/* Populate the queue list */
	BNXT_VNIC_BITMAP_SET(queue_list, q_index);

	/* check to see if the q_index is already in use */
	idx = bnxt_vnic_queue_db_lookup(bp, queue_list);
	if (idx < 0) {
		/* Assign the vnic slot */
		idx = bnxt_vnic_queue_db_add(bp, queue_list);
		if (idx < 0) {
			PMD_DRV_LOG(DEBUG, "Unable to alloc vnic for queue\n");
			return rc;
		}

		/* Allocate a new one */
		vnic_info = bnxt_vnic_queue_create(bp, idx, q_index);
		if (!vnic_info) {
			PMD_DRV_LOG(ERR, "failed to create vnic - %d\n",
				    q_index);
			bnxt_vnic_queue_db_del(bp, queue_list);
			return rc; /* failed */
		}
	} else {
		vnic_info = bnxt_vnic_queue_db_get_vnic(bp, idx);
		if (vnic_info == NULL) {
			PMD_DRV_LOG(ERR, "Unable to lookup vnic for queue %d\n",
				    q_index);
			return rc;
		}
		/* increment the reference count and return the vnic id */
		vnic_info->ref_cnt++;
	}
	*vnic_idx = (uint16_t)idx;
	*vnicid = vnic_info->fw_vnic_id;
	return 0;
}

int32_t
bnxt_vnic_queue_action_free(struct bnxt *bp, uint16_t vnic_id)
{
	struct bnxt_vnic_info *vnic_info;
	int32_t rc = -EINVAL;
	int32_t vnic_idx = vnic_id, idx;

	/* validate the given vnic idx */
	if (vnic_idx >= bp->max_vnics) {
		PMD_DRV_LOG(ERR, "invalid vnic idx %d\n", vnic_idx);
		return rc;
	}

	/* validate the vnic info */
	vnic_info = &bp->vnic_info[vnic_idx];
	if (!vnic_info->rx_queue_cnt) {
		PMD_DRV_LOG(ERR, "Invalid vnic idx, no queues being used\n");
		return rc;
	}
	if (vnic_info->ref_cnt) {
		vnic_info->ref_cnt--;
		if (!vnic_info->ref_cnt) {
			idx  = bnxt_vnic_queue_db_del(bp,
						      vnic_info->queue_bitmap);
			/* Check to ensure there is no corruption */
			if (idx != vnic_idx)
				PMD_DRV_LOG(ERR, "bad vnic idx %d\n", vnic_idx);

			bnxt_vnic_queue_delete(bp, vnic_idx);
		}
	}
	return 0;
}

int32_t
bnxt_vnic_rss_action_alloc(struct bnxt *bp,
				   struct bnxt_vnic_rss_info *rss_info,
				   uint16_t *vnic_idx,
				   uint16_t *vnicid)
{
	struct bnxt_vnic_info *vnic_info = NULL;
	int32_t rc = -EINVAL;
	int32_t idx;

	/* validate the given parameters */
	rc = bnxt_vnic_queue_db_rss_validate(bp, rss_info, &idx);
	if (rc == -EINVAL) {
		PMD_DRV_LOG(ERR, "Failed to apply the rss action.\n");
		return rc;
	} else if (rc == -ENOENT) {
		/* Allocate a new entry */
		idx = bnxt_vnic_queue_db_add(bp, rss_info->queue_list);
		if (idx < 0) {
			PMD_DRV_LOG(DEBUG, "Unable to alloc vnic for rss\n");
			return rc;
		}
		/* create the rss vnic */
		vnic_info = bnxt_vnic_rss_create(bp, rss_info, idx);
		if (!vnic_info) {
			PMD_DRV_LOG(ERR, "Failed to create rss action.\n");
			bnxt_vnic_queue_db_del(bp, rss_info->queue_list);
			return rc;
		}
	} else {
		vnic_info = bnxt_vnic_queue_db_get_vnic(bp, idx);
		if (vnic_info == NULL) {
			PMD_DRV_LOG(ERR, "Unable to lookup vnic for idx %d\n",
				    idx);
			return rc;
		}
		/* increment the reference count and return the vnic id */
		vnic_info->ref_cnt++;

		/* check configuration has changed then update hash details */
		rc = bnxt_vnic_rss_hash_algo_update(bp, vnic_info, rss_info);
		if (rc) {
			PMD_DRV_LOG(ERR, "Failed to update the rss action.\n");
			return rc;
		}
	}
	*vnic_idx = idx;
	*vnicid = vnic_info->fw_vnic_id;
	return 0;
}

/* Delete the vnic associated with the given rss action index */
int32_t
bnxt_vnic_rss_action_free(struct bnxt *bp, uint16_t vnic_id)
{
	uint64_t bitmap[BNXT_VNIC_MAX_QUEUE_SZ_IN_64BITS];
	struct bnxt_vnic_info *vnic_info;
	int32_t rc = -EINVAL;
	uint64_t *q_list;
	int32_t idx = 0;

	/* validate the given vnic id */
	if (vnic_id >= bp->max_vnics) {
		PMD_DRV_LOG(ERR, "invalid vnic id %d\n", vnic_id);
		return rc;
	}

	/* validate vnic info */
	vnic_info = &bp->vnic_info[vnic_id];
	if (!vnic_info->rx_queue_cnt) {
		PMD_DRV_LOG(ERR, "Invalid vnic id, not using any queues\n");
		return rc;
	}

	if (vnic_info->ref_cnt) {
		vnic_info->ref_cnt--;
		if (!vnic_info->ref_cnt) {
			if (bp->vnic_queue_db.dflt_vnic_id == vnic_id) {
				/* in case of default queue, list can be
				 * changed by reta config so need a list
				 * with all queues populated.
				 */
				memset(bitmap, 0, sizeof(bitmap));
				for (idx = 0;
				      idx < bp->vnic_queue_db.num_queues;
				      idx++)
					BNXT_VNIC_BITMAP_SET(bitmap, idx);
				q_list = bitmap;
			} else {
				q_list = vnic_info->queue_bitmap;
			}
			idx  = bnxt_vnic_queue_db_del(bp, q_list);

			/* check to ensure there is no corruption */
			if (idx != vnic_id)
				PMD_DRV_LOG(ERR, "bad vnic idx %d\n", vnic_id);
			bnxt_vnic_rss_delete(bp, vnic_id);
		}
	}
	return 0;
}

int32_t
bnxt_vnic_reta_config_update(struct bnxt *bp,
				     struct bnxt_vnic_info *vnic_info,
				     struct rte_eth_rss_reta_entry64 *reta_conf,
				     uint16_t reta_size)
{
	uint64_t l_bitmap[BNXT_VNIC_MAX_QUEUE_SZ_IN_64BITS] = {0};
	uint16_t i, sft, idx;
	uint16_t q_id;

	for (i = 0; i < reta_size; i++) {
		idx = i / RTE_ETH_RETA_GROUP_SIZE;
		sft = i % RTE_ETH_RETA_GROUP_SIZE;

		if (!(reta_conf[idx].mask & (1ULL << sft)))
			continue;

		q_id = reta_conf[idx].reta[sft];
		if (q_id >= bp->vnic_queue_db.num_queues ||
		    !bp->eth_dev->data->rx_queues[q_id]) {
			PMD_DRV_LOG(ERR, "Queue id %d is invalid\n", q_id);
			return -EINVAL;
		}
		BNXT_VNIC_BITMAP_SET(l_bitmap, q_id);
	}
	/* update the queue bitmap after the validation */
	memcpy(vnic_info->queue_bitmap, l_bitmap, sizeof(l_bitmap));
	return 0;
}

int32_t
bnxt_vnic_queue_id_is_valid(struct bnxt_vnic_info *vnic_info,
				    uint16_t queue_id)
{
	if (BNXT_VNIC_BITMAP_GET(vnic_info->queue_bitmap, queue_id))
		return 1;
	return 0;
}

void
bnxt_vnic_ring_grp_populate(struct bnxt *bp, struct bnxt_vnic_info *vnic)
{
	uint32_t i;

	/* check if ring group is supported */
	if (!BNXT_HAS_RING_GRPS(bp))
		return;

	/* map ring groups to this vnic */
	for (i = vnic->start_grp_id; i < vnic->end_grp_id; i++)
		if (bnxt_vnic_queue_id_is_valid(vnic, i) &&
			bp->rx_queues[i]->rx_started)
			vnic->fw_grp_ids[i] = bp->grp_info[i].fw_grp_id;

	vnic->dflt_ring_grp = bp->grp_info[vnic->start_grp_id].fw_grp_id;
}

void
bnxt_vnic_rules_init(struct bnxt_vnic_info *vnic)
{
	vnic->rss_rule = (uint16_t)HWRM_NA_SIGNATURE;
	vnic->cos_rule = (uint16_t)HWRM_NA_SIGNATURE;
	vnic->lb_rule = (uint16_t)HWRM_NA_SIGNATURE;
}

int32_t
bnxt_vnic_mru_config(struct bnxt *bp, uint16_t new_mtu)
{
	struct bnxt_vnic_info *vnic;
	uint16_t size = 0;
	int32_t rc = 0;
	uint32_t i;

	for (i = 0; i < bp->max_vnics; i++) {
		vnic = &bp->vnic_info[i];
		if (vnic->fw_vnic_id == INVALID_VNIC_ID)
			continue;

		vnic->mru = BNXT_VNIC_MRU(new_mtu);
		rc = bnxt_hwrm_vnic_cfg(bp, vnic);
		if (rc)
			break;

		size = rte_pktmbuf_data_room_size(bp->rx_queues[0]->mb_pool);
		size -= RTE_PKTMBUF_HEADROOM;

		if (size < new_mtu) {
			rc = bnxt_hwrm_vnic_plcmode_cfg(bp, vnic);
			if (rc)
				break;
		}
	}
	return rc;
}

struct bnxt_vnic_info *
bnxt_vnic_queue_db_get_vnic(struct bnxt *bp, uint16_t vnic_idx)
{
	struct bnxt_vnic_info *vnic_info;

	if (vnic_idx >= bp->max_vnics) {
		PMD_DRV_LOG(ERR, "invalid vnic index %u\n", vnic_idx);
		return NULL;
	}
	vnic_info = &bp->vnic_info[vnic_idx];
	return vnic_info;
}

struct bnxt_vnic_info *
bnxt_vnic_queue_id_get_next(struct bnxt *bp, uint16_t queue_id,
			    uint16_t *vnic_idx)
{
	struct bnxt_vnic_info *vnic = NULL;
	uint16_t i = *vnic_idx;

	while (i < bp->max_vnics) {
		vnic = &bp->vnic_info[i];
		if (vnic->ref_cnt && BNXT_VNIC_BITMAP_GET(vnic->queue_bitmap,
							  queue_id)) {
			/* found a vnic that has the queue id */
			*vnic_idx = i;
			return vnic;
		}
		i++;
	}
	return NULL;
}

void
bnxt_vnic_tpa_cfg(struct bnxt *bp, uint16_t queue_id, bool flag)
{
	struct bnxt_vnic_info *vnic = NULL;
	uint16_t vnic_idx = 0;

	while ((vnic = bnxt_vnic_queue_id_get_next(bp, queue_id,
						   &vnic_idx)) != NULL) {
		bnxt_hwrm_vnic_tpa_cfg(bp, vnic, flag);
		vnic_idx++;
	}
}

inline struct bnxt_vnic_info *
bnxt_get_default_vnic(struct bnxt *bp)
{
	return &bp->vnic_info[bp->vnic_queue_db.dflt_vnic_id];
}
