/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020-2021 Xilinx, Inc.
 */

#include "sfc_efx_mcdi.h"

#include "sfc_vdpa.h"
#include "sfc_vdpa_debug.h"
#include "sfc_vdpa_log.h"

static sfc_efx_mcdi_dma_alloc_cb sfc_vdpa_mcdi_dma_alloc;
static int
sfc_vdpa_mcdi_dma_alloc(void *cookie, const char *name, size_t len,
			efsys_mem_t *esmp)
{
	struct sfc_vdpa_adapter *sva = cookie;

	return sfc_vdpa_dma_alloc(sva, name, len, esmp);
}

static sfc_efx_mcdi_dma_free_cb sfc_vdpa_mcdi_dma_free;
static void
sfc_vdpa_mcdi_dma_free(void *cookie, efsys_mem_t *esmp)
{
	struct sfc_vdpa_adapter *sva = cookie;

	sfc_vdpa_dma_free(sva, esmp);
}

static sfc_efx_mcdi_sched_restart_cb sfc_vdpa_mcdi_sched_restart;
static void
sfc_vdpa_mcdi_sched_restart(void *cookie)
{
	RTE_SET_USED(cookie);
}

static sfc_efx_mcdi_mgmt_evq_poll_cb sfc_vdpa_mcdi_mgmt_evq_poll;
static void
sfc_vdpa_mcdi_mgmt_evq_poll(void *cookie)
{
	RTE_SET_USED(cookie);
}

static const struct sfc_efx_mcdi_ops sfc_vdpa_mcdi_ops = {
	.dma_alloc	= sfc_vdpa_mcdi_dma_alloc,
	.dma_free	= sfc_vdpa_mcdi_dma_free,
	.sched_restart  = sfc_vdpa_mcdi_sched_restart,
	.mgmt_evq_poll  = sfc_vdpa_mcdi_mgmt_evq_poll,

};

int
sfc_vdpa_mcdi_init(struct sfc_vdpa_adapter *sva)
{
	uint32_t logtype;

	sfc_vdpa_log_init(sva, "entry");

	logtype = sfc_vdpa_register_logtype(&(sva->pdev->addr),
					    SFC_VDPA_LOGTYPE_MCDI_STR,
					    RTE_LOG_NOTICE);

	return sfc_efx_mcdi_init(&sva->mcdi, logtype,
				 sva->log_prefix, sva->nic,
				 &sfc_vdpa_mcdi_ops, sva);
}

void
sfc_vdpa_mcdi_fini(struct sfc_vdpa_adapter *sva)
{
	sfc_vdpa_log_init(sva, "entry");
	sfc_efx_mcdi_fini(&sva->mcdi);
}
