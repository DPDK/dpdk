/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2015 Mellanox Technologies, Ltd
 */

#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <net/if.h>
#include <sys/mman.h>
#include <linux/rtnetlink.h>

/* Verbs header. */
/* ISO C doesn't support unnamed structs/unions, disabling -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_malloc.h>
#include <rte_ethdev_driver.h>
#include <rte_ethdev_pci.h>
#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <rte_common.h>
#include <rte_kvargs.h>
#include <rte_rwlock.h>
#include <rte_spinlock.h>
#include <rte_string_fns.h>
#include <rte_alarm.h>

#include <mlx5_glue.h>
#include <mlx5_devx_cmds.h>
#include <mlx5_common.h>

#include "mlx5_defs.h"
#include "mlx5.h"
#include "mlx5_utils.h"
#include "mlx5_rxtx.h"
#include "mlx5_autoconf.h"
#include "mlx5_mr.h"
#include "mlx5_flow.h"
#include "rte_pmd_mlx5.h"

/* Device parameter to enable RX completion queue compression. */
#define MLX5_RXQ_CQE_COMP_EN "rxq_cqe_comp_en"

/* Device parameter to enable RX completion entry padding to 128B. */
#define MLX5_RXQ_CQE_PAD_EN "rxq_cqe_pad_en"

/* Device parameter to enable padding Rx packet to cacheline size. */
#define MLX5_RXQ_PKT_PAD_EN "rxq_pkt_pad_en"

/* Device parameter to enable Multi-Packet Rx queue. */
#define MLX5_RX_MPRQ_EN "mprq_en"

/* Device parameter to configure log 2 of the number of strides for MPRQ. */
#define MLX5_RX_MPRQ_LOG_STRIDE_NUM "mprq_log_stride_num"

/* Device parameter to limit the size of memcpy'd packet for MPRQ. */
#define MLX5_RX_MPRQ_MAX_MEMCPY_LEN "mprq_max_memcpy_len"

/* Device parameter to set the minimum number of Rx queues to enable MPRQ. */
#define MLX5_RXQS_MIN_MPRQ "rxqs_min_mprq"

/* Device parameter to configure inline send. Deprecated, ignored.*/
#define MLX5_TXQ_INLINE "txq_inline"

/* Device parameter to limit packet size to inline with ordinary SEND. */
#define MLX5_TXQ_INLINE_MAX "txq_inline_max"

/* Device parameter to configure minimal data size to inline. */
#define MLX5_TXQ_INLINE_MIN "txq_inline_min"

/* Device parameter to limit packet size to inline with Enhanced MPW. */
#define MLX5_TXQ_INLINE_MPW "txq_inline_mpw"

/*
 * Device parameter to configure the number of TX queues threshold for
 * enabling inline send.
 */
#define MLX5_TXQS_MIN_INLINE "txqs_min_inline"

/*
 * Device parameter to configure the number of TX queues threshold for
 * enabling vectorized Tx, deprecated, ignored (no vectorized Tx routines).
 */
#define MLX5_TXQS_MAX_VEC "txqs_max_vec"

/* Device parameter to enable multi-packet send WQEs. */
#define MLX5_TXQ_MPW_EN "txq_mpw_en"

/*
 * Device parameter to force doorbell register mapping
 * to non-cahed region eliminating the extra write memory barrier.
 */
#define MLX5_TX_DB_NC "tx_db_nc"

/*
 * Device parameter to include 2 dsegs in the title WQEBB.
 * Deprecated, ignored.
 */
#define MLX5_TXQ_MPW_HDR_DSEG_EN "txq_mpw_hdr_dseg_en"

/*
 * Device parameter to limit the size of inlining packet.
 * Deprecated, ignored.
 */
#define MLX5_TXQ_MAX_INLINE_LEN "txq_max_inline_len"

/*
 * Device parameter to enable hardware Tx vector.
 * Deprecated, ignored (no vectorized Tx routines anymore).
 */
#define MLX5_TX_VEC_EN "tx_vec_en"

/* Device parameter to enable hardware Rx vector. */
#define MLX5_RX_VEC_EN "rx_vec_en"

/* Allow L3 VXLAN flow creation. */
#define MLX5_L3_VXLAN_EN "l3_vxlan_en"

/* Activate DV E-Switch flow steering. */
#define MLX5_DV_ESW_EN "dv_esw_en"

/* Activate DV flow steering. */
#define MLX5_DV_FLOW_EN "dv_flow_en"

/* Enable extensive flow metadata support. */
#define MLX5_DV_XMETA_EN "dv_xmeta_en"

/* Activate Netlink support in VF mode. */
#define MLX5_VF_NL_EN "vf_nl_en"

/* Enable extending memsegs when creating a MR. */
#define MLX5_MR_EXT_MEMSEG_EN "mr_ext_memseg_en"

/* Select port representors to instantiate. */
#define MLX5_REPRESENTOR "representor"

/* Device parameter to configure the maximum number of dump files per queue. */
#define MLX5_MAX_DUMP_FILES_NUM "max_dump_files_num"

/* Configure timeout of LRO session (in microseconds). */
#define MLX5_LRO_TIMEOUT_USEC "lro_timeout_usec"

#ifndef HAVE_IBV_MLX5_MOD_MPW
#define MLX5DV_CONTEXT_FLAGS_MPW_ALLOWED (1 << 2)
#define MLX5DV_CONTEXT_FLAGS_ENHANCED_MPW (1 << 3)
#endif

#ifndef HAVE_IBV_MLX5_MOD_CQE_128B_COMP
#define MLX5DV_CONTEXT_FLAGS_CQE_128B_COMP (1 << 4)
#endif

static const char *MZ_MLX5_PMD_SHARED_DATA = "mlx5_pmd_shared_data";

/* Shared memory between primary and secondary processes. */
struct mlx5_shared_data *mlx5_shared_data;

/* Spinlock for mlx5_shared_data allocation. */
static rte_spinlock_t mlx5_shared_data_lock = RTE_SPINLOCK_INITIALIZER;

/* Process local data for secondary processes. */
static struct mlx5_local_data mlx5_local_data;

/** Driver-specific log messages type. */
int mlx5_logtype;

/** Data associated with devices to spawn. */
struct mlx5_dev_spawn_data {
	uint32_t ifindex; /**< Network interface index. */
	uint32_t max_port; /**< IB device maximal port index. */
	uint32_t ibv_port; /**< IB device physical port index. */
	int pf_bond; /**< bonding device PF index. < 0 - no bonding */
	struct mlx5_switch_info info; /**< Switch information. */
	struct ibv_device *ibv_dev; /**< Associated IB device. */
	struct rte_eth_dev *eth_dev; /**< Associated Ethernet device. */
	struct rte_pci_device *pci_dev; /**< Backend PCI device. */
};

static LIST_HEAD(, mlx5_ibv_shared) mlx5_ibv_list = LIST_HEAD_INITIALIZER();
static pthread_mutex_t mlx5_ibv_list_mutex = PTHREAD_MUTEX_INITIALIZER;

#define MLX5_FLOW_MIN_ID_POOL_SIZE 512
#define MLX5_ID_GENERATION_ARRAY_FACTOR 16

#define MLX5_FLOW_TABLE_HLIST_ARRAY_SIZE 4096
#define MLX5_TAGS_HLIST_ARRAY_SIZE 8192

/**
 * Allocate ID pool structure.
 *
 * @param[in] max_id
 *   The maximum id can be allocated from the pool.
 *
 * @return
 *   Pointer to pool object, NULL value otherwise.
 */
struct mlx5_flow_id_pool *
mlx5_flow_id_pool_alloc(uint32_t max_id)
{
	struct mlx5_flow_id_pool *pool;
	void *mem;

	pool = rte_zmalloc("id pool allocation", sizeof(*pool),
			   RTE_CACHE_LINE_SIZE);
	if (!pool) {
		DRV_LOG(ERR, "can't allocate id pool");
		rte_errno  = ENOMEM;
		return NULL;
	}
	mem = rte_zmalloc("", MLX5_FLOW_MIN_ID_POOL_SIZE * sizeof(uint32_t),
			  RTE_CACHE_LINE_SIZE);
	if (!mem) {
		DRV_LOG(ERR, "can't allocate mem for id pool");
		rte_errno  = ENOMEM;
		goto error;
	}
	pool->free_arr = mem;
	pool->curr = pool->free_arr;
	pool->last = pool->free_arr + MLX5_FLOW_MIN_ID_POOL_SIZE;
	pool->base_index = 0;
	pool->max_id = max_id;
	return pool;
error:
	rte_free(pool);
	return NULL;
}

/**
 * Release ID pool structure.
 *
 * @param[in] pool
 *   Pointer to flow id pool object to free.
 */
void
mlx5_flow_id_pool_release(struct mlx5_flow_id_pool *pool)
{
	rte_free(pool->free_arr);
	rte_free(pool);
}

/**
 * Generate ID.
 *
 * @param[in] pool
 *   Pointer to flow id pool.
 * @param[out] id
 *   The generated ID.
 *
 * @return
 *   0 on success, error value otherwise.
 */
uint32_t
mlx5_flow_id_get(struct mlx5_flow_id_pool *pool, uint32_t *id)
{
	if (pool->curr == pool->free_arr) {
		if (pool->base_index == pool->max_id) {
			rte_errno  = ENOMEM;
			DRV_LOG(ERR, "no free id");
			return -rte_errno;
		}
		*id = ++pool->base_index;
		return 0;
	}
	*id = *(--pool->curr);
	return 0;
}

/**
 * Release ID.
 *
 * @param[in] pool
 *   Pointer to flow id pool.
 * @param[out] id
 *   The generated ID.
 *
 * @return
 *   0 on success, error value otherwise.
 */
uint32_t
mlx5_flow_id_release(struct mlx5_flow_id_pool *pool, uint32_t id)
{
	uint32_t size;
	uint32_t size2;
	void *mem;

	if (pool->curr == pool->last) {
		size = pool->curr - pool->free_arr;
		size2 = size * MLX5_ID_GENERATION_ARRAY_FACTOR;
		MLX5_ASSERT(size2 > size);
		mem = rte_malloc("", size2 * sizeof(uint32_t), 0);
		if (!mem) {
			DRV_LOG(ERR, "can't allocate mem for id pool");
			rte_errno  = ENOMEM;
			return -rte_errno;
		}
		memcpy(mem, pool->free_arr, size * sizeof(uint32_t));
		rte_free(pool->free_arr);
		pool->free_arr = mem;
		pool->curr = pool->free_arr + size;
		pool->last = pool->free_arr + size2;
	}
	*pool->curr = id;
	pool->curr++;
	return 0;
}

/**
 * Initialize the counters management structure.
 *
 * @param[in] sh
 *   Pointer to mlx5_ibv_shared object to free
 */
static void
mlx5_flow_counters_mng_init(struct mlx5_ibv_shared *sh)
{
	uint8_t i;

	TAILQ_INIT(&sh->cmng.flow_counters);
	for (i = 0; i < RTE_DIM(sh->cmng.ccont); ++i)
		TAILQ_INIT(&sh->cmng.ccont[i].pool_list);
}

/**
 * Destroy all the resources allocated for a counter memory management.
 *
 * @param[in] mng
 *   Pointer to the memory management structure.
 */
static void
mlx5_flow_destroy_counter_stat_mem_mng(struct mlx5_counter_stats_mem_mng *mng)
{
	uint8_t *mem = (uint8_t *)(uintptr_t)mng->raws[0].data;

	LIST_REMOVE(mng, next);
	claim_zero(mlx5_devx_cmd_destroy(mng->dm));
	claim_zero(mlx5_glue->devx_umem_dereg(mng->umem));
	rte_free(mem);
}

/**
 * Close and release all the resources of the counters management.
 *
 * @param[in] sh
 *   Pointer to mlx5_ibv_shared object to free.
 */
static void
mlx5_flow_counters_mng_close(struct mlx5_ibv_shared *sh)
{
	struct mlx5_counter_stats_mem_mng *mng;
	uint8_t i;
	int j;
	int retries = 1024;

	rte_errno = 0;
	while (--retries) {
		rte_eal_alarm_cancel(mlx5_flow_query_alarm, sh);
		if (rte_errno != EINPROGRESS)
			break;
		rte_pause();
	}
	for (i = 0; i < RTE_DIM(sh->cmng.ccont); ++i) {
		struct mlx5_flow_counter_pool *pool;
		uint32_t batch = !!(i % 2);

		if (!sh->cmng.ccont[i].pools)
			continue;
		pool = TAILQ_FIRST(&sh->cmng.ccont[i].pool_list);
		while (pool) {
			if (batch) {
				if (pool->min_dcs)
					claim_zero
					(mlx5_devx_cmd_destroy(pool->min_dcs));
			}
			for (j = 0; j < MLX5_COUNTERS_PER_POOL; ++j) {
				if (pool->counters_raw[j].action)
					claim_zero
					(mlx5_glue->destroy_flow_action
					       (pool->counters_raw[j].action));
				if (!batch && pool->counters_raw[j].dcs)
					claim_zero(mlx5_devx_cmd_destroy
						  (pool->counters_raw[j].dcs));
			}
			TAILQ_REMOVE(&sh->cmng.ccont[i].pool_list, pool,
				     next);
			rte_free(pool);
			pool = TAILQ_FIRST(&sh->cmng.ccont[i].pool_list);
		}
		rte_free(sh->cmng.ccont[i].pools);
	}
	mng = LIST_FIRST(&sh->cmng.mem_mngs);
	while (mng) {
		mlx5_flow_destroy_counter_stat_mem_mng(mng);
		mng = LIST_FIRST(&sh->cmng.mem_mngs);
	}
	memset(&sh->cmng, 0, sizeof(sh->cmng));
}

/**
 * Extract pdn of PD object using DV API.
 *
 * @param[in] pd
 *   Pointer to the verbs PD object.
 * @param[out] pdn
 *   Pointer to the PD object number variable.
 *
 * @return
 *   0 on success, error value otherwise.
 */
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
static int
mlx5_get_pdn(struct ibv_pd *pd __rte_unused, uint32_t *pdn __rte_unused)
{
	struct mlx5dv_obj obj;
	struct mlx5dv_pd pd_info;
	int ret = 0;

	obj.pd.in = pd;
	obj.pd.out = &pd_info;
	ret = mlx5_glue->dv_init_obj(&obj, MLX5DV_OBJ_PD);
	if (ret) {
		DRV_LOG(DEBUG, "Fail to get PD object info");
		return ret;
	}
	*pdn = pd_info.pdn;
	return 0;
}
#endif /* HAVE_IBV_FLOW_DV_SUPPORT */

static int
mlx5_config_doorbell_mapping_env(const struct mlx5_dev_config *config)
{
	char *env;
	int value;

	MLX5_ASSERT(rte_eal_process_type() == RTE_PROC_PRIMARY);
	/* Get environment variable to store. */
	env = getenv(MLX5_SHUT_UP_BF);
	value = env ? !!strcmp(env, "0") : MLX5_ARG_UNSET;
	if (config->dbnc == MLX5_ARG_UNSET)
		setenv(MLX5_SHUT_UP_BF, MLX5_SHUT_UP_BF_DEFAULT, 1);
	else
		setenv(MLX5_SHUT_UP_BF,
		       config->dbnc == MLX5_TXDB_NCACHED ? "1" : "0", 1);
	return value;
}

static void
mlx5_restore_doorbell_mapping_env(int value)
{
	MLX5_ASSERT(rte_eal_process_type() == RTE_PROC_PRIMARY);
	/* Restore the original environment variable state. */
	if (value == MLX5_ARG_UNSET)
		unsetenv(MLX5_SHUT_UP_BF);
	else
		setenv(MLX5_SHUT_UP_BF, value ? "1" : "0", 1);
}

/**
 * Allocate shared IB device context. If there is multiport device the
 * master and representors will share this context, if there is single
 * port dedicated IB device, the context will be used by only given
 * port due to unification.
 *
 * Routine first searches the context for the specified IB device name,
 * if found the shared context assumed and reference counter is incremented.
 * If no context found the new one is created and initialized with specified
 * IB device context and parameters.
 *
 * @param[in] spawn
 *   Pointer to the IB device attributes (name, port, etc).
 * @param[in] config
 *   Pointer to device configuration structure.
 *
 * @return
 *   Pointer to mlx5_ibv_shared object on success,
 *   otherwise NULL and rte_errno is set.
 */
static struct mlx5_ibv_shared *
mlx5_alloc_shared_ibctx(const struct mlx5_dev_spawn_data *spawn,
			const struct mlx5_dev_config *config)
{
	struct mlx5_ibv_shared *sh;
	int dbmap_env;
	int err = 0;
	uint32_t i;
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	struct mlx5_devx_tis_attr tis_attr = { 0 };
#endif

	MLX5_ASSERT(spawn);
	/* Secondary process should not create the shared context. */
	MLX5_ASSERT(rte_eal_process_type() == RTE_PROC_PRIMARY);
	pthread_mutex_lock(&mlx5_ibv_list_mutex);
	/* Search for IB context by device name. */
	LIST_FOREACH(sh, &mlx5_ibv_list, next) {
		if (!strcmp(sh->ibdev_name, spawn->ibv_dev->name)) {
			sh->refcnt++;
			goto exit;
		}
	}
	/* No device found, we have to create new shared context. */
	MLX5_ASSERT(spawn->max_port);
	sh = rte_zmalloc("ethdev shared ib context",
			 sizeof(struct mlx5_ibv_shared) +
			 spawn->max_port *
			 sizeof(struct mlx5_ibv_shared_port),
			 RTE_CACHE_LINE_SIZE);
	if (!sh) {
		DRV_LOG(ERR, "shared context allocation failure");
		rte_errno  = ENOMEM;
		goto exit;
	}
	/*
	 * Configure environment variable "MLX5_BF_SHUT_UP"
	 * before the device creation. The rdma_core library
	 * checks the variable at device creation and
	 * stores the result internally.
	 */
	dbmap_env = mlx5_config_doorbell_mapping_env(config);
	/* Try to open IB device with DV first, then usual Verbs. */
	errno = 0;
	sh->ctx = mlx5_glue->dv_open_device(spawn->ibv_dev);
	if (sh->ctx) {
		sh->devx = 1;
		DRV_LOG(DEBUG, "DevX is supported");
		/* The device is created, no need for environment. */
		mlx5_restore_doorbell_mapping_env(dbmap_env);
	} else {
		/* The environment variable is still configured. */
		sh->ctx = mlx5_glue->open_device(spawn->ibv_dev);
		err = errno ? errno : ENODEV;
		/*
		 * The environment variable is not needed anymore,
		 * all device creation attempts are completed.
		 */
		mlx5_restore_doorbell_mapping_env(dbmap_env);
		if (!sh->ctx)
			goto error;
		DRV_LOG(DEBUG, "DevX is NOT supported");
	}
	err = mlx5_glue->query_device_ex(sh->ctx, NULL, &sh->device_attr);
	if (err) {
		DRV_LOG(DEBUG, "ibv_query_device_ex() failed");
		goto error;
	}
	sh->refcnt = 1;
	sh->max_port = spawn->max_port;
	strncpy(sh->ibdev_name, sh->ctx->device->name,
		sizeof(sh->ibdev_name));
	strncpy(sh->ibdev_path, sh->ctx->device->ibdev_path,
		sizeof(sh->ibdev_path));
	pthread_mutex_init(&sh->intr_mutex, NULL);
	/*
	 * Setting port_id to max unallowed value means
	 * there is no interrupt subhandler installed for
	 * the given port index i.
	 */
	for (i = 0; i < sh->max_port; i++) {
		sh->port[i].ih_port_id = RTE_MAX_ETHPORTS;
		sh->port[i].devx_ih_port_id = RTE_MAX_ETHPORTS;
	}
	sh->pd = mlx5_glue->alloc_pd(sh->ctx);
	if (sh->pd == NULL) {
		DRV_LOG(ERR, "PD allocation failure");
		err = ENOMEM;
		goto error;
	}
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	if (sh->devx) {
		err = mlx5_get_pdn(sh->pd, &sh->pdn);
		if (err) {
			DRV_LOG(ERR, "Fail to extract pdn from PD");
			goto error;
		}
		sh->td = mlx5_devx_cmd_create_td(sh->ctx);
		if (!sh->td) {
			DRV_LOG(ERR, "TD allocation failure");
			err = ENOMEM;
			goto error;
		}
		tis_attr.transport_domain = sh->td->id;
		sh->tis = mlx5_devx_cmd_create_tis(sh->ctx, &tis_attr);
		if (!sh->tis) {
			DRV_LOG(ERR, "TIS allocation failure");
			err = ENOMEM;
			goto error;
		}
	}
	sh->flow_id_pool = mlx5_flow_id_pool_alloc(UINT32_MAX);
	if (!sh->flow_id_pool) {
		DRV_LOG(ERR, "can't create flow id pool");
		err = ENOMEM;
		goto error;
	}
#endif /* HAVE_IBV_FLOW_DV_SUPPORT */
	/*
	 * Once the device is added to the list of memory event
	 * callback, its global MR cache table cannot be expanded
	 * on the fly because of deadlock. If it overflows, lookup
	 * should be done by searching MR list linearly, which is slow.
	 *
	 * At this point the device is not added to the memory
	 * event list yet, context is just being created.
	 */
	err = mlx5_mr_btree_init(&sh->mr.cache,
				 MLX5_MR_BTREE_CACHE_N * 2,
				 spawn->pci_dev->device.numa_node);
	if (err) {
		err = rte_errno;
		goto error;
	}
	mlx5_flow_counters_mng_init(sh);
	/* Add device to memory callback list. */
	rte_rwlock_write_lock(&mlx5_shared_data->mem_event_rwlock);
	LIST_INSERT_HEAD(&mlx5_shared_data->mem_event_cb_list,
			 sh, mem_event_cb);
	rte_rwlock_write_unlock(&mlx5_shared_data->mem_event_rwlock);
	/* Add context to the global device list. */
	LIST_INSERT_HEAD(&mlx5_ibv_list, sh, next);
exit:
	pthread_mutex_unlock(&mlx5_ibv_list_mutex);
	return sh;
error:
	pthread_mutex_unlock(&mlx5_ibv_list_mutex);
	MLX5_ASSERT(sh);
	if (sh->tis)
		claim_zero(mlx5_devx_cmd_destroy(sh->tis));
	if (sh->td)
		claim_zero(mlx5_devx_cmd_destroy(sh->td));
	if (sh->pd)
		claim_zero(mlx5_glue->dealloc_pd(sh->pd));
	if (sh->ctx)
		claim_zero(mlx5_glue->close_device(sh->ctx));
	if (sh->flow_id_pool)
		mlx5_flow_id_pool_release(sh->flow_id_pool);
	rte_free(sh);
	MLX5_ASSERT(err > 0);
	rte_errno = err;
	return NULL;
}

/**
 * Free shared IB device context. Decrement counter and if zero free
 * all allocated resources and close handles.
 *
 * @param[in] sh
 *   Pointer to mlx5_ibv_shared object to free
 */
static void
mlx5_free_shared_ibctx(struct mlx5_ibv_shared *sh)
{
	pthread_mutex_lock(&mlx5_ibv_list_mutex);
#ifdef RTE_LIBRTE_MLX5_DEBUG
	/* Check the object presence in the list. */
	struct mlx5_ibv_shared *lctx;

	LIST_FOREACH(lctx, &mlx5_ibv_list, next)
		if (lctx == sh)
			break;
	MLX5_ASSERT(lctx);
	if (lctx != sh) {
		DRV_LOG(ERR, "Freeing non-existing shared IB context");
		goto exit;
	}
#endif
	MLX5_ASSERT(sh);
	MLX5_ASSERT(sh->refcnt);
	/* Secondary process should not free the shared context. */
	MLX5_ASSERT(rte_eal_process_type() == RTE_PROC_PRIMARY);
	if (--sh->refcnt)
		goto exit;
	/* Remove from memory callback device list. */
	rte_rwlock_write_lock(&mlx5_shared_data->mem_event_rwlock);
	LIST_REMOVE(sh, mem_event_cb);
	rte_rwlock_write_unlock(&mlx5_shared_data->mem_event_rwlock);
	/* Release created Memory Regions. */
	mlx5_mr_release(sh);
	/* Remove context from the global device list. */
	LIST_REMOVE(sh, next);
	/*
	 *  Ensure there is no async event handler installed.
	 *  Only primary process handles async device events.
	 **/
	mlx5_flow_counters_mng_close(sh);
	MLX5_ASSERT(!sh->intr_cnt);
	if (sh->intr_cnt)
		mlx5_intr_callback_unregister
			(&sh->intr_handle, mlx5_dev_interrupt_handler, sh);
#ifdef HAVE_MLX5_DEVX_ASYNC_SUPPORT
	if (sh->devx_intr_cnt) {
		if (sh->intr_handle_devx.fd)
			rte_intr_callback_unregister(&sh->intr_handle_devx,
					  mlx5_dev_interrupt_handler_devx, sh);
		if (sh->devx_comp)
			mlx5dv_devx_destroy_cmd_comp(sh->devx_comp);
	}
#endif
	pthread_mutex_destroy(&sh->intr_mutex);
	if (sh->pd)
		claim_zero(mlx5_glue->dealloc_pd(sh->pd));
	if (sh->tis)
		claim_zero(mlx5_devx_cmd_destroy(sh->tis));
	if (sh->td)
		claim_zero(mlx5_devx_cmd_destroy(sh->td));
	if (sh->ctx)
		claim_zero(mlx5_glue->close_device(sh->ctx));
	if (sh->flow_id_pool)
		mlx5_flow_id_pool_release(sh->flow_id_pool);
	rte_free(sh);
exit:
	pthread_mutex_unlock(&mlx5_ibv_list_mutex);
}

/**
 * Destroy table hash list and all the root entries per domain.
 *
 * @param[in] priv
 *   Pointer to the private device data structure.
 */
static void
mlx5_free_table_hash_list(struct mlx5_priv *priv)
{
	struct mlx5_ibv_shared *sh = priv->sh;
	struct mlx5_flow_tbl_data_entry *tbl_data;
	union mlx5_flow_tbl_key table_key = {
		{
			.table_id = 0,
			.reserved = 0,
			.domain = 0,
			.direction = 0,
		}
	};
	struct mlx5_hlist_entry *pos;

	if (!sh->flow_tbls)
		return;
	pos = mlx5_hlist_lookup(sh->flow_tbls, table_key.v64);
	if (pos) {
		tbl_data = container_of(pos, struct mlx5_flow_tbl_data_entry,
					entry);
		MLX5_ASSERT(tbl_data);
		mlx5_hlist_remove(sh->flow_tbls, pos);
		rte_free(tbl_data);
	}
	table_key.direction = 1;
	pos = mlx5_hlist_lookup(sh->flow_tbls, table_key.v64);
	if (pos) {
		tbl_data = container_of(pos, struct mlx5_flow_tbl_data_entry,
					entry);
		MLX5_ASSERT(tbl_data);
		mlx5_hlist_remove(sh->flow_tbls, pos);
		rte_free(tbl_data);
	}
	table_key.direction = 0;
	table_key.domain = 1;
	pos = mlx5_hlist_lookup(sh->flow_tbls, table_key.v64);
	if (pos) {
		tbl_data = container_of(pos, struct mlx5_flow_tbl_data_entry,
					entry);
		MLX5_ASSERT(tbl_data);
		mlx5_hlist_remove(sh->flow_tbls, pos);
		rte_free(tbl_data);
	}
	mlx5_hlist_destroy(sh->flow_tbls, NULL, NULL);
}

/**
 * Initialize flow table hash list and create the root tables entry
 * for each domain.
 *
 * @param[in] priv
 *   Pointer to the private device data structure.
 *
 * @return
 *   Zero on success, positive error code otherwise.
 */
static int
mlx5_alloc_table_hash_list(struct mlx5_priv *priv)
{
	struct mlx5_ibv_shared *sh = priv->sh;
	char s[MLX5_HLIST_NAMESIZE];
	int err = 0;

	MLX5_ASSERT(sh);
	snprintf(s, sizeof(s), "%s_flow_table", priv->sh->ibdev_name);
	sh->flow_tbls = mlx5_hlist_create(s, MLX5_FLOW_TABLE_HLIST_ARRAY_SIZE);
	if (!sh->flow_tbls) {
		DRV_LOG(ERR, "flow tables with hash creation failed.\n");
		err = ENOMEM;
		return err;
	}
#ifndef HAVE_MLX5DV_DR
	/*
	 * In case we have not DR support, the zero tables should be created
	 * because DV expect to see them even if they cannot be created by
	 * RDMA-CORE.
	 */
	union mlx5_flow_tbl_key table_key = {
		{
			.table_id = 0,
			.reserved = 0,
			.domain = 0,
			.direction = 0,
		}
	};
	struct mlx5_flow_tbl_data_entry *tbl_data = rte_zmalloc(NULL,
							  sizeof(*tbl_data), 0);

	if (!tbl_data) {
		err = ENOMEM;
		goto error;
	}
	tbl_data->entry.key = table_key.v64;
	err = mlx5_hlist_insert(sh->flow_tbls, &tbl_data->entry);
	if (err)
		goto error;
	rte_atomic32_init(&tbl_data->tbl.refcnt);
	rte_atomic32_inc(&tbl_data->tbl.refcnt);
	table_key.direction = 1;
	tbl_data = rte_zmalloc(NULL, sizeof(*tbl_data), 0);
	if (!tbl_data) {
		err = ENOMEM;
		goto error;
	}
	tbl_data->entry.key = table_key.v64;
	err = mlx5_hlist_insert(sh->flow_tbls, &tbl_data->entry);
	if (err)
		goto error;
	rte_atomic32_init(&tbl_data->tbl.refcnt);
	rte_atomic32_inc(&tbl_data->tbl.refcnt);
	table_key.direction = 0;
	table_key.domain = 1;
	tbl_data = rte_zmalloc(NULL, sizeof(*tbl_data), 0);
	if (!tbl_data) {
		err = ENOMEM;
		goto error;
	}
	tbl_data->entry.key = table_key.v64;
	err = mlx5_hlist_insert(sh->flow_tbls, &tbl_data->entry);
	if (err)
		goto error;
	rte_atomic32_init(&tbl_data->tbl.refcnt);
	rte_atomic32_inc(&tbl_data->tbl.refcnt);
	return err;
error:
	mlx5_free_table_hash_list(priv);
#endif /* HAVE_MLX5DV_DR */
	return err;
}

/**
 * Initialize DR related data within private structure.
 * Routine checks the reference counter and does actual
 * resources creation/initialization only if counter is zero.
 *
 * @param[in] priv
 *   Pointer to the private device data structure.
 *
 * @return
 *   Zero on success, positive error code otherwise.
 */
static int
mlx5_alloc_shared_dr(struct mlx5_priv *priv)
{
	struct mlx5_ibv_shared *sh = priv->sh;
	char s[MLX5_HLIST_NAMESIZE];
	int err = 0;

	if (!sh->flow_tbls)
		err = mlx5_alloc_table_hash_list(priv);
	else
		DRV_LOG(DEBUG, "sh->flow_tbls[%p] already created, reuse\n",
			(void *)sh->flow_tbls);
	if (err)
		return err;
	/* Create tags hash list table. */
	snprintf(s, sizeof(s), "%s_tags", sh->ibdev_name);
	sh->tag_table = mlx5_hlist_create(s, MLX5_TAGS_HLIST_ARRAY_SIZE);
	if (!sh->tag_table) {
		DRV_LOG(ERR, "tags with hash creation failed.\n");
		err = ENOMEM;
		goto error;
	}
#ifdef HAVE_MLX5DV_DR
	void *domain;

	if (sh->dv_refcnt) {
		/* Shared DV/DR structures is already initialized. */
		sh->dv_refcnt++;
		priv->dr_shared = 1;
		return 0;
	}
	/* Reference counter is zero, we should initialize structures. */
	domain = mlx5_glue->dr_create_domain(sh->ctx,
					     MLX5DV_DR_DOMAIN_TYPE_NIC_RX);
	if (!domain) {
		DRV_LOG(ERR, "ingress mlx5dv_dr_create_domain failed");
		err = errno;
		goto error;
	}
	sh->rx_domain = domain;
	domain = mlx5_glue->dr_create_domain(sh->ctx,
					     MLX5DV_DR_DOMAIN_TYPE_NIC_TX);
	if (!domain) {
		DRV_LOG(ERR, "egress mlx5dv_dr_create_domain failed");
		err = errno;
		goto error;
	}
	pthread_mutex_init(&sh->dv_mutex, NULL);
	sh->tx_domain = domain;
#ifdef HAVE_MLX5DV_DR_ESWITCH
	if (priv->config.dv_esw_en) {
		domain  = mlx5_glue->dr_create_domain
			(sh->ctx, MLX5DV_DR_DOMAIN_TYPE_FDB);
		if (!domain) {
			DRV_LOG(ERR, "FDB mlx5dv_dr_create_domain failed");
			err = errno;
			goto error;
		}
		sh->fdb_domain = domain;
		sh->esw_drop_action = mlx5_glue->dr_create_flow_action_drop();
	}
#endif
	sh->pop_vlan_action = mlx5_glue->dr_create_flow_action_pop_vlan();
#endif /* HAVE_MLX5DV_DR */
	sh->dv_refcnt++;
	priv->dr_shared = 1;
	return 0;
error:
	/* Rollback the created objects. */
	if (sh->rx_domain) {
		mlx5_glue->dr_destroy_domain(sh->rx_domain);
		sh->rx_domain = NULL;
	}
	if (sh->tx_domain) {
		mlx5_glue->dr_destroy_domain(sh->tx_domain);
		sh->tx_domain = NULL;
	}
	if (sh->fdb_domain) {
		mlx5_glue->dr_destroy_domain(sh->fdb_domain);
		sh->fdb_domain = NULL;
	}
	if (sh->esw_drop_action) {
		mlx5_glue->destroy_flow_action(sh->esw_drop_action);
		sh->esw_drop_action = NULL;
	}
	if (sh->pop_vlan_action) {
		mlx5_glue->destroy_flow_action(sh->pop_vlan_action);
		sh->pop_vlan_action = NULL;
	}
	if (sh->tag_table) {
		/* tags should be destroyed with flow before. */
		mlx5_hlist_destroy(sh->tag_table, NULL, NULL);
		sh->tag_table = NULL;
	}
	mlx5_free_table_hash_list(priv);
	return err;
}

/**
 * Destroy DR related data within private structure.
 *
 * @param[in] priv
 *   Pointer to the private device data structure.
 */
static void
mlx5_free_shared_dr(struct mlx5_priv *priv)
{
	struct mlx5_ibv_shared *sh;

	if (!priv->dr_shared)
		return;
	priv->dr_shared = 0;
	sh = priv->sh;
	MLX5_ASSERT(sh);
#ifdef HAVE_MLX5DV_DR
	MLX5_ASSERT(sh->dv_refcnt);
	if (sh->dv_refcnt && --sh->dv_refcnt)
		return;
	if (sh->rx_domain) {
		mlx5_glue->dr_destroy_domain(sh->rx_domain);
		sh->rx_domain = NULL;
	}
	if (sh->tx_domain) {
		mlx5_glue->dr_destroy_domain(sh->tx_domain);
		sh->tx_domain = NULL;
	}
#ifdef HAVE_MLX5DV_DR_ESWITCH
	if (sh->fdb_domain) {
		mlx5_glue->dr_destroy_domain(sh->fdb_domain);
		sh->fdb_domain = NULL;
	}
	if (sh->esw_drop_action) {
		mlx5_glue->destroy_flow_action(sh->esw_drop_action);
		sh->esw_drop_action = NULL;
	}
#endif
	if (sh->pop_vlan_action) {
		mlx5_glue->destroy_flow_action(sh->pop_vlan_action);
		sh->pop_vlan_action = NULL;
	}
	pthread_mutex_destroy(&sh->dv_mutex);
#endif /* HAVE_MLX5DV_DR */
	if (sh->tag_table) {
		/* tags should be destroyed with flow before. */
		mlx5_hlist_destroy(sh->tag_table, NULL, NULL);
		sh->tag_table = NULL;
	}
	mlx5_free_table_hash_list(priv);
}

/**
 * Initialize shared data between primary and secondary process.
 *
 * A memzone is reserved by primary process and secondary processes attach to
 * the memzone.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_init_shared_data(void)
{
	const struct rte_memzone *mz;
	int ret = 0;

	rte_spinlock_lock(&mlx5_shared_data_lock);
	if (mlx5_shared_data == NULL) {
		if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
			/* Allocate shared memory. */
			mz = rte_memzone_reserve(MZ_MLX5_PMD_SHARED_DATA,
						 sizeof(*mlx5_shared_data),
						 SOCKET_ID_ANY, 0);
			if (mz == NULL) {
				DRV_LOG(ERR,
					"Cannot allocate mlx5 shared data");
				ret = -rte_errno;
				goto error;
			}
			mlx5_shared_data = mz->addr;
			memset(mlx5_shared_data, 0, sizeof(*mlx5_shared_data));
			rte_spinlock_init(&mlx5_shared_data->lock);
		} else {
			/* Lookup allocated shared memory. */
			mz = rte_memzone_lookup(MZ_MLX5_PMD_SHARED_DATA);
			if (mz == NULL) {
				DRV_LOG(ERR,
					"Cannot attach mlx5 shared data");
				ret = -rte_errno;
				goto error;
			}
			mlx5_shared_data = mz->addr;
			memset(&mlx5_local_data, 0, sizeof(mlx5_local_data));
		}
	}
error:
	rte_spinlock_unlock(&mlx5_shared_data_lock);
	return ret;
}

/**
 * Retrieve integer value from environment variable.
 *
 * @param[in] name
 *   Environment variable name.
 *
 * @return
 *   Integer value, 0 if the variable is not set.
 */
int
mlx5_getenv_int(const char *name)
{
	const char *val = getenv(name);

	if (val == NULL)
		return 0;
	return atoi(val);
}

/**
 * Verbs callback to allocate a memory. This function should allocate the space
 * according to the size provided residing inside a huge page.
 * Please note that all allocation must respect the alignment from libmlx5
 * (i.e. currently sysconf(_SC_PAGESIZE)).
 *
 * @param[in] size
 *   The size in bytes of the memory to allocate.
 * @param[in] data
 *   A pointer to the callback data.
 *
 * @return
 *   Allocated buffer, NULL otherwise and rte_errno is set.
 */
static void *
mlx5_alloc_verbs_buf(size_t size, void *data)
{
	struct mlx5_priv *priv = data;
	void *ret;
	size_t alignment = sysconf(_SC_PAGESIZE);
	unsigned int socket = SOCKET_ID_ANY;

	if (priv->verbs_alloc_ctx.type == MLX5_VERBS_ALLOC_TYPE_TX_QUEUE) {
		const struct mlx5_txq_ctrl *ctrl = priv->verbs_alloc_ctx.obj;

		socket = ctrl->socket;
	} else if (priv->verbs_alloc_ctx.type ==
		   MLX5_VERBS_ALLOC_TYPE_RX_QUEUE) {
		const struct mlx5_rxq_ctrl *ctrl = priv->verbs_alloc_ctx.obj;

		socket = ctrl->socket;
	}
	MLX5_ASSERT(data != NULL);
	ret = rte_malloc_socket(__func__, size, alignment, socket);
	if (!ret && size)
		rte_errno = ENOMEM;
	return ret;
}

/**
 * Verbs callback to free a memory.
 *
 * @param[in] ptr
 *   A pointer to the memory to free.
 * @param[in] data
 *   A pointer to the callback data.
 */
static void
mlx5_free_verbs_buf(void *ptr, void *data __rte_unused)
{
	MLX5_ASSERT(data != NULL);
	rte_free(ptr);
}

/**
 * DPDK callback to add udp tunnel port
 *
 * @param[in] dev
 *   A pointer to eth_dev
 * @param[in] udp_tunnel
 *   A pointer to udp tunnel
 *
 * @return
 *   0 on valid udp ports and tunnels, -ENOTSUP otherwise.
 */
int
mlx5_udp_tunnel_port_add(struct rte_eth_dev *dev __rte_unused,
			 struct rte_eth_udp_tunnel *udp_tunnel)
{
	MLX5_ASSERT(udp_tunnel != NULL);
	if (udp_tunnel->prot_type == RTE_TUNNEL_TYPE_VXLAN &&
	    udp_tunnel->udp_port == 4789)
		return 0;
	if (udp_tunnel->prot_type == RTE_TUNNEL_TYPE_VXLAN_GPE &&
	    udp_tunnel->udp_port == 4790)
		return 0;
	return -ENOTSUP;
}

/**
 * Initialize process private data structure.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_proc_priv_init(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_proc_priv *ppriv;
	size_t ppriv_size;

	/*
	 * UAR register table follows the process private structure. BlueFlame
	 * registers for Tx queues are stored in the table.
	 */
	ppriv_size =
		sizeof(struct mlx5_proc_priv) + priv->txqs_n * sizeof(void *);
	ppriv = rte_malloc_socket("mlx5_proc_priv", ppriv_size,
				  RTE_CACHE_LINE_SIZE, dev->device->numa_node);
	if (!ppriv) {
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	ppriv->uar_table_sz = ppriv_size;
	dev->process_private = ppriv;
	return 0;
}

/**
 * Un-initialize process private data structure.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
static void
mlx5_proc_priv_uninit(struct rte_eth_dev *dev)
{
	if (!dev->process_private)
		return;
	rte_free(dev->process_private);
	dev->process_private = NULL;
}

/**
 * DPDK callback to close the device.
 *
 * Destroy all queues and objects, free memory.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
static void
mlx5_dev_close(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	unsigned int i;
	int ret;

	DRV_LOG(DEBUG, "port %u closing device \"%s\"",
		dev->data->port_id,
		((priv->sh->ctx != NULL) ? priv->sh->ctx->device->name : ""));
	/* In case mlx5_dev_stop() has not been called. */
	mlx5_dev_interrupt_handler_uninstall(dev);
	mlx5_dev_interrupt_handler_devx_uninstall(dev);
	/*
	 * If default mreg copy action is removed at the stop stage,
	 * the search will return none and nothing will be done anymore.
	 */
	mlx5_flow_stop_default(dev);
	mlx5_traffic_disable(dev);
	/*
	 * If all the flows are already flushed in the device stop stage,
	 * then this will return directly without any action.
	 */
	mlx5_flow_list_flush(dev, &priv->flows, true);
	mlx5_flow_meter_flush(dev, NULL);
	/* Free the intermediate buffers for flow creation. */
	mlx5_flow_free_intermediate(dev);
	/* Prevent crashes when queues are still in use. */
	dev->rx_pkt_burst = removed_rx_burst;
	dev->tx_pkt_burst = removed_tx_burst;
	rte_wmb();
	/* Disable datapath on secondary process. */
	mlx5_mp_req_stop_rxtx(dev);
	if (priv->rxqs != NULL) {
		/* XXX race condition if mlx5_rx_burst() is still running. */
		usleep(1000);
		for (i = 0; (i != priv->rxqs_n); ++i)
			mlx5_rxq_release(dev, i);
		priv->rxqs_n = 0;
		priv->rxqs = NULL;
	}
	if (priv->txqs != NULL) {
		/* XXX race condition if mlx5_tx_burst() is still running. */
		usleep(1000);
		for (i = 0; (i != priv->txqs_n); ++i)
			mlx5_txq_release(dev, i);
		priv->txqs_n = 0;
		priv->txqs = NULL;
	}
	mlx5_proc_priv_uninit(dev);
	if (priv->mreg_cp_tbl)
		mlx5_hlist_destroy(priv->mreg_cp_tbl, NULL, NULL);
	mlx5_mprq_free_mp(dev);
	mlx5_free_shared_dr(priv);
	if (priv->rss_conf.rss_key != NULL)
		rte_free(priv->rss_conf.rss_key);
	if (priv->reta_idx != NULL)
		rte_free(priv->reta_idx);
	if (priv->config.vf)
		mlx5_nl_mac_addr_flush(priv->nl_socket_route, mlx5_ifindex(dev),
				       dev->data->mac_addrs,
				       MLX5_MAX_MAC_ADDRESSES, priv->mac_own);
	if (priv->nl_socket_route >= 0)
		close(priv->nl_socket_route);
	if (priv->nl_socket_rdma >= 0)
		close(priv->nl_socket_rdma);
	if (priv->vmwa_context)
		mlx5_vlan_vmwa_exit(priv->vmwa_context);
	if (priv->sh) {
		/*
		 * Free the shared context in last turn, because the cleanup
		 * routines above may use some shared fields, like
		 * mlx5_nl_mac_addr_flush() uses ibdev_path for retrieveing
		 * ifindex if Netlink fails.
		 */
		mlx5_free_shared_ibctx(priv->sh);
		priv->sh = NULL;
	}
	ret = mlx5_hrxq_verify(dev);
	if (ret)
		DRV_LOG(WARNING, "port %u some hash Rx queue still remain",
			dev->data->port_id);
	ret = mlx5_ind_table_obj_verify(dev);
	if (ret)
		DRV_LOG(WARNING, "port %u some indirection table still remain",
			dev->data->port_id);
	ret = mlx5_rxq_obj_verify(dev);
	if (ret)
		DRV_LOG(WARNING, "port %u some Rx queue objects still remain",
			dev->data->port_id);
	ret = mlx5_rxq_verify(dev);
	if (ret)
		DRV_LOG(WARNING, "port %u some Rx queues still remain",
			dev->data->port_id);
	ret = mlx5_txq_obj_verify(dev);
	if (ret)
		DRV_LOG(WARNING, "port %u some Verbs Tx queue still remain",
			dev->data->port_id);
	ret = mlx5_txq_verify(dev);
	if (ret)
		DRV_LOG(WARNING, "port %u some Tx queues still remain",
			dev->data->port_id);
	ret = mlx5_flow_verify(dev);
	if (ret)
		DRV_LOG(WARNING, "port %u some flows still remain",
			dev->data->port_id);
	if (priv->domain_id != RTE_ETH_DEV_SWITCH_DOMAIN_ID_INVALID) {
		unsigned int c = 0;
		uint16_t port_id;

		MLX5_ETH_FOREACH_DEV(port_id, priv->pci_dev) {
			struct mlx5_priv *opriv =
				rte_eth_devices[port_id].data->dev_private;

			if (!opriv ||
			    opriv->domain_id != priv->domain_id ||
			    &rte_eth_devices[port_id] == dev)
				continue;
			++c;
			break;
		}
		if (!c)
			claim_zero(rte_eth_switch_domain_free(priv->domain_id));
	}
	memset(priv, 0, sizeof(*priv));
	priv->domain_id = RTE_ETH_DEV_SWITCH_DOMAIN_ID_INVALID;
	/*
	 * Reset mac_addrs to NULL such that it is not freed as part of
	 * rte_eth_dev_release_port(). mac_addrs is part of dev_private so
	 * it is freed when dev_private is freed.
	 */
	dev->data->mac_addrs = NULL;
}

const struct eth_dev_ops mlx5_dev_ops = {
	.dev_configure = mlx5_dev_configure,
	.dev_start = mlx5_dev_start,
	.dev_stop = mlx5_dev_stop,
	.dev_set_link_down = mlx5_set_link_down,
	.dev_set_link_up = mlx5_set_link_up,
	.dev_close = mlx5_dev_close,
	.promiscuous_enable = mlx5_promiscuous_enable,
	.promiscuous_disable = mlx5_promiscuous_disable,
	.allmulticast_enable = mlx5_allmulticast_enable,
	.allmulticast_disable = mlx5_allmulticast_disable,
	.link_update = mlx5_link_update,
	.stats_get = mlx5_stats_get,
	.stats_reset = mlx5_stats_reset,
	.xstats_get = mlx5_xstats_get,
	.xstats_reset = mlx5_xstats_reset,
	.xstats_get_names = mlx5_xstats_get_names,
	.fw_version_get = mlx5_fw_version_get,
	.dev_infos_get = mlx5_dev_infos_get,
	.read_clock = mlx5_read_clock,
	.dev_supported_ptypes_get = mlx5_dev_supported_ptypes_get,
	.vlan_filter_set = mlx5_vlan_filter_set,
	.rx_queue_setup = mlx5_rx_queue_setup,
	.rx_hairpin_queue_setup = mlx5_rx_hairpin_queue_setup,
	.tx_queue_setup = mlx5_tx_queue_setup,
	.tx_hairpin_queue_setup = mlx5_tx_hairpin_queue_setup,
	.rx_queue_release = mlx5_rx_queue_release,
	.tx_queue_release = mlx5_tx_queue_release,
	.flow_ctrl_get = mlx5_dev_get_flow_ctrl,
	.flow_ctrl_set = mlx5_dev_set_flow_ctrl,
	.mac_addr_remove = mlx5_mac_addr_remove,
	.mac_addr_add = mlx5_mac_addr_add,
	.mac_addr_set = mlx5_mac_addr_set,
	.set_mc_addr_list = mlx5_set_mc_addr_list,
	.mtu_set = mlx5_dev_set_mtu,
	.vlan_strip_queue_set = mlx5_vlan_strip_queue_set,
	.vlan_offload_set = mlx5_vlan_offload_set,
	.reta_update = mlx5_dev_rss_reta_update,
	.reta_query = mlx5_dev_rss_reta_query,
	.rss_hash_update = mlx5_rss_hash_update,
	.rss_hash_conf_get = mlx5_rss_hash_conf_get,
	.filter_ctrl = mlx5_dev_filter_ctrl,
	.rx_descriptor_status = mlx5_rx_descriptor_status,
	.tx_descriptor_status = mlx5_tx_descriptor_status,
	.rxq_info_get = mlx5_rxq_info_get,
	.txq_info_get = mlx5_txq_info_get,
	.rx_burst_mode_get = mlx5_rx_burst_mode_get,
	.tx_burst_mode_get = mlx5_tx_burst_mode_get,
	.rx_queue_count = mlx5_rx_queue_count,
	.rx_queue_intr_enable = mlx5_rx_intr_enable,
	.rx_queue_intr_disable = mlx5_rx_intr_disable,
	.is_removed = mlx5_is_removed,
	.udp_tunnel_port_add  = mlx5_udp_tunnel_port_add,
	.get_module_info = mlx5_get_module_info,
	.get_module_eeprom = mlx5_get_module_eeprom,
	.hairpin_cap_get = mlx5_hairpin_cap_get,
	.mtr_ops_get = mlx5_flow_meter_ops_get,
};

/* Available operations from secondary process. */
static const struct eth_dev_ops mlx5_dev_sec_ops = {
	.stats_get = mlx5_stats_get,
	.stats_reset = mlx5_stats_reset,
	.xstats_get = mlx5_xstats_get,
	.xstats_reset = mlx5_xstats_reset,
	.xstats_get_names = mlx5_xstats_get_names,
	.fw_version_get = mlx5_fw_version_get,
	.dev_infos_get = mlx5_dev_infos_get,
	.rx_descriptor_status = mlx5_rx_descriptor_status,
	.tx_descriptor_status = mlx5_tx_descriptor_status,
	.rxq_info_get = mlx5_rxq_info_get,
	.txq_info_get = mlx5_txq_info_get,
	.rx_burst_mode_get = mlx5_rx_burst_mode_get,
	.tx_burst_mode_get = mlx5_tx_burst_mode_get,
	.get_module_info = mlx5_get_module_info,
	.get_module_eeprom = mlx5_get_module_eeprom,
};

/* Available operations in flow isolated mode. */
const struct eth_dev_ops mlx5_dev_ops_isolate = {
	.dev_configure = mlx5_dev_configure,
	.dev_start = mlx5_dev_start,
	.dev_stop = mlx5_dev_stop,
	.dev_set_link_down = mlx5_set_link_down,
	.dev_set_link_up = mlx5_set_link_up,
	.dev_close = mlx5_dev_close,
	.promiscuous_enable = mlx5_promiscuous_enable,
	.promiscuous_disable = mlx5_promiscuous_disable,
	.allmulticast_enable = mlx5_allmulticast_enable,
	.allmulticast_disable = mlx5_allmulticast_disable,
	.link_update = mlx5_link_update,
	.stats_get = mlx5_stats_get,
	.stats_reset = mlx5_stats_reset,
	.xstats_get = mlx5_xstats_get,
	.xstats_reset = mlx5_xstats_reset,
	.xstats_get_names = mlx5_xstats_get_names,
	.fw_version_get = mlx5_fw_version_get,
	.dev_infos_get = mlx5_dev_infos_get,
	.dev_supported_ptypes_get = mlx5_dev_supported_ptypes_get,
	.vlan_filter_set = mlx5_vlan_filter_set,
	.rx_queue_setup = mlx5_rx_queue_setup,
	.rx_hairpin_queue_setup = mlx5_rx_hairpin_queue_setup,
	.tx_queue_setup = mlx5_tx_queue_setup,
	.tx_hairpin_queue_setup = mlx5_tx_hairpin_queue_setup,
	.rx_queue_release = mlx5_rx_queue_release,
	.tx_queue_release = mlx5_tx_queue_release,
	.flow_ctrl_get = mlx5_dev_get_flow_ctrl,
	.flow_ctrl_set = mlx5_dev_set_flow_ctrl,
	.mac_addr_remove = mlx5_mac_addr_remove,
	.mac_addr_add = mlx5_mac_addr_add,
	.mac_addr_set = mlx5_mac_addr_set,
	.set_mc_addr_list = mlx5_set_mc_addr_list,
	.mtu_set = mlx5_dev_set_mtu,
	.vlan_strip_queue_set = mlx5_vlan_strip_queue_set,
	.vlan_offload_set = mlx5_vlan_offload_set,
	.filter_ctrl = mlx5_dev_filter_ctrl,
	.rx_descriptor_status = mlx5_rx_descriptor_status,
	.tx_descriptor_status = mlx5_tx_descriptor_status,
	.rxq_info_get = mlx5_rxq_info_get,
	.txq_info_get = mlx5_txq_info_get,
	.rx_burst_mode_get = mlx5_rx_burst_mode_get,
	.tx_burst_mode_get = mlx5_tx_burst_mode_get,
	.rx_queue_intr_enable = mlx5_rx_intr_enable,
	.rx_queue_intr_disable = mlx5_rx_intr_disable,
	.is_removed = mlx5_is_removed,
	.get_module_info = mlx5_get_module_info,
	.get_module_eeprom = mlx5_get_module_eeprom,
	.hairpin_cap_get = mlx5_hairpin_cap_get,
	.mtr_ops_get = mlx5_flow_meter_ops_get,
};

/**
 * Verify and store value for device argument.
 *
 * @param[in] key
 *   Key argument to verify.
 * @param[in] val
 *   Value associated with key.
 * @param opaque
 *   User data.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_args_check(const char *key, const char *val, void *opaque)
{
	struct mlx5_dev_config *config = opaque;
	unsigned long tmp;

	/* No-op, port representors are processed in mlx5_dev_spawn(). */
	if (!strcmp(MLX5_REPRESENTOR, key))
		return 0;
	errno = 0;
	tmp = strtoul(val, NULL, 0);
	if (errno) {
		rte_errno = errno;
		DRV_LOG(WARNING, "%s: \"%s\" is not a valid integer", key, val);
		return -rte_errno;
	}
	if (strcmp(MLX5_RXQ_CQE_COMP_EN, key) == 0) {
		config->cqe_comp = !!tmp;
	} else if (strcmp(MLX5_RXQ_CQE_PAD_EN, key) == 0) {
		config->cqe_pad = !!tmp;
	} else if (strcmp(MLX5_RXQ_PKT_PAD_EN, key) == 0) {
		config->hw_padding = !!tmp;
	} else if (strcmp(MLX5_RX_MPRQ_EN, key) == 0) {
		config->mprq.enabled = !!tmp;
	} else if (strcmp(MLX5_RX_MPRQ_LOG_STRIDE_NUM, key) == 0) {
		config->mprq.stride_num_n = tmp;
	} else if (strcmp(MLX5_RX_MPRQ_MAX_MEMCPY_LEN, key) == 0) {
		config->mprq.max_memcpy_len = tmp;
	} else if (strcmp(MLX5_RXQS_MIN_MPRQ, key) == 0) {
		config->mprq.min_rxqs_num = tmp;
	} else if (strcmp(MLX5_TXQ_INLINE, key) == 0) {
		DRV_LOG(WARNING, "%s: deprecated parameter,"
				 " converted to txq_inline_max", key);
		config->txq_inline_max = tmp;
	} else if (strcmp(MLX5_TXQ_INLINE_MAX, key) == 0) {
		config->txq_inline_max = tmp;
	} else if (strcmp(MLX5_TXQ_INLINE_MIN, key) == 0) {
		config->txq_inline_min = tmp;
	} else if (strcmp(MLX5_TXQ_INLINE_MPW, key) == 0) {
		config->txq_inline_mpw = tmp;
	} else if (strcmp(MLX5_TXQS_MIN_INLINE, key) == 0) {
		config->txqs_inline = tmp;
	} else if (strcmp(MLX5_TXQS_MAX_VEC, key) == 0) {
		DRV_LOG(WARNING, "%s: deprecated parameter, ignored", key);
	} else if (strcmp(MLX5_TXQ_MPW_EN, key) == 0) {
		config->mps = !!tmp;
	} else if (strcmp(MLX5_TX_DB_NC, key) == 0) {
		if (tmp != MLX5_TXDB_CACHED &&
		    tmp != MLX5_TXDB_NCACHED &&
		    tmp != MLX5_TXDB_HEURISTIC) {
			DRV_LOG(ERR, "invalid Tx doorbell "
				     "mapping parameter");
			rte_errno = EINVAL;
			return -rte_errno;
		}
		config->dbnc = tmp;
	} else if (strcmp(MLX5_TXQ_MPW_HDR_DSEG_EN, key) == 0) {
		DRV_LOG(WARNING, "%s: deprecated parameter, ignored", key);
	} else if (strcmp(MLX5_TXQ_MAX_INLINE_LEN, key) == 0) {
		DRV_LOG(WARNING, "%s: deprecated parameter,"
				 " converted to txq_inline_mpw", key);
		config->txq_inline_mpw = tmp;
	} else if (strcmp(MLX5_TX_VEC_EN, key) == 0) {
		DRV_LOG(WARNING, "%s: deprecated parameter, ignored", key);
	} else if (strcmp(MLX5_RX_VEC_EN, key) == 0) {
		config->rx_vec_en = !!tmp;
	} else if (strcmp(MLX5_L3_VXLAN_EN, key) == 0) {
		config->l3_vxlan_en = !!tmp;
	} else if (strcmp(MLX5_VF_NL_EN, key) == 0) {
		config->vf_nl_en = !!tmp;
	} else if (strcmp(MLX5_DV_ESW_EN, key) == 0) {
		config->dv_esw_en = !!tmp;
	} else if (strcmp(MLX5_DV_FLOW_EN, key) == 0) {
		config->dv_flow_en = !!tmp;
	} else if (strcmp(MLX5_DV_XMETA_EN, key) == 0) {
		if (tmp != MLX5_XMETA_MODE_LEGACY &&
		    tmp != MLX5_XMETA_MODE_META16 &&
		    tmp != MLX5_XMETA_MODE_META32) {
			DRV_LOG(ERR, "invalid extensive "
				     "metadata parameter");
			rte_errno = EINVAL;
			return -rte_errno;
		}
		config->dv_xmeta_en = tmp;
	} else if (strcmp(MLX5_MR_EXT_MEMSEG_EN, key) == 0) {
		config->mr_ext_memseg_en = !!tmp;
	} else if (strcmp(MLX5_MAX_DUMP_FILES_NUM, key) == 0) {
		config->max_dump_files_num = tmp;
	} else if (strcmp(MLX5_LRO_TIMEOUT_USEC, key) == 0) {
		config->lro.timeout = tmp;
	} else if (strcmp(MLX5_CLASS_ARG_NAME, key) == 0) {
		DRV_LOG(DEBUG, "class argument is %s.", val);
	} else {
		DRV_LOG(WARNING, "%s: unknown parameter", key);
		rte_errno = EINVAL;
		return -rte_errno;
	}
	return 0;
}

/**
 * Parse device parameters.
 *
 * @param config
 *   Pointer to device configuration structure.
 * @param devargs
 *   Device arguments structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_args(struct mlx5_dev_config *config, struct rte_devargs *devargs)
{
	const char **params = (const char *[]){
		MLX5_RXQ_CQE_COMP_EN,
		MLX5_RXQ_CQE_PAD_EN,
		MLX5_RXQ_PKT_PAD_EN,
		MLX5_RX_MPRQ_EN,
		MLX5_RX_MPRQ_LOG_STRIDE_NUM,
		MLX5_RX_MPRQ_MAX_MEMCPY_LEN,
		MLX5_RXQS_MIN_MPRQ,
		MLX5_TXQ_INLINE,
		MLX5_TXQ_INLINE_MIN,
		MLX5_TXQ_INLINE_MAX,
		MLX5_TXQ_INLINE_MPW,
		MLX5_TXQS_MIN_INLINE,
		MLX5_TXQS_MAX_VEC,
		MLX5_TXQ_MPW_EN,
		MLX5_TXQ_MPW_HDR_DSEG_EN,
		MLX5_TXQ_MAX_INLINE_LEN,
		MLX5_TX_DB_NC,
		MLX5_TX_VEC_EN,
		MLX5_RX_VEC_EN,
		MLX5_L3_VXLAN_EN,
		MLX5_VF_NL_EN,
		MLX5_DV_ESW_EN,
		MLX5_DV_FLOW_EN,
		MLX5_DV_XMETA_EN,
		MLX5_MR_EXT_MEMSEG_EN,
		MLX5_REPRESENTOR,
		MLX5_MAX_DUMP_FILES_NUM,
		MLX5_LRO_TIMEOUT_USEC,
		MLX5_CLASS_ARG_NAME,
		NULL,
	};
	struct rte_kvargs *kvlist;
	int ret = 0;
	int i;

	if (devargs == NULL)
		return 0;
	/* Following UGLY cast is done to pass checkpatch. */
	kvlist = rte_kvargs_parse(devargs->args, params);
	if (kvlist == NULL) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	/* Process parameters. */
	for (i = 0; (params[i] != NULL); ++i) {
		if (rte_kvargs_count(kvlist, params[i])) {
			ret = rte_kvargs_process(kvlist, params[i],
						 mlx5_args_check, config);
			if (ret) {
				rte_errno = EINVAL;
				rte_kvargs_free(kvlist);
				return -rte_errno;
			}
		}
	}
	rte_kvargs_free(kvlist);
	return 0;
}

static struct rte_pci_driver mlx5_driver;

/**
 * PMD global initialization.
 *
 * Independent from individual device, this function initializes global
 * per-PMD data structures distinguishing primary and secondary processes.
 * Hence, each initialization is called once per a process.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_init_once(void)
{
	struct mlx5_shared_data *sd;
	struct mlx5_local_data *ld = &mlx5_local_data;
	int ret = 0;

	if (mlx5_init_shared_data())
		return -rte_errno;
	sd = mlx5_shared_data;
	MLX5_ASSERT(sd);
	rte_spinlock_lock(&sd->lock);
	switch (rte_eal_process_type()) {
	case RTE_PROC_PRIMARY:
		if (sd->init_done)
			break;
		LIST_INIT(&sd->mem_event_cb_list);
		rte_rwlock_init(&sd->mem_event_rwlock);
		rte_mem_event_callback_register("MLX5_MEM_EVENT_CB",
						mlx5_mr_mem_event_cb, NULL);
		ret = mlx5_mp_init_primary();
		if (ret)
			goto out;
		sd->init_done = true;
		break;
	case RTE_PROC_SECONDARY:
		if (ld->init_done)
			break;
		ret = mlx5_mp_init_secondary();
		if (ret)
			goto out;
		++sd->secondary_cnt;
		ld->init_done = true;
		break;
	default:
		break;
	}
out:
	rte_spinlock_unlock(&sd->lock);
	return ret;
}

/**
 * Configures the minimal amount of data to inline into WQE
 * while sending packets.
 *
 * - the txq_inline_min has the maximal priority, if this
 *   key is specified in devargs
 * - if DevX is enabled the inline mode is queried from the
 *   device (HCA attributes and NIC vport context if needed).
 * - otherwise L2 mode (18 bytes) is assumed for ConnectX-4/4 Lx
 *   and none (0 bytes) for other NICs
 *
 * @param spawn
 *   Verbs device parameters (name, port, switch_info) to spawn.
 * @param config
 *   Device configuration parameters.
 */
static void
mlx5_set_min_inline(struct mlx5_dev_spawn_data *spawn,
		    struct mlx5_dev_config *config)
{
	if (config->txq_inline_min != MLX5_ARG_UNSET) {
		/* Application defines size of inlined data explicitly. */
		switch (spawn->pci_dev->id.device_id) {
		case PCI_DEVICE_ID_MELLANOX_CONNECTX4:
		case PCI_DEVICE_ID_MELLANOX_CONNECTX4VF:
			if (config->txq_inline_min <
				       (int)MLX5_INLINE_HSIZE_L2) {
				DRV_LOG(DEBUG,
					"txq_inline_mix aligned to minimal"
					" ConnectX-4 required value %d",
					(int)MLX5_INLINE_HSIZE_L2);
				config->txq_inline_min = MLX5_INLINE_HSIZE_L2;
			}
			break;
		}
		goto exit;
	}
	if (config->hca_attr.eth_net_offloads) {
		/* We have DevX enabled, inline mode queried successfully. */
		switch (config->hca_attr.wqe_inline_mode) {
		case MLX5_CAP_INLINE_MODE_L2:
			/* outer L2 header must be inlined. */
			config->txq_inline_min = MLX5_INLINE_HSIZE_L2;
			goto exit;
		case MLX5_CAP_INLINE_MODE_NOT_REQUIRED:
			/* No inline data are required by NIC. */
			config->txq_inline_min = MLX5_INLINE_HSIZE_NONE;
			config->hw_vlan_insert =
				config->hca_attr.wqe_vlan_insert;
			DRV_LOG(DEBUG, "Tx VLAN insertion is supported");
			goto exit;
		case MLX5_CAP_INLINE_MODE_VPORT_CONTEXT:
			/* inline mode is defined by NIC vport context. */
			if (!config->hca_attr.eth_virt)
				break;
			switch (config->hca_attr.vport_inline_mode) {
			case MLX5_INLINE_MODE_NONE:
				config->txq_inline_min =
					MLX5_INLINE_HSIZE_NONE;
				goto exit;
			case MLX5_INLINE_MODE_L2:
				config->txq_inline_min =
					MLX5_INLINE_HSIZE_L2;
				goto exit;
			case MLX5_INLINE_MODE_IP:
				config->txq_inline_min =
					MLX5_INLINE_HSIZE_L3;
				goto exit;
			case MLX5_INLINE_MODE_TCP_UDP:
				config->txq_inline_min =
					MLX5_INLINE_HSIZE_L4;
				goto exit;
			case MLX5_INLINE_MODE_INNER_L2:
				config->txq_inline_min =
					MLX5_INLINE_HSIZE_INNER_L2;
				goto exit;
			case MLX5_INLINE_MODE_INNER_IP:
				config->txq_inline_min =
					MLX5_INLINE_HSIZE_INNER_L3;
				goto exit;
			case MLX5_INLINE_MODE_INNER_TCP_UDP:
				config->txq_inline_min =
					MLX5_INLINE_HSIZE_INNER_L4;
				goto exit;
			}
		}
	}
	/*
	 * We get here if we are unable to deduce
	 * inline data size with DevX. Try PCI ID
	 * to determine old NICs.
	 */
	switch (spawn->pci_dev->id.device_id) {
	case PCI_DEVICE_ID_MELLANOX_CONNECTX4:
	case PCI_DEVICE_ID_MELLANOX_CONNECTX4VF:
	case PCI_DEVICE_ID_MELLANOX_CONNECTX4LX:
	case PCI_DEVICE_ID_MELLANOX_CONNECTX4LXVF:
		config->txq_inline_min = MLX5_INLINE_HSIZE_L2;
		config->hw_vlan_insert = 0;
		break;
	case PCI_DEVICE_ID_MELLANOX_CONNECTX5:
	case PCI_DEVICE_ID_MELLANOX_CONNECTX5VF:
	case PCI_DEVICE_ID_MELLANOX_CONNECTX5EX:
	case PCI_DEVICE_ID_MELLANOX_CONNECTX5EXVF:
		/*
		 * These NICs support VLAN insertion from WQE and
		 * report the wqe_vlan_insert flag. But there is the bug
		 * and PFC control may be broken, so disable feature.
		 */
		config->hw_vlan_insert = 0;
		config->txq_inline_min = MLX5_INLINE_HSIZE_NONE;
		break;
	default:
		config->txq_inline_min = MLX5_INLINE_HSIZE_NONE;
		break;
	}
exit:
	DRV_LOG(DEBUG, "min tx inline configured: %d", config->txq_inline_min);
}

/**
 * Configures the metadata mask fields in the shared context.
 *
 * @param [in] dev
 *   Pointer to Ethernet device.
 */
static void
mlx5_set_metadata_mask(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_ibv_shared *sh = priv->sh;
	uint32_t meta, mark, reg_c0;

	reg_c0 = ~priv->vport_meta_mask;
	switch (priv->config.dv_xmeta_en) {
	case MLX5_XMETA_MODE_LEGACY:
		meta = UINT32_MAX;
		mark = MLX5_FLOW_MARK_MASK;
		break;
	case MLX5_XMETA_MODE_META16:
		meta = reg_c0 >> rte_bsf32(reg_c0);
		mark = MLX5_FLOW_MARK_MASK;
		break;
	case MLX5_XMETA_MODE_META32:
		meta = UINT32_MAX;
		mark = (reg_c0 >> rte_bsf32(reg_c0)) & MLX5_FLOW_MARK_MASK;
		break;
	default:
		meta = 0;
		mark = 0;
		MLX5_ASSERT(false);
		break;
	}
	if (sh->dv_mark_mask && sh->dv_mark_mask != mark)
		DRV_LOG(WARNING, "metadata MARK mask mismatche %08X:%08X",
				 sh->dv_mark_mask, mark);
	else
		sh->dv_mark_mask = mark;
	if (sh->dv_meta_mask && sh->dv_meta_mask != meta)
		DRV_LOG(WARNING, "metadata META mask mismatche %08X:%08X",
				 sh->dv_meta_mask, meta);
	else
		sh->dv_meta_mask = meta;
	if (sh->dv_regc0_mask && sh->dv_regc0_mask != reg_c0)
		DRV_LOG(WARNING, "metadata reg_c0 mask mismatche %08X:%08X",
				 sh->dv_meta_mask, reg_c0);
	else
		sh->dv_regc0_mask = reg_c0;
	DRV_LOG(DEBUG, "metadata mode %u", priv->config.dv_xmeta_en);
	DRV_LOG(DEBUG, "metadata MARK mask %08X", sh->dv_mark_mask);
	DRV_LOG(DEBUG, "metadata META mask %08X", sh->dv_meta_mask);
	DRV_LOG(DEBUG, "metadata reg_c0 mask %08X", sh->dv_regc0_mask);
}

/**
 * Allocate page of door-bells and register it using DevX API.
 *
 * @param [in] dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   Pointer to new page on success, NULL otherwise.
 */
static struct mlx5_devx_dbr_page *
mlx5_alloc_dbr_page(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_devx_dbr_page *page;

	/* Allocate space for door-bell page and management data. */
	page = rte_calloc_socket(__func__, 1, sizeof(struct mlx5_devx_dbr_page),
				 RTE_CACHE_LINE_SIZE, dev->device->numa_node);
	if (!page) {
		DRV_LOG(ERR, "port %u cannot allocate dbr page",
			dev->data->port_id);
		return NULL;
	}
	/* Register allocated memory. */
	page->umem = mlx5_glue->devx_umem_reg(priv->sh->ctx, page->dbrs,
					      MLX5_DBR_PAGE_SIZE, 0);
	if (!page->umem) {
		DRV_LOG(ERR, "port %u cannot umem reg dbr page",
			dev->data->port_id);
		rte_free(page);
		return NULL;
	}
	return page;
}

/**
 * Find the next available door-bell, allocate new page if needed.
 *
 * @param [in] dev
 *   Pointer to Ethernet device.
 * @param [out] dbr_page
 *   Door-bell page containing the page data.
 *
 * @return
 *   Door-bell address offset on success, a negative error value otherwise.
 */
int64_t
mlx5_get_dbr(struct rte_eth_dev *dev, struct mlx5_devx_dbr_page **dbr_page)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_devx_dbr_page *page = NULL;
	uint32_t i, j;

	LIST_FOREACH(page, &priv->dbrpgs, next)
		if (page->dbr_count < MLX5_DBR_PER_PAGE)
			break;
	if (!page) { /* No page with free door-bell exists. */
		page = mlx5_alloc_dbr_page(dev);
		if (!page) /* Failed to allocate new page. */
			return (-1);
		LIST_INSERT_HEAD(&priv->dbrpgs, page, next);
	}
	/* Loop to find bitmap part with clear bit. */
	for (i = 0;
	     i < MLX5_DBR_BITMAP_SIZE && page->dbr_bitmap[i] == UINT64_MAX;
	     i++)
		; /* Empty. */
	/* Find the first clear bit. */
	j = rte_bsf64(~page->dbr_bitmap[i]);
	MLX5_ASSERT(i < (MLX5_DBR_PER_PAGE / 64));
	page->dbr_bitmap[i] |= (1 << j);
	page->dbr_count++;
	*dbr_page = page;
	return (((i * 64) + j) * sizeof(uint64_t));
}

/**
 * Release a door-bell record.
 *
 * @param [in] dev
 *   Pointer to Ethernet device.
 * @param [in] umem_id
 *   UMEM ID of page containing the door-bell record to release.
 * @param [in] offset
 *   Offset of door-bell record in page.
 *
 * @return
 *   0 on success, a negative error value otherwise.
 */
int32_t
mlx5_release_dbr(struct rte_eth_dev *dev, uint32_t umem_id, uint64_t offset)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_devx_dbr_page *page = NULL;
	int ret = 0;

	LIST_FOREACH(page, &priv->dbrpgs, next)
		/* Find the page this address belongs to. */
		if (page->umem->umem_id == umem_id)
			break;
	if (!page)
		return -EINVAL;
	page->dbr_count--;
	if (!page->dbr_count) {
		/* Page not used, free it and remove from list. */
		LIST_REMOVE(page, next);
		if (page->umem)
			ret = -mlx5_glue->devx_umem_dereg(page->umem);
		rte_free(page);
	} else {
		/* Mark in bitmap that this door-bell is not in use. */
		offset /= MLX5_DBR_SIZE;
		int i = offset / 64;
		int j = offset % 64;

		page->dbr_bitmap[i] &= ~(1 << j);
	}
	return ret;
}

int
rte_pmd_mlx5_get_dyn_flag_names(char *names[], unsigned int n)
{
	static const char *const dynf_names[] = {
		RTE_PMD_MLX5_FINE_GRANULARITY_INLINE,
		RTE_MBUF_DYNFLAG_METADATA_NAME
	};
	unsigned int i;

	if (n < RTE_DIM(dynf_names))
		return -ENOMEM;
	for (i = 0; i < RTE_DIM(dynf_names); i++) {
		if (names[i] == NULL)
			return -EINVAL;
		strcpy(names[i], dynf_names[i]);
	}
	return RTE_DIM(dynf_names);
}

/**
 * Check sibling device configurations.
 *
 * Sibling devices sharing the Infiniband device context
 * should have compatible configurations. This regards
 * representors and bonding slaves.
 *
 * @param priv
 *   Private device descriptor.
 * @param config
 *   Configuration of the device is going to be created.
 *
 * @return
 *   0 on success, EINVAL otherwise
 */
static int
mlx5_dev_check_sibling_config(struct mlx5_priv *priv,
			      struct mlx5_dev_config *config)
{
	struct mlx5_ibv_shared *sh = priv->sh;
	struct mlx5_dev_config *sh_conf = NULL;
	uint16_t port_id;

	MLX5_ASSERT(sh);
	/* Nothing to compare for the single/first device. */
	if (sh->refcnt == 1)
		return 0;
	/* Find the device with shared context. */
	MLX5_ETH_FOREACH_DEV(port_id, priv->pci_dev) {
		struct mlx5_priv *opriv =
			rte_eth_devices[port_id].data->dev_private;

		if (opriv && opriv != priv && opriv->sh == sh) {
			sh_conf = &opriv->config;
			break;
		}
	}
	if (!sh_conf)
		return 0;
	if (sh_conf->dv_flow_en ^ config->dv_flow_en) {
		DRV_LOG(ERR, "\"dv_flow_en\" configuration mismatch"
			     " for shared %s context", sh->ibdev_name);
		rte_errno = EINVAL;
		return rte_errno;
	}
	if (sh_conf->dv_xmeta_en ^ config->dv_xmeta_en) {
		DRV_LOG(ERR, "\"dv_xmeta_en\" configuration mismatch"
			     " for shared %s context", sh->ibdev_name);
		rte_errno = EINVAL;
		return rte_errno;
	}
	return 0;
}
/**
 * Spawn an Ethernet device from Verbs information.
 *
 * @param dpdk_dev
 *   Backing DPDK device.
 * @param spawn
 *   Verbs device parameters (name, port, switch_info) to spawn.
 * @param config
 *   Device configuration parameters.
 *
 * @return
 *   A valid Ethernet device object on success, NULL otherwise and rte_errno
 *   is set. The following errors are defined:
 *
 *   EBUSY: device is not supposed to be spawned.
 *   EEXIST: device is already spawned
 */
static struct rte_eth_dev *
mlx5_dev_spawn(struct rte_device *dpdk_dev,
	       struct mlx5_dev_spawn_data *spawn,
	       struct mlx5_dev_config config)
{
	const struct mlx5_switch_info *switch_info = &spawn->info;
	struct mlx5_ibv_shared *sh = NULL;
	struct ibv_port_attr port_attr;
	struct mlx5dv_context dv_attr = { .comp_mask = 0 };
	struct rte_eth_dev *eth_dev = NULL;
	struct mlx5_priv *priv = NULL;
	int err = 0;
	unsigned int hw_padding = 0;
	unsigned int mps;
	unsigned int cqe_comp;
	unsigned int cqe_pad = 0;
	unsigned int tunnel_en = 0;
	unsigned int mpls_en = 0;
	unsigned int swp = 0;
	unsigned int mprq = 0;
	unsigned int mprq_min_stride_size_n = 0;
	unsigned int mprq_max_stride_size_n = 0;
	unsigned int mprq_min_stride_num_n = 0;
	unsigned int mprq_max_stride_num_n = 0;
	struct rte_ether_addr mac;
	char name[RTE_ETH_NAME_MAX_LEN];
	int own_domain_id = 0;
	uint16_t port_id;
	unsigned int i;
#ifdef HAVE_MLX5DV_DR_DEVX_PORT
	struct mlx5dv_devx_port devx_port = { .comp_mask = 0 };
#endif

	/* Determine if this port representor is supposed to be spawned. */
	if (switch_info->representor && dpdk_dev->devargs) {
		struct rte_eth_devargs eth_da;

		err = rte_eth_devargs_parse(dpdk_dev->devargs->args, &eth_da);
		if (err) {
			rte_errno = -err;
			DRV_LOG(ERR, "failed to process device arguments: %s",
				strerror(rte_errno));
			return NULL;
		}
		for (i = 0; i < eth_da.nb_representor_ports; ++i)
			if (eth_da.representor_ports[i] ==
			    (uint16_t)switch_info->port_name)
				break;
		if (i == eth_da.nb_representor_ports) {
			rte_errno = EBUSY;
			return NULL;
		}
	}
	/* Build device name. */
	if (spawn->pf_bond <  0) {
		/* Single device. */
		if (!switch_info->representor)
			strlcpy(name, dpdk_dev->name, sizeof(name));
		else
			snprintf(name, sizeof(name), "%s_representor_%u",
				 dpdk_dev->name, switch_info->port_name);
	} else {
		/* Bonding device. */
		if (!switch_info->representor)
			snprintf(name, sizeof(name), "%s_%s",
				 dpdk_dev->name, spawn->ibv_dev->name);
		else
			snprintf(name, sizeof(name), "%s_%s_representor_%u",
				 dpdk_dev->name, spawn->ibv_dev->name,
				 switch_info->port_name);
	}
	/* check if the device is already spawned */
	if (rte_eth_dev_get_port_by_name(name, &port_id) == 0) {
		rte_errno = EEXIST;
		return NULL;
	}
	DRV_LOG(DEBUG, "naming Ethernet device \"%s\"", name);
	if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		eth_dev = rte_eth_dev_attach_secondary(name);
		if (eth_dev == NULL) {
			DRV_LOG(ERR, "can not attach rte ethdev");
			rte_errno = ENOMEM;
			return NULL;
		}
		eth_dev->device = dpdk_dev;
		eth_dev->dev_ops = &mlx5_dev_sec_ops;
		err = mlx5_proc_priv_init(eth_dev);
		if (err)
			return NULL;
		/* Receive command fd from primary process */
		err = mlx5_mp_req_verbs_cmd_fd(eth_dev);
		if (err < 0)
			return NULL;
		/* Remap UAR for Tx queues. */
		err = mlx5_tx_uar_init_secondary(eth_dev, err);
		if (err)
			return NULL;
		/*
		 * Ethdev pointer is still required as input since
		 * the primary device is not accessible from the
		 * secondary process.
		 */
		eth_dev->rx_pkt_burst = mlx5_select_rx_function(eth_dev);
		eth_dev->tx_pkt_burst = mlx5_select_tx_function(eth_dev);
		return eth_dev;
	}
	/*
	 * Some parameters ("tx_db_nc" in particularly) are needed in
	 * advance to create dv/verbs device context. We proceed the
	 * devargs here to get ones, and later proceed devargs again
	 * to override some hardware settings.
	 */
	err = mlx5_args(&config, dpdk_dev->devargs);
	if (err) {
		err = rte_errno;
		DRV_LOG(ERR, "failed to process device arguments: %s",
			strerror(rte_errno));
		goto error;
	}
	sh = mlx5_alloc_shared_ibctx(spawn, &config);
	if (!sh)
		return NULL;
	config.devx = sh->devx;
#ifdef HAVE_MLX5DV_DR_ACTION_DEST_DEVX_TIR
	config.dest_tir = 1;
#endif
#ifdef HAVE_IBV_MLX5_MOD_SWP
	dv_attr.comp_mask |= MLX5DV_CONTEXT_MASK_SWP;
#endif
	/*
	 * Multi-packet send is supported by ConnectX-4 Lx PF as well
	 * as all ConnectX-5 devices.
	 */
#ifdef HAVE_IBV_DEVICE_TUNNEL_SUPPORT
	dv_attr.comp_mask |= MLX5DV_CONTEXT_MASK_TUNNEL_OFFLOADS;
#endif
#ifdef HAVE_IBV_DEVICE_STRIDING_RQ_SUPPORT
	dv_attr.comp_mask |= MLX5DV_CONTEXT_MASK_STRIDING_RQ;
#endif
	mlx5_glue->dv_query_device(sh->ctx, &dv_attr);
	if (dv_attr.flags & MLX5DV_CONTEXT_FLAGS_MPW_ALLOWED) {
		if (dv_attr.flags & MLX5DV_CONTEXT_FLAGS_ENHANCED_MPW) {
			DRV_LOG(DEBUG, "enhanced MPW is supported");
			mps = MLX5_MPW_ENHANCED;
		} else {
			DRV_LOG(DEBUG, "MPW is supported");
			mps = MLX5_MPW;
		}
	} else {
		DRV_LOG(DEBUG, "MPW isn't supported");
		mps = MLX5_MPW_DISABLED;
	}
#ifdef HAVE_IBV_MLX5_MOD_SWP
	if (dv_attr.comp_mask & MLX5DV_CONTEXT_MASK_SWP)
		swp = dv_attr.sw_parsing_caps.sw_parsing_offloads;
	DRV_LOG(DEBUG, "SWP support: %u", swp);
#endif
	config.swp = !!swp;
#ifdef HAVE_IBV_DEVICE_STRIDING_RQ_SUPPORT
	if (dv_attr.comp_mask & MLX5DV_CONTEXT_MASK_STRIDING_RQ) {
		struct mlx5dv_striding_rq_caps mprq_caps =
			dv_attr.striding_rq_caps;

		DRV_LOG(DEBUG, "\tmin_single_stride_log_num_of_bytes: %d",
			mprq_caps.min_single_stride_log_num_of_bytes);
		DRV_LOG(DEBUG, "\tmax_single_stride_log_num_of_bytes: %d",
			mprq_caps.max_single_stride_log_num_of_bytes);
		DRV_LOG(DEBUG, "\tmin_single_wqe_log_num_of_strides: %d",
			mprq_caps.min_single_wqe_log_num_of_strides);
		DRV_LOG(DEBUG, "\tmax_single_wqe_log_num_of_strides: %d",
			mprq_caps.max_single_wqe_log_num_of_strides);
		DRV_LOG(DEBUG, "\tsupported_qpts: %d",
			mprq_caps.supported_qpts);
		DRV_LOG(DEBUG, "device supports Multi-Packet RQ");
		mprq = 1;
		mprq_min_stride_size_n =
			mprq_caps.min_single_stride_log_num_of_bytes;
		mprq_max_stride_size_n =
			mprq_caps.max_single_stride_log_num_of_bytes;
		mprq_min_stride_num_n =
			mprq_caps.min_single_wqe_log_num_of_strides;
		mprq_max_stride_num_n =
			mprq_caps.max_single_wqe_log_num_of_strides;
		config.mprq.stride_num_n = RTE_MAX(MLX5_MPRQ_STRIDE_NUM_N,
						   mprq_min_stride_num_n);
	}
#endif
	if (RTE_CACHE_LINE_SIZE == 128 &&
	    !(dv_attr.flags & MLX5DV_CONTEXT_FLAGS_CQE_128B_COMP))
		cqe_comp = 0;
	else
		cqe_comp = 1;
	config.cqe_comp = cqe_comp;
#ifdef HAVE_IBV_MLX5_MOD_CQE_128B_PAD
	/* Whether device supports 128B Rx CQE padding. */
	cqe_pad = RTE_CACHE_LINE_SIZE == 128 &&
		  (dv_attr.flags & MLX5DV_CONTEXT_FLAGS_CQE_128B_PAD);
#endif
#ifdef HAVE_IBV_DEVICE_TUNNEL_SUPPORT
	if (dv_attr.comp_mask & MLX5DV_CONTEXT_MASK_TUNNEL_OFFLOADS) {
		tunnel_en = ((dv_attr.tunnel_offloads_caps &
			      MLX5DV_RAW_PACKET_CAP_TUNNELED_OFFLOAD_VXLAN) &&
			     (dv_attr.tunnel_offloads_caps &
			      MLX5DV_RAW_PACKET_CAP_TUNNELED_OFFLOAD_GRE) &&
			     (dv_attr.tunnel_offloads_caps &
			      MLX5DV_RAW_PACKET_CAP_TUNNELED_OFFLOAD_GENEVE));
	}
	DRV_LOG(DEBUG, "tunnel offloading is %ssupported",
		tunnel_en ? "" : "not ");
#else
	DRV_LOG(WARNING,
		"tunnel offloading disabled due to old OFED/rdma-core version");
#endif
	config.tunnel_en = tunnel_en;
#ifdef HAVE_IBV_DEVICE_MPLS_SUPPORT
	mpls_en = ((dv_attr.tunnel_offloads_caps &
		    MLX5DV_RAW_PACKET_CAP_TUNNELED_OFFLOAD_CW_MPLS_OVER_GRE) &&
		   (dv_attr.tunnel_offloads_caps &
		    MLX5DV_RAW_PACKET_CAP_TUNNELED_OFFLOAD_CW_MPLS_OVER_UDP));
	DRV_LOG(DEBUG, "MPLS over GRE/UDP tunnel offloading is %ssupported",
		mpls_en ? "" : "not ");
#else
	DRV_LOG(WARNING, "MPLS over GRE/UDP tunnel offloading disabled due to"
		" old OFED/rdma-core version or firmware configuration");
#endif
	config.mpls_en = mpls_en;
	/* Check port status. */
	err = mlx5_glue->query_port(sh->ctx, spawn->ibv_port, &port_attr);
	if (err) {
		DRV_LOG(ERR, "port query failed: %s", strerror(err));
		goto error;
	}
	if (port_attr.link_layer != IBV_LINK_LAYER_ETHERNET) {
		DRV_LOG(ERR, "port is not configured in Ethernet mode");
		err = EINVAL;
		goto error;
	}
	if (port_attr.state != IBV_PORT_ACTIVE)
		DRV_LOG(DEBUG, "port is not active: \"%s\" (%d)",
			mlx5_glue->port_state_str(port_attr.state),
			port_attr.state);
	/* Allocate private eth device data. */
	priv = rte_zmalloc("ethdev private structure",
			   sizeof(*priv),
			   RTE_CACHE_LINE_SIZE);
	if (priv == NULL) {
		DRV_LOG(ERR, "priv allocation failure");
		err = ENOMEM;
		goto error;
	}
	priv->sh = sh;
	priv->ibv_port = spawn->ibv_port;
	priv->pci_dev = spawn->pci_dev;
	priv->mtu = RTE_ETHER_MTU;
#ifndef RTE_ARCH_64
	/* Initialize UAR access locks for 32bit implementations. */
	rte_spinlock_init(&priv->uar_lock_cq);
	for (i = 0; i < MLX5_UAR_PAGE_NUM_MAX; i++)
		rte_spinlock_init(&priv->uar_lock[i]);
#endif
	/* Some internal functions rely on Netlink sockets, open them now. */
	priv->nl_socket_rdma = mlx5_nl_init(NETLINK_RDMA);
	priv->nl_socket_route =	mlx5_nl_init(NETLINK_ROUTE);
	priv->representor = !!switch_info->representor;
	priv->master = !!switch_info->master;
	priv->domain_id = RTE_ETH_DEV_SWITCH_DOMAIN_ID_INVALID;
	priv->vport_meta_tag = 0;
	priv->vport_meta_mask = 0;
	priv->pf_bond = spawn->pf_bond;
#ifdef HAVE_MLX5DV_DR_DEVX_PORT
	/*
	 * The DevX port query API is implemented. E-Switch may use
	 * either vport or reg_c[0] metadata register to match on
	 * vport index. The engaged part of metadata register is
	 * defined by mask.
	 */
	if (switch_info->representor || switch_info->master) {
		devx_port.comp_mask = MLX5DV_DEVX_PORT_VPORT |
				      MLX5DV_DEVX_PORT_MATCH_REG_C_0;
		err = mlx5_glue->devx_port_query(sh->ctx, spawn->ibv_port,
						 &devx_port);
		if (err) {
			DRV_LOG(WARNING,
				"can't query devx port %d on device %s",
				spawn->ibv_port, spawn->ibv_dev->name);
			devx_port.comp_mask = 0;
		}
	}
	if (devx_port.comp_mask & MLX5DV_DEVX_PORT_MATCH_REG_C_0) {
		priv->vport_meta_tag = devx_port.reg_c_0.value;
		priv->vport_meta_mask = devx_port.reg_c_0.mask;
		if (!priv->vport_meta_mask) {
			DRV_LOG(ERR, "vport zero mask for port %d"
				     " on bonding device %s",
				     spawn->ibv_port, spawn->ibv_dev->name);
			err = ENOTSUP;
			goto error;
		}
		if (priv->vport_meta_tag & ~priv->vport_meta_mask) {
			DRV_LOG(ERR, "invalid vport tag for port %d"
				     " on bonding device %s",
				     spawn->ibv_port, spawn->ibv_dev->name);
			err = ENOTSUP;
			goto error;
		}
	}
	if (devx_port.comp_mask & MLX5DV_DEVX_PORT_VPORT) {
		priv->vport_id = devx_port.vport_num;
	} else if (spawn->pf_bond >= 0) {
		DRV_LOG(ERR, "can't deduce vport index for port %d"
			     " on bonding device %s",
			     spawn->ibv_port, spawn->ibv_dev->name);
		err = ENOTSUP;
		goto error;
	} else {
		/* Suppose vport index in compatible way. */
		priv->vport_id = switch_info->representor ?
				 switch_info->port_name + 1 : -1;
	}
#else
	/*
	 * Kernel/rdma_core support single E-Switch per PF configurations
	 * only and vport_id field contains the vport index for
	 * associated VF, which is deduced from representor port name.
	 * For example, let's have the IB device port 10, it has
	 * attached network device eth0, which has port name attribute
	 * pf0vf2, we can deduce the VF number as 2, and set vport index
	 * as 3 (2+1). This assigning schema should be changed if the
	 * multiple E-Switch instances per PF configurations or/and PCI
	 * subfunctions are added.
	 */
	priv->vport_id = switch_info->representor ?
			 switch_info->port_name + 1 : -1;
#endif
	/* representor_id field keeps the unmodified VF index. */
	priv->representor_id = switch_info->representor ?
			       switch_info->port_name : -1;
	/*
	 * Look for sibling devices in order to reuse their switch domain
	 * if any, otherwise allocate one.
	 */
	MLX5_ETH_FOREACH_DEV(port_id, priv->pci_dev) {
		const struct mlx5_priv *opriv =
			rte_eth_devices[port_id].data->dev_private;

		if (!opriv ||
		    opriv->sh != priv->sh ||
			opriv->domain_id ==
			RTE_ETH_DEV_SWITCH_DOMAIN_ID_INVALID)
			continue;
		priv->domain_id = opriv->domain_id;
		break;
	}
	if (priv->domain_id == RTE_ETH_DEV_SWITCH_DOMAIN_ID_INVALID) {
		err = rte_eth_switch_domain_alloc(&priv->domain_id);
		if (err) {
			err = rte_errno;
			DRV_LOG(ERR, "unable to allocate switch domain: %s",
				strerror(rte_errno));
			goto error;
		}
		own_domain_id = 1;
	}
	/* Override some values set by hardware configuration. */
	mlx5_args(&config, dpdk_dev->devargs);
	err = mlx5_dev_check_sibling_config(priv, &config);
	if (err)
		goto error;
	config.hw_csum = !!(sh->device_attr.device_cap_flags_ex &
			    IBV_DEVICE_RAW_IP_CSUM);
	DRV_LOG(DEBUG, "checksum offloading is %ssupported",
		(config.hw_csum ? "" : "not "));
#if !defined(HAVE_IBV_DEVICE_COUNTERS_SET_V42) && \
	!defined(HAVE_IBV_DEVICE_COUNTERS_SET_V45)
	DRV_LOG(DEBUG, "counters are not supported");
#endif
#if !defined(HAVE_IBV_FLOW_DV_SUPPORT) || !defined(HAVE_MLX5DV_DR)
	if (config.dv_flow_en) {
		DRV_LOG(WARNING, "DV flow is not supported");
		config.dv_flow_en = 0;
	}
#endif
	config.ind_table_max_size =
		sh->device_attr.rss_caps.max_rwq_indirection_table_size;
	/*
	 * Remove this check once DPDK supports larger/variable
	 * indirection tables.
	 */
	if (config.ind_table_max_size > (unsigned int)ETH_RSS_RETA_SIZE_512)
		config.ind_table_max_size = ETH_RSS_RETA_SIZE_512;
	DRV_LOG(DEBUG, "maximum Rx indirection table size is %u",
		config.ind_table_max_size);
	config.hw_vlan_strip = !!(sh->device_attr.raw_packet_caps &
				  IBV_RAW_PACKET_CAP_CVLAN_STRIPPING);
	DRV_LOG(DEBUG, "VLAN stripping is %ssupported",
		(config.hw_vlan_strip ? "" : "not "));
	config.hw_fcs_strip = !!(sh->device_attr.raw_packet_caps &
				 IBV_RAW_PACKET_CAP_SCATTER_FCS);
	DRV_LOG(DEBUG, "FCS stripping configuration is %ssupported",
		(config.hw_fcs_strip ? "" : "not "));
#if defined(HAVE_IBV_WQ_FLAG_RX_END_PADDING)
	hw_padding = !!sh->device_attr.rx_pad_end_addr_align;
#elif defined(HAVE_IBV_WQ_FLAGS_PCI_WRITE_END_PADDING)
	hw_padding = !!(sh->device_attr.device_cap_flags_ex &
			IBV_DEVICE_PCI_WRITE_END_PADDING);
#endif
	if (config.hw_padding && !hw_padding) {
		DRV_LOG(DEBUG, "Rx end alignment padding isn't supported");
		config.hw_padding = 0;
	} else if (config.hw_padding) {
		DRV_LOG(DEBUG, "Rx end alignment padding is enabled");
	}
	config.tso = (sh->device_attr.tso_caps.max_tso > 0 &&
		      (sh->device_attr.tso_caps.supported_qpts &
		       (1 << IBV_QPT_RAW_PACKET)));
	if (config.tso)
		config.tso_max_payload_sz = sh->device_attr.tso_caps.max_tso;
	/*
	 * MPW is disabled by default, while the Enhanced MPW is enabled
	 * by default.
	 */
	if (config.mps == MLX5_ARG_UNSET)
		config.mps = (mps == MLX5_MPW_ENHANCED) ? MLX5_MPW_ENHANCED :
							  MLX5_MPW_DISABLED;
	else
		config.mps = config.mps ? mps : MLX5_MPW_DISABLED;
	DRV_LOG(INFO, "%sMPS is %s",
		config.mps == MLX5_MPW_ENHANCED ? "enhanced " :
		config.mps == MLX5_MPW ? "legacy " : "",
		config.mps != MLX5_MPW_DISABLED ? "enabled" : "disabled");
	if (config.cqe_comp && !cqe_comp) {
		DRV_LOG(WARNING, "Rx CQE compression isn't supported");
		config.cqe_comp = 0;
	}
	if (config.cqe_pad && !cqe_pad) {
		DRV_LOG(WARNING, "Rx CQE padding isn't supported");
		config.cqe_pad = 0;
	} else if (config.cqe_pad) {
		DRV_LOG(INFO, "Rx CQE padding is enabled");
	}
	if (config.devx) {
		priv->counter_fallback = 0;
		err = mlx5_devx_cmd_query_hca_attr(sh->ctx, &config.hca_attr);
		if (err) {
			err = -err;
			goto error;
		}
		if (!config.hca_attr.flow_counters_dump)
			priv->counter_fallback = 1;
#ifndef HAVE_IBV_DEVX_ASYNC
		priv->counter_fallback = 1;
#endif
		if (priv->counter_fallback)
			DRV_LOG(INFO, "Use fall-back DV counter management");
		/* Check for LRO support. */
		if (config.dest_tir && config.hca_attr.lro_cap &&
		    config.dv_flow_en) {
			/* TBD check tunnel lro caps. */
			config.lro.supported = config.hca_attr.lro_cap;
			DRV_LOG(DEBUG, "Device supports LRO");
			/*
			 * If LRO timeout is not configured by application,
			 * use the minimal supported value.
			 */
			if (!config.lro.timeout)
				config.lro.timeout =
				config.hca_attr.lro_timer_supported_periods[0];
			DRV_LOG(DEBUG, "LRO session timeout set to %d usec",
				config.lro.timeout);
		}
#if defined(HAVE_MLX5DV_DR) && defined(HAVE_MLX5_DR_CREATE_ACTION_FLOW_METER)
		if (config.hca_attr.qos.sup && config.hca_attr.qos.srtcm_sup &&
		    config.dv_flow_en) {
			uint8_t reg_c_mask =
				config.hca_attr.qos.flow_meter_reg_c_ids;
			/*
			 * Meter needs two REG_C's for color match and pre-sfx
			 * flow match. Here get the REG_C for color match.
			 * REG_C_0 and REG_C_1 is reserved for metadata feature.
			 */
			reg_c_mask &= 0xfc;
			if (__builtin_popcount(reg_c_mask) < 1) {
				priv->mtr_en = 0;
				DRV_LOG(WARNING, "No available register for"
					" meter.");
			} else {
				priv->mtr_color_reg = ffs(reg_c_mask) - 1 +
						      REG_C_0;
				priv->mtr_en = 1;
				priv->mtr_reg_share =
				      config.hca_attr.qos.flow_meter_reg_share;
				DRV_LOG(DEBUG, "The REG_C meter uses is %d",
					priv->mtr_color_reg);
			}
		}
#endif
	}
	if (config.mprq.enabled && mprq) {
		if (config.mprq.stride_num_n > mprq_max_stride_num_n ||
		    config.mprq.stride_num_n < mprq_min_stride_num_n) {
			config.mprq.stride_num_n =
				RTE_MAX(MLX5_MPRQ_STRIDE_NUM_N,
					mprq_min_stride_num_n);
			DRV_LOG(WARNING,
				"the number of strides"
				" for Multi-Packet RQ is out of range,"
				" setting default value (%u)",
				1 << config.mprq.stride_num_n);
		}
		config.mprq.min_stride_size_n = mprq_min_stride_size_n;
		config.mprq.max_stride_size_n = mprq_max_stride_size_n;
	} else if (config.mprq.enabled && !mprq) {
		DRV_LOG(WARNING, "Multi-Packet RQ isn't supported");
		config.mprq.enabled = 0;
	}
	if (config.max_dump_files_num == 0)
		config.max_dump_files_num = 128;
	eth_dev = rte_eth_dev_allocate(name);
	if (eth_dev == NULL) {
		DRV_LOG(ERR, "can not allocate rte ethdev");
		err = ENOMEM;
		goto error;
	}
	/* Flag to call rte_eth_dev_release_port() in rte_eth_dev_close(). */
	eth_dev->data->dev_flags |= RTE_ETH_DEV_CLOSE_REMOVE;
	if (priv->representor) {
		eth_dev->data->dev_flags |= RTE_ETH_DEV_REPRESENTOR;
		eth_dev->data->representor_id = priv->representor_id;
	}
	/*
	 * Store associated network device interface index. This index
	 * is permanent throughout the lifetime of device. So, we may store
	 * the ifindex here and use the cached value further.
	 */
	MLX5_ASSERT(spawn->ifindex);
	priv->if_index = spawn->ifindex;
	eth_dev->data->dev_private = priv;
	priv->dev_data = eth_dev->data;
	eth_dev->data->mac_addrs = priv->mac;
	eth_dev->device = dpdk_dev;
	/* Configure the first MAC address by default. */
	if (mlx5_get_mac(eth_dev, &mac.addr_bytes)) {
		DRV_LOG(ERR,
			"port %u cannot get MAC address, is mlx5_en"
			" loaded? (errno: %s)",
			eth_dev->data->port_id, strerror(rte_errno));
		err = ENODEV;
		goto error;
	}
	DRV_LOG(INFO,
		"port %u MAC address is %02x:%02x:%02x:%02x:%02x:%02x",
		eth_dev->data->port_id,
		mac.addr_bytes[0], mac.addr_bytes[1],
		mac.addr_bytes[2], mac.addr_bytes[3],
		mac.addr_bytes[4], mac.addr_bytes[5]);
#ifdef RTE_LIBRTE_MLX5_DEBUG
	{
		char ifname[IF_NAMESIZE];

		if (mlx5_get_ifname(eth_dev, &ifname) == 0)
			DRV_LOG(DEBUG, "port %u ifname is \"%s\"",
				eth_dev->data->port_id, ifname);
		else
			DRV_LOG(DEBUG, "port %u ifname is unknown",
				eth_dev->data->port_id);
	}
#endif
	/* Get actual MTU if possible. */
	err = mlx5_get_mtu(eth_dev, &priv->mtu);
	if (err) {
		err = rte_errno;
		goto error;
	}
	DRV_LOG(DEBUG, "port %u MTU is %u", eth_dev->data->port_id,
		priv->mtu);
	/* Initialize burst functions to prevent crashes before link-up. */
	eth_dev->rx_pkt_burst = removed_rx_burst;
	eth_dev->tx_pkt_burst = removed_tx_burst;
	eth_dev->dev_ops = &mlx5_dev_ops;
	/* Register MAC address. */
	claim_zero(mlx5_mac_addr_add(eth_dev, &mac, 0, 0));
	if (config.vf && config.vf_nl_en)
		mlx5_nl_mac_addr_sync(priv->nl_socket_route,
				      mlx5_ifindex(eth_dev),
				      eth_dev->data->mac_addrs,
				      MLX5_MAX_MAC_ADDRESSES);
	TAILQ_INIT(&priv->flows);
	TAILQ_INIT(&priv->ctrl_flows);
	TAILQ_INIT(&priv->flow_meters);
	TAILQ_INIT(&priv->flow_meter_profiles);
	/* Hint libmlx5 to use PMD allocator for data plane resources */
	struct mlx5dv_ctx_allocators alctr = {
		.alloc = &mlx5_alloc_verbs_buf,
		.free = &mlx5_free_verbs_buf,
		.data = priv,
	};
	mlx5_glue->dv_set_context_attr(sh->ctx,
				       MLX5DV_CTX_ATTR_BUF_ALLOCATORS,
				       (void *)((uintptr_t)&alctr));
	/* Bring Ethernet device up. */
	DRV_LOG(DEBUG, "port %u forcing Ethernet interface up",
		eth_dev->data->port_id);
	mlx5_set_link_up(eth_dev);
	/*
	 * Even though the interrupt handler is not installed yet,
	 * interrupts will still trigger on the async_fd from
	 * Verbs context returned by ibv_open_device().
	 */
	mlx5_link_update(eth_dev, 0);
#ifdef HAVE_MLX5DV_DR_ESWITCH
	if (!(config.hca_attr.eswitch_manager && config.dv_flow_en &&
	      (switch_info->representor || switch_info->master)))
		config.dv_esw_en = 0;
#else
	config.dv_esw_en = 0;
#endif
	/* Detect minimal data bytes to inline. */
	mlx5_set_min_inline(spawn, &config);
	/* Store device configuration on private structure. */
	priv->config = config;
	/* Create context for virtual machine VLAN workaround. */
	priv->vmwa_context = mlx5_vlan_vmwa_init(eth_dev, spawn->ifindex);
	if (config.dv_flow_en) {
		err = mlx5_alloc_shared_dr(priv);
		if (err)
			goto error;
		/*
		 * RSS id is shared with meter flow id. Meter flow id can only
		 * use the 24 MSB of the register.
		 */
		priv->qrss_id_pool = mlx5_flow_id_pool_alloc(UINT32_MAX >>
				     MLX5_MTR_COLOR_BITS);
		if (!priv->qrss_id_pool) {
			DRV_LOG(ERR, "can't create flow id pool");
			err = ENOMEM;
			goto error;
		}
	}
	/* Supported Verbs flow priority number detection. */
	err = mlx5_flow_discover_priorities(eth_dev);
	if (err < 0) {
		err = -err;
		goto error;
	}
	priv->config.flow_prio = err;
	if (!priv->config.dv_esw_en &&
	    priv->config.dv_xmeta_en != MLX5_XMETA_MODE_LEGACY) {
		DRV_LOG(WARNING, "metadata mode %u is not supported "
				 "(no E-Switch)", priv->config.dv_xmeta_en);
		priv->config.dv_xmeta_en = MLX5_XMETA_MODE_LEGACY;
	}
	mlx5_set_metadata_mask(eth_dev);
	if (priv->config.dv_xmeta_en != MLX5_XMETA_MODE_LEGACY &&
	    !priv->sh->dv_regc0_mask) {
		DRV_LOG(ERR, "metadata mode %u is not supported "
			     "(no metadata reg_c[0] is available)",
			     priv->config.dv_xmeta_en);
			err = ENOTSUP;
			goto error;
	}
	/*
	 * Allocate the buffer for flow creating, just once.
	 * The allocation must be done before any flow creating.
	 */
	mlx5_flow_alloc_intermediate(eth_dev);
	/* Query availibility of metadata reg_c's. */
	err = mlx5_flow_discover_mreg_c(eth_dev);
	if (err < 0) {
		err = -err;
		goto error;
	}
	if (!mlx5_flow_ext_mreg_supported(eth_dev)) {
		DRV_LOG(DEBUG,
			"port %u extensive metadata register is not supported",
			eth_dev->data->port_id);
		if (priv->config.dv_xmeta_en != MLX5_XMETA_MODE_LEGACY) {
			DRV_LOG(ERR, "metadata mode %u is not supported "
				     "(no metadata registers available)",
				     priv->config.dv_xmeta_en);
			err = ENOTSUP;
			goto error;
		}
	}
	if (priv->config.dv_flow_en &&
	    priv->config.dv_xmeta_en != MLX5_XMETA_MODE_LEGACY &&
	    mlx5_flow_ext_mreg_supported(eth_dev) &&
	    priv->sh->dv_regc0_mask) {
		priv->mreg_cp_tbl = mlx5_hlist_create(MLX5_FLOW_MREG_HNAME,
						      MLX5_FLOW_MREG_HTABLE_SZ);
		if (!priv->mreg_cp_tbl) {
			err = ENOMEM;
			goto error;
		}
	}
	return eth_dev;
error:
	if (priv) {
		if (priv->mreg_cp_tbl)
			mlx5_hlist_destroy(priv->mreg_cp_tbl, NULL, NULL);
		if (priv->sh)
			mlx5_free_shared_dr(priv);
		if (priv->nl_socket_route >= 0)
			close(priv->nl_socket_route);
		if (priv->nl_socket_rdma >= 0)
			close(priv->nl_socket_rdma);
		if (priv->vmwa_context)
			mlx5_vlan_vmwa_exit(priv->vmwa_context);
		if (priv->qrss_id_pool)
			mlx5_flow_id_pool_release(priv->qrss_id_pool);
		if (own_domain_id)
			claim_zero(rte_eth_switch_domain_free(priv->domain_id));
		rte_free(priv);
		if (eth_dev != NULL)
			eth_dev->data->dev_private = NULL;
	}
	if (eth_dev != NULL) {
		/* mac_addrs must not be freed alone because part of dev_private */
		eth_dev->data->mac_addrs = NULL;
		rte_eth_dev_release_port(eth_dev);
	}
	if (sh)
		mlx5_free_shared_ibctx(sh);
	MLX5_ASSERT(err > 0);
	rte_errno = err;
	return NULL;
}

/**
 * Comparison callback to sort device data.
 *
 * This is meant to be used with qsort().
 *
 * @param a[in]
 *   Pointer to pointer to first data object.
 * @param b[in]
 *   Pointer to pointer to second data object.
 *
 * @return
 *   0 if both objects are equal, less than 0 if the first argument is less
 *   than the second, greater than 0 otherwise.
 */
static int
mlx5_dev_spawn_data_cmp(const void *a, const void *b)
{
	const struct mlx5_switch_info *si_a =
		&((const struct mlx5_dev_spawn_data *)a)->info;
	const struct mlx5_switch_info *si_b =
		&((const struct mlx5_dev_spawn_data *)b)->info;
	int ret;

	/* Master device first. */
	ret = si_b->master - si_a->master;
	if (ret)
		return ret;
	/* Then representor devices. */
	ret = si_b->representor - si_a->representor;
	if (ret)
		return ret;
	/* Unidentified devices come last in no specific order. */
	if (!si_a->representor)
		return 0;
	/* Order representors by name. */
	return si_a->port_name - si_b->port_name;
}

/**
 * Match PCI information for possible slaves of bonding device.
 *
 * @param[in] ibv_dev
 *   Pointer to Infiniband device structure.
 * @param[in] pci_dev
 *   Pointer to PCI device structure to match PCI address.
 * @param[in] nl_rdma
 *   Netlink RDMA group socket handle.
 *
 * @return
 *   negative value if no bonding device found, otherwise
 *   positive index of slave PF in bonding.
 */
static int
mlx5_device_bond_pci_match(const struct ibv_device *ibv_dev,
			   const struct rte_pci_device *pci_dev,
			   int nl_rdma)
{
	char ifname[IF_NAMESIZE + 1];
	unsigned int ifindex;
	unsigned int np, i;
	FILE *file = NULL;
	int pf = -1;

	/*
	 * Try to get master device name. If something goes
	 * wrong suppose the lack of kernel support and no
	 * bonding devices.
	 */
	if (nl_rdma < 0)
		return -1;
	if (!strstr(ibv_dev->name, "bond"))
		return -1;
	np = mlx5_nl_portnum(nl_rdma, ibv_dev->name);
	if (!np)
		return -1;
	/*
	 * The Master device might not be on the predefined
	 * port (not on port index 1, it is not garanted),
	 * we have to scan all Infiniband device port and
	 * find master.
	 */
	for (i = 1; i <= np; ++i) {
		/* Check whether Infiniband port is populated. */
		ifindex = mlx5_nl_ifindex(nl_rdma, ibv_dev->name, i);
		if (!ifindex)
			continue;
		if (!if_indextoname(ifindex, ifname))
			continue;
		/* Try to read bonding slave names from sysfs. */
		MKSTR(slaves,
		      "/sys/class/net/%s/master/bonding/slaves", ifname);
		file = fopen(slaves, "r");
		if (file)
			break;
	}
	if (!file)
		return -1;
	/* Use safe format to check maximal buffer length. */
	MLX5_ASSERT(atol(RTE_STR(IF_NAMESIZE)) == IF_NAMESIZE);
	while (fscanf(file, "%" RTE_STR(IF_NAMESIZE) "s", ifname) == 1) {
		char tmp_str[IF_NAMESIZE + 32];
		struct rte_pci_addr pci_addr;
		struct mlx5_switch_info	info;

		/* Process slave interface names in the loop. */
		snprintf(tmp_str, sizeof(tmp_str),
			 "/sys/class/net/%s", ifname);
		if (mlx5_dev_to_pci_addr(tmp_str, &pci_addr)) {
			DRV_LOG(WARNING, "can not get PCI address"
					 " for netdev \"%s\"", ifname);
			continue;
		}
		if (pci_dev->addr.domain != pci_addr.domain ||
		    pci_dev->addr.bus != pci_addr.bus ||
		    pci_dev->addr.devid != pci_addr.devid ||
		    pci_dev->addr.function != pci_addr.function)
			continue;
		/* Slave interface PCI address match found. */
		fclose(file);
		snprintf(tmp_str, sizeof(tmp_str),
			 "/sys/class/net/%s/phys_port_name", ifname);
		file = fopen(tmp_str, "rb");
		if (!file)
			break;
		info.name_type = MLX5_PHYS_PORT_NAME_TYPE_NOTSET;
		if (fscanf(file, "%32s", tmp_str) == 1)
			mlx5_translate_port_name(tmp_str, &info);
		if (info.name_type == MLX5_PHYS_PORT_NAME_TYPE_LEGACY ||
		    info.name_type == MLX5_PHYS_PORT_NAME_TYPE_UPLINK)
			pf = info.port_name;
		break;
	}
	if (file)
		fclose(file);
	return pf;
}

/**
 * DPDK callback to register a PCI device.
 *
 * This function spawns Ethernet devices out of a given PCI device.
 *
 * @param[in] pci_drv
 *   PCI driver structure (mlx5_driver).
 * @param[in] pci_dev
 *   PCI device information.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	       struct rte_pci_device *pci_dev)
{
	struct ibv_device **ibv_list;
	/*
	 * Number of found IB Devices matching with requested PCI BDF.
	 * nd != 1 means there are multiple IB devices over the same
	 * PCI device and we have representors and master.
	 */
	unsigned int nd = 0;
	/*
	 * Number of found IB device Ports. nd = 1 and np = 1..n means
	 * we have the single multiport IB device, and there may be
	 * representors attached to some of found ports.
	 */
	unsigned int np = 0;
	/*
	 * Number of DPDK ethernet devices to Spawn - either over
	 * multiple IB devices or multiple ports of single IB device.
	 * Actually this is the number of iterations to spawn.
	 */
	unsigned int ns = 0;
	/*
	 * Bonding device
	 *   < 0 - no bonding device (single one)
	 *  >= 0 - bonding device (value is slave PF index)
	 */
	int bd = -1;
	struct mlx5_dev_spawn_data *list = NULL;
	struct mlx5_dev_config dev_config;
	int ret;

	if (mlx5_class_get(pci_dev->device.devargs) != MLX5_CLASS_NET) {
		DRV_LOG(DEBUG, "Skip probing - should be probed by other mlx5"
			" driver.");
		return 1;
	}
	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		mlx5_pmd_socket_init();
	ret = mlx5_init_once();
	if (ret) {
		DRV_LOG(ERR, "unable to init PMD global data: %s",
			strerror(rte_errno));
		return -rte_errno;
	}
	MLX5_ASSERT(pci_drv == &mlx5_driver);
	errno = 0;
	ibv_list = mlx5_glue->get_device_list(&ret);
	if (!ibv_list) {
		rte_errno = errno ? errno : ENOSYS;
		DRV_LOG(ERR, "cannot list devices, is ib_uverbs loaded?");
		return -rte_errno;
	}
	/*
	 * First scan the list of all Infiniband devices to find
	 * matching ones, gathering into the list.
	 */
	struct ibv_device *ibv_match[ret + 1];
	int nl_route = mlx5_nl_init(NETLINK_ROUTE);
	int nl_rdma = mlx5_nl_init(NETLINK_RDMA);
	unsigned int i;

	while (ret-- > 0) {
		struct rte_pci_addr pci_addr;

		DRV_LOG(DEBUG, "checking device \"%s\"", ibv_list[ret]->name);
		bd = mlx5_device_bond_pci_match
				(ibv_list[ret], pci_dev, nl_rdma);
		if (bd >= 0) {
			/*
			 * Bonding device detected. Only one match is allowed,
			 * the bonding is supported over multi-port IB device,
			 * there should be no matches on representor PCI
			 * functions or non VF LAG bonding devices with
			 * specified address.
			 */
			if (nd) {
				DRV_LOG(ERR,
					"multiple PCI match on bonding device"
					"\"%s\" found", ibv_list[ret]->name);
				rte_errno = ENOENT;
				ret = -rte_errno;
				goto exit;
			}
			DRV_LOG(INFO, "PCI information matches for"
				      " slave %d bonding device \"%s\"",
				      bd, ibv_list[ret]->name);
			ibv_match[nd++] = ibv_list[ret];
			break;
		}
		if (mlx5_dev_to_pci_addr
			(ibv_list[ret]->ibdev_path, &pci_addr))
			continue;
		if (pci_dev->addr.domain != pci_addr.domain ||
		    pci_dev->addr.bus != pci_addr.bus ||
		    pci_dev->addr.devid != pci_addr.devid ||
		    pci_dev->addr.function != pci_addr.function)
			continue;
		DRV_LOG(INFO, "PCI information matches for device \"%s\"",
			ibv_list[ret]->name);
		ibv_match[nd++] = ibv_list[ret];
	}
	ibv_match[nd] = NULL;
	if (!nd) {
		/* No device matches, just complain and bail out. */
		DRV_LOG(WARNING,
			"no Verbs device matches PCI device " PCI_PRI_FMT ","
			" are kernel drivers loaded?",
			pci_dev->addr.domain, pci_dev->addr.bus,
			pci_dev->addr.devid, pci_dev->addr.function);
		rte_errno = ENOENT;
		ret = -rte_errno;
		goto exit;
	}
	if (nd == 1) {
		/*
		 * Found single matching device may have multiple ports.
		 * Each port may be representor, we have to check the port
		 * number and check the representors existence.
		 */
		if (nl_rdma >= 0)
			np = mlx5_nl_portnum(nl_rdma, ibv_match[0]->name);
		if (!np)
			DRV_LOG(WARNING, "can not get IB device \"%s\""
					 " ports number", ibv_match[0]->name);
		if (bd >= 0 && !np) {
			DRV_LOG(ERR, "can not get ports"
				     " for bonding device");
			rte_errno = ENOENT;
			ret = -rte_errno;
			goto exit;
		}
	}
#ifndef HAVE_MLX5DV_DR_DEVX_PORT
	if (bd >= 0) {
		/*
		 * This may happen if there is VF LAG kernel support and
		 * application is compiled with older rdma_core library.
		 */
		DRV_LOG(ERR,
			"No kernel/verbs support for VF LAG bonding found.");
		rte_errno = ENOTSUP;
		ret = -rte_errno;
		goto exit;
	}
#endif
	/*
	 * Now we can determine the maximal
	 * amount of devices to be spawned.
	 */
	list = rte_zmalloc("device spawn data",
			 sizeof(struct mlx5_dev_spawn_data) *
			 (np ? np : nd),
			 RTE_CACHE_LINE_SIZE);
	if (!list) {
		DRV_LOG(ERR, "spawn data array allocation failure");
		rte_errno = ENOMEM;
		ret = -rte_errno;
		goto exit;
	}
	if (bd >= 0 || np > 1) {
		/*
		 * Single IB device with multiple ports found,
		 * it may be E-Switch master device and representors.
		 * We have to perform identification trough the ports.
		 */
		MLX5_ASSERT(nl_rdma >= 0);
		MLX5_ASSERT(ns == 0);
		MLX5_ASSERT(nd == 1);
		MLX5_ASSERT(np);
		for (i = 1; i <= np; ++i) {
			list[ns].max_port = np;
			list[ns].ibv_port = i;
			list[ns].ibv_dev = ibv_match[0];
			list[ns].eth_dev = NULL;
			list[ns].pci_dev = pci_dev;
			list[ns].pf_bond = bd;
			list[ns].ifindex = mlx5_nl_ifindex
					(nl_rdma, list[ns].ibv_dev->name, i);
			if (!list[ns].ifindex) {
				/*
				 * No network interface index found for the
				 * specified port, it means there is no
				 * representor on this port. It's OK,
				 * there can be disabled ports, for example
				 * if sriov_numvfs < sriov_totalvfs.
				 */
				continue;
			}
			ret = -1;
			if (nl_route >= 0)
				ret = mlx5_nl_switch_info
					       (nl_route,
						list[ns].ifindex,
						&list[ns].info);
			if (ret || (!list[ns].info.representor &&
				    !list[ns].info.master)) {
				/*
				 * We failed to recognize representors with
				 * Netlink, let's try to perform the task
				 * with sysfs.
				 */
				ret =  mlx5_sysfs_switch_info
						(list[ns].ifindex,
						 &list[ns].info);
			}
			if (!ret && bd >= 0) {
				switch (list[ns].info.name_type) {
				case MLX5_PHYS_PORT_NAME_TYPE_UPLINK:
					if (list[ns].info.port_name == bd)
						ns++;
					break;
				case MLX5_PHYS_PORT_NAME_TYPE_PFVF:
					if (list[ns].info.pf_num == bd)
						ns++;
					break;
				default:
					break;
				}
				continue;
			}
			if (!ret && (list[ns].info.representor ^
				     list[ns].info.master))
				ns++;
		}
		if (!ns) {
			DRV_LOG(ERR,
				"unable to recognize master/representors"
				" on the IB device with multiple ports");
			rte_errno = ENOENT;
			ret = -rte_errno;
			goto exit;
		}
	} else {
		/*
		 * The existence of several matching entries (nd > 1) means
		 * port representors have been instantiated. No existing Verbs
		 * call nor sysfs entries can tell them apart, this can only
		 * be done through Netlink calls assuming kernel drivers are
		 * recent enough to support them.
		 *
		 * In the event of identification failure through Netlink,
		 * try again through sysfs, then:
		 *
		 * 1. A single IB device matches (nd == 1) with single
		 *    port (np=0/1) and is not a representor, assume
		 *    no switch support.
		 *
		 * 2. Otherwise no safe assumptions can be made;
		 *    complain louder and bail out.
		 */
		np = 1;
		for (i = 0; i != nd; ++i) {
			memset(&list[ns].info, 0, sizeof(list[ns].info));
			list[ns].max_port = 1;
			list[ns].ibv_port = 1;
			list[ns].ibv_dev = ibv_match[i];
			list[ns].eth_dev = NULL;
			list[ns].pci_dev = pci_dev;
			list[ns].pf_bond = -1;
			list[ns].ifindex = 0;
			if (nl_rdma >= 0)
				list[ns].ifindex = mlx5_nl_ifindex
					(nl_rdma, list[ns].ibv_dev->name, 1);
			if (!list[ns].ifindex) {
				char ifname[IF_NAMESIZE];

				/*
				 * Netlink failed, it may happen with old
				 * ib_core kernel driver (before 4.16).
				 * We can assume there is old driver because
				 * here we are processing single ports IB
				 * devices. Let's try sysfs to retrieve
				 * the ifindex. The method works for
				 * master device only.
				 */
				if (nd > 1) {
					/*
					 * Multiple devices found, assume
					 * representors, can not distinguish
					 * master/representor and retrieve
					 * ifindex via sysfs.
					 */
					continue;
				}
				ret = mlx5_get_master_ifname
					(ibv_match[i]->ibdev_path, &ifname);
				if (!ret)
					list[ns].ifindex =
						if_nametoindex(ifname);
				if (!list[ns].ifindex) {
					/*
					 * No network interface index found
					 * for the specified device, it means
					 * there it is neither representor
					 * nor master.
					 */
					continue;
				}
			}
			ret = -1;
			if (nl_route >= 0)
				ret = mlx5_nl_switch_info
					       (nl_route,
						list[ns].ifindex,
						&list[ns].info);
			if (ret || (!list[ns].info.representor &&
				    !list[ns].info.master)) {
				/*
				 * We failed to recognize representors with
				 * Netlink, let's try to perform the task
				 * with sysfs.
				 */
				ret =  mlx5_sysfs_switch_info
						(list[ns].ifindex,
						 &list[ns].info);
			}
			if (!ret && (list[ns].info.representor ^
				     list[ns].info.master)) {
				ns++;
			} else if ((nd == 1) &&
				   !list[ns].info.representor &&
				   !list[ns].info.master) {
				/*
				 * Single IB device with
				 * one physical port and
				 * attached network device.
				 * May be SRIOV is not enabled
				 * or there is no representors.
				 */
				DRV_LOG(INFO, "no E-Switch support detected");
				ns++;
				break;
			}
		}
		if (!ns) {
			DRV_LOG(ERR,
				"unable to recognize master/representors"
				" on the multiple IB devices");
			rte_errno = ENOENT;
			ret = -rte_errno;
			goto exit;
		}
	}
	MLX5_ASSERT(ns);
	/*
	 * Sort list to probe devices in natural order for users convenience
	 * (i.e. master first, then representors from lowest to highest ID).
	 */
	qsort(list, ns, sizeof(*list), mlx5_dev_spawn_data_cmp);
	/* Default configuration. */
	dev_config = (struct mlx5_dev_config){
		.hw_padding = 0,
		.mps = MLX5_ARG_UNSET,
		.dbnc = MLX5_ARG_UNSET,
		.rx_vec_en = 1,
		.txq_inline_max = MLX5_ARG_UNSET,
		.txq_inline_min = MLX5_ARG_UNSET,
		.txq_inline_mpw = MLX5_ARG_UNSET,
		.txqs_inline = MLX5_ARG_UNSET,
		.vf_nl_en = 1,
		.mr_ext_memseg_en = 1,
		.mprq = {
			.enabled = 0, /* Disabled by default. */
			.stride_num_n = MLX5_MPRQ_STRIDE_NUM_N,
			.max_memcpy_len = MLX5_MPRQ_MEMCPY_DEFAULT_LEN,
			.min_rxqs_num = MLX5_MPRQ_MIN_RXQS,
		},
		.dv_esw_en = 1,
		.dv_flow_en = 1,
	};
	/* Device specific configuration. */
	switch (pci_dev->id.device_id) {
	case PCI_DEVICE_ID_MELLANOX_CONNECTX4VF:
	case PCI_DEVICE_ID_MELLANOX_CONNECTX4LXVF:
	case PCI_DEVICE_ID_MELLANOX_CONNECTX5VF:
	case PCI_DEVICE_ID_MELLANOX_CONNECTX5EXVF:
	case PCI_DEVICE_ID_MELLANOX_CONNECTX5BFVF:
	case PCI_DEVICE_ID_MELLANOX_CONNECTX6VF:
	case PCI_DEVICE_ID_MELLANOX_CONNECTX6DXVF:
		dev_config.vf = 1;
		break;
	default:
		break;
	}
	for (i = 0; i != ns; ++i) {
		uint32_t restore;

		list[i].eth_dev = mlx5_dev_spawn(&pci_dev->device,
						 &list[i],
						 dev_config);
		if (!list[i].eth_dev) {
			if (rte_errno != EBUSY && rte_errno != EEXIST)
				break;
			/* Device is disabled or already spawned. Ignore it. */
			continue;
		}
		restore = list[i].eth_dev->data->dev_flags;
		rte_eth_copy_pci_info(list[i].eth_dev, pci_dev);
		/* Restore non-PCI flags cleared by the above call. */
		list[i].eth_dev->data->dev_flags |= restore;
		mlx5_dev_interrupt_handler_devx_install(list[i].eth_dev);
		rte_eth_dev_probing_finish(list[i].eth_dev);
	}
	if (i != ns) {
		DRV_LOG(ERR,
			"probe of PCI device " PCI_PRI_FMT " aborted after"
			" encountering an error: %s",
			pci_dev->addr.domain, pci_dev->addr.bus,
			pci_dev->addr.devid, pci_dev->addr.function,
			strerror(rte_errno));
		ret = -rte_errno;
		/* Roll back. */
		while (i--) {
			if (!list[i].eth_dev)
				continue;
			mlx5_dev_close(list[i].eth_dev);
			/* mac_addrs must not be freed because in dev_private */
			list[i].eth_dev->data->mac_addrs = NULL;
			claim_zero(rte_eth_dev_release_port(list[i].eth_dev));
		}
		/* Restore original error. */
		rte_errno = -ret;
	} else {
		ret = 0;
	}
exit:
	/*
	 * Do the routine cleanup:
	 * - close opened Netlink sockets
	 * - free allocated spawn data array
	 * - free the Infiniband device list
	 */
	if (nl_rdma >= 0)
		close(nl_rdma);
	if (nl_route >= 0)
		close(nl_route);
	if (list)
		rte_free(list);
	MLX5_ASSERT(ibv_list);
	mlx5_glue->free_device_list(ibv_list);
	return ret;
}

/**
 * Look for the ethernet device belonging to mlx5 driver.
 *
 * @param[in] port_id
 *   port_id to start looking for device.
 * @param[in] pci_dev
 *   Pointer to the hint PCI device. When device is being probed
 *   the its siblings (master and preceding representors might
 *   not have assigned driver yet (because the mlx5_pci_probe()
 *   is not completed yet, for this case match on hint PCI
 *   device may be used to detect sibling device.
 *
 * @return
 *   port_id of found device, RTE_MAX_ETHPORT if not found.
 */
uint16_t
mlx5_eth_find_next(uint16_t port_id, struct rte_pci_device *pci_dev)
{
	while (port_id < RTE_MAX_ETHPORTS) {
		struct rte_eth_dev *dev = &rte_eth_devices[port_id];

		if (dev->state != RTE_ETH_DEV_UNUSED &&
		    dev->device &&
		    (dev->device == &pci_dev->device ||
		     (dev->device->driver &&
		     dev->device->driver->name &&
		     !strcmp(dev->device->driver->name, MLX5_DRIVER_NAME))))
			break;
		port_id++;
	}
	if (port_id >= RTE_MAX_ETHPORTS)
		return RTE_MAX_ETHPORTS;
	return port_id;
}

/**
 * DPDK callback to remove a PCI device.
 *
 * This function removes all Ethernet devices belong to a given PCI device.
 *
 * @param[in] pci_dev
 *   Pointer to the PCI device.
 *
 * @return
 *   0 on success, the function cannot fail.
 */
static int
mlx5_pci_remove(struct rte_pci_device *pci_dev)
{
	uint16_t port_id;

	RTE_ETH_FOREACH_DEV_OF(port_id, &pci_dev->device)
		rte_eth_dev_close(port_id);
	return 0;
}

static const struct rte_pci_id mlx5_pci_id_map[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_CONNECTX4)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_CONNECTX4VF)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_CONNECTX4LX)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_CONNECTX4LXVF)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_CONNECTX5)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_CONNECTX5VF)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_CONNECTX5EX)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_CONNECTX5EXVF)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_CONNECTX5BF)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_CONNECTX5BFVF)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
				PCI_DEVICE_ID_MELLANOX_CONNECTX6)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
				PCI_DEVICE_ID_MELLANOX_CONNECTX6VF)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
				PCI_DEVICE_ID_MELLANOX_CONNECTX6DX)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
				PCI_DEVICE_ID_MELLANOX_CONNECTX6DXVF)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
				PCI_DEVICE_ID_MELLANOX_CONNECTX6DXBF)
	},
	{
		.vendor_id = 0
	}
};

static struct rte_pci_driver mlx5_driver = {
	.driver = {
		.name = MLX5_DRIVER_NAME
	},
	.id_table = mlx5_pci_id_map,
	.probe = mlx5_pci_probe,
	.remove = mlx5_pci_remove,
	.dma_map = mlx5_dma_map,
	.dma_unmap = mlx5_dma_unmap,
	.drv_flags = RTE_PCI_DRV_INTR_LSC | RTE_PCI_DRV_INTR_RMV |
		     RTE_PCI_DRV_PROBE_AGAIN,
};

/**
 * Driver initialization routine.
 */
RTE_INIT(rte_mlx5_pmd_init)
{
	/* Initialize driver log type. */
	mlx5_logtype = rte_log_register("pmd.net.mlx5");
	if (mlx5_logtype >= 0)
		rte_log_set_level(mlx5_logtype, RTE_LOG_NOTICE);

	/* Build the static tables for Verbs conversion. */
	mlx5_set_ptype_table();
	mlx5_set_cksum_table();
	mlx5_set_swp_types_table();
	if (mlx5_glue)
		rte_pci_register(&mlx5_driver);
}

RTE_PMD_EXPORT_NAME(net_mlx5, __COUNTER__);
RTE_PMD_REGISTER_PCI_TABLE(net_mlx5, mlx5_pci_id_map);
RTE_PMD_REGISTER_KMOD_DEP(net_mlx5, "* ib_uverbs & mlx5_core & mlx5_ib");
