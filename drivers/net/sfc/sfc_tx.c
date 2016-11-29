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

#include "sfc.h"
#include "sfc_log.h"
#include "sfc_ev.h"
#include "sfc_tx.h"

static int
sfc_tx_qcheck_conf(struct sfc_adapter *sa,
		   const struct rte_eth_txconf *tx_conf)
{
	unsigned int flags = tx_conf->txq_flags;
	int rc = 0;

	if (tx_conf->tx_rs_thresh != 0) {
		sfc_err(sa, "RS bit in transmit descriptor is not supported");
		rc = EINVAL;
	}

	if (tx_conf->tx_free_thresh != 0) {
		sfc_err(sa,
			"setting explicit TX free threshold is not supported");
		rc = EINVAL;
	}

	if (tx_conf->tx_deferred_start != 0) {
		sfc_err(sa, "TX queue deferred start is not supported (yet)");
		rc = EINVAL;
	}

	if (tx_conf->tx_thresh.pthresh != 0 ||
	    tx_conf->tx_thresh.hthresh != 0 ||
	    tx_conf->tx_thresh.wthresh != 0) {
		sfc_err(sa,
			"prefetch/host/writeback thresholds are not supported");
		rc = EINVAL;
	}

	if ((flags & ETH_TXQ_FLAGS_NOVLANOFFL) == 0) {
		sfc_err(sa, "VLAN offload is not supported");
		rc = EINVAL;
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

int
sfc_tx_qinit(struct sfc_adapter *sa, unsigned int sw_index,
	     uint16_t nb_tx_desc, unsigned int socket_id,
	     const struct rte_eth_txconf *tx_conf)
{
	struct sfc_txq_info *txq_info;
	struct sfc_evq *evq;
	struct sfc_txq *txq;
	unsigned int evq_index = sfc_evq_index_by_txq_sw_index(sa, sw_index);
	int rc = 0;

	sfc_log_init(sa, "TxQ = %u", sw_index);

	rc = sfc_tx_qcheck_conf(sa, tx_conf);
	if (rc != 0)
		goto fail_bad_conf;

	SFC_ASSERT(sw_index < sa->txq_count);
	txq_info = &sa->txq_info[sw_index];

	SFC_ASSERT(nb_tx_desc <= sa->txq_max_entries);
	txq_info->entries = nb_tx_desc;

	rc = sfc_ev_qinit(sa, evq_index, txq_info->entries, socket_id);
	if (rc != 0)
		goto fail_ev_qinit;

	evq = sa->evq_info[evq_index].evq;

	rc = ENOMEM;
	txq = rte_zmalloc_socket("sfc-txq", sizeof(*txq), 0, socket_id);
	if (txq == NULL)
		goto fail_txq_alloc;

	rc = sfc_dma_alloc(sa, "txq", sw_index, EFX_TXQ_SIZE(txq_info->entries),
			   socket_id, &txq->mem);
	if (rc != 0)
		goto fail_dma_alloc;

	rc = ENOMEM;
	txq->pend_desc = rte_calloc_socket("sfc-txq-pend-desc",
					   EFX_TXQ_LIMIT(txq_info->entries),
					   sizeof(efx_desc_t), 0, socket_id);
	if (txq->pend_desc == NULL)
		goto fail_pend_desc_alloc;

	rc = ENOMEM;
	txq->sw_ring = rte_calloc_socket("sfc-txq-desc", txq_info->entries,
					 sizeof(*txq->sw_ring), 0, socket_id);
	if (txq->sw_ring == NULL)
		goto fail_desc_alloc;

	txq->state = SFC_TXQ_INITIALIZED;
	txq->ptr_mask = txq_info->entries - 1;
	txq->hw_index = sw_index;
	txq->flags = tx_conf->txq_flags;
	txq->evq = evq;

	evq->txq = txq;

	txq_info->txq = txq;

	return 0;

fail_desc_alloc:
	rte_free(txq->pend_desc);

fail_pend_desc_alloc:
	sfc_dma_free(sa, &txq->mem);

fail_dma_alloc:
	rte_free(txq);

fail_txq_alloc:
	sfc_ev_qfini(sa, evq_index);

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

	txq_info->txq = NULL;
	txq_info->entries = 0;

	rte_free(txq->sw_ring);
	rte_free(txq->pend_desc);
	sfc_dma_free(sa, &txq->mem);
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

int
sfc_tx_init(struct sfc_adapter *sa)
{
	const struct rte_eth_conf *dev_conf = &sa->eth_dev->data->dev_conf;
	unsigned int sw_index;
	int rc = 0;

	rc = sfc_tx_check_mode(sa, &dev_conf->txmode);
	if (rc != 0)
		goto fail_check_mode;

	sa->txq_count = sa->eth_dev->data->nb_tx_queues;

	sa->txq_info = rte_calloc_socket("sfc-txqs", sa->txq_count,
					 sizeof(sa->txq_info[0]), 0,
					 sa->socket_id);
	if (sa->txq_info == NULL)
		goto fail_txqs_alloc;

	for (sw_index = 0; sw_index < sa->txq_count; ++sw_index) {
		rc = sfc_tx_qinit_info(sa, sw_index);
		if (rc != 0)
			goto fail_tx_qinit_info;
	}

	return 0;

fail_tx_qinit_info:
	rte_free(sa->txq_info);
	sa->txq_info = NULL;

fail_txqs_alloc:
	sa->txq_count = 0;

fail_check_mode:
	sfc_log_init(sa, "failed (rc = %d)", rc);
	return rc;
}

void
sfc_tx_fini(struct sfc_adapter *sa)
{
	int sw_index;

	sw_index = sa->txq_count;
	while (--sw_index >= 0) {
		if (sa->txq_info[sw_index].txq != NULL)
			sfc_tx_qfini(sa, sw_index);
	}

	rte_free(sa->txq_info);
	sa->txq_info = NULL;
	sa->txq_count = 0;
}
