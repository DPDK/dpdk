/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2017-2018 Solarflare Communications Inc.
 * All rights reserved.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <rte_errno.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_malloc.h>

/*
 * The general idea of the bucket mempool driver is as follows.
 * We keep track of physically contiguous groups (buckets) of objects
 * of a certain size. Every such a group has a counter that is
 * incremented every time an object from that group is enqueued.
 * Until the bucket is full, no objects from it are eligible for allocation.
 * If a request is made to dequeue a multiply of bucket size, it is
 * satisfied by returning the whole buckets, instead of separate objects.
 */


struct bucket_header {
	unsigned int lcore_id;
	uint8_t fill_cnt;
};

struct bucket_stack {
	unsigned int top;
	unsigned int limit;
	void *objects[];
};

struct bucket_data {
	unsigned int header_size;
	unsigned int total_elt_size;
	unsigned int obj_per_bucket;
	unsigned int bucket_stack_thresh;
	uintptr_t bucket_page_mask;
	struct rte_ring *shared_bucket_ring;
	struct bucket_stack *buckets[RTE_MAX_LCORE];
	/*
	 * Multi-producer single-consumer ring to hold objects that are
	 * returned to the mempool at a different lcore than initially
	 * dequeued
	 */
	struct rte_ring *adoption_buffer_rings[RTE_MAX_LCORE];
	struct rte_ring *shared_orphan_ring;
	struct rte_mempool *pool;
	unsigned int bucket_mem_size;
	void *lcore_callback_handle;
};

static struct bucket_stack *
bucket_stack_create(const struct rte_mempool *mp, unsigned int n_elts)
{
	struct bucket_stack *stack;

	stack = rte_zmalloc_socket("bucket_stack",
				   sizeof(struct bucket_stack) +
				   n_elts * sizeof(void *),
				   RTE_CACHE_LINE_SIZE,
				   mp->socket_id);
	if (stack == NULL)
		return NULL;
	stack->limit = n_elts;
	stack->top = 0;

	return stack;
}

static void
bucket_stack_push(struct bucket_stack *stack, void *obj)
{
	RTE_ASSERT(stack->top < stack->limit);
	stack->objects[stack->top++] = obj;
}

static void *
bucket_stack_pop_unsafe(struct bucket_stack *stack)
{
	RTE_ASSERT(stack->top > 0);
	return stack->objects[--stack->top];
}

static void *
bucket_stack_pop(struct bucket_stack *stack)
{
	if (stack->top == 0)
		return NULL;
	return bucket_stack_pop_unsafe(stack);
}

static int
bucket_enqueue_single(struct bucket_data *bd, void *obj)
{
	int rc = 0;
	uintptr_t addr = (uintptr_t)obj;
	struct bucket_header *hdr;
	unsigned int lcore_id = rte_lcore_id();

	addr &= bd->bucket_page_mask;
	hdr = (struct bucket_header *)addr;

	if (likely(hdr->lcore_id == lcore_id)) {
		if (hdr->fill_cnt < bd->obj_per_bucket - 1) {
			hdr->fill_cnt++;
		} else {
			hdr->fill_cnt = 0;
			/* Stack is big enough to put all buckets */
			bucket_stack_push(bd->buckets[lcore_id], hdr);
		}
	} else if (hdr->lcore_id != LCORE_ID_ANY) {
		struct rte_ring *adopt_ring =
			bd->adoption_buffer_rings[hdr->lcore_id];

		rc = rte_ring_enqueue(adopt_ring, obj);
		/* Ring is big enough to put all objects */
		RTE_ASSERT(rc == 0);
	} else if (hdr->fill_cnt < bd->obj_per_bucket - 1) {
		hdr->fill_cnt++;
	} else {
		hdr->fill_cnt = 0;
		rc = rte_ring_enqueue(bd->shared_bucket_ring, hdr);
		/* Ring is big enough to put all buckets */
		RTE_ASSERT(rc == 0);
	}

	return rc;
}

static int
bucket_enqueue(struct rte_mempool *mp, void * const *obj_table,
	       unsigned int n)
{
	struct bucket_data *bd = mp->pool_data;
	struct bucket_stack *local_stack = bd->buckets[rte_lcore_id()];
	unsigned int i;
	int rc = 0;

	for (i = 0; i < n; i++) {
		rc = bucket_enqueue_single(bd, obj_table[i]);
		RTE_ASSERT(rc == 0);
	}
	if (local_stack->top > bd->bucket_stack_thresh) {
		rte_ring_enqueue_bulk(bd->shared_bucket_ring,
				      &local_stack->objects
				      [bd->bucket_stack_thresh],
				      local_stack->top -
				      bd->bucket_stack_thresh,
				      NULL);
	    local_stack->top = bd->bucket_stack_thresh;
	}
	return rc;
}

static void **
bucket_fill_obj_table(const struct bucket_data *bd, void **pstart,
		      void **obj_table, unsigned int n)
{
	unsigned int i;
	uint8_t *objptr = *pstart;

	for (objptr += bd->header_size, i = 0; i < n;
	     i++, objptr += bd->total_elt_size)
		*obj_table++ = objptr;
	*pstart = objptr;
	return obj_table;
}

static int
bucket_dequeue_orphans(struct bucket_data *bd, void **obj_table,
		       unsigned int n_orphans)
{
	unsigned int i;
	int rc;
	uint8_t *objptr;

	rc = rte_ring_dequeue_bulk(bd->shared_orphan_ring, obj_table,
				   n_orphans, NULL);
	if (unlikely(rc != (int)n_orphans)) {
		struct bucket_header *hdr;

		objptr = bucket_stack_pop(bd->buckets[rte_lcore_id()]);
		hdr = (struct bucket_header *)objptr;

		if (objptr == NULL) {
			rc = rte_ring_dequeue(bd->shared_bucket_ring,
					      (void **)&objptr);
			if (rc != 0) {
				rte_errno = ENOBUFS;
				return -rte_errno;
			}
			hdr = (struct bucket_header *)objptr;
			hdr->lcore_id = rte_lcore_id();
		}
		hdr->fill_cnt = 0;
		bucket_fill_obj_table(bd, (void **)&objptr, obj_table,
				      n_orphans);
		for (i = n_orphans; i < bd->obj_per_bucket; i++,
			     objptr += bd->total_elt_size) {
			rc = rte_ring_enqueue(bd->shared_orphan_ring,
					      objptr);
			if (rc != 0) {
				RTE_ASSERT(0);
				rte_errno = -rc;
				return rc;
			}
		}
	}

	return 0;
}

static int
bucket_dequeue_buckets(struct bucket_data *bd, void **obj_table,
		       unsigned int n_buckets)
{
	struct bucket_stack *cur_stack = bd->buckets[rte_lcore_id()];
	unsigned int n_buckets_from_stack = RTE_MIN(n_buckets, cur_stack->top);
	void **obj_table_base = obj_table;

	n_buckets -= n_buckets_from_stack;
	while (n_buckets_from_stack-- > 0) {
		void *obj = bucket_stack_pop_unsafe(cur_stack);

		obj_table = bucket_fill_obj_table(bd, &obj, obj_table,
						  bd->obj_per_bucket);
	}
	while (n_buckets-- > 0) {
		struct bucket_header *hdr;

		if (unlikely(rte_ring_dequeue(bd->shared_bucket_ring,
					      (void **)&hdr) != 0)) {
			/*
			 * Return the already-dequeued buffers
			 * back to the mempool
			 */
			bucket_enqueue(bd->pool, obj_table_base,
				       obj_table - obj_table_base);
			rte_errno = ENOBUFS;
			return -rte_errno;
		}
		hdr->lcore_id = rte_lcore_id();
		obj_table = bucket_fill_obj_table(bd, (void **)&hdr,
						  obj_table,
						  bd->obj_per_bucket);
	}

	return 0;
}

static int
bucket_adopt_orphans(struct bucket_data *bd)
{
	int rc = 0;
	struct rte_ring *adopt_ring =
		bd->adoption_buffer_rings[rte_lcore_id()];

	if (unlikely(!rte_ring_empty(adopt_ring))) {
		void *orphan;

		while (rte_ring_sc_dequeue(adopt_ring, &orphan) == 0) {
			rc = bucket_enqueue_single(bd, orphan);
			RTE_ASSERT(rc == 0);
		}
	}
	return rc;
}

static int
bucket_dequeue(struct rte_mempool *mp, void **obj_table, unsigned int n)
{
	struct bucket_data *bd = mp->pool_data;
	unsigned int n_buckets = n / bd->obj_per_bucket;
	unsigned int n_orphans = n - n_buckets * bd->obj_per_bucket;
	int rc = 0;

	bucket_adopt_orphans(bd);

	if (unlikely(n_orphans > 0)) {
		rc = bucket_dequeue_orphans(bd, obj_table +
					    (n_buckets * bd->obj_per_bucket),
					    n_orphans);
		if (rc != 0)
			return rc;
	}

	if (likely(n_buckets > 0)) {
		rc = bucket_dequeue_buckets(bd, obj_table, n_buckets);
		if (unlikely(rc != 0) && n_orphans > 0) {
			rte_ring_enqueue_bulk(bd->shared_orphan_ring,
					      obj_table + (n_buckets *
							   bd->obj_per_bucket),
					      n_orphans, NULL);
		}
	}

	return rc;
}

static int
bucket_dequeue_contig_blocks(struct rte_mempool *mp, void **first_obj_table,
			     unsigned int n)
{
	struct bucket_data *bd = mp->pool_data;
	const uint32_t header_size = bd->header_size;
	struct bucket_stack *cur_stack = bd->buckets[rte_lcore_id()];
	unsigned int n_buckets_from_stack = RTE_MIN(n, cur_stack->top);
	struct bucket_header *hdr;
	void **first_objp = first_obj_table;

	bucket_adopt_orphans(bd);

	n -= n_buckets_from_stack;
	while (n_buckets_from_stack-- > 0) {
		hdr = bucket_stack_pop_unsafe(cur_stack);
		*first_objp++ = (uint8_t *)hdr + header_size;
	}
	if (n > 0) {
		if (unlikely(rte_ring_dequeue_bulk(bd->shared_bucket_ring,
						   first_objp, n, NULL) != n)) {
			/* Return the already dequeued buckets */
			while (first_objp-- != first_obj_table) {
				bucket_stack_push(cur_stack,
						  (uint8_t *)*first_objp -
						  header_size);
			}
			rte_errno = ENOBUFS;
			return -rte_errno;
		}
		while (n-- > 0) {
			hdr = (struct bucket_header *)*first_objp;
			hdr->lcore_id = rte_lcore_id();
			*first_objp++ = (uint8_t *)hdr + header_size;
		}
	}

	return 0;
}

struct bucket_count_per_lcore_ctx {
	const struct bucket_data *bd;
	unsigned int count;
};

static int
bucket_count_per_lcore(unsigned int lcore_id, void *arg)
{
	struct bucket_count_per_lcore_ctx *bplc = arg;

	bplc->count += bplc->bd->obj_per_bucket *
		bplc->bd->buckets[lcore_id]->top;
	bplc->count +=
		rte_ring_count(bplc->bd->adoption_buffer_rings[lcore_id]);
	return 0;
}

static void
count_underfilled_buckets(struct rte_mempool *mp,
			  void *opaque,
			  struct rte_mempool_memhdr *memhdr,
			  __rte_unused unsigned int mem_idx)
{
	unsigned int *pcount = opaque;
	const struct bucket_data *bd = mp->pool_data;
	unsigned int bucket_page_sz =
		(unsigned int)(~bd->bucket_page_mask + 1);
	uintptr_t align;
	uint8_t *iter;

	align = (uintptr_t)RTE_PTR_ALIGN_CEIL(memhdr->addr, bucket_page_sz) -
		(uintptr_t)memhdr->addr;

	for (iter = (uint8_t *)memhdr->addr + align;
	     iter < (uint8_t *)memhdr->addr + memhdr->len;
	     iter += bucket_page_sz) {
		struct bucket_header *hdr = (struct bucket_header *)iter;

		*pcount += hdr->fill_cnt;
	}
}

static unsigned int
bucket_get_count(const struct rte_mempool *mp)
{
	struct bucket_count_per_lcore_ctx bplc;

	bplc.bd = mp->pool_data;
	bplc.count = bplc.bd->obj_per_bucket *
		rte_ring_count(bplc.bd->shared_bucket_ring);
	bplc.count += rte_ring_count(bplc.bd->shared_orphan_ring);

	rte_lcore_iterate(bucket_count_per_lcore, &bplc);
	rte_mempool_mem_iter((struct rte_mempool *)(uintptr_t)mp,
			     count_underfilled_buckets, &bplc.count);

	return bplc.count;
}

static int
bucket_init_per_lcore(unsigned int lcore_id, void *arg)
{
	char rg_name[RTE_RING_NAMESIZE];
	struct bucket_data *bd = arg;
	struct rte_mempool *mp;
	int rg_flags;
	int rc;

	mp = bd->pool;
	bd->buckets[lcore_id] = bucket_stack_create(mp,
		mp->size / bd->obj_per_bucket);
	if (bd->buckets[lcore_id] == NULL)
		goto error;

	rc = snprintf(rg_name, sizeof(rg_name), RTE_MEMPOOL_MZ_FORMAT ".a%u",
		mp->name, lcore_id);
	if (rc < 0 || rc >= (int)sizeof(rg_name))
		goto error;

	rg_flags = RING_F_SC_DEQ;
	if (mp->flags & MEMPOOL_F_SP_PUT)
		rg_flags |= RING_F_SP_ENQ;
	bd->adoption_buffer_rings[lcore_id] = rte_ring_create(rg_name,
		rte_align32pow2(mp->size + 1), mp->socket_id, rg_flags);
	if (bd->adoption_buffer_rings[lcore_id] == NULL)
		goto error;

	return 0;
error:
	rte_free(bd->buckets[lcore_id]);
	bd->buckets[lcore_id] = NULL;
	return -1;
}

static void
bucket_uninit_per_lcore(unsigned int lcore_id, void *arg)
{
	struct bucket_data *bd = arg;

	rte_ring_free(bd->adoption_buffer_rings[lcore_id]);
	bd->adoption_buffer_rings[lcore_id] = NULL;
	rte_free(bd->buckets[lcore_id]);
	bd->buckets[lcore_id] = NULL;
}

static int
bucket_alloc(struct rte_mempool *mp)
{
	int rg_flags = 0;
	int rc = 0;
	char rg_name[RTE_RING_NAMESIZE];
	struct bucket_data *bd;
	unsigned int bucket_header_size;
	size_t pg_sz;

	rc = rte_mempool_get_page_size(mp, &pg_sz);
	if (rc < 0)
		return rc;

	bd = rte_zmalloc_socket("bucket_pool", sizeof(*bd),
				RTE_CACHE_LINE_SIZE, mp->socket_id);
	if (bd == NULL) {
		rc = -ENOMEM;
		goto no_mem_for_data;
	}
	bd->pool = mp;
	if (mp->flags & MEMPOOL_F_NO_CACHE_ALIGN)
		bucket_header_size = sizeof(struct bucket_header);
	else
		bucket_header_size = RTE_CACHE_LINE_SIZE;
	RTE_BUILD_BUG_ON(sizeof(struct bucket_header) > RTE_CACHE_LINE_SIZE);
	bd->header_size = mp->header_size + bucket_header_size;
	bd->total_elt_size = mp->header_size + mp->elt_size + mp->trailer_size;
	bd->bucket_mem_size = RTE_MIN(pg_sz,
			(size_t)(RTE_DRIVER_MEMPOOL_BUCKET_SIZE_KB * 1024));
	bd->obj_per_bucket = (bd->bucket_mem_size - bucket_header_size) /
		bd->total_elt_size;
	bd->bucket_page_mask = ~(rte_align64pow2(bd->bucket_mem_size) - 1);
	/* eventually this should be a tunable parameter */
	bd->bucket_stack_thresh = (mp->size / bd->obj_per_bucket) * 4 / 3;

	bd->lcore_callback_handle = rte_lcore_callback_register("bucket",
		bucket_init_per_lcore, bucket_uninit_per_lcore, bd);
	if (bd->lcore_callback_handle == NULL) {
		rc = -ENOMEM;
		goto no_mem_for_stacks;
	}

	if (mp->flags & MEMPOOL_F_SP_PUT)
		rg_flags |= RING_F_SP_ENQ;
	if (mp->flags & MEMPOOL_F_SC_GET)
		rg_flags |= RING_F_SC_DEQ;
	rc = snprintf(rg_name, sizeof(rg_name),
		      RTE_MEMPOOL_MZ_FORMAT ".0", mp->name);
	if (rc < 0 || rc >= (int)sizeof(rg_name)) {
		rc = -ENAMETOOLONG;
		goto invalid_shared_orphan_ring;
	}
	bd->shared_orphan_ring =
		rte_ring_create(rg_name, rte_align32pow2(mp->size + 1),
				mp->socket_id, rg_flags);
	if (bd->shared_orphan_ring == NULL) {
		rc = -rte_errno;
		goto cannot_create_shared_orphan_ring;
	}

	rc = snprintf(rg_name, sizeof(rg_name),
		       RTE_MEMPOOL_MZ_FORMAT ".1", mp->name);
	if (rc < 0 || rc >= (int)sizeof(rg_name)) {
		rc = -ENAMETOOLONG;
		goto invalid_shared_bucket_ring;
	}
	bd->shared_bucket_ring =
		rte_ring_create(rg_name,
				rte_align32pow2((mp->size + 1) /
						bd->obj_per_bucket),
				mp->socket_id, rg_flags);
	if (bd->shared_bucket_ring == NULL) {
		rc = -rte_errno;
		goto cannot_create_shared_bucket_ring;
	}

	mp->pool_data = bd;

	return 0;

cannot_create_shared_bucket_ring:
invalid_shared_bucket_ring:
	rte_ring_free(bd->shared_orphan_ring);
cannot_create_shared_orphan_ring:
invalid_shared_orphan_ring:
	rte_lcore_callback_unregister(bd->lcore_callback_handle);
no_mem_for_stacks:
	rte_free(bd);
no_mem_for_data:
	rte_errno = -rc;
	return rc;
}

static void
bucket_free(struct rte_mempool *mp)
{
	struct bucket_data *bd = mp->pool_data;

	if (bd == NULL)
		return;

	rte_lcore_callback_unregister(bd->lcore_callback_handle);

	rte_ring_free(bd->shared_orphan_ring);
	rte_ring_free(bd->shared_bucket_ring);

	rte_free(bd);
}

static ssize_t
bucket_calc_mem_size(const struct rte_mempool *mp, uint32_t obj_num,
		     __rte_unused uint32_t pg_shift, size_t *min_total_elt_size,
		     size_t *align)
{
	struct bucket_data *bd = mp->pool_data;
	unsigned int bucket_page_sz;

	if (bd == NULL)
		return -EINVAL;

	bucket_page_sz = rte_align32pow2(bd->bucket_mem_size);
	*align = bucket_page_sz;
	*min_total_elt_size = bucket_page_sz;
	/*
	 * Each bucket occupies its own block aligned to
	 * bucket_page_sz, so the required amount of memory is
	 * a multiple of bucket_page_sz.
	 * We also need extra space for a bucket header
	 */
	return ((obj_num + bd->obj_per_bucket - 1) /
		bd->obj_per_bucket) * bucket_page_sz;
}

static int
bucket_populate(struct rte_mempool *mp, unsigned int max_objs,
		void *vaddr, rte_iova_t iova, size_t len,
		rte_mempool_populate_obj_cb_t *obj_cb, void *obj_cb_arg)
{
	struct bucket_data *bd = mp->pool_data;
	unsigned int bucket_page_sz;
	unsigned int bucket_header_sz;
	unsigned int n_objs;
	uintptr_t align;
	uint8_t *iter;
	int rc;

	if (bd == NULL)
		return -EINVAL;

	bucket_page_sz = rte_align32pow2(bd->bucket_mem_size);
	align = RTE_PTR_ALIGN_CEIL((uintptr_t)vaddr, bucket_page_sz) -
		(uintptr_t)vaddr;

	bucket_header_sz = bd->header_size - mp->header_size;
	if (iova != RTE_BAD_IOVA)
		iova += align + bucket_header_sz;

	for (iter = (uint8_t *)vaddr + align, n_objs = 0;
	     iter < (uint8_t *)vaddr + len && n_objs < max_objs;
	     iter += bucket_page_sz) {
		struct bucket_header *hdr = (struct bucket_header *)iter;
		unsigned int chunk_len = bd->bucket_mem_size;

		if ((size_t)(iter - (uint8_t *)vaddr) + chunk_len > len)
			chunk_len = len - (iter - (uint8_t *)vaddr);
		if (chunk_len <= bucket_header_sz)
			break;
		chunk_len -= bucket_header_sz;

		hdr->fill_cnt = 0;
		hdr->lcore_id = LCORE_ID_ANY;
		rc = rte_mempool_op_populate_helper(mp, 0,
						     RTE_MIN(bd->obj_per_bucket,
							     max_objs - n_objs),
						     iter + bucket_header_sz,
						     iova, chunk_len,
						     obj_cb, obj_cb_arg);
		if (rc < 0)
			return rc;
		n_objs += rc;
		if (iova != RTE_BAD_IOVA)
			iova += bucket_page_sz;
	}

	return n_objs;
}

static int
bucket_get_info(const struct rte_mempool *mp, struct rte_mempool_info *info)
{
	struct bucket_data *bd = mp->pool_data;

	info->contig_block_size = bd->obj_per_bucket;
	return 0;
}


static const struct rte_mempool_ops ops_bucket = {
	.name = "bucket",
	.alloc = bucket_alloc,
	.free = bucket_free,
	.enqueue = bucket_enqueue,
	.dequeue = bucket_dequeue,
	.get_count = bucket_get_count,
	.calc_mem_size = bucket_calc_mem_size,
	.populate = bucket_populate,
	.get_info = bucket_get_info,
	.dequeue_contig_blocks = bucket_dequeue_contig_blocks,
};


MEMPOOL_REGISTER_OPS(ops_bucket);
