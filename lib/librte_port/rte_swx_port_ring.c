/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */
#include <string.h>
#include <stdint.h>

#include <rte_mbuf.h>
#include <rte_ring.h>
#include <rte_hexdump.h>

#include "rte_swx_port_ring.h"

#ifndef TRACE_LEVEL
#define TRACE_LEVEL 0
#endif

#if TRACE_LEVEL
#define TRACE(...) printf(__VA_ARGS__)
#else
#define TRACE(...)
#endif

/*
 * Reader
 */
struct reader {
	struct {
		struct rte_ring *ring;
		char *name;
		uint32_t burst_size;
	} params;
	struct rte_swx_port_in_stats stats;
	struct rte_mbuf **pkts;
	int n_pkts;
	int pos;
};

static void *
reader_create(void *args)
{
	struct rte_swx_port_ring_reader_params *params = args;
	struct rte_ring *ring;
	struct reader *p = NULL;

	/* Check input parameters. */
	if (!params || !params->name || !params->burst_size)
		goto error;

	ring = rte_ring_lookup(params->name);
	if (!ring)
		goto error;

	/* Memory allocation. */
	p = calloc(1, sizeof(struct reader));
	if (!p)
		goto error;

	p->params.name = strdup(params->name);
	if (!p->params.name)
		goto error;

	p->pkts = calloc(params->burst_size, sizeof(struct rte_mbuf *));
	if (!p->pkts)
		goto error;

	/* Initialization. */
	p->params.ring = ring;
	p->params.burst_size = params->burst_size;

	return p;

error:
	if (!p)
		return NULL;

	free(p->pkts);
	free(p->params.name);
	free(p);
	return NULL;
}

static int
reader_pkt_rx(void *port, struct rte_swx_pkt *pkt)
{
	struct reader *p = port;
	struct rte_mbuf *m;

	if (p->pos == p->n_pkts) {
		int n_pkts;

		n_pkts = rte_ring_sc_dequeue_burst(p->params.ring,
			(void **) p->pkts,
			p->params.burst_size,
			NULL);
		if (!n_pkts) {
			p->stats.n_empty++;
			return 0;
		}

		TRACE("[Ring %s] %d packets in\n",
		      p->params.name,
		      n_pkts);

		p->n_pkts = n_pkts;
		p->pos = 0;
	}

	m = p->pkts[p->pos++];
	pkt->handle = m;
	pkt->pkt = m->buf_addr;
	pkt->offset = m->data_off;
	pkt->length = m->pkt_len;

	TRACE("[Ring %s] Pkt %d (%u bytes at offset %u)\n",
	      (uint32_t)p->params.name,
	      p->pos - 1,
	      pkt->length,
	      pkt->offset);
	if (TRACE_LEVEL)
		rte_hexdump(stdout,
			    NULL,
			    &((uint8_t *)m->buf_addr)[m->data_off],
			    m->data_len);

	p->stats.n_pkts++;
	p->stats.n_bytes += pkt->length;

	return 1;
}

static void
reader_free(void *port)
{
	struct reader *p = port;
	int i;

	if (!p)
		return;

	for (i = 0; i < p->n_pkts; i++) {
		struct rte_mbuf *pkt = p->pkts[i];

		rte_pktmbuf_free(pkt);
	}

	free(p->pkts);
	free(p->params.name);
	free(p);
}

static void
reader_stats_read(void *port, struct rte_swx_port_in_stats *stats)
{
	struct reader *p = port;

	if (!stats)
		return;

	memcpy(stats, &p->stats, sizeof(p->stats));
}

/*
 * Writer
 */
struct writer {
	struct {
		struct rte_ring *ring;
		char *name;
		uint32_t burst_size;
	} params;
	struct rte_swx_port_out_stats stats;

	struct rte_mbuf **pkts;
	int n_pkts;
};

static void *
writer_create(void *args)
{
	struct rte_swx_port_ring_writer_params *params = args;
	struct rte_ring *ring;
	struct writer *p = NULL;

	/* Check input parameters. */
	if (!params || !params->name || !params->burst_size)
		goto error;

	ring = rte_ring_lookup(params->name);
	if (!ring)
		goto error;

	/* Memory allocation. */
	p = calloc(1, sizeof(struct writer));
	if (!p)
		goto error;

	p->params.name = strdup(params->name);
	if (!p->params.name)
		goto error;

	p->pkts = calloc(params->burst_size, sizeof(struct rte_mbuf *));
	if (!p->pkts)
		goto error;

	/* Initialization. */
	p->params.ring = ring;
	p->params.burst_size = params->burst_size;

	return p;

error:
	if (!p)
		return NULL;

	free(p->params.name);
	free(p->pkts);
	free(p);
	return NULL;
}

static void
__writer_flush(struct writer *p)
{
	int n_pkts;

	for (n_pkts = 0; ; ) {
		n_pkts += rte_ring_sp_enqueue_burst(p->params.ring,
						    (void **)p->pkts + n_pkts,
						    p->n_pkts - n_pkts,
						    NULL);

		TRACE("[Ring %s] %d packets out\n", p->params.name, n_pkts);

		if (n_pkts == p->n_pkts)
			break;
	}

	p->n_pkts = 0;
}

static void
writer_pkt_tx(void *port, struct rte_swx_pkt *pkt)
{
	struct writer *p = port;
	struct rte_mbuf *m = pkt->handle;

	TRACE("[Ring %s] Pkt %d (%u bytes at offset %u)\n",
	      p->params.name,
	      p->n_pkts - 1,
	      pkt->length,
	      pkt->offset);
	if (TRACE_LEVEL)
		rte_hexdump(stdout, NULL, &pkt->pkt[pkt->offset], pkt->length);

	m->pkt_len = pkt->length;
	m->data_len = (uint16_t)pkt->length;
	m->data_off = (uint16_t)pkt->offset;

	p->stats.n_pkts++;
	p->stats.n_bytes += pkt->length;

	p->pkts[p->n_pkts++] = m;
	if (p->n_pkts ==  (int)p->params.burst_size)
		__writer_flush(p);
}

static void
writer_flush(void *port)
{
	struct writer *p = port;

	if (p->n_pkts)
		__writer_flush(p);
}

static void
writer_free(void *port)
{
	struct writer *p = port;

	if (!p)
		return;

	writer_flush(p);
	free(p->pkts);
	free(p->params.name);
	free(port);
}

static void
writer_stats_read(void *port, struct rte_swx_port_out_stats *stats)
{
	struct writer *p = port;

	if (!stats)
		return;

	memcpy(stats, &p->stats, sizeof(p->stats));
}

/*
 * Summary of port operations
 */
struct rte_swx_port_in_ops rte_swx_port_ring_reader_ops = {
	.create = reader_create,
	.free = reader_free,
	.pkt_rx = reader_pkt_rx,
	.stats_read = reader_stats_read,
};

struct rte_swx_port_out_ops rte_swx_port_ring_writer_ops = {
	.create = writer_create,
	.free = writer_free,
	.pkt_tx = writer_pkt_tx,
	.flush = writer_flush,
	.stats_read = writer_stats_read,
};
