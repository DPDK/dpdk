/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */

#ifndef _SW_EVDEV_H_
#define _SW_EVDEV_H_

#include "sw_evdev_log.h"
#include <rte_eventdev.h>
#include <eventdev_pmd_vdev.h>
#include <rte_atomic.h>

#define SW_DEFAULT_CREDIT_QUANTA 32
#define SW_DEFAULT_SCHED_QUANTA 128
#define SW_QID_NUM_FIDS 16384
#define SW_IQS_MAX 4
#define SW_Q_PRIORITY_MAX 255
#define SW_PORTS_MAX 64
#define MAX_SW_CONS_Q_DEPTH 128
#define SW_INFLIGHT_EVENTS_TOTAL 4096
/* allow for lots of over-provisioning */
#define MAX_SW_PROD_Q_DEPTH 4096
#define SW_FRAGMENTS_MAX 16

/* Should be power-of-two minus one, to leave room for the next pointer */
#define SW_EVS_PER_Q_CHUNK 255
#define SW_Q_CHUNK_SIZE ((SW_EVS_PER_Q_CHUNK + 1) * sizeof(struct rte_event))

/* report dequeue burst sizes in buckets */
#define SW_DEQ_STAT_BUCKET_SHIFT 2
/* how many packets pulled from port by sched */
#define SCHED_DEQUEUE_DEFAULT_BURST_SIZE 32
/* max buffer size */
#define SCHED_DEQUEUE_MAX_BURST_SIZE 256

/* Flush the pipeline after this many no enq to cq */
#define SCHED_NO_ENQ_CYCLE_FLUSH 256


#define SW_PORT_HIST_LIST (MAX_SW_PROD_Q_DEPTH) /* size of our history list */
#define NUM_SAMPLES 64 /* how many data points use for average stats */

#define EVENTDEV_NAME_SW_PMD event_sw
#define SW_PMD_NAME RTE_STR(event_sw)
#define SW_PMD_NAME_MAX 64

#define SW_SCHED_TYPE_DIRECT (RTE_SCHED_TYPE_PARALLEL + 1)

#define SW_NUM_POLL_BUCKETS (MAX_SW_CONS_Q_DEPTH >> SW_DEQ_STAT_BUCKET_SHIFT)

enum {
	QE_FLAG_VALID_SHIFT = 0,
	QE_FLAG_COMPLETE_SHIFT,
	QE_FLAG_NOT_EOP_SHIFT,
	_QE_FLAG_COUNT
};

#define QE_FLAG_VALID    (1 << QE_FLAG_VALID_SHIFT)    /* for NEW FWD, FRAG */
#define QE_FLAG_COMPLETE (1 << QE_FLAG_COMPLETE_SHIFT) /* set for FWD, DROP  */
#define QE_FLAG_NOT_EOP  (1 << QE_FLAG_NOT_EOP_SHIFT)  /* set for FRAG only  */

static const uint8_t sw_qe_flag_map[] = {
		QE_FLAG_VALID /* NEW Event */,
		QE_FLAG_VALID | QE_FLAG_COMPLETE /* FWD Event */,
		QE_FLAG_COMPLETE /* RELEASE Event */,

		/* Values which can be used for future support for partial
		 * events, i.e. where one event comes back to the scheduler
		 * as multiple which need to be tracked together
		 */
		QE_FLAG_VALID | QE_FLAG_COMPLETE | QE_FLAG_NOT_EOP,
};

/* Records basic event stats at a given point. Used in port and qid structs */
struct sw_point_stats {
	uint64_t rx_pkts;
	uint64_t rx_dropped;
	uint64_t tx_pkts;
};

/* structure used to track what port a flow (FID) is pinned to */
struct sw_fid_t {
	/* which CQ this FID is currently pinned to */
	int32_t cq;
	/* number of packets gone to the CQ with this FID */
	uint32_t pcount;
};

struct reorder_buffer_entry {
	uint16_t num_fragments;		/**< Number of packet fragments */
	uint16_t fragment_index;	/**< Points to the oldest valid frag */
	uint8_t ready;			/**< Entry is ready to be reordered */
	struct rte_event fragments[SW_FRAGMENTS_MAX];
};

struct sw_iq {
	struct sw_queue_chunk *head;
	struct sw_queue_chunk *tail;
	uint16_t head_idx;
	uint16_t tail_idx;
	uint16_t count;
};

struct sw_qid {
	/* set when the QID has been initialized */
	uint8_t initialized;
	/* The type of this QID */
	int8_t type;
	/* Integer ID representing the queue. This is used in history lists,
	 * to identify the stage of processing.
	 */
	uint32_t id;
	struct sw_point_stats stats;

	/* Internal priority rings for packets */
	struct sw_iq iq[SW_IQS_MAX];
	uint32_t iq_pkt_mask; /* A mask to indicate packets in an IQ */
	uint64_t iq_pkt_count[SW_IQS_MAX];

	/* Information on what CQs are polling this IQ */
	uint32_t cq_num_mapped_cqs;
	uint32_t cq_next_tx; /* cq to write next (non-atomic) packet */
	uint32_t cq_map[SW_PORTS_MAX];
	uint64_t to_port[SW_PORTS_MAX];

	/* Track flow ids for atomic load balancing */
	struct sw_fid_t fids[SW_QID_NUM_FIDS];

	/* Track packet order for reordering when needed */
	struct reorder_buffer_entry *reorder_buffer; /*< pkts await reorder */
	struct rob_ring *reorder_buffer_freelist; /* available reorder slots */
	uint32_t reorder_buffer_index; /* oldest valid reorder buffer entry */
	uint32_t window_size;          /* Used to wrap reorder_buffer_index */

	uint8_t priority;
};

struct sw_hist_list_entry {
	int32_t qid;
	int32_t fid;
	struct reorder_buffer_entry *rob_entry;
};

struct sw_evdev;

struct sw_port {
	/* new enqueue / dequeue API doesn't have an instance pointer, only the
	 * pointer to the port being enqueue/dequeued from
	 */
	struct sw_evdev *sw;

	/* set when the port is initialized */
	uint8_t initialized;
	/* A numeric ID for the port */
	uint8_t id;

	/* An atomic counter for when the port has been unlinked, and the
	 * scheduler has not yet acked this unlink - hence there may still be
	 * events in the buffers going to the port. When the unlinks in
	 * progress is read by the scheduler, no more events will be pushed to
	 * the port - hence the scheduler core can just assign zero.
	 */
	uint8_t unlinks_in_progress;

	int16_t is_directed; /** Takes from a single directed QID */
	/**
	 * For loadbalanced we can optimise pulling packets from
	 * producers if there is no reordering involved
	 */
	int16_t num_ordered_qids;

	/** Ring and buffer for pulling events from workers for scheduling */
	struct rte_event_ring *rx_worker_ring __rte_cache_aligned;
	/** Ring and buffer for pushing packets to workers after scheduling */
	struct rte_event_ring *cq_worker_ring;

	/* hole */

	/* num releases yet to be completed on this port */
	uint16_t outstanding_releases __rte_cache_aligned;
	uint16_t inflight_max; /* app requested max inflights for this port */
	uint16_t inflight_credits; /* num credits this port has right now */
	uint8_t implicit_release; /* release events before dequeuing */

	uint16_t last_dequeue_burst_sz; /* how big the burst was */
	uint64_t last_dequeue_ticks; /* used to track burst processing time */
	uint64_t avg_pkt_ticks;      /* tracks average over NUM_SAMPLES burst */
	uint64_t total_polls;        /* how many polls were counted in stats */
	uint64_t zero_polls;         /* tracks polls returning nothing */
	uint32_t poll_buckets[SW_NUM_POLL_BUCKETS];
		/* bucket values in 4s for shorter reporting */

	/* History list structs, containing info on pkts egressed to worker */
	uint16_t hist_head __rte_cache_aligned;
	uint16_t hist_tail;
	uint16_t inflights;
	struct sw_hist_list_entry hist_list[SW_PORT_HIST_LIST];

	/* track packets in and out of this port */
	struct sw_point_stats stats;


	uint32_t pp_buf_start;
	uint32_t pp_buf_count;
	uint16_t cq_buf_count;
	struct rte_event pp_buf[SCHED_DEQUEUE_MAX_BURST_SIZE];
	struct rte_event cq_buf[MAX_SW_CONS_Q_DEPTH];

	uint8_t num_qids_mapped;
};

struct sw_evdev {
	struct rte_eventdev_data *data;

	uint32_t port_count;
	uint32_t qid_count;
	uint32_t xstats_count;
	struct sw_xstats_entry *xstats;
	uint32_t xstats_count_mode_dev;
	uint32_t xstats_count_mode_port;
	uint32_t xstats_count_mode_queue;

	/* Minimum burst size*/
	uint32_t sched_min_burst_size __rte_cache_aligned;
	/* Port dequeue burst size*/
	uint32_t sched_deq_burst_size;
	/* Refill pp buffers only once per scheduler call*/
	uint32_t refill_once_per_iter;
	/* Current values */
	uint32_t sched_flush_count;
	uint32_t sched_min_burst;

	/* Contains all ports - load balanced and directed */
	struct sw_port ports[SW_PORTS_MAX] __rte_cache_aligned;

	rte_atomic32_t inflights __rte_cache_aligned;

	/*
	 * max events in this instance. Cached here for performance.
	 * (also available in data->conf.nb_events_limit)
	 */
	uint32_t nb_events_limit;

	/* Internal queues - one per logical queue */
	struct sw_qid qids[RTE_EVENT_MAX_QUEUES_PER_DEV] __rte_cache_aligned;
	struct sw_queue_chunk *chunk_list_head;
	struct sw_queue_chunk *chunks;

	/* Cache how many packets are in each cq */
	uint16_t cq_ring_space[SW_PORTS_MAX] __rte_cache_aligned;

	/* Array of pointers to load-balanced QIDs sorted by priority level */
	struct sw_qid *qids_prioritized[RTE_EVENT_MAX_QUEUES_PER_DEV];

	/* Stats */
	struct sw_point_stats stats __rte_cache_aligned;
	uint64_t sched_called;
	int32_t sched_quanta;
	uint64_t sched_no_iq_enqueues;
	uint64_t sched_no_cq_enqueues;
	uint64_t sched_cq_qid_called;
	uint64_t sched_last_iter_bitmask;
	uint8_t sched_progress_last_iter;

	uint8_t started;
	uint32_t credit_update_quanta;

	/* store num stats and offset of the stats for each port */
	uint16_t xstats_count_per_port[SW_PORTS_MAX];
	uint16_t xstats_offset_for_port[SW_PORTS_MAX];
	/* store num stats and offset of the stats for each queue */
	uint16_t xstats_count_per_qid[RTE_EVENT_MAX_QUEUES_PER_DEV];
	uint16_t xstats_offset_for_qid[RTE_EVENT_MAX_QUEUES_PER_DEV];

	uint32_t service_id;
	char service_name[SW_PMD_NAME_MAX];
};

static inline struct sw_evdev *
sw_pmd_priv(const struct rte_eventdev *eventdev)
{
	return eventdev->data->dev_private;
}

static inline const struct sw_evdev *
sw_pmd_priv_const(const struct rte_eventdev *eventdev)
{
	return eventdev->data->dev_private;
}

uint16_t sw_event_enqueue(void *port, const struct rte_event *ev);
uint16_t sw_event_enqueue_burst(void *port, const struct rte_event ev[],
		uint16_t num);

uint16_t sw_event_dequeue(void *port, struct rte_event *ev, uint64_t wait);
uint16_t sw_event_dequeue_burst(void *port, struct rte_event *ev, uint16_t num,
			uint64_t wait);
void sw_event_schedule(struct rte_eventdev *dev);
int sw_xstats_init(struct sw_evdev *dev);
int sw_xstats_uninit(struct sw_evdev *dev);
int sw_xstats_get_names(const struct rte_eventdev *dev,
	enum rte_event_dev_xstats_mode mode, uint8_t queue_port_id,
	struct rte_event_dev_xstats_name *xstats_names,
	unsigned int *ids, unsigned int size);
int sw_xstats_get(const struct rte_eventdev *dev,
		enum rte_event_dev_xstats_mode mode, uint8_t queue_port_id,
		const unsigned int ids[], uint64_t values[], unsigned int n);
uint64_t sw_xstats_get_by_name(const struct rte_eventdev *dev,
		const char *name, unsigned int *id);
int sw_xstats_reset(struct rte_eventdev *dev,
		enum rte_event_dev_xstats_mode mode,
		int16_t queue_port_id,
		const uint32_t ids[],
		uint32_t nb_ids);

int test_sw_eventdev(void);

#endif /* _SW_EVDEV_H_ */
