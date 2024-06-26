/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2024 Intel Corporation
 */

#ifndef _ICE_PHY_REGS_H_
#define _ICE_PHY_REGS_H_

#define CLKRX_CMN_CLK(i)	(0x7E8000 + (i) * 0x5000)
#define CLKRX_CMN_CLK_NUM	5

#define CLKRX_CMN_REG_10(i)	(CLKRX_CMN_CLK(i) + 0x28)
union clkrx_cmn_reg_10 {
	struct {
		u32 cmnntl_refck_pdmtchval : 19;
		u32 cmnntl_refckm_charge_up_locovr : 1;
		u32 cmnntl_refckm_pull_dn_locovr : 1;
		u32 cmnntl_refckm_sense_locovr : 1;
		u32 cmnntl_refckp_charge_up_locovr : 1;
		u32 cmnntl_refckp_pull_dn_locovr : 1;
		u32 cmnntl_refckp_sense_locovr : 1;
		u32 cmnpmu_h8_off_delay : 4;
		u32 cmnref_locovren : 1;
		u32 cmnref_pad2cmos_ana_en_locovr : 1;
		u32 cmnref_pad2cmos_dig_en_locovr : 1;
	} field;
	u32 val;
};

#define CLKRX_CMN_REG_12(i)	(CLKRX_CMN_CLK(i) + 0x30)
union clkrx_cmn_reg_12 {
	struct {
		u32 cmnpmu_restore_off_delay : 4;
		u32 cmnpmu_rst_off_delay : 4;
		u32 cmnref_cdrdivsel_locovr : 5;
		u32 cmnref_refsel0_locovr : 4;
		u32 cmnref_refsel1_locovr : 4;
		u32 cmnref_refsel1_powersave_en_locovr : 1;
		u32 cmnref_refsel2_locovr : 4;
		u32 cmnref_refsel2_powersave_en_locovr : 1;
		u32 cmnref_refsel3_locovr : 4;
		u32 cmnref_refsel3_powersave_en_locovr : 1;
	} field;
	u32 val;
};

#define CLKRX_CMN_REG_46(i)	(CLKRX_CMN_CLK(i) + 0x220)
union clkrx_cmn_reg_46 {
	struct {
		u32 cmnntl_refck_lkgcnt : 19;
		u32 cmnref_refsel0_loc : 4;
		u32 cmnref_refsel1_loc : 4;
		u32 cmnref_refsel1_powersave_en_loc : 1;
		u32 cmnref_refsel2_loc : 4;
	} field;
	u32 val;
};

#define SERDES_IP_IF_LN_FLXM_GENERAL(n, m) \
	(0x32B800 + (m) * 0x100000 + (n) * 0x8000)
union serdes_ip_if_ln_flxm_general {
	struct {
		u32 reserved0_1 : 2;
		u32 ictl_pcs_mode_nt : 1;
		u32 ictl_pcs_rcomp_slave_en_nt : 1;
		u32 ictl_pcs_cmn_force_pup_a : 1;
		u32 ictl_pcs_rcomp_slave_valid_a : 1;
		u32 ictl_pcs_ref_sel_rx_nt : 4;
		u32 idat_dfx_obs_dig_ : 2;
		u32 irst_apb_mem_b : 1;
		u32 ictl_pcs_disconnect_nt : 1;
		u32 ictl_pcs_isolate_nt : 1;
		u32 reserved15_15 : 1;
		u32 irst_pcs_tstbus_b_a : 1;
		u32 ictl_pcs_ref_term_hiz_en_nt : 1;
		u32 reserved18_19 : 2;
		u32 ictl_pcs_synthlcslow_force_pup_a : 1;
		u32 ictl_pcs_synthlcfast_force_pup_a : 1;
		u32 reserved22_24 : 3;
		u32 ictl_pcs_ref_sel_tx_nt : 4;
		u32 reserved29_31 : 3;
	} field;
	u32 val;
};
#endif /* _ICE_PHY_REGS_H_ */
