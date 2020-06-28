/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016 6WIND S.A.
 * Copyright 2020 Mellanox Technologies, Ltd
 */
#include <rte_eal_memconfig.h>
#include <rte_errno.h>
#include <rte_mempool.h>
#include <rte_malloc.h>
#include <rte_rwlock.h>

#include "mlx5_glue.h"
#include "mlx5_common_mp.h"
#include "mlx5_common_mr.h"
#include "mlx5_common_utils.h"
#include "mlx5_malloc.h"

struct mr_find_contig_memsegs_data {
	uintptr_t addr;
	uintptr_t start;
	uintptr_t end;
	const struct rte_memseg_list *msl;
};

/**
 * Expand B-tree table to a given size. Can't be called with holding
 * memory_hotplug_lock or share_cache.rwlock due to rte_realloc().
 *
 * @param bt
 *   Pointer to B-tree structure.
 * @param n
 *   Number of entries for expansion.
 *
 * @return
 *   0 on success, -1 on failure.
 */
static int
mr_btree_expand(struct mlx5_mr_btree *bt, int n)
{
	void *mem;
	int ret = 0;

	if (n <= bt->size)
		return ret;
	/*
	 * Downside of directly using rte_realloc() is that SOCKET_ID_ANY is
	 * used inside if there's no room to expand. Because this is a quite
	 * rare case and a part of very slow path, it is very acceptable.
	 * Initially cache_bh[] will be given practically enough space and once
	 * it is expanded, expansion wouldn't be needed again ever.
	 */
	mem = mlx5_realloc(bt->table, MLX5_MEM_RTE | MLX5_MEM_ZERO,
			   n * sizeof(struct mr_cache_entry), 0, SOCKET_ID_ANY);
	if (mem == NULL) {
		/* Not an error, B-tree search will be skipped. */
		DRV_LOG(WARNING, "failed to expand MR B-tree (%p) table",
			(void *)bt);
		ret = -1;
	} else {
		DRV_LOG(DEBUG, "expanded MR B-tree table (size=%u)", n);
		bt->table = mem;
		bt->size = n;
	}
	return ret;
}

/**
 * Look up LKey from given B-tree lookup table, store the last index and return
 * searched LKey.
 *
 * @param bt
 *   Pointer to B-tree structure.
 * @param[out] idx
 *   Pointer to index. Even on search failure, returns index where it stops
 *   searching so that index can be used when inserting a new entry.
 * @param addr
 *   Search key.
 *
 * @return
 *   Searched LKey on success, UINT32_MAX on no match.
 */
static uint32_t
mr_btree_lookup(struct mlx5_mr_btree *bt, uint16_t *idx, uintptr_t addr)
{
	struct mr_cache_entry *lkp_tbl;
	uint16_t n;
	uint16_t base = 0;

	MLX5_ASSERT(bt != NULL);
	lkp_tbl = *bt->table;
	n = bt->len;
	/* First entry must be NULL for comparison. */
	MLX5_ASSERT(bt->len > 0 || (lkp_tbl[0].start == 0 &&
				    lkp_tbl[0].lkey == UINT32_MAX));
	/* Binary search. */
	do {
		register uint16_t delta = n >> 1;

		if (addr < lkp_tbl[base + delta].start) {
			n = delta;
		} else {
			base += delta;
			n -= delta;
		}
	} while (n > 1);
	MLX5_ASSERT(addr >= lkp_tbl[base].start);
	*idx = base;
	if (addr < lkp_tbl[base].end)
		return lkp_tbl[base].lkey;
	/* Not found. */
	return UINT32_MAX;
}

/**
 * Insert an entry to B-tree lookup table.
 *
 * @param bt
 *   Pointer to B-tree structure.
 * @param entry
 *   Pointer to new entry to insert.
 *
 * @return
 *   0 on success, -1 on failure.
 */
static int
mr_btree_insert(struct mlx5_mr_btree *bt, struct mr_cache_entry *entry)
{
	struct mr_cache_entry *lkp_tbl;
	uint16_t idx = 0;
	size_t shift;

	MLX5_ASSERT(bt != NULL);
	MLX5_ASSERT(bt->len <= bt->size);
	MLX5_ASSERT(bt->len > 0);
	lkp_tbl = *bt->table;
	/* Find out the slot for insertion. */
	if (mr_btree_lookup(bt, &idx, entry->start) != UINT32_MAX) {
		DRV_LOG(DEBUG,
			"abort insertion to B-tree(%p): already exist at"
			" idx=%u [0x%" PRIxPTR ", 0x%" PRIxPTR ") lkey=0x%x",
			(void *)bt, idx, entry->start, entry->end, entry->lkey);
		/* Already exist, return. */
		return 0;
	}
	/* If table is full, return error. */
	if (unlikely(bt->len == bt->size)) {
		bt->overflow = 1;
		return -1;
	}
	/* Insert entry. */
	++idx;
	shift = (bt->len - idx) * sizeof(struct mr_cache_entry);
	if (shift)
		memmove(&lkp_tbl[idx + 1], &lkp_tbl[idx], shift);
	lkp_tbl[idx] = *entry;
	bt->len++;
	DRV_LOG(DEBUG,
		"inserted B-tree(%p)[%u],"
		" [0x%" PRIxPTR ", 0x%" PRIxPTR ") lkey=0x%x",
		(void *)bt, idx, entry->start, entry->end, entry->lkey);
	return 0;
}

/**
 * Initialize B-tree and allocate memory for lookup table.
 *
 * @param bt
 *   Pointer to B-tree structure.
 * @param n
 *   Number of entries to allocate.
 * @param socket
 *   NUMA socket on which memory must be allocated.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_mr_btree_init(struct mlx5_mr_btree *bt, int n, int socket)
{
	if (bt == NULL) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	MLX5_ASSERT(!bt->table && !bt->size);
	memset(bt, 0, sizeof(*bt));
	bt->table = mlx5_malloc(MLX5_MEM_RTE | MLX5_MEM_ZERO,
				sizeof(struct mr_cache_entry) * n,
				0, socket);
	if (bt->table == NULL) {
		rte_errno = ENOMEM;
		DEBUG("failed to allocate memory for btree cache on socket %d",
		      socket);
		return -rte_errno;
	}
	bt->size = n;
	/* First entry must be NULL for binary search. */
	(*bt->table)[bt->len++] = (struct mr_cache_entry) {
		.lkey = UINT32_MAX,
	};
	DEBUG("initialized B-tree %p with table %p",
	      (void *)bt, (void *)bt->table);
	return 0;
}

/**
 * Free B-tree resources.
 *
 * @param bt
 *   Pointer to B-tree structure.
 */
void
mlx5_mr_btree_free(struct mlx5_mr_btree *bt)
{
	if (bt == NULL)
		return;
	DEBUG("freeing B-tree %p with table %p",
	      (void *)bt, (void *)bt->table);
	mlx5_free(bt->table);
	memset(bt, 0, sizeof(*bt));
}

/**
 * Dump all the entries in a B-tree
 *
 * @param bt
 *   Pointer to B-tree structure.
 */
void
mlx5_mr_btree_dump(struct mlx5_mr_btree *bt __rte_unused)
{
#ifdef RTE_LIBRTE_MLX5_DEBUG
	int idx;
	struct mr_cache_entry *lkp_tbl;

	if (bt == NULL)
		return;
	lkp_tbl = *bt->table;
	for (idx = 0; idx < bt->len; ++idx) {
		struct mr_cache_entry *entry = &lkp_tbl[idx];

		DEBUG("B-tree(%p)[%u],"
		      " [0x%" PRIxPTR ", 0x%" PRIxPTR ") lkey=0x%x",
		      (void *)bt, idx, entry->start, entry->end, entry->lkey);
	}
#endif
}

/**
 * Find virtually contiguous memory chunk in a given MR.
 *
 * @param dev
 *   Pointer to MR structure.
 * @param[out] entry
 *   Pointer to returning MR cache entry. If not found, this will not be
 *   updated.
 * @param start_idx
 *   Start index of the memseg bitmap.
 *
 * @return
 *   Next index to go on lookup.
 */
static int
mr_find_next_chunk(struct mlx5_mr *mr, struct mr_cache_entry *entry,
		   int base_idx)
{
	uintptr_t start = 0;
	uintptr_t end = 0;
	uint32_t idx = 0;

	/* MR for external memory doesn't have memseg list. */
	if (mr->msl == NULL) {
		MLX5_ASSERT(mr->ms_bmp_n == 1);
		MLX5_ASSERT(mr->ms_n == 1);
		MLX5_ASSERT(base_idx == 0);
		/*
		 * Can't search it from memseg list but get it directly from
		 * pmd_mr as there's only one chunk.
		 */
		entry->start = (uintptr_t)mr->pmd_mr.addr;
		entry->end = (uintptr_t)mr->pmd_mr.addr + mr->pmd_mr.len;
		entry->lkey = rte_cpu_to_be_32(mr->pmd_mr.lkey);
		/* Returning 1 ends iteration. */
		return 1;
	}
	for (idx = base_idx; idx < mr->ms_bmp_n; ++idx) {
		if (rte_bitmap_get(mr->ms_bmp, idx)) {
			const struct rte_memseg_list *msl;
			const struct rte_memseg *ms;

			msl = mr->msl;
			ms = rte_fbarray_get(&msl->memseg_arr,
					     mr->ms_base_idx + idx);
			MLX5_ASSERT(msl->page_sz == ms->hugepage_sz);
			if (!start)
				start = ms->addr_64;
			end = ms->addr_64 + ms->hugepage_sz;
		} else if (start) {
			/* Passed the end of a fragment. */
			break;
		}
	}
	if (start) {
		/* Found one chunk. */
		entry->start = start;
		entry->end = end;
		entry->lkey = rte_cpu_to_be_32(mr->pmd_mr.lkey);
	}
	return idx;
}

/**
 * Insert a MR to the global B-tree cache. It may fail due to low-on-memory.
 * Then, this entry will have to be searched by mr_lookup_list() in
 * mlx5_mr_create() on miss.
 *
 * @param share_cache
 *   Pointer to a global shared MR cache.
 * @param mr
 *   Pointer to MR to insert.
 *
 * @return
 *   0 on success, -1 on failure.
 */
int
mlx5_mr_insert_cache(struct mlx5_mr_share_cache *share_cache,
		     struct mlx5_mr *mr)
{
	unsigned int n;

	DRV_LOG(DEBUG, "Inserting MR(%p) to global cache(%p)",
		(void *)mr, (void *)share_cache);
	for (n = 0; n < mr->ms_bmp_n; ) {
		struct mr_cache_entry entry;

		memset(&entry, 0, sizeof(entry));
		/* Find a contiguous chunk and advance the index. */
		n = mr_find_next_chunk(mr, &entry, n);
		if (!entry.end)
			break;
		if (mr_btree_insert(&share_cache->cache, &entry) < 0) {
			/*
			 * Overflowed, but the global table cannot be expanded
			 * because of deadlock.
			 */
			return -1;
		}
	}
	return 0;
}

/**
 * Look up address in the original global MR list.
 *
 * @param share_cache
 *   Pointer to a global shared MR cache.
 * @param[out] entry
 *   Pointer to returning MR cache entry. If no match, this will not be updated.
 * @param addr
 *   Search key.
 *
 * @return
 *   Found MR on match, NULL otherwise.
 */
struct mlx5_mr *
mlx5_mr_lookup_list(struct mlx5_mr_share_cache *share_cache,
		    struct mr_cache_entry *entry, uintptr_t addr)
{
	struct mlx5_mr *mr;

	/* Iterate all the existing MRs. */
	LIST_FOREACH(mr, &share_cache->mr_list, mr) {
		unsigned int n;

		if (mr->ms_n == 0)
			continue;
		for (n = 0; n < mr->ms_bmp_n; ) {
			struct mr_cache_entry ret;

			memset(&ret, 0, sizeof(ret));
			n = mr_find_next_chunk(mr, &ret, n);
			if (addr >= ret.start && addr < ret.end) {
				/* Found. */
				*entry = ret;
				return mr;
			}
		}
	}
	return NULL;
}

/**
 * Look up address on global MR cache.
 *
 * @param share_cache
 *   Pointer to a global shared MR cache.
 * @param[out] entry
 *   Pointer to returning MR cache entry. If no match, this will not be updated.
 * @param addr
 *   Search key.
 *
 * @return
 *   Searched LKey on success, UINT32_MAX on failure and rte_errno is set.
 */
uint32_t
mlx5_mr_lookup_cache(struct mlx5_mr_share_cache *share_cache,
		     struct mr_cache_entry *entry, uintptr_t addr)
{
	uint16_t idx;
	uint32_t lkey = UINT32_MAX;
	struct mlx5_mr *mr;

	/*
	 * If the global cache has overflowed since it failed to expand the
	 * B-tree table, it can't have all the existing MRs. Then, the address
	 * has to be searched by traversing the original MR list instead, which
	 * is very slow path. Otherwise, the global cache is all inclusive.
	 */
	if (!unlikely(share_cache->cache.overflow)) {
		lkey = mr_btree_lookup(&share_cache->cache, &idx, addr);
		if (lkey != UINT32_MAX)
			*entry = (*share_cache->cache.table)[idx];
	} else {
		/* Falling back to the slowest path. */
		mr = mlx5_mr_lookup_list(share_cache, entry, addr);
		if (mr != NULL)
			lkey = entry->lkey;
	}
	MLX5_ASSERT(lkey == UINT32_MAX || (addr >= entry->start &&
					   addr < entry->end));
	return lkey;
}

/**
 * Free MR resources. MR lock must not be held to avoid a deadlock. rte_free()
 * can raise memory free event and the callback function will spin on the lock.
 *
 * @param mr
 *   Pointer to MR to free.
 */
static void
mr_free(struct mlx5_mr *mr, mlx5_dereg_mr_t dereg_mr_cb)
{
	if (mr == NULL)
		return;
	DRV_LOG(DEBUG, "freeing MR(%p):", (void *)mr);
	dereg_mr_cb(&mr->pmd_mr);
	if (mr->ms_bmp != NULL)
		rte_bitmap_free(mr->ms_bmp);
	mlx5_free(mr);
}

void
mlx5_mr_rebuild_cache(struct mlx5_mr_share_cache *share_cache)
{
	struct mlx5_mr *mr;

	DRV_LOG(DEBUG, "Rebuild dev cache[] %p", (void *)share_cache);
	/* Flush cache to rebuild. */
	share_cache->cache.len = 1;
	share_cache->cache.overflow = 0;
	/* Iterate all the existing MRs. */
	LIST_FOREACH(mr, &share_cache->mr_list, mr)
		if (mlx5_mr_insert_cache(share_cache, mr) < 0)
			return;
}

/**
 * Release resources of detached MR having no online entry.
 *
 * @param share_cache
 *   Pointer to a global shared MR cache.
 */
static void
mlx5_mr_garbage_collect(struct mlx5_mr_share_cache *share_cache)
{
	struct mlx5_mr *mr_next;
	struct mlx5_mr_list free_list = LIST_HEAD_INITIALIZER(free_list);

	/* Must be called from the primary process. */
	MLX5_ASSERT(rte_eal_process_type() == RTE_PROC_PRIMARY);
	/*
	 * MR can't be freed with holding the lock because rte_free() could call
	 * memory free callback function. This will be a deadlock situation.
	 */
	rte_rwlock_write_lock(&share_cache->rwlock);
	/* Detach the whole free list and release it after unlocking. */
	free_list = share_cache->mr_free_list;
	LIST_INIT(&share_cache->mr_free_list);
	rte_rwlock_write_unlock(&share_cache->rwlock);
	/* Release resources. */
	mr_next = LIST_FIRST(&free_list);
	while (mr_next != NULL) {
		struct mlx5_mr *mr = mr_next;

		mr_next = LIST_NEXT(mr, mr);
		mr_free(mr, share_cache->dereg_mr_cb);
	}
}

/* Called during rte_memseg_contig_walk() by mlx5_mr_create(). */
static int
mr_find_contig_memsegs_cb(const struct rte_memseg_list *msl,
			  const struct rte_memseg *ms, size_t len, void *arg)
{
	struct mr_find_contig_memsegs_data *data = arg;

	if (data->addr < ms->addr_64 || data->addr >= ms->addr_64 + len)
		return 0;
	/* Found, save it and stop walking. */
	data->start = ms->addr_64;
	data->end = ms->addr_64 + len;
	data->msl = msl;
	return 1;
}

/**
 * Create a new global Memory Region (MR) for a missing virtual address.
 * This API should be called on a secondary process, then a request is sent to
 * the primary process in order to create a MR for the address. As the global MR
 * list is on the shared memory, following LKey lookup should succeed unless the
 * request fails.
 *
 * @param pd
 *   Pointer to pd of a device (net, regex, vdpa,...).
 * @param share_cache
 *   Pointer to a global shared MR cache.
 * @param[out] entry
 *   Pointer to returning MR cache entry, found in the global cache or newly
 *   created. If failed to create one, this will not be updated.
 * @param addr
 *   Target virtual address to register.
 * @param mr_ext_memseg_en
 *   Configurable flag about external memory segment enable or not.
 *
 * @return
 *   Searched LKey on success, UINT32_MAX on failure and rte_errno is set.
 */
static uint32_t
mlx5_mr_create_secondary(void *pd __rte_unused,
			 struct mlx5_mp_id *mp_id,
			 struct mlx5_mr_share_cache *share_cache,
			 struct mr_cache_entry *entry, uintptr_t addr,
			 unsigned int mr_ext_memseg_en __rte_unused)
{
	int ret;

	DEBUG("port %u requesting MR creation for address (%p)",
	      mp_id->port_id, (void *)addr);
	ret = mlx5_mp_req_mr_create(mp_id, addr);
	if (ret) {
		DEBUG("Fail to request MR creation for address (%p)",
		      (void *)addr);
		return UINT32_MAX;
	}
	rte_rwlock_read_lock(&share_cache->rwlock);
	/* Fill in output data. */
	mlx5_mr_lookup_cache(share_cache, entry, addr);
	/* Lookup can't fail. */
	MLX5_ASSERT(entry->lkey != UINT32_MAX);
	rte_rwlock_read_unlock(&share_cache->rwlock);
	DEBUG("MR CREATED by primary process for %p:\n"
	      "  [0x%" PRIxPTR ", 0x%" PRIxPTR "), lkey=0x%x",
	      (void *)addr, entry->start, entry->end, entry->lkey);
	return entry->lkey;
}

/**
 * Create a new global Memory Region (MR) for a missing virtual address.
 * Register entire virtually contiguous memory chunk around the address.
 *
 * @param pd
 *   Pointer to pd of a device (net, regex, vdpa,...).
 * @param share_cache
 *   Pointer to a global shared MR cache.
 * @param[out] entry
 *   Pointer to returning MR cache entry, found in the global cache or newly
 *   created. If failed to create one, this will not be updated.
 * @param addr
 *   Target virtual address to register.
 * @param mr_ext_memseg_en
 *   Configurable flag about external memory segment enable or not.
 *
 * @return
 *   Searched LKey on success, UINT32_MAX on failure and rte_errno is set.
 */
uint32_t
mlx5_mr_create_primary(void *pd,
		       struct mlx5_mr_share_cache *share_cache,
		       struct mr_cache_entry *entry, uintptr_t addr,
		       unsigned int mr_ext_memseg_en)
{
	struct mr_find_contig_memsegs_data data = {.addr = addr, };
	struct mr_find_contig_memsegs_data data_re;
	const struct rte_memseg_list *msl;
	const struct rte_memseg *ms;
	struct mlx5_mr *mr = NULL;
	int ms_idx_shift = -1;
	uint32_t bmp_size;
	void *bmp_mem;
	uint32_t ms_n;
	uint32_t n;
	size_t len;

	DRV_LOG(DEBUG, "Creating a MR using address (%p)", (void *)addr);
	/*
	 * Release detached MRs if any. This can't be called with holding either
	 * memory_hotplug_lock or share_cache->rwlock. MRs on the free list have
	 * been detached by the memory free event but it couldn't be released
	 * inside the callback due to deadlock. As a result, releasing resources
	 * is quite opportunistic.
	 */
	mlx5_mr_garbage_collect(share_cache);
	/*
	 * If enabled, find out a contiguous virtual address chunk in use, to
	 * which the given address belongs, in order to register maximum range.
	 * In the best case where mempools are not dynamically recreated and
	 * '--socket-mem' is specified as an EAL option, it is very likely to
	 * have only one MR(LKey) per a socket and per a hugepage-size even
	 * though the system memory is highly fragmented. As the whole memory
	 * chunk will be pinned by kernel, it can't be reused unless entire
	 * chunk is freed from EAL.
	 *
	 * If disabled, just register one memseg (page). Then, memory
	 * consumption will be minimized but it may drop performance if there
	 * are many MRs to lookup on the datapath.
	 */
	if (!mr_ext_memseg_en) {
		data.msl = rte_mem_virt2memseg_list((void *)addr);
		data.start = RTE_ALIGN_FLOOR(addr, data.msl->page_sz);
		data.end = data.start + data.msl->page_sz;
	} else if (!rte_memseg_contig_walk(mr_find_contig_memsegs_cb, &data)) {
		DRV_LOG(WARNING,
			"Unable to find virtually contiguous"
			" chunk for address (%p)."
			" rte_memseg_contig_walk() failed.", (void *)addr);
		rte_errno = ENXIO;
		goto err_nolock;
	}
alloc_resources:
	/* Addresses must be page-aligned. */
	MLX5_ASSERT(data.msl);
	MLX5_ASSERT(rte_is_aligned((void *)data.start, data.msl->page_sz));
	MLX5_ASSERT(rte_is_aligned((void *)data.end, data.msl->page_sz));
	msl = data.msl;
	ms = rte_mem_virt2memseg((void *)data.start, msl);
	len = data.end - data.start;
	MLX5_ASSERT(ms);
	MLX5_ASSERT(msl->page_sz == ms->hugepage_sz);
	/* Number of memsegs in the range. */
	ms_n = len / msl->page_sz;
	DEBUG("Extending %p to [0x%" PRIxPTR ", 0x%" PRIxPTR "),"
	      " page_sz=0x%" PRIx64 ", ms_n=%u",
	      (void *)addr, data.start, data.end, msl->page_sz, ms_n);
	/* Size of memory for bitmap. */
	bmp_size = rte_bitmap_get_memory_footprint(ms_n);
	mr = mlx5_malloc(MLX5_MEM_RTE |  MLX5_MEM_ZERO,
			 RTE_ALIGN_CEIL(sizeof(*mr), RTE_CACHE_LINE_SIZE) +
			 bmp_size, RTE_CACHE_LINE_SIZE, msl->socket_id);
	if (mr == NULL) {
		DEBUG("Unable to allocate memory for a new MR of"
		      " address (%p).", (void *)addr);
		rte_errno = ENOMEM;
		goto err_nolock;
	}
	mr->msl = msl;
	/*
	 * Save the index of the first memseg and initialize memseg bitmap. To
	 * see if a memseg of ms_idx in the memseg-list is still valid, check:
	 *	rte_bitmap_get(mr->bmp, ms_idx - mr->ms_base_idx)
	 */
	mr->ms_base_idx = rte_fbarray_find_idx(&msl->memseg_arr, ms);
	bmp_mem = RTE_PTR_ALIGN_CEIL(mr + 1, RTE_CACHE_LINE_SIZE);
	mr->ms_bmp = rte_bitmap_init(ms_n, bmp_mem, bmp_size);
	if (mr->ms_bmp == NULL) {
		DEBUG("Unable to initialize bitmap for a new MR of"
		      " address (%p).", (void *)addr);
		rte_errno = EINVAL;
		goto err_nolock;
	}
	/*
	 * Should recheck whether the extended contiguous chunk is still valid.
	 * Because memory_hotplug_lock can't be held if there's any memory
	 * related calls in a critical path, resource allocation above can't be
	 * locked. If the memory has been changed at this point, try again with
	 * just single page. If not, go on with the big chunk atomically from
	 * here.
	 */
	rte_mcfg_mem_read_lock();
	data_re = data;
	if (len > msl->page_sz &&
	    !rte_memseg_contig_walk(mr_find_contig_memsegs_cb, &data_re)) {
		DEBUG("Unable to find virtually contiguous"
		      " chunk for address (%p)."
		      " rte_memseg_contig_walk() failed.", (void *)addr);
		rte_errno = ENXIO;
		goto err_memlock;
	}
	if (data.start != data_re.start || data.end != data_re.end) {
		/*
		 * The extended contiguous chunk has been changed. Try again
		 * with single memseg instead.
		 */
		data.start = RTE_ALIGN_FLOOR(addr, msl->page_sz);
		data.end = data.start + msl->page_sz;
		rte_mcfg_mem_read_unlock();
		mr_free(mr, share_cache->dereg_mr_cb);
		goto alloc_resources;
	}
	MLX5_ASSERT(data.msl == data_re.msl);
	rte_rwlock_write_lock(&share_cache->rwlock);
	/*
	 * Check the address is really missing. If other thread already created
	 * one or it is not found due to overflow, abort and return.
	 */
	if (mlx5_mr_lookup_cache(share_cache, entry, addr) != UINT32_MAX) {
		/*
		 * Insert to the global cache table. It may fail due to
		 * low-on-memory. Then, this entry will have to be searched
		 * here again.
		 */
		mr_btree_insert(&share_cache->cache, entry);
		DEBUG("Found MR for %p on final lookup, abort", (void *)addr);
		rte_rwlock_write_unlock(&share_cache->rwlock);
		rte_mcfg_mem_read_unlock();
		/*
		 * Must be unlocked before calling rte_free() because
		 * mlx5_mr_mem_event_free_cb() can be called inside.
		 */
		mr_free(mr, share_cache->dereg_mr_cb);
		return entry->lkey;
	}
	/*
	 * Trim start and end addresses for verbs MR. Set bits for registering
	 * memsegs but exclude already registered ones. Bitmap can be
	 * fragmented.
	 */
	for (n = 0; n < ms_n; ++n) {
		uintptr_t start;
		struct mr_cache_entry ret;

		memset(&ret, 0, sizeof(ret));
		start = data_re.start + n * msl->page_sz;
		/* Exclude memsegs already registered by other MRs. */
		if (mlx5_mr_lookup_cache(share_cache, &ret, start) ==
		    UINT32_MAX) {
			/*
			 * Start from the first unregistered memseg in the
			 * extended range.
			 */
			if (ms_idx_shift == -1) {
				mr->ms_base_idx += n;
				data.start = start;
				ms_idx_shift = n;
			}
			data.end = start + msl->page_sz;
			rte_bitmap_set(mr->ms_bmp, n - ms_idx_shift);
			++mr->ms_n;
		}
	}
	len = data.end - data.start;
	mr->ms_bmp_n = len / msl->page_sz;
	MLX5_ASSERT(ms_idx_shift + mr->ms_bmp_n <= ms_n);
	/*
	 * Finally create an MR for the memory chunk. Verbs: ibv_reg_mr() can
	 * be called with holding the memory lock because it doesn't use
	 * mlx5_alloc_buf_extern() which eventually calls rte_malloc_socket()
	 * through mlx5_alloc_verbs_buf().
	 */
	share_cache->reg_mr_cb(pd, (void *)data.start, len, &mr->pmd_mr);
	if (mr->pmd_mr.obj == NULL) {
		DEBUG("Fail to create an MR for address (%p)",
		      (void *)addr);
		rte_errno = EINVAL;
		goto err_mrlock;
	}
	MLX5_ASSERT((uintptr_t)mr->pmd_mr.addr == data.start);
	MLX5_ASSERT(mr->pmd_mr.len);
	LIST_INSERT_HEAD(&share_cache->mr_list, mr, mr);
	DEBUG("MR CREATED (%p) for %p:\n"
	      "  [0x%" PRIxPTR ", 0x%" PRIxPTR "),"
	      " lkey=0x%x base_idx=%u ms_n=%u, ms_bmp_n=%u",
	      (void *)mr, (void *)addr, data.start, data.end,
	      rte_cpu_to_be_32(mr->pmd_mr.lkey),
	      mr->ms_base_idx, mr->ms_n, mr->ms_bmp_n);
	/* Insert to the global cache table. */
	mlx5_mr_insert_cache(share_cache, mr);
	/* Fill in output data. */
	mlx5_mr_lookup_cache(share_cache, entry, addr);
	/* Lookup can't fail. */
	MLX5_ASSERT(entry->lkey != UINT32_MAX);
	rte_rwlock_write_unlock(&share_cache->rwlock);
	rte_mcfg_mem_read_unlock();
	return entry->lkey;
err_mrlock:
	rte_rwlock_write_unlock(&share_cache->rwlock);
err_memlock:
	rte_mcfg_mem_read_unlock();
err_nolock:
	/*
	 * In case of error, as this can be called in a datapath, a warning
	 * message per an error is preferable instead. Must be unlocked before
	 * calling rte_free() because mlx5_mr_mem_event_free_cb() can be called
	 * inside.
	 */
	mr_free(mr, share_cache->dereg_mr_cb);
	return UINT32_MAX;
}

/**
 * Create a new global Memory Region (MR) for a missing virtual address.
 * This can be called from primary and secondary process.
 *
 * @param pd
 *   Pointer to pd handle of a device (net, regex, vdpa,...).
 * @param share_cache
 *   Pointer to a global shared MR cache.
 * @param[out] entry
 *   Pointer to returning MR cache entry, found in the global cache or newly
 *   created. If failed to create one, this will not be updated.
 * @param addr
 *   Target virtual address to register.
 *
 * @return
 *   Searched LKey on success, UINT32_MAX on failure and rte_errno is set.
 */
static uint32_t
mlx5_mr_create(void *pd, struct mlx5_mp_id *mp_id,
	       struct mlx5_mr_share_cache *share_cache,
	       struct mr_cache_entry *entry, uintptr_t addr,
	       unsigned int mr_ext_memseg_en)
{
	uint32_t ret = 0;

	switch (rte_eal_process_type()) {
	case RTE_PROC_PRIMARY:
		ret = mlx5_mr_create_primary(pd, share_cache, entry,
					     addr, mr_ext_memseg_en);
		break;
	case RTE_PROC_SECONDARY:
		ret = mlx5_mr_create_secondary(pd, mp_id, share_cache, entry,
					       addr, mr_ext_memseg_en);
		break;
	default:
		break;
	}
	return ret;
}

/**
 * Look up address in the global MR cache table. If not found, create a new MR.
 * Insert the found/created entry to local bottom-half cache table.
 *
 * @param pd
 *   Pointer to pd of a device (net, regex, vdpa,...).
 * @param share_cache
 *   Pointer to a global shared MR cache.
 * @param mr_ctrl
 *   Pointer to per-queue MR control structure.
 * @param[out] entry
 *   Pointer to returning MR cache entry, found in the global cache or newly
 *   created. If failed to create one, this is not written.
 * @param addr
 *   Search key.
 *
 * @return
 *   Searched LKey on success, UINT32_MAX on no match.
 */
static uint32_t
mr_lookup_caches(void *pd, struct mlx5_mp_id *mp_id,
		 struct mlx5_mr_share_cache *share_cache,
		 struct mlx5_mr_ctrl *mr_ctrl,
		 struct mr_cache_entry *entry, uintptr_t addr,
		 unsigned int mr_ext_memseg_en)
{
	struct mlx5_mr_btree *bt = &mr_ctrl->cache_bh;
	uint32_t lkey;
	uint16_t idx;

	/* If local cache table is full, try to double it. */
	if (unlikely(bt->len == bt->size))
		mr_btree_expand(bt, bt->size << 1);
	/* Look up in the global cache. */
	rte_rwlock_read_lock(&share_cache->rwlock);
	lkey = mr_btree_lookup(&share_cache->cache, &idx, addr);
	if (lkey != UINT32_MAX) {
		/* Found. */
		*entry = (*share_cache->cache.table)[idx];
		rte_rwlock_read_unlock(&share_cache->rwlock);
		/*
		 * Update local cache. Even if it fails, return the found entry
		 * to update top-half cache. Next time, this entry will be found
		 * in the global cache.
		 */
		mr_btree_insert(bt, entry);
		return lkey;
	}
	rte_rwlock_read_unlock(&share_cache->rwlock);
	/* First time to see the address? Create a new MR. */
	lkey = mlx5_mr_create(pd, mp_id, share_cache, entry, addr,
			      mr_ext_memseg_en);
	/*
	 * Update the local cache if successfully created a new global MR. Even
	 * if failed to create one, there's no action to take in this datapath
	 * code. As returning LKey is invalid, this will eventually make HW
	 * fail.
	 */
	if (lkey != UINT32_MAX)
		mr_btree_insert(bt, entry);
	return lkey;
}

/**
 * Bottom-half of LKey search on datapath. First search in cache_bh[] and if
 * misses, search in the global MR cache table and update the new entry to
 * per-queue local caches.
 *
 * @param pd
 *   Pointer to pd of a device (net, regex, vdpa,...).
 * @param share_cache
 *   Pointer to a global shared MR cache.
 * @param mr_ctrl
 *   Pointer to per-queue MR control structure.
 * @param addr
 *   Search key.
 *
 * @return
 *   Searched LKey on success, UINT32_MAX on no match.
 */
uint32_t mlx5_mr_addr2mr_bh(void *pd, struct mlx5_mp_id *mp_id,
			    struct mlx5_mr_share_cache *share_cache,
			    struct mlx5_mr_ctrl *mr_ctrl,
			    uintptr_t addr, unsigned int mr_ext_memseg_en)
{
	uint32_t lkey;
	uint16_t bh_idx = 0;
	/* Victim in top-half cache to replace with new entry. */
	struct mr_cache_entry *repl = &mr_ctrl->cache[mr_ctrl->head];

	/* Binary-search MR translation table. */
	lkey = mr_btree_lookup(&mr_ctrl->cache_bh, &bh_idx, addr);
	/* Update top-half cache. */
	if (likely(lkey != UINT32_MAX)) {
		*repl = (*mr_ctrl->cache_bh.table)[bh_idx];
	} else {
		/*
		 * If missed in local lookup table, search in the global cache
		 * and local cache_bh[] will be updated inside if possible.
		 * Top-half cache entry will also be updated.
		 */
		lkey = mr_lookup_caches(pd, mp_id, share_cache, mr_ctrl,
					repl, addr, mr_ext_memseg_en);
		if (unlikely(lkey == UINT32_MAX))
			return UINT32_MAX;
	}
	/* Update the most recently used entry. */
	mr_ctrl->mru = mr_ctrl->head;
	/* Point to the next victim, the oldest. */
	mr_ctrl->head = (mr_ctrl->head + 1) % MLX5_MR_CACHE_N;
	return lkey;
}

/**
 * Release all the created MRs and resources on global MR cache of a device.
 * list.
 *
 * @param share_cache
 *   Pointer to a global shared MR cache.
 */
void
mlx5_mr_release_cache(struct mlx5_mr_share_cache *share_cache)
{
	struct mlx5_mr *mr_next;

	rte_rwlock_write_lock(&share_cache->rwlock);
	/* Detach from MR list and move to free list. */
	mr_next = LIST_FIRST(&share_cache->mr_list);
	while (mr_next != NULL) {
		struct mlx5_mr *mr = mr_next;

		mr_next = LIST_NEXT(mr, mr);
		LIST_REMOVE(mr, mr);
		LIST_INSERT_HEAD(&share_cache->mr_free_list, mr, mr);
	}
	LIST_INIT(&share_cache->mr_list);
	/* Free global cache. */
	mlx5_mr_btree_free(&share_cache->cache);
	rte_rwlock_write_unlock(&share_cache->rwlock);
	/* Free all remaining MRs. */
	mlx5_mr_garbage_collect(share_cache);
}

/**
 * Flush all of the local cache entries.
 *
 * @param mr_ctrl
 *   Pointer to per-queue MR local cache.
 */
void
mlx5_mr_flush_local_cache(struct mlx5_mr_ctrl *mr_ctrl)
{
	/* Reset the most-recently-used index. */
	mr_ctrl->mru = 0;
	/* Reset the linear search array. */
	mr_ctrl->head = 0;
	memset(mr_ctrl->cache, 0, sizeof(mr_ctrl->cache));
	/* Reset the B-tree table. */
	mr_ctrl->cache_bh.len = 1;
	mr_ctrl->cache_bh.overflow = 0;
	/* Update the generation number. */
	mr_ctrl->cur_gen = *mr_ctrl->dev_gen_ptr;
	DRV_LOG(DEBUG, "mr_ctrl(%p): flushed, cur_gen=%d",
		(void *)mr_ctrl, mr_ctrl->cur_gen);
}

/**
 * Creates a memory region for external memory, that is memory which is not
 * part of the DPDK memory segments.
 *
 * @param pd
 *   Pointer to pd of a device (net, regex, vdpa,...).
 * @param addr
 *   Starting virtual address of memory.
 * @param len
 *   Length of memory segment being mapped.
 * @param socked_id
 *   Socket to allocate heap memory for the control structures.
 *
 * @return
 *   Pointer to MR structure on success, NULL otherwise.
 */
struct mlx5_mr *
mlx5_create_mr_ext(void *pd, uintptr_t addr, size_t len, int socket_id,
		   mlx5_reg_mr_t reg_mr_cb)
{
	struct mlx5_mr *mr = NULL;

	mr = mlx5_malloc(MLX5_MEM_RTE | MLX5_MEM_ZERO,
			 RTE_ALIGN_CEIL(sizeof(*mr), RTE_CACHE_LINE_SIZE),
			 RTE_CACHE_LINE_SIZE, socket_id);
	if (mr == NULL)
		return NULL;
	reg_mr_cb(pd, (void *)addr, len, &mr->pmd_mr);
	if (mr->pmd_mr.obj == NULL) {
		DRV_LOG(WARNING,
			"Fail to create MR for address (%p)",
			(void *)addr);
		mlx5_free(mr);
		return NULL;
	}
	mr->msl = NULL; /* Mark it is external memory. */
	mr->ms_bmp = NULL;
	mr->ms_n = 1;
	mr->ms_bmp_n = 1;
	DRV_LOG(DEBUG,
		"MR CREATED (%p) for external memory %p:\n"
		"  [0x%" PRIxPTR ", 0x%" PRIxPTR "),"
		" lkey=0x%x base_idx=%u ms_n=%u, ms_bmp_n=%u",
		(void *)mr, (void *)addr,
		addr, addr + len, rte_cpu_to_be_32(mr->pmd_mr.lkey),
		mr->ms_base_idx, mr->ms_n, mr->ms_bmp_n);
	return mr;
}

/**
 * Dump all the created MRs and the global cache entries.
 *
 * @param sh
 *   Pointer to Ethernet device shared context.
 */
void
mlx5_mr_dump_cache(struct mlx5_mr_share_cache *share_cache __rte_unused)
{
#ifdef RTE_LIBRTE_MLX5_DEBUG
	struct mlx5_mr *mr;
	int mr_n = 0;
	int chunk_n = 0;

	rte_rwlock_read_lock(&share_cache->rwlock);
	/* Iterate all the existing MRs. */
	LIST_FOREACH(mr, &share_cache->mr_list, mr) {
		unsigned int n;

		DEBUG("MR[%u], LKey = 0x%x, ms_n = %u, ms_bmp_n = %u",
		      mr_n++, rte_cpu_to_be_32(mr->pmd_mr.lkey),
		      mr->ms_n, mr->ms_bmp_n);
		if (mr->ms_n == 0)
			continue;
		for (n = 0; n < mr->ms_bmp_n; ) {
			struct mr_cache_entry ret = { 0, };

			n = mr_find_next_chunk(mr, &ret, n);
			if (!ret.end)
				break;
			DEBUG("  chunk[%u], [0x%" PRIxPTR ", 0x%" PRIxPTR ")",
			      chunk_n++, ret.start, ret.end);
		}
	}
	DEBUG("Dumping global cache %p", (void *)share_cache);
	mlx5_mr_btree_dump(&share_cache->cache);
	rte_rwlock_read_unlock(&share_cache->rwlock);
#endif
}
