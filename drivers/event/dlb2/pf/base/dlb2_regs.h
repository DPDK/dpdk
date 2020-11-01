/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2020 Intel Corporation
 */

#ifndef __DLB2_REGS_H
#define __DLB2_REGS_H

#include "dlb2_osdep_types.h"

#define DLB2_FUNC_PF_VF2PF_MAILBOX_BYTES 256
#define DLB2_FUNC_PF_VF2PF_MAILBOX(vf_id, x) \
	(0x1000 + 0x4 * (x) + (vf_id) * 0x10000)
#define DLB2_FUNC_PF_VF2PF_MAILBOX_RST 0x0
union dlb2_func_pf_vf2pf_mailbox {
	struct {
		u32 msg : 32;
	} field;
	u32 val;
};

#define DLB2_FUNC_PF_VF2PF_MAILBOX_ISR(vf_id) \
	(0x1f00 + (vf_id) * 0x10000)
#define DLB2_FUNC_PF_VF2PF_MAILBOX_ISR_RST 0x0
union dlb2_func_pf_vf2pf_mailbox_isr {
	struct {
		u32 vf0_isr : 1;
		u32 vf1_isr : 1;
		u32 vf2_isr : 1;
		u32 vf3_isr : 1;
		u32 vf4_isr : 1;
		u32 vf5_isr : 1;
		u32 vf6_isr : 1;
		u32 vf7_isr : 1;
		u32 vf8_isr : 1;
		u32 vf9_isr : 1;
		u32 vf10_isr : 1;
		u32 vf11_isr : 1;
		u32 vf12_isr : 1;
		u32 vf13_isr : 1;
		u32 vf14_isr : 1;
		u32 vf15_isr : 1;
		u32 rsvd0 : 16;
	} field;
	u32 val;
};

#define DLB2_FUNC_PF_VF2PF_FLR_ISR(vf_id) \
	(0x1f04 + (vf_id) * 0x10000)
#define DLB2_FUNC_PF_VF2PF_FLR_ISR_RST 0x0
union dlb2_func_pf_vf2pf_flr_isr {
	struct {
		u32 vf0_isr : 1;
		u32 vf1_isr : 1;
		u32 vf2_isr : 1;
		u32 vf3_isr : 1;
		u32 vf4_isr : 1;
		u32 vf5_isr : 1;
		u32 vf6_isr : 1;
		u32 vf7_isr : 1;
		u32 vf8_isr : 1;
		u32 vf9_isr : 1;
		u32 vf10_isr : 1;
		u32 vf11_isr : 1;
		u32 vf12_isr : 1;
		u32 vf13_isr : 1;
		u32 vf14_isr : 1;
		u32 vf15_isr : 1;
		u32 rsvd0 : 16;
	} field;
	u32 val;
};

#define DLB2_FUNC_PF_VF2PF_ISR_PEND(vf_id) \
	(0x1f10 + (vf_id) * 0x10000)
#define DLB2_FUNC_PF_VF2PF_ISR_PEND_RST 0x0
union dlb2_func_pf_vf2pf_isr_pend {
	struct {
		u32 isr_pend : 1;
		u32 rsvd0 : 31;
	} field;
	u32 val;
};

#define DLB2_FUNC_PF_PF2VF_MAILBOX_BYTES 64
#define DLB2_FUNC_PF_PF2VF_MAILBOX(vf_id, x) \
	(0x2000 + 0x4 * (x) + (vf_id) * 0x10000)
#define DLB2_FUNC_PF_PF2VF_MAILBOX_RST 0x0
union dlb2_func_pf_pf2vf_mailbox {
	struct {
		u32 msg : 32;
	} field;
	u32 val;
};

#define DLB2_FUNC_PF_PF2VF_MAILBOX_ISR(vf_id) \
	(0x2f00 + (vf_id) * 0x10000)
#define DLB2_FUNC_PF_PF2VF_MAILBOX_ISR_RST 0x0
union dlb2_func_pf_pf2vf_mailbox_isr {
	struct {
		u32 vf0_isr : 1;
		u32 vf1_isr : 1;
		u32 vf2_isr : 1;
		u32 vf3_isr : 1;
		u32 vf4_isr : 1;
		u32 vf5_isr : 1;
		u32 vf6_isr : 1;
		u32 vf7_isr : 1;
		u32 vf8_isr : 1;
		u32 vf9_isr : 1;
		u32 vf10_isr : 1;
		u32 vf11_isr : 1;
		u32 vf12_isr : 1;
		u32 vf13_isr : 1;
		u32 vf14_isr : 1;
		u32 vf15_isr : 1;
		u32 rsvd0 : 16;
	} field;
	u32 val;
};

#define DLB2_FUNC_PF_VF_RESET_IN_PROGRESS(vf_id) \
	(0x3000 + (vf_id) * 0x10000)
#define DLB2_FUNC_PF_VF_RESET_IN_PROGRESS_RST 0xffff
union dlb2_func_pf_vf_reset_in_progress {
	struct {
		u32 vf0_reset_in_progress : 1;
		u32 vf1_reset_in_progress : 1;
		u32 vf2_reset_in_progress : 1;
		u32 vf3_reset_in_progress : 1;
		u32 vf4_reset_in_progress : 1;
		u32 vf5_reset_in_progress : 1;
		u32 vf6_reset_in_progress : 1;
		u32 vf7_reset_in_progress : 1;
		u32 vf8_reset_in_progress : 1;
		u32 vf9_reset_in_progress : 1;
		u32 vf10_reset_in_progress : 1;
		u32 vf11_reset_in_progress : 1;
		u32 vf12_reset_in_progress : 1;
		u32 vf13_reset_in_progress : 1;
		u32 vf14_reset_in_progress : 1;
		u32 vf15_reset_in_progress : 1;
		u32 rsvd0 : 16;
	} field;
	u32 val;
};

#define DLB2_MSIX_MEM_VECTOR_CTRL(x) \
	(0x100000c + (x) * 0x10)
#define DLB2_MSIX_MEM_VECTOR_CTRL_RST 0x1
union dlb2_msix_mem_vector_ctrl {
	struct {
		u32 vec_mask : 1;
		u32 rsvd0 : 31;
	} field;
	u32 val;
};

#define DLB2_IOSF_FUNC_VF_BAR_DSBL(x) \
	(0x20 + (x) * 0x4)
#define DLB2_IOSF_FUNC_VF_BAR_DSBL_RST 0x0
union dlb2_iosf_func_vf_bar_dsbl {
	struct {
		u32 func_vf_bar_dis : 1;
		u32 rsvd0 : 31;
	} field;
	u32 val;
};

#define DLB2_SYS_TOTAL_VAS 0x1000011c
#define DLB2_SYS_TOTAL_VAS_RST 0x20
union dlb2_sys_total_vas {
	struct {
		u32 total_vas : 32;
	} field;
	u32 val;
};

#define DLB2_SYS_TOTAL_DIR_PORTS 0x10000118
#define DLB2_SYS_TOTAL_DIR_PORTS_RST 0x40
union dlb2_sys_total_dir_ports {
	struct {
		u32 total_dir_ports : 32;
	} field;
	u32 val;
};

#define DLB2_SYS_TOTAL_LDB_PORTS 0x10000114
#define DLB2_SYS_TOTAL_LDB_PORTS_RST 0x40
union dlb2_sys_total_ldb_ports {
	struct {
		u32 total_ldb_ports : 32;
	} field;
	u32 val;
};

#define DLB2_SYS_TOTAL_DIR_QID 0x10000110
#define DLB2_SYS_TOTAL_DIR_QID_RST 0x40
union dlb2_sys_total_dir_qid {
	struct {
		u32 total_dir_qid : 32;
	} field;
	u32 val;
};

#define DLB2_SYS_TOTAL_LDB_QID 0x1000010c
#define DLB2_SYS_TOTAL_LDB_QID_RST 0x20
union dlb2_sys_total_ldb_qid {
	struct {
		u32 total_ldb_qid : 32;
	} field;
	u32 val;
};

#define DLB2_SYS_TOTAL_DIR_CRDS 0x10000108
#define DLB2_SYS_TOTAL_DIR_CRDS_RST 0x1000
union dlb2_sys_total_dir_crds {
	struct {
		u32 total_dir_credits : 32;
	} field;
	u32 val;
};

#define DLB2_SYS_TOTAL_LDB_CRDS 0x10000104
#define DLB2_SYS_TOTAL_LDB_CRDS_RST 0x2000
union dlb2_sys_total_ldb_crds {
	struct {
		u32 total_ldb_credits : 32;
	} field;
	u32 val;
};

#define DLB2_SYS_ALARM_PF_SYND2 0x10000508
#define DLB2_SYS_ALARM_PF_SYND2_RST 0x0
union dlb2_sys_alarm_pf_synd2 {
	struct {
		u32 lock_id : 16;
		u32 meas : 1;
		u32 debug : 7;
		u32 cq_pop : 1;
		u32 qe_uhl : 1;
		u32 qe_orsp : 1;
		u32 qe_valid : 1;
		u32 cq_int_rearm : 1;
		u32 dsi_error : 1;
		u32 rsvd0 : 2;
	} field;
	u32 val;
};

#define DLB2_SYS_ALARM_PF_SYND1 0x10000504
#define DLB2_SYS_ALARM_PF_SYND1_RST 0x0
union dlb2_sys_alarm_pf_synd1 {
	struct {
		u32 dsi : 16;
		u32 qid : 8;
		u32 qtype : 2;
		u32 qpri : 3;
		u32 msg_type : 3;
	} field;
	u32 val;
};

#define DLB2_SYS_ALARM_PF_SYND0 0x10000500
#define DLB2_SYS_ALARM_PF_SYND0_RST 0x0
union dlb2_sys_alarm_pf_synd0 {
	struct {
		u32 syndrome : 8;
		u32 rtype : 2;
		u32 rsvd0 : 3;
		u32 is_ldb : 1;
		u32 cls : 2;
		u32 aid : 6;
		u32 unit : 4;
		u32 source : 4;
		u32 more : 1;
		u32 valid : 1;
	} field;
	u32 val;
};

#define DLB2_SYS_VF_LDB_VPP_V(x) \
	(0x10000f00 + (x) * 0x1000)
#define DLB2_SYS_VF_LDB_VPP_V_RST 0x0
union dlb2_sys_vf_ldb_vpp_v {
	struct {
		u32 vpp_v : 1;
		u32 rsvd0 : 31;
	} field;
	u32 val;
};

#define DLB2_SYS_VF_LDB_VPP2PP(x) \
	(0x10000f04 + (x) * 0x1000)
#define DLB2_SYS_VF_LDB_VPP2PP_RST 0x0
union dlb2_sys_vf_ldb_vpp2pp {
	struct {
		u32 pp : 6;
		u32 rsvd0 : 26;
	} field;
	u32 val;
};

#define DLB2_SYS_VF_DIR_VPP_V(x) \
	(0x10000f08 + (x) * 0x1000)
#define DLB2_SYS_VF_DIR_VPP_V_RST 0x0
union dlb2_sys_vf_dir_vpp_v {
	struct {
		u32 vpp_v : 1;
		u32 rsvd0 : 31;
	} field;
	u32 val;
};

#define DLB2_SYS_VF_DIR_VPP2PP(x) \
	(0x10000f0c + (x) * 0x1000)
#define DLB2_SYS_VF_DIR_VPP2PP_RST 0x0
union dlb2_sys_vf_dir_vpp2pp {
	struct {
		u32 pp : 6;
		u32 rsvd0 : 26;
	} field;
	u32 val;
};

#define DLB2_SYS_VF_LDB_VQID_V(x) \
	(0x10000f10 + (x) * 0x1000)
#define DLB2_SYS_VF_LDB_VQID_V_RST 0x0
union dlb2_sys_vf_ldb_vqid_v {
	struct {
		u32 vqid_v : 1;
		u32 rsvd0 : 31;
	} field;
	u32 val;
};

#define DLB2_SYS_VF_LDB_VQID2QID(x) \
	(0x10000f14 + (x) * 0x1000)
#define DLB2_SYS_VF_LDB_VQID2QID_RST 0x0
union dlb2_sys_vf_ldb_vqid2qid {
	struct {
		u32 qid : 5;
		u32 rsvd0 : 27;
	} field;
	u32 val;
};

#define DLB2_SYS_LDB_QID2VQID(x) \
	(0x10000f18 + (x) * 0x1000)
#define DLB2_SYS_LDB_QID2VQID_RST 0x0
union dlb2_sys_ldb_qid2vqid {
	struct {
		u32 vqid : 5;
		u32 rsvd0 : 27;
	} field;
	u32 val;
};

#define DLB2_SYS_VF_DIR_VQID_V(x) \
	(0x10000f1c + (x) * 0x1000)
#define DLB2_SYS_VF_DIR_VQID_V_RST 0x0
union dlb2_sys_vf_dir_vqid_v {
	struct {
		u32 vqid_v : 1;
		u32 rsvd0 : 31;
	} field;
	u32 val;
};

#define DLB2_SYS_VF_DIR_VQID2QID(x) \
	(0x10000f20 + (x) * 0x1000)
#define DLB2_SYS_VF_DIR_VQID2QID_RST 0x0
union dlb2_sys_vf_dir_vqid2qid {
	struct {
		u32 qid : 6;
		u32 rsvd0 : 26;
	} field;
	u32 val;
};

#define DLB2_SYS_LDB_VASQID_V(x) \
	(0x10000f24 + (x) * 0x1000)
#define DLB2_SYS_LDB_VASQID_V_RST 0x0
union dlb2_sys_ldb_vasqid_v {
	struct {
		u32 vasqid_v : 1;
		u32 rsvd0 : 31;
	} field;
	u32 val;
};

#define DLB2_SYS_DIR_VASQID_V(x) \
	(0x10000f28 + (x) * 0x1000)
#define DLB2_SYS_DIR_VASQID_V_RST 0x0
union dlb2_sys_dir_vasqid_v {
	struct {
		u32 vasqid_v : 1;
		u32 rsvd0 : 31;
	} field;
	u32 val;
};

#define DLB2_SYS_ALARM_VF_SYND2(x) \
	(0x10000f48 + (x) * 0x1000)
#define DLB2_SYS_ALARM_VF_SYND2_RST 0x0
union dlb2_sys_alarm_vf_synd2 {
	struct {
		u32 lock_id : 16;
		u32 debug : 8;
		u32 cq_pop : 1;
		u32 qe_uhl : 1;
		u32 qe_orsp : 1;
		u32 qe_valid : 1;
		u32 isz : 1;
		u32 dsi_error : 1;
		u32 dlbrsvd : 2;
	} field;
	u32 val;
};

#define DLB2_SYS_ALARM_VF_SYND1(x) \
	(0x10000f44 + (x) * 0x1000)
#define DLB2_SYS_ALARM_VF_SYND1_RST 0x0
union dlb2_sys_alarm_vf_synd1 {
	struct {
		u32 dsi : 16;
		u32 qid : 8;
		u32 qtype : 2;
		u32 qpri : 3;
		u32 msg_type : 3;
	} field;
	u32 val;
};

#define DLB2_SYS_ALARM_VF_SYND0(x) \
	(0x10000f40 + (x) * 0x1000)
#define DLB2_SYS_ALARM_VF_SYND0_RST 0x0
union dlb2_sys_alarm_vf_synd0 {
	struct {
		u32 syndrome : 8;
		u32 rtype : 2;
		u32 vf_synd0_parity : 1;
		u32 vf_synd1_parity : 1;
		u32 vf_synd2_parity : 1;
		u32 is_ldb : 1;
		u32 cls : 2;
		u32 aid : 6;
		u32 unit : 4;
		u32 source : 4;
		u32 more : 1;
		u32 valid : 1;
	} field;
	u32 val;
};

#define DLB2_SYS_LDB_QID_CFG_V(x) \
	(0x10000f58 + (x) * 0x1000)
#define DLB2_SYS_LDB_QID_CFG_V_RST 0x0
union dlb2_sys_ldb_qid_cfg_v {
	struct {
		u32 sn_cfg_v : 1;
		u32 fid_cfg_v : 1;
		u32 rsvd0 : 30;
	} field;
	u32 val;
};

#define DLB2_SYS_LDB_QID_ITS(x) \
	(0x10000f54 + (x) * 0x1000)
#define DLB2_SYS_LDB_QID_ITS_RST 0x0
union dlb2_sys_ldb_qid_its {
	struct {
		u32 qid_its : 1;
		u32 rsvd0 : 31;
	} field;
	u32 val;
};

#define DLB2_SYS_LDB_QID_V(x) \
	(0x10000f50 + (x) * 0x1000)
#define DLB2_SYS_LDB_QID_V_RST 0x0
union dlb2_sys_ldb_qid_v {
	struct {
		u32 qid_v : 1;
		u32 rsvd0 : 31;
	} field;
	u32 val;
};

#define DLB2_SYS_DIR_QID_ITS(x) \
	(0x10000f64 + (x) * 0x1000)
#define DLB2_SYS_DIR_QID_ITS_RST 0x0
union dlb2_sys_dir_qid_its {
	struct {
		u32 qid_its : 1;
		u32 rsvd0 : 31;
	} field;
	u32 val;
};

#define DLB2_SYS_DIR_QID_V(x) \
	(0x10000f60 + (x) * 0x1000)
#define DLB2_SYS_DIR_QID_V_RST 0x0
union dlb2_sys_dir_qid_v {
	struct {
		u32 qid_v : 1;
		u32 rsvd0 : 31;
	} field;
	u32 val;
};

#define DLB2_SYS_LDB_CQ_AI_DATA(x) \
	(0x10000fa8 + (x) * 0x1000)
#define DLB2_SYS_LDB_CQ_AI_DATA_RST 0x0
union dlb2_sys_ldb_cq_ai_data {
	struct {
		u32 cq_ai_data : 32;
	} field;
	u32 val;
};

#define DLB2_SYS_LDB_CQ_AI_ADDR(x) \
	(0x10000fa4 + (x) * 0x1000)
#define DLB2_SYS_LDB_CQ_AI_ADDR_RST 0x0
union dlb2_sys_ldb_cq_ai_addr {
	struct {
		u32 rsvd1 : 2;
		u32 cq_ai_addr : 18;
		u32 rsvd0 : 12;
	} field;
	u32 val;
};

#define DLB2_SYS_LDB_CQ_PASID(x) \
	(0x10000fa0 + (x) * 0x1000)
#define DLB2_SYS_LDB_CQ_PASID_RST 0x0
union dlb2_sys_ldb_cq_pasid {
	struct {
		u32 pasid : 20;
		u32 exe_req : 1;
		u32 priv_req : 1;
		u32 fmt2 : 1;
		u32 rsvd0 : 9;
	} field;
	u32 val;
};

#define DLB2_SYS_LDB_CQ_AT(x) \
	(0x10000f9c + (x) * 0x1000)
#define DLB2_SYS_LDB_CQ_AT_RST 0x0
union dlb2_sys_ldb_cq_at {
	struct {
		u32 cq_at : 2;
		u32 rsvd0 : 30;
	} field;
	u32 val;
};

#define DLB2_SYS_LDB_CQ_ISR(x) \
	(0x10000f98 + (x) * 0x1000)
#define DLB2_SYS_LDB_CQ_ISR_RST 0x0
/* CQ Interrupt Modes */
#define DLB2_CQ_ISR_MODE_DIS  0
#define DLB2_CQ_ISR_MODE_MSI  1
#define DLB2_CQ_ISR_MODE_MSIX 2
#define DLB2_CQ_ISR_MODE_ADI  3
union dlb2_sys_ldb_cq_isr {
	struct {
		u32 vector : 6;
		u32 vf : 4;
		u32 en_code : 2;
		u32 rsvd0 : 20;
	} field;
	u32 val;
};

#define DLB2_SYS_LDB_CQ2VF_PF_RO(x) \
	(0x10000f94 + (x) * 0x1000)
#define DLB2_SYS_LDB_CQ2VF_PF_RO_RST 0x0
union dlb2_sys_ldb_cq2vf_pf_ro {
	struct {
		u32 vf : 4;
		u32 is_pf : 1;
		u32 ro : 1;
		u32 rsvd0 : 26;
	} field;
	u32 val;
};

#define DLB2_SYS_LDB_PP_V(x) \
	(0x10000f90 + (x) * 0x1000)
#define DLB2_SYS_LDB_PP_V_RST 0x0
union dlb2_sys_ldb_pp_v {
	struct {
		u32 pp_v : 1;
		u32 rsvd0 : 31;
	} field;
	u32 val;
};

#define DLB2_SYS_LDB_PP2VDEV(x) \
	(0x10000f8c + (x) * 0x1000)
#define DLB2_SYS_LDB_PP2VDEV_RST 0x0
union dlb2_sys_ldb_pp2vdev {
	struct {
		u32 vdev : 4;
		u32 rsvd0 : 28;
	} field;
	u32 val;
};

#define DLB2_SYS_LDB_PP2VAS(x) \
	(0x10000f88 + (x) * 0x1000)
#define DLB2_SYS_LDB_PP2VAS_RST 0x0
union dlb2_sys_ldb_pp2vas {
	struct {
		u32 vas : 5;
		u32 rsvd0 : 27;
	} field;
	u32 val;
};

#define DLB2_SYS_LDB_CQ_ADDR_U(x) \
	(0x10000f84 + (x) * 0x1000)
#define DLB2_SYS_LDB_CQ_ADDR_U_RST 0x0
union dlb2_sys_ldb_cq_addr_u {
	struct {
		u32 addr_u : 32;
	} field;
	u32 val;
};

#define DLB2_SYS_LDB_CQ_ADDR_L(x) \
	(0x10000f80 + (x) * 0x1000)
#define DLB2_SYS_LDB_CQ_ADDR_L_RST 0x0
union dlb2_sys_ldb_cq_addr_l {
	struct {
		u32 rsvd0 : 6;
		u32 addr_l : 26;
	} field;
	u32 val;
};

#define DLB2_SYS_DIR_CQ_FMT(x) \
	(0x10000fec + (x) * 0x1000)
#define DLB2_SYS_DIR_CQ_FMT_RST 0x0
union dlb2_sys_dir_cq_fmt {
	struct {
		u32 keep_pf_ppid : 1;
		u32 rsvd0 : 31;
	} field;
	u32 val;
};

#define DLB2_SYS_DIR_CQ_AI_DATA(x) \
	(0x10000fe8 + (x) * 0x1000)
#define DLB2_SYS_DIR_CQ_AI_DATA_RST 0x0
union dlb2_sys_dir_cq_ai_data {
	struct {
		u32 cq_ai_data : 32;
	} field;
	u32 val;
};

#define DLB2_SYS_DIR_CQ_AI_ADDR(x) \
	(0x10000fe4 + (x) * 0x1000)
#define DLB2_SYS_DIR_CQ_AI_ADDR_RST 0x0
union dlb2_sys_dir_cq_ai_addr {
	struct {
		u32 rsvd1 : 2;
		u32 cq_ai_addr : 18;
		u32 rsvd0 : 12;
	} field;
	u32 val;
};

#define DLB2_SYS_DIR_CQ_PASID(x) \
	(0x10000fe0 + (x) * 0x1000)
#define DLB2_SYS_DIR_CQ_PASID_RST 0x0
union dlb2_sys_dir_cq_pasid {
	struct {
		u32 pasid : 20;
		u32 exe_req : 1;
		u32 priv_req : 1;
		u32 fmt2 : 1;
		u32 rsvd0 : 9;
	} field;
	u32 val;
};

#define DLB2_SYS_DIR_CQ_AT(x) \
	(0x10000fdc + (x) * 0x1000)
#define DLB2_SYS_DIR_CQ_AT_RST 0x0
union dlb2_sys_dir_cq_at {
	struct {
		u32 cq_at : 2;
		u32 rsvd0 : 30;
	} field;
	u32 val;
};

#define DLB2_SYS_DIR_CQ_ISR(x) \
	(0x10000fd8 + (x) * 0x1000)
#define DLB2_SYS_DIR_CQ_ISR_RST 0x0
union dlb2_sys_dir_cq_isr {
	struct {
		u32 vector : 6;
		u32 vf : 4;
		u32 en_code : 2;
		u32 rsvd0 : 20;
	} field;
	u32 val;
};

#define DLB2_SYS_DIR_CQ2VF_PF_RO(x) \
	(0x10000fd4 + (x) * 0x1000)
#define DLB2_SYS_DIR_CQ2VF_PF_RO_RST 0x0
union dlb2_sys_dir_cq2vf_pf_ro {
	struct {
		u32 vf : 4;
		u32 is_pf : 1;
		u32 ro : 1;
		u32 rsvd0 : 26;
	} field;
	u32 val;
};

#define DLB2_SYS_DIR_PP_V(x) \
	(0x10000fd0 + (x) * 0x1000)
#define DLB2_SYS_DIR_PP_V_RST 0x0
union dlb2_sys_dir_pp_v {
	struct {
		u32 pp_v : 1;
		u32 rsvd0 : 31;
	} field;
	u32 val;
};

#define DLB2_SYS_DIR_PP2VDEV(x) \
	(0x10000fcc + (x) * 0x1000)
#define DLB2_SYS_DIR_PP2VDEV_RST 0x0
union dlb2_sys_dir_pp2vdev {
	struct {
		u32 vdev : 4;
		u32 rsvd0 : 28;
	} field;
	u32 val;
};

#define DLB2_SYS_DIR_PP2VAS(x) \
	(0x10000fc8 + (x) * 0x1000)
#define DLB2_SYS_DIR_PP2VAS_RST 0x0
union dlb2_sys_dir_pp2vas {
	struct {
		u32 vas : 5;
		u32 rsvd0 : 27;
	} field;
	u32 val;
};

#define DLB2_SYS_DIR_CQ_ADDR_U(x) \
	(0x10000fc4 + (x) * 0x1000)
#define DLB2_SYS_DIR_CQ_ADDR_U_RST 0x0
union dlb2_sys_dir_cq_addr_u {
	struct {
		u32 addr_u : 32;
	} field;
	u32 val;
};

#define DLB2_SYS_DIR_CQ_ADDR_L(x) \
	(0x10000fc0 + (x) * 0x1000)
#define DLB2_SYS_DIR_CQ_ADDR_L_RST 0x0
union dlb2_sys_dir_cq_addr_l {
	struct {
		u32 rsvd0 : 6;
		u32 addr_l : 26;
	} field;
	u32 val;
};

#define DLB2_SYS_INGRESS_ALARM_ENBL 0x10000300
#define DLB2_SYS_INGRESS_ALARM_ENBL_RST 0x0
union dlb2_sys_ingress_alarm_enbl {
	struct {
		u32 illegal_hcw : 1;
		u32 illegal_pp : 1;
		u32 illegal_pasid : 1;
		u32 illegal_qid : 1;
		u32 disabled_qid : 1;
		u32 illegal_ldb_qid_cfg : 1;
		u32 rsvd0 : 26;
	} field;
	u32 val;
};

#define DLB2_SYS_MSIX_ACK 0x10000400
#define DLB2_SYS_MSIX_ACK_RST 0x0
union dlb2_sys_msix_ack {
	struct {
		u32 msix_0_ack : 1;
		u32 msix_1_ack : 1;
		u32 rsvd0 : 30;
	} field;
	u32 val;
};

#define DLB2_SYS_MSIX_PASSTHRU 0x10000404
#define DLB2_SYS_MSIX_PASSTHRU_RST 0x0
union dlb2_sys_msix_passthru {
	struct {
		u32 msix_0_passthru : 1;
		u32 msix_1_passthru : 1;
		u32 rsvd0 : 30;
	} field;
	u32 val;
};

#define DLB2_SYS_MSIX_MODE 0x10000408
#define DLB2_SYS_MSIX_MODE_RST 0x0
/* MSI-X Modes */
#define DLB2_MSIX_MODE_PACKED     0
#define DLB2_MSIX_MODE_COMPRESSED 1
union dlb2_sys_msix_mode {
	struct {
		u32 mode : 1;
		u32 poll_mode : 1;
		u32 poll_mask : 1;
		u32 poll_lock : 1;
		u32 rsvd0 : 28;
	} field;
	u32 val;
};

#define DLB2_SYS_DIR_CQ_31_0_OCC_INT_STS 0x10000440
#define DLB2_SYS_DIR_CQ_31_0_OCC_INT_STS_RST 0x0
union dlb2_sys_dir_cq_31_0_occ_int_sts {
	struct {
		u32 cq_0_occ_int : 1;
		u32 cq_1_occ_int : 1;
		u32 cq_2_occ_int : 1;
		u32 cq_3_occ_int : 1;
		u32 cq_4_occ_int : 1;
		u32 cq_5_occ_int : 1;
		u32 cq_6_occ_int : 1;
		u32 cq_7_occ_int : 1;
		u32 cq_8_occ_int : 1;
		u32 cq_9_occ_int : 1;
		u32 cq_10_occ_int : 1;
		u32 cq_11_occ_int : 1;
		u32 cq_12_occ_int : 1;
		u32 cq_13_occ_int : 1;
		u32 cq_14_occ_int : 1;
		u32 cq_15_occ_int : 1;
		u32 cq_16_occ_int : 1;
		u32 cq_17_occ_int : 1;
		u32 cq_18_occ_int : 1;
		u32 cq_19_occ_int : 1;
		u32 cq_20_occ_int : 1;
		u32 cq_21_occ_int : 1;
		u32 cq_22_occ_int : 1;
		u32 cq_23_occ_int : 1;
		u32 cq_24_occ_int : 1;
		u32 cq_25_occ_int : 1;
		u32 cq_26_occ_int : 1;
		u32 cq_27_occ_int : 1;
		u32 cq_28_occ_int : 1;
		u32 cq_29_occ_int : 1;
		u32 cq_30_occ_int : 1;
		u32 cq_31_occ_int : 1;
	} field;
	u32 val;
};

#define DLB2_SYS_DIR_CQ_63_32_OCC_INT_STS 0x10000444
#define DLB2_SYS_DIR_CQ_63_32_OCC_INT_STS_RST 0x0
union dlb2_sys_dir_cq_63_32_occ_int_sts {
	struct {
		u32 cq_32_occ_int : 1;
		u32 cq_33_occ_int : 1;
		u32 cq_34_occ_int : 1;
		u32 cq_35_occ_int : 1;
		u32 cq_36_occ_int : 1;
		u32 cq_37_occ_int : 1;
		u32 cq_38_occ_int : 1;
		u32 cq_39_occ_int : 1;
		u32 cq_40_occ_int : 1;
		u32 cq_41_occ_int : 1;
		u32 cq_42_occ_int : 1;
		u32 cq_43_occ_int : 1;
		u32 cq_44_occ_int : 1;
		u32 cq_45_occ_int : 1;
		u32 cq_46_occ_int : 1;
		u32 cq_47_occ_int : 1;
		u32 cq_48_occ_int : 1;
		u32 cq_49_occ_int : 1;
		u32 cq_50_occ_int : 1;
		u32 cq_51_occ_int : 1;
		u32 cq_52_occ_int : 1;
		u32 cq_53_occ_int : 1;
		u32 cq_54_occ_int : 1;
		u32 cq_55_occ_int : 1;
		u32 cq_56_occ_int : 1;
		u32 cq_57_occ_int : 1;
		u32 cq_58_occ_int : 1;
		u32 cq_59_occ_int : 1;
		u32 cq_60_occ_int : 1;
		u32 cq_61_occ_int : 1;
		u32 cq_62_occ_int : 1;
		u32 cq_63_occ_int : 1;
	} field;
	u32 val;
};

#define DLB2_SYS_LDB_CQ_31_0_OCC_INT_STS 0x10000460
#define DLB2_SYS_LDB_CQ_31_0_OCC_INT_STS_RST 0x0
union dlb2_sys_ldb_cq_31_0_occ_int_sts {
	struct {
		u32 cq_0_occ_int : 1;
		u32 cq_1_occ_int : 1;
		u32 cq_2_occ_int : 1;
		u32 cq_3_occ_int : 1;
		u32 cq_4_occ_int : 1;
		u32 cq_5_occ_int : 1;
		u32 cq_6_occ_int : 1;
		u32 cq_7_occ_int : 1;
		u32 cq_8_occ_int : 1;
		u32 cq_9_occ_int : 1;
		u32 cq_10_occ_int : 1;
		u32 cq_11_occ_int : 1;
		u32 cq_12_occ_int : 1;
		u32 cq_13_occ_int : 1;
		u32 cq_14_occ_int : 1;
		u32 cq_15_occ_int : 1;
		u32 cq_16_occ_int : 1;
		u32 cq_17_occ_int : 1;
		u32 cq_18_occ_int : 1;
		u32 cq_19_occ_int : 1;
		u32 cq_20_occ_int : 1;
		u32 cq_21_occ_int : 1;
		u32 cq_22_occ_int : 1;
		u32 cq_23_occ_int : 1;
		u32 cq_24_occ_int : 1;
		u32 cq_25_occ_int : 1;
		u32 cq_26_occ_int : 1;
		u32 cq_27_occ_int : 1;
		u32 cq_28_occ_int : 1;
		u32 cq_29_occ_int : 1;
		u32 cq_30_occ_int : 1;
		u32 cq_31_occ_int : 1;
	} field;
	u32 val;
};

#define DLB2_SYS_LDB_CQ_63_32_OCC_INT_STS 0x10000464
#define DLB2_SYS_LDB_CQ_63_32_OCC_INT_STS_RST 0x0
union dlb2_sys_ldb_cq_63_32_occ_int_sts {
	struct {
		u32 cq_32_occ_int : 1;
		u32 cq_33_occ_int : 1;
		u32 cq_34_occ_int : 1;
		u32 cq_35_occ_int : 1;
		u32 cq_36_occ_int : 1;
		u32 cq_37_occ_int : 1;
		u32 cq_38_occ_int : 1;
		u32 cq_39_occ_int : 1;
		u32 cq_40_occ_int : 1;
		u32 cq_41_occ_int : 1;
		u32 cq_42_occ_int : 1;
		u32 cq_43_occ_int : 1;
		u32 cq_44_occ_int : 1;
		u32 cq_45_occ_int : 1;
		u32 cq_46_occ_int : 1;
		u32 cq_47_occ_int : 1;
		u32 cq_48_occ_int : 1;
		u32 cq_49_occ_int : 1;
		u32 cq_50_occ_int : 1;
		u32 cq_51_occ_int : 1;
		u32 cq_52_occ_int : 1;
		u32 cq_53_occ_int : 1;
		u32 cq_54_occ_int : 1;
		u32 cq_55_occ_int : 1;
		u32 cq_56_occ_int : 1;
		u32 cq_57_occ_int : 1;
		u32 cq_58_occ_int : 1;
		u32 cq_59_occ_int : 1;
		u32 cq_60_occ_int : 1;
		u32 cq_61_occ_int : 1;
		u32 cq_62_occ_int : 1;
		u32 cq_63_occ_int : 1;
	} field;
	u32 val;
};

#define DLB2_SYS_DIR_CQ_OPT_CLR 0x100004c0
#define DLB2_SYS_DIR_CQ_OPT_CLR_RST 0x0
union dlb2_sys_dir_cq_opt_clr {
	struct {
		u32 cq : 6;
		u32 rsvd0 : 26;
	} field;
	u32 val;
};

#define DLB2_SYS_ALARM_HW_SYND 0x1000050c
#define DLB2_SYS_ALARM_HW_SYND_RST 0x0
union dlb2_sys_alarm_hw_synd {
	struct {
		u32 syndrome : 8;
		u32 rtype : 2;
		u32 alarm : 1;
		u32 cwd : 1;
		u32 vf_pf_mb : 1;
		u32 rsvd0 : 1;
		u32 cls : 2;
		u32 aid : 6;
		u32 unit : 4;
		u32 source : 4;
		u32 more : 1;
		u32 valid : 1;
	} field;
	u32 val;
};

#define DLB2_AQED_PIPE_QID_FID_LIM(x) \
	(0x20000000 + (x) * 0x1000)
#define DLB2_AQED_PIPE_QID_FID_LIM_RST 0x7ff
union dlb2_aqed_pipe_qid_fid_lim {
	struct {
		u32 qid_fid_limit : 13;
		u32 rsvd0 : 19;
	} field;
	u32 val;
};

#define DLB2_AQED_PIPE_QID_HID_WIDTH(x) \
	(0x20080000 + (x) * 0x1000)
#define DLB2_AQED_PIPE_QID_HID_WIDTH_RST 0x0
union dlb2_aqed_pipe_qid_hid_width {
	struct {
		u32 compress_code : 3;
		u32 rsvd0 : 29;
	} field;
	u32 val;
};

#define DLB2_AQED_PIPE_CFG_ARB_WEIGHTS_TQPRI_ATM_0 0x24000004
#define DLB2_AQED_PIPE_CFG_ARB_WEIGHTS_TQPRI_ATM_0_RST 0xfefcfaf8
union dlb2_aqed_pipe_cfg_arb_weights_tqpri_atm_0 {
	struct {
		u32 pri0 : 8;
		u32 pri1 : 8;
		u32 pri2 : 8;
		u32 pri3 : 8;
	} field;
	u32 val;
};

#define DLB2_ATM_QID2CQIDIX_00(x) \
	(0x30080000 + (x) * 0x1000)
#define DLB2_ATM_QID2CQIDIX_00_RST 0x0
#define DLB2_ATM_QID2CQIDIX(x, y) \
	(DLB2_ATM_QID2CQIDIX_00(x) + 0x80000 * (y))
#define DLB2_ATM_QID2CQIDIX_NUM 16
union dlb2_atm_qid2cqidix_00 {
	struct {
		u32 cq_p0 : 8;
		u32 cq_p1 : 8;
		u32 cq_p2 : 8;
		u32 cq_p3 : 8;
	} field;
	u32 val;
};

#define DLB2_ATM_CFG_ARB_WEIGHTS_RDY_BIN 0x34000004
#define DLB2_ATM_CFG_ARB_WEIGHTS_RDY_BIN_RST 0xfffefdfc
union dlb2_atm_cfg_arb_weights_rdy_bin {
	struct {
		u32 bin0 : 8;
		u32 bin1 : 8;
		u32 bin2 : 8;
		u32 bin3 : 8;
	} field;
	u32 val;
};

#define DLB2_ATM_CFG_ARB_WEIGHTS_SCHED_BIN 0x34000008
#define DLB2_ATM_CFG_ARB_WEIGHTS_SCHED_BIN_RST 0xfffefdfc
union dlb2_atm_cfg_arb_weights_sched_bin {
	struct {
		u32 bin0 : 8;
		u32 bin1 : 8;
		u32 bin2 : 8;
		u32 bin3 : 8;
	} field;
	u32 val;
};

#define DLB2_CHP_CFG_DIR_VAS_CRD(x) \
	(0x40000000 + (x) * 0x1000)
#define DLB2_CHP_CFG_DIR_VAS_CRD_RST 0x0
union dlb2_chp_cfg_dir_vas_crd {
	struct {
		u32 count : 14;
		u32 rsvd0 : 18;
	} field;
	u32 val;
};

#define DLB2_CHP_CFG_LDB_VAS_CRD(x) \
	(0x40080000 + (x) * 0x1000)
#define DLB2_CHP_CFG_LDB_VAS_CRD_RST 0x0
union dlb2_chp_cfg_ldb_vas_crd {
	struct {
		u32 count : 15;
		u32 rsvd0 : 17;
	} field;
	u32 val;
};

#define DLB2_CHP_ORD_QID_SN(x) \
	(0x40100000 + (x) * 0x1000)
#define DLB2_CHP_ORD_QID_SN_RST 0x0
union dlb2_chp_ord_qid_sn {
	struct {
		u32 sn : 10;
		u32 rsvd0 : 22;
	} field;
	u32 val;
};

#define DLB2_CHP_ORD_QID_SN_MAP(x) \
	(0x40180000 + (x) * 0x1000)
#define DLB2_CHP_ORD_QID_SN_MAP_RST 0x0
union dlb2_chp_ord_qid_sn_map {
	struct {
		u32 mode : 3;
		u32 slot : 4;
		u32 rsvz0 : 1;
		u32 grp : 1;
		u32 rsvz1 : 1;
		u32 rsvd0 : 22;
	} field;
	u32 val;
};

#define DLB2_CHP_SN_CHK_ENBL(x) \
	(0x40200000 + (x) * 0x1000)
#define DLB2_CHP_SN_CHK_ENBL_RST 0x0
union dlb2_chp_sn_chk_enbl {
	struct {
		u32 en : 1;
		u32 rsvd0 : 31;
	} field;
	u32 val;
};

#define DLB2_CHP_DIR_CQ_DEPTH(x) \
	(0x40280000 + (x) * 0x1000)
#define DLB2_CHP_DIR_CQ_DEPTH_RST 0x0
union dlb2_chp_dir_cq_depth {
	struct {
		u32 depth : 13;
		u32 rsvd0 : 19;
	} field;
	u32 val;
};

#define DLB2_CHP_DIR_CQ_INT_DEPTH_THRSH(x) \
	(0x40300000 + (x) * 0x1000)
#define DLB2_CHP_DIR_CQ_INT_DEPTH_THRSH_RST 0x0
union dlb2_chp_dir_cq_int_depth_thrsh {
	struct {
		u32 depth_threshold : 13;
		u32 rsvd0 : 19;
	} field;
	u32 val;
};

#define DLB2_CHP_DIR_CQ_INT_ENB(x) \
	(0x40380000 + (x) * 0x1000)
#define DLB2_CHP_DIR_CQ_INT_ENB_RST 0x0
union dlb2_chp_dir_cq_int_enb {
	struct {
		u32 en_tim : 1;
		u32 en_depth : 1;
		u32 rsvd0 : 30;
	} field;
	u32 val;
};

#define DLB2_CHP_DIR_CQ_TMR_THRSH(x) \
	(0x40480000 + (x) * 0x1000)
#define DLB2_CHP_DIR_CQ_TMR_THRSH_RST 0x1
union dlb2_chp_dir_cq_tmr_thrsh {
	struct {
		u32 thrsh_0 : 1;
		u32 thrsh_13_1 : 13;
		u32 rsvd0 : 18;
	} field;
	u32 val;
};

#define DLB2_CHP_DIR_CQ_TKN_DEPTH_SEL(x) \
	(0x40500000 + (x) * 0x1000)
#define DLB2_CHP_DIR_CQ_TKN_DEPTH_SEL_RST 0x0
union dlb2_chp_dir_cq_tkn_depth_sel {
	struct {
		u32 token_depth_select : 4;
		u32 rsvd0 : 28;
	} field;
	u32 val;
};

#define DLB2_CHP_DIR_CQ_WD_ENB(x) \
	(0x40580000 + (x) * 0x1000)
#define DLB2_CHP_DIR_CQ_WD_ENB_RST 0x0
union dlb2_chp_dir_cq_wd_enb {
	struct {
		u32 wd_enable : 1;
		u32 rsvd0 : 31;
	} field;
	u32 val;
};

#define DLB2_CHP_DIR_CQ_WPTR(x) \
	(0x40600000 + (x) * 0x1000)
#define DLB2_CHP_DIR_CQ_WPTR_RST 0x0
union dlb2_chp_dir_cq_wptr {
	struct {
		u32 write_pointer : 13;
		u32 rsvd0 : 19;
	} field;
	u32 val;
};

#define DLB2_CHP_DIR_CQ2VAS(x) \
	(0x40680000 + (x) * 0x1000)
#define DLB2_CHP_DIR_CQ2VAS_RST 0x0
union dlb2_chp_dir_cq2vas {
	struct {
		u32 cq2vas : 5;
		u32 rsvd0 : 27;
	} field;
	u32 val;
};

#define DLB2_CHP_HIST_LIST_BASE(x) \
	(0x40700000 + (x) * 0x1000)
#define DLB2_CHP_HIST_LIST_BASE_RST 0x0
union dlb2_chp_hist_list_base {
	struct {
		u32 base : 13;
		u32 rsvd0 : 19;
	} field;
	u32 val;
};

#define DLB2_CHP_HIST_LIST_LIM(x) \
	(0x40780000 + (x) * 0x1000)
#define DLB2_CHP_HIST_LIST_LIM_RST 0x0
union dlb2_chp_hist_list_lim {
	struct {
		u32 limit : 13;
		u32 rsvd0 : 19;
	} field;
	u32 val;
};

#define DLB2_CHP_HIST_LIST_POP_PTR(x) \
	(0x40800000 + (x) * 0x1000)
#define DLB2_CHP_HIST_LIST_POP_PTR_RST 0x0
union dlb2_chp_hist_list_pop_ptr {
	struct {
		u32 pop_ptr : 13;
		u32 generation : 1;
		u32 rsvd0 : 18;
	} field;
	u32 val;
};

#define DLB2_CHP_HIST_LIST_PUSH_PTR(x) \
	(0x40880000 + (x) * 0x1000)
#define DLB2_CHP_HIST_LIST_PUSH_PTR_RST 0x0
union dlb2_chp_hist_list_push_ptr {
	struct {
		u32 push_ptr : 13;
		u32 generation : 1;
		u32 rsvd0 : 18;
	} field;
	u32 val;
};

#define DLB2_CHP_LDB_CQ_DEPTH(x) \
	(0x40900000 + (x) * 0x1000)
#define DLB2_CHP_LDB_CQ_DEPTH_RST 0x0
union dlb2_chp_ldb_cq_depth {
	struct {
		u32 depth : 11;
		u32 rsvd0 : 21;
	} field;
	u32 val;
};

#define DLB2_CHP_LDB_CQ_INT_DEPTH_THRSH(x) \
	(0x40980000 + (x) * 0x1000)
#define DLB2_CHP_LDB_CQ_INT_DEPTH_THRSH_RST 0x0
union dlb2_chp_ldb_cq_int_depth_thrsh {
	struct {
		u32 depth_threshold : 11;
		u32 rsvd0 : 21;
	} field;
	u32 val;
};

#define DLB2_CHP_LDB_CQ_INT_ENB(x) \
	(0x40a00000 + (x) * 0x1000)
#define DLB2_CHP_LDB_CQ_INT_ENB_RST 0x0
union dlb2_chp_ldb_cq_int_enb {
	struct {
		u32 en_tim : 1;
		u32 en_depth : 1;
		u32 rsvd0 : 30;
	} field;
	u32 val;
};

#define DLB2_CHP_LDB_CQ_TMR_THRSH(x) \
	(0x40b00000 + (x) * 0x1000)
#define DLB2_CHP_LDB_CQ_TMR_THRSH_RST 0x1
union dlb2_chp_ldb_cq_tmr_thrsh {
	struct {
		u32 thrsh_0 : 1;
		u32 thrsh_13_1 : 13;
		u32 rsvd0 : 18;
	} field;
	u32 val;
};

#define DLB2_CHP_LDB_CQ_TKN_DEPTH_SEL(x) \
	(0x40b80000 + (x) * 0x1000)
#define DLB2_CHP_LDB_CQ_TKN_DEPTH_SEL_RST 0x0
union dlb2_chp_ldb_cq_tkn_depth_sel {
	struct {
		u32 token_depth_select : 4;
		u32 rsvd0 : 28;
	} field;
	u32 val;
};

#define DLB2_CHP_LDB_CQ_WD_ENB(x) \
	(0x40c00000 + (x) * 0x1000)
#define DLB2_CHP_LDB_CQ_WD_ENB_RST 0x0
union dlb2_chp_ldb_cq_wd_enb {
	struct {
		u32 wd_enable : 1;
		u32 rsvd0 : 31;
	} field;
	u32 val;
};

#define DLB2_CHP_LDB_CQ_WPTR(x) \
	(0x40c80000 + (x) * 0x1000)
#define DLB2_CHP_LDB_CQ_WPTR_RST 0x0
union dlb2_chp_ldb_cq_wptr {
	struct {
		u32 write_pointer : 11;
		u32 rsvd0 : 21;
	} field;
	u32 val;
};

#define DLB2_CHP_LDB_CQ2VAS(x) \
	(0x40d00000 + (x) * 0x1000)
#define DLB2_CHP_LDB_CQ2VAS_RST 0x0
union dlb2_chp_ldb_cq2vas {
	struct {
		u32 cq2vas : 5;
		u32 rsvd0 : 27;
	} field;
	u32 val;
};

#define DLB2_CHP_CFG_CHP_CSR_CTRL 0x44000008
#define DLB2_CHP_CFG_CHP_CSR_CTRL_RST 0x180002
union dlb2_chp_cfg_chp_csr_ctrl {
	struct {
		u32 int_cor_alarm_dis : 1;
		u32 int_cor_synd_dis : 1;
		u32 int_uncr_alarm_dis : 1;
		u32 int_unc_synd_dis : 1;
		u32 int_inf0_alarm_dis : 1;
		u32 int_inf0_synd_dis : 1;
		u32 int_inf1_alarm_dis : 1;
		u32 int_inf1_synd_dis : 1;
		u32 int_inf2_alarm_dis : 1;
		u32 int_inf2_synd_dis : 1;
		u32 int_inf3_alarm_dis : 1;
		u32 int_inf3_synd_dis : 1;
		u32 int_inf4_alarm_dis : 1;
		u32 int_inf4_synd_dis : 1;
		u32 int_inf5_alarm_dis : 1;
		u32 int_inf5_synd_dis : 1;
		u32 dlb_cor_alarm_enable : 1;
		u32 cfg_64bytes_qe_ldb_cq_mode : 1;
		u32 cfg_64bytes_qe_dir_cq_mode : 1;
		u32 pad_write_ldb : 1;
		u32 pad_write_dir : 1;
		u32 pad_first_write_ldb : 1;
		u32 pad_first_write_dir : 1;
		u32 rsvz0 : 9;
	} field;
	u32 val;
};

#define DLB2_CHP_DIR_CQ_INTR_ARMED0 0x4400005c
#define DLB2_CHP_DIR_CQ_INTR_ARMED0_RST 0x0
union dlb2_chp_dir_cq_intr_armed0 {
	struct {
		u32 armed : 32;
	} field;
	u32 val;
};

#define DLB2_CHP_DIR_CQ_INTR_ARMED1 0x44000060
#define DLB2_CHP_DIR_CQ_INTR_ARMED1_RST 0x0
union dlb2_chp_dir_cq_intr_armed1 {
	struct {
		u32 armed : 32;
	} field;
	u32 val;
};

#define DLB2_CHP_CFG_DIR_CQ_TIMER_CTL 0x44000084
#define DLB2_CHP_CFG_DIR_CQ_TIMER_CTL_RST 0x0
union dlb2_chp_cfg_dir_cq_timer_ctl {
	struct {
		u32 sample_interval : 8;
		u32 enb : 1;
		u32 rsvz0 : 23;
	} field;
	u32 val;
};

#define DLB2_CHP_CFG_DIR_WDTO_0 0x44000088
#define DLB2_CHP_CFG_DIR_WDTO_0_RST 0x0
union dlb2_chp_cfg_dir_wdto_0 {
	struct {
		u32 wdto : 32;
	} field;
	u32 val;
};

#define DLB2_CHP_CFG_DIR_WDTO_1 0x4400008c
#define DLB2_CHP_CFG_DIR_WDTO_1_RST 0x0
union dlb2_chp_cfg_dir_wdto_1 {
	struct {
		u32 wdto : 32;
	} field;
	u32 val;
};

#define DLB2_CHP_CFG_DIR_WD_DISABLE0 0x44000098
#define DLB2_CHP_CFG_DIR_WD_DISABLE0_RST 0xffffffff
union dlb2_chp_cfg_dir_wd_disable0 {
	struct {
		u32 wd_disable : 32;
	} field;
	u32 val;
};

#define DLB2_CHP_CFG_DIR_WD_DISABLE1 0x4400009c
#define DLB2_CHP_CFG_DIR_WD_DISABLE1_RST 0xffffffff
union dlb2_chp_cfg_dir_wd_disable1 {
	struct {
		u32 wd_disable : 32;
	} field;
	u32 val;
};

#define DLB2_CHP_CFG_DIR_WD_ENB_INTERVAL 0x440000a0
#define DLB2_CHP_CFG_DIR_WD_ENB_INTERVAL_RST 0x0
union dlb2_chp_cfg_dir_wd_enb_interval {
	struct {
		u32 sample_interval : 28;
		u32 enb : 1;
		u32 rsvz0 : 3;
	} field;
	u32 val;
};

#define DLB2_CHP_CFG_DIR_WD_THRESHOLD 0x440000ac
#define DLB2_CHP_CFG_DIR_WD_THRESHOLD_RST 0x0
union dlb2_chp_cfg_dir_wd_threshold {
	struct {
		u32 wd_threshold : 8;
		u32 rsvz0 : 24;
	} field;
	u32 val;
};

#define DLB2_CHP_LDB_CQ_INTR_ARMED0 0x440000b0
#define DLB2_CHP_LDB_CQ_INTR_ARMED0_RST 0x0
union dlb2_chp_ldb_cq_intr_armed0 {
	struct {
		u32 armed : 32;
	} field;
	u32 val;
};

#define DLB2_CHP_LDB_CQ_INTR_ARMED1 0x440000b4
#define DLB2_CHP_LDB_CQ_INTR_ARMED1_RST 0x0
union dlb2_chp_ldb_cq_intr_armed1 {
	struct {
		u32 armed : 32;
	} field;
	u32 val;
};

#define DLB2_CHP_CFG_LDB_CQ_TIMER_CTL 0x440000d8
#define DLB2_CHP_CFG_LDB_CQ_TIMER_CTL_RST 0x0
union dlb2_chp_cfg_ldb_cq_timer_ctl {
	struct {
		u32 sample_interval : 8;
		u32 enb : 1;
		u32 rsvz0 : 23;
	} field;
	u32 val;
};

#define DLB2_CHP_CFG_LDB_WDTO_0 0x440000dc
#define DLB2_CHP_CFG_LDB_WDTO_0_RST 0x0
union dlb2_chp_cfg_ldb_wdto_0 {
	struct {
		u32 wdto : 32;
	} field;
	u32 val;
};

#define DLB2_CHP_CFG_LDB_WDTO_1 0x440000e0
#define DLB2_CHP_CFG_LDB_WDTO_1_RST 0x0
union dlb2_chp_cfg_ldb_wdto_1 {
	struct {
		u32 wdto : 32;
	} field;
	u32 val;
};

#define DLB2_CHP_CFG_LDB_WD_DISABLE0 0x440000ec
#define DLB2_CHP_CFG_LDB_WD_DISABLE0_RST 0xffffffff
union dlb2_chp_cfg_ldb_wd_disable0 {
	struct {
		u32 wd_disable : 32;
	} field;
	u32 val;
};

#define DLB2_CHP_CFG_LDB_WD_DISABLE1 0x440000f0
#define DLB2_CHP_CFG_LDB_WD_DISABLE1_RST 0xffffffff
union dlb2_chp_cfg_ldb_wd_disable1 {
	struct {
		u32 wd_disable : 32;
	} field;
	u32 val;
};

#define DLB2_CHP_CFG_LDB_WD_ENB_INTERVAL 0x440000f4
#define DLB2_CHP_CFG_LDB_WD_ENB_INTERVAL_RST 0x0
union dlb2_chp_cfg_ldb_wd_enb_interval {
	struct {
		u32 sample_interval : 28;
		u32 enb : 1;
		u32 rsvz0 : 3;
	} field;
	u32 val;
};

#define DLB2_CHP_CFG_LDB_WD_THRESHOLD 0x44000100
#define DLB2_CHP_CFG_LDB_WD_THRESHOLD_RST 0x0
union dlb2_chp_cfg_ldb_wd_threshold {
	struct {
		u32 wd_threshold : 8;
		u32 rsvz0 : 24;
	} field;
	u32 val;
};

#define DLB2_CHP_CTRL_DIAG_02 0x4c000028
#define DLB2_CHP_CTRL_DIAG_02_RST 0x1555
union dlb2_chp_ctrl_diag_02 {
	struct {
		u32 egress_credit_status_empty : 1;
		u32 egress_credit_status_afull : 1;
		u32 chp_outbound_hcw_pipe_credit_status_empty : 1;
		u32 chp_outbound_hcw_pipe_credit_status_afull : 1;
		u32 chp_lsp_ap_cmp_pipe_credit_status_empty : 1;
		u32 chp_lsp_ap_cmp_pipe_credit_status_afull : 1;
		u32 chp_lsp_tok_pipe_credit_status_empty : 1;
		u32 chp_lsp_tok_pipe_credit_status_afull : 1;
		u32 chp_rop_pipe_credit_status_empty : 1;
		u32 chp_rop_pipe_credit_status_afull : 1;
		u32 qed_to_cq_pipe_credit_status_empty : 1;
		u32 qed_to_cq_pipe_credit_status_afull : 1;
		u32 egress_lsp_token_credit_status_empty : 1;
		u32 egress_lsp_token_credit_status_afull : 1;
		u32 rsvd0 : 18;
	} field;
	u32 val;
};

#define DLB2_DP_CFG_ARB_WEIGHTS_TQPRI_DIR_0 0x54000000
#define DLB2_DP_CFG_ARB_WEIGHTS_TQPRI_DIR_0_RST 0xfefcfaf8
union dlb2_dp_cfg_arb_weights_tqpri_dir_0 {
	struct {
		u32 pri0 : 8;
		u32 pri1 : 8;
		u32 pri2 : 8;
		u32 pri3 : 8;
	} field;
	u32 val;
};

#define DLB2_DP_CFG_ARB_WEIGHTS_TQPRI_DIR_1 0x54000004
#define DLB2_DP_CFG_ARB_WEIGHTS_TQPRI_DIR_1_RST 0x0
union dlb2_dp_cfg_arb_weights_tqpri_dir_1 {
	struct {
		u32 rsvz0 : 32;
	} field;
	u32 val;
};

#define DLB2_DP_CFG_ARB_WEIGHTS_TQPRI_REPLAY_0 0x54000008
#define DLB2_DP_CFG_ARB_WEIGHTS_TQPRI_REPLAY_0_RST 0xfefcfaf8
union dlb2_dp_cfg_arb_weights_tqpri_replay_0 {
	struct {
		u32 pri0 : 8;
		u32 pri1 : 8;
		u32 pri2 : 8;
		u32 pri3 : 8;
	} field;
	u32 val;
};

#define DLB2_DP_CFG_ARB_WEIGHTS_TQPRI_REPLAY_1 0x5400000c
#define DLB2_DP_CFG_ARB_WEIGHTS_TQPRI_REPLAY_1_RST 0x0
union dlb2_dp_cfg_arb_weights_tqpri_replay_1 {
	struct {
		u32 rsvz0 : 32;
	} field;
	u32 val;
};

#define DLB2_DP_DIR_CSR_CTRL 0x54000010
#define DLB2_DP_DIR_CSR_CTRL_RST 0x0
union dlb2_dp_dir_csr_ctrl {
	struct {
		u32 int_cor_alarm_dis : 1;
		u32 int_cor_synd_dis : 1;
		u32 int_uncr_alarm_dis : 1;
		u32 int_unc_synd_dis : 1;
		u32 int_inf0_alarm_dis : 1;
		u32 int_inf0_synd_dis : 1;
		u32 int_inf1_alarm_dis : 1;
		u32 int_inf1_synd_dis : 1;
		u32 int_inf2_alarm_dis : 1;
		u32 int_inf2_synd_dis : 1;
		u32 int_inf3_alarm_dis : 1;
		u32 int_inf3_synd_dis : 1;
		u32 int_inf4_alarm_dis : 1;
		u32 int_inf4_synd_dis : 1;
		u32 int_inf5_alarm_dis : 1;
		u32 int_inf5_synd_dis : 1;
		u32 rsvz0 : 16;
	} field;
	u32 val;
};

#define DLB2_NALB_PIPE_CFG_ARB_WEIGHTS_TQPRI_ATQ_0 0x84000000
#define DLB2_NALB_PIPE_CFG_ARB_WEIGHTS_TQPRI_ATQ_0_RST 0xfefcfaf8
union dlb2_nalb_pipe_cfg_arb_weights_tqpri_atq_0 {
	struct {
		u32 pri0 : 8;
		u32 pri1 : 8;
		u32 pri2 : 8;
		u32 pri3 : 8;
	} field;
	u32 val;
};

#define DLB2_NALB_PIPE_CFG_ARB_WEIGHTS_TQPRI_ATQ_1 0x84000004
#define DLB2_NALB_PIPE_CFG_ARB_WEIGHTS_TQPRI_ATQ_1_RST 0x0
union dlb2_nalb_pipe_cfg_arb_weights_tqpri_atq_1 {
	struct {
		u32 rsvz0 : 32;
	} field;
	u32 val;
};

#define DLB2_NALB_PIPE_CFG_ARB_WEIGHTS_TQPRI_NALB_0 0x84000008
#define DLB2_NALB_PIPE_CFG_ARB_WEIGHTS_TQPRI_NALB_0_RST 0xfefcfaf8
union dlb2_nalb_pipe_cfg_arb_weights_tqpri_nalb_0 {
	struct {
		u32 pri0 : 8;
		u32 pri1 : 8;
		u32 pri2 : 8;
		u32 pri3 : 8;
	} field;
	u32 val;
};

#define DLB2_NALB_PIPE_CFG_ARB_WEIGHTS_TQPRI_NALB_1 0x8400000c
#define DLB2_NALB_PIPE_CFG_ARB_WEIGHTS_TQPRI_NALB_1_RST 0x0
union dlb2_nalb_pipe_cfg_arb_weights_tqpri_nalb_1 {
	struct {
		u32 rsvz0 : 32;
	} field;
	u32 val;
};

#define DLB2_NALB_PIPE_CFG_ARB_WEIGHTS_TQPRI_REPLAY_0 0x84000010
#define DLB2_NALB_PIPE_CFG_ARB_WEIGHTS_TQPRI_REPLAY_0_RST 0xfefcfaf8
union dlb2_nalb_pipe_cfg_arb_weights_tqpri_replay_0 {
	struct {
		u32 pri0 : 8;
		u32 pri1 : 8;
		u32 pri2 : 8;
		u32 pri3 : 8;
	} field;
	u32 val;
};

#define DLB2_NALB_PIPE_CFG_ARB_WEIGHTS_TQPRI_REPLAY_1 0x84000014
#define DLB2_NALB_PIPE_CFG_ARB_WEIGHTS_TQPRI_REPLAY_1_RST 0x0
union dlb2_nalb_pipe_cfg_arb_weights_tqpri_replay_1 {
	struct {
		u32 rsvz0 : 32;
	} field;
	u32 val;
};

#define DLB2_RO_PIPE_GRP_0_SLT_SHFT(x) \
	(0x96000000 + (x) * 0x4)
#define DLB2_RO_PIPE_GRP_0_SLT_SHFT_RST 0x0
union dlb2_ro_pipe_grp_0_slt_shft {
	struct {
		u32 change : 10;
		u32 rsvd0 : 22;
	} field;
	u32 val;
};

#define DLB2_RO_PIPE_GRP_1_SLT_SHFT(x) \
	(0x96010000 + (x) * 0x4)
#define DLB2_RO_PIPE_GRP_1_SLT_SHFT_RST 0x0
union dlb2_ro_pipe_grp_1_slt_shft {
	struct {
		u32 change : 10;
		u32 rsvd0 : 22;
	} field;
	u32 val;
};

#define DLB2_RO_PIPE_GRP_SN_MODE 0x94000000
#define DLB2_RO_PIPE_GRP_SN_MODE_RST 0x0
union dlb2_ro_pipe_grp_sn_mode {
	struct {
		u32 sn_mode_0 : 3;
		u32 rszv0 : 5;
		u32 sn_mode_1 : 3;
		u32 rszv1 : 21;
	} field;
	u32 val;
};

#define DLB2_RO_PIPE_CFG_CTRL_GENERAL_0 0x9c000000
#define DLB2_RO_PIPE_CFG_CTRL_GENERAL_0_RST 0x0
union dlb2_ro_pipe_cfg_ctrl_general_0 {
	struct {
		u32 unit_single_step_mode : 1;
		u32 rr_en : 1;
		u32 rszv0 : 30;
	} field;
	u32 val;
};

#define DLB2_LSP_CQ2PRIOV(x) \
	(0xa0000000 + (x) * 0x1000)
#define DLB2_LSP_CQ2PRIOV_RST 0x0
union dlb2_lsp_cq2priov {
	struct {
		u32 prio : 24;
		u32 v : 8;
	} field;
	u32 val;
};

#define DLB2_LSP_CQ2QID0(x) \
	(0xa0080000 + (x) * 0x1000)
#define DLB2_LSP_CQ2QID0_RST 0x0
union dlb2_lsp_cq2qid0 {
	struct {
		u32 qid_p0 : 7;
		u32 rsvd3 : 1;
		u32 qid_p1 : 7;
		u32 rsvd2 : 1;
		u32 qid_p2 : 7;
		u32 rsvd1 : 1;
		u32 qid_p3 : 7;
		u32 rsvd0 : 1;
	} field;
	u32 val;
};

#define DLB2_LSP_CQ2QID1(x) \
	(0xa0100000 + (x) * 0x1000)
#define DLB2_LSP_CQ2QID1_RST 0x0
union dlb2_lsp_cq2qid1 {
	struct {
		u32 qid_p4 : 7;
		u32 rsvd3 : 1;
		u32 qid_p5 : 7;
		u32 rsvd2 : 1;
		u32 qid_p6 : 7;
		u32 rsvd1 : 1;
		u32 qid_p7 : 7;
		u32 rsvd0 : 1;
	} field;
	u32 val;
};

#define DLB2_LSP_CQ_DIR_DSBL(x) \
	(0xa0180000 + (x) * 0x1000)
#define DLB2_LSP_CQ_DIR_DSBL_RST 0x1
union dlb2_lsp_cq_dir_dsbl {
	struct {
		u32 disabled : 1;
		u32 rsvd0 : 31;
	} field;
	u32 val;
};

#define DLB2_LSP_CQ_DIR_TKN_CNT(x) \
	(0xa0200000 + (x) * 0x1000)
#define DLB2_LSP_CQ_DIR_TKN_CNT_RST 0x0
union dlb2_lsp_cq_dir_tkn_cnt {
	struct {
		u32 count : 13;
		u32 rsvd0 : 19;
	} field;
	u32 val;
};

#define DLB2_LSP_CQ_DIR_TKN_DEPTH_SEL_DSI(x) \
	(0xa0280000 + (x) * 0x1000)
#define DLB2_LSP_CQ_DIR_TKN_DEPTH_SEL_DSI_RST 0x0
union dlb2_lsp_cq_dir_tkn_depth_sel_dsi {
	struct {
		u32 token_depth_select : 4;
		u32 disable_wb_opt : 1;
		u32 ignore_depth : 1;
		u32 rsvd0 : 26;
	} field;
	u32 val;
};

#define DLB2_LSP_CQ_DIR_TOT_SCH_CNTL(x) \
	(0xa0300000 + (x) * 0x1000)
#define DLB2_LSP_CQ_DIR_TOT_SCH_CNTL_RST 0x0
union dlb2_lsp_cq_dir_tot_sch_cntl {
	struct {
		u32 count : 32;
	} field;
	u32 val;
};

#define DLB2_LSP_CQ_DIR_TOT_SCH_CNTH(x) \
	(0xa0380000 + (x) * 0x1000)
#define DLB2_LSP_CQ_DIR_TOT_SCH_CNTH_RST 0x0
union dlb2_lsp_cq_dir_tot_sch_cnth {
	struct {
		u32 count : 32;
	} field;
	u32 val;
};

#define DLB2_LSP_CQ_LDB_DSBL(x) \
	(0xa0400000 + (x) * 0x1000)
#define DLB2_LSP_CQ_LDB_DSBL_RST 0x1
union dlb2_lsp_cq_ldb_dsbl {
	struct {
		u32 disabled : 1;
		u32 rsvd0 : 31;
	} field;
	u32 val;
};

#define DLB2_LSP_CQ_LDB_INFL_CNT(x) \
	(0xa0480000 + (x) * 0x1000)
#define DLB2_LSP_CQ_LDB_INFL_CNT_RST 0x0
union dlb2_lsp_cq_ldb_infl_cnt {
	struct {
		u32 count : 12;
		u32 rsvd0 : 20;
	} field;
	u32 val;
};

#define DLB2_LSP_CQ_LDB_INFL_LIM(x) \
	(0xa0500000 + (x) * 0x1000)
#define DLB2_LSP_CQ_LDB_INFL_LIM_RST 0x0
union dlb2_lsp_cq_ldb_infl_lim {
	struct {
		u32 limit : 12;
		u32 rsvd0 : 20;
	} field;
	u32 val;
};

#define DLB2_LSP_CQ_LDB_TKN_CNT(x) \
	(0xa0580000 + (x) * 0x1000)
#define DLB2_LSP_CQ_LDB_TKN_CNT_RST 0x0
union dlb2_lsp_cq_ldb_tkn_cnt {
	struct {
		u32 token_count : 11;
		u32 rsvd0 : 21;
	} field;
	u32 val;
};

#define DLB2_LSP_CQ_LDB_TKN_DEPTH_SEL(x) \
	(0xa0600000 + (x) * 0x1000)
#define DLB2_LSP_CQ_LDB_TKN_DEPTH_SEL_RST 0x0
union dlb2_lsp_cq_ldb_tkn_depth_sel {
	struct {
		u32 token_depth_select : 4;
		u32 ignore_depth : 1;
		u32 rsvd0 : 27;
	} field;
	u32 val;
};

#define DLB2_LSP_CQ_LDB_TOT_SCH_CNTL(x) \
	(0xa0680000 + (x) * 0x1000)
#define DLB2_LSP_CQ_LDB_TOT_SCH_CNTL_RST 0x0
union dlb2_lsp_cq_ldb_tot_sch_cntl {
	struct {
		u32 count : 32;
	} field;
	u32 val;
};

#define DLB2_LSP_CQ_LDB_TOT_SCH_CNTH(x) \
	(0xa0700000 + (x) * 0x1000)
#define DLB2_LSP_CQ_LDB_TOT_SCH_CNTH_RST 0x0
union dlb2_lsp_cq_ldb_tot_sch_cnth {
	struct {
		u32 count : 32;
	} field;
	u32 val;
};

#define DLB2_LSP_QID_DIR_MAX_DEPTH(x) \
	(0xa0780000 + (x) * 0x1000)
#define DLB2_LSP_QID_DIR_MAX_DEPTH_RST 0x0
union dlb2_lsp_qid_dir_max_depth {
	struct {
		u32 depth : 13;
		u32 rsvd0 : 19;
	} field;
	u32 val;
};

#define DLB2_LSP_QID_DIR_TOT_ENQ_CNTL(x) \
	(0xa0800000 + (x) * 0x1000)
#define DLB2_LSP_QID_DIR_TOT_ENQ_CNTL_RST 0x0
union dlb2_lsp_qid_dir_tot_enq_cntl {
	struct {
		u32 count : 32;
	} field;
	u32 val;
};

#define DLB2_LSP_QID_DIR_TOT_ENQ_CNTH(x) \
	(0xa0880000 + (x) * 0x1000)
#define DLB2_LSP_QID_DIR_TOT_ENQ_CNTH_RST 0x0
union dlb2_lsp_qid_dir_tot_enq_cnth {
	struct {
		u32 count : 32;
	} field;
	u32 val;
};

#define DLB2_LSP_QID_DIR_ENQUEUE_CNT(x) \
	(0xa0900000 + (x) * 0x1000)
#define DLB2_LSP_QID_DIR_ENQUEUE_CNT_RST 0x0
union dlb2_lsp_qid_dir_enqueue_cnt {
	struct {
		u32 count : 13;
		u32 rsvd0 : 19;
	} field;
	u32 val;
};

#define DLB2_LSP_QID_DIR_DEPTH_THRSH(x) \
	(0xa0980000 + (x) * 0x1000)
#define DLB2_LSP_QID_DIR_DEPTH_THRSH_RST 0x0
union dlb2_lsp_qid_dir_depth_thrsh {
	struct {
		u32 thresh : 13;
		u32 rsvd0 : 19;
	} field;
	u32 val;
};

#define DLB2_LSP_QID_AQED_ACTIVE_CNT(x) \
	(0xa0a00000 + (x) * 0x1000)
#define DLB2_LSP_QID_AQED_ACTIVE_CNT_RST 0x0
union dlb2_lsp_qid_aqed_active_cnt {
	struct {
		u32 count : 12;
		u32 rsvd0 : 20;
	} field;
	u32 val;
};

#define DLB2_LSP_QID_AQED_ACTIVE_LIM(x) \
	(0xa0a80000 + (x) * 0x1000)
#define DLB2_LSP_QID_AQED_ACTIVE_LIM_RST 0x0
union dlb2_lsp_qid_aqed_active_lim {
	struct {
		u32 limit : 12;
		u32 rsvd0 : 20;
	} field;
	u32 val;
};

#define DLB2_LSP_QID_ATM_TOT_ENQ_CNTL(x) \
	(0xa0b00000 + (x) * 0x1000)
#define DLB2_LSP_QID_ATM_TOT_ENQ_CNTL_RST 0x0
union dlb2_lsp_qid_atm_tot_enq_cntl {
	struct {
		u32 count : 32;
	} field;
	u32 val;
};

#define DLB2_LSP_QID_ATM_TOT_ENQ_CNTH(x) \
	(0xa0b80000 + (x) * 0x1000)
#define DLB2_LSP_QID_ATM_TOT_ENQ_CNTH_RST 0x0
union dlb2_lsp_qid_atm_tot_enq_cnth {
	struct {
		u32 count : 32;
	} field;
	u32 val;
};

#define DLB2_LSP_QID_ATQ_ENQUEUE_CNT(x) \
	(0xa0c00000 + (x) * 0x1000)
#define DLB2_LSP_QID_ATQ_ENQUEUE_CNT_RST 0x0
union dlb2_lsp_qid_atq_enqueue_cnt {
	struct {
		u32 count : 14;
		u32 rsvd0 : 18;
	} field;
	u32 val;
};

#define DLB2_LSP_QID_LDB_ENQUEUE_CNT(x) \
	(0xa0c80000 + (x) * 0x1000)
#define DLB2_LSP_QID_LDB_ENQUEUE_CNT_RST 0x0
union dlb2_lsp_qid_ldb_enqueue_cnt {
	struct {
		u32 count : 14;
		u32 rsvd0 : 18;
	} field;
	u32 val;
};

#define DLB2_LSP_QID_LDB_INFL_CNT(x) \
	(0xa0d00000 + (x) * 0x1000)
#define DLB2_LSP_QID_LDB_INFL_CNT_RST 0x0
union dlb2_lsp_qid_ldb_infl_cnt {
	struct {
		u32 count : 12;
		u32 rsvd0 : 20;
	} field;
	u32 val;
};

#define DLB2_LSP_QID_LDB_INFL_LIM(x) \
	(0xa0d80000 + (x) * 0x1000)
#define DLB2_LSP_QID_LDB_INFL_LIM_RST 0x0
union dlb2_lsp_qid_ldb_infl_lim {
	struct {
		u32 limit : 12;
		u32 rsvd0 : 20;
	} field;
	u32 val;
};

#define DLB2_LSP_QID2CQIDIX_00(x) \
	(0xa0e00000 + (x) * 0x1000)
#define DLB2_LSP_QID2CQIDIX_00_RST 0x0
#define DLB2_LSP_QID2CQIDIX(x, y) \
	(DLB2_LSP_QID2CQIDIX_00(x) + 0x80000 * (y))
#define DLB2_LSP_QID2CQIDIX_NUM 16
union dlb2_lsp_qid2cqidix_00 {
	struct {
		u32 cq_p0 : 8;
		u32 cq_p1 : 8;
		u32 cq_p2 : 8;
		u32 cq_p3 : 8;
	} field;
	u32 val;
};

#define DLB2_LSP_QID2CQIDIX2_00(x) \
	(0xa1600000 + (x) * 0x1000)
#define DLB2_LSP_QID2CQIDIX2_00_RST 0x0
#define DLB2_LSP_QID2CQIDIX2(x, y) \
	(DLB2_LSP_QID2CQIDIX2_00(x) + 0x80000 * (y))
#define DLB2_LSP_QID2CQIDIX2_NUM 16
union dlb2_lsp_qid2cqidix2_00 {
	struct {
		u32 cq_p0 : 8;
		u32 cq_p1 : 8;
		u32 cq_p2 : 8;
		u32 cq_p3 : 8;
	} field;
	u32 val;
};

#define DLB2_LSP_QID_LDB_REPLAY_CNT(x) \
	(0xa1e00000 + (x) * 0x1000)
#define DLB2_LSP_QID_LDB_REPLAY_CNT_RST 0x0
union dlb2_lsp_qid_ldb_replay_cnt {
	struct {
		u32 count : 14;
		u32 rsvd0 : 18;
	} field;
	u32 val;
};

#define DLB2_LSP_QID_NALDB_MAX_DEPTH(x) \
	(0xa1f00000 + (x) * 0x1000)
#define DLB2_LSP_QID_NALDB_MAX_DEPTH_RST 0x0
union dlb2_lsp_qid_naldb_max_depth {
	struct {
		u32 depth : 14;
		u32 rsvd0 : 18;
	} field;
	u32 val;
};

#define DLB2_LSP_QID_NALDB_TOT_ENQ_CNTL(x) \
	(0xa1f80000 + (x) * 0x1000)
#define DLB2_LSP_QID_NALDB_TOT_ENQ_CNTL_RST 0x0
union dlb2_lsp_qid_naldb_tot_enq_cntl {
	struct {
		u32 count : 32;
	} field;
	u32 val;
};

#define DLB2_LSP_QID_NALDB_TOT_ENQ_CNTH(x) \
	(0xa2000000 + (x) * 0x1000)
#define DLB2_LSP_QID_NALDB_TOT_ENQ_CNTH_RST 0x0
union dlb2_lsp_qid_naldb_tot_enq_cnth {
	struct {
		u32 count : 32;
	} field;
	u32 val;
};

#define DLB2_LSP_QID_ATM_DEPTH_THRSH(x) \
	(0xa2080000 + (x) * 0x1000)
#define DLB2_LSP_QID_ATM_DEPTH_THRSH_RST 0x0
union dlb2_lsp_qid_atm_depth_thrsh {
	struct {
		u32 thresh : 14;
		u32 rsvd0 : 18;
	} field;
	u32 val;
};

#define DLB2_LSP_QID_NALDB_DEPTH_THRSH(x) \
	(0xa2100000 + (x) * 0x1000)
#define DLB2_LSP_QID_NALDB_DEPTH_THRSH_RST 0x0
union dlb2_lsp_qid_naldb_depth_thrsh {
	struct {
		u32 thresh : 14;
		u32 rsvd0 : 18;
	} field;
	u32 val;
};

#define DLB2_LSP_QID_ATM_ACTIVE(x) \
	(0xa2180000 + (x) * 0x1000)
#define DLB2_LSP_QID_ATM_ACTIVE_RST 0x0
union dlb2_lsp_qid_atm_active {
	struct {
		u32 count : 14;
		u32 rsvd0 : 18;
	} field;
	u32 val;
};

#define DLB2_LSP_CFG_ARB_WEIGHT_ATM_NALB_QID_0 0xa4000008
#define DLB2_LSP_CFG_ARB_WEIGHT_ATM_NALB_QID_0_RST 0x0
union dlb2_lsp_cfg_arb_weight_atm_nalb_qid_0 {
	struct {
		u32 pri0_weight : 8;
		u32 pri1_weight : 8;
		u32 pri2_weight : 8;
		u32 pri3_weight : 8;
	} field;
	u32 val;
};

#define DLB2_LSP_CFG_ARB_WEIGHT_ATM_NALB_QID_1 0xa400000c
#define DLB2_LSP_CFG_ARB_WEIGHT_ATM_NALB_QID_1_RST 0x0
union dlb2_lsp_cfg_arb_weight_atm_nalb_qid_1 {
	struct {
		u32 rsvz0 : 32;
	} field;
	u32 val;
};

#define DLB2_LSP_CFG_ARB_WEIGHT_LDB_QID_0 0xa4000014
#define DLB2_LSP_CFG_ARB_WEIGHT_LDB_QID_0_RST 0x0
union dlb2_lsp_cfg_arb_weight_ldb_qid_0 {
	struct {
		u32 pri0_weight : 8;
		u32 pri1_weight : 8;
		u32 pri2_weight : 8;
		u32 pri3_weight : 8;
	} field;
	u32 val;
};

#define DLB2_LSP_CFG_ARB_WEIGHT_LDB_QID_1 0xa4000018
#define DLB2_LSP_CFG_ARB_WEIGHT_LDB_QID_1_RST 0x0
union dlb2_lsp_cfg_arb_weight_ldb_qid_1 {
	struct {
		u32 rsvz0 : 32;
	} field;
	u32 val;
};

#define DLB2_LSP_LDB_SCHED_CTRL 0xa400002c
#define DLB2_LSP_LDB_SCHED_CTRL_RST 0x0
union dlb2_lsp_ldb_sched_ctrl {
	struct {
		u32 cq : 8;
		u32 qidix : 3;
		u32 value : 1;
		u32 nalb_haswork_v : 1;
		u32 rlist_haswork_v : 1;
		u32 slist_haswork_v : 1;
		u32 inflight_ok_v : 1;
		u32 aqed_nfull_v : 1;
		u32 rsvz0 : 15;
	} field;
	u32 val;
};

#define DLB2_LSP_DIR_SCH_CNT_L 0xa4000034
#define DLB2_LSP_DIR_SCH_CNT_L_RST 0x0
union dlb2_lsp_dir_sch_cnt_l {
	struct {
		u32 count : 32;
	} field;
	u32 val;
};

#define DLB2_LSP_DIR_SCH_CNT_H 0xa4000038
#define DLB2_LSP_DIR_SCH_CNT_H_RST 0x0
union dlb2_lsp_dir_sch_cnt_h {
	struct {
		u32 count : 32;
	} field;
	u32 val;
};

#define DLB2_LSP_LDB_SCH_CNT_L 0xa400003c
#define DLB2_LSP_LDB_SCH_CNT_L_RST 0x0
union dlb2_lsp_ldb_sch_cnt_l {
	struct {
		u32 count : 32;
	} field;
	u32 val;
};

#define DLB2_LSP_LDB_SCH_CNT_H 0xa4000040
#define DLB2_LSP_LDB_SCH_CNT_H_RST 0x0
union dlb2_lsp_ldb_sch_cnt_h {
	struct {
		u32 count : 32;
	} field;
	u32 val;
};

#define DLB2_LSP_CFG_SHDW_CTRL 0xa4000070
#define DLB2_LSP_CFG_SHDW_CTRL_RST 0x0
union dlb2_lsp_cfg_shdw_ctrl {
	struct {
		u32 transfer : 1;
		u32 rsvd0 : 31;
	} field;
	u32 val;
};

#define DLB2_LSP_CFG_SHDW_RANGE_COS(x) \
	(0xa4000074 + (x) * 4)
#define DLB2_LSP_CFG_SHDW_RANGE_COS_RST 0x40
union dlb2_lsp_cfg_shdw_range_cos {
	struct {
		u32 bw_range : 9;
		u32 rsvz0 : 22;
		u32 no_extra_credit : 1;
	} field;
	u32 val;
};

#define DLB2_LSP_CFG_CTRL_GENERAL_0 0xac000000
#define DLB2_LSP_CFG_CTRL_GENERAL_0_RST 0x0
union dlb2_lsp_cfg_ctrl_general_0 {
	struct {
		u32 disab_atq_empty_arb : 1;
		u32 inc_tok_unit_idle : 1;
		u32 disab_rlist_pri : 1;
		u32 inc_cmp_unit_idle : 1;
		u32 rsvz0 : 2;
		u32 dir_single_op : 1;
		u32 dir_half_bw : 1;
		u32 dir_single_out : 1;
		u32 dir_disab_multi : 1;
		u32 atq_single_op : 1;
		u32 atq_half_bw : 1;
		u32 atq_single_out : 1;
		u32 atq_disab_multi : 1;
		u32 dirrpl_single_op : 1;
		u32 dirrpl_half_bw : 1;
		u32 dirrpl_single_out : 1;
		u32 lbrpl_single_op : 1;
		u32 lbrpl_half_bw : 1;
		u32 lbrpl_single_out : 1;
		u32 ldb_single_op : 1;
		u32 ldb_half_bw : 1;
		u32 ldb_disab_multi : 1;
		u32 atm_single_sch : 1;
		u32 atm_single_cmp : 1;
		u32 ldb_ce_tog_arb : 1;
		u32 rsvz1 : 1;
		u32 smon0_valid_sel : 2;
		u32 smon0_value_sel : 1;
		u32 smon0_compare_sel : 2;
	} field;
	u32 val;
};

#define DLB2_CFG_MSTR_DIAG_RESET_STS 0xb4000000
#define DLB2_CFG_MSTR_DIAG_RESET_STS_RST 0x80000bff
union dlb2_cfg_mstr_diag_reset_sts {
	struct {
		u32 chp_pf_reset_done : 1;
		u32 rop_pf_reset_done : 1;
		u32 lsp_pf_reset_done : 1;
		u32 nalb_pf_reset_done : 1;
		u32 ap_pf_reset_done : 1;
		u32 dp_pf_reset_done : 1;
		u32 qed_pf_reset_done : 1;
		u32 dqed_pf_reset_done : 1;
		u32 aqed_pf_reset_done : 1;
		u32 sys_pf_reset_done : 1;
		u32 pf_reset_active : 1;
		u32 flrsm_state : 7;
		u32 rsvd0 : 13;
		u32 dlb_proc_reset_done : 1;
	} field;
	u32 val;
};

#define DLB2_CFG_MSTR_CFG_DIAGNOSTIC_IDLE_STATUS 0xb4000004
#define DLB2_CFG_MSTR_CFG_DIAGNOSTIC_IDLE_STATUS_RST 0x9d0fffff
union dlb2_cfg_mstr_cfg_diagnostic_idle_status {
	struct {
		u32 chp_pipeidle : 1;
		u32 rop_pipeidle : 1;
		u32 lsp_pipeidle : 1;
		u32 nalb_pipeidle : 1;
		u32 ap_pipeidle : 1;
		u32 dp_pipeidle : 1;
		u32 qed_pipeidle : 1;
		u32 dqed_pipeidle : 1;
		u32 aqed_pipeidle : 1;
		u32 sys_pipeidle : 1;
		u32 chp_unit_idle : 1;
		u32 rop_unit_idle : 1;
		u32 lsp_unit_idle : 1;
		u32 nalb_unit_idle : 1;
		u32 ap_unit_idle : 1;
		u32 dp_unit_idle : 1;
		u32 qed_unit_idle : 1;
		u32 dqed_unit_idle : 1;
		u32 aqed_unit_idle : 1;
		u32 sys_unit_idle : 1;
		u32 rsvd1 : 4;
		u32 mstr_cfg_ring_idle : 1;
		u32 mstr_cfg_mstr_idle : 1;
		u32 mstr_flr_clkreq_b : 1;
		u32 mstr_proc_idle : 1;
		u32 mstr_proc_idle_masked : 1;
		u32 rsvd0 : 2;
		u32 dlb_func_idle : 1;
	} field;
	u32 val;
};

#define DLB2_CFG_MSTR_CFG_PM_STATUS 0xb4000014
#define DLB2_CFG_MSTR_CFG_PM_STATUS_RST 0x100403e
union dlb2_cfg_mstr_cfg_pm_status {
	struct {
		u32 prochot : 1;
		u32 pgcb_dlb_idle : 1;
		u32 pgcb_dlb_pg_rdy_ack_b : 1;
		u32 pmsm_pgcb_req_b : 1;
		u32 pgbc_pmc_pg_req_b : 1;
		u32 pmc_pgcb_pg_ack_b : 1;
		u32 pmc_pgcb_fet_en_b : 1;
		u32 pgcb_fet_en_b : 1;
		u32 rsvz0 : 1;
		u32 rsvz1 : 1;
		u32 fuse_force_on : 1;
		u32 fuse_proc_disable : 1;
		u32 rsvz2 : 1;
		u32 rsvz3 : 1;
		u32 pm_fsm_d0tod3_ok : 1;
		u32 pm_fsm_d3tod0_ok : 1;
		u32 dlb_in_d3 : 1;
		u32 rsvz4 : 7;
		u32 pmsm : 8;
	} field;
	u32 val;
};

#define DLB2_CFG_MSTR_CFG_PM_PMCSR_DISABLE 0xb4000018
#define DLB2_CFG_MSTR_CFG_PM_PMCSR_DISABLE_RST 0x1
union dlb2_cfg_mstr_cfg_pm_pmcsr_disable {
	struct {
		u32 disable : 1;
		u32 rsvz0 : 31;
	} field;
	u32 val;
};

#define DLB2_FUNC_VF_VF2PF_MAILBOX_BYTES 256
#define DLB2_FUNC_VF_VF2PF_MAILBOX(x) \
	(0x1000 + (x) * 0x4)
#define DLB2_FUNC_VF_VF2PF_MAILBOX_RST 0x0
union dlb2_func_vf_vf2pf_mailbox {
	struct {
		u32 msg : 32;
	} field;
	u32 val;
};

#define DLB2_FUNC_VF_VF2PF_MAILBOX_ISR 0x1f00
#define DLB2_FUNC_VF_VF2PF_MAILBOX_ISR_RST 0x0
#define DLB2_FUNC_VF_SIOV_VF2PF_MAILBOX_ISR_TRIGGER 0x8000
union dlb2_func_vf_vf2pf_mailbox_isr {
	struct {
		u32 isr : 1;
		u32 rsvd0 : 31;
	} field;
	u32 val;
};

#define DLB2_FUNC_VF_PF2VF_MAILBOX_BYTES 64
#define DLB2_FUNC_VF_PF2VF_MAILBOX(x) \
	(0x2000 + (x) * 0x4)
#define DLB2_FUNC_VF_PF2VF_MAILBOX_RST 0x0
union dlb2_func_vf_pf2vf_mailbox {
	struct {
		u32 msg : 32;
	} field;
	u32 val;
};

#define DLB2_FUNC_VF_PF2VF_MAILBOX_ISR 0x2f00
#define DLB2_FUNC_VF_PF2VF_MAILBOX_ISR_RST 0x0
union dlb2_func_vf_pf2vf_mailbox_isr {
	struct {
		u32 pf_isr : 1;
		u32 rsvd0 : 31;
	} field;
	u32 val;
};

#define DLB2_FUNC_VF_VF_MSI_ISR_PEND 0x2f10
#define DLB2_FUNC_VF_VF_MSI_ISR_PEND_RST 0x0
union dlb2_func_vf_vf_msi_isr_pend {
	struct {
		u32 isr_pend : 32;
	} field;
	u32 val;
};

#define DLB2_FUNC_VF_VF_RESET_IN_PROGRESS 0x3000
#define DLB2_FUNC_VF_VF_RESET_IN_PROGRESS_RST 0x1
union dlb2_func_vf_vf_reset_in_progress {
	struct {
		u32 reset_in_progress : 1;
		u32 rsvd0 : 31;
	} field;
	u32 val;
};

#define DLB2_FUNC_VF_VF_MSI_ISR 0x4000
#define DLB2_FUNC_VF_VF_MSI_ISR_RST 0x0
union dlb2_func_vf_vf_msi_isr {
	struct {
		u32 vf_msi_isr : 32;
	} field;
	u32 val;
};

#endif /* __DLB2_REGS_H */
