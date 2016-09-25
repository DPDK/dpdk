/*******************************************************************************

Copyright (c) 2001-2015, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

 3. Neither the name of the Intel Corporation nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

***************************************************************************/

#ifndef _IXGBE_PHY_H_
#define _IXGBE_PHY_H_

#include "ixgbe_type.h"
#define IXGBE_I2C_EEPROM_DEV_ADDR	0xA0
#define IXGBE_I2C_EEPROM_DEV_ADDR2	0xA2
#define IXGBE_I2C_EEPROM_BANK_LEN	0xFF

/* EEPROM byte offsets */
#define IXGBE_SFF_IDENTIFIER		0x0
#define IXGBE_SFF_IDENTIFIER_SFP	0x3
#define IXGBE_SFF_VENDOR_OUI_BYTE0	0x25
#define IXGBE_SFF_VENDOR_OUI_BYTE1	0x26
#define IXGBE_SFF_VENDOR_OUI_BYTE2	0x27
#define IXGBE_SFF_1GBE_COMP_CODES	0x6
#define IXGBE_SFF_10GBE_COMP_CODES	0x3
#define IXGBE_SFF_CABLE_TECHNOLOGY	0x8
#define IXGBE_SFF_CABLE_SPEC_COMP	0x3C
#define IXGBE_SFF_SFF_8472_SWAP		0x5C
#define IXGBE_SFF_SFF_8472_COMP		0x5E
#define IXGBE_SFF_SFF_8472_OSCB		0x6E
#define IXGBE_SFF_SFF_8472_ESCB		0x76
#define IXGBE_SFF_IDENTIFIER_QSFP_PLUS	0xD
#define IXGBE_SFF_QSFP_VENDOR_OUI_BYTE0	0xA5
#define IXGBE_SFF_QSFP_VENDOR_OUI_BYTE1	0xA6
#define IXGBE_SFF_QSFP_VENDOR_OUI_BYTE2	0xA7
#define IXGBE_SFF_QSFP_CONNECTOR	0x82
#define IXGBE_SFF_QSFP_10GBE_COMP	0x83
#define IXGBE_SFF_QSFP_1GBE_COMP	0x86
#define IXGBE_SFF_QSFP_CABLE_LENGTH	0x92
#define IXGBE_SFF_QSFP_DEVICE_TECH	0x93

/* Bitmasks */
#define IXGBE_SFF_DA_PASSIVE_CABLE	0x4
#define IXGBE_SFF_DA_ACTIVE_CABLE	0x8
#define IXGBE_SFF_DA_SPEC_ACTIVE_LIMITING	0x4
#define IXGBE_SFF_1GBASESX_CAPABLE	0x1
#define IXGBE_SFF_1GBASELX_CAPABLE	0x2
#define IXGBE_SFF_1GBASET_CAPABLE	0x8
#define IXGBE_SFF_10GBASESR_CAPABLE	0x10
#define IXGBE_SFF_10GBASELR_CAPABLE	0x20
#define IXGBE_SFF_SOFT_RS_SELECT_MASK	0x8
#define IXGBE_SFF_SOFT_RS_SELECT_10G	0x8
#define IXGBE_SFF_SOFT_RS_SELECT_1G	0x0
#define IXGBE_SFF_ADDRESSING_MODE	0x4
#define IXGBE_SFF_QSFP_DA_ACTIVE_CABLE	0x1
#define IXGBE_SFF_QSFP_DA_PASSIVE_CABLE	0x8
#define IXGBE_SFF_QSFP_CONNECTOR_NOT_SEPARABLE	0x23
#define IXGBE_SFF_QSFP_TRANSMITER_850NM_VCSEL	0x0
#define IXGBE_I2C_EEPROM_READ_MASK	0x100
#define IXGBE_I2C_EEPROM_STATUS_MASK	0x3
#define IXGBE_I2C_EEPROM_STATUS_NO_OPERATION	0x0
#define IXGBE_I2C_EEPROM_STATUS_PASS	0x1
#define IXGBE_I2C_EEPROM_STATUS_FAIL	0x2
#define IXGBE_I2C_EEPROM_STATUS_IN_PROGRESS	0x3

#define IXGBE_CS4227			0xBE	/* CS4227 address */
#define IXGBE_CS4227_GLOBAL_ID_LSB	0
#define IXGBE_CS4227_GLOBAL_ID_MSB	1
#define IXGBE_CS4227_SCRATCH		2
#define IXGBE_CS4227_GLOBAL_ID_VALUE	0x03E5
#define IXGBE_CS4223_PHY_ID		0x7003	/* Quad port */
#define IXGBE_CS4227_PHY_ID		0x3003	/* Dual port */
#define IXGBE_CS4227_RESET_PENDING	0x1357
#define IXGBE_CS4227_RESET_COMPLETE	0x5AA5
#define IXGBE_CS4227_RETRIES		15
#define IXGBE_CS4227_EFUSE_STATUS	0x0181
#define IXGBE_CS4227_LINE_SPARE22_MSB	0x12AD	/* Reg to program speed */
#define IXGBE_CS4227_LINE_SPARE24_LSB	0x12B0	/* Reg to program EDC */
#define IXGBE_CS4227_HOST_SPARE22_MSB	0x1AAD	/* Reg to program speed */
#define IXGBE_CS4227_HOST_SPARE24_LSB	0x1AB0	/* Reg to program EDC */
#define IXGBE_CS4227_EEPROM_STATUS	0x5001
#define IXGBE_CS4227_EEPROM_LOAD_OK	0x0001
#define IXGBE_CS4227_SPEED_1G		0x8000
#define IXGBE_CS4227_SPEED_10G		0
#define IXGBE_CS4227_EDC_MODE_CX1	0x0002
#define IXGBE_CS4227_EDC_MODE_SR	0x0004
#define IXGBE_CS4227_EDC_MODE_DIAG	0x0008
#define IXGBE_CS4227_RESET_HOLD		500	/* microseconds */
#define IXGBE_CS4227_RESET_DELAY	450	/* milliseconds */
#define IXGBE_CS4227_CHECK_DELAY	30	/* milliseconds */
#define IXGBE_PE			0xE0	/* Port expander address */
#define IXGBE_PE_OUTPUT			1	/* Output register offset */
#define IXGBE_PE_CONFIG			3	/* Config register offset */
#define IXGBE_PE_BIT1			(1 << 1)

/* Flow control defines */
#define IXGBE_TAF_SYM_PAUSE		0x400
#define IXGBE_TAF_ASM_PAUSE		0x800

/* Bit-shift macros */
#define IXGBE_SFF_VENDOR_OUI_BYTE0_SHIFT	24
#define IXGBE_SFF_VENDOR_OUI_BYTE1_SHIFT	16
#define IXGBE_SFF_VENDOR_OUI_BYTE2_SHIFT	8

/* Vendor OUIs: format of OUI is 0x[byte0][byte1][byte2][00] */
#define IXGBE_SFF_VENDOR_OUI_TYCO	0x00407600
#define IXGBE_SFF_VENDOR_OUI_FTL	0x00906500
#define IXGBE_SFF_VENDOR_OUI_AVAGO	0x00176A00
#define IXGBE_SFF_VENDOR_OUI_INTEL	0x001B2100

/* I2C SDA and SCL timing parameters for standard mode */
#define IXGBE_I2C_T_HD_STA	4
#define IXGBE_I2C_T_LOW		5
#define IXGBE_I2C_T_HIGH	4
#define IXGBE_I2C_T_SU_STA	5
#define IXGBE_I2C_T_HD_DATA	5
#define IXGBE_I2C_T_SU_DATA	1
#define IXGBE_I2C_T_RISE	1
#define IXGBE_I2C_T_FALL	1
#define IXGBE_I2C_T_SU_STO	4
#define IXGBE_I2C_T_BUF		5

#ifndef IXGBE_SFP_DETECT_RETRIES
#define IXGBE_SFP_DETECT_RETRIES	10

#endif /* IXGBE_SFP_DETECT_RETRIES */
#define IXGBE_TN_LASI_STATUS_REG	0x9005
#define IXGBE_TN_LASI_STATUS_TEMP_ALARM	0x0008

/* SFP+ SFF-8472 Compliance */
#define IXGBE_SFF_SFF_8472_UNSUP	0x00

/* More phy definitions */
#define IXGBE_M88E1500_COPPER_CTRL		0	/* Page 0 reg */
#define IXGBE_M88E1500_COPPER_CTRL_RESET	(1u << 15)
#define IXGBE_M88E1500_COPPER_CTRL_AN_EN	(1u << 12)
#define IXGBE_M88E1500_COPPER_CTRL_POWER_DOWN	(1u << 11)
#define IXGBE_M88E1500_COPPER_CTRL_RESTART_AN	(1u << 9)
#define IXGBE_M88E1500_COPPER_CTRL_FULL_DUPLEX	(1u << 8)
#define IXGBE_M88E1500_COPPER_CTRL_SPEED_MSB	(1u << 6)
#define IXGBE_M88E1500_COPPER_STATUS		1	/* Page 0 reg */
#define IXGBE_M88E1500_COPPER_STATUS_AN_DONE	(1u << 5)
#define IXGBE_M88E1500_COPPER_AN		4	/* Page 0 reg */
#define IXGBE_M88E1500_COPPER_AN_AS_PAUSE	(1u << 11)
#define IXGBE_M88E1500_COPPER_AN_PAUSE		(1u << 10)
#define IXGBE_M88E1500_COPPER_AN_T4		(1u << 9)
#define IXGBE_M88E1500_COPPER_AN_100TX_FD	(1u << 8)
#define IXGBE_M88E1500_COPPER_AN_100TX_HD	(1u << 7)
#define IXGBE_M88E1500_COPPER_AN_10TX_FD	(1u << 6)
#define IXGBE_M88E1500_COPPER_AN_10TX_HD	(1u << 5)
#define IXGBE_M88E1500_COPPER_AN_LP_ABILITY	5	/* Page 0 reg */
#define IXGBE_M88E1500_COPPER_AN_LP_AS_PAUSE	(1u << 11)
#define IXGBE_M88E1500_COPPER_AN_LP_PAUSE	(1u << 10)
#define IXGBE_M88E1500_1000T_CTRL		9	/* Page 0 reg */
/* 1=Configure PHY as Master 0=Configure PHY as Slave */
#define IXGBE_M88E1500_1000T_CTRL_MS_VALUE	(1u << 11)
#define IXGBE_M88E1500_1000T_CTRL_1G_FD		(1u << 9)
/* 1=Master/Slave manual config value 0=Automatic Master/Slave config */
#define IXGBE_M88E1500_1000T_CTRL_MS_ENABLE	(1u << 12)
#define IXGBE_M88E1500_1000T_CTRL_FULL_DUPLEX	(1u << 9)
#define IXGBE_M88E1500_1000T_CTRL_HALF_DUPLEX	(1u << 8)
#define IXGBE_M88E1500_1000T_STATUS		10	/* Page 0 reg */
#define IXGBE_M88E1500_AUTO_COPPER_SGMII	0x2
#define IXGBE_M88E1500_AUTO_COPPER_BASEX	0x3
#define IXGBE_M88E1500_STATUS_LINK		(1u << 2) /* Interface Link Bit */
#define IXGBE_M88E1500_MAC_CTRL_1		16	/* Page 0 reg */
#define IXGBE_M88E1500_MAC_CTRL_1_MODE_MASK	0x0380 /* Mode Select */
#define IXGBE_M88E1500_MAC_CTRL_1_DWN_SHIFT	12
#define IXGBE_M88E1500_MAC_CTRL_1_DWN_4X	3u
#define IXGBE_M88E1500_MAC_CTRL_1_ED_SHIFT	8
#define IXGBE_M88E1500_MAC_CTRL_1_ED_TM		3u
#define IXGBE_M88E1500_MAC_CTRL_1_MDIX_SHIFT	5
#define IXGBE_M88E1500_MAC_CTRL_1_MDIX_AUTO	3u
#define IXGBE_M88E1500_MAC_CTRL_1_POWER_DOWN	(1u << 2)
#define IXGBE_M88E1500_PHY_SPEC_STATUS		17	/* Page 0 reg */
#define IXGBE_M88E1500_PHY_SPEC_STATUS_SPEED_SHIFT	14
#define IXGBE_M88E1500_PHY_SPEC_STATUS_SPEED_MASK	3u
#define IXGBE_M88E1500_PHY_SPEC_STATUS_SPEED_10		0u
#define IXGBE_M88E1500_PHY_SPEC_STATUS_SPEED_100	1u
#define IXGBE_M88E1500_PHY_SPEC_STATUS_SPEED_1000	2u
#define IXGBE_M88E1500_PHY_SPEC_STATUS_DUPLEX		(1u << 13)
#define IXGBE_M88E1500_PHY_SPEC_STATUS_RESOLVED		(1u << 11)
#define IXGBE_M88E1500_PHY_SPEC_STATUS_LINK		(1u << 10)
#define IXGBE_M88E1500_PAGE_ADDR		22	/* All pages reg */
#define IXGBE_M88E1500_FIBER_CTRL		0	/* Page 1 reg */
#define IXGBE_M88E1500_FIBER_CTRL_RESET		(1u << 15)
#define IXGBE_M88E1500_FIBER_CTRL_SPEED_LSB	(1u << 13)
#define IXGBE_M88E1500_FIBER_CTRL_AN_EN		(1u << 12)
#define IXGBE_M88E1500_FIBER_CTRL_POWER_DOWN	(1u << 11)
#define IXGBE_M88E1500_FIBER_CTRL_DUPLEX_FULL	(1u << 8)
#define IXGBE_M88E1500_FIBER_CTRL_SPEED_MSB	(1u << 6)
#define IXGBE_M88E1500_MAC_SPEC_CTRL		16	/* Page 2 reg */
#define IXGBE_M88E1500_MAC_SPEC_CTRL_POWER_DOWN	(1u << 3)
#define IXGBE_M88E1500_EEE_CTRL_1		0	/* Page 18 reg */
#define IXGBE_M88E1500_EEE_CTRL_1_MS		(1u << 0) /* EEE Master/Slave */
#define IXGBE_M88E1500_GEN_CTRL			20	/* Page 18 reg */
#define IXGBE_M88E1500_GEN_CTRL_RESET		(1u << 15)
#define IXGBE_M88E1500_GEN_CTRL_MODE_SGMII_COPPER	1u /* Mode bits 0-2 */

s32 ixgbe_init_phy_ops_generic(struct ixgbe_hw *hw);
bool ixgbe_validate_phy_addr(struct ixgbe_hw *hw, u32 phy_addr);
enum ixgbe_phy_type ixgbe_get_phy_type_from_id(u32 phy_id);
s32 ixgbe_get_phy_id(struct ixgbe_hw *hw);
s32 ixgbe_identify_phy_generic(struct ixgbe_hw *hw);
s32 ixgbe_reset_phy_generic(struct ixgbe_hw *hw);
s32 ixgbe_read_phy_reg_mdi(struct ixgbe_hw *hw, u32 reg_addr, u32 device_type,
			   u16 *phy_data);
s32 ixgbe_write_phy_reg_mdi(struct ixgbe_hw *hw, u32 reg_addr, u32 device_type,
			    u16 phy_data);
s32 ixgbe_read_phy_reg_generic(struct ixgbe_hw *hw, u32 reg_addr,
			       u32 device_type, u16 *phy_data);
s32 ixgbe_write_phy_reg_generic(struct ixgbe_hw *hw, u32 reg_addr,
				u32 device_type, u16 phy_data);
s32 ixgbe_setup_phy_link_generic(struct ixgbe_hw *hw);
s32 ixgbe_setup_phy_link_speed_generic(struct ixgbe_hw *hw,
				       ixgbe_link_speed speed,
				       bool autoneg_wait_to_complete);
s32 ixgbe_get_copper_link_capabilities_generic(struct ixgbe_hw *hw,
					       ixgbe_link_speed *speed,
					       bool *autoneg);
s32 ixgbe_check_reset_blocked(struct ixgbe_hw *hw);

/* PHY specific */
s32 ixgbe_check_phy_link_tnx(struct ixgbe_hw *hw,
			     ixgbe_link_speed *speed,
			     bool *link_up);
s32 ixgbe_setup_phy_link_tnx(struct ixgbe_hw *hw);
s32 ixgbe_get_phy_firmware_version_tnx(struct ixgbe_hw *hw,
				       u16 *firmware_version);
s32 ixgbe_get_phy_firmware_version_generic(struct ixgbe_hw *hw,
					   u16 *firmware_version);

s32 ixgbe_reset_phy_nl(struct ixgbe_hw *hw);
s32 ixgbe_set_copper_phy_power(struct ixgbe_hw *hw, bool on);
s32 ixgbe_identify_module_generic(struct ixgbe_hw *hw);
s32 ixgbe_identify_sfp_module_generic(struct ixgbe_hw *hw);
s32 ixgbe_get_supported_phy_sfp_layer_generic(struct ixgbe_hw *hw);
s32 ixgbe_identify_qsfp_module_generic(struct ixgbe_hw *hw);
s32 ixgbe_get_sfp_init_sequence_offsets(struct ixgbe_hw *hw,
					u16 *list_offset,
					u16 *data_offset);
s32 ixgbe_tn_check_overtemp(struct ixgbe_hw *hw);
s32 ixgbe_read_i2c_byte_generic(struct ixgbe_hw *hw, u8 byte_offset,
				u8 dev_addr, u8 *data);
s32 ixgbe_read_i2c_byte_generic_unlocked(struct ixgbe_hw *hw, u8 byte_offset,
					 u8 dev_addr, u8 *data);
s32 ixgbe_write_i2c_byte_generic(struct ixgbe_hw *hw, u8 byte_offset,
				 u8 dev_addr, u8 data);
s32 ixgbe_write_i2c_byte_generic_unlocked(struct ixgbe_hw *hw, u8 byte_offset,
					  u8 dev_addr, u8 data);
s32 ixgbe_read_i2c_eeprom_generic(struct ixgbe_hw *hw, u8 byte_offset,
				  u8 *eeprom_data);
s32 ixgbe_write_i2c_eeprom_generic(struct ixgbe_hw *hw, u8 byte_offset,
				   u8 eeprom_data);
void ixgbe_i2c_bus_clear(struct ixgbe_hw *hw);
s32 ixgbe_read_i2c_combined_generic_int(struct ixgbe_hw *, u8 addr, u16 reg,
					u16 *val, bool lock);
s32 ixgbe_write_i2c_combined_generic_int(struct ixgbe_hw *, u8 addr, u16 reg,
					 u16 val, bool lock);
#endif /* _IXGBE_PHY_H_ */
