/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */
#include <string.h>

#include <rte_malloc.h>

#include "rte_port_sched.h"

/*
 * Reader
 */
#ifdef RTE_PORT_STATS_COLLECT

#define RTE_PORT_SCHED_READER_PKTS_IN_ADD(port, val) \
	port->stats.n_pkts_in += val
#define RTE_PORT_SCHED_READER_PKTS_DROP_ADD(port, val) \
	port->stats.n_pkts_drop += val

#else

#define RTE_PORT_SCHED_READER_PKTS_IN_ADD(port, val)
#define RTE_PORT_SCHED_READER_PKTS_DROP_ADD(port, val)

#endif

struct rte_port_sched_reader {
	struct rte_port_in_stats stats;

	struct rte_sched_port *sched;
};

static void *
rte_port_sched_reader_create(void *params, int socket_id)
{
	struct rte_port_sched_reader_params *conf =
			params;
	struct rte_port_sched_reader *port;

	/* Check input parameters */
	if ((conf == NULL) ||
	    (conf->sched == NULL)) {
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
	port->sched = conf->sched;

	return port;
}

static int
rte_port_sched_reader_rx(void *port, struct rte_mbuf **pkts, uint32_t n_pkts)
{
	struct rte_port_sched_reader *p = port;
	uint32_t nb_rx;

	nb_rx = rte_sched_port_dequeue(p->sched, pkts, n_pkts);
	RTE_PORT_SCHED_READER_PKTS_IN_ADD(p, nb_rx);

	return nb_rx;
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

static int
rte_port_sched_reader_stats_read(void *port,
		struct rte_port_in_stats *stats, int clear)
{
	struct rte_port_sched_reader *p =
		port;

	if (stats != NULL)
		memcpy(stats, &p->stats, sizeof(p->stats));

	if (clear)
		memset(&p->stats, 0, sizeof(p->stats));

	return 0;
}

/*
 * Writer
 */
#ifdef RTE_PORT_STATS_COLLECT

#define RTE_PORT_SCHED_WRITER_STATS_PKTS_IN_ADD(port, val) \
	port->stats.n_pkts_in += val
#define RTE_PORT_SCHED_WRITER_STATS_PKTS_DROP_ADD(port, val) \
	port->stats.n_pkts_drop += val

#else

#define RTE_PORT_SCHED_WRITER_STATS_PKTS_IN_ADD(port, val)
#define RTE_PORT_SCHED_WRITER_STATS_PKTS_DROP_ADD(port, val)

#endif

struct rte_port_sched_writer {
	struct rte_port_out_stats stats;

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
			params;
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
			RTE_CACHE_LINE_SIZE, socket_id);
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
	RTE_PORT_SCHED_WRITER_STATS_PKTS_IN_ADD(p, 1);
	if (p->tx_buf_count >= p->tx_burst_sz) {
		__rte_unused uint32_t nb_tx;

		nb_tx = rte_sched_port_enqueue(p->sched, p->tx_buf, p->tx_buf_count);
		RTE_PORT_SCHED_WRITER_STATS_PKTS_DROP_ADD(p, p->tx_buf_count - nb_tx);
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
	uint64_t bsz_mask = p->bsz_mask;
	uint32_t tx_buf_count = p->tx_buf_count;
	uint64_t expr = (pkts_mask & (pkts_mask + 1)) |
			((pkts_mask & bsz_mask) ^ bsz_mask);

	if (expr == 0) {
		__rte_unused uint32_t nb_tx;
		uint64_t n_pkts = rte_popcount64(pkts_mask);

		if (tx_buf_count) {
			nb_tx = rte_sched_port_enqueue(p->sched, p->tx_buf,
				tx_buf_count);
			RTE_PORT_SCHED_WRITER_STATS_PKTS_DROP_ADD(p, tx_buf_count - nb_tx);
			p->tx_buf_count = 0;
		}

		nb_tx = rte_sched_port_enqueue(p->sched, pkts, n_pkts);
		RTE_PORT_SCHED_WRITER_STATS_PKTS_DROP_ADD(p, n_pkts - nb_tx);
	} else {
		for ( ; pkts_mask; ) {
			uint32_t pkt_index = rte_ctz64(pkts_mask);
			uint64_t pkt_mask = 1LLU << pkt_index;
			struct rte_mbuf *pkt = pkts[pkt_index];

			p->tx_buf[tx_buf_count++] = pkt;
			RTE_PORT_SCHED_WRITER_STATS_PKTS_IN_ADD(p, 1);
			pkts_mask &= ~pkt_mask;
		}
		p->tx_buf_count = tx_buf_count;

		if (tx_buf_count >= p->tx_burst_sz) {
			__rte_unused uint32_t nb_tx;

			nb_tx = rte_sched_port_enqueue(p->sched, p->tx_buf,
				tx_buf_count);
			RTE_PORT_SCHED_WRITER_STATS_PKTS_DROP_ADD(p, tx_buf_count - nb_tx);
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
		__rte_unused uint32_t nb_tx;

		nb_tx = rte_sched_port_enqueue(p->sched, p->tx_buf, p->tx_buf_count);
		RTE_PORT_SCHED_WRITER_STATS_PKTS_DROP_ADD(p, p->tx_buf_count - nb_tx);
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

static int
rte_port_sched_writer_stats_read(void *port,
		struct rte_port_out_stats *stats, int clear)
{
	struct rte_port_sched_writer *p =
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
struct rte_port_in_ops rte_port_sched_reader_ops = {
	.f_create = rte_port_sched_reader_create,
	.f_free = rte_port_sched_reader_free,
	.f_rx = rte_port_sched_reader_rx,
	.f_stats = rte_port_sched_reader_stats_read,
};

struct rte_port_out_ops rte_port_sched_writer_ops = {
	.f_create = rte_port_sched_writer_create,
	.f_free = rte_port_sched_writer_free,
	.f_tx = rte_port_sched_writer_tx,
	.f_tx_bulk = rte_port_sched_writer_tx_bulk,
	.f_flush = rte_port_sched_writer_flush,
	.f_stats = rte_port_sched_writer_stats_read,
};
