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
#include <net/if.h>
#include <netinet/in.h>
#include <sys/queue.h>

#include <rte_pci.h>
#include <rte_ether.h>
#include <rte_ethdev_driver.h>
#include <rte_rwlock.h>
#include <rte_interrupts.h>
#include <rte_errno.h>
#include <rte_flow.h>

#include <mlx5_glue.h>
#include <mlx5_devx_cmds.h>
#include <mlx5_prm.h>
#include <mlx5_nl.h>
#include <mlx5_common_mp.h>
#include <mlx5_common_mr.h>

#include "mlx5_defs.h"
#include "mlx5_utils.h"
#include "mlx5_os.h"
#include "mlx5_autoconf.h"

enum mlx5_ipool_index {
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	MLX5_IPOOL_DECAP_ENCAP = 0, /* Pool for encap/decap resource. */
	MLX5_IPOOL_PUSH_VLAN, /* Pool for push vlan resource. */
	MLX5_IPOOL_TAG, /* Pool for tag resource. */
	MLX5_IPOOL_PORT_ID, /* Pool for port id resource. */
	MLX5_IPOOL_JUMP, /* Pool for jump resource. */
#endif
	MLX5_IPOOL_MTR, /* Pool for meter resource. */
	MLX5_IPOOL_MCP, /* Pool for metadata resource. */
	MLX5_IPOOL_HRXQ, /* Pool for hrxq resource. */
	MLX5_IPOOL_MLX5_FLOW, /* Pool for mlx5 flow handle. */
	MLX5_IPOOL_RTE_FLOW, /* Pool for rte_flow. */
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

/* Device attributes used in mlx5 PMD */
struct mlx5_dev_attr {
	uint64_t	device_cap_flags_ex;
	int		max_qp_wr;
	int		max_sge;
	int		max_cq;
	int		max_qp;
	uint32_t	raw_packet_caps;
	uint32_t	max_rwq_indirection_table_size;
	uint32_t	max_tso;
	uint32_t	tso_supported_qpts;
	uint64_t	flags;
	uint64_t	comp_mask;
	uint32_t	sw_parsing_offloads;
	uint32_t	min_single_stride_log_num_of_bytes;
	uint32_t	max_single_stride_log_num_of_bytes;
	uint32_t	min_single_wqe_log_num_of_strides;
	uint32_t	max_single_wqe_log_num_of_strides;
	uint32_t	stride_supported_qpts;
	uint32_t	tunnel_offloads_caps;
	char		fw_ver[64];
};

/** Data associated with devices to spawn. */
struct mlx5_dev_spawn_data {
	uint32_t ifindex; /**< Network interface index. */
	uint32_t max_port; /**< Device maximal port index. */
	uint32_t phys_port; /**< Device physical port index. */
	int pf_bond; /**< bonding device PF index. < 0 - no bonding */
	struct mlx5_switch_info info; /**< Switch information. */
	void *phys_dev; /**< Associated physical device. */
	struct rte_eth_dev *eth_dev; /**< Associated Ethernet device. */
	struct rte_pci_device *pci_dev; /**< Backend PCI device. */
};

/** Key string for IPC. */
#define MLX5_MP_NAME "net_mlx5_mp"


LIST_HEAD(mlx5_dev_list, mlx5_dev_ctx_shared);

/* Shared data between primary and secondary processes. */
struct mlx5_shared_data {
	rte_spinlock_t lock;
	/* Global spinlock for primary and secondary processes. */
	int init_done; /* Whether primary has done initialization. */
	unsigned int secondary_cnt; /* Number of secondary processes init'd. */
	struct mlx5_dev_list mem_event_cb_list;
	rte_rwlock_t mem_event_rwlock;
};

/* Per-process data structure, not visible to other processes. */
struct mlx5_local_data {
	int init_done; /* Whether a secondary has done initialization. */
};

extern struct mlx5_shared_data *mlx5_shared_data;

/* Dev ops structs */
extern const struct eth_dev_ops mlx5_os_dev_ops;
extern const struct eth_dev_ops mlx5_os_dev_sec_ops;
extern const struct eth_dev_ops mlx5_os_dev_ops_isolate;

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
	/* Number of device stats identified by PMD. */
	uint16_t  mlx5_stats_n;
	/* Index in the device counters table. */
	uint16_t dev_table_idx[MLX5_MAX_XSTATS];
	uint64_t base[MLX5_MAX_XSTATS];
	uint64_t xstats[MLX5_MAX_XSTATS];
	uint64_t hw_stats[MLX5_MAX_XSTATS];
	struct mlx5_counter_ctrl info[MLX5_MAX_XSTATS];
};

struct mlx5_stats_ctrl {
	/* Base for imissed counter. */
	uint64_t imissed_base;
	uint64_t imissed;
};

/* Default PMD specific parameter value. */
#define MLX5_ARG_UNSET (-1)

#define MLX5_LRO_SUPPORTED(dev) \
	(((struct mlx5_priv *)((dev)->data->dev_private))->config.lro.supported)

/* Maximal size of coalesced segment for LRO is set in chunks of 256 Bytes. */
#define MLX5_LRO_SEG_CHUNK_SIZE	256u

/* Maximal size of aggregated LRO packet. */
#define MLX5_MAX_LRO_SIZE (UINT8_MAX * MLX5_LRO_SEG_CHUNK_SIZE)

/* LRO configurations structure. */
struct mlx5_lro_config {
	uint32_t supported:1; /* Whether LRO is supported. */
	uint32_t timeout; /* User configuration. */
};

/*
 * Device configuration structure.
 *
 * Merged configuration from:
 *
 *  - Device capabilities,
 *  - User device parameters disabled features.
 */
struct mlx5_dev_config {
	unsigned int hw_csum:1; /* Checksum offload is supported. */
	unsigned int hw_vlan_strip:1; /* VLAN stripping is supported. */
	unsigned int hw_vlan_insert:1; /* VLAN insertion in WQE is supported. */
	unsigned int hw_fcs_strip:1; /* FCS stripping is supported. */
	unsigned int hw_padding:1; /* End alignment padding is supported. */
	unsigned int vf:1; /* This is a VF. */
	unsigned int tunnel_en:1;
	/* Whether tunnel stateless offloads are supported. */
	unsigned int mpls_en:1; /* MPLS over GRE/UDP is enabled. */
	unsigned int cqe_comp:1; /* CQE compression is enabled. */
	unsigned int cqe_pad:1; /* CQE padding is enabled. */
	unsigned int tso:1; /* Whether TSO is supported. */
	unsigned int rx_vec_en:1; /* Rx vector is enabled. */
	unsigned int mr_ext_memseg_en:1;
	/* Whether memseg should be extended for MR creation. */
	unsigned int l3_vxlan_en:1; /* Enable L3 VXLAN flow creation. */
	unsigned int vf_nl_en:1; /* Enable Netlink requests in VF mode. */
	unsigned int dv_esw_en:1; /* Enable E-Switch DV flow. */
	unsigned int dv_flow_en:1; /* Enable DV flow. */
	unsigned int dv_xmeta_en:2; /* Enable extensive flow metadata. */
	unsigned int lacp_by_user:1;
	/* Enable user to manage LACP traffic. */
	unsigned int swp:1; /* Tx generic tunnel checksum and TSO offload. */
	unsigned int devx:1; /* Whether devx interface is available or not. */
	unsigned int dest_tir:1; /* Whether advanced DR API is available. */
	unsigned int reclaim_mode:2; /* Memory reclaim mode. */
	unsigned int rt_timestamp:1; /* realtime timestamp format. */
	unsigned int sys_mem_en:1; /* The default memory allocator. */
	unsigned int decap_en:1; /* Whether decap will be used or not. */
	struct {
		unsigned int enabled:1; /* Whether MPRQ is enabled. */
		unsigned int stride_num_n; /* Number of strides. */
		unsigned int stride_size_n; /* Size of a stride. */
		unsigned int min_stride_size_n; /* Min size of a stride. */
		unsigned int max_stride_size_n; /* Max size of a stride. */
		unsigned int max_memcpy_len;
		/* Maximum packet size to memcpy Rx packets. */
		unsigned int min_rxqs_num;
		/* Rx queue count threshold to enable MPRQ. */
	} mprq; /* Configurations for Multi-Packet RQ. */
	int mps; /* Multi-packet send supported mode. */
	int dbnc; /* Skip doorbell register write barrier. */
	unsigned int flow_prio; /* Number of flow priorities. */
	enum modify_reg flow_mreg_c[MLX5_MREG_C_NUM];
	/* Availibility of mreg_c's. */
	unsigned int tso_max_payload_sz; /* Maximum TCP payload for TSO. */
	unsigned int ind_table_max_size; /* Maximum indirection table size. */
	unsigned int max_dump_files_num; /* Maximum dump files per queue. */
	unsigned int log_hp_size; /* Single hairpin queue data size in total. */
	int txqs_inline; /* Queue number threshold for inlining. */
	int txq_inline_min; /* Minimal amount of data bytes to inline. */
	int txq_inline_max; /* Max packet size for inlining with SEND. */
	int txq_inline_mpw; /* Max packet size for inlining with eMPW. */
	int tx_pp; /* Timestamp scheduling granularity in nanoseconds. */
	int tx_skew; /* Tx scheduling skew between WQE and data on wire. */
	struct mlx5_hca_attr hca_attr; /* HCA attributes. */
	struct mlx5_lro_config lro; /* LRO configuration. */
};


/**
 * Type of object being allocated.
 */
enum mlx5_verbs_alloc_type {
	MLX5_VERBS_ALLOC_TYPE_NONE,
	MLX5_VERBS_ALLOC_TYPE_TX_QUEUE,
	MLX5_VERBS_ALLOC_TYPE_RX_QUEUE,
};

/* Structure for VF VLAN workaround. */
struct mlx5_vf_vlan {
	uint32_t tag:12;
	uint32_t created:1;
};

/**
 * Verbs allocator needs a context to know in the callback which kind of
 * resources it is allocating.
 */
struct mlx5_verbs_alloc_ctx {
	enum mlx5_verbs_alloc_type type; /* Kind of object being allocated. */
	const void *obj; /* Pointer to the DPDK object. */
};

/* Flow drop context necessary due to Verbs API. */
struct mlx5_drop {
	struct mlx5_hrxq *hrxq; /* Hash Rx queue queue. */
	struct mlx5_rxq_obj *rxq; /* Rx queue object. */
};

#define MLX5_COUNTERS_PER_POOL 512
#define MLX5_MAX_PENDING_QUERIES 4
#define MLX5_CNT_CONTAINER_RESIZE 64
#define MLX5_CNT_AGE_OFFSET 0x80000000
#define CNT_SIZE (sizeof(struct mlx5_flow_counter))
#define CNTEXT_SIZE (sizeof(struct mlx5_flow_counter_ext))
#define AGE_SIZE (sizeof(struct mlx5_age_param))
#define MLX5_AGING_TIME_DELAY	7
#define CNT_POOL_TYPE_EXT	(1 << 0)
#define CNT_POOL_TYPE_AGE	(1 << 1)
#define IS_EXT_POOL(pool) (((pool)->type) & CNT_POOL_TYPE_EXT)
#define IS_AGE_POOL(pool) (((pool)->type) & CNT_POOL_TYPE_AGE)
#define MLX_CNT_IS_AGE(counter) ((counter) & MLX5_CNT_AGE_OFFSET ? 1 : 0)
#define MLX5_CNT_LEN(pool) \
	(CNT_SIZE + \
	(IS_AGE_POOL(pool) ? AGE_SIZE : 0) + \
	(IS_EXT_POOL(pool) ? CNTEXT_SIZE : 0))
#define MLX5_POOL_GET_CNT(pool, index) \
	((struct mlx5_flow_counter *) \
	((uint8_t *)((pool) + 1) + (index) * (MLX5_CNT_LEN(pool))))
#define MLX5_CNT_ARRAY_IDX(pool, cnt) \
	((int)(((uint8_t *)(cnt) - (uint8_t *)((pool) + 1)) / \
	MLX5_CNT_LEN(pool)))
/*
 * The pool index and offset of counter in the pool array makes up the
 * counter index. In case the counter is from pool 0 and offset 0, it
 * should plus 1 to avoid index 0, since 0 means invalid counter index
 * currently.
 */
#define MLX5_MAKE_CNT_IDX(pi, offset) \
	((pi) * MLX5_COUNTERS_PER_POOL + (offset) + 1)
#define MLX5_CNT_TO_CNT_EXT(pool, cnt) \
	((struct mlx5_flow_counter_ext *)\
	((uint8_t *)((cnt) + 1) + \
	(IS_AGE_POOL(pool) ? AGE_SIZE : 0)))
#define MLX5_GET_POOL_CNT_EXT(pool, offset) \
	MLX5_CNT_TO_CNT_EXT(pool, MLX5_POOL_GET_CNT((pool), (offset)))
#define MLX5_CNT_TO_AGE(cnt) \
	((struct mlx5_age_param *)((cnt) + 1))
/*
 * The maximum single counter is 0x800000 as MLX5_CNT_BATCH_OFFSET
 * defines. The pool size is 512, pool index should never reach
 * INT16_MAX.
 */
#define POOL_IDX_INVALID UINT16_MAX

struct mlx5_flow_counter_pool;

/*age status*/
enum {
	AGE_FREE, /* Initialized state. */
	AGE_CANDIDATE, /* Counter assigned to flows. */
	AGE_TMOUT, /* Timeout, wait for rte_flow_get_aged_flows and destroy. */
};

#define MLX5_CNT_CONTAINER(sh, batch, age) (&(sh)->cmng.ccont \
					    [(batch) * 2 + (age)])

enum {
	MLX5_CCONT_TYPE_SINGLE,
	MLX5_CCONT_TYPE_SINGLE_FOR_AGE,
	MLX5_CCONT_TYPE_BATCH,
	MLX5_CCONT_TYPE_BATCH_FOR_AGE,
	MLX5_CCONT_TYPE_MAX,
};

/* Counter age parameter. */
struct mlx5_age_param {
	rte_atomic16_t state; /**< Age state. */
	uint16_t port_id; /**< Port id of the counter. */
	uint32_t timeout:15; /**< Age timeout in unit of 0.1sec. */
	uint32_t expire:16; /**< Expire time(0.1sec) in the future. */
	void *context; /**< Flow counter age context. */
};

struct flow_counter_stats {
	uint64_t hits;
	uint64_t bytes;
};

struct mlx5_flow_counter_pool;
/* Generic counters information. */
struct mlx5_flow_counter {
	TAILQ_ENTRY(mlx5_flow_counter) next;
	/**< Pointer to the next flow counter structure. */
	union {
		uint64_t hits; /**< Reset value of hits packets. */
		struct mlx5_flow_counter_pool *pool; /**< Counter pool. */
	};
	uint64_t bytes; /**< Reset value of bytes. */
	void *action; /**< Pointer to the dv action. */
};

/* Extend counters information for none batch counters. */
struct mlx5_flow_counter_ext {
	uint32_t shared:1; /**< Share counter ID with other flow rules. */
	uint32_t batch: 1;
	uint32_t skipped:1; /* This counter is skipped or not. */
	/**< Whether the counter was allocated by batch command. */
	uint32_t ref_cnt:29; /**< Reference counter. */
	uint32_t id; /**< User counter ID. */
	union {  /**< Holds the counters for the rule. */
#if defined(HAVE_IBV_DEVICE_COUNTERS_SET_V42)
		struct ibv_counter_set *cs;
#elif defined(HAVE_IBV_DEVICE_COUNTERS_SET_V45)
		struct ibv_counters *cs;
#endif
		struct mlx5_devx_obj *dcs; /**< Counter Devx object. */
	};
};

TAILQ_HEAD(mlx5_counters, mlx5_flow_counter);

/* Generic counter pool structure - query is in pool resolution. */
struct mlx5_flow_counter_pool {
	TAILQ_ENTRY(mlx5_flow_counter_pool) next;
	struct mlx5_counters counters[2]; /* Free counter list. */
	union {
		struct mlx5_devx_obj *min_dcs;
		rte_atomic64_t a64_dcs;
	};
	/* The devx object of the minimum counter ID. */
	uint32_t index:28; /* Pool index in container. */
	uint32_t type:2; /* Memory type behind the counter array. */
	uint32_t skip_cnt:1; /* Pool contains skipped counter. */
	volatile uint32_t query_gen:1; /* Query round. */
	rte_spinlock_t sl; /* The pool lock. */
	struct mlx5_counter_stats_raw *raw;
	struct mlx5_counter_stats_raw *raw_hw; /* The raw on HW working. */
};

struct mlx5_counter_stats_raw;

/* Memory management structure for group of counter statistics raws. */
struct mlx5_counter_stats_mem_mng {
	LIST_ENTRY(mlx5_counter_stats_mem_mng) next;
	struct mlx5_counter_stats_raw *raws;
	struct mlx5_devx_obj *dm;
	void *umem;
};

/* Raw memory structure for the counter statistics values of a pool. */
struct mlx5_counter_stats_raw {
	LIST_ENTRY(mlx5_counter_stats_raw) next;
	int min_dcs_id;
	struct mlx5_counter_stats_mem_mng *mem_mng;
	volatile struct flow_counter_stats *data;
};

TAILQ_HEAD(mlx5_counter_pools, mlx5_flow_counter_pool);

/* Container structure for counter pools. */
struct mlx5_pools_container {
	rte_atomic16_t n_valid; /* Number of valid pools. */
	uint16_t n; /* Number of pools. */
	uint16_t last_pool_idx; /* Last used pool index */
	int min_id; /* The minimum counter ID in the pools. */
	int max_id; /* The maximum counter ID in the pools. */
	rte_spinlock_t resize_sl; /* The resize lock. */
	rte_spinlock_t csl; /* The counter free list lock. */
	struct mlx5_counters counters; /* Free counter list. */
	struct mlx5_counter_pools pool_list; /* Counter pool list. */
	struct mlx5_flow_counter_pool **pools; /* Counter pool array. */
	struct mlx5_counter_stats_mem_mng *mem_mng;
	/* Hold the memory management for the next allocated pools raws. */
};

/* Counter global management structure. */
struct mlx5_flow_counter_mng {
	struct mlx5_pools_container ccont[MLX5_CCONT_TYPE_MAX];
	struct mlx5_counters flow_counters; /* Legacy flow counter list. */
	uint8_t pending_queries;
	uint8_t batch;
	uint16_t pool_index;
	uint8_t age;
	uint8_t query_thread_on;
	LIST_HEAD(mem_mngs, mlx5_counter_stats_mem_mng) mem_mngs;
	LIST_HEAD(stat_raws, mlx5_counter_stats_raw) free_stat_raws;
};

/* Default miss action resource structure. */
struct mlx5_flow_default_miss_resource {
	void *action; /* Pointer to the rdma-core action. */
	rte_atomic32_t refcnt; /* Default miss action reference counter. */
};

#define MLX5_AGE_EVENT_NEW		1
#define MLX5_AGE_TRIGGER		2
#define MLX5_AGE_SET(age_info, BIT) \
	((age_info)->flags |= (1 << (BIT)))
#define MLX5_AGE_GET(age_info, BIT) \
	((age_info)->flags & (1 << (BIT)))
#define GET_PORT_AGE_INFO(priv) \
	(&((priv)->sh->port[(priv)->dev_port - 1].age_info))

/* Aging information for per port. */
struct mlx5_age_info {
	uint8_t flags; /*Indicate if is new event or need be trigered*/
	struct mlx5_counters aged_counters; /* Aged flow counter list. */
	rte_spinlock_t aged_sl; /* Aged flow counter list lock. */
};

/* Per port data of shared IB device. */
struct mlx5_dev_shared_port {
	uint32_t ih_port_id;
	uint32_t devx_ih_port_id;
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

/* Table key of the hash organization. */
union mlx5_flow_tbl_key {
	struct {
		/* Table ID should be at the lowest address. */
		uint32_t table_id;	/**< ID of the table. */
		uint16_t reserved;	/**< must be zero for comparison. */
		uint8_t domain;		/**< 1 - FDB, 0 - NIC TX/RX. */
		uint8_t direction;	/**< 1 - egress, 0 - ingress. */
	};
	uint64_t v64;			/**< full 64bits value of key */
};

/* Table structure. */
struct mlx5_flow_tbl_resource {
	void *obj; /**< Pointer to DR table object. */
	rte_atomic32_t refcnt; /**< Reference counter. */
};

#define MLX5_MAX_TABLES UINT16_MAX
#define MLX5_FLOW_TABLE_LEVEL_METER (UINT16_MAX - 3)
#define MLX5_FLOW_TABLE_LEVEL_SUFFIX (UINT16_MAX - 2)
#define MLX5_HAIRPIN_TX_TABLE (UINT16_MAX - 1)
/* Reserve the last two tables for metadata register copy. */
#define MLX5_FLOW_MREG_ACT_TABLE_GROUP (MLX5_MAX_TABLES - 1)
#define MLX5_FLOW_MREG_CP_TABLE_GROUP (MLX5_MAX_TABLES - 2)
/* Tables for metering splits should be added here. */
#define MLX5_MAX_TABLES_EXTERNAL (MLX5_MAX_TABLES - 3)
#define MLX5_MAX_TABLES_FDB UINT16_MAX

/* ID generation structure. */
struct mlx5_flow_id_pool {
	uint32_t *free_arr; /**< Pointer to the a array of free values. */
	uint32_t base_index;
	/**< The next index that can be used without any free elements. */
	uint32_t *curr; /**< Pointer to the index to pop. */
	uint32_t *last; /**< Pointer to the last element in the empty arrray. */
	uint32_t max_id; /**< Maximum id can be allocated from the pool. */
};

/* Tx pacing queue structure - for Clock and Rearm queues. */
struct mlx5_txpp_wq {
	/* Completion Queue related data.*/
	struct mlx5_devx_obj *cq;
	struct mlx5dv_devx_umem *cq_umem;
	union {
		volatile void *cq_buf;
		volatile struct mlx5_cqe *cqes;
	};
	volatile uint32_t *cq_dbrec;
	uint32_t cq_ci:24;
	uint32_t arm_sn:2;
	/* Send Queue related data.*/
	struct mlx5_devx_obj *sq;
	struct mlx5dv_devx_umem *sq_umem;
	union {
		volatile void *sq_buf;
		volatile struct mlx5_wqe *wqes;
	};
	uint16_t sq_size; /* Number of WQEs in the queue. */
	uint16_t sq_ci; /* Next WQE to execute. */
	volatile uint32_t *sq_dbrec;
};

/* Tx packet pacing internal timestamp. */
struct mlx5_txpp_ts {
	rte_atomic64_t ci_ts;
	rte_atomic64_t ts;
};

/* Tx packet pacing structure. */
struct mlx5_dev_txpp {
	pthread_mutex_t mutex; /* Pacing create/destroy mutex. */
	uint32_t refcnt; /* Pacing reference counter. */
	uint32_t freq; /* Timestamp frequency, Hz. */
	uint32_t tick; /* Completion tick duration in nanoseconds. */
	uint32_t test; /* Packet pacing test mode. */
	int32_t skew; /* Scheduling skew. */
	uint32_t eqn; /* Event Queue number. */
	struct rte_intr_handle intr_handle; /* Periodic interrupt. */
	struct mlx5dv_devx_event_channel *echan; /* Event Channel. */
	struct mlx5_txpp_wq clock_queue; /* Clock Queue. */
	struct mlx5_txpp_wq rearm_queue; /* Clock Queue. */
	struct mlx5dv_pp *pp; /* Packet pacing context. */
	uint16_t pp_id; /* Packet pacing context index. */
	uint16_t ts_n; /* Number of captured timestamps. */
	uint16_t ts_p; /* Pointer to statisticks timestamp. */
	struct mlx5_txpp_ts *tsa; /* Timestamps sliding window stats. */
	struct mlx5_txpp_ts ts; /* Cached completion id/timestamp. */
	uint32_t sync_lost:1; /* ci/timestamp synchronization lost. */
	/* Statistics counters. */
	rte_atomic32_t err_miss_int; /* Missed service interrupt. */
	rte_atomic32_t err_rearm_queue; /* Rearm Queue errors. */
	rte_atomic32_t err_clock_queue; /* Clock Queue errors. */
	rte_atomic32_t err_ts_past; /* Timestamp in the past. */
	rte_atomic32_t err_ts_future; /* Timestamp in the distant future. */
};

/* Supported flex parser profile ID. */
enum mlx5_flex_parser_profile_id {
	MLX5_FLEX_PARSER_ECPRI_0 = 0,
	MLX5_FLEX_PARSER_MAX = 8,
};

/* Sample ID information of flex parser structure. */
struct mlx5_flex_parser_profiles {
	uint32_t num;		/* Actual number of samples. */
	uint32_t ids[8];	/* Sample IDs for this profile. */
	uint8_t offset[8];	/* Bytes offset of each parser. */
	void *obj;		/* Flex parser node object. */
};

/*
 * Shared Infiniband device context for Master/Representors
 * which belong to same IB device with multiple IB ports.
 **/
struct mlx5_dev_ctx_shared {
	LIST_ENTRY(mlx5_dev_ctx_shared) next;
	uint32_t refcnt;
	uint32_t devx:1; /* Opened with DV. */
	uint32_t max_port; /* Maximal IB device port index. */
	void *ctx; /* Verbs/DV/DevX context. */
	void *pd; /* Protection Domain. */
	uint32_t pdn; /* Protection Domain number. */
	uint32_t tdn; /* Transport Domain number. */
	char ibdev_name[DEV_SYSFS_NAME_MAX]; /* SYSFS dev name. */
	char ibdev_path[DEV_SYSFS_PATH_MAX]; /* SYSFS dev path for secondary */
	struct mlx5_dev_attr device_attr; /* Device properties. */
	int numa_node; /* Numa node of backing physical device. */
	LIST_ENTRY(mlx5_dev_ctx_shared) mem_event_cb;
	/**< Called by memory event callback. */
	struct mlx5_mr_share_cache share_cache;
	/* Packet pacing related structure. */
	struct mlx5_dev_txpp txpp;
	/* Shared DV/DR flow data section. */
	pthread_mutex_t dv_mutex; /* DV context mutex. */
	uint32_t dv_meta_mask; /* flow META metadata supported mask. */
	uint32_t dv_mark_mask; /* flow MARK metadata supported mask. */
	uint32_t dv_regc0_mask; /* available bits of metatada reg_c[0]. */
	uint32_t dv_refcnt; /* DV/DR data reference counter. */
	void *fdb_domain; /* FDB Direct Rules name space handle. */
	void *rx_domain; /* RX Direct Rules name space handle. */
	void *tx_domain; /* TX Direct Rules name space handle. */
#ifndef RTE_ARCH_64
	rte_spinlock_t uar_lock_cq; /* CQs share a common distinct UAR */
	rte_spinlock_t uar_lock[MLX5_UAR_PAGE_NUM_MAX];
	/* UAR same-page access control required in 32bit implementations. */
#endif
	struct mlx5_hlist *flow_tbls;
	/* Direct Rules tables for FDB, NIC TX+RX */
	void *esw_drop_action; /* Pointer to DR E-Switch drop action. */
	void *pop_vlan_action; /* Pointer to DR pop VLAN action. */
	uint32_t encaps_decaps; /* Encap/decap action indexed memory list. */
	LIST_HEAD(modify_cmd, mlx5_flow_dv_modify_hdr_resource) modify_cmds;
	struct mlx5_hlist *tag_table;
	uint32_t port_id_action_list; /* List of port ID actions. */
	uint32_t push_vlan_action_list; /* List of push VLAN actions. */
	struct mlx5_flow_counter_mng cmng; /* Counters management structure. */
	struct mlx5_flow_default_miss_resource default_miss;
	/* Default miss action resource structure. */
	struct mlx5_indexed_pool *ipool[MLX5_IPOOL_MAX];
	/* Memory Pool for mlx5 flow resources. */
	struct mlx5_l3t_tbl *cnt_id_tbl; /* Shared counter lookup table. */
	/* Shared interrupt handler section. */
	struct rte_intr_handle intr_handle; /* Interrupt handler for device. */
	struct rte_intr_handle intr_handle_devx; /* DEVX interrupt handler. */
	void *devx_comp; /* DEVX async comp obj. */
	struct mlx5_devx_obj *tis; /* TIS object. */
	struct mlx5_devx_obj *td; /* Transport domain. */
	struct mlx5_flow_id_pool *flow_id_pool; /* Flow ID pool. */
	struct mlx5dv_devx_uar *tx_uar; /* Tx/packer pacing shared UAR. */
	struct mlx5_flex_parser_profiles fp[MLX5_FLEX_PARSER_MAX];
	/* Flex parser profiles information. */
	struct mlx5dv_devx_uar *devx_rx_uar; /* DevX UAR for Rx. */
	struct mlx5_dev_shared_port port[]; /* per device port data array. */
};

/* Per-process private structure. */
struct mlx5_proc_priv {
	size_t uar_table_sz;
	/* Size of UAR register table. */
	void *uar_table[];
	/* Table of UAR registers for each process. */
};

/* MTR profile list. */
TAILQ_HEAD(mlx5_mtr_profiles, mlx5_flow_meter_profile);
/* MTR list. */
TAILQ_HEAD(mlx5_flow_meters, mlx5_flow_meter);

#define MLX5_PROC_PRIV(port_id) \
	((struct mlx5_proc_priv *)rte_eth_devices[port_id].process_private)

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
	unsigned int dr_shared:1; /* DV/DR data is shared. */
	unsigned int txpp_en:1; /* Tx packet pacing enabled. */
	unsigned int counter_fallback:1; /* Use counter fallback management. */
	unsigned int mtr_en:1; /* Whether support meter. */
	unsigned int mtr_reg_share:1; /* Whether support meter REG_C share. */
	uint16_t domain_id; /* Switch domain identifier. */
	uint16_t vport_id; /* Associated VF vport index (if any). */
	uint32_t vport_meta_tag; /* Used for vport index match ove VF LAG. */
	uint32_t vport_meta_mask; /* Used for vport index field match mask. */
	int32_t representor_id; /* Port representor identifier. */
	int32_t pf_bond; /* >=0 means PF index in bonding configuration. */
	unsigned int if_index; /* Associated kernel network device index. */
	/* RX/TX queues. */
	unsigned int rxqs_n; /* RX queues array size. */
	unsigned int txqs_n; /* TX queues array size. */
	struct mlx5_rxq_data *(*rxqs)[]; /* RX queues. */
	struct mlx5_txq_data *(*txqs)[]; /* TX queues. */
	struct rte_mempool *mprq_mp; /* Mempool for Multi-Packet RQ. */
	struct rte_eth_rss_conf rss_conf; /* RSS configuration. */
	unsigned int (*reta_idx)[]; /* RETA index table. */
	unsigned int reta_idx_n; /* RETA index size. */
	struct mlx5_drop drop_queue; /* Flow drop queues. */
	uint32_t flows; /* RTE Flow rules. */
	uint32_t ctrl_flows; /* Control flow rules. */
	void *inter_flows; /* Intermediate resources for flow creation. */
	void *rss_desc; /* Intermediate rss description resources. */
	int flow_idx; /* Intermediate device flow index. */
	int flow_nested_idx; /* Intermediate device flow index, nested. */
	LIST_HEAD(rxq, mlx5_rxq_ctrl) rxqsctrl; /* DPDK Rx queues. */
	LIST_HEAD(rxqobj, mlx5_rxq_obj) rxqsobj; /* Verbs/DevX Rx queues. */
	uint32_t hrxqs; /* Verbs Hash Rx queues. */
	LIST_HEAD(txq, mlx5_txq_ctrl) txqsctrl; /* DPDK Tx queues. */
	LIST_HEAD(txqobj, mlx5_txq_obj) txqsobj; /* Verbs/DevX Tx queues. */
	/* Indirection tables. */
	LIST_HEAD(ind_tables, mlx5_ind_table_obj) ind_tbls;
	/* Pointer to next element. */
	rte_atomic32_t refcnt; /**< Reference counter. */
	/**< Verbs modify header action object. */
	uint8_t ft_type; /**< Flow table type, Rx or Tx. */
	uint8_t max_lro_msg_size;
	/* Tags resources cache. */
	uint32_t link_speed_capa; /* Link speed capabilities. */
	struct mlx5_xstats_ctrl xstats_ctrl; /* Extended stats control. */
	struct mlx5_stats_ctrl stats_ctrl; /* Stats control. */
	struct mlx5_dev_config config; /* Device configuration. */
	struct mlx5_verbs_alloc_ctx verbs_alloc_ctx;
	/* Context for Verbs allocator. */
	int nl_socket_rdma; /* Netlink socket (NETLINK_RDMA). */
	int nl_socket_route; /* Netlink socket (NETLINK_ROUTE). */
	struct mlx5_dbr_page_list dbrpgs; /* Door-bell pages. */
	struct mlx5_nl_vlan_vmwa_context *vmwa_context; /* VLAN WA context. */
	struct mlx5_flow_id_pool *qrss_id_pool;
	struct mlx5_hlist *mreg_cp_tbl;
	/* Hash table of Rx metadata register copy table. */
	uint8_t mtr_sfx_reg; /* Meter prefix-suffix flow match REG_C. */
	uint8_t mtr_color_reg; /* Meter color match REG_C. */
	struct mlx5_mtr_profiles flow_meter_profiles; /* MTR profile list. */
	struct mlx5_flow_meters flow_meters; /* MTR list. */
	uint8_t skip_default_rss_reta; /* Skip configuration of default reta. */
	uint8_t fdb_def_rule; /* Whether fdb jump to table 1 is configured. */
	struct mlx5_mp_id mp_id; /* ID of a multi-process process */
	LIST_HEAD(fdir, mlx5_fdir_flow) fdir_flows; /* fdir flows. */
};

#define PORT_ID(priv) ((priv)->dev_data->port_id)
#define ETH_DEV(priv) (&rte_eth_devices[PORT_ID(priv)])

/* mlx5.c */

int mlx5_getenv_int(const char *);
int mlx5_proc_priv_init(struct rte_eth_dev *dev);
int mlx5_udp_tunnel_port_add(struct rte_eth_dev *dev,
			      struct rte_eth_udp_tunnel *udp_tunnel);
uint16_t mlx5_eth_find_next(uint16_t port_id, struct rte_pci_device *pci_dev);
void mlx5_dev_close(struct rte_eth_dev *dev);

/* Macro to iterate over all valid ports for mlx5 driver. */
#define MLX5_ETH_FOREACH_DEV(port_id, pci_dev) \
	for (port_id = mlx5_eth_find_next(0, pci_dev); \
	     port_id < RTE_MAX_ETHPORTS; \
	     port_id = mlx5_eth_find_next(port_id + 1, pci_dev))
int mlx5_args(struct mlx5_dev_config *config, struct rte_devargs *devargs);
struct mlx5_dev_ctx_shared *
mlx5_alloc_shared_dev_ctx(const struct mlx5_dev_spawn_data *spawn,
			   const struct mlx5_dev_config *config);
void mlx5_free_shared_dev_ctx(struct mlx5_dev_ctx_shared *sh);
void mlx5_free_table_hash_list(struct mlx5_priv *priv);
int mlx5_alloc_table_hash_list(struct mlx5_priv *priv);
void mlx5_set_min_inline(struct mlx5_dev_spawn_data *spawn,
			 struct mlx5_dev_config *config);
void mlx5_set_metadata_mask(struct rte_eth_dev *dev);
int mlx5_dev_check_sibling_config(struct mlx5_priv *priv,
				  struct mlx5_dev_config *config);
int mlx5_dev_configure(struct rte_eth_dev *dev);
int mlx5_dev_infos_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *info);
int mlx5_fw_version_get(struct rte_eth_dev *dev, char *fw_ver, size_t fw_size);
int mlx5_dev_set_mtu(struct rte_eth_dev *dev, uint16_t mtu);
int mlx5_hairpin_cap_get(struct rte_eth_dev *dev,
			 struct rte_eth_hairpin_cap *cap);
bool mlx5_flex_parser_ecpri_exist(struct rte_eth_dev *dev);
int mlx5_flex_parser_ecpri_alloc(struct rte_eth_dev *dev);

/* mlx5_ethdev.c */

int mlx5_dev_configure(struct rte_eth_dev *dev);
int mlx5_fw_version_get(struct rte_eth_dev *dev, char *fw_ver,
			size_t fw_size);
int mlx5_dev_infos_get(struct rte_eth_dev *dev,
		       struct rte_eth_dev_info *info);
const uint32_t *mlx5_dev_supported_ptypes_get(struct rte_eth_dev *dev);
int mlx5_dev_set_mtu(struct rte_eth_dev *dev, uint16_t mtu);
int mlx5_hairpin_cap_get(struct rte_eth_dev *dev,
			 struct rte_eth_hairpin_cap *cap);
eth_rx_burst_t mlx5_select_rx_function(struct rte_eth_dev *dev);
struct mlx5_priv *mlx5_port_to_eswitch_info(uint16_t port, bool valid);
struct mlx5_priv *mlx5_dev_to_eswitch_info(struct rte_eth_dev *dev);
int mlx5_dev_configure_rss_reta(struct rte_eth_dev *dev);

/* mlx5_ethdev_os.c */

int mlx5_get_ifname(const struct rte_eth_dev *dev, char (*ifname)[IF_NAMESIZE]);
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
int mlx5_set_link_down(struct rte_eth_dev *dev);
int mlx5_set_link_up(struct rte_eth_dev *dev);
int mlx5_is_removed(struct rte_eth_dev *dev);
int mlx5_sysfs_switch_info(unsigned int ifindex,
			   struct mlx5_switch_info *info);
void mlx5_translate_port_name(const char *port_name_in,
			      struct mlx5_switch_info *port_info_out);
void mlx5_intr_callback_unregister(const struct rte_intr_handle *handle,
				   rte_intr_callback_fn cb_fn, void *cb_arg);
int mlx5_get_module_info(struct rte_eth_dev *dev,
			 struct rte_eth_dev_module_info *modinfo);
int mlx5_get_module_eeprom(struct rte_eth_dev *dev,
			   struct rte_dev_eeprom_info *info);
int mlx5_os_read_dev_stat(struct mlx5_priv *priv,
			  const char *ctr_name, uint64_t *stat);
int mlx5_os_read_dev_counters(struct rte_eth_dev *dev, uint64_t *stats);
int mlx5_os_get_stats_n(struct rte_eth_dev *dev);
void mlx5_os_stats_init(struct rte_eth_dev *dev);

/* mlx5_mac.c */

void mlx5_mac_addr_remove(struct rte_eth_dev *dev, uint32_t index);
int mlx5_mac_addr_add(struct rte_eth_dev *dev, struct rte_ether_addr *mac,
		      uint32_t index, uint32_t vmdq);
struct mlx5_nl_vlan_vmwa_context *mlx5_vlan_vmwa_init
				    (struct rte_eth_dev *dev, uint32_t ifindex);
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
void mlx5_vlan_vmwa_exit(struct mlx5_nl_vlan_vmwa_context *ctx);
void mlx5_vlan_vmwa_release(struct rte_eth_dev *dev,
			    struct mlx5_vf_vlan *vf_vlan);
void mlx5_vlan_vmwa_acquire(struct rte_eth_dev *dev,
			    struct mlx5_vf_vlan *vf_vlan);

/* mlx5_trigger.c */

int mlx5_dev_start(struct rte_eth_dev *dev);
void mlx5_dev_stop(struct rte_eth_dev *dev);
int mlx5_traffic_enable(struct rte_eth_dev *dev);
void mlx5_traffic_disable(struct rte_eth_dev *dev);
int mlx5_traffic_restart(struct rte_eth_dev *dev);

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
void mlx5_flow_list_flush(struct rte_eth_dev *dev, uint32_t *list, bool active);
int mlx5_flow_flush(struct rte_eth_dev *dev, struct rte_flow_error *error);
int mlx5_flow_query(struct rte_eth_dev *dev, struct rte_flow *flow,
		    const struct rte_flow_action *action, void *data,
		    struct rte_flow_error *error);
int mlx5_flow_isolate(struct rte_eth_dev *dev, int enable,
		      struct rte_flow_error *error);
int mlx5_dev_filter_ctrl(struct rte_eth_dev *dev,
			 enum rte_filter_type filter_type,
			 enum rte_filter_op filter_op,
			 void *arg);
int mlx5_flow_start(struct rte_eth_dev *dev, uint32_t *list);
void mlx5_flow_stop(struct rte_eth_dev *dev, uint32_t *list);
int mlx5_flow_start_default(struct rte_eth_dev *dev);
void mlx5_flow_stop_default(struct rte_eth_dev *dev);
void mlx5_flow_alloc_intermediate(struct rte_eth_dev *dev);
void mlx5_flow_free_intermediate(struct rte_eth_dev *dev);
int mlx5_flow_verify(struct rte_eth_dev *dev);
int mlx5_ctrl_flow_source_queue(struct rte_eth_dev *dev, uint32_t queue);
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
int mlx5_flow_create_drop_queue(struct rte_eth_dev *dev);
void mlx5_flow_delete_drop_queue(struct rte_eth_dev *dev);
void mlx5_flow_async_pool_query_handle(struct mlx5_dev_ctx_shared *sh,
				       uint64_t async_id, int status);
void mlx5_set_query_alarm(struct mlx5_dev_ctx_shared *sh);
void mlx5_flow_query_alarm(void *arg);
uint32_t mlx5_counter_alloc(struct rte_eth_dev *dev);
void mlx5_counter_free(struct rte_eth_dev *dev, uint32_t cnt);
int mlx5_counter_query(struct rte_eth_dev *dev, uint32_t cnt,
		       bool clear, uint64_t *pkts, uint64_t *bytes);
int mlx5_flow_dev_dump(struct rte_eth_dev *dev, FILE *file,
		       struct rte_flow_error *error);
void mlx5_flow_rxq_dynf_metadata_set(struct rte_eth_dev *dev);
int mlx5_flow_get_aged_flows(struct rte_eth_dev *dev, void **contexts,
			uint32_t nb_contexts, struct rte_flow_error *error);

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

/* mlx5_flow_meter.c */

int mlx5_flow_meter_ops_get(struct rte_eth_dev *dev, void *arg);
struct mlx5_flow_meter *mlx5_flow_meter_find(struct mlx5_priv *priv,
					     uint32_t meter_id);
struct mlx5_flow_meter *mlx5_flow_meter_attach
					(struct mlx5_priv *priv,
					 uint32_t meter_id,
					 const struct rte_flow_attr *attr,
					 struct rte_flow_error *error);
void mlx5_flow_meter_detach(struct mlx5_flow_meter *fm);

/* mlx5_os.c */
struct rte_pci_driver;
int mlx5_os_get_dev_attr(void *ctx, struct mlx5_dev_attr *dev_attr);
void mlx5_os_free_shared_dr(struct mlx5_priv *priv);
int mlx5_os_open_device(const struct mlx5_dev_spawn_data *spawn,
			 const struct mlx5_dev_config *config,
			 struct mlx5_dev_ctx_shared *sh);
int mlx5_os_get_pdn(void *pd, uint32_t *pdn);
int mlx5_os_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
		       struct rte_pci_device *pci_dev);
void mlx5_os_dev_shared_handler_install(struct mlx5_dev_ctx_shared *sh);
void mlx5_os_dev_shared_handler_uninstall(struct mlx5_dev_ctx_shared *sh);
void mlx5_os_set_reg_mr_cb(mlx5_reg_mr_t *reg_mr_cb,
			   mlx5_dereg_mr_t *dereg_mr_cb);
void mlx5_os_mac_addr_remove(struct rte_eth_dev *dev, uint32_t index);
int mlx5_os_mac_addr_add(struct rte_eth_dev *dev, struct rte_ether_addr *mac,
			 uint32_t index);
int mlx5_os_vf_mac_addr_modify(struct mlx5_priv *priv, unsigned int iface_idx,
			       struct rte_ether_addr *mac_addr,
			       int vf_index);
int mlx5_os_set_promisc(struct rte_eth_dev *dev, int enable);
int mlx5_os_set_allmulti(struct rte_eth_dev *dev, int enable);
int mlx5_os_set_nonblock_channel_fd(int fd);

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

/* mlx5_rxtx.c */

eth_tx_burst_t mlx5_select_tx_function(struct rte_eth_dev *dev);

#endif /* RTE_PMD_MLX5_H_ */
