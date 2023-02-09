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
	struct rte_mbuf **sw_ring;  /**< RX software ring. */
	struct rte_mbuf *fake_mbuf; /**< dummy mbuf, for wraparound when processing RX ring. */
	uint64_t mbuf_initializer; /**< value to init mbufs. */
	struct rte_mempool *mpool; /**< mempool for mbuf allocation */

	/* Statistics */
	struct virtnet_stats stats;
};

struct virtnet_tx {
	const struct rte_memzone *hdr_mz; /**< memzone to populate hdr. */
	rte_iova_t hdr_mem;               /**< hdr for each xmit packet */

	struct virtnet_stats stats;       /* Statistics */
};

int virtio_rxq_vec_setup(struct virtnet_rx *rxvq);
void virtio_update_packet_stats(struct virtnet_stats *stats,
				struct rte_mbuf *mbuf);

#endif /* _VIRTIO_RXTX_H_ */
