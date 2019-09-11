/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#ifndef _VIRTIO_RXTX_H_
#define _VIRTIO_RXTX_H_

#define RTE_PMD_VIRTIO_RX_MAX_BURST 64

struct virtnet_stats {
	uint64_t	packets;
	uint64_t	bytes;
	uint64_t	errors;
	uint64_t	multicast;
	uint64_t	broadcast;
	/* Size bins in array as RFC 2819, undersized [0], 64 [1], etc */
	uint64_t	size_bins[8];
};

struct virtnet_rx {
	struct virtqueue *vq;
	/* dummy mbuf, for wraparound when processing RX ring. */
	struct rte_mbuf fake_mbuf;
	uint64_t mbuf_initializer; /**< value to init mbufs. */
	struct rte_mempool *mpool; /**< mempool for mbuf allocation */

	uint16_t queue_id;   /**< DPDK queue index. */
	uint16_t port_id;     /**< Device port identifier. */

	/* Statistics */
	struct virtnet_stats stats;

	const struct rte_memzone *mz; /**< mem zone to populate RX ring. */
};

struct virtnet_tx {
	struct virtqueue *vq;
	/**< memzone to populate hdr. */
	const struct rte_memzone *virtio_net_hdr_mz;
	rte_iova_t virtio_net_hdr_mem;   /**< hdr for each xmit packet */

	uint16_t    queue_id;            /**< DPDK queue index. */
	uint16_t    port_id;             /**< Device port identifier. */

	/* Statistics */
	struct virtnet_stats stats;

	const struct rte_memzone *mz;    /**< mem zone to populate TX ring. */
};

struct virtnet_ctl {
	struct virtqueue *vq;
	/**< memzone to populate hdr. */
	const struct rte_memzone *virtio_net_hdr_mz;
	rte_iova_t virtio_net_hdr_mem;  /**< hdr for each xmit packet */
	uint16_t port_id;               /**< Device port identifier. */
	const struct rte_memzone *mz;   /**< mem zone to populate CTL ring. */
	rte_spinlock_t lock;              /**< spinlock for control queue. */
};

int virtio_rxq_vec_setup(struct virtnet_rx *rxvq);
void virtio_update_packet_stats(struct virtnet_stats *stats,
				struct rte_mbuf *mbuf);

#endif /* _VIRTIO_RXTX_H_ */
