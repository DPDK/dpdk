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

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <sys/queue.h>

#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_eal_memconfig.h>
#include <rte_per_lcore.h>
#include <rte_errno.h>
#include <rte_string_fns.h>
#include <rte_common.h>

#include "malloc_heap.h"
#include "malloc_elem.h"
#include "eal_private.h"

static inline const struct rte_memzone *
memzone_lookup_thread_unsafe(const char *name)
{
	const struct rte_mem_config *mcfg;
	const struct rte_memzone *mz;
	unsigned i = 0;

	/* get pointer to global configuration */
	mcfg = rte_eal_get_configuration()->mem_config;

	/*
	 * the algorithm is not optimal (linear), but there are few
	 * zones and this function should be called at init only
	 */
	for (i = 0; i < RTE_MAX_MEMZONE; i++) {
		mz = &mcfg->memzone[i];
		if (mz->addr != NULL && !strncmp(name, mz->name, RTE_MEMZONE_NAMESIZE))
			return &mcfg->memzone[i];
	}

	return NULL;
}

static inline struct rte_memzone *
get_next_free_memzone(void)
{
	struct rte_mem_config *mcfg;
	unsigned i = 0;

	/* get pointer to global configuration */
	mcfg = rte_eal_get_configuration()->mem_config;

	for (i = 0; i < RTE_MAX_MEMZONE; i++) {
		if (mcfg->memzone[i].addr == NULL)
			return &mcfg->memzone[i];
	}

	return NULL;
}

/* This function will return the greatest free block if a heap has been
 * specified. If no heap has been specified, it will return the heap and
 * length of the greatest free block available in all heaps */
static size_t
find_heap_max_free_elem(int *s, unsigned align)
{
	struct rte_mem_config *mcfg;
	struct rte_malloc_socket_stats stats;
	int i, socket = *s;
	size_t len = 0;

	/* get pointer to global configuration */
	mcfg = rte_eal_get_configuration()->mem_config;

	for (i = 0; i < RTE_MAX_NUMA_NODES; i++) {
		if ((socket != SOCKET_ID_ANY) && (socket != i))
			continue;

		malloc_heap_get_stats(&mcfg->malloc_heaps[i], &stats);
		if (stats.greatest_free_size > len) {
			len = stats.greatest_free_size;
			*s = i;
		}
	}

	if (len < MALLOC_ELEM_OVERHEAD + align)
		return 0;

	return len - MALLOC_ELEM_OVERHEAD - align;
}

static const struct rte_memzone *
memzone_reserve_aligned_thread_unsafe(const char *name, size_t len,
		int socket_id, unsigned flags, unsigned align, unsigned bound)
{
	struct rte_memzone *mz;
	struct rte_mem_config *mcfg;
	size_t requested_len;
	int socket, i;

	/* get pointer to global configuration */
	mcfg = rte_eal_get_configuration()->mem_config;

	/* no more room in config */
	if (mcfg->memzone_cnt >= RTE_MAX_MEMZONE) {
		RTE_LOG(ERR, EAL, "%s(): No more room in config\n", __func__);
		rte_errno = ENOSPC;
		return NULL;
	}

	if (strlen(name) > sizeof(mz->name) - 1) {
		RTE_LOG(DEBUG, EAL, "%s(): memzone <%s>: name too long\n",
			__func__, name);
		rte_errno = ENAMETOOLONG;
		return NULL;
	}

	/* zone already exist */
	if ((memzone_lookup_thread_unsafe(name)) != NULL) {
		RTE_LOG(DEBUG, EAL, "%s(): memzone <%s> already exists\n",
			__func__, name);
		rte_errno = EEXIST;
		return NULL;
	}

	/* if alignment is not a power of two */
	if (align && !rte_is_power_of_2(align)) {
		RTE_LOG(ERR, EAL, "%s(): Invalid alignment: %u\n", __func__,
				align);
		rte_errno = EINVAL;
		return NULL;
	}

	/* alignment less than cache size is not allowed */
	if (align < RTE_CACHE_LINE_SIZE)
		align = RTE_CACHE_LINE_SIZE;

	/* align length on cache boundary. Check for overflow before doing so */
	if (len > SIZE_MAX - RTE_CACHE_LINE_MASK) {
		rte_errno = EINVAL; /* requested size too big */
		return NULL;
	}

	len += RTE_CACHE_LINE_MASK;
	len &= ~((size_t) RTE_CACHE_LINE_MASK);

	/* save minimal requested  length */
	requested_len = RTE_MAX((size_t)RTE_CACHE_LINE_SIZE,  len);

	/* check that boundary condition is valid */
	if (bound != 0 && (requested_len > bound || !rte_is_power_of_2(bound))) {
		rte_errno = EINVAL;
		return NULL;
	}

	if ((socket_id != SOCKET_ID_ANY) &&
	    (socket_id >= RTE_MAX_NUMA_NODES || socket_id < 0)) {
		rte_errno = EINVAL;
		return NULL;
	}

	if (!rte_eal_has_hugepages())
		socket_id = SOCKET_ID_ANY;

	if (len == 0) {
		if (bound != 0)
			requested_len = bound;
		else {
			requested_len = find_heap_max_free_elem(&socket_id, align);
			if (requested_len == 0) {
				rte_errno = ENOMEM;
				return NULL;
			}
		}
	}

	if (socket_id == SOCKET_ID_ANY)
		socket = malloc_get_numa_socket();
	else
		socket = socket_id;

	/* allocate memory on heap */
	void *mz_addr = malloc_heap_alloc(&mcfg->malloc_heaps[socket], NULL,
			requested_len, flags, align, bound);

	if ((mz_addr == NULL) && (socket_id == SOCKET_ID_ANY)) {
		/* try other heaps */
		for (i = 0; i < RTE_MAX_NUMA_NODES; i++) {
			if (socket == i)
				continue;

			mz_addr = malloc_heap_alloc(&mcfg->malloc_heaps[i],
					NULL, requested_len, flags, align, bound);
			if (mz_addr != NULL)
				break;
		}
	}

	if (mz_addr == NULL) {
		rte_errno = ENOMEM;
		return NULL;
	}

	const struct malloc_elem *elem = malloc_elem_from_data(mz_addr);

	/* fill the zone in config */
	mz = get_next_free_memzone();

	if (mz == NULL) {
		RTE_LOG(ERR, EAL, "%s(): Cannot find free memzone but there is room "
				"in config!\n", __func__);
		rte_errno = ENOSPC;
		return NULL;
	}

	mcfg->memzone_cnt++;
	snprintf(mz->name, sizeof(mz->name), "%s", name);
	mz->phys_addr = rte_malloc_virt2phy(mz_addr);
	mz->addr = mz_addr;
	mz->len = (requested_len == 0 ? elem->size : requested_len);
	mz->hugepage_sz = elem->ms->hugepage_sz;
	mz->socket_id = elem->ms->socket_id;
	mz->flags = 0;
	mz->memseg_id = elem->ms - rte_eal_get_configuration()->mem_config->memseg;

	return mz;
}

static const struct rte_memzone *
rte_memzone_reserve_thread_safe(const char *name, size_t len,
				int socket_id, unsigned flags, unsigned align,
				unsigned bound)
{
	struct rte_mem_config *mcfg;
	const struct rte_memzone *mz = NULL;

	/* get pointer to global configuration */
	mcfg = rte_eal_get_configuration()->mem_config;

	rte_rwlock_write_lock(&mcfg->mlock);

	mz = memzone_reserve_aligned_thread_unsafe(
		name, len, socket_id, flags, align, bound);

	rte_rwlock_write_unlock(&mcfg->mlock);

	return mz;
}

/*
 * Return a pointer to a correctly filled memzone descriptor (with a
 * specified alignment and boundary). If the allocation cannot be done,
 * return NULL.
 */
const struct rte_memzone *
rte_memzone_reserve_bounded(const char *name, size_t len, int socket_id,
			    unsigned flags, unsigned align, unsigned bound)
{
	return rte_memzone_reserve_thread_safe(name, len, socket_id, flags,
					       align, bound);
}

/*
 * Return a pointer to a correctly filled memzone descriptor (with a
 * specified alignment). If the allocation cannot be done, return NULL.
 */
const struct rte_memzone *
rte_memzone_reserve_aligned(const char *name, size_t len, int socket_id,
			    unsigned flags, unsigned align)
{
	return rte_memzone_reserve_thread_safe(name, len, socket_id, flags,
					       align, 0);
}

/*
 * Return a pointer to a correctly filled memzone descriptor. If the
 * allocation cannot be done, return NULL.
 */
const struct rte_memzone *
rte_memzone_reserve(const char *name, size_t len, int socket_id,
		    unsigned flags)
{
	return rte_memzone_reserve_thread_safe(name, len, socket_id,
					       flags, RTE_CACHE_LINE_SIZE, 0);
}

int
rte_memzone_free(const struct rte_memzone *mz)
{
	struct rte_mem_config *mcfg;
	int ret = 0;
	void *addr;
	unsigned idx;

	if (mz == NULL)
		return -EINVAL;

	mcfg = rte_eal_get_configuration()->mem_config;

	rte_rwlock_write_lock(&mcfg->mlock);

	idx = ((uintptr_t)mz - (uintptr_t)mcfg->memzone);
	idx = idx / sizeof(struct rte_memzone);

	addr = mcfg->memzone[idx].addr;
	if (addr == NULL)
		ret = -EINVAL;
	else if (mcfg->memzone_cnt == 0) {
		rte_panic("%s(): memzone address not NULL but memzone_cnt is 0!\n",
				__func__);
	} else {
		memset(&mcfg->memzone[idx], 0, sizeof(mcfg->memzone[idx]));
		mcfg->memzone_cnt--;
	}

	rte_rwlock_write_unlock(&mcfg->mlock);

	rte_free(addr);

	return ret;
}

/*
 * Lookup for the memzone identified by the given name
 */
const struct rte_memzone *
rte_memzone_lookup(const char *name)
{
	struct rte_mem_config *mcfg;
	const struct rte_memzone *memzone = NULL;

	mcfg = rte_eal_get_configuration()->mem_config;

	rte_rwlock_read_lock(&mcfg->mlock);

	memzone = memzone_lookup_thread_unsafe(name);

	rte_rwlock_read_unlock(&mcfg->mlock);

	return memzone;
}

/* Dump all reserved memory zones on console */
void
rte_memzone_dump(FILE *f)
{
	struct rte_mem_config *mcfg;
	unsigned i = 0;

	/* get pointer to global configuration */
	mcfg = rte_eal_get_configuration()->mem_config;

	rte_rwlock_read_lock(&mcfg->mlock);
	/* dump all zones */
	for (i=0; i<RTE_MAX_MEMZONE; i++) {
		if (mcfg->memzone[i].addr == NULL)
			break;
		fprintf(f, "Zone %u: name:<%s>, phys:0x%"PRIx64", len:0x%zx"
		       ", virt:%p, socket_id:%"PRId32", flags:%"PRIx32"\n", i,
		       mcfg->memzone[i].name,
		       mcfg->memzone[i].phys_addr,
		       mcfg->memzone[i].len,
		       mcfg->memzone[i].addr,
		       mcfg->memzone[i].socket_id,
		       mcfg->memzone[i].flags);
	}
	rte_rwlock_read_unlock(&mcfg->mlock);
}

/*
 * Init the memzone subsystem
 */
int
rte_eal_memzone_init(void)
{
	struct rte_mem_config *mcfg;
	const struct rte_memseg *memseg;

	/* get pointer to global configuration */
	mcfg = rte_eal_get_configuration()->mem_config;

	/* secondary processes don't need to initialise anything */
	if (rte_eal_process_type() == RTE_PROC_SECONDARY)
		return 0;

	memseg = rte_eal_get_physmem_layout();
	if (memseg == NULL) {
		RTE_LOG(ERR, EAL, "%s(): Cannot get physical layout\n", __func__);
		return -1;
	}

	rte_rwlock_write_lock(&mcfg->mlock);

	/* delete all zones */
	mcfg->memzone_cnt = 0;
	memset(mcfg->memzone, 0, sizeof(mcfg->memzone));

	rte_rwlock_write_unlock(&mcfg->mlock);

	return rte_eal_malloc_heap_init();
}

/* Walk all reserved memory zones */
void rte_memzone_walk(void (*func)(const struct rte_memzone *, void *),
		      void *arg)
{
	struct rte_mem_config *mcfg;
	unsigned i;

	mcfg = rte_eal_get_configuration()->mem_config;

	rte_rwlock_read_lock(&mcfg->mlock);
	for (i=0; i<RTE_MAX_MEMZONE; i++) {
		if (mcfg->memzone[i].addr != NULL)
			(*func)(&mcfg->memzone[i], arg);
	}
	rte_rwlock_read_unlock(&mcfg->mlock);
}
