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

#include <rte_table_stub.h>
#include <rte_table_lpm.h>
#include <rte_table_lpm_ipv6.h>
#include <rte_table_hash.h>
#include <rte_table_array.h>
#include <rte_pipeline.h>

#ifdef RTE_LIBRTE_ACL
#include <rte_table_acl.h>
#endif

#include <rte_port_ring.h>
#include <rte_port_ethdev.h>
#include <rte_port_source_sink.h>

#ifndef TEST_TABLE_H_
#define TEST_TABLE_H_

#define RING_SIZE 4096
#define MAX_BULK 32
#define N 65536
#define TIME_S 5
#define TEST_RING_FULL_EMTPY_ITER   8
#define N_PORTS             2
#define N_PKTS              2
#define N_PKTS_EXT          6
#define RING_RX rings_rx[0]
#define RING_RX_2 rings_rx[1]
#define RING_TX rings_tx[0]
#define RING_TX_2 rings_tx[1]
#define PORT_RX_RING_SIZE   128
#define PORT_TX_RING_SIZE   512
#define RING_RX_SIZE        128
#define RING_TX_SIZE        128
#define POOL_BUFFER_SIZE    RTE_MBUF_DEFAULT_BUF_SIZE
#define POOL_SIZE           (32 * 1024)
#define POOL_CACHE_SIZE     256
#define BURST_SIZE          8
#define WORKER_TYPE         1
#define MAX_DUMMY_PORTS     2
#define MP_NAME             "dummy_port_mempool"
#define MBUF_COUNT          (8000 * MAX_DUMMY_PORTS)
#define MP_CACHE_SZ         256
#define MP_SOCKET           0
#define MP_FLAGS            0

/* Macros */
#define APP_METADATA_OFFSET(offset) (sizeof(struct rte_mbuf) + (offset))

#define RING_ENQUEUE(ring, value) do {					\
	struct rte_mbuf *m;						\
	uint32_t *k32, *signature;					\
	uint8_t *key;							\
									\
	m = rte_pktmbuf_alloc(pool);					\
	if (m == NULL)							\
		return -1;						\
	signature = RTE_MBUF_METADATA_UINT32_PTR(m,			\
			APP_METADATA_OFFSET(0));		\
	key = RTE_MBUF_METADATA_UINT8_PTR(m,			\
			APP_METADATA_OFFSET(32));		\
	k32 = (uint32_t *) key;						\
	k32[0] = (value);						\
	*signature = pipeline_test_hash(key, 0, 0);			\
	rte_ring_enqueue((ring), m);					\
} while (0)

#define RUN_PIPELINE(pipeline) do {					\
	rte_pipeline_run((pipeline));					\
	rte_pipeline_flush((pipeline));					\
} while (0)

#define VERIFY(var, value) do {						\
	if ((var) != -(value))						\
		return var;						\
} while (0)

#define VERIFY_TRAFFIC(ring, sent, expected) do {			\
	unsigned i, n = 0;						\
	void *mbuf = NULL;						\
									\
	for (i = 0; i < (sent); i++) {					\
		if (!rte_ring_dequeue((ring), &mbuf)) {			\
			if (mbuf == NULL)				\
				continue;				\
			n++;						\
			rte_pktmbuf_free((struct rte_mbuf *)mbuf);	\
		}							\
		else							\
			break;						\
	}								\
	printf("Expected %d, got %d\n", expected, n);			\
	if (n != (expected)) {						\
		return -21;						\
	}								\
} while (0)

/* Function definitions */
uint64_t pipeline_test_hash(
	void *key,
	__attribute__((unused)) uint32_t key_size,
	__attribute__((unused)) uint64_t seed);

/* Extern variables */
extern struct rte_pipeline *p;
extern struct rte_ring *rings_rx[N_PORTS];
extern struct rte_ring *rings_tx[N_PORTS];
extern struct rte_mempool *pool;
extern uint32_t port_in_id[N_PORTS];
extern uint32_t port_out_id[N_PORTS];
extern uint32_t port_out_id_type[3];
extern uint32_t table_id[N_PORTS*2];
extern uint64_t override_hit_mask;
extern uint64_t override_miss_mask;
extern uint64_t non_reserved_actions_hit;
extern uint64_t non_reserved_actions_miss;
extern uint8_t connect_miss_action_to_port_out;
extern uint8_t connect_miss_action_to_table;
extern uint32_t table_entry_default_action;
extern uint32_t table_entry_hit_action;
extern uint32_t table_entry_miss_action;
extern rte_pipeline_port_in_action_handler port_in_action;
extern rte_pipeline_port_out_action_handler port_out_action;
extern rte_pipeline_table_action_handler_hit action_handler_hit;
extern rte_pipeline_table_action_handler_miss action_handler_miss;

/* Global data types */
struct manage_ops {
	uint32_t op_id;
	void *op_data;
	int expected_result;
};

/* Internal pipeline structures */
struct rte_port_in {
	struct rte_port_in_ops ops;
	uint32_t burst_size;
	uint32_t table_id;
	void *h_port;
};

struct rte_port_out {
	struct rte_port_out_ops ops;
	void *h_port;
};

struct rte_table {
	struct rte_table_ops ops;
	rte_pipeline_table_action_handler_hit f_action;
	uint32_t table_next_id;
	uint32_t table_next_id_valid;
	uint8_t actions_lookup_miss[RTE_CACHE_LINE_SIZE];
	uint32_t action_data_size;
	void *h_table;
};

#define RTE_PIPELINE_MAX_NAME_SZ                           124

struct rte_pipeline {
	char name[RTE_PIPELINE_MAX_NAME_SZ];
	uint32_t socket_id;
	struct rte_port_in ports_in[16];
	struct rte_port_out ports_out[16];
	struct rte_table tables[64];
	uint32_t num_ports_in;
	uint32_t num_ports_out;
	uint32_t num_tables;
	struct rte_mbuf *pkts[RTE_PORT_IN_BURST_SIZE_MAX];
	struct rte_table_entry *actions[RTE_PORT_IN_BURST_SIZE_MAX];
	uint64_t mask_action[64];
	uint32_t mask_actions;
};
#endif
