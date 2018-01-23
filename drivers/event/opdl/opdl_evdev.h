/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _OPDL_EVDEV_H_
#define _OPDL_EVDEV_H_

#include <rte_eventdev.h>
#include <rte_eventdev_pmd_vdev.h>
#include <rte_atomic.h>
#include "opdl_ring.h"

#define OPDL_QID_NUM_FIDS 1024
#define OPDL_IQS_MAX 1
#define OPDL_Q_PRIORITY_MAX 1
#define OPDL_PORTS_MAX 64
#define MAX_OPDL_CONS_Q_DEPTH 128
/* OPDL size */
#define OPDL_INFLIGHT_EVENTS_TOTAL 4096
/* allow for lots of over-provisioning */
#define OPDL_FRAGMENTS_MAX 1

/* report dequeue burst sizes in buckets */
#define OPDL_DEQ_STAT_BUCKET_SHIFT 2
/* how many packets pulled from port by sched */
#define SCHED_DEQUEUE_BURST_SIZE 32

/* size of our history list */
#define OPDL_PORT_HIST_LIST (MAX_OPDL_PROD_Q_DEPTH)

/* how many data points use for average stats */
#define NUM_SAMPLES 64

#define EVENTDEV_NAME_OPDL_PMD event_opdl
#define OPDL_PMD_NAME RTE_STR(event_opdl)
#define OPDL_PMD_NAME_MAX 64

#define OPDL_INVALID_QID 255

#define OPDL_SCHED_TYPE_DIRECT (RTE_SCHED_TYPE_PARALLEL + 1)

#define OPDL_NUM_POLL_BUCKETS  \
	(MAX_OPDL_CONS_Q_DEPTH >> OPDL_DEQ_STAT_BUCKET_SHIFT)

enum {
	QE_FLAG_VALID_SHIFT = 0,
	QE_FLAG_COMPLETE_SHIFT,
	QE_FLAG_NOT_EOP_SHIFT,
	_QE_FLAG_COUNT
};

enum port_type {
	OPDL_INVALID_PORT = 0,
	OPDL_REGULAR_PORT = 1,
	OPDL_PURE_RX_PORT,
	OPDL_PURE_TX_PORT,
	OPDL_ASYNC_PORT
};

enum queue_type {
	OPDL_Q_TYPE_INVALID = 0,
	OPDL_Q_TYPE_SINGLE_LINK = 1,
	OPDL_Q_TYPE_ATOMIC,
	OPDL_Q_TYPE_ORDERED
};

enum queue_pos {
	OPDL_Q_POS_START = 0,
	OPDL_Q_POS_MIDDLE,
	OPDL_Q_POS_END
};

#define QE_FLAG_VALID    (1 << QE_FLAG_VALID_SHIFT)    /* for NEW FWD, FRAG */
#define QE_FLAG_COMPLETE (1 << QE_FLAG_COMPLETE_SHIFT) /* set for FWD, DROP  */
#define QE_FLAG_NOT_EOP  (1 << QE_FLAG_NOT_EOP_SHIFT)  /* set for FRAG only  */

static const uint8_t opdl_qe_flag_map[] = {
	QE_FLAG_VALID /* NEW Event */,
	QE_FLAG_VALID | QE_FLAG_COMPLETE /* FWD Event */,
	QE_FLAG_COMPLETE /* RELEASE Event */,

	/* Values which can be used for future support for partial
	 * events, i.e. where one event comes back to the scheduler
	 * as multiple which need to be tracked together
	 */
	QE_FLAG_VALID | QE_FLAG_COMPLETE | QE_FLAG_NOT_EOP,
};


enum port_xstat_name {
	claim_pkts_requested = 0,
	claim_pkts_granted,
	claim_non_empty,
	claim_empty,
	total_cycles,
	max_num_port_xstat
};

#define OPDL_MAX_PORT_XSTAT_NUM (OPDL_PORTS_MAX * max_num_port_xstat)

struct opdl_port;

typedef uint16_t (*opdl_enq_operation)(struct opdl_port *port,
		const struct rte_event ev[],
		uint16_t num);

typedef uint16_t (*opdl_deq_operation)(struct opdl_port *port,
		struct rte_event ev[],
		uint16_t num);

struct opdl_evdev;

struct opdl_stage_meta_data {
	uint32_t num_claimed;	/* number of entries claimed by this stage */
	uint32_t burst_sz;	/* Port claim burst size */
};

struct opdl_port {

	/* back pointer */
	struct opdl_evdev *opdl;

	/* enq handler & stage instance */
	opdl_enq_operation enq;
	struct opdl_stage *enq_stage_inst;

	/* deq handler & stage instance */
	opdl_deq_operation deq;
	struct opdl_stage *deq_stage_inst;

	/* port id has correctly been set */
	uint8_t configured;

	/* set when the port is initialized */
	uint8_t initialized;

	/* A numeric ID for the port */
	uint8_t id;

	/* Space for claimed entries */
	struct rte_event *entries[MAX_OPDL_CONS_Q_DEPTH];

	/* RX/REGULAR/TX/ASYNC - determined on position in queue */
	enum port_type p_type;

	/* if the claim is static atomic type  */
	bool atomic_claim;

	/* Queue linked to this port - internal queue id*/
	uint8_t queue_id;

	/* Queue linked to this port - external queue id*/
	uint8_t external_qid;

	/* Next queue linked to this port - external queue id*/
	uint8_t next_external_qid;

	/* number of instances of this stage */
	uint32_t num_instance;

	/* instance ID of this stage*/
	uint32_t instance_id;

	/* track packets in and out of this port */
	uint64_t port_stat[max_num_port_xstat];
	uint64_t start_cycles;
};

struct opdl_queue_meta_data {
	uint8_t         ext_id;
	enum queue_type type;
	int8_t          setup;
};

struct opdl_xstats_entry {
	struct rte_event_dev_xstats_name stat;
	unsigned int id;
	uint64_t *value;
};

struct opdl_queue {

	/* Opdl ring this queue is associated with */
	uint32_t opdl_id;

	/* type and position have correctly been set */
	uint8_t configured;

	/* port number and associated ports have been associated */
	uint8_t initialized;

	/* type of this queue (Atomic, Ordered, Parallel, Direct)*/
	enum queue_type q_type;

	/* position of queue (START, MIDDLE, END) */
	enum queue_pos q_pos;

	/* external queue id. It is mapped to the queue position */
	uint8_t external_qid;

	struct opdl_port *ports[OPDL_PORTS_MAX];
	uint32_t nb_ports;

	/* priority, reserved for future */
	uint8_t priority;
};


#define OPDL_TUR_PER_DEV 12

/* PMD needs an extra queue per Opdl  */
#define OPDL_MAX_QUEUES (RTE_EVENT_MAX_QUEUES_PER_DEV - OPDL_TUR_PER_DEV)


struct opdl_evdev {
	struct rte_eventdev_data *data;

	uint8_t started;

	/* Max number of ports and queues*/
	uint32_t max_port_nb;
	uint32_t max_queue_nb;

	/* slots in the opdl ring */
	uint32_t nb_events_limit;

	/*
	 * Array holding all opdl for this device
	 */
	struct opdl_ring *opdl[OPDL_TUR_PER_DEV];
	uint32_t nb_opdls;

	struct opdl_queue_meta_data q_md[OPDL_MAX_QUEUES];
	uint32_t nb_q_md;

	/* Internal queues - one per logical queue */
	struct opdl_queue
		queue[RTE_EVENT_MAX_QUEUES_PER_DEV] __rte_cache_aligned;

	uint32_t nb_queues;

	struct opdl_stage_meta_data s_md[OPDL_PORTS_MAX];

	/* Contains all ports - load balanced and directed */
	struct opdl_port ports[OPDL_PORTS_MAX] __rte_cache_aligned;
	uint32_t nb_ports;

	uint8_t q_map_ex_to_in[OPDL_INVALID_QID];

	/* Stats */
	struct opdl_xstats_entry port_xstat[OPDL_MAX_PORT_XSTAT_NUM];

	char service_name[OPDL_PMD_NAME_MAX];
	int socket;
	int do_validation;
	int do_test;
};


static inline struct opdl_evdev *
opdl_pmd_priv(const struct rte_eventdev *eventdev)
{
	return eventdev->data->dev_private;
}

static inline uint8_t
opdl_pmd_dev_id(const struct opdl_evdev *opdl)
{
	return opdl->data->dev_id;
}

static inline const struct opdl_evdev *
opdl_pmd_priv_const(const struct rte_eventdev *eventdev)
{
	return eventdev->data->dev_private;
}

uint16_t opdl_event_enqueue(void *port, const struct rte_event *ev);
uint16_t opdl_event_enqueue_burst(void *port, const struct rte_event ev[],
		uint16_t num);

uint16_t opdl_event_dequeue(void *port, struct rte_event *ev, uint64_t wait);
uint16_t opdl_event_dequeue_burst(void *port, struct rte_event *ev,
		uint16_t num, uint64_t wait);
void opdl_event_schedule(struct rte_eventdev *dev);

void opdl_xstats_init(struct rte_eventdev *dev);
int opdl_xstats_uninit(struct rte_eventdev *dev);
int opdl_xstats_get_names(const struct rte_eventdev *dev,
		enum rte_event_dev_xstats_mode mode, uint8_t queue_port_id,
		struct rte_event_dev_xstats_name *xstats_names,
		unsigned int *ids, unsigned int size);
int opdl_xstats_get(const struct rte_eventdev *dev,
		enum rte_event_dev_xstats_mode mode, uint8_t queue_port_id,
		const unsigned int ids[], uint64_t values[], unsigned int n);
uint64_t opdl_xstats_get_by_name(const struct rte_eventdev *dev,
		const char *name, unsigned int *id);
int opdl_xstats_reset(struct rte_eventdev *dev,
		enum rte_event_dev_xstats_mode mode,
		int16_t queue_port_id,
		const uint32_t ids[],
		uint32_t nb_ids);

int opdl_add_event_handlers(struct rte_eventdev *dev);
int build_all_dependencies(struct rte_eventdev *dev);
int check_queues_linked(struct rte_eventdev *dev);
int create_queues_and_rings(struct rte_eventdev *dev);
int initialise_all_other_ports(struct rte_eventdev *dev);
int initialise_queue_zero_ports(struct rte_eventdev *dev);
int assign_internal_queue_ids(struct rte_eventdev *dev);
void destroy_queues_and_rings(struct rte_eventdev *dev);
int opdl_selftest(void);

#endif /* _OPDL_EVDEV_H_ */
