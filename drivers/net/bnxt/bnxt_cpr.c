/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2023 Broadcom
 * All rights reserved.
 */

#include <rte_malloc.h>
#include <rte_alarm.h>
#include <rte_cycles.h>

#include "bnxt.h"
#include "bnxt_hwrm.h"
#include "bnxt_ring.h"
#include "hsi_struct_def_dpdk.h"
#include "tfc_vf2pf_msg.h"

void bnxt_wait_for_device_shutdown(struct bnxt *bp)
{
	uint32_t val, timeout;

	/* if HWRM_FUNC_QCAPS_OUTPUT_FLAGS_ERR_RECOVER_RELOAD is set
	 * in HWRM_FUNC_QCAPS command, wait for FW_STATUS to set
	 * the SHUTDOWN bit in health register
	 */
	if (!(bp->recovery_info &&
	      (bp->fw_cap & BNXT_FW_CAP_ERR_RECOVER_RELOAD)))
		return;

	/* Driver has to wait for fw_reset_max_msecs or shutdown bit which comes
	 * first for FW to collect crash dump.
	 */
	timeout = bp->fw_reset_max_msecs;

	/* Driver has to poll for shutdown bit in fw_status register
	 *
	 * 1. in case of hot fw upgrade, this bit will be set after all
	 *    function drivers unregistered with fw.
	 * 2. in case of fw initiated error recovery, this bit will be
	 *    set after fw has collected the core dump
	 */
	do {
		val = bnxt_read_fw_status_reg(bp, BNXT_FW_STATUS_REG);
		if (val & BNXT_FW_STATUS_SHUTDOWN)
			return;

		rte_delay_ms(100);
		timeout -= 100;
	} while (timeout);
}

static void bnxt_handle_event_error_report(struct bnxt *bp,
					   uint32_t data1,
					   uint32_t data2)
{
	switch (BNXT_EVENT_ERROR_REPORT_TYPE(data1)) {
	case HWRM_ASYNC_EVENT_CMPL_ERROR_REPORT_BASE_EVENT_DATA1_ERROR_TYPE_PAUSE_STORM:
		PMD_DRV_LOG_LINE(WARNING, "Port:%d Pause Storm detected!",
			    bp->eth_dev->data->port_id);
		break;
	case HWRM_ASYNC_EVENT_CMPL_ERROR_REPORT_BASE_EVENT_DATA1_ERROR_TYPE_DUAL_DATA_RATE_NOT_SUPPORTED:
		PMD_DRV_LOG_LINE(WARNING, "Port:%d Speed change not supported with dual rate transceivers on this board",
			    bp->eth_dev->data->port_id);
		break;
	default:
		PMD_DRV_LOG_LINE(INFO, "FW reported unknown error type data1 %d"
			    " data2: %d", data1, data2);
		break;
	}
}

void bnxt_handle_vf_cfg_change(void *arg)
{
	struct bnxt *bp = arg;
	struct rte_eth_dev *eth_dev = bp->eth_dev;
	int rc;

	/* Free and recreate filters with default VLAN */
	if (eth_dev->data->dev_started) {
		rc = bnxt_dev_stop_op(eth_dev);
		if (rc != 0) {
			PMD_DRV_LOG_LINE(ERR, "Failed to stop Port:%u", eth_dev->data->port_id);
			return;
		}

		rc = bnxt_dev_start_op(eth_dev);
		if (rc != 0)
			PMD_DRV_LOG_LINE(ERR, "Failed to start Port:%u", eth_dev->data->port_id);
	}
}

static void
bnxt_process_vf_flr(struct bnxt *bp, uint32_t data1)
{
	uint16_t pfid, vfid;
	int rc;

	if (!BNXT_TRUFLOW_EN(bp))
		return;

	pfid = (data1 & HWRM_ASYNC_EVENT_CMPL_VF_FLR_EVENT_DATA1_PF_ID_MASK) >>
		HWRM_ASYNC_EVENT_CMPL_VF_FLR_EVENT_DATA1_PF_ID_SFT;
	vfid = (data1 & HWRM_ASYNC_EVENT_CMPL_VF_FLR_EVENT_DATA1_VF_ID_MASK) >>
		HWRM_ASYNC_EVENT_CMPL_VF_FLR_EVENT_DATA1_VF_ID_SFT;

	PMD_DRV_LOG_LINE(INFO, "VF FLR async event received pfid: %u, vfid: %u",
			 pfid, vfid);

	rc = tfc_tbl_scope_func_reset(&bp->tfcp, vfid);
	if (rc != 0)
		PMD_DRV_LOG_LINE(ERR, "Failed to reset vf");
}

/*
 * Async event handling
 */
void bnxt_handle_async_event(struct bnxt *bp,
			     struct cmpl_base *cmp)
{
	struct hwrm_async_event_cmpl *async_cmp =
				(struct hwrm_async_event_cmpl *)cmp;
	uint16_t event_id = rte_le_to_cpu_16(async_cmp->event_id);
	uint16_t port_id = bp->eth_dev->data->port_id;
	struct bnxt_error_recovery_info *info;
	uint32_t event_data;
	uint32_t data1, data2;
	uint32_t status;

	data1 = rte_le_to_cpu_32(async_cmp->event_data1);
	data2 = rte_le_to_cpu_32(async_cmp->event_data2);

	switch (event_id) {
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_LINK_STATUS_CHANGE:
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_LINK_SPEED_CHANGE:
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_LINK_SPEED_CFG_CHANGE:
		/* FALLTHROUGH */
		bnxt_link_update_op(bp->eth_dev, 0);
		rte_eth_dev_callback_process(bp->eth_dev,
			RTE_ETH_EVENT_INTR_LSC, NULL);
		break;
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_PF_DRVR_UNLOAD:
		PMD_DRV_LOG_LINE(INFO, "Async event: PF driver unloaded");
		break;
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_VF_CFG_CHANGE:
		PMD_DRV_LOG_LINE(INFO, "Port %u: VF config change async event", port_id);
		PMD_DRV_LOG_LINE(INFO, "event: data1 %#x data2 %#x", data1, data2);
		bnxt_hwrm_func_qcfg(bp, NULL);
		if (BNXT_VF(bp))
			rte_eal_alarm_set(1, bnxt_handle_vf_cfg_change, (void *)bp);
		break;
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_PORT_CONN_NOT_ALLOWED:
		PMD_DRV_LOG_LINE(INFO, "Port conn async event");
		break;
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_RESET_NOTIFY:
		/*
		 * Avoid any rx/tx packet processing during firmware reset
		 * operation.
		 */
		bnxt_stop_rxtx(bp->eth_dev);

		/* Ignore reset notify async events when stopping the port */
		if (!bp->eth_dev->data->dev_started) {
			bp->flags |= BNXT_FLAG_FATAL_ERROR;
			return;
		}

		rte_eth_dev_callback_process(bp->eth_dev,
					     RTE_ETH_EVENT_ERR_RECOVERING,
					     NULL);

		pthread_mutex_lock(&bp->err_recovery_lock);
		event_data = data1;
		/* timestamp_lo/hi values are in units of 100ms */
		bp->fw_reset_max_msecs = async_cmp->timestamp_hi ?
			rte_le_to_cpu_16(async_cmp->timestamp_hi) * 100 :
			BNXT_MAX_FW_RESET_TIMEOUT;
		bp->fw_reset_min_msecs = async_cmp->timestamp_lo ?
			async_cmp->timestamp_lo * 100 :
			BNXT_MIN_FW_READY_TIMEOUT;
		if ((event_data & EVENT_DATA1_REASON_CODE_MASK) ==
		    EVENT_DATA1_REASON_CODE_FW_EXCEPTION_FATAL) {
			PMD_DRV_LOG_LINE(INFO,
				    "Port %u: Firmware fatal reset event received",
				    port_id);
			bp->flags |= BNXT_FLAG_FATAL_ERROR;
		} else {
			PMD_DRV_LOG_LINE(INFO,
				    "Port %u: Firmware non-fatal reset event received",
				    port_id);
		}

		bp->flags |= BNXT_FLAG_FW_RESET;
		pthread_mutex_unlock(&bp->err_recovery_lock);
		rte_eal_alarm_set(US_PER_MS, bnxt_dev_reset_and_resume,
				  (void *)bp);
		break;
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_ERROR_RECOVERY:
		info = bp->recovery_info;

		if (!info)
			return;

		event_data = data1 & EVENT_DATA1_FLAGS_MASK;

		if (event_data & EVENT_DATA1_FLAGS_RECOVERY_ENABLED) {
			info->flags |= BNXT_FLAG_RECOVERY_ENABLED;
		} else {
			info->flags &= ~BNXT_FLAG_RECOVERY_ENABLED;
			PMD_DRV_LOG_LINE(INFO, "Driver recovery watchdog is disabled");
			return;
		}

		if (event_data & EVENT_DATA1_FLAGS_MASTER_FUNC)
			info->flags |= BNXT_FLAG_PRIMARY_FUNC;
		else
			info->flags &= ~BNXT_FLAG_PRIMARY_FUNC;

		status = bnxt_read_fw_status_reg(bp, BNXT_FW_STATUS_REG);
		PMD_DRV_LOG_LINE(INFO,
			    "Port: %u Driver recovery watchdog, role: %s, FW status: 0x%x (%s)",
			    port_id, bnxt_is_primary_func(bp) ? "primary" : "backup", status,
			    (status == BNXT_FW_STATUS_HEALTHY) ? "healthy" : "unhealthy");

		if (bp->flags & BNXT_FLAG_FW_HEALTH_CHECK_SCHEDULED)
			return;

		info->last_heart_beat =
			bnxt_read_fw_status_reg(bp, BNXT_FW_HEARTBEAT_CNT_REG);
		info->last_reset_counter =
			bnxt_read_fw_status_reg(bp, BNXT_FW_RECOVERY_CNT_REG);

		bnxt_schedule_fw_health_check(bp);
		break;
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_DEBUG_NOTIFICATION:
		PMD_DRV_LOG_LINE(INFO, "Port: %u DNC event: data1 %#x data2 %#x",
			    port_id, data1, data2);
		break;
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_ECHO_REQUEST:
		PMD_DRV_LOG_LINE(INFO,
			    "Port %u: Received fw echo request: data1 %#x data2 %#x",
			    port_id, data1, data2);
		if (bp->recovery_info)
			bnxt_hwrm_fw_echo_reply(bp, data1, data2);
		break;
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_ERROR_REPORT:
		bnxt_handle_event_error_report(bp, data1, data2);
		break;
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_VF_FLR:
		bnxt_process_vf_flr(bp, data1);
		break;
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_RSS_CHANGE:
		/* RSS change notification, re-read QCAPS */
		PMD_DRV_LOG_LINE(INFO, "Async event: RSS change event [%#x, %#x]",
				 data1, data2);
		bnxt_hwrm_vnic_qcaps(bp);
		break;
	default:
		PMD_DRV_LOG_LINE(DEBUG, "handle_async_event id = 0x%x", event_id);
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

	if (bp->pf->active_vfs <= 0) {
		PMD_DRV_LOG_LINE(ERR, "Forwarded VF with no active VFs");
		return;
	}

	/* Qualify the fwd request */
	fw_vf_id = rte_le_to_cpu_16(fwd_cmpl->source_id);
	vf_id = fw_vf_id - bp->pf->first_vf_id;

	req_len = (rte_le_to_cpu_16(fwd_cmpl->req_len_type) &
		   HWRM_FWD_REQ_CMPL_REQ_LEN_MASK) >>
		HWRM_FWD_REQ_CMPL_REQ_LEN_SFT;
	if (req_len > sizeof(fwreq->encap_request))
		req_len = sizeof(fwreq->encap_request);

	/* Locate VF's forwarded command */
	fwd_cmd = (struct input *)bp->pf->vf_info[vf_id].req_buf;

	if (fw_vf_id < bp->pf->first_vf_id ||
	    fw_vf_id >= bp->pf->first_vf_id + bp->pf->active_vfs) {
		PMD_DRV_LOG_LINE(ERR,
		"FWD req's source_id 0x%x out of range 0x%x - 0x%x (%d %d)",
			fw_vf_id, bp->pf->first_vf_id,
			(bp->pf->first_vf_id) + bp->pf->active_vfs - 1,
			bp->pf->first_vf_id, bp->pf->active_vfs);
		goto reject;
	}

	if (bnxt_rcv_msg_from_vf(bp, vf_id, fwd_cmd)) {
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

		if (fwd_cmd->req_type == HWRM_OEM_CMD) {
			struct hwrm_oem_cmd_input *oem_cmd = (void *)fwd_cmd;
			struct hwrm_oem_cmd_output oem_out = { 0 };

			if (oem_cmd->oem_id == 0x14e4 &&
			    oem_cmd->naming_authority
				== HWRM_OEM_CMD_INPUT_NAMING_AUTHORITY_PCI_SIG &&
			    oem_cmd->message_family
				== HWRM_OEM_CMD_INPUT_MESSAGE_FAMILY_TRUFLOW) {
				uint32_t resp[18] = { 0 };
				uint16_t oem_data_len = sizeof(oem_out.oem_data);
				uint16_t resp_len = oem_data_len;

				rc = tfc_oem_cmd_process(&bp->tfcp,
							 oem_cmd->oem_data,
							 resp,
							 &resp_len);
				if (rc) {
					PMD_DRV_LOG_LINE(ERR,
						"OEM cmd process error id 0x%x, name 0x%x, family 0x%x",
						oem_cmd->oem_id,
						oem_cmd->naming_authority,
						oem_cmd->message_family);
					goto reject;
				}

				oem_out.error_code = 0;
				oem_out.req_type = oem_cmd->req_type;
				oem_out.seq_id = oem_cmd->seq_id;
				oem_out.resp_len = rte_cpu_to_le_16(sizeof(oem_out));
				oem_out.oem_id = oem_cmd->oem_id;
				oem_out.naming_authority = oem_cmd->naming_authority;
				oem_out.message_family = oem_cmd->message_family;
				memcpy(oem_out.oem_data, resp, resp_len);
				oem_out.valid = 1;

				rc = bnxt_hwrm_fwd_resp(bp, fw_vf_id, &oem_out, oem_out.resp_len,
						oem_cmd->resp_addr, oem_cmd->cmpl_ring);
				if (rc) {
					PMD_DRV_LOG_LINE(ERR,
							 "Failed to send HWRM_FWD_RESP VF 0x%x, type",
							 fw_vf_id - bp->pf->first_vf_id);
				}
			} else {
				PMD_DRV_LOG_LINE(ERR,
						 "Unsupported OEM cmd id 0x%x, name 0x%x, family 0x%x",
						 oem_cmd->oem_id, oem_cmd->naming_authority,
						 oem_cmd->message_family);
				goto reject;
			}

			return;
		}

		/* Forward */
		rc = bnxt_hwrm_exec_fwd_resp(bp, fw_vf_id, fwd_cmd, req_len);
		if (rc) {
			PMD_DRV_LOG_LINE(ERR,
				"Failed to send FWD req VF 0x%x, type 0x%x.",
				fw_vf_id - bp->pf->first_vf_id,
				rte_le_to_cpu_16(fwd_cmd->req_type));
		}
		return;
	}

reject:
	rc = bnxt_hwrm_reject_fwd_resp(bp, fw_vf_id, fwd_cmd, req_len);
	if (rc) {
		PMD_DRV_LOG_LINE(ERR,
			"Failed to send REJECT req VF 0x%x, type 0x%x.",
			fw_vf_id - bp->pf->first_vf_id,
			rte_le_to_cpu_16(fwd_cmd->req_type));
	}

	return;
}

int bnxt_event_hwrm_resp_handler(struct bnxt *bp, struct cmpl_base *cmp)
{
	bool evt = 0;

	if (bp == NULL || cmp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "invalid NULL argument");
		return evt;
	}

	if (unlikely(is_bnxt_in_error(bp)))
		return 0;

	switch (CMP_TYPE(cmp)) {
	case CMPL_BASE_TYPE_HWRM_ASYNC_EVENT:
		/* Handle any async event */
		bnxt_handle_async_event(bp, cmp);
		evt = 1;
		break;
	case CMPL_BASE_TYPE_HWRM_FWD_REQ:
		/* Handle HWRM forwarded responses */
		bnxt_handle_fwd_req(bp, cmp);
		evt = 1;
		break;
	default:
		/* Ignore any other events */
		PMD_DRV_LOG_LINE(DEBUG, "Ignoring %02x completion", CMP_TYPE(cmp));
		break;
	}

	return evt;
}

bool bnxt_is_primary_func(struct bnxt *bp)
{
	if (bp->recovery_info->flags & BNXT_FLAG_PRIMARY_FUNC)
		return true;

	return false;
}

bool bnxt_is_recovery_enabled(struct bnxt *bp)
{
	struct bnxt_error_recovery_info *info;

	info = bp->recovery_info;
	if (info && (info->flags & BNXT_FLAG_RECOVERY_ENABLED))
		return true;

	return false;
}

void bnxt_stop_rxtx(struct rte_eth_dev *eth_dev)
{
	eth_dev->rx_pkt_burst = rte_eth_pkt_burst_dummy;
	eth_dev->tx_pkt_burst = rte_eth_pkt_burst_dummy;

	rte_eth_fp_ops[eth_dev->data->port_id].rx_pkt_burst =
		eth_dev->rx_pkt_burst;
	rte_eth_fp_ops[eth_dev->data->port_id].tx_pkt_burst =
		eth_dev->tx_pkt_burst;
	rte_mb();

	/* Allow time for threads to exit the real burst functions. */
	rte_delay_ms(100);
}
