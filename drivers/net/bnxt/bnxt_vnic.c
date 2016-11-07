/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2014-2015 Broadcom Corporation.
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
	int i, j;

	if (BNXT_PF(bp)) {
		struct bnxt_pf_info *pf = &bp->pf;

		max_vnics = pf->max_vnics;
	} else {
		struct bnxt_vf_info *vf = &bp->vf;

		max_vnics = vf->max_vnics;
	}
	STAILQ_INIT(&bp->free_vnic_list);
	for (i = 0; i < max_vnics; i++) {
		vnic = &bp->vnic_info[i];
		vnic->fw_vnic_id = (uint16_t)HWRM_NA_SIGNATURE;
		vnic->fw_rss_cos_lb_ctx = (uint16_t)HWRM_NA_SIGNATURE;
		vnic->ctx_is_rss_cos_lb = HW_CONTEXT_NONE;

		for (j = 0; j < MAX_QUEUES_PER_VNIC; j++)
			vnic->fw_grp_ids[j] = (uint16_t)HWRM_NA_SIGNATURE;

		prandom_bytes(vnic->rss_hash_key, HW_HASH_KEY_SIZE);
		STAILQ_INIT(&vnic->filter);
		STAILQ_INSERT_TAIL(&bp->free_vnic_list, vnic, next);
	}
	for (i = 0; i < MAX_FF_POOLS; i++)
		STAILQ_INIT(&bp->ff_pool[i]);
}

int bnxt_free_vnic(struct bnxt *bp, struct bnxt_vnic_info *vnic,
			  int pool)
{
	struct bnxt_vnic_info *temp;

	temp = STAILQ_FIRST(&bp->ff_pool[pool]);
	while (temp) {
		if (temp == vnic) {
			STAILQ_REMOVE(&bp->ff_pool[pool], vnic,
				      bnxt_vnic_info, next);
			vnic->fw_vnic_id = (uint16_t)HWRM_NA_SIGNATURE;
			STAILQ_INSERT_TAIL(&bp->free_vnic_list, vnic,
					   next);
			return 0;
		}
		temp = STAILQ_NEXT(temp, next);
	}
	RTE_LOG(ERR, PMD, "VNIC %p is not found in pool[%d]\n", vnic, pool);
	return -EINVAL;
}

struct bnxt_vnic_info *bnxt_alloc_vnic(struct bnxt *bp)
{
	struct bnxt_vnic_info *vnic;

	/* Find the 1st unused vnic from the free_vnic_list pool*/
	vnic = STAILQ_FIRST(&bp->free_vnic_list);
	if (!vnic) {
		RTE_LOG(ERR, PMD, "No more free VNIC resources\n");
		return NULL;
	}
	STAILQ_REMOVE_HEAD(&bp->free_vnic_list, next);
	return vnic;
}

void bnxt_free_all_vnics(struct bnxt *bp)
{
	struct bnxt_vnic_info *temp, *next;
	int i;

	for (i = 0; i < MAX_FF_POOLS; i++) {
		temp = STAILQ_FIRST(&bp->ff_pool[i]);
		while (temp) {
			next = STAILQ_NEXT(temp, next);
			STAILQ_REMOVE(&bp->ff_pool[i], temp, bnxt_vnic_info,
				      next);
			STAILQ_INSERT_TAIL(&bp->free_vnic_list, temp, next);
			temp = next;
		}
	}
}

void bnxt_free_vnic_attributes(struct bnxt *bp)
{
	struct bnxt_vnic_info *vnic;

	STAILQ_FOREACH(vnic, &bp->free_vnic_list, next) {
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
				HW_HASH_KEY_SIZE);
	uint16_t max_vnics;
	int i;

	if (BNXT_PF(bp)) {
		struct bnxt_pf_info *pf = &bp->pf;

		max_vnics = pf->max_vnics;
	} else {
		struct bnxt_vf_info *vf = &bp->vf;

		max_vnics = vf->max_vnics;
	}
	snprintf(mz_name, RTE_MEMZONE_NAMESIZE,
		 "bnxt_%04x:%02x:%02x:%02x_vnicattr", pdev->addr.domain,
		 pdev->addr.bus, pdev->addr.devid, pdev->addr.function);
	mz_name[RTE_MEMZONE_NAMESIZE - 1] = 0;
	mz = rte_memzone_lookup(mz_name);
	if (!mz) {
		mz = rte_memzone_reserve(mz_name,
					 entry_length * max_vnics,
					 SOCKET_ID_ANY,
					 RTE_MEMZONE_2MB |
					 RTE_MEMZONE_SIZE_HINT_ONLY);
		if (!mz)
			return -ENOMEM;
	}

	for (i = 0; i < max_vnics; i++) {
		vnic = &bp->vnic_info[i];

		/* Allocate rss table and hash key */
		vnic->rss_table =
			(void *)((char *)mz->addr + (entry_length * i));
		memset(vnic->rss_table, -1, entry_length);

		vnic->rss_table_dma_addr = mz->phys_addr + (entry_length * i);
		vnic->rss_hash_key = (void *)((char *)vnic->rss_table +
			     HW_HASH_INDEX_SIZE * sizeof(*vnic->rss_table));

		vnic->rss_hash_key_dma_addr = vnic->rss_table_dma_addr +
			     HW_HASH_INDEX_SIZE * sizeof(*vnic->rss_table);
	}

	return 0;
}

void bnxt_free_vnic_mem(struct bnxt *bp)
{
	struct bnxt_vnic_info *vnic;
	uint16_t max_vnics, i;

	if (bp->vnic_info == NULL)
		return;

	if (BNXT_PF(bp)) {
		struct bnxt_pf_info *pf = &bp->pf;

		max_vnics = pf->max_vnics;
	} else {
		struct bnxt_vf_info *vf = &bp->vf;

		max_vnics = vf->max_vnics;
	}
	for (i = 0; i < max_vnics; i++) {
		vnic = &bp->vnic_info[i];
		if (vnic->fw_vnic_id != (uint16_t)HWRM_NA_SIGNATURE) {
			RTE_LOG(ERR, PMD, "VNIC is not freed yet!\n");
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

	if (BNXT_PF(bp)) {
		struct bnxt_pf_info *pf = &bp->pf;

		max_vnics = pf->max_vnics;
	} else {
		struct bnxt_vf_info *vf = &bp->vf;

		max_vnics = vf->max_vnics;
	}
	/* Allocate memory for VNIC pool and filter pool */
	vnic_mem = rte_zmalloc("bnxt_vnic_info",
			       max_vnics * sizeof(struct bnxt_vnic_info), 0);
	if (vnic_mem == NULL) {
		RTE_LOG(ERR, PMD, "Failed to alloc memory for %d VNICs",
			max_vnics);
		return -ENOMEM;
	}
	bp->vnic_info = vnic_mem;
	return 0;
}
