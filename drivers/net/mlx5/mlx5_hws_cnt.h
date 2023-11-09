/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2022 Mellanox Technologies, Ltd
 */

#ifndef _MLX5_HWS_CNT_H_
#define _MLX5_HWS_CNT_H_

#include <rte_ring.h>
#include "mlx5_utils.h"
#include "mlx5_flow.h"

/*
 * HWS COUNTER ID's layout
 *       3                   2                   1                   0
 *     1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |  T  |     | D |                                               |
 *    ~  Y  |     | C |                    IDX                        ~
 *    |  P  |     | S |                                               |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 *    Bit 31:29 = TYPE = MLX5_INDIRECT_ACTION_TYPE_COUNT = b'10
 *    Bit 25:24 = DCS index
 *    Bit 23:00 = IDX in this counter belonged DCS bulk.
 */

#define MLX5_HWS_CNT_DCS_IDX_OFFSET 24
#define MLX5_HWS_CNT_DCS_IDX_MASK 0x3
#define MLX5_HWS_CNT_IDX_MASK ((1UL << MLX5_HWS_CNT_DCS_IDX_OFFSET) - 1)

#define MLX5_HWS_AGE_IDX_MASK (RTE_BIT32(MLX5_INDIRECT_ACTION_TYPE_OFFSET) - 1)

struct mlx5_hws_cnt_dcs {
	void *dr_action;
	uint32_t batch_sz;
	uint32_t iidx; /* internal index of first counter in this bulk. */
	struct mlx5_devx_obj *obj;
};

struct mlx5_hws_cnt_dcs_mng {
	uint32_t batch_total;
	struct mlx5_hws_cnt_dcs dcs[MLX5_HWS_CNT_DCS_NUM];
};

struct mlx5_hws_cnt {
	struct flow_counter_stats reset;
	bool in_used; /* Indicator whether this counter in used or in pool. */
	union {
		struct {
			uint32_t share:1;
			/*
			 * share will be set to 1 when this counter is used as
			 * indirect action.
			 */
			uint32_t age_idx:24;
			/*
			 * When this counter uses for aging, it save the index
			 * of AGE parameter. For pure counter (without aging)
			 * this index is zero.
			 */
		};
		/* This struct is only meaningful when user own this counter. */
		uint32_t query_gen_when_free;
		/*
		 * When PMD own this counter (user put back counter to PMD
		 * counter pool, i.e), this field recorded value of counter
		 * pools query generation at time user release the counter.
		 */
	};
};

struct mlx5_hws_cnt_raw_data_mng {
	struct flow_counter_stats *raw;
	struct mlx5_pmd_mr mr;
};

struct mlx5_hws_cache_param {
	uint32_t size;
	uint32_t q_num;
	uint32_t fetch_sz;
	uint32_t threshold;
	uint32_t preload_sz;
};

struct mlx5_hws_cnt_pool_cfg {
	char *name;
	uint32_t request_num;
	uint32_t alloc_factor;
	struct mlx5_hws_cnt_pool *host_cpool;
};

struct mlx5_hws_cnt_pool_caches {
	uint32_t fetch_sz;
	uint32_t threshold;
	uint32_t preload_sz;
	uint32_t q_num;
	struct rte_ring *qcache[];
};

struct mlx5_hws_cnt_pool {
	LIST_ENTRY(mlx5_hws_cnt_pool) next;
	struct mlx5_hws_cnt_pool_cfg cfg __rte_cache_aligned;
	struct mlx5_hws_cnt_dcs_mng dcs_mng __rte_cache_aligned;
	uint32_t query_gen __rte_cache_aligned;
	struct mlx5_hws_cnt *pool;
	struct mlx5_hws_cnt_raw_data_mng *raw_mng;
	struct rte_ring *reuse_list;
	struct rte_ring *free_list;
	struct rte_ring *wait_reset_list;
	struct mlx5_hws_cnt_pool_caches *cache;
	uint64_t time_of_last_age_check;
	struct mlx5_priv *priv;
} __rte_cache_aligned;

/* HWS AGE status. */
enum {
	HWS_AGE_FREE, /* Initialized state. */
	HWS_AGE_CANDIDATE, /* AGE assigned to flows. */
	HWS_AGE_CANDIDATE_INSIDE_RING,
	/*
	 * AGE assigned to flows but it still in ring. It was aged-out but the
	 * timeout was changed, so it in ring but still candidate.
	 */
	HWS_AGE_AGED_OUT_REPORTED,
	/*
	 * Aged-out, reported by rte_flow_get_q_aged_flows and wait for destroy.
	 */
	HWS_AGE_AGED_OUT_NOT_REPORTED,
	/*
	 * Aged-out, inside the aged-out ring.
	 * wait for rte_flow_get_q_aged_flows and destroy.
	 */
};

/* HWS counter age parameter. */
struct mlx5_hws_age_param {
	uint32_t timeout; /* Aging timeout in seconds (atomically accessed). */
	uint32_t sec_since_last_hit;
	/* Time in seconds since last hit (atomically accessed). */
	uint16_t state; /* AGE state (atomically accessed). */
	uint64_t accumulator_last_hits;
	/* Last total value of hits for comparing. */
	uint64_t accumulator_hits;
	/* Accumulator for hits coming from several counters. */
	uint32_t accumulator_cnt;
	/* Number counters which already updated the accumulator in this sec. */
	uint32_t nb_cnts; /* Number counters used by this AGE. */
	uint32_t queue_id; /* Queue id of the counter. */
	cnt_id_t own_cnt_index;
	/* Counter action created specifically for this AGE action. */
	void *context; /* Flow AGE context. */
} __rte_packed __rte_cache_aligned;


/**
 * Return the actual counter pool should be used in cross vHCA sharing mode.
 * as index of raw/cnt pool.
 *
 * @param cnt_id
 *   The external counter id
 * @return
 *   Internal index
 */
static __rte_always_inline struct mlx5_hws_cnt_pool *
mlx5_hws_cnt_host_pool(struct mlx5_hws_cnt_pool *cpool)
{
	return cpool->cfg.host_cpool ? cpool->cfg.host_cpool : cpool;
}

/**
 * Translate counter id into internal index (start from 0), which can be used
 * as index of raw/cnt pool.
 *
 * @param cnt_id
 *   The external counter id
 * @return
 *   Internal index
 */
static __rte_always_inline uint32_t
mlx5_hws_cnt_iidx(struct mlx5_hws_cnt_pool *cpool, cnt_id_t cnt_id)
{
	struct mlx5_hws_cnt_pool *hpool = mlx5_hws_cnt_host_pool(cpool);
	uint8_t dcs_idx = cnt_id >> MLX5_HWS_CNT_DCS_IDX_OFFSET;
	uint32_t offset = cnt_id & MLX5_HWS_CNT_IDX_MASK;

	dcs_idx &= MLX5_HWS_CNT_DCS_IDX_MASK;
	return (hpool->dcs_mng.dcs[dcs_idx].iidx + offset);
}

/**
 * Check if it's valid counter id.
 */
static __rte_always_inline bool
mlx5_hws_cnt_id_valid(cnt_id_t cnt_id)
{
	return (cnt_id >> MLX5_INDIRECT_ACTION_TYPE_OFFSET) ==
		MLX5_INDIRECT_ACTION_TYPE_COUNT ? true : false;
}

/**
 * Generate Counter id from internal index.
 *
 * @param cpool
 *   The pointer to counter pool
 * @param iidx
 *   The internal counter index.
 *
 * @return
 *   Counter id
 */
static __rte_always_inline cnt_id_t
mlx5_hws_cnt_id_gen(struct mlx5_hws_cnt_pool *cpool, uint32_t iidx)
{
	struct mlx5_hws_cnt_pool *hpool = mlx5_hws_cnt_host_pool(cpool);
	struct mlx5_hws_cnt_dcs_mng *dcs_mng = &hpool->dcs_mng;
	uint32_t idx;
	uint32_t offset;
	cnt_id_t cnt_id;

	for (idx = 0, offset = iidx; idx < dcs_mng->batch_total; idx++) {
		if (dcs_mng->dcs[idx].batch_sz <= offset)
			offset -= dcs_mng->dcs[idx].batch_sz;
		else
			break;
	}
	cnt_id = offset;
	cnt_id |= (idx << MLX5_HWS_CNT_DCS_IDX_OFFSET);
	return (MLX5_INDIRECT_ACTION_TYPE_COUNT <<
			MLX5_INDIRECT_ACTION_TYPE_OFFSET) | cnt_id;
}

static __rte_always_inline void
__hws_cnt_query_raw(struct mlx5_hws_cnt_pool *cpool, cnt_id_t cnt_id,
		uint64_t *raw_pkts, uint64_t *raw_bytes)
{
	struct mlx5_hws_cnt_pool *hpool = mlx5_hws_cnt_host_pool(cpool);
	struct mlx5_hws_cnt_raw_data_mng *raw_mng = hpool->raw_mng;
	struct flow_counter_stats s[2];
	uint8_t i = 0x1;
	size_t stat_sz = sizeof(s[0]);
	uint32_t iidx = mlx5_hws_cnt_iidx(cpool, cnt_id);

	memcpy(&s[0], &raw_mng->raw[iidx], stat_sz);
	do {
		memcpy(&s[i & 1], &raw_mng->raw[iidx], stat_sz);
		if (memcmp(&s[0], &s[1], stat_sz) == 0) {
			*raw_pkts = rte_be_to_cpu_64(s[0].hits);
			*raw_bytes = rte_be_to_cpu_64(s[0].bytes);
			break;
		}
		i = ~i;
	} while (1);
}

/**
 * Copy elements from one zero-copy ring to zero-copy ring in place.
 *
 * The input is a rte ring zero-copy data struct, which has two pointer.
 * in case of the wrapper happened, the ptr2 will be meaningful.
 *
 * So this routine needs to consider the situation that the address given by
 * source and destination could be both wrapped.
 * First, calculate the first number of element needs to be copied until wrapped
 * address, which could be in source or destination.
 * Second, copy left number of element until second wrapped address. If in first
 * step the wrapped address is source, then this time it must be in destination.
 * and vice-versa.
 * Third, copy all left number of element.
 *
 * In worst case, we need copy three pieces of continuous memory.
 *
 * @param zcdd
 *   A pointer to zero-copy data of destination ring.
 * @param zcds
 *   A pointer to zero-copy data of source ring.
 * @param n
 *   Number of elements to copy.
 */
static __rte_always_inline void
__hws_cnt_r2rcpy(struct rte_ring_zc_data *zcdd, struct rte_ring_zc_data *zcds,
		 unsigned int n)
{
	unsigned int n1, n2, n3;
	void *s1, *s2, *s3;
	void *d1, *d2, *d3;

	s1 = zcds->ptr1;
	d1 = zcdd->ptr1;
	n1 = RTE_MIN(zcdd->n1, zcds->n1);
	if (zcds->n1 > n1) {
		n2 = zcds->n1 - n1;
		s2 = RTE_PTR_ADD(zcds->ptr1, sizeof(cnt_id_t) * n1);
		d2 = zcdd->ptr2;
		n3 = n - n1 - n2;
		s3 = zcds->ptr2;
		d3 = RTE_PTR_ADD(zcdd->ptr2, sizeof(cnt_id_t) * n2);
	} else {
		n2 = zcdd->n1 - n1;
		s2 = zcds->ptr2;
		d2 = RTE_PTR_ADD(zcdd->ptr1, sizeof(cnt_id_t) * n1);
		n3 = n - n1 - n2;
		s3 = RTE_PTR_ADD(zcds->ptr2, sizeof(cnt_id_t) * n2);
		d3 = zcdd->ptr2;
	}
	memcpy(d1, s1, n1 * sizeof(cnt_id_t));
	if (n2 != 0)
		memcpy(d2, s2, n2 * sizeof(cnt_id_t));
	if (n3 != 0)
		memcpy(d3, s3, n3 * sizeof(cnt_id_t));
}

static __rte_always_inline int
mlx5_hws_cnt_pool_cache_flush(struct mlx5_hws_cnt_pool *cpool,
			      uint32_t queue_id)
{
	unsigned int ret __rte_unused;
	struct rte_ring_zc_data zcdr = {0};
	struct rte_ring_zc_data zcdc = {0};
	struct rte_ring *reset_list = NULL;
	struct rte_ring *qcache = cpool->cache->qcache[queue_id];
	uint32_t ring_size = rte_ring_count(qcache);

	ret = rte_ring_dequeue_zc_burst_elem_start(qcache, sizeof(cnt_id_t),
						   ring_size, &zcdc, NULL);
	MLX5_ASSERT(ret == ring_size);
	reset_list = cpool->wait_reset_list;
	ret = rte_ring_enqueue_zc_burst_elem_start(reset_list, sizeof(cnt_id_t),
						   ring_size, &zcdr, NULL);
	MLX5_ASSERT(ret == ring_size);
	__hws_cnt_r2rcpy(&zcdr, &zcdc, ring_size);
	rte_ring_enqueue_zc_elem_finish(reset_list, ring_size);
	rte_ring_dequeue_zc_elem_finish(qcache, ring_size);
	return 0;
}

static __rte_always_inline int
mlx5_hws_cnt_pool_cache_fetch(struct mlx5_hws_cnt_pool *cpool,
			      uint32_t queue_id)
{
	struct rte_ring *qcache = cpool->cache->qcache[queue_id];
	struct rte_ring *free_list = NULL;
	struct rte_ring *reuse_list = NULL;
	struct rte_ring *list = NULL;
	struct rte_ring_zc_data zcdf = {0};
	struct rte_ring_zc_data zcdc = {0};
	struct rte_ring_zc_data zcdu = {0};
	struct rte_ring_zc_data zcds = {0};
	struct mlx5_hws_cnt_pool_caches *cache = cpool->cache;
	unsigned int ret, actual_fetch_size __rte_unused;

	reuse_list = cpool->reuse_list;
	ret = rte_ring_dequeue_zc_burst_elem_start(reuse_list,
			sizeof(cnt_id_t), cache->fetch_sz, &zcdu, NULL);
	zcds = zcdu;
	list = reuse_list;
	if (unlikely(ret == 0)) { /* no reuse counter. */
		rte_ring_dequeue_zc_elem_finish(reuse_list, 0);
		free_list = cpool->free_list;
		ret = rte_ring_dequeue_zc_burst_elem_start(free_list,
							   sizeof(cnt_id_t),
							   cache->fetch_sz,
							   &zcdf, NULL);
		zcds = zcdf;
		list = free_list;
		if (unlikely(ret == 0)) { /* no free counter. */
			rte_ring_dequeue_zc_elem_finish(free_list, 0);
			if (rte_ring_count(cpool->wait_reset_list))
				return -EAGAIN;
			return -ENOENT;
		}
	}
	actual_fetch_size = ret;
	ret = rte_ring_enqueue_zc_burst_elem_start(qcache, sizeof(cnt_id_t),
						   ret, &zcdc, NULL);
	MLX5_ASSERT(ret == actual_fetch_size);
	__hws_cnt_r2rcpy(&zcdc, &zcds, ret);
	rte_ring_dequeue_zc_elem_finish(list, ret);
	rte_ring_enqueue_zc_elem_finish(qcache, ret);
	return 0;
}

static __rte_always_inline int
__mlx5_hws_cnt_pool_enqueue_revert(struct rte_ring *r, unsigned int n,
		struct rte_ring_zc_data *zcd)
{
	uint32_t current_head = 0;
	uint32_t revert2head = 0;

	MLX5_ASSERT(r->prod.sync_type == RTE_RING_SYNC_ST);
	MLX5_ASSERT(r->cons.sync_type == RTE_RING_SYNC_ST);
	current_head = rte_atomic_load_explicit(&r->prod.head, rte_memory_order_relaxed);
	MLX5_ASSERT(n <= r->capacity);
	MLX5_ASSERT(n <= rte_ring_count(r));
	revert2head = current_head - n;
	r->prod.head = revert2head; /* This ring should be SP. */
	__rte_ring_get_elem_addr(r, revert2head, sizeof(cnt_id_t), n,
			&zcd->ptr1, &zcd->n1, &zcd->ptr2);
	/* Update tail */
	rte_atomic_store_explicit(&r->prod.tail, revert2head, rte_memory_order_release);
	return n;
}

/**
 * Put one counter back in the mempool.
 *
 * @param cpool
 *   A pointer to the counter pool structure.
 * @param queue
 *   A pointer to HWS queue. If null, it means put into common pool.
 * @param cnt_id
 *   A counter id to be added.
 */
static __rte_always_inline void
mlx5_hws_cnt_pool_put(struct mlx5_hws_cnt_pool *cpool, uint32_t *queue,
		      cnt_id_t *cnt_id)
{
	unsigned int ret = 0;
	struct mlx5_hws_cnt_pool *hpool;
	struct rte_ring_zc_data zcdc = {0};
	struct rte_ring_zc_data zcdr = {0};
	struct rte_ring *qcache = NULL;
	unsigned int wb_num = 0; /* cache write-back number. */
	uint32_t iidx;

	hpool = mlx5_hws_cnt_host_pool(cpool);
	iidx = mlx5_hws_cnt_iidx(hpool, *cnt_id);
	hpool->pool[iidx].in_used = false;
	hpool->pool[iidx].query_gen_when_free =
		__atomic_load_n(&hpool->query_gen, __ATOMIC_RELAXED);
	if (likely(queue != NULL) && cpool->cfg.host_cpool == NULL)
		qcache = hpool->cache->qcache[*queue];
	if (unlikely(qcache == NULL)) {
		ret = rte_ring_enqueue_elem(hpool->wait_reset_list, cnt_id,
				sizeof(cnt_id_t));
		MLX5_ASSERT(ret == 0);
		return;
	}
	ret = rte_ring_enqueue_burst_elem(qcache, cnt_id, sizeof(cnt_id_t), 1,
					  NULL);
	if (unlikely(ret == 0)) { /* cache is full. */
		struct rte_ring *reset_list = cpool->wait_reset_list;

		wb_num = rte_ring_count(qcache) - cpool->cache->threshold;
		MLX5_ASSERT(wb_num < rte_ring_count(qcache));
		__mlx5_hws_cnt_pool_enqueue_revert(qcache, wb_num, &zcdc);
		ret = rte_ring_enqueue_zc_burst_elem_start(reset_list,
							   sizeof(cnt_id_t),
							   wb_num, &zcdr, NULL);
		MLX5_ASSERT(ret == wb_num);
		__hws_cnt_r2rcpy(&zcdr, &zcdc, ret);
		rte_ring_enqueue_zc_elem_finish(reset_list, ret);
		/* write-back THIS counter too */
		ret = rte_ring_enqueue_burst_elem(reset_list, cnt_id,
						  sizeof(cnt_id_t), 1, NULL);
	}
	MLX5_ASSERT(ret == 1);
}

/**
 * Get one counter from the pool.
 *
 * If @param queue is not null, objects will be retrieved first from queue's
 * cache, subsequently from the common pool. Note that it can return -ENOENT
 * when the local cache and common pool are empty, even if cache from other
 * queue are full.
 *
 * @param cntp
 *   A pointer to the counter pool structure.
 * @param queue
 *   A pointer to HWS queue. If null, it means fetch from common pool.
 * @param cnt_id
 *   A pointer to a cnt_id_t * pointer (counter id) that will be filled.
 * @param age_idx
 *   Index of AGE parameter using this counter, zero means there is no such AGE.
 *
 * @return
 *   - 0: Success; objects taken.
 *   - -ENOENT: Not enough entries in the mempool; no object is retrieved.
 *   - -EAGAIN: counter is not ready; try again.
 */
static __rte_always_inline int
mlx5_hws_cnt_pool_get(struct mlx5_hws_cnt_pool *cpool, uint32_t *queue,
		      cnt_id_t *cnt_id, uint32_t age_idx)
{
	unsigned int ret;
	struct rte_ring_zc_data zcdc = {0};
	struct rte_ring *qcache = NULL;
	uint32_t iidx, query_gen = 0;
	cnt_id_t tmp_cid = 0;

	if (likely(queue != NULL && cpool->cfg.host_cpool == NULL))
		qcache = cpool->cache->qcache[*queue];
	if (unlikely(qcache == NULL)) {
		cpool = mlx5_hws_cnt_host_pool(cpool);
		ret = rte_ring_dequeue_elem(cpool->reuse_list, &tmp_cid,
				sizeof(cnt_id_t));
		if (unlikely(ret != 0)) {
			ret = rte_ring_dequeue_elem(cpool->free_list, &tmp_cid,
					sizeof(cnt_id_t));
			if (unlikely(ret != 0)) {
				if (rte_ring_count(cpool->wait_reset_list))
					return -EAGAIN;
				return -ENOENT;
			}
		}
		*cnt_id = tmp_cid;
		iidx = mlx5_hws_cnt_iidx(cpool, *cnt_id);
		__hws_cnt_query_raw(cpool, *cnt_id,
				    &cpool->pool[iidx].reset.hits,
				    &cpool->pool[iidx].reset.bytes);
		cpool->pool[iidx].share = 0;
		MLX5_ASSERT(!cpool->pool[iidx].in_used);
		cpool->pool[iidx].in_used = true;
		cpool->pool[iidx].age_idx = age_idx;
		return 0;
	}
	ret = rte_ring_dequeue_zc_burst_elem_start(qcache, sizeof(cnt_id_t), 1,
						   &zcdc, NULL);
	if (unlikely(ret == 0)) { /* local cache is empty. */
		rte_ring_dequeue_zc_elem_finish(qcache, 0);
		/* let's fetch from global free list. */
		ret = mlx5_hws_cnt_pool_cache_fetch(cpool, *queue);
		if (unlikely(ret != 0))
			return ret;
		ret = rte_ring_dequeue_zc_burst_elem_start(qcache,
							   sizeof(cnt_id_t), 1,
							   &zcdc, NULL);
		MLX5_ASSERT(ret == 1);
	}
	/* get one from local cache. */
	*cnt_id = (*(cnt_id_t *)zcdc.ptr1);
	iidx = mlx5_hws_cnt_iidx(cpool, *cnt_id);
	query_gen = cpool->pool[iidx].query_gen_when_free;
	if (cpool->query_gen == query_gen) { /* counter is waiting to reset. */
		rte_ring_dequeue_zc_elem_finish(qcache, 0);
		/* write-back counter to reset list. */
		mlx5_hws_cnt_pool_cache_flush(cpool, *queue);
		/* let's fetch from global free list. */
		ret = mlx5_hws_cnt_pool_cache_fetch(cpool, *queue);
		if (unlikely(ret != 0))
			return ret;
		ret = rte_ring_dequeue_zc_burst_elem_start(qcache,
							   sizeof(cnt_id_t), 1,
							   &zcdc, NULL);
		MLX5_ASSERT(ret == 1);
		*cnt_id = *(cnt_id_t *)zcdc.ptr1;
		iidx = mlx5_hws_cnt_iidx(cpool, *cnt_id);
	}
	__hws_cnt_query_raw(cpool, *cnt_id, &cpool->pool[iidx].reset.hits,
			    &cpool->pool[iidx].reset.bytes);
	rte_ring_dequeue_zc_elem_finish(qcache, 1);
	cpool->pool[iidx].share = 0;
	MLX5_ASSERT(!cpool->pool[iidx].in_used);
	cpool->pool[iidx].in_used = true;
	cpool->pool[iidx].age_idx = age_idx;
	return 0;
}

/**
 * Check if counter pool allocated for HWS is shared between ports.
 *
 * @param[in] priv
 *   Pointer to the port private data structure.
 *
 * @return
 *   True if counter pools is shared between ports. False otherwise.
 */
static __rte_always_inline bool
mlx5_hws_cnt_is_pool_shared(struct mlx5_priv *priv)
{
	return priv && priv->hws_cpool &&
	    (priv->shared_refcnt || priv->hws_cpool->cfg.host_cpool != NULL);
}

static __rte_always_inline unsigned int
mlx5_hws_cnt_pool_get_size(struct mlx5_hws_cnt_pool *cpool)
{
	struct mlx5_hws_cnt_pool *hpool = mlx5_hws_cnt_host_pool(cpool);

	return rte_ring_get_capacity(hpool->free_list);
}

static __rte_always_inline int
mlx5_hws_cnt_pool_get_action_offset(struct mlx5_hws_cnt_pool *cpool,
		cnt_id_t cnt_id, struct mlx5dr_action **action,
		uint32_t *offset)
{
	uint8_t idx = cnt_id >> MLX5_HWS_CNT_DCS_IDX_OFFSET;

	idx &= MLX5_HWS_CNT_DCS_IDX_MASK;
	*action = cpool->dcs_mng.dcs[idx].dr_action;
	*offset = cnt_id & MLX5_HWS_CNT_IDX_MASK;
	return 0;
}

static __rte_always_inline int
mlx5_hws_cnt_shared_get(struct mlx5_hws_cnt_pool *cpool, cnt_id_t *cnt_id,
			uint32_t age_idx)
{
	struct mlx5_hws_cnt_pool *hpool = mlx5_hws_cnt_host_pool(cpool);
	uint32_t iidx;
	int ret;

	ret = mlx5_hws_cnt_pool_get(hpool, NULL, cnt_id, age_idx);
	if (ret != 0)
		return ret;
	iidx = mlx5_hws_cnt_iidx(hpool, *cnt_id);
	hpool->pool[iidx].share = 1;
	return 0;
}

static __rte_always_inline void
mlx5_hws_cnt_shared_put(struct mlx5_hws_cnt_pool *cpool, cnt_id_t *cnt_id)
{
	struct mlx5_hws_cnt_pool *hpool = mlx5_hws_cnt_host_pool(cpool);
	uint32_t iidx = mlx5_hws_cnt_iidx(hpool, *cnt_id);

	hpool->pool[iidx].share = 0;
	mlx5_hws_cnt_pool_put(hpool, NULL, cnt_id);
}

static __rte_always_inline bool
mlx5_hws_cnt_is_shared(struct mlx5_hws_cnt_pool *cpool, cnt_id_t cnt_id)
{
	struct mlx5_hws_cnt_pool *hpool = mlx5_hws_cnt_host_pool(cpool);
	uint32_t iidx = mlx5_hws_cnt_iidx(hpool, cnt_id);

	return hpool->pool[iidx].share ? true : false;
}

static __rte_always_inline void
mlx5_hws_cnt_age_set(struct mlx5_hws_cnt_pool *cpool, cnt_id_t cnt_id,
		     uint32_t age_idx)
{
	struct mlx5_hws_cnt_pool *hpool = mlx5_hws_cnt_host_pool(cpool);
	uint32_t iidx = mlx5_hws_cnt_iidx(hpool, cnt_id);

	MLX5_ASSERT(hpool->pool[iidx].share);
	hpool->pool[iidx].age_idx = age_idx;
}

static __rte_always_inline uint32_t
mlx5_hws_cnt_age_get(struct mlx5_hws_cnt_pool *cpool, cnt_id_t cnt_id)
{
	struct mlx5_hws_cnt_pool *hpool = mlx5_hws_cnt_host_pool(cpool);
	uint32_t iidx = mlx5_hws_cnt_iidx(hpool, cnt_id);

	MLX5_ASSERT(hpool->pool[iidx].share);
	return hpool->pool[iidx].age_idx;
}

static __rte_always_inline cnt_id_t
mlx5_hws_age_cnt_get(struct mlx5_priv *priv, struct mlx5_hws_age_param *param,
		     uint32_t age_idx)
{
	if (!param->own_cnt_index) {
		/* Create indirect counter one for internal usage. */
		if (mlx5_hws_cnt_shared_get(priv->hws_cpool,
					    &param->own_cnt_index, age_idx) < 0)
			return 0;
		param->nb_cnts++;
	}
	return param->own_cnt_index;
}

static __rte_always_inline void
mlx5_hws_age_nb_cnt_increase(struct mlx5_priv *priv, uint32_t age_idx)
{
	struct mlx5_age_info *age_info = GET_PORT_AGE_INFO(priv);
	struct mlx5_indexed_pool *ipool = age_info->ages_ipool;
	struct mlx5_hws_age_param *param = mlx5_ipool_get(ipool, age_idx);

	MLX5_ASSERT(param != NULL);
	param->nb_cnts++;
}

static __rte_always_inline void
mlx5_hws_age_nb_cnt_decrease(struct mlx5_priv *priv, uint32_t age_idx)
{
	struct mlx5_age_info *age_info = GET_PORT_AGE_INFO(priv);
	struct mlx5_indexed_pool *ipool = age_info->ages_ipool;
	struct mlx5_hws_age_param *param = mlx5_ipool_get(ipool, age_idx);

	if (param != NULL)
		param->nb_cnts--;
}

static __rte_always_inline bool
mlx5_hws_age_is_indirect(uint32_t age_idx)
{
	return (age_idx >> MLX5_INDIRECT_ACTION_TYPE_OFFSET) ==
		MLX5_INDIRECT_ACTION_TYPE_AGE ? true : false;
}

/* init HWS counter pool. */
int
mlx5_hws_cnt_service_thread_create(struct mlx5_dev_ctx_shared *sh);

void
mlx5_hws_cnt_service_thread_destroy(struct mlx5_dev_ctx_shared *sh);

struct mlx5_hws_cnt_pool *
mlx5_hws_cnt_pool_create(struct rte_eth_dev *dev,
		const struct rte_flow_port_attr *pattr, uint16_t nb_queue);

void
mlx5_hws_cnt_pool_destroy(struct mlx5_dev_ctx_shared *sh,
		struct mlx5_hws_cnt_pool *cpool);

int
mlx5_hws_cnt_svc_init(struct mlx5_dev_ctx_shared *sh);

void
mlx5_hws_cnt_svc_deinit(struct mlx5_dev_ctx_shared *sh);

int
mlx5_hws_age_action_destroy(struct mlx5_priv *priv, uint32_t idx,
			    struct rte_flow_error *error);

uint32_t
mlx5_hws_age_action_create(struct mlx5_priv *priv, uint32_t queue_id,
			   bool shared, const struct rte_flow_action_age *age,
			   uint32_t flow_idx, struct rte_flow_error *error);

int
mlx5_hws_age_action_update(struct mlx5_priv *priv, uint32_t idx,
			   const void *update, struct rte_flow_error *error);

void *
mlx5_hws_age_context_get(struct mlx5_priv *priv, uint32_t idx);

int
mlx5_hws_age_pool_init(struct rte_eth_dev *dev,
		       const struct rte_flow_port_attr *attr,
		       uint16_t nb_queues);

void
mlx5_hws_age_pool_destroy(struct mlx5_priv *priv);

#endif /* _MLX5_HWS_CNT_H_ */
