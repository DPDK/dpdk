/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef __INCLUDE_RTE_ETH_SOFTNIC_INTERNALS_H__
#define __INCLUDE_RTE_ETH_SOFTNIC_INTERNALS_H__

#include <stddef.h>
#include <stdint.h>
#include <sys/queue.h>

#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_ring.h>
#include <rte_ethdev.h>
#include <rte_swx_pipeline.h>
#include <rte_swx_ctl.h>

#include <rte_ethdev_core.h>
#include <ethdev_driver.h>

#include "rte_eth_softnic.h"
#include "conn.h"

#define NAME_SIZE                                            64
#define SOFTNIC_PATH_MAX                                     4096

/**
 * PMD Parameters
 */

struct pmd_params {
	char name[NAME_SIZE];
	char firmware[SOFTNIC_PATH_MAX];
	uint16_t conn_port;
	uint32_t cpu_id;
	int sc; /**< Service cores. */
};

/**
 * MEMPOOL
 */
struct softnic_mempool_params {
	uint32_t buffer_size;
	uint32_t pool_size;
	uint32_t cache_size;
};

struct softnic_mempool {
	TAILQ_ENTRY(softnic_mempool) node;
	char name[NAME_SIZE];
	struct rte_mempool *m;
	uint32_t buffer_size;
};

TAILQ_HEAD(softnic_mempool_list, softnic_mempool);

/**
 * SWQ
 */
struct softnic_swq_params {
	uint32_t size;
};

struct softnic_swq {
	TAILQ_ENTRY(softnic_swq) node;
	char name[NAME_SIZE];
	struct rte_ring *r;
};

TAILQ_HEAD(softnic_swq_list, softnic_swq);

/**
 * Pipeline
 */
struct pipeline {
	TAILQ_ENTRY(pipeline) node;
	char name[NAME_SIZE];

	struct rte_swx_pipeline *p;
	struct rte_swx_ctl_pipeline *ctl;

	int enabled;
	uint32_t thread_id;
};

TAILQ_HEAD(pipeline_list, pipeline);

/**
 * Thread
 */
#ifndef THREAD_PIPELINES_MAX
#define THREAD_PIPELINES_MAX                               256
#endif

#ifndef THREAD_MSGQ_SIZE
#define THREAD_MSGQ_SIZE                                   64
#endif

#ifndef THREAD_TIMER_PERIOD_MS
#define THREAD_TIMER_PERIOD_MS                             100
#endif

/* Pipeline instruction quanta: Needs to be big enough to do some meaningful
 * work, but not too big to avoid starving any other pipelines mapped to the
 * same thread. For a pipeline that executes 10 instructions per packet, a
 * quanta of 1000 instructions equates to processing 100 packets.
 */
#ifndef PIPELINE_INSTR_QUANTA
#define PIPELINE_INSTR_QUANTA                              1000
#endif

/**
 * Main thread: data plane thread context
 */
struct softnic_thread {
	struct rte_ring *msgq_req;
	struct rte_ring *msgq_rsp;

	uint32_t service_id;
};

/**
 * Data plane threads: context
 */
struct softnic_thread_data {
	struct rte_swx_pipeline *p[THREAD_PIPELINES_MAX];
	uint32_t n_pipelines;

	struct rte_ring *msgq_req;
	struct rte_ring *msgq_rsp;
	uint64_t timer_period; /* Measured in CPU cycles. */
	uint64_t time_next;
	uint64_t iter;
} __rte_cache_aligned;

/**
 * PMD Internals
 */
struct pmd_internals {
	/** Params */
	struct pmd_params params;

	struct softnic_conn *conn;
	struct softnic_mempool_list mempool_list;
	struct softnic_swq_list swq_list;
	struct pipeline_list pipeline_list;
	struct softnic_thread thread[RTE_MAX_LCORE];
	struct softnic_thread_data thread_data[RTE_MAX_LCORE];
};

static inline struct rte_eth_dev *
ETHDEV(struct pmd_internals *softnic)
{
	uint16_t port_id;
	int status;

	if (softnic == NULL)
		return NULL;

	status = rte_eth_dev_get_port_by_name(softnic->params.name, &port_id);
	if (status)
		return NULL;

	return &rte_eth_devices[port_id];
}

/**
 * MEMPOOL
 */
int
softnic_mempool_init(struct pmd_internals *p);

void
softnic_mempool_free(struct pmd_internals *p);

struct softnic_mempool *
softnic_mempool_find(struct pmd_internals *p,
	const char *name);

struct softnic_mempool *
softnic_mempool_create(struct pmd_internals *p,
	const char *name,
	struct softnic_mempool_params *params);

/**
 * SWQ
 */
int
softnic_swq_init(struct pmd_internals *p);

void
softnic_swq_free(struct pmd_internals *p);

void
softnic_softnic_swq_free_keep_rxq_txq(struct pmd_internals *p);

struct softnic_swq *
softnic_swq_find(struct pmd_internals *p,
	const char *name);

struct softnic_swq *
softnic_swq_create(struct pmd_internals *p,
	const char *name,
	struct softnic_swq_params *params);

/**
 * Pipeline
 */
int
softnic_pipeline_init(struct pmd_internals *p);

void
softnic_pipeline_free(struct pmd_internals *p);

void
softnic_pipeline_disable_all(struct pmd_internals *p);

uint32_t
softnic_pipeline_thread_count(struct pmd_internals *p, uint32_t thread_id);

struct pipeline *
softnic_pipeline_find(struct pmd_internals *p, const char *name);

struct pipeline *
softnic_pipeline_create(struct pmd_internals *p,
	const char *name,
	const char *lib_file_name,
	const char *iospec_file_name,
	int numa_node);

/**
 * Thread
 */
int
softnic_thread_init(struct pmd_internals *p);

void
softnic_thread_free(struct pmd_internals *p);

int
softnic_thread_pipeline_enable(struct pmd_internals *p,
	uint32_t thread_id,
	struct pipeline *pipeline);

int
softnic_thread_pipeline_disable(struct pmd_internals *p,
	uint32_t thread_id,
	struct pipeline *pipeline);

void
softnic_thread_pipeline_disable_all(struct pmd_internals *p);

/**
 * CLI
 */
void
softnic_cli_process(char *in,
	char *out,
	size_t out_size,
	void *arg);

int
softnic_cli_script_process(struct pmd_internals *softnic,
	const char *file_name,
	size_t msg_in_len_max,
	size_t msg_out_len_max);

#endif /* __INCLUDE_RTE_ETH_SOFTNIC_INTERNALS_H__ */
