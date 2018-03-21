/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2016-2018 Solarflare Communications Inc.
 * All rights reserved.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#include <stdbool.h>

#include <rte_mbuf.h>
#include <rte_io.h>

#include "efx.h"
#include "efx_types.h"
#include "efx_regs.h"
#include "efx_regs_ef10.h"

#include "sfc_dp_tx.h"
#include "sfc_tweak.h"
#include "sfc_kvargs.h"
#include "sfc_ef10.h"

#define sfc_ef10_tx_err(dpq, ...) \
	SFC_DP_LOG(SFC_KVARG_DATAPATH_EF10, ERR, dpq, __VA_ARGS__)

/** Maximum length of the DMA descriptor data */
#define SFC_EF10_TX_DMA_DESC_LEN_MAX \
	((1u << ESF_DZ_TX_KER_BYTE_CNT_WIDTH) - 1)

/**
 * Maximum number of descriptors/buffers in the Tx ring.
 * It should guarantee that corresponding event queue never overfill.
 * EF10 native datapath uses event queue of the same size as Tx queue.
 * Maximum number of events on datapath can be estimated as number of
 * Tx queue entries (one event per Tx buffer in the worst case) plus
 * Tx error and flush events.
 */
#define SFC_EF10_TXQ_LIMIT(_ndesc) \
	((_ndesc) - 1 /* head must not step on tail */ - \
	 (SFC_EF10_EV_PER_CACHE_LINE - 1) /* max unused EvQ entries */ - \
	 1 /* Rx error */ - 1 /* flush */)

struct sfc_ef10_tx_sw_desc {
	struct rte_mbuf			*mbuf;
};

struct sfc_ef10_txq {
	unsigned int			flags;
#define SFC_EF10_TXQ_STARTED		0x1
#define SFC_EF10_TXQ_NOT_RUNNING	0x2
#define SFC_EF10_TXQ_EXCEPTION		0x4

	unsigned int			ptr_mask;
	unsigned int			added;
	unsigned int			completed;
	unsigned int			max_fill_level;
	unsigned int			free_thresh;
	unsigned int			evq_read_ptr;
	struct sfc_ef10_tx_sw_desc	*sw_ring;
	efx_qword_t			*txq_hw_ring;
	volatile void			*doorbell;
	efx_qword_t			*evq_hw_ring;

	/* Datapath transmit queue anchor */
	struct sfc_dp_txq		dp;
};

static inline struct sfc_ef10_txq *
sfc_ef10_txq_by_dp_txq(struct sfc_dp_txq *dp_txq)
{
	return container_of(dp_txq, struct sfc_ef10_txq, dp);
}

static bool
sfc_ef10_tx_get_event(struct sfc_ef10_txq *txq, efx_qword_t *tx_ev)
{
	volatile efx_qword_t *evq_hw_ring = txq->evq_hw_ring;

	/*
	 * Exception flag is set when reap is done.
	 * It is never done twice per packet burst get and absence of
	 * the flag is checked on burst get entry.
	 */
	SFC_ASSERT((txq->flags & SFC_EF10_TXQ_EXCEPTION) == 0);

	*tx_ev = evq_hw_ring[txq->evq_read_ptr & txq->ptr_mask];

	if (!sfc_ef10_ev_present(*tx_ev))
		return false;

	if (unlikely(EFX_QWORD_FIELD(*tx_ev, FSF_AZ_EV_CODE) !=
		     FSE_AZ_EV_CODE_TX_EV)) {
		/*
		 * Do not move read_ptr to keep the event for exception
		 * handling by the control path.
		 */
		txq->flags |= SFC_EF10_TXQ_EXCEPTION;
		sfc_ef10_tx_err(&txq->dp.dpq,
				"TxQ exception at EvQ read ptr %#x",
				txq->evq_read_ptr);
		return false;
	}

	txq->evq_read_ptr++;
	return true;
}

static unsigned int
sfc_ef10_tx_process_events(struct sfc_ef10_txq *txq)
{
	const unsigned int curr_done = txq->completed - 1;
	unsigned int anew_done = curr_done;
	efx_qword_t tx_ev;

	while (sfc_ef10_tx_get_event(txq, &tx_ev)) {
		/*
		 * DROP_EVENT is an internal to the NIC, software should
		 * never see it and, therefore, may ignore it.
		 */

		/* Update the latest done descriptor */
		anew_done = EFX_QWORD_FIELD(tx_ev, ESF_DZ_TX_DESCR_INDX);
	}
	return (anew_done - curr_done) & txq->ptr_mask;
}

static void
sfc_ef10_tx_reap(struct sfc_ef10_txq *txq)
{
	const unsigned int old_read_ptr = txq->evq_read_ptr;
	const unsigned int ptr_mask = txq->ptr_mask;
	unsigned int completed = txq->completed;
	unsigned int pending = completed;

	pending += sfc_ef10_tx_process_events(txq);

	if (pending != completed) {
		struct rte_mbuf *bulk[SFC_TX_REAP_BULK_SIZE];
		unsigned int nb = 0;

		do {
			struct sfc_ef10_tx_sw_desc *txd;
			struct rte_mbuf *m;

			txd = &txq->sw_ring[completed & ptr_mask];
			if (txd->mbuf == NULL)
				continue;

			m = rte_pktmbuf_prefree_seg(txd->mbuf);
			txd->mbuf = NULL;
			if (m == NULL)
				continue;

			if ((nb == RTE_DIM(bulk)) ||
			    ((nb != 0) && (m->pool != bulk[0]->pool))) {
				rte_mempool_put_bulk(bulk[0]->pool,
						     (void *)bulk, nb);
				nb = 0;
			}

			bulk[nb++] = m;
		} while (++completed != pending);

		if (nb != 0)
			rte_mempool_put_bulk(bulk[0]->pool, (void *)bulk, nb);

		txq->completed = completed;
	}

	sfc_ef10_ev_qclear(txq->evq_hw_ring, ptr_mask, old_read_ptr,
			   txq->evq_read_ptr);
}

static void
sfc_ef10_tx_qdesc_dma_create(rte_iova_t addr, uint16_t size, bool eop,
			     efx_qword_t *edp)
{
	EFX_POPULATE_QWORD_4(*edp,
			     ESF_DZ_TX_KER_TYPE, 0,
			     ESF_DZ_TX_KER_CONT, !eop,
			     ESF_DZ_TX_KER_BYTE_CNT, size,
			     ESF_DZ_TX_KER_BUF_ADDR, addr);
}

static inline void
sfc_ef10_tx_qpush(struct sfc_ef10_txq *txq, unsigned int added,
		  unsigned int pushed)
{
	efx_qword_t desc;
	efx_oword_t oword;

	/*
	 * This improves performance by pushing a TX descriptor at the same
	 * time as the doorbell. The descriptor must be added to the TXQ,
	 * so that can be used if the hardware decides not to use the pushed
	 * descriptor.
	 */
	desc.eq_u64[0] = txq->txq_hw_ring[pushed & txq->ptr_mask].eq_u64[0];
	EFX_POPULATE_OWORD_3(oword,
		ERF_DZ_TX_DESC_WPTR, added & txq->ptr_mask,
		ERF_DZ_TX_DESC_HWORD, EFX_QWORD_FIELD(desc, EFX_DWORD_1),
		ERF_DZ_TX_DESC_LWORD, EFX_QWORD_FIELD(desc, EFX_DWORD_0));

	/* DMA sync to device is not required */

	/*
	 * rte_io_wmb() which guarantees that the STORE operations
	 * (i.e. Tx and event descriptor updates) that precede
	 * the rte_io_wmb() call are visible to NIC before the STORE
	 * operations that follow it (i.e. doorbell write).
	 */
	rte_io_wmb();

	*(volatile __m128i *)txq->doorbell = oword.eo_u128[0];
}

static unsigned int
sfc_ef10_tx_pkt_descs_max(const struct rte_mbuf *m)
{
	unsigned int extra_descs_per_seg;
	unsigned int extra_descs_per_pkt;

	/*
	 * VLAN offload is not supported yet, so no extra descriptors
	 * are required for VLAN option descriptor.
	 */

/** Maximum length of the mbuf segment data */
#define SFC_MBUF_SEG_LEN_MAX		UINT16_MAX
	RTE_BUILD_BUG_ON(sizeof(m->data_len) != 2);

	/*
	 * Each segment is already counted once below.  So, calculate
	 * how many extra DMA descriptors may be required per segment in
	 * the worst case because of maximum DMA descriptor length limit.
	 * If maximum segment length is less or equal to maximum DMA
	 * descriptor length, no extra DMA descriptors are required.
	 */
	extra_descs_per_seg =
		(SFC_MBUF_SEG_LEN_MAX - 1) / SFC_EF10_TX_DMA_DESC_LEN_MAX;

/** Maximum length of the packet */
#define SFC_MBUF_PKT_LEN_MAX		UINT32_MAX
	RTE_BUILD_BUG_ON(sizeof(m->pkt_len) != 4);

	/*
	 * One more limitation on maximum number of extra DMA descriptors
	 * comes from slicing entire packet because of DMA descriptor length
	 * limit taking into account that there is at least one segment
	 * which is already counted below (so division of the maximum
	 * packet length minus one with round down).
	 * TSO is not supported yet, so packet length is limited by
	 * maximum PDU size.
	 */
	extra_descs_per_pkt =
		(RTE_MIN((unsigned int)EFX_MAC_PDU_MAX,
			 SFC_MBUF_PKT_LEN_MAX) - 1) /
		SFC_EF10_TX_DMA_DESC_LEN_MAX;

	return m->nb_segs + RTE_MIN(m->nb_segs * extra_descs_per_seg,
				    extra_descs_per_pkt);
}

static uint16_t
sfc_ef10_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct sfc_ef10_txq * const txq = sfc_ef10_txq_by_dp_txq(tx_queue);
	unsigned int added;
	unsigned int dma_desc_space;
	bool reap_done;
	struct rte_mbuf **pktp;
	struct rte_mbuf **pktp_end;

	if (unlikely(txq->flags &
		     (SFC_EF10_TXQ_NOT_RUNNING | SFC_EF10_TXQ_EXCEPTION)))
		return 0;

	added = txq->added;
	dma_desc_space = txq->max_fill_level - (added - txq->completed);

	reap_done = (dma_desc_space < txq->free_thresh);
	if (reap_done) {
		sfc_ef10_tx_reap(txq);
		dma_desc_space = txq->max_fill_level - (added - txq->completed);
	}

	for (pktp = &tx_pkts[0], pktp_end = &tx_pkts[nb_pkts];
	     pktp != pktp_end;
	     ++pktp) {
		struct rte_mbuf *m_seg = *pktp;
		unsigned int pkt_start = added;
		uint32_t pkt_len;

		if (likely(pktp + 1 != pktp_end))
			rte_mbuf_prefetch_part1(pktp[1]);

		if (sfc_ef10_tx_pkt_descs_max(m_seg) > dma_desc_space) {
			if (reap_done)
				break;

			/* Push already prepared descriptors before polling */
			if (added != txq->added) {
				sfc_ef10_tx_qpush(txq, added, txq->added);
				txq->added = added;
			}

			sfc_ef10_tx_reap(txq);
			reap_done = true;
			dma_desc_space = txq->max_fill_level -
				(added - txq->completed);
			if (sfc_ef10_tx_pkt_descs_max(m_seg) > dma_desc_space)
				break;
		}

		pkt_len = m_seg->pkt_len;
		do {
			rte_iova_t seg_addr = rte_mbuf_data_iova(m_seg);
			unsigned int seg_len = rte_pktmbuf_data_len(m_seg);
			unsigned int id = added & txq->ptr_mask;

			SFC_ASSERT(seg_len <= SFC_EF10_TX_DMA_DESC_LEN_MAX);

			pkt_len -= seg_len;

			sfc_ef10_tx_qdesc_dma_create(seg_addr,
				seg_len, (pkt_len == 0),
				&txq->txq_hw_ring[id]);

			/*
			 * rte_pktmbuf_free() is commonly used in DPDK for
			 * recycling packets - the function checks every
			 * segment's reference counter and returns the
			 * buffer to its pool whenever possible;
			 * nevertheless, freeing mbuf segments one by one
			 * may entail some performance decline;
			 * from this point, sfc_efx_tx_reap() does the same job
			 * on its own and frees buffers in bulks (all mbufs
			 * within a bulk belong to the same pool);
			 * from this perspective, individual segment pointers
			 * must be associated with the corresponding SW
			 * descriptors independently so that only one loop
			 * is sufficient on reap to inspect all the buffers
			 */
			txq->sw_ring[id].mbuf = m_seg;

			++added;

		} while ((m_seg = m_seg->next) != 0);

		dma_desc_space -= (added - pkt_start);
	}

	if (likely(added != txq->added)) {
		sfc_ef10_tx_qpush(txq, added, txq->added);
		txq->added = added;
	}

#if SFC_TX_XMIT_PKTS_REAP_AT_LEAST_ONCE
	if (!reap_done)
		sfc_ef10_tx_reap(txq);
#endif

	return pktp - &tx_pkts[0];
}

static void
sfc_ef10_simple_tx_reap(struct sfc_ef10_txq *txq)
{
	const unsigned int old_read_ptr = txq->evq_read_ptr;
	const unsigned int ptr_mask = txq->ptr_mask;
	unsigned int completed = txq->completed;
	unsigned int pending = completed;

	pending += sfc_ef10_tx_process_events(txq);

	if (pending != completed) {
		struct rte_mbuf *bulk[SFC_TX_REAP_BULK_SIZE];
		unsigned int nb = 0;

		do {
			struct sfc_ef10_tx_sw_desc *txd;

			txd = &txq->sw_ring[completed & ptr_mask];

			if (nb == RTE_DIM(bulk)) {
				rte_mempool_put_bulk(bulk[0]->pool,
						     (void *)bulk, nb);
				nb = 0;
			}

			bulk[nb++] = txd->mbuf;
		} while (++completed != pending);

		rte_mempool_put_bulk(bulk[0]->pool, (void *)bulk, nb);

		txq->completed = completed;
	}

	sfc_ef10_ev_qclear(txq->evq_hw_ring, ptr_mask, old_read_ptr,
			   txq->evq_read_ptr);
}


static uint16_t
sfc_ef10_simple_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
			  uint16_t nb_pkts)
{
	struct sfc_ef10_txq * const txq = sfc_ef10_txq_by_dp_txq(tx_queue);
	unsigned int ptr_mask;
	unsigned int added;
	unsigned int dma_desc_space;
	bool reap_done;
	struct rte_mbuf **pktp;
	struct rte_mbuf **pktp_end;

	if (unlikely(txq->flags &
		     (SFC_EF10_TXQ_NOT_RUNNING | SFC_EF10_TXQ_EXCEPTION)))
		return 0;

	ptr_mask = txq->ptr_mask;
	added = txq->added;
	dma_desc_space = txq->max_fill_level - (added - txq->completed);

	reap_done = (dma_desc_space < RTE_MAX(txq->free_thresh, nb_pkts));
	if (reap_done) {
		sfc_ef10_simple_tx_reap(txq);
		dma_desc_space = txq->max_fill_level - (added - txq->completed);
	}

	pktp_end = &tx_pkts[MIN(nb_pkts, dma_desc_space)];
	for (pktp = &tx_pkts[0]; pktp != pktp_end; ++pktp) {
		struct rte_mbuf *pkt = *pktp;
		unsigned int id = added & ptr_mask;

		SFC_ASSERT(rte_pktmbuf_data_len(pkt) <=
			   SFC_EF10_TX_DMA_DESC_LEN_MAX);

		sfc_ef10_tx_qdesc_dma_create(rte_mbuf_data_iova(pkt),
					     rte_pktmbuf_data_len(pkt),
					     true, &txq->txq_hw_ring[id]);

		txq->sw_ring[id].mbuf = pkt;

		++added;
	}

	if (likely(added != txq->added)) {
		sfc_ef10_tx_qpush(txq, added, txq->added);
		txq->added = added;
	}

#if SFC_TX_XMIT_PKTS_REAP_AT_LEAST_ONCE
	if (!reap_done)
		sfc_ef10_simple_tx_reap(txq);
#endif

	return pktp - &tx_pkts[0];
}

static sfc_dp_tx_get_dev_info_t sfc_ef10_get_dev_info;
static void
sfc_ef10_get_dev_info(struct rte_eth_dev_info *dev_info)
{
	/*
	 * Number of descriptors just defines maximum number of pushed
	 * descriptors (fill level).
	 */
	dev_info->tx_desc_lim.nb_min = 1;
	dev_info->tx_desc_lim.nb_align = 1;
}

static sfc_dp_tx_qsize_up_rings_t sfc_ef10_tx_qsize_up_rings;
static int
sfc_ef10_tx_qsize_up_rings(uint16_t nb_tx_desc,
			   unsigned int *txq_entries,
			   unsigned int *evq_entries,
			   unsigned int *txq_max_fill_level)
{
	/*
	 * rte_ethdev API guarantees that the number meets min, max and
	 * alignment requirements.
	 */
	if (nb_tx_desc <= EFX_TXQ_MINNDESCS)
		*txq_entries = EFX_TXQ_MINNDESCS;
	else
		*txq_entries = rte_align32pow2(nb_tx_desc);

	*evq_entries = *txq_entries;

	*txq_max_fill_level = RTE_MIN(nb_tx_desc,
				      SFC_EF10_TXQ_LIMIT(*evq_entries));
	return 0;
}

static sfc_dp_tx_qcreate_t sfc_ef10_tx_qcreate;
static int
sfc_ef10_tx_qcreate(uint16_t port_id, uint16_t queue_id,
		    const struct rte_pci_addr *pci_addr, int socket_id,
		    const struct sfc_dp_tx_qcreate_info *info,
		    struct sfc_dp_txq **dp_txqp)
{
	struct sfc_ef10_txq *txq;
	int rc;

	rc = EINVAL;
	if (info->txq_entries != info->evq_entries)
		goto fail_bad_args;

	rc = ENOMEM;
	txq = rte_zmalloc_socket("sfc-ef10-txq", sizeof(*txq),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (txq == NULL)
		goto fail_txq_alloc;

	sfc_dp_queue_init(&txq->dp.dpq, port_id, queue_id, pci_addr);

	rc = ENOMEM;
	txq->sw_ring = rte_calloc_socket("sfc-ef10-txq-sw_ring",
					 info->txq_entries,
					 sizeof(*txq->sw_ring),
					 RTE_CACHE_LINE_SIZE, socket_id);
	if (txq->sw_ring == NULL)
		goto fail_sw_ring_alloc;

	txq->flags = SFC_EF10_TXQ_NOT_RUNNING;
	txq->ptr_mask = info->txq_entries - 1;
	txq->max_fill_level = info->max_fill_level;
	txq->free_thresh = info->free_thresh;
	txq->txq_hw_ring = info->txq_hw_ring;
	txq->doorbell = (volatile uint8_t *)info->mem_bar +
			ER_DZ_TX_DESC_UPD_REG_OFST +
			(info->hw_index << info->vi_window_shift);
	txq->evq_hw_ring = info->evq_hw_ring;

	*dp_txqp = &txq->dp;
	return 0;

fail_sw_ring_alloc:
	rte_free(txq);

fail_txq_alloc:
fail_bad_args:
	return rc;
}

static sfc_dp_tx_qdestroy_t sfc_ef10_tx_qdestroy;
static void
sfc_ef10_tx_qdestroy(struct sfc_dp_txq *dp_txq)
{
	struct sfc_ef10_txq *txq = sfc_ef10_txq_by_dp_txq(dp_txq);

	rte_free(txq->sw_ring);
	rte_free(txq);
}

static sfc_dp_tx_qstart_t sfc_ef10_tx_qstart;
static int
sfc_ef10_tx_qstart(struct sfc_dp_txq *dp_txq, unsigned int evq_read_ptr,
		   unsigned int txq_desc_index)
{
	struct sfc_ef10_txq *txq = sfc_ef10_txq_by_dp_txq(dp_txq);

	txq->evq_read_ptr = evq_read_ptr;
	txq->added = txq->completed = txq_desc_index;

	txq->flags |= SFC_EF10_TXQ_STARTED;
	txq->flags &= ~(SFC_EF10_TXQ_NOT_RUNNING | SFC_EF10_TXQ_EXCEPTION);

	return 0;
}

static sfc_dp_tx_qstop_t sfc_ef10_tx_qstop;
static void
sfc_ef10_tx_qstop(struct sfc_dp_txq *dp_txq, unsigned int *evq_read_ptr)
{
	struct sfc_ef10_txq *txq = sfc_ef10_txq_by_dp_txq(dp_txq);

	txq->flags |= SFC_EF10_TXQ_NOT_RUNNING;

	*evq_read_ptr = txq->evq_read_ptr;
}

static sfc_dp_tx_qtx_ev_t sfc_ef10_tx_qtx_ev;
static bool
sfc_ef10_tx_qtx_ev(struct sfc_dp_txq *dp_txq, __rte_unused unsigned int id)
{
	__rte_unused struct sfc_ef10_txq *txq = sfc_ef10_txq_by_dp_txq(dp_txq);

	SFC_ASSERT(txq->flags & SFC_EF10_TXQ_NOT_RUNNING);

	/*
	 * It is safe to ignore Tx event since we reap all mbufs on
	 * queue purge anyway.
	 */

	return false;
}

static sfc_dp_tx_qreap_t sfc_ef10_tx_qreap;
static void
sfc_ef10_tx_qreap(struct sfc_dp_txq *dp_txq)
{
	struct sfc_ef10_txq *txq = sfc_ef10_txq_by_dp_txq(dp_txq);
	unsigned int completed;

	for (completed = txq->completed; completed != txq->added; ++completed) {
		struct sfc_ef10_tx_sw_desc *txd;

		txd = &txq->sw_ring[completed & txq->ptr_mask];
		if (txd->mbuf != NULL) {
			rte_pktmbuf_free_seg(txd->mbuf);
			txd->mbuf = NULL;
		}
	}

	txq->flags &= ~SFC_EF10_TXQ_STARTED;
}

static sfc_dp_tx_qdesc_status_t sfc_ef10_tx_qdesc_status;
static int
sfc_ef10_tx_qdesc_status(__rte_unused struct sfc_dp_txq *dp_txq,
			 __rte_unused uint16_t offset)
{
	return -ENOTSUP;
}

struct sfc_dp_tx sfc_ef10_tx = {
	.dp = {
		.name		= SFC_KVARG_DATAPATH_EF10,
		.type		= SFC_DP_TX,
		.hw_fw_caps	= SFC_DP_HW_FW_CAP_EF10,
	},
	.features		= SFC_DP_TX_FEAT_MULTI_SEG |
				  SFC_DP_TX_FEAT_MULTI_POOL |
				  SFC_DP_TX_FEAT_REFCNT |
				  SFC_DP_TX_FEAT_MULTI_PROCESS,
	.get_dev_info		= sfc_ef10_get_dev_info,
	.qsize_up_rings		= sfc_ef10_tx_qsize_up_rings,
	.qcreate		= sfc_ef10_tx_qcreate,
	.qdestroy		= sfc_ef10_tx_qdestroy,
	.qstart			= sfc_ef10_tx_qstart,
	.qtx_ev			= sfc_ef10_tx_qtx_ev,
	.qstop			= sfc_ef10_tx_qstop,
	.qreap			= sfc_ef10_tx_qreap,
	.qdesc_status		= sfc_ef10_tx_qdesc_status,
	.pkt_burst		= sfc_ef10_xmit_pkts,
};

struct sfc_dp_tx sfc_ef10_simple_tx = {
	.dp = {
		.name		= SFC_KVARG_DATAPATH_EF10_SIMPLE,
		.type		= SFC_DP_TX,
	},
	.features		= SFC_DP_TX_FEAT_MULTI_PROCESS,
	.get_dev_info		= sfc_ef10_get_dev_info,
	.qsize_up_rings		= sfc_ef10_tx_qsize_up_rings,
	.qcreate		= sfc_ef10_tx_qcreate,
	.qdestroy		= sfc_ef10_tx_qdestroy,
	.qstart			= sfc_ef10_tx_qstart,
	.qtx_ev			= sfc_ef10_tx_qtx_ev,
	.qstop			= sfc_ef10_tx_qstop,
	.qreap			= sfc_ef10_tx_qreap,
	.qdesc_status		= sfc_ef10_tx_qdesc_status,
	.pkt_burst		= sfc_ef10_simple_xmit_pkts,
};
