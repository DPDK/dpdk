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

/*
 * At the momemt of writing DPDK v16.07 has notion of two types of
 * interrupts: LSC (link status change) and RXQ (receive indication).
 * It allows to register interrupt callback for entire device which is
 * not intended to be used for receive indication (i.e. link status
 * change indication only). The handler has no information which HW
 * interrupt has triggered it, so we don't know which event queue should
 * be polled/reprimed (except qmask in the case of legacy line interrupt).
 */

#include <rte_common.h>
#include <rte_interrupts.h>

#include "efx.h"

#include "sfc.h"
#include "sfc_log.h"
#include "sfc_ev.h"

static void
sfc_intr_handle_mgmt_evq(struct sfc_adapter *sa)
{
	struct sfc_evq *evq;

	rte_spinlock_lock(&sa->mgmt_evq_lock);

	evq = sa->mgmt_evq;

	if (evq->init_state != SFC_EVQ_STARTED) {
		sfc_log_init(sa, "interrupt on stopped EVQ %u", evq->evq_index);
	} else {
		sfc_ev_qpoll(evq);

		if (sfc_ev_qprime(evq) != 0)
			sfc_err(sa, "cannot prime EVQ %u", evq->evq_index);
	}

	rte_spinlock_unlock(&sa->mgmt_evq_lock);
}

static void
sfc_intr_line_handler(void *cb_arg)
{
	struct sfc_adapter *sa = (struct sfc_adapter *)cb_arg;
	efx_nic_t *enp = sa->nic;
	boolean_t fatal;
	uint32_t qmask;
	unsigned int lsc_seq = sa->port.lsc_seq;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(sa->eth_dev);

	sfc_log_init(sa, "entry");

	if (sa->state != SFC_ADAPTER_STARTED &&
	    sa->state != SFC_ADAPTER_STARTING &&
	    sa->state != SFC_ADAPTER_STOPPING) {
		sfc_log_init(sa,
			     "interrupt on stopped adapter, don't reenable");
		goto exit;
	}

	efx_intr_status_line(enp, &fatal, &qmask);
	if (fatal) {
		(void)efx_intr_disable(enp);
		(void)efx_intr_fatal(enp);
		sfc_err(sa, "fatal, interrupts disabled");
		goto exit;
	}

	if (qmask & (1 << sa->mgmt_evq_index))
		sfc_intr_handle_mgmt_evq(sa);

	if (rte_intr_enable(&pci_dev->intr_handle) != 0)
		sfc_err(sa, "cannot reenable interrupts");

	sfc_log_init(sa, "done");

exit:
	if (lsc_seq != sa->port.lsc_seq) {
		sfc_info(sa, "link status change event: link %s",
			 sa->eth_dev->data->dev_link.link_status ?
			 "UP" : "DOWN");
		_rte_eth_dev_callback_process(sa->eth_dev,
					      RTE_ETH_EVENT_INTR_LSC, NULL);
	}
}

static void
sfc_intr_message_handler(void *cb_arg)
{
	struct sfc_adapter *sa = (struct sfc_adapter *)cb_arg;
	efx_nic_t *enp = sa->nic;
	boolean_t fatal;
	unsigned int lsc_seq = sa->port.lsc_seq;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(sa->eth_dev);

	sfc_log_init(sa, "entry");

	if (sa->state != SFC_ADAPTER_STARTED &&
	    sa->state != SFC_ADAPTER_STARTING &&
	    sa->state != SFC_ADAPTER_STOPPING) {
		sfc_log_init(sa, "adapter not-started, don't reenable");
		goto exit;
	}

	efx_intr_status_message(enp, sa->mgmt_evq_index, &fatal);
	if (fatal) {
		(void)efx_intr_disable(enp);
		(void)efx_intr_fatal(enp);
		sfc_err(sa, "fatal, interrupts disabled");
		goto exit;
	}

	sfc_intr_handle_mgmt_evq(sa);

	if (rte_intr_enable(&pci_dev->intr_handle) != 0)
		sfc_err(sa, "cannot reenable interrupts");

	sfc_log_init(sa, "done");

exit:
	if (lsc_seq != sa->port.lsc_seq) {
		sfc_info(sa, "link status change event");
		_rte_eth_dev_callback_process(sa->eth_dev,
					      RTE_ETH_EVENT_INTR_LSC, NULL);
	}
}

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

	pci_dev = RTE_ETH_DEV_TO_PCI(sa->eth_dev);
	intr_handle = &pci_dev->intr_handle;

	if (intr->handler != NULL) {
		sfc_log_init(sa, "rte_intr_callback_register");
		rc = rte_intr_callback_register(intr_handle, intr->handler,
						(void *)sa);
		if (rc != 0) {
			sfc_err(sa,
				"cannot register interrupt handler (rc=%d)",
				rc);
			/*
			 * Convert error code from negative returned by RTE API
			 * to positive used in the driver.
			 */
			rc = -rc;
			goto fail_rte_intr_cb_reg;
		}

		sfc_log_init(sa, "rte_intr_enable");
		rc = rte_intr_enable(intr_handle);
		if (rc != 0) {
			sfc_err(sa, "cannot enable interrupts (rc=%d)", rc);
			/*
			 * Convert error code from negative returned by RTE API
			 * to positive used in the driver.
			 */
			rc = -rc;
			goto fail_rte_intr_enable;
		}

		sfc_log_init(sa, "efx_intr_enable");
		efx_intr_enable(sa->nic);
	}

	sfc_log_init(sa, "done type=%u max_intr=%d nb_efd=%u vec=%p",
		     intr_handle->type, intr_handle->max_intr,
		     intr_handle->nb_efd, intr_handle->intr_vec);
	return 0;

fail_rte_intr_enable:
	rte_intr_callback_unregister(intr_handle, intr->handler, (void *)sa);

fail_rte_intr_cb_reg:
	efx_intr_fini(sa->nic);

fail_intr_init:
	sfc_log_init(sa, "failed %d", rc);
	return rc;
}

void
sfc_intr_stop(struct sfc_adapter *sa)
{
	struct sfc_intr *intr = &sa->intr;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(sa->eth_dev);

	sfc_log_init(sa, "entry");

	if (intr->handler != NULL) {
		struct rte_intr_handle *intr_handle;
		int rc;

		efx_intr_disable(sa->nic);

		intr_handle = &pci_dev->intr_handle;
		if (rte_intr_disable(intr_handle) != 0)
			sfc_err(sa, "cannot disable interrupts");

		while ((rc = rte_intr_callback_unregister(intr_handle,
				intr->handler, (void *)sa)) == -EAGAIN)
			;
		if (rc != 1)
			sfc_err(sa,
				"cannot unregister interrupt handler %d",
				rc);
	}

	efx_intr_fini(sa->nic);

	sfc_log_init(sa, "done");
}

int
sfc_intr_configure(struct sfc_adapter *sa)
{
	struct sfc_intr *intr = &sa->intr;

	sfc_log_init(sa, "entry");

	intr->handler = NULL;
	intr->lsc_intr = (sa->eth_dev->data->dev_conf.intr_conf.lsc != 0);
	if (!intr->lsc_intr) {
		sfc_info(sa, "LSC tracking using interrupts is disabled");
		goto done;
	}

	switch (intr->type) {
	case EFX_INTR_MESSAGE:
		intr->handler = sfc_intr_message_handler;
		break;
	case EFX_INTR_LINE:
		intr->handler = sfc_intr_line_handler;
		break;
	case EFX_INTR_INVALID:
		sfc_warn(sa, "interrupts are not supported");
		break;
	default:
		sfc_panic(sa, "unexpected EFX interrupt type %u\n", intr->type);
		break;
	}

done:
	sfc_log_init(sa, "done");
	return 0;
}

void
sfc_intr_close(struct sfc_adapter *sa)
{
	sfc_log_init(sa, "entry");

	sfc_log_init(sa, "done");
}

int
sfc_intr_attach(struct sfc_adapter *sa)
{
	struct sfc_intr *intr = &sa->intr;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(sa->eth_dev);

	sfc_log_init(sa, "entry");

	switch (pci_dev->intr_handle.type) {
#ifdef RTE_EXEC_ENV_LINUXAPP
	case RTE_INTR_HANDLE_UIO_INTX:
	case RTE_INTR_HANDLE_VFIO_LEGACY:
		intr->type = EFX_INTR_LINE;
		break;
	case RTE_INTR_HANDLE_UIO:
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
