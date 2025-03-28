/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Mucse IC Design Ltd.
 */

#ifndef _RNP_ETH_REGS_H
#define _RNP_ETH_REGS_H

#define RNP_E_ENG_BYPASS	_ETH_(0x8000)
#define RNP_E_VXLAN_PARSE_EN	_ETH_(0x8004)
#define RNP_E_FILTER_EN		_ETH_(0x801c)
#define RNP_E_REDIR_EN		_ETH_(0x8030)

#define RNP_TC_PORT_OFFSET(lane)	_ETH_(0xe840 + 0x04 * (lane))

#endif /* _RNP_ETH_REGS_H */
