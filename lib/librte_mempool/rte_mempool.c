/*-
 *   BSD LICENSE
 * 
 *   Copyright(c) 2010-2012 Intel Corporation. All rights reserved.
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
 * 
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/queue.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_atomic.h>
#include <rte_launch.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_eal_memconfig.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_ring.h>
#include <rte_errno.h>
#include <rte_string_fns.h>
#include <rte_spinlock.h>

#include "rte_mempool.h"

TAILQ_HEAD(rte_mempool_list, rte_mempool);

#define CACHE_FLUSHTHRESH_MULTIPLIER 1.5

/*
 * return the greatest common divisor between a and b (fast algorithm)
 *
 */
static unsigned get_gcd(unsigned a, unsigned b)
{
	unsigned c;

	if (0 == a)
		return b;
	if (0 == b)
		return a;

	if (a < b) {
		c = a;
		a = b;
		b = c;
	}

	while (b != 0) {
		c = a % b;
		a = b;
		b = c;
	}

	return a;
}

/*
 * Depending on memory configuration, objects addresses are spreaded
 * between channels and ranks in RAM: the pool allocator will add
 * padding between objects. This function return the new size of the
 * object.
 */
static unsigned optimize_object_size(unsigned obj_size)
{
	unsigned nrank, nchan;
	unsigned new_obj_size;

	/* get number of channels */
	nchan = rte_memory_get_nchannel();
	if (nchan == 0)
		nchan = 1;

	nrank = rte_memory_get_nrank();
	if (nrank == 0)
		nrank = 1;

	/* process new object size */
	new_obj_size = (obj_size + CACHE_LINE_MASK) / CACHE_LINE_SIZE;
	while (get_gcd(new_obj_size, nrank * nchan) != 1 ||
			get_gcd(nchan, new_obj_size) != 1)
		new_obj_size++;
	return new_obj_size * CACHE_LINE_SIZE;
}

/* create the mempool */
struct rte_mempool *
rte_mempool_create(const char *name, unsigned n, unsigned elt_size,
		   unsigned cache_size, unsigned private_data_size,
		   rte_mempool_ctor_t *mp_init, void *mp_init_arg,
		   rte_mempool_obj_ctor_t *obj_init, void *obj_init_arg,
		   int socket_id, unsigned flags)
{
	char mz_name[RTE_MEMZONE_NAMESIZE];
	char rg_name[RTE_RING_NAMESIZE];
	struct rte_mempool *mp = NULL;
	struct rte_ring *r;
	const struct rte_memzone *mz;
	uint64_t mempool_size, total_elt_size;
	int mz_flags = RTE_MEMZONE_1GB|RTE_MEMZONE_SIZE_HINT_ONLY;
	int rg_flags = 0;
	uint32_t header_size, trailer_size;
	unsigned i;
	void *obj;

	/* compilation-time checks */
	RTE_BUILD_BUG_ON((sizeof(struct rte_mempool) &
			  CACHE_LINE_MASK) != 0);
#if RTE_MEMPOOL_CACHE_MAX_SIZE > 0
	RTE_BUILD_BUG_ON((sizeof(struct rte_mempool_cache) &
			  CACHE_LINE_MASK) != 0);
	RTE_BUILD_BUG_ON((offsetof(struct rte_mempool, local_cache) &
			  CACHE_LINE_MASK) != 0);
#endif
#ifdef RTE_LIBRTE_MEMPOOL_DEBUG
	RTE_BUILD_BUG_ON((sizeof(struct rte_mempool_debug_stats) &
			  CACHE_LINE_MASK) != 0);
	RTE_BUILD_BUG_ON((offsetof(struct rte_mempool, stats) &
			  CACHE_LINE_MASK) != 0);
#endif

	/* check that we have an initialised tail queue */
	if (RTE_TAILQ_LOOKUP_BY_IDX(RTE_TAILQ_MEMPOOL, rte_mempool_list) == NULL) {
		rte_errno = E_RTE_NO_TAILQ;
		return NULL;	
	}
	
	/* asked cache too big */
	if (cache_size > RTE_MEMPOOL_CACHE_MAX_SIZE){
		rte_errno = EINVAL;
		return NULL;
	}

	/* "no cache align" imply "no spread" */
	if (flags & MEMPOOL_F_NO_CACHE_ALIGN)
		flags |= MEMPOOL_F_NO_SPREAD;

	/* ring flags */
	if (flags & MEMPOOL_F_SP_PUT)
		rg_flags |= RING_F_SP_ENQ;
	if (flags & MEMPOOL_F_SC_GET)
		rg_flags |= RING_F_SC_DEQ;

	rte_rwlock_write_lock(RTE_EAL_MEMPOOL_RWLOCK);

	/* allocate the ring that will be used to store objects */
	/* Ring functions will return appropriate errors if we are
	 * running as a secondary process etc., so no checks made
	 * in this function for that condition */
	rte_snprintf(rg_name, sizeof(rg_name), "MP_%s", name);
	r = rte_ring_create(rg_name, rte_align32pow2(n+1), socket_id, rg_flags);
	if (r == NULL)
		goto exit;

	/*
	 * In header, we have at least the pointer to the pool, and
	 * optionaly a 64 bits cookie.
	 */
	header_size = 0;
	header_size += sizeof(struct rte_mempool *); /* ptr to pool */
#ifdef RTE_LIBRTE_MEMPOOL_DEBUG
	header_size += sizeof(uint64_t); /* cookie */
#endif
	if ((flags & MEMPOOL_F_NO_CACHE_ALIGN) == 0)
		header_size = (header_size + CACHE_LINE_MASK) & (~CACHE_LINE_MASK);

	/* trailer contains the cookie in debug mode */
	trailer_size = 0;
#ifdef RTE_LIBRTE_MEMPOOL_DEBUG
	trailer_size += sizeof(uint64_t); /* cookie */
#endif
	/* element size is 8 bytes-aligned at least */
	elt_size = (elt_size + 7) & (~7);

	/* expand trailer to next cache line */
	if ((flags & MEMPOOL_F_NO_CACHE_ALIGN) == 0) {
		total_elt_size = header_size + elt_size + trailer_size;
		trailer_size += ((CACHE_LINE_SIZE -
				  (total_elt_size & CACHE_LINE_MASK)) &
				 CACHE_LINE_MASK);
	}

	/*
	 * increase trailer to add padding between objects in order to
	 * spread them accross memory channels/ranks
	 */
	if ((flags & MEMPOOL_F_NO_SPREAD) == 0) {
		unsigned new_size;
		new_size = optimize_object_size(header_size + elt_size +
						trailer_size);
		trailer_size = new_size - header_size - elt_size;
	}

	/* this is the size of an object, including header and trailer */
	total_elt_size = header_size + elt_size + trailer_size;

	/* reserve a memory zone for this mempool: private data is
	 * cache-aligned */
	private_data_size = (private_data_size +
			     CACHE_LINE_MASK) & (~CACHE_LINE_MASK);
	mempool_size = total_elt_size * n +
		sizeof(struct rte_mempool) + private_data_size;
	rte_snprintf(mz_name, sizeof(mz_name), "MP_%s", name);

	mz = rte_memzone_reserve(mz_name, mempool_size, socket_id, mz_flags);

	/*
	 * no more memory: in this case we loose previously reserved
	 * space for the as we cannot free it
	 */
	if (mz == NULL)
		goto exit;

	/* init the mempool structure */
	mp = mz->addr;
	memset(mp, 0, sizeof(*mp));
	rte_snprintf(mp->name, sizeof(mp->name), "%s", name);
	mp->phys_addr = mz->phys_addr;
	mp->ring = r;
	mp->size = n;
	mp->flags = flags;
	mp->elt_size = elt_size;
	mp->header_size = header_size;
	mp->trailer_size = trailer_size;
	mp->cache_size = cache_size;
	mp->cache_flushthresh = (uint32_t)(cache_size * CACHE_FLUSHTHRESH_MULTIPLIER);
	mp->private_data_size = private_data_size;

	/* call the initializer */
	if (mp_init)
		mp_init(mp, mp_init_arg);

	/* fill the headers and trailers, and add objects in ring */
	obj = (char *)mp + sizeof(struct rte_mempool) + private_data_size;
	for (i = 0; i < n; i++) {
		struct rte_mempool **mpp;
		obj = (char *)obj + header_size;

		/* set mempool ptr in header */
		mpp = __mempool_from_obj(obj);
		*mpp = mp;

#ifdef RTE_LIBRTE_MEMPOOL_DEBUG
		__mempool_write_header_cookie(obj, 1);
		__mempool_write_trailer_cookie(obj);
#endif
		/* call the initializer */
		if (obj_init)
			obj_init(mp, obj_init_arg, obj, i);

		/* enqueue in ring */
		rte_ring_sp_enqueue(mp->ring, obj);
		obj = (char *)obj + elt_size + trailer_size;
	}

	RTE_EAL_TAILQ_INSERT_TAIL(RTE_TAILQ_MEMPOOL, rte_mempool_list, mp);

exit:
	rte_rwlock_write_unlock(RTE_EAL_MEMPOOL_RWLOCK);

	return mp;
}

/* Return the number of entries in the mempool */
unsigned
rte_mempool_count(const struct rte_mempool *mp)
{
	unsigned count;

	count = rte_ring_count(mp->ring);

#if RTE_MEMPOOL_CACHE_MAX_SIZE > 0
	{
		unsigned lcore_id;
		if (mp->cache_size == 0)
			return count;

		for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++)
			count += mp->local_cache[lcore_id].len;
	}
#endif

	/*
	 * due to race condition (access to len is not locked), the
	 * total can be greater than size... so fix the result
	 */
	if (count > mp->size)
		return mp->size;
	return count;
}

/* dump the cache status */
static unsigned
rte_mempool_dump_cache(const struct rte_mempool *mp)
{
#if RTE_MEMPOOL_CACHE_MAX_SIZE > 0
	unsigned lcore_id;
	unsigned count = 0;
	unsigned cache_count;

	printf("  cache infos:\n");
	printf("    cache_size=%"PRIu32"\n", mp->cache_size);
	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		cache_count = mp->local_cache[lcore_id].len;
		printf("    cache_count[%u]=%u\n", lcore_id, cache_count);
		count += cache_count;
	}
	printf("    total_cache_count=%u\n", count);
	return count;
#else
	RTE_SET_USED(mp);
	printf("  cache disabled\n");
	return 0;
#endif
}

#ifdef RTE_LIBRTE_MEMPOOL_DEBUG
/* check cookies before and after objects */
#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif
static void
mempool_audit_cookies(const struct rte_mempool *mp)
{
	unsigned i;
	void *obj;
	void * const *obj_table;

	obj = (char *)mp + sizeof(struct rte_mempool) + mp->private_data_size;
	for (i = 0; i < mp->size; i++) {
		obj = (char *)obj + mp->header_size;
		obj_table = &obj;
		__mempool_check_cookies(mp, obj_table, 1, 2);
		obj = (char *)obj + mp->elt_size + mp->trailer_size;
	}
}
#ifndef __INTEL_COMPILER
#pragma GCC diagnostic error "-Wcast-qual"
#endif
#else
#define mempool_audit_cookies(mp) do {} while(0)
#endif

#if RTE_MEMPOOL_CACHE_MAX_SIZE > 0
/* check cookies before and after objects */
static void
mempool_audit_cache(const struct rte_mempool *mp)
{
	/* check cache size consistency */
	unsigned lcore_id;
	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		if (mp->local_cache[lcore_id].len > mp->cache_flushthresh) {
			RTE_LOG(CRIT, MEMPOOL, "badness on cache[%u]\n",
				lcore_id);
			rte_panic("MEMPOOL: invalid cache len\n");
		}
	}
}
#else
#define mempool_audit_cache(mp) do {} while(0)
#endif


/* check the consistency of mempool (size, cookies, ...) */
void
rte_mempool_audit(const struct rte_mempool *mp)
{
	mempool_audit_cache(mp);
	mempool_audit_cookies(mp);

	/* For case where mempool DEBUG is not set, and cache size is 0 */
	RTE_SET_USED(mp);
}

/* dump the status of the mempool on the console */
void
rte_mempool_dump(const struct rte_mempool *mp)
{
#ifdef RTE_LIBRTE_MEMPOOL_DEBUG
	struct rte_mempool_debug_stats sum;
	unsigned lcore_id;
#endif
	unsigned common_count;
	unsigned cache_count;

	printf("mempool <%s>@%p\n", mp->name, mp);
	printf("  flags=%x\n", mp->flags);
	printf("  ring=<%s>@%p\n", mp->ring->name, mp->ring);
	printf("  size=%"PRIu32"\n", mp->size);
	printf("  header_size=%"PRIu32"\n", mp->header_size);
	printf("  elt_size=%"PRIu32"\n", mp->elt_size);
	printf("  trailer_size=%"PRIu32"\n", mp->trailer_size);
	printf("  total_obj_size=%"PRIu32"\n",
	       mp->header_size + mp->elt_size + mp->trailer_size);

	cache_count = rte_mempool_dump_cache(mp);
	common_count = rte_ring_count(mp->ring);
	if ((cache_count + common_count) > mp->size)
		common_count = mp->size - cache_count;
	printf("  common_pool_count=%u\n", common_count);

	/* sum and dump statistics */
#ifdef RTE_LIBRTE_MEMPOOL_DEBUG
	memset(&sum, 0, sizeof(sum));
	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		sum.put_bulk += mp->stats[lcore_id].put_bulk;
		sum.put_objs += mp->stats[lcore_id].put_objs;
		sum.get_success_bulk += mp->stats[lcore_id].get_success_bulk;
		sum.get_success_objs += mp->stats[lcore_id].get_success_objs;
		sum.get_fail_bulk += mp->stats[lcore_id].get_fail_bulk;
		sum.get_fail_objs += mp->stats[lcore_id].get_fail_objs;
	}
	printf("  stats:\n");
	printf("    put_bulk=%"PRIu64"\n", sum.put_bulk);
	printf("    put_objs=%"PRIu64"\n", sum.put_objs);
	printf("    get_success_bulk=%"PRIu64"\n", sum.get_success_bulk);
	printf("    get_success_objs=%"PRIu64"\n", sum.get_success_objs);
	printf("    get_fail_bulk=%"PRIu64"\n", sum.get_fail_bulk);
	printf("    get_fail_objs=%"PRIu64"\n", sum.get_fail_objs);
#else
	printf("  no statistics available\n");
#endif

	rte_mempool_audit(mp);
}

/* dump the status of all mempools on the console */
void
rte_mempool_list_dump(void)
{
	const struct rte_mempool *mp = NULL;
	struct rte_mempool_list *mempool_list;

	if ((mempool_list = 
	     RTE_TAILQ_LOOKUP_BY_IDX(RTE_TAILQ_MEMPOOL, rte_mempool_list)) == NULL) {
		rte_errno = E_RTE_NO_TAILQ;
		return;	
	}

	rte_rwlock_read_lock(RTE_EAL_MEMPOOL_RWLOCK);

	TAILQ_FOREACH(mp, mempool_list, next) {
		rte_mempool_dump(mp);
	}

	rte_rwlock_read_unlock(RTE_EAL_MEMPOOL_RWLOCK);
}

/* search a mempool from its name */
struct rte_mempool *
rte_mempool_lookup(const char *name)
{
	struct rte_mempool *mp = NULL;
	struct rte_mempool_list *mempool_list;

	if ((mempool_list = 
	     RTE_TAILQ_LOOKUP_BY_IDX(RTE_TAILQ_MEMPOOL, rte_mempool_list)) == NULL) {
		rte_errno = E_RTE_NO_TAILQ;
		return NULL;
	}

	rte_rwlock_read_lock(RTE_EAL_MEMPOOL_RWLOCK);

	TAILQ_FOREACH(mp, mempool_list, next) {
		if (strncmp(name, mp->name, RTE_MEMPOOL_NAMESIZE) == 0)
			break;
	}
	
	rte_rwlock_read_unlock(RTE_EAL_MEMPOOL_RWLOCK);
	
	if (mp == NULL)
		rte_errno = ENOENT;

	return mp;
}
