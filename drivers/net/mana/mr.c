/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2022 Microsoft Corporation
 */

#include <rte_malloc.h>
#include <ethdev_driver.h>
#include <rte_eal_paging.h>

#include <infiniband/verbs.h>

#include "mana.h"

struct mana_range {
	uintptr_t	start;
	uintptr_t	end;
	uint32_t	len;
};

void
mana_mempool_chunk_cb(struct rte_mempool *mp __rte_unused, void *opaque,
		      struct rte_mempool_memhdr *memhdr, unsigned int idx)
{
	struct mana_range *ranges = opaque;
	struct mana_range *range = &ranges[idx];
	uint64_t page_size = rte_mem_page_size();

	range->start = RTE_ALIGN_FLOOR((uintptr_t)memhdr->addr, page_size);
	range->end = RTE_ALIGN_CEIL((uintptr_t)memhdr->addr + memhdr->len,
				    page_size);
	range->len = range->end - range->start;
}

/*
 * Register all memory regions from pool.
 */
int
mana_new_pmd_mr(struct mana_mr_btree *local_tree, struct mana_priv *priv,
		struct rte_mempool *pool)
{
	struct ibv_mr *ibv_mr;
	struct mana_range ranges[pool->nb_mem_chunks];
	uint32_t i;
	struct mana_mr_cache *mr;
	int ret;

	rte_mempool_mem_iter(pool, mana_mempool_chunk_cb, ranges);

	for (i = 0; i < pool->nb_mem_chunks; i++) {
		if (ranges[i].len > priv->max_mr_size) {
			DP_LOG(ERR, "memory chunk size %u exceeding max MR",
			       ranges[i].len);
			return -ENOMEM;
		}

		DP_LOG(DEBUG,
		       "registering memory chunk start 0x%" PRIx64 " len %u",
		       ranges[i].start, ranges[i].len);

		if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
			/* Send a message to the primary to do MR */
			ret = mana_mp_req_mr_create(priv, ranges[i].start,
						    ranges[i].len);
			if (ret) {
				DP_LOG(ERR,
				       "MR failed start 0x%" PRIx64 " len %u",
				       ranges[i].start, ranges[i].len);
				return ret;
			}
			continue;
		}

		ibv_mr = ibv_reg_mr(priv->ib_pd, (void *)ranges[i].start,
				    ranges[i].len, IBV_ACCESS_LOCAL_WRITE);
		if (ibv_mr) {
			DP_LOG(DEBUG, "MR lkey %u addr %p len %" PRIu64,
			       ibv_mr->lkey, ibv_mr->addr, ibv_mr->length);

			mr = rte_calloc("MANA MR", 1, sizeof(*mr), 0);
			mr->lkey = ibv_mr->lkey;
			mr->addr = (uintptr_t)ibv_mr->addr;
			mr->len = ibv_mr->length;
			mr->verb_obj = ibv_mr;

			rte_spinlock_lock(&priv->mr_btree_lock);
			ret = mana_mr_btree_insert(&priv->mr_btree, mr);
			rte_spinlock_unlock(&priv->mr_btree_lock);
			if (ret) {
				ibv_dereg_mr(ibv_mr);
				DP_LOG(ERR, "Failed to add to global MR btree");
				return ret;
			}

			ret = mana_mr_btree_insert(local_tree, mr);
			if (ret) {
				/* Don't need to clean up MR as it's already
				 * in the global tree
				 */
				DP_LOG(ERR, "Failed to add to local MR btree");
				return ret;
			}
		} else {
			DP_LOG(ERR, "MR failed at 0x%" PRIx64 " len %u",
			       ranges[i].start, ranges[i].len);
			return -errno;
		}
	}
	return 0;
}

/*
 * Deregister a MR.
 */
void
mana_del_pmd_mr(struct mana_mr_cache *mr)
{
	int ret;
	struct ibv_mr *ibv_mr = (struct ibv_mr *)mr->verb_obj;

	ret = ibv_dereg_mr(ibv_mr);
	if (ret)
		DP_LOG(ERR, "dereg MR failed ret %d", ret);
}

/*
 * Find a MR from cache. If not found, register a new MR.
 */
struct mana_mr_cache *
mana_find_pmd_mr(struct mana_mr_btree *local_mr_btree, struct mana_priv *priv,
		 struct rte_mbuf *mbuf)
{
	struct rte_mempool *pool = mbuf->pool;
	int ret, second_try = 0;
	struct mana_mr_cache *mr;
	uint16_t idx;

	DP_LOG(DEBUG, "finding mr for mbuf addr %p len %d",
	       mbuf->buf_addr, mbuf->buf_len);

try_again:
	/* First try to find the MR in local queue tree */
	mr = mana_mr_btree_lookup(local_mr_btree, &idx,
				  (uintptr_t)mbuf->buf_addr, mbuf->buf_len);
	if (mr) {
		DP_LOG(DEBUG, "Local mr lkey %u addr 0x%" PRIx64 " len %" PRIu64,
		       mr->lkey, mr->addr, mr->len);
		return mr;
	}

	/* If not found, try to find the MR in global tree */
	rte_spinlock_lock(&priv->mr_btree_lock);
	mr = mana_mr_btree_lookup(&priv->mr_btree, &idx,
				  (uintptr_t)mbuf->buf_addr,
				  mbuf->buf_len);
	rte_spinlock_unlock(&priv->mr_btree_lock);

	/* If found in the global tree, add it to the local tree */
	if (mr) {
		ret = mana_mr_btree_insert(local_mr_btree, mr);
		if (ret) {
			DP_LOG(ERR, "Failed to add MR to local tree.");
			return NULL;
		}

		DP_LOG(DEBUG,
		       "Added local MR key %u addr 0x%" PRIx64 " len %" PRIu64,
		       mr->lkey, mr->addr, mr->len);
		return mr;
	}

	if (second_try) {
		DP_LOG(ERR, "Internal error second try failed");
		return NULL;
	}

	ret = mana_new_pmd_mr(local_mr_btree, priv, pool);
	if (ret) {
		DP_LOG(ERR, "Failed to allocate MR ret %d addr %p len %d",
		       ret, mbuf->buf_addr, mbuf->buf_len);
		return NULL;
	}

	second_try = 1;
	goto try_again;
}

void
mana_remove_all_mr(struct mana_priv *priv)
{
	struct mana_mr_btree *bt = &priv->mr_btree;
	struct mana_mr_cache *mr;
	struct ibv_mr *ibv_mr;
	uint16_t i;

	rte_spinlock_lock(&priv->mr_btree_lock);
	/* Start with index 1 as the 1st entry is always NULL */
	for (i = 1; i < bt->len; i++) {
		mr = &bt->table[i];
		ibv_mr = mr->verb_obj;
		ibv_dereg_mr(ibv_mr);
	}
	bt->len = 1;
	rte_spinlock_unlock(&priv->mr_btree_lock);
}

/*
 * Expand the MR cache.
 * MR cache is maintained as a btree and expand on demand.
 */
static int
mana_mr_btree_expand(struct mana_mr_btree *bt, int n)
{
	void *mem;

	mem = rte_realloc_socket(bt->table, n * sizeof(struct mana_mr_cache),
				 0, bt->socket);
	if (!mem) {
		DP_LOG(ERR, "Failed to expand btree size %d", n);
		return -1;
	}

	DP_LOG(ERR, "Expanded btree to size %d", n);
	bt->table = mem;
	bt->size = n;

	return 0;
}

/*
 * Look for a region of memory in MR cache.
 */
struct mana_mr_cache *
mana_mr_btree_lookup(struct mana_mr_btree *bt, uint16_t *idx,
		     uintptr_t addr, size_t len)
{
	struct mana_mr_cache *table;
	uint16_t n;
	uint16_t base = 0;
	int ret;

	n = bt->len;

	/* Try to double the cache if it's full */
	if (n == bt->size) {
		ret = mana_mr_btree_expand(bt, bt->size << 1);
		if (ret)
			return NULL;
	}

	table = bt->table;

	/* Do binary search on addr */
	do {
		uint16_t delta = n >> 1;

		if (addr < table[base + delta].addr) {
			n = delta;
		} else {
			base += delta;
			n -= delta;
		}
	} while (n > 1);

	*idx = base;

	if (addr + len <= table[base].addr + table[base].len)
		return &table[base];

	DP_LOG(DEBUG,
	       "addr 0x%" PRIx64 " len %zu idx %u sum 0x%" PRIx64 " not found",
	       addr, len, *idx, addr + len);

	return NULL;
}

int
mana_mr_btree_init(struct mana_mr_btree *bt, int n, int socket)
{
	memset(bt, 0, sizeof(*bt));
	bt->table = rte_calloc_socket("MANA B-tree table",
				      n,
				      sizeof(struct mana_mr_cache),
				      0, socket);
	if (!bt->table) {
		DRV_LOG(ERR, "Failed to allocate B-tree n %d socket %d",
			n, socket);
		return -ENOMEM;
	}

	bt->socket = socket;
	bt->size = n;

	/* First entry must be NULL for binary search to work */
	bt->table[0] = (struct mana_mr_cache) {
		.lkey = UINT32_MAX,
	};
	bt->len = 1;

	DRV_LOG(ERR, "B-tree initialized table %p size %d len %d",
		bt->table, n, bt->len);

	return 0;
}

void
mana_mr_btree_free(struct mana_mr_btree *bt)
{
	rte_free(bt->table);
	memset(bt, 0, sizeof(*bt));
}

int
mana_mr_btree_insert(struct mana_mr_btree *bt, struct mana_mr_cache *entry)
{
	struct mana_mr_cache *table;
	uint16_t idx = 0;
	uint16_t shift;

	if (mana_mr_btree_lookup(bt, &idx, entry->addr, entry->len)) {
		DP_LOG(DEBUG, "Addr 0x%" PRIx64 " len %zu exists in btree",
		       entry->addr, entry->len);
		return 0;
	}

	if (bt->len >= bt->size) {
		bt->overflow = 1;
		return -1;
	}

	table = bt->table;

	idx++;
	shift = (bt->len - idx) * sizeof(struct mana_mr_cache);
	if (shift) {
		DP_LOG(DEBUG, "Moving %u bytes from idx %u to %u",
		       shift, idx, idx + 1);
		memmove(&table[idx + 1], &table[idx], shift);
	}

	table[idx] = *entry;
	bt->len++;

	DP_LOG(DEBUG,
	       "Inserted MR b-tree table %p idx %d addr 0x%" PRIx64 " len %zu",
	       table, idx, entry->addr, entry->len);

	return 0;
}
