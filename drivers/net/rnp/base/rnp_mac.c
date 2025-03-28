/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Mucse IC Design Ltd.
 */

#include "rnp_osdep.h"

#include "rnp_mbx_fw.h"
#include "rnp_mac.h"
#include "rnp_eth_regs.h"
#include "rnp_mac_regs.h"
#include "../rnp.h"

static int
rnp_update_mpfm_indep(struct rnp_eth_port *port, u32 mode, bool en)
{
	u32 nr_lane = port->attr.nr_lane;
	struct rnp_hw *hw = port->hw;
	u32 disable = 0, enable = 0;
	u32 reg;

	reg = RNP_MAC_REG_RD(hw, nr_lane, RNP_MAC_PKT_FLT_CTRL);
	/* make sure not all receive modes are available */
	reg &= ~RNP_MAC_RA;
	switch (mode) {
	case RNP_MPF_MODE_NONE:
		break;
	case RNP_MPF_MODE_ALLMULTI:
		enable = RNP_MAC_PM;
		disable = 0;
		break;
	case RNP_MPF_MODE_PROMISC:
		enable = RNP_MAC_PROMISC_EN;
		disable = 0;
		break;
	default:
		RNP_PMD_LOG(ERR, "update_mpfm argument is invalid");
		return -EINVAL;
	}
	if (en) {
		reg &= ~disable;
		reg |= enable;
	} else {
		reg &= ~enable;
		reg |= disable;
	}
	/* disable common filter when indep mode */
	reg |= RNP_MAC_HPF;
	RNP_MAC_REG_WR(hw, nr_lane, RNP_MAC_PKT_FLT_CTRL, reg);
	RNP_MAC_REG_WR(hw, nr_lane, RNP_MAC_FCTRL, RNP_MAC_FCTRL_BYPASS);

	return 0;
}

static int
rnp_update_mpfm_pf(struct rnp_eth_port *port, u32 mode, bool en)
{
	u32 nr_lane = port->attr.nr_lane;
	struct rnp_hw *hw = port->hw;
	u32 mac_filter_ctrl;
	u32 filter_ctrl;
	u32 bypass_ctrl;
	u32 bypass = 0;

	bypass_ctrl = RNP_E_REG_RD(hw, RNP_MAC_FCTRL);
	bypass_ctrl |= RNP_MAC_FCTRL_BAM;

	filter_ctrl = RNP_MAC_MULTICASE_TBL_EN | RNP_MAC_UNICASE_TBL_EN;
	RNP_E_REG_WR(hw, RNP_MAC_MCSTCTRL, filter_ctrl);

	switch (mode) {
	case RNP_MPF_MODE_NONE:
		bypass = 0;
		break;
	case RNP_MPF_MODE_ALLMULTI:
		bypass = RNP_MAC_FCTRL_MPE;
		break;
	case RNP_MPF_MODE_PROMISC:
		bypass = RNP_MAC_FCTRL_UPE | RNP_MAC_FCTRL_MPE;
		break;
	default:
		RNP_PMD_LOG(ERR, "update_mpfm argument is invalid");
		return -EINVAL;
	}
	if (en)
		bypass_ctrl |= bypass;
	else
		bypass_ctrl &= ~bypass;

	RNP_E_REG_WR(hw, RNP_MAC_FCTRL, bypass_ctrl);
	mac_filter_ctrl = RNP_MAC_REG_RD(hw, nr_lane, RNP_MAC_PKT_FLT_CTRL);
	mac_filter_ctrl |= RNP_MAC_PM | RNP_MAC_PROMISC_EN;
	mac_filter_ctrl &= ~RNP_MAC_RA;
	RNP_MAC_REG_WR(hw, nr_lane, RNP_MAC_PKT_FLT_CTRL, mac_filter_ctrl);

	return 0;
}

const struct rnp_mac_ops rnp_mac_ops_pf = {
	.get_macaddr = rnp_mbx_fw_get_macaddr,
	.update_mpfm = rnp_update_mpfm_pf,
};

const struct rnp_mac_ops rnp_mac_ops_indep = {
	.get_macaddr = rnp_mbx_fw_get_macaddr,
	.update_mpfm = rnp_update_mpfm_indep,
};

int rnp_get_mac_addr(struct rnp_eth_port *port, u8 *mac)
{
	const struct rnp_mac_ops *mac_ops =
		RNP_DEV_PP_TO_MAC_OPS(port->eth_dev);

	return rnp_call_hwif_impl(port, mac_ops->get_macaddr, mac);
}

int rnp_update_mpfm(struct rnp_eth_port *port,
		    u32 mode, bool en)
{
	const struct rnp_mac_ops *mac_ops =
		RNP_DEV_PP_TO_MAC_OPS(port->eth_dev);

	return rnp_call_hwif_impl(port, mac_ops->update_mpfm, mode, en);
}

void rnp_mac_ops_init(struct rnp_hw *hw)
{
	struct rnp_proc_priv *proc_priv = RNP_DEV_TO_PROC_PRIV(hw->back->eth_dev);

	if (rnp_pf_is_multiple_ports(hw->device_id))
		proc_priv->mac_ops = &rnp_mac_ops_indep;
	else
		proc_priv->mac_ops = &rnp_mac_ops_pf;
}
