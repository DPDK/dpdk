/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2018 Broadcom
 * All rights reserved.
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
		bnxt_link_update_op(bp->eth_dev, 1);
		break;
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_PF_DRVR_UNLOAD:
		PMD_DRV_LOG(INFO, "Async event: PF driver unloaded\n");
		break;
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_VF_CFG_CHANGE:
		PMD_DRV_LOG(INFO, "Async event: VF config changed\n");
		break;
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_PORT_CONN_NOT_ALLOWED:
		PMD_DRV_LOG(INFO, "Port conn async event\n");
		break;
	default:
		PMD_DRV_LOG(INFO, "handle_async_event id = 0x%x\n", event_id);
		break;
	}
}

void bnxt_handle_fwd_req(struct bnxt *bp, struct cmpl_base *cmpl)
{
	struct hwrm_exec_fwd_resp_input *fwreq;
	struct hwrm_fwd_req_cmpl *fwd_cmpl = (struct hwrm_fwd_req_cmpl *)cmpl;
	struct input *fwd_cmd;
	uint16_t fw_vf_id;
	uint16_t vf_id;
	uint16_t req_len;
	int rc;

	if (bp->pf.active_vfs <= 0) {
		PMD_DRV_LOG(ERR, "Forwarded VF with no active VFs\n");
		return;
	}

	/* Qualify the fwd request */
	fw_vf_id = rte_le_to_cpu_16(fwd_cmpl->source_id);
	vf_id = fw_vf_id - bp->pf.first_vf_id;

	req_len = (rte_le_to_cpu_16(fwd_cmpl->req_len_type) &
		   HWRM_FWD_REQ_CMPL_REQ_LEN_MASK) >>
		HWRM_FWD_REQ_CMPL_REQ_LEN_SFT;
	if (req_len > sizeof(fwreq->encap_request))
		req_len = sizeof(fwreq->encap_request);

	/* Locate VF's forwarded command */
	fwd_cmd = (struct input *)bp->pf.vf_info[vf_id].req_buf;

	if (fw_vf_id < bp->pf.first_vf_id ||
	    fw_vf_id >= (bp->pf.first_vf_id) + bp->pf.active_vfs) {
		PMD_DRV_LOG(ERR,
		"FWD req's source_id 0x%x out of range 0x%x - 0x%x (%d %d)\n",
			fw_vf_id, bp->pf.first_vf_id,
			(bp->pf.first_vf_id) + bp->pf.active_vfs - 1,
			bp->pf.first_vf_id, bp->pf.active_vfs);
		goto reject;
	}

	if (bnxt_rcv_msg_from_vf(bp, vf_id, fwd_cmd) == true) {
		/*
		 * In older firmware versions, the MAC had to be all zeros for
		 * the VF to set it's MAC via hwrm_func_vf_cfg. Set to all
		 * zeros if it's being configured and has been ok'd by caller.
		 */
		if (fwd_cmd->req_type == HWRM_FUNC_VF_CFG) {
			struct hwrm_func_vf_cfg_input *vfc = (void *)fwd_cmd;

			if (vfc->enables &
			    HWRM_FUNC_VF_CFG_INPUT_ENABLES_DFLT_MAC_ADDR) {
				bnxt_hwrm_func_vf_mac(bp, vf_id,
				(const uint8_t *)"\x00\x00\x00\x00\x00");
			}
		}
		if (fwd_cmd->req_type == HWRM_CFA_L2_SET_RX_MASK) {
			struct hwrm_cfa_l2_set_rx_mask_input *srm =
							(void *)fwd_cmd;

			srm->vlan_tag_tbl_addr = rte_cpu_to_le_64(0);
			srm->num_vlan_tags = rte_cpu_to_le_32(0);
			srm->mask &= ~rte_cpu_to_le_32(
				HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_VLANONLY |
			    HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_VLAN_NONVLAN |
			    HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_ANYVLAN_NONVLAN);
		}
		/* Forward */
		rc = bnxt_hwrm_exec_fwd_resp(bp, fw_vf_id, fwd_cmd, req_len);
		if (rc) {
			PMD_DRV_LOG(ERR,
				"Failed to send FWD req VF 0x%x, type 0x%x.\n",
				fw_vf_id - bp->pf.first_vf_id,
				rte_le_to_cpu_16(fwd_cmd->req_type));
		}
		return;
	}

reject:
	rc = bnxt_hwrm_reject_fwd_resp(bp, fw_vf_id, fwd_cmd, req_len);
	if (rc) {
		PMD_DRV_LOG(ERR,
			"Failed to send REJECT req VF 0x%x, type 0x%x.\n",
			fw_vf_id - bp->pf.first_vf_id,
			rte_le_to_cpu_16(fwd_cmd->req_type));
	}

	return;
}

/* For the default completion ring only */
int bnxt_alloc_def_cp_ring(struct bnxt *bp)
{
	struct bnxt_cp_ring_info *cpr = bp->def_cp_ring;
	struct bnxt_ring *cp_ring = cpr->cp_ring_struct;
	int rc;

	rc = bnxt_hwrm_ring_alloc(bp, cp_ring,
				  HWRM_RING_ALLOC_INPUT_RING_TYPE_L2_CMPL,
				  0, HWRM_NA_SIGNATURE,
				  HWRM_NA_SIGNATURE);
	if (rc)
		goto err_out;
	cpr->cp_doorbell = bp->pdev->mem_resource[2].addr;
	B_CP_DIS_DB(cpr, cpr->cp_raw_cons);
	if (BNXT_PF(bp))
		rc = bnxt_hwrm_func_cfg_def_cp(bp);
	else
		rc = bnxt_hwrm_vf_func_cfg_def_cp(bp);

err_out:
	return rc;
}

void bnxt_free_def_cp_ring(struct bnxt *bp)
{
	struct bnxt_cp_ring_info *cpr = bp->def_cp_ring;

	if (cpr == NULL)
		return;

	bnxt_free_ring(cpr->cp_ring_struct);
	cpr->cp_ring_struct = NULL;
	rte_free(cpr->cp_ring_struct);
	rte_free(cpr);
	bp->def_cp_ring = NULL;
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
