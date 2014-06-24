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

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
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
 * Depending on memory configuration, objects addresses are spread
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
	while (get_gcd(new_obj_size, nrank * nchan) != 1)
		new_obj_size++;
	return new_obj_size * CACHE_LINE_SIZE;
}

static void
mempool_add_elem(struct rte_mempool *mp, void *obj, uint32_t obj_idx,
	rte_mempool_obj_ctor_t *obj_init, void *obj_init_arg)
{
	struct rte_mempool **mpp;

	obj = (char *)obj + mp->header_size;

	/* set mempool ptr in header */
	mpp = __mempool_from_obj(obj);
	*mpp = mp;

#ifdef RTE_LIBRTE_MEMPOOL_DEBUG
	__mempool_write_header_cookie(obj, 1);
	__mempool_write_trailer_cookie(obj);
#endif
	/* call the initializer */
	if (obj_init)
		obj_init(mp, obj_init_arg, obj, obj_idx);

	/* enqueue in ring */
	rte_ring_sp_enqueue(mp->ring, obj);
}

uint32_t
rte_mempool_obj_iter(void *vaddr, uint32_t elt_num, size_t elt_sz, size_t align,
	const phys_addr_t paddr[], uint32_t pg_num, uint32_t pg_shift,
	rte_mempool_obj_iter_t obj_iter, void *obj_iter_arg)
{
	uint32_t i, j, k;
	uint32_t pgn;
	uintptr_t end, start, va;
	uintptr_t pg_sz;

	pg_sz = (uintptr_t)1 << pg_shift;
	va = (uintptr_t)vaddr;

	i = 0;
	j = 0;

	while (i != elt_num && j != pg_num) {

		start = RTE_ALIGN_CEIL(va, align);
		end = start + elt_sz;

		pgn = (end >> pg_shift) - (start >> pg_shift);
		pgn += j;

		/* do we have enough space left for the next element. */
		if (pgn >= pg_num)
			break;

		for (k = j;
				k != pgn &&
				paddr[k] + pg_sz == paddr[k + 1];
				k++)
			;

		/*
		 * if next pgn chunks of memory physically continuous,
		 * use it to create next element.
		 * otherwise, just skip that chunk unused.
		 */
		if (k == pgn) {
			if (obj_iter != NULL)
				obj_iter(obj_iter_arg, (void *)start,
					(void *)end, i);
			va = end;
			j = pgn;
			i++;
		} else {
			va = RTE_ALIGN_CEIL((va + 1), pg_sz);
			j++;
		}
	}

	return (i);
}

/*
 * Populate  mempool with the objects.
 */

struct mempool_populate_arg {
	struct rte_mempool     *mp;
	rte_mempool_obj_ctor_t *obj_init;
	void                   *obj_init_arg;
};

static void
mempool_obj_populate(void *arg, void *start, void *end, uint32_t idx)
{
	struct mempool_populate_arg *pa = arg;

	mempool_add_elem(pa->mp, start, idx, pa->obj_init, pa->obj_init_arg);
	pa->mp->elt_va_end = (uintptr_t)end;
}

static void
mempool_populate(struct rte_mempool *mp, size_t num, size_t align,
	rte_mempool_obj_ctor_t *obj_init, void *obj_init_arg)
{
	uint32_t elt_sz;
	struct mempool_populate_arg arg;

	elt_sz = mp->elt_size + mp->header_size + mp->trailer_size;
	arg.mp = mp;
	arg.obj_init = obj_init;
	arg.obj_init_arg = obj_init_arg;

	mp->size = rte_mempool_obj_iter((void *)mp->elt_va_start,
		num, elt_sz, align,
		mp->elt_pa, mp->pg_num, mp->pg_shift,
		mempool_obj_populate, &arg);
}

uint32_t
rte_mempool_calc_obj_size(uint32_t elt_size, uint32_t flags,
	struct rte_mempool_objsz *sz)
{
	struct rte_mempool_objsz lsz;

	sz = (sz != NULL) ? sz : &lsz;

	/*
	 * In header, we have at least the pointer to the pool, and
	 * optionaly a 64 bits cookie.
	 */
	sz->header_size = 0;
	sz->header_size += sizeof(struct rte_mempool *); /* ptr to pool */
#ifdef RTE_LIBRTE_MEMPOOL_DEBUG
	sz->header_size += sizeof(uint64_t); /* cookie */
#endif
	if ((flags & MEMPOOL_F_NO_CACHE_ALIGN) == 0)
		sz->header_size = RTE_ALIGN_CEIL(sz->header_size,
			CACHE_LINE_SIZE);

	/* trailer contains the cookie in debug mode */
	sz->trailer_size = 0;
#ifdef RTE_LIBRTE_MEMPOOL_DEBUG
	sz->trailer_size += sizeof(uint64_t); /* cookie */
#endif
	/* element size is 8 bytes-aligned at least */
	sz->elt_size = RTE_ALIGN_CEIL(elt_size, sizeof(uint64_t));

	/* expand trailer to next cache line */
	if ((flags & MEMPOOL_F_NO_CACHE_ALIGN) == 0) {
		sz->total_size = sz->header_size + sz->elt_size +
			sz->trailer_size;
		sz->trailer_size += ((CACHE_LINE_SIZE -
				  (sz->total_size & CACHE_LINE_MASK)) &
				 CACHE_LINE_MASK);
	}

	/*
	 * increase trailer to add padding between objects in order to
	 * spread them across memory channels/ranks
	 */
	if ((flags & MEMPOOL_F_NO_SPREAD) == 0) {
		unsigned new_size;
		new_size = optimize_object_size(sz->header_size + sz->elt_size +
			sz->trailer_size);
		sz->trailer_size = new_size - sz->header_size - sz->elt_size;
	}

	if (! rte_eal_has_hugepages()) {
		/*
		 * compute trailer size so that pool elements fit exactly in
		 * a standard page
		 */
		int page_size = getpagesize();
		int new_size = page_size - sz->header_size - sz->elt_size;
		if (new_size < 0 || (unsigned int)new_size < sz->trailer_size) {
			printf("When hugepages are disabled, pool objects "
			       "can't exceed PAGE_SIZE: %d + %d + %d > %d\n",
			       sz->header_size, sz->elt_size, sz->trailer_size,
			       page_size);
			return 0;
		}
		sz->trailer_size = new_size;
	}

	/* this is the size of an object, including header and trailer */
	sz->total_size = sz->header_size + sz->elt_size + sz->trailer_size;

	return (sz->total_size);
}


/*
 * Calculate maximum amount of memory required to store given number of objects.
 */
size_t
rte_mempool_xmem_size(uint32_t elt_num, size_t elt_sz, uint32_t pg_shift)
{
	size_t n, pg_num, pg_sz, sz;

	pg_sz = (size_t)1 << pg_shift;

	if ((n = pg_sz / elt_sz) > 0) {
		pg_num = (elt_num + n - 1) / n;
		sz = pg_num << pg_shift;
	} else {
		sz = RTE_ALIGN_CEIL(elt_sz, pg_sz) * elt_num;
	}

	return (sz);
}

/*
 * Calculate how much memory would be actually required with the
 * given memory footprint to store required number of elements.
 */
static void
mempool_lelem_iter(void *arg, __rte_unused void *start, void *end,
        __rte_unused uint32_t idx)
{
        *(uintptr_t *)arg = (uintptr_t)end;
}

ssize_t
rte_mempool_xmem_usage(void *vaddr, uint32_t elt_num, size_t elt_sz,
	const phys_addr_t paddr[], uint32_t pg_num, uint32_t pg_shift)
{
	uint32_t n;
	uintptr_t va, uv;
	size_t pg_sz, usz;

	pg_sz = (size_t)1 << pg_shift;
	va = (uintptr_t)vaddr;
	uv = va;

	if ((n = rte_mempool_obj_iter(vaddr, elt_num, elt_sz, 1,
			paddr, pg_num, pg_shift, mempool_lelem_iter,
			&uv)) != elt_num) {
		return (-n);
	}

	uv = RTE_ALIGN_CEIL(uv, pg_sz);
	usz = uv - va;
	return (usz);
}

/* create the mempool */
struct rte_mempool *
rte_mempool_create(const char *name, unsigned n, unsigned elt_size,
		   unsigned cache_size, unsigned private_data_size,
		   rte_mempool_ctor_t *mp_init, void *mp_init_arg,
		   rte_mempool_obj_ctor_t *obj_init, void *obj_init_arg,
		   int socket_id, unsigned flags)
{
#ifdef RTE_LIBRTE_XEN_DOM0
	return (rte_dom0_mempool_create(name, n, elt_size,
		cache_size, private_data_size,
		mp_init, mp_init_arg,
		obj_init, obj_init_arg,
		socket_id, flags));
#else
	return (rte_mempool_xmem_create(name, n, elt_size,
		cache_size, private_data_size,
		mp_init, mp_init_arg,
		obj_init, obj_init_arg,
		socket_id, flags,
		NULL, NULL, MEMPOOL_PG_NUM_DEFAULT, MEMPOOL_PG_SHIFT_MAX));
#endif
}

/*
 * Create the mempool over already allocated chunk of memory.
 * That external memory buffer can consists of physically disjoint pages.
 * Setting vaddr to NULL, makes mempool to fallback to original behaviour
 * and allocate space for mempool and it's elements as one big chunk of
 * physically continuos memory.
 * */
struct rte_mempool *
rte_mempool_xmem_create(const char *name, unsigned n, unsigned elt_size,
		unsigned cache_size, unsigned private_data_size,
		rte_mempool_ctor_t *mp_init, void *mp_init_arg,
		rte_mempool_obj_ctor_t *obj_init, void *obj_init_arg,
		int socket_id, unsigned flags, void *vaddr,
		const phys_addr_t paddr[], uint32_t pg_num, uint32_t pg_shift)
{
	char mz_name[RTE_MEMZONE_NAMESIZE];
	char rg_name[RTE_RING_NAMESIZE];
	struct rte_mempool *mp = NULL;
	struct rte_ring *r;
	const struct rte_memzone *mz;
	size_t mempool_size;
	int mz_flags = RTE_MEMZONE_1GB|RTE_MEMZONE_SIZE_HINT_ONLY;
	int rg_flags = 0;
	void *obj;
	struct rte_mempool_objsz objsz;
	void *startaddr;
	int page_size = getpagesize();

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
	if (RTE_TAILQ_LOOKUP_BY_IDX(RTE_TAILQ_MEMPOOL,
			rte_mempool_list) == NULL) {
		rte_errno = E_RTE_NO_TAILQ;
		return NULL;
	}

	/* asked cache too big */
	if (cache_size > RTE_MEMPOOL_CACHE_MAX_SIZE) {
		rte_errno = EINVAL;
		return NULL;
	}

	/* check that we have both VA and PA */
	if (vaddr != NULL && paddr == NULL) {
		rte_errno = EINVAL;
		return NULL;
	}

	/* Check that pg_num and pg_shift parameters are valid. */
	if (pg_num < RTE_DIM(mp->elt_pa) || pg_shift > MEMPOOL_PG_SHIFT_MAX) {
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

	/* calculate mempool object sizes. */
	if (!rte_mempool_calc_obj_size(elt_size, flags, &objsz)) {
		rte_errno = EINVAL;
		return NULL;
	}

	rte_rwlock_write_lock(RTE_EAL_MEMPOOL_RWLOCK);

	/* allocate the ring that will be used to store objects */
	/* Ring functions will return appropriate errors if we are
	 * running as a secondary process etc., so no checks made
	 * in this function for that condition */
	snprintf(rg_name, sizeof(rg_name), RTE_MEMPOOL_MZ_FORMAT, name);
	r = rte_ring_create(rg_name, rte_align32pow2(n+1), socket_id, rg_flags);
	if (r == NULL)
		goto exit;

	/*
	 * reserve a memory zone for this mempool: private data is
	 * cache-aligned
	 */
	private_data_size = (private_data_size +
			     CACHE_LINE_MASK) & (~CACHE_LINE_MASK);

	if (! rte_eal_has_hugepages()) {
		/*
		 * expand private data size to a whole page, so that the
		 * first pool element will start on a new standard page
		 */
		int head = sizeof(struct rte_mempool);
		int new_size = (private_data_size + head) % page_size;
		if (new_size) {
			private_data_size += page_size - new_size;
		}
	}

	/*
	 * If user provided an external memory buffer, then use it to
	 * store mempool objects. Otherwise reserve memzone big enough to
	 * hold mempool header and metadata plus mempool objects.
	 */
	mempool_size = MEMPOOL_HEADER_SIZE(mp, pg_num) + private_data_size;
	if (vaddr == NULL)
		mempool_size += (size_t)objsz.total_size * n;

	if (! rte_eal_has_hugepages()) {
		/*
		 * we want the memory pool to start on a page boundary,
		 * because pool elements crossing page boundaries would
		 * result in discontiguous physical addresses
		 */
		mempool_size += page_size;
	}

	snprintf(mz_name, sizeof(mz_name), RTE_MEMPOOL_MZ_FORMAT, name);

	mz = rte_memzone_reserve(mz_name, mempool_size, socket_id, mz_flags);

	/*
	 * no more memory: in this case we loose previously reserved
	 * space for the as we cannot free it
	 */
	if (mz == NULL)
		goto exit;

	if (rte_eal_has_hugepages()) {
		startaddr = (void*)mz->addr;
	} else {
		/* align memory pool start address on a page boundary */
		unsigned long addr = (unsigned long)mz->addr;
		if (addr & (page_size - 1)) {
			addr += page_size;
			addr &= ~(page_size - 1);
		}
		startaddr = (void*)addr;
	}

	/* init the mempool structure */
	mp = startaddr;
	memset(mp, 0, sizeof(*mp));
	snprintf(mp->name, sizeof(mp->name), "%s", name);
	mp->phys_addr = mz->phys_addr;
	mp->ring = r;
	mp->size = n;
	mp->flags = flags;
	mp->elt_size = objsz.elt_size;
	mp->header_size = objsz.header_size;
	mp->trailer_size = objsz.trailer_size;
	mp->cache_size = cache_size;
	mp->cache_flushthresh = (uint32_t)
		(cache_size * CACHE_FLUSHTHRESH_MULTIPLIER);
	mp->private_data_size = private_data_size;

	/* calculate address of the first element for continuous mempool. */
	obj = (char *)mp + MEMPOOL_HEADER_SIZE(mp, pg_num) +
		private_data_size;

	/* populate address translation fields. */
	mp->pg_num = pg_num;
	mp->pg_shift = pg_shift;
	mp->pg_mask = RTE_LEN2MASK(mp->pg_shift, typeof(mp->pg_mask));

	/* mempool elements allocated together with mempool */
	if (vaddr == NULL) {
		mp->elt_va_start = (uintptr_t)obj;
		mp->elt_pa[0] = mp->phys_addr +
			(mp->elt_va_start - (uintptr_t)mp);

	/* mempool elements in a separate chunk of memory. */
	} else {
		mp->elt_va_start = (uintptr_t)vaddr;
		memcpy(mp->elt_pa, paddr, sizeof (mp->elt_pa[0]) * pg_num);
	}

	mp->elt_va_end = mp->elt_va_start;

	/* call the initializer */
	if (mp_init)
		mp_init(mp, mp_init_arg);

	mempool_populate(mp, n, 1, obj_init, obj_init_arg);

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
rte_mempool_dump_cache(FILE *f, const struct rte_mempool *mp)
{
#if RTE_MEMPOOL_CACHE_MAX_SIZE > 0
	unsigned lcore_id;
	unsigned count = 0;
	unsigned cache_count;

	fprintf(f, "  cache infos:\n");
	fprintf(f, "    cache_size=%"PRIu32"\n", mp->cache_size);
	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		cache_count = mp->local_cache[lcore_id].len;
		fprintf(f, "    cache_count[%u]=%u\n", lcore_id, cache_count);
		count += cache_count;
	}
	fprintf(f, "    total_cache_count=%u\n", count);
	return count;
#else
	RTE_SET_USED(mp);
	fprintf(f, "  cache disabled\n");
	return 0;
#endif
}

#ifdef RTE_LIBRTE_MEMPOOL_DEBUG
/* check cookies before and after objects */
#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

struct mempool_audit_arg {
	const struct rte_mempool *mp;
	uintptr_t obj_end;
	uint32_t obj_num;
};

static void
mempool_obj_audit(void *arg, void *start, void *end, uint32_t idx)
{
	struct mempool_audit_arg *pa = arg;
	void *obj;

	obj = (char *)start + pa->mp->header_size;
	pa->obj_end = (uintptr_t)end;
	pa->obj_num = idx + 1;
	__mempool_check_cookies(pa->mp, &obj, 1, 2);
}

static void
mempool_audit_cookies(const struct rte_mempool *mp)
{
	uint32_t elt_sz, num;
	struct mempool_audit_arg arg;

	elt_sz = mp->elt_size + mp->header_size + mp->trailer_size;

	arg.mp = mp;
	arg.obj_end = mp->elt_va_start;
	arg.obj_num = 0;

	num = rte_mempool_obj_iter((void *)mp->elt_va_start,
		mp->size, elt_sz, 1,
		mp->elt_pa, mp->pg_num, mp->pg_shift,
		mempool_obj_audit, &arg);

	if (num != mp->size) {
			rte_panic("rte_mempool_obj_iter(mempool=%p, size=%u) "
			"iterated only over %u elements\n",
			mp, mp->size, num);
	} else if (arg.obj_end != mp->elt_va_end || arg.obj_num != mp->size) {
			rte_panic("rte_mempool_obj_iter(mempool=%p, size=%u) "
			"last callback va_end: %#tx (%#tx expeceted), "
			"num of objects: %u (%u expected)\n",
			mp, mp->size,
			arg.obj_end, mp->elt_va_end,
			arg.obj_num, mp->size);
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
rte_mempool_dump(FILE *f, const struct rte_mempool *mp)
{
#ifdef RTE_LIBRTE_MEMPOOL_DEBUG
	struct rte_mempool_debug_stats sum;
	unsigned lcore_id;
#endif
	unsigned common_count;
	unsigned cache_count;

	fprintf(f, "mempool <%s>@%p\n", mp->name, mp);
	fprintf(f, "  flags=%x\n", mp->flags);
	fprintf(f, "  ring=<%s>@%p\n", mp->ring->name, mp->ring);
	fprintf(f, "  phys_addr=0x%" PRIx64 "\n", mp->phys_addr);
	fprintf(f, "  size=%"PRIu32"\n", mp->size);
	fprintf(f, "  header_size=%"PRIu32"\n", mp->header_size);
	fprintf(f, "  elt_size=%"PRIu32"\n", mp->elt_size);
	fprintf(f, "  trailer_size=%"PRIu32"\n", mp->trailer_size);
	fprintf(f, "  total_obj_size=%"PRIu32"\n",
	       mp->header_size + mp->elt_size + mp->trailer_size);

	fprintf(f, "  private_data_size=%"PRIu32"\n", mp->private_data_size);
	fprintf(f, "  pg_num=%"PRIu32"\n", mp->pg_num);
	fprintf(f, "  pg_shift=%"PRIu32"\n", mp->pg_shift);
	fprintf(f, "  pg_mask=%#tx\n", mp->pg_mask);
	fprintf(f, "  elt_va_start=%#tx\n", mp->elt_va_start);
	fprintf(f, "  elt_va_end=%#tx\n", mp->elt_va_end);
	fprintf(f, "  elt_pa[0]=0x%" PRIx64 "\n", mp->elt_pa[0]);

	if (mp->size != 0)
		fprintf(f, "  avg bytes/object=%#Lf\n",
			(long double)(mp->elt_va_end - mp->elt_va_start) /
			mp->size);

	cache_count = rte_mempool_dump_cache(f, mp);
	common_count = rte_ring_count(mp->ring);
	if ((cache_count + common_count) > mp->size)
		common_count = mp->size - cache_count;
	fprintf(f, "  common_pool_count=%u\n", common_count);

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
	fprintf(f, "  stats:\n");
	fprintf(f, "    put_bulk=%"PRIu64"\n", sum.put_bulk);
	fprintf(f, "    put_objs=%"PRIu64"\n", sum.put_objs);
	fprintf(f, "    get_success_bulk=%"PRIu64"\n", sum.get_success_bulk);
	fprintf(f, "    get_success_objs=%"PRIu64"\n", sum.get_success_objs);
	fprintf(f, "    get_fail_bulk=%"PRIu64"\n", sum.get_fail_bulk);
	fprintf(f, "    get_fail_objs=%"PRIu64"\n", sum.get_fail_objs);
#else
	fprintf(f, "  no statistics available\n");
#endif

	rte_mempool_audit(mp);
}

/* dump the status of all mempools on the console */
void
rte_mempool_list_dump(FILE *f)
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
		rte_mempool_dump(f, mp);
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

void rte_mempool_walk(void (*func)(const struct rte_mempool *, void *),
		      void *arg)
{
	struct rte_mempool *mp = NULL;
	struct rte_mempool_list *mempool_list;

	if ((mempool_list =
	     RTE_TAILQ_LOOKUP_BY_IDX(RTE_TAILQ_MEMPOOL, rte_mempool_list)) == NULL) {
		rte_errno = E_RTE_NO_TAILQ;
		return;
	}

	rte_rwlock_read_lock(RTE_EAL_MEMPOOL_RWLOCK);

	TAILQ_FOREACH(mp, mempool_list, next) {
		(*func)(mp, arg);
	}

	rte_rwlock_read_unlock(RTE_EAL_MEMPOOL_RWLOCK);
}
