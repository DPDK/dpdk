/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Mucse IC Design Ltd.
 */

#include "rnp_osdep.h"
#include "rnp_hw.h"
#include "rnp_mac_regs.h"
#include "rnp_eth_regs.h"
#include "rnp_dma_regs.h"
#include "rnp_common.h"
#include "rnp_mbx_fw.h"
#include "rnp_mac.h"
#include "../rnp.h"

static void
rnp_hw_reset(struct rnp_hw *hw)
{
	PMD_INIT_FUNC_TRACE();

	RNP_E_REG_WR(hw, RNP_NIC_RESET, 0);
	/* hardware reset valid must be 0 -> 1 */
	wmb();
	RNP_E_REG_WR(hw, RNP_NIC_RESET, 1);
	RNP_PMD_DRV_LOG(INFO, "PF[%d] reset nic finish", hw->mbx.pf_num);
}

int rnp_init_hw(struct rnp_hw *hw)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(hw->back->eth_dev);
	u32 version = 0;
	int ret = -1;
	u32 idx = 0;
	u32 state;

	PMD_INIT_FUNC_TRACE();
	version = RNP_E_REG_RD(hw, RNP_DMA_VERSION);
	RNP_PMD_DRV_LOG(INFO, "nic hw version:0x%.2x", version);
	rnp_fw_init(hw);
	RNP_E_REG_WR(hw, RNP_DMA_HW_EN, FALSE);
	do {
		state = RNP_E_REG_RD(hw, RNP_DMA_HW_STATE);
	} while (state == 0);
	ret = rnp_mbx_fw_get_capability(port);
	if (ret) {
		RNP_PMD_ERR("mbx_get_capability error! errcode=%d", ret);
		return ret;
	}
	rnp_hw_reset(hw);
	rnp_mbx_fw_reset_phy(hw);
	/* rx packet protocol engine bypass */
	RNP_E_REG_WR(hw, RNP_E_ENG_BYPASS, FALSE);
	/* enable host filter */
	RNP_E_REG_WR(hw, RNP_E_FILTER_EN, TRUE);
	/* enable vxlan parse */
	RNP_E_REG_WR(hw, RNP_E_VXLAN_PARSE_EN, TRUE);
	/* enable flow direct engine */
	RNP_E_REG_WR(hw, RNP_E_REDIR_EN, TRUE);
	/* enable dma engine */
	RNP_E_REG_WR(hw, RNP_DMA_HW_EN, RNP_DMA_EN_ALL);
#define RNP_TARGET_TC_PORT		(2)
#define RNP_PORT_OFF_QUEUE_NUM		(2)
	if (hw->nic_mode == RNP_DUAL_10G && hw->max_port_num == 2)
		RNP_E_REG_WR(hw, RNP_TC_PORT_OFFSET(RNP_TARGET_TC_PORT),
				RNP_PORT_OFF_QUEUE_NUM);
	/* setup mac resiger ctrl base */
	for (idx = 0; idx < hw->max_port_num; idx++)
		hw->mac_base[idx] = (u8 *)hw->e_ctrl + RNP_MAC_BASE_OFFSET(idx);
	/* tx all hw queue must be started */
	for (idx = 0; idx < RNP_MAX_RX_QUEUE_NUM; idx++)
		RNP_E_REG_WR(hw, RNP_TXQ_START(idx), true);

	return 0;
}

int
rnp_setup_common_ops(struct rnp_hw *hw)
{
	rnp_mac_ops_init(hw);

	return 0;
}
