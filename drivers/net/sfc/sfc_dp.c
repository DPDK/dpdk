/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2021 Xilinx, Inc.
 * Copyright(c) 2017-2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#include <sys/queue.h>
#include <string.h>
#include <errno.h>

#include <rte_ethdev.h>
#include <rte_log.h>
#include <rte_mbuf_dyn.h>

#include "efx.h"

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
			"sfc %s datapath '%s' already registered",
			entry->type == SFC_DP_RX ? "Rx" :
			entry->type == SFC_DP_TX ? "Tx" :
			"unknown",
			entry->name);
		return EEXIST;
	}

	TAILQ_INSERT_TAIL(head, entry, links);

	return 0;
}

uint64_t sfc_dp_mport_override;
int sfc_dp_mport_offset = -1;

int
sfc_dp_mport_register(void)
{
	static const struct rte_mbuf_dynfield mport = {
		.name = "rte_net_sfc_dynfield_mport",
		.size = sizeof(efx_mport_id_t),
		.align = __alignof__(efx_mport_id_t),
	};
	static const struct rte_mbuf_dynflag mport_override = {
		.name = "rte_net_sfc_dynflag_mport_override",
	};

	int field_offset;
	int flag;

	if (sfc_dp_mport_override != 0) {
		SFC_GENERIC_LOG(INFO, "%s() already registered", __func__);
		return 0;
	}

	field_offset = rte_mbuf_dynfield_register(&mport);
	if (field_offset < 0) {
		SFC_GENERIC_LOG(ERR, "%s() failed to register mport dynfield",
				__func__);
		return -1;
	}

	flag = rte_mbuf_dynflag_register(&mport_override);
	if (flag < 0) {
		SFC_GENERIC_LOG(ERR, "%s() failed to register mport dynflag",
				__func__);
		return -1;
	}

	sfc_dp_mport_offset = field_offset;
	sfc_dp_mport_override = UINT64_C(1) << flag;

	return 0;
}

int sfc_dp_ft_ctx_id_offset = -1;
uint64_t sfc_dp_ft_ctx_id_valid;

int
sfc_dp_ft_ctx_id_register(void)
{
	static const struct rte_mbuf_dynfield ft_ctx_id = {
		.name = "rte_net_sfc_dynfield_ft_ctx_id",
		.size = sizeof(uint8_t),
		.align = __alignof__(uint8_t),
	};

	int field_offset;

	SFC_GENERIC_LOG(INFO, "%s() entry", __func__);

	if (sfc_dp_ft_ctx_id_valid != 0) {
		SFC_GENERIC_LOG(INFO, "%s() already registered", __func__);
		return 0;
	}

	field_offset = rte_mbuf_dynfield_register(&ft_ctx_id);
	if (field_offset < 0) {
		SFC_GENERIC_LOG(ERR, "%s() failed to register ft_ctx_id dynfield",
				__func__);
		return -1;
	}

	sfc_dp_ft_ctx_id_valid = rte_flow_restore_info_dynflag();
	sfc_dp_ft_ctx_id_offset = field_offset;

	SFC_GENERIC_LOG(INFO, "%s() done", __func__);

	return 0;
}
