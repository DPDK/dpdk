/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Realtek Corporation. All rights reserved
 */

#ifndef R8169_ETHDEV_H
#define R8169_ETHDEV_H

#include <stdint.h>

#include <rte_ethdev.h>
#include <rte_ethdev_core.h>

#include "r8169_compat.h"

struct rtl_hw;

struct rtl_hw_ops {
	void (*hw_init_rxcfg)(struct rtl_hw *hw);
	void (*hw_ephy_config)(struct rtl_hw *hw);
	void (*hw_phy_config)(struct rtl_hw *hw);
	void (*hw_mac_mcu_config)(struct rtl_hw *hw);
	void (*hw_phy_mcu_config)(struct rtl_hw *hw);
};

struct rtl_hw {
	struct rtl_hw_ops hw_ops;
	u8  *mmio_addr;
	u32 mcfg;
	u32 mtu;
	u8  HwSuppIntMitiVer;
	u16 cur_page;

	u8  RequirePhyMdiSwapPatch;
	u8  NotWrMcuPatchCode;
	u8  HwSuppMacMcuVer;
	u16 MacMcuPageSize;

	u8  NotWrRamCodeToMicroP;
	u8  HwHasWrRamCodeToMicroP;

	/* Enable Tx No Close */
	u8 EnableTxNoClose;

	/* Dash */
	u8 HwSuppDashVer;
	u8 DASH;
	u8 HwSuppOcpChannelVer;
	u8 AllowAccessDashOcp;
};

struct rtl_sw_stats {
	u64 tx_packets;
	u64 tx_bytes;
	u64 tx_errors;
	u64 rx_packets;
	u64 rx_bytes;
	u64 rx_errors;
};

struct rtl_adapter {
	struct rtl_hw       hw;
	struct rtl_sw_stats sw_stats;
};

#define RTL_DEV_PRIVATE(eth_dev) \
	((struct rtl_adapter *)((eth_dev)->data->dev_private))

int rtl_rx_init(struct rte_eth_dev *dev);
int rtl_tx_init(struct rte_eth_dev *dev);

uint16_t rtl_xmit_pkts(void *txq, struct rte_mbuf **tx_pkts, uint16_t nb_pkts);
uint16_t rtl_recv_pkts(void *rxq, struct rte_mbuf **rx_pkts, uint16_t nb_pkts);

#endif /* R8169_ETHDEV_H */
