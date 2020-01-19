/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright(c) 2018-2019 Pensando Systems, Inc. All rights reserved.
 */

#ifndef _IONIC_RX_FILTER_H_
#define _IONIC_RX_FILTER_H_

#include <rte_spinlock.h>

#include "ionic_osdep.h"
#include "ionic_if.h"

#define IONIC_RXQ_INDEX_ANY		(0xFFFF)
struct ionic_rx_filter {
	uint32_t flow_id;
	uint32_t filter_id;
	uint16_t rxq_index;
	struct ionic_rx_filter_add_cmd cmd;
	LIST_ENTRY(ionic_rx_filter) by_hash;
	LIST_ENTRY(ionic_rx_filter) by_id;
};

#define IONIC_RX_FILTER_HLISTS	(1 << 10)
#define IONIC_RX_FILTER_HLISTS_MASK	(IONIC_RX_FILTER_HLISTS - 1)
struct ionic_rx_filters {
	rte_spinlock_t lock;
	LIST_HEAD(rx_filters_by_hash, ionic_rx_filter)
		by_hash[IONIC_RX_FILTER_HLISTS]; /* by pkt hash */
	LIST_HEAD(rx_filters_by_id,   ionic_rx_filter)
		by_id[IONIC_RX_FILTER_HLISTS];   /* by filter_id */
};

struct ionic_admin_ctx;
struct ionic_lif;

void ionic_rx_filter_free(struct ionic_rx_filter *f);
int ionic_rx_filter_del(struct ionic_lif *lif, struct ionic_rx_filter *f);
int ionic_rx_filters_init(struct ionic_lif *lif);
void ionic_rx_filters_deinit(struct ionic_lif *lif);
int ionic_rx_filter_save(struct ionic_lif *lif, uint32_t flow_id,
	uint16_t rxq_index, struct ionic_admin_ctx *ctx);
struct ionic_rx_filter *ionic_rx_filter_by_vlan(struct ionic_lif *lif,
	uint16_t vid);
struct ionic_rx_filter *ionic_rx_filter_by_addr(struct ionic_lif *lif,
	const uint8_t *addr);

#endif /* _IONIC_RX_FILTER_H_ */
