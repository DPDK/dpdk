/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Broadcom
 * All rights reserved.
 */

#ifndef _BNXT_RXTX_VEC_COMMON_H_
#define _BNXT_RXTX_VEC_COMMON_H_
#include "hsi_struct_def_dpdk.h"
#include "bnxt_rxq.h"
#include "bnxt_rxr.h"

#define TX_BD_FLAGS_CMPL ((1 << TX_BD_LONG_FLAGS_BD_CNT_SFT) | \
			  TX_BD_SHORT_FLAGS_COAL_NOW | \
			  TX_BD_SHORT_TYPE_TX_BD_SHORT | \
			  TX_BD_LONG_FLAGS_PACKET_END)

#define TX_BD_FLAGS_NOCMPL (TX_BD_FLAGS_CMPL | TX_BD_LONG_FLAGS_NO_CMPL)

static inline uint32_t
bnxt_xmit_flags_len(uint16_t len, uint16_t flags)
{
	switch (len >> 9) {
	case 0:
		return flags | TX_BD_LONG_FLAGS_LHINT_LT512;
	case 1:
		return flags | TX_BD_LONG_FLAGS_LHINT_LT1K;
	case 2:
		return flags | TX_BD_LONG_FLAGS_LHINT_LT2K;
	case 3:
		return flags | TX_BD_LONG_FLAGS_LHINT_LT2K;
	default:
		return flags | TX_BD_LONG_FLAGS_LHINT_GTE2K;
	}
}

static inline int
bnxt_rxq_vec_setup_common(struct bnxt_rx_queue *rxq)
{
	uintptr_t p;
	struct rte_mbuf mb_def = { .buf_addr = 0 }; /* zeroed mbuf */

	mb_def.nb_segs = 1;
	mb_def.data_off = RTE_PKTMBUF_HEADROOM;
	mb_def.port = rxq->port_id;
	rte_mbuf_refcnt_set(&mb_def, 1);

	/* prevent compiler reordering: rearm_data covers previous fields */
	rte_compiler_barrier();
	p = (uintptr_t)&mb_def.rearm_data;
	rxq->mbuf_initializer = *(uint64_t *)p;
	rxq->rxrearm_nb = 0;
	rxq->rxrearm_start = 0;
	return 0;
}

static inline void
bnxt_rxq_rearm(struct bnxt_rx_queue *rxq, struct bnxt_rx_ring_info *rxr)
{
	struct rx_prod_pkt_bd *rxbds = &rxr->rx_desc_ring[rxq->rxrearm_start];
	struct rte_mbuf **rx_bufs = &rxr->rx_buf_ring[rxq->rxrearm_start];
	int nb, i;

	/*
	 * Number of mbufs to allocate must be a multiple of four. The
	 * allocation must not go past the end of the ring.
	 */
	nb = RTE_MIN(rxq->rxrearm_nb & ~0x3,
		     rxq->nb_rx_desc - rxq->rxrearm_start);

	/* Allocate new mbufs into the software ring. */
	if (rte_mempool_get_bulk(rxq->mb_pool, (void *)rx_bufs, nb) < 0) {
		rte_eth_devices[rxq->port_id].data->rx_mbuf_alloc_failed += nb;

		for (i = 0; i < nb; i++)
			rx_bufs[i] = &rxq->fake_mbuf;
		return;
	}

	/* Initialize the mbufs in vector, process 4 mbufs per loop. */
	for (i = 0; i < nb; i += 4) {
		rxbds[0].address = rte_mbuf_data_iova_default(rx_bufs[0]);
		rxbds[1].address = rte_mbuf_data_iova_default(rx_bufs[1]);
		rxbds[2].address = rte_mbuf_data_iova_default(rx_bufs[2]);
		rxbds[3].address = rte_mbuf_data_iova_default(rx_bufs[3]);

		rxbds += 4;
		rx_bufs += 4;
	}

	rxq->rxrearm_start += nb;
	bnxt_db_write(&rxr->rx_db, rxq->rxrearm_start - 1);
	if (rxq->rxrearm_start >= rxq->nb_rx_desc)
		rxq->rxrearm_start = 0;

	rxq->rxrearm_nb -= nb;
}

/*
 * Transmit completion function for use when DEV_TX_OFFLOAD_MBUF_FAST_FREE
 * is enabled.
 */
static inline void
bnxt_tx_cmp_vec_fast(struct bnxt_tx_queue *txq, int nr_pkts)
{
	struct bnxt_tx_ring_info *txr = txq->tx_ring;
	uint32_t ring_mask = txr->tx_ring_struct->ring_mask;
	struct rte_mbuf **free = txq->free;
	uint16_t cons = txr->tx_cons;
	unsigned int blk = 0;

	while (nr_pkts--) {
		struct bnxt_sw_tx_bd *tx_buf;

		tx_buf = &txr->tx_buf_ring[cons];
		cons = (cons + 1) & ring_mask;
		free[blk++] = tx_buf->mbuf;
		tx_buf->mbuf = NULL;
	}
	if (blk)
		rte_mempool_put_bulk(free[0]->pool, (void **)free, blk);

	txr->tx_cons = cons;
}

static inline void
bnxt_tx_cmp_vec(struct bnxt_tx_queue *txq, int nr_pkts)
{
	struct bnxt_tx_ring_info *txr = txq->tx_ring;
	struct rte_mbuf **free = txq->free;
	uint16_t cons = txr->tx_cons;
	unsigned int blk = 0;
	uint32_t ring_mask = txr->tx_ring_struct->ring_mask;

	while (nr_pkts--) {
		struct bnxt_sw_tx_bd *tx_buf;
		struct rte_mbuf *mbuf;

		tx_buf = &txr->tx_buf_ring[cons];
		cons = (cons + 1) & ring_mask;
		mbuf = rte_pktmbuf_prefree_seg(tx_buf->mbuf);
		if (unlikely(mbuf == NULL))
			continue;
		tx_buf->mbuf = NULL;

		if (blk && mbuf->pool != free[0]->pool) {
			rte_mempool_put_bulk(free[0]->pool, (void **)free, blk);
			blk = 0;
		}
		free[blk++] = mbuf;
	}
	if (blk)
		rte_mempool_put_bulk(free[0]->pool, (void **)free, blk);

	txr->tx_cons = cons;
}
#endif /* _BNXT_RXTX_VEC_COMMON_H_ */
