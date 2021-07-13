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

struct mlx5_list *
mlx5_list_create(const char *name, void *ctx, bool lcores_share,
		 mlx5_list_create_cb cb_create,
		 mlx5_list_match_cb cb_match,
		 mlx5_list_remove_cb cb_remove,
		 mlx5_list_clone_cb cb_clone,
		 mlx5_list_clone_free_cb cb_clone_free)
{
	struct mlx5_list *list;
	int i;

	if (!cb_match || !cb_create || !cb_remove || !cb_clone ||
	    !cb_clone_free) {
		rte_errno = EINVAL;
		return NULL;
	}
	list = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*list), 0, SOCKET_ID_ANY);
	if (!list)
		return NULL;
	if (name)
		snprintf(list->name, sizeof(list->name), "%s", name);
	list->ctx = ctx;
	list->lcores_share = lcores_share;
	list->cb_create = cb_create;
	list->cb_match = cb_match;
	list->cb_remove = cb_remove;
	list->cb_clone = cb_clone;
	list->cb_clone_free = cb_clone_free;
	rte_rwlock_init(&list->lock);
	DRV_LOG(DEBUG, "mlx5 list %s initialized.", list->name);
	for (i = 0; i <= RTE_MAX_LCORE; i++)
		LIST_INIT(&list->cache[i].h);
	return list;
}

static struct mlx5_list_entry *
__list_lookup(struct mlx5_list *list, int lcore_index, void *ctx, bool reuse)
{
	struct mlx5_list_entry *entry = LIST_FIRST(&list->cache[lcore_index].h);
	uint32_t ret;

	while (entry != NULL) {
		if (list->cb_match(list, entry, ctx) == 0) {
			if (reuse) {
				ret = __atomic_add_fetch(&entry->ref_cnt, 1,
							 __ATOMIC_RELAXED) - 1;
				DRV_LOG(DEBUG, "mlx5 list %s entry %p ref: %u.",
					list->name, (void *)entry,
					entry->ref_cnt);
			} else if (lcore_index < RTE_MAX_LCORE) {
				ret = __atomic_load_n(&entry->ref_cnt,
						      __ATOMIC_RELAXED);
			}
			if (likely(ret != 0 || lcore_index == RTE_MAX_LCORE))
				return entry;
			if (reuse && ret == 0)
				entry->ref_cnt--; /* Invalid entry. */
		}
		entry = LIST_NEXT(entry, next);
	}
	return NULL;
}

struct mlx5_list_entry *
mlx5_list_lookup(struct mlx5_list *list, void *ctx)
{
	struct mlx5_list_entry *entry = NULL;
	int i;

	rte_rwlock_read_lock(&list->lock);
	for (i = 0; i < RTE_MAX_LCORE; i++) {
		entry = __list_lookup(list, i, ctx, false);
		if (entry)
			break;
	}
	rte_rwlock_read_unlock(&list->lock);
	return entry;
}

static struct mlx5_list_entry *
mlx5_list_cache_insert(struct mlx5_list *list, int lcore_index,
		       struct mlx5_list_entry *gentry, void *ctx)
{
	struct mlx5_list_entry *lentry = list->cb_clone(list, gentry, ctx);

	if (unlikely(!lentry))
		return NULL;
	lentry->ref_cnt = 1u;
	lentry->gentry = gentry;
	lentry->lcore_idx = (uint32_t)lcore_index;
	LIST_INSERT_HEAD(&list->cache[lcore_index].h, lentry, next);
	return lentry;
}

static void
__list_cache_clean(struct mlx5_list *list, int lcore_index)
{
	struct mlx5_list_cache *c = &list->cache[lcore_index];
	struct mlx5_list_entry *entry = LIST_FIRST(&c->h);
	uint32_t inv_cnt = __atomic_exchange_n(&c->inv_cnt, 0,
					       __ATOMIC_RELAXED);

	while (inv_cnt != 0 && entry != NULL) {
		struct mlx5_list_entry *nentry = LIST_NEXT(entry, next);

		if (__atomic_load_n(&entry->ref_cnt, __ATOMIC_RELAXED) == 0) {
			LIST_REMOVE(entry, next);
			if (list->lcores_share)
				list->cb_clone_free(list, entry);
			else
				list->cb_remove(list, entry);
			inv_cnt--;
		}
		entry = nentry;
	}
}

struct mlx5_list_entry *
mlx5_list_register(struct mlx5_list *list, void *ctx)
{
	struct mlx5_list_entry *entry = NULL, *local_entry;
	volatile uint32_t prev_gen_cnt = 0;
	int lcore_index = rte_lcore_index(rte_lcore_id());

	MLX5_ASSERT(list);
	MLX5_ASSERT(lcore_index < RTE_MAX_LCORE);
	if (unlikely(lcore_index == -1)) {
		rte_errno = ENOTSUP;
		return NULL;
	}
	/* 0. Free entries that was invalidated by other lcores. */
	__list_cache_clean(list, lcore_index);
	/* 1. Lookup in local cache. */
	local_entry = __list_lookup(list, lcore_index, ctx, true);
	if (local_entry)
		return local_entry;
	if (list->lcores_share) {
		/* 2. Lookup with read lock on global list, reuse if found. */
		rte_rwlock_read_lock(&list->lock);
		entry = __list_lookup(list, RTE_MAX_LCORE, ctx, true);
		if (likely(entry)) {
			rte_rwlock_read_unlock(&list->lock);
			return mlx5_list_cache_insert(list, lcore_index, entry,
						      ctx);
		}
		prev_gen_cnt = list->gen_cnt;
		rte_rwlock_read_unlock(&list->lock);
	}
	/* 3. Prepare new entry for global list and for cache. */
	entry = list->cb_create(list, entry, ctx);
	if (unlikely(!entry))
		return NULL;
	entry->ref_cnt = 1u;
	if (!list->lcores_share) {
		entry->lcore_idx = (uint32_t)lcore_index;
		LIST_INSERT_HEAD(&list->cache[lcore_index].h, entry, next);
		__atomic_add_fetch(&list->count, 1, __ATOMIC_RELAXED);
		DRV_LOG(DEBUG, "MLX5 list %s c%d entry %p new: %u.",
			list->name, lcore_index, (void *)entry, entry->ref_cnt);
		return entry;
	}
	local_entry = list->cb_clone(list, entry, ctx);
	if (unlikely(!local_entry)) {
		list->cb_remove(list, entry);
		return NULL;
	}
	local_entry->ref_cnt = 1u;
	local_entry->gentry = entry;
	local_entry->lcore_idx = (uint32_t)lcore_index;
	rte_rwlock_write_lock(&list->lock);
	/* 4. Make sure the same entry was not created before the write lock. */
	if (unlikely(prev_gen_cnt != list->gen_cnt)) {
		struct mlx5_list_entry *oentry = __list_lookup(list,
							       RTE_MAX_LCORE,
							       ctx, true);

		if (unlikely(oentry)) {
			/* 4.5. Found real race!!, reuse the old entry. */
			rte_rwlock_write_unlock(&list->lock);
			list->cb_remove(list, entry);
			list->cb_clone_free(list, local_entry);
			return mlx5_list_cache_insert(list, lcore_index, oentry,
						      ctx);
		}
	}
	/* 5. Update lists. */
	LIST_INSERT_HEAD(&list->cache[RTE_MAX_LCORE].h, entry, next);
	list->gen_cnt++;
	rte_rwlock_write_unlock(&list->lock);
	LIST_INSERT_HEAD(&list->cache[lcore_index].h, local_entry, next);
	__atomic_add_fetch(&list->count, 1, __ATOMIC_RELAXED);
	DRV_LOG(DEBUG, "mlx5 list %s entry %p new: %u.", list->name,
		(void *)entry, entry->ref_cnt);
	return local_entry;
}

int
mlx5_list_unregister(struct mlx5_list *list,
		      struct mlx5_list_entry *entry)
{
	struct mlx5_list_entry *gentry = entry->gentry;
	int lcore_idx;

	if (__atomic_sub_fetch(&entry->ref_cnt, 1, __ATOMIC_RELAXED) != 0)
		return 1;
	lcore_idx = rte_lcore_index(rte_lcore_id());
	MLX5_ASSERT(lcore_idx < RTE_MAX_LCORE);
	if (entry->lcore_idx == (uint32_t)lcore_idx) {
		LIST_REMOVE(entry, next);
		if (list->lcores_share)
			list->cb_clone_free(list, entry);
		else
			list->cb_remove(list, entry);
	} else if (likely(lcore_idx != -1)) {
		__atomic_add_fetch(&list->cache[entry->lcore_idx].inv_cnt, 1,
				   __ATOMIC_RELAXED);
	} else {
		return 0;
	}
	if (!list->lcores_share) {
		__atomic_sub_fetch(&list->count, 1, __ATOMIC_RELAXED);
		DRV_LOG(DEBUG, "mlx5 list %s entry %p removed.",
			list->name, (void *)entry);
		return 0;
	}
	if (__atomic_sub_fetch(&gentry->ref_cnt, 1, __ATOMIC_RELAXED) != 0)
		return 1;
	rte_rwlock_write_lock(&list->lock);
	if (likely(gentry->ref_cnt == 0)) {
		LIST_REMOVE(gentry, next);
		rte_rwlock_write_unlock(&list->lock);
		list->cb_remove(list, gentry);
		__atomic_sub_fetch(&list->count, 1, __ATOMIC_RELAXED);
		DRV_LOG(DEBUG, "mlx5 list %s entry %p removed.",
			list->name, (void *)gentry);
		return 0;
	}
	rte_rwlock_write_unlock(&list->lock);
	return 1;
}

void
mlx5_list_destroy(struct mlx5_list *list)
{
	struct mlx5_list_entry *entry;
	int i;

	MLX5_ASSERT(list);
	for (i = 0; i <= RTE_MAX_LCORE; i++) {
		while (!LIST_EMPTY(&list->cache[i].h)) {
			entry = LIST_FIRST(&list->cache[i].h);
			LIST_REMOVE(entry, next);
			if (i == RTE_MAX_LCORE) {
				list->cb_remove(list, entry);
				DRV_LOG(DEBUG, "mlx5 list %s entry %p "
					"destroyed.", list->name,
					(void *)entry);
			} else {
				list->cb_clone_free(list, entry);
			}
		}
	}
	mlx5_free(list);
}

uint32_t
mlx5_list_get_entry_num(struct mlx5_list *list)
{
	MLX5_ASSERT(list);
	return __atomic_load_n(&list->count, __ATOMIC_RELAXED);
}

/********************* Hash List **********************/

static struct mlx5_hlist_entry *
mlx5_hlist_default_create_cb(struct mlx5_hlist *h, uint64_t key __rte_unused,
			     void *ctx __rte_unused)
{
	return mlx5_malloc(MLX5_MEM_ZERO, h->entry_sz, 0, SOCKET_ID_ANY);
}

static void
mlx5_hlist_default_remove_cb(struct mlx5_hlist *h __rte_unused,
			     struct mlx5_hlist_entry *entry)
{
	mlx5_free(entry);
}

struct mlx5_hlist *
mlx5_hlist_create(const char *name, uint32_t size, uint32_t entry_size,
		  uint32_t flags, mlx5_hlist_create_cb cb_create,
		  mlx5_hlist_match_cb cb_match, mlx5_hlist_remove_cb cb_remove)
{
	struct mlx5_hlist *h;
	uint32_t act_size;
	uint32_t alloc_size;
	uint32_t i;

	if (!size || !cb_match || (!cb_create ^ !cb_remove))
		return NULL;
	/* Align to the next power of 2, 32bits integer is enough now. */
	if (!rte_is_power_of_2(size)) {
		act_size = rte_align32pow2(size);
		DRV_LOG(DEBUG, "Size 0x%" PRIX32 " is not power of 2, "
			"will be aligned to 0x%" PRIX32 ".", size, act_size);
	} else {
		act_size = size;
	}
	alloc_size = sizeof(struct mlx5_hlist) +
		     sizeof(struct mlx5_hlist_bucket) * act_size;
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
	h->entry_sz = entry_size;
	h->direct_key = !!(flags & MLX5_HLIST_DIRECT_KEY);
	h->write_most = !!(flags & MLX5_HLIST_WRITE_MOST);
	h->cb_create = cb_create ? cb_create : mlx5_hlist_default_create_cb;
	h->cb_match = cb_match;
	h->cb_remove = cb_remove ? cb_remove : mlx5_hlist_default_remove_cb;
	for (i = 0; i < act_size; i++)
		rte_rwlock_init(&h->buckets[i].lock);
	DRV_LOG(DEBUG, "Hash list with %s size 0x%" PRIX32 " is created.",
		h->name, act_size);
	return h;
}

static struct mlx5_hlist_entry *
__hlist_lookup(struct mlx5_hlist *h, uint64_t key, uint32_t idx,
	       void *ctx, bool reuse)
{
	struct mlx5_hlist_head *first;
	struct mlx5_hlist_entry *node;

	MLX5_ASSERT(h);
	first = &h->buckets[idx].head;
	LIST_FOREACH(node, first, next) {
		if (!h->cb_match(h, node, key, ctx)) {
			if (reuse) {
				__atomic_add_fetch(&node->ref_cnt, 1,
						   __ATOMIC_RELAXED);
				DRV_LOG(DEBUG, "Hash list %s entry %p "
					"reuse: %u.",
					h->name, (void *)node, node->ref_cnt);
			}
			break;
		}
	}
	return node;
}

static struct mlx5_hlist_entry *
hlist_lookup(struct mlx5_hlist *h, uint64_t key, uint32_t idx,
	     void *ctx, bool reuse)
{
	struct mlx5_hlist_entry *node;

	MLX5_ASSERT(h);
	rte_rwlock_read_lock(&h->buckets[idx].lock);
	node = __hlist_lookup(h, key, idx, ctx, reuse);
	rte_rwlock_read_unlock(&h->buckets[idx].lock);
	return node;
}

struct mlx5_hlist_entry *
mlx5_hlist_lookup(struct mlx5_hlist *h, uint64_t key, void *ctx)
{
	uint32_t idx;

	if (h->direct_key)
		idx = (uint32_t)(key & h->mask);
	else
		idx = rte_hash_crc_8byte(key, 0) & h->mask;
	return hlist_lookup(h, key, idx, ctx, false);
}

struct mlx5_hlist_entry*
mlx5_hlist_register(struct mlx5_hlist *h, uint64_t key, void *ctx)
{
	uint32_t idx;
	struct mlx5_hlist_head *first;
	struct mlx5_hlist_bucket *b;
	struct mlx5_hlist_entry *entry;
	uint32_t prev_gen_cnt = 0;

	if (h->direct_key)
		idx = (uint32_t)(key & h->mask);
	else
		idx = rte_hash_crc_8byte(key, 0) & h->mask;
	MLX5_ASSERT(h);
	b = &h->buckets[idx];
	/* Use write lock directly for write-most list. */
	if (!h->write_most) {
		prev_gen_cnt = __atomic_load_n(&b->gen_cnt, __ATOMIC_ACQUIRE);
		entry = hlist_lookup(h, key, idx, ctx, true);
		if (entry)
			return entry;
	}
	rte_rwlock_write_lock(&b->lock);
	/* Check if the list changed by other threads. */
	if (h->write_most ||
	    prev_gen_cnt != __atomic_load_n(&b->gen_cnt, __ATOMIC_ACQUIRE)) {
		entry = __hlist_lookup(h, key, idx, ctx, true);
		if (entry)
			goto done;
	}
	first = &b->head;
	entry = h->cb_create(h, key, ctx);
	if (!entry) {
		rte_errno = ENOMEM;
		DRV_LOG(DEBUG, "Can't allocate hash list %s entry.", h->name);
		goto done;
	}
	entry->idx = idx;
	entry->ref_cnt = 1;
	LIST_INSERT_HEAD(first, entry, next);
	__atomic_add_fetch(&b->gen_cnt, 1, __ATOMIC_ACQ_REL);
	DRV_LOG(DEBUG, "Hash list %s entry %p new: %u.",
		h->name, (void *)entry, entry->ref_cnt);
done:
	rte_rwlock_write_unlock(&b->lock);
	return entry;
}

int
mlx5_hlist_unregister(struct mlx5_hlist *h, struct mlx5_hlist_entry *entry)
{
	uint32_t idx = entry->idx;

	rte_rwlock_write_lock(&h->buckets[idx].lock);
	MLX5_ASSERT(entry && entry->ref_cnt && entry->next.le_prev);
	DRV_LOG(DEBUG, "Hash list %s entry %p deref: %u.",
		h->name, (void *)entry, entry->ref_cnt);
	if (--entry->ref_cnt) {
		rte_rwlock_write_unlock(&h->buckets[idx].lock);
		return 1;
	}
	LIST_REMOVE(entry, next);
	/* Set to NULL to get rid of removing action for more than once. */
	entry->next.le_prev = NULL;
	h->cb_remove(h, entry);
	rte_rwlock_write_unlock(&h->buckets[idx].lock);
	DRV_LOG(DEBUG, "Hash list %s entry %p removed.",
		h->name, (void *)entry);
	return 0;
}

void
mlx5_hlist_destroy(struct mlx5_hlist *h)
{
	uint32_t idx;
	struct mlx5_hlist_entry *entry;

	MLX5_ASSERT(h);
	for (idx = 0; idx < h->table_sz; ++idx) {
		/* No LIST_FOREACH_SAFE, using while instead. */
		while (!LIST_EMPTY(&h->buckets[idx].head)) {
			entry = LIST_FIRST(&h->buckets[idx].head);
			LIST_REMOVE(entry, next);
			/*
			 * The owner of whole element which contains data entry
			 * is the user, so it's the user's duty to do the clean
			 * up and the free work because someone may not put the
			 * hlist entry at the beginning(suggested to locate at
			 * the beginning). Or else the default free function
			 * will be used.
			 */
			h->cb_remove(h, entry);
		}
	}
	mlx5_free(h);
}
