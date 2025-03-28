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

#define RNP_MAC_VFE		RTE_BIT32(16)
/* mac link ctrl */
#define RNP_MAC_LPI_CTRL	(0xd0)
/* PHY Link Status Disable */
#define RNP_MAC_PLSDIS		RTE_BIT32(18)
/* PHY Link Status */
#define RNP_MAC_PLS		RTE_BIT32(17)

#endif /* _RNP_MAC_REGS_H_ */
