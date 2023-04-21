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

static void
bnxt_process_default_vnic_change(struct bnxt *bp,
				 struct hwrm_async_event_cmpl *async_cmp)
{
	uint16_t vnic_state, vf_fid, vf_id;
	struct bnxt_representor *vf_rep_bp;
	struct rte_eth_dev *eth_dev;
	bool vfr_found = false;
	uint32_t event_data;

	if (!BNXT_TRUFLOW_EN(bp))
		return;

	PMD_DRV_LOG(INFO, "Default vnic change async event received\n");
	event_data = rte_le_to_cpu_32(async_cmp->event_data1);

	vnic_state = (event_data & BNXT_DEFAULT_VNIC_STATE_MASK) >>
			BNXT_DEFAULT_VNIC_STATE_SFT;
	if (vnic_state != BNXT_DEFAULT_VNIC_ALLOC)
		return;

	if (!bp->rep_info)
		return;

	vf_fid = (event_data & BNXT_DEFAULT_VNIC_CHANGE_VF_ID_MASK) >>
			BNXT_DEFAULT_VNIC_CHANGE_VF_ID_SFT;
	PMD_DRV_LOG(INFO, "async event received vf_id 0x%x\n", vf_fid);

	for (vf_id = 0; vf_id < BNXT_MAX_VF_REPS(bp); vf_id++) {
		eth_dev = bp->rep_info[vf_id].vfr_eth_dev;
		if (!eth_dev)
			continue;
		vf_rep_bp = eth_dev->data->dev_private;
		if (vf_rep_bp &&
		    vf_rep_bp->fw_fid == vf_fid) {
			vfr_found = true;
			break;
		}
	}
	if (!vfr_found)
		return;

	bnxt_rep_dev_start_op(eth_dev);
}

static void bnxt_handle_event_error_report(struct bnxt *bp,
					   uint32_t data1,
					   uint32_t data2)
{
	switch (BNXT_EVENT_ERROR_REPORT_TYPE(data1)) {
	case HWRM_ASYNC_EVENT_CMPL_ERROR_REPORT_BASE_EVENT_DATA1_ERROR_TYPE_PAUSE_STORM:
		PMD_DRV_LOG(WARNING, "Port:%d Pause Storm detected!\n",
			    bp->eth_dev->data->port_id);
		break;
	default:
		PMD_DRV_LOG(INFO, "FW reported unknown error type data1 %d"
			    " data2: %d\n", data1, data2);
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
			PMD_DRV_LOG(ERR, "Failed to stop Port:%u\n", eth_dev->data->port_id);
			return;
		}

		rc = bnxt_dev_start_op(eth_dev);
		if (rc != 0)
			PMD_DRV_LOG(ERR, "Failed to start Port:%u\n", eth_dev->data->port_id);
	}
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
		PMD_DRV_LOG(INFO, "Async event: PF driver unloaded\n");
		break;
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_VF_CFG_CHANGE:
		PMD_DRV_LOG(INFO, "Port %u: VF config change async event\n", port_id);
		PMD_DRV_LOG(INFO, "event: data1 %#x data2 %#x\n", data1, data2);
		bnxt_hwrm_func_qcfg(bp, NULL);
		if (BNXT_VF(bp))
			rte_eal_alarm_set(1, bnxt_handle_vf_cfg_change, (void *)bp);
		break;
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_PORT_CONN_NOT_ALLOWED:
		PMD_DRV_LOG(INFO, "Port conn async event\n");
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
			PMD_DRV_LOG(INFO,
				    "Port %u: Firmware fatal reset event received\n",
				    port_id);
			bp->flags |= BNXT_FLAG_FATAL_ERROR;
		} else {
			PMD_DRV_LOG(INFO,
				    "Port %u: Firmware non-fatal reset event received\n",
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
			PMD_DRV_LOG(INFO, "Driver recovery watchdog is disabled\n");
			return;
		}

		if (event_data & EVENT_DATA1_FLAGS_MASTER_FUNC)
			info->flags |= BNXT_FLAG_PRIMARY_FUNC;
		else
			info->flags &= ~BNXT_FLAG_PRIMARY_FUNC;

		status = bnxt_read_fw_status_reg(bp, BNXT_FW_STATUS_REG);
		PMD_DRV_LOG(INFO,
			    "Port: %u Driver recovery watchdog, role: %s, FW status: 0x%x (%s)\n",
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
		PMD_DRV_LOG(INFO, "Port: %u DNC event: data1 %#x data2 %#x\n",
			    port_id, data1, data2);
		break;
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_DEFAULT_VNIC_CHANGE:
		bnxt_process_default_vnic_change(bp, async_cmp);
		break;
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_ECHO_REQUEST:
		PMD_DRV_LOG(INFO,
			    "Port %u: Received fw echo request: data1 %#x data2 %#x\n",
			    port_id, data1, data2);
		if (bp->recovery_info)
			bnxt_hwrm_fw_echo_reply(bp, data1, data2);
		break;
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_ERROR_REPORT:
		bnxt_handle_event_error_report(bp, data1, data2);
		break;
	default:
		PMD_DRV_LOG(DEBUG, "handle_async_event id = 0x%x\n", event_id);
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
		PMD_DRV_LOG(ERR, "Forwarded VF with no active VFs\n");
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
		PMD_DRV_LOG(ERR,
		"FWD req's source_id 0x%x out of range 0x%x - 0x%x (%d %d)\n",
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

		/* Forward */
		rc = bnxt_hwrm_exec_fwd_resp(bp, fw_vf_id, fwd_cmd, req_len);
		if (rc) {
			PMD_DRV_LOG(ERR,
				"Failed to send FWD req VF 0x%x, type 0x%x.\n",
				fw_vf_id - bp->pf->first_vf_id,
				rte_le_to_cpu_16(fwd_cmd->req_type));
		}
		return;
	}

reject:
	rc = bnxt_hwrm_reject_fwd_resp(bp, fw_vf_id, fwd_cmd, req_len);
	if (rc) {
		PMD_DRV_LOG(ERR,
			"Failed to send REJECT req VF 0x%x, type 0x%x.\n",
			fw_vf_id - bp->pf->first_vf_id,
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
		PMD_DRV_LOG(DEBUG, "Ignoring %02x completion\n", CMP_TYPE(cmp));
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
