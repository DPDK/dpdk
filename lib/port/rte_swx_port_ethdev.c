/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_hexdump.h>

#include "rte_swx_port_ethdev.h"

#define CHECK(condition)                                                       \
do {                                                                           \
	if (!(condition))                                                      \
		return NULL;                                                   \
} while (0)

#ifndef TRACE_LEVEL
#define TRACE_LEVEL 0
#endif

#if TRACE_LEVEL
#define TRACE(...) printf(__VA_ARGS__)
#else
#define TRACE(...)
#endif

/*
 * Port ETHDEV Reader
 */
struct reader {
	struct {
		uint16_t port_id;
		uint16_t queue_id;
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
	struct rte_eth_dev_info info;
	struct rte_swx_port_ethdev_reader_params *params = args;
	struct reader *p;
	int status;
	uint16_t port_id;

	/* Check input parameters. */
	CHECK(params);

	CHECK(params->dev_name);
	status = rte_eth_dev_get_port_by_name(params->dev_name, &port_id);
	CHECK(!status);

	status = rte_eth_dev_info_get(port_id, &info);
	CHECK((status == -ENOTSUP) || (params->queue_id < info.nb_rx_queues));

	CHECK(params->burst_size);

	/* Memory allocation. */
	p = calloc(1, sizeof(struct reader));
	CHECK(p);

	p->pkts = calloc(params->burst_size, sizeof(struct rte_mbuf *));
	if (!p->pkts) {
		free(p);
		CHECK(0);
	}

	/* Initialization. */
	p->params.port_id = port_id;
	p->params.queue_id = params->queue_id;
	p->params.burst_size = params->burst_size;

	return p;
}

static int
reader_pkt_rx(void *port, struct rte_swx_pkt *pkt)
{
	struct reader *p = port;
	struct rte_mbuf *m;

	if (p->pos == p->n_pkts) {
		int n_pkts;

		n_pkts = rte_eth_rx_burst(p->params.port_id,
					  p->params.queue_id,
					  p->pkts,
					  p->params.burst_size);
		if (!n_pkts) {
			p->stats.n_empty++;
			return 0;
		}

		TRACE("[Ethdev RX port %u queue %u] %d packets in\n",
		      (uint32_t)p->params.port_id,
		      (uint32_t)p->params.queue_id,
		      n_pkts);

		p->n_pkts = n_pkts;
		p->pos = 0;
	}

	m = p->pkts[p->pos++];
	pkt->handle = m;
	pkt->pkt = m->buf_addr;
	pkt->offset = m->data_off;
	pkt->length = m->pkt_len;

	TRACE("[Ethdev RX port %u queue %u] Pkt %d (%u bytes at offset %u)\n",
	      (uint32_t)p->params.port_id,
	      (uint32_t)p->params.queue_id,
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
	free(p);
}

static void
reader_stats_read(void *port, struct rte_swx_port_in_stats *stats)
{
	struct reader *p = port;

	memcpy(stats, &p->stats, sizeof(p->stats));
}

/*
 * Port ETHDEV Writer
 */
struct writer {
	struct {
		uint16_t port_id;
		uint16_t queue_id;
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
	struct rte_eth_dev_info info;
	struct rte_swx_port_ethdev_writer_params *params = args;
	struct writer *p;
	int status;
	uint16_t port_id;

	/* Check input parameters. */
	CHECK(params);

	CHECK(params->dev_name);
	status = rte_eth_dev_get_port_by_name(params->dev_name, &port_id);
	CHECK(!status);

	status = rte_eth_dev_info_get(port_id, &info);
	CHECK((status == -ENOTSUP) || (params->queue_id < info.nb_tx_queues));

	CHECK(params->burst_size);

	/* Memory allocation. */
	p = calloc(1, sizeof(struct writer));
	CHECK(p);

	p->pkts = calloc(params->burst_size, sizeof(struct rte_mbuf *));
	if (!p->pkts) {
		free(p);
		CHECK(0);
	}

	/* Initialization. */
	p->params.port_id = port_id;
	p->params.queue_id = params->queue_id;
	p->params.burst_size = params->burst_size;

	return p;
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
	n_pkts_tx = rte_eth_tx_burst(p->params.port_id,
				     p->params.queue_id,
				     pkts,
				     n_pkts);

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

	TRACE("[Ethdev TX port %u queue %u] Buffered packets flushed: %d out, %d dropped\n",
	      (uint32_t)p->params.port_id,
	      (uint32_t)p->params.queue_id,
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

	TRACE("[Ethdev TX port %u queue %u] Pkt %d (%u bytes at offset %u)\n",
	      (uint32_t)p->params.port_id,
	      (uint32_t)p->params.queue_id,
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

	TRACE("[Ethdev TX port %u queue %u] Pkt %d (%u bytes at offset %u) (fast clone)\n",
	      (uint32_t)p->params.port_id,
	      (uint32_t)p->params.queue_id,
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

	TRACE("[Ethdev TX port %u queue %u] Pkt %d (%u bytes at offset %u) (clone)\n",
	      (uint32_t)p->params.port_id,
	      (uint32_t)p->params.queue_id,
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
	free(port);
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
struct rte_swx_port_in_ops rte_swx_port_ethdev_reader_ops = {
	.create = reader_create,
	.free = reader_free,
	.pkt_rx = reader_pkt_rx,
	.stats_read = reader_stats_read,
};

struct rte_swx_port_out_ops rte_swx_port_ethdev_writer_ops = {
	.create = writer_create,
	.free = writer_free,
	.pkt_tx = writer_pkt_tx,
	.pkt_fast_clone_tx = writer_pkt_fast_clone_tx,
	.pkt_clone_tx = writer_pkt_clone_tx,
	.flush = writer_flush,
	.stats_read = writer_stats_read,
};
