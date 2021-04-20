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

static void
__writer_flush(struct writer *p)
{
	int n_pkts;

	for (n_pkts = 0; ; ) {
		n_pkts += rte_eth_tx_burst(p->params.port_id,
					   p->params.queue_id,
					   p->pkts + n_pkts,
					   p->n_pkts - n_pkts);

		TRACE("[Ethdev TX port %u queue %u] %d packets out\n",
		      (uint32_t)p->params.port_id,
		      (uint32_t)p->params.queue_id,
		      n_pkts);

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

	TRACE("[Ethdev TX port %u queue %u] Pkt %d (%u bytes at offset %u)\n",
	      (uint32_t)p->params.port_id,
	      (uint32_t)p->params.queue_id,
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
	.flush = writer_flush,
	.stats_read = writer_stats_read,
};
