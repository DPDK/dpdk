/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019 Mellanox Technologies, Ltd
 */

#include <rte_malloc.h>
#include <rte_hash_crc.h>
#include <rte_errno.h>

#include <mlx5_malloc.h>

#include "mlx5_common_utils.h"
#include "mlx5_common_log.h"

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
