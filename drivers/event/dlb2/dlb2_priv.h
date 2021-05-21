/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2020 Intel Corporation
 */

#ifndef _DLB2_PRIV_H_
#define _DLB2_PRIV_H_

#include <emmintrin.h>
#include <stdbool.h>

#include <rte_eventdev.h>
#include <rte_config.h>
#include "dlb2_user.h"
#include "dlb2_log.h"
#include "rte_pmd_dlb2.h"

#ifndef RTE_LIBRTE_PMD_DLB2_QUELL_STATS
#define DLB2_INC_STAT(_stat, _incr_val) ((_stat) += _incr_val)
#else
#define DLB2_INC_STAT(_stat, _incr_val)
#endif

#define EVDEV_DLB2_NAME_PMD dlb2_event

/*  command line arg strings */
#define NUMA_NODE_ARG "numa_node"
#define DLB2_MAX_NUM_EVENTS "max_num_events"
#define DLB2_NUM_DIR_CREDITS "num_dir_credits"
#define DEV_ID_ARG "dev_id"
#define DLB2_QID_DEPTH_THRESH_ARG "qid_depth_thresh"
#define DLB2_COS_ARG "cos"

/* Begin HW related defines and structs */

#define DLB2_MAX_NUM_DOMAINS 32
#define DLB2_MAX_NUM_VFS 16
#define DLB2_MAX_NUM_LDB_QUEUES 32
#define DLB2_MAX_NUM_LDB_PORTS 64
#define DLB2_MAX_NUM_DIR_PORTS 64
#define DLB2_MAX_NUM_DIR_QUEUES 64
#define DLB2_MAX_NUM_FLOWS (64 * 1024)
#define DLB2_MAX_NUM_LDB_CREDITS (8 * 1024)
#define DLB2_MAX_NUM_DIR_CREDITS (2 * 1024)
#define DLB2_MAX_NUM_LDB_CREDIT_POOLS 64
#define DLB2_MAX_NUM_DIR_CREDIT_POOLS 64
#define DLB2_MAX_NUM_HIST_LIST_ENTRIES 2048
#define DLB2_MAX_NUM_AQOS_ENTRIES 2048
#define DLB2_MAX_NUM_QIDS_PER_LDB_CQ 8
#define DLB2_QID_PRIORITIES 8
#define DLB2_MAX_DEVICE_PATH 32
#define DLB2_MIN_DEQUEUE_TIMEOUT_NS 1
/* Note: "- 1" here to support the timeout range check in eventdev_autotest */
#define DLB2_MAX_DEQUEUE_TIMEOUT_NS (UINT32_MAX - 1)
#define DLB2_SW_CREDIT_BATCH_SZ 32
#define DLB2_NUM_SN_GROUPS 2
#define DLB2_MAX_LDB_SN_ALLOC 1024
#define DLB2_MAX_QUEUE_DEPTH_THRESHOLD 8191

/* 2048 total hist list entries and 64 total ldb ports, which
 * makes for 2048/64 == 32 hist list entries per port. However, CQ
 * depth must be a power of 2 and must also be >= HIST LIST entries.
 * As a result we just limit the maximum dequeue depth to 32.
 */
#define DLB2_MIN_CQ_DEPTH 1
#define DLB2_MAX_CQ_DEPTH 32
#define DLB2_MIN_HARDWARE_CQ_DEPTH 8
#define DLB2_NUM_HIST_LIST_ENTRIES_PER_LDB_PORT \
	DLB2_MAX_CQ_DEPTH

/*
 * Static per queue/port provisioning values
 */
#define DLB2_NUM_ATOMIC_INFLIGHTS_PER_QUEUE 64

#define CQ_BASE(is_dir) ((is_dir) ? DLB2_DIR_CQ_BASE : DLB2_LDB_CQ_BASE)
#define CQ_SIZE(is_dir) ((is_dir) ? DLB2_DIR_CQ_MAX_SIZE : \
				    DLB2_LDB_CQ_MAX_SIZE)
#define PP_BASE(is_dir) ((is_dir) ? DLB2_DIR_PP_BASE : DLB2_LDB_PP_BASE)

#define PAGE_SIZE (sysconf(_SC_PAGESIZE))

#define DLB2_NUM_QES_PER_CACHE_LINE 4

#define DLB2_MAX_ENQUEUE_DEPTH 64
#define DLB2_MIN_ENQUEUE_DEPTH 4

#define DLB2_NAME_SIZE 64

#define DLB2_1K 1024
#define DLB2_2K (2 * DLB2_1K)
#define DLB2_4K (4 * DLB2_1K)
#define DLB2_16K (16 * DLB2_1K)
#define DLB2_32K (32 * DLB2_1K)
#define DLB2_1MB (DLB2_1K * DLB2_1K)
#define DLB2_16MB (16 * DLB2_1MB)

/* Use the upper 3 bits of the event priority to select the DLB2 priority */
#define EV_TO_DLB2_PRIO(x) ((x) >> 5)
#define DLB2_TO_EV_PRIO(x) ((x) << 5)

enum dlb2_hw_port_types {
	DLB2_LDB_PORT,
	DLB2_DIR_PORT,
	DLB2_NUM_PORT_TYPES /* Must be last */
};

enum dlb2_hw_queue_types {
	DLB2_LDB_QUEUE,
	DLB2_DIR_QUEUE,
	DLB2_NUM_QUEUE_TYPES /* Must be last */
};

#define PORT_TYPE(p) ((p)->is_directed ? DLB2_DIR_PORT : DLB2_LDB_PORT)

/* Do not change - must match hardware! */
enum dlb2_hw_sched_type {
	DLB2_SCHED_ATOMIC = 0,
	DLB2_SCHED_UNORDERED,
	DLB2_SCHED_ORDERED,
	DLB2_SCHED_DIRECTED,
	/* DLB2_NUM_HW_SCHED_TYPES must be last */
	DLB2_NUM_HW_SCHED_TYPES
};

struct dlb2_hw_rsrcs {
	int32_t nb_events_limit;
	uint32_t num_queues;		/* Total queues (lb + dir) */
	uint32_t num_ldb_queues;	/* Number of available ldb queues */
	uint32_t num_ldb_ports;         /* Number of load balanced ports */
	uint32_t num_dir_ports;         /* Number of directed ports */
	uint32_t num_ldb_credits;       /* Number of load balanced credits */
	uint32_t num_dir_credits;       /* Number of directed credits */
	uint32_t reorder_window_size;   /* Size of reorder window */
};

struct dlb2_hw_resource_info {
	/**> Max resources that can be provided */
	struct dlb2_hw_rsrcs hw_rsrc_max;
	int num_sched_domains;
	uint32_t socket_id;
};

enum dlb2_enqueue_type {
	/**>
	 * New : Used to inject a new packet into the QM.
	 */
	DLB2_ENQ_NEW,
	/**>
	 * Forward : Enqueues a packet, and
	 *  - if atomic: release any lock it holds in the QM
	 *  - if ordered: release the packet for egress re-ordering
	 */
	DLB2_ENQ_FWD,
	/**>
	 * Enqueue Drop : Release an inflight packet. Must be called with
	 * event == NULL. Used to drop a packet.
	 *
	 * Note that all packets dequeued from a load-balanced port must be
	 * released, either with DLB2_ENQ_DROP or DLB2_ENQ_FWD.
	 */
	DLB2_ENQ_DROP,

	/* marker for array sizing etc. */
	_DLB2_NB_ENQ_TYPES
};

/* hw-specific format - do not change */

struct dlb2_event_type {
	uint8_t major:4;
	uint8_t unused:4;
	uint8_t sub;
};

union dlb2_opaque_data {
	uint16_t opaque_data;
	struct dlb2_event_type event_type;
};

struct dlb2_msg_info {
	uint8_t qid;
	uint8_t sched_type:2;
	uint8_t priority:3;
	uint8_t msg_type:3;
};

#define DLB2_NEW_CMD_BYTE 0x08
#define DLB2_FWD_CMD_BYTE 0x0A
#define DLB2_COMP_CMD_BYTE 0x02
#define DLB2_POP_CMD_BYTE 0x01
#define DLB2_NOOP_CMD_BYTE 0x00

/* hw-specific format - do not change */
struct dlb2_enqueue_qe {
	uint64_t data;
	/* Word 3 */
	union dlb2_opaque_data u;
	uint8_t qid;
	uint8_t sched_type:2;
	uint8_t priority:3;
	uint8_t msg_type:3;
	/* Word 4 */
	uint16_t lock_id;
	uint8_t meas_lat:1;
	uint8_t rsvd1:2;
	uint8_t no_dec:1;
	uint8_t cmp_id:4;
	union {
		uint8_t cmd_byte;
		struct {
			uint8_t cq_token:1;
			uint8_t qe_comp:1;
			uint8_t qe_frag:1;
			uint8_t qe_valid:1;
			uint8_t rsvd3:1;
			uint8_t error:1;
			uint8_t rsvd:2;
		};
	};
};

/* hw-specific format - do not change */
struct dlb2_cq_pop_qe {
	uint64_t data;
	union dlb2_opaque_data u;
	uint8_t qid;
	uint8_t sched_type:2;
	uint8_t priority:3;
	uint8_t msg_type:3;
	uint16_t tokens:10;
	uint16_t rsvd2:6;
	uint8_t meas_lat:1;
	uint8_t rsvd1:2;
	uint8_t no_dec:1;
	uint8_t cmp_id:4;
	union {
		uint8_t cmd_byte;
		struct {
			uint8_t cq_token:1;
			uint8_t qe_comp:1;
			uint8_t qe_frag:1;
			uint8_t qe_valid:1;
			uint8_t rsvd3:1;
			uint8_t error:1;
			uint8_t rsvd:2;
		};
	};
};

/* hw-specific format - do not change */
struct dlb2_dequeue_qe {
	uint64_t data;
	union dlb2_opaque_data u;
	uint8_t qid;
	uint8_t sched_type:2;
	uint8_t priority:3;
	uint8_t msg_type:3;
	uint16_t flow_id:16; /* was pp_id in v1 */
	uint8_t debug;
	uint8_t cq_gen:1;
	uint8_t qid_depth:2; /* 2 bits in v2 */
	uint8_t rsvd1:2;
	uint8_t error:1;
	uint8_t rsvd2:2;
};

union dlb2_port_config {
	struct dlb2_create_ldb_port_args ldb;
	struct dlb2_create_dir_port_args dir;
};

enum dlb2_port_state {
	PORT_CLOSED,
	PORT_STARTED,
	PORT_STOPPED
};

enum dlb2_configuration_state {
	/* The resource has not been configured */
	DLB2_NOT_CONFIGURED,
	/* The resource was configured, but the device was stopped */
	DLB2_PREV_CONFIGURED,
	/* The resource is currently configured */
	DLB2_CONFIGURED
};

struct dlb2_port {
	uint32_t id;
	bool is_directed;
	bool gen_bit;
	uint16_t dir_credits;
	uint32_t dequeue_depth;
	enum dlb2_token_pop_mode token_pop_mode;
	union dlb2_port_config cfg;
	uint32_t *credit_pool[DLB2_NUM_QUEUE_TYPES]; /* use __atomic builtins */
	uint16_t cached_ldb_credits;
	uint16_t ldb_credits;
	uint16_t cached_dir_credits;
	bool int_armed;
	uint16_t owed_tokens;
	int16_t issued_releases;
	int16_t token_pop_thresh;
	int cq_depth;
	uint16_t cq_idx;
	uint16_t cq_idx_unmasked;
	uint16_t cq_depth_mask;
	uint16_t gen_bit_shift;
	enum dlb2_port_state state;
	enum dlb2_configuration_state config_state;
	int num_mapped_qids;
	uint8_t *qid_mappings;
	struct dlb2_enqueue_qe *qe4; /* Cache line's worth of QEs (4) */
	struct dlb2_enqueue_qe *int_arm_qe;
	struct dlb2_cq_pop_qe *consume_qe;
	struct dlb2_eventdev *dlb2; /* back ptr */
	struct dlb2_eventdev_port *ev_port; /* back ptr */
};

/* Per-process per-port mmio and memory pointers */
struct process_local_port_data {
	uint64_t *pp_addr;
	struct dlb2_dequeue_qe *cq_base;
	const struct rte_memzone *mz;
	bool mmaped;
};

struct dlb2_eventdev;

struct dlb2_config {
	int configured;
	int reserved;
	uint32_t num_ldb_credits;
	uint32_t num_dir_credits;
	struct dlb2_create_sched_domain_args resources;
};

enum dlb2_cos {
	DLB2_COS_DEFAULT = -1,
	DLB2_COS_0 = 0,
	DLB2_COS_1,
	DLB2_COS_2,
	DLB2_COS_3
};

struct dlb2_hw_dev {
	struct dlb2_config cfg;
	struct dlb2_hw_resource_info info;
	void *pf_dev; /* opaque pointer to PF PMD dev (struct dlb2_dev) */
	uint32_t domain_id;
	enum dlb2_cos cos_id;
	rte_spinlock_t resource_lock; /* for MP support */
} __rte_cache_aligned;

/* End HW related defines and structs */

/* Begin DLB2 PMD Eventdev related defines and structs */

#define DLB2_MAX_NUM_QUEUES \
	(DLB2_MAX_NUM_DIR_QUEUES + DLB2_MAX_NUM_LDB_QUEUES)

#define DLB2_MAX_NUM_PORTS (DLB2_MAX_NUM_DIR_PORTS + DLB2_MAX_NUM_LDB_PORTS)
#define DLB2_MAX_INPUT_QUEUE_DEPTH 256

/** Structure to hold the queue to port link establishment attributes */

struct dlb2_event_queue_link {
	uint8_t queue_id;
	uint8_t priority;
	bool mapped;
	bool valid;
};

struct dlb2_traffic_stats {
	uint64_t rx_ok;
	uint64_t rx_drop;
	uint64_t rx_interrupt_wait;
	uint64_t rx_umonitor_umwait;
	uint64_t tx_ok;
	uint64_t total_polls;
	uint64_t zero_polls;
	uint64_t tx_nospc_ldb_hw_credits;
	uint64_t tx_nospc_dir_hw_credits;
	uint64_t tx_nospc_inflight_max;
	uint64_t tx_nospc_new_event_limit;
	uint64_t tx_nospc_inflight_credits;
};

/* DLB2 HW sets the 2bit qid_depth in rx QEs based on the programmable depth
 * threshold. The global default value in config/common_base (or rte_config.h)
 * can be overridden on a per-qid basis using a vdev command line parameter.
 * 3: depth > threshold
 * 2: threshold >= depth > 3/4 threshold
 * 1: 3/4 threshold >= depth > 1/2 threshold
 * 0: depth <= 1/2 threshold.
 */
#define DLB2_QID_DEPTH_LE50 0
#define DLB2_QID_DEPTH_GT50_LE75 1
#define DLB2_QID_DEPTH_GT75_LE100 2
#define DLB2_QID_DEPTH_GT100 3
#define DLB2_NUM_QID_DEPTH_STAT_VALS 4 /* 2 bits */

struct dlb2_queue_stats {
	uint64_t enq_ok;
	uint64_t qid_depth[DLB2_NUM_QID_DEPTH_STAT_VALS];
};

struct dlb2_port_stats {
	struct dlb2_traffic_stats traffic;
	uint64_t tx_op_cnt[4]; /* indexed by rte_event.op */
	uint64_t tx_implicit_rel;
	uint64_t tx_sched_cnt[DLB2_NUM_HW_SCHED_TYPES];
	uint64_t tx_invalid;
	uint64_t rx_sched_cnt[DLB2_NUM_HW_SCHED_TYPES];
	uint64_t rx_sched_invalid;
	struct dlb2_queue_stats queue[DLB2_MAX_NUM_QUEUES];
};

struct dlb2_eventdev_port {
	struct dlb2_port qm_port; /* hw specific data structure */
	struct rte_event_port_conf conf; /* user-supplied configuration */
	uint16_t inflight_credits; /* num credits this port has right now */
	uint16_t credit_update_quanta;
	struct dlb2_eventdev *dlb2; /* backlink optimization */
	struct dlb2_port_stats stats __rte_cache_aligned;
	struct dlb2_event_queue_link link[DLB2_MAX_NUM_QIDS_PER_LDB_CQ];
	int num_links;
	uint32_t id; /* port id */
	/* num releases yet to be completed on this port.
	 * Only applies to load-balanced ports.
	 */
	uint16_t outstanding_releases;
	uint16_t inflight_max; /* app requested max inflights for this port */
	/* setup_done is set when the event port is setup */
	bool setup_done;
	/* enq_configured is set when the qm port is created */
	bool enq_configured;
	uint8_t implicit_release; /* release events before dequeueing */
}  __rte_cache_aligned;

struct dlb2_queue {
	uint32_t num_qid_inflights; /* User config */
	uint32_t num_atm_inflights; /* User config */
	enum dlb2_configuration_state config_state;
	int sched_type; /* LB queue only */
	uint32_t id;
	bool is_directed;
};

struct dlb2_eventdev_queue {
	struct dlb2_queue qm_queue;
	struct rte_event_queue_conf conf; /* User config */
	int depth_threshold; /* use default if 0 */
	uint32_t id;
	bool setup_done;
	uint8_t num_links;
};

enum dlb2_run_state {
	DLB2_RUN_STATE_STOPPED = 0,
	DLB2_RUN_STATE_STOPPING,
	DLB2_RUN_STATE_STARTING,
	DLB2_RUN_STATE_STARTED
};

struct dlb2_eventdev {
	struct dlb2_eventdev_port ev_ports[DLB2_MAX_NUM_PORTS];
	struct dlb2_eventdev_queue ev_queues[DLB2_MAX_NUM_QUEUES];
	uint8_t qm_ldb_to_ev_queue_id[DLB2_MAX_NUM_QUEUES];
	uint8_t qm_dir_to_ev_queue_id[DLB2_MAX_NUM_QUEUES];
	/* store num stats and offset of the stats for each queue */
	uint16_t xstats_count_per_qid[DLB2_MAX_NUM_QUEUES];
	uint16_t xstats_offset_for_qid[DLB2_MAX_NUM_QUEUES];
	/* store num stats and offset of the stats for each port */
	uint16_t xstats_count_per_port[DLB2_MAX_NUM_PORTS];
	uint16_t xstats_offset_for_port[DLB2_MAX_NUM_PORTS];
	struct dlb2_get_num_resources_args hw_rsrc_query_results;
	uint32_t xstats_count_mode_queue;
	struct dlb2_hw_dev qm_instance; /* strictly hw related */
	uint64_t global_dequeue_wait_ticks;
	struct dlb2_xstats_entry *xstats;
	struct rte_eventdev *event_dev; /* backlink to dev */
	uint32_t xstats_count_mode_dev;
	uint32_t xstats_count_mode_port;
	uint32_t xstats_count;
	uint32_t inflights; /* use __atomic builtins */
	uint32_t new_event_limit;
	int max_num_events_override;
	int num_dir_credits_override;
	volatile enum dlb2_run_state run_state;
	uint16_t num_dir_queues; /* total num of evdev dir queues requested */
	uint16_t num_dir_credits;
	uint16_t num_ldb_credits;
	uint16_t num_queues; /* total queues */
	uint16_t num_ldb_queues; /* total num of evdev ldb queues requested */
	uint16_t num_ports; /* total num of evdev ports requested */
	uint16_t num_ldb_ports; /* total num of ldb ports requested */
	uint16_t num_dir_ports; /* total num of dir ports requested */
	bool umwait_allowed;
	bool global_dequeue_wait; /* Not using per dequeue wait if true */
	enum dlb2_cq_poll_modes poll_mode;
	uint8_t revision;
	bool configured;
	uint16_t max_ldb_credits;
	uint16_t max_dir_credits;

	/* force hw credit pool counters into exclusive cache lines */

	/* use __atomic builtins */ /* shared hw cred */
	uint32_t ldb_credit_pool __rte_cache_aligned;
	/* use __atomic builtins */ /* shared hw cred */
	uint32_t dir_credit_pool __rte_cache_aligned;
};

/* used for collecting and passing around the dev args */
struct dlb2_qid_depth_thresholds {
	int val[DLB2_MAX_NUM_QUEUES];
};

struct dlb2_devargs {
	int socket_id;
	int max_num_events;
	int num_dir_credits_override;
	int dev_id;
	struct dlb2_qid_depth_thresholds qid_depth_thresholds;
	enum dlb2_cos cos_id;
};

/* End Eventdev related defines and structs */

/* Forwards for non-inlined functions */

void dlb2_eventdev_dump(struct rte_eventdev *dev, FILE *f);

int dlb2_xstats_init(struct dlb2_eventdev *dlb2);

void dlb2_xstats_uninit(struct dlb2_eventdev *dlb2);

int dlb2_eventdev_xstats_get(const struct rte_eventdev *dev,
		enum rte_event_dev_xstats_mode mode, uint8_t queue_port_id,
		const unsigned int ids[], uint64_t values[], unsigned int n);

int dlb2_eventdev_xstats_get_names(const struct rte_eventdev *dev,
		enum rte_event_dev_xstats_mode mode, uint8_t queue_port_id,
		struct rte_event_dev_xstats_name *xstat_names,
		unsigned int *ids, unsigned int size);

uint64_t dlb2_eventdev_xstats_get_by_name(const struct rte_eventdev *dev,
					  const char *name, unsigned int *id);

int dlb2_eventdev_xstats_reset(struct rte_eventdev *dev,
		enum rte_event_dev_xstats_mode mode,
		int16_t queue_port_id,
		const uint32_t ids[],
		uint32_t nb_ids);

int test_dlb2_eventdev(void);

int dlb2_primary_eventdev_probe(struct rte_eventdev *dev,
				const char *name,
				struct dlb2_devargs *dlb2_args);

int dlb2_secondary_eventdev_probe(struct rte_eventdev *dev,
				  const char *name);

uint32_t dlb2_get_queue_depth(struct dlb2_eventdev *dlb2,
			      struct dlb2_eventdev_queue *queue);

int dlb2_parse_params(const char *params,
		      const char *name,
		      struct dlb2_devargs *dlb2_args);

/* Extern globals */
extern struct process_local_port_data dlb2_port[][DLB2_NUM_PORT_TYPES];

#endif	/* _DLB2_PRIV_H_ */
