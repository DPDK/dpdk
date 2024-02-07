/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2022 Advanced Micro Devices, Inc.
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#include <rte_common.h>
#include <rte_byteorder.h>
#include <rte_errno.h>
#include <rte_log.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_ethdev.h>
#include <ethdev_driver.h>

#include "ionic.h"
#include "ionic_dev.h"
#include "ionic_lif.h"
#include "ionic_ethdev.h"
#include "ionic_rxtx.h"
#include "ionic_logs.h"

static void
ionic_empty_array(void **array, uint32_t cnt, uint16_t idx)
{
	uint32_t i;

	for (i = idx; i < cnt; i++)
		if (array[i])
			rte_pktmbuf_free_seg(array[i]);

	memset(array, 0, sizeof(void *) * cnt);
}

static void __rte_cold
ionic_tx_empty(struct ionic_tx_qcq *txq)
{
	struct ionic_queue *q = &txq->qcq.q;

	ionic_empty_array(q->info, q->num_descs * q->num_segs, 0);
}

static void __rte_cold
ionic_rx_empty(struct ionic_rx_qcq *rxq)
{
	struct ionic_queue *q = &rxq->qcq.q;

	/*
	 * Walk the full info array so that the clean up includes any
	 * fragments that were left dangling for later reuse
	 */
	ionic_empty_array(q->info, q->num_descs * q->num_segs, 0);

	ionic_empty_array((void **)rxq->mbs,
			IONIC_MBUF_BULK_ALLOC, rxq->mb_idx);
	rxq->mb_idx = 0;
}

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
	if (txq->flags & IONIC_QCQ_F_FAST_FREE)
		qinfo->conf.offloads |= RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;
	qinfo->conf.tx_deferred_start = txq->flags & IONIC_QCQ_F_DEFERRED;
}

void __rte_cold
ionic_dev_tx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	struct ionic_tx_qcq *txq = dev->data->tx_queues[qid];

	IONIC_PRINT_CALL();

	ionic_qcq_free(&txq->qcq);
}

int __rte_cold
ionic_dev_tx_queue_stop(struct rte_eth_dev *eth_dev, uint16_t tx_queue_id)
{
	struct ionic_tx_stats *stats;
	struct ionic_tx_qcq *txq;

	IONIC_PRINT(DEBUG, "Stopping TX queue %u", tx_queue_id);

	txq = eth_dev->data->tx_queues[tx_queue_id];

	eth_dev->data->tx_queue_state[tx_queue_id] =
		RTE_ETH_QUEUE_STATE_STOPPED;

	/*
	 * Note: we should better post NOP Tx desc and wait for its completion
	 * before disabling Tx queue
	 */

	ionic_lif_txq_deinit(txq);

	/* Free all buffers from descriptor ring */
	ionic_tx_empty(txq);

	stats = &txq->stats;
	IONIC_PRINT(DEBUG, "TX queue %u pkts %ju tso %ju",
		txq->qcq.q.index, stats->packets, stats->tso);

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

	if (tx_conf->tx_free_thresh > nb_desc) {
		IONIC_PRINT(ERR,
			"tx_free_thresh must be less than nb_desc (%u)",
			nb_desc);
		return -EINVAL;
	}

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
	if (offloads & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE)
		txq->flags |= IONIC_QCQ_F_FAST_FREE;

	txq->free_thresh =
		tx_conf->tx_free_thresh ? tx_conf->tx_free_thresh :
		nb_desc - IONIC_DEF_TXRX_BURST;

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

	err = ionic_lif_txq_init(txq);
	if (err)
		return err;

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
	struct rte_mbuf *txm_seg;
	void **info;
	uint64_t cmd;
	uint8_t flags = 0;
	int i;

	flags |= has_vlan ? IONIC_TXQ_DESC_FLAG_VLAN : 0;
	flags |= encap ? IONIC_TXQ_DESC_FLAG_ENCAP : 0;
	flags |= start ? IONIC_TXQ_DESC_FLAG_TSO_SOT : 0;
	flags |= done ? IONIC_TXQ_DESC_FLAG_TSO_EOT : 0;

	cmd = encode_txq_desc_cmd(IONIC_TXQ_DESC_OPCODE_TSO,
		flags, nsge, addr);
	desc->cmd = rte_cpu_to_le_64(cmd);
	desc->len = rte_cpu_to_le_16(len);
	desc->vlan_tci = rte_cpu_to_le_16(vlan_tci);
	desc->hdr_len = rte_cpu_to_le_16(hdrlen);
	desc->mss = rte_cpu_to_le_16(mss);

	if (done) {
		info = IONIC_INFO_PTR(q, q->head_idx);

		/* Walk the mbuf chain to stash pointers in the array */
		txm_seg = txm;
		for (i = 0; i < txm->nb_segs; i++) {
			info[i] = txm_seg;
			txm_seg = txm_seg->next;
		}
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

int
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
	uint8_t desc_nsge = 0;
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
	bool use_sgl = !!(txq->flags & IONIC_QCQ_F_SG);
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

	desc = ionic_tx_tso_next(txq, &elem);
	txm_seg = txm;
	start = true;
	seglen = hdrlen + mss;

	/* Walk the chain of mbufs */
	while (txm_seg != NULL) {
		offset = 0;
		data_iova = rte_mbuf_data_iova(txm_seg);
		left = txm_seg->data_len;

		/* Split the mbuf data up into multiple descriptors */
		while (left > 0) {
			next_addr = rte_cpu_to_le_64(data_iova + offset);
			if (frag_left > 0 && use_sgl) {
				/* Fill previous descriptor's SGE */
				len = RTE_MIN(frag_left, left);
				frag_left -= len;
				elem->addr = next_addr;
				elem->len = rte_cpu_to_le_16(len);
				elem++;
				desc_nsge++;
			} else {
				/* Fill new descriptor's data field */
				len = RTE_MIN(seglen, left);
				frag_left = seglen - len;
				desc_addr = next_addr;
				desc_len = len;
				desc_nsge = 0;
			}
			left -= len;
			offset += len;

			/* Pack the next mbuf's data into the descriptor */
			if (txm_seg->next != NULL && frag_left > 0 && use_sgl)
				break;

			done = (txm_seg->next == NULL && left == 0);
			ionic_tx_tso_post(q, desc, txm_seg,
				desc_addr, desc_nsge, desc_len,
				hdrlen, mss,
				encap,
				vlan_tci, has_vlan,
				start, done);
			desc = ionic_tx_tso_next(txq, &elem);
			start = false;
			seglen = mss;
		}

		txm_seg = txm_seg->next;
	}

	stats->tso++;

	return 0;
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

void __rte_cold
ionic_dev_rx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	struct ionic_rx_qcq *rxq = dev->data->rx_queues[qid];

	if (!rxq)
		return;

	IONIC_PRINT_CALL();

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

	err = ionic_rx_qcq_alloc(lif, socket_id, rx_queue_id, nb_desc, mp,
			&rxq);
	if (err) {
		IONIC_PRINT(ERR, "Queue %d allocation failure", rx_queue_id);
		return -EINVAL;
	}

	rxq->mb_pool = mp;
	rxq->wdog_ms = IONIC_Q_WDOG_MS;

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

#define IONIC_CSUM_FLAG_MASK (IONIC_RXQ_COMP_CSUM_F_VLAN - 1)
const uint64_t ionic_csum_flags[IONIC_CSUM_FLAG_MASK]
		__rte_cache_aligned = {
	/* IP_BAD set */
	[IONIC_RXQ_COMP_CSUM_F_IP_BAD] = RTE_MBUF_F_RX_IP_CKSUM_BAD,
	[IONIC_RXQ_COMP_CSUM_F_IP_BAD | IONIC_RXQ_COMP_CSUM_F_TCP_OK] =
			RTE_MBUF_F_RX_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_GOOD,
	[IONIC_RXQ_COMP_CSUM_F_IP_BAD | IONIC_RXQ_COMP_CSUM_F_TCP_BAD] =
			RTE_MBUF_F_RX_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_BAD,
	[IONIC_RXQ_COMP_CSUM_F_IP_BAD | IONIC_RXQ_COMP_CSUM_F_UDP_OK] =
			RTE_MBUF_F_RX_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_GOOD,
	[IONIC_RXQ_COMP_CSUM_F_IP_BAD | IONIC_RXQ_COMP_CSUM_F_UDP_BAD] =
			RTE_MBUF_F_RX_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_BAD,
	/* IP_OK set */
	[IONIC_RXQ_COMP_CSUM_F_IP_OK] = RTE_MBUF_F_RX_IP_CKSUM_GOOD,
	[IONIC_RXQ_COMP_CSUM_F_IP_OK | IONIC_RXQ_COMP_CSUM_F_TCP_OK] =
			RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_GOOD,
	[IONIC_RXQ_COMP_CSUM_F_IP_OK | IONIC_RXQ_COMP_CSUM_F_TCP_BAD] =
			RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_BAD,
	[IONIC_RXQ_COMP_CSUM_F_IP_OK | IONIC_RXQ_COMP_CSUM_F_UDP_OK] =
			RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_GOOD,
	[IONIC_RXQ_COMP_CSUM_F_IP_OK | IONIC_RXQ_COMP_CSUM_F_UDP_BAD] =
			RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_BAD,
	/* No IP flag set */
	[IONIC_RXQ_COMP_CSUM_F_TCP_OK] = RTE_MBUF_F_RX_L4_CKSUM_GOOD,
	[IONIC_RXQ_COMP_CSUM_F_TCP_BAD] = RTE_MBUF_F_RX_L4_CKSUM_BAD,
	[IONIC_RXQ_COMP_CSUM_F_UDP_OK] = RTE_MBUF_F_RX_L4_CKSUM_GOOD,
	[IONIC_RXQ_COMP_CSUM_F_UDP_BAD] = RTE_MBUF_F_RX_L4_CKSUM_BAD,
};

/* RTE_PTYPE_UNKNOWN is 0x0 */
const uint32_t ionic_ptype_table[IONIC_RXQ_COMP_PKT_TYPE_MASK]
		__rte_cache_aligned = {
	[IONIC_PKT_TYPE_NON_IP]   = RTE_PTYPE_UNKNOWN,
	[IONIC_PKT_TYPE_IPV4]     = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4,
	[IONIC_PKT_TYPE_IPV4_TCP] =
		RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4 | RTE_PTYPE_L4_TCP,
	[IONIC_PKT_TYPE_IPV4_UDP] =
		RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4 | RTE_PTYPE_L4_UDP,
	[IONIC_PKT_TYPE_IPV6]     = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6,
	[IONIC_PKT_TYPE_IPV6_TCP] =
		RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6 | RTE_PTYPE_L4_TCP,
	[IONIC_PKT_TYPE_IPV6_UDP] =
		RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6 | RTE_PTYPE_L4_UDP,
};

const uint32_t *
ionic_dev_supported_ptypes_get(struct rte_eth_dev *dev __rte_unused)
{
	/* See ionic_ptype_table[] */
	static const uint32_t ptypes[] = {
		RTE_PTYPE_L2_ETHER,
		RTE_PTYPE_L2_ETHER_TIMESYNC,
		RTE_PTYPE_L2_ETHER_LLDP,
		RTE_PTYPE_L2_ETHER_ARP,
		RTE_PTYPE_L3_IPV4,
		RTE_PTYPE_L3_IPV6,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_UNKNOWN
	};

	return ptypes;
}

/*
 * Perform one-time initialization of descriptor fields
 * which will not change for the life of the queue.
 */
static void __rte_cold
ionic_rx_init_descriptors(struct ionic_rx_qcq *rxq)
{
	struct ionic_queue *q = &rxq->qcq.q;
	struct ionic_rxq_desc *desc, *desc_base = q->base;
	struct ionic_rxq_sg_desc *sg_desc, *sg_desc_base = q->sg_base;
	uint32_t i, j;
	uint8_t opcode;

	opcode = (q->num_segs > 1) ?
		IONIC_RXQ_DESC_OPCODE_SG : IONIC_RXQ_DESC_OPCODE_SIMPLE;

	/*
	 * NB: Only the first segment needs to leave headroom (hdr_seg_size).
	 *     Later segments (seg_size) do not.
	 */
	for (i = 0; i < q->num_descs; i++) {
		desc = &desc_base[i];
		desc->len = rte_cpu_to_le_16(rxq->hdr_seg_size);
		desc->opcode = opcode;

		sg_desc = &sg_desc_base[i];
		for (j = 0; j < q->num_segs - 1u; j++)
			sg_desc->elems[j].len =
				rte_cpu_to_le_16(rxq->seg_size);
	}
}

/*
 * Start Receive Units for specified queue.
 */
int __rte_cold
ionic_dev_rx_queue_start(struct rte_eth_dev *eth_dev, uint16_t rx_queue_id)
{
	uint8_t *rx_queue_state = eth_dev->data->rx_queue_state;
	struct ionic_rx_qcq *rxq;
	struct ionic_queue *q;
	int err;

	if (rx_queue_state[rx_queue_id] == RTE_ETH_QUEUE_STATE_STARTED) {
		IONIC_PRINT(DEBUG, "RX queue %u already started",
			rx_queue_id);
		return 0;
	}

	rxq = eth_dev->data->rx_queues[rx_queue_id];
	q = &rxq->qcq.q;

	rxq->frame_size = rxq->qcq.lif->frame_size - RTE_ETHER_CRC_LEN;

	/* Recalculate segment count based on MTU */
	q->num_segs = 1 +
		(rxq->frame_size + RTE_PKTMBUF_HEADROOM - 1) / rxq->seg_size;

	IONIC_PRINT(DEBUG, "Starting RX queue %u, %u descs, size %u segs %u",
		rx_queue_id, q->num_descs, rxq->frame_size, q->num_segs);

	ionic_rx_init_descriptors(rxq);

	err = ionic_lif_rxq_init(rxq);
	if (err)
		return err;

	/* Allocate buffers for descriptor ring */
	if (rxq->flags & IONIC_QCQ_F_SG)
		err = ionic_rx_fill_sg(rxq);
	else
		err = ionic_rx_fill(rxq);
	if (err != 0) {
		IONIC_PRINT(ERR, "Could not fill queue %d", rx_queue_id);
		return -1;
	}

	rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STARTED;

	return 0;
}

/*
 * Stop Receive Units for specified queue.
 */
int __rte_cold
ionic_dev_rx_queue_stop(struct rte_eth_dev *eth_dev, uint16_t rx_queue_id)
{
	uint8_t *rx_queue_state = eth_dev->data->rx_queue_state;
	struct ionic_rx_stats *stats;
	struct ionic_rx_qcq *rxq;

	IONIC_PRINT(DEBUG, "Stopping RX queue %u", rx_queue_id);

	rxq = eth_dev->data->rx_queues[rx_queue_id];

	rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;

	ionic_lif_rxq_deinit(rxq);

	/* Free all buffers from descriptor ring */
	ionic_rx_empty(rxq);

	stats = &rxq->stats;
	IONIC_PRINT(DEBUG, "RX queue %u pkts %ju mtod %ju",
		rxq->qcq.q.index, stats->packets, stats->mtods);

	return 0;
}

int
ionic_dev_rx_descriptor_status(void *rx_queue, uint16_t offset)
{
	struct ionic_rx_qcq *rxq = rx_queue;
	struct ionic_qcq *qcq = &rxq->qcq;
	volatile struct ionic_rxq_comp *cq_desc;
	uint16_t mask, head, tail, pos;
	bool done_color;

	mask = qcq->q.size_mask;

	/* offset must be within the size of the ring */
	if (offset > mask)
		return -EINVAL;

	head = qcq->q.head_idx;
	tail = qcq->q.tail_idx;

	/* offset is beyond what is posted */
	if (offset >= ((head - tail) & mask))
		return RTE_ETH_RX_DESC_UNAVAIL;

	/* interested in this absolute position in the rxq */
	pos = (tail + offset) & mask;

	/* rx cq position == rx q position */
	cq_desc = qcq->cq.base;
	cq_desc = &cq_desc[pos];

	/* expected done color at this position */
	done_color = qcq->cq.done_color != (pos < tail);

	/* has the hw indicated the done color at this position? */
	if (color_match(cq_desc->pkt_type_color, done_color))
		return RTE_ETH_RX_DESC_DONE;

	return RTE_ETH_RX_DESC_AVAIL;
}

int
ionic_dev_tx_descriptor_status(void *tx_queue, uint16_t offset)
{
	struct ionic_tx_qcq *txq = tx_queue;
	struct ionic_qcq *qcq = &txq->qcq;
	volatile struct ionic_txq_comp *cq_desc;
	uint16_t mask, head, tail, pos, cq_pos;
	bool done_color;

	mask = qcq->q.size_mask;

	/* offset must be within the size of the ring */
	if (offset > mask)
		return -EINVAL;

	head = qcq->q.head_idx;
	tail = qcq->q.tail_idx;

	/* offset is beyond what is posted */
	if (offset >= ((head - tail) & mask))
		return RTE_ETH_TX_DESC_DONE;

	/* interested in this absolute position in the txq */
	pos = (tail + offset) & mask;

	/* tx cq position != tx q position, need to walk cq */
	cq_pos = qcq->cq.tail_idx;
	cq_desc = qcq->cq.base;
	cq_desc = &cq_desc[cq_pos];

	/* how far behind is pos from head? */
	offset = (head - pos) & mask;

	/* walk cq descriptors that match the expected done color */
	done_color = qcq->cq.done_color;
	while (color_match(cq_desc->color, done_color)) {
		/* is comp index no further behind than pos? */
		tail = rte_cpu_to_le_16(cq_desc->comp_index);
		if (((head - tail) & mask) <= offset)
			return RTE_ETH_TX_DESC_DONE;

		cq_pos = (cq_pos + 1) & mask;
		cq_desc = qcq->cq.base;
		cq_desc = &cq_desc[cq_pos];

		done_color = done_color != (cq_pos == 0);
	}

	return RTE_ETH_TX_DESC_FULL;
}
