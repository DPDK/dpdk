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

/* sysconf() */
#include <unistd.h>

#include <rte_errno.h>

#include "efx.h"

#include "sfc.h"
#include "sfc_log.h"


int
sfc_dma_alloc(const struct sfc_adapter *sa, const char *name, uint16_t id,
	      size_t len, int socket_id, efsys_mem_t *esmp)
{
	const struct rte_memzone *mz;

	sfc_log_init(sa, "name=%s id=%u len=%lu socket_id=%d",
		     name, id, len, socket_id);

	mz = rte_eth_dma_zone_reserve(sa->eth_dev, name, id, len,
				      sysconf(_SC_PAGESIZE), socket_id);
	if (mz == NULL) {
		sfc_err(sa, "cannot reserve DMA zone for %s:%u %#x@%d: %s",
			name, (unsigned int)id, (unsigned int)len, socket_id,
			rte_strerror(rte_errno));
		return ENOMEM;
	}

	esmp->esm_addr = rte_mem_phy2mch(mz->memseg_id, mz->phys_addr);
	if (esmp->esm_addr == RTE_BAD_PHYS_ADDR) {
		(void)rte_memzone_free(mz);
		return EFAULT;
	}

	esmp->esm_mz = mz;
	esmp->esm_base = mz->addr;

	return 0;
}

void
sfc_dma_free(const struct sfc_adapter *sa, efsys_mem_t *esmp)
{
	int rc;

	sfc_log_init(sa, "name=%s", esmp->esm_mz->name);

	rc = rte_memzone_free(esmp->esm_mz);
	if (rc != 0)
		sfc_err(sa, "rte_memzone_free(() failed: %d", rc);

	memset(esmp, 0, sizeof(*esmp));
}

int
sfc_configure(struct sfc_adapter *sa)
{
	sfc_log_init(sa, "entry");

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	SFC_ASSERT(sa->state == SFC_ADAPTER_INITIALIZED);
	sa->state = SFC_ADAPTER_CONFIGURING;

	sa->state = SFC_ADAPTER_CONFIGURED;
	sfc_log_init(sa, "done");
	return 0;
}

void
sfc_close(struct sfc_adapter *sa)
{
	sfc_log_init(sa, "entry");

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	SFC_ASSERT(sa->state == SFC_ADAPTER_CONFIGURED);
	sa->state = SFC_ADAPTER_CLOSING;

	sa->state = SFC_ADAPTER_INITIALIZED;
	sfc_log_init(sa, "done");
}

static int
sfc_mem_bar_init(struct sfc_adapter *sa)
{
	struct rte_eth_dev *eth_dev = sa->eth_dev;
	struct rte_pci_device *pci_dev = SFC_DEV_TO_PCI(eth_dev);
	efsys_bar_t *ebp = &sa->mem_bar;
	unsigned int i;
	struct rte_mem_resource *res;

	for (i = 0; i < RTE_DIM(pci_dev->mem_resource); i++) {
		res = &pci_dev->mem_resource[i];
		if ((res->len != 0) && (res->phys_addr != 0)) {
			/* Found first memory BAR */
			SFC_BAR_LOCK_INIT(ebp, eth_dev->data->name);
			ebp->esb_rid = i;
			ebp->esb_dev = pci_dev;
			ebp->esb_base = res->addr;
			return 0;
		}
	}

	return EFAULT;
}

static void
sfc_mem_bar_fini(struct sfc_adapter *sa)
{
	efsys_bar_t *ebp = &sa->mem_bar;

	SFC_BAR_LOCK_DESTROY(ebp);
	memset(ebp, 0, sizeof(*ebp));
}

int
sfc_attach(struct sfc_adapter *sa)
{
	struct rte_pci_device *pci_dev = SFC_DEV_TO_PCI(sa->eth_dev);
	efx_nic_t *enp;
	int rc;

	sfc_log_init(sa, "entry");

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	sa->socket_id = rte_socket_id();

	sfc_log_init(sa, "init mem bar");
	rc = sfc_mem_bar_init(sa);
	if (rc != 0)
		goto fail_mem_bar_init;

	sfc_log_init(sa, "get family");
	rc = efx_family(pci_dev->id.vendor_id, pci_dev->id.device_id,
			&sa->family);
	if (rc != 0)
		goto fail_family;
	sfc_log_init(sa, "family is %u", sa->family);

	sfc_log_init(sa, "create nic");
	rte_spinlock_init(&sa->nic_lock);
	rc = efx_nic_create(sa->family, (efsys_identifier_t *)sa,
			    &sa->mem_bar, &sa->nic_lock, &enp);
	if (rc != 0)
		goto fail_nic_create;
	sa->nic = enp;

	rc = sfc_mcdi_init(sa);
	if (rc != 0)
		goto fail_mcdi_init;

	sfc_log_init(sa, "probe nic");
	rc = efx_nic_probe(enp);
	if (rc != 0)
		goto fail_nic_probe;

	efx_mcdi_new_epoch(enp);

	sfc_log_init(sa, "reset nic");
	rc = efx_nic_reset(enp);
	if (rc != 0)
		goto fail_nic_reset;

	/* Initialize NIC to double-check hardware */
	sfc_log_init(sa, "init nic");
	rc = efx_nic_init(enp);
	if (rc != 0)
		goto fail_nic_init;

	sfc_log_init(sa, "fini nic");
	efx_nic_fini(enp);

	sa->rxq_max = 1;
	sa->txq_max = 1;

	sa->state = SFC_ADAPTER_INITIALIZED;

	sfc_log_init(sa, "done");
	return 0;

fail_nic_init:
fail_nic_reset:
	sfc_log_init(sa, "unprobe nic");
	efx_nic_unprobe(enp);

fail_nic_probe:
	sfc_mcdi_fini(sa);

fail_mcdi_init:
	sfc_log_init(sa, "destroy nic");
	sa->nic = NULL;
	efx_nic_destroy(enp);

fail_nic_create:
fail_family:
	sfc_mem_bar_fini(sa);

fail_mem_bar_init:
	sfc_log_init(sa, "failed %d", rc);
	return rc;
}

void
sfc_detach(struct sfc_adapter *sa)
{
	efx_nic_t *enp = sa->nic;

	sfc_log_init(sa, "entry");

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	sfc_log_init(sa, "unprobe nic");
	efx_nic_unprobe(enp);

	sfc_mcdi_fini(sa);

	sfc_log_init(sa, "destroy nic");
	sa->nic = NULL;
	efx_nic_destroy(enp);

	sfc_mem_bar_fini(sa);

	sa->state = SFC_ADAPTER_UNINITIALIZED;
}
