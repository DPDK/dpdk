/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#ifndef _HINIC3_ETHDEV_H_
#define _HINIC3_ETHDEV_H_

#include <rte_ethdev.h>
#include <rte_ethdev_core.h>

#define HINIC3_PMD_DRV_VERSION "B106"

#define PCI_DEV_TO_INTR_HANDLE(pci_dev) ((pci_dev)->intr_handle)

#define HINIC3_PKT_RX_L4_CKSUM_BAD     RTE_MBUF_F_RX_L4_CKSUM_BAD
#define HINIC3_PKT_RX_IP_CKSUM_BAD     RTE_MBUF_F_RX_IP_CKSUM_BAD
#define HINIC3_PKT_RX_IP_CKSUM_UNKNOWN RTE_MBUF_F_RX_IP_CKSUM_UNKNOWN
#define HINIC3_PKT_RX_L4_CKSUM_GOOD    RTE_MBUF_F_RX_L4_CKSUM_GOOD
#define HINIC3_PKT_RX_IP_CKSUM_GOOD    RTE_MBUF_F_RX_IP_CKSUM_GOOD
#define HINIC3_PKT_TX_TCP_SEG	       RTE_MBUF_F_TX_TCP_SEG
#define HINIC3_PKT_TX_UDP_CKSUM	       RTE_MBUF_F_TX_UDP_CKSUM
#define HINIC3_PKT_TX_TCP_CKSUM	       RTE_MBUF_F_TX_TCP_CKSUM
#define HINIC3_PKT_TX_IP_CKSUM	       RTE_MBUF_F_TX_IP_CKSUM
#define HINIC3_PKT_TX_VLAN_PKT	       RTE_MBUF_F_TX_VLAN
#define HINIC3_PKT_TX_L4_MASK	       RTE_MBUF_F_TX_L4_MASK
#define HINIC3_PKT_TX_SCTP_CKSUM       RTE_MBUF_F_TX_SCTP_CKSUM
#define HINIC3_PKT_TX_IPV6	       RTE_MBUF_F_TX_IPV6
#define HINIC3_PKT_TX_IPV4	       RTE_MBUF_F_TX_IPV4
#define HINIC3_PKT_RX_VLAN	       RTE_MBUF_F_RX_VLAN
#define HINIC3_PKT_RX_VLAN_STRIPPED    RTE_MBUF_F_RX_VLAN_STRIPPED
#define HINIC3_PKT_RX_RSS_HASH	       RTE_MBUF_F_RX_RSS_HASH
#define HINIC3_PKT_TX_TUNNEL_MASK      RTE_MBUF_F_TX_TUNNEL_MASK
#define HINIC3_PKT_TX_TUNNEL_VXLAN     RTE_MBUF_F_TX_TUNNEL_VXLAN
#define HINIC3_PKT_TX_OUTER_IP_CKSUM   RTE_MBUF_F_TX_OUTER_IP_CKSUM
#define HINIC3_PKT_TX_OUTER_IPV6       RTE_MBUF_F_TX_OUTER_IPV6
#define HINIC3_PKT_RX_LRO	       RTE_MBUF_F_RX_LRO
#define HINIC3_PKT_TX_L4_NO_CKSUM      RTE_MBUF_F_TX_L4_NO_CKSUM

#define HINCI3_CPY_MEMPOOL_NAME "cpy_mempool"
/* Mbuf pool for copy invalid mbuf segs. */
#define HINIC3_COPY_MEMPOOL_DEPTH 1024
#define HINIC3_COPY_MEMPOOL_CACHE 128
#define HINIC3_COPY_MBUF_SIZE	  4096

#define HINIC3_DEV_NAME_LEN 32
#define DEV_STOP_DELAY_MS   100
#define DEV_START_DELAY_MS  100

#define HINIC3_UINT32_BIT_SIZE (CHAR_BIT * sizeof(uint32_t))
#define HINIC3_VFTA_SIZE       (4096 / HINIC3_UINT32_BIT_SIZE)
#define HINIC3_MAX_QUEUE_NUM   64

#define HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev) \
	((struct hinic3_nic_dev *)(dev)->data->dev_private)

enum hinic3_dev_status {
	HINIC3_DEV_INIT,
	HINIC3_DEV_CLOSE,
	HINIC3_DEV_START,
	HINIC3_DEV_INTR_EN
};

enum hinic3_tx_cvlan_type {
	HINIC3_TX_TPID0,
};

enum nic_feature_cap {
	NIC_F_CSUM = BIT(0),
	NIC_F_SCTP_CRC = BIT(1),
	NIC_F_TSO = BIT(2),
	NIC_F_LRO = BIT(3),
	NIC_F_UFO = BIT(4),
	NIC_F_RSS = BIT(5),
	NIC_F_RX_VLAN_FILTER = BIT(6),
	NIC_F_RX_VLAN_STRIP = BIT(7),
	NIC_F_TX_VLAN_INSERT = BIT(8),
	NIC_F_VXLAN_OFFLOAD = BIT(9),
	NIC_F_IPSEC_OFFLOAD = BIT(10),
	NIC_F_FDIR = BIT(11),
	NIC_F_PROMISC = BIT(12),
	NIC_F_ALLMULTI = BIT(13),
};

#define DEFAULT_DRV_FEATURE 0x3FFF

struct hinic3_nic_dev {
	struct hinic3_hwdev *hwdev; /**< Hardware device. */
	struct hinic3_txq **txqs;
	struct hinic3_rxq **rxqs;
	struct rte_mempool *cpy_mpool;

	u16 num_sqs;
	u16 num_rqs;
	u16 max_sqs;
	u16 max_rqs;

	u16 rx_buff_len;
	u16 mtu_size;

	u16 rss_state;
	u8 num_rss; /**< Number of RSS queues. */
	u8 rsvd0;   /**< Reserved field 0. */

	u32 rx_mode;
	u8 rx_queue_list[HINIC3_MAX_QUEUE_NUM];
	rte_spinlock_t queue_list_lock;

	pthread_mutex_t rx_mode_mutex;

	u32 default_cos;
	u32 rx_csum_en;

	u8 rss_key[HINIC3_RSS_KEY_SIZE];

	RTE_ATOMIC(u64) dev_status;

	struct rte_ether_addr default_addr;
	struct rte_ether_addr *mc_list;

	char dev_name[HINIC3_DEV_NAME_LEN];
	u64 feature_cap;
	u32 vfta[HINIC3_VFTA_SIZE]; /**< VLAN bitmap. */
};

/**
 * Enable interrupt for the specified RX queue.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @param[in] queue_id
 * The ID of the receive queue for which the interrupt is being enabled.
 * @return
 * 0 on success, a negative error code on failure.
 */
int hinic3_dev_rx_queue_intr_enable(struct rte_eth_dev *dev, uint16_t queue_id);

/**
 * Disable interrupt for the specified RX queue.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @param[in] queue_id
 * The ID of the receive queue for which the interrupt is being disabled.
 * @return
 * 0 on success, a negative error code on failure.
 */
int hinic3_dev_rx_queue_intr_disable(struct rte_eth_dev *dev,
				     uint16_t queue_id);

#endif /* _HINIC3_ETHDEV_H_ */
