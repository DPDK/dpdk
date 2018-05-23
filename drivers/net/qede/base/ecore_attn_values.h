/*
 * Copyright (c) 2016 - 2018 Cavium Inc.
 * All rights reserved.
 * www.cavium.com
 *
 * See LICENSE.qede_pmd for copyright and licensing details.
 */

#ifndef __ATTN_VALUES_H__
#define __ATTN_VALUES_H__

#ifndef __PREVENT_INT_ATTN__

/* HW Attention register */
struct attn_hw_reg {
	u16 reg_idx;		/* Index of this register in its block */
	u16 num_of_bits;	/* number of valid attention bits */
	const u16 *bit_attn_idx;	/* attention index per valid bit */
	u32 sts_addr;		/* Address of the STS register */
	u32 sts_clr_addr;	/* Address of the STS_CLR register */
	u32 sts_wr_addr;	/* Address of the STS_WR register */
	u32 mask_addr;		/* Address of the MASK register */
};

/* HW block attention registers */
struct attn_hw_regs {
	u16 num_of_int_regs;	/* Number of interrupt regs */
	u16 num_of_prty_regs;	/* Number of parity regs */
	struct attn_hw_reg **int_regs;	/* interrupt regs */
	struct attn_hw_reg **prty_regs;	/* parity regs */
};

/* HW block attention registers */
struct attn_hw_block {
	const char *name;	/* Block name */
	const char **int_desc;	/* Array of interrupt attention descriptions */
	const char **prty_desc;	/* Array of parity attention descriptions */
	struct attn_hw_regs chip_regs[3];	/* attention regs per chip.*/
};

#ifdef ATTN_DESC
static const char *grc_int_attn_desc[5] = {
	"grc_address_error",
	"grc_timeout_event",
	"grc_global_reserved_address",
	"grc_path_isolation_error",
	"grc_trace_fifo_valid_data",
};
#else
#define grc_int_attn_desc OSAL_NULL
#endif

static const u16 grc_int0_bb_a0_attn_idx[4] = {
	0, 1, 2, 3,
};

static struct attn_hw_reg grc_int0_bb_a0 = {
	0, 4, grc_int0_bb_a0_attn_idx, 0x50180, 0x5018c, 0x50188, 0x50184
};

static struct attn_hw_reg *grc_int_bb_a0_regs[1] = {
	&grc_int0_bb_a0,
};

static const u16 grc_int0_bb_b0_attn_idx[4] = {
	0, 1, 2, 3,
};

static struct attn_hw_reg grc_int0_bb_b0 = {
	0, 4, grc_int0_bb_b0_attn_idx, 0x50180, 0x5018c, 0x50188, 0x50184
};

static struct attn_hw_reg *grc_int_bb_b0_regs[1] = {
	&grc_int0_bb_b0,
};

static const u16 grc_int0_k2_attn_idx[5] = {
	0, 1, 2, 3, 4,
};

static struct attn_hw_reg grc_int0_k2 = {
	0, 5, grc_int0_k2_attn_idx, 0x50180, 0x5018c, 0x50188, 0x50184
};

static struct attn_hw_reg *grc_int_k2_regs[1] = {
	&grc_int0_k2,
};

#ifdef ATTN_DESC
static const char *grc_prty_attn_desc[3] = {
	"grc_mem003_i_mem_prty",
	"grc_mem002_i_mem_prty",
	"grc_mem001_i_mem_prty",
};
#else
#define grc_prty_attn_desc OSAL_NULL
#endif

static const u16 grc_prty1_bb_a0_attn_idx[2] = {
	1, 2,
};

static struct attn_hw_reg grc_prty1_bb_a0 = {
	0, 2, grc_prty1_bb_a0_attn_idx, 0x50200, 0x5020c, 0x50208, 0x50204
};

static struct attn_hw_reg *grc_prty_bb_a0_regs[1] = {
	&grc_prty1_bb_a0,
};

static const u16 grc_prty1_bb_b0_attn_idx[2] = {
	0, 1,
};

static struct attn_hw_reg grc_prty1_bb_b0 = {
	0, 2, grc_prty1_bb_b0_attn_idx, 0x50200, 0x5020c, 0x50208, 0x50204
};

static struct attn_hw_reg *grc_prty_bb_b0_regs[1] = {
	&grc_prty1_bb_b0,
};

static const u16 grc_prty1_k2_attn_idx[2] = {
	0, 1,
};

static struct attn_hw_reg grc_prty1_k2 = {
	0, 2, grc_prty1_k2_attn_idx, 0x50200, 0x5020c, 0x50208, 0x50204
};

static struct attn_hw_reg *grc_prty_k2_regs[1] = {
	&grc_prty1_k2,
};

#ifdef ATTN_DESC
static const char *miscs_int_attn_desc[14] = {
	"miscs_address_error",
	"miscs_generic_sw",
	"miscs_cnig_interrupt",
	"miscs_opte_dorq_fifo_err_eng1",
	"miscs_opte_dorq_fifo_err_eng0",
	"miscs_opte_dbg_fifo_err_eng1",
	"miscs_opte_dbg_fifo_err_eng0",
	"miscs_opte_btb_if1_fifo_err_eng1",
	"miscs_opte_btb_if1_fifo_err_eng0",
	"miscs_opte_btb_if0_fifo_err_eng1",
	"miscs_opte_btb_if0_fifo_err_eng0",
	"miscs_opte_btb_sop_fifo_err_eng1",
	"miscs_opte_btb_sop_fifo_err_eng0",
	"miscs_opte_storm_fifo_err_eng0",
};
#else
#define miscs_int_attn_desc OSAL_NULL
#endif

static const u16 miscs_int0_bb_a0_attn_idx[2] = {
	0, 1,
};

static struct attn_hw_reg miscs_int0_bb_a0 = {
	0, 2, miscs_int0_bb_a0_attn_idx, 0x9180, 0x918c, 0x9188, 0x9184
};

static const u16 miscs_int1_bb_a0_attn_idx[11] = {
	3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
};

static struct attn_hw_reg miscs_int1_bb_a0 = {
	1, 11, miscs_int1_bb_a0_attn_idx, 0x9190, 0x919c, 0x9198, 0x9194
};

static struct attn_hw_reg *miscs_int_bb_a0_regs[2] = {
	&miscs_int0_bb_a0, &miscs_int1_bb_a0,
};

static const u16 miscs_int0_bb_b0_attn_idx[3] = {
	0, 1, 2,
};

static struct attn_hw_reg miscs_int0_bb_b0 = {
	0, 3, miscs_int0_bb_b0_attn_idx, 0x9180, 0x918c, 0x9188, 0x9184
};

static const u16 miscs_int1_bb_b0_attn_idx[11] = {
	3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
};

static struct attn_hw_reg miscs_int1_bb_b0 = {
	1, 11, miscs_int1_bb_b0_attn_idx, 0x9190, 0x919c, 0x9198, 0x9194
};

static struct attn_hw_reg *miscs_int_bb_b0_regs[2] = {
	&miscs_int0_bb_b0, &miscs_int1_bb_b0,
};

static const u16 miscs_int0_k2_attn_idx[3] = {
	0, 1, 2,
};

static struct attn_hw_reg miscs_int0_k2 = {
	0, 3, miscs_int0_k2_attn_idx, 0x9180, 0x918c, 0x9188, 0x9184
};

static struct attn_hw_reg *miscs_int_k2_regs[1] = {
	&miscs_int0_k2,
};

#ifdef ATTN_DESC
static const char *miscs_prty_attn_desc[1] = {
	"miscs_cnig_parity",
};
#else
#define miscs_prty_attn_desc OSAL_NULL
#endif

static const u16 miscs_prty0_bb_b0_attn_idx[1] = {
	0,
};

static struct attn_hw_reg miscs_prty0_bb_b0 = {
	0, 1, miscs_prty0_bb_b0_attn_idx, 0x91a0, 0x91ac, 0x91a8, 0x91a4
};

static struct attn_hw_reg *miscs_prty_bb_b0_regs[1] = {
	&miscs_prty0_bb_b0,
};

static const u16 miscs_prty0_k2_attn_idx[1] = {
	0,
};

static struct attn_hw_reg miscs_prty0_k2 = {
	0, 1, miscs_prty0_k2_attn_idx, 0x91a0, 0x91ac, 0x91a8, 0x91a4
};

static struct attn_hw_reg *miscs_prty_k2_regs[1] = {
	&miscs_prty0_k2,
};

#ifdef ATTN_DESC
static const char *misc_int_attn_desc[1] = {
	"misc_address_error",
};
#else
#define misc_int_attn_desc OSAL_NULL
#endif

static const u16 misc_int0_bb_a0_attn_idx[1] = {
	0,
};

static struct attn_hw_reg misc_int0_bb_a0 = {
	0, 1, misc_int0_bb_a0_attn_idx, 0x8180, 0x818c, 0x8188, 0x8184
};

static struct attn_hw_reg *misc_int_bb_a0_regs[1] = {
	&misc_int0_bb_a0,
};

static const u16 misc_int0_bb_b0_attn_idx[1] = {
	0,
};

static struct attn_hw_reg misc_int0_bb_b0 = {
	0, 1, misc_int0_bb_b0_attn_idx, 0x8180, 0x818c, 0x8188, 0x8184
};

static struct attn_hw_reg *misc_int_bb_b0_regs[1] = {
	&misc_int0_bb_b0,
};

static const u16 misc_int0_k2_attn_idx[1] = {
	0,
};

static struct attn_hw_reg misc_int0_k2 = {
	0, 1, misc_int0_k2_attn_idx, 0x8180, 0x818c, 0x8188, 0x8184
};

static struct attn_hw_reg *misc_int_k2_regs[1] = {
	&misc_int0_k2,
};

#ifdef ATTN_DESC
static const char *pglue_b_int_attn_desc[24] = {
	"pglue_b_address_error",
	"pglue_b_incorrect_rcv_behavior",
	"pglue_b_was_error_attn",
	"pglue_b_vf_length_violation_attn",
	"pglue_b_vf_grc_space_violation_attn",
	"pglue_b_tcpl_error_attn",
	"pglue_b_tcpl_in_two_rcbs_attn",
	"pglue_b_cssnoop_fifo_overflow",
	"pglue_b_tcpl_translation_size_different",
	"pglue_b_pcie_rx_l0s_timeout",
	"pglue_b_master_zlr_attn",
	"pglue_b_admin_window_violation_attn",
	"pglue_b_out_of_range_function_in_pretend",
	"pglue_b_illegal_address",
	"pglue_b_pgl_cpl_err",
	"pglue_b_pgl_txw_of",
	"pglue_b_pgl_cpl_aft",
	"pglue_b_pgl_cpl_of",
	"pglue_b_pgl_cpl_ecrc",
	"pglue_b_pgl_pcie_attn",
	"pglue_b_pgl_read_blocked",
	"pglue_b_pgl_write_blocked",
	"pglue_b_vf_ilt_err",
	"pglue_b_rxobffexception_attn",
};
#else
#define pglue_b_int_attn_desc OSAL_NULL
#endif

static const u16 pglue_b_int0_bb_a0_attn_idx[23] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22,
};

static struct attn_hw_reg pglue_b_int0_bb_a0 = {
	0, 23, pglue_b_int0_bb_a0_attn_idx, 0x2a8180, 0x2a818c, 0x2a8188,
	0x2a8184
};

static struct attn_hw_reg *pglue_b_int_bb_a0_regs[1] = {
	&pglue_b_int0_bb_a0,
};

static const u16 pglue_b_int0_bb_b0_attn_idx[23] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22,
};

static struct attn_hw_reg pglue_b_int0_bb_b0 = {
	0, 23, pglue_b_int0_bb_b0_attn_idx, 0x2a8180, 0x2a818c, 0x2a8188,
	0x2a8184
};

static struct attn_hw_reg *pglue_b_int_bb_b0_regs[1] = {
	&pglue_b_int0_bb_b0,
};

static const u16 pglue_b_int0_k2_attn_idx[24] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23,
};

static struct attn_hw_reg pglue_b_int0_k2 = {
	0, 24, pglue_b_int0_k2_attn_idx, 0x2a8180, 0x2a818c, 0x2a8188, 0x2a8184
};

static struct attn_hw_reg *pglue_b_int_k2_regs[1] = {
	&pglue_b_int0_k2,
};

#ifdef ATTN_DESC
static const char *pglue_b_prty_attn_desc[35] = {
	"pglue_b_datapath_registers",
	"pglue_b_mem027_i_mem_prty",
	"pglue_b_mem007_i_mem_prty",
	"pglue_b_mem009_i_mem_prty",
	"pglue_b_mem010_i_mem_prty",
	"pglue_b_mem008_i_mem_prty",
	"pglue_b_mem022_i_mem_prty",
	"pglue_b_mem023_i_mem_prty",
	"pglue_b_mem024_i_mem_prty",
	"pglue_b_mem025_i_mem_prty",
	"pglue_b_mem004_i_mem_prty",
	"pglue_b_mem005_i_mem_prty",
	"pglue_b_mem011_i_mem_prty",
	"pglue_b_mem016_i_mem_prty",
	"pglue_b_mem017_i_mem_prty",
	"pglue_b_mem012_i_mem_prty",
	"pglue_b_mem013_i_mem_prty",
	"pglue_b_mem014_i_mem_prty",
	"pglue_b_mem015_i_mem_prty",
	"pglue_b_mem018_i_mem_prty",
	"pglue_b_mem020_i_mem_prty",
	"pglue_b_mem021_i_mem_prty",
	"pglue_b_mem019_i_mem_prty",
	"pglue_b_mem026_i_mem_prty",
	"pglue_b_mem006_i_mem_prty",
	"pglue_b_mem003_i_mem_prty",
	"pglue_b_mem002_i_mem_prty_0",
	"pglue_b_mem002_i_mem_prty_1",
	"pglue_b_mem002_i_mem_prty_2",
	"pglue_b_mem002_i_mem_prty_3",
	"pglue_b_mem002_i_mem_prty_4",
	"pglue_b_mem002_i_mem_prty_5",
	"pglue_b_mem002_i_mem_prty_6",
	"pglue_b_mem002_i_mem_prty_7",
	"pglue_b_mem001_i_mem_prty",
};
#else
#define pglue_b_prty_attn_desc OSAL_NULL
#endif

static const u16 pglue_b_prty1_bb_a0_attn_idx[22] = {
	2, 3, 4, 5, 10, 11, 12, 15, 16, 17, 18, 24, 25, 26, 27, 28, 29, 30, 31,
	32, 33, 34,
};

static struct attn_hw_reg pglue_b_prty1_bb_a0 = {
	0, 22, pglue_b_prty1_bb_a0_attn_idx, 0x2a8200, 0x2a820c, 0x2a8208,
	0x2a8204
};

static struct attn_hw_reg *pglue_b_prty_bb_a0_regs[1] = {
	&pglue_b_prty1_bb_a0,
};

static const u16 pglue_b_prty0_bb_b0_attn_idx[1] = {
	0,
};

static struct attn_hw_reg pglue_b_prty0_bb_b0 = {
	0, 1, pglue_b_prty0_bb_b0_attn_idx, 0x2a8190, 0x2a819c, 0x2a8198,
	0x2a8194
};

static const u16 pglue_b_prty1_bb_b0_attn_idx[22] = {
	2, 3, 4, 5, 10, 11, 12, 15, 16, 17, 18, 24, 25, 26, 27, 28, 29, 30, 31,
	32, 33, 34,
};

static struct attn_hw_reg pglue_b_prty1_bb_b0 = {
	1, 22, pglue_b_prty1_bb_b0_attn_idx, 0x2a8200, 0x2a820c, 0x2a8208,
	0x2a8204
};

static struct attn_hw_reg *pglue_b_prty_bb_b0_regs[2] = {
	&pglue_b_prty0_bb_b0, &pglue_b_prty1_bb_b0,
};

static const u16 pglue_b_prty0_k2_attn_idx[1] = {
	0,
};

static struct attn_hw_reg pglue_b_prty0_k2 = {
	0, 1, pglue_b_prty0_k2_attn_idx, 0x2a8190, 0x2a819c, 0x2a8198, 0x2a8194
};

static const u16 pglue_b_prty1_k2_attn_idx[31] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
	21,
	22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
};

static struct attn_hw_reg pglue_b_prty1_k2 = {
	1, 31, pglue_b_prty1_k2_attn_idx, 0x2a8200, 0x2a820c, 0x2a8208,
	0x2a8204
};

static const u16 pglue_b_prty2_k2_attn_idx[3] = {
	32, 33, 34,
};

static struct attn_hw_reg pglue_b_prty2_k2 = {
	2, 3, pglue_b_prty2_k2_attn_idx, 0x2a8210, 0x2a821c, 0x2a8218, 0x2a8214
};

static struct attn_hw_reg *pglue_b_prty_k2_regs[3] = {
	&pglue_b_prty0_k2, &pglue_b_prty1_k2, &pglue_b_prty2_k2,
};

#ifdef ATTN_DESC
static const char *cnig_int_attn_desc[10] = {
	"cnig_address_error",
	"cnig_tx_illegal_sop_port0",
	"cnig_tx_illegal_sop_port1",
	"cnig_tx_illegal_sop_port2",
	"cnig_tx_illegal_sop_port3",
	"cnig_tdm_lane_0_bandwidth_exceed",
	"cnig_tdm_lane_1_bandwidth_exceed",
	"cnig_pmeg_intr",
	"cnig_pmfc_intr",
	"cnig_fifo_error",
};
#else
#define cnig_int_attn_desc OSAL_NULL
#endif

static const u16 cnig_int0_bb_a0_attn_idx[4] = {
	0, 7, 8, 9,
};

static struct attn_hw_reg cnig_int0_bb_a0 = {
	0, 4, cnig_int0_bb_a0_attn_idx, 0x2182e8, 0x2182f4, 0x2182f0, 0x2182ec
};

static struct attn_hw_reg *cnig_int_bb_a0_regs[1] = {
	&cnig_int0_bb_a0,
};

static const u16 cnig_int0_bb_b0_attn_idx[6] = {
	0, 1, 3, 7, 8, 9,
};

static struct attn_hw_reg cnig_int0_bb_b0 = {
	0, 6, cnig_int0_bb_b0_attn_idx, 0x2182e8, 0x2182f4, 0x2182f0, 0x2182ec
};

static struct attn_hw_reg *cnig_int_bb_b0_regs[1] = {
	&cnig_int0_bb_b0,
};

static const u16 cnig_int0_k2_attn_idx[7] = {
	0, 1, 2, 3, 4, 5, 6,
};

static struct attn_hw_reg cnig_int0_k2 = {
	0, 7, cnig_int0_k2_attn_idx, 0x218218, 0x218224, 0x218220, 0x21821c
};

static struct attn_hw_reg *cnig_int_k2_regs[1] = {
	&cnig_int0_k2,
};

#ifdef ATTN_DESC
static const char *cnig_prty_attn_desc[3] = {
	"cnig_unused_0",
	"cnig_datapath_tx",
	"cnig_datapath_rx",
};
#else
#define cnig_prty_attn_desc OSAL_NULL
#endif

static const u16 cnig_prty0_bb_b0_attn_idx[2] = {
	1, 2,
};

static struct attn_hw_reg cnig_prty0_bb_b0 = {
	0, 2, cnig_prty0_bb_b0_attn_idx, 0x218348, 0x218354, 0x218350, 0x21834c
};

static struct attn_hw_reg *cnig_prty_bb_b0_regs[1] = {
	&cnig_prty0_bb_b0,
};

static const u16 cnig_prty0_k2_attn_idx[1] = {
	1,
};

static struct attn_hw_reg cnig_prty0_k2 = {
	0, 1, cnig_prty0_k2_attn_idx, 0x21822c, 0x218238, 0x218234, 0x218230
};

static struct attn_hw_reg *cnig_prty_k2_regs[1] = {
	&cnig_prty0_k2,
};

#ifdef ATTN_DESC
static const char *cpmu_int_attn_desc[1] = {
	"cpmu_address_error",
};
#else
#define cpmu_int_attn_desc OSAL_NULL
#endif

static const u16 cpmu_int0_bb_a0_attn_idx[1] = {
	0,
};

static struct attn_hw_reg cpmu_int0_bb_a0 = {
	0, 1, cpmu_int0_bb_a0_attn_idx, 0x303e0, 0x303ec, 0x303e8, 0x303e4
};

static struct attn_hw_reg *cpmu_int_bb_a0_regs[1] = {
	&cpmu_int0_bb_a0,
};

static const u16 cpmu_int0_bb_b0_attn_idx[1] = {
	0,
};

static struct attn_hw_reg cpmu_int0_bb_b0 = {
	0, 1, cpmu_int0_bb_b0_attn_idx, 0x303e0, 0x303ec, 0x303e8, 0x303e4
};

static struct attn_hw_reg *cpmu_int_bb_b0_regs[1] = {
	&cpmu_int0_bb_b0,
};

static const u16 cpmu_int0_k2_attn_idx[1] = {
	0,
};

static struct attn_hw_reg cpmu_int0_k2 = {
	0, 1, cpmu_int0_k2_attn_idx, 0x303e0, 0x303ec, 0x303e8, 0x303e4
};

static struct attn_hw_reg *cpmu_int_k2_regs[1] = {
	&cpmu_int0_k2,
};

#ifdef ATTN_DESC
static const char *ncsi_int_attn_desc[1] = {
	"ncsi_address_error",
};
#else
#define ncsi_int_attn_desc OSAL_NULL
#endif

static const u16 ncsi_int0_bb_a0_attn_idx[1] = {
	0,
};

static struct attn_hw_reg ncsi_int0_bb_a0 = {
	0, 1, ncsi_int0_bb_a0_attn_idx, 0x404cc, 0x404d8, 0x404d4, 0x404d0
};

static struct attn_hw_reg *ncsi_int_bb_a0_regs[1] = {
	&ncsi_int0_bb_a0,
};

static const u16 ncsi_int0_bb_b0_attn_idx[1] = {
	0,
};

static struct attn_hw_reg ncsi_int0_bb_b0 = {
	0, 1, ncsi_int0_bb_b0_attn_idx, 0x404cc, 0x404d8, 0x404d4, 0x404d0
};

static struct attn_hw_reg *ncsi_int_bb_b0_regs[1] = {
	&ncsi_int0_bb_b0,
};

static const u16 ncsi_int0_k2_attn_idx[1] = {
	0,
};

static struct attn_hw_reg ncsi_int0_k2 = {
	0, 1, ncsi_int0_k2_attn_idx, 0x404cc, 0x404d8, 0x404d4, 0x404d0
};

static struct attn_hw_reg *ncsi_int_k2_regs[1] = {
	&ncsi_int0_k2,
};

#ifdef ATTN_DESC
static const char *ncsi_prty_attn_desc[1] = {
	"ncsi_mem002_i_mem_prty",
};
#else
#define ncsi_prty_attn_desc OSAL_NULL
#endif

static const u16 ncsi_prty1_bb_a0_attn_idx[1] = {
	0,
};

static struct attn_hw_reg ncsi_prty1_bb_a0 = {
	0, 1, ncsi_prty1_bb_a0_attn_idx, 0x40000, 0x4000c, 0x40008, 0x40004
};

static struct attn_hw_reg *ncsi_prty_bb_a0_regs[1] = {
	&ncsi_prty1_bb_a0,
};

static const u16 ncsi_prty1_bb_b0_attn_idx[1] = {
	0,
};

static struct attn_hw_reg ncsi_prty1_bb_b0 = {
	0, 1, ncsi_prty1_bb_b0_attn_idx, 0x40000, 0x4000c, 0x40008, 0x40004
};

static struct attn_hw_reg *ncsi_prty_bb_b0_regs[1] = {
	&ncsi_prty1_bb_b0,
};

static const u16 ncsi_prty1_k2_attn_idx[1] = {
	0,
};

static struct attn_hw_reg ncsi_prty1_k2 = {
	0, 1, ncsi_prty1_k2_attn_idx, 0x40000, 0x4000c, 0x40008, 0x40004
};

static struct attn_hw_reg *ncsi_prty_k2_regs[1] = {
	&ncsi_prty1_k2,
};

#ifdef ATTN_DESC
static const char *opte_prty_attn_desc[12] = {
	"opte_mem009_i_mem_prty",
	"opte_mem010_i_mem_prty",
	"opte_mem005_i_mem_prty",
	"opte_mem006_i_mem_prty",
	"opte_mem007_i_mem_prty",
	"opte_mem008_i_mem_prty",
	"opte_mem001_i_mem_prty",
	"opte_mem002_i_mem_prty",
	"opte_mem003_i_mem_prty",
	"opte_mem004_i_mem_prty",
	"opte_mem011_i_mem_prty",
	"opte_datapath_parity_error",
};
#else
#define opte_prty_attn_desc OSAL_NULL
#endif

static const u16 opte_prty1_bb_a0_attn_idx[11] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
};

static struct attn_hw_reg opte_prty1_bb_a0 = {
	0, 11, opte_prty1_bb_a0_attn_idx, 0x53000, 0x5300c, 0x53008, 0x53004
};

static struct attn_hw_reg *opte_prty_bb_a0_regs[1] = {
	&opte_prty1_bb_a0,
};

static const u16 opte_prty1_bb_b0_attn_idx[11] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
};

static struct attn_hw_reg opte_prty1_bb_b0 = {
	0, 11, opte_prty1_bb_b0_attn_idx, 0x53000, 0x5300c, 0x53008, 0x53004
};

static const u16 opte_prty0_bb_b0_attn_idx[1] = {
	11,
};

static struct attn_hw_reg opte_prty0_bb_b0 = {
	1, 1, opte_prty0_bb_b0_attn_idx, 0x53208, 0x53214, 0x53210, 0x5320c
};

static struct attn_hw_reg *opte_prty_bb_b0_regs[2] = {
	&opte_prty1_bb_b0, &opte_prty0_bb_b0,
};

static const u16 opte_prty1_k2_attn_idx[11] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
};

static struct attn_hw_reg opte_prty1_k2 = {
	0, 11, opte_prty1_k2_attn_idx, 0x53000, 0x5300c, 0x53008, 0x53004
};

static const u16 opte_prty0_k2_attn_idx[1] = {
	11,
};

static struct attn_hw_reg opte_prty0_k2 = {
	1, 1, opte_prty0_k2_attn_idx, 0x53208, 0x53214, 0x53210, 0x5320c
};

static struct attn_hw_reg *opte_prty_k2_regs[2] = {
	&opte_prty1_k2, &opte_prty0_k2,
};

#ifdef ATTN_DESC
static const char *bmb_int_attn_desc[297] = {
	"bmb_address_error",
	"bmb_rc_pkt0_rls_error",
	"bmb_unused_0",
	"bmb_rc_pkt0_protocol_error",
	"bmb_rc_pkt1_rls_error",
	"bmb_unused_1",
	"bmb_rc_pkt1_protocol_error",
	"bmb_rc_pkt2_rls_error",
	"bmb_unused_2",
	"bmb_rc_pkt2_protocol_error",
	"bmb_rc_pkt3_rls_error",
	"bmb_unused_3",
	"bmb_rc_pkt3_protocol_error",
	"bmb_rc_sop_req_tc_port_error",
	"bmb_unused_4",
	"bmb_wc0_protocol_error",
	"bmb_wc1_protocol_error",
	"bmb_wc2_protocol_error",
	"bmb_wc3_protocol_error",
	"bmb_unused_5",
	"bmb_ll_blk_error",
	"bmb_unused_6",
	"bmb_mac0_fc_cnt_error",
	"bmb_ll_arb_calc_error",
	"bmb_wc0_inp_fifo_error",
	"bmb_wc0_sop_fifo_error",
	"bmb_wc0_len_fifo_error",
	"bmb_wc0_queue_fifo_error",
	"bmb_wc0_free_point_fifo_error",
	"bmb_wc0_next_point_fifo_error",
	"bmb_wc0_strt_fifo_error",
	"bmb_wc0_second_dscr_fifo_error",
	"bmb_wc0_pkt_avail_fifo_error",
	"bmb_wc0_cos_cnt_fifo_error",
	"bmb_wc0_notify_fifo_error",
	"bmb_wc0_ll_req_fifo_error",
	"bmb_wc0_ll_pa_cnt_error",
	"bmb_wc0_bb_pa_cnt_error",
	"bmb_wc1_inp_fifo_error",
	"bmb_wc1_sop_fifo_error",
	"bmb_wc1_queue_fifo_error",
	"bmb_wc1_free_point_fifo_error",
	"bmb_wc1_next_point_fifo_error",
	"bmb_wc1_strt_fifo_error",
	"bmb_wc1_second_dscr_fifo_error",
	"bmb_wc1_pkt_avail_fifo_error",
	"bmb_wc1_cos_cnt_fifo_error",
	"bmb_wc1_notify_fifo_error",
	"bmb_wc1_ll_req_fifo_error",
	"bmb_wc1_ll_pa_cnt_error",
	"bmb_wc1_bb_pa_cnt_error",
	"bmb_wc2_inp_fifo_error",
	"bmb_wc2_sop_fifo_error",
	"bmb_wc2_queue_fifo_error",
	"bmb_wc2_free_point_fifo_error",
	"bmb_wc2_next_point_fifo_error",
	"bmb_wc2_strt_fifo_error",
	"bmb_wc2_second_dscr_fifo_error",
	"bmb_wc2_pkt_avail_fifo_error",
	"bmb_wc2_cos_cnt_fifo_error",
	"bmb_wc2_notify_fifo_error",
	"bmb_wc2_ll_req_fifo_error",
	"bmb_wc2_ll_pa_cnt_error",
	"bmb_wc2_bb_pa_cnt_error",
	"bmb_wc3_inp_fifo_error",
	"bmb_wc3_sop_fifo_error",
	"bmb_wc3_queue_fifo_error",
	"bmb_wc3_free_point_fifo_error",
	"bmb_wc3_next_point_fifo_error",
	"bmb_wc3_strt_fifo_error",
	"bmb_wc3_second_dscr_fifo_error",
	"bmb_wc3_pkt_avail_fifo_error",
	"bmb_wc3_cos_cnt_fifo_error",
	"bmb_wc3_notify_fifo_error",
	"bmb_wc3_ll_req_fifo_error",
	"bmb_wc3_ll_pa_cnt_error",
	"bmb_wc3_bb_pa_cnt_error",
	"bmb_rc_pkt0_side_fifo_error",
	"bmb_rc_pkt0_req_fifo_error",
	"bmb_rc_pkt0_blk_fifo_error",
	"bmb_rc_pkt0_rls_left_fifo_error",
	"bmb_rc_pkt0_strt_ptr_fifo_error",
	"bmb_rc_pkt0_second_ptr_fifo_error",
	"bmb_rc_pkt0_rsp_fifo_error",
	"bmb_rc_pkt0_dscr_fifo_error",
	"bmb_rc_pkt1_side_fifo_error",
	"bmb_rc_pkt1_req_fifo_error",
	"bmb_rc_pkt1_blk_fifo_error",
	"bmb_rc_pkt1_rls_left_fifo_error",
	"bmb_rc_pkt1_strt_ptr_fifo_error",
	"bmb_rc_pkt1_second_ptr_fifo_error",
	"bmb_rc_pkt1_rsp_fifo_error",
	"bmb_rc_pkt1_dscr_fifo_error",
	"bmb_rc_pkt2_side_fifo_error",
	"bmb_rc_pkt2_req_fifo_error",
	"bmb_rc_pkt2_blk_fifo_error",
	"bmb_rc_pkt2_rls_left_fifo_error",
	"bmb_rc_pkt2_strt_ptr_fifo_error",
	"bmb_rc_pkt2_second_ptr_fifo_error",
	"bmb_rc_pkt2_rsp_fifo_error",
	"bmb_rc_pkt2_dscr_fifo_error",
	"bmb_rc_pkt3_side_fifo_error",
	"bmb_rc_pkt3_req_fifo_error",
	"bmb_rc_pkt3_blk_fifo_error",
	"bmb_rc_pkt3_rls_left_fifo_error",
	"bmb_rc_pkt3_strt_ptr_fifo_error",
	"bmb_rc_pkt3_second_ptr_fifo_error",
	"bmb_rc_pkt3_rsp_fifo_error",
	"bmb_rc_pkt3_dscr_fifo_error",
	"bmb_rc_sop_strt_fifo_error",
	"bmb_rc_sop_req_fifo_error",
	"bmb_rc_sop_dscr_fifo_error",
	"bmb_rc_sop_queue_fifo_error",
	"bmb_ll_arb_rls_fifo_error",
	"bmb_ll_arb_prefetch_fifo_error",
	"bmb_rc_pkt0_rls_fifo_error",
	"bmb_rc_pkt1_rls_fifo_error",
	"bmb_rc_pkt2_rls_fifo_error",
	"bmb_rc_pkt3_rls_fifo_error",
	"bmb_rc_pkt4_rls_fifo_error",
	"bmb_rc_pkt5_rls_fifo_error",
	"bmb_rc_pkt6_rls_fifo_error",
	"bmb_rc_pkt7_rls_fifo_error",
	"bmb_rc_pkt8_rls_fifo_error",
	"bmb_rc_pkt9_rls_fifo_error",
	"bmb_rc_pkt4_rls_error",
	"bmb_rc_pkt4_protocol_error",
	"bmb_rc_pkt4_side_fifo_error",
	"bmb_rc_pkt4_req_fifo_error",
	"bmb_rc_pkt4_blk_fifo_error",
	"bmb_rc_pkt4_rls_left_fifo_error",
	"bmb_rc_pkt4_strt_ptr_fifo_error",
	"bmb_rc_pkt4_second_ptr_fifo_error",
	"bmb_rc_pkt4_rsp_fifo_error",
	"bmb_rc_pkt4_dscr_fifo_error",
	"bmb_rc_pkt5_rls_error",
	"bmb_rc_pkt5_protocol_error",
	"bmb_rc_pkt5_side_fifo_error",
	"bmb_rc_pkt5_req_fifo_error",
	"bmb_rc_pkt5_blk_fifo_error",
	"bmb_rc_pkt5_rls_left_fifo_error",
	"bmb_rc_pkt5_strt_ptr_fifo_error",
	"bmb_rc_pkt5_second_ptr_fifo_error",
	"bmb_rc_pkt5_rsp_fifo_error",
	"bmb_rc_pkt5_dscr_fifo_error",
	"bmb_rc_pkt6_rls_error",
	"bmb_rc_pkt6_protocol_error",
	"bmb_rc_pkt6_side_fifo_error",
	"bmb_rc_pkt6_req_fifo_error",
	"bmb_rc_pkt6_blk_fifo_error",
	"bmb_rc_pkt6_rls_left_fifo_error",
	"bmb_rc_pkt6_strt_ptr_fifo_error",
	"bmb_rc_pkt6_second_ptr_fifo_error",
	"bmb_rc_pkt6_rsp_fifo_error",
	"bmb_rc_pkt6_dscr_fifo_error",
	"bmb_rc_pkt7_rls_error",
	"bmb_rc_pkt7_protocol_error",
	"bmb_rc_pkt7_side_fifo_error",
	"bmb_rc_pkt7_req_fifo_error",
	"bmb_rc_pkt7_blk_fifo_error",
	"bmb_rc_pkt7_rls_left_fifo_error",
	"bmb_rc_pkt7_strt_ptr_fifo_error",
	"bmb_rc_pkt7_second_ptr_fifo_error",
	"bmb_rc_pkt7_rsp_fifo_error",
	"bmb_packet_available_sync_fifo_push_error",
	"bmb_rc_pkt8_rls_error",
	"bmb_rc_pkt8_protocol_error",
	"bmb_rc_pkt8_side_fifo_error",
	"bmb_rc_pkt8_req_fifo_error",
	"bmb_rc_pkt8_blk_fifo_error",
	"bmb_rc_pkt8_rls_left_fifo_error",
	"bmb_rc_pkt8_strt_ptr_fifo_error",
	"bmb_rc_pkt8_second_ptr_fifo_error",
	"bmb_rc_pkt8_rsp_fifo_error",
	"bmb_rc_pkt8_dscr_fifo_error",
	"bmb_rc_pkt9_rls_error",
	"bmb_rc_pkt9_protocol_error",
	"bmb_rc_pkt9_side_fifo_error",
	"bmb_rc_pkt9_req_fifo_error",
	"bmb_rc_pkt9_blk_fifo_error",
	"bmb_rc_pkt9_rls_left_fifo_error",
	"bmb_rc_pkt9_strt_ptr_fifo_error",
	"bmb_rc_pkt9_second_ptr_fifo_error",
	"bmb_rc_pkt9_rsp_fifo_error",
	"bmb_rc_pkt9_dscr_fifo_error",
	"bmb_wc4_protocol_error",
	"bmb_wc5_protocol_error",
	"bmb_wc6_protocol_error",
	"bmb_wc7_protocol_error",
	"bmb_wc8_protocol_error",
	"bmb_wc9_protocol_error",
	"bmb_wc4_inp_fifo_error",
	"bmb_wc4_sop_fifo_error",
	"bmb_wc4_queue_fifo_error",
	"bmb_wc4_free_point_fifo_error",
	"bmb_wc4_next_point_fifo_error",
	"bmb_wc4_strt_fifo_error",
	"bmb_wc4_second_dscr_fifo_error",
	"bmb_wc4_pkt_avail_fifo_error",
	"bmb_wc4_cos_cnt_fifo_error",
	"bmb_wc4_notify_fifo_error",
	"bmb_wc4_ll_req_fifo_error",
	"bmb_wc4_ll_pa_cnt_error",
	"bmb_wc4_bb_pa_cnt_error",
	"bmb_wc5_inp_fifo_error",
	"bmb_wc5_sop_fifo_error",
	"bmb_wc5_queue_fifo_error",
	"bmb_wc5_free_point_fifo_error",
	"bmb_wc5_next_point_fifo_error",
	"bmb_wc5_strt_fifo_error",
	"bmb_wc5_second_dscr_fifo_error",
	"bmb_wc5_pkt_avail_fifo_error",
	"bmb_wc5_cos_cnt_fifo_error",
	"bmb_wc5_notify_fifo_error",
	"bmb_wc5_ll_req_fifo_error",
	"bmb_wc5_ll_pa_cnt_error",
	"bmb_wc5_bb_pa_cnt_error",
	"bmb_wc6_inp_fifo_error",
	"bmb_wc6_sop_fifo_error",
	"bmb_wc6_queue_fifo_error",
	"bmb_wc6_free_point_fifo_error",
	"bmb_wc6_next_point_fifo_error",
	"bmb_wc6_strt_fifo_error",
	"bmb_wc6_second_dscr_fifo_error",
	"bmb_wc6_pkt_avail_fifo_error",
	"bmb_wc6_cos_cnt_fifo_error",
	"bmb_wc6_notify_fifo_error",
	"bmb_wc6_ll_req_fifo_error",
	"bmb_wc6_ll_pa_cnt_error",
	"bmb_wc6_bb_pa_cnt_error",
	"bmb_wc7_inp_fifo_error",
	"bmb_wc7_sop_fifo_error",
	"bmb_wc7_queue_fifo_error",
	"bmb_wc7_free_point_fifo_error",
	"bmb_wc7_next_point_fifo_error",
	"bmb_wc7_strt_fifo_error",
	"bmb_wc7_second_dscr_fifo_error",
	"bmb_wc7_pkt_avail_fifo_error",
	"bmb_wc7_cos_cnt_fifo_error",
	"bmb_wc7_notify_fifo_error",
	"bmb_wc7_ll_req_fifo_error",
	"bmb_wc7_ll_pa_cnt_error",
	"bmb_wc7_bb_pa_cnt_error",
	"bmb_wc8_inp_fifo_error",
	"bmb_wc8_sop_fifo_error",
	"bmb_wc8_queue_fifo_error",
	"bmb_wc8_free_point_fifo_error",
	"bmb_wc8_next_point_fifo_error",
	"bmb_wc8_strt_fifo_error",
	"bmb_wc8_second_dscr_fifo_error",
	"bmb_wc8_pkt_avail_fifo_error",
	"bmb_wc8_cos_cnt_fifo_error",
	"bmb_wc8_notify_fifo_error",
	"bmb_wc8_ll_req_fifo_error",
	"bmb_wc8_ll_pa_cnt_error",
	"bmb_wc8_bb_pa_cnt_error",
	"bmb_wc9_inp_fifo_error",
	"bmb_wc9_sop_fifo_error",
	"bmb_wc9_queue_fifo_error",
	"bmb_wc9_free_point_fifo_error",
	"bmb_wc9_next_point_fifo_error",
	"bmb_wc9_strt_fifo_error",
	"bmb_wc9_second_dscr_fifo_error",
	"bmb_wc9_pkt_avail_fifo_error",
	"bmb_wc9_cos_cnt_fifo_error",
	"bmb_wc9_notify_fifo_error",
	"bmb_wc9_ll_req_fifo_error",
	"bmb_wc9_ll_pa_cnt_error",
	"bmb_wc9_bb_pa_cnt_error",
	"bmb_rc9_sop_rc_out_sync_fifo_error",
	"bmb_rc9_sop_out_sync_fifo_push_error",
	"bmb_rc0_sop_pend_fifo_error",
	"bmb_rc1_sop_pend_fifo_error",
	"bmb_rc2_sop_pend_fifo_error",
	"bmb_rc3_sop_pend_fifo_error",
	"bmb_rc4_sop_pend_fifo_error",
	"bmb_rc5_sop_pend_fifo_error",
	"bmb_rc6_sop_pend_fifo_error",
	"bmb_rc7_sop_pend_fifo_error",
	"bmb_rc0_dscr_pend_fifo_error",
	"bmb_rc1_dscr_pend_fifo_error",
	"bmb_rc2_dscr_pend_fifo_error",
	"bmb_rc3_dscr_pend_fifo_error",
	"bmb_rc4_dscr_pend_fifo_error",
	"bmb_rc5_dscr_pend_fifo_error",
	"bmb_rc6_dscr_pend_fifo_error",
	"bmb_rc7_dscr_pend_fifo_error",
	"bmb_rc8_sop_inp_sync_fifo_push_error",
	"bmb_rc9_sop_inp_sync_fifo_push_error",
	"bmb_rc8_sop_out_sync_fifo_push_error",
	"bmb_rc_gnt_pend_fifo_error",
	"bmb_rc8_out_sync_fifo_push_error",
	"bmb_rc9_out_sync_fifo_push_error",
	"bmb_wc8_sync_fifo_push_error",
	"bmb_wc9_sync_fifo_push_error",
	"bmb_rc8_sop_rc_out_sync_fifo_error",
	"bmb_rc_pkt7_dscr_fifo_error",
};
#else
#define bmb_int_attn_desc OSAL_NULL
#endif

static const u16 bmb_int0_bb_a0_attn_idx[16] = {
	0, 1, 3, 4, 6, 7, 9, 10, 12, 13, 15, 16, 17, 18, 20, 22,
};

static struct attn_hw_reg bmb_int0_bb_a0 = {
	0, 16, bmb_int0_bb_a0_attn_idx, 0x5400c0, 0x5400cc, 0x5400c8, 0x5400c4
};

static const u16 bmb_int1_bb_a0_attn_idx[28] = {
	23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50,
};

static struct attn_hw_reg bmb_int1_bb_a0 = {
	1, 28, bmb_int1_bb_a0_attn_idx, 0x5400d8, 0x5400e4, 0x5400e0, 0x5400dc
};

static const u16 bmb_int2_bb_a0_attn_idx[26] = {
	51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68,
	69, 70, 71, 72, 73, 74, 75, 76,
};

static struct attn_hw_reg bmb_int2_bb_a0 = {
	2, 26, bmb_int2_bb_a0_attn_idx, 0x5400f0, 0x5400fc, 0x5400f8, 0x5400f4
};

static const u16 bmb_int3_bb_a0_attn_idx[31] = {
	77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94,
	95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107,
};

static struct attn_hw_reg bmb_int3_bb_a0 = {
	3, 31, bmb_int3_bb_a0_attn_idx, 0x540108, 0x540114, 0x540110, 0x54010c
};

static const u16 bmb_int4_bb_a0_attn_idx[27] = {
	108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121,
	122,
	123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134,
};

static struct attn_hw_reg bmb_int4_bb_a0 = {
	4, 27, bmb_int4_bb_a0_attn_idx, 0x540120, 0x54012c, 0x540128, 0x540124
};

static const u16 bmb_int5_bb_a0_attn_idx[29] = {
	135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148,
	149,
	150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163,
};

static struct attn_hw_reg bmb_int5_bb_a0 = {
	5, 29, bmb_int5_bb_a0_attn_idx, 0x540138, 0x540144, 0x540140, 0x54013c
};

static const u16 bmb_int6_bb_a0_attn_idx[30] = {
	164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177,
	178,
	179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192,
	    193,
};

static struct attn_hw_reg bmb_int6_bb_a0 = {
	6, 30, bmb_int6_bb_a0_attn_idx, 0x540150, 0x54015c, 0x540158, 0x540154
};

static const u16 bmb_int7_bb_a0_attn_idx[32] = {
	194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207,
	208,
	209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222,
	    223, 224,
	225,
};

static struct attn_hw_reg bmb_int7_bb_a0 = {
	7, 32, bmb_int7_bb_a0_attn_idx, 0x540168, 0x540174, 0x540170, 0x54016c
};

static const u16 bmb_int8_bb_a0_attn_idx[32] = {
	226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
	240,
	241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254,
	    255, 256,
	257,
};

static struct attn_hw_reg bmb_int8_bb_a0 = {
	8, 32, bmb_int8_bb_a0_attn_idx, 0x540184, 0x540190, 0x54018c, 0x540188
};

static const u16 bmb_int9_bb_a0_attn_idx[32] = {
	258, 259, 260, 261, 262, 263, 264, 265, 266, 267, 268, 269, 270, 271,
	272,
	273, 274, 275, 276, 277, 278, 279, 280, 281, 282, 283, 284, 285, 286,
	    287, 288,
	289,
};

static struct attn_hw_reg bmb_int9_bb_a0 = {
	9, 32, bmb_int9_bb_a0_attn_idx, 0x54019c, 0x5401a8, 0x5401a4, 0x5401a0
};

static const u16 bmb_int10_bb_a0_attn_idx[3] = {
	290, 291, 292,
};

static struct attn_hw_reg bmb_int10_bb_a0 = {
	10, 3, bmb_int10_bb_a0_attn_idx, 0x5401b4, 0x5401c0, 0x5401bc, 0x5401b8
};

static const u16 bmb_int11_bb_a0_attn_idx[4] = {
	293, 294, 295, 296,
};

static struct attn_hw_reg bmb_int11_bb_a0 = {
	11, 4, bmb_int11_bb_a0_attn_idx, 0x5401cc, 0x5401d8, 0x5401d4, 0x5401d0
};

static struct attn_hw_reg *bmb_int_bb_a0_regs[12] = {
	&bmb_int0_bb_a0, &bmb_int1_bb_a0, &bmb_int2_bb_a0, &bmb_int3_bb_a0,
	&bmb_int4_bb_a0, &bmb_int5_bb_a0, &bmb_int6_bb_a0, &bmb_int7_bb_a0,
	&bmb_int8_bb_a0, &bmb_int9_bb_a0,
	&bmb_int10_bb_a0, &bmb_int11_bb_a0,
};

static const u16 bmb_int0_bb_b0_attn_idx[16] = {
	0, 1, 3, 4, 6, 7, 9, 10, 12, 13, 15, 16, 17, 18, 20, 22,
};

static struct attn_hw_reg bmb_int0_bb_b0 = {
	0, 16, bmb_int0_bb_b0_attn_idx, 0x5400c0, 0x5400cc, 0x5400c8, 0x5400c4
};

static const u16 bmb_int1_bb_b0_attn_idx[28] = {
	23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50,
};

static struct attn_hw_reg bmb_int1_bb_b0 = {
	1, 28, bmb_int1_bb_b0_attn_idx, 0x5400d8, 0x5400e4, 0x5400e0, 0x5400dc
};

static const u16 bmb_int2_bb_b0_attn_idx[26] = {
	51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68,
	69, 70, 71, 72, 73, 74, 75, 76,
};

static struct attn_hw_reg bmb_int2_bb_b0 = {
	2, 26, bmb_int2_bb_b0_attn_idx, 0x5400f0, 0x5400fc, 0x5400f8, 0x5400f4
};

static const u16 bmb_int3_bb_b0_attn_idx[31] = {
	77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94,
	95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107,
};

static struct attn_hw_reg bmb_int3_bb_b0 = {
	3, 31, bmb_int3_bb_b0_attn_idx, 0x540108, 0x540114, 0x540110, 0x54010c
};

static const u16 bmb_int4_bb_b0_attn_idx[27] = {
	108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121,
	122,
	123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134,
};

static struct attn_hw_reg bmb_int4_bb_b0 = {
	4, 27, bmb_int4_bb_b0_attn_idx, 0x540120, 0x54012c, 0x540128, 0x540124
};

static const u16 bmb_int5_bb_b0_attn_idx[29] = {
	135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148,
	149,
	150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163,
};

static struct attn_hw_reg bmb_int5_bb_b0 = {
	5, 29, bmb_int5_bb_b0_attn_idx, 0x540138, 0x540144, 0x540140, 0x54013c
};

static const u16 bmb_int6_bb_b0_attn_idx[30] = {
	164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177,
	178,
	179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192,
	    193,
};

static struct attn_hw_reg bmb_int6_bb_b0 = {
	6, 30, bmb_int6_bb_b0_attn_idx, 0x540150, 0x54015c, 0x540158, 0x540154
};

static const u16 bmb_int7_bb_b0_attn_idx[32] = {
	194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207,
	208,
	209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222,
	    223, 224,
	225,
};

static struct attn_hw_reg bmb_int7_bb_b0 = {
	7, 32, bmb_int7_bb_b0_attn_idx, 0x540168, 0x540174, 0x540170, 0x54016c
};

static const u16 bmb_int8_bb_b0_attn_idx[32] = {
	226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
	240,
	241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254,
	    255, 256,
	257,
};

static struct attn_hw_reg bmb_int8_bb_b0 = {
	8, 32, bmb_int8_bb_b0_attn_idx, 0x540184, 0x540190, 0x54018c, 0x540188
};

static const u16 bmb_int9_bb_b0_attn_idx[32] = {
	258, 259, 260, 261, 262, 263, 264, 265, 266, 267, 268, 269, 270, 271,
	272,
	273, 274, 275, 276, 277, 278, 279, 280, 281, 282, 283, 284, 285, 286,
	    287, 288,
	289,
};

static struct attn_hw_reg bmb_int9_bb_b0 = {
	9, 32, bmb_int9_bb_b0_attn_idx, 0x54019c, 0x5401a8, 0x5401a4, 0x5401a0
};

static const u16 bmb_int10_bb_b0_attn_idx[3] = {
	290, 291, 292,
};

static struct attn_hw_reg bmb_int10_bb_b0 = {
	10, 3, bmb_int10_bb_b0_attn_idx, 0x5401b4, 0x5401c0, 0x5401bc, 0x5401b8
};

static const u16 bmb_int11_bb_b0_attn_idx[4] = {
	293, 294, 295, 296,
};

static struct attn_hw_reg bmb_int11_bb_b0 = {
	11, 4, bmb_int11_bb_b0_attn_idx, 0x5401cc, 0x5401d8, 0x5401d4, 0x5401d0
};

static struct attn_hw_reg *bmb_int_bb_b0_regs[12] = {
	&bmb_int0_bb_b0, &bmb_int1_bb_b0, &bmb_int2_bb_b0, &bmb_int3_bb_b0,
	&bmb_int4_bb_b0, &bmb_int5_bb_b0, &bmb_int6_bb_b0, &bmb_int7_bb_b0,
	&bmb_int8_bb_b0, &bmb_int9_bb_b0,
	&bmb_int10_bb_b0, &bmb_int11_bb_b0,
};

static const u16 bmb_int0_k2_attn_idx[16] = {
	0, 1, 3, 4, 6, 7, 9, 10, 12, 13, 15, 16, 17, 18, 20, 22,
};

static struct attn_hw_reg bmb_int0_k2 = {
	0, 16, bmb_int0_k2_attn_idx, 0x5400c0, 0x5400cc, 0x5400c8, 0x5400c4
};

static const u16 bmb_int1_k2_attn_idx[28] = {
	23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50,
};

static struct attn_hw_reg bmb_int1_k2 = {
	1, 28, bmb_int1_k2_attn_idx, 0x5400d8, 0x5400e4, 0x5400e0, 0x5400dc
};

static const u16 bmb_int2_k2_attn_idx[26] = {
	51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68,
	69, 70, 71, 72, 73, 74, 75, 76,
};

static struct attn_hw_reg bmb_int2_k2 = {
	2, 26, bmb_int2_k2_attn_idx, 0x5400f0, 0x5400fc, 0x5400f8, 0x5400f4
};

static const u16 bmb_int3_k2_attn_idx[31] = {
	77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94,
	95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107,
};

static struct attn_hw_reg bmb_int3_k2 = {
	3, 31, bmb_int3_k2_attn_idx, 0x540108, 0x540114, 0x540110, 0x54010c
};

static const u16 bmb_int4_k2_attn_idx[27] = {
	108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121,
	122,
	123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134,
};

static struct attn_hw_reg bmb_int4_k2 = {
	4, 27, bmb_int4_k2_attn_idx, 0x540120, 0x54012c, 0x540128, 0x540124
};

static const u16 bmb_int5_k2_attn_idx[29] = {
	135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148,
	149,
	150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163,
};

static struct attn_hw_reg bmb_int5_k2 = {
	5, 29, bmb_int5_k2_attn_idx, 0x540138, 0x540144, 0x540140, 0x54013c
};

static const u16 bmb_int6_k2_attn_idx[30] = {
	164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177,
	178,
	179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192,
	    193,
};

static struct attn_hw_reg bmb_int6_k2 = {
	6, 30, bmb_int6_k2_attn_idx, 0x540150, 0x54015c, 0x540158, 0x540154
};

static const u16 bmb_int7_k2_attn_idx[32] = {
	194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207,
	208,
	209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222,
	    223, 224,
	225,
};

static struct attn_hw_reg bmb_int7_k2 = {
	7, 32, bmb_int7_k2_attn_idx, 0x540168, 0x540174, 0x540170, 0x54016c
};

static const u16 bmb_int8_k2_attn_idx[32] = {
	226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
	240,
	241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254,
	    255, 256,
	257,
};

static struct attn_hw_reg bmb_int8_k2 = {
	8, 32, bmb_int8_k2_attn_idx, 0x540184, 0x540190, 0x54018c, 0x540188
};

static const u16 bmb_int9_k2_attn_idx[32] = {
	258, 259, 260, 261, 262, 263, 264, 265, 266, 267, 268, 269, 270, 271,
	272,
	273, 274, 275, 276, 277, 278, 279, 280, 281, 282, 283, 284, 285, 286,
	    287, 288,
	289,
};

static struct attn_hw_reg bmb_int9_k2 = {
	9, 32, bmb_int9_k2_attn_idx, 0x54019c, 0x5401a8, 0x5401a4, 0x5401a0
};

static const u16 bmb_int10_k2_attn_idx[3] = {
	290, 291, 292,
};

static struct attn_hw_reg bmb_int10_k2 = {
	10, 3, bmb_int10_k2_attn_idx, 0x5401b4, 0x5401c0, 0x5401bc, 0x5401b8
};

static const u16 bmb_int11_k2_attn_idx[4] = {
	293, 294, 295, 296,
};

static struct attn_hw_reg bmb_int11_k2 = {
	11, 4, bmb_int11_k2_attn_idx, 0x5401cc, 0x5401d8, 0x5401d4, 0x5401d0
};

static struct attn_hw_reg *bmb_int_k2_regs[12] = {
	&bmb_int0_k2, &bmb_int1_k2, &bmb_int2_k2, &bmb_int3_k2, &bmb_int4_k2,
	&bmb_int5_k2, &bmb_int6_k2, &bmb_int7_k2, &bmb_int8_k2, &bmb_int9_k2,
	&bmb_int10_k2, &bmb_int11_k2,
};

#ifdef ATTN_DESC
static const char *bmb_prty_attn_desc[61] = {
	"bmb_ll_bank0_mem_prty",
	"bmb_ll_bank1_mem_prty",
	"bmb_ll_bank2_mem_prty",
	"bmb_ll_bank3_mem_prty",
	"bmb_datapath_registers",
	"bmb_mem001_i_ecc_rf_int",
	"bmb_mem008_i_ecc_rf_int",
	"bmb_mem009_i_ecc_rf_int",
	"bmb_mem010_i_ecc_rf_int",
	"bmb_mem011_i_ecc_rf_int",
	"bmb_mem012_i_ecc_rf_int",
	"bmb_mem013_i_ecc_rf_int",
	"bmb_mem014_i_ecc_rf_int",
	"bmb_mem015_i_ecc_rf_int",
	"bmb_mem016_i_ecc_rf_int",
	"bmb_mem002_i_ecc_rf_int",
	"bmb_mem003_i_ecc_rf_int",
	"bmb_mem004_i_ecc_rf_int",
	"bmb_mem005_i_ecc_rf_int",
	"bmb_mem006_i_ecc_rf_int",
	"bmb_mem007_i_ecc_rf_int",
	"bmb_mem059_i_mem_prty",
	"bmb_mem060_i_mem_prty",
	"bmb_mem037_i_mem_prty",
	"bmb_mem038_i_mem_prty",
	"bmb_mem039_i_mem_prty",
	"bmb_mem040_i_mem_prty",
	"bmb_mem041_i_mem_prty",
	"bmb_mem042_i_mem_prty",
	"bmb_mem043_i_mem_prty",
	"bmb_mem044_i_mem_prty",
	"bmb_mem045_i_mem_prty",
	"bmb_mem046_i_mem_prty",
	"bmb_mem047_i_mem_prty",
	"bmb_mem048_i_mem_prty",
	"bmb_mem049_i_mem_prty",
	"bmb_mem050_i_mem_prty",
	"bmb_mem051_i_mem_prty",
	"bmb_mem052_i_mem_prty",
	"bmb_mem053_i_mem_prty",
	"bmb_mem054_i_mem_prty",
	"bmb_mem055_i_mem_prty",
	"bmb_mem056_i_mem_prty",
	"bmb_mem057_i_mem_prty",
	"bmb_mem058_i_mem_prty",
	"bmb_mem033_i_mem_prty",
	"bmb_mem034_i_mem_prty",
	"bmb_mem035_i_mem_prty",
	"bmb_mem036_i_mem_prty",
	"bmb_mem021_i_mem_prty",
	"bmb_mem022_i_mem_prty",
	"bmb_mem023_i_mem_prty",
	"bmb_mem024_i_mem_prty",
	"bmb_mem025_i_mem_prty",
	"bmb_mem026_i_mem_prty",
	"bmb_mem027_i_mem_prty",
	"bmb_mem028_i_mem_prty",
	"bmb_mem029_i_mem_prty",
	"bmb_mem030_i_mem_prty",
	"bmb_mem031_i_mem_prty",
	"bmb_mem032_i_mem_prty",
};
#else
#define bmb_prty_attn_desc OSAL_NULL
#endif

static const u16 bmb_prty1_bb_a0_attn_idx[31] = {
	5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
	24,
	25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
};

static struct attn_hw_reg bmb_prty1_bb_a0 = {
	0, 31, bmb_prty1_bb_a0_attn_idx, 0x540400, 0x54040c, 0x540408, 0x540404
};

static const u16 bmb_prty2_bb_a0_attn_idx[25] = {
	36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53,
	54, 55, 56, 57, 58, 59, 60,
};

static struct attn_hw_reg bmb_prty2_bb_a0 = {
	1, 25, bmb_prty2_bb_a0_attn_idx, 0x540410, 0x54041c, 0x540418, 0x540414
};

static struct attn_hw_reg *bmb_prty_bb_a0_regs[2] = {
	&bmb_prty1_bb_a0, &bmb_prty2_bb_a0,
};

static const u16 bmb_prty0_bb_b0_attn_idx[5] = {
	0, 1, 2, 3, 4,
};

static struct attn_hw_reg bmb_prty0_bb_b0 = {
	0, 5, bmb_prty0_bb_b0_attn_idx, 0x5401dc, 0x5401e8, 0x5401e4, 0x5401e0
};

static const u16 bmb_prty1_bb_b0_attn_idx[31] = {
	5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
	24,
	25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
};

static struct attn_hw_reg bmb_prty1_bb_b0 = {
	1, 31, bmb_prty1_bb_b0_attn_idx, 0x540400, 0x54040c, 0x540408, 0x540404
};

static const u16 bmb_prty2_bb_b0_attn_idx[15] = {
	36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50,
};

static struct attn_hw_reg bmb_prty2_bb_b0 = {
	2, 15, bmb_prty2_bb_b0_attn_idx, 0x540410, 0x54041c, 0x540418, 0x540414
};

static struct attn_hw_reg *bmb_prty_bb_b0_regs[3] = {
	&bmb_prty0_bb_b0, &bmb_prty1_bb_b0, &bmb_prty2_bb_b0,
};

static const u16 bmb_prty0_k2_attn_idx[5] = {
	0, 1, 2, 3, 4,
};

static struct attn_hw_reg bmb_prty0_k2 = {
	0, 5, bmb_prty0_k2_attn_idx, 0x5401dc, 0x5401e8, 0x5401e4, 0x5401e0
};

static const u16 bmb_prty1_k2_attn_idx[31] = {
	5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
	24,
	25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
};

static struct attn_hw_reg bmb_prty1_k2 = {
	1, 31, bmb_prty1_k2_attn_idx, 0x540400, 0x54040c, 0x540408, 0x540404
};

static const u16 bmb_prty2_k2_attn_idx[15] = {
	36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50,
};

static struct attn_hw_reg bmb_prty2_k2 = {
	2, 15, bmb_prty2_k2_attn_idx, 0x540410, 0x54041c, 0x540418, 0x540414
};

static struct attn_hw_reg *bmb_prty_k2_regs[3] = {
	&bmb_prty0_k2, &bmb_prty1_k2, &bmb_prty2_k2,
};

#ifdef ATTN_DESC
static const char *pcie_int_attn_desc[17] = {
	"pcie_address_error",
	"pcie_link_down_detect",
	"pcie_link_up_detect",
	"pcie_cfg_link_eq_req_int",
	"pcie_pcie_bandwidth_change_detect",
	"pcie_early_hot_reset_detect",
	"pcie_hot_reset_detect",
	"pcie_l1_entry_detect",
	"pcie_l1_exit_detect",
	"pcie_ltssm_state_match_detect",
	"pcie_fc_timeout_detect",
	"pcie_pme_turnoff_message_detect",
	"pcie_cfg_send_cor_err",
	"pcie_cfg_send_nf_err",
	"pcie_cfg_send_f_err",
	"pcie_qoverflow_detect",
	"pcie_vdm_detect",
};
#else
#define pcie_int_attn_desc OSAL_NULL
#endif

static const u16 pcie_int0_k2_attn_idx[17] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
};

static struct attn_hw_reg pcie_int0_k2 = {
	0, 17, pcie_int0_k2_attn_idx, 0x547a0, 0x547ac, 0x547a8, 0x547a4
};

static struct attn_hw_reg *pcie_int_k2_regs[1] = {
	&pcie_int0_k2,
};

#ifdef ATTN_DESC
static const char *pcie_prty_attn_desc[24] = {
	"pcie_mem003_i_ecc_rf_int",
	"pcie_mem004_i_ecc_rf_int",
	"pcie_mem008_i_mem_prty",
	"pcie_mem007_i_mem_prty",
	"pcie_mem005_i_mem_prty",
	"pcie_mem006_i_mem_prty",
	"pcie_mem001_i_mem_prty",
	"pcie_mem002_i_mem_prty",
	"pcie_mem001_i_ecc_rf_int",
	"pcie_mem005_i_ecc_rf_int",
	"pcie_mem010_i_ecc_rf_int",
	"pcie_mem009_i_ecc_rf_int",
	"pcie_mem007_i_ecc_rf_int",
	"pcie_mem004_i_mem_prty_0",
	"pcie_mem004_i_mem_prty_1",
	"pcie_mem004_i_mem_prty_2",
	"pcie_mem004_i_mem_prty_3",
	"pcie_mem011_i_mem_prty_1",
	"pcie_mem011_i_mem_prty_2",
	"pcie_mem012_i_mem_prty_1",
	"pcie_mem012_i_mem_prty_2",
	"pcie_app_parity_errs_0",
	"pcie_app_parity_errs_1",
	"pcie_app_parity_errs_2",
};
#else
#define pcie_prty_attn_desc OSAL_NULL
#endif

static const u16 pcie_prty1_bb_a0_attn_idx[17] = {
	0, 2, 5, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
};

static struct attn_hw_reg pcie_prty1_bb_a0 = {
	0, 17, pcie_prty1_bb_a0_attn_idx, 0x54000, 0x5400c, 0x54008, 0x54004
};

static struct attn_hw_reg *pcie_prty_bb_a0_regs[1] = {
	&pcie_prty1_bb_a0,
};

static const u16 pcie_prty1_bb_b0_attn_idx[17] = {
	0, 2, 5, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
};

static struct attn_hw_reg pcie_prty1_bb_b0 = {
	0, 17, pcie_prty1_bb_b0_attn_idx, 0x54000, 0x5400c, 0x54008, 0x54004
};

static struct attn_hw_reg *pcie_prty_bb_b0_regs[1] = {
	&pcie_prty1_bb_b0,
};

static const u16 pcie_prty1_k2_attn_idx[8] = {
	0, 1, 2, 3, 4, 5, 6, 7,
};

static struct attn_hw_reg pcie_prty1_k2 = {
	0, 8, pcie_prty1_k2_attn_idx, 0x54000, 0x5400c, 0x54008, 0x54004
};

static const u16 pcie_prty0_k2_attn_idx[3] = {
	21, 22, 23,
};

static struct attn_hw_reg pcie_prty0_k2 = {
	1, 3, pcie_prty0_k2_attn_idx, 0x547b0, 0x547bc, 0x547b8, 0x547b4
};

static struct attn_hw_reg *pcie_prty_k2_regs[2] = {
	&pcie_prty1_k2, &pcie_prty0_k2,
};

#ifdef ATTN_DESC
static const char *mcp2_prty_attn_desc[13] = {
	"mcp2_rom_parity",
	"mcp2_mem001_i_ecc_rf_int",
	"mcp2_mem006_i_ecc_0_rf_int",
	"mcp2_mem006_i_ecc_1_rf_int",
	"mcp2_mem006_i_ecc_2_rf_int",
	"mcp2_mem006_i_ecc_3_rf_int",
	"mcp2_mem007_i_ecc_rf_int",
	"mcp2_mem004_i_mem_prty",
	"mcp2_mem003_i_mem_prty",
	"mcp2_mem002_i_mem_prty",
	"mcp2_mem009_i_mem_prty",
	"mcp2_mem008_i_mem_prty",
	"mcp2_mem005_i_mem_prty",
};
#else
#define mcp2_prty_attn_desc OSAL_NULL
#endif

static const u16 mcp2_prty0_bb_a0_attn_idx[1] = {
	0,
};

static struct attn_hw_reg mcp2_prty0_bb_a0 = {
	0, 1, mcp2_prty0_bb_a0_attn_idx, 0x52040, 0x5204c, 0x52048, 0x52044
};

static const u16 mcp2_prty1_bb_a0_attn_idx[12] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
};

static struct attn_hw_reg mcp2_prty1_bb_a0 = {
	1, 12, mcp2_prty1_bb_a0_attn_idx, 0x52204, 0x52210, 0x5220c, 0x52208
};

static struct attn_hw_reg *mcp2_prty_bb_a0_regs[2] = {
	&mcp2_prty0_bb_a0, &mcp2_prty1_bb_a0,
};

static const u16 mcp2_prty0_bb_b0_attn_idx[1] = {
	0,
};

static struct attn_hw_reg mcp2_prty0_bb_b0 = {
	0, 1, mcp2_prty0_bb_b0_attn_idx, 0x52040, 0x5204c, 0x52048, 0x52044
};

static const u16 mcp2_prty1_bb_b0_attn_idx[12] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
};

static struct attn_hw_reg mcp2_prty1_bb_b0 = {
	1, 12, mcp2_prty1_bb_b0_attn_idx, 0x52204, 0x52210, 0x5220c, 0x52208
};

static struct attn_hw_reg *mcp2_prty_bb_b0_regs[2] = {
	&mcp2_prty0_bb_b0, &mcp2_prty1_bb_b0,
};

static const u16 mcp2_prty0_k2_attn_idx[1] = {
	0,
};

static struct attn_hw_reg mcp2_prty0_k2 = {
	0, 1, mcp2_prty0_k2_attn_idx, 0x52040, 0x5204c, 0x52048, 0x52044
};

static const u16 mcp2_prty1_k2_attn_idx[12] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
};

static struct attn_hw_reg mcp2_prty1_k2 = {
	1, 12, mcp2_prty1_k2_attn_idx, 0x52204, 0x52210, 0x5220c, 0x52208
};

static struct attn_hw_reg *mcp2_prty_k2_regs[2] = {
	&mcp2_prty0_k2, &mcp2_prty1_k2,
};

#ifdef ATTN_DESC
static const char *pswhst_int_attn_desc[18] = {
	"pswhst_address_error",
	"pswhst_hst_src_fifo1_err",
	"pswhst_hst_src_fifo2_err",
	"pswhst_hst_src_fifo3_err",
	"pswhst_hst_src_fifo4_err",
	"pswhst_hst_src_fifo5_err",
	"pswhst_hst_hdr_sync_fifo_err",
	"pswhst_hst_data_sync_fifo_err",
	"pswhst_hst_cpl_sync_fifo_err",
	"pswhst_hst_vf_disabled_access",
	"pswhst_hst_permission_violation",
	"pswhst_hst_incorrect_access",
	"pswhst_hst_src_fifo6_err",
	"pswhst_hst_src_fifo7_err",
	"pswhst_hst_src_fifo8_err",
	"pswhst_hst_src_fifo9_err",
	"pswhst_hst_source_credit_violation",
	"pswhst_hst_timeout",
};
#else
#define pswhst_int_attn_desc OSAL_NULL
#endif

static const u16 pswhst_int0_bb_a0_attn_idx[18] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17,
};

static struct attn_hw_reg pswhst_int0_bb_a0 = {
	0, 18, pswhst_int0_bb_a0_attn_idx, 0x2a0180, 0x2a018c, 0x2a0188,
	0x2a0184
};

static struct attn_hw_reg *pswhst_int_bb_a0_regs[1] = {
	&pswhst_int0_bb_a0,
};

static const u16 pswhst_int0_bb_b0_attn_idx[18] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17,
};

static struct attn_hw_reg pswhst_int0_bb_b0 = {
	0, 18, pswhst_int0_bb_b0_attn_idx, 0x2a0180, 0x2a018c, 0x2a0188,
	0x2a0184
};

static struct attn_hw_reg *pswhst_int_bb_b0_regs[1] = {
	&pswhst_int0_bb_b0,
};

static const u16 pswhst_int0_k2_attn_idx[18] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17,
};

static struct attn_hw_reg pswhst_int0_k2 = {
	0, 18, pswhst_int0_k2_attn_idx, 0x2a0180, 0x2a018c, 0x2a0188, 0x2a0184
};

static struct attn_hw_reg *pswhst_int_k2_regs[1] = {
	&pswhst_int0_k2,
};

#ifdef ATTN_DESC
static const char *pswhst_prty_attn_desc[18] = {
	"pswhst_datapath_registers",
	"pswhst_mem006_i_mem_prty",
	"pswhst_mem007_i_mem_prty",
	"pswhst_mem005_i_mem_prty",
	"pswhst_mem002_i_mem_prty",
	"pswhst_mem003_i_mem_prty",
	"pswhst_mem001_i_mem_prty",
	"pswhst_mem008_i_mem_prty",
	"pswhst_mem004_i_mem_prty",
	"pswhst_mem009_i_mem_prty",
	"pswhst_mem010_i_mem_prty",
	"pswhst_mem016_i_mem_prty",
	"pswhst_mem012_i_mem_prty",
	"pswhst_mem013_i_mem_prty",
	"pswhst_mem014_i_mem_prty",
	"pswhst_mem015_i_mem_prty",
	"pswhst_mem011_i_mem_prty",
	"pswhst_mem017_i_mem_prty",
};
#else
#define pswhst_prty_attn_desc OSAL_NULL
#endif

static const u16 pswhst_prty1_bb_a0_attn_idx[17] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17,
};

static struct attn_hw_reg pswhst_prty1_bb_a0 = {
	0, 17, pswhst_prty1_bb_a0_attn_idx, 0x2a0200, 0x2a020c, 0x2a0208,
	0x2a0204
};

static struct attn_hw_reg *pswhst_prty_bb_a0_regs[1] = {
	&pswhst_prty1_bb_a0,
};

static const u16 pswhst_prty0_bb_b0_attn_idx[1] = {
	0,
};

static struct attn_hw_reg pswhst_prty0_bb_b0 = {
	0, 1, pswhst_prty0_bb_b0_attn_idx, 0x2a0190, 0x2a019c, 0x2a0198,
	0x2a0194
};

static const u16 pswhst_prty1_bb_b0_attn_idx[17] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17,
};

static struct attn_hw_reg pswhst_prty1_bb_b0 = {
	1, 17, pswhst_prty1_bb_b0_attn_idx, 0x2a0200, 0x2a020c, 0x2a0208,
	0x2a0204
};

static struct attn_hw_reg *pswhst_prty_bb_b0_regs[2] = {
	&pswhst_prty0_bb_b0, &pswhst_prty1_bb_b0,
};

static const u16 pswhst_prty0_k2_attn_idx[1] = {
	0,
};

static struct attn_hw_reg pswhst_prty0_k2 = {
	0, 1, pswhst_prty0_k2_attn_idx, 0x2a0190, 0x2a019c, 0x2a0198, 0x2a0194
};

static const u16 pswhst_prty1_k2_attn_idx[17] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17,
};

static struct attn_hw_reg pswhst_prty1_k2 = {
	1, 17, pswhst_prty1_k2_attn_idx, 0x2a0200, 0x2a020c, 0x2a0208, 0x2a0204
};

static struct attn_hw_reg *pswhst_prty_k2_regs[2] = {
	&pswhst_prty0_k2, &pswhst_prty1_k2,
};

#ifdef ATTN_DESC
static const char *pswhst2_int_attn_desc[5] = {
	"pswhst2_address_error",
	"pswhst2_hst_header_fifo_err",
	"pswhst2_hst_data_fifo_err",
	"pswhst2_hst_cpl_fifo_err",
	"pswhst2_hst_ireq_fifo_err",
};
#else
#define pswhst2_int_attn_desc OSAL_NULL
#endif

static const u16 pswhst2_int0_bb_a0_attn_idx[5] = {
	0, 1, 2, 3, 4,
};

static struct attn_hw_reg pswhst2_int0_bb_a0 = {
	0, 5, pswhst2_int0_bb_a0_attn_idx, 0x29e180, 0x29e18c, 0x29e188,
	0x29e184
};

static struct attn_hw_reg *pswhst2_int_bb_a0_regs[1] = {
	&pswhst2_int0_bb_a0,
};

static const u16 pswhst2_int0_bb_b0_attn_idx[5] = {
	0, 1, 2, 3, 4,
};

static struct attn_hw_reg pswhst2_int0_bb_b0 = {
	0, 5, pswhst2_int0_bb_b0_attn_idx, 0x29e180, 0x29e18c, 0x29e188,
	0x29e184
};

static struct attn_hw_reg *pswhst2_int_bb_b0_regs[1] = {
	&pswhst2_int0_bb_b0,
};

static const u16 pswhst2_int0_k2_attn_idx[5] = {
	0, 1, 2, 3, 4,
};

static struct attn_hw_reg pswhst2_int0_k2 = {
	0, 5, pswhst2_int0_k2_attn_idx, 0x29e180, 0x29e18c, 0x29e188, 0x29e184
};

static struct attn_hw_reg *pswhst2_int_k2_regs[1] = {
	&pswhst2_int0_k2,
};

#ifdef ATTN_DESC
static const char *pswhst2_prty_attn_desc[1] = {
	"pswhst2_datapath_registers",
};
#else
#define pswhst2_prty_attn_desc OSAL_NULL
#endif

static const u16 pswhst2_prty0_bb_b0_attn_idx[1] = {
	0,
};

static struct attn_hw_reg pswhst2_prty0_bb_b0 = {
	0, 1, pswhst2_prty0_bb_b0_attn_idx, 0x29e190, 0x29e19c, 0x29e198,
	0x29e194
};

static struct attn_hw_reg *pswhst2_prty_bb_b0_regs[1] = {
	&pswhst2_prty0_bb_b0,
};

static const u16 pswhst2_prty0_k2_attn_idx[1] = {
	0,
};

static struct attn_hw_reg pswhst2_prty0_k2 = {
	0, 1, pswhst2_prty0_k2_attn_idx, 0x29e190, 0x29e19c, 0x29e198, 0x29e194
};

static struct attn_hw_reg *pswhst2_prty_k2_regs[1] = {
	&pswhst2_prty0_k2,
};

#ifdef ATTN_DESC
static const char *pswrd_int_attn_desc[3] = {
	"pswrd_address_error",
	"pswrd_pop_error",
	"pswrd_pop_pbf_error",
};
#else
#define pswrd_int_attn_desc OSAL_NULL
#endif

static const u16 pswrd_int0_bb_a0_attn_idx[3] = {
	0, 1, 2,
};

static struct attn_hw_reg pswrd_int0_bb_a0 = {
	0, 3, pswrd_int0_bb_a0_attn_idx, 0x29c180, 0x29c18c, 0x29c188, 0x29c184
};

static struct attn_hw_reg *pswrd_int_bb_a0_regs[1] = {
	&pswrd_int0_bb_a0,
};

static const u16 pswrd_int0_bb_b0_attn_idx[3] = {
	0, 1, 2,
};

static struct attn_hw_reg pswrd_int0_bb_b0 = {
	0, 3, pswrd_int0_bb_b0_attn_idx, 0x29c180, 0x29c18c, 0x29c188, 0x29c184
};

static struct attn_hw_reg *pswrd_int_bb_b0_regs[1] = {
	&pswrd_int0_bb_b0,
};

static const u16 pswrd_int0_k2_attn_idx[3] = {
	0, 1, 2,
};

static struct attn_hw_reg pswrd_int0_k2 = {
	0, 3, pswrd_int0_k2_attn_idx, 0x29c180, 0x29c18c, 0x29c188, 0x29c184
};

static struct attn_hw_reg *pswrd_int_k2_regs[1] = {
	&pswrd_int0_k2,
};

#ifdef ATTN_DESC
static const char *pswrd_prty_attn_desc[1] = {
	"pswrd_datapath_registers",
};
#else
#define pswrd_prty_attn_desc OSAL_NULL
#endif

static const u16 pswrd_prty0_bb_b0_attn_idx[1] = {
	0,
};

static struct attn_hw_reg pswrd_prty0_bb_b0 = {
	0, 1, pswrd_prty0_bb_b0_attn_idx, 0x29c190, 0x29c19c, 0x29c198,
	0x29c194
};

static struct attn_hw_reg *pswrd_prty_bb_b0_regs[1] = {
	&pswrd_prty0_bb_b0,
};

static const u16 pswrd_prty0_k2_attn_idx[1] = {
	0,
};

static struct attn_hw_reg pswrd_prty0_k2 = {
	0, 1, pswrd_prty0_k2_attn_idx, 0x29c190, 0x29c19c, 0x29c198, 0x29c194
};

static struct attn_hw_reg *pswrd_prty_k2_regs[1] = {
	&pswrd_prty0_k2,
};

#ifdef ATTN_DESC
static const char *pswrd2_int_attn_desc[5] = {
	"pswrd2_address_error",
	"pswrd2_sr_fifo_error",
	"pswrd2_blk_fifo_error",
	"pswrd2_push_error",
	"pswrd2_push_pbf_error",
};
#else
#define pswrd2_int_attn_desc OSAL_NULL
#endif

static const u16 pswrd2_int0_bb_a0_attn_idx[5] = {
	0, 1, 2, 3, 4,
};

static struct attn_hw_reg pswrd2_int0_bb_a0 = {
	0, 5, pswrd2_int0_bb_a0_attn_idx, 0x29d180, 0x29d18c, 0x29d188,
	0x29d184
};

static struct attn_hw_reg *pswrd2_int_bb_a0_regs[1] = {
	&pswrd2_int0_bb_a0,
};

static const u16 pswrd2_int0_bb_b0_attn_idx[5] = {
	0, 1, 2, 3, 4,
};

static struct attn_hw_reg pswrd2_int0_bb_b0 = {
	0, 5, pswrd2_int0_bb_b0_attn_idx, 0x29d180, 0x29d18c, 0x29d188,
	0x29d184
};

static struct attn_hw_reg *pswrd2_int_bb_b0_regs[1] = {
	&pswrd2_int0_bb_b0,
};

static const u16 pswrd2_int0_k2_attn_idx[5] = {
	0, 1, 2, 3, 4,
};

static struct attn_hw_reg pswrd2_int0_k2 = {
	0, 5, pswrd2_int0_k2_attn_idx, 0x29d180, 0x29d18c, 0x29d188, 0x29d184
};

static struct attn_hw_reg *pswrd2_int_k2_regs[1] = {
	&pswrd2_int0_k2,
};

#ifdef ATTN_DESC
static const char *pswrd2_prty_attn_desc[36] = {
	"pswrd2_datapath_registers",
	"pswrd2_mem017_i_ecc_rf_int",
	"pswrd2_mem018_i_ecc_rf_int",
	"pswrd2_mem019_i_ecc_rf_int",
	"pswrd2_mem020_i_ecc_rf_int",
	"pswrd2_mem021_i_ecc_rf_int",
	"pswrd2_mem022_i_ecc_rf_int",
	"pswrd2_mem023_i_ecc_rf_int",
	"pswrd2_mem024_i_ecc_rf_int",
	"pswrd2_mem025_i_ecc_rf_int",
	"pswrd2_mem015_i_ecc_rf_int",
	"pswrd2_mem034_i_mem_prty",
	"pswrd2_mem032_i_mem_prty",
	"pswrd2_mem028_i_mem_prty",
	"pswrd2_mem033_i_mem_prty",
	"pswrd2_mem030_i_mem_prty",
	"pswrd2_mem029_i_mem_prty",
	"pswrd2_mem031_i_mem_prty",
	"pswrd2_mem027_i_mem_prty",
	"pswrd2_mem026_i_mem_prty",
	"pswrd2_mem001_i_mem_prty",
	"pswrd2_mem007_i_mem_prty",
	"pswrd2_mem008_i_mem_prty",
	"pswrd2_mem009_i_mem_prty",
	"pswrd2_mem010_i_mem_prty",
	"pswrd2_mem011_i_mem_prty",
	"pswrd2_mem012_i_mem_prty",
	"pswrd2_mem013_i_mem_prty",
	"pswrd2_mem014_i_mem_prty",
	"pswrd2_mem002_i_mem_prty",
	"pswrd2_mem003_i_mem_prty",
	"pswrd2_mem004_i_mem_prty",
	"pswrd2_mem005_i_mem_prty",
	"pswrd2_mem006_i_mem_prty",
	"pswrd2_mem016_i_mem_prty",
	"pswrd2_mem015_i_mem_prty",
};
#else
#define pswrd2_prty_attn_desc OSAL_NULL
#endif

static const u16 pswrd2_prty1_bb_a0_attn_idx[31] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
	22,
	23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
};

static struct attn_hw_reg pswrd2_prty1_bb_a0 = {
	0, 31, pswrd2_prty1_bb_a0_attn_idx, 0x29d200, 0x29d20c, 0x29d208,
	0x29d204
};

static const u16 pswrd2_prty2_bb_a0_attn_idx[3] = {
	33, 34, 35,
};

static struct attn_hw_reg pswrd2_prty2_bb_a0 = {
	1, 3, pswrd2_prty2_bb_a0_attn_idx, 0x29d210, 0x29d21c, 0x29d218,
	0x29d214
};

static struct attn_hw_reg *pswrd2_prty_bb_a0_regs[2] = {
	&pswrd2_prty1_bb_a0, &pswrd2_prty2_bb_a0,
};

static const u16 pswrd2_prty0_bb_b0_attn_idx[1] = {
	0,
};

static struct attn_hw_reg pswrd2_prty0_bb_b0 = {
	0, 1, pswrd2_prty0_bb_b0_attn_idx, 0x29d190, 0x29d19c, 0x29d198,
	0x29d194
};

static const u16 pswrd2_prty1_bb_b0_attn_idx[31] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
	21,
	22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
};

static struct attn_hw_reg pswrd2_prty1_bb_b0 = {
	1, 31, pswrd2_prty1_bb_b0_attn_idx, 0x29d200, 0x29d20c, 0x29d208,
	0x29d204
};

static const u16 pswrd2_prty2_bb_b0_attn_idx[3] = {
	32, 33, 34,
};

static struct attn_hw_reg pswrd2_prty2_bb_b0 = {
	2, 3, pswrd2_prty2_bb_b0_attn_idx, 0x29d210, 0x29d21c, 0x29d218,
	0x29d214
};

static struct attn_hw_reg *pswrd2_prty_bb_b0_regs[3] = {
	&pswrd2_prty0_bb_b0, &pswrd2_prty1_bb_b0, &pswrd2_prty2_bb_b0,
};

static const u16 pswrd2_prty0_k2_attn_idx[1] = {
	0,
};

static struct attn_hw_reg pswrd2_prty0_k2 = {
	0, 1, pswrd2_prty0_k2_attn_idx, 0x29d190, 0x29d19c, 0x29d198, 0x29d194
};

static const u16 pswrd2_prty1_k2_attn_idx[31] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
	21,
	22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
};

static struct attn_hw_reg pswrd2_prty1_k2 = {
	1, 31, pswrd2_prty1_k2_attn_idx, 0x29d200, 0x29d20c, 0x29d208, 0x29d204
};

static const u16 pswrd2_prty2_k2_attn_idx[3] = {
	32, 33, 34,
};

static struct attn_hw_reg pswrd2_prty2_k2 = {
	2, 3, pswrd2_prty2_k2_attn_idx, 0x29d210, 0x29d21c, 0x29d218, 0x29d214
};

static struct attn_hw_reg *pswrd2_prty_k2_regs[3] = {
	&pswrd2_prty0_k2, &pswrd2_prty1_k2, &pswrd2_prty2_k2,
};

#ifdef ATTN_DESC
static const char *pswwr_int_attn_desc[16] = {
	"pswwr_address_error",
	"pswwr_src_fifo_overflow",
	"pswwr_qm_fifo_overflow",
	"pswwr_tm_fifo_overflow",
	"pswwr_usdm_fifo_overflow",
	"pswwr_usdmdp_fifo_overflow",
	"pswwr_xsdm_fifo_overflow",
	"pswwr_tsdm_fifo_overflow",
	"pswwr_cduwr_fifo_overflow",
	"pswwr_dbg_fifo_overflow",
	"pswwr_dmae_fifo_overflow",
	"pswwr_hc_fifo_overflow",
	"pswwr_msdm_fifo_overflow",
	"pswwr_ysdm_fifo_overflow",
	"pswwr_psdm_fifo_overflow",
	"pswwr_m2p_fifo_overflow",
};
#else
#define pswwr_int_attn_desc OSAL_NULL
#endif

static const u16 pswwr_int0_bb_a0_attn_idx[16] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
};

static struct attn_hw_reg pswwr_int0_bb_a0 = {
	0, 16, pswwr_int0_bb_a0_attn_idx, 0x29a180, 0x29a18c, 0x29a188,
	0x29a184
};

static struct attn_hw_reg *pswwr_int_bb_a0_regs[1] = {
	&pswwr_int0_bb_a0,
};

static const u16 pswwr_int0_bb_b0_attn_idx[16] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
};

static struct attn_hw_reg pswwr_int0_bb_b0 = {
	0, 16, pswwr_int0_bb_b0_attn_idx, 0x29a180, 0x29a18c, 0x29a188,
	0x29a184
};

static struct attn_hw_reg *pswwr_int_bb_b0_regs[1] = {
	&pswwr_int0_bb_b0,
};

static const u16 pswwr_int0_k2_attn_idx[16] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
};

static struct attn_hw_reg pswwr_int0_k2 = {
	0, 16, pswwr_int0_k2_attn_idx, 0x29a180, 0x29a18c, 0x29a188, 0x29a184
};

static struct attn_hw_reg *pswwr_int_k2_regs[1] = {
	&pswwr_int0_k2,
};

#ifdef ATTN_DESC
static const char *pswwr_prty_attn_desc[1] = {
	"pswwr_datapath_registers",
};
#else
#define pswwr_prty_attn_desc OSAL_NULL
#endif

static const u16 pswwr_prty0_bb_b0_attn_idx[1] = {
	0,
};

static struct attn_hw_reg pswwr_prty0_bb_b0 = {
	0, 1, pswwr_prty0_bb_b0_attn_idx, 0x29a190, 0x29a19c, 0x29a198,
	0x29a194
};

static struct attn_hw_reg *pswwr_prty_bb_b0_regs[1] = {
	&pswwr_prty0_bb_b0,
};

static const u16 pswwr_prty0_k2_attn_idx[1] = {
	0,
};

static struct attn_hw_reg pswwr_prty0_k2 = {
	0, 1, pswwr_prty0_k2_attn_idx, 0x29a190, 0x29a19c, 0x29a198, 0x29a194
};

static struct attn_hw_reg *pswwr_prty_k2_regs[1] = {
	&pswwr_prty0_k2,
};

#ifdef ATTN_DESC
static const char *pswwr2_int_attn_desc[19] = {
	"pswwr2_address_error",
	"pswwr2_pglue_eop_error",
	"pswwr2_pglue_lsr_error",
	"pswwr2_tm_underflow",
	"pswwr2_qm_underflow",
	"pswwr2_src_underflow",
	"pswwr2_usdm_underflow",
	"pswwr2_tsdm_underflow",
	"pswwr2_xsdm_underflow",
	"pswwr2_usdmdp_underflow",
	"pswwr2_cdu_underflow",
	"pswwr2_dbg_underflow",
	"pswwr2_dmae_underflow",
	"pswwr2_hc_underflow",
	"pswwr2_msdm_underflow",
	"pswwr2_ysdm_underflow",
	"pswwr2_psdm_underflow",
	"pswwr2_m2p_underflow",
	"pswwr2_pglue_eop_error_in_line",
};
#else
#define pswwr2_int_attn_desc OSAL_NULL
#endif

static const u16 pswwr2_int0_bb_a0_attn_idx[19] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
};

static struct attn_hw_reg pswwr2_int0_bb_a0 = {
	0, 19, pswwr2_int0_bb_a0_attn_idx, 0x29b180, 0x29b18c, 0x29b188,
	0x29b184
};

static struct attn_hw_reg *pswwr2_int_bb_a0_regs[1] = {
	&pswwr2_int0_bb_a0,
};

static const u16 pswwr2_int0_bb_b0_attn_idx[19] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
};

static struct attn_hw_reg pswwr2_int0_bb_b0 = {
	0, 19, pswwr2_int0_bb_b0_attn_idx, 0x29b180, 0x29b18c, 0x29b188,
	0x29b184
};

static struct attn_hw_reg *pswwr2_int_bb_b0_regs[1] = {
	&pswwr2_int0_bb_b0,
};

static const u16 pswwr2_int0_k2_attn_idx[19] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
};

static struct attn_hw_reg pswwr2_int0_k2 = {
	0, 19, pswwr2_int0_k2_attn_idx, 0x29b180, 0x29b18c, 0x29b188, 0x29b184
};

static struct attn_hw_reg *pswwr2_int_k2_regs[1] = {
	&pswwr2_int0_k2,
};

#ifdef ATTN_DESC
static const char *pswwr2_prty_attn_desc[114] = {
	"pswwr2_datapath_registers",
	"pswwr2_mem008_i_ecc_rf_int",
	"pswwr2_mem001_i_mem_prty",
	"pswwr2_mem014_i_mem_prty_0",
	"pswwr2_mem014_i_mem_prty_1",
	"pswwr2_mem014_i_mem_prty_2",
	"pswwr2_mem014_i_mem_prty_3",
	"pswwr2_mem014_i_mem_prty_4",
	"pswwr2_mem014_i_mem_prty_5",
	"pswwr2_mem014_i_mem_prty_6",
	"pswwr2_mem014_i_mem_prty_7",
	"pswwr2_mem014_i_mem_prty_8",
	"pswwr2_mem016_i_mem_prty_0",
	"pswwr2_mem016_i_mem_prty_1",
	"pswwr2_mem016_i_mem_prty_2",
	"pswwr2_mem016_i_mem_prty_3",
	"pswwr2_mem016_i_mem_prty_4",
	"pswwr2_mem016_i_mem_prty_5",
	"pswwr2_mem016_i_mem_prty_6",
	"pswwr2_mem016_i_mem_prty_7",
	"pswwr2_mem016_i_mem_prty_8",
	"pswwr2_mem007_i_mem_prty_0",
	"pswwr2_mem007_i_mem_prty_1",
	"pswwr2_mem007_i_mem_prty_2",
	"pswwr2_mem007_i_mem_prty_3",
	"pswwr2_mem007_i_mem_prty_4",
	"pswwr2_mem007_i_mem_prty_5",
	"pswwr2_mem007_i_mem_prty_6",
	"pswwr2_mem007_i_mem_prty_7",
	"pswwr2_mem007_i_mem_prty_8",
	"pswwr2_mem017_i_mem_prty_0",
	"pswwr2_mem017_i_mem_prty_1",
	"pswwr2_mem017_i_mem_prty_2",
	"pswwr2_mem017_i_mem_prty_3",
	"pswwr2_mem017_i_mem_prty_4",
	"pswwr2_mem017_i_mem_prty_5",
	"pswwr2_mem017_i_mem_prty_6",
	"pswwr2_mem017_i_mem_prty_7",
	"pswwr2_mem017_i_mem_prty_8",
	"pswwr2_mem009_i_mem_prty_0",
	"pswwr2_mem009_i_mem_prty_1",
	"pswwr2_mem009_i_mem_prty_2",
	"pswwr2_mem009_i_mem_prty_3",
	"pswwr2_mem009_i_mem_prty_4",
	"pswwr2_mem009_i_mem_prty_5",
	"pswwr2_mem009_i_mem_prty_6",
	"pswwr2_mem009_i_mem_prty_7",
	"pswwr2_mem009_i_mem_prty_8",
	"pswwr2_mem013_i_mem_prty_0",
	"pswwr2_mem013_i_mem_prty_1",
	"pswwr2_mem013_i_mem_prty_2",
	"pswwr2_mem013_i_mem_prty_3",
	"pswwr2_mem013_i_mem_prty_4",
	"pswwr2_mem013_i_mem_prty_5",
	"pswwr2_mem013_i_mem_prty_6",
	"pswwr2_mem013_i_mem_prty_7",
	"pswwr2_mem013_i_mem_prty_8",
	"pswwr2_mem006_i_mem_prty_0",
	"pswwr2_mem006_i_mem_prty_1",
	"pswwr2_mem006_i_mem_prty_2",
	"pswwr2_mem006_i_mem_prty_3",
	"pswwr2_mem006_i_mem_prty_4",
	"pswwr2_mem006_i_mem_prty_5",
	"pswwr2_mem006_i_mem_prty_6",
	"pswwr2_mem006_i_mem_prty_7",
	"pswwr2_mem006_i_mem_prty_8",
	"pswwr2_mem010_i_mem_prty_0",
	"pswwr2_mem010_i_mem_prty_1",
	"pswwr2_mem010_i_mem_prty_2",
	"pswwr2_mem010_i_mem_prty_3",
	"pswwr2_mem010_i_mem_prty_4",
	"pswwr2_mem010_i_mem_prty_5",
	"pswwr2_mem010_i_mem_prty_6",
	"pswwr2_mem010_i_mem_prty_7",
	"pswwr2_mem010_i_mem_prty_8",
	"pswwr2_mem012_i_mem_prty",
	"pswwr2_mem011_i_mem_prty_0",
	"pswwr2_mem011_i_mem_prty_1",
	"pswwr2_mem011_i_mem_prty_2",
	"pswwr2_mem011_i_mem_prty_3",
	"pswwr2_mem011_i_mem_prty_4",
	"pswwr2_mem011_i_mem_prty_5",
	"pswwr2_mem011_i_mem_prty_6",
	"pswwr2_mem011_i_mem_prty_7",
	"pswwr2_mem011_i_mem_prty_8",
	"pswwr2_mem004_i_mem_prty_0",
	"pswwr2_mem004_i_mem_prty_1",
	"pswwr2_mem004_i_mem_prty_2",
	"pswwr2_mem004_i_mem_prty_3",
	"pswwr2_mem004_i_mem_prty_4",
	"pswwr2_mem004_i_mem_prty_5",
	"pswwr2_mem004_i_mem_prty_6",
	"pswwr2_mem004_i_mem_prty_7",
	"pswwr2_mem004_i_mem_prty_8",
	"pswwr2_mem015_i_mem_prty_0",
	"pswwr2_mem015_i_mem_prty_1",
	"pswwr2_mem015_i_mem_prty_2",
	"pswwr2_mem005_i_mem_prty_0",
	"pswwr2_mem005_i_mem_prty_1",
	"pswwr2_mem005_i_mem_prty_2",
	"pswwr2_mem005_i_mem_prty_3",
	"pswwr2_mem005_i_mem_prty_4",
	"pswwr2_mem005_i_mem_prty_5",
	"pswwr2_mem005_i_mem_prty_6",
	"pswwr2_mem005_i_mem_prty_7",
	"pswwr2_mem005_i_mem_prty_8",
	"pswwr2_mem002_i_mem_prty_0",
	"pswwr2_mem002_i_mem_prty_1",
	"pswwr2_mem002_i_mem_prty_2",
	"pswwr2_mem002_i_mem_prty_3",
	"pswwr2_mem002_i_mem_prty_4",
	"pswwr2_mem003_i_mem_prty_0",
	"pswwr2_mem003_i_mem_prty_1",
	"pswwr2_mem003_i_mem_prty_2",
};
#else
#define pswwr2_prty_attn_desc OSAL_NULL
#endif

static const u16 pswwr2_prty1_bb_a0_attn_idx[31] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
	21,
	22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
};

static struct attn_hw_reg pswwr2_prty1_bb_a0 = {
	0, 31, pswwr2_prty1_bb_a0_attn_idx, 0x29b200, 0x29b20c, 0x29b208,
	0x29b204
};

static const u16 pswwr2_prty2_bb_a0_attn_idx[31] = {
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49,
	50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62,
};

static struct attn_hw_reg pswwr2_prty2_bb_a0 = {
	1, 31, pswwr2_prty2_bb_a0_attn_idx, 0x29b210, 0x29b21c, 0x29b218,
	0x29b214
};

static const u16 pswwr2_prty3_bb_a0_attn_idx[31] = {
	63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80,
	81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93,
};

static struct attn_hw_reg pswwr2_prty3_bb_a0 = {
	2, 31, pswwr2_prty3_bb_a0_attn_idx, 0x29b220, 0x29b22c, 0x29b228,
	0x29b224
};

static const u16 pswwr2_prty4_bb_a0_attn_idx[20] = {
	94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108,
	109,
	110, 111, 112, 113,
};

static struct attn_hw_reg pswwr2_prty4_bb_a0 = {
	3, 20, pswwr2_prty4_bb_a0_attn_idx, 0x29b230, 0x29b23c, 0x29b238,
	0x29b234
};

static struct attn_hw_reg *pswwr2_prty_bb_a0_regs[4] = {
	&pswwr2_prty1_bb_a0, &pswwr2_prty2_bb_a0, &pswwr2_prty3_bb_a0,
	&pswwr2_prty4_bb_a0,
};

static const u16 pswwr2_prty0_bb_b0_attn_idx[1] = {
	0,
};

static struct attn_hw_reg pswwr2_prty0_bb_b0 = {
	0, 1, pswwr2_prty0_bb_b0_attn_idx, 0x29b190, 0x29b19c, 0x29b198,
	0x29b194
};

static const u16 pswwr2_prty1_bb_b0_attn_idx[31] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
	21,
	22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
};

static struct attn_hw_reg pswwr2_prty1_bb_b0 = {
	1, 31, pswwr2_prty1_bb_b0_attn_idx, 0x29b200, 0x29b20c, 0x29b208,
	0x29b204
};

static const u16 pswwr2_prty2_bb_b0_attn_idx[31] = {
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49,
	50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62,
};

static struct attn_hw_reg pswwr2_prty2_bb_b0 = {
	2, 31, pswwr2_prty2_bb_b0_attn_idx, 0x29b210, 0x29b21c, 0x29b218,
	0x29b214
};

static const u16 pswwr2_prty3_bb_b0_attn_idx[31] = {
	63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80,
	81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93,
};

static struct attn_hw_reg pswwr2_prty3_bb_b0 = {
	3, 31, pswwr2_prty3_bb_b0_attn_idx, 0x29b220, 0x29b22c, 0x29b228,
	0x29b224
};

static const u16 pswwr2_prty4_bb_b0_attn_idx[20] = {
	94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108,
	109,
	110, 111, 112, 113,
};

static struct attn_hw_reg pswwr2_prty4_bb_b0 = {
	4, 20, pswwr2_prty4_bb_b0_attn_idx, 0x29b230, 0x29b23c, 0x29b238,
	0x29b234
};

static struct attn_hw_reg *pswwr2_prty_bb_b0_regs[5] = {
	&pswwr2_prty0_bb_b0, &pswwr2_prty1_bb_b0, &pswwr2_prty2_bb_b0,
	&pswwr2_prty3_bb_b0, &pswwr2_prty4_bb_b0,
};

static const u16 pswwr2_prty0_k2_attn_idx[1] = {
	0,
};

static struct attn_hw_reg pswwr2_prty0_k2 = {
	0, 1, pswwr2_prty0_k2_attn_idx, 0x29b190, 0x29b19c, 0x29b198, 0x29b194
};

static const u16 pswwr2_prty1_k2_attn_idx[31] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
	21,
	22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
};

static struct attn_hw_reg pswwr2_prty1_k2 = {
	1, 31, pswwr2_prty1_k2_attn_idx, 0x29b200, 0x29b20c, 0x29b208, 0x29b204
};

static const u16 pswwr2_prty2_k2_attn_idx[31] = {
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49,
	50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62,
};

static struct attn_hw_reg pswwr2_prty2_k2 = {
	2, 31, pswwr2_prty2_k2_attn_idx, 0x29b210, 0x29b21c, 0x29b218, 0x29b214
};

static const u16 pswwr2_prty3_k2_attn_idx[31] = {
	63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80,
	81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93,
};

static struct attn_hw_reg pswwr2_prty3_k2 = {
	3, 31, pswwr2_prty3_k2_attn_idx, 0x29b220, 0x29b22c, 0x29b228, 0x29b224
};

static const u16 pswwr2_prty4_k2_attn_idx[20] = {
	94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108,
	109,
	110, 111, 112, 113,
};

static struct attn_hw_reg pswwr2_prty4_k2 = {
	4, 20, pswwr2_prty4_k2_attn_idx, 0x29b230, 0x29b23c, 0x29b238, 0x29b234
};

static struct attn_hw_reg *pswwr2_prty_k2_regs[5] = {
	&pswwr2_prty0_k2, &pswwr2_prty1_k2, &pswwr2_prty2_k2, &pswwr2_prty3_k2,
	&pswwr2_prty4_k2,
};

#ifdef ATTN_DESC
static const char *pswrq_int_attn_desc[21] = {
	"pswrq_address_error",
	"pswrq_pbf_fifo_overflow",
	"pswrq_src_fifo_overflow",
	"pswrq_qm_fifo_overflow",
	"pswrq_tm_fifo_overflow",
	"pswrq_usdm_fifo_overflow",
	"pswrq_m2p_fifo_overflow",
	"pswrq_xsdm_fifo_overflow",
	"pswrq_tsdm_fifo_overflow",
	"pswrq_ptu_fifo_overflow",
	"pswrq_cduwr_fifo_overflow",
	"pswrq_cdurd_fifo_overflow",
	"pswrq_dmae_fifo_overflow",
	"pswrq_hc_fifo_overflow",
	"pswrq_dbg_fifo_overflow",
	"pswrq_msdm_fifo_overflow",
	"pswrq_ysdm_fifo_overflow",
	"pswrq_psdm_fifo_overflow",
	"pswrq_prm_fifo_overflow",
	"pswrq_muld_fifo_overflow",
	"pswrq_xyld_fifo_overflow",
};
#else
#define pswrq_int_attn_desc OSAL_NULL
#endif

static const u16 pswrq_int0_bb_a0_attn_idx[21] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
};

static struct attn_hw_reg pswrq_int0_bb_a0 = {
	0, 21, pswrq_int0_bb_a0_attn_idx, 0x280180, 0x28018c, 0x280188,
	0x280184
};

static struct attn_hw_reg *pswrq_int_bb_a0_regs[1] = {
	&pswrq_int0_bb_a0,
};

static const u16 pswrq_int0_bb_b0_attn_idx[21] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
};

static struct attn_hw_reg pswrq_int0_bb_b0 = {
	0, 21, pswrq_int0_bb_b0_attn_idx, 0x280180, 0x28018c, 0x280188,
	0x280184
};

static struct attn_hw_reg *pswrq_int_bb_b0_regs[1] = {
	&pswrq_int0_bb_b0,
};

static const u16 pswrq_int0_k2_attn_idx[21] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
};

static struct attn_hw_reg pswrq_int0_k2 = {
	0, 21, pswrq_int0_k2_attn_idx, 0x280180, 0x28018c, 0x280188, 0x280184
};

static struct attn_hw_reg *pswrq_int_k2_regs[1] = {
	&pswrq_int0_k2,
};

#ifdef ATTN_DESC
static const char *pswrq_prty_attn_desc[1] = {
	"pswrq_pxp_busip_parity",
};
#else
#define pswrq_prty_attn_desc OSAL_NULL
#endif

static const u16 pswrq_prty0_bb_b0_attn_idx[1] = {
	0,
};

static struct attn_hw_reg pswrq_prty0_bb_b0 = {
	0, 1, pswrq_prty0_bb_b0_attn_idx, 0x280190, 0x28019c, 0x280198,
	0x280194
};

static struct attn_hw_reg *pswrq_prty_bb_b0_regs[1] = {
	&pswrq_prty0_bb_b0,
};

static const u16 pswrq_prty0_k2_attn_idx[1] = {
	0,
};

static struct attn_hw_reg pswrq_prty0_k2 = {
	0, 1, pswrq_prty0_k2_attn_idx, 0x280190, 0x28019c, 0x280198, 0x280194
};

static struct attn_hw_reg *pswrq_prty_k2_regs[1] = {
	&pswrq_prty0_k2,
};

#ifdef ATTN_DESC
static const char *pswrq2_int_attn_desc[15] = {
	"pswrq2_address_error",
	"pswrq2_l2p_fifo_overflow",
	"pswrq2_wdfifo_overflow",
	"pswrq2_phyaddr_fifo_of",
	"pswrq2_l2p_violation_1",
	"pswrq2_l2p_violation_2",
	"pswrq2_free_list_empty",
	"pswrq2_elt_addr",
	"pswrq2_l2p_vf_err",
	"pswrq2_core_wdone_overflow",
	"pswrq2_treq_fifo_underflow",
	"pswrq2_treq_fifo_overflow",
	"pswrq2_icpl_fifo_underflow",
	"pswrq2_icpl_fifo_overflow",
	"pswrq2_back2back_atc_response",
};
#else
#define pswrq2_int_attn_desc OSAL_NULL
#endif

static const u16 pswrq2_int0_bb_a0_attn_idx[15] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
};

static struct attn_hw_reg pswrq2_int0_bb_a0 = {
	0, 15, pswrq2_int0_bb_a0_attn_idx, 0x240180, 0x24018c, 0x240188,
	0x240184
};

static struct attn_hw_reg *pswrq2_int_bb_a0_regs[1] = {
	&pswrq2_int0_bb_a0,
};

static const u16 pswrq2_int0_bb_b0_attn_idx[15] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
};

static struct attn_hw_reg pswrq2_int0_bb_b0 = {
	0, 15, pswrq2_int0_bb_b0_attn_idx, 0x240180, 0x24018c, 0x240188,
	0x240184
};

static struct attn_hw_reg *pswrq2_int_bb_b0_regs[1] = {
	&pswrq2_int0_bb_b0,
};

static const u16 pswrq2_int0_k2_attn_idx[15] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
};

static struct attn_hw_reg pswrq2_int0_k2 = {
	0, 15, pswrq2_int0_k2_attn_idx, 0x240180, 0x24018c, 0x240188, 0x240184
};

static struct attn_hw_reg *pswrq2_int_k2_regs[1] = {
	&pswrq2_int0_k2,
};

#ifdef ATTN_DESC
static const char *pswrq2_prty_attn_desc[11] = {
	"pswrq2_mem004_i_ecc_rf_int",
	"pswrq2_mem005_i_ecc_rf_int",
	"pswrq2_mem001_i_ecc_rf_int",
	"pswrq2_mem006_i_mem_prty",
	"pswrq2_mem008_i_mem_prty",
	"pswrq2_mem009_i_mem_prty",
	"pswrq2_mem003_i_mem_prty",
	"pswrq2_mem002_i_mem_prty",
	"pswrq2_mem010_i_mem_prty",
	"pswrq2_mem007_i_mem_prty",
	"pswrq2_mem005_i_mem_prty",
};
#else
#define pswrq2_prty_attn_desc OSAL_NULL
#endif

static const u16 pswrq2_prty1_bb_a0_attn_idx[9] = {
	0, 2, 3, 4, 5, 6, 7, 9, 10,
};

static struct attn_hw_reg pswrq2_prty1_bb_a0 = {
	0, 9, pswrq2_prty1_bb_a0_attn_idx, 0x240200, 0x24020c, 0x240208,
	0x240204
};

static struct attn_hw_reg *pswrq2_prty_bb_a0_regs[1] = {
	&pswrq2_prty1_bb_a0,
};

static const u16 pswrq2_prty1_bb_b0_attn_idx[9] = {
	0, 2, 3, 4, 5, 6, 7, 9, 10,
};

static struct attn_hw_reg pswrq2_prty1_bb_b0 = {
	0, 9, pswrq2_prty1_bb_b0_attn_idx, 0x240200, 0x24020c, 0x240208,
	0x240204
};

static struct attn_hw_reg *pswrq2_prty_bb_b0_regs[1] = {
	&pswrq2_prty1_bb_b0,
};

static const u16 pswrq2_prty1_k2_attn_idx[10] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
};

static struct attn_hw_reg pswrq2_prty1_k2 = {
	0, 10, pswrq2_prty1_k2_attn_idx, 0x240200, 0x24020c, 0x240208, 0x240204
};

static struct attn_hw_reg *pswrq2_prty_k2_regs[1] = {
	&pswrq2_prty1_k2,
};

#ifdef ATTN_DESC
static const char *pglcs_int_attn_desc[2] = {
	"pglcs_address_error",
	"pglcs_rasdp_error",
};
#else
#define pglcs_int_attn_desc OSAL_NULL
#endif

static const u16 pglcs_int0_bb_a0_attn_idx[1] = {
	0,
};

static struct attn_hw_reg pglcs_int0_bb_a0 = {
	0, 1, pglcs_int0_bb_a0_attn_idx, 0x1d00, 0x1d0c, 0x1d08, 0x1d04
};

static struct attn_hw_reg *pglcs_int_bb_a0_regs[1] = {
	&pglcs_int0_bb_a0,
};

static const u16 pglcs_int0_bb_b0_attn_idx[1] = {
	0,
};

static struct attn_hw_reg pglcs_int0_bb_b0 = {
	0, 1, pglcs_int0_bb_b0_attn_idx, 0x1d00, 0x1d0c, 0x1d08, 0x1d04
};

static struct attn_hw_reg *pglcs_int_bb_b0_regs[1] = {
	&pglcs_int0_bb_b0,
};

static const u16 pglcs_int0_k2_attn_idx[2] = {
	0, 1,
};

static struct attn_hw_reg pglcs_int0_k2 = {
	0, 2, pglcs_int0_k2_attn_idx, 0x1d00, 0x1d0c, 0x1d08, 0x1d04
};

static struct attn_hw_reg *pglcs_int_k2_regs[1] = {
	&pglcs_int0_k2,
};

#ifdef ATTN_DESC
static const char *dmae_int_attn_desc[2] = {
	"dmae_address_error",
	"dmae_pci_rd_buf_err",
};
#else
#define dmae_int_attn_desc OSAL_NULL
#endif

static const u16 dmae_int0_bb_a0_attn_idx[2] = {
	0, 1,
};

static struct attn_hw_reg dmae_int0_bb_a0 = {
	0, 2, dmae_int0_bb_a0_attn_idx, 0xc180, 0xc18c, 0xc188, 0xc184
};

static struct attn_hw_reg *dmae_int_bb_a0_regs[1] = {
	&dmae_int0_bb_a0,
};

static const u16 dmae_int0_bb_b0_attn_idx[2] = {
	0, 1,
};

static struct attn_hw_reg dmae_int0_bb_b0 = {
	0, 2, dmae_int0_bb_b0_attn_idx, 0xc180, 0xc18c, 0xc188, 0xc184
};

static struct attn_hw_reg *dmae_int_bb_b0_regs[1] = {
	&dmae_int0_bb_b0,
};

static const u16 dmae_int0_k2_attn_idx[2] = {
	0, 1,
};

static struct attn_hw_reg dmae_int0_k2 = {
	0, 2, dmae_int0_k2_attn_idx, 0xc180, 0xc18c, 0xc188, 0xc184
};

static struct attn_hw_reg *dmae_int_k2_regs[1] = {
	&dmae_int0_k2,
};

#ifdef ATTN_DESC
static const char *dmae_prty_attn_desc[3] = {
	"dmae_mem002_i_mem_prty",
	"dmae_mem001_i_mem_prty",
	"dmae_mem003_i_mem_prty",
};
#else
#define dmae_prty_attn_desc OSAL_NULL
#endif

static const u16 dmae_prty1_bb_a0_attn_idx[3] = {
	0, 1, 2,
};

static struct attn_hw_reg dmae_prty1_bb_a0 = {
	0, 3, dmae_prty1_bb_a0_attn_idx, 0xc200, 0xc20c, 0xc208, 0xc204
};

static struct attn_hw_reg *dmae_prty_bb_a0_regs[1] = {
	&dmae_prty1_bb_a0,
};

static const u16 dmae_prty1_bb_b0_attn_idx[3] = {
	0, 1, 2,
};

static struct attn_hw_reg dmae_prty1_bb_b0 = {
	0, 3, dmae_prty1_bb_b0_attn_idx, 0xc200, 0xc20c, 0xc208, 0xc204
};

static struct attn_hw_reg *dmae_prty_bb_b0_regs[1] = {
	&dmae_prty1_bb_b0,
};

static const u16 dmae_prty1_k2_attn_idx[3] = {
	0, 1, 2,
};

static struct attn_hw_reg dmae_prty1_k2 = {
	0, 3, dmae_prty1_k2_attn_idx, 0xc200, 0xc20c, 0xc208, 0xc204
};

static struct attn_hw_reg *dmae_prty_k2_regs[1] = {
	&dmae_prty1_k2,
};

#ifdef ATTN_DESC
static const char *ptu_int_attn_desc[8] = {
	"ptu_address_error",
	"ptu_atc_tcpl_to_not_pend",
	"ptu_atc_gpa_multiple_hits",
	"ptu_atc_rcpl_to_empty_cnt",
	"ptu_atc_tcpl_error",
	"ptu_atc_inv_halt",
	"ptu_atc_reuse_transpend",
	"ptu_atc_ireq_less_than_stu",
};
#else
#define ptu_int_attn_desc OSAL_NULL
#endif

static const u16 ptu_int0_bb_a0_attn_idx[8] = {
	0, 1, 2, 3, 4, 5, 6, 7,
};

static struct attn_hw_reg ptu_int0_bb_a0 = {
	0, 8, ptu_int0_bb_a0_attn_idx, 0x560180, 0x56018c, 0x560188, 0x560184
};

static struct attn_hw_reg *ptu_int_bb_a0_regs[1] = {
	&ptu_int0_bb_a0,
};

static const u16 ptu_int0_bb_b0_attn_idx[8] = {
	0, 1, 2, 3, 4, 5, 6, 7,
};

static struct attn_hw_reg ptu_int0_bb_b0 = {
	0, 8, ptu_int0_bb_b0_attn_idx, 0x560180, 0x56018c, 0x560188, 0x560184
};

static struct attn_hw_reg *ptu_int_bb_b0_regs[1] = {
	&ptu_int0_bb_b0,
};

static const u16 ptu_int0_k2_attn_idx[8] = {
	0, 1, 2, 3, 4, 5, 6, 7,
};

static struct attn_hw_reg ptu_int0_k2 = {
	0, 8, ptu_int0_k2_attn_idx, 0x560180, 0x56018c, 0x560188, 0x560184
};

static struct attn_hw_reg *ptu_int_k2_regs[1] = {
	&ptu_int0_k2,
};

#ifdef ATTN_DESC
static const char *ptu_prty_attn_desc[18] = {
	"ptu_mem017_i_ecc_rf_int",
	"ptu_mem018_i_mem_prty",
	"ptu_mem006_i_mem_prty",
	"ptu_mem001_i_mem_prty",
	"ptu_mem002_i_mem_prty",
	"ptu_mem003_i_mem_prty",
	"ptu_mem004_i_mem_prty",
	"ptu_mem005_i_mem_prty",
	"ptu_mem009_i_mem_prty",
	"ptu_mem010_i_mem_prty",
	"ptu_mem016_i_mem_prty",
	"ptu_mem007_i_mem_prty",
	"ptu_mem015_i_mem_prty",
	"ptu_mem013_i_mem_prty",
	"ptu_mem012_i_mem_prty",
	"ptu_mem014_i_mem_prty",
	"ptu_mem011_i_mem_prty",
	"ptu_mem008_i_mem_prty",
};
#else
#define ptu_prty_attn_desc OSAL_NULL
#endif

static const u16 ptu_prty1_bb_a0_attn_idx[18] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17,
};

static struct attn_hw_reg ptu_prty1_bb_a0 = {
	0, 18, ptu_prty1_bb_a0_attn_idx, 0x560200, 0x56020c, 0x560208, 0x560204
};

static struct attn_hw_reg *ptu_prty_bb_a0_regs[1] = {
	&ptu_prty1_bb_a0,
};

static const u16 ptu_prty1_bb_b0_attn_idx[18] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17,
};

static struct attn_hw_reg ptu_prty1_bb_b0 = {
	0, 18, ptu_prty1_bb_b0_attn_idx, 0x560200, 0x56020c, 0x560208, 0x560204
};

static struct attn_hw_reg *ptu_prty_bb_b0_regs[1] = {
	&ptu_prty1_bb_b0,
};

static const u16 ptu_prty1_k2_attn_idx[18] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17,
};

static struct attn_hw_reg ptu_prty1_k2 = {
	0, 18, ptu_prty1_k2_attn_idx, 0x560200, 0x56020c, 0x560208, 0x560204
};

static struct attn_hw_reg *ptu_prty_k2_regs[1] = {
	&ptu_prty1_k2,
};

#ifdef ATTN_DESC
static const char *tcm_int_attn_desc[41] = {
	"tcm_address_error",
	"tcm_is_storm_ovfl_err",
	"tcm_is_storm_under_err",
	"tcm_is_tsdm_ovfl_err",
	"tcm_is_tsdm_under_err",
	"tcm_is_msem_ovfl_err",
	"tcm_is_msem_under_err",
	"tcm_is_ysem_ovfl_err",
	"tcm_is_ysem_under_err",
	"tcm_is_dorq_ovfl_err",
	"tcm_is_dorq_under_err",
	"tcm_is_pbf_ovfl_err",
	"tcm_is_pbf_under_err",
	"tcm_is_prs_ovfl_err",
	"tcm_is_prs_under_err",
	"tcm_is_tm_ovfl_err",
	"tcm_is_tm_under_err",
	"tcm_is_qm_p_ovfl_err",
	"tcm_is_qm_p_under_err",
	"tcm_is_qm_s_ovfl_err",
	"tcm_is_qm_s_under_err",
	"tcm_is_grc_ovfl_err0",
	"tcm_is_grc_under_err0",
	"tcm_is_grc_ovfl_err1",
	"tcm_is_grc_under_err1",
	"tcm_is_grc_ovfl_err2",
	"tcm_is_grc_under_err2",
	"tcm_is_grc_ovfl_err3",
	"tcm_is_grc_under_err3",
	"tcm_in_prcs_tbl_ovfl",
	"tcm_agg_con_data_buf_ovfl",
	"tcm_agg_con_cmd_buf_ovfl",
	"tcm_sm_con_data_buf_ovfl",
	"tcm_sm_con_cmd_buf_ovfl",
	"tcm_agg_task_data_buf_ovfl",
	"tcm_agg_task_cmd_buf_ovfl",
	"tcm_sm_task_data_buf_ovfl",
	"tcm_sm_task_cmd_buf_ovfl",
	"tcm_fi_desc_input_violate",
	"tcm_se_desc_input_violate",
	"tcm_qmreg_more4",
};
#else
#define tcm_int_attn_desc OSAL_NULL
#endif

static const u16 tcm_int0_bb_a0_attn_idx[8] = {
	0, 1, 2, 3, 4, 5, 6, 7,
};

static struct attn_hw_reg tcm_int0_bb_a0 = {
	0, 8, tcm_int0_bb_a0_attn_idx, 0x1180180, 0x118018c, 0x1180188,
	0x1180184
};

static const u16 tcm_int1_bb_a0_attn_idx[32] = {
	8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
	26,
	27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
};

static struct attn_hw_reg tcm_int1_bb_a0 = {
	1, 32, tcm_int1_bb_a0_attn_idx, 0x1180190, 0x118019c, 0x1180198,
	0x1180194
};

static const u16 tcm_int2_bb_a0_attn_idx[1] = {
	40,
};

static struct attn_hw_reg tcm_int2_bb_a0 = {
	2, 1, tcm_int2_bb_a0_attn_idx, 0x11801a0, 0x11801ac, 0x11801a8,
	0x11801a4
};

static struct attn_hw_reg *tcm_int_bb_a0_regs[3] = {
	&tcm_int0_bb_a0, &tcm_int1_bb_a0, &tcm_int2_bb_a0,
};

static const u16 tcm_int0_bb_b0_attn_idx[8] = {
	0, 1, 2, 3, 4, 5, 6, 7,
};

static struct attn_hw_reg tcm_int0_bb_b0 = {
	0, 8, tcm_int0_bb_b0_attn_idx, 0x1180180, 0x118018c, 0x1180188,
	0x1180184
};

static const u16 tcm_int1_bb_b0_attn_idx[32] = {
	8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
	26,
	27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
};

static struct attn_hw_reg tcm_int1_bb_b0 = {
	1, 32, tcm_int1_bb_b0_attn_idx, 0x1180190, 0x118019c, 0x1180198,
	0x1180194
};

static const u16 tcm_int2_bb_b0_attn_idx[1] = {
	40,
};

static struct attn_hw_reg tcm_int2_bb_b0 = {
	2, 1, tcm_int2_bb_b0_attn_idx, 0x11801a0, 0x11801ac, 0x11801a8,
	0x11801a4
};

static struct attn_hw_reg *tcm_int_bb_b0_regs[3] = {
	&tcm_int0_bb_b0, &tcm_int1_bb_b0, &tcm_int2_bb_b0,
};

static const u16 tcm_int0_k2_attn_idx[8] = {
	0, 1, 2, 3, 4, 5, 6, 7,
};

static struct attn_hw_reg tcm_int0_k2 = {
	0, 8, tcm_int0_k2_attn_idx, 0x1180180, 0x118018c, 0x1180188, 0x1180184
};

static const u16 tcm_int1_k2_attn_idx[32] = {
	8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
	26,
	27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
};

static struct attn_hw_reg tcm_int1_k2 = {
	1, 32, tcm_int1_k2_attn_idx, 0x1180190, 0x118019c, 0x1180198, 0x1180194
};

static const u16 tcm_int2_k2_attn_idx[1] = {
	40,
};

static struct attn_hw_reg tcm_int2_k2 = {
	2, 1, tcm_int2_k2_attn_idx, 0x11801a0, 0x11801ac, 0x11801a8, 0x11801a4
};

static struct attn_hw_reg *tcm_int_k2_regs[3] = {
	&tcm_int0_k2, &tcm_int1_k2, &tcm_int2_k2,
};

#ifdef ATTN_DESC
static const char *tcm_prty_attn_desc[51] = {
	"tcm_mem026_i_ecc_rf_int",
	"tcm_mem003_i_ecc_0_rf_int",
	"tcm_mem003_i_ecc_1_rf_int",
	"tcm_mem022_i_ecc_0_rf_int",
	"tcm_mem022_i_ecc_1_rf_int",
	"tcm_mem005_i_ecc_0_rf_int",
	"tcm_mem005_i_ecc_1_rf_int",
	"tcm_mem024_i_ecc_0_rf_int",
	"tcm_mem024_i_ecc_1_rf_int",
	"tcm_mem018_i_mem_prty",
	"tcm_mem019_i_mem_prty",
	"tcm_mem015_i_mem_prty",
	"tcm_mem016_i_mem_prty",
	"tcm_mem017_i_mem_prty",
	"tcm_mem010_i_mem_prty",
	"tcm_mem020_i_mem_prty",
	"tcm_mem011_i_mem_prty",
	"tcm_mem012_i_mem_prty",
	"tcm_mem013_i_mem_prty",
	"tcm_mem014_i_mem_prty",
	"tcm_mem029_i_mem_prty",
	"tcm_mem028_i_mem_prty",
	"tcm_mem027_i_mem_prty",
	"tcm_mem004_i_mem_prty",
	"tcm_mem023_i_mem_prty",
	"tcm_mem006_i_mem_prty",
	"tcm_mem025_i_mem_prty",
	"tcm_mem021_i_mem_prty",
	"tcm_mem007_i_mem_prty_0",
	"tcm_mem007_i_mem_prty_1",
	"tcm_mem008_i_mem_prty",
	"tcm_mem025_i_ecc_rf_int",
	"tcm_mem021_i_ecc_0_rf_int",
	"tcm_mem021_i_ecc_1_rf_int",
	"tcm_mem023_i_ecc_0_rf_int",
	"tcm_mem023_i_ecc_1_rf_int",
	"tcm_mem026_i_mem_prty",
	"tcm_mem022_i_mem_prty",
	"tcm_mem024_i_mem_prty",
	"tcm_mem009_i_mem_prty",
	"tcm_mem024_i_ecc_rf_int",
	"tcm_mem001_i_ecc_0_rf_int",
	"tcm_mem001_i_ecc_1_rf_int",
	"tcm_mem019_i_ecc_0_rf_int",
	"tcm_mem019_i_ecc_1_rf_int",
	"tcm_mem022_i_ecc_rf_int",
	"tcm_mem002_i_mem_prty",
	"tcm_mem005_i_mem_prty_0",
	"tcm_mem005_i_mem_prty_1",
	"tcm_mem001_i_mem_prty",
	"tcm_mem007_i_mem_prty",
};
#else
#define tcm_prty_attn_desc OSAL_NULL
#endif

static const u16 tcm_prty1_bb_a0_attn_idx[31] = {
	1, 2, 9, 11, 12, 13, 14, 15, 16, 17, 18, 19, 22, 23, 24, 25, 26, 30, 32,
	33, 36, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
};

static struct attn_hw_reg tcm_prty1_bb_a0 = {
	0, 31, tcm_prty1_bb_a0_attn_idx, 0x1180200, 0x118020c, 0x1180208,
	0x1180204
};

static const u16 tcm_prty2_bb_a0_attn_idx[3] = {
	50, 21, 20,
};

static struct attn_hw_reg tcm_prty2_bb_a0 = {
	1, 3, tcm_prty2_bb_a0_attn_idx, 0x1180210, 0x118021c, 0x1180218,
	0x1180214
};

static struct attn_hw_reg *tcm_prty_bb_a0_regs[2] = {
	&tcm_prty1_bb_a0, &tcm_prty2_bb_a0,
};

static const u16 tcm_prty1_bb_b0_attn_idx[31] = {
	1, 2, 5, 6, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25,
	28,
	29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
};

static struct attn_hw_reg tcm_prty1_bb_b0 = {
	0, 31, tcm_prty1_bb_b0_attn_idx, 0x1180200, 0x118020c, 0x1180208,
	0x1180204
};

static const u16 tcm_prty2_bb_b0_attn_idx[2] = {
	49, 46,
};

static struct attn_hw_reg tcm_prty2_bb_b0 = {
	1, 2, tcm_prty2_bb_b0_attn_idx, 0x1180210, 0x118021c, 0x1180218,
	0x1180214
};

static struct attn_hw_reg *tcm_prty_bb_b0_regs[2] = {
	&tcm_prty1_bb_b0, &tcm_prty2_bb_b0,
};

static const u16 tcm_prty1_k2_attn_idx[31] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
};

static struct attn_hw_reg tcm_prty1_k2 = {
	0, 31, tcm_prty1_k2_attn_idx, 0x1180200, 0x118020c, 0x1180208,
	0x1180204
};

static const u16 tcm_prty2_k2_attn_idx[3] = {
	39, 49, 46,
};

static struct attn_hw_reg tcm_prty2_k2 = {
	1, 3, tcm_prty2_k2_attn_idx, 0x1180210, 0x118021c, 0x1180218, 0x1180214
};

static struct attn_hw_reg *tcm_prty_k2_regs[2] = {
	&tcm_prty1_k2, &tcm_prty2_k2,
};

#ifdef ATTN_DESC
static const char *mcm_int_attn_desc[41] = {
	"mcm_address_error",
	"mcm_is_storm_ovfl_err",
	"mcm_is_storm_under_err",
	"mcm_is_msdm_ovfl_err",
	"mcm_is_msdm_under_err",
	"mcm_is_ysdm_ovfl_err",
	"mcm_is_ysdm_under_err",
	"mcm_is_usdm_ovfl_err",
	"mcm_is_usdm_under_err",
	"mcm_is_tmld_ovfl_err",
	"mcm_is_tmld_under_err",
	"mcm_is_usem_ovfl_err",
	"mcm_is_usem_under_err",
	"mcm_is_ysem_ovfl_err",
	"mcm_is_ysem_under_err",
	"mcm_is_pbf_ovfl_err",
	"mcm_is_pbf_under_err",
	"mcm_is_qm_p_ovfl_err",
	"mcm_is_qm_p_under_err",
	"mcm_is_qm_s_ovfl_err",
	"mcm_is_qm_s_under_err",
	"mcm_is_grc_ovfl_err0",
	"mcm_is_grc_under_err0",
	"mcm_is_grc_ovfl_err1",
	"mcm_is_grc_under_err1",
	"mcm_is_grc_ovfl_err2",
	"mcm_is_grc_under_err2",
	"mcm_is_grc_ovfl_err3",
	"mcm_is_grc_under_err3",
	"mcm_in_prcs_tbl_ovfl",
	"mcm_agg_con_data_buf_ovfl",
	"mcm_agg_con_cmd_buf_ovfl",
	"mcm_sm_con_data_buf_ovfl",
	"mcm_sm_con_cmd_buf_ovfl",
	"mcm_agg_task_data_buf_ovfl",
	"mcm_agg_task_cmd_buf_ovfl",
	"mcm_sm_task_data_buf_ovfl",
	"mcm_sm_task_cmd_buf_ovfl",
	"mcm_fi_desc_input_violate",
	"mcm_se_desc_input_violate",
	"mcm_qmreg_more4",
};
#else
#define mcm_int_attn_desc OSAL_NULL
#endif

static const u16 mcm_int0_bb_a0_attn_idx[14] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
};

static struct attn_hw_reg mcm_int0_bb_a0 = {
	0, 14, mcm_int0_bb_a0_attn_idx, 0x1200180, 0x120018c, 0x1200188,
	0x1200184
};

static const u16 mcm_int1_bb_a0_attn_idx[26] = {
	14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
	32, 33, 34, 35, 36, 37, 38, 39,
};

static struct attn_hw_reg mcm_int1_bb_a0 = {
	1, 26, mcm_int1_bb_a0_attn_idx, 0x1200190, 0x120019c, 0x1200198,
	0x1200194
};

static const u16 mcm_int2_bb_a0_attn_idx[1] = {
	40,
};

static struct attn_hw_reg mcm_int2_bb_a0 = {
	2, 1, mcm_int2_bb_a0_attn_idx, 0x12001a0, 0x12001ac, 0x12001a8,
	0x12001a4
};

static struct attn_hw_reg *mcm_int_bb_a0_regs[3] = {
	&mcm_int0_bb_a0, &mcm_int1_bb_a0, &mcm_int2_bb_a0,
};

static const u16 mcm_int0_bb_b0_attn_idx[14] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
};

static struct attn_hw_reg mcm_int0_bb_b0 = {
	0, 14, mcm_int0_bb_b0_attn_idx, 0x1200180, 0x120018c, 0x1200188,
	0x1200184
};

static const u16 mcm_int1_bb_b0_attn_idx[26] = {
	14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
	32, 33, 34, 35, 36, 37, 38, 39,
};

static struct attn_hw_reg mcm_int1_bb_b0 = {
	1, 26, mcm_int1_bb_b0_attn_idx, 0x1200190, 0x120019c, 0x1200198,
	0x1200194
};

static const u16 mcm_int2_bb_b0_attn_idx[1] = {
	40,
};

static struct attn_hw_reg mcm_int2_bb_b0 = {
	2, 1, mcm_int2_bb_b0_attn_idx, 0x12001a0, 0x12001ac, 0x12001a8,
	0x12001a4
};

static struct attn_hw_reg *mcm_int_bb_b0_regs[3] = {
	&mcm_int0_bb_b0, &mcm_int1_bb_b0, &mcm_int2_bb_b0,
};

static const u16 mcm_int0_k2_attn_idx[14] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
};

static struct attn_hw_reg mcm_int0_k2 = {
	0, 14, mcm_int0_k2_attn_idx, 0x1200180, 0x120018c, 0x1200188, 0x1200184
};

static const u16 mcm_int1_k2_attn_idx[26] = {
	14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
	32, 33, 34, 35, 36, 37, 38, 39,
};

static struct attn_hw_reg mcm_int1_k2 = {
	1, 26, mcm_int1_k2_attn_idx, 0x1200190, 0x120019c, 0x1200198, 0x1200194
};

static const u16 mcm_int2_k2_attn_idx[1] = {
	40,
};

static struct attn_hw_reg mcm_int2_k2 = {
	2, 1, mcm_int2_k2_attn_idx, 0x12001a0, 0x12001ac, 0x12001a8, 0x12001a4
};

static struct attn_hw_reg *mcm_int_k2_regs[3] = {
	&mcm_int0_k2, &mcm_int1_k2, &mcm_int2_k2,
};

#ifdef ATTN_DESC
static const char *mcm_prty_attn_desc[46] = {
	"mcm_mem028_i_ecc_rf_int",
	"mcm_mem003_i_ecc_rf_int",
	"mcm_mem023_i_ecc_0_rf_int",
	"mcm_mem023_i_ecc_1_rf_int",
	"mcm_mem005_i_ecc_0_rf_int",
	"mcm_mem005_i_ecc_1_rf_int",
	"mcm_mem025_i_ecc_0_rf_int",
	"mcm_mem025_i_ecc_1_rf_int",
	"mcm_mem026_i_ecc_rf_int",
	"mcm_mem017_i_mem_prty",
	"mcm_mem019_i_mem_prty",
	"mcm_mem016_i_mem_prty",
	"mcm_mem015_i_mem_prty",
	"mcm_mem020_i_mem_prty",
	"mcm_mem021_i_mem_prty",
	"mcm_mem018_i_mem_prty",
	"mcm_mem011_i_mem_prty",
	"mcm_mem012_i_mem_prty",
	"mcm_mem013_i_mem_prty",
	"mcm_mem014_i_mem_prty",
	"mcm_mem031_i_mem_prty",
	"mcm_mem030_i_mem_prty",
	"mcm_mem029_i_mem_prty",
	"mcm_mem004_i_mem_prty",
	"mcm_mem024_i_mem_prty",
	"mcm_mem006_i_mem_prty",
	"mcm_mem027_i_mem_prty",
	"mcm_mem022_i_mem_prty",
	"mcm_mem007_i_mem_prty_0",
	"mcm_mem007_i_mem_prty_1",
	"mcm_mem008_i_mem_prty",
	"mcm_mem001_i_ecc_rf_int",
	"mcm_mem021_i_ecc_0_rf_int",
	"mcm_mem021_i_ecc_1_rf_int",
	"mcm_mem003_i_ecc_0_rf_int",
	"mcm_mem003_i_ecc_1_rf_int",
	"mcm_mem024_i_ecc_rf_int",
	"mcm_mem009_i_mem_prty",
	"mcm_mem010_i_mem_prty",
	"mcm_mem028_i_mem_prty",
	"mcm_mem002_i_mem_prty",
	"mcm_mem025_i_mem_prty",
	"mcm_mem005_i_mem_prty_0",
	"mcm_mem005_i_mem_prty_1",
	"mcm_mem001_i_mem_prty",
	"mcm_mem007_i_mem_prty",
};
#else
#define mcm_prty_attn_desc OSAL_NULL
#endif

static const u16 mcm_prty1_bb_a0_attn_idx[31] = {
	2, 3, 8, 9, 10, 11, 12, 13, 15, 16, 17, 18, 19, 22, 23, 25, 26, 27, 31,
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43,
};

static struct attn_hw_reg mcm_prty1_bb_a0 = {
	0, 31, mcm_prty1_bb_a0_attn_idx, 0x1200200, 0x120020c, 0x1200208,
	0x1200204
};

static const u16 mcm_prty2_bb_a0_attn_idx[4] = {
	45, 30, 21, 20,
};

static struct attn_hw_reg mcm_prty2_bb_a0 = {
	1, 4, mcm_prty2_bb_a0_attn_idx, 0x1200210, 0x120021c, 0x1200218,
	0x1200214
};

static struct attn_hw_reg *mcm_prty_bb_a0_regs[2] = {
	&mcm_prty1_bb_a0, &mcm_prty2_bb_a0,
};

static const u16 mcm_prty1_bb_b0_attn_idx[31] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
};

static struct attn_hw_reg mcm_prty1_bb_b0 = {
	0, 31, mcm_prty1_bb_b0_attn_idx, 0x1200200, 0x120020c, 0x1200208,
	0x1200204
};

static const u16 mcm_prty2_bb_b0_attn_idx[4] = {
	37, 38, 44, 40,
};

static struct attn_hw_reg mcm_prty2_bb_b0 = {
	1, 4, mcm_prty2_bb_b0_attn_idx, 0x1200210, 0x120021c, 0x1200218,
	0x1200214
};

static struct attn_hw_reg *mcm_prty_bb_b0_regs[2] = {
	&mcm_prty1_bb_b0, &mcm_prty2_bb_b0,
};

static const u16 mcm_prty1_k2_attn_idx[31] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
};

static struct attn_hw_reg mcm_prty1_k2 = {
	0, 31, mcm_prty1_k2_attn_idx, 0x1200200, 0x120020c, 0x1200208,
	0x1200204
};

static const u16 mcm_prty2_k2_attn_idx[4] = {
	37, 38, 44, 40,
};

static struct attn_hw_reg mcm_prty2_k2 = {
	1, 4, mcm_prty2_k2_attn_idx, 0x1200210, 0x120021c, 0x1200218, 0x1200214
};

static struct attn_hw_reg *mcm_prty_k2_regs[2] = {
	&mcm_prty1_k2, &mcm_prty2_k2,
};

#ifdef ATTN_DESC
static const char *ucm_int_attn_desc[47] = {
	"ucm_address_error",
	"ucm_is_storm_ovfl_err",
	"ucm_is_storm_under_err",
	"ucm_is_xsdm_ovfl_err",
	"ucm_is_xsdm_under_err",
	"ucm_is_ysdm_ovfl_err",
	"ucm_is_ysdm_under_err",
	"ucm_is_usdm_ovfl_err",
	"ucm_is_usdm_under_err",
	"ucm_is_rdif_ovfl_err",
	"ucm_is_rdif_under_err",
	"ucm_is_tdif_ovfl_err",
	"ucm_is_tdif_under_err",
	"ucm_is_muld_ovfl_err",
	"ucm_is_muld_under_err",
	"ucm_is_yuld_ovfl_err",
	"ucm_is_yuld_under_err",
	"ucm_is_dorq_ovfl_err",
	"ucm_is_dorq_under_err",
	"ucm_is_pbf_ovfl_err",
	"ucm_is_pbf_under_err",
	"ucm_is_tm_ovfl_err",
	"ucm_is_tm_under_err",
	"ucm_is_qm_p_ovfl_err",
	"ucm_is_qm_p_under_err",
	"ucm_is_qm_s_ovfl_err",
	"ucm_is_qm_s_under_err",
	"ucm_is_grc_ovfl_err0",
	"ucm_is_grc_under_err0",
	"ucm_is_grc_ovfl_err1",
	"ucm_is_grc_under_err1",
	"ucm_is_grc_ovfl_err2",
	"ucm_is_grc_under_err2",
	"ucm_is_grc_ovfl_err3",
	"ucm_is_grc_under_err3",
	"ucm_in_prcs_tbl_ovfl",
	"ucm_agg_con_data_buf_ovfl",
	"ucm_agg_con_cmd_buf_ovfl",
	"ucm_sm_con_data_buf_ovfl",
	"ucm_sm_con_cmd_buf_ovfl",
	"ucm_agg_task_data_buf_ovfl",
	"ucm_agg_task_cmd_buf_ovfl",
	"ucm_sm_task_data_buf_ovfl",
	"ucm_sm_task_cmd_buf_ovfl",
	"ucm_fi_desc_input_violate",
	"ucm_se_desc_input_violate",
	"ucm_qmreg_more4",
};
#else
#define ucm_int_attn_desc OSAL_NULL
#endif

static const u16 ucm_int0_bb_a0_attn_idx[17] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
};

static struct attn_hw_reg ucm_int0_bb_a0 = {
	0, 17, ucm_int0_bb_a0_attn_idx, 0x1280180, 0x128018c, 0x1280188,
	0x1280184
};

static const u16 ucm_int1_bb_a0_attn_idx[29] = {
	17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34,
	35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45,
};

static struct attn_hw_reg ucm_int1_bb_a0 = {
	1, 29, ucm_int1_bb_a0_attn_idx, 0x1280190, 0x128019c, 0x1280198,
	0x1280194
};

static const u16 ucm_int2_bb_a0_attn_idx[1] = {
	46,
};

static struct attn_hw_reg ucm_int2_bb_a0 = {
	2, 1, ucm_int2_bb_a0_attn_idx, 0x12801a0, 0x12801ac, 0x12801a8,
	0x12801a4
};

static struct attn_hw_reg *ucm_int_bb_a0_regs[3] = {
	&ucm_int0_bb_a0, &ucm_int1_bb_a0, &ucm_int2_bb_a0,
};

static const u16 ucm_int0_bb_b0_attn_idx[17] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
};

static struct attn_hw_reg ucm_int0_bb_b0 = {
	0, 17, ucm_int0_bb_b0_attn_idx, 0x1280180, 0x128018c, 0x1280188,
	0x1280184
};

static const u16 ucm_int1_bb_b0_attn_idx[29] = {
	17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34,
	35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45,
};

static struct attn_hw_reg ucm_int1_bb_b0 = {
	1, 29, ucm_int1_bb_b0_attn_idx, 0x1280190, 0x128019c, 0x1280198,
	0x1280194
};

static const u16 ucm_int2_bb_b0_attn_idx[1] = {
	46,
};

static struct attn_hw_reg ucm_int2_bb_b0 = {
	2, 1, ucm_int2_bb_b0_attn_idx, 0x12801a0, 0x12801ac, 0x12801a8,
	0x12801a4
};

static struct attn_hw_reg *ucm_int_bb_b0_regs[3] = {
	&ucm_int0_bb_b0, &ucm_int1_bb_b0, &ucm_int2_bb_b0,
};

static const u16 ucm_int0_k2_attn_idx[17] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
};

static struct attn_hw_reg ucm_int0_k2 = {
	0, 17, ucm_int0_k2_attn_idx, 0x1280180, 0x128018c, 0x1280188, 0x1280184
};

static const u16 ucm_int1_k2_attn_idx[29] = {
	17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34,
	35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45,
};

static struct attn_hw_reg ucm_int1_k2 = {
	1, 29, ucm_int1_k2_attn_idx, 0x1280190, 0x128019c, 0x1280198, 0x1280194
};

static const u16 ucm_int2_k2_attn_idx[1] = {
	46,
};

static struct attn_hw_reg ucm_int2_k2 = {
	2, 1, ucm_int2_k2_attn_idx, 0x12801a0, 0x12801ac, 0x12801a8, 0x12801a4
};

static struct attn_hw_reg *ucm_int_k2_regs[3] = {
	&ucm_int0_k2, &ucm_int1_k2, &ucm_int2_k2,
};

#ifdef ATTN_DESC
static const char *ucm_prty_attn_desc[54] = {
	"ucm_mem030_i_ecc_rf_int",
	"ucm_mem005_i_ecc_0_rf_int",
	"ucm_mem005_i_ecc_1_rf_int",
	"ucm_mem024_i_ecc_0_rf_int",
	"ucm_mem024_i_ecc_1_rf_int",
	"ucm_mem025_i_ecc_rf_int",
	"ucm_mem007_i_ecc_0_rf_int",
	"ucm_mem007_i_ecc_1_rf_int",
	"ucm_mem008_i_ecc_rf_int",
	"ucm_mem027_i_ecc_0_rf_int",
	"ucm_mem027_i_ecc_1_rf_int",
	"ucm_mem028_i_ecc_rf_int",
	"ucm_mem020_i_mem_prty",
	"ucm_mem021_i_mem_prty",
	"ucm_mem019_i_mem_prty",
	"ucm_mem013_i_mem_prty",
	"ucm_mem018_i_mem_prty",
	"ucm_mem022_i_mem_prty",
	"ucm_mem014_i_mem_prty",
	"ucm_mem015_i_mem_prty",
	"ucm_mem016_i_mem_prty",
	"ucm_mem017_i_mem_prty",
	"ucm_mem033_i_mem_prty",
	"ucm_mem032_i_mem_prty",
	"ucm_mem031_i_mem_prty",
	"ucm_mem006_i_mem_prty",
	"ucm_mem026_i_mem_prty",
	"ucm_mem009_i_mem_prty",
	"ucm_mem029_i_mem_prty",
	"ucm_mem023_i_mem_prty",
	"ucm_mem010_i_mem_prty_0",
	"ucm_mem003_i_ecc_0_rf_int",
	"ucm_mem003_i_ecc_1_rf_int",
	"ucm_mem022_i_ecc_0_rf_int",
	"ucm_mem022_i_ecc_1_rf_int",
	"ucm_mem023_i_ecc_rf_int",
	"ucm_mem006_i_ecc_rf_int",
	"ucm_mem025_i_ecc_0_rf_int",
	"ucm_mem025_i_ecc_1_rf_int",
	"ucm_mem026_i_ecc_rf_int",
	"ucm_mem011_i_mem_prty",
	"ucm_mem012_i_mem_prty",
	"ucm_mem030_i_mem_prty",
	"ucm_mem004_i_mem_prty",
	"ucm_mem024_i_mem_prty",
	"ucm_mem007_i_mem_prty",
	"ucm_mem027_i_mem_prty",
	"ucm_mem008_i_mem_prty_0",
	"ucm_mem010_i_mem_prty_1",
	"ucm_mem003_i_mem_prty",
	"ucm_mem001_i_mem_prty",
	"ucm_mem002_i_mem_prty",
	"ucm_mem008_i_mem_prty_1",
	"ucm_mem010_i_mem_prty",
};
#else
#define ucm_prty_attn_desc OSAL_NULL
#endif

static const u16 ucm_prty1_bb_a0_attn_idx[31] = {
	1, 2, 11, 12, 13, 14, 15, 16, 18, 19, 20, 21, 24, 28, 31, 32, 33, 34,
	35,
	36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
};

static struct attn_hw_reg ucm_prty1_bb_a0 = {
	0, 31, ucm_prty1_bb_a0_attn_idx, 0x1280200, 0x128020c, 0x1280208,
	0x1280204
};

static const u16 ucm_prty2_bb_a0_attn_idx[7] = {
	50, 51, 52, 27, 53, 23, 22,
};

static struct attn_hw_reg ucm_prty2_bb_a0 = {
	1, 7, ucm_prty2_bb_a0_attn_idx, 0x1280210, 0x128021c, 0x1280218,
	0x1280214
};

static struct attn_hw_reg *ucm_prty_bb_a0_regs[2] = {
	&ucm_prty1_bb_a0, &ucm_prty2_bb_a0,
};

static const u16 ucm_prty1_bb_b0_attn_idx[31] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
};

static struct attn_hw_reg ucm_prty1_bb_b0 = {
	0, 31, ucm_prty1_bb_b0_attn_idx, 0x1280200, 0x128020c, 0x1280208,
	0x1280204
};

static const u16 ucm_prty2_bb_b0_attn_idx[7] = {
	48, 40, 41, 49, 43, 50, 51,
};

static struct attn_hw_reg ucm_prty2_bb_b0 = {
	1, 7, ucm_prty2_bb_b0_attn_idx, 0x1280210, 0x128021c, 0x1280218,
	0x1280214
};

static struct attn_hw_reg *ucm_prty_bb_b0_regs[2] = {
	&ucm_prty1_bb_b0, &ucm_prty2_bb_b0,
};

static const u16 ucm_prty1_k2_attn_idx[31] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
};

static struct attn_hw_reg ucm_prty1_k2 = {
	0, 31, ucm_prty1_k2_attn_idx, 0x1280200, 0x128020c, 0x1280208,
	0x1280204
};

static const u16 ucm_prty2_k2_attn_idx[7] = {
	48, 40, 41, 49, 43, 50, 51,
};

static struct attn_hw_reg ucm_prty2_k2 = {
	1, 7, ucm_prty2_k2_attn_idx, 0x1280210, 0x128021c, 0x1280218, 0x1280214
};

static struct attn_hw_reg *ucm_prty_k2_regs[2] = {
	&ucm_prty1_k2, &ucm_prty2_k2,
};

#ifdef ATTN_DESC
static const char *xcm_int_attn_desc[49] = {
	"xcm_address_error",
	"xcm_is_storm_ovfl_err",
	"xcm_is_storm_under_err",
	"xcm_is_msdm_ovfl_err",
	"xcm_is_msdm_under_err",
	"xcm_is_xsdm_ovfl_err",
	"xcm_is_xsdm_under_err",
	"xcm_is_ysdm_ovfl_err",
	"xcm_is_ysdm_under_err",
	"xcm_is_usdm_ovfl_err",
	"xcm_is_usdm_under_err",
	"xcm_is_msem_ovfl_err",
	"xcm_is_msem_under_err",
	"xcm_is_usem_ovfl_err",
	"xcm_is_usem_under_err",
	"xcm_is_ysem_ovfl_err",
	"xcm_is_ysem_under_err",
	"xcm_is_dorq_ovfl_err",
	"xcm_is_dorq_under_err",
	"xcm_is_pbf_ovfl_err",
	"xcm_is_pbf_under_err",
	"xcm_is_tm_ovfl_err",
	"xcm_is_tm_under_err",
	"xcm_is_qm_p_ovfl_err",
	"xcm_is_qm_p_under_err",
	"xcm_is_qm_s_ovfl_err",
	"xcm_is_qm_s_under_err",
	"xcm_is_grc_ovfl_err0",
	"xcm_is_grc_under_err0",
	"xcm_is_grc_ovfl_err1",
	"xcm_is_grc_under_err1",
	"xcm_is_grc_ovfl_err2",
	"xcm_is_grc_under_err2",
	"xcm_is_grc_ovfl_err3",
	"xcm_is_grc_under_err3",
	"xcm_in_prcs_tbl_ovfl",
	"xcm_agg_con_data_buf_ovfl",
	"xcm_agg_con_cmd_buf_ovfl",
	"xcm_sm_con_data_buf_ovfl",
	"xcm_sm_con_cmd_buf_ovfl",
	"xcm_fi_desc_input_violate",
	"xcm_qm_act_st_cnt_msg_prcs_under",
	"xcm_qm_act_st_cnt_msg_prcs_ovfl",
	"xcm_qm_act_st_cnt_ext_ld_under",
	"xcm_qm_act_st_cnt_ext_ld_ovfl",
	"xcm_qm_act_st_cnt_rbc_under",
	"xcm_qm_act_st_cnt_rbc_ovfl",
	"xcm_qm_act_st_cnt_drop_under",
	"xcm_qm_act_st_cnt_illeg_pqnum",
};
#else
#define xcm_int_attn_desc OSAL_NULL
#endif

static const u16 xcm_int0_bb_a0_attn_idx[16] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
};

static struct attn_hw_reg xcm_int0_bb_a0 = {
	0, 16, xcm_int0_bb_a0_attn_idx, 0x1000180, 0x100018c, 0x1000188,
	0x1000184
};

static const u16 xcm_int1_bb_a0_attn_idx[25] = {
	16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33,
	34, 35, 36, 37, 38, 39, 40,
};

static struct attn_hw_reg xcm_int1_bb_a0 = {
	1, 25, xcm_int1_bb_a0_attn_idx, 0x1000190, 0x100019c, 0x1000198,
	0x1000194
};

static const u16 xcm_int2_bb_a0_attn_idx[8] = {
	41, 42, 43, 44, 45, 46, 47, 48,
};

static struct attn_hw_reg xcm_int2_bb_a0 = {
	2, 8, xcm_int2_bb_a0_attn_idx, 0x10001a0, 0x10001ac, 0x10001a8,
	0x10001a4
};

static struct attn_hw_reg *xcm_int_bb_a0_regs[3] = {
	&xcm_int0_bb_a0, &xcm_int1_bb_a0, &xcm_int2_bb_a0,
};

static const u16 xcm_int0_bb_b0_attn_idx[16] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
};

static struct attn_hw_reg xcm_int0_bb_b0 = {
	0, 16, xcm_int0_bb_b0_attn_idx, 0x1000180, 0x100018c, 0x1000188,
	0x1000184
};

static const u16 xcm_int1_bb_b0_attn_idx[25] = {
	16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33,
	34, 35, 36, 37, 38, 39, 40,
};

static struct attn_hw_reg xcm_int1_bb_b0 = {
	1, 25, xcm_int1_bb_b0_attn_idx, 0x1000190, 0x100019c, 0x1000198,
	0x1000194
};

static const u16 xcm_int2_bb_b0_attn_idx[8] = {
	41, 42, 43, 44, 45, 46, 47, 48,
};

static struct attn_hw_reg xcm_int2_bb_b0 = {
	2, 8, xcm_int2_bb_b0_attn_idx, 0x10001a0, 0x10001ac, 0x10001a8,
	0x10001a4
};

static struct attn_hw_reg *xcm_int_bb_b0_regs[3] = {
	&xcm_int0_bb_b0, &xcm_int1_bb_b0, &xcm_int2_bb_b0,
};

static const u16 xcm_int0_k2_attn_idx[16] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
};

static struct attn_hw_reg xcm_int0_k2 = {
	0, 16, xcm_int0_k2_attn_idx, 0x1000180, 0x100018c, 0x1000188, 0x1000184
};

static const u16 xcm_int1_k2_attn_idx[25] = {
	16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33,
	34, 35, 36, 37, 38, 39, 40,
};

static struct attn_hw_reg xcm_int1_k2 = {
	1, 25, xcm_int1_k2_attn_idx, 0x1000190, 0x100019c, 0x1000198, 0x1000194
};

static const u16 xcm_int2_k2_attn_idx[8] = {
	41, 42, 43, 44, 45, 46, 47, 48,
};

static struct attn_hw_reg xcm_int2_k2 = {
	2, 8, xcm_int2_k2_attn_idx, 0x10001a0, 0x10001ac, 0x10001a8, 0x10001a4
};

static struct attn_hw_reg *xcm_int_k2_regs[3] = {
	&xcm_int0_k2, &xcm_int1_k2, &xcm_int2_k2,
};

#ifdef ATTN_DESC
static const char *xcm_prty_attn_desc[59] = {
	"xcm_mem036_i_ecc_rf_int",
	"xcm_mem003_i_ecc_0_rf_int",
	"xcm_mem003_i_ecc_1_rf_int",
	"xcm_mem003_i_ecc_2_rf_int",
	"xcm_mem003_i_ecc_3_rf_int",
	"xcm_mem004_i_ecc_rf_int",
	"xcm_mem033_i_ecc_0_rf_int",
	"xcm_mem033_i_ecc_1_rf_int",
	"xcm_mem034_i_ecc_rf_int",
	"xcm_mem026_i_mem_prty",
	"xcm_mem025_i_mem_prty",
	"xcm_mem022_i_mem_prty",
	"xcm_mem029_i_mem_prty",
	"xcm_mem023_i_mem_prty",
	"xcm_mem028_i_mem_prty",
	"xcm_mem030_i_mem_prty",
	"xcm_mem017_i_mem_prty",
	"xcm_mem024_i_mem_prty",
	"xcm_mem027_i_mem_prty",
	"xcm_mem018_i_mem_prty",
	"xcm_mem019_i_mem_prty",
	"xcm_mem020_i_mem_prty",
	"xcm_mem021_i_mem_prty",
	"xcm_mem039_i_mem_prty",
	"xcm_mem038_i_mem_prty",
	"xcm_mem037_i_mem_prty",
	"xcm_mem005_i_mem_prty",
	"xcm_mem035_i_mem_prty",
	"xcm_mem031_i_mem_prty",
	"xcm_mem006_i_mem_prty",
	"xcm_mem015_i_mem_prty",
	"xcm_mem035_i_ecc_rf_int",
	"xcm_mem032_i_ecc_0_rf_int",
	"xcm_mem032_i_ecc_1_rf_int",
	"xcm_mem033_i_ecc_rf_int",
	"xcm_mem036_i_mem_prty",
	"xcm_mem034_i_mem_prty",
	"xcm_mem016_i_mem_prty",
	"xcm_mem002_i_ecc_0_rf_int",
	"xcm_mem002_i_ecc_1_rf_int",
	"xcm_mem002_i_ecc_2_rf_int",
	"xcm_mem002_i_ecc_3_rf_int",
	"xcm_mem003_i_ecc_rf_int",
	"xcm_mem031_i_ecc_0_rf_int",
	"xcm_mem031_i_ecc_1_rf_int",
	"xcm_mem032_i_ecc_rf_int",
	"xcm_mem004_i_mem_prty",
	"xcm_mem033_i_mem_prty",
	"xcm_mem014_i_mem_prty",
	"xcm_mem032_i_mem_prty",
	"xcm_mem007_i_mem_prty",
	"xcm_mem008_i_mem_prty",
	"xcm_mem009_i_mem_prty",
	"xcm_mem010_i_mem_prty",
	"xcm_mem011_i_mem_prty",
	"xcm_mem012_i_mem_prty",
	"xcm_mem013_i_mem_prty",
	"xcm_mem001_i_mem_prty",
	"xcm_mem002_i_mem_prty",
};
#else
#define xcm_prty_attn_desc OSAL_NULL
#endif

static const u16 xcm_prty1_bb_a0_attn_idx[31] = {
	8, 9, 10, 11, 12, 13, 14, 16, 17, 18, 19, 20, 21, 22, 25, 26, 27, 30,
	35,
	37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
};

static struct attn_hw_reg xcm_prty1_bb_a0 = {
	0, 31, xcm_prty1_bb_a0_attn_idx, 0x1000200, 0x100020c, 0x1000208,
	0x1000204
};

static const u16 xcm_prty2_bb_a0_attn_idx[11] = {
	50, 51, 52, 53, 54, 55, 56, 57, 15, 29, 24,
};

static struct attn_hw_reg xcm_prty2_bb_a0 = {
	1, 11, xcm_prty2_bb_a0_attn_idx, 0x1000210, 0x100021c, 0x1000218,
	0x1000214
};

static struct attn_hw_reg *xcm_prty_bb_a0_regs[2] = {
	&xcm_prty1_bb_a0, &xcm_prty2_bb_a0,
};

static const u16 xcm_prty1_bb_b0_attn_idx[31] = {
	1, 2, 3, 4, 5, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
	24,
	25, 26, 29, 30, 31, 32, 33, 34, 35, 36, 37,
};

static struct attn_hw_reg xcm_prty1_bb_b0 = {
	0, 31, xcm_prty1_bb_b0_attn_idx, 0x1000200, 0x100020c, 0x1000208,
	0x1000204
};

static const u16 xcm_prty2_bb_b0_attn_idx[11] = {
	50, 51, 52, 53, 54, 55, 56, 48, 57, 58, 28,
};

static struct attn_hw_reg xcm_prty2_bb_b0 = {
	1, 11, xcm_prty2_bb_b0_attn_idx, 0x1000210, 0x100021c, 0x1000218,
	0x1000214
};

static struct attn_hw_reg *xcm_prty_bb_b0_regs[2] = {
	&xcm_prty1_bb_b0, &xcm_prty2_bb_b0,
};

static const u16 xcm_prty1_k2_attn_idx[31] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
};

static struct attn_hw_reg xcm_prty1_k2 = {
	0, 31, xcm_prty1_k2_attn_idx, 0x1000200, 0x100020c, 0x1000208,
	0x1000204
};

static const u16 xcm_prty2_k2_attn_idx[12] = {
	37, 49, 50, 51, 52, 53, 54, 55, 56, 48, 57, 58,
};

static struct attn_hw_reg xcm_prty2_k2 = {
	1, 12, xcm_prty2_k2_attn_idx, 0x1000210, 0x100021c, 0x1000218,
	0x1000214
};

static struct attn_hw_reg *xcm_prty_k2_regs[2] = {
	&xcm_prty1_k2, &xcm_prty2_k2,
};

#ifdef ATTN_DESC
static const char *ycm_int_attn_desc[37] = {
	"ycm_address_error",
	"ycm_is_storm_ovfl_err",
	"ycm_is_storm_under_err",
	"ycm_is_msdm_ovfl_err",
	"ycm_is_msdm_under_err",
	"ycm_is_ysdm_ovfl_err",
	"ycm_is_ysdm_under_err",
	"ycm_is_xyld_ovfl_err",
	"ycm_is_xyld_under_err",
	"ycm_is_msem_ovfl_err",
	"ycm_is_msem_under_err",
	"ycm_is_usem_ovfl_err",
	"ycm_is_usem_under_err",
	"ycm_is_pbf_ovfl_err",
	"ycm_is_pbf_under_err",
	"ycm_is_qm_p_ovfl_err",
	"ycm_is_qm_p_under_err",
	"ycm_is_qm_s_ovfl_err",
	"ycm_is_qm_s_under_err",
	"ycm_is_grc_ovfl_err0",
	"ycm_is_grc_under_err0",
	"ycm_is_grc_ovfl_err1",
	"ycm_is_grc_under_err1",
	"ycm_is_grc_ovfl_err2",
	"ycm_is_grc_under_err2",
	"ycm_is_grc_ovfl_err3",
	"ycm_is_grc_under_err3",
	"ycm_in_prcs_tbl_ovfl",
	"ycm_sm_con_data_buf_ovfl",
	"ycm_sm_con_cmd_buf_ovfl",
	"ycm_agg_task_data_buf_ovfl",
	"ycm_agg_task_cmd_buf_ovfl",
	"ycm_sm_task_data_buf_ovfl",
	"ycm_sm_task_cmd_buf_ovfl",
	"ycm_fi_desc_input_violate",
	"ycm_se_desc_input_violate",
	"ycm_qmreg_more4",
};
#else
#define ycm_int_attn_desc OSAL_NULL
#endif

static const u16 ycm_int0_bb_a0_attn_idx[13] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
};

static struct attn_hw_reg ycm_int0_bb_a0 = {
	0, 13, ycm_int0_bb_a0_attn_idx, 0x1080180, 0x108018c, 0x1080188,
	0x1080184
};

static const u16 ycm_int1_bb_a0_attn_idx[23] = {
	13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
	31, 32, 33, 34, 35,
};

static struct attn_hw_reg ycm_int1_bb_a0 = {
	1, 23, ycm_int1_bb_a0_attn_idx, 0x1080190, 0x108019c, 0x1080198,
	0x1080194
};

static const u16 ycm_int2_bb_a0_attn_idx[1] = {
	36,
};

static struct attn_hw_reg ycm_int2_bb_a0 = {
	2, 1, ycm_int2_bb_a0_attn_idx, 0x10801a0, 0x10801ac, 0x10801a8,
	0x10801a4
};

static struct attn_hw_reg *ycm_int_bb_a0_regs[3] = {
	&ycm_int0_bb_a0, &ycm_int1_bb_a0, &ycm_int2_bb_a0,
};

static const u16 ycm_int0_bb_b0_attn_idx[13] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
};

static struct attn_hw_reg ycm_int0_bb_b0 = {
	0, 13, ycm_int0_bb_b0_attn_idx, 0x1080180, 0x108018c, 0x1080188,
	0x1080184
};

static const u16 ycm_int1_bb_b0_attn_idx[23] = {
	13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
	31, 32, 33, 34, 35,
};

static struct attn_hw_reg ycm_int1_bb_b0 = {
	1, 23, ycm_int1_bb_b0_attn_idx, 0x1080190, 0x108019c, 0x1080198,
	0x1080194
};

static const u16 ycm_int2_bb_b0_attn_idx[1] = {
	36,
};

static struct attn_hw_reg ycm_int2_bb_b0 = {
	2, 1, ycm_int2_bb_b0_attn_idx, 0x10801a0, 0x10801ac, 0x10801a8,
	0x10801a4
};

static struct attn_hw_reg *ycm_int_bb_b0_regs[3] = {
	&ycm_int0_bb_b0, &ycm_int1_bb_b0, &ycm_int2_bb_b0,
};

static const u16 ycm_int0_k2_attn_idx[13] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
};

static struct attn_hw_reg ycm_int0_k2 = {
	0, 13, ycm_int0_k2_attn_idx, 0x1080180, 0x108018c, 0x1080188, 0x1080184
};

static const u16 ycm_int1_k2_attn_idx[23] = {
	13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
	31, 32, 33, 34, 35,
};

static struct attn_hw_reg ycm_int1_k2 = {
	1, 23, ycm_int1_k2_attn_idx, 0x1080190, 0x108019c, 0x1080198, 0x1080194
};

static const u16 ycm_int2_k2_attn_idx[1] = {
	36,
};

static struct attn_hw_reg ycm_int2_k2 = {
	2, 1, ycm_int2_k2_attn_idx, 0x10801a0, 0x10801ac, 0x10801a8, 0x10801a4
};

static struct attn_hw_reg *ycm_int_k2_regs[3] = {
	&ycm_int0_k2, &ycm_int1_k2, &ycm_int2_k2,
};

#ifdef ATTN_DESC
static const char *ycm_prty_attn_desc[44] = {
	"ycm_mem027_i_ecc_rf_int",
	"ycm_mem003_i_ecc_0_rf_int",
	"ycm_mem003_i_ecc_1_rf_int",
	"ycm_mem022_i_ecc_0_rf_int",
	"ycm_mem022_i_ecc_1_rf_int",
	"ycm_mem023_i_ecc_rf_int",
	"ycm_mem005_i_ecc_0_rf_int",
	"ycm_mem005_i_ecc_1_rf_int",
	"ycm_mem025_i_ecc_0_rf_int",
	"ycm_mem025_i_ecc_1_rf_int",
	"ycm_mem018_i_mem_prty",
	"ycm_mem020_i_mem_prty",
	"ycm_mem017_i_mem_prty",
	"ycm_mem016_i_mem_prty",
	"ycm_mem019_i_mem_prty",
	"ycm_mem015_i_mem_prty",
	"ycm_mem011_i_mem_prty",
	"ycm_mem012_i_mem_prty",
	"ycm_mem013_i_mem_prty",
	"ycm_mem014_i_mem_prty",
	"ycm_mem030_i_mem_prty",
	"ycm_mem029_i_mem_prty",
	"ycm_mem028_i_mem_prty",
	"ycm_mem004_i_mem_prty",
	"ycm_mem024_i_mem_prty",
	"ycm_mem006_i_mem_prty",
	"ycm_mem026_i_mem_prty",
	"ycm_mem021_i_mem_prty",
	"ycm_mem007_i_mem_prty_0",
	"ycm_mem007_i_mem_prty_1",
	"ycm_mem008_i_mem_prty",
	"ycm_mem026_i_ecc_rf_int",
	"ycm_mem021_i_ecc_0_rf_int",
	"ycm_mem021_i_ecc_1_rf_int",
	"ycm_mem022_i_ecc_rf_int",
	"ycm_mem024_i_ecc_0_rf_int",
	"ycm_mem024_i_ecc_1_rf_int",
	"ycm_mem027_i_mem_prty",
	"ycm_mem023_i_mem_prty",
	"ycm_mem025_i_mem_prty",
	"ycm_mem009_i_mem_prty",
	"ycm_mem010_i_mem_prty",
	"ycm_mem001_i_mem_prty",
	"ycm_mem002_i_mem_prty",
};
#else
#define ycm_prty_attn_desc OSAL_NULL
#endif

static const u16 ycm_prty1_bb_a0_attn_idx[31] = {
	1, 2, 6, 7, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 28,
	29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
};

static struct attn_hw_reg ycm_prty1_bb_a0 = {
	0, 31, ycm_prty1_bb_a0_attn_idx, 0x1080200, 0x108020c, 0x1080208,
	0x1080204
};

static const u16 ycm_prty2_bb_a0_attn_idx[3] = {
	41, 42, 43,
};

static struct attn_hw_reg ycm_prty2_bb_a0 = {
	1, 3, ycm_prty2_bb_a0_attn_idx, 0x1080210, 0x108021c, 0x1080218,
	0x1080214
};

static struct attn_hw_reg *ycm_prty_bb_a0_regs[2] = {
	&ycm_prty1_bb_a0, &ycm_prty2_bb_a0,
};

static const u16 ycm_prty1_bb_b0_attn_idx[31] = {
	1, 2, 6, 7, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 28,
	29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
};

static struct attn_hw_reg ycm_prty1_bb_b0 = {
	0, 31, ycm_prty1_bb_b0_attn_idx, 0x1080200, 0x108020c, 0x1080208,
	0x1080204
};

static const u16 ycm_prty2_bb_b0_attn_idx[3] = {
	41, 42, 43,
};

static struct attn_hw_reg ycm_prty2_bb_b0 = {
	1, 3, ycm_prty2_bb_b0_attn_idx, 0x1080210, 0x108021c, 0x1080218,
	0x1080214
};

static struct attn_hw_reg *ycm_prty_bb_b0_regs[2] = {
	&ycm_prty1_bb_b0, &ycm_prty2_bb_b0,
};

static const u16 ycm_prty1_k2_attn_idx[31] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
};

static struct attn_hw_reg ycm_prty1_k2 = {
	0, 31, ycm_prty1_k2_attn_idx, 0x1080200, 0x108020c, 0x1080208,
	0x1080204
};

static const u16 ycm_prty2_k2_attn_idx[4] = {
	40, 41, 42, 43,
};

static struct attn_hw_reg ycm_prty2_k2 = {
	1, 4, ycm_prty2_k2_attn_idx, 0x1080210, 0x108021c, 0x1080218, 0x1080214
};

static struct attn_hw_reg *ycm_prty_k2_regs[2] = {
	&ycm_prty1_k2, &ycm_prty2_k2,
};

#ifdef ATTN_DESC
static const char *pcm_int_attn_desc[20] = {
	"pcm_address_error",
	"pcm_is_storm_ovfl_err",
	"pcm_is_storm_under_err",
	"pcm_is_psdm_ovfl_err",
	"pcm_is_psdm_under_err",
	"pcm_is_pbf_ovfl_err",
	"pcm_is_pbf_under_err",
	"pcm_is_grc_ovfl_err0",
	"pcm_is_grc_under_err0",
	"pcm_is_grc_ovfl_err1",
	"pcm_is_grc_under_err1",
	"pcm_is_grc_ovfl_err2",
	"pcm_is_grc_under_err2",
	"pcm_is_grc_ovfl_err3",
	"pcm_is_grc_under_err3",
	"pcm_in_prcs_tbl_ovfl",
	"pcm_sm_con_data_buf_ovfl",
	"pcm_sm_con_cmd_buf_ovfl",
	"pcm_fi_desc_input_violate",
	"pcm_qmreg_more4",
};
#else
#define pcm_int_attn_desc OSAL_NULL
#endif

static const u16 pcm_int0_bb_a0_attn_idx[5] = {
	0, 1, 2, 3, 4,
};

static struct attn_hw_reg pcm_int0_bb_a0 = {
	0, 5, pcm_int0_bb_a0_attn_idx, 0x1100180, 0x110018c, 0x1100188,
	0x1100184
};

static const u16 pcm_int1_bb_a0_attn_idx[14] = {
	5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
};

static struct attn_hw_reg pcm_int1_bb_a0 = {
	1, 14, pcm_int1_bb_a0_attn_idx, 0x1100190, 0x110019c, 0x1100198,
	0x1100194
};

static const u16 pcm_int2_bb_a0_attn_idx[1] = {
	19,
};

static struct attn_hw_reg pcm_int2_bb_a0 = {
	2, 1, pcm_int2_bb_a0_attn_idx, 0x11001a0, 0x11001ac, 0x11001a8,
	0x11001a4
};

static struct attn_hw_reg *pcm_int_bb_a0_regs[3] = {
	&pcm_int0_bb_a0, &pcm_int1_bb_a0, &pcm_int2_bb_a0,
};

static const u16 pcm_int0_bb_b0_attn_idx[5] = {
	0, 1, 2, 3, 4,
};

static struct attn_hw_reg pcm_int0_bb_b0 = {
	0, 5, pcm_int0_bb_b0_attn_idx, 0x1100180, 0x110018c, 0x1100188,
	0x1100184
};

static const u16 pcm_int1_bb_b0_attn_idx[14] = {
	5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
};

static struct attn_hw_reg pcm_int1_bb_b0 = {
	1, 14, pcm_int1_bb_b0_attn_idx, 0x1100190, 0x110019c, 0x1100198,
	0x1100194
};

static const u16 pcm_int2_bb_b0_attn_idx[1] = {
	19,
};

static struct attn_hw_reg pcm_int2_bb_b0 = {
	2, 1, pcm_int2_bb_b0_attn_idx, 0x11001a0, 0x11001ac, 0x11001a8,
	0x11001a4
};

static struct attn_hw_reg *pcm_int_bb_b0_regs[3] = {
	&pcm_int0_bb_b0, &pcm_int1_bb_b0, &pcm_int2_bb_b0,
};

static const u16 pcm_int0_k2_attn_idx[5] = {
	0, 1, 2, 3, 4,
};

static struct attn_hw_reg pcm_int0_k2 = {
	0, 5, pcm_int0_k2_attn_idx, 0x1100180, 0x110018c, 0x1100188, 0x1100184
};

static const u16 pcm_int1_k2_attn_idx[14] = {
	5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
};

static struct attn_hw_reg pcm_int1_k2 = {
	1, 14, pcm_int1_k2_attn_idx, 0x1100190, 0x110019c, 0x1100198, 0x1100194
};

static const u16 pcm_int2_k2_attn_idx[1] = {
	19,
};

static struct attn_hw_reg pcm_int2_k2 = {
	2, 1, pcm_int2_k2_attn_idx, 0x11001a0, 0x11001ac, 0x11001a8, 0x11001a4
};

static struct attn_hw_reg *pcm_int_k2_regs[3] = {
	&pcm_int0_k2, &pcm_int1_k2, &pcm_int2_k2,
};

#ifdef ATTN_DESC
static const char *pcm_prty_attn_desc[18] = {
	"pcm_mem012_i_ecc_rf_int",
	"pcm_mem010_i_ecc_0_rf_int",
	"pcm_mem010_i_ecc_1_rf_int",
	"pcm_mem008_i_mem_prty",
	"pcm_mem007_i_mem_prty",
	"pcm_mem006_i_mem_prty",
	"pcm_mem002_i_mem_prty",
	"pcm_mem003_i_mem_prty",
	"pcm_mem004_i_mem_prty",
	"pcm_mem005_i_mem_prty",
	"pcm_mem011_i_mem_prty",
	"pcm_mem001_i_mem_prty",
	"pcm_mem011_i_ecc_rf_int",
	"pcm_mem009_i_ecc_0_rf_int",
	"pcm_mem009_i_ecc_1_rf_int",
	"pcm_mem010_i_mem_prty",
	"pcm_mem013_i_mem_prty",
	"pcm_mem012_i_mem_prty",
};
#else
#define pcm_prty_attn_desc OSAL_NULL
#endif

static const u16 pcm_prty1_bb_a0_attn_idx[14] = {
	3, 4, 5, 6, 7, 8, 9, 11, 12, 13, 14, 15, 16, 17,
};

static struct attn_hw_reg pcm_prty1_bb_a0 = {
	0, 14, pcm_prty1_bb_a0_attn_idx, 0x1100200, 0x110020c, 0x1100208,
	0x1100204
};

static struct attn_hw_reg *pcm_prty_bb_a0_regs[1] = {
	&pcm_prty1_bb_a0,
};

static const u16 pcm_prty1_bb_b0_attn_idx[11] = {
	4, 5, 6, 7, 8, 9, 11, 12, 13, 14, 15,
};

static struct attn_hw_reg pcm_prty1_bb_b0 = {
	0, 11, pcm_prty1_bb_b0_attn_idx, 0x1100200, 0x110020c, 0x1100208,
	0x1100204
};

static struct attn_hw_reg *pcm_prty_bb_b0_regs[1] = {
	&pcm_prty1_bb_b0,
};

static const u16 pcm_prty1_k2_attn_idx[12] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
};

static struct attn_hw_reg pcm_prty1_k2 = {
	0, 12, pcm_prty1_k2_attn_idx, 0x1100200, 0x110020c, 0x1100208,
	0x1100204
};

static struct attn_hw_reg *pcm_prty_k2_regs[1] = {
	&pcm_prty1_k2,
};

#ifdef ATTN_DESC
static const char *qm_int_attn_desc[22] = {
	"qm_address_error",
	"qm_ovf_err_tx",
	"qm_ovf_err_other",
	"qm_pf_usg_cnt_err",
	"qm_vf_usg_cnt_err",
	"qm_voq_crd_inc_err",
	"qm_voq_crd_dec_err",
	"qm_byte_crd_inc_err",
	"qm_byte_crd_dec_err",
	"qm_err_incdec_rlglblcrd",
	"qm_err_incdec_rlpfcrd",
	"qm_err_incdec_wfqpfcrd",
	"qm_err_incdec_wfqvpcrd",
	"qm_err_incdec_voqlinecrd",
	"qm_err_incdec_voqbytecrd",
	"qm_fifos_error",
	"qm_qm_rl_dc_exp_pf_controller_pop_error",
	"qm_qm_rl_dc_exp_pf_controller_push_error",
	"qm_qm_rl_dc_rf_req_controller_pop_error",
	"qm_qm_rl_dc_rf_req_controller_push_error",
	"qm_qm_rl_dc_rf_res_controller_pop_error",
	"qm_qm_rl_dc_rf_res_controller_push_error",
};
#else
#define qm_int_attn_desc OSAL_NULL
#endif

static const u16 qm_int0_bb_a0_attn_idx[16] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
};

static struct attn_hw_reg qm_int0_bb_a0 = {
	0, 16, qm_int0_bb_a0_attn_idx, 0x2f0180, 0x2f018c, 0x2f0188, 0x2f0184
};

static struct attn_hw_reg *qm_int_bb_a0_regs[1] = {
	&qm_int0_bb_a0,
};

static const u16 qm_int0_bb_b0_attn_idx[22] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21,
};

static struct attn_hw_reg qm_int0_bb_b0 = {
	0, 22, qm_int0_bb_b0_attn_idx, 0x2f0180, 0x2f018c, 0x2f0188, 0x2f0184
};

static struct attn_hw_reg *qm_int_bb_b0_regs[1] = {
	&qm_int0_bb_b0,
};

static const u16 qm_int0_k2_attn_idx[22] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21,
};

static struct attn_hw_reg qm_int0_k2 = {
	0, 22, qm_int0_k2_attn_idx, 0x2f0180, 0x2f018c, 0x2f0188, 0x2f0184
};

static struct attn_hw_reg *qm_int_k2_regs[1] = {
	&qm_int0_k2,
};

#ifdef ATTN_DESC
static const char *qm_prty_attn_desc[109] = {
	"qm_xcm_wrc_fifo",
	"qm_ucm_wrc_fifo",
	"qm_tcm_wrc_fifo",
	"qm_ccm_wrc_fifo",
	"qm_bigramhigh",
	"qm_bigramlow",
	"qm_base_address",
	"qm_wrbuff",
	"qm_bigramhigh_ext_a",
	"qm_bigramlow_ext_a",
	"qm_base_address_ext_a",
	"qm_mem006_i_ecc_0_rf_int",
	"qm_mem006_i_ecc_1_rf_int",
	"qm_mem005_i_ecc_0_rf_int",
	"qm_mem005_i_ecc_1_rf_int",
	"qm_mem012_i_ecc_rf_int",
	"qm_mem037_i_mem_prty",
	"qm_mem036_i_mem_prty",
	"qm_mem039_i_mem_prty",
	"qm_mem038_i_mem_prty",
	"qm_mem040_i_mem_prty",
	"qm_mem042_i_mem_prty",
	"qm_mem041_i_mem_prty",
	"qm_mem056_i_mem_prty",
	"qm_mem055_i_mem_prty",
	"qm_mem053_i_mem_prty",
	"qm_mem054_i_mem_prty",
	"qm_mem057_i_mem_prty",
	"qm_mem058_i_mem_prty",
	"qm_mem062_i_mem_prty",
	"qm_mem061_i_mem_prty",
	"qm_mem059_i_mem_prty",
	"qm_mem060_i_mem_prty",
	"qm_mem063_i_mem_prty",
	"qm_mem064_i_mem_prty",
	"qm_mem033_i_mem_prty",
	"qm_mem032_i_mem_prty",
	"qm_mem030_i_mem_prty",
	"qm_mem031_i_mem_prty",
	"qm_mem034_i_mem_prty",
	"qm_mem035_i_mem_prty",
	"qm_mem051_i_mem_prty",
	"qm_mem042_i_ecc_0_rf_int",
	"qm_mem042_i_ecc_1_rf_int",
	"qm_mem041_i_ecc_0_rf_int",
	"qm_mem041_i_ecc_1_rf_int",
	"qm_mem048_i_ecc_rf_int",
	"qm_mem009_i_mem_prty",
	"qm_mem008_i_mem_prty",
	"qm_mem011_i_mem_prty",
	"qm_mem010_i_mem_prty",
	"qm_mem012_i_mem_prty",
	"qm_mem014_i_mem_prty",
	"qm_mem013_i_mem_prty",
	"qm_mem028_i_mem_prty",
	"qm_mem027_i_mem_prty",
	"qm_mem025_i_mem_prty",
	"qm_mem026_i_mem_prty",
	"qm_mem029_i_mem_prty",
	"qm_mem005_i_mem_prty",
	"qm_mem004_i_mem_prty",
	"qm_mem002_i_mem_prty",
	"qm_mem003_i_mem_prty",
	"qm_mem006_i_mem_prty",
	"qm_mem007_i_mem_prty",
	"qm_mem023_i_mem_prty",
	"qm_mem047_i_mem_prty",
	"qm_mem049_i_mem_prty",
	"qm_mem048_i_mem_prty",
	"qm_mem052_i_mem_prty",
	"qm_mem050_i_mem_prty",
	"qm_mem045_i_mem_prty",
	"qm_mem046_i_mem_prty",
	"qm_mem043_i_mem_prty",
	"qm_mem044_i_mem_prty",
	"qm_mem017_i_mem_prty",
	"qm_mem016_i_mem_prty",
	"qm_mem021_i_mem_prty",
	"qm_mem024_i_mem_prty",
	"qm_mem019_i_mem_prty",
	"qm_mem018_i_mem_prty",
	"qm_mem015_i_mem_prty",
	"qm_mem022_i_mem_prty",
	"qm_mem020_i_mem_prty",
	"qm_mem007_i_mem_prty_0",
	"qm_mem007_i_mem_prty_1",
	"qm_mem007_i_mem_prty_2",
	"qm_mem001_i_mem_prty",
	"qm_mem043_i_mem_prty_0",
	"qm_mem043_i_mem_prty_1",
	"qm_mem043_i_mem_prty_2",
	"qm_mem007_i_mem_prty_3",
	"qm_mem007_i_mem_prty_4",
	"qm_mem007_i_mem_prty_5",
	"qm_mem007_i_mem_prty_6",
	"qm_mem007_i_mem_prty_7",
	"qm_mem007_i_mem_prty_8",
	"qm_mem007_i_mem_prty_9",
	"qm_mem007_i_mem_prty_10",
	"qm_mem007_i_mem_prty_11",
	"qm_mem007_i_mem_prty_12",
	"qm_mem007_i_mem_prty_13",
	"qm_mem007_i_mem_prty_14",
	"qm_mem007_i_mem_prty_15",
	"qm_mem043_i_mem_prty_3",
	"qm_mem043_i_mem_prty_4",
	"qm_mem043_i_mem_prty_5",
	"qm_mem043_i_mem_prty_6",
	"qm_mem043_i_mem_prty_7",
};
#else
#define qm_prty_attn_desc OSAL_NULL
#endif

static const u16 qm_prty0_bb_a0_attn_idx[11] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
};

static struct attn_hw_reg qm_prty0_bb_a0 = {
	0, 11, qm_prty0_bb_a0_attn_idx, 0x2f0190, 0x2f019c, 0x2f0198, 0x2f0194
};

static const u16 qm_prty1_bb_a0_attn_idx[31] = {
	17, 35, 36, 37, 38, 39, 40, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52,
	53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65,
};

static struct attn_hw_reg qm_prty1_bb_a0 = {
	1, 31, qm_prty1_bb_a0_attn_idx, 0x2f0200, 0x2f020c, 0x2f0208, 0x2f0204
};

static const u16 qm_prty2_bb_a0_attn_idx[31] = {
	66, 67, 69, 70, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 87, 20, 18, 25,
	27, 32, 24, 26, 41, 31, 29, 28, 30, 23, 88, 89, 90,
};

static struct attn_hw_reg qm_prty2_bb_a0 = {
	2, 31, qm_prty2_bb_a0_attn_idx, 0x2f0210, 0x2f021c, 0x2f0218, 0x2f0214
};

static const u16 qm_prty3_bb_a0_attn_idx[11] = {
	104, 105, 106, 107, 108, 33, 16, 34, 19, 72, 71,
};

static struct attn_hw_reg qm_prty3_bb_a0 = {
	3, 11, qm_prty3_bb_a0_attn_idx, 0x2f0220, 0x2f022c, 0x2f0228, 0x2f0224
};

static struct attn_hw_reg *qm_prty_bb_a0_regs[4] = {
	&qm_prty0_bb_a0, &qm_prty1_bb_a0, &qm_prty2_bb_a0, &qm_prty3_bb_a0,
};

static const u16 qm_prty0_bb_b0_attn_idx[11] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
};

static struct attn_hw_reg qm_prty0_bb_b0 = {
	0, 11, qm_prty0_bb_b0_attn_idx, 0x2f0190, 0x2f019c, 0x2f0198, 0x2f0194
};

static const u16 qm_prty1_bb_b0_attn_idx[31] = {
	11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28,
	29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41,
};

static struct attn_hw_reg qm_prty1_bb_b0 = {
	1, 31, qm_prty1_bb_b0_attn_idx, 0x2f0200, 0x2f020c, 0x2f0208, 0x2f0204
};

static const u16 qm_prty2_bb_b0_attn_idx[31] = {
	66, 67, 68, 69, 70, 71, 72, 73, 74, 58, 60, 62, 49, 75, 76, 53, 77, 78,
	79, 80, 81, 52, 65, 57, 82, 56, 83, 48, 84, 85, 86,
};

static struct attn_hw_reg qm_prty2_bb_b0 = {
	2, 31, qm_prty2_bb_b0_attn_idx, 0x2f0210, 0x2f021c, 0x2f0218, 0x2f0214
};

static const u16 qm_prty3_bb_b0_attn_idx[11] = {
	91, 92, 93, 94, 95, 55, 87, 54, 61, 50, 47,
};

static struct attn_hw_reg qm_prty3_bb_b0 = {
	3, 11, qm_prty3_bb_b0_attn_idx, 0x2f0220, 0x2f022c, 0x2f0228, 0x2f0224
};

static struct attn_hw_reg *qm_prty_bb_b0_regs[4] = {
	&qm_prty0_bb_b0, &qm_prty1_bb_b0, &qm_prty2_bb_b0, &qm_prty3_bb_b0,
};

static const u16 qm_prty0_k2_attn_idx[11] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
};

static struct attn_hw_reg qm_prty0_k2 = {
	0, 11, qm_prty0_k2_attn_idx, 0x2f0190, 0x2f019c, 0x2f0198, 0x2f0194
};

static const u16 qm_prty1_k2_attn_idx[31] = {
	11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28,
	29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41,
};

static struct attn_hw_reg qm_prty1_k2 = {
	1, 31, qm_prty1_k2_attn_idx, 0x2f0200, 0x2f020c, 0x2f0208, 0x2f0204
};

static const u16 qm_prty2_k2_attn_idx[31] = {
	66, 67, 68, 69, 70, 71, 72, 73, 74, 58, 60, 62, 49, 75, 76, 53, 77, 78,
	79, 80, 81, 52, 65, 57, 82, 56, 83, 48, 84, 85, 86,
};

static struct attn_hw_reg qm_prty2_k2 = {
	2, 31, qm_prty2_k2_attn_idx, 0x2f0210, 0x2f021c, 0x2f0218, 0x2f0214
};

static const u16 qm_prty3_k2_attn_idx[19] = {
	91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 55, 87, 54, 61,
	50, 47,
};

static struct attn_hw_reg qm_prty3_k2 = {
	3, 19, qm_prty3_k2_attn_idx, 0x2f0220, 0x2f022c, 0x2f0228, 0x2f0224
};

static struct attn_hw_reg *qm_prty_k2_regs[4] = {
	&qm_prty0_k2, &qm_prty1_k2, &qm_prty2_k2, &qm_prty3_k2,
};

#ifdef ATTN_DESC
static const char *tm_int_attn_desc[43] = {
	"tm_address_error",
	"tm_pxp_read_data_fifo_ov",
	"tm_pxp_read_data_fifo_un",
	"tm_pxp_read_ctrl_fifo_ov",
	"tm_pxp_read_ctrl_fifo_un",
	"tm_cfc_load_command_fifo_ov",
	"tm_cfc_load_command_fifo_un",
	"tm_cfc_load_echo_fifo_ov",
	"tm_cfc_load_echo_fifo_un",
	"tm_client_out_fifo_ov",
	"tm_client_out_fifo_un",
	"tm_ac_command_fifo_ov",
	"tm_ac_command_fifo_un",
	"tm_client_in_pbf_fifo_ov",
	"tm_client_in_pbf_fifo_un",
	"tm_client_in_ucm_fifo_ov",
	"tm_client_in_ucm_fifo_un",
	"tm_client_in_tcm_fifo_ov",
	"tm_client_in_tcm_fifo_un",
	"tm_client_in_xcm_fifo_ov",
	"tm_client_in_xcm_fifo_un",
	"tm_expiration_cmd_fifo_ov",
	"tm_expiration_cmd_fifo_un",
	"tm_stop_all_lc_invalid",
	"tm_command_lc_invalid_0",
	"tm_command_lc_invalid_1",
	"tm_init_command_lc_valid",
	"tm_stop_all_exp_lc_valid",
	"tm_command_cid_invalid_0",
	"tm_reserved_command",
	"tm_command_cid_invalid_1",
	"tm_cload_res_loaderr_conn",
	"tm_cload_res_loadcancel_conn",
	"tm_cload_res_validerr_conn",
	"tm_context_rd_last",
	"tm_context_wr_last",
	"tm_pxp_rd_data_eop_bvalid",
	"tm_pend_conn_scan",
	"tm_pend_task_scan",
	"tm_pxp_rd_data_eop_error",
	"tm_cload_res_loaderr_task",
	"tm_cload_res_loadcancel_task",
	"tm_cload_res_validerr_task",
};
#else
#define tm_int_attn_desc OSAL_NULL
#endif

static const u16 tm_int0_bb_a0_attn_idx[32] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
};

static struct attn_hw_reg tm_int0_bb_a0 = {
	0, 32, tm_int0_bb_a0_attn_idx, 0x2c0180, 0x2c018c, 0x2c0188, 0x2c0184
};

static const u16 tm_int1_bb_a0_attn_idx[11] = {
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42,
};

static struct attn_hw_reg tm_int1_bb_a0 = {
	1, 11, tm_int1_bb_a0_attn_idx, 0x2c0190, 0x2c019c, 0x2c0198, 0x2c0194
};

static struct attn_hw_reg *tm_int_bb_a0_regs[2] = {
	&tm_int0_bb_a0, &tm_int1_bb_a0,
};

static const u16 tm_int0_bb_b0_attn_idx[32] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
};

static struct attn_hw_reg tm_int0_bb_b0 = {
	0, 32, tm_int0_bb_b0_attn_idx, 0x2c0180, 0x2c018c, 0x2c0188, 0x2c0184
};

static const u16 tm_int1_bb_b0_attn_idx[11] = {
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42,
};

static struct attn_hw_reg tm_int1_bb_b0 = {
	1, 11, tm_int1_bb_b0_attn_idx, 0x2c0190, 0x2c019c, 0x2c0198, 0x2c0194
};

static struct attn_hw_reg *tm_int_bb_b0_regs[2] = {
	&tm_int0_bb_b0, &tm_int1_bb_b0,
};

static const u16 tm_int0_k2_attn_idx[32] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
};

static struct attn_hw_reg tm_int0_k2 = {
	0, 32, tm_int0_k2_attn_idx, 0x2c0180, 0x2c018c, 0x2c0188, 0x2c0184
};

static const u16 tm_int1_k2_attn_idx[11] = {
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42,
};

static struct attn_hw_reg tm_int1_k2 = {
	1, 11, tm_int1_k2_attn_idx, 0x2c0190, 0x2c019c, 0x2c0198, 0x2c0194
};

static struct attn_hw_reg *tm_int_k2_regs[2] = {
	&tm_int0_k2, &tm_int1_k2,
};

#ifdef ATTN_DESC
static const char *tm_prty_attn_desc[17] = {
	"tm_mem012_i_ecc_0_rf_int",
	"tm_mem012_i_ecc_1_rf_int",
	"tm_mem003_i_ecc_rf_int",
	"tm_mem016_i_mem_prty",
	"tm_mem007_i_mem_prty",
	"tm_mem010_i_mem_prty",
	"tm_mem008_i_mem_prty",
	"tm_mem009_i_mem_prty",
	"tm_mem013_i_mem_prty",
	"tm_mem015_i_mem_prty",
	"tm_mem014_i_mem_prty",
	"tm_mem004_i_mem_prty",
	"tm_mem005_i_mem_prty",
	"tm_mem006_i_mem_prty",
	"tm_mem011_i_mem_prty",
	"tm_mem001_i_mem_prty",
	"tm_mem002_i_mem_prty",
};
#else
#define tm_prty_attn_desc OSAL_NULL
#endif

static const u16 tm_prty1_bb_a0_attn_idx[17] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
};

static struct attn_hw_reg tm_prty1_bb_a0 = {
	0, 17, tm_prty1_bb_a0_attn_idx, 0x2c0200, 0x2c020c, 0x2c0208, 0x2c0204
};

static struct attn_hw_reg *tm_prty_bb_a0_regs[1] = {
	&tm_prty1_bb_a0,
};

static const u16 tm_prty1_bb_b0_attn_idx[17] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
};

static struct attn_hw_reg tm_prty1_bb_b0 = {
	0, 17, tm_prty1_bb_b0_attn_idx, 0x2c0200, 0x2c020c, 0x2c0208, 0x2c0204
};

static struct attn_hw_reg *tm_prty_bb_b0_regs[1] = {
	&tm_prty1_bb_b0,
};

static const u16 tm_prty1_k2_attn_idx[17] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
};

static struct attn_hw_reg tm_prty1_k2 = {
	0, 17, tm_prty1_k2_attn_idx, 0x2c0200, 0x2c020c, 0x2c0208, 0x2c0204
};

static struct attn_hw_reg *tm_prty_k2_regs[1] = {
	&tm_prty1_k2,
};

#ifdef ATTN_DESC
static const char *dorq_int_attn_desc[9] = {
	"dorq_address_error",
	"dorq_db_drop",
	"dorq_dorq_fifo_ovfl_err",
	"dorq_dorq_fifo_afull",
	"dorq_cfc_byp_validation_err",
	"dorq_cfc_ld_resp_err",
	"dorq_xcm_done_cnt_err",
	"dorq_cfc_ld_req_fifo_ovfl_err",
	"dorq_cfc_ld_req_fifo_under_err",
};
#else
#define dorq_int_attn_desc OSAL_NULL
#endif

static const u16 dorq_int0_bb_a0_attn_idx[9] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8,
};

static struct attn_hw_reg dorq_int0_bb_a0 = {
	0, 9, dorq_int0_bb_a0_attn_idx, 0x100180, 0x10018c, 0x100188, 0x100184
};

static struct attn_hw_reg *dorq_int_bb_a0_regs[1] = {
	&dorq_int0_bb_a0,
};

static const u16 dorq_int0_bb_b0_attn_idx[9] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8,
};

static struct attn_hw_reg dorq_int0_bb_b0 = {
	0, 9, dorq_int0_bb_b0_attn_idx, 0x100180, 0x10018c, 0x100188, 0x100184
};

static struct attn_hw_reg *dorq_int_bb_b0_regs[1] = {
	&dorq_int0_bb_b0,
};

static const u16 dorq_int0_k2_attn_idx[9] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8,
};

static struct attn_hw_reg dorq_int0_k2 = {
	0, 9, dorq_int0_k2_attn_idx, 0x100180, 0x10018c, 0x100188, 0x100184
};

static struct attn_hw_reg *dorq_int_k2_regs[1] = {
	&dorq_int0_k2,
};

#ifdef ATTN_DESC
static const char *dorq_prty_attn_desc[7] = {
	"dorq_datapath_registers",
	"dorq_mem002_i_ecc_rf_int",
	"dorq_mem001_i_mem_prty",
	"dorq_mem003_i_mem_prty",
	"dorq_mem004_i_mem_prty",
	"dorq_mem005_i_mem_prty",
	"dorq_mem006_i_mem_prty",
};
#else
#define dorq_prty_attn_desc OSAL_NULL
#endif

static const u16 dorq_prty1_bb_a0_attn_idx[6] = {
	1, 2, 3, 4, 5, 6,
};

static struct attn_hw_reg dorq_prty1_bb_a0 = {
	0, 6, dorq_prty1_bb_a0_attn_idx, 0x100200, 0x10020c, 0x100208, 0x100204
};

static struct attn_hw_reg *dorq_prty_bb_a0_regs[1] = {
	&dorq_prty1_bb_a0,
};

static const u16 dorq_prty0_bb_b0_attn_idx[1] = {
	0,
};

static struct attn_hw_reg dorq_prty0_bb_b0 = {
	0, 1, dorq_prty0_bb_b0_attn_idx, 0x100190, 0x10019c, 0x100198, 0x100194
};

static const u16 dorq_prty1_bb_b0_attn_idx[6] = {
	1, 2, 3, 4, 5, 6,
};

static struct attn_hw_reg dorq_prty1_bb_b0 = {
	1, 6, dorq_prty1_bb_b0_attn_idx, 0x100200, 0x10020c, 0x100208, 0x100204
};

static struct attn_hw_reg *dorq_prty_bb_b0_regs[2] = {
	&dorq_prty0_bb_b0, &dorq_prty1_bb_b0,
};

static const u16 dorq_prty0_k2_attn_idx[1] = {
	0,
};

static struct attn_hw_reg dorq_prty0_k2 = {
	0, 1, dorq_prty0_k2_attn_idx, 0x100190, 0x10019c, 0x100198, 0x100194
};

static const u16 dorq_prty1_k2_attn_idx[6] = {
	1, 2, 3, 4, 5, 6,
};

static struct attn_hw_reg dorq_prty1_k2 = {
	1, 6, dorq_prty1_k2_attn_idx, 0x100200, 0x10020c, 0x100208, 0x100204
};

static struct attn_hw_reg *dorq_prty_k2_regs[2] = {
	&dorq_prty0_k2, &dorq_prty1_k2,
};

#ifdef ATTN_DESC
static const char *brb_int_attn_desc[237] = {
	"brb_address_error",
	"brb_rc_pkt0_rls_error",
	"brb_rc_pkt0_1st_error",
	"brb_rc_pkt0_len_error",
	"brb_rc_pkt0_middle_error",
	"brb_rc_pkt0_protocol_error",
	"brb_rc_pkt1_rls_error",
	"brb_rc_pkt1_1st_error",
	"brb_rc_pkt1_len_error",
	"brb_rc_pkt1_middle_error",
	"brb_rc_pkt1_protocol_error",
	"brb_rc_pkt2_rls_error",
	"brb_rc_pkt2_1st_error",
	"brb_rc_pkt2_len_error",
	"brb_rc_pkt2_middle_error",
	"brb_rc_pkt2_protocol_error",
	"brb_rc_pkt3_rls_error",
	"brb_rc_pkt3_1st_error",
	"brb_rc_pkt3_len_error",
	"brb_rc_pkt3_middle_error",
	"brb_rc_pkt3_protocol_error",
	"brb_rc_sop_req_tc_port_error",
	"brb_uncomplient_lossless_error",
	"brb_wc0_protocol_error",
	"brb_wc1_protocol_error",
	"brb_wc2_protocol_error",
	"brb_wc3_protocol_error",
	"brb_ll_arb_prefetch_sop_error",
	"brb_ll_blk_error",
	"brb_packet_counter_error",
	"brb_byte_counter_error",
	"brb_mac0_fc_cnt_error",
	"brb_mac1_fc_cnt_error",
	"brb_ll_arb_calc_error",
	"brb_unused_0",
	"brb_wc0_inp_fifo_error",
	"brb_wc0_sop_fifo_error",
	"brb_unused_1",
	"brb_wc0_eop_fifo_error",
	"brb_wc0_queue_fifo_error",
	"brb_wc0_free_point_fifo_error",
	"brb_wc0_next_point_fifo_error",
	"brb_wc0_strt_fifo_error",
	"brb_wc0_second_dscr_fifo_error",
	"brb_wc0_pkt_avail_fifo_error",
	"brb_wc0_cos_cnt_fifo_error",
	"brb_wc0_notify_fifo_error",
	"brb_wc0_ll_req_fifo_error",
	"brb_wc0_ll_pa_cnt_error",
	"brb_wc0_bb_pa_cnt_error",
	"brb_wc1_inp_fifo_error",
	"brb_wc1_sop_fifo_error",
	"brb_wc1_eop_fifo_error",
	"brb_wc1_queue_fifo_error",
	"brb_wc1_free_point_fifo_error",
	"brb_wc1_next_point_fifo_error",
	"brb_wc1_strt_fifo_error",
	"brb_wc1_second_dscr_fifo_error",
	"brb_wc1_pkt_avail_fifo_error",
	"brb_wc1_cos_cnt_fifo_error",
	"brb_wc1_notify_fifo_error",
	"brb_wc1_ll_req_fifo_error",
	"brb_wc1_ll_pa_cnt_error",
	"brb_wc1_bb_pa_cnt_error",
	"brb_wc2_inp_fifo_error",
	"brb_wc2_sop_fifo_error",
	"brb_wc2_eop_fifo_error",
	"brb_wc2_queue_fifo_error",
	"brb_wc2_free_point_fifo_error",
	"brb_wc2_next_point_fifo_error",
	"brb_wc2_strt_fifo_error",
	"brb_wc2_second_dscr_fifo_error",
	"brb_wc2_pkt_avail_fifo_error",
	"brb_wc2_cos_cnt_fifo_error",
	"brb_wc2_notify_fifo_error",
	"brb_wc2_ll_req_fifo_error",
	"brb_wc2_ll_pa_cnt_error",
	"brb_wc2_bb_pa_cnt_error",
	"brb_wc3_inp_fifo_error",
	"brb_wc3_sop_fifo_error",
	"brb_wc3_eop_fifo_error",
	"brb_wc3_queue_fifo_error",
	"brb_wc3_free_point_fifo_error",
	"brb_wc3_next_point_fifo_error",
	"brb_wc3_strt_fifo_error",
	"brb_wc3_second_dscr_fifo_error",
	"brb_wc3_pkt_avail_fifo_error",
	"brb_wc3_cos_cnt_fifo_error",
	"brb_wc3_notify_fifo_error",
	"brb_wc3_ll_req_fifo_error",
	"brb_wc3_ll_pa_cnt_error",
	"brb_wc3_bb_pa_cnt_error",
	"brb_rc_pkt0_side_fifo_error",
	"brb_rc_pkt0_req_fifo_error",
	"brb_rc_pkt0_blk_fifo_error",
	"brb_rc_pkt0_rls_left_fifo_error",
	"brb_rc_pkt0_strt_ptr_fifo_error",
	"brb_rc_pkt0_second_ptr_fifo_error",
	"brb_rc_pkt0_rsp_fifo_error",
	"brb_rc_pkt0_dscr_fifo_error",
	"brb_rc_pkt1_side_fifo_error",
	"brb_rc_pkt1_req_fifo_error",
	"brb_rc_pkt1_blk_fifo_error",
	"brb_rc_pkt1_rls_left_fifo_error",
	"brb_rc_pkt1_strt_ptr_fifo_error",
	"brb_rc_pkt1_second_ptr_fifo_error",
	"brb_rc_pkt1_rsp_fifo_error",
	"brb_rc_pkt1_dscr_fifo_error",
	"brb_rc_pkt2_side_fifo_error",
	"brb_rc_pkt2_req_fifo_error",
	"brb_rc_pkt2_blk_fifo_error",
	"brb_rc_pkt2_rls_left_fifo_error",
	"brb_rc_pkt2_strt_ptr_fifo_error",
	"brb_rc_pkt2_second_ptr_fifo_error",
	"brb_rc_pkt2_rsp_fifo_error",
	"brb_rc_pkt2_dscr_fifo_error",
	"brb_rc_pkt3_side_fifo_error",
	"brb_rc_pkt3_req_fifo_error",
	"brb_rc_pkt3_blk_fifo_error",
	"brb_rc_pkt3_rls_left_fifo_error",
	"brb_rc_pkt3_strt_ptr_fifo_error",
	"brb_rc_pkt3_second_ptr_fifo_error",
	"brb_rc_pkt3_rsp_fifo_error",
	"brb_rc_pkt3_dscr_fifo_error",
	"brb_rc_sop_strt_fifo_error",
	"brb_rc_sop_req_fifo_error",
	"brb_rc_sop_dscr_fifo_error",
	"brb_rc_sop_queue_fifo_error",
	"brb_rc0_eop_error",
	"brb_rc1_eop_error",
	"brb_ll_arb_rls_fifo_error",
	"brb_ll_arb_prefetch_fifo_error",
	"brb_rc_pkt0_rls_fifo_error",
	"brb_rc_pkt1_rls_fifo_error",
	"brb_rc_pkt2_rls_fifo_error",
	"brb_rc_pkt3_rls_fifo_error",
	"brb_rc_pkt4_rls_fifo_error",
	"brb_rc_pkt4_rls_error",
	"brb_rc_pkt4_1st_error",
	"brb_rc_pkt4_len_error",
	"brb_rc_pkt4_middle_error",
	"brb_rc_pkt4_protocol_error",
	"brb_rc_pkt4_side_fifo_error",
	"brb_rc_pkt4_req_fifo_error",
	"brb_rc_pkt4_blk_fifo_error",
	"brb_rc_pkt4_rls_left_fifo_error",
	"brb_rc_pkt4_strt_ptr_fifo_error",
	"brb_rc_pkt4_second_ptr_fifo_error",
	"brb_rc_pkt4_rsp_fifo_error",
	"brb_rc_pkt4_dscr_fifo_error",
	"brb_rc_pkt5_rls_error",
	"brb_packet_available_sync_fifo_push_error",
	"brb_wc4_protocol_error",
	"brb_wc5_protocol_error",
	"brb_wc6_protocol_error",
	"brb_wc7_protocol_error",
	"brb_wc4_inp_fifo_error",
	"brb_wc4_sop_fifo_error",
	"brb_wc4_queue_fifo_error",
	"brb_wc4_free_point_fifo_error",
	"brb_wc4_next_point_fifo_error",
	"brb_wc4_strt_fifo_error",
	"brb_wc4_second_dscr_fifo_error",
	"brb_wc4_pkt_avail_fifo_error",
	"brb_wc4_cos_cnt_fifo_error",
	"brb_wc4_notify_fifo_error",
	"brb_wc4_ll_req_fifo_error",
	"brb_wc4_ll_pa_cnt_error",
	"brb_wc4_bb_pa_cnt_error",
	"brb_wc5_inp_fifo_error",
	"brb_wc5_sop_fifo_error",
	"brb_wc5_queue_fifo_error",
	"brb_wc5_free_point_fifo_error",
	"brb_wc5_next_point_fifo_error",
	"brb_wc5_strt_fifo_error",
	"brb_wc5_second_dscr_fifo_error",
	"brb_wc5_pkt_avail_fifo_error",
	"brb_wc5_cos_cnt_fifo_error",
	"brb_wc5_notify_fifo_error",
	"brb_wc5_ll_req_fifo_error",
	"brb_wc5_ll_pa_cnt_error",
	"brb_wc5_bb_pa_cnt_error",
	"brb_wc6_inp_fifo_error",
	"brb_wc6_sop_fifo_error",
	"brb_wc6_queue_fifo_error",
	"brb_wc6_free_point_fifo_error",
	"brb_wc6_next_point_fifo_error",
	"brb_wc6_strt_fifo_error",
	"brb_wc6_second_dscr_fifo_error",
	"brb_wc6_pkt_avail_fifo_error",
	"brb_wc6_cos_cnt_fifo_error",
	"brb_wc6_notify_fifo_error",
	"brb_wc6_ll_req_fifo_error",
	"brb_wc6_ll_pa_cnt_error",
	"brb_wc6_bb_pa_cnt_error",
	"brb_wc7_inp_fifo_error",
	"brb_wc7_sop_fifo_error",
	"brb_wc7_queue_fifo_error",
	"brb_wc7_free_point_fifo_error",
	"brb_wc7_next_point_fifo_error",
	"brb_wc7_strt_fifo_error",
	"brb_wc7_second_dscr_fifo_error",
	"brb_wc7_pkt_avail_fifo_error",
	"brb_wc7_cos_cnt_fifo_error",
	"brb_wc7_notify_fifo_error",
	"brb_wc7_ll_req_fifo_error",
	"brb_wc7_ll_pa_cnt_error",
	"brb_wc7_bb_pa_cnt_error",
	"brb_wc9_queue_fifo_error",
	"brb_rc_sop_inp_sync_fifo_push_error",
	"brb_rc0_inp_sync_fifo_push_error",
	"brb_rc1_inp_sync_fifo_push_error",
	"brb_rc2_inp_sync_fifo_push_error",
	"brb_rc3_inp_sync_fifo_push_error",
	"brb_rc0_out_sync_fifo_push_error",
	"brb_rc1_out_sync_fifo_push_error",
	"brb_rc2_out_sync_fifo_push_error",
	"brb_rc3_out_sync_fifo_push_error",
	"brb_rc4_out_sync_fifo_push_error",
	"brb_unused_2",
	"brb_rc0_eop_inp_sync_fifo_push_error",
	"brb_rc1_eop_inp_sync_fifo_push_error",
	"brb_rc2_eop_inp_sync_fifo_push_error",
	"brb_rc3_eop_inp_sync_fifo_push_error",
	"brb_rc0_eop_out_sync_fifo_push_error",
	"brb_rc1_eop_out_sync_fifo_push_error",
	"brb_rc2_eop_out_sync_fifo_push_error",
	"brb_rc3_eop_out_sync_fifo_push_error",
	"brb_unused_3",
	"brb_rc2_eop_error",
	"brb_rc3_eop_error",
	"brb_mac2_fc_cnt_error",
	"brb_mac3_fc_cnt_error",
	"brb_wc4_eop_fifo_error",
	"brb_wc5_eop_fifo_error",
	"brb_wc6_eop_fifo_error",
	"brb_wc7_eop_fifo_error",
};
#else
#define brb_int_attn_desc OSAL_NULL
#endif

static const u16 brb_int0_bb_a0_attn_idx[32] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
};

static struct attn_hw_reg brb_int0_bb_a0 = {
	0, 32, brb_int0_bb_a0_attn_idx, 0x3400c0, 0x3400cc, 0x3400c8, 0x3400c4
};

static const u16 brb_int1_bb_a0_attn_idx[30] = {
	32, 33, 35, 36, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,
	52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
};

static struct attn_hw_reg brb_int1_bb_a0 = {
	1, 30, brb_int1_bb_a0_attn_idx, 0x3400d8, 0x3400e4, 0x3400e0, 0x3400dc
};

static const u16 brb_int2_bb_a0_attn_idx[28] = {
	64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81,
	82, 83, 84, 85, 86, 87, 88, 89, 90, 91,
};

static struct attn_hw_reg brb_int2_bb_a0 = {
	2, 28, brb_int2_bb_a0_attn_idx, 0x3400f0, 0x3400fc, 0x3400f8, 0x3400f4
};

static const u16 brb_int3_bb_a0_attn_idx[31] = {
	92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107,
	108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121,
	    122,
};

static struct attn_hw_reg brb_int3_bb_a0 = {
	3, 31, brb_int3_bb_a0_attn_idx, 0x340108, 0x340114, 0x340110, 0x34010c
};

static const u16 brb_int4_bb_a0_attn_idx[27] = {
	123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136,
	137,
	138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149,
};

static struct attn_hw_reg brb_int4_bb_a0 = {
	4, 27, brb_int4_bb_a0_attn_idx, 0x340120, 0x34012c, 0x340128, 0x340124
};

static const u16 brb_int5_bb_a0_attn_idx[1] = {
	150,
};

static struct attn_hw_reg brb_int5_bb_a0 = {
	5, 1, brb_int5_bb_a0_attn_idx, 0x340138, 0x340144, 0x340140, 0x34013c
};

static const u16 brb_int6_bb_a0_attn_idx[8] = {
	151, 152, 153, 154, 155, 156, 157, 158,
};

static struct attn_hw_reg brb_int6_bb_a0 = {
	6, 8, brb_int6_bb_a0_attn_idx, 0x340150, 0x34015c, 0x340158, 0x340154
};

static const u16 brb_int7_bb_a0_attn_idx[32] = {
	159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172,
	173,
	174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187,
	    188, 189,
	190,
};

static struct attn_hw_reg brb_int7_bb_a0 = {
	7, 32, brb_int7_bb_a0_attn_idx, 0x340168, 0x340174, 0x340170, 0x34016c
};

static const u16 brb_int8_bb_a0_attn_idx[17] = {
	191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204,
	205,
	206, 207,
};

static struct attn_hw_reg brb_int8_bb_a0 = {
	8, 17, brb_int8_bb_a0_attn_idx, 0x340184, 0x340190, 0x34018c, 0x340188
};

static const u16 brb_int9_bb_a0_attn_idx[1] = {
	208,
};

static struct attn_hw_reg brb_int9_bb_a0 = {
	9, 1, brb_int9_bb_a0_attn_idx, 0x34019c, 0x3401a8, 0x3401a4, 0x3401a0
};

static const u16 brb_int10_bb_a0_attn_idx[14] = {
	209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 220, 221, 224, 225,
};

static struct attn_hw_reg brb_int10_bb_a0 = {
	10, 14, brb_int10_bb_a0_attn_idx, 0x3401b4, 0x3401c0, 0x3401bc,
	0x3401b8
};

static const u16 brb_int11_bb_a0_attn_idx[8] = {
	229, 230, 231, 232, 233, 234, 235, 236,
};

static struct attn_hw_reg brb_int11_bb_a0 = {
	11, 8, brb_int11_bb_a0_attn_idx, 0x3401cc, 0x3401d8, 0x3401d4, 0x3401d0
};

static struct attn_hw_reg *brb_int_bb_a0_regs[12] = {
	&brb_int0_bb_a0, &brb_int1_bb_a0, &brb_int2_bb_a0, &brb_int3_bb_a0,
	&brb_int4_bb_a0, &brb_int5_bb_a0, &brb_int6_bb_a0, &brb_int7_bb_a0,
	&brb_int8_bb_a0, &brb_int9_bb_a0,
	&brb_int10_bb_a0, &brb_int11_bb_a0,
};

static const u16 brb_int0_bb_b0_attn_idx[32] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
};

static struct attn_hw_reg brb_int0_bb_b0 = {
	0, 32, brb_int0_bb_b0_attn_idx, 0x3400c0, 0x3400cc, 0x3400c8, 0x3400c4
};

static const u16 brb_int1_bb_b0_attn_idx[30] = {
	32, 33, 35, 36, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,
	52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
};

static struct attn_hw_reg brb_int1_bb_b0 = {
	1, 30, brb_int1_bb_b0_attn_idx, 0x3400d8, 0x3400e4, 0x3400e0, 0x3400dc
};

static const u16 brb_int2_bb_b0_attn_idx[28] = {
	64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81,
	82, 83, 84, 85, 86, 87, 88, 89, 90, 91,
};

static struct attn_hw_reg brb_int2_bb_b0 = {
	2, 28, brb_int2_bb_b0_attn_idx, 0x3400f0, 0x3400fc, 0x3400f8, 0x3400f4
};

static const u16 brb_int3_bb_b0_attn_idx[31] = {
	92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107,
	108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121,
	    122,
};

static struct attn_hw_reg brb_int3_bb_b0 = {
	3, 31, brb_int3_bb_b0_attn_idx, 0x340108, 0x340114, 0x340110, 0x34010c
};

static const u16 brb_int4_bb_b0_attn_idx[27] = {
	123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136,
	137,
	138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149,
};

static struct attn_hw_reg brb_int4_bb_b0 = {
	4, 27, brb_int4_bb_b0_attn_idx, 0x340120, 0x34012c, 0x340128, 0x340124
};

static const u16 brb_int5_bb_b0_attn_idx[1] = {
	150,
};

static struct attn_hw_reg brb_int5_bb_b0 = {
	5, 1, brb_int5_bb_b0_attn_idx, 0x340138, 0x340144, 0x340140, 0x34013c
};

static const u16 brb_int6_bb_b0_attn_idx[8] = {
	151, 152, 153, 154, 155, 156, 157, 158,
};

static struct attn_hw_reg brb_int6_bb_b0 = {
	6, 8, brb_int6_bb_b0_attn_idx, 0x340150, 0x34015c, 0x340158, 0x340154
};

static const u16 brb_int7_bb_b0_attn_idx[32] = {
	159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172,
	173,
	174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187,
	    188, 189,
	190,
};

static struct attn_hw_reg brb_int7_bb_b0 = {
	7, 32, brb_int7_bb_b0_attn_idx, 0x340168, 0x340174, 0x340170, 0x34016c
};

static const u16 brb_int8_bb_b0_attn_idx[17] = {
	191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204,
	205,
	206, 207,
};

static struct attn_hw_reg brb_int8_bb_b0 = {
	8, 17, brb_int8_bb_b0_attn_idx, 0x340184, 0x340190, 0x34018c, 0x340188
};

static const u16 brb_int9_bb_b0_attn_idx[1] = {
	208,
};

static struct attn_hw_reg brb_int9_bb_b0 = {
	9, 1, brb_int9_bb_b0_attn_idx, 0x34019c, 0x3401a8, 0x3401a4, 0x3401a0
};

static const u16 brb_int10_bb_b0_attn_idx[14] = {
	209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 220, 221, 224, 225,
};

static struct attn_hw_reg brb_int10_bb_b0 = {
	10, 14, brb_int10_bb_b0_attn_idx, 0x3401b4, 0x3401c0, 0x3401bc,
	0x3401b8
};

static const u16 brb_int11_bb_b0_attn_idx[8] = {
	229, 230, 231, 232, 233, 234, 235, 236,
};

static struct attn_hw_reg brb_int11_bb_b0 = {
	11, 8, brb_int11_bb_b0_attn_idx, 0x3401cc, 0x3401d8, 0x3401d4, 0x3401d0
};

static struct attn_hw_reg *brb_int_bb_b0_regs[12] = {
	&brb_int0_bb_b0, &brb_int1_bb_b0, &brb_int2_bb_b0, &brb_int3_bb_b0,
	&brb_int4_bb_b0, &brb_int5_bb_b0, &brb_int6_bb_b0, &brb_int7_bb_b0,
	&brb_int8_bb_b0, &brb_int9_bb_b0,
	&brb_int10_bb_b0, &brb_int11_bb_b0,
};

static const u16 brb_int0_k2_attn_idx[32] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
};

static struct attn_hw_reg brb_int0_k2 = {
	0, 32, brb_int0_k2_attn_idx, 0x3400c0, 0x3400cc, 0x3400c8, 0x3400c4
};

static const u16 brb_int1_k2_attn_idx[30] = {
	32, 33, 35, 36, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,
	52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
};

static struct attn_hw_reg brb_int1_k2 = {
	1, 30, brb_int1_k2_attn_idx, 0x3400d8, 0x3400e4, 0x3400e0, 0x3400dc
};

static const u16 brb_int2_k2_attn_idx[28] = {
	64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81,
	82, 83, 84, 85, 86, 87, 88, 89, 90, 91,
};

static struct attn_hw_reg brb_int2_k2 = {
	2, 28, brb_int2_k2_attn_idx, 0x3400f0, 0x3400fc, 0x3400f8, 0x3400f4
};

static const u16 brb_int3_k2_attn_idx[31] = {
	92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107,
	108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121,
	    122,
};

static struct attn_hw_reg brb_int3_k2 = {
	3, 31, brb_int3_k2_attn_idx, 0x340108, 0x340114, 0x340110, 0x34010c
};

static const u16 brb_int4_k2_attn_idx[27] = {
	123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136,
	137,
	138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149,
};

static struct attn_hw_reg brb_int4_k2 = {
	4, 27, brb_int4_k2_attn_idx, 0x340120, 0x34012c, 0x340128, 0x340124
};

static const u16 brb_int5_k2_attn_idx[1] = {
	150,
};

static struct attn_hw_reg brb_int5_k2 = {
	5, 1, brb_int5_k2_attn_idx, 0x340138, 0x340144, 0x340140, 0x34013c
};

static const u16 brb_int6_k2_attn_idx[8] = {
	151, 152, 153, 154, 155, 156, 157, 158,
};

static struct attn_hw_reg brb_int6_k2 = {
	6, 8, brb_int6_k2_attn_idx, 0x340150, 0x34015c, 0x340158, 0x340154
};

static const u16 brb_int7_k2_attn_idx[32] = {
	159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172,
	173,
	174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187,
	    188, 189,
	190,
};

static struct attn_hw_reg brb_int7_k2 = {
	7, 32, brb_int7_k2_attn_idx, 0x340168, 0x340174, 0x340170, 0x34016c
};

static const u16 brb_int8_k2_attn_idx[17] = {
	191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204,
	205,
	206, 207,
};

static struct attn_hw_reg brb_int8_k2 = {
	8, 17, brb_int8_k2_attn_idx, 0x340184, 0x340190, 0x34018c, 0x340188
};

static const u16 brb_int9_k2_attn_idx[1] = {
	208,
};

static struct attn_hw_reg brb_int9_k2 = {
	9, 1, brb_int9_k2_attn_idx, 0x34019c, 0x3401a8, 0x3401a4, 0x3401a0
};

static const u16 brb_int10_k2_attn_idx[18] = {
	209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 220, 221, 222, 223,
	224,
	225, 226, 227,
};

static struct attn_hw_reg brb_int10_k2 = {
	10, 18, brb_int10_k2_attn_idx, 0x3401b4, 0x3401c0, 0x3401bc, 0x3401b8
};

static const u16 brb_int11_k2_attn_idx[8] = {
	229, 230, 231, 232, 233, 234, 235, 236,
};

static struct attn_hw_reg brb_int11_k2 = {
	11, 8, brb_int11_k2_attn_idx, 0x3401cc, 0x3401d8, 0x3401d4, 0x3401d0
};

static struct attn_hw_reg *brb_int_k2_regs[12] = {
	&brb_int0_k2, &brb_int1_k2, &brb_int2_k2, &brb_int3_k2, &brb_int4_k2,
	&brb_int5_k2, &brb_int6_k2, &brb_int7_k2, &brb_int8_k2, &brb_int9_k2,
	&brb_int10_k2, &brb_int11_k2,
};

#ifdef ATTN_DESC
static const char *brb_prty_attn_desc[75] = {
	"brb_ll_bank0_mem_prty",
	"brb_ll_bank1_mem_prty",
	"brb_ll_bank2_mem_prty",
	"brb_ll_bank3_mem_prty",
	"brb_datapath_registers",
	"brb_mem001_i_ecc_rf_int",
	"brb_mem008_i_ecc_rf_int",
	"brb_mem009_i_ecc_rf_int",
	"brb_mem010_i_ecc_rf_int",
	"brb_mem011_i_ecc_rf_int",
	"brb_mem012_i_ecc_rf_int",
	"brb_mem013_i_ecc_rf_int",
	"brb_mem014_i_ecc_rf_int",
	"brb_mem015_i_ecc_rf_int",
	"brb_mem016_i_ecc_rf_int",
	"brb_mem002_i_ecc_rf_int",
	"brb_mem003_i_ecc_rf_int",
	"brb_mem004_i_ecc_rf_int",
	"brb_mem005_i_ecc_rf_int",
	"brb_mem006_i_ecc_rf_int",
	"brb_mem007_i_ecc_rf_int",
	"brb_mem070_i_mem_prty",
	"brb_mem069_i_mem_prty",
	"brb_mem053_i_mem_prty",
	"brb_mem054_i_mem_prty",
	"brb_mem055_i_mem_prty",
	"brb_mem056_i_mem_prty",
	"brb_mem057_i_mem_prty",
	"brb_mem058_i_mem_prty",
	"brb_mem059_i_mem_prty",
	"brb_mem060_i_mem_prty",
	"brb_mem061_i_mem_prty",
	"brb_mem062_i_mem_prty",
	"brb_mem063_i_mem_prty",
	"brb_mem064_i_mem_prty",
	"brb_mem065_i_mem_prty",
	"brb_mem045_i_mem_prty",
	"brb_mem046_i_mem_prty",
	"brb_mem047_i_mem_prty",
	"brb_mem048_i_mem_prty",
	"brb_mem049_i_mem_prty",
	"brb_mem050_i_mem_prty",
	"brb_mem051_i_mem_prty",
	"brb_mem052_i_mem_prty",
	"brb_mem041_i_mem_prty",
	"brb_mem042_i_mem_prty",
	"brb_mem043_i_mem_prty",
	"brb_mem044_i_mem_prty",
	"brb_mem040_i_mem_prty",
	"brb_mem035_i_mem_prty",
	"brb_mem066_i_mem_prty",
	"brb_mem067_i_mem_prty",
	"brb_mem068_i_mem_prty",
	"brb_mem030_i_mem_prty",
	"brb_mem031_i_mem_prty",
	"brb_mem032_i_mem_prty",
	"brb_mem033_i_mem_prty",
	"brb_mem037_i_mem_prty",
	"brb_mem038_i_mem_prty",
	"brb_mem034_i_mem_prty",
	"brb_mem036_i_mem_prty",
	"brb_mem017_i_mem_prty",
	"brb_mem018_i_mem_prty",
	"brb_mem019_i_mem_prty",
	"brb_mem020_i_mem_prty",
	"brb_mem021_i_mem_prty",
	"brb_mem022_i_mem_prty",
	"brb_mem023_i_mem_prty",
	"brb_mem024_i_mem_prty",
	"brb_mem029_i_mem_prty",
	"brb_mem026_i_mem_prty",
	"brb_mem027_i_mem_prty",
	"brb_mem028_i_mem_prty",
	"brb_mem025_i_mem_prty",
	"brb_mem039_i_mem_prty",
};
#else
#define brb_prty_attn_desc OSAL_NULL
#endif

static const u16 brb_prty1_bb_a0_attn_idx[31] = {
	5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 23, 24, 36,
	37,
	38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 49,
};

static struct attn_hw_reg brb_prty1_bb_a0 = {
	0, 31, brb_prty1_bb_a0_attn_idx, 0x340400, 0x34040c, 0x340408, 0x340404
};

static const u16 brb_prty2_bb_a0_attn_idx[19] = {
	53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 69, 70, 71, 72, 73, 74,
	48,
};

static struct attn_hw_reg brb_prty2_bb_a0 = {
	1, 19, brb_prty2_bb_a0_attn_idx, 0x340410, 0x34041c, 0x340418, 0x340414
};

static struct attn_hw_reg *brb_prty_bb_a0_regs[2] = {
	&brb_prty1_bb_a0, &brb_prty2_bb_a0,
};

static const u16 brb_prty0_bb_b0_attn_idx[5] = {
	0, 1, 2, 3, 4,
};

static struct attn_hw_reg brb_prty0_bb_b0 = {
	0, 5, brb_prty0_bb_b0_attn_idx, 0x3401dc, 0x3401e8, 0x3401e4, 0x3401e0
};

static const u16 brb_prty1_bb_b0_attn_idx[31] = {
	5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 23, 24, 36,
	37,
	38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
};

static struct attn_hw_reg brb_prty1_bb_b0 = {
	1, 31, brb_prty1_bb_b0_attn_idx, 0x340400, 0x34040c, 0x340408, 0x340404
};

static const u16 brb_prty2_bb_b0_attn_idx[14] = {
	53, 54, 55, 56, 59, 61, 62, 63, 64, 69, 70, 71, 72, 73,
};

static struct attn_hw_reg brb_prty2_bb_b0 = {
	2, 14, brb_prty2_bb_b0_attn_idx, 0x340410, 0x34041c, 0x340418, 0x340414
};

static struct attn_hw_reg *brb_prty_bb_b0_regs[3] = {
	&brb_prty0_bb_b0, &brb_prty1_bb_b0, &brb_prty2_bb_b0,
};

static const u16 brb_prty0_k2_attn_idx[5] = {
	0, 1, 2, 3, 4,
};

static struct attn_hw_reg brb_prty0_k2 = {
	0, 5, brb_prty0_k2_attn_idx, 0x3401dc, 0x3401e8, 0x3401e4, 0x3401e0
};

static const u16 brb_prty1_k2_attn_idx[31] = {
	5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
	24,
	25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
};

static struct attn_hw_reg brb_prty1_k2 = {
	1, 31, brb_prty1_k2_attn_idx, 0x340400, 0x34040c, 0x340408, 0x340404
};

static const u16 brb_prty2_k2_attn_idx[30] = {
	50, 51, 52, 36, 37, 38, 39, 40, 41, 42, 43, 47, 53, 54, 55, 56, 57, 58,
	59, 49, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69,
};

static struct attn_hw_reg brb_prty2_k2 = {
	2, 30, brb_prty2_k2_attn_idx, 0x340410, 0x34041c, 0x340418, 0x340414
};

static struct attn_hw_reg *brb_prty_k2_regs[3] = {
	&brb_prty0_k2, &brb_prty1_k2, &brb_prty2_k2,
};

#ifdef ATTN_DESC
static const char *src_int_attn_desc[1] = {
	"src_address_error",
};
#else
#define src_int_attn_desc OSAL_NULL
#endif

static const u16 src_int0_bb_a0_attn_idx[1] = {
	0,
};

static struct attn_hw_reg src_int0_bb_a0 = {
	0, 1, src_int0_bb_a0_attn_idx, 0x2381d8, 0x2381dc, 0x2381e0, 0x2381e4
};

static struct attn_hw_reg *src_int_bb_a0_regs[1] = {
	&src_int0_bb_a0,
};

static const u16 src_int0_bb_b0_attn_idx[1] = {
	0,
};

static struct attn_hw_reg src_int0_bb_b0 = {
	0, 1, src_int0_bb_b0_attn_idx, 0x2381d8, 0x2381dc, 0x2381e0, 0x2381e4
};

static struct attn_hw_reg *src_int_bb_b0_regs[1] = {
	&src_int0_bb_b0,
};

static const u16 src_int0_k2_attn_idx[1] = {
	0,
};

static struct attn_hw_reg src_int0_k2 = {
	0, 1, src_int0_k2_attn_idx, 0x2381d8, 0x2381dc, 0x2381e0, 0x2381e4
};

static struct attn_hw_reg *src_int_k2_regs[1] = {
	&src_int0_k2,
};

#ifdef ATTN_DESC
static const char *prs_int_attn_desc[2] = {
	"prs_address_error",
	"prs_lcid_validation_err",
};
#else
#define prs_int_attn_desc OSAL_NULL
#endif

static const u16 prs_int0_bb_a0_attn_idx[2] = {
	0, 1,
};

static struct attn_hw_reg prs_int0_bb_a0 = {
	0, 2, prs_int0_bb_a0_attn_idx, 0x1f0040, 0x1f004c, 0x1f0048, 0x1f0044
};

static struct attn_hw_reg *prs_int_bb_a0_regs[1] = {
	&prs_int0_bb_a0,
};

static const u16 prs_int0_bb_b0_attn_idx[2] = {
	0, 1,
};

static struct attn_hw_reg prs_int0_bb_b0 = {
	0, 2, prs_int0_bb_b0_attn_idx, 0x1f0040, 0x1f004c, 0x1f0048, 0x1f0044
};

static struct attn_hw_reg *prs_int_bb_b0_regs[1] = {
	&prs_int0_bb_b0,
};

static const u16 prs_int0_k2_attn_idx[2] = {
	0, 1,
};

static struct attn_hw_reg prs_int0_k2 = {
	0, 2, prs_int0_k2_attn_idx, 0x1f0040, 0x1f004c, 0x1f0048, 0x1f0044
};

static struct attn_hw_reg *prs_int_k2_regs[1] = {
	&prs_int0_k2,
};

#ifdef ATTN_DESC
static const char *prs_prty_attn_desc[75] = {
	"prs_cam_parity",
	"prs_gft_cam_parity",
	"prs_mem011_i_ecc_rf_int",
	"prs_mem012_i_ecc_rf_int",
	"prs_mem016_i_ecc_rf_int",
	"prs_mem017_i_ecc_rf_int",
	"prs_mem021_i_ecc_rf_int",
	"prs_mem022_i_ecc_rf_int",
	"prs_mem026_i_ecc_rf_int",
	"prs_mem027_i_ecc_rf_int",
	"prs_mem064_i_mem_prty",
	"prs_mem044_i_mem_prty",
	"prs_mem043_i_mem_prty",
	"prs_mem037_i_mem_prty",
	"prs_mem033_i_mem_prty",
	"prs_mem034_i_mem_prty",
	"prs_mem035_i_mem_prty",
	"prs_mem036_i_mem_prty",
	"prs_mem029_i_mem_prty",
	"prs_mem030_i_mem_prty",
	"prs_mem031_i_mem_prty",
	"prs_mem032_i_mem_prty",
	"prs_mem007_i_mem_prty",
	"prs_mem028_i_mem_prty",
	"prs_mem039_i_mem_prty",
	"prs_mem040_i_mem_prty",
	"prs_mem058_i_mem_prty",
	"prs_mem059_i_mem_prty",
	"prs_mem041_i_mem_prty",
	"prs_mem042_i_mem_prty",
	"prs_mem060_i_mem_prty",
	"prs_mem061_i_mem_prty",
	"prs_mem009_i_mem_prty",
	"prs_mem009_i_ecc_rf_int",
	"prs_mem010_i_ecc_rf_int",
	"prs_mem014_i_ecc_rf_int",
	"prs_mem015_i_ecc_rf_int",
	"prs_mem026_i_mem_prty",
	"prs_mem025_i_mem_prty",
	"prs_mem021_i_mem_prty",
	"prs_mem019_i_mem_prty",
	"prs_mem020_i_mem_prty",
	"prs_mem017_i_mem_prty",
	"prs_mem018_i_mem_prty",
	"prs_mem005_i_mem_prty",
	"prs_mem016_i_mem_prty",
	"prs_mem023_i_mem_prty",
	"prs_mem024_i_mem_prty",
	"prs_mem008_i_mem_prty",
	"prs_mem012_i_mem_prty",
	"prs_mem013_i_mem_prty",
	"prs_mem006_i_mem_prty",
	"prs_mem011_i_mem_prty",
	"prs_mem003_i_mem_prty",
	"prs_mem004_i_mem_prty",
	"prs_mem027_i_mem_prty",
	"prs_mem010_i_mem_prty",
	"prs_mem014_i_mem_prty",
	"prs_mem015_i_mem_prty",
	"prs_mem054_i_mem_prty",
	"prs_mem055_i_mem_prty",
	"prs_mem056_i_mem_prty",
	"prs_mem057_i_mem_prty",
	"prs_mem046_i_mem_prty",
	"prs_mem047_i_mem_prty",
	"prs_mem048_i_mem_prty",
	"prs_mem049_i_mem_prty",
	"prs_mem050_i_mem_prty",
	"prs_mem051_i_mem_prty",
	"prs_mem052_i_mem_prty",
	"prs_mem053_i_mem_prty",
	"prs_mem062_i_mem_prty",
	"prs_mem045_i_mem_prty",
	"prs_mem002_i_mem_prty",
	"prs_mem001_i_mem_prty",
};
#else
#define prs_prty_attn_desc OSAL_NULL
#endif

static const u16 prs_prty0_bb_a0_attn_idx[1] = {
	0,
};

static struct attn_hw_reg prs_prty0_bb_a0 = {
	0, 1, prs_prty0_bb_a0_attn_idx, 0x1f0050, 0x1f005c, 0x1f0058, 0x1f0054
};

static const u16 prs_prty1_bb_a0_attn_idx[31] = {
	13, 14, 15, 16, 18, 21, 22, 23, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42,
	43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55,
};

static struct attn_hw_reg prs_prty1_bb_a0 = {
	1, 31, prs_prty1_bb_a0_attn_idx, 0x1f0204, 0x1f0210, 0x1f020c, 0x1f0208
};

static const u16 prs_prty2_bb_a0_attn_idx[5] = {
	73, 74, 20, 17, 19,
};

static struct attn_hw_reg prs_prty2_bb_a0 = {
	2, 5, prs_prty2_bb_a0_attn_idx, 0x1f0214, 0x1f0220, 0x1f021c, 0x1f0218
};

static struct attn_hw_reg *prs_prty_bb_a0_regs[3] = {
	&prs_prty0_bb_a0, &prs_prty1_bb_a0, &prs_prty2_bb_a0,
};

static const u16 prs_prty0_bb_b0_attn_idx[2] = {
	0, 1,
};

static struct attn_hw_reg prs_prty0_bb_b0 = {
	0, 2, prs_prty0_bb_b0_attn_idx, 0x1f0050, 0x1f005c, 0x1f0058, 0x1f0054
};

static const u16 prs_prty1_bb_b0_attn_idx[31] = {
	13, 14, 15, 16, 18, 19, 21, 22, 23, 33, 34, 35, 36, 37, 38, 39, 40, 41,
	42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54,
};

static struct attn_hw_reg prs_prty1_bb_b0 = {
	1, 31, prs_prty1_bb_b0_attn_idx, 0x1f0204, 0x1f0210, 0x1f020c, 0x1f0208
};

static const u16 prs_prty2_bb_b0_attn_idx[5] = {
	73, 74, 20, 17, 55,
};

static struct attn_hw_reg prs_prty2_bb_b0 = {
	2, 5, prs_prty2_bb_b0_attn_idx, 0x1f0214, 0x1f0220, 0x1f021c, 0x1f0218
};

static struct attn_hw_reg *prs_prty_bb_b0_regs[3] = {
	&prs_prty0_bb_b0, &prs_prty1_bb_b0, &prs_prty2_bb_b0,
};

static const u16 prs_prty0_k2_attn_idx[2] = {
	0, 1,
};

static struct attn_hw_reg prs_prty0_k2 = {
	0, 2, prs_prty0_k2_attn_idx, 0x1f0050, 0x1f005c, 0x1f0058, 0x1f0054
};

static const u16 prs_prty1_k2_attn_idx[31] = {
	2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
	22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
};

static struct attn_hw_reg prs_prty1_k2 = {
	1, 31, prs_prty1_k2_attn_idx, 0x1f0204, 0x1f0210, 0x1f020c, 0x1f0208
};

static const u16 prs_prty2_k2_attn_idx[31] = {
	56, 57, 58, 40, 41, 47, 38, 48, 50, 43, 46, 59, 60, 61, 62, 53, 54, 44,
	51, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74,
};

static struct attn_hw_reg prs_prty2_k2 = {
	2, 31, prs_prty2_k2_attn_idx, 0x1f0214, 0x1f0220, 0x1f021c, 0x1f0218
};

static struct attn_hw_reg *prs_prty_k2_regs[3] = {
	&prs_prty0_k2, &prs_prty1_k2, &prs_prty2_k2,
};

#ifdef ATTN_DESC
static const char *tsdm_int_attn_desc[28] = {
	"tsdm_address_error",
	"tsdm_inp_queue_error",
	"tsdm_delay_fifo_error",
	"tsdm_async_host_error",
	"tsdm_prm_fifo_error",
	"tsdm_ccfc_load_pend_error",
	"tsdm_tcfc_load_pend_error",
	"tsdm_dst_int_ram_wait_error",
	"tsdm_dst_pas_buf_wait_error",
	"tsdm_dst_pxp_immed_error",
	"tsdm_dst_pxp_dst_pend_error",
	"tsdm_dst_brb_src_pend_error",
	"tsdm_dst_brb_src_addr_error",
	"tsdm_rsp_brb_pend_error",
	"tsdm_rsp_int_ram_pend_error",
	"tsdm_rsp_brb_rd_data_error",
	"tsdm_rsp_int_ram_rd_data_error",
	"tsdm_rsp_pxp_rd_data_error",
	"tsdm_cm_delay_error",
	"tsdm_sh_delay_error",
	"tsdm_cmpl_pend_error",
	"tsdm_cprm_pend_error",
	"tsdm_timer_addr_error",
	"tsdm_timer_pend_error",
	"tsdm_dorq_dpm_error",
	"tsdm_dst_pxp_done_error",
	"tsdm_xcm_rmt_buffer_error",
	"tsdm_ycm_rmt_buffer_error",
};
#else
#define tsdm_int_attn_desc OSAL_NULL
#endif

static const u16 tsdm_int0_bb_a0_attn_idx[26] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25,
};

static struct attn_hw_reg tsdm_int0_bb_a0 = {
	0, 26, tsdm_int0_bb_a0_attn_idx, 0xfb0040, 0xfb004c, 0xfb0048, 0xfb0044
};

static struct attn_hw_reg *tsdm_int_bb_a0_regs[1] = {
	&tsdm_int0_bb_a0,
};

static const u16 tsdm_int0_bb_b0_attn_idx[26] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25,
};

static struct attn_hw_reg tsdm_int0_bb_b0 = {
	0, 26, tsdm_int0_bb_b0_attn_idx, 0xfb0040, 0xfb004c, 0xfb0048, 0xfb0044
};

static struct attn_hw_reg *tsdm_int_bb_b0_regs[1] = {
	&tsdm_int0_bb_b0,
};

static const u16 tsdm_int0_k2_attn_idx[28] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25, 26, 27,
};

static struct attn_hw_reg tsdm_int0_k2 = {
	0, 28, tsdm_int0_k2_attn_idx, 0xfb0040, 0xfb004c, 0xfb0048, 0xfb0044
};

static struct attn_hw_reg *tsdm_int_k2_regs[1] = {
	&tsdm_int0_k2,
};

#ifdef ATTN_DESC
static const char *tsdm_prty_attn_desc[10] = {
	"tsdm_mem009_i_mem_prty",
	"tsdm_mem008_i_mem_prty",
	"tsdm_mem007_i_mem_prty",
	"tsdm_mem006_i_mem_prty",
	"tsdm_mem005_i_mem_prty",
	"tsdm_mem002_i_mem_prty",
	"tsdm_mem010_i_mem_prty",
	"tsdm_mem001_i_mem_prty",
	"tsdm_mem003_i_mem_prty",
	"tsdm_mem004_i_mem_prty",
};
#else
#define tsdm_prty_attn_desc OSAL_NULL
#endif

static const u16 tsdm_prty1_bb_a0_attn_idx[10] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
};

static struct attn_hw_reg tsdm_prty1_bb_a0 = {
	0, 10, tsdm_prty1_bb_a0_attn_idx, 0xfb0200, 0xfb020c, 0xfb0208,
	0xfb0204
};

static struct attn_hw_reg *tsdm_prty_bb_a0_regs[1] = {
	&tsdm_prty1_bb_a0,
};

static const u16 tsdm_prty1_bb_b0_attn_idx[10] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
};

static struct attn_hw_reg tsdm_prty1_bb_b0 = {
	0, 10, tsdm_prty1_bb_b0_attn_idx, 0xfb0200, 0xfb020c, 0xfb0208,
	0xfb0204
};

static struct attn_hw_reg *tsdm_prty_bb_b0_regs[1] = {
	&tsdm_prty1_bb_b0,
};

static const u16 tsdm_prty1_k2_attn_idx[10] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
};

static struct attn_hw_reg tsdm_prty1_k2 = {
	0, 10, tsdm_prty1_k2_attn_idx, 0xfb0200, 0xfb020c, 0xfb0208, 0xfb0204
};

static struct attn_hw_reg *tsdm_prty_k2_regs[1] = {
	&tsdm_prty1_k2,
};

#ifdef ATTN_DESC
static const char *msdm_int_attn_desc[28] = {
	"msdm_address_error",
	"msdm_inp_queue_error",
	"msdm_delay_fifo_error",
	"msdm_async_host_error",
	"msdm_prm_fifo_error",
	"msdm_ccfc_load_pend_error",
	"msdm_tcfc_load_pend_error",
	"msdm_dst_int_ram_wait_error",
	"msdm_dst_pas_buf_wait_error",
	"msdm_dst_pxp_immed_error",
	"msdm_dst_pxp_dst_pend_error",
	"msdm_dst_brb_src_pend_error",
	"msdm_dst_brb_src_addr_error",
	"msdm_rsp_brb_pend_error",
	"msdm_rsp_int_ram_pend_error",
	"msdm_rsp_brb_rd_data_error",
	"msdm_rsp_int_ram_rd_data_error",
	"msdm_rsp_pxp_rd_data_error",
	"msdm_cm_delay_error",
	"msdm_sh_delay_error",
	"msdm_cmpl_pend_error",
	"msdm_cprm_pend_error",
	"msdm_timer_addr_error",
	"msdm_timer_pend_error",
	"msdm_dorq_dpm_error",
	"msdm_dst_pxp_done_error",
	"msdm_xcm_rmt_buffer_error",
	"msdm_ycm_rmt_buffer_error",
};
#else
#define msdm_int_attn_desc OSAL_NULL
#endif

static const u16 msdm_int0_bb_a0_attn_idx[26] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25,
};

static struct attn_hw_reg msdm_int0_bb_a0 = {
	0, 26, msdm_int0_bb_a0_attn_idx, 0xfc0040, 0xfc004c, 0xfc0048, 0xfc0044
};

static struct attn_hw_reg *msdm_int_bb_a0_regs[1] = {
	&msdm_int0_bb_a0,
};

static const u16 msdm_int0_bb_b0_attn_idx[26] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25,
};

static struct attn_hw_reg msdm_int0_bb_b0 = {
	0, 26, msdm_int0_bb_b0_attn_idx, 0xfc0040, 0xfc004c, 0xfc0048, 0xfc0044
};

static struct attn_hw_reg *msdm_int_bb_b0_regs[1] = {
	&msdm_int0_bb_b0,
};

static const u16 msdm_int0_k2_attn_idx[28] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25, 26, 27,
};

static struct attn_hw_reg msdm_int0_k2 = {
	0, 28, msdm_int0_k2_attn_idx, 0xfc0040, 0xfc004c, 0xfc0048, 0xfc0044
};

static struct attn_hw_reg *msdm_int_k2_regs[1] = {
	&msdm_int0_k2,
};

#ifdef ATTN_DESC
static const char *msdm_prty_attn_desc[11] = {
	"msdm_mem009_i_mem_prty",
	"msdm_mem008_i_mem_prty",
	"msdm_mem007_i_mem_prty",
	"msdm_mem006_i_mem_prty",
	"msdm_mem005_i_mem_prty",
	"msdm_mem002_i_mem_prty",
	"msdm_mem011_i_mem_prty",
	"msdm_mem001_i_mem_prty",
	"msdm_mem003_i_mem_prty",
	"msdm_mem004_i_mem_prty",
	"msdm_mem010_i_mem_prty",
};
#else
#define msdm_prty_attn_desc OSAL_NULL
#endif

static const u16 msdm_prty1_bb_a0_attn_idx[11] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
};

static struct attn_hw_reg msdm_prty1_bb_a0 = {
	0, 11, msdm_prty1_bb_a0_attn_idx, 0xfc0200, 0xfc020c, 0xfc0208,
	0xfc0204
};

static struct attn_hw_reg *msdm_prty_bb_a0_regs[1] = {
	&msdm_prty1_bb_a0,
};

static const u16 msdm_prty1_bb_b0_attn_idx[11] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
};

static struct attn_hw_reg msdm_prty1_bb_b0 = {
	0, 11, msdm_prty1_bb_b0_attn_idx, 0xfc0200, 0xfc020c, 0xfc0208,
	0xfc0204
};

static struct attn_hw_reg *msdm_prty_bb_b0_regs[1] = {
	&msdm_prty1_bb_b0,
};

static const u16 msdm_prty1_k2_attn_idx[11] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
};

static struct attn_hw_reg msdm_prty1_k2 = {
	0, 11, msdm_prty1_k2_attn_idx, 0xfc0200, 0xfc020c, 0xfc0208, 0xfc0204
};

static struct attn_hw_reg *msdm_prty_k2_regs[1] = {
	&msdm_prty1_k2,
};

#ifdef ATTN_DESC
static const char *usdm_int_attn_desc[28] = {
	"usdm_address_error",
	"usdm_inp_queue_error",
	"usdm_delay_fifo_error",
	"usdm_async_host_error",
	"usdm_prm_fifo_error",
	"usdm_ccfc_load_pend_error",
	"usdm_tcfc_load_pend_error",
	"usdm_dst_int_ram_wait_error",
	"usdm_dst_pas_buf_wait_error",
	"usdm_dst_pxp_immed_error",
	"usdm_dst_pxp_dst_pend_error",
	"usdm_dst_brb_src_pend_error",
	"usdm_dst_brb_src_addr_error",
	"usdm_rsp_brb_pend_error",
	"usdm_rsp_int_ram_pend_error",
	"usdm_rsp_brb_rd_data_error",
	"usdm_rsp_int_ram_rd_data_error",
	"usdm_rsp_pxp_rd_data_error",
	"usdm_cm_delay_error",
	"usdm_sh_delay_error",
	"usdm_cmpl_pend_error",
	"usdm_cprm_pend_error",
	"usdm_timer_addr_error",
	"usdm_timer_pend_error",
	"usdm_dorq_dpm_error",
	"usdm_dst_pxp_done_error",
	"usdm_xcm_rmt_buffer_error",
	"usdm_ycm_rmt_buffer_error",
};
#else
#define usdm_int_attn_desc OSAL_NULL
#endif

static const u16 usdm_int0_bb_a0_attn_idx[26] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25,
};

static struct attn_hw_reg usdm_int0_bb_a0 = {
	0, 26, usdm_int0_bb_a0_attn_idx, 0xfd0040, 0xfd004c, 0xfd0048, 0xfd0044
};

static struct attn_hw_reg *usdm_int_bb_a0_regs[1] = {
	&usdm_int0_bb_a0,
};

static const u16 usdm_int0_bb_b0_attn_idx[26] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25,
};

static struct attn_hw_reg usdm_int0_bb_b0 = {
	0, 26, usdm_int0_bb_b0_attn_idx, 0xfd0040, 0xfd004c, 0xfd0048, 0xfd0044
};

static struct attn_hw_reg *usdm_int_bb_b0_regs[1] = {
	&usdm_int0_bb_b0,
};

static const u16 usdm_int0_k2_attn_idx[28] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25, 26, 27,
};

static struct attn_hw_reg usdm_int0_k2 = {
	0, 28, usdm_int0_k2_attn_idx, 0xfd0040, 0xfd004c, 0xfd0048, 0xfd0044
};

static struct attn_hw_reg *usdm_int_k2_regs[1] = {
	&usdm_int0_k2,
};

#ifdef ATTN_DESC
static const char *usdm_prty_attn_desc[10] = {
	"usdm_mem008_i_mem_prty",
	"usdm_mem007_i_mem_prty",
	"usdm_mem006_i_mem_prty",
	"usdm_mem005_i_mem_prty",
	"usdm_mem002_i_mem_prty",
	"usdm_mem010_i_mem_prty",
	"usdm_mem001_i_mem_prty",
	"usdm_mem003_i_mem_prty",
	"usdm_mem004_i_mem_prty",
	"usdm_mem009_i_mem_prty",
};
#else
#define usdm_prty_attn_desc OSAL_NULL
#endif

static const u16 usdm_prty1_bb_a0_attn_idx[10] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
};

static struct attn_hw_reg usdm_prty1_bb_a0 = {
	0, 10, usdm_prty1_bb_a0_attn_idx, 0xfd0200, 0xfd020c, 0xfd0208,
	0xfd0204
};

static struct attn_hw_reg *usdm_prty_bb_a0_regs[1] = {
	&usdm_prty1_bb_a0,
};

static const u16 usdm_prty1_bb_b0_attn_idx[10] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
};

static struct attn_hw_reg usdm_prty1_bb_b0 = {
	0, 10, usdm_prty1_bb_b0_attn_idx, 0xfd0200, 0xfd020c, 0xfd0208,
	0xfd0204
};

static struct attn_hw_reg *usdm_prty_bb_b0_regs[1] = {
	&usdm_prty1_bb_b0,
};

static const u16 usdm_prty1_k2_attn_idx[10] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
};

static struct attn_hw_reg usdm_prty1_k2 = {
	0, 10, usdm_prty1_k2_attn_idx, 0xfd0200, 0xfd020c, 0xfd0208, 0xfd0204
};

static struct attn_hw_reg *usdm_prty_k2_regs[1] = {
	&usdm_prty1_k2,
};

#ifdef ATTN_DESC
static const char *xsdm_int_attn_desc[28] = {
	"xsdm_address_error",
	"xsdm_inp_queue_error",
	"xsdm_delay_fifo_error",
	"xsdm_async_host_error",
	"xsdm_prm_fifo_error",
	"xsdm_ccfc_load_pend_error",
	"xsdm_tcfc_load_pend_error",
	"xsdm_dst_int_ram_wait_error",
	"xsdm_dst_pas_buf_wait_error",
	"xsdm_dst_pxp_immed_error",
	"xsdm_dst_pxp_dst_pend_error",
	"xsdm_dst_brb_src_pend_error",
	"xsdm_dst_brb_src_addr_error",
	"xsdm_rsp_brb_pend_error",
	"xsdm_rsp_int_ram_pend_error",
	"xsdm_rsp_brb_rd_data_error",
	"xsdm_rsp_int_ram_rd_data_error",
	"xsdm_rsp_pxp_rd_data_error",
	"xsdm_cm_delay_error",
	"xsdm_sh_delay_error",
	"xsdm_cmpl_pend_error",
	"xsdm_cprm_pend_error",
	"xsdm_timer_addr_error",
	"xsdm_timer_pend_error",
	"xsdm_dorq_dpm_error",
	"xsdm_dst_pxp_done_error",
	"xsdm_xcm_rmt_buffer_error",
	"xsdm_ycm_rmt_buffer_error",
};
#else
#define xsdm_int_attn_desc OSAL_NULL
#endif

static const u16 xsdm_int0_bb_a0_attn_idx[26] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25,
};

static struct attn_hw_reg xsdm_int0_bb_a0 = {
	0, 26, xsdm_int0_bb_a0_attn_idx, 0xf80040, 0xf8004c, 0xf80048, 0xf80044
};

static struct attn_hw_reg *xsdm_int_bb_a0_regs[1] = {
	&xsdm_int0_bb_a0,
};

static const u16 xsdm_int0_bb_b0_attn_idx[26] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25,
};

static struct attn_hw_reg xsdm_int0_bb_b0 = {
	0, 26, xsdm_int0_bb_b0_attn_idx, 0xf80040, 0xf8004c, 0xf80048, 0xf80044
};

static struct attn_hw_reg *xsdm_int_bb_b0_regs[1] = {
	&xsdm_int0_bb_b0,
};

static const u16 xsdm_int0_k2_attn_idx[28] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25, 26, 27,
};

static struct attn_hw_reg xsdm_int0_k2 = {
	0, 28, xsdm_int0_k2_attn_idx, 0xf80040, 0xf8004c, 0xf80048, 0xf80044
};

static struct attn_hw_reg *xsdm_int_k2_regs[1] = {
	&xsdm_int0_k2,
};

#ifdef ATTN_DESC
static const char *xsdm_prty_attn_desc[10] = {
	"xsdm_mem009_i_mem_prty",
	"xsdm_mem008_i_mem_prty",
	"xsdm_mem007_i_mem_prty",
	"xsdm_mem006_i_mem_prty",
	"xsdm_mem003_i_mem_prty",
	"xsdm_mem010_i_mem_prty",
	"xsdm_mem002_i_mem_prty",
	"xsdm_mem004_i_mem_prty",
	"xsdm_mem005_i_mem_prty",
	"xsdm_mem001_i_mem_prty",
};
#else
#define xsdm_prty_attn_desc OSAL_NULL
#endif

static const u16 xsdm_prty1_bb_a0_attn_idx[10] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
};

static struct attn_hw_reg xsdm_prty1_bb_a0 = {
	0, 10, xsdm_prty1_bb_a0_attn_idx, 0xf80200, 0xf8020c, 0xf80208,
	0xf80204
};

static struct attn_hw_reg *xsdm_prty_bb_a0_regs[1] = {
	&xsdm_prty1_bb_a0,
};

static const u16 xsdm_prty1_bb_b0_attn_idx[10] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
};

static struct attn_hw_reg xsdm_prty1_bb_b0 = {
	0, 10, xsdm_prty1_bb_b0_attn_idx, 0xf80200, 0xf8020c, 0xf80208,
	0xf80204
};

static struct attn_hw_reg *xsdm_prty_bb_b0_regs[1] = {
	&xsdm_prty1_bb_b0,
};

static const u16 xsdm_prty1_k2_attn_idx[10] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
};

static struct attn_hw_reg xsdm_prty1_k2 = {
	0, 10, xsdm_prty1_k2_attn_idx, 0xf80200, 0xf8020c, 0xf80208, 0xf80204
};

static struct attn_hw_reg *xsdm_prty_k2_regs[1] = {
	&xsdm_prty1_k2,
};

#ifdef ATTN_DESC
static const char *ysdm_int_attn_desc[28] = {
	"ysdm_address_error",
	"ysdm_inp_queue_error",
	"ysdm_delay_fifo_error",
	"ysdm_async_host_error",
	"ysdm_prm_fifo_error",
	"ysdm_ccfc_load_pend_error",
	"ysdm_tcfc_load_pend_error",
	"ysdm_dst_int_ram_wait_error",
	"ysdm_dst_pas_buf_wait_error",
	"ysdm_dst_pxp_immed_error",
	"ysdm_dst_pxp_dst_pend_error",
	"ysdm_dst_brb_src_pend_error",
	"ysdm_dst_brb_src_addr_error",
	"ysdm_rsp_brb_pend_error",
	"ysdm_rsp_int_ram_pend_error",
	"ysdm_rsp_brb_rd_data_error",
	"ysdm_rsp_int_ram_rd_data_error",
	"ysdm_rsp_pxp_rd_data_error",
	"ysdm_cm_delay_error",
	"ysdm_sh_delay_error",
	"ysdm_cmpl_pend_error",
	"ysdm_cprm_pend_error",
	"ysdm_timer_addr_error",
	"ysdm_timer_pend_error",
	"ysdm_dorq_dpm_error",
	"ysdm_dst_pxp_done_error",
	"ysdm_xcm_rmt_buffer_error",
	"ysdm_ycm_rmt_buffer_error",
};
#else
#define ysdm_int_attn_desc OSAL_NULL
#endif

static const u16 ysdm_int0_bb_a0_attn_idx[26] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25,
};

static struct attn_hw_reg ysdm_int0_bb_a0 = {
	0, 26, ysdm_int0_bb_a0_attn_idx, 0xf90040, 0xf9004c, 0xf90048, 0xf90044
};

static struct attn_hw_reg *ysdm_int_bb_a0_regs[1] = {
	&ysdm_int0_bb_a0,
};

static const u16 ysdm_int0_bb_b0_attn_idx[26] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25,
};

static struct attn_hw_reg ysdm_int0_bb_b0 = {
	0, 26, ysdm_int0_bb_b0_attn_idx, 0xf90040, 0xf9004c, 0xf90048, 0xf90044
};

static struct attn_hw_reg *ysdm_int_bb_b0_regs[1] = {
	&ysdm_int0_bb_b0,
};

static const u16 ysdm_int0_k2_attn_idx[28] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25, 26, 27,
};

static struct attn_hw_reg ysdm_int0_k2 = {
	0, 28, ysdm_int0_k2_attn_idx, 0xf90040, 0xf9004c, 0xf90048, 0xf90044
};

static struct attn_hw_reg *ysdm_int_k2_regs[1] = {
	&ysdm_int0_k2,
};

#ifdef ATTN_DESC
static const char *ysdm_prty_attn_desc[9] = {
	"ysdm_mem008_i_mem_prty",
	"ysdm_mem007_i_mem_prty",
	"ysdm_mem006_i_mem_prty",
	"ysdm_mem005_i_mem_prty",
	"ysdm_mem002_i_mem_prty",
	"ysdm_mem009_i_mem_prty",
	"ysdm_mem001_i_mem_prty",
	"ysdm_mem003_i_mem_prty",
	"ysdm_mem004_i_mem_prty",
};
#else
#define ysdm_prty_attn_desc OSAL_NULL
#endif

static const u16 ysdm_prty1_bb_a0_attn_idx[9] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8,
};

static struct attn_hw_reg ysdm_prty1_bb_a0 = {
	0, 9, ysdm_prty1_bb_a0_attn_idx, 0xf90200, 0xf9020c, 0xf90208, 0xf90204
};

static struct attn_hw_reg *ysdm_prty_bb_a0_regs[1] = {
	&ysdm_prty1_bb_a0,
};

static const u16 ysdm_prty1_bb_b0_attn_idx[9] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8,
};

static struct attn_hw_reg ysdm_prty1_bb_b0 = {
	0, 9, ysdm_prty1_bb_b0_attn_idx, 0xf90200, 0xf9020c, 0xf90208, 0xf90204
};

static struct attn_hw_reg *ysdm_prty_bb_b0_regs[1] = {
	&ysdm_prty1_bb_b0,
};

static const u16 ysdm_prty1_k2_attn_idx[9] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8,
};

static struct attn_hw_reg ysdm_prty1_k2 = {
	0, 9, ysdm_prty1_k2_attn_idx, 0xf90200, 0xf9020c, 0xf90208, 0xf90204
};

static struct attn_hw_reg *ysdm_prty_k2_regs[1] = {
	&ysdm_prty1_k2,
};

#ifdef ATTN_DESC
static const char *psdm_int_attn_desc[28] = {
	"psdm_address_error",
	"psdm_inp_queue_error",
	"psdm_delay_fifo_error",
	"psdm_async_host_error",
	"psdm_prm_fifo_error",
	"psdm_ccfc_load_pend_error",
	"psdm_tcfc_load_pend_error",
	"psdm_dst_int_ram_wait_error",
	"psdm_dst_pas_buf_wait_error",
	"psdm_dst_pxp_immed_error",
	"psdm_dst_pxp_dst_pend_error",
	"psdm_dst_brb_src_pend_error",
	"psdm_dst_brb_src_addr_error",
	"psdm_rsp_brb_pend_error",
	"psdm_rsp_int_ram_pend_error",
	"psdm_rsp_brb_rd_data_error",
	"psdm_rsp_int_ram_rd_data_error",
	"psdm_rsp_pxp_rd_data_error",
	"psdm_cm_delay_error",
	"psdm_sh_delay_error",
	"psdm_cmpl_pend_error",
	"psdm_cprm_pend_error",
	"psdm_timer_addr_error",
	"psdm_timer_pend_error",
	"psdm_dorq_dpm_error",
	"psdm_dst_pxp_done_error",
	"psdm_xcm_rmt_buffer_error",
	"psdm_ycm_rmt_buffer_error",
};
#else
#define psdm_int_attn_desc OSAL_NULL
#endif

static const u16 psdm_int0_bb_a0_attn_idx[26] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25,
};

static struct attn_hw_reg psdm_int0_bb_a0 = {
	0, 26, psdm_int0_bb_a0_attn_idx, 0xfa0040, 0xfa004c, 0xfa0048, 0xfa0044
};

static struct attn_hw_reg *psdm_int_bb_a0_regs[1] = {
	&psdm_int0_bb_a0,
};

static const u16 psdm_int0_bb_b0_attn_idx[26] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25,
};

static struct attn_hw_reg psdm_int0_bb_b0 = {
	0, 26, psdm_int0_bb_b0_attn_idx, 0xfa0040, 0xfa004c, 0xfa0048, 0xfa0044
};

static struct attn_hw_reg *psdm_int_bb_b0_regs[1] = {
	&psdm_int0_bb_b0,
};

static const u16 psdm_int0_k2_attn_idx[28] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25, 26, 27,
};

static struct attn_hw_reg psdm_int0_k2 = {
	0, 28, psdm_int0_k2_attn_idx, 0xfa0040, 0xfa004c, 0xfa0048, 0xfa0044
};

static struct attn_hw_reg *psdm_int_k2_regs[1] = {
	&psdm_int0_k2,
};

#ifdef ATTN_DESC
static const char *psdm_prty_attn_desc[9] = {
	"psdm_mem008_i_mem_prty",
	"psdm_mem007_i_mem_prty",
	"psdm_mem006_i_mem_prty",
	"psdm_mem005_i_mem_prty",
	"psdm_mem002_i_mem_prty",
	"psdm_mem009_i_mem_prty",
	"psdm_mem001_i_mem_prty",
	"psdm_mem003_i_mem_prty",
	"psdm_mem004_i_mem_prty",
};
#else
#define psdm_prty_attn_desc OSAL_NULL
#endif

static const u16 psdm_prty1_bb_a0_attn_idx[9] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8,
};

static struct attn_hw_reg psdm_prty1_bb_a0 = {
	0, 9, psdm_prty1_bb_a0_attn_idx, 0xfa0200, 0xfa020c, 0xfa0208, 0xfa0204
};

static struct attn_hw_reg *psdm_prty_bb_a0_regs[1] = {
	&psdm_prty1_bb_a0,
};

static const u16 psdm_prty1_bb_b0_attn_idx[9] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8,
};

static struct attn_hw_reg psdm_prty1_bb_b0 = {
	0, 9, psdm_prty1_bb_b0_attn_idx, 0xfa0200, 0xfa020c, 0xfa0208, 0xfa0204
};

static struct attn_hw_reg *psdm_prty_bb_b0_regs[1] = {
	&psdm_prty1_bb_b0,
};

static const u16 psdm_prty1_k2_attn_idx[9] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8,
};

static struct attn_hw_reg psdm_prty1_k2 = {
	0, 9, psdm_prty1_k2_attn_idx, 0xfa0200, 0xfa020c, 0xfa0208, 0xfa0204
};

static struct attn_hw_reg *psdm_prty_k2_regs[1] = {
	&psdm_prty1_k2,
};

#ifdef ATTN_DESC
static const char *tsem_int_attn_desc[46] = {
	"tsem_address_error",
	"tsem_fic_last_error",
	"tsem_fic_length_error",
	"tsem_fic_fifo_error",
	"tsem_pas_buf_fifo_error",
	"tsem_sync_fin_pop_error",
	"tsem_sync_dra_wr_push_error",
	"tsem_sync_dra_wr_pop_error",
	"tsem_sync_dra_rd_push_error",
	"tsem_sync_dra_rd_pop_error",
	"tsem_sync_fin_push_error",
	"tsem_sem_fast_address_error",
	"tsem_cam_lsb_inp_fifo",
	"tsem_cam_msb_inp_fifo",
	"tsem_cam_out_fifo",
	"tsem_fin_fifo",
	"tsem_thread_fifo_error",
	"tsem_thread_overrun",
	"tsem_sync_ext_store_push_error",
	"tsem_sync_ext_store_pop_error",
	"tsem_sync_ext_load_push_error",
	"tsem_sync_ext_load_pop_error",
	"tsem_sync_ram_rd_push_error",
	"tsem_sync_ram_rd_pop_error",
	"tsem_sync_ram_wr_pop_error",
	"tsem_sync_ram_wr_push_error",
	"tsem_sync_dbg_push_error",
	"tsem_sync_dbg_pop_error",
	"tsem_dbg_fifo_error",
	"tsem_cam_msb2_inp_fifo",
	"tsem_vfc_interrupt",
	"tsem_vfc_out_fifo_error",
	"tsem_storm_stack_uf_attn",
	"tsem_storm_stack_of_attn",
	"tsem_storm_runtime_error",
	"tsem_ext_load_pend_wr_error",
	"tsem_thread_rls_orun_error",
	"tsem_thread_rls_aloc_error",
	"tsem_thread_rls_vld_error",
	"tsem_ext_thread_oor_error",
	"tsem_ord_id_fifo_error",
	"tsem_invld_foc_error",
	"tsem_ext_ld_len_error",
	"tsem_thrd_ord_fifo_error",
	"tsem_invld_thrd_ord_error",
	"tsem_fast_memory_address_error",
};
#else
#define tsem_int_attn_desc OSAL_NULL
#endif

static const u16 tsem_int0_bb_a0_attn_idx[32] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
};

static struct attn_hw_reg tsem_int0_bb_a0 = {
	0, 32, tsem_int0_bb_a0_attn_idx, 0x1700040, 0x170004c, 0x1700048,
	0x1700044
};

static const u16 tsem_int1_bb_a0_attn_idx[13] = {
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44,
};

static struct attn_hw_reg tsem_int1_bb_a0 = {
	1, 13, tsem_int1_bb_a0_attn_idx, 0x1700050, 0x170005c, 0x1700058,
	0x1700054
};

static const u16 tsem_fast_memory_int0_bb_a0_attn_idx[1] = {
	45,
};

static struct attn_hw_reg tsem_fast_memory_int0_bb_a0 = {
	2, 1, tsem_fast_memory_int0_bb_a0_attn_idx, 0x1740040, 0x174004c,
	0x1740048, 0x1740044
};

static struct attn_hw_reg *tsem_int_bb_a0_regs[3] = {
	&tsem_int0_bb_a0, &tsem_int1_bb_a0, &tsem_fast_memory_int0_bb_a0,
};

static const u16 tsem_int0_bb_b0_attn_idx[32] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
};

static struct attn_hw_reg tsem_int0_bb_b0 = {
	0, 32, tsem_int0_bb_b0_attn_idx, 0x1700040, 0x170004c, 0x1700048,
	0x1700044
};

static const u16 tsem_int1_bb_b0_attn_idx[13] = {
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44,
};

static struct attn_hw_reg tsem_int1_bb_b0 = {
	1, 13, tsem_int1_bb_b0_attn_idx, 0x1700050, 0x170005c, 0x1700058,
	0x1700054
};

static const u16 tsem_fast_memory_int0_bb_b0_attn_idx[1] = {
	45,
};

static struct attn_hw_reg tsem_fast_memory_int0_bb_b0 = {
	2, 1, tsem_fast_memory_int0_bb_b0_attn_idx, 0x1740040, 0x174004c,
	0x1740048, 0x1740044
};

static struct attn_hw_reg *tsem_int_bb_b0_regs[3] = {
	&tsem_int0_bb_b0, &tsem_int1_bb_b0, &tsem_fast_memory_int0_bb_b0,
};

static const u16 tsem_int0_k2_attn_idx[32] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
};

static struct attn_hw_reg tsem_int0_k2 = {
	0, 32, tsem_int0_k2_attn_idx, 0x1700040, 0x170004c, 0x1700048,
	0x1700044
};

static const u16 tsem_int1_k2_attn_idx[13] = {
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44,
};

static struct attn_hw_reg tsem_int1_k2 = {
	1, 13, tsem_int1_k2_attn_idx, 0x1700050, 0x170005c, 0x1700058,
	0x1700054
};

static const u16 tsem_fast_memory_int0_k2_attn_idx[1] = {
	45,
};

static struct attn_hw_reg tsem_fast_memory_int0_k2 = {
	2, 1, tsem_fast_memory_int0_k2_attn_idx, 0x1740040, 0x174004c,
	0x1740048,
	0x1740044
};

static struct attn_hw_reg *tsem_int_k2_regs[3] = {
	&tsem_int0_k2, &tsem_int1_k2, &tsem_fast_memory_int0_k2,
};

#ifdef ATTN_DESC
static const char *tsem_prty_attn_desc[23] = {
	"tsem_vfc_rbc_parity_error",
	"tsem_storm_rf_parity_error",
	"tsem_reg_gen_parity_error",
	"tsem_mem005_i_ecc_0_rf_int",
	"tsem_mem005_i_ecc_1_rf_int",
	"tsem_mem004_i_mem_prty",
	"tsem_mem002_i_mem_prty",
	"tsem_mem003_i_mem_prty",
	"tsem_mem001_i_mem_prty",
	"tsem_fast_memory_mem024_i_mem_prty",
	"tsem_fast_memory_mem023_i_mem_prty",
	"tsem_fast_memory_mem022_i_mem_prty",
	"tsem_fast_memory_mem021_i_mem_prty",
	"tsem_fast_memory_mem020_i_mem_prty",
	"tsem_fast_memory_mem019_i_mem_prty",
	"tsem_fast_memory_mem018_i_mem_prty",
	"tsem_fast_memory_vfc_config_mem005_i_ecc_rf_int",
	"tsem_fast_memory_vfc_config_mem002_i_ecc_rf_int",
	"tsem_fast_memory_vfc_config_mem006_i_mem_prty",
	"tsem_fast_memory_vfc_config_mem001_i_mem_prty",
	"tsem_fast_memory_vfc_config_mem004_i_mem_prty",
	"tsem_fast_memory_vfc_config_mem003_i_mem_prty",
	"tsem_fast_memory_vfc_config_mem007_i_mem_prty",
};
#else
#define tsem_prty_attn_desc OSAL_NULL
#endif

static const u16 tsem_prty0_bb_a0_attn_idx[3] = {
	0, 1, 2,
};

static struct attn_hw_reg tsem_prty0_bb_a0 = {
	0, 3, tsem_prty0_bb_a0_attn_idx, 0x17000c8, 0x17000d4, 0x17000d0,
	0x17000cc
};

static const u16 tsem_prty1_bb_a0_attn_idx[6] = {
	3, 4, 5, 6, 7, 8,
};

static struct attn_hw_reg tsem_prty1_bb_a0 = {
	1, 6, tsem_prty1_bb_a0_attn_idx, 0x1700200, 0x170020c, 0x1700208,
	0x1700204
};

static const u16 tsem_fast_memory_vfc_config_prty1_bb_a0_attn_idx[6] = {
	16, 17, 19, 20, 21, 22,
};

static struct attn_hw_reg tsem_fast_memory_vfc_config_prty1_bb_a0 = {
	2, 6, tsem_fast_memory_vfc_config_prty1_bb_a0_attn_idx, 0x174a200,
	0x174a20c, 0x174a208, 0x174a204
};

static struct attn_hw_reg *tsem_prty_bb_a0_regs[3] = {
	&tsem_prty0_bb_a0, &tsem_prty1_bb_a0,
	&tsem_fast_memory_vfc_config_prty1_bb_a0,
};

static const u16 tsem_prty0_bb_b0_attn_idx[3] = {
	0, 1, 2,
};

static struct attn_hw_reg tsem_prty0_bb_b0 = {
	0, 3, tsem_prty0_bb_b0_attn_idx, 0x17000c8, 0x17000d4, 0x17000d0,
	0x17000cc
};

static const u16 tsem_prty1_bb_b0_attn_idx[6] = {
	3, 4, 5, 6, 7, 8,
};

static struct attn_hw_reg tsem_prty1_bb_b0 = {
	1, 6, tsem_prty1_bb_b0_attn_idx, 0x1700200, 0x170020c, 0x1700208,
	0x1700204
};

static const u16 tsem_fast_memory_vfc_config_prty1_bb_b0_attn_idx[6] = {
	16, 17, 19, 20, 21, 22,
};

static struct attn_hw_reg tsem_fast_memory_vfc_config_prty1_bb_b0 = {
	2, 6, tsem_fast_memory_vfc_config_prty1_bb_b0_attn_idx, 0x174a200,
	0x174a20c, 0x174a208, 0x174a204
};

static struct attn_hw_reg *tsem_prty_bb_b0_regs[3] = {
	&tsem_prty0_bb_b0, &tsem_prty1_bb_b0,
	&tsem_fast_memory_vfc_config_prty1_bb_b0,
};

static const u16 tsem_prty0_k2_attn_idx[3] = {
	0, 1, 2,
};

static struct attn_hw_reg tsem_prty0_k2 = {
	0, 3, tsem_prty0_k2_attn_idx, 0x17000c8, 0x17000d4, 0x17000d0,
	0x17000cc
};

static const u16 tsem_prty1_k2_attn_idx[6] = {
	3, 4, 5, 6, 7, 8,
};

static struct attn_hw_reg tsem_prty1_k2 = {
	1, 6, tsem_prty1_k2_attn_idx, 0x1700200, 0x170020c, 0x1700208,
	0x1700204
};

static const u16 tsem_fast_memory_prty1_k2_attn_idx[7] = {
	9, 10, 11, 12, 13, 14, 15,
};

static struct attn_hw_reg tsem_fast_memory_prty1_k2 = {
	2, 7, tsem_fast_memory_prty1_k2_attn_idx, 0x1740200, 0x174020c,
	0x1740208,
	0x1740204
};

static const u16 tsem_fast_memory_vfc_config_prty1_k2_attn_idx[6] = {
	16, 17, 18, 19, 20, 21,
};

static struct attn_hw_reg tsem_fast_memory_vfc_config_prty1_k2 = {
	3, 6, tsem_fast_memory_vfc_config_prty1_k2_attn_idx, 0x174a200,
	0x174a20c,
	0x174a208, 0x174a204
};

static struct attn_hw_reg *tsem_prty_k2_regs[4] = {
	&tsem_prty0_k2, &tsem_prty1_k2, &tsem_fast_memory_prty1_k2,
	&tsem_fast_memory_vfc_config_prty1_k2,
};

#ifdef ATTN_DESC
static const char *msem_int_attn_desc[46] = {
	"msem_address_error",
	"msem_fic_last_error",
	"msem_fic_length_error",
	"msem_fic_fifo_error",
	"msem_pas_buf_fifo_error",
	"msem_sync_fin_pop_error",
	"msem_sync_dra_wr_push_error",
	"msem_sync_dra_wr_pop_error",
	"msem_sync_dra_rd_push_error",
	"msem_sync_dra_rd_pop_error",
	"msem_sync_fin_push_error",
	"msem_sem_fast_address_error",
	"msem_cam_lsb_inp_fifo",
	"msem_cam_msb_inp_fifo",
	"msem_cam_out_fifo",
	"msem_fin_fifo",
	"msem_thread_fifo_error",
	"msem_thread_overrun",
	"msem_sync_ext_store_push_error",
	"msem_sync_ext_store_pop_error",
	"msem_sync_ext_load_push_error",
	"msem_sync_ext_load_pop_error",
	"msem_sync_ram_rd_push_error",
	"msem_sync_ram_rd_pop_error",
	"msem_sync_ram_wr_pop_error",
	"msem_sync_ram_wr_push_error",
	"msem_sync_dbg_push_error",
	"msem_sync_dbg_pop_error",
	"msem_dbg_fifo_error",
	"msem_cam_msb2_inp_fifo",
	"msem_vfc_interrupt",
	"msem_vfc_out_fifo_error",
	"msem_storm_stack_uf_attn",
	"msem_storm_stack_of_attn",
	"msem_storm_runtime_error",
	"msem_ext_load_pend_wr_error",
	"msem_thread_rls_orun_error",
	"msem_thread_rls_aloc_error",
	"msem_thread_rls_vld_error",
	"msem_ext_thread_oor_error",
	"msem_ord_id_fifo_error",
	"msem_invld_foc_error",
	"msem_ext_ld_len_error",
	"msem_thrd_ord_fifo_error",
	"msem_invld_thrd_ord_error",
	"msem_fast_memory_address_error",
};
#else
#define msem_int_attn_desc OSAL_NULL
#endif

static const u16 msem_int0_bb_a0_attn_idx[32] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
};

static struct attn_hw_reg msem_int0_bb_a0 = {
	0, 32, msem_int0_bb_a0_attn_idx, 0x1800040, 0x180004c, 0x1800048,
	0x1800044
};

static const u16 msem_int1_bb_a0_attn_idx[13] = {
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44,
};

static struct attn_hw_reg msem_int1_bb_a0 = {
	1, 13, msem_int1_bb_a0_attn_idx, 0x1800050, 0x180005c, 0x1800058,
	0x1800054
};

static const u16 msem_fast_memory_int0_bb_a0_attn_idx[1] = {
	45,
};

static struct attn_hw_reg msem_fast_memory_int0_bb_a0 = {
	2, 1, msem_fast_memory_int0_bb_a0_attn_idx, 0x1840040, 0x184004c,
	0x1840048, 0x1840044
};

static struct attn_hw_reg *msem_int_bb_a0_regs[3] = {
	&msem_int0_bb_a0, &msem_int1_bb_a0, &msem_fast_memory_int0_bb_a0,
};

static const u16 msem_int0_bb_b0_attn_idx[32] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
};

static struct attn_hw_reg msem_int0_bb_b0 = {
	0, 32, msem_int0_bb_b0_attn_idx, 0x1800040, 0x180004c, 0x1800048,
	0x1800044
};

static const u16 msem_int1_bb_b0_attn_idx[13] = {
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44,
};

static struct attn_hw_reg msem_int1_bb_b0 = {
	1, 13, msem_int1_bb_b0_attn_idx, 0x1800050, 0x180005c, 0x1800058,
	0x1800054
};

static const u16 msem_fast_memory_int0_bb_b0_attn_idx[1] = {
	45,
};

static struct attn_hw_reg msem_fast_memory_int0_bb_b0 = {
	2, 1, msem_fast_memory_int0_bb_b0_attn_idx, 0x1840040, 0x184004c,
	0x1840048, 0x1840044
};

static struct attn_hw_reg *msem_int_bb_b0_regs[3] = {
	&msem_int0_bb_b0, &msem_int1_bb_b0, &msem_fast_memory_int0_bb_b0,
};

static const u16 msem_int0_k2_attn_idx[32] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
};

static struct attn_hw_reg msem_int0_k2 = {
	0, 32, msem_int0_k2_attn_idx, 0x1800040, 0x180004c, 0x1800048,
	0x1800044
};

static const u16 msem_int1_k2_attn_idx[13] = {
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44,
};

static struct attn_hw_reg msem_int1_k2 = {
	1, 13, msem_int1_k2_attn_idx, 0x1800050, 0x180005c, 0x1800058,
	0x1800054
};

static const u16 msem_fast_memory_int0_k2_attn_idx[1] = {
	45,
};

static struct attn_hw_reg msem_fast_memory_int0_k2 = {
	2, 1, msem_fast_memory_int0_k2_attn_idx, 0x1840040, 0x184004c,
	0x1840048,
	0x1840044
};

static struct attn_hw_reg *msem_int_k2_regs[3] = {
	&msem_int0_k2, &msem_int1_k2, &msem_fast_memory_int0_k2,
};

#ifdef ATTN_DESC
static const char *msem_prty_attn_desc[23] = {
	"msem_vfc_rbc_parity_error",
	"msem_storm_rf_parity_error",
	"msem_reg_gen_parity_error",
	"msem_mem005_i_ecc_0_rf_int",
	"msem_mem005_i_ecc_1_rf_int",
	"msem_mem004_i_mem_prty",
	"msem_mem002_i_mem_prty",
	"msem_mem003_i_mem_prty",
	"msem_mem001_i_mem_prty",
	"msem_fast_memory_mem024_i_mem_prty",
	"msem_fast_memory_mem023_i_mem_prty",
	"msem_fast_memory_mem022_i_mem_prty",
	"msem_fast_memory_mem021_i_mem_prty",
	"msem_fast_memory_mem020_i_mem_prty",
	"msem_fast_memory_mem019_i_mem_prty",
	"msem_fast_memory_mem018_i_mem_prty",
	"msem_fast_memory_vfc_config_mem005_i_ecc_rf_int",
	"msem_fast_memory_vfc_config_mem002_i_ecc_rf_int",
	"msem_fast_memory_vfc_config_mem006_i_mem_prty",
	"msem_fast_memory_vfc_config_mem001_i_mem_prty",
	"msem_fast_memory_vfc_config_mem004_i_mem_prty",
	"msem_fast_memory_vfc_config_mem003_i_mem_prty",
	"msem_fast_memory_vfc_config_mem007_i_mem_prty",
};
#else
#define msem_prty_attn_desc OSAL_NULL
#endif

static const u16 msem_prty0_bb_a0_attn_idx[3] = {
	0, 1, 2,
};

static struct attn_hw_reg msem_prty0_bb_a0 = {
	0, 3, msem_prty0_bb_a0_attn_idx, 0x18000c8, 0x18000d4, 0x18000d0,
	0x18000cc
};

static const u16 msem_prty1_bb_a0_attn_idx[6] = {
	3, 4, 5, 6, 7, 8,
};

static struct attn_hw_reg msem_prty1_bb_a0 = {
	1, 6, msem_prty1_bb_a0_attn_idx, 0x1800200, 0x180020c, 0x1800208,
	0x1800204
};

static struct attn_hw_reg *msem_prty_bb_a0_regs[2] = {
	&msem_prty0_bb_a0, &msem_prty1_bb_a0,
};

static const u16 msem_prty0_bb_b0_attn_idx[3] = {
	0, 1, 2,
};

static struct attn_hw_reg msem_prty0_bb_b0 = {
	0, 3, msem_prty0_bb_b0_attn_idx, 0x18000c8, 0x18000d4, 0x18000d0,
	0x18000cc
};

static const u16 msem_prty1_bb_b0_attn_idx[6] = {
	3, 4, 5, 6, 7, 8,
};

static struct attn_hw_reg msem_prty1_bb_b0 = {
	1, 6, msem_prty1_bb_b0_attn_idx, 0x1800200, 0x180020c, 0x1800208,
	0x1800204
};

static struct attn_hw_reg *msem_prty_bb_b0_regs[2] = {
	&msem_prty0_bb_b0, &msem_prty1_bb_b0,
};

static const u16 msem_prty0_k2_attn_idx[3] = {
	0, 1, 2,
};

static struct attn_hw_reg msem_prty0_k2 = {
	0, 3, msem_prty0_k2_attn_idx, 0x18000c8, 0x18000d4, 0x18000d0,
	0x18000cc
};

static const u16 msem_prty1_k2_attn_idx[6] = {
	3, 4, 5, 6, 7, 8,
};

static struct attn_hw_reg msem_prty1_k2 = {
	1, 6, msem_prty1_k2_attn_idx, 0x1800200, 0x180020c, 0x1800208,
	0x1800204
};

static const u16 msem_fast_memory_prty1_k2_attn_idx[7] = {
	9, 10, 11, 12, 13, 14, 15,
};

static struct attn_hw_reg msem_fast_memory_prty1_k2 = {
	2, 7, msem_fast_memory_prty1_k2_attn_idx, 0x1840200, 0x184020c,
	0x1840208,
	0x1840204
};

static struct attn_hw_reg *msem_prty_k2_regs[3] = {
	&msem_prty0_k2, &msem_prty1_k2, &msem_fast_memory_prty1_k2,
};

#ifdef ATTN_DESC
static const char *usem_int_attn_desc[46] = {
	"usem_address_error",
	"usem_fic_last_error",
	"usem_fic_length_error",
	"usem_fic_fifo_error",
	"usem_pas_buf_fifo_error",
	"usem_sync_fin_pop_error",
	"usem_sync_dra_wr_push_error",
	"usem_sync_dra_wr_pop_error",
	"usem_sync_dra_rd_push_error",
	"usem_sync_dra_rd_pop_error",
	"usem_sync_fin_push_error",
	"usem_sem_fast_address_error",
	"usem_cam_lsb_inp_fifo",
	"usem_cam_msb_inp_fifo",
	"usem_cam_out_fifo",
	"usem_fin_fifo",
	"usem_thread_fifo_error",
	"usem_thread_overrun",
	"usem_sync_ext_store_push_error",
	"usem_sync_ext_store_pop_error",
	"usem_sync_ext_load_push_error",
	"usem_sync_ext_load_pop_error",
	"usem_sync_ram_rd_push_error",
	"usem_sync_ram_rd_pop_error",
	"usem_sync_ram_wr_pop_error",
	"usem_sync_ram_wr_push_error",
	"usem_sync_dbg_push_error",
	"usem_sync_dbg_pop_error",
	"usem_dbg_fifo_error",
	"usem_cam_msb2_inp_fifo",
	"usem_vfc_interrupt",
	"usem_vfc_out_fifo_error",
	"usem_storm_stack_uf_attn",
	"usem_storm_stack_of_attn",
	"usem_storm_runtime_error",
	"usem_ext_load_pend_wr_error",
	"usem_thread_rls_orun_error",
	"usem_thread_rls_aloc_error",
	"usem_thread_rls_vld_error",
	"usem_ext_thread_oor_error",
	"usem_ord_id_fifo_error",
	"usem_invld_foc_error",
	"usem_ext_ld_len_error",
	"usem_thrd_ord_fifo_error",
	"usem_invld_thrd_ord_error",
	"usem_fast_memory_address_error",
};
#else
#define usem_int_attn_desc OSAL_NULL
#endif

static const u16 usem_int0_bb_a0_attn_idx[32] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
};

static struct attn_hw_reg usem_int0_bb_a0 = {
	0, 32, usem_int0_bb_a0_attn_idx, 0x1900040, 0x190004c, 0x1900048,
	0x1900044
};

static const u16 usem_int1_bb_a0_attn_idx[13] = {
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44,
};

static struct attn_hw_reg usem_int1_bb_a0 = {
	1, 13, usem_int1_bb_a0_attn_idx, 0x1900050, 0x190005c, 0x1900058,
	0x1900054
};

static const u16 usem_fast_memory_int0_bb_a0_attn_idx[1] = {
	45,
};

static struct attn_hw_reg usem_fast_memory_int0_bb_a0 = {
	2, 1, usem_fast_memory_int0_bb_a0_attn_idx, 0x1940040, 0x194004c,
	0x1940048, 0x1940044
};

static struct attn_hw_reg *usem_int_bb_a0_regs[3] = {
	&usem_int0_bb_a0, &usem_int1_bb_a0, &usem_fast_memory_int0_bb_a0,
};

static const u16 usem_int0_bb_b0_attn_idx[32] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
};

static struct attn_hw_reg usem_int0_bb_b0 = {
	0, 32, usem_int0_bb_b0_attn_idx, 0x1900040, 0x190004c, 0x1900048,
	0x1900044
};

static const u16 usem_int1_bb_b0_attn_idx[13] = {
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44,
};

static struct attn_hw_reg usem_int1_bb_b0 = {
	1, 13, usem_int1_bb_b0_attn_idx, 0x1900050, 0x190005c, 0x1900058,
	0x1900054
};

static const u16 usem_fast_memory_int0_bb_b0_attn_idx[1] = {
	45,
};

static struct attn_hw_reg usem_fast_memory_int0_bb_b0 = {
	2, 1, usem_fast_memory_int0_bb_b0_attn_idx, 0x1940040, 0x194004c,
	0x1940048, 0x1940044
};

static struct attn_hw_reg *usem_int_bb_b0_regs[3] = {
	&usem_int0_bb_b0, &usem_int1_bb_b0, &usem_fast_memory_int0_bb_b0,
};

static const u16 usem_int0_k2_attn_idx[32] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
};

static struct attn_hw_reg usem_int0_k2 = {
	0, 32, usem_int0_k2_attn_idx, 0x1900040, 0x190004c, 0x1900048,
	0x1900044
};

static const u16 usem_int1_k2_attn_idx[13] = {
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44,
};

static struct attn_hw_reg usem_int1_k2 = {
	1, 13, usem_int1_k2_attn_idx, 0x1900050, 0x190005c, 0x1900058,
	0x1900054
};

static const u16 usem_fast_memory_int0_k2_attn_idx[1] = {
	45,
};

static struct attn_hw_reg usem_fast_memory_int0_k2 = {
	2, 1, usem_fast_memory_int0_k2_attn_idx, 0x1940040, 0x194004c,
	0x1940048,
	0x1940044
};

static struct attn_hw_reg *usem_int_k2_regs[3] = {
	&usem_int0_k2, &usem_int1_k2, &usem_fast_memory_int0_k2,
};

#ifdef ATTN_DESC
static const char *usem_prty_attn_desc[23] = {
	"usem_vfc_rbc_parity_error",
	"usem_storm_rf_parity_error",
	"usem_reg_gen_parity_error",
	"usem_mem005_i_ecc_0_rf_int",
	"usem_mem005_i_ecc_1_rf_int",
	"usem_mem004_i_mem_prty",
	"usem_mem002_i_mem_prty",
	"usem_mem003_i_mem_prty",
	"usem_mem001_i_mem_prty",
	"usem_fast_memory_mem024_i_mem_prty",
	"usem_fast_memory_mem023_i_mem_prty",
	"usem_fast_memory_mem022_i_mem_prty",
	"usem_fast_memory_mem021_i_mem_prty",
	"usem_fast_memory_mem020_i_mem_prty",
	"usem_fast_memory_mem019_i_mem_prty",
	"usem_fast_memory_mem018_i_mem_prty",
	"usem_fast_memory_vfc_config_mem005_i_ecc_rf_int",
	"usem_fast_memory_vfc_config_mem002_i_ecc_rf_int",
	"usem_fast_memory_vfc_config_mem006_i_mem_prty",
	"usem_fast_memory_vfc_config_mem001_i_mem_prty",
	"usem_fast_memory_vfc_config_mem004_i_mem_prty",
	"usem_fast_memory_vfc_config_mem003_i_mem_prty",
	"usem_fast_memory_vfc_config_mem007_i_mem_prty",
};
#else
#define usem_prty_attn_desc OSAL_NULL
#endif

static const u16 usem_prty0_bb_a0_attn_idx[3] = {
	0, 1, 2,
};

static struct attn_hw_reg usem_prty0_bb_a0 = {
	0, 3, usem_prty0_bb_a0_attn_idx, 0x19000c8, 0x19000d4, 0x19000d0,
	0x19000cc
};

static const u16 usem_prty1_bb_a0_attn_idx[6] = {
	3, 4, 5, 6, 7, 8,
};

static struct attn_hw_reg usem_prty1_bb_a0 = {
	1, 6, usem_prty1_bb_a0_attn_idx, 0x1900200, 0x190020c, 0x1900208,
	0x1900204
};

static struct attn_hw_reg *usem_prty_bb_a0_regs[2] = {
	&usem_prty0_bb_a0, &usem_prty1_bb_a0,
};

static const u16 usem_prty0_bb_b0_attn_idx[3] = {
	0, 1, 2,
};

static struct attn_hw_reg usem_prty0_bb_b0 = {
	0, 3, usem_prty0_bb_b0_attn_idx, 0x19000c8, 0x19000d4, 0x19000d0,
	0x19000cc
};

static const u16 usem_prty1_bb_b0_attn_idx[6] = {
	3, 4, 5, 6, 7, 8,
};

static struct attn_hw_reg usem_prty1_bb_b0 = {
	1, 6, usem_prty1_bb_b0_attn_idx, 0x1900200, 0x190020c, 0x1900208,
	0x1900204
};

static struct attn_hw_reg *usem_prty_bb_b0_regs[2] = {
	&usem_prty0_bb_b0, &usem_prty1_bb_b0,
};

static const u16 usem_prty0_k2_attn_idx[3] = {
	0, 1, 2,
};

static struct attn_hw_reg usem_prty0_k2 = {
	0, 3, usem_prty0_k2_attn_idx, 0x19000c8, 0x19000d4, 0x19000d0,
	0x19000cc
};

static const u16 usem_prty1_k2_attn_idx[6] = {
	3, 4, 5, 6, 7, 8,
};

static struct attn_hw_reg usem_prty1_k2 = {
	1, 6, usem_prty1_k2_attn_idx, 0x1900200, 0x190020c, 0x1900208,
	0x1900204
};

static const u16 usem_fast_memory_prty1_k2_attn_idx[7] = {
	9, 10, 11, 12, 13, 14, 15,
};

static struct attn_hw_reg usem_fast_memory_prty1_k2 = {
	2, 7, usem_fast_memory_prty1_k2_attn_idx, 0x1940200, 0x194020c,
	0x1940208,
	0x1940204
};

static struct attn_hw_reg *usem_prty_k2_regs[3] = {
	&usem_prty0_k2, &usem_prty1_k2, &usem_fast_memory_prty1_k2,
};

#ifdef ATTN_DESC
static const char *xsem_int_attn_desc[46] = {
	"xsem_address_error",
	"xsem_fic_last_error",
	"xsem_fic_length_error",
	"xsem_fic_fifo_error",
	"xsem_pas_buf_fifo_error",
	"xsem_sync_fin_pop_error",
	"xsem_sync_dra_wr_push_error",
	"xsem_sync_dra_wr_pop_error",
	"xsem_sync_dra_rd_push_error",
	"xsem_sync_dra_rd_pop_error",
	"xsem_sync_fin_push_error",
	"xsem_sem_fast_address_error",
	"xsem_cam_lsb_inp_fifo",
	"xsem_cam_msb_inp_fifo",
	"xsem_cam_out_fifo",
	"xsem_fin_fifo",
	"xsem_thread_fifo_error",
	"xsem_thread_overrun",
	"xsem_sync_ext_store_push_error",
	"xsem_sync_ext_store_pop_error",
	"xsem_sync_ext_load_push_error",
	"xsem_sync_ext_load_pop_error",
	"xsem_sync_ram_rd_push_error",
	"xsem_sync_ram_rd_pop_error",
	"xsem_sync_ram_wr_pop_error",
	"xsem_sync_ram_wr_push_error",
	"xsem_sync_dbg_push_error",
	"xsem_sync_dbg_pop_error",
	"xsem_dbg_fifo_error",
	"xsem_cam_msb2_inp_fifo",
	"xsem_vfc_interrupt",
	"xsem_vfc_out_fifo_error",
	"xsem_storm_stack_uf_attn",
	"xsem_storm_stack_of_attn",
	"xsem_storm_runtime_error",
	"xsem_ext_load_pend_wr_error",
	"xsem_thread_rls_orun_error",
	"xsem_thread_rls_aloc_error",
	"xsem_thread_rls_vld_error",
	"xsem_ext_thread_oor_error",
	"xsem_ord_id_fifo_error",
	"xsem_invld_foc_error",
	"xsem_ext_ld_len_error",
	"xsem_thrd_ord_fifo_error",
	"xsem_invld_thrd_ord_error",
	"xsem_fast_memory_address_error",
};
#else
#define xsem_int_attn_desc OSAL_NULL
#endif

static const u16 xsem_int0_bb_a0_attn_idx[32] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
};

static struct attn_hw_reg xsem_int0_bb_a0 = {
	0, 32, xsem_int0_bb_a0_attn_idx, 0x1400040, 0x140004c, 0x1400048,
	0x1400044
};

static const u16 xsem_int1_bb_a0_attn_idx[13] = {
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44,
};

static struct attn_hw_reg xsem_int1_bb_a0 = {
	1, 13, xsem_int1_bb_a0_attn_idx, 0x1400050, 0x140005c, 0x1400058,
	0x1400054
};

static const u16 xsem_fast_memory_int0_bb_a0_attn_idx[1] = {
	45,
};

static struct attn_hw_reg xsem_fast_memory_int0_bb_a0 = {
	2, 1, xsem_fast_memory_int0_bb_a0_attn_idx, 0x1440040, 0x144004c,
	0x1440048, 0x1440044
};

static struct attn_hw_reg *xsem_int_bb_a0_regs[3] = {
	&xsem_int0_bb_a0, &xsem_int1_bb_a0, &xsem_fast_memory_int0_bb_a0,
};

static const u16 xsem_int0_bb_b0_attn_idx[32] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
};

static struct attn_hw_reg xsem_int0_bb_b0 = {
	0, 32, xsem_int0_bb_b0_attn_idx, 0x1400040, 0x140004c, 0x1400048,
	0x1400044
};

static const u16 xsem_int1_bb_b0_attn_idx[13] = {
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44,
};

static struct attn_hw_reg xsem_int1_bb_b0 = {
	1, 13, xsem_int1_bb_b0_attn_idx, 0x1400050, 0x140005c, 0x1400058,
	0x1400054
};

static const u16 xsem_fast_memory_int0_bb_b0_attn_idx[1] = {
	45,
};

static struct attn_hw_reg xsem_fast_memory_int0_bb_b0 = {
	2, 1, xsem_fast_memory_int0_bb_b0_attn_idx, 0x1440040, 0x144004c,
	0x1440048, 0x1440044
};

static struct attn_hw_reg *xsem_int_bb_b0_regs[3] = {
	&xsem_int0_bb_b0, &xsem_int1_bb_b0, &xsem_fast_memory_int0_bb_b0,
};

static const u16 xsem_int0_k2_attn_idx[32] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
};

static struct attn_hw_reg xsem_int0_k2 = {
	0, 32, xsem_int0_k2_attn_idx, 0x1400040, 0x140004c, 0x1400048,
	0x1400044
};

static const u16 xsem_int1_k2_attn_idx[13] = {
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44,
};

static struct attn_hw_reg xsem_int1_k2 = {
	1, 13, xsem_int1_k2_attn_idx, 0x1400050, 0x140005c, 0x1400058,
	0x1400054
};

static const u16 xsem_fast_memory_int0_k2_attn_idx[1] = {
	45,
};

static struct attn_hw_reg xsem_fast_memory_int0_k2 = {
	2, 1, xsem_fast_memory_int0_k2_attn_idx, 0x1440040, 0x144004c,
	0x1440048,
	0x1440044
};

static struct attn_hw_reg *xsem_int_k2_regs[3] = {
	&xsem_int0_k2, &xsem_int1_k2, &xsem_fast_memory_int0_k2,
};

#ifdef ATTN_DESC
static const char *xsem_prty_attn_desc[24] = {
	"xsem_vfc_rbc_parity_error",
	"xsem_storm_rf_parity_error",
	"xsem_reg_gen_parity_error",
	"xsem_mem006_i_ecc_0_rf_int",
	"xsem_mem006_i_ecc_1_rf_int",
	"xsem_mem005_i_mem_prty",
	"xsem_mem002_i_mem_prty",
	"xsem_mem004_i_mem_prty",
	"xsem_mem003_i_mem_prty",
	"xsem_mem001_i_mem_prty",
	"xsem_fast_memory_mem024_i_mem_prty",
	"xsem_fast_memory_mem023_i_mem_prty",
	"xsem_fast_memory_mem022_i_mem_prty",
	"xsem_fast_memory_mem021_i_mem_prty",
	"xsem_fast_memory_mem020_i_mem_prty",
	"xsem_fast_memory_mem019_i_mem_prty",
	"xsem_fast_memory_mem018_i_mem_prty",
	"xsem_fast_memory_vfc_config_mem005_i_ecc_rf_int",
	"xsem_fast_memory_vfc_config_mem002_i_ecc_rf_int",
	"xsem_fast_memory_vfc_config_mem006_i_mem_prty",
	"xsem_fast_memory_vfc_config_mem001_i_mem_prty",
	"xsem_fast_memory_vfc_config_mem004_i_mem_prty",
	"xsem_fast_memory_vfc_config_mem003_i_mem_prty",
	"xsem_fast_memory_vfc_config_mem007_i_mem_prty",
};
#else
#define xsem_prty_attn_desc OSAL_NULL
#endif

static const u16 xsem_prty0_bb_a0_attn_idx[3] = {
	0, 1, 2,
};

static struct attn_hw_reg xsem_prty0_bb_a0 = {
	0, 3, xsem_prty0_bb_a0_attn_idx, 0x14000c8, 0x14000d4, 0x14000d0,
	0x14000cc
};

static const u16 xsem_prty1_bb_a0_attn_idx[7] = {
	3, 4, 5, 6, 7, 8, 9,
};

static struct attn_hw_reg xsem_prty1_bb_a0 = {
	1, 7, xsem_prty1_bb_a0_attn_idx, 0x1400200, 0x140020c, 0x1400208,
	0x1400204
};

static struct attn_hw_reg *xsem_prty_bb_a0_regs[2] = {
	&xsem_prty0_bb_a0, &xsem_prty1_bb_a0,
};

static const u16 xsem_prty0_bb_b0_attn_idx[3] = {
	0, 1, 2,
};

static struct attn_hw_reg xsem_prty0_bb_b0 = {
	0, 3, xsem_prty0_bb_b0_attn_idx, 0x14000c8, 0x14000d4, 0x14000d0,
	0x14000cc
};

static const u16 xsem_prty1_bb_b0_attn_idx[7] = {
	3, 4, 5, 6, 7, 8, 9,
};

static struct attn_hw_reg xsem_prty1_bb_b0 = {
	1, 7, xsem_prty1_bb_b0_attn_idx, 0x1400200, 0x140020c, 0x1400208,
	0x1400204
};

static struct attn_hw_reg *xsem_prty_bb_b0_regs[2] = {
	&xsem_prty0_bb_b0, &xsem_prty1_bb_b0,
};

static const u16 xsem_prty0_k2_attn_idx[3] = {
	0, 1, 2,
};

static struct attn_hw_reg xsem_prty0_k2 = {
	0, 3, xsem_prty0_k2_attn_idx, 0x14000c8, 0x14000d4, 0x14000d0,
	0x14000cc
};

static const u16 xsem_prty1_k2_attn_idx[7] = {
	3, 4, 5, 6, 7, 8, 9,
};

static struct attn_hw_reg xsem_prty1_k2 = {
	1, 7, xsem_prty1_k2_attn_idx, 0x1400200, 0x140020c, 0x1400208,
	0x1400204
};

static const u16 xsem_fast_memory_prty1_k2_attn_idx[7] = {
	10, 11, 12, 13, 14, 15, 16,
};

static struct attn_hw_reg xsem_fast_memory_prty1_k2 = {
	2, 7, xsem_fast_memory_prty1_k2_attn_idx, 0x1440200, 0x144020c,
	0x1440208,
	0x1440204
};

static struct attn_hw_reg *xsem_prty_k2_regs[3] = {
	&xsem_prty0_k2, &xsem_prty1_k2, &xsem_fast_memory_prty1_k2,
};

#ifdef ATTN_DESC
static const char *ysem_int_attn_desc[46] = {
	"ysem_address_error",
	"ysem_fic_last_error",
	"ysem_fic_length_error",
	"ysem_fic_fifo_error",
	"ysem_pas_buf_fifo_error",
	"ysem_sync_fin_pop_error",
	"ysem_sync_dra_wr_push_error",
	"ysem_sync_dra_wr_pop_error",
	"ysem_sync_dra_rd_push_error",
	"ysem_sync_dra_rd_pop_error",
	"ysem_sync_fin_push_error",
	"ysem_sem_fast_address_error",
	"ysem_cam_lsb_inp_fifo",
	"ysem_cam_msb_inp_fifo",
	"ysem_cam_out_fifo",
	"ysem_fin_fifo",
	"ysem_thread_fifo_error",
	"ysem_thread_overrun",
	"ysem_sync_ext_store_push_error",
	"ysem_sync_ext_store_pop_error",
	"ysem_sync_ext_load_push_error",
	"ysem_sync_ext_load_pop_error",
	"ysem_sync_ram_rd_push_error",
	"ysem_sync_ram_rd_pop_error",
	"ysem_sync_ram_wr_pop_error",
	"ysem_sync_ram_wr_push_error",
	"ysem_sync_dbg_push_error",
	"ysem_sync_dbg_pop_error",
	"ysem_dbg_fifo_error",
	"ysem_cam_msb2_inp_fifo",
	"ysem_vfc_interrupt",
	"ysem_vfc_out_fifo_error",
	"ysem_storm_stack_uf_attn",
	"ysem_storm_stack_of_attn",
	"ysem_storm_runtime_error",
	"ysem_ext_load_pend_wr_error",
	"ysem_thread_rls_orun_error",
	"ysem_thread_rls_aloc_error",
	"ysem_thread_rls_vld_error",
	"ysem_ext_thread_oor_error",
	"ysem_ord_id_fifo_error",
	"ysem_invld_foc_error",
	"ysem_ext_ld_len_error",
	"ysem_thrd_ord_fifo_error",
	"ysem_invld_thrd_ord_error",
	"ysem_fast_memory_address_error",
};
#else
#define ysem_int_attn_desc OSAL_NULL
#endif

static const u16 ysem_int0_bb_a0_attn_idx[32] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
};

static struct attn_hw_reg ysem_int0_bb_a0 = {
	0, 32, ysem_int0_bb_a0_attn_idx, 0x1500040, 0x150004c, 0x1500048,
	0x1500044
};

static const u16 ysem_int1_bb_a0_attn_idx[13] = {
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44,
};

static struct attn_hw_reg ysem_int1_bb_a0 = {
	1, 13, ysem_int1_bb_a0_attn_idx, 0x1500050, 0x150005c, 0x1500058,
	0x1500054
};

static const u16 ysem_fast_memory_int0_bb_a0_attn_idx[1] = {
	45,
};

static struct attn_hw_reg ysem_fast_memory_int0_bb_a0 = {
	2, 1, ysem_fast_memory_int0_bb_a0_attn_idx, 0x1540040, 0x154004c,
	0x1540048, 0x1540044
};

static struct attn_hw_reg *ysem_int_bb_a0_regs[3] = {
	&ysem_int0_bb_a0, &ysem_int1_bb_a0, &ysem_fast_memory_int0_bb_a0,
};

static const u16 ysem_int0_bb_b0_attn_idx[32] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
};

static struct attn_hw_reg ysem_int0_bb_b0 = {
	0, 32, ysem_int0_bb_b0_attn_idx, 0x1500040, 0x150004c, 0x1500048,
	0x1500044
};

static const u16 ysem_int1_bb_b0_attn_idx[13] = {
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44,
};

static struct attn_hw_reg ysem_int1_bb_b0 = {
	1, 13, ysem_int1_bb_b0_attn_idx, 0x1500050, 0x150005c, 0x1500058,
	0x1500054
};

static const u16 ysem_fast_memory_int0_bb_b0_attn_idx[1] = {
	45,
};

static struct attn_hw_reg ysem_fast_memory_int0_bb_b0 = {
	2, 1, ysem_fast_memory_int0_bb_b0_attn_idx, 0x1540040, 0x154004c,
	0x1540048, 0x1540044
};

static struct attn_hw_reg *ysem_int_bb_b0_regs[3] = {
	&ysem_int0_bb_b0, &ysem_int1_bb_b0, &ysem_fast_memory_int0_bb_b0,
};

static const u16 ysem_int0_k2_attn_idx[32] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
};

static struct attn_hw_reg ysem_int0_k2 = {
	0, 32, ysem_int0_k2_attn_idx, 0x1500040, 0x150004c, 0x1500048,
	0x1500044
};

static const u16 ysem_int1_k2_attn_idx[13] = {
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44,
};

static struct attn_hw_reg ysem_int1_k2 = {
	1, 13, ysem_int1_k2_attn_idx, 0x1500050, 0x150005c, 0x1500058,
	0x1500054
};

static const u16 ysem_fast_memory_int0_k2_attn_idx[1] = {
	45,
};

static struct attn_hw_reg ysem_fast_memory_int0_k2 = {
	2, 1, ysem_fast_memory_int0_k2_attn_idx, 0x1540040, 0x154004c,
	0x1540048,
	0x1540044
};

static struct attn_hw_reg *ysem_int_k2_regs[3] = {
	&ysem_int0_k2, &ysem_int1_k2, &ysem_fast_memory_int0_k2,
};

#ifdef ATTN_DESC
static const char *ysem_prty_attn_desc[24] = {
	"ysem_vfc_rbc_parity_error",
	"ysem_storm_rf_parity_error",
	"ysem_reg_gen_parity_error",
	"ysem_mem006_i_ecc_0_rf_int",
	"ysem_mem006_i_ecc_1_rf_int",
	"ysem_mem005_i_mem_prty",
	"ysem_mem002_i_mem_prty",
	"ysem_mem004_i_mem_prty",
	"ysem_mem003_i_mem_prty",
	"ysem_mem001_i_mem_prty",
	"ysem_fast_memory_mem024_i_mem_prty",
	"ysem_fast_memory_mem023_i_mem_prty",
	"ysem_fast_memory_mem022_i_mem_prty",
	"ysem_fast_memory_mem021_i_mem_prty",
	"ysem_fast_memory_mem020_i_mem_prty",
	"ysem_fast_memory_mem019_i_mem_prty",
	"ysem_fast_memory_mem018_i_mem_prty",
	"ysem_fast_memory_vfc_config_mem005_i_ecc_rf_int",
	"ysem_fast_memory_vfc_config_mem002_i_ecc_rf_int",
	"ysem_fast_memory_vfc_config_mem006_i_mem_prty",
	"ysem_fast_memory_vfc_config_mem001_i_mem_prty",
	"ysem_fast_memory_vfc_config_mem004_i_mem_prty",
	"ysem_fast_memory_vfc_config_mem003_i_mem_prty",
	"ysem_fast_memory_vfc_config_mem007_i_mem_prty",
};
#else
#define ysem_prty_attn_desc OSAL_NULL
#endif

static const u16 ysem_prty0_bb_a0_attn_idx[3] = {
	0, 1, 2,
};

static struct attn_hw_reg ysem_prty0_bb_a0 = {
	0, 3, ysem_prty0_bb_a0_attn_idx, 0x15000c8, 0x15000d4, 0x15000d0,
	0x15000cc
};

static const u16 ysem_prty1_bb_a0_attn_idx[7] = {
	3, 4, 5, 6, 7, 8, 9,
};

static struct attn_hw_reg ysem_prty1_bb_a0 = {
	1, 7, ysem_prty1_bb_a0_attn_idx, 0x1500200, 0x150020c, 0x1500208,
	0x1500204
};

static struct attn_hw_reg *ysem_prty_bb_a0_regs[2] = {
	&ysem_prty0_bb_a0, &ysem_prty1_bb_a0,
};

static const u16 ysem_prty0_bb_b0_attn_idx[3] = {
	0, 1, 2,
};

static struct attn_hw_reg ysem_prty0_bb_b0 = {
	0, 3, ysem_prty0_bb_b0_attn_idx, 0x15000c8, 0x15000d4, 0x15000d0,
	0x15000cc
};

static const u16 ysem_prty1_bb_b0_attn_idx[7] = {
	3, 4, 5, 6, 7, 8, 9,
};

static struct attn_hw_reg ysem_prty1_bb_b0 = {
	1, 7, ysem_prty1_bb_b0_attn_idx, 0x1500200, 0x150020c, 0x1500208,
	0x1500204
};

static struct attn_hw_reg *ysem_prty_bb_b0_regs[2] = {
	&ysem_prty0_bb_b0, &ysem_prty1_bb_b0,
};

static const u16 ysem_prty0_k2_attn_idx[3] = {
	0, 1, 2,
};

static struct attn_hw_reg ysem_prty0_k2 = {
	0, 3, ysem_prty0_k2_attn_idx, 0x15000c8, 0x15000d4, 0x15000d0,
	0x15000cc
};

static const u16 ysem_prty1_k2_attn_idx[7] = {
	3, 4, 5, 6, 7, 8, 9,
};

static struct attn_hw_reg ysem_prty1_k2 = {
	1, 7, ysem_prty1_k2_attn_idx, 0x1500200, 0x150020c, 0x1500208,
	0x1500204
};

static const u16 ysem_fast_memory_prty1_k2_attn_idx[7] = {
	10, 11, 12, 13, 14, 15, 16,
};

static struct attn_hw_reg ysem_fast_memory_prty1_k2 = {
	2, 7, ysem_fast_memory_prty1_k2_attn_idx, 0x1540200, 0x154020c,
	0x1540208,
	0x1540204
};

static struct attn_hw_reg *ysem_prty_k2_regs[3] = {
	&ysem_prty0_k2, &ysem_prty1_k2, &ysem_fast_memory_prty1_k2,
};

#ifdef ATTN_DESC
static const char *psem_int_attn_desc[46] = {
	"psem_address_error",
	"psem_fic_last_error",
	"psem_fic_length_error",
	"psem_fic_fifo_error",
	"psem_pas_buf_fifo_error",
	"psem_sync_fin_pop_error",
	"psem_sync_dra_wr_push_error",
	"psem_sync_dra_wr_pop_error",
	"psem_sync_dra_rd_push_error",
	"psem_sync_dra_rd_pop_error",
	"psem_sync_fin_push_error",
	"psem_sem_fast_address_error",
	"psem_cam_lsb_inp_fifo",
	"psem_cam_msb_inp_fifo",
	"psem_cam_out_fifo",
	"psem_fin_fifo",
	"psem_thread_fifo_error",
	"psem_thread_overrun",
	"psem_sync_ext_store_push_error",
	"psem_sync_ext_store_pop_error",
	"psem_sync_ext_load_push_error",
	"psem_sync_ext_load_pop_error",
	"psem_sync_ram_rd_push_error",
	"psem_sync_ram_rd_pop_error",
	"psem_sync_ram_wr_pop_error",
	"psem_sync_ram_wr_push_error",
	"psem_sync_dbg_push_error",
	"psem_sync_dbg_pop_error",
	"psem_dbg_fifo_error",
	"psem_cam_msb2_inp_fifo",
	"psem_vfc_interrupt",
	"psem_vfc_out_fifo_error",
	"psem_storm_stack_uf_attn",
	"psem_storm_stack_of_attn",
	"psem_storm_runtime_error",
	"psem_ext_load_pend_wr_error",
	"psem_thread_rls_orun_error",
	"psem_thread_rls_aloc_error",
	"psem_thread_rls_vld_error",
	"psem_ext_thread_oor_error",
	"psem_ord_id_fifo_error",
	"psem_invld_foc_error",
	"psem_ext_ld_len_error",
	"psem_thrd_ord_fifo_error",
	"psem_invld_thrd_ord_error",
	"psem_fast_memory_address_error",
};
#else
#define psem_int_attn_desc OSAL_NULL
#endif

static const u16 psem_int0_bb_a0_attn_idx[32] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
};

static struct attn_hw_reg psem_int0_bb_a0 = {
	0, 32, psem_int0_bb_a0_attn_idx, 0x1600040, 0x160004c, 0x1600048,
	0x1600044
};

static const u16 psem_int1_bb_a0_attn_idx[13] = {
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44,
};

static struct attn_hw_reg psem_int1_bb_a0 = {
	1, 13, psem_int1_bb_a0_attn_idx, 0x1600050, 0x160005c, 0x1600058,
	0x1600054
};

static const u16 psem_fast_memory_int0_bb_a0_attn_idx[1] = {
	45,
};

static struct attn_hw_reg psem_fast_memory_int0_bb_a0 = {
	2, 1, psem_fast_memory_int0_bb_a0_attn_idx, 0x1640040, 0x164004c,
	0x1640048, 0x1640044
};

static struct attn_hw_reg *psem_int_bb_a0_regs[3] = {
	&psem_int0_bb_a0, &psem_int1_bb_a0, &psem_fast_memory_int0_bb_a0,
};

static const u16 psem_int0_bb_b0_attn_idx[32] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
};

static struct attn_hw_reg psem_int0_bb_b0 = {
	0, 32, psem_int0_bb_b0_attn_idx, 0x1600040, 0x160004c, 0x1600048,
	0x1600044
};

static const u16 psem_int1_bb_b0_attn_idx[13] = {
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44,
};

static struct attn_hw_reg psem_int1_bb_b0 = {
	1, 13, psem_int1_bb_b0_attn_idx, 0x1600050, 0x160005c, 0x1600058,
	0x1600054
};

static const u16 psem_fast_memory_int0_bb_b0_attn_idx[1] = {
	45,
};

static struct attn_hw_reg psem_fast_memory_int0_bb_b0 = {
	2, 1, psem_fast_memory_int0_bb_b0_attn_idx, 0x1640040, 0x164004c,
	0x1640048, 0x1640044
};

static struct attn_hw_reg *psem_int_bb_b0_regs[3] = {
	&psem_int0_bb_b0, &psem_int1_bb_b0, &psem_fast_memory_int0_bb_b0,
};

static const u16 psem_int0_k2_attn_idx[32] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
};

static struct attn_hw_reg psem_int0_k2 = {
	0, 32, psem_int0_k2_attn_idx, 0x1600040, 0x160004c, 0x1600048,
	0x1600044
};

static const u16 psem_int1_k2_attn_idx[13] = {
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44,
};

static struct attn_hw_reg psem_int1_k2 = {
	1, 13, psem_int1_k2_attn_idx, 0x1600050, 0x160005c, 0x1600058,
	0x1600054
};

static const u16 psem_fast_memory_int0_k2_attn_idx[1] = {
	45,
};

static struct attn_hw_reg psem_fast_memory_int0_k2 = {
	2, 1, psem_fast_memory_int0_k2_attn_idx, 0x1640040, 0x164004c,
	0x1640048,
	0x1640044
};

static struct attn_hw_reg *psem_int_k2_regs[3] = {
	&psem_int0_k2, &psem_int1_k2, &psem_fast_memory_int0_k2,
};

#ifdef ATTN_DESC
static const char *psem_prty_attn_desc[23] = {
	"psem_vfc_rbc_parity_error",
	"psem_storm_rf_parity_error",
	"psem_reg_gen_parity_error",
	"psem_mem005_i_ecc_0_rf_int",
	"psem_mem005_i_ecc_1_rf_int",
	"psem_mem004_i_mem_prty",
	"psem_mem002_i_mem_prty",
	"psem_mem003_i_mem_prty",
	"psem_mem001_i_mem_prty",
	"psem_fast_memory_mem024_i_mem_prty",
	"psem_fast_memory_mem023_i_mem_prty",
	"psem_fast_memory_mem022_i_mem_prty",
	"psem_fast_memory_mem021_i_mem_prty",
	"psem_fast_memory_mem020_i_mem_prty",
	"psem_fast_memory_mem019_i_mem_prty",
	"psem_fast_memory_mem018_i_mem_prty",
	"psem_fast_memory_vfc_config_mem005_i_ecc_rf_int",
	"psem_fast_memory_vfc_config_mem002_i_ecc_rf_int",
	"psem_fast_memory_vfc_config_mem006_i_mem_prty",
	"psem_fast_memory_vfc_config_mem001_i_mem_prty",
	"psem_fast_memory_vfc_config_mem004_i_mem_prty",
	"psem_fast_memory_vfc_config_mem003_i_mem_prty",
	"psem_fast_memory_vfc_config_mem007_i_mem_prty",
};
#else
#define psem_prty_attn_desc OSAL_NULL
#endif

static const u16 psem_prty0_bb_a0_attn_idx[3] = {
	0, 1, 2,
};

static struct attn_hw_reg psem_prty0_bb_a0 = {
	0, 3, psem_prty0_bb_a0_attn_idx, 0x16000c8, 0x16000d4, 0x16000d0,
	0x16000cc
};

static const u16 psem_prty1_bb_a0_attn_idx[6] = {
	3, 4, 5, 6, 7, 8,
};

static struct attn_hw_reg psem_prty1_bb_a0 = {
	1, 6, psem_prty1_bb_a0_attn_idx, 0x1600200, 0x160020c, 0x1600208,
	0x1600204
};

static const u16 psem_fast_memory_vfc_config_prty1_bb_a0_attn_idx[6] = {
	16, 17, 19, 20, 21, 22,
};

static struct attn_hw_reg psem_fast_memory_vfc_config_prty1_bb_a0 = {
	2, 6, psem_fast_memory_vfc_config_prty1_bb_a0_attn_idx, 0x164a200,
	0x164a20c, 0x164a208, 0x164a204
};

static struct attn_hw_reg *psem_prty_bb_a0_regs[3] = {
	&psem_prty0_bb_a0, &psem_prty1_bb_a0,
	&psem_fast_memory_vfc_config_prty1_bb_a0,
};

static const u16 psem_prty0_bb_b0_attn_idx[3] = {
	0, 1, 2,
};

static struct attn_hw_reg psem_prty0_bb_b0 = {
	0, 3, psem_prty0_bb_b0_attn_idx, 0x16000c8, 0x16000d4, 0x16000d0,
	0x16000cc
};

static const u16 psem_prty1_bb_b0_attn_idx[6] = {
	3, 4, 5, 6, 7, 8,
};

static struct attn_hw_reg psem_prty1_bb_b0 = {
	1, 6, psem_prty1_bb_b0_attn_idx, 0x1600200, 0x160020c, 0x1600208,
	0x1600204
};

static const u16 psem_fast_memory_vfc_config_prty1_bb_b0_attn_idx[6] = {
	16, 17, 19, 20, 21, 22,
};

static struct attn_hw_reg psem_fast_memory_vfc_config_prty1_bb_b0 = {
	2, 6, psem_fast_memory_vfc_config_prty1_bb_b0_attn_idx, 0x164a200,
	0x164a20c, 0x164a208, 0x164a204
};

static struct attn_hw_reg *psem_prty_bb_b0_regs[3] = {
	&psem_prty0_bb_b0, &psem_prty1_bb_b0,
	&psem_fast_memory_vfc_config_prty1_bb_b0,
};

static const u16 psem_prty0_k2_attn_idx[3] = {
	0, 1, 2,
};

static struct attn_hw_reg psem_prty0_k2 = {
	0, 3, psem_prty0_k2_attn_idx, 0x16000c8, 0x16000d4, 0x16000d0,
	0x16000cc
};

static const u16 psem_prty1_k2_attn_idx[6] = {
	3, 4, 5, 6, 7, 8,
};

static struct attn_hw_reg psem_prty1_k2 = {
	1, 6, psem_prty1_k2_attn_idx, 0x1600200, 0x160020c, 0x1600208,
	0x1600204
};

static const u16 psem_fast_memory_prty1_k2_attn_idx[7] = {
	9, 10, 11, 12, 13, 14, 15,
};

static struct attn_hw_reg psem_fast_memory_prty1_k2 = {
	2, 7, psem_fast_memory_prty1_k2_attn_idx, 0x1640200, 0x164020c,
	0x1640208,
	0x1640204
};

static const u16 psem_fast_memory_vfc_config_prty1_k2_attn_idx[6] = {
	16, 17, 18, 19, 20, 21,
};

static struct attn_hw_reg psem_fast_memory_vfc_config_prty1_k2 = {
	3, 6, psem_fast_memory_vfc_config_prty1_k2_attn_idx, 0x164a200,
	0x164a20c,
	0x164a208, 0x164a204
};

static struct attn_hw_reg *psem_prty_k2_regs[4] = {
	&psem_prty0_k2, &psem_prty1_k2, &psem_fast_memory_prty1_k2,
	&psem_fast_memory_vfc_config_prty1_k2,
};

#ifdef ATTN_DESC
static const char *rss_int_attn_desc[12] = {
	"rss_address_error",
	"rss_msg_inp_cnt_error",
	"rss_msg_out_cnt_error",
	"rss_inp_state_error",
	"rss_out_state_error",
	"rss_main_state_error",
	"rss_calc_state_error",
	"rss_inp_fifo_error",
	"rss_cmd_fifo_error",
	"rss_msg_fifo_error",
	"rss_rsp_fifo_error",
	"rss_hdr_fifo_error",
};
#else
#define rss_int_attn_desc OSAL_NULL
#endif

static const u16 rss_int0_bb_a0_attn_idx[12] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
};

static struct attn_hw_reg rss_int0_bb_a0 = {
	0, 12, rss_int0_bb_a0_attn_idx, 0x238980, 0x23898c, 0x238988, 0x238984
};

static struct attn_hw_reg *rss_int_bb_a0_regs[1] = {
	&rss_int0_bb_a0,
};

static const u16 rss_int0_bb_b0_attn_idx[12] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
};

static struct attn_hw_reg rss_int0_bb_b0 = {
	0, 12, rss_int0_bb_b0_attn_idx, 0x238980, 0x23898c, 0x238988, 0x238984
};

static struct attn_hw_reg *rss_int_bb_b0_regs[1] = {
	&rss_int0_bb_b0,
};

static const u16 rss_int0_k2_attn_idx[12] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
};

static struct attn_hw_reg rss_int0_k2 = {
	0, 12, rss_int0_k2_attn_idx, 0x238980, 0x23898c, 0x238988, 0x238984
};

static struct attn_hw_reg *rss_int_k2_regs[1] = {
	&rss_int0_k2,
};

#ifdef ATTN_DESC
static const char *rss_prty_attn_desc[4] = {
	"rss_mem002_i_ecc_rf_int",
	"rss_mem001_i_ecc_rf_int",
	"rss_mem003_i_mem_prty",
	"rss_mem004_i_mem_prty",
};
#else
#define rss_prty_attn_desc OSAL_NULL
#endif

static const u16 rss_prty1_bb_a0_attn_idx[4] = {
	0, 1, 2, 3,
};

static struct attn_hw_reg rss_prty1_bb_a0 = {
	0, 4, rss_prty1_bb_a0_attn_idx, 0x238a00, 0x238a0c, 0x238a08, 0x238a04
};

static struct attn_hw_reg *rss_prty_bb_a0_regs[1] = {
	&rss_prty1_bb_a0,
};

static const u16 rss_prty1_bb_b0_attn_idx[4] = {
	0, 1, 2, 3,
};

static struct attn_hw_reg rss_prty1_bb_b0 = {
	0, 4, rss_prty1_bb_b0_attn_idx, 0x238a00, 0x238a0c, 0x238a08, 0x238a04
};

static struct attn_hw_reg *rss_prty_bb_b0_regs[1] = {
	&rss_prty1_bb_b0,
};

static const u16 rss_prty1_k2_attn_idx[4] = {
	0, 1, 2, 3,
};

static struct attn_hw_reg rss_prty1_k2 = {
	0, 4, rss_prty1_k2_attn_idx, 0x238a00, 0x238a0c, 0x238a08, 0x238a04
};

static struct attn_hw_reg *rss_prty_k2_regs[1] = {
	&rss_prty1_k2,
};

#ifdef ATTN_DESC
static const char *tmld_int_attn_desc[6] = {
	"tmld_address_error",
	"tmld_ld_hdr_err",
	"tmld_ld_seg_msg_err",
	"tmld_ld_tid_mini_cache_err",
	"tmld_ld_cid_mini_cache_err",
	"tmld_ld_long_message",
};
#else
#define tmld_int_attn_desc OSAL_NULL
#endif

static const u16 tmld_int0_bb_a0_attn_idx[6] = {
	0, 1, 2, 3, 4, 5,
};

static struct attn_hw_reg tmld_int0_bb_a0 = {
	0, 6, tmld_int0_bb_a0_attn_idx, 0x4d0180, 0x4d018c, 0x4d0188, 0x4d0184
};

static struct attn_hw_reg *tmld_int_bb_a0_regs[1] = {
	&tmld_int0_bb_a0,
};

static const u16 tmld_int0_bb_b0_attn_idx[6] = {
	0, 1, 2, 3, 4, 5,
};

static struct attn_hw_reg tmld_int0_bb_b0 = {
	0, 6, tmld_int0_bb_b0_attn_idx, 0x4d0180, 0x4d018c, 0x4d0188, 0x4d0184
};

static struct attn_hw_reg *tmld_int_bb_b0_regs[1] = {
	&tmld_int0_bb_b0,
};

static const u16 tmld_int0_k2_attn_idx[6] = {
	0, 1, 2, 3, 4, 5,
};

static struct attn_hw_reg tmld_int0_k2 = {
	0, 6, tmld_int0_k2_attn_idx, 0x4d0180, 0x4d018c, 0x4d0188, 0x4d0184
};

static struct attn_hw_reg *tmld_int_k2_regs[1] = {
	&tmld_int0_k2,
};

#ifdef ATTN_DESC
static const char *tmld_prty_attn_desc[8] = {
	"tmld_mem006_i_ecc_rf_int",
	"tmld_mem002_i_ecc_rf_int",
	"tmld_mem003_i_mem_prty",
	"tmld_mem004_i_mem_prty",
	"tmld_mem007_i_mem_prty",
	"tmld_mem008_i_mem_prty",
	"tmld_mem005_i_mem_prty",
	"tmld_mem001_i_mem_prty",
};
#else
#define tmld_prty_attn_desc OSAL_NULL
#endif

static const u16 tmld_prty1_bb_a0_attn_idx[8] = {
	0, 1, 2, 3, 4, 5, 6, 7,
};

static struct attn_hw_reg tmld_prty1_bb_a0 = {
	0, 8, tmld_prty1_bb_a0_attn_idx, 0x4d0200, 0x4d020c, 0x4d0208, 0x4d0204
};

static struct attn_hw_reg *tmld_prty_bb_a0_regs[1] = {
	&tmld_prty1_bb_a0,
};

static const u16 tmld_prty1_bb_b0_attn_idx[8] = {
	0, 1, 2, 3, 4, 5, 6, 7,
};

static struct attn_hw_reg tmld_prty1_bb_b0 = {
	0, 8, tmld_prty1_bb_b0_attn_idx, 0x4d0200, 0x4d020c, 0x4d0208, 0x4d0204
};

static struct attn_hw_reg *tmld_prty_bb_b0_regs[1] = {
	&tmld_prty1_bb_b0,
};

static const u16 tmld_prty1_k2_attn_idx[8] = {
	0, 1, 2, 3, 4, 5, 6, 7,
};

static struct attn_hw_reg tmld_prty1_k2 = {
	0, 8, tmld_prty1_k2_attn_idx, 0x4d0200, 0x4d020c, 0x4d0208, 0x4d0204
};

static struct attn_hw_reg *tmld_prty_k2_regs[1] = {
	&tmld_prty1_k2,
};

#ifdef ATTN_DESC
static const char *muld_int_attn_desc[6] = {
	"muld_address_error",
	"muld_ld_hdr_err",
	"muld_ld_seg_msg_err",
	"muld_ld_tid_mini_cache_err",
	"muld_ld_cid_mini_cache_err",
	"muld_ld_long_message",
};
#else
#define muld_int_attn_desc OSAL_NULL
#endif

static const u16 muld_int0_bb_a0_attn_idx[6] = {
	0, 1, 2, 3, 4, 5,
};

static struct attn_hw_reg muld_int0_bb_a0 = {
	0, 6, muld_int0_bb_a0_attn_idx, 0x4e0180, 0x4e018c, 0x4e0188, 0x4e0184
};

static struct attn_hw_reg *muld_int_bb_a0_regs[1] = {
	&muld_int0_bb_a0,
};

static const u16 muld_int0_bb_b0_attn_idx[6] = {
	0, 1, 2, 3, 4, 5,
};

static struct attn_hw_reg muld_int0_bb_b0 = {
	0, 6, muld_int0_bb_b0_attn_idx, 0x4e0180, 0x4e018c, 0x4e0188, 0x4e0184
};

static struct attn_hw_reg *muld_int_bb_b0_regs[1] = {
	&muld_int0_bb_b0,
};

static const u16 muld_int0_k2_attn_idx[6] = {
	0, 1, 2, 3, 4, 5,
};

static struct attn_hw_reg muld_int0_k2 = {
	0, 6, muld_int0_k2_attn_idx, 0x4e0180, 0x4e018c, 0x4e0188, 0x4e0184
};

static struct attn_hw_reg *muld_int_k2_regs[1] = {
	&muld_int0_k2,
};

#ifdef ATTN_DESC
static const char *muld_prty_attn_desc[10] = {
	"muld_mem005_i_ecc_rf_int",
	"muld_mem001_i_ecc_rf_int",
	"muld_mem008_i_ecc_rf_int",
	"muld_mem007_i_ecc_rf_int",
	"muld_mem002_i_mem_prty",
	"muld_mem003_i_mem_prty",
	"muld_mem009_i_mem_prty",
	"muld_mem010_i_mem_prty",
	"muld_mem004_i_mem_prty",
	"muld_mem006_i_mem_prty",
};
#else
#define muld_prty_attn_desc OSAL_NULL
#endif

static const u16 muld_prty1_bb_a0_attn_idx[10] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
};

static struct attn_hw_reg muld_prty1_bb_a0 = {
	0, 10, muld_prty1_bb_a0_attn_idx, 0x4e0200, 0x4e020c, 0x4e0208,
	0x4e0204
};

static struct attn_hw_reg *muld_prty_bb_a0_regs[1] = {
	&muld_prty1_bb_a0,
};

static const u16 muld_prty1_bb_b0_attn_idx[10] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
};

static struct attn_hw_reg muld_prty1_bb_b0 = {
	0, 10, muld_prty1_bb_b0_attn_idx, 0x4e0200, 0x4e020c, 0x4e0208,
	0x4e0204
};

static struct attn_hw_reg *muld_prty_bb_b0_regs[1] = {
	&muld_prty1_bb_b0,
};

static const u16 muld_prty1_k2_attn_idx[10] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
};

static struct attn_hw_reg muld_prty1_k2 = {
	0, 10, muld_prty1_k2_attn_idx, 0x4e0200, 0x4e020c, 0x4e0208, 0x4e0204
};

static struct attn_hw_reg *muld_prty_k2_regs[1] = {
	&muld_prty1_k2,
};

#ifdef ATTN_DESC
static const char *yuld_int_attn_desc[6] = {
	"yuld_address_error",
	"yuld_ld_hdr_err",
	"yuld_ld_seg_msg_err",
	"yuld_ld_tid_mini_cache_err",
	"yuld_ld_cid_mini_cache_err",
	"yuld_ld_long_message",
};
#else
#define yuld_int_attn_desc OSAL_NULL
#endif

static const u16 yuld_int0_bb_a0_attn_idx[6] = {
	0, 1, 2, 3, 4, 5,
};

static struct attn_hw_reg yuld_int0_bb_a0 = {
	0, 6, yuld_int0_bb_a0_attn_idx, 0x4c8180, 0x4c818c, 0x4c8188, 0x4c8184
};

static struct attn_hw_reg *yuld_int_bb_a0_regs[1] = {
	&yuld_int0_bb_a0,
};

static const u16 yuld_int0_bb_b0_attn_idx[6] = {
	0, 1, 2, 3, 4, 5,
};

static struct attn_hw_reg yuld_int0_bb_b0 = {
	0, 6, yuld_int0_bb_b0_attn_idx, 0x4c8180, 0x4c818c, 0x4c8188, 0x4c8184
};

static struct attn_hw_reg *yuld_int_bb_b0_regs[1] = {
	&yuld_int0_bb_b0,
};

static const u16 yuld_int0_k2_attn_idx[6] = {
	0, 1, 2, 3, 4, 5,
};

static struct attn_hw_reg yuld_int0_k2 = {
	0, 6, yuld_int0_k2_attn_idx, 0x4c8180, 0x4c818c, 0x4c8188, 0x4c8184
};

static struct attn_hw_reg *yuld_int_k2_regs[1] = {
	&yuld_int0_k2,
};

#ifdef ATTN_DESC
static const char *yuld_prty_attn_desc[6] = {
	"yuld_mem001_i_mem_prty",
	"yuld_mem002_i_mem_prty",
	"yuld_mem005_i_mem_prty",
	"yuld_mem006_i_mem_prty",
	"yuld_mem004_i_mem_prty",
	"yuld_mem003_i_mem_prty",
};
#else
#define yuld_prty_attn_desc OSAL_NULL
#endif

static const u16 yuld_prty1_bb_a0_attn_idx[6] = {
	0, 1, 2, 3, 4, 5,
};

static struct attn_hw_reg yuld_prty1_bb_a0 = {
	0, 6, yuld_prty1_bb_a0_attn_idx, 0x4c8200, 0x4c820c, 0x4c8208, 0x4c8204
};

static struct attn_hw_reg *yuld_prty_bb_a0_regs[1] = {
	&yuld_prty1_bb_a0,
};

static const u16 yuld_prty1_bb_b0_attn_idx[6] = {
	0, 1, 2, 3, 4, 5,
};

static struct attn_hw_reg yuld_prty1_bb_b0 = {
	0, 6, yuld_prty1_bb_b0_attn_idx, 0x4c8200, 0x4c820c, 0x4c8208, 0x4c8204
};

static struct attn_hw_reg *yuld_prty_bb_b0_regs[1] = {
	&yuld_prty1_bb_b0,
};

static const u16 yuld_prty1_k2_attn_idx[6] = {
	0, 1, 2, 3, 4, 5,
};

static struct attn_hw_reg yuld_prty1_k2 = {
	0, 6, yuld_prty1_k2_attn_idx, 0x4c8200, 0x4c820c, 0x4c8208, 0x4c8204
};

static struct attn_hw_reg *yuld_prty_k2_regs[1] = {
	&yuld_prty1_k2,
};

#ifdef ATTN_DESC
static const char *xyld_int_attn_desc[6] = {
	"xyld_address_error",
	"xyld_ld_hdr_err",
	"xyld_ld_seg_msg_err",
	"xyld_ld_tid_mini_cache_err",
	"xyld_ld_cid_mini_cache_err",
	"xyld_ld_long_message",
};
#else
#define xyld_int_attn_desc OSAL_NULL
#endif

static const u16 xyld_int0_bb_a0_attn_idx[6] = {
	0, 1, 2, 3, 4, 5,
};

static struct attn_hw_reg xyld_int0_bb_a0 = {
	0, 6, xyld_int0_bb_a0_attn_idx, 0x4c0180, 0x4c018c, 0x4c0188, 0x4c0184
};

static struct attn_hw_reg *xyld_int_bb_a0_regs[1] = {
	&xyld_int0_bb_a0,
};

static const u16 xyld_int0_bb_b0_attn_idx[6] = {
	0, 1, 2, 3, 4, 5,
};

static struct attn_hw_reg xyld_int0_bb_b0 = {
	0, 6, xyld_int0_bb_b0_attn_idx, 0x4c0180, 0x4c018c, 0x4c0188, 0x4c0184
};

static struct attn_hw_reg *xyld_int_bb_b0_regs[1] = {
	&xyld_int0_bb_b0,
};

static const u16 xyld_int0_k2_attn_idx[6] = {
	0, 1, 2, 3, 4, 5,
};

static struct attn_hw_reg xyld_int0_k2 = {
	0, 6, xyld_int0_k2_attn_idx, 0x4c0180, 0x4c018c, 0x4c0188, 0x4c0184
};

static struct attn_hw_reg *xyld_int_k2_regs[1] = {
	&xyld_int0_k2,
};

#ifdef ATTN_DESC
static const char *xyld_prty_attn_desc[9] = {
	"xyld_mem004_i_ecc_rf_int",
	"xyld_mem006_i_ecc_rf_int",
	"xyld_mem001_i_mem_prty",
	"xyld_mem002_i_mem_prty",
	"xyld_mem008_i_mem_prty",
	"xyld_mem009_i_mem_prty",
	"xyld_mem003_i_mem_prty",
	"xyld_mem005_i_mem_prty",
	"xyld_mem007_i_mem_prty",
};
#else
#define xyld_prty_attn_desc OSAL_NULL
#endif

static const u16 xyld_prty1_bb_a0_attn_idx[9] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8,
};

static struct attn_hw_reg xyld_prty1_bb_a0 = {
	0, 9, xyld_prty1_bb_a0_attn_idx, 0x4c0200, 0x4c020c, 0x4c0208, 0x4c0204
};

static struct attn_hw_reg *xyld_prty_bb_a0_regs[1] = {
	&xyld_prty1_bb_a0,
};

static const u16 xyld_prty1_bb_b0_attn_idx[9] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8,
};

static struct attn_hw_reg xyld_prty1_bb_b0 = {
	0, 9, xyld_prty1_bb_b0_attn_idx, 0x4c0200, 0x4c020c, 0x4c0208, 0x4c0204
};

static struct attn_hw_reg *xyld_prty_bb_b0_regs[1] = {
	&xyld_prty1_bb_b0,
};

static const u16 xyld_prty1_k2_attn_idx[9] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8,
};

static struct attn_hw_reg xyld_prty1_k2 = {
	0, 9, xyld_prty1_k2_attn_idx, 0x4c0200, 0x4c020c, 0x4c0208, 0x4c0204
};

static struct attn_hw_reg *xyld_prty_k2_regs[1] = {
	&xyld_prty1_k2,
};

#ifdef ATTN_DESC
static const char *prm_int_attn_desc[11] = {
	"prm_address_error",
	"prm_ififo_error",
	"prm_immed_fifo_error",
	"prm_ofst_pend_error",
	"prm_pad_pend_error",
	"prm_pbinp_pend_error",
	"prm_tag_pend_error",
	"prm_mstorm_eop_err",
	"prm_ustorm_eop_err",
	"prm_mstorm_que_err",
	"prm_ustorm_que_err",
};
#else
#define prm_int_attn_desc OSAL_NULL
#endif

static const u16 prm_int0_bb_a0_attn_idx[11] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
};

static struct attn_hw_reg prm_int0_bb_a0 = {
	0, 11, prm_int0_bb_a0_attn_idx, 0x230040, 0x23004c, 0x230048, 0x230044
};

static struct attn_hw_reg *prm_int_bb_a0_regs[1] = {
	&prm_int0_bb_a0,
};

static const u16 prm_int0_bb_b0_attn_idx[11] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
};

static struct attn_hw_reg prm_int0_bb_b0 = {
	0, 11, prm_int0_bb_b0_attn_idx, 0x230040, 0x23004c, 0x230048, 0x230044
};

static struct attn_hw_reg *prm_int_bb_b0_regs[1] = {
	&prm_int0_bb_b0,
};

static const u16 prm_int0_k2_attn_idx[11] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
};

static struct attn_hw_reg prm_int0_k2 = {
	0, 11, prm_int0_k2_attn_idx, 0x230040, 0x23004c, 0x230048, 0x230044
};

static struct attn_hw_reg *prm_int_k2_regs[1] = {
	&prm_int0_k2,
};

#ifdef ATTN_DESC
static const char *prm_prty_attn_desc[30] = {
	"prm_datapath_registers",
	"prm_mem012_i_ecc_rf_int",
	"prm_mem013_i_ecc_rf_int",
	"prm_mem014_i_ecc_rf_int",
	"prm_mem020_i_ecc_rf_int",
	"prm_mem004_i_mem_prty",
	"prm_mem024_i_mem_prty",
	"prm_mem016_i_mem_prty",
	"prm_mem017_i_mem_prty",
	"prm_mem008_i_mem_prty",
	"prm_mem009_i_mem_prty",
	"prm_mem010_i_mem_prty",
	"prm_mem015_i_mem_prty",
	"prm_mem011_i_mem_prty",
	"prm_mem003_i_mem_prty",
	"prm_mem002_i_mem_prty",
	"prm_mem005_i_mem_prty",
	"prm_mem023_i_mem_prty",
	"prm_mem006_i_mem_prty",
	"prm_mem007_i_mem_prty",
	"prm_mem001_i_mem_prty",
	"prm_mem022_i_mem_prty",
	"prm_mem021_i_mem_prty",
	"prm_mem019_i_mem_prty",
	"prm_mem015_i_ecc_rf_int",
	"prm_mem021_i_ecc_rf_int",
	"prm_mem025_i_mem_prty",
	"prm_mem018_i_mem_prty",
	"prm_mem012_i_mem_prty",
	"prm_mem020_i_mem_prty",
};
#else
#define prm_prty_attn_desc OSAL_NULL
#endif

static const u16 prm_prty1_bb_a0_attn_idx[25] = {
	2, 3, 5, 6, 7, 8, 9, 10, 11, 13, 14, 15, 16, 17, 18, 19, 20, 21, 23, 24,
	25, 26, 27, 28, 29,
};

static struct attn_hw_reg prm_prty1_bb_a0 = {
	0, 25, prm_prty1_bb_a0_attn_idx, 0x230200, 0x23020c, 0x230208, 0x230204
};

static struct attn_hw_reg *prm_prty_bb_a0_regs[1] = {
	&prm_prty1_bb_a0,
};

static const u16 prm_prty0_bb_b0_attn_idx[1] = {
	0,
};

static struct attn_hw_reg prm_prty0_bb_b0 = {
	0, 1, prm_prty0_bb_b0_attn_idx, 0x230050, 0x23005c, 0x230058, 0x230054
};

static const u16 prm_prty1_bb_b0_attn_idx[24] = {
	2, 3, 5, 6, 7, 8, 9, 10, 11, 13, 14, 15, 16, 17, 18, 19, 20, 21, 24, 25,
	26, 27, 28, 29,
};

static struct attn_hw_reg prm_prty1_bb_b0 = {
	1, 24, prm_prty1_bb_b0_attn_idx, 0x230200, 0x23020c, 0x230208, 0x230204
};

static struct attn_hw_reg *prm_prty_bb_b0_regs[2] = {
	&prm_prty0_bb_b0, &prm_prty1_bb_b0,
};

static const u16 prm_prty0_k2_attn_idx[1] = {
	0,
};

static struct attn_hw_reg prm_prty0_k2 = {
	0, 1, prm_prty0_k2_attn_idx, 0x230050, 0x23005c, 0x230058, 0x230054
};

static const u16 prm_prty1_k2_attn_idx[23] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
	21,
	22, 23,
};

static struct attn_hw_reg prm_prty1_k2 = {
	1, 23, prm_prty1_k2_attn_idx, 0x230200, 0x23020c, 0x230208, 0x230204
};

static struct attn_hw_reg *prm_prty_k2_regs[2] = {
	&prm_prty0_k2, &prm_prty1_k2,
};

#ifdef ATTN_DESC
static const char *pbf_pb1_int_attn_desc[9] = {
	"pbf_pb1_address_error",
	"pbf_pb1_eop_error",
	"pbf_pb1_ififo_error",
	"pbf_pb1_pfifo_error",
	"pbf_pb1_db_buf_error",
	"pbf_pb1_th_exec_error",
	"pbf_pb1_tq_error_wr",
	"pbf_pb1_tq_error_rd_th",
	"pbf_pb1_tq_error_rd_ih",
};
#else
#define pbf_pb1_int_attn_desc OSAL_NULL
#endif

static const u16 pbf_pb1_int0_bb_a0_attn_idx[9] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8,
};

static struct attn_hw_reg pbf_pb1_int0_bb_a0 = {
	0, 9, pbf_pb1_int0_bb_a0_attn_idx, 0xda0040, 0xda004c, 0xda0048,
	0xda0044
};

static struct attn_hw_reg *pbf_pb1_int_bb_a0_regs[1] = {
	&pbf_pb1_int0_bb_a0,
};

static const u16 pbf_pb1_int0_bb_b0_attn_idx[9] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8,
};

static struct attn_hw_reg pbf_pb1_int0_bb_b0 = {
	0, 9, pbf_pb1_int0_bb_b0_attn_idx, 0xda0040, 0xda004c, 0xda0048,
	0xda0044
};

static struct attn_hw_reg *pbf_pb1_int_bb_b0_regs[1] = {
	&pbf_pb1_int0_bb_b0,
};

static const u16 pbf_pb1_int0_k2_attn_idx[9] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8,
};

static struct attn_hw_reg pbf_pb1_int0_k2 = {
	0, 9, pbf_pb1_int0_k2_attn_idx, 0xda0040, 0xda004c, 0xda0048, 0xda0044
};

static struct attn_hw_reg *pbf_pb1_int_k2_regs[1] = {
	&pbf_pb1_int0_k2,
};

#ifdef ATTN_DESC
static const char *pbf_pb1_prty_attn_desc[1] = {
	"pbf_pb1_datapath_registers",
};
#else
#define pbf_pb1_prty_attn_desc OSAL_NULL
#endif

static const u16 pbf_pb1_prty0_bb_b0_attn_idx[1] = {
	0,
};

static struct attn_hw_reg pbf_pb1_prty0_bb_b0 = {
	0, 1, pbf_pb1_prty0_bb_b0_attn_idx, 0xda0050, 0xda005c, 0xda0058,
	0xda0054
};

static struct attn_hw_reg *pbf_pb1_prty_bb_b0_regs[1] = {
	&pbf_pb1_prty0_bb_b0,
};

static const u16 pbf_pb1_prty0_k2_attn_idx[1] = {
	0,
};

static struct attn_hw_reg pbf_pb1_prty0_k2 = {
	0, 1, pbf_pb1_prty0_k2_attn_idx, 0xda0050, 0xda005c, 0xda0058, 0xda0054
};

static struct attn_hw_reg *pbf_pb1_prty_k2_regs[1] = {
	&pbf_pb1_prty0_k2,
};

#ifdef ATTN_DESC
static const char *pbf_pb2_int_attn_desc[9] = {
	"pbf_pb2_address_error",
	"pbf_pb2_eop_error",
	"pbf_pb2_ififo_error",
	"pbf_pb2_pfifo_error",
	"pbf_pb2_db_buf_error",
	"pbf_pb2_th_exec_error",
	"pbf_pb2_tq_error_wr",
	"pbf_pb2_tq_error_rd_th",
	"pbf_pb2_tq_error_rd_ih",
};
#else
#define pbf_pb2_int_attn_desc OSAL_NULL
#endif

static const u16 pbf_pb2_int0_bb_a0_attn_idx[9] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8,
};

static struct attn_hw_reg pbf_pb2_int0_bb_a0 = {
	0, 9, pbf_pb2_int0_bb_a0_attn_idx, 0xda4040, 0xda404c, 0xda4048,
	0xda4044
};

static struct attn_hw_reg *pbf_pb2_int_bb_a0_regs[1] = {
	&pbf_pb2_int0_bb_a0,
};

static const u16 pbf_pb2_int0_bb_b0_attn_idx[9] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8,
};

static struct attn_hw_reg pbf_pb2_int0_bb_b0 = {
	0, 9, pbf_pb2_int0_bb_b0_attn_idx, 0xda4040, 0xda404c, 0xda4048,
	0xda4044
};

static struct attn_hw_reg *pbf_pb2_int_bb_b0_regs[1] = {
	&pbf_pb2_int0_bb_b0,
};

static const u16 pbf_pb2_int0_k2_attn_idx[9] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8,
};

static struct attn_hw_reg pbf_pb2_int0_k2 = {
	0, 9, pbf_pb2_int0_k2_attn_idx, 0xda4040, 0xda404c, 0xda4048, 0xda4044
};

static struct attn_hw_reg *pbf_pb2_int_k2_regs[1] = {
	&pbf_pb2_int0_k2,
};

#ifdef ATTN_DESC
static const char *pbf_pb2_prty_attn_desc[1] = {
	"pbf_pb2_datapath_registers",
};
#else
#define pbf_pb2_prty_attn_desc OSAL_NULL
#endif

static const u16 pbf_pb2_prty0_bb_b0_attn_idx[1] = {
	0,
};

static struct attn_hw_reg pbf_pb2_prty0_bb_b0 = {
	0, 1, pbf_pb2_prty0_bb_b0_attn_idx, 0xda4050, 0xda405c, 0xda4058,
	0xda4054
};

static struct attn_hw_reg *pbf_pb2_prty_bb_b0_regs[1] = {
	&pbf_pb2_prty0_bb_b0,
};

static const u16 pbf_pb2_prty0_k2_attn_idx[1] = {
	0,
};

static struct attn_hw_reg pbf_pb2_prty0_k2 = {
	0, 1, pbf_pb2_prty0_k2_attn_idx, 0xda4050, 0xda405c, 0xda4058, 0xda4054
};

static struct attn_hw_reg *pbf_pb2_prty_k2_regs[1] = {
	&pbf_pb2_prty0_k2,
};

#ifdef ATTN_DESC
static const char *rpb_int_attn_desc[9] = {
	"rpb_address_error",
	"rpb_eop_error",
	"rpb_ififo_error",
	"rpb_pfifo_error",
	"rpb_db_buf_error",
	"rpb_th_exec_error",
	"rpb_tq_error_wr",
	"rpb_tq_error_rd_th",
	"rpb_tq_error_rd_ih",
};
#else
#define rpb_int_attn_desc OSAL_NULL
#endif

static const u16 rpb_int0_bb_a0_attn_idx[9] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8,
};

static struct attn_hw_reg rpb_int0_bb_a0 = {
	0, 9, rpb_int0_bb_a0_attn_idx, 0x23c040, 0x23c04c, 0x23c048, 0x23c044
};

static struct attn_hw_reg *rpb_int_bb_a0_regs[1] = {
	&rpb_int0_bb_a0,
};

static const u16 rpb_int0_bb_b0_attn_idx[9] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8,
};

static struct attn_hw_reg rpb_int0_bb_b0 = {
	0, 9, rpb_int0_bb_b0_attn_idx, 0x23c040, 0x23c04c, 0x23c048, 0x23c044
};

static struct attn_hw_reg *rpb_int_bb_b0_regs[1] = {
	&rpb_int0_bb_b0,
};

static const u16 rpb_int0_k2_attn_idx[9] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8,
};

static struct attn_hw_reg rpb_int0_k2 = {
	0, 9, rpb_int0_k2_attn_idx, 0x23c040, 0x23c04c, 0x23c048, 0x23c044
};

static struct attn_hw_reg *rpb_int_k2_regs[1] = {
	&rpb_int0_k2,
};

#ifdef ATTN_DESC
static const char *rpb_prty_attn_desc[1] = {
	"rpb_datapath_registers",
};
#else
#define rpb_prty_attn_desc OSAL_NULL
#endif

static const u16 rpb_prty0_bb_b0_attn_idx[1] = {
	0,
};

static struct attn_hw_reg rpb_prty0_bb_b0 = {
	0, 1, rpb_prty0_bb_b0_attn_idx, 0x23c050, 0x23c05c, 0x23c058, 0x23c054
};

static struct attn_hw_reg *rpb_prty_bb_b0_regs[1] = {
	&rpb_prty0_bb_b0,
};

static const u16 rpb_prty0_k2_attn_idx[1] = {
	0,
};

static struct attn_hw_reg rpb_prty0_k2 = {
	0, 1, rpb_prty0_k2_attn_idx, 0x23c050, 0x23c05c, 0x23c058, 0x23c054
};

static struct attn_hw_reg *rpb_prty_k2_regs[1] = {
	&rpb_prty0_k2,
};

#ifdef ATTN_DESC
static const char *btb_int_attn_desc[139] = {
	"btb_address_error",
	"btb_rc_pkt0_rls_error",
	"btb_unused_0",
	"btb_rc_pkt0_len_error",
	"btb_unused_1",
	"btb_rc_pkt0_protocol_error",
	"btb_rc_pkt1_rls_error",
	"btb_unused_2",
	"btb_rc_pkt1_len_error",
	"btb_unused_3",
	"btb_rc_pkt1_protocol_error",
	"btb_rc_pkt2_rls_error",
	"btb_unused_4",
	"btb_rc_pkt2_len_error",
	"btb_unused_5",
	"btb_rc_pkt2_protocol_error",
	"btb_rc_pkt3_rls_error",
	"btb_unused_6",
	"btb_rc_pkt3_len_error",
	"btb_unused_7",
	"btb_rc_pkt3_protocol_error",
	"btb_rc_sop_req_tc_port_error",
	"btb_unused_8",
	"btb_wc0_protocol_error",
	"btb_unused_9",
	"btb_ll_blk_error",
	"btb_ll_arb_calc_error",
	"btb_fc_alm_calc_error",
	"btb_wc0_inp_fifo_error",
	"btb_wc0_sop_fifo_error",
	"btb_wc0_len_fifo_error",
	"btb_wc0_eop_fifo_error",
	"btb_wc0_queue_fifo_error",
	"btb_wc0_free_point_fifo_error",
	"btb_wc0_next_point_fifo_error",
	"btb_wc0_strt_fifo_error",
	"btb_wc0_second_dscr_fifo_error",
	"btb_wc0_pkt_avail_fifo_error",
	"btb_wc0_notify_fifo_error",
	"btb_wc0_ll_req_fifo_error",
	"btb_wc0_ll_pa_cnt_error",
	"btb_wc0_bb_pa_cnt_error",
	"btb_wc_dup_upd_data_fifo_error",
	"btb_wc_dup_rsp_dscr_fifo_error",
	"btb_wc_dup_upd_point_fifo_error",
	"btb_wc_dup_pkt_avail_fifo_error",
	"btb_wc_dup_pkt_avail_cnt_error",
	"btb_rc_pkt0_side_fifo_error",
	"btb_rc_pkt0_req_fifo_error",
	"btb_rc_pkt0_blk_fifo_error",
	"btb_rc_pkt0_rls_left_fifo_error",
	"btb_rc_pkt0_strt_ptr_fifo_error",
	"btb_rc_pkt0_second_ptr_fifo_error",
	"btb_rc_pkt0_rsp_fifo_error",
	"btb_rc_pkt0_dscr_fifo_error",
	"btb_rc_pkt1_side_fifo_error",
	"btb_rc_pkt1_req_fifo_error",
	"btb_rc_pkt1_blk_fifo_error",
	"btb_rc_pkt1_rls_left_fifo_error",
	"btb_rc_pkt1_strt_ptr_fifo_error",
	"btb_rc_pkt1_second_ptr_fifo_error",
	"btb_rc_pkt1_rsp_fifo_error",
	"btb_rc_pkt1_dscr_fifo_error",
	"btb_rc_pkt2_side_fifo_error",
	"btb_rc_pkt2_req_fifo_error",
	"btb_rc_pkt2_blk_fifo_error",
	"btb_rc_pkt2_rls_left_fifo_error",
	"btb_rc_pkt2_strt_ptr_fifo_error",
	"btb_rc_pkt2_second_ptr_fifo_error",
	"btb_rc_pkt2_rsp_fifo_error",
	"btb_rc_pkt2_dscr_fifo_error",
	"btb_rc_pkt3_side_fifo_error",
	"btb_rc_pkt3_req_fifo_error",
	"btb_rc_pkt3_blk_fifo_error",
	"btb_rc_pkt3_rls_left_fifo_error",
	"btb_rc_pkt3_strt_ptr_fifo_error",
	"btb_rc_pkt3_second_ptr_fifo_error",
	"btb_rc_pkt3_rsp_fifo_error",
	"btb_rc_pkt3_dscr_fifo_error",
	"btb_rc_sop_queue_fifo_error",
	"btb_ll_arb_rls_fifo_error",
	"btb_ll_arb_prefetch_fifo_error",
	"btb_rc_pkt0_rls_fifo_error",
	"btb_rc_pkt1_rls_fifo_error",
	"btb_rc_pkt2_rls_fifo_error",
	"btb_rc_pkt3_rls_fifo_error",
	"btb_rc_pkt4_rls_fifo_error",
	"btb_rc_pkt5_rls_fifo_error",
	"btb_rc_pkt6_rls_fifo_error",
	"btb_rc_pkt7_rls_fifo_error",
	"btb_rc_pkt4_rls_error",
	"btb_rc_pkt4_len_error",
	"btb_rc_pkt4_protocol_error",
	"btb_rc_pkt4_side_fifo_error",
	"btb_rc_pkt4_req_fifo_error",
	"btb_rc_pkt4_blk_fifo_error",
	"btb_rc_pkt4_rls_left_fifo_error",
	"btb_rc_pkt4_strt_ptr_fifo_error",
	"btb_rc_pkt4_second_ptr_fifo_error",
	"btb_rc_pkt4_rsp_fifo_error",
	"btb_rc_pkt4_dscr_fifo_error",
	"btb_rc_pkt5_rls_error",
	"btb_rc_pkt5_len_error",
	"btb_rc_pkt5_protocol_error",
	"btb_rc_pkt5_side_fifo_error",
	"btb_rc_pkt5_req_fifo_error",
	"btb_rc_pkt5_blk_fifo_error",
	"btb_rc_pkt5_rls_left_fifo_error",
	"btb_rc_pkt5_strt_ptr_fifo_error",
	"btb_rc_pkt5_second_ptr_fifo_error",
	"btb_rc_pkt5_rsp_fifo_error",
	"btb_rc_pkt5_dscr_fifo_error",
	"btb_rc_pkt6_rls_error",
	"btb_rc_pkt6_len_error",
	"btb_rc_pkt6_protocol_error",
	"btb_rc_pkt6_side_fifo_error",
	"btb_rc_pkt6_req_fifo_error",
	"btb_rc_pkt6_blk_fifo_error",
	"btb_rc_pkt6_rls_left_fifo_error",
	"btb_rc_pkt6_strt_ptr_fifo_error",
	"btb_rc_pkt6_second_ptr_fifo_error",
	"btb_rc_pkt6_rsp_fifo_error",
	"btb_rc_pkt6_dscr_fifo_error",
	"btb_rc_pkt7_rls_error",
	"btb_rc_pkt7_len_error",
	"btb_rc_pkt7_protocol_error",
	"btb_rc_pkt7_side_fifo_error",
	"btb_rc_pkt7_req_fifo_error",
	"btb_rc_pkt7_blk_fifo_error",
	"btb_rc_pkt7_rls_left_fifo_error",
	"btb_rc_pkt7_strt_ptr_fifo_error",
	"btb_rc_pkt7_second_ptr_fifo_error",
	"btb_rc_pkt7_rsp_fifo_error",
	"btb_packet_available_sync_fifo_push_error",
	"btb_wc6_notify_fifo_error",
	"btb_wc9_queue_fifo_error",
	"btb_wc0_sync_fifo_push_error",
	"btb_rls_sync_fifo_push_error",
	"btb_rc_pkt7_dscr_fifo_error",
};
#else
#define btb_int_attn_desc OSAL_NULL
#endif

static const u16 btb_int0_bb_a0_attn_idx[16] = {
	0, 1, 3, 5, 6, 8, 10, 11, 13, 15, 16, 18, 20, 21, 23, 25,
};

static struct attn_hw_reg btb_int0_bb_a0 = {
	0, 16, btb_int0_bb_a0_attn_idx, 0xdb00c0, 0xdb00cc, 0xdb00c8, 0xdb00c4
};

static const u16 btb_int1_bb_a0_attn_idx[16] = {
	26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41,
};

static struct attn_hw_reg btb_int1_bb_a0 = {
	1, 16, btb_int1_bb_a0_attn_idx, 0xdb00d8, 0xdb00e4, 0xdb00e0, 0xdb00dc
};

static const u16 btb_int2_bb_a0_attn_idx[4] = {
	42, 43, 44, 45,
};

static struct attn_hw_reg btb_int2_bb_a0 = {
	2, 4, btb_int2_bb_a0_attn_idx, 0xdb00f0, 0xdb00fc, 0xdb00f8, 0xdb00f4
};

static const u16 btb_int3_bb_a0_attn_idx[32] = {
	46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
	64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77,
};

static struct attn_hw_reg btb_int3_bb_a0 = {
	3, 32, btb_int3_bb_a0_attn_idx, 0xdb0108, 0xdb0114, 0xdb0110, 0xdb010c
};

static const u16 btb_int4_bb_a0_attn_idx[23] = {
	78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
	96, 97, 98, 99, 100,
};

static struct attn_hw_reg btb_int4_bb_a0 = {
	4, 23, btb_int4_bb_a0_attn_idx, 0xdb0120, 0xdb012c, 0xdb0128, 0xdb0124
};

static const u16 btb_int5_bb_a0_attn_idx[32] = {
	101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114,
	115,
	116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129,
	    130, 131,
	132,
};

static struct attn_hw_reg btb_int5_bb_a0 = {
	5, 32, btb_int5_bb_a0_attn_idx, 0xdb0138, 0xdb0144, 0xdb0140, 0xdb013c
};

static const u16 btb_int6_bb_a0_attn_idx[1] = {
	133,
};

static struct attn_hw_reg btb_int6_bb_a0 = {
	6, 1, btb_int6_bb_a0_attn_idx, 0xdb0150, 0xdb015c, 0xdb0158, 0xdb0154
};

static const u16 btb_int8_bb_a0_attn_idx[1] = {
	134,
};

static struct attn_hw_reg btb_int8_bb_a0 = {
	7, 1, btb_int8_bb_a0_attn_idx, 0xdb0184, 0xdb0190, 0xdb018c, 0xdb0188
};

static const u16 btb_int9_bb_a0_attn_idx[1] = {
	135,
};

static struct attn_hw_reg btb_int9_bb_a0 = {
	8, 1, btb_int9_bb_a0_attn_idx, 0xdb019c, 0xdb01a8, 0xdb01a4, 0xdb01a0
};

static const u16 btb_int10_bb_a0_attn_idx[1] = {
	136,
};

static struct attn_hw_reg btb_int10_bb_a0 = {
	9, 1, btb_int10_bb_a0_attn_idx, 0xdb01b4, 0xdb01c0, 0xdb01bc, 0xdb01b8
};

static const u16 btb_int11_bb_a0_attn_idx[2] = {
	137, 138,
};

static struct attn_hw_reg btb_int11_bb_a0 = {
	10, 2, btb_int11_bb_a0_attn_idx, 0xdb01cc, 0xdb01d8, 0xdb01d4, 0xdb01d0
};

static struct attn_hw_reg *btb_int_bb_a0_regs[11] = {
	&btb_int0_bb_a0, &btb_int1_bb_a0, &btb_int2_bb_a0, &btb_int3_bb_a0,
	&btb_int4_bb_a0, &btb_int5_bb_a0, &btb_int6_bb_a0, &btb_int8_bb_a0,
	&btb_int9_bb_a0, &btb_int10_bb_a0,
	&btb_int11_bb_a0,
};

static const u16 btb_int0_bb_b0_attn_idx[16] = {
	0, 1, 3, 5, 6, 8, 10, 11, 13, 15, 16, 18, 20, 21, 23, 25,
};

static struct attn_hw_reg btb_int0_bb_b0 = {
	0, 16, btb_int0_bb_b0_attn_idx, 0xdb00c0, 0xdb00cc, 0xdb00c8, 0xdb00c4
};

static const u16 btb_int1_bb_b0_attn_idx[16] = {
	26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41,
};

static struct attn_hw_reg btb_int1_bb_b0 = {
	1, 16, btb_int1_bb_b0_attn_idx, 0xdb00d8, 0xdb00e4, 0xdb00e0, 0xdb00dc
};

static const u16 btb_int2_bb_b0_attn_idx[4] = {
	42, 43, 44, 45,
};

static struct attn_hw_reg btb_int2_bb_b0 = {
	2, 4, btb_int2_bb_b0_attn_idx, 0xdb00f0, 0xdb00fc, 0xdb00f8, 0xdb00f4
};

static const u16 btb_int3_bb_b0_attn_idx[32] = {
	46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
	64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77,
};

static struct attn_hw_reg btb_int3_bb_b0 = {
	3, 32, btb_int3_bb_b0_attn_idx, 0xdb0108, 0xdb0114, 0xdb0110, 0xdb010c
};

static const u16 btb_int4_bb_b0_attn_idx[23] = {
	78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
	96, 97, 98, 99, 100,
};

static struct attn_hw_reg btb_int4_bb_b0 = {
	4, 23, btb_int4_bb_b0_attn_idx, 0xdb0120, 0xdb012c, 0xdb0128, 0xdb0124
};

static const u16 btb_int5_bb_b0_attn_idx[32] = {
	101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114,
	115,
	116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129,
	    130, 131,
	132,
};

static struct attn_hw_reg btb_int5_bb_b0 = {
	5, 32, btb_int5_bb_b0_attn_idx, 0xdb0138, 0xdb0144, 0xdb0140, 0xdb013c
};

static const u16 btb_int6_bb_b0_attn_idx[1] = {
	133,
};

static struct attn_hw_reg btb_int6_bb_b0 = {
	6, 1, btb_int6_bb_b0_attn_idx, 0xdb0150, 0xdb015c, 0xdb0158, 0xdb0154
};

static const u16 btb_int8_bb_b0_attn_idx[1] = {
	134,
};

static struct attn_hw_reg btb_int8_bb_b0 = {
	7, 1, btb_int8_bb_b0_attn_idx, 0xdb0184, 0xdb0190, 0xdb018c, 0xdb0188
};

static const u16 btb_int9_bb_b0_attn_idx[1] = {
	135,
};

static struct attn_hw_reg btb_int9_bb_b0 = {
	8, 1, btb_int9_bb_b0_attn_idx, 0xdb019c, 0xdb01a8, 0xdb01a4, 0xdb01a0
};

static const u16 btb_int10_bb_b0_attn_idx[1] = {
	136,
};

static struct attn_hw_reg btb_int10_bb_b0 = {
	9, 1, btb_int10_bb_b0_attn_idx, 0xdb01b4, 0xdb01c0, 0xdb01bc, 0xdb01b8
};

static const u16 btb_int11_bb_b0_attn_idx[2] = {
	137, 138,
};

static struct attn_hw_reg btb_int11_bb_b0 = {
	10, 2, btb_int11_bb_b0_attn_idx, 0xdb01cc, 0xdb01d8, 0xdb01d4, 0xdb01d0
};

static struct attn_hw_reg *btb_int_bb_b0_regs[11] = {
	&btb_int0_bb_b0, &btb_int1_bb_b0, &btb_int2_bb_b0, &btb_int3_bb_b0,
	&btb_int4_bb_b0, &btb_int5_bb_b0, &btb_int6_bb_b0, &btb_int8_bb_b0,
	&btb_int9_bb_b0, &btb_int10_bb_b0,
	&btb_int11_bb_b0,
};

static const u16 btb_int0_k2_attn_idx[16] = {
	0, 1, 3, 5, 6, 8, 10, 11, 13, 15, 16, 18, 20, 21, 23, 25,
};

static struct attn_hw_reg btb_int0_k2 = {
	0, 16, btb_int0_k2_attn_idx, 0xdb00c0, 0xdb00cc, 0xdb00c8, 0xdb00c4
};

static const u16 btb_int1_k2_attn_idx[16] = {
	26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41,
};

static struct attn_hw_reg btb_int1_k2 = {
	1, 16, btb_int1_k2_attn_idx, 0xdb00d8, 0xdb00e4, 0xdb00e0, 0xdb00dc
};

static const u16 btb_int2_k2_attn_idx[4] = {
	42, 43, 44, 45,
};

static struct attn_hw_reg btb_int2_k2 = {
	2, 4, btb_int2_k2_attn_idx, 0xdb00f0, 0xdb00fc, 0xdb00f8, 0xdb00f4
};

static const u16 btb_int3_k2_attn_idx[32] = {
	46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
	64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77,
};

static struct attn_hw_reg btb_int3_k2 = {
	3, 32, btb_int3_k2_attn_idx, 0xdb0108, 0xdb0114, 0xdb0110, 0xdb010c
};

static const u16 btb_int4_k2_attn_idx[23] = {
	78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
	96, 97, 98, 99, 100,
};

static struct attn_hw_reg btb_int4_k2 = {
	4, 23, btb_int4_k2_attn_idx, 0xdb0120, 0xdb012c, 0xdb0128, 0xdb0124
};

static const u16 btb_int5_k2_attn_idx[32] = {
	101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114,
	115,
	116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129,
	    130, 131,
	132,
};

static struct attn_hw_reg btb_int5_k2 = {
	5, 32, btb_int5_k2_attn_idx, 0xdb0138, 0xdb0144, 0xdb0140, 0xdb013c
};

static const u16 btb_int6_k2_attn_idx[1] = {
	133,
};

static struct attn_hw_reg btb_int6_k2 = {
	6, 1, btb_int6_k2_attn_idx, 0xdb0150, 0xdb015c, 0xdb0158, 0xdb0154
};

static const u16 btb_int8_k2_attn_idx[1] = {
	134,
};

static struct attn_hw_reg btb_int8_k2 = {
	7, 1, btb_int8_k2_attn_idx, 0xdb0184, 0xdb0190, 0xdb018c, 0xdb0188
};

static const u16 btb_int9_k2_attn_idx[1] = {
	135,
};

static struct attn_hw_reg btb_int9_k2 = {
	8, 1, btb_int9_k2_attn_idx, 0xdb019c, 0xdb01a8, 0xdb01a4, 0xdb01a0
};

static const u16 btb_int10_k2_attn_idx[1] = {
	136,
};

static struct attn_hw_reg btb_int10_k2 = {
	9, 1, btb_int10_k2_attn_idx, 0xdb01b4, 0xdb01c0, 0xdb01bc, 0xdb01b8
};

static const u16 btb_int11_k2_attn_idx[2] = {
	137, 138,
};

static struct attn_hw_reg btb_int11_k2 = {
	10, 2, btb_int11_k2_attn_idx, 0xdb01cc, 0xdb01d8, 0xdb01d4, 0xdb01d0
};

static struct attn_hw_reg *btb_int_k2_regs[11] = {
	&btb_int0_k2, &btb_int1_k2, &btb_int2_k2, &btb_int3_k2, &btb_int4_k2,
	&btb_int5_k2, &btb_int6_k2, &btb_int8_k2, &btb_int9_k2, &btb_int10_k2,
	&btb_int11_k2,
};

#ifdef ATTN_DESC
static const char *btb_prty_attn_desc[36] = {
	"btb_ll_bank0_mem_prty",
	"btb_ll_bank1_mem_prty",
	"btb_ll_bank2_mem_prty",
	"btb_ll_bank3_mem_prty",
	"btb_datapath_registers",
	"btb_mem001_i_ecc_rf_int",
	"btb_mem008_i_ecc_rf_int",
	"btb_mem009_i_ecc_rf_int",
	"btb_mem010_i_ecc_rf_int",
	"btb_mem011_i_ecc_rf_int",
	"btb_mem012_i_ecc_rf_int",
	"btb_mem013_i_ecc_rf_int",
	"btb_mem014_i_ecc_rf_int",
	"btb_mem015_i_ecc_rf_int",
	"btb_mem016_i_ecc_rf_int",
	"btb_mem002_i_ecc_rf_int",
	"btb_mem003_i_ecc_rf_int",
	"btb_mem004_i_ecc_rf_int",
	"btb_mem005_i_ecc_rf_int",
	"btb_mem006_i_ecc_rf_int",
	"btb_mem007_i_ecc_rf_int",
	"btb_mem033_i_mem_prty",
	"btb_mem035_i_mem_prty",
	"btb_mem034_i_mem_prty",
	"btb_mem032_i_mem_prty",
	"btb_mem031_i_mem_prty",
	"btb_mem021_i_mem_prty",
	"btb_mem022_i_mem_prty",
	"btb_mem023_i_mem_prty",
	"btb_mem024_i_mem_prty",
	"btb_mem025_i_mem_prty",
	"btb_mem026_i_mem_prty",
	"btb_mem027_i_mem_prty",
	"btb_mem028_i_mem_prty",
	"btb_mem030_i_mem_prty",
	"btb_mem029_i_mem_prty",
};
#else
#define btb_prty_attn_desc OSAL_NULL
#endif

static const u16 btb_prty1_bb_a0_attn_idx[27] = {
	5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 25, 26, 27,
	28,
	29, 30, 31, 32, 33, 34, 35,
};

static struct attn_hw_reg btb_prty1_bb_a0 = {
	0, 27, btb_prty1_bb_a0_attn_idx, 0xdb0400, 0xdb040c, 0xdb0408, 0xdb0404
};

static struct attn_hw_reg *btb_prty_bb_a0_regs[1] = {
	&btb_prty1_bb_a0,
};

static const u16 btb_prty0_bb_b0_attn_idx[5] = {
	0, 1, 2, 3, 4,
};

static struct attn_hw_reg btb_prty0_bb_b0 = {
	0, 5, btb_prty0_bb_b0_attn_idx, 0xdb01dc, 0xdb01e8, 0xdb01e4, 0xdb01e0
};

static const u16 btb_prty1_bb_b0_attn_idx[23] = {
	5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 25, 30, 31,
	32,
	33, 34, 35,
};

static struct attn_hw_reg btb_prty1_bb_b0 = {
	1, 23, btb_prty1_bb_b0_attn_idx, 0xdb0400, 0xdb040c, 0xdb0408, 0xdb0404
};

static struct attn_hw_reg *btb_prty_bb_b0_regs[2] = {
	&btb_prty0_bb_b0, &btb_prty1_bb_b0,
};

static const u16 btb_prty0_k2_attn_idx[5] = {
	0, 1, 2, 3, 4,
};

static struct attn_hw_reg btb_prty0_k2 = {
	0, 5, btb_prty0_k2_attn_idx, 0xdb01dc, 0xdb01e8, 0xdb01e4, 0xdb01e0
};

static const u16 btb_prty1_k2_attn_idx[31] = {
	5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
	24,
	25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
};

static struct attn_hw_reg btb_prty1_k2 = {
	1, 31, btb_prty1_k2_attn_idx, 0xdb0400, 0xdb040c, 0xdb0408, 0xdb0404
};

static struct attn_hw_reg *btb_prty_k2_regs[2] = {
	&btb_prty0_k2, &btb_prty1_k2,
};

#ifdef ATTN_DESC
static const char *pbf_int_attn_desc[1] = {
	"pbf_address_error",
};
#else
#define pbf_int_attn_desc OSAL_NULL
#endif

static const u16 pbf_int0_bb_a0_attn_idx[1] = {
	0,
};

static struct attn_hw_reg pbf_int0_bb_a0 = {
	0, 1, pbf_int0_bb_a0_attn_idx, 0xd80180, 0xd8018c, 0xd80188, 0xd80184
};

static struct attn_hw_reg *pbf_int_bb_a0_regs[1] = {
	&pbf_int0_bb_a0,
};

static const u16 pbf_int0_bb_b0_attn_idx[1] = {
	0,
};

static struct attn_hw_reg pbf_int0_bb_b0 = {
	0, 1, pbf_int0_bb_b0_attn_idx, 0xd80180, 0xd8018c, 0xd80188, 0xd80184
};

static struct attn_hw_reg *pbf_int_bb_b0_regs[1] = {
	&pbf_int0_bb_b0,
};

static const u16 pbf_int0_k2_attn_idx[1] = {
	0,
};

static struct attn_hw_reg pbf_int0_k2 = {
	0, 1, pbf_int0_k2_attn_idx, 0xd80180, 0xd8018c, 0xd80188, 0xd80184
};

static struct attn_hw_reg *pbf_int_k2_regs[1] = {
	&pbf_int0_k2,
};

#ifdef ATTN_DESC
static const char *pbf_prty_attn_desc[59] = {
	"pbf_datapath_registers",
	"pbf_mem041_i_ecc_rf_int",
	"pbf_mem042_i_ecc_rf_int",
	"pbf_mem033_i_ecc_rf_int",
	"pbf_mem003_i_ecc_rf_int",
	"pbf_mem018_i_ecc_rf_int",
	"pbf_mem009_i_ecc_0_rf_int",
	"pbf_mem009_i_ecc_1_rf_int",
	"pbf_mem012_i_ecc_0_rf_int",
	"pbf_mem012_i_ecc_1_rf_int",
	"pbf_mem012_i_ecc_2_rf_int",
	"pbf_mem012_i_ecc_3_rf_int",
	"pbf_mem012_i_ecc_4_rf_int",
	"pbf_mem012_i_ecc_5_rf_int",
	"pbf_mem012_i_ecc_6_rf_int",
	"pbf_mem012_i_ecc_7_rf_int",
	"pbf_mem012_i_ecc_8_rf_int",
	"pbf_mem012_i_ecc_9_rf_int",
	"pbf_mem012_i_ecc_10_rf_int",
	"pbf_mem012_i_ecc_11_rf_int",
	"pbf_mem012_i_ecc_12_rf_int",
	"pbf_mem012_i_ecc_13_rf_int",
	"pbf_mem012_i_ecc_14_rf_int",
	"pbf_mem012_i_ecc_15_rf_int",
	"pbf_mem040_i_mem_prty",
	"pbf_mem039_i_mem_prty",
	"pbf_mem038_i_mem_prty",
	"pbf_mem034_i_mem_prty",
	"pbf_mem032_i_mem_prty",
	"pbf_mem031_i_mem_prty",
	"pbf_mem030_i_mem_prty",
	"pbf_mem029_i_mem_prty",
	"pbf_mem022_i_mem_prty",
	"pbf_mem023_i_mem_prty",
	"pbf_mem021_i_mem_prty",
	"pbf_mem020_i_mem_prty",
	"pbf_mem001_i_mem_prty",
	"pbf_mem002_i_mem_prty",
	"pbf_mem006_i_mem_prty",
	"pbf_mem007_i_mem_prty",
	"pbf_mem005_i_mem_prty",
	"pbf_mem004_i_mem_prty",
	"pbf_mem028_i_mem_prty",
	"pbf_mem026_i_mem_prty",
	"pbf_mem027_i_mem_prty",
	"pbf_mem019_i_mem_prty",
	"pbf_mem016_i_mem_prty",
	"pbf_mem017_i_mem_prty",
	"pbf_mem008_i_mem_prty",
	"pbf_mem011_i_mem_prty",
	"pbf_mem010_i_mem_prty",
	"pbf_mem024_i_mem_prty",
	"pbf_mem025_i_mem_prty",
	"pbf_mem037_i_mem_prty",
	"pbf_mem036_i_mem_prty",
	"pbf_mem035_i_mem_prty",
	"pbf_mem014_i_mem_prty",
	"pbf_mem015_i_mem_prty",
	"pbf_mem013_i_mem_prty",
};
#else
#define pbf_prty_attn_desc OSAL_NULL
#endif

static const u16 pbf_prty1_bb_a0_attn_idx[31] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
	21,
	22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
};

static struct attn_hw_reg pbf_prty1_bb_a0 = {
	0, 31, pbf_prty1_bb_a0_attn_idx, 0xd80200, 0xd8020c, 0xd80208, 0xd80204
};

static const u16 pbf_prty2_bb_a0_attn_idx[27] = {
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49,
	50, 51, 52, 53, 54, 55, 56, 57, 58,
};

static struct attn_hw_reg pbf_prty2_bb_a0 = {
	1, 27, pbf_prty2_bb_a0_attn_idx, 0xd80210, 0xd8021c, 0xd80218, 0xd80214
};

static struct attn_hw_reg *pbf_prty_bb_a0_regs[2] = {
	&pbf_prty1_bb_a0, &pbf_prty2_bb_a0,
};

static const u16 pbf_prty0_bb_b0_attn_idx[1] = {
	0,
};

static struct attn_hw_reg pbf_prty0_bb_b0 = {
	0, 1, pbf_prty0_bb_b0_attn_idx, 0xd80190, 0xd8019c, 0xd80198, 0xd80194
};

static const u16 pbf_prty1_bb_b0_attn_idx[31] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
	21,
	22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
};

static struct attn_hw_reg pbf_prty1_bb_b0 = {
	1, 31, pbf_prty1_bb_b0_attn_idx, 0xd80200, 0xd8020c, 0xd80208, 0xd80204
};

static const u16 pbf_prty2_bb_b0_attn_idx[27] = {
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49,
	50, 51, 52, 53, 54, 55, 56, 57, 58,
};

static struct attn_hw_reg pbf_prty2_bb_b0 = {
	2, 27, pbf_prty2_bb_b0_attn_idx, 0xd80210, 0xd8021c, 0xd80218, 0xd80214
};

static struct attn_hw_reg *pbf_prty_bb_b0_regs[3] = {
	&pbf_prty0_bb_b0, &pbf_prty1_bb_b0, &pbf_prty2_bb_b0,
};

static const u16 pbf_prty0_k2_attn_idx[1] = {
	0,
};

static struct attn_hw_reg pbf_prty0_k2 = {
	0, 1, pbf_prty0_k2_attn_idx, 0xd80190, 0xd8019c, 0xd80198, 0xd80194
};

static const u16 pbf_prty1_k2_attn_idx[31] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
	21,
	22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
};

static struct attn_hw_reg pbf_prty1_k2 = {
	1, 31, pbf_prty1_k2_attn_idx, 0xd80200, 0xd8020c, 0xd80208, 0xd80204
};

static const u16 pbf_prty2_k2_attn_idx[27] = {
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49,
	50, 51, 52, 53, 54, 55, 56, 57, 58,
};

static struct attn_hw_reg pbf_prty2_k2 = {
	2, 27, pbf_prty2_k2_attn_idx, 0xd80210, 0xd8021c, 0xd80218, 0xd80214
};

static struct attn_hw_reg *pbf_prty_k2_regs[3] = {
	&pbf_prty0_k2, &pbf_prty1_k2, &pbf_prty2_k2,
};

#ifdef ATTN_DESC
static const char *rdif_int_attn_desc[9] = {
	"rdif_address_error",
	"rdif_fatal_dix_err",
	"rdif_fatal_config_err",
	"rdif_cmd_fifo_err",
	"rdif_order_fifo_err",
	"rdif_rdata_fifo_err",
	"rdif_dif_stop_err",
	"rdif_partial_dif_w_eob",
	"rdif_l1_dirty_bit",
};
#else
#define rdif_int_attn_desc OSAL_NULL
#endif

static const u16 rdif_int0_bb_a0_attn_idx[8] = {
	0, 1, 2, 3, 4, 5, 6, 7,
};

static struct attn_hw_reg rdif_int0_bb_a0 = {
	0, 8, rdif_int0_bb_a0_attn_idx, 0x300180, 0x30018c, 0x300188, 0x300184
};

static struct attn_hw_reg *rdif_int_bb_a0_regs[1] = {
	&rdif_int0_bb_a0,
};

static const u16 rdif_int0_bb_b0_attn_idx[8] = {
	0, 1, 2, 3, 4, 5, 6, 7,
};

static struct attn_hw_reg rdif_int0_bb_b0 = {
	0, 8, rdif_int0_bb_b0_attn_idx, 0x300180, 0x30018c, 0x300188, 0x300184
};

static struct attn_hw_reg *rdif_int_bb_b0_regs[1] = {
	&rdif_int0_bb_b0,
};

static const u16 rdif_int0_k2_attn_idx[9] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8,
};

static struct attn_hw_reg rdif_int0_k2 = {
	0, 9, rdif_int0_k2_attn_idx, 0x300180, 0x30018c, 0x300188, 0x300184
};

static struct attn_hw_reg *rdif_int_k2_regs[1] = {
	&rdif_int0_k2,
};

#ifdef ATTN_DESC
static const char *rdif_prty_attn_desc[2] = {
	"rdif_unused_0",
	"rdif_datapath_registers",
};
#else
#define rdif_prty_attn_desc OSAL_NULL
#endif

static const u16 rdif_prty0_bb_b0_attn_idx[1] = {
	1,
};

static struct attn_hw_reg rdif_prty0_bb_b0 = {
	0, 1, rdif_prty0_bb_b0_attn_idx, 0x300190, 0x30019c, 0x300198, 0x300194
};

static struct attn_hw_reg *rdif_prty_bb_b0_regs[1] = {
	&rdif_prty0_bb_b0,
};

static const u16 rdif_prty0_k2_attn_idx[1] = {
	1,
};

static struct attn_hw_reg rdif_prty0_k2 = {
	0, 1, rdif_prty0_k2_attn_idx, 0x300190, 0x30019c, 0x300198, 0x300194
};

static struct attn_hw_reg *rdif_prty_k2_regs[1] = {
	&rdif_prty0_k2,
};

#ifdef ATTN_DESC
static const char *tdif_int_attn_desc[9] = {
	"tdif_address_error",
	"tdif_fatal_dix_err",
	"tdif_fatal_config_err",
	"tdif_cmd_fifo_err",
	"tdif_order_fifo_err",
	"tdif_rdata_fifo_err",
	"tdif_dif_stop_err",
	"tdif_partial_dif_w_eob",
	"tdif_l1_dirty_bit",
};
#else
#define tdif_int_attn_desc OSAL_NULL
#endif

static const u16 tdif_int0_bb_a0_attn_idx[8] = {
	0, 1, 2, 3, 4, 5, 6, 7,
};

static struct attn_hw_reg tdif_int0_bb_a0 = {
	0, 8, tdif_int0_bb_a0_attn_idx, 0x310180, 0x31018c, 0x310188, 0x310184
};

static struct attn_hw_reg *tdif_int_bb_a0_regs[1] = {
	&tdif_int0_bb_a0,
};

static const u16 tdif_int0_bb_b0_attn_idx[8] = {
	0, 1, 2, 3, 4, 5, 6, 7,
};

static struct attn_hw_reg tdif_int0_bb_b0 = {
	0, 8, tdif_int0_bb_b0_attn_idx, 0x310180, 0x31018c, 0x310188, 0x310184
};

static struct attn_hw_reg *tdif_int_bb_b0_regs[1] = {
	&tdif_int0_bb_b0,
};

static const u16 tdif_int0_k2_attn_idx[9] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8,
};

static struct attn_hw_reg tdif_int0_k2 = {
	0, 9, tdif_int0_k2_attn_idx, 0x310180, 0x31018c, 0x310188, 0x310184
};

static struct attn_hw_reg *tdif_int_k2_regs[1] = {
	&tdif_int0_k2,
};

#ifdef ATTN_DESC
static const char *tdif_prty_attn_desc[13] = {
	"tdif_unused_0",
	"tdif_datapath_registers",
	"tdif_mem005_i_ecc_rf_int",
	"tdif_mem009_i_ecc_rf_int",
	"tdif_mem010_i_ecc_rf_int",
	"tdif_mem011_i_ecc_rf_int",
	"tdif_mem001_i_mem_prty",
	"tdif_mem003_i_mem_prty",
	"tdif_mem002_i_mem_prty",
	"tdif_mem006_i_mem_prty",
	"tdif_mem007_i_mem_prty",
	"tdif_mem008_i_mem_prty",
	"tdif_mem004_i_mem_prty",
};
#else
#define tdif_prty_attn_desc OSAL_NULL
#endif

static const u16 tdif_prty1_bb_a0_attn_idx[11] = {
	2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
};

static struct attn_hw_reg tdif_prty1_bb_a0 = {
	0, 11, tdif_prty1_bb_a0_attn_idx, 0x310200, 0x31020c, 0x310208,
	0x310204
};

static struct attn_hw_reg *tdif_prty_bb_a0_regs[1] = {
	&tdif_prty1_bb_a0,
};

static const u16 tdif_prty0_bb_b0_attn_idx[1] = {
	1,
};

static struct attn_hw_reg tdif_prty0_bb_b0 = {
	0, 1, tdif_prty0_bb_b0_attn_idx, 0x310190, 0x31019c, 0x310198, 0x310194
};

static const u16 tdif_prty1_bb_b0_attn_idx[11] = {
	2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
};

static struct attn_hw_reg tdif_prty1_bb_b0 = {
	1, 11, tdif_prty1_bb_b0_attn_idx, 0x310200, 0x31020c, 0x310208,
	0x310204
};

static struct attn_hw_reg *tdif_prty_bb_b0_regs[2] = {
	&tdif_prty0_bb_b0, &tdif_prty1_bb_b0,
};

static const u16 tdif_prty0_k2_attn_idx[1] = {
	1,
};

static struct attn_hw_reg tdif_prty0_k2 = {
	0, 1, tdif_prty0_k2_attn_idx, 0x310190, 0x31019c, 0x310198, 0x310194
};

static const u16 tdif_prty1_k2_attn_idx[11] = {
	2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
};

static struct attn_hw_reg tdif_prty1_k2 = {
	1, 11, tdif_prty1_k2_attn_idx, 0x310200, 0x31020c, 0x310208, 0x310204
};

static struct attn_hw_reg *tdif_prty_k2_regs[2] = {
	&tdif_prty0_k2, &tdif_prty1_k2,
};

#ifdef ATTN_DESC
static const char *cdu_int_attn_desc[8] = {
	"cdu_address_error",
	"cdu_ccfc_ld_l1_num_error",
	"cdu_tcfc_ld_l1_num_error",
	"cdu_ccfc_wb_l1_num_error",
	"cdu_tcfc_wb_l1_num_error",
	"cdu_ccfc_cvld_error",
	"cdu_tcfc_cvld_error",
	"cdu_bvalid_error",
};
#else
#define cdu_int_attn_desc OSAL_NULL
#endif

static const u16 cdu_int0_bb_a0_attn_idx[8] = {
	0, 1, 2, 3, 4, 5, 6, 7,
};

static struct attn_hw_reg cdu_int0_bb_a0 = {
	0, 8, cdu_int0_bb_a0_attn_idx, 0x5801c0, 0x5801c4, 0x5801c8, 0x5801cc
};

static struct attn_hw_reg *cdu_int_bb_a0_regs[1] = {
	&cdu_int0_bb_a0,
};

static const u16 cdu_int0_bb_b0_attn_idx[8] = {
	0, 1, 2, 3, 4, 5, 6, 7,
};

static struct attn_hw_reg cdu_int0_bb_b0 = {
	0, 8, cdu_int0_bb_b0_attn_idx, 0x5801c0, 0x5801c4, 0x5801c8, 0x5801cc
};

static struct attn_hw_reg *cdu_int_bb_b0_regs[1] = {
	&cdu_int0_bb_b0,
};

static const u16 cdu_int0_k2_attn_idx[8] = {
	0, 1, 2, 3, 4, 5, 6, 7,
};

static struct attn_hw_reg cdu_int0_k2 = {
	0, 8, cdu_int0_k2_attn_idx, 0x5801c0, 0x5801c4, 0x5801c8, 0x5801cc
};

static struct attn_hw_reg *cdu_int_k2_regs[1] = {
	&cdu_int0_k2,
};

#ifdef ATTN_DESC
static const char *cdu_prty_attn_desc[5] = {
	"cdu_mem001_i_mem_prty",
	"cdu_mem004_i_mem_prty",
	"cdu_mem002_i_mem_prty",
	"cdu_mem005_i_mem_prty",
	"cdu_mem003_i_mem_prty",
};
#else
#define cdu_prty_attn_desc OSAL_NULL
#endif

static const u16 cdu_prty1_bb_a0_attn_idx[5] = {
	0, 1, 2, 3, 4,
};

static struct attn_hw_reg cdu_prty1_bb_a0 = {
	0, 5, cdu_prty1_bb_a0_attn_idx, 0x580200, 0x58020c, 0x580208, 0x580204
};

static struct attn_hw_reg *cdu_prty_bb_a0_regs[1] = {
	&cdu_prty1_bb_a0,
};

static const u16 cdu_prty1_bb_b0_attn_idx[5] = {
	0, 1, 2, 3, 4,
};

static struct attn_hw_reg cdu_prty1_bb_b0 = {
	0, 5, cdu_prty1_bb_b0_attn_idx, 0x580200, 0x58020c, 0x580208, 0x580204
};

static struct attn_hw_reg *cdu_prty_bb_b0_regs[1] = {
	&cdu_prty1_bb_b0,
};

static const u16 cdu_prty1_k2_attn_idx[5] = {
	0, 1, 2, 3, 4,
};

static struct attn_hw_reg cdu_prty1_k2 = {
	0, 5, cdu_prty1_k2_attn_idx, 0x580200, 0x58020c, 0x580208, 0x580204
};

static struct attn_hw_reg *cdu_prty_k2_regs[1] = {
	&cdu_prty1_k2,
};

#ifdef ATTN_DESC
static const char *ccfc_int_attn_desc[2] = {
	"ccfc_address_error",
	"ccfc_exe_error",
};
#else
#define ccfc_int_attn_desc OSAL_NULL
#endif

static const u16 ccfc_int0_bb_a0_attn_idx[2] = {
	0, 1,
};

static struct attn_hw_reg ccfc_int0_bb_a0 = {
	0, 2, ccfc_int0_bb_a0_attn_idx, 0x2e0180, 0x2e018c, 0x2e0188, 0x2e0184
};

static struct attn_hw_reg *ccfc_int_bb_a0_regs[1] = {
	&ccfc_int0_bb_a0,
};

static const u16 ccfc_int0_bb_b0_attn_idx[2] = {
	0, 1,
};

static struct attn_hw_reg ccfc_int0_bb_b0 = {
	0, 2, ccfc_int0_bb_b0_attn_idx, 0x2e0180, 0x2e018c, 0x2e0188, 0x2e0184
};

static struct attn_hw_reg *ccfc_int_bb_b0_regs[1] = {
	&ccfc_int0_bb_b0,
};

static const u16 ccfc_int0_k2_attn_idx[2] = {
	0, 1,
};

static struct attn_hw_reg ccfc_int0_k2 = {
	0, 2, ccfc_int0_k2_attn_idx, 0x2e0180, 0x2e018c, 0x2e0188, 0x2e0184
};

static struct attn_hw_reg *ccfc_int_k2_regs[1] = {
	&ccfc_int0_k2,
};

#ifdef ATTN_DESC
static const char *ccfc_prty_attn_desc[10] = {
	"ccfc_mem001_i_ecc_rf_int",
	"ccfc_mem003_i_mem_prty",
	"ccfc_mem007_i_mem_prty",
	"ccfc_mem006_i_mem_prty",
	"ccfc_ccam_par_err",
	"ccfc_scam_par_err",
	"ccfc_lc_que_ram_porta_lsb_par_err",
	"ccfc_lc_que_ram_porta_msb_par_err",
	"ccfc_lc_que_ram_portb_lsb_par_err",
	"ccfc_lc_que_ram_portb_msb_par_err",
};
#else
#define ccfc_prty_attn_desc OSAL_NULL
#endif

static const u16 ccfc_prty1_bb_a0_attn_idx[4] = {
	0, 1, 2, 3,
};

static struct attn_hw_reg ccfc_prty1_bb_a0 = {
	0, 4, ccfc_prty1_bb_a0_attn_idx, 0x2e0200, 0x2e020c, 0x2e0208, 0x2e0204
};

static const u16 ccfc_prty0_bb_a0_attn_idx[2] = {
	4, 5,
};

static struct attn_hw_reg ccfc_prty0_bb_a0 = {
	1, 2, ccfc_prty0_bb_a0_attn_idx, 0x2e05e4, 0x2e05f0, 0x2e05ec, 0x2e05e8
};

static struct attn_hw_reg *ccfc_prty_bb_a0_regs[2] = {
	&ccfc_prty1_bb_a0, &ccfc_prty0_bb_a0,
};

static const u16 ccfc_prty1_bb_b0_attn_idx[2] = {
	0, 1,
};

static struct attn_hw_reg ccfc_prty1_bb_b0 = {
	0, 2, ccfc_prty1_bb_b0_attn_idx, 0x2e0200, 0x2e020c, 0x2e0208, 0x2e0204
};

static const u16 ccfc_prty0_bb_b0_attn_idx[6] = {
	4, 5, 6, 7, 8, 9,
};

static struct attn_hw_reg ccfc_prty0_bb_b0 = {
	1, 6, ccfc_prty0_bb_b0_attn_idx, 0x2e05e4, 0x2e05f0, 0x2e05ec, 0x2e05e8
};

static struct attn_hw_reg *ccfc_prty_bb_b0_regs[2] = {
	&ccfc_prty1_bb_b0, &ccfc_prty0_bb_b0,
};

static const u16 ccfc_prty1_k2_attn_idx[2] = {
	0, 1,
};

static struct attn_hw_reg ccfc_prty1_k2 = {
	0, 2, ccfc_prty1_k2_attn_idx, 0x2e0200, 0x2e020c, 0x2e0208, 0x2e0204
};

static const u16 ccfc_prty0_k2_attn_idx[6] = {
	4, 5, 6, 7, 8, 9,
};

static struct attn_hw_reg ccfc_prty0_k2 = {
	1, 6, ccfc_prty0_k2_attn_idx, 0x2e05e4, 0x2e05f0, 0x2e05ec, 0x2e05e8
};

static struct attn_hw_reg *ccfc_prty_k2_regs[2] = {
	&ccfc_prty1_k2, &ccfc_prty0_k2,
};

#ifdef ATTN_DESC
static const char *tcfc_int_attn_desc[2] = {
	"tcfc_address_error",
	"tcfc_exe_error",
};
#else
#define tcfc_int_attn_desc OSAL_NULL
#endif

static const u16 tcfc_int0_bb_a0_attn_idx[2] = {
	0, 1,
};

static struct attn_hw_reg tcfc_int0_bb_a0 = {
	0, 2, tcfc_int0_bb_a0_attn_idx, 0x2d0180, 0x2d018c, 0x2d0188, 0x2d0184
};

static struct attn_hw_reg *tcfc_int_bb_a0_regs[1] = {
	&tcfc_int0_bb_a0,
};

static const u16 tcfc_int0_bb_b0_attn_idx[2] = {
	0, 1,
};

static struct attn_hw_reg tcfc_int0_bb_b0 = {
	0, 2, tcfc_int0_bb_b0_attn_idx, 0x2d0180, 0x2d018c, 0x2d0188, 0x2d0184
};

static struct attn_hw_reg *tcfc_int_bb_b0_regs[1] = {
	&tcfc_int0_bb_b0,
};

static const u16 tcfc_int0_k2_attn_idx[2] = {
	0, 1,
};

static struct attn_hw_reg tcfc_int0_k2 = {
	0, 2, tcfc_int0_k2_attn_idx, 0x2d0180, 0x2d018c, 0x2d0188, 0x2d0184
};

static struct attn_hw_reg *tcfc_int_k2_regs[1] = {
	&tcfc_int0_k2,
};

#ifdef ATTN_DESC
static const char *tcfc_prty_attn_desc[10] = {
	"tcfc_mem002_i_mem_prty",
	"tcfc_mem001_i_mem_prty",
	"tcfc_mem006_i_mem_prty",
	"tcfc_mem005_i_mem_prty",
	"tcfc_ccam_par_err",
	"tcfc_scam_par_err",
	"tcfc_lc_que_ram_porta_lsb_par_err",
	"tcfc_lc_que_ram_porta_msb_par_err",
	"tcfc_lc_que_ram_portb_lsb_par_err",
	"tcfc_lc_que_ram_portb_msb_par_err",
};
#else
#define tcfc_prty_attn_desc OSAL_NULL
#endif

static const u16 tcfc_prty1_bb_a0_attn_idx[4] = {
	0, 1, 2, 3,
};

static struct attn_hw_reg tcfc_prty1_bb_a0 = {
	0, 4, tcfc_prty1_bb_a0_attn_idx, 0x2d0200, 0x2d020c, 0x2d0208, 0x2d0204
};

static const u16 tcfc_prty0_bb_a0_attn_idx[2] = {
	4, 5,
};

static struct attn_hw_reg tcfc_prty0_bb_a0 = {
	1, 2, tcfc_prty0_bb_a0_attn_idx, 0x2d05e4, 0x2d05f0, 0x2d05ec, 0x2d05e8
};

static struct attn_hw_reg *tcfc_prty_bb_a0_regs[2] = {
	&tcfc_prty1_bb_a0, &tcfc_prty0_bb_a0,
};

static const u16 tcfc_prty1_bb_b0_attn_idx[2] = {
	0, 1,
};

static struct attn_hw_reg tcfc_prty1_bb_b0 = {
	0, 2, tcfc_prty1_bb_b0_attn_idx, 0x2d0200, 0x2d020c, 0x2d0208, 0x2d0204
};

static const u16 tcfc_prty0_bb_b0_attn_idx[6] = {
	4, 5, 6, 7, 8, 9,
};

static struct attn_hw_reg tcfc_prty0_bb_b0 = {
	1, 6, tcfc_prty0_bb_b0_attn_idx, 0x2d05e4, 0x2d05f0, 0x2d05ec, 0x2d05e8
};

static struct attn_hw_reg *tcfc_prty_bb_b0_regs[2] = {
	&tcfc_prty1_bb_b0, &tcfc_prty0_bb_b0,
};

static const u16 tcfc_prty1_k2_attn_idx[2] = {
	0, 1,
};

static struct attn_hw_reg tcfc_prty1_k2 = {
	0, 2, tcfc_prty1_k2_attn_idx, 0x2d0200, 0x2d020c, 0x2d0208, 0x2d0204
};

static const u16 tcfc_prty0_k2_attn_idx[6] = {
	4, 5, 6, 7, 8, 9,
};

static struct attn_hw_reg tcfc_prty0_k2 = {
	1, 6, tcfc_prty0_k2_attn_idx, 0x2d05e4, 0x2d05f0, 0x2d05ec, 0x2d05e8
};

static struct attn_hw_reg *tcfc_prty_k2_regs[2] = {
	&tcfc_prty1_k2, &tcfc_prty0_k2,
};

#ifdef ATTN_DESC
static const char *igu_int_attn_desc[11] = {
	"igu_address_error",
	"igu_ctrl_fifo_error_err",
	"igu_pxp_req_length_too_big",
	"igu_host_tries2access_prod_upd",
	"igu_vf_tries2acc_attn_cmd",
	"igu_mme_bigger_then_5",
	"igu_sb_index_is_not_valid",
	"igu_durin_int_read_with_simd_dis",
	"igu_cmd_fid_not_match",
	"igu_segment_access_invalid",
	"igu_attn_prod_acc",
};
#else
#define igu_int_attn_desc OSAL_NULL
#endif

static const u16 igu_int0_bb_a0_attn_idx[11] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
};

static struct attn_hw_reg igu_int0_bb_a0 = {
	0, 11, igu_int0_bb_a0_attn_idx, 0x180180, 0x18018c, 0x180188, 0x180184
};

static struct attn_hw_reg *igu_int_bb_a0_regs[1] = {
	&igu_int0_bb_a0,
};

static const u16 igu_int0_bb_b0_attn_idx[11] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
};

static struct attn_hw_reg igu_int0_bb_b0 = {
	0, 11, igu_int0_bb_b0_attn_idx, 0x180180, 0x18018c, 0x180188, 0x180184
};

static struct attn_hw_reg *igu_int_bb_b0_regs[1] = {
	&igu_int0_bb_b0,
};

static const u16 igu_int0_k2_attn_idx[11] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
};

static struct attn_hw_reg igu_int0_k2 = {
	0, 11, igu_int0_k2_attn_idx, 0x180180, 0x18018c, 0x180188, 0x180184
};

static struct attn_hw_reg *igu_int_k2_regs[1] = {
	&igu_int0_k2,
};

#ifdef ATTN_DESC
static const char *igu_prty_attn_desc[42] = {
	"igu_cam_parity",
	"igu_mem009_i_ecc_rf_int",
	"igu_mem015_i_mem_prty",
	"igu_mem016_i_mem_prty",
	"igu_mem017_i_mem_prty",
	"igu_mem018_i_mem_prty",
	"igu_mem019_i_mem_prty",
	"igu_mem001_i_mem_prty",
	"igu_mem002_i_mem_prty_0",
	"igu_mem002_i_mem_prty_1",
	"igu_mem004_i_mem_prty_0",
	"igu_mem004_i_mem_prty_1",
	"igu_mem004_i_mem_prty_2",
	"igu_mem003_i_mem_prty",
	"igu_mem005_i_mem_prty",
	"igu_mem006_i_mem_prty_0",
	"igu_mem006_i_mem_prty_1",
	"igu_mem008_i_mem_prty_0",
	"igu_mem008_i_mem_prty_1",
	"igu_mem008_i_mem_prty_2",
	"igu_mem007_i_mem_prty",
	"igu_mem010_i_mem_prty_0",
	"igu_mem010_i_mem_prty_1",
	"igu_mem012_i_mem_prty_0",
	"igu_mem012_i_mem_prty_1",
	"igu_mem012_i_mem_prty_2",
	"igu_mem011_i_mem_prty",
	"igu_mem013_i_mem_prty",
	"igu_mem014_i_mem_prty",
	"igu_mem020_i_mem_prty",
	"igu_mem003_i_mem_prty_0",
	"igu_mem003_i_mem_prty_1",
	"igu_mem003_i_mem_prty_2",
	"igu_mem002_i_mem_prty",
	"igu_mem007_i_mem_prty_0",
	"igu_mem007_i_mem_prty_1",
	"igu_mem007_i_mem_prty_2",
	"igu_mem006_i_mem_prty",
	"igu_mem010_i_mem_prty_2",
	"igu_mem010_i_mem_prty_3",
	"igu_mem013_i_mem_prty_0",
	"igu_mem013_i_mem_prty_1",
};
#else
#define igu_prty_attn_desc OSAL_NULL
#endif

static const u16 igu_prty0_bb_a0_attn_idx[1] = {
	0,
};

static struct attn_hw_reg igu_prty0_bb_a0 = {
	0, 1, igu_prty0_bb_a0_attn_idx, 0x180190, 0x18019c, 0x180198, 0x180194
};

static const u16 igu_prty1_bb_a0_attn_idx[31] = {
	1, 3, 4, 5, 6, 7, 10, 11, 14, 17, 18, 21, 22, 23, 24, 25, 26, 28, 29,
	30,
	31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41,
};

static struct attn_hw_reg igu_prty1_bb_a0 = {
	1, 31, igu_prty1_bb_a0_attn_idx, 0x180200, 0x18020c, 0x180208, 0x180204
};

static const u16 igu_prty2_bb_a0_attn_idx[1] = {
	2,
};

static struct attn_hw_reg igu_prty2_bb_a0 = {
	2, 1, igu_prty2_bb_a0_attn_idx, 0x180210, 0x18021c, 0x180218, 0x180214
};

static struct attn_hw_reg *igu_prty_bb_a0_regs[3] = {
	&igu_prty0_bb_a0, &igu_prty1_bb_a0, &igu_prty2_bb_a0,
};

static const u16 igu_prty0_bb_b0_attn_idx[1] = {
	0,
};

static struct attn_hw_reg igu_prty0_bb_b0 = {
	0, 1, igu_prty0_bb_b0_attn_idx, 0x180190, 0x18019c, 0x180198, 0x180194
};

static const u16 igu_prty1_bb_b0_attn_idx[31] = {
	1, 3, 4, 5, 6, 7, 10, 11, 14, 17, 18, 21, 22, 23, 24, 25, 26, 28, 29,
	30,
	31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41,
};

static struct attn_hw_reg igu_prty1_bb_b0 = {
	1, 31, igu_prty1_bb_b0_attn_idx, 0x180200, 0x18020c, 0x180208, 0x180204
};

static const u16 igu_prty2_bb_b0_attn_idx[1] = {
	2,
};

static struct attn_hw_reg igu_prty2_bb_b0 = {
	2, 1, igu_prty2_bb_b0_attn_idx, 0x180210, 0x18021c, 0x180218, 0x180214
};

static struct attn_hw_reg *igu_prty_bb_b0_regs[3] = {
	&igu_prty0_bb_b0, &igu_prty1_bb_b0, &igu_prty2_bb_b0,
};

static const u16 igu_prty0_k2_attn_idx[1] = {
	0,
};

static struct attn_hw_reg igu_prty0_k2 = {
	0, 1, igu_prty0_k2_attn_idx, 0x180190, 0x18019c, 0x180198, 0x180194
};

static const u16 igu_prty1_k2_attn_idx[28] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
	21,
	22, 23, 24, 25, 26, 27, 28,
};

static struct attn_hw_reg igu_prty1_k2 = {
	1, 28, igu_prty1_k2_attn_idx, 0x180200, 0x18020c, 0x180208, 0x180204
};

static struct attn_hw_reg *igu_prty_k2_regs[2] = {
	&igu_prty0_k2, &igu_prty1_k2,
};

#ifdef ATTN_DESC
static const char *cau_int_attn_desc[11] = {
	"cau_address_error",
	"cau_unauthorized_pxp_rd_cmd",
	"cau_unauthorized_pxp_length_cmd",
	"cau_pxp_sb_address_error",
	"cau_pxp_pi_number_error",
	"cau_cleanup_reg_sb_idx_error",
	"cau_fsm_invalid_line",
	"cau_cqe_fifo_err",
	"cau_igu_wdata_fifo_err",
	"cau_igu_req_fifo_err",
	"cau_igu_cmd_fifo_err",
};
#else
#define cau_int_attn_desc OSAL_NULL
#endif

static const u16 cau_int0_bb_a0_attn_idx[11] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
};

static struct attn_hw_reg cau_int0_bb_a0 = {
	0, 11, cau_int0_bb_a0_attn_idx, 0x1c00d4, 0x1c00d8, 0x1c00dc, 0x1c00e0
};

static struct attn_hw_reg *cau_int_bb_a0_regs[1] = {
	&cau_int0_bb_a0,
};

static const u16 cau_int0_bb_b0_attn_idx[11] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
};

static struct attn_hw_reg cau_int0_bb_b0 = {
	0, 11, cau_int0_bb_b0_attn_idx, 0x1c00d4, 0x1c00d8, 0x1c00dc, 0x1c00e0
};

static struct attn_hw_reg *cau_int_bb_b0_regs[1] = {
	&cau_int0_bb_b0,
};

static const u16 cau_int0_k2_attn_idx[11] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
};

static struct attn_hw_reg cau_int0_k2 = {
	0, 11, cau_int0_k2_attn_idx, 0x1c00d4, 0x1c00d8, 0x1c00dc, 0x1c00e0
};

static struct attn_hw_reg *cau_int_k2_regs[1] = {
	&cau_int0_k2,
};

#ifdef ATTN_DESC
static const char *cau_prty_attn_desc[15] = {
	"cau_mem006_i_ecc_rf_int",
	"cau_mem001_i_ecc_0_rf_int",
	"cau_mem001_i_ecc_1_rf_int",
	"cau_mem002_i_ecc_rf_int",
	"cau_mem004_i_ecc_rf_int",
	"cau_mem005_i_mem_prty",
	"cau_mem007_i_mem_prty",
	"cau_mem008_i_mem_prty",
	"cau_mem009_i_mem_prty",
	"cau_mem010_i_mem_prty",
	"cau_mem011_i_mem_prty",
	"cau_mem003_i_mem_prty_0",
	"cau_mem003_i_mem_prty_1",
	"cau_mem002_i_mem_prty",
	"cau_mem004_i_mem_prty",
};
#else
#define cau_prty_attn_desc OSAL_NULL
#endif

static const u16 cau_prty1_bb_a0_attn_idx[13] = {
	0, 1, 2, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
};

static struct attn_hw_reg cau_prty1_bb_a0 = {
	0, 13, cau_prty1_bb_a0_attn_idx, 0x1c0200, 0x1c020c, 0x1c0208, 0x1c0204
};

static struct attn_hw_reg *cau_prty_bb_a0_regs[1] = {
	&cau_prty1_bb_a0,
};

static const u16 cau_prty1_bb_b0_attn_idx[13] = {
	0, 1, 2, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
};

static struct attn_hw_reg cau_prty1_bb_b0 = {
	0, 13, cau_prty1_bb_b0_attn_idx, 0x1c0200, 0x1c020c, 0x1c0208, 0x1c0204
};

static struct attn_hw_reg *cau_prty_bb_b0_regs[1] = {
	&cau_prty1_bb_b0,
};

static const u16 cau_prty1_k2_attn_idx[13] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
};

static struct attn_hw_reg cau_prty1_k2 = {
	0, 13, cau_prty1_k2_attn_idx, 0x1c0200, 0x1c020c, 0x1c0208, 0x1c0204
};

static struct attn_hw_reg *cau_prty_k2_regs[1] = {
	&cau_prty1_k2,
};

#ifdef ATTN_DESC
static const char *umac_int_attn_desc[2] = {
	"umac_address_error",
	"umac_tx_overflow",
};
#else
#define umac_int_attn_desc OSAL_NULL
#endif

static const u16 umac_int0_k2_attn_idx[2] = {
	0, 1,
};

static struct attn_hw_reg umac_int0_k2 = {
	0, 2, umac_int0_k2_attn_idx, 0x51180, 0x5118c, 0x51188, 0x51184
};

static struct attn_hw_reg *umac_int_k2_regs[1] = {
	&umac_int0_k2,
};

#ifdef ATTN_DESC
static const char *dbg_int_attn_desc[1] = {
	"dbg_address_error",
};
#else
#define dbg_int_attn_desc OSAL_NULL
#endif

static const u16 dbg_int0_bb_a0_attn_idx[1] = {
	0,
};

static struct attn_hw_reg dbg_int0_bb_a0 = {
	0, 1, dbg_int0_bb_a0_attn_idx, 0x10180, 0x1018c, 0x10188, 0x10184
};

static struct attn_hw_reg *dbg_int_bb_a0_regs[1] = {
	&dbg_int0_bb_a0,
};

static const u16 dbg_int0_bb_b0_attn_idx[1] = {
	0,
};

static struct attn_hw_reg dbg_int0_bb_b0 = {
	0, 1, dbg_int0_bb_b0_attn_idx, 0x10180, 0x1018c, 0x10188, 0x10184
};

static struct attn_hw_reg *dbg_int_bb_b0_regs[1] = {
	&dbg_int0_bb_b0,
};

static const u16 dbg_int0_k2_attn_idx[1] = {
	0,
};

static struct attn_hw_reg dbg_int0_k2 = {
	0, 1, dbg_int0_k2_attn_idx, 0x10180, 0x1018c, 0x10188, 0x10184
};

static struct attn_hw_reg *dbg_int_k2_regs[1] = {
	&dbg_int0_k2,
};

#ifdef ATTN_DESC
static const char *dbg_prty_attn_desc[1] = {
	"dbg_mem001_i_mem_prty",
};
#else
#define dbg_prty_attn_desc OSAL_NULL
#endif

static const u16 dbg_prty1_bb_a0_attn_idx[1] = {
	0,
};

static struct attn_hw_reg dbg_prty1_bb_a0 = {
	0, 1, dbg_prty1_bb_a0_attn_idx, 0x10200, 0x1020c, 0x10208, 0x10204
};

static struct attn_hw_reg *dbg_prty_bb_a0_regs[1] = {
	&dbg_prty1_bb_a0,
};

static const u16 dbg_prty1_bb_b0_attn_idx[1] = {
	0,
};

static struct attn_hw_reg dbg_prty1_bb_b0 = {
	0, 1, dbg_prty1_bb_b0_attn_idx, 0x10200, 0x1020c, 0x10208, 0x10204
};

static struct attn_hw_reg *dbg_prty_bb_b0_regs[1] = {
	&dbg_prty1_bb_b0,
};

static const u16 dbg_prty1_k2_attn_idx[1] = {
	0,
};

static struct attn_hw_reg dbg_prty1_k2 = {
	0, 1, dbg_prty1_k2_attn_idx, 0x10200, 0x1020c, 0x10208, 0x10204
};

static struct attn_hw_reg *dbg_prty_k2_regs[1] = {
	&dbg_prty1_k2,
};

#ifdef ATTN_DESC
static const char *nig_int_attn_desc[196] = {
	"nig_address_error",
	"nig_debug_fifo_error",
	"nig_dorq_fifo_error",
	"nig_dbg_syncfifo_error_wr",
	"nig_dorq_syncfifo_error_wr",
	"nig_storm_syncfifo_error_wr",
	"nig_dbgmux_syncfifo_error_wr",
	"nig_msdm_syncfifo_error_wr",
	"nig_tsdm_syncfifo_error_wr",
	"nig_usdm_syncfifo_error_wr",
	"nig_xsdm_syncfifo_error_wr",
	"nig_ysdm_syncfifo_error_wr",
	"nig_tx_sopq0_error",
	"nig_tx_sopq1_error",
	"nig_tx_sopq2_error",
	"nig_tx_sopq3_error",
	"nig_tx_sopq4_error",
	"nig_tx_sopq5_error",
	"nig_tx_sopq6_error",
	"nig_tx_sopq7_error",
	"nig_tx_sopq8_error",
	"nig_tx_sopq9_error",
	"nig_tx_sopq10_error",
	"nig_tx_sopq11_error",
	"nig_tx_sopq12_error",
	"nig_tx_sopq13_error",
	"nig_tx_sopq14_error",
	"nig_tx_sopq15_error",
	"nig_lb_sopq0_error",
	"nig_lb_sopq1_error",
	"nig_lb_sopq2_error",
	"nig_lb_sopq3_error",
	"nig_lb_sopq4_error",
	"nig_lb_sopq5_error",
	"nig_lb_sopq6_error",
	"nig_lb_sopq7_error",
	"nig_lb_sopq8_error",
	"nig_lb_sopq9_error",
	"nig_lb_sopq10_error",
	"nig_lb_sopq11_error",
	"nig_lb_sopq12_error",
	"nig_lb_sopq13_error",
	"nig_lb_sopq14_error",
	"nig_lb_sopq15_error",
	"nig_p0_purelb_sopq_error",
	"nig_p0_rx_macfifo_error",
	"nig_p0_tx_macfifo_error",
	"nig_p0_tx_bmb_fifo_error",
	"nig_p0_lb_bmb_fifo_error",
	"nig_p0_tx_btb_fifo_error",
	"nig_p0_lb_btb_fifo_error",
	"nig_p0_rx_llh_dfifo_error",
	"nig_p0_tx_llh_dfifo_error",
	"nig_p0_lb_llh_dfifo_error",
	"nig_p0_rx_llh_hfifo_error",
	"nig_p0_tx_llh_hfifo_error",
	"nig_p0_lb_llh_hfifo_error",
	"nig_p0_rx_llh_rfifo_error",
	"nig_p0_tx_llh_rfifo_error",
	"nig_p0_lb_llh_rfifo_error",
	"nig_p0_storm_fifo_error",
	"nig_p0_storm_dscr_fifo_error",
	"nig_p0_tx_gnt_fifo_error",
	"nig_p0_lb_gnt_fifo_error",
	"nig_p0_tx_pause_too_long_int",
	"nig_p0_tc0_pause_too_long_int",
	"nig_p0_tc1_pause_too_long_int",
	"nig_p0_tc2_pause_too_long_int",
	"nig_p0_tc3_pause_too_long_int",
	"nig_p0_tc4_pause_too_long_int",
	"nig_p0_tc5_pause_too_long_int",
	"nig_p0_tc6_pause_too_long_int",
	"nig_p0_tc7_pause_too_long_int",
	"nig_p0_lb_tc0_pause_too_long_int",
	"nig_p0_lb_tc1_pause_too_long_int",
	"nig_p0_lb_tc2_pause_too_long_int",
	"nig_p0_lb_tc3_pause_too_long_int",
	"nig_p0_lb_tc4_pause_too_long_int",
	"nig_p0_lb_tc5_pause_too_long_int",
	"nig_p0_lb_tc6_pause_too_long_int",
	"nig_p0_lb_tc7_pause_too_long_int",
	"nig_p0_lb_tc8_pause_too_long_int",
	"nig_p1_purelb_sopq_error",
	"nig_p1_rx_macfifo_error",
	"nig_p1_tx_macfifo_error",
	"nig_p1_tx_bmb_fifo_error",
	"nig_p1_lb_bmb_fifo_error",
	"nig_p1_tx_btb_fifo_error",
	"nig_p1_lb_btb_fifo_error",
	"nig_p1_rx_llh_dfifo_error",
	"nig_p1_tx_llh_dfifo_error",
	"nig_p1_lb_llh_dfifo_error",
	"nig_p1_rx_llh_hfifo_error",
	"nig_p1_tx_llh_hfifo_error",
	"nig_p1_lb_llh_hfifo_error",
	"nig_p1_rx_llh_rfifo_error",
	"nig_p1_tx_llh_rfifo_error",
	"nig_p1_lb_llh_rfifo_error",
	"nig_p1_storm_fifo_error",
	"nig_p1_storm_dscr_fifo_error",
	"nig_p1_tx_gnt_fifo_error",
	"nig_p1_lb_gnt_fifo_error",
	"nig_p1_tx_pause_too_long_int",
	"nig_p1_tc0_pause_too_long_int",
	"nig_p1_tc1_pause_too_long_int",
	"nig_p1_tc2_pause_too_long_int",
	"nig_p1_tc3_pause_too_long_int",
	"nig_p1_tc4_pause_too_long_int",
	"nig_p1_tc5_pause_too_long_int",
	"nig_p1_tc6_pause_too_long_int",
	"nig_p1_tc7_pause_too_long_int",
	"nig_p1_lb_tc0_pause_too_long_int",
	"nig_p1_lb_tc1_pause_too_long_int",
	"nig_p1_lb_tc2_pause_too_long_int",
	"nig_p1_lb_tc3_pause_too_long_int",
	"nig_p1_lb_tc4_pause_too_long_int",
	"nig_p1_lb_tc5_pause_too_long_int",
	"nig_p1_lb_tc6_pause_too_long_int",
	"nig_p1_lb_tc7_pause_too_long_int",
	"nig_p1_lb_tc8_pause_too_long_int",
	"nig_p2_purelb_sopq_error",
	"nig_p2_rx_macfifo_error",
	"nig_p2_tx_macfifo_error",
	"nig_p2_tx_bmb_fifo_error",
	"nig_p2_lb_bmb_fifo_error",
	"nig_p2_tx_btb_fifo_error",
	"nig_p2_lb_btb_fifo_error",
	"nig_p2_rx_llh_dfifo_error",
	"nig_p2_tx_llh_dfifo_error",
	"nig_p2_lb_llh_dfifo_error",
	"nig_p2_rx_llh_hfifo_error",
	"nig_p2_tx_llh_hfifo_error",
	"nig_p2_lb_llh_hfifo_error",
	"nig_p2_rx_llh_rfifo_error",
	"nig_p2_tx_llh_rfifo_error",
	"nig_p2_lb_llh_rfifo_error",
	"nig_p2_storm_fifo_error",
	"nig_p2_storm_dscr_fifo_error",
	"nig_p2_tx_gnt_fifo_error",
	"nig_p2_lb_gnt_fifo_error",
	"nig_p2_tx_pause_too_long_int",
	"nig_p2_tc0_pause_too_long_int",
	"nig_p2_tc1_pause_too_long_int",
	"nig_p2_tc2_pause_too_long_int",
	"nig_p2_tc3_pause_too_long_int",
	"nig_p2_tc4_pause_too_long_int",
	"nig_p2_tc5_pause_too_long_int",
	"nig_p2_tc6_pause_too_long_int",
	"nig_p2_tc7_pause_too_long_int",
	"nig_p2_lb_tc0_pause_too_long_int",
	"nig_p2_lb_tc1_pause_too_long_int",
	"nig_p2_lb_tc2_pause_too_long_int",
	"nig_p2_lb_tc3_pause_too_long_int",
	"nig_p2_lb_tc4_pause_too_long_int",
	"nig_p2_lb_tc5_pause_too_long_int",
	"nig_p2_lb_tc6_pause_too_long_int",
	"nig_p2_lb_tc7_pause_too_long_int",
	"nig_p2_lb_tc8_pause_too_long_int",
	"nig_p3_purelb_sopq_error",
	"nig_p3_rx_macfifo_error",
	"nig_p3_tx_macfifo_error",
	"nig_p3_tx_bmb_fifo_error",
	"nig_p3_lb_bmb_fifo_error",
	"nig_p3_tx_btb_fifo_error",
	"nig_p3_lb_btb_fifo_error",
	"nig_p3_rx_llh_dfifo_error",
	"nig_p3_tx_llh_dfifo_error",
	"nig_p3_lb_llh_dfifo_error",
	"nig_p3_rx_llh_hfifo_error",
	"nig_p3_tx_llh_hfifo_error",
	"nig_p3_lb_llh_hfifo_error",
	"nig_p3_rx_llh_rfifo_error",
	"nig_p3_tx_llh_rfifo_error",
	"nig_p3_lb_llh_rfifo_error",
	"nig_p3_storm_fifo_error",
	"nig_p3_storm_dscr_fifo_error",
	"nig_p3_tx_gnt_fifo_error",
	"nig_p3_lb_gnt_fifo_error",
	"nig_p3_tx_pause_too_long_int",
	"nig_p3_tc0_pause_too_long_int",
	"nig_p3_tc1_pause_too_long_int",
	"nig_p3_tc2_pause_too_long_int",
	"nig_p3_tc3_pause_too_long_int",
	"nig_p3_tc4_pause_too_long_int",
	"nig_p3_tc5_pause_too_long_int",
	"nig_p3_tc6_pause_too_long_int",
	"nig_p3_tc7_pause_too_long_int",
	"nig_p3_lb_tc0_pause_too_long_int",
	"nig_p3_lb_tc1_pause_too_long_int",
	"nig_p3_lb_tc2_pause_too_long_int",
	"nig_p3_lb_tc3_pause_too_long_int",
	"nig_p3_lb_tc4_pause_too_long_int",
	"nig_p3_lb_tc5_pause_too_long_int",
	"nig_p3_lb_tc6_pause_too_long_int",
	"nig_p3_lb_tc7_pause_too_long_int",
	"nig_p3_lb_tc8_pause_too_long_int",
};
#else
#define nig_int_attn_desc OSAL_NULL
#endif

static const u16 nig_int0_bb_a0_attn_idx[12] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
};

static struct attn_hw_reg nig_int0_bb_a0 = {
	0, 12, nig_int0_bb_a0_attn_idx, 0x500040, 0x50004c, 0x500048, 0x500044
};

static const u16 nig_int1_bb_a0_attn_idx[32] = {
	12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
	30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43,
};

static struct attn_hw_reg nig_int1_bb_a0 = {
	1, 32, nig_int1_bb_a0_attn_idx, 0x500050, 0x50005c, 0x500058, 0x500054
};

static const u16 nig_int2_bb_a0_attn_idx[20] = {
	44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61,
	62, 63,
};

static struct attn_hw_reg nig_int2_bb_a0 = {
	2, 20, nig_int2_bb_a0_attn_idx, 0x500060, 0x50006c, 0x500068, 0x500064
};

static const u16 nig_int3_bb_a0_attn_idx[18] = {
	64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81,
};

static struct attn_hw_reg nig_int3_bb_a0 = {
	3, 18, nig_int3_bb_a0_attn_idx, 0x500070, 0x50007c, 0x500078, 0x500074
};

static const u16 nig_int4_bb_a0_attn_idx[20] = {
	82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99,
	100, 101,
};

static struct attn_hw_reg nig_int4_bb_a0 = {
	4, 20, nig_int4_bb_a0_attn_idx, 0x500080, 0x50008c, 0x500088, 0x500084
};

static const u16 nig_int5_bb_a0_attn_idx[18] = {
	102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115,
	116,
	117, 118, 119,
};

static struct attn_hw_reg nig_int5_bb_a0 = {
	5, 18, nig_int5_bb_a0_attn_idx, 0x500090, 0x50009c, 0x500098, 0x500094
};

static struct attn_hw_reg *nig_int_bb_a0_regs[6] = {
	&nig_int0_bb_a0, &nig_int1_bb_a0, &nig_int2_bb_a0, &nig_int3_bb_a0,
	&nig_int4_bb_a0, &nig_int5_bb_a0,
};

static const u16 nig_int0_bb_b0_attn_idx[12] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
};

static struct attn_hw_reg nig_int0_bb_b0 = {
	0, 12, nig_int0_bb_b0_attn_idx, 0x500040, 0x50004c, 0x500048, 0x500044
};

static const u16 nig_int1_bb_b0_attn_idx[32] = {
	12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
	30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43,
};

static struct attn_hw_reg nig_int1_bb_b0 = {
	1, 32, nig_int1_bb_b0_attn_idx, 0x500050, 0x50005c, 0x500058, 0x500054
};

static const u16 nig_int2_bb_b0_attn_idx[20] = {
	44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61,
	62, 63,
};

static struct attn_hw_reg nig_int2_bb_b0 = {
	2, 20, nig_int2_bb_b0_attn_idx, 0x500060, 0x50006c, 0x500068, 0x500064
};

static const u16 nig_int3_bb_b0_attn_idx[18] = {
	64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81,
};

static struct attn_hw_reg nig_int3_bb_b0 = {
	3, 18, nig_int3_bb_b0_attn_idx, 0x500070, 0x50007c, 0x500078, 0x500074
};

static const u16 nig_int4_bb_b0_attn_idx[20] = {
	82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99,
	100, 101,
};

static struct attn_hw_reg nig_int4_bb_b0 = {
	4, 20, nig_int4_bb_b0_attn_idx, 0x500080, 0x50008c, 0x500088, 0x500084
};

static const u16 nig_int5_bb_b0_attn_idx[18] = {
	102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115,
	116,
	117, 118, 119,
};

static struct attn_hw_reg nig_int5_bb_b0 = {
	5, 18, nig_int5_bb_b0_attn_idx, 0x500090, 0x50009c, 0x500098, 0x500094
};

static struct attn_hw_reg *nig_int_bb_b0_regs[6] = {
	&nig_int0_bb_b0, &nig_int1_bb_b0, &nig_int2_bb_b0, &nig_int3_bb_b0,
	&nig_int4_bb_b0, &nig_int5_bb_b0,
};

static const u16 nig_int0_k2_attn_idx[12] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
};

static struct attn_hw_reg nig_int0_k2 = {
	0, 12, nig_int0_k2_attn_idx, 0x500040, 0x50004c, 0x500048, 0x500044
};

static const u16 nig_int1_k2_attn_idx[32] = {
	12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
	30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43,
};

static struct attn_hw_reg nig_int1_k2 = {
	1, 32, nig_int1_k2_attn_idx, 0x500050, 0x50005c, 0x500058, 0x500054
};

static const u16 nig_int2_k2_attn_idx[20] = {
	44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61,
	62, 63,
};

static struct attn_hw_reg nig_int2_k2 = {
	2, 20, nig_int2_k2_attn_idx, 0x500060, 0x50006c, 0x500068, 0x500064
};

static const u16 nig_int3_k2_attn_idx[18] = {
	64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81,
};

static struct attn_hw_reg nig_int3_k2 = {
	3, 18, nig_int3_k2_attn_idx, 0x500070, 0x50007c, 0x500078, 0x500074
};

static const u16 nig_int4_k2_attn_idx[20] = {
	82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99,
	100, 101,
};

static struct attn_hw_reg nig_int4_k2 = {
	4, 20, nig_int4_k2_attn_idx, 0x500080, 0x50008c, 0x500088, 0x500084
};

static const u16 nig_int5_k2_attn_idx[18] = {
	102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115,
	116,
	117, 118, 119,
};

static struct attn_hw_reg nig_int5_k2 = {
	5, 18, nig_int5_k2_attn_idx, 0x500090, 0x50009c, 0x500098, 0x500094
};

static const u16 nig_int6_k2_attn_idx[20] = {
	120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133,
	134,
	135, 136, 137, 138, 139,
};

static struct attn_hw_reg nig_int6_k2 = {
	6, 20, nig_int6_k2_attn_idx, 0x5000a0, 0x5000ac, 0x5000a8, 0x5000a4
};

static const u16 nig_int7_k2_attn_idx[18] = {
	140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153,
	154,
	155, 156, 157,
};

static struct attn_hw_reg nig_int7_k2 = {
	7, 18, nig_int7_k2_attn_idx, 0x5000b0, 0x5000bc, 0x5000b8, 0x5000b4
};

static const u16 nig_int8_k2_attn_idx[20] = {
	158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171,
	172,
	173, 174, 175, 176, 177,
};

static struct attn_hw_reg nig_int8_k2 = {
	8, 20, nig_int8_k2_attn_idx, 0x5000c0, 0x5000cc, 0x5000c8, 0x5000c4
};

static const u16 nig_int9_k2_attn_idx[18] = {
	178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191,
	192,
	193, 194, 195,
};

static struct attn_hw_reg nig_int9_k2 = {
	9, 18, nig_int9_k2_attn_idx, 0x5000d0, 0x5000dc, 0x5000d8, 0x5000d4
};

static struct attn_hw_reg *nig_int_k2_regs[10] = {
	&nig_int0_k2, &nig_int1_k2, &nig_int2_k2, &nig_int3_k2, &nig_int4_k2,
	&nig_int5_k2, &nig_int6_k2, &nig_int7_k2, &nig_int8_k2, &nig_int9_k2,
};

#ifdef ATTN_DESC
static const char *nig_prty_attn_desc[113] = {
	"nig_datapath_parity_error",
	"nig_mem107_i_mem_prty",
	"nig_mem103_i_mem_prty",
	"nig_mem104_i_mem_prty",
	"nig_mem105_i_mem_prty",
	"nig_mem106_i_mem_prty",
	"nig_mem072_i_mem_prty",
	"nig_mem071_i_mem_prty",
	"nig_mem074_i_mem_prty",
	"nig_mem073_i_mem_prty",
	"nig_mem076_i_mem_prty",
	"nig_mem075_i_mem_prty",
	"nig_mem078_i_mem_prty",
	"nig_mem077_i_mem_prty",
	"nig_mem055_i_mem_prty",
	"nig_mem062_i_mem_prty",
	"nig_mem063_i_mem_prty",
	"nig_mem064_i_mem_prty",
	"nig_mem065_i_mem_prty",
	"nig_mem066_i_mem_prty",
	"nig_mem067_i_mem_prty",
	"nig_mem068_i_mem_prty",
	"nig_mem069_i_mem_prty",
	"nig_mem070_i_mem_prty",
	"nig_mem056_i_mem_prty",
	"nig_mem057_i_mem_prty",
	"nig_mem058_i_mem_prty",
	"nig_mem059_i_mem_prty",
	"nig_mem060_i_mem_prty",
	"nig_mem061_i_mem_prty",
	"nig_mem035_i_mem_prty",
	"nig_mem046_i_mem_prty",
	"nig_mem051_i_mem_prty",
	"nig_mem052_i_mem_prty",
	"nig_mem090_i_mem_prty",
	"nig_mem089_i_mem_prty",
	"nig_mem092_i_mem_prty",
	"nig_mem091_i_mem_prty",
	"nig_mem109_i_mem_prty",
	"nig_mem110_i_mem_prty",
	"nig_mem001_i_mem_prty",
	"nig_mem008_i_mem_prty",
	"nig_mem009_i_mem_prty",
	"nig_mem010_i_mem_prty",
	"nig_mem011_i_mem_prty",
	"nig_mem012_i_mem_prty",
	"nig_mem013_i_mem_prty",
	"nig_mem014_i_mem_prty",
	"nig_mem015_i_mem_prty",
	"nig_mem016_i_mem_prty",
	"nig_mem002_i_mem_prty",
	"nig_mem003_i_mem_prty",
	"nig_mem004_i_mem_prty",
	"nig_mem005_i_mem_prty",
	"nig_mem006_i_mem_prty",
	"nig_mem007_i_mem_prty",
	"nig_mem080_i_mem_prty",
	"nig_mem081_i_mem_prty",
	"nig_mem082_i_mem_prty",
	"nig_mem083_i_mem_prty",
	"nig_mem048_i_mem_prty",
	"nig_mem049_i_mem_prty",
	"nig_mem102_i_mem_prty",
	"nig_mem087_i_mem_prty",
	"nig_mem086_i_mem_prty",
	"nig_mem088_i_mem_prty",
	"nig_mem079_i_mem_prty",
	"nig_mem047_i_mem_prty",
	"nig_mem050_i_mem_prty",
	"nig_mem053_i_mem_prty",
	"nig_mem054_i_mem_prty",
	"nig_mem036_i_mem_prty",
	"nig_mem037_i_mem_prty",
	"nig_mem038_i_mem_prty",
	"nig_mem039_i_mem_prty",
	"nig_mem040_i_mem_prty",
	"nig_mem041_i_mem_prty",
	"nig_mem042_i_mem_prty",
	"nig_mem043_i_mem_prty",
	"nig_mem044_i_mem_prty",
	"nig_mem045_i_mem_prty",
	"nig_mem093_i_mem_prty",
	"nig_mem094_i_mem_prty",
	"nig_mem027_i_mem_prty",
	"nig_mem028_i_mem_prty",
	"nig_mem029_i_mem_prty",
	"nig_mem030_i_mem_prty",
	"nig_mem017_i_mem_prty",
	"nig_mem018_i_mem_prty",
	"nig_mem095_i_mem_prty",
	"nig_mem084_i_mem_prty",
	"nig_mem085_i_mem_prty",
	"nig_mem099_i_mem_prty",
	"nig_mem100_i_mem_prty",
	"nig_mem096_i_mem_prty",
	"nig_mem097_i_mem_prty",
	"nig_mem098_i_mem_prty",
	"nig_mem031_i_mem_prty",
	"nig_mem032_i_mem_prty",
	"nig_mem033_i_mem_prty",
	"nig_mem034_i_mem_prty",
	"nig_mem019_i_mem_prty",
	"nig_mem020_i_mem_prty",
	"nig_mem021_i_mem_prty",
	"nig_mem022_i_mem_prty",
	"nig_mem101_i_mem_prty",
	"nig_mem023_i_mem_prty",
	"nig_mem024_i_mem_prty",
	"nig_mem025_i_mem_prty",
	"nig_mem026_i_mem_prty",
	"nig_mem108_i_mem_prty",
	"nig_mem031_ext_i_mem_prty",
	"nig_mem034_ext_i_mem_prty",
};
#else
#define nig_prty_attn_desc OSAL_NULL
#endif

static const u16 nig_prty1_bb_a0_attn_idx[31] = {
	1, 2, 5, 12, 13, 23, 35, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,
	52, 53, 54, 55, 56, 60, 61, 62, 63, 64, 65, 66,
};

static struct attn_hw_reg nig_prty1_bb_a0 = {
	0, 31, nig_prty1_bb_a0_attn_idx, 0x500200, 0x50020c, 0x500208, 0x500204
};

static const u16 nig_prty2_bb_a0_attn_idx[31] = {
	33, 69, 70, 90, 91, 8, 11, 10, 14, 17, 18, 19, 20, 21, 22, 7, 6, 24, 25,
	26, 27, 28, 29, 15, 16, 57, 58, 59, 9, 94, 95,
};

static struct attn_hw_reg nig_prty2_bb_a0 = {
	1, 31, nig_prty2_bb_a0_attn_idx, 0x500210, 0x50021c, 0x500218, 0x500214
};

static const u16 nig_prty3_bb_a0_attn_idx[31] = {
	96, 97, 98, 103, 104, 92, 93, 105, 106, 107, 108, 109, 80, 31, 67, 83,
	84,
	3, 68, 85, 86, 89, 77, 78, 79, 4, 32, 36, 81, 82, 87,
};

static struct attn_hw_reg nig_prty3_bb_a0 = {
	2, 31, nig_prty3_bb_a0_attn_idx, 0x500220, 0x50022c, 0x500228, 0x500224
};

static const u16 nig_prty4_bb_a0_attn_idx[14] = {
	88, 101, 102, 75, 71, 74, 76, 73, 72, 34, 37, 99, 30, 100,
};

static struct attn_hw_reg nig_prty4_bb_a0 = {
	3, 14, nig_prty4_bb_a0_attn_idx, 0x500230, 0x50023c, 0x500238, 0x500234
};

static struct attn_hw_reg *nig_prty_bb_a0_regs[4] = {
	&nig_prty1_bb_a0, &nig_prty2_bb_a0, &nig_prty3_bb_a0, &nig_prty4_bb_a0,
};

static const u16 nig_prty0_bb_b0_attn_idx[1] = {
	0,
};

static struct attn_hw_reg nig_prty0_bb_b0 = {
	0, 1, nig_prty0_bb_b0_attn_idx, 0x5000a0, 0x5000ac, 0x5000a8, 0x5000a4
};

static const u16 nig_prty1_bb_b0_attn_idx[31] = {
	4, 5, 9, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
	48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59,
};

static struct attn_hw_reg nig_prty1_bb_b0 = {
	1, 31, nig_prty1_bb_b0_attn_idx, 0x500200, 0x50020c, 0x500208, 0x500204
};

static const u16 nig_prty2_bb_b0_attn_idx[31] = {
	90, 91, 64, 63, 65, 8, 11, 10, 13, 12, 66, 14, 17, 18, 19, 20, 21, 22,
	23,
	7, 6, 24, 25, 26, 27, 28, 29, 15, 16, 92, 93,
};

static struct attn_hw_reg nig_prty2_bb_b0 = {
	2, 31, nig_prty2_bb_b0_attn_idx, 0x500210, 0x50021c, 0x500218, 0x500214
};

static const u16 nig_prty3_bb_b0_attn_idx[31] = {
	94, 95, 96, 97, 99, 100, 103, 104, 105, 62, 108, 109, 80, 31, 1, 67, 60,
	69, 83, 84, 2, 3, 110, 61, 68, 70, 85, 86, 111, 112, 89,
};

static struct attn_hw_reg nig_prty3_bb_b0 = {
	3, 31, nig_prty3_bb_b0_attn_idx, 0x500220, 0x50022c, 0x500228, 0x500224
};

static const u16 nig_prty4_bb_b0_attn_idx[17] = {
	106, 107, 87, 88, 81, 82, 101, 102, 75, 71, 74, 76, 77, 78, 79, 73, 72,
};

static struct attn_hw_reg nig_prty4_bb_b0 = {
	4, 17, nig_prty4_bb_b0_attn_idx, 0x500230, 0x50023c, 0x500238, 0x500234
};

static struct attn_hw_reg *nig_prty_bb_b0_regs[5] = {
	&nig_prty0_bb_b0, &nig_prty1_bb_b0, &nig_prty2_bb_b0, &nig_prty3_bb_b0,
	&nig_prty4_bb_b0,
};

static const u16 nig_prty0_k2_attn_idx[1] = {
	0,
};

static struct attn_hw_reg nig_prty0_k2 = {
	0, 1, nig_prty0_k2_attn_idx, 0x5000e0, 0x5000ec, 0x5000e8, 0x5000e4
};

static const u16 nig_prty1_k2_attn_idx[31] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
	21,
	22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
};

static struct attn_hw_reg nig_prty1_k2 = {
	1, 31, nig_prty1_k2_attn_idx, 0x500200, 0x50020c, 0x500208, 0x500204
};

static const u16 nig_prty2_k2_attn_idx[31] = {
	67, 60, 61, 68, 32, 33, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80,
	37, 36, 81, 82, 83, 84, 85, 86, 48, 49, 87, 88, 89,
};

static struct attn_hw_reg nig_prty2_k2 = {
	2, 31, nig_prty2_k2_attn_idx, 0x500210, 0x50021c, 0x500218, 0x500214
};

static const u16 nig_prty3_k2_attn_idx[31] = {
	94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 92, 93, 105, 62, 106,
	107, 108, 109, 59, 90, 91, 64, 55, 41, 42, 43, 63, 65, 35, 34,
};

static struct attn_hw_reg nig_prty3_k2 = {
	3, 31, nig_prty3_k2_attn_idx, 0x500220, 0x50022c, 0x500228, 0x500224
};

static const u16 nig_prty4_k2_attn_idx[14] = {
	44, 45, 46, 47, 40, 50, 66, 56, 57, 58, 51, 52, 53, 54,
};

static struct attn_hw_reg nig_prty4_k2 = {
	4, 14, nig_prty4_k2_attn_idx, 0x500230, 0x50023c, 0x500238, 0x500234
};

static struct attn_hw_reg *nig_prty_k2_regs[5] = {
	&nig_prty0_k2, &nig_prty1_k2, &nig_prty2_k2, &nig_prty3_k2,
	&nig_prty4_k2,
};

#ifdef ATTN_DESC
static const char *wol_int_attn_desc[1] = {
	"wol_address_error",
};
#else
#define wol_int_attn_desc OSAL_NULL
#endif

static const u16 wol_int0_k2_attn_idx[1] = {
	0,
};

static struct attn_hw_reg wol_int0_k2 = {
	0, 1, wol_int0_k2_attn_idx, 0x600040, 0x60004c, 0x600048, 0x600044
};

static struct attn_hw_reg *wol_int_k2_regs[1] = {
	&wol_int0_k2,
};

#ifdef ATTN_DESC
static const char *wol_prty_attn_desc[24] = {
	"wol_mem017_i_mem_prty",
	"wol_mem018_i_mem_prty",
	"wol_mem019_i_mem_prty",
	"wol_mem020_i_mem_prty",
	"wol_mem021_i_mem_prty",
	"wol_mem022_i_mem_prty",
	"wol_mem023_i_mem_prty",
	"wol_mem024_i_mem_prty",
	"wol_mem001_i_mem_prty",
	"wol_mem008_i_mem_prty",
	"wol_mem009_i_mem_prty",
	"wol_mem010_i_mem_prty",
	"wol_mem011_i_mem_prty",
	"wol_mem012_i_mem_prty",
	"wol_mem013_i_mem_prty",
	"wol_mem014_i_mem_prty",
	"wol_mem015_i_mem_prty",
	"wol_mem016_i_mem_prty",
	"wol_mem002_i_mem_prty",
	"wol_mem003_i_mem_prty",
	"wol_mem004_i_mem_prty",
	"wol_mem005_i_mem_prty",
	"wol_mem006_i_mem_prty",
	"wol_mem007_i_mem_prty",
};
#else
#define wol_prty_attn_desc OSAL_NULL
#endif

static const u16 wol_prty1_k2_attn_idx[24] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23,
};

static struct attn_hw_reg wol_prty1_k2 = {
	0, 24, wol_prty1_k2_attn_idx, 0x600200, 0x60020c, 0x600208, 0x600204
};

static struct attn_hw_reg *wol_prty_k2_regs[1] = {
	&wol_prty1_k2,
};

#ifdef ATTN_DESC
static const char *bmbn_int_attn_desc[1] = {
	"bmbn_address_error",
};
#else
#define bmbn_int_attn_desc OSAL_NULL
#endif

static const u16 bmbn_int0_k2_attn_idx[1] = {
	0,
};

static struct attn_hw_reg bmbn_int0_k2 = {
	0, 1, bmbn_int0_k2_attn_idx, 0x610040, 0x61004c, 0x610048, 0x610044
};

static struct attn_hw_reg *bmbn_int_k2_regs[1] = {
	&bmbn_int0_k2,
};

#ifdef ATTN_DESC
static const char *ipc_int_attn_desc[14] = {
	"ipc_address_error",
	"ipc_unused_0",
	"ipc_vmain_por_assert",
	"ipc_vmain_por_deassert",
	"ipc_perst_assert",
	"ipc_perst_deassert",
	"ipc_otp_ecc_ded_0",
	"ipc_otp_ecc_ded_1",
	"ipc_otp_ecc_ded_2",
	"ipc_otp_ecc_ded_3",
	"ipc_otp_ecc_ded_4",
	"ipc_otp_ecc_ded_5",
	"ipc_otp_ecc_ded_6",
	"ipc_otp_ecc_ded_7",
};
#else
#define ipc_int_attn_desc OSAL_NULL
#endif

static const u16 ipc_int0_bb_a0_attn_idx[5] = {
	0, 2, 3, 4, 5,
};

static struct attn_hw_reg ipc_int0_bb_a0 = {
	0, 5, ipc_int0_bb_a0_attn_idx, 0x2050c, 0x20518, 0x20514, 0x20510
};

static struct attn_hw_reg *ipc_int_bb_a0_regs[1] = {
	&ipc_int0_bb_a0,
};

static const u16 ipc_int0_bb_b0_attn_idx[13] = {
	0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
};

static struct attn_hw_reg ipc_int0_bb_b0 = {
	0, 13, ipc_int0_bb_b0_attn_idx, 0x2050c, 0x20518, 0x20514, 0x20510
};

static struct attn_hw_reg *ipc_int_bb_b0_regs[1] = {
	&ipc_int0_bb_b0,
};

static const u16 ipc_int0_k2_attn_idx[5] = {
	0, 2, 3, 4, 5,
};

static struct attn_hw_reg ipc_int0_k2 = {
	0, 5, ipc_int0_k2_attn_idx, 0x202dc, 0x202e8, 0x202e4, 0x202e0
};

static struct attn_hw_reg *ipc_int_k2_regs[1] = {
	&ipc_int0_k2,
};

#ifdef ATTN_DESC
static const char *ipc_prty_attn_desc[1] = {
	"ipc_fake_par_err",
};
#else
#define ipc_prty_attn_desc OSAL_NULL
#endif

static const u16 ipc_prty0_bb_a0_attn_idx[1] = {
	0,
};

static struct attn_hw_reg ipc_prty0_bb_a0 = {
	0, 1, ipc_prty0_bb_a0_attn_idx, 0x2051c, 0x20528, 0x20524, 0x20520
};

static struct attn_hw_reg *ipc_prty_bb_a0_regs[1] = {
	&ipc_prty0_bb_a0,
};

static const u16 ipc_prty0_bb_b0_attn_idx[1] = {
	0,
};

static struct attn_hw_reg ipc_prty0_bb_b0 = {
	0, 1, ipc_prty0_bb_b0_attn_idx, 0x2051c, 0x20528, 0x20524, 0x20520
};

static struct attn_hw_reg *ipc_prty_bb_b0_regs[1] = {
	&ipc_prty0_bb_b0,
};

#ifdef ATTN_DESC
static const char *nwm_int_attn_desc[18] = {
	"nwm_address_error",
	"nwm_tx_overflow_0",
	"nwm_tx_underflow_0",
	"nwm_tx_overflow_1",
	"nwm_tx_underflow_1",
	"nwm_tx_overflow_2",
	"nwm_tx_underflow_2",
	"nwm_tx_overflow_3",
	"nwm_tx_underflow_3",
	"nwm_unused_0",
	"nwm_ln0_at_10M",
	"nwm_ln0_at_100M",
	"nwm_ln1_at_10M",
	"nwm_ln1_at_100M",
	"nwm_ln2_at_10M",
	"nwm_ln2_at_100M",
	"nwm_ln3_at_10M",
	"nwm_ln3_at_100M",
};
#else
#define nwm_int_attn_desc OSAL_NULL
#endif

static const u16 nwm_int0_k2_attn_idx[17] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14, 15, 16, 17,
};

static struct attn_hw_reg nwm_int0_k2 = {
	0, 17, nwm_int0_k2_attn_idx, 0x800004, 0x800010, 0x80000c, 0x800008
};

static struct attn_hw_reg *nwm_int_k2_regs[1] = {
	&nwm_int0_k2,
};

#ifdef ATTN_DESC
static const char *nwm_prty_attn_desc[72] = {
	"nwm_mem020_i_mem_prty",
	"nwm_mem028_i_mem_prty",
	"nwm_mem036_i_mem_prty",
	"nwm_mem044_i_mem_prty",
	"nwm_mem023_i_mem_prty",
	"nwm_mem031_i_mem_prty",
	"nwm_mem039_i_mem_prty",
	"nwm_mem047_i_mem_prty",
	"nwm_mem024_i_mem_prty",
	"nwm_mem032_i_mem_prty",
	"nwm_mem040_i_mem_prty",
	"nwm_mem048_i_mem_prty",
	"nwm_mem018_i_mem_prty",
	"nwm_mem026_i_mem_prty",
	"nwm_mem034_i_mem_prty",
	"nwm_mem042_i_mem_prty",
	"nwm_mem017_i_mem_prty",
	"nwm_mem025_i_mem_prty",
	"nwm_mem033_i_mem_prty",
	"nwm_mem041_i_mem_prty",
	"nwm_mem021_i_mem_prty",
	"nwm_mem029_i_mem_prty",
	"nwm_mem037_i_mem_prty",
	"nwm_mem045_i_mem_prty",
	"nwm_mem019_i_mem_prty",
	"nwm_mem027_i_mem_prty",
	"nwm_mem035_i_mem_prty",
	"nwm_mem043_i_mem_prty",
	"nwm_mem022_i_mem_prty",
	"nwm_mem030_i_mem_prty",
	"nwm_mem038_i_mem_prty",
	"nwm_mem046_i_mem_prty",
	"nwm_mem057_i_mem_prty",
	"nwm_mem059_i_mem_prty",
	"nwm_mem061_i_mem_prty",
	"nwm_mem063_i_mem_prty",
	"nwm_mem058_i_mem_prty",
	"nwm_mem060_i_mem_prty",
	"nwm_mem062_i_mem_prty",
	"nwm_mem064_i_mem_prty",
	"nwm_mem009_i_mem_prty",
	"nwm_mem010_i_mem_prty",
	"nwm_mem011_i_mem_prty",
	"nwm_mem012_i_mem_prty",
	"nwm_mem013_i_mem_prty",
	"nwm_mem014_i_mem_prty",
	"nwm_mem015_i_mem_prty",
	"nwm_mem016_i_mem_prty",
	"nwm_mem001_i_mem_prty",
	"nwm_mem002_i_mem_prty",
	"nwm_mem003_i_mem_prty",
	"nwm_mem004_i_mem_prty",
	"nwm_mem005_i_mem_prty",
	"nwm_mem006_i_mem_prty",
	"nwm_mem007_i_mem_prty",
	"nwm_mem008_i_mem_prty",
	"nwm_mem049_i_mem_prty",
	"nwm_mem053_i_mem_prty",
	"nwm_mem050_i_mem_prty",
	"nwm_mem054_i_mem_prty",
	"nwm_mem051_i_mem_prty",
	"nwm_mem055_i_mem_prty",
	"nwm_mem052_i_mem_prty",
	"nwm_mem056_i_mem_prty",
	"nwm_mem066_i_mem_prty",
	"nwm_mem068_i_mem_prty",
	"nwm_mem070_i_mem_prty",
	"nwm_mem072_i_mem_prty",
	"nwm_mem065_i_mem_prty",
	"nwm_mem067_i_mem_prty",
	"nwm_mem069_i_mem_prty",
	"nwm_mem071_i_mem_prty",
};
#else
#define nwm_prty_attn_desc OSAL_NULL
#endif

static const u16 nwm_prty1_k2_attn_idx[31] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20,
	21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
};

static struct attn_hw_reg nwm_prty1_k2 = {
	0, 31, nwm_prty1_k2_attn_idx, 0x800200, 0x80020c, 0x800208, 0x800204
};

static const u16 nwm_prty2_k2_attn_idx[31] = {
	31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
	49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61,
};

static struct attn_hw_reg nwm_prty2_k2 = {
	1, 31, nwm_prty2_k2_attn_idx, 0x800210, 0x80021c, 0x800218, 0x800214
};

static const u16 nwm_prty3_k2_attn_idx[10] = {
	62, 63, 64, 65, 66, 67, 68, 69, 70, 71,
};

static struct attn_hw_reg nwm_prty3_k2 = {
	2, 10, nwm_prty3_k2_attn_idx, 0x800220, 0x80022c, 0x800228, 0x800224
};

static struct attn_hw_reg *nwm_prty_k2_regs[3] = {
	&nwm_prty1_k2, &nwm_prty2_k2, &nwm_prty3_k2,
};

#ifdef ATTN_DESC
static const char *nws_int_attn_desc[38] = {
	"nws_address_error",
	"nws_ln0_an_resolve_50g_cr2",
	"nws_ln0_an_resolve_50g_kr2",
	"nws_ln0_an_resolve_40g_cr4",
	"nws_ln0_an_resolve_40g_kr4",
	"nws_ln0_an_resolve_25g_gr",
	"nws_ln0_an_resolve_25g_cr",
	"nws_ln0_an_resolve_25g_kr",
	"nws_ln0_an_resolve_10g_kr",
	"nws_ln0_an_resolve_1g_kx",
	"nws_unused_0",
	"nws_ln1_an_resolve_50g_cr2",
	"nws_ln1_an_resolve_50g_kr2",
	"nws_ln1_an_resolve_40g_cr4",
	"nws_ln1_an_resolve_40g_kr4",
	"nws_ln1_an_resolve_25g_gr",
	"nws_ln1_an_resolve_25g_cr",
	"nws_ln1_an_resolve_25g_kr",
	"nws_ln1_an_resolve_10g_kr",
	"nws_ln1_an_resolve_1g_kx",
	"nws_ln2_an_resolve_50g_cr2",
	"nws_ln2_an_resolve_50g_kr2",
	"nws_ln2_an_resolve_40g_cr4",
	"nws_ln2_an_resolve_40g_kr4",
	"nws_ln2_an_resolve_25g_gr",
	"nws_ln2_an_resolve_25g_cr",
	"nws_ln2_an_resolve_25g_kr",
	"nws_ln2_an_resolve_10g_kr",
	"nws_ln2_an_resolve_1g_kx",
	"nws_ln3_an_resolve_50g_cr2",
	"nws_ln3_an_resolve_50g_kr2",
	"nws_ln3_an_resolve_40g_cr4",
	"nws_ln3_an_resolve_40g_kr4",
	"nws_ln3_an_resolve_25g_gr",
	"nws_ln3_an_resolve_25g_cr",
	"nws_ln3_an_resolve_25g_kr",
	"nws_ln3_an_resolve_10g_kr",
	"nws_ln3_an_resolve_1g_kx",
};
#else
#define nws_int_attn_desc OSAL_NULL
#endif

static const u16 nws_int0_k2_attn_idx[10] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
};

static struct attn_hw_reg nws_int0_k2 = {
	0, 10, nws_int0_k2_attn_idx, 0x700180, 0x70018c, 0x700188, 0x700184
};

static const u16 nws_int1_k2_attn_idx[9] = {
	11, 12, 13, 14, 15, 16, 17, 18, 19,
};

static struct attn_hw_reg nws_int1_k2 = {
	1, 9, nws_int1_k2_attn_idx, 0x700190, 0x70019c, 0x700198, 0x700194
};

static const u16 nws_int2_k2_attn_idx[9] = {
	20, 21, 22, 23, 24, 25, 26, 27, 28,
};

static struct attn_hw_reg nws_int2_k2 = {
	2, 9, nws_int2_k2_attn_idx, 0x7001a0, 0x7001ac, 0x7001a8, 0x7001a4
};

static const u16 nws_int3_k2_attn_idx[9] = {
	29, 30, 31, 32, 33, 34, 35, 36, 37,
};

static struct attn_hw_reg nws_int3_k2 = {
	3, 9, nws_int3_k2_attn_idx, 0x7001b0, 0x7001bc, 0x7001b8, 0x7001b4
};

static struct attn_hw_reg *nws_int_k2_regs[4] = {
	&nws_int0_k2, &nws_int1_k2, &nws_int2_k2, &nws_int3_k2,
};

#ifdef ATTN_DESC
static const char *nws_prty_attn_desc[4] = {
	"nws_mem003_i_mem_prty",
	"nws_mem001_i_mem_prty",
	"nws_mem004_i_mem_prty",
	"nws_mem002_i_mem_prty",
};
#else
#define nws_prty_attn_desc OSAL_NULL
#endif

static const u16 nws_prty1_k2_attn_idx[4] = {
	0, 1, 2, 3,
};

static struct attn_hw_reg nws_prty1_k2 = {
	0, 4, nws_prty1_k2_attn_idx, 0x700200, 0x70020c, 0x700208, 0x700204
};

static struct attn_hw_reg *nws_prty_k2_regs[1] = {
	&nws_prty1_k2,
};

#ifdef ATTN_DESC
static const char *ms_int_attn_desc[1] = {
	"ms_address_error",
};
#else
#define ms_int_attn_desc OSAL_NULL
#endif

static const u16 ms_int0_k2_attn_idx[1] = {
	0,
};

static struct attn_hw_reg ms_int0_k2 = {
	0, 1, ms_int0_k2_attn_idx, 0x6a0180, 0x6a018c, 0x6a0188, 0x6a0184
};

static struct attn_hw_reg *ms_int_k2_regs[1] = {
	&ms_int0_k2,
};

static struct attn_hw_block attn_blocks[] = {
	{"grc", grc_int_attn_desc, grc_prty_attn_desc, {
							{1, 1,
							 grc_int_bb_a0_regs,
							 grc_prty_bb_a0_regs},
							{1, 1,
							 grc_int_bb_b0_regs,
							 grc_prty_bb_b0_regs},
							{1, 1, grc_int_k2_regs,
							 grc_prty_k2_regs} } },
	{"miscs", miscs_int_attn_desc, miscs_prty_attn_desc, {
							      {2, 0,

							miscs_int_bb_a0_regs,
							       OSAL_NULL},
							      {2, 1,

							miscs_int_bb_b0_regs,

							miscs_prty_bb_b0_regs},
							      {1, 1,

							miscs_int_k2_regs,

						miscs_prty_k2_regs } } },
	{"misc", misc_int_attn_desc, OSAL_NULL, {
						 {1, 0, misc_int_bb_a0_regs,
						  OSAL_NULL},
						 {1, 0, misc_int_bb_b0_regs,
						  OSAL_NULL},
						 {1, 0, misc_int_k2_regs,
						  OSAL_NULL } } },
	{"dbu", OSAL_NULL, OSAL_NULL, {
				       {0, 0, OSAL_NULL, OSAL_NULL},
				       {0, 0, OSAL_NULL, OSAL_NULL},
				       {0, 0, OSAL_NULL, OSAL_NULL } } },
	{"pglue_b", pglue_b_int_attn_desc, pglue_b_prty_attn_desc, {
								    {1, 1,

						pglue_b_int_bb_a0_regs,

						pglue_b_prty_bb_a0_regs},
								    {1, 2,

						pglue_b_int_bb_b0_regs,

						pglue_b_prty_bb_b0_regs},
								    {1, 3,

					     pglue_b_int_k2_regs,

					     pglue_b_prty_k2_regs } } },
	{"cnig", cnig_int_attn_desc, cnig_prty_attn_desc, {
							   {1, 0,
						    cnig_int_bb_a0_regs,
							    OSAL_NULL},
							   {1, 1,
						    cnig_int_bb_b0_regs,

						    cnig_prty_bb_b0_regs},
							   {1, 1,
							    cnig_int_k2_regs,

						    cnig_prty_k2_regs } } },
	{"cpmu", cpmu_int_attn_desc, OSAL_NULL, {
						 {1, 0, cpmu_int_bb_a0_regs,
						  OSAL_NULL},
						 {1, 0, cpmu_int_bb_b0_regs,
						  OSAL_NULL},
						 {1, 0, cpmu_int_k2_regs,
						  OSAL_NULL } } },
	{"ncsi", ncsi_int_attn_desc, ncsi_prty_attn_desc, {
							   {1, 1,
						    ncsi_int_bb_a0_regs,

						    ncsi_prty_bb_a0_regs},
							   {1, 1,
						    ncsi_int_bb_b0_regs,

						    ncsi_prty_bb_b0_regs},
							   {1, 1,
							    ncsi_int_k2_regs,

						    ncsi_prty_k2_regs } } },
	{"opte", OSAL_NULL, opte_prty_attn_desc, {
						  {0, 1, OSAL_NULL,
						   opte_prty_bb_a0_regs},
						  {0, 2, OSAL_NULL,
						   opte_prty_bb_b0_regs},
						  {0, 2, OSAL_NULL,
						   opte_prty_k2_regs } } },
	{"bmb", bmb_int_attn_desc, bmb_prty_attn_desc, {
							{12, 2,
							 bmb_int_bb_a0_regs,
							 bmb_prty_bb_a0_regs},
							{12, 3,
							 bmb_int_bb_b0_regs,
							 bmb_prty_bb_b0_regs},
						{12, 3, bmb_int_k2_regs,
							 bmb_prty_k2_regs } } },
	{"pcie", pcie_int_attn_desc, pcie_prty_attn_desc, {
							   {0, 1, OSAL_NULL,

						    pcie_prty_bb_a0_regs},
							   {0, 1, OSAL_NULL,

						    pcie_prty_bb_b0_regs},
							   {1, 2,
							    pcie_int_k2_regs,

						    pcie_prty_k2_regs } } },
	{"mcp", OSAL_NULL, OSAL_NULL, {
				       {0, 0, OSAL_NULL, OSAL_NULL},
				       {0, 0, OSAL_NULL, OSAL_NULL},
				       {0, 0, OSAL_NULL, OSAL_NULL } } },
	{"mcp2", OSAL_NULL, mcp2_prty_attn_desc, {
						  {0, 2, OSAL_NULL,
						   mcp2_prty_bb_a0_regs},
						  {0, 2, OSAL_NULL,
						   mcp2_prty_bb_b0_regs},
						  {0, 2, OSAL_NULL,
						   mcp2_prty_k2_regs } } },
	{"pswhst", pswhst_int_attn_desc, pswhst_prty_attn_desc, {
								 {1, 1,

						  pswhst_int_bb_a0_regs,

						  pswhst_prty_bb_a0_regs},
								 {1, 2,

						  pswhst_int_bb_b0_regs,

						  pswhst_prty_bb_b0_regs},
								 {1, 2,

						  pswhst_int_k2_regs,

						  pswhst_prty_k2_regs } } },
	{"pswhst2", pswhst2_int_attn_desc, pswhst2_prty_attn_desc, {
								    {1, 0,

						     pswhst2_int_bb_a0_regs,
							     OSAL_NULL},
								    {1, 1,

						     pswhst2_int_bb_b0_regs,

						pswhst2_prty_bb_b0_regs},
								    {1, 1,

					     pswhst2_int_k2_regs,

					     pswhst2_prty_k2_regs } } },
	{"pswrd", pswrd_int_attn_desc, pswrd_prty_attn_desc, {
							      {1, 0,

					      pswrd_int_bb_a0_regs,
							       OSAL_NULL},
							      {1, 1,

						       pswrd_int_bb_b0_regs,

						       pswrd_prty_bb_b0_regs},
							      {1, 1,

						       pswrd_int_k2_regs,

						       pswrd_prty_k2_regs } } },
	{"pswrd2", pswrd2_int_attn_desc, pswrd2_prty_attn_desc, {
								 {1, 2,

						  pswrd2_int_bb_a0_regs,

						  pswrd2_prty_bb_a0_regs},
								 {1, 3,

						  pswrd2_int_bb_b0_regs,

						  pswrd2_prty_bb_b0_regs},
								 {1, 3,

						  pswrd2_int_k2_regs,

						  pswrd2_prty_k2_regs } } },
	{"pswwr", pswwr_int_attn_desc, pswwr_prty_attn_desc, {
							      {1, 0,

					       pswwr_int_bb_a0_regs,
							       OSAL_NULL},
							      {1, 1,

					       pswwr_int_bb_b0_regs,

					       pswwr_prty_bb_b0_regs},
							      {1, 1,

					       pswwr_int_k2_regs,

					       pswwr_prty_k2_regs } } },
	{"pswwr2", pswwr2_int_attn_desc, pswwr2_prty_attn_desc, {
								 {1, 4,

						  pswwr2_int_bb_a0_regs,

						  pswwr2_prty_bb_a0_regs},
								 {1, 5,

						  pswwr2_int_bb_b0_regs,

						  pswwr2_prty_bb_b0_regs},
								 {1, 5,

						  pswwr2_int_k2_regs,

						  pswwr2_prty_k2_regs } } },
	{"pswrq", pswrq_int_attn_desc, pswrq_prty_attn_desc, {
							      {1, 0,

					       pswrq_int_bb_a0_regs,
							       OSAL_NULL},
							      {1, 1,

					       pswrq_int_bb_b0_regs,

					       pswrq_prty_bb_b0_regs},
							      {1, 1,

					       pswrq_int_k2_regs,

					       pswrq_prty_k2_regs } } },
	{"pswrq2", pswrq2_int_attn_desc, pswrq2_prty_attn_desc, {
								 {1, 1,

						  pswrq2_int_bb_a0_regs,

						  pswrq2_prty_bb_a0_regs},
								 {1, 1,

						  pswrq2_int_bb_b0_regs,

						  pswrq2_prty_bb_b0_regs},
								 {1, 1,

						  pswrq2_int_k2_regs,

						  pswrq2_prty_k2_regs } } },
	{"pglcs", pglcs_int_attn_desc, OSAL_NULL, {
						   {1, 0, pglcs_int_bb_a0_regs,
						    OSAL_NULL},
						   {1, 0, pglcs_int_bb_b0_regs,
						    OSAL_NULL},
						   {1, 0, pglcs_int_k2_regs,
						    OSAL_NULL } } },
	{"dmae", dmae_int_attn_desc, dmae_prty_attn_desc, {
							   {1, 1,
						    dmae_int_bb_a0_regs,

						    dmae_prty_bb_a0_regs},
							   {1, 1,
						    dmae_int_bb_b0_regs,

						    dmae_prty_bb_b0_regs},
							   {1, 1,
							    dmae_int_k2_regs,

					    dmae_prty_k2_regs } } },
	{"ptu", ptu_int_attn_desc, ptu_prty_attn_desc, {
							{1, 1,
							 ptu_int_bb_a0_regs,
							 ptu_prty_bb_a0_regs},
							{1, 1,
							 ptu_int_bb_b0_regs,
							 ptu_prty_bb_b0_regs},
							{1, 1, ptu_int_k2_regs,
							 ptu_prty_k2_regs } } },
	{"tcm", tcm_int_attn_desc, tcm_prty_attn_desc, {
							{3, 2,
							 tcm_int_bb_a0_regs,
							 tcm_prty_bb_a0_regs},
							{3, 2,
							 tcm_int_bb_b0_regs,
							 tcm_prty_bb_b0_regs},
							{3, 2, tcm_int_k2_regs,
							 tcm_prty_k2_regs } } },
	{"mcm", mcm_int_attn_desc, mcm_prty_attn_desc, {
							{3, 2,
							 mcm_int_bb_a0_regs,
							 mcm_prty_bb_a0_regs},
							{3, 2,
							 mcm_int_bb_b0_regs,
							 mcm_prty_bb_b0_regs},
							{3, 2, mcm_int_k2_regs,
							 mcm_prty_k2_regs } } },
	{"ucm", ucm_int_attn_desc, ucm_prty_attn_desc, {
							{3, 2,
							 ucm_int_bb_a0_regs,
							 ucm_prty_bb_a0_regs},
							{3, 2,
							 ucm_int_bb_b0_regs,
							 ucm_prty_bb_b0_regs},
							{3, 2, ucm_int_k2_regs,
							 ucm_prty_k2_regs } } },
	{"xcm", xcm_int_attn_desc, xcm_prty_attn_desc, {
							{3, 2,
							 xcm_int_bb_a0_regs,
							 xcm_prty_bb_a0_regs},
							{3, 2,
							 xcm_int_bb_b0_regs,
							 xcm_prty_bb_b0_regs},
							{3, 2, xcm_int_k2_regs,
							 xcm_prty_k2_regs } } },
	{"ycm", ycm_int_attn_desc, ycm_prty_attn_desc, {
							{3, 2,
							 ycm_int_bb_a0_regs,
							 ycm_prty_bb_a0_regs},
							{3, 2,
							 ycm_int_bb_b0_regs,
							 ycm_prty_bb_b0_regs},
							{3, 2, ycm_int_k2_regs,
							 ycm_prty_k2_regs } } },
	{"pcm", pcm_int_attn_desc, pcm_prty_attn_desc, {
							{3, 1,
							 pcm_int_bb_a0_regs,
							 pcm_prty_bb_a0_regs},
							{3, 1,
							 pcm_int_bb_b0_regs,
							 pcm_prty_bb_b0_regs},
							{3, 1, pcm_int_k2_regs,
							 pcm_prty_k2_regs } } },
	{"qm", qm_int_attn_desc, qm_prty_attn_desc, {
						     {1, 4, qm_int_bb_a0_regs,
						      qm_prty_bb_a0_regs},
						     {1, 4, qm_int_bb_b0_regs,
						      qm_prty_bb_b0_regs},
						     {1, 4, qm_int_k2_regs,
						      qm_prty_k2_regs } } },
	{"tm", tm_int_attn_desc, tm_prty_attn_desc, {
						     {2, 1, tm_int_bb_a0_regs,
						      tm_prty_bb_a0_regs},
						     {2, 1, tm_int_bb_b0_regs,
						      tm_prty_bb_b0_regs},
						     {2, 1, tm_int_k2_regs,
						      tm_prty_k2_regs } } },
	{"dorq", dorq_int_attn_desc, dorq_prty_attn_desc, {
							   {1, 1,
						    dorq_int_bb_a0_regs,

						    dorq_prty_bb_a0_regs},
							   {1, 2,
						    dorq_int_bb_b0_regs,

						    dorq_prty_bb_b0_regs},
							   {1, 2,
							    dorq_int_k2_regs,

						    dorq_prty_k2_regs } } },
	{"brb", brb_int_attn_desc, brb_prty_attn_desc, {
							{12, 2,
							 brb_int_bb_a0_regs,
							 brb_prty_bb_a0_regs},
							{12, 3,
							 brb_int_bb_b0_regs,
							 brb_prty_bb_b0_regs},
						{12, 3, brb_int_k2_regs,
							 brb_prty_k2_regs } } },
	{"src", src_int_attn_desc, OSAL_NULL, {
					       {1, 0, src_int_bb_a0_regs,
						OSAL_NULL},
					       {1, 0, src_int_bb_b0_regs,
						OSAL_NULL},
					       {1, 0, src_int_k2_regs,
						OSAL_NULL } } },
	{"prs", prs_int_attn_desc, prs_prty_attn_desc, {
							{1, 3,
							 prs_int_bb_a0_regs,
							 prs_prty_bb_a0_regs},
							{1, 3,
							 prs_int_bb_b0_regs,
							 prs_prty_bb_b0_regs},
							{1, 3, prs_int_k2_regs,
							 prs_prty_k2_regs } } },
	{"tsdm", tsdm_int_attn_desc, tsdm_prty_attn_desc, {
							   {1, 1,
						    tsdm_int_bb_a0_regs,

						    tsdm_prty_bb_a0_regs},
							   {1, 1,
						    tsdm_int_bb_b0_regs,

						    tsdm_prty_bb_b0_regs},
							   {1, 1,
						    tsdm_int_k2_regs,

						    tsdm_prty_k2_regs } } },
	{"msdm", msdm_int_attn_desc, msdm_prty_attn_desc, {
							   {1, 1,
						    msdm_int_bb_a0_regs,

						    msdm_prty_bb_a0_regs},
							   {1, 1,
						    msdm_int_bb_b0_regs,

						    msdm_prty_bb_b0_regs},
							   {1, 1,
							    msdm_int_k2_regs,

						    msdm_prty_k2_regs } } },
	{"usdm", usdm_int_attn_desc, usdm_prty_attn_desc, {
							   {1, 1,
						    usdm_int_bb_a0_regs,

						    usdm_prty_bb_a0_regs},
							   {1, 1,
						    usdm_int_bb_b0_regs,

						    usdm_prty_bb_b0_regs},
							   {1, 1,
							    usdm_int_k2_regs,

						    usdm_prty_k2_regs } } },
	{"xsdm", xsdm_int_attn_desc, xsdm_prty_attn_desc, {
							   {1, 1,
						    xsdm_int_bb_a0_regs,

						    xsdm_prty_bb_a0_regs},
							   {1, 1,
						    xsdm_int_bb_b0_regs,

						    xsdm_prty_bb_b0_regs},
							   {1, 1,
						    xsdm_int_k2_regs,

						    xsdm_prty_k2_regs } } },
	{"ysdm", ysdm_int_attn_desc, ysdm_prty_attn_desc, {
							   {1, 1,
						    ysdm_int_bb_a0_regs,

						    ysdm_prty_bb_a0_regs},
							   {1, 1,
						    ysdm_int_bb_b0_regs,

						    ysdm_prty_bb_b0_regs},
							   {1, 1,
						    ysdm_int_k2_regs,

						    ysdm_prty_k2_regs } } },
	{"psdm", psdm_int_attn_desc, psdm_prty_attn_desc, {
							   {1, 1,
						    psdm_int_bb_a0_regs,

						    psdm_prty_bb_a0_regs},
							   {1, 1,
						    psdm_int_bb_b0_regs,

						    psdm_prty_bb_b0_regs},
							   {1, 1,
						    psdm_int_k2_regs,

						    psdm_prty_k2_regs } } },
	{"tsem", tsem_int_attn_desc, tsem_prty_attn_desc, {
							   {3, 3,
						    tsem_int_bb_a0_regs,

						    tsem_prty_bb_a0_regs},
							   {3, 3,
						    tsem_int_bb_b0_regs,

						    tsem_prty_bb_b0_regs},
							   {3, 4,
						    tsem_int_k2_regs,

						    tsem_prty_k2_regs } } },
	{"msem", msem_int_attn_desc, msem_prty_attn_desc, {
							   {3, 2,
						    msem_int_bb_a0_regs,

						    msem_prty_bb_a0_regs},
							   {3, 2,
						    msem_int_bb_b0_regs,

						    msem_prty_bb_b0_regs},
							   {3, 3,
						    msem_int_k2_regs,

						    msem_prty_k2_regs } } },
	{"usem", usem_int_attn_desc, usem_prty_attn_desc, {
							   {3, 2,
						    usem_int_bb_a0_regs,

						    usem_prty_bb_a0_regs},
							   {3, 2,
						    usem_int_bb_b0_regs,

						    usem_prty_bb_b0_regs},
							   {3, 3,
						    usem_int_k2_regs,

						    usem_prty_k2_regs } } },
	{"xsem", xsem_int_attn_desc, xsem_prty_attn_desc, {
							   {3, 2,
						    xsem_int_bb_a0_regs,

						    xsem_prty_bb_a0_regs},
							   {3, 2,
						    xsem_int_bb_b0_regs,

						    xsem_prty_bb_b0_regs},
							   {3, 3,
						    xsem_int_k2_regs,

						    xsem_prty_k2_regs } } },
	{"ysem", ysem_int_attn_desc, ysem_prty_attn_desc, {
							   {3, 2,
						    ysem_int_bb_a0_regs,

						    ysem_prty_bb_a0_regs},
							   {3, 2,
						    ysem_int_bb_b0_regs,

						    ysem_prty_bb_b0_regs},
							   {3, 3,
						    ysem_int_k2_regs,

						    ysem_prty_k2_regs } } },
	{"psem", psem_int_attn_desc, psem_prty_attn_desc, {
							   {3, 3,
						    psem_int_bb_a0_regs,

						    psem_prty_bb_a0_regs},
							   {3, 3,
						    psem_int_bb_b0_regs,

						    psem_prty_bb_b0_regs},
							   {3, 4,
						    psem_int_k2_regs,

						    psem_prty_k2_regs } } },
	{"rss", rss_int_attn_desc, rss_prty_attn_desc, {
							{1, 1,
							 rss_int_bb_a0_regs,
							 rss_prty_bb_a0_regs},
							{1, 1,
							 rss_int_bb_b0_regs,
							 rss_prty_bb_b0_regs},
							{1, 1, rss_int_k2_regs,
							 rss_prty_k2_regs } } },
	{"tmld", tmld_int_attn_desc, tmld_prty_attn_desc, {
							   {1, 1,
						    tmld_int_bb_a0_regs,

						    tmld_prty_bb_a0_regs},
							   {1, 1,
						    tmld_int_bb_b0_regs,

						    tmld_prty_bb_b0_regs},
							   {1, 1,
							    tmld_int_k2_regs,

						    tmld_prty_k2_regs } } },
	{"muld", muld_int_attn_desc, muld_prty_attn_desc, {
							   {1, 1,
						    muld_int_bb_a0_regs,

						    muld_prty_bb_a0_regs},
							   {1, 1,
						    muld_int_bb_b0_regs,

						    muld_prty_bb_b0_regs},
							   {1, 1,
						    muld_int_k2_regs,

						    muld_prty_k2_regs } } },
	{"yuld", yuld_int_attn_desc, yuld_prty_attn_desc, {
							   {1, 1,
						    yuld_int_bb_a0_regs,

						    yuld_prty_bb_a0_regs},
							   {1, 1,
						    yuld_int_bb_b0_regs,

						    yuld_prty_bb_b0_regs},
							   {1, 1,
						    yuld_int_k2_regs,

						    yuld_prty_k2_regs } } },
	{"xyld", xyld_int_attn_desc, xyld_prty_attn_desc, {
							   {1, 1,
						    xyld_int_bb_a0_regs,

						    xyld_prty_bb_a0_regs},
							   {1, 1,
						    xyld_int_bb_b0_regs,

						    xyld_prty_bb_b0_regs},
							   {1, 1,
						    xyld_int_k2_regs,

						    xyld_prty_k2_regs } } },
	{"prm", prm_int_attn_desc, prm_prty_attn_desc, {
							{1, 1,
							 prm_int_bb_a0_regs,
							 prm_prty_bb_a0_regs},
							{1, 2,
							 prm_int_bb_b0_regs,
							 prm_prty_bb_b0_regs},
							{1, 2, prm_int_k2_regs,
							 prm_prty_k2_regs } } },
	{"pbf_pb1", pbf_pb1_int_attn_desc, pbf_pb1_prty_attn_desc, {
								    {1, 0,

						     pbf_pb1_int_bb_a0_regs,
						     OSAL_NULL},
								    {1, 1,

						     pbf_pb1_int_bb_b0_regs,

						     pbf_pb1_prty_bb_b0_regs},
								    {1, 1,

						     pbf_pb1_int_k2_regs,

						     pbf_pb1_prty_k2_regs } } },
	{"pbf_pb2", pbf_pb2_int_attn_desc, pbf_pb2_prty_attn_desc, {
								    {1, 0,

						     pbf_pb2_int_bb_a0_regs,
						     OSAL_NULL},
								    {1, 1,

						     pbf_pb2_int_bb_b0_regs,

						     pbf_pb2_prty_bb_b0_regs},
								    {1, 1,

						     pbf_pb2_int_k2_regs,

						     pbf_pb2_prty_k2_regs } } },
	{"rpb", rpb_int_attn_desc, rpb_prty_attn_desc, {
							{1, 0,
							 rpb_int_bb_a0_regs,
							 OSAL_NULL},
							{1, 1,
							 rpb_int_bb_b0_regs,
							 rpb_prty_bb_b0_regs},
							{1, 1, rpb_int_k2_regs,
							 rpb_prty_k2_regs } } },
	{"btb", btb_int_attn_desc, btb_prty_attn_desc, {
							{11, 1,
							 btb_int_bb_a0_regs,
							 btb_prty_bb_a0_regs},
							{11, 2,
							 btb_int_bb_b0_regs,
							 btb_prty_bb_b0_regs},
						{11, 2, btb_int_k2_regs,
							 btb_prty_k2_regs } } },
	{"pbf", pbf_int_attn_desc, pbf_prty_attn_desc, {
							{1, 2,
							 pbf_int_bb_a0_regs,
							 pbf_prty_bb_a0_regs},
							{1, 3,
							 pbf_int_bb_b0_regs,
							 pbf_prty_bb_b0_regs},
							{1, 3, pbf_int_k2_regs,
							 pbf_prty_k2_regs } } },
	{"rdif", rdif_int_attn_desc, rdif_prty_attn_desc, {
							   {1, 0,
					    rdif_int_bb_a0_regs,
							    OSAL_NULL},
							   {1, 1,
					    rdif_int_bb_b0_regs,

					    rdif_prty_bb_b0_regs},
							   {1, 1,
							    rdif_int_k2_regs,

					    rdif_prty_k2_regs } } },
	{"tdif", tdif_int_attn_desc, tdif_prty_attn_desc, {
							   {1, 1,
					    tdif_int_bb_a0_regs,

					    tdif_prty_bb_a0_regs},
							   {1, 2,
					    tdif_int_bb_b0_regs,

					    tdif_prty_bb_b0_regs},
							   {1, 2,
					    tdif_int_k2_regs,

					    tdif_prty_k2_regs } } },
	{"cdu", cdu_int_attn_desc, cdu_prty_attn_desc, {
							{1, 1,
							 cdu_int_bb_a0_regs,
							 cdu_prty_bb_a0_regs},
							{1, 1,
							 cdu_int_bb_b0_regs,
							 cdu_prty_bb_b0_regs},
					{1, 1, cdu_int_k2_regs,
							 cdu_prty_k2_regs } } },
	{"ccfc", ccfc_int_attn_desc, ccfc_prty_attn_desc, {
							   {1, 2,
					    ccfc_int_bb_a0_regs,

					    ccfc_prty_bb_a0_regs},
							   {1, 2,
					    ccfc_int_bb_b0_regs,

					    ccfc_prty_bb_b0_regs},
							   {1, 2,
					    ccfc_int_k2_regs,

					    ccfc_prty_k2_regs } } },
	{"tcfc", tcfc_int_attn_desc, tcfc_prty_attn_desc, {
							   {1, 2,
					    tcfc_int_bb_a0_regs,

					    tcfc_prty_bb_a0_regs},
							   {1, 2,
					    tcfc_int_bb_b0_regs,

					    tcfc_prty_bb_b0_regs},
							   {1, 2,
					    tcfc_int_k2_regs,

					    tcfc_prty_k2_regs } } },
	{"igu", igu_int_attn_desc, igu_prty_attn_desc, {
							{1, 3,
							 igu_int_bb_a0_regs,
							 igu_prty_bb_a0_regs},
							{1, 3,
							 igu_int_bb_b0_regs,
							 igu_prty_bb_b0_regs},
							{1, 2, igu_int_k2_regs,
							 igu_prty_k2_regs } } },
	{"cau", cau_int_attn_desc, cau_prty_attn_desc, {
							{1, 1,
							 cau_int_bb_a0_regs,
							 cau_prty_bb_a0_regs},
							{1, 1,
							 cau_int_bb_b0_regs,
							 cau_prty_bb_b0_regs},
							{1, 1, cau_int_k2_regs,
							 cau_prty_k2_regs } } },
	{"umac", umac_int_attn_desc, OSAL_NULL, {
						 {0, 0, OSAL_NULL, OSAL_NULL},
						 {0, 0, OSAL_NULL, OSAL_NULL},
						 {1, 0, umac_int_k2_regs,
						  OSAL_NULL } } },
	{"xmac", OSAL_NULL, OSAL_NULL, {
					{0, 0, OSAL_NULL, OSAL_NULL},
					{0, 0, OSAL_NULL, OSAL_NULL},
					{0, 0, OSAL_NULL, OSAL_NULL } } },
	{"dbg", dbg_int_attn_desc, dbg_prty_attn_desc, {
							{1, 1,
							 dbg_int_bb_a0_regs,
							 dbg_prty_bb_a0_regs},
							{1, 1,
							 dbg_int_bb_b0_regs,
							 dbg_prty_bb_b0_regs},
							{1, 1, dbg_int_k2_regs,
							 dbg_prty_k2_regs } } },
	{"nig", nig_int_attn_desc, nig_prty_attn_desc, {
							{6, 4,
							 nig_int_bb_a0_regs,
							 nig_prty_bb_a0_regs},
							{6, 5,
							 nig_int_bb_b0_regs,
							 nig_prty_bb_b0_regs},
					{10, 5, nig_int_k2_regs,
							 nig_prty_k2_regs } } },
	{"wol", wol_int_attn_desc, wol_prty_attn_desc, {
							{0, 0, OSAL_NULL,
							 OSAL_NULL},
							{0, 0, OSAL_NULL,
							 OSAL_NULL},
							{1, 1, wol_int_k2_regs,
							 wol_prty_k2_regs } } },
	{"bmbn", bmbn_int_attn_desc, OSAL_NULL, {
						 {0, 0, OSAL_NULL, OSAL_NULL},
						 {0, 0, OSAL_NULL, OSAL_NULL},
						 {1, 0, bmbn_int_k2_regs,
						  OSAL_NULL } } },
	{"ipc", ipc_int_attn_desc, ipc_prty_attn_desc, {
							{1, 1,
							 ipc_int_bb_a0_regs,
							 ipc_prty_bb_a0_regs},
							{1, 1,
							 ipc_int_bb_b0_regs,
							 ipc_prty_bb_b0_regs},
							{1, 0, ipc_int_k2_regs,
							 OSAL_NULL } } },
	{"nwm", nwm_int_attn_desc, nwm_prty_attn_desc, {
							{0, 0, OSAL_NULL,
							 OSAL_NULL},
							{0, 0, OSAL_NULL,
							 OSAL_NULL},
							{1, 3, nwm_int_k2_regs,
							 nwm_prty_k2_regs } } },
	{"nws", nws_int_attn_desc, nws_prty_attn_desc, {
							{0, 0, OSAL_NULL,
							 OSAL_NULL},
							{0, 0, OSAL_NULL,
							 OSAL_NULL},
							{4, 1, nws_int_k2_regs,
							 nws_prty_k2_regs } } },
	{"ms", ms_int_attn_desc, OSAL_NULL, {
					     {0, 0, OSAL_NULL, OSAL_NULL},
					     {0, 0, OSAL_NULL, OSAL_NULL},
					     {1, 0, ms_int_k2_regs,
					      OSAL_NULL } } },
	{"phy_pcie", OSAL_NULL, OSAL_NULL, {
					    {0, 0, OSAL_NULL, OSAL_NULL},
					    {0, 0, OSAL_NULL, OSAL_NULL},
					    {0, 0, OSAL_NULL, OSAL_NULL } } },
	{"misc_aeu", OSAL_NULL, OSAL_NULL, {
					    {0, 0, OSAL_NULL, OSAL_NULL},
					    {0, 0, OSAL_NULL, OSAL_NULL},
					    {0, 0, OSAL_NULL, OSAL_NULL } } },
	{"bar0_map", OSAL_NULL, OSAL_NULL, {
					    {0, 0, OSAL_NULL, OSAL_NULL},
					    {0, 0, OSAL_NULL, OSAL_NULL},
					    {0, 0, OSAL_NULL, OSAL_NULL } } },
};

#define NUM_INT_REGS 423
#define NUM_PRTY_REGS 378

#endif /* __PREVENT_INT_ATTN__ */

#endif /* __ATTN_VALUES_H__ */
