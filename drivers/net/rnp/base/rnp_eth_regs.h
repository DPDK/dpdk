/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Mucse IC Design Ltd.
 */

#ifndef _RNP_ETH_REGS_H
#define _RNP_ETH_REGS_H

#define RNP_E_ENG_BYPASS	_ETH_(0x8000)
#define RNP_E_VXLAN_PARSE_EN	_ETH_(0x8004)
#define RNP_E_FILTER_EN		_ETH_(0x801c)
#define RNP_E_REDIR_EN		_ETH_(0x8030)

/* rx queue flow ctrl */
#define RNP_RX_FC_ENABLE	_ETH_(0x8520)
#define RNP_RING_FC_EN(n)	_ETH_(0x8524 + ((0x4) * ((n) / 32)))
#define RNP_RING_FC_THRESH(n)	_ETH_(0x8a00 + ((0x4) * (n)))
/* Mac Host Filter  */
#define RNP_MAC_FCTRL		_ETH_(0x9110)
#define RNP_MAC_FCTRL_MPE	RTE_BIT32(8)  /* Multicast Promiscuous En */
#define RNP_MAC_FCTRL_UPE	RTE_BIT32(9)  /* Unicast Promiscuous En */
#define RNP_MAC_FCTRL_BAM	RTE_BIT32(10) /* Broadcast Accept Mode */
#define RNP_MAC_FCTRL_BYPASS	(\
		RNP_MAC_FCTRL_MPE | \
		RNP_MAC_FCTRL_UPE | \
		RNP_MAC_FCTRL_BAM)
/* Mucast unicast mac hash filter ctrl */
#define RNP_MAC_MCSTCTRL		_ETH_(0x9114)
#define RNP_MAC_HASH_MASK		RTE_GENMASK32(11, 0)
#define RNP_MAC_MULTICASE_TBL_EN	RTE_BIT32(2)
#define RNP_MAC_UNICASE_TBL_EN		RTE_BIT32(3)

#define RNP_TC_PORT_OFFSET(lane)	_ETH_(0xe840 + 0x04 * (lane))

#endif /* _RNP_ETH_REGS_H */
