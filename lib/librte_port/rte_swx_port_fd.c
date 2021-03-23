/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_hexdump.h>

#include "rte_swx_port_fd.h"

#ifndef TRACE_LEVEL
#define TRACE_LEVEL 0
#endif

#if TRACE_LEVEL
#define TRACE(...) printf(__VA_ARGS__)
#else
#define TRACE(...)
#endif

/*
 * FD Reader
 */
struct reader {
	struct {
		int fd;
		uint32_t mtu;
		uint32_t burst_size;
		struct rte_mempool *mempool;
	} params;

	struct rte_swx_port_in_stats stats;
	struct rte_mbuf **pkts;
	uint32_t n_pkts;
	uint32_t pos;
};

static void *
reader_create(void *args)
{
	struct rte_swx_port_fd_reader_params *conf = args;
	struct reader *p;

	/* Check input parameters. */
	if (!conf || conf->fd < 0 || conf->mtu == 0 || !conf->mempool)
		return NULL;

	/* Memory allocation. */
	p = calloc(1, sizeof(struct reader));
	if (!p)
		return NULL;

	p->pkts = calloc(conf->burst_size, sizeof(struct rte_mbuf *));
	if (!p->pkts) {
		free(p);
		return NULL;
	}

	/* Initialization. */
	p->params.fd = conf->fd;
	p->params.mtu = conf->mtu;
	p->params.burst_size = conf->burst_size;
	p->params.mempool = conf->mempool;

	return p;
}

static void
reader_free(void *port)
{
	struct reader *p = port;
	uint32_t i;

	if (!p)
		return;

	for (i = 0; i < p->n_pkts; i++)
		rte_pktmbuf_free(p->pkts[i]);

	free(p->pkts);
	free(p);
}

static int
reader_pkt_rx(void *port, struct rte_swx_pkt *pkt)
{
	struct reader *p = port;
	struct rte_mbuf *m;
	void *pkt_data;
	ssize_t n_bytes;
	uint32_t i, j;

	if (p->n_pkts == p->pos) {
		if (rte_pktmbuf_alloc_bulk(p->params.mempool, p->pkts, p->params.burst_size) != 0)
			return 0;

		for (i = 0; i < p->params.burst_size; i++) {
			m = p->pkts[i];
			pkt_data = rte_pktmbuf_mtod(m, void *);
			n_bytes = read(p->params.fd, pkt_data, (size_t) p->params.mtu);

			if (n_bytes <= 0)
				break;

			m->data_len = n_bytes;
			m->pkt_len = n_bytes;

			p->stats.n_pkts++;
			p->stats.n_bytes += n_bytes;
		}

		for (j = i; j < p->params.burst_size; j++)
			rte_pktmbuf_free(p->pkts[j]);

		p->n_pkts = i;
		p->pos = 0;

		if (!p->n_pkts)
			return 0;
	}

	m = p->pkts[p->pos++];
	pkt->handle = m;
	pkt->pkt = m->buf_addr;
	pkt->offset = m->data_off;
	pkt->length = m->pkt_len;

	TRACE("[FD %u] Pkt %d (%u bytes at offset %u)\n",
		(uint32_t)p->params.fd,
		p->pos - 1,
		pkt->length,
		pkt->offset);

	if (TRACE_LEVEL)
		rte_hexdump(stdout, NULL,
			&((uint8_t *)m->buf_addr)[m->data_off], m->data_len);

	return 1;
}

static void
reader_stats_read(void *port, struct rte_swx_port_in_stats *stats)
{
	struct reader *p = port;

	memcpy(stats, &p->stats, sizeof(p->stats));
}

/*
 * FD Writer
 */
struct writer {
	struct {
		int fd;
		uint32_t mtu;
		uint32_t burst_size;
		struct rte_mempool *mempool;
	} params;

	struct rte_swx_port_out_stats stats;
	struct rte_mbuf **pkts;
	uint32_t n_pkts;
};

static void *
writer_create(void *args)
{
	struct rte_swx_port_fd_writer_params *conf = args;
	struct writer *p;

	/* Check input parameters. */
	if (!conf)
		return NULL;

	/* Memory allocation. */
	p = calloc(1, sizeof(struct writer));
	if (!p)
		return NULL;


	p->pkts = calloc(conf->burst_size, sizeof(struct rte_mbuf *));
	if (!p->pkts) {
		free(p);
		return NULL;
	}

	/* Initialization. */
	p->params.fd = conf->fd;
	p->params.burst_size = conf->burst_size;

	return p;
}

static void
__writer_flush(struct writer *p)
{
	struct rte_mbuf *pkt;
	void *pkt_data;
	size_t n_bytes;
	ssize_t ret;
	uint32_t i;

	for (i = 0; i < p->n_pkts; i++) {
		pkt = p->pkts[i];
		pkt_data = rte_pktmbuf_mtod(pkt, void*);
		n_bytes = rte_pktmbuf_data_len(pkt);

		ret = write(p->params.fd, pkt_data, n_bytes);
		if (ret < 0)
			break;
	}

	TRACE("[FD %u] %u packets out\n",
		(uint32_t)p->params.fd,
		p->n_pkts);

	for (i = 0; i < p->n_pkts; i++)
		rte_pktmbuf_free(p->pkts[i]);

	p->n_pkts = 0;
}

static void
writer_pkt_tx(void *port, struct rte_swx_pkt *pkt)
{
	struct writer *p = port;
	struct rte_mbuf *m = pkt->handle;

	TRACE("[FD %u] Pkt %u (%u bytes at offset %u)\n",
		(uint32_t)p->params.fd,
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
	if (p->n_pkts == p->params.burst_size)
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
	free(p);
}

static void
writer_stats_read(void *port, struct rte_swx_port_out_stats *stats)
{
	struct writer *p = port;

	memcpy(stats, &p->stats, sizeof(p->stats));
}

/*
 * Summary of port operations
 */
struct rte_swx_port_in_ops rte_swx_port_fd_reader_ops = {
	.create = reader_create,
	.free = reader_free,
	.pkt_rx = reader_pkt_rx,
	.stats_read = reader_stats_read,
};

struct rte_swx_port_out_ops rte_swx_port_fd_writer_ops = {
	.create = writer_create,
	.free = writer_free,
	.pkt_tx = writer_pkt_tx,
	.flush = writer_flush,
	.stats_read = writer_stats_read,
};
