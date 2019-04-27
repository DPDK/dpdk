/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016 6WIND S.A.
 * Copyright 2016 Mellanox Technologies, Ltd
 */

#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_mempool.h>
#include <rte_malloc.h>
#include <rte_rwlock.h>
#include <rte_bus_pci.h>

#include "mlx5.h"
#include "mlx5_mr.h"
#include "mlx5_rxtx.h"
#include "mlx5_glue.h"

struct mr_find_contig_memsegs_data {
	uintptr_t addr;
	uintptr_t start;
	uintptr_t end;
	const struct rte_memseg_list *msl;
};

struct mr_update_mp_data {
	struct rte_eth_dev *dev;
	struct mlx5_mr_ctrl *mr_ctrl;
	int ret;
};

/**
 * Expand B-tree table to a given size. Can't be called with holding
 * memory_hotplug_lock or sh->mr.rwlock due to rte_realloc().
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
	mem = rte_realloc(bt->table, n * sizeof(struct mlx5_mr_cache), 0);
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
	struct mlx5_mr_cache *lkp_tbl;
	uint16_t n;
	uint16_t base = 0;

	assert(bt != NULL);
	lkp_tbl = *bt->table;
	n = bt->len;
	/* First entry must be NULL for comparison. */
	assert(bt->len > 0 || (lkp_tbl[0].start == 0 &&
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
	assert(addr >= lkp_tbl[base].start);
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
mr_btree_insert(struct mlx5_mr_btree *bt, struct mlx5_mr_cache *entry)
{
	struct mlx5_mr_cache *lkp_tbl;
	uint16_t idx = 0;
	size_t shift;

	assert(bt != NULL);
	assert(bt->len <= bt->size);
	assert(bt->len > 0);
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
	shift = (bt->len - idx) * sizeof(struct mlx5_mr_cache);
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
	assert(!bt->table && !bt->size);
	memset(bt, 0, sizeof(*bt));
	bt->table = rte_calloc_socket("B-tree table",
				      n, sizeof(struct mlx5_mr_cache),
				      0, socket);
	if (bt->table == NULL) {
		rte_errno = ENOMEM;
		DEBUG("failed to allocate memory for btree cache on socket %d",
		      socket);
		return -rte_errno;
	}
	bt->size = n;
	/* First entry must be NULL for binary search. */
	(*bt->table)[bt->len++] = (struct mlx5_mr_cache) {
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
	rte_free(bt->table);
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
#ifndef NDEBUG
	int idx;
	struct mlx5_mr_cache *lkp_tbl;

	if (bt == NULL)
		return;
	lkp_tbl = *bt->table;
	for (idx = 0; idx < bt->len; ++idx) {
		struct mlx5_mr_cache *entry = &lkp_tbl[idx];

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
mr_find_next_chunk(struct mlx5_mr *mr, struct mlx5_mr_cache *entry,
		   int base_idx)
{
	uintptr_t start = 0;
	uintptr_t end = 0;
	uint32_t idx = 0;

	/* MR for external memory doesn't have memseg list. */
	if (mr->msl == NULL) {
		struct ibv_mr *ibv_mr = mr->ibv_mr;

		assert(mr->ms_bmp_n == 1);
		assert(mr->ms_n == 1);
		assert(base_idx == 0);
		/*
		 * Can't search it from memseg list but get it directly from
		 * verbs MR as there's only one chunk.
		 */
		entry->start = (uintptr_t)ibv_mr->addr;
		entry->end = (uintptr_t)ibv_mr->addr + mr->ibv_mr->length;
		entry->lkey = rte_cpu_to_be_32(mr->ibv_mr->lkey);
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
			assert(msl->page_sz == ms->hugepage_sz);
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
		entry->lkey = rte_cpu_to_be_32(mr->ibv_mr->lkey);
	}
	return idx;
}

/**
 * Insert a MR to the global B-tree cache. It may fail due to low-on-memory.
 * Then, this entry will have to be searched by mr_lookup_dev_list() in
 * mlx5_mr_create() on miss.
 *
 * @param dev
 *   Pointer to Ethernet device shared context.
 * @param mr
 *   Pointer to MR to insert.
 *
 * @return
 *   0 on success, -1 on failure.
 */
static int
mr_insert_dev_cache(struct mlx5_ibv_shared *sh, struct mlx5_mr *mr)
{
	unsigned int n;

	DRV_LOG(DEBUG, "device %s inserting MR(%p) to global cache",
		sh->ibdev_name, (void *)mr);
	for (n = 0; n < mr->ms_bmp_n; ) {
		struct mlx5_mr_cache entry;

		memset(&entry, 0, sizeof(entry));
		/* Find a contiguous chunk and advance the index. */
		n = mr_find_next_chunk(mr, &entry, n);
		if (!entry.end)
			break;
		if (mr_btree_insert(&sh->mr.cache, &entry) < 0) {
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
 * @param sh
 *   Pointer to Ethernet device shared context.
 * @param[out] entry
 *   Pointer to returning MR cache entry. If no match, this will not be updated.
 * @param addr
 *   Search key.
 *
 * @return
 *   Found MR on match, NULL otherwise.
 */
static struct mlx5_mr *
mr_lookup_dev_list(struct mlx5_ibv_shared *sh, struct mlx5_mr_cache *entry,
		   uintptr_t addr)
{
	struct mlx5_mr *mr;

	/* Iterate all the existing MRs. */
	LIST_FOREACH(mr, &sh->mr.mr_list, mr) {
		unsigned int n;

		if (mr->ms_n == 0)
			continue;
		for (n = 0; n < mr->ms_bmp_n; ) {
			struct mlx5_mr_cache ret;

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
 * Look up address on device.
 *
 * @param dev
 *   Pointer to Ethernet device shared context.
 * @param[out] entry
 *   Pointer to returning MR cache entry. If no match, this will not be updated.
 * @param addr
 *   Search key.
 *
 * @return
 *   Searched LKey on success, UINT32_MAX on failure and rte_errno is set.
 */
static uint32_t
mr_lookup_dev(struct mlx5_ibv_shared *sh, struct mlx5_mr_cache *entry,
	      uintptr_t addr)
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
	if (!unlikely(sh->mr.cache.overflow)) {
		lkey = mr_btree_lookup(&sh->mr.cache, &idx, addr);
		if (lkey != UINT32_MAX)
			*entry = (*sh->mr.cache.table)[idx];
	} else {
		/* Falling back to the slowest path. */
		mr = mr_lookup_dev_list(sh, entry, addr);
		if (mr != NULL)
			lkey = entry->lkey;
	}
	assert(lkey == UINT32_MAX || (addr >= entry->start &&
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
mr_free(struct mlx5_mr *mr)
{
	if (mr == NULL)
		return;
	DRV_LOG(DEBUG, "freeing MR(%p):", (void *)mr);
	if (mr->ibv_mr != NULL)
		claim_zero(mlx5_glue->dereg_mr(mr->ibv_mr));
	if (mr->ms_bmp != NULL)
		rte_bitmap_free(mr->ms_bmp);
	rte_free(mr);
}

/**
 * Release resources of detached MR having no online entry.
 *
 * @param sh
 *   Pointer to Ethernet device shared context.
 */
static void
mlx5_mr_garbage_collect(struct mlx5_ibv_shared *sh)
{
	struct mlx5_mr *mr_next;
	struct mlx5_mr_list free_list = LIST_HEAD_INITIALIZER(free_list);

	/* Must be called from the primary process. */
	assert(rte_eal_process_type() == RTE_PROC_PRIMARY);
	/*
	 * MR can't be freed with holding the lock because rte_free() could call
	 * memory free callback function. This will be a deadlock situation.
	 */
	rte_rwlock_write_lock(&sh->mr.rwlock);
	/* Detach the whole free list and release it after unlocking. */
	free_list = sh->mr.mr_free_list;
	LIST_INIT(&sh->mr.mr_free_list);
	rte_rwlock_write_unlock(&sh->mr.rwlock);
	/* Release resources. */
	mr_next = LIST_FIRST(&free_list);
	while (mr_next != NULL) {
		struct mlx5_mr *mr = mr_next;

		mr_next = LIST_NEXT(mr, mr);
		mr_free(mr);
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
 * @param dev
 *   Pointer to Ethernet device.
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
mlx5_mr_create_secondary(struct rte_eth_dev *dev, struct mlx5_mr_cache *entry,
			 uintptr_t addr)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	int ret;

	DEBUG("port %u requesting MR creation for address (%p)",
	      dev->data->port_id, (void *)addr);
	ret = mlx5_mp_req_mr_create(dev, addr);
	if (ret) {
		DEBUG("port %u fail to request MR creation for address (%p)",
		      dev->data->port_id, (void *)addr);
		return UINT32_MAX;
	}
	rte_rwlock_read_lock(&priv->sh->mr.rwlock);
	/* Fill in output data. */
	mr_lookup_dev(priv->sh, entry, addr);
	/* Lookup can't fail. */
	assert(entry->lkey != UINT32_MAX);
	rte_rwlock_read_unlock(&priv->sh->mr.rwlock);
	DEBUG("port %u MR CREATED by primary process for %p:\n"
	      "  [0x%" PRIxPTR ", 0x%" PRIxPTR "), lkey=0x%x",
	      dev->data->port_id, (void *)addr,
	      entry->start, entry->end, entry->lkey);
	return entry->lkey;
}

/**
 * Create a new global Memory Region (MR) for a missing virtual address.
 * Register entire virtually contiguous memory chunk around the address.
 * This must be called from the primary process.
 *
 * @param dev
 *   Pointer to Ethernet device.
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
mlx5_mr_create_primary(struct rte_eth_dev *dev, struct mlx5_mr_cache *entry,
		       uintptr_t addr)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_ibv_shared *sh = priv->sh;
	struct mlx5_dev_config *config = &priv->config;
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	const struct rte_memseg_list *msl;
	const struct rte_memseg *ms;
	struct mlx5_mr *mr = NULL;
	size_t len;
	uint32_t ms_n;
	uint32_t bmp_size;
	void *bmp_mem;
	int ms_idx_shift = -1;
	unsigned int n;
	struct mr_find_contig_memsegs_data data = {
		.addr = addr,
	};
	struct mr_find_contig_memsegs_data data_re;

	DRV_LOG(DEBUG, "port %u creating a MR using address (%p)",
		dev->data->port_id, (void *)addr);
	/*
	 * Release detached MRs if any. This can't be called with holding either
	 * memory_hotplug_lock or sh->mr.rwlock. MRs on the free list have
	 * been detached by the memory free event but it couldn't be released
	 * inside the callback due to deadlock. As a result, releasing resources
	 * is quite opportunistic.
	 */
	mlx5_mr_garbage_collect(sh);
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
	if (!config->mr_ext_memseg_en) {
		data.msl = rte_mem_virt2memseg_list((void *)addr);
		data.start = RTE_ALIGN_FLOOR(addr, data.msl->page_sz);
		data.end = data.start + data.msl->page_sz;
	} else if (!rte_memseg_contig_walk(mr_find_contig_memsegs_cb, &data)) {
		DRV_LOG(WARNING,
			"port %u unable to find virtually contiguous"
			" chunk for address (%p)."
			" rte_memseg_contig_walk() failed.",
			dev->data->port_id, (void *)addr);
		rte_errno = ENXIO;
		goto err_nolock;
	}
alloc_resources:
	/* Addresses must be page-aligned. */
	assert(rte_is_aligned((void *)data.start, data.msl->page_sz));
	assert(rte_is_aligned((void *)data.end, data.msl->page_sz));
	msl = data.msl;
	ms = rte_mem_virt2memseg((void *)data.start, msl);
	len = data.end - data.start;
	assert(msl->page_sz == ms->hugepage_sz);
	/* Number of memsegs in the range. */
	ms_n = len / msl->page_sz;
	DEBUG("port %u extending %p to [0x%" PRIxPTR ", 0x%" PRIxPTR "),"
	      " page_sz=0x%" PRIx64 ", ms_n=%u",
	      dev->data->port_id, (void *)addr,
	      data.start, data.end, msl->page_sz, ms_n);
	/* Size of memory for bitmap. */
	bmp_size = rte_bitmap_get_memory_footprint(ms_n);
	mr = rte_zmalloc_socket(NULL,
				RTE_ALIGN_CEIL(sizeof(*mr),
					       RTE_CACHE_LINE_SIZE) +
				bmp_size,
				RTE_CACHE_LINE_SIZE, msl->socket_id);
	if (mr == NULL) {
		DEBUG("port %u unable to allocate memory for a new MR of"
		      " address (%p).",
		      dev->data->port_id, (void *)addr);
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
		DEBUG("port %u unable to initialize bitmap for a new MR of"
		      " address (%p).",
		      dev->data->port_id, (void *)addr);
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
	rte_rwlock_read_lock(&mcfg->memory_hotplug_lock);
	data_re = data;
	if (len > msl->page_sz &&
	    !rte_memseg_contig_walk(mr_find_contig_memsegs_cb, &data_re)) {
		DEBUG("port %u unable to find virtually contiguous"
		      " chunk for address (%p)."
		      " rte_memseg_contig_walk() failed.",
		      dev->data->port_id, (void *)addr);
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
		rte_rwlock_read_unlock(&mcfg->memory_hotplug_lock);
		mr_free(mr);
		goto alloc_resources;
	}
	assert(data.msl == data_re.msl);
	rte_rwlock_write_lock(&sh->mr.rwlock);
	/*
	 * Check the address is really missing. If other thread already created
	 * one or it is not found due to overflow, abort and return.
	 */
	if (mr_lookup_dev(sh, entry, addr) != UINT32_MAX) {
		/*
		 * Insert to the global cache table. It may fail due to
		 * low-on-memory. Then, this entry will have to be searched
		 * here again.
		 */
		mr_btree_insert(&sh->mr.cache, entry);
		DEBUG("port %u found MR for %p on final lookup, abort",
		      dev->data->port_id, (void *)addr);
		rte_rwlock_write_unlock(&sh->mr.rwlock);
		rte_rwlock_read_unlock(&mcfg->memory_hotplug_lock);
		/*
		 * Must be unlocked before calling rte_free() because
		 * mlx5_mr_mem_event_free_cb() can be called inside.
		 */
		mr_free(mr);
		return entry->lkey;
	}
	/*
	 * Trim start and end addresses for verbs MR. Set bits for registering
	 * memsegs but exclude already registered ones. Bitmap can be
	 * fragmented.
	 */
	for (n = 0; n < ms_n; ++n) {
		uintptr_t start;
		struct mlx5_mr_cache ret;

		memset(&ret, 0, sizeof(ret));
		start = data_re.start + n * msl->page_sz;
		/* Exclude memsegs already registered by other MRs. */
		if (mr_lookup_dev(sh, &ret, start) == UINT32_MAX) {
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
	assert(ms_idx_shift + mr->ms_bmp_n <= ms_n);
	/*
	 * Finally create a verbs MR for the memory chunk. ibv_reg_mr() can be
	 * called with holding the memory lock because it doesn't use
	 * mlx5_alloc_buf_extern() which eventually calls rte_malloc_socket()
	 * through mlx5_alloc_verbs_buf().
	 */
	mr->ibv_mr = mlx5_glue->reg_mr(sh->pd, (void *)data.start, len,
				       IBV_ACCESS_LOCAL_WRITE);
	if (mr->ibv_mr == NULL) {
		DEBUG("port %u fail to create a verbs MR for address (%p)",
		      dev->data->port_id, (void *)addr);
		rte_errno = EINVAL;
		goto err_mrlock;
	}
	assert((uintptr_t)mr->ibv_mr->addr == data.start);
	assert(mr->ibv_mr->length == len);
	LIST_INSERT_HEAD(&sh->mr.mr_list, mr, mr);
	DEBUG("port %u MR CREATED (%p) for %p:\n"
	      "  [0x%" PRIxPTR ", 0x%" PRIxPTR "),"
	      " lkey=0x%x base_idx=%u ms_n=%u, ms_bmp_n=%u",
	      dev->data->port_id, (void *)mr, (void *)addr,
	      data.start, data.end, rte_cpu_to_be_32(mr->ibv_mr->lkey),
	      mr->ms_base_idx, mr->ms_n, mr->ms_bmp_n);
	/* Insert to the global cache table. */
	mr_insert_dev_cache(sh, mr);
	/* Fill in output data. */
	mr_lookup_dev(sh, entry, addr);
	/* Lookup can't fail. */
	assert(entry->lkey != UINT32_MAX);
	rte_rwlock_write_unlock(&sh->mr.rwlock);
	rte_rwlock_read_unlock(&mcfg->memory_hotplug_lock);
	return entry->lkey;
err_mrlock:
	rte_rwlock_write_unlock(&sh->mr.rwlock);
err_memlock:
	rte_rwlock_read_unlock(&mcfg->memory_hotplug_lock);
err_nolock:
	/*
	 * In case of error, as this can be called in a datapath, a warning
	 * message per an error is preferable instead. Must be unlocked before
	 * calling rte_free() because mlx5_mr_mem_event_free_cb() can be called
	 * inside.
	 */
	mr_free(mr);
	return UINT32_MAX;
}

/**
 * Create a new global Memory Region (MR) for a missing virtual address.
 * This can be called from primary and secondary process.
 *
 * @param dev
 *   Pointer to Ethernet device.
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
mlx5_mr_create(struct rte_eth_dev *dev, struct mlx5_mr_cache *entry,
	       uintptr_t addr)
{
	uint32_t ret = 0;

	switch (rte_eal_process_type()) {
	case RTE_PROC_PRIMARY:
		ret = mlx5_mr_create_primary(dev, entry, addr);
		break;
	case RTE_PROC_SECONDARY:
		ret = mlx5_mr_create_secondary(dev, entry, addr);
		break;
	default:
		break;
	}
	return ret;
}

/**
 * Rebuild the global B-tree cache of device from the original MR list.
 *
 * @param sh
 *   Pointer to Ethernet device shared context.
 */
static void
mr_rebuild_dev_cache(struct mlx5_ibv_shared *sh)
{
	struct mlx5_mr *mr;

	DRV_LOG(DEBUG, "device %s rebuild dev cache[]", sh->ibdev_name);
	/* Flush cache to rebuild. */
	sh->mr.cache.len = 1;
	sh->mr.cache.overflow = 0;
	/* Iterate all the existing MRs. */
	LIST_FOREACH(mr, &sh->mr.mr_list, mr)
		if (mr_insert_dev_cache(sh, mr) < 0)
			return;
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
 * @param sh
 *   Pointer to the Ethernet device shared context.
 * @param addr
 *   Address of freed memory.
 * @param len
 *   Size of freed memory.
 */
static void
mlx5_mr_mem_event_free_cb(struct mlx5_ibv_shared *sh,
			  const void *addr, size_t len)
{
	const struct rte_memseg_list *msl;
	struct mlx5_mr *mr;
	int ms_n;
	int i;
	int rebuild = 0;

	DEBUG("device %s free callback: addr=%p, len=%zu",
	      sh->ibdev_name, addr, len);
	msl = rte_mem_virt2memseg_list(addr);
	/* addr and len must be page-aligned. */
	assert((uintptr_t)addr == RTE_ALIGN((uintptr_t)addr, msl->page_sz));
	assert(len == RTE_ALIGN(len, msl->page_sz));
	ms_n = len / msl->page_sz;
	rte_rwlock_write_lock(&sh->mr.rwlock);
	/* Clear bits of freed memsegs from MR. */
	for (i = 0; i < ms_n; ++i) {
		const struct rte_memseg *ms;
		struct mlx5_mr_cache entry;
		uintptr_t start;
		int ms_idx;
		uint32_t pos;

		/* Find MR having this memseg. */
		start = (uintptr_t)addr + i * msl->page_sz;
		mr = mr_lookup_dev_list(sh, &entry, start);
		if (mr == NULL)
			continue;
		assert(mr->msl); /* Can't be external memory. */
		ms = rte_mem_virt2memseg((void *)start, msl);
		assert(ms != NULL);
		assert(msl->page_sz == ms->hugepage_sz);
		ms_idx = rte_fbarray_find_idx(&msl->memseg_arr, ms);
		pos = ms_idx - mr->ms_base_idx;
		assert(rte_bitmap_get(mr->ms_bmp, pos));
		assert(pos < mr->ms_bmp_n);
		DEBUG("device %s MR(%p): clear bitmap[%u] for addr %p",
		      sh->ibdev_name, (void *)mr, pos, (void *)start);
		rte_bitmap_clear(mr->ms_bmp, pos);
		if (--mr->ms_n == 0) {
			LIST_REMOVE(mr, mr);
			LIST_INSERT_HEAD(&sh->mr.mr_free_list, mr, mr);
			DEBUG("device %s remove MR(%p) from list",
			      sh->ibdev_name, (void *)mr);
		}
		/*
		 * MR is fragmented or will be freed. the global cache must be
		 * rebuilt.
		 */
		rebuild = 1;
	}
	if (rebuild) {
		mr_rebuild_dev_cache(sh);
		/*
		 * Flush local caches by propagating invalidation across cores.
		 * rte_smp_wmb() is enough to synchronize this event. If one of
		 * freed memsegs is seen by other core, that means the memseg
		 * has been allocated by allocator, which will come after this
		 * free call. Therefore, this store instruction (incrementing
		 * generation below) will be guaranteed to be seen by other core
		 * before the core sees the newly allocated memory.
		 */
		++sh->mr.dev_gen;
		DEBUG("broadcasting local cache flush, gen=%d",
		      sh->mr.dev_gen);
		rte_smp_wmb();
	}
	rte_rwlock_write_unlock(&sh->mr.rwlock);
}

/**
 * Callback for memory event. This can be called from both primary and secondary
 * process.
 *
 * @param event_type
 *   Memory event type.
 * @param addr
 *   Address of memory.
 * @param len
 *   Size of memory.
 */
void
mlx5_mr_mem_event_cb(enum rte_mem_event event_type, const void *addr,
		     size_t len, void *arg __rte_unused)
{
	struct mlx5_ibv_shared *sh;
	struct mlx5_dev_list *dev_list = &mlx5_shared_data->mem_event_cb_list;

	/* Must be called from the primary process. */
	assert(rte_eal_process_type() == RTE_PROC_PRIMARY);
	switch (event_type) {
	case RTE_MEM_EVENT_FREE:
		rte_rwlock_write_lock(&mlx5_shared_data->mem_event_rwlock);
		/* Iterate all the existing mlx5 devices. */
		LIST_FOREACH(sh, dev_list, mem_event_cb)
			mlx5_mr_mem_event_free_cb(sh, addr, len);
		rte_rwlock_write_unlock(&mlx5_shared_data->mem_event_rwlock);
		break;
	case RTE_MEM_EVENT_ALLOC:
	default:
		break;
	}
}

/**
 * Look up address in the global MR cache table. If not found, create a new MR.
 * Insert the found/created entry to local bottom-half cache table.
 *
 * @param dev
 *   Pointer to Ethernet device.
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
mlx5_mr_lookup_dev(struct rte_eth_dev *dev, struct mlx5_mr_ctrl *mr_ctrl,
		   struct mlx5_mr_cache *entry, uintptr_t addr)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_ibv_shared *sh = priv->sh;
	struct mlx5_mr_btree *bt = &mr_ctrl->cache_bh;
	uint16_t idx;
	uint32_t lkey;

	/* If local cache table is full, try to double it. */
	if (unlikely(bt->len == bt->size))
		mr_btree_expand(bt, bt->size << 1);
	/* Look up in the global cache. */
	rte_rwlock_read_lock(&sh->mr.rwlock);
	lkey = mr_btree_lookup(&sh->mr.cache, &idx, addr);
	if (lkey != UINT32_MAX) {
		/* Found. */
		*entry = (*sh->mr.cache.table)[idx];
		rte_rwlock_read_unlock(&sh->mr.rwlock);
		/*
		 * Update local cache. Even if it fails, return the found entry
		 * to update top-half cache. Next time, this entry will be found
		 * in the global cache.
		 */
		mr_btree_insert(bt, entry);
		return lkey;
	}
	rte_rwlock_read_unlock(&sh->mr.rwlock);
	/* First time to see the address? Create a new MR. */
	lkey = mlx5_mr_create(dev, entry, addr);
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
 * Bottom-half of LKey search on datapath. Firstly search in cache_bh[] and if
 * misses, search in the global MR cache table and update the new entry to
 * per-queue local caches.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param mr_ctrl
 *   Pointer to per-queue MR control structure.
 * @param addr
 *   Search key.
 *
 * @return
 *   Searched LKey on success, UINT32_MAX on no match.
 */
static uint32_t
mlx5_mr_addr2mr_bh(struct rte_eth_dev *dev, struct mlx5_mr_ctrl *mr_ctrl,
		   uintptr_t addr)
{
	uint32_t lkey;
	uint16_t bh_idx = 0;
	/* Victim in top-half cache to replace with new entry. */
	struct mlx5_mr_cache *repl = &mr_ctrl->cache[mr_ctrl->head];

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
		lkey = mlx5_mr_lookup_dev(dev, mr_ctrl, repl, addr);
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
 * Bottom-half of LKey search on Rx.
 *
 * @param rxq
 *   Pointer to Rx queue structure.
 * @param addr
 *   Search key.
 *
 * @return
 *   Searched LKey on success, UINT32_MAX on no match.
 */
uint32_t
mlx5_rx_addr2mr_bh(struct mlx5_rxq_data *rxq, uintptr_t addr)
{
	struct mlx5_rxq_ctrl *rxq_ctrl =
		container_of(rxq, struct mlx5_rxq_ctrl, rxq);
	struct mlx5_mr_ctrl *mr_ctrl = &rxq->mr_ctrl;
	struct mlx5_priv *priv = rxq_ctrl->priv;

	return mlx5_mr_addr2mr_bh(ETH_DEV(priv), mr_ctrl, addr);
}

/**
 * Bottom-half of LKey search on Tx.
 *
 * @param txq
 *   Pointer to Tx queue structure.
 * @param addr
 *   Search key.
 *
 * @return
 *   Searched LKey on success, UINT32_MAX on no match.
 */
static uint32_t
mlx5_tx_addr2mr_bh(struct mlx5_txq_data *txq, uintptr_t addr)
{
	struct mlx5_txq_ctrl *txq_ctrl =
		container_of(txq, struct mlx5_txq_ctrl, txq);
	struct mlx5_mr_ctrl *mr_ctrl = &txq->mr_ctrl;
	struct mlx5_priv *priv = txq_ctrl->priv;

	return mlx5_mr_addr2mr_bh(ETH_DEV(priv), mr_ctrl, addr);
}

/**
 * Bottom-half of LKey search on Tx. If it can't be searched in the memseg
 * list, register the mempool of the mbuf as externally allocated memory.
 *
 * @param txq
 *   Pointer to Tx queue structure.
 * @param mb
 *   Pointer to mbuf.
 *
 * @return
 *   Searched LKey on success, UINT32_MAX on no match.
 */
uint32_t
mlx5_tx_mb2mr_bh(struct mlx5_txq_data *txq, struct rte_mbuf *mb)
{
	uintptr_t addr = (uintptr_t)mb->buf_addr;
	uint32_t lkey;

	lkey = mlx5_tx_addr2mr_bh(txq, addr);
	if (lkey == UINT32_MAX && rte_errno == ENXIO) {
		/* Mempool may have externally allocated memory. */
		return mlx5_tx_update_ext_mp(txq, addr, mlx5_mb2mp(mb));
	}
	return lkey;
}

/**
 * Flush all of the local cache entries.
 *
 * @param mr_ctrl
 *   Pointer to per-queue MR control structure.
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
 * @param dev
 *   Pointer to the ethernet device.
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
static struct mlx5_mr *
mlx5_create_mr_ext(struct rte_eth_dev *dev, uintptr_t addr, size_t len,
		   int socket_id)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_mr *mr = NULL;

	mr = rte_zmalloc_socket(NULL,
				RTE_ALIGN_CEIL(sizeof(*mr),
					       RTE_CACHE_LINE_SIZE),
				RTE_CACHE_LINE_SIZE, socket_id);
	if (mr == NULL)
		return NULL;
	mr->ibv_mr = mlx5_glue->reg_mr(priv->sh->pd, (void *)addr, len,
				       IBV_ACCESS_LOCAL_WRITE);
	if (mr->ibv_mr == NULL) {
		DRV_LOG(WARNING,
			"port %u fail to create a verbs MR for address (%p)",
			dev->data->port_id, (void *)addr);
		rte_free(mr);
		return NULL;
	}
	mr->msl = NULL; /* Mark it is external memory. */
	mr->ms_bmp = NULL;
	mr->ms_n = 1;
	mr->ms_bmp_n = 1;
	DRV_LOG(DEBUG,
		"port %u MR CREATED (%p) for external memory %p:\n"
		"  [0x%" PRIxPTR ", 0x%" PRIxPTR "),"
		" lkey=0x%x base_idx=%u ms_n=%u, ms_bmp_n=%u",
		dev->data->port_id, (void *)mr, (void *)addr,
		addr, addr + len, rte_cpu_to_be_32(mr->ibv_mr->lkey),
		mr->ms_base_idx, mr->ms_n, mr->ms_bmp_n);
	return mr;
}

/**
 * Called during rte_mempool_mem_iter() by mlx5_mr_update_ext_mp().
 *
 * Externally allocated chunk is registered and a MR is created for the chunk.
 * The MR object is added to the global list. If memseg list of a MR object
 * (mr->msl) is null, the MR object can be regarded as externally allocated
 * memory.
 *
 * Once external memory is registered, it should be static. If the memory is
 * freed and the virtual address range has different physical memory mapped
 * again, it may cause crash on device due to the wrong translation entry. PMD
 * can't track the free event of the external memory for now.
 */
static void
mlx5_mr_update_ext_mp_cb(struct rte_mempool *mp, void *opaque,
			 struct rte_mempool_memhdr *memhdr,
			 unsigned mem_idx __rte_unused)
{
	struct mr_update_mp_data *data = opaque;
	struct rte_eth_dev *dev = data->dev;
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_ibv_shared *sh = priv->sh;
	struct mlx5_mr_ctrl *mr_ctrl = data->mr_ctrl;
	struct mlx5_mr *mr = NULL;
	uintptr_t addr = (uintptr_t)memhdr->addr;
	size_t len = memhdr->len;
	struct mlx5_mr_cache entry;
	uint32_t lkey;

	assert(rte_eal_process_type() == RTE_PROC_PRIMARY);
	/* If already registered, it should return. */
	rte_rwlock_read_lock(&sh->mr.rwlock);
	lkey = mr_lookup_dev(sh, &entry, addr);
	rte_rwlock_read_unlock(&sh->mr.rwlock);
	if (lkey != UINT32_MAX)
		return;
	DRV_LOG(DEBUG, "port %u register MR for chunk #%d of mempool (%s)",
		dev->data->port_id, mem_idx, mp->name);
	mr = mlx5_create_mr_ext(dev, addr, len, mp->socket_id);
	if (!mr) {
		DRV_LOG(WARNING,
			"port %u unable to allocate a new MR of"
			" mempool (%s).",
			dev->data->port_id, mp->name);
		data->ret = -1;
		return;
	}
	rte_rwlock_write_lock(&sh->mr.rwlock);
	LIST_INSERT_HEAD(&sh->mr.mr_list, mr, mr);
	/* Insert to the global cache table. */
	mr_insert_dev_cache(sh, mr);
	rte_rwlock_write_unlock(&sh->mr.rwlock);
	/* Insert to the local cache table */
	mlx5_mr_addr2mr_bh(dev, mr_ctrl, addr);
}

/**
 * Finds the first ethdev that match the pci device.
 * The existence of multiple ethdev per pci device is only with representors.
 * On such case, it is enough to get only one of the ports as they all share
 * the same ibv context.
 *
 * @param pdev
 *   Pointer to the PCI device.
 *
 * @return
 *   Pointer to the ethdev if found, NULL otherwise.
 */
static struct rte_eth_dev *
pci_dev_to_eth_dev(struct rte_pci_device *pdev)
{
	uint16_t port_id;

	RTE_ETH_FOREACH_DEV_OF(port_id, &pdev->device)
		return &rte_eth_devices[port_id];
	return NULL;
}

/**
 * DPDK callback to DMA map external memory to a PCI device.
 *
 * @param pdev
 *   Pointer to the PCI device.
 * @param addr
 *   Starting virtual address of memory to be mapped.
 * @param iova
 *   Starting IOVA address of memory to be mapped.
 * @param len
 *   Length of memory segment being mapped.
 *
 * @return
 *   0 on success, negative value on error.
 */
int
mlx5_dma_map(struct rte_pci_device *pdev, void *addr,
	     uint64_t iova __rte_unused, size_t len)
{
	struct rte_eth_dev *dev;
	struct mlx5_mr *mr;
	struct mlx5_priv *priv;
	struct mlx5_ibv_shared *sh;

	dev = pci_dev_to_eth_dev(pdev);
	if (!dev) {
		DRV_LOG(WARNING, "unable to find matching ethdev "
				 "to PCI device %p", (void *)pdev);
		rte_errno = ENODEV;
		return -1;
	}
	priv = dev->data->dev_private;
	mr = mlx5_create_mr_ext(dev, (uintptr_t)addr, len, SOCKET_ID_ANY);
	if (!mr) {
		DRV_LOG(WARNING,
			"port %u unable to dma map", dev->data->port_id);
		rte_errno = EINVAL;
		return -1;
	}
	sh = priv->sh;
	rte_rwlock_write_lock(&sh->mr.rwlock);
	LIST_INSERT_HEAD(&sh->mr.mr_list, mr, mr);
	/* Insert to the global cache table. */
	mr_insert_dev_cache(sh, mr);
	rte_rwlock_write_unlock(&sh->mr.rwlock);
	return 0;
}

/**
 * DPDK callback to DMA unmap external memory to a PCI device.
 *
 * @param pdev
 *   Pointer to the PCI device.
 * @param addr
 *   Starting virtual address of memory to be unmapped.
 * @param iova
 *   Starting IOVA address of memory to be unmapped.
 * @param len
 *   Length of memory segment being unmapped.
 *
 * @return
 *   0 on success, negative value on error.
 */
int
mlx5_dma_unmap(struct rte_pci_device *pdev, void *addr,
	       uint64_t iova __rte_unused, size_t len __rte_unused)
{
	struct rte_eth_dev *dev;
	struct mlx5_priv *priv;
	struct mlx5_ibv_shared *sh;
	struct mlx5_mr *mr;
	struct mlx5_mr_cache entry;

	dev = pci_dev_to_eth_dev(pdev);
	if (!dev) {
		DRV_LOG(WARNING, "unable to find matching ethdev "
				 "to PCI device %p", (void *)pdev);
		rte_errno = ENODEV;
		return -1;
	}
	priv = dev->data->dev_private;
	sh = priv->sh;
	rte_rwlock_read_lock(&sh->mr.rwlock);
	mr = mr_lookup_dev_list(sh, &entry, (uintptr_t)addr);
	if (!mr) {
		rte_rwlock_read_unlock(&sh->mr.rwlock);
		DRV_LOG(WARNING, "address 0x%" PRIxPTR " wasn't registered "
				 "to PCI device %p", (uintptr_t)addr,
				 (void *)pdev);
		rte_errno = EINVAL;
		return -1;
	}
	LIST_REMOVE(mr, mr);
	LIST_INSERT_HEAD(&sh->mr.mr_free_list, mr, mr);
	DEBUG("port %u remove MR(%p) from list", dev->data->port_id,
	      (void *)mr);
	mr_rebuild_dev_cache(sh);
	/*
	 * Flush local caches by propagating invalidation across cores.
	 * rte_smp_wmb() is enough to synchronize this event. If one of
	 * freed memsegs is seen by other core, that means the memseg
	 * has been allocated by allocator, which will come after this
	 * free call. Therefore, this store instruction (incrementing
	 * generation below) will be guaranteed to be seen by other core
	 * before the core sees the newly allocated memory.
	 */
	++sh->mr.dev_gen;
	DEBUG("broadcasting local cache flush, gen=%d",	sh->mr.dev_gen);
	rte_smp_wmb();
	rte_rwlock_read_unlock(&sh->mr.rwlock);
	return 0;
}

/**
 * Register MR for entire memory chunks in a Mempool having externally allocated
 * memory and fill in local cache.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param mr_ctrl
 *   Pointer to per-queue MR control structure.
 * @param mp
 *   Pointer to registering Mempool.
 *
 * @return
 *   0 on success, -1 on failure.
 */
static uint32_t
mlx5_mr_update_ext_mp(struct rte_eth_dev *dev, struct mlx5_mr_ctrl *mr_ctrl,
		      struct rte_mempool *mp)
{
	struct mr_update_mp_data data = {
		.dev = dev,
		.mr_ctrl = mr_ctrl,
		.ret = 0,
	};

	rte_mempool_mem_iter(mp, mlx5_mr_update_ext_mp_cb, &data);
	return data.ret;
}

/**
 * Register MR entire memory chunks in a Mempool having externally allocated
 * memory and search LKey of the address to return.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param addr
 *   Search key.
 * @param mp
 *   Pointer to registering Mempool where addr belongs.
 *
 * @return
 *   LKey for address on success, UINT32_MAX on failure.
 */
uint32_t
mlx5_tx_update_ext_mp(struct mlx5_txq_data *txq, uintptr_t addr,
		      struct rte_mempool *mp)
{
	struct mlx5_txq_ctrl *txq_ctrl =
		container_of(txq, struct mlx5_txq_ctrl, txq);
	struct mlx5_mr_ctrl *mr_ctrl = &txq->mr_ctrl;
	struct mlx5_priv *priv = txq_ctrl->priv;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		DRV_LOG(WARNING,
			"port %u using address (%p) from unregistered mempool"
			" having externally allocated memory"
			" in secondary process, please create mempool"
			" prior to rte_eth_dev_start()",
			PORT_ID(priv), (void *)addr);
		return UINT32_MAX;
	}
	mlx5_mr_update_ext_mp(ETH_DEV(priv), mr_ctrl, mp);
	return mlx5_tx_addr2mr_bh(txq, addr);
}

/* Called during rte_mempool_mem_iter() by mlx5_mr_update_mp(). */
static void
mlx5_mr_update_mp_cb(struct rte_mempool *mp __rte_unused, void *opaque,
		     struct rte_mempool_memhdr *memhdr,
		     unsigned mem_idx __rte_unused)
{
	struct mr_update_mp_data *data = opaque;
	uint32_t lkey;

	/* Stop iteration if failed in the previous walk. */
	if (data->ret < 0)
		return;
	/* Register address of the chunk and update local caches. */
	lkey = mlx5_mr_addr2mr_bh(data->dev, data->mr_ctrl,
				  (uintptr_t)memhdr->addr);
	if (lkey == UINT32_MAX)
		data->ret = -1;
}

/**
 * Register entire memory chunks in a Mempool.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param mr_ctrl
 *   Pointer to per-queue MR control structure.
 * @param mp
 *   Pointer to registering Mempool.
 *
 * @return
 *   0 on success, -1 on failure.
 */
int
mlx5_mr_update_mp(struct rte_eth_dev *dev, struct mlx5_mr_ctrl *mr_ctrl,
		  struct rte_mempool *mp)
{
	struct mr_update_mp_data data = {
		.dev = dev,
		.mr_ctrl = mr_ctrl,
		.ret = 0,
	};

	rte_mempool_mem_iter(mp, mlx5_mr_update_mp_cb, &data);
	if (data.ret < 0 && rte_errno == ENXIO) {
		/* Mempool may have externally allocated memory. */
		return mlx5_mr_update_ext_mp(dev, mr_ctrl, mp);
	}
	return data.ret;
}

/**
 * Dump all the created MRs and the global cache entries.
 *
 * @param sh
 *   Pointer to Ethernet device shared context.
 */
void
mlx5_mr_dump_dev(struct mlx5_ibv_shared *sh __rte_unused)
{
#ifndef NDEBUG
	struct mlx5_mr *mr;
	int mr_n = 0;
	int chunk_n = 0;

	rte_rwlock_read_lock(&sh->mr.rwlock);
	/* Iterate all the existing MRs. */
	LIST_FOREACH(mr, &sh->mr.mr_list, mr) {
		unsigned int n;

		DEBUG("device %s MR[%u], LKey = 0x%x, ms_n = %u, ms_bmp_n = %u",
		      sh->ibdev_name, mr_n++,
		      rte_cpu_to_be_32(mr->ibv_mr->lkey),
		      mr->ms_n, mr->ms_bmp_n);
		if (mr->ms_n == 0)
			continue;
		for (n = 0; n < mr->ms_bmp_n; ) {
			struct mlx5_mr_cache ret = { 0, };

			n = mr_find_next_chunk(mr, &ret, n);
			if (!ret.end)
				break;
			DEBUG("  chunk[%u], [0x%" PRIxPTR ", 0x%" PRIxPTR ")",
			      chunk_n++, ret.start, ret.end);
		}
	}
	DEBUG("device %s dumping global cache", sh->ibdev_name);
	mlx5_mr_btree_dump(&sh->mr.cache);
	rte_rwlock_read_unlock(&sh->mr.rwlock);
#endif
}

/**
 * Release all the created MRs and resources for shared device context.
 * list.
 *
 * @param sh
 *   Pointer to Ethernet device shared context.
 */
void
mlx5_mr_release(struct mlx5_ibv_shared *sh)
{
	struct mlx5_mr *mr_next;

	if (rte_log_get_level(mlx5_logtype) == RTE_LOG_DEBUG)
		mlx5_mr_dump_dev(sh);
	rte_rwlock_write_lock(&sh->mr.rwlock);
	/* Detach from MR list and move to free list. */
	mr_next = LIST_FIRST(&sh->mr.mr_list);
	while (mr_next != NULL) {
		struct mlx5_mr *mr = mr_next;

		mr_next = LIST_NEXT(mr, mr);
		LIST_REMOVE(mr, mr);
		LIST_INSERT_HEAD(&sh->mr.mr_free_list, mr, mr);
	}
	LIST_INIT(&sh->mr.mr_list);
	/* Free global cache. */
	mlx5_mr_btree_free(&sh->mr.cache);
	rte_rwlock_write_unlock(&sh->mr.rwlock);
	/* Free all remaining MRs. */
	mlx5_mr_garbage_collect(sh);
}
