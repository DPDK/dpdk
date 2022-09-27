/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#ifndef _INCLUDE_OBJ_H_
#define _INCLUDE_OBJ_H_

#include <stdint.h>
#include <sys/queue.h>

#include <rte_mempool.h>
#include <rte_swx_pipeline.h>
#include <rte_swx_ctl.h>

#ifndef NAME_SIZE
#define NAME_SIZE 64
#endif

/*
 * obj
 */
struct obj;

struct obj *
obj_init(void);

/*
 * mempool
 */
struct mempool_params {
	uint32_t buffer_size;
	uint32_t pool_size;
	uint32_t cache_size;
	uint32_t cpu_id;
};

struct mempool {
	TAILQ_ENTRY(mempool) node;
	char name[NAME_SIZE];
	struct rte_mempool *m;
	uint32_t buffer_size;
};

struct mempool *
mempool_create(struct obj *obj,
	       const char *name,
	       struct mempool_params *params);

struct mempool *
mempool_find(struct obj *obj,
	     const char *name);

/*
 * link
 */
#ifndef LINK_RXQ_RSS_MAX
#define LINK_RXQ_RSS_MAX                                   16
#endif

struct link_params_rss {
	uint32_t queue_id[LINK_RXQ_RSS_MAX];
	uint32_t n_queues;
};

struct link_params {
	struct {
		uint32_t n_queues;
		uint32_t queue_size;
		const char *mempool_name;
		struct link_params_rss *rss;
	} rx;

	struct {
		uint32_t n_queues;
		uint32_t queue_size;
	} tx;

	int promiscuous;
};

struct link {
	TAILQ_ENTRY(link) node;
	char name[NAME_SIZE];
	uint16_t port_id;
	uint32_t n_rxq;
	uint32_t n_txq;
};

struct link *
link_create(struct obj *obj,
	    const char *name,
	    struct link_params *params);

int
link_is_up(struct obj *obj, const char *name);

struct link *
link_find(struct obj *obj, const char *name);

struct link *
link_next(struct obj *obj, struct link *link);

/*
 * ring
 */
struct ring_params {
	uint32_t size;
	uint32_t numa_node;
};

struct ring {
	TAILQ_ENTRY(ring) node;
	char name[NAME_SIZE];
};

struct ring *
ring_create(struct obj *obj,
	   const char *name,
	   struct ring_params *params);

struct ring *
ring_find(struct obj *obj, const char *name);

#endif /* _INCLUDE_OBJ_H_ */
