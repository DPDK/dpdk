/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Mucse IC Design Ltd.
 */

#ifndef _RNP_MBX_FW_H_
#define _RNP_MBX_FW_H_

#include "rnp_osdep.h"
#include "rnp_hw.h"

struct rnp_eth_port;

int rnp_mbx_fw_get_macaddr(struct rnp_eth_port *port, u8 *mac_addr);
int rnp_mbx_fw_get_capability(struct rnp_eth_port *port);
int rnp_mbx_fw_get_lane_stat(struct rnp_eth_port *port);
int rnp_mbx_fw_reset_phy(struct rnp_hw *hw);
int rnp_mbx_fw_pf_link_event_en(struct rnp_eth_port *port, bool en);
int rnp_fw_init(struct rnp_hw *hw);
int rnp_rcv_msg_from_fw(struct rnp_eth_adapter *adapter, u32 *msgbuf);
int rnp_fw_mbx_ifup_down(struct rnp_eth_port *port, int up);
int rnp_mbx_fw_lane_link_event_en(struct rnp_eth_port *port, bool en);

#endif /* _RNP_MBX_FW_H_ */
