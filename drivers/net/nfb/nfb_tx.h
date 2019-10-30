/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Cesnet
 * Copyright(c) 2019 Netcope Technologies, a.s. <info@netcope.com>
 * All rights reserved.
 */

#ifndef _NFB_TX_H_
#define _NFB_TX_H_

#include <nfb/nfb.h>
#include <nfb/ndp.h>

#include <rte_ethdev_driver.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>

struct ndp_tx_queue {
	struct nfb_device *nfb;     /* nfb dev structure */
	struct ndp_queue *queue;    /* tx queue */
	uint16_t          tx_queue_id;       /* index */
	volatile uint64_t tx_pkts;  /* packets transmitted */
	volatile uint64_t tx_bytes; /* bytes transmitted */
	volatile uint64_t err_pkts; /* erroneous packets */
};

/**
 * DPDK callback to setup a TX queue for use.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param idx
 *   RX queue index.
 * @param desc
 *   Number of descriptors to configure in queue.
 * @param socket
 *   NUMA socket on which memory must be allocated.
 * @param[in] conf
 *   Thresholds parameters.
 * @param mp
 *   Memory pool for buffer allocations.
 *
 * @return
 *   0 on success, a negative errno value otherwise.
 */
int
nfb_eth_tx_queue_setup(struct rte_eth_dev *dev,
	uint16_t tx_queue_id,
	uint16_t nb_tx_desc __rte_unused,
	unsigned int socket_id,
	const struct rte_eth_txconf *tx_conf __rte_unused);

/**
 * Initialize ndp_tx_queue structure
 *
 * @param nfb
 *   Pointer to nfb device structure.
 * @param tx_queue_id
 *   TX queue index.
 * @param[out] txq
 *   Pointer to ndp_tx_queue output structure
 *
 * @return
 *   0 on success, a negative errno value otherwise.
 */
int
nfb_eth_tx_queue_init(struct nfb_device *nfb,
	uint16_t tx_queue_id,
	struct ndp_tx_queue *txq);

/**
 * DPDK callback to release a RX queue.
 *
 * @param dpdk_rxq
 *   Generic RX queue pointer.
 */
void
nfb_eth_tx_queue_release(void *q);

/**
 * Start traffic on Tx queue.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param txq_id
 *   TX queue index.
 *
 * @return
 *   0 on success, a negative errno value otherwise.
 */
int
nfb_eth_tx_queue_start(struct rte_eth_dev *dev, uint16_t txq_id);

/**
 * Stop traffic on Tx queue.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param txq_id
 *   TX queue index.
 *
 * @return
 *   0 on success, a negative errno value otherwise.
 */
int
nfb_eth_tx_queue_stop(struct rte_eth_dev *dev, uint16_t txq_id);

/**
 * DPDK callback for TX.
 *
 * @param dpdk_txq
 *   Generic pointer to TX queue structure.
 * @param bufs
 *   Packets to transmit.
 * @param nb_pkts
 *   Number of packets in array.
 *
 * @return
 *   Number of packets successfully transmitted (<= nb_pkts).
 */
static __rte_always_inline uint16_t
nfb_eth_ndp_tx(void *queue,
	struct rte_mbuf **bufs,
	uint16_t nb_pkts)
{
	int i;
	struct rte_mbuf *mbuf;
	struct ndp_tx_queue *ndp = queue;
	uint16_t num_tx = 0;
	uint64_t num_bytes = 0;

	void *dst;
	uint32_t pkt_len;
	uint8_t mbuf_segs;

	struct ndp_packet packets[nb_pkts];

	if (unlikely(ndp->queue == NULL || nb_pkts == 0)) {
		RTE_LOG(ERR, PMD, "TX invalid arguments!\n");
		return 0;
	}

	for (i = 0; i < nb_pkts; i++) {
		packets[i].data_length = bufs[i]->pkt_len;
		packets[i].header_length = 0;
	}

	num_tx = ndp_tx_burst_get(ndp->queue, packets, nb_pkts);

	if (unlikely(num_tx != nb_pkts))
		return 0;

	for (i = 0; i < nb_pkts; ++i) {
		mbuf = bufs[i];

		pkt_len = mbuf->pkt_len;
		mbuf_segs = mbuf->nb_segs;

		num_bytes += pkt_len;
		if (mbuf_segs == 1) {
			/*
			 * non-scattered packet,
			 * transmit from one mbuf
			 */
			rte_memcpy(packets[i].data,
				rte_pktmbuf_mtod(mbuf, const void *),
				pkt_len);
		} else {
			/* scattered packet, transmit from more mbufs */
			struct rte_mbuf *m = mbuf;
			while (m) {
				dst = packets[i].data;

				rte_memcpy(dst,
					rte_pktmbuf_mtod(m,
					const void *),
					m->data_len);
				dst = ((uint8_t *)(dst)) +
					m->data_len;
				m = m->next;
			}
		}

		rte_pktmbuf_free(mbuf);
	}

	ndp_tx_burst_flush(ndp->queue);

	ndp->tx_pkts += num_tx;
	ndp->err_pkts += nb_pkts - num_tx;
	ndp->tx_bytes += num_bytes;
	return num_tx;
}

#endif /* _NFB_TX_H_ */
