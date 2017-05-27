/*-
 *   BSD LICENSE
 *
 * Copyright (c) 2016-2017 Solarflare Communications Inc.
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

#include "sfc.h"
#include "sfc_debug.h"
#include "sfc_log.h"
#include "sfc_ev.h"
#include "sfc_tx.h"
#include "sfc_tweak.h"
#include "sfc_kvargs.h"

/*
 * Maximum number of TX queue flush attempts in case of
 * failure or flush timeout
 */
#define SFC_TX_QFLUSH_ATTEMPTS		(3)

/*
 * Time to wait between event queue polling attempts when waiting for TX
 * queue flush done or flush failed events
 */
#define SFC_TX_QFLUSH_POLL_WAIT_MS	(1)

/*
 * Maximum number of event queue polling attempts when waiting for TX queue
 * flush done or flush failed events; it defines TX queue flush attempt timeout
 * together with SFC_TX_QFLUSH_POLL_WAIT_MS
 */
#define SFC_TX_QFLUSH_POLL_ATTEMPTS	(2000)

static int
sfc_tx_qcheck_conf(struct sfc_adapter *sa, uint16_t nb_tx_desc,
		   const struct rte_eth_txconf *tx_conf)
{
	unsigned int flags = tx_conf->txq_flags;
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(sa->nic);
	int rc = 0;

	if (tx_conf->tx_rs_thresh != 0) {
		sfc_err(sa, "RS bit in transmit descriptor is not supported");
		rc = EINVAL;
	}

	if (tx_conf->tx_free_thresh > EFX_TXQ_LIMIT(nb_tx_desc)) {
		sfc_err(sa,
			"TxQ free threshold too large: %u vs maximum %u",
			tx_conf->tx_free_thresh, EFX_TXQ_LIMIT(nb_tx_desc));
		rc = EINVAL;
	}

	if (tx_conf->tx_thresh.pthresh != 0 ||
	    tx_conf->tx_thresh.hthresh != 0 ||
	    tx_conf->tx_thresh.wthresh != 0) {
		sfc_err(sa,
			"prefetch/host/writeback thresholds are not supported");
		rc = EINVAL;
	}

	if (((flags & ETH_TXQ_FLAGS_NOMULTSEGS) == 0) &&
	    (~sa->dp_tx->features & SFC_DP_TX_FEAT_MULTI_SEG)) {
		sfc_err(sa, "Multi-segment is not supported by %s datapath",
			sa->dp_tx->dp.name);
		rc = EINVAL;
	}

	if ((flags & ETH_TXQ_FLAGS_NOVLANOFFL) == 0) {
		if (!encp->enc_hw_tx_insert_vlan_enabled) {
			sfc_err(sa, "VLAN offload is not supported");
			rc = EINVAL;
		} else if (~sa->dp_tx->features & SFC_DP_TX_FEAT_VLAN_INSERT) {
			sfc_err(sa,
				"VLAN offload is not supported by %s datapath",
				sa->dp_tx->dp.name);
			rc = EINVAL;
		}
	}

	if ((flags & ETH_TXQ_FLAGS_NOXSUMSCTP) == 0) {
		sfc_err(sa, "SCTP offload is not supported");
		rc = EINVAL;
	}

	/* We either perform both TCP and UDP offload, or no offload at all */
	if (((flags & ETH_TXQ_FLAGS_NOXSUMTCP) == 0) !=
	    ((flags & ETH_TXQ_FLAGS_NOXSUMUDP) == 0)) {
		sfc_err(sa, "TCP and UDP offloads can't be set independently");
		rc = EINVAL;
	}

	return rc;
}

void
sfc_tx_qflush_done(struct sfc_txq *txq)
{
	txq->state |= SFC_TXQ_FLUSHED;
	txq->state &= ~SFC_TXQ_FLUSHING;
}

int
sfc_tx_qinit(struct sfc_adapter *sa, unsigned int sw_index,
	     uint16_t nb_tx_desc, unsigned int socket_id,
	     const struct rte_eth_txconf *tx_conf)
{
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(sa->nic);
	struct sfc_txq_info *txq_info;
	struct sfc_evq *evq;
	struct sfc_txq *txq;
	int rc = 0;
	struct sfc_dp_tx_qcreate_info info;

	sfc_log_init(sa, "TxQ = %u", sw_index);

	rc = sfc_tx_qcheck_conf(sa, nb_tx_desc, tx_conf);
	if (rc != 0)
		goto fail_bad_conf;

	SFC_ASSERT(sw_index < sa->txq_count);
	txq_info = &sa->txq_info[sw_index];

	SFC_ASSERT(nb_tx_desc <= sa->txq_max_entries);
	txq_info->entries = nb_tx_desc;

	rc = sfc_ev_qinit(sa, SFC_EVQ_TYPE_TX, sw_index,
			  txq_info->entries, socket_id, &evq);
	if (rc != 0)
		goto fail_ev_qinit;

	rc = ENOMEM;
	txq = rte_zmalloc_socket("sfc-txq", sizeof(*txq), 0, socket_id);
	if (txq == NULL)
		goto fail_txq_alloc;

	txq_info->txq = txq;

	txq->hw_index = sw_index;
	txq->evq = evq;
	txq->free_thresh =
		(tx_conf->tx_free_thresh) ? tx_conf->tx_free_thresh :
		SFC_TX_DEFAULT_FREE_THRESH;
	txq->flags = tx_conf->txq_flags;

	rc = sfc_dma_alloc(sa, "txq", sw_index, EFX_TXQ_SIZE(txq_info->entries),
			   socket_id, &txq->mem);
	if (rc != 0)
		goto fail_dma_alloc;

	memset(&info, 0, sizeof(info));
	info.free_thresh = txq->free_thresh;
	info.flags = tx_conf->txq_flags;
	info.txq_entries = txq_info->entries;
	info.dma_desc_size_max = encp->enc_tx_dma_desc_size_max;
	info.txq_hw_ring = txq->mem.esm_base;
	info.evq_entries = txq_info->entries;
	info.evq_hw_ring = evq->mem.esm_base;
	info.hw_index = txq->hw_index;
	info.mem_bar = sa->mem_bar.esb_base;

	rc = sa->dp_tx->qcreate(sa->eth_dev->data->port_id, sw_index,
				&RTE_ETH_DEV_TO_PCI(sa->eth_dev)->addr,
				socket_id, &info, &txq->dp);
	if (rc != 0)
		goto fail_dp_tx_qinit;

	evq->dp_txq = txq->dp;

	txq->state = SFC_TXQ_INITIALIZED;

	txq_info->deferred_start = (tx_conf->tx_deferred_start != 0);

	return 0;

fail_dp_tx_qinit:
	sfc_dma_free(sa, &txq->mem);

fail_dma_alloc:
	txq_info->txq = NULL;
	rte_free(txq);

fail_txq_alloc:
	sfc_ev_qfini(evq);

fail_ev_qinit:
	txq_info->entries = 0;

fail_bad_conf:
	sfc_log_init(sa, "failed (TxQ = %u, rc = %d)", sw_index, rc);
	return rc;
}

void
sfc_tx_qfini(struct sfc_adapter *sa, unsigned int sw_index)
{
	struct sfc_txq_info *txq_info;
	struct sfc_txq *txq;

	sfc_log_init(sa, "TxQ = %u", sw_index);

	SFC_ASSERT(sw_index < sa->txq_count);
	txq_info = &sa->txq_info[sw_index];

	txq = txq_info->txq;
	SFC_ASSERT(txq != NULL);
	SFC_ASSERT(txq->state == SFC_TXQ_INITIALIZED);

	sa->dp_tx->qdestroy(txq->dp);
	txq->dp = NULL;

	txq_info->txq = NULL;
	txq_info->entries = 0;

	sfc_dma_free(sa, &txq->mem);

	sfc_ev_qfini(txq->evq);
	txq->evq = NULL;

	rte_free(txq);
}

static int
sfc_tx_qinit_info(struct sfc_adapter *sa, unsigned int sw_index)
{
	sfc_log_init(sa, "TxQ = %u", sw_index);

	return 0;
}

static int
sfc_tx_check_mode(struct sfc_adapter *sa, const struct rte_eth_txmode *txmode)
{
	int rc = 0;

	switch (txmode->mq_mode) {
	case ETH_MQ_TX_NONE:
		break;
	default:
		sfc_err(sa, "Tx multi-queue mode %u not supported",
			txmode->mq_mode);
		rc = EINVAL;
	}

	/*
	 * These features are claimed to be i40e-specific,
	 * but it does make sense to double-check their absence
	 */
	if (txmode->hw_vlan_reject_tagged) {
		sfc_err(sa, "Rejecting tagged packets not supported");
		rc = EINVAL;
	}

	if (txmode->hw_vlan_reject_untagged) {
		sfc_err(sa, "Rejecting untagged packets not supported");
		rc = EINVAL;
	}

	if (txmode->hw_vlan_insert_pvid) {
		sfc_err(sa, "Port-based VLAN insertion not supported");
		rc = EINVAL;
	}

	return rc;
}

/**
 * Destroy excess queues that are no longer needed after reconfiguration
 * or complete close.
 */
static void
sfc_tx_fini_queues(struct sfc_adapter *sa, unsigned int nb_tx_queues)
{
	int sw_index;

	SFC_ASSERT(nb_tx_queues <= sa->txq_count);

	sw_index = sa->txq_count;
	while (--sw_index >= (int)nb_tx_queues) {
		if (sa->txq_info[sw_index].txq != NULL)
			sfc_tx_qfini(sa, sw_index);
	}

	sa->txq_count = nb_tx_queues;
}

int
sfc_tx_configure(struct sfc_adapter *sa)
{
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(sa->nic);
	const struct rte_eth_conf *dev_conf = &sa->eth_dev->data->dev_conf;
	const unsigned int nb_tx_queues = sa->eth_dev->data->nb_tx_queues;
	int rc = 0;

	sfc_log_init(sa, "nb_tx_queues=%u (old %u)",
		     nb_tx_queues, sa->txq_count);

	/*
	 * The datapath implementation assumes absence of boundary
	 * limits on Tx DMA descriptors. Addition of these checks on
	 * datapath would simply make the datapath slower.
	 */
	if (encp->enc_tx_dma_desc_boundary != 0) {
		rc = ENOTSUP;
		goto fail_tx_dma_desc_boundary;
	}

	rc = sfc_tx_check_mode(sa, &dev_conf->txmode);
	if (rc != 0)
		goto fail_check_mode;

	if (nb_tx_queues == sa->txq_count)
		goto done;

	if (sa->txq_info == NULL) {
		sa->txq_info = rte_calloc_socket("sfc-txqs", nb_tx_queues,
						 sizeof(sa->txq_info[0]), 0,
						 sa->socket_id);
		if (sa->txq_info == NULL)
			goto fail_txqs_alloc;
	} else {
		struct sfc_txq_info *new_txq_info;

		if (nb_tx_queues < sa->txq_count)
			sfc_tx_fini_queues(sa, nb_tx_queues);

		new_txq_info =
			rte_realloc(sa->txq_info,
				    nb_tx_queues * sizeof(sa->txq_info[0]), 0);
		if (new_txq_info == NULL && nb_tx_queues > 0)
			goto fail_txqs_realloc;

		sa->txq_info = new_txq_info;
		if (nb_tx_queues > sa->txq_count)
			memset(&sa->txq_info[sa->txq_count], 0,
			       (nb_tx_queues - sa->txq_count) *
			       sizeof(sa->txq_info[0]));
	}

	while (sa->txq_count < nb_tx_queues) {
		rc = sfc_tx_qinit_info(sa, sa->txq_count);
		if (rc != 0)
			goto fail_tx_qinit_info;

		sa->txq_count++;
	}

done:
	return 0;

fail_tx_qinit_info:
fail_txqs_realloc:
fail_txqs_alloc:
	sfc_tx_close(sa);

fail_check_mode:
fail_tx_dma_desc_boundary:
	sfc_log_init(sa, "failed (rc = %d)", rc);
	return rc;
}

void
sfc_tx_close(struct sfc_adapter *sa)
{
	sfc_tx_fini_queues(sa, 0);

	rte_free(sa->txq_info);
	sa->txq_info = NULL;
}

int
sfc_tx_qstart(struct sfc_adapter *sa, unsigned int sw_index)
{
	struct rte_eth_dev_data *dev_data;
	struct sfc_txq_info *txq_info;
	struct sfc_txq *txq;
	struct sfc_evq *evq;
	uint16_t flags;
	unsigned int desc_index;
	int rc = 0;

	sfc_log_init(sa, "TxQ = %u", sw_index);

	SFC_ASSERT(sw_index < sa->txq_count);
	txq_info = &sa->txq_info[sw_index];

	txq = txq_info->txq;

	SFC_ASSERT(txq->state == SFC_TXQ_INITIALIZED);

	evq = txq->evq;

	rc = sfc_ev_qstart(evq, sfc_evq_index_by_txq_sw_index(sa, sw_index));
	if (rc != 0)
		goto fail_ev_qstart;

	/*
	 * It seems that DPDK has no controls regarding IPv4 offloads,
	 * hence, we always enable it here
	 */
	if ((txq->flags & ETH_TXQ_FLAGS_NOXSUMTCP) ||
	    (txq->flags & ETH_TXQ_FLAGS_NOXSUMUDP)) {
		flags = EFX_TXQ_CKSUM_IPV4;
	} else {
		flags = EFX_TXQ_CKSUM_IPV4 | EFX_TXQ_CKSUM_TCPUDP;

		if (sa->tso)
			flags |= EFX_TXQ_FATSOV2;
	}

	rc = efx_tx_qcreate(sa->nic, sw_index, 0, &txq->mem,
			    txq_info->entries, 0 /* not used on EF10 */,
			    flags, evq->common,
			    &txq->common, &desc_index);
	if (rc != 0) {
		if (sa->tso && (rc == ENOSPC))
			sfc_err(sa, "ran out of TSO contexts");

		goto fail_tx_qcreate;
	}

	efx_tx_qenable(txq->common);

	txq->state |= SFC_TXQ_STARTED;

	rc = sa->dp_tx->qstart(txq->dp, evq->read_ptr, desc_index);
	if (rc != 0)
		goto fail_dp_qstart;

	/*
	 * It seems to be used by DPDK for debug purposes only ('rte_ether')
	 */
	dev_data = sa->eth_dev->data;
	dev_data->tx_queue_state[sw_index] = RTE_ETH_QUEUE_STATE_STARTED;

	return 0;

fail_dp_qstart:
	txq->state = SFC_TXQ_INITIALIZED;
	efx_tx_qdestroy(txq->common);

fail_tx_qcreate:
	sfc_ev_qstop(evq);

fail_ev_qstart:
	return rc;
}

void
sfc_tx_qstop(struct sfc_adapter *sa, unsigned int sw_index)
{
	struct rte_eth_dev_data *dev_data;
	struct sfc_txq_info *txq_info;
	struct sfc_txq *txq;
	unsigned int retry_count;
	unsigned int wait_count;
	int rc;

	sfc_log_init(sa, "TxQ = %u", sw_index);

	SFC_ASSERT(sw_index < sa->txq_count);
	txq_info = &sa->txq_info[sw_index];

	txq = txq_info->txq;

	if (txq->state == SFC_TXQ_INITIALIZED)
		return;

	SFC_ASSERT(txq->state & SFC_TXQ_STARTED);

	sa->dp_tx->qstop(txq->dp, &txq->evq->read_ptr);

	/*
	 * Retry TX queue flushing in case of flush failed or
	 * timeout; in the worst case it can delay for 6 seconds
	 */
	for (retry_count = 0;
	     ((txq->state & SFC_TXQ_FLUSHED) == 0) &&
	     (retry_count < SFC_TX_QFLUSH_ATTEMPTS);
	     ++retry_count) {
		rc = efx_tx_qflush(txq->common);
		if (rc != 0) {
			txq->state |= (rc == EALREADY) ?
				SFC_TXQ_FLUSHED : SFC_TXQ_FLUSH_FAILED;
			break;
		}

		/*
		 * Wait for TX queue flush done or flush failed event at least
		 * SFC_TX_QFLUSH_POLL_WAIT_MS milliseconds and not more
		 * than 2 seconds (SFC_TX_QFLUSH_POLL_WAIT_MS multiplied
		 * by SFC_TX_QFLUSH_POLL_ATTEMPTS)
		 */
		wait_count = 0;
		do {
			rte_delay_ms(SFC_TX_QFLUSH_POLL_WAIT_MS);
			sfc_ev_qpoll(txq->evq);
		} while ((txq->state & SFC_TXQ_FLUSHING) &&
			 wait_count++ < SFC_TX_QFLUSH_POLL_ATTEMPTS);

		if (txq->state & SFC_TXQ_FLUSHING)
			sfc_err(sa, "TxQ %u flush timed out", sw_index);

		if (txq->state & SFC_TXQ_FLUSHED)
			sfc_info(sa, "TxQ %u flushed", sw_index);
	}

	sa->dp_tx->qreap(txq->dp);

	txq->state = SFC_TXQ_INITIALIZED;

	efx_tx_qdestroy(txq->common);

	sfc_ev_qstop(txq->evq);

	/*
	 * It seems to be used by DPDK for debug purposes only ('rte_ether')
	 */
	dev_data = sa->eth_dev->data;
	dev_data->tx_queue_state[sw_index] = RTE_ETH_QUEUE_STATE_STOPPED;
}

int
sfc_tx_start(struct sfc_adapter *sa)
{
	unsigned int sw_index;
	int rc = 0;

	sfc_log_init(sa, "txq_count = %u", sa->txq_count);

	if (sa->tso) {
		if (!efx_nic_cfg_get(sa->nic)->enc_fw_assisted_tso_v2_enabled) {
			sfc_warn(sa, "TSO support was unable to be restored");
			sa->tso = B_FALSE;
		}
	}

	rc = efx_tx_init(sa->nic);
	if (rc != 0)
		goto fail_efx_tx_init;

	for (sw_index = 0; sw_index < sa->txq_count; ++sw_index) {
		if (!(sa->txq_info[sw_index].deferred_start) ||
		    sa->txq_info[sw_index].deferred_started) {
			rc = sfc_tx_qstart(sa, sw_index);
			if (rc != 0)
				goto fail_tx_qstart;
		}
	}

	return 0;

fail_tx_qstart:
	while (sw_index-- > 0)
		sfc_tx_qstop(sa, sw_index);

	efx_tx_fini(sa->nic);

fail_efx_tx_init:
	sfc_log_init(sa, "failed (rc = %d)", rc);
	return rc;
}

void
sfc_tx_stop(struct sfc_adapter *sa)
{
	unsigned int sw_index;

	sfc_log_init(sa, "txq_count = %u", sa->txq_count);

	sw_index = sa->txq_count;
	while (sw_index-- > 0) {
		if (sa->txq_info[sw_index].txq != NULL)
			sfc_tx_qstop(sa, sw_index);
	}

	efx_tx_fini(sa->nic);
}

static void
sfc_efx_tx_reap(struct sfc_efx_txq *txq)
{
	unsigned int completed;

	sfc_ev_qpoll(txq->evq);

	for (completed = txq->completed;
	     completed != txq->pending; completed++) {
		struct sfc_efx_tx_sw_desc *txd;

		txd = &txq->sw_ring[completed & txq->ptr_mask];

		if (txd->mbuf != NULL) {
			rte_pktmbuf_free(txd->mbuf);
			txd->mbuf = NULL;
		}
	}

	txq->completed = completed;
}

/*
 * The function is used to insert or update VLAN tag;
 * the firmware has state of the firmware tag to insert per TxQ
 * (controlled by option descriptors), hence, if the tag of the
 * packet to be sent is different from one remembered by the firmware,
 * the function will update it
 */
static unsigned int
sfc_efx_tx_maybe_insert_tag(struct sfc_efx_txq *txq, struct rte_mbuf *m,
			    efx_desc_t **pend)
{
	uint16_t this_tag = ((m->ol_flags & PKT_TX_VLAN_PKT) ?
			     m->vlan_tci : 0);

	if (this_tag == txq->hw_vlan_tci)
		return 0;

	/*
	 * The expression inside SFC_ASSERT() is not desired to be checked in
	 * a non-debug build because it might be too expensive on the data path
	 */
	SFC_ASSERT(efx_nic_cfg_get(txq->evq->sa->nic)->enc_hw_tx_insert_vlan_enabled);

	efx_tx_qdesc_vlantci_create(txq->common, rte_cpu_to_be_16(this_tag),
				    *pend);
	(*pend)++;
	txq->hw_vlan_tci = this_tag;

	return 1;
}

static uint16_t
sfc_efx_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct sfc_dp_txq *dp_txq = (struct sfc_dp_txq *)tx_queue;
	struct sfc_efx_txq *txq = sfc_efx_txq_by_dp_txq(dp_txq);
	unsigned int added = txq->added;
	unsigned int pushed = added;
	unsigned int pkts_sent = 0;
	efx_desc_t *pend = &txq->pend_desc[0];
	const unsigned int hard_max_fill = EFX_TXQ_LIMIT(txq->ptr_mask + 1);
	const unsigned int soft_max_fill = hard_max_fill - txq->free_thresh;
	unsigned int fill_level = added - txq->completed;
	boolean_t reap_done;
	int rc __rte_unused;
	struct rte_mbuf **pktp;

	if (unlikely((txq->flags & SFC_EFX_TXQ_FLAG_RUNNING) == 0))
		goto done;

	/*
	 * If insufficient space for a single packet is present,
	 * we should reap; otherwise, we shouldn't do that all the time
	 * to avoid latency increase
	 */
	reap_done = (fill_level > soft_max_fill);

	if (reap_done) {
		sfc_efx_tx_reap(txq);
		/*
		 * Recalculate fill level since 'txq->completed'
		 * might have changed on reap
		 */
		fill_level = added - txq->completed;
	}

	for (pkts_sent = 0, pktp = &tx_pkts[0];
	     (pkts_sent < nb_pkts) && (fill_level <= soft_max_fill);
	     pkts_sent++, pktp++) {
		struct rte_mbuf		*m_seg = *pktp;
		size_t			pkt_len = m_seg->pkt_len;
		unsigned int		pkt_descs = 0;
		size_t			in_off = 0;

		/*
		 * Here VLAN TCI is expected to be zero in case if no
		 * DEV_TX_VLAN_OFFLOAD capability is advertised;
		 * if the calling app ignores the absence of
		 * DEV_TX_VLAN_OFFLOAD and pushes VLAN TCI, then
		 * TX_ERROR will occur
		 */
		pkt_descs += sfc_efx_tx_maybe_insert_tag(txq, m_seg, &pend);

		if (m_seg->ol_flags & PKT_TX_TCP_SEG) {
			/*
			 * We expect correct 'pkt->l[2, 3, 4]_len' values
			 * to be set correctly by the caller
			 */
			if (sfc_efx_tso_do(txq, added, &m_seg, &in_off, &pend,
					   &pkt_descs, &pkt_len) != 0) {
				/* We may have reached this place for
				 * one of the following reasons:
				 *
				 * 1) Packet header length is greater
				 *    than SFC_TSOH_STD_LEN
				 * 2) TCP header starts at more then
				 *    208 bytes into the frame
				 *
				 * We will deceive RTE saying that we have sent
				 * the packet, but we will actually drop it.
				 * Hence, we should revert 'pend' to the
				 * previous state (in case we have added
				 * VLAN descriptor) and start processing
				 * another one packet. But the original
				 * mbuf shouldn't be orphaned
				 */
				pend -= pkt_descs;

				rte_pktmbuf_free(*pktp);

				continue;
			}

			/*
			 * We've only added 2 FATSOv2 option descriptors
			 * and 1 descriptor for the linearized packet header.
			 * The outstanding work will be done in the same manner
			 * as for the usual non-TSO path
			 */
		}

		for (; m_seg != NULL; m_seg = m_seg->next) {
			efsys_dma_addr_t	next_frag;
			size_t			seg_len;

			seg_len = m_seg->data_len;
			next_frag = rte_mbuf_data_dma_addr(m_seg);

			/*
			 * If we've started TSO transaction few steps earlier,
			 * we'll skip packet header using an offset in the
			 * current segment (which has been set to the
			 * first one containing payload)
			 */
			seg_len -= in_off;
			next_frag += in_off;
			in_off = 0;

			do {
				efsys_dma_addr_t	frag_addr = next_frag;
				size_t			frag_len;

				/*
				 * It is assumed here that there is no
				 * limitation on address boundary
				 * crossing by DMA descriptor.
				 */
				frag_len = MIN(seg_len, txq->dma_desc_size_max);
				next_frag += frag_len;
				seg_len -= frag_len;
				pkt_len -= frag_len;

				efx_tx_qdesc_dma_create(txq->common,
							frag_addr, frag_len,
							(pkt_len == 0),
							pend++);

				pkt_descs++;
			} while (seg_len != 0);
		}

		added += pkt_descs;

		fill_level += pkt_descs;
		if (unlikely(fill_level > hard_max_fill)) {
			/*
			 * Our estimation for maximum number of descriptors
			 * required to send a packet seems to be wrong.
			 * Try to reap (if we haven't yet).
			 */
			if (!reap_done) {
				sfc_efx_tx_reap(txq);
				reap_done = B_TRUE;
				fill_level = added - txq->completed;
				if (fill_level > hard_max_fill) {
					pend -= pkt_descs;
					break;
				}
			} else {
				pend -= pkt_descs;
				break;
			}
		}

		/* Assign mbuf to the last used desc */
		txq->sw_ring[(added - 1) & txq->ptr_mask].mbuf = *pktp;
	}

	if (likely(pkts_sent > 0)) {
		rc = efx_tx_qdesc_post(txq->common, txq->pend_desc,
				       pend - &txq->pend_desc[0],
				       txq->completed, &txq->added);
		SFC_ASSERT(rc == 0);

		if (likely(pushed != txq->added))
			efx_tx_qpush(txq->common, txq->added, pushed);
	}

#if SFC_TX_XMIT_PKTS_REAP_AT_LEAST_ONCE
	if (!reap_done)
		sfc_efx_tx_reap(txq);
#endif

done:
	return pkts_sent;
}

struct sfc_txq *
sfc_txq_by_dp_txq(const struct sfc_dp_txq *dp_txq)
{
	const struct sfc_dp_queue *dpq = &dp_txq->dpq;
	struct rte_eth_dev *eth_dev;
	struct sfc_adapter *sa;
	struct sfc_txq *txq;

	SFC_ASSERT(rte_eth_dev_is_valid_port(dpq->port_id));
	eth_dev = &rte_eth_devices[dpq->port_id];

	sa = eth_dev->data->dev_private;

	SFC_ASSERT(dpq->queue_id < sa->txq_count);
	txq = sa->txq_info[dpq->queue_id].txq;

	SFC_ASSERT(txq != NULL);
	return txq;
}

static sfc_dp_tx_qcreate_t sfc_efx_tx_qcreate;
static int
sfc_efx_tx_qcreate(uint16_t port_id, uint16_t queue_id,
		   const struct rte_pci_addr *pci_addr,
		   int socket_id,
		   const struct sfc_dp_tx_qcreate_info *info,
		   struct sfc_dp_txq **dp_txqp)
{
	struct sfc_efx_txq *txq;
	struct sfc_txq *ctrl_txq;
	int rc;

	rc = ENOMEM;
	txq = rte_zmalloc_socket("sfc-efx-txq", sizeof(*txq),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (txq == NULL)
		goto fail_txq_alloc;

	sfc_dp_queue_init(&txq->dp.dpq, port_id, queue_id, pci_addr);

	rc = ENOMEM;
	txq->pend_desc = rte_calloc_socket("sfc-efx-txq-pend-desc",
					   EFX_TXQ_LIMIT(info->txq_entries),
					   sizeof(*txq->pend_desc), 0,
					   socket_id);
	if (txq->pend_desc == NULL)
		goto fail_pend_desc_alloc;

	rc = ENOMEM;
	txq->sw_ring = rte_calloc_socket("sfc-efx-txq-sw_ring",
					 info->txq_entries,
					 sizeof(*txq->sw_ring),
					 RTE_CACHE_LINE_SIZE, socket_id);
	if (txq->sw_ring == NULL)
		goto fail_sw_ring_alloc;

	ctrl_txq = sfc_txq_by_dp_txq(&txq->dp);
	if (ctrl_txq->evq->sa->tso) {
		rc = sfc_efx_tso_alloc_tsoh_objs(txq->sw_ring,
						 info->txq_entries, socket_id);
		if (rc != 0)
			goto fail_alloc_tsoh_objs;
	}

	txq->evq = ctrl_txq->evq;
	txq->ptr_mask = info->txq_entries - 1;
	txq->free_thresh = info->free_thresh;
	txq->dma_desc_size_max = info->dma_desc_size_max;

	*dp_txqp = &txq->dp;
	return 0;

fail_alloc_tsoh_objs:
	rte_free(txq->sw_ring);

fail_sw_ring_alloc:
	rte_free(txq->pend_desc);

fail_pend_desc_alloc:
	rte_free(txq);

fail_txq_alloc:
	return rc;
}

static sfc_dp_tx_qdestroy_t sfc_efx_tx_qdestroy;
static void
sfc_efx_tx_qdestroy(struct sfc_dp_txq *dp_txq)
{
	struct sfc_efx_txq *txq = sfc_efx_txq_by_dp_txq(dp_txq);

	sfc_efx_tso_free_tsoh_objs(txq->sw_ring, txq->ptr_mask + 1);
	rte_free(txq->sw_ring);
	rte_free(txq->pend_desc);
	rte_free(txq);
}

static sfc_dp_tx_qstart_t sfc_efx_tx_qstart;
static int
sfc_efx_tx_qstart(struct sfc_dp_txq *dp_txq,
		  __rte_unused unsigned int evq_read_ptr,
		  unsigned int txq_desc_index)
{
	/* libefx-based datapath is specific to libefx-based PMD */
	struct sfc_efx_txq *txq = sfc_efx_txq_by_dp_txq(dp_txq);
	struct sfc_txq *ctrl_txq = sfc_txq_by_dp_txq(dp_txq);

	txq->common = ctrl_txq->common;

	txq->pending = txq->completed = txq->added = txq_desc_index;
	txq->hw_vlan_tci = 0;

	txq->flags |= (SFC_EFX_TXQ_FLAG_STARTED | SFC_EFX_TXQ_FLAG_RUNNING);

	return 0;
}

static sfc_dp_tx_qstop_t sfc_efx_tx_qstop;
static void
sfc_efx_tx_qstop(struct sfc_dp_txq *dp_txq,
		 __rte_unused unsigned int *evq_read_ptr)
{
	struct sfc_efx_txq *txq = sfc_efx_txq_by_dp_txq(dp_txq);

	txq->flags &= ~SFC_EFX_TXQ_FLAG_RUNNING;
}

static sfc_dp_tx_qreap_t sfc_efx_tx_qreap;
static void
sfc_efx_tx_qreap(struct sfc_dp_txq *dp_txq)
{
	struct sfc_efx_txq *txq = sfc_efx_txq_by_dp_txq(dp_txq);
	unsigned int txds;

	sfc_efx_tx_reap(txq);

	for (txds = 0; txds <= txq->ptr_mask; txds++) {
		if (txq->sw_ring[txds].mbuf != NULL) {
			rte_pktmbuf_free(txq->sw_ring[txds].mbuf);
			txq->sw_ring[txds].mbuf = NULL;
		}
	}

	txq->flags &= ~SFC_EFX_TXQ_FLAG_STARTED;
}

struct sfc_dp_tx sfc_efx_tx = {
	.dp = {
		.name		= SFC_KVARG_DATAPATH_EFX,
		.type		= SFC_DP_TX,
		.hw_fw_caps	= 0,
	},
	.features		= SFC_DP_TX_FEAT_VLAN_INSERT |
				  SFC_DP_TX_FEAT_TSO |
				  SFC_DP_TX_FEAT_MULTI_SEG,
	.qcreate		= sfc_efx_tx_qcreate,
	.qdestroy		= sfc_efx_tx_qdestroy,
	.qstart			= sfc_efx_tx_qstart,
	.qstop			= sfc_efx_tx_qstop,
	.qreap			= sfc_efx_tx_qreap,
	.pkt_burst		= sfc_efx_xmit_pkts,
};
