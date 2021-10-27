/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#ifndef _VIRTIO_ETHDEV_H_
#define _VIRTIO_ETHDEV_H_

#include <stdint.h>

#include <ethdev_driver.h>

#include "virtio.h"

#define VIRTIO_MAX_RX_QUEUES 128U
#define VIRTIO_MAX_TX_QUEUES 128U
#define VIRTIO_MAX_MAC_ADDRS 64
#define VIRTIO_MIN_RX_BUFSIZE 64
#define VIRTIO_MAX_RX_PKTLEN  9728U

/* Features desired/implemented by this driver. */
#define VIRTIO_PMD_DEFAULT_GUEST_FEATURES	\
	(1u << VIRTIO_NET_F_MAC		  |	\
	 1u << VIRTIO_NET_F_STATUS	  |	\
	 1u << VIRTIO_NET_F_MQ		  |	\
	 1u << VIRTIO_NET_F_CTRL_MAC_ADDR |	\
	 1u << VIRTIO_NET_F_CTRL_VQ	  |	\
	 1u << VIRTIO_NET_F_CTRL_RX	  |	\
	 1u << VIRTIO_NET_F_CTRL_VLAN	  |	\
	 1u << VIRTIO_NET_F_MRG_RXBUF	  |	\
	 1u << VIRTIO_NET_F_MTU	| \
	 1ULL << VIRTIO_NET_F_GUEST_ANNOUNCE |	\
	 1u << VIRTIO_RING_F_INDIRECT_DESC |    \
	 1ULL << VIRTIO_F_VERSION_1       |	\
	 1ULL << VIRTIO_F_IN_ORDER        |	\
	 1ULL << VIRTIO_F_RING_PACKED	  |	\
	 1ULL << VIRTIO_F_IOMMU_PLATFORM  |	\
	 1ULL << VIRTIO_F_ORDER_PLATFORM  |	\
	 1ULL << VIRTIO_F_NOTIFICATION_DATA | \
	 1ULL << VIRTIO_NET_F_SPEED_DUPLEX)

#define VIRTIO_PMD_SUPPORTED_GUEST_FEATURES	\
	(VIRTIO_PMD_DEFAULT_GUEST_FEATURES |	\
	 1u << VIRTIO_NET_F_GUEST_CSUM	   |	\
	 1u << VIRTIO_NET_F_GUEST_TSO4     |	\
	 1u << VIRTIO_NET_F_GUEST_TSO6     |	\
	 1u << VIRTIO_NET_F_CSUM           |	\
	 1u << VIRTIO_NET_F_HOST_TSO4      |	\
	 1u << VIRTIO_NET_F_HOST_TSO6      |	\
	 1ULL << VIRTIO_NET_F_RSS)

extern const struct eth_dev_ops virtio_user_secondary_eth_dev_ops;

/*
 * CQ function prototype
 */
void virtio_dev_cq_start(struct rte_eth_dev *dev);

/*
 * RX/TX function prototypes
 */

int  virtio_dev_rx_queue_setup(struct rte_eth_dev *dev, uint16_t rx_queue_id,
		uint16_t nb_rx_desc, unsigned int socket_id,
		const struct rte_eth_rxconf *rx_conf,
		struct rte_mempool *mb_pool);

int virtio_dev_rx_queue_setup_finish(struct rte_eth_dev *dev,
				uint16_t rx_queue_id);

int  virtio_dev_tx_queue_setup(struct rte_eth_dev *dev, uint16_t tx_queue_id,
		uint16_t nb_tx_desc, unsigned int socket_id,
		const struct rte_eth_txconf *tx_conf);

int virtio_dev_tx_queue_setup_finish(struct rte_eth_dev *dev,
				uint16_t tx_queue_id);

uint16_t virtio_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts);
uint16_t virtio_recv_pkts_packed(void *rx_queue, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts);

uint16_t virtio_recv_mergeable_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts);

uint16_t virtio_recv_mergeable_pkts_packed(void *rx_queue,
		struct rte_mbuf **rx_pkts, uint16_t nb_pkts);

uint16_t virtio_recv_pkts_inorder(void *rx_queue,
		struct rte_mbuf **rx_pkts, uint16_t nb_pkts);

uint16_t virtio_xmit_pkts_prepare(void *tx_queue, struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts);

uint16_t virtio_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts);
uint16_t virtio_xmit_pkts_packed(void *tx_queue, struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts);

uint16_t virtio_xmit_pkts_inorder(void *tx_queue, struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts);

uint16_t virtio_recv_pkts_vec(void *rx_queue, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts);

uint16_t virtio_recv_pkts_packed_vec(void *rx_queue, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts);

uint16_t virtio_xmit_pkts_packed_vec(void *tx_queue, struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts);

int eth_virtio_dev_init(struct rte_eth_dev *eth_dev);

void virtio_interrupt_handler(void *param);

int virtio_dev_pause(struct rte_eth_dev *dev);
void virtio_dev_resume(struct rte_eth_dev *dev);
int virtio_dev_stop(struct rte_eth_dev *dev);
int virtio_dev_close(struct rte_eth_dev *dev);
int virtio_inject_pkts(struct rte_eth_dev *dev, struct rte_mbuf **tx_pkts,
		int nb_pkts);

bool virtio_rx_check_scatter(uint16_t max_rx_pkt_len, uint16_t rx_buf_size,
			bool rx_scatter_enabled, const char **error);

uint16_t virtio_rx_mem_pool_buf_size(struct rte_mempool *mp);

#endif /* _VIRTIO_ETHDEV_H_ */
