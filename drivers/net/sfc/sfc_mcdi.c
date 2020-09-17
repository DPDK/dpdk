/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2020 Xilinx, Inc.
 * Copyright(c) 2016-2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#include "sfc_efx_mcdi.h"

#include "sfc.h"
#include "sfc_debug.h"
#include "sfc_log.h"
#include "sfc_ev.h"

static sfc_efx_mcdi_dma_alloc_cb sfc_mcdi_dma_alloc;
static int
sfc_mcdi_dma_alloc(void *cookie, const char *name, size_t len,
		   efsys_mem_t *esmp)
{
	const struct sfc_adapter *sa = cookie;

	return sfc_dma_alloc(sa, name, 0, len, sa->socket_id, esmp);
}

static sfc_efx_mcdi_dma_free_cb sfc_mcdi_dma_free;
static void
sfc_mcdi_dma_free(void *cookie, efsys_mem_t *esmp)
{
	const struct sfc_adapter *sa = cookie;

	sfc_dma_free(sa, esmp);
}

static sfc_efx_mcdi_sched_restart_cb sfc_mcdi_sched_restart;
static void
sfc_mcdi_sched_restart(void *cookie)
{
	struct sfc_adapter *sa = cookie;

	sfc_schedule_restart(sa);
}

static sfc_efx_mcdi_mgmt_evq_poll_cb sfc_mcdi_mgmt_evq_poll;
static void
sfc_mcdi_mgmt_evq_poll(void *cookie)
{
	struct sfc_adapter *sa = cookie;

	sfc_ev_mgmt_qpoll(sa);
}

static const struct sfc_efx_mcdi_ops sfc_mcdi_ops = {
	.dma_alloc	= sfc_mcdi_dma_alloc,
	.dma_free	= sfc_mcdi_dma_free,
	.sched_restart	= sfc_mcdi_sched_restart,
	.mgmt_evq_poll	= sfc_mcdi_mgmt_evq_poll,
};

int
sfc_mcdi_init(struct sfc_adapter *sa)
{
	uint32_t logtype;

	sfc_log_init(sa, "entry");

	logtype = sfc_register_logtype(&sa->priv.shared->pci_addr,
				       SFC_LOGTYPE_MCDI_STR,
				       RTE_LOG_NOTICE);

	return sfc_efx_mcdi_init(&sa->mcdi, logtype,
				 sa->priv.shared->log_prefix, sa->nic,
				 &sfc_mcdi_ops, sa);
}

void
sfc_mcdi_fini(struct sfc_adapter *sa)
{
	sfc_log_init(sa, "entry");
	sfc_efx_mcdi_fini(&sa->mcdi);
}
