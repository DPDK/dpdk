/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016 Intel Corporation.
 * Copyright 2017 Cavium, Inc.
 */

#include <stdbool.h>

#include <rte_eal.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_launch.h>
#include <rte_malloc.h>
#include <rte_random.h>
#include <rte_cycles.h>
#include <rte_ethdev.h>
#include <rte_eventdev.h>
#include <rte_event_eth_rx_adapter.h>
#include <rte_service.h>
#include <rte_service_component.h>

#define MAX_NUM_STAGES 8
#define BATCH_SIZE 16
#define MAX_NUM_CORE 64

struct cons_data {
	uint8_t dev_id;
	uint8_t port_id;
	uint8_t release;
} __rte_cache_aligned;

struct worker_data {
	uint8_t dev_id;
	uint8_t port_id;
} __rte_cache_aligned;

typedef int (*worker_loop)(void *);
typedef int (*consumer_loop)(void);
typedef void (*schedule_loop)(unsigned int);
typedef int (*eventdev_setup)(struct cons_data *, struct worker_data *);
typedef void (*rx_adapter_setup)(uint16_t nb_ports);
typedef void (*opt_check)(void);

struct setup_data {
	worker_loop worker;
	consumer_loop consumer;
	schedule_loop scheduler;
	eventdev_setup evdev_setup;
	rx_adapter_setup adptr_setup;
	opt_check check_opt;
};

struct fastpath_data {
	volatile int done;
	uint32_t tx_lock;
	uint32_t evdev_service_id;
	uint32_t rxadptr_service_id;
	bool rx_single;
	bool tx_single;
	bool sched_single;
	unsigned int rx_core[MAX_NUM_CORE];
	unsigned int tx_core[MAX_NUM_CORE];
	unsigned int sched_core[MAX_NUM_CORE];
	unsigned int worker_core[MAX_NUM_CORE];
	struct rte_eth_dev_tx_buffer *tx_buf[RTE_MAX_ETHPORTS];
	struct setup_data cap;
} __rte_cache_aligned;

struct config_data {
	unsigned int active_cores;
	unsigned int num_workers;
	int64_t num_packets;
	uint64_t num_mbuf;
	unsigned int num_fids;
	int queue_type;
	int worker_cycles;
	int enable_queue_priorities;
	int quiet;
	int dump_dev;
	int dump_dev_signal;
	int all_type_queues;
	unsigned int num_stages;
	unsigned int worker_cq_depth;
	unsigned int rx_stride;
	/* Use rx stride value to reduce congestion in entry queue when using
	 * multiple eth ports by forming multiple event queue pipelines.
	 */
	int16_t next_qid[MAX_NUM_STAGES+2];
	int16_t qid[MAX_NUM_STAGES];
	uint8_t rx_adapter_id;
	uint64_t worker_lcore_mask;
	uint64_t rx_lcore_mask;
	uint64_t tx_lcore_mask;
	uint64_t sched_lcore_mask;
};

struct port_link {
	uint8_t queue_id;
	uint8_t priority;
};

struct cons_data cons_data;

struct fastpath_data *fdata;
struct config_data cdata;

static __rte_always_inline void
exchange_mac(struct rte_mbuf *m)
{
	struct ether_hdr *eth;
	struct ether_addr addr;

	/* change mac addresses on packet (to use mbuf data) */
	eth = rte_pktmbuf_mtod(m, struct ether_hdr *);
	ether_addr_copy(&eth->d_addr, &addr);
	ether_addr_copy(&addr, &eth->d_addr);
}

static __rte_always_inline void
work(void)
{
	/* do a number of cycles of work per packet */
	volatile uint64_t start_tsc = rte_rdtsc();
	while (rte_rdtsc() < start_tsc + cdata.worker_cycles)
		rte_pause();
}

static __rte_always_inline void
schedule_devices(unsigned int lcore_id)
{
	if (fdata->rx_core[lcore_id]) {
		rte_service_run_iter_on_app_lcore(fdata->rxadptr_service_id,
				!fdata->rx_single);
	}

	if (fdata->sched_core[lcore_id]) {
		rte_service_run_iter_on_app_lcore(fdata->evdev_service_id,
				!fdata->sched_single);
		if (cdata.dump_dev_signal) {
			rte_event_dev_dump(0, stdout);
			cdata.dump_dev_signal = 0;
		}
	}

	if (fdata->tx_core[lcore_id] && (fdata->tx_single ||
			 rte_atomic32_cmpset(&(fdata->tx_lock), 0, 1))) {
		fdata->cap.consumer();
		rte_atomic32_clear((rte_atomic32_t *)&(fdata->tx_lock));
	}
}

void set_worker_generic_setup_data(struct setup_data *caps, bool burst);
void set_worker_tx_setup_data(struct setup_data *caps, bool burst);
