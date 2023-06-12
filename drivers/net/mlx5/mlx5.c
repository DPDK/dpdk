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
#include <fcntl.h>

#include <rte_malloc.h>
#include <ethdev_driver.h>
#include <rte_pci.h>
#include <bus_pci_driver.h>
#include <rte_common.h>
#include <rte_kvargs.h>
#include <rte_rwlock.h>
#include <rte_spinlock.h>
#include <rte_string_fns.h>
#include <rte_eal_paging.h>
#include <rte_alarm.h>
#include <rte_cycles.h>
#include <rte_interrupts.h>

#include <mlx5_glue.h>
#include <mlx5_devx_cmds.h>
#include <mlx5_common.h>
#include <mlx5_common_os.h>
#include <mlx5_common_mp.h>
#include <mlx5_malloc.h>

#include "mlx5_defs.h"
#include "mlx5.h"
#include "mlx5_utils.h"
#include "mlx5_rxtx.h"
#include "mlx5_rx.h"
#include "mlx5_tx.h"
#include "mlx5_autoconf.h"
#include "mlx5_flow.h"
#include "mlx5_flow_os.h"
#include "rte_pmd_mlx5.h"

#define MLX5_ETH_DRIVER_NAME mlx5_eth

/* Device parameter to enable RX completion queue compression. */
#define MLX5_RXQ_CQE_COMP_EN "rxq_cqe_comp_en"

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

/* Decap will be used or not. */
#define MLX5_DECAP_EN "decap_en"

/* Device parameter to configure allow or prevent duplicate rules pattern. */
#define MLX5_ALLOW_DUPLICATE_PATTERN "allow_duplicate_pattern"

/* Device parameter to configure the delay drop when creating Rxqs. */
#define MLX5_DELAY_DROP "delay_drop"

/* Device parameter to create the fdb default rule in PMD */
#define MLX5_FDB_DEFAULT_RULE_EN "fdb_def_rule_en"

/* HW steering counter configuration. */
#define MLX5_HWS_CNT_SERVICE_CORE "service_core"

/* HW steering counter's query interval. */
#define MLX5_HWS_CNT_CYCLE_TIME "svc_cycle_time"

/* Device parameter to control representor matching in ingress/egress flows with HWS. */
#define MLX5_REPR_MATCHING_EN "repr_matching_en"

/* Shared memory between primary and secondary processes. */
struct mlx5_shared_data *mlx5_shared_data;

/** Driver-specific log messages type. */
int mlx5_logtype;

static LIST_HEAD(, mlx5_dev_ctx_shared) mlx5_dev_ctx_list =
						LIST_HEAD_INITIALIZER();
static pthread_mutex_t mlx5_dev_ctx_list_mutex;
static const struct mlx5_indexed_pool_config mlx5_ipool_cfg[] = {
#if defined(HAVE_IBV_FLOW_DV_SUPPORT) || !defined(HAVE_INFINIBAND_VERBS_H)
	[MLX5_IPOOL_DECAP_ENCAP] = {
		.size = sizeof(struct mlx5_flow_dv_encap_decap_resource),
		.trunk_size = 64,
		.grow_trunk = 3,
		.grow_shift = 2,
		.need_lock = 1,
		.release_mem_en = 1,
		.malloc = mlx5_malloc,
		.free = mlx5_free,
		.type = "mlx5_encap_decap_ipool",
	},
	[MLX5_IPOOL_PUSH_VLAN] = {
		.size = sizeof(struct mlx5_flow_dv_push_vlan_action_resource),
		.trunk_size = 64,
		.grow_trunk = 3,
		.grow_shift = 2,
		.need_lock = 1,
		.release_mem_en = 1,
		.malloc = mlx5_malloc,
		.free = mlx5_free,
		.type = "mlx5_push_vlan_ipool",
	},
	[MLX5_IPOOL_TAG] = {
		.size = sizeof(struct mlx5_flow_dv_tag_resource),
		.trunk_size = 64,
		.grow_trunk = 3,
		.grow_shift = 2,
		.need_lock = 1,
		.release_mem_en = 0,
		.per_core_cache = (1 << 16),
		.malloc = mlx5_malloc,
		.free = mlx5_free,
		.type = "mlx5_tag_ipool",
	},
	[MLX5_IPOOL_PORT_ID] = {
		.size = sizeof(struct mlx5_flow_dv_port_id_action_resource),
		.trunk_size = 64,
		.grow_trunk = 3,
		.grow_shift = 2,
		.need_lock = 1,
		.release_mem_en = 1,
		.malloc = mlx5_malloc,
		.free = mlx5_free,
		.type = "mlx5_port_id_ipool",
	},
	[MLX5_IPOOL_JUMP] = {
		.size = sizeof(struct mlx5_flow_tbl_data_entry),
		.trunk_size = 64,
		.grow_trunk = 3,
		.grow_shift = 2,
		.need_lock = 1,
		.release_mem_en = 1,
		.malloc = mlx5_malloc,
		.free = mlx5_free,
		.type = "mlx5_jump_ipool",
	},
	[MLX5_IPOOL_SAMPLE] = {
		.size = sizeof(struct mlx5_flow_dv_sample_resource),
		.trunk_size = 64,
		.grow_trunk = 3,
		.grow_shift = 2,
		.need_lock = 1,
		.release_mem_en = 1,
		.malloc = mlx5_malloc,
		.free = mlx5_free,
		.type = "mlx5_sample_ipool",
	},
	[MLX5_IPOOL_DEST_ARRAY] = {
		.size = sizeof(struct mlx5_flow_dv_dest_array_resource),
		.trunk_size = 64,
		.grow_trunk = 3,
		.grow_shift = 2,
		.need_lock = 1,
		.release_mem_en = 1,
		.malloc = mlx5_malloc,
		.free = mlx5_free,
		.type = "mlx5_dest_array_ipool",
	},
	[MLX5_IPOOL_TUNNEL_ID] = {
		.size = sizeof(struct mlx5_flow_tunnel),
		.trunk_size = MLX5_MAX_TUNNELS,
		.need_lock = 1,
		.release_mem_en = 1,
		.type = "mlx5_tunnel_offload",
	},
	[MLX5_IPOOL_TNL_TBL_ID] = {
		.size = 0,
		.need_lock = 1,
		.type = "mlx5_flow_tnl_tbl_ipool",
	},
#endif
	[MLX5_IPOOL_MTR] = {
		/**
		 * The ipool index should grow continually from small to big,
		 * for meter idx, so not set grow_trunk to avoid meter index
		 * not jump continually.
		 */
		.size = sizeof(struct mlx5_legacy_flow_meter),
		.trunk_size = 64,
		.need_lock = 1,
		.release_mem_en = 1,
		.malloc = mlx5_malloc,
		.free = mlx5_free,
		.type = "mlx5_meter_ipool",
	},
	[MLX5_IPOOL_MCP] = {
		.size = sizeof(struct mlx5_flow_mreg_copy_resource),
		.trunk_size = 64,
		.grow_trunk = 3,
		.grow_shift = 2,
		.need_lock = 1,
		.release_mem_en = 1,
		.malloc = mlx5_malloc,
		.free = mlx5_free,
		.type = "mlx5_mcp_ipool",
	},
	[MLX5_IPOOL_HRXQ] = {
		.size = (sizeof(struct mlx5_hrxq) + MLX5_RSS_HASH_KEY_LEN),
		.trunk_size = 64,
		.grow_trunk = 3,
		.grow_shift = 2,
		.need_lock = 1,
		.release_mem_en = 1,
		.malloc = mlx5_malloc,
		.free = mlx5_free,
		.type = "mlx5_hrxq_ipool",
	},
	[MLX5_IPOOL_MLX5_FLOW] = {
		/*
		 * MLX5_IPOOL_MLX5_FLOW size varies for DV and VERBS flows.
		 * It set in run time according to PCI function configuration.
		 */
		.size = 0,
		.trunk_size = 64,
		.grow_trunk = 3,
		.grow_shift = 2,
		.need_lock = 1,
		.release_mem_en = 0,
		.per_core_cache = 1 << 19,
		.malloc = mlx5_malloc,
		.free = mlx5_free,
		.type = "mlx5_flow_handle_ipool",
	},
	[MLX5_IPOOL_RTE_FLOW] = {
		.size = sizeof(struct rte_flow),
		.trunk_size = 4096,
		.need_lock = 1,
		.release_mem_en = 1,
		.malloc = mlx5_malloc,
		.free = mlx5_free,
		.type = "rte_flow_ipool",
	},
	[MLX5_IPOOL_RSS_EXPANTION_FLOW_ID] = {
		.size = 0,
		.need_lock = 1,
		.type = "mlx5_flow_rss_id_ipool",
	},
	[MLX5_IPOOL_RSS_SHARED_ACTIONS] = {
		.size = sizeof(struct mlx5_shared_action_rss),
		.trunk_size = 64,
		.grow_trunk = 3,
		.grow_shift = 2,
		.need_lock = 1,
		.release_mem_en = 1,
		.malloc = mlx5_malloc,
		.free = mlx5_free,
		.type = "mlx5_shared_action_rss",
	},
	[MLX5_IPOOL_MTR_POLICY] = {
		/**
		 * The ipool index should grow continually from small to big,
		 * for policy idx, so not set grow_trunk to avoid policy index
		 * not jump continually.
		 */
		.size = sizeof(struct mlx5_flow_meter_sub_policy),
		.trunk_size = 64,
		.need_lock = 1,
		.release_mem_en = 1,
		.malloc = mlx5_malloc,
		.free = mlx5_free,
		.type = "mlx5_meter_policy_ipool",
	},
};

#define MLX5_FLOW_MIN_ID_POOL_SIZE 512
#define MLX5_ID_GENERATION_ARRAY_FACTOR 16

#define MLX5_FLOW_TABLE_HLIST_ARRAY_SIZE 1024

#define MLX5_RXQ_ENH_CQE_COMP_MASK 0x80

/**
 * Decide whether representor ID is a HPF(host PF) port on BF2.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   Non-zero if HPF, otherwise 0.
 */
bool
mlx5_is_hpf(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	uint16_t repr = MLX5_REPRESENTOR_REPR(priv->representor_id);
	int type = MLX5_REPRESENTOR_TYPE(priv->representor_id);

	return priv->representor != 0 && type == RTE_ETH_REPRESENTOR_VF &&
	       MLX5_REPRESENTOR_REPR(-1) == repr;
}

/**
 * Decide whether representor ID is a SF port representor.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   Non-zero if HPF, otherwise 0.
 */
bool
mlx5_is_sf_repr(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	int type = MLX5_REPRESENTOR_TYPE(priv->representor_id);

	return priv->representor != 0 && type == RTE_ETH_REPRESENTOR_SF;
}

/**
 * Initialize the ASO aging management structure.
 *
 * @param[in] sh
 *   Pointer to mlx5_dev_ctx_shared object to free
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_aso_age_mng_init(struct mlx5_dev_ctx_shared *sh)
{
	int err;

	if (sh->aso_age_mng)
		return 0;
	sh->aso_age_mng = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*sh->aso_age_mng),
				      RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
	if (!sh->aso_age_mng) {
		DRV_LOG(ERR, "aso_age_mng allocation was failed.");
		rte_errno = ENOMEM;
		return -ENOMEM;
	}
	err = mlx5_aso_queue_init(sh, ASO_OPC_MOD_FLOW_HIT, 1);
	if (err) {
		mlx5_free(sh->aso_age_mng);
		return -1;
	}
	rte_rwlock_init(&sh->aso_age_mng->resize_rwl);
	rte_spinlock_init(&sh->aso_age_mng->free_sl);
	LIST_INIT(&sh->aso_age_mng->free);
	return 0;
}

/**
 * Close and release all the resources of the ASO aging management structure.
 *
 * @param[in] sh
 *   Pointer to mlx5_dev_ctx_shared object to free.
 */
static void
mlx5_flow_aso_age_mng_close(struct mlx5_dev_ctx_shared *sh)
{
	int i, j;

	mlx5_aso_flow_hit_queue_poll_stop(sh);
	mlx5_aso_queue_uninit(sh, ASO_OPC_MOD_FLOW_HIT);
	if (sh->aso_age_mng->pools) {
		struct mlx5_aso_age_pool *pool;

		for (i = 0; i < sh->aso_age_mng->next; ++i) {
			pool = sh->aso_age_mng->pools[i];
			claim_zero(mlx5_devx_cmd_destroy
						(pool->flow_hit_aso_obj));
			for (j = 0; j < MLX5_COUNTERS_PER_POOL; ++j)
				if (pool->actions[j].dr_action)
					claim_zero
					    (mlx5_flow_os_destroy_flow_action
					      (pool->actions[j].dr_action));
			mlx5_free(pool);
		}
		mlx5_free(sh->aso_age_mng->pools);
	}
	mlx5_free(sh->aso_age_mng);
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

	/*
	 * In HW steering, aging information structure is initialized later
	 * during configure function.
	 */
	if (sh->config.dv_flow_en == 2)
		return;
	for (i = 0; i < sh->max_port; i++) {
		age_info = &sh->port[i].age_info;
		age_info->flags = 0;
		TAILQ_INIT(&age_info->aged_counters);
		LIST_INIT(&age_info->aged_aso);
		rte_spinlock_init(&age_info->aged_sl);
		MLX5_AGE_SET(age_info, MLX5_AGE_TRIGGER);
	}
}

/**
 * DV flow counter mode detect and config.
 *
 * @param dev
 *   Pointer to rte_eth_dev structure.
 *
 */
void
mlx5_flow_counter_mode_config(struct rte_eth_dev *dev __rte_unused)
{
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	struct mlx5_hca_attr *hca_attr = &sh->cdev->config.hca_attr;
	bool fallback;

#ifndef HAVE_IBV_DEVX_ASYNC
	fallback = true;
#else
	fallback = false;
	if (!sh->cdev->config.devx || !sh->config.dv_flow_en ||
	    !hca_attr->flow_counters_dump ||
	    !(hca_attr->flow_counter_bulk_alloc_bitmap & 0x4) ||
	    (mlx5_flow_dv_discover_counter_offset_support(dev) == -ENOTSUP))
		fallback = true;
#endif
	if (fallback)
		DRV_LOG(INFO, "Use fall-back DV counter management. Flow "
			"counter dump:%d, bulk_alloc_bitmap:0x%hhx.",
			hca_attr->flow_counters_dump,
			hca_attr->flow_counter_bulk_alloc_bitmap);
	/* Initialize fallback mode only on the port initializes sh. */
	if (sh->refcnt == 1)
		sh->sws_cmng.counter_fallback = fallback;
	else if (fallback != sh->sws_cmng.counter_fallback)
		DRV_LOG(WARNING, "Port %d in sh has different fallback mode "
			"with others:%d.", PORT_ID(priv), fallback);
#endif
}

/**
 * Initialize the counters management structure.
 *
 * @param[in] sh
 *   Pointer to mlx5_dev_ctx_shared object to free
 *
 * @return
 *   0 on success, otherwise negative errno value and rte_errno is set.
 */
static int
mlx5_flow_counters_mng_init(struct mlx5_dev_ctx_shared *sh)
{
	int i, j;

	if (sh->config.dv_flow_en < 2) {
		void *pools;

		pools = mlx5_malloc(MLX5_MEM_ZERO,
				    sizeof(struct mlx5_flow_counter_pool *) *
				    MLX5_COUNTER_POOLS_MAX_NUM,
				    0, SOCKET_ID_ANY);
		if (!pools) {
			DRV_LOG(ERR,
				"Counter management allocation was failed.");
			rte_errno = ENOMEM;
			return -rte_errno;
		}
		memset(&sh->sws_cmng, 0, sizeof(sh->sws_cmng));
		TAILQ_INIT(&sh->sws_cmng.flow_counters);
		sh->sws_cmng.min_id = MLX5_CNT_BATCH_OFFSET;
		sh->sws_cmng.max_id = -1;
		sh->sws_cmng.last_pool_idx = POOL_IDX_INVALID;
		sh->sws_cmng.pools = pools;
		rte_spinlock_init(&sh->sws_cmng.pool_update_sl);
		for (i = 0; i < MLX5_COUNTER_TYPE_MAX; i++) {
			TAILQ_INIT(&sh->sws_cmng.counters[i]);
			rte_spinlock_init(&sh->sws_cmng.csl[i]);
		}
	} else {
		struct mlx5_hca_attr *attr = &sh->cdev->config.hca_attr;
		uint32_t fw_max_nb_cnts = attr->max_flow_counter;
		uint8_t log_dcs = log2above(fw_max_nb_cnts) - 1;
		uint32_t max_nb_cnts = 0;

		for (i = 0, j = 0; j < MLX5_HWS_CNT_DCS_NUM; ++i) {
			int log_dcs_i = log_dcs - i;

			if (log_dcs_i < 0)
				break;
			if ((max_nb_cnts | RTE_BIT32(log_dcs_i)) >
			    fw_max_nb_cnts)
				continue;
			max_nb_cnts |= RTE_BIT32(log_dcs_i);
			j++;
		}
		sh->hws_max_log_bulk_sz = log_dcs;
		sh->hws_max_nb_counters = max_nb_cnts;
	}
	return 0;
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
	mlx5_os_wrapped_mkey_destroy(&mng->wm);
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
	int i, j;
	int retries = 1024;

	rte_errno = 0;
	while (--retries) {
		rte_eal_alarm_cancel(mlx5_flow_query_alarm, sh);
		if (rte_errno != EINPROGRESS)
			break;
		rte_pause();
	}

	if (sh->sws_cmng.pools) {
		struct mlx5_flow_counter_pool *pool;
		uint16_t n_valid = sh->sws_cmng.n_valid;
		bool fallback = sh->sws_cmng.counter_fallback;

		for (i = 0; i < n_valid; ++i) {
			pool = sh->sws_cmng.pools[i];
			if (!fallback && pool->min_dcs)
				claim_zero(mlx5_devx_cmd_destroy
							       (pool->min_dcs));
			for (j = 0; j < MLX5_COUNTERS_PER_POOL; ++j) {
				struct mlx5_flow_counter *cnt =
						MLX5_POOL_GET_CNT(pool, j);

				if (cnt->action)
					claim_zero
					 (mlx5_flow_os_destroy_flow_action
					  (cnt->action));
				if (fallback && cnt->dcs_when_free)
					claim_zero(mlx5_devx_cmd_destroy
						   (cnt->dcs_when_free));
			}
			mlx5_free(pool);
		}
		mlx5_free(sh->sws_cmng.pools);
	}
	mng = LIST_FIRST(&sh->sws_cmng.mem_mngs);
	while (mng) {
		mlx5_flow_destroy_counter_stat_mem_mng(mng);
		mng = LIST_FIRST(&sh->sws_cmng.mem_mngs);
	}
	memset(&sh->sws_cmng, 0, sizeof(sh->sws_cmng));
}

/**
 * Initialize the aso flow meters management structure.
 *
 * @param[in] sh
 *   Pointer to mlx5_dev_ctx_shared object to free
 */
int
mlx5_aso_flow_mtrs_mng_init(struct mlx5_dev_ctx_shared *sh)
{
	if (!sh->mtrmng) {
		sh->mtrmng = mlx5_malloc(MLX5_MEM_ZERO,
			sizeof(*sh->mtrmng),
			RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
		if (!sh->mtrmng) {
			DRV_LOG(ERR,
			"meter management allocation was failed.");
			rte_errno = ENOMEM;
			return -ENOMEM;
		}
		if (sh->meter_aso_en) {
			rte_spinlock_init(&sh->mtrmng->pools_mng.mtrsl);
			rte_rwlock_init(&sh->mtrmng->pools_mng.resize_mtrwl);
			LIST_INIT(&sh->mtrmng->pools_mng.meters);
		}
		sh->mtrmng->def_policy_id = MLX5_INVALID_POLICY_ID;
	}
	return 0;
}

/**
 * Close and release all the resources of
 * the ASO flow meter management structure.
 *
 * @param[in] sh
 *   Pointer to mlx5_dev_ctx_shared object to free.
 */
static void
mlx5_aso_flow_mtrs_mng_close(struct mlx5_dev_ctx_shared *sh)
{
	struct mlx5_aso_mtr_pool *mtr_pool;
	struct mlx5_flow_mtr_mng *mtrmng = sh->mtrmng;
	uint32_t idx;
#ifdef HAVE_MLX5_DR_CREATE_ACTION_ASO
	struct mlx5_aso_mtr *aso_mtr;
	int i;
#endif /* HAVE_MLX5_DR_CREATE_ACTION_ASO */

	if (sh->meter_aso_en) {
		mlx5_aso_queue_uninit(sh, ASO_OPC_MOD_POLICER);
		idx = mtrmng->pools_mng.n_valid;
		while (idx--) {
			mtr_pool = mtrmng->pools_mng.pools[idx];
#ifdef HAVE_MLX5_DR_CREATE_ACTION_ASO
			for (i = 0; i < MLX5_ASO_MTRS_PER_POOL; i++) {
				aso_mtr = &mtr_pool->mtrs[i];
				if (aso_mtr->fm.meter_action_g)
					claim_zero
					(mlx5_glue->destroy_flow_action
					(aso_mtr->fm.meter_action_g));
				if (aso_mtr->fm.meter_action_y)
					claim_zero
					(mlx5_glue->destroy_flow_action
					(aso_mtr->fm.meter_action_y));
			}
#endif /* HAVE_MLX5_DR_CREATE_ACTION_ASO */
			claim_zero(mlx5_devx_cmd_destroy
						(mtr_pool->devx_obj));
			mtrmng->pools_mng.n_valid--;
			mlx5_free(mtr_pool);
		}
		mlx5_free(sh->mtrmng->pools_mng.pools);
	}
	mlx5_free(sh->mtrmng);
	sh->mtrmng = NULL;
}

/* Send FLOW_AGED event if needed. */
void
mlx5_age_event_prepare(struct mlx5_dev_ctx_shared *sh)
{
	struct mlx5_age_info *age_info;
	uint32_t i;

	for (i = 0; i < sh->max_port; i++) {
		age_info = &sh->port[i].age_info;
		if (!MLX5_AGE_GET(age_info, MLX5_AGE_EVENT_NEW))
			continue;
		MLX5_AGE_UNSET(age_info, MLX5_AGE_EVENT_NEW);
		if (MLX5_AGE_GET(age_info, MLX5_AGE_TRIGGER)) {
			MLX5_AGE_UNSET(age_info, MLX5_AGE_TRIGGER);
			rte_eth_dev_callback_process
				(&rte_eth_devices[sh->port[i].devx_ih_port_id],
				RTE_ETH_EVENT_FLOW_AGED, NULL);
		}
	}
}

/*
 * Initialize the ASO connection tracking structure.
 *
 * @param[in] sh
 *   Pointer to mlx5_dev_ctx_shared object.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_aso_ct_mng_init(struct mlx5_dev_ctx_shared *sh)
{
	int err;

	if (sh->ct_mng)
		return 0;
	sh->ct_mng = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*sh->ct_mng) +
				 sizeof(struct mlx5_aso_sq) * MLX5_ASO_CT_SQ_NUM,
				 RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
	if (!sh->ct_mng) {
		DRV_LOG(ERR, "ASO CT management allocation failed.");
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	err = mlx5_aso_queue_init(sh, ASO_OPC_MOD_CONNECTION_TRACKING, MLX5_ASO_CT_SQ_NUM);
	if (err) {
		mlx5_free(sh->ct_mng);
		/* rte_errno should be extracted from the failure. */
		rte_errno = EINVAL;
		return -rte_errno;
	}
	rte_spinlock_init(&sh->ct_mng->ct_sl);
	rte_rwlock_init(&sh->ct_mng->resize_rwl);
	LIST_INIT(&sh->ct_mng->free_cts);
	return 0;
}

/*
 * Close and release all the resources of the
 * ASO connection tracking management structure.
 *
 * @param[in] sh
 *   Pointer to mlx5_dev_ctx_shared object to free.
 */
static void
mlx5_flow_aso_ct_mng_close(struct mlx5_dev_ctx_shared *sh)
{
	struct mlx5_aso_ct_pools_mng *mng = sh->ct_mng;
	struct mlx5_aso_ct_pool *ct_pool;
	struct mlx5_aso_ct_action *ct;
	uint32_t idx;
	uint32_t val;
	uint32_t cnt;
	int i;

	mlx5_aso_queue_uninit(sh, ASO_OPC_MOD_CONNECTION_TRACKING);
	idx = mng->next;
	while (idx--) {
		cnt = 0;
		ct_pool = mng->pools[idx];
		for (i = 0; i < MLX5_ASO_CT_ACTIONS_PER_POOL; i++) {
			ct = &ct_pool->actions[i];
			val = __atomic_fetch_sub(&ct->refcnt, 1,
						 __ATOMIC_RELAXED);
			MLX5_ASSERT(val == 1);
			if (val > 1)
				cnt++;
#ifdef HAVE_MLX5_DR_ACTION_ASO_CT
			if (ct->dr_action_orig)
				claim_zero(mlx5_glue->destroy_flow_action
							(ct->dr_action_orig));
			if (ct->dr_action_rply)
				claim_zero(mlx5_glue->destroy_flow_action
							(ct->dr_action_rply));
#endif
		}
		claim_zero(mlx5_devx_cmd_destroy(ct_pool->devx_obj));
		if (cnt) {
			DRV_LOG(DEBUG, "%u ASO CT objects are being used in the pool %u",
				cnt, i);
		}
		mlx5_free(ct_pool);
		/* in case of failure. */
		mng->next--;
	}
	mlx5_free(mng->pools);
	mlx5_free(mng);
	/* Management structure must be cleared to 0s during allocation. */
	sh->ct_mng = NULL;
}

/**
 * Initialize the flow resources' indexed mempool.
 *
 * @param[in] sh
 *   Pointer to mlx5_dev_ctx_shared object.
 */
static void
mlx5_flow_ipool_create(struct mlx5_dev_ctx_shared *sh)
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
			cfg.size = sh->config.dv_flow_en ?
				sizeof(struct mlx5_flow_handle) :
				MLX5_FLOW_HANDLE_VERBS_SIZE;
			break;
		}
		if (sh->config.reclaim_mode) {
			cfg.release_mem_en = 1;
			cfg.per_core_cache = 0;
		} else {
			cfg.release_mem_en = 0;
		}
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
	for (i = 0; i < MLX5_MAX_MODIFY_NUM; ++i)
		if (sh->mdh_ipools[i])
			mlx5_ipool_destroy(sh->mdh_ipools[i]);
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
	struct mlx5_ecpri_parser_profile *prf = &priv->sh->ecpri_parser;

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
	struct mlx5_ecpri_parser_profile *prf =	&priv->sh->ecpri_parser;
	struct mlx5_devx_graph_node_attr node = {
		.modify_field_select = 0,
	};
	uint32_t ids[8];
	int ret;

	if (!priv->sh->cdev->config.hca_attr.parse_graph_flex_node) {
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
	prf->obj = mlx5_devx_cmd_create_flex_parser(priv->sh->cdev->ctx, &node);
	if (!prf->obj) {
		DRV_LOG(ERR, "Failed to create flex parser node object.");
		return (rte_errno == 0) ? -ENODEV : -rte_errno;
	}
	prf->num = 2;
	ret = mlx5_devx_cmd_query_parse_samples(prf->obj, ids, prf->num, NULL);
	if (ret) {
		DRV_LOG(ERR, "Failed to query sample IDs.");
		goto error;
	}
	prf->offset[0] = 0x0;
	prf->offset[1] = sizeof(uint32_t);
	prf->ids[0] = ids[0];
	prf->ids[1] = ids[1];
	return 0;
error:
	mlx5_devx_cmd_destroy(prf->obj);
	return (rte_errno == 0) ? -ENODEV : -rte_errno;
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
	struct mlx5_ecpri_parser_profile *prf = &priv->sh->ecpri_parser;

	if (prf->obj)
		mlx5_devx_cmd_destroy(prf->obj);
	prf->obj = NULL;
}

/*
 * Allocation of a flex parser for srh. Once refcnt is zero, the resources held
 * by this parser will be freed.
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_alloc_srh_flex_parser(struct rte_eth_dev *dev)
{
	struct mlx5_devx_graph_node_attr node = {
		.modify_field_select = 0,
	};
	uint32_t ids[MLX5_GRAPH_NODE_SAMPLE_NUM];
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_common_dev_config *config = &priv->sh->cdev->config;
	struct mlx5_hca_flex_attr *attr = &priv->sh->cdev->config.hca_attr.flex;
	void *fp = NULL, *ibv_ctx = priv->sh->cdev->ctx;
	int ret;

	memset(ids, 0xff, sizeof(ids));
	if (!config->hca_attr.parse_graph_flex_node ||
	    !config->hca_attr.flex.query_match_sample_info) {
		DRV_LOG(ERR, "Dynamic flex parser is not supported on HWS");
		return -ENOTSUP;
	}
	if (__atomic_fetch_add(&priv->sh->srh_flex_parser.refcnt, 1, __ATOMIC_RELAXED) + 1 > 1)
		return 0;
	priv->sh->srh_flex_parser.flex.devx_fp = mlx5_malloc(MLX5_MEM_ZERO,
			sizeof(struct mlx5_flex_parser_devx), 0, SOCKET_ID_ANY);
	if (!priv->sh->srh_flex_parser.flex.devx_fp)
		return -ENOMEM;
	node.header_length_mode = MLX5_GRAPH_NODE_LEN_FIELD;
	/* Srv6 first two DW are not counted in. */
	node.header_length_base_value = 0x8;
	/* The unit is uint64_t. */
	node.header_length_field_shift = 0x3;
	/* Header length is the 2nd byte. */
	node.header_length_field_offset = 0x8;
	if (attr->header_length_mask_width < 8)
		node.header_length_field_offset += 8 - attr->header_length_mask_width;
	node.header_length_field_mask = 0xF;
	/* One byte next header protocol. */
	node.next_header_field_size = 0x8;
	node.in[0].arc_parse_graph_node = MLX5_GRAPH_ARC_NODE_IP;
	node.in[0].compare_condition_value = IPPROTO_ROUTING;
	node.sample[0].flow_match_sample_en = 1;
	/* First come first serve no matter inner or outer. */
	node.sample[0].flow_match_sample_tunnel_mode = MLX5_GRAPH_SAMPLE_TUNNEL_FIRST;
	node.sample[0].flow_match_sample_offset_mode = MLX5_GRAPH_SAMPLE_OFFSET_FIXED;
	node.out[0].arc_parse_graph_node = MLX5_GRAPH_ARC_NODE_TCP;
	node.out[0].compare_condition_value = IPPROTO_TCP;
	node.out[1].arc_parse_graph_node = MLX5_GRAPH_ARC_NODE_UDP;
	node.out[1].compare_condition_value = IPPROTO_UDP;
	node.out[2].arc_parse_graph_node = MLX5_GRAPH_ARC_NODE_IPV6;
	node.out[2].compare_condition_value = IPPROTO_IPV6;
	fp = mlx5_devx_cmd_create_flex_parser(ibv_ctx, &node);
	if (!fp) {
		DRV_LOG(ERR, "Failed to create flex parser node object.");
		goto error;
	}
	priv->sh->srh_flex_parser.flex.devx_fp->devx_obj = fp;
	priv->sh->srh_flex_parser.flex.mapnum = 1;
	priv->sh->srh_flex_parser.flex.devx_fp->num_samples = 1;

	ret = mlx5_devx_cmd_query_parse_samples(fp, ids, priv->sh->srh_flex_parser.flex.mapnum,
						&priv->sh->srh_flex_parser.flex.devx_fp->anchor_id);
	if (ret) {
		DRV_LOG(ERR, "Failed to query sample IDs.");
		goto error;
	}
	ret = mlx5_devx_cmd_match_sample_info_query(ibv_ctx, ids[0],
				&priv->sh->srh_flex_parser.flex.devx_fp->sample_info[0]);
	if (ret) {
		DRV_LOG(ERR, "Failed to query sample id information.");
		goto error;
	}
	return 0;
error:
	if (fp)
		mlx5_devx_cmd_destroy(fp);
	if (priv->sh->srh_flex_parser.flex.devx_fp)
		mlx5_free(priv->sh->srh_flex_parser.flex.devx_fp);
	return (rte_errno == 0) ? -ENODEV : -rte_errno;
}

/*
 * Destroy the flex parser node, including the parser itself, input / output
 * arcs and DW samples. Resources could be reused then.
 *
 * @param dev
 *   Pointer to Ethernet device structure
 */
void
mlx5_free_srh_flex_parser(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_internal_flex_parser_profile *fp = &priv->sh->srh_flex_parser;

	if (__atomic_fetch_sub(&fp->refcnt, 1, __ATOMIC_RELAXED) - 1)
		return;
	mlx5_devx_cmd_destroy(fp->flex.devx_fp->devx_obj);
	mlx5_free(fp->flex.devx_fp);
	fp->flex.devx_fp = NULL;
}

uint32_t
mlx5_get_supported_sw_parsing_offloads(const struct mlx5_hca_attr *attr)
{
	uint32_t sw_parsing_offloads = 0;

	if (attr->swp) {
		sw_parsing_offloads |= MLX5_SW_PARSING_CAP;
		if (attr->swp_csum)
			sw_parsing_offloads |= MLX5_SW_PARSING_CSUM_CAP;

		if (attr->swp_lso)
			sw_parsing_offloads |= MLX5_SW_PARSING_TSO_CAP;
	}
	return sw_parsing_offloads;
}

uint32_t
mlx5_get_supported_tunneling_offloads(const struct mlx5_hca_attr *attr)
{
	uint32_t tn_offloads = 0;

	if (attr->tunnel_stateless_vxlan)
		tn_offloads |= MLX5_TUNNELED_OFFLOADS_VXLAN_CAP;
	if (attr->tunnel_stateless_gre)
		tn_offloads |= MLX5_TUNNELED_OFFLOADS_GRE_CAP;
	if (attr->tunnel_stateless_geneve_rx)
		tn_offloads |= MLX5_TUNNELED_OFFLOADS_GENEVE_CAP;
	return tn_offloads;
}

/* Fill all fields of UAR structure. */
static int
mlx5_rxtx_uars_prepare(struct mlx5_dev_ctx_shared *sh)
{
	int ret;

	ret = mlx5_devx_uar_prepare(sh->cdev, &sh->tx_uar);
	if (ret) {
		DRV_LOG(ERR, "Failed to prepare Tx DevX UAR.");
		return -rte_errno;
	}
	MLX5_ASSERT(sh->tx_uar.obj);
	MLX5_ASSERT(mlx5_os_get_devx_uar_base_addr(sh->tx_uar.obj));
	ret = mlx5_devx_uar_prepare(sh->cdev, &sh->rx_uar);
	if (ret) {
		DRV_LOG(ERR, "Failed to prepare Rx DevX UAR.");
		mlx5_devx_uar_release(&sh->tx_uar);
		return -rte_errno;
	}
	MLX5_ASSERT(sh->rx_uar.obj);
	MLX5_ASSERT(mlx5_os_get_devx_uar_base_addr(sh->rx_uar.obj));
	return 0;
}

static void
mlx5_rxtx_uars_release(struct mlx5_dev_ctx_shared *sh)
{
	mlx5_devx_uar_release(&sh->rx_uar);
	mlx5_devx_uar_release(&sh->tx_uar);
}

/**
 * rte_mempool_walk() callback to unregister Rx mempools.
 * It used when implicit mempool registration is disabled.
 *
 * @param mp
 *   The mempool being walked.
 * @param arg
 *   Pointer to the device shared context.
 */
static void
mlx5_dev_ctx_shared_rx_mempool_unregister_cb(struct rte_mempool *mp, void *arg)
{
	struct mlx5_dev_ctx_shared *sh = arg;

	mlx5_dev_mempool_unregister(sh->cdev, mp);
}

/**
 * Callback used when implicit mempool registration is disabled
 * in order to track Rx mempool destruction.
 *
 * @param event
 *   Mempool life cycle event.
 * @param mp
 *   An Rx mempool registered explicitly when the port is started.
 * @param arg
 *   Pointer to a device shared context.
 */
static void
mlx5_dev_ctx_shared_rx_mempool_event_cb(enum rte_mempool_event event,
					struct rte_mempool *mp, void *arg)
{
	struct mlx5_dev_ctx_shared *sh = arg;

	if (event == RTE_MEMPOOL_EVENT_DESTROY)
		mlx5_dev_mempool_unregister(sh->cdev, mp);
}

int
mlx5_dev_ctx_shared_mempool_subscribe(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	int ret;

	/* Check if we only need to track Rx mempool destruction. */
	if (!sh->cdev->config.mr_mempool_reg_en) {
		ret = rte_mempool_event_callback_register
				(mlx5_dev_ctx_shared_rx_mempool_event_cb, sh);
		return ret == 0 || rte_errno == EEXIST ? 0 : ret;
	}
	return mlx5_dev_mempool_subscribe(sh->cdev);
}

/**
 * Set up multiple TISs with different affinities according to
 * number of bonding ports
 *
 * @param priv
 * Pointer of shared context.
 *
 * @return
 * Zero on success, -1 otherwise.
 */
static int
mlx5_setup_tis(struct mlx5_dev_ctx_shared *sh)
{
	struct mlx5_devx_lag_context lag_ctx = { 0 };
	struct mlx5_devx_tis_attr tis_attr = { 0 };
	int i;

	tis_attr.transport_domain = sh->td->id;
	if (sh->bond.n_port) {
		if (!mlx5_devx_cmd_query_lag(sh->cdev->ctx, &lag_ctx)) {
			sh->lag.tx_remap_affinity[0] =
				lag_ctx.tx_remap_affinity_1;
			sh->lag.tx_remap_affinity[1] =
				lag_ctx.tx_remap_affinity_2;
			sh->lag.affinity_mode = lag_ctx.port_select_mode;
		} else {
			DRV_LOG(ERR, "Failed to query lag affinity.");
			return -1;
		}
		if (sh->lag.affinity_mode == MLX5_LAG_MODE_TIS)
			DRV_LOG(DEBUG, "LAG number of ports : %d, affinity_1 & 2 : pf%d & %d.\n",
				sh->bond.n_port, lag_ctx.tx_remap_affinity_1,
				lag_ctx.tx_remap_affinity_2);
		else if (sh->lag.affinity_mode == MLX5_LAG_MODE_HASH)
			DRV_LOG(INFO, "Device %s enabled HW hash based LAG.",
					sh->ibdev_name);
	}
	for (i = 0; i <= sh->bond.n_port; i++) {
		/*
		 * lag_tx_port_affinity: 0 auto-selection, 1 PF1, 2 PF2 vice versa.
		 * Each TIS binds to one PF by setting lag_tx_port_affinity (> 0).
		 * Once LAG enabled, we create multiple TISs and bind each one to
		 * different PFs, then TIS[i+1] gets affinity i+1 and goes to PF i+1.
		 * TIS[0] is reserved for HW Hash mode.
		 */
		tis_attr.lag_tx_port_affinity = i;
		sh->tis[i] = mlx5_devx_cmd_create_tis(sh->cdev->ctx, &tis_attr);
		if (!sh->tis[i]) {
			DRV_LOG(ERR, "Failed to create TIS %d/%d for [bonding] device"
				" %s.", i, sh->bond.n_port,
				sh->ibdev_name);
			return -1;
		}
	}
	return 0;
}

/**
 * Verify and store value for share device argument.
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
mlx5_dev_args_check_handler(const char *key, const char *val, void *opaque)
{
	struct mlx5_sh_config *config = opaque;
	signed long tmp;

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
	if (strcmp(MLX5_TX_PP, key) == 0) {
		unsigned long mod = tmp >= 0 ? tmp : -tmp;

		if (!mod) {
			DRV_LOG(ERR, "Zero Tx packet pacing parameter.");
			rte_errno = EINVAL;
			return -rte_errno;
		}
		config->tx_pp = tmp;
	} else if (strcmp(MLX5_TX_SKEW, key) == 0) {
		config->tx_skew = tmp;
	} else if (strcmp(MLX5_L3_VXLAN_EN, key) == 0) {
		config->l3_vxlan_en = !!tmp;
	} else if (strcmp(MLX5_VF_NL_EN, key) == 0) {
		config->vf_nl_en = !!tmp;
	} else if (strcmp(MLX5_DV_ESW_EN, key) == 0) {
		config->dv_esw_en = !!tmp;
	} else if (strcmp(MLX5_DV_FLOW_EN, key) == 0) {
		if (tmp > 2) {
			DRV_LOG(ERR, "Invalid %s parameter.", key);
			rte_errno = EINVAL;
			return -rte_errno;
		}
		config->dv_flow_en = tmp;
	} else if (strcmp(MLX5_DV_XMETA_EN, key) == 0) {
		if (tmp != MLX5_XMETA_MODE_LEGACY &&
		    tmp != MLX5_XMETA_MODE_META16 &&
		    tmp != MLX5_XMETA_MODE_META32 &&
		    tmp != MLX5_XMETA_MODE_MISS_INFO &&
		    tmp != MLX5_XMETA_MODE_META32_HWS) {
			DRV_LOG(ERR, "Invalid extensive metadata parameter.");
			rte_errno = EINVAL;
			return -rte_errno;
		}
		if (tmp != MLX5_XMETA_MODE_MISS_INFO)
			config->dv_xmeta_en = tmp;
		else
			config->dv_miss_info = 1;
	} else if (strcmp(MLX5_LACP_BY_USER, key) == 0) {
		config->lacp_by_user = !!tmp;
	} else if (strcmp(MLX5_RECLAIM_MEM, key) == 0) {
		if (tmp != MLX5_RCM_NONE &&
		    tmp != MLX5_RCM_LIGHT &&
		    tmp != MLX5_RCM_AGGR) {
			DRV_LOG(ERR, "Unrecognize %s: \"%s\"", key, val);
			rte_errno = EINVAL;
			return -rte_errno;
		}
		config->reclaim_mode = tmp;
	} else if (strcmp(MLX5_DECAP_EN, key) == 0) {
		config->decap_en = !!tmp;
	} else if (strcmp(MLX5_ALLOW_DUPLICATE_PATTERN, key) == 0) {
		config->allow_duplicate_pattern = !!tmp;
	} else if (strcmp(MLX5_FDB_DEFAULT_RULE_EN, key) == 0) {
		config->fdb_def_rule = !!tmp;
	} else if (strcmp(MLX5_HWS_CNT_SERVICE_CORE, key) == 0) {
		config->cnt_svc.service_core = tmp;
	} else if (strcmp(MLX5_HWS_CNT_CYCLE_TIME, key) == 0) {
		config->cnt_svc.cycle_time = tmp;
	} else if (strcmp(MLX5_REPR_MATCHING_EN, key) == 0) {
		config->repr_matching = !!tmp;
	}
	return 0;
}

/**
 * Parse user device parameters and adjust them according to device
 * capabilities.
 *
 * @param sh
 *   Pointer to shared device context.
 * @param mkvlist
 *   Pointer to mlx5 kvargs control, can be NULL if there is no devargs.
 * @param config
 *   Pointer to shared device configuration structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_shared_dev_ctx_args_config(struct mlx5_dev_ctx_shared *sh,
				struct mlx5_kvargs_ctrl *mkvlist,
				struct mlx5_sh_config *config)
{
	const char **params = (const char *[]){
		MLX5_TX_PP,
		MLX5_TX_SKEW,
		MLX5_L3_VXLAN_EN,
		MLX5_VF_NL_EN,
		MLX5_DV_ESW_EN,
		MLX5_DV_FLOW_EN,
		MLX5_DV_XMETA_EN,
		MLX5_LACP_BY_USER,
		MLX5_RECLAIM_MEM,
		MLX5_DECAP_EN,
		MLX5_ALLOW_DUPLICATE_PATTERN,
		MLX5_FDB_DEFAULT_RULE_EN,
		MLX5_HWS_CNT_SERVICE_CORE,
		MLX5_HWS_CNT_CYCLE_TIME,
		MLX5_REPR_MATCHING_EN,
		NULL,
	};
	int ret = 0;

	/* Default configuration. */
	memset(config, 0, sizeof(*config));
	config->vf_nl_en = 1;
	config->dv_esw_en = 1;
	config->dv_flow_en = 1;
	config->decap_en = 1;
	config->allow_duplicate_pattern = 1;
	config->fdb_def_rule = 1;
	config->cnt_svc.cycle_time = MLX5_CNT_SVC_CYCLE_TIME_DEFAULT;
	config->cnt_svc.service_core = rte_get_main_lcore();
	config->repr_matching = 1;
	if (mkvlist != NULL) {
		/* Process parameters. */
		ret = mlx5_kvargs_process(mkvlist, params,
					  mlx5_dev_args_check_handler, config);
		if (ret) {
			DRV_LOG(ERR, "Failed to process device arguments: %s",
				strerror(rte_errno));
			return -rte_errno;
		}
	}
	/* Adjust parameters according to device capabilities. */
	if (config->dv_flow_en && !sh->dev_cap.dv_flow_en) {
		DRV_LOG(WARNING, "DV flow is not supported.");
		config->dv_flow_en = 0;
	}
	if (config->dv_esw_en && !sh->dev_cap.dv_esw_en) {
		DRV_LOG(DEBUG, "E-Switch DV flow is not supported.");
		config->dv_esw_en = 0;
	}
	if (config->dv_esw_en && !config->dv_flow_en) {
		DRV_LOG(DEBUG,
			"E-Switch DV flow is supported only when DV flow is enabled.");
		config->dv_esw_en = 0;
	}
	if (config->dv_miss_info && config->dv_esw_en)
		config->dv_xmeta_en = MLX5_XMETA_MODE_META16;
	if (!config->dv_esw_en &&
	    config->dv_xmeta_en != MLX5_XMETA_MODE_LEGACY) {
		DRV_LOG(WARNING,
			"Metadata mode %u is not supported (no E-Switch).",
			config->dv_xmeta_en);
		config->dv_xmeta_en = MLX5_XMETA_MODE_LEGACY;
	}
	if (config->dv_flow_en != 2 && !config->repr_matching) {
		DRV_LOG(DEBUG, "Disabling representor matching is valid only "
			       "when HW Steering is enabled.");
		config->repr_matching = 1;
	}
	if (config->tx_pp && !sh->dev_cap.txpp_en) {
		DRV_LOG(ERR, "Packet pacing is not supported.");
		rte_errno = ENODEV;
		return -rte_errno;
	}
	if (!config->tx_pp && config->tx_skew &&
	    !sh->cdev->config.hca_attr.wait_on_time) {
		DRV_LOG(WARNING,
			"\"tx_skew\" doesn't affect without \"tx_pp\".");
	}
	/* Check for LRO support. */
	if (mlx5_devx_obj_ops_en(sh) && sh->cdev->config.hca_attr.lro_cap) {
		/* TBD check tunnel lro caps. */
		config->lro_allowed = 1;
		DRV_LOG(DEBUG, "LRO is allowed.");
		DRV_LOG(DEBUG,
			"LRO minimal size of TCP segment required for coalescing is %d bytes.",
			sh->cdev->config.hca_attr.lro_min_mss_size);
	}
	/*
	 * If HW has bug working with tunnel packet decapsulation and scatter
	 * FCS, and decapsulation is needed, clear the hw_fcs_strip bit.
	 * Then RTE_ETH_RX_OFFLOAD_KEEP_CRC bit will not be set anymore.
	 */
	if (sh->dev_cap.scatter_fcs_w_decap_disable && sh->config.decap_en)
		config->hw_fcs_strip = 0;
	else
		config->hw_fcs_strip = sh->dev_cap.hw_fcs_strip;
	DRV_LOG(DEBUG, "FCS stripping configuration is %ssupported",
		(config->hw_fcs_strip ? "" : "not "));
	DRV_LOG(DEBUG, "\"tx_pp\" is %d.", config->tx_pp);
	DRV_LOG(DEBUG, "\"tx_skew\" is %d.", config->tx_skew);
	DRV_LOG(DEBUG, "\"reclaim_mode\" is %u.", config->reclaim_mode);
	DRV_LOG(DEBUG, "\"dv_esw_en\" is %u.", config->dv_esw_en);
	DRV_LOG(DEBUG, "\"dv_flow_en\" is %u.", config->dv_flow_en);
	DRV_LOG(DEBUG, "\"dv_xmeta_en\" is %u.", config->dv_xmeta_en);
	DRV_LOG(DEBUG, "\"dv_miss_info\" is %u.", config->dv_miss_info);
	DRV_LOG(DEBUG, "\"l3_vxlan_en\" is %u.", config->l3_vxlan_en);
	DRV_LOG(DEBUG, "\"vf_nl_en\" is %u.", config->vf_nl_en);
	DRV_LOG(DEBUG, "\"lacp_by_user\" is %u.", config->lacp_by_user);
	DRV_LOG(DEBUG, "\"decap_en\" is %u.", config->decap_en);
	DRV_LOG(DEBUG, "\"allow_duplicate_pattern\" is %u.",
		config->allow_duplicate_pattern);
	DRV_LOG(DEBUG, "\"fdb_def_rule_en\" is %u.", config->fdb_def_rule);
	DRV_LOG(DEBUG, "\"repr_matching_en\" is %u.", config->repr_matching);
	return 0;
}

/**
 * Configure realtime timestamp format.
 *
 * @param sh
 *   Pointer to mlx5_dev_ctx_shared object.
 * @param hca_attr
 *   Pointer to DevX HCA capabilities structure.
 */
void
mlx5_rt_timestamp_config(struct mlx5_dev_ctx_shared *sh,
			 struct mlx5_hca_attr *hca_attr)
{
	uint32_t dw_cnt = MLX5_ST_SZ_DW(register_mtutc);
	uint32_t reg[dw_cnt];
	int ret = ENOTSUP;

	if (hca_attr->access_register_user)
		ret = mlx5_devx_cmd_register_read(sh->cdev->ctx,
						  MLX5_REGISTER_ID_MTUTC, 0,
						  reg, dw_cnt);
	if (!ret) {
		uint32_t ts_mode;

		/* MTUTC register is read successfully. */
		ts_mode = MLX5_GET(register_mtutc, reg, time_stamp_mode);
		if (ts_mode == MLX5_MTUTC_TIMESTAMP_MODE_REAL_TIME)
			sh->dev_cap.rt_timestamp = 1;
	} else {
		/* Kernel does not support register reading. */
		if (hca_attr->dev_freq_khz == (NS_PER_S / MS_PER_S))
			sh->dev_cap.rt_timestamp = 1;
	}
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
 * @param mkvlist
 *   Pointer to mlx5 kvargs control, can be NULL if there is no devargs.
 *
 * @return
 *   Pointer to mlx5_dev_ctx_shared object on success,
 *   otherwise NULL and rte_errno is set.
 */
struct mlx5_dev_ctx_shared *
mlx5_alloc_shared_dev_ctx(const struct mlx5_dev_spawn_data *spawn,
			  struct mlx5_kvargs_ctrl *mkvlist)
{
	struct mlx5_dev_ctx_shared *sh;
	int err = 0;
	uint32_t i;

	MLX5_ASSERT(spawn);
	/* Secondary process should not create the shared context. */
	MLX5_ASSERT(rte_eal_process_type() == RTE_PROC_PRIMARY);
	pthread_mutex_lock(&mlx5_dev_ctx_list_mutex);
	/* Search for IB context by device name. */
	LIST_FOREACH(sh, &mlx5_dev_ctx_list, next) {
		if (!strcmp(sh->ibdev_name, spawn->phys_dev_name)) {
			sh->refcnt++;
			goto exit;
		}
	}
	/* No device found, we have to create new shared context. */
	MLX5_ASSERT(spawn->max_port);
	sh = mlx5_malloc(MLX5_MEM_ZERO | MLX5_MEM_RTE,
			 sizeof(struct mlx5_dev_ctx_shared) +
			 spawn->max_port * sizeof(struct mlx5_dev_shared_port),
			 RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
	if (!sh) {
		DRV_LOG(ERR, "Shared context allocation failure.");
		rte_errno = ENOMEM;
		goto exit;
	}
	pthread_mutex_init(&sh->txpp.mutex, NULL);
	sh->numa_node = spawn->cdev->dev->numa_node;
	sh->cdev = spawn->cdev;
	sh->esw_mode = !!(spawn->info.master || spawn->info.representor);
	if (spawn->bond_info)
		sh->bond = *spawn->bond_info;
	err = mlx5_os_capabilities_prepare(sh);
	if (err) {
		DRV_LOG(ERR, "Fail to configure device capabilities.");
		goto error;
	}
	err = mlx5_shared_dev_ctx_args_config(sh, mkvlist, &sh->config);
	if (err) {
		DRV_LOG(ERR, "Failed to process device configure: %s",
			strerror(rte_errno));
		goto error;
	}
	sh->refcnt = 1;
	sh->max_port = spawn->max_port;
	strncpy(sh->ibdev_name, mlx5_os_get_ctx_device_name(sh->cdev->ctx),
		sizeof(sh->ibdev_name) - 1);
	strncpy(sh->ibdev_path, mlx5_os_get_ctx_device_path(sh->cdev->ctx),
		sizeof(sh->ibdev_path) - 1);
	/*
	 * Setting port_id to max unallowed value means there is no interrupt
	 * subhandler installed for the given port index i.
	 */
	for (i = 0; i < sh->max_port; i++) {
		sh->port[i].ih_port_id = RTE_MAX_ETHPORTS;
		sh->port[i].devx_ih_port_id = RTE_MAX_ETHPORTS;
		sh->port[i].nl_ih_port_id = RTE_MAX_ETHPORTS;
	}
	if (sh->cdev->config.devx) {
		sh->td = mlx5_devx_cmd_create_td(sh->cdev->ctx);
		if (!sh->td) {
			DRV_LOG(ERR, "TD allocation failure");
			rte_errno = ENOMEM;
			goto error;
		}
		if (mlx5_setup_tis(sh)) {
			DRV_LOG(ERR, "TIS allocation failure");
			rte_errno = ENOMEM;
			goto error;
		}
		err = mlx5_rxtx_uars_prepare(sh);
		if (err)
			goto error;
#ifndef RTE_ARCH_64
	} else {
		/* Initialize UAR access locks for 32bit implementations. */
		rte_spinlock_init(&sh->uar_lock_cq);
		for (i = 0; i < MLX5_UAR_PAGE_NUM_MAX; i++)
			rte_spinlock_init(&sh->uar_lock[i]);
#endif
	}
	mlx5_os_dev_shared_handler_install(sh);
	if (LIST_EMPTY(&mlx5_dev_ctx_list)) {
		err = mlx5_flow_os_init_workspace_once();
		if (err)
			goto error;
	}
	err = mlx5_flow_counters_mng_init(sh);
	if (err) {
		DRV_LOG(ERR, "Fail to initialize counters manage.");
		goto error;
	}
	mlx5_flow_aging_init(sh);
	mlx5_flow_ipool_create(sh);
	/* Add context to the global device list. */
	LIST_INSERT_HEAD(&mlx5_dev_ctx_list, sh, next);
	rte_spinlock_init(&sh->geneve_tlv_opt_sl);
exit:
	pthread_mutex_unlock(&mlx5_dev_ctx_list_mutex);
	return sh;
error:
	err = rte_errno;
	pthread_mutex_destroy(&sh->txpp.mutex);
	pthread_mutex_unlock(&mlx5_dev_ctx_list_mutex);
	MLX5_ASSERT(sh);
	mlx5_rxtx_uars_release(sh);
	i = 0;
	do {
		if (sh->tis[i])
			claim_zero(mlx5_devx_cmd_destroy(sh->tis[i]));
	} while (++i < (uint32_t)sh->bond.n_port);
	if (sh->td)
		claim_zero(mlx5_devx_cmd_destroy(sh->td));
	mlx5_free(sh);
	rte_errno = err;
	return NULL;
}

/**
 * Create LWM event_channel and interrupt handle for shared device
 * context. All rxqs sharing the device context share the event_channel.
 * A callback is registered in interrupt thread to receive the LWM event.
 *
 * @param[in] priv
 *   Pointer to mlx5_priv instance.
 *
 * @return
 *   0 on success, negative with rte_errno set.
 */
int
mlx5_lwm_setup(struct mlx5_priv *priv)
{
	int fd_lwm;

	pthread_mutex_init(&priv->sh->lwm_config_lock, NULL);
	priv->sh->devx_channel_lwm = mlx5_os_devx_create_event_channel
			(priv->sh->cdev->ctx,
			 MLX5DV_DEVX_CREATE_EVENT_CHANNEL_FLAGS_OMIT_EV_DATA);
	if (!priv->sh->devx_channel_lwm)
		goto err;
	fd_lwm = mlx5_os_get_devx_channel_fd(priv->sh->devx_channel_lwm);
	priv->sh->intr_handle_lwm = mlx5_os_interrupt_handler_create
		(RTE_INTR_INSTANCE_F_SHARED, true,
		 fd_lwm, mlx5_dev_interrupt_handler_lwm, priv);
	if (!priv->sh->intr_handle_lwm)
		goto err;
	return 0;
err:
	if (priv->sh->devx_channel_lwm) {
		mlx5_os_devx_destroy_event_channel
			(priv->sh->devx_channel_lwm);
		priv->sh->devx_channel_lwm = NULL;
	}
	pthread_mutex_destroy(&priv->sh->lwm_config_lock);
	return -rte_errno;
}

/**
 * Destroy LWM event_channel and interrupt handle for shared device
 * context before free this context. The interrupt handler is also
 * unregistered.
 *
 * @param[in] sh
 *   Pointer to shared device context.
 */
void
mlx5_lwm_unset(struct mlx5_dev_ctx_shared *sh)
{
	if (sh->intr_handle_lwm) {
		mlx5_os_interrupt_handler_destroy(sh->intr_handle_lwm,
			mlx5_dev_interrupt_handler_lwm, (void *)-1);
		sh->intr_handle_lwm = NULL;
	}
	if (sh->devx_channel_lwm) {
		mlx5_os_devx_destroy_event_channel
			(sh->devx_channel_lwm);
		sh->devx_channel_lwm = NULL;
	}
	pthread_mutex_destroy(&sh->lwm_config_lock);
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
	int ret;
	int i = 0;

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
	/* Stop watching for mempool events and unregister all mempools. */
	if (!sh->cdev->config.mr_mempool_reg_en) {
		ret = rte_mempool_event_callback_unregister
				(mlx5_dev_ctx_shared_rx_mempool_event_cb, sh);
		if (ret == 0)
			rte_mempool_walk
			     (mlx5_dev_ctx_shared_rx_mempool_unregister_cb, sh);
	}
	/* Remove context from the global device list. */
	LIST_REMOVE(sh, next);
	/* Release resources on the last device removal. */
	if (LIST_EMPTY(&mlx5_dev_ctx_list)) {
		mlx5_os_net_cleanup();
		mlx5_flow_os_release_workspace();
	}
	pthread_mutex_unlock(&mlx5_dev_ctx_list_mutex);
	if (sh->flex_parsers_dv) {
		mlx5_list_destroy(sh->flex_parsers_dv);
		sh->flex_parsers_dv = NULL;
	}
	/*
	 *  Ensure there is no async event handler installed.
	 *  Only primary process handles async device events.
	 **/
	mlx5_flow_counters_mng_close(sh);
	if (sh->ct_mng)
		mlx5_flow_aso_ct_mng_close(sh);
	if (sh->aso_age_mng) {
		mlx5_flow_aso_age_mng_close(sh);
		sh->aso_age_mng = NULL;
	}
	if (sh->mtrmng)
		mlx5_aso_flow_mtrs_mng_close(sh);
	mlx5_flow_ipool_destroy(sh);
	mlx5_os_dev_shared_handler_uninstall(sh);
	mlx5_rxtx_uars_release(sh);
	do {
		if (sh->tis[i])
			claim_zero(mlx5_devx_cmd_destroy(sh->tis[i]));
	} while (++i < sh->bond.n_port);
	if (sh->td)
		claim_zero(mlx5_devx_cmd_destroy(sh->td));
#ifdef HAVE_MLX5_HWS_SUPPORT
	/* HWS manages geneve_tlv_option resource as global. */
	if (sh->config.dv_flow_en == 2)
		flow_dev_geneve_tlv_option_resource_release(sh);
	else
#endif
		MLX5_ASSERT(sh->geneve_tlv_option_resource == NULL);
	pthread_mutex_destroy(&sh->txpp.mutex);
	mlx5_lwm_unset(sh);
	mlx5_free(sh);
	return;
exit:
	pthread_mutex_unlock(&mlx5_dev_ctx_list_mutex);
}

/**
 * Destroy table hash list.
 *
 * @param[in] priv
 *   Pointer to the private device data structure.
 */
void
mlx5_free_table_hash_list(struct mlx5_priv *priv)
{
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	struct mlx5_hlist **tbls = (priv->sh->config.dv_flow_en == 2) ?
				   &sh->groups : &sh->flow_tbls;
	if (*tbls == NULL)
		return;
	mlx5_hlist_destroy(*tbls);
	*tbls = NULL;
}

#ifdef HAVE_MLX5_HWS_SUPPORT
/**
 * Allocate HW steering group hash list.
 *
 * @param[in] priv
 *   Pointer to the private device data structure.
 */
static int
mlx5_alloc_hw_group_hash_list(struct mlx5_priv *priv)
{
	int err = 0;
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	char s[MLX5_NAME_SIZE];

	MLX5_ASSERT(sh);
	snprintf(s, sizeof(s), "%s_flow_groups", priv->sh->ibdev_name);
	sh->groups = mlx5_hlist_create
			(s, MLX5_FLOW_TABLE_HLIST_ARRAY_SIZE,
			 false, true, sh,
			 flow_hw_grp_create_cb,
			 flow_hw_grp_match_cb,
			 flow_hw_grp_remove_cb,
			 flow_hw_grp_clone_cb,
			 flow_hw_grp_clone_free_cb);
	if (!sh->groups) {
		DRV_LOG(ERR, "flow groups with hash creation failed.");
		err = ENOMEM;
	}
	return err;
}
#endif


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
mlx5_alloc_table_hash_list(struct mlx5_priv *priv __rte_unused)
{
	int err = 0;

	/* Tables are only used in DV and DR modes. */
#if defined(HAVE_IBV_FLOW_DV_SUPPORT) || !defined(HAVE_INFINIBAND_VERBS_H)
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	char s[MLX5_NAME_SIZE];

#ifdef HAVE_MLX5_HWS_SUPPORT
	if (priv->sh->config.dv_flow_en == 2)
		return mlx5_alloc_hw_group_hash_list(priv);
#endif
	MLX5_ASSERT(sh);
	snprintf(s, sizeof(s), "%s_flow_table", priv->sh->ibdev_name);
	sh->flow_tbls = mlx5_hlist_create(s, MLX5_FLOW_TABLE_HLIST_ARRAY_SIZE,
					  false, true, sh,
					  flow_dv_tbl_create_cb,
					  flow_dv_tbl_match_cb,
					  flow_dv_tbl_remove_cb,
					  flow_dv_tbl_clone_cb,
					  flow_dv_tbl_clone_free_cb);
	if (!sh->flow_tbls) {
		DRV_LOG(ERR, "flow tables with hash creation failed.");
		err = ENOMEM;
		return err;
	}
#ifndef HAVE_MLX5DV_DR
	struct rte_flow_error error;
	struct rte_eth_dev *dev = &rte_eth_devices[priv->dev_data->port_id];

	/*
	 * In case we have not DR support, the zero tables should be created
	 * because DV expect to see them even if they cannot be created by
	 * RDMA-CORE.
	 */
	if (!flow_dv_tbl_resource_get(dev, 0, 0, 0, 0,
		NULL, 0, 1, 0, &error) ||
	    !flow_dv_tbl_resource_get(dev, 0, 1, 0, 0,
		NULL, 0, 1, 0, &error) ||
	    !flow_dv_tbl_resource_get(dev, 0, 0, 1, 0,
		NULL, 0, 1, 0, &error)) {
		err = ENOMEM;
		goto error;
	}
	return err;
error:
	mlx5_free_table_hash_list(priv);
#endif /* HAVE_MLX5DV_DR */
#endif
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
	if (udp_tunnel->prot_type == RTE_ETH_TUNNEL_TYPE_VXLAN &&
	    udp_tunnel->udp_port == 4789)
		return 0;
	if (udp_tunnel->prot_type == RTE_ETH_TUNNEL_TYPE_VXLAN_GPE &&
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

	mlx5_proc_priv_uninit(dev);
	/*
	 * UAR register table follows the process private structure. BlueFlame
	 * registers for Tx queues are stored in the table.
	 */
	ppriv_size = sizeof(struct mlx5_proc_priv) +
		     priv->txqs_n * sizeof(struct mlx5_uar_data);
	ppriv = mlx5_malloc(MLX5_MEM_RTE | MLX5_MEM_ZERO, ppriv_size,
			    RTE_CACHE_LINE_SIZE, dev->device->numa_node);
	if (!ppriv) {
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	ppriv->uar_table_sz = priv->txqs_n;
	dev->process_private = ppriv;
	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		priv->sh->pppriv = ppriv;
	return 0;
}

/**
 * Un-initialize process private data structure.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
void
mlx5_proc_priv_uninit(struct rte_eth_dev *dev)
{
	struct mlx5_proc_priv *ppriv = dev->process_private;

	if (!ppriv)
		return;
	if (ppriv->hca_bar)
		mlx5_txpp_unmap_hca_bar(dev);
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
int
mlx5_dev_close(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	unsigned int i;
	int ret;

	if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		/* Check if process_private released. */
		if (!dev->process_private)
			return 0;
		mlx5_tx_uar_uninit_secondary(dev);
		mlx5_proc_priv_uninit(dev);
		rte_eth_dev_release_port(dev);
		return 0;
	}
	if (!priv->sh)
		return 0;
	if (priv->shared_refcnt) {
		DRV_LOG(ERR, "port %u is shared host in use (%u)",
			dev->data->port_id, priv->shared_refcnt);
		rte_errno = EBUSY;
		return -EBUSY;
	}
	DRV_LOG(DEBUG, "port %u closing device \"%s\"",
		dev->data->port_id,
		((priv->sh->cdev->ctx != NULL) ?
		mlx5_os_get_ctx_device_name(priv->sh->cdev->ctx) : ""));
	/*
	 * If default mreg copy action is removed at the stop stage,
	 * the search will return none and nothing will be done anymore.
	 */
	if (priv->sh->config.dv_flow_en != 2)
		mlx5_flow_stop_default(dev);
	mlx5_traffic_disable(dev);
	/*
	 * If all the flows are already flushed in the device stop stage,
	 * then this will return directly without any action.
	 */
	mlx5_flow_list_flush(dev, MLX5_FLOW_TYPE_GEN, true);
	mlx5_action_handle_flush(dev);
	mlx5_flow_meter_flush(dev, NULL);
	/* Prevent crashes when queues are still in use. */
	dev->rx_pkt_burst = rte_eth_pkt_burst_dummy;
	dev->tx_pkt_burst = rte_eth_pkt_burst_dummy;
	rte_wmb();
	/* Disable datapath on secondary process. */
	mlx5_mp_os_req_stop_rxtx(dev);
	/* Free the eCPRI flex parser resource. */
	mlx5_flex_parser_ecpri_release(dev);
	mlx5_flex_item_port_cleanup(dev);
#ifdef HAVE_MLX5_HWS_SUPPORT
	flow_hw_destroy_vport_action(dev);
	flow_hw_resource_release(dev);
	flow_hw_clear_port_info(dev);
	if (priv->sh->config.dv_flow_en == 2) {
		flow_hw_clear_flow_metadata_config();
		flow_hw_clear_tags_set(dev);
	}
#endif
	if (priv->rxq_privs != NULL) {
		/* XXX race condition if mlx5_rx_burst() is still running. */
		rte_delay_us_sleep(1000);
		for (i = 0; (i != priv->rxqs_n); ++i)
			mlx5_rxq_release(dev, i);
		priv->rxqs_n = 0;
		mlx5_free(priv->rxq_privs);
		priv->rxq_privs = NULL;
	}
	if (priv->txqs != NULL) {
		/* XXX race condition if mlx5_tx_burst() is still running. */
		rte_delay_us_sleep(1000);
		for (i = 0; (i != priv->txqs_n); ++i)
			mlx5_txq_release(dev, i);
		priv->txqs_n = 0;
		priv->txqs = NULL;
	}
	mlx5_proc_priv_uninit(dev);
	if (priv->q_counters) {
		mlx5_devx_cmd_destroy(priv->q_counters);
		priv->q_counters = NULL;
	}
	if (priv->drop_queue.hrxq)
		mlx5_drop_action_destroy(dev);
	if (priv->mreg_cp_tbl)
		mlx5_hlist_destroy(priv->mreg_cp_tbl);
	mlx5_mprq_free_mp(dev);
	mlx5_os_free_shared_dr(priv);
	if (priv->rss_conf.rss_key != NULL)
		mlx5_free(priv->rss_conf.rss_key);
	if (priv->reta_idx != NULL)
		mlx5_free(priv->reta_idx);
	if (priv->sh->dev_cap.vf)
		mlx5_os_mac_addr_flush(dev);
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
	ret = mlx5_ext_rxq_verify(dev);
	if (ret)
		DRV_LOG(WARNING, "Port %u some external RxQ still remain.",
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
	if (priv->hrxqs)
		mlx5_list_destroy(priv->hrxqs);
	mlx5_free(priv->ext_rxqs);
	priv->sh->port[priv->dev_port - 1].nl_ih_port_id = RTE_MAX_ETHPORTS;
	/*
	 * The interrupt handler port id must be reset before priv is reset
	 * since 'mlx5_dev_interrupt_nl_cb' uses priv.
	 */
	rte_io_wmb();
	/*
	 * Free the shared context in last turn, because the cleanup
	 * routines above may use some shared fields, like
	 * mlx5_os_mac_addr_flush() uses ibdev_path for retrieving
	 * ifindex if Netlink fails.
	 */
	mlx5_free_shared_dev_ctx(priv->sh);
	if (priv->domain_id != RTE_ETH_DEV_SWITCH_DOMAIN_ID_INVALID) {
		unsigned int c = 0;
		uint16_t port_id;

		MLX5_ETH_FOREACH_DEV(port_id, dev->device) {
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
	return 0;
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
	.representor_info_get = mlx5_representor_info_get,
	.read_clock = mlx5_txpp_read_clock,
	.dev_supported_ptypes_get = mlx5_dev_supported_ptypes_get,
	.vlan_filter_set = mlx5_vlan_filter_set,
	.rx_queue_setup = mlx5_rx_queue_setup,
	.rx_queue_avail_thresh_set = mlx5_rx_queue_lwm_set,
	.rx_queue_avail_thresh_query = mlx5_rx_queue_lwm_query,
	.rx_hairpin_queue_setup = mlx5_rx_hairpin_queue_setup,
	.tx_queue_setup = mlx5_tx_queue_setup,
	.tx_hairpin_queue_setup = mlx5_tx_hairpin_queue_setup,
	.rx_queue_release = mlx5_rx_queue_release,
	.tx_queue_release = mlx5_tx_queue_release,
	.rx_queue_start = mlx5_rx_queue_start,
	.rx_queue_stop = mlx5_rx_queue_stop,
	.tx_queue_start = mlx5_tx_queue_start,
	.tx_queue_stop = mlx5_tx_queue_stop,
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
	.flow_ops_get = mlx5_flow_ops_get,
	.rxq_info_get = mlx5_rxq_info_get,
	.txq_info_get = mlx5_txq_info_get,
	.rx_burst_mode_get = mlx5_rx_burst_mode_get,
	.tx_burst_mode_get = mlx5_tx_burst_mode_get,
	.rx_queue_intr_enable = mlx5_rx_intr_enable,
	.rx_queue_intr_disable = mlx5_rx_intr_disable,
	.is_removed = mlx5_is_removed,
	.udp_tunnel_port_add  = mlx5_udp_tunnel_port_add,
	.get_module_info = mlx5_get_module_info,
	.get_module_eeprom = mlx5_get_module_eeprom,
	.hairpin_cap_get = mlx5_hairpin_cap_get,
	.mtr_ops_get = mlx5_flow_meter_ops_get,
	.hairpin_bind = mlx5_hairpin_bind,
	.hairpin_unbind = mlx5_hairpin_unbind,
	.hairpin_get_peer_ports = mlx5_hairpin_get_peer_ports,
	.hairpin_queue_peer_update = mlx5_hairpin_queue_peer_update,
	.hairpin_queue_peer_bind = mlx5_hairpin_queue_peer_bind,
	.hairpin_queue_peer_unbind = mlx5_hairpin_queue_peer_unbind,
	.get_monitor_addr = mlx5_get_monitor_addr,
	.count_aggr_ports = mlx5_count_aggr_ports,
	.map_aggr_tx_affinity = mlx5_map_aggr_tx_affinity,
	.rx_metadata_negotiate = mlx5_flow_rx_metadata_negotiate,
};

/* Available operations from secondary process. */
const struct eth_dev_ops mlx5_dev_sec_ops = {
	.stats_get = mlx5_stats_get,
	.stats_reset = mlx5_stats_reset,
	.xstats_get = mlx5_xstats_get,
	.xstats_reset = mlx5_xstats_reset,
	.xstats_get_names = mlx5_xstats_get_names,
	.fw_version_get = mlx5_fw_version_get,
	.dev_infos_get = mlx5_dev_infos_get,
	.representor_info_get = mlx5_representor_info_get,
	.read_clock = mlx5_txpp_read_clock,
	.rx_queue_start = mlx5_rx_queue_start,
	.rx_queue_stop = mlx5_rx_queue_stop,
	.tx_queue_start = mlx5_tx_queue_start,
	.tx_queue_stop = mlx5_tx_queue_stop,
	.rxq_info_get = mlx5_rxq_info_get,
	.txq_info_get = mlx5_txq_info_get,
	.rx_burst_mode_get = mlx5_rx_burst_mode_get,
	.tx_burst_mode_get = mlx5_tx_burst_mode_get,
	.get_module_info = mlx5_get_module_info,
	.get_module_eeprom = mlx5_get_module_eeprom,
	.count_aggr_ports = mlx5_count_aggr_ports,
	.map_aggr_tx_affinity = mlx5_map_aggr_tx_affinity,
	.rx_metadata_negotiate = mlx5_flow_rx_metadata_negotiate,
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
	.representor_info_get = mlx5_representor_info_get,
	.read_clock = mlx5_txpp_read_clock,
	.dev_supported_ptypes_get = mlx5_dev_supported_ptypes_get,
	.vlan_filter_set = mlx5_vlan_filter_set,
	.rx_queue_setup = mlx5_rx_queue_setup,
	.rx_hairpin_queue_setup = mlx5_rx_hairpin_queue_setup,
	.tx_queue_setup = mlx5_tx_queue_setup,
	.tx_hairpin_queue_setup = mlx5_tx_hairpin_queue_setup,
	.rx_queue_release = mlx5_rx_queue_release,
	.tx_queue_release = mlx5_tx_queue_release,
	.rx_queue_start = mlx5_rx_queue_start,
	.rx_queue_stop = mlx5_rx_queue_stop,
	.tx_queue_start = mlx5_tx_queue_start,
	.tx_queue_stop = mlx5_tx_queue_stop,
	.flow_ctrl_get = mlx5_dev_get_flow_ctrl,
	.flow_ctrl_set = mlx5_dev_set_flow_ctrl,
	.mac_addr_remove = mlx5_mac_addr_remove,
	.mac_addr_add = mlx5_mac_addr_add,
	.mac_addr_set = mlx5_mac_addr_set,
	.set_mc_addr_list = mlx5_set_mc_addr_list,
	.mtu_set = mlx5_dev_set_mtu,
	.vlan_strip_queue_set = mlx5_vlan_strip_queue_set,
	.vlan_offload_set = mlx5_vlan_offload_set,
	.flow_ops_get = mlx5_flow_ops_get,
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
	.hairpin_bind = mlx5_hairpin_bind,
	.hairpin_unbind = mlx5_hairpin_unbind,
	.hairpin_get_peer_ports = mlx5_hairpin_get_peer_ports,
	.hairpin_queue_peer_update = mlx5_hairpin_queue_peer_update,
	.hairpin_queue_peer_bind = mlx5_hairpin_queue_peer_bind,
	.hairpin_queue_peer_unbind = mlx5_hairpin_queue_peer_unbind,
	.get_monitor_addr = mlx5_get_monitor_addr,
	.count_aggr_ports = mlx5_count_aggr_ports,
	.map_aggr_tx_affinity = mlx5_map_aggr_tx_affinity,
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
mlx5_port_args_check_handler(const char *key, const char *val, void *opaque)
{
	struct mlx5_port_config *config = opaque;
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
	if (tmp < 0) {
		/* Negative values are acceptable for some keys only. */
		rte_errno = EINVAL;
		DRV_LOG(WARNING, "%s: invalid negative value \"%s\"", key, val);
		return -rte_errno;
	}
	if (strcmp(MLX5_RXQ_CQE_COMP_EN, key) == 0) {
		if ((tmp & ~MLX5_RXQ_ENH_CQE_COMP_MASK) >
		    MLX5_CQE_RESP_FORMAT_L34H_STRIDX) {
			DRV_LOG(ERR, "invalid CQE compression "
				     "format parameter");
			rte_errno = EINVAL;
			return -rte_errno;
		}
		config->cqe_comp = !!tmp;
		config->cqe_comp_fmt = tmp & ~MLX5_RXQ_ENH_CQE_COMP_MASK;
		config->enh_cqe_comp = !!(tmp & MLX5_RXQ_ENH_CQE_COMP_MASK);
	} else if (strcmp(MLX5_RXQ_PKT_PAD_EN, key) == 0) {
		config->hw_padding = !!tmp;
	} else if (strcmp(MLX5_RX_MPRQ_EN, key) == 0) {
		config->mprq.enabled = !!tmp;
	} else if (strcmp(MLX5_RX_MPRQ_LOG_STRIDE_NUM, key) == 0) {
		config->mprq.log_stride_num = tmp;
	} else if (strcmp(MLX5_RX_MPRQ_LOG_STRIDE_SIZE, key) == 0) {
		config->mprq.log_stride_size = tmp;
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
	} else if (strcmp(MLX5_MAX_DUMP_FILES_NUM, key) == 0) {
		config->max_dump_files_num = tmp;
	} else if (strcmp(MLX5_LRO_TIMEOUT_USEC, key) == 0) {
		config->lro_timeout = tmp;
	} else if (strcmp(MLX5_HP_BUF_SIZE, key) == 0) {
		config->log_hp_size = tmp;
	} else if (strcmp(MLX5_DELAY_DROP, key) == 0) {
		config->std_delay_drop = !!(tmp & MLX5_DELAY_DROP_STANDARD);
		config->hp_delay_drop = !!(tmp & MLX5_DELAY_DROP_HAIRPIN);
	}
	return 0;
}

/**
 * Parse user port parameters and adjust them according to device capabilities.
 *
 * @param priv
 *   Pointer to shared device context.
 * @param mkvlist
 *   Pointer to mlx5 kvargs control, can be NULL if there is no devargs.
 * @param config
 *   Pointer to port configuration structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_port_args_config(struct mlx5_priv *priv, struct mlx5_kvargs_ctrl *mkvlist,
		      struct mlx5_port_config *config)
{
	struct mlx5_hca_attr *hca_attr = &priv->sh->cdev->config.hca_attr;
	struct mlx5_dev_cap *dev_cap = &priv->sh->dev_cap;
	bool devx = priv->sh->cdev->config.devx;
	const char **params = (const char *[]){
		MLX5_RXQ_CQE_COMP_EN,
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
		MLX5_TX_VEC_EN,
		MLX5_RX_VEC_EN,
		MLX5_REPRESENTOR,
		MLX5_MAX_DUMP_FILES_NUM,
		MLX5_LRO_TIMEOUT_USEC,
		MLX5_HP_BUF_SIZE,
		MLX5_DELAY_DROP,
		NULL,
	};
	int ret = 0;

	/* Default configuration. */
	memset(config, 0, sizeof(*config));
	config->mps = MLX5_ARG_UNSET;
	config->cqe_comp = 1;
	config->rx_vec_en = 1;
	config->txq_inline_max = MLX5_ARG_UNSET;
	config->txq_inline_min = MLX5_ARG_UNSET;
	config->txq_inline_mpw = MLX5_ARG_UNSET;
	config->txqs_inline = MLX5_ARG_UNSET;
	config->mprq.max_memcpy_len = MLX5_MPRQ_MEMCPY_DEFAULT_LEN;
	config->mprq.min_rxqs_num = MLX5_MPRQ_MIN_RXQS;
	config->mprq.log_stride_num = MLX5_MPRQ_DEFAULT_LOG_STRIDE_NUM;
	config->log_hp_size = MLX5_ARG_UNSET;
	config->std_delay_drop = 0;
	config->hp_delay_drop = 0;
	if (mkvlist != NULL) {
		/* Process parameters. */
		ret = mlx5_kvargs_process(mkvlist, params,
					  mlx5_port_args_check_handler, config);
		if (ret) {
			DRV_LOG(ERR, "Failed to process port arguments: %s",
				strerror(rte_errno));
			return -rte_errno;
		}
	}
	/* Adjust parameters according to device capabilities. */
	if (config->hw_padding && !dev_cap->hw_padding) {
		DRV_LOG(DEBUG, "Rx end alignment padding isn't supported.");
		config->hw_padding = 0;
	} else if (config->hw_padding) {
		DRV_LOG(DEBUG, "Rx end alignment padding is enabled.");
	}
	/*
	 * MPW is disabled by default, while the Enhanced MPW is enabled
	 * by default.
	 */
	if (config->mps == MLX5_ARG_UNSET)
		config->mps = (dev_cap->mps == MLX5_MPW_ENHANCED) ?
			      MLX5_MPW_ENHANCED : MLX5_MPW_DISABLED;
	else
		config->mps = config->mps ? dev_cap->mps : MLX5_MPW_DISABLED;
	DRV_LOG(INFO, "%sMPS is %s",
		config->mps == MLX5_MPW_ENHANCED ? "enhanced " :
		config->mps == MLX5_MPW ? "legacy " : "",
		config->mps != MLX5_MPW_DISABLED ? "enabled" : "disabled");
	if (priv->sh->config.lro_allowed) {
		/*
		 * If LRO timeout is not configured by application,
		 * use the minimal supported value.
		 */
		if (!config->lro_timeout)
			config->lro_timeout =
				       hca_attr->lro_timer_supported_periods[0];
		DRV_LOG(DEBUG, "LRO session timeout set to %d usec.",
			config->lro_timeout);
	}
	if (config->cqe_comp && !dev_cap->cqe_comp) {
		DRV_LOG(WARNING, "Rx CQE 128B compression is not supported.");
		config->cqe_comp = 0;
	}
	if (config->cqe_comp_fmt == MLX5_CQE_RESP_FORMAT_FTAG_STRIDX &&
	    (!devx || !hca_attr->mini_cqe_resp_flow_tag)) {
		DRV_LOG(WARNING,
			"Flow Tag CQE compression format isn't supported.");
		config->cqe_comp = 0;
	}
	if (config->cqe_comp_fmt == MLX5_CQE_RESP_FORMAT_L34H_STRIDX &&
	    (!devx || !hca_attr->mini_cqe_resp_l3_l4_tag)) {
		DRV_LOG(WARNING,
			"L3/L4 Header CQE compression format isn't supported.");
		config->cqe_comp = 0;
	}
	if (config->enh_cqe_comp && !hca_attr->enhanced_cqe_compression) {
		DRV_LOG(WARNING,
			"Enhanced CQE compression isn't supported.");
		config->enh_cqe_comp = 0;
	}
	DRV_LOG(DEBUG, "%sRx CQE compression is %ssupported.",
		config->enh_cqe_comp ? "Enhanced " : "",
		config->cqe_comp ? "" : "not ");
	if ((config->std_delay_drop || config->hp_delay_drop) &&
	    !dev_cap->rq_delay_drop_en) {
		config->std_delay_drop = 0;
		config->hp_delay_drop = 0;
		DRV_LOG(WARNING, "dev_port-%u: Rxq delay drop isn't supported.",
			priv->dev_port);
	}
	if (config->mprq.enabled && !priv->sh->dev_cap.mprq.enabled) {
		DRV_LOG(WARNING, "Multi-Packet RQ isn't supported.");
		config->mprq.enabled = 0;
	}
	if (config->max_dump_files_num == 0)
		config->max_dump_files_num = 128;
	/* Detect minimal data bytes to inline. */
	mlx5_set_min_inline(priv);
	DRV_LOG(DEBUG, "VLAN insertion in WQE is %ssupported.",
		config->hw_vlan_insert ? "" : "not ");
	DRV_LOG(DEBUG, "\"rxq_pkt_pad_en\" is %u.", config->hw_padding);
	DRV_LOG(DEBUG, "\"rxq_cqe_comp_en\" is %u.", config->cqe_comp);
	DRV_LOG(DEBUG, "\"cqe_comp_fmt\" is %u.", config->cqe_comp_fmt);
	DRV_LOG(DEBUG, "\"enh_cqe_comp\" is %u.", config->enh_cqe_comp);
	DRV_LOG(DEBUG, "\"rx_vec_en\" is %u.", config->rx_vec_en);
	DRV_LOG(DEBUG, "Standard \"delay_drop\" is %u.",
		config->std_delay_drop);
	DRV_LOG(DEBUG, "Hairpin \"delay_drop\" is %u.", config->hp_delay_drop);
	DRV_LOG(DEBUG, "\"max_dump_files_num\" is %u.",
		config->max_dump_files_num);
	DRV_LOG(DEBUG, "\"log_hp_size\" is %u.", config->log_hp_size);
	DRV_LOG(DEBUG, "\"mprq_en\" is %u.", config->mprq.enabled);
	DRV_LOG(DEBUG, "\"mprq_log_stride_num\" is %u.",
		config->mprq.log_stride_num);
	DRV_LOG(DEBUG, "\"mprq_log_stride_size\" is %u.",
		config->mprq.log_stride_size);
	DRV_LOG(DEBUG, "\"mprq_max_memcpy_len\" is %u.",
		config->mprq.max_memcpy_len);
	DRV_LOG(DEBUG, "\"rxqs_min_mprq\" is %u.", config->mprq.min_rxqs_num);
	DRV_LOG(DEBUG, "\"lro_timeout_usec\" is %u.", config->lro_timeout);
	DRV_LOG(DEBUG, "\"txq_mpw_en\" is %d.", config->mps);
	DRV_LOG(DEBUG, "\"txqs_min_inline\" is %d.", config->txqs_inline);
	DRV_LOG(DEBUG, "\"txq_inline_min\" is %d.", config->txq_inline_min);
	DRV_LOG(DEBUG, "\"txq_inline_max\" is %d.", config->txq_inline_max);
	DRV_LOG(DEBUG, "\"txq_inline_mpw\" is %d.", config->txq_inline_mpw);
	return 0;
}

/**
 * Print the key for device argument.
 *
 * It is "dummy" handler whose whole purpose is to enable using
 * mlx5_kvargs_process() function which set devargs as used.
 *
 * @param key
 *   Key argument.
 * @param val
 *   Value associated with key, unused.
 * @param opaque
 *   Unused, can be NULL.
 *
 * @return
 *   0 on success, function cannot fail.
 */
static int
mlx5_dummy_handler(const char *key, const char *val, void *opaque)
{
	DRV_LOG(DEBUG, "\tKey: \"%s\" is set as used.", key);
	RTE_SET_USED(opaque);
	RTE_SET_USED(val);
	return 0;
}

/**
 * Set requested devargs as used when device is already spawned.
 *
 * It is necessary since it is valid to ask probe again for existing device,
 * if its devargs don't assign as used, mlx5_kvargs_validate() will fail.
 *
 * @param name
 *   Name of the existing device.
 * @param port_id
 *   Port identifier of the device.
 * @param mkvlist
 *   Pointer to mlx5 kvargs control to sign as used.
 */
void
mlx5_port_args_set_used(const char *name, uint16_t port_id,
			struct mlx5_kvargs_ctrl *mkvlist)
{
	const char **params = (const char *[]){
		MLX5_RXQ_CQE_COMP_EN,
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
		MLX5_TX_VEC_EN,
		MLX5_RX_VEC_EN,
		MLX5_REPRESENTOR,
		MLX5_MAX_DUMP_FILES_NUM,
		MLX5_LRO_TIMEOUT_USEC,
		MLX5_HP_BUF_SIZE,
		MLX5_DELAY_DROP,
		NULL,
	};

	/* Secondary process should not handle devargs. */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return;
	MLX5_ASSERT(mkvlist != NULL);
	DRV_LOG(DEBUG, "Ethernet device \"%s\" for port %u "
		"already exists, set devargs as used:", name, port_id);
	/* This function cannot fail with this handler. */
	mlx5_kvargs_process(mkvlist, params, mlx5_dummy_handler, NULL);
}

/**
 * Check sibling device configurations when probing again.
 *
 * Sibling devices sharing infiniband device context should have compatible
 * configurations. This regards representors and bonding device.
 *
 * @param cdev
 *   Pointer to mlx5 device structure.
 * @param mkvlist
 *   Pointer to mlx5 kvargs control, can be NULL if there is no devargs.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_probe_again_args_validate(struct mlx5_common_device *cdev,
			       struct mlx5_kvargs_ctrl *mkvlist)
{
	struct mlx5_dev_ctx_shared *sh = NULL;
	struct mlx5_sh_config *config;
	int ret;

	/* Secondary process should not handle devargs. */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;
	pthread_mutex_lock(&mlx5_dev_ctx_list_mutex);
	/* Search for IB context by common device pointer. */
	LIST_FOREACH(sh, &mlx5_dev_ctx_list, next)
		if (sh->cdev == cdev)
			break;
	pthread_mutex_unlock(&mlx5_dev_ctx_list_mutex);
	/* There is sh for this device -> it isn't probe again. */
	if (sh == NULL)
		return 0;
	config = mlx5_malloc(MLX5_MEM_ZERO | MLX5_MEM_RTE,
			     sizeof(struct mlx5_sh_config),
			     RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
	if (config == NULL) {
		rte_errno = -ENOMEM;
		return -rte_errno;
	}
	/*
	 * Creates a temporary IB context configure structure according to new
	 * devargs attached in probing again.
	 */
	ret = mlx5_shared_dev_ctx_args_config(sh, mkvlist, config);
	if (ret) {
		DRV_LOG(ERR, "Failed to process device configure: %s",
			strerror(rte_errno));
		mlx5_free(config);
		return ret;
	}
	/*
	 * Checks the match between the temporary structure and the existing
	 * IB context structure.
	 */
	if (sh->config.dv_flow_en ^ config->dv_flow_en) {
		DRV_LOG(ERR, "\"dv_flow_en\" "
			"configuration mismatch for shared %s context.",
			sh->ibdev_name);
		goto error;
	}
	if ((sh->config.dv_xmeta_en ^ config->dv_xmeta_en) ||
	    (sh->config.dv_miss_info ^ config->dv_miss_info)) {
		DRV_LOG(ERR, "\"dv_xmeta_en\" "
			"configuration mismatch for shared %s context.",
			sh->ibdev_name);
		goto error;
	}
	if (sh->config.dv_esw_en ^ config->dv_esw_en) {
		DRV_LOG(ERR, "\"dv_esw_en\" "
			"configuration mismatch for shared %s context.",
			sh->ibdev_name);
		goto error;
	}
	if (sh->config.reclaim_mode ^ config->reclaim_mode) {
		DRV_LOG(ERR, "\"reclaim_mode\" "
			"configuration mismatch for shared %s context.",
			sh->ibdev_name);
		goto error;
	}
	if (sh->config.allow_duplicate_pattern ^
	    config->allow_duplicate_pattern) {
		DRV_LOG(ERR, "\"allow_duplicate_pattern\" "
			"configuration mismatch for shared %s context.",
			sh->ibdev_name);
		goto error;
	}
	if (sh->config.fdb_def_rule ^ config->fdb_def_rule) {
		DRV_LOG(ERR, "\"fdb_def_rule_en\" configuration mismatch for shared %s context.",
			sh->ibdev_name);
		goto error;
	}
	if (sh->config.l3_vxlan_en ^ config->l3_vxlan_en) {
		DRV_LOG(ERR, "\"l3_vxlan_en\" "
			"configuration mismatch for shared %s context.",
			sh->ibdev_name);
		goto error;
	}
	if (sh->config.decap_en ^ config->decap_en) {
		DRV_LOG(ERR, "\"decap_en\" "
			"configuration mismatch for shared %s context.",
			sh->ibdev_name);
		goto error;
	}
	if (sh->config.lacp_by_user ^ config->lacp_by_user) {
		DRV_LOG(ERR, "\"lacp_by_user\" "
			"configuration mismatch for shared %s context.",
			sh->ibdev_name);
		goto error;
	}
	if (sh->config.tx_pp ^ config->tx_pp) {
		DRV_LOG(ERR, "\"tx_pp\" "
			"configuration mismatch for shared %s context.",
			sh->ibdev_name);
		goto error;
	}
	if (sh->config.tx_skew ^ config->tx_skew) {
		DRV_LOG(ERR, "\"tx_skew\" "
			"configuration mismatch for shared %s context.",
			sh->ibdev_name);
		goto error;
	}
	mlx5_free(config);
	return 0;
error:
	mlx5_free(config);
	rte_errno = EINVAL;
	return -rte_errno;
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
 * @param priv
 *   Pointer to the private device data structure.
 */
void
mlx5_set_min_inline(struct mlx5_priv *priv)
{
	struct mlx5_hca_attr *hca_attr = &priv->sh->cdev->config.hca_attr;
	struct mlx5_port_config *config = &priv->config;

	if (config->txq_inline_min != MLX5_ARG_UNSET) {
		/* Application defines size of inlined data explicitly. */
		if (priv->pci_dev != NULL) {
			switch (priv->pci_dev->id.device_id) {
			case PCI_DEVICE_ID_MELLANOX_CONNECTX4:
			case PCI_DEVICE_ID_MELLANOX_CONNECTX4VF:
				if (config->txq_inline_min <
					       (int)MLX5_INLINE_HSIZE_L2) {
					DRV_LOG(DEBUG,
						"txq_inline_mix aligned to minimal ConnectX-4 required value %d",
						(int)MLX5_INLINE_HSIZE_L2);
					config->txq_inline_min =
							MLX5_INLINE_HSIZE_L2;
				}
				break;
			}
		}
		goto exit;
	}
	if (hca_attr->eth_net_offloads) {
		/* We have DevX enabled, inline mode queried successfully. */
		switch (hca_attr->wqe_inline_mode) {
		case MLX5_CAP_INLINE_MODE_L2:
			/* outer L2 header must be inlined. */
			config->txq_inline_min = MLX5_INLINE_HSIZE_L2;
			goto exit;
		case MLX5_CAP_INLINE_MODE_NOT_REQUIRED:
			/* No inline data are required by NIC. */
			config->txq_inline_min = MLX5_INLINE_HSIZE_NONE;
			config->hw_vlan_insert =
				hca_attr->wqe_vlan_insert;
			DRV_LOG(DEBUG, "Tx VLAN insertion is supported");
			goto exit;
		case MLX5_CAP_INLINE_MODE_VPORT_CONTEXT:
			/* inline mode is defined by NIC vport context. */
			if (!hca_attr->eth_virt)
				break;
			switch (hca_attr->vport_inline_mode) {
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
	if (priv->pci_dev == NULL) {
		config->txq_inline_min = MLX5_INLINE_HSIZE_NONE;
		goto exit;
	}
	/*
	 * We get here if we are unable to deduce
	 * inline data size with DevX. Try PCI ID
	 * to determine old NICs.
	 */
	switch (priv->pci_dev->id.device_id) {
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
	switch (sh->config.dv_xmeta_en) {
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
	case MLX5_XMETA_MODE_META32_HWS:
		meta = UINT32_MAX;
		mark = MLX5_FLOW_MARK_MASK;
		break;
	default:
		meta = 0;
		mark = 0;
		MLX5_ASSERT(false);
		break;
	}
	if (sh->dv_mark_mask && sh->dv_mark_mask != mark)
		DRV_LOG(WARNING, "metadata MARK mask mismatch %08X:%08X",
				 sh->dv_mark_mask, mark);
	else
		sh->dv_mark_mask = mark;
	if (sh->dv_meta_mask && sh->dv_meta_mask != meta)
		DRV_LOG(WARNING, "metadata META mask mismatch %08X:%08X",
				 sh->dv_meta_mask, meta);
	else
		sh->dv_meta_mask = meta;
	if (sh->dv_regc0_mask && sh->dv_regc0_mask != reg_c0)
		DRV_LOG(WARNING, "metadata reg_c0 mask mismatch %08X:%08X",
				 sh->dv_meta_mask, reg_c0);
	else
		sh->dv_regc0_mask = reg_c0;
	DRV_LOG(DEBUG, "metadata mode %u", sh->config.dv_xmeta_en);
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
 * Look for the ethernet device belonging to mlx5 driver.
 *
 * @param[in] port_id
 *   port_id to start looking for device.
 * @param[in] odev
 *   Pointer to the hint device. When device is being probed
 *   the its siblings (master and preceding representors might
 *   not have assigned driver yet (because the mlx5_os_pci_probe()
 *   is not completed yet, for this case match on hint
 *   device may be used to detect sibling device.
 *
 * @return
 *   port_id of found device, RTE_MAX_ETHPORT if not found.
 */
uint16_t
mlx5_eth_find_next(uint16_t port_id, struct rte_device *odev)
{
	while (port_id < RTE_MAX_ETHPORTS) {
		struct rte_eth_dev *dev = &rte_eth_devices[port_id];

		if (dev->state != RTE_ETH_DEV_UNUSED &&
		    dev->device &&
		    (dev->device == odev ||
		     (dev->device->driver &&
		     dev->device->driver->name &&
		     ((strcmp(dev->device->driver->name,
			      MLX5_PCI_DRIVER_NAME) == 0) ||
		      (strcmp(dev->device->driver->name,
			      MLX5_AUXILIARY_DRIVER_NAME) == 0)))))
			break;
		port_id++;
	}
	if (port_id >= RTE_MAX_ETHPORTS)
		return RTE_MAX_ETHPORTS;
	return port_id;
}

/**
 * Callback to remove a device.
 *
 * This function removes all Ethernet devices belong to a given device.
 *
 * @param[in] cdev
 *   Pointer to the generic device.
 *
 * @return
 *   0 on success, the function cannot fail.
 */
int
mlx5_net_remove(struct mlx5_common_device *cdev)
{
	uint16_t port_id;
	int ret = 0;

	RTE_ETH_FOREACH_DEV_OF(port_id, cdev->dev) {
		/*
		 * mlx5_dev_close() is not registered to secondary process,
		 * call the close function explicitly for secondary process.
		 */
		if (rte_eal_process_type() == RTE_PROC_SECONDARY)
			ret |= mlx5_dev_close(&rte_eth_devices[port_id]);
		else
			ret |= rte_eth_dev_close(port_id);
	}
	return ret == 0 ? 0 : -EIO;
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
			       PCI_DEVICE_ID_MELLANOX_BLUEFIELD)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_BLUEFIELDVF)
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
				PCI_DEVICE_ID_MELLANOX_CONNECTXVF)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
				PCI_DEVICE_ID_MELLANOX_BLUEFIELD2)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
				PCI_DEVICE_ID_MELLANOX_CONNECTX6LX)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
				PCI_DEVICE_ID_MELLANOX_CONNECTX7)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
				PCI_DEVICE_ID_MELLANOX_BLUEFIELD3)
	},
	{
		.vendor_id = 0
	}
};

static struct mlx5_class_driver mlx5_net_driver = {
	.drv_class = MLX5_CLASS_ETH,
	.name = RTE_STR(MLX5_ETH_DRIVER_NAME),
	.id_table = mlx5_pci_id_map,
	.probe = mlx5_os_net_probe,
	.remove = mlx5_net_remove,
	.probe_again = 1,
	.intr_lsc = 1,
	.intr_rmv = 1,
};

/* Initialize driver log type. */
RTE_LOG_REGISTER_DEFAULT(mlx5_logtype, NOTICE)

/**
 * Driver initialization routine.
 */
RTE_INIT(rte_mlx5_pmd_init)
{
	pthread_mutex_init(&mlx5_dev_ctx_list_mutex, NULL);
	mlx5_common_init();
	/* Build the static tables for Verbs conversion. */
	mlx5_set_ptype_table();
	mlx5_set_cksum_table();
	mlx5_set_swp_types_table();
	if (mlx5_glue)
		mlx5_class_driver_register(&mlx5_net_driver);
}

RTE_PMD_EXPORT_NAME(MLX5_ETH_DRIVER_NAME, __COUNTER__);
RTE_PMD_REGISTER_PCI_TABLE(MLX5_ETH_DRIVER_NAME, mlx5_pci_id_map);
RTE_PMD_REGISTER_KMOD_DEP(MLX5_ETH_DRIVER_NAME, "* ib_uverbs & mlx5_core & mlx5_ib");
