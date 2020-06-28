/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019 Mellanox Technologies, Ltd
 */

#include <rte_malloc.h>
#include <rte_hash_crc.h>

#include <mlx5_malloc.h>

#include "mlx5_utils.h"

struct mlx5_hlist *
mlx5_hlist_create(const char *name, uint32_t size)
{
	struct mlx5_hlist *h;
	uint32_t act_size;
	uint32_t alloc_size;

	if (!size)
		return NULL;
	/* Align to the next power of 2, 32bits integer is enough now. */
	if (!rte_is_power_of_2(size)) {
		act_size = rte_align32pow2(size);
		DRV_LOG(WARNING, "Size 0x%" PRIX32 " is not power of 2, will "
			"be aligned to 0x%" PRIX32 ".", size, act_size);
	} else {
		act_size = size;
	}
	alloc_size = sizeof(struct mlx5_hlist) +
		     sizeof(struct mlx5_hlist_head) * act_size;
	/* Using zmalloc, then no need to initialize the heads. */
	h = mlx5_malloc(MLX5_MEM_ZERO, alloc_size, RTE_CACHE_LINE_SIZE,
			SOCKET_ID_ANY);
	if (!h) {
		DRV_LOG(ERR, "No memory for hash list %s creation",
			name ? name : "None");
		return NULL;
	}
	if (name)
		snprintf(h->name, MLX5_HLIST_NAMESIZE, "%s", name);
	h->table_sz = act_size;
	h->mask = act_size - 1;
	DRV_LOG(DEBUG, "Hash list with %s size 0x%" PRIX32 " is created.",
		h->name, act_size);
	return h;
}

struct mlx5_hlist_entry *
mlx5_hlist_lookup(struct mlx5_hlist *h, uint64_t key)
{
	uint32_t idx;
	struct mlx5_hlist_head *first;
	struct mlx5_hlist_entry *node;

	MLX5_ASSERT(h);
	idx = rte_hash_crc_8byte(key, 0) & h->mask;
	first = &h->heads[idx];
	LIST_FOREACH(node, first, next) {
		if (node->key == key)
			return node;
	}
	return NULL;
}

int
mlx5_hlist_insert(struct mlx5_hlist *h, struct mlx5_hlist_entry *entry)
{
	uint32_t idx;
	struct mlx5_hlist_head *first;
	struct mlx5_hlist_entry *node;

	MLX5_ASSERT(h && entry);
	idx = rte_hash_crc_8byte(entry->key, 0) & h->mask;
	first = &h->heads[idx];
	/* No need to reuse the lookup function. */
	LIST_FOREACH(node, first, next) {
		if (node->key == entry->key)
			return -EEXIST;
	}
	LIST_INSERT_HEAD(first, entry, next);
	return 0;
}

void
mlx5_hlist_remove(struct mlx5_hlist *h __rte_unused,
		  struct mlx5_hlist_entry *entry)
{
	MLX5_ASSERT(entry && entry->next.le_prev);
	LIST_REMOVE(entry, next);
	/* Set to NULL to get rid of removing action for more than once. */
	entry->next.le_prev = NULL;
}

void
mlx5_hlist_destroy(struct mlx5_hlist *h,
		   mlx5_hlist_destroy_callback_fn cb, void *ctx)
{
	uint32_t idx;
	struct mlx5_hlist_entry *entry;

	MLX5_ASSERT(h);
	for (idx = 0; idx < h->table_sz; ++idx) {
		/* no LIST_FOREACH_SAFE, using while instead */
		while (!LIST_EMPTY(&h->heads[idx])) {
			entry = LIST_FIRST(&h->heads[idx]);
			LIST_REMOVE(entry, next);
			/*
			 * The owner of whole element which contains data entry
			 * is the user, so it's the user's duty to do the clean
			 * up and the free work because someone may not put the
			 * hlist entry at the beginning(suggested to locate at
			 * the beginning). Or else the default free function
			 * will be used.
			 */
			if (cb)
				cb(entry, ctx);
			else
				mlx5_free(entry);
		}
	}
	mlx5_free(h);
}

static inline void
mlx5_ipool_lock(struct mlx5_indexed_pool *pool)
{
	if (pool->cfg.need_lock)
		rte_spinlock_lock(&pool->lock);
}

static inline void
mlx5_ipool_unlock(struct mlx5_indexed_pool *pool)
{
	if (pool->cfg.need_lock)
		rte_spinlock_unlock(&pool->lock);
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

	if (!cfg || !cfg->size || (!cfg->malloc ^ !cfg->free) ||
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
	pool->free_list = TRUNK_INVALID;
	if (pool->cfg.need_lock)
		rte_spinlock_init(&pool->lock);
	/*
	 * Initialize the dynamic grow trunk size lookup table to have a quick
	 * lookup for the trunk entry index offset.
	 */
	for (i = 0; i < cfg->grow_trunk; i++) {
		pool->grow_tbl[i] = cfg->trunk_size << (cfg->grow_shift * i);
		if (i > 0)
			pool->grow_tbl[i] += pool->grow_tbl[i - 1];
	}
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
	uint32_t idx;

	if (pool->n_trunk_valid == TRUNK_MAX_IDX)
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
	MLX5_ASSERT(trunk->bmp);
	pool->n_trunk_valid++;
#ifdef POOL_DEBUG
	pool->trunk_new++;
	pool->trunk_avail++;
#endif
	return 0;
}

void *
mlx5_ipool_malloc(struct mlx5_indexed_pool *pool, uint32_t *idx)
{
	struct mlx5_indexed_trunk *trunk;
	uint64_t slab = 0;
	uint32_t iidx = 0;
	void *p;

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

	if (entry)
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
	struct mlx5_indexed_trunk **trunks;
	uint32_t i;

	MLX5_ASSERT(pool);
	mlx5_ipool_lock(pool);
	trunks = pool->trunks;
	for (i = 0; i < pool->n_trunk; i++) {
		if (trunks[i])
			pool->cfg.free(trunks[i]);
	}
	if (!pool->trunks)
		pool->cfg.free(pool->trunks);
	mlx5_ipool_unlock(pool);
	mlx5_free(pool);
	return 0;
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
		l3t_ip_cfg.size = sizeof(struct mlx5_l3t_entry_word) +
				  sizeof(uint16_t) * MLX5_L3T_ET_SIZE;
		l3t_ip_cfg.type = "mlx5_l3t_e_tbl_w";
		break;
	case MLX5_L3T_TYPE_DWORD:
		l3t_ip_cfg.size = sizeof(struct mlx5_l3t_entry_dword) +
				  sizeof(uint32_t) * MLX5_L3T_ET_SIZE;
		l3t_ip_cfg.type = "mlx5_l3t_e_tbl_dw";
		break;
	case MLX5_L3T_TYPE_QWORD:
		l3t_ip_cfg.size = sizeof(struct mlx5_l3t_entry_qword) +
				  sizeof(uint64_t) * MLX5_L3T_ET_SIZE;
		l3t_ip_cfg.type = "mlx5_l3t_e_tbl_qw";
		break;
	default:
		l3t_ip_cfg.size = sizeof(struct mlx5_l3t_entry_ptr) +
				  sizeof(void *) * MLX5_L3T_ET_SIZE;
		l3t_ip_cfg.type = "mlx5_l3t_e_tbl_tpr";
		break;
	}
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

uint32_t
mlx5_l3t_get_entry(struct mlx5_l3t_tbl *tbl, uint32_t idx,
		   union mlx5_l3t_data *data)
{
	struct mlx5_l3t_level_tbl *g_tbl, *m_tbl;
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
		data->word = ((struct mlx5_l3t_entry_word *)e_tbl)->entry
			     [entry_idx];
		break;
	case MLX5_L3T_TYPE_DWORD:
		data->dword = ((struct mlx5_l3t_entry_dword *)e_tbl)->entry
			     [entry_idx];
		break;
	case MLX5_L3T_TYPE_QWORD:
		data->qword = ((struct mlx5_l3t_entry_qword *)e_tbl)->entry
			      [entry_idx];
		break;
	default:
		data->ptr = ((struct mlx5_l3t_entry_ptr *)e_tbl)->entry
			    [entry_idx];
		break;
	}
	return 0;
}

void
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

	g_tbl = tbl->tbl;
	if (!g_tbl)
		return;
	m_tbl = g_tbl->tbl[(idx >> MLX5_L3T_GT_OFFSET) & MLX5_L3T_GT_MASK];
	if (!m_tbl)
		return;
	e_tbl = m_tbl->tbl[(idx >> MLX5_L3T_MT_OFFSET) & MLX5_L3T_MT_MASK];
	if (!e_tbl)
		return;
	entry_idx = idx & MLX5_L3T_ET_MASK;
	switch (tbl->type) {
	case MLX5_L3T_TYPE_WORD:
		w_e_tbl = (struct mlx5_l3t_entry_word *)e_tbl;
		w_e_tbl->entry[entry_idx] = 0;
		ref_cnt = --w_e_tbl->ref_cnt;
		break;
	case MLX5_L3T_TYPE_DWORD:
		dw_e_tbl = (struct mlx5_l3t_entry_dword *)e_tbl;
		dw_e_tbl->entry[entry_idx] = 0;
		ref_cnt = --dw_e_tbl->ref_cnt;
		break;
	case MLX5_L3T_TYPE_QWORD:
		qw_e_tbl = (struct mlx5_l3t_entry_qword *)e_tbl;
		qw_e_tbl->entry[entry_idx] = 0;
		ref_cnt = --qw_e_tbl->ref_cnt;
		break;
	default:
		ptr_e_tbl = (struct mlx5_l3t_entry_ptr *)e_tbl;
		ptr_e_tbl->entry[entry_idx] = NULL;
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
}

uint32_t
mlx5_l3t_set_entry(struct mlx5_l3t_tbl *tbl, uint32_t idx,
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
		w_e_tbl->entry[entry_idx] = data->word;
		w_e_tbl->ref_cnt++;
		break;
	case MLX5_L3T_TYPE_DWORD:
		dw_e_tbl = (struct mlx5_l3t_entry_dword *)e_tbl;
		dw_e_tbl->entry[entry_idx] = data->dword;
		dw_e_tbl->ref_cnt++;
		break;
	case MLX5_L3T_TYPE_QWORD:
		qw_e_tbl = (struct mlx5_l3t_entry_qword *)e_tbl;
		qw_e_tbl->entry[entry_idx] = data->qword;
		qw_e_tbl->ref_cnt++;
		break;
	default:
		ptr_e_tbl = (struct mlx5_l3t_entry_ptr *)e_tbl;
		ptr_e_tbl->entry[entry_idx] = data->ptr;
		ptr_e_tbl->ref_cnt++;
		break;
	}
	return 0;
}
