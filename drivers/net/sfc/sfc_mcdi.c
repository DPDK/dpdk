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

#include <rte_cycles.h>

#include "efx.h"
#include "efx_mcdi.h"
#include "efx_regs_mcdi.h"

#include "sfc.h"
#include "sfc_log.h"
#include "sfc_kvargs.h"
#include "sfc_ev.h"

#define SFC_MCDI_POLL_INTERVAL_MIN_US	10		/* 10us in 1us units */
#define SFC_MCDI_POLL_INTERVAL_MAX_US	(US_PER_S / 10)	/* 100ms in 1us units */
#define SFC_MCDI_WATCHDOG_INTERVAL_US	(10 * US_PER_S)	/* 10s in 1us units */

static void
sfc_mcdi_timeout(struct sfc_adapter *sa)
{
	sfc_warn(sa, "MC TIMEOUT");

	sfc_panic(sa, "MCDI timeout handling is not implemented\n");
}

static inline boolean_t
sfc_mcdi_proxy_event_available(struct sfc_adapter *sa)
{
	struct sfc_mcdi *mcdi = &sa->mcdi;

	mcdi->proxy_handle = 0;
	mcdi->proxy_result = ETIMEDOUT;
	sfc_ev_mgmt_qpoll(sa);
	if (mcdi->proxy_result != ETIMEDOUT)
		return B_TRUE;

	return B_FALSE;
}

static void
sfc_mcdi_poll(struct sfc_adapter *sa, boolean_t proxy)
{
	efx_nic_t *enp;
	unsigned int delay_total;
	unsigned int delay_us;
	boolean_t aborted __rte_unused;

	delay_total = 0;
	delay_us = SFC_MCDI_POLL_INTERVAL_MIN_US;
	enp = sa->nic;

	do {
		boolean_t poll_completed;

		poll_completed = (proxy) ? sfc_mcdi_proxy_event_available(sa) :
					   efx_mcdi_request_poll(enp);
		if (poll_completed)
			return;

		if (delay_total > SFC_MCDI_WATCHDOG_INTERVAL_US) {
			if (!proxy) {
				aborted = efx_mcdi_request_abort(enp);
				SFC_ASSERT(aborted);
				sfc_mcdi_timeout(sa);
			}

			return;
		}

		rte_delay_us(delay_us);

		delay_total += delay_us;

		/* Exponentially back off the poll frequency */
		RTE_BUILD_BUG_ON(SFC_MCDI_POLL_INTERVAL_MAX_US > UINT_MAX / 2);
		delay_us *= 2;
		if (delay_us > SFC_MCDI_POLL_INTERVAL_MAX_US)
			delay_us = SFC_MCDI_POLL_INTERVAL_MAX_US;

	} while (1);
}

static void
sfc_mcdi_execute(void *arg, efx_mcdi_req_t *emrp)
{
	struct sfc_adapter *sa = (struct sfc_adapter *)arg;
	struct sfc_mcdi *mcdi = &sa->mcdi;
	uint32_t proxy_handle;

	rte_spinlock_lock(&mcdi->lock);

	SFC_ASSERT(mcdi->state == SFC_MCDI_INITIALIZED);

	efx_mcdi_request_start(sa->nic, emrp, B_FALSE);
	sfc_mcdi_poll(sa, B_FALSE);

	if (efx_mcdi_get_proxy_handle(sa->nic, emrp, &proxy_handle) == 0) {
		/*
		 * Authorization is required for the MCDI request;
		 * wait for an MCDI proxy response event to bring
		 * a non-zero proxy handle (should be the same as
		 * the value obtained above) and operation status
		 */
		sfc_mcdi_poll(sa, B_TRUE);

		if ((mcdi->proxy_handle != 0) &&
		    (mcdi->proxy_handle != proxy_handle)) {
			sfc_err(sa, "Unexpected MCDI proxy event");
			emrp->emr_rc = EFAULT;
		} else if (mcdi->proxy_result == 0) {
			/*
			 * Authorization succeeded; re-issue the original
			 * request and poll for an ordinary MCDI response
			 */
			efx_mcdi_request_start(sa->nic, emrp, B_FALSE);
			sfc_mcdi_poll(sa, B_FALSE);
		} else {
			emrp->emr_rc = mcdi->proxy_result;
			sfc_err(sa, "MCDI proxy authorization failed "
				    "(handle=%08x, result=%d)",
				    proxy_handle, mcdi->proxy_result);
		}
	}

	rte_spinlock_unlock(&mcdi->lock);
}

static void
sfc_mcdi_ev_cpl(void *arg)
{
	struct sfc_adapter *sa = (struct sfc_adapter *)arg;
	struct sfc_mcdi *mcdi __rte_unused;

	mcdi = &sa->mcdi;
	SFC_ASSERT(mcdi->state == SFC_MCDI_INITIALIZED);

	/* MCDI is polled, completions are not expected */
	SFC_ASSERT(0);
}

static void
sfc_mcdi_exception(void *arg, efx_mcdi_exception_t eme)
{
	struct sfc_adapter *sa = (struct sfc_adapter *)arg;

	sfc_warn(sa, "MC %s",
	    (eme == EFX_MCDI_EXCEPTION_MC_REBOOT) ? "REBOOT" :
	    (eme == EFX_MCDI_EXCEPTION_MC_BADASSERT) ? "BADASSERT" : "UNKNOWN");

	sfc_panic(sa, "MCDI exceptions handling is not implemented\n");
}

#define SFC_MCDI_LOG_BUF_SIZE	128

static size_t
sfc_mcdi_do_log(const struct sfc_adapter *sa,
		char *buffer, void *data, size_t data_size,
		size_t pfxsize, size_t position)
{
	uint32_t *words = data;
	/* Space separator plus 2 characters per byte */
	const size_t word_str_space = 1 + 2 * sizeof(*words);
	size_t i;

	for (i = 0; i < data_size; i += sizeof(*words)) {
		if (position + word_str_space >=
		    SFC_MCDI_LOG_BUF_SIZE) {
			/* Flush at SFC_MCDI_LOG_BUF_SIZE with backslash
			 * at the end which is required by netlogdecode.
			 */
			buffer[position] = '\0';
			sfc_info(sa, "%s \\", buffer);
			/* Preserve prefix for the next log message */
			position = pfxsize;
		}
		position += snprintf(buffer + position,
				     SFC_MCDI_LOG_BUF_SIZE - position,
				     " %08x", *words);
		words++;
	}
	return position;
}

static void
sfc_mcdi_logger(void *arg, efx_log_msg_t type,
		void *header, size_t header_size,
		void *data, size_t data_size)
{
	struct sfc_adapter *sa = (struct sfc_adapter *)arg;
	char buffer[SFC_MCDI_LOG_BUF_SIZE];
	size_t pfxsize;
	size_t start;

	if (!sa->mcdi.logging)
		return;

	/* The format including prefix added by sfc_info() is the format
	 * consumed by the Solarflare netlogdecode tool.
	 */
	pfxsize = snprintf(buffer, sizeof(buffer), "MCDI RPC %s:",
			   type == EFX_LOG_MCDI_REQUEST ? "REQ" :
			   type == EFX_LOG_MCDI_RESPONSE ? "RESP" : "???");
	start = sfc_mcdi_do_log(sa, buffer, header, header_size,
				pfxsize, pfxsize);
	start = sfc_mcdi_do_log(sa, buffer, data, data_size, pfxsize, start);
	if (start != pfxsize) {
		buffer[start] = '\0';
		sfc_info(sa, "%s", buffer);
	}
}

static void
sfc_mcdi_ev_proxy_response(void *arg, uint32_t handle, efx_rc_t result)
{
	struct sfc_adapter *sa = (struct sfc_adapter *)arg;
	struct sfc_mcdi *mcdi = &sa->mcdi;

	mcdi->proxy_handle = handle;
	mcdi->proxy_result = result;
}

int
sfc_mcdi_init(struct sfc_adapter *sa)
{
	struct sfc_mcdi *mcdi;
	size_t max_msg_size;
	efx_mcdi_transport_t *emtp;
	int rc;

	sfc_log_init(sa, "entry");

	mcdi = &sa->mcdi;

	SFC_ASSERT(mcdi->state == SFC_MCDI_UNINITIALIZED);

	rte_spinlock_init(&mcdi->lock);

	mcdi->state = SFC_MCDI_INITIALIZED;

	max_msg_size = sizeof(uint32_t) + MCDI_CTL_SDU_LEN_MAX_V2;
	rc = sfc_dma_alloc(sa, "mcdi", 0, max_msg_size, sa->socket_id,
			   &mcdi->mem);
	if (rc != 0)
		goto fail_dma_alloc;

	/* Convert negative error to positive used in the driver */
	rc = sfc_kvargs_process(sa, SFC_KVARG_MCDI_LOGGING,
				sfc_kvarg_bool_handler, &mcdi->logging);
	if (rc != 0)
		goto fail_kvargs_process;

	emtp = &mcdi->transport;
	emtp->emt_context = sa;
	emtp->emt_dma_mem = &mcdi->mem;
	emtp->emt_execute = sfc_mcdi_execute;
	emtp->emt_ev_cpl = sfc_mcdi_ev_cpl;
	emtp->emt_exception = sfc_mcdi_exception;
	emtp->emt_logger = sfc_mcdi_logger;
	emtp->emt_ev_proxy_response = sfc_mcdi_ev_proxy_response;

	sfc_log_init(sa, "init MCDI");
	rc = efx_mcdi_init(sa->nic, emtp);
	if (rc != 0)
		goto fail_mcdi_init;

	return 0;

fail_mcdi_init:
	memset(emtp, 0, sizeof(*emtp));

fail_kvargs_process:
	sfc_dma_free(sa, &mcdi->mem);

fail_dma_alloc:
	mcdi->state = SFC_MCDI_UNINITIALIZED;
	return rc;
}

void
sfc_mcdi_fini(struct sfc_adapter *sa)
{
	struct sfc_mcdi *mcdi;
	efx_mcdi_transport_t *emtp;

	sfc_log_init(sa, "entry");

	mcdi = &sa->mcdi;
	emtp = &mcdi->transport;

	rte_spinlock_lock(&mcdi->lock);

	SFC_ASSERT(mcdi->state == SFC_MCDI_INITIALIZED);
	mcdi->state = SFC_MCDI_UNINITIALIZED;

	sfc_log_init(sa, "fini MCDI");
	efx_mcdi_fini(sa->nic);
	memset(emtp, 0, sizeof(*emtp));

	rte_spinlock_unlock(&mcdi->lock);

	sfc_dma_free(sa, &mcdi->mem);
}
