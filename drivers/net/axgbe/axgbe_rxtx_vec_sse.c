/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright(c) 2018 Advanced Micro Devices, Inc. All rights reserved.
 *   Copyright(c) 2018 Synopsys, Inc. All rights reserved.
 */

#include "axgbe_ethdev.h"
#include "axgbe_rxtx.h"
#include "axgbe_phy.h"

#include <rte_time.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>

/* Useful to avoid shifting for every descriptor preparation */
#define TX_DESC_CTRL_FLAGS 0xb000000000000000
#define TX_DESC_CTRL_FLAG_TMST 0x40000000
#define TX_FREE_BULK	   8
#define TX_FREE_BULK_CHECK (TX_FREE_BULK - 1)

static inline void
axgbe_vec_tx(volatile struct axgbe_tx_desc *desc,
	     struct rte_mbuf *mbuf)
{
	uint64_t tmst_en = 0;
	/* Timestamp enablement check */
	if (mbuf->ol_flags & RTE_MBUF_F_TX_IEEE1588_TMST)
		tmst_en = TX_DESC_CTRL_FLAG_TMST;
	__m128i descriptor = _mm_set_epi64x((uint64_t)mbuf->pkt_len << 32 |
					    TX_DESC_CTRL_FLAGS | mbuf->data_len
					    | tmst_en,
					    mbuf->buf_iova
					    + mbuf->data_off);
	_mm_store_si128((__m128i *)desc, descriptor);
}

static void
axgbe_xmit_cleanup_vec(struct axgbe_tx_queue *txq)
{
	volatile struct axgbe_tx_desc *desc;
	int idx, i;

	idx = AXGBE_GET_DESC_IDX(txq, txq->dirty + txq->free_batch_cnt
				 - 1);
	desc = &txq->desc[idx];
	if (desc->desc3 & AXGBE_DESC_OWN)
		return;
	/* memset avoided for desc ctrl fields since in vec_tx path
	 * all 128 bits are populated
	 */
	for (i = 0; i < txq->free_batch_cnt; i++, idx--)
		rte_pktmbuf_free_seg(txq->sw_ring[idx]);


	txq->dirty += txq->free_batch_cnt;
	txq->nb_desc_free += txq->free_batch_cnt;
}

uint16_t
axgbe_xmit_pkts_vec(void *tx_queue, struct rte_mbuf **tx_pkts,
		    uint16_t nb_pkts)
{
	PMD_INIT_FUNC_TRACE();

	struct axgbe_tx_queue *txq;
	uint16_t idx, nb_commit, loop, i;
	uint32_t tail_addr;

	txq  = (struct axgbe_tx_queue *)tx_queue;
	if (txq->nb_desc_free < txq->free_thresh) {
		axgbe_xmit_cleanup_vec(txq);
		if (unlikely(txq->nb_desc_free == 0))
			return 0;
	}
	nb_pkts = RTE_MIN(txq->nb_desc_free, nb_pkts);
	nb_commit = nb_pkts;
	idx = AXGBE_GET_DESC_IDX(txq, txq->cur);
	loop = txq->nb_desc - idx;
	if (nb_commit >= loop) {
		for (i = 0; i < loop; ++i, ++idx, ++tx_pkts) {
			axgbe_vec_tx(&txq->desc[idx], *tx_pkts);
			txq->sw_ring[idx] = *tx_pkts;
		}
		nb_commit -= loop;
		idx = 0;
	}
	for (i = 0; i < nb_commit; ++i, ++idx, ++tx_pkts) {
		axgbe_vec_tx(&txq->desc[idx], *tx_pkts);
		txq->sw_ring[idx] = *tx_pkts;
	}
	txq->cur += nb_pkts;
	tail_addr = (uint32_t)(txq->ring_phys_addr +
			       idx * sizeof(struct axgbe_tx_desc));
	/* Update tail reg with next immediate address to kick Tx DMA channel*/
	rte_write32(tail_addr, (void *)txq->dma_tail_reg);
	txq->pkts += nb_pkts;
	txq->nb_desc_free -= nb_pkts;

	return nb_pkts;
}
