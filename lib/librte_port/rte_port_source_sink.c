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
#include <stdint.h>
#include <string.h>

#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_malloc.h>

#include "rte_port_source_sink.h"

/*
 * Port SOURCE
 */
struct rte_port_source {
	struct rte_mempool *mempool;
};

static void *
rte_port_source_create(void *params, int socket_id)
{
	struct rte_port_source_params *p =
			(struct rte_port_source_params *) params;
	struct rte_port_source *port;

	/* Check input arguments*/
	if ((p == NULL) || (p->mempool == NULL)) {
		RTE_LOG(ERR, PORT, "%s: Invalid params\n", __func__);
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
	port->mempool = (struct rte_mempool *) p->mempool;

	return port;
}

static int
rte_port_source_free(void *port)
{
	/* Check input parameters */
	if (port == NULL)
		return 0;

	rte_free(port);

	return 0;
}

static int
rte_port_source_rx(void *port, struct rte_mbuf **pkts, uint32_t n_pkts)
{
	struct rte_port_source *p = (struct rte_port_source *) port;

	if (rte_mempool_get_bulk(p->mempool, (void **) pkts, n_pkts) != 0)
		return 0;

	return n_pkts;
}

/*
 * Port SINK
 */
static void *
rte_port_sink_create(__rte_unused void *params, __rte_unused int socket_id)
{
	return (void *) 1;
}

static int
rte_port_sink_tx(__rte_unused void *port, struct rte_mbuf *pkt)
{
	rte_pktmbuf_free(pkt);

	return 0;
}

static int
rte_port_sink_tx_bulk(__rte_unused void *port, struct rte_mbuf **pkts,
	uint64_t pkts_mask)
{
	if ((pkts_mask & (pkts_mask + 1)) == 0) {
		uint64_t n_pkts = __builtin_popcountll(pkts_mask);
		uint32_t i;

		for (i = 0; i < n_pkts; i++) {
			struct rte_mbuf *pkt = pkts[i];

			rte_pktmbuf_free(pkt);
		}
	} else {
		for ( ; pkts_mask; ) {
			uint32_t pkt_index = __builtin_ctzll(pkts_mask);
			uint64_t pkt_mask = 1LLU << pkt_index;
			struct rte_mbuf *pkt = pkts[pkt_index];

			rte_pktmbuf_free(pkt);
			pkts_mask &= ~pkt_mask;
		}
	}

	return 0;
}

/*
 * Summary of port operations
 */
struct rte_port_in_ops rte_port_source_ops = {
	.f_create = rte_port_source_create,
	.f_free = rte_port_source_free,
	.f_rx = rte_port_source_rx,
};

struct rte_port_out_ops rte_port_sink_ops = {
	.f_create = rte_port_sink_create,
	.f_free = NULL,
	.f_tx = rte_port_sink_tx,
	.f_tx_bulk = rte_port_sink_tx_bulk,
	.f_flush = NULL,
};
