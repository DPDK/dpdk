/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#ifndef _TXGBE_REGS_H_
#define _TXGBE_REGS_H_

#define TXGBE_PVMBX_QSIZE          (16) /* 16*4B */
#define TXGBE_PVMBX_BSIZE          (TXGBE_PVMBX_QSIZE * 4)

#define TXGBE_REMOVED(a) (0)

#define TXGBE_REG_DUMMY             0xFFFFFF

#define MS8(shift, mask)          (((u8)(mask)) << (shift))
#define LS8(val, shift, mask)     (((u8)(val) & (u8)(mask)) << (shift))
#define RS8(reg, shift, mask)     (((u8)(reg) >> (shift)) & (u8)(mask))

#define MS16(shift, mask)         (((u16)(mask)) << (shift))
#define LS16(val, shift, mask)    (((u16)(val) & (u16)(mask)) << (shift))
#define RS16(reg, shift, mask)    (((u16)(reg) >> (shift)) & (u16)(mask))

#define MS32(shift, mask)         (((u32)(mask)) << (shift))
#define LS32(val, shift, mask)    (((u32)(val) & (u32)(mask)) << (shift))
#define RS32(reg, shift, mask)    (((u32)(reg) >> (shift)) & (u32)(mask))

#define MS64(shift, mask)         (((u64)(mask)) << (shift))
#define LS64(val, shift, mask)    (((u64)(val) & (u64)(mask)) << (shift))
#define RS64(reg, shift, mask)    (((u64)(reg) >> (shift)) & (u64)(mask))

#define MS(shift, mask)           MS32(shift, mask)
#define LS(val, shift, mask)      LS32(val, shift, mask)
#define RS(reg, shift, mask)      RS32(reg, shift, mask)

#define ROUND_UP(x, y)          (((x) + (y) - 1) / (y) * (y))
#define ROUND_DOWN(x, y)        ((x) / (y) * (y))
#define ROUND_OVER(x, maxbits, unitbits) \
	((x) >= 1 << (maxbits) ? 0 : (x) >> (unitbits))

/* autoc bits definition */
#define TXGBE_AUTOC                       TXGBE_REG_DUMMY
#define   TXGBE_AUTOC_FLU                 MS64(0, 0x1)
#define   TXGBE_AUTOC_10G_PMA_PMD_MASK    MS64(7, 0x3) /* parallel */
#define   TXGBE_AUTOC_10G_XAUI            LS64(0, 7, 0x3)
#define   TXGBE_AUTOC_10G_KX4             LS64(1, 7, 0x3)
#define   TXGBE_AUTOC_10G_CX4             LS64(2, 7, 0x3)
#define   TXGBE_AUTOC_10G_KR              LS64(3, 7, 0x3) /* fixme */
#define   TXGBE_AUTOC_1G_PMA_PMD_MASK     MS64(9, 0x7)
#define   TXGBE_AUTOC_1G_BX               LS64(0, 9, 0x7)
#define   TXGBE_AUTOC_1G_KX               LS64(1, 9, 0x7)
#define   TXGBE_AUTOC_1G_SFI              LS64(0, 9, 0x7)
#define   TXGBE_AUTOC_1G_KX_BX            LS64(1, 9, 0x7)
#define   TXGBE_AUTOC_AN_RESTART          MS64(12, 0x1)
#define   TXGBE_AUTOC_LMS_MASK            MS64(13, 0x7)
#define   TXGBE_AUTOC_LMS_10G             LS64(3, 13, 0x7)
#define   TXGBE_AUTOC_LMS_KX4_KX_KR       LS64(4, 13, 0x7)
#define   TXGBE_AUTOC_LMS_SGMII_1G_100M   LS64(5, 13, 0x7)
#define   TXGBE_AUTOC_LMS_KX4_KX_KR_1G_AN LS64(6, 13, 0x7)
#define   TXGBE_AUTOC_LMS_KX4_KX_KR_SGMII LS64(7, 13, 0x7)
#define   TXGBE_AUTOC_LMS_1G_LINK_NO_AN   LS64(0, 13, 0x7)
#define   TXGBE_AUTOC_LMS_10G_LINK_NO_AN  LS64(1, 13, 0x7)
#define   TXGBE_AUTOC_LMS_1G_AN           LS64(2, 13, 0x7)
#define   TXGBE_AUTOC_LMS_KX4_AN          LS64(4, 13, 0x7)
#define   TXGBE_AUTOC_LMS_KX4_AN_1G_AN    LS64(6, 13, 0x7)
#define   TXGBE_AUTOC_LMS_ATTACH_TYPE     LS64(7, 13, 0x7)
#define   TXGBE_AUTOC_LMS_AN              MS64(15, 0x7)

#define   TXGBE_AUTOC_KR_SUPP             MS64(16, 0x1)
#define   TXGBE_AUTOC_FECR                MS64(17, 0x1)
#define   TXGBE_AUTOC_FECA                MS64(18, 0x1)
#define   TXGBE_AUTOC_AN_RX_ALIGN         MS64(18, 0x1F) /* fixme */
#define   TXGBE_AUTOC_AN_RX_DRIFT         MS64(23, 0x3)
#define   TXGBE_AUTOC_AN_RX_LOOSE         MS64(24, 0x3)
#define   TXGBE_AUTOC_PD_TMR              MS64(25, 0x3)
#define   TXGBE_AUTOC_RF                  MS64(27, 0x1)
#define   TXGBE_AUTOC_ASM_PAUSE           MS64(29, 0x1)
#define   TXGBE_AUTOC_SYM_PAUSE           MS64(28, 0x1)
#define   TXGBE_AUTOC_PAUSE               MS64(28, 0x3)
#define   TXGBE_AUTOC_KX_SUPP             MS64(30, 0x1)
#define   TXGBE_AUTOC_KX4_SUPP            MS64(31, 0x1)

#define   TXGBE_AUTOC_10GS_PMA_PMD_MASK   MS64(48, 0x3)  /* serial */
#define   TXGBE_AUTOC_10GS_KR             LS64(0, 48, 0x3)
#define   TXGBE_AUTOC_10GS_XFI            LS64(1, 48, 0x3)
#define   TXGBE_AUTOC_10GS_SFI            LS64(2, 48, 0x3)
#define   TXGBE_AUTOC_LINK_DIA_MASK       MS64(60, 0x7)
#define   TXGBE_AUTOC_LINK_DIA_D3_MASK    LS64(5, 60, 0x7)

#define   TXGBE_AUTOC_SPEED_MASK          MS64(32, 0xFFFF)
#define   TXGBD_AUTOC_SPEED(r)            RS64(r, 32, 0xFFFF)
#define   TXGBE_AUTOC_SPEED(v)            LS64(v, 32, 0xFFFF)
#define     TXGBE_LINK_SPEED_UNKNOWN      0
#define     TXGBE_LINK_SPEED_10M_FULL     0x0002
#define     TXGBE_LINK_SPEED_100M_FULL    0x0008
#define     TXGBE_LINK_SPEED_1GB_FULL     0x0020
#define     TXGBE_LINK_SPEED_2_5GB_FULL   0x0400
#define     TXGBE_LINK_SPEED_5GB_FULL     0x0800
#define     TXGBE_LINK_SPEED_10GB_FULL    0x0080
#define     TXGBE_LINK_SPEED_40GB_FULL    0x0100
#define   TXGBE_AUTOC_AUTONEG             MS64(63, 0x1)



/* Hardware Datapath:
 *  RX:     / Queue <- Filter \
 *      Host     |             TC <=> SEC <=> MAC <=> PHY
 *  TX:     \ Queue -> Filter /
 *
 * Packet Filter:
 *  RX: RSS < FDIR < Filter < Encrypt
 *
 * Macro Argument Naming:
 *   rp = ring pair         [0,127]
 *   tc = traffic class     [0,7]
 *   up = user priority     [0,7]
 *   pi = pool index        [0,63]
 *   r  = register
 *   v  = value
 *   s  = shift
 *   m  = mask
 *   i,j,k  = array index
 *   H,L    = high/low bits
 *   HI,LO  = high/low state
 */

#define TXGBE_ETHPHYIF                  TXGBE_REG_DUMMY
#define   TXGBE_ETHPHYIF_MDIO_ACT       MS(1, 0x1)
#define   TXGBE_ETHPHYIF_MDIO_MODE      MS(2, 0x1)
#define   TXGBE_ETHPHYIF_MDIO_BASE(r)   RS(r, 3, 0x1F)
#define   TXGBE_ETHPHYIF_MDIO_SHARED    MS(13, 0x1)
#define   TXGBE_ETHPHYIF_SPEED_10M      MS(17, 0x1)
#define   TXGBE_ETHPHYIF_SPEED_100M     MS(18, 0x1)
#define   TXGBE_ETHPHYIF_SPEED_1G       MS(19, 0x1)
#define   TXGBE_ETHPHYIF_SPEED_2_5G     MS(20, 0x1)
#define   TXGBE_ETHPHYIF_SPEED_10G      MS(21, 0x1)
#define   TXGBE_ETHPHYIF_SGMII_ENABLE   MS(25, 0x1)
#define   TXGBE_ETHPHYIF_INT_PHY_MODE   MS(24, 0x1)
#define   TXGBE_ETHPHYIF_IO_XPCS        MS(30, 0x1)
#define   TXGBE_ETHPHYIF_IO_EPHY        MS(31, 0x1)

/******************************************************************************
 * Chip Registers
 ******************************************************************************/
/**
 * Chip Status
 **/
#define TXGBE_PWR                  0x010000
#define   TXGBE_PWR_LAN(r)         RS(r, 30, 0x3)
#define     TXGBE_PWR_LAN_0          (1)
#define     TXGBE_PWR_LAN_1          (2)
#define     TXGBE_PWR_LAN_A          (3)
#define TXGBE_CTL                  0x010004
#define TXGBE_LOCKPF               0x010008
#define TXGBE_RST                  0x01000C
#define   TXGBE_RST_SW             MS(0, 0x1)
#define   TXGBE_RST_LAN(i)         MS(((i) + 1), 0x1)
#define   TXGBE_RST_FW             MS(3, 0x1)
#define   TXGBE_RST_ETH(i)         MS(((i) + 29), 0x1)
#define   TXGBE_RST_GLB            MS(31, 0x1)
#define   TXGBE_RST_DEFAULT        (TXGBE_RST_SW | \
				   TXGBE_RST_LAN(0) | \
				   TXGBE_RST_LAN(1))

#define TXGBE_STAT			0x010028
#define   TXGBE_STAT_MNGINIT		MS(0, 0x1)
#define   TXGBE_STAT_MNGVETO		MS(8, 0x1)
#define   TXGBE_STAT_ECCLAN0		MS(16, 0x1)
#define   TXGBE_STAT_ECCLAN1		MS(17, 0x1)
#define   TXGBE_STAT_ECCMNG		MS(18, 0x1)
#define   TXGBE_STAT_ECCPCIE		MS(19, 0x1)
#define   TXGBE_STAT_ECCPCIW		MS(20, 0x1)
#define TXGBE_RSTSTAT                   0x010030
#define   TXGBE_RSTSTAT_PROG            MS(20, 0x1)
#define   TXGBE_RSTSTAT_PREP            MS(19, 0x1)
#define   TXGBE_RSTSTAT_TYPE_MASK       MS(16, 0x7)
#define   TXGBE_RSTSTAT_TYPE(r)         RS(r, 16, 0x7)
#define   TXGBE_RSTSTAT_TYPE_PE         LS(0, 16, 0x7)
#define   TXGBE_RSTSTAT_TYPE_PWR        LS(1, 16, 0x7)
#define   TXGBE_RSTSTAT_TYPE_HOT        LS(2, 16, 0x7)
#define   TXGBE_RSTSTAT_TYPE_SW         LS(3, 16, 0x7)
#define   TXGBE_RSTSTAT_TYPE_FW         LS(4, 16, 0x7)
#define   TXGBE_RSTSTAT_TMRINIT_MASK    MS(8, 0xFF)
#define   TXGBE_RSTSTAT_TMRINIT(v)      LS(v, 8, 0xFF)
#define   TXGBE_RSTSTAT_TMRCNT_MASK     MS(0, 0xFF)
#define   TXGBE_RSTSTAT_TMRCNT(v)       LS(v, 0, 0xFF)
#define TXGBE_PWRTMR			0x010034

/**
 * SPI(Flash)
 **/
#define TXGBE_SPICMD               0x010104
#define   TXGBE_SPICMD_ADDR(v)     LS(v, 0, 0xFFFFFF)
#define   TXGBE_SPICMD_CLK(v)      LS(v, 25, 0x7)
#define   TXGBE_SPICMD_CMD(v)      LS(v, 28, 0x7)
#define TXGBE_SPIDAT               0x010108
#define   TXGBE_SPIDAT_BYPASS      MS(31, 0x1)
#define   TXGBE_SPIDAT_STATUS(v)   LS(v, 16, 0xFF)
#define   TXGBE_SPIDAT_OPDONE      MS(0, 0x1)
#define TXGBE_SPISTATUS            0x01010C
#define   TXGBE_SPISTATUS_OPDONE   MS(0, 0x1)
#define   TXGBE_SPISTATUS_BYPASS   MS(31, 0x1)
#define TXGBE_SPIUSRCMD            0x010110
#define TXGBE_SPICFG0              0x010114
#define TXGBE_SPICFG1              0x010118
#define TXGBE_FLASH                0x010120
#define   TXGBE_FLASH_PERSTD       MS(0, 0x1)
#define   TXGBE_FLASH_PWRRSTD      MS(1, 0x1)
#define   TXGBE_FLASH_SWRSTD       MS(7, 0x1)
#define   TXGBE_FLASH_LANRSTD(i)   MS(((i) + 9), 0x1)
#define TXGBE_SRAM                 0x010124
#define   TXGBE_SRAM_SZ(v)         LS(v, 28, 0x7)
#define TXGBE_SRAMCTLECC           0x010130
#define TXGBE_SRAMINJECC           0x010134
#define TXGBE_SRAMECC              0x010138

/**
 * Thermel Sensor
 **/
#define TXGBE_TSCTL                0x010300
#define   TXGBE_TSCTL_MODE         MS(31, 0x1)
#define TXGBE_TSREVAL              0x010304
#define   TXGBE_TSREVAL_EA         MS(0, 0x1)
#define TXGBE_TSDAT                0x010308
#define   TXGBE_TSDAT_TMP(r)       ((r) & 0x3FF)
#define   TXGBE_TSDAT_VLD          MS(16, 0x1)
#define TXGBE_TSALMWTRHI           0x01030C
#define   TXGBE_TSALMWTRHI_VAL(v)  (((v) & 0x3FF))
#define TXGBE_TSALMWTRLO           0x010310
#define   TXGBE_TSALMWTRLO_VAL(v)  (((v) & 0x3FF))
#define TXGBE_TSINTWTR             0x010314
#define   TXGBE_TSINTWTR_HI        MS(0, 0x1)
#define   TXGBE_TSINTWTR_LO        MS(1, 0x1)
#define TXGBE_TSALM                0x010318
#define   TXGBE_TSALM_LO           MS(0, 0x1)
#define   TXGBE_TSALM_HI           MS(1, 0x1)

/**
 * Management
 **/
#define TXGBE_MNGTC                0x01CD10
#define TXGBE_MNGFWSYNC            0x01E000
#define   TXGBE_MNGFWSYNC_REQ      MS(0, 0x1)
#define TXGBE_MNGSWSYNC            0x01E004
#define   TXGBE_MNGSWSYNC_REQ      MS(0, 0x1)
#define TXGBE_SWSEM                0x01002C
#define   TXGBE_SWSEM_PF           MS(0, 0x1)
#define TXGBE_MNGSEM               0x01E008
#define   TXGBE_MNGSEM_SW(v)       LS(v, 0, 0xFFFF)
#define   TXGBE_MNGSEM_SWPHY       MS(0, 0x1)
#define   TXGBE_MNGSEM_SWMBX       MS(2, 0x1)
#define   TXGBE_MNGSEM_SWFLASH     MS(3, 0x1)
#define   TXGBE_MNGSEM_FW(v)       LS(v, 16, 0xFFFF)
#define   TXGBE_MNGSEM_FWPHY       MS(16, 0x1)
#define   TXGBE_MNGSEM_FWMBX       MS(18, 0x1)
#define   TXGBE_MNGSEM_FWFLASH     MS(19, 0x1)
#define TXGBE_MNGMBXCTL            0x01E044
#define   TXGBE_MNGMBXCTL_SWRDY    MS(0, 0x1)
#define   TXGBE_MNGMBXCTL_SWACK    MS(1, 0x1)
#define   TXGBE_MNGMBXCTL_FWRDY    MS(2, 0x1)
#define   TXGBE_MNGMBXCTL_FWACK    MS(3, 0x1)
#define TXGBE_MNGMBX               0x01E100

/******************************************************************************
 * Port Registers
 ******************************************************************************/
/* Port Control */
#define TXGBE_PORTCTL                   0x014400
#define   TXGBE_PORTCTL_VLANEXT         MS(0, 0x1)
#define   TXGBE_PORTCTL_ETAG            MS(1, 0x1)
#define   TXGBE_PORTCTL_QINQ            MS(2, 0x1)
#define   TXGBE_PORTCTL_DRVLOAD         MS(3, 0x1)
#define   TXGBE_PORTCTL_UPLNK           MS(4, 0x1)
#define   TXGBE_PORTCTL_DCB             MS(10, 0x1)
#define   TXGBE_PORTCTL_NUMTC_MASK      MS(11, 0x1)
#define   TXGBE_PORTCTL_NUMTC_4         LS(0, 11, 0x1)
#define   TXGBE_PORTCTL_NUMTC_8         LS(1, 11, 0x1)
#define   TXGBE_PORTCTL_NUMVT_MASK      MS(12, 0x3)
#define   TXGBE_PORTCTL_NUMVT_16        LS(1, 12, 0x3)
#define   TXGBE_PORTCTL_NUMVT_32        LS(2, 12, 0x3)
#define   TXGBE_PORTCTL_NUMVT_64        LS(3, 12, 0x3)
#define   TXGBE_PORTCTL_RSTDONE         MS(14, 0x1)
#define   TXGBE_PORTCTL_TEREDODIA       MS(27, 0x1)
#define   TXGBE_PORTCTL_GENEVEDIA       MS(28, 0x1)
#define   TXGBE_PORTCTL_VXLANGPEDIA     MS(30, 0x1)
#define   TXGBE_PORTCTL_VXLANDIA        MS(31, 0x1)

#define TXGBE_PORT                      0x014404
#define   TXGBE_PORT_LINKUP             MS(0, 0x1)
#define   TXGBE_PORT_LINK10G            MS(1, 0x1)
#define   TXGBE_PORT_LINK1000M          MS(2, 0x1)
#define   TXGBE_PORT_LINK100M           MS(3, 0x1)
#define   TXGBE_PORT_LANID(r)           RS(r, 8, 0x1)
#define TXGBE_EXTAG                     0x014408
#define   TXGBE_EXTAG_ETAG_MASK         MS(0, 0xFFFF)
#define   TXGBE_EXTAG_ETAG(v)           LS(v, 0, 0xFFFF)
#define   TXGBE_EXTAG_VLAN_MASK         MS(16, 0xFFFF)
#define   TXGBE_EXTAG_VLAN(v)           LS(v, 16, 0xFFFF)
#define TXGBE_VXLANPORT                 0x014410
#define TXGBE_VXLANPORTGPE              0x014414
#define TXGBE_GENEVEPORT                0x014418
#define TXGBE_TEREDOPORT                0x01441C
#define TXGBE_LEDCTL                    0x014424
#define   TXGBE_LEDCTL_SEL_MASK         MS(0, 0xFFFF)
#define   TXGBE_LEDCTL_ORD_MASK         MS(16, 0xFFFF)
#define   TXGBE_LEDCTL_ORD_SHIFT        16
#define   TXGBE_LEDCTL_UP		MS(0, 0x1)
#define   TXGBE_LEDCTL_10G		MS(1, 0x1)
#define   TXGBE_LEDCTL_1G		MS(2, 0x1)
#define   TXGBE_LEDCTL_100M		MS(3, 0x1)
#define   TXGBE_LEDCTL_ACTIVE		MS(4, 0x1)
#define TXGBE_TAGTPID(i)                (0x014430 + (i) * 4) /* 0-3 */
#define   TXGBE_TAGTPID_LSB_MASK        MS(0, 0xFFFF)
#define   TXGBE_TAGTPID_LSB(v)          LS(v, 0, 0xFFFF)
#define   TXGBE_TAGTPID_MSB_MASK        MS(16, 0xFFFF)
#define   TXGBE_TAGTPID_MSB(v)          LS(v, 16, 0xFFFF)

/**
 * GPIO Control
 * P0: link speed change
 * P1:
 * P2:
 * P3: optical laser disable
 * P4:
 * P5: link speed selection
 * P6:
 * P7: external phy event
 **/
#define TXGBE_SDP                  0x014800
#define   TXGBE_SDP_0              MS(0, 0x1)
#define   TXGBE_SDP_1              MS(1, 0x1)
#define   TXGBE_SDP_2              MS(2, 0x1)
#define   TXGBE_SDP_3              MS(3, 0x1)
#define   TXGBE_SDP_4              MS(4, 0x1)
#define   TXGBE_SDP_5              MS(5, 0x1)
#define   TXGBE_SDP_6              MS(6, 0x1)
#define   TXGBE_SDP_7              MS(7, 0x1)
#define TXGBE_SDPDIR               0x014804
#define TXGBE_SDPCTL               0x014808
#define TXGBE_SDPINTEA             0x014830
#define TXGBE_SDPINTMSK            0x014834
#define TXGBE_SDPINTTYP            0x014838
#define TXGBE_SDPINTPOL            0x01483C
#define TXGBE_SDPINT               0x014840
#define TXGBE_SDPINTDB             0x014848
#define TXGBE_SDPINTEND            0x01484C
#define TXGBE_SDPDAT               0x014850
#define TXGBE_SDPLVLSYN            0x014854

/**
 * MDIO(PHY)
 **/
#define TXGBE_MDIOSCA                   0x011200
#define   TXGBE_MDIOSCA_REG(v)          LS(v, 0, 0xFFFF)
#define   TXGBE_MDIOSCA_PORT(v)         LS(v, 16, 0x1F)
#define   TXGBE_MDIOSCA_DEV(v)          LS(v, 21, 0x1F)
#define TXGBE_MDIOSCD                   0x011204
#define   TXGBD_MDIOSCD_DAT(r)          RS(r, 0, 0xFFFF)
#define   TXGBE_MDIOSCD_DAT(v)          LS(v, 0, 0xFFFF)
#define   TXGBE_MDIOSCD_CMD_PREAD       LS(1, 16, 0x3)
#define   TXGBE_MDIOSCD_CMD_WRITE       LS(2, 16, 0x3)
#define   TXGBE_MDIOSCD_CMD_READ        LS(3, 16, 0x3)
#define   TXGBE_MDIOSCD_SADDR           MS(18, 0x1)
#define   TXGBE_MDIOSCD_CLOCK(v)        LS(v, 19, 0x7)
#define   TXGBE_MDIOSCD_BUSY            MS(22, 0x1)

/**
 * I2C (SFP)
 **/
#define TXGBE_I2CCTL               0x014900
#define   TXGBE_I2CCTL_MAEA        MS(0, 0x1)
#define   TXGBE_I2CCTL_SPEED(v)    LS(v, 1, 0x3)
#define   TXGBE_I2CCTL_RESTART     MS(5, 0x1)
#define   TXGBE_I2CCTL_SLDA        MS(6, 0x1)
#define TXGBE_I2CTGT               0x014904
#define   TXGBE_I2CTGT_ADDR(v)     LS(v, 0, 0x3FF)
#define TXGBE_I2CCMD               0x014910
#define   TXGBE_I2CCMD_READ        (MS(9, 0x1) | 0x100)
#define   TXGBE_I2CCMD_WRITE       (MS(9, 0x1))
#define TXGBE_I2CSCLHITM           0x014914
#define TXGBE_I2CSCLLOTM           0x014918
#define TXGBE_I2CINT               0x014934
#define   TXGBE_I2CINT_RXFULL      MS(2, 0x1)
#define   TXGBE_I2CINT_TXEMPTY     MS(4, 0x1)
#define TXGBE_I2CINTMSK            0x014930
#define TXGBE_I2CRXFIFO            0x014938
#define TXGBE_I2CTXFIFO            0x01493C
#define TXGBE_I2CEA                0x01496C
#define TXGBE_I2CST                0x014970
#define   TXGBE_I2CST_ACT          MS(5, 0x1)
#define TXGBE_I2CSCLTM             0x0149AC
#define TXGBE_I2CSDATM             0x0149B0

/**
 * TPH
 **/
#define TXGBE_TPHCFG               0x014F00

/******************************************************************************
 * Pool Registers
 ******************************************************************************/
#define TXGBE_POOLETHCTL(pl)            (0x015600 + (pl) * 4)
#define   TXGBE_POOLETHCTL_LBDIA        MS(0, 0x1)
#define   TXGBE_POOLETHCTL_LLBDIA       MS(1, 0x1)
#define   TXGBE_POOLETHCTL_LLB          MS(2, 0x1)
#define   TXGBE_POOLETHCTL_UCP          MS(4, 0x1)
#define   TXGBE_POOLETHCTL_ETP          MS(5, 0x1)
#define   TXGBE_POOLETHCTL_VLA          MS(6, 0x1)
#define   TXGBE_POOLETHCTL_VLP          MS(7, 0x1)
#define   TXGBE_POOLETHCTL_UTA          MS(8, 0x1)
#define   TXGBE_POOLETHCTL_MCHA         MS(9, 0x1)
#define   TXGBE_POOLETHCTL_UCHA         MS(10, 0x1)
#define   TXGBE_POOLETHCTL_BCA          MS(11, 0x1)
#define   TXGBE_POOLETHCTL_MCP          MS(12, 0x1)

/* DMA Control */
#define TXGBE_POOLRXENA(i)              (0x012004 + (i) * 4) /* 0-1 */
#define TXGBE_POOLRXDNA(i)              (0x012060 + (i) * 4) /* 0-1 */
#define TXGBE_POOLTXENA(i)              (0x018004 + (i) * 4) /* 0-1 */
#define TXGBE_POOLTXDSA(i)              (0x0180A0 + (i) * 4) /* 0-1 */
#define TXGBE_POOLTXLBET(i)             (0x018050 + (i) * 4) /* 0-1 */
#define TXGBE_POOLTXASET(i)             (0x018058 + (i) * 4) /* 0-1 */
#define TXGBE_POOLTXASMAC(i)            (0x018060 + (i) * 4) /* 0-1 */
#define TXGBE_POOLTXASVLAN(i)           (0x018070 + (i) * 4) /* 0-1 */
#define TXGBE_POOLDROPSWBK(i)           (0x0151C8 + (i) * 4) /* 0-1 */

#define TXGBE_POOLTAG(pl)               (0x018100 + (pl) * 4)
#define   TXGBE_POOLTAG_VTAG(v)         LS(v, 0, 0xFFFF)
#define   TXGBE_POOLTAG_VTAG_MASK       MS(0, 0xFFFF)
#define   TXGBD_POOLTAG_VTAG_UP(r)	RS(r, 13, 0x7)
#define   TXGBE_POOLTAG_TPIDSEL(v)      LS(v, 24, 0x7)
#define   TXGBE_POOLTAG_ETAG_MASK       MS(27, 0x3)
#define   TXGBE_POOLTAG_ETAG            LS(2, 27, 0x3)
#define   TXGBE_POOLTAG_ACT_MASK        MS(30, 0x3)
#define   TXGBE_POOLTAG_ACT_ALWAYS      LS(1, 30, 0x3)
#define   TXGBE_POOLTAG_ACT_NEVER       LS(2, 30, 0x3)
#define TXGBE_POOLTXARB                 0x018204
#define   TXGBE_POOLTXARB_WRR           MS(1, 0x1)
#define TXGBE_POOLETAG(pl)              (0x018700 + (pl) * 4)

/* RSS Hash */
#define TXGBE_POOLRSS(pl)          (0x019300 + (pl) * 4)
#define   TXGBE_POOLRSS_L4HDR      MS(1, 0x1)
#define   TXGBE_POOLRSS_L3HDR      MS(2, 0x1)
#define   TXGBE_POOLRSS_L2HDR      MS(3, 0x1)
#define   TXGBE_POOLRSS_L2TUN      MS(4, 0x1)
#define   TXGBE_POOLRSS_TUNHDR     MS(5, 0x1)
#define TXGBE_POOLRSSKEY(pl, i)    (0x01A000 + (pl) * 0x40 + (i) * 4)
#define TXGBE_POOLRSSMAP(pl, i)    (0x01B000 + (pl) * 0x40 + (i) * 4)

/******************************************************************************
 * Packet Buffer
 ******************************************************************************/
/* Flow Control */
#define TXGBE_FCXOFFTM(i)               (0x019200 + (i) * 4) /* 0-3 */
#define TXGBE_FCWTRLO(tc)               (0x019220 + (tc) * 4)
#define   TXGBE_FCWTRLO_TH(v)           LS(v, 10, 0x1FF) /* KB */
#define   TXGBE_FCWTRLO_XON             MS(31, 0x1)
#define TXGBE_FCWTRHI(tc)               (0x019260 + (tc) * 4)
#define   TXGBE_FCWTRHI_TH(v)           LS(v, 10, 0x1FF) /* KB */
#define   TXGBE_FCWTRHI_XOFF            MS(31, 0x1)
#define TXGBE_RXFCRFSH                  0x0192A0
#define   TXGBE_RXFCFSH_TIME(v)         LS(v, 0, 0xFFFF)
#define TXGBE_FCSTAT                    0x01CE00
#define   TXGBE_FCSTAT_DLNK(tc)         MS((tc), 0x1)
#define   TXGBE_FCSTAT_ULNK(tc)         MS((tc) + 8, 0x1)

#define TXGBE_RXFCCFG                   0x011090
#define   TXGBE_RXFCCFG_FC              MS(0, 0x1)
#define   TXGBE_RXFCCFG_PFC             MS(8, 0x1)
#define TXGBE_TXFCCFG                   0x0192A4
#define   TXGBE_TXFCCFG_FC              MS(3, 0x1)
#define   TXGBE_TXFCCFG_PFC             MS(4, 0x1)

/* Data Buffer */
#define TXGBE_PBRXCTL                   0x019000
#define   TXGBE_PBRXCTL_ST              MS(0, 0x1)
#define   TXGBE_PBRXCTL_ENA             MS(31, 0x1)
#define TXGBE_PBRXUP2TC                 0x019008
#define TXGBE_PBTXUP2TC                 0x01C800
#define   TXGBE_DCBUP2TC_MAP(tc, v)     LS(v, 3 * (tc), 0x7)
#define   TXGBE_DCBUP2TC_DEC(tc, r)     RS(r, 3 * (tc), 0x7)
#define TXGBE_PBRXSIZE(tc)              (0x019020 + (tc) * 4)
#define   TXGBE_PBRXSIZE_KB(v)          LS(v, 10, 0x3FF)

#define TXGBE_PBRXOFTMR                 0x019094
#define TXGBE_PBRXDBGCMD                0x019090
#define TXGBE_PBRXDBGDAT(tc)            (0x0190A0 + (tc) * 4)
#define TXGBE_PBTXDMATH(tc)             (0x018020 + (tc) * 4)
#define TXGBE_PBTXSIZE(tc)              (0x01CC00 + (tc) * 4)

/* LLI */
#define TXGBE_PBRXLLI              0x19080
#define   TXGBE_PBRXLLI_SZLT(v)    LS(v, 0, 0xFFF)
#define   TXGBE_PBRXLLI_UPLT(v)    LS(v, 16, 0x7)
#define   TXGBE_PBRXLLI_UPEA       MS(19, 0x1)
#define   TXGBE_PBRXLLI_CNM        MS(20, 0x1)

/* Port Arbiter(QoS) */
#define TXGBE_PARBTXCTL            0x01CD00
#define   TXGBE_PARBTXCTL_SP       MS(5, 0x1)
#define   TXGBE_PARBTXCTL_DA       MS(6, 0x1)
#define   TXGBE_PARBTXCTL_RECYC    MS(8, 0x1)
#define TXGBE_PARBTXCFG(tc)        (0x01CD20 + (tc) * 4)
#define   TXGBE_PARBTXCFG_CRQ(v)   LS(v, 0, 0x1FF)
#define   TXGBE_PARBTXCFG_BWG(v)   LS(v, 9, 0x7)
#define   TXGBE_PARBTXCFG_MCL(v)   LS(v, 12, 0xFFF)
#define   TXGBE_PARBTXCFG_GSP      MS(30, 0x1)
#define   TXGBE_PARBTXCFG_LSP      MS(31, 0x1)

/******************************************************************************
 * Queue Registers
 ******************************************************************************/
/* Queue Control */
#define TXGBE_QPRXDROP(i)               (0x012080 + (i) * 4) /* 0-3 */
#define TXGBE_QPRXSTRPVLAN(i)           (0x012090 + (i) * 4) /* 0-3 */
#define TXGBE_QPTXLLI(i)                (0x018040 + (i) * 4) /* 0-3 */

/* Queue Arbiter(QoS) */
#define TXGBE_QARBRXCTL            0x012000
#define   TXGBE_QARBRXCTL_RC       MS(1, 0x1)
#define   TXGBE_QARBRXCTL_WSP      MS(2, 0x1)
#define   TXGBE_QARBRXCTL_DA       MS(6, 0x1)
#define TXGBE_QARBRXCFG(tc)        (0x012040 + (tc) * 4)
#define   TXGBE_QARBRXCFG_CRQ(v)   LS(v, 0, 0x1FF)
#define   TXGBE_QARBRXCFG_BWG(v)   LS(v, 9, 0x7)
#define   TXGBE_QARBRXCFG_MCL(v)   LS(v, 12, 0xFFF)
#define   TXGBE_QARBRXCFG_GSP      MS(30, 0x1)
#define   TXGBE_QARBRXCFG_LSP      MS(31, 0x1)
#define TXGBE_QARBRXTC             0x0194F8
#define   TXGBE_QARBRXTC_RR        MS(0, 0x1)

#define TXGBE_QARBTXCTL            0x018200
#define   TXGBE_QARBTXCTL_WSP      MS(1, 0x1)
#define   TXGBE_QARBTXCTL_RECYC    MS(4, 0x1)
#define   TXGBE_QARBTXCTL_DA       MS(6, 0x1)
#define TXGBE_QARBTXCFG(tc)        (0x018220 + (tc) * 4)
#define   TXGBE_QARBTXCFG_CRQ(v)   LS(v, 0, 0x1FF)
#define   TXGBE_QARBTXCFG_BWG(v)   LS(v, 9, 0x7)
#define   TXGBE_QARBTXCFG_MCL(v)   LS(v, 12, 0xFFF)
#define   TXGBE_QARBTXCFG_GSP      MS(30, 0x1)
#define   TXGBE_QARBTXCFG_LSP      MS(31, 0x1)
#define TXGBE_QARBTXMMW            0x018208
#define     TXGBE_QARBTXMMW_DEF     (4)
#define     TXGBE_QARBTXMMW_JF      (20)
#define TXGBE_QARBTXRATEI          0x01820C
#define TXGBE_QARBTXRATE           0x018404
#define   TXGBE_QARBTXRATE_MIN(v)  LS(v, 0, 0x3FFF)
#define   TXGBE_QARBTXRATE_MAX(v)  LS(v, 16, 0x3FFF)
#define TXGBE_QARBTXCRED(rp)       (0x018500 + (rp) * 4)

/* QCN */
#define TXGBE_QCNADJ               0x018210
#define TXGBE_QCNRP                0x018400
#define TXGBE_QCNRPRATE            0x018404
#define TXGBE_QCNRPADJ             0x018408
#define TXGBE_QCNRPRLD             0x01840C

/* Misc Control */
#define TXGBE_RSECCTL                    0x01200C
#define   TXGBE_RSECCTL_TSRSC            MS(0, 0x1)
#define TXGBE_DMATXCTRL                  0x018000
#define   TXGBE_DMATXCTRL_ENA            MS(0, 0x1)
#define   TXGBE_DMATXCTRL_TPID_MASK      MS(16, 0xFFFF)
#define   TXGBE_DMATXCTRL_TPID(v)        LS(v, 16, 0xFFFF)

/******************************************************************************
 * Packet Filter (L2-7)
 ******************************************************************************/
/**
 * Receive Scaling
 **/
#define TXGBE_RSSTBL(i)                 (0x019400 + (i) * 4) /* 32 */
#define TXGBE_RSSKEY(i)                 (0x019480 + (i) * 4) /* 10 */
#define TXGBE_RSSPBHASH                 0x0194F0
#define   TXGBE_RSSPBHASH_BITS(tc, v)   LS(v, 3 * (tc), 0x7)
#define TXGBE_RACTL                     0x0194F4
#define   TXGBE_RACTL_RSSMKEY           MS(0, 0x1)
#define   TXGBE_RACTL_RSSENA            MS(2, 0x1)
#define   TXGBE_RACTL_RSSMASK           MS(16, 0xFFFF)
#define   TXGBE_RACTL_RSSIPV4TCP        MS(16, 0x1)
#define   TXGBE_RACTL_RSSIPV4           MS(17, 0x1)
#define   TXGBE_RACTL_RSSIPV6           MS(20, 0x1)
#define   TXGBE_RACTL_RSSIPV6TCP        MS(21, 0x1)
#define   TXGBE_RACTL_RSSIPV4UDP        MS(22, 0x1)
#define   TXGBE_RACTL_RSSIPV6UDP        MS(23, 0x1)

/**
 * Flow Director
 **/
#define PERFECT_BUCKET_64KB_HASH_MASK   0x07FF  /* 11 bits */
#define PERFECT_BUCKET_128KB_HASH_MASK  0x0FFF  /* 12 bits */
#define PERFECT_BUCKET_256KB_HASH_MASK  0x1FFF  /* 13 bits */
#define SIG_BUCKET_64KB_HASH_MASK       0x1FFF  /* 13 bits */
#define SIG_BUCKET_128KB_HASH_MASK      0x3FFF  /* 14 bits */
#define SIG_BUCKET_256KB_HASH_MASK      0x7FFF  /* 15 bits */

#define TXGBE_FDIRCTL                   0x019500
#define   TXGBE_FDIRCTL_BUF_MASK        MS(0, 0x3)
#define   TXGBE_FDIRCTL_BUF_64K         LS(1, 0, 0x3)
#define   TXGBE_FDIRCTL_BUF_128K        LS(2, 0, 0x3)
#define   TXGBE_FDIRCTL_BUF_256K        LS(3, 0, 0x3)
#define   TXGBD_FDIRCTL_BUF_BYTE(r)     (1 << (15 + RS(r, 0, 0x3)))
#define   TXGBE_FDIRCTL_INITDONE        MS(3, 0x1)
#define   TXGBE_FDIRCTL_PERFECT         MS(4, 0x1)
#define   TXGBE_FDIRCTL_REPORT_MASK     MS(5, 0x7)
#define   TXGBE_FDIRCTL_REPORT_MATCH    LS(1, 5, 0x7)
#define   TXGBE_FDIRCTL_REPORT_ALWAYS   LS(5, 5, 0x7)
#define   TXGBE_FDIRCTL_DROPQP_MASK     MS(8, 0x7F)
#define   TXGBE_FDIRCTL_DROPQP(v)       LS(v, 8, 0x7F)
#define   TXGBE_FDIRCTL_HASHBITS_MASK   LS(20, 0xF)
#define   TXGBE_FDIRCTL_HASHBITS(v)     LS(v, 20, 0xF)
#define   TXGBE_FDIRCTL_MAXLEN(v)       LS(v, 24, 0xF)
#define   TXGBE_FDIRCTL_FULLTHR(v)      LS(v, 28, 0xF)
#define TXGBE_FDIRFLEXCFG(i)            (0x019580 + (i) * 4) /* 0-15 */
#define   TXGBD_FDIRFLEXCFG_ALL(r, i)   RS(0, (i) << 3, 0xFF)
#define   TXGBE_FDIRFLEXCFG_ALL(v, i)   LS(v, (i) << 3, 0xFF)
#define   TXGBE_FDIRFLEXCFG_BASE_MAC    LS(0, 0, 0x3)
#define   TXGBE_FDIRFLEXCFG_BASE_L2     LS(1, 0, 0x3)
#define   TXGBE_FDIRFLEXCFG_BASE_L3     LS(2, 0, 0x3)
#define   TXGBE_FDIRFLEXCFG_BASE_PAY    LS(3, 0, 0x3)
#define   TXGBE_FDIRFLEXCFG_DIA         MS(2, 0x1)
#define   TXGBE_FDIRFLEXCFG_OFST_MASK   MS(3, 0x1F)
#define   TXGBD_FDIRFLEXCFG_OFST(r)     RS(r, 3, 0x1F)
#define   TXGBE_FDIRFLEXCFG_OFST(v)     LS(v, 3, 0x1F)
#define TXGBE_FDIRBKTHKEY               0x019568
#define TXGBE_FDIRSIGHKEY               0x01956C

/* Common Mask */
#define TXGBE_FDIRDIP4MSK               0x01953C
#define TXGBE_FDIRSIP4MSK               0x019540
#define TXGBE_FDIRIP6MSK                0x019574
#define   TXGBE_FDIRIP6MSK_SRC(v)       LS(v, 0, 0xFFFF)
#define   TXGBE_FDIRIP6MSK_DST(v)       LS(v, 16, 0xFFFF)
#define TXGBE_FDIRTCPMSK                0x019544
#define   TXGBE_FDIRTCPMSK_SRC(v)       LS(v, 0, 0xFFFF)
#define   TXGBE_FDIRTCPMSK_DST(v)       LS(v, 16, 0xFFFF)
#define TXGBE_FDIRUDPMSK                0x019548
#define   TXGBE_FDIRUDPMSK_SRC(v)       LS(v, 0, 0xFFFF)
#define   TXGBE_FDIRUDPMSK_DST(v)       LS(v, 16, 0xFFFF)
#define TXGBE_FDIRSCTPMSK               0x019560
#define   TXGBE_FDIRSCTPMSK_SRC(v)      LS(v, 0, 0xFFFF)
#define   TXGBE_FDIRSCTPMSK_DST(v)      LS(v, 16, 0xFFFF)
#define TXGBE_FDIRMSK                   0x019570
#define   TXGBE_FDIRMSK_POOL            MS(2, 0x1)
#define   TXGBE_FDIRMSK_L4P             MS(3, 0x1)
#define   TXGBE_FDIRMSK_L3P             MS(4, 0x1)
#define   TXGBE_FDIRMSK_TUNTYPE         MS(5, 0x1)
#define   TXGBE_FDIRMSK_TUNIP           MS(6, 0x1)
#define   TXGBE_FDIRMSK_TUNPKT          MS(7, 0x1)

/* Programming Interface */
#define TXGBE_FDIRPIPORT                0x019520
#define   TXGBE_FDIRPIPORT_SRC(v)       LS(v, 0, 0xFFFF)
#define   TXGBE_FDIRPIPORT_DST(v)       LS(v, 16, 0xFFFF)
#define TXGBE_FDIRPISIP6(i)             (0x01950C + (i) * 4) /* [0,2] */
#define TXGBE_FDIRPISIP4                0x019518
#define TXGBE_FDIRPIDIP4                0x01951C
#define TXGBE_FDIRPIFLEX                0x019524
#define   TXGBE_FDIRPIFLEX_PTYPE(v)     LS(v, 0, 0xFF)
#define   TXGBE_FDIRPIFLEX_FLEX(v)      LS(v, 16, 0xFFFF)
#define TXGBE_FDIRPIHASH                0x019528
#define   TXGBE_FDIRPIHASH_BKT(v)       LS(v, 0, 0x7FFF)
#define   TXGBE_FDIRPIHASH_VLD          MS(15, 0x1)
#define   TXGBE_FDIRPIHASH_SIG(v)       LS(v, 16, 0x7FFF)
#define   TXGBE_FDIRPIHASH_IDX(v)       LS(v, 16, 0xFFFF)
#define TXGBE_FDIRPICMD                 0x01952C
#define   TXGBE_FDIRPICMD_OP_MASK       MS(0, 0x3)
#define   TXGBE_FDIRPICMD_OP_ADD        LS(1, 0, 0x3)
#define   TXGBE_FDIRPICMD_OP_REM        LS(2, 0, 0x3)
#define   TXGBE_FDIRPICMD_OP_QRY        LS(3, 0, 0x3)
#define   TXGBE_FDIRPICMD_VLD           MS(2, 0x1)
#define   TXGBE_FDIRPICMD_UPD           MS(3, 0x1)
#define   TXGBE_FDIRPICMD_DIP6          MS(4, 0x1)
#define   TXGBE_FDIRPICMD_FT(v)         LS(v, 5, 0x3)
#define   TXGBE_FDIRPICMD_FT_MASK       MS(5, 0x3)
#define   TXGBE_FDIRPICMD_FT_UDP        LS(1, 5, 0x3)
#define   TXGBE_FDIRPICMD_FT_TCP        LS(2, 5, 0x3)
#define   TXGBE_FDIRPICMD_FT_SCTP       LS(3, 5, 0x3)
#define   TXGBE_FDIRPICMD_IP6           MS(7, 0x1)
#define   TXGBE_FDIRPICMD_CLR           MS(8, 0x1)
#define   TXGBE_FDIRPICMD_DROP          MS(9, 0x1)
#define   TXGBE_FDIRPICMD_LLI           MS(10, 0x1)
#define   TXGBE_FDIRPICMD_LAST          MS(11, 0x1)
#define   TXGBE_FDIRPICMD_COLLI         MS(12, 0x1)
#define   TXGBE_FDIRPICMD_QPENA         MS(15, 0x1)
#define   TXGBE_FDIRPICMD_QP(v)         LS(v, 16, 0x7F)
#define   TXGBE_FDIRPICMD_POOL(v)       LS(v, 24, 0x3F)

/**
 * 5-tuple Filter
 **/
#define TXGBE_5TFSADDR(i)               (0x019600 + (i) * 4) /* 0-127 */
#define TXGBE_5TFDADDR(i)               (0x019800 + (i) * 4) /* 0-127 */
#define TXGBE_5TFPORT(i)                (0x019A00 + (i) * 4) /* 0-127 */
#define   TXGBE_5TFPORT_SRC(v)          LS(v, 0, 0xFFFF)
#define   TXGBE_5TFPORT_DST(v)          LS(v, 16, 0xFFFF)
#define TXGBE_5TFCTL0(i)                (0x019C00 + (i) * 4) /* 0-127 */
#define   TXGBE_5TFCTL0_PROTO(v)        LS(v, 0, 0x3)
enum txgbe_5tuple_protocol {
	TXGBE_5TF_PROT_TCP = 0,
	TXGBE_5TF_PROT_UDP,
	TXGBE_5TF_PROT_SCTP,
	TXGBE_5TF_PROT_NONE,
};
#define   TXGBE_5TFCTL0_PRI(v)          LS(v, 2, 0x7)
#define   TXGBE_5TFCTL0_POOL(v)         LS(v, 8, 0x3F)
#define   TXGBE_5TFCTL0_MASK            MS(25, 0x3F)
#define     TXGBE_5TFCTL0_MSADDR        MS(25, 0x1)
#define     TXGBE_5TFCTL0_MDADDR        MS(26, 0x1)
#define     TXGBE_5TFCTL0_MSPORT        MS(27, 0x1)
#define     TXGBE_5TFCTL0_MDPORT        MS(28, 0x1)
#define     TXGBE_5TFCTL0_MPROTO        MS(29, 0x1)
#define     TXGBE_5TFCTL0_MPOOL         MS(30, 0x1)
#define   TXGBE_5TFCTL0_ENA             MS(31, 0x1)
#define TXGBE_5TFCTL1(i)                (0x019E00 + (i) * 4) /* 0-127 */
#define   TXGBE_5TFCTL1_CHKSZ           MS(12, 0x1)
#define   TXGBE_5TFCTL1_LLI             MS(20, 0x1)
#define   TXGBE_5TFCTL1_QP(v)           LS(v, 21, 0x7F)

/**
 * Storm Control
 **/
#define TXGBE_STRMCTL              0x015004
#define   TXGBE_STRMCTL_MCPNSH     MS(0, 0x1)
#define   TXGBE_STRMCTL_MCDROP     MS(1, 0x1)
#define   TXGBE_STRMCTL_BCPNSH     MS(2, 0x1)
#define   TXGBE_STRMCTL_BCDROP     MS(3, 0x1)
#define   TXGBE_STRMCTL_DFTPOOL    MS(4, 0x1)
#define   TXGBE_STRMCTL_ITVL(v)    LS(v, 8, 0x3FF)
#define TXGBE_STRMTH               0x015008
#define   TXGBE_STRMTH_MC(v)       LS(v, 0, 0xFFFF)
#define   TXGBE_STRMTH_BC(v)       LS(v, 16, 0xFFFF)

/******************************************************************************
 * Ether Flow
 ******************************************************************************/
#define TXGBE_PSRCTL                    0x015000
#define   TXGBE_PSRCTL_TPE              MS(4, 0x1)
#define   TXGBE_PSRCTL_ADHF12_MASK      MS(5, 0x3)
#define   TXGBE_PSRCTL_ADHF12(v)        LS(v, 5, 0x3)
#define   TXGBE_PSRCTL_UCHFENA          MS(7, 0x1)
#define   TXGBE_PSRCTL_MCHFENA          MS(7, 0x1)
#define   TXGBE_PSRCTL_MCP              MS(8, 0x1)
#define   TXGBE_PSRCTL_UCP              MS(9, 0x1)
#define   TXGBE_PSRCTL_BCA              MS(10, 0x1)
#define   TXGBE_PSRCTL_L4CSUM           MS(12, 0x1)
#define   TXGBE_PSRCTL_PCSD             MS(13, 0x1)
#define   TXGBE_PSRCTL_RSCPUSH          MS(15, 0x1)
#define   TXGBE_PSRCTL_RSCDIA           MS(16, 0x1)
#define   TXGBE_PSRCTL_RSCACK           MS(17, 0x1)
#define   TXGBE_PSRCTL_LBENA            MS(18, 0x1)
#define TXGBE_FRMSZ                     0x015020
#define   TXGBE_FRMSZ_MAX_MASK          MS(0, 0xFFFF)
#define   TXGBE_FRMSZ_MAX(v)            LS((v) + 4, 0, 0xFFFF)
#define TXGBE_VLANCTL                   0x015088
#define   TXGBE_VLANCTL_TPID_MASK       MS(0, 0xFFFF)
#define   TXGBE_VLANCTL_TPID(v)         LS(v, 0, 0xFFFF)
#define   TXGBE_VLANCTL_CFI             MS(28, 0x1)
#define   TXGBE_VLANCTL_CFIENA          MS(29, 0x1)
#define   TXGBE_VLANCTL_VFE             MS(30, 0x1)
#define TXGBE_POOLCTL                   0x0151B0
#define   TXGBE_POOLCTL_DEFDSA          MS(29, 0x1)
#define   TXGBE_POOLCTL_RPLEN           MS(30, 0x1)
#define   TXGBE_POOLCTL_MODE_MASK       MS(16, 0x3)
#define     TXGBE_PSRPOOL_MODE_MAC      LS(0, 16, 0x3)
#define     TXGBE_PSRPOOL_MODE_ETAG     LS(1, 16, 0x3)
#define   TXGBE_POOLCTL_DEFPL(v)        LS(v, 7, 0x3F)
#define     TXGBE_POOLCTL_DEFPL_MASK    MS(7, 0x3F)

#define TXGBE_ETFLT(i)                  (0x015128 + (i) * 4) /* 0-7 */
#define   TXGBE_ETFLT_ETID(v)           LS(v, 0, 0xFFFF)
#define   TXGBE_ETFLT_ETID_MASK         MS(0, 0xFFFF)
#define   TXGBE_ETFLT_POOL(v)           LS(v, 20, 0x3FF)
#define   TXGBE_ETFLT_POOLENA           MS(26, 0x1)
#define   TXGBE_ETFLT_FCOE              MS(27, 0x1)
#define   TXGBE_ETFLT_TXAS              MS(29, 0x1)
#define   TXGBE_ETFLT_1588              MS(30, 0x1)
#define   TXGBE_ETFLT_ENA               MS(31, 0x1)
#define TXGBE_ETCLS(i)                  (0x019100 + (i) * 4) /* 0-7 */
#define   TXGBE_ETCLS_QPID(v)           LS(v, 16, 0x7F)
#define   TXGBD_ETCLS_QPID(r)           RS(r, 16, 0x7F)
#define   TXGBE_ETCLS_LLI               MS(29, 0x1)
#define   TXGBE_ETCLS_QENA              MS(31, 0x1)
#define TXGBE_SYNCLS                    0x019130
#define   TXGBE_SYNCLS_ENA              MS(0, 0x1)
#define   TXGBE_SYNCLS_QPID(v)          LS(v, 1, 0x7F)
#define   TXGBD_SYNCLS_QPID(r)          RS(r, 1, 0x7F)
#define   TXGBE_SYNCLS_QPID_MASK        MS(1, 0x7F)
#define   TXGBE_SYNCLS_HIPRIO           MS(31, 0x1)

/* MAC & VLAN & NVE */
#define TXGBE_PSRVLANIDX           0x016230 /* 0-63 */
#define TXGBE_PSRVLAN              0x016220
#define   TXGBE_PSRVLAN_VID(v)     LS(v, 0, 0xFFF)
#define   TXGBE_PSRVLAN_EA         MS(31, 0x1)
#define TXGBE_PSRVLANPLM(i)        (0x016224 + (i) * 4) /* 0-1 */

#define TXGBE_PSRNVEI              0x016260 /* 256 */
#define TXGBE_PSRNVEADDR(i)        (0x016240 + (i) * 4) /* 0-3 */
#define TXGBE_PSRNVE               0x016250
#define   TXGBE_PSRNVE_KEY(v)      LS(v, 0, 0xFFFFFF)
#define   TXGBE_PSRNVE_TYPE(v)     LS(v, 24, 0x3)
#define TXGBE_PSRNVECTL            0x016254
#define   TXGBE_PSRNVECTL_MKEY     MS(0, 0x1)
#define   TXGBE_PSRNVECTL_MADDR    MS(1, 0x1)
#define   TXGBE_PSRNVECTL_SEL(v)   LS(v, 8, 0x3)
#define     TXGBE_PSRNVECTL_SEL_ODIP    (0)
#define     TXGBE_PSRNVECTL_SEL_IDMAC   (1)
#define     TXGBE_PSRNVECTL_SEL_IDIP    (2)
#define   TXGBE_PSRNVECTL_EA       MS(31, 0x1)
#define TXGBE_PSRNVEPM(i)          (0x016258 + (i) * 4) /* 0-1 */

/**
 * FCoE
 **/
#define TXGBE_FCCTL                0x015100
#define   TXGBE_FCCTL_LLI          MS(0, 0x1)
#define   TXGBE_FCCTL_SAVBAD       MS(1, 0x1)
#define   TXGBE_FCCTL_FRSTRDH      MS(2, 0x1)
#define   TXGBE_FCCTL_LSEQH        MS(3, 0x1)
#define   TXGBE_FCCTL_ALLH         MS(4, 0x1)
#define   TXGBE_FCCTL_FSEQH        MS(5, 0x1)
#define   TXGBE_FCCTL_ICRC         MS(6, 0x1)
#define   TXGBE_FCCTL_CRCBO        MS(7, 0x1)
#define   TXGBE_FCCTL_VER(v)       LS(v, 8, 0xF)
#define TXGBE_FCRSSCTL             0x019140
#define   TXGBE_FCRSSCTL_EA        MS(0, 0x1)
#define TXGBE_FCRSSTBL(i)          (0x019160 + (i) * 4) /* 0-7 */
#define   TXGBE_FCRSSTBL_QUE(v)    LS(v, 0, 0x7F)

#define TXGBE_FCRXEOF              0x015158
#define TXGBE_FCRXSOF              0x0151F8
#define TXGBE_FCTXEOF              0x018384
#define TXGBE_FCTXSOF              0x018380
#define TXGBE_FCRXFCDESC(i)        (0x012410 + (i) * 4) /* 0-1 */
#define TXGBE_FCRXFCBUF            0x012418
#define TXGBE_FCRXFCDDP            0x012420
#define TXGBE_FCRXCTXINVL(i)       (0x0190C0 + (i) * 4) /* 0-15 */

/* Programming Interface */
#define TXGBE_FCCTXT               0x015110
#define   TXGBE_FCCTXT_ID(v)       (((v) & 0x1FF)) /* 512 */
#define   TXGBE_FCCTXT_REVA        LS(0x1, 13, 0x1)
#define   TXGBE_FCCTXT_WREA        LS(0x1, 14, 0x1)
#define   TXGBE_FCCTXT_RDEA        LS(0x1, 15, 0x1)
#define TXGBE_FCCTXTCTL            0x015108
#define   TXGBE_FCCTXTCTL_EA       MS(0, 0x1)
#define   TXGBE_FCCTXTCTL_FIRST    MS(1, 0x1)
#define   TXGBE_FCCTXTCTL_WR       MS(2, 0x1)
#define   TXGBE_FCCTXTCTL_SEQID(v) LS(v, 8, 0xFF)
#define   TXGBE_FCCTXTCTL_SEQNR(v) LS(v, 16, 0xFFFF)
#define TXGBE_FCCTXTPARM           0x0151D8

/**
 * Mirror Rules
 **/
#define TXGBE_MIRRCTL(i)           (0x015B00 + (i) * 4)
#define  TXGBE_MIRRCTL_POOL        MS(0, 0x1)
#define  TXGBE_MIRRCTL_UPLINK      MS(1, 0x1)
#define  TXGBE_MIRRCTL_DNLINK      MS(2, 0x1)
#define  TXGBE_MIRRCTL_VLAN        MS(3, 0x1)
#define  TXGBE_MIRRCTL_DESTP(v)    LS(v, 8, 0x3F)
#define TXGBE_MIRRVLANL(i)         (0x015B10 + (i) * 8)
#define TXGBE_MIRRVLANH(i)         (0x015B14 + (i) * 8)
#define TXGBE_MIRRPOOLL(i)         (0x015B30 + (i) * 8)
#define TXGBE_MIRRPOOLH(i)         (0x015B34 + (i) * 8)

/**
 * Time Stamp
 **/
#define TXGBE_TSRXCTL              0x015188
#define   TXGBE_TSRXCTL_VLD        MS(0, 0x1)
#define   TXGBE_TSRXCTL_TYPE(v)    LS(v, 1, 0x7)
#define     TXGBE_TSRXCTL_TYPE_V2L2         (0)
#define     TXGBE_TSRXCTL_TYPE_V1L4         (1)
#define     TXGBE_TSRXCTL_TYPE_V2L24        (2)
#define     TXGBE_TSRXCTL_TYPE_V2EVENT      (5)
#define   TXGBE_TSRXCTL_ENA        MS(4, 0x1)
#define TXGBE_TSRXSTMPL            0x0151E8
#define TXGBE_TSRXSTMPH            0x0151A4
#define TXGBE_TSTXCTL              0x01D400
#define   TXGBE_TSTXCTL_VLD        MS(0, 0x1)
#define   TXGBE_TSTXCTL_ENA        MS(4, 0x1)
#define TXGBE_TSTXSTMPL            0x01D404
#define TXGBE_TSTXSTMPH            0x01D408
#define TXGBE_TSTIMEL              0x01D40C
#define TXGBE_TSTIMEH              0x01D410
#define TXGBE_TSTIMEINC            0x01D414
#define   TXGBE_TSTIMEINC_IV(v)    LS(v, 0, 0xFFFFFF)
#define   TXGBE_TSTIMEINC_IP(v)    LS(v, 24, 0xFF)
#define   TXGBE_TSTIMEINC_VP(v, p) \
			(((v) & MS(0, 0xFFFFFF)) | TXGBE_TSTIMEINC_IP(p))

/**
 * Wake on Lan
 **/
#define TXGBE_WOLCTL               0x015B80
#define TXGBE_WOLIPCTL             0x015B84
#define TXGBE_WOLIP4(i)            (0x015BC0 + (i) * 4) /* 0-3 */
#define TXGBE_WOLIP6(i)            (0x015BE0 + (i) * 4) /* 0-3 */

#define TXGBE_WOLFLEXCTL           0x015CFC
#define TXGBE_WOLFLEXI             0x015B8C
#define TXGBE_WOLFLEXDAT(i)        (0x015C00 + (i) * 16) /* 0-15 */
#define TXGBE_WOLFLEXMSK(i)        (0x015C08 + (i) * 16) /* 0-15 */

/******************************************************************************
 * Security Registers
 ******************************************************************************/
#define TXGBE_SECRXCTL             0x017000
#define   TXGBE_SECRXCTL_ODSA      MS(0, 0x1)
#define   TXGBE_SECRXCTL_XDSA      MS(1, 0x1)
#define   TXGBE_SECRXCTL_CRCSTRIP  MS(2, 0x1)
#define   TXGBE_SECRXCTL_SAVEBAD   MS(6, 0x1)
#define TXGBE_SECRXSTAT            0x017004
#define   TXGBE_SECRXSTAT_RDY      MS(0, 0x1)
#define   TXGBE_SECRXSTAT_ECC      MS(1, 0x1)

#define TXGBE_SECTXCTL             0x01D000
#define   TXGBE_SECTXCTL_ODSA      MS(0, 0x1)
#define   TXGBE_SECTXCTL_XDSA      MS(1, 0x1)
#define   TXGBE_SECTXCTL_STFWD     MS(2, 0x1)
#define   TXGBE_SECTXCTL_MSKIV     MS(3, 0x1)
#define TXGBE_SECTXSTAT            0x01D004
#define   TXGBE_SECTXSTAT_RDY      MS(0, 0x1)
#define   TXGBE_SECTXSTAT_ECC      MS(1, 0x1)
#define TXGBE_SECTXBUFAF           0x01D008
#define TXGBE_SECTXBUFAE           0x01D00C
#define TXGBE_SECTXIFG             0x01D020
#define   TXGBE_SECTXIFG_MIN(v)    LS(v, 0, 0xF)
#define   TXGBE_SECTXIFG_MIN_MASK  MS(0, 0xF)


/**
 * LinkSec
 **/
#define TXGBE_LSECRXCAP	               0x017200
#define TXGBE_LSECRXCTL                0x017204
	/* disabled(0),check(1),strict(2),drop(3) */
#define   TXGBE_LSECRXCTL_MODE_MASK    MS(2, 0x3)
#define   TXGBE_LSECRXCTL_MODE_STRICT  LS(2, 2, 0x3)
#define   TXGBE_LSECRXCTL_POSTHDR      MS(6, 0x1)
#define   TXGBE_LSECRXCTL_REPLAY       MS(7, 0x1)
#define TXGBE_LSECRXSCIL               0x017208
#define TXGBE_LSECRXSCIH               0x01720C
#define TXGBE_LSECRXSA(i)              (0x017210 + (i) * 4) /* 0-1 */
#define TXGBE_LSECRXPN(i)              (0x017218 + (i) * 4) /* 0-1 */
#define TXGBE_LSECRXKEY(n, i)	       (0x017220 + 0x10 * (n) + 4 * (i)) /*0-3*/
#define TXGBE_LSECTXCAP                0x01D200
#define TXGBE_LSECTXCTL                0x01D204
	/* disabled(0), auth(1), auth+encrypt(2) */
#define   TXGBE_LSECTXCTL_MODE_MASK    MS(0, 0x3)
#define   TXGBE_LSECTXCTL_MODE_AUTH    LS(1, 0, 0x3)
#define   TXGBE_LSECTXCTL_MODE_AENC    LS(2, 0, 0x3)
#define   TXGBE_LSECTXCTL_PNTRH_MASK   MS(8, 0xFFFFFF)
#define   TXGBE_LSECTXCTL_PNTRH(v)     LS(v, 8, 0xFFFFFF)
#define TXGBE_LSECTXSCIL               0x01D208
#define TXGBE_LSECTXSCIH               0x01D20C
#define TXGBE_LSECTXSA                 0x01D210
#define TXGBE_LSECTXPN0                0x01D214
#define TXGBE_LSECTXPN1                0x01D218
#define TXGBE_LSECTXKEY0(i)            (0x01D21C + (i) * 4) /* 0-3 */
#define TXGBE_LSECTXKEY1(i)            (0x01D22C + (i) * 4) /* 0-3 */

#define TXGBE_LSECRX_UTPKT             0x017240
#define TXGBE_LSECRX_DECOCT            0x017244
#define TXGBE_LSECRX_VLDOCT            0x017248
#define TXGBE_LSECRX_BTPKT             0x01724C
#define TXGBE_LSECRX_NOSCIPKT          0x017250
#define TXGBE_LSECRX_UNSCIPKT          0x017254
#define TXGBE_LSECRX_UNCHKPKT          0x017258
#define TXGBE_LSECRX_DLYPKT            0x01725C
#define TXGBE_LSECRX_LATEPKT           0x017260
#define TXGBE_LSECRX_OKPKT(i)          (0x017264 + (i) * 4) /* 0-1 */
#define TXGBE_LSECRX_BADPKT(i)         (0x01726C + (i) * 4) /* 0-1 */
#define TXGBE_LSECRX_INVPKT(i)         (0x017274 + (i) * 4) /* 0-1 */
#define TXGBE_LSECRX_BADSAPKT          0x01727C
#define TXGBE_LSECRX_INVSAPKT          0x017280
#define TXGBE_LSECTX_UTPKT             0x01D23C
#define TXGBE_LSECTX_ENCPKT            0x01D240
#define TXGBE_LSECTX_PROTPKT           0x01D244
#define TXGBE_LSECTX_ENCOCT            0x01D248
#define TXGBE_LSECTX_PROTOCT           0x01D24C

/**
 * IpSec
 **/
#define TXGBE_ISECRXIDX            0x017100
#define TXGBE_ISECRXADDR(i)        (0x017104 + (i) * 4) /*0-3*/
#define TXGBE_ISECRXSPI            0x017114
#define TXGBE_ISECRXIPIDX          0x017118
#define TXGBE_ISECRXKEY(i)         (0x01711C + (i) * 4) /*0-3*/
#define TXGBE_ISECRXSALT           0x01712C
#define TXGBE_ISECRXMODE           0x017130

#define TXGBE_ISECTXIDX            0x01D100
#define   TXGBE_ISECTXIDX_WT       0x80000000U
#define   TXGBE_ISECTXIDX_RD       0x40000000U
#define   TXGBE_ISECTXIDX_SDIDX    0x0U
#define   TXGBE_ISECTXIDX_ENA      0x00000001U

#define TXGBE_ISECTXSALT           0x01D104
#define TXGBE_ISECTXKEY(i)         (0x01D108 + (i) * 4) /* 0-3 */

/******************************************************************************
 * MAC Registers
 ******************************************************************************/
#define TXGBE_MACRXCFG                  0x011004
#define   TXGBE_MACRXCFG_ENA            MS(0, 0x1)
#define   TXGBE_MACRXCFG_JUMBO          MS(8, 0x1)
#define   TXGBE_MACRXCFG_LB             MS(10, 0x1)
#define TXGBE_MACCNTCTL                 0x011800
#define   TXGBE_MACCNTCTL_RC            MS(2, 0x1)

#define TXGBE_MACRXFLT                  0x011008
#define   TXGBE_MACRXFLT_PROMISC        MS(0, 0x1)
#define   TXGBE_MACRXFLT_CTL_MASK       MS(6, 0x3)
#define   TXGBE_MACRXFLT_CTL_DROP       LS(0, 6, 0x3)
#define   TXGBE_MACRXFLT_CTL_NOPS       LS(1, 6, 0x3)
#define   TXGBE_MACRXFLT_CTL_NOFT       LS(2, 6, 0x3)
#define   TXGBE_MACRXFLT_CTL_PASS       LS(3, 6, 0x3)
#define   TXGBE_MACRXFLT_RXALL          MS(31, 0x1)

/******************************************************************************
 * Statistic Registers
 ******************************************************************************/
/* Ring Counter */
#define TXGBE_QPRXPKT(rp)                 (0x001014 + 0x40 * (rp))
#define TXGBE_QPRXOCTL(rp)                (0x001018 + 0x40 * (rp))
#define TXGBE_QPRXOCTH(rp)                (0x00101C + 0x40 * (rp))
#define TXGBE_QPTXPKT(rp)                 (0x003014 + 0x40 * (rp))
#define TXGBE_QPTXOCTL(rp)                (0x003018 + 0x40 * (rp))
#define TXGBE_QPTXOCTH(rp)                (0x00301C + 0x40 * (rp))
#define TXGBE_QPRXMPKT(rp)                (0x001020 + 0x40 * (rp))

/* Host DMA Counter */
#define TXGBE_DMATXDROP                   0x018300
#define TXGBE_DMATXSECDROP                0x018304
#define TXGBE_DMATXPKT                    0x018308
#define TXGBE_DMATXOCTL                   0x01830C
#define TXGBE_DMATXOCTH                   0x018310
#define TXGBE_DMATXMNG                    0x018314
#define TXGBE_DMARXDROP                   0x012500
#define TXGBE_DMARXPKT                    0x012504
#define TXGBE_DMARXOCTL                   0x012508
#define TXGBE_DMARXOCTH                   0x01250C
#define TXGBE_DMARXMNG                    0x012510

/* Packet Buffer Counter */
#define TXGBE_PBRXMISS(tc)                (0x019040 + (tc) * 4)
#define TXGBE_PBRXPKT                     0x019060
#define TXGBE_PBRXREP                     0x019064
#define TXGBE_PBRXDROP                    0x019068
#define TXGBE_PBRXLNKXOFF                 0x011988
#define TXGBE_PBRXLNKXON                  0x011E0C
#define TXGBE_PBRXUPXON(up)               (0x011E30 + (up) * 4)
#define TXGBE_PBRXUPXOFF(up)              (0x011E10 + (up) * 4)

#define TXGBE_PBTXLNKXOFF                 0x019218
#define TXGBE_PBTXLNKXON                  0x01921C
#define TXGBE_PBTXUPXON(up)               (0x0192E0 + (up) * 4)
#define TXGBE_PBTXUPXOFF(up)              (0x0192C0 + (up) * 4)
#define TXGBE_PBTXUPOFF(up)               (0x019280 + (up) * 4)

#define TXGBE_PBLPBK                      0x01CF08

/* Ether Flow Counter */
#define TXGBE_LANPKTDROP                  0x0151C0
#define TXGBE_MNGPKTDROP                  0x0151C4

/* MAC Counter */
#define TXGBE_MACRXERRCRCL           0x011928
#define TXGBE_MACRXERRCRCH           0x01192C
#define TXGBE_MACRXERRLENL           0x011978
#define TXGBE_MACRXERRLENH           0x01197C
#define TXGBE_MACRX1TO64L            0x011940
#define TXGBE_MACRX1TO64H            0x011944
#define TXGBE_MACRX65TO127L          0x011948
#define TXGBE_MACRX65TO127H          0x01194C
#define TXGBE_MACRX128TO255L         0x011950
#define TXGBE_MACRX128TO255H         0x011954
#define TXGBE_MACRX256TO511L         0x011958
#define TXGBE_MACRX256TO511H         0x01195C
#define TXGBE_MACRX512TO1023L        0x011960
#define TXGBE_MACRX512TO1023H        0x011964
#define TXGBE_MACRX1024TOMAXL        0x011968
#define TXGBE_MACRX1024TOMAXH        0x01196C
#define TXGBE_MACTX1TO64L            0x011834
#define TXGBE_MACTX1TO64H            0x011838
#define TXGBE_MACTX65TO127L          0x01183C
#define TXGBE_MACTX65TO127H          0x011840
#define TXGBE_MACTX128TO255L         0x011844
#define TXGBE_MACTX128TO255H         0x011848
#define TXGBE_MACTX256TO511L         0x01184C
#define TXGBE_MACTX256TO511H         0x011850
#define TXGBE_MACTX512TO1023L        0x011854
#define TXGBE_MACTX512TO1023H        0x011858
#define TXGBE_MACTX1024TOMAXL        0x01185C
#define TXGBE_MACTX1024TOMAXH        0x011860

#define TXGBE_MACRXUNDERSIZE         0x011938
#define TXGBE_MACRXOVERSIZE          0x01193C
#define TXGBE_MACRXJABBER            0x011934

#define TXGBE_MACRXPKTL                0x011900
#define TXGBE_MACRXPKTH                0x011904
#define TXGBE_MACTXPKTL                0x01181C
#define TXGBE_MACTXPKTH                0x011820
#define TXGBE_MACRXGBOCTL              0x011908
#define TXGBE_MACRXGBOCTH              0x01190C
#define TXGBE_MACTXGBOCTL              0x011814
#define TXGBE_MACTXGBOCTH              0x011818

#define TXGBE_MACRXOCTL                0x011918
#define TXGBE_MACRXOCTH                0x01191C
#define TXGBE_MACRXMPKTL               0x011920
#define TXGBE_MACRXMPKTH               0x011924
#define TXGBE_MACTXOCTL                0x011824
#define TXGBE_MACTXOCTH                0x011828
#define TXGBE_MACTXMPKTL               0x01182C
#define TXGBE_MACTXMPKTH               0x011830

/* Management Counter */
#define TXGBE_MNGOUT              0x01CF00
#define TXGBE_MNGIN               0x01CF04

/* MAC SEC Counter */
#define TXGBE_LSECRXUNTAG         0x017240
#define TXGBE_LSECRXDECOCT        0x017244
#define TXGBE_LSECRXVLDOCT        0x017248
#define TXGBE_LSECRXBADTAG        0x01724C
#define TXGBE_LSECRXNOSCI         0x017250
#define TXGBE_LSECRXUKSCI         0x017254
#define TXGBE_LSECRXUNCHK         0x017258
#define TXGBE_LSECRXDLY           0x01725C
#define TXGBE_LSECRXLATE          0x017260
#define TXGBE_LSECRXGOOD          0x017264
#define TXGBE_LSECRXBAD           0x01726C
#define TXGBE_LSECRXUK            0x017274
#define TXGBE_LSECRXBADSA         0x01727C
#define TXGBE_LSECRXUKSA          0x017280
#define TXGBE_LSECTXUNTAG         0x01D23C
#define TXGBE_LSECTXENC           0x01D240
#define TXGBE_LSECTXPTT           0x01D244
#define TXGBE_LSECTXENCOCT        0x01D248
#define TXGBE_LSECTXPTTOCT        0x01D24C

/* IP SEC Counter */

/* FDIR Counter */
#define TXGBE_FDIRFREE                  0x019538
#define   TXGBE_FDIRFREE_FLT(r)         RS(r, 0, 0xFFFF)
#define TXGBE_FDIRLEN                   0x01954C
#define   TXGBE_FDIRLEN_BKTLEN(r)       RS(r, 0, 0x3F)
#define   TXGBE_FDIRLEN_MAXLEN(r)       RS(r, 8, 0x3F)
#define TXGBE_FDIRUSED                  0x019550
#define   TXGBE_FDIRUSED_ADD(r)         RS(r, 0, 0xFFFF)
#define   TXGBE_FDIRUSED_REM(r)         RS(r, 16, 0xFFFF)
#define TXGBE_FDIRFAIL                  0x019554
#define   TXGBE_FDIRFAIL_ADD(r)         RS(r, 0, 0xFF)
#define   TXGBE_FDIRFAIL_REM(r)         RS(r, 8, 0xFF)
#define TXGBE_FDIRMATCH                 0x019558
#define TXGBE_FDIRMISS                  0x01955C

/* FCOE Counter */
#define TXGBE_FCOECRC                   0x015160
#define TXGBE_FCOERPDC                  0x012514
#define TXGBE_FCOELAST                  0x012518
#define TXGBE_FCOEPRC                   0x015164
#define TXGBE_FCOEDWRC                  0x015168
#define TXGBE_FCOEPTC                   0x018318
#define TXGBE_FCOEDWTC                  0x01831C

/* Management Counter */
#define TXGBE_MNGOS2BMC                 0x01E094
#define TXGBE_MNGBMC2OS                 0x01E090

/******************************************************************************
 * PF(Physical Function) Registers
 ******************************************************************************/
/* Interrupt */
#define TXGBE_ICRMISC          0x000100
#define   TXGBE_ICRMISC_MASK   MS(8, 0xFFFFFF)
#define   TXGBE_ICRMISC_LNKDN  MS(8, 0x1) /* eth link down */
#define   TXGBE_ICRMISC_RST    MS(10, 0x1) /* device reset event */
#define   TXGBE_ICRMISC_TS     MS(11, 0x1) /* time sync */
#define   TXGBE_ICRMISC_STALL  MS(12, 0x1) /* trans or recv path is stalled */
#define   TXGBE_ICRMISC_LNKSEC MS(13, 0x1) /* Tx LinkSec require key exchange */
#define   TXGBE_ICRMISC_ERRBUF MS(14, 0x1) /* Packet Buffer Overrun */
#define   TXGBE_ICRMISC_FDIR   MS(15, 0x1) /* FDir Exception */
#define   TXGBE_ICRMISC_I2C    MS(16, 0x1) /* I2C interrupt */
#define   TXGBE_ICRMISC_ERRMAC MS(17, 0x1) /* err reported by MAC */
#define   TXGBE_ICRMISC_LNKUP  MS(18, 0x1) /* link up */
#define   TXGBE_ICRMISC_ANDONE MS(19, 0x1) /* link auto-nego done */
#define   TXGBE_ICRMISC_ERRIG  MS(20, 0x1) /* integrity error */
#define   TXGBE_ICRMISC_SPI    MS(21, 0x1) /* SPI interface */
#define   TXGBE_ICRMISC_VFMBX  MS(22, 0x1) /* VF-PF message box */
#define   TXGBE_ICRMISC_GPIO   MS(26, 0x1) /* GPIO interrupt */
#define   TXGBE_ICRMISC_ERRPCI MS(27, 0x1) /* pcie request error */
#define   TXGBE_ICRMISC_HEAT   MS(28, 0x1) /* overheat detection */
#define   TXGBE_ICRMISC_PROBE  MS(29, 0x1) /* probe match */
#define   TXGBE_ICRMISC_MNGMBX MS(30, 0x1) /* mng mailbox */
#define   TXGBE_ICRMISC_TIMER  MS(31, 0x1) /* tcp timer */
#define   TXGBE_ICRMISC_DEFAULT ( \
			TXGBE_ICRMISC_LNKDN | \
			TXGBE_ICRMISC_RST | \
			TXGBE_ICRMISC_ERRMAC | \
			TXGBE_ICRMISC_LNKUP | \
			TXGBE_ICRMISC_ANDONE | \
			TXGBE_ICRMISC_ERRIG | \
			TXGBE_ICRMISC_VFMBX | \
			TXGBE_ICRMISC_MNGMBX | \
			TXGBE_ICRMISC_STALL | \
			TXGBE_ICRMISC_TIMER)
#define   TXGBE_ICRMISC_LSC ( \
			TXGBE_ICRMISC_LNKDN | \
			TXGBE_ICRMISC_LNKUP)
#define TXGBE_ICSMISC                   0x000104
#define TXGBE_IENMISC                   0x000108
#define TXGBE_IVARMISC                  0x0004FC
#define   TXGBE_IVARMISC_VEC(v)         LS(v, 0, 0x7)
#define   TXGBE_IVARMISC_VLD            MS(7, 0x1)
#define TXGBE_PX_INTA			0x000110
#define TXGBE_ICR(i)                    (0x000120 + (i) * 4) /* 0-1 */
#define   TXGBE_ICR_MASK                MS(0, 0xFFFFFFFF)
#define TXGBE_ICS(i)                    (0x000130 + (i) * 4) /* 0-1 */
#define   TXGBE_ICS_MASK                TXGBE_ICR_MASK
#define TXGBE_IMS(i)                    (0x000140 + (i) * 4) /* 0-1 */
#define   TXGBE_IMS_MASK                TXGBE_ICR_MASK
#define TXGBE_IMC(i)                    (0x000150 + (i) * 4) /* 0-1 */
#define   TXGBE_IMC_MASK                TXGBE_ICR_MASK
#define TXGBE_IVAR(i)                   (0x000500 + (i) * 4) /* 0-3 */
#define   TXGBE_IVAR_VEC(v)             LS(v, 0, 0x7)
#define   TXGBE_IVAR_VLD                MS(7, 0x1)
#define TXGBE_TCPTMR                    0x000170
#define TXGBE_ITRSEL                    0x000180

/* P2V Mailbox */
#define TXGBE_MBMEM(i)           (0x005000 + 0x40 * (i)) /* 0-63 */
#define TXGBE_MBCTL(i)           (0x000600 + 4 * (i)) /* 0-63 */
#define   TXGBE_MBCTL_STS        MS(0, 0x1) /* Initiate message send to VF */
#define   TXGBE_MBCTL_ACK        MS(1, 0x1) /* Ack message recv'd from VF */
#define   TXGBE_MBCTL_VFU        MS(2, 0x1) /* VF owns the mailbox buffer */
#define   TXGBE_MBCTL_PFU        MS(3, 0x1) /* PF owns the mailbox buffer */
#define   TXGBE_MBCTL_RVFU       MS(4, 0x1) /* Reset VFU - used when VF stuck */
#define TXGBE_MBVFICR(i)                (0x000480 + 4 * (i)) /* 0-3 */
#define   TXGBE_MBVFICR_INDEX(vf)       ((vf) >> 4)
#define   TXGBE_MBVFICR_VFREQ_MASK      (0x0000FFFF) /* bits for VF messages */
#define   TXGBE_MBVFICR_VFREQ_VF1       (0x00000001) /* bit for VF 1 message */
#define   TXGBE_MBVFICR_VFACK_MASK      (0xFFFF0000) /* bits for VF acks */
#define   TXGBE_MBVFICR_VFACK_VF1       (0x00010000) /* bit for VF 1 ack */
#define TXGBE_FLRVFP(i)                 (0x000490 + 4 * (i)) /* 0-1 */
#define TXGBE_FLRVFE(i)                 (0x0004A0 + 4 * (i)) /* 0-1 */
#define TXGBE_FLRVFEC(i)                (0x0004A8 + 4 * (i)) /* 0-1 */

/******************************************************************************
 * VF(Virtual Function) Registers
 ******************************************************************************/
#define TXGBE_VFPBWRAP                  0x000000
#define   TXGBE_VFPBWRAP_WRAP(r, tc)    ((0x7 << 4 * (tc) & (r)) >> 4 * (tc))
#define   TXGBE_VFPBWRAP_EMPT(r, tc)    ((0x8 << 4 * (tc) & (r)) >> 4 * (tc))
#define TXGBE_VFSTATUS                  0x000004
#define   TXGBE_VFSTATUS_UP             MS(0, 0x1)
#define   TXGBE_VFSTATUS_BW_MASK        MS(1, 0x7)
#define     TXGBE_VFSTATUS_BW_10G       LS(0x1, 1, 0x7)
#define     TXGBE_VFSTATUS_BW_1G        LS(0x2, 1, 0x7)
#define     TXGBE_VFSTATUS_BW_100M      LS(0x4, 1, 0x7)
#define   TXGBE_VFSTATUS_BUSY           MS(4, 0x1)
#define   TXGBE_VFSTATUS_LANID          MS(8, 0x1)
#define TXGBE_VFRST                     0x000008
#define   TXGBE_VFRST_SET               MS(0, 0x1)
#define TXGBE_VFPLCFG                   0x000078
#define   TXGBE_VFPLCFG_RSV             MS(0, 0x1)
#define   TXGBE_VFPLCFG_PSR(v)          LS(v, 1, 0x1F)
#define     TXGBE_VFPLCFG_PSRL4HDR      (0x1)
#define     TXGBE_VFPLCFG_PSRL3HDR      (0x2)
#define     TXGBE_VFPLCFG_PSRL2HDR      (0x4)
#define     TXGBE_VFPLCFG_PSRTUNHDR     (0x8)
#define     TXGBE_VFPLCFG_PSRTUNMAC     (0x10)
#define   TXGBE_VFPLCFG_RSSMASK         MS(16, 0xFF)
#define   TXGBE_VFPLCFG_RSSIPV4TCP      MS(16, 0x1)
#define   TXGBE_VFPLCFG_RSSIPV4         MS(17, 0x1)
#define   TXGBE_VFPLCFG_RSSIPV6         MS(20, 0x1)
#define   TXGBE_VFPLCFG_RSSIPV6TCP      MS(21, 0x1)
#define   TXGBE_VFPLCFG_RSSIPV4UDP      MS(22, 0x1)
#define   TXGBE_VFPLCFG_RSSIPV6UDP      MS(23, 0x1)
#define   TXGBE_VFPLCFG_RSSENA          MS(24, 0x1)
#define   TXGBE_VFPLCFG_RSSHASH(v)      LS(v, 29, 0x7)
#define TXGBE_VFRSSKEY(i)               (0x000080 + (i) * 4) /* 0-9 */
#define TXGBE_VFRSSTBL(i)               (0x0000C0 + (i) * 4) /* 0-15 */
#define TXGBE_VFICR                     0x000100
#define   TXGBE_VFICR_MASK              LS(7, 0, 0x7)
#define   TXGBE_VFICR_MBX               MS(0, 0x1)
#define   TXGBE_VFICR_DONE1             MS(1, 0x1)
#define   TXGBE_VFICR_DONE2             MS(2, 0x1)
#define TXGBE_VFICS                     0x000104
#define   TXGBE_VFICS_MASK              TXGBE_VFICR_MASK
#define TXGBE_VFIMS                     0x000108
#define   TXGBE_VFIMS_MASK              TXGBE_VFICR_MASK
#define TXGBE_VFIMC                     0x00010C
#define   TXGBE_VFIMC_MASK              TXGBE_VFICR_MASK
#define TXGBE_VFGPIE                    0x000118
#define TXGBE_VFIVAR(i)                 (0x000240 + 4 * (i)) /* 0-3 */
#define TXGBE_VFIVARMISC                0x000260
#define   TXGBE_VFIVAR_ALLOC(v)         LS(v, 0, 0x3)
#define   TXGBE_VFIVAR_VLD              MS(7, 0x1)

#define TXGBE_VFMBCTL                   0x000600
#define   TXGBE_VFMBCTL_REQ     MS(0, 0x1) /* Request for PF Ready bit */
#define   TXGBE_VFMBCTL_ACK     MS(1, 0x1) /* Ack PF message received */
#define   TXGBE_VFMBCTL_VFU     MS(2, 0x1) /* VF owns the mailbox buffer */
#define   TXGBE_VFMBCTL_PFU     MS(3, 0x1) /* PF owns the mailbox buffer */
#define   TXGBE_VFMBCTL_PFSTS   MS(4, 0x1) /* PF wrote a message in the MB */
#define   TXGBE_VFMBCTL_PFACK   MS(5, 0x1) /* PF ack the previous VF msg */
#define   TXGBE_VFMBCTL_RSTI    MS(6, 0x1) /* PF has reset indication */
#define   TXGBE_VFMBCTL_RSTD    MS(7, 0x1) /* PF has indicated reset done */
#define   TXGBE_VFMBCTL_R2C_BITS        (TXGBE_VFMBCTL_RSTD | \
					 TXGBE_VFMBCTL_PFSTS | \
					 TXGBE_VFMBCTL_PFACK)
#define TXGBE_VFMBX                     0x000C00 /* 0-15 */
#define TXGBE_VFTPHCTL(i)               (0x000D00 + 4 * (i)) /* 0-7 */

/******************************************************************************
 * PF&VF TxRx Interface
 ******************************************************************************/
#define RNGLEN(v)     ROUND_OVER(v, 13, 7)
#define HDRLEN(v)     ROUND_OVER(v, 10, 6)
#define PKTLEN(v)     ROUND_OVER(v, 14, 10)
#define INTTHR(v)     ROUND_OVER(v, 4,  0)

#define	TXGBE_RING_DESC_ALIGN	128
#define	TXGBE_RING_DESC_MIN	128
#define	TXGBE_RING_DESC_MAX	8192
#define TXGBE_RXD_ALIGN		TXGBE_RING_DESC_ALIGN
#define TXGBE_TXD_ALIGN		TXGBE_RING_DESC_ALIGN

/* receive ring */
#define TXGBE_RXBAL(rp)                 (0x001000 + 0x40 * (rp))
#define TXGBE_RXBAH(rp)                 (0x001004 + 0x40 * (rp))
#define TXGBE_RXRP(rp)                  (0x00100C + 0x40 * (rp))
#define TXGBE_RXWP(rp)                  (0x001008 + 0x40 * (rp))
#define TXGBE_RXCFG(rp)                 (0x001010 + 0x40 * (rp))
#define   TXGBE_RXCFG_ENA               MS(0, 0x1)
#define   TXGBE_RXCFG_RNGLEN(v)         LS(RNGLEN(v), 1, 0x3F)
#define   TXGBE_RXCFG_PKTLEN(v)         LS(PKTLEN(v), 8, 0xF)
#define     TXGBE_RXCFG_PKTLEN_MASK     MS(8, 0xF)
#define   TXGBE_RXCFG_HDRLEN(v)         LS(HDRLEN(v), 12, 0xF)
#define     TXGBE_RXCFG_HDRLEN_MASK     MS(12, 0xF)
#define   TXGBE_RXCFG_WTHRESH(v)        LS(v, 16, 0x7)
#define   TXGBE_RXCFG_ETAG              MS(22, 0x1)
#define   TXGBE_RXCFG_RSCMAX_MASK       MS(23, 0x3)
#define     TXGBE_RXCFG_RSCMAX_1        LS(0, 23, 0x3)
#define     TXGBE_RXCFG_RSCMAX_4        LS(1, 23, 0x3)
#define     TXGBE_RXCFG_RSCMAX_8        LS(2, 23, 0x3)
#define     TXGBE_RXCFG_RSCMAX_16       LS(3, 23, 0x3)
#define   TXGBE_RXCFG_STALL             MS(25, 0x1)
#define   TXGBE_RXCFG_SPLIT             MS(26, 0x1)
#define   TXGBE_RXCFG_RSCMODE           MS(27, 0x1)
#define   TXGBE_RXCFG_CNTAG             MS(28, 0x1)
#define   TXGBE_RXCFG_RSCENA            MS(29, 0x1)
#define   TXGBE_RXCFG_DROP              MS(30, 0x1)
#define   TXGBE_RXCFG_VLAN              MS(31, 0x1)

/* transmit ring */
#define TXGBE_TXBAL(rp)                 (0x003000 + 0x40 * (rp))
#define TXGBE_TXBAH(rp)                 (0x003004 + 0x40 * (rp))
#define TXGBE_TXWP(rp)                  (0x003008 + 0x40 * (rp))
#define TXGBE_TXRP(rp)                  (0x00300C + 0x40 * (rp))
#define TXGBE_TXCFG(rp)                 (0x003010 + 0x40 * (rp))
#define   TXGBE_TXCFG_ENA               MS(0, 0x1)
#define   TXGBE_TXCFG_BUFLEN_MASK       MS(1, 0x3F)
#define   TXGBE_TXCFG_BUFLEN(v)         LS(RNGLEN(v), 1, 0x3F)
#define   TXGBE_TXCFG_HTHRESH_MASK      MS(8, 0xF)
#define   TXGBE_TXCFG_HTHRESH(v)        LS(v, 8, 0xF)
#define   TXGBE_TXCFG_WTHRESH_MASK      MS(16, 0x7F)
#define   TXGBE_TXCFG_WTHRESH(v)        LS(v, 16, 0x7F)
#define   TXGBE_TXCFG_FLUSH             MS(26, 0x1)

/* interrupt registers */
#define TXGBE_ITRI                      0x000180
#define TXGBE_ITR(i)                    (0x000200 + 4 * (i))
#define   TXGBE_ITR_IVAL_MASK           MS(2, 0x3FE)
#define   TXGBE_ITR_IVAL(v)             LS(v, 2, 0x3FE)
#define     TXGBE_ITR_IVAL_1G(us)       TXGBE_ITR_IVAL((us) / 2)
#define     TXGBE_ITR_IVAL_10G(us)      TXGBE_ITR_IVAL((us) / 20)
#define   TXGBE_ITR_LLIEA               MS(15, 0x1)
#define   TXGBE_ITR_LLICREDIT(v)        LS(v, 16, 0x1F)
#define   TXGBE_ITR_CNT(v)              LS(v, 21, 0x7F)
#define   TXGBE_ITR_WRDSA               MS(31, 0x1)
#define TXGBE_GPIE                      0x000118
#define   TXGBE_GPIE_MSIX               MS(0, 0x1)
#define   TXGBE_GPIE_LLIEA              MS(1, 0x1)
#define   TXGBE_GPIE_LLIVAL(v)          LS(v, 4, 0xF)
#define   TXGBE_GPIE_RSCDLY(v)          LS(v, 8, 0x7)

/******************************************************************************
 * Debug Registers
 ******************************************************************************/
/**
 * Probe
 **/
#define TXGBE_PROB                      0x010010
#define TXGBE_IODRV                     0x010024

#define TXGBE_PRBCTL                    0x010200
#define TXGBE_PRBSTA                    0x010204
#define TXGBE_PRBDAT                    0x010220
#define TXGBE_PRBPTN                    0x010224
#define TXGBE_PRBCNT                    0x010228
#define TXGBE_PRBMSK                    0x01022C

#define TXGBE_PRBPCI                    0x01F010
#define TXGBE_PRBRDMA                   0x012010
#define TXGBE_PRBTDMA                   0x018010
#define TXGBE_PRBPSR                    0x015010
#define TXGBE_PRBRDB                    0x019010
#define TXGBE_PRBTDB                    0x01C010
#define TXGBE_PRBRSEC                   0x017010
#define TXGBE_PRBTSEC                   0x01D010
#define TXGBE_PRBMNG                    0x01E010
#define TXGBE_PRBRMAC                   0x011014
#define TXGBE_PRBTMAC                   0x011010
#define TXGBE_PRBREMAC                  0x011E04
#define TXGBE_PRBTEMAC                  0x011E00

/**
 * ECC
 **/
#define TXGBE_ECCRXDMACTL               0x012014
#define TXGBE_ECCRXDMAINJ               0x012018
#define TXGBE_ECCRXDMA                  0x01201C
#define TXGBE_ECCTXDMACTL               0x018014
#define TXGBE_ECCTXDMAINJ               0x018018
#define TXGBE_ECCTXDMA                  0x01801C

#define TXGBE_ECCRXPBCTL                0x019014
#define TXGBE_ECCRXPBINJ                0x019018
#define TXGBE_ECCRXPB                   0x01901C
#define TXGBE_ECCTXPBCTL                0x01C014
#define TXGBE_ECCTXPBINJ                0x01C018
#define TXGBE_ECCTXPB                   0x01C01C

#define TXGBE_ECCRXETHCTL               0x015014
#define TXGBE_ECCRXETHINJ               0x015018
#define TXGBE_ECCRXETH                  0x01401C

#define TXGBE_ECCRXSECCTL               0x017014
#define TXGBE_ECCRXSECINJ               0x017018
#define TXGBE_ECCRXSEC                  0x01701C
#define TXGBE_ECCTXSECCTL               0x01D014
#define TXGBE_ECCTXSECINJ               0x01D018
#define TXGBE_ECCTXSEC                  0x01D01C

/**
 * Inspection
 **/
#define TXGBE_PBLBSTAT                  0x01906C
#define   TXGBE_PBLBSTAT_FREE(r)        RS(r, 0, 0x3FF)
#define   TXGBE_PBLBSTAT_FULL           MS(11, 0x1)
#define TXGBE_PBRXSTAT                  0x019004
#define   TXGBE_PBRXSTAT_WRAP(tc, r)    ((7u << 4 * (tc) & (r)) >> 4 * (tc))
#define   TXGBE_PBRXSTAT_EMPT(tc, r)    ((8u << 4 * (tc) & (r)) >> 4 * (tc))
#define TXGBE_PBRXSTAT2(tc)             (0x019180 + (tc) * 4)
#define   TXGBE_PBRXSTAT2_USED(r)       RS(r, 0, 0xFFFF)
#define TXGBE_PBRXWRPTR(tc)             (0x019180 + (tc) * 4)
#define   TXGBE_PBRXWRPTR_HEAD(r)       RS(r, 0, 0xFFFF)
#define   TXGBE_PBRXWRPTR_TAIL(r)       RS(r, 16, 0xFFFF)
#define TXGBE_PBRXRDPTR(tc)             (0x0191A0 + (tc) * 4)
#define   TXGBE_PBRXRDPTR_HEAD(r)       RS(r, 0, 0xFFFF)
#define   TXGBE_PBRXRDPTR_TAIL(r)       RS(r, 16, 0xFFFF)
#define TXGBE_PBRXDATA(tc)              (0x0191C0 + (tc) * 4)
#define   TXGBE_PBRXDATA_RDPTR(r)       RS(r, 0, 0xFFFF)
#define   TXGBE_PBRXDATA_WRPTR(r)       RS(r, 16, 0xFFFF)
#define TXGBE_PBTXSTAT                  0x01C004
#define   TXGBE_PBTXSTAT_EMPT(tc, r)    ((1 << (tc) & (r)) >> (tc))

#define TXGBE_RXPBPFCDMACL              0x019210
#define TXGBE_RXPBPFCDMACH              0x019214

#define TXGBE_PSRLANPKTCNT              0x0151B8
#define TXGBE_PSRMNGPKTCNT              0x0151BC

#define TXGBE_P2VMBX_SIZE          (16) /* 16*4B */
#define TXGBE_P2MMBX_SIZE          (64) /* 64*4B */

/**************** Global Registers ****************************/
/* chip control Registers */
#define TXGBE_PWR                       0x010000
#define   TXGBE_PWR_LANID(r)            RS(r, 30, 0x3)
#define   TXGBE_PWR_LANID_SWAP          LS(2, 30, 0x3)

/* Sensors for PVT(Process Voltage Temperature) */
#define TXGBE_TSCTRL                    0x010300
#define   TXGBE_TSCTRL_EVALMD           MS(31, 0x1)
#define TXGBE_TSEN                      0x010304
#define   TXGBE_TSEN_ENA                MS(0, 0x1)
#define TXGBE_TSSTAT                    0x010308
#define   TXGBE_TSSTAT_VLD              MS(16, 0x1)
#define   TXGBE_TSSTAT_DATA(r)          RS(r, 0, 0x3FF)

#define TXGBE_TSATHRE                   0x01030C
#define TXGBE_TSDTHRE                   0x010310
#define TXGBE_TSINTR                    0x010314
#define   TXGBE_TSINTR_AEN              MS(0, 0x1)
#define   TXGBE_TSINTR_DEN              MS(1, 0x1)
#define TXGBE_TS_ALARM_ST               0x10318
#define TXGBE_TS_ALARM_ST_DALARM        0x00000002U
#define TXGBE_TS_ALARM_ST_ALARM         0x00000001U

/* FMGR Registers */
#define TXGBE_ILDRSTAT                  0x010120
#define   TXGBE_ILDRSTAT_PCIRST         MS(0, 0x1)
#define   TXGBE_ILDRSTAT_PWRRST         MS(1, 0x1)
#define   TXGBE_ILDRSTAT_SWRST          MS(7, 0x1)
#define   TXGBE_ILDRSTAT_SWRST_LAN0     MS(9, 0x1)
#define   TXGBE_ILDRSTAT_SWRST_LAN1     MS(10, 0x1)

#define TXGBE_SPISTAT                   0x01010C
#define   TXGBE_SPISTAT_OPDONE          MS(0, 0x1)
#define   TXGBE_SPISTAT_BPFLASH         MS(31, 0x1)

/************************* Port Registers ************************************/
/* I2C registers */
#define TXGBE_I2CCON                 0x014900 /* I2C Control */
#define   TXGBE_I2CCON_SDIA          ((1 << 6))
#define   TXGBE_I2CCON_RESTART       ((1 << 5))
#define   TXGBE_I2CCON_M10BITADDR    ((1 << 4))
#define   TXGBE_I2CCON_S10BITADDR    ((1 << 3))
#define   TXGBE_I2CCON_SPEED(v)      (((v) & 0x3) << 1)
#define   TXGBE_I2CCON_MENA          ((1 << 0))
#define TXGBE_I2CTAR                 0x014904 /* I2C Target Address */
#define TXGBE_I2CDATA                0x014910 /* I2C Rx/Tx Data Buf and Cmd */
#define   TXGBE_I2CDATA_STOP         ((1 << 9))
#define   TXGBE_I2CDATA_READ         ((1 << 8) | TXGBE_I2CDATA_STOP)
#define   TXGBE_I2CDATA_WRITE        ((0 << 8) | TXGBE_I2CDATA_STOP)
#define TXGBE_I2CSSSCLHCNT           0x014914
#define TXGBE_I2CSSSCLLCNT           0x014918
#define TXGBE_I2CICR                 0x014934 /* I2C Raw Interrupt Status */
#define   TXGBE_I2CICR_RXFULL        ((0x1) << 2)
#define   TXGBE_I2CICR_TXEMPTY       ((0x1) << 4)
#define TXGBE_I2CICM                 0x014930 /* I2C Interrupt Mask */
#define TXGBE_I2CRXTL                0x014938 /* I2C Receive FIFO Threshold */
#define TXGBE_I2CTXTL                0x01493C /* I2C TX FIFO Threshold */
#define TXGBE_I2CENA                 0x01496C /* I2C Enable */
#define TXGBE_I2CSTAT                0x014970 /* I2C Status register */
#define   TXGBE_I2CSTAT_MST          ((1U << 5))
#define TXGBE_I2CSCLTMOUT            0x0149AC
#define TXGBE_I2CSDATMOUT            0x0149B0 /*I2C SDA Stuck at Low Timeout*/

/* port cfg Registers */
#define TXGBE_PORTSTAT                  0x014404
#define   TXGBE_PORTSTAT_UP             MS(0, 0x1)
#define   TXGBE_PORTSTAT_BW_MASK        MS(1, 0x7)
#define     TXGBE_PORTSTAT_BW_10G       MS(1, 0x1)
#define     TXGBE_PORTSTAT_BW_1G        MS(2, 0x1)
#define     TXGBE_PORTSTAT_BW_100M      MS(3, 0x1)
#define   TXGBE_PORTSTAT_ID(r)          RS(r, 8, 0x1)

#define TXGBE_VXLAN                     0x014410
#define TXGBE_VXLAN_GPE                 0x014414
#define TXGBE_GENEVE                    0x014418
#define TXGBE_TEREDO                    0x01441C
#define TXGBE_TCPTIME                   0x014420

/* GPIO Registers */
#define TXGBE_GPIODATA                  0x014800
#define   TXGBE_GPIOBIT_0      MS(0, 0x1) /* O:tx fault */
#define   TXGBE_GPIOBIT_1      MS(1, 0x1) /* O:tx disabled */
#define   TXGBE_GPIOBIT_2      MS(2, 0x1) /* I:sfp module absent */
#define   TXGBE_GPIOBIT_3      MS(3, 0x1) /* I:rx signal lost */
#define   TXGBE_GPIOBIT_4      MS(4, 0x1) /* O:rate select, 1G(0) 10G(1) */
#define   TXGBE_GPIOBIT_5      MS(5, 0x1) /* O:rate select, 1G(0) 10G(1) */
#define   TXGBE_GPIOBIT_6      MS(6, 0x1) /* I:ext phy interrupt */
#define   TXGBE_GPIOBIT_7      MS(7, 0x1) /* I:fan speed alarm */
#define TXGBE_GPIODIR                   0x014804
#define TXGBE_GPIOCTL                   0x014808
#define TXGBE_GPIOINTEN                 0x014830
#define TXGBE_GPIOINTMASK               0x014834
#define TXGBE_GPIOINTTYPE               0x014838
#define TXGBE_GPIOINTSTAT               0x014840
#define TXGBE_GPIOEOI                   0x01484C


#define TXGBE_ARBPOOLIDX                0x01820C
#define TXGBE_ARBTXRATE                 0x018404
#define   TXGBE_ARBTXRATE_MIN(v)        LS(v, 0, 0x3FFF)
#define   TXGBE_ARBTXRATE_MAX(v)        LS(v, 16, 0x3FFF)

/* qos */
#define TXGBE_ARBTXCTL                  0x018200
#define   TXGBE_ARBTXCTL_RRM            MS(1, 0x1)
#define   TXGBE_ARBTXCTL_WSP            MS(2, 0x1)
#define   TXGBE_ARBTXCTL_DIA            MS(6, 0x1)
#define TXGBE_ARBTXMMW                  0x018208

/**************************** Receive DMA registers **************************/
/* receive control */
#define TXGBE_ARBRXCTL                  0x012000
#define   TXGBE_ARBRXCTL_RRM            MS(1, 0x1)
#define   TXGBE_ARBRXCTL_WSP            MS(2, 0x1)
#define   TXGBE_ARBRXCTL_DIA            MS(6, 0x1)

#define TXGBE_RPUP2TC                   0x019008
#define   TXGBE_RPUP2TC_UP_SHIFT        3
#define   TXGBE_RPUP2TC_UP_MASK         0x7

/* mac switcher */
#define TXGBE_ETHADDRL                  0x016200
#define   TXGBE_ETHADDRL_AD0(v)         LS(v, 0, 0xFF)
#define   TXGBE_ETHADDRL_AD1(v)         LS(v, 8, 0xFF)
#define   TXGBE_ETHADDRL_AD2(v)         LS(v, 16, 0xFF)
#define   TXGBE_ETHADDRL_AD3(v)         LS(v, 24, 0xFF)
#define   TXGBE_ETHADDRL_ETAG(r)        RS(r, 0, 0x3FFF)
#define TXGBE_ETHADDRH                  0x016204
#define   TXGBE_ETHADDRH_AD4(v)         LS(v, 0, 0xFF)
#define   TXGBE_ETHADDRH_AD5(v)         LS(v, 8, 0xFF)
#define   TXGBE_ETHADDRH_AD_MASK        MS(0, 0xFFFF)
#define   TXGBE_ETHADDRH_ETAG           MS(30, 0x1)
#define   TXGBE_ETHADDRH_VLD            MS(31, 0x1)
#define TXGBE_ETHADDRASSL               0x016208
#define TXGBE_ETHADDRASSH               0x01620C
#define TXGBE_ETHADDRIDX                0x016210

/* Outmost Barrier Filters */
#define TXGBE_MCADDRTBL(i)              (0x015200 + (i) * 4) /* 0-127 */
#define TXGBE_UCADDRTBL(i)              (0x015400 + (i) * 4) /* 0-127 */
#define TXGBE_VLANTBL(i)                (0x016000 + (i) * 4) /* 0-127 */

#define TXGBE_MNGFLEXSEL                0x1582C
#define TXGBE_MNGFLEXDWL(i)             (0x15A00 + ((i) * 16))
#define TXGBE_MNGFLEXDWH(i)             (0x15A04 + ((i) * 16))
#define TXGBE_MNGFLEXMSK(i)             (0x15A08 + ((i) * 16))

#define TXGBE_LANFLEXSEL                0x15B8C
#define TXGBE_LANFLEXDWL(i)             (0x15C00 + ((i) * 16))
#define TXGBE_LANFLEXDWH(i)             (0x15C04 + ((i) * 16))
#define TXGBE_LANFLEXMSK(i)             (0x15C08 + ((i) * 16))
#define TXGBE_LANFLEXCTL                0x15CFC

/* ipsec */
#define TXGBE_IPSRXIDX                  0x017100
#define   TXGBE_IPSRXIDX_ENA            MS(0, 0x1)
#define   TXGBE_IPSRXIDX_TB_MASK        MS(1, 0x3)
#define   TXGBE_IPSRXIDX_TB_IP          LS(1, 1, 0x3)
#define   TXGBE_IPSRXIDX_TB_SPI         LS(2, 1, 0x3)
#define   TXGBE_IPSRXIDX_TB_KEY         LS(3, 1, 0x3)
#define   TXGBE_IPSRXIDX_TBIDX(v)       LS(v, 3, 0x3FF)
#define   TXGBE_IPSRXIDX_READ           MS(30, 0x1)
#define   TXGBE_IPSRXIDX_WRITE          MS(31, 0x1)
#define TXGBE_IPSRXADDR(i)              (0x017104 + (i) * 4)

#define TXGBE_IPSRXSPI                  0x017114
#define TXGBE_IPSRXADDRIDX              0x017118
#define TXGBE_IPSRXKEY(i)               (0x01711C + (i) * 4)
#define TXGBE_IPSRXSALT                 0x01712C
#define TXGBE_IPSRXMODE                 0x017130
#define   TXGBE_IPSRXMODE_IPV6          0x00000010
#define   TXGBE_IPSRXMODE_DEC           0x00000008
#define   TXGBE_IPSRXMODE_ESP           0x00000004
#define   TXGBE_IPSRXMODE_AH            0x00000002
#define   TXGBE_IPSRXMODE_VLD           0x00000001
#define TXGBE_IPSTXIDX                  0x01D100
#define   TXGBE_IPSTXIDX_ENA            MS(0, 0x1)
#define   TXGBE_IPSTXIDX_SAIDX(v)       LS(v, 3, 0x3FF)
#define   TXGBE_IPSTXIDX_READ           MS(30, 0x1)
#define   TXGBE_IPSTXIDX_WRITE          MS(31, 0x1)
#define TXGBE_IPSTXSALT                 0x01D104
#define TXGBE_IPSTXKEY(i)               (0x01D108 + (i) * 4)

#define TXGBE_MACTXCFG                  0x011000
#define   TXGBE_MACTXCFG_TXE            MS(0, 0x1)
#define   TXGBE_MACTXCFG_SPEED_MASK     MS(29, 0x3)
#define   TXGBE_MACTXCFG_SPEED(v)       LS(v, 29, 0x3)
#define   TXGBE_MACTXCFG_SPEED_10G      LS(0, 29, 0x3)
#define   TXGBE_MACTXCFG_SPEED_1G       LS(3, 29, 0x3)

#define TXGBE_ISBADDRL                  0x000160
#define TXGBE_ISBADDRH                  0x000164

#define NVM_OROM_OFFSET		0x17
#define NVM_OROM_BLK_LOW	0x83
#define NVM_OROM_BLK_HI		0x84
#define NVM_OROM_PATCH_MASK	0xFF
#define NVM_OROM_SHIFT		8
#define NVM_VER_MASK		0x00FF /* version mask */
#define NVM_VER_SHIFT		8     /* version bit shift */
#define NVM_OEM_PROD_VER_PTR	0x1B  /* OEM Product version block pointer */
#define NVM_OEM_PROD_VER_CAP_OFF 0x1  /* OEM Product version format offset */
#define NVM_OEM_PROD_VER_OFF_L	0x2   /* OEM Product version offset low */
#define NVM_OEM_PROD_VER_OFF_H	0x3   /* OEM Product version offset high */
#define NVM_OEM_PROD_VER_CAP_MASK 0xF /* OEM Product version cap mask */
#define NVM_OEM_PROD_VER_MOD_LEN 0x3  /* OEM Product version module length */
#define NVM_ETK_OFF_LOW		0x2D  /* version low order word */
#define NVM_ETK_OFF_HI		0x2E  /* version high order word */
#define NVM_ETK_SHIFT		16    /* high version word shift */
#define NVM_VER_INVALID		0xFFFF
#define NVM_ETK_VALID		0x8000
#define NVM_INVALID_PTR		0xFFFF
#define NVM_VER_SIZE		32    /* version sting size */

#define TXGBE_REG_RSSTBL   TXGBE_RSSTBL(0)
#define TXGBE_REG_RSSKEY   TXGBE_RSSKEY(0)

static inline u32
txgbe_map_reg(struct txgbe_hw *hw, u32 reg)
{
	switch (reg) {
	case TXGBE_REG_RSSTBL:
		if (hw->mac.type == txgbe_mac_raptor_vf)
			reg = TXGBE_VFRSSTBL(0);
		break;
	case TXGBE_REG_RSSKEY:
		if (hw->mac.type == txgbe_mac_raptor_vf)
			reg = TXGBE_VFRSSKEY(0);
		break;
	default:
		/* you should never reach here */
		reg = TXGBE_REG_DUMMY;
		break;
	}

	return reg;
}

/*
 * read non-rc counters
 */
#define TXGBE_UPDCNT32(reg, last, cur)                           \
do {                                                             \
	uint32_t latest = rd32(hw, reg);                         \
	if (hw->offset_loaded || hw->rx_loaded)			 \
		last = 0;					 \
	cur += (latest - last) & UINT_MAX;                       \
	last = latest;                                           \
} while (0)

#define TXGBE_UPDCNT36(regl, last, cur)                          \
do {                                                             \
	uint64_t new_lsb = rd32(hw, regl);                       \
	uint64_t new_msb = rd32(hw, regl + 4);                   \
	uint64_t latest = ((new_msb << 32) | new_lsb);           \
	if (hw->offset_loaded || hw->rx_loaded)			 \
		last = 0;					 \
	cur += (0x1000000000LL + latest - last) & 0xFFFFFFFFFLL; \
	last = latest;                                           \
} while (0)

/**
 * register operations
 **/
#define TXGBE_REG_READ32(addr)               rte_read32(addr)
#define TXGBE_REG_READ32_RELAXED(addr)       rte_read32_relaxed(addr)
#define TXGBE_REG_WRITE32(addr, val)         rte_write32(val, addr)
#define TXGBE_REG_WRITE32_RELAXED(addr, val) rte_write32_relaxed(val, addr)

#define TXGBE_DEAD_READ_REG         0xdeadbeefU
#define TXGBE_FAILED_READ_REG       0xffffffffU
#define TXGBE_REG_ADDR(hw, reg) \
	((volatile u32 *)((char *)(hw)->hw_addr + (reg)))

static inline u32
txgbe_get32(volatile u32 *addr)
{
	u32 val = TXGBE_REG_READ32(addr);
	return rte_le_to_cpu_32(val);
}

static inline void
txgbe_set32(volatile u32 *addr, u32 val)
{
	val = rte_cpu_to_le_32(val);
	TXGBE_REG_WRITE32(addr, val);
}

static inline u32
txgbe_get32_masked(volatile u32 *addr, u32 mask)
{
	u32 val = txgbe_get32(addr);
	val &= mask;
	return val;
}

static inline void
txgbe_set32_masked(volatile u32 *addr, u32 mask, u32 field)
{
	u32 val = txgbe_get32(addr);
	val = ((val & ~mask) | (field & mask));
	txgbe_set32(addr, val);
}

static inline u32
txgbe_get32_relaxed(volatile u32 *addr)
{
	u32 val = TXGBE_REG_READ32_RELAXED(addr);
	return rte_le_to_cpu_32(val);
}

static inline void
txgbe_set32_relaxed(volatile u32 *addr, u32 val)
{
	val = rte_cpu_to_le_32(val);
	TXGBE_REG_WRITE32_RELAXED(addr, val);
}

static inline u32
rd32(struct txgbe_hw *hw, u32 reg)
{
	if (reg == TXGBE_REG_DUMMY)
		return 0;
	return txgbe_get32(TXGBE_REG_ADDR(hw, reg));
}

static inline void
wr32(struct txgbe_hw *hw, u32 reg, u32 val)
{
	if (reg == TXGBE_REG_DUMMY)
		return;
	txgbe_set32(TXGBE_REG_ADDR(hw, reg), val);
}

static inline u32
rd32m(struct txgbe_hw *hw, u32 reg, u32 mask)
{
	u32 val = rd32(hw, reg);
	val &= mask;
	return val;
}

static inline void
wr32m(struct txgbe_hw *hw, u32 reg, u32 mask, u32 field)
{
	u32 val = rd32(hw, reg);
	val = ((val & ~mask) | (field & mask));
	wr32(hw, reg, val);
}

static inline u64
rd64(struct txgbe_hw *hw, u32 reg)
{
	u64 lsb = rd32(hw, reg);
	u64 msb = rd32(hw, reg + 4);
	return (lsb | msb << 32);
}

static inline void
wr64(struct txgbe_hw *hw, u32 reg, u64 val)
{
	wr32(hw, reg, (u32)val);
	wr32(hw, reg + 4, (u32)(val >> 32));
}

/* poll register */
static inline u32
po32m(struct txgbe_hw *hw, u32 reg, u32 mask, u32 expect, u32 *actual,
	u32 loop, u32 slice)
{
	bool usec = true;
	u32 value = 0, all = 0;

	if (slice > 1000 * MAX_UDELAY_MS) {
		usec = false;
		slice = (slice + 500) / 1000;
	}

	do {
		all |= rd32(hw, reg);
		value |= mask & all;
		if (value == expect)
			break;

		usec ? usec_delay(slice) : msec_delay(slice);
	} while (--loop > 0);

	if (actual)
		*actual = all;

	return loop;
}

/* flush all write operations */
#define txgbe_flush(hw) rd32(hw, 0x00100C)

#define rd32a(hw, reg, idx) ( \
	rd32((hw), (reg) + ((idx) << 2)))
#define wr32a(hw, reg, idx, val) \
	wr32((hw), (reg) + ((idx) << 2), (val))

#define rd32at(hw, reg, idx) \
		rd32a(hw, txgbe_map_reg(hw, reg), idx)
#define wr32at(hw, reg, idx, val) \
		wr32a(hw, txgbe_map_reg(hw, reg), idx, val)

#define rd32w(hw, reg, mask, slice) do { \
	rd32((hw), reg); \
	po32m((hw), reg, mask, mask, NULL, 5, slice); \
} while (0)

#define wr32w(hw, reg, val, mask, slice) do { \
	wr32((hw), reg, val); \
	po32m((hw), reg, mask, mask, NULL, 5, slice); \
} while (0)

#define TXGBE_XPCS_IDAADDR    0x13000
#define TXGBE_XPCS_IDADATA    0x13004
#define TXGBE_EPHY_IDAADDR    0x13008
#define TXGBE_EPHY_IDADATA    0x1300C
static inline u32
rd32_epcs(struct txgbe_hw *hw, u32 addr)
{
	u32 data;
	wr32(hw, TXGBE_XPCS_IDAADDR, addr);
	data = rd32(hw, TXGBE_XPCS_IDADATA);
	return data;
}

static inline void
wr32_epcs(struct txgbe_hw *hw, u32 addr, u32 data)
{
	wr32(hw, TXGBE_XPCS_IDAADDR, addr);
	wr32(hw, TXGBE_XPCS_IDADATA, data);
}

static inline u32
rd32_ephy(struct txgbe_hw *hw, u32 addr)
{
	u32 data;
	wr32(hw, TXGBE_EPHY_IDAADDR, addr);
	data = rd32(hw, TXGBE_EPHY_IDADATA);
	return data;
}

static inline void
wr32_ephy(struct txgbe_hw *hw, u32 addr, u32 data)
{
	wr32(hw, TXGBE_EPHY_IDAADDR, addr);
	wr32(hw, TXGBE_EPHY_IDADATA, data);
}

#endif /* _TXGBE_REGS_H_ */
