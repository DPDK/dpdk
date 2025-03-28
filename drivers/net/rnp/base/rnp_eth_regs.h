/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Mucse IC Design Ltd.
 */

#ifndef _RNP_ETH_REGS_H
#define _RNP_ETH_REGS_H

#define RNP_ETH_TX_FIFO_STATE		_ETH_(0x0330)
#define RNP_ETH_TX_FIFO_EMPT(lane)	((1 << (lane)) | (1 << ((lane) + 4)))

#define RNP_E_ENG_BYPASS	_ETH_(0x8000)
#define RNP_E_VXLAN_PARSE_EN	_ETH_(0x8004)
#define RNP_E_FILTER_EN		_ETH_(0x801c)
#define RNP_E_REDIR_EN		_ETH_(0x8030)

#define RNP_RX_ETH_F_CTRL(n)	_ETH_(0x8070 + ((n) * 0x8))
#define RNP_RX_ETH_F_OFF	(0x7ff)
#define RNP_RX_ETH_F_ON		(0x270)
/* max/min pkts length receive limit ctrl */
#define RNP_MIN_FRAME_CTRL	_ETH_(0x80f0)
#define RNP_MAX_FRAME_CTRL	_ETH_(0x80f4)
/* rx queue flow ctrl */
#define RNP_RX_FC_ENABLE	_ETH_(0x8520)
#define RNP_RING_FC_EN(n)	_ETH_(0x8524 + ((0x4) * ((n) / 32)))
#define RNP_RING_FC_THRESH(n)	_ETH_(0x8a00 + ((0x4) * (n)))
/* ETH Statistic */
#define RNP_ETH_RXTRANS_DROP	_ETH_(0x8904)
#define RNP_ETH_RXSLAN_DROP	_ETH_(0x8914)
#define RNP_ETH_RXGLAN_DROP	_ETH_(0x8918)
#define RNP_ETH_RXIPH_E_DROP	_ETH_(0x891c)
#define RNP_ETH_RXCKSUM_E_DROP	_ETH_(0x8920)
#define RNP_ETH_RXTRUNC_DROP	_ETH_(0x8928)
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
/* rss function ctrl */
#define RNP_RSS_INNER_CTRL	_ETH_(0x805c)
#define RNP_INNER_RSS_EN	(1)
#define RNP_INNER_RSS_DIS	(0)
#define RNP_RSS_REDIR_TB(n, id) _ETH_(0xe000 + ((n) * 0x200) + ((id) * 0x4))
#define RNP_RSS_MRQC_ADDR	_ETH_(0x92a0)
/* RSS policy */
#define RNP_RSS_HASH_CFG_MASK	(0x3F30000)
#define RNP_RSS_HASH_IPV4_TCP	RTE_BIT32(16)
#define RNP_RSS_HASH_IPV4	RTE_BIT32(17)
#define RNP_RSS_HASH_IPV6	RTE_BIT32(20)
#define RNP_RSS_HASH_IPV6_TCP	RTE_BIT32(21)
#define RNP_RSS_HASH_IPV4_UDP	RTE_BIT32(22)
#define RNP_RSS_HASH_IPV6_UDP   RTE_BIT32(23)
#define RNP_RSS_HASH_IPV4_SCTP  RTE_BIT32(24)
#define RNP_RSS_HASH_IPV6_SCTP  RTE_BIT32(25)
/* rss hash key */
#define RNP_RSS_KEY_TABLE(idx)	_ETH_(0x92d0 + ((idx) * 0x4))

#define RNP_TC_PORT_OFFSET(lane)	_ETH_(0xe840 + 0x04 * (lane))
/* host mac address filter */
#define RNP_RAL_BASE_ADDR(n)	_ETH_(0xA000 + (0x04 * (n)))
#define RNP_RAH_BASE_ADDR(n)	_ETH_(0xA400 + (0x04 * (n)))
#define RNP_MAC_FILTER_EN	RTE_BIT32(31)

#endif /* _RNP_ETH_REGS_H */
