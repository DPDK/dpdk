/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2019 NXP
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
#include <rte_bus_vdev.h>
#include <rte_test.h>
#include <rte_fslmc.h>

#include "dpaa2_eventdev.h"
#include "dpaa2_eventdev_logs.h"

#define MAX_PORTS 4
#define NUM_PACKETS (1 << 18)
#define MAX_EVENTS  8
#define DPAA2_TEST_RUN(setup, teardown, test) \
	dpaa2_test_run(setup, teardown, test, #test)

static int total;
static int passed;
static int failed;
static int unsupported;

static int evdev;
static struct rte_mempool *eventdev_test_mempool;

struct event_attr {
	uint32_t flow_id;
	uint8_t event_type;
	uint8_t sub_event_type;
	uint8_t sched_type;
	uint8_t queue;
	uint8_t port;
	uint8_t seq;
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
	const char *eventdev_name = "event_dpaa2";

	evdev = rte_event_dev_get_dev_id(eventdev_name);
	if (evdev < 0) {
		dpaa2_evdev_dbg("%d: Eventdev %s not found - creating.",
				__LINE__, eventdev_name);
		if (rte_vdev_init(eventdev_name, NULL) < 0) {
			dpaa2_evdev_err("Error creating eventdev %s",
					eventdev_name);
			return -1;
		}
		evdev = rte_event_dev_get_dev_id(eventdev_name);
		if (evdev < 0) {
			dpaa2_evdev_err("Error finding newly created eventdev");
			return -1;
		}
	}

	return 0;
}

static void
testsuite_teardown(void)
{
	rte_event_dev_close(evdev);
}

static void
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

static int
_eventdev_setup(int mode)
{
	int i, ret;
	struct rte_event_dev_config dev_conf;
	struct rte_event_dev_info info;
	const char *pool_name = "evdev_dpaa2_test_pool";

	/* Create and destroy pool for each test case to make it standalone */
	eventdev_test_mempool = rte_pktmbuf_pool_create(pool_name,
					MAX_EVENTS,
					0 /*MBUF_CACHE_SIZE*/,
					0,
					512, /* Use very small mbufs */
					rte_socket_id());
	if (!eventdev_test_mempool) {
		dpaa2_evdev_err("ERROR creating mempool");
		return -1;
	}

	ret = rte_event_dev_info_get(evdev, &info);
	RTE_TEST_ASSERT_SUCCESS(ret, "Failed to get event dev info");
	RTE_TEST_ASSERT(info.max_num_events >= (int32_t)MAX_EVENTS,
			"ERROR max_num_events=%d < max_events=%d",
				info.max_num_events, MAX_EVENTS);

	devconf_set_default_sane_values(&dev_conf, &info);
	if (mode == TEST_EVENTDEV_SETUP_DEQUEUE_TIMEOUT)
		dev_conf.event_dev_cfg |= RTE_EVENT_DEV_CFG_PER_DEQUEUE_TIMEOUT;

	ret = rte_event_dev_configure(evdev, &dev_conf);
	RTE_TEST_ASSERT_SUCCESS(ret, "Failed to configure eventdev");

	uint32_t queue_count;
	RTE_TEST_ASSERT_SUCCESS(rte_event_dev_attr_get(evdev,
			    RTE_EVENT_DEV_ATTR_QUEUE_COUNT,
			    &queue_count), "Queue count get failed");

	if (mode == TEST_EVENTDEV_SETUP_PRIORITY) {
		if (queue_count > 8) {
			dpaa2_evdev_err(
				"test expects the unique priority per queue");
			return -ENOTSUP;
		}

		/* Configure event queues(0 to n) with
		 * RTE_EVENT_DEV_PRIORITY_HIGHEST to
		 * RTE_EVENT_DEV_PRIORITY_LOWEST
		 */
		uint8_t step = (RTE_EVENT_DEV_PRIORITY_LOWEST + 1) /
				queue_count;
		for (i = 0; i < (int)queue_count; i++) {
			struct rte_event_queue_conf queue_conf;

			ret = rte_event_queue_default_conf_get(evdev, i,
						&queue_conf);
			RTE_TEST_ASSERT_SUCCESS(ret, "Failed to get def_conf%d",
					i);
			queue_conf.priority = i * step;
			ret = rte_event_queue_setup(evdev, i, &queue_conf);
			RTE_TEST_ASSERT_SUCCESS(ret, "Failed to setup queue=%d",
					i);
		}

	} else {
		/* Configure event queues with default priority */
		for (i = 0; i < (int)queue_count; i++) {
			ret = rte_event_queue_setup(evdev, i, NULL);
			RTE_TEST_ASSERT_SUCCESS(ret, "Failed to setup queue=%d",
					i);
		}
	}
	/* Configure event ports */
	uint32_t port_count;
	RTE_TEST_ASSERT_SUCCESS(rte_event_dev_attr_get(evdev,
				RTE_EVENT_DEV_ATTR_PORT_COUNT,
				&port_count), "Port count get failed");
	for (i = 0; i < (int)port_count; i++) {
		ret = rte_event_port_setup(evdev, i, NULL);
		RTE_TEST_ASSERT_SUCCESS(ret, "Failed to setup port=%d", i);
		ret = rte_event_port_link(evdev, i, NULL, NULL, 0);
		RTE_TEST_ASSERT(ret >= 0, "Failed to link all queues port=%d",
				i);
	}

	ret = rte_event_dev_start(evdev);
	RTE_TEST_ASSERT_SUCCESS(ret, "Failed to start device");

	return 0;
}

static int
eventdev_setup(void)
{
	return _eventdev_setup(TEST_EVENTDEV_SETUP_DEFAULT);
}

static void
eventdev_teardown(void)
{
	rte_event_dev_stop(evdev);
	rte_mempool_free(eventdev_test_mempool);
}

static void
update_event_and_validation_attr(struct rte_mbuf *m, struct rte_event *ev,
			uint32_t flow_id, uint8_t event_type,
			uint8_t sub_event_type, uint8_t sched_type,
			uint8_t queue, uint8_t port, uint8_t seq)
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
	attr->seq = seq;

	ev->flow_id = flow_id;
	ev->sub_event_type = sub_event_type;
	ev->event_type = event_type;
	/* Inject the new event */
	ev->op = RTE_EVENT_OP_NEW;
	ev->sched_type = sched_type;
	ev->queue_id = queue;
	ev->mbuf = m;
}

static int
inject_events(uint32_t flow_id, uint8_t event_type, uint8_t sub_event_type,
		uint8_t sched_type, uint8_t queue, uint8_t port,
		unsigned int events)
{
	struct rte_mbuf *m;
	unsigned int i;

	for (i = 0; i < events; i++) {
		struct rte_event ev = {.event = 0, .u64 = 0};

		m = rte_pktmbuf_alloc(eventdev_test_mempool);
		RTE_TEST_ASSERT_NOT_NULL(m, "mempool alloc failed");

		update_event_and_validation_attr(m, &ev, flow_id, event_type,
			sub_event_type, sched_type, queue, port, i);
		rte_event_enqueue_burst(evdev, port, &ev, 1);
	}
	return 0;
}

static int
check_excess_events(uint8_t port)
{
	int i;
	uint16_t valid_event;
	struct rte_event ev;

	/* Check for excess events, try for a few times and exit */
	for (i = 0; i < 32; i++) {
		valid_event = rte_event_dequeue_burst(evdev, port, &ev, 1, 0);

		RTE_TEST_ASSERT_SUCCESS(valid_event,
				"Unexpected valid event=%d",
				*dpaa2_seqn(ev.mbuf));
	}
	return 0;
}

static int
generate_random_events(const unsigned int total_events)
{
	struct rte_event_dev_info info;
	unsigned int i;
	int ret;

	uint32_t queue_count;
	RTE_TEST_ASSERT_SUCCESS(rte_event_dev_attr_get(evdev,
			    RTE_EVENT_DEV_ATTR_QUEUE_COUNT,
			    &queue_count), "Queue count get failed");

	ret = rte_event_dev_info_get(evdev, &info);
	RTE_TEST_ASSERT_SUCCESS(ret, "Failed to get event dev info");
	for (i = 0; i < total_events; i++) {
		ret = inject_events(
			rte_rand() % info.max_event_queue_flows /*flow_id */,
			RTE_EVENT_TYPE_CPU /* event_type */,
			rte_rand() % 256 /* sub_event_type */,
			rte_rand() % (RTE_SCHED_TYPE_PARALLEL + 1),
			rte_rand() % queue_count /* queue */,
			0 /* port */,
			1 /* events */);
		if (ret)
			return -1;
	}
	return ret;
}


static int
validate_event(struct rte_event *ev)
{
	struct event_attr *attr;

	attr = rte_pktmbuf_mtod(ev->mbuf, struct event_attr *);
	RTE_TEST_ASSERT_EQUAL(attr->flow_id, ev->flow_id,
			"flow_id mismatch enq=%d deq =%d",
			attr->flow_id, ev->flow_id);
	RTE_TEST_ASSERT_EQUAL(attr->event_type, ev->event_type,
			"event_type mismatch enq=%d deq =%d",
			attr->event_type, ev->event_type);
	RTE_TEST_ASSERT_EQUAL(attr->sub_event_type, ev->sub_event_type,
			"sub_event_type mismatch enq=%d deq =%d",
			attr->sub_event_type, ev->sub_event_type);
	RTE_TEST_ASSERT_EQUAL(attr->sched_type, ev->sched_type,
			"sched_type mismatch enq=%d deq =%d",
			attr->sched_type, ev->sched_type);
	RTE_TEST_ASSERT_EQUAL(attr->queue, ev->queue_id,
			"queue mismatch enq=%d deq =%d",
			attr->queue, ev->queue_id);
	return 0;
}

typedef int (*validate_event_cb)(uint32_t index, uint8_t port,
				 struct rte_event *ev);

static int
consume_events(uint8_t port, const uint32_t total_events, validate_event_cb fn)
{
	int ret;
	uint16_t valid_event;
	uint32_t events = 0, forward_progress_cnt = 0, index = 0;
	struct rte_event ev;

	while (1) {
		if (++forward_progress_cnt > UINT16_MAX) {
			dpaa2_evdev_err("Detected deadlock");
			return -1;
		}

		valid_event = rte_event_dequeue_burst(evdev, port, &ev, 1, 0);
		if (!valid_event)
			continue;

		forward_progress_cnt = 0;
		ret = validate_event(&ev);
		if (ret)
			return -1;

		if (fn != NULL) {
			ret = fn(index, port, &ev);
			RTE_TEST_ASSERT_SUCCESS(ret,
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
	struct event_attr *attr;

	attr = rte_pktmbuf_mtod(ev->mbuf, struct event_attr *);

	RTE_SET_USED(port);
	RTE_TEST_ASSERT_EQUAL(index, attr->seq,
		"index=%d != seqn=%d", index, attr->seq);
	return 0;
}

static int
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
		return -1;

	return consume_events(0 /* port */, MAX_EVENTS,	validate_simple_enqdeq);
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
		return -1;

	return consume_events(0 /* port */, MAX_EVENTS, NULL);
}

static int
worker_multi_port_fn(void *arg)
{
	struct test_core_param *param = arg;
	struct rte_event ev;
	uint16_t valid_event;
	uint8_t port = param->port;
	rte_atomic32_t *total_events = param->total_events;
	int ret;

	while (rte_atomic32_read(total_events) > 0) {
		valid_event = rte_event_dequeue_burst(evdev, port, &ev, 1, 0);
		if (!valid_event)
			continue;

		ret = validate_event(&ev);
		RTE_TEST_ASSERT_SUCCESS(ret, "Failed to validate event");
		rte_pktmbuf_free(ev.mbuf);
		rte_atomic32_sub(total_events, 1);
	}
	return 0;
}

static int
wait_workers_to_join(int lcore, const rte_atomic32_t *count)
{
	uint64_t cycles, print_cycles;

	RTE_SET_USED(count);

	print_cycles = cycles = rte_get_timer_cycles();
	while (rte_eal_get_lcore_state(lcore) != WAIT) {
		uint64_t new_cycles = rte_get_timer_cycles();

		if (new_cycles - print_cycles > rte_get_timer_hz()) {
			dpaa2_evdev_dbg("\r%s: events %d", __func__,
				rte_atomic32_read(count));
			print_cycles = new_cycles;
		}
		if (new_cycles - cycles > rte_get_timer_hz() * 10) {
			dpaa2_evdev_info(
				"%s: No schedules for seconds, deadlock (%d)",
				__func__,
				rte_atomic32_read(count));
			rte_event_dev_dump(evdev, stdout);
			cycles = new_cycles;
			return -1;
		}
	}
	rte_eal_mp_wait_lcore();
	return 0;
}


static int
launch_workers_and_wait(int (*main_worker)(void *),
			int (*workers)(void *), uint32_t total_events,
			uint8_t nb_workers, uint8_t sched_type)
{
	uint8_t port = 0;
	int w_lcore;
	int ret;
	struct test_core_param *param;
	rte_atomic32_t atomic_total_events;
	uint64_t dequeue_tmo_ticks;

	if (!nb_workers)
		return 0;

	rte_atomic32_set(&atomic_total_events, total_events);
	RTE_BUILD_BUG_ON(NUM_PACKETS < MAX_EVENTS);

	param = malloc(sizeof(struct test_core_param) * nb_workers);
	if (!param)
		return -1;

	ret = rte_event_dequeue_timeout_ticks(evdev,
		rte_rand() % 10000000/* 10ms */, &dequeue_tmo_ticks);
	if (ret) {
		free(param);
		return -1;
	}

	param[0].total_events = &atomic_total_events;
	param[0].sched_type = sched_type;
	param[0].port = 0;
	param[0].dequeue_tmo_ticks = dequeue_tmo_ticks;
	rte_smp_wmb();

	w_lcore = rte_get_next_lcore(
			/* start core */ -1,
			/* skip main */ 1,
			/* wrap */ 0);
	rte_eal_remote_launch(main_worker, &param[0], w_lcore);

	for (port = 1; port < nb_workers; port++) {
		param[port].total_events = &atomic_total_events;
		param[port].sched_type = sched_type;
		param[port].port = port;
		param[port].dequeue_tmo_ticks = dequeue_tmo_ticks;
		rte_smp_wmb();
		w_lcore = rte_get_next_lcore(w_lcore, 1, 0);
		rte_eal_remote_launch(workers, &param[port], w_lcore);
	}

	ret = wait_workers_to_join(w_lcore, &atomic_total_events);
	free(param);
	return ret;
}

/*
 * Generate a prescribed number of events and spread them across available
 * queues. Dequeue the events through multiple ports and verify the enqueued
 * event attributes
 */
static int
test_multi_queue_enq_multi_port_deq(void)
{
	const unsigned int total_events = MAX_EVENTS;
	uint32_t nr_ports;
	int ret;

	ret = generate_random_events(total_events);
	if (ret)
		return -1;

	RTE_TEST_ASSERT_SUCCESS(rte_event_dev_attr_get(evdev,
				RTE_EVENT_DEV_ATTR_PORT_COUNT,
				&nr_ports), "Port count get failed");
	nr_ports = RTE_MIN(nr_ports, rte_lcore_count() - 1);

	if (!nr_ports) {
		dpaa2_evdev_err("%s: Not enough ports=%d or workers=%d",
				__func__, nr_ports, rte_lcore_count() - 1);
		return 0;
	}

	return launch_workers_and_wait(worker_multi_port_fn,
					worker_multi_port_fn, total_events,
					nr_ports, 0xff /* invalid */);
}

static
void flush(uint8_t dev_id, struct rte_event event, void *arg)
{
	unsigned int *count = arg;

	RTE_SET_USED(dev_id);
	if (event.event_type == RTE_EVENT_TYPE_CPU)
		*count = *count + 1;

}

static int
test_dev_stop_flush(void)
{
	unsigned int total_events = MAX_EVENTS, count = 0;
	int ret;

	ret = generate_random_events(total_events);
	if (ret)
		return -1;

	ret = rte_event_dev_stop_flush_callback_register(evdev, flush, &count);
	if (ret)
		return -2;
	rte_event_dev_stop(evdev);
	ret = rte_event_dev_stop_flush_callback_register(evdev, NULL, NULL);
	if (ret)
		return -3;
	RTE_TEST_ASSERT_EQUAL(total_events, count,
				"count mismatch total_events=%d count=%d",
				total_events, count);
	return 0;
}

static int
validate_queue_to_port_single_link(uint32_t index, uint8_t port,
			struct rte_event *ev)
{
	RTE_SET_USED(index);
	RTE_TEST_ASSERT_EQUAL(port, ev->queue_id,
				"queue mismatch enq=%d deq =%d",
				port, ev->queue_id);
	return 0;
}

/*
 * Link queue x to port x and check correctness of link by checking
 * queue_id == x on dequeue on the specific port x
 */
static int
test_queue_to_port_single_link(void)
{
	int i, nr_links, ret;

	uint32_t port_count;

	RTE_TEST_ASSERT_SUCCESS(rte_event_dev_attr_get(evdev,
				RTE_EVENT_DEV_ATTR_PORT_COUNT,
				&port_count), "Port count get failed");

	/* Unlink all connections that created in eventdev_setup */
	for (i = 0; i < (int)port_count; i++) {
		ret = rte_event_port_unlink(evdev, i, NULL, 0);
		RTE_TEST_ASSERT(ret >= 0,
				"Failed to unlink all queues port=%d", i);
	}

	uint32_t queue_count;

	RTE_TEST_ASSERT_SUCCESS(rte_event_dev_attr_get(evdev,
			    RTE_EVENT_DEV_ATTR_QUEUE_COUNT,
			    &queue_count), "Queue count get failed");

	nr_links = RTE_MIN(port_count, queue_count);
	const unsigned int total_events = MAX_EVENTS / nr_links;

	/* Link queue x to port x and inject events to queue x through port x */
	for (i = 0; i < nr_links; i++) {
		uint8_t queue = (uint8_t)i;

		ret = rte_event_port_link(evdev, i, &queue, NULL, 1);
		RTE_TEST_ASSERT(ret == 1, "Failed to link queue to port %d", i);

		ret = inject_events(
			0x100 /*flow_id */,
			RTE_EVENT_TYPE_CPU /* event_type */,
			rte_rand() % 256 /* sub_event_type */,
			rte_rand() % (RTE_SCHED_TYPE_PARALLEL + 1),
			queue /* queue */,
			i /* port */,
			total_events /* events */);
		if (ret)
			return -1;
	}

	/* Verify the events generated from correct queue */
	for (i = 0; i < nr_links; i++) {
		ret = consume_events(i /* port */, total_events,
				validate_queue_to_port_single_link);
		if (ret)
			return -1;
	}

	return 0;
}

static int
validate_queue_to_port_multi_link(uint32_t index, uint8_t port,
			struct rte_event *ev)
{
	RTE_SET_USED(index);
	RTE_TEST_ASSERT_EQUAL(port, (ev->queue_id & 0x1),
				"queue mismatch enq=%d deq =%d",
				port, ev->queue_id);
	return 0;
}

/*
 * Link all even number of queues to port 0 and all odd number of queues to
 * port 1 and verify the link connection on dequeue
 */
static int
test_queue_to_port_multi_link(void)
{
	int ret, port0_events = 0, port1_events = 0;
	uint8_t queue, port;
	uint32_t nr_queues = 0;
	uint32_t nr_ports = 0;

	RTE_TEST_ASSERT_SUCCESS(rte_event_dev_attr_get(evdev,
			    RTE_EVENT_DEV_ATTR_QUEUE_COUNT,
			    &nr_queues), "Queue count get failed");

	RTE_TEST_ASSERT_SUCCESS(rte_event_dev_attr_get(evdev,
				RTE_EVENT_DEV_ATTR_QUEUE_COUNT,
				&nr_queues), "Queue count get failed");
	RTE_TEST_ASSERT_SUCCESS(rte_event_dev_attr_get(evdev,
				RTE_EVENT_DEV_ATTR_PORT_COUNT,
				&nr_ports), "Port count get failed");

	if (nr_ports < 2) {
		dpaa2_evdev_err("%s: Not enough ports to test ports=%d",
				__func__, nr_ports);
		return 0;
	}

	/* Unlink all connections that created in eventdev_setup */
	for (port = 0; port < nr_ports; port++) {
		ret = rte_event_port_unlink(evdev, port, NULL, 0);
		RTE_TEST_ASSERT(ret >= 0, "Failed to unlink all queues port=%d",
					port);
	}

	const unsigned int total_events = MAX_EVENTS / nr_queues;

	/* Link all even number of queues to port0 and odd numbers to port 1*/
	for (queue = 0; queue < nr_queues; queue++) {
		port = queue & 0x1;
		ret = rte_event_port_link(evdev, port, &queue, NULL, 1);
		RTE_TEST_ASSERT(ret == 1, "Failed to link queue=%d to port=%d",
					queue, port);

		ret = inject_events(
			0x100 /*flow_id */,
			RTE_EVENT_TYPE_CPU /* event_type */,
			rte_rand() % 256 /* sub_event_type */,
			rte_rand() % (RTE_SCHED_TYPE_PARALLEL + 1),
			queue /* queue */,
			port /* port */,
			total_events /* events */);
		if (ret)
			return -1;

		if (port == 0)
			port0_events += total_events;
		else
			port1_events += total_events;
	}

	ret = consume_events(0 /* port */, port0_events,
				validate_queue_to_port_multi_link);
	if (ret)
		return -1;
	ret = consume_events(1 /* port */, port1_events,
				validate_queue_to_port_multi_link);
	if (ret)
		return -1;

	return 0;
}

static void dpaa2_test_run(int (*setup)(void), void (*tdown)(void),
		int (*test)(void), const char *name)
{
	if (setup() < 0) {
		RTE_LOG(INFO, PMD, "Error setting up test %s", name);
		unsupported++;
	} else {
		if (test() < 0) {
			failed++;
			RTE_LOG(INFO, PMD, "%s Failed\n", name);
		} else {
			passed++;
			RTE_LOG(INFO, PMD, "%s Passed", name);
		}
	}

	total++;
	tdown();
}

int
test_eventdev_dpaa2(void)
{
	testsuite_setup();

	DPAA2_TEST_RUN(eventdev_setup, eventdev_teardown,
			test_simple_enqdeq_atomic);
	DPAA2_TEST_RUN(eventdev_setup, eventdev_teardown,
			test_simple_enqdeq_parallel);
	DPAA2_TEST_RUN(eventdev_setup, eventdev_teardown,
			test_multi_queue_enq_single_port_deq);
	DPAA2_TEST_RUN(eventdev_setup, eventdev_teardown,
			test_dev_stop_flush);
	DPAA2_TEST_RUN(eventdev_setup, eventdev_teardown,
			test_multi_queue_enq_multi_port_deq);
	DPAA2_TEST_RUN(eventdev_setup, eventdev_teardown,
			test_queue_to_port_single_link);
	DPAA2_TEST_RUN(eventdev_setup, eventdev_teardown,
			test_queue_to_port_multi_link);

	DPAA2_EVENTDEV_INFO("Total tests   : %d", total);
	DPAA2_EVENTDEV_INFO("Passed        : %d", passed);
	DPAA2_EVENTDEV_INFO("Failed        : %d", failed);
	DPAA2_EVENTDEV_INFO("Not supported : %d", unsupported);

	testsuite_teardown();

	if (failed)
		return -1;

	return 0;
}
