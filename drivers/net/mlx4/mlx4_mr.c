/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 6WIND S.A.
 * Copyright 2017 Mellanox Technologies, Ltd
 */

/**
 * @file
 * Memory management functions for mlx4 driver.
 */

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Verbs headers do not support -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_branch_prediction.h>
#include <rte_common.h>
#include <rte_errno.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_mempool.h>
#include <rte_rwlock.h>

#include "mlx4_glue.h"
#include "mlx4_mr.h"
#include "mlx4_rxtx.h"
#include "mlx4_utils.h"

struct mr_find_contig_memsegs_data {
	uintptr_t addr;
	uintptr_t start;
	uintptr_t end;
	const struct rte_memseg_list *msl;
};

struct mr_update_mp_data {
	struct rte_eth_dev *dev;
	struct mlx4_mr_ctrl *mr_ctrl;
	int ret;
};

/**
 * Expand B-tree table to a given size. Can't be called with holding
 * memory_hotplug_lock or priv->mr.rwlock due to rte_realloc().
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
mr_btree_expand(struct mlx4_mr_btree *bt, int n)
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
	mem = rte_realloc(bt->table, n * sizeof(struct mlx4_mr_cache), 0);
	if (mem == NULL) {
		/* Not an error, B-tree search will be skipped. */
		WARN("failed to expand MR B-tree (%p) table", (void *)bt);
		ret = -1;
	} else {
		DEBUG("expanded MR B-tree table (size=%u)", n);
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
mr_btree_lookup(struct mlx4_mr_btree *bt, uint16_t *idx, uintptr_t addr)
{
	struct mlx4_mr_cache *lkp_tbl;
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
mr_btree_insert(struct mlx4_mr_btree *bt, struct mlx4_mr_cache *entry)
{
	struct mlx4_mr_cache *lkp_tbl;
	uint16_t idx = 0;
	size_t shift;

	assert(bt != NULL);
	assert(bt->len <= bt->size);
	assert(bt->len > 0);
	lkp_tbl = *bt->table;
	/* Find out the slot for insertion. */
	if (mr_btree_lookup(bt, &idx, entry->start) != UINT32_MAX) {
		DEBUG("abort insertion to B-tree(%p): already exist at"
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
	shift = (bt->len - idx) * sizeof(struct mlx4_mr_cache);
	if (shift)
		memmove(&lkp_tbl[idx + 1], &lkp_tbl[idx], shift);
	lkp_tbl[idx] = *entry;
	bt->len++;
	DEBUG("inserted B-tree(%p)[%u],"
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
mlx4_mr_btree_init(struct mlx4_mr_btree *bt, int n, int socket)
{
	if (bt == NULL) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	memset(bt, 0, sizeof(*bt));
	bt->table = rte_calloc_socket("B-tree table",
				      n, sizeof(struct mlx4_mr_cache),
				      0, socket);
	if (bt->table == NULL) {
		rte_errno = ENOMEM;
		ERROR("failed to allocate memory for btree cache on socket %d",
		      socket);
		return -rte_errno;
	}
	bt->size = n;
	/* First entry must be NULL for binary search. */
	(*bt->table)[bt->len++] = (struct mlx4_mr_cache) {
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
mlx4_mr_btree_free(struct mlx4_mr_btree *bt)
{
	if (bt == NULL)
		return;
	DEBUG("freeing B-tree %p with table %p", (void *)bt, (void *)bt->table);
	rte_free(bt->table);
	memset(bt, 0, sizeof(*bt));
}

#ifndef NDEBUG
/**
 * Dump all the entries in a B-tree
 *
 * @param bt
 *   Pointer to B-tree structure.
 */
void
mlx4_mr_btree_dump(struct mlx4_mr_btree *bt)
{
	int idx;
	struct mlx4_mr_cache *lkp_tbl;

	if (bt == NULL)
		return;
	lkp_tbl = *bt->table;
	for (idx = 0; idx < bt->len; ++idx) {
		struct mlx4_mr_cache *entry = &lkp_tbl[idx];

		DEBUG("B-tree(%p)[%u],"
		      " [0x%" PRIxPTR ", 0x%" PRIxPTR ") lkey=0x%x",
		      (void *)bt, idx, entry->start, entry->end, entry->lkey);
	}
}
#endif

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
mr_find_next_chunk(struct mlx4_mr *mr, struct mlx4_mr_cache *entry,
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
 * mlx4_mr_create() on miss.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param mr
 *   Pointer to MR to insert.
 *
 * @return
 *   0 on success, -1 on failure.
 */
static int
mr_insert_dev_cache(struct rte_eth_dev *dev, struct mlx4_mr *mr)
{
	struct mlx4_priv *priv = dev->data->dev_private;
	unsigned int n;

	DEBUG("port %u inserting MR(%p) to global cache",
	      dev->data->port_id, (void *)mr);
	for (n = 0; n < mr->ms_bmp_n; ) {
		struct mlx4_mr_cache entry;

		memset(&entry, 0, sizeof(entry));
		/* Find a contiguous chunk and advance the index. */
		n = mr_find_next_chunk(mr, &entry, n);
		if (!entry.end)
			break;
		if (mr_btree_insert(&priv->mr.cache, &entry) < 0) {
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
 * @param dev
 *   Pointer to Ethernet device.
 * @param[out] entry
 *   Pointer to returning MR cache entry. If no match, this will not be updated.
 * @param addr
 *   Search key.
 *
 * @return
 *   Found MR on match, NULL otherwise.
 */
static struct mlx4_mr *
mr_lookup_dev_list(struct rte_eth_dev *dev, struct mlx4_mr_cache *entry,
		   uintptr_t addr)
{
	struct mlx4_priv *priv = dev->data->dev_private;
	struct mlx4_mr *mr;

	/* Iterate all the existing MRs. */
	LIST_FOREACH(mr, &priv->mr.mr_list, mr) {
		unsigned int n;

		if (mr->ms_n == 0)
			continue;
		for (n = 0; n < mr->ms_bmp_n; ) {
			struct mlx4_mr_cache ret;

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
 *   Pointer to Ethernet device.
 * @param[out] entry
 *   Pointer to returning MR cache entry. If no match, this will not be updated.
 * @param addr
 *   Search key.
 *
 * @return
 *   Searched LKey on success, UINT32_MAX on failure and rte_errno is set.
 */
static uint32_t
mr_lookup_dev(struct rte_eth_dev *dev, struct mlx4_mr_cache *entry,
	      uintptr_t addr)
{
	struct mlx4_priv *priv = dev->data->dev_private;
	uint16_t idx;
	uint32_t lkey = UINT32_MAX;
	struct mlx4_mr *mr;

	/*
	 * If the global cache has overflowed since it failed to expand the
	 * B-tree table, it can't have all the existing MRs. Then, the address
	 * has to be searched by traversing the original MR list instead, which
	 * is very slow path. Otherwise, the global cache is all inclusive.
	 */
	if (!unlikely(priv->mr.cache.overflow)) {
		lkey = mr_btree_lookup(&priv->mr.cache, &idx, addr);
		if (lkey != UINT32_MAX)
			*entry = (*priv->mr.cache.table)[idx];
	} else {
		/* Falling back to the slowest path. */
		mr = mr_lookup_dev_list(dev, entry, addr);
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
mr_free(struct mlx4_mr *mr)
{
	if (mr == NULL)
		return;
	DEBUG("freeing MR(%p):", (void *)mr);
	if (mr->ibv_mr != NULL)
		claim_zero(mlx4_glue->dereg_mr(mr->ibv_mr));
	if (mr->ms_bmp != NULL)
		rte_bitmap_free(mr->ms_bmp);
	rte_free(mr);
}

/**
 * Release resources of detached MR having no online entry.
 *
 * @param dev
 *   Pointer to Ethernet device.
 */
static void
mlx4_mr_garbage_collect(struct rte_eth_dev *dev)
{
	struct mlx4_priv *priv = dev->data->dev_private;
	struct mlx4_mr *mr_next;
	struct mlx4_mr_list free_list = LIST_HEAD_INITIALIZER(free_list);

	/* Must be called from the primary process. */
	assert(rte_eal_process_type() == RTE_PROC_PRIMARY);
	/*
	 * MR can't be freed with holding the lock because rte_free() could call
	 * memory free callback function. This will be a deadlock situation.
	 */
	rte_rwlock_write_lock(&priv->mr.rwlock);
	/* Detach the whole free list and release it after unlocking. */
	free_list = priv->mr.mr_free_list;
	LIST_INIT(&priv->mr.mr_free_list);
	rte_rwlock_write_unlock(&priv->mr.rwlock);
	/* Release resources. */
	mr_next = LIST_FIRST(&free_list);
	while (mr_next != NULL) {
		struct mlx4_mr *mr = mr_next;

		mr_next = LIST_NEXT(mr, mr);
		mr_free(mr);
	}
}

/* Called during rte_memseg_contig_walk() by mlx4_mr_create(). */
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
mlx4_mr_create_secondary(struct rte_eth_dev *dev, struct mlx4_mr_cache *entry,
			 uintptr_t addr)
{
	struct mlx4_priv *priv = dev->data->dev_private;
	int ret;

	DEBUG("port %u requesting MR creation for address (%p)",
	      dev->data->port_id, (void *)addr);
	ret = mlx4_mp_req_mr_create(dev, addr);
	if (ret) {
		DEBUG("port %u fail to request MR creation for address (%p)",
		      dev->data->port_id, (void *)addr);
		return UINT32_MAX;
	}
	rte_rwlock_read_lock(&priv->mr.rwlock);
	/* Fill in output data. */
	mr_lookup_dev(dev, entry, addr);
	/* Lookup can't fail. */
	assert(entry->lkey != UINT32_MAX);
	rte_rwlock_read_unlock(&priv->mr.rwlock);
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
mlx4_mr_create_primary(struct rte_eth_dev *dev, struct mlx4_mr_cache *entry,
		       uintptr_t addr)
{
	struct mlx4_priv *priv = dev->data->dev_private;
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	const struct rte_memseg_list *msl;
	const struct rte_memseg *ms;
	struct mlx4_mr *mr = NULL;
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

	DEBUG("port %u creating a MR using address (%p)",
	      dev->data->port_id, (void *)addr);
	/*
	 * Release detached MRs if any. This can't be called with holding either
	 * memory_hotplug_lock or priv->mr.rwlock. MRs on the free list have
	 * been detached by the memory free event but it couldn't be released
	 * inside the callback due to deadlock. As a result, releasing resources
	 * is quite opportunistic.
	 */
	mlx4_mr_garbage_collect(dev);
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
	if (!priv->mr_ext_memseg_en) {
		data.msl = rte_mem_virt2memseg_list((void *)addr);
		data.start = RTE_ALIGN_FLOOR(addr, data.msl->page_sz);
		data.end = data.start + data.msl->page_sz;
	} else if (!rte_memseg_contig_walk(mr_find_contig_memsegs_cb, &data)) {
		WARN("port %u unable to find virtually contiguous"
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
		WARN("port %u unable to allocate memory for a new MR of"
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
		WARN("port %u unable to initialize bitmap for a new MR of"
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
		WARN("port %u unable to find virtually contiguous"
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
	rte_rwlock_write_lock(&priv->mr.rwlock);
	/*
	 * Check the address is really missing. If other thread already created
	 * one or it is not found due to overflow, abort and return.
	 */
	if (mr_lookup_dev(dev, entry, addr) != UINT32_MAX) {
		/*
		 * Insert to the global cache table. It may fail due to
		 * low-on-memory. Then, this entry will have to be searched
		 * here again.
		 */
		mr_btree_insert(&priv->mr.cache, entry);
		DEBUG("port %u found MR for %p on final lookup, abort",
		      dev->data->port_id, (void *)addr);
		rte_rwlock_write_unlock(&priv->mr.rwlock);
		rte_rwlock_read_unlock(&mcfg->memory_hotplug_lock);
		/*
		 * Must be unlocked before calling rte_free() because
		 * mlx4_mr_mem_event_free_cb() can be called inside.
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
		struct mlx4_mr_cache ret;

		memset(&ret, 0, sizeof(ret));
		start = data_re.start + n * msl->page_sz;
		/* Exclude memsegs already registered by other MRs. */
		if (mr_lookup_dev(dev, &ret, start) == UINT32_MAX) {
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
	 * mlx4_alloc_buf_extern() which eventually calls rte_malloc_socket()
	 * through mlx4_alloc_verbs_buf().
	 */
	mr->ibv_mr = mlx4_glue->reg_mr(priv->pd, (void *)data.start, len,
				       IBV_ACCESS_LOCAL_WRITE);
	if (mr->ibv_mr == NULL) {
		WARN("port %u fail to create a verbs MR for address (%p)",
		     dev->data->port_id, (void *)addr);
		rte_errno = EINVAL;
		goto err_mrlock;
	}
	assert((uintptr_t)mr->ibv_mr->addr == data.start);
	assert(mr->ibv_mr->length == len);
	LIST_INSERT_HEAD(&priv->mr.mr_list, mr, mr);
	DEBUG("port %u MR CREATED (%p) for %p:\n"
	      "  [0x%" PRIxPTR ", 0x%" PRIxPTR "),"
	      " lkey=0x%x base_idx=%u ms_n=%u, ms_bmp_n=%u",
	      dev->data->port_id, (void *)mr, (void *)addr,
	      data.start, data.end, rte_cpu_to_be_32(mr->ibv_mr->lkey),
	      mr->ms_base_idx, mr->ms_n, mr->ms_bmp_n);
	/* Insert to the global cache table. */
	mr_insert_dev_cache(dev, mr);
	/* Fill in output data. */
	mr_lookup_dev(dev, entry, addr);
	/* Lookup can't fail. */
	assert(entry->lkey != UINT32_MAX);
	rte_rwlock_write_unlock(&priv->mr.rwlock);
	rte_rwlock_read_unlock(&mcfg->memory_hotplug_lock);
	return entry->lkey;
err_mrlock:
	rte_rwlock_write_unlock(&priv->mr.rwlock);
err_memlock:
	rte_rwlock_read_unlock(&mcfg->memory_hotplug_lock);
err_nolock:
	/*
	 * In case of error, as this can be called in a datapath, a warning
	 * message per an error is preferable instead. Must be unlocked before
	 * calling rte_free() because mlx4_mr_mem_event_free_cb() can be called
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
mlx4_mr_create(struct rte_eth_dev *dev, struct mlx4_mr_cache *entry,
	       uintptr_t addr)
{
	uint32_t ret = 0;

	switch (rte_eal_process_type()) {
	case RTE_PROC_PRIMARY:
		ret = mlx4_mr_create_primary(dev, entry, addr);
		break;
	case RTE_PROC_SECONDARY:
		ret = mlx4_mr_create_secondary(dev, entry, addr);
		break;
	default:
		break;
	}
	return ret;
}

/**
 * Rebuild the global B-tree cache of device from the original MR list.
 *
 * @param dev
 *   Pointer to Ethernet device.
 */
static void
mr_rebuild_dev_cache(struct rte_eth_dev *dev)
{
	struct mlx4_priv *priv = dev->data->dev_private;
	struct mlx4_mr *mr;

	DEBUG("port %u rebuild dev cache[]", dev->data->port_id);
	/* Flush cache to rebuild. */
	priv->mr.cache.len = 1;
	priv->mr.cache.overflow = 0;
	/* Iterate all the existing MRs. */
	LIST_FOREACH(mr, &priv->mr.mr_list, mr)
		if (mr_insert_dev_cache(dev, mr) < 0)
			return;
}

/**
 * Callback for memory free event. Iterate freed memsegs and check whether it
 * belongs to an existing MR. If found, clear the bit from bitmap of MR. As a
 * result, the MR would be fragmented. If it becomes empty, the MR will be freed
 * later by mlx4_mr_garbage_collect().
 *
 * The global cache must be rebuilt if there's any change and this event has to
 * be propagated to dataplane threads to flush the local caches.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param addr
 *   Address of freed memory.
 * @param len
 *   Size of freed memory.
 */
static void
mlx4_mr_mem_event_free_cb(struct rte_eth_dev *dev, const void *addr, size_t len)
{
	struct mlx4_priv *priv = dev->data->dev_private;
	const struct rte_memseg_list *msl;
	struct mlx4_mr *mr;
	int ms_n;
	int i;
	int rebuild = 0;

	DEBUG("port %u free callback: addr=%p, len=%zu",
	      dev->data->port_id, addr, len);
	msl = rte_mem_virt2memseg_list(addr);
	/* addr and len must be page-aligned. */
	assert((uintptr_t)addr == RTE_ALIGN((uintptr_t)addr, msl->page_sz));
	assert(len == RTE_ALIGN(len, msl->page_sz));
	ms_n = len / msl->page_sz;
	rte_rwlock_write_lock(&priv->mr.rwlock);
	/* Clear bits of freed memsegs from MR. */
	for (i = 0; i < ms_n; ++i) {
		const struct rte_memseg *ms;
		struct mlx4_mr_cache entry;
		uintptr_t start;
		int ms_idx;
		uint32_t pos;

		/* Find MR having this memseg. */
		start = (uintptr_t)addr + i * msl->page_sz;
		mr = mr_lookup_dev_list(dev, &entry, start);
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
		DEBUG("port %u MR(%p): clear bitmap[%u] for addr %p",
		      dev->data->port_id, (void *)mr, pos, (void *)start);
		rte_bitmap_clear(mr->ms_bmp, pos);
		if (--mr->ms_n == 0) {
			LIST_REMOVE(mr, mr);
			LIST_INSERT_HEAD(&priv->mr.mr_free_list, mr, mr);
			DEBUG("port %u remove MR(%p) from list",
			      dev->data->port_id, (void *)mr);
		}
		/*
		 * MR is fragmented or will be freed. the global cache must be
		 * rebuilt.
		 */
		rebuild = 1;
	}
	if (rebuild) {
		mr_rebuild_dev_cache(dev);
		/*
		 * Flush local caches by propagating invalidation across cores.
		 * rte_smp_wmb() is enough to synchronize this event. If one of
		 * freed memsegs is seen by other core, that means the memseg
		 * has been allocated by allocator, which will come after this
		 * free call. Therefore, this store instruction (incrementing
		 * generation below) will be guaranteed to be seen by other core
		 * before the core sees the newly allocated memory.
		 */
		++priv->mr.dev_gen;
		DEBUG("broadcasting local cache flush, gen=%d",
		      priv->mr.dev_gen);
		rte_smp_wmb();
	}
	rte_rwlock_write_unlock(&priv->mr.rwlock);
#ifndef NDEBUG
	if (rebuild)
		mlx4_mr_dump_dev(dev);
#endif
}

/**
 * Callback for memory event.
 *
 * @param event_type
 *   Memory event type.
 * @param addr
 *   Address of memory.
 * @param len
 *   Size of memory.
 */
void
mlx4_mr_mem_event_cb(enum rte_mem_event event_type, const void *addr,
		     size_t len, void *arg __rte_unused)
{
	struct mlx4_priv *priv;
	struct mlx4_dev_list *dev_list = &mlx4_shared_data->mem_event_cb_list;

	/* Must be called from the primary process. */
	assert(rte_eal_process_type() == RTE_PROC_PRIMARY);
	switch (event_type) {
	case RTE_MEM_EVENT_FREE:
		rte_rwlock_read_lock(&mlx4_shared_data->mem_event_rwlock);
		/* Iterate all the existing mlx4 devices. */
		LIST_FOREACH(priv, dev_list, mem_event_cb)
			mlx4_mr_mem_event_free_cb(ETH_DEV(priv), addr, len);
		rte_rwlock_read_unlock(&mlx4_shared_data->mem_event_rwlock);
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
mlx4_mr_lookup_dev(struct rte_eth_dev *dev, struct mlx4_mr_ctrl *mr_ctrl,
		   struct mlx4_mr_cache *entry, uintptr_t addr)
{
	struct mlx4_priv *priv = dev->data->dev_private;
	struct mlx4_mr_btree *bt = &mr_ctrl->cache_bh;
	uint16_t idx;
	uint32_t lkey;

	/* If local cache table is full, try to double it. */
	if (unlikely(bt->len == bt->size))
		mr_btree_expand(bt, bt->size << 1);
	/* Look up in the global cache. */
	rte_rwlock_read_lock(&priv->mr.rwlock);
	lkey = mr_btree_lookup(&priv->mr.cache, &idx, addr);
	if (lkey != UINT32_MAX) {
		/* Found. */
		*entry = (*priv->mr.cache.table)[idx];
		rte_rwlock_read_unlock(&priv->mr.rwlock);
		/*
		 * Update local cache. Even if it fails, return the found entry
		 * to update top-half cache. Next time, this entry will be found
		 * in the global cache.
		 */
		mr_btree_insert(bt, entry);
		return lkey;
	}
	rte_rwlock_read_unlock(&priv->mr.rwlock);
	/* First time to see the address? Create a new MR. */
	lkey = mlx4_mr_create(dev, entry, addr);
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
mlx4_mr_addr2mr_bh(struct rte_eth_dev *dev, struct mlx4_mr_ctrl *mr_ctrl,
		   uintptr_t addr)
{
	uint32_t lkey;
	uint16_t bh_idx = 0;
	/* Victim in top-half cache to replace with new entry. */
	struct mlx4_mr_cache *repl = &mr_ctrl->cache[mr_ctrl->head];

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
		lkey = mlx4_mr_lookup_dev(dev, mr_ctrl, repl, addr);
		if (unlikely(lkey == UINT32_MAX))
			return UINT32_MAX;
	}
	/* Update the most recently used entry. */
	mr_ctrl->mru = mr_ctrl->head;
	/* Point to the next victim, the oldest. */
	mr_ctrl->head = (mr_ctrl->head + 1) % MLX4_MR_CACHE_N;
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
mlx4_rx_addr2mr_bh(struct rxq *rxq, uintptr_t addr)
{
	struct mlx4_mr_ctrl *mr_ctrl = &rxq->mr_ctrl;
	struct mlx4_priv *priv = rxq->priv;

	return mlx4_mr_addr2mr_bh(ETH_DEV(priv), mr_ctrl, addr);
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
mlx4_tx_addr2mr_bh(struct txq *txq, uintptr_t addr)
{
	struct mlx4_mr_ctrl *mr_ctrl = &txq->mr_ctrl;
	struct mlx4_priv *priv = txq->priv;

	return mlx4_mr_addr2mr_bh(ETH_DEV(priv), mr_ctrl, addr);
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
mlx4_tx_mb2mr_bh(struct txq *txq, struct rte_mbuf *mb)
{
	uintptr_t addr = (uintptr_t)mb->buf_addr;
	uint32_t lkey;

	lkey = mlx4_tx_addr2mr_bh(txq, addr);
	if (lkey == UINT32_MAX && rte_errno == ENXIO) {
		/* Mempool may have externally allocated memory. */
		return mlx4_tx_update_ext_mp(txq, addr, mlx4_mb2mp(mb));
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
mlx4_mr_flush_local_cache(struct mlx4_mr_ctrl *mr_ctrl)
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
	DEBUG("mr_ctrl(%p): flushed, cur_gen=%d",
	      (void *)mr_ctrl, mr_ctrl->cur_gen);
}

/**
 * Called during rte_mempool_mem_iter() by mlx4_mr_update_ext_mp().
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
mlx4_mr_update_ext_mp_cb(struct rte_mempool *mp, void *opaque,
			 struct rte_mempool_memhdr *memhdr,
			 unsigned mem_idx __rte_unused)
{
	struct mr_update_mp_data *data = opaque;
	struct rte_eth_dev *dev = data->dev;
	struct mlx4_priv *priv = dev->data->dev_private;
	struct mlx4_mr_ctrl *mr_ctrl = data->mr_ctrl;
	struct mlx4_mr *mr = NULL;
	uintptr_t addr = (uintptr_t)memhdr->addr;
	size_t len = memhdr->len;
	struct mlx4_mr_cache entry;
	uint32_t lkey;

	assert(rte_eal_process_type() == RTE_PROC_PRIMARY);
	/* If already registered, it should return. */
	rte_rwlock_read_lock(&priv->mr.rwlock);
	lkey = mr_lookup_dev(dev, &entry, addr);
	rte_rwlock_read_unlock(&priv->mr.rwlock);
	if (lkey != UINT32_MAX)
		return;
	mr = rte_zmalloc_socket(NULL,
				RTE_ALIGN_CEIL(sizeof(*mr),
					       RTE_CACHE_LINE_SIZE),
				RTE_CACHE_LINE_SIZE, mp->socket_id);
	if (mr == NULL) {
		WARN("port %u unable to allocate memory for a new MR of"
		     " mempool (%s).",
		     dev->data->port_id, mp->name);
		data->ret = -1;
		return;
	}
	DEBUG("port %u register MR for chunk #%d of mempool (%s)",
	      dev->data->port_id, mem_idx, mp->name);
	mr->ibv_mr = mlx4_glue->reg_mr(priv->pd, (void *)addr, len,
				       IBV_ACCESS_LOCAL_WRITE);
	if (mr->ibv_mr == NULL) {
		WARN("port %u fail to create a verbs MR for address (%p)",
		     dev->data->port_id, (void *)addr);
		rte_free(mr);
		data->ret = -1;
		return;
	}
	mr->msl = NULL; /* Mark it is external memory. */
	mr->ms_bmp = NULL;
	mr->ms_n = 1;
	mr->ms_bmp_n = 1;
	rte_rwlock_write_lock(&priv->mr.rwlock);
	LIST_INSERT_HEAD(&priv->mr.mr_list, mr, mr);
	DEBUG("port %u MR CREATED (%p) for external memory %p:\n"
	      "  [0x%" PRIxPTR ", 0x%" PRIxPTR "),"
	      " lkey=0x%x base_idx=%u ms_n=%u, ms_bmp_n=%u",
	      dev->data->port_id, (void *)mr, (void *)addr,
	      addr, addr + len, rte_cpu_to_be_32(mr->ibv_mr->lkey),
	      mr->ms_base_idx, mr->ms_n, mr->ms_bmp_n);
	/* Insert to the global cache table. */
	mr_insert_dev_cache(dev, mr);
	rte_rwlock_write_unlock(&priv->mr.rwlock);
	/* Insert to the local cache table */
	mlx4_mr_addr2mr_bh(dev, mr_ctrl, addr);
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
mlx4_mr_update_ext_mp(struct rte_eth_dev *dev, struct mlx4_mr_ctrl *mr_ctrl,
		      struct rte_mempool *mp)
{
	struct mr_update_mp_data data = {
		.dev = dev,
		.mr_ctrl = mr_ctrl,
		.ret = 0,
	};

	rte_mempool_mem_iter(mp, mlx4_mr_update_ext_mp_cb, &data);
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
mlx4_tx_update_ext_mp(struct txq *txq, uintptr_t addr, struct rte_mempool *mp)
{
	struct mlx4_mr_ctrl *mr_ctrl = &txq->mr_ctrl;
	struct mlx4_priv *priv = txq->priv;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		WARN("port %u using address (%p) from unregistered mempool"
		     " having externally allocated memory"
		     " in secondary process, please create mempool"
		     " prior to rte_eth_dev_start()",
		     PORT_ID(priv), (void *)addr);
		return UINT32_MAX;
	}
	mlx4_mr_update_ext_mp(ETH_DEV(priv), mr_ctrl, mp);
	return mlx4_tx_addr2mr_bh(txq, addr);
}

/* Called during rte_mempool_mem_iter() by mlx4_mr_update_mp(). */
static void
mlx4_mr_update_mp_cb(struct rte_mempool *mp __rte_unused, void *opaque,
		     struct rte_mempool_memhdr *memhdr,
		     unsigned mem_idx __rte_unused)
{
	struct mr_update_mp_data *data = opaque;
	uint32_t lkey;

	/* Stop iteration if failed in the previous walk. */
	if (data->ret < 0)
		return;
	/* Register address of the chunk and update local caches. */
	lkey = mlx4_mr_addr2mr_bh(data->dev, data->mr_ctrl,
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
mlx4_mr_update_mp(struct rte_eth_dev *dev, struct mlx4_mr_ctrl *mr_ctrl,
		  struct rte_mempool *mp)
{
	struct mr_update_mp_data data = {
		.dev = dev,
		.mr_ctrl = mr_ctrl,
		.ret = 0,
	};

	rte_mempool_mem_iter(mp, mlx4_mr_update_mp_cb, &data);
	if (data.ret < 0 && rte_errno == ENXIO) {
		/* Mempool may have externally allocated memory. */
		return mlx4_mr_update_ext_mp(dev, mr_ctrl, mp);
	}
	return data.ret;
}

#ifndef NDEBUG
/**
 * Dump all the created MRs and the global cache entries.
 *
 * @param dev
 *   Pointer to Ethernet device.
 */
void
mlx4_mr_dump_dev(struct rte_eth_dev *dev)
{
	struct mlx4_priv *priv = dev->data->dev_private;
	struct mlx4_mr *mr;
	int mr_n = 0;
	int chunk_n = 0;

	rte_rwlock_read_lock(&priv->mr.rwlock);
	/* Iterate all the existing MRs. */
	LIST_FOREACH(mr, &priv->mr.mr_list, mr) {
		unsigned int n;

		DEBUG("port %u MR[%u], LKey = 0x%x, ms_n = %u, ms_bmp_n = %u",
		      dev->data->port_id, mr_n++,
		      rte_cpu_to_be_32(mr->ibv_mr->lkey),
		      mr->ms_n, mr->ms_bmp_n);
		if (mr->ms_n == 0)
			continue;
		for (n = 0; n < mr->ms_bmp_n; ) {
			struct mlx4_mr_cache ret;

			memset(&ret, 0, sizeof(ret));
			n = mr_find_next_chunk(mr, &ret, n);
			if (!ret.end)
				break;
			DEBUG("  chunk[%u], [0x%" PRIxPTR ", 0x%" PRIxPTR ")",
			      chunk_n++, ret.start, ret.end);
		}
	}
	DEBUG("port %u dumping global cache", dev->data->port_id);
	mlx4_mr_btree_dump(&priv->mr.cache);
	rte_rwlock_read_unlock(&priv->mr.rwlock);
}
#endif

/**
 * Release all the created MRs and resources. Remove device from memory callback
 * list.
 *
 * @param dev
 *   Pointer to Ethernet device.
 */
void
mlx4_mr_release(struct rte_eth_dev *dev)
{
	struct mlx4_priv *priv = dev->data->dev_private;
	struct mlx4_mr *mr_next;

	/* Remove from memory callback device list. */
	rte_rwlock_write_lock(&mlx4_shared_data->mem_event_rwlock);
	LIST_REMOVE(priv, mem_event_cb);
	rte_rwlock_write_unlock(&mlx4_shared_data->mem_event_rwlock);
#ifndef NDEBUG
	mlx4_mr_dump_dev(dev);
#endif
	rte_rwlock_write_lock(&priv->mr.rwlock);
	/* Detach from MR list and move to free list. */
	mr_next = LIST_FIRST(&priv->mr.mr_list);
	while (mr_next != NULL) {
		struct mlx4_mr *mr = mr_next;

		mr_next = LIST_NEXT(mr, mr);
		LIST_REMOVE(mr, mr);
		LIST_INSERT_HEAD(&priv->mr.mr_free_list, mr, mr);
	}
	LIST_INIT(&priv->mr.mr_list);
	/* Free global cache. */
	mlx4_mr_btree_free(&priv->mr.cache);
	rte_rwlock_write_unlock(&priv->mr.rwlock);
	/* Free all remaining MRs. */
	mlx4_mr_garbage_collect(dev);
}
