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
#include "sfc_rx.h"

static int
sfc_rx_qinit_info(struct sfc_adapter *sa, unsigned int sw_index)
{
	struct sfc_rxq_info *rxq_info = &sa->rxq_info[sw_index];
	unsigned int max_entries;

	max_entries = EFX_RXQ_MAXNDESCS;
	SFC_ASSERT(rte_is_power_of_2(max_entries));

	rxq_info->max_entries = max_entries;

	return 0;
}

/**
 * Initialize Rx subsystem.
 *
 * Called at device configuration stage when number of receive queues is
 * specified together with other device level receive configuration.
 *
 * It should be used to allocate NUMA-unaware resources.
 */
int
sfc_rx_init(struct sfc_adapter *sa)
{
	unsigned int sw_index;
	int rc;

	sa->rxq_count = sa->eth_dev->data->nb_rx_queues;

	rc = ENOMEM;
	sa->rxq_info = rte_calloc_socket("sfc-rxqs", sa->rxq_count,
					 sizeof(struct sfc_rxq_info), 0,
					 sa->socket_id);
	if (sa->rxq_info == NULL)
		goto fail_rxqs_alloc;

	for (sw_index = 0; sw_index < sa->rxq_count; ++sw_index) {
		rc = sfc_rx_qinit_info(sa, sw_index);
		if (rc != 0)
			goto fail_rx_qinit_info;
	}

	return 0;

fail_rx_qinit_info:
	rte_free(sa->rxq_info);
	sa->rxq_info = NULL;

fail_rxqs_alloc:
	sa->rxq_count = 0;
	sfc_log_init(sa, "failed %d", rc);
	return rc;
}

/**
 * Shutdown Rx subsystem.
 *
 * Called at device close stage, for example, before device
 * reconfiguration or shutdown.
 */
void
sfc_rx_fini(struct sfc_adapter *sa)
{
	rte_free(sa->rxq_info);
	sa->rxq_info = NULL;
	sa->rxq_count = 0;
}
