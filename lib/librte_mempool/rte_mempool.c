/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   Copyright(c) 2016 6WIND S.A.
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
#include <sys/mman.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_malloc.h>
#include <rte_atomic.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_eal_memconfig.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_errno.h>
#include <rte_string_fns.h>
#include <rte_spinlock.h>

#include "rte_mempool.h"

TAILQ_HEAD(rte_mempool_list, rte_tailq_entry);

static struct rte_tailq_elem rte_mempool_tailq = {
	.name = "RTE_MEMPOOL",
};
EAL_REGISTER_TAILQ(rte_mempool_tailq)

#define CACHE_FLUSHTHRESH_MULTIPLIER 1.5
#define CALC_CACHE_FLUSHTHRESH(c)	\
	((typeof(c))((c) * CACHE_FLUSHTHRESH_MULTIPLIER))

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
		nchan = 4;

	nrank = rte_memory_get_nrank();
	if (nrank == 0)
		nrank = 1;

	/* process new object size */
	new_obj_size = (obj_size + RTE_MEMPOOL_ALIGN_MASK) / RTE_MEMPOOL_ALIGN;
	while (get_gcd(new_obj_size, nrank * nchan) != 1)
		new_obj_size++;
	return new_obj_size * RTE_MEMPOOL_ALIGN;
}

static void
mempool_add_elem(struct rte_mempool *mp, void *obj, phys_addr_t physaddr)
{
	struct rte_mempool_objhdr *hdr;
	struct rte_mempool_objtlr *tlr __rte_unused;

	/* set mempool ptr in header */
	hdr = RTE_PTR_SUB(obj, sizeof(*hdr));
	hdr->mp = mp;
	hdr->physaddr = physaddr;
	STAILQ_INSERT_TAIL(&mp->elt_list, hdr, next);
	mp->populated_size++;

#ifdef RTE_LIBRTE_MEMPOOL_DEBUG
	hdr->cookie = RTE_MEMPOOL_HEADER_COOKIE2;
	tlr = __mempool_get_trailer(obj);
	tlr->cookie = RTE_MEMPOOL_TRAILER_COOKIE;
#endif

	/* enqueue in ring */
	rte_mempool_ops_enqueue_bulk(mp, &obj, 1);
}

/* call obj_cb() for each mempool element */
uint32_t
rte_mempool_obj_iter(struct rte_mempool *mp,
	rte_mempool_obj_cb_t *obj_cb, void *obj_cb_arg)
{
	struct rte_mempool_objhdr *hdr;
	void *obj;
	unsigned n = 0;

	STAILQ_FOREACH(hdr, &mp->elt_list, next) {
		obj = (char *)hdr + sizeof(*hdr);
		obj_cb(mp, obj_cb_arg, obj, n);
		n++;
	}

	return n;
}

/* call mem_cb() for each mempool memory chunk */
uint32_t
rte_mempool_mem_iter(struct rte_mempool *mp,
	rte_mempool_mem_cb_t *mem_cb, void *mem_cb_arg)
{
	struct rte_mempool_memhdr *hdr;
	unsigned n = 0;

	STAILQ_FOREACH(hdr, &mp->mem_list, next) {
		mem_cb(mp, mem_cb_arg, hdr, n);
		n++;
	}

	return n;
}

/* get the header, trailer and total size of a mempool element. */
uint32_t
rte_mempool_calc_obj_size(uint32_t elt_size, uint32_t flags,
	struct rte_mempool_objsz *sz)
{
	struct rte_mempool_objsz lsz;

	sz = (sz != NULL) ? sz : &lsz;

	sz->header_size = sizeof(struct rte_mempool_objhdr);
	if ((flags & MEMPOOL_F_NO_CACHE_ALIGN) == 0)
		sz->header_size = RTE_ALIGN_CEIL(sz->header_size,
			RTE_MEMPOOL_ALIGN);

#ifdef RTE_LIBRTE_MEMPOOL_DEBUG
	sz->trailer_size = sizeof(struct rte_mempool_objtlr);
#else
	sz->trailer_size = 0;
#endif

	/* element size is 8 bytes-aligned at least */
	sz->elt_size = RTE_ALIGN_CEIL(elt_size, sizeof(uint64_t));

	/* expand trailer to next cache line */
	if ((flags & MEMPOOL_F_NO_CACHE_ALIGN) == 0) {
		sz->total_size = sz->header_size + sz->elt_size +
			sz->trailer_size;
		sz->trailer_size += ((RTE_MEMPOOL_ALIGN -
				  (sz->total_size & RTE_MEMPOOL_ALIGN_MASK)) &
				 RTE_MEMPOOL_ALIGN_MASK);
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

	/* this is the size of an object, including header and trailer */
	sz->total_size = sz->header_size + sz->elt_size + sz->trailer_size;

	return sz->total_size;
}


/*
 * Calculate maximum amount of memory required to store given number of objects.
 */
size_t
rte_mempool_xmem_size(uint32_t elt_num, size_t total_elt_sz, uint32_t pg_shift)
{
	size_t obj_per_page, pg_num, pg_sz;

	if (total_elt_sz == 0)
		return 0;

	if (pg_shift == 0)
		return total_elt_sz * elt_num;

	pg_sz = (size_t)1 << pg_shift;
	obj_per_page = pg_sz / total_elt_sz;
	if (obj_per_page == 0)
		return RTE_ALIGN_CEIL(total_elt_sz, pg_sz) * elt_num;

	pg_num = (elt_num + obj_per_page - 1) / obj_per_page;
	return pg_num << pg_shift;
}

/*
 * Calculate how much memory would be actually required with the
 * given memory footprint to store required number of elements.
 */
ssize_t
rte_mempool_xmem_usage(__rte_unused void *vaddr, uint32_t elt_num,
	size_t total_elt_sz, const phys_addr_t paddr[], uint32_t pg_num,
	uint32_t pg_shift)
{
	uint32_t elt_cnt = 0;
	phys_addr_t start, end;
	uint32_t paddr_idx;
	size_t pg_sz = (size_t)1 << pg_shift;

	/* if paddr is NULL, assume contiguous memory */
	if (paddr == NULL) {
		start = 0;
		end = pg_sz * pg_num;
		paddr_idx = pg_num;
	} else {
		start = paddr[0];
		end = paddr[0] + pg_sz;
		paddr_idx = 1;
	}
	while (elt_cnt < elt_num) {

		if (end - start >= total_elt_sz) {
			/* enough contiguous memory, add an object */
			start += total_elt_sz;
			elt_cnt++;
		} else if (paddr_idx < pg_num) {
			/* no room to store one obj, add a page */
			if (end == paddr[paddr_idx]) {
				end += pg_sz;
			} else {
				start = paddr[paddr_idx];
				end = paddr[paddr_idx] + pg_sz;
			}
			paddr_idx++;

		} else {
			/* no more page, return how many elements fit */
			return -(size_t)elt_cnt;
		}
	}

	return (size_t)paddr_idx << pg_shift;
}

/* free a memchunk allocated with rte_memzone_reserve() */
static void
rte_mempool_memchunk_mz_free(__rte_unused struct rte_mempool_memhdr *memhdr,
	void *opaque)
{
	const struct rte_memzone *mz = opaque;
	rte_memzone_free(mz);
}

/* Free memory chunks used by a mempool. Objects must be in pool */
static void
rte_mempool_free_memchunks(struct rte_mempool *mp)
{
	struct rte_mempool_memhdr *memhdr;
	void *elt;

	while (!STAILQ_EMPTY(&mp->elt_list)) {
		rte_mempool_ops_dequeue_bulk(mp, &elt, 1);
		(void)elt;
		STAILQ_REMOVE_HEAD(&mp->elt_list, next);
		mp->populated_size--;
	}

	while (!STAILQ_EMPTY(&mp->mem_list)) {
		memhdr = STAILQ_FIRST(&mp->mem_list);
		STAILQ_REMOVE_HEAD(&mp->mem_list, next);
		if (memhdr->free_cb != NULL)
			memhdr->free_cb(memhdr, memhdr->opaque);
		rte_free(memhdr);
		mp->nb_mem_chunks--;
	}
}

/* Add objects in the pool, using a physically contiguous memory
 * zone. Return the number of objects added, or a negative value
 * on error.
 */
int
rte_mempool_populate_phys(struct rte_mempool *mp, char *vaddr,
	phys_addr_t paddr, size_t len, rte_mempool_memchunk_free_cb_t *free_cb,
	void *opaque)
{
	unsigned total_elt_sz;
	unsigned i = 0;
	size_t off;
	struct rte_mempool_memhdr *memhdr;
	int ret;

	/* create the internal ring if not already done */
	if ((mp->flags & MEMPOOL_F_POOL_CREATED) == 0) {
		ret = rte_mempool_ops_alloc(mp);
		if (ret != 0)
			return ret;
		mp->flags |= MEMPOOL_F_POOL_CREATED;
	}

	/* mempool is already populated */
	if (mp->populated_size >= mp->size)
		return -ENOSPC;

	total_elt_sz = mp->header_size + mp->elt_size + mp->trailer_size;

	memhdr = rte_zmalloc("MEMPOOL_MEMHDR", sizeof(*memhdr), 0);
	if (memhdr == NULL)
		return -ENOMEM;

	memhdr->mp = mp;
	memhdr->addr = vaddr;
	memhdr->phys_addr = paddr;
	memhdr->len = len;
	memhdr->free_cb = free_cb;
	memhdr->opaque = opaque;

	if (mp->flags & MEMPOOL_F_NO_CACHE_ALIGN)
		off = RTE_PTR_ALIGN_CEIL(vaddr, 8) - vaddr;
	else
		off = RTE_PTR_ALIGN_CEIL(vaddr, RTE_CACHE_LINE_SIZE) - vaddr;

	while (off + total_elt_sz <= len && mp->populated_size < mp->size) {
		off += mp->header_size;
		if (paddr == RTE_BAD_PHYS_ADDR)
			mempool_add_elem(mp, (char *)vaddr + off,
				RTE_BAD_PHYS_ADDR);
		else
			mempool_add_elem(mp, (char *)vaddr + off, paddr + off);
		off += mp->elt_size + mp->trailer_size;
		i++;
	}

	/* not enough room to store one object */
	if (i == 0)
		return -EINVAL;

	STAILQ_INSERT_TAIL(&mp->mem_list, memhdr, next);
	mp->nb_mem_chunks++;
	return i;
}

/* Add objects in the pool, using a table of physical pages. Return the
 * number of objects added, or a negative value on error.
 */
int
rte_mempool_populate_phys_tab(struct rte_mempool *mp, char *vaddr,
	const phys_addr_t paddr[], uint32_t pg_num, uint32_t pg_shift,
	rte_mempool_memchunk_free_cb_t *free_cb, void *opaque)
{
	uint32_t i, n;
	int ret, cnt = 0;
	size_t pg_sz = (size_t)1 << pg_shift;

	/* mempool must not be populated */
	if (mp->nb_mem_chunks != 0)
		return -EEXIST;

	if (mp->flags & MEMPOOL_F_NO_PHYS_CONTIG)
		return rte_mempool_populate_phys(mp, vaddr, RTE_BAD_PHYS_ADDR,
			pg_num * pg_sz, free_cb, opaque);

	for (i = 0; i < pg_num && mp->populated_size < mp->size; i += n) {

		/* populate with the largest group of contiguous pages */
		for (n = 1; (i + n) < pg_num &&
			     paddr[i + n - 1] + pg_sz == paddr[i + n]; n++)
			;

		ret = rte_mempool_populate_phys(mp, vaddr + i * pg_sz,
			paddr[i], n * pg_sz, free_cb, opaque);
		if (ret < 0) {
			rte_mempool_free_memchunks(mp);
			return ret;
		}
		/* no need to call the free callback for next chunks */
		free_cb = NULL;
		cnt += ret;
	}
	return cnt;
}

/* Populate the mempool with a virtual area. Return the number of
 * objects added, or a negative value on error.
 */
int
rte_mempool_populate_virt(struct rte_mempool *mp, char *addr,
	size_t len, size_t pg_sz, rte_mempool_memchunk_free_cb_t *free_cb,
	void *opaque)
{
	phys_addr_t paddr;
	size_t off, phys_len;
	int ret, cnt = 0;

	/* mempool must not be populated */
	if (mp->nb_mem_chunks != 0)
		return -EEXIST;
	/* address and len must be page-aligned */
	if (RTE_PTR_ALIGN_CEIL(addr, pg_sz) != addr)
		return -EINVAL;
	if (RTE_ALIGN_CEIL(len, pg_sz) != len)
		return -EINVAL;

	if (mp->flags & MEMPOOL_F_NO_PHYS_CONTIG)
		return rte_mempool_populate_phys(mp, addr, RTE_BAD_PHYS_ADDR,
			len, free_cb, opaque);

	for (off = 0; off + pg_sz <= len &&
		     mp->populated_size < mp->size; off += phys_len) {

		paddr = rte_mem_virt2phy(addr + off);
		/* required for xen_dom0 to get the machine address */
		paddr = rte_mem_phy2mch(-1, paddr);

		if (paddr == RTE_BAD_PHYS_ADDR) {
			ret = -EINVAL;
			goto fail;
		}

		/* populate with the largest group of contiguous pages */
		for (phys_len = pg_sz; off + phys_len < len; phys_len += pg_sz) {
			phys_addr_t paddr_tmp;

			paddr_tmp = rte_mem_virt2phy(addr + off + phys_len);
			paddr_tmp = rte_mem_phy2mch(-1, paddr_tmp);

			if (paddr_tmp != paddr + phys_len)
				break;
		}

		ret = rte_mempool_populate_phys(mp, addr + off, paddr,
			phys_len, free_cb, opaque);
		if (ret < 0)
			goto fail;
		/* no need to call the free callback for next chunks */
		free_cb = NULL;
		cnt += ret;
	}

	return cnt;

 fail:
	rte_mempool_free_memchunks(mp);
	return ret;
}

/* Default function to populate the mempool: allocate memory in memzones,
 * and populate them. Return the number of objects added, or a negative
 * value on error.
 */
int
rte_mempool_populate_default(struct rte_mempool *mp)
{
	int mz_flags = RTE_MEMZONE_1GB|RTE_MEMZONE_SIZE_HINT_ONLY;
	char mz_name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *mz;
	size_t size, total_elt_sz, align, pg_sz, pg_shift;
	phys_addr_t paddr;
	unsigned mz_id, n;
	int ret;

	/* mempool must not be populated */
	if (mp->nb_mem_chunks != 0)
		return -EEXIST;

	if (rte_xen_dom0_supported()) {
		pg_sz = RTE_PGSIZE_2M;
		pg_shift = rte_bsf32(pg_sz);
		align = pg_sz;
	} else if (rte_eal_has_hugepages()) {
		pg_shift = 0; /* not needed, zone is physically contiguous */
		pg_sz = 0;
		align = RTE_CACHE_LINE_SIZE;
	} else {
		pg_sz = getpagesize();
		pg_shift = rte_bsf32(pg_sz);
		align = pg_sz;
	}

	total_elt_sz = mp->header_size + mp->elt_size + mp->trailer_size;
	for (mz_id = 0, n = mp->size; n > 0; mz_id++, n -= ret) {
		size = rte_mempool_xmem_size(n, total_elt_sz, pg_shift);

		ret = snprintf(mz_name, sizeof(mz_name),
			RTE_MEMPOOL_MZ_FORMAT "_%d", mp->name, mz_id);
		if (ret < 0 || ret >= (int)sizeof(mz_name)) {
			ret = -ENAMETOOLONG;
			goto fail;
		}

		mz = rte_memzone_reserve_aligned(mz_name, size,
			mp->socket_id, mz_flags, align);
		/* not enough memory, retry with the biggest zone we have */
		if (mz == NULL)
			mz = rte_memzone_reserve_aligned(mz_name, 0,
				mp->socket_id, mz_flags, align);
		if (mz == NULL) {
			ret = -rte_errno;
			goto fail;
		}

		if (mp->flags & MEMPOOL_F_NO_PHYS_CONTIG)
			paddr = RTE_BAD_PHYS_ADDR;
		else
			paddr = mz->phys_addr;

		if (rte_eal_has_hugepages() && !rte_xen_dom0_supported())
			ret = rte_mempool_populate_phys(mp, mz->addr,
				paddr, mz->len,
				rte_mempool_memchunk_mz_free,
				(void *)(uintptr_t)mz);
		else
			ret = rte_mempool_populate_virt(mp, mz->addr,
				mz->len, pg_sz,
				rte_mempool_memchunk_mz_free,
				(void *)(uintptr_t)mz);
		if (ret < 0) {
			rte_memzone_free(mz);
			goto fail;
		}
	}

	return mp->size;

 fail:
	rte_mempool_free_memchunks(mp);
	return ret;
}

/* return the memory size required for mempool objects in anonymous mem */
static size_t
get_anon_size(const struct rte_mempool *mp)
{
	size_t size, total_elt_sz, pg_sz, pg_shift;

	pg_sz = getpagesize();
	pg_shift = rte_bsf32(pg_sz);
	total_elt_sz = mp->header_size + mp->elt_size + mp->trailer_size;
	size = rte_mempool_xmem_size(mp->size, total_elt_sz, pg_shift);

	return size;
}

/* unmap a memory zone mapped by rte_mempool_populate_anon() */
static void
rte_mempool_memchunk_anon_free(struct rte_mempool_memhdr *memhdr,
	void *opaque)
{
	munmap(opaque, get_anon_size(memhdr->mp));
}

/* populate the mempool with an anonymous mapping */
int
rte_mempool_populate_anon(struct rte_mempool *mp)
{
	size_t size;
	int ret;
	char *addr;

	/* mempool is already populated, error */
	if (!STAILQ_EMPTY(&mp->mem_list)) {
		rte_errno = EINVAL;
		return 0;
	}

	/* get chunk of virtually continuous memory */
	size = get_anon_size(mp);
	addr = mmap(NULL, size, PROT_READ | PROT_WRITE,
		MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (addr == MAP_FAILED) {
		rte_errno = errno;
		return 0;
	}
	/* can't use MMAP_LOCKED, it does not exist on BSD */
	if (mlock(addr, size) < 0) {
		rte_errno = errno;
		munmap(addr, size);
		return 0;
	}

	ret = rte_mempool_populate_virt(mp, addr, size, getpagesize(),
		rte_mempool_memchunk_anon_free, addr);
	if (ret == 0)
		goto fail;

	return mp->populated_size;

 fail:
	rte_mempool_free_memchunks(mp);
	return 0;
}

/* free a mempool */
void
rte_mempool_free(struct rte_mempool *mp)
{
	struct rte_mempool_list *mempool_list = NULL;
	struct rte_tailq_entry *te;

	if (mp == NULL)
		return;

	mempool_list = RTE_TAILQ_CAST(rte_mempool_tailq.head, rte_mempool_list);
	rte_rwlock_write_lock(RTE_EAL_TAILQ_RWLOCK);
	/* find out tailq entry */
	TAILQ_FOREACH(te, mempool_list, next) {
		if (te->data == (void *)mp)
			break;
	}

	if (te != NULL) {
		TAILQ_REMOVE(mempool_list, te, next);
		rte_free(te);
	}
	rte_rwlock_write_unlock(RTE_EAL_TAILQ_RWLOCK);

	rte_mempool_free_memchunks(mp);
	rte_mempool_ops_free(mp);
	rte_memzone_free(mp->mz);
}

static void
mempool_cache_init(struct rte_mempool_cache *cache, uint32_t size)
{
	cache->size = size;
	cache->flushthresh = CALC_CACHE_FLUSHTHRESH(size);
	cache->len = 0;
}

/*
 * Create and initialize a cache for objects that are retrieved from and
 * returned to an underlying mempool. This structure is identical to the
 * local_cache[lcore_id] pointed to by the mempool structure.
 */
struct rte_mempool_cache *
rte_mempool_cache_create(uint32_t size, int socket_id)
{
	struct rte_mempool_cache *cache;

	if (size == 0 || size > RTE_MEMPOOL_CACHE_MAX_SIZE) {
		rte_errno = EINVAL;
		return NULL;
	}

	cache = rte_zmalloc_socket("MEMPOOL_CACHE", sizeof(*cache),
				  RTE_CACHE_LINE_SIZE, socket_id);
	if (cache == NULL) {
		RTE_LOG(ERR, MEMPOOL, "Cannot allocate mempool cache.\n");
		rte_errno = ENOMEM;
		return NULL;
	}

	mempool_cache_init(cache, size);

	return cache;
}

/*
 * Free a cache. It's the responsibility of the user to make sure that any
 * remaining objects in the cache are flushed to the corresponding
 * mempool.
 */
void
rte_mempool_cache_free(struct rte_mempool_cache *cache)
{
	rte_free(cache);
}

/* create an empty mempool */
struct rte_mempool *
rte_mempool_create_empty(const char *name, unsigned n, unsigned elt_size,
	unsigned cache_size, unsigned private_data_size,
	int socket_id, unsigned flags)
{
	char mz_name[RTE_MEMZONE_NAMESIZE];
	struct rte_mempool_list *mempool_list;
	struct rte_mempool *mp = NULL;
	struct rte_tailq_entry *te = NULL;
	const struct rte_memzone *mz = NULL;
	size_t mempool_size;
	int mz_flags = RTE_MEMZONE_1GB|RTE_MEMZONE_SIZE_HINT_ONLY;
	struct rte_mempool_objsz objsz;
	unsigned lcore_id;
	int ret;

	/* compilation-time checks */
	RTE_BUILD_BUG_ON((sizeof(struct rte_mempool) &
			  RTE_CACHE_LINE_MASK) != 0);
	RTE_BUILD_BUG_ON((sizeof(struct rte_mempool_cache) &
			  RTE_CACHE_LINE_MASK) != 0);
#ifdef RTE_LIBRTE_MEMPOOL_DEBUG
	RTE_BUILD_BUG_ON((sizeof(struct rte_mempool_debug_stats) &
			  RTE_CACHE_LINE_MASK) != 0);
	RTE_BUILD_BUG_ON((offsetof(struct rte_mempool, stats) &
			  RTE_CACHE_LINE_MASK) != 0);
#endif

	mempool_list = RTE_TAILQ_CAST(rte_mempool_tailq.head, rte_mempool_list);

	/* asked cache too big */
	if (cache_size > RTE_MEMPOOL_CACHE_MAX_SIZE ||
	    CALC_CACHE_FLUSHTHRESH(cache_size) > n) {
		rte_errno = EINVAL;
		return NULL;
	}

	/* "no cache align" imply "no spread" */
	if (flags & MEMPOOL_F_NO_CACHE_ALIGN)
		flags |= MEMPOOL_F_NO_SPREAD;

	/* calculate mempool object sizes. */
	if (!rte_mempool_calc_obj_size(elt_size, flags, &objsz)) {
		rte_errno = EINVAL;
		return NULL;
	}

	rte_rwlock_write_lock(RTE_EAL_MEMPOOL_RWLOCK);

	/*
	 * reserve a memory zone for this mempool: private data is
	 * cache-aligned
	 */
	private_data_size = (private_data_size +
			     RTE_MEMPOOL_ALIGN_MASK) & (~RTE_MEMPOOL_ALIGN_MASK);


	/* try to allocate tailq entry */
	te = rte_zmalloc("MEMPOOL_TAILQ_ENTRY", sizeof(*te), 0);
	if (te == NULL) {
		RTE_LOG(ERR, MEMPOOL, "Cannot allocate tailq entry!\n");
		goto exit_unlock;
	}

	mempool_size = MEMPOOL_HEADER_SIZE(mp, cache_size);
	mempool_size += private_data_size;
	mempool_size = RTE_ALIGN_CEIL(mempool_size, RTE_MEMPOOL_ALIGN);

	ret = snprintf(mz_name, sizeof(mz_name), RTE_MEMPOOL_MZ_FORMAT, name);
	if (ret < 0 || ret >= (int)sizeof(mz_name)) {
		rte_errno = ENAMETOOLONG;
		goto exit_unlock;
	}

	mz = rte_memzone_reserve(mz_name, mempool_size, socket_id, mz_flags);
	if (mz == NULL)
		goto exit_unlock;

	/* init the mempool structure */
	mp = mz->addr;
	memset(mp, 0, MEMPOOL_HEADER_SIZE(mp, cache_size));
	ret = snprintf(mp->name, sizeof(mp->name), "%s", name);
	if (ret < 0 || ret >= (int)sizeof(mp->name)) {
		rte_errno = ENAMETOOLONG;
		goto exit_unlock;
	}
	mp->mz = mz;
	mp->size = n;
	mp->flags = flags;
	mp->socket_id = socket_id;
	mp->elt_size = objsz.elt_size;
	mp->header_size = objsz.header_size;
	mp->trailer_size = objsz.trailer_size;
	/* Size of default caches, zero means disabled. */
	mp->cache_size = cache_size;
	mp->private_data_size = private_data_size;
	STAILQ_INIT(&mp->elt_list);
	STAILQ_INIT(&mp->mem_list);

	/*
	 * local_cache pointer is set even if cache_size is zero.
	 * The local_cache points to just past the elt_pa[] array.
	 */
	mp->local_cache = (struct rte_mempool_cache *)
		RTE_PTR_ADD(mp, MEMPOOL_HEADER_SIZE(mp, 0));

	/* Init all default caches. */
	if (cache_size != 0) {
		for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++)
			mempool_cache_init(&mp->local_cache[lcore_id],
					   cache_size);
	}

	te->data = mp;

	rte_rwlock_write_lock(RTE_EAL_TAILQ_RWLOCK);
	TAILQ_INSERT_TAIL(mempool_list, te, next);
	rte_rwlock_write_unlock(RTE_EAL_TAILQ_RWLOCK);
	rte_rwlock_write_unlock(RTE_EAL_MEMPOOL_RWLOCK);

	return mp;

exit_unlock:
	rte_rwlock_write_unlock(RTE_EAL_MEMPOOL_RWLOCK);
	rte_free(te);
	rte_mempool_free(mp);
	return NULL;
}

/* create the mempool */
struct rte_mempool *
rte_mempool_create(const char *name, unsigned n, unsigned elt_size,
	unsigned cache_size, unsigned private_data_size,
	rte_mempool_ctor_t *mp_init, void *mp_init_arg,
	rte_mempool_obj_cb_t *obj_init, void *obj_init_arg,
	int socket_id, unsigned flags)
{
	int ret;
	struct rte_mempool *mp;

	mp = rte_mempool_create_empty(name, n, elt_size, cache_size,
		private_data_size, socket_id, flags);
	if (mp == NULL)
		return NULL;

	/*
	 * Since we have 4 combinations of the SP/SC/MP/MC examine the flags to
	 * set the correct index into the table of ops structs.
	 */
	if ((flags & MEMPOOL_F_SP_PUT) && (flags & MEMPOOL_F_SC_GET))
		ret = rte_mempool_set_ops_byname(mp, "ring_sp_sc", NULL);
	else if (flags & MEMPOOL_F_SP_PUT)
		ret = rte_mempool_set_ops_byname(mp, "ring_sp_mc", NULL);
	else if (flags & MEMPOOL_F_SC_GET)
		ret = rte_mempool_set_ops_byname(mp, "ring_mp_sc", NULL);
	else
		ret = rte_mempool_set_ops_byname(mp, "ring_mp_mc", NULL);

	if (ret)
		goto fail;

	/* call the mempool priv initializer */
	if (mp_init)
		mp_init(mp, mp_init_arg);

	if (rte_mempool_populate_default(mp) < 0)
		goto fail;

	/* call the object initializers */
	if (obj_init)
		rte_mempool_obj_iter(mp, obj_init, obj_init_arg);

	return mp;

 fail:
	rte_mempool_free(mp);
	return NULL;
}

/*
 * Create the mempool over already allocated chunk of memory.
 * That external memory buffer can consists of physically disjoint pages.
 * Setting vaddr to NULL, makes mempool to fallback to rte_mempool_create()
 * behavior.
 */
struct rte_mempool *
rte_mempool_xmem_create(const char *name, unsigned n, unsigned elt_size,
		unsigned cache_size, unsigned private_data_size,
		rte_mempool_ctor_t *mp_init, void *mp_init_arg,
		rte_mempool_obj_cb_t *obj_init, void *obj_init_arg,
		int socket_id, unsigned flags, void *vaddr,
		const phys_addr_t paddr[], uint32_t pg_num, uint32_t pg_shift)
{
	struct rte_mempool *mp = NULL;
	int ret;

	/* no virtual address supplied, use rte_mempool_create() */
	if (vaddr == NULL)
		return rte_mempool_create(name, n, elt_size, cache_size,
			private_data_size, mp_init, mp_init_arg,
			obj_init, obj_init_arg, socket_id, flags);

	/* check that we have both VA and PA */
	if (paddr == NULL) {
		rte_errno = EINVAL;
		return NULL;
	}

	/* Check that pg_shift parameter is valid. */
	if (pg_shift > MEMPOOL_PG_SHIFT_MAX) {
		rte_errno = EINVAL;
		return NULL;
	}

	mp = rte_mempool_create_empty(name, n, elt_size, cache_size,
		private_data_size, socket_id, flags);
	if (mp == NULL)
		return NULL;

	/* call the mempool priv initializer */
	if (mp_init)
		mp_init(mp, mp_init_arg);

	ret = rte_mempool_populate_phys_tab(mp, vaddr, paddr, pg_num, pg_shift,
		NULL, NULL);
	if (ret < 0 || ret != (int)mp->size)
		goto fail;

	/* call the object initializers */
	if (obj_init)
		rte_mempool_obj_iter(mp, obj_init, obj_init_arg);

	return mp;

 fail:
	rte_mempool_free(mp);
	return NULL;
}

/* Return the number of entries in the mempool */
unsigned int
rte_mempool_avail_count(const struct rte_mempool *mp)
{
	unsigned count;
	unsigned lcore_id;

	count = rte_mempool_ops_get_count(mp);

	if (mp->cache_size == 0)
		return count;

	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++)
		count += mp->local_cache[lcore_id].len;

	/*
	 * due to race condition (access to len is not locked), the
	 * total can be greater than size... so fix the result
	 */
	if (count > mp->size)
		return mp->size;
	return count;
}

/* return the number of entries allocated from the mempool */
unsigned int
rte_mempool_in_use_count(const struct rte_mempool *mp)
{
	return mp->size - rte_mempool_avail_count(mp);
}

/* dump the cache status */
static unsigned
rte_mempool_dump_cache(FILE *f, const struct rte_mempool *mp)
{
	unsigned lcore_id;
	unsigned count = 0;
	unsigned cache_count;

	fprintf(f, "  internal cache infos:\n");
	fprintf(f, "    cache_size=%"PRIu32"\n", mp->cache_size);

	if (mp->cache_size == 0)
		return count;

	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		cache_count = mp->local_cache[lcore_id].len;
		fprintf(f, "    cache_count[%u]=%"PRIu32"\n",
			lcore_id, cache_count);
		count += cache_count;
	}
	fprintf(f, "    total_cache_count=%u\n", count);
	return count;
}

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

/* check and update cookies or panic (internal) */
void rte_mempool_check_cookies(const struct rte_mempool *mp,
	void * const *obj_table_const, unsigned n, int free)
{
#ifdef RTE_LIBRTE_MEMPOOL_DEBUG
	struct rte_mempool_objhdr *hdr;
	struct rte_mempool_objtlr *tlr;
	uint64_t cookie;
	void *tmp;
	void *obj;
	void **obj_table;

	/* Force to drop the "const" attribute. This is done only when
	 * DEBUG is enabled */
	tmp = (void *) obj_table_const;
	obj_table = tmp;

	while (n--) {
		obj = obj_table[n];

		if (rte_mempool_from_obj(obj) != mp)
			rte_panic("MEMPOOL: object is owned by another "
				  "mempool\n");

		hdr = __mempool_get_header(obj);
		cookie = hdr->cookie;

		if (free == 0) {
			if (cookie != RTE_MEMPOOL_HEADER_COOKIE1) {
				RTE_LOG(CRIT, MEMPOOL,
					"obj=%p, mempool=%p, cookie=%" PRIx64 "\n",
					obj, (const void *) mp, cookie);
				rte_panic("MEMPOOL: bad header cookie (put)\n");
			}
			hdr->cookie = RTE_MEMPOOL_HEADER_COOKIE2;
		} else if (free == 1) {
			if (cookie != RTE_MEMPOOL_HEADER_COOKIE2) {
				RTE_LOG(CRIT, MEMPOOL,
					"obj=%p, mempool=%p, cookie=%" PRIx64 "\n",
					obj, (const void *) mp, cookie);
				rte_panic("MEMPOOL: bad header cookie (get)\n");
			}
			hdr->cookie = RTE_MEMPOOL_HEADER_COOKIE1;
		} else if (free == 2) {
			if (cookie != RTE_MEMPOOL_HEADER_COOKIE1 &&
			    cookie != RTE_MEMPOOL_HEADER_COOKIE2) {
				RTE_LOG(CRIT, MEMPOOL,
					"obj=%p, mempool=%p, cookie=%" PRIx64 "\n",
					obj, (const void *) mp, cookie);
				rte_panic("MEMPOOL: bad header cookie (audit)\n");
			}
		}
		tlr = __mempool_get_trailer(obj);
		cookie = tlr->cookie;
		if (cookie != RTE_MEMPOOL_TRAILER_COOKIE) {
			RTE_LOG(CRIT, MEMPOOL,
				"obj=%p, mempool=%p, cookie=%" PRIx64 "\n",
				obj, (const void *) mp, cookie);
			rte_panic("MEMPOOL: bad trailer cookie\n");
		}
	}
#else
	RTE_SET_USED(mp);
	RTE_SET_USED(obj_table_const);
	RTE_SET_USED(n);
	RTE_SET_USED(free);
#endif
}

#ifdef RTE_LIBRTE_MEMPOOL_DEBUG
static void
mempool_obj_audit(struct rte_mempool *mp, __rte_unused void *opaque,
	void *obj, __rte_unused unsigned idx)
{
	__mempool_check_cookies(mp, &obj, 1, 2);
}

static void
mempool_audit_cookies(struct rte_mempool *mp)
{
	unsigned num;

	num = rte_mempool_obj_iter(mp, mempool_obj_audit, NULL);
	if (num != mp->size) {
		rte_panic("rte_mempool_obj_iter(mempool=%p, size=%u) "
			"iterated only over %u elements\n",
			mp, mp->size, num);
	}
}
#else
#define mempool_audit_cookies(mp) do {} while(0)
#endif

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic error "-Wcast-qual"
#endif

/* check cookies before and after objects */
static void
mempool_audit_cache(const struct rte_mempool *mp)
{
	/* check cache size consistency */
	unsigned lcore_id;

	if (mp->cache_size == 0)
		return;

	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		const struct rte_mempool_cache *cache;
		cache = &mp->local_cache[lcore_id];
		if (cache->len > cache->flushthresh) {
			RTE_LOG(CRIT, MEMPOOL, "badness on cache[%u]\n",
				lcore_id);
			rte_panic("MEMPOOL: invalid cache len\n");
		}
	}
}

/* check the consistency of mempool (size, cookies, ...) */
void
rte_mempool_audit(struct rte_mempool *mp)
{
	mempool_audit_cache(mp);
	mempool_audit_cookies(mp);

	/* For case where mempool DEBUG is not set, and cache size is 0 */
	RTE_SET_USED(mp);
}

/* dump the status of the mempool on the console */
void
rte_mempool_dump(FILE *f, struct rte_mempool *mp)
{
#ifdef RTE_LIBRTE_MEMPOOL_DEBUG
	struct rte_mempool_debug_stats sum;
	unsigned lcore_id;
#endif
	struct rte_mempool_memhdr *memhdr;
	unsigned common_count;
	unsigned cache_count;
	size_t mem_len = 0;

	RTE_ASSERT(f != NULL);
	RTE_ASSERT(mp != NULL);

	fprintf(f, "mempool <%s>@%p\n", mp->name, mp);
	fprintf(f, "  flags=%x\n", mp->flags);
	fprintf(f, "  pool=%p\n", mp->pool_data);
	fprintf(f, "  phys_addr=0x%" PRIx64 "\n", mp->mz->phys_addr);
	fprintf(f, "  nb_mem_chunks=%u\n", mp->nb_mem_chunks);
	fprintf(f, "  size=%"PRIu32"\n", mp->size);
	fprintf(f, "  populated_size=%"PRIu32"\n", mp->populated_size);
	fprintf(f, "  header_size=%"PRIu32"\n", mp->header_size);
	fprintf(f, "  elt_size=%"PRIu32"\n", mp->elt_size);
	fprintf(f, "  trailer_size=%"PRIu32"\n", mp->trailer_size);
	fprintf(f, "  total_obj_size=%"PRIu32"\n",
	       mp->header_size + mp->elt_size + mp->trailer_size);

	fprintf(f, "  private_data_size=%"PRIu32"\n", mp->private_data_size);

	STAILQ_FOREACH(memhdr, &mp->mem_list, next)
		mem_len += memhdr->len;
	if (mem_len != 0) {
		fprintf(f, "  avg bytes/object=%#Lf\n",
			(long double)mem_len / mp->size);
	}

	cache_count = rte_mempool_dump_cache(f, mp);
	common_count = rte_mempool_ops_get_count(mp);
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
	struct rte_mempool *mp = NULL;
	struct rte_tailq_entry *te;
	struct rte_mempool_list *mempool_list;

	mempool_list = RTE_TAILQ_CAST(rte_mempool_tailq.head, rte_mempool_list);

	rte_rwlock_read_lock(RTE_EAL_MEMPOOL_RWLOCK);

	TAILQ_FOREACH(te, mempool_list, next) {
		mp = (struct rte_mempool *) te->data;
		rte_mempool_dump(f, mp);
	}

	rte_rwlock_read_unlock(RTE_EAL_MEMPOOL_RWLOCK);
}

/* search a mempool from its name */
struct rte_mempool *
rte_mempool_lookup(const char *name)
{
	struct rte_mempool *mp = NULL;
	struct rte_tailq_entry *te;
	struct rte_mempool_list *mempool_list;

	mempool_list = RTE_TAILQ_CAST(rte_mempool_tailq.head, rte_mempool_list);

	rte_rwlock_read_lock(RTE_EAL_MEMPOOL_RWLOCK);

	TAILQ_FOREACH(te, mempool_list, next) {
		mp = (struct rte_mempool *) te->data;
		if (strncmp(name, mp->name, RTE_MEMPOOL_NAMESIZE) == 0)
			break;
	}

	rte_rwlock_read_unlock(RTE_EAL_MEMPOOL_RWLOCK);

	if (te == NULL) {
		rte_errno = ENOENT;
		return NULL;
	}

	return mp;
}

void rte_mempool_walk(void (*func)(struct rte_mempool *, void *),
		      void *arg)
{
	struct rte_tailq_entry *te = NULL;
	struct rte_mempool_list *mempool_list;
	void *tmp_te;

	mempool_list = RTE_TAILQ_CAST(rte_mempool_tailq.head, rte_mempool_list);

	rte_rwlock_read_lock(RTE_EAL_MEMPOOL_RWLOCK);

	TAILQ_FOREACH_SAFE(te, mempool_list, next, tmp_te) {
		(*func)((struct rte_mempool *) te->data, arg);
	}

	rte_rwlock_read_unlock(RTE_EAL_MEMPOOL_RWLOCK);
}
