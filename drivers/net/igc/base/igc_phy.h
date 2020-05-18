/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2020 Intel Corporation
 */

#ifndef _IGC_PHY_H_
#define _IGC_PHY_H_

void igc_init_phy_ops_generic(struct igc_hw *hw);
s32  igc_null_read_reg(struct igc_hw *hw, u32 offset, u16 *data);
void igc_null_phy_generic(struct igc_hw *hw);
s32  igc_null_lplu_state(struct igc_hw *hw, bool active);
s32  igc_null_write_reg(struct igc_hw *hw, u32 offset, u16 data);
s32  igc_null_set_page(struct igc_hw *hw, u16 data);
s32 igc_read_i2c_byte_null(struct igc_hw *hw, u8 byte_offset,
			     u8 dev_addr, u8 *data);
s32 igc_write_i2c_byte_null(struct igc_hw *hw, u8 byte_offset,
			      u8 dev_addr, u8 data);
s32  igc_check_downshift_generic(struct igc_hw *hw);
s32  igc_check_polarity_m88(struct igc_hw *hw);
s32  igc_check_polarity_igp(struct igc_hw *hw);
s32  igc_check_polarity_ife(struct igc_hw *hw);
s32  igc_check_reset_block_generic(struct igc_hw *hw);
s32  igc_phy_setup_autoneg(struct igc_hw *hw);
s32  igc_copper_link_autoneg(struct igc_hw *hw);
s32  igc_copper_link_setup_igp(struct igc_hw *hw);
s32  igc_copper_link_setup_m88(struct igc_hw *hw);
s32  igc_copper_link_setup_m88_gen2(struct igc_hw *hw);
s32  igc_phy_force_speed_duplex_igp(struct igc_hw *hw);
s32  igc_phy_force_speed_duplex_m88(struct igc_hw *hw);
s32  igc_phy_force_speed_duplex_ife(struct igc_hw *hw);
s32  igc_get_cable_length_m88(struct igc_hw *hw);
s32  igc_get_cable_length_m88_gen2(struct igc_hw *hw);
s32  igc_get_cable_length_igp_2(struct igc_hw *hw);
s32  igc_get_cfg_done_generic(struct igc_hw *hw);
s32  igc_get_phy_id(struct igc_hw *hw);
s32  igc_get_phy_info_igp(struct igc_hw *hw);
s32  igc_get_phy_info_m88(struct igc_hw *hw);
s32  igc_get_phy_info_ife(struct igc_hw *hw);
s32  igc_phy_sw_reset_generic(struct igc_hw *hw);
void igc_phy_force_speed_duplex_setup(struct igc_hw *hw, u16 *phy_ctrl);
s32  igc_phy_hw_reset_generic(struct igc_hw *hw);
s32  igc_phy_reset_dsp_generic(struct igc_hw *hw);
s32  igc_read_kmrn_reg_generic(struct igc_hw *hw, u32 offset, u16 *data);
s32  igc_read_kmrn_reg_locked(struct igc_hw *hw, u32 offset, u16 *data);
s32  igc_set_page_igp(struct igc_hw *hw, u16 page);
s32  igc_read_phy_reg_igp(struct igc_hw *hw, u32 offset, u16 *data);
s32  igc_read_phy_reg_igp_locked(struct igc_hw *hw, u32 offset, u16 *data);
s32  igc_read_phy_reg_m88(struct igc_hw *hw, u32 offset, u16 *data);
s32  igc_set_d3_lplu_state_generic(struct igc_hw *hw, bool active);
s32  igc_setup_copper_link_generic(struct igc_hw *hw);
s32  igc_write_kmrn_reg_generic(struct igc_hw *hw, u32 offset, u16 data);
s32  igc_write_kmrn_reg_locked(struct igc_hw *hw, u32 offset, u16 data);
s32  igc_write_phy_reg_igp(struct igc_hw *hw, u32 offset, u16 data);
s32  igc_write_phy_reg_igp_locked(struct igc_hw *hw, u32 offset, u16 data);
s32  igc_write_phy_reg_m88(struct igc_hw *hw, u32 offset, u16 data);
s32  igc_phy_has_link_generic(struct igc_hw *hw, u32 iterations,
				u32 usec_interval, bool *success);
s32  igc_phy_init_script_igp3(struct igc_hw *hw);
enum igc_phy_type igc_get_phy_type_from_id(u32 phy_id);
s32  igc_determine_phy_address(struct igc_hw *hw);
s32  igc_write_phy_reg_bm(struct igc_hw *hw, u32 offset, u16 data);
s32  igc_read_phy_reg_bm(struct igc_hw *hw, u32 offset, u16 *data);
s32  igc_enable_phy_wakeup_reg_access_bm(struct igc_hw *hw, u16 *phy_reg);
s32  igc_disable_phy_wakeup_reg_access_bm(struct igc_hw *hw, u16 *phy_reg);
s32  igc_read_phy_reg_bm2(struct igc_hw *hw, u32 offset, u16 *data);
s32  igc_write_phy_reg_bm2(struct igc_hw *hw, u32 offset, u16 data);
void igc_power_up_phy_copper(struct igc_hw *hw);
void igc_power_down_phy_copper(struct igc_hw *hw);
s32  igc_read_phy_reg_mdic(struct igc_hw *hw, u32 offset, u16 *data);
s32  igc_write_phy_reg_mdic(struct igc_hw *hw, u32 offset, u16 data);
s32  igc_read_phy_reg_i2c(struct igc_hw *hw, u32 offset, u16 *data);
s32  igc_write_phy_reg_i2c(struct igc_hw *hw, u32 offset, u16 data);
s32  igc_read_sfp_data_byte(struct igc_hw *hw, u16 offset, u8 *data);
s32  igc_write_sfp_data_byte(struct igc_hw *hw, u16 offset, u8 data);
s32  igc_read_phy_reg_hv(struct igc_hw *hw, u32 offset, u16 *data);
s32  igc_read_phy_reg_hv_locked(struct igc_hw *hw, u32 offset, u16 *data);
s32  igc_read_phy_reg_page_hv(struct igc_hw *hw, u32 offset, u16 *data);
s32  igc_write_phy_reg_hv(struct igc_hw *hw, u32 offset, u16 data);
s32  igc_write_phy_reg_hv_locked(struct igc_hw *hw, u32 offset, u16 data);
s32  igc_write_phy_reg_page_hv(struct igc_hw *hw, u32 offset, u16 data);
s32  igc_link_stall_workaround_hv(struct igc_hw *hw);
s32  igc_copper_link_setup_82577(struct igc_hw *hw);
s32  igc_check_polarity_82577(struct igc_hw *hw);
s32  igc_get_phy_info_82577(struct igc_hw *hw);
s32  igc_phy_force_speed_duplex_82577(struct igc_hw *hw);
s32  igc_get_cable_length_82577(struct igc_hw *hw);
s32  igc_write_phy_reg_gs40g(struct igc_hw *hw, u32 offset, u16 data);
s32  igc_read_phy_reg_gs40g(struct igc_hw *hw, u32 offset, u16 *data);
s32  igc_write_phy_reg_gpy(struct igc_hw *hw, u32 offset, u16 data);
s32  igc_read_phy_reg_gpy(struct igc_hw *hw, u32 offset, u16 *data);
s32 igc_read_phy_reg_mphy(struct igc_hw *hw, u32 address, u32 *data);
s32 igc_write_phy_reg_mphy(struct igc_hw *hw, u32 address, u32 data,
			     bool line_override);
bool igc_is_mphy_ready(struct igc_hw *hw);

s32 igc_read_xmdio_reg(struct igc_hw *hw, u16 addr, u8 dev_addr,
			 u16 *data);
s32 igc_write_xmdio_reg(struct igc_hw *hw, u16 addr, u8 dev_addr,
			  u16 data);

#define IGC_MAX_PHY_ADDR		8

/* IGP01E1000 Specific Registers */
#define IGP01IGC_PHY_PORT_CONFIG	0x10 /* Port Config */
#define IGP01IGC_PHY_PORT_STATUS	0x11 /* Status */
#define IGP01IGC_PHY_PORT_CTRL	0x12 /* Control */
#define IGP01IGC_PHY_LINK_HEALTH	0x13 /* PHY Link Health */
#define IGP01IGC_GMII_FIFO		0x14 /* GMII FIFO */
#define IGP02IGC_PHY_POWER_MGMT	0x19 /* Power Management */
#define IGP01IGC_PHY_PAGE_SELECT	0x1F /* Page Select */
#define BM_PHY_PAGE_SELECT		22   /* Page Select for BM */
#define IGP_PAGE_SHIFT			5
#define PHY_REG_MASK			0x1F

/* GS40G - I210 PHY defines */
#define GS40G_PAGE_SELECT		0x16
#define GS40G_PAGE_SHIFT		16
#define GS40G_OFFSET_MASK		0xFFFF
#define GS40G_PAGE_2			0x20000
#define GS40G_MAC_REG2			0x15
#define GS40G_MAC_LB			0x4140
#define GS40G_MAC_SPEED_1G		0X0006
#define GS40G_COPPER_SPEC		0x0010

#define IGC_I225_PHPM			0x0E14 /* I225 PHY Power Management */
#define IGC_I225_PHPM_DIS_1000_D3	0x0008 /* Disable 1G in D3 */
#define IGC_I225_PHPM_LINK_ENERGY	0x0010 /* Link Energy Detect */
#define IGC_I225_PHPM_GO_LINKD	0x0020 /* Go Link Disconnect */
#define IGC_I225_PHPM_DIS_1000	0x0040 /* Disable 1G globally */
#define IGC_I225_PHPM_SPD_B2B_EN	0x0080 /* Smart Power Down Back2Back */
#define IGC_I225_PHPM_RST_COMPL	0x0100 /* PHY Reset Completed */
#define IGC_I225_PHPM_DIS_100_D3	0x0200 /* Disable 100M in D3 */
#define IGC_I225_PHPM_ULP		0x0400 /* Ultra Low-Power Mode */
#define IGC_I225_PHPM_DIS_2500	0x0800 /* Disable 2.5G globally */
#define IGC_I225_PHPM_DIS_2500_D3	0x1000 /* Disable 2.5G in D3 */
/* GPY211 - I225 defines */
#define GPY_MMD_MASK			0xFFFF0000
#define GPY_MMD_SHIFT			16
#define GPY_REG_MASK			0x0000FFFF
/* BM/HV Specific Registers */
#define BM_PORT_CTRL_PAGE		769
#define BM_WUC_PAGE			800
#define BM_WUC_ADDRESS_OPCODE		0x11
#define BM_WUC_DATA_OPCODE		0x12
#define BM_WUC_ENABLE_PAGE		BM_PORT_CTRL_PAGE
#define BM_WUC_ENABLE_REG		17
#define BM_WUC_ENABLE_BIT		(1 << 2)
#define BM_WUC_HOST_WU_BIT		(1 << 4)
#define BM_WUC_ME_WU_BIT		(1 << 5)

#define PHY_UPPER_SHIFT			21

#define BM_PHY_REG(page, reg)	(	\
	__extension__ ({		\
		typeof(page) _page = (page);	\
		typeof(reg) _reg = (reg);	\
		(_reg & MAX_PHY_REG_ADDRESS) |	\
		((_page & 0xFFFF) << PHY_PAGE_SHIFT) |	\
		((_reg & ~MAX_PHY_REG_ADDRESS) <<	\
		(PHY_UPPER_SHIFT - PHY_PAGE_SHIFT));	\
	}))

#define BM_PHY_REG_PAGE(offset) \
	((u16)(((offset) >> PHY_PAGE_SHIFT) & 0xFFFF))

#define BM_PHY_REG_NUM(offset)	(	\
	__extension__ ({		\
		typeof(offset) _offset = (offset);	\
		(u16)((_offset & MAX_PHY_REG_ADDRESS) |	\
		((_offset >> (PHY_UPPER_SHIFT - PHY_PAGE_SHIFT)) &	\
		~MAX_PHY_REG_ADDRESS));			\
	}))

#define HV_INTC_FC_PAGE_START		768
#define I82578_ADDR_REG			29
#define I82577_ADDR_REG			16
#define I82577_CFG_REG			22
#define I82577_CFG_ASSERT_CRS_ON_TX	(1 << 15)
#define I82577_CFG_ENABLE_DOWNSHIFT	(3 << 10) /* auto downshift */
#define I82577_CTRL_REG			23

/* 82577 specific PHY registers */
#define I82577_PHY_CTRL_2		18
#define I82577_PHY_LBK_CTRL		19
#define I82577_PHY_STATUS_2		26
#define I82577_PHY_DIAG_STATUS		31

/* I82577 PHY Status 2 */
#define I82577_PHY_STATUS2_REV_POLARITY		0x0400
#define I82577_PHY_STATUS2_MDIX			0x0800
#define I82577_PHY_STATUS2_SPEED_MASK		0x0300
#define I82577_PHY_STATUS2_SPEED_1000MBPS	0x0200

/* I82577 PHY Control 2 */
#define I82577_PHY_CTRL2_MANUAL_MDIX		0x0200
#define I82577_PHY_CTRL2_AUTO_MDI_MDIX		0x0400
#define I82577_PHY_CTRL2_MDIX_CFG_MASK		0x0600

/* I82577 PHY Diagnostics Status */
#define I82577_DSTATUS_CABLE_LENGTH		0x03FC
#define I82577_DSTATUS_CABLE_LENGTH_SHIFT	2

/* 82580 PHY Power Management */
#define IGC_82580_PHY_POWER_MGMT	0xE14
#define IGC_82580_PM_SPD		0x0001 /* Smart Power Down */
#define IGC_82580_PM_D0_LPLU		0x0002 /* For D0a states */
#define IGC_82580_PM_D3_LPLU		0x0004 /* For all other states */
#define IGC_82580_PM_GO_LINKD		0x0020 /* Go Link Disconnect */

#define IGC_MPHY_DIS_ACCESS		0x80000000 /* disable_access bit */
#define IGC_MPHY_ENA_ACCESS		0x40000000 /* enable_access bit */
#define IGC_MPHY_BUSY			0x00010000 /* busy bit */
#define IGC_MPHY_ADDRESS_FNC_OVERRIDE	0x20000000 /* fnc_override bit */
#define IGC_MPHY_ADDRESS_MASK		0x0000FFFF /* address mask */

/* BM PHY Copper Specific Control 1 */
#define BM_CS_CTRL1			16

/* BM PHY Copper Specific Status */
#define BM_CS_STATUS			17
#define BM_CS_STATUS_LINK_UP		0x0400
#define BM_CS_STATUS_RESOLVED		0x0800
#define BM_CS_STATUS_SPEED_MASK		0xC000
#define BM_CS_STATUS_SPEED_1000		0x8000

/* 82577 Mobile Phy Status Register */
#define HV_M_STATUS			26
#define HV_M_STATUS_AUTONEG_COMPLETE	0x1000
#define HV_M_STATUS_SPEED_MASK		0x0300
#define HV_M_STATUS_SPEED_1000		0x0200
#define HV_M_STATUS_SPEED_100		0x0100
#define HV_M_STATUS_LINK_UP		0x0040

#define IGP01IGC_PHY_PCS_INIT_REG	0x00B4
#define IGP01IGC_PHY_POLARITY_MASK	0x0078

#define IGP01IGC_PSCR_AUTO_MDIX	0x1000
#define IGP01IGC_PSCR_FORCE_MDI_MDIX	0x2000 /* 0=MDI, 1=MDIX */

#define IGP01IGC_PSCFR_SMART_SPEED	0x0080

/* Enable flexible speed on link-up */
#define IGP01IGC_GMII_FLEX_SPD	0x0010
#define IGP01IGC_GMII_SPD		0x0020 /* Enable SPD */

#define IGP02IGC_PM_SPD		0x0001 /* Smart Power Down */
#define IGP02IGC_PM_D0_LPLU		0x0002 /* For D0a states */
#define IGP02IGC_PM_D3_LPLU		0x0004 /* For all other states */

#define IGP01IGC_PLHR_SS_DOWNGRADE	0x8000

#define IGP01IGC_PSSR_POLARITY_REVERSED	0x0002
#define IGP01IGC_PSSR_MDIX		0x0800
#define IGP01IGC_PSSR_SPEED_MASK	0xC000
#define IGP01IGC_PSSR_SPEED_1000MBPS	0xC000

#define IGP02IGC_PHY_CHANNEL_NUM	4
#define IGP02IGC_PHY_AGC_A		0x11B1
#define IGP02IGC_PHY_AGC_B		0x12B1
#define IGP02IGC_PHY_AGC_C		0x14B1
#define IGP02IGC_PHY_AGC_D		0x18B1

#define IGP02IGC_AGC_LENGTH_SHIFT	9   /* Course=15:13, Fine=12:9 */
#define IGP02IGC_AGC_LENGTH_MASK	0x7F
#define IGP02IGC_AGC_RANGE		15

#define IGC_CABLE_LENGTH_UNDEFINED	0xFF

#define IGC_KMRNCTRLSTA_OFFSET	0x001F0000
#define IGC_KMRNCTRLSTA_OFFSET_SHIFT	16
#define IGC_KMRNCTRLSTA_REN		0x00200000
#define IGC_KMRNCTRLSTA_CTRL_OFFSET	0x1    /* Kumeran Control */
#define IGC_KMRNCTRLSTA_DIAG_OFFSET	0x3    /* Kumeran Diagnostic */
#define IGC_KMRNCTRLSTA_TIMEOUTS	0x4    /* Kumeran Timeouts */
#define IGC_KMRNCTRLSTA_INBAND_PARAM	0x9    /* Kumeran InBand Parameters */
#define IGC_KMRNCTRLSTA_IBIST_DISABLE	0x0200 /* Kumeran IBIST Disable */
#define IGC_KMRNCTRLSTA_DIAG_NELPBK	0x1000 /* Nearend Loopback mode */
#define IGC_KMRNCTRLSTA_K1_CONFIG	0x7
#define IGC_KMRNCTRLSTA_K1_ENABLE	0x0002 /* enable K1 */
#define IGC_KMRNCTRLSTA_HD_CTRL	0x10   /* Kumeran HD Control */
#define IGC_KMRNCTRLSTA_K0S_CTRL	0x1E	/* Kumeran K0s Control */
#define IGC_KMRNCTRLSTA_K0S_CTRL_ENTRY_LTNCY_SHIFT	0
#define IGC_KMRNCTRLSTA_K0S_CTRL_MIN_TIME_SHIFT	4
#define IGC_KMRNCTRLSTA_K0S_CTRL_ENTRY_LTNCY_MASK	\
	(3 << IGC_KMRNCTRLSTA_K0S_CTRL_ENTRY_LTNCY_SHIFT)
#define IGC_KMRNCTRLSTA_K0S_CTRL_MIN_TIME_MASK \
	(7 << IGC_KMRNCTRLSTA_K0S_CTRL_MIN_TIME_SHIFT)
#define IGC_KMRNCTRLSTA_OP_MODES	0x1F   /* Kumeran Modes of Operation */
#define IGC_KMRNCTRLSTA_OP_MODES_LSC2CSC	0x0002 /* change LSC to CSC */

#define IFE_PHY_EXTENDED_STATUS_CONTROL	0x10
#define IFE_PHY_SPECIAL_CONTROL		0x11 /* 100BaseTx PHY Special Ctrl */
#define IFE_PHY_SPECIAL_CONTROL_LED	0x1B /* PHY Special and LED Ctrl */
#define IFE_PHY_MDIX_CONTROL		0x1C /* MDI/MDI-X Control */

/* IFE PHY Extended Status Control */
#define IFE_PESC_POLARITY_REVERSED	0x0100

/* IFE PHY Special Control */
#define IFE_PSC_AUTO_POLARITY_DISABLE	0x0010
#define IFE_PSC_FORCE_POLARITY		0x0020

/* IFE PHY Special Control and LED Control */
#define IFE_PSCL_PROBE_MODE		0x0020
#define IFE_PSCL_PROBE_LEDS_OFF		0x0006 /* Force LEDs 0 and 2 off */
#define IFE_PSCL_PROBE_LEDS_ON		0x0007 /* Force LEDs 0 and 2 on */

/* IFE PHY MDIX Control */
#define IFE_PMC_MDIX_STATUS		0x0020 /* 1=MDI-X, 0=MDI */
#define IFE_PMC_FORCE_MDIX		0x0040 /* 1=force MDI-X, 0=force MDI */
#define IFE_PMC_AUTO_MDIX		0x0080 /* 1=enable auto, 0=disable */

/* SFP modules ID memory locations */
#define IGC_SFF_IDENTIFIER_OFFSET	0x00
#define IGC_SFF_IDENTIFIER_SFF	0x02
#define IGC_SFF_IDENTIFIER_SFP	0x03

#define IGC_SFF_ETH_FLAGS_OFFSET	0x06
/* Flags for SFP modules compatible with ETH up to 1Gb */
struct sfp_igc_flags {
	u8 igc_base_sx:1;
	u8 igc_base_lx:1;
	u8 igc_base_cx:1;
	u8 igc_base_t:1;
	u8 e100_base_lx:1;
	u8 e100_base_fx:1;
	u8 e10_base_bx10:1;
	u8 e10_base_px:1;
};

/* Vendor OUIs: format of OUI is 0x[byte0][byte1][byte2][00] */
#define IGC_SFF_VENDOR_OUI_TYCO	0x00407600
#define IGC_SFF_VENDOR_OUI_FTL	0x00906500
#define IGC_SFF_VENDOR_OUI_AVAGO	0x00176A00
#define IGC_SFF_VENDOR_OUI_INTEL	0x001B2100

#endif
