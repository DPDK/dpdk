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

enum rtl_rss_register_content {
	/* RSS */
	RSS_CTRL_TCP_IPV4_SUPP     = (1 << 0),
	RSS_CTRL_IPV4_SUPP         = (1 << 1),
	RSS_CTRL_TCP_IPV6_SUPP     = (1 << 2),
	RSS_CTRL_IPV6_SUPP         = (1 << 3),
	RSS_CTRL_IPV6_EXT_SUPP     = (1 << 4),
	RSS_CTRL_TCP_IPV6_EXT_SUPP = (1 << 5),
	RSS_HALF_SUPP              = (1 << 7),
	RSS_CTRL_UDP_IPV4_SUPP     = (1 << 11),
	RSS_CTRL_UDP_IPV6_SUPP     = (1 << 12),
	RSS_CTRL_UDP_IPV6_EXT_SUPP = (1 << 13),
	RSS_QUAD_CPU_EN            = (1 << 16),
	RSS_HQ_Q_SUP_R             = (1 << 31),
};

#define RTL_RSS_OFFLOAD_ALL ( \
	RTE_ETH_RSS_IPV4 | \
	RTE_ETH_RSS_NONFRAG_IPV4_TCP | \
	RTE_ETH_RSS_NONFRAG_IPV4_UDP | \
	RTE_ETH_RSS_IPV6 | \
	RTE_ETH_RSS_NONFRAG_IPV6_TCP | \
	RTE_ETH_RSS_NONFRAG_IPV6_UDP | \
	RTE_ETH_RSS_IPV6_EX | \
	RTE_ETH_RSS_IPV6_TCP_EX | \
	RTE_ETH_RSS_IPV6_UDP_EX)

#define RTL_RSS_CTRL_OFFLOAD_ALL ( \
	RSS_CTRL_TCP_IPV4_SUPP | \
	RSS_CTRL_IPV4_SUPP | \
	RSS_CTRL_TCP_IPV6_SUPP | \
	RSS_CTRL_IPV6_SUPP | \
	RSS_CTRL_IPV6_EXT_SUPP | \
	RSS_CTRL_TCP_IPV6_EXT_SUPP | \
	RSS_CTRL_UDP_IPV4_SUPP | \
	RSS_CTRL_UDP_IPV6_SUPP | \
	RSS_CTRL_UDP_IPV6_EXT_SUPP)

#define RTL_RSS_KEY_SIZE     40  /* size of RSS Hash Key in bytes */
#define RTL_MAX_INDIRECTION_TABLE_ENTRIES RTE_ETH_RSS_RETA_SIZE_128
#define RSS_MASK_BITS_OFFSET 8
#define RSS_CPU_NUM_OFFSET   16

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
	u32 MaxTxDescPtrMask;

	/* Dash */
	u8 HwSuppDashVer;
	u8 DASH;
	u8 HwSuppOcpChannelVer;
	u8 AllowAccessDashOcp;
	u8 HwPkgDet;
	u8 HwSuppSerDesPhyVer;

	/* Fiber */
	u32 HwFiberModeVer;

	/* Multi queue*/
	u8 EnableRss;
	u8 HwSuppRxDescType;
	u16 RxDescLength;
	u8 rss_key[RTL_RSS_KEY_SIZE];
	u8 rss_indir_tbl[RTL_MAX_INDIRECTION_TABLE_ENTRIES];
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

/* Struct RxDesc in kernel r8169 */
struct rtl_rx_desc {
	u32 opts1;
	u32 opts2;
	u64 addr;
};

struct rtl_rx_descv3 {
	union {
		struct {
			u32 rsv1;
			u32 rsv2;
		} RxDescDDWord1;
	};

	union {
		struct {
			u32 RSSResult;
			u16 HeaderBufferLen;
			u16 HeaderInfo;
		} RxDescNormalDDWord2;

		struct {
			u32 rsv5;
			u32 rsv6;
		} RxDescDDWord2;
	};

	union {
		u64   addr;

		struct {
			u32 TimeStampLow;
			u32 TimeStampHigh;
		} RxDescTimeStamp;

		struct {
			u32 rsv8;
			u32 rsv9;
		} RxDescDDWord3;
	};

	union {
		struct {
			u32 opts2;
			u32 opts1;
		} RxDescNormalDDWord4;

		struct {
			u16 TimeStampHHigh;
			u16 rsv11;
			u32 opts1;
		} RxDescPTPDDWord4;
	};
};

struct rtl_rx_descv4 {
	union {
		u64   addr;

		struct {
			u32 RSSInfo;
			u32 RSSResult;
		} RxDescNormalDDWord1;
	};

	struct {
		u32 opts2;
		u32 opts1;
	} RxDescNormalDDWord2;
};

enum rx_desc_ring_type {
	RX_DESC_RING_TYPE_UNKNOWN = 0,
	RX_DESC_RING_TYPE_1,
	RX_DESC_RING_TYPE_2,
	RX_DESC_RING_TYPE_3,
	RX_DESC_RING_TYPE_4,
	RX_DESC_RING_TYPE_MAX
};

enum rx_desc_len {
	RX_DESC_LEN_TYPE_1 = (sizeof(struct rtl_rx_desc)),
	RX_DESC_LEN_TYPE_3 = (sizeof(struct rtl_rx_descv3)),
	RX_DESC_LEN_TYPE_4 = (sizeof(struct rtl_rx_descv4))
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

uint64_t rtl_get_rx_port_offloads(struct rtl_hw *hw);
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

static inline struct rtl_rx_desc*
rtl_get_rxdesc(struct rtl_hw *hw, struct rtl_rx_desc *base, u32 const number)
{
	return (struct rtl_rx_desc *)((u8 *)base + hw->RxDescLength * number);
}

#endif /* R8169_ETHDEV_H */
