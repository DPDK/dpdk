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

#include "efx.h"

#include "sfc.h"
#include "sfc_log.h"

int
sfc_intr_start(struct sfc_adapter *sa)
{
	struct sfc_intr *intr = &sa->intr;
	struct rte_intr_handle *intr_handle;
	struct rte_pci_device *pci_dev;
	int rc;

	sfc_log_init(sa, "entry");

	/*
	 * The EFX common code event queue module depends on the interrupt
	 * module. Ensure that the interrupt module is always initialized
	 * (even if interrupts are not used).  Status memory is required
	 * for Siena only and may be NULL for EF10.
	 */
	sfc_log_init(sa, "efx_intr_init");
	rc = efx_intr_init(sa->nic, intr->type, NULL);
	if (rc != 0)
		goto fail_intr_init;

	pci_dev = SFC_DEV_TO_PCI(sa->eth_dev);
	intr_handle = &pci_dev->intr_handle;

	sfc_log_init(sa, "done type=%u max_intr=%d nb_efd=%u vec=%p",
		     intr_handle->type, intr_handle->max_intr,
		     intr_handle->nb_efd, intr_handle->intr_vec);
	return 0;

fail_intr_init:
	sfc_log_init(sa, "failed %d", rc);
	return rc;
}

void
sfc_intr_stop(struct sfc_adapter *sa)
{
	sfc_log_init(sa, "entry");

	efx_intr_fini(sa->nic);

	sfc_log_init(sa, "done");
}

int
sfc_intr_init(struct sfc_adapter *sa)
{
	sfc_log_init(sa, "entry");

	sfc_log_init(sa, "done");
	return 0;
}

void
sfc_intr_fini(struct sfc_adapter *sa)
{
	sfc_log_init(sa, "entry");

	sfc_log_init(sa, "done");
}

int
sfc_intr_attach(struct sfc_adapter *sa)
{
	struct sfc_intr *intr = &sa->intr;
	struct rte_pci_device *pci_dev = SFC_DEV_TO_PCI(sa->eth_dev);

	sfc_log_init(sa, "entry");

	switch (pci_dev->intr_handle.type) {
#ifdef RTE_EXEC_ENV_LINUXAPP
	case RTE_INTR_HANDLE_VFIO_LEGACY:
		intr->type = EFX_INTR_LINE;
		break;
	case RTE_INTR_HANDLE_VFIO_MSI:
	case RTE_INTR_HANDLE_VFIO_MSIX:
		intr->type = EFX_INTR_MESSAGE;
		break;
#endif
	default:
		intr->type = EFX_INTR_INVALID;
		break;
	}

	sfc_log_init(sa, "done");
	return 0;
}

void
sfc_intr_detach(struct sfc_adapter *sa)
{
	sfc_log_init(sa, "entry");

	sa->intr.type = EFX_INTR_INVALID;

	sfc_log_init(sa, "done");
}
