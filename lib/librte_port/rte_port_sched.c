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
#include <string.h>

#include <rte_mbuf.h>
#include <rte_malloc.h>

#include "rte_port_sched.h"

/*
 * Reader
 */
struct rte_port_sched_reader {
	struct rte_sched_port *sched;
};

static void *
rte_port_sched_reader_create(void *params, int socket_id)
{
	struct rte_port_sched_reader_params *conf =
			(struct rte_port_sched_reader_params *) params;
	struct rte_port_sched_reader *port;

	/* Check input parameters */
	if ((conf == NULL) ||
	    (conf->sched == NULL)) {
		RTE_LOG(ERR, PORT, "%s: Invalid params\n", __func__);
		return NULL;
	}

	/* Memory allocation */
	port = rte_zmalloc_socket("PORT", sizeof(*port),
			CACHE_LINE_SIZE, socket_id);
	if (port == NULL) {
		RTE_LOG(ERR, PORT, "%s: Failed to allocate port\n", __func__);
		return NULL;
	}

	/* Initialization */
	port->sched = conf->sched;

	return port;
}

static int
rte_port_sched_reader_rx(void *port, struct rte_mbuf **pkts, uint32_t n_pkts)
{
	struct rte_port_sched_reader *p = (struct rte_port_sched_reader *) port;

	return rte_sched_port_dequeue(p->sched, pkts, n_pkts);
}

static int
rte_port_sched_reader_free(void *port)
{
	if (port == NULL) {
		RTE_LOG(ERR, PORT, "%s: port is NULL\n", __func__);
		return -EINVAL;
	}

	rte_free(port);

	return 0;
}

/*
 * Writer
 */
struct rte_port_sched_writer {
	struct rte_mbuf *tx_buf[2 * RTE_PORT_IN_BURST_SIZE_MAX];
	struct rte_sched_port *sched;
	uint32_t tx_burst_sz;
	uint32_t tx_buf_count;
	uint64_t bsz_mask;
};

static void *
rte_port_sched_writer_create(void *params, int socket_id)
{
	struct rte_port_sched_writer_params *conf =
			(struct rte_port_sched_writer_params *) params;
	struct rte_port_sched_writer *port;

	/* Check input parameters */
	if ((conf == NULL) ||
	    (conf->sched == NULL) ||
	    (conf->tx_burst_sz == 0) ||
	    (conf->tx_burst_sz > RTE_PORT_IN_BURST_SIZE_MAX) ||
		(!rte_is_power_of_2(conf->tx_burst_sz))) {
		RTE_LOG(ERR, PORT, "%s: Invalid params\n", __func__);
		return NULL;
	}

	/* Memory allocation */
	port = rte_zmalloc_socket("PORT", sizeof(*port),
			CACHE_LINE_SIZE, socket_id);
	if (port == NULL) {
		RTE_LOG(ERR, PORT, "%s: Failed to allocate port\n", __func__);
		return NULL;
	}

	/* Initialization */
	port->sched = conf->sched;
	port->tx_burst_sz = conf->tx_burst_sz;
	port->tx_buf_count = 0;
	port->bsz_mask = 1LLU << (conf->tx_burst_sz - 1);

	return port;
}

static int
rte_port_sched_writer_tx(void *port, struct rte_mbuf *pkt)
{
	struct rte_port_sched_writer *p = (struct rte_port_sched_writer *) port;

	p->tx_buf[p->tx_buf_count++] = pkt;
	if (p->tx_buf_count >= p->tx_burst_sz) {
		rte_sched_port_enqueue(p->sched, p->tx_buf, p->tx_buf_count);
		p->tx_buf_count = 0;
	}

	return 0;
}

static int
rte_port_sched_writer_tx_bulk(void *port,
		struct rte_mbuf **pkts,
		uint64_t pkts_mask)
{
	struct rte_port_sched_writer *p = (struct rte_port_sched_writer *) port;
	uint32_t bsz_mask = p->bsz_mask;
	uint32_t tx_buf_count = p->tx_buf_count;
	uint64_t expr = (pkts_mask & (pkts_mask + 1)) |
			((pkts_mask & bsz_mask) ^ bsz_mask);

	if (expr == 0) {
		uint64_t n_pkts = __builtin_popcountll(pkts_mask);

		if (tx_buf_count) {
			rte_sched_port_enqueue(p->sched, p->tx_buf,
				tx_buf_count);
			p->tx_buf_count = 0;
		}

		rte_sched_port_enqueue(p->sched, pkts, n_pkts);
	} else {
		for ( ; pkts_mask; ) {
			uint32_t pkt_index = __builtin_ctzll(pkts_mask);
			uint64_t pkt_mask = 1LLU << pkt_index;
			struct rte_mbuf *pkt = pkts[pkt_index];

			p->tx_buf[tx_buf_count++] = pkt;
			pkts_mask &= ~pkt_mask;
		}
		p->tx_buf_count = tx_buf_count;

		if (tx_buf_count >= p->tx_burst_sz) {
			rte_sched_port_enqueue(p->sched, p->tx_buf,
				tx_buf_count);
			p->tx_buf_count = 0;
		}
	}

	return 0;
}

static int
rte_port_sched_writer_flush(void *port)
{
	struct rte_port_sched_writer *p = (struct rte_port_sched_writer *) port;

	if (p->tx_buf_count) {
		rte_sched_port_enqueue(p->sched, p->tx_buf, p->tx_buf_count);
		p->tx_buf_count = 0;
	}

	return 0;
}

static int
rte_port_sched_writer_free(void *port)
{
	if (port == NULL) {
		RTE_LOG(ERR, PORT, "%s: port is NULL\n", __func__);
		return -EINVAL;
	}

	rte_port_sched_writer_flush(port);
	rte_free(port);

	return 0;
}

/*
 * Summary of port operations
 */
struct rte_port_in_ops rte_port_sched_reader_ops = {
	.f_create = rte_port_sched_reader_create,
	.f_free = rte_port_sched_reader_free,
	.f_rx = rte_port_sched_reader_rx,
};

struct rte_port_out_ops rte_port_sched_writer_ops = {
	.f_create = rte_port_sched_writer_create,
	.f_free = rte_port_sched_writer_free,
	.f_tx = rte_port_sched_writer_tx,
	.f_tx_bulk = rte_port_sched_writer_tx_bulk,
	.f_flush = rte_port_sched_writer_flush,
};
