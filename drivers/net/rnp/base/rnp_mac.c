/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Mucse IC Design Ltd.
 */

#include "rnp_osdep.h"

#include "rnp_mbx_fw.h"
#include "rnp_mac.h"
#include "../rnp.h"

const struct rnp_mac_ops rnp_mac_ops_pf = {
	.get_macaddr = rnp_mbx_fw_get_macaddr,
};

int rnp_get_mac_addr(struct rnp_eth_port *port, u8 *mac)
{
	const struct rnp_mac_ops *mac_ops =
		RNP_DEV_PP_TO_MAC_OPS(port->eth_dev);

	return rnp_call_hwif_impl(port, mac_ops->get_macaddr, mac);
}

void rnp_mac_ops_init(struct rnp_hw *hw)
{
	struct rnp_proc_priv *proc_priv = RNP_DEV_TO_PROC_PRIV(hw->back->eth_dev);

	proc_priv->mac_ops = &rnp_mac_ops_pf;
}
