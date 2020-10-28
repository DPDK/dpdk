/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#ifndef _TEST_ORDER_COMMON_
#define _TEST_ORDER_COMMON_

#include <stdio.h>
#include <stdbool.h>

#include <rte_cycles.h>
#include <rte_eventdev.h>
#include <rte_lcore.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_mbuf_dyn.h>

#include "evt_common.h"
#include "evt_options.h"
#include "evt_test.h"

#define BURST_SIZE 16

typedef uint32_t flow_id_t;
typedef uint32_t seqn_t;

struct test_order;

struct worker_data {
	uint8_t dev_id;
	uint8_t port_id;
	struct test_order *t;
};

struct prod_data {
	uint8_t dev_id;
	uint8_t port_id;
	uint8_t queue_id;
	struct test_order *t;
};

struct test_order {
	/* Don't change the offset of "err". Signal handler use this memory
	 * to terminate all lcores work.
	 */
	int err;
	/*
	 * The atomic_* is an expensive operation,Since it is a functional test,
	 * We are using the atomic_ operation to reduce the code complexity.
	 */
	rte_atomic64_t outstand_pkts;
	enum evt_test_result result;
	uint32_t nb_flows;
	uint64_t nb_pkts;
	struct rte_mempool *pool;
	int flow_id_dynfield_offset;
	int seqn_dynfield_offset;
	struct prod_data prod;
	struct worker_data worker[EVT_MAX_PORTS];
	uint32_t *producer_flow_seq;
	uint32_t *expected_flow_seq;
	struct evt_options *opt;
} __rte_cache_aligned;

static inline void
order_flow_id_copy_from_mbuf(struct test_order *t, struct rte_event *event)
{
	event->flow_id = *RTE_MBUF_DYNFIELD(event->mbuf,
			t->flow_id_dynfield_offset, flow_id_t *);
}

static inline void
order_flow_id_save(struct test_order *t, flow_id_t flow_id,
		struct rte_mbuf *mbuf, struct rte_event *event)
{
	*RTE_MBUF_DYNFIELD(mbuf,
			t->flow_id_dynfield_offset, flow_id_t *) = flow_id;
	event->flow_id = flow_id;
	event->mbuf = mbuf;
}

static inline seqn_t *
order_mbuf_seqn(struct test_order *t, struct rte_mbuf *mbuf)
{
	return RTE_MBUF_DYNFIELD(mbuf, t->seqn_dynfield_offset, seqn_t *);
}

static inline int
order_nb_event_ports(struct evt_options *opt)
{
	return evt_nr_active_lcores(opt->wlcores) + 1 /* producer */;
}

static __rte_always_inline void
order_process_stage_1(struct test_order *const t,
		struct rte_event *const ev, const uint32_t nb_flows,
		uint32_t *const expected_flow_seq,
		rte_atomic64_t *const outstand_pkts)
{
	const uint32_t flow = (uintptr_t)ev->mbuf % nb_flows;
	/* compare the seqn against expected value */
	if (*order_mbuf_seqn(t, ev->mbuf) != expected_flow_seq[flow]) {
		evt_err("flow=%x seqn mismatch got=%x expected=%x",
			flow, *order_mbuf_seqn(t, ev->mbuf),
			expected_flow_seq[flow]);
		t->err = true;
		rte_smp_wmb();
	}
	/*
	 * Events from an atomic flow of an event queue can be scheduled only to
	 * a single port at a time. The port is guaranteed to have exclusive
	 * (atomic) access for given atomic flow.So we don't need to update
	 * expected_flow_seq in critical section.
	 */
	expected_flow_seq[flow]++;
	rte_pktmbuf_free(ev->mbuf);
	rte_atomic64_sub(outstand_pkts, 1);
}

static __rte_always_inline void
order_process_stage_invalid(struct test_order *const t,
			struct rte_event *const ev)
{
	evt_err("invalid queue %d", ev->queue_id);
	t->err = true;
	rte_smp_wmb();
}

#define ORDER_WORKER_INIT\
	struct worker_data *w  = arg;\
	struct test_order *t = w->t;\
	struct evt_options *opt = t->opt;\
	const uint8_t dev_id = w->dev_id;\
	const uint8_t port = w->port_id;\
	const uint32_t nb_flows = t->nb_flows;\
	uint32_t *expected_flow_seq = t->expected_flow_seq;\
	rte_atomic64_t *outstand_pkts = &t->outstand_pkts;\
	if (opt->verbose_level > 1)\
		printf("%s(): lcore %d dev_id %d port=%d\n",\
			__func__, rte_lcore_id(), dev_id, port)

int order_test_result(struct evt_test *test, struct evt_options *opt);
int order_opt_check(struct evt_options *opt);
int order_test_setup(struct evt_test *test, struct evt_options *opt);
int order_mempool_setup(struct evt_test *test, struct evt_options *opt);
int order_launch_lcores(struct evt_test *test, struct evt_options *opt,
			int (*worker)(void *));
int order_event_dev_port_setup(struct evt_test *test, struct evt_options *opt,
				uint8_t nb_workers, uint8_t nb_queues);
void order_test_destroy(struct evt_test *test, struct evt_options *opt);
void order_opt_dump(struct evt_options *opt);
void order_mempool_destroy(struct evt_test *test, struct evt_options *opt);
void order_eventdev_destroy(struct evt_test *test, struct evt_options *opt);

#endif /* _TEST_ORDER_COMMON_ */
