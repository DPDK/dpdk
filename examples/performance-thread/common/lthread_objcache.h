/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2015 Intel Corporation. All rights reserved.
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
#ifndef LTHREAD_OBJCACHE_H_
#define LTHREAD_OBJCACHE_H_

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



#endif				/* LTHREAD_OBJCACHE_H_ */
