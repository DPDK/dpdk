/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include <string.h>
#include <stdint.h>

#include <rte_mbuf.h>
#include <rte_malloc.h>

#include "rte_port_eventdev.h"

/*
 * Port EVENTDEV Reader
 */
#ifdef RTE_PORT_STATS_COLLECT

#define RTE_PORT_EVENTDEV_READER_STATS_PKTS_IN_ADD(port, val) \
	do {port->stats.n_pkts_in += val;} while (0)
#define RTE_PORT_EVENTDEV_READER_STATS_PKTS_DROP_ADD(port, val) \
	do {port->stats.n_pkts_drop += val;} while (0)

#else

#define RTE_PORT_EVENTDEV_READER_STATS_PKTS_IN_ADD(port, val)
#define RTE_PORT_EVENTDEV_READER_STATS_PKTS_DROP_ADD(port, val)

#endif

struct rte_port_eventdev_reader {
	struct rte_port_in_stats stats;

	uint8_t  eventdev_id;
	uint16_t port_id;

	struct rte_event ev[RTE_PORT_IN_BURST_SIZE_MAX];
};

static void *
rte_port_eventdev_reader_create(void *params, int socket_id)
{
	struct rte_port_eventdev_reader_params *conf =
			params;
	struct rte_port_eventdev_reader *port;

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
	port->eventdev_id = conf->eventdev_id;
	port->port_id = conf->port_id;

	return port;
}

static int
rte_port_eventdev_reader_rx(void *port, struct rte_mbuf **pkts, uint32_t n_pkts)
{
	struct rte_port_eventdev_reader *p = port;
	uint16_t rx_evts_cnt, i;

	rx_evts_cnt = rte_event_dequeue_burst(p->eventdev_id, p->port_id,
			p->ev, n_pkts, 0);

	for (i = 0; i < rx_evts_cnt; i++)
		pkts[i] = p->ev[i].mbuf;

	RTE_PORT_EVENTDEV_READER_STATS_PKTS_IN_ADD(p, rx_evts_cnt);

	return rx_evts_cnt;
}

static int
rte_port_eventdev_reader_free(void *port)
{
	if (port == NULL) {
		RTE_LOG(ERR, PORT, "%s: port is NULL\n", __func__);
		return -EINVAL;
	}

	rte_free(port);

	return 0;
}

static int rte_port_eventdev_reader_stats_read(void *port,
	struct rte_port_in_stats *stats, int clear)
{
	struct rte_port_eventdev_reader *p =
			port;

	if (stats != NULL)
		memcpy(stats, &p->stats, sizeof(p->stats));

	if (clear)
		memset(&p->stats, 0, sizeof(p->stats));

	return 0;
}

/*
 * Port EVENTDEV Writer
 */
#ifdef RTE_PORT_STATS_COLLECT

#define RTE_PORT_EVENTDEV_WRITER_STATS_PKTS_IN_ADD(port, val) \
	do {port->stats.n_pkts_in += val;} while (0)
#define RTE_PORT_EVENTDEV_WRITER_STATS_PKTS_DROP_ADD(port, val) \
	do {port->stats.n_pkts_drop += val;} while (0)

#else

#define RTE_PORT_EVENTDEV_WRITER_STATS_PKTS_IN_ADD(port, val)
#define RTE_PORT_EVENTDEV_WRITER_STATS_PKTS_DROP_ADD(port, val)

#endif

struct rte_port_eventdev_writer {
	struct rte_port_out_stats stats;

	struct rte_event ev[2 * RTE_PORT_IN_BURST_SIZE_MAX];

	uint32_t enq_burst_sz;
	uint32_t enq_buf_count;
	uint64_t bsz_mask;

	uint8_t eventdev_id;
	uint8_t port_id;
	uint8_t queue_id;
	uint8_t sched_type;
	uint8_t evt_op;
};

static void *
rte_port_eventdev_writer_create(void *params, int socket_id)
{
	struct rte_port_eventdev_writer_params *conf =
			params;
	struct rte_port_eventdev_writer *port;
	unsigned int i;

	/* Check input parameters */
	if ((conf == NULL) ||
		(conf->enq_burst_sz == 0) ||
		(conf->enq_burst_sz > RTE_PORT_IN_BURST_SIZE_MAX) ||
		(!rte_is_power_of_2(conf->enq_burst_sz))) {
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
	port->enq_burst_sz = conf->enq_burst_sz;
	port->enq_buf_count = 0;
	port->bsz_mask = 1LLU << (conf->enq_burst_sz - 1);

	port->eventdev_id = conf->eventdev_id;
	port->port_id = conf->port_id;
	port->queue_id = conf->queue_id;
	port->sched_type = conf->sched_type;
	port->evt_op = conf->evt_op;
	memset(&port->ev, 0, sizeof(port->ev));

	for (i = 0; i < RTE_DIM(port->ev); i++) {
		port->ev[i].queue_id = port->queue_id;
		port->ev[i].sched_type = port->sched_type;
		port->ev[i].op = port->evt_op;
	}

	return port;
}

static inline void
send_burst(struct rte_port_eventdev_writer *p)
{
	uint32_t nb_enq;

	nb_enq = rte_event_enqueue_burst(p->eventdev_id, p->port_id,
			p->ev, p->enq_buf_count);

	RTE_PORT_EVENTDEV_WRITER_STATS_PKTS_DROP_ADD(p, p->enq_buf_count -
			nb_enq);

	for (; nb_enq < p->enq_buf_count; nb_enq++)
		rte_pktmbuf_free(p->ev[nb_enq].mbuf);

	p->enq_buf_count = 0;
}

static int
rte_port_eventdev_writer_tx(void *port, struct rte_mbuf *pkt)
{
	struct rte_port_eventdev_writer *p = port;

	p->ev[p->enq_buf_count++].mbuf  = pkt;
	RTE_PORT_EVENTDEV_WRITER_STATS_PKTS_IN_ADD(p, 1);
	if (p->enq_buf_count >= p->enq_burst_sz)
		send_burst(p);

	return 0;
}

static int
rte_port_eventdev_writer_tx_bulk(void *port,
	struct rte_mbuf **pkts,
	uint64_t pkts_mask)
{
	struct rte_port_eventdev_writer *p =
			port;
	uint64_t bsz_mask = p->bsz_mask;
	uint32_t enq_buf_count = p->enq_buf_count;
	uint64_t expr = (pkts_mask & (pkts_mask + 1)) |
					((pkts_mask & bsz_mask) ^ bsz_mask);

	if (expr == 0) {
		uint64_t n_pkts = rte_popcount64(pkts_mask);
		uint32_t i, n_enq_ok;

		if (enq_buf_count)
			send_burst(p);

		RTE_PORT_EVENTDEV_WRITER_STATS_PKTS_IN_ADD(p, n_pkts);

		struct rte_event events[2 * RTE_PORT_IN_BURST_SIZE_MAX] = {};
		for (i = 0; i < n_pkts; i++) {
			events[i].mbuf = pkts[i];
			events[i].queue_id = p->queue_id;
			events[i].sched_type = p->sched_type;
			events[i].op = p->evt_op;
		}

		n_enq_ok = rte_event_enqueue_burst(p->eventdev_id, p->port_id,
				events, n_pkts);

		RTE_PORT_EVENTDEV_WRITER_STATS_PKTS_DROP_ADD(p,
				n_pkts - n_enq_ok);
		for (; n_enq_ok < n_pkts; n_enq_ok++)
			rte_pktmbuf_free(pkts[n_enq_ok]);

	} else {
		for (; pkts_mask;) {
			uint32_t pkt_index = rte_ctz64(pkts_mask);
			uint64_t pkt_mask = 1LLU << pkt_index;

			p->ev[enq_buf_count++].mbuf = pkts[pkt_index];

			RTE_PORT_EVENTDEV_WRITER_STATS_PKTS_IN_ADD(p, 1);
			pkts_mask &= ~pkt_mask;
		}

		p->enq_buf_count = enq_buf_count;
		if (enq_buf_count >= p->enq_burst_sz)
			send_burst(p);
	}

	return 0;
}

static int
rte_port_eventdev_writer_flush(void *port)
{
	struct rte_port_eventdev_writer *p =
			port;

	if (p->enq_buf_count > 0)
		send_burst(p);

	return 0;
}

static int
rte_port_eventdev_writer_free(void *port)
{
	if (port == NULL) {
		RTE_LOG(ERR, PORT, "%s: Port is NULL\n", __func__);
		return -EINVAL;
	}

	rte_port_eventdev_writer_flush(port);
	rte_free(port);

	return 0;
}

static int rte_port_eventdev_writer_stats_read(void *port,
	struct rte_port_out_stats *stats, int clear)
{
	struct rte_port_eventdev_writer *p =
			port;

	if (stats != NULL)
		memcpy(stats, &p->stats, sizeof(p->stats));

	if (clear)
		memset(&p->stats, 0, sizeof(p->stats));

	return 0;
}

/*
 * Port EVENTDEV Writer Nodrop
 */
#ifdef RTE_PORT_STATS_COLLECT

#define RTE_PORT_EVENTDEV_WRITER_NODROP_STATS_PKTS_IN_ADD(port, val) \
	do {port->stats.n_pkts_in += val;} while (0)
#define RTE_PORT_EVENTDEV_WRITER_NODROP_STATS_PKTS_DROP_ADD(port, val) \
	do {port->stats.n_pkts_drop += val;} while (0)

#else

#define RTE_PORT_EVENTDEV_WRITER_NODROP_STATS_PKTS_IN_ADD(port, val)
#define RTE_PORT_EVENTDEV_WRITER_NODROP_STATS_PKTS_DROP_ADD(port, val)

#endif

struct rte_port_eventdev_writer_nodrop {
	struct rte_port_out_stats stats;

	struct rte_event ev[2 * RTE_PORT_IN_BURST_SIZE_MAX];

	uint32_t enq_burst_sz;
	uint32_t enq_buf_count;
	uint64_t bsz_mask;
	uint64_t n_retries;
	uint8_t eventdev_id;
	uint8_t port_id;
	uint8_t queue_id;
	uint8_t sched_type;
	uint8_t evt_op;
};


static void *
rte_port_eventdev_writer_nodrop_create(void *params, int socket_id)
{
	struct rte_port_eventdev_writer_nodrop_params *conf =
			params;
	struct rte_port_eventdev_writer_nodrop *port;
	unsigned int i;

	/* Check input parameters */
	if ((conf == NULL) ||
		(conf->enq_burst_sz == 0) ||
		(conf->enq_burst_sz > RTE_PORT_IN_BURST_SIZE_MAX) ||
		(!rte_is_power_of_2(conf->enq_burst_sz))) {
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
	port->enq_burst_sz = conf->enq_burst_sz;
	port->enq_buf_count = 0;
	port->bsz_mask = 1LLU << (conf->enq_burst_sz - 1);

	port->eventdev_id = conf->eventdev_id;
	port->port_id = conf->port_id;
	port->queue_id = conf->queue_id;
	port->sched_type = conf->sched_type;
	port->evt_op = conf->evt_op;
	memset(&port->ev, 0, sizeof(port->ev));

	for (i = 0; i < RTE_DIM(port->ev); i++) {
		port->ev[i].queue_id = port->queue_id;
		port->ev[i].sched_type = port->sched_type;
		port->ev[i].op = port->evt_op;
	}
	/*
	 * When n_retries is 0 it means that we should wait for every event to
	 * send no matter how many retries should it take. To limit number of
	 * branches in fast path, we use UINT64_MAX instead of branching.
	 */
	port->n_retries = (conf->n_retries == 0) ? UINT64_MAX : conf->n_retries;

	return port;
}

static inline void
send_burst_nodrop(struct rte_port_eventdev_writer_nodrop *p)
{
	uint32_t nb_enq, i;

	nb_enq = rte_event_enqueue_burst(p->eventdev_id, p->port_id,
			p->ev, p->enq_buf_count);

	/* We sent all the packets in a first try */
	if (nb_enq >= p->enq_buf_count) {
		p->enq_buf_count = 0;
		return;
	}

	for (i = 0; i < p->n_retries; i++) {
		nb_enq += rte_event_enqueue_burst(p->eventdev_id, p->port_id,
							p->ev + nb_enq,
							p->enq_buf_count - nb_enq);

		/* We sent all the events in more than one try */
		if (nb_enq >= p->enq_buf_count) {
			p->enq_buf_count = 0;
			return;
		}
	}
	/* We didn't send the events in maximum allowed attempts */
	RTE_PORT_EVENTDEV_WRITER_NODROP_STATS_PKTS_DROP_ADD(p,
			p->enq_buf_count - nb_enq);
	for (; nb_enq < p->enq_buf_count; nb_enq++)
		rte_pktmbuf_free(p->ev[nb_enq].mbuf);

	p->enq_buf_count = 0;
}

static int
rte_port_eventdev_writer_nodrop_tx(void *port, struct rte_mbuf *pkt)
{
	struct rte_port_eventdev_writer_nodrop *p = port;

	p->ev[p->enq_buf_count++].mbuf = pkt;

	RTE_PORT_EVENTDEV_WRITER_NODROP_STATS_PKTS_IN_ADD(p, 1);
	if (p->enq_buf_count >= p->enq_burst_sz)
		send_burst_nodrop(p);

	return 0;
}

static int
rte_port_eventdev_writer_nodrop_tx_bulk(void *port,
	struct rte_mbuf **pkts,
	uint64_t pkts_mask)
{
	struct rte_port_eventdev_writer_nodrop *p =
			port;

	uint64_t bsz_mask = p->bsz_mask;
	uint32_t enq_buf_count = p->enq_buf_count;
	uint64_t expr = (pkts_mask & (pkts_mask + 1)) |
					((pkts_mask & bsz_mask) ^ bsz_mask);

	if (expr == 0) {
		uint64_t n_pkts = rte_popcount64(pkts_mask);
		uint32_t i, n_enq_ok;

		if (enq_buf_count)
			send_burst_nodrop(p);

		RTE_PORT_EVENTDEV_WRITER_NODROP_STATS_PKTS_IN_ADD(p, n_pkts);

		struct rte_event events[RTE_PORT_IN_BURST_SIZE_MAX] = {};

		for (i = 0; i < n_pkts; i++) {
			events[i].mbuf = pkts[i];
			events[i].queue_id = p->queue_id;
			events[i].sched_type = p->sched_type;
			events[i].op = p->evt_op;
		}

		n_enq_ok = rte_event_enqueue_burst(p->eventdev_id, p->port_id,
				events, n_pkts);

		if (n_enq_ok >= n_pkts)
			return 0;

		/*
		 * If we did not manage to enqueue all events in single burst,
		 * move remaining events to the buffer and call send burst.
		 */
		for (; n_enq_ok < n_pkts; n_enq_ok++) {
			struct rte_mbuf *pkt = pkts[n_enq_ok];
			p->ev[p->enq_buf_count++].mbuf = pkt;
		}
		send_burst_nodrop(p);
	} else {
		for (; pkts_mask;) {
			uint32_t pkt_index = rte_ctz64(pkts_mask);
			uint64_t pkt_mask = 1LLU << pkt_index;

			p->ev[enq_buf_count++].mbuf = pkts[pkt_index];

			RTE_PORT_EVENTDEV_WRITER_STATS_PKTS_IN_ADD(p, 1);
			pkts_mask &= ~pkt_mask;
		}

		p->enq_buf_count = enq_buf_count;
		if (enq_buf_count >= p->enq_burst_sz)
			send_burst_nodrop(p);
	}

	return 0;
}

static int
rte_port_eventdev_writer_nodrop_flush(void *port)
{
	struct rte_port_eventdev_writer_nodrop *p =
			port;

	if (p->enq_buf_count > 0)
		send_burst_nodrop(p);

	return 0;
}

static int
rte_port_eventdev_writer_nodrop_free(void *port)
{
	if (port == NULL) {
		RTE_LOG(ERR, PORT, "%s: Port is NULL\n", __func__);
		return -EINVAL;
	}

	rte_port_eventdev_writer_nodrop_flush(port);
	rte_free(port);

	return 0;
}

static int rte_port_eventdev_writer_nodrop_stats_read(void *port,
	struct rte_port_out_stats *stats, int clear)
{
	struct rte_port_eventdev_writer_nodrop *p =
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
struct rte_port_in_ops rte_port_eventdev_reader_ops = {
	.f_create = rte_port_eventdev_reader_create,
	.f_free = rte_port_eventdev_reader_free,
	.f_rx = rte_port_eventdev_reader_rx,
	.f_stats = rte_port_eventdev_reader_stats_read,
};

struct rte_port_out_ops rte_port_eventdev_writer_ops = {
	.f_create = rte_port_eventdev_writer_create,
	.f_free = rte_port_eventdev_writer_free,
	.f_tx = rte_port_eventdev_writer_tx,
	.f_tx_bulk = rte_port_eventdev_writer_tx_bulk,
	.f_flush = rte_port_eventdev_writer_flush,
	.f_stats = rte_port_eventdev_writer_stats_read,
};

struct rte_port_out_ops rte_port_eventdev_writer_nodrop_ops = {
	.f_create = rte_port_eventdev_writer_nodrop_create,
	.f_free = rte_port_eventdev_writer_nodrop_free,
	.f_tx = rte_port_eventdev_writer_nodrop_tx,
	.f_tx_bulk = rte_port_eventdev_writer_nodrop_tx_bulk,
	.f_flush = rte_port_eventdev_writer_nodrop_flush,
	.f_stats = rte_port_eventdev_writer_nodrop_stats_read,
};
