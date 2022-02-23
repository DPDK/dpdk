/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019 Mellanox Technologies, Ltd
 */

#include <rte_malloc.h>

#include <mlx5_malloc.h>

#include "mlx5_utils.h"

/********************* Indexed pool **********************/

static inline void
mlx5_ipool_lock(struct mlx5_indexed_pool *pool)
{
	if (pool->cfg.need_lock)
		rte_spinlock_lock(&pool->rsz_lock);
}

static inline void
mlx5_ipool_unlock(struct mlx5_indexed_pool *pool)
{
	if (pool->cfg.need_lock)
		rte_spinlock_unlock(&pool->rsz_lock);
}

static inline uint32_t
mlx5_trunk_idx_get(struct mlx5_indexed_pool *pool, uint32_t entry_idx)
{
	struct mlx5_indexed_pool_config *cfg = &pool->cfg;
	uint32_t trunk_idx = 0;
	uint32_t i;

	if (!cfg->grow_trunk)
		return entry_idx / cfg->trunk_size;
	if (entry_idx >= pool->grow_tbl[cfg->grow_trunk - 1]) {
		trunk_idx = (entry_idx - pool->grow_tbl[cfg->grow_trunk - 1]) /
			    (cfg->trunk_size << (cfg->grow_shift *
			    cfg->grow_trunk)) + cfg->grow_trunk;
	} else {
		for (i = 0; i < cfg->grow_trunk; i++) {
			if (entry_idx < pool->grow_tbl[i])
				break;
		}
		trunk_idx = i;
	}
	return trunk_idx;
}

static inline uint32_t
mlx5_trunk_size_get(struct mlx5_indexed_pool *pool, uint32_t trunk_idx)
{
	struct mlx5_indexed_pool_config *cfg = &pool->cfg;

	return cfg->trunk_size << (cfg->grow_shift *
	       (trunk_idx > cfg->grow_trunk ? cfg->grow_trunk : trunk_idx));
}

static inline uint32_t
mlx5_trunk_idx_offset_get(struct mlx5_indexed_pool *pool, uint32_t trunk_idx)
{
	struct mlx5_indexed_pool_config *cfg = &pool->cfg;
	uint32_t offset = 0;

	if (!trunk_idx)
		return 0;
	if (!cfg->grow_trunk)
		return cfg->trunk_size * trunk_idx;
	if (trunk_idx < cfg->grow_trunk)
		offset = pool->grow_tbl[trunk_idx - 1];
	else
		offset = pool->grow_tbl[cfg->grow_trunk - 1] +
			 (cfg->trunk_size << (cfg->grow_shift *
			 cfg->grow_trunk)) * (trunk_idx - cfg->grow_trunk);
	return offset;
}

struct mlx5_indexed_pool *
mlx5_ipool_create(struct mlx5_indexed_pool_config *cfg)
{
	struct mlx5_indexed_pool *pool;
	uint32_t i;

	if (!cfg || (!cfg->malloc ^ !cfg->free) ||
	    (cfg->per_core_cache && cfg->release_mem_en) ||
	    (cfg->trunk_size && ((cfg->trunk_size & (cfg->trunk_size - 1)) ||
	    ((__builtin_ffs(cfg->trunk_size) + TRUNK_IDX_BITS) > 32))))
		return NULL;
	pool = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*pool) + cfg->grow_trunk *
			   sizeof(pool->grow_tbl[0]), RTE_CACHE_LINE_SIZE,
			   SOCKET_ID_ANY);
	if (!pool)
		return NULL;
	pool->cfg = *cfg;
	if (!pool->cfg.trunk_size)
		pool->cfg.trunk_size = MLX5_IPOOL_DEFAULT_TRUNK_SIZE;
	if (!cfg->malloc && !cfg->free) {
		pool->cfg.malloc = mlx5_malloc;
		pool->cfg.free = mlx5_free;
	}
	if (pool->cfg.need_lock)
		rte_spinlock_init(&pool->rsz_lock);
	/*
	 * Initialize the dynamic grow trunk size lookup table to have a quick
	 * lookup for the trunk entry index offset.
	 */
	for (i = 0; i < cfg->grow_trunk; i++) {
		pool->grow_tbl[i] = cfg->trunk_size << (cfg->grow_shift * i);
		if (i > 0)
			pool->grow_tbl[i] += pool->grow_tbl[i - 1];
	}
	if (!pool->cfg.max_idx)
		pool->cfg.max_idx =
			mlx5_trunk_idx_offset_get(pool, TRUNK_MAX_IDX + 1);
	if (!cfg->per_core_cache)
		pool->free_list = TRUNK_INVALID;
	rte_spinlock_init(&pool->lcore_lock);
	return pool;
}

static int
mlx5_ipool_grow(struct mlx5_indexed_pool *pool)
{
	struct mlx5_indexed_trunk *trunk;
	struct mlx5_indexed_trunk **trunk_tmp;
	struct mlx5_indexed_trunk **p;
	size_t trunk_size = 0;
	size_t data_size;
	size_t bmp_size;
	uint32_t idx, cur_max_idx, i;

	cur_max_idx = mlx5_trunk_idx_offset_get(pool, pool->n_trunk_valid);
	if (pool->n_trunk_valid == TRUNK_MAX_IDX ||
	    cur_max_idx >= pool->cfg.max_idx)
		return -ENOMEM;
	if (pool->n_trunk_valid == pool->n_trunk) {
		/* No free trunk flags, expand trunk list. */
		int n_grow = pool->n_trunk_valid ? pool->n_trunk :
			     RTE_CACHE_LINE_SIZE / sizeof(void *);

		p = pool->cfg.malloc(0, (pool->n_trunk_valid + n_grow) *
				     sizeof(struct mlx5_indexed_trunk *),
				     RTE_CACHE_LINE_SIZE, rte_socket_id());
		if (!p)
			return -ENOMEM;
		if (pool->trunks)
			memcpy(p, pool->trunks, pool->n_trunk_valid *
			       sizeof(struct mlx5_indexed_trunk *));
		memset(RTE_PTR_ADD(p, pool->n_trunk_valid * sizeof(void *)), 0,
		       n_grow * sizeof(void *));
		trunk_tmp = pool->trunks;
		pool->trunks = p;
		if (trunk_tmp)
			pool->cfg.free(trunk_tmp);
		pool->n_trunk += n_grow;
	}
	if (!pool->cfg.release_mem_en) {
		idx = pool->n_trunk_valid;
	} else {
		/* Find the first available slot in trunk list */
		for (idx = 0; idx < pool->n_trunk; idx++)
			if (pool->trunks[idx] == NULL)
				break;
	}
	trunk_size += sizeof(*trunk);
	data_size = mlx5_trunk_size_get(pool, idx);
	bmp_size = rte_bitmap_get_memory_footprint(data_size);
	/* rte_bitmap requires memory cacheline aligned. */
	trunk_size += RTE_CACHE_LINE_ROUNDUP(data_size * pool->cfg.size);
	trunk_size += bmp_size;
	trunk = pool->cfg.malloc(0, trunk_size,
				 RTE_CACHE_LINE_SIZE, rte_socket_id());
	if (!trunk)
		return -ENOMEM;
	pool->trunks[idx] = trunk;
	trunk->idx = idx;
	trunk->free = data_size;
	trunk->prev = TRUNK_INVALID;
	trunk->next = TRUNK_INVALID;
	MLX5_ASSERT(pool->free_list == TRUNK_INVALID);
	pool->free_list = idx;
	/* Mark all entries as available. */
	trunk->bmp = rte_bitmap_init_with_all_set(data_size, &trunk->data
		     [RTE_CACHE_LINE_ROUNDUP(data_size * pool->cfg.size)],
		     bmp_size);
	/* Clear the overhead bits in the trunk if it happens. */
	if (cur_max_idx + data_size > pool->cfg.max_idx) {
		for (i = pool->cfg.max_idx - cur_max_idx; i < data_size; i++)
			rte_bitmap_clear(trunk->bmp, i);
	}
	MLX5_ASSERT(trunk->bmp);
	pool->n_trunk_valid++;
#ifdef POOL_DEBUG
	pool->trunk_new++;
	pool->trunk_avail++;
#endif
	return 0;
}

static inline struct mlx5_indexed_cache *
mlx5_ipool_update_global_cache(struct mlx5_indexed_pool *pool, int cidx)
{
	struct mlx5_indexed_cache *gc, *lc, *olc = NULL;

	lc = pool->cache[cidx]->lc;
	gc = __atomic_load_n(&pool->gc, __ATOMIC_RELAXED);
	if (gc && lc != gc) {
		mlx5_ipool_lock(pool);
		if (lc && !(--lc->ref_cnt))
			olc = lc;
		lc = pool->gc;
		lc->ref_cnt++;
		pool->cache[cidx]->lc = lc;
		mlx5_ipool_unlock(pool);
		if (olc)
			pool->cfg.free(olc);
	}
	return lc;
}

static uint32_t
mlx5_ipool_allocate_from_global(struct mlx5_indexed_pool *pool, int cidx)
{
	struct mlx5_indexed_trunk *trunk;
	struct mlx5_indexed_cache *p, *lc, *olc = NULL;
	size_t trunk_size = 0;
	size_t data_size;
	uint32_t cur_max_idx, trunk_idx, trunk_n;
	uint32_t fetch_size, ts_idx, i;
	int n_grow;

check_again:
	p = NULL;
	fetch_size = 0;
	/*
	 * Fetch new index from global if possible. First round local
	 * cache will be NULL.
	 */
	lc = pool->cache[cidx]->lc;
	mlx5_ipool_lock(pool);
	/* Try to update local cache first. */
	if (likely(pool->gc)) {
		if (lc != pool->gc) {
			if (lc && !(--lc->ref_cnt))
				olc = lc;
			lc = pool->gc;
			lc->ref_cnt++;
			pool->cache[cidx]->lc = lc;
		}
		if (lc->len) {
			/* Use the updated local cache to fetch index. */
			fetch_size = pool->cfg.per_core_cache >> 2;
			if (lc->len < fetch_size)
				fetch_size = lc->len;
			lc->len -= fetch_size;
			memcpy(pool->cache[cidx]->idx, &lc->idx[lc->len],
			       sizeof(uint32_t) * fetch_size);
		}
	}
	mlx5_ipool_unlock(pool);
	if (unlikely(olc)) {
		pool->cfg.free(olc);
		olc = NULL;
	}
	if (fetch_size) {
		pool->cache[cidx]->len = fetch_size - 1;
		return pool->cache[cidx]->idx[pool->cache[cidx]->len];
	}
	trunk_idx = lc ? __atomic_load_n(&lc->n_trunk_valid,
			 __ATOMIC_ACQUIRE) : 0;
	trunk_n = lc ? lc->n_trunk : 0;
	cur_max_idx = mlx5_trunk_idx_offset_get(pool, trunk_idx);
	/* Check if index reach maximum. */
	if (trunk_idx == TRUNK_MAX_IDX ||
	    cur_max_idx >= pool->cfg.max_idx)
		return 0;
	/* No enough space in trunk array, resize the trunks array. */
	if (trunk_idx == trunk_n) {
		n_grow = trunk_idx ? trunk_idx :
			     RTE_CACHE_LINE_SIZE / sizeof(void *);
		cur_max_idx = mlx5_trunk_idx_offset_get(pool, trunk_n + n_grow);
		/* Resize the trunk array. */
		p = pool->cfg.malloc(0, ((trunk_idx + n_grow) *
			sizeof(struct mlx5_indexed_trunk *)) +
			(cur_max_idx * sizeof(uint32_t)) + sizeof(*p),
			RTE_CACHE_LINE_SIZE, rte_socket_id());
		if (!p)
			return 0;
		p->trunks = (struct mlx5_indexed_trunk **)&p->idx[cur_max_idx];
		if (lc)
			memcpy(p->trunks, lc->trunks, trunk_idx *
		       sizeof(struct mlx5_indexed_trunk *));
#ifdef RTE_LIBRTE_MLX5_DEBUG
		memset(RTE_PTR_ADD(p->trunks, trunk_idx * sizeof(void *)), 0,
			n_grow * sizeof(void *));
#endif
		p->n_trunk_valid = trunk_idx;
		p->n_trunk = trunk_n + n_grow;
		p->len = 0;
	}
	/* Prepare the new trunk. */
	trunk_size = sizeof(*trunk);
	data_size = mlx5_trunk_size_get(pool, trunk_idx);
	trunk_size += RTE_CACHE_LINE_ROUNDUP(data_size * pool->cfg.size);
	trunk = pool->cfg.malloc(0, trunk_size,
				 RTE_CACHE_LINE_SIZE, rte_socket_id());
	if (unlikely(!trunk)) {
		pool->cfg.free(p);
		return 0;
	}
	trunk->idx = trunk_idx;
	trunk->free = data_size;
	mlx5_ipool_lock(pool);
	/*
	 * Double check if trunks has been updated or have available index.
	 * During the new trunk allocate, index may still be flushed to the
	 * global cache. So also need to check the pool->gc->len.
	 */
	if (pool->gc && (lc != pool->gc ||
	    lc->n_trunk_valid != trunk_idx ||
	    pool->gc->len)) {
		mlx5_ipool_unlock(pool);
		if (p)
			pool->cfg.free(p);
		pool->cfg.free(trunk);
		goto check_again;
	}
	/* Resize the trunk array and update local cache first.  */
	if (p) {
		if (lc && !(--lc->ref_cnt))
			olc = lc;
		lc = p;
		lc->ref_cnt = 1;
		pool->cache[cidx]->lc = lc;
		__atomic_store_n(&pool->gc, p, __ATOMIC_RELAXED);
	}
	/* Add trunk to trunks array. */
	lc->trunks[trunk_idx] = trunk;
	__atomic_fetch_add(&lc->n_trunk_valid, 1, __ATOMIC_RELAXED);
	/* Enqueue half of the index to global. */
	ts_idx = mlx5_trunk_idx_offset_get(pool, trunk_idx) + 1;
	fetch_size = trunk->free >> 1;
	if (fetch_size > pool->cfg.per_core_cache)
		fetch_size = trunk->free - pool->cfg.per_core_cache;
	for (i = 0; i < fetch_size; i++)
		lc->idx[i] = ts_idx + i;
	lc->len = fetch_size;
	mlx5_ipool_unlock(pool);
	/* Copy left half - 1 to local cache index array. */
	pool->cache[cidx]->len = trunk->free - fetch_size - 1;
	ts_idx += fetch_size;
	for (i = 0; i < pool->cache[cidx]->len; i++)
		pool->cache[cidx]->idx[i] = ts_idx + i;
	if (olc)
		pool->cfg.free(olc);
	return ts_idx + i;
}

static void *
_mlx5_ipool_get_cache(struct mlx5_indexed_pool *pool, int cidx, uint32_t idx)
{
	struct mlx5_indexed_trunk *trunk;
	struct mlx5_indexed_cache *lc;
	uint32_t trunk_idx;
	uint32_t entry_idx;

	MLX5_ASSERT(idx);
	if (unlikely(!pool->cache[cidx])) {
		pool->cache[cidx] = pool->cfg.malloc(MLX5_MEM_ZERO,
			sizeof(struct mlx5_ipool_per_lcore) +
			(pool->cfg.per_core_cache * sizeof(uint32_t)),
			RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
		if (!pool->cache[cidx]) {
			DRV_LOG(ERR, "Ipool cache%d allocate failed\n", cidx);
			return NULL;
		}
	}
	lc = mlx5_ipool_update_global_cache(pool, cidx);
	idx -= 1;
	trunk_idx = mlx5_trunk_idx_get(pool, idx);
	trunk = lc->trunks[trunk_idx];
	MLX5_ASSERT(trunk);
	entry_idx = idx - mlx5_trunk_idx_offset_get(pool, trunk_idx);
	return &trunk->data[entry_idx * pool->cfg.size];
}

static void *
mlx5_ipool_get_cache(struct mlx5_indexed_pool *pool, uint32_t idx)
{
	void *entry;
	int cidx;

	cidx = rte_lcore_index(rte_lcore_id());
	if (unlikely(cidx == -1)) {
		cidx = RTE_MAX_LCORE;
		rte_spinlock_lock(&pool->lcore_lock);
	}
	entry = _mlx5_ipool_get_cache(pool, cidx, idx);
	if (unlikely(cidx == RTE_MAX_LCORE))
		rte_spinlock_unlock(&pool->lcore_lock);
	return entry;
}


static void *
_mlx5_ipool_malloc_cache(struct mlx5_indexed_pool *pool, int cidx,
			 uint32_t *idx)
{
	if (unlikely(!pool->cache[cidx])) {
		pool->cache[cidx] = pool->cfg.malloc(MLX5_MEM_ZERO,
			sizeof(struct mlx5_ipool_per_lcore) +
			(pool->cfg.per_core_cache * sizeof(uint32_t)),
			RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
		if (!pool->cache[cidx]) {
			DRV_LOG(ERR, "Ipool cache%d allocate failed\n", cidx);
			return NULL;
		}
	} else if (pool->cache[cidx]->len) {
		pool->cache[cidx]->len--;
		*idx = pool->cache[cidx]->idx[pool->cache[cidx]->len];
		return _mlx5_ipool_get_cache(pool, cidx, *idx);
	}
	/* Not enough idx in global cache. Keep fetching from global. */
	*idx = mlx5_ipool_allocate_from_global(pool, cidx);
	if (unlikely(!(*idx)))
		return NULL;
	return _mlx5_ipool_get_cache(pool, cidx, *idx);
}

static void *
mlx5_ipool_malloc_cache(struct mlx5_indexed_pool *pool, uint32_t *idx)
{
	void *entry;
	int cidx;

	cidx = rte_lcore_index(rte_lcore_id());
	if (unlikely(cidx == -1)) {
		cidx = RTE_MAX_LCORE;
		rte_spinlock_lock(&pool->lcore_lock);
	}
	entry = _mlx5_ipool_malloc_cache(pool, cidx, idx);
	if (unlikely(cidx == RTE_MAX_LCORE))
		rte_spinlock_unlock(&pool->lcore_lock);
	return entry;
}

static void
_mlx5_ipool_free_cache(struct mlx5_indexed_pool *pool, int cidx, uint32_t idx)
{
	struct mlx5_ipool_per_lcore *ilc;
	struct mlx5_indexed_cache *gc, *olc = NULL;
	uint32_t reclaim_num = 0;

	MLX5_ASSERT(idx);
	/*
	 * When index was allocated on core A but freed on core B. In this
	 * case check if local cache on core B was allocated before.
	 */
	if (unlikely(!pool->cache[cidx])) {
		pool->cache[cidx] = pool->cfg.malloc(MLX5_MEM_ZERO,
			sizeof(struct mlx5_ipool_per_lcore) +
			(pool->cfg.per_core_cache * sizeof(uint32_t)),
			RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
		if (!pool->cache[cidx]) {
			DRV_LOG(ERR, "Ipool cache%d allocate failed\n", cidx);
			return;
		}
	}
	/* Try to enqueue to local index cache. */
	if (pool->cache[cidx]->len < pool->cfg.per_core_cache) {
		pool->cache[cidx]->idx[pool->cache[cidx]->len] = idx;
		pool->cache[cidx]->len++;
		return;
	}
	ilc = pool->cache[cidx];
	reclaim_num = pool->cfg.per_core_cache >> 2;
	ilc->len -= reclaim_num;
	/* Local index cache full, try with global index cache. */
	mlx5_ipool_lock(pool);
	gc = pool->gc;
	if (ilc->lc != gc) {
		if (!(--ilc->lc->ref_cnt))
			olc = ilc->lc;
		gc->ref_cnt++;
		ilc->lc = gc;
	}
	memcpy(&gc->idx[gc->len], &ilc->idx[ilc->len],
	       reclaim_num * sizeof(uint32_t));
	gc->len += reclaim_num;
	mlx5_ipool_unlock(pool);
	if (olc)
		pool->cfg.free(olc);
	pool->cache[cidx]->idx[pool->cache[cidx]->len] = idx;
	pool->cache[cidx]->len++;
}

static void
mlx5_ipool_free_cache(struct mlx5_indexed_pool *pool, uint32_t idx)
{
	int cidx;

	cidx = rte_lcore_index(rte_lcore_id());
	if (unlikely(cidx == -1)) {
		cidx = RTE_MAX_LCORE;
		rte_spinlock_lock(&pool->lcore_lock);
	}
	_mlx5_ipool_free_cache(pool, cidx, idx);
	if (unlikely(cidx == RTE_MAX_LCORE))
		rte_spinlock_unlock(&pool->lcore_lock);
}

void *
mlx5_ipool_malloc(struct mlx5_indexed_pool *pool, uint32_t *idx)
{
	struct mlx5_indexed_trunk *trunk;
	uint64_t slab = 0;
	uint32_t iidx = 0;
	void *p;

	if (pool->cfg.per_core_cache)
		return mlx5_ipool_malloc_cache(pool, idx);
	mlx5_ipool_lock(pool);
	if (pool->free_list == TRUNK_INVALID) {
		/* If no available trunks, grow new. */
		if (mlx5_ipool_grow(pool)) {
			mlx5_ipool_unlock(pool);
			return NULL;
		}
	}
	MLX5_ASSERT(pool->free_list != TRUNK_INVALID);
	trunk = pool->trunks[pool->free_list];
	MLX5_ASSERT(trunk->free);
	if (!rte_bitmap_scan(trunk->bmp, &iidx, &slab)) {
		mlx5_ipool_unlock(pool);
		return NULL;
	}
	MLX5_ASSERT(slab);
	iidx += __builtin_ctzll(slab);
	MLX5_ASSERT(iidx != UINT32_MAX);
	MLX5_ASSERT(iidx < mlx5_trunk_size_get(pool, trunk->idx));
	rte_bitmap_clear(trunk->bmp, iidx);
	p = &trunk->data[iidx * pool->cfg.size];
	/*
	 * The ipool index should grow continually from small to big,
	 * some features as metering only accept limited bits of index.
	 * Random index with MSB set may be rejected.
	 */
	iidx += mlx5_trunk_idx_offset_get(pool, trunk->idx);
	iidx += 1; /* non-zero index. */
	trunk->free--;
#ifdef POOL_DEBUG
	pool->n_entry++;
#endif
	if (!trunk->free) {
		/* Full trunk will be removed from free list in imalloc. */
		MLX5_ASSERT(pool->free_list == trunk->idx);
		pool->free_list = trunk->next;
		if (trunk->next != TRUNK_INVALID)
			pool->trunks[trunk->next]->prev = TRUNK_INVALID;
		trunk->prev = TRUNK_INVALID;
		trunk->next = TRUNK_INVALID;
#ifdef POOL_DEBUG
		pool->trunk_empty++;
		pool->trunk_avail--;
#endif
	}
	*idx = iidx;
	mlx5_ipool_unlock(pool);
	return p;
}

void *
mlx5_ipool_zmalloc(struct mlx5_indexed_pool *pool, uint32_t *idx)
{
	void *entry = mlx5_ipool_malloc(pool, idx);

	if (entry && pool->cfg.size)
		memset(entry, 0, pool->cfg.size);
	return entry;
}

void
mlx5_ipool_free(struct mlx5_indexed_pool *pool, uint32_t idx)
{
	struct mlx5_indexed_trunk *trunk;
	uint32_t trunk_idx;
	uint32_t entry_idx;

	if (!idx)
		return;
	if (pool->cfg.per_core_cache) {
		mlx5_ipool_free_cache(pool, idx);
		return;
	}
	idx -= 1;
	mlx5_ipool_lock(pool);
	trunk_idx = mlx5_trunk_idx_get(pool, idx);
	if ((!pool->cfg.release_mem_en && trunk_idx >= pool->n_trunk_valid) ||
	    (pool->cfg.release_mem_en && trunk_idx >= pool->n_trunk))
		goto out;
	trunk = pool->trunks[trunk_idx];
	if (!trunk)
		goto out;
	entry_idx = idx - mlx5_trunk_idx_offset_get(pool, trunk->idx);
	if (trunk_idx != trunk->idx ||
	    rte_bitmap_get(trunk->bmp, entry_idx))
		goto out;
	rte_bitmap_set(trunk->bmp, entry_idx);
	trunk->free++;
	if (pool->cfg.release_mem_en && trunk->free == mlx5_trunk_size_get
	   (pool, trunk->idx)) {
		if (pool->free_list == trunk->idx)
			pool->free_list = trunk->next;
		if (trunk->next != TRUNK_INVALID)
			pool->trunks[trunk->next]->prev = trunk->prev;
		if (trunk->prev != TRUNK_INVALID)
			pool->trunks[trunk->prev]->next = trunk->next;
		pool->cfg.free(trunk);
		pool->trunks[trunk_idx] = NULL;
		pool->n_trunk_valid--;
#ifdef POOL_DEBUG
		pool->trunk_avail--;
		pool->trunk_free++;
#endif
		if (pool->n_trunk_valid == 0) {
			pool->cfg.free(pool->trunks);
			pool->trunks = NULL;
			pool->n_trunk = 0;
		}
	} else if (trunk->free == 1) {
		/* Put into free trunk list head. */
		MLX5_ASSERT(pool->free_list != trunk->idx);
		trunk->next = pool->free_list;
		trunk->prev = TRUNK_INVALID;
		if (pool->free_list != TRUNK_INVALID)
			pool->trunks[pool->free_list]->prev = trunk->idx;
		pool->free_list = trunk->idx;
#ifdef POOL_DEBUG
		pool->trunk_empty--;
		pool->trunk_avail++;
#endif
	}
#ifdef POOL_DEBUG
	pool->n_entry--;
#endif
out:
	mlx5_ipool_unlock(pool);
}

void *
mlx5_ipool_get(struct mlx5_indexed_pool *pool, uint32_t idx)
{
	struct mlx5_indexed_trunk *trunk;
	void *p = NULL;
	uint32_t trunk_idx;
	uint32_t entry_idx;

	if (!idx)
		return NULL;
	if (pool->cfg.per_core_cache)
		return mlx5_ipool_get_cache(pool, idx);
	idx -= 1;
	mlx5_ipool_lock(pool);
	trunk_idx = mlx5_trunk_idx_get(pool, idx);
	if ((!pool->cfg.release_mem_en && trunk_idx >= pool->n_trunk_valid) ||
	    (pool->cfg.release_mem_en && trunk_idx >= pool->n_trunk))
		goto out;
	trunk = pool->trunks[trunk_idx];
	if (!trunk)
		goto out;
	entry_idx = idx - mlx5_trunk_idx_offset_get(pool, trunk->idx);
	if (trunk_idx != trunk->idx ||
	    rte_bitmap_get(trunk->bmp, entry_idx))
		goto out;
	p = &trunk->data[entry_idx * pool->cfg.size];
out:
	mlx5_ipool_unlock(pool);
	return p;
}

int
mlx5_ipool_destroy(struct mlx5_indexed_pool *pool)
{
	struct mlx5_indexed_trunk **trunks = NULL;
	struct mlx5_indexed_cache *gc = pool->gc;
	uint32_t i, n_trunk_valid = 0;

	MLX5_ASSERT(pool);
	mlx5_ipool_lock(pool);
	if (pool->cfg.per_core_cache) {
		for (i = 0; i <= RTE_MAX_LCORE; i++) {
			/*
			 * Free only old global cache. Pool gc will be
			 * freed at last.
			 */
			if (pool->cache[i]) {
				if (pool->cache[i]->lc &&
				    pool->cache[i]->lc != pool->gc &&
				    (!(--pool->cache[i]->lc->ref_cnt)))
					pool->cfg.free(pool->cache[i]->lc);
				pool->cfg.free(pool->cache[i]);
			}
		}
		if (gc) {
			trunks = gc->trunks;
			n_trunk_valid = gc->n_trunk_valid;
		}
	} else {
		gc = NULL;
		trunks = pool->trunks;
		n_trunk_valid = pool->n_trunk_valid;
	}
	for (i = 0; i < n_trunk_valid; i++) {
		if (trunks[i])
			pool->cfg.free(trunks[i]);
	}
	if (!gc && trunks)
		pool->cfg.free(trunks);
	if (gc)
		pool->cfg.free(gc);
	mlx5_ipool_unlock(pool);
	mlx5_free(pool);
	return 0;
}

void
mlx5_ipool_flush_cache(struct mlx5_indexed_pool *pool)
{
	uint32_t i, j;
	struct mlx5_indexed_cache *gc;
	struct rte_bitmap *ibmp;
	uint32_t bmp_num, mem_size;

	if (!pool->cfg.per_core_cache)
		return;
	gc = pool->gc;
	if (!gc)
		return;
	/* Reset bmp. */
	bmp_num = mlx5_trunk_idx_offset_get(pool, gc->n_trunk_valid);
	mem_size = rte_bitmap_get_memory_footprint(bmp_num);
	pool->bmp_mem = pool->cfg.malloc(MLX5_MEM_ZERO, mem_size,
					 RTE_CACHE_LINE_SIZE, rte_socket_id());
	if (!pool->bmp_mem) {
		DRV_LOG(ERR, "Ipool bitmap mem allocate failed.\n");
		return;
	}
	ibmp = rte_bitmap_init_with_all_set(bmp_num, pool->bmp_mem, mem_size);
	if (!ibmp) {
		pool->cfg.free(pool->bmp_mem);
		pool->bmp_mem = NULL;
		DRV_LOG(ERR, "Ipool bitmap create failed.\n");
		return;
	}
	pool->ibmp = ibmp;
	/* Clear global cache. */
	for (i = 0; i < gc->len; i++)
		rte_bitmap_clear(ibmp, gc->idx[i] - 1);
	/* Clear core cache. */
	for (i = 0; i < RTE_MAX_LCORE + 1; i++) {
		struct mlx5_ipool_per_lcore *ilc = pool->cache[i];

		if (!ilc)
			continue;
		for (j = 0; j < ilc->len; j++)
			rte_bitmap_clear(ibmp, ilc->idx[j] - 1);
	}
}

static void *
mlx5_ipool_get_next_cache(struct mlx5_indexed_pool *pool, uint32_t *pos)
{
	struct rte_bitmap *ibmp;
	uint64_t slab = 0;
	uint32_t iidx = *pos;

	ibmp = pool->ibmp;
	if (!ibmp || !rte_bitmap_scan(ibmp, &iidx, &slab)) {
		if (pool->bmp_mem) {
			pool->cfg.free(pool->bmp_mem);
			pool->bmp_mem = NULL;
			pool->ibmp = NULL;
		}
		return NULL;
	}
	iidx += __builtin_ctzll(slab);
	rte_bitmap_clear(ibmp, iidx);
	iidx++;
	*pos = iidx;
	return mlx5_ipool_get_cache(pool, iidx);
}

void *
mlx5_ipool_get_next(struct mlx5_indexed_pool *pool, uint32_t *pos)
{
	uint32_t idx = *pos;
	void *entry;

	if (pool->cfg.per_core_cache)
		return mlx5_ipool_get_next_cache(pool, pos);
	while (idx <= mlx5_trunk_idx_offset_get(pool, pool->n_trunk)) {
		entry = mlx5_ipool_get(pool, idx);
		if (entry) {
			*pos = idx;
			return entry;
		}
		idx++;
	}
	return NULL;
}

void
mlx5_ipool_dump(struct mlx5_indexed_pool *pool)
{
	printf("Pool %s entry size %u, trunks %u, %d entry per trunk, "
	       "total: %d\n",
	       pool->cfg.type, pool->cfg.size, pool->n_trunk_valid,
	       pool->cfg.trunk_size, pool->n_trunk_valid);
#ifdef POOL_DEBUG
	printf("Pool %s entry %u, trunk alloc %u, empty: %u, "
	       "available %u free %u\n",
	       pool->cfg.type, pool->n_entry, pool->trunk_new,
	       pool->trunk_empty, pool->trunk_avail, pool->trunk_free);
#endif
}

struct mlx5_l3t_tbl *
mlx5_l3t_create(enum mlx5_l3t_type type)
{
	struct mlx5_l3t_tbl *tbl;
	struct mlx5_indexed_pool_config l3t_ip_cfg = {
		.trunk_size = 16,
		.grow_trunk = 6,
		.grow_shift = 1,
		.need_lock = 0,
		.release_mem_en = 1,
		.malloc = mlx5_malloc,
		.free = mlx5_free,
	};

	if (type >= MLX5_L3T_TYPE_MAX) {
		rte_errno = EINVAL;
		return NULL;
	}
	tbl = mlx5_malloc(MLX5_MEM_ZERO, sizeof(struct mlx5_l3t_tbl), 1,
			  SOCKET_ID_ANY);
	if (!tbl) {
		rte_errno = ENOMEM;
		return NULL;
	}
	tbl->type = type;
	switch (type) {
	case MLX5_L3T_TYPE_WORD:
		l3t_ip_cfg.size = sizeof(struct mlx5_l3t_entry_word);
		l3t_ip_cfg.type = "mlx5_l3t_e_tbl_w";
		break;
	case MLX5_L3T_TYPE_DWORD:
		l3t_ip_cfg.size = sizeof(struct mlx5_l3t_entry_dword);
		l3t_ip_cfg.type = "mlx5_l3t_e_tbl_dw";
		break;
	case MLX5_L3T_TYPE_QWORD:
		l3t_ip_cfg.size = sizeof(struct mlx5_l3t_entry_qword);
		l3t_ip_cfg.type = "mlx5_l3t_e_tbl_qw";
		break;
	default:
		l3t_ip_cfg.size = sizeof(struct mlx5_l3t_entry_ptr);
		l3t_ip_cfg.type = "mlx5_l3t_e_tbl_tpr";
		break;
	}
	rte_spinlock_init(&tbl->sl);
	tbl->eip = mlx5_ipool_create(&l3t_ip_cfg);
	if (!tbl->eip) {
		rte_errno = ENOMEM;
		mlx5_free(tbl);
		tbl = NULL;
	}
	return tbl;
}

void
mlx5_l3t_destroy(struct mlx5_l3t_tbl *tbl)
{
	struct mlx5_l3t_level_tbl *g_tbl, *m_tbl;
	uint32_t i, j;

	if (!tbl)
		return;
	g_tbl = tbl->tbl;
	if (g_tbl) {
		for (i = 0; i < MLX5_L3T_GT_SIZE; i++) {
			m_tbl = g_tbl->tbl[i];
			if (!m_tbl)
				continue;
			for (j = 0; j < MLX5_L3T_MT_SIZE; j++) {
				if (!m_tbl->tbl[j])
					continue;
				MLX5_ASSERT(!((struct mlx5_l3t_entry_word *)
					    m_tbl->tbl[j])->ref_cnt);
				mlx5_ipool_free(tbl->eip,
						((struct mlx5_l3t_entry_word *)
						m_tbl->tbl[j])->idx);
				m_tbl->tbl[j] = 0;
				if (!(--m_tbl->ref_cnt))
					break;
			}
			MLX5_ASSERT(!m_tbl->ref_cnt);
			mlx5_free(g_tbl->tbl[i]);
			g_tbl->tbl[i] = 0;
			if (!(--g_tbl->ref_cnt))
				break;
		}
		MLX5_ASSERT(!g_tbl->ref_cnt);
		mlx5_free(tbl->tbl);
		tbl->tbl = 0;
	}
	mlx5_ipool_destroy(tbl->eip);
	mlx5_free(tbl);
}

static int32_t
__l3t_get_entry(struct mlx5_l3t_tbl *tbl, uint32_t idx,
		union mlx5_l3t_data *data)
{
	struct mlx5_l3t_level_tbl *g_tbl, *m_tbl;
	struct mlx5_l3t_entry_word *w_e_tbl;
	struct mlx5_l3t_entry_dword *dw_e_tbl;
	struct mlx5_l3t_entry_qword *qw_e_tbl;
	struct mlx5_l3t_entry_ptr *ptr_e_tbl;
	void *e_tbl;
	uint32_t entry_idx;

	g_tbl = tbl->tbl;
	if (!g_tbl)
		return -1;
	m_tbl = g_tbl->tbl[(idx >> MLX5_L3T_GT_OFFSET) & MLX5_L3T_GT_MASK];
	if (!m_tbl)
		return -1;
	e_tbl = m_tbl->tbl[(idx >> MLX5_L3T_MT_OFFSET) & MLX5_L3T_MT_MASK];
	if (!e_tbl)
		return -1;
	entry_idx = idx & MLX5_L3T_ET_MASK;
	switch (tbl->type) {
	case MLX5_L3T_TYPE_WORD:
		w_e_tbl = (struct mlx5_l3t_entry_word *)e_tbl;
		data->word = w_e_tbl->entry[entry_idx].data;
		if (w_e_tbl->entry[entry_idx].data)
			w_e_tbl->entry[entry_idx].ref_cnt++;
		break;
	case MLX5_L3T_TYPE_DWORD:
		dw_e_tbl = (struct mlx5_l3t_entry_dword *)e_tbl;
		data->dword = dw_e_tbl->entry[entry_idx].data;
		if (dw_e_tbl->entry[entry_idx].data)
			dw_e_tbl->entry[entry_idx].ref_cnt++;
		break;
	case MLX5_L3T_TYPE_QWORD:
		qw_e_tbl = (struct mlx5_l3t_entry_qword *)e_tbl;
		data->qword = qw_e_tbl->entry[entry_idx].data;
		if (qw_e_tbl->entry[entry_idx].data)
			qw_e_tbl->entry[entry_idx].ref_cnt++;
		break;
	default:
		ptr_e_tbl = (struct mlx5_l3t_entry_ptr *)e_tbl;
		data->ptr = ptr_e_tbl->entry[entry_idx].data;
		if (ptr_e_tbl->entry[entry_idx].data)
			ptr_e_tbl->entry[entry_idx].ref_cnt++;
		break;
	}
	return 0;
}

int32_t
mlx5_l3t_get_entry(struct mlx5_l3t_tbl *tbl, uint32_t idx,
		   union mlx5_l3t_data *data)
{
	int ret;

	rte_spinlock_lock(&tbl->sl);
	ret = __l3t_get_entry(tbl, idx, data);
	rte_spinlock_unlock(&tbl->sl);
	return ret;
}

int32_t
mlx5_l3t_clear_entry(struct mlx5_l3t_tbl *tbl, uint32_t idx)
{
	struct mlx5_l3t_level_tbl *g_tbl, *m_tbl;
	struct mlx5_l3t_entry_word *w_e_tbl;
	struct mlx5_l3t_entry_dword *dw_e_tbl;
	struct mlx5_l3t_entry_qword *qw_e_tbl;
	struct mlx5_l3t_entry_ptr *ptr_e_tbl;
	void *e_tbl;
	uint32_t entry_idx;
	uint64_t ref_cnt;
	int32_t ret = -1;

	rte_spinlock_lock(&tbl->sl);
	g_tbl = tbl->tbl;
	if (!g_tbl)
		goto out;
	m_tbl = g_tbl->tbl[(idx >> MLX5_L3T_GT_OFFSET) & MLX5_L3T_GT_MASK];
	if (!m_tbl)
		goto out;
	e_tbl = m_tbl->tbl[(idx >> MLX5_L3T_MT_OFFSET) & MLX5_L3T_MT_MASK];
	if (!e_tbl)
		goto out;
	entry_idx = idx & MLX5_L3T_ET_MASK;
	switch (tbl->type) {
	case MLX5_L3T_TYPE_WORD:
		w_e_tbl = (struct mlx5_l3t_entry_word *)e_tbl;
		MLX5_ASSERT(w_e_tbl->entry[entry_idx].ref_cnt);
		ret = --w_e_tbl->entry[entry_idx].ref_cnt;
		if (ret)
			goto out;
		w_e_tbl->entry[entry_idx].data = 0;
		ref_cnt = --w_e_tbl->ref_cnt;
		break;
	case MLX5_L3T_TYPE_DWORD:
		dw_e_tbl = (struct mlx5_l3t_entry_dword *)e_tbl;
		MLX5_ASSERT(dw_e_tbl->entry[entry_idx].ref_cnt);
		ret = --dw_e_tbl->entry[entry_idx].ref_cnt;
		if (ret)
			goto out;
		dw_e_tbl->entry[entry_idx].data = 0;
		ref_cnt = --dw_e_tbl->ref_cnt;
		break;
	case MLX5_L3T_TYPE_QWORD:
		qw_e_tbl = (struct mlx5_l3t_entry_qword *)e_tbl;
		MLX5_ASSERT(qw_e_tbl->entry[entry_idx].ref_cnt);
		ret = --qw_e_tbl->entry[entry_idx].ref_cnt;
		if (ret)
			goto out;
		qw_e_tbl->entry[entry_idx].data = 0;
		ref_cnt = --qw_e_tbl->ref_cnt;
		break;
	default:
		ptr_e_tbl = (struct mlx5_l3t_entry_ptr *)e_tbl;
		MLX5_ASSERT(ptr_e_tbl->entry[entry_idx].ref_cnt);
		ret = --ptr_e_tbl->entry[entry_idx].ref_cnt;
		if (ret)
			goto out;
		ptr_e_tbl->entry[entry_idx].data = NULL;
		ref_cnt = --ptr_e_tbl->ref_cnt;
		break;
	}
	if (!ref_cnt) {
		mlx5_ipool_free(tbl->eip,
				((struct mlx5_l3t_entry_word *)e_tbl)->idx);
		m_tbl->tbl[(idx >> MLX5_L3T_MT_OFFSET) & MLX5_L3T_MT_MASK] =
									NULL;
		if (!(--m_tbl->ref_cnt)) {
			mlx5_free(m_tbl);
			g_tbl->tbl
			[(idx >> MLX5_L3T_GT_OFFSET) & MLX5_L3T_GT_MASK] = NULL;
			if (!(--g_tbl->ref_cnt)) {
				mlx5_free(g_tbl);
				tbl->tbl = 0;
			}
		}
	}
out:
	rte_spinlock_unlock(&tbl->sl);
	return ret;
}

static int32_t
__l3t_set_entry(struct mlx5_l3t_tbl *tbl, uint32_t idx,
		union mlx5_l3t_data *data)
{
	struct mlx5_l3t_level_tbl *g_tbl, *m_tbl;
	struct mlx5_l3t_entry_word *w_e_tbl;
	struct mlx5_l3t_entry_dword *dw_e_tbl;
	struct mlx5_l3t_entry_qword *qw_e_tbl;
	struct mlx5_l3t_entry_ptr *ptr_e_tbl;
	void *e_tbl;
	uint32_t entry_idx, tbl_idx = 0;

	/* Check the global table, create it if empty. */
	g_tbl = tbl->tbl;
	if (!g_tbl) {
		g_tbl = mlx5_malloc(MLX5_MEM_ZERO,
				    sizeof(struct mlx5_l3t_level_tbl) +
				    sizeof(void *) * MLX5_L3T_GT_SIZE, 1,
				    SOCKET_ID_ANY);
		if (!g_tbl) {
			rte_errno = ENOMEM;
			return -1;
		}
		tbl->tbl = g_tbl;
	}
	/*
	 * Check the middle table, create it if empty. Ref_cnt will be
	 * increased if new sub table created.
	 */
	m_tbl = g_tbl->tbl[(idx >> MLX5_L3T_GT_OFFSET) & MLX5_L3T_GT_MASK];
	if (!m_tbl) {
		m_tbl = mlx5_malloc(MLX5_MEM_ZERO,
				    sizeof(struct mlx5_l3t_level_tbl) +
				    sizeof(void *) * MLX5_L3T_MT_SIZE, 1,
				    SOCKET_ID_ANY);
		if (!m_tbl) {
			rte_errno = ENOMEM;
			return -1;
		}
		g_tbl->tbl[(idx >> MLX5_L3T_GT_OFFSET) & MLX5_L3T_GT_MASK] =
									m_tbl;
		g_tbl->ref_cnt++;
	}
	/*
	 * Check the entry table, create it if empty. Ref_cnt will be
	 * increased if new sub entry table created.
	 */
	e_tbl = m_tbl->tbl[(idx >> MLX5_L3T_MT_OFFSET) & MLX5_L3T_MT_MASK];
	if (!e_tbl) {
		e_tbl = mlx5_ipool_zmalloc(tbl->eip, &tbl_idx);
		if (!e_tbl) {
			rte_errno = ENOMEM;
			return -1;
		}
		((struct mlx5_l3t_entry_word *)e_tbl)->idx = tbl_idx;
		m_tbl->tbl[(idx >> MLX5_L3T_MT_OFFSET) & MLX5_L3T_MT_MASK] =
									e_tbl;
		m_tbl->ref_cnt++;
	}
	entry_idx = idx & MLX5_L3T_ET_MASK;
	switch (tbl->type) {
	case MLX5_L3T_TYPE_WORD:
		w_e_tbl = (struct mlx5_l3t_entry_word *)e_tbl;
		if (w_e_tbl->entry[entry_idx].data) {
			data->word = w_e_tbl->entry[entry_idx].data;
			w_e_tbl->entry[entry_idx].ref_cnt++;
			rte_errno = EEXIST;
			return -1;
		}
		w_e_tbl->entry[entry_idx].data = data->word;
		w_e_tbl->entry[entry_idx].ref_cnt = 1;
		w_e_tbl->ref_cnt++;
		break;
	case MLX5_L3T_TYPE_DWORD:
		dw_e_tbl = (struct mlx5_l3t_entry_dword *)e_tbl;
		if (dw_e_tbl->entry[entry_idx].data) {
			data->dword = dw_e_tbl->entry[entry_idx].data;
			dw_e_tbl->entry[entry_idx].ref_cnt++;
			rte_errno = EEXIST;
			return -1;
		}
		dw_e_tbl->entry[entry_idx].data = data->dword;
		dw_e_tbl->entry[entry_idx].ref_cnt = 1;
		dw_e_tbl->ref_cnt++;
		break;
	case MLX5_L3T_TYPE_QWORD:
		qw_e_tbl = (struct mlx5_l3t_entry_qword *)e_tbl;
		if (qw_e_tbl->entry[entry_idx].data) {
			data->qword = qw_e_tbl->entry[entry_idx].data;
			qw_e_tbl->entry[entry_idx].ref_cnt++;
			rte_errno = EEXIST;
			return -1;
		}
		qw_e_tbl->entry[entry_idx].data = data->qword;
		qw_e_tbl->entry[entry_idx].ref_cnt = 1;
		qw_e_tbl->ref_cnt++;
		break;
	default:
		ptr_e_tbl = (struct mlx5_l3t_entry_ptr *)e_tbl;
		if (ptr_e_tbl->entry[entry_idx].data) {
			data->ptr = ptr_e_tbl->entry[entry_idx].data;
			ptr_e_tbl->entry[entry_idx].ref_cnt++;
			rte_errno = EEXIST;
			return -1;
		}
		ptr_e_tbl->entry[entry_idx].data = data->ptr;
		ptr_e_tbl->entry[entry_idx].ref_cnt = 1;
		ptr_e_tbl->ref_cnt++;
		break;
	}
	return 0;
}

int32_t
mlx5_l3t_set_entry(struct mlx5_l3t_tbl *tbl, uint32_t idx,
		   union mlx5_l3t_data *data)
{
	int ret;

	rte_spinlock_lock(&tbl->sl);
	ret = __l3t_set_entry(tbl, idx, data);
	rte_spinlock_unlock(&tbl->sl);
	return ret;
}
