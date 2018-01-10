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
} __rte_cache_aligned;

struct config_data {
	unsigned int active_cores;
	unsigned int num_workers;
	int64_t num_packets;
	unsigned int num_fids;
	int queue_type;
	int worker_cycles;
	int enable_queue_priorities;
	int quiet;
	int dump_dev;
	int dump_dev_signal;
	unsigned int num_stages;
	unsigned int worker_cq_depth;
	int16_t next_qid[MAX_NUM_STAGES+2];
	int16_t qid[MAX_NUM_STAGES];
	uint8_t rx_adapter_id;
};

struct cons_data cons_data;

struct fastpath_data *fdata;
struct config_data cdata;
