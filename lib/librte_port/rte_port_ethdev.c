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
#include <stdint.h>

#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>

#include "rte_port_ethdev.h"

/*
 * Port ETHDEV Reader
 */
#ifdef RTE_PORT_STATS_COLLECT

#define RTE_PORT_ETHDEV_READER_STATS_PKTS_IN_ADD(port, val) \
	port->stats.n_pkts_in += val
#define RTE_PORT_ETHDEV_READER_STATS_PKTS_DROP_ADD(port, val) \
	port->stats.n_pkts_drop += val

#else

#define RTE_PORT_ETHDEV_READER_STATS_PKTS_IN_ADD(port, val)
#define RTE_PORT_ETHDEV_READER_STATS_PKTS_DROP_ADD(port, val)

#endif

struct rte_port_ethdev_reader {
	struct rte_port_in_stats stats;

	uint16_t queue_id;
	uint8_t port_id;
};

static void *
rte_port_ethdev_reader_create(void *params, int socket_id)
{
	struct rte_port_ethdev_reader_params *conf =
			params;
	struct rte_port_ethdev_reader *port;

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
	port->port_id = conf->port_id;
	port->queue_id = conf->queue_id;

	return port;
}

static int
rte_port_ethdev_reader_rx(void *port, struct rte_mbuf **pkts, uint32_t n_pkts)
{
	struct rte_port_ethdev_reader *p =
		port;
	uint16_t rx_pkt_cnt;

	rx_pkt_cnt = rte_eth_rx_burst(p->port_id, p->queue_id, pkts, n_pkts);
	RTE_PORT_ETHDEV_READER_STATS_PKTS_IN_ADD(p, rx_pkt_cnt);
	return rx_pkt_cnt;
}

static int
rte_port_ethdev_reader_free(void *port)
{
	if (port == NULL) {
		RTE_LOG(ERR, PORT, "%s: port is NULL\n", __func__);
		return -EINVAL;
	}

	rte_free(port);

	return 0;
}

static int rte_port_ethdev_reader_stats_read(void *port,
		struct rte_port_in_stats *stats, int clear)
{
	struct rte_port_ethdev_reader *p =
			port;

	if (stats != NULL)
		memcpy(stats, &p->stats, sizeof(p->stats));

	if (clear)
		memset(&p->stats, 0, sizeof(p->stats));

	return 0;
}

/*
 * Port ETHDEV Writer
 */
#ifdef RTE_PORT_STATS_COLLECT

#define RTE_PORT_ETHDEV_WRITER_STATS_PKTS_IN_ADD(port, val) \
	port->stats.n_pkts_in += val
#define RTE_PORT_ETHDEV_WRITER_STATS_PKTS_DROP_ADD(port, val) \
	port->stats.n_pkts_drop += val

#else

#define RTE_PORT_ETHDEV_WRITER_STATS_PKTS_IN_ADD(port, val)
#define RTE_PORT_ETHDEV_WRITER_STATS_PKTS_DROP_ADD(port, val)

#endif

struct rte_port_ethdev_writer {
	struct rte_port_out_stats stats;

	struct rte_mbuf *tx_buf[2 * RTE_PORT_IN_BURST_SIZE_MAX];
	uint32_t tx_burst_sz;
	uint16_t tx_buf_count;
	uint64_t bsz_mask;
	uint16_t queue_id;
	uint8_t port_id;
};

static void *
rte_port_ethdev_writer_create(void *params, int socket_id)
{
	struct rte_port_ethdev_writer_params *conf =
			params;
	struct rte_port_ethdev_writer *port;

	/* Check input parameters */
	if ((conf == NULL) ||
		(conf->tx_burst_sz == 0) ||
		(conf->tx_burst_sz > RTE_PORT_IN_BURST_SIZE_MAX) ||
		(!rte_is_power_of_2(conf->tx_burst_sz))) {
		RTE_LOG(ERR, PORT, "%s: Invalid input parameters\n", __func__);
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
	port->port_id = conf->port_id;
	port->queue_id = conf->queue_id;
	port->tx_burst_sz = conf->tx_burst_sz;
	port->tx_buf_count = 0;
	port->bsz_mask = 1LLU << (conf->tx_burst_sz - 1);

	return port;
}

static inline void
send_burst(struct rte_port_ethdev_writer *p)
{
	uint32_t nb_tx;

	nb_tx = rte_eth_tx_burst(p->port_id, p->queue_id,
			 p->tx_buf, p->tx_buf_count);

	RTE_PORT_ETHDEV_WRITER_STATS_PKTS_DROP_ADD(p, p->tx_buf_count - nb_tx);
	for ( ; nb_tx < p->tx_buf_count; nb_tx++)
		rte_pktmbuf_free(p->tx_buf[nb_tx]);

	p->tx_buf_count = 0;
}

static int
rte_port_ethdev_writer_tx(void *port, struct rte_mbuf *pkt)
{
	struct rte_port_ethdev_writer *p =
		port;

	p->tx_buf[p->tx_buf_count++] = pkt;
	RTE_PORT_ETHDEV_WRITER_STATS_PKTS_IN_ADD(p, 1);
	if (p->tx_buf_count >= p->tx_burst_sz)
		send_burst(p);

	return 0;
}

static int
rte_port_ethdev_writer_tx_bulk(void *port,
		struct rte_mbuf **pkts,
		uint64_t pkts_mask)
{
	struct rte_port_ethdev_writer *p =
		port;
	uint64_t bsz_mask = p->bsz_mask;
	uint32_t tx_buf_count = p->tx_buf_count;
	uint64_t expr = (pkts_mask & (pkts_mask + 1)) |
			((pkts_mask & bsz_mask) ^ bsz_mask);

	if (expr == 0) {
		uint64_t n_pkts = __builtin_popcountll(pkts_mask);
		uint32_t n_pkts_ok;

		if (tx_buf_count)
			send_burst(p);

		RTE_PORT_ETHDEV_WRITER_STATS_PKTS_IN_ADD(p, n_pkts);
		n_pkts_ok = rte_eth_tx_burst(p->port_id, p->queue_id, pkts,
			n_pkts);

		RTE_PORT_ETHDEV_WRITER_STATS_PKTS_DROP_ADD(p, n_pkts - n_pkts_ok);
		for ( ; n_pkts_ok < n_pkts; n_pkts_ok++) {
			struct rte_mbuf *pkt = pkts[n_pkts_ok];

			rte_pktmbuf_free(pkt);
		}
	} else {
		for ( ; pkts_mask; ) {
			uint32_t pkt_index = __builtin_ctzll(pkts_mask);
			uint64_t pkt_mask = 1LLU << pkt_index;
			struct rte_mbuf *pkt = pkts[pkt_index];

			p->tx_buf[tx_buf_count++] = pkt;
			RTE_PORT_ETHDEV_WRITER_STATS_PKTS_IN_ADD(p, 1);
			pkts_mask &= ~pkt_mask;
		}

		p->tx_buf_count = tx_buf_count;
		if (tx_buf_count >= p->tx_burst_sz)
			send_burst(p);
	}

	return 0;
}

static int
rte_port_ethdev_writer_flush(void *port)
{
	struct rte_port_ethdev_writer *p =
		port;

	if (p->tx_buf_count > 0)
		send_burst(p);

	return 0;
}

static int
rte_port_ethdev_writer_free(void *port)
{
	if (port == NULL) {
		RTE_LOG(ERR, PORT, "%s: Port is NULL\n", __func__);
		return -EINVAL;
	}

	rte_port_ethdev_writer_flush(port);
	rte_free(port);

	return 0;
}

static int rte_port_ethdev_writer_stats_read(void *port,
		struct rte_port_out_stats *stats, int clear)
{
	struct rte_port_ethdev_writer *p =
		port;

	if (stats != NULL)
		memcpy(stats, &p->stats, sizeof(p->stats));

	if (clear)
		memset(&p->stats, 0, sizeof(p->stats));

	return 0;
}

/*
 * Port ETHDEV Writer Nodrop
 */
#ifdef RTE_PORT_STATS_COLLECT

#define RTE_PORT_ETHDEV_WRITER_NODROP_STATS_PKTS_IN_ADD(port, val) \
	port->stats.n_pkts_in += val
#define RTE_PORT_ETHDEV_WRITER_NODROP_STATS_PKTS_DROP_ADD(port, val) \
	port->stats.n_pkts_drop += val

#else

#define RTE_PORT_ETHDEV_WRITER_NODROP_STATS_PKTS_IN_ADD(port, val)
#define RTE_PORT_ETHDEV_WRITER_NODROP_STATS_PKTS_DROP_ADD(port, val)

#endif

struct rte_port_ethdev_writer_nodrop {
	struct rte_port_out_stats stats;

	struct rte_mbuf *tx_buf[2 * RTE_PORT_IN_BURST_SIZE_MAX];
	uint32_t tx_burst_sz;
	uint16_t tx_buf_count;
	uint64_t bsz_mask;
	uint64_t n_retries;
	uint16_t queue_id;
	uint8_t port_id;
};

static void *
rte_port_ethdev_writer_nodrop_create(void *params, int socket_id)
{
	struct rte_port_ethdev_writer_nodrop_params *conf =
			params;
	struct rte_port_ethdev_writer_nodrop *port;

	/* Check input parameters */
	if ((conf == NULL) ||
		(conf->tx_burst_sz == 0) ||
		(conf->tx_burst_sz > RTE_PORT_IN_BURST_SIZE_MAX) ||
		(!rte_is_power_of_2(conf->tx_burst_sz))) {
		RTE_LOG(ERR, PORT, "%s: Invalid input parameters\n", __func__);
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
	port->port_id = conf->port_id;
	port->queue_id = conf->queue_id;
	port->tx_burst_sz = conf->tx_burst_sz;
	port->tx_buf_count = 0;
	port->bsz_mask = 1LLU << (conf->tx_burst_sz - 1);

	/*
	 * When n_retries is 0 it means that we should wait for every packet to
	 * send no matter how many retries should it take. To limit number of
	 * branches in fast path, we use UINT64_MAX instead of branching.
	 */
	port->n_retries = (conf->n_retries == 0) ? UINT64_MAX : conf->n_retries;

	return port;
}

static inline void
send_burst_nodrop(struct rte_port_ethdev_writer_nodrop *p)
{
	uint32_t nb_tx = 0, i;

	nb_tx = rte_eth_tx_burst(p->port_id, p->queue_id, p->tx_buf,
			p->tx_buf_count);

	/* We sent all the packets in a first try */
	if (nb_tx >= p->tx_buf_count) {
		p->tx_buf_count = 0;
		return;
	}

	for (i = 0; i < p->n_retries; i++) {
		nb_tx += rte_eth_tx_burst(p->port_id, p->queue_id,
							 p->tx_buf + nb_tx, p->tx_buf_count - nb_tx);

		/* We sent all the packets in more than one try */
		if (nb_tx >= p->tx_buf_count) {
			p->tx_buf_count = 0;
			return;
		}
	}

	/* We didn't send the packets in maximum allowed attempts */
	RTE_PORT_ETHDEV_WRITER_NODROP_STATS_PKTS_DROP_ADD(p, p->tx_buf_count - nb_tx);
	for ( ; nb_tx < p->tx_buf_count; nb_tx++)
		rte_pktmbuf_free(p->tx_buf[nb_tx]);

	p->tx_buf_count = 0;
}

static int
rte_port_ethdev_writer_nodrop_tx(void *port, struct rte_mbuf *pkt)
{
	struct rte_port_ethdev_writer_nodrop *p =
		port;

	p->tx_buf[p->tx_buf_count++] = pkt;
	RTE_PORT_ETHDEV_WRITER_NODROP_STATS_PKTS_IN_ADD(p, 1);
	if (p->tx_buf_count >= p->tx_burst_sz)
		send_burst_nodrop(p);

	return 0;
}

static int
rte_port_ethdev_writer_nodrop_tx_bulk(void *port,
		struct rte_mbuf **pkts,
		uint64_t pkts_mask)
{
	struct rte_port_ethdev_writer_nodrop *p =
		port;

	uint64_t bsz_mask = p->bsz_mask;
	uint32_t tx_buf_count = p->tx_buf_count;
	uint64_t expr = (pkts_mask & (pkts_mask + 1)) |
			((pkts_mask & bsz_mask) ^ bsz_mask);

	if (expr == 0) {
		uint64_t n_pkts = __builtin_popcountll(pkts_mask);
		uint32_t n_pkts_ok;

		if (tx_buf_count)
			send_burst_nodrop(p);

		RTE_PORT_ETHDEV_WRITER_NODROP_STATS_PKTS_IN_ADD(p, n_pkts);
		n_pkts_ok = rte_eth_tx_burst(p->port_id, p->queue_id, pkts,
			n_pkts);

		if (n_pkts_ok >= n_pkts)
			return 0;

		/*
		 * If we did not manage to send all packets in single burst,
		 * move remaining packets to the buffer and call send burst.
		 */
		for (; n_pkts_ok < n_pkts; n_pkts_ok++) {
			struct rte_mbuf *pkt = pkts[n_pkts_ok];
			p->tx_buf[p->tx_buf_count++] = pkt;
		}
		send_burst_nodrop(p);
	} else {
		for ( ; pkts_mask; ) {
			uint32_t pkt_index = __builtin_ctzll(pkts_mask);
			uint64_t pkt_mask = 1LLU << pkt_index;
			struct rte_mbuf *pkt = pkts[pkt_index];

			p->tx_buf[tx_buf_count++] = pkt;
			RTE_PORT_ETHDEV_WRITER_NODROP_STATS_PKTS_IN_ADD(p, 1);
			pkts_mask &= ~pkt_mask;
		}

		p->tx_buf_count = tx_buf_count;
		if (tx_buf_count >= p->tx_burst_sz)
			send_burst_nodrop(p);
	}

	return 0;
}

static int
rte_port_ethdev_writer_nodrop_flush(void *port)
{
	struct rte_port_ethdev_writer_nodrop *p =
		port;

	if (p->tx_buf_count > 0)
		send_burst_nodrop(p);

	return 0;
}

static int
rte_port_ethdev_writer_nodrop_free(void *port)
{
	if (port == NULL) {
		RTE_LOG(ERR, PORT, "%s: Port is NULL\n", __func__);
		return -EINVAL;
	}

	rte_port_ethdev_writer_nodrop_flush(port);
	rte_free(port);

	return 0;
}

static int rte_port_ethdev_writer_nodrop_stats_read(void *port,
		struct rte_port_out_stats *stats, int clear)
{
	struct rte_port_ethdev_writer_nodrop *p =
		port;

	if (stats != NULL)
		memcpy(stats, &p->stats, sizeof(p->stats));

	if (clear)
		memset(&p->stats, 0, sizeof(p->stats));

	return 0;
}

/*
 * Summary of port operations
 */
struct rte_port_in_ops rte_port_ethdev_reader_ops = {
	.f_create = rte_port_ethdev_reader_create,
	.f_free = rte_port_ethdev_reader_free,
	.f_rx = rte_port_ethdev_reader_rx,
	.f_stats = rte_port_ethdev_reader_stats_read,
};

struct rte_port_out_ops rte_port_ethdev_writer_ops = {
	.f_create = rte_port_ethdev_writer_create,
	.f_free = rte_port_ethdev_writer_free,
	.f_tx = rte_port_ethdev_writer_tx,
	.f_tx_bulk = rte_port_ethdev_writer_tx_bulk,
	.f_flush = rte_port_ethdev_writer_flush,
	.f_stats = rte_port_ethdev_writer_stats_read,
};

struct rte_port_out_ops rte_port_ethdev_writer_nodrop_ops = {
	.f_create = rte_port_ethdev_writer_nodrop_create,
	.f_free = rte_port_ethdev_writer_nodrop_free,
	.f_tx = rte_port_ethdev_writer_nodrop_tx,
	.f_tx_bulk = rte_port_ethdev_writer_nodrop_tx_bulk,
	.f_flush = rte_port_ethdev_writer_nodrop_flush,
	.f_stats = rte_port_ethdev_writer_nodrop_stats_read,
};
