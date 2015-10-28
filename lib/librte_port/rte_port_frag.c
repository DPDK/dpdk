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
#include <rte_memory.h>

#include "rte_port_frag.h"

/* Max number of fragments per packet allowed */
#define	RTE_PORT_FRAG_MAX_FRAGS_PER_PACKET 0x80

#ifdef RTE_PORT_STATS_COLLECT

#define RTE_PORT_RING_READER_FRAG_STATS_PKTS_IN_ADD(port, val) \
	port->stats.n_pkts_in += val
#define RTE_PORT_RING_READER_FRAG_STATS_PKTS_DROP_ADD(port, val) \
	port->stats.n_pkts_drop += val

#else

#define RTE_PORT_RING_READER_FRAG_STATS_PKTS_IN_ADD(port, val)
#define RTE_PORT_RING_READER_FRAG_STATS_PKTS_DROP_ADD(port, val)

#endif

typedef int32_t
		(*frag_op)(struct rte_mbuf *pkt_in,
			struct rte_mbuf **pkts_out,
			uint16_t nb_pkts_out,
			uint16_t mtu_size,
			struct rte_mempool *pool_direct,
			struct rte_mempool *pool_indirect);

struct rte_port_ring_reader_frag {
	struct rte_port_in_stats stats;

	/* Input parameters */
	struct rte_ring *ring;
	uint32_t mtu;
	uint32_t metadata_size;
	struct rte_mempool *pool_direct;
	struct rte_mempool *pool_indirect;

	/* Internal buffers */
	struct rte_mbuf *pkts[RTE_PORT_IN_BURST_SIZE_MAX];
	struct rte_mbuf *frags[RTE_PORT_FRAG_MAX_FRAGS_PER_PACKET];
	uint32_t n_pkts;
	uint32_t pos_pkts;
	uint32_t n_frags;
	uint32_t pos_frags;

	frag_op f_frag;
} __rte_cache_aligned;

static void *
rte_port_ring_reader_frag_create(void *params, int socket_id, int is_ipv4)
{
	struct rte_port_ring_reader_frag_params *conf =
			(struct rte_port_ring_reader_frag_params *) params;
	struct rte_port_ring_reader_frag *port;

	/* Check input parameters */
	if (conf == NULL) {
		RTE_LOG(ERR, PORT, "%s: Parameter conf is NULL\n", __func__);
		return NULL;
	}
	if (conf->ring == NULL) {
		RTE_LOG(ERR, PORT, "%s: Parameter ring is NULL\n", __func__);
		return NULL;
	}
	if (conf->mtu == 0) {
		RTE_LOG(ERR, PORT, "%s: Parameter mtu is invalid\n", __func__);
		return NULL;
	}
	if (conf->pool_direct == NULL) {
		RTE_LOG(ERR, PORT, "%s: Parameter pool_direct is NULL\n",
			__func__);
		return NULL;
	}
	if (conf->pool_indirect == NULL) {
		RTE_LOG(ERR, PORT, "%s: Parameter pool_indirect is NULL\n",
			__func__);
		return NULL;
	}

	/* Memory allocation */
	port = rte_zmalloc_socket("PORT", sizeof(*port), RTE_CACHE_LINE_SIZE,
		socket_id);
	if (port == NULL) {
		RTE_LOG(ERR, PORT, "%s: port is NULL\n", __func__);
		return NULL;
	}

	/* Initialization */
	port->ring = conf->ring;
	port->mtu = conf->mtu;
	port->metadata_size = conf->metadata_size;
	port->pool_direct = conf->pool_direct;
	port->pool_indirect = conf->pool_indirect;

	port->n_pkts = 0;
	port->pos_pkts = 0;
	port->n_frags = 0;
	port->pos_frags = 0;

	port->f_frag = (is_ipv4) ?
			rte_ipv4_fragment_packet : rte_ipv6_fragment_packet;

	return port;
}

static void *
rte_port_ring_reader_ipv4_frag_create(void *params, int socket_id)
{
	return rte_port_ring_reader_frag_create(params, socket_id, 1);
}

static void *
rte_port_ring_reader_ipv6_frag_create(void *params, int socket_id)
{
	return rte_port_ring_reader_frag_create(params, socket_id, 0);
}

static int
rte_port_ring_reader_frag_rx(void *port,
		struct rte_mbuf **pkts,
		uint32_t n_pkts)
{
	struct rte_port_ring_reader_frag *p =
			(struct rte_port_ring_reader_frag *) port;
	uint32_t n_pkts_out;

	n_pkts_out = 0;

	/* Get packets from the "frag" buffer */
	if (p->n_frags >= n_pkts) {
		memcpy(pkts, &p->frags[p->pos_frags], n_pkts * sizeof(void *));
		p->pos_frags += n_pkts;
		p->n_frags -= n_pkts;

		return n_pkts;
	}

	memcpy(pkts, &p->frags[p->pos_frags], p->n_frags * sizeof(void *));
	n_pkts_out = p->n_frags;
	p->n_frags = 0;

	/* Look to "pkts" buffer to get more packets */
	for ( ; ; ) {
		struct rte_mbuf *pkt;
		uint32_t n_pkts_to_provide, i;
		int status;

		/* If "pkts" buffer is empty, read packet burst from ring */
		if (p->n_pkts == 0) {
			p->n_pkts = rte_ring_sc_dequeue_burst(p->ring,
				(void **) p->pkts, RTE_PORT_IN_BURST_SIZE_MAX);
			RTE_PORT_RING_READER_FRAG_STATS_PKTS_IN_ADD(p, p->n_pkts);
			if (p->n_pkts == 0)
				return n_pkts_out;
			p->pos_pkts = 0;
		}

		/* Read next packet from "pkts" buffer */
		pkt = p->pkts[p->pos_pkts++];
		p->n_pkts--;

		/* If not jumbo, pass current packet to output */
		if (pkt->pkt_len <= p->mtu) {
			pkts[n_pkts_out++] = pkt;

			n_pkts_to_provide = n_pkts - n_pkts_out;
			if (n_pkts_to_provide == 0)
				return n_pkts;

			continue;
		}

		/* Fragment current packet into the "frags" buffer */
		status = p->f_frag(
			pkt,
			p->frags,
			RTE_PORT_FRAG_MAX_FRAGS_PER_PACKET,
			p->mtu,
			p->pool_direct,
			p->pool_indirect
		);

		if (status < 0) {
			rte_pktmbuf_free(pkt);
			RTE_PORT_RING_READER_FRAG_STATS_PKTS_DROP_ADD(p, 1);
			continue;
		}

		p->n_frags = (uint32_t) status;
		p->pos_frags = 0;

		/* Copy meta-data from input jumbo packet to its fragments */
		for (i = 0; i < p->n_frags; i++) {
			uint8_t *src =
			  RTE_MBUF_METADATA_UINT8_PTR(pkt, sizeof(struct rte_mbuf));
			uint8_t *dst =
			  RTE_MBUF_METADATA_UINT8_PTR(p->frags[i], sizeof(struct rte_mbuf));

			memcpy(dst, src, p->metadata_size);
		}

		/* Free input jumbo packet */
		rte_pktmbuf_free(pkt);

		/* Get packets from "frag" buffer */
		n_pkts_to_provide = n_pkts - n_pkts_out;
		if (p->n_frags >= n_pkts_to_provide) {
			memcpy(&pkts[n_pkts_out], p->frags,
				n_pkts_to_provide * sizeof(void *));
			p->n_frags -= n_pkts_to_provide;
			p->pos_frags += n_pkts_to_provide;

			return n_pkts;
		}

		memcpy(&pkts[n_pkts_out], p->frags,
			p->n_frags * sizeof(void *));
		n_pkts_out += p->n_frags;
		p->n_frags = 0;
	}
}

static int
rte_port_ring_reader_frag_free(void *port)
{
	if (port == NULL) {
		RTE_LOG(ERR, PORT, "%s: Parameter port is NULL\n", __func__);
		return -1;
	}

	rte_free(port);

	return 0;
}

static int
rte_port_frag_reader_stats_read(void *port,
		struct rte_port_in_stats *stats, int clear)
{
	struct rte_port_ring_reader_frag *p =
		(struct rte_port_ring_reader_frag *) port;

	if (stats != NULL)
		memcpy(stats, &p->stats, sizeof(p->stats));

	if (clear)
		memset(&p->stats, 0, sizeof(p->stats));

	return 0;
}

/*
 * Summary of port operations
 */
struct rte_port_in_ops rte_port_ring_reader_ipv4_frag_ops = {
	.f_create = rte_port_ring_reader_ipv4_frag_create,
	.f_free = rte_port_ring_reader_frag_free,
	.f_rx = rte_port_ring_reader_frag_rx,
	.f_stats = rte_port_frag_reader_stats_read,
};

struct rte_port_in_ops rte_port_ring_reader_ipv6_frag_ops = {
	.f_create = rte_port_ring_reader_ipv6_frag_create,
	.f_free = rte_port_ring_reader_frag_free,
	.f_rx = rte_port_ring_reader_frag_rx,
	.f_stats = rte_port_frag_reader_stats_read,
};
