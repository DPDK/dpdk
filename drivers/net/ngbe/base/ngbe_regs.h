/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#ifndef _NGBE_REGS_H_
#define _NGBE_REGS_H_

#define NGBE_PVMBX_QSIZE          (16) /* 16*4B */
#define NGBE_PVMBX_BSIZE          (NGBE_PVMBX_QSIZE * 4)

#define NGBE_REMOVED(a) (0)

#define NGBE_REG_DUMMY             0xFFFFFF

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
#define NGBE_AUTOC                       NGBE_REG_DUMMY
#define   NGBE_AUTOC_FLU                 MS64(0, 0x1)
#define   NGBE_AUTOC_10G_PMA_PMD_MASK    MS64(7, 0x3) /* parallel */
#define   NGBE_AUTOC_10G_XAUI            LS64(0, 7, 0x3)
#define   NGBE_AUTOC_10G_KX4             LS64(1, 7, 0x3)
#define   NGBE_AUTOC_10G_CX4             LS64(2, 7, 0x3)
#define   NGBE_AUTOC_10G_KR              LS64(3, 7, 0x3) /* fixme */
#define   NGBE_AUTOC_1G_PMA_PMD_MASK     MS64(9, 0x7)
#define   NGBE_AUTOC_1G_BX               LS64(0, 9, 0x7)
#define   NGBE_AUTOC_1G_KX               LS64(1, 9, 0x7)
#define   NGBE_AUTOC_1G_SFI              LS64(0, 9, 0x7)
#define   NGBE_AUTOC_1G_KX_BX            LS64(1, 9, 0x7)
#define   NGBE_AUTOC_AN_RESTART          MS64(12, 0x1)
#define   NGBE_AUTOC_LMS_MASK            MS64(13, 0x7)
#define   NGBE_AUTOC_LMS_10G             LS64(3, 13, 0x7)
#define   NGBE_AUTOC_LMS_KX4_KX_KR       LS64(4, 13, 0x7)
#define   NGBE_AUTOC_LMS_SGMII_1G_100M   LS64(5, 13, 0x7)
#define   NGBE_AUTOC_LMS_KX4_KX_KR_1G_AN LS64(6, 13, 0x7)
#define   NGBE_AUTOC_LMS_KX4_KX_KR_SGMII LS64(7, 13, 0x7)
#define   NGBE_AUTOC_LMS_1G_LINK_NO_AN   LS64(0, 13, 0x7)
#define   NGBE_AUTOC_LMS_10G_LINK_NO_AN  LS64(1, 13, 0x7)
#define   NGBE_AUTOC_LMS_1G_AN           LS64(2, 13, 0x7)
#define   NGBE_AUTOC_LMS_KX4_AN          LS64(4, 13, 0x7)
#define   NGBE_AUTOC_LMS_KX4_AN_1G_AN    LS64(6, 13, 0x7)
#define   NGBE_AUTOC_LMS_ATTACH_TYPE     LS64(7, 13, 0x7)
#define   NGBE_AUTOC_LMS_AN              MS64(15, 0x7)

#define   NGBE_AUTOC_KR_SUPP             MS64(16, 0x1)
#define   NGBE_AUTOC_FECR                MS64(17, 0x1)
#define   NGBE_AUTOC_FECA                MS64(18, 0x1)
#define   NGBE_AUTOC_AN_RX_ALIGN         MS64(18, 0x1F) /* fixme */
#define   NGBE_AUTOC_AN_RX_DRIFT         MS64(23, 0x3)
#define   NGBE_AUTOC_AN_RX_LOOSE         MS64(24, 0x3)
#define   NGBE_AUTOC_PD_TMR              MS64(25, 0x3)
#define   NGBE_AUTOC_RF                  MS64(27, 0x1)
#define   NGBE_AUTOC_ASM_PAUSE           MS64(29, 0x1)
#define   NGBE_AUTOC_SYM_PAUSE           MS64(28, 0x1)
#define   NGBE_AUTOC_PAUSE               MS64(28, 0x3)
#define   NGBE_AUTOC_KX_SUPP             MS64(30, 0x1)
#define   NGBE_AUTOC_KX4_SUPP            MS64(31, 0x1)

#define   NGBE_AUTOC_10GS_PMA_PMD_MASK   MS64(48, 0x3)  /* serial */
#define   NGBE_AUTOC_10GS_KR             LS64(0, 48, 0x3)
#define   NGBE_AUTOC_10GS_XFI            LS64(1, 48, 0x3)
#define   NGBE_AUTOC_10GS_SFI            LS64(2, 48, 0x3)
#define   NGBE_AUTOC_LINK_DIA_MASK       MS64(60, 0x7)
#define   NGBE_AUTOC_LINK_DIA_D3_MASK    LS64(5, 60, 0x7)

#define   NGBE_AUTOC_SPEED_MASK          MS64(32, 0xFFFF)
#define   NGBD_AUTOC_SPEED(r)            RS64(r, 32, 0xFFFF)
#define   NGBE_AUTOC_SPEED(v)            LS64(v, 32, 0xFFFF)
#define     NGBE_LINK_SPEED_UNKNOWN      0
#define     NGBE_LINK_SPEED_10M_FULL     0x0002
#define     NGBE_LINK_SPEED_100M_FULL    0x0008
#define     NGBE_LINK_SPEED_1GB_FULL     0x0020
#define     NGBE_LINK_SPEED_2_5GB_FULL   0x0400
#define     NGBE_LINK_SPEED_5GB_FULL     0x0800
#define     NGBE_LINK_SPEED_10GB_FULL    0x0080
#define     NGBE_LINK_SPEED_40GB_FULL    0x0100
#define   NGBE_AUTOC_AUTONEG             MS64(63, 0x1)



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

#define NGBE_ETHPHYIF                  NGBE_REG_DUMMY
#define   NGBE_ETHPHYIF_MDIO_ACT       MS(1, 0x1)
#define   NGBE_ETHPHYIF_MDIO_MODE      MS(2, 0x1)
#define   NGBE_ETHPHYIF_MDIO_BASE(r)   RS(r, 3, 0x1F)
#define   NGBE_ETHPHYIF_MDIO_SHARED    MS(13, 0x1)
#define   NGBE_ETHPHYIF_SPEED_10M      MS(17, 0x1)
#define   NGBE_ETHPHYIF_SPEED_100M     MS(18, 0x1)
#define   NGBE_ETHPHYIF_SPEED_1G       MS(19, 0x1)
#define   NGBE_ETHPHYIF_SPEED_2_5G     MS(20, 0x1)
#define   NGBE_ETHPHYIF_SPEED_10G      MS(21, 0x1)
#define   NGBE_ETHPHYIF_SGMII_ENABLE   MS(25, 0x1)
#define   NGBE_ETHPHYIF_INT_PHY_MODE   MS(24, 0x1)
#define   NGBE_ETHPHYIF_IO_XPCS        MS(30, 0x1)
#define   NGBE_ETHPHYIF_IO_EPHY        MS(31, 0x1)

/******************************************************************************
 * Chip Registers
 ******************************************************************************/
/**
 * Chip Status
 **/
#define NGBE_PWR		0x010000
#define   NGBE_PWR_LAN(r)	RS(r, 28, 0xC)
#define     NGBE_PWR_LAN_0	(1)
#define     NGBE_PWR_LAN_1	(2)
#define     NGBE_PWR_LAN_2	(3)
#define     NGBE_PWR_LAN_3	(4)
#define NGBE_CTL		0x010004
#define NGBE_LOCKPF		0x010008
#define NGBE_RST		0x01000C
#define   NGBE_RST_SW		MS(0, 0x1)
#define   NGBE_RST_LAN(i)	MS(((i) + 1), 0x1)
#define   NGBE_RST_FW		MS(5, 0x1)
#define   NGBE_RST_ETH(i)	MS(((i) + 29), 0x1)
#define   NGBE_RST_GLB		MS(31, 0x1)
#define   NGBE_RST_DEFAULT	(NGBE_RST_SW | \
				NGBE_RST_LAN(0) | \
				NGBE_RST_LAN(1) | \
				NGBE_RST_LAN(2) | \
				NGBE_RST_LAN(3))
#define NGBE_PROB			0x010010
#define NGBE_IODRV			0x010024
#define NGBE_STAT			0x010028
#define   NGBE_STAT_MNGINIT		MS(0, 0x1)
#define   NGBE_STAT_MNGVETO		MS(8, 0x1)
#define   NGBE_STAT_ECCLAN0		MS(16, 0x1)
#define   NGBE_STAT_ECCLAN1		MS(17, 0x1)
#define   NGBE_STAT_ECCLAN2		MS(18, 0x1)
#define   NGBE_STAT_ECCLAN3		MS(19, 0x1)
#define   NGBE_STAT_ECCMNG		MS(20, 0x1)
#define   NGBE_STAT_ECCPCORE		MS(21, 0X1)
#define   NGBE_STAT_ECCPCIW		MS(22, 0x1)
#define   NGBE_STAT_ECCPCIEPHY		MS(23, 0x1)
#define   NGBE_STAT_ECCFMGR		MS(24, 0x1)
#define   NGBE_STAT_GPHY_IN_RST(i)	MS(((i) + 9), 0x1)
#define NGBE_RSTSTAT			0x010030
#define   NGBE_RSTSTAT_PROG		MS(20, 0x1)
#define   NGBE_RSTSTAT_PREP		MS(19, 0x1)
#define   NGBE_RSTSTAT_TYPE_MASK	MS(16, 0x7)
#define   NGBE_RSTSTAT_TYPE(r)		RS(r, 16, 0x7)
#define   NGBE_RSTSTAT_TYPE_PE		LS(0, 16, 0x7)
#define   NGBE_RSTSTAT_TYPE_PWR		LS(1, 16, 0x7)
#define   NGBE_RSTSTAT_TYPE_HOT		LS(2, 16, 0x7)
#define   NGBE_RSTSTAT_TYPE_SW		LS(3, 16, 0x7)
#define   NGBE_RSTSTAT_TYPE_FW		LS(4, 16, 0x7)
#define   NGBE_RSTSTAT_TMRINIT_MASK	MS(8, 0xFF)
#define   NGBE_RSTSTAT_TMRINIT(v)	LS(v, 8, 0xFF)
#define   NGBE_RSTSTAT_TMRCNT_MASK	MS(0, 0xFF)
#define   NGBE_RSTSTAT_TMRCNT(v)	LS(v, 0, 0xFF)
#define NGBE_PWRTMR			0x010034

/**
 * SPI(Flash)
 **/
#define NGBE_SPICMD               0x010104
#define   NGBE_SPICMD_ADDR(v)     LS(v, 0, 0xFFFFFF)
#define   NGBE_SPICMD_CLK(v)      LS(v, 25, 0x7)
#define   NGBE_SPICMD_CMD(v)      LS(v, 28, 0x7)
#define NGBE_SPIDAT               0x010108
#define   NGBE_SPIDAT_BYPASS      MS(31, 0x1)
#define   NGBE_SPIDAT_STATUS(v)   LS(v, 16, 0xFF)
#define   NGBE_SPIDAT_OPDONE      MS(0, 0x1)
#define NGBE_SPISTAT              0x01010C
#define   NGBE_SPISTAT_OPDONE     MS(0, 0x1)
#define   NGBE_SPISTAT_BPFLASH    MS(31, 0x1)
#define NGBE_SPIUSRCMD            0x010110
#define NGBE_SPICFG0              0x010114
#define NGBE_SPICFG1              0x010118

/* FMGR Registers */
#define NGBE_ILDRSTAT                  0x010120
#define   NGBE_ILDRSTAT_PCIRST         MS(0, 0x1)
#define   NGBE_ILDRSTAT_PWRRST         MS(1, 0x1)
#define   NGBE_ILDRSTAT_SWRST          MS(11, 0x1)
#define   NGBE_ILDRSTAT_SWRST_LAN0     MS(13, 0x1)
#define   NGBE_ILDRSTAT_SWRST_LAN1     MS(14, 0x1)
#define   NGBE_ILDRSTAT_SWRST_LAN2     MS(15, 0x1)
#define   NGBE_ILDRSTAT_SWRST_LAN3     MS(16, 0x1)

#define NGBE_SRAM                 0x010124
#define   NGBE_SRAM_SZ(v)         LS(v, 28, 0x7)
#define NGBE_SRAMCTLECC           0x010130
#define NGBE_SRAMINJECC           0x010134
#define NGBE_SRAMECC              0x010138

/* Sensors for PVT(Process Voltage Temperature) */
#define NGBE_TSCTRL			0x010300
#define   NGBE_TSCTRL_EVALMD		MS(31, 0x1)
#define NGBE_TSEN			0x010304
#define   NGBE_TSEN_ENA			MS(0, 0x1)
#define NGBE_TSSTAT			0x010308
#define   NGBE_TSSTAT_VLD		MS(16, 0x1)
#define   NGBE_TSSTAT_DATA(r)		RS(r, 0, 0x3FF)
#define NGBE_TSATHRE			0x01030C
#define NGBE_TSDTHRE			0x010310
#define NGBE_TSINTR			0x010314
#define   NGBE_TSINTR_AEN		MS(0, 0x1)
#define   NGBE_TSINTR_DEN		MS(1, 0x1)
#define NGBE_TSALM			0x010318
#define   NGBE_TSALM_LO			MS(0, 0x1)
#define   NGBE_TSALM_HI			MS(1, 0x1)

#define NGBE_EFUSE_WDATA0          0x010320
#define NGBE_EFUSE_WDATA1          0x010324
#define NGBE_EFUSE_RDATA0          0x010328
#define NGBE_EFUSE_RDATA1          0x01032C
#define NGBE_EFUSE_STATUS          0x010330

/******************************************************************************
 * Port Registers
 ******************************************************************************/
/* Internal PHY reg_offset [0,31] */
#define NGBE_PHY_CONFIG(reg_offset)	(0x014000 + (reg_offset) * 4)

/* Port Control */
#define NGBE_PORTCTL                   0x014400
#define   NGBE_PORTCTL_VLANEXT         MS(0, 0x1)
#define   NGBE_PORTCTL_ETAG            MS(1, 0x1)
#define   NGBE_PORTCTL_QINQ            MS(2, 0x1)
#define   NGBE_PORTCTL_DRVLOAD         MS(3, 0x1)
#define   NGBE_PORTCTL_NUMVT_MASK      MS(12, 0x1)
#define   NGBE_PORTCTL_NUMVT_8         LS(1, 12, 0x1)
#define   NGBE_PORTCTL_RSTDONE         MS(14, 0x1)
#define   NGBE_PORTCTL_TEREDODIA       MS(27, 0x1)
#define   NGBE_PORTCTL_GENEVEDIA       MS(28, 0x1)
#define   NGBE_PORTCTL_VXLANGPEDIA     MS(30, 0x1)
#define   NGBE_PORTCTL_VXLANDIA        MS(31, 0x1)

/* Port Status */
#define NGBE_PORTSTAT                  0x014404
#define   NGBE_PORTSTAT_BW_MASK        MS(1, 0x7)
#define     NGBE_PORTSTAT_BW_1G        MS(1, 0x1)
#define     NGBE_PORTSTAT_BW_100M      MS(2, 0x1)
#define     NGBE_PORTSTAT_BW_10M       MS(3, 0x1)
#define   NGBE_PORTSTAT_ID(r)          RS(r, 8, 0x3)

#define NGBE_EXTAG                     0x014408
#define   NGBE_EXTAG_ETAG_MASK         MS(0, 0xFFFF)
#define   NGBE_EXTAG_ETAG(v)           LS(v, 0, 0xFFFF)
#define   NGBE_EXTAG_VLAN_MASK         MS(16, 0xFFFF)
#define   NGBE_EXTAG_VLAN(v)           LS(v, 16, 0xFFFF)

#define NGBE_TCPTIME                   0x014420

#define NGBE_LEDCTL                     0x014424
#define   NGBE_LEDCTL_SEL(s)            MS((s), 0x1)
#define   NGBE_LEDCTL_OD(s)             MS(((s) + 16), 0x1)
	/* s=1G(1),100M(2),10M(3) */
#define   NGBE_LEDCTL_100M      (NGBE_LEDCTL_SEL(2) | NGBE_LEDCTL_OD(2))

#define NGBE_TAGTPID(i)                (0x014430 + (i) * 4) /*0-3*/
#define   NGBE_TAGTPID_LSB_MASK        MS(0, 0xFFFF)
#define   NGBE_TAGTPID_LSB(v)          LS(v, 0, 0xFFFF)
#define   NGBE_TAGTPID_MSB_MASK        MS(16, 0xFFFF)
#define   NGBE_TAGTPID_MSB(v)          LS(v, 16, 0xFFFF)

#define NGBE_LAN_SPEED			0x014440
#define   NGBE_LAN_SPEED_MASK		MS(0, 0x3)

/* GPIO Registers */
#define NGBE_GPIODATA			0x014800
#define   NGBE_GPIOBIT_0      MS(0, 0x1) /* O:tx fault */
#define   NGBE_GPIOBIT_1      MS(1, 0x1) /* O:tx disabled */
#define   NGBE_GPIOBIT_2      MS(2, 0x1) /* I:sfp module absent */
#define   NGBE_GPIOBIT_3      MS(3, 0x1) /* I:rx signal lost */
#define   NGBE_GPIOBIT_4      MS(4, 0x1) /* O:rate select, 1G(0) 10G(1) */
#define   NGBE_GPIOBIT_5      MS(5, 0x1) /* O:rate select, 1G(0) 10G(1) */
#define   NGBE_GPIOBIT_6      MS(6, 0x1) /* I:ext phy interrupt */
#define   NGBE_GPIOBIT_7      MS(7, 0x1) /* I:fan speed alarm */
#define NGBE_GPIODIR			0x014804
#define   NGBE_GPIODIR_DDR(v)		LS(v, 0, 0x3)
#define NGBE_GPIOCTL			0x014808
#define NGBE_GPIOINTEN			0x014830
#define   NGBE_GPIOINTEN_INT(v)		LS(v, 0, 0x3)
#define NGBE_GPIOINTMASK		0x014834
#define NGBE_GPIOINTTYPE		0x014838
#define   NGBE_GPIOINTTYPE_LEVEL(v)	LS(v, 0, 0x3)
#define NGBE_GPIOINTPOL			0x01483C
#define   NGBE_GPIOINTPOL_ACT(v)	LS(v, 0, 0x3)
#define NGBE_GPIOINTSTAT		0x014840
#define NGBE_GPIOINTDB			0x014848
#define NGBE_GPIOEOI			0x01484C
#define NGBE_GPIODAT			0x014850

/* TPH */
#define NGBE_TPHCFG               0x014F00

/******************************************************************************
 * Transmit DMA Registers
 ******************************************************************************/
/* TDMA Control */
#define NGBE_DMATXCTRL			0x018000
#define   NGBE_DMATXCTRL_ENA		MS(0, 0x1)
#define   NGBE_DMATXCTRL_TPID_MASK	MS(16, 0xFFFF)
#define   NGBE_DMATXCTRL_TPID(v)	LS(v, 16, 0xFFFF)
#define NGBE_POOLTXENA(i)		(0x018004 + (i) * 4) /*0*/
#define NGBE_PRBTXDMACTL		0x018010
#define NGBE_ECCTXDMACTL		0x018014
#define NGBE_ECCTXDMAINJ		0x018018
#define NGBE_ECCTXDMA			0x01801C
#define NGBE_PBTXDMATH			0x018020
#define NGBE_QPTXLLI			0x018040
#define NGBE_POOLTXLBET			0x018050
#define NGBE_POOLTXASET			0x018058
#define NGBE_POOLTXASMAC		0x018060
#define NGBE_POOLTXASVLAN		0x018070
#define NGBE_POOLTXDSA			0x0180A0
#define NGBE_POOLTAG(pl)		(0x018100 + (pl) * 4) /*0-7*/
#define   NGBE_POOLTAG_VTAG(v)		LS(v, 0, 0xFFFF)
#define   NGBE_POOLTAG_VTAG_MASK	MS(0, 0xFFFF)
#define   TXGBD_POOLTAG_VTAG_UP(r)	RS(r, 13, 0x7)
#define   NGBE_POOLTAG_TPIDSEL(v)	LS(v, 24, 0x7)
#define   NGBE_POOLTAG_ETAG_MASK	MS(27, 0x3)
#define   NGBE_POOLTAG_ETAG		LS(2, 27, 0x3)
#define   NGBE_POOLTAG_ACT_MASK		MS(30, 0x3)
#define   NGBE_POOLTAG_ACT_ALWAYS	LS(1, 30, 0x3)
#define   NGBE_POOLTAG_ACT_NEVER	LS(2, 30, 0x3)

/* Queue Arbiter(QoS) */
#define NGBE_QARBTXCTL			0x018200
#define   NGBE_QARBTXCTL_DA		MS(6, 0x1)
#define NGBE_QARBTXRATE			0x018404
#define   NGBE_QARBTXRATE_MIN(v)	LS(v, 0, 0x3FFF)
#define   NGBE_QARBTXRATE_MAX(v)	LS(v, 16, 0x3FFF)

/* ETAG */
#define NGBE_POOLETAG(pl)         (0x018700 + (pl) * 4)

/******************************************************************************
 * Receive DMA Registers
 ******************************************************************************/
/* Receive Control */
#define NGBE_ARBRXCTL			0x012000
#define   NGBE_ARBRXCTL_DIA		MS(6, 0x1)
#define NGBE_POOLRXENA(i)		(0x012004 + (i) * 4) /*0*/
#define NGBE_PRBRDMA			0x012010
#define NGBE_ECCRXDMACTL		0x012014
#define NGBE_ECCRXDMAINJ		0x012018
#define NGBE_ECCRXDMA			0x01201C
#define NGBE_POOLRXDNA			0x0120A0
#define NGBE_QPRXDROP			0x012080
#define NGBE_QPRXSTRPVLAN		0x012090

/******************************************************************************
 * Packet Buffer
 ******************************************************************************/
/* Flow Control */
#define NGBE_FCXOFFTM			0x019200
#define NGBE_FCWTRLO			0x019220
#define   NGBE_FCWTRLO_TH(v)		LS(v, 10, 0x1FF) /*KB*/
#define   NGBE_FCWTRLO_XON		MS(31, 0x1)
#define NGBE_FCWTRHI			0x019260
#define   NGBE_FCWTRHI_TH(v)		LS(v, 10, 0x1FF) /*KB*/
#define   NGBE_FCWTRHI_XOFF		MS(31, 0x1)
#define NGBE_RXFCRFSH			0x0192A0
#define   NGBE_RXFCFSH_TIME(v)		LS(v, 0, 0xFFFF)
#define NGBE_FCSTAT			0x01CE00
#define   NGBE_FCSTAT_DLNK		MS(0, 0x1)
#define   NGBE_FCSTAT_ULNK		MS(8, 0x1)

#define NGBE_RXFCCFG                   0x011090
#define   NGBE_RXFCCFG_FC              MS(0, 0x1)
#define NGBE_TXFCCFG                   0x0192A4
#define   NGBE_TXFCCFG_FC              MS(3, 0x1)

/* Data Buffer */
#define NGBE_PBRXCTL                   0x019000
#define   NGBE_PBRXCTL_ST              MS(0, 0x1)
#define   NGBE_PBRXCTL_ENA             MS(31, 0x1)
#define NGBE_PBRXSTAT                  0x019004
#define NGBE_PBRXSIZE                  0x019020
#define   NGBE_PBRXSIZE_KB(v)          LS(v, 10, 0x3F)

#define NGBE_PBRXOFTMR                 0x019094
#define NGBE_PBRXDBGCMD                0x019090
#define NGBE_PBRXDBGDAT                0x0190A0

#define NGBE_PBTXSIZE                  0x01CC00

/* LLI */
#define NGBE_PBRXLLI              0x19080
#define   NGBE_PBRXLLI_SZLT(v)    LS(v, 0, 0xFFF)
#define   NGBE_PBRXLLI_UPLT(v)    LS(v, 16, 0x7)
#define   NGBE_PBRXLLI_UPEA       MS(19, 0x1)

/* Port Arbiter(QoS) */
#define NGBE_PARBTXCTL            0x01CD00
#define   NGBE_PARBTXCTL_DA       MS(6, 0x1)

/******************************************************************************
 * Packet Filter (L2-7)
 ******************************************************************************/
/**
 * Receive Scaling
 **/
#define NGBE_POOLRSS(pl)		(0x019300 + (pl) * 4) /*0-7*/
#define   NGBE_POOLRSS_L4HDR		MS(1, 0x1)
#define   NGBE_POOLRSS_L3HDR		MS(2, 0x1)
#define   NGBE_POOLRSS_L2HDR		MS(3, 0x1)
#define   NGBE_POOLRSS_L2TUN		MS(4, 0x1)
#define   NGBE_POOLRSS_TUNHDR		MS(5, 0x1)
#define NGBE_RSSTBL(i)			(0x019400 + (i) * 4) /*32*/
#define NGBE_RSSKEY(i)			(0x019480 + (i) * 4) /*10*/
#define NGBE_RACTL			0x0194F4
#define   NGBE_RACTL_RSSENA		MS(2, 0x1)
#define   NGBE_RACTL_RSSMASK		MS(16, 0xFFFF)
#define   NGBE_RACTL_RSSIPV4TCP		MS(16, 0x1)
#define   NGBE_RACTL_RSSIPV4		MS(17, 0x1)
#define   NGBE_RACTL_RSSIPV6		MS(20, 0x1)
#define   NGBE_RACTL_RSSIPV6TCP		MS(21, 0x1)
#define   NGBE_RACTL_RSSIPV4UDP		MS(22, 0x1)
#define   NGBE_RACTL_RSSIPV6UDP		MS(23, 0x1)

/**
 * Flow Director
 **/
#define PERFECT_BUCKET_64KB_HASH_MASK	0x07FF	/* 11 bits */
#define PERFECT_BUCKET_128KB_HASH_MASK	0x0FFF	/* 12 bits */
#define PERFECT_BUCKET_256KB_HASH_MASK	0x1FFF	/* 13 bits */
#define SIG_BUCKET_64KB_HASH_MASK	0x1FFF	/* 13 bits */
#define SIG_BUCKET_128KB_HASH_MASK	0x3FFF	/* 14 bits */
#define SIG_BUCKET_256KB_HASH_MASK	0x7FFF	/* 15 bits */

/**
 * 5-tuple Filter
 **/
#define NGBE_5TFPORT(i)			(0x019A00 + (i) * 4) /*0-7*/
#define   NGBE_5TFPORT_SRC(v)		LS(v, 0, 0xFFFF)
#define   NGBE_5TFPORT_DST(v)		LS(v, 16, 0xFFFF)
#define NGBE_5TFCTL0(i)			(0x019C00 + (i) * 4) /*0-7*/
#define   NGBE_5TFCTL0_PROTO(v)		LS(v, 0, 0x3)
enum ngbe_5tuple_protocol {
	NGBE_5TF_PROT_TCP = 0,
	NGBE_5TF_PROT_UDP,
	NGBE_5TF_PROT_SCTP,
	NGBE_5TF_PROT_NONE,
};
#define   NGBE_5TFCTL0_PRI(v)		LS(v, 2, 0x7)
#define   NGBE_5TFCTL0_POOL(v)		LS(v, 8, 0x7)
#define   NGBE_5TFCTL0_MASK		MS(27, 0xF)
#define     NGBE_5TFCTL0_MSPORT		MS(27, 0x1)
#define     NGBE_5TFCTL0_MDPORT		MS(28, 0x1)
#define     NGBE_5TFCTL0_MPROTO		MS(29, 0x1)
#define     NGBE_5TFCTL0_MPOOL		MS(30, 0x1)
#define   NGBE_5TFCTL0_ENA		MS(31, 0x1)
#define NGBE_5TFCTL1(i)			(0x019E00 + (i) * 4) /*0-7*/
#define   NGBE_5TFCTL1_CHKSZ		MS(12, 0x1)
#define   NGBE_5TFCTL1_LLI		MS(20, 0x1)
#define   NGBE_5TFCTL1_QP(v)		LS(v, 21, 0x7)

/**
 * Storm Control
 **/
#define NGBE_STRMCTL              0x015004
#define   NGBE_STRMCTL_MCPNSH     MS(0, 0x1)
#define   NGBE_STRMCTL_MCDROP     MS(1, 0x1)
#define   NGBE_STRMCTL_BCPNSH     MS(2, 0x1)
#define   NGBE_STRMCTL_BCDROP     MS(3, 0x1)
#define   NGBE_STRMCTL_DFTPOOL    MS(4, 0x1)
#define   NGBE_STRMCTL_ITVL(v)    LS(v, 8, 0x3FF)
#define NGBE_STRMTH               0x015008
#define   NGBE_STRMTH_MC(v)       LS(v, 0, 0xFFFF)
#define   NGBE_STRMTH_BC(v)       LS(v, 16, 0xFFFF)

/******************************************************************************
 * Ether Flow
 ******************************************************************************/
#define NGBE_PSRCTL		       0x015000
#define   NGBE_PSRCTL_TPE	       MS(4, 0x1)
#define   NGBE_PSRCTL_ADHF12_MASK      MS(5, 0x3)
#define   NGBE_PSRCTL_ADHF12(v)        LS(v, 5, 0x3)
#define   NGBE_PSRCTL_UCHFENA	       MS(7, 0x1)
#define   NGBE_PSRCTL_MCHFENA	       MS(7, 0x1)
#define   NGBE_PSRCTL_MCP	       MS(8, 0x1)
#define   NGBE_PSRCTL_UCP	       MS(9, 0x1)
#define   NGBE_PSRCTL_BCA	       MS(10, 0x1)
#define   NGBE_PSRCTL_L4CSUM	       MS(12, 0x1)
#define   NGBE_PSRCTL_PCSD	       MS(13, 0x1)
#define   NGBE_PSRCTL_LBENA	       MS(18, 0x1)
#define NGBE_FRMSZ		       0x015020
#define   NGBE_FRMSZ_MAX_MASK	       MS(0, 0xFFFF)
#define   NGBE_FRMSZ_MAX(v)	       LS(v, 0, 0xFFFF)
#define NGBE_VLANCTL		       0x015088
#define   NGBE_VLANCTL_TPID_MASK       MS(0, 0xFFFF)
#define   NGBE_VLANCTL_TPID(v)	       LS(v, 0, 0xFFFF)
#define   NGBE_VLANCTL_CFI	       MS(28, 0x1)
#define   NGBE_VLANCTL_CFIENA	       MS(29, 0x1)
#define   NGBE_VLANCTL_VFE	       MS(30, 0x1)
#define NGBE_POOLCTL		       0x0151B0
#define   NGBE_POOLCTL_DEFDSA	       MS(29, 0x1)
#define   NGBE_POOLCTL_RPLEN	       MS(30, 0x1)
#define   NGBE_POOLCTL_MODE_MASK       MS(16, 0x3)
#define     NGBE_PSRPOOL_MODE_MAC      LS(0, 16, 0x3)
#define     NGBE_PSRPOOL_MODE_ETAG     LS(1, 16, 0x3)
#define   NGBE_POOLCTL_DEFPL(v)        LS(v, 7, 0x7)
#define     NGBE_POOLCTL_DEFPL_MASK    MS(7, 0x7)

#define NGBE_ETFLT(i)                  (0x015128 + (i) * 4) /*0-7*/
#define   NGBE_ETFLT_ETID(v)           LS(v, 0, 0xFFFF)
#define   NGBE_ETFLT_ETID_MASK         MS(0, 0xFFFF)
#define   NGBE_ETFLT_POOL(v)           LS(v, 20, 0x7)
#define   NGBE_ETFLT_POOLENA           MS(26, 0x1)
#define   NGBE_ETFLT_TXAS              MS(29, 0x1)
#define   NGBE_ETFLT_1588              MS(30, 0x1)
#define   NGBE_ETFLT_ENA               MS(31, 0x1)
#define NGBE_ETCLS(i)                  (0x019100 + (i) * 4) /*0-7*/
#define   NGBE_ETCLS_QPID(v)           LS(v, 16, 0x7)
#define   NGBD_ETCLS_QPID(r)           RS(r, 16, 0x7)
#define   NGBE_ETCLS_LLI               MS(29, 0x1)
#define   NGBE_ETCLS_QENA              MS(31, 0x1)
#define NGBE_SYNCLS                    0x019130
#define   NGBE_SYNCLS_ENA              MS(0, 0x1)
#define   NGBE_SYNCLS_QPID(v)          LS(v, 1, 0x7)
#define   NGBD_SYNCLS_QPID(r)          RS(r, 1, 0x7)
#define   NGBE_SYNCLS_QPID_MASK        MS(1, 0x7)
#define   NGBE_SYNCLS_HIPRIO           MS(31, 0x1)

/* MAC & VLAN & NVE */
#define NGBE_PSRVLANIDX           0x016230 /*0-31*/
#define NGBE_PSRVLAN              0x016220
#define   NGBE_PSRVLAN_VID(v)     LS(v, 0, 0xFFF)
#define   NGBE_PSRVLAN_EA         MS(31, 0x1)
#define NGBE_PSRVLANPLM(i)        (0x016224 + (i) * 4) /*0-1*/

/**
 * Mirror Rules
 **/
#define NGBE_MIRRCTL(i)	               (0x015B00 + (i) * 4)
#define  NGBE_MIRRCTL_POOL	       MS(0, 0x1)
#define  NGBE_MIRRCTL_UPLINK	       MS(1, 0x1)
#define  NGBE_MIRRCTL_DNLINK	       MS(2, 0x1)
#define  NGBE_MIRRCTL_VLAN	       MS(3, 0x1)
#define  NGBE_MIRRCTL_DESTP(v)	       LS(v, 8, 0x7)
#define NGBE_MIRRVLANL(i)	       (0x015B10 + (i) * 8)
#define NGBE_MIRRPOOLL(i)	       (0x015B30 + (i) * 8)

/**
 * Time Stamp
 **/
#define NGBE_TSRXCTL		0x015188
#define   NGBE_TSRXCTL_VLD	MS(0, 0x1)
#define   NGBE_TSRXCTL_TYPE(v)	LS(v, 1, 0x7)
#define     NGBE_TSRXCTL_TYPE_V2L2	(0)
#define     NGBE_TSRXCTL_TYPE_V1L4	(1)
#define     NGBE_TSRXCTL_TYPE_V2L24	(2)
#define     NGBE_TSRXCTL_TYPE_V2EVENT	(5)
#define   NGBE_TSRXCTL_ENA	MS(4, 0x1)
#define NGBE_TSRXSTMPL		0x0151E8
#define NGBE_TSRXSTMPH		0x0151A4
#define NGBE_TSTXCTL		0x011F00
#define   NGBE_TSTXCTL_VLD	MS(0, 0x1)
#define   NGBE_TSTXCTL_ENA	MS(4, 0x1)
#define NGBE_TSTXSTMPL		0x011F04
#define NGBE_TSTXSTMPH		0x011F08
#define NGBE_TSTIMEL		0x011F0C
#define NGBE_TSTIMEH		0x011F10
#define NGBE_TSTIMEINC		0x011F14
#define   NGBE_TSTIMEINC_IV(v)	LS(v, 0, 0x7FFFFFF)

/**
 * Wake on Lan
 **/
#define NGBE_WOLCTL               0x015B80
#define NGBE_WOLIPCTL             0x015B84
#define NGBE_WOLIP4(i)            (0x015BC0 + (i) * 4) /* 0-3 */
#define NGBE_WOLIP6(i)            (0x015BE0 + (i) * 4) /* 0-3 */

#define NGBE_WOLFLEXCTL           0x015CFC
#define NGBE_WOLFLEXI             0x015B8C
#define NGBE_WOLFLEXDAT(i)        (0x015C00 + (i) * 16) /* 0-15 */
#define NGBE_WOLFLEXMSK(i)        (0x015C08 + (i) * 16) /* 0-15 */

/******************************************************************************
 * Security Registers
 ******************************************************************************/
#define NGBE_SECRXCTL			0x017000
#define   NGBE_SECRXCTL_ODSA		MS(0, 0x1)
#define   NGBE_SECRXCTL_XDSA		MS(1, 0x1)
#define   NGBE_SECRXCTL_CRCSTRIP	MS(2, 0x1)
#define   NGBE_SECRXCTL_SAVEBAD		MS(6, 0x1)
#define NGBE_SECRXSTAT			0x017004
#define   NGBE_SECRXSTAT_RDY		MS(0, 0x1)
#define   NGBE_SECRXSTAT_ECC		MS(1, 0x1)

#define NGBE_SECTXCTL			0x01D000
#define   NGBE_SECTXCTL_ODSA		MS(0, 0x1)
#define   NGBE_SECTXCTL_XDSA		MS(1, 0x1)
#define   NGBE_SECTXCTL_STFWD		MS(2, 0x1)
#define   NGBE_SECTXCTL_MSKIV		MS(3, 0x1)
#define NGBE_SECTXSTAT			0x01D004
#define   NGBE_SECTXSTAT_RDY		MS(0, 0x1)
#define   NGBE_SECTXSTAT_ECC		MS(1, 0x1)
#define NGBE_SECTXBUFAF			0x01D008
#define NGBE_SECTXBUFAE			0x01D00C
#define NGBE_SECTXIFG			0x01D020
#define   NGBE_SECTXIFG_MIN(v)		LS(v, 0, 0xF)
#define   NGBE_SECTXIFG_MIN_MASK	MS(0, 0xF)

/**
 * LinkSec
 **/
#define NGBE_LSECRXCAP	               0x017200
#define NGBE_LSECRXCTL                0x017204
	/* disabled(0),check(1),strict(2),drop(3) */
#define   NGBE_LSECRXCTL_MODE_MASK    MS(2, 0x3)
#define   NGBE_LSECRXCTL_MODE_STRICT  LS(2, 2, 0x3)
#define   NGBE_LSECRXCTL_POSTHDR      MS(6, 0x1)
#define   NGBE_LSECRXCTL_REPLAY       MS(7, 0x1)
#define NGBE_LSECRXSCIL               0x017208
#define NGBE_LSECRXSCIH               0x01720C
#define NGBE_LSECRXSA(i)              (0x017210 + (i) * 4) /* 0-1 */
#define NGBE_LSECRXPN(i)              (0x017218 + (i) * 4) /* 0-1 */
#define NGBE_LSECRXKEY(n, i)	       (0x017220 + 0x10 * (n) + 4 * (i)) /*0-3*/
#define NGBE_LSECTXCAP                0x01D200
#define NGBE_LSECTXCTL                0x01D204
	/* disabled(0), auth(1), auth+encrypt(2) */
#define   NGBE_LSECTXCTL_MODE_MASK    MS(0, 0x3)
#define   NGBE_LSECTXCTL_MODE_AUTH    LS(1, 0, 0x3)
#define   NGBE_LSECTXCTL_MODE_AENC    LS(2, 0, 0x3)
#define   NGBE_LSECTXCTL_PNTRH_MASK   MS(8, 0xFFFFFF)
#define   NGBE_LSECTXCTL_PNTRH(v)     LS(v, 8, 0xFFFFFF)
#define NGBE_LSECTXSCIL               0x01D208
#define NGBE_LSECTXSCIH               0x01D20C
#define NGBE_LSECTXSA                 0x01D210
#define NGBE_LSECTXPN0                0x01D214
#define NGBE_LSECTXPN1                0x01D218
#define NGBE_LSECTXKEY0(i)            (0x01D21C + (i) * 4) /* 0-3 */
#define NGBE_LSECTXKEY1(i)            (0x01D22C + (i) * 4) /* 0-3 */

#define NGBE_LSECRX_UTPKT             0x017240
#define NGBE_LSECRX_DECOCT            0x017244
#define NGBE_LSECRX_VLDOCT            0x017248
#define NGBE_LSECRX_BTPKT             0x01724C
#define NGBE_LSECRX_NOSCIPKT          0x017250
#define NGBE_LSECRX_UNSCIPKT          0x017254
#define NGBE_LSECRX_UNCHKPKT          0x017258
#define NGBE_LSECRX_DLYPKT            0x01725C
#define NGBE_LSECRX_LATEPKT           0x017260
#define NGBE_LSECRX_OKPKT(i)          (0x017264 + (i) * 4) /* 0-1 */
#define NGBE_LSECRX_BADPKT(i)         (0x01726C + (i) * 4) /* 0-1 */
#define NGBE_LSECRX_INVPKT(i)         (0x017274 + (i) * 4) /* 0-1 */
#define NGBE_LSECRX_BADSAPKT(i)       (0x01727C + (i) * 8) /* 0-3 */
#define NGBE_LSECRX_INVSAPKT(i)       (0x017280 + (i) * 8) /* 0-3 */
#define NGBE_LSECTX_UTPKT             0x01D23C
#define NGBE_LSECTX_ENCPKT            0x01D240
#define NGBE_LSECTX_PROTPKT           0x01D244
#define NGBE_LSECTX_ENCOCT            0x01D248
#define NGBE_LSECTX_PROTOCT           0x01D24C

/******************************************************************************
 * MAC Registers
 ******************************************************************************/
#define NGBE_MACRXCFG                  0x011004
#define   NGBE_MACRXCFG_ENA            MS(0, 0x1)
#define   NGBE_MACRXCFG_JUMBO          MS(8, 0x1)
#define   NGBE_MACRXCFG_LB             MS(10, 0x1)
#define NGBE_MACCNTCTL                 0x011800
#define   NGBE_MACCNTCTL_RC            MS(2, 0x1)

#define NGBE_MACRXFLT                  0x011008
#define   NGBE_MACRXFLT_PROMISC        MS(0, 0x1)
#define   NGBE_MACRXFLT_CTL_MASK       MS(6, 0x3)
#define   NGBE_MACRXFLT_CTL_DROP       LS(0, 6, 0x3)
#define   NGBE_MACRXFLT_CTL_NOPS       LS(1, 6, 0x3)
#define   NGBE_MACRXFLT_CTL_NOFT       LS(2, 6, 0x3)
#define   NGBE_MACRXFLT_CTL_PASS       LS(3, 6, 0x3)
#define   NGBE_MACRXFLT_RXALL          MS(31, 0x1)

/******************************************************************************
 * Statistic Registers
 ******************************************************************************/
/* Ring Counter */
#define NGBE_QPRXPKT(rp)                 (0x001014 + 0x40 * (rp))
#define NGBE_QPRXOCTL(rp)                (0x001018 + 0x40 * (rp))
#define NGBE_QPRXOCTH(rp)                (0x00101C + 0x40 * (rp))
#define NGBE_QPRXMPKT(rp)                (0x001020 + 0x40 * (rp))
#define NGBE_QPRXBPKT(rp)                (0x001024 + 0x40 * (rp))
#define NGBE_QPTXPKT(rp)                 (0x003014 + 0x40 * (rp))
#define NGBE_QPTXOCTL(rp)                (0x003018 + 0x40 * (rp))
#define NGBE_QPTXOCTH(rp)                (0x00301C + 0x40 * (rp))
#define NGBE_QPTXMPKT(rp)                (0x003020 + 0x40 * (rp))
#define NGBE_QPTXBPKT(rp)                (0x003024 + 0x40 * (rp))

/* TDMA Counter */
#define NGBE_DMATXDROP			0x018300
#define NGBE_DMATXSECDROP		0x018304
#define NGBE_DMATXPKT			0x018308
#define NGBE_DMATXOCTL			0x01830C
#define NGBE_DMATXOCTH			0x018310
#define NGBE_DMATXMNG			0x018314

/* RDMA Counter */
#define NGBE_DMARXDROP			0x012500
#define NGBE_DMARXPKT			0x012504
#define NGBE_DMARXOCTL			0x012508
#define NGBE_DMARXOCTH			0x01250C
#define NGBE_DMARXMNG			0x012510

/* Packet Buffer Counter */
#define NGBE_PBRXMISS			0x019040
#define NGBE_PBRXPKT			0x019060
#define NGBE_PBRXREP			0x019064
#define NGBE_PBRXDROP			0x019068
#define NGBE_PBLBSTAT			0x01906C
#define   NGBE_PBLBSTAT_FREE(r)		RS(r, 0, 0x3FF)
#define   NGBE_PBLBSTAT_FULL		MS(11, 0x1)
#define NGBE_PBRXWRPTR			0x019180
#define   NGBE_PBRXWRPTR_HEAD(r)	RS(r, 0, 0xFFFF)
#define   NGBE_PBRXWRPTR_TAIL(r)	RS(r, 16, 0xFFFF)
#define NGBE_PBRXRDPTR			0x0191A0
#define   NGBE_PBRXRDPTR_HEAD(r)	RS(r, 0, 0xFFFF)
#define   NGBE_PBRXRDPTR_TAIL(r)	RS(r, 16, 0xFFFF)
#define NGBE_PBRXDATA			0x0191C0
#define   NGBE_PBRXDATA_RDPTR(r)	RS(r, 0, 0xFFFF)
#define   NGBE_PBRXDATA_WRPTR(r)	RS(r, 16, 0xFFFF)
#define NGBE_PBRX_USDSP			0x0191E0
#define NGBE_RXPBPFCDMACL		0x019210
#define NGBE_RXPBPFCDMACH		0x019214
#define NGBE_PBTXLNKXOFF		0x019218
#define NGBE_PBTXLNKXON			0x01921C

#define NGBE_PBTXSTAT			0x01C004
#define   NGBE_PBTXSTAT_EMPT(tc, r)	((1 << (tc) & (r)) >> (tc))

#define NGBE_PBRXLNKXOFF		0x011988
#define NGBE_PBRXLNKXON			0x011E0C

#define NGBE_PBLPBK			0x01CF08

/* Ether Flow Counter */
#define NGBE_LANPKTDROP			0x0151C0
#define NGBE_MNGPKTDROP			0x0151C4

#define NGBE_PSRLANPKTCNT		0x0151B8
#define NGBE_PSRMNGPKTCNT		0x0151BC

/* MAC Counter */
#define NGBE_MACRXERRCRCL           0x011928
#define NGBE_MACRXERRCRCH           0x01192C
#define NGBE_MACRXERRLENL           0x011978
#define NGBE_MACRXERRLENH           0x01197C
#define NGBE_MACRX1TO64L            0x011940
#define NGBE_MACRX1TO64H            0x011944
#define NGBE_MACRX65TO127L          0x011948
#define NGBE_MACRX65TO127H          0x01194C
#define NGBE_MACRX128TO255L         0x011950
#define NGBE_MACRX128TO255H         0x011954
#define NGBE_MACRX256TO511L         0x011958
#define NGBE_MACRX256TO511H         0x01195C
#define NGBE_MACRX512TO1023L        0x011960
#define NGBE_MACRX512TO1023H        0x011964
#define NGBE_MACRX1024TOMAXL        0x011968
#define NGBE_MACRX1024TOMAXH        0x01196C
#define NGBE_MACTX1TO64L            0x011834
#define NGBE_MACTX1TO64H            0x011838
#define NGBE_MACTX65TO127L          0x01183C
#define NGBE_MACTX65TO127H          0x011840
#define NGBE_MACTX128TO255L         0x011844
#define NGBE_MACTX128TO255H         0x011848
#define NGBE_MACTX256TO511L         0x01184C
#define NGBE_MACTX256TO511H         0x011850
#define NGBE_MACTX512TO1023L        0x011854
#define NGBE_MACTX512TO1023H        0x011858
#define NGBE_MACTX1024TOMAXL        0x01185C
#define NGBE_MACTX1024TOMAXH        0x011860

#define NGBE_MACRXUNDERSIZE         0x011938
#define NGBE_MACRXOVERSIZE          0x01193C
#define NGBE_MACRXJABBER            0x011934

#define NGBE_MACRXPKTL                0x011900
#define NGBE_MACRXPKTH                0x011904
#define NGBE_MACTXPKTL                0x01181C
#define NGBE_MACTXPKTH                0x011820
#define NGBE_MACRXGBOCTL              0x011908
#define NGBE_MACRXGBOCTH              0x01190C
#define NGBE_MACTXGBOCTL              0x011814
#define NGBE_MACTXGBOCTH              0x011818

#define NGBE_MACRXOCTL                0x011918
#define NGBE_MACRXOCTH                0x01191C
#define NGBE_MACRXMPKTL               0x011920
#define NGBE_MACRXMPKTH               0x011924
#define NGBE_MACTXOCTL                0x011824
#define NGBE_MACTXOCTH                0x011828
#define NGBE_MACTXMPKTL               0x01182C
#define NGBE_MACTXMPKTH               0x011830

/* Management Counter */
#define NGBE_MNGOUT		0x01CF00
#define NGBE_MNGIN		0x01CF04
#define NGBE_MNGDROP		0x01CF0C

/* MAC SEC Counter */
#define NGBE_LSECRXUNTAG	0x017240
#define NGBE_LSECRXDECOCT	0x017244
#define NGBE_LSECRXVLDOCT	0x017248
#define NGBE_LSECRXBADTAG	0x01724C
#define NGBE_LSECRXNOSCI	0x017250
#define NGBE_LSECRXUKSCI	0x017254
#define NGBE_LSECRXUNCHK	0x017258
#define NGBE_LSECRXDLY		0x01725C
#define NGBE_LSECRXLATE		0x017260
#define NGBE_LSECRXGOOD		0x017264
#define NGBE_LSECRXBAD		0x01726C
#define NGBE_LSECRXUK		0x017274
#define NGBE_LSECRXBADSA	0x01727C
#define NGBE_LSECRXUKSA		0x017280
#define NGBE_LSECTXUNTAG	0x01D23C
#define NGBE_LSECTXENC		0x01D240
#define NGBE_LSECTXPTT		0x01D244
#define NGBE_LSECTXENCOCT	0x01D248
#define NGBE_LSECTXPTTOCT	0x01D24C

/* Management Counter */
#define NGBE_MNGOS2BMC                 0x01E094
#define NGBE_MNGBMC2OS                 0x01E090

/******************************************************************************
 * PF(Physical Function) Registers
 ******************************************************************************/
/* Interrupt */
#define NGBE_BMECTL		0x012020
#define   NGBE_BMECTL_VFDRP	MS(1, 0x1)
#define   NGBE_BMECTL_PFDRP	MS(0, 0x1)
#define NGBE_ICRMISC		0x000100
#define   NGBE_ICRMISC_MASK	MS(8, 0xFFFFFF)
#define   NGBE_ICRMISC_RST	MS(10, 0x1) /* device reset event */
#define   NGBE_ICRMISC_TS	MS(11, 0x1) /* time sync */
#define   NGBE_ICRMISC_STALL	MS(12, 0x1) /* trans or recv path is stalled */
#define   NGBE_ICRMISC_LNKSEC	MS(13, 0x1) /* Tx LinkSec require key exchange*/
#define   NGBE_ICRMISC_ERRBUF	MS(14, 0x1) /* Packet Buffer Overrun */
#define   NGBE_ICRMISC_ERRMAC	MS(17, 0x1) /* err reported by MAC */
#define   NGBE_ICRMISC_PHY	MS(18, 0x1) /* interrupt reported by eth phy */
#define   NGBE_ICRMISC_ERRIG	MS(20, 0x1) /* integrity error */
#define   NGBE_ICRMISC_SPI	MS(21, 0x1) /* SPI interface */
#define   NGBE_ICRMISC_VFMBX	MS(23, 0x1) /* VF-PF message box */
#define   NGBE_ICRMISC_GPIO	MS(26, 0x1) /* GPIO interrupt */
#define   NGBE_ICRMISC_ERRPCI	MS(27, 0x1) /* pcie request error */
#define   NGBE_ICRMISC_HEAT	MS(28, 0x1) /* overheat detection */
#define   NGBE_ICRMISC_PROBE	MS(29, 0x1) /* probe match */
#define   NGBE_ICRMISC_MNGMBX	MS(30, 0x1) /* mng mailbox */
#define   NGBE_ICRMISC_TIMER	MS(31, 0x1) /* tcp timer */
#define   NGBE_ICRMISC_DEFAULT	( \
			NGBE_ICRMISC_RST | \
			NGBE_ICRMISC_ERRMAC | \
			NGBE_ICRMISC_PHY | \
			NGBE_ICRMISC_ERRIG | \
			NGBE_ICRMISC_GPIO | \
			NGBE_ICRMISC_VFMBX | \
			NGBE_ICRMISC_MNGMBX | \
			NGBE_ICRMISC_STALL | \
			NGBE_ICRMISC_TIMER)
#define NGBE_ICSMISC			0x000104
#define NGBE_IENMISC			0x000108
#define NGBE_IVARMISC			0x0004FC
#define   NGBE_IVARMISC_VEC(v)		LS(v, 0, 0x7)
#define   NGBE_IVARMISC_VLD		MS(7, 0x1)
#define NGBE_ICR(i)			(0x000120 + (i) * 4) /*0*/
#define   NGBE_ICR_MASK			MS(0, 0x1FF)
#define NGBE_ICS(i)			(0x000130 + (i) * 4) /*0*/
#define   NGBE_ICS_MASK			NGBE_ICR_MASK
#define NGBE_IMS(i)			(0x000140 + (i) * 4) /*0*/
#define   NGBE_IMS_MASK			NGBE_ICR_MASK
#define NGBE_IMC(i)			(0x000150 + (i) * 4) /*0*/
#define   NGBE_IMC_MASK			NGBE_ICR_MASK
#define NGBE_IVAR(i)			(0x000500 + (i) * 4) /*0-3*/
#define   NGBE_IVAR_VEC(v)		LS(v, 0, 0x7)
#define   NGBE_IVAR_VLD			MS(7, 0x1)
#define NGBE_TCPTMR			0x000170
#define NGBE_ITRSEL			0x000180

/* P2V Mailbox */
#define NGBE_MBMEM(i)		(0x005000 + 0x40 * (i)) /*0-7*/
#define NGBE_MBCTL(i)		(0x000600 + 4 * (i)) /*0-7*/
#define   NGBE_MBCTL_STS	MS(0, 0x1) /* Initiate message send to VF */
#define   NGBE_MBCTL_ACK	MS(1, 0x1) /* Ack message recv'd from VF */
#define   NGBE_MBCTL_VFU	MS(2, 0x1) /* VF owns the mailbox buffer */
#define   NGBE_MBCTL_PFU	MS(3, 0x1) /* PF owns the mailbox buffer */
#define   NGBE_MBCTL_RVFU	MS(4, 0x1) /* Reset VFU - used when VF stuck */
#define NGBE_MBVFICR			0x000480
#define   NGBE_MBVFICR_INDEX(vf)	((vf) >> 4)
#define   NGBE_MBVFICR_VFREQ_MASK	(0x0000FFFF) /* bits for VF messages */
#define   NGBE_MBVFICR_VFREQ_VF1	(0x00000001) /* bit for VF 1 message */
#define   NGBE_MBVFICR_VFACK_MASK	(0xFFFF0000) /* bits for VF acks */
#define   NGBE_MBVFICR_VFACK_VF1	(0x00010000) /* bit for VF 1 ack */
#define NGBE_FLRVFP			0x000490
#define NGBE_FLRVFE			0x0004A0
#define NGBE_FLRVFEC			0x0004A8

/******************************************************************************
 * VF(Virtual Function) Registers
 ******************************************************************************/
#define NGBE_VFPBWRAP			0x000000
#define   NGBE_VFPBWRAP_WRAP		MS(0, 0x7)
#define   NGBE_VFPBWRAP_EMPT		MS(3, 0x1)
#define NGBE_VFSTATUS			0x000004
#define   NGBE_VFSTATUS_UP		MS(0, 0x1)
#define   NGBE_VFSTATUS_BW_MASK		MS(1, 0x7)
#define     NGBE_VFSTATUS_BW_1G		LS(0x1, 1, 0x7)
#define     NGBE_VFSTATUS_BW_100M	LS(0x2, 1, 0x7)
#define     NGBE_VFSTATUS_BW_10M	LS(0x4, 1, 0x7)
#define   NGBE_VFSTATUS_BUSY		MS(4, 0x1)
#define   NGBE_VFSTATUS_LANID		MS(8, 0x3)
#define NGBE_VFRST			0x000008
#define   NGBE_VFRST_SET		MS(0, 0x1)
#define NGBE_VFMSIXECC			0x00000C
#define NGBE_VFPLCFG			0x000078
#define   NGBE_VFPLCFG_RSV		MS(0, 0x1)
#define   NGBE_VFPLCFG_PSR(v)		LS(v, 1, 0x1F)
#define     NGBE_VFPLCFG_PSRL4HDR	(0x1)
#define     NGBE_VFPLCFG_PSRL3HDR	(0x2)
#define     NGBE_VFPLCFG_PSRL2HDR	(0x4)
#define     NGBE_VFPLCFG_PSRTUNHDR	(0x8)
#define     NGBE_VFPLCFG_PSRTUNMAC	(0x10)
#define NGBE_VFICR			0x000100
#define   NGBE_VFICR_MASK		LS(3, 0, 0x3)
#define   NGBE_VFICR_MBX		MS(1, 0x1)
#define   NGBE_VFICR_DONE1		MS(0, 0x1)
#define NGBE_VFICS			0x000104
#define   NGBE_VFICS_MASK		NGBE_VFICR_MASK
#define NGBE_VFIMS			0x000108
#define   NGBE_VFIMS_MASK		NGBE_VFICR_MASK
#define NGBE_VFIMC			0x00010C
#define   NGBE_VFIMC_MASK		NGBE_VFICR_MASK
#define NGBE_VFGPIE			0x000118
#define NGBE_VFIVAR(i)			(0x000240 + 4 * (i)) /*0-1*/
#define NGBE_VFIVARMISC			0x000260
#define   NGBE_VFIVAR_ALLOC(v)		LS(v, 0, 0x1)
#define   NGBE_VFIVAR_VLD		MS(7, 0x1)

#define NGBE_VFMBCTL			0x000600
#define   NGBE_VFMBCTL_REQ         MS(0, 0x1) /* Request for PF Ready bit */
#define   NGBE_VFMBCTL_ACK         MS(1, 0x1) /* Ack PF message received */
#define   NGBE_VFMBCTL_VFU         MS(2, 0x1) /* VF owns the mailbox buffer */
#define   NGBE_VFMBCTL_PFU         MS(3, 0x1) /* PF owns the mailbox buffer */
#define   NGBE_VFMBCTL_PFSTS       MS(4, 0x1) /* PF wrote a message in the MB */
#define   NGBE_VFMBCTL_PFACK       MS(5, 0x1) /* PF ack the previous VF msg */
#define   NGBE_VFMBCTL_RSTI        MS(6, 0x1) /* PF has reset indication */
#define   NGBE_VFMBCTL_RSTD        MS(7, 0x1) /* PF has indicated reset done */
#define   NGBE_VFMBCTL_R2C_BITS		(NGBE_VFMBCTL_RSTD | \
					NGBE_VFMBCTL_PFSTS | \
					NGBE_VFMBCTL_PFACK)
#define NGBE_VFMBX			0x000C00 /*0-15*/
#define NGBE_VFTPHCTL(i)		0x000D00

/******************************************************************************
 * PF&VF TxRx Interface
 ******************************************************************************/
#define RNGLEN(v)     ROUND_OVER(v, 13, 7)
#define HDRLEN(v)     ROUND_OVER(v, 10, 6)
#define PKTLEN(v)     ROUND_OVER(v, 14, 10)
#define INTTHR(v)     ROUND_OVER(v, 4,  0)

#define	NGBE_RING_DESC_ALIGN	128
#define	NGBE_RING_DESC_MIN	128
#define	NGBE_RING_DESC_MAX	8192
#define NGBE_RXD_ALIGN		NGBE_RING_DESC_ALIGN
#define NGBE_TXD_ALIGN		NGBE_RING_DESC_ALIGN

/* receive ring */
#define NGBE_RXBAL(rp)                 (0x001000 + 0x40 * (rp))
#define NGBE_RXBAH(rp)                 (0x001004 + 0x40 * (rp))
#define NGBE_RXRP(rp)                  (0x00100C + 0x40 * (rp))
#define NGBE_RXWP(rp)                  (0x001008 + 0x40 * (rp))
#define NGBE_RXCFG(rp)                 (0x001010 + 0x40 * (rp))
#define   NGBE_RXCFG_ENA               MS(0, 0x1)
#define   NGBE_RXCFG_RNGLEN(v)         LS(RNGLEN(v), 1, 0x3F)
#define   NGBE_RXCFG_PKTLEN(v)         LS(PKTLEN(v), 8, 0xF)
#define     NGBE_RXCFG_PKTLEN_MASK     MS(8, 0xF)
#define   NGBE_RXCFG_HDRLEN(v)         LS(HDRLEN(v), 12, 0xF)
#define     NGBE_RXCFG_HDRLEN_MASK     MS(12, 0xF)
#define   NGBE_RXCFG_WTHRESH(v)        LS(v, 16, 0x7)
#define   NGBE_RXCFG_ETAG              MS(22, 0x1)
#define   NGBE_RXCFG_SPLIT             MS(26, 0x1)
#define   NGBE_RXCFG_CNTAG             MS(28, 0x1)
#define   NGBE_RXCFG_DROP              MS(30, 0x1)
#define   NGBE_RXCFG_VLAN              MS(31, 0x1)

/* transmit ring */
#define NGBE_TXBAL(rp)                 (0x003000 + 0x40 * (rp)) /*0-7*/
#define NGBE_TXBAH(rp)                 (0x003004 + 0x40 * (rp))
#define NGBE_TXWP(rp)                  (0x003008 + 0x40 * (rp))
#define NGBE_TXRP(rp)                  (0x00300C + 0x40 * (rp))
#define NGBE_TXCFG(rp)                 (0x003010 + 0x40 * (rp))
#define   NGBE_TXCFG_ENA               MS(0, 0x1)
#define   NGBE_TXCFG_BUFLEN_MASK       MS(1, 0x3F)
#define   NGBE_TXCFG_BUFLEN(v)         LS(RNGLEN(v), 1, 0x3F)
#define   NGBE_TXCFG_HTHRESH_MASK      MS(8, 0xF)
#define   NGBE_TXCFG_HTHRESH(v)        LS(v, 8, 0xF)
#define   NGBE_TXCFG_WTHRESH_MASK      MS(16, 0x7F)
#define   NGBE_TXCFG_WTHRESH(v)        LS(v, 16, 0x7F)
#define   NGBE_TXCFG_FLUSH             MS(26, 0x1)

/* interrupt registers */
#define NGBE_BMEPEND			0x000168
#define   NGBE_BMEPEND_ST		MS(0, 0x1)
#define NGBE_ITRI			0x000180
#define NGBE_ITR(i)			(0x000200 + 4 * (i))
#define   NGBE_ITR_IVAL_MASK		MS(2, 0x1FFF) /* 1ns/10G, 10ns/REST */
#define   NGBE_ITR_IVAL(v)		LS(v, 2, 0x1FFF) /*1ns/10G, 10ns/REST*/
#define     NGBE_ITR_IVAL_1G(us)	NGBE_ITR_IVAL((us) / 2)
#define     NGBE_ITR_IVAL_10G(us)	NGBE_ITR_IVAL((us) / 20)
#define   NGBE_ITR_LLIEA		MS(15, 0x1)
#define   NGBE_ITR_LLICREDIT(v)		LS(v, 16, 0x1F)
#define   NGBE_ITR_CNT(v)		LS(v, 21, 0x3FF)
#define   NGBE_ITR_WRDSA		MS(31, 0x1)
#define NGBE_GPIE			0x000118
#define   NGBE_GPIE_MSIX		MS(0, 0x1)
#define   NGBE_GPIE_LLIEA		MS(1, 0x1)
#define   NGBE_GPIE_LLIVAL(v)		LS(v, 3, 0x1F)
#define   NGBE_GPIE_LLIVAL_H(v)		LS(v, 16, 0x7FF)

/******************************************************************************
 * Debug Registers
 ******************************************************************************/
/**
 * Probe
 **/
#define NGBE_PRBCTL                    0x010200
#define NGBE_PRBSTA                    0x010204
#define NGBE_PRBDAT                    0x010220
#define NGBE_PRBCNT                    0x010228

#define NGBE_PRBPCI                    0x01F010
#define NGBE_PRBPSR                    0x015010
#define NGBE_PRBRDB                    0x019010
#define NGBE_PRBTDB                    0x01C010
#define NGBE_PRBRSEC                   0x017010
#define NGBE_PRBTSEC                   0x01D010
#define NGBE_PRBMNG                    0x01E010
#define NGBE_PRBRMAC                   0x011014
#define NGBE_PRBTMAC                   0x011010
#define NGBE_PRBREMAC                  0x011E04
#define NGBE_PRBTEMAC                  0x011E00

/**
 * ECC
 **/
#define NGBE_ECCRXPBCTL                0x019014
#define NGBE_ECCRXPBINJ                0x019018
#define NGBE_ECCRXPB                   0x01901C
#define NGBE_ECCTXPBCTL                0x01C014
#define NGBE_ECCTXPBINJ                0x01C018
#define NGBE_ECCTXPB                   0x01C01C

#define NGBE_ECCRXETHCTL               0x015014
#define NGBE_ECCRXETHINJ               0x015018
#define NGBE_ECCRXETH                  0x01401C

#define NGBE_ECCRXSECCTL               0x017014
#define NGBE_ECCRXSECINJ               0x017018
#define NGBE_ECCRXSEC                  0x01701C
#define NGBE_ECCTXSECCTL               0x01D014
#define NGBE_ECCTXSECINJ               0x01D018
#define NGBE_ECCTXSEC                  0x01D01C

#define NGBE_P2VMBX_SIZE          (16) /* 16*4B */
#define NGBE_P2MMBX_SIZE          (64) /* 64*4B */

/**************** Global Registers ****************************/
#define NGBE_POOLETHCTL(pl)            (0x015600 + (pl) * 4)
#define   NGBE_POOLETHCTL_LBDIA        MS(0, 0x1)
#define   NGBE_POOLETHCTL_LLBDIA       MS(1, 0x1)
#define   NGBE_POOLETHCTL_LLB          MS(2, 0x1)
#define   NGBE_POOLETHCTL_UCP          MS(4, 0x1)
#define   NGBE_POOLETHCTL_ETP          MS(5, 0x1)
#define   NGBE_POOLETHCTL_VLA          MS(6, 0x1)
#define   NGBE_POOLETHCTL_VLP          MS(7, 0x1)
#define   NGBE_POOLETHCTL_UTA          MS(8, 0x1)
#define   NGBE_POOLETHCTL_MCHA         MS(9, 0x1)
#define   NGBE_POOLETHCTL_UCHA         MS(10, 0x1)
#define   NGBE_POOLETHCTL_BCA          MS(11, 0x1)
#define   NGBE_POOLETHCTL_MCP          MS(12, 0x1)
#define NGBE_POOLDROPSWBK(i)           (0x0151C8 + (i) * 4) /*0-1*/

/**************************** Receive DMA registers **************************/

#define NGBE_RPUP2TC                   0x019008
#define   NGBE_RPUP2TC_UP_SHIFT        3
#define   NGBE_RPUP2TC_UP_MASK         0x7

/* mac switcher */
#define NGBE_ETHADDRL                  0x016200
#define   NGBE_ETHADDRL_AD0(v)         LS(v, 0, 0xFF)
#define   NGBE_ETHADDRL_AD1(v)         LS(v, 8, 0xFF)
#define   NGBE_ETHADDRL_AD2(v)         LS(v, 16, 0xFF)
#define   NGBE_ETHADDRL_AD3(v)         LS(v, 24, 0xFF)
#define   NGBE_ETHADDRL_ETAG(r)        RS(r, 0, 0x3FFF)
#define NGBE_ETHADDRH                  0x016204
#define   NGBE_ETHADDRH_AD4(v)         LS(v, 0, 0xFF)
#define   NGBE_ETHADDRH_AD5(v)         LS(v, 8, 0xFF)
#define   NGBE_ETHADDRH_AD_MASK        MS(0, 0xFFFF)
#define   NGBE_ETHADDRH_ETAG           MS(30, 0x1)
#define   NGBE_ETHADDRH_VLD            MS(31, 0x1)
#define NGBE_ETHADDRASS                0x016208
#define NGBE_ETHADDRIDX                0x016210

/* Outmost Barrier Filters */
#define NGBE_MCADDRTBL(i)              (0x015200 + (i) * 4) /*0-127*/
#define NGBE_UCADDRTBL(i)              (0x015400 + (i) * 4) /*0-127*/
#define NGBE_VLANTBL(i)                (0x016000 + (i) * 4) /*0-127*/

#define NGBE_MNGFLEXSEL                0x1582C
#define NGBE_MNGFLEXDWL(i)             (0x15A00 + ((i) * 16))
#define NGBE_MNGFLEXDWH(i)             (0x15A04 + ((i) * 16))
#define NGBE_MNGFLEXMSK(i)             (0x15A08 + ((i) * 16))

#define NGBE_LANFLEXSEL                0x15B8C
#define NGBE_LANFLEXDWL(i)             (0x15C00 + ((i) * 16))
#define NGBE_LANFLEXDWH(i)             (0x15C04 + ((i) * 16))
#define NGBE_LANFLEXMSK(i)             (0x15C08 + ((i) * 16))
#define NGBE_LANFLEXCTL                0x15CFC

/* ipsec */
#define NGBE_IPSRXIDX                  0x017100
#define   NGBE_IPSRXIDX_ENA            MS(0, 0x1)
#define   NGBE_IPSRXIDX_TB_MASK        MS(1, 0x3)
#define   NGBE_IPSRXIDX_TB_IP          LS(1, 1, 0x3)
#define   NGBE_IPSRXIDX_TB_SPI         LS(2, 1, 0x3)
#define   NGBE_IPSRXIDX_TB_KEY         LS(3, 1, 0x3)
#define   NGBE_IPSRXIDX_TBIDX(v)       LS(v, 3, 0xF)
#define   NGBE_IPSRXIDX_READ           MS(30, 0x1)
#define   NGBE_IPSRXIDX_WRITE          MS(31, 0x1)
#define NGBE_IPSRXADDR(i)              (0x017104 + (i) * 4)

#define NGBE_IPSRXSPI                  0x017114
#define NGBE_IPSRXADDRIDX              0x017118
#define NGBE_IPSRXKEY(i)               (0x01711C + (i) * 4)
#define NGBE_IPSRXSALT                 0x01712C
#define NGBE_IPSRXMODE                 0x017130
#define   NGBE_IPSRXMODE_IPV6          0x00000010
#define   NGBE_IPSRXMODE_DEC           0x00000008
#define   NGBE_IPSRXMODE_ESP           0x00000004
#define   NGBE_IPSRXMODE_AH            0x00000002
#define   NGBE_IPSRXMODE_VLD           0x00000001
#define NGBE_IPSTXIDX                  0x01D100
#define   NGBE_IPSTXIDX_ENA            MS(0, 0x1)
#define   NGBE_IPSTXIDX_SAIDX(v)       LS(v, 3, 0x3FF)
#define   NGBE_IPSTXIDX_READ           MS(30, 0x1)
#define   NGBE_IPSTXIDX_WRITE          MS(31, 0x1)
#define NGBE_IPSTXSALT                 0x01D104
#define NGBE_IPSTXKEY(i)               (0x01D108 + (i) * 4)

#define NGBE_MACTXCFG                  0x011000
#define   NGBE_MACTXCFG_TE             MS(0, 0x1)
#define   NGBE_MACTXCFG_SPEED_MASK     MS(29, 0x3)
#define   NGBE_MACTXCFG_SPEED(v)       LS(v, 29, 0x3)
#define   NGBE_MACTXCFG_SPEED_10G      LS(0, 29, 0x3)
#define   NGBE_MACTXCFG_SPEED_1G       LS(3, 29, 0x3)

#define NGBE_ISBADDRL                  0x000160
#define NGBE_ISBADDRH                  0x000164

#define NGBE_ARBPOOLIDX                0x01820C
#define NGBE_ARBTXRATE                 0x018404
#define   NGBE_ARBTXRATE_MIN(v)        LS(v, 0, 0x3FFF)
#define   NGBE_ARBTXRATE_MAX(v)        LS(v, 16, 0x3FFF)

/* qos */
#define NGBE_ARBTXCTL                  0x018200
#define   NGBE_ARBTXCTL_RRM            MS(1, 0x1)
#define   NGBE_ARBTXCTL_WSP            MS(2, 0x1)
#define   NGBE_ARBTXCTL_DIA            MS(6, 0x1)
#define NGBE_ARBTXMMW                  0x018208

/* Management */
#define NGBE_MNGFWSYNC            0x01E000
#define   NGBE_MNGFWSYNC_REQ      MS(0, 0x1)
#define NGBE_MNGSWSYNC            0x01E004
#define   NGBE_MNGSWSYNC_REQ      MS(0, 0x1)
#define NGBE_SWSEM                0x01002C
#define   NGBE_SWSEM_PF           MS(0, 0x1)
#define NGBE_MNGSEM               0x01E008
#define   NGBE_MNGSEM_SW(v)       LS(v, 0, 0xFFFF)
#define   NGBE_MNGSEM_SWPHY       MS(0, 0x1)
#define   NGBE_MNGSEM_SWMBX       MS(2, 0x1)
#define   NGBE_MNGSEM_SWFLASH     MS(3, 0x1)
#define   NGBE_MNGSEM_FW(v)       LS(v, 16, 0xFFFF)
#define   NGBE_MNGSEM_FWPHY       MS(16, 0x1)
#define   NGBE_MNGSEM_FWMBX       MS(18, 0x1)
#define   NGBE_MNGSEM_FWFLASH     MS(19, 0x1)
#define NGBE_MNGMBXCTL            0x01E044
#define   NGBE_MNGMBXCTL_SWRDY    MS(0, 0x1)
#define   NGBE_MNGMBXCTL_SWACK    MS(1, 0x1)
#define   NGBE_MNGMBXCTL_FWRDY    MS(2, 0x1)
#define   NGBE_MNGMBXCTL_FWACK    MS(3, 0x1)
#define NGBE_MNGMBX               0x01E100

/**
 * MDIO(PHY)
 **/
#define NGBE_MDIOSCA                   0x011200
#define   NGBE_MDIOSCA_REG(v)          LS(v, 0, 0xFFFF)
#define   NGBE_MDIOSCA_PORT(v)         LS(v, 16, 0x1F)
#define   NGBE_MDIOSCA_DEV(v)          LS(v, 21, 0x1F)
#define NGBE_MDIOSCD                   0x011204
#define   NGBE_MDIOSCD_DAT_R(r)        RS(r, 0, 0xFFFF)
#define   NGBE_MDIOSCD_DAT(v)          LS(v, 0, 0xFFFF)
#define   NGBE_MDIOSCD_CMD_PREAD       LS(2, 16, 0x3)
#define   NGBE_MDIOSCD_CMD_WRITE       LS(1, 16, 0x3)
#define   NGBE_MDIOSCD_CMD_READ        LS(3, 16, 0x3)
#define   NGBE_MDIOSCD_SADDR           MS(18, 0x1)
#define   NGBE_MDIOSCD_CLOCK(v)        LS(v, 19, 0x7)
#define   NGBE_MDIOSCD_BUSY            MS(22, 0x1)

#define NGBE_MDIOMODE			0x011220
#define   NGBE_MDIOMODE_MASK		MS(0, 0xF)
#define   NGBE_MDIOMODE_PRT3CL22	MS(3, 0x1)
#define   NGBE_MDIOMODE_PRT2CL22	MS(2, 0x1)
#define   NGBE_MDIOMODE_PRT1CL22	MS(1, 0x1)
#define   NGBE_MDIOMODE_PRT0CL22	MS(0, 0x1)

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
#define NVM_VER_SIZE		32    /* version string size */

#define NGBE_REG_RSSTBL   NGBE_RSSTBL(0)
#define NGBE_REG_RSSKEY   NGBE_RSSKEY(0)

/*
 * read non-rc counters
 */
#define NGBE_UPDCNT32(reg, last, cur)                           \
do {                                                             \
	uint32_t latest = rd32(hw, reg);                         \
	if (hw->offset_loaded || hw->rx_loaded)			 \
		last = 0;					 \
	cur += (latest - last) & UINT_MAX;                       \
	last = latest;                                           \
} while (0)

#define NGBE_UPDCNT36(regl, last, cur)                          \
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
#define NGBE_REG_READ32(addr)               rte_read32(addr)
#define NGBE_REG_READ32_RELAXED(addr)       rte_read32_relaxed(addr)
#define NGBE_REG_WRITE32(addr, val)         rte_write32(val, addr)
#define NGBE_REG_WRITE32_RELAXED(addr, val) rte_write32_relaxed(val, addr)

#define NGBE_DEAD_READ_REG         0xdeadbeefU
#define NGBE_FAILED_READ_REG       0xffffffffU
#define NGBE_REG_ADDR(hw, reg) \
	((volatile u32 *)((char *)(hw)->hw_addr + (reg)))

static inline u32
ngbe_get32(volatile u32 *addr)
{
	u32 val = NGBE_REG_READ32(addr);
	return rte_le_to_cpu_32(val);
}

static inline void
ngbe_set32(volatile u32 *addr, u32 val)
{
	val = rte_cpu_to_le_32(val);
	NGBE_REG_WRITE32(addr, val);
}

static inline u32
ngbe_get32_masked(volatile u32 *addr, u32 mask)
{
	u32 val = ngbe_get32(addr);
	val &= mask;
	return val;
}

static inline void
ngbe_set32_masked(volatile u32 *addr, u32 mask, u32 field)
{
	u32 val = ngbe_get32(addr);
	val = ((val & ~mask) | (field & mask));
	ngbe_set32(addr, val);
}

static inline u32
ngbe_get32_relaxed(volatile u32 *addr)
{
	u32 val = NGBE_REG_READ32_RELAXED(addr);
	return rte_le_to_cpu_32(val);
}

static inline void
ngbe_set32_relaxed(volatile u32 *addr, u32 val)
{
	val = rte_cpu_to_le_32(val);
	NGBE_REG_WRITE32_RELAXED(addr, val);
}

static inline u32
rd32(struct ngbe_hw *hw, u32 reg)
{
	if (reg == NGBE_REG_DUMMY)
		return 0;
	return ngbe_get32(NGBE_REG_ADDR(hw, reg));
}

static inline void
wr32(struct ngbe_hw *hw, u32 reg, u32 val)
{
	if (reg == NGBE_REG_DUMMY)
		return;
	ngbe_set32(NGBE_REG_ADDR(hw, reg), val);
}

static inline u32
rd32m(struct ngbe_hw *hw, u32 reg, u32 mask)
{
	u32 val = rd32(hw, reg);
	val &= mask;
	return val;
}

static inline void
wr32m(struct ngbe_hw *hw, u32 reg, u32 mask, u32 field)
{
	u32 val = rd32(hw, reg);
	val = ((val & ~mask) | (field & mask));
	wr32(hw, reg, val);
}

static inline u64
rd64(struct ngbe_hw *hw, u32 reg)
{
	u64 lsb = rd32(hw, reg);
	u64 msb = rd32(hw, reg + 4);
	return (lsb | msb << 32);
}

static inline void
wr64(struct ngbe_hw *hw, u32 reg, u64 val)
{
	wr32(hw, reg, (u32)val);
	wr32(hw, reg + 4, (u32)(val >> 32));
}

/* poll register */
static inline u32
po32m(struct ngbe_hw *hw, u32 reg, u32 mask, u32 expect, u32 *actual,
	u32 loop, u32 slice)
{
	bool usec = true;
	u32 value = 0, all = 0;

	if (slice > 1000 * MAX_UDELAY_MS) {
		usec = false;
		slice = (slice + 500) / 1000;
	}

	do {
		if (expect != 0) {
			all |= rd32(hw, reg);
			value |= mask & all;
		} else {
			all = rd32(hw, reg);
			value = mask & all;
		}
		if (value == expect)
			break;

		usec ? usec_delay(slice) : msec_delay(slice);
	} while (--loop > 0);

	if (actual)
		*actual = all;

	return loop;
}

/* flush all write operations */
#define ngbe_flush(hw) rd32(hw, 0x00100C)

#define rd32a(hw, reg, idx) ( \
	rd32((hw), (reg) + ((idx) << 2)))
#define wr32a(hw, reg, idx, val) \
	wr32((hw), (reg) + ((idx) << 2), (val))

#define rd32w(hw, reg, mask, slice) do { \
	rd32((hw), reg); \
	po32m((hw), reg, mask, mask, NULL, 5, slice); \
} while (0)

#define wr32w(hw, reg, val, mask, slice) do { \
	wr32((hw), reg, val); \
	po32m((hw), reg, mask, mask, NULL, 5, slice); \
} while (0)

#define NGBE_XPCS_IDAADDR    0x13000
#define NGBE_XPCS_IDADATA    0x13004
#define NGBE_EPHY_IDAADDR    0x13008
#define NGBE_EPHY_IDADATA    0x1300C
static inline u32
rd32_epcs(struct ngbe_hw *hw, u32 addr)
{
	u32 data;
	wr32(hw, NGBE_XPCS_IDAADDR, addr);
	data = rd32(hw, NGBE_XPCS_IDADATA);
	return data;
}

static inline void
wr32_epcs(struct ngbe_hw *hw, u32 addr, u32 data)
{
	wr32(hw, NGBE_XPCS_IDAADDR, addr);
	wr32(hw, NGBE_XPCS_IDADATA, data);
}

static inline u32
rd32_ephy(struct ngbe_hw *hw, u32 addr)
{
	u32 data;
	wr32(hw, NGBE_EPHY_IDAADDR, addr);
	data = rd32(hw, NGBE_EPHY_IDADATA);
	return data;
}

static inline void
wr32_ephy(struct ngbe_hw *hw, u32 addr, u32 data)
{
	wr32(hw, NGBE_EPHY_IDAADDR, addr);
	wr32(hw, NGBE_EPHY_IDADATA, data);
}

#endif /* _NGBE_REGS_H_ */
