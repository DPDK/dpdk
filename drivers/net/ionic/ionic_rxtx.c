/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright(c) 2018-2019 Pensando Systems, Inc. All rights reserved.
 */

#include <sys/queue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <inttypes.h>

#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_mempool.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <ethdev_driver.h>
#include <rte_prefetch.h>
#include <rte_udp.h>
#include <rte_tcp.h>
#include <rte_sctp.h>
#include <rte_string_fns.h>
#include <rte_errno.h>
#include <rte_ip.h>
#include <rte_net.h>

#include "ionic_logs.h"
#include "ionic_mac_api.h"
#include "ionic_ethdev.h"
#include "ionic_lif.h"
#include "ionic_rxtx.h"

/*********************************************************************
 *
 *  TX functions
 *
 **********************************************************************/

void
ionic_txq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
		struct rte_eth_txq_info *qinfo)
{
	struct ionic_tx_qcq *txq = dev->data->tx_queues[queue_id];
	struct ionic_queue *q = &txq->qcq.q;

	qinfo->nb_desc = q->num_descs;
	qinfo->conf.offloads = dev->data->dev_conf.txmode.offloads;
	qinfo->conf.tx_deferred_start = txq->flags & IONIC_QCQ_F_DEFERRED;
}

static __rte_always_inline void
ionic_tx_flush(struct ionic_tx_qcq *txq)
{
	struct ionic_cq *cq = &txq->qcq.cq;
	struct ionic_queue *q = &txq->qcq.q;
	struct rte_mbuf *txm, *next;
	struct ionic_txq_comp *cq_desc_base = cq->base;
	struct ionic_txq_comp *cq_desc;
	void **info;
	u_int32_t comp_index = (u_int32_t)-1;

	cq_desc = &cq_desc_base[cq->tail_idx];
	while (color_match(cq_desc->color, cq->done_color)) {
		cq->tail_idx = Q_NEXT_TO_SRVC(cq, 1);

		/* Prefetch the next 4 descriptors (not really useful here) */
		if ((cq->tail_idx & 0x3) == 0)
			rte_prefetch0(&cq_desc_base[cq->tail_idx]);

		if (cq->tail_idx == 0)
			cq->done_color = !cq->done_color;

		comp_index = cq_desc->comp_index;

		cq_desc = &cq_desc_base[cq->tail_idx];
	}

	if (comp_index != (u_int32_t)-1) {
		while (q->tail_idx != comp_index) {
			info = IONIC_INFO_PTR(q, q->tail_idx);

			q->tail_idx = Q_NEXT_TO_SRVC(q, 1);

			/* Prefetch the next 4 descriptors */
			if ((q->tail_idx & 0x3) == 0)
				/* q desc info */
				rte_prefetch0(&q->info[q->tail_idx]);

			/*
			 * Note: you can just use rte_pktmbuf_free,
			 * but this loop is faster
			 */
			txm = info[0];
			while (txm != NULL) {
				next = txm->next;
				rte_pktmbuf_free_seg(txm);
				txm = next;
			}
		}
	}
}

void __rte_cold
ionic_dev_tx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	struct ionic_tx_qcq *txq = dev->data->tx_queues[qid];
	struct ionic_tx_stats *stats = &txq->stats;

	IONIC_PRINT_CALL();

	IONIC_PRINT(DEBUG, "TX queue %u pkts %ju tso %ju",
		txq->qcq.q.index, stats->packets, stats->tso);

	ionic_lif_txq_deinit(txq);

	ionic_qcq_free(&txq->qcq);
}

int __rte_cold
ionic_dev_tx_queue_stop(struct rte_eth_dev *eth_dev, uint16_t tx_queue_id)
{
	struct ionic_tx_qcq *txq;

	IONIC_PRINT(DEBUG, "Stopping TX queue %u", tx_queue_id);

	txq = eth_dev->data->tx_queues[tx_queue_id];

	eth_dev->data->tx_queue_state[tx_queue_id] =
		RTE_ETH_QUEUE_STATE_STOPPED;

	/*
	 * Note: we should better post NOP Tx desc and wait for its completion
	 * before disabling Tx queue
	 */

	ionic_qcq_disable(&txq->qcq);

	ionic_tx_flush(txq);

	return 0;
}

int __rte_cold
ionic_dev_tx_queue_setup(struct rte_eth_dev *eth_dev, uint16_t tx_queue_id,
		uint16_t nb_desc, uint32_t socket_id,
		const struct rte_eth_txconf *tx_conf)
{
	struct ionic_lif *lif = IONIC_ETH_DEV_TO_LIF(eth_dev);
	struct ionic_tx_qcq *txq;
	uint64_t offloads;
	int err;

	if (tx_queue_id >= lif->ntxqcqs) {
		IONIC_PRINT(DEBUG, "Queue index %u not available "
			"(max %u queues)",
			tx_queue_id, lif->ntxqcqs);
		return -EINVAL;
	}

	offloads = tx_conf->offloads | eth_dev->data->dev_conf.txmode.offloads;
	IONIC_PRINT(DEBUG,
		"Configuring skt %u TX queue %u with %u buffers, offloads %jx",
		socket_id, tx_queue_id, nb_desc, offloads);

	/* Validate number of receive descriptors */
	if (!rte_is_power_of_2(nb_desc) || nb_desc < IONIC_MIN_RING_DESC)
		return -EINVAL; /* or use IONIC_DEFAULT_RING_DESC */

	/* Free memory prior to re-allocation if needed... */
	if (eth_dev->data->tx_queues[tx_queue_id] != NULL) {
		ionic_dev_tx_queue_release(eth_dev, tx_queue_id);
		eth_dev->data->tx_queues[tx_queue_id] = NULL;
	}

	eth_dev->data->tx_queue_state[tx_queue_id] =
		RTE_ETH_QUEUE_STATE_STOPPED;

	err = ionic_tx_qcq_alloc(lif, socket_id, tx_queue_id, nb_desc, &txq);
	if (err) {
		IONIC_PRINT(DEBUG, "Queue allocation failure");
		return -EINVAL;
	}

	/* Do not start queue with rte_eth_dev_start() */
	if (tx_conf->tx_deferred_start)
		txq->flags |= IONIC_QCQ_F_DEFERRED;

	/* Convert the offload flags into queue flags */
	if (offloads & RTE_ETH_TX_OFFLOAD_IPV4_CKSUM)
		txq->flags |= IONIC_QCQ_F_CSUM_L3;
	if (offloads & RTE_ETH_TX_OFFLOAD_TCP_CKSUM)
		txq->flags |= IONIC_QCQ_F_CSUM_TCP;
	if (offloads & RTE_ETH_TX_OFFLOAD_UDP_CKSUM)
		txq->flags |= IONIC_QCQ_F_CSUM_UDP;

	eth_dev->data->tx_queues[tx_queue_id] = txq;

	return 0;
}

/*
 * Start Transmit Units for specified queue.
 */
int __rte_cold
ionic_dev_tx_queue_start(struct rte_eth_dev *eth_dev, uint16_t tx_queue_id)
{
	uint8_t *tx_queue_state = eth_dev->data->tx_queue_state;
	struct ionic_tx_qcq *txq;
	int err;

	if (tx_queue_state[tx_queue_id] == RTE_ETH_QUEUE_STATE_STARTED) {
		IONIC_PRINT(DEBUG, "TX queue %u already started",
			tx_queue_id);
		return 0;
	}

	txq = eth_dev->data->tx_queues[tx_queue_id];

	IONIC_PRINT(DEBUG, "Starting TX queue %u, %u descs",
		tx_queue_id, txq->qcq.q.num_descs);

	if (!(txq->flags & IONIC_QCQ_F_INITED)) {
		err = ionic_lif_txq_init(txq);
		if (err)
			return err;
	} else {
		ionic_qcq_enable(&txq->qcq);
	}

	tx_queue_state[tx_queue_id] = RTE_ETH_QUEUE_STATE_STARTED;

	return 0;
}

static void
ionic_tx_tcp_pseudo_csum(struct rte_mbuf *txm)
{
	struct ether_hdr *eth_hdr = rte_pktmbuf_mtod(txm, struct ether_hdr *);
	char *l3_hdr = ((char *)eth_hdr) + txm->l2_len;
	struct rte_tcp_hdr *tcp_hdr = (struct rte_tcp_hdr *)
		(l3_hdr + txm->l3_len);

	if (txm->ol_flags & RTE_MBUF_F_TX_IP_CKSUM) {
		struct rte_ipv4_hdr *ipv4_hdr = (struct rte_ipv4_hdr *)l3_hdr;
		ipv4_hdr->hdr_checksum = 0;
		tcp_hdr->cksum = 0;
		tcp_hdr->cksum = rte_ipv4_udptcp_cksum(ipv4_hdr, tcp_hdr);
	} else {
		struct rte_ipv6_hdr *ipv6_hdr = (struct rte_ipv6_hdr *)l3_hdr;
		tcp_hdr->cksum = 0;
		tcp_hdr->cksum = rte_ipv6_udptcp_cksum(ipv6_hdr, tcp_hdr);
	}
}

static void
ionic_tx_tcp_inner_pseudo_csum(struct rte_mbuf *txm)
{
	struct ether_hdr *eth_hdr = rte_pktmbuf_mtod(txm, struct ether_hdr *);
	char *l3_hdr = ((char *)eth_hdr) + txm->outer_l2_len +
		txm->outer_l3_len + txm->l2_len;
	struct rte_tcp_hdr *tcp_hdr = (struct rte_tcp_hdr *)
		(l3_hdr + txm->l3_len);

	if (txm->ol_flags & RTE_MBUF_F_TX_IPV4) {
		struct rte_ipv4_hdr *ipv4_hdr = (struct rte_ipv4_hdr *)l3_hdr;
		ipv4_hdr->hdr_checksum = 0;
		tcp_hdr->cksum = 0;
		tcp_hdr->cksum = rte_ipv4_udptcp_cksum(ipv4_hdr, tcp_hdr);
	} else {
		struct rte_ipv6_hdr *ipv6_hdr = (struct rte_ipv6_hdr *)l3_hdr;
		tcp_hdr->cksum = 0;
		tcp_hdr->cksum = rte_ipv6_udptcp_cksum(ipv6_hdr, tcp_hdr);
	}
}

static void
ionic_tx_tso_post(struct ionic_queue *q, struct ionic_txq_desc *desc,
		struct rte_mbuf *txm,
		rte_iova_t addr, uint8_t nsge, uint16_t len,
		uint32_t hdrlen, uint32_t mss,
		bool encap,
		uint16_t vlan_tci, bool has_vlan,
		bool start, bool done)
{
	void **info;
	uint8_t flags = 0;
	flags |= has_vlan ? IONIC_TXQ_DESC_FLAG_VLAN : 0;
	flags |= encap ? IONIC_TXQ_DESC_FLAG_ENCAP : 0;
	flags |= start ? IONIC_TXQ_DESC_FLAG_TSO_SOT : 0;
	flags |= done ? IONIC_TXQ_DESC_FLAG_TSO_EOT : 0;

	desc->cmd = encode_txq_desc_cmd(IONIC_TXQ_DESC_OPCODE_TSO,
		flags, nsge, addr);
	desc->len = len;
	desc->vlan_tci = vlan_tci;
	desc->hdr_len = hdrlen;
	desc->mss = mss;

	if (done) {
		info = IONIC_INFO_PTR(q, q->head_idx);
		info[0] = txm;
	}

	q->head_idx = Q_NEXT_TO_POST(q, 1);
}

static struct ionic_txq_desc *
ionic_tx_tso_next(struct ionic_tx_qcq *txq, struct ionic_txq_sg_elem **elem)
{
	struct ionic_queue *q = &txq->qcq.q;
	struct ionic_txq_desc *desc_base = q->base;
	struct ionic_txq_sg_desc_v1 *sg_desc_base = q->sg_base;
	struct ionic_txq_desc *desc = &desc_base[q->head_idx];
	struct ionic_txq_sg_desc_v1 *sg_desc = &sg_desc_base[q->head_idx];

	*elem = sg_desc->elems;
	return desc;
}

static int
ionic_tx_tso(struct ionic_tx_qcq *txq, struct rte_mbuf *txm)
{
	struct ionic_queue *q = &txq->qcq.q;
	struct ionic_tx_stats *stats = &txq->stats;
	struct ionic_txq_desc *desc;
	struct ionic_txq_sg_elem *elem;
	struct rte_mbuf *txm_seg;
	rte_iova_t data_iova;
	uint64_t desc_addr = 0, next_addr;
	uint16_t desc_len = 0;
	uint8_t desc_nsge;
	uint32_t hdrlen;
	uint32_t mss = txm->tso_segsz;
	uint32_t frag_left = 0;
	uint32_t left;
	uint32_t seglen;
	uint32_t len;
	uint32_t offset = 0;
	bool start, done;
	bool encap;
	bool has_vlan = !!(txm->ol_flags & RTE_MBUF_F_TX_VLAN);
	uint16_t vlan_tci = txm->vlan_tci;
	uint64_t ol_flags = txm->ol_flags;

	encap = ((ol_flags & RTE_MBUF_F_TX_OUTER_IP_CKSUM) ||
		 (ol_flags & RTE_MBUF_F_TX_OUTER_UDP_CKSUM)) &&
		((ol_flags & RTE_MBUF_F_TX_OUTER_IPV4) ||
		 (ol_flags & RTE_MBUF_F_TX_OUTER_IPV6));

	/* Preload inner-most TCP csum field with IP pseudo hdr
	 * calculated with IP length set to zero.  HW will later
	 * add in length to each TCP segment resulting from the TSO.
	 */

	if (encap) {
		ionic_tx_tcp_inner_pseudo_csum(txm);
		hdrlen = txm->outer_l2_len + txm->outer_l3_len +
			txm->l2_len + txm->l3_len + txm->l4_len;
	} else {
		ionic_tx_tcp_pseudo_csum(txm);
		hdrlen = txm->l2_len + txm->l3_len + txm->l4_len;
	}

	seglen = hdrlen + mss;
	left = txm->data_len;
	data_iova = rte_mbuf_data_iova(txm);

	desc = ionic_tx_tso_next(txq, &elem);
	start = true;

	/* Chop data up into desc segments */

	while (left > 0) {
		len = RTE_MIN(seglen, left);
		frag_left = seglen - len;
		desc_addr = rte_cpu_to_le_64(data_iova + offset);
		desc_len = len;
		desc_nsge = 0;
		left -= len;
		offset += len;
		if (txm->nb_segs > 1 && frag_left > 0)
			continue;
		done = (txm->nb_segs == 1 && left == 0);
		ionic_tx_tso_post(q, desc, txm,
			desc_addr, desc_nsge, desc_len,
			hdrlen, mss,
			encap,
			vlan_tci, has_vlan,
			start, done);
		desc = ionic_tx_tso_next(txq, &elem);
		start = false;
		seglen = mss;
	}

	/* Chop frags into desc segments */

	txm_seg = txm->next;
	while (txm_seg != NULL) {
		offset = 0;
		data_iova = rte_mbuf_data_iova(txm_seg);
		left = txm_seg->data_len;

		while (left > 0) {
			next_addr = rte_cpu_to_le_64(data_iova + offset);
			if (frag_left > 0) {
				len = RTE_MIN(frag_left, left);
				frag_left -= len;
				elem->addr = next_addr;
				elem->len = len;
				elem++;
				desc_nsge++;
			} else {
				len = RTE_MIN(mss, left);
				frag_left = mss - len;
				desc_addr = next_addr;
				desc_len = len;
				desc_nsge = 0;
			}
			left -= len;
			offset += len;
			if (txm_seg->next != NULL && frag_left > 0)
				continue;

			done = (txm_seg->next == NULL && left == 0);
			ionic_tx_tso_post(q, desc, txm_seg,
				desc_addr, desc_nsge, desc_len,
				hdrlen, mss,
				encap,
				vlan_tci, has_vlan,
				start, done);
			desc = ionic_tx_tso_next(txq, &elem);
			start = false;
		}

		txm_seg = txm_seg->next;
	}

	stats->tso++;

	return 0;
}

static __rte_always_inline int
ionic_tx(struct ionic_tx_qcq *txq, struct rte_mbuf *txm)
{
	struct ionic_queue *q = &txq->qcq.q;
	struct ionic_txq_desc *desc, *desc_base = q->base;
	struct ionic_txq_sg_desc_v1 *sg_desc_base = q->sg_base;
	struct ionic_txq_sg_elem *elem;
	struct ionic_tx_stats *stats = &txq->stats;
	struct rte_mbuf *txm_seg;
	void **info;
	bool encap;
	bool has_vlan;
	uint64_t ol_flags = txm->ol_flags;
	uint64_t addr;
	uint8_t opcode = IONIC_TXQ_DESC_OPCODE_CSUM_NONE;
	uint8_t flags = 0;

	desc = &desc_base[q->head_idx];
	info = IONIC_INFO_PTR(q, q->head_idx);

	if ((ol_flags & RTE_MBUF_F_TX_IP_CKSUM) &&
	    (txq->flags & IONIC_QCQ_F_CSUM_L3)) {
		opcode = IONIC_TXQ_DESC_OPCODE_CSUM_HW;
		flags |= IONIC_TXQ_DESC_FLAG_CSUM_L3;
	}

	if (((ol_flags & RTE_MBUF_F_TX_TCP_CKSUM) &&
	     (txq->flags & IONIC_QCQ_F_CSUM_TCP)) ||
	    ((ol_flags & RTE_MBUF_F_TX_UDP_CKSUM) &&
	     (txq->flags & IONIC_QCQ_F_CSUM_UDP))) {
		opcode = IONIC_TXQ_DESC_OPCODE_CSUM_HW;
		flags |= IONIC_TXQ_DESC_FLAG_CSUM_L4;
	}

	if (opcode == IONIC_TXQ_DESC_OPCODE_CSUM_NONE)
		stats->no_csum++;

	has_vlan = (ol_flags & RTE_MBUF_F_TX_VLAN);
	encap = ((ol_flags & RTE_MBUF_F_TX_OUTER_IP_CKSUM) ||
			(ol_flags & RTE_MBUF_F_TX_OUTER_UDP_CKSUM)) &&
			((ol_flags & RTE_MBUF_F_TX_OUTER_IPV4) ||
			 (ol_flags & RTE_MBUF_F_TX_OUTER_IPV6));

	flags |= has_vlan ? IONIC_TXQ_DESC_FLAG_VLAN : 0;
	flags |= encap ? IONIC_TXQ_DESC_FLAG_ENCAP : 0;

	addr = rte_cpu_to_le_64(rte_mbuf_data_iova(txm));

	desc->cmd = encode_txq_desc_cmd(opcode, flags, txm->nb_segs - 1, addr);
	desc->len = txm->data_len;
	desc->vlan_tci = txm->vlan_tci;

	info[0] = txm;

	elem = sg_desc_base[q->head_idx].elems;

	txm_seg = txm->next;
	while (txm_seg != NULL) {
		elem->len = txm_seg->data_len;
		elem->addr = rte_cpu_to_le_64(rte_mbuf_data_iova(txm_seg));
		elem++;
		txm_seg = txm_seg->next;
	}

	q->head_idx = Q_NEXT_TO_POST(q, 1);

	return 0;
}

uint16_t
ionic_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts)
{
	struct ionic_tx_qcq *txq = tx_queue;
	struct ionic_queue *q = &txq->qcq.q;
	struct ionic_tx_stats *stats = &txq->stats;
	uint32_t next_q_head_idx;
	uint32_t bytes_tx = 0;
	uint16_t nb_avail, nb_tx = 0;
	int err;

	/* Cleaning old buffers */
	ionic_tx_flush(txq);

	nb_avail = ionic_q_space_avail(q);
	if (unlikely(nb_avail < nb_pkts)) {
		stats->stop += nb_pkts - nb_avail;
		nb_pkts = nb_avail;
	}

	while (nb_tx < nb_pkts) {
		next_q_head_idx = Q_NEXT_TO_POST(q, 1);
		if ((next_q_head_idx & 0x3) == 0) {
			struct ionic_txq_desc *desc_base = q->base;
			rte_prefetch0(&desc_base[next_q_head_idx]);
			rte_prefetch0(&q->info[next_q_head_idx]);
		}

		if (tx_pkts[nb_tx]->ol_flags & RTE_MBUF_F_TX_TCP_SEG)
			err = ionic_tx_tso(txq, tx_pkts[nb_tx]);
		else
			err = ionic_tx(txq, tx_pkts[nb_tx]);
		if (err) {
			stats->drop += nb_pkts - nb_tx;
			break;
		}

		bytes_tx += tx_pkts[nb_tx]->pkt_len;
		nb_tx++;
	}

	if (nb_tx > 0) {
		rte_wmb();
		ionic_q_flush(q);
	}

	stats->packets += nb_tx;
	stats->bytes += bytes_tx;

	return nb_tx;
}

/*********************************************************************
 *
 *  TX prep functions
 *
 **********************************************************************/

#define IONIC_TX_OFFLOAD_MASK (RTE_MBUF_F_TX_IPV4 |		\
	RTE_MBUF_F_TX_IPV6 |		\
	RTE_MBUF_F_TX_VLAN |		\
	RTE_MBUF_F_TX_IP_CKSUM |	\
	RTE_MBUF_F_TX_TCP_SEG |	\
	RTE_MBUF_F_TX_L4_MASK)

#define IONIC_TX_OFFLOAD_NOTSUP_MASK \
	(RTE_MBUF_F_TX_OFFLOAD_MASK ^ IONIC_TX_OFFLOAD_MASK)

uint16_t
ionic_prep_pkts(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct ionic_tx_qcq *txq = tx_queue;
	struct rte_mbuf *txm;
	uint64_t offloads;
	int i = 0;

	for (i = 0; i < nb_pkts; i++) {
		txm = tx_pkts[i];

		if (txm->nb_segs > txq->num_segs_fw) {
			rte_errno = -EINVAL;
			break;
		}

		offloads = txm->ol_flags;

		if (offloads & IONIC_TX_OFFLOAD_NOTSUP_MASK) {
			rte_errno = -ENOTSUP;
			break;
		}
	}

	return i;
}

/*********************************************************************
 *
 *  RX functions
 *
 **********************************************************************/

static void ionic_rx_recycle(struct ionic_queue *q, uint32_t q_desc_index,
		struct rte_mbuf *mbuf);

void
ionic_rxq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
		struct rte_eth_rxq_info *qinfo)
{
	struct ionic_rx_qcq *rxq = dev->data->rx_queues[queue_id];
	struct ionic_queue *q = &rxq->qcq.q;

	qinfo->mp = rxq->mb_pool;
	qinfo->scattered_rx = dev->data->scattered_rx;
	qinfo->nb_desc = q->num_descs;
	qinfo->conf.rx_deferred_start = rxq->flags & IONIC_QCQ_F_DEFERRED;
	qinfo->conf.offloads = dev->data->dev_conf.rxmode.offloads;
}

static void __rte_cold
ionic_rx_empty(struct ionic_rx_qcq *rxq)
{
	struct ionic_queue *q = &rxq->qcq.q;
	struct rte_mbuf *mbuf;
	void **info;

	while (q->tail_idx != q->head_idx) {
		info = IONIC_INFO_PTR(q, q->tail_idx);
		mbuf = info[0];
		rte_mempool_put(rxq->mb_pool, mbuf);

		q->tail_idx = Q_NEXT_TO_SRVC(q, 1);
	}
}

void __rte_cold
ionic_dev_rx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	struct ionic_rx_qcq *rxq = dev->data->rx_queues[qid];
	struct ionic_rx_stats *stats;

	if (!rxq)
		return;

	IONIC_PRINT_CALL();

	stats = &rxq->stats;

	IONIC_PRINT(DEBUG, "RX queue %u pkts %ju mtod %ju",
		rxq->qcq.q.index, stats->packets, stats->mtods);

	ionic_rx_empty(rxq);

	ionic_lif_rxq_deinit(rxq);

	ionic_qcq_free(&rxq->qcq);
}

int __rte_cold
ionic_dev_rx_queue_setup(struct rte_eth_dev *eth_dev,
		uint16_t rx_queue_id,
		uint16_t nb_desc,
		uint32_t socket_id,
		const struct rte_eth_rxconf *rx_conf,
		struct rte_mempool *mp)
{
	struct ionic_lif *lif = IONIC_ETH_DEV_TO_LIF(eth_dev);
	struct ionic_rx_qcq *rxq;
	uint64_t offloads;
	int err;

	if (rx_queue_id >= lif->nrxqcqs) {
		IONIC_PRINT(ERR,
			"Queue index %u not available (max %u queues)",
			rx_queue_id, lif->nrxqcqs);
		return -EINVAL;
	}

	offloads = rx_conf->offloads | eth_dev->data->dev_conf.rxmode.offloads;
	IONIC_PRINT(DEBUG,
		"Configuring skt %u RX queue %u with %u buffers, offloads %jx",
		socket_id, rx_queue_id, nb_desc, offloads);

	if (!rx_conf->rx_drop_en)
		IONIC_PRINT(WARNING, "No-drop mode is not supported");

	/* Validate number of receive descriptors */
	if (!rte_is_power_of_2(nb_desc) ||
			nb_desc < IONIC_MIN_RING_DESC ||
			nb_desc > IONIC_MAX_RING_DESC) {
		IONIC_PRINT(ERR,
			"Bad descriptor count (%u) for queue %u (min: %u)",
			nb_desc, rx_queue_id, IONIC_MIN_RING_DESC);
		return -EINVAL; /* or use IONIC_DEFAULT_RING_DESC */
	}

	/* Free memory prior to re-allocation if needed... */
	if (eth_dev->data->rx_queues[rx_queue_id] != NULL) {
		ionic_dev_rx_queue_release(eth_dev, rx_queue_id);
		eth_dev->data->rx_queues[rx_queue_id] = NULL;
	}

	eth_dev->data->rx_queue_state[rx_queue_id] =
		RTE_ETH_QUEUE_STATE_STOPPED;

	err = ionic_rx_qcq_alloc(lif, socket_id, rx_queue_id, nb_desc,
			&rxq);
	if (err) {
		IONIC_PRINT(ERR, "Queue %d allocation failure", rx_queue_id);
		return -EINVAL;
	}

	rxq->mb_pool = mp;

	/*
	 * Note: the interface does not currently support
	 * RTE_ETH_RX_OFFLOAD_KEEP_CRC, please also consider ETHER_CRC_LEN
	 * when the adapter will be able to keep the CRC and subtract
	 * it to the length for all received packets:
	 * if (eth_dev->data->dev_conf.rxmode.offloads &
	 *     RTE_ETH_RX_OFFLOAD_KEEP_CRC)
	 *   rxq->crc_len = ETHER_CRC_LEN;
	 */

	/* Do not start queue with rte_eth_dev_start() */
	if (rx_conf->rx_deferred_start)
		rxq->flags |= IONIC_QCQ_F_DEFERRED;

	eth_dev->data->rx_queues[rx_queue_id] = rxq;

	return 0;
}

static __rte_always_inline void
ionic_rx_clean(struct ionic_rx_qcq *rxq,
		uint32_t q_desc_index, uint32_t cq_desc_index,
		void *service_cb_arg)
{
	struct ionic_queue *q = &rxq->qcq.q;
	struct ionic_cq *cq = &rxq->qcq.cq;
	struct ionic_rxq_comp *cq_desc_base = cq->base;
	struct ionic_rxq_comp *cq_desc = &cq_desc_base[cq_desc_index];
	struct rte_mbuf *rxm, *rxm_seg;
	uint32_t max_frame_size =
		rxq->qcq.lif->eth_dev->data->mtu + RTE_ETHER_HDR_LEN;
	uint64_t pkt_flags = 0;
	uint32_t pkt_type;
	struct ionic_rx_stats *stats = &rxq->stats;
	struct ionic_rx_service *recv_args = (struct ionic_rx_service *)
		service_cb_arg;
	uint32_t buf_size = (uint16_t)
		(rte_pktmbuf_data_room_size(rxq->mb_pool) -
		RTE_PKTMBUF_HEADROOM);
	uint32_t left;
	void **info;

	assert(q_desc_index == cq_desc->comp_index);

	info = IONIC_INFO_PTR(q, cq_desc->comp_index);

	rxm = info[0];

	if (!recv_args) {
		stats->no_cb_arg++;
		/* Flush */
		rte_pktmbuf_free(rxm);
		/*
		 * Note: rte_mempool_put is faster with no segs
		 * rte_mempool_put(rxq->mb_pool, rxm);
		 */
		return;
	}

	if (cq_desc->status) {
		stats->bad_cq_status++;
		ionic_rx_recycle(q, q_desc_index, rxm);
		return;
	}

	if (recv_args->nb_rx >= recv_args->nb_pkts) {
		stats->no_room++;
		ionic_rx_recycle(q, q_desc_index, rxm);
		return;
	}

	if (cq_desc->len > max_frame_size ||
			cq_desc->len == 0) {
		stats->bad_len++;
		ionic_rx_recycle(q, q_desc_index, rxm);
		return;
	}

	rxm->data_off = RTE_PKTMBUF_HEADROOM;
	rte_prefetch1((char *)rxm->buf_addr + rxm->data_off);
	rxm->nb_segs = 1; /* cq_desc->num_sg_elems */
	rxm->pkt_len = cq_desc->len;
	rxm->port = rxq->qcq.lif->port_id;

	left = cq_desc->len;

	rxm->data_len = RTE_MIN(buf_size, left);
	left -= rxm->data_len;

	rxm_seg = rxm->next;
	while (rxm_seg && left) {
		rxm_seg->data_len = RTE_MIN(buf_size, left);
		left -= rxm_seg->data_len;

		rxm_seg = rxm_seg->next;
		rxm->nb_segs++;
	}

	/* RSS */
	pkt_flags |= RTE_MBUF_F_RX_RSS_HASH;
	rxm->hash.rss = cq_desc->rss_hash;

	/* Vlan Strip */
	if (cq_desc->csum_flags & IONIC_RXQ_COMP_CSUM_F_VLAN) {
		pkt_flags |= RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED;
		rxm->vlan_tci = cq_desc->vlan_tci;
	}

	/* Checksum */
	if (cq_desc->csum_flags & IONIC_RXQ_COMP_CSUM_F_CALC) {
		if (cq_desc->csum_flags & IONIC_RXQ_COMP_CSUM_F_IP_OK)
			pkt_flags |= RTE_MBUF_F_RX_IP_CKSUM_GOOD;
		else if (cq_desc->csum_flags & IONIC_RXQ_COMP_CSUM_F_IP_BAD)
			pkt_flags |= RTE_MBUF_F_RX_IP_CKSUM_BAD;

		if ((cq_desc->csum_flags & IONIC_RXQ_COMP_CSUM_F_TCP_OK) ||
			(cq_desc->csum_flags & IONIC_RXQ_COMP_CSUM_F_UDP_OK))
			pkt_flags |= RTE_MBUF_F_RX_L4_CKSUM_GOOD;
		else if ((cq_desc->csum_flags &
				IONIC_RXQ_COMP_CSUM_F_TCP_BAD) ||
				(cq_desc->csum_flags &
				IONIC_RXQ_COMP_CSUM_F_UDP_BAD))
			pkt_flags |= RTE_MBUF_F_RX_L4_CKSUM_BAD;
	}

	rxm->ol_flags = pkt_flags;

	/* Packet Type */
	switch (cq_desc->pkt_type_color & IONIC_RXQ_COMP_PKT_TYPE_MASK) {
	case IONIC_PKT_TYPE_IPV4:
		pkt_type = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4;
		break;
	case IONIC_PKT_TYPE_IPV6:
		pkt_type = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6;
		break;
	case IONIC_PKT_TYPE_IPV4_TCP:
		pkt_type = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4 |
			RTE_PTYPE_L4_TCP;
		break;
	case IONIC_PKT_TYPE_IPV6_TCP:
		pkt_type = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6 |
			RTE_PTYPE_L4_TCP;
		break;
	case IONIC_PKT_TYPE_IPV4_UDP:
		pkt_type = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4 |
			RTE_PTYPE_L4_UDP;
		break;
	case IONIC_PKT_TYPE_IPV6_UDP:
		pkt_type = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6 |
			RTE_PTYPE_L4_UDP;
		break;
	default:
		{
			struct rte_ether_hdr *eth_h = rte_pktmbuf_mtod(rxm,
				struct rte_ether_hdr *);
			uint16_t ether_type = eth_h->ether_type;
			if (ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_ARP))
				pkt_type = RTE_PTYPE_L2_ETHER_ARP;
			else
				pkt_type = RTE_PTYPE_UNKNOWN;
			stats->mtods++;
			break;
		}
	}

	rxm->packet_type = pkt_type;

	recv_args->rx_pkts[recv_args->nb_rx] = rxm;
	recv_args->nb_rx++;

	stats->packets++;
	stats->bytes += rxm->pkt_len;
}

static void
ionic_rx_recycle(struct ionic_queue *q, uint32_t q_desc_index,
		 struct rte_mbuf *mbuf)
{
	struct ionic_rxq_desc *desc_base = q->base;
	struct ionic_rxq_desc *old = &desc_base[q_desc_index];
	struct ionic_rxq_desc *new = &desc_base[q->head_idx];

	new->addr = old->addr;
	new->len = old->len;

	q->info[q->head_idx] = mbuf;

	q->head_idx = Q_NEXT_TO_POST(q, 1);

	ionic_q_flush(q);
}

static __rte_always_inline int
ionic_rx_fill(struct ionic_rx_qcq *rxq, uint32_t len)
{
	struct ionic_queue *q = &rxq->qcq.q;
	struct ionic_rxq_desc *desc, *desc_base = q->base;
	struct ionic_rxq_sg_desc *sg_desc, *sg_desc_base = q->sg_base;
	struct ionic_rxq_sg_elem *elem;
	void **info;
	rte_iova_t dma_addr;
	uint32_t i, j, nsegs, buf_size, size;

	buf_size = (uint16_t)(rte_pktmbuf_data_room_size(rxq->mb_pool) -
		RTE_PKTMBUF_HEADROOM);

	/* Initialize software ring entries */
	for (i = ionic_q_space_avail(q); i; i--) {
		struct rte_mbuf *rxm = rte_mbuf_raw_alloc(rxq->mb_pool);
		struct rte_mbuf *prev_rxm_seg;

		if (rxm == NULL) {
			IONIC_PRINT(ERR, "RX mbuf alloc failed");
			return -ENOMEM;
		}

		info = IONIC_INFO_PTR(q, q->head_idx);

		nsegs = (len + buf_size - 1) / buf_size;

		desc = &desc_base[q->head_idx];
		dma_addr = rte_cpu_to_le_64(rte_mbuf_data_iova_default(rxm));
		desc->addr = dma_addr;
		desc->len = buf_size;
		size = buf_size;
		desc->opcode = (nsegs > 1) ? IONIC_RXQ_DESC_OPCODE_SG :
			IONIC_RXQ_DESC_OPCODE_SIMPLE;
		rxm->next = NULL;

		prev_rxm_seg = rxm;
		sg_desc = &sg_desc_base[q->head_idx];
		elem = sg_desc->elems;
		for (j = 0; j < nsegs - 1 && j < IONIC_RX_MAX_SG_ELEMS; j++) {
			struct rte_mbuf *rxm_seg;
			rte_iova_t data_iova;

			rxm_seg = rte_mbuf_raw_alloc(rxq->mb_pool);
			if (rxm_seg == NULL) {
				IONIC_PRINT(ERR, "RX mbuf alloc failed");
				return -ENOMEM;
			}

			data_iova = rte_mbuf_data_iova(rxm_seg);
			dma_addr = rte_cpu_to_le_64(data_iova);
			elem->addr = dma_addr;
			elem->len = buf_size;
			size += buf_size;
			elem++;
			rxm_seg->next = NULL;
			prev_rxm_seg->next = rxm_seg;
			prev_rxm_seg = rxm_seg;
		}

		if (size < len)
			IONIC_PRINT(ERR, "Rx SG size is not sufficient (%d < %d)",
				size, len);

		info[0] = rxm;

		q->head_idx = Q_NEXT_TO_POST(q, 1);
	}

	ionic_q_flush(q);

	return 0;
}

/*
 * Start Receive Units for specified queue.
 */
int __rte_cold
ionic_dev_rx_queue_start(struct rte_eth_dev *eth_dev, uint16_t rx_queue_id)
{
	uint32_t frame_size = eth_dev->data->mtu + RTE_ETHER_HDR_LEN;
	uint8_t *rx_queue_state = eth_dev->data->rx_queue_state;
	struct ionic_rx_qcq *rxq;
	int err;

	if (rx_queue_state[rx_queue_id] == RTE_ETH_QUEUE_STATE_STARTED) {
		IONIC_PRINT(DEBUG, "RX queue %u already started",
			rx_queue_id);
		return 0;
	}

	rxq = eth_dev->data->rx_queues[rx_queue_id];

	IONIC_PRINT(DEBUG, "Starting RX queue %u, %u descs (size: %u)",
		rx_queue_id, rxq->qcq.q.num_descs, frame_size);

	if (!(rxq->flags & IONIC_QCQ_F_INITED)) {
		err = ionic_lif_rxq_init(rxq);
		if (err)
			return err;
	} else {
		ionic_qcq_enable(&rxq->qcq);
	}

	/* Allocate buffers for descriptor rings */
	if (ionic_rx_fill(rxq, frame_size) != 0) {
		IONIC_PRINT(ERR, "Could not alloc mbuf for queue:%d",
			rx_queue_id);
		return -1;
	}

	rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STARTED;

	return 0;
}

static __rte_always_inline void
ionic_rxq_service(struct ionic_rx_qcq *rxq, uint32_t work_to_do,
		void *service_cb_arg)
{
	struct ionic_cq *cq = &rxq->qcq.cq;
	struct ionic_queue *q = &rxq->qcq.q;
	struct ionic_rxq_comp *cq_desc, *cq_desc_base = cq->base;
	bool more;
	uint32_t curr_q_tail_idx, curr_cq_tail_idx;
	uint32_t work_done = 0;

	if (work_to_do == 0)
		return;

	cq_desc = &cq_desc_base[cq->tail_idx];
	while (color_match(cq_desc->pkt_type_color, cq->done_color)) {
		curr_cq_tail_idx = cq->tail_idx;
		cq->tail_idx = Q_NEXT_TO_SRVC(cq, 1);

		if (cq->tail_idx == 0)
			cq->done_color = !cq->done_color;

		/* Prefetch the next 4 descriptors */
		if ((cq->tail_idx & 0x3) == 0)
			rte_prefetch0(&cq_desc_base[cq->tail_idx]);

		do {
			more = (q->tail_idx != cq_desc->comp_index);

			curr_q_tail_idx = q->tail_idx;
			q->tail_idx = Q_NEXT_TO_SRVC(q, 1);

			/* Prefetch the next 4 descriptors */
			if ((q->tail_idx & 0x3) == 0)
				/* q desc info */
				rte_prefetch0(&q->info[q->tail_idx]);

			ionic_rx_clean(rxq, curr_q_tail_idx, curr_cq_tail_idx,
				service_cb_arg);

		} while (more);

		if (++work_done == work_to_do)
			break;

		cq_desc = &cq_desc_base[cq->tail_idx];
	}
}

/*
 * Stop Receive Units for specified queue.
 */
int __rte_cold
ionic_dev_rx_queue_stop(struct rte_eth_dev *eth_dev, uint16_t rx_queue_id)
{
	struct ionic_rx_qcq *rxq;

	IONIC_PRINT(DEBUG, "Stopping RX queue %u", rx_queue_id);

	rxq = eth_dev->data->rx_queues[rx_queue_id];

	eth_dev->data->rx_queue_state[rx_queue_id] =
		RTE_ETH_QUEUE_STATE_STOPPED;

	ionic_qcq_disable(&rxq->qcq);

	/* Flush */
	ionic_rxq_service(rxq, -1, NULL);

	return 0;
}

uint16_t
ionic_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts)
{
	struct ionic_rx_qcq *rxq = rx_queue;
	uint32_t frame_size =
		rxq->qcq.lif->eth_dev->data->mtu + RTE_ETHER_HDR_LEN;
	struct ionic_rx_service service_cb_arg;

	service_cb_arg.rx_pkts = rx_pkts;
	service_cb_arg.nb_pkts = nb_pkts;
	service_cb_arg.nb_rx = 0;

	ionic_rxq_service(rxq, nb_pkts, &service_cb_arg);

	ionic_rx_fill(rxq, frame_size);

	return service_cb_arg.nb_rx;
}
