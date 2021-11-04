/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _MAIN_H_
#define _MAIN_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_sched.h>

#define RTE_LOGTYPE_APP RTE_LOGTYPE_USER1

/*
 * Configurable number of RX/TX ring descriptors
 */
#define APP_INTERACTIVE_DEFAULT 0

#define APP_RX_DESC_DEFAULT 1024
#define APP_TX_DESC_DEFAULT 1024

#define APP_RING_SIZE (8*1024)
#define NB_MBUF   (2*1024*1024)

#define MAX_PKT_RX_BURST 64
#define PKT_ENQUEUE 64
#define PKT_DEQUEUE 32
#define MAX_PKT_TX_BURST 64

#define RX_PTHRESH 8 /**< Default values of RX prefetch threshold reg. */
#define RX_HTHRESH 8 /**< Default values of RX host threshold reg. */
#define RX_WTHRESH 4 /**< Default values of RX write-back threshold reg. */

#define TX_PTHRESH 36 /**< Default values of TX prefetch threshold reg. */
#define TX_HTHRESH 0  /**< Default values of TX host threshold reg. */
#define TX_WTHRESH 0  /**< Default values of TX write-back threshold reg. */

#define BURST_TX_DRAIN_US 100

#ifndef APP_MAX_LCORE
#if (RTE_MAX_LCORE > 64)
#define APP_MAX_LCORE 64
#else
#define APP_MAX_LCORE RTE_MAX_LCORE
#endif
#endif

#define MAX_DATA_STREAMS (APP_MAX_LCORE/2)
#define MAX_SCHED_SUBPORTS		8
#define MAX_SCHED_PIPES		4096
#define MAX_SCHED_PIPE_PROFILES		256
#define MAX_SCHED_SUBPORT_PROFILES	8

#ifndef APP_COLLECT_STAT
#define APP_COLLECT_STAT		1
#endif

#if APP_COLLECT_STAT
#define APP_STATS_ADD(stat,val) (stat) += (val)
#else
#define APP_STATS_ADD(stat,val) do {(void) (val);} while (0)
#endif

#define APP_QAVG_NTIMES 10
#define APP_QAVG_PERIOD 100

struct thread_stat
{
	uint64_t nb_rx;
	uint64_t nb_drop;
};


struct thread_conf
{
	uint32_t counter;
	uint32_t n_mbufs;
	struct rte_mbuf **m_table;

	uint16_t rx_port;
	uint16_t tx_port;
	uint16_t rx_queue;
	uint16_t tx_queue;
	struct rte_ring *rx_ring;
	struct rte_ring *tx_ring;
	struct rte_sched_port *sched_port;

#if APP_COLLECT_STAT
	struct thread_stat stat;
#endif
} __rte_cache_aligned;


struct flow_conf
{
	uint32_t rx_core;
	uint32_t wt_core;
	uint32_t tx_core;
	uint16_t rx_port;
	uint16_t tx_port;
	uint16_t rx_queue;
	uint16_t tx_queue;
	struct rte_ring *rx_ring;
	struct rte_ring *tx_ring;
	struct rte_sched_port *sched_port;
	struct rte_mempool *mbuf_pool;

	struct thread_conf rx_thread;
	struct thread_conf wt_thread;
	struct thread_conf tx_thread;
};


struct ring_conf
{
	uint32_t rx_size;
	uint32_t ring_size;
	uint32_t tx_size;
};

struct burst_conf
{
	uint16_t rx_burst;
	uint16_t ring_burst;
	uint16_t qos_dequeue;
	uint16_t tx_burst;
};

struct ring_thresh
{
	uint8_t pthresh; /**< Ring prefetch threshold. */
	uint8_t hthresh; /**< Ring host threshold. */
	uint8_t wthresh; /**< Ring writeback threshold. */
};

extern uint8_t interactive;
extern uint32_t qavg_period;
extern uint32_t qavg_ntimes;
extern uint32_t nb_pfc;
extern const char *cfg_profile;
extern int mp_size;
extern struct flow_conf qos_conf[];
extern int app_pipe_to_profile[MAX_SCHED_SUBPORTS][MAX_SCHED_PIPES];

extern struct ring_conf ring_conf;
extern struct burst_conf burst_conf;
extern struct ring_thresh rx_thresh;
extern struct ring_thresh tx_thresh;

extern uint32_t active_queues[RTE_SCHED_QUEUES_PER_PIPE];
extern uint32_t n_active_queues;

extern struct rte_sched_port_params port_params;
#ifdef RTE_SCHED_CMAN
extern struct rte_sched_cman_params cman_params;
#endif
extern struct rte_sched_subport_params subport_params[MAX_SCHED_SUBPORTS];

int app_parse_args(int argc, char **argv);
int app_init(void);

void prompt(void);
void app_rx_thread(struct thread_conf **qconf);
void app_tx_thread(struct thread_conf **qconf);
void app_worker_thread(struct thread_conf **qconf);
void app_mixed_thread(struct thread_conf **qconf);

void app_stat(void);
int subport_stat(uint16_t port_id, uint32_t subport_id);
int pipe_stat(uint16_t port_id, uint32_t subport_id, uint32_t pipe_id);
int qavg_q(uint16_t port_id, uint32_t subport_id, uint32_t pipe_id,
	   uint8_t tc, uint8_t q);
int qavg_tcpipe(uint16_t port_id, uint32_t subport_id, uint32_t pipe_id,
		uint8_t tc);
int qavg_pipe(uint16_t port_id, uint32_t subport_id, uint32_t pipe_id);
int qavg_tcsubport(uint16_t port_id, uint32_t subport_id, uint8_t tc);
int qavg_subport(uint16_t port_id, uint32_t subport_id);

#ifdef __cplusplus
}
#endif

#endif /* _MAIN_H_ */
