/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015 Intel Corporation
 */
#ifndef LTHREAD_OBJCACHE_H_
#define LTHREAD_OBJCACHE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>

#include <rte_per_lcore.h>
#include <rte_malloc.h>
#include <rte_memory.h>

#include "lthread_int.h"
#include "lthread_diag.h"
#include "lthread_queue.h"


RTE_DECLARE_PER_LCORE(struct lthread_sched *, this_sched);

struct lthread_objcache {
	struct lthread_queue *q;
	size_t obj_size;
	int prealloc_size;
	char name[LT_MAX_NAME_SIZE];

	DIAG_COUNT_DEFINE(rd);
	DIAG_COUNT_DEFINE(wr);
	DIAG_COUNT_DEFINE(prealloc);
	DIAG_COUNT_DEFINE(capacity);
	DIAG_COUNT_DEFINE(available);
};

/*
 * Create a cache
 */
static inline struct
lthread_objcache *_lthread_objcache_create(const char *name,
					size_t obj_size,
					int prealloc_size)
{
	struct lthread_objcache *c =
	    rte_malloc_socket(NULL, sizeof(struct lthread_objcache),
				RTE_CACHE_LINE_SIZE,
				rte_socket_id());
	if (c == NULL)
		return NULL;

	c->q = _lthread_queue_create("cache queue");
	if (c->q == NULL) {
		rte_free(c);
		return NULL;
	}
	c->obj_size = obj_size;
	c->prealloc_size = prealloc_size;

	if (name != NULL)
		strncpy(c->name, name, LT_MAX_NAME_SIZE);
	c->name[sizeof(c->name)-1] = 0;

	DIAG_COUNT_INIT(c, rd);
	DIAG_COUNT_INIT(c, wr);
	DIAG_COUNT_INIT(c, prealloc);
	DIAG_COUNT_INIT(c, capacity);
	DIAG_COUNT_INIT(c, available);
	return c;
}

/*
 * Destroy an objcache
 */
static inline int
_lthread_objcache_destroy(struct lthread_objcache *c)
{
	if (_lthread_queue_destroy(c->q) == 0) {
		rte_free(c);
		return 0;
	}
	return -1;
}

/*
 * Allocate an object from an object cache
 */
static inline void *
_lthread_objcache_alloc(struct lthread_objcache *c)
{
	int i;
	void *data;
	struct lthread_queue *q = c->q;
	size_t obj_size = c->obj_size;
	int prealloc_size = c->prealloc_size;

	data = _lthread_queue_remove(q);

	if (data == NULL) {
		DIAG_COUNT_INC(c, prealloc);
		for (i = 0; i < prealloc_size; i++) {
			data =
			    rte_zmalloc_socket(NULL, obj_size,
					RTE_CACHE_LINE_SIZE,
					rte_socket_id());
			if (data == NULL)
				return NULL;

			DIAG_COUNT_INC(c, available);
			DIAG_COUNT_INC(c, capacity);
			_lthread_queue_insert_mp(q, data);
		}
		data = _lthread_queue_remove(q);
	}
	DIAG_COUNT_INC(c, rd);
	DIAG_COUNT_DEC(c, available);
	return data;
}

/*
 * free an object to a cache
 */
static inline void
_lthread_objcache_free(struct lthread_objcache *c, void *obj)
{
	DIAG_COUNT_INC(c, wr);
	DIAG_COUNT_INC(c, available);
	_lthread_queue_insert_mp(c->q, obj);
}


#ifdef __cplusplus
}
#endif

#endif				/* LTHREAD_OBJCACHE_H_ */
