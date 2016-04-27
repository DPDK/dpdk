/*
 * Copyright (c) 2016 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.qede_pmd for copyright and licensing details.
 */

#include "bcm_osal.h"
#include "reg_addr.h"
#include "ecore_gtt_reg_addr.h"
#include "ecore.h"
#include "ecore_chain.h"
#include "ecore_status.h"
#include "ecore_hw.h"
#include "ecore_rt_defs.h"
#include "ecore_init_ops.h"
#include "ecore_int.h"
#include "ecore_cxt.h"
#include "ecore_spq.h"
#include "ecore_init_fw_funcs.h"
#include "ecore_sp_commands.h"
#include "ecore_dev_api.h"
#include "ecore_sriov.h"
#include "ecore_vf.h"
#include "ecore_mcp.h"
#include "ecore_hw_defs.h"
#include "mcp_public.h"
#include "ecore_iro.h"
#include "nvm_cfg.h"
#include "ecore_dev_api.h"
#include "ecore_attn_values.h"
#include "ecore_dcbx.h"

/* Configurable */
#define ECORE_MIN_DPIS		(4)	/* The minimal number of DPIs required
					 * load the driver. The number was
					 * arbitrarily set.
					 */

/* Derived */
#define ECORE_MIN_PWM_REGION	((ECORE_WID_SIZE) * (ECORE_MIN_DPIS))

enum BAR_ID {
	BAR_ID_0,		/* used for GRC */
	BAR_ID_1		/* Used for doorbells */
};

static u32 ecore_hw_bar_size(struct ecore_hwfn *p_hwfn, enum BAR_ID bar_id)
{
	u32 bar_reg = (bar_id == BAR_ID_0 ?
		       PGLUE_B_REG_PF_BAR0_SIZE : PGLUE_B_REG_PF_BAR1_SIZE);
	u32 val = ecore_rd(p_hwfn, p_hwfn->p_main_ptt, bar_reg);

	/* The above registers were updated in the past only in CMT mode. Since
	 * they were found to be useful MFW started updating them from 8.7.7.0.
	 * In older MFW versions they are set to 0 which means disabled.
	 */
	if (!val) {
		if (p_hwfn->p_dev->num_hwfns > 1) {
			DP_NOTICE(p_hwfn, false,
				  "BAR size not configured. Assuming BAR"
				  " size of 256kB for GRC and 512kB for DB\n");
			return BAR_ID_0 ? 256 * 1024 : 512 * 1024;
		}

		DP_NOTICE(p_hwfn, false,
			  "BAR size not configured. Assuming BAR"
			  " size of 512kB for GRC and 512kB for DB\n");
		return 512 * 1024;
	}

	return 1 << (val + 15);
}

void ecore_init_dp(struct ecore_dev *p_dev,
		   u32 dp_module, u8 dp_level, void *dp_ctx)
{
	u32 i;

	p_dev->dp_level = dp_level;
	p_dev->dp_module = dp_module;
	p_dev->dp_ctx = dp_ctx;
	for (i = 0; i < MAX_HWFNS_PER_DEVICE; i++) {
		struct ecore_hwfn *p_hwfn = &p_dev->hwfns[i];

		p_hwfn->dp_level = dp_level;
		p_hwfn->dp_module = dp_module;
		p_hwfn->dp_ctx = dp_ctx;
	}
}

void ecore_init_struct(struct ecore_dev *p_dev)
{
	u8 i;

	for (i = 0; i < MAX_HWFNS_PER_DEVICE; i++) {
		struct ecore_hwfn *p_hwfn = &p_dev->hwfns[i];

		p_hwfn->p_dev = p_dev;
		p_hwfn->my_id = i;
		p_hwfn->b_active = false;

		OSAL_MUTEX_ALLOC(p_hwfn, &p_hwfn->dmae_info.mutex);
		OSAL_MUTEX_INIT(&p_hwfn->dmae_info.mutex);
	}

	/* hwfn 0 is always active */
	p_dev->hwfns[0].b_active = true;

	/* set the default cache alignment to 128 (may be overridden later) */
	p_dev->cache_shift = 7;
}

static void ecore_qm_info_free(struct ecore_hwfn *p_hwfn)
{
	struct ecore_qm_info *qm_info = &p_hwfn->qm_info;

	OSAL_FREE(p_hwfn->p_dev, qm_info->qm_pq_params);
	qm_info->qm_pq_params = OSAL_NULL;
	OSAL_FREE(p_hwfn->p_dev, qm_info->qm_vport_params);
	qm_info->qm_vport_params = OSAL_NULL;
	OSAL_FREE(p_hwfn->p_dev, qm_info->qm_port_params);
	qm_info->qm_port_params = OSAL_NULL;
	OSAL_FREE(p_hwfn->p_dev, qm_info->wfq_data);
	qm_info->wfq_data = OSAL_NULL;
}

void ecore_resc_free(struct ecore_dev *p_dev)
{
	int i;

	if (IS_VF(p_dev))
		return;

	OSAL_FREE(p_dev, p_dev->fw_data);
	p_dev->fw_data = OSAL_NULL;

	OSAL_FREE(p_dev, p_dev->reset_stats);

	for_each_hwfn(p_dev, i) {
		struct ecore_hwfn *p_hwfn = &p_dev->hwfns[i];

		OSAL_FREE(p_dev, p_hwfn->p_tx_cids);
		p_hwfn->p_tx_cids = OSAL_NULL;
		OSAL_FREE(p_dev, p_hwfn->p_rx_cids);
		p_hwfn->p_rx_cids = OSAL_NULL;
	}

	for_each_hwfn(p_dev, i) {
		struct ecore_hwfn *p_hwfn = &p_dev->hwfns[i];

		ecore_cxt_mngr_free(p_hwfn);
		ecore_qm_info_free(p_hwfn);
		ecore_spq_free(p_hwfn);
		ecore_eq_free(p_hwfn, p_hwfn->p_eq);
		ecore_consq_free(p_hwfn, p_hwfn->p_consq);
		ecore_int_free(p_hwfn);
		ecore_iov_free(p_hwfn);
		ecore_dmae_info_free(p_hwfn);
		ecore_dcbx_info_free(p_hwfn, p_hwfn->p_dcbx_info);
		/* @@@TBD Flush work-queue ? */
	}
}

static enum _ecore_status_t ecore_init_qm_info(struct ecore_hwfn *p_hwfn,
					       bool b_sleepable)
{
	u8 num_vports, vf_offset = 0, i, vport_id, num_ports;
	struct ecore_qm_info *qm_info = &p_hwfn->qm_info;
	struct init_qm_port_params *p_qm_port;
	u16 num_pqs, multi_cos_tcs = 1;
#ifdef CONFIG_ECORE_SRIOV
	u16 num_vfs = p_hwfn->p_dev->sriov_info.total_vfs;
#else
	u16 num_vfs = 0;
#endif

	OSAL_MEM_ZERO(qm_info, sizeof(*qm_info));

#ifndef ASIC_ONLY
	/* @TMP - Don't allocate QM queues for VFs on emulation */
	if (CHIP_REV_IS_EMUL(p_hwfn->p_dev)) {
		DP_NOTICE(p_hwfn, false,
			  "Emulation - skip configuring QM queues for VFs\n");
		num_vfs = 0;
	}
#endif

	num_pqs = multi_cos_tcs + num_vfs + 1;	/* The '1' is for pure-LB */
	num_vports = (u8)RESC_NUM(p_hwfn, ECORE_VPORT);

	/* Sanity checking that setup requires legal number of resources */
	if (num_pqs > RESC_NUM(p_hwfn, ECORE_PQ)) {
		DP_ERR(p_hwfn,
		       "Need too many Physical queues - 0x%04x when"
			" only %04x are available\n",
		       num_pqs, RESC_NUM(p_hwfn, ECORE_PQ));
		return ECORE_INVAL;
	}

	/* PQs will be arranged as follows: First per-TC PQ, then pure-LB queue,
	 * then special queues, then per-VF PQ.
	 */
	qm_info->qm_pq_params = OSAL_ZALLOC(p_hwfn->p_dev,
					    b_sleepable ? GFP_KERNEL :
					    GFP_ATOMIC,
					    sizeof(struct init_qm_pq_params) *
					    num_pqs);
	if (!qm_info->qm_pq_params)
		goto alloc_err;

	qm_info->qm_vport_params = OSAL_ZALLOC(p_hwfn->p_dev,
					       b_sleepable ? GFP_KERNEL :
					       GFP_ATOMIC,
					       sizeof(struct
						      init_qm_vport_params) *
					       num_vports);
	if (!qm_info->qm_vport_params)
		goto alloc_err;

	qm_info->qm_port_params = OSAL_ZALLOC(p_hwfn->p_dev,
					      b_sleepable ? GFP_KERNEL :
					      GFP_ATOMIC,
					      sizeof(struct init_qm_port_params)
					      * MAX_NUM_PORTS);
	if (!qm_info->qm_port_params)
		goto alloc_err;

	qm_info->wfq_data = OSAL_ZALLOC(p_hwfn->p_dev,
					b_sleepable ? GFP_KERNEL :
					GFP_ATOMIC,
					sizeof(struct ecore_wfq_data) *
					num_vports);

	if (!qm_info->wfq_data)
		goto alloc_err;

	vport_id = (u8)RESC_START(p_hwfn, ECORE_VPORT);

	/* First init per-TC PQs */
	for (i = 0; i < multi_cos_tcs; i++) {
		struct init_qm_pq_params *params = &qm_info->qm_pq_params[i];

		if (p_hwfn->hw_info.personality == ECORE_PCI_ETH) {
			params->vport_id = vport_id;
			params->tc_id = p_hwfn->hw_info.non_offload_tc;
			params->wrr_group = 1;	/* @@@TBD ECORE_WRR_MEDIUM */
		} else {
			params->vport_id = vport_id;
			params->tc_id = p_hwfn->hw_info.offload_tc;
			params->wrr_group = 1;	/* @@@TBD ECORE_WRR_MEDIUM */
		}
	}

	/* Then init pure-LB PQ */
	qm_info->pure_lb_pq = i;
	qm_info->qm_pq_params[i].vport_id =
	    (u8)RESC_START(p_hwfn, ECORE_VPORT);
	qm_info->qm_pq_params[i].tc_id = PURE_LB_TC;
	qm_info->qm_pq_params[i].wrr_group = 1;
	i++;

	/* Then init per-VF PQs */
	vf_offset = i;
	for (i = 0; i < num_vfs; i++) {
		/* First vport is used by the PF */
		qm_info->qm_pq_params[vf_offset + i].vport_id = vport_id +
		    i + 1;
		qm_info->qm_pq_params[vf_offset + i].tc_id =
		    p_hwfn->hw_info.non_offload_tc;
		qm_info->qm_pq_params[vf_offset + i].wrr_group = 1;
	};

	qm_info->vf_queues_offset = vf_offset;
	qm_info->num_pqs = num_pqs;
	qm_info->num_vports = num_vports;

	/* Initialize qm port parameters */
	num_ports = p_hwfn->p_dev->num_ports_in_engines;
	for (i = 0; i < num_ports; i++) {
		p_qm_port = &qm_info->qm_port_params[i];
		p_qm_port->active = 1;
		/* @@@TMP - was NUM_OF_PHYS_TCS; Changed until dcbx will
		 * be in place
		 */
		if (num_ports == 4)
			p_qm_port->num_active_phys_tcs = 2;
		else
			p_qm_port->num_active_phys_tcs = 5;
		p_qm_port->num_pbf_cmd_lines = PBF_MAX_CMD_LINES / num_ports;
		p_qm_port->num_btb_blocks = BTB_MAX_BLOCKS / num_ports;
	}

	if (ECORE_IS_AH(p_hwfn->p_dev) && (num_ports == 4))
		qm_info->max_phys_tcs_per_port = NUM_PHYS_TCS_4PORT_K2;
	else
		qm_info->max_phys_tcs_per_port = NUM_OF_PHYS_TCS;

	qm_info->start_pq = (u16)RESC_START(p_hwfn, ECORE_PQ);

	qm_info->num_vf_pqs = num_vfs;
	qm_info->start_vport = (u8)RESC_START(p_hwfn, ECORE_VPORT);

	for (i = 0; i < qm_info->num_vports; i++)
		qm_info->qm_vport_params[i].vport_wfq = 1;

	qm_info->pf_wfq = 0;
	qm_info->pf_rl = 0;
	qm_info->vport_rl_en = 1;
	qm_info->vport_wfq_en = 1;

	return ECORE_SUCCESS;

alloc_err:
	DP_NOTICE(p_hwfn, false, "Failed to allocate memory for QM params\n");
	ecore_qm_info_free(p_hwfn);
	return ECORE_NOMEM;
}

/* This function reconfigures the QM pf on the fly.
 * For this purpose we:
 * 1. reconfigure the QM database
 * 2. set new values to runtime arrat
 * 3. send an sdm_qm_cmd through the rbc interface to stop the QM
 * 4. activate init tool in QM_PF stage
 * 5. send an sdm_qm_cmd through rbc interface to release the QM
 */
enum _ecore_status_t ecore_qm_reconf(struct ecore_hwfn *p_hwfn,
				     struct ecore_ptt *p_ptt)
{
	struct ecore_qm_info *qm_info = &p_hwfn->qm_info;
	enum _ecore_status_t rc;
	bool b_rc;

	/* qm_info is allocated in ecore_init_qm_info() which is already called
	 * from ecore_resc_alloc() or previous call of ecore_qm_reconf().
	 * The allocated size may change each init, so we free it before next
	 * allocation.
	 */
	ecore_qm_info_free(p_hwfn);

	/* initialize ecore's qm data structure */
	rc = ecore_init_qm_info(p_hwfn, false);
	if (rc != ECORE_SUCCESS)
		return rc;

	/* stop PF's qm queues */
	b_rc = ecore_send_qm_stop_cmd(p_hwfn, p_ptt, false, true,
				      qm_info->start_pq, qm_info->num_pqs);
	if (!b_rc)
		return ECORE_INVAL;

	/* clear the QM_PF runtime phase leftovers from previous init */
	ecore_init_clear_rt_data(p_hwfn);

	/* prepare QM portion of runtime array */
	ecore_qm_init_pf(p_hwfn);

	/* activate init tool on runtime array */
	rc = ecore_init_run(p_hwfn, p_ptt, PHASE_QM_PF, p_hwfn->rel_pf_id,
			    p_hwfn->hw_info.hw_mode);
	if (rc != ECORE_SUCCESS)
		return rc;

	/* start PF's qm queues */
	b_rc = ecore_send_qm_stop_cmd(p_hwfn, p_ptt, true, true,
				      qm_info->start_pq, qm_info->num_pqs);
	if (!rc)
		return ECORE_INVAL;

	return ECORE_SUCCESS;
}

enum _ecore_status_t ecore_resc_alloc(struct ecore_dev *p_dev)
{
	enum _ecore_status_t rc = ECORE_SUCCESS;
	struct ecore_consq *p_consq;
	struct ecore_eq *p_eq;
	int i;

	if (IS_VF(p_dev))
		return rc;

	p_dev->fw_data = OSAL_ZALLOC(p_dev, GFP_KERNEL,
				     sizeof(struct ecore_fw_data));
	if (!p_dev->fw_data)
		return ECORE_NOMEM;

	/* Allocate Memory for the Queue->CID mapping */
	for_each_hwfn(p_dev, i) {
		struct ecore_hwfn *p_hwfn = &p_dev->hwfns[i];

		/* @@@TMP - resc management, change to actual required size */
		int tx_size = sizeof(struct ecore_hw_cid_data) *
		    RESC_NUM(p_hwfn, ECORE_L2_QUEUE);
		int rx_size = sizeof(struct ecore_hw_cid_data) *
		    RESC_NUM(p_hwfn, ECORE_L2_QUEUE);

		p_hwfn->p_tx_cids = OSAL_ZALLOC(p_hwfn->p_dev, GFP_KERNEL,
						tx_size);
		if (!p_hwfn->p_tx_cids) {
			DP_NOTICE(p_hwfn, true,
				  "Failed to allocate memory for Tx Cids\n");
			goto alloc_no_mem;
		}

		p_hwfn->p_rx_cids = OSAL_ZALLOC(p_hwfn->p_dev, GFP_KERNEL,
						rx_size);
		if (!p_hwfn->p_rx_cids) {
			DP_NOTICE(p_hwfn, true,
				  "Failed to allocate memory for Rx Cids\n");
			goto alloc_no_mem;
		}
	}

	for_each_hwfn(p_dev, i) {
		struct ecore_hwfn *p_hwfn = &p_dev->hwfns[i];

		/* First allocate the context manager structure */
		rc = ecore_cxt_mngr_alloc(p_hwfn);
		if (rc)
			goto alloc_err;

		/* Set the HW cid/tid numbers (in the contest manager)
		 * Must be done prior to any further computations.
		 */
		rc = ecore_cxt_set_pf_params(p_hwfn);
		if (rc)
			goto alloc_err;

		/* Prepare and process QM requirements */
		rc = ecore_init_qm_info(p_hwfn, true);
		if (rc)
			goto alloc_err;

		/* Compute the ILT client partition */
		rc = ecore_cxt_cfg_ilt_compute(p_hwfn);
		if (rc)
			goto alloc_err;

		/* CID map / ILT shadow table / T2
		 * The talbes sizes are determined by the computations above
		 */
		rc = ecore_cxt_tables_alloc(p_hwfn);
		if (rc)
			goto alloc_err;

		/* SPQ, must follow ILT because initializes SPQ context */
		rc = ecore_spq_alloc(p_hwfn);
		if (rc)
			goto alloc_err;

		/* SP status block allocation */
		p_hwfn->p_dpc_ptt = ecore_get_reserved_ptt(p_hwfn,
							   RESERVED_PTT_DPC);

		rc = ecore_int_alloc(p_hwfn, p_hwfn->p_main_ptt);
		if (rc)
			goto alloc_err;

		rc = ecore_iov_alloc(p_hwfn);
		if (rc)
			goto alloc_err;

		/* EQ */
		p_eq = ecore_eq_alloc(p_hwfn, 256);
		if (!p_eq)
			goto alloc_no_mem;
		p_hwfn->p_eq = p_eq;

		p_consq = ecore_consq_alloc(p_hwfn);
		if (!p_consq)
			goto alloc_no_mem;
		p_hwfn->p_consq = p_consq;

		/* DMA info initialization */
		rc = ecore_dmae_info_alloc(p_hwfn);
		if (rc) {
			DP_NOTICE(p_hwfn, true,
				  "Failed to allocate memory for"
				  " dmae_info structure\n");
			goto alloc_err;
		}

		/* DCBX initialization */
		rc = ecore_dcbx_info_alloc(p_hwfn);
		if (rc) {
			DP_NOTICE(p_hwfn, true,
				  "Failed to allocate memory for dcbxstruct\n");
			goto alloc_err;
		}
	}

	p_dev->reset_stats = OSAL_ZALLOC(p_dev, GFP_KERNEL,
					 sizeof(struct ecore_eth_stats));
	if (!p_dev->reset_stats) {
		DP_NOTICE(p_dev, true, "Failed to allocate reset statistics\n");
		goto alloc_no_mem;
	}

	return ECORE_SUCCESS;

alloc_no_mem:
	rc = ECORE_NOMEM;
alloc_err:
	ecore_resc_free(p_dev);
	return rc;
}

void ecore_resc_setup(struct ecore_dev *p_dev)
{
	int i;

	if (IS_VF(p_dev))
		return;

	for_each_hwfn(p_dev, i) {
		struct ecore_hwfn *p_hwfn = &p_dev->hwfns[i];

		ecore_cxt_mngr_setup(p_hwfn);
		ecore_spq_setup(p_hwfn);
		ecore_eq_setup(p_hwfn, p_hwfn->p_eq);
		ecore_consq_setup(p_hwfn, p_hwfn->p_consq);

		/* Read shadow of current MFW mailbox */
		ecore_mcp_read_mb(p_hwfn, p_hwfn->p_main_ptt);
		OSAL_MEMCPY(p_hwfn->mcp_info->mfw_mb_shadow,
			    p_hwfn->mcp_info->mfw_mb_cur,
			    p_hwfn->mcp_info->mfw_mb_length);

		ecore_int_setup(p_hwfn, p_hwfn->p_main_ptt);

		ecore_iov_setup(p_hwfn, p_hwfn->p_main_ptt);
	}
}

#define FINAL_CLEANUP_POLL_CNT	(100)
#define FINAL_CLEANUP_POLL_TIME	(10)
enum _ecore_status_t ecore_final_cleanup(struct ecore_hwfn *p_hwfn,
					 struct ecore_ptt *p_ptt,
					 u16 id, bool is_vf)
{
	u32 command = 0, addr, count = FINAL_CLEANUP_POLL_CNT;
	enum _ecore_status_t rc = ECORE_TIMEOUT;

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_TEDIBEAR(p_hwfn->p_dev) ||
	    CHIP_REV_IS_SLOW(p_hwfn->p_dev)) {
		DP_INFO(p_hwfn, "Skipping final cleanup for non-ASIC\n");
		return ECORE_SUCCESS;
	}
#endif

	addr = GTT_BAR0_MAP_REG_USDM_RAM +
	    USTORM_FLR_FINAL_ACK_OFFSET(p_hwfn->rel_pf_id);

	if (is_vf)
		id += 0x10;

	command |= X_FINAL_CLEANUP_AGG_INT <<
	    SDM_AGG_INT_COMP_PARAMS_AGG_INT_INDEX_SHIFT;
	command |= 1 << SDM_AGG_INT_COMP_PARAMS_AGG_VECTOR_ENABLE_SHIFT;
	command |= id << SDM_AGG_INT_COMP_PARAMS_AGG_VECTOR_BIT_SHIFT;
	command |= SDM_COMP_TYPE_AGG_INT << SDM_OP_GEN_COMP_TYPE_SHIFT;

	/* Make sure notification is not set before initiating final cleanup */
	if (REG_RD(p_hwfn, addr)) {
		DP_NOTICE(p_hwfn, false,
			  "Unexpected; Found final cleanup notification "
			  "before initiating final cleanup\n");
		REG_WR(p_hwfn, addr, 0);
	}

	DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
		   "Sending final cleanup for PFVF[%d] [Command %08x\n]",
		   id, OSAL_CPU_TO_LE32(command));

	ecore_wr(p_hwfn, p_ptt, XSDM_REG_OPERATION_GEN,
		 OSAL_CPU_TO_LE32(command));

	/* Poll until completion */
	while (!REG_RD(p_hwfn, addr) && count--)
		OSAL_MSLEEP(FINAL_CLEANUP_POLL_TIME);

	if (REG_RD(p_hwfn, addr))
		rc = ECORE_SUCCESS;
	else
		DP_NOTICE(p_hwfn, true,
			  "Failed to receive FW final cleanup notification\n");

	/* Cleanup afterwards */
	REG_WR(p_hwfn, addr, 0);

	return rc;
}

static void ecore_calc_hw_mode(struct ecore_hwfn *p_hwfn)
{
	int hw_mode = 0;

	switch (ECORE_GET_TYPE(p_hwfn->p_dev)) {
	case CHIP_BB_A0:
		hw_mode |= 1 << MODE_BB_A0;
		break;
	case CHIP_BB_B0:
		hw_mode |= 1 << MODE_BB_B0;
		break;
	case CHIP_K2:
		hw_mode |= 1 << MODE_K2;
		break;
	default:
		DP_NOTICE(p_hwfn, true, "Can't initialize chip ID %d\n",
			  ECORE_GET_TYPE(p_hwfn->p_dev));
		return;
	}

	/* Ports per engine is based on the values in CNIG_REG_NW_PORT_MODE */
	switch (p_hwfn->p_dev->num_ports_in_engines) {
	case 1:
		hw_mode |= 1 << MODE_PORTS_PER_ENG_1;
		break;
	case 2:
		hw_mode |= 1 << MODE_PORTS_PER_ENG_2;
		break;
	case 4:
		hw_mode |= 1 << MODE_PORTS_PER_ENG_4;
		break;
	default:
		DP_NOTICE(p_hwfn, true,
			  "num_ports_in_engine = %d not supported\n",
			  p_hwfn->p_dev->num_ports_in_engines);
		return;
	}

	switch (p_hwfn->p_dev->mf_mode) {
	case ECORE_MF_DEFAULT:
	case ECORE_MF_NPAR:
		hw_mode |= 1 << MODE_MF_SI;
		break;
	case ECORE_MF_OVLAN:
		hw_mode |= 1 << MODE_MF_SD;
		break;
	default:
		DP_NOTICE(p_hwfn, true,
			  "Unsupported MF mode, init as DEFAULT\n");
		hw_mode |= 1 << MODE_MF_SI;
	}

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_SLOW(p_hwfn->p_dev)) {
		if (CHIP_REV_IS_FPGA(p_hwfn->p_dev)) {
			hw_mode |= 1 << MODE_FPGA;
		} else {
			if (p_hwfn->p_dev->b_is_emul_full)
				hw_mode |= 1 << MODE_EMUL_FULL;
			else
				hw_mode |= 1 << MODE_EMUL_REDUCED;
		}
	} else
#endif
		hw_mode |= 1 << MODE_ASIC;

	if (ENABLE_EAGLE_ENG1_WORKAROUND(p_hwfn))
		hw_mode |= 1 << MODE_EAGLE_ENG1_WORKAROUND;

	if (p_hwfn->p_dev->num_hwfns > 1)
		hw_mode |= 1 << MODE_100G;

	p_hwfn->hw_info.hw_mode = hw_mode;

	DP_VERBOSE(p_hwfn, (ECORE_MSG_PROBE | ECORE_MSG_IFUP),
		   "Configuring function for hw_mode: 0x%08x\n",
		   p_hwfn->hw_info.hw_mode);
}

#ifndef ASIC_ONLY
/* MFW-replacement initializations for non-ASIC */
static void ecore_hw_init_chip(struct ecore_hwfn *p_hwfn,
			       struct ecore_ptt *p_ptt)
{
	u32 pl_hv = 1;
	int i;

	if (CHIP_REV_IS_EMUL(p_hwfn->p_dev) && ECORE_IS_AH(p_hwfn->p_dev))
		pl_hv |= 0x600;

	ecore_wr(p_hwfn, p_ptt, MISCS_REG_RESET_PL_HV + 4, pl_hv);

	if (CHIP_REV_IS_EMUL(p_hwfn->p_dev) && ECORE_IS_AH(p_hwfn->p_dev))
		ecore_wr(p_hwfn, p_ptt, MISCS_REG_RESET_PL_HV_2, 0x3ffffff);

	/* initialize interrupt masks */
	for (i = 0;
	     i <
	     attn_blocks[BLOCK_MISCS].chip_regs[ECORE_GET_TYPE(p_hwfn->p_dev)].
	     num_of_int_regs; i++)
		ecore_wr(p_hwfn, p_ptt,
			 attn_blocks[BLOCK_MISCS].
			 chip_regs[ECORE_GET_TYPE(p_hwfn->p_dev)].int_regs[i]->
			 mask_addr, 0);

	if (!CHIP_REV_IS_EMUL(p_hwfn->p_dev) || !ECORE_IS_AH(p_hwfn->p_dev))
		ecore_wr(p_hwfn, p_ptt,
			 attn_blocks[BLOCK_CNIG].
			 chip_regs[ECORE_GET_TYPE(p_hwfn->p_dev)].int_regs[0]->
			 mask_addr, 0);
	ecore_wr(p_hwfn, p_ptt,
		 attn_blocks[BLOCK_PGLCS].
		 chip_regs[ECORE_GET_TYPE(p_hwfn->p_dev)].int_regs[0]->
		 mask_addr, 0);
	ecore_wr(p_hwfn, p_ptt,
		 attn_blocks[BLOCK_CPMU].
		 chip_regs[ECORE_GET_TYPE(p_hwfn->p_dev)].int_regs[0]->
		 mask_addr, 0);
	/* Currently A0 and B0 interrupt bits are the same in pglue_b;
	 * If this changes, need to set this according to chip type. <14/09/23>
	 */
	ecore_wr(p_hwfn, p_ptt,
		 attn_blocks[BLOCK_PGLUE_B].
		 chip_regs[ECORE_GET_TYPE(p_hwfn->p_dev)].int_regs[0]->
		 mask_addr, 0x80000);

	/* initialize port mode to 4x10G_E (10G with 4x10 SERDES) */
	/* CNIG_REG_NW_PORT_MODE is same for A0 and B0 */
	if (!CHIP_REV_IS_EMUL(p_hwfn->p_dev) || !ECORE_IS_AH(p_hwfn->p_dev))
		ecore_wr(p_hwfn, p_ptt, CNIG_REG_NW_PORT_MODE_BB_B0, 4);

	if (CHIP_REV_IS_EMUL(p_hwfn->p_dev) && ECORE_IS_AH(p_hwfn->p_dev)) {
		/* 2 for 4-port, 1 for 2-port, 0 for 1-port */
		ecore_wr(p_hwfn, p_ptt, MISC_REG_PORT_MODE,
			 (p_hwfn->p_dev->num_ports_in_engines >> 1));

		ecore_wr(p_hwfn, p_ptt, MISC_REG_BLOCK_256B_EN,
			 p_hwfn->p_dev->num_ports_in_engines == 4 ? 0 : 3);
	}

	/* Poll on RBC */
	ecore_wr(p_hwfn, p_ptt, PSWRQ2_REG_RBC_DONE, 1);
	for (i = 0; i < 100; i++) {
		OSAL_UDELAY(50);
		if (ecore_rd(p_hwfn, p_ptt, PSWRQ2_REG_CFG_DONE) == 1)
			break;
	}
	if (i == 100)
		DP_NOTICE(p_hwfn, true,
			  "RBC done failed to complete in PSWRQ2\n");
}
#endif

/* Init run time data for all PFs and their VFs on an engine.
 * TBD - for VFs - Once we have parent PF info for each VF in
 * shmem available as CAU requires knowledge of parent PF for each VF.
 */
static void ecore_init_cau_rt_data(struct ecore_dev *p_dev)
{
	u32 offset = CAU_REG_SB_VAR_MEMORY_RT_OFFSET;
	int i, sb_id;

	for_each_hwfn(p_dev, i) {
		struct ecore_hwfn *p_hwfn = &p_dev->hwfns[i];
		struct ecore_igu_info *p_igu_info;
		struct ecore_igu_block *p_block;
		struct cau_sb_entry sb_entry;

		p_igu_info = p_hwfn->hw_info.p_igu_info;

		for (sb_id = 0; sb_id < ECORE_MAPPING_MEMORY_SIZE(p_dev);
		     sb_id++) {
			p_block = &p_igu_info->igu_map.igu_blocks[sb_id];

			if (!p_block->is_pf)
				continue;

			ecore_init_cau_sb_entry(p_hwfn, &sb_entry,
						p_block->function_id, 0, 0);
			STORE_RT_REG_AGG(p_hwfn, offset + sb_id * 2, sb_entry);
		}
	}
}

static enum _ecore_status_t ecore_hw_init_common(struct ecore_hwfn *p_hwfn,
						 struct ecore_ptt *p_ptt,
						 int hw_mode)
{
	struct ecore_qm_info *qm_info = &p_hwfn->qm_info;
	enum _ecore_status_t rc = ECORE_SUCCESS;
	struct ecore_dev *p_dev = p_hwfn->p_dev;
	u8 vf_id, max_num_vfs;
	u16 num_pfs, pf_id;
	u32 concrete_fid;

	ecore_init_cau_rt_data(p_dev);

	/* Program GTT windows */
	ecore_gtt_init(p_hwfn);

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_EMUL(p_hwfn->p_dev))
		ecore_hw_init_chip(p_hwfn, p_hwfn->p_main_ptt);
#endif

	if (p_hwfn->mcp_info) {
		if (p_hwfn->mcp_info->func_info.bandwidth_max)
			qm_info->pf_rl_en = 1;
		if (p_hwfn->mcp_info->func_info.bandwidth_min)
			qm_info->pf_wfq_en = 1;
	}

	ecore_qm_common_rt_init(p_hwfn,
				p_hwfn->p_dev->num_ports_in_engines,
				qm_info->max_phys_tcs_per_port,
				qm_info->pf_rl_en, qm_info->pf_wfq_en,
				qm_info->vport_rl_en, qm_info->vport_wfq_en,
				qm_info->qm_port_params);

	ecore_cxt_hw_init_common(p_hwfn);

	/* Close gate from NIG to BRB/Storm; By default they are open, but
	 * we close them to prevent NIG from passing data to reset blocks.
	 * Should have been done in the ENGINE phase, but init-tool lacks
	 * proper port-pretend capabilities.
	 */
	ecore_wr(p_hwfn, p_ptt, NIG_REG_RX_BRB_OUT_EN, 0);
	ecore_wr(p_hwfn, p_ptt, NIG_REG_STORM_OUT_EN, 0);
	ecore_port_pretend(p_hwfn, p_ptt, p_hwfn->port_id ^ 1);
	ecore_wr(p_hwfn, p_ptt, NIG_REG_RX_BRB_OUT_EN, 0);
	ecore_wr(p_hwfn, p_ptt, NIG_REG_STORM_OUT_EN, 0);
	ecore_port_unpretend(p_hwfn, p_ptt);

	rc = ecore_init_run(p_hwfn, p_ptt, PHASE_ENGINE, ANY_PHASE_ID, hw_mode);
	if (rc != ECORE_SUCCESS)
		return rc;

	/* @@TBD MichalK - should add VALIDATE_VFID to init tool...
	 * need to decide with which value, maybe runtime
	 */
	ecore_wr(p_hwfn, p_ptt, PSWRQ2_REG_L2P_VALIDATE_VFID, 0);
	ecore_wr(p_hwfn, p_ptt, PGLUE_B_REG_USE_CLIENTID_IN_TAG, 1);

	if (ECORE_IS_BB(p_hwfn->p_dev)) {
		num_pfs = NUM_OF_ENG_PFS(p_hwfn->p_dev);
		if (num_pfs == 1)
			return rc;
		/* pretend to original PF */
		ecore_fid_pretend(p_hwfn, p_ptt, p_hwfn->rel_pf_id);
	}

	/* Workaround for avoiding CCFC execution error when getting packets
	 * with CRC errors, and allowing instead the invoking of the FW error
	 * handler.
	 * This is not done inside the init tool since it currently can't
	 * perform a pretending to VFs.
	 */
	max_num_vfs = ECORE_IS_AH(p_hwfn->p_dev) ? MAX_NUM_VFS_K2
	    : MAX_NUM_VFS_BB;
	for (vf_id = 0; vf_id < max_num_vfs; vf_id++) {
		concrete_fid = ecore_vfid_to_concrete(p_hwfn, vf_id);
		ecore_fid_pretend(p_hwfn, p_ptt, (u16)concrete_fid);
		ecore_wr(p_hwfn, p_ptt, CCFC_REG_STRONG_ENABLE_VF, 0x1);
	}
	/* pretend to original PF */
	ecore_fid_pretend(p_hwfn, p_ptt, p_hwfn->rel_pf_id);

	return rc;
}

#ifndef ASIC_ONLY
#define MISC_REG_RESET_REG_2_XMAC_BIT (1 << 4)
#define MISC_REG_RESET_REG_2_XMAC_SOFT_BIT (1 << 5)

#define PMEG_IF_BYTE_COUNT	8

static void ecore_wr_nw_port(struct ecore_hwfn *p_hwfn,
			     struct ecore_ptt *p_ptt,
			     u32 addr, u64 data, u8 reg_type, u8 port)
{
	DP_VERBOSE(p_hwfn, ECORE_MSG_LINK,
		   "CMD: %08x, ADDR: 0x%08x, DATA: %08x:%08x\n",
		   ecore_rd(p_hwfn, p_ptt, CNIG_REG_PMEG_IF_CMD_BB_B0) |
		   (8 << PMEG_IF_BYTE_COUNT),
		   (reg_type << 25) | (addr << 8) | port,
		   (u32)((data >> 32) & 0xffffffff),
		   (u32)(data & 0xffffffff));

	ecore_wr(p_hwfn, p_ptt, CNIG_REG_PMEG_IF_CMD_BB_B0,
		 (ecore_rd(p_hwfn, p_ptt, CNIG_REG_PMEG_IF_CMD_BB_B0) &
		  0xffff00fe) | (8 << PMEG_IF_BYTE_COUNT));
	ecore_wr(p_hwfn, p_ptt, CNIG_REG_PMEG_IF_ADDR_BB_B0,
		 (reg_type << 25) | (addr << 8) | port);
	ecore_wr(p_hwfn, p_ptt, CNIG_REG_PMEG_IF_WRDATA_BB_B0,
		 data & 0xffffffff);
	ecore_wr(p_hwfn, p_ptt, CNIG_REG_PMEG_IF_WRDATA_BB_B0,
		 (data >> 32) & 0xffffffff);
}

#define XLPORT_MODE_REG	(0x20a)
#define XLPORT_MAC_CONTROL (0x210)
#define XLPORT_FLOW_CONTROL_CONFIG (0x207)
#define XLPORT_ENABLE_REG (0x20b)

#define XLMAC_CTRL (0x600)
#define XLMAC_MODE (0x601)
#define XLMAC_RX_MAX_SIZE (0x608)
#define XLMAC_TX_CTRL (0x604)
#define XLMAC_PAUSE_CTRL (0x60d)
#define XLMAC_PFC_CTRL (0x60e)

static void ecore_emul_link_init_ah(struct ecore_hwfn *p_hwfn,
				    struct ecore_ptt *p_ptt)
{
	u8 port = p_hwfn->port_id;
	u32 mac_base = NWM_REG_MAC0 + (port << 2) * NWM_REG_MAC0_SIZE;

	ecore_wr(p_hwfn, p_ptt, CNIG_REG_NIG_PORT0_CONF_K2 + (port << 2),
		 (1 << CNIG_REG_NIG_PORT0_CONF_NIG_PORT_ENABLE_0_SHIFT) |
		 (port << CNIG_REG_NIG_PORT0_CONF_NIG_PORT_NWM_PORT_MAP_0_SHIFT)
		 | (0 << CNIG_REG_NIG_PORT0_CONF_NIG_PORT_RATE_0_SHIFT));

	ecore_wr(p_hwfn, p_ptt, mac_base + ETH_MAC_REG_XIF_MODE,
		 1 << ETH_MAC_REG_XIF_MODE_XGMII_SHIFT);

	ecore_wr(p_hwfn, p_ptt, mac_base + ETH_MAC_REG_FRM_LENGTH,
		 9018 << ETH_MAC_REG_FRM_LENGTH_FRM_LENGTH_SHIFT);

	ecore_wr(p_hwfn, p_ptt, mac_base + ETH_MAC_REG_TX_IPG_LENGTH,
		 0xc << ETH_MAC_REG_TX_IPG_LENGTH_TXIPG_SHIFT);

	ecore_wr(p_hwfn, p_ptt, mac_base + ETH_MAC_REG_RX_FIFO_SECTIONS,
		 8 << ETH_MAC_REG_RX_FIFO_SECTIONS_RX_SECTION_FULL_SHIFT);

	ecore_wr(p_hwfn, p_ptt, mac_base + ETH_MAC_REG_TX_FIFO_SECTIONS,
		 (0xA << ETH_MAC_REG_TX_FIFO_SECTIONS_TX_SECTION_EMPTY_SHIFT) |
		 (8 << ETH_MAC_REG_TX_FIFO_SECTIONS_TX_SECTION_FULL_SHIFT));

	ecore_wr(p_hwfn, p_ptt, mac_base + ETH_MAC_REG_COMMAND_CONFIG, 0xa853);
}

static void ecore_emul_link_init(struct ecore_hwfn *p_hwfn,
				 struct ecore_ptt *p_ptt)
{
	u8 loopback = 0, port = p_hwfn->port_id * 2;

	DP_INFO(p_hwfn->p_dev, "Configurating Emulation Link %02x\n", port);

	if (ECORE_IS_AH(p_hwfn->p_dev)) {
		ecore_emul_link_init_ah(p_hwfn, p_ptt);
		return;
	}

	ecore_wr_nw_port(p_hwfn, p_ptt, XLPORT_MODE_REG, (0x4 << 4) | 0x4, 1,
				port);
	ecore_wr_nw_port(p_hwfn, p_ptt, XLPORT_MAC_CONTROL, 0, 1, port);
	ecore_wr_nw_port(p_hwfn, p_ptt, XLMAC_CTRL, 0x40, 0, port);
	ecore_wr_nw_port(p_hwfn, p_ptt, XLMAC_MODE, 0x40, 0, port);
	ecore_wr_nw_port(p_hwfn, p_ptt, XLMAC_RX_MAX_SIZE, 0x3fff, 0, port);
	ecore_wr_nw_port(p_hwfn, p_ptt, XLMAC_TX_CTRL,
			 0x01000000800ULL | (0xa << 12) | ((u64)1 << 38),
			 0, port);
	ecore_wr_nw_port(p_hwfn, p_ptt, XLMAC_PAUSE_CTRL, 0x7c000, 0, port);
	ecore_wr_nw_port(p_hwfn, p_ptt, XLMAC_PFC_CTRL,
			 0x30ffffc000ULL, 0, port);
	ecore_wr_nw_port(p_hwfn, p_ptt, XLMAC_CTRL, 0x3 | (loopback << 2), 0,
			port);
	ecore_wr_nw_port(p_hwfn, p_ptt, XLMAC_CTRL, 0x1003 | (loopback << 2),
			0, port);
	ecore_wr_nw_port(p_hwfn, p_ptt, XLPORT_FLOW_CONTROL_CONFIG, 1, 0, port);
	ecore_wr_nw_port(p_hwfn, p_ptt, XLPORT_ENABLE_REG, 0xf, 1, port);
}

static void ecore_link_init(struct ecore_hwfn *p_hwfn,
			    struct ecore_ptt *p_ptt, u8 port)
{
	int port_offset = port ? 0x800 : 0;
	u32 xmac_rxctrl = 0;

	/* Reset of XMAC */
	/* FIXME: move to common start */
	ecore_wr(p_hwfn, p_ptt, MISC_REG_RESET_PL_PDA_VAUX + 2 * sizeof(u32),
		MISC_REG_RESET_REG_2_XMAC_BIT);	/* Clear */
	OSAL_MSLEEP(1);
	ecore_wr(p_hwfn, p_ptt, MISC_REG_RESET_PL_PDA_VAUX + sizeof(u32),
		MISC_REG_RESET_REG_2_XMAC_BIT);	/* Set */

	ecore_wr(p_hwfn, p_ptt, MISC_REG_XMAC_CORE_PORT_MODE, 1);

	/* Set the number of ports on the Warp Core to 10G */
	ecore_wr(p_hwfn, p_ptt, MISC_REG_XMAC_PHY_PORT_MODE, 3);

	/* Soft reset of XMAC */
	ecore_wr(p_hwfn, p_ptt, MISC_REG_RESET_PL_PDA_VAUX + 2 * sizeof(u32),
		 MISC_REG_RESET_REG_2_XMAC_SOFT_BIT);
	OSAL_MSLEEP(1);
	ecore_wr(p_hwfn, p_ptt, MISC_REG_RESET_PL_PDA_VAUX + sizeof(u32),
		 MISC_REG_RESET_REG_2_XMAC_SOFT_BIT);

	/* FIXME: move to common end */
	if (CHIP_REV_IS_FPGA(p_hwfn->p_dev))
		ecore_wr(p_hwfn, p_ptt, XMAC_REG_MODE + port_offset, 0x20);

	/* Set Max packet size: initialize XMAC block register for port 0 */
	ecore_wr(p_hwfn, p_ptt, XMAC_REG_RX_MAX_SIZE + port_offset, 0x2710);

	/* CRC append for Tx packets: init XMAC block register for port 1 */
	ecore_wr(p_hwfn, p_ptt, XMAC_REG_TX_CTRL_LO + port_offset, 0xC800);

	/* Enable TX and RX: initialize XMAC block register for port 1 */
	ecore_wr(p_hwfn, p_ptt, XMAC_REG_CTRL + port_offset,
		 XMAC_REG_CTRL_TX_EN | XMAC_REG_CTRL_RX_EN);
	xmac_rxctrl = ecore_rd(p_hwfn, p_ptt, XMAC_REG_RX_CTRL + port_offset);
	xmac_rxctrl |= XMAC_REG_RX_CTRL_PROCESS_VARIABLE_PREAMBLE;
	ecore_wr(p_hwfn, p_ptt, XMAC_REG_RX_CTRL + port_offset, xmac_rxctrl);
}
#endif

static enum _ecore_status_t ecore_hw_init_port(struct ecore_hwfn *p_hwfn,
					       struct ecore_ptt *p_ptt,
					       int hw_mode)
{
	enum _ecore_status_t rc = ECORE_SUCCESS;

	/* Init sequence */
	rc = ecore_init_run(p_hwfn, p_ptt, PHASE_PORT, p_hwfn->port_id,
			    hw_mode);
	if (rc != ECORE_SUCCESS)
		return rc;

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_ASIC(p_hwfn->p_dev))
		return ECORE_SUCCESS;

	if (CHIP_REV_IS_FPGA(p_hwfn->p_dev)) {
		if (ECORE_IS_AH(p_hwfn->p_dev))
			return ECORE_SUCCESS;
		ecore_link_init(p_hwfn, p_ptt, p_hwfn->port_id);
	} else if (CHIP_REV_IS_EMUL(p_hwfn->p_dev)) {
		if (p_hwfn->p_dev->num_hwfns > 1) {
			/* Activate OPTE in CMT */
			u32 val;

			val = ecore_rd(p_hwfn, p_ptt, MISCS_REG_RESET_PL_HV);
			val |= 0x10;
			ecore_wr(p_hwfn, p_ptt, MISCS_REG_RESET_PL_HV, val);
			ecore_wr(p_hwfn, p_ptt, MISC_REG_CLK_100G_MODE, 1);
			ecore_wr(p_hwfn, p_ptt, MISCS_REG_CLK_100G_MODE, 1);
			ecore_wr(p_hwfn, p_ptt, MISC_REG_OPTE_MODE, 1);
			ecore_wr(p_hwfn, p_ptt,
				 NIG_REG_LLH_ENG_CLS_TCP_4_TUPLE_SEARCH, 1);
			ecore_wr(p_hwfn, p_ptt,
				 NIG_REG_LLH_ENG_CLS_ENG_ID_TBL, 0x55555555);
			ecore_wr(p_hwfn, p_ptt,
				 NIG_REG_LLH_ENG_CLS_ENG_ID_TBL + 0x4,
				 0x55555555);
		}

		ecore_emul_link_init(p_hwfn, p_ptt);
	} else {
		DP_INFO(p_hwfn->p_dev, "link is not being configured\n");
	}
#endif

	return rc;
}

static enum _ecore_status_t
ecore_hw_init_pf_doorbell_bar(struct ecore_hwfn *p_hwfn,
			      struct ecore_ptt *p_ptt)
{
	u32 pwm_regsize, norm_regsize;
	u32 non_pwm_conn, min_addr_reg1;
	u32 db_bar_size, n_cpus;
	u32 pf_dems_shift;
	int rc = ECORE_SUCCESS;

	db_bar_size = ecore_hw_bar_size(p_hwfn, BAR_ID_1);
	if (p_hwfn->p_dev->num_hwfns > 1)
		db_bar_size /= 2;

	/* Calculate doorbell regions
	 * -----------------------------------
	 * The doorbell BAR is made of two regions. The first is called normal
	 * region and the second is called PWM region. In the normal region
	 * each ICID has its own set of addresses so that writing to that
	 * specific address identifies the ICID. In the Process Window Mode
	 * region the ICID is given in the data written to the doorbell. The
	 * above per PF register denotes the offset in the doorbell BAR in which
	 * the PWM region begins.
	 * The normal region has ECORE_PF_DEMS_SIZE bytes per ICID, that is per
	 * non-PWM connection. The calculation below computes the total non-PWM
	 * connections. The DORQ_REG_PF_MIN_ADDR_REG1 register is
	 * in units of 4,096 bytes.
	 */
	non_pwm_conn = ecore_cxt_get_proto_cid_start(p_hwfn, PROTOCOLID_CORE) +
	    ecore_cxt_get_proto_cid_count(p_hwfn, PROTOCOLID_CORE,
					  OSAL_NULL) +
	    ecore_cxt_get_proto_cid_count(p_hwfn, PROTOCOLID_ETH, OSAL_NULL);
	norm_regsize = ROUNDUP(ECORE_PF_DEMS_SIZE * non_pwm_conn, 4096);
	min_addr_reg1 = norm_regsize / 4096;
	pwm_regsize = db_bar_size - norm_regsize;

	/* Check that the normal and PWM sizes are valid */
	if (db_bar_size < norm_regsize) {
		DP_ERR(p_hwfn->p_dev,
		       "Doorbell BAR size 0x%x is too"
		       " small (normal region is 0x%0x )\n",
		       db_bar_size, norm_regsize);
		return ECORE_NORESOURCES;
	}
	if (pwm_regsize < ECORE_MIN_PWM_REGION) {
		DP_ERR(p_hwfn->p_dev,
		       "PWM region size 0x%0x is too small."
		       " Should be at least 0x%0x (Doorbell BAR size"
		       " is 0x%x and normal region size is 0x%0x)\n",
		       pwm_regsize, ECORE_MIN_PWM_REGION, db_bar_size,
		       norm_regsize);
		return ECORE_NORESOURCES;
	}

	/* Update hwfn */
	p_hwfn->dpi_start_offset = norm_regsize; /* this is later used to
						  * calculate the doorbell
						  * address
						  */

	/* Update registers */
	/* DEMS size is configured log2 of DWORDs, hence the division by 4 */
	pf_dems_shift = OSAL_LOG2(ECORE_PF_DEMS_SIZE / 4);
	ecore_wr(p_hwfn, p_ptt, DORQ_REG_PF_ICID_BIT_SHIFT_NORM, pf_dems_shift);
	ecore_wr(p_hwfn, p_ptt, DORQ_REG_PF_MIN_ADDR_REG1, min_addr_reg1);

	DP_INFO(p_hwfn,
		"Doorbell size 0x%x, Normal region 0x%x, PWM region 0x%x\n",
		db_bar_size, norm_regsize, pwm_regsize);
	DP_INFO(p_hwfn, "DPI size 0x%x, DPI count 0x%x\n", p_hwfn->dpi_size,
		p_hwfn->dpi_count);

	return ECORE_SUCCESS;
}

static enum _ecore_status_t
ecore_hw_init_pf(struct ecore_hwfn *p_hwfn,
		 struct ecore_ptt *p_ptt,
		 struct ecore_tunn_start_params *p_tunn,
		 int hw_mode,
		 bool b_hw_start,
		 enum ecore_int_mode int_mode, bool allow_npar_tx_switch)
{
	enum _ecore_status_t rc = ECORE_SUCCESS;
	u8 rel_pf_id = p_hwfn->rel_pf_id;
	u32 prs_reg;
	u16 ctrl;
	int pos;

	/* ILT/DQ/CM/QM */
	if (p_hwfn->mcp_info) {
		struct ecore_mcp_function_info *p_info;

		p_info = &p_hwfn->mcp_info->func_info;
		if (p_info->bandwidth_min)
			p_hwfn->qm_info.pf_wfq = p_info->bandwidth_min;

		/* Update rate limit once we'll actually have a link */
		p_hwfn->qm_info.pf_rl = 100;
	}
	ecore_cxt_hw_init_pf(p_hwfn);

	ecore_int_igu_init_rt(p_hwfn);	/* @@@TBD TODO MichalS multi hwfn ?? */

	/* Set VLAN in NIG if needed */
	if (hw_mode & (1 << MODE_MF_SD)) {
		DP_VERBOSE(p_hwfn, ECORE_MSG_HW, "Configuring LLH_FUNC_TAG\n");
		STORE_RT_REG(p_hwfn, NIG_REG_LLH_FUNC_TAG_EN_RT_OFFSET, 1);
		STORE_RT_REG(p_hwfn, NIG_REG_LLH_FUNC_TAG_VALUE_RT_OFFSET,
			     p_hwfn->hw_info.ovlan);
	}

	/* Enable classification by MAC if needed */
	if (hw_mode & (1 << MODE_MF_SI)) {
		DP_VERBOSE(p_hwfn, ECORE_MSG_HW,
			   "Configuring TAGMAC_CLS_TYPE\n");
		STORE_RT_REG(p_hwfn, NIG_REG_LLH_FUNC_TAGMAC_CLS_TYPE_RT_OFFSET,
			     1);
	}

	/* Protocl Configuration  - @@@TBD - should we set 0 otherwise? */
	STORE_RT_REG(p_hwfn, PRS_REG_SEARCH_TCP_RT_OFFSET, 0);

	/* perform debug configuration when chip is out of reset */
	OSAL_BEFORE_PF_START((void *)p_hwfn->p_dev, p_hwfn->my_id);

	/* Cleanup chip from previous driver if such remains exist */
	rc = ecore_final_cleanup(p_hwfn, p_ptt, rel_pf_id, false);
	if (rc != ECORE_SUCCESS) {
		ecore_hw_err_notify(p_hwfn, ECORE_HW_ERR_RAMROD_FAIL);
		return rc;
	}

	/* PF Init sequence */
	rc = ecore_init_run(p_hwfn, p_ptt, PHASE_PF, rel_pf_id, hw_mode);
	if (rc)
		return rc;

	/* QM_PF Init sequence (may be invoked separately e.g. for DCB) */
	rc = ecore_init_run(p_hwfn, p_ptt, PHASE_QM_PF, rel_pf_id, hw_mode);
	if (rc)
		return rc;

	/* Pure runtime initializations - directly to the HW  */
	ecore_int_igu_init_pure_rt(p_hwfn, p_ptt, true, true);

	/* PCI relaxed ordering causes a decrease in the performance on some
	 * systems. Till a root cause is found, disable this attribute in the
	 * PCI config space.
	 */
	/* Not in use @DPDK
	 * pos = OSAL_PCI_FIND_CAPABILITY(p_hwfn->p_dev, PCI_CAP_ID_EXP);
	 * if (!pos) {
	 *      DP_NOTICE(p_hwfn, true,
	 *                "Failed to find the PCI Express"
	 *                " Capability structure in the PCI config space\n");
	 *      return ECORE_IO;
	 * }
	 * OSAL_PCI_READ_CONFIG_WORD(p_hwfn->p_dev, pos + PCI_EXP_DEVCTL,
	 *                           &ctrl);
	 * ctrl &= ~PCI_EXP_DEVCTL_RELAX_EN;
	 * OSAL_PCI_WRITE_CONFIG_WORD(p_hwfn->p_dev, pos + PCI_EXP_DEVCTL,
	 *                           &ctrl);
	 */

#ifndef ASIC_ONLY
	/*@@TMP - On B0 build 1, need to mask the datapath_registers parity */
	if (CHIP_REV_IS_EMUL_B0(p_hwfn->p_dev) &&
	    (p_hwfn->p_dev->chip_metal == 1)) {
		u32 reg_addr, tmp;

		reg_addr =
		    attn_blocks[BLOCK_PGLUE_B].
		    chip_regs[ECORE_GET_TYPE(p_hwfn->p_dev)].prty_regs[0]->
		    mask_addr;
		DP_NOTICE(p_hwfn, false,
			  "Masking datapath registers parity on"
			  " B0 emulation [build 1]\n");
		tmp = ecore_rd(p_hwfn, p_ptt, reg_addr);
		tmp |= (1 << 0);	/* Was PRTY_MASK_DATAPATH_REGISTERS */
		ecore_wr(p_hwfn, p_ptt, reg_addr, tmp);
	}
#endif

	rc = ecore_hw_init_pf_doorbell_bar(p_hwfn, p_ptt);
	if (rc)
		return rc;

	if (b_hw_start) {
		/* enable interrupts */
		ecore_int_igu_enable(p_hwfn, p_ptt, int_mode);

		/* send function start command */
		rc = ecore_sp_pf_start(p_hwfn, p_tunn, p_hwfn->p_dev->mf_mode,
				       allow_npar_tx_switch);
		if (rc) {
			DP_NOTICE(p_hwfn, true,
				  "Function start ramrod failed\n");
		} else {
			prs_reg = ecore_rd(p_hwfn, p_ptt, PRS_REG_SEARCH_TAG1);
			DP_VERBOSE(p_hwfn, ECORE_MSG_STORAGE,
				   "PRS_REG_SEARCH_TAG1: %x\n", prs_reg);

			DP_VERBOSE(p_hwfn, ECORE_MSG_STORAGE,
				   "PRS_REG_SEARCH register after start PFn\n");
			prs_reg = ecore_rd(p_hwfn, p_ptt, PRS_REG_SEARCH_TCP);
			DP_VERBOSE(p_hwfn, ECORE_MSG_STORAGE,
				   "PRS_REG_SEARCH_TCP: %x\n", prs_reg);
			prs_reg = ecore_rd(p_hwfn, p_ptt, PRS_REG_SEARCH_UDP);
			DP_VERBOSE(p_hwfn, ECORE_MSG_STORAGE,
				   "PRS_REG_SEARCH_UDP: %x\n", prs_reg);
			prs_reg = ecore_rd(p_hwfn, p_ptt,
					   PRS_REG_SEARCH_TCP_FIRST_FRAG);
			DP_VERBOSE(p_hwfn, ECORE_MSG_STORAGE,
				   "PRS_REG_SEARCH_TCP_FIRST_FRAG: %x\n",
				   prs_reg);
			prs_reg = ecore_rd(p_hwfn, p_ptt, PRS_REG_SEARCH_TAG1);
			DP_VERBOSE(p_hwfn, ECORE_MSG_STORAGE,
				   "PRS_REG_SEARCH_TAG1: %x\n", prs_reg);
		}
	}
	return rc;
}

static enum _ecore_status_t
ecore_change_pci_hwfn(struct ecore_hwfn *p_hwfn,
		      struct ecore_ptt *p_ptt, u8 enable)
{
	u32 delay_idx = 0, val, set_val = enable ? 1 : 0;

	/* Change PF in PXP */
	ecore_wr(p_hwfn, p_ptt,
		 PGLUE_B_REG_INTERNAL_PFID_ENABLE_MASTER, set_val);

	/* wait until value is set - try for 1 second every 50us */
	for (delay_idx = 0; delay_idx < 20000; delay_idx++) {
		val = ecore_rd(p_hwfn, p_ptt,
			       PGLUE_B_REG_INTERNAL_PFID_ENABLE_MASTER);
		if (val == set_val)
			break;

		OSAL_UDELAY(50);
	}

	if (val != set_val) {
		DP_NOTICE(p_hwfn, true,
			  "PFID_ENABLE_MASTER wasn't changed after a second\n");
		return ECORE_UNKNOWN_ERROR;
	}

	return ECORE_SUCCESS;
}

static void ecore_reset_mb_shadow(struct ecore_hwfn *p_hwfn,
				  struct ecore_ptt *p_main_ptt)
{
	/* Read shadow of current MFW mailbox */
	ecore_mcp_read_mb(p_hwfn, p_main_ptt);
	OSAL_MEMCPY(p_hwfn->mcp_info->mfw_mb_shadow,
		    p_hwfn->mcp_info->mfw_mb_cur,
		    p_hwfn->mcp_info->mfw_mb_length);
}

enum _ecore_status_t ecore_hw_init(struct ecore_dev *p_dev,
				   struct ecore_tunn_start_params *p_tunn,
				   bool b_hw_start,
				   enum ecore_int_mode int_mode,
				   bool allow_npar_tx_switch,
				   const u8 *bin_fw_data)
{
	enum _ecore_status_t rc, mfw_rc;
	u32 load_code, param;
	int i, j;

	if (IS_PF(p_dev)) {
		rc = ecore_init_fw_data(p_dev, bin_fw_data);
		if (rc != ECORE_SUCCESS)
			return rc;
	}

	for_each_hwfn(p_dev, i) {
		struct ecore_hwfn *p_hwfn = &p_dev->hwfns[i];

		if (IS_VF(p_dev)) {
			rc = ecore_vf_pf_init(p_hwfn);
			if (rc)
				return rc;
			continue;
		}

		/* Enable DMAE in PXP */
		rc = ecore_change_pci_hwfn(p_hwfn, p_hwfn->p_main_ptt, true);

		ecore_calc_hw_mode(p_hwfn);
		/* @@@TBD need to add here:
		 * Check for fan failure
		 * Prev_unload
		 */
		rc = ecore_mcp_load_req(p_hwfn, p_hwfn->p_main_ptt, &load_code);
		if (rc) {
			DP_NOTICE(p_hwfn, true,
				  "Failed sending LOAD_REQ command\n");
			return rc;
		}

		/* CQ75580:
		 * When coming back from hiberbate state, the registers from
		 * which shadow is read initially are not initialized. It turns
		 * out that these registers get initialized during the call to
		 * ecore_mcp_load_req request. So we need to reread them here
		 * to get the proper shadow register value.
		 * Note: This is a workaround for the missinginig MFW
		 * initialization. It may be removed once the implementation
		 * is done.
		 */
		ecore_reset_mb_shadow(p_hwfn, p_hwfn->p_main_ptt);

		DP_VERBOSE(p_hwfn, ECORE_MSG_SP,
			   "Load request was sent.Resp:0x%x, Load code: 0x%x\n",
			   rc, load_code);

		/* Only relevant for recovery:
		 * Clear the indication after the LOAD_REQ command is responded
		 * by the MFW.
		 */
		p_dev->recov_in_prog = false;

		p_hwfn->first_on_engine = (load_code ==
					   FW_MSG_CODE_DRV_LOAD_ENGINE);

		switch (load_code) {
		case FW_MSG_CODE_DRV_LOAD_ENGINE:
			rc = ecore_hw_init_common(p_hwfn, p_hwfn->p_main_ptt,
						  p_hwfn->hw_info.hw_mode);
			if (rc)
				break;
			/* Fall into */
		case FW_MSG_CODE_DRV_LOAD_PORT:
			rc = ecore_hw_init_port(p_hwfn, p_hwfn->p_main_ptt,
						p_hwfn->hw_info.hw_mode);
			if (rc)
				break;

			if (ENABLE_EAGLE_ENG1_WORKAROUND(p_hwfn)) {
				struct init_nig_pri_tc_map_req tc_map;

				OSAL_MEM_ZERO(&tc_map, sizeof(tc_map));

				/* remove this once flow control is
				 * implemented
				 */
				for (j = 0; j < NUM_OF_VLAN_PRIORITIES; j++) {
					tc_map.pri[j].tc_id = 0;
					tc_map.pri[j].valid = 1;
				}
				ecore_init_nig_pri_tc_map(p_hwfn,
							  p_hwfn->p_main_ptt,
							  &tc_map);
			}
			/* fallthrough */
		case FW_MSG_CODE_DRV_LOAD_FUNCTION:
			rc = ecore_hw_init_pf(p_hwfn, p_hwfn->p_main_ptt,
					      p_tunn, p_hwfn->hw_info.hw_mode,
					      b_hw_start, int_mode,
					      allow_npar_tx_switch);
			break;
		default:
			rc = ECORE_NOTIMPL;
			break;
		}

		if (rc != ECORE_SUCCESS)
			DP_NOTICE(p_hwfn, true,
				  "init phase failed loadcode 0x%x (rc %d)\n",
				  load_code, rc);

		/* ACK mfw regardless of success or failure of initialization */
		mfw_rc = ecore_mcp_cmd(p_hwfn, p_hwfn->p_main_ptt,
				       DRV_MSG_CODE_LOAD_DONE,
				       0, &load_code, &param);
		if (rc != ECORE_SUCCESS)
			return rc;
		if (mfw_rc != ECORE_SUCCESS) {
			DP_NOTICE(p_hwfn, true,
				  "Failed sending LOAD_DONE command\n");
			return mfw_rc;
		}

		/* send DCBX attention request command */
		DP_VERBOSE(p_hwfn, ECORE_MSG_DCB,
			   "sending phony dcbx set command to trigger DCBx"
			   " attention handling\n");
		mfw_rc = ecore_mcp_cmd(p_hwfn, p_hwfn->p_main_ptt,
				       DRV_MSG_CODE_SET_DCBX,
				       1 << DRV_MB_PARAM_DCBX_NOTIFY_SHIFT,
				       &load_code, &param);
		if (mfw_rc != ECORE_SUCCESS) {
			DP_NOTICE(p_hwfn, true,
				  "Failed to send DCBX attention request\n");
			return mfw_rc;
		}

		p_hwfn->hw_init_done = true;
	}

	return ECORE_SUCCESS;
}

#define ECORE_HW_STOP_RETRY_LIMIT	(10)
static OSAL_INLINE void ecore_hw_timers_stop(struct ecore_dev *p_dev,
					     struct ecore_hwfn *p_hwfn,
					     struct ecore_ptt *p_ptt)
{
	int i;

	/* close timers */
	ecore_wr(p_hwfn, p_ptt, TM_REG_PF_ENABLE_CONN, 0x0);
	ecore_wr(p_hwfn, p_ptt, TM_REG_PF_ENABLE_TASK, 0x0);
	for (i = 0; i < ECORE_HW_STOP_RETRY_LIMIT &&
					!p_dev->recov_in_prog; i++) {
		if ((!ecore_rd(p_hwfn, p_ptt,
			       TM_REG_PF_SCAN_ACTIVE_CONN)) &&
		    (!ecore_rd(p_hwfn, p_ptt, TM_REG_PF_SCAN_ACTIVE_TASK)))
			break;

		/* Dependent on number of connection/tasks, possibly
		 * 1ms sleep is required between polls
		 */
		OSAL_MSLEEP(1);
	}
	if (i == ECORE_HW_STOP_RETRY_LIMIT)
		DP_NOTICE(p_hwfn, true,
			  "Timers linear scans are not over"
			  " [Connection %02x Tasks %02x]\n",
			  (u8)ecore_rd(p_hwfn, p_ptt,
				       TM_REG_PF_SCAN_ACTIVE_CONN),
			  (u8)ecore_rd(p_hwfn, p_ptt,
				       TM_REG_PF_SCAN_ACTIVE_TASK));
}

void ecore_hw_timers_stop_all(struct ecore_dev *p_dev)
{
	int j;

	for_each_hwfn(p_dev, j) {
		struct ecore_hwfn *p_hwfn = &p_dev->hwfns[j];
		struct ecore_ptt *p_ptt = p_hwfn->p_main_ptt;

		ecore_hw_timers_stop(p_dev, p_hwfn, p_ptt);
	}
}

enum _ecore_status_t ecore_hw_stop(struct ecore_dev *p_dev)
{
	enum _ecore_status_t rc = ECORE_SUCCESS, t_rc;
	int j;

	for_each_hwfn(p_dev, j) {
		struct ecore_hwfn *p_hwfn = &p_dev->hwfns[j];
		struct ecore_ptt *p_ptt = p_hwfn->p_main_ptt;

		DP_VERBOSE(p_hwfn, ECORE_MSG_IFDOWN, "Stopping hw/fw\n");

		if (IS_VF(p_dev)) {
			ecore_vf_pf_int_cleanup(p_hwfn);
			continue;
		}

		/* mark the hw as uninitialized... */
		p_hwfn->hw_init_done = false;

		rc = ecore_sp_pf_stop(p_hwfn);
		if (rc)
			DP_NOTICE(p_hwfn, true,
				  "Failed to close PF against FW. Continue to"
				  " stop HW to prevent illegal host access"
				  " by the device\n");

		/* perform debug action after PF stop was sent */
		OSAL_AFTER_PF_STOP((void *)p_hwfn->p_dev, p_hwfn->my_id);

		/* close NIG to BRB gate */
		ecore_wr(p_hwfn, p_ptt,
			 NIG_REG_RX_LLH_BRB_GATE_DNTFWD_PERPF, 0x1);

		/* close parser */
		ecore_wr(p_hwfn, p_ptt, PRS_REG_SEARCH_TCP, 0x0);
		ecore_wr(p_hwfn, p_ptt, PRS_REG_SEARCH_UDP, 0x0);
		ecore_wr(p_hwfn, p_ptt, PRS_REG_SEARCH_OPENFLOW, 0x0);

		/* @@@TBD - clean transmission queues (5.b) */
		/* @@@TBD - clean BTB (5.c) */

		ecore_hw_timers_stop(p_dev, p_hwfn, p_ptt);

		/* @@@TBD - verify DMAE requests are done (8) */

		/* Disable Attention Generation */
		ecore_int_igu_disable_int(p_hwfn, p_ptt);
		ecore_wr(p_hwfn, p_ptt, IGU_REG_LEADING_EDGE_LATCH, 0);
		ecore_wr(p_hwfn, p_ptt, IGU_REG_TRAILING_EDGE_LATCH, 0);
		ecore_int_igu_init_pure_rt(p_hwfn, p_ptt, false, true);
		/* Need to wait 1ms to guarantee SBs are cleared */
		OSAL_MSLEEP(1);
	}

	if (IS_PF(p_dev)) {
		/* Disable DMAE in PXP - in CMT, this should only be done for
		 * first hw-function, and only after all transactions have
		 * stopped for all active hw-functions.
		 */
		t_rc = ecore_change_pci_hwfn(&p_dev->hwfns[0],
					     p_dev->hwfns[0].p_main_ptt, false);
		if (t_rc != ECORE_SUCCESS)
			rc = t_rc;
	}

	return rc;
}

void ecore_hw_stop_fastpath(struct ecore_dev *p_dev)
{
	int j;

	for_each_hwfn(p_dev, j) {
		struct ecore_hwfn *p_hwfn = &p_dev->hwfns[j];
		struct ecore_ptt *p_ptt = p_hwfn->p_main_ptt;

		if (IS_VF(p_dev)) {
			ecore_vf_pf_int_cleanup(p_hwfn);
			continue;
		}

		DP_VERBOSE(p_hwfn, ECORE_MSG_IFDOWN,
			   "Shutting down the fastpath\n");

		ecore_wr(p_hwfn, p_ptt,
			 NIG_REG_RX_LLH_BRB_GATE_DNTFWD_PERPF, 0x1);

		ecore_wr(p_hwfn, p_ptt, PRS_REG_SEARCH_TCP, 0x0);
		ecore_wr(p_hwfn, p_ptt, PRS_REG_SEARCH_UDP, 0x0);
		ecore_wr(p_hwfn, p_ptt, PRS_REG_SEARCH_OPENFLOW, 0x0);

		/* @@@TBD - clean transmission queues (5.b) */
		/* @@@TBD - clean BTB (5.c) */

		/* @@@TBD - verify DMAE requests are done (8) */

		ecore_int_igu_init_pure_rt(p_hwfn, p_ptt, false, false);
		/* Need to wait 1ms to guarantee SBs are cleared */
		OSAL_MSLEEP(1);
	}
}

void ecore_hw_start_fastpath(struct ecore_hwfn *p_hwfn)
{
	struct ecore_ptt *p_ptt = p_hwfn->p_main_ptt;

	if (IS_VF(p_hwfn->p_dev))
		return;

	/* Re-open incoming traffic */
	ecore_wr(p_hwfn, p_hwfn->p_main_ptt,
		 NIG_REG_RX_LLH_BRB_GATE_DNTFWD_PERPF, 0x0);
}

static enum _ecore_status_t ecore_reg_assert(struct ecore_hwfn *p_hwfn,
					     struct ecore_ptt *p_ptt, u32 reg,
					     bool expected)
{
	u32 assert_val = ecore_rd(p_hwfn, p_ptt, reg);

	if (assert_val != expected) {
		DP_NOTICE(p_hwfn, true, "Value at address 0x%08x != 0x%08x\n",
			  reg, expected);
		return ECORE_UNKNOWN_ERROR;
	}

	return 0;
}

enum _ecore_status_t ecore_hw_reset(struct ecore_dev *p_dev)
{
	enum _ecore_status_t rc = ECORE_SUCCESS;
	u32 unload_resp, unload_param;
	int i;

	for_each_hwfn(p_dev, i) {
		struct ecore_hwfn *p_hwfn = &p_dev->hwfns[i];

		if (IS_VF(p_dev)) {
			rc = ecore_vf_pf_reset(p_hwfn);
			if (rc)
				return rc;
			continue;
		}

		DP_VERBOSE(p_hwfn, ECORE_MSG_IFDOWN, "Resetting hw/fw\n");

		/* Check for incorrect states */
		if (!p_dev->recov_in_prog) {
			ecore_reg_assert(p_hwfn, p_hwfn->p_main_ptt,
					 QM_REG_USG_CNT_PF_TX, 0);
			ecore_reg_assert(p_hwfn, p_hwfn->p_main_ptt,
					 QM_REG_USG_CNT_PF_OTHER, 0);
			/* @@@TBD - assert on incorrect xCFC values (10.b) */
		}

		/* Disable PF in HW blocks */
		ecore_wr(p_hwfn, p_hwfn->p_main_ptt, DORQ_REG_PF_DB_ENABLE, 0);
		ecore_wr(p_hwfn, p_hwfn->p_main_ptt, QM_REG_PF_EN, 0);
		ecore_wr(p_hwfn, p_hwfn->p_main_ptt,
			 TCFC_REG_STRONG_ENABLE_PF, 0);
		ecore_wr(p_hwfn, p_hwfn->p_main_ptt,
			 CCFC_REG_STRONG_ENABLE_PF, 0);

		if (p_dev->recov_in_prog) {
			DP_VERBOSE(p_hwfn, ECORE_MSG_IFDOWN,
				   "Recovery is in progress -> skip "
				   "sending unload_req/done\n");
			break;
		}

		/* Send unload command to MCP */
		rc = ecore_mcp_cmd(p_hwfn, p_hwfn->p_main_ptt,
				   DRV_MSG_CODE_UNLOAD_REQ,
				   DRV_MB_PARAM_UNLOAD_WOL_MCP,
				   &unload_resp, &unload_param);
		if (rc != ECORE_SUCCESS) {
			DP_NOTICE(p_hwfn, true,
				  "ecore_hw_reset: UNLOAD_REQ failed\n");
			/* @@TBD - what to do? for now, assume ENG. */
			unload_resp = FW_MSG_CODE_DRV_UNLOAD_ENGINE;
		}

		rc = ecore_mcp_cmd(p_hwfn, p_hwfn->p_main_ptt,
				   DRV_MSG_CODE_UNLOAD_DONE,
				   0, &unload_resp, &unload_param);
		if (rc != ECORE_SUCCESS) {
			DP_NOTICE(p_hwfn,
				  true, "ecore_hw_reset: UNLOAD_DONE failed\n");
			/* @@@TBD - Should it really ASSERT here ? */
			return rc;
		}
	}

	return rc;
}

/* Free hwfn memory and resources acquired in hw_hwfn_prepare */
static void ecore_hw_hwfn_free(struct ecore_hwfn *p_hwfn)
{
	ecore_ptt_pool_free(p_hwfn);
	OSAL_FREE(p_hwfn->p_dev, p_hwfn->hw_info.p_igu_info);
}

/* Setup bar access */
static void ecore_hw_hwfn_prepare(struct ecore_hwfn *p_hwfn)
{
	/* clear indirect access */
	ecore_wr(p_hwfn, p_hwfn->p_main_ptt, PGLUE_B_REG_PGL_ADDR_88_F0, 0);
	ecore_wr(p_hwfn, p_hwfn->p_main_ptt, PGLUE_B_REG_PGL_ADDR_8C_F0, 0);
	ecore_wr(p_hwfn, p_hwfn->p_main_ptt, PGLUE_B_REG_PGL_ADDR_90_F0, 0);
	ecore_wr(p_hwfn, p_hwfn->p_main_ptt, PGLUE_B_REG_PGL_ADDR_94_F0, 0);

	/* Clean Previous errors if such exist */
	ecore_wr(p_hwfn, p_hwfn->p_main_ptt,
		 PGLUE_B_REG_WAS_ERROR_PF_31_0_CLR, 1 << p_hwfn->abs_pf_id);

	/* enable internal target-read */
	ecore_wr(p_hwfn, p_hwfn->p_main_ptt,
		 PGLUE_B_REG_INTERNAL_PFID_ENABLE_TARGET_READ, 1);
}

static void get_function_id(struct ecore_hwfn *p_hwfn)
{
	/* ME Register */
	p_hwfn->hw_info.opaque_fid = (u16)REG_RD(p_hwfn,
						 PXP_PF_ME_OPAQUE_ADDR);

	p_hwfn->hw_info.concrete_fid = REG_RD(p_hwfn, PXP_PF_ME_CONCRETE_ADDR);

	/* Bits 16-19 from the ME registers are the pf_num */
	/* @@ @TBD - check, may be wrong after B0 implementation for CMT */
	p_hwfn->abs_pf_id = (p_hwfn->hw_info.concrete_fid >> 16) & 0xf;
	p_hwfn->rel_pf_id = GET_FIELD(p_hwfn->hw_info.concrete_fid,
				      PXP_CONCRETE_FID_PFID);
	p_hwfn->port_id = GET_FIELD(p_hwfn->hw_info.concrete_fid,
				    PXP_CONCRETE_FID_PORT);

	DP_VERBOSE(p_hwfn, ECORE_MSG_PROBE,
		   "Read ME register: Concrete 0x%08x Opaque 0x%04x\n",
		   p_hwfn->hw_info.concrete_fid, p_hwfn->hw_info.opaque_fid);
}

static void ecore_hw_set_feat(struct ecore_hwfn *p_hwfn)
{
	u32 *feat_num = p_hwfn->hw_info.feat_num;
	int num_features = 1;

	/* L2 Queues require each: 1 status block. 1 L2 queue */
	feat_num[ECORE_PF_L2_QUE] =
	    OSAL_MIN_T(u32,
		       RESC_NUM(p_hwfn, ECORE_SB) / num_features,
		       RESC_NUM(p_hwfn, ECORE_L2_QUEUE));

	DP_VERBOSE(p_hwfn, ECORE_MSG_PROBE,
		   "#PF_L2_QUEUES=%d #SBS=%d num_features=%d\n",
		   feat_num[ECORE_PF_L2_QUE],
		   RESC_NUM(p_hwfn, ECORE_SB), num_features);
}

/* @@@TBD MK RESC: This info is currently hard code and set as if we were MF
 * need to read it from shmem...
 */
static enum _ecore_status_t ecore_hw_get_resc(struct ecore_hwfn *p_hwfn)
{
	u32 *resc_start = p_hwfn->hw_info.resc_start;
	u8 num_funcs = p_hwfn->num_funcs_on_engine;
	u32 *resc_num = p_hwfn->hw_info.resc_num;
	int i, max_vf_vlan_filters;
	struct ecore_sb_cnt_info sb_cnt_info;
	bool b_ah = ECORE_IS_AH(p_hwfn->p_dev);

	OSAL_MEM_ZERO(&sb_cnt_info, sizeof(sb_cnt_info));

#ifdef CONFIG_ECORE_SRIOV
	max_vf_vlan_filters = ECORE_ETH_MAX_VF_NUM_VLAN_FILTERS;
#else
	max_vf_vlan_filters = 0;
#endif

	ecore_int_get_num_sbs(p_hwfn, &sb_cnt_info);
	resc_num[ECORE_SB] = OSAL_MIN_T(u32,
					(MAX_SB_PER_PATH_BB / num_funcs),
					sb_cnt_info.sb_cnt);

	resc_num[ECORE_L2_QUEUE] = (b_ah ? MAX_NUM_L2_QUEUES_K2 :
				    MAX_NUM_L2_QUEUES_BB) / num_funcs;
	resc_num[ECORE_VPORT] = (b_ah ? MAX_NUM_VPORTS_K2 :
				 MAX_NUM_VPORTS_BB) / num_funcs;
	resc_num[ECORE_RSS_ENG] = (b_ah ? ETH_RSS_ENGINE_NUM_K2 :
				   ETH_RSS_ENGINE_NUM_BB) / num_funcs;
	resc_num[ECORE_PQ] = (b_ah ? MAX_QM_TX_QUEUES_K2 :
			      MAX_QM_TX_QUEUES_BB) / num_funcs;
	resc_num[ECORE_RL] = 8;
	resc_num[ECORE_MAC] = ETH_NUM_MAC_FILTERS / num_funcs;
	resc_num[ECORE_VLAN] = (ETH_NUM_VLAN_FILTERS -
				max_vf_vlan_filters +
				1 /*For vlan0 */) / num_funcs;

	/* TODO - there will be a problem in AH - there are only 11k lines */
	resc_num[ECORE_ILT] = (b_ah ? PXP_NUM_ILT_RECORDS_K2 :
			       PXP_NUM_ILT_RECORDS_BB) / num_funcs;

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_SLOW(p_hwfn->p_dev)) {
		/* Reduced build contains less PQs */
		if (!(p_hwfn->p_dev->b_is_emul_full))
			resc_num[ECORE_PQ] = 32;

		/* For AH emulation, since we have a possible maximal number of
		 * 16 enabled PFs, in case there are not enough ILT lines -
		 * allocate only first PF as RoCE and have all the other ETH
		 * only with less ILT lines.
		 */
		if (!p_hwfn->rel_pf_id && p_hwfn->p_dev->b_is_emul_full)
			resc_num[ECORE_ILT] = resc_num[ECORE_ILT];
	}
#endif

	for (i = 0; i < ECORE_MAX_RESC; i++)
		resc_start[i] = resc_num[i] * p_hwfn->rel_pf_id;

#ifndef ASIC_ONLY
	/* Correct the common ILT calculation if PF0 has more */
	if (CHIP_REV_IS_SLOW(p_hwfn->p_dev) &&
	    p_hwfn->p_dev->b_is_emul_full &&
	    p_hwfn->rel_pf_id && resc_num[ECORE_ILT])
		resc_start[ECORE_ILT] += resc_num[ECORE_ILT];
#endif

	/* Sanity for ILT */
	if ((b_ah && (RESC_END(p_hwfn, ECORE_ILT) > PXP_NUM_ILT_RECORDS_K2)) ||
	    (!b_ah && (RESC_END(p_hwfn, ECORE_ILT) > PXP_NUM_ILT_RECORDS_BB))) {
		DP_NOTICE(p_hwfn, true,
			  "Can't assign ILT pages [%08x,...,%08x]\n",
			  RESC_START(p_hwfn, ECORE_ILT), RESC_END(p_hwfn,
								  ECORE_ILT) -
			  1);
		return ECORE_INVAL;
	}

	ecore_hw_set_feat(p_hwfn);

	DP_VERBOSE(p_hwfn, ECORE_MSG_PROBE,
		   "The numbers for each resource are:\n"
		   "SB = %d start = %d\n"
		   "L2_QUEUE = %d start = %d\n"
		   "VPORT = %d start = %d\n"
		   "PQ = %d start = %d\n"
		   "RL = %d start = %d\n"
		   "MAC = %d start = %d\n"
		   "VLAN = %d start = %d\n"
		   "ILT = %d start = %d\n"
		   "CMDQS_CQS = %d start = %d\n",
		   RESC_NUM(p_hwfn, ECORE_SB), RESC_START(p_hwfn, ECORE_SB),
		   RESC_NUM(p_hwfn, ECORE_L2_QUEUE),
		   RESC_START(p_hwfn, ECORE_L2_QUEUE),
		   RESC_NUM(p_hwfn, ECORE_VPORT),
		   RESC_START(p_hwfn, ECORE_VPORT),
		   RESC_NUM(p_hwfn, ECORE_PQ), RESC_START(p_hwfn, ECORE_PQ),
		   RESC_NUM(p_hwfn, ECORE_RL), RESC_START(p_hwfn, ECORE_RL),
		   RESC_NUM(p_hwfn, ECORE_MAC), RESC_START(p_hwfn, ECORE_MAC),
		   RESC_NUM(p_hwfn, ECORE_VLAN),
		   RESC_START(p_hwfn, ECORE_VLAN),
		   RESC_NUM(p_hwfn, ECORE_ILT), RESC_START(p_hwfn, ECORE_ILT),
		   RESC_NUM(p_hwfn, ECORE_CMDQS_CQS),
		   RESC_START(p_hwfn, ECORE_CMDQS_CQS));

	return ECORE_SUCCESS;
}

static enum _ecore_status_t ecore_hw_get_nvm_info(struct ecore_hwfn *p_hwfn,
						  struct ecore_ptt *p_ptt)
{
	u32 nvm_cfg1_offset, mf_mode, addr, generic_cont0, core_cfg;
	u32 port_cfg_addr, link_temp, device_capabilities;
	struct ecore_mcp_link_params *link;

	/* Read global nvm_cfg address */
	u32 nvm_cfg_addr = ecore_rd(p_hwfn, p_ptt, MISC_REG_GEN_PURP_CR0);

	/* Verify MCP has initialized it */
	if (nvm_cfg_addr == 0) {
		DP_NOTICE(p_hwfn, false, "Shared memory not initialized\n");
		return ECORE_INVAL;
	}

	/* Read nvm_cfg1  (Notice this is just offset, and not offsize (TBD) */
	nvm_cfg1_offset = ecore_rd(p_hwfn, p_ptt, nvm_cfg_addr + 4);

	addr = MCP_REG_SCRATCH + nvm_cfg1_offset +
	    OFFSETOF(struct nvm_cfg1, glob) + OFFSETOF(struct nvm_cfg1_glob,
						       core_cfg);

	core_cfg = ecore_rd(p_hwfn, p_ptt, addr);

	switch ((core_cfg & NVM_CFG1_GLOB_NETWORK_PORT_MODE_MASK) >>
		NVM_CFG1_GLOB_NETWORK_PORT_MODE_OFFSET) {
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_DE_2X40G:
		p_hwfn->hw_info.port_mode = ECORE_PORT_MODE_DE_2X40G;
		break;
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_DE_2X50G:
		p_hwfn->hw_info.port_mode = ECORE_PORT_MODE_DE_2X50G;
		break;
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_DE_1X100G:
		p_hwfn->hw_info.port_mode = ECORE_PORT_MODE_DE_1X100G;
		break;
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_DE_4X10G_F:
		p_hwfn->hw_info.port_mode = ECORE_PORT_MODE_DE_4X10G_F;
		break;
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_DE_4X10G_E:
		p_hwfn->hw_info.port_mode = ECORE_PORT_MODE_DE_4X10G_E;
		break;
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_DE_4X20G:
		p_hwfn->hw_info.port_mode = ECORE_PORT_MODE_DE_4X20G;
		break;
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_DE_1X40G:
		p_hwfn->hw_info.port_mode = ECORE_PORT_MODE_DE_1X40G;
		break;
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_DE_2X25G:
		p_hwfn->hw_info.port_mode = ECORE_PORT_MODE_DE_2X25G;
		break;
	case NVM_CFG1_GLOB_NETWORK_PORT_MODE_DE_1X25G:
		p_hwfn->hw_info.port_mode = ECORE_PORT_MODE_DE_1X25G;
		break;
	default:
		DP_NOTICE(p_hwfn, true, "Unknown port mode in 0x%08x\n",
			  core_cfg);
		break;
	}

	/* Read default link configuration */
	link = &p_hwfn->mcp_info->link_input;
	port_cfg_addr = MCP_REG_SCRATCH + nvm_cfg1_offset +
	    OFFSETOF(struct nvm_cfg1, port[MFW_PORT(p_hwfn)]);
	link_temp = ecore_rd(p_hwfn, p_ptt,
			     port_cfg_addr +
			     OFFSETOF(struct nvm_cfg1_port, speed_cap_mask));
	link_temp &= NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_MASK;
	link->speed.advertised_speeds = link_temp;

	link_temp = link->speed.advertised_speeds;
	p_hwfn->mcp_info->link_capabilities.speed_capabilities = link_temp;

	link_temp = ecore_rd(p_hwfn, p_ptt,
			     port_cfg_addr +
			     OFFSETOF(struct nvm_cfg1_port, link_settings));
	switch ((link_temp & NVM_CFG1_PORT_DRV_LINK_SPEED_MASK) >>
		NVM_CFG1_PORT_DRV_LINK_SPEED_OFFSET) {
	case NVM_CFG1_PORT_DRV_LINK_SPEED_AUTONEG:
		link->speed.autoneg = true;
		break;
	case NVM_CFG1_PORT_DRV_LINK_SPEED_1G:
		link->speed.forced_speed = 1000;
		break;
	case NVM_CFG1_PORT_DRV_LINK_SPEED_10G:
		link->speed.forced_speed = 10000;
		break;
	case NVM_CFG1_PORT_DRV_LINK_SPEED_25G:
		link->speed.forced_speed = 25000;
		break;
	case NVM_CFG1_PORT_DRV_LINK_SPEED_40G:
		link->speed.forced_speed = 40000;
		break;
	case NVM_CFG1_PORT_DRV_LINK_SPEED_50G:
		link->speed.forced_speed = 50000;
		break;
	case NVM_CFG1_PORT_DRV_LINK_SPEED_100G:
		link->speed.forced_speed = 100000;
		break;
	default:
		DP_NOTICE(p_hwfn, true, "Unknown Speed in 0x%08x\n", link_temp);
	}

	link_temp &= NVM_CFG1_PORT_DRV_FLOW_CONTROL_MASK;
	link_temp >>= NVM_CFG1_PORT_DRV_FLOW_CONTROL_OFFSET;
	link->pause.autoneg = !!(link_temp &
				  NVM_CFG1_PORT_DRV_FLOW_CONTROL_AUTONEG);
	link->pause.forced_rx = !!(link_temp &
				    NVM_CFG1_PORT_DRV_FLOW_CONTROL_RX);
	link->pause.forced_tx = !!(link_temp &
				    NVM_CFG1_PORT_DRV_FLOW_CONTROL_TX);
	link->loopback_mode = 0;

	DP_VERBOSE(p_hwfn, ECORE_MSG_LINK,
		   "Read default link: Speed 0x%08x, Adv. Speed 0x%08x,"
		   " AN: 0x%02x, PAUSE AN: 0x%02x\n",
		   link->speed.forced_speed, link->speed.advertised_speeds,
		   link->speed.autoneg, link->pause.autoneg);

	/* Read Multi-function information from shmem */
	addr = MCP_REG_SCRATCH + nvm_cfg1_offset +
	    OFFSETOF(struct nvm_cfg1, glob) +
	    OFFSETOF(struct nvm_cfg1_glob, generic_cont0);

	generic_cont0 = ecore_rd(p_hwfn, p_ptt, addr);

	mf_mode = (generic_cont0 & NVM_CFG1_GLOB_MF_MODE_MASK) >>
	    NVM_CFG1_GLOB_MF_MODE_OFFSET;

	switch (mf_mode) {
	case NVM_CFG1_GLOB_MF_MODE_MF_ALLOWED:
		p_hwfn->p_dev->mf_mode = ECORE_MF_OVLAN;
		break;
	case NVM_CFG1_GLOB_MF_MODE_NPAR1_0:
		p_hwfn->p_dev->mf_mode = ECORE_MF_NPAR;
		break;
	case NVM_CFG1_GLOB_MF_MODE_DEFAULT:
		p_hwfn->p_dev->mf_mode = ECORE_MF_DEFAULT;
		break;
	}
	DP_INFO(p_hwfn, "Multi function mode is %08x\n",
		p_hwfn->p_dev->mf_mode);

	/* Read Multi-function information from shmem */
	addr = MCP_REG_SCRATCH + nvm_cfg1_offset +
	    OFFSETOF(struct nvm_cfg1, glob) +
	    OFFSETOF(struct nvm_cfg1_glob, device_capabilities);

	device_capabilities = ecore_rd(p_hwfn, p_ptt, addr);
	if (device_capabilities & NVM_CFG1_GLOB_DEVICE_CAPABILITIES_ETHERNET)
		OSAL_SET_BIT(ECORE_DEV_CAP_ETH,
			     &p_hwfn->hw_info.device_capabilities);

	return ecore_mcp_fill_shmem_func_info(p_hwfn, p_ptt);
}

static void ecore_get_num_funcs(struct ecore_hwfn *p_hwfn,
				struct ecore_ptt *p_ptt)
{
	u8 num_funcs;
	u32 tmp, mask;

	num_funcs = ECORE_IS_AH(p_hwfn->p_dev) ? MAX_NUM_PFS_K2
	    : MAX_NUM_PFS_BB;

	/* Bit 0 of MISCS_REG_FUNCTION_HIDE indicates whether the bypass values
	 * in the other bits are selected.
	 * Bits 1-15 are for functions 1-15, respectively, and their value is
	 * '0' only for enabled functions (function 0 always exists and
	 * enabled).
	 * In case of CMT, only the "even" functions are enabled, and thus the
	 * number of functions for both hwfns is learnt from the same bits.
	 */

	tmp = ecore_rd(p_hwfn, p_ptt, MISCS_REG_FUNCTION_HIDE);
	if (tmp & 0x1) {
		if (ECORE_PATH_ID(p_hwfn) && p_hwfn->p_dev->num_hwfns == 1) {
			num_funcs = 0;
			mask = 0xaaaa;
		} else {
			num_funcs = 1;
			mask = 0x5554;
		}

		tmp = (tmp ^ 0xffffffff) & mask;
		while (tmp) {
			if (tmp & 0x1)
				num_funcs++;
			tmp >>= 0x1;
		}
	}

	p_hwfn->num_funcs_on_engine = num_funcs;

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_FPGA(p_hwfn->p_dev)) {
		DP_NOTICE(p_hwfn, false,
			  "FPGA: Limit number of PFs to 4 [would affect"
			  " resource allocation, needed for IOV]\n");
		p_hwfn->num_funcs_on_engine = 4;
	}
#endif

	DP_VERBOSE(p_hwfn, ECORE_MSG_PROBE, "num_funcs_on_engine = %d\n",
		   p_hwfn->num_funcs_on_engine);
}

static void ecore_hw_info_port_num_bb(struct ecore_hwfn *p_hwfn,
				      struct ecore_ptt *p_ptt)
{
	u32 port_mode;

#ifndef ASIC_ONLY
	/* Read the port mode */
	if (CHIP_REV_IS_FPGA(p_hwfn->p_dev))
		port_mode = 4;
	else if (CHIP_REV_IS_EMUL(p_hwfn->p_dev) &&
		 (p_hwfn->p_dev->num_hwfns > 1))
		/* In CMT on emulation, assume 1 port */
		port_mode = 1;
	else
#endif
		port_mode = ecore_rd(p_hwfn, p_ptt,
				     CNIG_REG_NW_PORT_MODE_BB_B0);

	if (port_mode < 3) {
		p_hwfn->p_dev->num_ports_in_engines = 1;
	} else if (port_mode <= 5) {
		p_hwfn->p_dev->num_ports_in_engines = 2;
	} else {
		DP_NOTICE(p_hwfn, true, "PORT MODE: %d not supported\n",
			  p_hwfn->p_dev->num_ports_in_engines);

		/* Default num_ports_in_engines to something */
		p_hwfn->p_dev->num_ports_in_engines = 1;
	}
}

static void ecore_hw_info_port_num_ah(struct ecore_hwfn *p_hwfn,
				      struct ecore_ptt *p_ptt)
{
	u32 port;
	int i;

	p_hwfn->p_dev->num_ports_in_engines = 0;

	for (i = 0; i < MAX_NUM_PORTS_K2; i++) {
		port = ecore_rd(p_hwfn, p_ptt,
				CNIG_REG_NIG_PORT0_CONF_K2 + (i * 4));
		if (port & 1)
			p_hwfn->p_dev->num_ports_in_engines++;
	}
}

static void ecore_hw_info_port_num(struct ecore_hwfn *p_hwfn,
				   struct ecore_ptt *p_ptt)
{
	if (ECORE_IS_BB(p_hwfn->p_dev))
		ecore_hw_info_port_num_bb(p_hwfn, p_ptt);
	else
		ecore_hw_info_port_num_ah(p_hwfn, p_ptt);
}

static enum _ecore_status_t
ecore_get_hw_info(struct ecore_hwfn *p_hwfn,
		  struct ecore_ptt *p_ptt,
		  enum ecore_pci_personality personality)
{
	enum _ecore_status_t rc;

	rc = ecore_iov_hw_info(p_hwfn, p_hwfn->p_main_ptt);
	if (rc)
		return rc;

	/* TODO In get_hw_info, amoungst others:
	 * Get MCP FW revision and determine according to it the supported
	 * featrues (e.g. DCB)
	 * Get boot mode
	 * ecore_get_pcie_width_speed, WOL capability.
	 * Number of global CQ-s (for storage
	 */
	ecore_hw_info_port_num(p_hwfn, p_ptt);

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_ASIC(p_hwfn->p_dev))
#endif
		ecore_hw_get_nvm_info(p_hwfn, p_ptt);

	rc = ecore_int_igu_read_cam(p_hwfn, p_ptt);
	if (rc)
		return rc;

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_ASIC(p_hwfn->p_dev) && ecore_mcp_is_init(p_hwfn)) {
#endif
		OSAL_MEMCPY(p_hwfn->hw_info.hw_mac_addr,
			    p_hwfn->mcp_info->func_info.mac, ETH_ALEN);
#ifndef ASIC_ONLY
	} else {
		static u8 mcp_hw_mac[6] = { 0, 2, 3, 4, 5, 6 };

		OSAL_MEMCPY(p_hwfn->hw_info.hw_mac_addr, mcp_hw_mac, ETH_ALEN);
		p_hwfn->hw_info.hw_mac_addr[5] = p_hwfn->abs_pf_id;
	}
#endif

	if (ecore_mcp_is_init(p_hwfn)) {
		if (p_hwfn->mcp_info->func_info.ovlan != ECORE_MCP_VLAN_UNSET)
			p_hwfn->hw_info.ovlan =
			    p_hwfn->mcp_info->func_info.ovlan;

		ecore_mcp_cmd_port_init(p_hwfn, p_ptt);
	}

	if (personality != ECORE_PCI_DEFAULT)
		p_hwfn->hw_info.personality = personality;
	else if (ecore_mcp_is_init(p_hwfn))
		p_hwfn->hw_info.personality =
		    p_hwfn->mcp_info->func_info.protocol;

#ifndef ASIC_ONLY
	/* To overcome ILT lack for emulation, until at least until we'll have
	 * a definite answer from system about it, allow only PF0 to be RoCE.
	 */
	if (CHIP_REV_IS_EMUL(p_hwfn->p_dev) && ECORE_IS_AH(p_hwfn->p_dev))
		p_hwfn->hw_info.personality = ECORE_PCI_ETH;
#endif

	ecore_get_num_funcs(p_hwfn, p_ptt);

	/* Feat num is dependent on personality and on the number of functions
	 * on the engine. Therefore it should be come after personality
	 * initialization and after getting the number of functions.
	 */
	return ecore_hw_get_resc(p_hwfn);
}

/* @TMP - this should move to a proper .h */
#define CHIP_NUM_AH			0x8070

static enum _ecore_status_t ecore_get_dev_info(struct ecore_dev *p_dev)
{
	struct ecore_hwfn *p_hwfn = ECORE_LEADING_HWFN(p_dev);
	u32 tmp;

	/* Read Vendor Id / Device Id */
	OSAL_PCI_READ_CONFIG_WORD(p_dev, PCICFG_VENDOR_ID_OFFSET,
				  &p_dev->vendor_id);
	OSAL_PCI_READ_CONFIG_WORD(p_dev, PCICFG_DEVICE_ID_OFFSET,
				  &p_dev->device_id);

	p_dev->chip_num = (u16)ecore_rd(p_hwfn, p_hwfn->p_main_ptt,
					 MISCS_REG_CHIP_NUM);
	p_dev->chip_rev = (u16)ecore_rd(p_hwfn, p_hwfn->p_main_ptt,
					MISCS_REG_CHIP_REV);

	MASK_FIELD(CHIP_REV, p_dev->chip_rev);

	/* Determine type */
	if (p_dev->device_id == CHIP_NUM_AH)
		p_dev->type = ECORE_DEV_TYPE_AH;
	else
		p_dev->type = ECORE_DEV_TYPE_BB;

	/* Learn number of HW-functions */
	tmp = ecore_rd(p_hwfn, p_hwfn->p_main_ptt,
		       MISCS_REG_CMT_ENABLED_FOR_PAIR);

	if (tmp & (1 << p_hwfn->rel_pf_id)) {
		DP_NOTICE(p_dev->hwfns, false, "device in CMT mode\n");
		p_dev->num_hwfns = 2;
	} else {
		p_dev->num_hwfns = 1;
	}

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_EMUL(p_dev)) {
		/* For some reason we have problems with this register
		 * in B0 emulation; Simply assume no CMT
		 */
		DP_NOTICE(p_dev->hwfns, false,
			  "device on emul - assume no CMT\n");
		p_dev->num_hwfns = 1;
	}
#endif

	p_dev->chip_bond_id = ecore_rd(p_hwfn, p_hwfn->p_main_ptt,
				       MISCS_REG_CHIP_TEST_REG) >> 4;
	MASK_FIELD(CHIP_BOND_ID, p_dev->chip_bond_id);
	p_dev->chip_metal = (u16)ecore_rd(p_hwfn, p_hwfn->p_main_ptt,
					  MISCS_REG_CHIP_METAL);
	MASK_FIELD(CHIP_METAL, p_dev->chip_metal);
	DP_INFO(p_dev->hwfns,
		"Chip details - %s%d, Num: %04x Rev: %04x Bond id: %04x"
		" Metal: %04x\n",
		ECORE_IS_BB(p_dev) ? "BB" : "AH",
		CHIP_REV_IS_A0(p_dev) ? 0 : 1,
		p_dev->chip_num, p_dev->chip_rev, p_dev->chip_bond_id,
		p_dev->chip_metal);

	if (ECORE_IS_BB(p_dev) && CHIP_REV_IS_A0(p_dev)) {
		DP_NOTICE(p_dev->hwfns, false,
			  "The chip type/rev (BB A0) is not supported!\n");
		return ECORE_ABORTED;
	}
#ifndef ASIC_ONLY
	if (CHIP_REV_IS_EMUL(p_dev) && ECORE_IS_AH(p_dev))
		ecore_wr(p_hwfn, p_hwfn->p_main_ptt,
			 MISCS_REG_PLL_MAIN_CTRL_4, 0x1);

	if (CHIP_REV_IS_EMUL(p_dev)) {
		tmp = ecore_rd(p_hwfn, p_hwfn->p_main_ptt,
			       MISCS_REG_ECO_RESERVED);
		if (tmp & (1 << 29)) {
			DP_NOTICE(p_hwfn, false,
				  "Emulation: Running on a FULL build\n");
			p_dev->b_is_emul_full = true;
		} else {
			DP_NOTICE(p_hwfn, false,
				  "Emulation: Running on a REDUCED build\n");
		}
	}
#endif

	return ECORE_SUCCESS;
}

void ecore_prepare_hibernate(struct ecore_dev *p_dev)
{
	int j;

	if (IS_VF(p_dev))
		return;

	for_each_hwfn(p_dev, j) {
		struct ecore_hwfn *p_hwfn = &p_dev->hwfns[j];

		DP_VERBOSE(p_hwfn, ECORE_MSG_IFDOWN,
			   "Mark hw/fw uninitialized\n");

		p_hwfn->hw_init_done = false;
		p_hwfn->first_on_engine = false;
	}
}

static enum _ecore_status_t
ecore_hw_prepare_single(struct ecore_hwfn *p_hwfn,
			void OSAL_IOMEM *p_regview,
			void OSAL_IOMEM *p_doorbells,
			enum ecore_pci_personality personality)
{
	enum _ecore_status_t rc = ECORE_SUCCESS;

	/* Split PCI bars evenly between hwfns */
	p_hwfn->regview = p_regview;
	p_hwfn->doorbells = p_doorbells;

	/* Validate that chip access is feasible */
	if (REG_RD(p_hwfn, PXP_PF_ME_OPAQUE_ADDR) == 0xffffffff) {
		DP_ERR(p_hwfn,
		       "Reading the ME register returns all Fs;"
		       " Preventing further chip access\n");
		return ECORE_INVAL;
	}

	get_function_id(p_hwfn);

	/* Allocate PTT pool */
	rc = ecore_ptt_pool_alloc(p_hwfn);
	if (rc) {
		DP_NOTICE(p_hwfn, true, "Failed to prepare hwfn's hw\n");
		goto err0;
	}

	/* Allocate the main PTT */
	p_hwfn->p_main_ptt = ecore_get_reserved_ptt(p_hwfn, RESERVED_PTT_MAIN);

	/* First hwfn learns basic information, e.g., number of hwfns */
	if (!p_hwfn->my_id) {
		rc = ecore_get_dev_info(p_hwfn->p_dev);
		if (rc != ECORE_SUCCESS)
			goto err1;
	}

	ecore_hw_hwfn_prepare(p_hwfn);

	/* Initialize MCP structure */
	rc = ecore_mcp_cmd_init(p_hwfn, p_hwfn->p_main_ptt);
	if (rc) {
		DP_NOTICE(p_hwfn, true, "Failed initializing mcp command\n");
		goto err1;
	}

	/* Read the device configuration information from the HW and SHMEM */
	rc = ecore_get_hw_info(p_hwfn, p_hwfn->p_main_ptt, personality);
	if (rc) {
		DP_NOTICE(p_hwfn, true, "Failed to get HW information\n");
		goto err2;
	}

	/* Allocate the init RT array and initialize the init-ops engine */
	rc = ecore_init_alloc(p_hwfn);
	if (rc) {
		DP_NOTICE(p_hwfn, true, "Failed to allocate the init array\n");
		goto err2;
	}
#ifndef ASIC_ONLY
	if (CHIP_REV_IS_FPGA(p_hwfn->p_dev)) {
		DP_NOTICE(p_hwfn, false,
			  "FPGA: workaround; Prevent DMAE parities\n");
		ecore_wr(p_hwfn, p_hwfn->p_main_ptt, PCIE_REG_PRTY_MASK, 7);

		DP_NOTICE(p_hwfn, false,
			  "FPGA: workaround: Set VF bar0 size\n");
		ecore_wr(p_hwfn, p_hwfn->p_main_ptt,
			 PGLUE_B_REG_VF_BAR0_SIZE, 4);
	}
#endif

	return rc;
err2:
	ecore_mcp_free(p_hwfn);
err1:
	ecore_hw_hwfn_free(p_hwfn);
err0:
	return rc;
}

enum _ecore_status_t ecore_hw_prepare(struct ecore_dev *p_dev, int personality)
{
	struct ecore_hwfn *p_hwfn = ECORE_LEADING_HWFN(p_dev);
	enum _ecore_status_t rc;

	if (IS_VF(p_dev))
		return ecore_vf_hw_prepare(p_dev);

	/* Store the precompiled init data ptrs */
	ecore_init_iro_array(p_dev);

	/* Initialize the first hwfn - will learn number of hwfns */
	rc = ecore_hw_prepare_single(p_hwfn,
				     p_dev->regview,
				     p_dev->doorbells, personality);
	if (rc != ECORE_SUCCESS)
		return rc;

	personality = p_hwfn->hw_info.personality;

	/* initialalize 2nd hwfn if necessary */
	if (p_dev->num_hwfns > 1) {
		void OSAL_IOMEM *p_regview, *p_doorbell;
		u8 OSAL_IOMEM *addr;

		/* adjust bar offset for second engine */
		addr = (u8 OSAL_IOMEM *)p_dev->regview +
		    ecore_hw_bar_size(p_hwfn, BAR_ID_0) / 2;
		p_regview = (void OSAL_IOMEM *)addr;

		addr = (u8 OSAL_IOMEM *)p_dev->doorbells +
		    ecore_hw_bar_size(p_hwfn, BAR_ID_1) / 2;
		p_doorbell = (void OSAL_IOMEM *)addr;

		/* prepare second hw function */
		rc = ecore_hw_prepare_single(&p_dev->hwfns[1], p_regview,
					     p_doorbell, personality);

		/* in case of error, need to free the previously
		 * initialiazed hwfn 0
		 */
		if (rc != ECORE_SUCCESS) {
			ecore_init_free(p_hwfn);
			ecore_mcp_free(p_hwfn);
			ecore_hw_hwfn_free(p_hwfn);
			return rc;
		}
	}

	return ECORE_SUCCESS;
}

void ecore_hw_remove(struct ecore_dev *p_dev)
{
	int i;

	for_each_hwfn(p_dev, i) {
		struct ecore_hwfn *p_hwfn = &p_dev->hwfns[i];

		if (IS_VF(p_dev)) {
			ecore_vf_pf_release(p_hwfn);
			continue;
		}

		ecore_init_free(p_hwfn);
		ecore_hw_hwfn_free(p_hwfn);
		ecore_mcp_free(p_hwfn);

		OSAL_MUTEX_DEALLOC(&p_hwfn->dmae_info.mutex);
	}
}

static void ecore_chain_free_next_ptr(struct ecore_dev *p_dev,
				      struct ecore_chain *p_chain)
{
	void *p_virt = p_chain->p_virt_addr, *p_virt_next = OSAL_NULL;
	dma_addr_t p_phys = p_chain->p_phys_addr, p_phys_next = 0;
	struct ecore_chain_next *p_next;
	u32 size, i;

	if (!p_virt)
		return;

	size = p_chain->elem_size * p_chain->usable_per_page;

	for (i = 0; i < p_chain->page_cnt; i++) {
		if (!p_virt)
			break;

		p_next = (struct ecore_chain_next *)((u8 *)p_virt + size);
		p_virt_next = p_next->next_virt;
		p_phys_next = HILO_DMA_REGPAIR(p_next->next_phys);

		OSAL_DMA_FREE_COHERENT(p_dev, p_virt, p_phys,
				       ECORE_CHAIN_PAGE_SIZE);

		p_virt = p_virt_next;
		p_phys = p_phys_next;
	}
}

static void ecore_chain_free_single(struct ecore_dev *p_dev,
				    struct ecore_chain *p_chain)
{
	if (!p_chain->p_virt_addr)
		return;

	OSAL_DMA_FREE_COHERENT(p_dev, p_chain->p_virt_addr,
			       p_chain->p_phys_addr, ECORE_CHAIN_PAGE_SIZE);
}

static void ecore_chain_free_pbl(struct ecore_dev *p_dev,
				 struct ecore_chain *p_chain)
{
	void **pp_virt_addr_tbl = p_chain->pbl.pp_virt_addr_tbl;
	u8 *p_pbl_virt = (u8 *)p_chain->pbl.p_virt_table;
	u32 page_cnt = p_chain->page_cnt, i, pbl_size;

	if (!pp_virt_addr_tbl)
		return;

	if (!p_chain->pbl.p_virt_table)
		goto out;

	for (i = 0; i < page_cnt; i++) {
		if (!pp_virt_addr_tbl[i])
			break;

		OSAL_DMA_FREE_COHERENT(p_dev, pp_virt_addr_tbl[i],
				       *(dma_addr_t *)p_pbl_virt,
				       ECORE_CHAIN_PAGE_SIZE);

		p_pbl_virt += ECORE_CHAIN_PBL_ENTRY_SIZE;
	}

	pbl_size = page_cnt * ECORE_CHAIN_PBL_ENTRY_SIZE;
	OSAL_DMA_FREE_COHERENT(p_dev, p_chain->pbl.p_virt_table,
			       p_chain->pbl.p_phys_table, pbl_size);
out:
	OSAL_VFREE(p_dev, p_chain->pbl.pp_virt_addr_tbl);
}

void ecore_chain_free(struct ecore_dev *p_dev, struct ecore_chain *p_chain)
{
	switch (p_chain->mode) {
	case ECORE_CHAIN_MODE_NEXT_PTR:
		ecore_chain_free_next_ptr(p_dev, p_chain);
		break;
	case ECORE_CHAIN_MODE_SINGLE:
		ecore_chain_free_single(p_dev, p_chain);
		break;
	case ECORE_CHAIN_MODE_PBL:
		ecore_chain_free_pbl(p_dev, p_chain);
		break;
	}
}

static enum _ecore_status_t
ecore_chain_alloc_sanity_check(struct ecore_dev *p_dev,
			       enum ecore_chain_cnt_type cnt_type,
			       osal_size_t elem_size, u32 page_cnt)
{
	u64 chain_size = ELEMS_PER_PAGE(elem_size) * page_cnt;

	/* The actual chain size can be larger than the maximal possible value
	 * after rounding up the requested elements number to pages, and after
	 * taking into acount the unusuable elements (next-ptr elements).
	 * The size of a "u16" chain can be (U16_MAX + 1) since the chain
	 * size/capacity fields are of a u32 type.
	 */
	if ((cnt_type == ECORE_CHAIN_CNT_TYPE_U16 &&
	     chain_size > ((u32)ECORE_U16_MAX + 1)) ||
	    (cnt_type == ECORE_CHAIN_CNT_TYPE_U32 &&
	     chain_size > ECORE_U32_MAX)) {
		DP_NOTICE(p_dev, true,
			  "The actual chain size (0x%lx) is larger than"
			  " the maximal possible value\n",
			  (unsigned long)chain_size);
		return ECORE_INVAL;
	}

	return ECORE_SUCCESS;
}

static enum _ecore_status_t
ecore_chain_alloc_next_ptr(struct ecore_dev *p_dev, struct ecore_chain *p_chain)
{
	void *p_virt = OSAL_NULL, *p_virt_prev = OSAL_NULL;
	dma_addr_t p_phys = 0;
	u32 i;

	for (i = 0; i < p_chain->page_cnt; i++) {
		p_virt = OSAL_DMA_ALLOC_COHERENT(p_dev, &p_phys,
						 ECORE_CHAIN_PAGE_SIZE);
		if (!p_virt) {
			DP_NOTICE(p_dev, true,
				  "Failed to allocate chain memory\n");
			return ECORE_NOMEM;
		}

		if (i == 0) {
			ecore_chain_init_mem(p_chain, p_virt, p_phys);
			ecore_chain_reset(p_chain);
		} else {
			ecore_chain_init_next_ptr_elem(p_chain, p_virt_prev,
						       p_virt, p_phys);
		}

		p_virt_prev = p_virt;
	}
	/* Last page's next element should point to the beginning of the
	 * chain.
	 */
	ecore_chain_init_next_ptr_elem(p_chain, p_virt_prev,
				       p_chain->p_virt_addr,
				       p_chain->p_phys_addr);

	return ECORE_SUCCESS;
}

static enum _ecore_status_t
ecore_chain_alloc_single(struct ecore_dev *p_dev, struct ecore_chain *p_chain)
{
	void *p_virt = OSAL_NULL;
	dma_addr_t p_phys = 0;

	p_virt = OSAL_DMA_ALLOC_COHERENT(p_dev, &p_phys, ECORE_CHAIN_PAGE_SIZE);
	if (!p_virt) {
		DP_NOTICE(p_dev, true, "Failed to allocate chain memory\n");
		return ECORE_NOMEM;
	}

	ecore_chain_init_mem(p_chain, p_virt, p_phys);
	ecore_chain_reset(p_chain);

	return ECORE_SUCCESS;
}

static enum _ecore_status_t ecore_chain_alloc_pbl(struct ecore_dev *p_dev,
						  struct ecore_chain *p_chain)
{
	void *p_virt = OSAL_NULL;
	u8 *p_pbl_virt = OSAL_NULL;
	void **pp_virt_addr_tbl = OSAL_NULL;
	dma_addr_t p_phys = 0, p_pbl_phys = 0;
	u32 page_cnt = p_chain->page_cnt, size, i;

	size = page_cnt * sizeof(*pp_virt_addr_tbl);
	pp_virt_addr_tbl = (void **)OSAL_VALLOC(p_dev, size);
	if (!pp_virt_addr_tbl) {
		DP_NOTICE(p_dev, true,
			  "Failed to allocate memory for the chain"
			  " virtual addresses table\n");
		return ECORE_NOMEM;
	}
	OSAL_MEM_ZERO(pp_virt_addr_tbl, size);

	/* The allocation of the PBL table is done with its full size, since it
	 * is expected to be successive.
	 */
	size = page_cnt * ECORE_CHAIN_PBL_ENTRY_SIZE;
	p_pbl_virt = OSAL_DMA_ALLOC_COHERENT(p_dev, &p_pbl_phys, size);
	if (!p_pbl_virt) {
		DP_NOTICE(p_dev, true, "Failed to allocate chain pbl memory\n");
		return ECORE_NOMEM;
	}

	ecore_chain_init_pbl_mem(p_chain, p_pbl_virt, p_pbl_phys,
				 pp_virt_addr_tbl);

	for (i = 0; i < page_cnt; i++) {
		p_virt = OSAL_DMA_ALLOC_COHERENT(p_dev, &p_phys,
						 ECORE_CHAIN_PAGE_SIZE);
		if (!p_virt) {
			DP_NOTICE(p_dev, true,
				  "Failed to allocate chain memory\n");
			return ECORE_NOMEM;
		}

		if (i == 0) {
			ecore_chain_init_mem(p_chain, p_virt, p_phys);
			ecore_chain_reset(p_chain);
		}

		/* Fill the PBL table with the physical address of the page */
		*(dma_addr_t *)p_pbl_virt = p_phys;
		/* Keep the virtual address of the page */
		p_chain->pbl.pp_virt_addr_tbl[i] = p_virt;

		p_pbl_virt += ECORE_CHAIN_PBL_ENTRY_SIZE;
	}

	return ECORE_SUCCESS;
}

enum _ecore_status_t ecore_chain_alloc(struct ecore_dev *p_dev,
				       enum ecore_chain_use_mode intended_use,
				       enum ecore_chain_mode mode,
				       enum ecore_chain_cnt_type cnt_type,
				       u32 num_elems, osal_size_t elem_size,
				       struct ecore_chain *p_chain)
{
	u32 page_cnt;
	enum _ecore_status_t rc = ECORE_SUCCESS;

	if (mode == ECORE_CHAIN_MODE_SINGLE)
		page_cnt = 1;
	else
		page_cnt = ECORE_CHAIN_PAGE_CNT(num_elems, elem_size, mode);

	rc = ecore_chain_alloc_sanity_check(p_dev, cnt_type, elem_size,
					    page_cnt);
	if (rc) {
		DP_NOTICE(p_dev, true,
			  "Cannot allocate a chain with the given arguments:\n"
			  " [use_mode %d, mode %d, cnt_type %d, num_elems %d,"
			  " elem_size %zu]\n",
			  intended_use, mode, cnt_type, num_elems, elem_size);
		return rc;
	}

	ecore_chain_init_params(p_chain, page_cnt, (u8)elem_size, intended_use,
				mode, cnt_type);

	switch (mode) {
	case ECORE_CHAIN_MODE_NEXT_PTR:
		rc = ecore_chain_alloc_next_ptr(p_dev, p_chain);
		break;
	case ECORE_CHAIN_MODE_SINGLE:
		rc = ecore_chain_alloc_single(p_dev, p_chain);
		break;
	case ECORE_CHAIN_MODE_PBL:
		rc = ecore_chain_alloc_pbl(p_dev, p_chain);
		break;
	}
	if (rc)
		goto nomem;

	return ECORE_SUCCESS;

nomem:
	ecore_chain_free(p_dev, p_chain);
	return rc;
}

enum _ecore_status_t ecore_fw_l2_queue(struct ecore_hwfn *p_hwfn,
				       u16 src_id, u16 *dst_id)
{
	if (src_id >= RESC_NUM(p_hwfn, ECORE_L2_QUEUE)) {
		u16 min, max;

		min = (u16)RESC_START(p_hwfn, ECORE_L2_QUEUE);
		max = min + RESC_NUM(p_hwfn, ECORE_L2_QUEUE);
		DP_NOTICE(p_hwfn, true,
			  "l2_queue id [%d] is not valid, available"
			  " indices [%d - %d]\n",
			  src_id, min, max);

		return ECORE_INVAL;
	}

	*dst_id = RESC_START(p_hwfn, ECORE_L2_QUEUE) + src_id;

	return ECORE_SUCCESS;
}

enum _ecore_status_t ecore_fw_vport(struct ecore_hwfn *p_hwfn,
				    u8 src_id, u8 *dst_id)
{
	if (src_id >= RESC_NUM(p_hwfn, ECORE_VPORT)) {
		u8 min, max;

		min = (u8)RESC_START(p_hwfn, ECORE_VPORT);
		max = min + RESC_NUM(p_hwfn, ECORE_VPORT);
		DP_NOTICE(p_hwfn, true,
			  "vport id [%d] is not valid, available"
			  " indices [%d - %d]\n",
			  src_id, min, max);

		return ECORE_INVAL;
	}

	*dst_id = RESC_START(p_hwfn, ECORE_VPORT) + src_id;

	return ECORE_SUCCESS;
}

enum _ecore_status_t ecore_fw_rss_eng(struct ecore_hwfn *p_hwfn,
				      u8 src_id, u8 *dst_id)
{
	if (src_id >= RESC_NUM(p_hwfn, ECORE_RSS_ENG)) {
		u8 min, max;

		min = (u8)RESC_START(p_hwfn, ECORE_RSS_ENG);
		max = min + RESC_NUM(p_hwfn, ECORE_RSS_ENG);
		DP_NOTICE(p_hwfn, true,
			  "rss_eng id [%d] is not valid,avail idx [%d - %d]\n",
			  src_id, min, max);

		return ECORE_INVAL;
	}

	*dst_id = RESC_START(p_hwfn, ECORE_RSS_ENG) + src_id;

	return ECORE_SUCCESS;
}

enum _ecore_status_t ecore_llh_add_mac_filter(struct ecore_hwfn *p_hwfn,
					      struct ecore_ptt *p_ptt,
					      u8 *p_filter)
{
	u32 high, low, en;
	int i;

	if (!(IS_MF_SI(p_hwfn) || IS_MF_DEFAULT(p_hwfn)))
		return ECORE_SUCCESS;

	high = p_filter[1] | (p_filter[0] << 8);
	low = p_filter[5] | (p_filter[4] << 8) |
	    (p_filter[3] << 16) | (p_filter[2] << 24);

	/* Find a free entry and utilize it */
	for (i = 0; i < NIG_REG_LLH_FUNC_FILTER_EN_SIZE; i++) {
		en = ecore_rd(p_hwfn, p_ptt,
			      NIG_REG_LLH_FUNC_FILTER_EN + i * sizeof(u32));
		if (en)
			continue;
		ecore_wr(p_hwfn, p_ptt,
			 NIG_REG_LLH_FUNC_FILTER_VALUE +
			 2 * i * sizeof(u32), low);
		ecore_wr(p_hwfn, p_ptt,
			 NIG_REG_LLH_FUNC_FILTER_VALUE +
			 (2 * i + 1) * sizeof(u32), high);
		ecore_wr(p_hwfn, p_ptt,
			 NIG_REG_LLH_FUNC_FILTER_MODE + i * sizeof(u32), 0);
		ecore_wr(p_hwfn, p_ptt,
			 NIG_REG_LLH_FUNC_FILTER_PROTOCOL_TYPE +
			 i * sizeof(u32), 0);
		ecore_wr(p_hwfn, p_ptt,
			 NIG_REG_LLH_FUNC_FILTER_EN + i * sizeof(u32), 1);
		break;
	}
	if (i >= NIG_REG_LLH_FUNC_FILTER_EN_SIZE) {
		DP_NOTICE(p_hwfn, false,
			  "Failed to find an empty LLH filter to utilize\n");
		return ECORE_INVAL;
	}

	DP_VERBOSE(p_hwfn, ECORE_MSG_HW,
		   "MAC: %x:%x:%x:%x:%x:%x is added at %d\n",
		   p_filter[0], p_filter[1], p_filter[2],
		   p_filter[3], p_filter[4], p_filter[5], i);

	return ECORE_SUCCESS;
}

void ecore_llh_remove_mac_filter(struct ecore_hwfn *p_hwfn,
				 struct ecore_ptt *p_ptt, u8 *p_filter)
{
	u32 high, low;
	int i;

	if (!(IS_MF_SI(p_hwfn) || IS_MF_DEFAULT(p_hwfn)))
		return;

	high = p_filter[1] | (p_filter[0] << 8);
	low = p_filter[5] | (p_filter[4] << 8) |
	    (p_filter[3] << 16) | (p_filter[2] << 24);

	/* Find the entry and clean it */
	for (i = 0; i < NIG_REG_LLH_FUNC_FILTER_EN_SIZE; i++) {
		if (ecore_rd(p_hwfn, p_ptt,
			     NIG_REG_LLH_FUNC_FILTER_VALUE +
			     2 * i * sizeof(u32)) != low)
			continue;
		if (ecore_rd(p_hwfn, p_ptt,
			     NIG_REG_LLH_FUNC_FILTER_VALUE +
			     (2 * i + 1) * sizeof(u32)) != high)
			continue;

		ecore_wr(p_hwfn, p_ptt,
			 NIG_REG_LLH_FUNC_FILTER_EN + i * sizeof(u32), 0);
		ecore_wr(p_hwfn, p_ptt,
			 NIG_REG_LLH_FUNC_FILTER_VALUE +
			 2 * i * sizeof(u32), 0);
		ecore_wr(p_hwfn, p_ptt,
			 NIG_REG_LLH_FUNC_FILTER_VALUE +
			 (2 * i + 1) * sizeof(u32), 0);
		break;
	}
	if (i >= NIG_REG_LLH_FUNC_FILTER_EN_SIZE)
		DP_NOTICE(p_hwfn, false,
			  "Tried to remove a non-configured filter\n");
}

enum _ecore_status_t ecore_llh_add_ethertype_filter(struct ecore_hwfn *p_hwfn,
						    struct ecore_ptt *p_ptt,
						    u16 filter)
{
	u32 high, low, en;
	int i;

	if (!(IS_MF_SI(p_hwfn) || IS_MF_DEFAULT(p_hwfn)))
		return ECORE_SUCCESS;

	high = filter;
	low = 0;

	/* Find a free entry and utilize it */
	for (i = 0; i < NIG_REG_LLH_FUNC_FILTER_EN_SIZE; i++) {
		en = ecore_rd(p_hwfn, p_ptt,
			      NIG_REG_LLH_FUNC_FILTER_EN + i * sizeof(u32));
		if (en)
			continue;
		ecore_wr(p_hwfn, p_ptt,
			 NIG_REG_LLH_FUNC_FILTER_VALUE +
			 2 * i * sizeof(u32), low);
		ecore_wr(p_hwfn, p_ptt,
			 NIG_REG_LLH_FUNC_FILTER_VALUE +
			 (2 * i + 1) * sizeof(u32), high);
		ecore_wr(p_hwfn, p_ptt,
			 NIG_REG_LLH_FUNC_FILTER_MODE + i * sizeof(u32), 1);
		ecore_wr(p_hwfn, p_ptt,
			 NIG_REG_LLH_FUNC_FILTER_PROTOCOL_TYPE +
			 i * sizeof(u32), 1);
		ecore_wr(p_hwfn, p_ptt,
			 NIG_REG_LLH_FUNC_FILTER_EN + i * sizeof(u32), 1);
		break;
	}
	if (i >= NIG_REG_LLH_FUNC_FILTER_EN_SIZE) {
		DP_NOTICE(p_hwfn, false,
			  "Failed to find an empty LLH filter to utilize\n");
		return ECORE_INVAL;
	}

	DP_VERBOSE(p_hwfn, ECORE_MSG_HW,
		   "ETH type: %x is added at %d\n", filter, i);

	return ECORE_SUCCESS;
}

void ecore_llh_remove_ethertype_filter(struct ecore_hwfn *p_hwfn,
				       struct ecore_ptt *p_ptt, u16 filter)
{
	u32 high, low;
	int i;

	if (!(IS_MF_SI(p_hwfn) || IS_MF_DEFAULT(p_hwfn)))
		return;

	high = filter;
	low = 0;

	/* Find the entry and clean it */
	for (i = 0; i < NIG_REG_LLH_FUNC_FILTER_EN_SIZE; i++) {
		if (ecore_rd(p_hwfn, p_ptt,
			     NIG_REG_LLH_FUNC_FILTER_VALUE +
			     2 * i * sizeof(u32)) != low)
			continue;
		if (ecore_rd(p_hwfn, p_ptt,
			     NIG_REG_LLH_FUNC_FILTER_VALUE +
			     (2 * i + 1) * sizeof(u32)) != high)
			continue;

		ecore_wr(p_hwfn, p_ptt,
			 NIG_REG_LLH_FUNC_FILTER_EN + i * sizeof(u32), 0);
		ecore_wr(p_hwfn, p_ptt,
			 NIG_REG_LLH_FUNC_FILTER_VALUE +
			 2 * i * sizeof(u32), 0);
		ecore_wr(p_hwfn, p_ptt,
			 NIG_REG_LLH_FUNC_FILTER_VALUE +
			 (2 * i + 1) * sizeof(u32), 0);
		break;
	}
	if (i >= NIG_REG_LLH_FUNC_FILTER_EN_SIZE)
		DP_NOTICE(p_hwfn, false,
			  "Tried to remove a non-configured filter\n");
}

void ecore_llh_clear_all_filters(struct ecore_hwfn *p_hwfn,
				 struct ecore_ptt *p_ptt)
{
	int i;

	if (!(IS_MF_SI(p_hwfn) || IS_MF_DEFAULT(p_hwfn)))
		return;

	for (i = 0; i < NIG_REG_LLH_FUNC_FILTER_EN_SIZE; i++) {
		ecore_wr(p_hwfn, p_ptt,
			 NIG_REG_LLH_FUNC_FILTER_EN + i * sizeof(u32), 0);
		ecore_wr(p_hwfn, p_ptt,
			 NIG_REG_LLH_FUNC_FILTER_VALUE +
			 2 * i * sizeof(u32), 0);
		ecore_wr(p_hwfn, p_ptt,
			 NIG_REG_LLH_FUNC_FILTER_VALUE +
			 (2 * i + 1) * sizeof(u32), 0);
	}
}

enum _ecore_status_t ecore_test_registers(struct ecore_hwfn *p_hwfn,
					  struct ecore_ptt *p_ptt)
{
	u32 reg_tbl[] = {
		BRB_REG_HEADER_SIZE,
		BTB_REG_HEADER_SIZE,
		CAU_REG_LONG_TIMEOUT_THRESHOLD,
		CCFC_REG_ACTIVITY_COUNTER,
		CDU_REG_CID_ADDR_PARAMS,
		DBG_REG_CLIENT_ENABLE,
		DMAE_REG_INIT,
		DORQ_REG_IFEN,
		GRC_REG_TIMEOUT_EN,
		IGU_REG_BLOCK_CONFIGURATION,
		MCM_REG_INIT,
		MCP2_REG_DBG_DWORD_ENABLE,
		MISC_REG_PORT_MODE,
		MISCS_REG_CLK_100G_MODE,
		MSDM_REG_ENABLE_IN1,
		MSEM_REG_ENABLE_IN,
		NIG_REG_CM_HDR,
		NCSI_REG_CONFIG,
		PBF_REG_INIT,
		PTU_REG_ATC_INIT_ARRAY,
		PCM_REG_INIT,
		PGLUE_B_REG_ADMIN_PER_PF_REGION,
		PRM_REG_DISABLE_PRM,
		PRS_REG_SOFT_RST,
		PSDM_REG_ENABLE_IN1,
		PSEM_REG_ENABLE_IN,
		PSWRQ_REG_DBG_SELECT,
		PSWRQ2_REG_CDUT_P_SIZE,
		PSWHST_REG_DISCARD_INTERNAL_WRITES,
		PSWHST2_REG_DBGSYN_ALMOST_FULL_THR,
		PSWRD_REG_DBG_SELECT,
		PSWRD2_REG_CONF11,
		PSWWR_REG_USDM_FULL_TH,
		PSWWR2_REG_CDU_FULL_TH2,
		QM_REG_MAXPQSIZE_0,
		RSS_REG_RSS_INIT_EN,
		RDIF_REG_STOP_ON_ERROR,
		SRC_REG_SOFT_RST,
		TCFC_REG_ACTIVITY_COUNTER,
		TCM_REG_INIT,
		TM_REG_PXP_READ_DATA_FIFO_INIT,
		TSDM_REG_ENABLE_IN1,
		TSEM_REG_ENABLE_IN,
		TDIF_REG_STOP_ON_ERROR,
		UCM_REG_INIT,
		UMAC_REG_IPG_HD_BKP_CNTL_BB_B0,
		USDM_REG_ENABLE_IN1,
		USEM_REG_ENABLE_IN,
		XCM_REG_INIT,
		XSDM_REG_ENABLE_IN1,
		XSEM_REG_ENABLE_IN,
		YCM_REG_INIT,
		YSDM_REG_ENABLE_IN1,
		YSEM_REG_ENABLE_IN,
		XYLD_REG_SCBD_STRICT_PRIO,
		TMLD_REG_SCBD_STRICT_PRIO,
		MULD_REG_SCBD_STRICT_PRIO,
		YULD_REG_SCBD_STRICT_PRIO,
	};
	u32 test_val[] = { 0x0, 0x1 };
	u32 val, save_val, i, j;

	for (i = 0; i < OSAL_ARRAY_SIZE(test_val); i++) {
		for (j = 0; j < OSAL_ARRAY_SIZE(reg_tbl); j++) {
			save_val = ecore_rd(p_hwfn, p_ptt, reg_tbl[j]);
			ecore_wr(p_hwfn, p_ptt, reg_tbl[j], test_val[i]);
			val = ecore_rd(p_hwfn, p_ptt, reg_tbl[j]);
			/* Restore the original register's value */
			ecore_wr(p_hwfn, p_ptt, reg_tbl[j], save_val);
			if (val != test_val[i]) {
				DP_INFO(p_hwfn->p_dev,
					"offset 0x%x: val 0x%x != 0x%x\n",
					reg_tbl[j], val, test_val[i]);
				return ECORE_AGAIN;
			}
		}
	}
	return ECORE_SUCCESS;
}

static enum _ecore_status_t ecore_set_coalesce(struct ecore_hwfn *p_hwfn,
					       struct ecore_ptt *p_ptt,
					       u32 hw_addr, void *p_qzone,
					       osal_size_t qzone_size,
					       u8 timeset)
{
	struct coalescing_timeset *p_coalesce_timeset;

	if (IS_VF(p_hwfn->p_dev)) {
		DP_NOTICE(p_hwfn, true, "VF coalescing config not supported\n");
		return ECORE_INVAL;
	}

	if (p_hwfn->p_dev->int_coalescing_mode != ECORE_COAL_MODE_ENABLE) {
		DP_NOTICE(p_hwfn, true,
			  "Coalescing configuration not enabled\n");
		return ECORE_INVAL;
	}

	OSAL_MEMSET(p_qzone, 0, qzone_size);
	p_coalesce_timeset = p_qzone;
	p_coalesce_timeset->timeset = timeset;
	p_coalesce_timeset->valid = 1;
	ecore_memcpy_to(p_hwfn, p_ptt, hw_addr, p_qzone, qzone_size);

	return ECORE_SUCCESS;
}

enum _ecore_status_t ecore_set_rxq_coalesce(struct ecore_hwfn *p_hwfn,
					    struct ecore_ptt *p_ptt,
					    u8 coalesce, u8 qid)
{
	struct ustorm_eth_queue_zone qzone;
	u16 fw_qid = 0;
	u32 address;
	u8 timeset;
	enum _ecore_status_t rc;

	rc = ecore_fw_l2_queue(p_hwfn, (u16)qid, &fw_qid);
	if (rc != ECORE_SUCCESS)
		return rc;

	address = BAR0_MAP_REG_USDM_RAM + USTORM_ETH_QUEUE_ZONE_OFFSET(fw_qid);
	/* Translate the coalescing time into a timeset, according to:
	 * Timeout[Rx] = TimeSet[Rx] << (TimerRes[Rx] + 1)
	 */
	timeset = coalesce >> (ECORE_CAU_DEF_RX_TIMER_RES + 1);

	rc = ecore_set_coalesce(p_hwfn, p_ptt, address, &qzone,
				sizeof(struct ustorm_eth_queue_zone), timeset);
	if (rc != ECORE_SUCCESS)
		goto out;

	p_hwfn->p_dev->rx_coalesce_usecs = coalesce;
out:
	return rc;
}

enum _ecore_status_t ecore_set_txq_coalesce(struct ecore_hwfn *p_hwfn,
					    struct ecore_ptt *p_ptt,
					    u8 coalesce, u8 qid)
{
	struct ystorm_eth_queue_zone qzone;
	u16 fw_qid = 0;
	u32 address;
	u8 timeset;
	enum _ecore_status_t rc;

	rc = ecore_fw_l2_queue(p_hwfn, (u16)qid, &fw_qid);
	if (rc != ECORE_SUCCESS)
		return rc;

	address = BAR0_MAP_REG_YSDM_RAM + YSTORM_ETH_QUEUE_ZONE_OFFSET(fw_qid);
	/* Translate the coalescing time into a timeset, according to:
	 * Timeout[Tx] = TimeSet[Tx] << (TimerRes[Tx] + 1)
	 */
	timeset = coalesce >> (ECORE_CAU_DEF_TX_TIMER_RES + 1);

	rc = ecore_set_coalesce(p_hwfn, p_ptt, address, &qzone,
				sizeof(struct ystorm_eth_queue_zone), timeset);
	if (rc != ECORE_SUCCESS)
		goto out;

	p_hwfn->p_dev->tx_coalesce_usecs = coalesce;
out:
	return rc;
}

/* Calculate final WFQ values for all vports and configure it.
 * After this configuration each vport must have
 * approx min rate =  vport_wfq * min_pf_rate / ECORE_WFQ_UNIT
 */
static void ecore_configure_wfq_for_all_vports(struct ecore_hwfn *p_hwfn,
					       struct ecore_ptt *p_ptt,
					       u32 min_pf_rate)
{
	struct init_qm_vport_params *vport_params;
	int i, num_vports;

	vport_params = p_hwfn->qm_info.qm_vport_params;
	num_vports = p_hwfn->qm_info.num_vports;

	for (i = 0; i < num_vports; i++) {
		u32 wfq_speed = p_hwfn->qm_info.wfq_data[i].min_speed;

		vport_params[i].vport_wfq =
		    (wfq_speed * ECORE_WFQ_UNIT) / min_pf_rate;
		ecore_init_vport_wfq(p_hwfn, p_ptt,
				     vport_params[i].first_tx_pq_id,
				     vport_params[i].vport_wfq);
	}
}

static void
ecore_init_wfq_default_param(struct ecore_hwfn *p_hwfn, u32 min_pf_rate)
{
	int i, num_vports;
	u32 min_speed;

	num_vports = p_hwfn->qm_info.num_vports;
	min_speed = min_pf_rate / num_vports;

	for (i = 0; i < num_vports; i++) {
		p_hwfn->qm_info.qm_vport_params[i].vport_wfq = 1;
		p_hwfn->qm_info.wfq_data[i].default_min_speed = min_speed;
	}
}

static void ecore_disable_wfq_for_all_vports(struct ecore_hwfn *p_hwfn,
					     struct ecore_ptt *p_ptt,
					     u32 min_pf_rate)
{
	struct init_qm_vport_params *vport_params;
	int i, num_vports;

	vport_params = p_hwfn->qm_info.qm_vport_params;
	num_vports = p_hwfn->qm_info.num_vports;

	for (i = 0; i < num_vports; i++) {
		ecore_init_wfq_default_param(p_hwfn, min_pf_rate);
		ecore_init_vport_wfq(p_hwfn, p_ptt,
				     vport_params[i].first_tx_pq_id,
				     vport_params[i].vport_wfq);
	}
}

/* validate wfq for a given vport and required min rate */
static enum _ecore_status_t ecore_init_wfq_param(struct ecore_hwfn *p_hwfn,
						 u16 vport_id, u32 req_rate,
						 u32 min_pf_rate)
{
	u32 total_req_min_rate = 0, total_left_rate = 0, left_rate_per_vp = 0;
	int non_requested_count = 0, req_count = 0, i, num_vports;

	num_vports = p_hwfn->qm_info.num_vports;

	/* Check pre-set data for some of the vports */
	for (i = 0; i < num_vports; i++) {
		u32 tmp_speed;

		if ((i != vport_id) && p_hwfn->qm_info.wfq_data[i].configured) {
			req_count++;
			tmp_speed = p_hwfn->qm_info.wfq_data[i].min_speed;
			total_req_min_rate += tmp_speed;
		}
	}

	/* Include current vport data as well */
	req_count++;
	total_req_min_rate += req_rate;
	non_requested_count = p_hwfn->qm_info.num_vports - req_count;

	/* validate possible error cases */
	if (req_rate > min_pf_rate) {
		DP_VERBOSE(p_hwfn, ECORE_MSG_LINK,
			   "Vport [%d] - Requested rate[%d Mbps] is greater"
			   " than configured PF min rate[%d Mbps]\n",
			   vport_id, req_rate, min_pf_rate);
		return ECORE_INVAL;
	}

	if (req_rate * ECORE_WFQ_UNIT / min_pf_rate < 1) {
		DP_VERBOSE(p_hwfn, ECORE_MSG_LINK,
			   "Vport [%d] - Requested rate[%d Mbps] is less than"
			   " one percent of configured PF min rate[%d Mbps]\n",
			   vport_id, req_rate, min_pf_rate);
		return ECORE_INVAL;
	}

	/* TBD - for number of vports greater than 100 */
	if (ECORE_WFQ_UNIT / p_hwfn->qm_info.num_vports < 1) {
		DP_VERBOSE(p_hwfn, ECORE_MSG_LINK,
			   "Number of vports are greater than 100\n");
		return ECORE_INVAL;
	}

	if (total_req_min_rate > min_pf_rate) {
		DP_VERBOSE(p_hwfn, ECORE_MSG_LINK,
			   "Total requested min rate for all vports[%d Mbps]"
			   "is greater than configured PF min rate[%d Mbps]\n",
			   total_req_min_rate, min_pf_rate);
		return ECORE_INVAL;
	}

	/* Data left for non requested vports */
	total_left_rate = min_pf_rate - total_req_min_rate;
	left_rate_per_vp = total_left_rate / non_requested_count;

	/* validate if non requested get < 1% of min bw */
	if (left_rate_per_vp * ECORE_WFQ_UNIT / min_pf_rate < 1)
		return ECORE_INVAL;

	/* now req_rate for given vport passes all scenarios.
	 * assign final wfq rates to all vports.
	 */
	p_hwfn->qm_info.wfq_data[vport_id].min_speed = req_rate;
	p_hwfn->qm_info.wfq_data[vport_id].configured = true;

	for (i = 0; i < num_vports; i++) {
		if (p_hwfn->qm_info.wfq_data[i].configured)
			continue;

		p_hwfn->qm_info.wfq_data[i].min_speed = left_rate_per_vp;
	}

	return ECORE_SUCCESS;
}

static int __ecore_configure_vport_wfq(struct ecore_hwfn *p_hwfn,
				       struct ecore_ptt *p_ptt,
				       u16 vp_id, u32 rate)
{
	struct ecore_mcp_link_state *p_link;
	int rc = ECORE_SUCCESS;

	p_link = &p_hwfn->p_dev->hwfns[0].mcp_info->link_output;

	if (!p_link->min_pf_rate) {
		p_hwfn->qm_info.wfq_data[vp_id].min_speed = rate;
		p_hwfn->qm_info.wfq_data[vp_id].configured = true;
		return rc;
	}

	rc = ecore_init_wfq_param(p_hwfn, vp_id, rate, p_link->min_pf_rate);

	if (rc == ECORE_SUCCESS)
		ecore_configure_wfq_for_all_vports(p_hwfn, p_ptt,
						   p_link->min_pf_rate);
	else
		DP_NOTICE(p_hwfn, false,
			  "Validation failed while configuring min rate\n");

	return rc;
}

static int __ecore_configure_vp_wfq_on_link_change(struct ecore_hwfn *p_hwfn,
						   struct ecore_ptt *p_ptt,
						   u32 min_pf_rate)
{
	int rc = ECORE_SUCCESS;
	bool use_wfq = false;
	u16 i, num_vports;

	num_vports = p_hwfn->qm_info.num_vports;

	/* Validate all pre configured vports for wfq */
	for (i = 0; i < num_vports; i++) {
		if (p_hwfn->qm_info.wfq_data[i].configured) {
			u32 rate = p_hwfn->qm_info.wfq_data[i].min_speed;

			use_wfq = true;
			rc = ecore_init_wfq_param(p_hwfn, i, rate, min_pf_rate);
			if (rc == ECORE_INVAL) {
				DP_NOTICE(p_hwfn, false,
					  "Validation failed while"
					  " configuring min rate\n");
				break;
			}
		}
	}

	if (rc == ECORE_SUCCESS && use_wfq)
		ecore_configure_wfq_for_all_vports(p_hwfn, p_ptt, min_pf_rate);
	else
		ecore_disable_wfq_for_all_vports(p_hwfn, p_ptt, min_pf_rate);

	return rc;
}

/* Main API for ecore clients to configure vport min rate.
 * vp_id - vport id in PF Range[0 - (total_num_vports_per_pf - 1)]
 * rate - Speed in Mbps needs to be assigned to a given vport.
 */
int ecore_configure_vport_wfq(struct ecore_dev *p_dev, u16 vp_id, u32 rate)
{
	int i, rc = ECORE_INVAL;

	/* TBD - for multiple hardware functions - that is 100 gig */
	if (p_dev->num_hwfns > 1) {
		DP_NOTICE(p_dev, false,
			  "WFQ configuration is not supported for this dev\n");
		return rc;
	}

	for_each_hwfn(p_dev, i) {
		struct ecore_hwfn *p_hwfn = &p_dev->hwfns[i];
		struct ecore_ptt *p_ptt;

		p_ptt = ecore_ptt_acquire(p_hwfn);
		if (!p_ptt)
			return ECORE_TIMEOUT;

		rc = __ecore_configure_vport_wfq(p_hwfn, p_ptt, vp_id, rate);

		if (rc != ECORE_SUCCESS) {
			ecore_ptt_release(p_hwfn, p_ptt);
			return rc;
		}

		ecore_ptt_release(p_hwfn, p_ptt);
	}

	return rc;
}

/* API to configure WFQ from mcp link change */
void ecore_configure_vp_wfq_on_link_change(struct ecore_dev *p_dev,
					   u32 min_pf_rate)
{
	int i;

	/* TBD - for multiple hardware functions - that is 100 gig */
	if (p_dev->num_hwfns > 1) {
		DP_VERBOSE(p_dev, ECORE_MSG_LINK,
			   "WFQ configuration is not supported for this dev\n");
		return;
	}

	for_each_hwfn(p_dev, i) {
		struct ecore_hwfn *p_hwfn = &p_dev->hwfns[i];

		__ecore_configure_vp_wfq_on_link_change(p_hwfn,
							p_hwfn->p_dpc_ptt,
							min_pf_rate);
	}
}

int __ecore_configure_pf_max_bandwidth(struct ecore_hwfn *p_hwfn,
				       struct ecore_ptt *p_ptt,
				       struct ecore_mcp_link_state *p_link,
				       u8 max_bw)
{
	int rc = ECORE_SUCCESS;

	p_hwfn->mcp_info->func_info.bandwidth_max = max_bw;

	if (!p_link->line_speed)
		return rc;

	p_link->speed = (p_link->line_speed * max_bw) / 100;

	rc = ecore_init_pf_rl(p_hwfn, p_ptt, p_hwfn->rel_pf_id, p_link->speed);

	DP_VERBOSE(p_hwfn, ECORE_MSG_LINK,
		   "Configured MAX bandwidth to be %08x Mb/sec\n",
		   p_link->speed);

	return rc;
}

/* Main API to configure PF max bandwidth where bw range is [1 - 100] */
int ecore_configure_pf_max_bandwidth(struct ecore_dev *p_dev, u8 max_bw)
{
	int i, rc = ECORE_INVAL;

	if (max_bw < 1 || max_bw > 100) {
		DP_NOTICE(p_dev, false, "PF max bw valid range is [1-100]\n");
		return rc;
	}

	for_each_hwfn(p_dev, i) {
		struct ecore_hwfn *p_hwfn = &p_dev->hwfns[i];
		struct ecore_hwfn *p_lead = ECORE_LEADING_HWFN(p_dev);
		struct ecore_mcp_link_state *p_link;
		struct ecore_ptt *p_ptt;

		p_link = &p_lead->mcp_info->link_output;

		p_ptt = ecore_ptt_acquire(p_hwfn);
		if (!p_ptt)
			return ECORE_TIMEOUT;

		rc = __ecore_configure_pf_max_bandwidth(p_hwfn, p_ptt,
							p_link, max_bw);
		if (rc != ECORE_SUCCESS) {
			ecore_ptt_release(p_hwfn, p_ptt);
			return rc;
		}

		ecore_ptt_release(p_hwfn, p_ptt);
	}

	return rc;
}

int __ecore_configure_pf_min_bandwidth(struct ecore_hwfn *p_hwfn,
				       struct ecore_ptt *p_ptt,
				       struct ecore_mcp_link_state *p_link,
				       u8 min_bw)
{
	int rc = ECORE_SUCCESS;

	p_hwfn->mcp_info->func_info.bandwidth_min = min_bw;

	if (!p_link->line_speed)
		return rc;

	p_link->min_pf_rate = (p_link->line_speed * min_bw) / 100;

	rc = ecore_init_pf_wfq(p_hwfn, p_ptt, p_hwfn->rel_pf_id, min_bw);

	DP_VERBOSE(p_hwfn, ECORE_MSG_LINK,
		   "Configured MIN bandwidth to be %d Mb/sec\n",
		   p_link->min_pf_rate);

	return rc;
}

/* Main API to configure PF min bandwidth where bw range is [1-100] */
int ecore_configure_pf_min_bandwidth(struct ecore_dev *p_dev, u8 min_bw)
{
	int i, rc = ECORE_INVAL;

	if (min_bw < 1 || min_bw > 100) {
		DP_NOTICE(p_dev, false, "PF min bw valid range is [1-100]\n");
		return rc;
	}

	for_each_hwfn(p_dev, i) {
		struct ecore_hwfn *p_hwfn = &p_dev->hwfns[i];
		struct ecore_hwfn *p_lead = ECORE_LEADING_HWFN(p_dev);
		struct ecore_mcp_link_state *p_link;
		struct ecore_ptt *p_ptt;

		p_link = &p_lead->mcp_info->link_output;

		p_ptt = ecore_ptt_acquire(p_hwfn);
		if (!p_ptt)
			return ECORE_TIMEOUT;

		rc = __ecore_configure_pf_min_bandwidth(p_hwfn, p_ptt,
							p_link, min_bw);
		if (rc != ECORE_SUCCESS) {
			ecore_ptt_release(p_hwfn, p_ptt);
			return rc;
		}

		if (p_link->min_pf_rate) {
			u32 min_rate = p_link->min_pf_rate;

			rc = __ecore_configure_vp_wfq_on_link_change(p_hwfn,
								     p_ptt,
								     min_rate);
		}

		ecore_ptt_release(p_hwfn, p_ptt);
	}

	return rc;
}

void ecore_clean_wfq_db(struct ecore_hwfn *p_hwfn, struct ecore_ptt *p_ptt)
{
	struct ecore_mcp_link_state *p_link;

	p_link = &p_hwfn->mcp_info->link_output;

	if (p_link->min_pf_rate)
		ecore_disable_wfq_for_all_vports(p_hwfn, p_ptt,
						 p_link->min_pf_rate);

	OSAL_MEMSET(p_hwfn->qm_info.wfq_data, 0,
		    sizeof(*p_hwfn->qm_info.wfq_data) *
		    p_hwfn->qm_info.num_vports);
}

int ecore_device_num_engines(struct ecore_dev *p_dev)
{
	return ECORE_IS_BB(p_dev) ? 2 : 1;
}

int ecore_device_num_ports(struct ecore_dev *p_dev)
{
	/* in CMT always only one port */
	if (p_dev->num_hwfns > 1)
		return 1;

	return p_dev->num_ports_in_engines * ecore_device_num_engines(p_dev);
}
