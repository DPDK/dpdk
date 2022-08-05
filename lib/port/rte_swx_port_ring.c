/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

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
	uint32_t n_bytes;
	int flush_flag;
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

static inline void
__writer_flush(struct writer *p)
{
	struct rte_mbuf **pkts = p->pkts;
	uint64_t n_pkts_total = p->stats.n_pkts;
	uint64_t n_bytes_total = p->stats.n_bytes;
	uint64_t n_pkts_drop_total = p->stats.n_pkts_drop;
	uint64_t n_bytes_drop_total = p->stats.n_bytes_drop;
	int n_pkts = p->n_pkts, n_pkts_drop, n_pkts_tx;
	uint32_t n_bytes = p->n_bytes, n_bytes_drop = 0;

	/* Packet TX. */
	n_pkts_tx = rte_ring_sp_enqueue_burst(p->params.ring,
					      (void **)pkts,
					      n_pkts,
					      NULL);

	/* Packet drop. */
	n_pkts_drop = n_pkts - n_pkts_tx;

	for ( ; n_pkts_tx < n_pkts; n_pkts_tx++) {
		struct rte_mbuf *m = pkts[n_pkts_tx];

		n_bytes_drop += m->pkt_len;
		rte_pktmbuf_free(m);
	}

	/* Port update. */
	p->stats.n_pkts = n_pkts_total + n_pkts - n_pkts_drop;
	p->stats.n_bytes = n_bytes_total + n_bytes - n_bytes_drop;
	p->stats.n_pkts_drop = n_pkts_drop_total + n_pkts_drop;
	p->stats.n_bytes_drop = n_bytes_drop_total + n_bytes_drop;
	p->n_pkts = 0;
	p->n_bytes = 0;
	p->flush_flag = 0;

	TRACE("[Ring %s] Buffered packets flushed: %d out, %d dropped\n",
	      p->params.name,
	      n_pkts - n_pkts_drop,
	      n_pkts_drop);
}

static void
writer_pkt_tx(void *port, struct rte_swx_pkt *pkt)
{
	struct writer *p = port;
	int n_pkts = p->n_pkts;
	uint32_t n_bytes = p->n_bytes;
	struct rte_mbuf *m = pkt->handle;
	uint32_t pkt_length = pkt->length;

	TRACE("[Ring %s] Pkt %d (%u bytes at offset %u)\n",
	      p->params.name,
	      p->n_pkts - 1,
	      pkt->length,
	      pkt->offset);
	if (TRACE_LEVEL)
		rte_hexdump(stdout, NULL, &pkt->pkt[pkt->offset], pkt->length);

	m->data_len = (uint16_t)(pkt_length + m->data_len - m->pkt_len);
	m->pkt_len = pkt_length;
	m->data_off = (uint16_t)pkt->offset;

	p->pkts[n_pkts++] = m;
	p->n_pkts = n_pkts;
	p->n_bytes = n_bytes + pkt_length;

	if (n_pkts == (int)p->params.burst_size)
		__writer_flush(p);
}

static void
writer_pkt_fast_clone_tx(void *port, struct rte_swx_pkt *pkt)
{
	struct writer *p = port;
	int n_pkts = p->n_pkts;
	uint32_t n_bytes = p->n_bytes;
	uint64_t n_pkts_clone = p->stats.n_pkts_clone;
	struct rte_mbuf *m = pkt->handle;
	uint32_t pkt_length = pkt->length;

	TRACE("[Ring %s] Pkt %d (%u bytes at offset %u) (fast clone)\n",
	      p->params.name,
	      p->n_pkts - 1,
	      pkt->length,
	      pkt->offset);
	if (TRACE_LEVEL)
		rte_hexdump(stdout, NULL, &pkt->pkt[pkt->offset], pkt->length);

	m->data_len = (uint16_t)(pkt_length + m->data_len - m->pkt_len);
	m->pkt_len = pkt_length;
	m->data_off = (uint16_t)pkt->offset;
	rte_pktmbuf_refcnt_update(m, 1);

	p->pkts[n_pkts++] = m;
	p->n_pkts = n_pkts;
	p->n_bytes = n_bytes + pkt_length;
	p->stats.n_pkts_clone = n_pkts_clone + 1;

	if (n_pkts == (int)p->params.burst_size)
		__writer_flush(p);
}

static void
writer_pkt_clone_tx(void *port, struct rte_swx_pkt *pkt, uint32_t truncation_length)
{
	struct writer *p = port;
	int n_pkts = p->n_pkts;
	uint32_t n_bytes = p->n_bytes;
	uint64_t n_pkts_clone = p->stats.n_pkts_clone;
	struct rte_mbuf *m = pkt->handle, *m_clone;
	uint32_t pkt_length = pkt->length;

	TRACE("[Ring %s] Pkt %d (%u bytes at offset %u) (clone)\n",
	      p->params.name,
	      p->n_pkts - 1,
	      pkt->length,
	      pkt->offset);
	if (TRACE_LEVEL)
		rte_hexdump(stdout, NULL, &pkt->pkt[pkt->offset], pkt->length);

	m->data_len = (uint16_t)(pkt_length + m->data_len - m->pkt_len);
	m->pkt_len = pkt_length;
	m->data_off = (uint16_t)pkt->offset;

	m_clone = rte_pktmbuf_copy(m, m->pool, 0, truncation_length);
	if (!m_clone) {
		p->stats.n_pkts_clone_err++;
		return;
	}

	p->pkts[n_pkts++] = m_clone;
	p->n_pkts = n_pkts;
	p->n_bytes = n_bytes + pkt_length;
	p->stats.n_pkts_clone = n_pkts_clone + 1;

	if (n_pkts == (int)p->params.burst_size)
		__writer_flush(p);
}

static void
writer_flush(void *port)
{
	struct writer *p = port;

	if (p->n_pkts && p->flush_flag)
		__writer_flush(p);

	p->flush_flag = 1;
}

static void
writer_free(void *port)
{
	struct writer *p = port;
	int i;

	if (!p)
		return;

	for (i = 0; i < p->n_pkts; i++) {
		struct rte_mbuf *m = p->pkts[i];

		rte_pktmbuf_free(m);
	}

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
	.pkt_fast_clone_tx = writer_pkt_fast_clone_tx,
	.pkt_clone_tx = writer_pkt_clone_tx,
	.flush = writer_flush,
	.stats_read = writer_stats_read,
};
