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
#include <mlx5_common_os.h>
#include <mlx5_common_mp.h>
#include <mlx5_common_pci.h>
#include <mlx5_malloc.h>

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

/* Device parameter to configure log 2 of the stride size for MPRQ. */
#define MLX5_RX_MPRQ_LOG_STRIDE_SIZE "mprq_log_stride_size"

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
 * Device parameter to enable Tx scheduling on timestamps
 * and specify the packet pacing granularity in nanoseconds.
 */
#define MLX5_TX_PP "tx_pp"

/*
 * Device parameter to specify skew in nanoseconds on Tx datapath,
 * it represents the time between SQ start WQE processing and
 * appearing actual packet data on the wire.
 */
#define MLX5_TX_SKEW "tx_skew"

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

/* Device parameter to let the user manage the lacp traffic of bonded device */
#define MLX5_LACP_BY_USER "lacp_by_user"

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

/*
 * Device parameter to configure the total data buffer size for a single
 * hairpin queue (logarithm value).
 */
#define MLX5_HP_BUF_SIZE "hp_buf_log_sz"

/* Flow memory reclaim mode. */
#define MLX5_RECLAIM_MEM "reclaim_mem_mode"

/* The default memory allocator used in PMD. */
#define MLX5_SYS_MEM_EN "sys_mem_en"
/* Decap will be used or not. */
#define MLX5_DECAP_EN "decap_en"

/* Shared memory between primary and secondary processes. */
struct mlx5_shared_data *mlx5_shared_data;

/** Driver-specific log messages type. */
int mlx5_logtype;

static LIST_HEAD(, mlx5_dev_ctx_shared) mlx5_dev_ctx_list =
						LIST_HEAD_INITIALIZER();
static pthread_mutex_t mlx5_dev_ctx_list_mutex = PTHREAD_MUTEX_INITIALIZER;

static const struct mlx5_indexed_pool_config mlx5_ipool_cfg[] = {
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	{
		.size = sizeof(struct mlx5_flow_dv_encap_decap_resource),
		.trunk_size = 64,
		.grow_trunk = 3,
		.grow_shift = 2,
		.need_lock = 0,
		.release_mem_en = 1,
		.malloc = mlx5_malloc,
		.free = mlx5_free,
		.type = "mlx5_encap_decap_ipool",
	},
	{
		.size = sizeof(struct mlx5_flow_dv_push_vlan_action_resource),
		.trunk_size = 64,
		.grow_trunk = 3,
		.grow_shift = 2,
		.need_lock = 0,
		.release_mem_en = 1,
		.malloc = mlx5_malloc,
		.free = mlx5_free,
		.type = "mlx5_push_vlan_ipool",
	},
	{
		.size = sizeof(struct mlx5_flow_dv_tag_resource),
		.trunk_size = 64,
		.grow_trunk = 3,
		.grow_shift = 2,
		.need_lock = 0,
		.release_mem_en = 1,
		.malloc = mlx5_malloc,
		.free = mlx5_free,
		.type = "mlx5_tag_ipool",
	},
	{
		.size = sizeof(struct mlx5_flow_dv_port_id_action_resource),
		.trunk_size = 64,
		.grow_trunk = 3,
		.grow_shift = 2,
		.need_lock = 0,
		.release_mem_en = 1,
		.malloc = mlx5_malloc,
		.free = mlx5_free,
		.type = "mlx5_port_id_ipool",
	},
	{
		.size = sizeof(struct mlx5_flow_tbl_data_entry),
		.trunk_size = 64,
		.grow_trunk = 3,
		.grow_shift = 2,
		.need_lock = 0,
		.release_mem_en = 1,
		.malloc = mlx5_malloc,
		.free = mlx5_free,
		.type = "mlx5_jump_ipool",
	},
#endif
	{
		.size = sizeof(struct mlx5_flow_meter),
		.trunk_size = 64,
		.grow_trunk = 3,
		.grow_shift = 2,
		.need_lock = 0,
		.release_mem_en = 1,
		.malloc = mlx5_malloc,
		.free = mlx5_free,
		.type = "mlx5_meter_ipool",
	},
	{
		.size = sizeof(struct mlx5_flow_mreg_copy_resource),
		.trunk_size = 64,
		.grow_trunk = 3,
		.grow_shift = 2,
		.need_lock = 0,
		.release_mem_en = 1,
		.malloc = mlx5_malloc,
		.free = mlx5_free,
		.type = "mlx5_mcp_ipool",
	},
	{
		.size = (sizeof(struct mlx5_hrxq) + MLX5_RSS_HASH_KEY_LEN),
		.trunk_size = 64,
		.grow_trunk = 3,
		.grow_shift = 2,
		.need_lock = 0,
		.release_mem_en = 1,
		.malloc = mlx5_malloc,
		.free = mlx5_free,
		.type = "mlx5_hrxq_ipool",
	},
	{
		/*
		 * MLX5_IPOOL_MLX5_FLOW size varies for DV and VERBS flows.
		 * It set in run time according to PCI function configuration.
		 */
		.size = 0,
		.trunk_size = 64,
		.grow_trunk = 3,
		.grow_shift = 2,
		.need_lock = 0,
		.release_mem_en = 1,
		.malloc = mlx5_malloc,
		.free = mlx5_free,
		.type = "mlx5_flow_handle_ipool",
	},
	{
		.size = sizeof(struct rte_flow),
		.trunk_size = 4096,
		.need_lock = 1,
		.release_mem_en = 1,
		.malloc = mlx5_malloc,
		.free = mlx5_free,
		.type = "rte_flow_ipool",
	},
};


#define MLX5_FLOW_MIN_ID_POOL_SIZE 512
#define MLX5_ID_GENERATION_ARRAY_FACTOR 16

#define MLX5_FLOW_TABLE_HLIST_ARRAY_SIZE 4096

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

	pool = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*pool),
			   RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
	if (!pool) {
		DRV_LOG(ERR, "can't allocate id pool");
		rte_errno  = ENOMEM;
		return NULL;
	}
	mem = mlx5_malloc(MLX5_MEM_ZERO,
			  MLX5_FLOW_MIN_ID_POOL_SIZE * sizeof(uint32_t),
			  RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
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
	mlx5_free(pool);
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
	mlx5_free(pool->free_arr);
	mlx5_free(pool);
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
		mem = mlx5_malloc(0, size2 * sizeof(uint32_t), 0,
				  SOCKET_ID_ANY);
		if (!mem) {
			DRV_LOG(ERR, "can't allocate mem for id pool");
			rte_errno  = ENOMEM;
			return -rte_errno;
		}
		memcpy(mem, pool->free_arr, size * sizeof(uint32_t));
		mlx5_free(pool->free_arr);
		pool->free_arr = mem;
		pool->curr = pool->free_arr + size;
		pool->last = pool->free_arr + size2;
	}
	*pool->curr = id;
	pool->curr++;
	return 0;
}

/**
 * Initialize the shared aging list information per port.
 *
 * @param[in] sh
 *   Pointer to mlx5_dev_ctx_shared object.
 */
static void
mlx5_flow_aging_init(struct mlx5_dev_ctx_shared *sh)
{
	uint32_t i;
	struct mlx5_age_info *age_info;

	for (i = 0; i < sh->max_port; i++) {
		age_info = &sh->port[i].age_info;
		age_info->flags = 0;
		TAILQ_INIT(&age_info->aged_counters);
		rte_spinlock_init(&age_info->aged_sl);
		MLX5_AGE_SET(age_info, MLX5_AGE_TRIGGER);
	}
}

/**
 * Initialize the counters management structure.
 *
 * @param[in] sh
 *   Pointer to mlx5_dev_ctx_shared object to free
 */
static void
mlx5_flow_counters_mng_init(struct mlx5_dev_ctx_shared *sh)
{
	int i;

	memset(&sh->cmng, 0, sizeof(sh->cmng));
	TAILQ_INIT(&sh->cmng.flow_counters);
	for (i = 0; i < MLX5_CCONT_TYPE_MAX; ++i) {
		sh->cmng.ccont[i].min_id = MLX5_CNT_BATCH_OFFSET;
		sh->cmng.ccont[i].max_id = -1;
		sh->cmng.ccont[i].last_pool_idx = POOL_IDX_INVALID;
		TAILQ_INIT(&sh->cmng.ccont[i].pool_list);
		rte_spinlock_init(&sh->cmng.ccont[i].resize_sl);
		TAILQ_INIT(&sh->cmng.ccont[i].counters);
		rte_spinlock_init(&sh->cmng.ccont[i].csl);
	}
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
	mlx5_free(mem);
}

/**
 * Close and release all the resources of the counters management.
 *
 * @param[in] sh
 *   Pointer to mlx5_dev_ctx_shared object to free.
 */
static void
mlx5_flow_counters_mng_close(struct mlx5_dev_ctx_shared *sh)
{
	struct mlx5_counter_stats_mem_mng *mng;
	int i;
	int j;
	int retries = 1024;

	rte_errno = 0;
	while (--retries) {
		rte_eal_alarm_cancel(mlx5_flow_query_alarm, sh);
		if (rte_errno != EINPROGRESS)
			break;
		rte_pause();
	}
	for (i = 0; i < MLX5_CCONT_TYPE_MAX; ++i) {
		struct mlx5_flow_counter_pool *pool;
		uint32_t batch = !!(i > 1);

		if (!sh->cmng.ccont[i].pools)
			continue;
		pool = TAILQ_FIRST(&sh->cmng.ccont[i].pool_list);
		while (pool) {
			if (batch && pool->min_dcs)
				claim_zero(mlx5_devx_cmd_destroy
							       (pool->min_dcs));
			for (j = 0; j < MLX5_COUNTERS_PER_POOL; ++j) {
				if (MLX5_POOL_GET_CNT(pool, j)->action)
					claim_zero
					 (mlx5_glue->destroy_flow_action
					  (MLX5_POOL_GET_CNT
					  (pool, j)->action));
				if (!batch && MLX5_GET_POOL_CNT_EXT
				    (pool, j)->dcs)
					claim_zero(mlx5_devx_cmd_destroy
						   (MLX5_GET_POOL_CNT_EXT
						    (pool, j)->dcs));
			}
			TAILQ_REMOVE(&sh->cmng.ccont[i].pool_list, pool, next);
			mlx5_free(pool);
			pool = TAILQ_FIRST(&sh->cmng.ccont[i].pool_list);
		}
		mlx5_free(sh->cmng.ccont[i].pools);
	}
	mng = LIST_FIRST(&sh->cmng.mem_mngs);
	while (mng) {
		mlx5_flow_destroy_counter_stat_mem_mng(mng);
		mng = LIST_FIRST(&sh->cmng.mem_mngs);
	}
	memset(&sh->cmng, 0, sizeof(sh->cmng));
}

/**
 * Initialize the flow resources' indexed mempool.
 *
 * @param[in] sh
 *   Pointer to mlx5_dev_ctx_shared object.
 * @param[in] sh
 *   Pointer to user dev config.
 */
static void
mlx5_flow_ipool_create(struct mlx5_dev_ctx_shared *sh,
		       const struct mlx5_dev_config *config)
{
	uint8_t i;
	struct mlx5_indexed_pool_config cfg;

	for (i = 0; i < MLX5_IPOOL_MAX; ++i) {
		cfg = mlx5_ipool_cfg[i];
		switch (i) {
		default:
			break;
		/*
		 * Set MLX5_IPOOL_MLX5_FLOW ipool size
		 * according to PCI function flow configuration.
		 */
		case MLX5_IPOOL_MLX5_FLOW:
			cfg.size = config->dv_flow_en ?
				sizeof(struct mlx5_flow_handle) :
				MLX5_FLOW_HANDLE_VERBS_SIZE;
			break;
		}
		if (config->reclaim_mode)
			cfg.release_mem_en = 1;
		sh->ipool[i] = mlx5_ipool_create(&cfg);
	}
}

/**
 * Release the flow resources' indexed mempool.
 *
 * @param[in] sh
 *   Pointer to mlx5_dev_ctx_shared object.
 */
static void
mlx5_flow_ipool_destroy(struct mlx5_dev_ctx_shared *sh)
{
	uint8_t i;

	for (i = 0; i < MLX5_IPOOL_MAX; ++i)
		mlx5_ipool_destroy(sh->ipool[i]);
}

/*
 * Check if dynamic flex parser for eCPRI already exists.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   true on exists, false on not.
 */
bool
mlx5_flex_parser_ecpri_exist(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flex_parser_profiles *prf =
				&priv->sh->fp[MLX5_FLEX_PARSER_ECPRI_0];

	return !!prf->obj;
}

/*
 * Allocation of a flex parser for eCPRI. Once created, this parser related
 * resources will be held until the device is closed.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flex_parser_ecpri_alloc(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flex_parser_profiles *prf =
				&priv->sh->fp[MLX5_FLEX_PARSER_ECPRI_0];
	struct mlx5_devx_graph_node_attr node = {
		.modify_field_select = 0,
	};
	uint32_t ids[8];
	int ret;

	if (!priv->config.hca_attr.parse_graph_flex_node) {
		DRV_LOG(ERR, "Dynamic flex parser is not supported "
			"for device %s.", priv->dev_data->name);
		return -ENOTSUP;
	}
	node.header_length_mode = MLX5_GRAPH_NODE_LEN_FIXED;
	/* 8 bytes now: 4B common header + 4B message body header. */
	node.header_length_base_value = 0x8;
	/* After MAC layer: Ether / VLAN. */
	node.in[0].arc_parse_graph_node = MLX5_GRAPH_ARC_NODE_MAC;
	/* Type of compared condition should be 0xAEFE in the L2 layer. */
	node.in[0].compare_condition_value = RTE_ETHER_TYPE_ECPRI;
	/* Sample #0: type in common header. */
	node.sample[0].flow_match_sample_en = 1;
	/* Fixed offset. */
	node.sample[0].flow_match_sample_offset_mode = 0x0;
	/* Only the 2nd byte will be used. */
	node.sample[0].flow_match_sample_field_base_offset = 0x0;
	/* Sample #1: message payload. */
	node.sample[1].flow_match_sample_en = 1;
	/* Fixed offset. */
	node.sample[1].flow_match_sample_offset_mode = 0x0;
	/*
	 * Only the first two bytes will be used right now, and its offset will
	 * start after the common header that with the length of a DW(u32).
	 */
	node.sample[1].flow_match_sample_field_base_offset = sizeof(uint32_t);
	prf->obj = mlx5_devx_cmd_create_flex_parser(priv->sh->ctx, &node);
	if (!prf->obj) {
		DRV_LOG(ERR, "Failed to create flex parser node object.");
		return (rte_errno == 0) ? -ENODEV : -rte_errno;
	}
	prf->num = 2;
	ret = mlx5_devx_cmd_query_parse_samples(prf->obj, ids, prf->num);
	if (ret) {
		DRV_LOG(ERR, "Failed to query sample IDs.");
		return (rte_errno == 0) ? -ENODEV : -rte_errno;
	}
	prf->offset[0] = 0x0;
	prf->offset[1] = sizeof(uint32_t);
	prf->ids[0] = ids[0];
	prf->ids[1] = ids[1];
	return 0;
}

/*
 * Destroy the flex parser node, including the parser itself, input / output
 * arcs and DW samples. Resources could be reused then.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
static void
mlx5_flex_parser_ecpri_release(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flex_parser_profiles *prf =
				&priv->sh->fp[MLX5_FLEX_PARSER_ECPRI_0];

	if (prf->obj)
		mlx5_devx_cmd_destroy(prf->obj);
	prf->obj = NULL;
}

/*
 * Allocate Rx and Tx UARs in robust fashion.
 * This routine handles the following UAR allocation issues:
 *
 *  - tries to allocate the UAR with the most appropriate memory
 *    mapping type from the ones supported by the host
 *
 *  - tries to allocate the UAR with non-NULL base address
 *    OFED 5.0.x and Upstream rdma_core before v29 returned the NULL as
 *    UAR base address if UAR was not the first object in the UAR page.
 *    It caused the PMD failure and we should try to get another UAR
 *    till we get the first one with non-NULL base address returned.
 */
static int
mlx5_alloc_rxtx_uars(struct mlx5_dev_ctx_shared *sh,
		     const struct mlx5_dev_config *config)
{
	uint32_t uar_mapping, retry;
	int err = 0;

	for (retry = 0; retry < MLX5_ALLOC_UAR_RETRY; ++retry) {
#ifdef MLX5DV_UAR_ALLOC_TYPE_NC
		/* Control the mapping type according to the settings. */
		uar_mapping = (config->dbnc == MLX5_TXDB_NCACHED) ?
			      MLX5DV_UAR_ALLOC_TYPE_NC :
			      MLX5DV_UAR_ALLOC_TYPE_BF;
#else
		RTE_SET_USED(config);
		/*
		 * It seems we have no way to control the memory mapping type
		 * for the UAR, the default "Write-Combining" type is supposed.
		 * The UAR initialization on queue creation queries the
		 * actual mapping type done by Verbs/kernel and setups the
		 * PMD datapath accordingly.
		 */
		uar_mapping = 0;
#endif
		sh->tx_uar = mlx5_glue->devx_alloc_uar(sh->ctx, uar_mapping);
#ifdef MLX5DV_UAR_ALLOC_TYPE_NC
		if (!sh->tx_uar &&
		    uar_mapping == MLX5DV_UAR_ALLOC_TYPE_BF) {
			if (config->dbnc == MLX5_TXDB_CACHED ||
			    config->dbnc == MLX5_TXDB_HEURISTIC)
				DRV_LOG(WARNING, "Devarg tx_db_nc setting "
						 "is not supported by DevX");
			/*
			 * In some environments like virtual machine
			 * the Write Combining mapped might be not supported
			 * and UAR allocation fails. We try "Non-Cached"
			 * mapping for the case. The tx_burst routines take
			 * the UAR mapping type into account on UAR setup
			 * on queue creation.
			 */
			DRV_LOG(WARNING, "Failed to allocate Tx DevX UAR (BF)");
			uar_mapping = MLX5DV_UAR_ALLOC_TYPE_NC;
			sh->tx_uar = mlx5_glue->devx_alloc_uar
							(sh->ctx, uar_mapping);
		} else if (!sh->tx_uar &&
			   uar_mapping == MLX5DV_UAR_ALLOC_TYPE_NC) {
			if (config->dbnc == MLX5_TXDB_NCACHED)
				DRV_LOG(WARNING, "Devarg tx_db_nc settings "
						 "is not supported by DevX");
			/*
			 * If Verbs/kernel does not support "Non-Cached"
			 * try the "Write-Combining".
			 */
			DRV_LOG(WARNING, "Failed to allocate Tx DevX UAR (NC)");
			uar_mapping = MLX5DV_UAR_ALLOC_TYPE_BF;
			sh->tx_uar = mlx5_glue->devx_alloc_uar
							(sh->ctx, uar_mapping);
		}
#endif
		if (!sh->tx_uar) {
			DRV_LOG(ERR, "Failed to allocate Tx DevX UAR (BF/NC)");
			err = ENOMEM;
			goto exit;
		}
		if (sh->tx_uar->base_addr)
			break;
		/*
		 * The UARs are allocated by rdma_core within the
		 * IB device context, on context closure all UARs
		 * will be freed, should be no memory/object leakage.
		 */
		DRV_LOG(WARNING, "Retrying to allocate Tx DevX UAR");
		sh->tx_uar = NULL;
	}
	/* Check whether we finally succeeded with valid UAR allocation. */
	if (!sh->tx_uar) {
		DRV_LOG(ERR, "Failed to allocate Tx DevX UAR (NULL base)");
		err = ENOMEM;
		goto exit;
	}
	for (retry = 0; retry < MLX5_ALLOC_UAR_RETRY; ++retry) {
		uar_mapping = 0;
		sh->devx_rx_uar = mlx5_glue->devx_alloc_uar
							(sh->ctx, uar_mapping);
#ifdef MLX5DV_UAR_ALLOC_TYPE_NC
		if (!sh->devx_rx_uar &&
		    uar_mapping == MLX5DV_UAR_ALLOC_TYPE_BF) {
			/*
			 * Rx UAR is used to control interrupts only,
			 * should be no datapath noticeable impact,
			 * can try "Non-Cached" mapping safely.
			 */
			DRV_LOG(WARNING, "Failed to allocate Rx DevX UAR (BF)");
			uar_mapping = MLX5DV_UAR_ALLOC_TYPE_NC;
			sh->devx_rx_uar = mlx5_glue->devx_alloc_uar
							(sh->ctx, uar_mapping);
		}
#endif
		if (!sh->devx_rx_uar) {
			DRV_LOG(ERR, "Failed to allocate Rx DevX UAR (BF/NC)");
			err = ENOMEM;
			goto exit;
		}
		if (sh->devx_rx_uar->base_addr)
			break;
		/*
		 * The UARs are allocated by rdma_core within the
		 * IB device context, on context closure all UARs
		 * will be freed, should be no memory/object leakage.
		 */
		DRV_LOG(WARNING, "Retrying to allocate Rx DevX UAR");
		sh->devx_rx_uar = NULL;
	}
	/* Check whether we finally succeeded with valid UAR allocation. */
	if (!sh->devx_rx_uar) {
		DRV_LOG(ERR, "Failed to allocate Rx DevX UAR (NULL base)");
		err = ENOMEM;
	}
exit:
	return err;
}

/**
 * Allocate shared device context. If there is multiport device the
 * master and representors will share this context, if there is single
 * port dedicated device, the context will be used by only given
 * port due to unification.
 *
 * Routine first searches the context for the specified device name,
 * if found the shared context assumed and reference counter is incremented.
 * If no context found the new one is created and initialized with specified
 * device context and parameters.
 *
 * @param[in] spawn
 *   Pointer to the device attributes (name, port, etc).
 * @param[in] config
 *   Pointer to device configuration structure.
 *
 * @return
 *   Pointer to mlx5_dev_ctx_shared object on success,
 *   otherwise NULL and rte_errno is set.
 */
struct mlx5_dev_ctx_shared *
mlx5_alloc_shared_dev_ctx(const struct mlx5_dev_spawn_data *spawn,
			   const struct mlx5_dev_config *config)
{
	struct mlx5_dev_ctx_shared *sh;
	int err = 0;
	uint32_t i;
	struct mlx5_devx_tis_attr tis_attr = { 0 };

	MLX5_ASSERT(spawn);
	/* Secondary process should not create the shared context. */
	MLX5_ASSERT(rte_eal_process_type() == RTE_PROC_PRIMARY);
	pthread_mutex_lock(&mlx5_dev_ctx_list_mutex);
	/* Search for IB context by device name. */
	LIST_FOREACH(sh, &mlx5_dev_ctx_list, next) {
		if (!strcmp(sh->ibdev_name,
			mlx5_os_get_dev_device_name(spawn->phys_dev))) {
			sh->refcnt++;
			goto exit;
		}
	}
	/* No device found, we have to create new shared context. */
	MLX5_ASSERT(spawn->max_port);
	sh = mlx5_malloc(MLX5_MEM_ZERO | MLX5_MEM_RTE,
			 sizeof(struct mlx5_dev_ctx_shared) +
			 spawn->max_port *
			 sizeof(struct mlx5_dev_shared_port),
			 RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
	if (!sh) {
		DRV_LOG(ERR, "shared context allocation failure");
		rte_errno  = ENOMEM;
		goto exit;
	}
	err = mlx5_os_open_device(spawn, config, sh);
	if (!sh->ctx)
		goto error;
	err = mlx5_os_get_dev_attr(sh->ctx, &sh->device_attr);
	if (err) {
		DRV_LOG(DEBUG, "mlx5_os_get_dev_attr() failed");
		goto error;
	}
	sh->refcnt = 1;
	sh->max_port = spawn->max_port;
	strncpy(sh->ibdev_name, mlx5_os_get_ctx_device_name(sh->ctx),
		sizeof(sh->ibdev_name) - 1);
	strncpy(sh->ibdev_path, mlx5_os_get_ctx_device_path(sh->ctx),
		sizeof(sh->ibdev_path) - 1);
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
	if (sh->devx) {
		err = mlx5_os_get_pdn(sh->pd, &sh->pdn);
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
		err = mlx5_alloc_rxtx_uars(sh, config);
		if (err)
			goto error;
		MLX5_ASSERT(sh->tx_uar && sh->tx_uar->base_addr);
		MLX5_ASSERT(sh->devx_rx_uar && sh->devx_rx_uar->base_addr);
	}
	sh->flow_id_pool = mlx5_flow_id_pool_alloc
					((1 << HAIRPIN_FLOW_ID_BITS) - 1);
	if (!sh->flow_id_pool) {
		DRV_LOG(ERR, "can't create flow id pool");
		err = ENOMEM;
		goto error;
	}
#ifndef RTE_ARCH_64
	/* Initialize UAR access locks for 32bit implementations. */
	rte_spinlock_init(&sh->uar_lock_cq);
	for (i = 0; i < MLX5_UAR_PAGE_NUM_MAX; i++)
		rte_spinlock_init(&sh->uar_lock[i]);
#endif
	/*
	 * Once the device is added to the list of memory event
	 * callback, its global MR cache table cannot be expanded
	 * on the fly because of deadlock. If it overflows, lookup
	 * should be done by searching MR list linearly, which is slow.
	 *
	 * At this point the device is not added to the memory
	 * event list yet, context is just being created.
	 */
	err = mlx5_mr_btree_init(&sh->share_cache.cache,
				 MLX5_MR_BTREE_CACHE_N * 2,
				 spawn->pci_dev->device.numa_node);
	if (err) {
		err = rte_errno;
		goto error;
	}
	mlx5_os_set_reg_mr_cb(&sh->share_cache.reg_mr_cb,
			      &sh->share_cache.dereg_mr_cb);
	mlx5_os_dev_shared_handler_install(sh);
	sh->cnt_id_tbl = mlx5_l3t_create(MLX5_L3T_TYPE_DWORD);
	if (!sh->cnt_id_tbl) {
		err = rte_errno;
		goto error;
	}
	mlx5_flow_aging_init(sh);
	mlx5_flow_counters_mng_init(sh);
	mlx5_flow_ipool_create(sh, config);
	/* Add device to memory callback list. */
	rte_rwlock_write_lock(&mlx5_shared_data->mem_event_rwlock);
	LIST_INSERT_HEAD(&mlx5_shared_data->mem_event_cb_list,
			 sh, mem_event_cb);
	rte_rwlock_write_unlock(&mlx5_shared_data->mem_event_rwlock);
	/* Add context to the global device list. */
	LIST_INSERT_HEAD(&mlx5_dev_ctx_list, sh, next);
exit:
	pthread_mutex_unlock(&mlx5_dev_ctx_list_mutex);
	return sh;
error:
	pthread_mutex_destroy(&sh->txpp.mutex);
	pthread_mutex_unlock(&mlx5_dev_ctx_list_mutex);
	MLX5_ASSERT(sh);
	if (sh->cnt_id_tbl)
		mlx5_l3t_destroy(sh->cnt_id_tbl);
	if (sh->tis)
		claim_zero(mlx5_devx_cmd_destroy(sh->tis));
	if (sh->td)
		claim_zero(mlx5_devx_cmd_destroy(sh->td));
	if (sh->devx_rx_uar)
		mlx5_glue->devx_free_uar(sh->devx_rx_uar);
	if (sh->tx_uar)
		mlx5_glue->devx_free_uar(sh->tx_uar);
	if (sh->pd)
		claim_zero(mlx5_glue->dealloc_pd(sh->pd));
	if (sh->ctx)
		claim_zero(mlx5_glue->close_device(sh->ctx));
	if (sh->flow_id_pool)
		mlx5_flow_id_pool_release(sh->flow_id_pool);
	mlx5_free(sh);
	MLX5_ASSERT(err > 0);
	rte_errno = err;
	return NULL;
}

/**
 * Free shared IB device context. Decrement counter and if zero free
 * all allocated resources and close handles.
 *
 * @param[in] sh
 *   Pointer to mlx5_dev_ctx_shared object to free
 */
void
mlx5_free_shared_dev_ctx(struct mlx5_dev_ctx_shared *sh)
{
	pthread_mutex_lock(&mlx5_dev_ctx_list_mutex);
#ifdef RTE_LIBRTE_MLX5_DEBUG
	/* Check the object presence in the list. */
	struct mlx5_dev_ctx_shared *lctx;

	LIST_FOREACH(lctx, &mlx5_dev_ctx_list, next)
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
	mlx5_mr_release_cache(&sh->share_cache);
	/* Remove context from the global device list. */
	LIST_REMOVE(sh, next);
	pthread_mutex_unlock(&mlx5_dev_ctx_list_mutex);
	/*
	 *  Ensure there is no async event handler installed.
	 *  Only primary process handles async device events.
	 **/
	mlx5_flow_counters_mng_close(sh);
	mlx5_flow_ipool_destroy(sh);
	mlx5_os_dev_shared_handler_uninstall(sh);
	if (sh->cnt_id_tbl) {
		mlx5_l3t_destroy(sh->cnt_id_tbl);
		sh->cnt_id_tbl = NULL;
	}
	if (sh->tx_uar) {
		mlx5_glue->devx_free_uar(sh->tx_uar);
		sh->tx_uar = NULL;
	}
	if (sh->pd)
		claim_zero(mlx5_glue->dealloc_pd(sh->pd));
	if (sh->tis)
		claim_zero(mlx5_devx_cmd_destroy(sh->tis));
	if (sh->td)
		claim_zero(mlx5_devx_cmd_destroy(sh->td));
	if (sh->devx_rx_uar)
		mlx5_glue->devx_free_uar(sh->devx_rx_uar);
	if (sh->ctx)
		claim_zero(mlx5_glue->close_device(sh->ctx));
	if (sh->flow_id_pool)
		mlx5_flow_id_pool_release(sh->flow_id_pool);
	pthread_mutex_destroy(&sh->txpp.mutex);
	mlx5_free(sh);
	return;
exit:
	pthread_mutex_unlock(&mlx5_dev_ctx_list_mutex);
}

/**
 * Destroy table hash list and all the root entries per domain.
 *
 * @param[in] priv
 *   Pointer to the private device data structure.
 */
void
mlx5_free_table_hash_list(struct mlx5_priv *priv)
{
	struct mlx5_dev_ctx_shared *sh = priv->sh;
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
		mlx5_free(tbl_data);
	}
	table_key.direction = 1;
	pos = mlx5_hlist_lookup(sh->flow_tbls, table_key.v64);
	if (pos) {
		tbl_data = container_of(pos, struct mlx5_flow_tbl_data_entry,
					entry);
		MLX5_ASSERT(tbl_data);
		mlx5_hlist_remove(sh->flow_tbls, pos);
		mlx5_free(tbl_data);
	}
	table_key.direction = 0;
	table_key.domain = 1;
	pos = mlx5_hlist_lookup(sh->flow_tbls, table_key.v64);
	if (pos) {
		tbl_data = container_of(pos, struct mlx5_flow_tbl_data_entry,
					entry);
		MLX5_ASSERT(tbl_data);
		mlx5_hlist_remove(sh->flow_tbls, pos);
		mlx5_free(tbl_data);
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
int
mlx5_alloc_table_hash_list(struct mlx5_priv *priv)
{
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	char s[MLX5_HLIST_NAMESIZE];
	int err = 0;

	MLX5_ASSERT(sh);
	snprintf(s, sizeof(s), "%s_flow_table", priv->sh->ibdev_name);
	sh->flow_tbls = mlx5_hlist_create(s, MLX5_FLOW_TABLE_HLIST_ARRAY_SIZE);
	if (!sh->flow_tbls) {
		DRV_LOG(ERR, "flow tables with hash creation failed.");
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
	struct mlx5_flow_tbl_data_entry *tbl_data = mlx5_malloc(MLX5_MEM_ZERO,
							  sizeof(*tbl_data), 0,
							  SOCKET_ID_ANY);

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
	tbl_data = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*tbl_data), 0,
			       SOCKET_ID_ANY);
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
	tbl_data = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*tbl_data), 0,
			       SOCKET_ID_ANY);
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
	ppriv = mlx5_malloc(MLX5_MEM_RTE, ppriv_size, RTE_CACHE_LINE_SIZE,
			    dev->device->numa_node);
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
	mlx5_free(dev->process_private);
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
void
mlx5_dev_close(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	unsigned int i;
	int ret;

	if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		/* Check if process_private released. */
		if (!dev->process_private)
			return;
		mlx5_tx_uar_uninit_secondary(dev);
		mlx5_proc_priv_uninit(dev);
		rte_eth_dev_release_port(dev);
		return;
	}
	if (!priv->sh)
		return;
	DRV_LOG(DEBUG, "port %u closing device \"%s\"",
		dev->data->port_id,
		((priv->sh->ctx != NULL) ?
		mlx5_os_get_ctx_device_name(priv->sh->ctx) : ""));
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
	mlx5_mp_os_req_stop_rxtx(dev);
	/* Free the eCPRI flex parser resource. */
	mlx5_flex_parser_ecpri_release(dev);
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
	mlx5_os_free_shared_dr(priv);
	if (priv->rss_conf.rss_key != NULL)
		mlx5_free(priv->rss_conf.rss_key);
	if (priv->reta_idx != NULL)
		mlx5_free(priv->reta_idx);
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
	/*
	 * Free the shared context in last turn, because the cleanup
	 * routines above may use some shared fields, like
	 * mlx5_nl_mac_addr_flush() uses ibdev_path for retrieveing
	 * ifindex if Netlink fails.
	 */
	mlx5_free_shared_dev_ctx(priv->sh);
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
	unsigned long mod;
	signed long tmp;

	/* No-op, port representors are processed in mlx5_dev_spawn(). */
	if (!strcmp(MLX5_REPRESENTOR, key))
		return 0;
	errno = 0;
	tmp = strtol(val, NULL, 0);
	if (errno) {
		rte_errno = errno;
		DRV_LOG(WARNING, "%s: \"%s\" is not a valid integer", key, val);
		return -rte_errno;
	}
	if (tmp < 0 && strcmp(MLX5_TX_PP, key) && strcmp(MLX5_TX_SKEW, key)) {
		/* Negative values are acceptable for some keys only. */
		rte_errno = EINVAL;
		DRV_LOG(WARNING, "%s: invalid negative value \"%s\"", key, val);
		return -rte_errno;
	}
	mod = tmp >= 0 ? tmp : -tmp;
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
	} else if (strcmp(MLX5_RX_MPRQ_LOG_STRIDE_SIZE, key) == 0) {
		config->mprq.stride_size_n = tmp;
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
	} else if (strcmp(MLX5_TX_PP, key) == 0) {
		if (!mod) {
			DRV_LOG(ERR, "Zero Tx packet pacing parameter");
			rte_errno = EINVAL;
			return -rte_errno;
		}
		config->tx_pp = tmp;
	} else if (strcmp(MLX5_TX_SKEW, key) == 0) {
		config->tx_skew = tmp;
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
	} else if (strcmp(MLX5_LACP_BY_USER, key) == 0) {
		config->lacp_by_user = !!tmp;
	} else if (strcmp(MLX5_MR_EXT_MEMSEG_EN, key) == 0) {
		config->mr_ext_memseg_en = !!tmp;
	} else if (strcmp(MLX5_MAX_DUMP_FILES_NUM, key) == 0) {
		config->max_dump_files_num = tmp;
	} else if (strcmp(MLX5_LRO_TIMEOUT_USEC, key) == 0) {
		config->lro.timeout = tmp;
	} else if (strcmp(MLX5_CLASS_ARG_NAME, key) == 0) {
		DRV_LOG(DEBUG, "class argument is %s.", val);
	} else if (strcmp(MLX5_HP_BUF_SIZE, key) == 0) {
		config->log_hp_size = tmp;
	} else if (strcmp(MLX5_RECLAIM_MEM, key) == 0) {
		if (tmp != MLX5_RCM_NONE &&
		    tmp != MLX5_RCM_LIGHT &&
		    tmp != MLX5_RCM_AGGR) {
			DRV_LOG(ERR, "Unrecognize %s: \"%s\"", key, val);
			rte_errno = EINVAL;
			return -rte_errno;
		}
		config->reclaim_mode = tmp;
	} else if (strcmp(MLX5_SYS_MEM_EN, key) == 0) {
		config->sys_mem_en = !!tmp;
	} else if (strcmp(MLX5_DECAP_EN, key) == 0) {
		config->decap_en = !!tmp;
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
int
mlx5_args(struct mlx5_dev_config *config, struct rte_devargs *devargs)
{
	const char **params = (const char *[]){
		MLX5_RXQ_CQE_COMP_EN,
		MLX5_RXQ_CQE_PAD_EN,
		MLX5_RXQ_PKT_PAD_EN,
		MLX5_RX_MPRQ_EN,
		MLX5_RX_MPRQ_LOG_STRIDE_NUM,
		MLX5_RX_MPRQ_LOG_STRIDE_SIZE,
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
		MLX5_TX_PP,
		MLX5_TX_SKEW,
		MLX5_TX_VEC_EN,
		MLX5_RX_VEC_EN,
		MLX5_L3_VXLAN_EN,
		MLX5_VF_NL_EN,
		MLX5_DV_ESW_EN,
		MLX5_DV_FLOW_EN,
		MLX5_DV_XMETA_EN,
		MLX5_LACP_BY_USER,
		MLX5_MR_EXT_MEMSEG_EN,
		MLX5_REPRESENTOR,
		MLX5_MAX_DUMP_FILES_NUM,
		MLX5_LRO_TIMEOUT_USEC,
		MLX5_CLASS_ARG_NAME,
		MLX5_HP_BUF_SIZE,
		MLX5_RECLAIM_MEM,
		MLX5_SYS_MEM_EN,
		MLX5_DECAP_EN,
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
void
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
void
mlx5_set_metadata_mask(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_ctx_shared *sh = priv->sh;
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

int
rte_pmd_mlx5_get_dyn_flag_names(char *names[], unsigned int n)
{
	static const char *const dynf_names[] = {
		RTE_PMD_MLX5_FINE_GRANULARITY_INLINE,
		RTE_MBUF_DYNFLAG_METADATA_NAME,
		RTE_MBUF_DYNFLAG_TX_TIMESTAMP_NAME
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
int
mlx5_dev_check_sibling_config(struct mlx5_priv *priv,
			      struct mlx5_dev_config *config)
{
	struct mlx5_dev_ctx_shared *sh = priv->sh;
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
 * Look for the ethernet device belonging to mlx5 driver.
 *
 * @param[in] port_id
 *   port_id to start looking for device.
 * @param[in] pci_dev
 *   Pointer to the hint PCI device. When device is being probed
 *   the its siblings (master and preceding representors might
 *   not have assigned driver yet (because the mlx5_os_pci_probe()
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

	RTE_ETH_FOREACH_DEV_OF(port_id, &pci_dev->device) {
		/*
		 * mlx5_dev_close() is not registered to secondary process,
		 * call the close function explicitly for secondary process.
		 */
		if (rte_eal_process_type() == RTE_PROC_SECONDARY)
			mlx5_dev_close(&rte_eth_devices[port_id]);
		else
			rte_eth_dev_close(port_id);
	}
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
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
				PCI_DEVICE_ID_MELLANOX_CONNECTX6LX)
	},
	{
		.vendor_id = 0
	}
};

static struct mlx5_pci_driver mlx5_driver = {
	.driver_class = MLX5_CLASS_NET,
	.pci_driver = {
		.driver = {
			.name = MLX5_DRIVER_NAME,
		},
		.id_table = mlx5_pci_id_map,
		.probe = mlx5_os_pci_probe,
		.remove = mlx5_pci_remove,
		.dma_map = mlx5_dma_map,
		.dma_unmap = mlx5_dma_unmap,
		.drv_flags = PCI_DRV_FLAGS,
	},
};

/* Initialize driver log type. */
RTE_LOG_REGISTER(mlx5_logtype, pmd.net.mlx5, NOTICE)

/**
 * Driver initialization routine.
 */
RTE_INIT(rte_mlx5_pmd_init)
{
	mlx5_common_init();
	/* Build the static tables for Verbs conversion. */
	mlx5_set_ptype_table();
	mlx5_set_cksum_table();
	mlx5_set_swp_types_table();
	if (mlx5_glue)
		mlx5_pci_driver_register(&mlx5_driver);
}

RTE_PMD_EXPORT_NAME(net_mlx5, __COUNTER__);
RTE_PMD_REGISTER_PCI_TABLE(net_mlx5, mlx5_pci_id_map);
RTE_PMD_REGISTER_KMOD_DEP(net_mlx5, "* ib_uverbs & mlx5_core & mlx5_ib");
