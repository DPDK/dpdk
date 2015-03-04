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
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <string.h>
#include <sys/queue.h>
#include <stdarg.h>
#include <errno.h>
#include <getopt.h>

#include <rte_common.h>
#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_string_fns.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_lpm.h>
#include <rte_lpm6.h>

#include "main.h"

#define NA                             APP_SWQ_INVALID

struct app_params app = {
	/* CPU cores */
	.cores = {
	{0, APP_CORE_MASTER, {15, 16, 17, NA, NA, NA, NA, NA},
		{12, 13, 14, NA, NA, NA, NA, NA} },
	{0, APP_CORE_RX,     {NA, NA, NA, NA, NA, NA, NA, 12},
		{ 0,  1,  2,  3, NA, NA, NA, 15} },
	{0, APP_CORE_FC,     { 0,  1,  2,  3, NA, NA, NA, 13},
		{ 4,  5,  6,  7, NA, NA, NA, 16} },
	{0, APP_CORE_RT,     { 4,  5,  6,  7, NA, NA, NA, 14},
		{ 8,  9, 10, 11, NA, NA, NA, 17} },
	{0, APP_CORE_TX,     { 8,  9, 10, 11, NA, NA, NA, NA},
		{NA, NA, NA, NA, NA, NA, NA, NA} },
	},

	/* Ports*/
	.n_ports = APP_MAX_PORTS,
	.rsz_hwq_rx = 128,
	.rsz_hwq_tx = 512,
	.bsz_hwq_rd = 64,
	.bsz_hwq_wr = 64,

	.port_conf = {
		.rxmode = {
			.split_hdr_size = 0,
			.header_split   = 0, /* Header Split disabled */
			.hw_ip_checksum = 1, /* IP checksum offload enabled */
			.hw_vlan_filter = 0, /* VLAN filtering disabled */
			.jumbo_frame    = 1, /* Jumbo Frame Support enabled */
			.max_rx_pkt_len = 9000, /* Jumbo Frame MAC pkt length */
			.hw_strip_crc   = 0, /* CRC stripped by hardware */
		},
		.rx_adv_conf = {
			.rss_conf = {
				.rss_key = NULL,
				.rss_hf = ETH_RSS_IP,
			},
		},
		.txmode = {
			.mq_mode = ETH_MQ_TX_NONE,
		},
	},

	.rx_conf = {
		.rx_thresh = {
			.pthresh = 8,
			.hthresh = 8,
			.wthresh = 4,
		},
		.rx_free_thresh = 64,
		.rx_drop_en = 0,
	},

	.tx_conf = {
		.tx_thresh = {
			.pthresh = 36,
			.hthresh = 0,
			.wthresh = 0,
		},
		.tx_free_thresh = 0,
		.tx_rs_thresh = 0,
	},

	/* SWQs */
	.rsz_swq = 128,
	.bsz_swq_rd = 64,
	.bsz_swq_wr = 64,

	/* Buffer pool */
	.pool_buffer_size = 2048 + sizeof(struct rte_mbuf) +
		RTE_PKTMBUF_HEADROOM,
	.pool_size = 32 * 1024,
	.pool_cache_size = 256,

	/* Message buffer pool */
	.msg_pool_buffer_size = 256,
	.msg_pool_size = 1024,
	.msg_pool_cache_size = 64,

	/* Rule tables */
	.max_arp_rules = 1 << 10,
	.max_firewall_rules = 1 << 5,
	.max_routing_rules = 1 << 24,
	.max_flow_rules = 1 << 24,

	/* Application processing */
	.ether_hdr_pop_push = 0,
};

struct app_core_params *
app_get_core_params(uint32_t core_id)
{
	uint32_t i;

	for (i = 0; i < RTE_MAX_LCORE; i++) {
		struct app_core_params *p = &app.cores[i];

		if (p->core_id != core_id)
			continue;

		return p;
	}

	return NULL;
}

static uint32_t
app_get_n_swq_in(void)
{
	uint32_t max_swq_id = 0, i, j;

	for (i = 0; i < RTE_MAX_LCORE; i++) {
		struct app_core_params *p = &app.cores[i];

		if (p->core_type == APP_CORE_NONE)
			continue;

		for (j = 0; j < APP_MAX_SWQ_PER_CORE; j++) {
			uint32_t swq_id = p->swq_in[j];

			if ((swq_id != APP_SWQ_INVALID) &&
				(swq_id > max_swq_id))
				max_swq_id = swq_id;
		}
	}

	return (1 + max_swq_id);
}

static uint32_t
app_get_n_swq_out(void)
{
	uint32_t max_swq_id = 0, i, j;

	for (i = 0; i < RTE_MAX_LCORE; i++) {
		struct app_core_params *p = &app.cores[i];

		if (p->core_type == APP_CORE_NONE)
			continue;

		for (j = 0; j < APP_MAX_SWQ_PER_CORE; j++) {
			uint32_t swq_id = p->swq_out[j];

			if ((swq_id != APP_SWQ_INVALID) &&
				(swq_id > max_swq_id))
				max_swq_id = swq_id;
		}
	}

	return (1 + max_swq_id);
}

static uint32_t
app_get_swq_in_count(uint32_t swq_id)
{
	uint32_t n, i;

	for (n = 0, i = 0; i < RTE_MAX_LCORE; i++) {
		struct app_core_params *p = &app.cores[i];
		uint32_t j;

		if (p->core_type == APP_CORE_NONE)
			continue;

		for (j = 0; j < APP_MAX_SWQ_PER_CORE; j++)
			if (p->swq_in[j] == swq_id)
				n++;
	}

	return n;
}

static uint32_t
app_get_swq_out_count(uint32_t swq_id)
{
	uint32_t n, i;

	for (n = 0, i = 0; i < RTE_MAX_LCORE; i++) {
		struct app_core_params *p = &app.cores[i];
		uint32_t j;

		if (p->core_type == APP_CORE_NONE)
			continue;

		for (j = 0; j < APP_MAX_SWQ_PER_CORE; j++)
			if (p->swq_out[j] == swq_id)
				n++;
	}

	return n;
}

void
app_check_core_params(void)
{
	uint32_t n_swq_in = app_get_n_swq_in();
	uint32_t n_swq_out = app_get_n_swq_out();
	uint32_t i;

	/* Check that range of SW queues is contiguous and each SW queue has
	   exactly one reader and one writer */
	if (n_swq_in != n_swq_out)
		rte_panic("Number of input SW queues is not equal to the "
			"number of output SW queues\n");

	for (i = 0; i < n_swq_in; i++) {
		uint32_t n = app_get_swq_in_count(i);

		if (n == 0)
			rte_panic("SW queue %u has no reader\n", i);

		if (n > 1)
			rte_panic("SW queue %u has more than one reader\n", i);
	}

	for (i = 0; i < n_swq_out; i++) {
		uint32_t n = app_get_swq_out_count(i);

		if (n == 0)
			rte_panic("SW queue %u has no writer\n", i);

		if (n > 1)
			rte_panic("SW queue %u has more than one writer\n", i);
	}

	/* Check the request and response queues are valid */
	for (i = 0; i < RTE_MAX_LCORE; i++) {
		struct app_core_params *p = &app.cores[i];
		uint32_t ring_id_req, ring_id_resp;

		if ((p->core_type != APP_CORE_FC) &&
		    (p->core_type != APP_CORE_FW) &&
			(p->core_type != APP_CORE_RT)) {
			continue;
		}

		ring_id_req = p->swq_in[APP_SWQ_IN_REQ];
		if (ring_id_req == APP_SWQ_INVALID)
			rte_panic("Core %u of type %u has invalid request "
				"queue ID\n", p->core_id, p->core_type);

		ring_id_resp = p->swq_out[APP_SWQ_OUT_RESP];
		if (ring_id_resp == APP_SWQ_INVALID)
			rte_panic("Core %u of type %u has invalid response "
				"queue ID\n", p->core_id, p->core_type);
	}

	return;
}

uint32_t
app_get_first_core_id(enum app_core_type core_type)
{
	uint32_t i;

	for (i = 0; i < RTE_MAX_LCORE; i++) {
		struct app_core_params *p = &app.cores[i];

		if (p->core_type == core_type)
			return p->core_id;
	}

	return RTE_MAX_LCORE;
}

struct rte_ring *
app_get_ring_req(uint32_t core_id)
{
	struct app_core_params *p = app_get_core_params(core_id);
	uint32_t ring_req_id = p->swq_in[APP_SWQ_IN_REQ];

	return app.rings[ring_req_id];
}

struct rte_ring *
app_get_ring_resp(uint32_t core_id)
{
	struct app_core_params *p = app_get_core_params(core_id);
	uint32_t ring_resp_id = p->swq_out[APP_SWQ_OUT_RESP];

	return app.rings[ring_resp_id];
}

static void
app_init_mbuf_pools(void)
{
	/* Init the buffer pool */
	RTE_LOG(INFO, USER1, "Creating the mbuf pool ...\n");
	app.pool = rte_mempool_create(
		"mempool",
		app.pool_size,
		app.pool_buffer_size,
		app.pool_cache_size,
		sizeof(struct rte_pktmbuf_pool_private),
		rte_pktmbuf_pool_init, NULL,
		rte_pktmbuf_init, NULL,
		rte_socket_id(),
		0);
	if (app.pool == NULL)
		rte_panic("Cannot create mbuf pool\n");

	/* Init the indirect buffer pool */
	RTE_LOG(INFO, USER1, "Creating the indirect mbuf pool ...\n");
	app.indirect_pool = rte_mempool_create(
		"indirect mempool",
		app.pool_size,
		sizeof(struct rte_mbuf) + sizeof(struct app_pkt_metadata),
		app.pool_cache_size,
		0,
		NULL, NULL,
		rte_pktmbuf_init, NULL,
		rte_socket_id(),
		0);
	if (app.indirect_pool == NULL)
		rte_panic("Cannot create mbuf pool\n");

	/* Init the message buffer pool */
	RTE_LOG(INFO, USER1, "Creating the message pool ...\n");
	app.msg_pool = rte_mempool_create(
		"mempool msg",
		app.msg_pool_size,
		app.msg_pool_buffer_size,
		app.msg_pool_cache_size,
		0,
		NULL, NULL,
		rte_ctrlmbuf_init, NULL,
		rte_socket_id(),
		0);
	if (app.msg_pool == NULL)
		rte_panic("Cannot create message pool\n");
}

static void
app_init_rings(void)
{
	uint32_t n_swq, i;

	n_swq = app_get_n_swq_in();
	RTE_LOG(INFO, USER1, "Initializing %u SW rings ...\n", n_swq);

	app.rings = rte_malloc_socket(NULL, n_swq * sizeof(struct rte_ring *),
		RTE_CACHE_LINE_SIZE, rte_socket_id());
	if (app.rings == NULL)
		rte_panic("Cannot allocate memory to store ring pointers\n");

	for (i = 0; i < n_swq; i++) {
		struct rte_ring *ring;
		char name[32];

		snprintf(name, sizeof(name), "app_ring_%u", i);

		ring = rte_ring_create(
			name,
			app.rsz_swq,
			rte_socket_id(),
			RING_F_SP_ENQ | RING_F_SC_DEQ);

		if (ring == NULL)
			rte_panic("Cannot create ring %u\n", i);

		app.rings[i] = ring;
	}
}

static void
app_ports_check_link(void)
{
	uint32_t all_ports_up, i;

	all_ports_up = 1;

	for (i = 0; i < app.n_ports; i++) {
		struct rte_eth_link link;
		uint32_t port;

		port = app.ports[i];
		memset(&link, 0, sizeof(link));
		rte_eth_link_get_nowait(port, &link);
		RTE_LOG(INFO, USER1, "Port %u (%u Gbps) %s\n",
			port,
			link.link_speed / 1000,
			link.link_status ? "UP" : "DOWN");

		if (link.link_status == 0)
			all_ports_up = 0;
	}

	if (all_ports_up == 0)
		rte_panic("Some NIC ports are DOWN\n");
}

static void
app_init_ports(void)
{
	uint32_t i;

	/* Init NIC ports, then start the ports */
	for (i = 0; i < app.n_ports; i++) {
		uint32_t port;
		int ret;

		port = app.ports[i];
		RTE_LOG(INFO, USER1, "Initializing NIC port %u ...\n", port);

		/* Init port */
		ret = rte_eth_dev_configure(
			port,
			1,
			1,
			&app.port_conf);
		if (ret < 0)
			rte_panic("Cannot init NIC port %u (%d)\n", port, ret);
		rte_eth_promiscuous_enable(port);

		/* Init RX queues */
		ret = rte_eth_rx_queue_setup(
			port,
			0,
			app.rsz_hwq_rx,
			rte_eth_dev_socket_id(port),
			&app.rx_conf,
			app.pool);
		if (ret < 0)
			rte_panic("Cannot init RX for port %u (%d)\n",
				(uint32_t) port, ret);

		/* Init TX queues */
		ret = rte_eth_tx_queue_setup(
			port,
			0,
			app.rsz_hwq_tx,
			rte_eth_dev_socket_id(port),
			&app.tx_conf);
		if (ret < 0)
			rte_panic("Cannot init TX for port %u (%d)\n", port,
				ret);

		/* Start port */
		ret = rte_eth_dev_start(port);
		if (ret < 0)
			rte_panic("Cannot start port %u (%d)\n", port, ret);
	}

	app_ports_check_link();
}

#define APP_PING_TIMEOUT_SEC                               5

void
app_ping(void)
{
	unsigned i;
	uint64_t timestamp, diff_tsc;

	const uint64_t timeout = rte_get_tsc_hz() * APP_PING_TIMEOUT_SEC;

	for (i = 0; i < RTE_MAX_LCORE; i++) {
		struct app_core_params *p = &app.cores[i];
		struct rte_ring *ring_req, *ring_resp;
		void *msg;
		struct app_msg_req *req;
		int status;

		if ((p->core_type != APP_CORE_FC) &&
		    (p->core_type != APP_CORE_FW) &&
			(p->core_type != APP_CORE_RT) &&
			(p->core_type != APP_CORE_RX))
			continue;

		ring_req = app_get_ring_req(p->core_id);
		ring_resp = app_get_ring_resp(p->core_id);

		/* Fill request message */
		msg = (void *)rte_ctrlmbuf_alloc(app.msg_pool);
		if (msg == NULL)
			rte_panic("Unable to allocate new message\n");

		req = (struct app_msg_req *)
				rte_ctrlmbuf_data((struct rte_mbuf *)msg);
		req->type = APP_MSG_REQ_PING;

		/* Send request */
		do {
			status = rte_ring_sp_enqueue(ring_req, msg);
		} while (status == -ENOBUFS);

		/* Wait for response */
		timestamp = rte_rdtsc();
		do {
			status = rte_ring_sc_dequeue(ring_resp, &msg);
			diff_tsc = rte_rdtsc() - timestamp;

			if (unlikely(diff_tsc > timeout))
				rte_panic("Core %u of type %d does not respond "
					"to requests\n", p->core_id,
					p->core_type);
		} while (status != 0);

		/* Free message buffer */
		rte_ctrlmbuf_free(msg);
	}
}

static void
app_init_etc(void)
{
	if ((app_get_first_core_id(APP_CORE_IPV4_FRAG) != RTE_MAX_LCORE) ||
		(app_get_first_core_id(APP_CORE_IPV4_RAS) != RTE_MAX_LCORE)) {
		RTE_LOG(INFO, USER1,
			"Activating the Ethernet header pop/push ...\n");
		app.ether_hdr_pop_push = 1;
	}
}

void
app_init(void)
{
	if ((sizeof(struct app_pkt_metadata) % RTE_CACHE_LINE_SIZE) != 0)
		rte_panic("Application pkt meta-data size mismatch\n");

	app_check_core_params();

	app_init_mbuf_pools();
	app_init_rings();
	app_init_ports();
	app_init_etc();

	RTE_LOG(INFO, USER1, "Initialization completed\n");
}
