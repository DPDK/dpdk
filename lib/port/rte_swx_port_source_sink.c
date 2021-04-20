/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef RTE_PORT_PCAP
#include <pcap.h>
#endif
#include <sys/time.h>

#include <rte_common.h>
#include <rte_mbuf.h>
#include <rte_hexdump.h>

#include "rte_swx_port_source_sink.h"

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
 * Port SOURCE
 */
#ifdef RTE_PORT_PCAP

struct source {
	struct {
		struct rte_mempool *pool;
	} params;
	struct rte_swx_port_in_stats stats;
	struct rte_mbuf **pkts;
	uint32_t n_pkts;
	uint32_t pos;
};

static void
source_free(void *port)
{
	struct source *p = port;
	uint32_t i;

	if (!p)
		return;

	for (i = 0; i < p->n_pkts; i++)
		rte_pktmbuf_free(p->pkts[i]);

	free(p->pkts);

	free(p);
}

static void *
source_create(void *args)
{
	char pcap_errbuf[PCAP_ERRBUF_SIZE];
	struct rte_swx_port_source_params *params = args;
	struct source *p = NULL;
	pcap_t *f = NULL;
	uint32_t n_pkts_max, i;

	/* Check input arguments. */
	CHECK(params);
	CHECK(params->pool);
	CHECK(params->file_name && params->file_name[0]);
	n_pkts_max = params->n_pkts_max ?
		params->n_pkts_max :
		RTE_SWX_PORT_SOURCE_PKTS_MAX;

	/* Resource allocation. */
	f = pcap_open_offline(params->file_name, pcap_errbuf);
	if (!f)
		goto error;

	p = calloc(1, sizeof(struct source));
	if (!p)
		goto error;

	p->pkts = calloc(n_pkts_max, sizeof(struct rte_mbuf *));
	if (!p->pkts)
		goto error;

	/* Initialization. */
	p->params.pool = params->pool;

	/* PCAP file. */
	for (i = 0; i < n_pkts_max; i++) {
		struct pcap_pkthdr pcap_pkthdr;
		const uint8_t *pcap_pktdata;
		struct rte_mbuf *m;
		uint8_t *m_data;

		/* Read new packet from PCAP file. */
		pcap_pktdata = pcap_next(f, &pcap_pkthdr);
		if (!pcap_pktdata)
			break;

		/* Allocate new buffer from pool. */
		m = rte_pktmbuf_alloc(params->pool);
		if (!m)
			goto error;
		m_data = rte_pktmbuf_mtod(m, uint8_t *);

		rte_memcpy(m_data, pcap_pktdata, pcap_pkthdr.caplen);
		m->data_len = pcap_pkthdr.caplen;
		m->pkt_len = pcap_pkthdr.caplen;

		p->pkts[p->n_pkts] = m;
		p->n_pkts++;
	}

	if (!p->n_pkts)
		goto error;

	pcap_close(f);
	return p;

error:
	source_free(p);
	if (f)
		pcap_close(f);
	return NULL;
}

static int
source_pkt_rx(void *port, struct rte_swx_pkt *pkt)
{
	struct source *p = port;
	struct rte_mbuf *m_dst, *m_src;
	uint8_t *m_dst_data, *m_src_data;

	/* m_src identification. */
	m_src = p->pkts[p->pos];
	m_src_data = rte_pktmbuf_mtod(m_src, uint8_t *);

	/* m_dst allocation from pool. */
	m_dst = rte_pktmbuf_alloc(p->params.pool);
	if (!m_dst)
		return 0;

	/* m_dst initialization. */
	m_dst->data_len = m_src->data_len;
	m_dst->pkt_len = m_src->pkt_len;
	m_dst->data_off = m_src->data_off;

	m_dst_data = rte_pktmbuf_mtod(m_dst, uint8_t *);
	rte_memcpy(m_dst_data, m_src_data, m_src->data_len);

	/* pkt initialization. */
	pkt->handle = m_dst;
	pkt->pkt = m_dst->buf_addr;
	pkt->offset = m_dst->data_off;
	pkt->length = m_dst->pkt_len;

	TRACE("[Source port] Pkt RX (%u bytes at offset %u)\n",
	      pkt->length,
	      pkt->offset);
	if (TRACE_LEVEL)
		rte_hexdump(stdout, NULL, &pkt->pkt[pkt->offset], pkt->length);

	/* port stats update. */
	p->stats.n_pkts++;
	p->stats.n_bytes += pkt->length;

	/* m_src next. */
	p->pos++;
	if (p->pos == p->n_pkts)
		p->pos = 0;

	return 1;
}

static void
source_stats_read(void *port, struct rte_swx_port_in_stats *stats)
{
	struct source *p = port;

	if (!p || !stats)
		return;

	memcpy(stats, &p->stats, sizeof(p->stats));
}

struct rte_swx_port_in_ops rte_swx_port_source_ops = {
	.create = source_create,
	.free = source_free,
	.pkt_rx = source_pkt_rx,
	.stats_read = source_stats_read,
};

#else

struct rte_swx_port_in_ops rte_swx_port_source_ops = {
	.create = NULL,
	.free = NULL,
	.pkt_rx = NULL,
	.stats_read = NULL,
};

#endif

/*
 * Port SINK
 */
struct sink {
	struct rte_swx_port_out_stats stats;

#ifdef RTE_PORT_PCAP
	pcap_t *f_pcap;
	pcap_dumper_t *f_dump;
#endif
};

static void
sink_free(void *port)
{
	struct sink *p = port;

	if (!p)
		return;

#ifdef RTE_PORT_PCAP
	if (p->f_dump)
		pcap_dump_close(p->f_dump);
	if (p->f_pcap)
		pcap_close(p->f_pcap);
#endif

	free(p);
}

static void *
sink_create(void *args __rte_unused)
{
	struct sink *p;

	/* Memory allocation. */
	p = calloc(1, sizeof(struct sink));
	if (!p)
		goto error;

#ifdef RTE_PORT_PCAP
	if (args) {
		struct rte_swx_port_sink_params *params = args;

		if (params->file_name && params->file_name[0]) {
			p->f_pcap = pcap_open_dead(DLT_EN10MB, 65535);
			if (!p->f_pcap)
				goto error;

			p->f_dump = pcap_dump_open(p->f_pcap,
						   params->file_name);
			if (!p->f_dump)
				goto error;
		}
	}
#endif

	return p;

error:
	sink_free(p);
	return NULL;
}

static void
sink_pkt_tx(void *port, struct rte_swx_pkt *pkt)
{
	struct sink *p = port;
	struct rte_mbuf *m = pkt->handle;

	TRACE("[Sink port] Pkt TX (%u bytes at offset %u)\n",
	      pkt->length,
	      pkt->offset);
	if (TRACE_LEVEL)
		rte_hexdump(stdout, NULL, &pkt->pkt[pkt->offset], pkt->length);

	m->pkt_len = pkt->length;
	m->data_len = (uint16_t)pkt->length;
	m->data_off = (uint16_t)pkt->offset;

	p->stats.n_pkts++;
	p->stats.n_bytes += pkt->length;

#ifdef RTE_PORT_PCAP
	if (p->f_dump) {
		struct pcap_pkthdr pcap_pkthdr;
		uint8_t *m_data = rte_pktmbuf_mtod(m, uint8_t *);

		pcap_pkthdr.len = m->pkt_len;
		pcap_pkthdr.caplen = m->data_len;
		gettimeofday(&pcap_pkthdr.ts, NULL);

		pcap_dump((uint8_t *)p->f_dump, &pcap_pkthdr, m_data);
		pcap_dump_flush(p->f_dump);
	}
#endif

	rte_pktmbuf_free(m);
}

static void
sink_stats_read(void *port, struct rte_swx_port_out_stats *stats)
{
	struct sink *p = port;

	if (!p || !stats)
		return;

	memcpy(stats, &p->stats, sizeof(p->stats));
}

/*
 * Summary of port operations
 */
struct rte_swx_port_out_ops rte_swx_port_sink_ops = {
	.create = sink_create,
	.free = sink_free,
	.pkt_tx = sink_pkt_tx,
	.flush = NULL,
	.stats_read = sink_stats_read,
};
