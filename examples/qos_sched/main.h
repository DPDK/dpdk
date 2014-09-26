/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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

#define APP_RX_DESC_DEFAULT 128
#define APP_TX_DESC_DEFAULT 256

#define MBUF_SIZE (1528 + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)
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

#define MAX_DATA_STREAMS (RTE_MAX_LCORE/2)
#define MAX_SCHED_SUBPORTS		8
#define MAX_SCHED_PIPES  		4096

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

	uint8_t rx_port;
	uint8_t tx_port;
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
	uint8_t rx_port;
	uint8_t tx_port;
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

extern struct rte_sched_port_params port_params;

int app_parse_args(int argc, char **argv);
int app_init(void);

void prompt(void);
void app_rx_thread(struct thread_conf **qconf);
void app_tx_thread(struct thread_conf **qconf);
void app_worker_thread(struct thread_conf **qconf);
void app_mixed_thread(struct thread_conf **qconf);

void app_stat(void);
int subport_stat(uint8_t port_id, uint32_t subport_id);
int pipe_stat(uint8_t port_id, uint32_t subport_id, uint32_t pipe_id);
int qavg_q(uint8_t port_id, uint32_t subport_id, uint32_t pipe_id, uint8_t tc, uint8_t q);
int qavg_tcpipe(uint8_t port_id, uint32_t subport_id, uint32_t pipe_id, uint8_t tc);
int qavg_pipe(uint8_t port_id, uint32_t subport_id, uint32_t pipe_id);
int qavg_tcsubport(uint8_t port_id, uint32_t subport_id, uint8_t tc);
int qavg_subport(uint8_t port_id, uint32_t subport_id);

#ifdef __cplusplus
}
#endif

#endif /* _MAIN_H_ */
