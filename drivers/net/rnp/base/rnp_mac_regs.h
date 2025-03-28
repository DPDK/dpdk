/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Mucse IC Design Ltd.
 */

#ifndef _RNP_MAC_REGS_H_
#define _RNP_MAC_REGS_H_

#define RNP_MAC_BASE_OFFSET(n)  (_MAC_(0) + ((0x10000) * (n)))

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


#endif /* _RNP_MAC_REGS_H_ */
