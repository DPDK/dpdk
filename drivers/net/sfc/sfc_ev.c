/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2020 Xilinx, Inc.
 * Copyright(c) 2016-2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#include <rte_debug.h>
#include <rte_cycles.h>
#include <rte_alarm.h>
#include <rte_branch_prediction.h>

#include "efx.h"

#include "sfc.h"
#include "sfc_debug.h"
#include "sfc_log.h"
#include "sfc_ev.h"
#include "sfc_rx.h"
#include "sfc_tx.h"
#include "sfc_kvargs.h"


/* Initial delay when waiting for event queue init complete event */
#define SFC_EVQ_INIT_BACKOFF_START_US	(1)
/* Maximum delay between event queue polling attempts */
#define SFC_EVQ_INIT_BACKOFF_MAX_US	(10 * 1000)
/* Event queue init approx timeout */
#define SFC_EVQ_INIT_TIMEOUT_US		(2 * US_PER_S)

/* Management event queue polling period in microseconds */
#define SFC_MGMT_EV_QPOLL_PERIOD_US	(US_PER_S)

static const char *
sfc_evq_type2str(enum sfc_evq_type type)
{
	switch (type) {
	case SFC_EVQ_TYPE_MGMT:
		return "mgmt-evq";
	case SFC_EVQ_TYPE_RX:
		return "rx-evq";
	case SFC_EVQ_TYPE_TX:
		return "tx-evq";
	default:
		SFC_ASSERT(B_FALSE);
		return NULL;
	}
}

static boolean_t
sfc_ev_initialized(void *arg)
{
	struct sfc_evq *evq = arg;

	/* Init done events may be duplicated on SFN7xxx (SFC bug 31631) */
	SFC_ASSERT(evq->init_state == SFC_EVQ_STARTING ||
		   evq->init_state == SFC_EVQ_STARTED);

	evq->init_state = SFC_EVQ_STARTED;

	return B_FALSE;
}

static boolean_t
sfc_ev_nop_rx(void *arg, uint32_t label, uint32_t id,
	      uint32_t size, uint16_t flags)
{
	struct sfc_evq *evq = arg;

	sfc_err(evq->sa,
		"EVQ %u unexpected Rx event label=%u id=%#x size=%u flags=%#x",
		evq->evq_index, label, id, size, flags);
	return B_TRUE;
}

static boolean_t
sfc_ev_efx_rx(void *arg, __rte_unused uint32_t label, uint32_t id,
	      uint32_t size, uint16_t flags)
{
	struct sfc_evq *evq = arg;
	struct sfc_efx_rxq *rxq;
	unsigned int stop;
	unsigned int pending_id;
	unsigned int delta;
	unsigned int i;
	struct sfc_efx_rx_sw_desc *rxd;

	if (unlikely(evq->exception))
		goto done;

	rxq = sfc_efx_rxq_by_dp_rxq(evq->dp_rxq);

	SFC_ASSERT(rxq != NULL);
	SFC_ASSERT(rxq->evq == evq);
	SFC_ASSERT(rxq->flags & SFC_EFX_RXQ_FLAG_STARTED);

	stop = (id + 1) & rxq->ptr_mask;
	pending_id = rxq->pending & rxq->ptr_mask;
	delta = (stop >= pending_id) ? (stop - pending_id) :
		(rxq->ptr_mask + 1 - pending_id + stop);

	if (delta == 0) {
		/*
		 * Rx event with no new descriptors done and zero length
		 * is used to abort scattered packet when there is no room
		 * for the tail.
		 */
		if (unlikely(size != 0)) {
			evq->exception = B_TRUE;
			sfc_err(evq->sa,
				"EVQ %u RxQ %u invalid RX abort "
				"(id=%#x size=%u flags=%#x); needs restart",
				evq->evq_index, rxq->dp.dpq.queue_id,
				id, size, flags);
			goto done;
		}

		/* Add discard flag to the first fragment */
		rxq->sw_desc[pending_id].flags |= EFX_DISCARD;
		/* Remove continue flag from the last fragment */
		rxq->sw_desc[id].flags &= ~EFX_PKT_CONT;
	} else if (unlikely(delta > rxq->batch_max)) {
		evq->exception = B_TRUE;

		sfc_err(evq->sa,
			"EVQ %u RxQ %u completion out of order "
			"(id=%#x delta=%u flags=%#x); needs restart",
			evq->evq_index, rxq->dp.dpq.queue_id,
			id, delta, flags);

		goto done;
	}

	for (i = pending_id; i != stop; i = (i + 1) & rxq->ptr_mask) {
		rxd = &rxq->sw_desc[i];

		rxd->flags = flags;

		SFC_ASSERT(size < (1 << 16));
		rxd->size = (uint16_t)size;
	}

	rxq->pending += delta;

done:
	return B_FALSE;
}

static boolean_t
sfc_ev_dp_rx(void *arg, __rte_unused uint32_t label, uint32_t id,
	     __rte_unused uint32_t size, __rte_unused uint16_t flags)
{
	struct sfc_evq *evq = arg;
	struct sfc_dp_rxq *dp_rxq;

	dp_rxq = evq->dp_rxq;
	SFC_ASSERT(dp_rxq != NULL);

	SFC_ASSERT(evq->sa->priv.dp_rx->qrx_ev != NULL);
	return evq->sa->priv.dp_rx->qrx_ev(dp_rxq, id);
}

static boolean_t
sfc_ev_nop_rx_ps(void *arg, uint32_t label, uint32_t id,
		 uint32_t pkt_count, uint16_t flags)
{
	struct sfc_evq *evq = arg;

	sfc_err(evq->sa,
		"EVQ %u unexpected packed stream Rx event label=%u id=%#x pkt_count=%u flags=%#x",
		evq->evq_index, label, id, pkt_count, flags);
	return B_TRUE;
}

/* It is not actually used on datapath, but required on RxQ flush */
static boolean_t
sfc_ev_dp_rx_ps(void *arg, __rte_unused uint32_t label, uint32_t id,
		__rte_unused uint32_t pkt_count, __rte_unused uint16_t flags)
{
	struct sfc_evq *evq = arg;
	struct sfc_dp_rxq *dp_rxq;

	dp_rxq = evq->dp_rxq;
	SFC_ASSERT(dp_rxq != NULL);

	if (evq->sa->priv.dp_rx->qrx_ps_ev != NULL)
		return evq->sa->priv.dp_rx->qrx_ps_ev(dp_rxq, id);
	else
		return B_FALSE;
}

static boolean_t
sfc_ev_nop_tx(void *arg, uint32_t label, uint32_t id)
{
	struct sfc_evq *evq = arg;

	sfc_err(evq->sa, "EVQ %u unexpected Tx event label=%u id=%#x",
		evq->evq_index, label, id);
	return B_TRUE;
}

static boolean_t
sfc_ev_tx(void *arg, __rte_unused uint32_t label, uint32_t id)
{
	struct sfc_evq *evq = arg;
	struct sfc_dp_txq *dp_txq;
	struct sfc_efx_txq *txq;
	unsigned int stop;
	unsigned int delta;

	dp_txq = evq->dp_txq;
	SFC_ASSERT(dp_txq != NULL);

	txq = sfc_efx_txq_by_dp_txq(dp_txq);
	SFC_ASSERT(txq->evq == evq);

	if (unlikely((txq->flags & SFC_EFX_TXQ_FLAG_STARTED) == 0))
		goto done;

	stop = (id + 1) & txq->ptr_mask;
	id = txq->pending & txq->ptr_mask;

	delta = (stop >= id) ? (stop - id) : (txq->ptr_mask + 1 - id + stop);

	txq->pending += delta;

done:
	return B_FALSE;
}

static boolean_t
sfc_ev_dp_tx(void *arg, __rte_unused uint32_t label, uint32_t id)
{
	struct sfc_evq *evq = arg;
	struct sfc_dp_txq *dp_txq;

	dp_txq = evq->dp_txq;
	SFC_ASSERT(dp_txq != NULL);

	SFC_ASSERT(evq->sa->priv.dp_tx->qtx_ev != NULL);
	return evq->sa->priv.dp_tx->qtx_ev(dp_txq, id);
}

static boolean_t
sfc_ev_exception(void *arg, uint32_t code, __rte_unused uint32_t data)
{
	struct sfc_evq *evq = arg;

	if (code == EFX_EXCEPTION_UNKNOWN_SENSOREVT)
		return B_FALSE;

	evq->exception = B_TRUE;
	sfc_warn(evq->sa,
		 "hardware exception %s (code=%u, data=%#x) on EVQ %u;"
		 " needs recovery",
		 (code == EFX_EXCEPTION_RX_RECOVERY) ? "RX_RECOVERY" :
		 (code == EFX_EXCEPTION_RX_DSC_ERROR) ? "RX_DSC_ERROR" :
		 (code == EFX_EXCEPTION_TX_DSC_ERROR) ? "TX_DSC_ERROR" :
		 (code == EFX_EXCEPTION_FWALERT_SRAM) ? "FWALERT_SRAM" :
		 (code == EFX_EXCEPTION_UNKNOWN_FWALERT) ? "UNKNOWN_FWALERT" :
		 (code == EFX_EXCEPTION_RX_ERROR) ? "RX_ERROR" :
		 (code == EFX_EXCEPTION_TX_ERROR) ? "TX_ERROR" :
		 (code == EFX_EXCEPTION_EV_ERROR) ? "EV_ERROR" :
		 "UNKNOWN",
		 code, data, evq->evq_index);

	return B_TRUE;
}

static boolean_t
sfc_ev_nop_rxq_flush_done(void *arg, uint32_t rxq_hw_index)
{
	struct sfc_evq *evq = arg;

	sfc_err(evq->sa, "EVQ %u unexpected RxQ %u flush done",
		evq->evq_index, rxq_hw_index);
	return B_TRUE;
}

static boolean_t
sfc_ev_rxq_flush_done(void *arg, __rte_unused uint32_t rxq_hw_index)
{
	struct sfc_evq *evq = arg;
	struct sfc_dp_rxq *dp_rxq;
	struct sfc_rxq *rxq;

	dp_rxq = evq->dp_rxq;
	SFC_ASSERT(dp_rxq != NULL);

	rxq = sfc_rxq_by_dp_rxq(dp_rxq);
	SFC_ASSERT(rxq != NULL);
	SFC_ASSERT(rxq->hw_index == rxq_hw_index);
	SFC_ASSERT(rxq->evq == evq);
	RTE_SET_USED(rxq);

	sfc_rx_qflush_done(sfc_rxq_info_by_dp_rxq(dp_rxq));

	return B_FALSE;
}

static boolean_t
sfc_ev_nop_rxq_flush_failed(void *arg, uint32_t rxq_hw_index)
{
	struct sfc_evq *evq = arg;

	sfc_err(evq->sa, "EVQ %u unexpected RxQ %u flush failed",
		evq->evq_index, rxq_hw_index);
	return B_TRUE;
}

static boolean_t
sfc_ev_rxq_flush_failed(void *arg, __rte_unused uint32_t rxq_hw_index)
{
	struct sfc_evq *evq = arg;
	struct sfc_dp_rxq *dp_rxq;
	struct sfc_rxq *rxq;

	dp_rxq = evq->dp_rxq;
	SFC_ASSERT(dp_rxq != NULL);

	rxq = sfc_rxq_by_dp_rxq(dp_rxq);
	SFC_ASSERT(rxq != NULL);
	SFC_ASSERT(rxq->hw_index == rxq_hw_index);
	SFC_ASSERT(rxq->evq == evq);
	RTE_SET_USED(rxq);

	sfc_rx_qflush_failed(sfc_rxq_info_by_dp_rxq(dp_rxq));

	return B_FALSE;
}

static boolean_t
sfc_ev_nop_txq_flush_done(void *arg, uint32_t txq_hw_index)
{
	struct sfc_evq *evq = arg;

	sfc_err(evq->sa, "EVQ %u unexpected TxQ %u flush done",
		evq->evq_index, txq_hw_index);
	return B_TRUE;
}

static boolean_t
sfc_ev_txq_flush_done(void *arg, __rte_unused uint32_t txq_hw_index)
{
	struct sfc_evq *evq = arg;
	struct sfc_dp_txq *dp_txq;
	struct sfc_txq *txq;

	dp_txq = evq->dp_txq;
	SFC_ASSERT(dp_txq != NULL);

	txq = sfc_txq_by_dp_txq(dp_txq);
	SFC_ASSERT(txq != NULL);
	SFC_ASSERT(txq->hw_index == txq_hw_index);
	SFC_ASSERT(txq->evq == evq);
	RTE_SET_USED(txq);

	sfc_tx_qflush_done(sfc_txq_info_by_dp_txq(dp_txq));

	return B_FALSE;
}

static boolean_t
sfc_ev_software(void *arg, uint16_t magic)
{
	struct sfc_evq *evq = arg;

	sfc_err(evq->sa, "EVQ %u unexpected software event magic=%#.4x",
		evq->evq_index, magic);
	return B_TRUE;
}

static boolean_t
sfc_ev_sram(void *arg, uint32_t code)
{
	struct sfc_evq *evq = arg;

	sfc_err(evq->sa, "EVQ %u unexpected SRAM event code=%u",
		evq->evq_index, code);
	return B_TRUE;
}

static boolean_t
sfc_ev_wake_up(void *arg, uint32_t index)
{
	struct sfc_evq *evq = arg;

	sfc_err(evq->sa, "EVQ %u unexpected wake up event index=%u",
		evq->evq_index, index);
	return B_TRUE;
}

static boolean_t
sfc_ev_timer(void *arg, uint32_t index)
{
	struct sfc_evq *evq = arg;

	sfc_err(evq->sa, "EVQ %u unexpected timer event index=%u",
		evq->evq_index, index);
	return B_TRUE;
}

static boolean_t
sfc_ev_nop_link_change(void *arg, __rte_unused efx_link_mode_t link_mode)
{
	struct sfc_evq *evq = arg;

	sfc_err(evq->sa, "EVQ %u unexpected link change event",
		evq->evq_index);
	return B_TRUE;
}

static boolean_t
sfc_ev_link_change(void *arg, efx_link_mode_t link_mode)
{
	struct sfc_evq *evq = arg;
	struct sfc_adapter *sa = evq->sa;
	struct rte_eth_link new_link;

	sfc_port_link_mode_to_info(link_mode, &new_link);
	if (rte_eth_linkstatus_set(sa->eth_dev, &new_link) == 0)
		evq->sa->port.lsc_seq++;

	return B_FALSE;
}

static const efx_ev_callbacks_t sfc_ev_callbacks = {
	.eec_initialized	= sfc_ev_initialized,
	.eec_rx			= sfc_ev_nop_rx,
	.eec_rx_ps		= sfc_ev_nop_rx_ps,
	.eec_tx			= sfc_ev_nop_tx,
	.eec_exception		= sfc_ev_exception,
	.eec_rxq_flush_done	= sfc_ev_nop_rxq_flush_done,
	.eec_rxq_flush_failed	= sfc_ev_nop_rxq_flush_failed,
	.eec_txq_flush_done	= sfc_ev_nop_txq_flush_done,
	.eec_software		= sfc_ev_software,
	.eec_sram		= sfc_ev_sram,
	.eec_wake_up		= sfc_ev_wake_up,
	.eec_timer		= sfc_ev_timer,
	.eec_link_change	= sfc_ev_link_change,
};

static const efx_ev_callbacks_t sfc_ev_callbacks_efx_rx = {
	.eec_initialized	= sfc_ev_initialized,
	.eec_rx			= sfc_ev_efx_rx,
	.eec_rx_ps		= sfc_ev_nop_rx_ps,
	.eec_tx			= sfc_ev_nop_tx,
	.eec_exception		= sfc_ev_exception,
	.eec_rxq_flush_done	= sfc_ev_rxq_flush_done,
	.eec_rxq_flush_failed	= sfc_ev_rxq_flush_failed,
	.eec_txq_flush_done	= sfc_ev_nop_txq_flush_done,
	.eec_software		= sfc_ev_software,
	.eec_sram		= sfc_ev_sram,
	.eec_wake_up		= sfc_ev_wake_up,
	.eec_timer		= sfc_ev_timer,
	.eec_link_change	= sfc_ev_nop_link_change,
};

static const efx_ev_callbacks_t sfc_ev_callbacks_dp_rx = {
	.eec_initialized	= sfc_ev_initialized,
	.eec_rx			= sfc_ev_dp_rx,
	.eec_rx_ps		= sfc_ev_dp_rx_ps,
	.eec_tx			= sfc_ev_nop_tx,
	.eec_exception		= sfc_ev_exception,
	.eec_rxq_flush_done	= sfc_ev_rxq_flush_done,
	.eec_rxq_flush_failed	= sfc_ev_rxq_flush_failed,
	.eec_txq_flush_done	= sfc_ev_nop_txq_flush_done,
	.eec_software		= sfc_ev_software,
	.eec_sram		= sfc_ev_sram,
	.eec_wake_up		= sfc_ev_wake_up,
	.eec_timer		= sfc_ev_timer,
	.eec_link_change	= sfc_ev_nop_link_change,
};

static const efx_ev_callbacks_t sfc_ev_callbacks_efx_tx = {
	.eec_initialized	= sfc_ev_initialized,
	.eec_rx			= sfc_ev_nop_rx,
	.eec_rx_ps		= sfc_ev_nop_rx_ps,
	.eec_tx			= sfc_ev_tx,
	.eec_exception		= sfc_ev_exception,
	.eec_rxq_flush_done	= sfc_ev_nop_rxq_flush_done,
	.eec_rxq_flush_failed	= sfc_ev_nop_rxq_flush_failed,
	.eec_txq_flush_done	= sfc_ev_txq_flush_done,
	.eec_software		= sfc_ev_software,
	.eec_sram		= sfc_ev_sram,
	.eec_wake_up		= sfc_ev_wake_up,
	.eec_timer		= sfc_ev_timer,
	.eec_link_change	= sfc_ev_nop_link_change,
};

static const efx_ev_callbacks_t sfc_ev_callbacks_dp_tx = {
	.eec_initialized	= sfc_ev_initialized,
	.eec_rx			= sfc_ev_nop_rx,
	.eec_rx_ps		= sfc_ev_nop_rx_ps,
	.eec_tx			= sfc_ev_dp_tx,
	.eec_exception		= sfc_ev_exception,
	.eec_rxq_flush_done	= sfc_ev_nop_rxq_flush_done,
	.eec_rxq_flush_failed	= sfc_ev_nop_rxq_flush_failed,
	.eec_txq_flush_done	= sfc_ev_txq_flush_done,
	.eec_software		= sfc_ev_software,
	.eec_sram		= sfc_ev_sram,
	.eec_wake_up		= sfc_ev_wake_up,
	.eec_timer		= sfc_ev_timer,
	.eec_link_change	= sfc_ev_nop_link_change,
};


void
sfc_ev_qpoll(struct sfc_evq *evq)
{
	SFC_ASSERT(evq->init_state == SFC_EVQ_STARTED ||
		   evq->init_state == SFC_EVQ_STARTING);

	/* Synchronize the DMA memory for reading not required */

	efx_ev_qpoll(evq->common, &evq->read_ptr, evq->callbacks, evq);

	if (unlikely(evq->exception) && sfc_adapter_trylock(evq->sa)) {
		struct sfc_adapter *sa = evq->sa;
		int rc;

		if (evq->dp_rxq != NULL) {
			unsigned int rxq_sw_index;

			rxq_sw_index = evq->dp_rxq->dpq.queue_id;

			sfc_warn(sa,
				 "restart RxQ %u because of exception on its EvQ %u",
				 rxq_sw_index, evq->evq_index);

			sfc_rx_qstop(sa, rxq_sw_index);
			rc = sfc_rx_qstart(sa, rxq_sw_index);
			if (rc != 0)
				sfc_err(sa, "cannot restart RxQ %u",
					rxq_sw_index);
		}

		if (evq->dp_txq != NULL) {
			unsigned int txq_sw_index;

			txq_sw_index = evq->dp_txq->dpq.queue_id;

			sfc_warn(sa,
				 "restart TxQ %u because of exception on its EvQ %u",
				 txq_sw_index, evq->evq_index);

			sfc_tx_qstop(sa, txq_sw_index);
			rc = sfc_tx_qstart(sa, txq_sw_index);
			if (rc != 0)
				sfc_err(sa, "cannot restart TxQ %u",
					txq_sw_index);
		}

		if (evq->exception)
			sfc_panic(sa, "unrecoverable exception on EvQ %u",
				  evq->evq_index);

		sfc_adapter_unlock(sa);
	}

	/* Poll-mode driver does not re-prime the event queue for interrupts */
}

void
sfc_ev_mgmt_qpoll(struct sfc_adapter *sa)
{
	if (rte_spinlock_trylock(&sa->mgmt_evq_lock)) {
		if (sa->mgmt_evq_running)
			sfc_ev_qpoll(sa->mgmt_evq);

		rte_spinlock_unlock(&sa->mgmt_evq_lock);
	}
}

int
sfc_ev_qprime(struct sfc_evq *evq)
{
	SFC_ASSERT(evq->init_state == SFC_EVQ_STARTED);
	return efx_ev_qprime(evq->common, evq->read_ptr);
}

/* Event queue HW index allocation scheme is described in sfc_ev.h. */
int
sfc_ev_qstart(struct sfc_evq *evq, unsigned int hw_index)
{
	struct sfc_adapter *sa = evq->sa;
	efsys_mem_t *esmp;
	uint32_t evq_flags = sa->evq_flags;
	unsigned int total_delay_us;
	unsigned int delay_us;
	int rc;

	sfc_log_init(sa, "hw_index=%u", hw_index);

	esmp = &evq->mem;

	evq->evq_index = hw_index;

	/* Clear all events */
	(void)memset((void *)esmp->esm_base, 0xff,
		     efx_evq_size(sa->nic, evq->entries));

	if ((sa->intr.lsc_intr && hw_index == sa->mgmt_evq_index) ||
	    (sa->intr.rxq_intr && evq->dp_rxq != NULL))
		evq_flags |= EFX_EVQ_FLAGS_NOTIFY_INTERRUPT;
	else
		evq_flags |= EFX_EVQ_FLAGS_NOTIFY_DISABLED;

	/* Create the common code event queue */
	rc = efx_ev_qcreate(sa->nic, hw_index, esmp, evq->entries,
			    0 /* unused on EF10 */, 0, evq_flags,
			    &evq->common);
	if (rc != 0)
		goto fail_ev_qcreate;

	SFC_ASSERT(evq->dp_rxq == NULL || evq->dp_txq == NULL);
	if (evq->dp_rxq != 0) {
		if (strcmp(sa->priv.dp_rx->dp.name,
			   SFC_KVARG_DATAPATH_EFX) == 0)
			evq->callbacks = &sfc_ev_callbacks_efx_rx;
		else
			evq->callbacks = &sfc_ev_callbacks_dp_rx;
	} else if (evq->dp_txq != 0) {
		if (strcmp(sa->priv.dp_tx->dp.name,
			   SFC_KVARG_DATAPATH_EFX) == 0)
			evq->callbacks = &sfc_ev_callbacks_efx_tx;
		else
			evq->callbacks = &sfc_ev_callbacks_dp_tx;
	} else {
		evq->callbacks = &sfc_ev_callbacks;
	}

	evq->init_state = SFC_EVQ_STARTING;

	/* Wait for the initialization event */
	total_delay_us = 0;
	delay_us = SFC_EVQ_INIT_BACKOFF_START_US;
	do {
		(void)sfc_ev_qpoll(evq);

		/* Check to see if the initialization complete indication
		 * posted by the hardware.
		 */
		if (evq->init_state == SFC_EVQ_STARTED)
			goto done;

		/* Give event queue some time to init */
		rte_delay_us(delay_us);

		total_delay_us += delay_us;

		/* Exponential backoff */
		delay_us *= 2;
		if (delay_us > SFC_EVQ_INIT_BACKOFF_MAX_US)
			delay_us = SFC_EVQ_INIT_BACKOFF_MAX_US;

	} while (total_delay_us < SFC_EVQ_INIT_TIMEOUT_US);

	rc = ETIMEDOUT;
	goto fail_timedout;

done:
	return 0;

fail_timedout:
	evq->init_state = SFC_EVQ_INITIALIZED;
	efx_ev_qdestroy(evq->common);

fail_ev_qcreate:
	sfc_log_init(sa, "failed %d", rc);
	return rc;
}

void
sfc_ev_qstop(struct sfc_evq *evq)
{
	if (evq == NULL)
		return;

	sfc_log_init(evq->sa, "hw_index=%u", evq->evq_index);

	if (evq->init_state != SFC_EVQ_STARTED)
		return;

	evq->init_state = SFC_EVQ_INITIALIZED;
	evq->callbacks = NULL;
	evq->read_ptr = 0;
	evq->exception = B_FALSE;

	efx_ev_qdestroy(evq->common);

	evq->evq_index = 0;
}

static void
sfc_ev_mgmt_periodic_qpoll(void *arg)
{
	struct sfc_adapter *sa = arg;
	int rc;

	sfc_ev_mgmt_qpoll(sa);

	rc = rte_eal_alarm_set(SFC_MGMT_EV_QPOLL_PERIOD_US,
			       sfc_ev_mgmt_periodic_qpoll, sa);
	if (rc == -ENOTSUP) {
		sfc_warn(sa, "alarms are not supported");
		sfc_warn(sa, "management EVQ must be polled indirectly using no-wait link status update");
	} else if (rc != 0) {
		sfc_err(sa,
			"cannot rearm management EVQ polling alarm (rc=%d)",
			rc);
	}
}

static void
sfc_ev_mgmt_periodic_qpoll_start(struct sfc_adapter *sa)
{
	sfc_ev_mgmt_periodic_qpoll(sa);
}

static void
sfc_ev_mgmt_periodic_qpoll_stop(struct sfc_adapter *sa)
{
	rte_eal_alarm_cancel(sfc_ev_mgmt_periodic_qpoll, sa);
}

int
sfc_ev_start(struct sfc_adapter *sa)
{
	int rc;

	sfc_log_init(sa, "entry");

	rc = efx_ev_init(sa->nic);
	if (rc != 0)
		goto fail_ev_init;

	/* Start management EVQ used for global events */

	/*
	 * Management event queue start polls the queue, but it cannot
	 * interfere with other polling contexts since mgmt_evq_running
	 * is false yet.
	 */
	rc = sfc_ev_qstart(sa->mgmt_evq, sa->mgmt_evq_index);
	if (rc != 0)
		goto fail_mgmt_evq_start;

	rte_spinlock_lock(&sa->mgmt_evq_lock);
	sa->mgmt_evq_running = true;
	rte_spinlock_unlock(&sa->mgmt_evq_lock);

	if (sa->intr.lsc_intr) {
		rc = sfc_ev_qprime(sa->mgmt_evq);
		if (rc != 0)
			goto fail_mgmt_evq_prime;
	}

	/*
	 * Start management EVQ polling. If interrupts are disabled
	 * (not used), it is required to process link status change
	 * and other device level events to avoid unrecoverable
	 * error because the event queue overflow.
	 */
	sfc_ev_mgmt_periodic_qpoll_start(sa);

	/*
	 * Rx/Tx event queues are started/stopped when corresponding
	 * Rx/Tx queue is started/stopped.
	 */

	return 0;

fail_mgmt_evq_prime:
	sfc_ev_qstop(sa->mgmt_evq);

fail_mgmt_evq_start:
	efx_ev_fini(sa->nic);

fail_ev_init:
	sfc_log_init(sa, "failed %d", rc);
	return rc;
}

void
sfc_ev_stop(struct sfc_adapter *sa)
{
	sfc_log_init(sa, "entry");

	sfc_ev_mgmt_periodic_qpoll_stop(sa);

	rte_spinlock_lock(&sa->mgmt_evq_lock);
	sa->mgmt_evq_running = false;
	rte_spinlock_unlock(&sa->mgmt_evq_lock);

	sfc_ev_qstop(sa->mgmt_evq);

	efx_ev_fini(sa->nic);
}

int
sfc_ev_qinit(struct sfc_adapter *sa,
	     enum sfc_evq_type type, unsigned int type_index,
	     unsigned int entries, int socket_id, struct sfc_evq **evqp)
{
	struct sfc_evq *evq;
	int rc;

	sfc_log_init(sa, "type=%s type_index=%u",
		     sfc_evq_type2str(type), type_index);

	SFC_ASSERT(rte_is_power_of_2(entries));

	rc = ENOMEM;
	evq = rte_zmalloc_socket("sfc-evq", sizeof(*evq), RTE_CACHE_LINE_SIZE,
				 socket_id);
	if (evq == NULL)
		goto fail_evq_alloc;

	evq->sa = sa;
	evq->type = type;
	evq->entries = entries;

	/* Allocate DMA space */
	rc = sfc_dma_alloc(sa, sfc_evq_type2str(type), type_index,
			   efx_evq_size(sa->nic, evq->entries), socket_id,
			   &evq->mem);
	if (rc != 0)
		goto fail_dma_alloc;

	evq->init_state = SFC_EVQ_INITIALIZED;

	sa->evq_count++;

	*evqp = evq;

	return 0;

fail_dma_alloc:
	rte_free(evq);

fail_evq_alloc:

	sfc_log_init(sa, "failed %d", rc);
	return rc;
}

void
sfc_ev_qfini(struct sfc_evq *evq)
{
	struct sfc_adapter *sa = evq->sa;

	SFC_ASSERT(evq->init_state == SFC_EVQ_INITIALIZED);

	sfc_dma_free(sa, &evq->mem);

	rte_free(evq);

	SFC_ASSERT(sa->evq_count > 0);
	sa->evq_count--;
}

static int
sfc_kvarg_perf_profile_handler(__rte_unused const char *key,
			       const char *value_str, void *opaque)
{
	uint32_t *value = opaque;

	if (strcasecmp(value_str, SFC_KVARG_PERF_PROFILE_THROUGHPUT) == 0)
		*value = EFX_EVQ_FLAGS_TYPE_THROUGHPUT;
	else if (strcasecmp(value_str, SFC_KVARG_PERF_PROFILE_LOW_LATENCY) == 0)
		*value = EFX_EVQ_FLAGS_TYPE_LOW_LATENCY;
	else if (strcasecmp(value_str, SFC_KVARG_PERF_PROFILE_AUTO) == 0)
		*value = EFX_EVQ_FLAGS_TYPE_AUTO;
	else
		return -EINVAL;

	return 0;
}

int
sfc_ev_attach(struct sfc_adapter *sa)
{
	int rc;

	sfc_log_init(sa, "entry");

	sa->evq_flags = EFX_EVQ_FLAGS_TYPE_THROUGHPUT;
	rc = sfc_kvargs_process(sa, SFC_KVARG_PERF_PROFILE,
				sfc_kvarg_perf_profile_handler,
				&sa->evq_flags);
	if (rc != 0) {
		sfc_err(sa, "invalid %s parameter value",
			SFC_KVARG_PERF_PROFILE);
		goto fail_kvarg_perf_profile;
	}

	sa->mgmt_evq_index = 0;
	rte_spinlock_init(&sa->mgmt_evq_lock);

	rc = sfc_ev_qinit(sa, SFC_EVQ_TYPE_MGMT, 0, sa->evq_min_entries,
			  sa->socket_id, &sa->mgmt_evq);
	if (rc != 0)
		goto fail_mgmt_evq_init;

	/*
	 * Rx/Tx event queues are created/destroyed when corresponding
	 * Rx/Tx queue is created/destroyed.
	 */

	return 0;

fail_mgmt_evq_init:

fail_kvarg_perf_profile:
	sfc_log_init(sa, "failed %d", rc);
	return rc;
}

void
sfc_ev_detach(struct sfc_adapter *sa)
{
	sfc_log_init(sa, "entry");

	sfc_ev_qfini(sa->mgmt_evq);

	if (sa->evq_count != 0)
		sfc_err(sa, "%u EvQs are not destroyed before detach",
			sa->evq_count);
}
