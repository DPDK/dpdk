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

static void prandom_bytes(void *dest_ptr, size_t len)
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

void bnxt_init_vnics(struct bnxt *bp)
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

		prandom_bytes(vnic->rss_hash_key, HW_HASH_KEY_SIZE);
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
	struct bnxt_vnic_info *temp;
	unsigned int i;

	for (i = 0; i < bp->nr_vnics; i++) {
		temp = &bp->vnic_info[i];
		STAILQ_INSERT_TAIL(&bp->free_vnic_list, temp, next);
	}
}

void bnxt_free_vnic_attributes(struct bnxt *bp)
{
	struct bnxt_vnic_info *vnic;
	unsigned int i;

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
	uint32_t entry_length = RTE_CACHE_LINE_ROUNDUP(
				HW_HASH_INDEX_SIZE * sizeof(*vnic->rss_table) +
				HW_HASH_KEY_SIZE +
				BNXT_MAX_MC_ADDRS * ETHER_ADDR_LEN);
	uint16_t max_vnics;
	int i;
	rte_iova_t mz_phys_addr;

	max_vnics = bp->max_vnics;
	snprintf(mz_name, RTE_MEMZONE_NAMESIZE,
		 "bnxt_%04x:%02x:%02x:%02x_vnicattr", pdev->addr.domain,
		 pdev->addr.bus, pdev->addr.devid, pdev->addr.function);
	mz_name[RTE_MEMZONE_NAMESIZE - 1] = 0;
	mz = rte_memzone_lookup(mz_name);
	if (!mz) {
		mz = rte_memzone_reserve(mz_name,
				entry_length * max_vnics, SOCKET_ID_ANY,
				RTE_MEMZONE_2MB |
				RTE_MEMZONE_SIZE_HINT_ONLY |
				RTE_MEMZONE_IOVA_CONTIG);
		if (!mz)
			return -ENOMEM;
	}
	mz_phys_addr = mz->iova;
	if ((unsigned long)mz->addr == mz_phys_addr) {
		PMD_DRV_LOG(WARNING,
			"Memzone physical address same as virtual.\n");
		PMD_DRV_LOG(WARNING,
			"Using rte_mem_virt2iova()\n");
		mz_phys_addr = rte_mem_virt2iova(mz->addr);
		if (mz_phys_addr == 0) {
			PMD_DRV_LOG(ERR,
			"unable to map vnic address to physical memory\n");
			return -ENOMEM;
		}
	}

	for (i = 0; i < max_vnics; i++) {
		vnic = &bp->vnic_info[i];

		/* Allocate rss table and hash key */
		vnic->rss_table =
			(void *)((char *)mz->addr + (entry_length * i));
		memset(vnic->rss_table, -1, entry_length);

		vnic->rss_table_dma_addr = mz_phys_addr + (entry_length * i);
		vnic->rss_hash_key = (void *)((char *)vnic->rss_table +
			     HW_HASH_INDEX_SIZE * sizeof(*vnic->rss_table));

		vnic->rss_hash_key_dma_addr = vnic->rss_table_dma_addr +
			     HW_HASH_INDEX_SIZE * sizeof(*vnic->rss_table);
		vnic->mc_list = (void *)((char *)vnic->rss_hash_key +
				HW_HASH_KEY_SIZE);
		vnic->mc_list_dma_addr = vnic->rss_hash_key_dma_addr +
				HW_HASH_KEY_SIZE;
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
	return 0;
}
