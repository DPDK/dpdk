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

#include <rte_ether.h>
#include <rte_ip_frag.h>
#include <rte_cycles.h>
#include <rte_log.h>

#include "rte_port_ras.h"

#ifndef IPV4_RAS_N_BUCKETS
#define IPV4_RAS_N_BUCKETS                                 4094
#endif

#ifndef IPV4_RAS_N_ENTRIES_PER_BUCKET
#define IPV4_RAS_N_ENTRIES_PER_BUCKET                      8
#endif

#ifndef IPV4_RAS_N_ENTRIES
#define IPV4_RAS_N_ENTRIES (IPV4_RAS_N_BUCKETS * IPV4_RAS_N_ENTRIES_PER_BUCKET)
#endif

struct rte_port_ring_writer_ipv4_ras {
	struct rte_mbuf *tx_buf[RTE_PORT_IN_BURST_SIZE_MAX];
	struct rte_ring *ring;
	uint32_t tx_burst_sz;
	uint32_t tx_buf_count;
	struct rte_ip_frag_tbl *frag_tbl;
	struct rte_ip_frag_death_row death_row;
};

static void *
rte_port_ring_writer_ipv4_ras_create(void *params, int socket_id)
{
	struct rte_port_ring_writer_ipv4_ras_params *conf =
			(struct rte_port_ring_writer_ipv4_ras_params *) params;
	struct rte_port_ring_writer_ipv4_ras *port;
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
			CACHE_LINE_SIZE, socket_id);
	if (port == NULL) {
		RTE_LOG(ERR, PORT, "%s: Failed to allocate socket\n", __func__);
		return NULL;
	}

	/* Create fragmentation table */
	frag_cycles = (rte_get_tsc_hz() + MS_PER_S - 1) / MS_PER_S * MS_PER_S;
	frag_cycles *= 100;

	port->frag_tbl = rte_ip_frag_table_create(
		IPV4_RAS_N_BUCKETS,
		IPV4_RAS_N_ENTRIES_PER_BUCKET,
		IPV4_RAS_N_ENTRIES,
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

	return port;
}

static inline void
send_burst(struct rte_port_ring_writer_ipv4_ras *p)
{
	uint32_t nb_tx;

	nb_tx = rte_ring_sp_enqueue_burst(p->ring, (void **)p->tx_buf,
			p->tx_buf_count);

	for ( ; nb_tx < p->tx_buf_count; nb_tx++)
		rte_pktmbuf_free(p->tx_buf[nb_tx]);

	p->tx_buf_count = 0;
}

static inline void
process_one(struct rte_port_ring_writer_ipv4_ras *p, struct rte_mbuf *pkt)
{
	/* Assume there is no ethernet header */
	struct ipv4_hdr *pkt_hdr = (struct ipv4_hdr *)
			(rte_pktmbuf_mtod(pkt, unsigned char *));

	/* Get "Do not fragment" flag and fragment offset */
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

		/* Process this fragment */
		mo = rte_ipv4_frag_reassemble_packet(tbl, dr, pkt, rte_rdtsc(), pkt_hdr);
		if (mo != NULL)
			p->tx_buf[p->tx_buf_count++] = mo;

		rte_ip_frag_free_death_row(&p->death_row, 3);
	}
}

static int
rte_port_ring_writer_ipv4_ras_tx(void *port, struct rte_mbuf *pkt)
{
	struct rte_port_ring_writer_ipv4_ras *p =
			(struct rte_port_ring_writer_ipv4_ras *) port;

	process_one(p, pkt);
	if (p->tx_buf_count >= p->tx_burst_sz)
		send_burst(p);

	return 0;
}

static int
rte_port_ring_writer_ipv4_ras_tx_bulk(void *port,
		struct rte_mbuf **pkts,
		uint64_t pkts_mask)
{
	struct rte_port_ring_writer_ipv4_ras *p =
			(struct rte_port_ring_writer_ipv4_ras *) port;

	if ((pkts_mask & (pkts_mask + 1)) == 0) {
		uint64_t n_pkts = __builtin_popcountll(pkts_mask);
		uint32_t i;

		for (i = 0; i < n_pkts; i++) {
			struct rte_mbuf *pkt = pkts[i];

			process_one(p, pkt);
			if (p->tx_buf_count >= p->tx_burst_sz)
				send_burst(p);
		}
	} else {
		for ( ; pkts_mask; ) {
			uint32_t pkt_index = __builtin_ctzll(pkts_mask);
			uint64_t pkt_mask = 1LLU << pkt_index;
			struct rte_mbuf *pkt = pkts[pkt_index];

			process_one(p, pkt);
			if (p->tx_buf_count >= p->tx_burst_sz)
				send_burst(p);

			pkts_mask &= ~pkt_mask;
		}
	}

	return 0;
}

static int
rte_port_ring_writer_ipv4_ras_flush(void *port)
{
	struct rte_port_ring_writer_ipv4_ras *p =
			(struct rte_port_ring_writer_ipv4_ras *) port;

	if (p->tx_buf_count > 0)
		send_burst(p);

	return 0;
}

static int
rte_port_ring_writer_ipv4_ras_free(void *port)
{
	struct rte_port_ring_writer_ipv4_ras *p =
			(struct rte_port_ring_writer_ipv4_ras *) port;

	if (port == NULL) {
		RTE_LOG(ERR, PORT, "%s: Parameter port is NULL\n", __func__);
		return -1;
	}

	rte_port_ring_writer_ipv4_ras_flush(port);
	rte_ip_frag_table_destroy(p->frag_tbl);
	rte_free(port);

	return 0;
}

/*
 * Summary of port operations
 */
struct rte_port_out_ops rte_port_ring_writer_ipv4_ras_ops = {
	.f_create = rte_port_ring_writer_ipv4_ras_create,
	.f_free = rte_port_ring_writer_ipv4_ras_free,
	.f_tx = rte_port_ring_writer_ipv4_ras_tx,
	.f_tx_bulk = rte_port_ring_writer_ipv4_ras_tx_bulk,
	.f_flush = rte_port_ring_writer_ipv4_ras_flush,
};
