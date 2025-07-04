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
	void (*hw_config)(struct rtl_hw *hw);
	void (*hw_init_rxcfg)(struct rtl_hw *hw);
	void (*hw_ephy_config)(struct rtl_hw *hw);
	void (*hw_phy_config)(struct rtl_hw *hw);
	void (*hw_mac_mcu_config)(struct rtl_hw *hw);
	void (*hw_phy_mcu_config)(struct rtl_hw *hw);
};

/* Flow control settings */
enum rtl_fc_mode {
	rtl_fc_none = 0,
	rtl_fc_rx_pause,
	rtl_fc_tx_pause,
	rtl_fc_full,
	rtl_fc_default
};

struct rtl_hw {
	struct rtl_hw_ops hw_ops;
	u8  *mmio_addr;
	u8  *cmac_ioaddr; /* cmac memory map physical address */
	u8  chipset_name;
	u8  HwIcVerUnknown;
	u32 mcfg;
	u32 mtu;
	u8  HwSuppIntMitiVer;
	u16 cur_page;
	u8  mac_addr[RTE_ETHER_ADDR_LEN];
	u32 rx_buf_sz;

	struct rtl_counters *tally_vaddr;
	u64 tally_paddr;

	u8  RequirePhyMdiSwapPatch;
	u8  NotWrMcuPatchCode;
	u8  HwSuppMacMcuVer;
	u16 MacMcuPageSize;
	u64 hw_mcu_patch_code_ver;
	u64 bin_mcu_patch_code_ver;

	u8 NotWrRamCodeToMicroP;
	u8 HwHasWrRamCodeToMicroP;
	u8 HwSuppCheckPhyDisableModeVer;

	u16 sw_ram_code_ver;
	u16 hw_ram_code_ver;

	u8  autoneg;
	u8  duplex;
	u32 speed;
	u64 advertising;
	enum rtl_fc_mode fcpause;

	u32 HwSuppMaxPhyLinkSpeed;

	u8 HwSuppNowIsOobVer;

	u16 mcu_pme_setting;

	/* Enable Tx No Close */
	u8  HwSuppTxNoCloseVer;
	u8  EnableTxNoClose;
	u16 hw_clo_ptr_reg;
	u16 sw_tail_ptr_reg;
	u32 MaxTxDescPtrMask;
	u32 NextHwDesCloPtr0;
	u32 BeginHwDesCloPtr0;

	/* Dash */
	u8 HwSuppDashVer;
	u8 DASH;
	u8 HwSuppOcpChannelVer;
	u8 AllowAccessDashOcp;
	u8 HwPkgDet;
	u8 HwSuppSerDesPhyVer;

	/* Fiber */
	u32 HwFiberModeVer;
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

#define R8169_LINK_CHECK_TIMEOUT  50   /* 10s */
#define R8169_LINK_CHECK_INTERVAL 200  /* ms */

#define PCI_READ_CONFIG_BYTE(dev, val, where) \
	rte_pci_read_config(dev, val, 1, where)

#define PCI_READ_CONFIG_WORD(dev, val, where) \
	rte_pci_read_config(dev, val, 2, where)

#define PCI_READ_CONFIG_DWORD(dev, val, where) \
	rte_pci_read_config(dev, val, 4, where)

#define PCI_WRITE_CONFIG_BYTE(dev, val, where) \
	rte_pci_write_config(dev, val, 1, where)

#define PCI_WRITE_CONFIG_WORD(dev, val, where) \
	rte_pci_write_config(dev, val, 2, where)

#define PCI_WRITE_CONFIG_DWORD(dev, val, where) \
	rte_pci_write_config(dev, val, 4, where)

int rtl_rx_init(struct rte_eth_dev *dev);
int rtl_tx_init(struct rte_eth_dev *dev);

uint16_t rtl_xmit_pkts(void *txq, struct rte_mbuf **tx_pkts, uint16_t nb_pkts);
uint16_t rtl_recv_pkts(void *rxq, struct rte_mbuf **rx_pkts, uint16_t nb_pkts);
uint16_t rtl_recv_scattered_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
				 uint16_t nb_pkts);

void rtl_rx_queue_release(struct rte_eth_dev *dev, uint16_t rx_queue_id);
void rtl_tx_queue_release(struct rte_eth_dev *dev, uint16_t tx_queue_id);

void rtl_rxq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
		      struct rte_eth_rxq_info *qinfo);
void rtl_txq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
		      struct rte_eth_txq_info *qinfo);

uint64_t rtl_get_rx_port_offloads(void);
uint64_t rtl_get_tx_port_offloads(void);

int rtl_rx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
		       uint16_t nb_rx_desc, unsigned int socket_id,
		       const struct rte_eth_rxconf *rx_conf,
		       struct rte_mempool *mb_pool);
int rtl_tx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
		       uint16_t nb_tx_desc, unsigned int socket_id,
		       const struct rte_eth_txconf *tx_conf);

int rtl_tx_done_cleanup(void *tx_queue, uint32_t free_cnt);

int rtl_stop_queues(struct rte_eth_dev *dev);
void rtl_free_queues(struct rte_eth_dev *dev);

#endif /* R8169_ETHDEV_H */
