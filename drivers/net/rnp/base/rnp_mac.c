/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Mucse IC Design Ltd.
 */

#include "rnp_osdep.h"

#include "rnp_mbx_fw.h"
#include "rnp_mac.h"
#include "rnp_eth_regs.h"
#include "rnp_mac_regs.h"
#include "rnp_bitrev.h"
#include "rnp_crc32.h"
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

static int
rnp_set_mac_addr_pf(struct rnp_eth_port *port,
		    const u8 *addr, u32 index)
{
	struct rnp_hw *hw = port->hw;
	u32 addr_hi = 0, addr_lo = 0;
	u8 *mac = NULL;

	mac = (u8 *)&addr_hi;
	mac[0] = addr[1];
	mac[1] = addr[0];
	mac = (u8 *)&addr_lo;
	mac[0] = addr[5];
	mac[1] = addr[4];
	mac[2] = addr[3];
	mac[3] = addr[2];
	addr_hi |= RNP_MAC_FILTER_EN;
	RNP_E_REG_WR(hw, RNP_RAH_BASE_ADDR(index), addr_hi);
	RNP_E_REG_WR(hw, RNP_RAL_BASE_ADDR(index), addr_lo);

	return 0;
}

static int
rnp_set_mac_addr_indep(struct rnp_eth_port *port,
		       const u8 *addr, u32 index)
{
	u16 lane = port->attr.nr_lane;
	struct rnp_hw *hw = port->hw;
	u32 addr_hi = 0, addr_lo = 0;
	u8 *mac = NULL;

	mac = (u8 *)&addr_lo;
	mac[0] = addr[0];
	mac[1] = addr[1];
	mac[2] = addr[2];
	mac[3] = addr[3];
	mac = (u8 *)&addr_hi;
	mac[0] = addr[4];
	mac[1] = addr[5];

	addr_hi |= RNP_MAC_AE;
	RNP_MAC_REG_WR(hw, lane, RNP_MAC_ADDR_HI(index), addr_hi);
	RNP_MAC_REG_WR(hw, lane, RNP_MAC_ADDR_LO(index), addr_lo);

	return 0;
}

static int
rnp_clear_mac_pf(struct rnp_eth_port *port, u32 index)
{
	struct rnp_hw *hw = port->hw;

	RNP_E_REG_WR(hw, RNP_RAL_BASE_ADDR(index), 0);
	RNP_E_REG_WR(hw, RNP_RAH_BASE_ADDR(index), 0);

	return 0;
}

static int
rnp_clear_mac_indep(struct rnp_eth_port *port, u32 index)
{
	u16 lane = port->attr.nr_lane;
	struct rnp_hw *hw = port->hw;

	RNP_MAC_REG_WR(hw, lane, RNP_MAC_ADDR_HI(index), 0);
	RNP_MAC_REG_WR(hw, lane, RNP_MAC_ADDR_LO(index), 0);

	return 0;
}

static int
rnp_en_vlan_filter_pf(struct rnp_eth_port *port, bool en)
{
	struct rnp_hw *hw = port->hw;
	u32 ctrl;

	/* enable/disable all vlan filter configuration */
	ctrl = RNP_E_REG_RD(hw, RNP_VLAN_FILTER_CTRL);
	if (en)
		ctrl |= RNP_VLAN_FILTER_EN;
	else
		ctrl &= ~RNP_VLAN_FILTER_EN;
	RNP_E_REG_WR(hw, RNP_VLAN_FILTER_CTRL, ctrl);

	return 0;
}

static int
rnp_en_vlan_filter_indep(struct rnp_eth_port *port, bool en)
{
	u16 lane = port->attr.nr_lane;
	struct rnp_hw *hw = port->hw;
	u32 flt_reg, vlan_reg;

	flt_reg = RNP_MAC_REG_RD(hw, lane, RNP_MAC_PKT_FLT_CTRL);
	vlan_reg = RNP_MAC_REG_RD(hw, lane, RNP_MAC_VLAN_TAG);
	if (en) {
		flt_reg |= RNP_MAC_VTFE;
		vlan_reg |= (RNP_MAC_VLAN_VTHM | RNP_MAC_VLAN_ETV |
			     RNP_MAC_VLAN_HASH_EN);
	} else {
		flt_reg &= ~RNP_MAC_VTFE;
		vlan_reg &= ~(RNP_MAC_VLAN_VTHM | RNP_MAC_VLAN_ETV |
			      RNP_MAC_VLAN_HASH_EN);
	}
	RNP_MAC_REG_WR(hw, lane, RNP_MAC_PKT_FLT_CTRL, flt_reg);
	RNP_MAC_REG_WR(hw, lane, RNP_MAC_VLAN_TAG, vlan_reg);

	return 0;
}

static int
rnp_update_vlan_filter_pf(struct rnp_eth_port *port,
			  u16 vlan, bool add)
{
	struct rnp_vlan_filter *vfta_tb = &port->vfta;
	struct rnp_hw *hw = port->hw;
	u32 vid_idx;
	u32 vid_bit;
	u32 vfta;

	vid_idx = (u32)((vlan >> 5) & 0x7F);
	vid_bit = (u32)(1 << (vlan & 0x1F));
	vfta = RNP_E_REG_RD(hw, RNP_VFTA_HASH_TABLE(vid_idx));
	if (add)
		vfta |= vid_bit;
	else
		vfta &= ~vid_bit;
	RNP_E_REG_WR(hw, RNP_VFTA_HASH_TABLE(vid_idx), vfta);
	/* update local VFTA copy */
	vfta_tb->vfta_entries[vid_idx] = vfta;

	return 0;
}

static void
rnp_update_vlan_hash_indep(struct rnp_eth_port *port)
{
	struct rnp_hw *hw = port->hw;
	u16 lane = port->attr.nr_lane;
	u64 vid_idx, vid_bit;
	u16 shift_bit = 0;
	u16 hash = 0;
	u16 vid_le;
	u32 crc;
	u16 vid;

	/* Generate VLAN Hash Table */
	for (vid = 0; vid < VLAN_N_VID; vid++) {
		vid_idx = RNP_VLAN_BITMAP_IDX(vid);
		vid_bit = port->vfta.vlans_bitmap[vid_idx];
		shift_bit = vid - (BITS_TO_LONGS(VLAN_N_VID) * vid_idx);
		shift_bit &= 0x3f;
		vid_bit = (u64)(vid_bit >> shift_bit);
		/* If Vid isn't Set, Calc Next Vid Hash Value */
		if (!(vid_bit & 1))
			continue;
		vid_le = cpu_to_le16(vid);
		crc = bitrev32(~rnp_vid_crc32_calc(~0, vid_le));
		crc >>= RNP_MAC_VLAN_HASH_SHIFT;
		hash |= (1 << crc);
	}
	/* Update vlan hash table */
	RNP_MAC_REG_WR(hw, lane, RNP_MAC_VLAN_HASH, hash);
}

static int
rnp_update_vlan_filter_indep(struct rnp_eth_port *port,
			     u16 vid,
			     bool add)
{
	u64 vid_bit, vid_idx;

	vid_bit = RNP_VLAN_BITMAP_BIT(vid);
	vid_idx = RNP_VLAN_BITMAP_IDX(vid);
	if (add)
		port->vfta.vlans_bitmap[vid_idx] |= vid_bit;
	else
		port->vfta.vlans_bitmap[vid_idx] &= ~vid_bit;

	rnp_update_vlan_hash_indep(port);

	return 0;
}

const struct rnp_mac_ops rnp_mac_ops_pf = {
	.get_macaddr = rnp_mbx_fw_get_macaddr,
	.update_mpfm = rnp_update_mpfm_pf,
	.set_rafb = rnp_set_mac_addr_pf,
	.clear_rafb = rnp_clear_mac_pf,
	.vlan_f_en = rnp_en_vlan_filter_pf,
	.update_vlan = rnp_update_vlan_filter_pf,
};

const struct rnp_mac_ops rnp_mac_ops_indep = {
	.get_macaddr = rnp_mbx_fw_get_macaddr,
	.update_mpfm = rnp_update_mpfm_indep,
	.set_rafb = rnp_set_mac_addr_indep,
	.clear_rafb = rnp_clear_mac_indep,
	.vlan_f_en = rnp_en_vlan_filter_indep,
	.update_vlan = rnp_update_vlan_filter_indep,
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

int rnp_set_macaddr(struct rnp_eth_port *port, u8 *mac, u32 index)
{
	const struct rnp_mac_ops *mac_ops =
		RNP_DEV_PP_TO_MAC_OPS(port->eth_dev);

	return rnp_call_hwif_impl(port, mac_ops->set_rafb, mac, index);
}

int rnp_clear_macaddr(struct rnp_eth_port *port, u32 index)
{
	const struct rnp_mac_ops *mac_ops =
		RNP_DEV_PP_TO_MAC_OPS(port->eth_dev);

	return rnp_call_hwif_impl(port, mac_ops->clear_rafb, index);
}

int rnp_rx_vlan_filter_en(struct rnp_eth_port *port, bool en)
{
	const struct rnp_mac_ops *mac_ops =
		RNP_DEV_PP_TO_MAC_OPS(port->eth_dev);

	return rnp_call_hwif_impl(port, mac_ops->vlan_f_en, en);
}

int rnp_update_vlan_filter(struct rnp_eth_port *port, u16 vid, bool en)
{
	const struct rnp_mac_ops *mac_ops =
		RNP_DEV_PP_TO_MAC_OPS(port->eth_dev);

	return rnp_call_hwif_impl(port, mac_ops->update_vlan, vid, en);
}

void rnp_mac_ops_init(struct rnp_hw *hw)
{
	struct rnp_proc_priv *proc_priv = RNP_DEV_TO_PROC_PRIV(hw->back->eth_dev);

	if (rnp_pf_is_multiple_ports(hw->device_id))
		proc_priv->mac_ops = &rnp_mac_ops_indep;
	else
		proc_priv->mac_ops = &rnp_mac_ops_pf;
}
