/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2015 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_H_
#define RTE_PMD_MLX5_H_

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <sys/queue.h>

#include <rte_pci.h>
#include <rte_ether.h>
#include <ethdev_driver.h>
#include <rte_rwlock.h>
#include <rte_interrupts.h>
#include <rte_errno.h>
#include <rte_flow.h>
#include <rte_mtr.h>

#include <mlx5_glue.h>
#include <mlx5_devx_cmds.h>
#include <mlx5_prm.h>
#include <mlx5_common_mp.h>
#include <mlx5_common_mr.h>
#include <mlx5_common_devx.h>
#include <mlx5_common_defs.h>

#include "mlx5_defs.h"
#include "mlx5_utils.h"
#include "mlx5_os.h"
#include "mlx5_autoconf.h"
#include "rte_pmd_mlx5.h"
#if defined(HAVE_IBV_FLOW_DV_SUPPORT) || !defined(HAVE_INFINIBAND_VERBS_H)
#ifndef RTE_EXEC_ENV_WINDOWS
#define HAVE_MLX5_HWS_SUPPORT 1
#else
#define __be64 uint64_t
#endif
#include "hws/mlx5dr.h"
#endif

#define MLX5_SH(dev) (((struct mlx5_priv *)(dev)->data->dev_private)->sh)

#define MLX5_HW_INV_QUEUE UINT32_MAX

/*
 * The default ipool threshold value indicates which per_core_cache
 * value to set.
 */
#define MLX5_HW_IPOOL_SIZE_THRESHOLD (1 << 19)
/* The default min local cache size. */
#define MLX5_HW_IPOOL_CACHE_MIN (1 << 9)

/*
 * Number of modification commands.
 * The maximal actions amount in FW is some constant, and it is 16 in the
 * latest releases. In some old releases, it will be limited to 8.
 * Since there is no interface to query the capacity, the maximal value should
 * be used to allow PMD to create the flow. The validation will be done in the
 * lower driver layer or FW. A failure will be returned if exceeds the maximal
 * supported actions number on the root table.
 * On non-root tables, there is no limitation, but 32 is enough right now.
 */
#define MLX5_MAX_MODIFY_NUM			32
#define MLX5_ROOT_TBL_MODIFY_NUM		16

/* Maximal number of flex items created on the port.*/
#define MLX5_PORT_FLEX_ITEM_NUM			4

/* Maximal number of field/field parts to map into sample registers .*/
#define MLX5_FLEX_ITEM_MAPPING_NUM		32

enum mlx5_ipool_index {
#if defined(HAVE_IBV_FLOW_DV_SUPPORT) || !defined(HAVE_INFINIBAND_VERBS_H)
	MLX5_IPOOL_DECAP_ENCAP = 0, /* Pool for encap/decap resource. */
	MLX5_IPOOL_PUSH_VLAN, /* Pool for push vlan resource. */
	MLX5_IPOOL_TAG, /* Pool for tag resource. */
	MLX5_IPOOL_PORT_ID, /* Pool for port id resource. */
	MLX5_IPOOL_JUMP, /* Pool for SWS jump resource. */
	/* Pool for HWS group. Jump action will be created internally. */
	MLX5_IPOOL_HW_GRP = MLX5_IPOOL_JUMP,
	MLX5_IPOOL_SAMPLE, /* Pool for sample resource. */
	MLX5_IPOOL_DEST_ARRAY, /* Pool for destination array resource. */
	MLX5_IPOOL_TUNNEL_ID, /* Pool for tunnel offload context */
	MLX5_IPOOL_TNL_TBL_ID, /* Pool for tunnel table ID. */
#endif
	MLX5_IPOOL_MTR, /* Pool for meter resource. */
	MLX5_IPOOL_MCP, /* Pool for metadata resource. */
	MLX5_IPOOL_HRXQ, /* Pool for hrxq resource. */
	MLX5_IPOOL_MLX5_FLOW, /* Pool for mlx5 flow handle. */
	MLX5_IPOOL_RTE_FLOW, /* Pool for rte_flow. */
	MLX5_IPOOL_RSS_EXPANTION_FLOW_ID, /* Pool for Queue/RSS flow ID. */
	MLX5_IPOOL_RSS_SHARED_ACTIONS, /* Pool for RSS shared actions. */
	MLX5_IPOOL_MTR_POLICY, /* Pool for meter policy resource. */
	MLX5_IPOOL_MAX,
};

/*
 * There are three reclaim memory mode supported.
 * 0(none) means no memory reclaim.
 * 1(light) means only PMD level reclaim.
 * 2(aggressive) means both PMD and rdma-core level reclaim.
 */
enum mlx5_reclaim_mem_mode {
	MLX5_RCM_NONE, /* Don't reclaim memory. */
	MLX5_RCM_LIGHT, /* Reclaim PMD level. */
	MLX5_RCM_AGGR, /* Reclaim PMD and rdma-core level. */
};

/* The type of flow. */
enum mlx5_flow_type {
	MLX5_FLOW_TYPE_CTL, /* Control flow. */
	MLX5_FLOW_TYPE_GEN, /* General flow. */
	MLX5_FLOW_TYPE_MCP, /* MCP flow. */
	MLX5_FLOW_TYPE_MAXI,
};

/* The mode of delay drop for Rx queues. */
enum mlx5_delay_drop_mode {
	MLX5_DELAY_DROP_NONE = 0, /* All disabled. */
	MLX5_DELAY_DROP_STANDARD = RTE_BIT32(0), /* Standard queues enable. */
	MLX5_DELAY_DROP_HAIRPIN = RTE_BIT32(1), /* Hairpin queues enable. */
};

/* The HWS action type root/non-root. */
enum mlx5_hw_action_flag_type {
	MLX5_HW_ACTION_FLAG_ROOT, /* Root action. */
	MLX5_HW_ACTION_FLAG_NONE_ROOT, /* Non-root ation. */
	MLX5_HW_ACTION_FLAG_MAX, /* Maximum action flag. */
};

/* Hlist and list callback context. */
struct mlx5_flow_cb_ctx {
	struct rte_eth_dev *dev;
	struct rte_flow_error *error;
	void *data;
	void *data2;
};

/* Device capabilities structure which isn't changed in any stage. */
struct mlx5_dev_cap {
	int max_cq; /* Maximum number of supported CQs */
	int max_qp; /* Maximum number of supported QPs. */
	int max_qp_wr; /* Maximum number of outstanding WR on any WQ. */
	int max_sge;
	/* Maximum number of s/g per WR for SQ & RQ of QP for non RDMA Read
	 * operations.
	 */
	int mps; /* Multi-packet send supported mode. */
	uint32_t vf:1; /* This is a VF. */
	uint32_t sf:1; /* This is a SF. */
	uint32_t txpp_en:1; /* Tx packet pacing is supported. */
	uint32_t mpls_en:1; /* MPLS over GRE/UDP is supported. */
	uint32_t cqe_comp:1; /* CQE compression is supported. */
	uint32_t hw_csum:1; /* Checksum offload is supported. */
	uint32_t hw_padding:1; /* End alignment padding is supported. */
	uint32_t dest_tir:1; /* Whether advanced DR API is available. */
	uint32_t dv_esw_en:1; /* E-Switch DV flow is supported. */
	uint32_t dv_flow_en:1; /* DV flow is supported. */
	uint32_t swp:3; /* Tx generic tunnel checksum and TSO offload. */
	uint32_t hw_vlan_strip:1; /* VLAN stripping is supported. */
	uint32_t scatter_fcs_w_decap_disable:1;
	/* HW has bug working with tunnel packet decap and scatter FCS. */
	uint32_t hw_fcs_strip:1; /* FCS stripping is supported. */
	uint32_t rt_timestamp:1; /* Realtime timestamp format. */
	uint32_t rq_delay_drop_en:1; /* Enable RxQ delay drop. */
	uint32_t tunnel_en:3;
	/* Whether tunnel stateless offloads are supported. */
	uint32_t ind_table_max_size;
	/* Maximum receive WQ indirection table size. */
	uint32_t tso:1; /* Whether TSO is supported. */
	uint32_t tso_max_payload_sz; /* Maximum TCP payload for TSO. */
	struct {
		uint32_t enabled:1; /* Whether MPRQ is enabled. */
		uint32_t log_min_stride_size; /* Log min size of a stride. */
		uint32_t log_max_stride_size; /* Log max size of a stride. */
		uint32_t log_min_stride_num; /* Log min num of strides. */
		uint32_t log_max_stride_num; /* Log max num of strides. */
		uint32_t log_min_stride_wqe_size;
		/* Log min WQE size, (size of single stride)*(num of strides).*/
	} mprq; /* Capability for Multi-Packet RQ. */
	char fw_ver[64]; /* Firmware version of this device. */
};

#define MLX5_MPESW_PORT_INVALID (-1)

/** Data associated with devices to spawn. */
struct mlx5_dev_spawn_data {
	uint32_t ifindex; /**< Network interface index. */
	uint32_t max_port; /**< Device maximal port index. */
	uint32_t phys_port; /**< Device physical port index. */
	int pf_bond; /**< bonding device PF index. < 0 - no bonding */
	int mpesw_port; /**< MPESW uplink index. Valid if mpesw_owner_port >= 0. */
	struct mlx5_switch_info info; /**< Switch information. */
	const char *phys_dev_name; /**< Name of physical device. */
	struct rte_eth_dev *eth_dev; /**< Associated Ethernet device. */
	struct rte_pci_device *pci_dev; /**< Backend PCI device. */
	struct mlx5_common_device *cdev; /**< Backend common device. */
	struct mlx5_bond_info *bond_info;
};

/**
 * Check if the port requested to be probed is MPESW physical device
 * or a representor port.
 *
 * @param spawn
 *   Parameters of the probed port.
 *
 * @return
 *   True if the probed port is a physical device or representor in MPESW setup.
 *   False otherwise or MPESW was not configured.
 */
static inline bool
mlx5_is_probed_port_on_mpesw_device(struct mlx5_dev_spawn_data *spawn)
{
	return spawn->mpesw_port >= 0;
}

/** Data associated with socket messages. */
struct mlx5_flow_dump_req  {
	uint32_t port_id; /**< There are plans in DPDK to extend port_id. */
	uint64_t flow_id;
} __rte_packed;

struct mlx5_flow_dump_ack {
	int rc; /**< Return code. */
};

LIST_HEAD(mlx5_dev_list, mlx5_dev_ctx_shared);

/* Shared data between primary and secondary processes. */
struct mlx5_shared_data {
	rte_spinlock_t lock;
	/* Global spinlock for primary and secondary processes. */
	int init_done; /* Whether primary has done initialization. */
	unsigned int secondary_cnt; /* Number of secondary processes init'd. */
};

/* Per-process data structure, not visible to other processes. */
struct mlx5_local_data {
	int init_done; /* Whether a secondary has done initialization. */
};

extern struct mlx5_shared_data *mlx5_shared_data;

/* Dev ops structs */
extern const struct eth_dev_ops mlx5_dev_ops;
extern const struct eth_dev_ops mlx5_dev_sec_ops;
extern const struct eth_dev_ops mlx5_dev_ops_isolate;

struct mlx5_counter_ctrl {
	/* Name of the counter. */
	char dpdk_name[RTE_ETH_XSTATS_NAME_SIZE];
	/* Name of the counter on the device table. */
	char ctr_name[RTE_ETH_XSTATS_NAME_SIZE];
	uint32_t dev:1; /**< Nonzero for dev counters. */
};

struct mlx5_xstats_ctrl {
	/* Number of device stats. */
	uint16_t stats_n;
	/* Number of device stats, for the 2nd port in bond. */
	uint16_t stats_n_2nd;
	/* Number of device stats identified by PMD. */
	uint16_t mlx5_stats_n;
	/* Index in the device counters table. */
	uint16_t dev_table_idx[MLX5_MAX_XSTATS];
	/* Index in the output table. */
	uint16_t xstats_o_idx[MLX5_MAX_XSTATS];
	uint64_t base[MLX5_MAX_XSTATS];
	uint64_t xstats[MLX5_MAX_XSTATS];
	uint64_t hw_stats[MLX5_MAX_XSTATS];
	struct mlx5_counter_ctrl info[MLX5_MAX_XSTATS];
	/* Index in the device counters table, for the 2nd port in bond. */
	uint16_t dev_table_idx_2nd[MLX5_MAX_XSTATS];
	/* Index in the output table, for the 2nd port in bond. */
	uint16_t xstats_o_idx_2nd[MLX5_MAX_XSTATS];
};

struct mlx5_stats_ctrl {
	/* Base for imissed counter. */
	uint64_t imissed_base;
	uint64_t imissed;
};

/* Maximal size of coalesced segment for LRO is set in chunks of 256 Bytes. */
#define MLX5_LRO_SEG_CHUNK_SIZE	256u

/* Maximal size of aggregated LRO packet. */
#define MLX5_MAX_LRO_SIZE (UINT8_MAX * MLX5_LRO_SEG_CHUNK_SIZE)

/* Maximal number of segments to split. */
#define MLX5_MAX_RXQ_NSEG (1u << MLX5_MAX_LOG_RQ_SEGS)

/*
 * Port configuration structure.
 * User device parameters disabled features.
 * This structure contains all configurations coming from devargs which
 * oriented to port. When probing again, devargs doesn't have to be compatible
 * with primary devargs. It is updated for each port in spawn function.
 */
struct mlx5_port_config {
	unsigned int hw_vlan_insert:1; /* VLAN insertion in WQE is supported. */
	unsigned int hw_padding:1; /* End alignment padding is supported. */
	unsigned int cqe_comp:1; /* CQE compression is enabled. */
	unsigned int enh_cqe_comp:1; /* Enhanced CQE compression is enabled. */
	unsigned int cqe_comp_fmt:3; /* CQE compression format. */
	unsigned int rx_vec_en:1; /* Rx vector is enabled. */
	unsigned int std_delay_drop:1; /* Enable standard Rxq delay drop. */
	unsigned int hp_delay_drop:1; /* Enable hairpin Rxq delay drop. */
	struct {
		unsigned int enabled:1; /* Whether MPRQ is enabled. */
		unsigned int log_stride_num; /* Log number of strides. */
		unsigned int log_stride_size; /* Log size of a stride. */
		unsigned int max_memcpy_len;
		/* Maximum packet size to memcpy Rx packets. */
		unsigned int min_rxqs_num;
		/* Rx queue count threshold to enable MPRQ. */
	} mprq; /* Configurations for Multi-Packet RQ. */
	int mps; /* Multi-packet send supported mode. */
	unsigned int max_dump_files_num; /* Maximum dump files per queue. */
	unsigned int log_hp_size; /* Single hairpin queue data size in total. */
	unsigned int lro_timeout; /* LRO user configuration. */
	int txqs_inline; /* Queue number threshold for inlining. */
	int txq_inline_min; /* Minimal amount of data bytes to inline. */
	int txq_inline_max; /* Max packet size for inlining with SEND. */
	int txq_inline_mpw; /* Max packet size for inlining with eMPW. */
};

/*
 * Share context device configuration structure.
 * User device parameters disabled features.
 * This structure updated once for device in mlx5_alloc_shared_dev_ctx()
 * function and cannot change even when probing again.
 */
struct mlx5_sh_config {
	int tx_pp; /* Timestamp scheduling granularity in nanoseconds. */
	int tx_skew; /* Tx scheduling skew between WQE and data on wire. */
	uint32_t reclaim_mode:2; /* Memory reclaim mode. */
	uint32_t dv_esw_en:1; /* Enable E-Switch DV flow. */
	/* Enable DV flow. 1 means SW steering, 2 means HW steering. */
	uint32_t dv_flow_en:2; /* Enable DV flow. */
	uint32_t dv_xmeta_en:3; /* Enable extensive flow metadata. */
	uint32_t dv_miss_info:1; /* Restore packet after partial hw miss. */
	uint32_t l3_vxlan_en:1; /* Enable L3 VXLAN flow creation. */
	uint32_t vf_nl_en:1; /* Enable Netlink requests in VF mode. */
	uint32_t lacp_by_user:1; /* Enable user to manage LACP traffic. */
	uint32_t decap_en:1; /* Whether decap will be used or not. */
	uint32_t hw_fcs_strip:1; /* FCS stripping is supported. */
	uint32_t allow_duplicate_pattern:1;
	uint32_t lro_allowed:1; /* Whether LRO is allowed. */
	struct {
		uint16_t service_core;
		uint32_t cycle_time; /* query cycle time in milli-second. */
	} cnt_svc; /* configure for HW steering's counter's service. */
	/* Allow/Prevent the duplicate rules pattern. */
	uint32_t fdb_def_rule:1; /* Create FDB default jump rule */
	uint32_t repr_matching:1; /* Enable implicit vport matching in HWS FDB. */
};

/* Structure for VF VLAN workaround. */
struct mlx5_vf_vlan {
	uint32_t tag:12;
	uint32_t created:1;
};

/* Flow drop context necessary due to Verbs API. */
struct mlx5_drop {
	struct mlx5_hrxq *hrxq; /* Hash Rx queue queue. */
	struct mlx5_rxq_priv *rxq; /* Rx queue. */
};

/* Loopback dummy queue resources required due to Verbs API. */
struct mlx5_lb_ctx {
	struct ibv_qp *qp; /* QP object. */
	void *ibv_cq; /* Completion queue. */
	uint16_t refcnt; /* Reference count for representors. */
};

/* HW steering queue job descriptor type. */
enum mlx5_hw_job_type {
	MLX5_HW_Q_JOB_TYPE_CREATE, /* Flow create job type. */
	MLX5_HW_Q_JOB_TYPE_DESTROY, /* Flow destroy job type. */
	MLX5_HW_Q_JOB_TYPE_UPDATE, /* Flow update job type. */
	MLX5_HW_Q_JOB_TYPE_QUERY, /* Flow query job type. */
	MLX5_HW_Q_JOB_TYPE_UPDATE_QUERY, /* Flow update and query job type. */
};

enum mlx5_hw_indirect_type {
	MLX5_HW_INDIRECT_TYPE_LEGACY,
	MLX5_HW_INDIRECT_TYPE_LIST
};

#define MLX5_HW_MAX_ITEMS (16)

/* HW steering flow management job descriptor. */
struct mlx5_hw_q_job {
	uint32_t type; /* Job type. */
	uint32_t indirect_type;
	union {
		struct rte_flow_hw *flow; /* Flow attached to the job. */
		const void *action; /* Indirect action attached to the job. */
	};
	void *user_data; /* Job user data. */
	uint8_t *encap_data; /* Encap data. */
	uint8_t *push_data; /* IPv6 routing push data. */
	struct mlx5_modification_cmd *mhdr_cmd;
	struct rte_flow_item *items;
	union {
		struct {
			/* User memory for query output */
			void *user;
			/* Data extracted from hardware */
			void *hw;
		} __rte_packed query;
		struct rte_flow_item_ethdev port_spec;
		struct rte_flow_item_tag tag_spec;
	} __rte_packed;
	struct rte_flow_hw *upd_flow; /* Flow with updated values. */
};

/* HW steering job descriptor LIFO pool. */
struct mlx5_hw_q {
	uint32_t job_idx; /* Free job index. */
	uint32_t size; /* LIFO size. */
	struct mlx5_hw_q_job **job; /* LIFO header. */
	struct rte_ring *indir_cq; /* Indirect action SW completion queue. */
	struct rte_ring *indir_iq; /* Indirect action SW in progress queue. */
} __rte_cache_aligned;


#define MLX5_COUNTER_POOLS_MAX_NUM (1 << 15)
#define MLX5_COUNTERS_PER_POOL 512
#define MLX5_MAX_PENDING_QUERIES 4
#define MLX5_CNT_MR_ALLOC_BULK 64
#define MLX5_CNT_SHARED_OFFSET 0x80000000
#define IS_BATCH_CNT(cnt) (((cnt) & (MLX5_CNT_SHARED_OFFSET - 1)) >= \
			   MLX5_CNT_BATCH_OFFSET)
#define MLX5_CNT_SIZE (sizeof(struct mlx5_flow_counter))
#define MLX5_AGE_SIZE (sizeof(struct mlx5_age_param))

#define MLX5_CNT_LEN(pool) \
	(MLX5_CNT_SIZE + \
	((pool)->is_aged ? MLX5_AGE_SIZE : 0))
#define MLX5_POOL_GET_CNT(pool, index) \
	((struct mlx5_flow_counter *) \
	((uint8_t *)((pool) + 1) + (index) * (MLX5_CNT_LEN(pool))))
#define MLX5_CNT_ARRAY_IDX(pool, cnt) \
	((int)(((uint8_t *)(cnt) - (uint8_t *)((pool) + 1)) / \
	MLX5_CNT_LEN(pool)))
#define MLX5_TS_MASK_SECS 8ull
/* timestamp wrapping in seconds, must be  power of 2. */

/*
 * The pool index and offset of counter in the pool array makes up the
 * counter index. In case the counter is from pool 0 and offset 0, it
 * should plus 1 to avoid index 0, since 0 means invalid counter index
 * currently.
 */
#define MLX5_MAKE_CNT_IDX(pi, offset) \
	((pi) * MLX5_COUNTERS_PER_POOL + (offset) + 1)
#define MLX5_CNT_TO_AGE(cnt) \
	((struct mlx5_age_param *)((cnt) + 1))
/*
 * The maximum single counter is 0x800000 as MLX5_CNT_BATCH_OFFSET
 * defines. The pool size is 512, pool index should never reach
 * INT16_MAX.
 */
#define POOL_IDX_INVALID UINT16_MAX

/* Age status. */
enum {
	AGE_FREE, /* Initialized state. */
	AGE_CANDIDATE, /* Counter assigned to flows. */
	AGE_TMOUT, /* Timeout, wait for rte_flow_get_aged_flows and destroy. */
};

enum mlx5_counter_type {
	MLX5_COUNTER_TYPE_ORIGIN,
	MLX5_COUNTER_TYPE_AGE,
	MLX5_COUNTER_TYPE_MAX,
};

/* Counter age parameter. */
struct mlx5_age_param {
	uint16_t state; /**< Age state (atomically accessed). */
	uint16_t port_id; /**< Port id of the counter. */
	uint32_t timeout:24; /**< Aging timeout in seconds. */
	uint32_t sec_since_last_hit;
	/**< Time in seconds since last hit (atomically accessed). */
	void *context; /**< Flow counter age context. */
};

struct flow_counter_stats {
	uint64_t hits;
	uint64_t bytes;
};

/* Shared counters information for counters. */
struct mlx5_flow_counter_shared {
	union {
		uint32_t refcnt; /* Only for shared action management. */
		uint32_t id; /* User counter ID for legacy sharing. */
	};
};

struct mlx5_flow_counter_pool;
/* Generic counters information. */
struct mlx5_flow_counter {
	union {
		/*
		 * User-defined counter shared info is only used during
		 * counter active time. And aging counter sharing is not
		 * supported, so active shared counter will not be chained
		 * to the aging list. For shared counter, only when it is
		 * released, the TAILQ entry memory will be used, at that
		 * time, shared memory is not used anymore.
		 *
		 * Similarly to none-batch counter dcs, since it doesn't
		 * support aging, while counter is allocated, the entry
		 * memory is not used anymore. In this case, as bytes
		 * memory is used only when counter is allocated, and
		 * entry memory is used only when counter is free. The
		 * dcs pointer can be saved to these two different place
		 * at different stage. It will eliminate the individual
		 * counter extend struct.
		 */
		TAILQ_ENTRY(mlx5_flow_counter) next;
		/**< Pointer to the next flow counter structure. */
		struct {
			struct mlx5_flow_counter_shared shared_info;
			/**< Shared counter information. */
			void *dcs_when_active;
			/*
			 * For non-batch mode, the dcs will be saved
			 * here when the counter is free.
			 */
		};
	};
	union {
		uint64_t hits; /**< Reset value of hits packets. */
		struct mlx5_flow_counter_pool *pool; /**< Counter pool. */
	};
	union {
		uint64_t bytes; /**< Reset value of bytes. */
		void *dcs_when_free;
		/*
		 * For non-batch mode, the dcs will be saved here
		 * when the counter is free.
		 */
	};
	void *action; /**< Pointer to the dv action. */
};

TAILQ_HEAD(mlx5_counters, mlx5_flow_counter);

/* Generic counter pool structure - query is in pool resolution. */
struct mlx5_flow_counter_pool {
	TAILQ_ENTRY(mlx5_flow_counter_pool) next;
	struct mlx5_counters counters[2]; /* Free counter list. */
	struct mlx5_devx_obj *min_dcs;
	/* The devx object of the minimum counter ID. */
	uint64_t time_of_last_age_check;
	/* System time (from rte_rdtsc()) read in the last aging check. */
	uint32_t index:30; /* Pool index in container. */
	uint32_t is_aged:1; /* Pool with aging counter. */
	volatile uint32_t query_gen:1; /* Query round. */
	rte_spinlock_t sl; /* The pool lock. */
	rte_spinlock_t csl; /* The pool counter free list lock. */
	struct mlx5_counter_stats_raw *raw;
	struct mlx5_counter_stats_raw *raw_hw;
	/* The raw on HW working. */
};

/* Memory management structure for group of counter statistics raws. */
struct mlx5_counter_stats_mem_mng {
	LIST_ENTRY(mlx5_counter_stats_mem_mng) next;
	struct mlx5_counter_stats_raw *raws;
	struct mlx5_pmd_wrapped_mr wm;
};

/* Raw memory structure for the counter statistics values of a pool. */
struct mlx5_counter_stats_raw {
	LIST_ENTRY(mlx5_counter_stats_raw) next;
	struct mlx5_counter_stats_mem_mng *mem_mng;
	volatile struct flow_counter_stats *data;
};

TAILQ_HEAD(mlx5_counter_pools, mlx5_flow_counter_pool);

/* Counter global management structure. */
struct mlx5_flow_counter_mng {
	volatile uint16_t n_valid; /* Number of valid pools. */
	uint16_t last_pool_idx; /* Last used pool index */
	int min_id; /* The minimum counter ID in the pools. */
	int max_id; /* The maximum counter ID in the pools. */
	rte_spinlock_t pool_update_sl; /* The pool update lock. */
	rte_spinlock_t csl[MLX5_COUNTER_TYPE_MAX];
	/* The counter free list lock. */
	struct mlx5_counters counters[MLX5_COUNTER_TYPE_MAX];
	/* Free counter list. */
	struct mlx5_flow_counter_pool **pools; /* Counter pool array. */
	struct mlx5_counter_stats_mem_mng *mem_mng;
	/* Hold the memory management for the next allocated pools raws. */
	struct mlx5_counters flow_counters; /* Legacy flow counter list. */
	uint8_t pending_queries;
	uint16_t pool_index;
	uint8_t query_thread_on;
	bool counter_fallback; /* Use counter fallback management. */
	LIST_HEAD(mem_mngs, mlx5_counter_stats_mem_mng) mem_mngs;
	LIST_HEAD(stat_raws, mlx5_counter_stats_raw) free_stat_raws;
};

/* ASO structures. */
#define MLX5_ASO_QUEUE_LOG_DESC 10

struct mlx5_aso_cq {
	uint16_t log_desc_n;
	uint32_t cq_ci:24;
	struct mlx5_devx_cq cq_obj;
	uint64_t errors;
};

struct mlx5_aso_sq_elem {
	union {
		struct {
			struct mlx5_aso_age_pool *pool;
			uint16_t burst_size;
		};
		struct mlx5_aso_mtr *mtr;
		struct {
			struct mlx5_aso_ct_action *ct;
			char *query_data;
		};
		void *user_data;
		struct mlx5_quota *quota_obj;
	};
};

struct mlx5_aso_sq {
	uint16_t log_desc_n;
	rte_spinlock_t sqsl;
	struct mlx5_aso_cq cq;
	struct mlx5_devx_sq sq_obj;
	struct mlx5_pmd_mr mr;
	volatile struct mlx5_aso_wqe *db;
	uint16_t pi;
	uint16_t db_pi;
	uint32_t head;
	uint32_t tail;
	uint32_t sqn;
	struct mlx5_aso_sq_elem elts[1 << MLX5_ASO_QUEUE_LOG_DESC];
	uint16_t next; /* Pool index of the next pool to query. */
};

struct mlx5_aso_age_action {
	LIST_ENTRY(mlx5_aso_age_action) next;
	void *dr_action;
	uint32_t refcnt;
	/* Following fields relevant only when action is active. */
	uint16_t offset; /* Offset of ASO Flow Hit flag in DevX object. */
	struct mlx5_age_param age_params;
};

#define MLX5_ASO_AGE_ACTIONS_PER_POOL 512
#define MLX5_ASO_AGE_CONTAINER_RESIZE 64

struct mlx5_aso_age_pool {
	struct mlx5_devx_obj *flow_hit_aso_obj;
	uint16_t index; /* Pool index in pools array. */
	uint64_t time_of_last_age_check; /* In seconds. */
	struct mlx5_aso_age_action actions[MLX5_ASO_AGE_ACTIONS_PER_POOL];
};

LIST_HEAD(aso_age_list, mlx5_aso_age_action);

struct mlx5_aso_age_mng {
	struct mlx5_aso_age_pool **pools;
	uint16_t n; /* Total number of pools. */
	uint16_t next; /* Number of pools in use, index of next free pool. */
	rte_rwlock_t resize_rwl; /* Lock for resize objects. */
	rte_spinlock_t free_sl; /* Lock for free list access. */
	struct aso_age_list free; /* Free age actions list - ready to use. */
	struct mlx5_aso_sq aso_sq; /* ASO queue objects. */
};

/* Management structure for geneve tlv option */
struct mlx5_geneve_tlv_option_resource {
	struct mlx5_devx_obj *obj; /* Pointer to the geneve tlv opt object. */
	rte_be16_t option_class; /* geneve tlv opt class.*/
	uint8_t option_type; /* geneve tlv opt type.*/
	uint8_t length; /* geneve tlv opt length. */
	uint32_t refcnt; /* geneve tlv object reference counter */
};


#define MLX5_AGE_EVENT_NEW		1
#define MLX5_AGE_TRIGGER		2
#define MLX5_AGE_SET(age_info, BIT) \
	((age_info)->flags |= (1 << (BIT)))
#define MLX5_AGE_UNSET(age_info, BIT) \
	((age_info)->flags &= ~(1 << (BIT)))
#define MLX5_AGE_GET(age_info, BIT) \
	((age_info)->flags & (1 << (BIT)))
#define GET_PORT_AGE_INFO(priv) \
	(&((priv)->sh->port[(priv)->dev_port - 1].age_info))
/* Current time in seconds. */
#define MLX5_CURR_TIME_SEC	(rte_rdtsc() / rte_get_tsc_hz())

/*
 * HW steering queue oriented AGE info.
 * It contains an array of rings, one for each HWS queue.
 */
struct mlx5_hws_q_age_info {
	uint16_t nb_rings; /* Number of aged-out ring lists. */
	struct rte_ring *aged_lists[]; /* Aged-out lists. */
};

/*
 * HW steering AGE info.
 * It has a ring list containing all aged out flow rules.
 */
struct mlx5_hws_age_info {
	struct rte_ring *aged_list; /* Aged out lists. */
};

/* Aging information for per port. */
struct mlx5_age_info {
	uint8_t flags; /* Indicate if is new event or need to be triggered. */
	union {
		/* SW/FW steering AGE info. */
		struct {
			struct mlx5_counters aged_counters;
			/* Aged counter list. */
			struct aso_age_list aged_aso;
			/* Aged ASO actions list. */
			rte_spinlock_t aged_sl; /* Aged flow list lock. */
		};
		struct {
			struct mlx5_indexed_pool *ages_ipool;
			union {
				struct mlx5_hws_age_info hw_age;
				/* HW steering AGE info. */
				struct mlx5_hws_q_age_info *hw_q_age;
				/* HW steering queue oriented AGE info. */
			};
		};
	};
};

/* Per port data of shared IB device. */
struct mlx5_dev_shared_port {
	uint32_t ih_port_id;
	uint32_t devx_ih_port_id;
	uint32_t nl_ih_port_id;
	/*
	 * Interrupt handler port_id. Used by shared interrupt
	 * handler to find the corresponding rte_eth device
	 * by IB port index. If value is equal or greater
	 * RTE_MAX_ETHPORTS it means there is no subhandler
	 * installed for specified IB port index.
	 */
	struct mlx5_age_info age_info;
	/* Aging information for per port. */
};

/*
 * Max number of actions per DV flow.
 * See CREATE_FLOW_MAX_FLOW_ACTIONS_SUPPORTED
 * in rdma-core file providers/mlx5/verbs.c.
 */
#define MLX5_DV_MAX_NUMBER_OF_ACTIONS 8

/* ASO flow meter structures */
/* Modify this value if enum rte_mtr_color changes. */
#define RTE_MTR_DROPPED RTE_COLORS
/* Yellow is now supported. */
#define MLX5_MTR_RTE_COLORS (RTE_COLOR_YELLOW + 1)
/* table_id 22 bits in mlx5_flow_tbl_key so limit policy number. */
#define MLX5_MAX_SUB_POLICY_TBL_NUM 0x3FFFFF
#define MLX5_INVALID_POLICY_ID UINT32_MAX
/* Suffix table_id on MLX5_FLOW_TABLE_LEVEL_METER. */
#define MLX5_MTR_TABLE_ID_SUFFIX 1
/* Drop table_id on MLX5_FLOW_TABLE_LEVEL_METER. */
#define MLX5_MTR_TABLE_ID_DROP 2
/* Priority of the meter policy matcher. */
#define MLX5_MTR_POLICY_MATCHER_PRIO 0
/* Green & yellow color valid for now. */
#define MLX5_MTR_POLICY_MODE_ALL 0
/* Default policy. */
#define MLX5_MTR_POLICY_MODE_DEF 1
/* Only green color valid. */
#define MLX5_MTR_POLICY_MODE_OG 2
/* Only yellow color valid. */
#define MLX5_MTR_POLICY_MODE_OY 3

enum mlx5_meter_domain {
	MLX5_MTR_DOMAIN_INGRESS,
	MLX5_MTR_DOMAIN_EGRESS,
	MLX5_MTR_DOMAIN_TRANSFER,
	MLX5_MTR_DOMAIN_MAX,
};
#define MLX5_MTR_DOMAIN_INGRESS_BIT  (1 << MLX5_MTR_DOMAIN_INGRESS)
#define MLX5_MTR_DOMAIN_EGRESS_BIT   (1 << MLX5_MTR_DOMAIN_EGRESS)
#define MLX5_MTR_DOMAIN_TRANSFER_BIT (1 << MLX5_MTR_DOMAIN_TRANSFER)
#define MLX5_MTR_ALL_DOMAIN_BIT      (MLX5_MTR_DOMAIN_INGRESS_BIT | \
					MLX5_MTR_DOMAIN_EGRESS_BIT | \
					MLX5_MTR_DOMAIN_TRANSFER_BIT)

/* The color tag rule structure. */
struct mlx5_sub_policy_color_rule {
	void *rule;
	/* The color rule. */
	struct mlx5_flow_dv_matcher *matcher;
	/* The color matcher. */
	TAILQ_ENTRY(mlx5_sub_policy_color_rule) next_port;
	/**< Pointer to the next color rule structure. */
	int32_t src_port;
	/* On which src port this rule applied. */
};

TAILQ_HEAD(mlx5_sub_policy_color_rules, mlx5_sub_policy_color_rule);

/*
 * Meter sub-policy structure.
 * Each RSS TIR in meter policy need its own sub-policy resource.
 */
struct mlx5_flow_meter_sub_policy {
	uint32_t main_policy_id:1;
	/* Main policy id is same as this sub_policy id. */
	uint32_t idx:31;
	/* Index to sub_policy ipool entity. */
	void *main_policy;
	/* Point to struct mlx5_flow_meter_policy. */
	struct mlx5_flow_tbl_resource *tbl_rsc;
	/* The sub-policy table resource. */
	uint32_t rix_hrxq[MLX5_MTR_RTE_COLORS];
	/* Index to TIR resource. */
	struct mlx5_flow_tbl_resource *jump_tbl[MLX5_MTR_RTE_COLORS];
	/* Meter jump/drop table. */
	struct mlx5_sub_policy_color_rules color_rules[RTE_COLORS];
	/* List for the color rules. */
};

struct mlx5_meter_policy_acts {
	uint8_t actions_n;
	/* Number of actions. */
	void *dv_actions[MLX5_DV_MAX_NUMBER_OF_ACTIONS];
	/* Action list. */
};

struct mlx5_meter_policy_action_container {
	uint32_t rix_mark;
	/* Index to the mark action. */
	struct mlx5_flow_dv_modify_hdr_resource *modify_hdr;
	/* Pointer to modify header resource in cache. */
	uint8_t fate_action;
	/* Fate action type. */
	union {
		struct rte_flow_action *rss;
		/* Rss action configuration. */
		uint32_t rix_port_id_action;
		/* Index to port ID action resource. */
		void *dr_jump_action[MLX5_MTR_DOMAIN_MAX];
		/* Jump/drop action per color. */
		uint16_t queue;
		/* Queue action configuration. */
		struct {
			uint32_t next_mtr_id;
			/* The next meter id. */
			void *next_sub_policy;
			/* Next meter's sub-policy. */
		};
	};
};

/* Flow meter policy parameter structure. */
struct mlx5_flow_meter_policy {
	uint32_t is_rss:1;
	/* Is RSS policy table. */
	uint32_t ingress:1;
	/* Rule applies to ingress domain. */
	uint32_t egress:1;
	/* Rule applies to egress domain. */
	uint32_t transfer:1;
	/* Rule applies to transfer domain. */
	uint32_t is_queue:1;
	/* Is queue action in policy table. */
	uint32_t is_hierarchy:1;
	/* Is meter action in policy table. */
	uint32_t match_port:1;
	/* If policy flows match src port. */
	uint32_t hierarchy_match_port:1;
	/* Is any meter in hierarchy contains policy flow that matches src port. */
	uint32_t skip_r:1;
	/* If red color policy is skipped. */
	uint32_t skip_y:1;
	/* If yellow color policy is skipped. */
	uint32_t skip_g:1;
	/* If green color policy is skipped. */
	uint32_t mark:1;
	/* If policy contains mark action. */
	uint32_t initialized:1;
	/* Initialized. */
	uint16_t group;
	/* The group. */
	rte_spinlock_t sl;
	uint32_t ref_cnt;
	/* Use count. */
	struct rte_flow_pattern_template *hws_item_templ;
	/* Hardware steering item templates. */
	struct rte_flow_actions_template *hws_act_templ[MLX5_MTR_DOMAIN_MAX];
	/* Hardware steering action templates. */
	struct rte_flow_template_table *hws_flow_table[MLX5_MTR_DOMAIN_MAX];
	/* Hardware steering tables. */
	struct rte_flow *hws_flow_rule[MLX5_MTR_DOMAIN_MAX][RTE_COLORS];
	/* Hardware steering rules. */
	struct mlx5_meter_policy_action_container act_cnt[MLX5_MTR_RTE_COLORS];
	/* Policy actions container. */
	void *dr_drop_action[MLX5_MTR_DOMAIN_MAX];
	/* drop action for red color. */
	uint16_t sub_policy_num;
	/* Count sub policy tables, 3 bits per domain. */
	struct mlx5_flow_meter_sub_policy **sub_policys[MLX5_MTR_DOMAIN_MAX];
	/* Sub policy table array must be the end of struct. */
};

/* The maximum sub policy is relate to struct mlx5_rss_hash_fields[]. */
#define MLX5_MTR_RSS_MAX_SUB_POLICY 7
#define MLX5_MTR_SUB_POLICY_NUM_SHIFT  3
#define MLX5_MTR_SUB_POLICY_NUM_MASK  0x7
#define MLX5_MTRS_DEFAULT_RULE_PRIORITY 0xFFFF
#define MLX5_MTR_CHAIN_MAX_NUM 8

/* Flow meter default policy parameter structure.
 * Policy index 0 is reserved by default policy table.
 * Action per color as below:
 * green - do nothing, yellow - do nothing, red - drop
 */
struct mlx5_flow_meter_def_policy {
	struct mlx5_flow_meter_sub_policy sub_policy;
	/* Policy rules jump to other tables. */
	void *dr_jump_action[RTE_COLORS];
	/* Jump action per color. */
};

/* Meter parameter structure. */
struct mlx5_flow_meter_info {
	uint32_t meter_id;
	/**< Meter id. */
	uint32_t policy_id;
	/* Policy id, the first sub_policy idx. */
	struct mlx5_flow_meter_profile *profile;
	/**< Meter profile parameters. */
	rte_spinlock_t sl; /**< Meter action spinlock. */
	/** Set of stats counters to be enabled.
	 * @see enum rte_mtr_stats_type
	 */
	uint32_t bytes_dropped:1;
	/** Set bytes dropped stats to be enabled. */
	uint32_t pkts_dropped:1;
	/** Set packets dropped stats to be enabled. */
	uint32_t active_state:1;
	/**< Meter hw active state. */
	uint32_t shared:1;
	/**< Meter shared or not. */
	uint32_t is_enable:1;
	/**< Meter disable/enable state. */
	uint32_t ingress:1;
	/**< Rule applies to egress traffic. */
	uint32_t egress:1;
	/**
	 * Instead of simply matching the properties of traffic as it would
	 * appear on a given DPDK port ID, enabling this attribute transfers
	 * a flow rule to the lowest possible level of any device endpoints
	 * found in the pattern.
	 *
	 * When supported, this effectively enables an application to
	 * re-route traffic not necessarily intended for it (e.g. coming
	 * from or addressed to different physical ports, VFs or
	 * applications) at the device level.
	 *
	 * It complements the behavior of some pattern items such as
	 * RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT and is meaningless without them.
	 *
	 * When transferring flow rules, ingress and egress attributes keep
	 * their original meaning, as if processing traffic emitted or
	 * received by the application.
	 */
	uint32_t transfer:1;
	uint32_t def_policy:1;
	uint32_t initialized:1;
	/* Meter points to default policy. */
	uint32_t color_aware:1;
	/* Meter is color aware mode. */
	void *drop_rule[MLX5_MTR_DOMAIN_MAX];
	/* Meter drop rule in drop table. */
	uint32_t drop_cnt;
	/**< Color counter for drop. */
	uint32_t ref_cnt;
	/**< Use count. */
	struct mlx5_indexed_pool *flow_ipool;
	/**< Index pool for flow id. */
	void *meter_action_g;
	/**< Flow meter action. */
	void *meter_action_y;
	/**< Flow meter action for yellow init_color. */
	uint32_t meter_offset;
	/**< Flow meter offset. */
	uint16_t group;
	/**< Flow meter group. */
};

/* PPS(packets per second) map to BPS(Bytes per second).
 * HW treat packet as 128bytes in PPS mode
 */
#define MLX5_MTRS_PPS_MAP_BPS_SHIFT 7

/* RFC2697 parameter structure. */
struct mlx5_flow_meter_srtcm_rfc2697_prm {
	rte_be32_t cbs_cir;
	/*
	 * bit 24-28: cbs_exponent, bit 16-23 cbs_mantissa,
	 * bit 8-12: cir_exponent, bit 0-7 cir_mantissa.
	 */
	rte_be32_t ebs_eir;
	/*
	 * bit 24-28: ebs_exponent, bit 16-23 ebs_mantissa,
	 * bit 8-12: eir_exponent, bit 0-7 eir_mantissa.
	 */
};

/* Flow meter profile structure. */
struct mlx5_flow_meter_profile {
	TAILQ_ENTRY(mlx5_flow_meter_profile) next;
	/**< Pointer to the next flow meter structure. */
	uint32_t id; /**< Profile id. */
	struct rte_mtr_meter_profile profile; /**< Profile detail. */
	union {
		struct mlx5_flow_meter_srtcm_rfc2697_prm srtcm_prm;
		/**< srtcm_rfc2697 struct. */
	};
	uint32_t ref_cnt; /**< Use count. */
	uint32_t g_support:1; /**< If G color will be generated. */
	uint32_t y_support:1; /**< If Y color will be generated. */
	uint32_t initialized:1; /**< Initialized. */
};

/* 2 meters in each ASO cache line */
#define MLX5_MTRS_CONTAINER_RESIZE 64
/*
 * The pool index and offset of meter in the pool array makes up the
 * meter index. In case the meter is from pool 0 and offset 0, it
 * should plus 1 to avoid index 0, since 0 means invalid meter index
 * currently.
 */
#define MLX5_MAKE_MTR_IDX(pi, offset) \
		((pi) * MLX5_ASO_MTRS_PER_POOL + (offset) + 1)

/*aso flow meter state*/
enum mlx5_aso_mtr_state {
	ASO_METER_FREE, /* In free list. */
	ASO_METER_WAIT, /* ACCESS_ASO WQE in progress. */
	ASO_METER_WAIT_ASYNC, /* CQE will be handled by async pull. */
	ASO_METER_READY, /* CQE received. */
};

/*aso flow meter type*/
enum mlx5_aso_mtr_type {
	ASO_METER_INDIRECT,
	ASO_METER_DIRECT,
};

/* Generic aso_flow_meter information. */
struct mlx5_aso_mtr {
	union {
		LIST_ENTRY(mlx5_aso_mtr) next;
		struct mlx5_aso_mtr_pool *pool;
	};
	enum mlx5_aso_mtr_type type;
	struct mlx5_flow_meter_info fm;
	/**< Pointer to the next aso flow meter structure. */
	uint8_t state; /**< ASO flow meter state. */
	uint32_t offset;
	enum rte_color init_color;
};

/* Generic aso_flow_meter pool structure. */
struct mlx5_aso_mtr_pool {
	struct mlx5_aso_mtr mtrs[MLX5_ASO_MTRS_PER_POOL];
	/*Must be the first in pool*/
	struct mlx5_devx_obj *devx_obj;
	/* The devx object of the minimum aso flow meter ID. */
	struct mlx5dr_action *action; /* HWS action. */
	struct mlx5_indexed_pool *idx_pool; /* HWS index pool. */
	uint32_t index; /* Pool index in management structure. */
	uint32_t nb_sq; /* Number of ASO SQ. */
	struct mlx5_aso_sq *sq; /* ASO SQs. */
};

LIST_HEAD(aso_meter_list, mlx5_aso_mtr);
/* Pools management structure for ASO flow meter pools. */
struct mlx5_aso_mtr_pools_mng {
	volatile uint16_t n_valid; /* Number of valid pools. */
	uint16_t n; /* Number of pools. */
	rte_spinlock_t mtrsl; /* The ASO flow meter free list lock. */
	rte_rwlock_t resize_mtrwl; /* Lock for resize objects. */
	struct aso_meter_list meters; /* Free ASO flow meter list. */
	struct mlx5_aso_sq sq; /*SQ using by ASO flow meter. */
	struct mlx5_aso_mtr_pool **pools; /* ASO flow meter pool array. */
};

/* Bulk management structure for ASO flow meter. */
struct mlx5_mtr_bulk {
	uint32_t size; /* Number of ASO objects. */
	struct mlx5dr_action *action; /* HWS action */
	struct mlx5_devx_obj *devx_obj; /* DEVX object. */
	struct mlx5_aso_mtr *aso; /* Array of ASO objects. */
};

/* Meter management structure for global flow meter resource. */
struct mlx5_flow_mtr_mng {
	struct mlx5_aso_mtr_pools_mng pools_mng;
	/* Pools management structure for ASO flow meter pools. */
	struct mlx5_flow_meter_def_policy *def_policy[MLX5_MTR_DOMAIN_MAX];
	/* Default policy table. */
	uint32_t def_policy_id;
	/* Default policy id. */
	uint32_t def_policy_ref_cnt;
	/** def_policy meter use count. */
	struct mlx5_flow_tbl_resource *drop_tbl[MLX5_MTR_DOMAIN_MAX];
	/* Meter drop table. */
	struct mlx5_flow_dv_matcher *
			drop_matcher[MLX5_MTR_DOMAIN_MAX][MLX5_REG_BITS];
	/* Matcher meter in drop table. */
	struct mlx5_flow_dv_matcher *def_matcher[MLX5_MTR_DOMAIN_MAX];
	/* Default matcher in drop table. */
	void *def_rule[MLX5_MTR_DOMAIN_MAX];
	/* Default rule in drop table. */
	uint8_t max_mtr_bits;
	/* Indicate how many bits are used by meter id at the most. */
	uint8_t max_mtr_flow_bits;
	/* Indicate how many bits are used by meter flow id at the most. */
};

/* Table key of the hash organization. */
union mlx5_flow_tbl_key {
	struct {
		/* Table ID should be at the lowest address. */
		uint32_t level;	/**< Level of the table. */
		uint32_t id:22;	/**< ID of the table. */
		uint32_t dummy:1;	/**< Dummy table for DV API. */
		uint32_t is_fdb:1;	/**< 1 - FDB, 0 - NIC TX/RX. */
		uint32_t is_egress:1;	/**< 1 - egress, 0 - ingress. */
		uint32_t reserved:7;	/**< must be zero for comparison. */
	};
	uint64_t v64;			/**< full 64bits value of key */
};

/* Table structure. */
struct mlx5_flow_tbl_resource {
	void *obj; /**< Pointer to DR table object. */
};

#define MLX5_MAX_TABLES UINT16_MAX
#define MLX5_HAIRPIN_TX_TABLE (UINT16_MAX - 1)
/* Reserve the last two tables for metadata register copy. */
#define MLX5_FLOW_MREG_ACT_TABLE_GROUP (MLX5_MAX_TABLES - 1)
#define MLX5_FLOW_MREG_CP_TABLE_GROUP (MLX5_MAX_TABLES - 2)
/* Tables for metering splits should be added here. */
#define MLX5_FLOW_TABLE_LEVEL_METER (MLX5_MAX_TABLES - 3)
#define MLX5_FLOW_TABLE_LEVEL_POLICY (MLX5_MAX_TABLES - 4)
#define MLX5_MAX_TABLES_EXTERNAL MLX5_FLOW_TABLE_LEVEL_POLICY
#define MLX5_FLOW_TABLE_HWS_POLICY (MLX5_MAX_TABLES - 10)
#define MLX5_MAX_TABLES_FDB UINT16_MAX
#define MLX5_FLOW_TABLE_FACTOR 10

/* ID generation structure. */
struct mlx5_flow_id_pool {
	uint32_t *free_arr; /**< Pointer to the a array of free values. */
	uint32_t base_index;
	/**< The next index that can be used without any free elements. */
	uint32_t *curr; /**< Pointer to the index to pop. */
	uint32_t *last; /**< Pointer to the last element in the empty array. */
	uint32_t max_id; /**< Maximum id can be allocated from the pool. */
};

/* Tx pacing queue structure - for Clock and Rearm queues. */
struct mlx5_txpp_wq {
	/* Completion Queue related data.*/
	struct mlx5_devx_cq cq_obj;
	uint32_t cq_ci:24;
	uint32_t arm_sn:2;
	/* Send Queue related data.*/
	struct mlx5_devx_sq sq_obj;
	uint16_t sq_size; /* Number of WQEs in the queue. */
	uint16_t sq_ci; /* Next WQE to execute. */
};

/* Tx packet pacing internal timestamp. */
struct mlx5_txpp_ts {
	uint64_t ci_ts;
	uint64_t ts;
};

/* Tx packet pacing structure. */
struct mlx5_dev_txpp {
	pthread_mutex_t mutex; /* Pacing create/destroy mutex. */
	uint32_t refcnt; /* Pacing reference counter. */
	uint32_t freq; /* Timestamp frequency, Hz. */
	uint32_t tick; /* Completion tick duration in nanoseconds. */
	uint32_t test; /* Packet pacing test mode. */
	int32_t skew; /* Scheduling skew. */
	struct rte_intr_handle *intr_handle; /* Periodic interrupt. */
	void *echan; /* Event Channel. */
	struct mlx5_txpp_wq clock_queue; /* Clock Queue. */
	struct mlx5_txpp_wq rearm_queue; /* Clock Queue. */
	void *pp; /* Packet pacing context. */
	uint16_t pp_id; /* Packet pacing context index. */
	uint16_t ts_n; /* Number of captured timestamps. */
	uint16_t ts_p; /* Pointer to statistics timestamp. */
	struct mlx5_txpp_ts *tsa; /* Timestamps sliding window stats. */
	struct mlx5_txpp_ts ts; /* Cached completion id/timestamp. */
	uint32_t sync_lost:1; /* ci/timestamp synchronization lost. */
	/* Statistics counters. */
	uint64_t err_miss_int; /* Missed service interrupt. */
	uint64_t err_rearm_queue; /* Rearm Queue errors. */
	uint64_t err_clock_queue; /* Clock Queue errors. */
	uint64_t err_ts_past; /* Timestamp in the past. */
	uint64_t err_ts_future; /* Timestamp in the distant future. */
	uint64_t err_ts_order; /* Timestamp not in ascending order. */
};

/* Sample ID information of eCPRI flex parser structure. */
struct mlx5_ecpri_parser_profile {
	uint32_t num;		/* Actual number of samples. */
	uint32_t ids[8];	/* Sample IDs for this profile. */
	uint8_t offset[8];	/* Bytes offset of each parser. */
	void *obj;		/* Flex parser node object. */
};

/* Max member ports per bonding device. */
#define MLX5_BOND_MAX_PORTS 2

/* Bonding device information. */
struct mlx5_bond_info {
	int n_port; /* Number of bond member ports. */
	uint32_t ifindex;
	char ifname[MLX5_NAMESIZE + 1];
	struct {
		char ifname[MLX5_NAMESIZE + 1];
		uint32_t ifindex;
		struct rte_pci_addr pci_addr;
	} ports[MLX5_BOND_MAX_PORTS];
};

/* Number of connection tracking objects per pool: must be a power of 2. */
#define MLX5_ASO_CT_ACTIONS_PER_POOL 64

/* Generate incremental and unique CT index from pool and offset. */
#define MLX5_MAKE_CT_IDX(pool, offset) \
	((pool) * MLX5_ASO_CT_ACTIONS_PER_POOL + (offset) + 1)

/* ASO Conntrack state. */
enum mlx5_aso_ct_state {
	ASO_CONNTRACK_FREE, /* Inactive, in the free list. */
	ASO_CONNTRACK_WAIT, /* WQE sent in the SQ. */
	ASO_CONNTRACK_WAIT_ASYNC, /* CQE will be handled by async pull. */
	ASO_CONNTRACK_READY, /* CQE received w/o error. */
	ASO_CONNTRACK_QUERY, /* WQE for query sent. */
	ASO_CONNTRACK_MAX, /* Guard. */
};

/* Generic ASO connection tracking structure. */
struct mlx5_aso_ct_action {
	union {
		/* SWS mode struct. */
		struct {
			/* Pointer to the next ASO CT. Used only in SWS. */
			LIST_ENTRY(mlx5_aso_ct_action) next;
		};
		/* HWS mode struct. */
		struct {
			/* Pointer to action pool. Used only in HWS. */
			struct mlx5_aso_ct_pool *pool;
		};
	};
	/* General action object for original dir. */
	void *dr_action_orig;
	/* General action object for reply dir. */
	void *dr_action_rply;
	uint32_t refcnt; /* Action used count in device flows. */
	uint32_t offset; /* Offset of ASO CT in DevX objects bulk. */
	uint16_t peer; /* The only peer port index could also use this CT. */
	enum mlx5_aso_ct_state state; /* ASO CT state. */
	bool is_original; /* The direction of the DR action to be used. */
};

/* CT action object state update. */
#define MLX5_ASO_CT_UPDATE_STATE(c, s) \
	__atomic_store_n(&((c)->state), (s), __ATOMIC_RELAXED)

#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

/* ASO connection tracking software pool definition. */
struct mlx5_aso_ct_pool {
	uint16_t index; /* Pool index in pools array. */
	/* Free ASO CT index in the pool. Used by HWS. */
	struct mlx5_indexed_pool *cts;
	struct mlx5_devx_obj *devx_obj;
	union {
		void *dummy_action;
		/* Dummy action to increase the reference count in the driver. */
		struct mlx5dr_action *dr_action;
		/* HWS action. */
	};
	struct mlx5_aso_sq *sq; /* Async ASO SQ. */
	struct mlx5_aso_sq *shared_sq; /* Shared ASO SQ. */
	struct mlx5_aso_ct_action actions[0];
	/* CT action structures bulk. */
};

LIST_HEAD(aso_ct_list, mlx5_aso_ct_action);

#define MLX5_ASO_CT_SQ_NUM 16

/* Pools management structure for ASO connection tracking pools. */
struct mlx5_aso_ct_pools_mng {
	struct mlx5_aso_ct_pool **pools;
	uint16_t n; /* Total number of pools. */
	uint16_t next; /* Number of pools in use, index of next free pool. */
	uint32_t nb_sq; /* Number of ASO SQ. */
	rte_spinlock_t ct_sl; /* The ASO CT free list lock. */
	rte_rwlock_t resize_rwl; /* The ASO CT pool resize lock. */
	struct aso_ct_list free_cts; /* Free ASO CT objects list. */
	struct mlx5_aso_sq aso_sqs[0]; /* ASO queue objects. */
};

#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

/* LAG attr. */
struct mlx5_lag {
	uint8_t tx_remap_affinity[16]; /* The PF port number of affinity */
	uint8_t affinity_mode; /* TIS or hash based affinity */
};

/* DevX flex parser context. */
struct mlx5_flex_parser_devx {
	struct mlx5_list_entry entry;  /* List element at the beginning. */
	uint32_t num_samples;
	uint8_t anchor_id;
	void *devx_obj;
	struct mlx5_devx_graph_node_attr devx_conf;
	uint32_t sample_ids[MLX5_GRAPH_NODE_SAMPLE_NUM];
	struct mlx5_devx_match_sample_info_query_attr sample_info[MLX5_GRAPH_NODE_SAMPLE_NUM];
};

/* Pattern field descriptor - how to translate flex pattern into samples. */
__extension__
struct mlx5_flex_pattern_field {
	uint16_t width:6;
	uint16_t shift:5;
	uint16_t reg_id:5;
};

#define MLX5_INVALID_SAMPLE_REG_ID 0x1F

/* Port flex item context. */
struct mlx5_flex_item {
	struct mlx5_flex_parser_devx *devx_fp; /* DevX flex parser object. */
	uint32_t refcnt; /* Atomically accessed refcnt by flows. */
	enum rte_flow_item_flex_tunnel_mode tunnel_mode; /* Tunnel mode. */
	uint32_t mapnum; /* Number of pattern translation entries. */
	struct mlx5_flex_pattern_field map[MLX5_FLEX_ITEM_MAPPING_NUM];
};

/*
 * Sample an IPv6 address and the first dword of SRv6 header.
 * Then it is 16 + 4 = 20 bytes which is 5 dwords.
 */
#define MLX5_SRV6_SAMPLE_NUM 5
/* Mlx5 internal flex parser profile structure. */
struct mlx5_internal_flex_parser_profile {
	uint32_t refcnt;
	struct mlx5_flex_item flex; /* Hold map info for modify field. */
};

struct mlx5_send_to_kernel_action {
	void *action;
	void *tbl;
};

#define HWS_CNT_ASO_SQ_NUM 4

struct mlx5_hws_aso_mng {
	uint16_t sq_num;
	struct mlx5_aso_sq sqs[HWS_CNT_ASO_SQ_NUM];
};

struct mlx5_hws_cnt_svc_mng {
	uint32_t refcnt;
	uint32_t service_core;
	uint32_t query_interval;
	rte_thread_t service_thread;
	uint8_t svc_running;
	struct mlx5_hws_aso_mng aso_mng __rte_cache_aligned;
};

#define MLX5_FLOW_HW_TAGS_MAX 12

struct mlx5_dev_registers {
	enum modify_reg aso_reg;
	enum modify_reg hw_avl_tags[MLX5_FLOW_HW_TAGS_MAX];
};

#if defined(HAVE_MLX5DV_DR) && \
	(defined(HAVE_MLX5_DR_CREATE_ACTION_FLOW_METER) || \
	 defined(HAVE_MLX5_DR_CREATE_ACTION_ASO))
#define HAVE_MLX5_DR_CREATE_ACTION_ASO_EXT
#endif

/*
 * Shared Infiniband device context for Master/Representors
 * which belong to same IB device with multiple IB ports.
 **/
struct mlx5_dev_ctx_shared {
	LIST_ENTRY(mlx5_dev_ctx_shared) next;
	uint32_t refcnt;
	uint32_t esw_mode:1; /* Whether is E-Switch mode. */
	uint32_t flow_hit_aso_en:1; /* Flow Hit ASO is supported. */
	uint32_t steering_format_version:4;
	/* Indicates the device steering logic format. */
	uint32_t meter_aso_en:1; /* Flow Meter ASO is supported. */
	uint32_t ct_aso_en:1; /* Connection Tracking ASO is supported. */
	uint32_t tunnel_header_0_1:1; /* tunnel_header_0_1 is supported. */
	uint32_t tunnel_header_2_3:1; /* tunnel_header_2_3 is supported. */
	uint32_t misc5_cap:1; /* misc5 matcher parameter is supported. */
	uint32_t dr_root_drop_action_en:1; /* DR drop action is usable on root tables. */
	uint32_t drop_action_check_flag:1; /* Check Flag for drop action. */
	uint32_t flow_priority_check_flag:1; /* Check Flag for flow priority. */
	uint32_t metadata_regc_check_flag:1; /* Check Flag for metadata REGC. */
	uint32_t shared_mark_enabled:1;
	/* If mark action is enabled on Rxqs (shared E-Switch domain). */
	uint32_t lag_rx_port_affinity_en:1;
	/* lag_rx_port_affinity is supported. */
	uint32_t hws_max_log_bulk_sz:5;
	/* Log of minimal HWS counters created hard coded. */
	uint32_t hws_max_nb_counters; /* Maximal number for HWS counters. */
	uint32_t max_port; /* Maximal IB device port index. */
	struct mlx5_bond_info bond; /* Bonding information. */
	struct mlx5_common_device *cdev; /* Backend mlx5 device. */
	uint32_t tdn; /* Transport Domain number. */
	char ibdev_name[MLX5_FS_NAME_MAX]; /* SYSFS dev name. */
	char ibdev_path[MLX5_FS_PATH_MAX]; /* SYSFS dev path for secondary */
	struct mlx5_dev_cap dev_cap; /* Device capabilities. */
	struct mlx5_sh_config config; /* Device configuration. */
	int numa_node; /* Numa node of backing physical device. */
	/* Packet pacing related structure. */
	struct mlx5_dev_txpp txpp;
	/* Shared DV/DR flow data section. */
	uint32_t dv_meta_mask; /* flow META metadata supported mask. */
	uint32_t dv_mark_mask; /* flow MARK metadata supported mask. */
	uint32_t dv_regc0_mask; /* available bits of metadata reg_c[0]. */
	void *fdb_domain; /* FDB Direct Rules name space handle. */
	void *rx_domain; /* RX Direct Rules name space handle. */
	void *tx_domain; /* TX Direct Rules name space handle. */
#ifndef RTE_ARCH_64
	rte_spinlock_t uar_lock_cq; /* CQs share a common distinct UAR. */
	rte_spinlock_t uar_lock[MLX5_UAR_PAGE_NUM_MAX];
	/* UAR same-page access control required in 32bit implementations. */
#endif
	union {
		struct mlx5_hlist *flow_tbls; /* SWS flow table. */
		struct mlx5_hlist *groups; /* HWS flow group. */
	};
	struct mlx5_flow_tunnel_hub *tunnel_hub;
	/* Direct Rules tables for FDB, NIC TX+RX */
	void *dr_drop_action; /* Pointer to DR drop action, any domain. */
	void *pop_vlan_action; /* Pointer to DR pop VLAN action. */
#if defined(HAVE_IBV_FLOW_DV_SUPPORT) || !defined(HAVE_INFINIBAND_VERBS_H)
	struct mlx5_send_to_kernel_action send_to_kernel_action[MLX5DR_TABLE_TYPE_MAX];
#endif
	struct mlx5_hlist *encaps_decaps; /* Encap/decap action hash list. */
	struct mlx5_hlist *modify_cmds;
	struct mlx5_hlist *tag_table;
	struct mlx5_list *port_id_action_list; /* Port ID action list. */
	struct mlx5_list *push_vlan_action_list; /* Push VLAN actions. */
	struct mlx5_list *sample_action_list; /* List of sample actions. */
	struct mlx5_list *dest_array_list;
	struct mlx5_list *flex_parsers_dv; /* Flex Item parsers. */
	/* List of destination array actions. */
	struct mlx5_flow_counter_mng sws_cmng;
	/* SW steering counters management structure. */
	void *default_miss_action; /* Default miss action. */
	struct mlx5_indexed_pool *ipool[MLX5_IPOOL_MAX];
	struct mlx5_indexed_pool *mdh_ipools[MLX5_MAX_MODIFY_NUM];
	/* Shared interrupt handler section. */
	struct rte_intr_handle *intr_handle; /* Interrupt handler for device. */
	struct rte_intr_handle *intr_handle_devx; /* DEVX interrupt handler. */
	struct rte_intr_handle *intr_handle_nl; /* Netlink interrupt handler. */
	void *devx_comp; /* DEVX async comp obj. */
	struct mlx5_devx_obj *tis[16]; /* TIS object. */
	struct mlx5_devx_obj *td; /* Transport domain. */
	struct mlx5_lag lag; /* LAG attributes */
	struct mlx5_uar tx_uar; /* DevX UAR for Tx and Txpp and ASO SQs. */
	struct mlx5_uar rx_uar; /* DevX UAR for Rx. */
	struct mlx5_proc_priv *pppriv; /* Pointer to primary private process. */
	struct mlx5_ecpri_parser_profile ecpri_parser;
	struct mlx5_internal_flex_parser_profile srh_flex_parser; /* srh flex parser structure. */
	/* Flex parser profiles information. */
	LIST_HEAD(shared_rxqs, mlx5_rxq_ctrl) shared_rxqs; /* Shared RXQs. */
	struct mlx5_aso_age_mng *aso_age_mng;
	/* Management data for aging mechanism using ASO Flow Hit. */
	struct mlx5_geneve_tlv_option_resource *geneve_tlv_option_resource;
	/* Management structure for geneve tlv option */
	rte_spinlock_t geneve_tlv_opt_sl; /* Lock for geneve tlv resource */
	struct mlx5_flow_mtr_mng *mtrmng;
	/* Meter management structure. */
	struct mlx5_aso_ct_pools_mng *ct_mng; /* Management data for ASO CT in HWS only. */
	struct mlx5_lb_ctx self_lb; /* QP to enable self loopback for Devx. */
	unsigned int flow_max_priority;
	enum modify_reg flow_mreg_c[MLX5_MREG_C_NUM];
	/* Availability of mreg_c's. */
	void *devx_channel_lwm;
	struct rte_intr_handle *intr_handle_lwm;
	pthread_mutex_t lwm_config_lock;
	uint32_t host_shaper_rate:8;
	uint32_t lwm_triggered:1;
	struct mlx5_hws_cnt_svc_mng *cnt_svc;
	rte_spinlock_t cpool_lock;
	LIST_HEAD(hws_cpool_list, mlx5_hws_cnt_pool) hws_cpool_list; /* Count pool list. */
	struct mlx5_dev_registers registers;
	struct mlx5_dev_shared_port port[]; /* per device port data array. */
};

/*
 * Per-process private structure.
 * Caution, secondary process may rebuild the struct during port start.
 */
struct mlx5_proc_priv {
	void *hca_bar;
	/* Mapped HCA PCI BAR area. */
	size_t uar_table_sz;
	/* Size of UAR register table. */
	struct mlx5_uar_data uar_table[];
	/* Table of UAR registers for each process. */
};

/* MTR profile list. */
TAILQ_HEAD(mlx5_mtr_profiles, mlx5_flow_meter_profile);
/* MTR list. */
TAILQ_HEAD(mlx5_legacy_flow_meters, mlx5_legacy_flow_meter);

struct mlx5_mtr_config {
	uint32_t nb_meters; /**< Number of configured meters */
	uint32_t nb_meter_profiles; /**< Number of configured meter profiles */
	uint32_t nb_meter_policies; /**< Number of configured meter policies */
};

/* RSS description. */
struct mlx5_flow_rss_desc {
	bool symmetric_hash_function; /**< Symmetric hash function */
	uint32_t level;
	uint32_t queue_num; /**< Number of entries in @p queue. */
	uint64_t types; /**< Specific RSS hash types (see RTE_ETH_RSS_*). */
	uint64_t hash_fields; /* Verbs Hash fields. */
	uint8_t key[MLX5_RSS_HASH_KEY_LEN]; /**< RSS hash key. */
	uint32_t key_len; /**< RSS hash key len. */
	uint32_t hws_flags; /**< HW steering action. */
	uint32_t tunnel; /**< Queue in tunnel. */
	uint32_t shared_rss; /**< Shared RSS index. */
	struct mlx5_ind_table_obj *ind_tbl;
	/**< Indirection table for shared RSS hash RX queues. */
	union {
		uint16_t *queue; /**< Destination queues. */
		const uint16_t *const_q; /**< Const pointer convert. */
	};
};

#define MLX5_PROC_PRIV(port_id) \
	((struct mlx5_proc_priv *)rte_eth_devices[port_id].process_private)

/* Verbs/DevX Rx queue elements. */
struct mlx5_rxq_obj {
	LIST_ENTRY(mlx5_rxq_obj) next; /* Pointer to the next element. */
	struct mlx5_rxq_ctrl *rxq_ctrl; /* Back pointer to parent. */
	int fd; /* File descriptor for event channel */
	union {
		struct {
			void *wq; /* Work Queue. */
			void *ibv_cq; /* Completion Queue. */
			void *ibv_channel;
		};
		struct mlx5_devx_obj *rq; /* DevX RQ object for hairpin. */
		struct {
			struct mlx5_devx_rmp devx_rmp; /* RMP for shared RQ. */
			struct mlx5_devx_cq cq_obj; /* DevX CQ object. */
			void *devx_channel;
		};
	};
};

/* Indirection table. */
struct mlx5_ind_table_obj {
	LIST_ENTRY(mlx5_ind_table_obj) next; /* Pointer to the next element. */
	uint32_t refcnt; /* Reference counter. */
	union {
		void *ind_table; /**< Indirection table. */
		struct mlx5_devx_obj *rqt; /* DevX RQT object. */
	};
	uint32_t queues_n; /**< Number of queues in the list. */
	uint16_t *queues; /**< Queue list. */
};

/* Hash Rx queue. */
__extension__
struct mlx5_hrxq {
	struct mlx5_list_entry entry; /* List entry. */
	uint32_t standalone:1; /* This object used in shared action. */
	struct mlx5_ind_table_obj *ind_table; /* Indirection table. */
	union {
		void *qp; /* Verbs queue pair. */
		struct mlx5_devx_obj *tir; /* DevX TIR object. */
	};
#if defined(HAVE_IBV_FLOW_DV_SUPPORT) || !defined(HAVE_INFINIBAND_VERBS_H)
	void *action; /* DV QP action pointer. */
#endif
	bool symmetric_hash_function; /* Symmetric hash function */
	uint32_t hws_flags; /* Hw steering flags. */
	uint64_t hash_fields; /* Verbs Hash fields. */
	uint32_t rss_key_len; /* Hash key length in bytes. */
	uint32_t idx; /* Hash Rx queue index. */
	uint8_t rss_key[]; /* Hash key. */
};

/* Verbs/DevX Tx queue elements. */
struct mlx5_txq_obj {
	LIST_ENTRY(mlx5_txq_obj) next; /* Pointer to the next element. */
	struct mlx5_txq_ctrl *txq_ctrl; /* Pointer to the control queue. */
	union {
		struct {
			void *cq; /* Completion Queue. */
			void *qp; /* Queue Pair. */
		};
		struct {
			struct mlx5_devx_obj *sq;
			/* DevX object for Sx queue. */
			struct mlx5_devx_obj *tis; /* The TIS object. */
			void *umem_buf_wq_buffer;
			void *umem_obj_wq_buffer;
		};
		struct {
			struct rte_eth_dev *dev;
			struct mlx5_devx_cq cq_obj;
			/* DevX CQ object and its resources. */
			struct mlx5_devx_sq sq_obj;
			/* DevX SQ object and its resources. */
		};
	};
};

enum mlx5_rxq_modify_type {
	MLX5_RXQ_MOD_ERR2RST, /* modify state from error to reset. */
	MLX5_RXQ_MOD_RST2RDY, /* modify state from reset to ready. */
	MLX5_RXQ_MOD_RDY2ERR, /* modify state from ready to error. */
	MLX5_RXQ_MOD_RDY2RST, /* modify state from ready to reset. */
	MLX5_RXQ_MOD_RDY2RDY, /* modify state from ready to ready. */
};

enum mlx5_txq_modify_type {
	MLX5_TXQ_MOD_RST2RDY, /* modify state from reset to ready. */
	MLX5_TXQ_MOD_RDY2RST, /* modify state from ready to reset. */
	MLX5_TXQ_MOD_ERR2RDY, /* modify state from error to ready. */
};

struct mlx5_rxq_priv;
struct mlx5_priv;

/* HW objects operations structure. */
struct mlx5_obj_ops {
	int (*rxq_obj_modify_vlan_strip)(struct mlx5_rxq_priv *rxq, int on);
	int (*rxq_obj_new)(struct mlx5_rxq_priv *rxq);
	int (*rxq_event_get)(struct mlx5_rxq_obj *rxq_obj);
	int (*rxq_obj_modify)(struct mlx5_rxq_priv *rxq, uint8_t type);
	void (*rxq_obj_release)(struct mlx5_rxq_priv *rxq);
	int (*rxq_event_get_lwm)(struct mlx5_priv *priv, int *rxq_idx, int *port_id);
	int (*ind_table_new)(struct rte_eth_dev *dev, const unsigned int log_n,
			     struct mlx5_ind_table_obj *ind_tbl);
	int (*ind_table_modify)(struct rte_eth_dev *dev,
				const unsigned int log_n,
				const uint16_t *queues, const uint32_t queues_n,
				struct mlx5_ind_table_obj *ind_tbl);
	void (*ind_table_destroy)(struct mlx5_ind_table_obj *ind_tbl);
	int (*hrxq_new)(struct rte_eth_dev *dev, struct mlx5_hrxq *hrxq,
			int tunnel __rte_unused);
	int (*hrxq_modify)(struct rte_eth_dev *dev, struct mlx5_hrxq *hrxq,
			   const uint8_t *rss_key,
			   uint64_t hash_fields,
			   bool symmetric_hash_function,
			   const struct mlx5_ind_table_obj *ind_tbl);
	void (*hrxq_destroy)(struct mlx5_hrxq *hrxq);
	int (*drop_action_create)(struct rte_eth_dev *dev);
	void (*drop_action_destroy)(struct rte_eth_dev *dev);
	int (*txq_obj_new)(struct rte_eth_dev *dev, uint16_t idx);
	int (*txq_obj_modify)(struct mlx5_txq_obj *obj,
			      enum mlx5_txq_modify_type type, uint8_t dev_port);
	void (*txq_obj_release)(struct mlx5_txq_obj *txq_obj);
	int (*lb_dummy_queue_create)(struct rte_eth_dev *dev);
	void (*lb_dummy_queue_release)(struct rte_eth_dev *dev);
};

#define MLX5_RSS_HASH_FIELDS_LEN RTE_DIM(mlx5_rss_hash_fields)

enum mlx5_hw_ctrl_flow_type {
	MLX5_HW_CTRL_FLOW_TYPE_GENERAL,
	MLX5_HW_CTRL_FLOW_TYPE_SQ_MISS_ROOT,
	MLX5_HW_CTRL_FLOW_TYPE_SQ_MISS,
	MLX5_HW_CTRL_FLOW_TYPE_DEFAULT_JUMP,
	MLX5_HW_CTRL_FLOW_TYPE_TX_META_COPY,
	MLX5_HW_CTRL_FLOW_TYPE_TX_REPR_MATCH,
	MLX5_HW_CTRL_FLOW_TYPE_LACP_RX,
	MLX5_HW_CTRL_FLOW_TYPE_DEFAULT_RX_RSS,
};

/** Additional info about control flow rule. */
struct mlx5_hw_ctrl_flow_info {
	/** Determines the kind of control flow rule. */
	enum mlx5_hw_ctrl_flow_type type;
	union {
		/**
		 * If control flow is a SQ miss flow (root or not),
		 * then fields contains matching SQ number.
		 */
		uint32_t esw_mgr_sq;
		/**
		 * If control flow is a Tx representor matching,
		 * then fields contains matching SQ number.
		 */
		uint32_t tx_repr_sq;
	};
};

/** Entry for tracking control flow rules in HWS. */
struct mlx5_hw_ctrl_flow {
	LIST_ENTRY(mlx5_hw_ctrl_flow) next;
	/**
	 * Owner device is a port on behalf of which flow rule was created.
	 *
	 * It's different from the port which really created the flow rule
	 * if and only if flow rule is created on transfer proxy port
	 * on behalf of representor port.
	 */
	struct rte_eth_dev *owner_dev;
	/** Pointer to flow rule handle. */
	struct rte_flow *flow;
	/** Additional information about the control flow rule. */
	struct mlx5_hw_ctrl_flow_info info;
};

/*
 * Flow rule structure for flow engine mode control, focus on group 0.
 * Apply to all supported domains.
 */
struct mlx5_dv_flow_info {
	LIST_ENTRY(mlx5_dv_flow_info) next;
	uint32_t orig_prio; /* prio set by user */
	uint32_t flow_idx_high_prio;
	/* flow index owned by standby mode. priority is lower unless DUP flags. */
	uint32_t flow_idx_low_prio;
	struct rte_flow_item *items;
	struct rte_flow_action *actions;
	struct rte_flow_attr attr;
};

struct rte_pmd_mlx5_flow_engine_mode_info {
	enum rte_pmd_mlx5_flow_engine_mode mode;
	uint32_t mode_flag;
	/* The list is maintained in insertion order. */
	LIST_HEAD(hot_up_info, mlx5_dv_flow_info) hot_upgrade;
};
/* HW Steering port configuration passed to rte_flow_configure(). */
struct mlx5_flow_hw_attr {
	struct rte_flow_port_attr port_attr;
	uint16_t nb_queue;
	struct rte_flow_queue_attr *queue_attr;
};

struct mlx5_flow_hw_ctrl_rx;

enum mlx5_quota_state {
	MLX5_QUOTA_STATE_FREE,	/* quota not in use */
	MLX5_QUOTA_STATE_READY, /* quota is ready   */
	MLX5_QUOTA_STATE_WAIT	/* quota waits WR completion */
};

struct mlx5_quota {
	uint8_t state; /* object state */
	uint8_t mode;  /* metering mode */
	/**
	 * Keep track of application update types.
	 * PMD does not allow 2 consecutive ADD updates.
	 */
	enum rte_flow_update_quota_op last_update;
};

/* Bulk management structure for flow quota. */
struct mlx5_quota_ctx {
	struct mlx5dr_action *dr_action; /* HWS action */
	struct mlx5_devx_obj *devx_obj; /* DEVX ranged object. */
	struct mlx5_pmd_mr mr; /* MR for READ from MTR ASO */
	struct mlx5_aso_mtr_dseg **read_buf; /* Buffers for READ */
	struct mlx5_aso_sq *sq; /* SQs for sync/async ACCESS_ASO WRs */
	struct mlx5_indexed_pool *quota_ipool; /* Manage quota objects */
};

struct mlx5_priv {
	struct rte_eth_dev_data *dev_data;  /* Pointer to device data. */
	struct mlx5_dev_ctx_shared *sh; /* Shared device context. */
	uint32_t dev_port; /* Device port number. */
	struct rte_pci_device *pci_dev; /* Backend PCI device. */
	struct rte_ether_addr mac[MLX5_MAX_MAC_ADDRESSES]; /* MAC addresses. */
	BITFIELD_DECLARE(mac_own, uint64_t, MLX5_MAX_MAC_ADDRESSES);
	/* Bit-field of MAC addresses owned by the PMD. */
	uint16_t vlan_filter[MLX5_MAX_VLAN_IDS]; /* VLAN filters table. */
	unsigned int vlan_filter_n; /* Number of configured VLAN filters. */
	/* Device properties. */
	uint16_t mtu; /* Configured MTU. */
	unsigned int isolated:1; /* Whether isolated mode is enabled. */
	unsigned int representor:1; /* Device is a port representor. */
	unsigned int master:1; /* Device is a E-Switch master. */
	unsigned int txpp_en:1; /* Tx packet pacing enabled. */
	unsigned int sampler_en:1; /* Whether support sampler. */
	unsigned int mtr_en:1; /* Whether support meter. */
	unsigned int mtr_reg_share:1; /* Whether support meter REG_C share. */
	unsigned int lb_used:1; /* Loopback queue is referred to. */
	unsigned int rmv_notified:1; /* Notified about removal event */
	uint32_t mark_enabled:1; /* If mark action is enabled on rxqs. */
	uint32_t num_lag_ports:4; /* Number of ports can be bonded. */
	uint32_t tunnel_enabled:1; /* If tunnel offloading is enabled on rxqs. */
	uint16_t domain_id; /* Switch domain identifier. */
	uint16_t vport_id; /* Associated VF vport index (if any). */
	uint32_t vport_meta_tag; /* Used for vport index match ove VF LAG. */
	uint32_t vport_meta_mask; /* Used for vport index field match mask. */
	uint16_t representor_id; /* UINT16_MAX if not a representor. */
	int32_t pf_bond; /* >=0, representor owner PF index in bonding. */
	int32_t mpesw_owner; /* >=0, representor owner PF index in MPESW. */
	int32_t mpesw_port; /* Related port index of MPESW device. < 0 - no MPESW. */
	bool mpesw_uplink; /* If true, port is an uplink port. */
	unsigned int if_index; /* Associated kernel network device index. */
	/* RX/TX queues. */
	unsigned int rxqs_n; /* RX queues array size. */
	unsigned int txqs_n; /* TX queues array size. */
	struct mlx5_external_rxq *ext_rxqs; /* External RX queues array. */
	struct mlx5_rxq_priv *(*rxq_privs)[]; /* RX queue non-shared data. */
	struct mlx5_txq_data *(*txqs)[]; /* TX queues. */
	struct rte_mempool *mprq_mp; /* Mempool for Multi-Packet RQ. */
	struct rte_eth_rss_conf rss_conf; /* RSS configuration. */
	unsigned int (*reta_idx)[]; /* RETA index table. */
	unsigned int reta_idx_n; /* RETA index size. */
	struct mlx5_drop drop_queue; /* Flow drop queues. */
	void *root_drop_action; /* Pointer to root drop action. */
	rte_spinlock_t hw_ctrl_lock;
	LIST_HEAD(hw_ctrl_flow, mlx5_hw_ctrl_flow) hw_ctrl_flows;
	LIST_HEAD(hw_ext_ctrl_flow, mlx5_hw_ctrl_flow) hw_ext_ctrl_flows;
	struct mlx5_flow_hw_ctrl_fdb *hw_ctrl_fdb;
	struct rte_flow_pattern_template *hw_tx_repr_tagging_pt;
	struct rte_flow_actions_template *hw_tx_repr_tagging_at;
	struct rte_flow_template_table *hw_tx_repr_tagging_tbl;
	struct mlx5_indexed_pool *flows[MLX5_FLOW_TYPE_MAXI];
	/* RTE Flow rules. */
	uint32_t ctrl_flows; /* Control flow rules. */
	rte_spinlock_t flow_list_lock;
	struct mlx5_obj_ops obj_ops; /* HW objects operations. */
	LIST_HEAD(rxq, mlx5_rxq_ctrl) rxqsctrl; /* DPDK Rx queues. */
	LIST_HEAD(rxqobj, mlx5_rxq_obj) rxqsobj; /* Verbs/DevX Rx queues. */
	struct mlx5_list *hrxqs; /* Hash Rx queues. */
	LIST_HEAD(txq, mlx5_txq_ctrl) txqsctrl; /* DPDK Tx queues. */
	LIST_HEAD(txqobj, mlx5_txq_obj) txqsobj; /* Verbs/DevX Tx queues. */
	/* Indirection tables. */
	LIST_HEAD(ind_tables, mlx5_ind_table_obj) ind_tbls;
	/* Standalone indirect tables. */
	LIST_HEAD(stdl_ind_tables, mlx5_ind_table_obj) standalone_ind_tbls;
	/* Objects created with indirect list action */
	LIST_HEAD(indirect_list, mlx5_indirect_list) indirect_list_head;
	/* Pointer to next element. */
	rte_rwlock_t ind_tbls_lock;
	uint32_t refcnt; /**< Reference counter. */
	/**< Verbs modify header action object. */
	uint8_t ft_type; /**< Flow table type, Rx or Tx. */
	uint32_t max_lro_msg_size;
	uint32_t link_speed_capa; /* Link speed capabilities. */
	struct mlx5_xstats_ctrl xstats_ctrl; /* Extended stats control. */
	struct mlx5_stats_ctrl stats_ctrl; /* Stats control. */
	struct mlx5_port_config config; /* Port configuration. */
	/* Context for Verbs allocator. */
	int nl_socket_rdma; /* Netlink socket (NETLINK_RDMA). */
	int nl_socket_route; /* Netlink socket (NETLINK_ROUTE). */
	struct mlx5_nl_vlan_vmwa_context *vmwa_context; /* VLAN WA context. */
	struct mlx5_hlist *mreg_cp_tbl;
	/* Hash table of Rx metadata register copy table. */
	struct mlx5_mtr_config mtr_config; /* Meter configuration */
	uint8_t mtr_sfx_reg; /* Meter prefix-suffix flow match REG_C. */
	struct mlx5_legacy_flow_meters flow_meters; /* MTR list. */
	struct mlx5_l3t_tbl *mtr_profile_tbl; /* Meter index lookup table. */
	struct mlx5_flow_meter_profile *mtr_profile_arr; /* Profile array. */
	struct mlx5_l3t_tbl *policy_idx_tbl; /* Policy index lookup table. */
	struct mlx5_flow_meter_policy *mtr_policy_arr; /* Policy array. */
	struct mlx5_l3t_tbl *mtr_idx_tbl; /* Meter index lookup table. */
	struct mlx5_mtr_bulk mtr_bulk; /* Meter index mapping for HWS */
	struct mlx5_quota_ctx quota_ctx; /* Quota index mapping for HWS */
	uint8_t skip_default_rss_reta; /* Skip configuration of default reta. */
	uint8_t fdb_def_rule; /* Whether fdb jump to table 1 is configured. */
	struct mlx5_mp_id mp_id; /* ID of a multi-process process */
	LIST_HEAD(fdir, mlx5_fdir_flow) fdir_flows; /* fdir flows. */
	rte_spinlock_t shared_act_sl; /* Shared actions spinlock. */
	uint32_t rss_shared_actions; /* RSS shared actions. */
	struct mlx5_devx_obj *q_counters; /* DevX queue counter object. */
	uint32_t counter_set_id; /* Queue counter ID to set in DevX objects. */
	uint32_t lag_affinity_idx; /* LAG mode queue 0 affinity starting. */
	rte_spinlock_t flex_item_sl; /* Flex item list spinlock. */
	struct mlx5_flex_item flex_item[MLX5_PORT_FLEX_ITEM_NUM];
	/* Flex items have been created on the port. */
	uint32_t flex_item_map; /* Map of allocated flex item elements. */
	uint32_t nb_queue; /* HW steering queue number. */
	struct mlx5_hws_cnt_pool *hws_cpool; /* HW steering's counter pool. */
	uint32_t hws_mark_refcnt; /* HWS mark action reference counter. */
	struct rte_pmd_mlx5_flow_engine_mode_info mode_info; /* Process set flow engine info. */
	struct mlx5_flow_hw_attr *hw_attr; /* HW Steering port configuration. */
#if defined(HAVE_IBV_FLOW_DV_SUPPORT) || !defined(HAVE_INFINIBAND_VERBS_H)
	/* Item template list. */
	LIST_HEAD(flow_hw_itt, rte_flow_pattern_template) flow_hw_itt;
	/* Action template list. */
	LIST_HEAD(flow_hw_at, rte_flow_actions_template) flow_hw_at;
	struct mlx5dr_context *dr_ctx; /**< HW steering DR context. */
	/* HW steering queue polling mechanism job descriptor LIFO. */
	uint32_t hws_strict_queue:1;
	/**< Whether all operations strictly happen on the same HWS queue. */
	uint32_t hws_age_req:1; /**< Whether this port has AGE indexed pool. */
	struct mlx5_hw_q *hw_q;
	/* HW steering rte flow table list header. */
	LIST_HEAD(flow_hw_tbl, rte_flow_template_table) flow_hw_tbl;
	/* HW steering rte flow group list header */
	LIST_HEAD(flow_hw_grp, mlx5_flow_group) flow_hw_grp;
	struct mlx5dr_action *hw_push_vlan[MLX5DR_TABLE_TYPE_MAX];
	struct mlx5dr_action *hw_pop_vlan[MLX5DR_TABLE_TYPE_MAX];
	struct mlx5dr_action **hw_vport;
	/* HW steering global drop action. */
	struct mlx5dr_action *hw_drop[2];
	/* HW steering global tag action. */
	struct mlx5dr_action *hw_tag[2];
	/* HW steering global default miss action. */
	struct mlx5dr_action *hw_def_miss;
	/* HW steering global send to kernel action. */
	struct mlx5dr_action *hw_send_to_kernel[MLX5DR_TABLE_TYPE_MAX];
	/* HW steering create ongoing rte flow table list header. */
	LIST_HEAD(flow_hw_tbl_ongo, rte_flow_template_table) flow_hw_tbl_ongo;
	struct mlx5_indexed_pool *acts_ipool; /* Action data indexed pool. */
	struct mlx5_aso_ct_pools_mng *ct_mng;
	/* Management data for ASO connection tracking. */
	struct mlx5_aso_ct_pool *hws_ctpool; /* HW steering's CT pool. */
	struct mlx5_aso_mtr_pool *hws_mpool; /* HW steering's Meter pool. */
	struct mlx5_flow_hw_ctrl_rx *hw_ctrl_rx;
	/**< HW steering templates used to create control flow rules. */
#endif
	struct rte_eth_dev *shared_host; /* Host device for HW steering. */
	uint16_t shared_refcnt; /* HW steering host reference counter. */
};

#define PORT_ID(priv) ((priv)->dev_data->port_id)
#define ETH_DEV(priv) (&rte_eth_devices[PORT_ID(priv)])
#define CTRL_QUEUE_ID(priv) ((priv)->nb_queue - 1)

struct rte_hairpin_peer_info {
	uint32_t qp_id;
	uint32_t vhca_id;
	uint16_t peer_q;
	uint16_t tx_explicit;
	uint16_t manual_bind;
};

#define BUF_SIZE 1024
enum dr_dump_rec_type {
	DR_DUMP_REC_TYPE_PMD_PKT_REFORMAT = 4410,
	DR_DUMP_REC_TYPE_PMD_MODIFY_HDR = 4420,
	DR_DUMP_REC_TYPE_PMD_COUNTER = 4430,
};

#if defined(HAVE_MLX5_HWS_SUPPORT)
static __rte_always_inline struct mlx5_hw_q_job *
flow_hw_job_get(struct mlx5_priv *priv, uint32_t queue)
{
	MLX5_ASSERT(priv->hw_q[queue].job_idx <= priv->hw_q[queue].size);
	return priv->hw_q[queue].job_idx ?
	       priv->hw_q[queue].job[--priv->hw_q[queue].job_idx] : NULL;
}

static __rte_always_inline void
flow_hw_job_put(struct mlx5_priv *priv, struct mlx5_hw_q_job *job, uint32_t queue)
{
	MLX5_ASSERT(priv->hw_q[queue].job_idx < priv->hw_q[queue].size);
	priv->hw_q[queue].job[priv->hw_q[queue].job_idx++] = job;
}

struct mlx5_hw_q_job *
mlx5_flow_action_job_init(struct mlx5_priv *priv, uint32_t queue,
			  const struct rte_flow_action_handle *handle,
			  void *user_data, void *query_data,
			  enum mlx5_hw_job_type type,
			  struct rte_flow_error *error);
#endif

/**
 * Indicates whether HW objects operations can be created by DevX.
 *
 * This function is used for both:
 *  Before creation - deciding whether to create HW objects operations by DevX.
 *  After creation - indicator if HW objects operations were created by DevX.
 *
 * @param sh
 *   Pointer to shared device context.
 *
 * @return
 *   True if HW objects were created by DevX, False otherwise.
 */
static inline bool
mlx5_devx_obj_ops_en(struct mlx5_dev_ctx_shared *sh)
{
	/*
	 * When advanced DR API is available and DV flow is supported and
	 * DevX is supported, HW objects operations are created by DevX.
	 */
	return (sh->cdev->config.devx && sh->config.dv_flow_en &&
		sh->dev_cap.dest_tir);
}

/**
 * Check if the port is either MPESW physical device or a representor port.
 *
 * @param priv
 *   Pointer to port's private data.
 *
 * @return
 *   True if the port is a physical device or representor in MPESW setup.
 *   False otherwise or MPESW was not configured.
 */
static inline bool
mlx5_is_port_on_mpesw_device(struct mlx5_priv *priv)
{
	return priv->mpesw_port >= 0;
}

/* mlx5.c */

int mlx5_getenv_int(const char *);
int mlx5_proc_priv_init(struct rte_eth_dev *dev);
void mlx5_proc_priv_uninit(struct rte_eth_dev *dev);
int mlx5_udp_tunnel_port_add(struct rte_eth_dev *dev,
			      struct rte_eth_udp_tunnel *udp_tunnel);
uint16_t mlx5_eth_find_next(uint16_t port_id, struct rte_device *odev);
int mlx5_dev_close(struct rte_eth_dev *dev);
int mlx5_net_remove(struct mlx5_common_device *cdev);
bool mlx5_is_hpf(struct rte_eth_dev *dev);
bool mlx5_is_sf_repr(struct rte_eth_dev *dev);
void mlx5_age_event_prepare(struct mlx5_dev_ctx_shared *sh);
int mlx5_lwm_setup(struct mlx5_priv *priv);
void mlx5_lwm_unset(struct mlx5_dev_ctx_shared *sh);

/* Macro to iterate over all valid ports for mlx5 driver. */
#define MLX5_ETH_FOREACH_DEV(port_id, dev) \
	for (port_id = mlx5_eth_find_next(0, dev); \
	     port_id < RTE_MAX_ETHPORTS; \
	     port_id = mlx5_eth_find_next(port_id + 1, dev))
void mlx5_rt_timestamp_config(struct mlx5_dev_ctx_shared *sh,
			      struct mlx5_hca_attr *hca_attr);
struct mlx5_dev_ctx_shared *
mlx5_alloc_shared_dev_ctx(const struct mlx5_dev_spawn_data *spawn,
			  struct mlx5_kvargs_ctrl *mkvlist);
void mlx5_free_shared_dev_ctx(struct mlx5_dev_ctx_shared *sh);
int mlx5_dev_ctx_shared_mempool_subscribe(struct rte_eth_dev *dev);
void mlx5_free_table_hash_list(struct mlx5_priv *priv);
int mlx5_alloc_table_hash_list(struct mlx5_priv *priv);
void mlx5_set_min_inline(struct mlx5_priv *priv);
void mlx5_set_metadata_mask(struct rte_eth_dev *dev);
int mlx5_probe_again_args_validate(struct mlx5_common_device *cdev,
				   struct mlx5_kvargs_ctrl *mkvlist);
int mlx5_port_args_config(struct mlx5_priv *priv,
			  struct mlx5_kvargs_ctrl *mkvlist,
			  struct mlx5_port_config *config);
void mlx5_port_args_set_used(const char *name, uint16_t port_id,
			     struct mlx5_kvargs_ctrl *mkvlist);
bool mlx5_flex_parser_ecpri_exist(struct rte_eth_dev *dev);
int mlx5_flex_parser_ecpri_alloc(struct rte_eth_dev *dev);
void mlx5_flow_counter_mode_config(struct rte_eth_dev *dev);
int mlx5_flow_aso_age_mng_init(struct mlx5_dev_ctx_shared *sh);
int mlx5_aso_flow_mtrs_mng_init(struct mlx5_dev_ctx_shared *sh);
int mlx5_flow_aso_ct_mng_init(struct mlx5_dev_ctx_shared *sh);

/* mlx5_ethdev.c */

int mlx5_dev_configure(struct rte_eth_dev *dev);
int mlx5_representor_info_get(struct rte_eth_dev *dev,
			      struct rte_eth_representor_info *info);
#define MLX5_REPRESENTOR_ID(pf, type, repr) \
		(((pf) << 14) + ((type) << 12) + ((repr) & 0xfff))
#define MLX5_REPRESENTOR_REPR(repr_id) \
		((repr_id) & 0xfff)
#define MLX5_REPRESENTOR_TYPE(repr_id) \
		(((repr_id) >> 12) & 3)
uint16_t mlx5_representor_id_encode(const struct mlx5_switch_info *info,
				    enum rte_eth_representor_type hpf_type);
int mlx5_dev_infos_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *info);
int mlx5_fw_version_get(struct rte_eth_dev *dev, char *fw_ver, size_t fw_size);
const uint32_t *mlx5_dev_supported_ptypes_get(struct rte_eth_dev *dev);
int mlx5_dev_set_mtu(struct rte_eth_dev *dev, uint16_t mtu);
int mlx5_hairpin_cap_get(struct rte_eth_dev *dev,
			 struct rte_eth_hairpin_cap *cap);
eth_rx_burst_t mlx5_select_rx_function(struct rte_eth_dev *dev);
struct mlx5_priv *mlx5_port_to_eswitch_info(uint16_t port, bool valid);
struct mlx5_priv *mlx5_dev_to_eswitch_info(struct rte_eth_dev *dev);
int mlx5_dev_configure_rss_reta(struct rte_eth_dev *dev);

/* mlx5_ethdev_os.c */

int mlx5_get_ifname(const struct rte_eth_dev *dev,
			char (*ifname)[MLX5_NAMESIZE]);
unsigned int mlx5_ifindex(const struct rte_eth_dev *dev);
int mlx5_get_mac(struct rte_eth_dev *dev, uint8_t (*mac)[RTE_ETHER_ADDR_LEN]);
int mlx5_get_mtu(struct rte_eth_dev *dev, uint16_t *mtu);
int mlx5_set_mtu(struct rte_eth_dev *dev, uint16_t mtu);
int mlx5_read_clock(struct rte_eth_dev *dev, uint64_t *clock);
int mlx5_link_update(struct rte_eth_dev *dev, int wait_to_complete);
int mlx5_dev_get_flow_ctrl(struct rte_eth_dev *dev,
			   struct rte_eth_fc_conf *fc_conf);
int mlx5_dev_set_flow_ctrl(struct rte_eth_dev *dev,
			   struct rte_eth_fc_conf *fc_conf);
void mlx5_dev_interrupt_handler(void *arg);
void mlx5_dev_interrupt_handler_devx(void *arg);
void mlx5_dev_interrupt_handler_nl(void *arg);
int mlx5_set_link_down(struct rte_eth_dev *dev);
int mlx5_set_link_up(struct rte_eth_dev *dev);
int mlx5_is_removed(struct rte_eth_dev *dev);
int mlx5_sysfs_switch_info(unsigned int ifindex,
			   struct mlx5_switch_info *info);
void mlx5_translate_port_name(const char *port_name_in,
			      struct mlx5_switch_info *port_info_out);
int mlx5_sysfs_bond_info(unsigned int pf_ifindex, unsigned int *ifindex,
			 char *ifname);
int mlx5_get_module_info(struct rte_eth_dev *dev,
			 struct rte_eth_dev_module_info *modinfo);
int mlx5_get_module_eeprom(struct rte_eth_dev *dev,
			   struct rte_dev_eeprom_info *info);
int mlx5_os_read_dev_stat(struct mlx5_priv *priv,
			  const char *ctr_name, uint64_t *stat);
int mlx5_os_read_dev_counters(struct rte_eth_dev *dev, bool bond_master, uint64_t *stats);
int mlx5_os_get_stats_n(struct rte_eth_dev *dev, bool bond_master,
			uint16_t *n_stats, uint16_t *n_stats_sec);
void mlx5_os_stats_init(struct rte_eth_dev *dev);
int mlx5_get_flag_dropless_rq(struct rte_eth_dev *dev);

/* mlx5_mac.c */

void mlx5_mac_addr_remove(struct rte_eth_dev *dev, uint32_t index);
int mlx5_mac_addr_add(struct rte_eth_dev *dev, struct rte_ether_addr *mac,
		      uint32_t index, uint32_t vmdq);
int mlx5_mac_addr_set(struct rte_eth_dev *dev, struct rte_ether_addr *mac_addr);
int mlx5_set_mc_addr_list(struct rte_eth_dev *dev,
			struct rte_ether_addr *mc_addr_set,
			uint32_t nb_mc_addr);

/* mlx5_rss.c */

int mlx5_rss_hash_update(struct rte_eth_dev *dev,
			 struct rte_eth_rss_conf *rss_conf);
int mlx5_rss_hash_conf_get(struct rte_eth_dev *dev,
			   struct rte_eth_rss_conf *rss_conf);
int mlx5_rss_reta_index_resize(struct rte_eth_dev *dev, unsigned int reta_size);
int mlx5_dev_rss_reta_query(struct rte_eth_dev *dev,
			    struct rte_eth_rss_reta_entry64 *reta_conf,
			    uint16_t reta_size);
int mlx5_dev_rss_reta_update(struct rte_eth_dev *dev,
			     struct rte_eth_rss_reta_entry64 *reta_conf,
			     uint16_t reta_size);

/* mlx5_rxmode.c */

int mlx5_promiscuous_enable(struct rte_eth_dev *dev);
int mlx5_promiscuous_disable(struct rte_eth_dev *dev);
int mlx5_allmulticast_enable(struct rte_eth_dev *dev);
int mlx5_allmulticast_disable(struct rte_eth_dev *dev);

/* mlx5_stats.c */

int mlx5_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats);
int mlx5_stats_reset(struct rte_eth_dev *dev);
int mlx5_xstats_get(struct rte_eth_dev *dev, struct rte_eth_xstat *stats,
		    unsigned int n);
int mlx5_xstats_reset(struct rte_eth_dev *dev);
int mlx5_xstats_get_names(struct rte_eth_dev *dev __rte_unused,
			  struct rte_eth_xstat_name *xstats_names,
			  unsigned int n);

/* mlx5_vlan.c */

int mlx5_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vlan_id, int on);
void mlx5_vlan_strip_queue_set(struct rte_eth_dev *dev, uint16_t queue, int on);
int mlx5_vlan_offload_set(struct rte_eth_dev *dev, int mask);

/* mlx5_vlan_os.c */

void mlx5_vlan_vmwa_exit(void *ctx);
void mlx5_vlan_vmwa_release(struct rte_eth_dev *dev,
			    struct mlx5_vf_vlan *vf_vlan);
void mlx5_vlan_vmwa_acquire(struct rte_eth_dev *dev,
			    struct mlx5_vf_vlan *vf_vlan);
void *mlx5_vlan_vmwa_init(struct rte_eth_dev *dev, uint32_t ifindex);

/* mlx5_trigger.c */

int mlx5_dev_start(struct rte_eth_dev *dev);
int mlx5_dev_stop(struct rte_eth_dev *dev);
int mlx5_traffic_enable(struct rte_eth_dev *dev);
void mlx5_traffic_disable(struct rte_eth_dev *dev);
int mlx5_traffic_restart(struct rte_eth_dev *dev);
int mlx5_hairpin_queue_peer_update(struct rte_eth_dev *dev, uint16_t peer_queue,
				   struct rte_hairpin_peer_info *current_info,
				   struct rte_hairpin_peer_info *peer_info,
				   uint32_t direction);
int mlx5_hairpin_queue_peer_bind(struct rte_eth_dev *dev, uint16_t cur_queue,
				 struct rte_hairpin_peer_info *peer_info,
				 uint32_t direction);
int mlx5_hairpin_queue_peer_unbind(struct rte_eth_dev *dev, uint16_t cur_queue,
				   uint32_t direction);
int mlx5_hairpin_bind(struct rte_eth_dev *dev, uint16_t rx_port);
int mlx5_hairpin_unbind(struct rte_eth_dev *dev, uint16_t rx_port);
int mlx5_hairpin_get_peer_ports(struct rte_eth_dev *dev, uint16_t *peer_ports,
				size_t len, uint32_t direction);

/* mlx5_flow.c */

int mlx5_flow_discover_mreg_c(struct rte_eth_dev *eth_dev);
bool mlx5_flow_ext_mreg_supported(struct rte_eth_dev *dev);
void mlx5_flow_print(struct rte_flow *flow);
int mlx5_flow_validate(struct rte_eth_dev *dev,
		       const struct rte_flow_attr *attr,
		       const struct rte_flow_item items[],
		       const struct rte_flow_action actions[],
		       struct rte_flow_error *error);
struct rte_flow *mlx5_flow_create(struct rte_eth_dev *dev,
				  const struct rte_flow_attr *attr,
				  const struct rte_flow_item items[],
				  const struct rte_flow_action actions[],
				  struct rte_flow_error *error);
int mlx5_flow_destroy(struct rte_eth_dev *dev, struct rte_flow *flow,
		      struct rte_flow_error *error);
void mlx5_flow_list_flush(struct rte_eth_dev *dev, enum mlx5_flow_type type,
			  bool active);
int mlx5_flow_flush(struct rte_eth_dev *dev, struct rte_flow_error *error);
int mlx5_flow_query(struct rte_eth_dev *dev, struct rte_flow *flow,
		    const struct rte_flow_action *action, void *data,
		    struct rte_flow_error *error);
int mlx5_flow_isolate(struct rte_eth_dev *dev, int enable,
		      struct rte_flow_error *error);
int mlx5_flow_ops_get(struct rte_eth_dev *dev, const struct rte_flow_ops **ops);
int mlx5_flow_start_default(struct rte_eth_dev *dev);
void mlx5_flow_stop_default(struct rte_eth_dev *dev);
int mlx5_flow_verify(struct rte_eth_dev *dev);
int mlx5_ctrl_flow_source_queue(struct rte_eth_dev *dev, uint32_t sq_num);
int mlx5_ctrl_flow_vlan(struct rte_eth_dev *dev,
			struct rte_flow_item_eth *eth_spec,
			struct rte_flow_item_eth *eth_mask,
			struct rte_flow_item_vlan *vlan_spec,
			struct rte_flow_item_vlan *vlan_mask);
int mlx5_ctrl_flow(struct rte_eth_dev *dev,
		   struct rte_flow_item_eth *eth_spec,
		   struct rte_flow_item_eth *eth_mask);
int mlx5_flow_lacp_miss(struct rte_eth_dev *dev);
struct rte_flow *mlx5_flow_create_esw_table_zero_flow(struct rte_eth_dev *dev);
uint32_t mlx5_flow_create_devx_sq_miss_flow(struct rte_eth_dev *dev,
					    uint32_t sq_num);
void mlx5_flow_async_pool_query_handle(struct mlx5_dev_ctx_shared *sh,
				       uint64_t async_id, int status);
void mlx5_set_query_alarm(struct mlx5_dev_ctx_shared *sh);
void mlx5_flow_query_alarm(void *arg);
uint32_t mlx5_counter_alloc(struct rte_eth_dev *dev);
void mlx5_counter_free(struct rte_eth_dev *dev, uint32_t cnt);
int mlx5_counter_query(struct rte_eth_dev *dev, uint32_t cnt,
		    bool clear, uint64_t *pkts, uint64_t *bytes, void **action);
int mlx5_flow_dev_dump(struct rte_eth_dev *dev, struct rte_flow *flow,
			FILE *file, struct rte_flow_error *error);
int save_dump_file(const unsigned char *data, uint32_t size,
		uint32_t type, uint64_t id, void *arg, FILE *file);
int mlx5_flow_query_counter(struct rte_eth_dev *dev, struct rte_flow *flow,
	struct rte_flow_query_count *count, struct rte_flow_error *error);
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
int mlx5_flow_dev_dump_ipool(struct rte_eth_dev *dev, struct rte_flow *flow,
		FILE *file, struct rte_flow_error *error);
#endif
int mlx5_flow_rx_metadata_negotiate(struct rte_eth_dev *dev,
	uint64_t *features);
void mlx5_flow_rxq_dynf_set(struct rte_eth_dev *dev);
int mlx5_flow_get_aged_flows(struct rte_eth_dev *dev, void **contexts,
			uint32_t nb_contexts, struct rte_flow_error *error);
int mlx5_validate_action_ct(struct rte_eth_dev *dev,
			    const struct rte_flow_action_conntrack *conntrack,
			    struct rte_flow_error *error);

int mlx5_flow_get_q_aged_flows(struct rte_eth_dev *dev, uint32_t queue_id,
			       void **contexts, uint32_t nb_contexts,
			       struct rte_flow_error *error);

/* mlx5_mp_os.c */

int mlx5_mp_os_primary_handle(const struct rte_mp_msg *mp_msg,
			      const void *peer);
int mlx5_mp_os_secondary_handle(const struct rte_mp_msg *mp_msg,
				const void *peer);
void mlx5_mp_os_req_start_rxtx(struct rte_eth_dev *dev);
void mlx5_mp_os_req_stop_rxtx(struct rte_eth_dev *dev);
int mlx5_mp_os_req_queue_control(struct rte_eth_dev *dev, uint16_t queue_id,
				 enum mlx5_mp_req_type req_type);

/* mlx5_socket.c */

int mlx5_pmd_socket_init(void);
void mlx5_pmd_socket_uninit(void);

/* mlx5_flow_meter.c */

int mlx5_flow_meter_init(struct rte_eth_dev *dev,
			 uint32_t nb_meters,
			 uint32_t nb_meter_profiles,
			 uint32_t nb_meter_policies,
			 uint32_t nb_queues);
void mlx5_flow_meter_uninit(struct rte_eth_dev *dev);
int mlx5_flow_meter_ops_get(struct rte_eth_dev *dev, void *arg);
struct mlx5_flow_meter_info *mlx5_flow_meter_find(struct mlx5_priv *priv,
		uint32_t meter_id, uint32_t *mtr_idx);
struct mlx5_flow_meter_info *
flow_dv_meter_find_by_idx(struct mlx5_priv *priv, uint32_t idx);
int mlx5_flow_meter_attach(struct mlx5_priv *priv,
			   struct mlx5_flow_meter_info *fm,
			   const struct rte_flow_attr *attr,
			   struct rte_flow_error *error);
void mlx5_flow_meter_detach(struct mlx5_priv *priv,
			    struct mlx5_flow_meter_info *fm);
struct mlx5_flow_meter_policy *mlx5_flow_meter_policy_find
		(struct rte_eth_dev *dev,
		uint32_t policy_id,
		uint32_t *policy_idx);
struct mlx5_flow_meter_info *
mlx5_flow_meter_hierarchy_next_meter(struct mlx5_priv *priv,
				     struct mlx5_flow_meter_policy *policy,
				     uint32_t *mtr_idx);
struct mlx5_flow_meter_policy *
mlx5_flow_meter_hierarchy_get_final_policy(struct rte_eth_dev *dev,
					struct mlx5_flow_meter_policy *policy);
int mlx5_flow_meter_flush(struct rte_eth_dev *dev,
			  struct rte_mtr_error *error);
void mlx5_flow_meter_rxq_flush(struct rte_eth_dev *dev);

/* mlx5_os.c */

struct rte_pci_driver;
int mlx5_os_capabilities_prepare(struct mlx5_dev_ctx_shared *sh);
void mlx5_os_free_shared_dr(struct mlx5_priv *priv);
int mlx5_os_net_probe(struct mlx5_common_device *cdev,
		      struct mlx5_kvargs_ctrl *mkvlist);
void mlx5_os_dev_shared_handler_install(struct mlx5_dev_ctx_shared *sh);
void mlx5_os_dev_shared_handler_uninstall(struct mlx5_dev_ctx_shared *sh);
void mlx5_os_mac_addr_remove(struct rte_eth_dev *dev, uint32_t index);
int mlx5_os_mac_addr_add(struct rte_eth_dev *dev, struct rte_ether_addr *mac,
			 uint32_t index);
int mlx5_os_vf_mac_addr_modify(struct mlx5_priv *priv, unsigned int iface_idx,
			       struct rte_ether_addr *mac_addr,
			       int vf_index);
int mlx5_os_set_promisc(struct rte_eth_dev *dev, int enable);
int mlx5_os_set_allmulti(struct rte_eth_dev *dev, int enable);
int mlx5_os_set_nonblock_channel_fd(int fd);
void mlx5_os_mac_addr_flush(struct rte_eth_dev *dev);
void mlx5_os_net_cleanup(void);

/* mlx5_txpp.c */

int mlx5_txpp_start(struct rte_eth_dev *dev);
void mlx5_txpp_stop(struct rte_eth_dev *dev);
int mlx5_txpp_read_clock(struct rte_eth_dev *dev, uint64_t *timestamp);
int mlx5_txpp_xstats_get(struct rte_eth_dev *dev,
			 struct rte_eth_xstat *stats,
			 unsigned int n, unsigned int n_used);
int mlx5_txpp_xstats_reset(struct rte_eth_dev *dev);
int mlx5_txpp_xstats_get_names(struct rte_eth_dev *dev,
			       struct rte_eth_xstat_name *xstats_names,
			       unsigned int n, unsigned int n_used);
void mlx5_txpp_interrupt_handler(void *cb_arg);
int mlx5_txpp_map_hca_bar(struct rte_eth_dev *dev);
void mlx5_txpp_unmap_hca_bar(struct rte_eth_dev *dev);

/* mlx5_rxtx.c */

eth_tx_burst_t mlx5_select_tx_function(struct rte_eth_dev *dev);

/* mlx5_flow_aso.c */

int mlx5_aso_mtr_queue_init(struct mlx5_dev_ctx_shared *sh,
			    struct mlx5_aso_mtr_pool *hws_pool,
			    struct mlx5_aso_mtr_pools_mng *pool_mng,
			    uint32_t nb_queues);
void mlx5_aso_mtr_queue_uninit(struct mlx5_dev_ctx_shared *sh,
			       struct mlx5_aso_mtr_pool *hws_pool,
			       struct mlx5_aso_mtr_pools_mng *pool_mng);
int mlx5_aso_queue_init(struct mlx5_dev_ctx_shared *sh,
			enum mlx5_access_aso_opc_mod aso_opc_mode,
			uint32_t nb_queues);
int mlx5_aso_flow_hit_queue_poll_start(struct mlx5_dev_ctx_shared *sh);
int mlx5_aso_flow_hit_queue_poll_stop(struct mlx5_dev_ctx_shared *sh);
void mlx5_aso_queue_uninit(struct mlx5_dev_ctx_shared *sh,
			   enum mlx5_access_aso_opc_mod aso_opc_mod);
int mlx5_aso_meter_update_by_wqe(struct mlx5_priv *priv, uint32_t queue,
				 struct mlx5_aso_mtr *mtr,
				 struct mlx5_mtr_bulk *bulk,
				 struct mlx5_hw_q_job *job, bool push);
int mlx5_aso_mtr_wait(struct mlx5_priv *priv,
		      struct mlx5_aso_mtr *mtr, bool is_tmpl_api);
int mlx5_aso_ct_update_by_wqe(struct mlx5_dev_ctx_shared *sh, uint32_t queue,
			      struct mlx5_aso_ct_action *ct,
			      const struct rte_flow_action_conntrack *profile,
			      void *user_data,
			      bool push);
int mlx5_aso_ct_wait_ready(struct mlx5_dev_ctx_shared *sh, uint32_t queue,
			   struct mlx5_aso_ct_action *ct);
int mlx5_aso_ct_query_by_wqe(struct mlx5_dev_ctx_shared *sh, uint32_t queue,
			     struct mlx5_aso_ct_action *ct,
			     struct rte_flow_action_conntrack *profile,
			     void *user_data, bool push);
int mlx5_aso_ct_available(struct mlx5_dev_ctx_shared *sh, uint32_t queue,
			  struct mlx5_aso_ct_action *ct);
uint32_t
mlx5_get_supported_sw_parsing_offloads(const struct mlx5_hca_attr *attr);
uint32_t
mlx5_get_supported_tunneling_offloads(const struct mlx5_hca_attr *attr);

void mlx5_aso_ct_obj_analyze(struct rte_flow_action_conntrack *profile,
			     char *wdata);
void mlx5_aso_push_wqe(struct mlx5_dev_ctx_shared *sh,
		       struct mlx5_aso_sq *sq);
int mlx5_aso_pull_completion(struct mlx5_aso_sq *sq,
			     struct rte_flow_op_result res[],
			     uint16_t n_res);
int mlx5_aso_cnt_queue_init(struct mlx5_dev_ctx_shared *sh);
void mlx5_aso_cnt_queue_uninit(struct mlx5_dev_ctx_shared *sh);
int mlx5_aso_cnt_query(struct mlx5_dev_ctx_shared *sh,
		struct mlx5_hws_cnt_pool *cpool);
int mlx5_aso_ct_queue_init(struct mlx5_dev_ctx_shared *sh,
			   struct mlx5_aso_ct_pools_mng *ct_mng,
			   uint32_t nb_queues);
int mlx5_aso_ct_queue_uninit(struct mlx5_dev_ctx_shared *sh,
			     struct mlx5_aso_ct_pools_mng *ct_mng);
int
mlx5_aso_sq_create(struct mlx5_common_device *cdev, struct mlx5_aso_sq *sq,
		   void *uar, uint16_t log_desc_n);
void
mlx5_aso_destroy_sq(struct mlx5_aso_sq *sq);
void
mlx5_aso_mtr_init_sq(struct mlx5_aso_sq *sq);
void
mlx5_aso_cqe_err_handle(struct mlx5_aso_sq *sq);

/* mlx5_flow_flex.c */

struct rte_flow_item_flex_handle *
flow_dv_item_create(struct rte_eth_dev *dev,
		    const struct rte_flow_item_flex_conf *conf,
		    struct rte_flow_error *error);
int flow_dv_item_release(struct rte_eth_dev *dev,
		    const struct rte_flow_item_flex_handle *flex_handle,
		    struct rte_flow_error *error);
int mlx5_flex_item_port_init(struct rte_eth_dev *dev);
void mlx5_flex_item_port_cleanup(struct rte_eth_dev *dev);
void mlx5_flex_flow_translate_item(struct rte_eth_dev *dev, void *matcher,
				   void *key, const struct rte_flow_item *item,
				   bool is_inner);
int mlx5_flex_get_sample_id(const struct mlx5_flex_item *tp,
			    uint32_t idx, uint32_t *pos,
			    bool is_inner, uint32_t *def);
int mlx5_flex_get_parser_value_per_byte_off(const struct rte_flow_item_flex *item,
					    void *flex, uint32_t byte_off,
					    bool is_mask, bool tunnel, uint32_t *value);
int mlx5_flex_acquire_index(struct rte_eth_dev *dev,
			    struct rte_flow_item_flex_handle *handle,
			    bool acquire);
int mlx5_flex_release_index(struct rte_eth_dev *dev, int index);

/* Flex parser list callbacks. */
struct mlx5_list_entry *mlx5_flex_parser_create_cb(void *list_ctx, void *ctx);
int mlx5_flex_parser_match_cb(void *list_ctx,
			      struct mlx5_list_entry *iter, void *ctx);
void mlx5_flex_parser_remove_cb(void *list_ctx,	struct mlx5_list_entry *entry);
struct mlx5_list_entry *mlx5_flex_parser_clone_cb(void *list_ctx,
						  struct mlx5_list_entry *entry,
						  void *ctx);
void mlx5_flex_parser_clone_free_cb(void *tool_ctx,
				    struct mlx5_list_entry *entry);

int
mlx5_flow_quota_destroy(struct rte_eth_dev *dev);
int
mlx5_flow_quota_init(struct rte_eth_dev *dev, uint32_t nb_quotas);
struct rte_flow_action_handle *
mlx5_quota_alloc(struct rte_eth_dev *dev, uint32_t queue,
		 const struct rte_flow_action_quota *conf,
		 struct mlx5_hw_q_job *job, bool push,
		 struct rte_flow_error *error);
void
mlx5_quota_async_completion(struct rte_eth_dev *dev, uint32_t queue,
			    struct mlx5_hw_q_job *job);
int
mlx5_quota_query_update(struct rte_eth_dev *dev, uint32_t queue,
			struct rte_flow_action_handle *handle,
			const struct rte_flow_action *update,
			struct rte_flow_query_quota *query,
			struct mlx5_hw_q_job *async_job, bool push,
			struct rte_flow_error *error);
int mlx5_quota_query(struct rte_eth_dev *dev, uint32_t queue,
		     const struct rte_flow_action_handle *handle,
		     struct rte_flow_query_quota *query,
		     struct mlx5_hw_q_job *async_job, bool push,
		     struct rte_flow_error *error);

int mlx5_alloc_srh_flex_parser(struct rte_eth_dev *dev);

void mlx5_free_srh_flex_parser(struct rte_eth_dev *dev);
#endif /* RTE_PMD_MLX5_H_ */
