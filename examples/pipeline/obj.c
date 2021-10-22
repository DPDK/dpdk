/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#ifdef RTE_EXEC_ENV_LINUX
#include <linux/if.h>
#include <linux/if_tun.h>
#endif
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_swx_port_ethdev.h>
#include <rte_swx_port_fd.h>
#include <rte_swx_port_ring.h>
#include <rte_swx_port_source_sink.h>
#include <rte_swx_table_em.h>
#include <rte_swx_table_wm.h>
#include <rte_swx_pipeline.h>
#include <rte_swx_ctl.h>

#include "obj.h"

/*
 * mempool
 */
TAILQ_HEAD(mempool_list, mempool);

/*
 * link
 */
TAILQ_HEAD(link_list, link);

/*
 * ring
 */
TAILQ_HEAD(ring_list, ring);

/*
 * tap
 */
TAILQ_HEAD(tap_list, tap);

/*
 * pipeline
 */
TAILQ_HEAD(pipeline_list, pipeline);

/*
 * obj
 */
struct obj {
	struct mempool_list mempool_list;
	struct link_list link_list;
	struct ring_list ring_list;
	struct pipeline_list pipeline_list;
	struct tap_list tap_list;
};

/*
 * mempool
 */
#define BUFFER_SIZE_MIN (sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)

struct mempool *
mempool_create(struct obj *obj, const char *name, struct mempool_params *params)
{
	struct mempool *mempool;
	struct rte_mempool *m;

	/* Check input params */
	if ((name == NULL) ||
		mempool_find(obj, name) ||
		(params == NULL) ||
		(params->buffer_size < BUFFER_SIZE_MIN) ||
		(params->pool_size == 0))
		return NULL;

	/* Resource create */
	m = rte_pktmbuf_pool_create(
		name,
		params->pool_size,
		params->cache_size,
		0,
		params->buffer_size - sizeof(struct rte_mbuf),
		params->cpu_id);

	if (m == NULL)
		return NULL;

	/* Node allocation */
	mempool = calloc(1, sizeof(struct mempool));
	if (mempool == NULL) {
		rte_mempool_free(m);
		return NULL;
	}

	/* Node fill in */
	strlcpy(mempool->name, name, sizeof(mempool->name));
	mempool->m = m;
	mempool->buffer_size = params->buffer_size;

	/* Node add to list */
	TAILQ_INSERT_TAIL(&obj->mempool_list, mempool, node);

	return mempool;
}

struct mempool *
mempool_find(struct obj *obj, const char *name)
{
	struct mempool *mempool;

	if (!obj || !name)
		return NULL;

	TAILQ_FOREACH(mempool, &obj->mempool_list, node)
		if (strcmp(mempool->name, name) == 0)
			return mempool;

	return NULL;
}

/*
 * link
 */
static struct rte_eth_conf port_conf_default = {
	.link_speeds = 0,
	.rxmode = {
		.mq_mode = RTE_ETH_MQ_RX_NONE,
		.mtu = 9000 - (RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN), /* Jumbo frame MTU */
		.split_hdr_size = 0, /* Header split buffer size */
	},
	.rx_adv_conf = {
		.rss_conf = {
			.rss_key = NULL,
			.rss_key_len = 40,
			.rss_hf = 0,
		},
	},
	.txmode = {
		.mq_mode = RTE_ETH_MQ_TX_NONE,
	},
	.lpbk_mode = 0,
};

#define RETA_CONF_SIZE     (RTE_ETH_RSS_RETA_SIZE_512 / RTE_ETH_RETA_GROUP_SIZE)

static int
rss_setup(uint16_t port_id,
	uint16_t reta_size,
	struct link_params_rss *rss)
{
	struct rte_eth_rss_reta_entry64 reta_conf[RETA_CONF_SIZE];
	uint32_t i;
	int status;

	/* RETA setting */
	memset(reta_conf, 0, sizeof(reta_conf));

	for (i = 0; i < reta_size; i++)
		reta_conf[i / RTE_ETH_RETA_GROUP_SIZE].mask = UINT64_MAX;

	for (i = 0; i < reta_size; i++) {
		uint32_t reta_id = i / RTE_ETH_RETA_GROUP_SIZE;
		uint32_t reta_pos = i % RTE_ETH_RETA_GROUP_SIZE;
		uint32_t rss_qs_pos = i % rss->n_queues;

		reta_conf[reta_id].reta[reta_pos] =
			(uint16_t) rss->queue_id[rss_qs_pos];
	}

	/* RETA update */
	status = rte_eth_dev_rss_reta_update(port_id,
		reta_conf,
		reta_size);

	return status;
}

struct link *
link_create(struct obj *obj, const char *name, struct link_params *params)
{
	struct rte_eth_dev_info port_info;
	struct rte_eth_conf port_conf;
	struct link *link;
	struct link_params_rss *rss;
	struct mempool *mempool;
	uint32_t cpu_id, i;
	int status;
	uint16_t port_id;

	/* Check input params */
	if ((name == NULL) ||
		link_find(obj, name) ||
		(params == NULL) ||
		(params->rx.n_queues == 0) ||
		(params->rx.queue_size == 0) ||
		(params->tx.n_queues == 0) ||
		(params->tx.queue_size == 0))
		return NULL;

	port_id = params->port_id;
	if (params->dev_name) {
		status = rte_eth_dev_get_port_by_name(params->dev_name,
			&port_id);

		if (status)
			return NULL;
	} else
		if (!rte_eth_dev_is_valid_port(port_id))
			return NULL;

	if (rte_eth_dev_info_get(port_id, &port_info) != 0)
		return NULL;

	mempool = mempool_find(obj, params->rx.mempool_name);
	if (mempool == NULL)
		return NULL;

	rss = params->rx.rss;
	if (rss) {
		if ((port_info.reta_size == 0) ||
			(port_info.reta_size > RTE_ETH_RSS_RETA_SIZE_512))
			return NULL;

		if ((rss->n_queues == 0) ||
			(rss->n_queues >= LINK_RXQ_RSS_MAX))
			return NULL;

		for (i = 0; i < rss->n_queues; i++)
			if (rss->queue_id[i] >= port_info.max_rx_queues)
				return NULL;
	}

	/**
	 * Resource create
	 */
	/* Port */
	memcpy(&port_conf, &port_conf_default, sizeof(port_conf));
	if (rss) {
		port_conf.rxmode.mq_mode = RTE_ETH_MQ_RX_RSS;
		port_conf.rx_adv_conf.rss_conf.rss_hf =
			(RTE_ETH_RSS_IP | RTE_ETH_RSS_TCP | RTE_ETH_RSS_UDP) &
			port_info.flow_type_rss_offloads;
	}

	cpu_id = (uint32_t) rte_eth_dev_socket_id(port_id);
	if (cpu_id == (uint32_t) SOCKET_ID_ANY)
		cpu_id = 0;

	status = rte_eth_dev_configure(
		port_id,
		params->rx.n_queues,
		params->tx.n_queues,
		&port_conf);

	if (status < 0)
		return NULL;

	if (params->promiscuous) {
		status = rte_eth_promiscuous_enable(port_id);
		if (status != 0)
			return NULL;
	}

	/* Port RX */
	for (i = 0; i < params->rx.n_queues; i++) {
		status = rte_eth_rx_queue_setup(
			port_id,
			i,
			params->rx.queue_size,
			cpu_id,
			NULL,
			mempool->m);

		if (status < 0)
			return NULL;
	}

	/* Port TX */
	for (i = 0; i < params->tx.n_queues; i++) {
		status = rte_eth_tx_queue_setup(
			port_id,
			i,
			params->tx.queue_size,
			cpu_id,
			NULL);

		if (status < 0)
			return NULL;
	}

	/* Port start */
	status = rte_eth_dev_start(port_id);
	if (status < 0)
		return NULL;

	if (rss) {
		status = rss_setup(port_id, port_info.reta_size, rss);

		if (status) {
			rte_eth_dev_stop(port_id);
			return NULL;
		}
	}

	/* Port link up */
	status = rte_eth_dev_set_link_up(port_id);
	if ((status < 0) && (status != -ENOTSUP)) {
		rte_eth_dev_stop(port_id);
		return NULL;
	}

	/* Node allocation */
	link = calloc(1, sizeof(struct link));
	if (link == NULL) {
		rte_eth_dev_stop(port_id);
		return NULL;
	}

	/* Node fill in */
	strlcpy(link->name, name, sizeof(link->name));
	link->port_id = port_id;
	rte_eth_dev_get_name_by_port(port_id, link->dev_name);
	link->n_rxq = params->rx.n_queues;
	link->n_txq = params->tx.n_queues;

	/* Node add to list */
	TAILQ_INSERT_TAIL(&obj->link_list, link, node);

	return link;
}

int
link_is_up(struct obj *obj, const char *name)
{
	struct rte_eth_link link_params;
	struct link *link;

	/* Check input params */
	if (!obj || !name)
		return 0;

	link = link_find(obj, name);
	if (link == NULL)
		return 0;

	/* Resource */
	if (rte_eth_link_get(link->port_id, &link_params) < 0)
		return 0;

	return (link_params.link_status == RTE_ETH_LINK_DOWN) ? 0 : 1;
}

struct link *
link_find(struct obj *obj, const char *name)
{
	struct link *link;

	if (!obj || !name)
		return NULL;

	TAILQ_FOREACH(link, &obj->link_list, node)
		if (strcmp(link->name, name) == 0)
			return link;

	return NULL;
}

struct link *
link_next(struct obj *obj, struct link *link)
{
	return (link == NULL) ?
		TAILQ_FIRST(&obj->link_list) : TAILQ_NEXT(link, node);
}

/*
 * ring
 */
struct ring *
ring_create(struct obj *obj, const char *name, struct ring_params *params)
{
	struct ring *ring;
	struct rte_ring *r;
	unsigned int flags = RING_F_SP_ENQ | RING_F_SC_DEQ;

	/* Check input params */
	if (!name || ring_find(obj, name) || !params || !params->size)
		return NULL;

	/**
	 * Resource create
	 */
	r = rte_ring_create(
		name,
		params->size,
		params->numa_node,
		flags);
	if (!r)
		return NULL;

	/* Node allocation */
	ring = calloc(1, sizeof(struct ring));
	if (!ring) {
		rte_ring_free(r);
		return NULL;
	}

	/* Node fill in */
	strlcpy(ring->name, name, sizeof(ring->name));

	/* Node add to list */
	TAILQ_INSERT_TAIL(&obj->ring_list, ring, node);

	return ring;
}

struct ring *
ring_find(struct obj *obj, const char *name)
{
	struct ring *ring;

	if (!obj || !name)
		return NULL;

	TAILQ_FOREACH(ring, &obj->ring_list, node)
		if (strcmp(ring->name, name) == 0)
			return ring;

	return NULL;
}

/*
 * tap
 */
#define TAP_DEV		"/dev/net/tun"

struct tap *
tap_find(struct obj *obj, const char *name)
{
	struct tap *tap;

	if (!obj || !name)
		return NULL;

	TAILQ_FOREACH(tap, &obj->tap_list, node)
		if (strcmp(tap->name, name) == 0)
			return tap;

	return NULL;
}

struct tap *
tap_next(struct obj *obj, struct tap *tap)
{
	return (tap == NULL) ?
		TAILQ_FIRST(&obj->tap_list) : TAILQ_NEXT(tap, node);
}

#ifndef RTE_EXEC_ENV_LINUX

struct tap *
tap_create(struct obj *obj __rte_unused, const char *name __rte_unused)
{
	return NULL;
}

#else

struct tap *
tap_create(struct obj *obj, const char *name)
{
	struct tap *tap;
	struct ifreq ifr;
	int fd, status;

	/* Check input params */
	if ((name == NULL) ||
		tap_find(obj, name))
		return NULL;

	/* Resource create */
	fd = open(TAP_DEV, O_RDWR | O_NONBLOCK);
	if (fd < 0)
		return NULL;

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = IFF_TAP | IFF_NO_PI; /* No packet information */
	strlcpy(ifr.ifr_name, name, IFNAMSIZ);

	status = ioctl(fd, TUNSETIFF, (void *) &ifr);
	if (status < 0) {
		close(fd);
		return NULL;
	}

	/* Node allocation */
	tap = calloc(1, sizeof(struct tap));
	if (tap == NULL) {
		close(fd);
		return NULL;
	}
	/* Node fill in */
	strlcpy(tap->name, name, sizeof(tap->name));
	tap->fd = fd;

	/* Node add to list */
	TAILQ_INSERT_TAIL(&obj->tap_list, tap, node);

	return tap;
}

#endif

/*
 * pipeline
 */
#ifndef PIPELINE_MSGQ_SIZE
#define PIPELINE_MSGQ_SIZE                                 64
#endif

struct pipeline *
pipeline_create(struct obj *obj, const char *name, int numa_node)
{
	struct pipeline *pipeline;
	struct rte_swx_pipeline *p = NULL;
	int status;

	/* Check input params */
	if ((name == NULL) ||
		pipeline_find(obj, name))
		return NULL;

	/* Resource create */
	status = rte_swx_pipeline_config(&p, numa_node);
	if (status)
		goto error;

	status = rte_swx_pipeline_port_in_type_register(p,
		"ethdev",
		&rte_swx_port_ethdev_reader_ops);
	if (status)
		goto error;

	status = rte_swx_pipeline_port_out_type_register(p,
		"ethdev",
		&rte_swx_port_ethdev_writer_ops);
	if (status)
		goto error;

	status = rte_swx_pipeline_port_in_type_register(p,
		"ring",
		&rte_swx_port_ring_reader_ops);
	if (status)
		goto error;

	status = rte_swx_pipeline_port_out_type_register(p,
		"ring",
		&rte_swx_port_ring_writer_ops);
	if (status)
		goto error;

#ifdef RTE_PORT_PCAP
	status = rte_swx_pipeline_port_in_type_register(p,
		"source",
		&rte_swx_port_source_ops);
	if (status)
		goto error;
#endif

	status = rte_swx_pipeline_port_out_type_register(p,
		"sink",
		&rte_swx_port_sink_ops);
	if (status)
		goto error;

	status = rte_swx_pipeline_port_in_type_register(p,
		"fd",
		&rte_swx_port_fd_reader_ops);
	if (status)
		goto error;

	status = rte_swx_pipeline_port_out_type_register(p,
		"fd",
		&rte_swx_port_fd_writer_ops);
	if (status)
		goto error;

	status = rte_swx_pipeline_table_type_register(p,
		"exact",
		RTE_SWX_TABLE_MATCH_EXACT,
		&rte_swx_table_exact_match_ops);
	if (status)
		goto error;

	status = rte_swx_pipeline_table_type_register(p,
		"wildcard",
		RTE_SWX_TABLE_MATCH_WILDCARD,
		&rte_swx_table_wildcard_match_ops);
	if (status)
		goto error;

	/* Node allocation */
	pipeline = calloc(1, sizeof(struct pipeline));
	if (pipeline == NULL)
		goto error;

	/* Node fill in */
	strlcpy(pipeline->name, name, sizeof(pipeline->name));
	pipeline->p = p;
	pipeline->timer_period_ms = 10;

	/* Node add to list */
	TAILQ_INSERT_TAIL(&obj->pipeline_list, pipeline, node);

	return pipeline;

error:
	rte_swx_pipeline_free(p);
	return NULL;
}

struct pipeline *
pipeline_find(struct obj *obj, const char *name)
{
	struct pipeline *pipeline;

	if (!obj || !name)
		return NULL;

	TAILQ_FOREACH(pipeline, &obj->pipeline_list, node)
		if (strcmp(name, pipeline->name) == 0)
			return pipeline;

	return NULL;
}

/*
 * obj
 */
struct obj *
obj_init(void)
{
	struct obj *obj;

	obj = calloc(1, sizeof(struct obj));
	if (!obj)
		return NULL;

	TAILQ_INIT(&obj->mempool_list);
	TAILQ_INIT(&obj->link_list);
	TAILQ_INIT(&obj->ring_list);
	TAILQ_INIT(&obj->pipeline_list);
	TAILQ_INIT(&obj->tap_list);

	return obj;
}
