/*-
 *   BSD LICENSE
 *
 * Copyright (c) 2017 Solarflare Communications Inc.
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

#include <sys/queue.h>
#include <string.h>
#include <errno.h>

#include <rte_log.h>

#include "sfc_dp.h"

void
sfc_dp_queue_init(struct sfc_dp_queue *dpq, uint16_t port_id, uint16_t queue_id,
		  const struct rte_pci_addr *pci_addr)
{
	dpq->port_id = port_id;
	dpq->queue_id = queue_id;
	dpq->pci_addr = *pci_addr;
}

struct sfc_dp *
sfc_dp_find_by_name(struct sfc_dp_list *head, enum sfc_dp_type type,
		    const char *name)
{
	struct sfc_dp *entry;

	TAILQ_FOREACH(entry, head, links) {
		if (entry->type != type)
			continue;

		if (strcmp(entry->name, name) == 0)
			return entry;
	}

	return NULL;
}

struct sfc_dp *
sfc_dp_find_by_caps(struct sfc_dp_list *head, enum sfc_dp_type type,
		    unsigned int avail_caps)
{
	struct sfc_dp *entry;

	TAILQ_FOREACH(entry, head, links) {
		if (entry->type != type)
			continue;

		/* Take the first matching */
		if (sfc_dp_match_hw_fw_caps(entry, avail_caps))
			return entry;
	}

	return NULL;
}

int
sfc_dp_register(struct sfc_dp_list *head, struct sfc_dp *entry)
{
	if (sfc_dp_find_by_name(head, entry->type, entry->name) != NULL) {
		rte_log(RTE_LOG_ERR, RTE_LOGTYPE_PMD,
			"sfc %s dapapath '%s' already registered\n",
			entry->type == SFC_DP_RX ? "Rx" :
			entry->type == SFC_DP_TX ? "Tx" :
			"unknown",
			entry->name);
		return EEXIST;
	}

	TAILQ_INSERT_TAIL(head, entry, links);

	return 0;
}
