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
#include <rte_ring.h>
#include <rte_malloc.h>

#include "rte_port_ring.h"

/*
 * Port RING Reader
 */
struct rte_port_ring_reader {
	struct rte_ring *ring;
};

static void *
rte_port_ring_reader_create(void *params, int socket_id)
{
	struct rte_port_ring_reader_params *conf =
			(struct rte_port_ring_reader_params *) params;
	struct rte_port_ring_reader *port;

	/* Check input parameters */
	if (conf == NULL) {
		RTE_LOG(ERR, PORT, "%s: params is NULL\n", __func__);
		return NULL;
	}

	/* Memory allocation */
	port = rte_zmalloc_socket("PORT", sizeof(*port),
			RTE_CACHE_LINE_SIZE, socket_id);
	if (port == NULL) {
		RTE_LOG(ERR, PORT, "%s: Failed to allocate port\n", __func__);
		return NULL;
	}

	/* Initialization */
	port->ring = conf->ring;

	return port;
}

static int
rte_port_ring_reader_rx(void *port, struct rte_mbuf **pkts, uint32_t n_pkts)
{
	struct rte_port_ring_reader *p = (struct rte_port_ring_reader *) port;

	return rte_ring_sc_dequeue_burst(p->ring, (void **) pkts, n_pkts);
}

static int
rte_port_ring_reader_free(void *port)
{
	if (port == NULL) {
		RTE_LOG(ERR, PORT, "%s: port is NULL\n", __func__);
		return -EINVAL;
	}

	rte_free(port);

	return 0;
}

/*
 * Port RING Writer
 */
struct rte_port_ring_writer {
	struct rte_mbuf *tx_buf[RTE_PORT_IN_BURST_SIZE_MAX];
	struct rte_ring *ring;
	uint32_t tx_burst_sz;
	uint32_t tx_buf_count;
};

static void *
rte_port_ring_writer_create(void *params, int socket_id)
{
	struct rte_port_ring_writer_params *conf =
			(struct rte_port_ring_writer_params *) params;
	struct rte_port_ring_writer *port;

	/* Check input parameters */
	if ((conf == NULL) ||
	    (conf->ring == NULL) ||
		(conf->tx_burst_sz > RTE_PORT_IN_BURST_SIZE_MAX)) {
		RTE_LOG(ERR, PORT, "%s: Invalid Parameters\n", __func__);
		return NULL;
	}

	/* Memory allocation */
	port = rte_zmalloc_socket("PORT", sizeof(*port),
			RTE_CACHE_LINE_SIZE, socket_id);
	if (port == NULL) {
		RTE_LOG(ERR, PORT, "%s: Failed to allocate port\n", __func__);
		return NULL;
	}

	/* Initialization */
	port->ring = conf->ring;
	port->tx_burst_sz = conf->tx_burst_sz;
	port->tx_buf_count = 0;

	return port;
}

static inline void
send_burst(struct rte_port_ring_writer *p)
{
	uint32_t nb_tx;

	nb_tx = rte_ring_sp_enqueue_burst(p->ring, (void **)p->tx_buf,
			p->tx_buf_count);

	for ( ; nb_tx < p->tx_buf_count; nb_tx++)
		rte_pktmbuf_free(p->tx_buf[nb_tx]);

	p->tx_buf_count = 0;
}

static int
rte_port_ring_writer_tx(void *port, struct rte_mbuf *pkt)
{
	struct rte_port_ring_writer *p = (struct rte_port_ring_writer *) port;

	p->tx_buf[p->tx_buf_count++] = pkt;
	if (p->tx_buf_count >= p->tx_burst_sz)
		send_burst(p);

	return 0;
}

static int
rte_port_ring_writer_tx_bulk(void *port,
		struct rte_mbuf **pkts,
		uint64_t pkts_mask)
{
	struct rte_port_ring_writer *p = (struct rte_port_ring_writer *) port;

	if ((pkts_mask & (pkts_mask + 1)) == 0) {
		uint64_t n_pkts = __builtin_popcountll(pkts_mask);
		uint32_t i;

		for (i = 0; i < n_pkts; i++) {
			struct rte_mbuf *pkt = pkts[i];

			p->tx_buf[p->tx_buf_count++] = pkt;
			if (p->tx_buf_count >= p->tx_burst_sz)
				send_burst(p);
		}
	} else {
		for ( ; pkts_mask; ) {
			uint32_t pkt_index = __builtin_ctzll(pkts_mask);
			uint64_t pkt_mask = 1LLU << pkt_index;
			struct rte_mbuf *pkt = pkts[pkt_index];

			p->tx_buf[p->tx_buf_count++] = pkt;
			if (p->tx_buf_count >= p->tx_burst_sz)
				send_burst(p);
			pkts_mask &= ~pkt_mask;
		}
	}

	return 0;
}

static int
rte_port_ring_writer_flush(void *port)
{
	struct rte_port_ring_writer *p = (struct rte_port_ring_writer *) port;

	if (p->tx_buf_count > 0)
		send_burst(p);

	return 0;
}

static int
rte_port_ring_writer_free(void *port)
{
	if (port == NULL) {
		RTE_LOG(ERR, PORT, "%s: Port is NULL\n", __func__);
		return -EINVAL;
	}

	rte_port_ring_writer_flush(port);
	rte_free(port);

	return 0;
}

/*
 * Summary of port operations
 */
struct rte_port_in_ops rte_port_ring_reader_ops = {
	.f_create = rte_port_ring_reader_create,
	.f_free = rte_port_ring_reader_free,
	.f_rx = rte_port_ring_reader_rx,
};

struct rte_port_out_ops rte_port_ring_writer_ops = {
	.f_create = rte_port_ring_writer_create,
	.f_free = rte_port_ring_writer_free,
	.f_tx = rte_port_ring_writer_tx,
	.f_tx_bulk = rte_port_ring_writer_tx_bulk,
	.f_flush = rte_port_ring_writer_flush,
};
