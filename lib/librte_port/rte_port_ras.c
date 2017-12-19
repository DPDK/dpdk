/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */
#include <string.h>

#include <rte_ether.h>
#include <rte_ip_frag.h>
#include <rte_cycles.h>
#include <rte_log.h>

#include "rte_port_ras.h"

#ifndef RTE_PORT_RAS_N_BUCKETS
#define RTE_PORT_RAS_N_BUCKETS                                 4094
#endif

#ifndef RTE_PORT_RAS_N_ENTRIES_PER_BUCKET
#define RTE_PORT_RAS_N_ENTRIES_PER_BUCKET                      8
#endif

#ifndef RTE_PORT_RAS_N_ENTRIES
#define RTE_PORT_RAS_N_ENTRIES (RTE_PORT_RAS_N_BUCKETS * RTE_PORT_RAS_N_ENTRIES_PER_BUCKET)
#endif

#ifdef RTE_PORT_STATS_COLLECT

#define RTE_PORT_RING_WRITER_RAS_STATS_PKTS_IN_ADD(port, val) \
	port->stats.n_pkts_in += val
#define RTE_PORT_RING_WRITER_RAS_STATS_PKTS_DROP_ADD(port, val) \
	port->stats.n_pkts_drop += val

#else

#define RTE_PORT_RING_WRITER_RAS_STATS_PKTS_IN_ADD(port, val)
#define RTE_PORT_RING_WRITER_RAS_STATS_PKTS_DROP_ADD(port, val)

#endif

struct rte_port_ring_writer_ras;

typedef void (*ras_op)(
		struct rte_port_ring_writer_ras *p,
		struct rte_mbuf *pkt);

static void
process_ipv4(struct rte_port_ring_writer_ras *p, struct rte_mbuf *pkt);
static void
process_ipv6(struct rte_port_ring_writer_ras *p, struct rte_mbuf *pkt);

struct rte_port_ring_writer_ras {
	struct rte_port_out_stats stats;

	struct rte_mbuf *tx_buf[RTE_PORT_IN_BURST_SIZE_MAX];
	struct rte_ring *ring;
	uint32_t tx_burst_sz;
	uint32_t tx_buf_count;
	struct rte_ip_frag_tbl *frag_tbl;
	struct rte_ip_frag_death_row death_row;

	ras_op f_ras;
};

static void *
rte_port_ring_writer_ras_create(void *params, int socket_id, int is_ipv4)
{
	struct rte_port_ring_writer_ras_params *conf =
			params;
	struct rte_port_ring_writer_ras *port;
	uint64_t frag_cycles;

	/* Check input parameters */
	if (conf == NULL) {
		RTE_LOG(ERR, PORT, "%s: Parameter conf is NULL\n", __func__);
		return NULL;
	}
	if (conf->ring == NULL) {
		RTE_LOG(ERR, PORT, "%s: Parameter ring is NULL\n", __func__);
		return NULL;
	}
	if ((conf->tx_burst_sz == 0) ||
	    (conf->tx_burst_sz > RTE_PORT_IN_BURST_SIZE_MAX)) {
		RTE_LOG(ERR, PORT, "%s: Parameter tx_burst_sz is invalid\n",
			__func__);
		return NULL;
	}

	/* Memory allocation */
	port = rte_zmalloc_socket("PORT", sizeof(*port),
			RTE_CACHE_LINE_SIZE, socket_id);
	if (port == NULL) {
		RTE_LOG(ERR, PORT, "%s: Failed to allocate socket\n", __func__);
		return NULL;
	}

	/* Create fragmentation table */
	frag_cycles = (rte_get_tsc_hz() + MS_PER_S - 1) / MS_PER_S * MS_PER_S;
	frag_cycles *= 100;

	port->frag_tbl = rte_ip_frag_table_create(
		RTE_PORT_RAS_N_BUCKETS,
		RTE_PORT_RAS_N_ENTRIES_PER_BUCKET,
		RTE_PORT_RAS_N_ENTRIES,
		frag_cycles,
		socket_id);

	if (port->frag_tbl == NULL) {
		RTE_LOG(ERR, PORT, "%s: rte_ip_frag_table_create failed\n",
			__func__);
		rte_free(port);
		return NULL;
	}

	/* Initialization */
	port->ring = conf->ring;
	port->tx_burst_sz = conf->tx_burst_sz;
	port->tx_buf_count = 0;

	port->f_ras = (is_ipv4 == 1) ? process_ipv4 : process_ipv6;

	return port;
}

static void *
rte_port_ring_writer_ipv4_ras_create(void *params, int socket_id)
{
	return rte_port_ring_writer_ras_create(params, socket_id, 1);
}

static void *
rte_port_ring_writer_ipv6_ras_create(void *params, int socket_id)
{
	return rte_port_ring_writer_ras_create(params, socket_id, 0);
}

static inline void
send_burst(struct rte_port_ring_writer_ras *p)
{
	uint32_t nb_tx;

	nb_tx = rte_ring_sp_enqueue_burst(p->ring, (void **)p->tx_buf,
			p->tx_buf_count, NULL);

	RTE_PORT_RING_WRITER_RAS_STATS_PKTS_DROP_ADD(p, p->tx_buf_count - nb_tx);
	for ( ; nb_tx < p->tx_buf_count; nb_tx++)
		rte_pktmbuf_free(p->tx_buf[nb_tx]);

	p->tx_buf_count = 0;
}

static void
process_ipv4(struct rte_port_ring_writer_ras *p, struct rte_mbuf *pkt)
{
	/* Assume there is no ethernet header */
	struct ipv4_hdr *pkt_hdr = rte_pktmbuf_mtod(pkt, struct ipv4_hdr *);

	/* Get "More fragments" flag and fragment offset */
	uint16_t frag_field = rte_be_to_cpu_16(pkt_hdr->fragment_offset);
	uint16_t frag_offset = (uint16_t)(frag_field & IPV4_HDR_OFFSET_MASK);
	uint16_t frag_flag = (uint16_t)(frag_field & IPV4_HDR_MF_FLAG);

	/* If it is a fragmented packet, then try to reassemble */
	if ((frag_flag == 0) && (frag_offset == 0))
		p->tx_buf[p->tx_buf_count++] = pkt;
	else {
		struct rte_mbuf *mo;
		struct rte_ip_frag_tbl *tbl = p->frag_tbl;
		struct rte_ip_frag_death_row *dr = &p->death_row;

		pkt->l3_len = sizeof(*pkt_hdr);

		/* Process this fragment */
		mo = rte_ipv4_frag_reassemble_packet(tbl, dr, pkt, rte_rdtsc(),
				pkt_hdr);
		if (mo != NULL)
			p->tx_buf[p->tx_buf_count++] = mo;

		rte_ip_frag_free_death_row(&p->death_row, 3);
	}
}

static void
process_ipv6(struct rte_port_ring_writer_ras *p, struct rte_mbuf *pkt)
{
	/* Assume there is no ethernet header */
	struct ipv6_hdr *pkt_hdr = rte_pktmbuf_mtod(pkt, struct ipv6_hdr *);

	struct ipv6_extension_fragment *frag_hdr;
	uint16_t frag_data = 0;
	frag_hdr = rte_ipv6_frag_get_ipv6_fragment_header(pkt_hdr);
	if (frag_hdr != NULL)
		frag_data = rte_be_to_cpu_16(frag_hdr->frag_data);

	/* If it is a fragmented packet, then try to reassemble */
	if ((frag_data & RTE_IPV6_FRAG_USED_MASK) == 0)
		p->tx_buf[p->tx_buf_count++] = pkt;
	else {
		struct rte_mbuf *mo;
		struct rte_ip_frag_tbl *tbl = p->frag_tbl;
		struct rte_ip_frag_death_row *dr = &p->death_row;

		pkt->l3_len = sizeof(*pkt_hdr) + sizeof(*frag_hdr);

		/* Process this fragment */
		mo = rte_ipv6_frag_reassemble_packet(tbl, dr, pkt, rte_rdtsc(), pkt_hdr,
				frag_hdr);
		if (mo != NULL)
			p->tx_buf[p->tx_buf_count++] = mo;

		rte_ip_frag_free_death_row(&p->death_row, 3);
	}
}

static int
rte_port_ring_writer_ras_tx(void *port, struct rte_mbuf *pkt)
{
	struct rte_port_ring_writer_ras *p =
			port;

	RTE_PORT_RING_WRITER_RAS_STATS_PKTS_IN_ADD(p, 1);
	p->f_ras(p, pkt);
	if (p->tx_buf_count >= p->tx_burst_sz)
		send_burst(p);

	return 0;
}

static int
rte_port_ring_writer_ras_tx_bulk(void *port,
		struct rte_mbuf **pkts,
		uint64_t pkts_mask)
{
	struct rte_port_ring_writer_ras *p =
			port;

	if ((pkts_mask & (pkts_mask + 1)) == 0) {
		uint64_t n_pkts = __builtin_popcountll(pkts_mask);
		uint32_t i;

		for (i = 0; i < n_pkts; i++) {
			struct rte_mbuf *pkt = pkts[i];

			RTE_PORT_RING_WRITER_RAS_STATS_PKTS_IN_ADD(p, 1);
			p->f_ras(p, pkt);
			if (p->tx_buf_count >= p->tx_burst_sz)
				send_burst(p);
		}
	} else {
		for ( ; pkts_mask; ) {
			uint32_t pkt_index = __builtin_ctzll(pkts_mask);
			uint64_t pkt_mask = 1LLU << pkt_index;
			struct rte_mbuf *pkt = pkts[pkt_index];

			RTE_PORT_RING_WRITER_RAS_STATS_PKTS_IN_ADD(p, 1);
			p->f_ras(p, pkt);
			if (p->tx_buf_count >= p->tx_burst_sz)
				send_burst(p);

			pkts_mask &= ~pkt_mask;
		}
	}

	return 0;
}

static int
rte_port_ring_writer_ras_flush(void *port)
{
	struct rte_port_ring_writer_ras *p =
			port;

	if (p->tx_buf_count > 0)
		send_burst(p);

	return 0;
}

static int
rte_port_ring_writer_ras_free(void *port)
{
	struct rte_port_ring_writer_ras *p =
			port;

	if (port == NULL) {
		RTE_LOG(ERR, PORT, "%s: Parameter port is NULL\n", __func__);
		return -1;
	}

	rte_port_ring_writer_ras_flush(port);
	rte_ip_frag_table_destroy(p->frag_tbl);
	rte_free(port);

	return 0;
}

static int
rte_port_ras_writer_stats_read(void *port,
		struct rte_port_out_stats *stats, int clear)
{
	struct rte_port_ring_writer_ras *p =
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
struct rte_port_out_ops rte_port_ring_writer_ipv4_ras_ops = {
	.f_create = rte_port_ring_writer_ipv4_ras_create,
	.f_free = rte_port_ring_writer_ras_free,
	.f_tx = rte_port_ring_writer_ras_tx,
	.f_tx_bulk = rte_port_ring_writer_ras_tx_bulk,
	.f_flush = rte_port_ring_writer_ras_flush,
	.f_stats = rte_port_ras_writer_stats_read,
};

struct rte_port_out_ops rte_port_ring_writer_ipv6_ras_ops = {
	.f_create = rte_port_ring_writer_ipv6_ras_create,
	.f_free = rte_port_ring_writer_ras_free,
	.f_tx = rte_port_ring_writer_ras_tx,
	.f_tx_bulk = rte_port_ring_writer_ras_tx_bulk,
	.f_flush = rte_port_ring_writer_ras_flush,
	.f_stats = rte_port_ras_writer_stats_read,
};
