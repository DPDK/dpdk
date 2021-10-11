/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019 Mellanox Technologies, Ltd
 */

#include <rte_malloc.h>
#include <rte_hash_crc.h>
#include <rte_errno.h>

#include <mlx5_malloc.h>

#include "mlx5_common_utils.h"
#include "mlx5_common_log.h"

/********************* mlx5 list ************************/

static int
mlx5_list_init(struct mlx5_list_inconst *l_inconst,
	       struct mlx5_list_const *l_const,
	       struct mlx5_list_cache *gc)
{
	rte_rwlock_init(&l_inconst->lock);
	if (l_const->lcores_share) {
		l_inconst->cache[MLX5_LIST_GLOBAL] = gc;
		LIST_INIT(&l_inconst->cache[MLX5_LIST_GLOBAL]->h);
	}
	return 0;
}

struct mlx5_list *
mlx5_list_create(const char *name, void *ctx, bool lcores_share,
		 mlx5_list_create_cb cb_create,
		 mlx5_list_match_cb cb_match,
		 mlx5_list_remove_cb cb_remove,
		 mlx5_list_clone_cb cb_clone,
		 mlx5_list_clone_free_cb cb_clone_free)
{
	struct mlx5_list *list;
	struct mlx5_list_cache *gc = NULL;

	if (!cb_match || !cb_create || !cb_remove || !cb_clone ||
	    !cb_clone_free) {
		rte_errno = EINVAL;
		return NULL;
	}
	list = mlx5_malloc(MLX5_MEM_ZERO,
			   sizeof(*list) + (lcores_share ? sizeof(*gc) : 0),
			   0, SOCKET_ID_ANY);

	if (!list)
		return NULL;
	if (name)
		snprintf(list->l_const.name,
			 sizeof(list->l_const.name), "%s", name);
	list->l_const.ctx = ctx;
	list->l_const.lcores_share = lcores_share;
	list->l_const.cb_create = cb_create;
	list->l_const.cb_match = cb_match;
	list->l_const.cb_remove = cb_remove;
	list->l_const.cb_clone = cb_clone;
	list->l_const.cb_clone_free = cb_clone_free;
	rte_spinlock_init(&list->l_const.lcore_lock);
	if (lcores_share)
		gc = (struct mlx5_list_cache *)(list + 1);
	if (mlx5_list_init(&list->l_inconst, &list->l_const, gc) != 0) {
		mlx5_free(list);
		return NULL;
	}
	DRV_LOG(DEBUG, "mlx5 list %s was created.", name);
	return list;
}

static struct mlx5_list_entry *
__list_lookup(struct mlx5_list_inconst *l_inconst,
	      struct mlx5_list_const *l_const,
	      int lcore_index, void *ctx, bool reuse)
{
	struct mlx5_list_entry *entry =
				LIST_FIRST(&l_inconst->cache[lcore_index]->h);
	uint32_t ret;

	while (entry != NULL) {
		if (l_const->cb_match(l_const->ctx, entry, ctx) == 0) {
			if (reuse) {
				ret = __atomic_add_fetch(&entry->ref_cnt, 1,
							 __ATOMIC_RELAXED) - 1;
				DRV_LOG(DEBUG, "mlx5 list %s entry %p ref: %u.",
					l_const->name, (void *)entry,
					entry->ref_cnt);
			} else if (lcore_index < MLX5_LIST_GLOBAL) {
				ret = __atomic_load_n(&entry->ref_cnt,
						      __ATOMIC_RELAXED);
			}
			if (likely(ret != 0 || lcore_index == MLX5_LIST_GLOBAL))
				return entry;
			if (reuse && ret == 0)
				entry->ref_cnt--; /* Invalid entry. */
		}
		entry = LIST_NEXT(entry, next);
	}
	return NULL;
}

static inline struct mlx5_list_entry *
_mlx5_list_lookup(struct mlx5_list_inconst *l_inconst,
		  struct mlx5_list_const *l_const, void *ctx)
{
	struct mlx5_list_entry *entry = NULL;
	int i;

	rte_rwlock_read_lock(&l_inconst->lock);
	for (i = 0; i < MLX5_LIST_GLOBAL; i++) {
		if (!l_inconst->cache[i])
			continue;
		entry = __list_lookup(l_inconst, l_const, i,
			      ctx, false);
		if (entry)
			break;
	}
	rte_rwlock_read_unlock(&l_inconst->lock);
	return entry;
}

struct mlx5_list_entry *
mlx5_list_lookup(struct mlx5_list *list, void *ctx)
{
	return _mlx5_list_lookup(&list->l_inconst, &list->l_const, ctx);
}


static struct mlx5_list_entry *
mlx5_list_cache_insert(struct mlx5_list_inconst *l_inconst,
		       struct mlx5_list_const *l_const, int lcore_index,
		       struct mlx5_list_entry *gentry, void *ctx)
{
	struct mlx5_list_entry *lentry =
			l_const->cb_clone(l_const->ctx, gentry, ctx);

	if (unlikely(!lentry))
		return NULL;
	lentry->ref_cnt = 1u;
	lentry->gentry = gentry;
	lentry->lcore_idx = (uint32_t)lcore_index;
	LIST_INSERT_HEAD(&l_inconst->cache[lcore_index]->h, lentry, next);
	return lentry;
}

static void
__list_cache_clean(struct mlx5_list_inconst *l_inconst,
		   struct mlx5_list_const *l_const,
		   int lcore_index)
{
	struct mlx5_list_cache *c = l_inconst->cache[lcore_index];
	struct mlx5_list_entry *entry = LIST_FIRST(&c->h);
	uint32_t inv_cnt = __atomic_exchange_n(&c->inv_cnt, 0,
					       __ATOMIC_RELAXED);

	while (inv_cnt != 0 && entry != NULL) {
		struct mlx5_list_entry *nentry = LIST_NEXT(entry, next);

		if (__atomic_load_n(&entry->ref_cnt, __ATOMIC_RELAXED) == 0) {
			LIST_REMOVE(entry, next);
			if (l_const->lcores_share)
				l_const->cb_clone_free(l_const->ctx, entry);
			else
				l_const->cb_remove(l_const->ctx, entry);
			inv_cnt--;
		}
		entry = nentry;
	}
}

static inline struct mlx5_list_entry *
_mlx5_list_register(struct mlx5_list_inconst *l_inconst,
		    struct mlx5_list_const *l_const,
		    void *ctx, int lcore_index)
{
	struct mlx5_list_entry *entry = NULL, *local_entry;
	volatile uint32_t prev_gen_cnt = 0;
	MLX5_ASSERT(l_inconst);
	if (unlikely(!l_inconst->cache[lcore_index])) {
		l_inconst->cache[lcore_index] = mlx5_malloc(0,
					sizeof(struct mlx5_list_cache),
					RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
		if (!l_inconst->cache[lcore_index]) {
			rte_errno = ENOMEM;
			return NULL;
		}
		l_inconst->cache[lcore_index]->inv_cnt = 0;
		LIST_INIT(&l_inconst->cache[lcore_index]->h);
	}
	/* 0. Free entries that was invalidated by other lcores. */
	__list_cache_clean(l_inconst, l_const, lcore_index);
	/* 1. Lookup in local cache. */
	local_entry = __list_lookup(l_inconst, l_const, lcore_index, ctx, true);
	if (local_entry)
		return local_entry;
	if (l_const->lcores_share) {
		/* 2. Lookup with read lock on global list, reuse if found. */
		rte_rwlock_read_lock(&l_inconst->lock);
		entry = __list_lookup(l_inconst, l_const, MLX5_LIST_GLOBAL,
				      ctx, true);
		if (likely(entry)) {
			rte_rwlock_read_unlock(&l_inconst->lock);
			return mlx5_list_cache_insert(l_inconst, l_const,
						      lcore_index,
						      entry, ctx);
		}
		prev_gen_cnt = l_inconst->gen_cnt;
		rte_rwlock_read_unlock(&l_inconst->lock);
	}
	/* 3. Prepare new entry for global list and for cache. */
	entry = l_const->cb_create(l_const->ctx, ctx);
	if (unlikely(!entry))
		return NULL;
	entry->ref_cnt = 1u;
	if (!l_const->lcores_share) {
		entry->lcore_idx = (uint32_t)lcore_index;
		LIST_INSERT_HEAD(&l_inconst->cache[lcore_index]->h,
				 entry, next);
		__atomic_add_fetch(&l_inconst->count, 1, __ATOMIC_RELAXED);
		DRV_LOG(DEBUG, "MLX5 list %s c%d entry %p new: %u.",
			l_const->name, lcore_index,
			(void *)entry, entry->ref_cnt);
		return entry;
	}
	local_entry = l_const->cb_clone(l_const->ctx, entry, ctx);
	if (unlikely(!local_entry)) {
		l_const->cb_remove(l_const->ctx, entry);
		return NULL;
	}
	local_entry->ref_cnt = 1u;
	local_entry->gentry = entry;
	local_entry->lcore_idx = (uint32_t)lcore_index;
	rte_rwlock_write_lock(&l_inconst->lock);
	/* 4. Make sure the same entry was not created before the write lock. */
	if (unlikely(prev_gen_cnt != l_inconst->gen_cnt)) {
		struct mlx5_list_entry *oentry = __list_lookup(l_inconst,
							       l_const,
							       MLX5_LIST_GLOBAL,
							       ctx, true);

		if (unlikely(oentry)) {
			/* 4.5. Found real race!!, reuse the old entry. */
			rte_rwlock_write_unlock(&l_inconst->lock);
			l_const->cb_remove(l_const->ctx, entry);
			l_const->cb_clone_free(l_const->ctx, local_entry);
			return mlx5_list_cache_insert(l_inconst, l_const,
						      lcore_index,
						      oentry, ctx);
		}
	}
	/* 5. Update lists. */
	LIST_INSERT_HEAD(&l_inconst->cache[MLX5_LIST_GLOBAL]->h, entry, next);
	l_inconst->gen_cnt++;
	rte_rwlock_write_unlock(&l_inconst->lock);
	LIST_INSERT_HEAD(&l_inconst->cache[lcore_index]->h, local_entry, next);
	__atomic_add_fetch(&l_inconst->count, 1, __ATOMIC_RELAXED);
	DRV_LOG(DEBUG, "mlx5 list %s entry %p new: %u.", l_const->name,
		(void *)entry, entry->ref_cnt);
	return local_entry;
}

struct mlx5_list_entry *
mlx5_list_register(struct mlx5_list *list, void *ctx)
{
	struct mlx5_list_entry *entry;
	int lcore_index = rte_lcore_index(rte_lcore_id());

	if (unlikely(lcore_index == -1)) {
		lcore_index = MLX5_LIST_NLCORE;
		rte_spinlock_lock(&list->l_const.lcore_lock);
	}
	entry =  _mlx5_list_register(&list->l_inconst, &list->l_const, ctx,
				     lcore_index);
	if (unlikely(lcore_index == MLX5_LIST_NLCORE))
		rte_spinlock_unlock(&list->l_const.lcore_lock);
	return entry;
}

static inline int
_mlx5_list_unregister(struct mlx5_list_inconst *l_inconst,
		      struct mlx5_list_const *l_const,
		      struct mlx5_list_entry *entry,
		      int lcore_idx)
{
	struct mlx5_list_entry *gentry = entry->gentry;

	if (__atomic_sub_fetch(&entry->ref_cnt, 1, __ATOMIC_RELAXED) != 0)
		return 1;
	if (entry->lcore_idx == (uint32_t)lcore_idx) {
		LIST_REMOVE(entry, next);
		if (l_const->lcores_share)
			l_const->cb_clone_free(l_const->ctx, entry);
		else
			l_const->cb_remove(l_const->ctx, entry);
	} else if (likely(lcore_idx != -1)) {
		__atomic_add_fetch(&l_inconst->cache[entry->lcore_idx]->inv_cnt,
				   1, __ATOMIC_RELAXED);
	} else {
		return 0;
	}
	if (!l_const->lcores_share) {
		__atomic_sub_fetch(&l_inconst->count, 1, __ATOMIC_RELAXED);
		DRV_LOG(DEBUG, "mlx5 list %s entry %p removed.",
			l_const->name, (void *)entry);
		return 0;
	}
	if (__atomic_sub_fetch(&gentry->ref_cnt, 1, __ATOMIC_RELAXED) != 0)
		return 1;
	rte_rwlock_write_lock(&l_inconst->lock);
	if (likely(gentry->ref_cnt == 0)) {
		LIST_REMOVE(gentry, next);
		rte_rwlock_write_unlock(&l_inconst->lock);
		l_const->cb_remove(l_const->ctx, gentry);
		__atomic_sub_fetch(&l_inconst->count, 1, __ATOMIC_RELAXED);
		DRV_LOG(DEBUG, "mlx5 list %s entry %p removed.",
			l_const->name, (void *)gentry);
		return 0;
	}
	rte_rwlock_write_unlock(&l_inconst->lock);
	return 1;
}

int
mlx5_list_unregister(struct mlx5_list *list,
		      struct mlx5_list_entry *entry)
{
	int ret;
	int lcore_index = rte_lcore_index(rte_lcore_id());

	if (unlikely(lcore_index == -1)) {
		lcore_index = MLX5_LIST_NLCORE;
		rte_spinlock_lock(&list->l_const.lcore_lock);
	}
	ret = _mlx5_list_unregister(&list->l_inconst, &list->l_const, entry,
				    lcore_index);
	if (unlikely(lcore_index == MLX5_LIST_NLCORE))
		rte_spinlock_unlock(&list->l_const.lcore_lock);
	return ret;

}

static void
mlx5_list_uninit(struct mlx5_list_inconst *l_inconst,
		 struct mlx5_list_const *l_const)
{
	struct mlx5_list_entry *entry;
	int i;

	MLX5_ASSERT(l_inconst);
	for (i = 0; i < MLX5_LIST_MAX; i++) {
		if (!l_inconst->cache[i])
			continue;
		while (!LIST_EMPTY(&l_inconst->cache[i]->h)) {
			entry = LIST_FIRST(&l_inconst->cache[i]->h);
			LIST_REMOVE(entry, next);
			if (i == MLX5_LIST_GLOBAL) {
				l_const->cb_remove(l_const->ctx, entry);
				DRV_LOG(DEBUG, "mlx5 list %s entry %p "
					"destroyed.", l_const->name,
					(void *)entry);
			} else {
				l_const->cb_clone_free(l_const->ctx, entry);
			}
		}
		if (i != MLX5_LIST_GLOBAL)
			mlx5_free(l_inconst->cache[i]);
	}
}

void
mlx5_list_destroy(struct mlx5_list *list)
{
	mlx5_list_uninit(&list->l_inconst, &list->l_const);
	mlx5_free(list);
}

uint32_t
mlx5_list_get_entry_num(struct mlx5_list *list)
{
	MLX5_ASSERT(list);
	return __atomic_load_n(&list->l_inconst.count, __ATOMIC_RELAXED);
}

/********************* Hash List **********************/

struct mlx5_hlist *
mlx5_hlist_create(const char *name, uint32_t size, bool direct_key,
		  bool lcores_share, void *ctx, mlx5_list_create_cb cb_create,
		  mlx5_list_match_cb cb_match,
		  mlx5_list_remove_cb cb_remove,
		  mlx5_list_clone_cb cb_clone,
		  mlx5_list_clone_free_cb cb_clone_free)
{
	struct mlx5_hlist *h;
	struct mlx5_list_cache *gc;
	uint32_t act_size;
	uint32_t alloc_size;
	uint32_t i;

	if (!cb_match || !cb_create || !cb_remove || !cb_clone ||
	    !cb_clone_free) {
		rte_errno = EINVAL;
		return NULL;
	}
	/* Align to the next power of 2, 32bits integer is enough now. */
	if (!rte_is_power_of_2(size)) {
		act_size = rte_align32pow2(size);
		DRV_LOG(WARNING, "Size 0x%" PRIX32 " is not power of 2, will "
			"be aligned to 0x%" PRIX32 ".", size, act_size);
	} else {
		act_size = size;
	}
	alloc_size = sizeof(struct mlx5_hlist) +
		     sizeof(struct mlx5_hlist_bucket) * act_size;
	if (lcores_share)
		alloc_size += sizeof(struct mlx5_list_cache)  * act_size;
	/* Using zmalloc, then no need to initialize the heads. */
	h = mlx5_malloc(MLX5_MEM_ZERO, alloc_size, RTE_CACHE_LINE_SIZE,
			SOCKET_ID_ANY);
	if (!h) {
		DRV_LOG(ERR, "No memory for hash list %s creation",
			name ? name : "None");
		return NULL;
	}
	if (name)
		snprintf(h->l_const.name, sizeof(h->l_const.name), "%s", name);
	h->l_const.ctx = ctx;
	h->l_const.lcores_share = lcores_share;
	h->l_const.cb_create = cb_create;
	h->l_const.cb_match = cb_match;
	h->l_const.cb_remove = cb_remove;
	h->l_const.cb_clone = cb_clone;
	h->l_const.cb_clone_free = cb_clone_free;
	rte_spinlock_init(&h->l_const.lcore_lock);
	h->mask = act_size - 1;
	h->direct_key = direct_key;
	gc = (struct mlx5_list_cache *)&h->buckets[act_size];
	for (i = 0; i < act_size; i++) {
		if (mlx5_list_init(&h->buckets[i].l, &h->l_const,
		    lcores_share ? &gc[i] : NULL) != 0) {
			mlx5_free(h);
			return NULL;
		}
	}
	DRV_LOG(DEBUG, "Hash list %s with size 0x%" PRIX32 " was created.",
		name, act_size);
	return h;
}


struct mlx5_list_entry *
mlx5_hlist_lookup(struct mlx5_hlist *h, uint64_t key, void *ctx)
{
	uint32_t idx;

	if (h->direct_key)
		idx = (uint32_t)(key & h->mask);
	else
		idx = rte_hash_crc_8byte(key, 0) & h->mask;
	return _mlx5_list_lookup(&h->buckets[idx].l, &h->l_const, ctx);
}

struct mlx5_list_entry*
mlx5_hlist_register(struct mlx5_hlist *h, uint64_t key, void *ctx)
{
	uint32_t idx;
	struct mlx5_list_entry *entry;
	int lcore_index = rte_lcore_index(rte_lcore_id());

	if (h->direct_key)
		idx = (uint32_t)(key & h->mask);
	else
		idx = rte_hash_crc_8byte(key, 0) & h->mask;
	if (unlikely(lcore_index == -1)) {
		lcore_index = MLX5_LIST_NLCORE;
		rte_spinlock_lock(&h->l_const.lcore_lock);
	}
	entry = _mlx5_list_register(&h->buckets[idx].l, &h->l_const, ctx,
				    lcore_index);
	if (likely(entry)) {
		if (h->l_const.lcores_share)
			entry->gentry->bucket_idx = idx;
		else
			entry->bucket_idx = idx;
	}
	if (unlikely(lcore_index == MLX5_LIST_NLCORE))
		rte_spinlock_unlock(&h->l_const.lcore_lock);
	return entry;
}

int
mlx5_hlist_unregister(struct mlx5_hlist *h, struct mlx5_list_entry *entry)
{
	int lcore_index = rte_lcore_index(rte_lcore_id());
	int ret;
	uint32_t idx = h->l_const.lcores_share ? entry->gentry->bucket_idx :
							      entry->bucket_idx;
	if (unlikely(lcore_index == -1)) {
		lcore_index = MLX5_LIST_NLCORE;
		rte_spinlock_lock(&h->l_const.lcore_lock);
	}
	ret = _mlx5_list_unregister(&h->buckets[idx].l, &h->l_const, entry,
				    lcore_index);
	if (unlikely(lcore_index == MLX5_LIST_NLCORE))
		rte_spinlock_unlock(&h->l_const.lcore_lock);
	return ret;
}

void
mlx5_hlist_destroy(struct mlx5_hlist *h)
{
	uint32_t i;

	for (i = 0; i <= h->mask; i++)
		mlx5_list_uninit(&h->buckets[i].l, &h->l_const);
	mlx5_free(h);
}
