/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2020 Xilinx, Inc.
 * Copyright(c) 2017-2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#include <sys/queue.h>
#include <string.h>
#include <errno.h>

#include <rte_log.h>

#include "sfc_dp.h"
#include "sfc_log.h"

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
		SFC_GENERIC_LOG(ERR,
			"sfc %s dapapath '%s' already registered",
			entry->type == SFC_DP_RX ? "Rx" :
			entry->type == SFC_DP_TX ? "Tx" :
			"unknown",
			entry->name);
		return EEXIST;
	}

	TAILQ_INSERT_TAIL(head, entry, links);

	return 0;
}
