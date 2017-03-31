/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016 Intel Corporation. All rights reserved.
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
#include <stdint.h>
#include <unistd.h>

#include <rte_mbuf.h>
#include <rte_malloc.h>

#include "rte_port_fd.h"

/*
 * Port FD Reader
 */
#ifdef RTE_PORT_STATS_COLLECT

#define RTE_PORT_FD_READER_STATS_PKTS_IN_ADD(port, val) \
	do { port->stats.n_pkts_in += val; } while (0)
#define RTE_PORT_FD_READER_STATS_PKTS_DROP_ADD(port, val) \
	do { port->stats.n_pkts_drop += val; } while (0)

#else

#define RTE_PORT_FD_READER_STATS_PKTS_IN_ADD(port, val)
#define RTE_PORT_FD_READER_STATS_PKTS_DROP_ADD(port, val)

#endif

struct rte_port_fd_reader {
	struct rte_port_in_stats stats;
	int fd;
	uint32_t mtu;
	struct rte_mempool *mempool;
};

static void *
rte_port_fd_reader_create(void *params, int socket_id)
{
	struct rte_port_fd_reader_params *conf =
			params;
	struct rte_port_fd_reader *port;

	/* Check input parameters */
	if (conf == NULL) {
		RTE_LOG(ERR, PORT, "%s: params is NULL\n", __func__);
		return NULL;
	}
	if (conf->fd < 0) {
		RTE_LOG(ERR, PORT, "%s: Invalid file descriptor\n", __func__);
		return NULL;
	}
	if (conf->mtu == 0) {
		RTE_LOG(ERR, PORT, "%s: Invalid MTU\n", __func__);
		return NULL;
	}
	if (conf->mempool == NULL) {
		RTE_LOG(ERR, PORT, "%s: Invalid mempool\n", __func__);
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
	port->fd = conf->fd;
	port->mtu = conf->mtu;
	port->mempool = conf->mempool;

	return port;
}

static int
rte_port_fd_reader_rx(void *port, struct rte_mbuf **pkts, uint32_t n_pkts)
{
	struct rte_port_fd_reader *p = port;
	uint32_t i, j;

	if (rte_pktmbuf_alloc_bulk(p->mempool, pkts, n_pkts) != 0)
		return 0;

	for (i = 0; i < n_pkts; i++) {
		struct rte_mbuf *pkt = pkts[i];
		void *pkt_data = rte_pktmbuf_mtod(pkt, void *);
		ssize_t n_bytes;

		n_bytes = read(p->fd, pkt_data, (size_t) p->mtu);
		if (n_bytes <= 0)
			break;

		pkt->data_len = n_bytes;
		pkt->pkt_len = n_bytes;
	}

	for (j = i; j < n_pkts; j++)
		rte_pktmbuf_free(pkts[j]);

	RTE_PORT_FD_READER_STATS_PKTS_IN_ADD(p, i);

	return i;
}

static int
rte_port_fd_reader_free(void *port)
{
	if (port == NULL) {
		RTE_LOG(ERR, PORT, "%s: port is NULL\n", __func__);
		return -EINVAL;
	}

	rte_free(port);

	return 0;
}

static int rte_port_fd_reader_stats_read(void *port,
		struct rte_port_in_stats *stats, int clear)
{
	struct rte_port_fd_reader *p =
			port;

	if (stats != NULL)
		memcpy(stats, &p->stats, sizeof(p->stats));

	if (clear)
		memset(&p->stats, 0, sizeof(p->stats));

	return 0;
}

/*
 * Port FD Writer
 */
#ifdef RTE_PORT_STATS_COLLECT

#define RTE_PORT_FD_WRITER_STATS_PKTS_IN_ADD(port, val) \
	do { port->stats.n_pkts_in += val; } while (0)
#define RTE_PORT_FD_WRITER_STATS_PKTS_DROP_ADD(port, val) \
	do { port->stats.n_pkts_drop += val; } while (0)

#else

#define RTE_PORT_FD_WRITER_STATS_PKTS_IN_ADD(port, val)
#define RTE_PORT_FD_WRITER_STATS_PKTS_DROP_ADD(port, val)

#endif

struct rte_port_fd_writer {
	struct rte_port_out_stats stats;

	struct rte_mbuf *tx_buf[2 * RTE_PORT_IN_BURST_SIZE_MAX];
	uint32_t tx_burst_sz;
	uint16_t tx_buf_count;
	uint32_t fd;
};

static void *
rte_port_fd_writer_create(void *params, int socket_id)
{
	struct rte_port_fd_writer_params *conf =
		params;
	struct rte_port_fd_writer *port;

	/* Check input parameters */
	if ((conf == NULL) ||
		(conf->tx_burst_sz == 0) ||
		(conf->tx_burst_sz > RTE_PORT_IN_BURST_SIZE_MAX) ||
		(!rte_is_power_of_2(conf->tx_burst_sz))) {
		RTE_LOG(ERR, PORT, "%s: Invalid input parameters\n", __func__);
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
	port->fd = conf->fd;
	port->tx_burst_sz = conf->tx_burst_sz;
	port->tx_buf_count = 0;

	return port;
}

static inline void
send_burst(struct rte_port_fd_writer *p)
{
	uint32_t i;

	for (i = 0; i < p->tx_buf_count; i++) {
		struct rte_mbuf *pkt = p->tx_buf[i];
		void *pkt_data = rte_pktmbuf_mtod(pkt, void*);
		size_t n_bytes = rte_pktmbuf_data_len(pkt);
		ssize_t ret;

		ret = write(p->fd, pkt_data, n_bytes);
		if (ret < 0)
			break;
	}

	RTE_PORT_FD_WRITER_STATS_PKTS_DROP_ADD(p, p->tx_buf_count - i);

	for (i = 0; i < p->tx_buf_count; i++)
		rte_pktmbuf_free(p->tx_buf[i]);

	p->tx_buf_count = 0;
}

static int
rte_port_fd_writer_tx(void *port, struct rte_mbuf *pkt)
{
	struct rte_port_fd_writer *p =
		port;

	p->tx_buf[p->tx_buf_count++] = pkt;
	RTE_PORT_FD_WRITER_STATS_PKTS_IN_ADD(p, 1);
	if (p->tx_buf_count >= p->tx_burst_sz)
		send_burst(p);

	return 0;
}

static int
rte_port_fd_writer_tx_bulk(void *port,
	struct rte_mbuf **pkts,
	uint64_t pkts_mask)
{
	struct rte_port_fd_writer *p =
		port;
	uint32_t tx_buf_count = p->tx_buf_count;

	if ((pkts_mask & (pkts_mask + 1)) == 0) {
		uint64_t n_pkts = __builtin_popcountll(pkts_mask);
		uint32_t i;

		for (i = 0; i < n_pkts; i++)
			p->tx_buf[tx_buf_count++] = pkts[i];
		RTE_PORT_FD_WRITER_STATS_PKTS_IN_ADD(p, n_pkts);
	} else
		for ( ; pkts_mask; ) {
			uint32_t pkt_index = __builtin_ctzll(pkts_mask);
			uint64_t pkt_mask = 1LLU << pkt_index;
			struct rte_mbuf *pkt = pkts[pkt_index];

			p->tx_buf[tx_buf_count++] = pkt;
			RTE_PORT_FD_WRITER_STATS_PKTS_IN_ADD(p, 1);
			pkts_mask &= ~pkt_mask;
		}

	p->tx_buf_count = tx_buf_count;
	if (tx_buf_count >= p->tx_burst_sz)
		send_burst(p);

	return 0;
}

static int
rte_port_fd_writer_flush(void *port)
{
	struct rte_port_fd_writer *p =
		port;

	if (p->tx_buf_count > 0)
		send_burst(p);

	return 0;
}

static int
rte_port_fd_writer_free(void *port)
{
	if (port == NULL) {
		RTE_LOG(ERR, PORT, "%s: Port is NULL\n", __func__);
		return -EINVAL;
	}

	rte_port_fd_writer_flush(port);
	rte_free(port);

	return 0;
}

static int rte_port_fd_writer_stats_read(void *port,
		struct rte_port_out_stats *stats, int clear)
{
	struct rte_port_fd_writer *p =
		port;

	if (stats != NULL)
		memcpy(stats, &p->stats, sizeof(p->stats));

	if (clear)
		memset(&p->stats, 0, sizeof(p->stats));

	return 0;
}

/*
 * Port FD Writer Nodrop
 */
#ifdef RTE_PORT_STATS_COLLECT

#define RTE_PORT_FD_WRITER_NODROP_STATS_PKTS_IN_ADD(port, val) \
	do { port->stats.n_pkts_in += val; } while (0)
#define RTE_PORT_FD_WRITER_NODROP_STATS_PKTS_DROP_ADD(port, val) \
	do { port->stats.n_pkts_drop += val; } while (0)

#else

#define RTE_PORT_FD_WRITER_NODROP_STATS_PKTS_IN_ADD(port, val)
#define RTE_PORT_FD_WRITER_NODROP_STATS_PKTS_DROP_ADD(port, val)

#endif

struct rte_port_fd_writer_nodrop {
	struct rte_port_out_stats stats;

	struct rte_mbuf *tx_buf[2 * RTE_PORT_IN_BURST_SIZE_MAX];
	uint32_t tx_burst_sz;
	uint16_t tx_buf_count;
	uint64_t n_retries;
	uint32_t fd;
};

static void *
rte_port_fd_writer_nodrop_create(void *params, int socket_id)
{
	struct rte_port_fd_writer_nodrop_params *conf =
			params;
	struct rte_port_fd_writer_nodrop *port;

	/* Check input parameters */
	if ((conf == NULL) ||
		(conf->fd < 0) ||
		(conf->tx_burst_sz == 0) ||
		(conf->tx_burst_sz > RTE_PORT_IN_BURST_SIZE_MAX) ||
		(!rte_is_power_of_2(conf->tx_burst_sz))) {
		RTE_LOG(ERR, PORT, "%s: Invalid input parameters\n", __func__);
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
	port->fd = conf->fd;
	port->tx_burst_sz = conf->tx_burst_sz;
	port->tx_buf_count = 0;

	/*
	 * When n_retries is 0 it means that we should wait for every packet to
	 * send no matter how many retries should it take. To limit number of
	 * branches in fast path, we use UINT64_MAX instead of branching.
	 */
	port->n_retries = (conf->n_retries == 0) ? UINT64_MAX : conf->n_retries;

	return port;
}

static inline void
send_burst_nodrop(struct rte_port_fd_writer_nodrop *p)
{
	uint64_t n_retries;
	uint32_t i;

	n_retries = 0;
	for (i = 0; (i < p->tx_buf_count) && (n_retries < p->n_retries); i++) {
		struct rte_mbuf *pkt = p->tx_buf[i];
		void *pkt_data = rte_pktmbuf_mtod(pkt, void*);
		size_t n_bytes = rte_pktmbuf_data_len(pkt);

		for ( ; n_retries < p->n_retries; n_retries++) {
			ssize_t ret;

			ret = write(p->fd, pkt_data, n_bytes);
			if (ret)
				break;
		}
	}

	RTE_PORT_FD_WRITER_NODROP_STATS_PKTS_DROP_ADD(p, p->tx_buf_count - i);

	for (i = 0; i < p->tx_buf_count; i++)
		rte_pktmbuf_free(p->tx_buf[i]);

	p->tx_buf_count = 0;
}

static int
rte_port_fd_writer_nodrop_tx(void *port, struct rte_mbuf *pkt)
{
	struct rte_port_fd_writer_nodrop *p =
		port;

	p->tx_buf[p->tx_buf_count++] = pkt;
	RTE_PORT_FD_WRITER_NODROP_STATS_PKTS_IN_ADD(p, 1);
	if (p->tx_buf_count >= p->tx_burst_sz)
		send_burst_nodrop(p);

	return 0;
}

static int
rte_port_fd_writer_nodrop_tx_bulk(void *port,
	struct rte_mbuf **pkts,
	uint64_t pkts_mask)
{
	struct rte_port_fd_writer_nodrop *p =
		port;
	uint32_t tx_buf_count = p->tx_buf_count;

	if ((pkts_mask & (pkts_mask + 1)) == 0) {
		uint64_t n_pkts = __builtin_popcountll(pkts_mask);
		uint32_t i;

		for (i = 0; i < n_pkts; i++)
			p->tx_buf[tx_buf_count++] = pkts[i];
		RTE_PORT_FD_WRITER_NODROP_STATS_PKTS_IN_ADD(p, n_pkts);
	} else
		for ( ; pkts_mask; ) {
			uint32_t pkt_index = __builtin_ctzll(pkts_mask);
			uint64_t pkt_mask = 1LLU << pkt_index;
			struct rte_mbuf *pkt = pkts[pkt_index];

			p->tx_buf[tx_buf_count++] = pkt;
			RTE_PORT_FD_WRITER_NODROP_STATS_PKTS_IN_ADD(p, 1);
			pkts_mask &= ~pkt_mask;
		}

	p->tx_buf_count = tx_buf_count;
	if (tx_buf_count >= p->tx_burst_sz)
		send_burst_nodrop(p);

	return 0;
}

static int
rte_port_fd_writer_nodrop_flush(void *port)
{
	struct rte_port_fd_writer_nodrop *p =
		port;

	if (p->tx_buf_count > 0)
		send_burst_nodrop(p);

	return 0;
}

static int
rte_port_fd_writer_nodrop_free(void *port)
{
	if (port == NULL) {
		RTE_LOG(ERR, PORT, "%s: Port is NULL\n", __func__);
		return -EINVAL;
	}

	rte_port_fd_writer_nodrop_flush(port);
	rte_free(port);

return 0;
}

static int rte_port_fd_writer_nodrop_stats_read(void *port,
		struct rte_port_out_stats *stats, int clear)
{
	struct rte_port_fd_writer_nodrop *p =
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
struct rte_port_in_ops rte_port_fd_reader_ops = {
	.f_create = rte_port_fd_reader_create,
	.f_free = rte_port_fd_reader_free,
	.f_rx = rte_port_fd_reader_rx,
	.f_stats = rte_port_fd_reader_stats_read,
};

struct rte_port_out_ops rte_port_fd_writer_ops = {
	.f_create = rte_port_fd_writer_create,
	.f_free = rte_port_fd_writer_free,
	.f_tx = rte_port_fd_writer_tx,
	.f_tx_bulk = rte_port_fd_writer_tx_bulk,
	.f_flush = rte_port_fd_writer_flush,
	.f_stats = rte_port_fd_writer_stats_read,
};

struct rte_port_out_ops rte_port_fd_writer_nodrop_ops = {
	.f_create = rte_port_fd_writer_nodrop_create,
	.f_free = rte_port_fd_writer_nodrop_free,
	.f_tx = rte_port_fd_writer_nodrop_tx,
	.f_tx_bulk = rte_port_fd_writer_nodrop_tx_bulk,
	.f_flush = rte_port_fd_writer_nodrop_flush,
	.f_stats = rte_port_fd_writer_nodrop_stats_read,
};
