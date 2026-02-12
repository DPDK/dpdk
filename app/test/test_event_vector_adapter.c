/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Marvell International Ltd.
 */

#include <string.h>

#include <rte_common.h>
#include <rte_malloc.h>

#include "test.h"

#ifdef RTE_EXEC_ENV_WINDOWS
static int __rte_unused
test_event_vector_adapter(void)
{
	printf("event_vector_adapter not supported on Windows, skipping test\n");
	return TEST_SKIPPED;
}

#else

#include <rte_bus_vdev.h>
#include <rte_event_vector_adapter.h>
#include <rte_eventdev.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_mempool.h>
#include <rte_per_lcore.h>
#include <rte_random.h>
#include <rte_service.h>
#include <stdbool.h>

#define MAX_VECTOR_SIZE 8
#define MAX_EVENTS	512
#define MAX_RETRIES	16

static int sw_slcore = -1;
static int adapter_slcore = -1;
static uint8_t evdev;
static bool using_services;
static uint8_t vector_adptr_id;
static uint32_t vector_service_id = UINT32_MAX;
static uint32_t evdev_service_id = UINT32_MAX;
static uint64_t vector_tmo_us;
static uint8_t evdev_max_queues;
static struct rte_mempool *vector_mp;

static uint64_t objs[MAX_VECTOR_SIZE] = {0xDEADBEAF, 0xDEADBEEF, 0xDEADC0DE, 0xDEADCAFE,
					 0xDEADFACE, 0xDEADFADE, 0xDEADFAAA, 0xDEADFAAB};

static void
wait_service_us(uint32_t service_id, uint64_t us)
{
#define USEC2TICK(__us, __freq) (((__us) * (__freq)) / 1E6)
	uint64_t service_cycles;
	uint64_t cycles = USEC2TICK(us, rte_get_timer_hz());

	if (service_id == UINT32_MAX) {
		rte_delay_us(us);
		return;
	}

	rte_service_attr_reset_all(service_id);
	do {
		service_cycles = 0;
		if (vector_service_id != UINT32_MAX)
			rte_service_attr_get(vector_service_id, RTE_SERVICE_ATTR_CYCLES,
					     &service_cycles);
		rte_delay_us(us);
	} while ((vector_service_id != UINT32_MAX && service_cycles < cycles));
}

static int
test_event_vector_adapter_create_multi(void)
{
	struct rte_event_vector_adapter *adapter[RTE_EVENT_MAX_QUEUES_PER_DEV]
						[RTE_EVENT_VECTOR_ADAPTER_MAX_INSTANCE_PER_QUEUE];
	struct rte_event_vector_adapter_conf conf;
	struct rte_event_vector_adapter_info info;
	int ret, i, j;

	memset(&conf, 0, sizeof(conf));
	memset(&info, 0, sizeof(info));

	ret = rte_event_vector_adapter_info_get(evdev, &info);
	TEST_ASSERT_SUCCESS(ret, "Failed to get event vector adapter info");

	vector_mp = rte_event_vector_pool_create("vector_mp", MAX_EVENTS, 0, MAX_VECTOR_SIZE,
						 rte_socket_id());

	TEST_ASSERT(vector_mp != NULL, "Failed to create mempool");

	conf.event_dev_id = evdev;
	conf.socket_id = rte_socket_id();
	conf.vector_sz = RTE_MIN(MAX_VECTOR_SIZE, info.max_vector_sz);
	conf.vector_timeout_ns = info.max_vector_timeout_ns;
	conf.vector_mp = vector_mp;

	conf.ev.queue_id = 0;
	conf.ev.event_type = RTE_EVENT_TYPE_VECTOR | RTE_EVENT_TYPE_CPU;
	conf.ev.sched_type = RTE_SCHED_TYPE_PARALLEL;

	for (i = 0; i < evdev_max_queues; i++) {
		for (j = 0; j < info.max_vector_adapters_per_event_queue; j++) {
			conf.ev.queue_id = j;
			adapter[i][j] = rte_event_vector_adapter_create(&conf);
			TEST_ASSERT(adapter[i][j] != NULL, "Failed to create event vector adapter");
		}
	}

	for (i = 0; i < evdev_max_queues; i++)
		for (j = 0; j < info.max_vector_adapters_per_event_queue; j++)
			TEST_ASSERT(adapter[i][j] == rte_event_vector_adapter_lookup(
							     adapter[i][j]->adapter_id),
				    "Failed to lookup event vector adapter");

	for (i = 0; i < evdev_max_queues; i++)
		for (j = 0; j < info.max_vector_adapters_per_event_queue; j++)
			rte_event_vector_adapter_destroy(adapter[i][j]);

	rte_mempool_free(vector_mp);
	vector_mp = NULL;

	return TEST_SUCCESS;
}

static int
test_event_vector_adapter_create(void)
{
	struct rte_event_vector_adapter_conf conf;
	struct rte_event_vector_adapter_info info;
	struct rte_event_vector_adapter *adapter;
	uint32_t service_id;
	int ret;

	memset(&conf, 0, sizeof(conf));
	memset(&info, 0, sizeof(info));

	ret = rte_event_vector_adapter_info_get(evdev, &info);
	TEST_ASSERT_SUCCESS(ret, "Failed to get event vector adapter info");

	vector_mp = rte_event_vector_pool_create("vector_mp", MAX_EVENTS, 0, MAX_VECTOR_SIZE,
						 rte_socket_id());
	TEST_ASSERT(vector_mp != NULL, "Failed to create mempool");

	conf.event_dev_id = evdev;
	conf.socket_id = rte_socket_id();
	conf.vector_sz = RTE_MIN(MAX_VECTOR_SIZE, info.max_vector_sz);
	conf.vector_timeout_ns = RTE_MIN(1E6, info.max_vector_timeout_ns);
	conf.vector_mp = vector_mp;

	conf.ev.queue_id = 0;
	conf.ev.event_type = RTE_EVENT_TYPE_VECTOR | RTE_EVENT_TYPE_CPU;
	conf.ev.sched_type = RTE_SCHED_TYPE_PARALLEL;

	conf.ev_fallback.event_type = RTE_EVENT_TYPE_CPU;
	adapter = rte_event_vector_adapter_create(&conf);
	TEST_ASSERT(adapter != NULL, "Failed to create event vector adapter");

	vector_adptr_id = adapter->adapter_id;

	TEST_ASSERT(adapter == rte_event_vector_adapter_lookup(vector_adptr_id),
		    "Failed to lookup event vector adapter");

	if (rte_event_vector_adapter_service_id_get(adapter, &service_id) == 0) {
		if (sw_slcore < 0) {
			adapter_slcore = rte_get_next_lcore(sw_slcore, 1, 0);
			TEST_ASSERT_SUCCESS(rte_service_lcore_add(adapter_slcore),
					    "Failed to add service core");
			TEST_ASSERT_SUCCESS(rte_service_lcore_start(adapter_slcore),
					    "Failed to start service core");
		} else
			adapter_slcore = sw_slcore;
		TEST_ASSERT(rte_service_map_lcore_set(service_id, adapter_slcore, 1) == 0,
			    "Failed to map adapter service");
		TEST_ASSERT(rte_service_runstate_set(service_id, 1) == 0,
			    "Failed to start adapter service");
		rte_service_set_stats_enable(service_id, 1);
		vector_service_id = service_id;
	}

	vector_tmo_us = (conf.vector_timeout_ns / 1E3) * 2;

	return TEST_SUCCESS;
}

static void
test_event_vector_adapter_free(void)
{
	struct rte_event_vector_adapter *adapter;
	uint32_t service_id;

	adapter = rte_event_vector_adapter_lookup(vector_adptr_id);

	if (adapter != NULL) {
		if (rte_event_vector_adapter_service_id_get(adapter, &service_id) == 0) {
			rte_service_runstate_set(service_id, 0);
			rte_service_map_lcore_set(service_id, adapter_slcore, 0);
			if (adapter_slcore != sw_slcore) {
				rte_service_lcore_stop(adapter_slcore);
				rte_service_lcore_del(adapter_slcore);
			}
			adapter_slcore = -1;
			vector_service_id = UINT32_MAX;
		}
		rte_event_vector_adapter_destroy(adapter);
	}
	rte_mempool_free(vector_mp);
	vector_mp = NULL;
}

static int
test_event_vector_adapter_enqueue(void)
{
	struct rte_event_vector_adapter *adapter;
	struct rte_event ev;
	int ret, i;

	adapter = rte_event_vector_adapter_lookup(vector_adptr_id);
	TEST_ASSERT(adapter != NULL, "Failed to lookup event vector adapter");

	ret = rte_event_vector_adapter_enqueue(adapter, objs, MAX_VECTOR_SIZE, 0);
	TEST_ASSERT((ret == MAX_VECTOR_SIZE), "Failed to enqueue event vector %d", ret);

	wait_service_us(vector_service_id, 1E2);
	for (i = 0; i < MAX_RETRIES; i++) {
		ret = rte_event_dequeue_burst(evdev, 0, &ev, 1, 0);
		if (ret)
			break;

		wait_service_us(evdev_service_id, 1E2);
	}

	TEST_ASSERT((ret == 1), "Failed to dequeue event vector %d", ret);
	TEST_ASSERT((ev.event_type == (RTE_EVENT_TYPE_VECTOR | RTE_EVENT_TYPE_CPU)),
		    "Invalid event type %d", ev.event_type);

	TEST_ASSERT((ev.vec->nb_elem == MAX_VECTOR_SIZE), "Incomplete event vector %d",
		    ev.vec->nb_elem);
	TEST_ASSERT((ev.queue_id == 0), "Invalid event type %d", ev.queue_id);
	TEST_ASSERT((ev.sched_type == RTE_SCHED_TYPE_PARALLEL), "Invalid sched type %d",
		    ev.sched_type);

	for (i = 0; i < MAX_VECTOR_SIZE; i++)
		TEST_ASSERT((ev.vec->u64s[i] == objs[i]), "Invalid object in event vector %" PRIx64,
			    ev.vec->u64s[i]);
	rte_mempool_put(rte_mempool_from_obj(ev.vec), ev.vec);
	return TEST_SUCCESS;
}

static int
test_event_vector_adapter_enqueue_tmo(void)
{
	struct rte_event_vector_adapter *adapter;
	uint16_t vec_sz = MAX_VECTOR_SIZE - 4;
	struct rte_event ev;
	int ret, i;

	adapter = rte_event_vector_adapter_lookup(vector_adptr_id);
	TEST_ASSERT(adapter != NULL, "Failed to lookup event vector adapter");

	ret = rte_event_vector_adapter_enqueue(adapter, objs, vec_sz, 0);
	TEST_ASSERT((ret == vec_sz), "Failed to enqueue event vector %d", ret);

	wait_service_us(vector_service_id, vector_tmo_us);

	for (i = 0; i < MAX_RETRIES; i++) {
		ret = rte_event_dequeue_burst(evdev, 0, &ev, 1, 0);
		if (ret)
			break;

		wait_service_us(evdev_service_id, 1E2);
	}

	TEST_ASSERT((ret == 1), "Failed to dequeue event vector %d", ret);
	TEST_ASSERT((ev.event_type == (RTE_EVENT_TYPE_VECTOR | RTE_EVENT_TYPE_CPU)),
		    "Invalid event type %d", ev.event_type);

	TEST_ASSERT((ev.vec->nb_elem == vec_sz), "Incomplete event vector %d", ev.vec->nb_elem);
	TEST_ASSERT((ev.queue_id == 0), "Invalid event type %d", ev.queue_id);
	TEST_ASSERT((ev.sched_type == RTE_SCHED_TYPE_PARALLEL), "Invalid sched type %d",
		    ev.sched_type);

	for (i = 0; i < vec_sz; i++)
		TEST_ASSERT((ev.vec->u64s[i] == objs[i]), "Invalid object in event vector %" PRIx64,
			    ev.vec->u64s[i]);
	rte_mempool_put(rte_mempool_from_obj(ev.vec), ev.vec);
	return TEST_SUCCESS;
}

static int
test_event_vector_adapter_enqueue_fallback(void)
{
	struct rte_event_vector_adapter *adapter;
	uint64_t vec[MAX_EVENTS];
	struct rte_event ev;
	int ret, i;

	adapter = rte_event_vector_adapter_lookup(vector_adptr_id);
	TEST_ASSERT(adapter != NULL, "Failed to lookup event vector adapter");

	ret = rte_mempool_get_bulk(vector_mp, (void **)vec, MAX_EVENTS);
	TEST_ASSERT(ret == 0, "Failed to get mempool objects %d", ret);

	ret = rte_event_vector_adapter_enqueue(adapter, objs, 1, 0);
	TEST_ASSERT((ret == 1), "Failed to enqueue event vector %d", ret);

	wait_service_us(vector_service_id, vector_tmo_us);
	for (i = 0; i < MAX_RETRIES; i++) {
		ret = rte_event_dequeue_burst(evdev, 0, &ev, 1, 0);
		if (ret)
			break;

		wait_service_us(evdev_service_id, 1E2);
	}

	TEST_ASSERT((ret == 1), "Failed to dequeue event vector %d", ret);
	TEST_ASSERT((ev.event_type == RTE_EVENT_TYPE_CPU), "Incorrect fallback event type %d",
		    ev.event_type);
	TEST_ASSERT((ev.sched_type == RTE_SCHED_TYPE_PARALLEL), "Invalid sched type %d",
		    ev.sched_type);

	rte_mempool_put_bulk(vector_mp, (void **)vec, MAX_EVENTS);

	return TEST_SUCCESS;
}

static int
test_event_vector_adapter_enqueue_sov(void)
{
	struct rte_event_vector_adapter *adapter;
	uint16_t vec_sz = MAX_VECTOR_SIZE - 4;
	struct rte_event ev;
	uint32_t caps;
	int ret, i;

	caps = 0;
	ret = rte_event_vector_adapter_caps_get(evdev, &caps);
	TEST_ASSERT_SUCCESS(ret, "Failed to get event vector adapter caps");

	if (!(caps & RTE_EVENT_VECTOR_ADAPTER_CAP_SOV_EOV)) {
		printf("SOV/EOV not supported, skipping test\n");
		return TEST_SKIPPED;
	}

	adapter = rte_event_vector_adapter_lookup(vector_adptr_id);
	TEST_ASSERT(adapter != NULL, "Failed to lookup event vector adapter");

	ret = rte_event_vector_adapter_enqueue(adapter, objs, vec_sz, 0);
	TEST_ASSERT((ret == vec_sz), "Failed to enqueue event vector %d", ret);

	ret = rte_event_vector_adapter_enqueue(adapter, &objs[vec_sz], 2, RTE_EVENT_VECTOR_ENQ_SOV);
	TEST_ASSERT((ret == 2), "Failed to enqueue event vector %d", ret);

	wait_service_us(vector_service_id, vector_tmo_us);
	for (i = 0; i < MAX_RETRIES; i++) {
		ret = rte_event_dequeue_burst(evdev, 0, &ev, 1, 0);
		if (ret)
			break;

		wait_service_us(evdev_service_id, 1E2);
	}

	TEST_ASSERT((ret == 1), "Failed to dequeue event vector %d", ret);
	TEST_ASSERT((ev.event_type == (RTE_EVENT_TYPE_VECTOR | RTE_EVENT_TYPE_CPU)),
		    "Invalid event type %d", ev.event_type);
	TEST_ASSERT((ev.vec->nb_elem == vec_sz), "Incorrect event vector %d", ev.vec->nb_elem);

	for (i = 0; i < vec_sz; i++)
		TEST_ASSERT((ev.vec->u64s[i] == objs[i]), "Invalid object in event vector %" PRIx64,
			    ev.vec->u64s[i]);

	for (i = 0; i < MAX_RETRIES; i++) {
		ret = rte_event_dequeue_burst(evdev, 0, &ev, 1, 0);
		if (ret)
			break;

		wait_service_us(evdev_service_id, 1E2);
	}
	TEST_ASSERT((ret == 1), "Failed to dequeue event vector %d", ret);
	TEST_ASSERT((ev.event_type == (RTE_EVENT_TYPE_VECTOR | RTE_EVENT_TYPE_CPU)),
		    "Invalid event type %d", ev.event_type);
	TEST_ASSERT((ev.vec->nb_elem == 2), "Incorrect event vector %d", ev.vec->nb_elem);

	for (i = 0; i < 2; i++)
		TEST_ASSERT((ev.vec->u64s[i] == objs[vec_sz + i]),
			    "Invalid object in event vector %" PRIx64, ev.vec->u64s[i]);

	return TEST_SUCCESS;
}

static int
test_event_vector_adapter_enqueue_eov(void)
{
	struct rte_event_vector_adapter *adapter;
	uint16_t vec_sz = MAX_VECTOR_SIZE - 4;
	struct rte_event ev;
	uint32_t caps;
	int ret, i;

	caps = 0;
	ret = rte_event_vector_adapter_caps_get(evdev, &caps);
	TEST_ASSERT_SUCCESS(ret, "Failed to get event vector adapter caps");

	if (!(caps & RTE_EVENT_VECTOR_ADAPTER_CAP_SOV_EOV)) {
		printf("SOV/EOV not supported, skipping test\n");
		return TEST_SKIPPED;
	}

	adapter = rte_event_vector_adapter_lookup(vector_adptr_id);
	TEST_ASSERT(adapter != NULL, "Failed to lookup event vector adapter");

	ret = rte_event_vector_adapter_enqueue(adapter, objs, vec_sz, 0);
	TEST_ASSERT((ret == vec_sz), "Failed to enqueue event vector %d", ret);

	ret = rte_event_vector_adapter_enqueue(adapter, &objs[vec_sz], 1, RTE_EVENT_VECTOR_ENQ_EOV);
	TEST_ASSERT((ret == 1), "Failed to enqueue event vector %d", ret);

	for (i = 0; i < MAX_RETRIES; i++) {
		ret = rte_event_dequeue_burst(evdev, 0, &ev, 1, 0);
		if (ret)
			break;

		wait_service_us(evdev_service_id, 1E2);
	}

	TEST_ASSERT((ret == 1), "Failed to dequeue event vector %d", ret);
	TEST_ASSERT((ev.event_type == (RTE_EVENT_TYPE_VECTOR | RTE_EVENT_TYPE_CPU)),
		    "Invalid event type %d", ev.event_type);
	TEST_ASSERT((ev.vec->nb_elem == vec_sz + 1), "Incorrect event vector %d", ev.vec->nb_elem);

	ret = rte_event_vector_adapter_enqueue(adapter, objs, MAX_VECTOR_SIZE - 1, 0);
	TEST_ASSERT((ret == MAX_VECTOR_SIZE - 1), "Failed to enqueue event vector %d", ret);

	ret = rte_event_vector_adapter_enqueue(adapter, &objs[vec_sz], vec_sz,
					       RTE_EVENT_VECTOR_ENQ_EOV);
	TEST_ASSERT((ret == vec_sz), "Failed to enqueue event vector %d", ret);

	for (i = 0; i < MAX_RETRIES; i++) {
		ret = rte_event_dequeue_burst(evdev, 0, &ev, 1, 0);
		if (ret)
			break;

		wait_service_us(evdev_service_id, 1E2);
	}

	TEST_ASSERT((ret == 1), "Failed to dequeue event vector %d", ret);
	TEST_ASSERT((ev.event_type == (RTE_EVENT_TYPE_VECTOR | RTE_EVENT_TYPE_CPU)),
		    "Invalid event type %d", ev.event_type);
	TEST_ASSERT((ev.vec->nb_elem == MAX_VECTOR_SIZE), "Incorrect event vector %d",
		    ev.vec->nb_elem);

	for (i = 0; i < MAX_VECTOR_SIZE - 1; i++)
		TEST_ASSERT((ev.vec->u64s[i] == objs[i]), "Invalid object in event vector %" PRIx64,
			    ev.vec->u64s[i]);

	TEST_ASSERT((ev.vec->u64s[MAX_VECTOR_SIZE - 1] == objs[vec_sz]),
		    "Invalid object in event vector %" PRIx64, ev.vec->u64s[MAX_VECTOR_SIZE - 1]);

	wait_service_us(vector_service_id, vector_tmo_us);
	for (i = 0; i < MAX_RETRIES; i++) {
		ret = rte_event_dequeue_burst(evdev, 0, &ev, 1, 0);
		if (ret)
			break;

		wait_service_us(evdev_service_id, 1E2);
	}
	TEST_ASSERT((ret == 1), "Failed to dequeue event vector %d", ret);
	TEST_ASSERT((ev.event_type == (RTE_EVENT_TYPE_VECTOR | RTE_EVENT_TYPE_CPU)),
		    "Invalid event type %d", ev.event_type);
	TEST_ASSERT((ev.vec->nb_elem == vec_sz - 1), "Incorrect event vector %d", ev.vec->nb_elem);

	for (i = 0; i < vec_sz - 1; i++)
		TEST_ASSERT((ev.vec->u64s[i] == objs[vec_sz + i + 1]),
			    "Invalid object in event vector %" PRIx64, ev.vec->u64s[i]);

	return TEST_SUCCESS;
}

static int
test_event_vector_adapter_enqueue_sov_eov(void)
{
	struct rte_event_vector_adapter_info info;
	struct rte_event_vector_adapter *adapter;
	uint16_t vec_sz = MAX_VECTOR_SIZE - 4;
	struct rte_event ev;
	uint32_t caps;
	int ret, i;

	memset(&info, 0, sizeof(info));
	ret = rte_event_vector_adapter_info_get(evdev, &info);
	TEST_ASSERT_SUCCESS(ret, "Failed to get event vector adapter info");

	caps = 0;
	ret = rte_event_vector_adapter_caps_get(evdev, &caps);
	TEST_ASSERT_SUCCESS(ret, "Failed to get event vector adapter caps");

	if (!(caps & RTE_EVENT_VECTOR_ADAPTER_CAP_SOV_EOV)) {
		printf("SOV/EOV not supported, skipping test\n");
		return TEST_SKIPPED;
	}

	adapter = rte_event_vector_adapter_lookup(vector_adptr_id);
	TEST_ASSERT(adapter != NULL, "Failed to lookup event vector adapter");

	ret = rte_event_vector_adapter_enqueue(adapter, objs, vec_sz,
					       RTE_EVENT_VECTOR_ENQ_SOV | RTE_EVENT_VECTOR_ENQ_EOV);
	TEST_ASSERT((ret == vec_sz), "Failed to enqueue event vector %d", ret);

	for (i = 0; i < MAX_RETRIES; i++) {
		ret = rte_event_dequeue_burst(evdev, 0, &ev, 1, 0);
		if (ret)
			break;

		wait_service_us(evdev_service_id, 1E2);
	}

	TEST_ASSERT((ret == 1), "Failed to dequeue event vector %d", ret);
	TEST_ASSERT((ev.event_type == (RTE_EVENT_TYPE_CPU | RTE_EVENT_TYPE_VECTOR)),
		    "Incorrect event type %d", ev.event_type);
	TEST_ASSERT((ev.vec->nb_elem == vec_sz), "Incorrect event vector %d", ev.vec->nb_elem);

	for (i = 0; i < vec_sz; i++)
		TEST_ASSERT((ev.vec->u64s[i] == objs[i]), "Invalid object in event vector %" PRIx64,
			    ev.vec->u64s[i]);

	ret = rte_event_vector_adapter_enqueue(adapter, objs, 1,
					       RTE_EVENT_VECTOR_ENQ_SOV | RTE_EVENT_VECTOR_ENQ_EOV);
	TEST_ASSERT((ret == 1), "Failed to enqueue event vector %d", ret);

	for (i = 0; i < MAX_RETRIES; i++) {
		ret = rte_event_dequeue_burst(evdev, 0, &ev, 1, 0);
		if (ret)
			break;

		wait_service_us(evdev_service_id, 1E2);
	}
	TEST_ASSERT((ret == 1), "Failed to dequeue event vector %d", ret);
	if (info.min_vector_sz > 1)
		TEST_ASSERT((ev.event_type == RTE_EVENT_TYPE_CPU), "Incorrect event type %d",
			    ev.event_type);
	else
		TEST_ASSERT((ev.event_type == (RTE_EVENT_TYPE_CPU | RTE_EVENT_TYPE_VECTOR)),
			    "Incorrect event type %d", ev.event_type);

	return TEST_SUCCESS;
}

static int
test_event_vector_adapter_enqueue_flush(void)
{
	struct rte_event_vector_adapter *adapter;
	struct rte_event ev;
	int ret, i;

	adapter = rte_event_vector_adapter_lookup(vector_adptr_id);
	TEST_ASSERT(adapter != NULL, "Failed to lookup event vector adapter");

	ret = rte_event_vector_adapter_enqueue(adapter, objs, MAX_VECTOR_SIZE - 1, 0);
	TEST_ASSERT((ret == MAX_VECTOR_SIZE - 1), "Failed to enqueue event vector %d", ret);

	ret = rte_event_vector_adapter_enqueue(adapter, NULL, 0, RTE_EVENT_VECTOR_ENQ_FLUSH);
	TEST_ASSERT((ret == 0), "Failed to enqueue event vector %d", ret);

	for (i = 0; i < MAX_RETRIES; i++) {
		ret = rte_event_dequeue_burst(evdev, 0, &ev, 1, 0);
		if (ret)
			break;

		wait_service_us(evdev_service_id, 1E2);
	}

	TEST_ASSERT((ret == 1), "Failed to dequeue event vector %d", ret);
	TEST_ASSERT((ev.event_type == (RTE_EVENT_TYPE_CPU | RTE_EVENT_TYPE_VECTOR)), "Incorrect event type %d",
		    ev.event_type);
	TEST_ASSERT((ev.sched_type == RTE_SCHED_TYPE_PARALLEL), "Invalid sched type %d",
		    ev.sched_type);

	return TEST_SUCCESS;
}

static inline int
eventdev_setup(void)
{
	struct rte_event_queue_conf queue_conf;
	struct rte_event_dev_config conf;
	struct rte_event_dev_info info;
	uint32_t service_id;
	int ret, i;

	ret = rte_event_dev_info_get(evdev, &info);
	TEST_ASSERT_SUCCESS(ret, "Failed to get event dev info");

	memset(&conf, 0, sizeof(conf));
	conf.nb_event_port_dequeue_depth = info.max_event_port_dequeue_depth;
	conf.nb_event_port_enqueue_depth = info.max_event_port_enqueue_depth;
	conf.nb_event_queue_flows = info.max_event_queue_flows;
	conf.dequeue_timeout_ns = info.min_dequeue_timeout_ns;
	conf.nb_events_limit = info.max_num_events;
	conf.nb_event_queues = info.max_event_queues;
	conf.nb_event_ports = 1;

	ret = rte_event_dev_configure(evdev, &conf);
	TEST_ASSERT_SUCCESS(ret, "Failed to configure eventdev");

	ret = rte_event_queue_default_conf_get(evdev, 0, &queue_conf);
	TEST_ASSERT_SUCCESS(ret, "Failed to get default queue conf");

	queue_conf.schedule_type = RTE_SCHED_TYPE_PARALLEL;
	for (i = 0; i < info.max_event_queues; i++) {
		ret = rte_event_queue_setup(evdev, i, &queue_conf);
		TEST_ASSERT_SUCCESS(ret, "Failed to setup queue=%d", i);
	}

	/* Configure event port */
	ret = rte_event_port_setup(evdev, 0, NULL);
	TEST_ASSERT_SUCCESS(ret, "Failed to setup port=%d", 0);
	ret = rte_event_port_link(evdev, 0, NULL, NULL, 0);
	TEST_ASSERT(ret >= 0, "Failed to link all queues port=%d", 0);

	/* If this is a software event device, map and start its service */
	if (rte_event_dev_service_id_get(evdev, &service_id) == 0) {
		TEST_ASSERT_SUCCESS(rte_service_lcore_add(sw_slcore), "Failed to add service core");
		TEST_ASSERT_SUCCESS(rte_service_lcore_start(sw_slcore),
				    "Failed to start service core");
		TEST_ASSERT_SUCCESS(rte_service_map_lcore_set(service_id, sw_slcore, 1),
				    "Failed to map evdev service");
		TEST_ASSERT_SUCCESS(rte_service_runstate_set(service_id, 1),
				    "Failed to start evdev service");
		rte_service_set_stats_enable(service_id, 1);
		evdev_service_id = service_id;
	}

	ret = rte_event_dev_start(evdev);
	TEST_ASSERT_SUCCESS(ret, "Failed to start device");

	evdev_max_queues = info.max_event_queues;

	return TEST_SUCCESS;
}

static int
testsuite_setup(void)
{
	uint32_t service_id;
	uint32_t caps = 0;

	rte_service_lcore_reset_all();

	if (rte_event_dev_count() == 0) {
		RTE_LOG(DEBUG, EAL,
			"Failed to find a valid event device... "
			"testing with event_sw device\n");
		TEST_ASSERT_SUCCESS(rte_vdev_init("event_sw0", NULL), "Error creating eventdev");
		evdev = rte_event_dev_get_dev_id("event_sw0");
	}

	rte_event_vector_adapter_caps_get(evdev, &caps);

	if (rte_event_dev_service_id_get(evdev, &service_id) == 0)
		using_services = true;

	if (using_services)
		sw_slcore = rte_get_next_lcore(-1, 1, 0);

	return eventdev_setup();
}

static void
testsuite_teardown(void)
{
	rte_event_dev_stop(evdev);
	rte_event_dev_close(evdev);
}

static struct unit_test_suite functional_testsuite = {
	.suite_name = "Event vector adapter test suite",
	.setup = testsuite_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {
		TEST_CASE_ST(NULL, test_event_vector_adapter_free,
			     test_event_vector_adapter_create),
		TEST_CASE_ST(NULL, test_event_vector_adapter_free,
			     test_event_vector_adapter_create_multi),
		TEST_CASE_ST(test_event_vector_adapter_create, test_event_vector_adapter_free,
			     test_event_vector_adapter_enqueue),
		TEST_CASE_ST(test_event_vector_adapter_create, test_event_vector_adapter_free,
			     test_event_vector_adapter_enqueue_tmo),
		TEST_CASE_ST(test_event_vector_adapter_create, test_event_vector_adapter_free,
			     test_event_vector_adapter_enqueue_fallback),
		TEST_CASE_ST(test_event_vector_adapter_create, test_event_vector_adapter_free,
			     test_event_vector_adapter_enqueue_sov),
		TEST_CASE_ST(test_event_vector_adapter_create, test_event_vector_adapter_free,
			     test_event_vector_adapter_enqueue_eov),
		TEST_CASE_ST(test_event_vector_adapter_create, test_event_vector_adapter_free,
			     test_event_vector_adapter_enqueue_sov_eov),
		TEST_CASE_ST(test_event_vector_adapter_create, test_event_vector_adapter_free,
			     test_event_vector_adapter_enqueue_flush),
		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static int __rte_unused
test_event_vector_adapter(void)
{
	return unit_test_suite_runner(&functional_testsuite);
}

#endif

/* disabled because of reported failures, waiting for a fix
 * REGISTER_FAST_TEST(event_vector_adapter_autotest, NOHUGE_OK, ASAN_OK, test_event_vector_adapter);
 */
