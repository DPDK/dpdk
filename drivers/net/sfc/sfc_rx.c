/*-
 * Copyright (c) 2016 Solarflare Communications Inc.
 * All rights reserved.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <rte_mempool.h>

#include "efx.h"

#include "sfc.h"
#include "sfc_debug.h"
#include "sfc_log.h"
#include "sfc_ev.h"
#include "sfc_rx.h"
#include "sfc_tweak.h"

/*
 * Maximum number of Rx queue flush attempt in the case of failure or
 * flush timeout
 */
#define SFC_RX_QFLUSH_ATTEMPTS		(3)

/*
 * Time to wait between event queue polling attempts when waiting for Rx
 * queue flush done or failed events.
 */
#define SFC_RX_QFLUSH_POLL_WAIT_MS	(1)

/*
 * Maximum number of event queue polling attempts when waiting for Rx queue
 * flush done or failed events. It defines Rx queue flush attempt timeout
 * together with SFC_RX_QFLUSH_POLL_WAIT_MS.
 */
#define SFC_RX_QFLUSH_POLL_ATTEMPTS	(2000)

void
sfc_rx_qflush_done(struct sfc_rxq *rxq)
{
	rxq->state |= SFC_RXQ_FLUSHED;
	rxq->state &= ~SFC_RXQ_FLUSHING;
}

void
sfc_rx_qflush_failed(struct sfc_rxq *rxq)
{
	rxq->state |= SFC_RXQ_FLUSH_FAILED;
	rxq->state &= ~SFC_RXQ_FLUSHING;
}

static void
sfc_rx_qrefill(struct sfc_rxq *rxq)
{
	unsigned int free_space;
	unsigned int bulks;
	void *objs[SFC_RX_REFILL_BULK];
	efsys_dma_addr_t addr[RTE_DIM(objs)];
	unsigned int added = rxq->added;
	unsigned int id;
	unsigned int i;
	struct sfc_rx_sw_desc *rxd;
	struct rte_mbuf *m;
	uint8_t port_id = rxq->port_id;

	free_space = EFX_RXQ_LIMIT(rxq->ptr_mask + 1) -
		(added - rxq->completed);
	bulks = free_space / RTE_DIM(objs);

	id = added & rxq->ptr_mask;
	while (bulks-- > 0) {
		if (rte_mempool_get_bulk(rxq->refill_mb_pool, objs,
					 RTE_DIM(objs)) < 0) {
			/*
			 * It is hardly a safe way to increment counter
			 * from different contexts, but all PMDs do it.
			 */
			rxq->evq->sa->eth_dev->data->rx_mbuf_alloc_failed +=
				RTE_DIM(objs);
			break;
		}

		for (i = 0; i < RTE_DIM(objs);
		     ++i, id = (id + 1) & rxq->ptr_mask) {
			m = objs[i];

			rxd = &rxq->sw_desc[id];
			rxd->mbuf = m;

			rte_mbuf_refcnt_set(m, 1);
			m->data_off = RTE_PKTMBUF_HEADROOM;
			m->next = NULL;
			m->nb_segs = 1;
			m->port = port_id;

			addr[i] = rte_pktmbuf_mtophys(m);
		}

		efx_rx_qpost(rxq->common, addr, rxq->buf_size,
			     RTE_DIM(objs), rxq->completed, added);
		added += RTE_DIM(objs);
	}

	/* Push doorbell if something is posted */
	if (rxq->added != added) {
		rxq->added = added;
		efx_rx_qpush(rxq->common, added, &rxq->pushed);
	}
}

static uint64_t
sfc_rx_desc_flags_to_offload_flags(const unsigned int desc_flags)
{
	uint64_t mbuf_flags = 0;

	switch (desc_flags & (EFX_PKT_IPV4 | EFX_CKSUM_IPV4)) {
	case (EFX_PKT_IPV4 | EFX_CKSUM_IPV4):
		mbuf_flags |= PKT_RX_IP_CKSUM_GOOD;
		break;
	case EFX_PKT_IPV4:
		mbuf_flags |= PKT_RX_IP_CKSUM_BAD;
		break;
	default:
		RTE_BUILD_BUG_ON(PKT_RX_IP_CKSUM_UNKNOWN != 0);
		SFC_ASSERT((mbuf_flags & PKT_RX_IP_CKSUM_MASK) ==
			   PKT_RX_IP_CKSUM_UNKNOWN);
		break;
	}

	switch ((desc_flags &
		 (EFX_PKT_TCP | EFX_PKT_UDP | EFX_CKSUM_TCPUDP))) {
	case (EFX_PKT_TCP | EFX_CKSUM_TCPUDP):
	case (EFX_PKT_UDP | EFX_CKSUM_TCPUDP):
		mbuf_flags |= PKT_RX_L4_CKSUM_GOOD;
		break;
	case EFX_PKT_TCP:
	case EFX_PKT_UDP:
		mbuf_flags |= PKT_RX_L4_CKSUM_BAD;
		break;
	default:
		RTE_BUILD_BUG_ON(PKT_RX_L4_CKSUM_UNKNOWN != 0);
		SFC_ASSERT((mbuf_flags & PKT_RX_L4_CKSUM_MASK) ==
			   PKT_RX_L4_CKSUM_UNKNOWN);
		break;
	}

	return mbuf_flags;
}

uint16_t
sfc_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct sfc_rxq *rxq = rx_queue;
	unsigned int completed;
	unsigned int prefix_size = rxq->prefix_size;
	unsigned int done_pkts = 0;
	boolean_t discard_next = B_FALSE;

	if (unlikely((rxq->state & SFC_RXQ_RUNNING) == 0))
		return 0;

	sfc_ev_qpoll(rxq->evq);

	completed = rxq->completed;
	while (completed != rxq->pending && done_pkts < nb_pkts) {
		unsigned int id;
		struct sfc_rx_sw_desc *rxd;
		struct rte_mbuf *m;
		unsigned int seg_len;
		unsigned int desc_flags;

		id = completed++ & rxq->ptr_mask;
		rxd = &rxq->sw_desc[id];
		m = rxd->mbuf;
		desc_flags = rxd->flags;

		if (discard_next)
			goto discard;

		if (desc_flags & (EFX_ADDR_MISMATCH | EFX_DISCARD))
			goto discard;

		if (desc_flags & EFX_PKT_CONT)
			goto discard;

		if (desc_flags & EFX_PKT_PREFIX_LEN) {
			uint16_t tmp_size;
			int rc __rte_unused;

			rc = efx_pseudo_hdr_pkt_length_get(rxq->common,
				rte_pktmbuf_mtod(m, uint8_t *), &tmp_size);
			SFC_ASSERT(rc == 0);
			seg_len = tmp_size;
		} else {
			seg_len = rxd->size - prefix_size;
		}

		m->data_off += prefix_size;
		rte_pktmbuf_data_len(m) = seg_len;
		rte_pktmbuf_pkt_len(m) = seg_len;

		m->ol_flags = sfc_rx_desc_flags_to_offload_flags(desc_flags);
		m->packet_type = RTE_PTYPE_L2_ETHER;

		*rx_pkts++ = m;
		done_pkts++;
		continue;

discard:
		discard_next = ((desc_flags & EFX_PKT_CONT) != 0);
		rte_mempool_put(rxq->refill_mb_pool, m);
		rxd->mbuf = NULL;
	}

	rxq->completed = completed;

	sfc_rx_qrefill(rxq);

	return done_pkts;
}

static void
sfc_rx_qpurge(struct sfc_rxq *rxq)
{
	unsigned int i;
	struct sfc_rx_sw_desc *rxd;

	for (i = rxq->completed; i != rxq->added; ++i) {
		rxd = &rxq->sw_desc[i & rxq->ptr_mask];
		rte_mempool_put(rxq->refill_mb_pool, rxd->mbuf);
		rxd->mbuf = NULL;
	}
}

static void
sfc_rx_qflush(struct sfc_adapter *sa, unsigned int sw_index)
{
	struct sfc_rxq *rxq;
	unsigned int retry_count;
	unsigned int wait_count;

	rxq = sa->rxq_info[sw_index].rxq;
	SFC_ASSERT(rxq->state & SFC_RXQ_STARTED);

	/*
	 * Retry Rx queue flushing in the case of flush failed or
	 * timeout. In the worst case it can delay for 6 seconds.
	 */
	for (retry_count = 0;
	     ((rxq->state & SFC_RXQ_FLUSHED) == 0) &&
	     (retry_count < SFC_RX_QFLUSH_ATTEMPTS);
	     ++retry_count) {
		if (efx_rx_qflush(rxq->common) != 0) {
			rxq->state |= SFC_RXQ_FLUSH_FAILED;
			break;
		}
		rxq->state &= ~SFC_RXQ_FLUSH_FAILED;
		rxq->state |= SFC_RXQ_FLUSHING;

		/*
		 * Wait for Rx queue flush done or failed event at least
		 * SFC_RX_QFLUSH_POLL_WAIT_MS milliseconds and not more
		 * than 2 seconds (SFC_RX_QFLUSH_POLL_WAIT_MS multiplied
		 * by SFC_RX_QFLUSH_POLL_ATTEMPTS).
		 */
		wait_count = 0;
		do {
			rte_delay_ms(SFC_RX_QFLUSH_POLL_WAIT_MS);
			sfc_ev_qpoll(rxq->evq);
		} while ((rxq->state & SFC_RXQ_FLUSHING) &&
			 (wait_count++ < SFC_RX_QFLUSH_POLL_ATTEMPTS));

		if (rxq->state & SFC_RXQ_FLUSHING)
			sfc_err(sa, "RxQ %u flush timed out", sw_index);

		if (rxq->state & SFC_RXQ_FLUSH_FAILED)
			sfc_err(sa, "RxQ %u flush failed", sw_index);

		if (rxq->state & SFC_RXQ_FLUSHED)
			sfc_info(sa, "RxQ %u flushed", sw_index);
	}

	sfc_rx_qpurge(rxq);
}

int
sfc_rx_qstart(struct sfc_adapter *sa, unsigned int sw_index)
{
	struct sfc_rxq_info *rxq_info;
	struct sfc_rxq *rxq;
	struct sfc_evq *evq;
	int rc;

	sfc_log_init(sa, "sw_index=%u", sw_index);

	SFC_ASSERT(sw_index < sa->rxq_count);

	rxq_info = &sa->rxq_info[sw_index];
	rxq = rxq_info->rxq;
	SFC_ASSERT(rxq->state == SFC_RXQ_INITIALIZED);

	evq = rxq->evq;

	rc = sfc_ev_qstart(sa, evq->evq_index);
	if (rc != 0)
		goto fail_ev_qstart;

	rc = efx_rx_qcreate(sa->nic, rxq->hw_index, 0, rxq_info->type,
			    &rxq->mem, rxq_info->entries,
			    0 /* not used on EF10 */, evq->common,
			    &rxq->common);
	if (rc != 0)
		goto fail_rx_qcreate;

	efx_rx_qenable(rxq->common);

	rxq->pending = rxq->completed = rxq->added = rxq->pushed = 0;

	rxq->state |= (SFC_RXQ_STARTED | SFC_RXQ_RUNNING);

	sfc_rx_qrefill(rxq);

	if (sw_index == 0) {
		rc = efx_mac_filter_default_rxq_set(sa->nic, rxq->common,
						    B_FALSE);
		if (rc != 0)
			goto fail_mac_filter_default_rxq_set;
	}

	/* It seems to be used by DPDK for debug purposes only ('rte_ether') */
	sa->eth_dev->data->rx_queue_state[sw_index] =
		RTE_ETH_QUEUE_STATE_STARTED;

	return 0;

fail_mac_filter_default_rxq_set:
	sfc_rx_qflush(sa, sw_index);

fail_rx_qcreate:
	sfc_ev_qstop(sa, evq->evq_index);

fail_ev_qstart:
	return rc;
}

void
sfc_rx_qstop(struct sfc_adapter *sa, unsigned int sw_index)
{
	struct sfc_rxq_info *rxq_info;
	struct sfc_rxq *rxq;

	sfc_log_init(sa, "sw_index=%u", sw_index);

	SFC_ASSERT(sw_index < sa->rxq_count);

	rxq_info = &sa->rxq_info[sw_index];
	rxq = rxq_info->rxq;
	SFC_ASSERT(rxq->state & SFC_RXQ_STARTED);

	/* It seems to be used by DPDK for debug purposes only ('rte_ether') */
	sa->eth_dev->data->rx_queue_state[sw_index] =
		RTE_ETH_QUEUE_STATE_STOPPED;

	rxq->state &= ~SFC_RXQ_RUNNING;

	if (sw_index == 0)
		efx_mac_filter_default_rxq_clear(sa->nic);

	sfc_rx_qflush(sa, sw_index);

	rxq->state = SFC_RXQ_INITIALIZED;

	efx_rx_qdestroy(rxq->common);

	sfc_ev_qstop(sa, rxq->evq->evq_index);
}

static int
sfc_rx_qcheck_conf(struct sfc_adapter *sa,
		   const struct rte_eth_rxconf *rx_conf)
{
	int rc = 0;

	if (rx_conf->rx_thresh.pthresh != 0 ||
	    rx_conf->rx_thresh.hthresh != 0 ||
	    rx_conf->rx_thresh.wthresh != 0) {
		sfc_err(sa,
			"RxQ prefetch/host/writeback thresholds are not supported");
		rc = EINVAL;
	}

	if (rx_conf->rx_free_thresh != 0) {
		sfc_err(sa, "RxQ free threshold is not supported");
		rc = EINVAL;
	}

	if (rx_conf->rx_drop_en == 0) {
		sfc_err(sa, "RxQ drop disable is not supported");
		rc = EINVAL;
	}

	if (rx_conf->rx_deferred_start != 0) {
		sfc_err(sa, "RxQ deferred start is not supported");
		rc = EINVAL;
	}

	return rc;
}

static unsigned int
sfc_rx_mbuf_data_alignment(struct rte_mempool *mb_pool)
{
	uint32_t data_off;
	uint32_t order;

	/* The mbuf object itself is always cache line aligned */
	order = rte_bsf32(RTE_CACHE_LINE_SIZE);

	/* Data offset from mbuf object start */
	data_off = sizeof(struct rte_mbuf) + rte_pktmbuf_priv_size(mb_pool) +
		RTE_PKTMBUF_HEADROOM;

	order = MIN(order, rte_bsf32(data_off));

	return 1u << (order - 1);
}

static uint16_t
sfc_rx_mb_pool_buf_size(struct sfc_adapter *sa, struct rte_mempool *mb_pool)
{
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(sa->nic);
	const uint32_t nic_align_start = MAX(1, encp->enc_rx_buf_align_start);
	const uint32_t nic_align_end = MAX(1, encp->enc_rx_buf_align_end);
	uint16_t buf_size;
	unsigned int buf_aligned;
	unsigned int start_alignment;
	unsigned int end_padding_alignment;

	/* Below it is assumed that both alignments are power of 2 */
	SFC_ASSERT(rte_is_power_of_2(nic_align_start));
	SFC_ASSERT(rte_is_power_of_2(nic_align_end));

	/*
	 * mbuf is always cache line aligned, double-check
	 * that it meets rx buffer start alignment requirements.
	 */

	/* Start from mbuf pool data room size */
	buf_size = rte_pktmbuf_data_room_size(mb_pool);

	/* Remove headroom */
	if (buf_size <= RTE_PKTMBUF_HEADROOM) {
		sfc_err(sa,
			"RxQ mbuf pool %s object data room size %u is smaller than headroom %u",
			mb_pool->name, buf_size, RTE_PKTMBUF_HEADROOM);
		return 0;
	}
	buf_size -= RTE_PKTMBUF_HEADROOM;

	/* Calculate guaranteed data start alignment */
	buf_aligned = sfc_rx_mbuf_data_alignment(mb_pool);

	/* Reserve space for start alignment */
	if (buf_aligned < nic_align_start) {
		start_alignment = nic_align_start - buf_aligned;
		if (buf_size <= start_alignment) {
			sfc_err(sa,
				"RxQ mbuf pool %s object data room size %u is insufficient for headroom %u and buffer start alignment %u required by NIC",
				mb_pool->name,
				rte_pktmbuf_data_room_size(mb_pool),
				RTE_PKTMBUF_HEADROOM, start_alignment);
			return 0;
		}
		buf_aligned = nic_align_start;
		buf_size -= start_alignment;
	} else {
		start_alignment = 0;
	}

	/* Make sure that end padding does not write beyond the buffer */
	if (buf_aligned < nic_align_end) {
		/*
		 * Estimate space which can be lost. If guarnteed buffer
		 * size is odd, lost space is (nic_align_end - 1). More
		 * accurate formula is below.
		 */
		end_padding_alignment = nic_align_end -
			MIN(buf_aligned, 1u << (rte_bsf32(buf_size) - 1));
		if (buf_size <= end_padding_alignment) {
			sfc_err(sa,
				"RxQ mbuf pool %s object data room size %u is insufficient for headroom %u, buffer start alignment %u and end padding alignment %u required by NIC",
				mb_pool->name,
				rte_pktmbuf_data_room_size(mb_pool),
				RTE_PKTMBUF_HEADROOM, start_alignment,
				end_padding_alignment);
			return 0;
		}
		buf_size -= end_padding_alignment;
	} else {
		/*
		 * Start is aligned the same or better than end,
		 * just align length.
		 */
		buf_size = P2ALIGN(buf_size, nic_align_end);
	}

	return buf_size;
}

int
sfc_rx_qinit(struct sfc_adapter *sa, unsigned int sw_index,
	     uint16_t nb_rx_desc, unsigned int socket_id,
	     const struct rte_eth_rxconf *rx_conf,
	     struct rte_mempool *mb_pool)
{
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(sa->nic);
	int rc;
	uint16_t buf_size;
	struct sfc_rxq_info *rxq_info;
	unsigned int evq_index;
	struct sfc_evq *evq;
	struct sfc_rxq *rxq;

	rc = sfc_rx_qcheck_conf(sa, rx_conf);
	if (rc != 0)
		goto fail_bad_conf;

	buf_size = sfc_rx_mb_pool_buf_size(sa, mb_pool);
	if (buf_size == 0) {
		sfc_err(sa, "RxQ %u mbuf pool object size is too small",
			sw_index);
		rc = EINVAL;
		goto fail_bad_conf;
	}

	if ((buf_size < sa->port.pdu + encp->enc_rx_prefix_size) &&
	    !sa->eth_dev->data->dev_conf.rxmode.enable_scatter) {
		sfc_err(sa, "Rx scatter is disabled and RxQ %u mbuf pool "
			"object size is too small", sw_index);
		sfc_err(sa, "RxQ %u calculated Rx buffer size is %u vs "
			"PDU size %u plus Rx prefix %u bytes",
			sw_index, buf_size, (unsigned int)sa->port.pdu,
			encp->enc_rx_prefix_size);
		rc = EINVAL;
		goto fail_bad_conf;
	}

	SFC_ASSERT(sw_index < sa->rxq_count);
	rxq_info = &sa->rxq_info[sw_index];

	SFC_ASSERT(nb_rx_desc <= rxq_info->max_entries);
	rxq_info->entries = nb_rx_desc;
	rxq_info->type = EFX_RXQ_TYPE_DEFAULT;

	evq_index = sfc_evq_index_by_rxq_sw_index(sa, sw_index);

	rc = sfc_ev_qinit(sa, evq_index, rxq_info->entries, socket_id);
	if (rc != 0)
		goto fail_ev_qinit;

	evq = sa->evq_info[evq_index].evq;

	rc = ENOMEM;
	rxq = rte_zmalloc_socket("sfc-rxq", sizeof(*rxq), RTE_CACHE_LINE_SIZE,
				 socket_id);
	if (rxq == NULL)
		goto fail_rxq_alloc;

	rc = sfc_dma_alloc(sa, "rxq", sw_index, EFX_RXQ_SIZE(rxq_info->entries),
			   socket_id, &rxq->mem);
	if (rc != 0)
		goto fail_dma_alloc;

	rc = ENOMEM;
	rxq->sw_desc = rte_calloc_socket("sfc-rxq-sw_desc", rxq_info->entries,
					 sizeof(*rxq->sw_desc),
					 RTE_CACHE_LINE_SIZE, socket_id);
	if (rxq->sw_desc == NULL)
		goto fail_desc_alloc;

	evq->rxq = rxq;
	rxq->evq = evq;
	rxq->ptr_mask = rxq_info->entries - 1;
	rxq->refill_mb_pool = mb_pool;
	rxq->buf_size = buf_size;
	rxq->hw_index = sw_index;
	rxq->port_id = sa->eth_dev->data->port_id;

	/* Cache limits required on datapath in RxQ structure */
	rxq->batch_max = encp->enc_rx_batch_max;
	rxq->prefix_size = encp->enc_rx_prefix_size;

	rxq->state = SFC_RXQ_INITIALIZED;

	rxq_info->rxq = rxq;

	return 0;

fail_desc_alloc:
	sfc_dma_free(sa, &rxq->mem);

fail_dma_alloc:
	rte_free(rxq);

fail_rxq_alloc:
	sfc_ev_qfini(sa, evq_index);

fail_ev_qinit:
	rxq_info->entries = 0;

fail_bad_conf:
	sfc_log_init(sa, "failed %d", rc);
	return rc;
}

void
sfc_rx_qfini(struct sfc_adapter *sa, unsigned int sw_index)
{
	struct sfc_rxq_info *rxq_info;
	struct sfc_rxq *rxq;

	SFC_ASSERT(sw_index < sa->rxq_count);

	rxq_info = &sa->rxq_info[sw_index];

	rxq = rxq_info->rxq;
	SFC_ASSERT(rxq->state == SFC_RXQ_INITIALIZED);

	rxq_info->rxq = NULL;
	rxq_info->entries = 0;

	rte_free(rxq->sw_desc);
	sfc_dma_free(sa, &rxq->mem);
	rte_free(rxq);
}

int
sfc_rx_start(struct sfc_adapter *sa)
{
	unsigned int sw_index;
	int rc;

	sfc_log_init(sa, "rxq_count=%u", sa->rxq_count);

	rc = efx_rx_init(sa->nic);
	if (rc != 0)
		goto fail_rx_init;

	for (sw_index = 0; sw_index < sa->rxq_count; ++sw_index) {
		rc = sfc_rx_qstart(sa, sw_index);
		if (rc != 0)
			goto fail_rx_qstart;
	}

	return 0;

fail_rx_qstart:
	while (sw_index-- > 0)
		sfc_rx_qstop(sa, sw_index);

	efx_rx_fini(sa->nic);

fail_rx_init:
	sfc_log_init(sa, "failed %d", rc);
	return rc;
}

void
sfc_rx_stop(struct sfc_adapter *sa)
{
	unsigned int sw_index;

	sfc_log_init(sa, "rxq_count=%u", sa->rxq_count);

	sw_index = sa->rxq_count;
	while (sw_index-- > 0) {
		if (sa->rxq_info[sw_index].rxq != NULL)
			sfc_rx_qstop(sa, sw_index);
	}

	efx_rx_fini(sa->nic);
}

static int
sfc_rx_qinit_info(struct sfc_adapter *sa, unsigned int sw_index)
{
	struct sfc_rxq_info *rxq_info = &sa->rxq_info[sw_index];
	unsigned int max_entries;

	max_entries = EFX_RXQ_MAXNDESCS;
	SFC_ASSERT(rte_is_power_of_2(max_entries));

	rxq_info->max_entries = max_entries;

	return 0;
}

static int
sfc_rx_check_mode(struct sfc_adapter *sa, struct rte_eth_rxmode *rxmode)
{
	int rc = 0;

	switch (rxmode->mq_mode) {
	case ETH_MQ_RX_NONE:
		/* No special checks are required */
		break;
	default:
		sfc_err(sa, "Rx multi-queue mode %u not supported",
			rxmode->mq_mode);
		rc = EINVAL;
	}

	if (rxmode->header_split) {
		sfc_err(sa, "Header split on Rx not supported");
		rc = EINVAL;
	}

	if (rxmode->hw_vlan_filter) {
		sfc_err(sa, "HW VLAN filtering not supported");
		rc = EINVAL;
	}

	if (rxmode->hw_vlan_strip) {
		sfc_err(sa, "HW VLAN stripping not supported");
		rc = EINVAL;
	}

	if (rxmode->hw_vlan_extend) {
		sfc_err(sa,
			"Q-in-Q HW VLAN stripping not supported");
		rc = EINVAL;
	}

	if (!rxmode->hw_strip_crc) {
		sfc_warn(sa,
			 "FCS stripping control not supported - always stripped");
		rxmode->hw_strip_crc = 1;
	}

	if (rxmode->enable_scatter) {
		sfc_err(sa, "Scatter on Rx not supported");
		rc = EINVAL;
	}

	if (rxmode->enable_lro) {
		sfc_err(sa, "LRO not supported");
		rc = EINVAL;
	}

	return rc;
}

/**
 * Initialize Rx subsystem.
 *
 * Called at device configuration stage when number of receive queues is
 * specified together with other device level receive configuration.
 *
 * It should be used to allocate NUMA-unaware resources.
 */
int
sfc_rx_init(struct sfc_adapter *sa)
{
	struct rte_eth_conf *dev_conf = &sa->eth_dev->data->dev_conf;
	unsigned int sw_index;
	int rc;

	rc = sfc_rx_check_mode(sa, &dev_conf->rxmode);
	if (rc != 0)
		goto fail_check_mode;

	sa->rxq_count = sa->eth_dev->data->nb_rx_queues;

	rc = ENOMEM;
	sa->rxq_info = rte_calloc_socket("sfc-rxqs", sa->rxq_count,
					 sizeof(struct sfc_rxq_info), 0,
					 sa->socket_id);
	if (sa->rxq_info == NULL)
		goto fail_rxqs_alloc;

	for (sw_index = 0; sw_index < sa->rxq_count; ++sw_index) {
		rc = sfc_rx_qinit_info(sa, sw_index);
		if (rc != 0)
			goto fail_rx_qinit_info;
	}

	return 0;

fail_rx_qinit_info:
	rte_free(sa->rxq_info);
	sa->rxq_info = NULL;

fail_rxqs_alloc:
	sa->rxq_count = 0;
fail_check_mode:
	sfc_log_init(sa, "failed %d", rc);
	return rc;
}

/**
 * Shutdown Rx subsystem.
 *
 * Called at device close stage, for example, before device
 * reconfiguration or shutdown.
 */
void
sfc_rx_fini(struct sfc_adapter *sa)
{
	unsigned int sw_index;

	sw_index = sa->rxq_count;
	while (sw_index-- > 0) {
		if (sa->rxq_info[sw_index].rxq != NULL)
			sfc_rx_qfini(sa, sw_index);
	}

	rte_free(sa->rxq_info);
	sa->rxq_info = NULL;
	sa->rxq_count = 0;
}
