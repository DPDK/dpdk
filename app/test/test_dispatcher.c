/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#include <rte_bus_vdev.h>
#include <rte_dispatcher.h>
#include <rte_eventdev.h>
#include <rte_random.h>
#include <rte_service.h>
#include <rte_stdatomic.h>

#include "test.h"

#define NUM_WORKERS 3
#define NUM_PORTS (NUM_WORKERS + 1)
#define WORKER_PORT_ID(worker_idx) (worker_idx)
#define DRIVER_PORT_ID (NUM_PORTS - 1)

#define NUM_SERVICE_CORES NUM_WORKERS
#define MIN_LCORES (NUM_SERVICE_CORES + 1)

/* Eventdev */
#define NUM_QUEUES 8
#define LAST_QUEUE_ID (NUM_QUEUES - 1)
#define MAX_EVENTS 4096
#define NEW_EVENT_THRESHOLD (MAX_EVENTS / 2)
#define DEQUEUE_BURST_SIZE 32
#define ENQUEUE_BURST_SIZE 32

#define NUM_EVENTS 10000000
#define NUM_FLOWS 16

#define DSW_VDEV "event_dsw0"

struct app_queue {
	uint8_t queue_id;
	uint64_t sn[NUM_FLOWS];
	int dispatcher_reg_id;
};

struct cb_count {
	uint8_t expected_event_dev_id;
	uint8_t expected_event_port_id[RTE_MAX_LCORE];
	RTE_ATOMIC(int) count;
};

struct test_app {
	uint8_t event_dev_id;
	struct rte_dispatcher *dispatcher;
	uint32_t dispatcher_service_id;

	unsigned int service_lcores[NUM_SERVICE_CORES];

	int never_match_reg_id;
	uint64_t never_match_count;
	struct cb_count never_process_count;

	struct app_queue queues[NUM_QUEUES];

	int finalize_reg_id;
	struct cb_count finalize_count;

	bool running;

	RTE_ATOMIC(int) completed_events;
	RTE_ATOMIC(int) errors;
};

static struct test_app *
test_app_create(void)
{
	int i;
	struct test_app *app;

	app = calloc(1, sizeof(struct test_app));

	if (app == NULL)
		return NULL;

	for (i = 0; i < NUM_QUEUES; i++)
		app->queues[i].queue_id = i;

	return app;
}

static void
test_app_free(struct test_app *app)
{
	free(app);
}

static int
test_app_create_vdev(struct test_app *app)
{
	int rc;

	rc = rte_vdev_init(DSW_VDEV, NULL);
	if (rc < 0)
		return TEST_SKIPPED;

	rc = rte_event_dev_get_dev_id(DSW_VDEV);

	app->event_dev_id = (uint8_t)rc;

	return TEST_SUCCESS;
}

static int
test_app_destroy_vdev(struct test_app *app)
{
	int rc;

	rc = rte_event_dev_close(app->event_dev_id);
	TEST_ASSERT_SUCCESS(rc, "Error while closing event device");

	rc = rte_vdev_uninit(DSW_VDEV);
	TEST_ASSERT_SUCCESS(rc, "Error while uninitializing virtual device");

	return TEST_SUCCESS;
}

static int
test_app_setup_event_dev(struct test_app *app)
{
	int rc;
	int i;

	rc = test_app_create_vdev(app);
	if (rc != TEST_SUCCESS)
		return rc;

	struct rte_event_dev_config config = {
		.nb_event_queues = NUM_QUEUES,
		.nb_event_ports = NUM_PORTS,
		.nb_events_limit = MAX_EVENTS,
		.nb_event_queue_flows = 64,
		.nb_event_port_dequeue_depth = DEQUEUE_BURST_SIZE,
		.nb_event_port_enqueue_depth = ENQUEUE_BURST_SIZE
	};

	rc = rte_event_dev_configure(app->event_dev_id, &config);

	TEST_ASSERT_SUCCESS(rc, "Unable to configure event device");

	struct rte_event_queue_conf queue_config = {
		.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
		.schedule_type = RTE_SCHED_TYPE_ATOMIC,
		.nb_atomic_flows = 64
	};

	for (i = 0; i < NUM_QUEUES; i++) {
		uint8_t queue_id = i;

		rc = rte_event_queue_setup(app->event_dev_id, queue_id,
					   &queue_config);

		TEST_ASSERT_SUCCESS(rc, "Unable to setup queue %d", queue_id);
	}

	struct rte_event_port_conf port_config = {
		.new_event_threshold = NEW_EVENT_THRESHOLD,
		.dequeue_depth = DEQUEUE_BURST_SIZE,
		.enqueue_depth = ENQUEUE_BURST_SIZE
	};

	for (i = 0; i < NUM_PORTS; i++) {
		uint8_t event_port_id = i;

		rc = rte_event_port_setup(app->event_dev_id, event_port_id,
					  &port_config);
		TEST_ASSERT_SUCCESS(rc, "Failed to create event port %d",
				    event_port_id);

		if (event_port_id == DRIVER_PORT_ID)
			continue;

		rc = rte_event_port_link(app->event_dev_id, event_port_id,
					 NULL, NULL, 0);

		TEST_ASSERT_EQUAL(rc, NUM_QUEUES, "Failed to link port %d",
				  event_port_id);
	}

	return TEST_SUCCESS;
}

static int
test_app_teardown_event_dev(struct test_app *app)
{
	return test_app_destroy_vdev(app);
}

static int
test_app_start_event_dev(struct test_app *app)
{
	int rc;

	rc = rte_event_dev_start(app->event_dev_id);
	TEST_ASSERT_SUCCESS(rc, "Unable to start event device");

	return TEST_SUCCESS;
}

static void
test_app_stop_event_dev(struct test_app *app)
{
	rte_event_dev_stop(app->event_dev_id);
}

static int
test_app_create_dispatcher(struct test_app *app)
{
	int rc;

	app->dispatcher = rte_dispatcher_create(app->event_dev_id);

	TEST_ASSERT(app->dispatcher != NULL, "Unable to create event "
		    "dispatcher");

	app->dispatcher_service_id =
		rte_dispatcher_service_id_get(app->dispatcher);

	rc = rte_service_set_stats_enable(app->dispatcher_service_id, 1);

	TEST_ASSERT_SUCCESS(rc, "Unable to enable event dispatcher service "
			    "stats");

	rc = rte_service_runstate_set(app->dispatcher_service_id, 1);

	TEST_ASSERT_SUCCESS(rc, "Unable to set dispatcher service runstate");

	return TEST_SUCCESS;
}

static int
test_app_free_dispatcher(struct test_app *app)
{
	int rc;

	rc = rte_service_runstate_set(app->dispatcher_service_id, 0);
	TEST_ASSERT_SUCCESS(rc, "Error disabling dispatcher service");

	rc = rte_dispatcher_free(app->dispatcher);
	TEST_ASSERT_SUCCESS(rc, "Error freeing dispatcher");

	return TEST_SUCCESS;
}

static int
test_app_bind_ports(struct test_app *app)
{
	int i;

	app->never_process_count.expected_event_dev_id =
		app->event_dev_id;
	app->finalize_count.expected_event_dev_id =
		app->event_dev_id;

	for (i = 0; i < NUM_WORKERS; i++) {
		unsigned int lcore_id = app->service_lcores[i];
		uint8_t port_id = WORKER_PORT_ID(i);

		int rc = rte_dispatcher_bind_port_to_lcore(
			app->dispatcher, port_id, DEQUEUE_BURST_SIZE, 0,
			lcore_id
		);

		TEST_ASSERT_SUCCESS(rc, "Unable to bind event device port %d "
				    "to lcore %d", port_id, lcore_id);

		app->never_process_count.expected_event_port_id[lcore_id] =
			port_id;
		app->finalize_count.expected_event_port_id[lcore_id] = port_id;
	}


	return TEST_SUCCESS;
}

static int
test_app_unbind_ports(struct test_app *app)
{
	int i;

	for (i = 0; i < NUM_WORKERS; i++) {
		unsigned int lcore_id = app->service_lcores[i];

		int rc = rte_dispatcher_unbind_port_from_lcore(
			app->dispatcher,
			WORKER_PORT_ID(i),
			lcore_id
		);

		TEST_ASSERT_SUCCESS(rc, "Unable to unbind event device port %d "
				    "from lcore %d", WORKER_PORT_ID(i),
				    lcore_id);
	}

	return TEST_SUCCESS;
}

static bool
match_queue(const struct rte_event *event, void *cb_data)
{
	uintptr_t queue_id = (uintptr_t)cb_data;

	return event->queue_id == queue_id;
}

static int
test_app_get_worker_index(struct test_app *app, unsigned int lcore_id)
{
	int i;

	for (i = 0; i < NUM_SERVICE_CORES; i++)
		if (app->service_lcores[i] == lcore_id)
			return i;

	return -1;
}

static int
test_app_get_worker_port(struct test_app *app, unsigned int lcore_id)
{
	int worker;

	worker = test_app_get_worker_index(app, lcore_id);

	if (worker < 0)
		return -1;

	return WORKER_PORT_ID(worker);
}

static void
test_app_queue_note_error(struct test_app *app)
{
	rte_atomic_fetch_add_explicit(&app->errors, 1, rte_memory_order_relaxed);
}

static void
test_app_process_queue(uint8_t p_event_dev_id, uint8_t p_event_port_id,
	struct rte_event *in_events, uint16_t num,
	void *cb_data)
{
	struct app_queue *app_queue = cb_data;
	struct test_app *app = container_of(app_queue, struct test_app,
					    queues[app_queue->queue_id]);
	unsigned int lcore_id = rte_lcore_id();
	bool intermediate_queue = app_queue->queue_id != LAST_QUEUE_ID;
	int event_port_id;
	uint16_t i;
	struct rte_event out_events[num];

	event_port_id = test_app_get_worker_port(app, lcore_id);

	if (event_port_id < 0 || p_event_dev_id != app->event_dev_id ||
	    p_event_port_id != event_port_id) {
		test_app_queue_note_error(app);
		return;
	}

	for (i = 0; i < num; i++) {
		const struct rte_event *in_event = &in_events[i];
		struct rte_event *out_event = &out_events[i];
		uint64_t sn = in_event->u64;
		uint64_t expected_sn;

		if (in_event->queue_id != app_queue->queue_id) {
			test_app_queue_note_error(app);
			return;
		}

		expected_sn = app_queue->sn[in_event->flow_id]++;

		if (expected_sn != sn) {
			test_app_queue_note_error(app);
			return;
		}

		if (intermediate_queue)
			*out_event = (struct rte_event) {
				.queue_id = in_event->queue_id + 1,
				.flow_id = in_event->flow_id,
				.sched_type = RTE_SCHED_TYPE_ATOMIC,
				.op = RTE_EVENT_OP_FORWARD,
				.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
				.u64 = sn
			};
	}

	if (intermediate_queue) {
		uint16_t n = 0;

		do {
			n += rte_event_enqueue_forward_burst(p_event_dev_id,
							     p_event_port_id,
							     out_events + n,
							     num - n);
		} while (n != num);
	} else
		rte_atomic_fetch_add_explicit(&app->completed_events, num,
					      rte_memory_order_relaxed);
}

static bool
never_match(const struct rte_event *event __rte_unused, void *cb_data)
{
	uint64_t *count = cb_data;

	(*count)++;

	return false;
}

static void
test_app_never_process(uint8_t event_dev_id, uint8_t event_port_id,
	struct rte_event *in_events __rte_unused, uint16_t num, void *cb_data)
{
	struct cb_count *count = cb_data;
	unsigned int lcore_id = rte_lcore_id();

	if (event_dev_id == count->expected_event_dev_id &&
	    event_port_id == count->expected_event_port_id[lcore_id])
		rte_atomic_fetch_add_explicit(&count->count, num,
					      rte_memory_order_relaxed);
}

static void
finalize(uint8_t event_dev_id, uint8_t event_port_id, void *cb_data)
{
	struct cb_count *count = cb_data;
	unsigned int lcore_id = rte_lcore_id();

	if (event_dev_id == count->expected_event_dev_id &&
	    event_port_id == count->expected_event_port_id[lcore_id])
		rte_atomic_fetch_add_explicit(&count->count, 1,
					      rte_memory_order_relaxed);
}

static int
test_app_register_callbacks(struct test_app *app)
{
	int i;

	app->never_match_reg_id =
		rte_dispatcher_register(app->dispatcher, never_match,
					&app->never_match_count,
					test_app_never_process,
					&app->never_process_count);

	TEST_ASSERT(app->never_match_reg_id >= 0, "Unable to register "
		    "never-match handler");

	for (i = 0; i < NUM_QUEUES; i++) {
		struct app_queue *app_queue = &app->queues[i];
		uintptr_t queue_id = app_queue->queue_id;
		int reg_id;

		reg_id = rte_dispatcher_register(app->dispatcher,
						 match_queue, (void *)queue_id,
						 test_app_process_queue,
						 app_queue);

		TEST_ASSERT(reg_id >= 0, "Unable to register consumer "
			    "callback for queue %d", i);

		app_queue->dispatcher_reg_id = reg_id;
	}

	app->finalize_reg_id =
		rte_dispatcher_finalize_register(app->dispatcher,
						       finalize,
						       &app->finalize_count);
	TEST_ASSERT_SUCCESS(app->finalize_reg_id, "Error registering "
			    "finalize callback");

	return TEST_SUCCESS;
}

static int
test_app_unregister_callback(struct test_app *app, uint8_t queue_id)
{
	int reg_id = app->queues[queue_id].dispatcher_reg_id;
	int rc;

	if (reg_id < 0) /* unregistered already */
		return 0;

	rc = rte_dispatcher_unregister(app->dispatcher, reg_id);

	TEST_ASSERT_SUCCESS(rc, "Unable to unregister consumer "
			    "callback for queue %d", queue_id);

	app->queues[queue_id].dispatcher_reg_id = -1;

	return TEST_SUCCESS;
}

static int
test_app_unregister_callbacks(struct test_app *app)
{
	int i;
	int rc;

	if (app->never_match_reg_id >= 0) {
		rc = rte_dispatcher_unregister(app->dispatcher,
						     app->never_match_reg_id);

		TEST_ASSERT_SUCCESS(rc, "Unable to unregister never-match "
				    "handler");
		app->never_match_reg_id = -1;
	}

	for (i = 0; i < NUM_QUEUES; i++) {
		rc = test_app_unregister_callback(app, i);
		if (rc != TEST_SUCCESS)
			return rc;
	}

	if (app->finalize_reg_id >= 0) {
		rc = rte_dispatcher_finalize_unregister(
			app->dispatcher, app->finalize_reg_id
		);
		app->finalize_reg_id = -1;
	}

	return TEST_SUCCESS;
}

static void
test_app_start_dispatcher(struct test_app *app)
{
	rte_dispatcher_start(app->dispatcher);
}

static void
test_app_stop_dispatcher(struct test_app *app)
{
	rte_dispatcher_stop(app->dispatcher);
}

static int
test_app_reset_dispatcher_stats(struct test_app *app)
{
	struct rte_dispatcher_stats stats;

	rte_dispatcher_stats_reset(app->dispatcher);

	memset(&stats, 0xff, sizeof(stats));

	rte_dispatcher_stats_get(app->dispatcher, &stats);

	TEST_ASSERT_EQUAL(stats.poll_count, 0, "Poll count not zero");
	TEST_ASSERT_EQUAL(stats.ev_batch_count, 0, "Batch count not zero");
	TEST_ASSERT_EQUAL(stats.ev_dispatch_count, 0, "Dispatch count "
			  "not zero");
	TEST_ASSERT_EQUAL(stats.ev_drop_count, 0, "Drop count not zero");

	return TEST_SUCCESS;
}

static int
test_app_setup_service_core(struct test_app *app, unsigned int lcore_id)
{
	int rc;

	rc = rte_service_lcore_add(lcore_id);
	TEST_ASSERT_SUCCESS(rc, "Unable to make lcore %d an event dispatcher "
			    "service core", lcore_id);

	rc = rte_service_map_lcore_set(app->dispatcher_service_id, lcore_id, 1);
	TEST_ASSERT_SUCCESS(rc, "Unable to map event dispatcher service");

	return TEST_SUCCESS;
}

static int
test_app_setup_service_cores(struct test_app *app)
{
	int i;
	int lcore_id = -1;

	for (i = 0; i < NUM_SERVICE_CORES; i++) {
		lcore_id = rte_get_next_lcore(lcore_id, 1, 0);

		app->service_lcores[i] = lcore_id;
	}

	for (i = 0; i < NUM_SERVICE_CORES; i++) {
		int rc;

		rc = test_app_setup_service_core(app, app->service_lcores[i]);
		if (rc != TEST_SUCCESS)
			return rc;
	}

	return TEST_SUCCESS;
}

static int
test_app_teardown_service_core(struct test_app *app, unsigned int lcore_id)
{
	int rc;

	rc = rte_service_map_lcore_set(app->dispatcher_service_id, lcore_id, 0);
	TEST_ASSERT_SUCCESS(rc, "Unable to unmap event dispatcher service");

	rc = rte_service_lcore_del(lcore_id);
	TEST_ASSERT_SUCCESS(rc, "Unable change role of service lcore %d",
			    lcore_id);

	return TEST_SUCCESS;
}

static int
test_app_teardown_service_cores(struct test_app *app)
{
	int i;

	for (i = 0; i < NUM_SERVICE_CORES; i++) {
		unsigned int lcore_id = app->service_lcores[i];
		int rc;

		rc = test_app_teardown_service_core(app, lcore_id);
		if (rc != TEST_SUCCESS)
			return rc;
	}

	return TEST_SUCCESS;
}

static int
test_app_start_service_cores(struct test_app *app)
{
	int i;

	for (i = 0; i < NUM_SERVICE_CORES; i++) {
		unsigned int lcore_id = app->service_lcores[i];
		int rc;

		rc = rte_service_lcore_start(lcore_id);
		TEST_ASSERT_SUCCESS(rc, "Unable to start service lcore %d",
				    lcore_id);
	}

	return TEST_SUCCESS;
}

static int
test_app_stop_service_cores(struct test_app *app)
{
	int i;

	for (i = 0; i < NUM_SERVICE_CORES; i++) {
		unsigned int lcore_id = app->service_lcores[i];
		int rc;

		rc = rte_service_lcore_stop(lcore_id);
		TEST_ASSERT_SUCCESS(rc, "Unable to stop service lcore %d",
				    lcore_id);
	}

	return TEST_SUCCESS;
}

static int
test_app_start(struct test_app *app)
{
	int rc;

	rc = test_app_start_event_dev(app);
	if (rc != TEST_SUCCESS)
		return rc;

	rc = test_app_start_service_cores(app);
	if (rc != TEST_SUCCESS)
		return rc;

	test_app_start_dispatcher(app);

	app->running = true;

	return TEST_SUCCESS;
}

static int
test_app_stop(struct test_app *app)
{
	int rc;

	test_app_stop_dispatcher(app);

	rc = test_app_stop_service_cores(app);
	if (rc != TEST_SUCCESS)
		return rc;

	test_app_stop_event_dev(app);

	app->running = false;

	return TEST_SUCCESS;
}

struct test_app *test_app;

static int
test_setup(void)
{
	int rc;

	if (rte_lcore_count() < MIN_LCORES) {
		printf("Not enough cores for dispatcher_autotest; expecting at "
		       "least %d.\n", MIN_LCORES);
		return TEST_SKIPPED;
	}

	test_app = test_app_create();
	TEST_ASSERT(test_app != NULL, "Unable to allocate memory");

	rc = test_app_setup_event_dev(test_app);
	if (rc != TEST_SUCCESS)
		goto err_free_app;

	rc = test_app_create_dispatcher(test_app);
	if (rc != TEST_SUCCESS)
		goto err_teardown_event_dev;

	rc = test_app_setup_service_cores(test_app);
	if (rc != TEST_SUCCESS)
		goto err_free_dispatcher;

	rc = test_app_register_callbacks(test_app);
	if (rc != TEST_SUCCESS)
		goto err_teardown_service_cores;

	rc = test_app_bind_ports(test_app);
	if (rc != TEST_SUCCESS)
		goto err_unregister_callbacks;

	return TEST_SUCCESS;

err_unregister_callbacks:
	test_app_unregister_callbacks(test_app);
err_teardown_service_cores:
	test_app_teardown_service_cores(test_app);
err_free_dispatcher:
	test_app_free_dispatcher(test_app);
err_teardown_event_dev:
	test_app_teardown_event_dev(test_app);
err_free_app:
	test_app_free(test_app);

	test_app = NULL;

	return rc;
}

static void test_teardown(void)
{
	if (test_app == NULL)
		return;

	if (test_app->running)
		test_app_stop(test_app);

	test_app_teardown_service_cores(test_app);

	test_app_unregister_callbacks(test_app);

	test_app_unbind_ports(test_app);

	test_app_free_dispatcher(test_app);

	test_app_teardown_event_dev(test_app);

	test_app_free(test_app);

	test_app = NULL;
}

static int
test_app_get_completed_events(struct test_app *app)
{
	return rte_atomic_load_explicit(&app->completed_events,
					rte_memory_order_relaxed);
}

static int
test_app_get_errors(struct test_app *app)
{
	return rte_atomic_load_explicit(&app->errors, rte_memory_order_relaxed);
}

static int
test_basic(void)
{
	int rc;
	int i;

	rc = test_app_start(test_app);
	if (rc != TEST_SUCCESS)
		return rc;

	uint64_t sns[NUM_FLOWS] = { 0 };

	for (i = 0; i < NUM_EVENTS;) {
		struct rte_event events[ENQUEUE_BURST_SIZE];
		int left;
		int batch_size;
		int j;
		uint16_t n = 0;

		batch_size = 1 + rte_rand_max(ENQUEUE_BURST_SIZE);
		left = NUM_EVENTS - i;

		batch_size = RTE_MIN(left, batch_size);

		for (j = 0; j < batch_size; j++) {
			struct rte_event *event = &events[j];
			uint64_t sn;
			uint32_t flow_id;

			flow_id = rte_rand_max(NUM_FLOWS);

			sn = sns[flow_id]++;

			*event = (struct rte_event) {
				.queue_id = 0,
				.flow_id = flow_id,
				.sched_type = RTE_SCHED_TYPE_ATOMIC,
				.op = RTE_EVENT_OP_NEW,
				.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
				.u64 = sn
			};
		}

		while (n < batch_size)
			n += rte_event_enqueue_new_burst(test_app->event_dev_id,
							 DRIVER_PORT_ID,
							 events + n,
							 batch_size - n);

		i += batch_size;
	}

	while (test_app_get_completed_events(test_app) != NUM_EVENTS)
		rte_event_maintain(test_app->event_dev_id, DRIVER_PORT_ID, 0);

	rc = test_app_get_errors(test_app);
	TEST_ASSERT(rc == 0, "%d errors occurred", rc);

	rc = test_app_stop(test_app);
	if (rc != TEST_SUCCESS)
		return rc;

	struct rte_dispatcher_stats stats;
	rte_dispatcher_stats_get(test_app->dispatcher, &stats);

	TEST_ASSERT_EQUAL(stats.ev_drop_count, 0, "Drop count is not zero");
	TEST_ASSERT_EQUAL(stats.ev_dispatch_count, NUM_EVENTS * NUM_QUEUES,
			  "Invalid dispatch count");
	TEST_ASSERT(stats.poll_count > 0, "Poll count is zero");

	TEST_ASSERT_EQUAL(test_app->never_process_count.count, 0,
			  "Never-match handler's process function has "
			  "been called");

	int finalize_count =
		rte_atomic_load_explicit(&test_app->finalize_count.count,
					 rte_memory_order_relaxed);

	TEST_ASSERT(finalize_count > 0, "Finalize count is zero");
	TEST_ASSERT(finalize_count <= (int)stats.ev_dispatch_count,
		    "Finalize count larger than event count");

	TEST_ASSERT_EQUAL(finalize_count, (int)stats.ev_batch_count,
			  "%"PRIu64" batches dequeued, but finalize called %d "
			  "times", stats.ev_batch_count, finalize_count);

	/*
	 * The event dispatcher should call often-matching match functions
	 * more often, and thus this never-matching match function should
	 * be called relatively infrequently.
	 */
	TEST_ASSERT(test_app->never_match_count <
		    (stats.ev_dispatch_count / 4),
		    "Never-matching match function called suspiciously often");

	rc = test_app_reset_dispatcher_stats(test_app);
	if (rc != TEST_SUCCESS)
		return rc;

	return TEST_SUCCESS;
}

static int
test_drop(void)
{
	int rc;
	uint8_t unhandled_queue;
	struct rte_dispatcher_stats stats;

	unhandled_queue = (uint8_t)rte_rand_max(NUM_QUEUES);

	rc = test_app_start(test_app);
	if (rc != TEST_SUCCESS)
		return rc;

	rc = test_app_unregister_callback(test_app, unhandled_queue);
	if (rc != TEST_SUCCESS)
		return rc;

	struct rte_event event = {
	    .queue_id = unhandled_queue,
	    .flow_id = 0,
	    .sched_type = RTE_SCHED_TYPE_ATOMIC,
	    .op = RTE_EVENT_OP_NEW,
	    .priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
	    .u64 = 0
	};

	do {
		rc = rte_event_enqueue_burst(test_app->event_dev_id,
					     DRIVER_PORT_ID, &event, 1);
	} while (rc == 0);

	do {
		rte_dispatcher_stats_get(test_app->dispatcher, &stats);

		rte_event_maintain(test_app->event_dev_id, DRIVER_PORT_ID, 0);
	} while (stats.ev_drop_count == 0 && stats.ev_dispatch_count == 0);

	rc = test_app_stop(test_app);
	if (rc != TEST_SUCCESS)
		return rc;

	TEST_ASSERT_EQUAL(stats.ev_drop_count, 1, "Drop count is not one");
	TEST_ASSERT_EQUAL(stats.ev_dispatch_count, 0,
			  "Dispatch count is not zero");
	TEST_ASSERT(stats.poll_count > 0, "Poll count is zero");

	return TEST_SUCCESS;
}

#define MORE_THAN_MAX_HANDLERS 1000
#define MIN_HANDLERS 32

static int
test_many_handler_registrations(void)
{
	int rc;
	int num_regs = 0;
	int reg_ids[MORE_THAN_MAX_HANDLERS];
	int reg_id;
	int i;

	rc = test_app_unregister_callbacks(test_app);
	if (rc != TEST_SUCCESS)
		return rc;

	for (i = 0; i < MORE_THAN_MAX_HANDLERS; i++) {
		reg_id = rte_dispatcher_register(test_app->dispatcher,
						 never_match, NULL,
						 test_app_never_process, NULL);
		if (reg_id < 0)
			break;

		reg_ids[num_regs++] = reg_id;
	}

	TEST_ASSERT_EQUAL(reg_id, -ENOMEM, "Incorrect return code. Expected "
			  "%d but was %d", -ENOMEM, reg_id);
	TEST_ASSERT(num_regs >= MIN_HANDLERS, "Registration failed already "
		    "after %d handler registrations.", num_regs);

	for (i = 0; i < num_regs; i++) {
		rc = rte_dispatcher_unregister(test_app->dispatcher,
					       reg_ids[i]);
		TEST_ASSERT_SUCCESS(rc, "Unable to unregister handler %d",
				    reg_ids[i]);
	}

	return TEST_SUCCESS;
}

static void
dummy_finalize(uint8_t event_dev_id __rte_unused,
	       uint8_t event_port_id __rte_unused,
	       void *cb_data __rte_unused)
{
}

#define MORE_THAN_MAX_FINALIZERS 1000
#define MIN_FINALIZERS 16

static int
test_many_finalize_registrations(void)
{
	int rc;
	int num_regs = 0;
	int reg_ids[MORE_THAN_MAX_FINALIZERS];
	int reg_id;
	int i;

	rc = test_app_unregister_callbacks(test_app);
	if (rc != TEST_SUCCESS)
		return rc;

	for (i = 0; i < MORE_THAN_MAX_FINALIZERS; i++) {
		reg_id = rte_dispatcher_finalize_register(
			test_app->dispatcher, dummy_finalize, NULL
		);

		if (reg_id < 0)
			break;

		reg_ids[num_regs++] = reg_id;
	}

	TEST_ASSERT_EQUAL(reg_id, -ENOMEM, "Incorrect return code. Expected "
			  "%d but was %d", -ENOMEM, reg_id);
	TEST_ASSERT(num_regs >= MIN_FINALIZERS, "Finalize registration failed "
		    "already after %d registrations.", num_regs);

	for (i = 0; i < num_regs; i++) {
		rc = rte_dispatcher_finalize_unregister(
			test_app->dispatcher, reg_ids[i]
		);
		TEST_ASSERT_SUCCESS(rc, "Unable to unregister finalizer %d",
				    reg_ids[i]);
	}

	return TEST_SUCCESS;
}

static struct unit_test_suite test_suite = {
	.suite_name = "Event dispatcher test suite",
	.unit_test_cases = {
		TEST_CASE_ST(test_setup, test_teardown, test_basic),
		TEST_CASE_ST(test_setup, test_teardown, test_drop),
		TEST_CASE_ST(test_setup, test_teardown,
			     test_many_handler_registrations),
		TEST_CASE_ST(test_setup, test_teardown,
			     test_many_finalize_registrations),
		TEST_CASES_END()
	}
};

static int
test_dispatcher(void)
{
	return unit_test_suite_runner(&test_suite);
}

REGISTER_FAST_TEST(dispatcher_autotest, false, true, test_dispatcher);
