/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#ifndef _TXGBE_PHY_H_
#define _TXGBE_PHY_H_

#include "txgbe_type.h"

#define TXGBE_SFP_DETECT_RETRIES	10
#define TXGBE_MD_COMMAND_TIMEOUT	100 /* PHY Timeout for 1 GB mode */


/* ETH PHY Registers */
#define SR_XS_PCS_MMD_STATUS1           0x030001
#define SR_XS_PCS_CTRL2                 0x030007
#define   SR_PCS_CTRL2_TYPE_SEL         MS16(0, 0x3)
#define   SR_PCS_CTRL2_TYPE_SEL_R       LS16(0, 0, 0x3)
#define   SR_PCS_CTRL2_TYPE_SEL_X       LS16(1, 0, 0x3)
#define   SR_PCS_CTRL2_TYPE_SEL_W       LS16(2, 0, 0x3)
#define SR_XS_PCS_KR_STS1		0x030020
#define   SR_XS_PCS_KR_STS1_PLU		MS16(12, 0x1)
#define SR_PMA_CTRL1                    0x010000
#define   SR_PMA_CTRL1_SS13             MS16(13, 0x1)
#define   SR_PMA_CTRL1_SS13_KX          LS16(0, 13, 0x1)
#define   SR_PMA_CTRL1_SS13_KX4         LS16(1, 13, 0x1)
#define   SR_PMA_CTRL1_LB               MS16(0, 0x1)
#define SR_PMA_KR_PMD_CTRL		0x010096
#define   SR_PMA_KR_PMD_CTRL_EN_TR	MS16(1, 0x1)
#define   SR_PMA_KR_PMD_CTRL_RS_TR	MS16(0, 0x1)
#define SR_PMA_KR_PMD_STS		0x010097
#define   SR_PMA_KR_PMD_STS_TR_FAIL	MS16(3, 0x1)
#define   SR_PMA_KR_PMD_STS_RCV		MS16(0, 0x1)
#define SR_PMA_KR_LP_CEU		0x010098
#define SR_PMA_KR_LP_CESTS		0x010099
#define   SR_PMA_KR_LP_CESTS_RR		MS16(15, 0x1)
#define SR_PMA_KR_LD_CEU		0x01009A
#define SR_PMA_KR_LD_CESTS		0x01009B
#define   SR_PMA_KR_LD_CESTS_RR		MS16(15, 0x1)
#define SR_PMA_KR_FEC_CTRL              0x0100AB
#define   SR_PMA_KR_FEC_CTRL_EN		MS16(0, 0x1)
#define SR_MII_MMD_CTL                  0x1F0000
#define   SR_MII_MMD_CTL_AN_EN              0x1000
#define   SR_MII_MMD_CTL_RESTART_AN         0x0200
#define SR_MII_MMD_DIGI_CTL             0x1F8000
#define SR_MII_MMD_AN_CTL               0x1F8001
#define SR_MII_MMD_AN_ADV               0x1F0004
#define   SR_MII_MMD_AN_ADV_PAUSE(v)    ((0x3 & (v)) << 7)
#define   SR_MII_MMD_AN_ADV_PAUSE_ASM   0x80
#define   SR_MII_MMD_AN_ADV_PAUSE_SYM   0x100
#define SR_MII_MMD_LP_BABL              0x1F0005

#define BP_TYPE_KX		0x20
#define BP_TYPE_KX4		0x40
#define BP_TYPE_KX4_KX		0x60
#define BP_TYPE_KR		0x80
#define BP_TYPE_KR_KX		0xA0
#define BP_TYPE_KR_KX4		0xC0
#define BP_TYPE_KR_KX4_KX	0xE0

#define SR_AN_CTRL                      0x070000
#define   SR_AN_CTRL_RSTRT_AN           MS16(9, 0x1)
#define   SR_AN_CTRL_AN_EN              MS16(12, 0x1)
#define   SR_AN_CTRL_EXT_NP             MS16(13, 0x1)
#define SR_AN_MMD_ADV_REG1                0x070010
#define   SR_AN_MMD_ADV_REG1_PAUSE(v)      ((0x3 & (v)) << 10)
#define   SR_AN_MMD_ADV_REG1_PAUSE_SYM      0x400
#define   SR_AN_MMD_ADV_REG1_PAUSE_ASM      0x800
#define   SR_AN_MMD_ADV_REG1_NP(v)	  RS16(v, 15, 0x1)
#define SR_AN_MMD_ADV_REG2		  0x070011
#define   SR_AN_MMD_ADV_REG2_BP_TYPE_KX4	BP_TYPE_KX4
#define   SR_AN_MMD_ADV_REG2_BP_TYPE_KX		BP_TYPE_KX
#define   SR_AN_MMD_ADV_REG2_BP_TYPE_KR		BP_TYPE_KR
#define   SR_AN_MMD_ADV_REG2_BP_TYPE_KX4_KX	BP_TYPE_KX4_KX
#define   SR_AN_MMD_ADV_REG2_BP_TYPE_KR_KX	BP_TYPE_KR_KX
#define   SR_AN_MMD_ADV_REG2_BP_TYPE_KR_KX4	BP_TYPE_KR_KX4
#define   SR_AN_MMD_ADV_REG2_BP_TYPE_KR_KX4_KX	BP_TYPE_KR_KX4_KX
#define   SR_AN_MMD_ADV_REG2_BP_TYPE_MASK	0xFFFF
#define SR_AN_MMD_ADV_REG3                0x070012
#define   SR_AN_MMD_ADV_REG3_FCE(v)	  RS16(v, 14, 0x3)
#define SR_AN_MMD_LP_ABL1                 0x070013
#define   SR_MMD_LP_ABL1_ADV_NP(v)	  RS16(v, 15, 0x1)
#define SR_AN_MMD_LP_ABL2		  0x070014
#define   SR_AN_MMD_LP_ABL2_BP_TYPE_KX4		BP_TYPE_KX4
#define   SR_AN_MMD_LP_ABL2_BP_TYPE_KX		BP_TYPE_KX
#define   SR_AN_MMD_LP_ABL2_BP_TYPE_KR		BP_TYPE_KR
#define   SR_AN_MMD_LP_ABL2_BP_TYPE_KX4_KX	BP_TYPE_KX4_KX
#define   SR_AN_MMD_LP_ABL2_BP_TYPE_KR_KX	BP_TYPE_KR_KX
#define   SR_AN_MMD_LP_ABL2_BP_TYPE_KR_KX4	BP_TYPE_KR_KX4
#define   SR_AN_MMD_LP_ABL2_BP_TYPE_KR_KX4_KX	BP_TYPE_KR_KX4_KX
#define   SR_AN_MMD_LP_ABL2_BP_TYPE_MASK	0xFFFF
#define SR_AN_MMD_LP_ABL3		  0x070015
#define   SR_AN_MMD_LP_ABL3_FCE(v)	  RS16(v, 14, 0x3)
#define SR_AN_XNP_TX1			  0x070016
#define   SR_AN_XNP_TX1_NP		  MS16(15, 0x1)
#define SR_AN_LP_XNP_ABL1		  0x070019
#define   SR_AN_LP_XNP_ABL1_NP(v)	  RS16(v, 15, 0x1)

#define VR_AN_INTR_MSK			  0x078001
#define   VR_AN_INTR_CMPLT_IE		  MS16(0, 0x1)
#define   VR_AN_INTR_LINK_IE		  MS16(1, 0x1)
#define   VR_AN_INTR_PG_RCV_IE		  MS16(2, 0x1)
#define VR_AN_INTR			  0x078002
#define   VR_AN_INTR_CMPLT		  MS16(0, 0x1)
#define   VR_AN_INTR_LINK		  MS16(1, 0x1)
#define   VR_AN_INTR_PG_RCV		  MS16(2, 0x1)
#define VR_AN_KR_MODE_CL                  0x078003
#define   VR_AN_KR_MODE_CL_PDET		  MS16(0, 0x1)
#define VR_XS_OR_PCS_MMD_DIGI_CTL1        0x038000
#define   VR_XS_OR_PCS_MMD_DIGI_CTL1_ENABLE 0x1000
#define   VR_XS_OR_PCS_MMD_DIGI_CTL1_VR_RST 0x8000
#define VR_XS_OR_PCS_MMD_DIGI_CTL2        0x038001
#define VR_XS_OR_PCS_MMD_DIGI_STATUS      0x038010
#define   VR_XS_OR_PCS_MMD_DIGI_STATUS_PSEQ_MASK            0x1C
#define   VR_XS_OR_PCS_MMD_DIGI_STATUS_PSEQ_POWER_GOOD      0x10
#define VR_PMA_KRTR_PRBS_CTRL0		  0x018003
#define   VR_PMA_KRTR_PRBS31_EN		  MS16(1, 0x1)
#define   VR_PMA_KRTR_PRBS_MODE_EN	  MS16(0, 0x1)
#define VR_PMA_KRTR_PRBS_CTRL1		  0x018004
#define   VR_PMA_KRTR_PRBS_TIME_LMT	  MS16(0, 0xFFFF)
#define VR_PMA_KRTR_PRBS_CTRL2		  0x018005
#define   VR_PMA_KRTR_PRBS_ERR_LIM	  MS16(0, 0x2FFF)
#define VR_PMA_KRTR_TIMER_CTRL0		  0x018006
#define   VR_PMA_KRTR_TIMER_MAX_WAIT	  MS16(0, 0xFFFF)
#define VR_PMA_KRTR_TIMER_CTRL2		  0x018008

#define TXGBE_PHY_MPLLA_CTL0                    0x018071
#define TXGBE_PHY_MPLLA_CTL3                    0x018077
#define TXGBE_PHY_MISC_CTL0                     0x018090
#define TXGBE_PHY_VCO_CAL_LD0                   0x018092
#define TXGBE_PHY_VCO_CAL_LD1                   0x018093
#define TXGBE_PHY_VCO_CAL_LD2                   0x018094
#define TXGBE_PHY_VCO_CAL_LD3                   0x018095
#define TXGBE_PHY_VCO_CAL_REF0                  0x018096
#define TXGBE_PHY_VCO_CAL_REF1                  0x018097
#define TXGBE_PHY_RX_AD_ACK                     0x018098
#define TXGBE_PHY_AFE_DFE_ENABLE                0x01805D
#define TXGBE_PHY_DFE_TAP_CTL0                  0x01805E
#define TXGBE_PHY_RX_EQ_ATT_LVL0                0x018057
#define TXGBE_PHY_RX_EQ_CTL0                    0x018058
#define TXGBE_PHY_RX_EQ_CTL                     0x01805C
#define TXGBE_PHY_TX_EQ_CTL0                    0x018036
#define TXGBE_PHY_TX_EQ_CTL1                    0x018037
#define   TXGBE_PHY_TX_EQ_CTL1_DEF		MS16(7, 0x1)
#define TXGBE_PHY_TX_RATE_CTL                   0x018034
#define TXGBE_PHY_RX_RATE_CTL                   0x018054
#define TXGBE_PHY_TX_GEN_CTL2                   0x018032
#define TXGBE_PHY_RX_GEN_CTL2                   0x018052
#define TXGBE_PHY_RX_GEN_CTL3                   0x018053
#define TXGBE_PHY_MPLLA_CTL2                    0x018073
#define TXGBE_PHY_RX_POWER_ST_CTL               0x018055
#define TXGBE_PHY_TX_POWER_ST_CTL               0x018035
#define TXGBE_PHY_TX_GENCTRL1                   0x018031
#define TXGBE_PHY_EQ_INIT_CTL0			0x01803A
#define TXGBE_PHY_EQ_INIT_CTL1			0x01803B

#define TXGBE_PHY_MPLLA_CTL0_MULTIPLIER_1GBASEX_KX              32
#define TXGBE_PHY_MPLLA_CTL0_MULTIPLIER_10GBASER_KR             33
#define TXGBE_PHY_MPLLA_CTL0_MULTIPLIER_OTHER                   40
#define TXGBE_PHY_MPLLA_CTL0_MULTIPLIER_MASK                    0xFF
#define TXGBE_PHY_MPLLA_CTL3_MULTIPLIER_BW_1GBASEX_KX           0x56
#define TXGBE_PHY_MPLLA_CTL3_MULTIPLIER_BW_10GBASER_KR          0x7B
#define TXGBE_PHY_MPLLA_CTL3_MULTIPLIER_BW_OTHER                0x56
#define TXGBE_PHY_MPLLA_CTL3_MULTIPLIER_BW_MASK                 0x7FF
#define TXGBE_PHY_MISC_CTL0_TX2RX_LB_EN_0                       0x1
#define TXGBE_PHY_MISC_CTL0_TX2RX_LB_EN_3_1                     0xE
#define TXGBE_PHY_MISC_CTL0_RX_VREF_CTRL                        0x1F00
#define TXGBE_PHY_VCO_CAL_LD0_1GBASEX_KX                        1344
#define TXGBE_PHY_VCO_CAL_LD0_10GBASER_KR                       1353
#define TXGBE_PHY_VCO_CAL_LD0_OTHER                             1360
#define TXGBE_PHY_VCO_CAL_LD0_MASK                              0x1000
#define TXGBE_PHY_VCO_CAL_REF0_LD0_1GBASEX_KX                   42
#define TXGBE_PHY_VCO_CAL_REF0_LD0_10GBASER_KR                  41
#define TXGBE_PHY_VCO_CAL_REF0_LD0_OTHER                        34
#define TXGBE_PHY_VCO_CAL_REF0_LD0_MASK                         0x3F
#define TXGBE_PHY_AFE_DFE_ENABLE_DFE_EN0                        0x10
#define TXGBE_PHY_AFE_DFE_ENABLE_AFE_EN0                        0x1
#define TXGBE_PHY_AFE_DFE_ENABLE_MASK                           0xFF
#define TXGBE_PHY_RX_EQ_CTL_CONT_ADAPT0                         0x1
#define TXGBE_PHY_RX_EQ_CTL_CONT_ADAPT_MASK                     0xF
#define TXGBE_PHY_TX_RATE_CTL_TX0_RATE_10GBASER_KR              0x0
#define TXGBE_PHY_TX_RATE_CTL_TX0_RATE_RXAUI                    0x1
#define TXGBE_PHY_TX_RATE_CTL_TX0_RATE_1GBASEX_KX               0x3
#define TXGBE_PHY_TX_RATE_CTL_TX0_RATE_OTHER                    0x2
#define TXGBE_PHY_TX_RATE_CTL_TX1_RATE_OTHER                    0x20
#define TXGBE_PHY_TX_RATE_CTL_TX2_RATE_OTHER                    0x200
#define TXGBE_PHY_TX_RATE_CTL_TX3_RATE_OTHER                    0x2000
#define TXGBE_PHY_TX_RATE_CTL_TX0_RATE_MASK                     0x7
#define TXGBE_PHY_TX_RATE_CTL_TX1_RATE_MASK                     0x70
#define TXGBE_PHY_TX_RATE_CTL_TX2_RATE_MASK                     0x700
#define TXGBE_PHY_TX_RATE_CTL_TX3_RATE_MASK                     0x7000
#define TXGBE_PHY_RX_RATE_CTL_RX0_RATE_10GBASER_KR              0x0
#define TXGBE_PHY_RX_RATE_CTL_RX0_RATE_RXAUI                    0x1
#define TXGBE_PHY_RX_RATE_CTL_RX0_RATE_1GBASEX_KX               0x3
#define TXGBE_PHY_RX_RATE_CTL_RX0_RATE_OTHER                    0x2
#define TXGBE_PHY_RX_RATE_CTL_RX1_RATE_OTHER                    0x20
#define TXGBE_PHY_RX_RATE_CTL_RX2_RATE_OTHER                    0x200
#define TXGBE_PHY_RX_RATE_CTL_RX3_RATE_OTHER                    0x2000
#define TXGBE_PHY_RX_RATE_CTL_RX0_RATE_MASK                     0x7
#define TXGBE_PHY_RX_RATE_CTL_RX1_RATE_MASK                     0x70
#define TXGBE_PHY_RX_RATE_CTL_RX2_RATE_MASK                     0x700
#define TXGBE_PHY_RX_RATE_CTL_RX3_RATE_MASK                     0x7000
#define TXGBE_PHY_TX_GEN_CTL2_TX0_WIDTH_10GBASER_KR             0x200
#define TXGBE_PHY_TX_GEN_CTL2_TX0_WIDTH_10GBASER_KR_RXAUI       0x300
#define TXGBE_PHY_TX_GEN_CTL2_TX0_WIDTH_OTHER                   0x100
#define TXGBE_PHY_TX_GEN_CTL2_TX0_WIDTH_MASK                    0x300
#define TXGBE_PHY_TX_GEN_CTL2_TX1_WIDTH_OTHER                   0x400
#define TXGBE_PHY_TX_GEN_CTL2_TX1_WIDTH_MASK                    0xC00
#define TXGBE_PHY_TX_GEN_CTL2_TX2_WIDTH_OTHER                   0x1000
#define TXGBE_PHY_TX_GEN_CTL2_TX2_WIDTH_MASK                    0x3000
#define TXGBE_PHY_TX_GEN_CTL2_TX3_WIDTH_OTHER                   0x4000
#define TXGBE_PHY_TX_GEN_CTL2_TX3_WIDTH_MASK                    0xC000
#define TXGBE_PHY_RX_GEN_CTL2_RX0_WIDTH_10GBASER_KR             0x200
#define TXGBE_PHY_RX_GEN_CTL2_RX0_WIDTH_10GBASER_KR_RXAUI       0x300
#define TXGBE_PHY_RX_GEN_CTL2_RX0_WIDTH_OTHER                   0x100
#define TXGBE_PHY_RX_GEN_CTL2_RX0_WIDTH_MASK                    0x300
#define TXGBE_PHY_RX_GEN_CTL2_RX1_WIDTH_OTHER                   0x400
#define TXGBE_PHY_RX_GEN_CTL2_RX1_WIDTH_MASK                    0xC00
#define TXGBE_PHY_RX_GEN_CTL2_RX2_WIDTH_OTHER                   0x1000
#define TXGBE_PHY_RX_GEN_CTL2_RX2_WIDTH_MASK                    0x3000
#define TXGBE_PHY_RX_GEN_CTL2_RX3_WIDTH_OTHER                   0x4000
#define TXGBE_PHY_RX_GEN_CTL2_RX3_WIDTH_MASK                    0xC000
#define TXGBE_PHY_MPLLA_CTL2_DIV_CLK_EN_8                       0x100
#define TXGBE_PHY_MPLLA_CTL2_DIV_CLK_EN_10                      0x200
#define TXGBE_PHY_MPLLA_CTL2_DIV_CLK_EN_16P5                    0x400
#define TXGBE_PHY_MPLLA_CTL2_DIV_CLK_EN_MASK                    0x700
#define TXGBE_PHY_LANE0_TX_EQ_CTL1				0x100E
#define   TXGBE_PHY_LANE0_TX_EQ_CTL1_MAIN(v)			RS16(v, 6, 0x3F)
#define TXGBE_PHY_LANE0_TX_EQ_CTL2				0x100F
#define   TXGBE_PHY_LANE0_TX_EQ_CTL2_PRE			MS16(0, 0x3F)
#define   TXGBE_PHY_LANE0_TX_EQ_CTL2_POST(v)			RS16(v, 6, 0x3F)

/******************************************************************************
 * SFP I2C Registers:
 ******************************************************************************/
/* SFP IDs: format of OUI is 0x[byte0][byte1][byte2][00] */
#define TXGBE_SFF_VENDOR_OUI_TYCO	0x00407600
#define TXGBE_SFF_VENDOR_OUI_FTL	0x00906500
#define TXGBE_SFF_VENDOR_OUI_AVAGO	0x00176A00
#define TXGBE_SFF_VENDOR_OUI_INTEL	0x001B2100

/* EEPROM (dev_addr = 0xA0) */
#define TXGBE_I2C_EEPROM_DEV_ADDR	0xA0
#define TXGBE_SFF_IDENTIFIER		0x00
#define TXGBE_SFF_IDENTIFIER_SFP	0x03
#define TXGBE_SFF_VENDOR_OUI_BYTE0	0x25
#define TXGBE_SFF_VENDOR_OUI_BYTE1	0x26
#define TXGBE_SFF_VENDOR_OUI_BYTE2	0x27
#define TXGBE_SFF_1GBE_COMP_CODES	0x06
#define TXGBE_SFF_10GBE_COMP_CODES	0x03
#define TXGBE_SFF_CABLE_TECHNOLOGY	0x08
#define   TXGBE_SFF_CABLE_DA_PASSIVE    0x4
#define   TXGBE_SFF_CABLE_DA_ACTIVE     0x8
#define TXGBE_SFF_CABLE_SPEC_COMP	0x3C
#define TXGBE_SFF_SFF_8472_SWAP		0x5C
#define TXGBE_SFF_SFF_8472_COMP		0x5E
#define TXGBE_SFF_SFF_8472_OSCB		0x6E
#define TXGBE_SFF_SFF_8472_ESCB		0x76

#define TXGBE_SFF_IDENTIFIER_QSFP_PLUS	0x0D
#define TXGBE_SFF_QSFP_VENDOR_OUI_BYTE0	0xA5
#define TXGBE_SFF_QSFP_VENDOR_OUI_BYTE1	0xA6
#define TXGBE_SFF_QSFP_VENDOR_OUI_BYTE2	0xA7
#define TXGBE_SFF_QSFP_CONNECTOR	0x82
#define TXGBE_SFF_QSFP_10GBE_COMP	0x83
#define TXGBE_SFF_QSFP_1GBE_COMP	0x86
#define TXGBE_SFF_QSFP_CABLE_LENGTH	0x92
#define TXGBE_SFF_QSFP_DEVICE_TECH	0x93

/* Bitmasks */
#define TXGBE_SFF_DA_SPEC_ACTIVE_LIMITING	0x4
#define TXGBE_SFF_1GBASESX_CAPABLE		0x1
#define TXGBE_SFF_1GBASELX_CAPABLE		0x2
#define TXGBE_SFF_1GBASET_CAPABLE		0x8
#define TXGBE_SFF_10GBASESR_CAPABLE		0x10
#define TXGBE_SFF_10GBASELR_CAPABLE		0x20
#define TXGBE_SFF_SOFT_RS_SELECT_MASK		0x8
#define TXGBE_SFF_SOFT_RS_SELECT_10G		0x8
#define TXGBE_SFF_SOFT_RS_SELECT_1G		0x0
#define TXGBE_SFF_ADDRESSING_MODE		0x4
#define TXGBE_SFF_QSFP_DA_ACTIVE_CABLE		0x1
#define TXGBE_SFF_QSFP_DA_PASSIVE_CABLE		0x8
#define TXGBE_SFF_QSFP_CONNECTOR_NOT_SEPARABLE	0x23
#define TXGBE_SFF_QSFP_TRANSMITTER_850NM_VCSEL	0x0
#define TXGBE_I2C_EEPROM_READ_MASK		0x100
#define TXGBE_I2C_EEPROM_STATUS_MASK		0x3
#define TXGBE_I2C_EEPROM_STATUS_NO_OPERATION	0x0
#define TXGBE_I2C_EEPROM_STATUS_PASS		0x1
#define TXGBE_I2C_EEPROM_STATUS_FAIL		0x2
#define TXGBE_I2C_EEPROM_STATUS_IN_PROGRESS	0x3

/* EEPROM for SFF-8472 (dev_addr = 0xA2) */
#define TXGBE_I2C_EEPROM_DEV_ADDR2	0xA2

/* SFP+ SFF-8472 Compliance */
#define TXGBE_SFF_SFF_8472_UNSUP	0x00

/******************************************************************************
 * PHY MDIO Registers:
 ******************************************************************************/
#define TXGBE_MAX_PHY_ADDR		32
/* PHY IDs*/
#define TXGBE_PHYID_MTD3310             0x00000000U
#define TXGBE_PHYID_TN1010              0x00A19410U
#define TXGBE_PHYID_QT2022              0x0043A400U
#define TXGBE_PHYID_ATH                 0x03429050U

/* (dev_type = 1) */
#define TXGBE_MD_DEV_PMA_PMD		0x1
#define TXGBE_MD_PHY_ID_HIGH		0x2 /* PHY ID High Reg*/
#define TXGBE_MD_PHY_ID_LOW		0x3 /* PHY ID Low Reg*/
#define   TXGBE_PHY_REVISION_MASK	0xFFFFFFF0
#define TXGBE_MD_PHY_SPEED_ABILITY	0x4 /* Speed Ability Reg */
#define TXGBE_MD_PHY_SPEED_10G		0x0001 /* 10G capable */
#define TXGBE_MD_PHY_SPEED_1G		0x0010 /* 1G capable */
#define TXGBE_MD_PHY_SPEED_100M		0x0020 /* 100M capable */
#define TXGBE_MD_PHY_EXT_ABILITY	0xB /* Ext Ability Reg */
#define TXGBE_MD_PHY_10GBASET_ABILITY	0x0004 /* 10GBaseT capable */
#define TXGBE_MD_PHY_1000BASET_ABILITY	0x0020 /* 1000BaseT capable */
#define TXGBE_MD_PHY_100BASETX_ABILITY	0x0080 /* 100BaseTX capable */
#define TXGBE_MD_PHY_SET_LOW_POWER_MODE	0x0800 /* Set low power mode */

#define TXGBE_MD_TX_VENDOR_ALARMS_3	0xCC02 /* Vendor Alarms 3 Reg */
#define TXGBE_MD_PMA_PMD_SDA_SCL_ADDR	0xC30A /* PHY_XS SDA/SCL Addr Reg */
#define TXGBE_MD_PMA_PMD_SDA_SCL_DATA	0xC30B /* PHY_XS SDA/SCL Data Reg */
#define TXGBE_MD_PMA_PMD_SDA_SCL_STAT	0xC30C /* PHY_XS SDA/SCL Status Reg */

#define TXGBE_MD_FW_REV_LO		0xC011
#define TXGBE_MD_FW_REV_HI		0xC012

#define TXGBE_TN_LASI_STATUS_REG	0x9005
#define TXGBE_TN_LASI_STATUS_TEMP_ALARM	0x0008

/* (dev_type = 3) */
#define TXGBE_MD_DEV_PCS	0x3
#define TXGBE_PCRC8ECL		0x0E810 /* PCR CRC-8 Error Count Lo */
#define TXGBE_PCRC8ECH		0x0E811 /* PCR CRC-8 Error Count Hi */
#define   TXGBE_PCRC8ECH_MASK	0x1F
#define TXGBE_LDPCECL		0x0E820 /* PCR Uncorrected Error Count Lo */
#define TXGBE_LDPCECH		0x0E821 /* PCR Uncorrected Error Count Hi */

/* (dev_type = 4) */
#define TXGBE_MD_DEV_PHY_XS		0x4
#define TXGBE_MD_PHY_XS_CONTROL		0x0 /* PHY_XS Control Reg */
#define TXGBE_MD_PHY_XS_RESET		0x8000 /* PHY_XS Reset */

/* (dev_type = 7) */
#define TXGBE_MD_DEV_AUTO_NEG		0x7

#define TXGBE_MD_AUTO_NEG_CONTROL	   0x0 /* AUTO_NEG Control Reg */
#define TXGBE_MD_AUTO_NEG_STATUS           0x1 /* AUTO_NEG Status Reg */
#define TXGBE_MD_AUTO_NEG_VENDOR_STAT      0xC800 /*AUTO_NEG Vendor Status Reg*/
#define TXGBE_MD_AUTO_NEG_VENDOR_TX_ALARM  0xCC00 /* AUTO_NEG Vendor TX Reg */
#define TXGBE_MD_AUTO_NEG_VENDOR_TX_ALARM2 0xCC01 /* AUTO_NEG Vendor Tx Reg */
#define TXGBE_MD_AUTO_NEG_VEN_LSC	   0x1 /* AUTO_NEG Vendor Tx LSC */
#define TXGBE_MD_AUTO_NEG_ADVT		   0x10 /* AUTO_NEG Advt Reg */
#define   TXGBE_TAF_SYM_PAUSE		   MS16(10, 0x3)
#define   TXGBE_TAF_ASM_PAUSE		   MS16(11, 0x3)

#define TXGBE_MD_AUTO_NEG_LP		0x13 /* AUTO_NEG LP Status Reg */
#define TXGBE_MD_AUTO_NEG_EEE_ADVT	0x3C /* AUTO_NEG EEE Advt Reg */
/* PHY address definitions for new protocol MDIO commands */
#define TXGBE_MII_10GBASE_T_AUTONEG_CTRL_REG	0x20   /* 10G Control Reg */
#define TXGBE_MII_AUTONEG_VENDOR_PROVISION_1_REG 0xC400 /* 1G Provisioning 1 */
#define TXGBE_MII_AUTONEG_XNP_TX_REG		0x17   /* 1G XNP Transmit */
#define TXGBE_MII_AUTONEG_ADVERTISE_REG		0x10   /* 100M Advertisement */
#define TXGBE_MII_10GBASE_T_ADVERTISE		0x1000 /* full duplex, bit:12*/
#define TXGBE_MII_1GBASE_T_ADVERTISE_XNP_TX	0x4000 /* full duplex, bit:14*/
#define TXGBE_MII_1GBASE_T_ADVERTISE		0x8000 /* full duplex, bit:15*/
#define TXGBE_MII_2_5GBASE_T_ADVERTISE		0x0400
#define TXGBE_MII_5GBASE_T_ADVERTISE		0x0800
#define TXGBE_MII_100BASE_T_ADVERTISE		0x0100 /* full duplex, bit:8 */
#define TXGBE_MII_100BASE_T_ADVERTISE_HALF	0x0080 /* half duplex, bit:7 */
#define TXGBE_MII_RESTART			0x200
#define TXGBE_MII_AUTONEG_COMPLETE		0x20
#define TXGBE_MII_AUTONEG_LINK_UP		0x04
#define TXGBE_MII_AUTONEG_REG			0x0
#define TXGBE_MD_PMA_TX_VEN_LASI_INT_MASK 0xD401 /* PHY TX Vendor LASI */
#define TXGBE_MD_PMA_TX_VEN_LASI_INT_EN   0x1 /* PHY TX Vendor LASI enable */
#define TXGBE_MD_PMD_STD_TX_DISABLE_CNTR 0x9 /* Standard Transmit Dis Reg */
#define TXGBE_MD_PMD_GLOBAL_TX_DISABLE 0x0001 /* PMD Global Transmit Dis */

/* (dev_type = 30) */
#define TXGBE_MD_DEV_VENDOR_1	30
#define TXGBE_MD_DEV_XFI_DSP	30
#define TNX_FW_REV		0xB
#define TXGBE_MD_VENDOR_SPECIFIC_1_CONTROL		0x0 /* VS1 Ctrl Reg */
#define TXGBE_MD_VENDOR_SPECIFIC_1_STATUS		0x1 /* VS1 Status Reg */
#define TXGBE_MD_VENDOR_SPECIFIC_1_LINK_STATUS		0x0008 /* 1 = Link Up */
#define TXGBE_MD_VENDOR_SPECIFIC_1_SPEED_STATUS		0x0010 /* 0-10G, 1-1G */
#define TXGBE_MD_VENDOR_SPECIFIC_1_10G_SPEED		0x0018
#define TXGBE_MD_VENDOR_SPECIFIC_1_1G_SPEED		0x0010

/* (dev_type = 31) */
#define TXGBE_MD_DEV_GENERAL          31
#define TXGBE_MD_PORT_CTRL            0xF001
#define   TXGBE_MD_PORT_CTRL_RESET    MS16(14, 0x1)

#define TXGBE_BP_M_NULL                      0
#define TXGBE_BP_M_SFI                       1
#define TXGBE_BP_M_KR                        2
#define TXGBE_BP_M_KX4                       3
#define TXGBE_BP_M_KX                        4
#define TXGBE_BP_M_NAUTO                     0
#define TXGBE_BP_M_AUTO                      1

#ifndef CL72_KRTR_PRBS_MODE_EN
#define CL72_KRTR_PRBS_MODE_EN	0xFFFF	/* open kr prbs check */
#endif

/******************************************************************************
 * SFP I2C Registers:
 ******************************************************************************/
#define TXGBE_I2C_SLAVEADDR            (0x50)

bool txgbe_validate_phy_addr(struct txgbe_hw *hw, u32 phy_addr);
enum txgbe_phy_type txgbe_get_phy_type_from_id(u32 phy_id);
s32 txgbe_get_phy_id(struct txgbe_hw *hw);
s32 txgbe_identify_phy(struct txgbe_hw *hw);
s32 txgbe_reset_phy(struct txgbe_hw *hw);
s32 txgbe_read_phy_reg_mdi(struct txgbe_hw *hw, u32 reg_addr, u32 device_type,
			   u16 *phy_data);
s32 txgbe_write_phy_reg_mdi(struct txgbe_hw *hw, u32 reg_addr, u32 device_type,
			    u16 phy_data);
s32 txgbe_read_phy_reg(struct txgbe_hw *hw, u32 reg_addr,
			       u32 device_type, u16 *phy_data);
s32 txgbe_write_phy_reg(struct txgbe_hw *hw, u32 reg_addr,
				u32 device_type, u16 phy_data);
s32 txgbe_setup_phy_link(struct txgbe_hw *hw);
s32 txgbe_setup_phy_link_speed(struct txgbe_hw *hw,
				       u32 speed,
				       bool autoneg_wait_to_complete);
s32 txgbe_get_phy_fw_version(struct txgbe_hw *hw, u32 *fw_version);
s32 txgbe_get_copper_link_capabilities(struct txgbe_hw *hw,
					       u32 *speed,
					       bool *autoneg);
s32 txgbe_check_reset_blocked(struct txgbe_hw *hw);

/* PHY specific */
s32 txgbe_check_phy_link_tnx(struct txgbe_hw *hw,
			     u32 *speed,
			     bool *link_up);
s32 txgbe_setup_phy_link_tnx(struct txgbe_hw *hw);

s32 txgbe_identify_module(struct txgbe_hw *hw);
s32 txgbe_identify_sfp_module(struct txgbe_hw *hw);
s32 txgbe_identify_qsfp_module(struct txgbe_hw *hw);

s32 txgbe_read_i2c_byte(struct txgbe_hw *hw, u8 byte_offset,
				u8 dev_addr, u8 *data);
s32 txgbe_read_i2c_byte_unlocked(struct txgbe_hw *hw, u8 byte_offset,
					 u8 dev_addr, u8 *data);
s32 txgbe_write_i2c_byte(struct txgbe_hw *hw, u8 byte_offset,
				 u8 dev_addr, u8 data);
s32 txgbe_write_i2c_byte_unlocked(struct txgbe_hw *hw, u8 byte_offset,
					  u8 dev_addr, u8 data);
s32 txgbe_read_i2c_sff8472(struct txgbe_hw *hw, u8 byte_offset,
					  u8 *sff8472_data);
s32 txgbe_read_i2c_eeprom(struct txgbe_hw *hw, u8 byte_offset,
				  u8 *eeprom_data);
s32 txgbe_write_i2c_eeprom(struct txgbe_hw *hw, u8 byte_offset,
				   u8 eeprom_data);
u64 txgbe_autoc_read(struct txgbe_hw *hw);
void txgbe_autoc_write(struct txgbe_hw *hw, u64 value);
void txgbe_bp_mode_set(struct txgbe_hw *hw);
void txgbe_set_phy_temp(struct txgbe_hw *hw);
void txgbe_bp_down_event(struct txgbe_hw *hw);
s32 txgbe_kr_handle(struct txgbe_hw *hw);

#endif /* _TXGBE_PHY_H_ */
