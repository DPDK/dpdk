/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 * Copyright(c) 2017-2018 Intel Corporation.
 */

#include <rte_atomic.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_debug.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_eventdev.h>
#include <rte_event_timer_adapter.h>
#include <rte_mempool.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_random.h>
#include <rte_bus_vdev.h>
#include <rte_service.h>
#include <stdbool.h>

#include "test.h"

/* 4K timers corresponds to sw evdev max inflight events */
#define MAX_TIMERS  (4 * 1024)
#define BKT_TCK_NSEC

#define NSECPERSEC 1E9
#define BATCH_SIZE 16
/* Both the app lcore and adapter ports are linked to this queue */
#define TEST_QUEUE_ID 0
/* Port the application dequeues from */
#define TEST_PORT_ID 0
#define TEST_ADAPTER_ID 0

/* Handle log statements in same manner as test macros */
#define LOG_DBG(...)	RTE_LOG(DEBUG, EAL, __VA_ARGS__)

static int evdev;
static struct rte_event_timer_adapter *timdev;
static struct rte_mempool *eventdev_test_mempool;
static struct rte_ring *timer_producer_ring;
static uint64_t global_bkt_tck_ns;
static uint64_t global_info_bkt_tck_ns;
static volatile uint8_t arm_done;

#define CALC_TICKS(tks)					\
	((tks * global_bkt_tck_ns) / global_info_bkt_tck_ns)


static bool using_services;
static uint32_t test_lcore1;
static uint32_t test_lcore2;
static uint32_t test_lcore3;
static uint32_t sw_evdev_slcore;
static uint32_t sw_adptr_slcore;

static inline void
devconf_set_default_sane_values(struct rte_event_dev_config *dev_conf,
		struct rte_event_dev_info *info)
{
	memset(dev_conf, 0, sizeof(struct rte_event_dev_config));
	dev_conf->dequeue_timeout_ns = info->min_dequeue_timeout_ns;
	dev_conf->nb_event_ports = 1;
	dev_conf->nb_event_queues = 1;
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

static inline int
eventdev_setup(void)
{
	int ret;
	struct rte_event_dev_config dev_conf;
	struct rte_event_dev_info info;
	uint32_t service_id;

	ret = rte_event_dev_info_get(evdev, &info);
	TEST_ASSERT_SUCCESS(ret, "Failed to get event dev info");
	TEST_ASSERT(info.max_num_events < 0 ||
			info.max_num_events >= (int32_t)MAX_TIMERS,
			"ERROR max_num_events=%d < max_events=%d",
			info.max_num_events, MAX_TIMERS);

	devconf_set_default_sane_values(&dev_conf, &info);
	ret = rte_event_dev_configure(evdev, &dev_conf);
	TEST_ASSERT_SUCCESS(ret, "Failed to configure eventdev");

	ret = rte_event_queue_setup(evdev, 0, NULL);
	TEST_ASSERT_SUCCESS(ret, "Failed to setup queue=%d", 0);

	/* Configure event port */
	ret = rte_event_port_setup(evdev, 0, NULL);
	TEST_ASSERT_SUCCESS(ret, "Failed to setup port=%d", 0);
	ret = rte_event_port_link(evdev, 0, NULL, NULL, 0);
	TEST_ASSERT(ret >= 0, "Failed to link all queues port=%d", 0);

	/* If this is a software event device, map and start its service */
	if (rte_event_dev_service_id_get(evdev, &service_id) == 0) {
		TEST_ASSERT_SUCCESS(rte_service_lcore_add(sw_evdev_slcore),
				"Failed to add service core");
		TEST_ASSERT_SUCCESS(rte_service_lcore_start(
				sw_evdev_slcore),
				"Failed to start service core");
		TEST_ASSERT_SUCCESS(rte_service_map_lcore_set(
				service_id, sw_evdev_slcore, 1),
				"Failed to map evdev service");
		TEST_ASSERT_SUCCESS(rte_service_runstate_set(
				service_id, 1),
				"Failed to start evdev service");
	}

	ret = rte_event_dev_start(evdev);
	TEST_ASSERT_SUCCESS(ret, "Failed to start device");

	return TEST_SUCCESS;
}

static int
testsuite_setup(void)
{
	/* Some of the multithreaded tests require 3 other lcores to run */
	unsigned int required_lcore_count = 4;
	uint32_t service_id;

	/* To make it easier to map services later if needed, just reset
	 * service core state.
	 */
	(void) rte_service_lcore_reset_all();

	if (!rte_event_dev_count()) {
		/* If there is no hardware eventdev, or no software vdev was
		 * specified on the command line, create an instance of
		 * event_sw.
		 */
		LOG_DBG("Failed to find a valid event device... testing with"
			" event_sw device\n");
		TEST_ASSERT_SUCCESS(rte_vdev_init("event_sw0", NULL),
					"Error creating eventdev");
		evdev = rte_event_dev_get_dev_id("event_sw0");
	}

	if (rte_event_dev_service_id_get(evdev, &service_id) == 0) {
		/* A software event device will use a software event timer
		 * adapter as well. 2 more cores required to convert to
		 * service cores.
		 */
		required_lcore_count += 2;
		using_services = true;
	}

	if (rte_lcore_count() < required_lcore_count) {
		printf("Not enough cores for event_timer_adapter_test, expecting at least %u\n",
		       required_lcore_count);
		return TEST_SKIPPED;
	}

	/* Assign lcores for various tasks */
	test_lcore1 = rte_get_next_lcore(-1, 1, 0);
	test_lcore2 = rte_get_next_lcore(test_lcore1, 1, 0);
	test_lcore3 = rte_get_next_lcore(test_lcore2, 1, 0);
	if (using_services) {
		sw_evdev_slcore = rte_get_next_lcore(test_lcore3, 1, 0);
		sw_adptr_slcore = rte_get_next_lcore(sw_evdev_slcore, 1, 0);
	}

	return eventdev_setup();
}

static void
testsuite_teardown(void)
{
	rte_event_dev_stop(evdev);
	rte_event_dev_close(evdev);
}

static int
setup_adapter_service(struct rte_event_timer_adapter *adptr)
{
	uint32_t adapter_service_id;
	int ret;

	/* retrieve service ids */
	TEST_ASSERT_SUCCESS(rte_event_timer_adapter_service_id_get(adptr,
			&adapter_service_id), "Failed to get event timer "
			"adapter service id");
	/* add a service core and start it */
	ret = rte_service_lcore_add(sw_adptr_slcore);
	TEST_ASSERT(ret == 0 || ret == -EALREADY,
			"Failed to add service core");
	ret = rte_service_lcore_start(sw_adptr_slcore);
	TEST_ASSERT(ret == 0 || ret == -EALREADY,
			"Failed to start service core");

	/* map services to it */
	TEST_ASSERT_SUCCESS(rte_service_map_lcore_set(adapter_service_id,
			sw_adptr_slcore, 1),
			"Failed to map adapter service");

	/* set services to running */
	TEST_ASSERT_SUCCESS(rte_service_runstate_set(adapter_service_id, 1),
			"Failed to start event timer adapter service");

	return TEST_SUCCESS;
}

static int
test_port_conf_cb(uint16_t id, uint8_t event_dev_id, uint8_t *event_port_id,
		  void *conf_arg)
{
	struct rte_event_dev_config dev_conf;
	struct rte_event_dev_info info;
	struct rte_event_port_conf *port_conf, def_port_conf = {0};
	uint32_t started;
	static int port_allocated;
	static uint8_t port_id;
	int ret;

	if (port_allocated) {
		*event_port_id = port_id;
		return 0;
	}

	RTE_SET_USED(id);

	ret = rte_event_dev_attr_get(event_dev_id, RTE_EVENT_DEV_ATTR_STARTED,
				     &started);
	if (ret < 0)
		return ret;

	if (started)
		rte_event_dev_stop(event_dev_id);

	ret = rte_event_dev_info_get(evdev, &info);
	if (ret < 0)
		return ret;

	devconf_set_default_sane_values(&dev_conf, &info);

	port_id = dev_conf.nb_event_ports;
	dev_conf.nb_event_ports++;

	ret = rte_event_dev_configure(event_dev_id, &dev_conf);
	if (ret < 0) {
		if (started)
			rte_event_dev_start(event_dev_id);
		return ret;
	}

	if (conf_arg != NULL)
		port_conf = conf_arg;
	else {
		port_conf = &def_port_conf;
		ret = rte_event_port_default_conf_get(event_dev_id, port_id,
						      port_conf);
		if (ret < 0)
			return ret;
	}

	ret = rte_event_port_setup(event_dev_id, port_id, port_conf);
	if (ret < 0)
		return ret;

	*event_port_id = port_id;

	if (started)
		rte_event_dev_start(event_dev_id);

	/* Reuse this port number next time this is called */
	port_allocated = 1;

	return 0;
}

static int
_timdev_setup(uint64_t max_tmo_ns, uint64_t bkt_tck_ns)
{
	struct rte_event_timer_adapter_info info;
	struct rte_event_timer_adapter_conf config = {
		.event_dev_id = evdev,
		.timer_adapter_id = TEST_ADAPTER_ID,
		.timer_tick_ns = bkt_tck_ns,
		.max_tmo_ns = max_tmo_ns,
		.nb_timers = MAX_TIMERS * 10,
		.flags = RTE_EVENT_TIMER_ADAPTER_F_ADJUST_RES,
	};
	uint32_t caps = 0;
	const char *pool_name = "timdev_test_pool";

	global_bkt_tck_ns = bkt_tck_ns;

	TEST_ASSERT_SUCCESS(rte_event_timer_adapter_caps_get(evdev, &caps),
				"failed to get adapter capabilities");
	if (!(caps & RTE_EVENT_TIMER_ADAPTER_CAP_INTERNAL_PORT)) {
		timdev = rte_event_timer_adapter_create_ext(&config,
							    test_port_conf_cb,
							    NULL);
		setup_adapter_service(timdev);
		using_services = true;
	} else
		timdev = rte_event_timer_adapter_create(&config);

	TEST_ASSERT_NOT_NULL(timdev,
			"failed to create event timer ring");

	TEST_ASSERT_EQUAL(rte_event_timer_adapter_start(timdev), 0,
			"failed to Start event timer adapter");

	/* Create event timer mempool */
	eventdev_test_mempool = rte_mempool_create(pool_name,
			MAX_TIMERS * 2,
			sizeof(struct rte_event_timer), /* element size*/
			0, /* cache size*/
			0, NULL, NULL, NULL, NULL,
			rte_socket_id(), 0);
	if (!eventdev_test_mempool) {
		printf("ERROR creating mempool\n");
		return TEST_FAILED;
	}

	rte_event_timer_adapter_get_info(timdev, &info);

	global_info_bkt_tck_ns = info.min_resolution_ns;

	return TEST_SUCCESS;
}

static int
timdev_setup_usec(void)
{
	return using_services ?
		/* Max timeout is 10,000us and bucket interval is 100us */
		_timdev_setup(1E7, 1E5) :
		/* Max timeout is 100us and bucket interval is 1us */
		_timdev_setup(1E5, 1E3);
}

static int
timdev_setup_usec_multicore(void)
{
	return using_services ?
		/* Max timeout is 10,000us and bucket interval is 100us */
		_timdev_setup(1E7, 1E5) :
		/* Max timeout is 100us and bucket interval is 1us */
		_timdev_setup(1E5, 1E3);
}

static int
timdev_setup_msec(void)
{
	/* Max timeout is 2 mins, and bucket interval is 100 ms */
	return _timdev_setup(180 * NSECPERSEC, NSECPERSEC / 10);
}

static int
timdev_setup_sec(void)
{
	/* Max timeout is 100sec and bucket interval is 1sec */
	return _timdev_setup(1E11, 1E9);
}

static int
timdev_setup_sec_multicore(void)
{
	/* Max timeout is 100sec and bucket interval is 1sec */
	return _timdev_setup(1E11, 1E9);
}

static void
timdev_teardown(void)
{
	rte_event_timer_adapter_stop(timdev);
	rte_event_timer_adapter_free(timdev);

	rte_mempool_free(eventdev_test_mempool);
}

static inline int
test_timer_state(void)
{
	struct rte_event_timer *ev_tim;
	struct rte_event ev;
	const struct rte_event_timer tim = {
		.ev.op = RTE_EVENT_OP_NEW,
		.ev.queue_id = 0,
		.ev.sched_type = RTE_SCHED_TYPE_ATOMIC,
		.ev.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
		.ev.event_type =  RTE_EVENT_TYPE_TIMER,
		.state = RTE_EVENT_TIMER_NOT_ARMED,
	};


	rte_mempool_get(eventdev_test_mempool, (void **)&ev_tim);
	*ev_tim = tim;
	ev_tim->ev.event_ptr = ev_tim;
	ev_tim->timeout_ticks = CALC_TICKS(120);

	TEST_ASSERT_EQUAL(rte_event_timer_arm_burst(timdev, &ev_tim, 1), 0,
			"Armed timer exceeding max_timeout.");
	TEST_ASSERT_EQUAL(ev_tim->state, RTE_EVENT_TIMER_ERROR_TOOLATE,
			"Improper timer state set expected %d returned %d",
			RTE_EVENT_TIMER_ERROR_TOOLATE, ev_tim->state);

	ev_tim->state = RTE_EVENT_TIMER_NOT_ARMED;
	ev_tim->timeout_ticks = CALC_TICKS(10);

	TEST_ASSERT_EQUAL(rte_event_timer_arm_burst(timdev, &ev_tim, 1), 1,
			"Failed to arm timer with proper timeout.");
	TEST_ASSERT_EQUAL(ev_tim->state, RTE_EVENT_TIMER_ARMED,
			"Improper timer state set expected %d returned %d",
			RTE_EVENT_TIMER_ARMED, ev_tim->state);

	if (!using_services)
		rte_delay_us(20);
	else
		rte_delay_us(1000 + 200);
	TEST_ASSERT_EQUAL(rte_event_dequeue_burst(evdev, 0, &ev, 1, 0), 1,
			"Armed timer failed to trigger.");

	ev_tim->state = RTE_EVENT_TIMER_NOT_ARMED;
	ev_tim->timeout_ticks = CALC_TICKS(90);
	TEST_ASSERT_EQUAL(rte_event_timer_arm_burst(timdev, &ev_tim, 1), 1,
			"Failed to arm timer with proper timeout.");
	TEST_ASSERT_EQUAL(rte_event_timer_cancel_burst(timdev, &ev_tim, 1),
			1, "Failed to cancel armed timer");
	TEST_ASSERT_EQUAL(ev_tim->state, RTE_EVENT_TIMER_CANCELED,
			"Improper timer state set expected %d returned %d",
			RTE_EVENT_TIMER_CANCELED, ev_tim->state);

	rte_mempool_put(eventdev_test_mempool, (void *)ev_tim);

	return TEST_SUCCESS;
}

static inline int
_arm_timers(uint64_t timeout_tcks, uint64_t timers)
{
	uint64_t i;
	struct rte_event_timer *ev_tim;
	const struct rte_event_timer tim = {
		.ev.op = RTE_EVENT_OP_NEW,
		.ev.queue_id = 0,
		.ev.sched_type = RTE_SCHED_TYPE_ATOMIC,
		.ev.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
		.ev.event_type =  RTE_EVENT_TYPE_TIMER,
		.state = RTE_EVENT_TIMER_NOT_ARMED,
		.timeout_ticks = CALC_TICKS(timeout_tcks),
	};

	for (i = 0; i < timers; i++) {

		TEST_ASSERT_SUCCESS(rte_mempool_get(eventdev_test_mempool,
					(void **)&ev_tim),
				"mempool alloc failed");
		*ev_tim = tim;
		ev_tim->ev.event_ptr = ev_tim;

		TEST_ASSERT_EQUAL(rte_event_timer_arm_burst(timdev, &ev_tim,
					1), 1, "Failed to arm timer %d",
				rte_errno);
	}

	return TEST_SUCCESS;
}

static inline int
_wait_timer_triggers(uint64_t wait_sec, uint64_t arm_count,
		uint64_t cancel_count)
{
	uint8_t valid_event;
	uint64_t events = 0;
	uint64_t wait_start, max_wait;
	struct rte_event ev;

	max_wait = rte_get_timer_hz() * wait_sec;
	wait_start = rte_get_timer_cycles();
	while (1) {
		if (rte_get_timer_cycles() - wait_start > max_wait) {
			if (events + cancel_count != arm_count)
				TEST_ASSERT_SUCCESS(max_wait,
					"Max time limit for timers exceeded.");
			break;
		}

		valid_event = rte_event_dequeue_burst(evdev, 0, &ev, 1, 0);
		if (!valid_event)
			continue;

		rte_mempool_put(eventdev_test_mempool, ev.event_ptr);
		events++;
	}

	return TEST_SUCCESS;
}

static inline int
test_timer_arm(void)
{
	TEST_ASSERT_SUCCESS(_arm_timers(20, MAX_TIMERS),
			"Failed to arm timers");
	TEST_ASSERT_SUCCESS(_wait_timer_triggers(10, MAX_TIMERS, 0),
			"Timer triggered count doesn't match arm count");
	return TEST_SUCCESS;
}

static int
_arm_wrapper(void *arg)
{
	RTE_SET_USED(arg);

	TEST_ASSERT_SUCCESS(_arm_timers(20, MAX_TIMERS),
			"Failed to arm timers");

	return TEST_SUCCESS;
}

static inline int
test_timer_arm_multicore(void)
{

	uint32_t lcore_1 = rte_get_next_lcore(-1, 1, 0);
	uint32_t lcore_2 = rte_get_next_lcore(lcore_1, 1, 0);

	rte_eal_remote_launch(_arm_wrapper, NULL, lcore_1);
	rte_eal_remote_launch(_arm_wrapper, NULL, lcore_2);

	rte_eal_mp_wait_lcore();
	TEST_ASSERT_SUCCESS(_wait_timer_triggers(10, MAX_TIMERS * 2, 0),
			"Timer triggered count doesn't match arm count");

	return TEST_SUCCESS;
}

#define MAX_BURST 16
static inline int
_arm_timers_burst(uint64_t timeout_tcks, uint64_t timers)
{
	uint64_t i;
	int j;
	struct rte_event_timer *ev_tim[MAX_BURST];
	const struct rte_event_timer tim = {
		.ev.op = RTE_EVENT_OP_NEW,
		.ev.queue_id = 0,
		.ev.sched_type = RTE_SCHED_TYPE_ATOMIC,
		.ev.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
		.ev.event_type =  RTE_EVENT_TYPE_TIMER,
		.state = RTE_EVENT_TIMER_NOT_ARMED,
		.timeout_ticks = CALC_TICKS(timeout_tcks),
	};

	for (i = 0; i < timers / MAX_BURST; i++) {
		TEST_ASSERT_SUCCESS(rte_mempool_get_bulk(
				eventdev_test_mempool,
				(void **)ev_tim, MAX_BURST),
				"mempool alloc failed");

		for (j = 0; j < MAX_BURST; j++) {
			*ev_tim[j] = tim;
			ev_tim[j]->ev.event_ptr = ev_tim[j];
		}

		TEST_ASSERT_EQUAL(rte_event_timer_arm_tmo_tick_burst(timdev,
				ev_tim, tim.timeout_ticks, MAX_BURST),
				MAX_BURST, "Failed to arm timer %d", rte_errno);
	}

	return TEST_SUCCESS;
}

static inline int
test_timer_arm_burst(void)
{
	TEST_ASSERT_SUCCESS(_arm_timers_burst(20, MAX_TIMERS),
			"Failed to arm timers");
	TEST_ASSERT_SUCCESS(_wait_timer_triggers(10, MAX_TIMERS, 0),
			"Timer triggered count doesn't match arm count");

	return TEST_SUCCESS;
}

static int
_arm_wrapper_burst(void *arg)
{
	RTE_SET_USED(arg);

	TEST_ASSERT_SUCCESS(_arm_timers_burst(20, MAX_TIMERS),
			"Failed to arm timers");

	return TEST_SUCCESS;
}

static inline int
test_timer_arm_burst_multicore(void)
{
	rte_eal_remote_launch(_arm_wrapper_burst, NULL, test_lcore1);
	rte_eal_remote_launch(_arm_wrapper_burst, NULL, test_lcore2);

	rte_eal_mp_wait_lcore();
	TEST_ASSERT_SUCCESS(_wait_timer_triggers(10, MAX_TIMERS * 2, 0),
			"Timer triggered count doesn't match arm count");

	return TEST_SUCCESS;
}

static inline int
test_timer_cancel(void)
{
	uint64_t i;
	struct rte_event_timer *ev_tim;
	const struct rte_event_timer tim = {
		.ev.op = RTE_EVENT_OP_NEW,
		.ev.queue_id = 0,
		.ev.sched_type = RTE_SCHED_TYPE_ATOMIC,
		.ev.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
		.ev.event_type =  RTE_EVENT_TYPE_TIMER,
		.state = RTE_EVENT_TIMER_NOT_ARMED,
		.timeout_ticks = CALC_TICKS(20),
	};

	for (i = 0; i < MAX_TIMERS; i++) {
		TEST_ASSERT_SUCCESS(rte_mempool_get(eventdev_test_mempool,
					(void **)&ev_tim),
				"mempool alloc failed");
		*ev_tim = tim;
		ev_tim->ev.event_ptr = ev_tim;

		TEST_ASSERT_EQUAL(rte_event_timer_arm_burst(timdev, &ev_tim,
					1), 1, "Failed to arm timer %d",
				rte_errno);

		rte_delay_us(100 + (i % 5000));

		TEST_ASSERT_EQUAL(rte_event_timer_cancel_burst(timdev,
					&ev_tim, 1), 1,
				"Failed to cancel event timer %d", rte_errno);
		rte_mempool_put(eventdev_test_mempool, ev_tim);
	}


	TEST_ASSERT_SUCCESS(_wait_timer_triggers(30, MAX_TIMERS,
				MAX_TIMERS),
		"Timer triggered count doesn't match arm, cancel count");

	return TEST_SUCCESS;
}

static int
_cancel_producer(uint64_t timeout_tcks, uint64_t timers)
{
	uint64_t i;
	struct rte_event_timer *ev_tim;
	const struct rte_event_timer tim = {
		.ev.op = RTE_EVENT_OP_NEW,
		.ev.queue_id = 0,
		.ev.sched_type = RTE_SCHED_TYPE_ATOMIC,
		.ev.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
		.ev.event_type =  RTE_EVENT_TYPE_TIMER,
		.state = RTE_EVENT_TIMER_NOT_ARMED,
		.timeout_ticks = CALC_TICKS(timeout_tcks),
	};

	for (i = 0; i < timers; i++) {
		TEST_ASSERT_SUCCESS(rte_mempool_get(eventdev_test_mempool,
					(void **)&ev_tim),
				"mempool alloc failed");

		*ev_tim = tim;
		ev_tim->ev.event_ptr = ev_tim;

		TEST_ASSERT_EQUAL(rte_event_timer_arm_burst(timdev, &ev_tim,
					1), 1, "Failed to arm timer %d",
				rte_errno);

		TEST_ASSERT_EQUAL(ev_tim->state, RTE_EVENT_TIMER_ARMED,
				  "Failed to arm event timer");

		while (rte_ring_enqueue(timer_producer_ring, ev_tim) != 0)
			;
	}

	return TEST_SUCCESS;
}

static int
_cancel_producer_burst(uint64_t timeout_tcks, uint64_t timers)
{

	uint64_t i;
	int j, ret;
	struct rte_event_timer *ev_tim[MAX_BURST];
	const struct rte_event_timer tim = {
		.ev.op = RTE_EVENT_OP_NEW,
		.ev.queue_id = 0,
		.ev.sched_type = RTE_SCHED_TYPE_ATOMIC,
		.ev.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
		.ev.event_type =  RTE_EVENT_TYPE_TIMER,
		.state = RTE_EVENT_TIMER_NOT_ARMED,
		.timeout_ticks = CALC_TICKS(timeout_tcks),
	};
	int arm_count = 0;

	for (i = 0; i < timers / MAX_BURST; i++) {
		TEST_ASSERT_SUCCESS(rte_mempool_get_bulk(
				eventdev_test_mempool,
				(void **)ev_tim, MAX_BURST),
				"mempool alloc failed");

		for (j = 0; j < MAX_BURST; j++) {
			*ev_tim[j] = tim;
			ev_tim[j]->ev.event_ptr = ev_tim[j];
		}

		TEST_ASSERT_EQUAL(rte_event_timer_arm_tmo_tick_burst(timdev,
				ev_tim, tim.timeout_ticks, MAX_BURST),
				MAX_BURST, "Failed to arm timer %d", rte_errno);

		for (j = 0; j < MAX_BURST; j++)
			TEST_ASSERT_EQUAL(ev_tim[j]->state,
					  RTE_EVENT_TIMER_ARMED,
					  "Event timer not armed, state = %d",
					  ev_tim[j]->state);

		ret = rte_ring_enqueue_bulk(timer_producer_ring,
				(void **)ev_tim, MAX_BURST, NULL);
		TEST_ASSERT_EQUAL(ret, MAX_BURST,
				"Failed to enqueue event timers to ring");
		arm_count += ret;
	}

	TEST_ASSERT_EQUAL(arm_count, MAX_TIMERS,
			  "Failed to arm expected number of event timers");

	return TEST_SUCCESS;
}

static int
_cancel_producer_wrapper(void *args)
{
	RTE_SET_USED(args);

	return _cancel_producer(20, MAX_TIMERS);
}

static int
_cancel_producer_burst_wrapper(void *args)
{
	RTE_SET_USED(args);

	return _cancel_producer_burst(100, MAX_TIMERS);
}

static int
_cancel_thread(void *args)
{
	RTE_SET_USED(args);
	struct rte_event_timer *ev_tim = NULL;
	uint64_t cancel_count = 0;
	uint16_t ret;

	while (!arm_done || rte_ring_count(timer_producer_ring) > 0) {
		if (rte_ring_dequeue(timer_producer_ring, (void **)&ev_tim))
			continue;

		ret = rte_event_timer_cancel_burst(timdev, &ev_tim, 1);
		TEST_ASSERT_EQUAL(ret, 1, "Failed to cancel timer");
		rte_mempool_put(eventdev_test_mempool, (void *)ev_tim);
		cancel_count++;
	}

	return TEST_SUCCESS;
}

static int
_cancel_burst_thread(void *args)
{
	RTE_SET_USED(args);

	int ret, i, n;
	struct rte_event_timer *ev_tim[MAX_BURST];
	uint64_t cancel_count = 0;
	uint64_t dequeue_count = 0;

	while (!arm_done || rte_ring_count(timer_producer_ring) > 0) {
		n = rte_ring_dequeue_burst(timer_producer_ring,
				(void **)ev_tim, MAX_BURST, NULL);
		if (!n)
			continue;

		dequeue_count += n;

		for (i = 0; i < n; i++)
			TEST_ASSERT_EQUAL(ev_tim[i]->state,
					  RTE_EVENT_TIMER_ARMED,
					  "Event timer not armed, state = %d",
					  ev_tim[i]->state);

		ret = rte_event_timer_cancel_burst(timdev, ev_tim, n);
		TEST_ASSERT_EQUAL(n, ret, "Failed to cancel complete burst of "
				  "event timers");
		rte_mempool_put_bulk(eventdev_test_mempool, (void **)ev_tim,
				RTE_MIN(ret, MAX_BURST));

		cancel_count += ret;
	}

	TEST_ASSERT_EQUAL(cancel_count, MAX_TIMERS,
			  "Failed to cancel expected number of timers: "
			  "expected = %d, cancel_count = %"PRIu64", "
			  "dequeue_count = %"PRIu64"\n", MAX_TIMERS,
			  cancel_count, dequeue_count);

	return TEST_SUCCESS;
}

static inline int
test_timer_cancel_multicore(void)
{
	arm_done = 0;
	timer_producer_ring = rte_ring_create("timer_cancel_queue",
			MAX_TIMERS * 2, rte_socket_id(), 0);
	TEST_ASSERT_NOT_NULL(timer_producer_ring,
			"Unable to reserve memory for ring");

	rte_eal_remote_launch(_cancel_thread, NULL, test_lcore3);
	rte_eal_remote_launch(_cancel_producer_wrapper, NULL, test_lcore1);
	rte_eal_remote_launch(_cancel_producer_wrapper, NULL, test_lcore2);

	rte_eal_wait_lcore(test_lcore1);
	rte_eal_wait_lcore(test_lcore2);
	arm_done = 1;
	rte_eal_wait_lcore(test_lcore3);
	rte_ring_free(timer_producer_ring);

	TEST_ASSERT_SUCCESS(_wait_timer_triggers(30, MAX_TIMERS * 2,
			MAX_TIMERS * 2),
			"Timer triggered count doesn't match arm count");

	return TEST_SUCCESS;
}

static inline int
test_timer_cancel_burst_multicore(void)
{
	arm_done = 0;
	timer_producer_ring = rte_ring_create("timer_cancel_queue",
			MAX_TIMERS * 2, rte_socket_id(), 0);
	TEST_ASSERT_NOT_NULL(timer_producer_ring,
			"Unable to reserve memory for ring");

	rte_eal_remote_launch(_cancel_burst_thread, NULL, test_lcore2);
	rte_eal_remote_launch(_cancel_producer_burst_wrapper, NULL,
			test_lcore1);

	rte_eal_wait_lcore(test_lcore1);
	arm_done = 1;
	rte_eal_wait_lcore(test_lcore2);
	rte_ring_free(timer_producer_ring);

	TEST_ASSERT_SUCCESS(_wait_timer_triggers(30, MAX_TIMERS,
			MAX_TIMERS),
			"Timer triggered count doesn't match arm count");

	return TEST_SUCCESS;
}

static inline int
test_timer_cancel_random(void)
{
	uint64_t i;
	uint64_t events_canceled = 0;
	struct rte_event_timer *ev_tim;
	const struct rte_event_timer tim = {
		.ev.op = RTE_EVENT_OP_NEW,
		.ev.queue_id = 0,
		.ev.sched_type = RTE_SCHED_TYPE_ATOMIC,
		.ev.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
		.ev.event_type =  RTE_EVENT_TYPE_TIMER,
		.state = RTE_EVENT_TIMER_NOT_ARMED,
		.timeout_ticks = CALC_TICKS(20),
	};

	for (i = 0; i < MAX_TIMERS; i++) {

		TEST_ASSERT_SUCCESS(rte_mempool_get(eventdev_test_mempool,
					(void **)&ev_tim),
				"mempool alloc failed");
		*ev_tim = tim;
		ev_tim->ev.event_ptr = ev_tim;

		TEST_ASSERT_EQUAL(rte_event_timer_arm_burst(timdev, &ev_tim,
					1), 1, "Failed to arm timer %d",
				rte_errno);

		if (rte_rand() & 1) {
			rte_delay_us(100 + (i % 5000));
			TEST_ASSERT_EQUAL(rte_event_timer_cancel_burst(
						timdev,
						&ev_tim, 1), 1,
				"Failed to cancel event timer %d", rte_errno);
			rte_mempool_put(eventdev_test_mempool, ev_tim);
			events_canceled++;
		}
	}

	TEST_ASSERT_SUCCESS(_wait_timer_triggers(30, MAX_TIMERS,
				events_canceled),
		       "Timer triggered count doesn't match arm, cancel count");

	return TEST_SUCCESS;
}

/* Check that the adapter can be created correctly */
static int
adapter_create(void)
{
	int adapter_id = 0;
	struct rte_event_timer_adapter *adapter, *adapter2;

	struct rte_event_timer_adapter_conf conf = {
		.event_dev_id = evdev + 1,  // invalid event dev id
		.timer_adapter_id = adapter_id,
		.clk_src = RTE_EVENT_TIMER_ADAPTER_CPU_CLK,
		.timer_tick_ns = NSECPERSEC / 10,
		.max_tmo_ns = 180 * NSECPERSEC,
		.nb_timers = MAX_TIMERS,
		.flags = RTE_EVENT_TIMER_ADAPTER_F_ADJUST_RES,
	};
	uint32_t caps = 0;

	/* Test invalid conf */
	adapter = rte_event_timer_adapter_create(&conf);
	TEST_ASSERT_NULL(adapter, "Created adapter with invalid "
			"event device id");
	TEST_ASSERT_EQUAL(rte_errno, EINVAL, "Incorrect errno value for "
			"invalid event device id");

	/* Test valid conf */
	conf.event_dev_id = evdev;
	TEST_ASSERT_SUCCESS(rte_event_timer_adapter_caps_get(evdev, &caps),
			"failed to get adapter capabilities");
	if (!(caps & RTE_EVENT_TIMER_ADAPTER_CAP_INTERNAL_PORT))
		adapter = rte_event_timer_adapter_create_ext(&conf,
				test_port_conf_cb,
				NULL);
	else
		adapter = rte_event_timer_adapter_create(&conf);
	TEST_ASSERT_NOT_NULL(adapter, "Failed to create adapter with valid "
			"configuration");

	/* Test existing id */
	adapter2 = rte_event_timer_adapter_create(&conf);
	TEST_ASSERT_NULL(adapter2, "Created adapter with in-use id");
	TEST_ASSERT(rte_errno == EEXIST, "Incorrect errno value for existing "
			"id");

	TEST_ASSERT_SUCCESS(rte_event_timer_adapter_free(adapter),
			"Failed to free adapter");

	rte_mempool_free(eventdev_test_mempool);

	return TEST_SUCCESS;
}


/* Test that adapter can be freed correctly. */
static int
adapter_free(void)
{
	TEST_ASSERT_SUCCESS(rte_event_timer_adapter_stop(timdev),
			"Failed to stop adapter");

	TEST_ASSERT_SUCCESS(rte_event_timer_adapter_free(timdev),
			"Failed to free valid adapter");

	/* Test free of already freed adapter */
	TEST_ASSERT_FAIL(rte_event_timer_adapter_free(timdev),
			"Freed adapter that was already freed");

	/* Test free of null adapter */
	timdev = NULL;
	TEST_ASSERT_FAIL(rte_event_timer_adapter_free(timdev),
			"Freed null adapter");

	rte_mempool_free(eventdev_test_mempool);

	return TEST_SUCCESS;
}

/* Test that adapter info can be retrieved and is correct. */
static int
adapter_get_info(void)
{
	struct rte_event_timer_adapter_info info;

	TEST_ASSERT_SUCCESS(rte_event_timer_adapter_get_info(timdev, &info),
			"Failed to get adapter info");

	if (using_services)
		TEST_ASSERT_EQUAL(info.event_dev_port_id, 1,
				"Expected port id = 1, got port id = %d",
				info.event_dev_port_id);

	return TEST_SUCCESS;
}

/* Test adapter lookup via adapter ID. */
static int
adapter_lookup(void)
{
	struct rte_event_timer_adapter *adapter;

	adapter = rte_event_timer_adapter_lookup(TEST_ADAPTER_ID);
	TEST_ASSERT_NOT_NULL(adapter, "Failed to lookup adapter");

	return TEST_SUCCESS;
}

static int
adapter_start(void)
{
	TEST_ASSERT_SUCCESS(_timdev_setup(180 * NSECPERSEC,
			NSECPERSEC / 10),
			"Failed to start adapter");
	TEST_ASSERT_EQUAL(rte_event_timer_adapter_start(timdev), -EALREADY,
			"Timer adapter started without call to stop.");

	return TEST_SUCCESS;
}

/* Test that adapter stops correctly. */
static int
adapter_stop(void)
{
	struct rte_event_timer_adapter *l_adapter = NULL;

	/* Test adapter stop */
	TEST_ASSERT_SUCCESS(rte_event_timer_adapter_stop(timdev),
			"Failed to stop event adapter");

	TEST_ASSERT_FAIL(rte_event_timer_adapter_stop(l_adapter),
			"Erroneously stopped null event adapter");

	TEST_ASSERT_SUCCESS(rte_event_timer_adapter_free(timdev),
			"Failed to free adapter");

	rte_mempool_free(eventdev_test_mempool);

	return TEST_SUCCESS;
}

/* Test increment and reset of ev_enq_count stat */
static int
stat_inc_reset_ev_enq(void)
{
	int ret, i, n;
	int num_evtims = MAX_TIMERS;
	struct rte_event_timer *evtims[num_evtims];
	struct rte_event evs[BATCH_SIZE];
	struct rte_event_timer_adapter_stats stats;
	const struct rte_event_timer init_tim = {
		.ev.op = RTE_EVENT_OP_NEW,
		.ev.queue_id = TEST_QUEUE_ID,
		.ev.sched_type = RTE_SCHED_TYPE_ATOMIC,
		.ev.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
		.ev.event_type =  RTE_EVENT_TYPE_TIMER,
		.state = RTE_EVENT_TIMER_NOT_ARMED,
		.timeout_ticks = CALC_TICKS(5), // expire in .5 sec
	};

	ret = rte_mempool_get_bulk(eventdev_test_mempool, (void **)evtims,
				   num_evtims);
	TEST_ASSERT_EQUAL(ret, 0, "Failed to get array of timer objs: ret = %d",
			  ret);

	for (i = 0; i < num_evtims; i++) {
		*evtims[i] = init_tim;
		evtims[i]->ev.event_ptr = evtims[i];
	}

	ret = rte_event_timer_adapter_stats_get(timdev, &stats);
	TEST_ASSERT_EQUAL(ret, 0, "Failed to get stats");
	TEST_ASSERT_EQUAL((int)stats.ev_enq_count, 0, "Stats not clear at "
			  "startup");

	/* Test with the max value for the adapter */
	ret = rte_event_timer_arm_burst(timdev, evtims, num_evtims);
	TEST_ASSERT_EQUAL(ret, num_evtims,
			  "Failed to arm all event timers: attempted = %d, "
			  "succeeded = %d, rte_errno = %s",
			  num_evtims, ret, rte_strerror(rte_errno));

	rte_delay_ms(1000);

#define MAX_TRIES num_evtims
	int sum = 0;
	int tries = 0;
	bool done = false;
	while (!done) {
		sum += rte_event_dequeue_burst(evdev, TEST_PORT_ID, evs,
					       RTE_DIM(evs), 10);
		if (sum >= num_evtims || ++tries >= MAX_TRIES)
			done = true;

		rte_delay_ms(10);
	}

	TEST_ASSERT_EQUAL(sum, num_evtims, "Expected %d timer expiry events, "
			  "got %d", num_evtims, sum);

	TEST_ASSERT(tries < MAX_TRIES, "Exceeded max tries");

	rte_delay_ms(100);

	/* Make sure the eventdev is still empty */
	n = rte_event_dequeue_burst(evdev, TEST_PORT_ID, evs, RTE_DIM(evs),
				      10);

	TEST_ASSERT_EQUAL(n, 0, "Dequeued unexpected number of timer expiry "
			  "events from event device");

	/* Check stats again */
	ret = rte_event_timer_adapter_stats_get(timdev, &stats);
	TEST_ASSERT_EQUAL(ret, 0, "Failed to get stats");
	TEST_ASSERT_EQUAL((int)stats.ev_enq_count, num_evtims,
			  "Expected enqueue stat = %d; got %d", num_evtims,
			  (int)stats.ev_enq_count);

	/* Reset and check again */
	ret = rte_event_timer_adapter_stats_reset(timdev);
	TEST_ASSERT_EQUAL(ret, 0, "Failed to reset stats");

	ret = rte_event_timer_adapter_stats_get(timdev, &stats);
	TEST_ASSERT_EQUAL(ret, 0, "Failed to get stats");
	TEST_ASSERT_EQUAL((int)stats.ev_enq_count, 0,
			  "Expected enqueue stat = %d; got %d", 0,
			  (int)stats.ev_enq_count);

	rte_mempool_put_bulk(eventdev_test_mempool, (void **)evtims,
			     num_evtims);

	return TEST_SUCCESS;
}

/* Test various cases in arming timers */
static int
event_timer_arm(void)
{
	uint16_t n;
	int ret;
	struct rte_event_timer_adapter *adapter = timdev;
	struct rte_event_timer *evtim = NULL;
	struct rte_event evs[BATCH_SIZE];
	const struct rte_event_timer init_tim = {
		.ev.op = RTE_EVENT_OP_NEW,
		.ev.queue_id = TEST_QUEUE_ID,
		.ev.sched_type = RTE_SCHED_TYPE_ATOMIC,
		.ev.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
		.ev.event_type =  RTE_EVENT_TYPE_TIMER,
		.state = RTE_EVENT_TIMER_NOT_ARMED,
		.timeout_ticks = CALC_TICKS(5), // expire in .5 sec
	};

	rte_mempool_get(eventdev_test_mempool, (void **)&evtim);
	if (evtim == NULL) {
		/* Failed to get an event timer object */
		return TEST_FAILED;
	}

	/* Set up a timer */
	*evtim = init_tim;
	evtim->ev.event_ptr = evtim;

	/* Test single timer arm succeeds */
	ret = rte_event_timer_arm_burst(adapter, &evtim, 1);
	TEST_ASSERT_EQUAL(ret, 1, "Failed to arm event timer: %s\n",
			  rte_strerror(rte_errno));
	TEST_ASSERT_EQUAL(evtim->state, RTE_EVENT_TIMER_ARMED, "Event timer "
			  "in incorrect state");

	/* Test arm of armed timer fails */
	ret = rte_event_timer_arm_burst(adapter, &evtim, 1);
	TEST_ASSERT_EQUAL(ret, 0, "expected return value from "
			  "rte_event_timer_arm_burst: 0, got: %d", ret);
	TEST_ASSERT_EQUAL(rte_errno, EALREADY, "Unexpected rte_errno value "
			  "after arming already armed timer");

	/* Let timer expire */
	rte_delay_ms(1000);

	n = rte_event_dequeue_burst(evdev, TEST_PORT_ID, evs, RTE_DIM(evs), 0);
	TEST_ASSERT_EQUAL(n, 1, "Failed to dequeue expected number of expiry "
			  "events from event device");

	rte_mempool_put(eventdev_test_mempool, evtim);

	return TEST_SUCCESS;
}

/* This test checks that repeated references to the same event timer in the
 * arm request work as expected; only the first one through should succeed.
 */
static int
event_timer_arm_double(void)
{
	uint16_t n;
	int ret;
	struct rte_event_timer_adapter *adapter = timdev;
	struct rte_event_timer *evtim = NULL;
	struct rte_event evs[BATCH_SIZE];
	const struct rte_event_timer init_tim = {
		.ev.op = RTE_EVENT_OP_NEW,
		.ev.queue_id = TEST_QUEUE_ID,
		.ev.sched_type = RTE_SCHED_TYPE_ATOMIC,
		.ev.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
		.ev.event_type =  RTE_EVENT_TYPE_TIMER,
		.state = RTE_EVENT_TIMER_NOT_ARMED,
		.timeout_ticks = CALC_TICKS(5), // expire in .5 sec
	};

	rte_mempool_get(eventdev_test_mempool, (void **)&evtim);
	if (evtim == NULL) {
		/* Failed to get an event timer object */
		return TEST_FAILED;
	}

	/* Set up a timer */
	*evtim = init_tim;
	evtim->ev.event_ptr = evtim;

	struct rte_event_timer *evtim_arr[] = {evtim, evtim};
	ret = rte_event_timer_arm_burst(adapter, evtim_arr, RTE_DIM(evtim_arr));
	TEST_ASSERT_EQUAL(ret, 1, "Unexpected return value from "
			  "rte_event_timer_arm_burst");
	TEST_ASSERT_EQUAL(rte_errno, EALREADY, "Unexpected rte_errno value "
			  "after double-arm");

	/* Let timer expire */
	rte_delay_ms(600);

	n = rte_event_dequeue_burst(evdev, TEST_PORT_ID, evs, RTE_DIM(evs), 0);
	TEST_ASSERT_EQUAL(n, 1, "Dequeued incorrect number of expiry events - "
			  "expected: 1, actual: %d", n);

	rte_mempool_put(eventdev_test_mempool, evtim);

	return TEST_SUCCESS;
}

/* Test the timer expiry event is generated at the expected time.  */
static int
event_timer_arm_expiry(void)
{
	uint16_t n;
	int ret;
	struct rte_event_timer_adapter *adapter = timdev;
	struct rte_event_timer *evtim = NULL;
	struct rte_event_timer *evtim2 = NULL;
	struct rte_event evs[BATCH_SIZE];
	const struct rte_event_timer init_tim = {
		.ev.op = RTE_EVENT_OP_NEW,
		.ev.queue_id = TEST_QUEUE_ID,
		.ev.sched_type = RTE_SCHED_TYPE_ATOMIC,
		.ev.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
		.ev.event_type =  RTE_EVENT_TYPE_TIMER,
		.state = RTE_EVENT_TIMER_NOT_ARMED,
	};

	rte_mempool_get(eventdev_test_mempool, (void **)&evtim);
	if (evtim == NULL) {
		/* Failed to get an event timer object */
		return TEST_FAILED;
	}

	/* Set up an event timer */
	*evtim = init_tim;
	evtim->timeout_ticks = CALC_TICKS(30),	// expire in 3 secs
	evtim->ev.event_ptr = evtim;

	ret = rte_event_timer_arm_burst(adapter, &evtim, 1);
	TEST_ASSERT_EQUAL(ret, 1, "Failed to arm event timer: %s",
			  rte_strerror(rte_errno));
	TEST_ASSERT_EQUAL(evtim->state, RTE_EVENT_TIMER_ARMED, "Event "
			  "timer in incorrect state");

	rte_delay_ms(2999);

	n = rte_event_dequeue_burst(evdev, TEST_PORT_ID, evs, RTE_DIM(evs), 0);
	TEST_ASSERT_EQUAL(n, 0, "Dequeued unexpected timer expiry event");

	/* Delay 100 ms to account for the adapter tick window - should let us
	 * dequeue one event
	 */
	rte_delay_ms(100);

	n = rte_event_dequeue_burst(evdev, TEST_PORT_ID, evs, RTE_DIM(evs), 0);
	TEST_ASSERT_EQUAL(n, 1, "Dequeued incorrect number (%d) of timer "
			  "expiry events", n);
	TEST_ASSERT_EQUAL(evs[0].event_type, RTE_EVENT_TYPE_TIMER,
			  "Dequeued unexpected type of event");

	/* Check that we recover the original event timer and then free it */
	evtim2 = evs[0].event_ptr;
	TEST_ASSERT_EQUAL(evtim, evtim2,
			  "Failed to recover pointer to original event timer");
	rte_mempool_put(eventdev_test_mempool, evtim2);

	return TEST_SUCCESS;
}

/* Check that rearming a timer works as expected. */
static int
event_timer_arm_rearm(void)
{
	uint16_t n;
	int ret;
	struct rte_event_timer *evtim = NULL;
	struct rte_event_timer *evtim2 = NULL;
	struct rte_event evs[BATCH_SIZE];
	const struct rte_event_timer init_tim = {
		.ev.op = RTE_EVENT_OP_NEW,
		.ev.queue_id = TEST_QUEUE_ID,
		.ev.sched_type = RTE_SCHED_TYPE_ATOMIC,
		.ev.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
		.ev.event_type = RTE_EVENT_TYPE_TIMER,
		.state = RTE_EVENT_TIMER_NOT_ARMED,
	};

	rte_mempool_get(eventdev_test_mempool, (void **)&evtim);
	if (evtim == NULL) {
		/* Failed to get an event timer object */
		return TEST_FAILED;
	}

	/* Set up a timer */
	*evtim = init_tim;
	evtim->timeout_ticks = CALC_TICKS(1);  // expire in 0.1 sec
	evtim->ev.event_ptr = evtim;

	/* Arm it */
	ret = rte_event_timer_arm_burst(timdev, &evtim, 1);
	TEST_ASSERT_EQUAL(ret, 1, "Failed to arm event timer: %s\n",
			  rte_strerror(rte_errno));

	/* Add 100ms to account for the adapter tick window */
	rte_delay_ms(100 + 100);

	n = rte_event_dequeue_burst(evdev, TEST_PORT_ID, evs, RTE_DIM(evs), 0);
	TEST_ASSERT_EQUAL(n, 1, "Failed to dequeue expected number of expiry "
			  "events from event device");

	/* Recover the timer through the event that was dequeued. */
	evtim2 = evs[0].event_ptr;
	TEST_ASSERT_EQUAL(evtim, evtim2,
			  "Failed to recover pointer to original event timer");

	/* Need to reset state in case implementation can't do it */
	evtim2->state = RTE_EVENT_TIMER_NOT_ARMED;

	/* Rearm it */
	ret = rte_event_timer_arm_burst(timdev, &evtim2, 1);
	TEST_ASSERT_EQUAL(ret, 1, "Failed to arm event timer: %s\n",
			  rte_strerror(rte_errno));

	/* Add 100ms to account for the adapter tick window */
	rte_delay_ms(100 + 100);

	n = rte_event_dequeue_burst(evdev, TEST_PORT_ID, evs, RTE_DIM(evs), 0);
	TEST_ASSERT_EQUAL(n, 1, "Failed to dequeue expected number of expiry "
			  "events from event device");

	/* Free it */
	evtim2 = evs[0].event_ptr;
	TEST_ASSERT_EQUAL(evtim, evtim2,
			  "Failed to recover pointer to original event timer");
	rte_mempool_put(eventdev_test_mempool, evtim2);

	return TEST_SUCCESS;
}

/* Check that the adapter handles the max specified number of timers as
 * expected.
 */
static int
event_timer_arm_max(void)
{
	int ret, i, n;
	int num_evtims = MAX_TIMERS;
	struct rte_event_timer *evtims[num_evtims];
	struct rte_event evs[BATCH_SIZE];
	const struct rte_event_timer init_tim = {
		.ev.op = RTE_EVENT_OP_NEW,
		.ev.queue_id = TEST_QUEUE_ID,
		.ev.sched_type = RTE_SCHED_TYPE_ATOMIC,
		.ev.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
		.ev.event_type =  RTE_EVENT_TYPE_TIMER,
		.state = RTE_EVENT_TIMER_NOT_ARMED,
		.timeout_ticks = CALC_TICKS(5), // expire in .5 sec
	};

	ret = rte_mempool_get_bulk(eventdev_test_mempool, (void **)evtims,
				   num_evtims);
	TEST_ASSERT_EQUAL(ret, 0, "Failed to get array of timer objs: ret = %d",
			  ret);

	for (i = 0; i < num_evtims; i++) {
		*evtims[i] = init_tim;
		evtims[i]->ev.event_ptr = evtims[i];
	}

	/* Test with the max value for the adapter */
	ret = rte_event_timer_arm_burst(timdev, evtims, num_evtims);
	TEST_ASSERT_EQUAL(ret, num_evtims,
			  "Failed to arm all event timers: attempted = %d, "
			  "succeeded = %d, rte_errno = %s",
			  num_evtims, ret, rte_strerror(rte_errno));

	rte_delay_ms(1000);

#define MAX_TRIES num_evtims
	int sum = 0;
	int tries = 0;
	bool done = false;
	while (!done) {
		sum += rte_event_dequeue_burst(evdev, TEST_PORT_ID, evs,
					       RTE_DIM(evs), 10);
		if (sum >= num_evtims || ++tries >= MAX_TRIES)
			done = true;

		rte_delay_ms(10);
	}

	TEST_ASSERT_EQUAL(sum, num_evtims, "Expected %d timer expiry events, "
			  "got %d", num_evtims, sum);

	TEST_ASSERT(tries < MAX_TRIES, "Exceeded max tries");

	rte_delay_ms(100);

	/* Make sure the eventdev is still empty */
	n = rte_event_dequeue_burst(evdev, TEST_PORT_ID, evs, RTE_DIM(evs),
				    10);

	TEST_ASSERT_EQUAL(n, 0, "Dequeued unexpected number of timer expiry "
			  "events from event device");

	rte_mempool_put_bulk(eventdev_test_mempool, (void **)evtims,
			     num_evtims);

	return TEST_SUCCESS;
}

/* Check that creating an event timer with incorrect event sched type fails. */
static int
event_timer_arm_invalid_sched_type(void)
{
	int ret;
	struct rte_event_timer *evtim = NULL;
	const struct rte_event_timer init_tim = {
		.ev.op = RTE_EVENT_OP_NEW,
		.ev.queue_id = TEST_QUEUE_ID,
		.ev.sched_type = RTE_SCHED_TYPE_ATOMIC,
		.ev.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
		.ev.event_type =  RTE_EVENT_TYPE_TIMER,
		.state = RTE_EVENT_TIMER_NOT_ARMED,
		.timeout_ticks = CALC_TICKS(5), // expire in .5 sec
	};

	if (!using_services)
		return -ENOTSUP;

	rte_mempool_get(eventdev_test_mempool, (void **)&evtim);
	if (evtim == NULL) {
		/* Failed to get an event timer object */
		return TEST_FAILED;
	}

	*evtim = init_tim;
	evtim->ev.event_ptr = evtim;
	evtim->ev.sched_type = RTE_SCHED_TYPE_PARALLEL; // bad sched type

	ret = rte_event_timer_arm_burst(timdev, &evtim, 1);
	TEST_ASSERT_EQUAL(ret, 0, "Expected to fail timer arm with invalid "
			  "sched type, but didn't");
	TEST_ASSERT_EQUAL(rte_errno, EINVAL, "Unexpected rte_errno value after"
			  " arm fail with invalid queue");

	rte_mempool_put(eventdev_test_mempool, &evtim);

	return TEST_SUCCESS;
}

/* Check that creating an event timer with a timeout value that is too small or
 * too big fails.
 */
static int
event_timer_arm_invalid_timeout(void)
{
	int ret;
	struct rte_event_timer *evtim = NULL;
	const struct rte_event_timer init_tim = {
		.ev.op = RTE_EVENT_OP_NEW,
		.ev.queue_id = TEST_QUEUE_ID,
		.ev.sched_type = RTE_SCHED_TYPE_ATOMIC,
		.ev.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
		.ev.event_type =  RTE_EVENT_TYPE_TIMER,
		.state = RTE_EVENT_TIMER_NOT_ARMED,
		.timeout_ticks = CALC_TICKS(5), // expire in .5 sec
	};

	rte_mempool_get(eventdev_test_mempool, (void **)&evtim);
	if (evtim == NULL) {
		/* Failed to get an event timer object */
		return TEST_FAILED;
	}

	*evtim = init_tim;
	evtim->ev.event_ptr = evtim;
	evtim->timeout_ticks = 0;  // timeout too small

	ret = rte_event_timer_arm_burst(timdev, &evtim, 1);
	TEST_ASSERT_EQUAL(ret, 0, "Expected to fail timer arm with invalid "
			  "timeout, but didn't");
	TEST_ASSERT_EQUAL(rte_errno, EINVAL, "Unexpected rte_errno value after"
			  " arm fail with invalid timeout");
	TEST_ASSERT_EQUAL(evtim->state, RTE_EVENT_TIMER_ERROR_TOOEARLY,
			  "Unexpected event timer state");

	*evtim = init_tim;
	evtim->ev.event_ptr = evtim;
	evtim->timeout_ticks = CALC_TICKS(1801);  // timeout too big

	ret = rte_event_timer_arm_burst(timdev, &evtim, 1);
	TEST_ASSERT_EQUAL(ret, 0, "Expected to fail timer arm with invalid "
			  "timeout, but didn't");
	TEST_ASSERT_EQUAL(rte_errno, EINVAL, "Unexpected rte_errno value after"
			  " arm fail with invalid timeout");
	TEST_ASSERT_EQUAL(evtim->state, RTE_EVENT_TIMER_ERROR_TOOLATE,
			  "Unexpected event timer state");

	rte_mempool_put(eventdev_test_mempool, evtim);

	return TEST_SUCCESS;
}

static int
event_timer_cancel(void)
{
	uint16_t n;
	int ret;
	struct rte_event_timer_adapter *adapter = timdev;
	struct rte_event_timer *evtim = NULL;
	struct rte_event evs[BATCH_SIZE];
	const struct rte_event_timer init_tim = {
		.ev.op = RTE_EVENT_OP_NEW,
		.ev.queue_id = TEST_QUEUE_ID,
		.ev.sched_type = RTE_SCHED_TYPE_ATOMIC,
		.ev.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
		.ev.event_type =  RTE_EVENT_TYPE_TIMER,
		.state = RTE_EVENT_TIMER_NOT_ARMED,
	};

	rte_mempool_get(eventdev_test_mempool, (void **)&evtim);
	if (evtim == NULL) {
		/* Failed to get an event timer object */
		return TEST_FAILED;
	}

	/* Check that cancelling an uninited timer fails */
	ret = rte_event_timer_cancel_burst(adapter, &evtim, 1);
	TEST_ASSERT_EQUAL(ret, 0, "Succeeded unexpectedly in canceling "
			  "uninited timer");
	TEST_ASSERT_EQUAL(rte_errno, EINVAL, "Unexpected rte_errno value after "
			  "cancelling uninited timer");

	/* Set up a timer */
	*evtim = init_tim;
	evtim->ev.event_ptr = evtim;
	evtim->timeout_ticks = CALC_TICKS(30);  // expire in 3 sec

	/* Check that cancelling an inited but unarmed timer fails */
	ret = rte_event_timer_cancel_burst(adapter, &evtim, 1);
	TEST_ASSERT_EQUAL(ret, 0, "Succeeded unexpectedly in canceling "
			  "unarmed timer");
	TEST_ASSERT_EQUAL(rte_errno, EINVAL, "Unexpected rte_errno value after "
			  "cancelling unarmed timer");

	ret = rte_event_timer_arm_burst(adapter, &evtim, 1);
	TEST_ASSERT_EQUAL(ret, 1, "Failed to arm event timer: %s\n",
			  rte_strerror(rte_errno));
	TEST_ASSERT_EQUAL(evtim->state, RTE_EVENT_TIMER_ARMED,
			  "evtim in incorrect state");

	/* Delay 1 sec */
	rte_delay_ms(1000);

	ret = rte_event_timer_cancel_burst(adapter, &evtim, 1);
	TEST_ASSERT_EQUAL(ret, 1, "Failed to cancel event_timer: %s\n",
			  rte_strerror(rte_errno));
	TEST_ASSERT_EQUAL(evtim->state, RTE_EVENT_TIMER_CANCELED,
			  "evtim in incorrect state");

	rte_delay_ms(3000);

	/* Make sure that no expiry event was generated */
	n = rte_event_dequeue_burst(evdev, TEST_PORT_ID, evs, RTE_DIM(evs), 0);
	TEST_ASSERT_EQUAL(n, 0, "Dequeued unexpected timer expiry event\n");

	rte_mempool_put(eventdev_test_mempool, evtim);

	return TEST_SUCCESS;
}

static int
event_timer_cancel_double(void)
{
	uint16_t n;
	int ret;
	struct rte_event_timer_adapter *adapter = timdev;
	struct rte_event_timer *evtim = NULL;
	struct rte_event evs[BATCH_SIZE];
	const struct rte_event_timer init_tim = {
		.ev.op = RTE_EVENT_OP_NEW,
		.ev.queue_id = TEST_QUEUE_ID,
		.ev.sched_type = RTE_SCHED_TYPE_ATOMIC,
		.ev.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
		.ev.event_type =  RTE_EVENT_TYPE_TIMER,
		.state = RTE_EVENT_TIMER_NOT_ARMED,
		.timeout_ticks = CALC_TICKS(5), // expire in .5 sec
	};

	rte_mempool_get(eventdev_test_mempool, (void **)&evtim);
	if (evtim == NULL) {
		/* Failed to get an event timer object */
		return TEST_FAILED;
	}

	/* Set up a timer */
	*evtim = init_tim;
	evtim->ev.event_ptr = evtim;
	evtim->timeout_ticks = CALC_TICKS(30);  // expire in 3 sec

	ret = rte_event_timer_arm_burst(adapter, &evtim, 1);
	TEST_ASSERT_EQUAL(ret, 1, "Failed to arm event timer: %s\n",
			  rte_strerror(rte_errno));
	TEST_ASSERT_EQUAL(evtim->state, RTE_EVENT_TIMER_ARMED,
			  "timer in unexpected state");

	/* Now, test that referencing the same timer twice in the same call
	 * fails
	 */
	struct rte_event_timer *evtim_arr[] = {evtim, evtim};
	ret = rte_event_timer_cancel_burst(adapter, evtim_arr,
					   RTE_DIM(evtim_arr));

	/* Two requests to cancel same timer, only one should succeed */
	TEST_ASSERT_EQUAL(ret, 1, "Succeeded unexpectedly in canceling timer "
			  "twice");

	TEST_ASSERT_EQUAL(rte_errno, EALREADY, "Unexpected rte_errno value "
			  "after double-cancel: rte_errno = %d", rte_errno);

	rte_delay_ms(3000);

	/* Still make sure that no expiry event was generated */
	n = rte_event_dequeue_burst(evdev, TEST_PORT_ID, evs, RTE_DIM(evs), 0);
	TEST_ASSERT_EQUAL(n, 0, "Dequeued unexpected timer expiry event\n");

	rte_mempool_put(eventdev_test_mempool, evtim);

	return TEST_SUCCESS;
}

/* Check that event timer adapter tick resolution works as expected by testing
 * the number of adapter ticks that occur within a particular time interval.
 */
static int
adapter_tick_resolution(void)
{
	struct rte_event_timer_adapter_stats stats;
	uint64_t adapter_tick_count;

	/* Only run this test in the software driver case */
	if (!using_services)
		return -ENOTSUP;

	TEST_ASSERT_SUCCESS(rte_event_timer_adapter_stats_reset(timdev),
				"Failed to reset stats");

	TEST_ASSERT_SUCCESS(rte_event_timer_adapter_stats_get(timdev,
			&stats), "Failed to get adapter stats");
	TEST_ASSERT_EQUAL(stats.adapter_tick_count, 0, "Adapter tick count "
			"not zeroed out");

	/* Delay 1 second; should let at least 10 ticks occur with the default
	 * adapter configuration used by this test.
	 */
	rte_delay_ms(1000);

	TEST_ASSERT_SUCCESS(rte_event_timer_adapter_stats_get(timdev,
			&stats), "Failed to get adapter stats");

	adapter_tick_count = stats.adapter_tick_count;
	TEST_ASSERT(adapter_tick_count >= 10 && adapter_tick_count <= 12,
			"Expected 10-12 adapter ticks, got %"PRIu64"\n",
			adapter_tick_count);

	return TEST_SUCCESS;
}

static int
adapter_create_max(void)
{
	int i;
	uint32_t svc_start_count, svc_end_count;
	struct rte_event_timer_adapter *adapters[
					RTE_EVENT_TIMER_ADAPTER_NUM_MAX + 1];

	struct rte_event_timer_adapter_conf conf = {
		.event_dev_id = evdev,
		// timer_adapter_id set in loop
		.clk_src = RTE_EVENT_TIMER_ADAPTER_CPU_CLK,
		.timer_tick_ns = NSECPERSEC / 10,
		.max_tmo_ns = 180 * NSECPERSEC,
		.nb_timers = MAX_TIMERS,
		.flags = RTE_EVENT_TIMER_ADAPTER_F_ADJUST_RES,
	};

	if (!using_services)
		return -ENOTSUP;

	svc_start_count = rte_service_get_count();

	/* This test expects that there are sufficient service IDs available
	 * to be allocated. I.e., RTE_EVENT_TIMER_ADAPTER_NUM_MAX may need to
	 * be less than RTE_SERVICE_NUM_MAX if anything else uses a service
	 * (the SW event device, for example).
	 */
	for (i = 0; i < RTE_EVENT_TIMER_ADAPTER_NUM_MAX; i++) {
		conf.timer_adapter_id = i;
		adapters[i] = rte_event_timer_adapter_create_ext(&conf,
				test_port_conf_cb, NULL);
		TEST_ASSERT_NOT_NULL(adapters[i], "Failed to create adapter "
				"%d", i);
	}

	conf.timer_adapter_id = i;
	adapters[i] = rte_event_timer_adapter_create(&conf);
	TEST_ASSERT_NULL(adapters[i], "Created too many adapters");

	/* Check that at least RTE_EVENT_TIMER_ADAPTER_NUM_MAX services
	 * have been created
	 */
	svc_end_count = rte_service_get_count();
	TEST_ASSERT_EQUAL(svc_end_count - svc_start_count,
			RTE_EVENT_TIMER_ADAPTER_NUM_MAX,
			"Failed to create expected number of services");

	for (i = 0; i < RTE_EVENT_TIMER_ADAPTER_NUM_MAX; i++)
		TEST_ASSERT_SUCCESS(rte_event_timer_adapter_free(adapters[i]),
				"Failed to free adapter %d", i);

	/* Check that service count is back to where it was at start */
	svc_end_count = rte_service_get_count();
	TEST_ASSERT_EQUAL(svc_start_count, svc_end_count, "Failed to release "
			  "correct number of services");

	return TEST_SUCCESS;
}

static struct unit_test_suite event_timer_adptr_functional_testsuite  = {
	.suite_name = "event timer functional test suite",
	.setup = testsuite_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {
		TEST_CASE_ST(timdev_setup_usec, timdev_teardown,
				test_timer_state),
		TEST_CASE_ST(timdev_setup_usec, timdev_teardown,
				test_timer_arm),
		TEST_CASE_ST(timdev_setup_usec, timdev_teardown,
				test_timer_arm_burst),
		TEST_CASE_ST(timdev_setup_sec, timdev_teardown,
				test_timer_cancel),
		TEST_CASE_ST(timdev_setup_sec, timdev_teardown,
				test_timer_cancel_random),
		TEST_CASE_ST(timdev_setup_usec_multicore, timdev_teardown,
				test_timer_arm_multicore),
		TEST_CASE_ST(timdev_setup_usec_multicore, timdev_teardown,
				test_timer_arm_burst_multicore),
		TEST_CASE_ST(timdev_setup_sec_multicore, timdev_teardown,
				test_timer_cancel_multicore),
		TEST_CASE_ST(timdev_setup_sec_multicore, timdev_teardown,
				test_timer_cancel_burst_multicore),
		TEST_CASE(adapter_create),
		TEST_CASE_ST(timdev_setup_msec, NULL, adapter_free),
		TEST_CASE_ST(timdev_setup_msec, timdev_teardown,
				adapter_get_info),
		TEST_CASE_ST(timdev_setup_msec, timdev_teardown,
				adapter_lookup),
		TEST_CASE_ST(NULL, timdev_teardown,
				adapter_start),
		TEST_CASE_ST(timdev_setup_msec, NULL,
				adapter_stop),
		TEST_CASE_ST(timdev_setup_msec, timdev_teardown,
				stat_inc_reset_ev_enq),
		TEST_CASE_ST(timdev_setup_msec, timdev_teardown,
			     event_timer_arm),
		TEST_CASE_ST(timdev_setup_msec, timdev_teardown,
			     event_timer_arm_double),
		TEST_CASE_ST(timdev_setup_msec, timdev_teardown,
			     event_timer_arm_expiry),
		TEST_CASE_ST(timdev_setup_msec, timdev_teardown,
				event_timer_arm_rearm),
		TEST_CASE_ST(timdev_setup_msec, timdev_teardown,
				event_timer_arm_max),
		TEST_CASE_ST(timdev_setup_msec, timdev_teardown,
				event_timer_arm_invalid_sched_type),
		TEST_CASE_ST(timdev_setup_msec, timdev_teardown,
				event_timer_arm_invalid_timeout),
		TEST_CASE_ST(timdev_setup_msec, timdev_teardown,
				event_timer_cancel),
		TEST_CASE_ST(timdev_setup_msec, timdev_teardown,
				event_timer_cancel_double),
		TEST_CASE_ST(timdev_setup_msec, timdev_teardown,
				adapter_tick_resolution),
		TEST_CASE(adapter_create_max),
		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static int
test_event_timer_adapter_func(void)
{
	return unit_test_suite_runner(&event_timer_adptr_functional_testsuite);
}

REGISTER_TEST_COMMAND(event_timer_adapter_test, test_event_timer_adapter_func);
