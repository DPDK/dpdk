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

#include <rte_malloc.h>

#include "bnxt.h"
#include "bnxt_cpr.h"
#include "bnxt_hwrm.h"
#include "bnxt_ring.h"
#include "hsi_struct_def_dpdk.h"

/*
 * Async event handling
 */
void bnxt_handle_async_event(struct bnxt *bp,
			     struct cmpl_base *cmp)
{
	struct hwrm_async_event_cmpl *async_cmp =
				(struct hwrm_async_event_cmpl *)cmp;
	uint16_t event_id = rte_le_to_cpu_16(async_cmp->event_id);

	/* TODO: HWRM async events are not defined yet */
	/* Needs to handle: link events, error events, etc. */
	switch (event_id) {
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_LINK_STATUS_CHANGE:
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_LINK_SPEED_CHANGE:
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_LINK_SPEED_CFG_CHANGE:
		bnxt_link_update_op(bp->eth_dev, 0);
		break;
	default:
		RTE_LOG(ERR, PMD, "handle_async_event id = 0x%x\n", event_id);
		break;
	}
}

void bnxt_handle_fwd_req(struct bnxt *bp, struct cmpl_base *cmpl)
{
	struct hwrm_fwd_req_cmpl *fwd_cmpl = (struct hwrm_fwd_req_cmpl *)cmpl;
	struct input *fwd_cmd;
	uint16_t logical_vf_id, error_code;

	/* Qualify the fwd request */
	if (fwd_cmpl->source_id < bp->pf.first_vf_id) {
		RTE_LOG(ERR, PMD,
			"FWD req's source_id 0x%x > first_vf_id 0x%x\n",
			fwd_cmpl->source_id, bp->pf.first_vf_id);
		error_code = HWRM_ERR_CODE_RESOURCE_ACCESS_DENIED;
		goto reject;
	} else if (fwd_cmpl->req_len_type >> HWRM_FWD_REQ_CMPL_REQ_LEN_SFT >
		   128 - sizeof(struct input)) {
		RTE_LOG(ERR, PMD,
		    "FWD req's cmd len 0x%x > 108 bytes allowed\n",
		    fwd_cmpl->req_len_type >> HWRM_FWD_REQ_CMPL_REQ_LEN_SFT);
		error_code = HWRM_ERR_CODE_INVALID_PARAMS;
		goto reject;
	}

	/* Locate VF's forwarded command */
	logical_vf_id = fwd_cmpl->source_id - bp->pf.first_vf_id;
	fwd_cmd = (struct input *)((uint8_t *)bp->pf.vf_req_buf +
		   (logical_vf_id * 128));

	/* Provision the request */
	switch (fwd_cmd->req_type) {
	case HWRM_CFA_L2_FILTER_ALLOC:
	case HWRM_CFA_L2_FILTER_FREE:
	case HWRM_CFA_L2_FILTER_CFG:
	case HWRM_CFA_L2_SET_RX_MASK:
		break;
	default:
		error_code = HWRM_ERR_CODE_INVALID_PARAMS;
		goto reject;
	}

	/* Forward */
	fwd_cmd->target_id = fwd_cmpl->source_id;
	bnxt_hwrm_exec_fwd_resp(bp, fwd_cmd);
	return;

reject:
	/* TODO: Encap the reject error resp into the hwrm_err_iput? */
	/* Use the error_code for the reject cmd */
	RTE_LOG(ERR, PMD,
		"Error 0x%x found in the forward request\n", error_code);
}

/* For the default completion ring only */
void bnxt_free_def_cp_ring(struct bnxt *bp)
{
	struct bnxt_cp_ring_info *cpr = bp->def_cp_ring;

	if (cpr == NULL)
		return;

	bnxt_free_ring(cpr->cp_ring_struct);
	rte_free(cpr->cp_ring_struct);
	rte_free(cpr);
}

/* For the default completion ring only */
int bnxt_init_def_ring_struct(struct bnxt *bp, unsigned int socket_id)
{
	struct bnxt_cp_ring_info *cpr;
	struct bnxt_ring *ring;

	cpr = rte_zmalloc_socket("cpr",
				 sizeof(struct bnxt_cp_ring_info),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (cpr == NULL)
		return -ENOMEM;
	bp->def_cp_ring = cpr;

	ring = rte_zmalloc_socket("bnxt_cp_ring_struct",
				  sizeof(struct bnxt_ring),
				  RTE_CACHE_LINE_SIZE, socket_id);
	if (ring == NULL)
		return -ENOMEM;
	cpr->cp_ring_struct = ring;
	ring->bd = (void *)cpr->cp_desc_ring;
	ring->bd_dma = cpr->cp_desc_mapping;
	ring->ring_size = rte_align32pow2(DEFAULT_CP_RING_SIZE);
	ring->ring_mask = ring->ring_size - 1;
	ring->vmem_size = 0;
	ring->vmem = NULL;

	return 0;
}
