/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2020 Xilinx, Inc.
 * Copyright(c) 2017-2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

/* EF10 equal stride packed stream receive native datapath implementation */

#include <stdbool.h>

#include <rte_byteorder.h>
#include <rte_mbuf.h>
#include <rte_io.h>

#include "efx_types.h"
#include "efx_regs_ef10.h"

#include "sfc_tweak.h"
#include "sfc_dp_rx.h"
#include "sfc_kvargs.h"
#include "sfc_ef10.h"

/* Tunnels are not supported */
#define SFC_EF10_RX_EV_ENCAP_SUPPORT	0
#include "sfc_ef10_rx_ev.h"

#define sfc_ef10_essb_rx_err(dpq, ...) \
	SFC_DP_LOG(SFC_KVARG_DATAPATH_EF10_ESSB, ERR, dpq, __VA_ARGS__)

#define sfc_ef10_essb_rx_info(dpq, ...) \
	SFC_DP_LOG(SFC_KVARG_DATAPATH_EF10_ESSB, INFO, dpq, __VA_ARGS__)

/*
 * Fake length for RXQ descriptors in equal stride super-buffer mode
 * to make hardware happy.
 */
#define SFC_EF10_ESSB_RX_FAKE_BUF_SIZE	32

/**
 * Minimum number of Rx buffers the datapath allows to use.
 *
 * Each HW Rx descriptor has many Rx buffers. The number of buffers
 * in one HW Rx descriptor is equal to size of contiguous block
 * provided by Rx buffers memory pool. The contiguous block size
 * depends on CONFIG_RTE_DRIVER_MEMPOOL_BUCKET_SIZE_KB and rte_mbuf
 * data size specified on the memory pool creation. Typical rte_mbuf
 * data size is about 2k which makes a bit less than 32 buffers in
 * contiguous block with default bucket size equal to 64k.
 * Since HW Rx descriptors are pushed by 8 (see SFC_EF10_RX_WPTR_ALIGN),
 * it makes about 256 as required minimum. Double it in advertised
 * minimum to allow for at least 2 refill blocks.
 */
#define SFC_EF10_ESSB_RX_DESCS_MIN	512

/**
 * Number of Rx buffers should be aligned to.
 *
 * There are no extra requirements on alignment since actual number of
 * pushed Rx buffers will be multiple by contiguous block size which
 * is unknown beforehand.
 */
#define SFC_EF10_ESSB_RX_DESCS_ALIGN	1

/**
 * Maximum number of descriptors/buffers in the Rx ring.
 * It should guarantee that corresponding event queue never overfill.
 */
#define SFC_EF10_ESSB_RXQ_LIMIT(_nevs) \
	((_nevs) - 1 /* head must not step on tail */ - \
	 (SFC_EF10_EV_PER_CACHE_LINE - 1) /* max unused EvQ entries */ - \
	 1 /* Rx error */ - 1 /* flush */)

struct sfc_ef10_essb_rx_sw_desc {
	struct rte_mbuf			*first_mbuf;
};

struct sfc_ef10_essb_rxq {
	/* Used on data path */
	unsigned int			flags;
#define SFC_EF10_ESSB_RXQ_STARTED	0x1
#define SFC_EF10_ESSB_RXQ_NOT_RUNNING	0x2
#define SFC_EF10_ESSB_RXQ_EXCEPTION	0x4
	unsigned int			rxq_ptr_mask;
	unsigned int			block_size;
	unsigned int			buf_stride;
	unsigned int			bufs_ptr;
	unsigned int			completed;
	unsigned int			pending_id;
	unsigned int			bufs_pending;
	unsigned int			left_in_completed;
	unsigned int			left_in_pending;
	unsigned int			evq_read_ptr;
	unsigned int			evq_ptr_mask;
	efx_qword_t			*evq_hw_ring;
	struct sfc_ef10_essb_rx_sw_desc	*sw_ring;
	uint16_t			port_id;

	/* Used on refill */
	unsigned int			added;
	unsigned int			max_fill_level;
	unsigned int			refill_threshold;
	struct rte_mempool		*refill_mb_pool;
	efx_qword_t			*rxq_hw_ring;
	volatile void			*doorbell;

	/* Datapath receive queue anchor */
	struct sfc_dp_rxq		dp;
};

static inline struct sfc_ef10_essb_rxq *
sfc_ef10_essb_rxq_by_dp_rxq(struct sfc_dp_rxq *dp_rxq)
{
	return container_of(dp_rxq, struct sfc_ef10_essb_rxq, dp);
}

static struct rte_mbuf *
sfc_ef10_essb_next_mbuf(const struct sfc_ef10_essb_rxq *rxq,
			struct rte_mbuf *mbuf)
{
	struct rte_mbuf *m;

	m = (struct rte_mbuf *)((uintptr_t)mbuf + rxq->buf_stride);
	MBUF_RAW_ALLOC_CHECK(m);
	return m;
}

static struct rte_mbuf *
sfc_ef10_essb_mbuf_by_index(const struct sfc_ef10_essb_rxq *rxq,
			    struct rte_mbuf *mbuf, unsigned int idx)
{
	struct rte_mbuf *m;

	m = (struct rte_mbuf *)((uintptr_t)mbuf + idx * rxq->buf_stride);
	MBUF_RAW_ALLOC_CHECK(m);
	return m;
}

static struct rte_mbuf *
sfc_ef10_essb_maybe_next_completed(struct sfc_ef10_essb_rxq *rxq)
{
	const struct sfc_ef10_essb_rx_sw_desc *rxd;

	if (rxq->left_in_completed != 0) {
		rxd = &rxq->sw_ring[rxq->completed & rxq->rxq_ptr_mask];
		return sfc_ef10_essb_mbuf_by_index(rxq, rxd->first_mbuf,
				rxq->block_size - rxq->left_in_completed);
	} else {
		rxq->completed++;
		rxd = &rxq->sw_ring[rxq->completed & rxq->rxq_ptr_mask];
		rxq->left_in_completed = rxq->block_size;
		return rxd->first_mbuf;
	}
}

static void
sfc_ef10_essb_rx_qrefill(struct sfc_ef10_essb_rxq *rxq)
{
	const unsigned int rxq_ptr_mask = rxq->rxq_ptr_mask;
	unsigned int free_space;
	unsigned int bulks;
	void *mbuf_blocks[SFC_EF10_RX_WPTR_ALIGN];
	unsigned int added = rxq->added;

	free_space = rxq->max_fill_level - (added - rxq->completed);

	if (free_space < rxq->refill_threshold)
		return;

	bulks = free_space / RTE_DIM(mbuf_blocks);
	/* refill_threshold guarantees that bulks is positive */
	SFC_ASSERT(bulks > 0);

	do {
		unsigned int id;
		unsigned int i;

		if (unlikely(rte_mempool_get_contig_blocks(rxq->refill_mb_pool,
				mbuf_blocks, RTE_DIM(mbuf_blocks)) < 0)) {
			struct rte_eth_dev_data *dev_data =
				rte_eth_devices[rxq->port_id].data;

			/*
			 * It is hardly a safe way to increment counter
			 * from different contexts, but all PMDs do it.
			 */
			dev_data->rx_mbuf_alloc_failed += RTE_DIM(mbuf_blocks);
			/* Return if we have posted nothing yet */
			if (added == rxq->added)
				return;
			/* Push posted */
			break;
		}

		for (i = 0, id = added & rxq_ptr_mask;
		     i < RTE_DIM(mbuf_blocks);
		     ++i, ++id) {
			struct rte_mbuf *m = mbuf_blocks[i];
			struct sfc_ef10_essb_rx_sw_desc *rxd;

			SFC_ASSERT((id & ~rxq_ptr_mask) == 0);
			rxd = &rxq->sw_ring[id];
			rxd->first_mbuf = m;

			/* RX_KER_BYTE_CNT is ignored by firmware */
			EFX_POPULATE_QWORD_2(rxq->rxq_hw_ring[id],
					     ESF_DZ_RX_KER_BYTE_CNT,
					     SFC_EF10_ESSB_RX_FAKE_BUF_SIZE,
					     ESF_DZ_RX_KER_BUF_ADDR,
					     rte_mbuf_data_iova_default(m));
		}

		added += RTE_DIM(mbuf_blocks);

	} while (--bulks > 0);

	SFC_ASSERT(rxq->added != added);
	rxq->added = added;
	sfc_ef10_rx_qpush(rxq->doorbell, added, rxq_ptr_mask);
}

static bool
sfc_ef10_essb_rx_event_get(struct sfc_ef10_essb_rxq *rxq, efx_qword_t *rx_ev)
{
	*rx_ev = rxq->evq_hw_ring[rxq->evq_read_ptr & rxq->evq_ptr_mask];

	if (!sfc_ef10_ev_present(*rx_ev))
		return false;

	if (unlikely(EFX_QWORD_FIELD(*rx_ev, FSF_AZ_EV_CODE) !=
		     FSE_AZ_EV_CODE_RX_EV)) {
		/*
		 * Do not move read_ptr to keep the event for exception
		 * handling
		 */
		rxq->flags |= SFC_EF10_ESSB_RXQ_EXCEPTION;
		sfc_ef10_essb_rx_err(&rxq->dp.dpq,
				     "RxQ exception at EvQ read ptr %#x",
				     rxq->evq_read_ptr);
		return false;
	}

	rxq->evq_read_ptr++;
	return true;
}

static void
sfc_ef10_essb_rx_process_ev(struct sfc_ef10_essb_rxq *rxq, efx_qword_t rx_ev)
{
	unsigned int ready;

	ready = (EFX_QWORD_FIELD(rx_ev, ESF_DZ_RX_DSC_PTR_LBITS) -
		 rxq->bufs_ptr) &
		EFX_MASK32(ESF_DZ_RX_DSC_PTR_LBITS);

	rxq->bufs_ptr += ready;
	rxq->bufs_pending += ready;

	SFC_ASSERT(ready > 0);
	do {
		const struct sfc_ef10_essb_rx_sw_desc *rxd;
		struct rte_mbuf *m;
		unsigned int todo_bufs;
		struct rte_mbuf *m0;

		rxd = &rxq->sw_ring[rxq->pending_id];
		m = sfc_ef10_essb_mbuf_by_index(rxq, rxd->first_mbuf,
			rxq->block_size - rxq->left_in_pending);

		if (ready < rxq->left_in_pending) {
			todo_bufs = ready;
			ready = 0;
			rxq->left_in_pending -= todo_bufs;
		} else {
			todo_bufs = rxq->left_in_pending;
			ready -= todo_bufs;
			rxq->left_in_pending = rxq->block_size;
			if (rxq->pending_id != rxq->rxq_ptr_mask)
				rxq->pending_id++;
			else
				rxq->pending_id = 0;
		}

		SFC_ASSERT(todo_bufs > 0);
		--todo_bufs;

		sfc_ef10_rx_ev_to_offloads(rx_ev, m, ~0ull);

		/* Prefetch pseudo-header */
		rte_prefetch0((uint8_t *)m->buf_addr + RTE_PKTMBUF_HEADROOM);

		m0 = m;
		while (todo_bufs-- > 0) {
			m = sfc_ef10_essb_next_mbuf(rxq, m);
			m->ol_flags = m0->ol_flags;
			m->packet_type = m0->packet_type;
			/* Prefetch pseudo-header */
			rte_prefetch0((uint8_t *)m->buf_addr +
				      RTE_PKTMBUF_HEADROOM);
		}
	} while (ready > 0);
}

static unsigned int
sfc_ef10_essb_rx_get_pending(struct sfc_ef10_essb_rxq *rxq,
			     struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	unsigned int n_rx_pkts = 0;
	unsigned int todo_bufs;
	struct rte_mbuf *m;

	while ((todo_bufs = RTE_MIN(nb_pkts - n_rx_pkts,
				    rxq->bufs_pending)) > 0) {
		m = sfc_ef10_essb_maybe_next_completed(rxq);

		todo_bufs = RTE_MIN(todo_bufs, rxq->left_in_completed);

		rxq->bufs_pending -= todo_bufs;
		rxq->left_in_completed -= todo_bufs;

		SFC_ASSERT(todo_bufs > 0);
		todo_bufs--;

		do {
			const efx_qword_t *qwordp;
			uint16_t pkt_len;

			/* Buffers to be discarded have 0 in packet type */
			if (unlikely(m->packet_type == 0)) {
				rte_mbuf_raw_free(m);
				goto next_buf;
			}

			rx_pkts[n_rx_pkts++] = m;

			/* Parse pseudo-header */
			qwordp = (const efx_qword_t *)
				((uint8_t *)m->buf_addr + RTE_PKTMBUF_HEADROOM);
			pkt_len =
				EFX_QWORD_FIELD(*qwordp,
						ES_EZ_ESSB_RX_PREFIX_DATA_LEN);

			m->data_off = RTE_PKTMBUF_HEADROOM +
				ES_EZ_ESSB_RX_PREFIX_LEN;
			m->port = rxq->port_id;

			rte_pktmbuf_pkt_len(m) = pkt_len;
			rte_pktmbuf_data_len(m) = pkt_len;

			m->ol_flags |=
				(PKT_RX_RSS_HASH *
				 !!EFX_TEST_QWORD_BIT(*qwordp,
					ES_EZ_ESSB_RX_PREFIX_HASH_VALID_LBN)) |
				(PKT_RX_FDIR_ID *
				 !!EFX_TEST_QWORD_BIT(*qwordp,
					ES_EZ_ESSB_RX_PREFIX_MARK_VALID_LBN)) |
				(PKT_RX_FDIR *
				 !!EFX_TEST_QWORD_BIT(*qwordp,
					ES_EZ_ESSB_RX_PREFIX_MATCH_FLAG_LBN));

			/* EFX_QWORD_FIELD converts little-endian to CPU */
			m->hash.rss =
				EFX_QWORD_FIELD(*qwordp,
						ES_EZ_ESSB_RX_PREFIX_HASH);
			m->hash.fdir.hi =
				EFX_QWORD_FIELD(*qwordp,
						ES_EZ_ESSB_RX_PREFIX_MARK);

next_buf:
			m = sfc_ef10_essb_next_mbuf(rxq, m);
		} while (todo_bufs-- > 0);
	}

	return n_rx_pkts;
}


static uint16_t
sfc_ef10_essb_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
			uint16_t nb_pkts)
{
	struct sfc_ef10_essb_rxq *rxq = sfc_ef10_essb_rxq_by_dp_rxq(rx_queue);
	const unsigned int evq_old_read_ptr = rxq->evq_read_ptr;
	uint16_t n_rx_pkts;
	efx_qword_t rx_ev;

	if (unlikely(rxq->flags & (SFC_EF10_ESSB_RXQ_NOT_RUNNING |
				   SFC_EF10_ESSB_RXQ_EXCEPTION)))
		return 0;

	n_rx_pkts = sfc_ef10_essb_rx_get_pending(rxq, rx_pkts, nb_pkts);

	while (n_rx_pkts != nb_pkts &&
	       sfc_ef10_essb_rx_event_get(rxq, &rx_ev)) {
		/*
		 * DROP_EVENT is an internal to the NIC, software should
		 * never see it and, therefore, may ignore it.
		 */

		sfc_ef10_essb_rx_process_ev(rxq, rx_ev);
		n_rx_pkts += sfc_ef10_essb_rx_get_pending(rxq,
							  rx_pkts + n_rx_pkts,
							  nb_pkts - n_rx_pkts);
	}

	sfc_ef10_ev_qclear(rxq->evq_hw_ring, rxq->evq_ptr_mask,
			   evq_old_read_ptr, rxq->evq_read_ptr);

	/* It is not a problem if we refill in the case of exception */
	sfc_ef10_essb_rx_qrefill(rxq);

	return n_rx_pkts;
}

static sfc_dp_rx_qdesc_npending_t sfc_ef10_essb_rx_qdesc_npending;
static unsigned int
sfc_ef10_essb_rx_qdesc_npending(struct sfc_dp_rxq *dp_rxq)
{
	struct sfc_ef10_essb_rxq *rxq = sfc_ef10_essb_rxq_by_dp_rxq(dp_rxq);
	const unsigned int evq_old_read_ptr = rxq->evq_read_ptr;
	efx_qword_t rx_ev;

	if (unlikely(rxq->flags & (SFC_EF10_ESSB_RXQ_NOT_RUNNING |
				   SFC_EF10_ESSB_RXQ_EXCEPTION)))
		return rxq->bufs_pending;

	while (sfc_ef10_essb_rx_event_get(rxq, &rx_ev)) {
		/*
		 * DROP_EVENT is an internal to the NIC, software should
		 * never see it and, therefore, may ignore it.
		 */
		sfc_ef10_essb_rx_process_ev(rxq, rx_ev);
	}

	sfc_ef10_ev_qclear(rxq->evq_hw_ring, rxq->evq_ptr_mask,
			   evq_old_read_ptr, rxq->evq_read_ptr);

	return rxq->bufs_pending;
}

static sfc_dp_rx_qdesc_status_t sfc_ef10_essb_rx_qdesc_status;
static int
sfc_ef10_essb_rx_qdesc_status(struct sfc_dp_rxq *dp_rxq, uint16_t offset)
{
	struct sfc_ef10_essb_rxq *rxq = sfc_ef10_essb_rxq_by_dp_rxq(dp_rxq);
	unsigned int pending = sfc_ef10_essb_rx_qdesc_npending(dp_rxq);

	if (offset < pending)
		return RTE_ETH_RX_DESC_DONE;

	if (offset < (rxq->added - rxq->completed) * rxq->block_size +
		     rxq->left_in_completed - rxq->block_size)
		return RTE_ETH_RX_DESC_AVAIL;

	return RTE_ETH_RX_DESC_UNAVAIL;
}

static sfc_dp_rx_get_dev_info_t sfc_ef10_essb_rx_get_dev_info;
static void
sfc_ef10_essb_rx_get_dev_info(struct rte_eth_dev_info *dev_info)
{
	/*
	 * Number of descriptors just defines maximum number of pushed
	 * descriptors (fill level).
	 */
	dev_info->rx_desc_lim.nb_min = SFC_EF10_ESSB_RX_DESCS_MIN;
	dev_info->rx_desc_lim.nb_align = SFC_EF10_ESSB_RX_DESCS_ALIGN;
}

static sfc_dp_rx_pool_ops_supported_t sfc_ef10_essb_rx_pool_ops_supported;
static int
sfc_ef10_essb_rx_pool_ops_supported(const char *pool)
{
	SFC_ASSERT(pool != NULL);

	if (strcmp(pool, "bucket") == 0)
		return 0;

	return -ENOTSUP;
}

static sfc_dp_rx_qsize_up_rings_t sfc_ef10_essb_rx_qsize_up_rings;
static int
sfc_ef10_essb_rx_qsize_up_rings(uint16_t nb_rx_desc,
				struct sfc_dp_rx_hw_limits *limits,
				struct rte_mempool *mb_pool,
				unsigned int *rxq_entries,
				unsigned int *evq_entries,
				unsigned int *rxq_max_fill_level)
{
	int rc;
	struct rte_mempool_info mp_info;
	unsigned int nb_hw_rx_desc;
	unsigned int max_events;

	rc = rte_mempool_ops_get_info(mb_pool, &mp_info);
	if (rc != 0)
		return -rc;
	if (mp_info.contig_block_size == 0)
		return EINVAL;

	/*
	 * Calculate required number of hardware Rx descriptors each
	 * carrying contig block size Rx buffers.
	 * It cannot be less than Rx write pointer alignment plus 1
	 * in order to avoid cases when the ring is guaranteed to be
	 * empty.
	 */
	nb_hw_rx_desc = RTE_MAX(SFC_DIV_ROUND_UP(nb_rx_desc,
						 mp_info.contig_block_size),
				SFC_EF10_RX_WPTR_ALIGN + 1);
	if (nb_hw_rx_desc <= limits->rxq_min_entries) {
		*rxq_entries = limits->rxq_min_entries;
	} else {
		*rxq_entries = rte_align32pow2(nb_hw_rx_desc);
		if (*rxq_entries > limits->rxq_max_entries)
			return EINVAL;
	}

	max_events = RTE_ALIGN_FLOOR(nb_hw_rx_desc, SFC_EF10_RX_WPTR_ALIGN) *
		mp_info.contig_block_size +
		(SFC_EF10_EV_PER_CACHE_LINE - 1) /* max unused EvQ entries */ +
		1 /* Rx error */ + 1 /* flush */ + 1 /* head-tail space */;

	*evq_entries = rte_align32pow2(max_events);
	*evq_entries = RTE_MAX(*evq_entries, limits->evq_min_entries);
	*evq_entries = RTE_MIN(*evq_entries, limits->evq_max_entries);

	/*
	 * May be even maximum event queue size is insufficient to handle
	 * so many Rx descriptors. If so, we should limit Rx queue fill level.
	 */
	*rxq_max_fill_level = RTE_MIN(nb_rx_desc,
				      SFC_EF10_ESSB_RXQ_LIMIT(*evq_entries));
	return 0;
}

static sfc_dp_rx_qcreate_t sfc_ef10_essb_rx_qcreate;
static int
sfc_ef10_essb_rx_qcreate(uint16_t port_id, uint16_t queue_id,
			 const struct rte_pci_addr *pci_addr, int socket_id,
			 const struct sfc_dp_rx_qcreate_info *info,
			 struct sfc_dp_rxq **dp_rxqp)
{
	struct rte_mempool * const mp = info->refill_mb_pool;
	struct rte_mempool_info mp_info;
	struct sfc_ef10_essb_rxq *rxq;
	int rc;

	rc = rte_mempool_ops_get_info(mp, &mp_info);
	if (rc != 0) {
		/* Positive errno is used in the driver */
		rc = -rc;
		goto fail_get_contig_block_size;
	}

	/* Check if the mempool provides block dequeue */
	rc = EINVAL;
	if (mp_info.contig_block_size == 0)
		goto fail_no_block_dequeue;

	rc = ENOMEM;
	rxq = rte_zmalloc_socket("sfc-ef10-rxq", sizeof(*rxq),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (rxq == NULL)
		goto fail_rxq_alloc;

	sfc_dp_queue_init(&rxq->dp.dpq, port_id, queue_id, pci_addr);

	rc = ENOMEM;
	rxq->sw_ring = rte_calloc_socket("sfc-ef10-rxq-sw_ring",
					 info->rxq_entries,
					 sizeof(*rxq->sw_ring),
					 RTE_CACHE_LINE_SIZE, socket_id);
	if (rxq->sw_ring == NULL)
		goto fail_desc_alloc;

	rxq->block_size = mp_info.contig_block_size;
	rxq->buf_stride = mp->header_size + mp->elt_size + mp->trailer_size;
	rxq->rxq_ptr_mask = info->rxq_entries - 1;
	rxq->evq_ptr_mask = info->evq_entries - 1;
	rxq->evq_hw_ring = info->evq_hw_ring;
	rxq->port_id = port_id;

	rxq->max_fill_level = info->max_fill_level / mp_info.contig_block_size;
	rxq->refill_threshold =
		RTE_MAX(info->refill_threshold / mp_info.contig_block_size,
			SFC_EF10_RX_WPTR_ALIGN);
	rxq->refill_mb_pool = mp;
	rxq->rxq_hw_ring = info->rxq_hw_ring;

	rxq->doorbell = (volatile uint8_t *)info->mem_bar +
			ER_DZ_RX_DESC_UPD_REG_OFST +
			(info->hw_index << info->vi_window_shift);

	sfc_ef10_essb_rx_info(&rxq->dp.dpq,
			      "block size is %u, buf stride is %u",
			      rxq->block_size, rxq->buf_stride);
	sfc_ef10_essb_rx_info(&rxq->dp.dpq,
			      "max fill level is %u descs (%u bufs), "
			      "refill threashold %u descs (%u bufs)",
			      rxq->max_fill_level,
			      rxq->max_fill_level * rxq->block_size,
			      rxq->refill_threshold,
			      rxq->refill_threshold * rxq->block_size);

	*dp_rxqp = &rxq->dp;
	return 0;

fail_desc_alloc:
	rte_free(rxq);

fail_rxq_alloc:
fail_no_block_dequeue:
fail_get_contig_block_size:
	return rc;
}

static sfc_dp_rx_qdestroy_t sfc_ef10_essb_rx_qdestroy;
static void
sfc_ef10_essb_rx_qdestroy(struct sfc_dp_rxq *dp_rxq)
{
	struct sfc_ef10_essb_rxq *rxq = sfc_ef10_essb_rxq_by_dp_rxq(dp_rxq);

	rte_free(rxq->sw_ring);
	rte_free(rxq);
}

static sfc_dp_rx_qstart_t sfc_ef10_essb_rx_qstart;
static int
sfc_ef10_essb_rx_qstart(struct sfc_dp_rxq *dp_rxq, unsigned int evq_read_ptr)
{
	struct sfc_ef10_essb_rxq *rxq = sfc_ef10_essb_rxq_by_dp_rxq(dp_rxq);

	rxq->evq_read_ptr = evq_read_ptr;

	/* Initialize before refill */
	rxq->completed = rxq->pending_id = rxq->added = 0;
	rxq->left_in_completed = rxq->left_in_pending = rxq->block_size;
	rxq->bufs_ptr = UINT_MAX;
	rxq->bufs_pending = 0;

	sfc_ef10_essb_rx_qrefill(rxq);

	rxq->flags |= SFC_EF10_ESSB_RXQ_STARTED;
	rxq->flags &=
		~(SFC_EF10_ESSB_RXQ_NOT_RUNNING | SFC_EF10_ESSB_RXQ_EXCEPTION);

	return 0;
}

static sfc_dp_rx_qstop_t sfc_ef10_essb_rx_qstop;
static void
sfc_ef10_essb_rx_qstop(struct sfc_dp_rxq *dp_rxq, unsigned int *evq_read_ptr)
{
	struct sfc_ef10_essb_rxq *rxq = sfc_ef10_essb_rxq_by_dp_rxq(dp_rxq);

	rxq->flags |= SFC_EF10_ESSB_RXQ_NOT_RUNNING;

	*evq_read_ptr = rxq->evq_read_ptr;
}

static sfc_dp_rx_qrx_ev_t sfc_ef10_essb_rx_qrx_ev;
static bool
sfc_ef10_essb_rx_qrx_ev(struct sfc_dp_rxq *dp_rxq, __rte_unused unsigned int id)
{
	__rte_unused struct sfc_ef10_essb_rxq *rxq;

	rxq = sfc_ef10_essb_rxq_by_dp_rxq(dp_rxq);
	SFC_ASSERT(rxq->flags & SFC_EF10_ESSB_RXQ_NOT_RUNNING);

	/*
	 * It is safe to ignore Rx event since we free all mbufs on
	 * queue purge anyway.
	 */

	return false;
}

static sfc_dp_rx_qpurge_t sfc_ef10_essb_rx_qpurge;
static void
sfc_ef10_essb_rx_qpurge(struct sfc_dp_rxq *dp_rxq)
{
	struct sfc_ef10_essb_rxq *rxq = sfc_ef10_essb_rxq_by_dp_rxq(dp_rxq);
	unsigned int i;
	const struct sfc_ef10_essb_rx_sw_desc *rxd;
	struct rte_mbuf *m;

	for (i = rxq->completed; i != rxq->added; ++i) {
		rxd = &rxq->sw_ring[i & rxq->rxq_ptr_mask];
		m = sfc_ef10_essb_mbuf_by_index(rxq, rxd->first_mbuf,
				rxq->block_size - rxq->left_in_completed);
		while (rxq->left_in_completed > 0) {
			rte_mbuf_raw_free(m);
			m = sfc_ef10_essb_next_mbuf(rxq, m);
			rxq->left_in_completed--;
		}
		rxq->left_in_completed = rxq->block_size;
	}

	rxq->flags &= ~SFC_EF10_ESSB_RXQ_STARTED;
}

struct sfc_dp_rx sfc_ef10_essb_rx = {
	.dp = {
		.name		= SFC_KVARG_DATAPATH_EF10_ESSB,
		.type		= SFC_DP_RX,
		.hw_fw_caps	= SFC_DP_HW_FW_CAP_EF10 |
				  SFC_DP_HW_FW_CAP_RX_ES_SUPER_BUFFER,
	},
	.features		= SFC_DP_RX_FEAT_FLOW_FLAG |
				  SFC_DP_RX_FEAT_FLOW_MARK,
	.dev_offload_capa	= DEV_RX_OFFLOAD_CHECKSUM |
				  DEV_RX_OFFLOAD_RSS_HASH,
	.queue_offload_capa	= 0,
	.get_dev_info		= sfc_ef10_essb_rx_get_dev_info,
	.pool_ops_supported	= sfc_ef10_essb_rx_pool_ops_supported,
	.qsize_up_rings		= sfc_ef10_essb_rx_qsize_up_rings,
	.qcreate		= sfc_ef10_essb_rx_qcreate,
	.qdestroy		= sfc_ef10_essb_rx_qdestroy,
	.qstart			= sfc_ef10_essb_rx_qstart,
	.qstop			= sfc_ef10_essb_rx_qstop,
	.qrx_ev			= sfc_ef10_essb_rx_qrx_ev,
	.qpurge			= sfc_ef10_essb_rx_qpurge,
	.supported_ptypes_get	= sfc_ef10_supported_ptypes_get,
	.qdesc_npending		= sfc_ef10_essb_rx_qdesc_npending,
	.qdesc_status		= sfc_ef10_essb_rx_qdesc_status,
	.pkt_burst		= sfc_ef10_essb_recv_pkts,
};
