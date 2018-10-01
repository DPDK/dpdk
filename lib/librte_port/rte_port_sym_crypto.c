/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */
#include <string.h>

#include <rte_common.h>
#include <rte_malloc.h>

#include "rte_port_sym_crypto.h"

/*
 * Port Crypto Reader
 */
#ifdef RTE_PORT_STATS_COLLECT

#define RTE_PORT_SYM_CRYPTO_READER_STATS_PKTS_IN_ADD(port, val) \
	(port)->stats.n_pkts_in += (val)
#define RTE_PORT_SYM_CRYPTO_READER_STATS_PKTS_DROP_ADD(port, val) \
	(port)->stats.n_pkts_drop += (val)

#else

#define RTE_PORT_SYM_CRYPTO_READER_STATS_PKTS_IN_ADD(port, val)
#define RTE_PORT_SYM_CRYPTO_READER_STATS_PKTS_DROP_ADD(port, val)

#endif

struct rte_port_sym_crypto_reader {
	struct rte_port_in_stats stats;

	uint8_t cryptodev_id;
	uint16_t queue_id;
	struct rte_crypto_op *ops[RTE_PORT_IN_BURST_SIZE_MAX];
	rte_port_sym_crypto_reader_callback_fn f_callback;
	void *arg_callback;
};

static void *
rte_port_sym_crypto_reader_create(void *params, int socket_id)
{
	struct rte_port_sym_crypto_reader_params *conf =
			params;
	struct rte_port_sym_crypto_reader *port;

	/* Check input parameters */
	if (conf == NULL) {
		RTE_LOG(ERR, PORT, "%s: params is NULL\n", __func__);
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
	port->cryptodev_id = conf->cryptodev_id;
	port->queue_id = conf->queue_id;
	port->f_callback = conf->f_callback;
	port->arg_callback = conf->arg_callback;

	return port;
}

static int
rte_port_sym_crypto_reader_rx(void *port, struct rte_mbuf **pkts, uint32_t n_pkts)
{
	struct rte_port_sym_crypto_reader *p =
			port;
	uint16_t rx_ops_cnt, i, n = 0;

	rx_ops_cnt = rte_cryptodev_dequeue_burst(p->cryptodev_id, p->queue_id,
			p->ops, n_pkts);

	for (i = 0; i < rx_ops_cnt; i++) {
		struct rte_crypto_op *op = p->ops[i];

		/** Drop failed pkts */
		if (unlikely(op->status != RTE_CRYPTO_OP_STATUS_SUCCESS)) {
			rte_pktmbuf_free(op->sym->m_src);
			continue;
		}

		pkts[n++] = op->sym->m_src;
	}

	if (p->f_callback)
		(*p->f_callback)(pkts, n, p->arg_callback);

	RTE_PORT_SYM_CRYPTO_READER_STATS_PKTS_IN_ADD(p, n);
	RTE_PORT_SYM_CRYPTO_READER_STATS_PKTS_DROP_ADD(p, rx_ops_cnt - n);

	return n;
}

static int
rte_port_sym_crypto_reader_free(void *port)
{
	if (port == NULL) {
		RTE_LOG(ERR, PORT, "%s: port is NULL\n", __func__);
		return -EINVAL;
	}

	rte_free(port);

	return 0;
}

static int rte_port_sym_crypto_reader_stats_read(void *port,
	struct rte_port_in_stats *stats, int clear)
{
	struct rte_port_sym_crypto_reader *p =
			port;

	if (stats != NULL)
		memcpy(stats, &p->stats, sizeof(p->stats));

	if (clear)
		memset(&p->stats, 0, sizeof(p->stats));

	return 0;
}

/*
 * Port crypto Writer
 */
#ifdef RTE_PORT_STATS_COLLECT

#define RTE_PORT_SYM_CRYPTO_WRITER_STATS_PKTS_IN_ADD(port, val) \
	(port)->stats.n_pkts_in += (val)
#define RTE_PORT_SYM_CRYPTO_WRITER_STATS_PKTS_DROP_ADD(port, val) \
	(port)->stats.n_pkts_drop += (val)

#else

#define RTE_PORT_SYM_CRYPTO_WRITER_STATS_PKTS_IN_ADD(port, val)
#define RTE_PORT_SYM_CRYPTO_WRITER_STATS_PKTS_DROP_ADD(port, val)

#endif

struct rte_port_sym_crypto_writer {
	struct rte_port_out_stats stats;

	struct rte_crypto_op *tx_buf[2 * RTE_PORT_IN_BURST_SIZE_MAX];

	uint32_t tx_burst_sz;
	uint32_t tx_buf_count;
	uint64_t bsz_mask;

	uint8_t cryptodev_id;
	uint16_t queue_id;
	uint16_t crypto_op_offset;
};

static void *
rte_port_sym_crypto_writer_create(void *params, int socket_id)
{
	struct rte_port_sym_crypto_writer_params *conf =
			params;
	struct rte_port_sym_crypto_writer *port;

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
	port->tx_burst_sz = conf->tx_burst_sz;
	port->tx_buf_count = 0;
	port->bsz_mask = 1LLU << (conf->tx_burst_sz - 1);

	port->cryptodev_id = conf->cryptodev_id;
	port->queue_id = conf->queue_id;
	port->crypto_op_offset = conf->crypto_op_offset;

	return port;
}

static inline void
send_burst(struct rte_port_sym_crypto_writer *p)
{
	uint32_t nb_tx;

	nb_tx = rte_cryptodev_enqueue_burst(p->cryptodev_id, p->queue_id,
			p->tx_buf, p->tx_buf_count);

	RTE_PORT_SYM_CRYPTO_WRITER_STATS_PKTS_DROP_ADD(p, p->tx_buf_count -
			nb_tx);
	for (; nb_tx < p->tx_buf_count; nb_tx++)
		rte_pktmbuf_free(p->tx_buf[nb_tx]->sym->m_src);

	p->tx_buf_count = 0;
}

static int
rte_port_sym_crypto_writer_tx(void *port, struct rte_mbuf *pkt)
{
	struct rte_port_sym_crypto_writer *p =
			port;

	p->tx_buf[p->tx_buf_count++] = (struct rte_crypto_op *)
			RTE_MBUF_METADATA_UINT8_PTR(pkt, p->crypto_op_offset);
	RTE_PORT_SYM_CRYPTO_WRITER_STATS_PKTS_IN_ADD(p, 1);
	if (p->tx_buf_count >= p->tx_burst_sz)
		send_burst(p);

	return 0;
}

static int
rte_port_sym_crypto_writer_tx_bulk(void *port,
	struct rte_mbuf **pkts,
	uint64_t pkts_mask)
{
	struct rte_port_sym_crypto_writer *p =
			port;
	uint64_t bsz_mask = p->bsz_mask;
	uint32_t tx_buf_count = p->tx_buf_count;
	uint64_t expr = (pkts_mask & (pkts_mask + 1)) |
					((pkts_mask & bsz_mask) ^ bsz_mask);

	if (expr == 0) {
		uint64_t n_pkts = __builtin_popcountll(pkts_mask);
		uint32_t i;

		RTE_PORT_SYM_CRYPTO_WRITER_STATS_PKTS_IN_ADD(p, n_pkts);

		for (i = 0; i < n_pkts; i++)
			p->tx_buf[p->tx_buf_count++] = (struct rte_crypto_op *)
					RTE_MBUF_METADATA_UINT8_PTR(pkts[i],
							p->crypto_op_offset);

		if (p->tx_buf_count >= p->tx_burst_sz)
			send_burst(p);
	} else {
		for (; pkts_mask;) {
			uint32_t pkt_index = __builtin_ctzll(pkts_mask);
			uint64_t pkt_mask = 1LLU << pkt_index;
			struct rte_mbuf *pkt = pkts[pkt_index];

			p->tx_buf[tx_buf_count++] = (struct rte_crypto_op *)
					RTE_MBUF_METADATA_UINT8_PTR(pkt,
							p->crypto_op_offset);

			RTE_PORT_SYM_CRYPTO_WRITER_STATS_PKTS_IN_ADD(p, 1);
			pkts_mask &= ~pkt_mask;
		}

		p->tx_buf_count = tx_buf_count;
		if (tx_buf_count >= p->tx_burst_sz)
			send_burst(p);
	}

	return 0;
}

static int
rte_port_sym_crypto_writer_flush(void *port)
{
	struct rte_port_sym_crypto_writer *p =
			port;

	if (p->tx_buf_count > 0)
		send_burst(p);

	return 0;
}

static int
rte_port_sym_crypto_writer_free(void *port)
{
	if (port == NULL) {
		RTE_LOG(ERR, PORT, "%s: Port is NULL\n", __func__);
		return -EINVAL;
	}

	rte_port_sym_crypto_writer_flush(port);
	rte_free(port);

	return 0;
}

static int rte_port_sym_crypto_writer_stats_read(void *port,
	struct rte_port_out_stats *stats, int clear)
{
	struct rte_port_sym_crypto_writer *p =
			port;

	if (stats != NULL)
		memcpy(stats, &p->stats, sizeof(p->stats));

	if (clear)
		memset(&p->stats, 0, sizeof(p->stats));

	return 0;
}

/*
 * Port crypto Writer Nodrop
 */
#ifdef RTE_PORT_STATS_COLLECT

#define RTE_PORT_SYM_CRYPTO_WRITER_NODROP_STATS_PKTS_IN_ADD(port, val) \
	(port)->stats.n_pkts_in += (val)
#define RTE_PORT_SYM_CRYPTO_WRITER_NODROP_STATS_PKTS_DROP_ADD(port, val) \
	(port)->stats.n_pkts_drop += (val)

#else

#define RTE_PORT_SYM_CRYPTO_WRITER_NODROP_STATS_PKTS_IN_ADD(port, val)
#define RTE_PORT_SYM_CRYPTO_WRITER_NODROP_STATS_PKTS_DROP_ADD(port, val)

#endif

struct rte_port_sym_crypto_writer_nodrop {
	struct rte_port_out_stats stats;

	struct rte_crypto_op *tx_buf[2 * RTE_PORT_IN_BURST_SIZE_MAX];
	uint32_t tx_burst_sz;
	uint32_t tx_buf_count;
	uint64_t bsz_mask;
	uint64_t n_retries;

	uint8_t cryptodev_id;
	uint16_t queue_id;
	uint16_t crypto_op_offset;
};

static void *
rte_port_sym_crypto_writer_nodrop_create(void *params, int socket_id)
{
	struct rte_port_sym_crypto_writer_nodrop_params *conf =
		params;
	struct rte_port_sym_crypto_writer_nodrop *port;

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
	port->cryptodev_id = conf->cryptodev_id;
	port->queue_id = conf->queue_id;
	port->crypto_op_offset = conf->crypto_op_offset;
	port->tx_burst_sz = conf->tx_burst_sz;
	port->tx_buf_count = 0;
	port->bsz_mask = 1LLU << (conf->tx_burst_sz - 1);

	/*
	 * When n_retries is 0 it means that we should wait for every packet to
	 * send no matter how many retries should it take. To limit number of
	 * branches in fast path, we use UINT64_MAX instead of branching.
	 */
	port->n_retries = (conf->n_retries == 0) ? UINT64_MAX : conf->n_retries;

	return port;
}

static inline void
send_burst_nodrop(struct rte_port_sym_crypto_writer_nodrop *p)
{
	uint32_t nb_tx = 0, i;

	nb_tx = rte_cryptodev_enqueue_burst(p->cryptodev_id, p->queue_id,
			p->tx_buf, p->tx_buf_count);

	/* We sent all the packets in a first try */
	if (nb_tx >= p->tx_buf_count) {
		p->tx_buf_count = 0;
		return;
	}

	for (i = 0; i < p->n_retries; i++) {
		nb_tx += rte_cryptodev_enqueue_burst(p->cryptodev_id,
				p->queue_id, p->tx_buf + nb_tx,
				p->tx_buf_count - nb_tx);

		/* We sent all the packets in more than one try */
		if (nb_tx >= p->tx_buf_count) {
			p->tx_buf_count = 0;
			return;
		}
	}

	/* We didn't send the packets in maximum allowed attempts */
	RTE_PORT_SYM_CRYPTO_WRITER_NODROP_STATS_PKTS_DROP_ADD(p,
			p->tx_buf_count - nb_tx);
	for ( ; nb_tx < p->tx_buf_count; nb_tx++)
		rte_pktmbuf_free(p->tx_buf[nb_tx]->sym->m_src);

	p->tx_buf_count = 0;
}

static int
rte_port_sym_crypto_writer_nodrop_tx(void *port, struct rte_mbuf *pkt)
{
	struct rte_port_sym_crypto_writer_nodrop *p =
			port;

	p->tx_buf[p->tx_buf_count++] = (struct rte_crypto_op *)
			RTE_MBUF_METADATA_UINT8_PTR(pkt, p->crypto_op_offset);
	RTE_PORT_SYM_CRYPTO_WRITER_STATS_PKTS_IN_ADD(p, 1);
	if (p->tx_buf_count >= p->tx_burst_sz)
		send_burst_nodrop(p);

	return 0;
}

static int
rte_port_sym_crypto_writer_nodrop_tx_bulk(void *port,
	struct rte_mbuf **pkts,
	uint64_t pkts_mask)
{
	struct rte_port_sym_crypto_writer_nodrop *p =
			port;

	uint64_t bsz_mask = p->bsz_mask;
	uint32_t tx_buf_count = p->tx_buf_count;
	uint64_t expr = (pkts_mask & (pkts_mask + 1)) |
					((pkts_mask & bsz_mask) ^ bsz_mask);

	if (expr == 0) {
		uint64_t n_pkts = __builtin_popcountll(pkts_mask);
		uint32_t i;

		RTE_PORT_SYM_CRYPTO_WRITER_NODROP_STATS_PKTS_IN_ADD(p, n_pkts);

		for (i = 0; i < n_pkts; i++)
			p->tx_buf[p->tx_buf_count++] = (struct rte_crypto_op *)
					RTE_MBUF_METADATA_UINT8_PTR(pkts[i],
							p->crypto_op_offset);

		if (p->tx_buf_count >= p->tx_burst_sz)
			send_burst_nodrop(p);
	} else {
		for ( ; pkts_mask; ) {
			uint32_t pkt_index = __builtin_ctzll(pkts_mask);
			uint64_t pkt_mask = 1LLU << pkt_index;
			struct rte_mbuf *pkt = pkts[pkt_index];

			p->tx_buf[tx_buf_count++] = (struct rte_crypto_op *)
					RTE_MBUF_METADATA_UINT8_PTR(pkt,
							p->crypto_op_offset);
			RTE_PORT_SYM_CRYPTO_WRITER_NODROP_STATS_PKTS_IN_ADD(p,
					1);
			pkts_mask &= ~pkt_mask;
		}

		p->tx_buf_count = tx_buf_count;
		if (tx_buf_count >= p->tx_burst_sz)
			send_burst_nodrop(p);
	}

	return 0;
}

static int
rte_port_sym_crypto_writer_nodrop_flush(void *port)
{
	struct rte_port_sym_crypto_writer_nodrop *p =
		port;

	if (p->tx_buf_count > 0)
		send_burst_nodrop(p);

	return 0;
}

static int
rte_port_sym_crypto_writer_nodrop_free(void *port)
{
	if (port == NULL) {
		RTE_LOG(ERR, PORT, "%s: Port is NULL\n", __func__);
		return -EINVAL;
	}

	rte_port_sym_crypto_writer_nodrop_flush(port);
	rte_free(port);

	return 0;
}

static int rte_port_sym_crypto_writer_nodrop_stats_read(void *port,
	struct rte_port_out_stats *stats, int clear)
{
	struct rte_port_sym_crypto_writer_nodrop *p =
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
struct rte_port_in_ops rte_port_sym_crypto_reader_ops = {
	.f_create = rte_port_sym_crypto_reader_create,
	.f_free = rte_port_sym_crypto_reader_free,
	.f_rx = rte_port_sym_crypto_reader_rx,
	.f_stats = rte_port_sym_crypto_reader_stats_read,
};

struct rte_port_out_ops rte_port_sym_crypto_writer_ops = {
	.f_create = rte_port_sym_crypto_writer_create,
	.f_free = rte_port_sym_crypto_writer_free,
	.f_tx = rte_port_sym_crypto_writer_tx,
	.f_tx_bulk = rte_port_sym_crypto_writer_tx_bulk,
	.f_flush = rte_port_sym_crypto_writer_flush,
	.f_stats = rte_port_sym_crypto_writer_stats_read,
};

struct rte_port_out_ops rte_port_sym_crypto_writer_nodrop_ops = {
	.f_create = rte_port_sym_crypto_writer_nodrop_create,
	.f_free = rte_port_sym_crypto_writer_nodrop_free,
	.f_tx = rte_port_sym_crypto_writer_nodrop_tx,
	.f_tx_bulk = rte_port_sym_crypto_writer_nodrop_tx_bulk,
	.f_flush = rte_port_sym_crypto_writer_nodrop_flush,
	.f_stats = rte_port_sym_crypto_writer_nodrop_stats_read,
};
