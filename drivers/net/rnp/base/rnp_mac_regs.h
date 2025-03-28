/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Mucse IC Design Ltd.
 */

#ifndef _RNP_MAC_REGS_H_
#define _RNP_MAC_REGS_H_

#define RNP_MAC_BASE_OFFSET(n)  (_MAC_(0) + ((0x10000) * (n)))

#define RNP_MAC_TX_CFG		(0x0)
/* Transmitter Enable */
#define RNP_MAC_TE		RTE_BIT32(0)
/* Jabber Disable */
#define RNP_MAC_JD		RTE_BIT32(16)
#define RNP_SPEED_SEL_MASK	RTE_GENMASK32(30, 28)
#define RNP_SPEED_SEL_S		(28)
#define RNP_SPEED_SEL_1G	(b111 << RNP_SPEED_SEL_S)
#define RNP_SPEED_SEL_10G	(b010 << RNP_SPEED_SEL_S)
#define RNP_SPEED_SEL_40G	(b000 << RNP_SPEED_SEL_S)

#define RNP_MAC_RX_CFG		(0x4)
/* Receiver Enable */
#define RNP_MAC_RE		RTE_BIT32(0)
/* Automatic Pad or CRC Stripping */
#define RNP_MAC_ACS		RTE_BIT32(1)
/* CRC stripping for Type packets */
#define RNP_MAC_CST		RTE_BIT32(2)
/* Disable CRC Check */
#define RNP_MAC_DCRCC		RTE_BIT32(3)
/* Enable Max Frame Size Limit */
#define RNP_MAC_GPSLCE		RTE_BIT32(6)
/* Watchdog Disable */
#define RNP_MAC_WD		RTE_BIT32(7)
/* Jumbo Packet Support En */
#define RNP_MAC_JE		RTE_BIT32(8)
/* Enable IPC */
#define RNP_MAC_IPC		RTE_BIT32(9)
/* Loopback Mode */
#define RNP_MAC_LM		RTE_BIT32(10)
/* Giant Packet Size Limit */
#define RNP_MAC_GPSL_MASK	RTE_GENMASK32(29, 16)
#define RNP_MAC_MAX_GPSL	(1518)
#define RNP_MAC_CPSL_SHIFT	(16)

#define RNP_MAC_PKT_FLT_CTRL	(0x8)
/* Receive All */
#define RNP_MAC_RA		RTE_BIT32(31)
/* Pass Control Packets */
#define RNP_MAC_PCF		RTE_GENMASK32(7, 6)
#define RNP_MAC_PCF_S		(6)
/* Mac Filter ALL Ctrl Frame */
#define RNP_MAC_PCF_FAC		(0)
/* Mac Forward ALL Ctrl Frame Except Pause */
#define RNP_MAC_PCF_NO_PAUSE	(1)
/* Mac Forward All Ctrl Pkt */
#define RNP_MAC_PCF_PA		(2)
/* Mac Forward Ctrl Frame Match Unicast */
#define RNP_MAC_PCF_PUN		(3)
/* Promiscuous Mode */
#define RNP_MAC_PROMISC_EN	RTE_BIT32(0)
/* Hash Unicast */
#define RNP_MAC_HUC		RTE_BIT32(1)
/* Hash Multicast */
#define RNP_MAC_HMC		RTE_BIT32(2)
/*  Pass All Multicast */
#define RNP_MAC_PM		RTE_BIT32(4)
/* Disable Broadcast Packets */
#define RNP_MAC_DBF		RTE_BIT32(5)
/* Hash or Perfect Filter */
#define RNP_MAC_HPF		RTE_BIT32(10)
#define RNP_MAC_VTFE		RTE_BIT32(16)
/* mac vlan ctrl reg */
#define RNP_MAC_VLAN_TAG	(0x50)
/* En Double Vlan Processing */
#define RNP_MAC_VLAN_EDVLP	RTE_BIT32(26)
/* VLAN Tag Hash Table Match Enable */
#define RNP_MAC_VLAN_VTHM	RTE_BIT32(25)
/*  Enable VLAN Tag in Rx status */
#define RNP_MAC_VLAN_EVLRXS	RTE_BIT32(24)
/* Disable VLAN Type Check */
#define RNP_MAC_VLAN_DOVLTC	RTE_BIT32(20)
/* Enable S-VLAN */
#define RNP_MAC_VLAN_ESVL	RTE_BIT32(18)
/* Enable 12-Bit VLAN Tag Comparison Filter */
#define RNP_MAC_VLAN_ETV	RTE_BIT32(16)
/* enable vid valid  */
#define RNP_MAC_VLAN_HASH_EN	RTE_GENMASK32(15, 0)
/* MAC VLAN CTRL INSERT REG */
#define RNP_MAC_VLAN_INCL	(0x60)
#define RNP_MAC_INNER_VLAN_INCL	(0x64)
/* VLAN_Tag Insert From Description */
#define RNP_MAC_VLAN_VLTI	RTE_BIT32(20)
/* C-VLAN or S-VLAN */
#define RNP_MAC_VLAN_CSVL		RTE_BIT32(19)
#define RNP_MAC_VLAN_INSERT_CVLAN	(0 << 19)
#define RNP_MAC_VLAN_INSERT_SVLAN	(1 << 19)
/* VLAN Tag Control in Transmit Packets */
#define RNP_MAC_VLAN_VLC		RTE_GENMASK32(17, 16)
/* VLAN Tag Control Offset Bit */
#define RNP_MAC_VLAN_VLC_SHIFT		(16)
/* Don't Anything ON TX VLAN*/
#define RNP_MAC_VLAN_VLC_NONE		(0x0 << RNP_MAC_VLAN_VLC_SHIFT)
/* MAC Delete VLAN */
#define RNP_MAC_VLAN_VLC_DEL		(0x1 << RNP_MAC_VLAN_VLC_SHIFT)
/* MAC Add VLAN */
#define RNP_MAC_VLAN_VLC_ADD		(0x2 << RNP_MAC_VLAN_VLC_SHIFT)
/* MAC Replace VLAN */
#define RNP_MAC_VLAN_VLC_REPLACE	(0x3 << RNP_MAC_VLAN_VLC_SHIFT)
/* VLAN Tag for Transmit Packets For Insert/Remove */
#define RNP_MAC_VLAN_VLT		RTE_GENMASK32(15, 0)
/* mac link ctrl */
#define RNP_MAC_LPI_CTRL	(0xd0)
/* PHY Link Status Disable */
#define RNP_MAC_PLSDIS		RTE_BIT32(18)
/* PHY Link Status */
#define RNP_MAC_PLS		RTE_BIT32(17)

/* Rx macaddr filter ctrl */
#define RNP_MAC_ADDR_HI(n)	(0x0300 + ((n) * 0x8))
#define RNP_MAC_AE		RTE_BIT32(31)
#define RNP_MAC_ADDR_LO(n)	(0x0304 + ((n) * 0x8))
/* Mac Manage Counts */
#define RNP_MMC_CTRL		(0x0800)
#define RNP_MMC_RSTONRD	RTE_BIT32(2)
/* Tx Good And Bad Bytes Base */
#define RNP_MMC_TX_GBOCTGB	(0x0814)
/* Tx Good And Bad Frame Num Base */
#define RNP_MMC_TX_GBFRMB	(0x081c)
/* Tx Good Broadcast Frame Num Base */
#define RNP_MMC_TX_BCASTB	(0x0824)
/* Tx Good Multicast Frame Num Base */
#define RNP_MMC_TX_MCASTB	(0x082c)
/* Tx 64Bytes Frame Num */
#define RNP_MMC_TX_64_BYTESB	(0x0834)
#define RNP_MMC_TX_65TO127_BYTESB	(0x083c)
#define RNP_MMC_TX_128TO255_BYTEB	(0x0844)
#define RNP_MMC_TX_256TO511_BYTEB	(0x084c)
#define RNP_MMC_TX_512TO1023_BYTEB	(0x0854)
#define RNP_MMC_TX_1024TOMAX_BYTEB	(0x085c)
/* Tx Good And Bad Unicast Frame Num Base */
#define RNP_MMC_TX_GBUCASTB		(0x0864)
/* Tx Good And Bad Multicast Frame Num Base */
#define RNP_MMC_TX_GBMCASTB		(0x086c)
/* Tx Good And Bad Broadcast Frame NUM Base */
#define RNP_MMC_TX_GBBCASTB		(0x0874)
/* Tx Frame Underflow Error */
#define RNP_MMC_TX_UNDRFLWB		(0x087c)
/* Tx Good Frame Bytes Base */
#define RNP_MMC_TX_GBYTESB		(0x0884)
/* Tx Good Frame Num Base*/
#define RNP_MMC_TX_GBRMB		(0x088c)
/* Tx Good Pause Frame Num Base */
#define RNP_MMC_TX_PAUSEB		(0x0894)
/* Tx Good Vlan Frame Num Base */
#define RNP_MMC_TX_VLANB		(0x089c)

/* Rx Good And Bad Frames Num Base */
#define RNP_MMC_RX_GBFRMB		(0x0900)
/* Rx Good And Bad Frames Bytes Base */
#define RNP_MMC_RX_GBOCTGB		(0x0908)
/* Rx Good Framse Bytes Base */
#define RNP_MMC_RX_GOCTGB		(0x0910)
/* Rx Good Broadcast Frames Num Base */
#define RNP_MMC_RX_BCASTGB		(0x0918)
/* Rx Good Multicast Frames Num Base */
#define RNP_MMC_RX_MCASTGB		(0x0920)
/* Rx Crc Error Frames Num Base */
#define RNP_MMC_RX_CRCERB		(0x0928)
/* Rx Less Than 64Byes with Crc Err Base*/
#define RNP_MMC_RX_RUNTERB		(0x0930)
/* Receive Jumbo Frame Error */
#define RNP_MMC_RX_JABBER_ERR		(0x0934)
/* Shorter Than 64Bytes without Any Errora Base */
#define RNP_MMC_RX_USIZEGB		(0x0938)
/* Len Oversize 9k */
#define RNP_MMC_RX_OSIZEGB		(0x093c)
/* Rx 64Byes Frame Num Base */
#define RNP_MMC_RX_64_BYTESB		(0x0940)
/* Rx 65Bytes To 127Bytes Frame Num Base */
#define RNP_MMC_RX_65TO127_BYTESB	(0x0948)
/* Rx 128Bytes To 255Bytes Frame Num Base */
#define RNP_MMC_RX_128TO255_BYTESB	(0x0950)
/* Rx 256Bytes To 511Bytes Frame Num Base */
#define RNP_MMC_RX_256TO511_BYTESB	(0x0958)
/* Rx 512Bytes To 1023Bytes Frame Num Base */
#define RNP_MMC_RX_512TO1203_BYTESB	(0x0960)
/* Rx Len Bigger Than 1024Bytes Base */
#define RNP_MMC_RX_1024TOMAX_BYTESB	(0x0968)
/* Rx Unicast Frame Good Num Base */
#define RNP_MMC_RX_UCASTGB		(0x0970)
/* Rx Length Error Of Frame Part */
#define RNP_MMC_RX_LENERRB		(0x0978)
/* Rx received with a Length field not equal to the valid frame size */
#define RNP_MMC_RX_OUTOF_RANGE		(0x0980)
/* Rx Pause Frame Good Num Base */
#define RNP_MMC_RX_PAUSEB		(0x0988)
/* Rx Vlan Frame Good Num Base */
#define RNP_MMC_RX_VLANGB		(0x0998)
/* Rx With A Watchdog Timeout Err Frame Base */
#define RNP_MMC_RX_WDOGERRB		(0x09a0)

#endif /* _RNP_MAC_REGS_H_ */
