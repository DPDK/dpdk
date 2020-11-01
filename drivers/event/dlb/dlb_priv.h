/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2020 Intel Corporation
 */

#ifndef _DLB_PRIV_H_
#define _DLB_PRIV_H_

#include <emmintrin.h>
#include <stdbool.h>

#include <rte_bus_pci.h>
#include <rte_eventdev.h>
#include <rte_eventdev_pmd.h>
#include <rte_eventdev_pmd_pci.h>
#include <rte_pci.h>

#include "dlb_user.h"
#include "dlb_log.h"
#include "rte_pmd_dlb.h"

#ifndef RTE_LIBRTE_PMD_DLB_QUELL_STATS
#define DLB_INC_STAT(_stat, _incr_val) ((_stat) += _incr_val)
#else
#define DLB_INC_STAT(_stat, _incr_val)
#endif

#define EVDEV_DLB_NAME_PMD_STR "dlb_event"

/* command line arg strings */
#define NUMA_NODE_ARG "numa_node"
#define DLB_MAX_NUM_EVENTS "max_num_events"
#define DLB_NUM_DIR_CREDITS "num_dir_credits"
#define DEV_ID_ARG "dev_id"
#define DLB_DEFER_SCHED_ARG "defer_sched"
#define DLB_NUM_ATM_INFLIGHTS_ARG "atm_inflights"

/* Begin HW related defines and structs */

#define DLB_MAX_NUM_DOMAINS 32
#define DLB_MAX_NUM_VFS 16
#define DLB_MAX_NUM_LDB_QUEUES 128
#define DLB_MAX_NUM_LDB_PORTS 64
#define DLB_MAX_NUM_DIR_PORTS 128
#define DLB_MAX_NUM_DIR_QUEUES 128
#define DLB_MAX_NUM_FLOWS (64 * 1024)
#define DLB_MAX_NUM_LDB_CREDITS 16384
#define DLB_MAX_NUM_DIR_CREDITS 4096
#define DLB_MAX_NUM_LDB_CREDIT_POOLS 64
#define DLB_MAX_NUM_DIR_CREDIT_POOLS 64
#define DLB_MAX_NUM_HIST_LIST_ENTRIES 5120
#define DLB_MAX_NUM_ATM_INFLIGHTS 2048
#define DLB_MAX_NUM_QIDS_PER_LDB_CQ 8
#define DLB_QID_PRIORITIES 8
#define DLB_MAX_DEVICE_PATH 32
#define DLB_MIN_DEQUEUE_TIMEOUT_NS 1
#define DLB_NUM_SN_GROUPS 4
#define DLB_MAX_LDB_SN_ALLOC 1024
/* Note: "- 1" here to support the timeout range check in eventdev_autotest */
#define DLB_MAX_DEQUEUE_TIMEOUT_NS (UINT32_MAX - 1)
#define DLB_DEF_UNORDERED_QID_INFLIGHTS 2048

/* 5120 total hist list entries and 64 total ldb ports, which
 * makes for 5120/64 == 80 hist list entries per port. However, CQ
 * depth must be a power of 2 and must also be >= HIST LIST entries.
 * As a result we just limit the maximum dequeue depth to 64.
 */
#define DLB_MIN_LDB_CQ_DEPTH 1
#define DLB_MIN_DIR_CQ_DEPTH 8
#define DLB_MIN_HARDWARE_CQ_DEPTH 8
#define DLB_MAX_CQ_DEPTH 64
#define DLB_NUM_HIST_LIST_ENTRIES_PER_LDB_PORT \
	DLB_MAX_CQ_DEPTH

/* Static per queue/port provisioning values */
#define DLB_NUM_ATOMIC_INFLIGHTS_PER_QUEUE 16

#define PP_BASE(is_dir) ((is_dir) ? DLB_DIR_PP_BASE : DLB_LDB_PP_BASE)

#define PAGE_SIZE (sysconf(_SC_PAGESIZE))

#define DLB_NUM_QES_PER_CACHE_LINE 4

#define DLB_MAX_ENQUEUE_DEPTH 64
#define DLB_MIN_ENQUEUE_DEPTH 4

#define DLB_NAME_SIZE 64

/* Use the upper 3 bits of the event priority to select the DLB priority */
#define EV_TO_DLB_PRIO(x) ((x) >> 5)
#define DLB_TO_EV_PRIO(x) ((x) << 5)

enum dlb_hw_port_type {
	DLB_LDB,
	DLB_DIR,

	/* NUM_DLB_PORT_TYPES must be last */
	NUM_DLB_PORT_TYPES
};

#define PORT_TYPE(p) ((p)->is_directed ? DLB_DIR : DLB_LDB)

/* Do not change - must match hardware! */
enum dlb_hw_sched_type {
	DLB_SCHED_ATOMIC = 0,
	DLB_SCHED_UNORDERED,
	DLB_SCHED_ORDERED,
	DLB_SCHED_DIRECTED,

	/* DLB_NUM_HW_SCHED_TYPES must be last */
	DLB_NUM_HW_SCHED_TYPES
};

struct dlb_devargs {
	int socket_id;
	int max_num_events;
	int num_dir_credits_override;
	int dev_id;
	int defer_sched;
	int num_atm_inflights;
};

struct dlb_hw_rsrcs {
	int32_t nb_events_limit;
	uint32_t num_queues;		/* Total queues (ldb + dir) */
	uint32_t num_ldb_queues;	/* Number of available ldb queues */
	uint32_t num_ldb_ports;         /* Number of load balanced ports */
	uint32_t num_dir_ports;         /* Number of directed ports */
	uint32_t num_ldb_credits;       /* Number of load balanced credits */
	uint32_t num_dir_credits;       /* Number of directed credits */
	uint32_t reorder_window_size;   /* Size of reorder window */
};

struct dlb_hw_resource_info {
	/**> Max resources that can be provided */
	struct dlb_hw_rsrcs hw_rsrc_max;
	int num_sched_domains;
	uint32_t socket_id;
	/**> EAL flags passed to this DLB instance, allowing the application to
	 * identify the pmd backend indicating hardware or software.
	 */
	const char *eal_flags;
};

/* hw-specific format - do not change */

struct dlb_event_type {
	uint8_t major:4;
	uint8_t unused:4;
	uint8_t sub;
};

union dlb_opaque_data {
	uint16_t opaque_data;
	struct dlb_event_type event_type;
};

struct dlb_msg_info {
	uint8_t qid;
	uint8_t sched_type:2;
	uint8_t priority:3;
	uint8_t msg_type:3;
};

#define DLB_NEW_CMD_BYTE 0x08
#define DLB_FWD_CMD_BYTE 0x0A
#define DLB_COMP_CMD_BYTE 0x02
#define DLB_NOOP_CMD_BYTE 0x00
#define DLB_POP_CMD_BYTE 0x01

/* hw-specific format - do not change */
struct dlb_enqueue_qe {
	uint64_t data;
	/* Word 3 */
	union dlb_opaque_data u;
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
			uint8_t int_arm:1;
			uint8_t error:1;
			uint8_t rsvd:2;
		};
	};
};

/* hw-specific format - do not change */
struct dlb_cq_pop_qe {
	uint64_t data;
	union dlb_opaque_data u;
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
			uint8_t int_arm:1;
			uint8_t error:1;
			uint8_t rsvd:2;
		};
	};
};

/* hw-specific format - do not change */
struct dlb_dequeue_qe {
	uint64_t data;
	union dlb_opaque_data u;
	uint8_t qid;
	uint8_t sched_type:2;
	uint8_t priority:3;
	uint8_t msg_type:3;
	uint16_t pp_id:10;
	uint16_t rsvd0:6;
	uint8_t debug;
	uint8_t cq_gen:1;
	uint8_t qid_depth:1;
	uint8_t rsvd1:3;
	uint8_t error:1;
	uint8_t rsvd2:2;
};

enum dlb_port_state {
	PORT_CLOSED,
	PORT_STARTED,
	PORT_STOPPED
};

enum dlb_configuration_state {
	/* The resource has not been configured */
	DLB_NOT_CONFIGURED,
	/* The resource was configured, but the device was stopped */
	DLB_PREV_CONFIGURED,
	/* The resource is currently configured */
	DLB_CONFIGURED
};

struct dlb_port {
	uint32_t id;
	bool is_directed;
	bool gen_bit;
	uint16_t dir_credits;
	uint32_t dequeue_depth;
	enum dlb_token_pop_mode token_pop_mode;
	int pp_mmio_base;
	uint16_t cached_ldb_credits;
	uint16_t ldb_pushcount_at_credit_expiry;
	uint16_t ldb_credits;
	uint16_t cached_dir_credits;
	uint16_t dir_pushcount_at_credit_expiry;
	bool int_armed;
	bool use_rsvd_token_scheme;
	uint8_t cq_rsvd_token_deficit;
	uint16_t owed_tokens;
	int16_t issued_releases;
	int16_t token_pop_thresh;
	int cq_depth;
	uint16_t cq_idx;
	uint16_t cq_idx_unmasked;
	uint16_t cq_depth_mask;
	uint16_t gen_bit_shift;
	enum dlb_port_state state;
	enum dlb_configuration_state config_state;
	int num_mapped_qids;
	uint8_t *qid_mappings;
	struct dlb_enqueue_qe *qe4; /* Cache line's worth of QEs (4) */
	struct dlb_cq_pop_qe *consume_qe;
	struct dlb_eventdev *dlb; /* back ptr */
	struct dlb_eventdev_port *ev_port; /* back ptr */
};

/* Per-process per-port mmio and memory pointers */
struct process_local_port_data {
	uint64_t *pp_addr;
	uint16_t *ldb_popcount;
	uint16_t *dir_popcount;
	struct dlb_dequeue_qe *cq_base;
	const struct rte_memzone *mz;
	bool mmaped;
};

struct dlb_config {
	int configured;
	int reserved;
	uint32_t ldb_credit_pool_id;
	uint32_t dir_credit_pool_id;
	uint32_t num_ldb_credits;
	uint32_t num_dir_credits;
	struct dlb_create_sched_domain_args resources;
};

struct dlb_hw_dev {
	struct dlb_config cfg;
	struct dlb_hw_resource_info info;
	void *pf_dev; /* opaque pointer to PF PMD dev (struct dlb_dev) */
	int device_id;
	uint32_t domain_id;
	int domain_id_valid;
	rte_spinlock_t resource_lock; /* for MP support */
} __rte_cache_aligned;

/* End HW related defines and structs */

/* Begin DLB PMD Eventdev related defines and structs */

#define DLB_MAX_NUM_QUEUES \
	(DLB_MAX_NUM_DIR_QUEUES + DLB_MAX_NUM_LDB_QUEUES)

#define DLB_MAX_NUM_PORTS (DLB_MAX_NUM_DIR_PORTS + DLB_MAX_NUM_LDB_PORTS)
#define DLB_MAX_INPUT_QUEUE_DEPTH 256

/** Structure to hold the queue to port link establishment attributes */

struct dlb_event_queue_link {
	uint8_t queue_id;
	uint8_t priority;
	bool mapped;
	bool valid;
};

struct dlb_traffic_stats {
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

struct dlb_port_stats {
	struct dlb_traffic_stats traffic;
	uint64_t tx_op_cnt[4]; /* indexed by rte_event.op */
	uint64_t tx_implicit_rel;
	uint64_t tx_sched_cnt[DLB_NUM_HW_SCHED_TYPES];
	uint64_t tx_invalid;
	uint64_t rx_sched_cnt[DLB_NUM_HW_SCHED_TYPES];
	uint64_t rx_sched_invalid;
	uint64_t enq_ok[DLB_MAX_NUM_QUEUES]; /* per-queue enq_ok */
};

struct dlb_eventdev_port {
	struct dlb_port qm_port; /* hw specific data structure */
	struct rte_event_port_conf conf; /* user-supplied configuration */
	uint16_t inflight_credits; /* num credits this port has right now */
	uint16_t credit_update_quanta;
	struct dlb_eventdev *dlb; /* backlink optimization */
	struct dlb_port_stats stats __rte_cache_aligned;
	struct dlb_event_queue_link link[DLB_MAX_NUM_QIDS_PER_LDB_CQ];
	int num_links;
	uint32_t id;
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
} __rte_cache_aligned;

struct dlb_queue {
	uint32_t num_qid_inflights; /* User config */
	uint32_t num_atm_inflights; /* User config */
	enum dlb_configuration_state config_state;
	int sched_type; /* LB queue only */
	uint32_t id;
	bool is_directed;
};

struct dlb_eventdev_queue {
	struct dlb_queue qm_queue;
	struct rte_event_queue_conf conf; /* User config */
	uint64_t enq_ok;
	uint32_t id;
	bool setup_done;
	uint8_t num_links;
};

enum dlb_run_state {
	DLB_RUN_STATE_STOPPED = 0,
	DLB_RUN_STATE_STOPPING,
	DLB_RUN_STATE_STARTING,
	DLB_RUN_STATE_STARTED
};

struct dlb_eventdev {
	struct dlb_eventdev_port ev_ports[DLB_MAX_NUM_PORTS];
	struct dlb_eventdev_queue ev_queues[DLB_MAX_NUM_QUEUES];
	uint8_t qm_ldb_to_ev_queue_id[DLB_MAX_NUM_QUEUES];
	uint8_t qm_dir_to_ev_queue_id[DLB_MAX_NUM_QUEUES];

	/* store num stats and offset of the stats for each queue */
	uint16_t xstats_count_per_qid[DLB_MAX_NUM_QUEUES];
	uint16_t xstats_offset_for_qid[DLB_MAX_NUM_QUEUES];

	/* store num stats and offset of the stats for each port */
	uint16_t xstats_count_per_port[DLB_MAX_NUM_PORTS];
	uint16_t xstats_offset_for_port[DLB_MAX_NUM_PORTS];
	struct dlb_get_num_resources_args hw_rsrc_query_results;
	uint32_t xstats_count_mode_queue;
	struct dlb_hw_dev qm_instance; /* strictly hw related */
	uint64_t global_dequeue_wait_ticks;
	struct dlb_xstats_entry *xstats;
	struct rte_eventdev *event_dev; /* backlink to dev */
	uint32_t xstats_count_mode_port;
	uint32_t xstats_count_mode_dev;
	uint32_t xstats_count;
	uint32_t inflights; /* use __atomic builtins to access */
	uint32_t new_event_limit;
	int max_num_events_override;
	int num_dir_credits_override;
	volatile enum dlb_run_state run_state;
	uint16_t num_dir_queues; /* total num of evdev dir queues requested */
	uint16_t num_dir_credits;
	uint16_t num_ldb_credits;
	uint16_t num_queues; /* total queues */
	uint16_t num_ldb_queues; /* total num of evdev ldb queues requested */
	uint16_t num_ports; /* total num of evdev ports requested */
	uint16_t num_ldb_ports; /* total num of ldb ports requested */
	uint16_t num_dir_ports; /* total num of dir ports requested */
	bool is_vdev;
	bool umwait_allowed;
	bool global_dequeue_wait; /* Not using per dequeue wait if true */
	bool defer_sched;
	unsigned int num_atm_inflights_per_queue;
	enum dlb_cq_poll_modes poll_mode;
	uint8_t revision;
	bool configured;
};

/* End Eventdev related defines and structs */

/* externs */

extern struct process_local_port_data dlb_port[][NUM_DLB_PORT_TYPES];

/* Forwards for non-inlined functions */

void dlb_eventdev_dump(struct rte_eventdev *dev, FILE *f);

int dlb_xstats_init(struct dlb_eventdev *dlb);

void dlb_xstats_uninit(struct dlb_eventdev *dlb);

int dlb_eventdev_xstats_get(const struct rte_eventdev *dev,
			    enum rte_event_dev_xstats_mode mode,
			    uint8_t queue_port_id, const unsigned int ids[],
			    uint64_t values[], unsigned int n);

int dlb_eventdev_xstats_get_names(const struct rte_eventdev *dev,
				  enum rte_event_dev_xstats_mode mode,
				  uint8_t queue_port_id,
				  struct rte_event_dev_xstats_name *xstat_names,
				  unsigned int *ids, unsigned int size);

uint64_t dlb_eventdev_xstats_get_by_name(const struct rte_eventdev *dev,
					 const char *name, unsigned int *id);

int dlb_eventdev_xstats_reset(struct rte_eventdev *dev,
			      enum rte_event_dev_xstats_mode mode,
			      int16_t queue_port_id,
			      const uint32_t ids[],
			      uint32_t nb_ids);

int test_dlb_eventdev(void);

int dlb_primary_eventdev_probe(struct rte_eventdev *dev,
			       const char *name,
			       struct dlb_devargs *dlb_args);

int dlb_secondary_eventdev_probe(struct rte_eventdev *dev,
				 const char *name);

uint32_t dlb_get_queue_depth(struct dlb_eventdev *dlb,
			     struct dlb_eventdev_queue *queue);

int dlb_parse_params(const char *params,
		     const char *name,
		     struct dlb_devargs *dlb_args);

void dlb_entry_points_init(struct rte_eventdev *dev);

#endif	/* _DLB_PRIV_H_ */
