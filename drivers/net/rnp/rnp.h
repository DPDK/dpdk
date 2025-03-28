/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Mucse IC Design Ltd.
 */

#ifndef __RNP_H__
#define __RNP_H__

#include <ethdev_driver.h>
#include <rte_interrupts.h>

#include "base/rnp_hw.h"

#define PCI_VENDOR_ID_MUCSE	(0x8848)
#define RNP_DEV_ID_N10G_X2	(0x1000)
#define RNP_DEV_ID_N10G_X4	(0x1020)
#define RNP_DEV_ID_N10G_X8	(0x1060)
#define RNP_MAX_VF_NUM		(64)
#define RNP_MISC_VEC_ID		RTE_INTR_VEC_ZERO_OFFSET
/* maximum frame size supported */
#define RNP_ETH_OVERHEAD \
	(RTE_ETHER_HDR_LEN + RTE_VLAN_HLEN * 2)
#define RNP_MAC_MAXFRM_SIZE	(9590)

#define RNP_RX_MAX_MTU_SEG	(64)
#define RNP_TX_MAX_MTU_SEG	(32)
#define RNP_RX_MAX_SEG		(150)
#define RNP_TX_MAX_SEG		(UINT8_MAX)
#define RNP_MIN_DMA_BUF_SIZE	(1024)
/* rss support info */
#define RNP_RSS_INDIR_SIZE	(128)
#define RNP_MAX_HASH_KEY_SIZE	(10)
#define RNP_SUPPORT_RSS_OFFLOAD_ALL ( \
		RTE_ETH_RSS_IPV4 | \
		RTE_ETH_RSS_FRAG_IPV4 | \
		RTE_ETH_RSS_NONFRAG_IPV4_OTHER | \
		RTE_ETH_RSS_NONFRAG_IPV4_TCP | \
		RTE_ETH_RSS_NONFRAG_IPV4_UDP | \
		RTE_ETH_RSS_NONFRAG_IPV4_SCTP |\
		RTE_ETH_RSS_IPV6 | \
		RTE_ETH_RSS_FRAG_IPV6 | \
		RTE_ETH_RSS_NONFRAG_IPV6_OTHER | \
		RTE_ETH_RSS_IPV6_EX | \
		RTE_ETH_RSS_IPV6_TCP_EX | \
		RTE_ETH_RSS_NONFRAG_IPV6_TCP | \
		RTE_ETH_RSS_NONFRAG_IPV6_UDP | \
		RTE_ETH_RSS_IPV6_UDP_EX | \
		RTE_ETH_RSS_NONFRAG_IPV6_SCTP)
/* Ring info special */
#define RNP_MAX_BD_COUNT	(4096)
#define RNP_MIN_BD_COUNT	(128)
#define RNP_BD_ALIGN		(2)
/* Hardware resource info */
#define RNP_MAX_MSIX_NUM		(64)
#define RNP_MAX_RX_QUEUE_NUM		(128)
#define RNP_MAX_TX_QUEUE_NUM		(128)
/* l2 filter hardware resource info */
#define RNP_MAX_MAC_ADDRS		(128)	/* max unicast extract mac num */
#define RNP_MAX_HASH_UC_MAC_SIZE	(4096)	/* max unicast hash mac num */
#define RNP_MAX_HASH_MC_MAC_SIZE	(4096)	/* max multicast hash mac num */
#define RNP_MAX_UC_HASH_TABLE		(128)	/* max unicast hash mac filter table */
#define RNP_MAC_MC_HASH_TABLE		(128)	/* max multicast hash mac filter table*/
/* hardware media type */
enum rnp_media_type {
	RNP_MEDIA_TYPE_UNKNOWN,
	RNP_MEDIA_TYPE_FIBER,
	RNP_MEDIA_TYPE_COPPER,
	RNP_MEDIA_TYPE_BACKPLANE,
	RNP_MEDIA_TYPE_NONE,
};

struct rnp_phy_meta {
	uint32_t speed_cap;
	uint32_t supported_link;
	uint16_t link_duplex;
	uint16_t link_autoneg;
	uint32_t phy_identifier;
	uint16_t phy_type;
	uint8_t media_type;
	bool is_sgmii;
	bool is_backplane;
	bool fec;
};

struct rnp_port_attr {
	uint16_t max_mac_addrs;	  /* max support mac address */
	uint16_t max_uc_mac_hash; /* max hash unicast mac size */
	uint16_t max_mc_mac_hash; /* max hash multicast mac size */
	uint16_t uc_hash_tb_size; /* max unicast hash table block num */
	uint16_t mc_hash_tb_size; /* max multicast hash table block num */
	uint16_t max_rx_queues;   /* belong to this port rxq resource */
	uint16_t max_tx_queues;   /* belong to this port txq resource */

	struct rnp_phy_meta phy_meta;

	bool link_ready;
	bool pre_link;
	bool duplex;
	uint32_t speed;

	uint16_t port_id;	/* platform manage port sequence id */
	uint8_t port_offset;	/* port queue offset */
	uint8_t sw_id;		/* software port init sequence id */
	uint16_t nr_lane;	/* phy lane of This PF:0~3 */
};

struct rnp_proc_priv {
	const struct rnp_mac_ops *mac_ops;
	const struct rnp_mbx_ops *mbx_ops;
};

struct rnp_eth_port {
	struct rnp_proc_priv *proc_priv;
	struct rte_ether_addr mac_addr;
	struct rte_eth_dev *eth_dev;
	struct rnp_port_attr attr;
	struct rnp_tx_queue *tx_queues[RNP_MAX_RX_QUEUE_NUM];
	struct rnp_hw *hw;

	struct rte_eth_rss_conf rss_conf;
	uint16_t last_rx_num;
	bool rxq_num_changed;
	bool reta_has_cfg;
	bool hw_rss_en;
	uint32_t indirtbl[RNP_RSS_INDIR_SIZE];

	uint16_t cur_mtu;
	bool jumbo_en;

	rte_spinlock_t rx_mac_lock;
	bool port_stopped;
};

enum rnp_pf_op {
	RNP_PF_OP_DONE,
	RNP_PF_OP_CLOSING = 1,
	RNP_PF_OP_PROCESS,
};

struct rnp_eth_adapter {
	struct rnp_hw hw;
	struct rte_pci_device *pdev;
	struct rte_eth_dev *eth_dev; /* alloc eth_dev by platform */

	struct rte_mempool *reset_pool;
	struct rnp_eth_port *ports[RNP_MAX_PORT_OF_PF];
	RTE_ATOMIC(uint16_t) pf_op;
	uint16_t closed_ports;
	uint16_t inited_ports;
	bool intr_registered;
};

#define RNP_DEV_TO_PORT(eth_dev) \
	((struct rnp_eth_port *)(eth_dev)->data->dev_private)
#define RNP_DEV_TO_ADAPTER(eth_dev) \
	((struct rnp_eth_adapter *)(RNP_DEV_TO_PORT(eth_dev))->hw->back)
#define RNP_DEV_TO_PROC_PRIV(eth_dev) \
	((struct rnp_proc_priv *)(eth_dev)->process_private)
#define RNP_DEV_PP_TO_MBX_OPS(priv) \
	(((RNP_DEV_TO_PROC_PRIV(priv))->mbx_ops))
#define RNP_DEV_PP_TO_MAC_OPS(priv) \
	(((RNP_DEV_TO_PROC_PRIV(priv))->mac_ops))

static inline int
rnp_pf_own_ports(uint32_t id)
{
	return (id == 0) ? 1 : (id == 1) ? 2 : 4;
}

static inline int
rnp_pf_is_multiple_ports(uint32_t device_id)
{
	uint32_t verbit = (device_id >> 5) & 0x3;

	return rnp_pf_own_ports(verbit) == 1 ? 0 : 1;
}

#endif /* __RNP_H__ */
