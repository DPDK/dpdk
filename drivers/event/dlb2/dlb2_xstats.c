/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2020 Intel Corporation
 */

#include <inttypes.h>

#include <rte_malloc.h>
#include <rte_eventdev.h>

#include "dlb2_priv.h"
#include "dlb2_inline_fns.h"
#include "pf/base/dlb2_regs.h"

enum dlb2_xstats_type {
	/* common to device and port */
	rx_ok,				/**< Receive an event */
	rx_drop,                        /**< Error bit set in received QE */
	rx_interrupt_wait,		/**< Wait on an interrupt */
	rx_umonitor_umwait,		/**< Block using umwait */
	tx_ok,				/**< Transmit an event */
	total_polls,			/**< Call dequeue_burst */
	zero_polls,			/**< Call dequeue burst and return 0 */
	tx_nospc_ldb_hw_credits,	/**< Insufficient LDB h/w credits */
	tx_nospc_dir_hw_credits,	/**< Insufficient DIR h/w credits */
	tx_nospc_hw_credits,		/**< Insufficient h/w credits */
	tx_nospc_inflight_max,		/**< Reach the new_event_threshold */
	tx_nospc_new_event_limit,	/**< Insufficient s/w credits */
	tx_nospc_inflight_credits,	/**< Port has too few s/w credits */
	/* device specific */
	nb_events_limit,
	inflight_events,
	ldb_pool_size,
	dir_pool_size,
	pool_size,
	/* port specific */
	tx_new,				/**< Send an OP_NEW event */
	tx_fwd,				/**< Send an OP_FORWARD event */
	tx_rel,				/**< Send an OP_RELEASE event */
	tx_implicit_rel,		/**< Issue an implicit event release */
	tx_sched_ordered,		/**< Send a SCHED_TYPE_ORDERED event */
	tx_sched_unordered,		/**< Send a SCHED_TYPE_PARALLEL event */
	tx_sched_atomic,		/**< Send a SCHED_TYPE_ATOMIC event */
	tx_sched_directed,		/**< Send a directed event */
	tx_invalid,                     /**< Send an event with an invalid op */
	outstanding_releases,		/**< # of releases a port owes */
	max_outstanding_releases,	/**< max # of releases a port can owe */
	rx_sched_ordered,		/**< Dequeue an ordered event */
	rx_sched_unordered,		/**< Dequeue an unordered event */
	rx_sched_atomic,		/**< Dequeue an atomic event */
	rx_sched_directed,		/**< Dequeue an directed event */
	rx_sched_invalid,               /**< Dequeue event sched type invalid */
	/* common to port and queue */
	is_configured,			/**< Port is configured */
	is_load_balanced,		/**< Port is LDB */
	hw_id,				/**< Hardware ID */
	/* queue specific */
	num_links,			/**< Number of ports linked */
	sched_type,			/**< Queue sched type */
	enq_ok,				/**< # events enqueued to the queue */
	current_depth,			/**< Current queue depth */
	depth_threshold,		/**< Programmed depth threshold */
	depth_le50_threshold,
	/**< Depth LE to 50% of the configured hardware threshold */
	depth_gt50_le75_threshold,
	/**< Depth GT 50%, but LE to 75% of the configured hardware threshold */
	depth_gt75_le100_threshold,
	/**< Depth GT 75%. but LE to the configured hardware threshold */
	depth_gt100_threshold
	/**< Depth GT 100% of the configured hw threshold */
};

typedef uint64_t (*dlb2_xstats_fn)(struct dlb2_eventdev *dlb2,
		uint16_t obj_idx, /* port or queue id */
		enum dlb2_xstats_type stat, int extra_arg);

enum dlb2_xstats_fn_type {
	DLB2_XSTATS_FN_DEV,
	DLB2_XSTATS_FN_PORT,
	DLB2_XSTATS_FN_QUEUE
};

struct dlb2_xstats_entry {
	struct rte_event_dev_xstats_name name;
	uint64_t reset_value; /* an offset to be taken away to emulate resets */
	enum dlb2_xstats_fn_type fn_id;
	enum dlb2_xstats_type stat;
	enum rte_event_dev_xstats_mode mode;
	int extra_arg;
	uint16_t obj_idx;
	uint8_t reset_allowed; /* when set, this value can be reset */
};

/* Some device stats are simply a summation of the corresponding port values */
static uint64_t
dlb2_device_traffic_stat_get(struct dlb2_eventdev *dlb2,
			     int which_stat)
{
	int i;
	uint64_t val = 0;

	for (i = 0; i < DLB2_MAX_NUM_PORTS(dlb2->version); i++) {
		struct dlb2_eventdev_port *port = &dlb2->ev_ports[i];

		if (!port->setup_done)
			continue;

		switch (which_stat) {
		case rx_ok:
			val += port->stats.traffic.rx_ok;
			break;
		case rx_drop:
			val += port->stats.traffic.rx_drop;
			break;
		case rx_interrupt_wait:
			val += port->stats.traffic.rx_interrupt_wait;
			break;
		case rx_umonitor_umwait:
			val += port->stats.traffic.rx_umonitor_umwait;
			break;
		case tx_ok:
			val += port->stats.traffic.tx_ok;
			break;
		case total_polls:
			val += port->stats.traffic.total_polls;
			break;
		case zero_polls:
			val += port->stats.traffic.zero_polls;
			break;
		case tx_nospc_ldb_hw_credits:
			val += port->stats.traffic.tx_nospc_ldb_hw_credits;
			break;
		case tx_nospc_dir_hw_credits:
			val += port->stats.traffic.tx_nospc_dir_hw_credits;
			break;
		case tx_nospc_hw_credits:
			val += port->stats.traffic.tx_nospc_hw_credits;
			break;
		case tx_nospc_inflight_max:
			val += port->stats.traffic.tx_nospc_inflight_max;
			break;
		case tx_nospc_new_event_limit:
			val += port->stats.traffic.tx_nospc_new_event_limit;
			break;
		case tx_nospc_inflight_credits:
			val += port->stats.traffic.tx_nospc_inflight_credits;
			break;
		default:
			return -1;
		}
	}
	return val;
}

static uint64_t
get_dev_stat(struct dlb2_eventdev *dlb2, uint16_t obj_idx __rte_unused,
	     enum dlb2_xstats_type type, int extra_arg __rte_unused)
{
	switch (type) {
	case rx_ok:
	case rx_drop:
	case rx_interrupt_wait:
	case rx_umonitor_umwait:
	case tx_ok:
	case total_polls:
	case zero_polls:
	case tx_nospc_ldb_hw_credits:
	case tx_nospc_dir_hw_credits:
	case tx_nospc_hw_credits:
	case tx_nospc_inflight_max:
	case tx_nospc_new_event_limit:
	case tx_nospc_inflight_credits:
		return dlb2_device_traffic_stat_get(dlb2, type);
	case nb_events_limit:
		return dlb2->new_event_limit;
	case inflight_events:
		return __atomic_load_n(&dlb2->inflights, __ATOMIC_SEQ_CST);
	case ldb_pool_size:
		return dlb2->num_ldb_credits;
	case dir_pool_size:
		return dlb2->num_dir_credits;
	case pool_size:
		return dlb2->num_credits;
	default: return -1;
	}
}

static uint64_t
get_port_stat(struct dlb2_eventdev *dlb2, uint16_t obj_idx,
	      enum dlb2_xstats_type type, int extra_arg __rte_unused)
{
	struct dlb2_eventdev_port *ev_port = &dlb2->ev_ports[obj_idx];

	switch (type) {
	case rx_ok: return ev_port->stats.traffic.rx_ok;

	case rx_drop: return ev_port->stats.traffic.rx_drop;

	case rx_interrupt_wait: return ev_port->stats.traffic.rx_interrupt_wait;

	case rx_umonitor_umwait:
		return ev_port->stats.traffic.rx_umonitor_umwait;

	case tx_ok: return ev_port->stats.traffic.tx_ok;

	case total_polls: return ev_port->stats.traffic.total_polls;

	case zero_polls: return ev_port->stats.traffic.zero_polls;

	case tx_nospc_ldb_hw_credits:
		return ev_port->stats.traffic.tx_nospc_ldb_hw_credits;

	case tx_nospc_dir_hw_credits:
		return ev_port->stats.traffic.tx_nospc_dir_hw_credits;

	case tx_nospc_hw_credits:
		return ev_port->stats.traffic.tx_nospc_hw_credits;

	case tx_nospc_inflight_max:
		return ev_port->stats.traffic.tx_nospc_inflight_max;

	case tx_nospc_new_event_limit:
		return ev_port->stats.traffic.tx_nospc_new_event_limit;

	case tx_nospc_inflight_credits:
		return ev_port->stats.traffic.tx_nospc_inflight_credits;

	case is_configured: return ev_port->setup_done;

	case is_load_balanced: return !ev_port->qm_port.is_directed;

	case hw_id: return ev_port->qm_port.id;

	case tx_new: return ev_port->stats.tx_op_cnt[RTE_EVENT_OP_NEW];

	case tx_fwd: return ev_port->stats.tx_op_cnt[RTE_EVENT_OP_FORWARD];

	case tx_rel: return ev_port->stats.tx_op_cnt[RTE_EVENT_OP_RELEASE];

	case tx_implicit_rel: return ev_port->stats.tx_implicit_rel;

	case tx_sched_ordered:
		return ev_port->stats.tx_sched_cnt[DLB2_SCHED_ORDERED];

	case tx_sched_unordered:
		return ev_port->stats.tx_sched_cnt[DLB2_SCHED_UNORDERED];

	case tx_sched_atomic:
		return ev_port->stats.tx_sched_cnt[DLB2_SCHED_ATOMIC];

	case tx_sched_directed:
		return ev_port->stats.tx_sched_cnt[DLB2_SCHED_DIRECTED];

	case tx_invalid: return ev_port->stats.tx_invalid;

	case outstanding_releases: return ev_port->outstanding_releases;

	case max_outstanding_releases:
		return DLB2_NUM_HIST_LIST_ENTRIES_PER_LDB_PORT;

	case rx_sched_ordered:
		return ev_port->stats.rx_sched_cnt[DLB2_SCHED_ORDERED];

	case rx_sched_unordered:
		return ev_port->stats.rx_sched_cnt[DLB2_SCHED_UNORDERED];

	case rx_sched_atomic:
		return ev_port->stats.rx_sched_cnt[DLB2_SCHED_ATOMIC];

	case rx_sched_directed:
		return ev_port->stats.rx_sched_cnt[DLB2_SCHED_DIRECTED];

	case rx_sched_invalid: return ev_port->stats.rx_sched_invalid;

	default: return -1;
	}
}

static uint64_t
dlb2_get_threshold_stat(struct dlb2_eventdev *dlb2, int qid, int stat)
{
	int port = 0;
	uint64_t tally = 0;

	for (port = 0; port < DLB2_MAX_NUM_PORTS(dlb2->version); port++)
		tally += dlb2->ev_ports[port].stats.queue[qid].qid_depth[stat];

	return tally;
}

static uint64_t
dlb2_get_enq_ok_stat(struct dlb2_eventdev *dlb2, int qid)
{
	int port = 0;
	uint64_t enq_ok_tally = 0;

	for (port = 0; port < DLB2_MAX_NUM_PORTS(dlb2->version); port++)
		enq_ok_tally += dlb2->ev_ports[port].stats.queue[qid].enq_ok;

	return enq_ok_tally;
}

static uint64_t
get_queue_stat(struct dlb2_eventdev *dlb2, uint16_t obj_idx,
	       enum dlb2_xstats_type type, int extra_arg __rte_unused)
{
	struct dlb2_eventdev_queue *ev_queue =
		&dlb2->ev_queues[obj_idx];

	switch (type) {
	case is_configured: return ev_queue->setup_done;

	case is_load_balanced: return !ev_queue->qm_queue.is_directed;

	case hw_id: return ev_queue->qm_queue.id;

	case num_links: return ev_queue->num_links;

	case sched_type: return ev_queue->qm_queue.sched_type;

	case enq_ok: return dlb2_get_enq_ok_stat(dlb2, obj_idx);

	case current_depth: return dlb2_get_queue_depth(dlb2, ev_queue);

	case depth_threshold: return ev_queue->depth_threshold;

	case depth_le50_threshold:
		return dlb2_get_threshold_stat(dlb2, ev_queue->id,
					       DLB2_QID_DEPTH_LE50);

	case depth_gt50_le75_threshold:
		return dlb2_get_threshold_stat(dlb2, ev_queue->id,
					       DLB2_QID_DEPTH_GT50_LE75);

	case depth_gt75_le100_threshold:
		return dlb2_get_threshold_stat(dlb2, ev_queue->id,
					       DLB2_QID_DEPTH_GT75_LE100);

	case depth_gt100_threshold:
		return dlb2_get_threshold_stat(dlb2, ev_queue->id,
					       DLB2_QID_DEPTH_GT100);

	default: return -1;
	}
}

int
dlb2_xstats_init(struct dlb2_eventdev *dlb2)
{
	/*
	 * define the stats names and types. Used to build up the device
	 * xstats array
	 * There are multiple set of stats:
	 *   - device-level,
	 *   - per-port,
	 *   - per-qid,
	 *
	 * For each of these sets, we have three parallel arrays, one for the
	 * names, the other for the stat type parameter to be passed in the fn
	 * call to get that stat. The third array allows resetting or not.
	 * All these arrays must be kept in sync
	 */
	static const char * const dev_stats[] = {
		"rx_ok",
		"rx_drop",
		"rx_interrupt_wait",
		"rx_umonitor_umwait",
		"tx_ok",
		"total_polls",
		"zero_polls",
		"tx_nospc_ldb_hw_credits",
		"tx_nospc_dir_hw_credits",
		"tx_nospc_hw_credits",
		"tx_nospc_inflight_max",
		"tx_nospc_new_event_limit",
		"tx_nospc_inflight_credits",
		"nb_events_limit",
		"inflight_events",
		"ldb_pool_size",
		"dir_pool_size",
		"pool_size",
	};
	static const enum dlb2_xstats_type dev_types[] = {
		rx_ok,
		rx_drop,
		rx_interrupt_wait,
		rx_umonitor_umwait,
		tx_ok,
		total_polls,
		zero_polls,
		tx_nospc_ldb_hw_credits,
		tx_nospc_dir_hw_credits,
		tx_nospc_hw_credits,
		tx_nospc_inflight_max,
		tx_nospc_new_event_limit,
		tx_nospc_inflight_credits,
		nb_events_limit,
		inflight_events,
		ldb_pool_size,
		dir_pool_size,
		pool_size,
	};
	/* Note: generated device stats are not allowed to be reset. */
	static const uint8_t dev_reset_allowed[] = {
		0, /* rx_ok */
		0, /* rx_drop */
		0, /* rx_interrupt_wait */
		0, /* rx_umonitor_umwait */
		0, /* tx_ok */
		0, /* total_polls */
		0, /* zero_polls */
		0, /* tx_nospc_ldb_hw_credits */
		0, /* tx_nospc_dir_hw_credits */
		0, /* tx_nospc_hw_credits */
		0, /* tx_nospc_inflight_max */
		0, /* tx_nospc_new_event_limit */
		0, /* tx_nospc_inflight_credits */
		0, /* nb_events_limit */
		0, /* inflight_events */
		0, /* ldb_pool_size */
		0, /* dir_pool_size */
		0, /* pool_size */
	};
	static const char * const port_stats[] = {
		"is_configured",
		"is_load_balanced",
		"hw_id",
		"rx_ok",
		"rx_drop",
		"rx_interrupt_wait",
		"rx_umonitor_umwait",
		"tx_ok",
		"total_polls",
		"zero_polls",
		"tx_nospc_ldb_hw_credits",
		"tx_nospc_dir_hw_credits",
		"tx_nospc_hw_credits",
		"tx_nospc_inflight_max",
		"tx_nospc_new_event_limit",
		"tx_nospc_inflight_credits",
		"tx_new",
		"tx_fwd",
		"tx_rel",
		"tx_implicit_rel",
		"tx_sched_ordered",
		"tx_sched_unordered",
		"tx_sched_atomic",
		"tx_sched_directed",
		"tx_invalid",
		"outstanding_releases",
		"max_outstanding_releases",
		"rx_sched_ordered",
		"rx_sched_unordered",
		"rx_sched_atomic",
		"rx_sched_directed",
		"rx_sched_invalid"
	};
	static const enum dlb2_xstats_type port_types[] = {
		is_configured,
		is_load_balanced,
		hw_id,
		rx_ok,
		rx_drop,
		rx_interrupt_wait,
		rx_umonitor_umwait,
		tx_ok,
		total_polls,
		zero_polls,
		tx_nospc_ldb_hw_credits,
		tx_nospc_dir_hw_credits,
		tx_nospc_hw_credits,
		tx_nospc_inflight_max,
		tx_nospc_new_event_limit,
		tx_nospc_inflight_credits,
		tx_new,
		tx_fwd,
		tx_rel,
		tx_implicit_rel,
		tx_sched_ordered,
		tx_sched_unordered,
		tx_sched_atomic,
		tx_sched_directed,
		tx_invalid,
		outstanding_releases,
		max_outstanding_releases,
		rx_sched_ordered,
		rx_sched_unordered,
		rx_sched_atomic,
		rx_sched_directed,
		rx_sched_invalid
	};
	static const uint8_t port_reset_allowed[] = {
		0, /* is_configured */
		0, /* is_load_balanced */
		0, /* hw_id */
		1, /* rx_ok */
		1, /* rx_drop */
		1, /* rx_interrupt_wait */
		1, /* rx_umonitor_umwait */
		1, /* tx_ok */
		1, /* total_polls */
		1, /* zero_polls */
		1, /* tx_nospc_ldb_hw_credits */
		1, /* tx_nospc_dir_hw_credits */
		1, /* tx_nospc_hw_credits */
		1, /* tx_nospc_inflight_max */
		1, /* tx_nospc_new_event_limit */
		1, /* tx_nospc_inflight_credits */
		1, /* tx_new */
		1, /* tx_fwd */
		1, /* tx_rel */
		1, /* tx_implicit_rel */
		1, /* tx_sched_ordered */
		1, /* tx_sched_unordered */
		1, /* tx_sched_atomic */
		1, /* tx_sched_directed */
		1, /* tx_invalid */
		0, /* outstanding_releases */
		0, /* max_outstanding_releases */
		1, /* rx_sched_ordered */
		1, /* rx_sched_unordered */
		1, /* rx_sched_atomic */
		1, /* rx_sched_directed */
		1  /* rx_sched_invalid */
	};

	/* QID specific stats */
	static const char * const qid_stats[] = {
		"is_configured",
		"is_load_balanced",
		"hw_id",
		"num_links",
		"sched_type",
		"enq_ok",
		"current_depth",
		"depth_threshold",
		"depth_le50_threshold",
		"depth_gt50_le75_threshold",
		"depth_gt75_le100_threshold",
		"depth_gt100_threshold",
	};
	static const enum dlb2_xstats_type qid_types[] = {
		is_configured,
		is_load_balanced,
		hw_id,
		num_links,
		sched_type,
		enq_ok,
		current_depth,
		depth_threshold,
		depth_le50_threshold,
		depth_gt50_le75_threshold,
		depth_gt75_le100_threshold,
		depth_gt100_threshold,
	};
	static const uint8_t qid_reset_allowed[] = {
		0, /* is_configured */
		0, /* is_load_balanced */
		0, /* hw_id */
		0, /* num_links */
		0, /* sched_type */
		1, /* enq_ok */
		0, /* current_depth */
		0, /* depth_threshold */
		1, /* depth_le50_threshold */
		1, /* depth_gt50_le75_threshold */
		1, /* depth_gt75_le100_threshold */
		1, /* depth_gt100_threshold */
	};

	/* ---- end of stat definitions ---- */

	/* check sizes, since a missed comma can lead to strings being
	 * joined by the compiler.
	 */
	RTE_BUILD_BUG_ON(RTE_DIM(dev_stats) != RTE_DIM(dev_types));
	RTE_BUILD_BUG_ON(RTE_DIM(port_stats) != RTE_DIM(port_types));
	RTE_BUILD_BUG_ON(RTE_DIM(qid_stats) != RTE_DIM(qid_types));

	RTE_BUILD_BUG_ON(RTE_DIM(dev_stats) != RTE_DIM(dev_reset_allowed));
	RTE_BUILD_BUG_ON(RTE_DIM(port_stats) != RTE_DIM(port_reset_allowed));
	RTE_BUILD_BUG_ON(RTE_DIM(qid_stats) != RTE_DIM(qid_reset_allowed));

	/* other vars */
	const unsigned int count = RTE_DIM(dev_stats) +
		DLB2_MAX_NUM_PORTS(dlb2->version) * RTE_DIM(port_stats) +
		DLB2_MAX_NUM_QUEUES(dlb2->version) * RTE_DIM(qid_stats);
	unsigned int i, port, qid, stat_id = 0;

	dlb2->xstats = rte_zmalloc_socket(NULL,
			sizeof(dlb2->xstats[0]) * count, 0,
			dlb2->qm_instance.info.socket_id);
	if (dlb2->xstats == NULL)
		return -ENOMEM;

#define sname dlb2->xstats[stat_id].name.name
	for (i = 0; i < RTE_DIM(dev_stats); i++, stat_id++) {
		dlb2->xstats[stat_id] = (struct dlb2_xstats_entry) {
			.fn_id = DLB2_XSTATS_FN_DEV,
			.stat = dev_types[i],
			.mode = RTE_EVENT_DEV_XSTATS_DEVICE,
			.reset_allowed = dev_reset_allowed[i],
		};
		snprintf(sname, sizeof(sname), "dev_%s", dev_stats[i]);
	}
	dlb2->xstats_count_mode_dev = stat_id;

	for (port = 0; port < DLB2_MAX_NUM_PORTS(dlb2->version); port++) {
		dlb2->xstats_offset_for_port[port] = stat_id;

		uint32_t count_offset = stat_id;

		for (i = 0; i < RTE_DIM(port_stats); i++, stat_id++) {
			dlb2->xstats[stat_id] = (struct dlb2_xstats_entry){
				.fn_id = DLB2_XSTATS_FN_PORT,
				.obj_idx = port,
				.stat = port_types[i],
				.mode = RTE_EVENT_DEV_XSTATS_PORT,
				.reset_allowed = port_reset_allowed[i],
			};
			snprintf(sname, sizeof(sname), "port_%u_%s",
				 port, port_stats[i]);
		}

		dlb2->xstats_count_per_port[port] = stat_id - count_offset;
	}

	dlb2->xstats_count_mode_port = stat_id - dlb2->xstats_count_mode_dev;

	for (qid = 0; qid < DLB2_MAX_NUM_QUEUES(dlb2->version); qid++) {
		uint32_t count_offset = stat_id;

		dlb2->xstats_offset_for_qid[qid] = stat_id;

		for (i = 0; i < RTE_DIM(qid_stats); i++, stat_id++) {
			dlb2->xstats[stat_id] = (struct dlb2_xstats_entry){
				.fn_id = DLB2_XSTATS_FN_QUEUE,
				.obj_idx = qid,
				.stat = qid_types[i],
				.mode = RTE_EVENT_DEV_XSTATS_QUEUE,
				.reset_allowed = qid_reset_allowed[i],
			};
			snprintf(sname, sizeof(sname), "qid_%u_%s",
				 qid, qid_stats[i]);
		}

		dlb2->xstats_count_per_qid[qid] = stat_id - count_offset;
	}

	dlb2->xstats_count_mode_queue = stat_id -
		(dlb2->xstats_count_mode_dev + dlb2->xstats_count_mode_port);
#undef sname

	dlb2->xstats_count = stat_id;

	return 0;
}

void
dlb2_xstats_uninit(struct dlb2_eventdev *dlb2)
{
	rte_free(dlb2->xstats);
	dlb2->xstats_count = 0;
}

int
dlb2_eventdev_xstats_get_names(const struct rte_eventdev *dev,
		enum rte_event_dev_xstats_mode mode, uint8_t queue_port_id,
		struct rte_event_dev_xstats_name *xstats_names,
		uint64_t *ids, unsigned int size)
{
	const struct dlb2_eventdev *dlb2 = dlb2_pmd_priv(dev);
	unsigned int i;
	unsigned int xidx = 0;
	uint32_t xstats_mode_count = 0;
	uint32_t start_offset = 0;

	switch (mode) {
	case RTE_EVENT_DEV_XSTATS_DEVICE:
		xstats_mode_count = dlb2->xstats_count_mode_dev;
		break;
	case RTE_EVENT_DEV_XSTATS_PORT:
		if (queue_port_id >= DLB2_MAX_NUM_PORTS(dlb2->version))
			break;
		xstats_mode_count = dlb2->xstats_count_per_port[queue_port_id];
		start_offset = dlb2->xstats_offset_for_port[queue_port_id];
		break;
	case RTE_EVENT_DEV_XSTATS_QUEUE:
		if (queue_port_id >= DLB2_MAX_NUM_QUEUES(dlb2->version) &&
		    (DLB2_MAX_NUM_QUEUES(dlb2->version) <= 255))
			break;
		xstats_mode_count = dlb2->xstats_count_per_qid[queue_port_id];
		start_offset = dlb2->xstats_offset_for_qid[queue_port_id];
		break;
	default:
		return -EINVAL;
	};

	if (xstats_mode_count > size || ids == NULL || xstats_names == NULL)
		return xstats_mode_count;

	for (i = 0; i < dlb2->xstats_count && xidx < size; i++) {
		if (dlb2->xstats[i].mode != mode)
			continue;

		if (mode != RTE_EVENT_DEV_XSTATS_DEVICE &&
		    queue_port_id != dlb2->xstats[i].obj_idx)
			continue;

		xstats_names[xidx] = dlb2->xstats[i].name;
		if (ids)
			ids[xidx] = start_offset + xidx;
		xidx++;
	}
	return xidx;
}

static int
dlb2_xstats_update(struct dlb2_eventdev *dlb2,
		enum rte_event_dev_xstats_mode mode,
		uint8_t queue_port_id, const uint64_t ids[],
		uint64_t values[], unsigned int n, const uint32_t reset)
{
	unsigned int i;
	unsigned int xidx = 0;
	uint32_t xstats_mode_count = 0;

	switch (mode) {
	case RTE_EVENT_DEV_XSTATS_DEVICE:
		xstats_mode_count = dlb2->xstats_count_mode_dev;
		break;
	case RTE_EVENT_DEV_XSTATS_PORT:
		if (queue_port_id >= DLB2_MAX_NUM_PORTS(dlb2->version))
			goto invalid_value;
		xstats_mode_count = dlb2->xstats_count_per_port[queue_port_id];
		break;
	case RTE_EVENT_DEV_XSTATS_QUEUE:
#if (DLB2_MAX_NUM_QUEUES(DLB2_HW_V2_5) <= 255) /* max 8 bit value */
		if (queue_port_id >= DLB2_MAX_NUM_QUEUES(dlb2->version))
			goto invalid_value;
#endif
		xstats_mode_count = dlb2->xstats_count_per_qid[queue_port_id];
		break;
	default:
		goto invalid_value;
	};

	for (i = 0; i < n && xidx < xstats_mode_count; i++) {
		struct dlb2_xstats_entry *xs = &dlb2->xstats[ids[i]];
		dlb2_xstats_fn fn;

		if (ids[i] > dlb2->xstats_count || xs->mode != mode)
			continue;

		if (mode != RTE_EVENT_DEV_XSTATS_DEVICE &&
		    queue_port_id != xs->obj_idx)
			continue;

		switch (xs->fn_id) {
		case DLB2_XSTATS_FN_DEV:
			fn = get_dev_stat;
			break;
		case DLB2_XSTATS_FN_PORT:
			fn = get_port_stat;
			break;
		case DLB2_XSTATS_FN_QUEUE:
			fn = get_queue_stat;
			break;
		default:
			DLB2_LOG_ERR("Unexpected xstat fn_id %d\n", xs->fn_id);
			goto invalid_value;
		}

		uint64_t val = fn(dlb2, xs->obj_idx, xs->stat,
				  xs->extra_arg) - xs->reset_value;

		if (values)
			values[xidx] = val;

		if (xs->reset_allowed && reset)
			xs->reset_value += val;

		xidx++;
	}

	return xidx;

invalid_value:
	return -EINVAL;
}

int
dlb2_eventdev_xstats_get(const struct rte_eventdev *dev,
		enum rte_event_dev_xstats_mode mode, uint8_t queue_port_id,
		const uint64_t ids[], uint64_t values[], unsigned int n)
{
	struct dlb2_eventdev *dlb2 = dlb2_pmd_priv(dev);
	const uint32_t reset = 0;

	return dlb2_xstats_update(dlb2, mode, queue_port_id, ids, values, n,
				  reset);
}

uint64_t
dlb2_eventdev_xstats_get_by_name(const struct rte_eventdev *dev,
				 const char *name, uint64_t *id)
{
	struct dlb2_eventdev *dlb2 = dlb2_pmd_priv(dev);
	unsigned int i;
	dlb2_xstats_fn fn;

	for (i = 0; i < dlb2->xstats_count; i++) {
		struct dlb2_xstats_entry *xs = &dlb2->xstats[i];

		if (strncmp(xs->name.name, name,
			    RTE_EVENT_DEV_XSTATS_NAME_SIZE) == 0){
			if (id != NULL)
				*id = i;

			switch (xs->fn_id) {
			case DLB2_XSTATS_FN_DEV:
				fn = get_dev_stat;
				break;
			case DLB2_XSTATS_FN_PORT:
				fn = get_port_stat;
				break;
			case DLB2_XSTATS_FN_QUEUE:
				fn = get_queue_stat;
				break;
			default:
				DLB2_LOG_ERR("Unexpected xstat fn_id %d\n",
					  xs->fn_id);
				return (uint64_t)-1;
			}

			return fn(dlb2, xs->obj_idx, xs->stat,
				  xs->extra_arg) - xs->reset_value;
		}
	}
	if (id != NULL)
		*id = (uint32_t)-1;
	return (uint64_t)-1;
}

static void
dlb2_xstats_reset_range(struct dlb2_eventdev *dlb2, uint32_t start,
			uint32_t num)
{
	uint32_t i;
	dlb2_xstats_fn fn;

	for (i = start; i < start + num; i++) {
		struct dlb2_xstats_entry *xs = &dlb2->xstats[i];

		if (!xs->reset_allowed)
			continue;

		switch (xs->fn_id) {
		case DLB2_XSTATS_FN_DEV:
			fn = get_dev_stat;
			break;
		case DLB2_XSTATS_FN_PORT:
			fn = get_port_stat;
			break;
		case DLB2_XSTATS_FN_QUEUE:
			fn = get_queue_stat;
			break;
		default:
			DLB2_LOG_ERR("Unexpected xstat fn_id %d\n", xs->fn_id);
			return;
		}

		uint64_t val = fn(dlb2, xs->obj_idx, xs->stat, xs->extra_arg);
		xs->reset_value = val;
	}
}

static int
dlb2_xstats_reset_queue(struct dlb2_eventdev *dlb2, uint8_t queue_id,
			const uint64_t ids[], uint32_t nb_ids)
{
	const uint32_t reset = 1;

	if (ids) {
		uint32_t nb_reset = dlb2_xstats_update(dlb2,
					RTE_EVENT_DEV_XSTATS_QUEUE,
					queue_id, ids, NULL, nb_ids,
					reset);
		return nb_reset == nb_ids ? 0 : -EINVAL;
	}

	if (ids == NULL)
		dlb2_xstats_reset_range(dlb2,
			dlb2->xstats_offset_for_qid[queue_id],
			dlb2->xstats_count_per_qid[queue_id]);

	return 0;
}

static int
dlb2_xstats_reset_port(struct dlb2_eventdev *dlb2, uint8_t port_id,
		       const uint64_t ids[], uint32_t nb_ids)
{
	const uint32_t reset = 1;
	int offset = dlb2->xstats_offset_for_port[port_id];
	int nb_stat = dlb2->xstats_count_per_port[port_id];

	if (ids) {
		uint32_t nb_reset = dlb2_xstats_update(dlb2,
					RTE_EVENT_DEV_XSTATS_PORT, port_id,
					ids, NULL, nb_ids,
					reset);
		return nb_reset == nb_ids ? 0 : -EINVAL;
	}

	dlb2_xstats_reset_range(dlb2, offset, nb_stat);
	return 0;
}

static int
dlb2_xstats_reset_dev(struct dlb2_eventdev *dlb2, const uint64_t ids[],
		      uint32_t nb_ids)
{
	uint32_t i;

	if (ids) {
		for (i = 0; i < nb_ids; i++) {
			uint64_t id = ids[i];

			if (id >= dlb2->xstats_count_mode_dev)
				return -EINVAL;
			dlb2_xstats_reset_range(dlb2, id, 1);
		}
	} else {
		for (i = 0; i < dlb2->xstats_count_mode_dev; i++)
			dlb2_xstats_reset_range(dlb2, i, 1);
	}

	return 0;
}

int
dlb2_eventdev_xstats_reset(struct rte_eventdev *dev,
			   enum rte_event_dev_xstats_mode mode,
			   int16_t queue_port_id,
			   const uint64_t ids[],
			   uint32_t nb_ids)
{
	struct dlb2_eventdev *dlb2 = dlb2_pmd_priv(dev);
	uint32_t i;

	/* handle -1 for queue_port_id here, looping over all ports/queues */
	switch (mode) {
	case RTE_EVENT_DEV_XSTATS_DEVICE:
		if (dlb2_xstats_reset_dev(dlb2, ids, nb_ids))
			return -EINVAL;
		break;
	case RTE_EVENT_DEV_XSTATS_PORT:
		if (queue_port_id == -1) {
			for (i = 0;
			     i < DLB2_MAX_NUM_PORTS(dlb2->version); i++) {
				if (dlb2_xstats_reset_port(dlb2, i,
							   ids, nb_ids))
					return -EINVAL;
			}
		} else if (queue_port_id < DLB2_MAX_NUM_PORTS(dlb2->version)) {
			if (dlb2_xstats_reset_port(dlb2, queue_port_id,
						   ids, nb_ids))
				return -EINVAL;
		}
		break;
	case RTE_EVENT_DEV_XSTATS_QUEUE:
		if (queue_port_id == -1) {
			for (i = 0;
			     i < DLB2_MAX_NUM_QUEUES(dlb2->version); i++) {
				if (dlb2_xstats_reset_queue(dlb2, i,
							    ids, nb_ids))
					return -EINVAL;
			}
		} else if (queue_port_id < DLB2_MAX_NUM_QUEUES(dlb2->version)) {
			if (dlb2_xstats_reset_queue(dlb2, queue_port_id,
						    ids, nb_ids))
				return -EINVAL;
		}
		break;
	};

	return 0;
}

void
dlb2_eventdev_dump(struct rte_eventdev *dev, FILE *f)
{
	struct dlb2_eventdev *dlb2;
	struct dlb2_hw_dev *handle;
	int i;

	dlb2 = dlb2_pmd_priv(dev);

	if (dlb2 == NULL) {
		fprintf(f, "DLB2 Event device cannot be dumped!\n");
		return;
	}

	if (!dlb2->configured)
		fprintf(f, "DLB2 Event device is not configured\n");

	handle = &dlb2->qm_instance;

	fprintf(f, "================\n");
	fprintf(f, "DLB2 Device Dump\n");
	fprintf(f, "================\n");

	fprintf(f, "Processor supports umonitor/umwait instructions = %s\n",
		dlb2->umwait_allowed ? "yes" : "no");

	/* Generic top level device information */

	fprintf(f, "device is configured and run state =");
	if (dlb2->run_state == DLB2_RUN_STATE_STOPPED)
		fprintf(f, "STOPPED\n");
	else if (dlb2->run_state == DLB2_RUN_STATE_STOPPING)
		fprintf(f, "STOPPING\n");
	else if (dlb2->run_state == DLB2_RUN_STATE_STARTING)
		fprintf(f, "STARTING\n");
	else if (dlb2->run_state == DLB2_RUN_STATE_STARTED)
		fprintf(f, "STARTED\n");
	else
		fprintf(f, "UNEXPECTED\n");

	fprintf(f, "domain ID=%u, socket_id=%u, evdev=%p\n",
		handle->domain_id, handle->info.socket_id, dlb2->event_dev);

	fprintf(f, "num dir ports=%u, num dir queues=%u\n",
		dlb2->num_dir_ports, dlb2->num_dir_queues);

	fprintf(f, "num ldb ports=%u, num ldb queues=%u\n",
		dlb2->num_ldb_ports, dlb2->num_ldb_queues);

	fprintf(f, "num atomic inflights=%u, hist list entries=%u\n",
		handle->cfg.resources.num_atomic_inflights,
		handle->cfg.resources.num_hist_list_entries);

	fprintf(f, "results from most recent hw resource query:\n");

	fprintf(f, "\tnum_sched_domains = %u\n",
		dlb2->hw_rsrc_query_results.num_sched_domains);

	fprintf(f, "\tnum_ldb_queues = %u\n",
		dlb2->hw_rsrc_query_results.num_ldb_queues);

	fprintf(f, "\tnum_ldb_ports = %u\n",
		dlb2->hw_rsrc_query_results.num_ldb_ports);

	fprintf(f, "\tnum_dir_ports = %u\n",
		dlb2->hw_rsrc_query_results.num_dir_ports);

	fprintf(f, "\tnum_atomic_inflights = %u\n",
		dlb2->hw_rsrc_query_results.num_atomic_inflights);

	fprintf(f, "\tnum_hist_list_entries = %u\n",
		dlb2->hw_rsrc_query_results.num_hist_list_entries);

	fprintf(f, "\tmax_contiguous_hist_list_entries = %u\n",
		dlb2->hw_rsrc_query_results.max_contiguous_hist_list_entries);

	fprintf(f, "\tnum_ldb_credits = %u\n",
		dlb2->hw_rsrc_query_results.num_ldb_credits);

	fprintf(f, "\tnum_dir_credits = %u\n",
		dlb2->hw_rsrc_query_results.num_dir_credits);

	fprintf(f, "\tnum_credits = %u\n",
		dlb2->hw_rsrc_query_results.num_credits);

	/* Port level information */

	for (i = 0; i < dlb2->num_ports; i++) {
		struct dlb2_eventdev_port *p = &dlb2->ev_ports[i];
		int j;

		if (!p->enq_configured)
			fprintf(f, "Port_%d is not configured\n", i);

		fprintf(f, "Port_%d\n", i);
		fprintf(f, "=======\n");

		fprintf(f, "\tevport_%u is configured, setup done=%d\n",
			p->id, p->setup_done);

		fprintf(f, "\tconfig state=%d, port state=%d\n",
			p->qm_port.config_state, p->qm_port.state);

		fprintf(f, "\tport is %s\n",
			p->qm_port.is_directed ? "directed" : "load balanced");

		fprintf(f, "\toutstanding releases=%u\n",
			p->outstanding_releases);

		fprintf(f, "\tinflight max=%u, inflight credits=%u\n",
			p->inflight_max, p->inflight_credits);

		fprintf(f, "\tcredit update quanta=%u, implicit release =%u\n",
			p->credit_update_quanta, p->implicit_release);

		fprintf(f, "\tnum_links=%d, queues -> ", p->num_links);

		for (j = 0; j < DLB2_MAX_NUM_QIDS_PER_LDB_CQ; j++) {
			if (p->link[j].valid)
				fprintf(f, "id=%u prio=%u ",
					p->link[j].queue_id,
					p->link[j].priority);
		}
		fprintf(f, "\n");

		fprintf(f, "\thardware port id=%u\n", p->qm_port.id);

		fprintf(f, "\tcached_ldb_credits=%u\n",
			p->qm_port.cached_ldb_credits);

		fprintf(f, "\tldb_credits = %u\n",
			p->qm_port.ldb_credits);

		fprintf(f, "\tcached_dir_credits = %u\n",
			p->qm_port.cached_dir_credits);

		fprintf(f, "\tdir_credits = %u\n",
			p->qm_port.dir_credits);

		fprintf(f, "\tcached_credits = %u\n",
			p->qm_port.cached_credits);

		fprintf(f, "\tdir_credits = %u\n",
			p->qm_port.credits);

		fprintf(f, "\tgenbit=%d, cq_idx=%d, cq_depth=%d\n",
			p->qm_port.gen_bit,
			p->qm_port.cq_idx,
			p->qm_port.cq_depth);

		fprintf(f, "\tinterrupt armed=%d\n",
			p->qm_port.int_armed);

		fprintf(f, "\tPort statistics\n");

		fprintf(f, "\t\trx_ok %" PRIu64 "\n",
			p->stats.traffic.rx_ok);

		fprintf(f, "\t\trx_drop %" PRIu64 "\n",
			p->stats.traffic.rx_drop);

		fprintf(f, "\t\trx_interrupt_wait %" PRIu64 "\n",
			p->stats.traffic.rx_interrupt_wait);

		fprintf(f, "\t\trx_umonitor_umwait %" PRIu64 "\n",
			p->stats.traffic.rx_umonitor_umwait);

		fprintf(f, "\t\ttx_ok %" PRIu64 "\n",
			p->stats.traffic.tx_ok);

		fprintf(f, "\t\ttotal_polls %" PRIu64 "\n",
			p->stats.traffic.total_polls);

		fprintf(f, "\t\tzero_polls %" PRIu64 "\n",
			p->stats.traffic.zero_polls);

		fprintf(f, "\t\ttx_nospc_ldb_hw_credits %" PRIu64 "\n",
			p->stats.traffic.tx_nospc_ldb_hw_credits);

		fprintf(f, "\t\ttx_nospc_dir_hw_credits %" PRIu64 "\n",
			p->stats.traffic.tx_nospc_dir_hw_credits);

		fprintf(f, "\t\ttx_nospc_hw_credits %" PRIu64 "\n",
			p->stats.traffic.tx_nospc_hw_credits);

		fprintf(f, "\t\ttx_nospc_inflight_max %" PRIu64 "\n",
			p->stats.traffic.tx_nospc_inflight_max);

		fprintf(f, "\t\ttx_nospc_new_event_limit %" PRIu64 "\n",
			p->stats.traffic.tx_nospc_new_event_limit);

		fprintf(f, "\t\ttx_nospc_inflight_credits %" PRIu64 "\n",
			p->stats.traffic.tx_nospc_inflight_credits);

		fprintf(f, "\t\ttx_new %" PRIu64 "\n",
			p->stats.tx_op_cnt[RTE_EVENT_OP_NEW]);

		fprintf(f, "\t\ttx_fwd %" PRIu64 "\n",
			p->stats.tx_op_cnt[RTE_EVENT_OP_FORWARD]);

		fprintf(f, "\t\ttx_rel %" PRIu64 "\n",
			p->stats.tx_op_cnt[RTE_EVENT_OP_RELEASE]);

		fprintf(f, "\t\ttx_implicit_rel %" PRIu64 "\n",
			p->stats.tx_implicit_rel);

		fprintf(f, "\t\ttx_sched_ordered %" PRIu64 "\n",
			p->stats.tx_sched_cnt[DLB2_SCHED_ORDERED]);

		fprintf(f, "\t\ttx_sched_unordered %" PRIu64 "\n",
			p->stats.tx_sched_cnt[DLB2_SCHED_UNORDERED]);

		fprintf(f, "\t\ttx_sched_atomic %" PRIu64 "\n",
			p->stats.tx_sched_cnt[DLB2_SCHED_ATOMIC]);

		fprintf(f, "\t\ttx_sched_directed %" PRIu64 "\n",
			p->stats.tx_sched_cnt[DLB2_SCHED_DIRECTED]);

		fprintf(f, "\t\ttx_invalid %" PRIu64 "\n",
			p->stats.tx_invalid);

		fprintf(f, "\t\trx_sched_ordered %" PRIu64 "\n",
			p->stats.rx_sched_cnt[DLB2_SCHED_ORDERED]);

		fprintf(f, "\t\trx_sched_unordered %" PRIu64 "\n",
			p->stats.rx_sched_cnt[DLB2_SCHED_UNORDERED]);

		fprintf(f, "\t\trx_sched_atomic %" PRIu64 "\n",
			p->stats.rx_sched_cnt[DLB2_SCHED_ATOMIC]);

		fprintf(f, "\t\trx_sched_directed %" PRIu64 "\n",
			p->stats.rx_sched_cnt[DLB2_SCHED_DIRECTED]);

		fprintf(f, "\t\trx_sched_invalid %" PRIu64 "\n",
			p->stats.rx_sched_invalid);
	}

	/* Queue level information */

	for (i = 0; i < dlb2->num_queues; i++) {
		struct dlb2_eventdev_queue *q = &dlb2->ev_queues[i];
		int j, k;

		if (!q->setup_done)
			fprintf(f, "Queue_%d is not configured\n", i);

		fprintf(f, "Queue_%d\n", i);
		fprintf(f, "========\n");

		fprintf(f, "\tevqueue_%u is set up\n", q->id);

		fprintf(f, "\tqueue is %s\n",
			q->qm_queue.is_directed ? "directed" : "load balanced");

		fprintf(f, "\tnum_links=%d, ports -> ", q->num_links);

		for (j = 0; j < dlb2->num_ports; j++) {
			struct dlb2_eventdev_port *p = &dlb2->ev_ports[j];

			for (k = 0; k < DLB2_MAX_NUM_QIDS_PER_LDB_CQ; k++) {
				if (p->link[k].valid &&
				    p->link[k].queue_id == q->id)
					fprintf(f, "id=%u prio=%u ",
						p->id, p->link[k].priority);
			}
		}
		fprintf(f, "\n");

		fprintf(f, "\tcurrent depth: %u events\n",
			dlb2_get_queue_depth(dlb2, q));

		fprintf(f, "\tnum qid inflights=%u, sched_type=%d\n",
			q->qm_queue.num_qid_inflights, q->qm_queue.sched_type);
	}
}
