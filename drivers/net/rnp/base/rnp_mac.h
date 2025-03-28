/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Mucse IC Design Ltd.
 */

#ifndef _RNP_MAC_H_
#define _RNP_MAC_H_

#include "rnp_osdep.h"
#include "rnp_hw.h"

void rnp_mac_ops_init(struct rnp_hw *hw);
int rnp_get_mac_addr(struct rnp_eth_port *port, u8 *mac);

#endif /* _RNP_MAC_H_ */
