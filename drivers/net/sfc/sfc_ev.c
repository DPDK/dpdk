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

#include "efx.h"

#include "sfc.h"
#include "sfc_debug.h"
#include "sfc_log.h"
#include "sfc_ev.h"


/* Initial delay when waiting for event queue init complete event */
#define SFC_EVQ_INIT_BACKOFF_START_US	(1)
/* Maximum delay between event queue polling attempts */
#define SFC_EVQ_INIT_BACKOFF_MAX_US	(10 * 1000)
/* Event queue init approx timeout */
#define SFC_EVQ_INIT_TIMEOUT_US		(2 * US_PER_S)


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
sfc_ev_rx(void *arg, __rte_unused uint32_t label, __rte_unused uint32_t id,
	  __rte_unused uint32_t size, __rte_unused uint16_t flags)
{
	struct sfc_evq *evq = arg;

	sfc_err(evq->sa, "EVQ %u unexpected Rx event", evq->evq_index);
	return B_TRUE;
}

static boolean_t
sfc_ev_tx(void *arg, __rte_unused uint32_t label, __rte_unused uint32_t id)
{
	struct sfc_evq *evq = arg;

	sfc_err(evq->sa, "EVQ %u unexpected Tx event", evq->evq_index);
	return B_TRUE;
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

	sfc_err(evq->sa, "EVQ %u unexpected Rx flush done event",
		evq->evq_index);
	return B_TRUE;
}

static boolean_t
sfc_ev_rxq_flush_failed(void *arg, __rte_unused uint32_t rxq_hw_index)
{
	struct sfc_evq *evq = arg;

	sfc_err(evq->sa, "EVQ %u unexpected Rx flush failed event",
		evq->evq_index);
	return B_TRUE;
}

static boolean_t
sfc_ev_txq_flush_done(void *arg, __rte_unused uint32_t txq_hw_index)
{
	struct sfc_evq *evq = arg;

	sfc_err(evq->sa, "EVQ %u unexpected Tx flush done event",
		evq->evq_index);
	return B_TRUE;
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
sfc_ev_link_change(void *arg, __rte_unused efx_link_mode_t link_mode)
{
	struct sfc_evq *evq = arg;

	sfc_err(evq->sa, "EVQ %u unexpected link change",
		evq->evq_index);
	return B_TRUE;
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

	/* Poll-mode driver does not re-prime the event queue for interrupts */
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
			    0 /* unused on EF10 */, 0,
			    EFX_EVQ_FLAGS_TYPE_THROUGHPUT |
			    EFX_EVQ_FLAGS_NOTIFY_DISABLED,
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

int
sfc_ev_start(struct sfc_adapter *sa)
{
	int rc;

	sfc_log_init(sa, "entry");

	rc = efx_ev_init(sa->nic);
	if (rc != 0)
		goto fail_ev_init;

	/*
	 * Rx/Tx event queues are started/stopped when corresponding queue
	 * is started/stopped.
	 */

	return 0;

fail_ev_init:
	sfc_log_init(sa, "failed %d", rc);
	return rc;
}

void
sfc_ev_stop(struct sfc_adapter *sa)
{
	unsigned int sw_index;

	sfc_log_init(sa, "entry");

	/* Make sure that all event queues are stopped */
	sw_index = sa->evq_count;
	while (sw_index-- > 0)
		sfc_ev_qstop(sa, sw_index);

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

	sa->evq_count = sfc_ev_qcount(sa);
	sa->mgmt_evq_index = 0;

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

	/*
	 * Rx/Tx event queues are created/destroyed when corresponding
	 * Rx/Tx queue is created/destroyed.
	 */

	return 0;

fail_ev_qinit_info:
	while (sw_index-- > 0)
		sfc_ev_qfini_info(sa, sw_index);

	rte_free(sa->evq_info);
	sa->evq_info = NULL;

fail_evqs_alloc:
	sa->evq_count = 0;
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
