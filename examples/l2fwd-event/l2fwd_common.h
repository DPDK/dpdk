/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef __L2FWD_COMMON_H__
#define __L2FWD_COMMON_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <netinet/in.h>
#include <setjmp.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>

#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_eventdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_spinlock.h>

#define MAX_PKT_BURST 32
#define MAX_RX_QUEUE_PER_LCORE 16
#define MAX_TX_QUEUE_PER_PORT 16

#define RTE_TEST_RX_DESC_DEFAULT 1024
#define RTE_TEST_TX_DESC_DEFAULT 1024

#define BURST_TX_DRAIN_US 100 /* TX drain every ~100us */
#define MEMPOOL_CACHE_SIZE 256

#define DEFAULT_TIMER_PERIOD	10 /* default period is 10 seconds */
#define MAX_TIMER_PERIOD	86400 /* 1 day max */

#define VECTOR_SIZE_DEFAULT   MAX_PKT_BURST
#define VECTOR_TMO_NS_DEFAULT 1E6 /* 1ms */

/* Per-port statistics struct */
struct l2fwd_port_statistics {
	uint64_t dropped;
	uint64_t tx;
	uint64_t rx;
} __rte_cache_aligned;

/* Event vector attributes */
struct l2fwd_event_vector_params {
	uint8_t enabled;
	uint16_t size;
	uint64_t timeout_ns;
};

struct l2fwd_resources {
	volatile uint8_t force_quit;
	uint8_t event_mode;
	uint8_t sched_type;
	uint8_t mac_updating;
	uint8_t rx_queue_per_lcore;
	bool port_pairs;
	uint16_t nb_rxd;
	uint16_t nb_txd;
	uint32_t enabled_port_mask;
	uint64_t timer_period;
	struct rte_mempool *pktmbuf_pool;
	struct rte_mempool *evt_vec_pool;
	uint32_t dst_ports[RTE_MAX_ETHPORTS];
	struct rte_ether_addr eth_addr[RTE_MAX_ETHPORTS];
	struct l2fwd_port_statistics port_stats[RTE_MAX_ETHPORTS];
	struct l2fwd_event_vector_params evt_vec;
	void *evt_rsrc;
	void *poll_rsrc;
} __rte_cache_aligned;

static __rte_always_inline void
l2fwd_mac_updating(struct rte_mbuf *m, uint32_t dest_port_id,
		   struct rte_ether_addr *addr)
{
	struct rte_ether_hdr *eth;
	void *tmp;

	eth = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);

	/* 02:00:00:00:00:xx */
	tmp = &eth->dst_addr.addr_bytes[0];
	*((uint64_t *)tmp) = 0x000000000002 + ((uint64_t)dest_port_id << 40);

	/* src addr */
	rte_ether_addr_copy(addr, &eth->src_addr);
}

static __rte_always_inline struct l2fwd_resources *
l2fwd_get_rsrc(void)
{
	static const char name[RTE_MEMZONE_NAMESIZE] = "rsrc";
	const struct rte_memzone *mz;

	mz = rte_memzone_lookup(name);
	if (mz != NULL)
		return mz->addr;

	mz = rte_memzone_reserve(name, sizeof(struct l2fwd_resources), 0, 0);
	if (mz != NULL) {
		struct l2fwd_resources *rsrc = mz->addr;

		memset(rsrc, 0, sizeof(struct l2fwd_resources));
		rsrc->mac_updating = true;
		rsrc->event_mode = true;
		rsrc->rx_queue_per_lcore = 1;
		rsrc->sched_type = RTE_SCHED_TYPE_ATOMIC;
		rsrc->timer_period = 10 * rte_get_timer_hz();

		return mz->addr;
	}

	rte_panic("Unable to allocate memory for l2fwd resources\n");

	return NULL;
}

int l2fwd_event_init_ports(struct l2fwd_resources *rsrc);

#endif /* __L2FWD_COMMON_H__ */
