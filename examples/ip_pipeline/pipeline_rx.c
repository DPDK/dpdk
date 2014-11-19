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

#include <stdio.h>
#include <stdlib.h>

#include <rte_common.h>
#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_jhash.h>

#include <rte_port_ethdev.h>
#include <rte_port_ring.h>
#include <rte_table_stub.h>
#include <rte_pipeline.h>


#include "main.h"

struct app_core_rx_message_handle_params {
	struct rte_ring *ring_req;
	struct rte_ring *ring_resp;

	struct rte_pipeline *p;
	uint32_t *port_in_id;
};

static void
app_message_handle(struct app_core_rx_message_handle_params *params);

static int
app_pipeline_rx_port_in_action_handler(struct rte_mbuf **pkts, uint32_t n,
	uint64_t *pkts_mask, void *arg);

void
app_main_loop_pipeline_rx(void) {
	struct rte_pipeline *p;
	uint32_t port_in_id[APP_MAX_PORTS];
	uint32_t port_out_id[APP_MAX_PORTS];
	uint32_t table_id[APP_MAX_PORTS];
	uint32_t i;

	uint32_t core_id = rte_lcore_id();
	struct app_core_params *core_params = app_get_core_params(core_id);
	struct app_core_rx_message_handle_params mh_params;

	if ((core_params == NULL) || (core_params->core_type != APP_CORE_RX))
		rte_panic("Core %u misconfiguration\n", core_id);

	RTE_LOG(INFO, USER1, "Core %u is doing RX\n", core_id);

	/* Pipeline configuration */
	struct rte_pipeline_params pipeline_params = {
		.name = "pipeline",
		.socket_id = rte_socket_id(),
	};

	p = rte_pipeline_create(&pipeline_params);
	if (p == NULL)
		rte_panic("%s: Unable to configure the pipeline\n", __func__);

	/* Input port configuration */
	for (i = 0; i < app.n_ports; i++) {
		struct rte_port_ethdev_reader_params port_ethdev_params = {
			.port_id = app.ports[i],
			.queue_id = 0,
		};

		struct rte_pipeline_port_in_params port_params = {
			.ops = &rte_port_ethdev_reader_ops,
			.arg_create = (void *) &port_ethdev_params,
			.f_action = app_pipeline_rx_port_in_action_handler,
			.arg_ah = NULL,
			.burst_size = app.bsz_hwq_rd,
		};

		if (rte_pipeline_port_in_create(p, &port_params,
			&port_in_id[i]))
			rte_panic("%s: Unable to configure input port for "
				"port %d\n", __func__, app.ports[i]);
	}

	/* Output port configuration */
	for (i = 0; i < app.n_ports; i++) {
		struct rte_port_ring_writer_params port_ring_params = {
			.ring = app.rings[core_params->swq_out[i]],
			.tx_burst_sz = app.bsz_swq_wr,
		};

		struct rte_pipeline_port_out_params port_params = {
			.ops = &rte_port_ring_writer_ops,
			.arg_create = (void *) &port_ring_params,
			.f_action = NULL,
			.f_action_bulk = NULL,
			.arg_ah = NULL,
		};

		if (rte_pipeline_port_out_create(p, &port_params,
			&port_out_id[i]))
			rte_panic("%s: Unable to configure output port for "
				"ring RX %i\n", __func__, i);
	}

	/* Table configuration */
	for (i = 0; i < app.n_ports; i++) {
		struct rte_pipeline_table_params table_params = {
			.ops = &rte_table_stub_ops,
			.arg_create = NULL,
			.f_action_hit = NULL,
			.f_action_miss = NULL,
			.arg_ah = NULL,
			.action_data_size = 0,
		};

		if (rte_pipeline_table_create(p, &table_params, &table_id[i]))
			rte_panic("%s: Unable to configure table %u\n",
				__func__, table_id[i]);
	}

	/* Interconnecting ports and tables */
	for (i = 0; i < app.n_ports; i++)
		if (rte_pipeline_port_in_connect_to_table(p, port_in_id[i],
			table_id[i]))
			rte_panic("%s: Unable to connect input port %u to "
				"table %u\n", __func__, port_in_id[i],
				table_id[i]);

	/* Add entries to tables */
	for (i = 0; i < app.n_ports; i++) {
		struct rte_pipeline_table_entry default_entry = {
			.action = RTE_PIPELINE_ACTION_PORT,
			{.port_id = port_out_id[i]},
		};

		struct rte_pipeline_table_entry *default_entry_ptr;

		if (rte_pipeline_table_default_entry_add(p, table_id[i],
			&default_entry, &default_entry_ptr))
			rte_panic("%s: Unable to add default entry to "
				"table %u\n", __func__, table_id[i]);
	}

	/* Enable input ports */
	for (i = 0; i < app.n_ports; i++)
		if (rte_pipeline_port_in_enable(p, port_in_id[i]))
			rte_panic("Unable to enable input port %u\n",
				port_in_id[i]);

	/* Check pipeline consistency */
	if (rte_pipeline_check(p) < 0)
		rte_panic("%s: Pipeline consistency check failed\n", __func__);

	/* Message handling */
	mh_params.ring_req =
		app_get_ring_req(app_get_first_core_id(APP_CORE_RX));
	mh_params.ring_resp =
		app_get_ring_resp(app_get_first_core_id(APP_CORE_RX));
	mh_params.p = p;
	mh_params.port_in_id = port_in_id;

	/* Run-time */
	for (i = 0; ; i++) {
		rte_pipeline_run(p);

		if ((i & APP_FLUSH) == 0) {
			rte_pipeline_flush(p);
			app_message_handle(&mh_params);
		}
	}
}

uint64_t test_hash(
	void *key,
	__attribute__((unused)) uint32_t key_size,
	__attribute__((unused)) uint64_t seed)
{
	struct app_flow_key *flow_key = (struct app_flow_key *) key;
	uint32_t ip_dst = rte_be_to_cpu_32(flow_key->ip_dst);
	uint64_t signature = (ip_dst & 0x00FFFFFFLLU) >> 2;

	return signature;
}

uint32_t
rte_jhash2_16(uint32_t *k, uint32_t initval)
{
	uint32_t a, b, c;

	a = b = RTE_JHASH_GOLDEN_RATIO;
	c = initval;

	a += k[0];
	b += k[1];
	c += k[2];
	__rte_jhash_mix(a, b, c);

	c += 16; /* length in bytes */
	a += k[3]; /* Remaining word */

	__rte_jhash_mix(a, b, c);

	return c;
}

static inline void
app_pkt_metadata_fill(struct rte_mbuf *m)
{
	uint8_t *m_data = rte_pktmbuf_mtod(m, uint8_t *);
	struct app_pkt_metadata *c =
		(struct app_pkt_metadata *) RTE_MBUF_METADATA_UINT8_PTR(m, 0);
	struct ipv4_hdr *ip_hdr =
		(struct ipv4_hdr *) &m_data[sizeof(struct ether_hdr)];
	uint64_t *ipv4_hdr_slab = (uint64_t *) ip_hdr;

	/* TTL and Header Checksum are set to 0 */
	c->flow_key.slab0 = ipv4_hdr_slab[1] & 0xFFFFFFFF0000FF00LLU;
	c->flow_key.slab1 = ipv4_hdr_slab[2];
	c->signature = test_hash((void *) &c->flow_key, 0, 0);

	/* Pop Ethernet header */
	if (app.ether_hdr_pop_push) {
		rte_pktmbuf_adj(m, (uint16_t)sizeof(struct ether_hdr));
		m->l2_len = 0;
		m->l3_len = sizeof(struct ipv4_hdr);
	}
}

int
app_pipeline_rx_port_in_action_handler(
	struct rte_mbuf **pkts,
	uint32_t n,
	uint64_t *pkts_mask,
	__rte_unused void *arg)
{
	uint32_t i;

	for (i = 0; i < n; i++) {
		struct rte_mbuf *m = pkts[i];

		app_pkt_metadata_fill(m);
	}

	*pkts_mask = (~0LLU) >> (64 - n);

	return 0;
}

void
app_main_loop_rx(void) {
	struct app_mbuf_array *ma;
	uint32_t i, j;
	int ret;

	uint32_t core_id = rte_lcore_id();
	struct app_core_params *core_params = app_get_core_params(core_id);

	if ((core_params == NULL) || (core_params->core_type != APP_CORE_RX))
		rte_panic("Core %u misconfiguration\n", core_id);

	RTE_LOG(INFO, USER1, "Core %u is doing RX (no pipeline)\n", core_id);

	ma = rte_malloc_socket(NULL, sizeof(struct app_mbuf_array),
		RTE_CACHE_LINE_SIZE, rte_socket_id());
	if (ma == NULL)
		rte_panic("%s: cannot allocate buffer space\n", __func__);

	for (i = 0; ; i = ((i + 1) & (app.n_ports - 1))) {
		uint32_t n_mbufs;

		n_mbufs = rte_eth_rx_burst(
			app.ports[i],
			0,
			ma->array,
			app.bsz_hwq_rd);

		if (n_mbufs == 0)
			continue;

		for (j = 0; j < n_mbufs; j++) {
			struct rte_mbuf *m = ma->array[j];

			app_pkt_metadata_fill(m);
		}

		do {
			ret = rte_ring_sp_enqueue_bulk(
				app.rings[core_params->swq_out[i]],
				(void **) ma->array,
				n_mbufs);
		} while (ret < 0);
	}
}

void
app_message_handle(struct app_core_rx_message_handle_params *params)
{
	struct rte_ring *ring_req = params->ring_req;
	struct rte_ring *ring_resp;
	void *msg;
	struct app_msg_req *req;
	struct app_msg_resp *resp;
	struct rte_pipeline *p;
	uint32_t *port_in_id;
	int result;

	/* Read request message */
	result = rte_ring_sc_dequeue(ring_req, &msg);
	if (result != 0)
		return;

	ring_resp = params->ring_resp;
	p = params->p;
	port_in_id = params->port_in_id;

	/* Handle request */
	req = (struct app_msg_req *)rte_ctrlmbuf_data((struct rte_mbuf *)msg);
	switch (req->type) {
	case APP_MSG_REQ_PING:
	{
		result = 0;
		break;
	}

	case APP_MSG_REQ_RX_PORT_ENABLE:
	{
		result = rte_pipeline_port_in_enable(p,
			port_in_id[req->rx_up.port]);
		break;
	}

	case APP_MSG_REQ_RX_PORT_DISABLE:
	{
		result = rte_pipeline_port_in_disable(p,
			port_in_id[req->rx_down.port]);
		break;
	}

	default:
		rte_panic("RX Unrecognized message type (%u)\n", req->type);
	}

	/* Fill in response message */
	resp = (struct app_msg_resp *)rte_ctrlmbuf_data((struct rte_mbuf *)msg);
	resp->result = result;

	/* Send response */
	do {
		result = rte_ring_sp_enqueue(ring_resp, msg);
	} while (result == -ENOBUFS);
}
