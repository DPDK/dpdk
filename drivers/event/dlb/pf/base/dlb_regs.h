/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2020 Intel Corporation
 */

#ifndef __DLB_REGS_H
#define __DLB_REGS_H

#include "dlb_osdep_types.h"

#define DLB_MSIX_MEM_VECTOR_CTRL(x) \
	(0x100000c + (x) * 0x10)
#define DLB_MSIX_MEM_VECTOR_CTRL_RST 0x1
union dlb_msix_mem_vector_ctrl {
	struct {
		u32 vec_mask : 1;
		u32 rsvd0 : 31;
	} field;
	u32 val;
};

#define DLB_SYS_TOTAL_VAS 0x124
#define DLB_SYS_TOTAL_VAS_RST 0x20
union dlb_sys_total_vas {
	struct {
		u32 total_vas : 32;
	} field;
	u32 val;
};

#define DLB_SYS_ALARM_PF_SYND2 0x508
#define DLB_SYS_ALARM_PF_SYND2_RST 0x0
union dlb_sys_alarm_pf_synd2 {
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

#define DLB_SYS_ALARM_PF_SYND1 0x504
#define DLB_SYS_ALARM_PF_SYND1_RST 0x0
union dlb_sys_alarm_pf_synd1 {
	struct {
		u32 dsi : 16;
		u32 qid : 8;
		u32 qtype : 2;
		u32 qpri : 3;
		u32 msg_type : 3;
	} field;
	u32 val;
};

#define DLB_SYS_ALARM_PF_SYND0 0x500
#define DLB_SYS_ALARM_PF_SYND0_RST 0x0
union dlb_sys_alarm_pf_synd0 {
	struct {
		u32 syndrome : 8;
		u32 rtype : 2;
		u32 rsvd0 : 2;
		u32 from_dmv : 1;
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

#define DLB_SYS_LDB_VASQID_V(x) \
	(0xf60 + (x) * 0x1000)
#define DLB_SYS_LDB_VASQID_V_RST 0x0
union dlb_sys_ldb_vasqid_v {
	struct {
		u32 vasqid_v : 1;
		u32 rsvd0 : 31;
	} field;
	u32 val;
};

#define DLB_SYS_DIR_VASQID_V(x) \
	(0xf68 + (x) * 0x1000)
#define DLB_SYS_DIR_VASQID_V_RST 0x0
union dlb_sys_dir_vasqid_v {
	struct {
		u32 vasqid_v : 1;
		u32 rsvd0 : 31;
	} field;
	u32 val;
};

#define DLB_SYS_WBUF_DIR_FLAGS(x) \
	(0xf70 + (x) * 0x1000)
#define DLB_SYS_WBUF_DIR_FLAGS_RST 0x0
union dlb_sys_wbuf_dir_flags {
	struct {
		u32 wb_v : 4;
		u32 cl : 1;
		u32 busy : 1;
		u32 opt : 1;
		u32 rsvd0 : 25;
	} field;
	u32 val;
};

#define DLB_SYS_WBUF_LDB_FLAGS(x) \
	(0xf78 + (x) * 0x1000)
#define DLB_SYS_WBUF_LDB_FLAGS_RST 0x0
union dlb_sys_wbuf_ldb_flags {
	struct {
		u32 wb_v : 4;
		u32 cl : 1;
		u32 busy : 1;
		u32 rsvd0 : 26;
	} field;
	u32 val;
};

#define DLB_SYS_LDB_QID_V(x) \
	(0x8000034 + (x) * 0x1000)
#define DLB_SYS_LDB_QID_V_RST 0x0
union dlb_sys_ldb_qid_v {
	struct {
		u32 qid_v : 1;
		u32 rsvd0 : 31;
	} field;
	u32 val;
};

#define DLB_SYS_LDB_QID_CFG_V(x) \
	(0x8000030 + (x) * 0x1000)
#define DLB_SYS_LDB_QID_CFG_V_RST 0x0
union dlb_sys_ldb_qid_cfg_v {
	struct {
		u32 sn_cfg_v : 1;
		u32 fid_cfg_v : 1;
		u32 rsvd0 : 30;
	} field;
	u32 val;
};

#define DLB_SYS_DIR_QID_V(x) \
	(0x8000040 + (x) * 0x1000)
#define DLB_SYS_DIR_QID_V_RST 0x0
union dlb_sys_dir_qid_v {
	struct {
		u32 qid_v : 1;
		u32 rsvd0 : 31;
	} field;
	u32 val;
};

#define DLB_SYS_LDB_POOL_ENBLD(x) \
	(0x8000070 + (x) * 0x1000)
#define DLB_SYS_LDB_POOL_ENBLD_RST 0x0
union dlb_sys_ldb_pool_enbld {
	struct {
		u32 pool_enabled : 1;
		u32 rsvd0 : 31;
	} field;
	u32 val;
};

#define DLB_SYS_DIR_POOL_ENBLD(x) \
	(0x8000080 + (x) * 0x1000)
#define DLB_SYS_DIR_POOL_ENBLD_RST 0x0
union dlb_sys_dir_pool_enbld {
	struct {
		u32 pool_enabled : 1;
		u32 rsvd0 : 31;
	} field;
	u32 val;
};

#define DLB_SYS_LDB_PP2VPP(x) \
	(0x8000090 + (x) * 0x1000)
#define DLB_SYS_LDB_PP2VPP_RST 0x0
union dlb_sys_ldb_pp2vpp {
	struct {
		u32 vpp : 6;
		u32 rsvd0 : 26;
	} field;
	u32 val;
};

#define DLB_SYS_DIR_PP2VPP(x) \
	(0x8000094 + (x) * 0x1000)
#define DLB_SYS_DIR_PP2VPP_RST 0x0
union dlb_sys_dir_pp2vpp {
	struct {
		u32 vpp : 7;
		u32 rsvd0 : 25;
	} field;
	u32 val;
};

#define DLB_SYS_LDB_PP_V(x) \
	(0x8000128 + (x) * 0x1000)
#define DLB_SYS_LDB_PP_V_RST 0x0
union dlb_sys_ldb_pp_v {
	struct {
		u32 pp_v : 1;
		u32 rsvd0 : 31;
	} field;
	u32 val;
};

#define DLB_SYS_LDB_CQ_ISR(x) \
	(0x8000124 + (x) * 0x1000)
#define DLB_SYS_LDB_CQ_ISR_RST 0x0
/* CQ Interrupt Modes */
#define DLB_CQ_ISR_MODE_DIS  0
#define DLB_CQ_ISR_MODE_MSI  1
#define DLB_CQ_ISR_MODE_MSIX 2
union dlb_sys_ldb_cq_isr {
	struct {
		u32 vector : 6;
		u32 vf : 4;
		u32 en_code : 2;
		u32 rsvd0 : 20;
	} field;
	u32 val;
};

#define DLB_SYS_LDB_CQ2VF_PF(x) \
	(0x8000120 + (x) * 0x1000)
#define DLB_SYS_LDB_CQ2VF_PF_RST 0x0
union dlb_sys_ldb_cq2vf_pf {
	struct {
		u32 vf : 4;
		u32 is_pf : 1;
		u32 rsvd0 : 27;
	} field;
	u32 val;
};

#define DLB_SYS_LDB_PP2VAS(x) \
	(0x800011c + (x) * 0x1000)
#define DLB_SYS_LDB_PP2VAS_RST 0x0
union dlb_sys_ldb_pp2vas {
	struct {
		u32 vas : 5;
		u32 rsvd0 : 27;
	} field;
	u32 val;
};

#define DLB_SYS_LDB_PP2LDBPOOL(x) \
	(0x8000118 + (x) * 0x1000)
#define DLB_SYS_LDB_PP2LDBPOOL_RST 0x0
union dlb_sys_ldb_pp2ldbpool {
	struct {
		u32 ldbpool : 6;
		u32 rsvd0 : 26;
	} field;
	u32 val;
};

#define DLB_SYS_LDB_PP2DIRPOOL(x) \
	(0x8000114 + (x) * 0x1000)
#define DLB_SYS_LDB_PP2DIRPOOL_RST 0x0
union dlb_sys_ldb_pp2dirpool {
	struct {
		u32 dirpool : 6;
		u32 rsvd0 : 26;
	} field;
	u32 val;
};

#define DLB_SYS_LDB_PP2VF_PF(x) \
	(0x8000110 + (x) * 0x1000)
#define DLB_SYS_LDB_PP2VF_PF_RST 0x0
union dlb_sys_ldb_pp2vf_pf {
	struct {
		u32 vf : 4;
		u32 is_pf : 1;
		u32 rsvd0 : 27;
	} field;
	u32 val;
};

#define DLB_SYS_LDB_PP_ADDR_U(x) \
	(0x800010c + (x) * 0x1000)
#define DLB_SYS_LDB_PP_ADDR_U_RST 0x0
union dlb_sys_ldb_pp_addr_u {
	struct {
		u32 addr_u : 32;
	} field;
	u32 val;
};

#define DLB_SYS_LDB_PP_ADDR_L(x) \
	(0x8000108 + (x) * 0x1000)
#define DLB_SYS_LDB_PP_ADDR_L_RST 0x0
union dlb_sys_ldb_pp_addr_l {
	struct {
		u32 rsvd0 : 7;
		u32 addr_l : 25;
	} field;
	u32 val;
};

#define DLB_SYS_LDB_CQ_ADDR_U(x) \
	(0x8000104 + (x) * 0x1000)
#define DLB_SYS_LDB_CQ_ADDR_U_RST 0x0
union dlb_sys_ldb_cq_addr_u {
	struct {
		u32 addr_u : 32;
	} field;
	u32 val;
};

#define DLB_SYS_LDB_CQ_ADDR_L(x) \
	(0x8000100 + (x) * 0x1000)
#define DLB_SYS_LDB_CQ_ADDR_L_RST 0x0
union dlb_sys_ldb_cq_addr_l {
	struct {
		u32 rsvd0 : 6;
		u32 addr_l : 26;
	} field;
	u32 val;
};

#define DLB_SYS_DIR_PP_V(x) \
	(0x8000228 + (x) * 0x1000)
#define DLB_SYS_DIR_PP_V_RST 0x0
union dlb_sys_dir_pp_v {
	struct {
		u32 pp_v : 1;
		u32 mb_dm : 1;
		u32 rsvd0 : 30;
	} field;
	u32 val;
};

#define DLB_SYS_DIR_CQ_ISR(x) \
	(0x8000224 + (x) * 0x1000)
#define DLB_SYS_DIR_CQ_ISR_RST 0x0
union dlb_sys_dir_cq_isr {
	struct {
		u32 vector : 6;
		u32 vf : 4;
		u32 en_code : 2;
		u32 rsvd0 : 20;
	} field;
	u32 val;
};

#define DLB_SYS_DIR_CQ2VF_PF(x) \
	(0x8000220 + (x) * 0x1000)
#define DLB_SYS_DIR_CQ2VF_PF_RST 0x0
union dlb_sys_dir_cq2vf_pf {
	struct {
		u32 vf : 4;
		u32 is_pf : 1;
		u32 rsvd0 : 27;
	} field;
	u32 val;
};

#define DLB_SYS_DIR_PP2VAS(x) \
	(0x800021c + (x) * 0x1000)
#define DLB_SYS_DIR_PP2VAS_RST 0x0
union dlb_sys_dir_pp2vas {
	struct {
		u32 vas : 5;
		u32 rsvd0 : 27;
	} field;
	u32 val;
};

#define DLB_SYS_DIR_PP2LDBPOOL(x) \
	(0x8000218 + (x) * 0x1000)
#define DLB_SYS_DIR_PP2LDBPOOL_RST 0x0
union dlb_sys_dir_pp2ldbpool {
	struct {
		u32 ldbpool : 6;
		u32 rsvd0 : 26;
	} field;
	u32 val;
};

#define DLB_SYS_DIR_PP2DIRPOOL(x) \
	(0x8000214 + (x) * 0x1000)
#define DLB_SYS_DIR_PP2DIRPOOL_RST 0x0
union dlb_sys_dir_pp2dirpool {
	struct {
		u32 dirpool : 6;
		u32 rsvd0 : 26;
	} field;
	u32 val;
};

#define DLB_SYS_DIR_PP2VF_PF(x) \
	(0x8000210 + (x) * 0x1000)
#define DLB_SYS_DIR_PP2VF_PF_RST 0x0
union dlb_sys_dir_pp2vf_pf {
	struct {
		u32 vf : 4;
		u32 is_pf : 1;
		u32 is_hw_dsi : 1;
		u32 rsvd0 : 26;
	} field;
	u32 val;
};

#define DLB_SYS_DIR_PP_ADDR_U(x) \
	(0x800020c + (x) * 0x1000)
#define DLB_SYS_DIR_PP_ADDR_U_RST 0x0
union dlb_sys_dir_pp_addr_u {
	struct {
		u32 addr_u : 32;
	} field;
	u32 val;
};

#define DLB_SYS_DIR_PP_ADDR_L(x) \
	(0x8000208 + (x) * 0x1000)
#define DLB_SYS_DIR_PP_ADDR_L_RST 0x0
union dlb_sys_dir_pp_addr_l {
	struct {
		u32 rsvd0 : 7;
		u32 addr_l : 25;
	} field;
	u32 val;
};

#define DLB_SYS_DIR_CQ_ADDR_U(x) \
	(0x8000204 + (x) * 0x1000)
#define DLB_SYS_DIR_CQ_ADDR_U_RST 0x0
union dlb_sys_dir_cq_addr_u {
	struct {
		u32 addr_u : 32;
	} field;
	u32 val;
};

#define DLB_SYS_DIR_CQ_ADDR_L(x) \
	(0x8000200 + (x) * 0x1000)
#define DLB_SYS_DIR_CQ_ADDR_L_RST 0x0
union dlb_sys_dir_cq_addr_l {
	struct {
		u32 rsvd0 : 6;
		u32 addr_l : 26;
	} field;
	u32 val;
};

#define DLB_SYS_INGRESS_ALARM_ENBL 0x300
#define DLB_SYS_INGRESS_ALARM_ENBL_RST 0x0
union dlb_sys_ingress_alarm_enbl {
	struct {
		u32 illegal_hcw : 1;
		u32 illegal_pp : 1;
		u32 disabled_pp : 1;
		u32 illegal_qid : 1;
		u32 disabled_qid : 1;
		u32 illegal_ldb_qid_cfg : 1;
		u32 illegal_cqid : 1;
		u32 rsvd0 : 25;
	} field;
	u32 val;
};

#define DLB_SYS_CQ_MODE 0x30c
#define DLB_SYS_CQ_MODE_RST 0x0
union dlb_sys_cq_mode {
	struct {
		u32 ldb_cq64 : 1;
		u32 dir_cq64 : 1;
		u32 rsvd0 : 30;
	} field;
	u32 val;
};

#define DLB_SYS_MSIX_ACK 0x400
#define DLB_SYS_MSIX_ACK_RST 0x0
union dlb_sys_msix_ack {
	struct {
		u32 msix_0_ack : 1;
		u32 msix_1_ack : 1;
		u32 msix_2_ack : 1;
		u32 msix_3_ack : 1;
		u32 msix_4_ack : 1;
		u32 msix_5_ack : 1;
		u32 msix_6_ack : 1;
		u32 msix_7_ack : 1;
		u32 msix_8_ack : 1;
		u32 rsvd0 : 23;
	} field;
	u32 val;
};

#define DLB_SYS_MSIX_PASSTHRU 0x404
#define DLB_SYS_MSIX_PASSTHRU_RST 0x0
union dlb_sys_msix_passthru {
	struct {
		u32 msix_0_passthru : 1;
		u32 msix_1_passthru : 1;
		u32 msix_2_passthru : 1;
		u32 msix_3_passthru : 1;
		u32 msix_4_passthru : 1;
		u32 msix_5_passthru : 1;
		u32 msix_6_passthru : 1;
		u32 msix_7_passthru : 1;
		u32 msix_8_passthru : 1;
		u32 rsvd0 : 23;
	} field;
	u32 val;
};

#define DLB_SYS_MSIX_MODE 0x408
#define DLB_SYS_MSIX_MODE_RST 0x0
/* MSI-X Modes */
#define DLB_MSIX_MODE_PACKED     0
#define DLB_MSIX_MODE_COMPRESSED 1
union dlb_sys_msix_mode {
	struct {
		u32 mode : 1;
		u32 rsvd0 : 31;
	} field;
	u32 val;
};

#define DLB_SYS_DIR_CQ_31_0_OCC_INT_STS 0x440
#define DLB_SYS_DIR_CQ_31_0_OCC_INT_STS_RST 0x0
union dlb_sys_dir_cq_31_0_occ_int_sts {
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

#define DLB_SYS_DIR_CQ_63_32_OCC_INT_STS 0x444
#define DLB_SYS_DIR_CQ_63_32_OCC_INT_STS_RST 0x0
union dlb_sys_dir_cq_63_32_occ_int_sts {
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

#define DLB_SYS_DIR_CQ_95_64_OCC_INT_STS 0x448
#define DLB_SYS_DIR_CQ_95_64_OCC_INT_STS_RST 0x0
union dlb_sys_dir_cq_95_64_occ_int_sts {
	struct {
		u32 cq_64_occ_int : 1;
		u32 cq_65_occ_int : 1;
		u32 cq_66_occ_int : 1;
		u32 cq_67_occ_int : 1;
		u32 cq_68_occ_int : 1;
		u32 cq_69_occ_int : 1;
		u32 cq_70_occ_int : 1;
		u32 cq_71_occ_int : 1;
		u32 cq_72_occ_int : 1;
		u32 cq_73_occ_int : 1;
		u32 cq_74_occ_int : 1;
		u32 cq_75_occ_int : 1;
		u32 cq_76_occ_int : 1;
		u32 cq_77_occ_int : 1;
		u32 cq_78_occ_int : 1;
		u32 cq_79_occ_int : 1;
		u32 cq_80_occ_int : 1;
		u32 cq_81_occ_int : 1;
		u32 cq_82_occ_int : 1;
		u32 cq_83_occ_int : 1;
		u32 cq_84_occ_int : 1;
		u32 cq_85_occ_int : 1;
		u32 cq_86_occ_int : 1;
		u32 cq_87_occ_int : 1;
		u32 cq_88_occ_int : 1;
		u32 cq_89_occ_int : 1;
		u32 cq_90_occ_int : 1;
		u32 cq_91_occ_int : 1;
		u32 cq_92_occ_int : 1;
		u32 cq_93_occ_int : 1;
		u32 cq_94_occ_int : 1;
		u32 cq_95_occ_int : 1;
	} field;
	u32 val;
};

#define DLB_SYS_DIR_CQ_127_96_OCC_INT_STS 0x44c
#define DLB_SYS_DIR_CQ_127_96_OCC_INT_STS_RST 0x0
union dlb_sys_dir_cq_127_96_occ_int_sts {
	struct {
		u32 cq_96_occ_int : 1;
		u32 cq_97_occ_int : 1;
		u32 cq_98_occ_int : 1;
		u32 cq_99_occ_int : 1;
		u32 cq_100_occ_int : 1;
		u32 cq_101_occ_int : 1;
		u32 cq_102_occ_int : 1;
		u32 cq_103_occ_int : 1;
		u32 cq_104_occ_int : 1;
		u32 cq_105_occ_int : 1;
		u32 cq_106_occ_int : 1;
		u32 cq_107_occ_int : 1;
		u32 cq_108_occ_int : 1;
		u32 cq_109_occ_int : 1;
		u32 cq_110_occ_int : 1;
		u32 cq_111_occ_int : 1;
		u32 cq_112_occ_int : 1;
		u32 cq_113_occ_int : 1;
		u32 cq_114_occ_int : 1;
		u32 cq_115_occ_int : 1;
		u32 cq_116_occ_int : 1;
		u32 cq_117_occ_int : 1;
		u32 cq_118_occ_int : 1;
		u32 cq_119_occ_int : 1;
		u32 cq_120_occ_int : 1;
		u32 cq_121_occ_int : 1;
		u32 cq_122_occ_int : 1;
		u32 cq_123_occ_int : 1;
		u32 cq_124_occ_int : 1;
		u32 cq_125_occ_int : 1;
		u32 cq_126_occ_int : 1;
		u32 cq_127_occ_int : 1;
	} field;
	u32 val;
};

#define DLB_SYS_LDB_CQ_31_0_OCC_INT_STS 0x460
#define DLB_SYS_LDB_CQ_31_0_OCC_INT_STS_RST 0x0
union dlb_sys_ldb_cq_31_0_occ_int_sts {
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

#define DLB_SYS_LDB_CQ_63_32_OCC_INT_STS 0x464
#define DLB_SYS_LDB_CQ_63_32_OCC_INT_STS_RST 0x0
union dlb_sys_ldb_cq_63_32_occ_int_sts {
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

#define DLB_SYS_ALARM_HW_SYND 0x50c
#define DLB_SYS_ALARM_HW_SYND_RST 0x0
union dlb_sys_alarm_hw_synd {
	struct {
		u32 syndrome : 8;
		u32 rtype : 2;
		u32 rsvd0 : 2;
		u32 from_dmv : 1;
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

#define DLB_SYS_SYS_ALARM_INT_ENABLE 0xc001048
#define DLB_SYS_SYS_ALARM_INT_ENABLE_RST 0x7fffff
union dlb_sys_sys_alarm_int_enable {
	struct {
		u32 cq_addr_overflow_error : 1;
		u32 ingress_perr : 1;
		u32 egress_perr : 1;
		u32 alarm_perr : 1;
		u32 vf_to_pf_isr_pend_error : 1;
		u32 pf_to_vf_isr_pend_error : 1;
		u32 timeout_error : 1;
		u32 dmvw_sm_error : 1;
		u32 pptr_sm_par_error : 1;
		u32 pptr_sm_len_error : 1;
		u32 sch_sm_error : 1;
		u32 wbuf_flag_error : 1;
		u32 dmvw_cl_error : 1;
		u32 dmvr_cl_error : 1;
		u32 cmpl_data_error : 1;
		u32 cmpl_error : 1;
		u32 fifo_underflow : 1;
		u32 fifo_overflow : 1;
		u32 sb_ep_parity_err : 1;
		u32 ti_parity_err : 1;
		u32 ri_parity_err : 1;
		u32 cfgm_ppw_err : 1;
		u32 system_csr_perr : 1;
		u32 rsvd0 : 9;
	} field;
	u32 val;
};

#define DLB_LSP_CQ_LDB_TOT_SCH_CNT_CTRL(x) \
	(0x20000000 + (x) * 0x1000)
#define DLB_LSP_CQ_LDB_TOT_SCH_CNT_CTRL_RST 0x0
union dlb_lsp_cq_ldb_tot_sch_cnt_ctrl {
	struct {
		u32 count : 32;
	} field;
	u32 val;
};

#define DLB_LSP_CQ_LDB_DSBL(x) \
	(0x20000124 + (x) * 0x1000)
#define DLB_LSP_CQ_LDB_DSBL_RST 0x1
union dlb_lsp_cq_ldb_dsbl {
	struct {
		u32 disabled : 1;
		u32 rsvd0 : 31;
	} field;
	u32 val;
};

#define DLB_LSP_CQ_LDB_TOT_SCH_CNTH(x) \
	(0x20000120 + (x) * 0x1000)
#define DLB_LSP_CQ_LDB_TOT_SCH_CNTH_RST 0x0
union dlb_lsp_cq_ldb_tot_sch_cnth {
	struct {
		u32 count : 32;
	} field;
	u32 val;
};

#define DLB_LSP_CQ_LDB_TOT_SCH_CNTL(x) \
	(0x2000011c + (x) * 0x1000)
#define DLB_LSP_CQ_LDB_TOT_SCH_CNTL_RST 0x0
union dlb_lsp_cq_ldb_tot_sch_cntl {
	struct {
		u32 count : 32;
	} field;
	u32 val;
};

#define DLB_LSP_CQ_LDB_TKN_DEPTH_SEL(x) \
	(0x20000118 + (x) * 0x1000)
#define DLB_LSP_CQ_LDB_TKN_DEPTH_SEL_RST 0x0
union dlb_lsp_cq_ldb_tkn_depth_sel {
	struct {
		u32 token_depth_select : 4;
		u32 ignore_depth : 1;
		u32 enab_shallow_cq : 1;
		u32 rsvd0 : 26;
	} field;
	u32 val;
};

#define DLB_LSP_CQ_LDB_TKN_CNT(x) \
	(0x20000114 + (x) * 0x1000)
#define DLB_LSP_CQ_LDB_TKN_CNT_RST 0x0
union dlb_lsp_cq_ldb_tkn_cnt {
	struct {
		u32 token_count : 11;
		u32 rsvd0 : 21;
	} field;
	u32 val;
};

#define DLB_LSP_CQ_LDB_INFL_LIM(x) \
	(0x20000110 + (x) * 0x1000)
#define DLB_LSP_CQ_LDB_INFL_LIM_RST 0x0
union dlb_lsp_cq_ldb_infl_lim {
	struct {
		u32 limit : 13;
		u32 rsvd0 : 19;
	} field;
	u32 val;
};

#define DLB_LSP_CQ_LDB_INFL_CNT(x) \
	(0x2000010c + (x) * 0x1000)
#define DLB_LSP_CQ_LDB_INFL_CNT_RST 0x0
union dlb_lsp_cq_ldb_infl_cnt {
	struct {
		u32 count : 13;
		u32 rsvd0 : 19;
	} field;
	u32 val;
};

#define DLB_LSP_CQ2QID(x, y) \
	(0x20000104 + (x) * 0x1000 + (y) * 0x4)
#define DLB_LSP_CQ2QID_RST 0x0
union dlb_lsp_cq2qid {
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

#define DLB_LSP_CQ2PRIOV(x) \
	(0x20000100 + (x) * 0x1000)
#define DLB_LSP_CQ2PRIOV_RST 0x0
union dlb_lsp_cq2priov {
	struct {
		u32 prio : 24;
		u32 v : 8;
	} field;
	u32 val;
};

#define DLB_LSP_CQ_DIR_DSBL(x) \
	(0x20000310 + (x) * 0x1000)
#define DLB_LSP_CQ_DIR_DSBL_RST 0x1
union dlb_lsp_cq_dir_dsbl {
	struct {
		u32 disabled : 1;
		u32 rsvd0 : 31;
	} field;
	u32 val;
};

#define DLB_LSP_CQ_DIR_TKN_DEPTH_SEL_DSI(x) \
	(0x2000030c + (x) * 0x1000)
#define DLB_LSP_CQ_DIR_TKN_DEPTH_SEL_DSI_RST 0x0
union dlb_lsp_cq_dir_tkn_depth_sel_dsi {
	struct {
		u32 token_depth_select : 4;
		u32 disable_wb_opt : 1;
		u32 ignore_depth : 1;
		u32 rsvd0 : 26;
	} field;
	u32 val;
};

#define DLB_LSP_CQ_DIR_TOT_SCH_CNTH(x) \
	(0x20000308 + (x) * 0x1000)
#define DLB_LSP_CQ_DIR_TOT_SCH_CNTH_RST 0x0
union dlb_lsp_cq_dir_tot_sch_cnth {
	struct {
		u32 count : 32;
	} field;
	u32 val;
};

#define DLB_LSP_CQ_DIR_TOT_SCH_CNTL(x) \
	(0x20000304 + (x) * 0x1000)
#define DLB_LSP_CQ_DIR_TOT_SCH_CNTL_RST 0x0
union dlb_lsp_cq_dir_tot_sch_cntl {
	struct {
		u32 count : 32;
	} field;
	u32 val;
};

#define DLB_LSP_CQ_DIR_TKN_CNT(x) \
	(0x20000300 + (x) * 0x1000)
#define DLB_LSP_CQ_DIR_TKN_CNT_RST 0x0
union dlb_lsp_cq_dir_tkn_cnt {
	struct {
		u32 count : 11;
		u32 rsvd0 : 21;
	} field;
	u32 val;
};

#define DLB_LSP_QID_LDB_QID2CQIDX(x, y) \
	(0x20000400 + (x) * 0x1000 + (y) * 0x4)
#define DLB_LSP_QID_LDB_QID2CQIDX_RST 0x0
union dlb_lsp_qid_ldb_qid2cqidx {
	struct {
		u32 cq_p0 : 8;
		u32 cq_p1 : 8;
		u32 cq_p2 : 8;
		u32 cq_p3 : 8;
	} field;
	u32 val;
};

#define DLB_LSP_QID_LDB_QID2CQIDX2(x, y) \
	(0x20000500 + (x) * 0x1000 + (y) * 0x4)
#define DLB_LSP_QID_LDB_QID2CQIDX2_RST 0x0
union dlb_lsp_qid_ldb_qid2cqidx2 {
	struct {
		u32 cq_p0 : 8;
		u32 cq_p1 : 8;
		u32 cq_p2 : 8;
		u32 cq_p3 : 8;
	} field;
	u32 val;
};

#define DLB_LSP_QID_ATQ_ENQUEUE_CNT(x) \
	(0x2000066c + (x) * 0x1000)
#define DLB_LSP_QID_ATQ_ENQUEUE_CNT_RST 0x0
union dlb_lsp_qid_atq_enqueue_cnt {
	struct {
		u32 count : 15;
		u32 rsvd0 : 17;
	} field;
	u32 val;
};

#define DLB_LSP_QID_LDB_INFL_LIM(x) \
	(0x2000064c + (x) * 0x1000)
#define DLB_LSP_QID_LDB_INFL_LIM_RST 0x0
union dlb_lsp_qid_ldb_infl_lim {
	struct {
		u32 limit : 13;
		u32 rsvd0 : 19;
	} field;
	u32 val;
};

#define DLB_LSP_QID_LDB_INFL_CNT(x) \
	(0x2000062c + (x) * 0x1000)
#define DLB_LSP_QID_LDB_INFL_CNT_RST 0x0
union dlb_lsp_qid_ldb_infl_cnt {
	struct {
		u32 count : 13;
		u32 rsvd0 : 19;
	} field;
	u32 val;
};

#define DLB_LSP_QID_AQED_ACTIVE_LIM(x) \
	(0x20000628 + (x) * 0x1000)
#define DLB_LSP_QID_AQED_ACTIVE_LIM_RST 0x0
union dlb_lsp_qid_aqed_active_lim {
	struct {
		u32 limit : 12;
		u32 rsvd0 : 20;
	} field;
	u32 val;
};

#define DLB_LSP_QID_AQED_ACTIVE_CNT(x) \
	(0x20000624 + (x) * 0x1000)
#define DLB_LSP_QID_AQED_ACTIVE_CNT_RST 0x0
union dlb_lsp_qid_aqed_active_cnt {
	struct {
		u32 count : 12;
		u32 rsvd0 : 20;
	} field;
	u32 val;
};

#define DLB_LSP_QID_LDB_ENQUEUE_CNT(x) \
	(0x20000604 + (x) * 0x1000)
#define DLB_LSP_QID_LDB_ENQUEUE_CNT_RST 0x0
union dlb_lsp_qid_ldb_enqueue_cnt {
	struct {
		u32 count : 15;
		u32 rsvd0 : 17;
	} field;
	u32 val;
};

#define DLB_LSP_QID_LDB_REPLAY_CNT(x) \
	(0x20000600 + (x) * 0x1000)
#define DLB_LSP_QID_LDB_REPLAY_CNT_RST 0x0
union dlb_lsp_qid_ldb_replay_cnt {
	struct {
		u32 count : 15;
		u32 rsvd0 : 17;
	} field;
	u32 val;
};

#define DLB_LSP_QID_DIR_ENQUEUE_CNT(x) \
	(0x20000700 + (x) * 0x1000)
#define DLB_LSP_QID_DIR_ENQUEUE_CNT_RST 0x0
union dlb_lsp_qid_dir_enqueue_cnt {
	struct {
		u32 count : 13;
		u32 rsvd0 : 19;
	} field;
	u32 val;
};

#define DLB_LSP_CTRL_CONFIG_0 0x2800002c
#define DLB_LSP_CTRL_CONFIG_0_RST 0x12cc
union dlb_lsp_ctrl_config_0 {
	struct {
		u32 atm_cq_qid_priority_prot : 1;
		u32 ldb_arb_ignore_empty : 1;
		u32 ldb_arb_mode : 2;
		u32 ldb_arb_threshold : 18;
		u32 cfg_cq_sla_upd_always : 1;
		u32 cfg_cq_wcn_upd_always : 1;
		u32 spare : 8;
	} field;
	u32 val;
};

#define DLB_LSP_CFG_ARB_WEIGHT_ATM_NALB_QID_1 0x28000028
#define DLB_LSP_CFG_ARB_WEIGHT_ATM_NALB_QID_1_RST 0x0
union dlb_lsp_cfg_arb_weight_atm_nalb_qid_1 {
	struct {
		u32 slot4_weight : 8;
		u32 slot5_weight : 8;
		u32 slot6_weight : 8;
		u32 slot7_weight : 8;
	} field;
	u32 val;
};

#define DLB_LSP_CFG_ARB_WEIGHT_ATM_NALB_QID_0 0x28000024
#define DLB_LSP_CFG_ARB_WEIGHT_ATM_NALB_QID_0_RST 0x0
union dlb_lsp_cfg_arb_weight_atm_nalb_qid_0 {
	struct {
		u32 slot0_weight : 8;
		u32 slot1_weight : 8;
		u32 slot2_weight : 8;
		u32 slot3_weight : 8;
	} field;
	u32 val;
};

#define DLB_LSP_CFG_ARB_WEIGHT_LDB_QID_1 0x28000020
#define DLB_LSP_CFG_ARB_WEIGHT_LDB_QID_1_RST 0x0
union dlb_lsp_cfg_arb_weight_ldb_qid_1 {
	struct {
		u32 slot4_weight : 8;
		u32 slot5_weight : 8;
		u32 slot6_weight : 8;
		u32 slot7_weight : 8;
	} field;
	u32 val;
};

#define DLB_LSP_CFG_ARB_WEIGHT_LDB_QID_0 0x2800001c
#define DLB_LSP_CFG_ARB_WEIGHT_LDB_QID_0_RST 0x0
union dlb_lsp_cfg_arb_weight_ldb_qid_0 {
	struct {
		u32 slot0_weight : 8;
		u32 slot1_weight : 8;
		u32 slot2_weight : 8;
		u32 slot3_weight : 8;
	} field;
	u32 val;
};

#define DLB_LSP_LDB_SCHED_CTRL 0x28100000
#define DLB_LSP_LDB_SCHED_CTRL_RST 0x0
union dlb_lsp_ldb_sched_ctrl {
	struct {
		u32 cq : 8;
		u32 qidix : 3;
		u32 value : 1;
		u32 nalb_haswork_v : 1;
		u32 rlist_haswork_v : 1;
		u32 slist_haswork_v : 1;
		u32 inflight_ok_v : 1;
		u32 aqed_nfull_v : 1;
		u32 spare0 : 15;
	} field;
	u32 val;
};

#define DLB_LSP_DIR_SCH_CNT_H 0x2820000c
#define DLB_LSP_DIR_SCH_CNT_H_RST 0x0
union dlb_lsp_dir_sch_cnt_h {
	struct {
		u32 count : 32;
	} field;
	u32 val;
};

#define DLB_LSP_DIR_SCH_CNT_L 0x28200008
#define DLB_LSP_DIR_SCH_CNT_L_RST 0x0
union dlb_lsp_dir_sch_cnt_l {
	struct {
		u32 count : 32;
	} field;
	u32 val;
};

#define DLB_LSP_LDB_SCH_CNT_H 0x28200004
#define DLB_LSP_LDB_SCH_CNT_H_RST 0x0
union dlb_lsp_ldb_sch_cnt_h {
	struct {
		u32 count : 32;
	} field;
	u32 val;
};

#define DLB_LSP_LDB_SCH_CNT_L 0x28200000
#define DLB_LSP_LDB_SCH_CNT_L_RST 0x0
union dlb_lsp_ldb_sch_cnt_l {
	struct {
		u32 count : 32;
	} field;
	u32 val;
};

#define DLB_DP_DIR_CSR_CTRL 0x38000018
#define DLB_DP_DIR_CSR_CTRL_RST 0xc0000000
union dlb_dp_dir_csr_ctrl {
	struct {
		u32 cfg_int_dis : 1;
		u32 cfg_int_dis_sbe : 1;
		u32 cfg_int_dis_mbe : 1;
		u32 spare0 : 27;
		u32 cfg_vasr_dis : 1;
		u32 cfg_int_dis_synd : 1;
	} field;
	u32 val;
};

#define DLB_DP_CFG_CTRL_ARB_WEIGHTS_TQPRI_DIR_1 0x38000014
#define DLB_DP_CFG_CTRL_ARB_WEIGHTS_TQPRI_DIR_1_RST 0xfffefdfc
union dlb_dp_cfg_ctrl_arb_weights_tqpri_dir_1 {
	struct {
		u32 pri4 : 8;
		u32 pri5 : 8;
		u32 pri6 : 8;
		u32 pri7 : 8;
	} field;
	u32 val;
};

#define DLB_DP_CFG_CTRL_ARB_WEIGHTS_TQPRI_DIR_0 0x38000010
#define DLB_DP_CFG_CTRL_ARB_WEIGHTS_TQPRI_DIR_0_RST 0xfbfaf9f8
union dlb_dp_cfg_ctrl_arb_weights_tqpri_dir_0 {
	struct {
		u32 pri0 : 8;
		u32 pri1 : 8;
		u32 pri2 : 8;
		u32 pri3 : 8;
	} field;
	u32 val;
};

#define DLB_DP_CFG_CTRL_ARB_WEIGHTS_TQPRI_REPLAY_1 0x3800000c
#define DLB_DP_CFG_CTRL_ARB_WEIGHTS_TQPRI_REPLAY_1_RST 0xfffefdfc
union dlb_dp_cfg_ctrl_arb_weights_tqpri_replay_1 {
	struct {
		u32 pri4 : 8;
		u32 pri5 : 8;
		u32 pri6 : 8;
		u32 pri7 : 8;
	} field;
	u32 val;
};

#define DLB_DP_CFG_CTRL_ARB_WEIGHTS_TQPRI_REPLAY_0 0x38000008
#define DLB_DP_CFG_CTRL_ARB_WEIGHTS_TQPRI_REPLAY_0_RST 0xfbfaf9f8
union dlb_dp_cfg_ctrl_arb_weights_tqpri_replay_0 {
	struct {
		u32 pri0 : 8;
		u32 pri1 : 8;
		u32 pri2 : 8;
		u32 pri3 : 8;
	} field;
	u32 val;
};

#define DLB_NALB_PIPE_CTRL_ARB_WEIGHTS_TQPRI_NALB_1 0x6800001c
#define DLB_NALB_PIPE_CTRL_ARB_WEIGHTS_TQPRI_NALB_1_RST 0xfffefdfc
union dlb_nalb_pipe_ctrl_arb_weights_tqpri_nalb_1 {
	struct {
		u32 pri4 : 8;
		u32 pri5 : 8;
		u32 pri6 : 8;
		u32 pri7 : 8;
	} field;
	u32 val;
};

#define DLB_NALB_PIPE_CTRL_ARB_WEIGHTS_TQPRI_NALB_0 0x68000018
#define DLB_NALB_PIPE_CTRL_ARB_WEIGHTS_TQPRI_NALB_0_RST 0xfbfaf9f8
union dlb_nalb_pipe_ctrl_arb_weights_tqpri_nalb_0 {
	struct {
		u32 pri0 : 8;
		u32 pri1 : 8;
		u32 pri2 : 8;
		u32 pri3 : 8;
	} field;
	u32 val;
};

#define DLB_NALB_PIPE_CFG_CTRL_ARB_WEIGHTS_TQPRI_ATQ_1 0x68000014
#define DLB_NALB_PIPE_CFG_CTRL_ARB_WEIGHTS_TQPRI_ATQ_1_RST 0xfffefdfc
union dlb_nalb_pipe_cfg_ctrl_arb_weights_tqpri_atq_1 {
	struct {
		u32 pri4 : 8;
		u32 pri5 : 8;
		u32 pri6 : 8;
		u32 pri7 : 8;
	} field;
	u32 val;
};

#define DLB_NALB_PIPE_CFG_CTRL_ARB_WEIGHTS_TQPRI_ATQ_0 0x68000010
#define DLB_NALB_PIPE_CFG_CTRL_ARB_WEIGHTS_TQPRI_ATQ_0_RST 0xfbfaf9f8
union dlb_nalb_pipe_cfg_ctrl_arb_weights_tqpri_atq_0 {
	struct {
		u32 pri0 : 8;
		u32 pri1 : 8;
		u32 pri2 : 8;
		u32 pri3 : 8;
	} field;
	u32 val;
};

#define DLB_NALB_PIPE_CFG_CTRL_ARB_WEIGHTS_TQPRI_REPLAY_1 0x6800000c
#define DLB_NALB_PIPE_CFG_CTRL_ARB_WEIGHTS_TQPRI_REPLAY_1_RST 0xfffefdfc
union dlb_nalb_pipe_cfg_ctrl_arb_weights_tqpri_replay_1 {
	struct {
		u32 pri4 : 8;
		u32 pri5 : 8;
		u32 pri6 : 8;
		u32 pri7 : 8;
	} field;
	u32 val;
};

#define DLB_NALB_PIPE_CFG_CTRL_ARB_WEIGHTS_TQPRI_REPLAY_0 0x68000008
#define DLB_NALB_PIPE_CFG_CTRL_ARB_WEIGHTS_TQPRI_REPLAY_0_RST 0xfbfaf9f8
union dlb_nalb_pipe_cfg_ctrl_arb_weights_tqpri_replay_0 {
	struct {
		u32 pri0 : 8;
		u32 pri1 : 8;
		u32 pri2 : 8;
		u32 pri3 : 8;
	} field;
	u32 val;
};

#define DLB_ATM_PIPE_QID_LDB_QID2CQIDX(x, y) \
	(0x70000000 + (x) * 0x1000 + (y) * 0x4)
#define DLB_ATM_PIPE_QID_LDB_QID2CQIDX_RST 0x0
union dlb_atm_pipe_qid_ldb_qid2cqidx {
	struct {
		u32 cq_p0 : 8;
		u32 cq_p1 : 8;
		u32 cq_p2 : 8;
		u32 cq_p3 : 8;
	} field;
	u32 val;
};

#define DLB_ATM_PIPE_CFG_CTRL_ARB_WEIGHTS_SCHED_BIN 0x7800000c
#define DLB_ATM_PIPE_CFG_CTRL_ARB_WEIGHTS_SCHED_BIN_RST 0xfffefdfc
union dlb_atm_pipe_cfg_ctrl_arb_weights_sched_bin {
	struct {
		u32 bin0 : 8;
		u32 bin1 : 8;
		u32 bin2 : 8;
		u32 bin3 : 8;
	} field;
	u32 val;
};

#define DLB_ATM_PIPE_CTRL_ARB_WEIGHTS_RDY_BIN 0x78000008
#define DLB_ATM_PIPE_CTRL_ARB_WEIGHTS_RDY_BIN_RST 0xfffefdfc
union dlb_atm_pipe_ctrl_arb_weights_rdy_bin {
	struct {
		u32 bin0 : 8;
		u32 bin1 : 8;
		u32 bin2 : 8;
		u32 bin3 : 8;
	} field;
	u32 val;
};

#define DLB_AQED_PIPE_QID_FID_LIM(x) \
	(0x80000014 + (x) * 0x1000)
#define DLB_AQED_PIPE_QID_FID_LIM_RST 0x7ff
union dlb_aqed_pipe_qid_fid_lim {
	struct {
		u32 qid_fid_limit : 13;
		u32 rsvd0 : 19;
	} field;
	u32 val;
};

#define DLB_AQED_PIPE_FL_POP_PTR(x) \
	(0x80000010 + (x) * 0x1000)
#define DLB_AQED_PIPE_FL_POP_PTR_RST 0x0
union dlb_aqed_pipe_fl_pop_ptr {
	struct {
		u32 pop_ptr : 11;
		u32 generation : 1;
		u32 rsvd0 : 20;
	} field;
	u32 val;
};

#define DLB_AQED_PIPE_FL_PUSH_PTR(x) \
	(0x8000000c + (x) * 0x1000)
#define DLB_AQED_PIPE_FL_PUSH_PTR_RST 0x0
union dlb_aqed_pipe_fl_push_ptr {
	struct {
		u32 push_ptr : 11;
		u32 generation : 1;
		u32 rsvd0 : 20;
	} field;
	u32 val;
};

#define DLB_AQED_PIPE_FL_BASE(x) \
	(0x80000008 + (x) * 0x1000)
#define DLB_AQED_PIPE_FL_BASE_RST 0x0
union dlb_aqed_pipe_fl_base {
	struct {
		u32 base : 11;
		u32 rsvd0 : 21;
	} field;
	u32 val;
};

#define DLB_AQED_PIPE_FL_LIM(x) \
	(0x80000004 + (x) * 0x1000)
#define DLB_AQED_PIPE_FL_LIM_RST 0x800
union dlb_aqed_pipe_fl_lim {
	struct {
		u32 limit : 11;
		u32 freelist_disable : 1;
		u32 rsvd0 : 20;
	} field;
	u32 val;
};

#define DLB_AQED_PIPE_CFG_CTRL_ARB_WEIGHTS_TQPRI_ATM_0 0x88000008
#define DLB_AQED_PIPE_CFG_CTRL_ARB_WEIGHTS_TQPRI_ATM_0_RST 0xfffe
union dlb_aqed_pipe_cfg_ctrl_arb_weights_tqpri_atm_0 {
	struct {
		u32 pri0 : 8;
		u32 pri1 : 8;
		u32 pri2 : 8;
		u32 pri3 : 8;
	} field;
	u32 val;
};

#define DLB_RO_PIPE_QID2GRPSLT(x) \
	(0x90000000 + (x) * 0x1000)
#define DLB_RO_PIPE_QID2GRPSLT_RST 0x0
union dlb_ro_pipe_qid2grpslt {
	struct {
		u32 slot : 5;
		u32 rsvd1 : 3;
		u32 group : 2;
		u32 rsvd0 : 22;
	} field;
	u32 val;
};

#define DLB_RO_PIPE_GRP_SN_MODE 0x98000008
#define DLB_RO_PIPE_GRP_SN_MODE_RST 0x0
union dlb_ro_pipe_grp_sn_mode {
	struct {
		u32 sn_mode_0 : 3;
		u32 reserved0 : 5;
		u32 sn_mode_1 : 3;
		u32 reserved1 : 5;
		u32 sn_mode_2 : 3;
		u32 reserved2 : 5;
		u32 sn_mode_3 : 3;
		u32 reserved3 : 5;
	} field;
	u32 val;
};

#define DLB_CHP_CFG_DIR_PP_SW_ALARM_EN(x) \
	(0xa000003c + (x) * 0x1000)
#define DLB_CHP_CFG_DIR_PP_SW_ALARM_EN_RST 0x1
union dlb_chp_cfg_dir_pp_sw_alarm_en {
	struct {
		u32 alarm_enable : 1;
		u32 rsvd0 : 31;
	} field;
	u32 val;
};

#define DLB_CHP_DIR_CQ_WD_ENB(x) \
	(0xa0000038 + (x) * 0x1000)
#define DLB_CHP_DIR_CQ_WD_ENB_RST 0x0
union dlb_chp_dir_cq_wd_enb {
	struct {
		u32 wd_enable : 1;
		u32 rsvd0 : 31;
	} field;
	u32 val;
};

#define DLB_CHP_DIR_LDB_PP2POOL(x) \
	(0xa0000034 + (x) * 0x1000)
#define DLB_CHP_DIR_LDB_PP2POOL_RST 0x0
union dlb_chp_dir_ldb_pp2pool {
	struct {
		u32 pool : 6;
		u32 rsvd0 : 26;
	} field;
	u32 val;
};

#define DLB_CHP_DIR_DIR_PP2POOL(x) \
	(0xa0000030 + (x) * 0x1000)
#define DLB_CHP_DIR_DIR_PP2POOL_RST 0x0
union dlb_chp_dir_dir_pp2pool {
	struct {
		u32 pool : 6;
		u32 rsvd0 : 26;
	} field;
	u32 val;
};

#define DLB_CHP_DIR_PP_LDB_CRD_CNT(x) \
	(0xa000002c + (x) * 0x1000)
#define DLB_CHP_DIR_PP_LDB_CRD_CNT_RST 0x0
union dlb_chp_dir_pp_ldb_crd_cnt {
	struct {
		u32 count : 16;
		u32 rsvd0 : 16;
	} field;
	u32 val;
};

#define DLB_CHP_DIR_PP_DIR_CRD_CNT(x) \
	(0xa0000028 + (x) * 0x1000)
#define DLB_CHP_DIR_PP_DIR_CRD_CNT_RST 0x0
union dlb_chp_dir_pp_dir_crd_cnt {
	struct {
		u32 count : 14;
		u32 rsvd0 : 18;
	} field;
	u32 val;
};

#define DLB_CHP_DIR_CQ_TMR_THRESHOLD(x) \
	(0xa0000024 + (x) * 0x1000)
#define DLB_CHP_DIR_CQ_TMR_THRESHOLD_RST 0x0
union dlb_chp_dir_cq_tmr_threshold {
	struct {
		u32 timer_thrsh : 14;
		u32 rsvd0 : 18;
	} field;
	u32 val;
};

#define DLB_CHP_DIR_CQ_INT_ENB(x) \
	(0xa0000020 + (x) * 0x1000)
#define DLB_CHP_DIR_CQ_INT_ENB_RST 0x0
union dlb_chp_dir_cq_int_enb {
	struct {
		u32 en_tim : 1;
		u32 en_depth : 1;
		u32 rsvd0 : 30;
	} field;
	u32 val;
};

#define DLB_CHP_DIR_CQ_INT_DEPTH_THRSH(x) \
	(0xa000001c + (x) * 0x1000)
#define DLB_CHP_DIR_CQ_INT_DEPTH_THRSH_RST 0x0
union dlb_chp_dir_cq_int_depth_thrsh {
	struct {
		u32 depth_threshold : 12;
		u32 rsvd0 : 20;
	} field;
	u32 val;
};

#define DLB_CHP_DIR_CQ_TKN_DEPTH_SEL(x) \
	(0xa0000018 + (x) * 0x1000)
#define DLB_CHP_DIR_CQ_TKN_DEPTH_SEL_RST 0x0
union dlb_chp_dir_cq_tkn_depth_sel {
	struct {
		u32 token_depth_select : 4;
		u32 rsvd0 : 28;
	} field;
	u32 val;
};

#define DLB_CHP_DIR_PP_LDB_MIN_CRD_QNT(x) \
	(0xa0000014 + (x) * 0x1000)
#define DLB_CHP_DIR_PP_LDB_MIN_CRD_QNT_RST 0x1
union dlb_chp_dir_pp_ldb_min_crd_qnt {
	struct {
		u32 quanta : 10;
		u32 rsvd0 : 22;
	} field;
	u32 val;
};

#define DLB_CHP_DIR_PP_DIR_MIN_CRD_QNT(x) \
	(0xa0000010 + (x) * 0x1000)
#define DLB_CHP_DIR_PP_DIR_MIN_CRD_QNT_RST 0x1
union dlb_chp_dir_pp_dir_min_crd_qnt {
	struct {
		u32 quanta : 10;
		u32 rsvd0 : 22;
	} field;
	u32 val;
};

#define DLB_CHP_DIR_PP_LDB_CRD_LWM(x) \
	(0xa000000c + (x) * 0x1000)
#define DLB_CHP_DIR_PP_LDB_CRD_LWM_RST 0x0
union dlb_chp_dir_pp_ldb_crd_lwm {
	struct {
		u32 lwm : 16;
		u32 rsvd0 : 16;
	} field;
	u32 val;
};

#define DLB_CHP_DIR_PP_LDB_CRD_HWM(x) \
	(0xa0000008 + (x) * 0x1000)
#define DLB_CHP_DIR_PP_LDB_CRD_HWM_RST 0x0
union dlb_chp_dir_pp_ldb_crd_hwm {
	struct {
		u32 hwm : 16;
		u32 rsvd0 : 16;
	} field;
	u32 val;
};

#define DLB_CHP_DIR_PP_DIR_CRD_LWM(x) \
	(0xa0000004 + (x) * 0x1000)
#define DLB_CHP_DIR_PP_DIR_CRD_LWM_RST 0x0
union dlb_chp_dir_pp_dir_crd_lwm {
	struct {
		u32 lwm : 14;
		u32 rsvd0 : 18;
	} field;
	u32 val;
};

#define DLB_CHP_DIR_PP_DIR_CRD_HWM(x) \
	(0xa0000000 + (x) * 0x1000)
#define DLB_CHP_DIR_PP_DIR_CRD_HWM_RST 0x0
union dlb_chp_dir_pp_dir_crd_hwm {
	struct {
		u32 hwm : 14;
		u32 rsvd0 : 18;
	} field;
	u32 val;
};

#define DLB_CHP_CFG_LDB_PP_SW_ALARM_EN(x) \
	(0xa0000148 + (x) * 0x1000)
#define DLB_CHP_CFG_LDB_PP_SW_ALARM_EN_RST 0x1
union dlb_chp_cfg_ldb_pp_sw_alarm_en {
	struct {
		u32 alarm_enable : 1;
		u32 rsvd0 : 31;
	} field;
	u32 val;
};

#define DLB_CHP_LDB_CQ_WD_ENB(x) \
	(0xa0000144 + (x) * 0x1000)
#define DLB_CHP_LDB_CQ_WD_ENB_RST 0x0
union dlb_chp_ldb_cq_wd_enb {
	struct {
		u32 wd_enable : 1;
		u32 rsvd0 : 31;
	} field;
	u32 val;
};

#define DLB_CHP_SN_CHK_ENBL(x) \
	(0xa0000140 + (x) * 0x1000)
#define DLB_CHP_SN_CHK_ENBL_RST 0x0
union dlb_chp_sn_chk_enbl {
	struct {
		u32 en : 1;
		u32 rsvd0 : 31;
	} field;
	u32 val;
};

#define DLB_CHP_HIST_LIST_BASE(x) \
	(0xa000013c + (x) * 0x1000)
#define DLB_CHP_HIST_LIST_BASE_RST 0x0
union dlb_chp_hist_list_base {
	struct {
		u32 base : 13;
		u32 rsvd0 : 19;
	} field;
	u32 val;
};

#define DLB_CHP_HIST_LIST_LIM(x) \
	(0xa0000138 + (x) * 0x1000)
#define DLB_CHP_HIST_LIST_LIM_RST 0x0
union dlb_chp_hist_list_lim {
	struct {
		u32 limit : 13;
		u32 rsvd0 : 19;
	} field;
	u32 val;
};

#define DLB_CHP_LDB_LDB_PP2POOL(x) \
	(0xa0000134 + (x) * 0x1000)
#define DLB_CHP_LDB_LDB_PP2POOL_RST 0x0
union dlb_chp_ldb_ldb_pp2pool {
	struct {
		u32 pool : 6;
		u32 rsvd0 : 26;
	} field;
	u32 val;
};

#define DLB_CHP_LDB_DIR_PP2POOL(x) \
	(0xa0000130 + (x) * 0x1000)
#define DLB_CHP_LDB_DIR_PP2POOL_RST 0x0
union dlb_chp_ldb_dir_pp2pool {
	struct {
		u32 pool : 6;
		u32 rsvd0 : 26;
	} field;
	u32 val;
};

#define DLB_CHP_LDB_PP_LDB_CRD_CNT(x) \
	(0xa000012c + (x) * 0x1000)
#define DLB_CHP_LDB_PP_LDB_CRD_CNT_RST 0x0
union dlb_chp_ldb_pp_ldb_crd_cnt {
	struct {
		u32 count : 16;
		u32 rsvd0 : 16;
	} field;
	u32 val;
};

#define DLB_CHP_LDB_PP_DIR_CRD_CNT(x) \
	(0xa0000128 + (x) * 0x1000)
#define DLB_CHP_LDB_PP_DIR_CRD_CNT_RST 0x0
union dlb_chp_ldb_pp_dir_crd_cnt {
	struct {
		u32 count : 14;
		u32 rsvd0 : 18;
	} field;
	u32 val;
};

#define DLB_CHP_LDB_CQ_TMR_THRESHOLD(x) \
	(0xa0000124 + (x) * 0x1000)
#define DLB_CHP_LDB_CQ_TMR_THRESHOLD_RST 0x0
union dlb_chp_ldb_cq_tmr_threshold {
	struct {
		u32 thrsh : 14;
		u32 rsvd0 : 18;
	} field;
	u32 val;
};

#define DLB_CHP_LDB_CQ_INT_ENB(x) \
	(0xa0000120 + (x) * 0x1000)
#define DLB_CHP_LDB_CQ_INT_ENB_RST 0x0
union dlb_chp_ldb_cq_int_enb {
	struct {
		u32 en_tim : 1;
		u32 en_depth : 1;
		u32 rsvd0 : 30;
	} field;
	u32 val;
};

#define DLB_CHP_LDB_CQ_INT_DEPTH_THRSH(x) \
	(0xa000011c + (x) * 0x1000)
#define DLB_CHP_LDB_CQ_INT_DEPTH_THRSH_RST 0x0
union dlb_chp_ldb_cq_int_depth_thrsh {
	struct {
		u32 depth_threshold : 12;
		u32 rsvd0 : 20;
	} field;
	u32 val;
};

#define DLB_CHP_LDB_CQ_TKN_DEPTH_SEL(x) \
	(0xa0000118 + (x) * 0x1000)
#define DLB_CHP_LDB_CQ_TKN_DEPTH_SEL_RST 0x0
union dlb_chp_ldb_cq_tkn_depth_sel {
	struct {
		u32 token_depth_select : 4;
		u32 rsvd0 : 28;
	} field;
	u32 val;
};

#define DLB_CHP_LDB_PP_LDB_MIN_CRD_QNT(x) \
	(0xa0000114 + (x) * 0x1000)
#define DLB_CHP_LDB_PP_LDB_MIN_CRD_QNT_RST 0x1
union dlb_chp_ldb_pp_ldb_min_crd_qnt {
	struct {
		u32 quanta : 10;
		u32 rsvd0 : 22;
	} field;
	u32 val;
};

#define DLB_CHP_LDB_PP_DIR_MIN_CRD_QNT(x) \
	(0xa0000110 + (x) * 0x1000)
#define DLB_CHP_LDB_PP_DIR_MIN_CRD_QNT_RST 0x1
union dlb_chp_ldb_pp_dir_min_crd_qnt {
	struct {
		u32 quanta : 10;
		u32 rsvd0 : 22;
	} field;
	u32 val;
};

#define DLB_CHP_LDB_PP_LDB_CRD_LWM(x) \
	(0xa000010c + (x) * 0x1000)
#define DLB_CHP_LDB_PP_LDB_CRD_LWM_RST 0x0
union dlb_chp_ldb_pp_ldb_crd_lwm {
	struct {
		u32 lwm : 16;
		u32 rsvd0 : 16;
	} field;
	u32 val;
};

#define DLB_CHP_LDB_PP_LDB_CRD_HWM(x) \
	(0xa0000108 + (x) * 0x1000)
#define DLB_CHP_LDB_PP_LDB_CRD_HWM_RST 0x0
union dlb_chp_ldb_pp_ldb_crd_hwm {
	struct {
		u32 hwm : 16;
		u32 rsvd0 : 16;
	} field;
	u32 val;
};

#define DLB_CHP_LDB_PP_DIR_CRD_LWM(x) \
	(0xa0000104 + (x) * 0x1000)
#define DLB_CHP_LDB_PP_DIR_CRD_LWM_RST 0x0
union dlb_chp_ldb_pp_dir_crd_lwm {
	struct {
		u32 lwm : 14;
		u32 rsvd0 : 18;
	} field;
	u32 val;
};

#define DLB_CHP_LDB_PP_DIR_CRD_HWM(x) \
	(0xa0000100 + (x) * 0x1000)
#define DLB_CHP_LDB_PP_DIR_CRD_HWM_RST 0x0
union dlb_chp_ldb_pp_dir_crd_hwm {
	struct {
		u32 hwm : 14;
		u32 rsvd0 : 18;
	} field;
	u32 val;
};

#define DLB_CHP_DIR_CQ_DEPTH(x) \
	(0xa0000218 + (x) * 0x1000)
#define DLB_CHP_DIR_CQ_DEPTH_RST 0x0
union dlb_chp_dir_cq_depth {
	struct {
		u32 cq_depth : 11;
		u32 rsvd0 : 21;
	} field;
	u32 val;
};

#define DLB_CHP_DIR_CQ_WPTR(x) \
	(0xa0000214 + (x) * 0x1000)
#define DLB_CHP_DIR_CQ_WPTR_RST 0x0
union dlb_chp_dir_cq_wptr {
	struct {
		u32 write_pointer : 10;
		u32 rsvd0 : 22;
	} field;
	u32 val;
};

#define DLB_CHP_DIR_PP_LDB_PUSH_PTR(x) \
	(0xa0000210 + (x) * 0x1000)
#define DLB_CHP_DIR_PP_LDB_PUSH_PTR_RST 0x0
union dlb_chp_dir_pp_ldb_push_ptr {
	struct {
		u32 push_pointer : 16;
		u32 rsvd0 : 16;
	} field;
	u32 val;
};

#define DLB_CHP_DIR_PP_DIR_PUSH_PTR(x) \
	(0xa000020c + (x) * 0x1000)
#define DLB_CHP_DIR_PP_DIR_PUSH_PTR_RST 0x0
union dlb_chp_dir_pp_dir_push_ptr {
	struct {
		u32 push_pointer : 16;
		u32 rsvd0 : 16;
	} field;
	u32 val;
};

#define DLB_CHP_DIR_PP_STATE_RESET(x) \
	(0xa0000204 + (x) * 0x1000)
#define DLB_CHP_DIR_PP_STATE_RESET_RST 0x0
union dlb_chp_dir_pp_state_reset {
	struct {
		u32 rsvd1 : 7;
		u32 dir_type : 1;
		u32 rsvd0 : 23;
		u32 reset_pp_state : 1;
	} field;
	u32 val;
};

#define DLB_CHP_DIR_PP_CRD_REQ_STATE(x) \
	(0xa0000200 + (x) * 0x1000)
#define DLB_CHP_DIR_PP_CRD_REQ_STATE_RST 0x0
union dlb_chp_dir_pp_crd_req_state {
	struct {
		u32 dir_crd_req_active_valid : 1;
		u32 dir_crd_req_active_check : 1;
		u32 dir_crd_req_active_busy : 1;
		u32 rsvd1 : 1;
		u32 ldb_crd_req_active_valid : 1;
		u32 ldb_crd_req_active_check : 1;
		u32 ldb_crd_req_active_busy : 1;
		u32 rsvd0 : 1;
		u32 no_pp_credit_update : 1;
		u32 crd_req_state : 23;
	} field;
	u32 val;
};

#define DLB_CHP_LDB_CQ_DEPTH(x) \
	(0xa0000320 + (x) * 0x1000)
#define DLB_CHP_LDB_CQ_DEPTH_RST 0x0
union dlb_chp_ldb_cq_depth {
	struct {
		u32 depth : 11;
		u32 reserved : 2;
		u32 rsvd0 : 19;
	} field;
	u32 val;
};

#define DLB_CHP_LDB_CQ_WPTR(x) \
	(0xa000031c + (x) * 0x1000)
#define DLB_CHP_LDB_CQ_WPTR_RST 0x0
union dlb_chp_ldb_cq_wptr {
	struct {
		u32 write_pointer : 10;
		u32 rsvd0 : 22;
	} field;
	u32 val;
};

#define DLB_CHP_LDB_PP_LDB_PUSH_PTR(x) \
	(0xa0000318 + (x) * 0x1000)
#define DLB_CHP_LDB_PP_LDB_PUSH_PTR_RST 0x0
union dlb_chp_ldb_pp_ldb_push_ptr {
	struct {
		u32 push_pointer : 16;
		u32 rsvd0 : 16;
	} field;
	u32 val;
};

#define DLB_CHP_LDB_PP_DIR_PUSH_PTR(x) \
	(0xa0000314 + (x) * 0x1000)
#define DLB_CHP_LDB_PP_DIR_PUSH_PTR_RST 0x0
union dlb_chp_ldb_pp_dir_push_ptr {
	struct {
		u32 push_pointer : 16;
		u32 rsvd0 : 16;
	} field;
	u32 val;
};

#define DLB_CHP_HIST_LIST_POP_PTR(x) \
	(0xa000030c + (x) * 0x1000)
#define DLB_CHP_HIST_LIST_POP_PTR_RST 0x0
union dlb_chp_hist_list_pop_ptr {
	struct {
		u32 pop_ptr : 13;
		u32 generation : 1;
		u32 rsvd0 : 18;
	} field;
	u32 val;
};

#define DLB_CHP_HIST_LIST_PUSH_PTR(x) \
	(0xa0000308 + (x) * 0x1000)
#define DLB_CHP_HIST_LIST_PUSH_PTR_RST 0x0
union dlb_chp_hist_list_push_ptr {
	struct {
		u32 push_ptr : 13;
		u32 generation : 1;
		u32 rsvd0 : 18;
	} field;
	u32 val;
};

#define DLB_CHP_LDB_PP_STATE_RESET(x) \
	(0xa0000304 + (x) * 0x1000)
#define DLB_CHP_LDB_PP_STATE_RESET_RST 0x0
union dlb_chp_ldb_pp_state_reset {
	struct {
		u32 rsvd1 : 7;
		u32 dir_type : 1;
		u32 rsvd0 : 23;
		u32 reset_pp_state : 1;
	} field;
	u32 val;
};

#define DLB_CHP_LDB_PP_CRD_REQ_STATE(x) \
	(0xa0000300 + (x) * 0x1000)
#define DLB_CHP_LDB_PP_CRD_REQ_STATE_RST 0x0
union dlb_chp_ldb_pp_crd_req_state {
	struct {
		u32 dir_crd_req_active_valid : 1;
		u32 dir_crd_req_active_check : 1;
		u32 dir_crd_req_active_busy : 1;
		u32 rsvd1 : 1;
		u32 ldb_crd_req_active_valid : 1;
		u32 ldb_crd_req_active_check : 1;
		u32 ldb_crd_req_active_busy : 1;
		u32 rsvd0 : 1;
		u32 no_pp_credit_update : 1;
		u32 crd_req_state : 23;
	} field;
	u32 val;
};

#define DLB_CHP_ORD_QID_SN(x) \
	(0xa0000408 + (x) * 0x1000)
#define DLB_CHP_ORD_QID_SN_RST 0x0
union dlb_chp_ord_qid_sn {
	struct {
		u32 sn : 12;
		u32 rsvd0 : 20;
	} field;
	u32 val;
};

#define DLB_CHP_ORD_QID_SN_MAP(x) \
	(0xa0000404 + (x) * 0x1000)
#define DLB_CHP_ORD_QID_SN_MAP_RST 0x0
union dlb_chp_ord_qid_sn_map {
	struct {
		u32 mode : 3;
		u32 slot : 5;
		u32 grp : 2;
		u32 rsvd0 : 22;
	} field;
	u32 val;
};

#define DLB_CHP_LDB_POOL_CRD_CNT(x) \
	(0xa000050c + (x) * 0x1000)
#define DLB_CHP_LDB_POOL_CRD_CNT_RST 0x0
union dlb_chp_ldb_pool_crd_cnt {
	struct {
		u32 count : 16;
		u32 rsvd0 : 16;
	} field;
	u32 val;
};

#define DLB_CHP_QED_FL_BASE(x) \
	(0xa0000508 + (x) * 0x1000)
#define DLB_CHP_QED_FL_BASE_RST 0x0
union dlb_chp_qed_fl_base {
	struct {
		u32 base : 14;
		u32 rsvd0 : 18;
	} field;
	u32 val;
};

#define DLB_CHP_QED_FL_LIM(x) \
	(0xa0000504 + (x) * 0x1000)
#define DLB_CHP_QED_FL_LIM_RST 0x8000
union dlb_chp_qed_fl_lim {
	struct {
		u32 limit : 14;
		u32 rsvd1 : 1;
		u32 freelist_disable : 1;
		u32 rsvd0 : 16;
	} field;
	u32 val;
};

#define DLB_CHP_LDB_POOL_CRD_LIM(x) \
	(0xa0000500 + (x) * 0x1000)
#define DLB_CHP_LDB_POOL_CRD_LIM_RST 0x0
union dlb_chp_ldb_pool_crd_lim {
	struct {
		u32 limit : 16;
		u32 rsvd0 : 16;
	} field;
	u32 val;
};

#define DLB_CHP_QED_FL_POP_PTR(x) \
	(0xa0000604 + (x) * 0x1000)
#define DLB_CHP_QED_FL_POP_PTR_RST 0x0
union dlb_chp_qed_fl_pop_ptr {
	struct {
		u32 pop_ptr : 14;
		u32 reserved0 : 1;
		u32 generation : 1;
		u32 rsvd0 : 16;
	} field;
	u32 val;
};

#define DLB_CHP_QED_FL_PUSH_PTR(x) \
	(0xa0000600 + (x) * 0x1000)
#define DLB_CHP_QED_FL_PUSH_PTR_RST 0x0
union dlb_chp_qed_fl_push_ptr {
	struct {
		u32 push_ptr : 14;
		u32 reserved0 : 1;
		u32 generation : 1;
		u32 rsvd0 : 16;
	} field;
	u32 val;
};

#define DLB_CHP_DIR_POOL_CRD_CNT(x) \
	(0xa000070c + (x) * 0x1000)
#define DLB_CHP_DIR_POOL_CRD_CNT_RST 0x0
union dlb_chp_dir_pool_crd_cnt {
	struct {
		u32 count : 14;
		u32 rsvd0 : 18;
	} field;
	u32 val;
};

#define DLB_CHP_DQED_FL_BASE(x) \
	(0xa0000708 + (x) * 0x1000)
#define DLB_CHP_DQED_FL_BASE_RST 0x0
union dlb_chp_dqed_fl_base {
	struct {
		u32 base : 12;
		u32 rsvd0 : 20;
	} field;
	u32 val;
};

#define DLB_CHP_DQED_FL_LIM(x) \
	(0xa0000704 + (x) * 0x1000)
#define DLB_CHP_DQED_FL_LIM_RST 0x2000
union dlb_chp_dqed_fl_lim {
	struct {
		u32 limit : 12;
		u32 rsvd1 : 1;
		u32 freelist_disable : 1;
		u32 rsvd0 : 18;
	} field;
	u32 val;
};

#define DLB_CHP_DIR_POOL_CRD_LIM(x) \
	(0xa0000700 + (x) * 0x1000)
#define DLB_CHP_DIR_POOL_CRD_LIM_RST 0x0
union dlb_chp_dir_pool_crd_lim {
	struct {
		u32 limit : 14;
		u32 rsvd0 : 18;
	} field;
	u32 val;
};

#define DLB_CHP_DQED_FL_POP_PTR(x) \
	(0xa0000804 + (x) * 0x1000)
#define DLB_CHP_DQED_FL_POP_PTR_RST 0x0
union dlb_chp_dqed_fl_pop_ptr {
	struct {
		u32 pop_ptr : 12;
		u32 reserved0 : 1;
		u32 generation : 1;
		u32 rsvd0 : 18;
	} field;
	u32 val;
};

#define DLB_CHP_DQED_FL_PUSH_PTR(x) \
	(0xa0000800 + (x) * 0x1000)
#define DLB_CHP_DQED_FL_PUSH_PTR_RST 0x0
union dlb_chp_dqed_fl_push_ptr {
	struct {
		u32 push_ptr : 12;
		u32 reserved0 : 1;
		u32 generation : 1;
		u32 rsvd0 : 18;
	} field;
	u32 val;
};

#define DLB_CHP_CTRL_DIAG_02 0xa8000154
#define DLB_CHP_CTRL_DIAG_02_RST 0x0
union dlb_chp_ctrl_diag_02 {
	struct {
		u32 control : 32;
	} field;
	u32 val;
};

#define DLB_CHP_CFG_CHP_CSR_CTRL 0xa8000130
#define DLB_CHP_CFG_CHP_CSR_CTRL_RST 0xc0003fff
#define DLB_CHP_CFG_EXCESS_TOKENS_SHIFT 12
union dlb_chp_cfg_chp_csr_ctrl {
	struct {
		u32 int_inf_alarm_enable_0 : 1;
		u32 int_inf_alarm_enable_1 : 1;
		u32 int_inf_alarm_enable_2 : 1;
		u32 int_inf_alarm_enable_3 : 1;
		u32 int_inf_alarm_enable_4 : 1;
		u32 int_inf_alarm_enable_5 : 1;
		u32 int_inf_alarm_enable_6 : 1;
		u32 int_inf_alarm_enable_7 : 1;
		u32 int_inf_alarm_enable_8 : 1;
		u32 int_inf_alarm_enable_9 : 1;
		u32 int_inf_alarm_enable_10 : 1;
		u32 int_inf_alarm_enable_11 : 1;
		u32 int_inf_alarm_enable_12 : 1;
		u32 int_cor_alarm_enable : 1;
		u32 csr_control_spare : 14;
		u32 cfg_vasr_dis : 1;
		u32 counter_clear : 1;
		u32 blk_cor_report : 1;
		u32 blk_cor_synd : 1;
	} field;
	u32 val;
};

#define DLB_CHP_LDB_CQ_INTR_ARMED1 0xa8000068
#define DLB_CHP_LDB_CQ_INTR_ARMED1_RST 0x0
union dlb_chp_ldb_cq_intr_armed1 {
	struct {
		u32 armed : 32;
	} field;
	u32 val;
};

#define DLB_CHP_LDB_CQ_INTR_ARMED0 0xa8000064
#define DLB_CHP_LDB_CQ_INTR_ARMED0_RST 0x0
union dlb_chp_ldb_cq_intr_armed0 {
	struct {
		u32 armed : 32;
	} field;
	u32 val;
};

#define DLB_CHP_DIR_CQ_INTR_ARMED3 0xa8000024
#define DLB_CHP_DIR_CQ_INTR_ARMED3_RST 0x0
union dlb_chp_dir_cq_intr_armed3 {
	struct {
		u32 armed : 32;
	} field;
	u32 val;
};

#define DLB_CHP_DIR_CQ_INTR_ARMED2 0xa8000020
#define DLB_CHP_DIR_CQ_INTR_ARMED2_RST 0x0
union dlb_chp_dir_cq_intr_armed2 {
	struct {
		u32 armed : 32;
	} field;
	u32 val;
};

#define DLB_CHP_DIR_CQ_INTR_ARMED1 0xa800001c
#define DLB_CHP_DIR_CQ_INTR_ARMED1_RST 0x0
union dlb_chp_dir_cq_intr_armed1 {
	struct {
		u32 armed : 32;
	} field;
	u32 val;
};

#define DLB_CHP_DIR_CQ_INTR_ARMED0 0xa8000018
#define DLB_CHP_DIR_CQ_INTR_ARMED0_RST 0x0
union dlb_chp_dir_cq_intr_armed0 {
	struct {
		u32 armed : 32;
	} field;
	u32 val;
};

#define DLB_CFG_MSTR_DIAG_RESET_STS 0xb8000004
#define DLB_CFG_MSTR_DIAG_RESET_STS_RST 0x1ff
union dlb_cfg_mstr_diag_reset_sts {
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
		u32 rsvd1 : 6;
		u32 pf_reset_active : 1;
		u32 chp_vf_reset_done : 1;
		u32 rop_vf_reset_done : 1;
		u32 lsp_vf_reset_done : 1;
		u32 nalb_vf_reset_done : 1;
		u32 ap_vf_reset_done : 1;
		u32 dp_vf_reset_done : 1;
		u32 qed_vf_reset_done : 1;
		u32 dqed_vf_reset_done : 1;
		u32 aqed_vf_reset_done : 1;
		u32 rsvd0 : 6;
		u32 vf_reset_active : 1;
	} field;
	u32 val;
};

#define DLB_CFG_MSTR_BCAST_RESET_VF_START 0xc8100000
#define DLB_CFG_MSTR_BCAST_RESET_VF_START_RST 0x0
/* HW Reset Types */
#define VF_RST_TYPE_CQ_LDB   0
#define VF_RST_TYPE_QID_LDB  1
#define VF_RST_TYPE_POOL_LDB 2
#define VF_RST_TYPE_CQ_DIR   8
#define VF_RST_TYPE_QID_DIR  9
#define VF_RST_TYPE_POOL_DIR 10
union dlb_cfg_mstr_bcast_reset_vf_start {
	struct {
		u32 vf_reset_start : 1;
		u32 reserved : 3;
		u32 vf_reset_type : 4;
		u32 vf_reset_id : 24;
	} field;
	u32 val;
};

#endif /* __DLB_REGS_H */
