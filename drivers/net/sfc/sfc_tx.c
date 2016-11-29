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

#include "sfc.h"
#include "sfc_log.h"
#include "sfc_ev.h"
#include "sfc_tx.h"

static int
sfc_tx_qinit_info(struct sfc_adapter *sa, unsigned int sw_index)
{
	sfc_log_init(sa, "TxQ = %u", sw_index);

	return 0;
}

int
sfc_tx_init(struct sfc_adapter *sa)
{
	unsigned int sw_index;
	int rc = 0;

	sa->txq_count = sa->eth_dev->data->nb_tx_queues;

	sa->txq_info = rte_calloc_socket("sfc-txqs", sa->txq_count,
					 sizeof(sa->txq_info[0]), 0,
					 sa->socket_id);
	if (sa->txq_info == NULL)
		goto fail_txqs_alloc;

	for (sw_index = 0; sw_index < sa->txq_count; ++sw_index) {
		rc = sfc_tx_qinit_info(sa, sw_index);
		if (rc != 0)
			goto fail_tx_qinit_info;
	}

	return 0;

fail_tx_qinit_info:
	rte_free(sa->txq_info);
	sa->txq_info = NULL;

fail_txqs_alloc:
	sa->txq_count = 0;

	sfc_log_init(sa, "failed (rc = %d)", rc);
	return rc;
}

void
sfc_tx_fini(struct sfc_adapter *sa)
{
	rte_free(sa->txq_info);
	sa->txq_info = NULL;
	sa->txq_count = 0;
}
