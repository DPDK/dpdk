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

#include <sys/queue.h>

#include <rte_log.h>
#include <rte_malloc.h>

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

	/* Find the 1st unused filter from the free_filter_list pool*/
	filter = STAILQ_FIRST(&bp->free_filter_list);
	if (!filter) {
		RTE_LOG(ERR, PMD, "No more free filter resources\n");
		return NULL;
	}
	STAILQ_REMOVE_HEAD(&bp->free_filter_list, next);

	/* Default to L2 MAC Addr filter */
	filter->flags = HWRM_CFA_L2_FILTER_ALLOC_INPUT_FLAGS_PATH_RX;
	filter->enables = HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_ADDR |
			HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_ADDR_MASK;
	memcpy(filter->l2_addr, bp->eth_dev->data->mac_addrs->addr_bytes,
	       ETHER_ADDR_LEN);
	memset(filter->l2_addr_mask, 0xff, ETHER_ADDR_LEN);
	return filter;
}

void bnxt_init_filters(struct bnxt *bp)
{
	struct bnxt_filter_info *filter;
	int i, max_filters;

	if (BNXT_PF(bp)) {
		struct bnxt_pf_info *pf = &bp->pf;

		max_filters = pf->max_l2_ctx;
	} else {
		struct bnxt_vf_info *vf = &bp->vf;

		max_filters = vf->max_l2_ctx;
	}
	STAILQ_INIT(&bp->free_filter_list);
	for (i = 0; i < max_filters; i++) {
		filter = &bp->filter_info[i];
		filter->fw_l2_filter_id = -1;
		STAILQ_INSERT_TAIL(&bp->free_filter_list, filter, next);
	}
}

void bnxt_free_all_filters(struct bnxt *bp)
{
	struct bnxt_vnic_info *vnic;
	struct bnxt_filter_info *filter, *temp_filter;
	int i;

	for (i = 0; i < MAX_FF_POOLS; i++) {
		STAILQ_FOREACH(vnic, &bp->ff_pool[i], next) {
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
	if (BNXT_PF(bp)) {
		struct bnxt_pf_info *pf = &bp->pf;

		max_filters = pf->max_l2_ctx;
	} else {
		struct bnxt_vf_info *vf = &bp->vf;

		max_filters = vf->max_l2_ctx;
	}
	for (i = 0; i < max_filters; i++) {
		filter = &bp->filter_info[i];
		if (filter->fw_l2_filter_id != ((uint64_t)-1)) {
			RTE_LOG(ERR, PMD, "HWRM filter is not freed??\n");
			/* Call HWRM to try to free filter again */
			rc = bnxt_hwrm_clear_filter(bp, filter);
			if (rc)
				RTE_LOG(ERR, PMD,
				       "HWRM filter cannot be freed rc = %d\n",
					rc);
		}
		filter->fw_l2_filter_id = -1;
	}
	STAILQ_INIT(&bp->free_filter_list);

	rte_free(bp->filter_info);
	bp->filter_info = NULL;
}

int bnxt_alloc_filter_mem(struct bnxt *bp)
{
	struct bnxt_filter_info *filter_mem;
	uint16_t max_filters;

	if (BNXT_PF(bp)) {
		struct bnxt_pf_info *pf = &bp->pf;

		max_filters = pf->max_l2_ctx;
	} else {
		struct bnxt_vf_info *vf = &bp->vf;

		max_filters = vf->max_l2_ctx;
	}
	/* Allocate memory for VNIC pool and filter pool */
	filter_mem = rte_zmalloc("bnxt_filter_info",
				 max_filters * sizeof(struct bnxt_filter_info),
				 0);
	if (filter_mem == NULL) {
		RTE_LOG(ERR, PMD, "Failed to alloc memory for %d filters",
			max_filters);
		return -ENOMEM;
	}
	bp->filter_info = filter_mem;
	return 0;
}
