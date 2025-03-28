/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Mucse IC Design Ltd.
 */

#ifndef _RNP_MAC_H_
#define _RNP_MAC_H_

#include "rnp_osdep.h"
#include "rnp_hw.h"
#include "rnp_eth_regs.h"

#define RNP_RX_ETH_DISABLE(hw, nr_lane) do { \
	wmb(); \
	RNP_E_REG_WR(hw, RNP_RX_ETH_F_CTRL(nr_lane), \
			RNP_RX_ETH_F_OFF); \
} while (0)

#define RNP_RX_ETH_ENABLE(hw, nr_lane) do { \
	wmb(); \
	RNP_E_REG_WR(hw, RNP_RX_ETH_F_CTRL(nr_lane), \
			RNP_RX_ETH_F_ON); \
} while (0)


void rnp_mac_ops_init(struct rnp_hw *hw);
int rnp_get_mac_addr(struct rnp_eth_port *port, u8 *mac);
int rnp_set_macaddr(struct rnp_eth_port *port, u8 *mac, u32 index);
int rnp_clear_macaddr(struct rnp_eth_port *port, u32 index);
int rnp_update_mpfm(struct rnp_eth_port *port,
		    u32 mode, bool en);
int rnp_rx_vlan_filter_en(struct rnp_eth_port *port, bool en);

#endif /* _RNP_MAC_H_ */
