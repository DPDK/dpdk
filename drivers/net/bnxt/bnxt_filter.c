/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2018 Broadcom
 * All rights reserved.
 */

#include <sys/queue.h>

#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_flow.h>
#include <rte_flow_driver.h>
#include <rte_tailq.h>

#include "bnxt.h"
#include "bnxt_filter.h"
#include "bnxt_hwrm.h"
#include "bnxt_vnic.h"
#include "hsi_struct_def_dpdk.h"

/*
 * Filter Functions
 */

struct bnxt_filter_info *bnxt_alloc_filter(struct bnxt *bp)
{
	struct bnxt_filter_info *filter;

	filter = bnxt_get_unused_filter(bp);
	if (!filter) {
		PMD_DRV_LOG(ERR, "No more free filter resources\n");
		return NULL;
	}

	filter->mac_index = INVALID_MAC_INDEX;
	/* Default to L2 MAC Addr filter */
	filter->flags = HWRM_CFA_L2_FILTER_ALLOC_INPUT_FLAGS_PATH_RX;
	filter->enables = HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_ADDR |
			HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_ADDR_MASK;
	memcpy(filter->l2_addr, bp->mac_addr, RTE_ETHER_ADDR_LEN);
	memset(filter->l2_addr_mask, 0xff, RTE_ETHER_ADDR_LEN);

	return filter;
}

struct bnxt_filter_info *bnxt_alloc_vf_filter(struct bnxt *bp, uint16_t vf)
{
	struct bnxt_filter_info *filter;

	filter = rte_zmalloc("bnxt_vf_filter_info", sizeof(*filter), 0);
	if (!filter) {
		PMD_DRV_LOG(ERR, "Failed to alloc memory for VF %hu filters\n",
			vf);
		return NULL;
	}

	filter->fw_l2_filter_id = UINT64_MAX;
	STAILQ_INSERT_TAIL(&bp->pf->vf_info[vf].filter, filter, next);
	return filter;
}

static void bnxt_init_filters(struct bnxt *bp)
{
	struct bnxt_filter_info *filter;
	int i, max_filters;

	max_filters = bp->max_l2_ctx;
	STAILQ_INIT(&bp->free_filter_list);
	for (i = 0; i < max_filters; i++) {
		filter = &bp->filter_info[i];
		filter->fw_l2_filter_id = UINT64_MAX;
		filter->fw_em_filter_id = UINT64_MAX;
		filter->fw_ntuple_filter_id = UINT64_MAX;
		STAILQ_INSERT_TAIL(&bp->free_filter_list, filter, next);
	}
}

void bnxt_free_all_filters(struct bnxt *bp)
{
	struct bnxt_vnic_info *vnic;
	struct bnxt_filter_info *filter, *temp_filter;
	unsigned int i;

	for (i = 0; i < bp->nr_vnics; i++) {
		vnic = &bp->vnic_info[i];
		filter = STAILQ_FIRST(&vnic->filter);
		while (filter) {
			temp_filter = STAILQ_NEXT(filter, next);
			STAILQ_REMOVE(&vnic->filter, filter,
					bnxt_filter_info, next);
			STAILQ_INSERT_TAIL(&bp->free_filter_list,
					filter, next);
			filter = temp_filter;
		}
		STAILQ_INIT(&vnic->filter);
	}

	for (i = 0; i < bp->pf->max_vfs; i++) {
		STAILQ_FOREACH(filter, &bp->pf->vf_info[i].filter, next) {
			bnxt_hwrm_clear_l2_filter(bp, filter);
		}
	}
}

void bnxt_free_filter_mem(struct bnxt *bp)
{
	struct bnxt_filter_info *filter;
	uint16_t max_filters, i;
	int rc = 0;

	if (bp->filter_info == NULL)
		return;

	/* Ensure that all filters are freed */
	max_filters = bp->max_l2_ctx;
	for (i = 0; i < max_filters; i++) {
		filter = &bp->filter_info[i];
		if (filter->fw_ntuple_filter_id != ((uint64_t)-1) &&
		    filter->filter_type == HWRM_CFA_NTUPLE_FILTER) {
			/* Call HWRM to try to free filter again */
			rc = bnxt_hwrm_clear_ntuple_filter(bp, filter);
			if (rc)
				PMD_DRV_LOG(ERR,
					    "Cannot free ntuple filter: %d\n",
					    rc);
		}
		filter->fw_ntuple_filter_id = UINT64_MAX;

		if (filter->fw_l2_filter_id != ((uint64_t)-1) &&
		    filter->filter_type == HWRM_CFA_L2_FILTER) {
			PMD_DRV_LOG(DEBUG, "L2 filter is not free\n");
			/* Call HWRM to try to free filter again */
			rc = bnxt_hwrm_clear_l2_filter(bp, filter);
			if (rc)
				PMD_DRV_LOG(ERR,
					    "Cannot free L2 filter: %d\n",
					    rc);
		}
		filter->fw_l2_filter_id = UINT64_MAX;

	}
	STAILQ_INIT(&bp->free_filter_list);

	rte_free(bp->filter_info);
	bp->filter_info = NULL;

	for (i = 0; i < bp->pf->max_vfs; i++) {
		STAILQ_FOREACH(filter, &bp->pf->vf_info[i].filter, next) {
			rte_free(filter);
			STAILQ_REMOVE(&bp->pf->vf_info[i].filter, filter,
				      bnxt_filter_info, next);
		}
	}
}

int bnxt_alloc_filter_mem(struct bnxt *bp)
{
	struct bnxt_filter_info *filter_mem;
	uint16_t max_filters;

	max_filters = bp->max_l2_ctx;
	/* Allocate memory for VNIC pool and filter pool */
	filter_mem = rte_zmalloc("bnxt_filter_info",
				 max_filters * sizeof(struct bnxt_filter_info),
				 0);
	if (filter_mem == NULL) {
		PMD_DRV_LOG(ERR, "Failed to alloc memory for %d filters",
			max_filters);
		return -ENOMEM;
	}
	bp->filter_info = filter_mem;
	bnxt_init_filters(bp);
	return 0;
}

struct bnxt_filter_info *bnxt_get_unused_filter(struct bnxt *bp)
{
	struct bnxt_filter_info *filter;

	/* Find the 1st unused filter from the free_filter_list pool*/
	filter = STAILQ_FIRST(&bp->free_filter_list);
	if (!filter) {
		PMD_DRV_LOG(ERR, "No more free filter resources\n");
		return NULL;
	}
	STAILQ_REMOVE_HEAD(&bp->free_filter_list, next);

	return filter;
}

void bnxt_free_filter(struct bnxt *bp, struct bnxt_filter_info *filter)
{
	memset(filter, 0, sizeof(*filter));
	filter->mac_index = INVALID_MAC_INDEX;
	filter->fw_l2_filter_id = UINT64_MAX;
	filter->fw_ntuple_filter_id = UINT64_MAX;
	filter->fw_em_filter_id = UINT64_MAX;
	STAILQ_INSERT_TAIL(&bp->free_filter_list, filter, next);
}
