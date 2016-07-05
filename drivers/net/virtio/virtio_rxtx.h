/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
	uint8_t port_id;     /**< Device port identifier. */

	/* Statistics */
	struct virtnet_stats stats;

	const struct rte_memzone *mz; /**< mem zone to populate RX ring. */
};

struct virtnet_tx {
	struct virtqueue *vq;
	/**< memzone to populate hdr. */
	const struct rte_memzone *virtio_net_hdr_mz;
	phys_addr_t virtio_net_hdr_mem;  /**< hdr for each xmit packet */

	uint16_t    queue_id;            /**< DPDK queue index. */
	uint8_t     port_id;             /**< Device port identifier. */

	/* Statistics */
	struct virtnet_stats stats;

	const struct rte_memzone *mz;    /**< mem zone to populate TX ring. */
};

struct virtnet_ctl {
	struct virtqueue *vq;
	/**< memzone to populate hdr. */
	const struct rte_memzone *virtio_net_hdr_mz;
	phys_addr_t virtio_net_hdr_mem; /**< hdr for each xmit packet */
	uint8_t port_id;                /**< Device port identifier. */
	const struct rte_memzone *mz;   /**< mem zone to populate RX ring. */
};

int virtio_rxq_vec_setup(struct virtnet_rx *rxvq);

int virtqueue_enqueue_recv_refill_simple(struct virtqueue *vq,
	struct rte_mbuf *m);

#endif /* _VIRTIO_RXTX_H_ */
