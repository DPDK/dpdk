/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#include <stdint.h>
#include <rte_malloc.h>
#include <mlx5_malloc.h>
#include <rte_ring.h>
#include <mlx5_devx_cmds.h>
#include <rte_cycles.h>
#include <rte_eal_paging.h>
#include <rte_thread.h>

#if defined(HAVE_IBV_FLOW_DV_SUPPORT) || !defined(HAVE_INFINIBAND_VERBS_H)

#include "mlx5_utils.h"
#include "mlx5_hws_cnt.h"

#define HWS_CNT_CACHE_SZ_DEFAULT 511
#define HWS_CNT_CACHE_PRELOAD_DEFAULT 254
#define HWS_CNT_CACHE_FETCH_DEFAULT 254
#define HWS_CNT_CACHE_THRESHOLD_DEFAULT 254
#define HWS_CNT_ALLOC_FACTOR_DEFAULT 20

static void
__hws_cnt_id_load(struct mlx5_hws_cnt_pool *cpool)
{
	uint32_t cnt_num = mlx5_hws_cnt_pool_get_size(cpool);
	uint32_t iidx;

	/*
	 * Counter ID order is important for tracking the max number of in used
	 * counter for querying, which means counter internal index order must
	 * be from zero to the number user configured, i.e: 0 - 8000000.
	 * Need to load counter ID in this order into the cache firstly,
	 * and then the global free list.
	 * In the end, user fetch the counter from minimal to the maximum.
	 */
	for (iidx = 0; iidx < cnt_num; iidx++) {
		cnt_id_t cnt_id  = mlx5_hws_cnt_id_gen(cpool, iidx);

		rte_ring_enqueue_elem(cpool->free_list, &cnt_id,
				sizeof(cnt_id));
	}
}

static void
__mlx5_hws_cnt_svc(struct mlx5_dev_ctx_shared *sh,
		   struct mlx5_hws_cnt_pool *cpool)
{
	struct rte_ring *reset_list = cpool->wait_reset_list;
	struct rte_ring *reuse_list = cpool->reuse_list;
	uint32_t reset_cnt_num;
	struct rte_ring_zc_data zcdr = {0};
	struct rte_ring_zc_data zcdu = {0};
	uint32_t ret __rte_unused;

	reset_cnt_num = rte_ring_count(reset_list);
	do {
		cpool->query_gen++;
		mlx5_aso_cnt_query(sh, cpool);
		zcdr.n1 = 0;
		zcdu.n1 = 0;
		ret = rte_ring_enqueue_zc_burst_elem_start(reuse_list,
							   sizeof(cnt_id_t),
							   reset_cnt_num, &zcdu,
							   NULL);
		MLX5_ASSERT(ret == reset_cnt_num);
		ret = rte_ring_dequeue_zc_burst_elem_start(reset_list,
							   sizeof(cnt_id_t),
							   reset_cnt_num, &zcdr,
							   NULL);
		MLX5_ASSERT(ret == reset_cnt_num);
		__hws_cnt_r2rcpy(&zcdu, &zcdr, reset_cnt_num);
		rte_ring_dequeue_zc_elem_finish(reset_list, reset_cnt_num);
		rte_ring_enqueue_zc_elem_finish(reuse_list, reset_cnt_num);
		reset_cnt_num = rte_ring_count(reset_list);
	} while (reset_cnt_num > 0);
}

/**
 * Release AGE parameter.
 *
 * @param priv
 *   Pointer to the port private data structure.
 * @param own_cnt_index
 *   Counter ID to created only for this AGE to release.
 *   Zero means there is no such counter.
 * @param age_ipool
 *   Pointer to AGE parameter indexed pool.
 * @param idx
 *   Index of AGE parameter in the indexed pool.
 */
static void
mlx5_hws_age_param_free(struct mlx5_priv *priv, cnt_id_t own_cnt_index,
			struct mlx5_indexed_pool *age_ipool, uint32_t idx)
{
	if (own_cnt_index) {
		struct mlx5_hws_cnt_pool *cpool = priv->hws_cpool;

		MLX5_ASSERT(mlx5_hws_cnt_is_shared(cpool, own_cnt_index));
		mlx5_hws_cnt_shared_put(cpool, &own_cnt_index);
	}
	mlx5_ipool_free(age_ipool, idx);
}

/**
 * Check and callback event for new aged flow in the HWS counter pool.
 *
 * @param[in] priv
 *   Pointer to port private object.
 * @param[in] cpool
 *   Pointer to current counter pool.
 */
static void
mlx5_hws_aging_check(struct mlx5_priv *priv, struct mlx5_hws_cnt_pool *cpool)
{
	struct mlx5_age_info *age_info = GET_PORT_AGE_INFO(priv);
	struct flow_counter_stats *stats = cpool->raw_mng->raw;
	struct mlx5_hws_age_param *param;
	struct rte_ring *r;
	const uint64_t curr_time = MLX5_CURR_TIME_SEC;
	const uint32_t time_delta = curr_time - cpool->time_of_last_age_check;
	uint32_t nb_alloc_cnts = mlx5_hws_cnt_pool_get_size(cpool);
	uint16_t expected1 = HWS_AGE_CANDIDATE;
	uint16_t expected2 = HWS_AGE_CANDIDATE_INSIDE_RING;
	uint32_t i;

	cpool->time_of_last_age_check = curr_time;
	for (i = 0; i < nb_alloc_cnts; ++i) {
		uint32_t age_idx = cpool->pool[i].age_idx;
		uint64_t hits;

		if (!cpool->pool[i].in_used || age_idx == 0)
			continue;
		param = mlx5_ipool_get(age_info->ages_ipool, age_idx);
		if (unlikely(param == NULL)) {
			/*
			 * When AGE which used indirect counter it is user
			 * responsibility not using this indirect counter
			 * without this AGE.
			 * If this counter is used after the AGE was freed, the
			 * AGE index is invalid and using it here will cause a
			 * segmentation fault.
			 */
			DRV_LOG(WARNING,
				"Counter %u is lost his AGE, it is unused.", i);
			continue;
		}
		if (param->timeout == 0)
			continue;
		switch (__atomic_load_n(&param->state, __ATOMIC_RELAXED)) {
		case HWS_AGE_AGED_OUT_NOT_REPORTED:
		case HWS_AGE_AGED_OUT_REPORTED:
			/* Already aged-out, no action is needed. */
			continue;
		case HWS_AGE_CANDIDATE:
		case HWS_AGE_CANDIDATE_INSIDE_RING:
			/* This AGE candidate to be aged-out, go to checking. */
			break;
		case HWS_AGE_FREE:
			/*
			 * AGE parameter with state "FREE" couldn't be pointed
			 * by any counter since counter is destroyed first.
			 * Fall-through.
			 */
		default:
			MLX5_ASSERT(0);
			continue;
		}
		hits = rte_be_to_cpu_64(stats[i].hits);
		if (param->nb_cnts == 1) {
			if (hits != param->accumulator_last_hits) {
				__atomic_store_n(&param->sec_since_last_hit, 0,
						 __ATOMIC_RELAXED);
				param->accumulator_last_hits = hits;
				continue;
			}
		} else {
			param->accumulator_hits += hits;
			param->accumulator_cnt++;
			if (param->accumulator_cnt < param->nb_cnts)
				continue;
			param->accumulator_cnt = 0;
			if (param->accumulator_last_hits !=
						param->accumulator_hits) {
				__atomic_store_n(&param->sec_since_last_hit,
						 0, __ATOMIC_RELAXED);
				param->accumulator_last_hits =
							param->accumulator_hits;
				param->accumulator_hits = 0;
				continue;
			}
			param->accumulator_hits = 0;
		}
		if (__atomic_fetch_add(&param->sec_since_last_hit, time_delta,
				       __ATOMIC_RELAXED) + time_delta <=
		   __atomic_load_n(&param->timeout, __ATOMIC_RELAXED))
			continue;
		/* Prepare the relevant ring for this AGE parameter */
		if (priv->hws_strict_queue)
			r = age_info->hw_q_age->aged_lists[param->queue_id];
		else
			r = age_info->hw_age.aged_list;
		/* Changing the state atomically and insert it into the ring. */
		if (__atomic_compare_exchange_n(&param->state, &expected1,
						HWS_AGE_AGED_OUT_NOT_REPORTED,
						false, __ATOMIC_RELAXED,
						__ATOMIC_RELAXED)) {
			int ret = rte_ring_enqueue_burst_elem(r, &age_idx,
							      sizeof(uint32_t),
							      1, NULL);

			/*
			 * The ring doesn't have enough room for this entry,
			 * it replace back the state for the next second.
			 *
			 * FIXME: if until next sec it get traffic, we are going
			 *        to lose this "aged out", will be fixed later
			 *        when optimise it to fill ring in bulks.
			 */
			expected2 = HWS_AGE_AGED_OUT_NOT_REPORTED;
			if (ret == 0 &&
			    !__atomic_compare_exchange_n(&param->state,
							 &expected2, expected1,
							 false,
							 __ATOMIC_RELAXED,
							 __ATOMIC_RELAXED) &&
			    expected2 == HWS_AGE_FREE)
				mlx5_hws_age_param_free(priv,
							param->own_cnt_index,
							age_info->ages_ipool,
							age_idx);
			/* The event is irrelevant in strict queue mode. */
			if (!priv->hws_strict_queue)
				MLX5_AGE_SET(age_info, MLX5_AGE_EVENT_NEW);
		} else {
			__atomic_compare_exchange_n(&param->state, &expected2,
						  HWS_AGE_AGED_OUT_NOT_REPORTED,
						  false, __ATOMIC_RELAXED,
						  __ATOMIC_RELAXED);
		}
	}
	/* The event is irrelevant in strict queue mode. */
	if (!priv->hws_strict_queue)
		mlx5_age_event_prepare(priv->sh);
}

static void
mlx5_hws_cnt_raw_data_free(struct mlx5_dev_ctx_shared *sh,
			   struct mlx5_hws_cnt_raw_data_mng *mng)
{
	if (mng == NULL)
		return;
	sh->cdev->mr_scache.dereg_mr_cb(&mng->mr);
	mlx5_free(mng->raw);
	mlx5_free(mng);
}

__rte_unused
static struct mlx5_hws_cnt_raw_data_mng *
mlx5_hws_cnt_raw_data_alloc(struct mlx5_dev_ctx_shared *sh, uint32_t n)
{
	struct mlx5_hws_cnt_raw_data_mng *mng = NULL;
	int ret;
	size_t sz = n * sizeof(struct flow_counter_stats);
	size_t pgsz = rte_mem_page_size();

	MLX5_ASSERT(pgsz > 0);
	mng = mlx5_malloc(MLX5_MEM_ANY | MLX5_MEM_ZERO, sizeof(*mng), 0,
			SOCKET_ID_ANY);
	if (mng == NULL)
		goto error;
	mng->raw = mlx5_malloc(MLX5_MEM_ANY | MLX5_MEM_ZERO, sz, pgsz,
			SOCKET_ID_ANY);
	if (mng->raw == NULL)
		goto error;
	ret = sh->cdev->mr_scache.reg_mr_cb(sh->cdev->pd, mng->raw, sz,
					    &mng->mr);
	if (ret) {
		rte_errno = errno;
		goto error;
	}
	return mng;
error:
	mlx5_hws_cnt_raw_data_free(sh, mng);
	return NULL;
}

static uint32_t
mlx5_hws_cnt_svc(void *opaque)
{
	struct mlx5_dev_ctx_shared *sh =
		(struct mlx5_dev_ctx_shared *)opaque;
	uint64_t interval =
		(uint64_t)sh->cnt_svc->query_interval * (US_PER_S / MS_PER_S);
	struct mlx5_hws_cnt_pool *hws_cpool;
	uint64_t start_cycle, query_cycle = 0;
	uint64_t query_us;
	uint64_t sleep_us;

	while (sh->cnt_svc->svc_running != 0) {
		if (rte_spinlock_trylock(&sh->cpool_lock) == 0)
			continue;
		start_cycle = rte_rdtsc();
		/* 200ms for 16M counters. */
		LIST_FOREACH(hws_cpool, &sh->hws_cpool_list, next) {
			struct mlx5_priv *opriv = hws_cpool->priv;

			__mlx5_hws_cnt_svc(sh, hws_cpool);
			if (opriv->hws_age_req)
				mlx5_hws_aging_check(opriv, hws_cpool);
		}
		query_cycle = rte_rdtsc() - start_cycle;
		rte_spinlock_unlock(&sh->cpool_lock);
		query_us = query_cycle / (rte_get_timer_hz() / US_PER_S);
		sleep_us = interval - query_us;
		if (interval > query_us)
			rte_delay_us_sleep(sleep_us);
	}
	return 0;
}

static void
mlx5_hws_cnt_pool_deinit(struct mlx5_hws_cnt_pool * const cntp)
{
	uint32_t qidx = 0;
	if (cntp == NULL)
		return;
	rte_ring_free(cntp->free_list);
	rte_ring_free(cntp->wait_reset_list);
	rte_ring_free(cntp->reuse_list);
	if (cntp->cache) {
		for (qidx = 0; qidx < cntp->cache->q_num; qidx++)
			rte_ring_free(cntp->cache->qcache[qidx]);
	}
	mlx5_free(cntp->cache);
	mlx5_free(cntp->raw_mng);
	mlx5_free(cntp->pool);
	mlx5_free(cntp);
}

static bool
mlx5_hws_cnt_should_enable_cache(const struct mlx5_hws_cnt_pool_cfg *pcfg,
				 const struct mlx5_hws_cache_param *ccfg)
{
	/*
	 * Enable cache if and only if there are enough counters requested
	 * to populate all of the caches.
	 */
	return pcfg->request_num >= ccfg->q_num * ccfg->size;
}

static struct mlx5_hws_cnt_pool_caches *
mlx5_hws_cnt_cache_init(const struct mlx5_hws_cnt_pool_cfg *pcfg,
			const struct mlx5_hws_cache_param *ccfg)
{
	struct mlx5_hws_cnt_pool_caches *cache;
	char mz_name[RTE_MEMZONE_NAMESIZE];
	uint32_t qidx;

	/* If counter pool is big enough, setup the counter pool cache. */
	cache = mlx5_malloc(MLX5_MEM_ANY | MLX5_MEM_ZERO,
			sizeof(*cache) +
			sizeof(((struct mlx5_hws_cnt_pool_caches *)0)->qcache[0])
				* ccfg->q_num, 0, SOCKET_ID_ANY);
	if (cache == NULL)
		return NULL;
	/* Store the necessary cache parameters. */
	cache->fetch_sz = ccfg->fetch_sz;
	cache->preload_sz = ccfg->preload_sz;
	cache->threshold = ccfg->threshold;
	cache->q_num = ccfg->q_num;
	for (qidx = 0; qidx < ccfg->q_num; qidx++) {
		snprintf(mz_name, sizeof(mz_name), "%s_qc/%x", pcfg->name, qidx);
		cache->qcache[qidx] = rte_ring_create(mz_name, ccfg->size,
				SOCKET_ID_ANY,
				RING_F_SP_ENQ | RING_F_SC_DEQ |
				RING_F_EXACT_SZ);
		if (cache->qcache[qidx] == NULL)
			goto error;
	}
	return cache;

error:
	while (qidx--)
		rte_ring_free(cache->qcache[qidx]);
	mlx5_free(cache);
	return NULL;
}

static struct mlx5_hws_cnt_pool *
mlx5_hws_cnt_pool_init(struct mlx5_dev_ctx_shared *sh,
		       const struct mlx5_hws_cnt_pool_cfg *pcfg,
		       const struct mlx5_hws_cache_param *ccfg)
{
	char mz_name[RTE_MEMZONE_NAMESIZE];
	struct mlx5_hws_cnt_pool *cntp;
	uint64_t cnt_num = 0;

	MLX5_ASSERT(pcfg);
	MLX5_ASSERT(ccfg);
	cntp = mlx5_malloc(MLX5_MEM_ANY | MLX5_MEM_ZERO, sizeof(*cntp), 0,
			   SOCKET_ID_ANY);
	if (cntp == NULL)
		return NULL;

	cntp->cfg = *pcfg;
	if (cntp->cfg.host_cpool)
		return cntp;
	if (pcfg->request_num > sh->hws_max_nb_counters) {
		DRV_LOG(ERR, "Counter number %u "
			"is greater than the maximum supported (%u).",
			pcfg->request_num, sh->hws_max_nb_counters);
		goto error;
	}
	cnt_num = pcfg->request_num * (100 + pcfg->alloc_factor) / 100;
	if (cnt_num > UINT32_MAX) {
		DRV_LOG(ERR, "counter number %"PRIu64" is out of 32bit range",
			cnt_num);
		goto error;
	}
	/*
	 * When counter request number is supported, but the factor takes it
	 * out of size, the factor is reduced.
	 */
	cnt_num = RTE_MIN((uint32_t)cnt_num, sh->hws_max_nb_counters);
	cntp->pool = mlx5_malloc(MLX5_MEM_ANY | MLX5_MEM_ZERO,
				 sizeof(struct mlx5_hws_cnt) * cnt_num,
				 0, SOCKET_ID_ANY);
	if (cntp->pool == NULL)
		goto error;
	snprintf(mz_name, sizeof(mz_name), "%s_F_RING", pcfg->name);
	cntp->free_list = rte_ring_create_elem(mz_name, sizeof(cnt_id_t),
				(uint32_t)cnt_num, SOCKET_ID_ANY,
				RING_F_MP_HTS_ENQ | RING_F_MC_HTS_DEQ |
				RING_F_EXACT_SZ);
	if (cntp->free_list == NULL) {
		DRV_LOG(ERR, "failed to create free list ring");
		goto error;
	}
	snprintf(mz_name, sizeof(mz_name), "%s_R_RING", pcfg->name);
	cntp->wait_reset_list = rte_ring_create_elem(mz_name, sizeof(cnt_id_t),
			(uint32_t)cnt_num, SOCKET_ID_ANY,
			RING_F_MP_HTS_ENQ | RING_F_SC_DEQ | RING_F_EXACT_SZ);
	if (cntp->wait_reset_list == NULL) {
		DRV_LOG(ERR, "failed to create free list ring");
		goto error;
	}
	snprintf(mz_name, sizeof(mz_name), "%s_U_RING", pcfg->name);
	cntp->reuse_list = rte_ring_create_elem(mz_name, sizeof(cnt_id_t),
			(uint32_t)cnt_num, SOCKET_ID_ANY,
			RING_F_MP_HTS_ENQ | RING_F_MC_HTS_DEQ | RING_F_EXACT_SZ);
	if (cntp->reuse_list == NULL) {
		DRV_LOG(ERR, "failed to create reuse list ring");
		goto error;
	}
	/* Allocate counter cache only if needed. */
	if (mlx5_hws_cnt_should_enable_cache(pcfg, ccfg)) {
		cntp->cache = mlx5_hws_cnt_cache_init(pcfg, ccfg);
		if (cntp->cache == NULL)
			goto error;
	}
	/* Initialize the time for aging-out calculation. */
	cntp->time_of_last_age_check = MLX5_CURR_TIME_SEC;
	return cntp;
error:
	mlx5_hws_cnt_pool_deinit(cntp);
	return NULL;
}

int
mlx5_hws_cnt_service_thread_create(struct mlx5_dev_ctx_shared *sh)
{
	char name[RTE_THREAD_INTERNAL_NAME_SIZE];
	rte_thread_attr_t attr;
	int ret;
	uint32_t service_core = sh->cnt_svc->service_core;

	ret = rte_thread_attr_init(&attr);
	if (ret != 0)
		goto error;
	CPU_SET(service_core, &attr.cpuset);
	sh->cnt_svc->svc_running = 1;
	ret = rte_thread_create(&sh->cnt_svc->service_thread,
			&attr, mlx5_hws_cnt_svc, sh);
	if (ret != 0)
		goto error;
	snprintf(name, sizeof(name), "mlx5-cn%d", service_core);
	rte_thread_set_prefixed_name(sh->cnt_svc->service_thread, name);

	return 0;
error:
	DRV_LOG(ERR, "Failed to create HW steering's counter service thread.");
	return ret;
}

void
mlx5_hws_cnt_service_thread_destroy(struct mlx5_dev_ctx_shared *sh)
{
	if (sh->cnt_svc->service_thread.opaque_id == 0)
		return;
	sh->cnt_svc->svc_running = 0;
	rte_thread_join(sh->cnt_svc->service_thread, NULL);
	sh->cnt_svc->service_thread.opaque_id = 0;
}

static int
mlx5_hws_cnt_pool_dcs_alloc(struct mlx5_dev_ctx_shared *sh,
			    struct mlx5_hws_cnt_pool *cpool)
{
	struct mlx5_hca_attr *hca_attr = &sh->cdev->config.hca_attr;
	uint32_t max_log_bulk_sz = sh->hws_max_log_bulk_sz;
	uint32_t log_bulk_sz;
	uint32_t idx, alloc_candidate, alloced = 0;
	unsigned int cnt_num = mlx5_hws_cnt_pool_get_size(cpool);
	struct mlx5_devx_counter_attr attr = {0};
	struct mlx5_devx_obj *dcs;

	MLX5_ASSERT(cpool->cfg.host_cpool == NULL);
	if (hca_attr->flow_counter_bulk_log_max_alloc == 0) {
		DRV_LOG(ERR, "Fw doesn't support bulk log max alloc");
		return -1;
	}
	cnt_num = RTE_ALIGN_CEIL(cnt_num, 4); /* minimal 4 counter in bulk. */
	log_bulk_sz = RTE_MIN(max_log_bulk_sz, rte_log2_u32(cnt_num));
	attr.pd = sh->cdev->pdn;
	attr.pd_valid = 1;
	attr.bulk_log_max_alloc = 1;
	attr.flow_counter_bulk_log_size = log_bulk_sz;
	idx = 0;
	dcs = mlx5_devx_cmd_flow_counter_alloc_general(sh->cdev->ctx, &attr);
	if (dcs == NULL)
		goto error;
	cpool->dcs_mng.dcs[idx].obj = dcs;
	cpool->dcs_mng.dcs[idx].batch_sz = (1 << log_bulk_sz);
	cpool->dcs_mng.batch_total++;
	idx++;
	cpool->dcs_mng.dcs[0].iidx = 0;
	alloced = cpool->dcs_mng.dcs[0].batch_sz;
	if (cnt_num > cpool->dcs_mng.dcs[0].batch_sz) {
		while (idx < MLX5_HWS_CNT_DCS_NUM) {
			attr.flow_counter_bulk_log_size = --max_log_bulk_sz;
			alloc_candidate = RTE_BIT32(max_log_bulk_sz);
			if (alloced + alloc_candidate > sh->hws_max_nb_counters)
				continue;
			dcs = mlx5_devx_cmd_flow_counter_alloc_general
				(sh->cdev->ctx, &attr);
			if (dcs == NULL)
				goto error;
			cpool->dcs_mng.dcs[idx].obj = dcs;
			cpool->dcs_mng.dcs[idx].batch_sz = alloc_candidate;
			cpool->dcs_mng.dcs[idx].iidx = alloced;
			alloced += cpool->dcs_mng.dcs[idx].batch_sz;
			cpool->dcs_mng.batch_total++;
			if (alloced >= cnt_num)
				break;
			idx++;
		}
	}
	return 0;
error:
	DRV_LOG(DEBUG,
		"Cannot alloc device counter, allocated[%" PRIu32 "] request[%" PRIu32 "]",
		alloced, cnt_num);
	for (idx = 0; idx < cpool->dcs_mng.batch_total; idx++) {
		mlx5_devx_cmd_destroy(cpool->dcs_mng.dcs[idx].obj);
		cpool->dcs_mng.dcs[idx].obj = NULL;
		cpool->dcs_mng.dcs[idx].batch_sz = 0;
		cpool->dcs_mng.dcs[idx].iidx = 0;
	}
	cpool->dcs_mng.batch_total = 0;
	return -1;
}

static void
mlx5_hws_cnt_pool_dcs_free(struct mlx5_dev_ctx_shared *sh,
			   struct mlx5_hws_cnt_pool *cpool)
{
	uint32_t idx;

	if (cpool == NULL)
		return;
	for (idx = 0; idx < MLX5_HWS_CNT_DCS_NUM; idx++)
		mlx5_devx_cmd_destroy(cpool->dcs_mng.dcs[idx].obj);
	if (cpool->raw_mng) {
		mlx5_hws_cnt_raw_data_free(sh, cpool->raw_mng);
		cpool->raw_mng = NULL;
	}
}

static void
mlx5_hws_cnt_pool_action_destroy(struct mlx5_hws_cnt_pool *cpool)
{
	uint32_t idx;

	for (idx = 0; idx < cpool->dcs_mng.batch_total; idx++) {
		struct mlx5_hws_cnt_dcs *dcs = &cpool->dcs_mng.dcs[idx];

		if (dcs->dr_action != NULL) {
			mlx5dr_action_destroy(dcs->dr_action);
			dcs->dr_action = NULL;
		}
	}
}

static int
mlx5_hws_cnt_pool_action_create(struct mlx5_priv *priv,
		struct mlx5_hws_cnt_pool *cpool)
{
	struct mlx5_hws_cnt_pool *hpool = mlx5_hws_cnt_host_pool(cpool);
	uint32_t idx;
	int ret = 0;
	uint32_t flags;

	flags = MLX5DR_ACTION_FLAG_HWS_RX | MLX5DR_ACTION_FLAG_HWS_TX;
	if (priv->sh->config.dv_esw_en && priv->master)
		flags |= MLX5DR_ACTION_FLAG_HWS_FDB;
	for (idx = 0; idx < hpool->dcs_mng.batch_total; idx++) {
		struct mlx5_hws_cnt_dcs *hdcs = &hpool->dcs_mng.dcs[idx];
		struct mlx5_hws_cnt_dcs *dcs = &cpool->dcs_mng.dcs[idx];

		dcs->dr_action = mlx5dr_action_create_counter(priv->dr_ctx,
					(struct mlx5dr_devx_obj *)hdcs->obj,
					flags);
		if (dcs->dr_action == NULL) {
			mlx5_hws_cnt_pool_action_destroy(cpool);
			ret = -ENOSYS;
			break;
		}
	}
	return ret;
}

struct mlx5_hws_cnt_pool *
mlx5_hws_cnt_pool_create(struct rte_eth_dev *dev,
		const struct rte_flow_port_attr *pattr, uint16_t nb_queue)
{
	struct mlx5_hws_cnt_pool *cpool = NULL;
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_hws_cache_param cparam = {0};
	struct mlx5_hws_cnt_pool_cfg pcfg = {0};
	char *mp_name;
	int ret = 0;
	size_t sz;

	mp_name = mlx5_malloc(MLX5_MEM_ZERO, RTE_MEMZONE_NAMESIZE, 0, SOCKET_ID_ANY);
	if (mp_name == NULL)
		goto error;
	snprintf(mp_name, RTE_MEMZONE_NAMESIZE, "MLX5_HWS_CNT_P_%x", dev->data->port_id);
	pcfg.name = mp_name;
	pcfg.request_num = pattr->nb_counters;
	pcfg.alloc_factor = HWS_CNT_ALLOC_FACTOR_DEFAULT;
	if (pattr->flags & RTE_FLOW_PORT_FLAG_SHARE_INDIRECT) {
		struct mlx5_priv *host_priv =
				priv->shared_host->data->dev_private;
		struct mlx5_hws_cnt_pool *chost = host_priv->hws_cpool;

		pcfg.host_cpool = chost;
		cpool = mlx5_hws_cnt_pool_init(priv->sh, &pcfg, &cparam);
		if (cpool == NULL)
			goto error;
		ret = mlx5_hws_cnt_pool_action_create(priv, cpool);
		if (ret != 0)
			goto error;
		return cpool;
	}
	/* init cnt service if not. */
	if (priv->sh->cnt_svc == NULL) {
		ret = mlx5_hws_cnt_svc_init(priv->sh);
		if (ret != 0)
			return NULL;
	}
	cparam.fetch_sz = HWS_CNT_CACHE_FETCH_DEFAULT;
	cparam.preload_sz = HWS_CNT_CACHE_PRELOAD_DEFAULT;
	cparam.q_num = nb_queue;
	cparam.threshold = HWS_CNT_CACHE_THRESHOLD_DEFAULT;
	cparam.size = HWS_CNT_CACHE_SZ_DEFAULT;
	cpool = mlx5_hws_cnt_pool_init(priv->sh, &pcfg, &cparam);
	if (cpool == NULL)
		goto error;
	ret = mlx5_hws_cnt_pool_dcs_alloc(priv->sh, cpool);
	if (ret != 0)
		goto error;
	sz = RTE_ALIGN_CEIL(mlx5_hws_cnt_pool_get_size(cpool), 4);
	cpool->raw_mng = mlx5_hws_cnt_raw_data_alloc(priv->sh, sz);
	if (cpool->raw_mng == NULL)
		goto error;
	__hws_cnt_id_load(cpool);
	/*
	 * Bump query gen right after pool create so the
	 * pre-loaded counters can be used directly
	 * because they already have init value no need
	 * to wait for query.
	 */
	cpool->query_gen = 1;
	ret = mlx5_hws_cnt_pool_action_create(priv, cpool);
	if (ret != 0)
		goto error;
	priv->sh->cnt_svc->refcnt++;
	cpool->priv = priv;
	rte_spinlock_lock(&priv->sh->cpool_lock);
	LIST_INSERT_HEAD(&priv->sh->hws_cpool_list, cpool, next);
	rte_spinlock_unlock(&priv->sh->cpool_lock);
	return cpool;
error:
	mlx5_hws_cnt_pool_destroy(priv->sh, cpool);
	return NULL;
}

void
mlx5_hws_cnt_pool_destroy(struct mlx5_dev_ctx_shared *sh,
		struct mlx5_hws_cnt_pool *cpool)
{
	if (cpool == NULL)
		return;
	/*
	 * 16M counter consumes 200ms to finish the query.
	 * Maybe blocked for at most 200ms here.
	 */
	rte_spinlock_lock(&sh->cpool_lock);
	LIST_REMOVE(cpool, next);
	rte_spinlock_unlock(&sh->cpool_lock);
	if (cpool->cfg.host_cpool == NULL) {
		if (--sh->cnt_svc->refcnt == 0)
			mlx5_hws_cnt_svc_deinit(sh);
	}
	mlx5_hws_cnt_pool_action_destroy(cpool);
	if (cpool->cfg.host_cpool == NULL) {
		mlx5_hws_cnt_pool_dcs_free(sh, cpool);
		mlx5_hws_cnt_raw_data_free(sh, cpool->raw_mng);
	}
	mlx5_free((void *)cpool->cfg.name);
	mlx5_hws_cnt_pool_deinit(cpool);
}

int
mlx5_hws_cnt_svc_init(struct mlx5_dev_ctx_shared *sh)
{
	int ret;

	sh->cnt_svc = mlx5_malloc(MLX5_MEM_ANY | MLX5_MEM_ZERO,
			sizeof(*sh->cnt_svc), 0, SOCKET_ID_ANY);
	if (sh->cnt_svc == NULL)
		return -1;
	sh->cnt_svc->query_interval = sh->config.cnt_svc.cycle_time;
	sh->cnt_svc->service_core = sh->config.cnt_svc.service_core;
	ret = mlx5_aso_cnt_queue_init(sh);
	if (ret != 0) {
		mlx5_free(sh->cnt_svc);
		sh->cnt_svc = NULL;
		return -1;
	}
	ret = mlx5_hws_cnt_service_thread_create(sh);
	if (ret != 0) {
		mlx5_aso_cnt_queue_uninit(sh);
		mlx5_free(sh->cnt_svc);
		sh->cnt_svc = NULL;
	}
	return 0;
}

void
mlx5_hws_cnt_svc_deinit(struct mlx5_dev_ctx_shared *sh)
{
	if (sh->cnt_svc == NULL)
		return;
	mlx5_hws_cnt_service_thread_destroy(sh);
	mlx5_aso_cnt_queue_uninit(sh);
	mlx5_free(sh->cnt_svc);
	sh->cnt_svc = NULL;
}

/**
 * Destroy AGE action.
 *
 * @param priv
 *   Pointer to the port private data structure.
 * @param idx
 *   Index of AGE parameter.
 * @param error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_hws_age_action_destroy(struct mlx5_priv *priv, uint32_t idx,
			    struct rte_flow_error *error)
{
	struct mlx5_age_info *age_info = GET_PORT_AGE_INFO(priv);
	struct mlx5_indexed_pool *ipool = age_info->ages_ipool;
	struct mlx5_hws_age_param *param = mlx5_ipool_get(ipool, idx);

	if (param == NULL)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "invalid AGE parameter index");
	switch (__atomic_exchange_n(&param->state, HWS_AGE_FREE,
				    __ATOMIC_RELAXED)) {
	case HWS_AGE_CANDIDATE:
	case HWS_AGE_AGED_OUT_REPORTED:
		mlx5_hws_age_param_free(priv, param->own_cnt_index, ipool, idx);
		break;
	case HWS_AGE_AGED_OUT_NOT_REPORTED:
	case HWS_AGE_CANDIDATE_INSIDE_RING:
		/*
		 * In both cases AGE is inside the ring. Change the state here
		 * and destroy it later when it is taken out of ring.
		 */
		break;
	case HWS_AGE_FREE:
		/*
		 * If index is valid and state is FREE, it says this AGE has
		 * been freed for the user but not for the PMD since it is
		 * inside the ring.
		 */
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "this AGE has already been released");
	default:
		MLX5_ASSERT(0);
		break;
	}
	return 0;
}

/**
 * Create AGE action parameter.
 *
 * @param[in] priv
 *   Pointer to the port private data structure.
 * @param[in] queue_id
 *   Which HWS queue to be used.
 * @param[in] shared
 *   Whether it indirect AGE action.
 * @param[in] flow_idx
 *   Flow index from indexed pool.
 *   For indirect AGE action it doesn't affect.
 * @param[in] age
 *   Pointer to the aging action configuration.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   Index to AGE action parameter on success, 0 otherwise.
 */
uint32_t
mlx5_hws_age_action_create(struct mlx5_priv *priv, uint32_t queue_id,
			   bool shared, const struct rte_flow_action_age *age,
			   uint32_t flow_idx, struct rte_flow_error *error)
{
	struct mlx5_age_info *age_info = GET_PORT_AGE_INFO(priv);
	struct mlx5_indexed_pool *ipool = age_info->ages_ipool;
	struct mlx5_hws_age_param *param;
	uint32_t age_idx;

	param = mlx5_ipool_malloc(ipool, &age_idx);
	if (param == NULL) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "cannot allocate AGE parameter");
		return 0;
	}
	MLX5_ASSERT(__atomic_load_n(&param->state,
				    __ATOMIC_RELAXED) == HWS_AGE_FREE);
	if (shared) {
		param->nb_cnts = 0;
		param->accumulator_hits = 0;
		param->accumulator_cnt = 0;
		flow_idx = age_idx;
	} else {
		param->nb_cnts = 1;
	}
	param->context = age->context ? age->context :
					(void *)(uintptr_t)flow_idx;
	param->timeout = age->timeout;
	param->queue_id = queue_id;
	param->accumulator_last_hits = 0;
	param->own_cnt_index = 0;
	param->sec_since_last_hit = 0;
	param->state = HWS_AGE_CANDIDATE;
	return age_idx;
}

/**
 * Update indirect AGE action parameter.
 *
 * @param[in] priv
 *   Pointer to the port private data structure.
 * @param[in] idx
 *   Index of AGE parameter.
 * @param[in] update
 *   Update value.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_hws_age_action_update(struct mlx5_priv *priv, uint32_t idx,
			   const void *update, struct rte_flow_error *error)
{
	const struct rte_flow_update_age *update_ade = update;
	struct mlx5_age_info *age_info = GET_PORT_AGE_INFO(priv);
	struct mlx5_indexed_pool *ipool = age_info->ages_ipool;
	struct mlx5_hws_age_param *param = mlx5_ipool_get(ipool, idx);
	bool sec_since_last_hit_reset = false;
	bool state_update = false;

	if (param == NULL)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "invalid AGE parameter index");
	if (update_ade->timeout_valid) {
		uint32_t old_timeout = __atomic_exchange_n(&param->timeout,
							   update_ade->timeout,
							   __ATOMIC_RELAXED);

		if (old_timeout == 0)
			sec_since_last_hit_reset = true;
		else if (old_timeout < update_ade->timeout ||
			 update_ade->timeout == 0)
			/*
			 * When timeout is increased, aged-out flows might be
			 * active again and state should be updated accordingly.
			 * When new timeout is 0, we update the state for not
			 * reporting aged-out stopped.
			 */
			state_update = true;
	}
	if (update_ade->touch) {
		sec_since_last_hit_reset = true;
		state_update = true;
	}
	if (sec_since_last_hit_reset)
		__atomic_store_n(&param->sec_since_last_hit, 0,
				 __ATOMIC_RELAXED);
	if (state_update) {
		uint16_t expected = HWS_AGE_AGED_OUT_NOT_REPORTED;

		/*
		 * Change states of aged-out flows to active:
		 *  - AGED_OUT_NOT_REPORTED -> CANDIDATE_INSIDE_RING
		 *  - AGED_OUT_REPORTED -> CANDIDATE
		 */
		if (!__atomic_compare_exchange_n(&param->state, &expected,
						 HWS_AGE_CANDIDATE_INSIDE_RING,
						 false, __ATOMIC_RELAXED,
						 __ATOMIC_RELAXED) &&
		    expected == HWS_AGE_AGED_OUT_REPORTED)
			__atomic_store_n(&param->state, HWS_AGE_CANDIDATE,
					 __ATOMIC_RELAXED);
	}
	return 0;
}

/**
 * Get the AGE context if the aged-out index is still valid.
 *
 * @param priv
 *   Pointer to the port private data structure.
 * @param idx
 *   Index of AGE parameter.
 *
 * @return
 *   AGE context if the index is still aged-out, NULL otherwise.
 */
void *
mlx5_hws_age_context_get(struct mlx5_priv *priv, uint32_t idx)
{
	struct mlx5_age_info *age_info = GET_PORT_AGE_INFO(priv);
	struct mlx5_indexed_pool *ipool = age_info->ages_ipool;
	struct mlx5_hws_age_param *param = mlx5_ipool_get(ipool, idx);
	uint16_t expected = HWS_AGE_AGED_OUT_NOT_REPORTED;

	MLX5_ASSERT(param != NULL);
	if (__atomic_compare_exchange_n(&param->state, &expected,
					HWS_AGE_AGED_OUT_REPORTED, false,
					__ATOMIC_RELAXED, __ATOMIC_RELAXED))
		return param->context;
	switch (expected) {
	case HWS_AGE_FREE:
		/*
		 * This AGE couldn't have been destroyed since it was inside
		 * the ring. Its state has updated, and now it is actually
		 * destroyed.
		 */
		mlx5_hws_age_param_free(priv, param->own_cnt_index, ipool, idx);
		break;
	case HWS_AGE_CANDIDATE_INSIDE_RING:
		__atomic_store_n(&param->state, HWS_AGE_CANDIDATE,
				 __ATOMIC_RELAXED);
		break;
	case HWS_AGE_CANDIDATE:
		/*
		 * Only BG thread pushes to ring and it never pushes this state.
		 * When AGE inside the ring becomes candidate, it has a special
		 * state called HWS_AGE_CANDIDATE_INSIDE_RING.
		 * Fall-through.
		 */
	case HWS_AGE_AGED_OUT_REPORTED:
		/*
		 * Only this thread (doing query) may write this state, and it
		 * happens only after the query thread takes it out of the ring.
		 * Fall-through.
		 */
	case HWS_AGE_AGED_OUT_NOT_REPORTED:
		/*
		 * In this case the compare return true and function return
		 * the context immediately.
		 * Fall-through.
		 */
	default:
		MLX5_ASSERT(0);
		break;
	}
	return NULL;
}

#ifdef RTE_ARCH_64
#define MLX5_HWS_AGED_OUT_RING_SIZE_MAX UINT32_MAX
#else
#define MLX5_HWS_AGED_OUT_RING_SIZE_MAX RTE_BIT32(8)
#endif

/**
 * Get the size of aged out ring list for each queue.
 *
 * The size is one percent of nb_counters divided by nb_queues.
 * The ring size must be power of 2, so it align up to power of 2.
 * In 32 bit systems, the size is limited by 256.
 *
 * This function is called when RTE_FLOW_PORT_FLAG_STRICT_QUEUE is on.
 *
 * @param nb_counters
 *   Final number of allocated counter in the pool.
 * @param nb_queues
 *   Number of HWS queues in this port.
 *
 * @return
 *   Size of aged out ring per queue.
 */
static __rte_always_inline uint32_t
mlx5_hws_aged_out_q_ring_size_get(uint32_t nb_counters, uint32_t nb_queues)
{
	uint32_t size = rte_align32pow2((nb_counters / 100) / nb_queues);
	uint32_t max_size = MLX5_HWS_AGED_OUT_RING_SIZE_MAX;

	return RTE_MIN(size, max_size);
}

/**
 * Get the size of the aged out ring list.
 *
 * The size is one percent of nb_counters.
 * The ring size must be power of 2, so it align up to power of 2.
 * In 32 bit systems, the size is limited by 256.
 *
 * This function is called when RTE_FLOW_PORT_FLAG_STRICT_QUEUE is off.
 *
 * @param nb_counters
 *   Final number of allocated counter in the pool.
 *
 * @return
 *   Size of the aged out ring list.
 */
static __rte_always_inline uint32_t
mlx5_hws_aged_out_ring_size_get(uint32_t nb_counters)
{
	uint32_t size = rte_align32pow2(nb_counters / 100);
	uint32_t max_size = MLX5_HWS_AGED_OUT_RING_SIZE_MAX;

	return RTE_MIN(size, max_size);
}

/**
 * Initialize the shared aging list information per port.
 *
 * @param dev
 *   Pointer to the rte_eth_dev structure.
 * @param nb_queues
 *   Number of HWS queues.
 * @param strict_queue
 *   Indicator whether is strict_queue mode.
 * @param ring_size
 *   Size of aged-out ring for creation.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_hws_age_info_init(struct rte_eth_dev *dev, uint16_t nb_queues,
		       bool strict_queue, uint32_t ring_size)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_age_info *age_info = GET_PORT_AGE_INFO(priv);
	uint32_t flags = RING_F_SP_ENQ | RING_F_SC_DEQ | RING_F_EXACT_SZ;
	char mz_name[RTE_MEMZONE_NAMESIZE];
	struct rte_ring *r = NULL;
	uint32_t qidx;

	age_info->flags = 0;
	if (strict_queue) {
		size_t size = sizeof(*age_info->hw_q_age) +
			      sizeof(struct rte_ring *) * nb_queues;

		age_info->hw_q_age = mlx5_malloc(MLX5_MEM_ANY | MLX5_MEM_ZERO,
						 size, 0, SOCKET_ID_ANY);
		if (age_info->hw_q_age == NULL)
			return -ENOMEM;
		for (qidx = 0; qidx < nb_queues; ++qidx) {
			snprintf(mz_name, sizeof(mz_name),
				 "port_%u_queue_%u_aged_out_ring",
				 dev->data->port_id, qidx);
			r = rte_ring_create(mz_name, ring_size, SOCKET_ID_ANY,
					    flags);
			if (r == NULL) {
				DRV_LOG(ERR, "\"%s\" creation failed: %s",
					mz_name, rte_strerror(rte_errno));
				goto error;
			}
			age_info->hw_q_age->aged_lists[qidx] = r;
			DRV_LOG(DEBUG,
				"\"%s\" is successfully created (size=%u).",
				mz_name, ring_size);
		}
		age_info->hw_q_age->nb_rings = nb_queues;
	} else {
		snprintf(mz_name, sizeof(mz_name), "port_%u_aged_out_ring",
			 dev->data->port_id);
		r = rte_ring_create(mz_name, ring_size, SOCKET_ID_ANY, flags);
		if (r == NULL) {
			DRV_LOG(ERR, "\"%s\" creation failed: %s", mz_name,
				rte_strerror(rte_errno));
			return -rte_errno;
		}
		age_info->hw_age.aged_list = r;
		DRV_LOG(DEBUG, "\"%s\" is successfully created (size=%u).",
			mz_name, ring_size);
		/* In non "strict_queue" mode, initialize the event. */
		MLX5_AGE_SET(age_info, MLX5_AGE_TRIGGER);
	}
	return 0;
error:
	MLX5_ASSERT(strict_queue);
	while (qidx--)
		rte_ring_free(age_info->hw_q_age->aged_lists[qidx]);
	mlx5_free(age_info->hw_q_age);
	return -1;
}

/**
 * Cleanup aged-out ring before destroying.
 *
 * @param priv
 *   Pointer to port private object.
 * @param r
 *   Pointer to aged-out ring object.
 */
static void
mlx5_hws_aged_out_ring_cleanup(struct mlx5_priv *priv, struct rte_ring *r)
{
	int ring_size = rte_ring_count(r);

	while (ring_size > 0) {
		uint32_t age_idx = 0;

		if (rte_ring_dequeue_elem(r, &age_idx, sizeof(uint32_t)) < 0)
			break;
		/* get the AGE context if the aged-out index is still valid. */
		mlx5_hws_age_context_get(priv, age_idx);
		ring_size--;
	}
	rte_ring_free(r);
}

/**
 * Destroy the shared aging list information per port.
 *
 * @param priv
 *   Pointer to port private object.
 */
static void
mlx5_hws_age_info_destroy(struct mlx5_priv *priv)
{
	struct mlx5_age_info *age_info = GET_PORT_AGE_INFO(priv);
	uint16_t nb_queues = age_info->hw_q_age->nb_rings;
	struct rte_ring *r;

	if (priv->hws_strict_queue) {
		uint32_t qidx;

		for (qidx = 0; qidx < nb_queues; ++qidx) {
			r = age_info->hw_q_age->aged_lists[qidx];
			mlx5_hws_aged_out_ring_cleanup(priv, r);
		}
		mlx5_free(age_info->hw_q_age);
	} else {
		r = age_info->hw_age.aged_list;
		mlx5_hws_aged_out_ring_cleanup(priv, r);
	}
}

/**
 * Initialize the aging mechanism per port.
 *
 * @param dev
 *   Pointer to the rte_eth_dev structure.
 * @param attr
 *   Port configuration attributes.
 * @param nb_queues
 *   Number of HWS queues.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_hws_age_pool_init(struct rte_eth_dev *dev,
		       const struct rte_flow_port_attr *attr,
		       uint16_t nb_queues)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_age_info *age_info = GET_PORT_AGE_INFO(priv);
	struct mlx5_indexed_pool_config cfg = {
		.size =
		      RTE_CACHE_LINE_ROUNDUP(sizeof(struct mlx5_hws_age_param)),
		.trunk_size = 1 << 12,
		.per_core_cache = 1 << 13,
		.need_lock = 1,
		.release_mem_en = !!priv->sh->config.reclaim_mode,
		.malloc = mlx5_malloc,
		.free = mlx5_free,
		.type = "mlx5_hws_age_pool",
	};
	bool strict_queue = false;
	uint32_t nb_alloc_cnts;
	uint32_t rsize;
	uint32_t nb_ages_updated;
	int ret;

	strict_queue = !!(attr->flags & RTE_FLOW_PORT_FLAG_STRICT_QUEUE);
	MLX5_ASSERT(priv->hws_cpool);
	if (attr->flags & RTE_FLOW_PORT_FLAG_SHARE_INDIRECT) {
		DRV_LOG(ERR, "Aging sn not supported "
			     "in cross vHCA sharing mode");
		rte_errno = ENOTSUP;
		return -ENOTSUP;
	}
	nb_alloc_cnts = mlx5_hws_cnt_pool_get_size(priv->hws_cpool);
	if (strict_queue) {
		rsize = mlx5_hws_aged_out_q_ring_size_get(nb_alloc_cnts,
							  nb_queues);
		nb_ages_updated = rsize * nb_queues + attr->nb_aging_objects;
	} else {
		rsize = mlx5_hws_aged_out_ring_size_get(nb_alloc_cnts);
		nb_ages_updated = rsize + attr->nb_aging_objects;
	}
	ret = mlx5_hws_age_info_init(dev, nb_queues, strict_queue, rsize);
	if (ret < 0)
		return ret;
	cfg.max_idx = rte_align32pow2(nb_ages_updated);
	if (cfg.max_idx <= cfg.trunk_size) {
		cfg.per_core_cache = 0;
		cfg.trunk_size = cfg.max_idx;
	} else if (cfg.max_idx <= MLX5_HW_IPOOL_SIZE_THRESHOLD) {
		cfg.per_core_cache = MLX5_HW_IPOOL_CACHE_MIN;
	}
	age_info->ages_ipool = mlx5_ipool_create(&cfg);
	if (age_info->ages_ipool == NULL) {
		mlx5_hws_age_info_destroy(priv);
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	priv->hws_age_req = 1;
	return 0;
}

/**
 * Cleanup all aging resources per port.
 *
 * @param priv
 *   Pointer to port private object.
 */
void
mlx5_hws_age_pool_destroy(struct mlx5_priv *priv)
{
	struct mlx5_age_info *age_info = GET_PORT_AGE_INFO(priv);

	rte_spinlock_lock(&priv->sh->cpool_lock);
	MLX5_ASSERT(priv->hws_age_req);
	mlx5_hws_age_info_destroy(priv);
	mlx5_ipool_destroy(age_info->ages_ipool);
	age_info->ages_ipool = NULL;
	priv->hws_age_req = 0;
	rte_spinlock_unlock(&priv->sh->cpool_lock);
}

#endif
