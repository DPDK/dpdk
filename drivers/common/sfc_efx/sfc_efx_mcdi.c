/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2021 Xilinx, Inc.
 * Copyright(c) 2016-2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#include <rte_cycles.h>

#include "efx.h"
#include "efx_mcdi.h"
#include "efx_regs_mcdi.h"

#include "sfc_efx_mcdi.h"
#include "sfc_efx_debug.h"

#define SFC_EFX_MCDI_POLL_INTERVAL_MIN_US	10		/* 10us */
#define SFC_EFX_MCDI_POLL_INTERVAL_MAX_US	(US_PER_S / 10)	/* 100ms */
#define SFC_EFX_MCDI_WATCHDOG_INTERVAL_US	(10 * US_PER_S)	/* 10s */

#define sfc_efx_mcdi_log(mcdi, level, ...) \
	do {								\
		const struct sfc_efx_mcdi *_mcdi = (mcdi);		\
									\
		rte_log(level, _mcdi->logtype,				\
			RTE_FMT("%s" RTE_FMT_HEAD(__VA_ARGS__ ,) "\n",	\
				_mcdi->log_prefix,			\
				RTE_FMT_TAIL(__VA_ARGS__,)));		\
	} while (0)

#define sfc_efx_mcdi_crit(mcdi, ...) \
	sfc_efx_mcdi_log(mcdi, RTE_LOG_CRIT, __VA_ARGS__)

#define sfc_efx_mcdi_err(mcdi, ...) \
	sfc_efx_mcdi_log(mcdi, RTE_LOG_ERR, __VA_ARGS__)

#define sfc_efx_mcdi_warn(mcdi, ...) \
	sfc_efx_mcdi_log(mcdi, RTE_LOG_WARNING, __VA_ARGS__)

#define sfc_efx_mcdi_info(mcdi, ...) \
	sfc_efx_mcdi_log(mcdi, RTE_LOG_INFO, __VA_ARGS__)

/** Level value used by MCDI log statements */
#define SFC_EFX_LOG_LEVEL_MCDI	RTE_LOG_INFO

#define sfc_efx_log_mcdi(mcdi, ...) \
	sfc_efx_mcdi_log(mcdi, SFC_EFX_LOG_LEVEL_MCDI, __VA_ARGS__)

static void
sfc_efx_mcdi_timeout(struct sfc_efx_mcdi *mcdi)
{
	sfc_efx_mcdi_warn(mcdi, "MC TIMEOUT");

	sfc_efx_mcdi_crit(mcdi, "MCDI timeout handling is not implemented");
	sfc_efx_mcdi_crit(mcdi, "NIC is unusable");
	mcdi->state = SFC_EFX_MCDI_DEAD;
}

static inline boolean_t
sfc_efx_mcdi_proxy_event_available(struct sfc_efx_mcdi *mcdi)
{
	mcdi->proxy_handle = 0;
	mcdi->proxy_result = ETIMEDOUT;
	mcdi->ops->mgmt_evq_poll(mcdi->ops_cookie);
	if (mcdi->proxy_result != ETIMEDOUT)
		return B_TRUE;

	return B_FALSE;
}

static void
sfc_efx_mcdi_poll(struct sfc_efx_mcdi *mcdi, boolean_t proxy)
{
	efx_nic_t *enp;
	unsigned int delay_total;
	unsigned int delay_us;
	boolean_t aborted __rte_unused;

	delay_total = 0;
	delay_us = SFC_EFX_MCDI_POLL_INTERVAL_MIN_US;
	enp = mcdi->nic;

	do {
		boolean_t poll_completed;

		poll_completed = (proxy) ?
				sfc_efx_mcdi_proxy_event_available(mcdi) :
				efx_mcdi_request_poll(enp);
		if (poll_completed)
			return;

		if (delay_total > SFC_EFX_MCDI_WATCHDOG_INTERVAL_US) {
			if (!proxy) {
				aborted = efx_mcdi_request_abort(enp);
				SFC_EFX_ASSERT(aborted);
				sfc_efx_mcdi_timeout(mcdi);
			}

			return;
		}

		rte_delay_us(delay_us);

		delay_total += delay_us;

		/* Exponentially back off the poll frequency */
		RTE_BUILD_BUG_ON(SFC_EFX_MCDI_POLL_INTERVAL_MAX_US >
				 UINT_MAX / 2);
		delay_us *= 2;
		if (delay_us > SFC_EFX_MCDI_POLL_INTERVAL_MAX_US)
			delay_us = SFC_EFX_MCDI_POLL_INTERVAL_MAX_US;

	} while (1);
}

static void
sfc_efx_mcdi_execute(void *arg, efx_mcdi_req_t *emrp)
{
	struct sfc_efx_mcdi *mcdi = (struct sfc_efx_mcdi *)arg;
	uint32_t proxy_handle;

	if (mcdi->state == SFC_EFX_MCDI_DEAD) {
		emrp->emr_rc = ENOEXEC;
		return;
	}

	rte_spinlock_lock(&mcdi->lock);

	SFC_EFX_ASSERT(mcdi->state == SFC_EFX_MCDI_INITIALIZED);

	efx_mcdi_request_start(mcdi->nic, emrp, B_FALSE);
	sfc_efx_mcdi_poll(mcdi, B_FALSE);

	if (efx_mcdi_get_proxy_handle(mcdi->nic, emrp, &proxy_handle) == 0) {
		/*
		 * Authorization is required for the MCDI request;
		 * wait for an MCDI proxy response event to bring
		 * a non-zero proxy handle (should be the same as
		 * the value obtained above) and operation status
		 */
		sfc_efx_mcdi_poll(mcdi, B_TRUE);

		if ((mcdi->proxy_handle != 0) &&
		    (mcdi->proxy_handle != proxy_handle)) {
			sfc_efx_mcdi_err(mcdi, "Unexpected MCDI proxy event");
			emrp->emr_rc = EFAULT;
		} else if (mcdi->proxy_result == 0) {
			/*
			 * Authorization succeeded; re-issue the original
			 * request and poll for an ordinary MCDI response
			 */
			efx_mcdi_request_start(mcdi->nic, emrp, B_FALSE);
			sfc_efx_mcdi_poll(mcdi, B_FALSE);
		} else {
			emrp->emr_rc = mcdi->proxy_result;
			sfc_efx_mcdi_err(mcdi,
				"MCDI proxy authorization failed (handle=%08x, result=%d)",
				proxy_handle, mcdi->proxy_result);
		}
	}

	rte_spinlock_unlock(&mcdi->lock);
}

static void
sfc_efx_mcdi_ev_cpl(void *arg)
{
	struct sfc_efx_mcdi *mcdi = (struct sfc_efx_mcdi *)arg;

	RTE_SET_USED(mcdi);
	SFC_EFX_ASSERT(mcdi->state == SFC_EFX_MCDI_INITIALIZED);

	/* MCDI is polled, completions are not expected */
	SFC_EFX_ASSERT(0);
}

static void
sfc_efx_mcdi_exception(void *arg, efx_mcdi_exception_t eme)
{
	struct sfc_efx_mcdi *mcdi = (struct sfc_efx_mcdi *)arg;

	sfc_efx_mcdi_warn(mcdi, "MC %s",
	    (eme == EFX_MCDI_EXCEPTION_MC_REBOOT) ? "REBOOT" :
	    (eme == EFX_MCDI_EXCEPTION_MC_BADASSERT) ? "BADASSERT" : "UNKNOWN");

	mcdi->ops->sched_restart(mcdi->ops_cookie);
}

#define SFC_MCDI_LOG_BUF_SIZE	128

static size_t
sfc_efx_mcdi_do_log(const struct sfc_efx_mcdi *mcdi,
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
			sfc_efx_log_mcdi(mcdi, "%s \\", buffer);
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
sfc_efx_mcdi_logger(void *arg, efx_log_msg_t type,
		void *header, size_t header_size,
		void *data, size_t data_size)
{
	struct sfc_efx_mcdi *mcdi = (struct sfc_efx_mcdi *)arg;
	char buffer[SFC_MCDI_LOG_BUF_SIZE];
	size_t pfxsize;
	size_t start;

	/*
	 * Unlike the other cases, MCDI logging implies more onerous work
	 * needed to produce a message. If the dynamic log level prevents
	 * the end result from being printed, the CPU time will be wasted.
	 *
	 * To avoid wasting time, the actual level is examined in advance.
	 */
	if (rte_log_get_level(mcdi->logtype) < (int)SFC_EFX_LOG_LEVEL_MCDI)
		return;

	/* The format including prefix added by sfc_efx_log_mcdi() is the
	 * format consumed by the Solarflare netlogdecode tool.
	 */
	pfxsize = snprintf(buffer, sizeof(buffer), "MCDI RPC %s:",
			   type == EFX_LOG_MCDI_REQUEST ? "REQ" :
			   type == EFX_LOG_MCDI_RESPONSE ? "RESP" : "???");
	start = sfc_efx_mcdi_do_log(mcdi, buffer, header, header_size,
				    pfxsize, pfxsize);
	start = sfc_efx_mcdi_do_log(mcdi, buffer, data, data_size,
				    pfxsize, start);
	if (start != pfxsize) {
		buffer[start] = '\0';
		sfc_efx_log_mcdi(mcdi, "%s", buffer);
	}
}

static void
sfc_efx_mcdi_ev_proxy_response(void *arg, uint32_t handle, efx_rc_t result)
{
	struct sfc_efx_mcdi *mcdi = (struct sfc_efx_mcdi *)arg;

	mcdi->proxy_handle = handle;
	mcdi->proxy_result = result;
}

int
sfc_efx_mcdi_init(struct sfc_efx_mcdi *mcdi,
		  uint32_t logtype, const char *log_prefix, efx_nic_t *nic,
		  const struct sfc_efx_mcdi_ops *ops, void *ops_cookie)
{
	size_t max_msg_size;
	efx_mcdi_transport_t *emtp;
	int rc;

	if (ops->dma_alloc == NULL || ops->dma_free == NULL ||
	    ops->sched_restart == NULL || ops->mgmt_evq_poll == NULL)
		return EINVAL;

	SFC_EFX_ASSERT(mcdi->state == SFC_EFX_MCDI_UNINITIALIZED);

	rte_spinlock_init(&mcdi->lock);

	mcdi->ops = ops;
	mcdi->ops_cookie = ops_cookie;
	mcdi->nic = nic;

	mcdi->state = SFC_EFX_MCDI_INITIALIZED;

	mcdi->logtype = logtype;
	mcdi->log_prefix = log_prefix;

	max_msg_size = sizeof(uint32_t) + MCDI_CTL_SDU_LEN_MAX_V2;
	rc = ops->dma_alloc(ops_cookie, "mcdi", max_msg_size, &mcdi->mem);
	if (rc != 0)
		goto fail_dma_alloc;

	emtp = &mcdi->transport;
	emtp->emt_context = mcdi;
	emtp->emt_dma_mem = &mcdi->mem;
	emtp->emt_execute = sfc_efx_mcdi_execute;
	emtp->emt_ev_cpl = sfc_efx_mcdi_ev_cpl;
	emtp->emt_exception = sfc_efx_mcdi_exception;
	emtp->emt_logger = sfc_efx_mcdi_logger;
	emtp->emt_ev_proxy_response = sfc_efx_mcdi_ev_proxy_response;

	sfc_efx_mcdi_info(mcdi, "init MCDI");
	rc = efx_mcdi_init(mcdi->nic, emtp);
	if (rc != 0)
		goto fail_mcdi_init;

	return 0;

fail_mcdi_init:
	memset(emtp, 0, sizeof(*emtp));
	ops->dma_free(ops_cookie, &mcdi->mem);

fail_dma_alloc:
	mcdi->state = SFC_EFX_MCDI_UNINITIALIZED;
	return rc;
}

void
sfc_efx_mcdi_fini(struct sfc_efx_mcdi *mcdi)
{
	efx_mcdi_transport_t *emtp;

	emtp = &mcdi->transport;

	rte_spinlock_lock(&mcdi->lock);

	SFC_EFX_ASSERT(mcdi->state == SFC_EFX_MCDI_INITIALIZED ||
		       mcdi->state == SFC_EFX_MCDI_DEAD);
	mcdi->state = SFC_EFX_MCDI_UNINITIALIZED;

	sfc_efx_mcdi_info(mcdi, "fini MCDI");
	efx_mcdi_fini(mcdi->nic);
	memset(emtp, 0, sizeof(*emtp));

	rte_spinlock_unlock(&mcdi->lock);

	mcdi->ops->dma_free(mcdi->ops_cookie, &mcdi->mem);
}
