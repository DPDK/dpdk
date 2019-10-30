/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef __L2FWD_POLL_H__
#define __L2FWD_POLL_H__

#include "l2fwd_common.h"

typedef void (*poll_main_loop_cb)(struct l2fwd_resources *rsrc);

struct lcore_queue_conf {
	uint32_t rx_port_list[MAX_RX_QUEUE_PER_LCORE];
	uint32_t n_rx_port;
} __rte_cache_aligned;

struct l2fwd_poll_resources {
	poll_main_loop_cb poll_main_loop;
	struct rte_eth_dev_tx_buffer *tx_buffer[RTE_MAX_ETHPORTS];
	struct lcore_queue_conf lcore_queue_conf[RTE_MAX_LCORE];
};

void l2fwd_poll_resource_setup(struct l2fwd_resources *rsrc);

#endif
