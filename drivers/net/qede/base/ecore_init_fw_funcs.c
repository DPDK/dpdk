/*
 * Copyright (c) 2016 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.qede_pmd for copyright and licensing details.
 */

#include "bcm_osal.h"
#include "ecore_hw.h"
#include "ecore_init_ops.h"
#include "reg_addr.h"
#include "ecore_rt_defs.h"
#include "ecore_hsi_common.h"
#include "ecore_hsi_tools.h"
#include "ecore_init_fw_funcs.h"

/* @DPDK CmInterfaceEnum */
enum cm_interface_enum {
	MCM_SEC,
	MCM_PRI,
	UCM_SEC,
	UCM_PRI,
	TCM_SEC,
	TCM_PRI,
	YCM_SEC,
	YCM_PRI,
	XCM_SEC,
	XCM_PRI,
	NUM_OF_CM_INTERFACES
};
/* general constants */
#define QM_PQ_MEM_4KB(pq_size) \
(pq_size ? DIV_ROUND_UP((pq_size + 1) * QM_PQ_ELEMENT_SIZE, 0x1000) : 0)
#define QM_PQ_SIZE_256B(pq_size) \
(pq_size ? DIV_ROUND_UP(pq_size, 0x100) - 1 : 0)
#define QM_INVALID_PQ_ID			0xffff
/* feature enable */
#define QM_BYPASS_EN				1
#define QM_BYTE_CRD_EN				1
/* other PQ constants */
#define QM_OTHER_PQS_PER_PF			4
/* WFQ constants */
#define QM_WFQ_UPPER_BOUND			62500000
#define QM_WFQ_VP_PQ_VOQ_SHIFT		0
#define QM_WFQ_VP_PQ_PF_SHIFT		5
#define QM_WFQ_INC_VAL(weight)		((weight) * 0x9000)
#define QM_WFQ_MAX_INC_VAL			43750000
/* RL constants */
#define QM_RL_UPPER_BOUND			62500000
#define QM_RL_PERIOD				5
#define QM_RL_PERIOD_CLK_25M		(25 * QM_RL_PERIOD)
#define QM_RL_INC_VAL(rate) \
OSAL_MAX_T(u32, (((rate ? rate : 1000000) * QM_RL_PERIOD * 1.01) / 8), 1)
#define QM_RL_MAX_INC_VAL			43750000
/* AFullOprtnstcCrdMask constants */
#define QM_OPPOR_LINE_VOQ_DEF		1
#define QM_OPPOR_FW_STOP_DEF		0
#define QM_OPPOR_PQ_EMPTY_DEF		1
#define EAGLE_WORKAROUND_TC			7
/* Command Queue constants */
#define PBF_CMDQ_PURE_LB_LINES			150
#define PBF_CMDQ_EAGLE_WORKAROUND_LINES		8 /* eagle workaround CmdQ */
#define PBF_CMDQ_LINES_RT_OFFSET(voq) \
(PBF_REG_YCMD_QS_NUM_LINES_VOQ0_RT_OFFSET + \
voq * (PBF_REG_YCMD_QS_NUM_LINES_VOQ1_RT_OFFSET \
- PBF_REG_YCMD_QS_NUM_LINES_VOQ0_RT_OFFSET))
#define PBF_BTB_GUARANTEED_RT_OFFSET(voq) \
(PBF_REG_BTB_GUARANTEED_VOQ0_RT_OFFSET + voq * \
(PBF_REG_BTB_GUARANTEED_VOQ1_RT_OFFSET - PBF_REG_BTB_GUARANTEED_VOQ0_RT_OFFSET))
#define QM_VOQ_LINE_CRD(pbf_cmd_lines) \
((((pbf_cmd_lines) - 4) * 2) | QM_LINE_CRD_REG_SIGN_BIT)
/* BTB: blocks constants (block size = 256B) */
#define BTB_JUMBO_PKT_BLOCKS 38	/* 256B blocks in 9700B packet */
#define BTB_HEADROOM_BLOCKS BTB_JUMBO_PKT_BLOCKS	/* headroom per-port */
#define BTB_EAGLE_WORKAROUND_BLOCKS	4	/* eagle workaround blocks */
#define BTB_PURE_LB_FACTOR		10
#define BTB_PURE_LB_RATIO		7 /* factored (hence really 0.7) */
/* QM stop command constants */
#define QM_STOP_PQ_MASK_WIDTH			32
#define QM_STOP_CMD_ADDR				0x2
#define QM_STOP_CMD_STRUCT_SIZE			2
#define QM_STOP_CMD_PAUSE_MASK_OFFSET	0
#define QM_STOP_CMD_PAUSE_MASK_SHIFT	0
#define QM_STOP_CMD_PAUSE_MASK_MASK		-1
#define QM_STOP_CMD_GROUP_ID_OFFSET		1
#define QM_STOP_CMD_GROUP_ID_SHIFT		16
#define QM_STOP_CMD_GROUP_ID_MASK		15
#define QM_STOP_CMD_PQ_TYPE_OFFSET		1
#define QM_STOP_CMD_PQ_TYPE_SHIFT		24
#define QM_STOP_CMD_PQ_TYPE_MASK		1
#define QM_STOP_CMD_MAX_POLL_COUNT		100
#define QM_STOP_CMD_POLL_PERIOD_US		500
/* QM command macros */
#define QM_CMD_STRUCT_SIZE(cmd)	cmd##_STRUCT_SIZE
#define QM_CMD_SET_FIELD(var, cmd, field, value) \
SET_FIELD(var[cmd##_##field##_OFFSET], cmd##_##field, value)
/* QM: VOQ macros */
#define PHYS_VOQ(port, tc, max_phys_tcs_per_port) \
((port) * (max_phys_tcs_per_port) + (tc))
#define LB_VOQ(port)				(MAX_PHYS_VOQS + (port))
#define VOQ(port, tc, max_phys_tcs_per_port) \
((tc) < LB_TC ? PHYS_VOQ(port, tc, max_phys_tcs_per_port) : LB_VOQ(port))
/******************** INTERNAL IMPLEMENTATION *********************/
/* Prepare PF RL enable/disable runtime init values */
static void ecore_enable_pf_rl(struct ecore_hwfn *p_hwfn, bool pf_rl_en)
{
	STORE_RT_REG(p_hwfn, QM_REG_RLPFENABLE_RT_OFFSET, pf_rl_en ? 1 : 0);
	if (pf_rl_en) {
		/* enable RLs for all VOQs */
		STORE_RT_REG(p_hwfn, QM_REG_RLPFVOQENABLE_RT_OFFSET,
			     (1 << MAX_NUM_VOQS) - 1);
		/* write RL period */
		STORE_RT_REG(p_hwfn, QM_REG_RLPFPERIOD_RT_OFFSET,
			     QM_RL_PERIOD_CLK_25M);
		STORE_RT_REG(p_hwfn, QM_REG_RLPFPERIODTIMER_RT_OFFSET,
			     QM_RL_PERIOD_CLK_25M);
		/* set credit threshold for QM bypass flow */
		if (QM_BYPASS_EN)
			STORE_RT_REG(p_hwfn, QM_REG_AFULLQMBYPTHRPFRL_RT_OFFSET,
				     QM_RL_UPPER_BOUND);
	}
}

/* Prepare PF WFQ enable/disable runtime init values */
static void ecore_enable_pf_wfq(struct ecore_hwfn *p_hwfn, bool pf_wfq_en)
{
	STORE_RT_REG(p_hwfn, QM_REG_WFQPFENABLE_RT_OFFSET, pf_wfq_en ? 1 : 0);
	/* set credit threshold for QM bypass flow */
	if (pf_wfq_en && QM_BYPASS_EN)
		STORE_RT_REG(p_hwfn, QM_REG_AFULLQMBYPTHRPFWFQ_RT_OFFSET,
			     QM_WFQ_UPPER_BOUND);
}

/* Prepare VPORT RL enable/disable runtime init values */
static void ecore_enable_vport_rl(struct ecore_hwfn *p_hwfn, bool vport_rl_en)
{
	STORE_RT_REG(p_hwfn, QM_REG_RLGLBLENABLE_RT_OFFSET,
		     vport_rl_en ? 1 : 0);
	if (vport_rl_en) {
		/* write RL period (use timer 0 only) */
		STORE_RT_REG(p_hwfn, QM_REG_RLGLBLPERIOD_0_RT_OFFSET,
			     QM_RL_PERIOD_CLK_25M);
		STORE_RT_REG(p_hwfn, QM_REG_RLGLBLPERIODTIMER_0_RT_OFFSET,
			     QM_RL_PERIOD_CLK_25M);
		/* set credit threshold for QM bypass flow */
		if (QM_BYPASS_EN)
			STORE_RT_REG(p_hwfn,
				     QM_REG_AFULLQMBYPTHRGLBLRL_RT_OFFSET,
				     QM_RL_UPPER_BOUND);
	}
}

/* Prepare VPORT WFQ enable/disable runtime init values */
static void ecore_enable_vport_wfq(struct ecore_hwfn *p_hwfn, bool vport_wfq_en)
{
	STORE_RT_REG(p_hwfn, QM_REG_WFQVPENABLE_RT_OFFSET,
		     vport_wfq_en ? 1 : 0);
	/* set credit threshold for QM bypass flow */
	if (vport_wfq_en && QM_BYPASS_EN)
		STORE_RT_REG(p_hwfn, QM_REG_AFULLQMBYPTHRVPWFQ_RT_OFFSET,
			     QM_WFQ_UPPER_BOUND);
}

/* Prepare runtime init values to allocate PBF command queue lines for
 * the specified VOQ
 */
static void ecore_cmdq_lines_voq_rt_init(struct ecore_hwfn *p_hwfn,
					 u8 voq, u16 cmdq_lines)
{
	u32 qm_line_crd;
	bool is_bb_a0 = ECORE_IS_BB_A0(p_hwfn->p_dev);
	if (is_bb_a0)
		cmdq_lines = OSAL_MIN_T(u32, cmdq_lines, 1022);
	qm_line_crd = QM_VOQ_LINE_CRD(cmdq_lines);
	OVERWRITE_RT_REG(p_hwfn, PBF_CMDQ_LINES_RT_OFFSET(voq),
			 (u32)cmdq_lines);
	STORE_RT_REG(p_hwfn, QM_REG_VOQCRDLINE_RT_OFFSET + voq, qm_line_crd);
	STORE_RT_REG(p_hwfn, QM_REG_VOQINITCRDLINE_RT_OFFSET + voq,
		     qm_line_crd);
}

/* Prepare runtime init values to allocate PBF command queue lines. */
static void ecore_cmdq_lines_rt_init(struct ecore_hwfn *p_hwfn,
				     u8 max_ports_per_engine,
				     u8 max_phys_tcs_per_port,
				     struct init_qm_port_params
				     port_params[MAX_NUM_PORTS])
{
	u8 tc, voq, port_id;
	bool eagle_workaround = ENABLE_EAGLE_ENG1_WORKAROUND(p_hwfn);
	/* clear PBF lines for all VOQs */
	for (voq = 0; voq < MAX_NUM_VOQS; voq++)
		STORE_RT_REG(p_hwfn, PBF_CMDQ_LINES_RT_OFFSET(voq), 0);
	for (port_id = 0; port_id < max_ports_per_engine; port_id++) {
		if (port_params[port_id].active) {
			u16 phys_lines, phys_lines_per_tc;
			phys_lines =
			    port_params[port_id].num_pbf_cmd_lines -
			    PBF_CMDQ_PURE_LB_LINES;
			if (eagle_workaround)
				phys_lines -= PBF_CMDQ_EAGLE_WORKAROUND_LINES;
			/* find #lines per active physical TC */
			phys_lines_per_tc =
			    phys_lines /
			    port_params[port_id].num_active_phys_tcs;
			/* init registers per active TC */
			for (tc = 0;
			     tc < port_params[port_id].num_active_phys_tcs;
			     tc++) {
				voq =
				    PHYS_VOQ(port_id, tc,
					     max_phys_tcs_per_port);
				ecore_cmdq_lines_voq_rt_init(p_hwfn, voq,
							     phys_lines_per_tc);
			}
			/* init registers for pure LB TC */
			ecore_cmdq_lines_voq_rt_init(p_hwfn, LB_VOQ(port_id),
						     PBF_CMDQ_PURE_LB_LINES);
			/* init registers for eagle workaround */
			if (eagle_workaround) {
				voq =
				    PHYS_VOQ(port_id, EAGLE_WORKAROUND_TC,
					     max_phys_tcs_per_port);
				ecore_cmdq_lines_voq_rt_init(p_hwfn, voq,
					     PBF_CMDQ_EAGLE_WORKAROUND_LINES);
			}
		}
	}
}

/*
 * Prepare runtime init values to allocate guaranteed BTB blocks for the
 * specified port. The guaranteed BTB space is divided between the TCs as
 * follows (shared space Is currently not used):
 * 1. Parameters:
 *     B BTB blocks for this port
 *     C Number of physical TCs for this port
 * 2. Calculation:
 *     a. 38 blocks (9700B jumbo frame) are allocated for global per port
 *        headroom
 *     b. B = B 38 (remainder after global headroom allocation)
 *     c. MAX(38,B/(C+0.7)) blocks are allocated for the pure LB VOQ.
 *     d. B = B MAX(38, B/(C+0.7)) (remainder after pure LB allocation).
 *     e. B/C blocks are allocated for each physical TC.
 * Assumptions:
 * - MTU is up to 9700 bytes (38 blocks)
 * - All TCs are considered symmetrical (same rate and packet size)
 * - No optimization for lossy TC (all are considered lossless). Shared space is
 *   not enabled and allocated for each TC.
 */
static void ecore_btb_blocks_rt_init(struct ecore_hwfn *p_hwfn,
				     u8 max_ports_per_engine,
				     u8 max_phys_tcs_per_port,
				     struct init_qm_port_params
				     port_params[MAX_NUM_PORTS])
{
	u8 tc, voq, port_id;
	u32 usable_blocks, pure_lb_blocks, phys_blocks;
	bool eagle_workaround = ENABLE_EAGLE_ENG1_WORKAROUND(p_hwfn);
	for (port_id = 0; port_id < max_ports_per_engine; port_id++) {
		if (port_params[port_id].active) {
			/* subtract headroom blocks */
			usable_blocks =
			    port_params[port_id].num_btb_blocks -
			    BTB_HEADROOM_BLOCKS;
			if (eagle_workaround)
				usable_blocks -= BTB_EAGLE_WORKAROUND_BLOCKS;
			pure_lb_blocks =
			    (usable_blocks * BTB_PURE_LB_FACTOR) /
			    (port_params[port_id].num_active_phys_tcs *
			     BTB_PURE_LB_FACTOR + BTB_PURE_LB_RATIO);
			pure_lb_blocks =
			    OSAL_MAX_T(u32, BTB_JUMBO_PKT_BLOCKS,
				       pure_lb_blocks / BTB_PURE_LB_FACTOR);
			phys_blocks =
			    (usable_blocks -
			     pure_lb_blocks) /
			    port_params[port_id].num_active_phys_tcs;
			/* init physical TCs */
			for (tc = 0;
			     tc < port_params[port_id].num_active_phys_tcs;
			     tc++) {
				voq =
				    PHYS_VOQ(port_id, tc,
					     max_phys_tcs_per_port);
				STORE_RT_REG(p_hwfn,
					     PBF_BTB_GUARANTEED_RT_OFFSET(voq),
					     phys_blocks);
			}
			/* init pure LB TC */
			STORE_RT_REG(p_hwfn,
				     PBF_BTB_GUARANTEED_RT_OFFSET(LB_VOQ
								  (port_id)),
				     pure_lb_blocks);
			/* init eagle workaround */
			if (eagle_workaround) {
				voq =
				    PHYS_VOQ(port_id, EAGLE_WORKAROUND_TC,
					     max_phys_tcs_per_port);
				STORE_RT_REG(p_hwfn,
					     PBF_BTB_GUARANTEED_RT_OFFSET(voq),
					     BTB_EAGLE_WORKAROUND_BLOCKS);
			}
		}
	}
}

/* Prepare Tx PQ mapping runtime init values for the specified PF */
static void ecore_tx_pq_map_rt_init(struct ecore_hwfn *p_hwfn,
				    struct ecore_ptt *p_ptt,
				    u8 port_id,
				    u8 pf_id,
				    u8 max_phys_tcs_per_port,
				    bool is_first_pf,
				    u32 num_pf_cids,
				    u32 num_vf_cids,
				    u16 start_pq,
				    u16 num_pf_pqs,
				    u16 num_vf_pqs,
				    u8 start_vport,
				    u32 base_mem_addr_4kb,
				    struct init_qm_pq_params *pq_params,
				    struct init_qm_vport_params *vport_params)
{
	u16 i, pq_id, pq_group;
	u16 num_pqs = num_pf_pqs + num_vf_pqs;
	u16 first_pq_group = start_pq / QM_PF_QUEUE_GROUP_SIZE;
	u16 last_pq_group = (start_pq + num_pqs - 1) / QM_PF_QUEUE_GROUP_SIZE;
	bool is_bb_a0 = ECORE_IS_BB_A0(p_hwfn->p_dev);
	/* a bit per Tx PQ indicating if the PQ is associated with a VF */
	u32 tx_pq_vf_mask[MAX_QM_TX_QUEUES / QM_PF_QUEUE_GROUP_SIZE] = { 0 };
	u32 tx_pq_vf_mask_width = is_bb_a0 ? 32 : QM_PF_QUEUE_GROUP_SIZE;
	u32 num_tx_pq_vf_masks = MAX_QM_TX_QUEUES / tx_pq_vf_mask_width;
	u32 pq_mem_4kb = QM_PQ_MEM_4KB(num_pf_cids);
	u32 vport_pq_mem_4kb = QM_PQ_MEM_4KB(num_vf_cids);
	u32 mem_addr_4kb = base_mem_addr_4kb;
	/* set mapping from PQ group to PF */
	for (pq_group = first_pq_group; pq_group <= last_pq_group; pq_group++)
		STORE_RT_REG(p_hwfn, QM_REG_PQTX2PF_0_RT_OFFSET + pq_group,
			     (u32)(pf_id));
	/* set PQ sizes */
	STORE_RT_REG(p_hwfn, QM_REG_MAXPQSIZE_0_RT_OFFSET,
		     QM_PQ_SIZE_256B(num_pf_cids));
	STORE_RT_REG(p_hwfn, QM_REG_MAXPQSIZE_1_RT_OFFSET,
		     QM_PQ_SIZE_256B(num_vf_cids));
	/* go over all Tx PQs */
	for (i = 0, pq_id = start_pq; i < num_pqs; i++, pq_id++) {
		struct qm_rf_pq_map tx_pq_map;
		u8 voq =
		    VOQ(port_id, pq_params[i].tc_id, max_phys_tcs_per_port);
		bool is_vf_pq = (i >= num_pf_pqs);
		/* update first Tx PQ of VPORT/TC */
		u8 vport_id_in_pf = pq_params[i].vport_id - start_vport;
		u16 first_tx_pq_id =
		    vport_params[vport_id_in_pf].first_tx_pq_id[pq_params[i].
								tc_id];
		if (first_tx_pq_id == QM_INVALID_PQ_ID) {
			/* create new VP PQ */
			vport_params[vport_id_in_pf].
			    first_tx_pq_id[pq_params[i].tc_id] = pq_id;
			first_tx_pq_id = pq_id;
			/* map VP PQ to VOQ and PF */
			STORE_RT_REG(p_hwfn,
				     QM_REG_WFQVPMAP_RT_OFFSET + first_tx_pq_id,
				     (voq << QM_WFQ_VP_PQ_VOQ_SHIFT) | (pf_id <<
							QM_WFQ_VP_PQ_PF_SHIFT));
		}
		/* fill PQ map entry */
		OSAL_MEMSET(&tx_pq_map, 0, sizeof(tx_pq_map));
		SET_FIELD(tx_pq_map.reg, QM_RF_PQ_MAP_PQ_VALID, 1);
		SET_FIELD(tx_pq_map.reg, QM_RF_PQ_MAP_RL_VALID,
			  is_vf_pq ? 1 : 0);
		SET_FIELD(tx_pq_map.reg, QM_RF_PQ_MAP_VP_PQ_ID, first_tx_pq_id);
		SET_FIELD(tx_pq_map.reg, QM_RF_PQ_MAP_RL_ID,
			  is_vf_pq ? pq_params[i].vport_id : 0);
		SET_FIELD(tx_pq_map.reg, QM_RF_PQ_MAP_VOQ, voq);
		SET_FIELD(tx_pq_map.reg, QM_RF_PQ_MAP_WRR_WEIGHT_GROUP,
			  pq_params[i].wrr_group);
		/* write PQ map entry to CAM */
		STORE_RT_REG(p_hwfn, QM_REG_TXPQMAP_RT_OFFSET + pq_id,
			     *((u32 *)&tx_pq_map));
		/* set base address */
		STORE_RT_REG(p_hwfn, QM_REG_BASEADDRTXPQ_RT_OFFSET + pq_id,
			     mem_addr_4kb);
		/* check if VF PQ */
		if (is_vf_pq) {
			tx_pq_vf_mask[pq_id / tx_pq_vf_mask_width] |=
			    (1 << (pq_id % tx_pq_vf_mask_width));
			mem_addr_4kb += vport_pq_mem_4kb;
		} else {
			mem_addr_4kb += pq_mem_4kb;
		}
	}
	/* store Tx PQ VF mask to size select register */
	for (i = 0; i < num_tx_pq_vf_masks; i++) {
		if (tx_pq_vf_mask[i]) {
			if (is_bb_a0) {
				u32 curr_mask =
				    is_first_pf ? 0 : ecore_rd(p_hwfn, p_ptt,
						       QM_REG_MAXPQSIZETXSEL_0
							       + i * 4);
				STORE_RT_REG(p_hwfn,
					     QM_REG_MAXPQSIZETXSEL_0_RT_OFFSET +
					     i, curr_mask | tx_pq_vf_mask[i]);
			} else
				STORE_RT_REG(p_hwfn,
					     QM_REG_MAXPQSIZETXSEL_0_RT_OFFSET +
					     i, tx_pq_vf_mask[i]);
		}
	}
}

/* Prepare Other PQ mapping runtime init values for the specified PF */
static void ecore_other_pq_map_rt_init(struct ecore_hwfn *p_hwfn,
				       u8 port_id,
				       u8 pf_id,
				       u32 num_pf_cids,
				       u32 num_tids, u32 base_mem_addr_4kb)
{
	u16 i, pq_id;
	u16 pq_group = pf_id;
	u32 pq_size = num_pf_cids + num_tids;
	u32 pq_mem_4kb = QM_PQ_MEM_4KB(pq_size);
	u32 mem_addr_4kb = base_mem_addr_4kb;
	/* map PQ group to PF */
	STORE_RT_REG(p_hwfn, QM_REG_PQOTHER2PF_0_RT_OFFSET + pq_group,
		     (u32)(pf_id));
	/* set PQ sizes */
	STORE_RT_REG(p_hwfn, QM_REG_MAXPQSIZE_2_RT_OFFSET,
		     QM_PQ_SIZE_256B(pq_size));
	/* set base address */
	for (i = 0, pq_id = pf_id * QM_PF_QUEUE_GROUP_SIZE;
	     i < QM_OTHER_PQS_PER_PF; i++, pq_id++) {
		STORE_RT_REG(p_hwfn, QM_REG_BASEADDROTHERPQ_RT_OFFSET + pq_id,
			     mem_addr_4kb);
		mem_addr_4kb += pq_mem_4kb;
	}
}

static int ecore_pf_wfq_rt_init(struct ecore_hwfn *p_hwfn,
				u8 port_id,
				u8 pf_id,
				u16 pf_wfq,
				u8 max_phys_tcs_per_port,
				u16 num_tx_pqs,
				struct init_qm_pq_params *pq_params)
{
	u16 i;
	u32 inc_val;
	u32 crd_reg_offset =
	    (pf_id <
	     MAX_NUM_PFS_BB ? QM_REG_WFQPFCRD_RT_OFFSET :
	     QM_REG_WFQPFCRD_MSB_RT_OFFSET) + (pf_id % MAX_NUM_PFS_BB);
	inc_val = QM_WFQ_INC_VAL(pf_wfq);
	if (inc_val == 0 || inc_val > QM_WFQ_MAX_INC_VAL) {
		DP_NOTICE(p_hwfn, true, "Invalid PF WFQ weight configuration");
		return -1;
	}
	for (i = 0; i < num_tx_pqs; i++) {
		u8 voq =
		    VOQ(port_id, pq_params[i].tc_id, max_phys_tcs_per_port);
		OVERWRITE_RT_REG(p_hwfn, crd_reg_offset + voq * MAX_NUM_PFS_BB,
				 QM_WFQ_CRD_REG_SIGN_BIT);
	}
	STORE_RT_REG(p_hwfn, QM_REG_WFQPFUPPERBOUND_RT_OFFSET + pf_id,
		     QM_WFQ_UPPER_BOUND | QM_WFQ_CRD_REG_SIGN_BIT);
	STORE_RT_REG(p_hwfn, QM_REG_WFQPFWEIGHT_RT_OFFSET + pf_id, inc_val);
	return 0;
}

/* Prepare PF RL runtime init values for the specified PF. Return -1 on err */
static int ecore_pf_rl_rt_init(struct ecore_hwfn *p_hwfn, u8 pf_id, u32 pf_rl)
{
	u32 inc_val = QM_RL_INC_VAL(pf_rl);
	if (inc_val > QM_RL_MAX_INC_VAL) {
		DP_NOTICE(p_hwfn, true, "Invalid PF rate limit configuration");
		return -1;
	}
	STORE_RT_REG(p_hwfn, QM_REG_RLPFCRD_RT_OFFSET + pf_id,
		     QM_RL_CRD_REG_SIGN_BIT);
	STORE_RT_REG(p_hwfn, QM_REG_RLPFUPPERBOUND_RT_OFFSET + pf_id,
		     QM_RL_UPPER_BOUND | QM_RL_CRD_REG_SIGN_BIT);
	STORE_RT_REG(p_hwfn, QM_REG_RLPFINCVAL_RT_OFFSET + pf_id, inc_val);
	return 0;
}

static int ecore_vp_wfq_rt_init(struct ecore_hwfn *p_hwfn,
				u8 num_vports,
				struct init_qm_vport_params *vport_params)
{
	u8 tc, i;
	u32 inc_val;
	/* go over all PF VPORTs */
	for (i = 0; i < num_vports; i++) {
		if (vport_params[i].vport_wfq) {
			inc_val = QM_WFQ_INC_VAL(vport_params[i].vport_wfq);
			if (inc_val > QM_WFQ_MAX_INC_VAL) {
				DP_NOTICE(p_hwfn, true,
					  "Invalid VPORT WFQ weight config");
				return -1;
			}
			for (tc = 0; tc < NUM_OF_TCS; tc++) {
				u16 vport_pq_id =
				    vport_params[i].first_tx_pq_id[tc];
				if (vport_pq_id != QM_INVALID_PQ_ID) {
					STORE_RT_REG(p_hwfn,
						     QM_REG_WFQVPCRD_RT_OFFSET +
						     vport_pq_id,
						     QM_WFQ_CRD_REG_SIGN_BIT);
					STORE_RT_REG(p_hwfn,
						QM_REG_WFQVPWEIGHT_RT_OFFSET
						     + vport_pq_id, inc_val);
				}
			}
		}
	}
	return 0;
}

/* Prepare VPORT RL runtime init values for specified VPORT. Ret -1 on error. */
static int ecore_vport_rl_rt_init(struct ecore_hwfn *p_hwfn,
				  u8 start_vport,
				  u8 num_vports,
				  struct init_qm_vport_params *vport_params)
{
	u8 i, vport_id;
	/* go over all PF VPORTs */
	for (i = 0, vport_id = start_vport; i < num_vports; i++, vport_id++) {
		u32 inc_val = QM_RL_INC_VAL(vport_params[i].vport_rl);
		if (inc_val > QM_RL_MAX_INC_VAL) {
			DP_NOTICE(p_hwfn, true,
				  "Invalid VPORT rate-limit configuration");
			return -1;
		}
		STORE_RT_REG(p_hwfn, QM_REG_RLGLBLCRD_RT_OFFSET + vport_id,
			     QM_RL_CRD_REG_SIGN_BIT);
		STORE_RT_REG(p_hwfn,
			     QM_REG_RLGLBLUPPERBOUND_RT_OFFSET + vport_id,
			     QM_RL_UPPER_BOUND | QM_RL_CRD_REG_SIGN_BIT);
		STORE_RT_REG(p_hwfn, QM_REG_RLGLBLINCVAL_RT_OFFSET + vport_id,
			     inc_val);
	}
	return 0;
}

static bool ecore_poll_on_qm_cmd_ready(struct ecore_hwfn *p_hwfn,
				       struct ecore_ptt *p_ptt)
{
	u32 reg_val, i;
	for (i = 0, reg_val = 0; i < QM_STOP_CMD_MAX_POLL_COUNT && reg_val == 0;
	     i++) {
		OSAL_UDELAY(QM_STOP_CMD_POLL_PERIOD_US);
		reg_val = ecore_rd(p_hwfn, p_ptt, QM_REG_SDMCMDREADY);
	}
	/* check if timeout while waiting for SDM command ready */
	if (i == QM_STOP_CMD_MAX_POLL_COUNT) {
		DP_VERBOSE(p_hwfn, ECORE_MSG_HW,
			   "Timeout waiting for QM SDM cmd ready signal\n");
		return false;
	}
	return true;
}

static bool ecore_send_qm_cmd(struct ecore_hwfn *p_hwfn,
			      struct ecore_ptt *p_ptt,
			      u32 cmd_addr, u32 cmd_data_lsb, u32 cmd_data_msb)
{
	if (!ecore_poll_on_qm_cmd_ready(p_hwfn, p_ptt))
		return false;
	ecore_wr(p_hwfn, p_ptt, QM_REG_SDMCMDADDR, cmd_addr);
	ecore_wr(p_hwfn, p_ptt, QM_REG_SDMCMDDATALSB, cmd_data_lsb);
	ecore_wr(p_hwfn, p_ptt, QM_REG_SDMCMDDATAMSB, cmd_data_msb);
	ecore_wr(p_hwfn, p_ptt, QM_REG_SDMCMDGO, 1);
	ecore_wr(p_hwfn, p_ptt, QM_REG_SDMCMDGO, 0);
	return ecore_poll_on_qm_cmd_ready(p_hwfn, p_ptt);
}

/******************** INTERFACE IMPLEMENTATION *********************/
u32 ecore_qm_pf_mem_size(u8 pf_id,
			 u32 num_pf_cids,
			 u32 num_vf_cids,
			 u32 num_tids, u16 num_pf_pqs, u16 num_vf_pqs)
{
	return QM_PQ_MEM_4KB(num_pf_cids) * num_pf_pqs +
	    QM_PQ_MEM_4KB(num_vf_cids) * num_vf_pqs +
	    QM_PQ_MEM_4KB(num_pf_cids + num_tids) * QM_OTHER_PQS_PER_PF;
}

int ecore_qm_common_rt_init(struct ecore_hwfn *p_hwfn,
			    u8 max_ports_per_engine,
			    u8 max_phys_tcs_per_port,
			    bool pf_rl_en,
			    bool pf_wfq_en,
			    bool vport_rl_en,
			    bool vport_wfq_en,
			    struct init_qm_port_params
			    port_params[MAX_NUM_PORTS])
{
	u8 port_id;
	/* init AFullOprtnstcCrdMask */
	u32 mask =
	    (QM_OPPOR_LINE_VOQ_DEF << QM_RF_OPPORTUNISTIC_MASK_LINEVOQ_SHIFT) |
	    (QM_BYTE_CRD_EN << QM_RF_OPPORTUNISTIC_MASK_BYTEVOQ_SHIFT) |
	    (pf_wfq_en << QM_RF_OPPORTUNISTIC_MASK_PFWFQ_SHIFT) |
	    (vport_wfq_en << QM_RF_OPPORTUNISTIC_MASK_VPWFQ_SHIFT) |
	    (pf_rl_en << QM_RF_OPPORTUNISTIC_MASK_PFRL_SHIFT) |
	    (vport_rl_en << QM_RF_OPPORTUNISTIC_MASK_VPQCNRL_SHIFT) |
	    (QM_OPPOR_FW_STOP_DEF << QM_RF_OPPORTUNISTIC_MASK_FWPAUSE_SHIFT) |
	    (QM_OPPOR_PQ_EMPTY_DEF <<
	     QM_RF_OPPORTUNISTIC_MASK_QUEUEEMPTY_SHIFT);
	STORE_RT_REG(p_hwfn, QM_REG_AFULLOPRTNSTCCRDMASK_RT_OFFSET, mask);
	/* check eagle workaround */
	for (port_id = 0; port_id < max_ports_per_engine; port_id++) {
		if (port_params[port_id].active &&
		    port_params[port_id].num_active_phys_tcs >
		    EAGLE_WORKAROUND_TC &&
		    ENABLE_EAGLE_ENG1_WORKAROUND(p_hwfn)) {
			DP_NOTICE(p_hwfn, true,
				  "Can't config 8 TCs with Eagle"
				  " eng1 workaround");
			return -1;
		}
	}
	/* enable/disable PF RL */
	ecore_enable_pf_rl(p_hwfn, pf_rl_en);
	/* enable/disable PF WFQ */
	ecore_enable_pf_wfq(p_hwfn, pf_wfq_en);
	/* enable/disable VPORT RL */
	ecore_enable_vport_rl(p_hwfn, vport_rl_en);
	/* enable/disable VPORT WFQ */
	ecore_enable_vport_wfq(p_hwfn, vport_wfq_en);
	/* init PBF CMDQ line credit */
	ecore_cmdq_lines_rt_init(p_hwfn, max_ports_per_engine,
				 max_phys_tcs_per_port, port_params);
	/* init BTB blocks in PBF */
	ecore_btb_blocks_rt_init(p_hwfn, max_ports_per_engine,
				 max_phys_tcs_per_port, port_params);
	return 0;
}

int ecore_qm_pf_rt_init(struct ecore_hwfn *p_hwfn,
			struct ecore_ptt *p_ptt,
			u8 port_id,
			u8 pf_id,
			u8 max_phys_tcs_per_port,
			bool is_first_pf,
			u32 num_pf_cids,
			u32 num_vf_cids,
			u32 num_tids,
			u16 start_pq,
			u16 num_pf_pqs,
			u16 num_vf_pqs,
			u8 start_vport,
			u8 num_vports,
			u16 pf_wfq,
			u32 pf_rl,
			struct init_qm_pq_params *pq_params,
			struct init_qm_vport_params *vport_params)
{
	u8 tc, i;
	u32 other_mem_size_4kb =
	    QM_PQ_MEM_4KB(num_pf_cids + num_tids) * QM_OTHER_PQS_PER_PF;
	/* clear first Tx PQ ID array for each VPORT */
	for (i = 0; i < num_vports; i++)
		for (tc = 0; tc < NUM_OF_TCS; tc++)
			vport_params[i].first_tx_pq_id[tc] = QM_INVALID_PQ_ID;
	/* map Other PQs (if any) */
#if QM_OTHER_PQS_PER_PF > 0
	ecore_other_pq_map_rt_init(p_hwfn, port_id, pf_id, num_pf_cids,
				   num_tids, 0);
#endif
	/* map Tx PQs */
	ecore_tx_pq_map_rt_init(p_hwfn, p_ptt, port_id, pf_id,
				max_phys_tcs_per_port, is_first_pf, num_pf_cids,
				num_vf_cids, start_pq, num_pf_pqs, num_vf_pqs,
				start_vport, other_mem_size_4kb, pq_params,
				vport_params);
	/* init PF WFQ */
	if (pf_wfq)
		if (ecore_pf_wfq_rt_init
		    (p_hwfn, port_id, pf_id, pf_wfq, max_phys_tcs_per_port,
		     num_pf_pqs + num_vf_pqs, pq_params) != 0)
			return -1;
	/* init PF RL */
	if (ecore_pf_rl_rt_init(p_hwfn, pf_id, pf_rl) != 0)
		return -1;
	/* set VPORT WFQ */
	if (ecore_vp_wfq_rt_init(p_hwfn, num_vports, vport_params) != 0)
		return -1;
	/* set VPORT RL */
	if (ecore_vport_rl_rt_init
	    (p_hwfn, start_vport, num_vports, vport_params) != 0)
		return -1;
	return 0;
}

int ecore_init_pf_wfq(struct ecore_hwfn *p_hwfn,
		      struct ecore_ptt *p_ptt, u8 pf_id, u16 pf_wfq)
{
	u32 inc_val = QM_WFQ_INC_VAL(pf_wfq);
	if (inc_val == 0 || inc_val > QM_WFQ_MAX_INC_VAL) {
		DP_NOTICE(p_hwfn, true, "Invalid PF WFQ weight configuration");
		return -1;
	}
	ecore_wr(p_hwfn, p_ptt, QM_REG_WFQPFWEIGHT + pf_id * 4, inc_val);
	return 0;
}

int ecore_init_pf_rl(struct ecore_hwfn *p_hwfn,
		     struct ecore_ptt *p_ptt, u8 pf_id, u32 pf_rl)
{
	u32 inc_val = QM_RL_INC_VAL(pf_rl);
	if (inc_val > QM_RL_MAX_INC_VAL) {
		DP_NOTICE(p_hwfn, true, "Invalid PF rate limit configuration");
		return -1;
	}
	ecore_wr(p_hwfn, p_ptt, QM_REG_RLPFCRD + pf_id * 4,
		 QM_RL_CRD_REG_SIGN_BIT);
	ecore_wr(p_hwfn, p_ptt, QM_REG_RLPFINCVAL + pf_id * 4, inc_val);
	return 0;
}

int ecore_init_vport_wfq(struct ecore_hwfn *p_hwfn,
			 struct ecore_ptt *p_ptt,
			 u16 first_tx_pq_id[NUM_OF_TCS], u16 vport_wfq)
{
	u8 tc;
	u32 inc_val = QM_WFQ_INC_VAL(vport_wfq);
	if (inc_val == 0 || inc_val > QM_WFQ_MAX_INC_VAL) {
		DP_NOTICE(p_hwfn, true,
			  "Invalid VPORT WFQ weight configuration");
		return -1;
	}
	for (tc = 0; tc < NUM_OF_TCS; tc++) {
		u16 vport_pq_id = first_tx_pq_id[tc];
		if (vport_pq_id != QM_INVALID_PQ_ID) {
			ecore_wr(p_hwfn, p_ptt,
				 QM_REG_WFQVPWEIGHT + vport_pq_id * 4, inc_val);
		}
	}
	return 0;
}

int ecore_init_vport_rl(struct ecore_hwfn *p_hwfn,
			struct ecore_ptt *p_ptt, u8 vport_id, u32 vport_rl)
{
	u32 inc_val = QM_RL_INC_VAL(vport_rl);
	if (inc_val > QM_RL_MAX_INC_VAL) {
		DP_NOTICE(p_hwfn, true,
			  "Invalid VPORT rate-limit configuration");
		return -1;
	}
	ecore_wr(p_hwfn, p_ptt, QM_REG_RLGLBLCRD + vport_id * 4,
		 QM_RL_CRD_REG_SIGN_BIT);
	ecore_wr(p_hwfn, p_ptt, QM_REG_RLGLBLINCVAL + vport_id * 4, inc_val);
	return 0;
}

bool ecore_send_qm_stop_cmd(struct ecore_hwfn *p_hwfn,
			    struct ecore_ptt *p_ptt,
			    bool is_release_cmd,
			    bool is_tx_pq, u16 start_pq, u16 num_pqs)
{
	u32 cmd_arr[QM_CMD_STRUCT_SIZE(QM_STOP_CMD)] = { 0 };
	u32 pq_mask = 0, last_pq = start_pq + num_pqs - 1, pq_id;
	/* set command's PQ type */
	QM_CMD_SET_FIELD(cmd_arr, QM_STOP_CMD, PQ_TYPE, is_tx_pq ? 0 : 1);
	/* go over requested PQs */
	for (pq_id = start_pq; pq_id <= last_pq; pq_id++) {
		/* set PQ bit in mask (stop command only) */
		if (!is_release_cmd)
			pq_mask |= (1 << (pq_id % QM_STOP_PQ_MASK_WIDTH));
		/* if last PQ or end of PQ mask, write command */
		if ((pq_id == last_pq) ||
		    (pq_id % QM_STOP_PQ_MASK_WIDTH ==
		    (QM_STOP_PQ_MASK_WIDTH - 1))) {
			QM_CMD_SET_FIELD(cmd_arr, QM_STOP_CMD, PAUSE_MASK,
					 pq_mask);
			QM_CMD_SET_FIELD(cmd_arr, QM_STOP_CMD, GROUP_ID,
					 pq_id / QM_STOP_PQ_MASK_WIDTH);
			if (!ecore_send_qm_cmd
			    (p_hwfn, p_ptt, QM_STOP_CMD_ADDR, cmd_arr[0],
			     cmd_arr[1]))
				return false;
			pq_mask = 0;
		}
	}
	return true;
}

/* NIG: ETS configuration constants */
#define NIG_TX_ETS_CLIENT_OFFSET	4
#define NIG_LB_ETS_CLIENT_OFFSET	1
#define NIG_ETS_MIN_WFQ_BYTES		1600
/* NIG: ETS constants */
#define NIG_ETS_UP_BOUND(weight, mtu) \
(2 * ((weight) > (mtu) ? (weight) : (mtu)))
/* NIG: RL constants */
#define NIG_RL_BASE_TYPE			1	/* byte base type */
#define NIG_RL_PERIOD				1	/* in us */
#define NIG_RL_PERIOD_CLK_25M		(25 * NIG_RL_PERIOD)
#define NIG_RL_INC_VAL(rate)		(((rate) * NIG_RL_PERIOD) / 8)
#define NIG_RL_MAX_VAL(inc_val, mtu) \
(2 * ((inc_val) > (mtu) ? (inc_val) : (mtu)))
/* NIG: packet prioritry configuration constants */
#define NIG_PRIORITY_MAP_TC_BITS 4
void ecore_init_nig_ets(struct ecore_hwfn *p_hwfn,
			struct ecore_ptt *p_ptt,
			struct init_ets_req *req, bool is_lb)
{
	u8 tc, sp_tc_map = 0, wfq_tc_map = 0;
	u8 num_tc = is_lb ? NUM_OF_TCS : NUM_OF_PHYS_TCS;
	u8 tc_client_offset =
	    is_lb ? NIG_LB_ETS_CLIENT_OFFSET : NIG_TX_ETS_CLIENT_OFFSET;
	u32 min_weight = 0xffffffff;
	u32 tc_weight_base_addr =
	    is_lb ? NIG_REG_LB_ARB_CREDIT_WEIGHT_0 :
	    NIG_REG_TX_ARB_CREDIT_WEIGHT_0;
	u32 tc_weight_addr_diff =
	    is_lb ? NIG_REG_LB_ARB_CREDIT_WEIGHT_1 -
	    NIG_REG_LB_ARB_CREDIT_WEIGHT_0 : NIG_REG_TX_ARB_CREDIT_WEIGHT_1 -
	    NIG_REG_TX_ARB_CREDIT_WEIGHT_0;
	u32 tc_bound_base_addr =
	    is_lb ? NIG_REG_LB_ARB_CREDIT_UPPER_BOUND_0 :
	    NIG_REG_TX_ARB_CREDIT_UPPER_BOUND_0;
	u32 tc_bound_addr_diff =
	    is_lb ? NIG_REG_LB_ARB_CREDIT_UPPER_BOUND_1 -
	    NIG_REG_LB_ARB_CREDIT_UPPER_BOUND_0 :
	    NIG_REG_TX_ARB_CREDIT_UPPER_BOUND_1 -
	    NIG_REG_TX_ARB_CREDIT_UPPER_BOUND_0;
	for (tc = 0; tc < num_tc; tc++) {
		struct init_ets_tc_req *tc_req = &req->tc_req[tc];
		/* update SP map */
		if (tc_req->use_sp)
			sp_tc_map |= (1 << tc);
		if (tc_req->use_wfq) {
			/* update WFQ map */
			wfq_tc_map |= (1 << tc);
			/* find minimal weight */
			if (tc_req->weight < min_weight)
				min_weight = tc_req->weight;
		}
	}
	/* write SP map */
	ecore_wr(p_hwfn, p_ptt,
		 is_lb ? NIG_REG_LB_ARB_CLIENT_IS_STRICT :
		 NIG_REG_TX_ARB_CLIENT_IS_STRICT,
		 (sp_tc_map << tc_client_offset));
	/* write WFQ map */
	ecore_wr(p_hwfn, p_ptt,
		 is_lb ? NIG_REG_LB_ARB_CLIENT_IS_SUBJECT2WFQ :
		 NIG_REG_TX_ARB_CLIENT_IS_SUBJECT2WFQ,
		 (wfq_tc_map << tc_client_offset));
	/* write WFQ weights */
	for (tc = 0; tc < num_tc; tc++, tc_client_offset++) {
		struct init_ets_tc_req *tc_req = &req->tc_req[tc];
		if (tc_req->use_wfq) {
			/* translate weight to bytes */
			u32 byte_weight =
			    (NIG_ETS_MIN_WFQ_BYTES * tc_req->weight) /
			    min_weight;
			/* write WFQ weight */
			ecore_wr(p_hwfn, p_ptt,
				 tc_weight_base_addr +
				 tc_weight_addr_diff * tc_client_offset,
				 byte_weight);
			/* write WFQ upper bound */
			ecore_wr(p_hwfn, p_ptt,
				 tc_bound_base_addr +
				 tc_bound_addr_diff * tc_client_offset,
				 NIG_ETS_UP_BOUND(byte_weight, req->mtu));
		}
	}
}

void ecore_init_nig_lb_rl(struct ecore_hwfn *p_hwfn,
			  struct ecore_ptt *p_ptt,
			  struct init_nig_lb_rl_req *req)
{
	u8 tc;
	u32 ctrl, inc_val, reg_offset;
	/* disable global MAC+LB RL */
	ctrl =
	    NIG_RL_BASE_TYPE <<
	    NIG_REG_TX_LB_GLBRATELIMIT_CTRL_TX_LB_GLBRATELIMIT_BASE_TYPE_SHIFT;
	ecore_wr(p_hwfn, p_ptt, NIG_REG_TX_LB_GLBRATELIMIT_CTRL, ctrl);
	/* configure and enable global MAC+LB RL */
	if (req->lb_mac_rate) {
		/* configure  */
		ecore_wr(p_hwfn, p_ptt, NIG_REG_TX_LB_GLBRATELIMIT_INC_PERIOD,
			 NIG_RL_PERIOD_CLK_25M);
		inc_val = NIG_RL_INC_VAL(req->lb_mac_rate);
		ecore_wr(p_hwfn, p_ptt, NIG_REG_TX_LB_GLBRATELIMIT_INC_VALUE,
			 inc_val);
		ecore_wr(p_hwfn, p_ptt, NIG_REG_TX_LB_GLBRATELIMIT_MAX_VALUE,
			 NIG_RL_MAX_VAL(inc_val, req->mtu));
		/* enable */
		ctrl |=
		    1 <<
		    NIG_REG_TX_LB_GLBRATELIMIT_CTRL_TX_LB_GLBRATELIMIT_EN_SHIFT;
		ecore_wr(p_hwfn, p_ptt, NIG_REG_TX_LB_GLBRATELIMIT_CTRL, ctrl);
	}
	/* disable global LB-only RL */
	ctrl =
	    NIG_RL_BASE_TYPE <<
	    NIG_REG_LB_BRBRATELIMIT_CTRL_LB_BRBRATELIMIT_BASE_TYPE_SHIFT;
	ecore_wr(p_hwfn, p_ptt, NIG_REG_LB_BRBRATELIMIT_CTRL, ctrl);
	/* configure and enable global LB-only RL */
	if (req->lb_rate) {
		/* configure  */
		ecore_wr(p_hwfn, p_ptt, NIG_REG_LB_BRBRATELIMIT_INC_PERIOD,
			 NIG_RL_PERIOD_CLK_25M);
		inc_val = NIG_RL_INC_VAL(req->lb_rate);
		ecore_wr(p_hwfn, p_ptt, NIG_REG_LB_BRBRATELIMIT_INC_VALUE,
			 inc_val);
		ecore_wr(p_hwfn, p_ptt, NIG_REG_LB_BRBRATELIMIT_MAX_VALUE,
			 NIG_RL_MAX_VAL(inc_val, req->mtu));
		/* enable */
		ctrl |=
		    1 << NIG_REG_LB_BRBRATELIMIT_CTRL_LB_BRBRATELIMIT_EN_SHIFT;
		ecore_wr(p_hwfn, p_ptt, NIG_REG_LB_BRBRATELIMIT_CTRL, ctrl);
	}
	/* per-TC RLs */
	for (tc = 0, reg_offset = 0; tc < NUM_OF_PHYS_TCS;
	     tc++, reg_offset += 4) {
		/* disable TC RL */
		ctrl =
		    NIG_RL_BASE_TYPE <<
		NIG_REG_LB_TCRATELIMIT_CTRL_0_LB_TCRATELIMIT_BASE_TYPE_0_SHIFT;
		ecore_wr(p_hwfn, p_ptt,
			 NIG_REG_LB_TCRATELIMIT_CTRL_0 + reg_offset, ctrl);
		/* configure and enable TC RL */
		if (req->tc_rate[tc]) {
			/* configure */
			ecore_wr(p_hwfn, p_ptt,
				 NIG_REG_LB_TCRATELIMIT_INC_PERIOD_0 +
				 reg_offset, NIG_RL_PERIOD_CLK_25M);
			inc_val = NIG_RL_INC_VAL(req->tc_rate[tc]);
			ecore_wr(p_hwfn, p_ptt,
				 NIG_REG_LB_TCRATELIMIT_INC_VALUE_0 +
				 reg_offset, inc_val);
			ecore_wr(p_hwfn, p_ptt,
				 NIG_REG_LB_TCRATELIMIT_MAX_VALUE_0 +
				 reg_offset, NIG_RL_MAX_VAL(inc_val, req->mtu));
			/* enable */
			ctrl |=
			    1 <<
		NIG_REG_LB_TCRATELIMIT_CTRL_0_LB_TCRATELIMIT_EN_0_SHIFT;
			ecore_wr(p_hwfn, p_ptt,
				 NIG_REG_LB_TCRATELIMIT_CTRL_0 + reg_offset,
				 ctrl);
		}
	}
}

void ecore_init_nig_pri_tc_map(struct ecore_hwfn *p_hwfn,
			       struct ecore_ptt *p_ptt,
			       struct init_nig_pri_tc_map_req *req)
{
	u8 pri, tc;
	u32 pri_tc_mask = 0;
	u8 tc_pri_mask[NUM_OF_PHYS_TCS] = { 0 };
	for (pri = 0; pri < NUM_OF_VLAN_PRIORITIES; pri++) {
		if (req->pri[pri].valid) {
			pri_tc_mask |=
			    (req->pri[pri].
			     tc_id << (pri * NIG_PRIORITY_MAP_TC_BITS));
			tc_pri_mask[req->pri[pri].tc_id] |= (1 << pri);
		}
	}
	/* write priority -> TC mask */
	ecore_wr(p_hwfn, p_ptt, NIG_REG_PKT_PRIORITY_TO_TC, pri_tc_mask);
	/* write TC -> priority mask */
	for (tc = 0; tc < NUM_OF_PHYS_TCS; tc++) {
		ecore_wr(p_hwfn, p_ptt, NIG_REG_PRIORITY_FOR_TC_0 + tc * 4,
			 tc_pri_mask[tc]);
		ecore_wr(p_hwfn, p_ptt, NIG_REG_RX_TC0_PRIORITY_MASK + tc * 4,
			 tc_pri_mask[tc]);
	}
}

/* PRS: ETS configuration constants */
#define PRS_ETS_MIN_WFQ_BYTES			1600
#define PRS_ETS_UP_BOUND(weight, mtu) \
(2 * ((weight) > (mtu) ? (weight) : (mtu)))
void ecore_init_prs_ets(struct ecore_hwfn *p_hwfn,
			struct ecore_ptt *p_ptt, struct init_ets_req *req)
{
	u8 tc, sp_tc_map = 0, wfq_tc_map = 0;
	u32 min_weight = 0xffffffff;
	u32 tc_weight_addr_diff =
	    PRS_REG_ETS_ARB_CREDIT_WEIGHT_1 - PRS_REG_ETS_ARB_CREDIT_WEIGHT_0;
	u32 tc_bound_addr_diff =
	    PRS_REG_ETS_ARB_CREDIT_UPPER_BOUND_1 -
	    PRS_REG_ETS_ARB_CREDIT_UPPER_BOUND_0;
	for (tc = 0; tc < NUM_OF_TCS; tc++) {
		struct init_ets_tc_req *tc_req = &req->tc_req[tc];
		/* update SP map */
		if (tc_req->use_sp)
			sp_tc_map |= (1 << tc);
		if (tc_req->use_wfq) {
			/* update WFQ map */
			wfq_tc_map |= (1 << tc);
			/* find minimal weight */
			if (tc_req->weight < min_weight)
				min_weight = tc_req->weight;
		}
	}
	/* write SP map */
	ecore_wr(p_hwfn, p_ptt, PRS_REG_ETS_ARB_CLIENT_IS_STRICT, sp_tc_map);
	/* write WFQ map */
	ecore_wr(p_hwfn, p_ptt, PRS_REG_ETS_ARB_CLIENT_IS_SUBJECT2WFQ,
		 wfq_tc_map);
	/* write WFQ weights */
	for (tc = 0; tc < NUM_OF_TCS; tc++) {
		struct init_ets_tc_req *tc_req = &req->tc_req[tc];
		if (tc_req->use_wfq) {
			/* translate weight to bytes */
			u32 byte_weight =
			    (PRS_ETS_MIN_WFQ_BYTES * tc_req->weight) /
			    min_weight;
			/* write WFQ weight */
			ecore_wr(p_hwfn, p_ptt,
				 PRS_REG_ETS_ARB_CREDIT_WEIGHT_0 +
				 tc * tc_weight_addr_diff, byte_weight);
			/* write WFQ upper bound */
			ecore_wr(p_hwfn, p_ptt,
				 PRS_REG_ETS_ARB_CREDIT_UPPER_BOUND_0 +
				 tc * tc_bound_addr_diff,
				 PRS_ETS_UP_BOUND(byte_weight, req->mtu));
		}
	}
}

/* BRB: RAM configuration constants */
#define BRB_TOTAL_RAM_BLOCKS_BB	4800
#define BRB_TOTAL_RAM_BLOCKS_K2	5632
#define BRB_BLOCK_SIZE			128	/* in bytes */
#define BRB_MIN_BLOCKS_PER_TC	9
#define BRB_HYST_BYTES			10240
#define BRB_HYST_BLOCKS			(BRB_HYST_BYTES / BRB_BLOCK_SIZE)
/*
 * temporary big RAM allocation - should be updated
 */
void ecore_init_brb_ram(struct ecore_hwfn *p_hwfn,
			struct ecore_ptt *p_ptt, struct init_brb_ram_req *req)
{
	u8 port, active_ports = 0;
	u32 active_port_blocks, reg_offset = 0;
	u32 tc_headroom_blocks =
	    (u32)DIV_ROUND_UP(req->headroom_per_tc, BRB_BLOCK_SIZE);
	u32 min_pkt_size_blocks =
	    (u32)DIV_ROUND_UP(req->min_pkt_size, BRB_BLOCK_SIZE);
	u32 total_blocks =
	    ECORE_IS_K2(p_hwfn->
			p_dev) ? BRB_TOTAL_RAM_BLOCKS_K2 :
	    BRB_TOTAL_RAM_BLOCKS_BB;
	/* find number of active ports */
	for (port = 0; port < MAX_NUM_PORTS; port++)
		if (req->num_active_tcs[port])
			active_ports++;
	active_port_blocks = (u32)(total_blocks / active_ports);
	for (port = 0; port < req->max_ports_per_engine; port++) {
		/* calculate per-port sizes */
		u32 tc_guaranteed_blocks =
		    (u32)DIV_ROUND_UP(req->guranteed_per_tc, BRB_BLOCK_SIZE);
		u32 port_blocks =
		    req->num_active_tcs[port] ? active_port_blocks : 0;
		u32 port_guaranteed_blocks =
		    req->num_active_tcs[port] * tc_guaranteed_blocks;
		u32 port_shared_blocks = port_blocks - port_guaranteed_blocks;
		u32 full_xoff_th =
		    req->num_active_tcs[port] * BRB_MIN_BLOCKS_PER_TC;
		u32 full_xon_th = full_xoff_th + min_pkt_size_blocks;
		u32 pause_xoff_th = tc_headroom_blocks;
		u32 pause_xon_th = pause_xoff_th + min_pkt_size_blocks;
		u8 tc;
		/* init total size per port */
		ecore_wr(p_hwfn, p_ptt, BRB_REG_TOTAL_MAC_SIZE + port * 4,
			 port_blocks);
		/* init shared size per port */
		ecore_wr(p_hwfn, p_ptt, BRB_REG_SHARED_HR_AREA + port * 4,
			 port_shared_blocks);
		for (tc = 0; tc < NUM_OF_TCS; tc++, reg_offset += 4) {
			/* clear init values for non-active TCs */
			if (tc == req->num_active_tcs[port]) {
				tc_guaranteed_blocks = 0;
				full_xoff_th = 0;
				full_xon_th = 0;
				pause_xoff_th = 0;
				pause_xon_th = 0;
			}
			/* init guaranteed size per TC */
			ecore_wr(p_hwfn, p_ptt,
				 BRB_REG_TC_GUARANTIED_0 + reg_offset,
				 tc_guaranteed_blocks);
			ecore_wr(p_hwfn, p_ptt,
				 BRB_REG_MAIN_TC_GUARANTIED_HYST_0 + reg_offset,
				 BRB_HYST_BLOCKS);
			ecore_wr(p_hwfn, p_ptt,
				 BRB_REG_LB_TC_FULL_XOFF_THRESHOLD_0 +
				 reg_offset, full_xoff_th);
			ecore_wr(p_hwfn, p_ptt,
				 BRB_REG_LB_TC_FULL_XON_THRESHOLD_0 +
				 reg_offset, full_xon_th);
			ecore_wr(p_hwfn, p_ptt,
				 BRB_REG_LB_TC_PAUSE_XOFF_THRESHOLD_0 +
				 reg_offset, pause_xoff_th);
			ecore_wr(p_hwfn, p_ptt,
				 BRB_REG_LB_TC_PAUSE_XON_THRESHOLD_0 +
				 reg_offset, pause_xon_th);
			ecore_wr(p_hwfn, p_ptt,
				 BRB_REG_MAIN_TC_FULL_XOFF_THRESHOLD_0 +
				 reg_offset, full_xoff_th);
			ecore_wr(p_hwfn, p_ptt,
				 BRB_REG_MAIN_TC_FULL_XON_THRESHOLD_0 +
				 reg_offset, full_xon_th);
			ecore_wr(p_hwfn, p_ptt,
				 BRB_REG_MAIN_TC_PAUSE_XOFF_THRESHOLD_0 +
				 reg_offset, pause_xoff_th);
			ecore_wr(p_hwfn, p_ptt,
				 BRB_REG_MAIN_TC_PAUSE_XON_THRESHOLD_0 +
				 reg_offset, pause_xon_th);
		}
	}
}

/*In MF should be called once per engine to set EtherType of OuterTag*/
void ecore_set_engine_mf_ovlan_eth_type(struct ecore_hwfn *p_hwfn,
					struct ecore_ptt *p_ptt, u32 eth_type)
{
	/* update PRS register */
	STORE_RT_REG(p_hwfn, PRS_REG_TAG_ETHERTYPE_0_RT_OFFSET, eth_type);
	/* update NIG register */
	STORE_RT_REG(p_hwfn, NIG_REG_TAG_ETHERTYPE_0_RT_OFFSET, eth_type);
	/* update PBF register */
	STORE_RT_REG(p_hwfn, PBF_REG_TAG_ETHERTYPE_0_RT_OFFSET, eth_type);
}

/*In MF should be called once per port to set EtherType of OuterTag*/
void ecore_set_port_mf_ovlan_eth_type(struct ecore_hwfn *p_hwfn,
				      struct ecore_ptt *p_ptt, u32 eth_type)
{
	/* update DORQ register */
	STORE_RT_REG(p_hwfn, DORQ_REG_TAG1_ETHERTYPE_RT_OFFSET, eth_type);
}

#define SET_TUNNEL_TYPE_ENABLE_BIT(var, offset, enable) \
(var = ((var) & ~(1 << (offset))) | ((enable) ? (1 << (offset)) : 0))
#define PRS_ETH_TUNN_FIC_FORMAT        -188897008
void ecore_set_vxlan_dest_port(struct ecore_hwfn *p_hwfn,
			       struct ecore_ptt *p_ptt, u16 dest_port)
{
	/* update PRS register */
	ecore_wr(p_hwfn, p_ptt, PRS_REG_VXLAN_PORT, dest_port);
	/* update NIG register */
	ecore_wr(p_hwfn, p_ptt, NIG_REG_VXLAN_PORT, dest_port);
	/* update PBF register */
	ecore_wr(p_hwfn, p_ptt, PBF_REG_VXLAN_PORT, dest_port);
}

void ecore_set_vxlan_enable(struct ecore_hwfn *p_hwfn,
			    struct ecore_ptt *p_ptt, bool vxlan_enable)
{
	u32 reg_val;
	/* update PRS register */
	reg_val = ecore_rd(p_hwfn, p_ptt, PRS_REG_ENCAPSULATION_TYPE_EN);
	SET_TUNNEL_TYPE_ENABLE_BIT(reg_val,
			   PRS_REG_ENCAPSULATION_TYPE_EN_VXLAN_ENABLE_SHIFT,
			   vxlan_enable);
	ecore_wr(p_hwfn, p_ptt, PRS_REG_ENCAPSULATION_TYPE_EN, reg_val);
	if (reg_val) {
		ecore_wr(p_hwfn, p_ptt, PRS_REG_OUTPUT_FORMAT_4_0,
			 PRS_ETH_TUNN_FIC_FORMAT);
	}
	/* update NIG register */
	reg_val = ecore_rd(p_hwfn, p_ptt, NIG_REG_ENC_TYPE_ENABLE);
	SET_TUNNEL_TYPE_ENABLE_BIT(reg_val,
				   NIG_REG_ENC_TYPE_ENABLE_VXLAN_ENABLE_SHIFT,
				   vxlan_enable);
	ecore_wr(p_hwfn, p_ptt, NIG_REG_ENC_TYPE_ENABLE, reg_val);
	/* update DORQ register */
	ecore_wr(p_hwfn, p_ptt, DORQ_REG_L2_EDPM_TUNNEL_VXLAN_EN,
		 vxlan_enable ? 1 : 0);
}

void ecore_set_gre_enable(struct ecore_hwfn *p_hwfn,
			  struct ecore_ptt *p_ptt,
			  bool eth_gre_enable, bool ip_gre_enable)
{
	u32 reg_val;
	/* update PRS register */
	reg_val = ecore_rd(p_hwfn, p_ptt, PRS_REG_ENCAPSULATION_TYPE_EN);
	SET_TUNNEL_TYPE_ENABLE_BIT(reg_val,
		   PRS_REG_ENCAPSULATION_TYPE_EN_ETH_OVER_GRE_ENABLE_SHIFT,
		   eth_gre_enable);
	SET_TUNNEL_TYPE_ENABLE_BIT(reg_val,
		   PRS_REG_ENCAPSULATION_TYPE_EN_IP_OVER_GRE_ENABLE_SHIFT,
		   ip_gre_enable);
	ecore_wr(p_hwfn, p_ptt, PRS_REG_ENCAPSULATION_TYPE_EN, reg_val);
	if (reg_val) {
		ecore_wr(p_hwfn, p_ptt, PRS_REG_OUTPUT_FORMAT_4_0,
			 PRS_ETH_TUNN_FIC_FORMAT);
	}
	/* update NIG register */
	reg_val = ecore_rd(p_hwfn, p_ptt, NIG_REG_ENC_TYPE_ENABLE);
	SET_TUNNEL_TYPE_ENABLE_BIT(reg_val,
		   NIG_REG_ENC_TYPE_ENABLE_ETH_OVER_GRE_ENABLE_SHIFT,
		   eth_gre_enable);
	SET_TUNNEL_TYPE_ENABLE_BIT(reg_val,
		   NIG_REG_ENC_TYPE_ENABLE_IP_OVER_GRE_ENABLE_SHIFT,
		   ip_gre_enable);
	ecore_wr(p_hwfn, p_ptt, NIG_REG_ENC_TYPE_ENABLE, reg_val);
	/* update DORQ registers */
	ecore_wr(p_hwfn, p_ptt, DORQ_REG_L2_EDPM_TUNNEL_GRE_ETH_EN,
		 eth_gre_enable ? 1 : 0);
	ecore_wr(p_hwfn, p_ptt, DORQ_REG_L2_EDPM_TUNNEL_GRE_IP_EN,
		 ip_gre_enable ? 1 : 0);
}

void ecore_set_geneve_dest_port(struct ecore_hwfn *p_hwfn,
				struct ecore_ptt *p_ptt, u16 dest_port)
{
	/* geneve tunnel not supported in BB_A0 */
	if (ECORE_IS_BB_A0(p_hwfn->p_dev))
		return;
	/* update PRS register */
	ecore_wr(p_hwfn, p_ptt, PRS_REG_NGE_PORT, dest_port);
	/* update NIG register */
	ecore_wr(p_hwfn, p_ptt, NIG_REG_NGE_PORT, dest_port);
	/* update PBF register */
	ecore_wr(p_hwfn, p_ptt, PBF_REG_NGE_PORT, dest_port);
}

void ecore_set_geneve_enable(struct ecore_hwfn *p_hwfn,
			     struct ecore_ptt *p_ptt,
			     bool eth_geneve_enable, bool ip_geneve_enable)
{
	u32 reg_val;
	/* geneve tunnel not supported in BB_A0 */
	if (ECORE_IS_BB_A0(p_hwfn->p_dev))
		return;
	/* update PRS register */
	reg_val = ecore_rd(p_hwfn, p_ptt, PRS_REG_ENCAPSULATION_TYPE_EN);
	SET_TUNNEL_TYPE_ENABLE_BIT(reg_val,
		   PRS_REG_ENCAPSULATION_TYPE_EN_ETH_OVER_GENEVE_ENABLE_SHIFT,
		   eth_geneve_enable);
	SET_TUNNEL_TYPE_ENABLE_BIT(reg_val,
		   PRS_REG_ENCAPSULATION_TYPE_EN_IP_OVER_GENEVE_ENABLE_SHIFT,
		   ip_geneve_enable);
	ecore_wr(p_hwfn, p_ptt, PRS_REG_ENCAPSULATION_TYPE_EN, reg_val);
	if (reg_val) {
		ecore_wr(p_hwfn, p_ptt, PRS_REG_OUTPUT_FORMAT_4_0,
			 PRS_ETH_TUNN_FIC_FORMAT);
	}
	/* update NIG register */
	ecore_wr(p_hwfn, p_ptt, NIG_REG_NGE_ETH_ENABLE,
		 eth_geneve_enable ? 1 : 0);
	ecore_wr(p_hwfn, p_ptt, NIG_REG_NGE_IP_ENABLE,
		 ip_geneve_enable ? 1 : 0);
	/* comp ver */
	reg_val = (ip_geneve_enable || eth_geneve_enable) ? 1 : 0;
	ecore_wr(p_hwfn, p_ptt, NIG_REG_NGE_COMP_VER, reg_val);
	ecore_wr(p_hwfn, p_ptt, PBF_REG_NGE_COMP_VER, reg_val);
	ecore_wr(p_hwfn, p_ptt, PRS_REG_NGE_COMP_VER, reg_val);
	/* EDPM with geneve tunnel not supported in BB_B0 */
	if (ECORE_IS_BB_B0(p_hwfn->p_dev))
		return;
	/* update DORQ registers */
	ecore_wr(p_hwfn, p_ptt, DORQ_REG_L2_EDPM_TUNNEL_NGE_ETH_EN,
		 eth_geneve_enable ? 1 : 0);
	ecore_wr(p_hwfn, p_ptt, DORQ_REG_L2_EDPM_TUNNEL_NGE_IP_EN,
		 ip_geneve_enable ? 1 : 0);
}
