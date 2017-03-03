/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Cavium networks. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *	 * Redistributions of source code must retain the above copyright
 *	   notice, this list of conditions and the following disclaimer.
 *	 * Redistributions in binary form must reproduce the above copyright
 *	   notice, this list of conditions and the following disclaimer in
 *	   the documentation and/or other materials provided with the
 *	   distribution.
 *	 * Neither the name of Cavium networks nor the names of its
 *	   contributors may be used to endorse or promote products derived
 *	   from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <rte_atomic.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_debug.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_eventdev.h>
#include <rte_hexdump.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_random.h>

#include "test.h"

#define NUM_PACKETS (1 << 18)
#define MAX_EVENTS  (16 * 1024)

static int evdev;
static struct rte_mempool *eventdev_test_mempool;

struct event_attr {
	uint32_t flow_id;
	uint8_t event_type;
	uint8_t sub_event_type;
	uint8_t sched_type;
	uint8_t queue;
	uint8_t port;
};

struct test_core_param {
	rte_atomic32_t *total_events;
	uint64_t dequeue_tmo_ticks;
	uint8_t port;
	uint8_t sched_type;
};

static int
testsuite_setup(void)
{
	const char *eventdev_name = "event_octeontx";

	evdev = rte_event_dev_get_dev_id(eventdev_name);
	if (evdev < 0) {
		printf("%d: Eventdev %s not found - creating.\n",
				__LINE__, eventdev_name);
		if (rte_eal_vdev_init(eventdev_name, NULL) < 0) {
			printf("Error creating eventdev %s\n", eventdev_name);
			return TEST_FAILED;
		}
		evdev = rte_event_dev_get_dev_id(eventdev_name);
		if (evdev < 0) {
			printf("Error finding newly created eventdev\n");
			return TEST_FAILED;
		}
	}

	return TEST_SUCCESS;
}

static void
testsuite_teardown(void)
{
	rte_event_dev_close(evdev);
}

static inline void
devconf_set_default_sane_values(struct rte_event_dev_config *dev_conf,
			struct rte_event_dev_info *info)
{
	memset(dev_conf, 0, sizeof(struct rte_event_dev_config));
	dev_conf->dequeue_timeout_ns = info->min_dequeue_timeout_ns;
	dev_conf->nb_event_ports = info->max_event_ports;
	dev_conf->nb_event_queues = info->max_event_queues;
	dev_conf->nb_event_queue_flows = info->max_event_queue_flows;
	dev_conf->nb_event_port_dequeue_depth =
			info->max_event_port_dequeue_depth;
	dev_conf->nb_event_port_enqueue_depth =
			info->max_event_port_enqueue_depth;
	dev_conf->nb_event_port_enqueue_depth =
			info->max_event_port_enqueue_depth;
	dev_conf->nb_events_limit =
			info->max_num_events;
}

enum {
	TEST_EVENTDEV_SETUP_DEFAULT,
	TEST_EVENTDEV_SETUP_PRIORITY,
	TEST_EVENTDEV_SETUP_DEQUEUE_TIMEOUT,
};

static inline int
_eventdev_setup(int mode)
{
	int i, ret;
	struct rte_event_dev_config dev_conf;
	struct rte_event_dev_info info;
	const char *pool_name = "evdev_octeontx_test_pool";

	/* Create and destrory pool for each test case to make it standalone */
	eventdev_test_mempool = rte_pktmbuf_pool_create(pool_name,
					MAX_EVENTS,
					0 /*MBUF_CACHE_SIZE*/,
					0,
					512, /* Use very small mbufs */
					rte_socket_id());
	if (!eventdev_test_mempool) {
		printf("ERROR creating mempool\n");
		return TEST_FAILED;
	}

	ret = rte_event_dev_info_get(evdev, &info);
	TEST_ASSERT_SUCCESS(ret, "Failed to get event dev info");
	TEST_ASSERT(info.max_num_events >= (int32_t)MAX_EVENTS,
			"max_num_events=%d < max_events=%d",
			info.max_num_events, MAX_EVENTS);

	devconf_set_default_sane_values(&dev_conf, &info);
	if (mode == TEST_EVENTDEV_SETUP_DEQUEUE_TIMEOUT)
		dev_conf.event_dev_cfg |= RTE_EVENT_DEV_CFG_PER_DEQUEUE_TIMEOUT;

	ret = rte_event_dev_configure(evdev, &dev_conf);
	TEST_ASSERT_SUCCESS(ret, "Failed to configure eventdev");

	if (mode == TEST_EVENTDEV_SETUP_PRIORITY) {
		/* Configure event queues(0 to n) with
		 * RTE_EVENT_DEV_PRIORITY_HIGHEST to
		 * RTE_EVENT_DEV_PRIORITY_LOWEST
		 */
		uint8_t step = (RTE_EVENT_DEV_PRIORITY_LOWEST + 1) /
				rte_event_queue_count(evdev);
		for (i = 0; i < rte_event_queue_count(evdev); i++) {
			struct rte_event_queue_conf queue_conf;

			ret = rte_event_queue_default_conf_get(evdev, i,
						&queue_conf);
			TEST_ASSERT_SUCCESS(ret, "Failed to get def_conf%d", i);
			queue_conf.priority = i * step;
			ret = rte_event_queue_setup(evdev, i, &queue_conf);
			TEST_ASSERT_SUCCESS(ret, "Failed to setup queue=%d", i);
		}

	} else {
		/* Configure event queues with default priority */
		for (i = 0; i < rte_event_queue_count(evdev); i++) {
			ret = rte_event_queue_setup(evdev, i, NULL);
			TEST_ASSERT_SUCCESS(ret, "Failed to setup queue=%d", i);
		}
	}
	/* Configure event ports */
	for (i = 0; i < rte_event_port_count(evdev); i++) {
		ret = rte_event_port_setup(evdev, i, NULL);
		TEST_ASSERT_SUCCESS(ret, "Failed to setup port=%d", i);
		ret = rte_event_port_link(evdev, i, NULL, NULL, 0);
		TEST_ASSERT(ret >= 0, "Failed to link all queues port=%d", i);
	}

	ret = rte_event_dev_start(evdev);
	TEST_ASSERT_SUCCESS(ret, "Failed to start device");

	return TEST_SUCCESS;
}

static inline int
eventdev_setup(void)
{
	return _eventdev_setup(TEST_EVENTDEV_SETUP_DEFAULT);
}

static inline int
eventdev_setup_priority(void)
{
	return _eventdev_setup(TEST_EVENTDEV_SETUP_PRIORITY);
}

static inline void
eventdev_teardown(void)
{
	rte_event_dev_stop(evdev);
	rte_mempool_free(eventdev_test_mempool);
}

static inline void
update_event_and_validation_attr(struct rte_mbuf *m, struct rte_event *ev,
			uint32_t flow_id, uint8_t event_type,
			uint8_t sub_event_type, uint8_t sched_type,
			uint8_t queue, uint8_t port)
{
	struct event_attr *attr;

	/* Store the event attributes in mbuf for future reference */
	attr = rte_pktmbuf_mtod(m, struct event_attr *);
	attr->flow_id = flow_id;
	attr->event_type = event_type;
	attr->sub_event_type = sub_event_type;
	attr->sched_type = sched_type;
	attr->queue = queue;
	attr->port = port;

	ev->flow_id = flow_id;
	ev->sub_event_type = sub_event_type;
	ev->event_type = event_type;
	/* Inject the new event */
	ev->op = RTE_EVENT_OP_NEW;
	ev->sched_type = sched_type;
	ev->queue_id = queue;
	ev->mbuf = m;
}

static inline int
inject_events(uint32_t flow_id, uint8_t event_type, uint8_t sub_event_type,
		uint8_t sched_type, uint8_t queue, uint8_t port,
		unsigned int events)
{
	struct rte_mbuf *m;
	unsigned int i;

	for (i = 0; i < events; i++) {
		struct rte_event ev = {.event = 0, .u64 = 0};

		m = rte_pktmbuf_alloc(eventdev_test_mempool);
		TEST_ASSERT_NOT_NULL(m, "mempool alloc failed");

		m->seqn = i;
		update_event_and_validation_attr(m, &ev, flow_id, event_type,
			sub_event_type, sched_type, queue, port);
		rte_event_enqueue_burst(evdev, port, &ev, 1);
	}
	return 0;
}

static inline int
check_excess_events(uint8_t port)
{
	int i;
	uint16_t valid_event;
	struct rte_event ev;

	/* Check for excess events, try for a few times and exit */
	for (i = 0; i < 32; i++) {
		valid_event = rte_event_dequeue_burst(evdev, port, &ev, 1, 0);

		TEST_ASSERT_SUCCESS(valid_event, "Unexpected valid event=%d",
					ev.mbuf->seqn);
	}
	return 0;
}

static inline int
generate_random_events(const unsigned int total_events)
{
	struct rte_event_dev_info info;
	unsigned int i;
	int ret;

	ret = rte_event_dev_info_get(evdev, &info);
	TEST_ASSERT_SUCCESS(ret, "Failed to get event dev info");
	for (i = 0; i < total_events; i++) {
		ret = inject_events(
			rte_rand() % info.max_event_queue_flows /*flow_id */,
			rte_rand() % (RTE_EVENT_TYPE_CPU + 1) /* event_type */,
			rte_rand() % 256 /* sub_event_type */,
			rte_rand() % (RTE_SCHED_TYPE_PARALLEL + 1),
			rte_rand() % rte_event_queue_count(evdev) /* queue */,
			0 /* port */,
			1 /* events */);
		if (ret)
			return TEST_FAILED;
	}
	return ret;
}


static inline int
validate_event(struct rte_event *ev)
{
	struct event_attr *attr;

	attr = rte_pktmbuf_mtod(ev->mbuf, struct event_attr *);
	TEST_ASSERT_EQUAL(attr->flow_id, ev->flow_id,
			"flow_id mismatch enq=%d deq =%d",
			attr->flow_id, ev->flow_id);
	TEST_ASSERT_EQUAL(attr->event_type, ev->event_type,
			"event_type mismatch enq=%d deq =%d",
			attr->event_type, ev->event_type);
	TEST_ASSERT_EQUAL(attr->sub_event_type, ev->sub_event_type,
			"sub_event_type mismatch enq=%d deq =%d",
			attr->sub_event_type, ev->sub_event_type);
	TEST_ASSERT_EQUAL(attr->sched_type, ev->sched_type,
			"sched_type mismatch enq=%d deq =%d",
			attr->sched_type, ev->sched_type);
	TEST_ASSERT_EQUAL(attr->queue, ev->queue_id,
			"queue mismatch enq=%d deq =%d",
			attr->queue, ev->queue_id);
	return 0;
}

typedef int (*validate_event_cb)(uint32_t index, uint8_t port,
				 struct rte_event *ev);

static inline int
consume_events(uint8_t port, const uint32_t total_events, validate_event_cb fn)
{
	int ret;
	uint16_t valid_event;
	uint32_t events = 0, forward_progress_cnt = 0, index = 0;
	struct rte_event ev;

	while (1) {
		if (++forward_progress_cnt > UINT16_MAX) {
			printf("Detected deadlock\n");
			return TEST_FAILED;
		}

		valid_event = rte_event_dequeue_burst(evdev, port, &ev, 1, 0);
		if (!valid_event)
			continue;

		forward_progress_cnt = 0;
		ret = validate_event(&ev);
		if (ret)
			return TEST_FAILED;

		if (fn != NULL) {
			ret = fn(index, port, &ev);
			TEST_ASSERT_SUCCESS(ret,
				"Failed to validate test specific event");
		}

		++index;

		rte_pktmbuf_free(ev.mbuf);
		if (++events >= total_events)
			break;
	}

	return check_excess_events(port);
}

static int
validate_simple_enqdeq(uint32_t index, uint8_t port, struct rte_event *ev)
{
	RTE_SET_USED(port);
	TEST_ASSERT_EQUAL(index, ev->mbuf->seqn, "index=%d != seqn=%d", index,
					ev->mbuf->seqn);
	return 0;
}

static inline int
test_simple_enqdeq(uint8_t sched_type)
{
	int ret;

	ret = inject_events(0 /*flow_id */,
				RTE_EVENT_TYPE_CPU /* event_type */,
				0 /* sub_event_type */,
				sched_type,
				0 /* queue */,
				0 /* port */,
				MAX_EVENTS);
	if (ret)
		return TEST_FAILED;

	return consume_events(0 /* port */, MAX_EVENTS,	validate_simple_enqdeq);
}

static int
test_simple_enqdeq_ordered(void)
{
	return test_simple_enqdeq(RTE_SCHED_TYPE_ORDERED);
}

static int
test_simple_enqdeq_atomic(void)
{
	return test_simple_enqdeq(RTE_SCHED_TYPE_ATOMIC);
}

static int
test_simple_enqdeq_parallel(void)
{
	return test_simple_enqdeq(RTE_SCHED_TYPE_PARALLEL);
}

/*
 * Generate a prescribed number of events and spread them across available
 * queues. On dequeue, using single event port(port 0) verify the enqueued
 * event attributes
 */
static int
test_multi_queue_enq_single_port_deq(void)
{
	int ret;

	ret = generate_random_events(MAX_EVENTS);
	if (ret)
		return TEST_FAILED;

	return consume_events(0 /* port */, MAX_EVENTS, NULL);
}

/*
 * Inject 0..MAX_EVENTS events over 0..rte_event_queue_count() with modulus
 * operation
 *
 * For example, Inject 32 events over 0..7 queues
 * enqueue events 0, 8, 16, 24 in queue 0
 * enqueue events 1, 9, 17, 25 in queue 1
 * ..
 * ..
 * enqueue events 7, 15, 23, 31 in queue 7
 *
 * On dequeue, Validate the events comes in 0,8,16,24,1,9,17,25..,7,15,23,31
 * order from queue0(highest priority) to queue7(lowest_priority)
 */
static int
validate_queue_priority(uint32_t index, uint8_t port, struct rte_event *ev)
{
	uint32_t range = MAX_EVENTS / rte_event_queue_count(evdev);
	uint32_t expected_val = (index % range) * rte_event_queue_count(evdev);

	expected_val += ev->queue_id;
	RTE_SET_USED(port);
	TEST_ASSERT_EQUAL(ev->mbuf->seqn, expected_val,
	"seqn=%d index=%d expected=%d range=%d nb_queues=%d max_event=%d",
			ev->mbuf->seqn, index, expected_val, range,
			rte_event_queue_count(evdev), MAX_EVENTS);
	return 0;
}

static int
test_multi_queue_priority(void)
{
	uint8_t queue;
	struct rte_mbuf *m;
	int i, max_evts_roundoff;

	/* See validate_queue_priority() comments for priority validate logic */
	max_evts_roundoff  = MAX_EVENTS / rte_event_queue_count(evdev);
	max_evts_roundoff *= rte_event_queue_count(evdev);

	for (i = 0; i < max_evts_roundoff; i++) {
		struct rte_event ev = {.event = 0, .u64 = 0};

		m = rte_pktmbuf_alloc(eventdev_test_mempool);
		TEST_ASSERT_NOT_NULL(m, "mempool alloc failed");

		m->seqn = i;
		queue = i % rte_event_queue_count(evdev);
		update_event_and_validation_attr(m, &ev, 0, RTE_EVENT_TYPE_CPU,
			0, RTE_SCHED_TYPE_PARALLEL, queue, 0);
		rte_event_enqueue_burst(evdev, 0, &ev, 1);
	}

	return consume_events(0, max_evts_roundoff, validate_queue_priority);
}

static struct unit_test_suite eventdev_octeontx_testsuite  = {
	.suite_name = "eventdev octeontx unit test suite",
	.setup = testsuite_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {
		TEST_CASE_ST(eventdev_setup, eventdev_teardown,
			test_simple_enqdeq_ordered),
		TEST_CASE_ST(eventdev_setup, eventdev_teardown,
			test_simple_enqdeq_atomic),
		TEST_CASE_ST(eventdev_setup, eventdev_teardown,
			test_simple_enqdeq_parallel),
		TEST_CASE_ST(eventdev_setup, eventdev_teardown,
			test_multi_queue_enq_single_port_deq),
		TEST_CASE_ST(eventdev_setup_priority, eventdev_teardown,
			test_multi_queue_priority),
		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static int
test_eventdev_octeontx(void)
{
	return unit_test_suite_runner(&eventdev_octeontx_testsuite);
}

REGISTER_TEST_COMMAND(eventdev_octeontx_autotest, test_eventdev_octeontx);
