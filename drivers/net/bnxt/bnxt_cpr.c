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
		/* FALLTHROUGH */
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

int bnxt_event_hwrm_resp_handler(struct bnxt *bp, struct cmpl_base *cmp)
{
	bool evt = 0;

	if (bp == NULL || cmp == NULL) {
		PMD_DRV_LOG(ERR, "invalid NULL argument\n");
		return evt;
	}

	switch (CMP_TYPE(cmp)) {
	case CMPL_BASE_TYPE_HWRM_ASYNC_EVENT:
		/* Handle any async event */
		bnxt_handle_async_event(bp, cmp);
		evt = 1;
		break;
	case CMPL_BASE_TYPE_HWRM_FWD_RESP:
		/* Handle HWRM forwarded responses */
		bnxt_handle_fwd_req(bp, cmp);
		evt = 1;
		break;
	default:
		/* Ignore any other events */
		PMD_DRV_LOG(INFO, "Ignoring %02x completion\n", CMP_TYPE(cmp));
		break;
	}

	return evt;
}
