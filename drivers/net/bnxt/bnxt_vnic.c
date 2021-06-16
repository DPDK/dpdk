/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2018 Broadcom
 * All rights reserved.
 */

#include <inttypes.h>

#include <rte_memzone.h>
#include <rte_malloc.h>

#include "bnxt.h"
#include "bnxt_vnic.h"
#include "hsi_struct_def_dpdk.h"

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
		if (vnic->rss_table) {
			/* 'Unreserve' the rss_table */
			/* N/A */

			vnic->rss_table = NULL;
		}

		if (vnic->rss_hash_key) {
			/* 'Unreserve' the rss_hash_key */
			/* N/A */

			vnic->rss_hash_key = NULL;
		}
	}
}

int bnxt_alloc_vnic_attributes(struct bnxt *bp)
{
	struct bnxt_vnic_info *vnic;
	struct rte_pci_device *pdev = bp->pdev;
	const struct rte_memzone *mz;
	char mz_name[RTE_MEMZONE_NAMESIZE];
	uint32_t entry_length;
	size_t rss_table_size;
	uint16_t max_vnics;
	int i;
	rte_iova_t mz_phys_addr;

	entry_length = HW_HASH_KEY_SIZE +
		       BNXT_MAX_MC_ADDRS * RTE_ETHER_ADDR_LEN;

	if (BNXT_CHIP_THOR(bp))
		rss_table_size = BNXT_RSS_TBL_SIZE_THOR *
				 2 * sizeof(*vnic->rss_table);
	else
		rss_table_size = HW_HASH_INDEX_SIZE * sizeof(*vnic->rss_table);

	entry_length = RTE_CACHE_LINE_ROUNDUP(entry_length + rss_table_size);

	max_vnics = bp->max_vnics;
	snprintf(mz_name, RTE_MEMZONE_NAMESIZE,
		 "bnxt_" PCI_PRI_FMT "_vnicattr", pdev->addr.domain,
		 pdev->addr.bus, pdev->addr.devid, pdev->addr.function);
	mz_name[RTE_MEMZONE_NAMESIZE - 1] = 0;
	mz = rte_memzone_lookup(mz_name);
	if (!mz) {
		mz = rte_memzone_reserve(mz_name,
				entry_length * max_vnics,
				bp->eth_dev->device->numa_node,
				RTE_MEMZONE_2MB |
				RTE_MEMZONE_SIZE_HINT_ONLY |
				RTE_MEMZONE_IOVA_CONTIG);
		if (!mz)
			return -ENOMEM;
	}
	mz_phys_addr = mz->iova;

	for (i = 0; i < max_vnics; i++) {
		vnic = &bp->vnic_info[i];

		/* Allocate rss table and hash key */
		vnic->rss_table =
			(void *)((char *)mz->addr + (entry_length * i));
		memset(vnic->rss_table, -1, entry_length);

		vnic->rss_table_dma_addr = mz_phys_addr + (entry_length * i);
		vnic->rss_hash_key = (void *)((char *)vnic->rss_table +
					      rss_table_size);

		vnic->rss_hash_key_dma_addr = vnic->rss_table_dma_addr +
					      rss_table_size;
		vnic->mc_list = (void *)((char *)vnic->rss_hash_key +
				HW_HASH_KEY_SIZE);
		vnic->mc_list_dma_addr = vnic->rss_hash_key_dma_addr +
				HW_HASH_KEY_SIZE;
		bnxt_prandom_bytes(vnic->rss_hash_key, HW_HASH_KEY_SIZE);
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

	vnic->fw_grp_ids = rte_zmalloc("vnic_fw_grp_ids", size, 0);
	if (!vnic->fw_grp_ids) {
		PMD_DRV_LOG(ERR,
			    "Failed to alloc %d bytes for group ids\n",
			    size);
		return -ENOMEM;
	}
	memset(vnic->fw_grp_ids, -1, size);

	return 0;
}

uint16_t bnxt_rte_to_hwrm_hash_types(uint64_t rte_type)
{
	uint16_t hwrm_type = 0;

	if (rte_type & ETH_RSS_IPV4)
		hwrm_type |= HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_IPV4;
	if (rte_type & ETH_RSS_NONFRAG_IPV4_TCP)
		hwrm_type |= HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_TCP_IPV4;
	if (rte_type & ETH_RSS_NONFRAG_IPV4_UDP)
		hwrm_type |= HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_UDP_IPV4;
	if (rte_type & ETH_RSS_IPV6)
		hwrm_type |= HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_IPV6;
	if (rte_type & ETH_RSS_NONFRAG_IPV6_TCP)
		hwrm_type |= HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_TCP_IPV6;
	if (rte_type & ETH_RSS_NONFRAG_IPV6_UDP)
		hwrm_type |= HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_UDP_IPV6;

	return hwrm_type;
}

int bnxt_rte_to_hwrm_hash_level(struct bnxt *bp, uint64_t hash_f, uint32_t lvl)
{
	uint32_t mode = HWRM_VNIC_RSS_CFG_INPUT_HASH_MODE_FLAGS_DEFAULT;
	bool l3 = (hash_f & (ETH_RSS_IPV4 | ETH_RSS_IPV6));
	bool l4 = (hash_f & (ETH_RSS_NONFRAG_IPV4_UDP |
			     ETH_RSS_NONFRAG_IPV6_UDP |
			     ETH_RSS_NONFRAG_IPV4_TCP |
			     ETH_RSS_NONFRAG_IPV6_TCP));
	bool l3_only = l3 && !l4;
	bool l3_and_l4 = l3 && l4;

	/* If FW has not advertised capability to configure outer/inner
	 * RSS hashing , just log a message. HW will work in default RSS mode.
	 */
	if (!(bp->vnic_cap_flags & BNXT_VNIC_CAP_OUTER_RSS)) {
		PMD_DRV_LOG(ERR, "RSS hash level cannot be configured\n");
		return mode;
	}

	switch (lvl) {
	case BNXT_RSS_LEVEL_INNERMOST:
		if (l3_and_l4 || l4)
			mode =
			HWRM_VNIC_RSS_CFG_INPUT_HASH_MODE_FLAGS_INNERMOST_4;
		else if (l3_only)
			mode =
			HWRM_VNIC_RSS_CFG_INPUT_HASH_MODE_FLAGS_INNERMOST_2;
		break;
	case BNXT_RSS_LEVEL_OUTERMOST:
		if (l3_and_l4 || l4)
			mode =
			HWRM_VNIC_RSS_CFG_INPUT_HASH_MODE_FLAGS_OUTERMOST_4;
		else if (l3_only)
			mode =
			HWRM_VNIC_RSS_CFG_INPUT_HASH_MODE_FLAGS_OUTERMOST_2;
		break;
	default:
		mode = HWRM_VNIC_RSS_CFG_INPUT_HASH_MODE_FLAGS_DEFAULT;
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
	if (!(bp->vnic_cap_flags & BNXT_VNIC_CAP_OUTER_RSS))
		return ETH_RSS_LEVEL_PMD_DEFAULT;

	if (mode == HWRM_VNIC_RSS_CFG_INPUT_HASH_MODE_FLAGS_OUTERMOST_2 ||
	    mode == HWRM_VNIC_RSS_CFG_INPUT_HASH_MODE_FLAGS_OUTERMOST_4)
		rss_level |= ETH_RSS_LEVEL_OUTERMOST;
	else if (mode == HWRM_VNIC_RSS_CFG_INPUT_HASH_MODE_FLAGS_INNERMOST_2 ||
		 mode == HWRM_VNIC_RSS_CFG_INPUT_HASH_MODE_FLAGS_INNERMOST_4)
		rss_level |= ETH_RSS_LEVEL_INNERMOST;
	else
		rss_level |= ETH_RSS_LEVEL_PMD_DEFAULT;

	return rss_level;
}
