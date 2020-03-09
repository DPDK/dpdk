/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright(c) 2018-2019 Pensando Systems, Inc. All rights reserved.
 */

#include <errno.h>
#include <stdbool.h>

#include <rte_malloc.h>

#include "ionic_lif.h"
#include "ionic_rx_filter.h"

void
ionic_rx_filter_free(struct ionic_rx_filter *f)
{
	LIST_REMOVE(f, by_id);
	LIST_REMOVE(f, by_hash);
	rte_free(f);
}

int
ionic_rx_filter_del(struct ionic_lif *lif, struct ionic_rx_filter *f)
{
	struct ionic_admin_ctx ctx = {
		.pending_work = true,
		.cmd.rx_filter_del = {
			.opcode = IONIC_CMD_RX_FILTER_DEL,
			.filter_id = f->filter_id,
		},
	};

	return ionic_adminq_post(lif, &ctx);
}

int
ionic_rx_filters_init(struct ionic_lif *lif)
{
	uint32_t i;

	rte_spinlock_init(&lif->rx_filters.lock);

	for (i = 0; i < IONIC_RX_FILTER_HLISTS; i++) {
		LIST_INIT(&lif->rx_filters.by_hash[i]);
		LIST_INIT(&lif->rx_filters.by_id[i]);
	}

	return 0;
}

void
ionic_rx_filters_deinit(struct ionic_lif *lif)
{
	struct ionic_rx_filter *f;
	uint32_t i;

	for (i = 0; i < IONIC_RX_FILTER_HLISTS; i++) {
		while (!LIST_EMPTY(&lif->rx_filters.by_id[i])) {
			f = LIST_FIRST(&lif->rx_filters.by_id[i]);
			ionic_rx_filter_free(f);
		}
	}
}

int
ionic_rx_filter_save(struct ionic_lif *lif, uint32_t flow_id,
		uint16_t rxq_index, struct ionic_admin_ctx *ctx)
{
	struct ionic_rx_filter *f;
	uint32_t key;

	f = rte_zmalloc("ionic", sizeof(*f), 0);

	if (!f)
		return -ENOMEM;

	f->flow_id = flow_id;
	f->filter_id = ctx->comp.rx_filter_add.filter_id;
	f->rxq_index = rxq_index;
	memcpy(&f->cmd, &ctx->cmd, sizeof(f->cmd));

	switch (f->cmd.match) {
	case IONIC_RX_FILTER_MATCH_VLAN:
		key = f->cmd.vlan.vlan & IONIC_RX_FILTER_HLISTS_MASK;
		break;
	case IONIC_RX_FILTER_MATCH_MAC:
		memcpy(&key, f->cmd.mac.addr, sizeof(key));
		key &= IONIC_RX_FILTER_HLISTS_MASK;
		break;
	case IONIC_RX_FILTER_MATCH_MAC_VLAN:
		key = f->cmd.mac_vlan.vlan & IONIC_RX_FILTER_HLISTS_MASK;
		break;
	default:
		return -EINVAL;
	}

	rte_spinlock_lock(&lif->rx_filters.lock);

	LIST_INSERT_HEAD(&lif->rx_filters.by_hash[key], f, by_hash);

	key = f->filter_id & IONIC_RX_FILTER_HLISTS_MASK;

	LIST_INSERT_HEAD(&lif->rx_filters.by_id[key], f, by_id);

	rte_spinlock_unlock(&lif->rx_filters.lock);

	return 0;
}

struct ionic_rx_filter *
ionic_rx_filter_by_vlan(struct ionic_lif *lif, uint16_t vid)
{
	uint32_t key = vid & IONIC_RX_FILTER_HLISTS_MASK;
	struct ionic_rx_filter *f;

	LIST_FOREACH(f, &lif->rx_filters.by_hash[key], by_hash) {
		if (f->cmd.match != IONIC_RX_FILTER_MATCH_VLAN)
			continue;
		if (f->cmd.vlan.vlan == vid)
			return f;
	}

	return NULL;
}

struct ionic_rx_filter *
ionic_rx_filter_by_addr(struct ionic_lif *lif, const uint8_t *addr)
{
	const uint32_t key = *(const uint32_t *)addr &
		IONIC_RX_FILTER_HLISTS_MASK;
	struct ionic_rx_filter *f;

	LIST_FOREACH(f, &lif->rx_filters.by_hash[key], by_hash) {
		if (f->cmd.match != IONIC_RX_FILTER_MATCH_MAC)
			continue;
		if (memcmp(addr, f->cmd.mac.addr, RTE_ETHER_ADDR_LEN) == 0)
			return f;
	}

	return NULL;
}
