/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#include <stdint.h>
#include <rte_malloc.h>
#include <mlx5_malloc.h>
#include <rte_ring.h>
#include <mlx5_devx_cmds.h>
#include <rte_cycles.h>

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
	uint32_t preload;
	uint32_t q_num = cpool->cache->q_num;
	uint32_t cnt_num = mlx5_hws_cnt_pool_get_size(cpool);
	cnt_id_t cnt_id, iidx = 0;
	uint32_t qidx;
	struct rte_ring *qcache = NULL;

	/*
	 * Counter ID order is important for tracking the max number of in used
	 * counter for querying, which means counter internal index order must
	 * be from zero to the number user configured, i.e: 0 - 8000000.
	 * Need to load counter ID in this order into the cache firstly,
	 * and then the global free list.
	 * In the end, user fetch the counter from minimal to the maximum.
	 */
	preload = RTE_MIN(cpool->cache->preload_sz, cnt_num / q_num);
	for (qidx = 0; qidx < q_num; qidx++) {
		for (; iidx < preload * (qidx + 1); iidx++) {
			cnt_id = mlx5_hws_cnt_id_gen(cpool, iidx);
			qcache = cpool->cache->qcache[qidx];
			if (qcache)
				rte_ring_enqueue_elem(qcache, &cnt_id,
						sizeof(cnt_id));
		}
	}
	for (; iidx < cnt_num; iidx++) {
		cnt_id = mlx5_hws_cnt_id_gen(cpool, iidx);
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

	reset_cnt_num = rte_ring_count(reset_list);
	do {
		cpool->query_gen++;
		mlx5_aso_cnt_query(sh, cpool);
		zcdr.n1 = 0;
		zcdu.n1 = 0;
		rte_ring_enqueue_zc_burst_elem_start(reuse_list,
				sizeof(cnt_id_t), reset_cnt_num, &zcdu,
				NULL);
		rte_ring_dequeue_zc_burst_elem_start(reset_list,
				sizeof(cnt_id_t), reset_cnt_num, &zcdr,
				NULL);
		__hws_cnt_r2rcpy(&zcdu, &zcdr, reset_cnt_num);
		rte_ring_dequeue_zc_elem_finish(reset_list,
				reset_cnt_num);
		rte_ring_enqueue_zc_elem_finish(reuse_list,
				reset_cnt_num);
		reset_cnt_num = rte_ring_count(reset_list);
	} while (reset_cnt_num > 0);
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

	mng = mlx5_malloc(MLX5_MEM_ANY | MLX5_MEM_ZERO, sizeof(*mng), 0,
			SOCKET_ID_ANY);
	if (mng == NULL)
		goto error;
	mng->raw = mlx5_malloc(MLX5_MEM_ANY | MLX5_MEM_ZERO, sz, 0,
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

static void *
mlx5_hws_cnt_svc(void *opaque)
{
	struct mlx5_dev_ctx_shared *sh =
		(struct mlx5_dev_ctx_shared *)opaque;
	uint64_t interval =
		(uint64_t)sh->cnt_svc->query_interval * (US_PER_S / MS_PER_S);
	uint16_t port_id;
	uint64_t start_cycle, query_cycle = 0;
	uint64_t query_us;
	uint64_t sleep_us;

	while (sh->cnt_svc->svc_running != 0) {
		start_cycle = rte_rdtsc();
		MLX5_ETH_FOREACH_DEV(port_id, sh->cdev->dev) {
			struct mlx5_priv *opriv =
				rte_eth_devices[port_id].data->dev_private;
			if (opriv != NULL &&
			    opriv->sh == sh &&
			    opriv->hws_cpool != NULL) {
				__mlx5_hws_cnt_svc(sh, opriv->hws_cpool);
			}
		}
		query_cycle = rte_rdtsc() - start_cycle;
		query_us = query_cycle / (rte_get_timer_hz() / US_PER_S);
		sleep_us = interval - query_us;
		if (interval > query_us)
			rte_delay_us_sleep(sleep_us);
	}
	return NULL;
}

struct mlx5_hws_cnt_pool *
mlx5_hws_cnt_pool_init(const struct mlx5_hws_cnt_pool_cfg *pcfg,
		const struct mlx5_hws_cache_param *ccfg)
{
	char mz_name[RTE_MEMZONE_NAMESIZE];
	struct mlx5_hws_cnt_pool *cntp;
	uint64_t cnt_num = 0;
	uint32_t qidx;

	MLX5_ASSERT(pcfg);
	MLX5_ASSERT(ccfg);
	cntp = mlx5_malloc(MLX5_MEM_ANY | MLX5_MEM_ZERO, sizeof(*cntp), 0,
			   SOCKET_ID_ANY);
	if (cntp == NULL)
		return NULL;

	cntp->cfg = *pcfg;
	cntp->cache = mlx5_malloc(MLX5_MEM_ANY | MLX5_MEM_ZERO,
			sizeof(*cntp->cache) +
			sizeof(((struct mlx5_hws_cnt_pool_caches *)0)->qcache[0])
				* ccfg->q_num, 0, SOCKET_ID_ANY);
	if (cntp->cache == NULL)
		goto error;
	 /* store the necessary cache parameters. */
	cntp->cache->fetch_sz = ccfg->fetch_sz;
	cntp->cache->preload_sz = ccfg->preload_sz;
	cntp->cache->threshold = ccfg->threshold;
	cntp->cache->q_num = ccfg->q_num;
	cnt_num = pcfg->request_num * (100 + pcfg->alloc_factor) / 100;
	if (cnt_num > UINT32_MAX) {
		DRV_LOG(ERR, "counter number %"PRIu64" is out of 32bit range",
			cnt_num);
		goto error;
	}
	cntp->pool = mlx5_malloc(MLX5_MEM_ANY | MLX5_MEM_ZERO,
			sizeof(struct mlx5_hws_cnt) *
			pcfg->request_num * (100 + pcfg->alloc_factor) / 100,
			0, SOCKET_ID_ANY);
	if (cntp->pool == NULL)
		goto error;
	snprintf(mz_name, sizeof(mz_name), "%s_F_RING", pcfg->name);
	cntp->free_list = rte_ring_create_elem(mz_name, sizeof(cnt_id_t),
			(uint32_t)cnt_num, SOCKET_ID_ANY,
			RING_F_SP_ENQ | RING_F_MC_HTS_DEQ | RING_F_EXACT_SZ);
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
			RING_F_SP_ENQ | RING_F_MC_HTS_DEQ | RING_F_EXACT_SZ);
	if (cntp->reuse_list == NULL) {
		DRV_LOG(ERR, "failed to create reuse list ring");
		goto error;
	}
	for (qidx = 0; qidx < ccfg->q_num; qidx++) {
		snprintf(mz_name, sizeof(mz_name), "%s_cache/%u", pcfg->name,
				qidx);
		cntp->cache->qcache[qidx] = rte_ring_create(mz_name, ccfg->size,
				SOCKET_ID_ANY,
				RING_F_SP_ENQ | RING_F_SC_DEQ |
				RING_F_EXACT_SZ);
		if (cntp->cache->qcache[qidx] == NULL)
			goto error;
	}
	return cntp;
error:
	mlx5_hws_cnt_pool_deinit(cntp);
	return NULL;
}

void
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

int
mlx5_hws_cnt_service_thread_create(struct mlx5_dev_ctx_shared *sh)
{
#define CNT_THREAD_NAME_MAX 256
	char name[CNT_THREAD_NAME_MAX];
	rte_cpuset_t cpuset;
	int ret;
	uint32_t service_core = sh->cnt_svc->service_core;

	CPU_ZERO(&cpuset);
	sh->cnt_svc->svc_running = 1;
	ret = pthread_create(&sh->cnt_svc->service_thread, NULL,
			mlx5_hws_cnt_svc, sh);
	if (ret != 0) {
		DRV_LOG(ERR, "Failed to create HW steering's counter service thread.");
		return -ENOSYS;
	}
	snprintf(name, CNT_THREAD_NAME_MAX - 1, "%s/svc@%d",
		 sh->ibdev_name, service_core);
	rte_thread_setname(sh->cnt_svc->service_thread, name);
	CPU_SET(service_core, &cpuset);
	pthread_setaffinity_np(sh->cnt_svc->service_thread, sizeof(cpuset),
				&cpuset);
	return 0;
}

void
mlx5_hws_cnt_service_thread_destroy(struct mlx5_dev_ctx_shared *sh)
{
	if (sh->cnt_svc->service_thread == 0)
		return;
	sh->cnt_svc->svc_running = 0;
	pthread_join(sh->cnt_svc->service_thread, NULL);
	sh->cnt_svc->service_thread = 0;
}

int
mlx5_hws_cnt_pool_dcs_alloc(struct mlx5_dev_ctx_shared *sh,
			    struct mlx5_hws_cnt_pool *cpool)
{
	struct mlx5_hca_attr *hca_attr = &sh->cdev->config.hca_attr;
	uint32_t max_log_bulk_sz = 0;
	uint32_t log_bulk_sz;
	uint32_t idx, alloced = 0;
	unsigned int cnt_num = mlx5_hws_cnt_pool_get_size(cpool);
	struct mlx5_devx_counter_attr attr = {0};
	struct mlx5_devx_obj *dcs;

	if (hca_attr->flow_counter_bulk_log_max_alloc == 0) {
		DRV_LOG(ERR,
			"Fw doesn't support bulk log max alloc");
		return -1;
	}
	max_log_bulk_sz = 23; /* hard code to 8M (1 << 23). */
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
		for (; idx < MLX5_HWS_CNT_DCS_NUM; idx++) {
			attr.flow_counter_bulk_log_size = --max_log_bulk_sz;
			dcs = mlx5_devx_cmd_flow_counter_alloc_general
				(sh->cdev->ctx, &attr);
			if (dcs == NULL)
				goto error;
			cpool->dcs_mng.dcs[idx].obj = dcs;
			cpool->dcs_mng.dcs[idx].batch_sz =
				(1 << max_log_bulk_sz);
			cpool->dcs_mng.dcs[idx].iidx = alloced;
			alloced += cpool->dcs_mng.dcs[idx].batch_sz;
			cpool->dcs_mng.batch_total++;
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

void
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

int
mlx5_hws_cnt_pool_action_create(struct mlx5_priv *priv,
		struct mlx5_hws_cnt_pool *cpool)
{
	uint32_t idx;
	int ret = 0;
	struct mlx5_hws_cnt_dcs *dcs;
	uint32_t flags;

	flags = MLX5DR_ACTION_FLAG_HWS_RX | MLX5DR_ACTION_FLAG_HWS_TX;
	if (priv->sh->config.dv_esw_en && priv->master)
		flags |= MLX5DR_ACTION_FLAG_HWS_FDB;
	for (idx = 0; idx < cpool->dcs_mng.batch_total; idx++) {
		dcs = &cpool->dcs_mng.dcs[idx];
		dcs->dr_action = mlx5dr_action_create_counter(priv->dr_ctx,
					(struct mlx5dr_devx_obj *)dcs->obj,
					flags);
		if (dcs->dr_action == NULL) {
			mlx5_hws_cnt_pool_action_destroy(cpool);
			ret = -ENOSYS;
			break;
		}
	}
	return ret;
}

void
mlx5_hws_cnt_pool_action_destroy(struct mlx5_hws_cnt_pool *cpool)
{
	uint32_t idx;
	struct mlx5_hws_cnt_dcs *dcs;

	for (idx = 0; idx < cpool->dcs_mng.batch_total; idx++) {
		dcs = &cpool->dcs_mng.dcs[idx];
		if (dcs->dr_action != NULL) {
			mlx5dr_action_destroy(dcs->dr_action);
			dcs->dr_action = NULL;
		}
	}
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
	pcfg.alloc_factor = HWS_CNT_ALLOC_FACTOR_DEFAULT;
	mp_name = mlx5_malloc(MLX5_MEM_ZERO, RTE_MEMZONE_NAMESIZE, 0,
			SOCKET_ID_ANY);
	if (mp_name == NULL)
		goto error;
	snprintf(mp_name, RTE_MEMZONE_NAMESIZE, "MLX5_HWS_CNT_POOL_%u",
			dev->data->port_id);
	pcfg.name = mp_name;
	pcfg.request_num = pattr->nb_counters;
	cpool = mlx5_hws_cnt_pool_init(&pcfg, &cparam);
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
	if (--sh->cnt_svc->refcnt == 0)
		mlx5_hws_cnt_svc_deinit(sh);
	mlx5_hws_cnt_pool_action_destroy(cpool);
	mlx5_hws_cnt_pool_dcs_free(sh, cpool);
	mlx5_hws_cnt_raw_data_free(sh, cpool->raw_mng);
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

#endif
