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
sfc_ev_rx(void *arg, __rte_unused uint32_t label, uint32_t id,
	  uint32_t size, uint16_t flags)
{
	struct sfc_evq *evq = arg;
	struct sfc_rxq *rxq;
	unsigned int stop;
	unsigned int pending_id;
	unsigned int delta;
	unsigned int i;
	struct sfc_rx_sw_desc *rxd;

	if (unlikely(evq->exception))
		goto done;

	rxq = evq->rxq;

	SFC_ASSERT(rxq != NULL);
	SFC_ASSERT(rxq->evq == evq);
	SFC_ASSERT(rxq->state & SFC_RXQ_STARTED);

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
				evq->evq_index, sfc_rxq_sw_index(rxq),
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
			evq->evq_index, sfc_rxq_sw_index(rxq), id, delta,
			flags);

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
sfc_ev_tx(void *arg, __rte_unused uint32_t label, uint32_t id)
{
	struct sfc_evq *evq = arg;
	struct sfc_txq *txq;
	unsigned int stop;
	unsigned int delta;

	txq = evq->txq;

	SFC_ASSERT(txq != NULL);
	SFC_ASSERT(txq->evq == evq);

	if (unlikely((txq->state & SFC_TXQ_STARTED) == 0))
		goto done;

	stop = (id + 1) & txq->ptr_mask;
	id = txq->pending & txq->ptr_mask;

	delta = (stop >= id) ? (stop - id) : (txq->ptr_mask + 1 - id + stop);

	txq->pending += delta;

done:
	return B_FALSE;
}

static boolean_t
sfc_ev_exception(void *arg, __rte_unused uint32_t code,
		 __rte_unused uint32_t data)
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
sfc_ev_rxq_flush_done(void *arg, __rte_unused uint32_t rxq_hw_index)
{
	struct sfc_evq *evq = arg;
	struct sfc_rxq *rxq;

	rxq = evq->rxq;
	SFC_ASSERT(rxq != NULL);
	SFC_ASSERT(rxq->hw_index == rxq_hw_index);
	SFC_ASSERT(rxq->evq == evq);
	sfc_rx_qflush_done(rxq);

	return B_FALSE;
}

static boolean_t
sfc_ev_rxq_flush_failed(void *arg, __rte_unused uint32_t rxq_hw_index)
{
	struct sfc_evq *evq = arg;
	struct sfc_rxq *rxq;

	rxq = evq->rxq;
	SFC_ASSERT(rxq != NULL);
	SFC_ASSERT(rxq->hw_index == rxq_hw_index);
	SFC_ASSERT(rxq->evq == evq);
	sfc_rx_qflush_failed(rxq);

	return B_FALSE;
}

static boolean_t
sfc_ev_txq_flush_done(void *arg, __rte_unused uint32_t txq_hw_index)
{
	struct sfc_evq *evq = arg;
	struct sfc_txq *txq;

	txq = evq->txq;
	SFC_ASSERT(txq != NULL);
	SFC_ASSERT(txq->hw_index == txq_hw_index);
	SFC_ASSERT(txq->evq == evq);
	sfc_tx_qflush_done(txq);

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
sfc_ev_link_change(void *arg, efx_link_mode_t link_mode)
{
	struct sfc_evq *evq = arg;
	struct sfc_adapter *sa = evq->sa;
	struct rte_eth_link *dev_link = &sa->eth_dev->data->dev_link;
	struct rte_eth_link new_link;
	uint64_t new_link_u64;
	uint64_t old_link_u64;

	EFX_STATIC_ASSERT(sizeof(*dev_link) == sizeof(rte_atomic64_t));

	sfc_port_link_mode_to_info(link_mode, &new_link);

	new_link_u64 = *(uint64_t *)&new_link;
	do {
		old_link_u64 = rte_atomic64_read((rte_atomic64_t *)dev_link);
		if (old_link_u64 == new_link_u64)
			break;

		if (rte_atomic64_cmpset((volatile uint64_t *)dev_link,
					old_link_u64, new_link_u64)) {
			evq->sa->port.lsc_seq++;
			break;
		}
	} while (B_TRUE);

	return B_FALSE;
}

static const efx_ev_callbacks_t sfc_ev_callbacks = {
	.eec_initialized	= sfc_ev_initialized,
	.eec_rx			= sfc_ev_rx,
	.eec_tx			= sfc_ev_tx,
	.eec_exception		= sfc_ev_exception,
	.eec_rxq_flush_done	= sfc_ev_rxq_flush_done,
	.eec_rxq_flush_failed	= sfc_ev_rxq_flush_failed,
	.eec_txq_flush_done	= sfc_ev_txq_flush_done,
	.eec_software		= sfc_ev_software,
	.eec_sram		= sfc_ev_sram,
	.eec_wake_up		= sfc_ev_wake_up,
	.eec_timer		= sfc_ev_timer,
	.eec_link_change	= sfc_ev_link_change,
};


void
sfc_ev_qpoll(struct sfc_evq *evq)
{
	SFC_ASSERT(evq->init_state == SFC_EVQ_STARTED ||
		   evq->init_state == SFC_EVQ_STARTING);

	/* Synchronize the DMA memory for reading not required */

	efx_ev_qpoll(evq->common, &evq->read_ptr, &sfc_ev_callbacks, evq);

	if (unlikely(evq->exception) && sfc_adapter_trylock(evq->sa)) {
		struct sfc_adapter *sa = evq->sa;
		int rc;

		if ((evq->rxq != NULL) && (evq->rxq->state & SFC_RXQ_RUNNING)) {
			unsigned int rxq_sw_index = sfc_rxq_sw_index(evq->rxq);

			sfc_warn(sa,
				 "restart RxQ %u because of exception on its EvQ %u",
				 rxq_sw_index, evq->evq_index);

			sfc_rx_qstop(sa, rxq_sw_index);
			rc = sfc_rx_qstart(sa, rxq_sw_index);
			if (rc != 0)
				sfc_err(sa, "cannot restart RxQ %u",
					rxq_sw_index);
		}

		if (evq->txq != NULL) {
			unsigned int txq_sw_index = sfc_txq_sw_index(evq->txq);

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
		struct sfc_evq *mgmt_evq = sa->evq_info[sa->mgmt_evq_index].evq;

		if (mgmt_evq->init_state == SFC_EVQ_STARTED)
			sfc_ev_qpoll(mgmt_evq);

		rte_spinlock_unlock(&sa->mgmt_evq_lock);
	}
}

int
sfc_ev_qprime(struct sfc_evq *evq)
{
	SFC_ASSERT(evq->init_state == SFC_EVQ_STARTED);
	return efx_ev_qprime(evq->common, evq->read_ptr);
}

int
sfc_ev_qstart(struct sfc_adapter *sa, unsigned int sw_index)
{
	const struct sfc_evq_info *evq_info;
	struct sfc_evq *evq;
	efsys_mem_t *esmp;
	unsigned int total_delay_us;
	unsigned int delay_us;
	int rc;

	sfc_log_init(sa, "sw_index=%u", sw_index);

	evq_info = &sa->evq_info[sw_index];
	evq = evq_info->evq;
	esmp = &evq->mem;

	/* Clear all events */
	(void)memset((void *)esmp->esm_base, 0xff,
		     EFX_EVQ_SIZE(evq_info->entries));

	/* Create the common code event queue */
	rc = efx_ev_qcreate(sa->nic, sw_index, esmp, evq_info->entries,
			    0 /* unused on EF10 */, 0, evq_info->flags,
			    &evq->common);
	if (rc != 0)
		goto fail_ev_qcreate;

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
sfc_ev_qstop(struct sfc_adapter *sa, unsigned int sw_index)
{
	const struct sfc_evq_info *evq_info;
	struct sfc_evq *evq;

	sfc_log_init(sa, "sw_index=%u", sw_index);

	SFC_ASSERT(sw_index < sa->evq_count);

	evq_info = &sa->evq_info[sw_index];
	evq = evq_info->evq;

	if (evq == NULL || evq->init_state != SFC_EVQ_STARTED)
		return;

	evq->init_state = SFC_EVQ_INITIALIZED;
	evq->read_ptr = 0;
	evq->exception = B_FALSE;

	efx_ev_qdestroy(evq->common);
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
	rte_spinlock_lock(&sa->mgmt_evq_lock);

	rc = sfc_ev_qstart(sa, sa->mgmt_evq_index);
	if (rc != 0)
		goto fail_mgmt_evq_start;

	if (sa->intr.lsc_intr) {
		rc = sfc_ev_qprime(sa->evq_info[sa->mgmt_evq_index].evq);
		if (rc != 0)
			goto fail_evq0_prime;
	}

	rte_spinlock_unlock(&sa->mgmt_evq_lock);

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

fail_evq0_prime:
	sfc_ev_qstop(sa, 0);

fail_mgmt_evq_start:
	rte_spinlock_unlock(&sa->mgmt_evq_lock);
	efx_ev_fini(sa->nic);

fail_ev_init:
	sfc_log_init(sa, "failed %d", rc);
	return rc;
}

void
sfc_ev_stop(struct sfc_adapter *sa)
{
	unsigned int sw_index;

	sfc_log_init(sa, "entry");

	sfc_ev_mgmt_periodic_qpoll_stop(sa);

	/* Make sure that all event queues are stopped */
	sw_index = sa->evq_count;
	while (sw_index-- > 0) {
		if (sw_index == sa->mgmt_evq_index) {
			/* Locks are required for the management EVQ */
			rte_spinlock_lock(&sa->mgmt_evq_lock);
			sfc_ev_qstop(sa, sa->mgmt_evq_index);
			rte_spinlock_unlock(&sa->mgmt_evq_lock);
		} else {
			sfc_ev_qstop(sa, sw_index);
		}
	}

	efx_ev_fini(sa->nic);
}

int
sfc_ev_qinit(struct sfc_adapter *sa, unsigned int sw_index,
	     unsigned int entries, int socket_id)
{
	struct sfc_evq_info *evq_info;
	struct sfc_evq *evq;
	int rc;

	sfc_log_init(sa, "sw_index=%u", sw_index);

	evq_info = &sa->evq_info[sw_index];

	SFC_ASSERT(rte_is_power_of_2(entries));
	SFC_ASSERT(entries <= evq_info->max_entries);
	evq_info->entries = entries;

	evq = rte_zmalloc_socket("sfc-evq", sizeof(*evq), RTE_CACHE_LINE_SIZE,
				 socket_id);
	if (evq == NULL)
		return ENOMEM;

	evq->sa = sa;
	evq->evq_index = sw_index;

	/* Allocate DMA space */
	rc = sfc_dma_alloc(sa, "evq", sw_index, EFX_EVQ_SIZE(evq_info->entries),
			   socket_id, &evq->mem);
	if (rc != 0)
		return rc;

	evq->init_state = SFC_EVQ_INITIALIZED;

	evq_info->evq = evq;

	return 0;
}

void
sfc_ev_qfini(struct sfc_adapter *sa, unsigned int sw_index)
{
	struct sfc_evq *evq;

	sfc_log_init(sa, "sw_index=%u", sw_index);

	evq = sa->evq_info[sw_index].evq;

	SFC_ASSERT(evq->init_state == SFC_EVQ_INITIALIZED);

	sa->evq_info[sw_index].evq = NULL;

	sfc_dma_free(sa, &evq->mem);

	rte_free(evq);
}

static int
sfc_ev_qinit_info(struct sfc_adapter *sa, unsigned int sw_index)
{
	struct sfc_evq_info *evq_info = &sa->evq_info[sw_index];
	unsigned int max_entries;

	sfc_log_init(sa, "sw_index=%u", sw_index);

	max_entries = sfc_evq_max_entries(sa, sw_index);
	SFC_ASSERT(rte_is_power_of_2(max_entries));

	evq_info->max_entries = max_entries;
	evq_info->flags = sa->evq_flags |
		((sa->intr.lsc_intr && sw_index == sa->mgmt_evq_index) ?
			EFX_EVQ_FLAGS_NOTIFY_INTERRUPT :
			EFX_EVQ_FLAGS_NOTIFY_DISABLED);

	return 0;
}

static int
sfc_kvarg_perf_profile_handler(__rte_unused const char *key,
			       const char *value_str, void *opaque)
{
	uint64_t *value = opaque;

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

static void
sfc_ev_qfini_info(struct sfc_adapter *sa, unsigned int sw_index)
{
	sfc_log_init(sa, "sw_index=%u", sw_index);

	/* Nothing to cleanup */
}

int
sfc_ev_init(struct sfc_adapter *sa)
{
	int rc;
	unsigned int sw_index;

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

	sa->evq_count = sfc_ev_qcount(sa);
	sa->mgmt_evq_index = 0;
	rte_spinlock_init(&sa->mgmt_evq_lock);

	/* Allocate EVQ info array */
	rc = ENOMEM;
	sa->evq_info = rte_calloc_socket("sfc-evqs", sa->evq_count,
					 sizeof(struct sfc_evq_info), 0,
					 sa->socket_id);
	if (sa->evq_info == NULL)
		goto fail_evqs_alloc;

	for (sw_index = 0; sw_index < sa->evq_count; ++sw_index) {
		rc = sfc_ev_qinit_info(sa, sw_index);
		if (rc != 0)
			goto fail_ev_qinit_info;
	}

	rc = sfc_ev_qinit(sa, sa->mgmt_evq_index, SFC_MGMT_EVQ_ENTRIES,
			  sa->socket_id);
	if (rc != 0)
		goto fail_mgmt_evq_init;

	/*
	 * Rx/Tx event queues are created/destroyed when corresponding
	 * Rx/Tx queue is created/destroyed.
	 */

	return 0;

fail_mgmt_evq_init:
fail_ev_qinit_info:
	while (sw_index-- > 0)
		sfc_ev_qfini_info(sa, sw_index);

	rte_free(sa->evq_info);
	sa->evq_info = NULL;

fail_evqs_alloc:
	sa->evq_count = 0;

fail_kvarg_perf_profile:
	sfc_log_init(sa, "failed %d", rc);
	return rc;
}

void
sfc_ev_fini(struct sfc_adapter *sa)
{
	int sw_index;

	sfc_log_init(sa, "entry");

	/* Cleanup all event queues */
	sw_index = sa->evq_count;
	while (--sw_index >= 0) {
		if (sa->evq_info[sw_index].evq != NULL)
			sfc_ev_qfini(sa, sw_index);
		sfc_ev_qfini_info(sa, sw_index);
	}

	rte_free(sa->evq_info);
	sa->evq_info = NULL;
	sa->evq_count = 0;
}
