/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2020 Xilinx, Inc.
 * Copyright(c) 2018-2019 Solarflare Communications Inc.
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
#include "efx_regs_ef100.h"

#include "sfc_debug.h"
#include "sfc_dp_tx.h"
#include "sfc_tweak.h"
#include "sfc_kvargs.h"
#include "sfc_ef100.h"


#define sfc_ef100_tx_err(_txq, ...) \
	SFC_DP_LOG(SFC_KVARG_DATAPATH_EF100, ERR, &(_txq)->dp.dpq, __VA_ARGS__)

#define sfc_ef100_tx_debug(_txq, ...) \
	SFC_DP_LOG(SFC_KVARG_DATAPATH_EF100, DEBUG, &(_txq)->dp.dpq, \
		   __VA_ARGS__)


/** Maximum length of the send descriptor data */
#define SFC_EF100_TX_SEND_DESC_LEN_MAX \
	((1u << ESF_GZ_TX_SEND_LEN_WIDTH) - 1)

/** Maximum length of the segment descriptor data */
#define SFC_EF100_TX_SEG_DESC_LEN_MAX \
	((1u << ESF_GZ_TX_SEG_LEN_WIDTH) - 1)

/**
 * Maximum number of descriptors/buffers in the Tx ring.
 * It should guarantee that corresponding event queue never overfill.
 * EF100 native datapath uses event queue of the same size as Tx queue.
 * Maximum number of events on datapath can be estimated as number of
 * Tx queue entries (one event per Tx buffer in the worst case) plus
 * Tx error and flush events.
 */
#define SFC_EF100_TXQ_LIMIT(_ndesc) \
	((_ndesc) - 1 /* head must not step on tail */ - \
	 1 /* Rx error */ - 1 /* flush */)

struct sfc_ef100_tx_sw_desc {
	struct rte_mbuf			*mbuf;
};

struct sfc_ef100_txq {
	unsigned int			flags;
#define SFC_EF100_TXQ_STARTED		0x1
#define SFC_EF100_TXQ_NOT_RUNNING	0x2
#define SFC_EF100_TXQ_EXCEPTION		0x4

	unsigned int			ptr_mask;
	unsigned int			added;
	unsigned int			completed;
	unsigned int			max_fill_level;
	unsigned int			free_thresh;
	struct sfc_ef100_tx_sw_desc	*sw_ring;
	efx_oword_t			*txq_hw_ring;
	volatile void			*doorbell;

	/* Completion/reap */
	unsigned int			evq_read_ptr;
	unsigned int			evq_phase_bit_shift;
	volatile efx_qword_t		*evq_hw_ring;

	/* Datapath transmit queue anchor */
	struct sfc_dp_txq		dp;
};

static inline struct sfc_ef100_txq *
sfc_ef100_txq_by_dp_txq(struct sfc_dp_txq *dp_txq)
{
	return container_of(dp_txq, struct sfc_ef100_txq, dp);
}

static uint16_t
sfc_ef100_tx_prepare_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
			  uint16_t nb_pkts)
{
	struct sfc_ef100_txq * const txq = sfc_ef100_txq_by_dp_txq(tx_queue);
	uint16_t i;

	for (i = 0; i < nb_pkts; i++) {
		struct rte_mbuf *m = tx_pkts[i];
		int ret;

		ret = sfc_dp_tx_prepare_pkt(m, 0, txq->max_fill_level, 0, 0);
		if (unlikely(ret != 0)) {
			rte_errno = ret;
			break;
		}

		if (m->nb_segs > EFX_MASK32(ESF_GZ_TX_SEND_NUM_SEGS)) {
			rte_errno = EINVAL;
			break;
		}
	}

	return i;
}

static bool
sfc_ef100_tx_get_event(struct sfc_ef100_txq *txq, efx_qword_t *ev)
{
	volatile efx_qword_t *evq_hw_ring = txq->evq_hw_ring;

	/*
	 * Exception flag is set when reap is done.
	 * It is never done twice per packet burst get, and absence of
	 * the flag is checked on burst get entry.
	 */
	SFC_ASSERT((txq->flags & SFC_EF100_TXQ_EXCEPTION) == 0);

	*ev = evq_hw_ring[txq->evq_read_ptr & txq->ptr_mask];

	if (!sfc_ef100_ev_present(ev,
			(txq->evq_read_ptr >> txq->evq_phase_bit_shift) & 1))
		return false;

	if (unlikely(!sfc_ef100_ev_type_is(ev,
					   ESE_GZ_EF100_EV_TX_COMPLETION))) {
		/*
		 * Do not move read_ptr to keep the event for exception
		 * handling by the control path.
		 */
		txq->flags |= SFC_EF100_TXQ_EXCEPTION;
		sfc_ef100_tx_err(txq,
			"TxQ exception at EvQ ptr %u(%#x), event %08x:%08x",
			txq->evq_read_ptr, txq->evq_read_ptr & txq->ptr_mask,
			EFX_QWORD_FIELD(*ev, EFX_DWORD_1),
			EFX_QWORD_FIELD(*ev, EFX_DWORD_0));
		return false;
	}

	sfc_ef100_tx_debug(txq, "TxQ got event %08x:%08x at %u (%#x)",
			   EFX_QWORD_FIELD(*ev, EFX_DWORD_1),
			   EFX_QWORD_FIELD(*ev, EFX_DWORD_0),
			   txq->evq_read_ptr,
			   txq->evq_read_ptr & txq->ptr_mask);

	txq->evq_read_ptr++;
	return true;
}

static unsigned int
sfc_ef100_tx_process_events(struct sfc_ef100_txq *txq)
{
	unsigned int num_descs = 0;
	efx_qword_t tx_ev;

	while (sfc_ef100_tx_get_event(txq, &tx_ev))
		num_descs += EFX_QWORD_FIELD(tx_ev, ESF_GZ_EV_TXCMPL_NUM_DESC);

	return num_descs;
}

static void
sfc_ef100_tx_reap_num_descs(struct sfc_ef100_txq *txq, unsigned int num_descs)
{
	if (num_descs > 0) {
		unsigned int completed = txq->completed;
		unsigned int pending = completed + num_descs;
		struct rte_mbuf *bulk[SFC_TX_REAP_BULK_SIZE];
		unsigned int nb = 0;

		do {
			struct sfc_ef100_tx_sw_desc *txd;
			struct rte_mbuf *m;

			txd = &txq->sw_ring[completed & txq->ptr_mask];
			if (txd->mbuf == NULL)
				continue;

			m = rte_pktmbuf_prefree_seg(txd->mbuf);
			if (m == NULL)
				continue;

			txd->mbuf = NULL;

			if (nb == RTE_DIM(bulk) ||
			    (nb != 0 && m->pool != bulk[0]->pool)) {
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
}

static void
sfc_ef100_tx_reap(struct sfc_ef100_txq *txq)
{
	sfc_ef100_tx_reap_num_descs(txq, sfc_ef100_tx_process_events(txq));
}

static void
sfc_ef100_tx_qdesc_send_create(const struct rte_mbuf *m, efx_oword_t *tx_desc)
{
	EFX_POPULATE_OWORD_4(*tx_desc,
			ESF_GZ_TX_SEND_ADDR, rte_mbuf_data_iova(m),
			ESF_GZ_TX_SEND_LEN, rte_pktmbuf_data_len(m),
			ESF_GZ_TX_SEND_NUM_SEGS, m->nb_segs,
			ESF_GZ_TX_DESC_TYPE, ESE_GZ_TX_DESC_TYPE_SEND);
}

static void
sfc_ef100_tx_qdesc_seg_create(rte_iova_t addr, uint16_t len,
			      efx_oword_t *tx_desc)
{
	EFX_POPULATE_OWORD_3(*tx_desc,
			ESF_GZ_TX_SEG_ADDR, addr,
			ESF_GZ_TX_SEG_LEN, len,
			ESF_GZ_TX_DESC_TYPE, ESE_GZ_TX_DESC_TYPE_SEG);
}

static inline void
sfc_ef100_tx_qpush(struct sfc_ef100_txq *txq, unsigned int added)
{
	efx_dword_t dword;

	EFX_POPULATE_DWORD_1(dword, ERF_GZ_TX_RING_PIDX, added & txq->ptr_mask);

	/* DMA sync to device is not required */

	/*
	 * rte_write32() has rte_io_wmb() which guarantees that the STORE
	 * operations (i.e. Rx and event descriptor updates) that precede
	 * the rte_io_wmb() call are visible to NIC before the STORE
	 * operations that follow it (i.e. doorbell write).
	 */
	rte_write32(dword.ed_u32[0], txq->doorbell);

	sfc_ef100_tx_debug(txq, "TxQ pushed doorbell at pidx %u (added=%u)",
			   EFX_DWORD_FIELD(dword, ERF_GZ_TX_RING_PIDX),
			   added);
}

static unsigned int
sfc_ef100_tx_pkt_descs_max(const struct rte_mbuf *m)
{
/** Maximum length of an mbuf segment data */
#define SFC_MBUF_SEG_LEN_MAX		UINT16_MAX
	RTE_BUILD_BUG_ON(sizeof(m->data_len) != 2);

	/*
	 * mbuf segment cannot be bigger than maximum segment length and
	 * maximum packet length since TSO is not supported yet.
	 * Make sure that the first segment does not need fragmentation
	 * (split into many Tx descriptors).
	 */
	RTE_BUILD_BUG_ON(SFC_EF100_TX_SEND_DESC_LEN_MAX <
		RTE_MIN((unsigned int)EFX_MAC_PDU_MAX, SFC_MBUF_SEG_LEN_MAX));

	/*
	 * Any segment of scattered packet cannot be bigger than maximum
	 * segment length and maximum packet length since TSO is not
	 * supported yet.
	 * Make sure that subsequent segments do not need fragmentation (split
	 * into many Tx descriptors).
	 */
	RTE_BUILD_BUG_ON(SFC_EF100_TX_SEG_DESC_LEN_MAX <
		RTE_MIN((unsigned int)EFX_MAC_PDU_MAX, SFC_MBUF_SEG_LEN_MAX));

	return m->nb_segs;
}

static uint16_t
sfc_ef100_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct sfc_ef100_txq * const txq = sfc_ef100_txq_by_dp_txq(tx_queue);
	unsigned int added;
	unsigned int dma_desc_space;
	bool reap_done;
	struct rte_mbuf **pktp;
	struct rte_mbuf **pktp_end;

	if (unlikely(txq->flags &
		     (SFC_EF100_TXQ_NOT_RUNNING | SFC_EF100_TXQ_EXCEPTION)))
		return 0;

	added = txq->added;
	dma_desc_space = txq->max_fill_level - (added - txq->completed);

	reap_done = (dma_desc_space < txq->free_thresh);
	if (reap_done) {
		sfc_ef100_tx_reap(txq);
		dma_desc_space = txq->max_fill_level - (added - txq->completed);
	}

	for (pktp = &tx_pkts[0], pktp_end = &tx_pkts[nb_pkts];
	     pktp != pktp_end;
	     ++pktp) {
		struct rte_mbuf *m_seg = *pktp;
		unsigned int pkt_start = added;
		unsigned int id;

		if (likely(pktp + 1 != pktp_end))
			rte_mbuf_prefetch_part1(pktp[1]);

		if (sfc_ef100_tx_pkt_descs_max(m_seg) > dma_desc_space) {
			if (reap_done)
				break;

			/* Push already prepared descriptors before polling */
			if (added != txq->added) {
				sfc_ef100_tx_qpush(txq, added);
				txq->added = added;
			}

			sfc_ef100_tx_reap(txq);
			reap_done = true;
			dma_desc_space = txq->max_fill_level -
				(added - txq->completed);
			if (sfc_ef100_tx_pkt_descs_max(m_seg) > dma_desc_space)
				break;
		}

		id = added++ & txq->ptr_mask;
		sfc_ef100_tx_qdesc_send_create(m_seg, &txq->txq_hw_ring[id]);

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

		while ((m_seg = m_seg->next) != NULL) {
			RTE_BUILD_BUG_ON(SFC_MBUF_SEG_LEN_MAX >
					 SFC_EF100_TX_SEG_DESC_LEN_MAX);

			id = added++ & txq->ptr_mask;
			sfc_ef100_tx_qdesc_seg_create(rte_mbuf_data_iova(m_seg),
					rte_pktmbuf_data_len(m_seg),
					&txq->txq_hw_ring[id]);
			txq->sw_ring[id].mbuf = m_seg;
		}

		dma_desc_space -= (added - pkt_start);
	}

	if (likely(added != txq->added)) {
		sfc_ef100_tx_qpush(txq, added);
		txq->added = added;
	}

#if SFC_TX_XMIT_PKTS_REAP_AT_LEAST_ONCE
	if (!reap_done)
		sfc_ef100_tx_reap(txq);
#endif

	return pktp - &tx_pkts[0];
}

static sfc_dp_tx_get_dev_info_t sfc_ef100_get_dev_info;
static void
sfc_ef100_get_dev_info(struct rte_eth_dev_info *dev_info)
{
	/*
	 * Number of descriptors just defines maximum number of pushed
	 * descriptors (fill level).
	 */
	dev_info->tx_desc_lim.nb_min = 1;
	dev_info->tx_desc_lim.nb_align = 1;
}

static sfc_dp_tx_qsize_up_rings_t sfc_ef100_tx_qsize_up_rings;
static int
sfc_ef100_tx_qsize_up_rings(uint16_t nb_tx_desc,
			   struct sfc_dp_tx_hw_limits *limits,
			   unsigned int *txq_entries,
			   unsigned int *evq_entries,
			   unsigned int *txq_max_fill_level)
{
	/*
	 * rte_ethdev API guarantees that the number meets min, max and
	 * alignment requirements.
	 */
	if (nb_tx_desc <= limits->txq_min_entries)
		*txq_entries = limits->txq_min_entries;
	else
		*txq_entries = rte_align32pow2(nb_tx_desc);

	*evq_entries = *txq_entries;

	*txq_max_fill_level = RTE_MIN(nb_tx_desc,
				      SFC_EF100_TXQ_LIMIT(*evq_entries));
	return 0;
}

static sfc_dp_tx_qcreate_t sfc_ef100_tx_qcreate;
static int
sfc_ef100_tx_qcreate(uint16_t port_id, uint16_t queue_id,
		    const struct rte_pci_addr *pci_addr, int socket_id,
		    const struct sfc_dp_tx_qcreate_info *info,
		    struct sfc_dp_txq **dp_txqp)
{
	struct sfc_ef100_txq *txq;
	int rc;

	rc = EINVAL;
	if (info->txq_entries != info->evq_entries)
		goto fail_bad_args;

	rc = ENOMEM;
	txq = rte_zmalloc_socket("sfc-ef100-txq", sizeof(*txq),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (txq == NULL)
		goto fail_txq_alloc;

	sfc_dp_queue_init(&txq->dp.dpq, port_id, queue_id, pci_addr);

	rc = ENOMEM;
	txq->sw_ring = rte_calloc_socket("sfc-ef100-txq-sw_ring",
					 info->txq_entries,
					 sizeof(*txq->sw_ring),
					 RTE_CACHE_LINE_SIZE, socket_id);
	if (txq->sw_ring == NULL)
		goto fail_sw_ring_alloc;

	txq->flags = SFC_EF100_TXQ_NOT_RUNNING;
	txq->ptr_mask = info->txq_entries - 1;
	txq->max_fill_level = info->max_fill_level;
	txq->free_thresh = info->free_thresh;
	txq->evq_phase_bit_shift = rte_bsf32(info->evq_entries);
	txq->txq_hw_ring = info->txq_hw_ring;
	txq->doorbell = (volatile uint8_t *)info->mem_bar +
			ER_GZ_TX_RING_DOORBELL_OFST +
			(info->hw_index << info->vi_window_shift);
	txq->evq_hw_ring = info->evq_hw_ring;

	sfc_ef100_tx_debug(txq, "TxQ doorbell is %p", txq->doorbell);

	*dp_txqp = &txq->dp;
	return 0;

fail_sw_ring_alloc:
	rte_free(txq);

fail_txq_alloc:
fail_bad_args:
	return rc;
}

static sfc_dp_tx_qdestroy_t sfc_ef100_tx_qdestroy;
static void
sfc_ef100_tx_qdestroy(struct sfc_dp_txq *dp_txq)
{
	struct sfc_ef100_txq *txq = sfc_ef100_txq_by_dp_txq(dp_txq);

	rte_free(txq->sw_ring);
	rte_free(txq);
}

static sfc_dp_tx_qstart_t sfc_ef100_tx_qstart;
static int
sfc_ef100_tx_qstart(struct sfc_dp_txq *dp_txq, unsigned int evq_read_ptr,
		   unsigned int txq_desc_index)
{
	struct sfc_ef100_txq *txq = sfc_ef100_txq_by_dp_txq(dp_txq);

	txq->evq_read_ptr = evq_read_ptr;
	txq->added = txq->completed = txq_desc_index;

	txq->flags |= SFC_EF100_TXQ_STARTED;
	txq->flags &= ~(SFC_EF100_TXQ_NOT_RUNNING | SFC_EF100_TXQ_EXCEPTION);

	return 0;
}

static sfc_dp_tx_qstop_t sfc_ef100_tx_qstop;
static void
sfc_ef100_tx_qstop(struct sfc_dp_txq *dp_txq, unsigned int *evq_read_ptr)
{
	struct sfc_ef100_txq *txq = sfc_ef100_txq_by_dp_txq(dp_txq);

	txq->flags |= SFC_EF100_TXQ_NOT_RUNNING;

	*evq_read_ptr = txq->evq_read_ptr;
}

static sfc_dp_tx_qtx_ev_t sfc_ef100_tx_qtx_ev;
static bool
sfc_ef100_tx_qtx_ev(struct sfc_dp_txq *dp_txq, unsigned int num_descs)
{
	struct sfc_ef100_txq *txq = sfc_ef100_txq_by_dp_txq(dp_txq);

	SFC_ASSERT(txq->flags & SFC_EF100_TXQ_NOT_RUNNING);

	sfc_ef100_tx_reap_num_descs(txq, num_descs);

	return false;
}

static sfc_dp_tx_qreap_t sfc_ef100_tx_qreap;
static void
sfc_ef100_tx_qreap(struct sfc_dp_txq *dp_txq)
{
	struct sfc_ef100_txq *txq = sfc_ef100_txq_by_dp_txq(dp_txq);
	unsigned int completed;

	for (completed = txq->completed; completed != txq->added; ++completed) {
		struct sfc_ef100_tx_sw_desc *txd;

		txd = &txq->sw_ring[completed & txq->ptr_mask];
		if (txd->mbuf != NULL) {
			rte_pktmbuf_free_seg(txd->mbuf);
			txd->mbuf = NULL;
		}
	}

	txq->flags &= ~SFC_EF100_TXQ_STARTED;
}

static unsigned int
sfc_ef100_tx_qdesc_npending(struct sfc_ef100_txq *txq)
{
	const unsigned int evq_old_read_ptr = txq->evq_read_ptr;
	unsigned int npending = 0;
	efx_qword_t tx_ev;

	if (unlikely(txq->flags &
		     (SFC_EF100_TXQ_NOT_RUNNING | SFC_EF100_TXQ_EXCEPTION)))
		return 0;

	while (sfc_ef100_tx_get_event(txq, &tx_ev))
		npending += EFX_QWORD_FIELD(tx_ev, ESF_GZ_EV_TXCMPL_NUM_DESC);

	/*
	 * The function does not process events, so return event queue read
	 * pointer to the original position to allow the events that were
	 * read to be processed later
	 */
	txq->evq_read_ptr = evq_old_read_ptr;

	return npending;
}

static sfc_dp_tx_qdesc_status_t sfc_ef100_tx_qdesc_status;
static int
sfc_ef100_tx_qdesc_status(struct sfc_dp_txq *dp_txq, uint16_t offset)
{
	struct sfc_ef100_txq *txq = sfc_ef100_txq_by_dp_txq(dp_txq);
	unsigned int pushed = txq->added - txq->completed;

	if (unlikely(offset > txq->ptr_mask))
		return -EINVAL;

	if (unlikely(offset >= txq->max_fill_level))
		return RTE_ETH_TX_DESC_UNAVAIL;

	return (offset >= pushed ||
		offset < sfc_ef100_tx_qdesc_npending(txq)) ?
		RTE_ETH_TX_DESC_DONE : RTE_ETH_TX_DESC_FULL;
}

struct sfc_dp_tx sfc_ef100_tx = {
	.dp = {
		.name		= SFC_KVARG_DATAPATH_EF100,
		.type		= SFC_DP_TX,
		.hw_fw_caps	= SFC_DP_HW_FW_CAP_EF100,
	},
	.features		= SFC_DP_TX_FEAT_MULTI_PROCESS,
	.dev_offload_capa	= 0,
	.queue_offload_capa	= DEV_TX_OFFLOAD_MULTI_SEGS,
	.get_dev_info		= sfc_ef100_get_dev_info,
	.qsize_up_rings		= sfc_ef100_tx_qsize_up_rings,
	.qcreate		= sfc_ef100_tx_qcreate,
	.qdestroy		= sfc_ef100_tx_qdestroy,
	.qstart			= sfc_ef100_tx_qstart,
	.qtx_ev			= sfc_ef100_tx_qtx_ev,
	.qstop			= sfc_ef100_tx_qstop,
	.qreap			= sfc_ef100_tx_qreap,
	.qdesc_status		= sfc_ef100_tx_qdesc_status,
	.pkt_prepare		= sfc_ef100_tx_prepare_pkts,
	.pkt_burst		= sfc_ef100_xmit_pkts,
};
