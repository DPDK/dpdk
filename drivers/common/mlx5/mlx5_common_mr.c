/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016 6WIND S.A.
 * Copyright 2020 Mellanox Technologies, Ltd
 */
#include <stddef.h>

#include <rte_eal_memconfig.h>
#include <rte_eal_paging.h>
#include <rte_errno.h>
#include <rte_mempool.h>
#include <rte_malloc.h>
#include <rte_rwlock.h>

#include "mlx5_glue.h"
#include "mlx5_common.h"
#include "mlx5_common_mp.h"
#include "mlx5_common_mr.h"
#include "mlx5_common_os.h"
#include "mlx5_common_log.h"
#include "mlx5_malloc.h"

struct mr_find_contig_memsegs_data {
	uintptr_t addr;
	uintptr_t start;
	uintptr_t end;
	const struct rte_memseg_list *msl;
};

/* Virtual memory range. */
struct mlx5_range {
	uintptr_t start;
	uintptr_t end;
};

/** Memory region for a mempool. */
struct mlx5_mempool_mr {
	struct mlx5_pmd_mr pmd_mr;
	uint32_t refcnt; /**< Number of mempools sharing this MR. */
};

/* Mempool registration. */
struct mlx5_mempool_reg {
	LIST_ENTRY(mlx5_mempool_reg) next;
	/** Registered mempool, used to designate registrations. */
	struct rte_mempool *mp;
	/** Memory regions for the address ranges of the mempool. */
	struct mlx5_mempool_mr *mrs;
	/** Number of memory regions. */
	unsigned int mrs_n;
	/** Whether the MR were created for external pinned memory. */
	bool is_extmem;
};

void
mlx5_mprq_buf_free_cb(void *addr __rte_unused, void *opaque)
{
	struct mlx5_mprq_buf *buf = opaque;

	if (__atomic_load_n(&buf->refcnt, __ATOMIC_RELAXED) == 1) {
		rte_mempool_put(buf->mp, buf);
	} else if (unlikely(__atomic_fetch_sub(&buf->refcnt, 1,
					       __ATOMIC_RELAXED) - 1 == 0)) {
		__atomic_store_n(&buf->refcnt, 1, __ATOMIC_RELAXED);
		rte_mempool_put(buf->mp, buf);
	}
}

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
mr_btree_expand(struct mlx5_mr_btree *bt, uint32_t n)
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
mr_btree_lookup(struct mlx5_mr_btree *bt, uint32_t *idx, uintptr_t addr)
{
	struct mr_cache_entry *lkp_tbl;
	uint32_t n;
	uint32_t base = 0;

	MLX5_ASSERT(bt != NULL);
	lkp_tbl = *bt->table;
	n = bt->len;
	/* First entry must be NULL for comparison. */
	MLX5_ASSERT(bt->len > 0 || (lkp_tbl[0].start == 0 &&
				    lkp_tbl[0].lkey == UINT32_MAX));
	/* Binary search. */
	do {
		register uint32_t delta = n >> 1;

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
	uint32_t idx = 0;
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
	/* Caller must ensure that there is enough place for a new entry. */
	MLX5_ASSERT(bt->len < bt->size);
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
static int
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
		DRV_LOG(DEBUG,
			"failed to allocate memory for btree cache on socket "
			"%d", socket);
		return -rte_errno;
	}
	bt->size = n;
	/* First entry must be NULL for binary search. */
	(*bt->table)[bt->len++] = (struct mr_cache_entry) {
		.lkey = UINT32_MAX,
	};
	DRV_LOG(DEBUG, "initialized B-tree %p with table %p",
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
	DRV_LOG(DEBUG, "freeing B-tree %p with table %p",
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
	uint32_t idx;
	struct mr_cache_entry *lkp_tbl;

	if (bt == NULL)
		return;
	lkp_tbl = *bt->table;
	for (idx = 0; idx < bt->len; ++idx) {
		struct mr_cache_entry *entry = &lkp_tbl[idx];

		DRV_LOG(DEBUG, "B-tree(%p)[%u],"
		      " [0x%" PRIxPTR ", 0x%" PRIxPTR ") lkey=0x%x",
		      (void *)bt, idx, entry->start, entry->end, entry->lkey);
	}
#endif
}

/**
 * Initialize per-queue MR control descriptor.
 *
 * @param mr_ctrl
 *   Pointer to MR control structure.
 * @param dev_gen_ptr
 *   Pointer to generation number of global cache.
 * @param socket
 *   NUMA socket on which memory must be allocated.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_mr_ctrl_init(struct mlx5_mr_ctrl *mr_ctrl, uint32_t *dev_gen_ptr,
		  int socket)
{
	if (mr_ctrl == NULL) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	/* Save pointer of global generation number to check memory event. */
	mr_ctrl->dev_gen_ptr = dev_gen_ptr;
	/* Initialize B-tree and allocate memory for bottom-half cache table. */
	return mlx5_mr_btree_init(&mr_ctrl->cache_bh, MLX5_MR_BTREE_CACHE_N,
				  socket);
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
		if (mr_btree_insert(&share_cache->cache, &entry) < 0)
			return -1;
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
static uint32_t
mlx5_mr_lookup_cache(struct mlx5_mr_share_cache *share_cache,
		     struct mr_cache_entry *entry, uintptr_t addr)
{
	uint32_t idx;
	uint32_t lkey;

	lkey = mr_btree_lookup(&share_cache->cache, &idx, addr);
	if (lkey != UINT32_MAX)
		*entry = (*share_cache->cache.table)[idx];
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
void
mlx5_mr_free(struct mlx5_mr *mr, mlx5_dereg_mr_t dereg_mr_cb)
{
	if (mr == NULL)
		return;
	DRV_LOG(DEBUG, "freeing MR(%p):", (void *)mr);
	dereg_mr_cb(&mr->pmd_mr);
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
		mlx5_mr_free(mr, share_cache->dereg_mr_cb);
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
 * Get the number of virtually-contiguous chunks in the MR.
 * HW MR does not need to be already created to use this function.
 *
 * @param mr
 *   Pointer to the MR.
 *
 * @return
 *   Number of chunks.
 */
static uint32_t
mr_get_chunk_count(const struct mlx5_mr *mr)
{
	uint32_t i, count = 0;
	bool was_in_chunk = false;
	bool is_in_chunk;

	/* There is only one chunk in case of external memory. */
	if (mr->msl == NULL)
		return 1;
	for (i = 0; i < mr->ms_bmp_n; i++) {
		is_in_chunk = rte_bitmap_get(mr->ms_bmp, i);
		if (!was_in_chunk && is_in_chunk)
			count++;
		was_in_chunk = is_in_chunk;
	}
	return count;
}

/**
 * Thread-safely expand the global MR cache to at least @p new_size slots.
 *
 * @param share_cache
 *  Shared MR cache for locking.
 * @param new_size
 *  Desired cache size.
 * @param socket
 *  NUMA node.
 *
 * @return
 *  0 in success, negative on failure and rte_errno is set.
 */
int
mlx5_mr_expand_cache(struct mlx5_mr_share_cache *share_cache,
		     uint32_t size, int socket)
{
	struct mlx5_mr_btree cache = {0};
	struct mlx5_mr_btree *bt;
	struct mr_cache_entry *lkp_tbl;
	int ret;

	size = rte_align32pow2(size);
	ret = mlx5_mr_btree_init(&cache, size, socket);
	if (ret < 0)
		return ret;
	rte_rwlock_write_lock(&share_cache->rwlock);
	bt = &share_cache->cache;
	lkp_tbl = *bt->table;
	if (cache.size > bt->size) {
		rte_memcpy(cache.table, lkp_tbl, bt->len * sizeof(lkp_tbl[0]));
		RTE_SWAP(*bt, cache);
		DRV_LOG(DEBUG, "Global MR cache expanded to %u slots", size);
	}
	rte_rwlock_write_unlock(&share_cache->rwlock);
	mlx5_mr_btree_free(&cache);
	return 0;
}

/**
 * Create a new global Memory Region (MR) for a missing virtual address.
 * This API should be called on a secondary process, then a request is sent to
 * the primary process in order to create a MR for the address. As the global MR
 * list is on the shared memory, following LKey lookup should succeed unless the
 * request fails.
 *
 * @param cdev
 *   Pointer to the mlx5 common device.
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
mlx5_mr_create_secondary(struct mlx5_common_device *cdev,
			 struct mlx5_mr_share_cache *share_cache,
			 struct mr_cache_entry *entry, uintptr_t addr)
{
	int ret;

	DRV_LOG(DEBUG, "Requesting MR creation for address (%p)", (void *)addr);
	ret = mlx5_mp_req_mr_create(cdev, addr);
	if (ret) {
		DRV_LOG(DEBUG, "Fail to request MR creation for address (%p)",
			(void *)addr);
		return UINT32_MAX;
	}
	rte_rwlock_read_lock(&share_cache->rwlock);
	/* Fill in output data. */
	mlx5_mr_lookup_cache(share_cache, entry, addr);
	/* Lookup can't fail. */
	MLX5_ASSERT(entry->lkey != UINT32_MAX);
	rte_rwlock_read_unlock(&share_cache->rwlock);
	DRV_LOG(DEBUG, "MR CREATED by primary process for %p:\n"
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
static uint32_t
mlx5_mr_create_primary(void *pd,
		       struct mlx5_mr_share_cache *share_cache,
		       struct mr_cache_entry *entry, uintptr_t addr,
		       unsigned int mr_ext_memseg_en)
{
	struct mr_find_contig_memsegs_data data = {.addr = addr, };
	struct mr_find_contig_memsegs_data data_re;
	const struct rte_memseg_list *msl;
	const struct rte_memseg *ms;
	struct mlx5_mr_btree *bt;
	struct mlx5_mr *mr = NULL;
	int ms_idx_shift = -1;
	uint32_t bmp_size;
	void *bmp_mem;
	uint32_t ms_n;
	uint32_t n;
	uint32_t chunks_n;
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
find_range:
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
	DRV_LOG(DEBUG, "Extending %p to [0x%" PRIxPTR ", 0x%" PRIxPTR "),"
	      " page_sz=0x%" PRIx64 ", ms_n=%u",
	      (void *)addr, data.start, data.end, msl->page_sz, ms_n);
	/* Size of memory for bitmap. */
	bmp_size = rte_bitmap_get_memory_footprint(ms_n);
	mr = mlx5_malloc(MLX5_MEM_RTE |  MLX5_MEM_ZERO,
			 RTE_ALIGN_CEIL(sizeof(*mr), RTE_CACHE_LINE_SIZE) +
			 bmp_size, RTE_CACHE_LINE_SIZE, msl->socket_id);
	if (mr == NULL) {
		DRV_LOG(DEBUG, "Unable to allocate memory for a new MR of"
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
		DRV_LOG(DEBUG, "Unable to initialize bitmap for a new MR of"
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
		DRV_LOG(DEBUG,
			"Unable to find virtually contiguous chunk for address "
			"(%p). rte_memseg_contig_walk() failed.", (void *)addr);
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
		mlx5_mr_free(mr, share_cache->dereg_mr_cb);
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
		DRV_LOG(DEBUG, "Found MR for %p on final lookup, abort",
			(void *)addr);
		rte_rwlock_write_unlock(&share_cache->rwlock);
		rte_mcfg_mem_read_unlock();
		/*
		 * Must be unlocked before calling rte_free() because
		 * mlx5_mr_mem_event_free_cb() can be called inside.
		 */
		mlx5_mr_free(mr, share_cache->dereg_mr_cb);
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
	 * It is now known how many entries will be used in the global cache.
	 * If there is not enough, expand the cache.
	 * This cannot be done while holding the memory hotplug lock.
	 * While it is released, memory layout may change,
	 * so the process must be repeated from the beginning.
	 */
	bt = &share_cache->cache;
	chunks_n = mr_get_chunk_count(mr);
	if (bt->len + chunks_n > bt->size) {
		struct mlx5_common_device *cdev;
		uint32_t size;

		size = bt->size + chunks_n;
		MLX5_ASSERT(size > bt->size);
		cdev = container_of(share_cache, struct mlx5_common_device,
				    mr_scache);
		rte_rwlock_write_unlock(&share_cache->rwlock);
		rte_mcfg_mem_read_unlock();
		if (mlx5_mr_expand_cache(share_cache, size,
					 cdev->dev->numa_node) < 0) {
			DRV_LOG(ERR, "Failed to expand global MR cache to %u slots",
				size);
			goto err_nolock;
		}
		goto find_range;
	}
	/*
	 * Finally create an MR for the memory chunk. Verbs: ibv_reg_mr() can
	 * be called with holding the memory lock because it doesn't use
	 * mlx5_alloc_buf_extern() which eventually calls rte_malloc_socket()
	 * through mlx5_alloc_verbs_buf().
	 */
	share_cache->reg_mr_cb(pd, (void *)data.start, len, &mr->pmd_mr);
	if (mr->pmd_mr.obj == NULL) {
		DRV_LOG(DEBUG, "Fail to create an MR for address (%p)",
		      (void *)addr);
		rte_errno = EINVAL;
		goto err_mrlock;
	}
	MLX5_ASSERT((uintptr_t)mr->pmd_mr.addr == data.start);
	MLX5_ASSERT(mr->pmd_mr.len);
	LIST_INSERT_HEAD(&share_cache->mr_list, mr, mr);
	DRV_LOG(DEBUG, "MR CREATED (%p) for %p:\n"
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
	mlx5_mr_free(mr, share_cache->dereg_mr_cb);
	return UINT32_MAX;
}

/**
 * Create a new global Memory Region (MR) for a missing virtual address.
 * This can be called from primary and secondary process.
 *
 * @param cdev
 *   Pointer to the mlx5 common device.
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
uint32_t
mlx5_mr_create(struct mlx5_common_device *cdev,
	       struct mlx5_mr_share_cache *share_cache,
	       struct mr_cache_entry *entry, uintptr_t addr)
{
	uint32_t ret = 0;

	switch (rte_eal_process_type()) {
	case RTE_PROC_PRIMARY:
		ret = mlx5_mr_create_primary(cdev->pd, share_cache, entry, addr,
					     cdev->config.mr_ext_memseg_en);
		break;
	case RTE_PROC_SECONDARY:
		ret = mlx5_mr_create_secondary(cdev, share_cache, entry, addr);
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
mr_lookup_caches(struct mlx5_mr_ctrl *mr_ctrl,
		 struct mr_cache_entry *entry, uintptr_t addr)
{
	struct mlx5_mr_share_cache *share_cache =
		container_of(mr_ctrl->dev_gen_ptr, struct mlx5_mr_share_cache,
			     dev_gen);
	struct mlx5_common_device *cdev =
		container_of(share_cache, struct mlx5_common_device, mr_scache);
	struct mlx5_mr_btree *bt = &mr_ctrl->cache_bh;
	uint32_t lkey;
	uint32_t idx;

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
	lkey = mlx5_mr_create(cdev, share_cache, entry, addr);
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
 * @param mr_ctrl
 *   Pointer to per-queue MR control structure.
 * @param addr
 *   Search key.
 *
 * @return
 *   Searched LKey on success, UINT32_MAX on no match.
 */
uint32_t
mlx5_mr_addr2mr_bh(struct mlx5_mr_ctrl *mr_ctrl, uintptr_t addr)
{
	uint32_t lkey;
	uint32_t bh_idx = 0;
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
		lkey = mr_lookup_caches(mr_ctrl, repl, addr);
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
 * Release all the created MRs and resources on global MR cache of a device
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
 * Initialize global MR cache of a device.
 *
 * @param share_cache
 *   Pointer to a global shared MR cache.
 * @param socket
 *   NUMA socket on which memory must be allocated.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_mr_create_cache(struct mlx5_mr_share_cache *share_cache, int socket)
{
	/* Set the reg_mr and dereg_mr callback functions */
	mlx5_os_set_reg_mr_cb(&share_cache->reg_mr_cb,
			      &share_cache->dereg_mr_cb);
	rte_rwlock_init(&share_cache->rwlock);
	rte_rwlock_init(&share_cache->mprwlock);
	/* Initialize B-tree and allocate memory for global MR cache table. */
	return mlx5_mr_btree_init(&share_cache->cache,
				  MLX5_MR_BTREE_CACHE_N * 2, socket);
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
 * Callback for memory free event. Iterate freed memsegs and check whether it
 * belongs to an existing MR. If found, clear the bit from bitmap of MR. As a
 * result, the MR would be fragmented. If it becomes empty, the MR will be freed
 * later by mlx5_mr_garbage_collect(). Even if this callback is called from a
 * secondary process, the garbage collector will be called in primary process
 * as the secondary process can't call mlx5_mr_create().
 *
 * The global cache must be rebuilt if there's any change and this event has to
 * be propagated to dataplane threads to flush the local caches.
 *
 * @param share_cache
 *   Pointer to a global shared MR cache.
 * @param ibdev_name
 *   Name of ibv device.
 * @param addr
 *   Address of freed memory.
 * @param len
 *   Size of freed memory.
 */
void
mlx5_free_mr_by_addr(struct mlx5_mr_share_cache *share_cache,
		     const char *ibdev_name, const void *addr, size_t len)
{
	const struct rte_memseg_list *msl;
	struct mlx5_mr *mr;
	int ms_n;
	int i;
	int rebuild = 0;

	DRV_LOG(DEBUG, "device %s free callback: addr=%p, len=%zu",
		ibdev_name, addr, len);
	msl = rte_mem_virt2memseg_list(addr);
	/* addr and len must be page-aligned. */
	MLX5_ASSERT((uintptr_t)addr ==
		    RTE_ALIGN((uintptr_t)addr, msl->page_sz));
	MLX5_ASSERT(len == RTE_ALIGN(len, msl->page_sz));
	ms_n = len / msl->page_sz;
	rte_rwlock_write_lock(&share_cache->rwlock);
	/* Clear bits of freed memsegs from MR. */
	for (i = 0; i < ms_n; ++i) {
		const struct rte_memseg *ms;
		struct mr_cache_entry entry;
		uintptr_t start;
		int ms_idx;
		uint32_t pos;

		/* Find MR having this memseg. */
		start = (uintptr_t)addr + i * msl->page_sz;
		mr = mlx5_mr_lookup_list(share_cache, &entry, start);
		if (mr == NULL)
			continue;
		MLX5_ASSERT(mr->msl); /* Can't be external memory. */
		ms = rte_mem_virt2memseg((void *)start, msl);
		MLX5_ASSERT(ms != NULL);
		MLX5_ASSERT(msl->page_sz == ms->hugepage_sz);
		ms_idx = rte_fbarray_find_idx(&msl->memseg_arr, ms);
		pos = ms_idx - mr->ms_base_idx;
		MLX5_ASSERT(rte_bitmap_get(mr->ms_bmp, pos));
		MLX5_ASSERT(pos < mr->ms_bmp_n);
		DRV_LOG(DEBUG, "device %s MR(%p): clear bitmap[%u] for addr %p",
			ibdev_name, (void *)mr, pos, (void *)start);
		rte_bitmap_clear(mr->ms_bmp, pos);
		if (--mr->ms_n == 0) {
			LIST_REMOVE(mr, mr);
			LIST_INSERT_HEAD(&share_cache->mr_free_list, mr, mr);
			DRV_LOG(DEBUG, "device %s remove MR(%p) from list",
				ibdev_name, (void *)mr);
		}
		/*
		 * MR is fragmented or will be freed. the global cache must be
		 * rebuilt.
		 */
		rebuild = 1;
	}
	if (rebuild) {
		mlx5_mr_rebuild_cache(share_cache);
		/*
		 * No explicit wmb is needed after updating dev_gen due to
		 * store-release ordering in unlock that provides the
		 * implicit barrier at the software visible level.
		 */
		++share_cache->dev_gen;
		DRV_LOG(DEBUG, "broadcasting local cache flush, gen=%d",
			share_cache->dev_gen);
	}
	rte_rwlock_write_unlock(&share_cache->rwlock);
}

/**
 * Dump all the created MRs and the global cache entries.
 *
 * @param share_cache
 *   Pointer to a global shared MR cache.
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

		DRV_LOG(DEBUG, "MR[%u], LKey = 0x%x, ms_n = %u, ms_bmp_n = %u",
		      mr_n++, rte_cpu_to_be_32(mr->pmd_mr.lkey),
		      mr->ms_n, mr->ms_bmp_n);
		if (mr->ms_n == 0)
			continue;
		for (n = 0; n < mr->ms_bmp_n; ) {
			struct mr_cache_entry ret = { 0, };

			n = mr_find_next_chunk(mr, &ret, n);
			if (!ret.end)
				break;
			DRV_LOG(DEBUG,
				"  chunk[%u], [0x%" PRIxPTR ", 0x%" PRIxPTR ")",
				chunk_n++, ret.start, ret.end);
		}
	}
	DRV_LOG(DEBUG, "Dumping global cache %p", (void *)share_cache);
	mlx5_mr_btree_dump(&share_cache->cache);
	rte_rwlock_read_unlock(&share_cache->rwlock);
#endif
}

static int
mlx5_range_compare_start(const void *lhs, const void *rhs)
{
	const struct mlx5_range *r1 = lhs, *r2 = rhs;

	if (r1->start > r2->start)
		return 1;
	else if (r1->start < r2->start)
		return -1;
	return 0;
}

static void
mlx5_range_from_mempool_chunk(struct rte_mempool *mp, void *opaque,
			      struct rte_mempool_memhdr *memhdr,
			      unsigned int idx)
{
	struct mlx5_range *ranges = opaque, *range = &ranges[idx];
	uintptr_t start = (uintptr_t)memhdr->addr;
	uint64_t page_size = rte_mem_page_size();

	RTE_SET_USED(mp);
	range->start = RTE_ALIGN_FLOOR(start, page_size);
	range->end = RTE_ALIGN_CEIL(start + memhdr->len, page_size);
}

/**
 * Collect page-aligned memory ranges of the mempool.
 */
static int
mlx5_mempool_get_chunks(struct rte_mempool *mp, struct mlx5_range **out,
			unsigned int *out_n)
{
	unsigned int n;

	DRV_LOG(DEBUG, "Collecting chunks of regular mempool %s", mp->name);
	n = mp->nb_mem_chunks;
	*out = calloc(n, sizeof(**out));
	if (*out == NULL)
		return -1;
	rte_mempool_mem_iter(mp, mlx5_range_from_mempool_chunk, *out);
	*out_n = n;
	return 0;
}

struct mlx5_mempool_get_extmem_data {
	struct mlx5_range *heap;
	unsigned int heap_size;
	int ret;
};

static void
mlx5_mempool_get_extmem_cb(struct rte_mempool *mp, void *opaque,
			   void *obj, unsigned int obj_idx)
{
	struct mlx5_mempool_get_extmem_data *data = opaque;
	struct rte_mbuf *mbuf = obj;
	uintptr_t addr = (uintptr_t)mbuf->buf_addr;
	struct mlx5_range *seg, *heap;
	struct rte_memseg_list *msl;
	size_t page_size;
	uintptr_t page_start;
	unsigned int pos = 0, len = data->heap_size, delta;

	RTE_SET_USED(mp);
	RTE_SET_USED(obj_idx);
	if (data->ret < 0)
		return;
	/* Binary search for an already visited page. */
	while (len > 1) {
		delta = len / 2;
		if (addr < data->heap[pos + delta].start) {
			len = delta;
		} else {
			pos += delta;
			len -= delta;
		}
	}
	if (data->heap != NULL) {
		seg = &data->heap[pos];
		if (seg->start <= addr && addr < seg->end)
			return;
	}
	/* Determine the page boundaries and remember them. */
	heap = realloc(data->heap, sizeof(heap[0]) * (data->heap_size + 1));
	if (heap == NULL) {
		free(data->heap);
		data->heap = NULL;
		data->ret = -1;
		return;
	}
	data->heap = heap;
	data->heap_size++;
	seg = &heap[data->heap_size - 1];
	msl = rte_mem_virt2memseg_list((void *)addr);
	page_size = msl != NULL ? msl->page_sz : rte_mem_page_size();
	page_start = RTE_PTR_ALIGN_FLOOR(addr, page_size);
	seg->start = page_start;
	seg->end = page_start + page_size;
	/* Maintain the heap order. */
	qsort(data->heap, data->heap_size, sizeof(heap[0]),
	      mlx5_range_compare_start);
}

/**
 * Recover pages of external memory as close as possible
 * for a mempool with RTE_PKTMBUF_POOL_PINNED_EXT_BUF.
 * Pages are stored in a heap for efficient search, for mbufs are many.
 */
static int
mlx5_mempool_get_extmem(struct rte_mempool *mp, struct mlx5_range **out,
			unsigned int *out_n)
{
	struct mlx5_mempool_get_extmem_data data;

	DRV_LOG(DEBUG, "Recovering external pinned pages of mempool %s",
		mp->name);
	memset(&data, 0, sizeof(data));
	rte_mempool_obj_iter(mp, mlx5_mempool_get_extmem_cb, &data);
	*out = data.heap;
	*out_n = data.heap_size;
	return data.ret;
}

/**
 * Get VA-contiguous ranges of the mempool memory.
 * Each range start and end is aligned to the system page size.
 *
 * @param[in] mp
 *   Analyzed mempool.
 * @param[in] is_extmem
 *   Whether the pool is contains only external pinned buffers.
 * @param[out] out
 *   Receives the ranges, caller must release it with free().
 * @param[out] out_n
 *   Receives the number of @p out elements.
 *
 * @return
 *   0 on success, (-1) on failure.
 */
static int
mlx5_get_mempool_ranges(struct rte_mempool *mp, bool is_extmem,
			struct mlx5_range **out, unsigned int *out_n)
{
	struct mlx5_range *chunks;
	unsigned int chunks_n, contig_n, i;
	int ret;

	/* Collect the pool underlying memory. */
	ret = is_extmem ? mlx5_mempool_get_extmem(mp, &chunks, &chunks_n) :
			  mlx5_mempool_get_chunks(mp, &chunks, &chunks_n);
	if (ret < 0)
		return ret;
	/* Merge adjacent chunks and place them at the beginning. */
	qsort(chunks, chunks_n, sizeof(chunks[0]), mlx5_range_compare_start);
	contig_n = 1;
	for (i = 1; i < chunks_n; i++)
		if (chunks[i - 1].end != chunks[i].start) {
			chunks[contig_n - 1].end = chunks[i - 1].end;
			chunks[contig_n] = chunks[i];
			contig_n++;
		}
	/* Extend the last contiguous chunk to the end of the mempool. */
	chunks[contig_n - 1].end = chunks[i - 1].end;
	*out = chunks;
	*out_n = contig_n;
	return 0;
}

/**
 * Analyze mempool memory to select memory ranges to register.
 *
 * @param[in] mp
 *   Mempool to analyze.
 * @param[in] is_extmem
 *   Whether the pool is contains only external pinned buffers.
 * @param[out] out
 *   Receives memory ranges to register, aligned to the system page size.
 *   The caller must release them with free().
 * @param[out] out_n
 *   Receives the number of @p out items.
 * @param[out] share_hugepage
 *   Receives True if the entire pool resides within a single hugepage.
 *
 * @return
 *   0 on success, (-1) on failure.
 */
static int
mlx5_mempool_reg_analyze(struct rte_mempool *mp, bool is_extmem,
			 struct mlx5_range **out, unsigned int *out_n,
			 bool *share_hugepage)
{
	struct mlx5_range *ranges = NULL;
	unsigned int i, ranges_n = 0;
	struct rte_memseg_list *msl;

	if (mlx5_get_mempool_ranges(mp, is_extmem, &ranges, &ranges_n) < 0) {
		DRV_LOG(ERR, "Cannot get address ranges for mempool %s",
			mp->name);
		return -1;
	}
	/* Check if the hugepage of the pool can be shared. */
	*share_hugepage = false;
	msl = rte_mem_virt2memseg_list((void *)ranges[0].start);
	if (msl != NULL) {
		uint64_t hugepage_sz = 0;

		/* Check that all ranges are on pages of the same size. */
		for (i = 0; i < ranges_n; i++) {
			if (hugepage_sz != 0 && hugepage_sz != msl->page_sz)
				break;
			hugepage_sz = msl->page_sz;
		}
		if (i == ranges_n) {
			/*
			 * If the entire pool is within one hugepage,
			 * combine all ranges into one of the hugepage size.
			 */
			uintptr_t reg_start = ranges[0].start;
			uintptr_t reg_end = ranges[ranges_n - 1].end;
			uintptr_t hugepage_start =
				RTE_ALIGN_FLOOR(reg_start, hugepage_sz);
			uintptr_t hugepage_end = hugepage_start + hugepage_sz;
			if (reg_end < hugepage_end) {
				ranges[0].start = hugepage_start;
				ranges[0].end = hugepage_end;
				ranges_n = 1;
				*share_hugepage = true;
			}
		}
	}
	*out = ranges;
	*out_n = ranges_n;
	return 0;
}

/** Create a registration object for the mempool. */
static struct mlx5_mempool_reg *
mlx5_mempool_reg_create(struct rte_mempool *mp, unsigned int mrs_n,
			bool is_extmem)
{
	struct mlx5_mempool_reg *mpr = NULL;

	mpr = mlx5_malloc(MLX5_MEM_RTE | MLX5_MEM_ZERO,
			  sizeof(struct mlx5_mempool_reg),
			  RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
	if (mpr == NULL) {
		DRV_LOG(ERR, "Cannot allocate mempool %s registration object",
			mp->name);
		return NULL;
	}
	mpr->mrs = mlx5_malloc(MLX5_MEM_RTE | MLX5_MEM_ZERO,
			       mrs_n * sizeof(struct mlx5_mempool_mr),
			       RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
	if (!mpr->mrs) {
		DRV_LOG(ERR, "Cannot allocate mempool %s registration MRs",
			mp->name);
		mlx5_free(mpr);
		return NULL;
	}
	mpr->mp = mp;
	mpr->mrs_n = mrs_n;
	mpr->is_extmem = is_extmem;
	return mpr;
}

/**
 * Destroy a mempool registration object.
 *
 * @param standalone
 *   Whether @p mpr owns its MRs exclusively, i.e. they are not shared.
 */
static void
mlx5_mempool_reg_destroy(struct mlx5_mr_share_cache *share_cache,
			 struct mlx5_mempool_reg *mpr, bool standalone)
{
	if (standalone) {
		unsigned int i;

		for (i = 0; i < mpr->mrs_n; i++)
			share_cache->dereg_mr_cb(&mpr->mrs[i].pmd_mr);
		mlx5_free(mpr->mrs);
	}
	mlx5_free(mpr);
}

/** Find registration object of a mempool. */
static struct mlx5_mempool_reg *
mlx5_mempool_reg_lookup(struct mlx5_mr_share_cache *share_cache,
			struct rte_mempool *mp)
{
	struct mlx5_mempool_reg *mpr;

	LIST_FOREACH(mpr, &share_cache->mempool_reg_list, next)
		if (mpr->mp == mp)
			break;
	return mpr;
}

/** Increment reference counters of MRs used in the registration. */
static void
mlx5_mempool_reg_attach(struct mlx5_mempool_reg *mpr)
{
	unsigned int i;

	for (i = 0; i < mpr->mrs_n; i++)
		__atomic_fetch_add(&mpr->mrs[i].refcnt, 1, __ATOMIC_RELAXED);
}

/**
 * Decrement reference counters of MRs used in the registration.
 *
 * @return True if no more references to @p mpr MRs exist, False otherwise.
 */
static bool
mlx5_mempool_reg_detach(struct mlx5_mempool_reg *mpr)
{
	unsigned int i;
	bool ret = false;

	for (i = 0; i < mpr->mrs_n; i++)
		ret |= __atomic_fetch_sub(&mpr->mrs[i].refcnt, 1,
					  __ATOMIC_RELAXED) - 1 == 0;
	return ret;
}

static int
mlx5_mr_mempool_register_primary(struct mlx5_mr_share_cache *share_cache,
				 void *pd, struct rte_mempool *mp,
				 bool is_extmem)
{
	struct mlx5_range *ranges = NULL;
	struct mlx5_mempool_reg *mpr, *old_mpr, *new_mpr;
	unsigned int i, ranges_n;
	bool share_hugepage, standalone = false;
	int ret = -1;

	/* Early check to avoid unnecessary creation of MRs. */
	rte_rwlock_read_lock(&share_cache->rwlock);
	old_mpr = mlx5_mempool_reg_lookup(share_cache, mp);
	rte_rwlock_read_unlock(&share_cache->rwlock);
	if (old_mpr != NULL && (!is_extmem || old_mpr->is_extmem)) {
		DRV_LOG(DEBUG, "Mempool %s is already registered for PD %p",
			mp->name, pd);
		rte_errno = EEXIST;
		goto exit;
	}
	if (mlx5_mempool_reg_analyze(mp, is_extmem, &ranges, &ranges_n,
				     &share_hugepage) < 0) {
		DRV_LOG(ERR, "Cannot get mempool %s memory ranges", mp->name);
		rte_errno = ENOMEM;
		goto exit;
	}
	new_mpr = mlx5_mempool_reg_create(mp, ranges_n, is_extmem);
	if (new_mpr == NULL) {
		DRV_LOG(ERR,
			"Cannot create a registration object for mempool %s in PD %p",
			mp->name, pd);
		rte_errno = ENOMEM;
		goto exit;
	}
	/*
	 * If the entire mempool fits in a single hugepage, the MR for this
	 * hugepage can be shared across mempools that also fit in it.
	 */
	if (share_hugepage) {
		rte_rwlock_write_lock(&share_cache->rwlock);
		LIST_FOREACH(mpr, &share_cache->mempool_reg_list, next) {
			if (mpr->mrs[0].pmd_mr.addr == (void *)ranges[0].start)
				break;
		}
		if (mpr != NULL) {
			new_mpr->mrs = mpr->mrs;
			mlx5_mempool_reg_attach(new_mpr);
			LIST_INSERT_HEAD(&share_cache->mempool_reg_list,
					 new_mpr, next);
		}
		rte_rwlock_write_unlock(&share_cache->rwlock);
		if (mpr != NULL) {
			DRV_LOG(DEBUG, "Shared MR %#x in PD %p for mempool %s with mempool %s",
				mpr->mrs[0].pmd_mr.lkey, pd, mp->name,
				mpr->mp->name);
			ret = 0;
			goto exit;
		}
	}
	for (i = 0; i < ranges_n; i++) {
		struct mlx5_mempool_mr *mr = &new_mpr->mrs[i];
		const struct mlx5_range *range = &ranges[i];
		size_t len = range->end - range->start;

		if (share_cache->reg_mr_cb(pd, (void *)range->start, len,
		    &mr->pmd_mr) < 0) {
			DRV_LOG(ERR,
				"Failed to create an MR in PD %p for address range "
				"[0x%" PRIxPTR ", 0x%" PRIxPTR "] (%zu bytes) for mempool %s",
				pd, range->start, range->end, len, mp->name);
			break;
		}
		DRV_LOG(DEBUG,
			"Created a new MR %#x in PD %p for address range "
			"[0x%" PRIxPTR ", 0x%" PRIxPTR "] (%zu bytes) for mempool %s",
			mr->pmd_mr.lkey, pd, range->start, range->end, len,
			mp->name);
	}
	if (i != ranges_n) {
		mlx5_mempool_reg_destroy(share_cache, new_mpr, true);
		rte_errno = EINVAL;
		goto exit;
	}
	/* Concurrent registration is not supposed to happen. */
	rte_rwlock_write_lock(&share_cache->rwlock);
	mpr = mlx5_mempool_reg_lookup(share_cache, mp);
	if (mpr == old_mpr && old_mpr != NULL) {
		LIST_REMOVE(old_mpr, next);
		standalone = mlx5_mempool_reg_detach(mpr);
		/* No need to flush the cache: old MRs cannot be in use. */
		mpr = NULL;
	}
	if (mpr == NULL) {
		mlx5_mempool_reg_attach(new_mpr);
		LIST_INSERT_HEAD(&share_cache->mempool_reg_list, new_mpr, next);
		ret = 0;
	}
	rte_rwlock_write_unlock(&share_cache->rwlock);
	if (mpr != NULL) {
		DRV_LOG(DEBUG, "Mempool %s is already registered for PD %p",
			mp->name, pd);
		mlx5_mempool_reg_destroy(share_cache, new_mpr, true);
		rte_errno = EEXIST;
		goto exit;
	} else if (old_mpr != NULL) {
		DRV_LOG(DEBUG, "Mempool %s registration for PD %p updated for external memory",
			mp->name, pd);
		mlx5_mempool_reg_destroy(share_cache, old_mpr, standalone);
	}
exit:
	free(ranges);
	return ret;
}

static int
mlx5_mr_mempool_register_secondary(struct mlx5_common_device *cdev,
				   struct rte_mempool *mp, bool is_extmem)
{
	return mlx5_mp_req_mempool_reg(cdev, mp, true, is_extmem);
}

/**
 * Register the memory of a mempool in the protection domain.
 *
 * @param cdev
 *   Pointer to the mlx5 common device.
 * @param mp
 *   Mempool to register.
 *
 * @return
 *   0 on success, (-1) on failure and rte_errno is set.
 */
int
mlx5_mr_mempool_register(struct mlx5_common_device *cdev,
			 struct rte_mempool *mp, bool is_extmem)
{
	if (mp->flags & RTE_MEMPOOL_F_NON_IO)
		return 0;
	switch (rte_eal_process_type()) {
	case RTE_PROC_PRIMARY:
		return mlx5_mr_mempool_register_primary(&cdev->mr_scache,
							cdev->pd, mp,
							is_extmem);
	case RTE_PROC_SECONDARY:
		return mlx5_mr_mempool_register_secondary(cdev, mp, is_extmem);
	default:
		return -1;
	}
}

static int
mlx5_mr_mempool_unregister_primary(struct mlx5_mr_share_cache *share_cache,
				   struct rte_mempool *mp)
{
	struct mlx5_mempool_reg *mpr;
	bool standalone = false;

	rte_rwlock_write_lock(&share_cache->rwlock);
	LIST_FOREACH(mpr, &share_cache->mempool_reg_list, next)
		if (mpr->mp == mp) {
			LIST_REMOVE(mpr, next);
			standalone = mlx5_mempool_reg_detach(mpr);
			if (standalone)
				/*
				 * The unlock operation below provides a memory
				 * barrier due to its store-release semantics.
				 */
				++share_cache->dev_gen;
			break;
		}
	rte_rwlock_write_unlock(&share_cache->rwlock);
	if (mpr == NULL) {
		rte_errno = ENOENT;
		return -1;
	}
	mlx5_mempool_reg_destroy(share_cache, mpr, standalone);
	return 0;
}

static int
mlx5_mr_mempool_unregister_secondary(struct mlx5_common_device *cdev,
				     struct rte_mempool *mp)
{
	return mlx5_mp_req_mempool_reg(cdev, mp, false, false /* is_extmem */);
}

/**
 * Unregister the memory of a mempool from the protection domain.
 *
 * @param cdev
 *   Pointer to the mlx5 common device.
 * @param mp
 *   Mempool to unregister.
 *
 * @return
 *   0 on success, (-1) on failure and rte_errno is set.
 */
int
mlx5_mr_mempool_unregister(struct mlx5_common_device *cdev,
			   struct rte_mempool *mp)
{
	if (mp->flags & RTE_MEMPOOL_F_NON_IO)
		return 0;
	switch (rte_eal_process_type()) {
	case RTE_PROC_PRIMARY:
		return mlx5_mr_mempool_unregister_primary(&cdev->mr_scache, mp);
	case RTE_PROC_SECONDARY:
		return mlx5_mr_mempool_unregister_secondary(cdev, mp);
	default:
		return -1;
	}
}

/**
 * Lookup a MR key by and address in a registered mempool.
 *
 * @param mpr
 *   Mempool registration object.
 * @param addr
 *   Address within the mempool.
 * @param entry
 *   Bottom-half cache entry to fill.
 *
 * @return
 *   MR key or UINT32_MAX on failure, which can only happen
 *   if the address is not from within the mempool.
 */
static uint32_t
mlx5_mempool_reg_addr2mr(struct mlx5_mempool_reg *mpr, uintptr_t addr,
			 struct mr_cache_entry *entry)
{
	uint32_t lkey = UINT32_MAX;
	unsigned int i;

	for (i = 0; i < mpr->mrs_n; i++) {
		const struct mlx5_pmd_mr *mr = &mpr->mrs[i].pmd_mr;
		uintptr_t mr_start = (uintptr_t)mr->addr;
		uintptr_t mr_end = mr_start + mr->len;

		if (mr_start <= addr && addr < mr_end) {
			lkey = rte_cpu_to_be_32(mr->lkey);
			entry->start = mr_start;
			entry->end = mr_end;
			entry->lkey = lkey;
			break;
		}
	}
	return lkey;
}

/**
 * Update bottom-half cache from the list of mempool registrations.
 *
 * @param mr_ctrl
 *   Per-queue MR control handle.
 * @param entry
 *   Pointer to an entry in the bottom-half cache to update
 *   with the MR lkey looked up.
 * @param mp
 *   Mempool containing the address.
 * @param addr
 *   Address to lookup.
 * @return
 *   MR lkey on success, UINT32_MAX on failure.
 */
static uint32_t
mlx5_lookup_mempool_regs(struct mlx5_mr_ctrl *mr_ctrl,
			 struct mr_cache_entry *entry,
			 struct rte_mempool *mp, uintptr_t addr)
{
	struct mlx5_mr_share_cache *share_cache =
		container_of(mr_ctrl->dev_gen_ptr, struct mlx5_mr_share_cache,
			     dev_gen);
	struct mlx5_mr_btree *bt = &mr_ctrl->cache_bh;
	struct mlx5_mempool_reg *mpr;
	uint32_t lkey = UINT32_MAX;

	/* If local cache table is full, try to double it. */
	if (unlikely(bt->len == bt->size))
		mr_btree_expand(bt, bt->size << 1);
	/* Look up in mempool registrations. */
	rte_rwlock_read_lock(&share_cache->rwlock);
	mpr = mlx5_mempool_reg_lookup(share_cache, mp);
	if (mpr != NULL)
		lkey = mlx5_mempool_reg_addr2mr(mpr, addr, entry);
	rte_rwlock_read_unlock(&share_cache->rwlock);
	/*
	 * Update local cache. Even if it fails, return the found entry
	 * to update top-half cache. Next time, this entry will be found
	 * in the global cache.
	 */
	if (lkey != UINT32_MAX)
		mr_btree_insert(bt, entry);
	return lkey;
}

/**
 * Populate cache with LKeys of all MRs used by the mempool.
 * It is intended to be used to register Rx mempools in advance.
 *
 * @param mr_ctrl
 *  Per-queue MR control handle.
 * @param mp
 *  Registered memory pool.
 *
 * @return
 *  0 on success, (-1) on failure and rte_errno is set.
 */
int
mlx5_mr_mempool_populate_cache(struct mlx5_mr_ctrl *mr_ctrl,
			       struct rte_mempool *mp)
{
	struct mlx5_mr_share_cache *share_cache =
		container_of(mr_ctrl->dev_gen_ptr, struct mlx5_mr_share_cache,
			     dev_gen);
	struct mlx5_mr_btree *bt = &mr_ctrl->cache_bh;
	struct mlx5_mempool_reg *mpr;
	unsigned int i;

	/*
	 * Registration is valid after the lock is released,
	 * because the function is called after the mempool is registered.
	 */
	rte_rwlock_read_lock(&share_cache->rwlock);
	mpr = mlx5_mempool_reg_lookup(share_cache, mp);
	rte_rwlock_read_unlock(&share_cache->rwlock);
	if (mpr == NULL) {
		DRV_LOG(ERR, "Mempool %s is not registered", mp->name);
		rte_errno = ENOENT;
		return -1;
	}
	for (i = 0; i < mpr->mrs_n; i++) {
		struct mlx5_mempool_mr *mr = &mpr->mrs[i];
		struct mr_cache_entry entry;
		uint32_t lkey;
		uint32_t idx;

		lkey = mr_btree_lookup(bt, &idx, (uintptr_t)mr->pmd_mr.addr);
		if (lkey != UINT32_MAX)
			continue;
		if (bt->len == bt->size)
			mr_btree_expand(bt, bt->size << 1);
		entry.start = (uintptr_t)mr->pmd_mr.addr;
		entry.end = entry.start + mr->pmd_mr.len;
		entry.lkey = rte_cpu_to_be_32(mr->pmd_mr.lkey);
		if (mr_btree_insert(bt, &entry) < 0) {
			DRV_LOG(ERR, "Cannot insert cache entry for mempool %s MR %08x",
				mp->name, entry.lkey);
			rte_errno = EINVAL;
			return -1;
		}
	}
	return 0;
}

/**
 * Bottom-half lookup for the address from the mempool.
 *
 * @param mr_ctrl
 *   Per-queue MR control handle.
 * @param mp
 *   Mempool containing the address.
 * @param addr
 *   Address to lookup.
 * @return
 *   MR lkey on success, UINT32_MAX on failure.
 */
uint32_t
mlx5_mr_mempool2mr_bh(struct mlx5_mr_ctrl *mr_ctrl,
		      struct rte_mempool *mp, uintptr_t addr)
{
	struct mr_cache_entry *repl = &mr_ctrl->cache[mr_ctrl->head];
	uint32_t lkey;
	uint32_t bh_idx = 0;

	/* Binary-search MR translation table. */
	lkey = mr_btree_lookup(&mr_ctrl->cache_bh, &bh_idx, addr);
	/* Update top-half cache. */
	if (likely(lkey != UINT32_MAX)) {
		*repl = (*mr_ctrl->cache_bh.table)[bh_idx];
	} else {
		lkey = mlx5_lookup_mempool_regs(mr_ctrl, repl, mp, addr);
		/* Can only fail if the address is not from the mempool. */
		if (unlikely(lkey == UINT32_MAX))
			return UINT32_MAX;
	}
	/* Update the most recently used entry. */
	mr_ctrl->mru = mr_ctrl->head;
	/* Point to the next victim, the oldest. */
	mr_ctrl->head = (mr_ctrl->head + 1) % MLX5_MR_CACHE_N;
	return lkey;
}

uint32_t
mlx5_mr_mb2mr_bh(struct mlx5_mr_ctrl *mr_ctrl, struct rte_mbuf *mb)
{
	struct rte_mempool *mp;
	struct mlx5_mprq_buf *buf;
	uint32_t lkey;
	uintptr_t addr = (uintptr_t)mb->buf_addr;
	struct mlx5_mr_share_cache *share_cache =
		container_of(mr_ctrl->dev_gen_ptr, struct mlx5_mr_share_cache,
			     dev_gen);
	struct mlx5_common_device *cdev =
		container_of(share_cache, struct mlx5_common_device, mr_scache);
	bool external, mprq, pinned = false;

	/* Recover MPRQ mempool. */
	external = RTE_MBUF_HAS_EXTBUF(mb);
	if (external && mb->shinfo->free_cb == mlx5_mprq_buf_free_cb) {
		mprq = true;
		buf = mb->shinfo->fcb_opaque;
		mp = buf->mp;
	} else {
		mprq = false;
		mp = mlx5_mb2mp(mb);
		pinned = rte_pktmbuf_priv_flags(mp) &
			 RTE_PKTMBUF_POOL_F_PINNED_EXT_BUF;
	}
	if (!external || mprq || pinned) {
		lkey = mlx5_mr_mempool2mr_bh(mr_ctrl, mp, addr);
		if (lkey != UINT32_MAX)
			return lkey;
		/* MPRQ is always registered. */
		MLX5_ASSERT(!mprq);
	}
	/* Register pinned external memory if the mempool is not used for Rx. */
	if (cdev->config.mr_mempool_reg_en && pinned) {
		if (mlx5_mr_mempool_register(cdev, mp, true) < 0)
			return UINT32_MAX;
		lkey = mlx5_mr_mempool2mr_bh(mr_ctrl, mp, addr);
		MLX5_ASSERT(lkey != UINT32_MAX);
		return lkey;
	}
	/* Fallback to generic mechanism in corner cases. */
	return mlx5_mr_addr2mr_bh(mr_ctrl, addr);
}
