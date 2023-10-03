/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Cavium, Inc
 */

#include "test.h"

#include <rte_common.h>
#include <rte_hexdump.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>

#ifdef RTE_EXEC_ENV_WINDOWS
static int
test_eventdev_common(void)
{
	printf("eventdev_common not supported on Windows, skipping test\n");
	return TEST_SKIPPED;
}

#else

#include <rte_eventdev.h>
#include <rte_dev.h>
#include <rte_bus_vdev.h>

#define TEST_DEV_ID   0

static int
testsuite_setup(void)
{
	RTE_BUILD_BUG_ON(sizeof(struct rte_event) != 16);
	uint8_t count;
	count = rte_event_dev_count();
	if (!count) {
		printf("Failed to find a valid event device,"
			" testing with event_skeleton device\n");
		return rte_vdev_init("event_skeleton", NULL);
	}
	return TEST_SUCCESS;
}

static void
testsuite_teardown(void)
{
}

static int
test_eventdev_count(void)
{
	uint8_t count;
	count = rte_event_dev_count();
	TEST_ASSERT(count > 0, "Invalid eventdev count %" PRIu8, count);
	return TEST_SUCCESS;
}

static int
test_eventdev_get_dev_id(void)
{
	int ret;
	ret = rte_event_dev_get_dev_id("not_a_valid_eventdev_driver");
	TEST_ASSERT_FAIL(ret, "Expected <0 for invalid dev name ret=%d", ret);
	return TEST_SUCCESS;
}

static int
test_eventdev_socket_id(void)
{
	int socket_id;
	socket_id = rte_event_dev_socket_id(TEST_DEV_ID);
	TEST_ASSERT(socket_id != -EINVAL, "Failed to get socket_id %d",
				socket_id);
	socket_id = rte_event_dev_socket_id(RTE_EVENT_MAX_DEVS);
	TEST_ASSERT(socket_id == -EINVAL, "Expected -EINVAL %d", socket_id);

	return TEST_SUCCESS;
}

static int
test_eventdev_info_get(void)
{
	int ret;
	struct rte_event_dev_info info;
	ret = rte_event_dev_info_get(TEST_DEV_ID, NULL);
	TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);
	ret = rte_event_dev_info_get(TEST_DEV_ID, &info);
	TEST_ASSERT_SUCCESS(ret, "Failed to get event dev info");
	TEST_ASSERT(info.max_event_ports > 0,
			"Not enough event ports %d", info.max_event_ports);
	TEST_ASSERT(info.max_event_queues > 0,
			"Not enough event queues %d", info.max_event_queues);
	return TEST_SUCCESS;
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

static int
test_ethdev_config_run(struct rte_event_dev_config *dev_conf,
		struct rte_event_dev_info *info,
		void (*fn)(struct rte_event_dev_config *dev_conf,
			struct rte_event_dev_info *info))
{
	devconf_set_default_sane_values(dev_conf, info);
	fn(dev_conf, info);
	return rte_event_dev_configure(TEST_DEV_ID, dev_conf);
}

static void
max_dequeue_limit(struct rte_event_dev_config *dev_conf,
		  struct rte_event_dev_info *info)
{
	dev_conf->dequeue_timeout_ns = info->max_dequeue_timeout_ns + 1;
}

static void
max_events_limit(struct rte_event_dev_config *dev_conf,
		  struct rte_event_dev_info *info)
{
	dev_conf->nb_events_limit  = info->max_num_events + 1;
}

static void
max_event_ports(struct rte_event_dev_config *dev_conf,
		  struct rte_event_dev_info *info)
{
	dev_conf->nb_event_ports = info->max_event_ports + 1;
}

static void
max_event_queues(struct rte_event_dev_config *dev_conf,
		  struct rte_event_dev_info *info)
{
	dev_conf->nb_event_queues = info->max_event_queues + 1;
}

static void
max_event_queue_flows(struct rte_event_dev_config *dev_conf,
		  struct rte_event_dev_info *info)
{
	dev_conf->nb_event_queue_flows = info->max_event_queue_flows + 1;
}

static void
max_event_port_dequeue_depth(struct rte_event_dev_config *dev_conf,
		  struct rte_event_dev_info *info)
{
	dev_conf->nb_event_port_dequeue_depth =
		info->max_event_port_dequeue_depth + 1;
}

static void
max_event_port_enqueue_depth(struct rte_event_dev_config *dev_conf,
		  struct rte_event_dev_info *info)
{
	dev_conf->nb_event_port_enqueue_depth =
		info->max_event_port_enqueue_depth + 1;
}


static int
test_eventdev_configure(void)
{
	int ret;
	struct rte_event_dev_config dev_conf;
	struct rte_event_dev_info info;
	ret = rte_event_dev_configure(TEST_DEV_ID, NULL);
	TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);

	ret = rte_event_dev_info_get(TEST_DEV_ID, &info);
	TEST_ASSERT_SUCCESS(ret, "Failed to get event dev info");

	/* Check limits */
	TEST_ASSERT_EQUAL(-EINVAL,
		test_ethdev_config_run(&dev_conf, &info, max_dequeue_limit),
		 "Config negative test failed");
	TEST_ASSERT_EQUAL(-EINVAL,
		test_ethdev_config_run(&dev_conf, &info, max_events_limit),
		 "Config negative test failed");
	TEST_ASSERT_EQUAL(-EINVAL,
		test_ethdev_config_run(&dev_conf, &info, max_event_ports),
		 "Config negative test failed");
	TEST_ASSERT_EQUAL(-EINVAL,
		test_ethdev_config_run(&dev_conf, &info, max_event_queues),
		 "Config negative test failed");
	TEST_ASSERT_EQUAL(-EINVAL,
		test_ethdev_config_run(&dev_conf, &info, max_event_queue_flows),
		"Config negative test failed");

	if (info.event_dev_cap & RTE_EVENT_DEV_CAP_BURST_MODE) {
		TEST_ASSERT_EQUAL(-EINVAL,
				test_ethdev_config_run(&dev_conf, &info,
					max_event_port_dequeue_depth),
				"Config negative test failed");
		TEST_ASSERT_EQUAL(-EINVAL,
				test_ethdev_config_run(&dev_conf, &info,
					max_event_port_enqueue_depth),
				"Config negative test failed");
	}

	/* Positive case */
	devconf_set_default_sane_values(&dev_conf, &info);
	ret = rte_event_dev_configure(TEST_DEV_ID, &dev_conf);
	TEST_ASSERT_SUCCESS(ret, "Failed to configure eventdev");

	/* re-configure */
	devconf_set_default_sane_values(&dev_conf, &info);
	dev_conf.nb_event_ports = RTE_MAX(info.max_event_ports/2, 1);
	dev_conf.nb_event_queues = RTE_MAX(info.max_event_queues/2, 1);
	ret = rte_event_dev_configure(TEST_DEV_ID, &dev_conf);
	TEST_ASSERT_SUCCESS(ret, "Failed to re configure eventdev");

	/* re-configure back to max_event_queues and max_event_ports */
	devconf_set_default_sane_values(&dev_conf, &info);
	ret = rte_event_dev_configure(TEST_DEV_ID, &dev_conf);
	TEST_ASSERT_SUCCESS(ret, "Failed to re-configure eventdev");

	return TEST_SUCCESS;

}

static int
eventdev_configure_setup(void)
{
	int ret;
	struct rte_event_dev_config dev_conf;
	struct rte_event_dev_info info;

	ret = rte_event_dev_info_get(TEST_DEV_ID, &info);
	TEST_ASSERT_SUCCESS(ret, "Failed to get event dev info");
	devconf_set_default_sane_values(&dev_conf, &info);
	ret = rte_event_dev_configure(TEST_DEV_ID, &dev_conf);
	TEST_ASSERT_SUCCESS(ret, "Failed to configure eventdev");

	return TEST_SUCCESS;
}

static int
test_eventdev_queue_default_conf_get(void)
{
	int i, ret;
	struct rte_event_queue_conf qconf;

	ret = rte_event_queue_default_conf_get(TEST_DEV_ID, 0, NULL);
	TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);

	uint32_t queue_count;
	TEST_ASSERT_SUCCESS(rte_event_dev_attr_get(TEST_DEV_ID,
			    RTE_EVENT_DEV_ATTR_QUEUE_COUNT, &queue_count),
			    "Queue count get failed");

	for (i = 0; i < (int)queue_count; i++) {
		ret = rte_event_queue_default_conf_get(TEST_DEV_ID, i,
						 &qconf);
		TEST_ASSERT_SUCCESS(ret, "Failed to get queue%d info", i);
	}

	return TEST_SUCCESS;
}

static int
test_eventdev_queue_setup(void)
{
	int i, ret;
	struct rte_event_dev_info info;
	struct rte_event_queue_conf qconf;

	ret = rte_event_dev_info_get(TEST_DEV_ID, &info);
	TEST_ASSERT_SUCCESS(ret, "Failed to get event dev info");

	/* Negative cases */
	ret = rte_event_queue_default_conf_get(TEST_DEV_ID, 0, &qconf);
	TEST_ASSERT_SUCCESS(ret, "Failed to get queue0 info");
	qconf.event_queue_cfg =	RTE_EVENT_QUEUE_CFG_ALL_TYPES;
	qconf.nb_atomic_flows = info.max_event_queue_flows + 1;
	ret = rte_event_queue_setup(TEST_DEV_ID, 0, &qconf);
	TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);

	qconf.nb_atomic_flows = info.max_event_queue_flows;
	qconf.schedule_type = RTE_SCHED_TYPE_ORDERED;
	qconf.nb_atomic_order_sequences = info.max_event_queue_flows + 1;
	ret = rte_event_queue_setup(TEST_DEV_ID, 0, &qconf);
	TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);

	ret = rte_event_queue_setup(TEST_DEV_ID, info.max_event_queues,
					&qconf);
	TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);

	/* Positive case */
	ret = rte_event_queue_default_conf_get(TEST_DEV_ID, 0, &qconf);
	TEST_ASSERT_SUCCESS(ret, "Failed to get queue0 info");
	ret = rte_event_queue_setup(TEST_DEV_ID, 0, &qconf);
	TEST_ASSERT_SUCCESS(ret, "Failed to setup queue0");

	uint32_t queue_count;
	TEST_ASSERT_SUCCESS(rte_event_dev_attr_get(TEST_DEV_ID,
			    RTE_EVENT_DEV_ATTR_QUEUE_COUNT, &queue_count),
			    "Queue count get failed");

	for (i = 0; i < (int)queue_count; i++) {
		ret = rte_event_queue_setup(TEST_DEV_ID, i, NULL);
		TEST_ASSERT_SUCCESS(ret, "Failed to setup queue%d", i);
	}

	return TEST_SUCCESS;
}

static int
test_eventdev_queue_count(void)
{
	int ret;
	struct rte_event_dev_info info;

	ret = rte_event_dev_info_get(TEST_DEV_ID, &info);
	TEST_ASSERT_SUCCESS(ret, "Failed to get event dev info");

	uint32_t queue_count;
	TEST_ASSERT_SUCCESS(rte_event_dev_attr_get(TEST_DEV_ID,
			    RTE_EVENT_DEV_ATTR_QUEUE_COUNT, &queue_count),
			    "Queue count get failed");
	TEST_ASSERT_EQUAL(queue_count, info.max_event_queues,
			  "Wrong queue count");

	return TEST_SUCCESS;
}

static int
test_eventdev_queue_attr_priority(void)
{
	int i, ret;
	struct rte_event_dev_info info;
	struct rte_event_queue_conf qconf;
	uint8_t priority;

	ret = rte_event_dev_info_get(TEST_DEV_ID, &info);
	TEST_ASSERT_SUCCESS(ret, "Failed to get event dev info");

	uint32_t queue_count;
	TEST_ASSERT_SUCCESS(rte_event_dev_attr_get(TEST_DEV_ID,
			    RTE_EVENT_DEV_ATTR_QUEUE_COUNT, &queue_count),
			    "Queue count get failed");

	for (i = 0; i < (int)queue_count; i++) {
		ret = rte_event_queue_default_conf_get(TEST_DEV_ID, i,
					&qconf);
		TEST_ASSERT_SUCCESS(ret, "Failed to get queue%d def conf", i);
		qconf.priority = i %  RTE_EVENT_DEV_PRIORITY_LOWEST;
		ret = rte_event_queue_setup(TEST_DEV_ID, i, &qconf);
		TEST_ASSERT_SUCCESS(ret, "Failed to setup queue%d", i);
	}

	for (i = 0; i < (int)queue_count; i++) {
		uint32_t tmp;
		TEST_ASSERT_SUCCESS(rte_event_queue_attr_get(TEST_DEV_ID, i,
				    RTE_EVENT_QUEUE_ATTR_PRIORITY, &tmp),
				    "Queue priority get failed");
		priority = tmp;

		if (info.event_dev_cap & RTE_EVENT_DEV_CAP_QUEUE_QOS)
			TEST_ASSERT_EQUAL(priority,
			 i %  RTE_EVENT_DEV_PRIORITY_LOWEST,
			 "Wrong priority value for queue%d", i);
		else
			TEST_ASSERT_EQUAL(priority,
			 RTE_EVENT_DEV_PRIORITY_NORMAL,
			 "Wrong priority value for queue%d", i);
	}

	return TEST_SUCCESS;
}

static int
test_eventdev_queue_attr_priority_runtime(void)
{
	uint32_t queue_count, queue_req, prio, deq_cnt;
	struct rte_event_queue_conf qconf;
	struct rte_event_port_conf pconf;
	struct rte_event_dev_info info;
	struct rte_event event = {
		.op = RTE_EVENT_OP_NEW,
		.event_type = RTE_EVENT_TYPE_CPU,
		.sched_type = RTE_SCHED_TYPE_ATOMIC,
		.u64 = 0xbadbadba,
	};
	int i, ret;

	ret = rte_event_dev_info_get(TEST_DEV_ID, &info);
	TEST_ASSERT_SUCCESS(ret, "Failed to get event dev info");

	if (!(info.event_dev_cap & RTE_EVENT_DEV_CAP_RUNTIME_QUEUE_ATTR))
		return TEST_SKIPPED;

	TEST_ASSERT_SUCCESS(rte_event_dev_attr_get(
				    TEST_DEV_ID, RTE_EVENT_DEV_ATTR_QUEUE_COUNT,
				    &queue_count),
			    "Queue count get failed");

	/* Need at least 2 queues to test LOW and HIGH priority. */
	TEST_ASSERT(queue_count > 1, "Not enough event queues, needed 2");
	queue_req = 2;

	for (i = 0; i < (int)queue_count; i++) {
		ret = rte_event_queue_default_conf_get(TEST_DEV_ID, i, &qconf);
		TEST_ASSERT_SUCCESS(ret, "Failed to get queue%d def conf", i);
		ret = rte_event_queue_setup(TEST_DEV_ID, i, &qconf);
		TEST_ASSERT_SUCCESS(ret, "Failed to setup queue%d", i);
	}

	ret = rte_event_queue_attr_set(TEST_DEV_ID, 0,
				       RTE_EVENT_QUEUE_ATTR_PRIORITY,
				       RTE_EVENT_DEV_PRIORITY_LOWEST);
	if (ret == -ENOTSUP)
		return TEST_SKIPPED;
	TEST_ASSERT_SUCCESS(ret, "Queue0 priority set failed");

	ret = rte_event_queue_attr_set(TEST_DEV_ID, 1,
				       RTE_EVENT_QUEUE_ATTR_PRIORITY,
				       RTE_EVENT_DEV_PRIORITY_HIGHEST);
	if (ret == -ENOTSUP)
		return TEST_SKIPPED;
	TEST_ASSERT_SUCCESS(ret, "Queue1 priority set failed");

	/* Setup event port 0 */
	ret = rte_event_port_default_conf_get(TEST_DEV_ID, 0, &pconf);
	TEST_ASSERT_SUCCESS(ret, "Failed to get port0 info");
	ret = rte_event_port_setup(TEST_DEV_ID, 0, &pconf);
	TEST_ASSERT_SUCCESS(ret, "Failed to setup port0");
	ret = rte_event_port_link(TEST_DEV_ID, 0, NULL, NULL, 0);
	TEST_ASSERT(ret == (int)queue_count, "Failed to link port, device %d",
		    TEST_DEV_ID);

	ret = rte_event_dev_start(TEST_DEV_ID);
	TEST_ASSERT_SUCCESS(ret, "Failed to start device%d", TEST_DEV_ID);

	for (i = 0; i < (int)queue_req; i++) {
		event.queue_id = i;
		while (rte_event_enqueue_burst(TEST_DEV_ID, 0, &event, 1) != 1)
			rte_pause();
	}

	prio = RTE_EVENT_DEV_PRIORITY_HIGHEST;
	deq_cnt = 0;
	while (deq_cnt < queue_req) {
		uint32_t queue_prio;

		if (rte_event_dequeue_burst(TEST_DEV_ID, 0, &event, 1, 0) == 0)
			continue;

		ret = rte_event_queue_attr_get(TEST_DEV_ID, event.queue_id,
					       RTE_EVENT_QUEUE_ATTR_PRIORITY,
					       &queue_prio);
		if (ret == -ENOTSUP)
			return TEST_SKIPPED;

		TEST_ASSERT_SUCCESS(ret, "Queue priority get failed");
		TEST_ASSERT(queue_prio >= prio,
			    "Received event from a lower priority queue first");
		prio = queue_prio;
		deq_cnt++;
	}

	return TEST_SUCCESS;
}

static int
test_eventdev_queue_attr_weight_runtime(void)
{
	struct rte_event_queue_conf qconf;
	struct rte_event_dev_info info;
	uint32_t queue_count;
	int i, ret;

	ret = rte_event_dev_info_get(TEST_DEV_ID, &info);
	TEST_ASSERT_SUCCESS(ret, "Failed to get event dev info");

	if (!(info.event_dev_cap & RTE_EVENT_DEV_CAP_RUNTIME_QUEUE_ATTR))
		return TEST_SKIPPED;

	TEST_ASSERT_SUCCESS(rte_event_dev_attr_get(
				    TEST_DEV_ID, RTE_EVENT_DEV_ATTR_QUEUE_COUNT,
				    &queue_count),
			    "Queue count get failed");

	for (i = 0; i < (int)queue_count; i++) {
		ret = rte_event_queue_default_conf_get(TEST_DEV_ID, i, &qconf);
		TEST_ASSERT_SUCCESS(ret, "Failed to get queue%d def conf", i);
		ret = rte_event_queue_setup(TEST_DEV_ID, i, &qconf);
		TEST_ASSERT_SUCCESS(ret, "Failed to setup queue%d", i);
	}

	for (i = 0; i < (int)queue_count; i++) {
		uint32_t get_val;
		uint64_t set_val;

		set_val = i % RTE_EVENT_QUEUE_WEIGHT_HIGHEST;
		ret = rte_event_queue_attr_set(
			TEST_DEV_ID, i, RTE_EVENT_QUEUE_ATTR_WEIGHT, set_val);
		if (ret == -ENOTSUP)
			return TEST_SKIPPED;

		TEST_ASSERT_SUCCESS(ret, "Queue weight set failed");

		ret = rte_event_queue_attr_get(
			TEST_DEV_ID, i, RTE_EVENT_QUEUE_ATTR_WEIGHT, &get_val);
		if (ret == -ENOTSUP)
			return TEST_SKIPPED;

		TEST_ASSERT_SUCCESS(ret, "Queue weight get failed");
		TEST_ASSERT_EQUAL(get_val, set_val,
				  "Wrong weight value for queue%d", i);
	}

	return TEST_SUCCESS;
}

static int
test_eventdev_queue_attr_affinity_runtime(void)
{
	struct rte_event_queue_conf qconf;
	struct rte_event_dev_info info;
	uint32_t queue_count;
	int i, ret;

	ret = rte_event_dev_info_get(TEST_DEV_ID, &info);
	TEST_ASSERT_SUCCESS(ret, "Failed to get event dev info");

	if (!(info.event_dev_cap & RTE_EVENT_DEV_CAP_RUNTIME_QUEUE_ATTR))
		return TEST_SKIPPED;

	TEST_ASSERT_SUCCESS(rte_event_dev_attr_get(
				    TEST_DEV_ID, RTE_EVENT_DEV_ATTR_QUEUE_COUNT,
				    &queue_count),
			    "Queue count get failed");

	for (i = 0; i < (int)queue_count; i++) {
		ret = rte_event_queue_default_conf_get(TEST_DEV_ID, i, &qconf);
		TEST_ASSERT_SUCCESS(ret, "Failed to get queue%d def conf", i);
		ret = rte_event_queue_setup(TEST_DEV_ID, i, &qconf);
		TEST_ASSERT_SUCCESS(ret, "Failed to setup queue%d", i);
	}

	for (i = 0; i < (int)queue_count; i++) {
		uint32_t get_val;
		uint64_t set_val;

		set_val = i % RTE_EVENT_QUEUE_AFFINITY_HIGHEST;
		ret = rte_event_queue_attr_set(
			TEST_DEV_ID, i, RTE_EVENT_QUEUE_ATTR_AFFINITY, set_val);
		if (ret == -ENOTSUP)
			return TEST_SKIPPED;

		TEST_ASSERT_SUCCESS(ret, "Queue affinity set failed");

		ret = rte_event_queue_attr_get(
			TEST_DEV_ID, i, RTE_EVENT_QUEUE_ATTR_AFFINITY, &get_val);
		if (ret == -ENOTSUP)
			return TEST_SKIPPED;

		TEST_ASSERT_SUCCESS(ret, "Queue affinity get failed");
		TEST_ASSERT_EQUAL(get_val, set_val,
				  "Wrong affinity value for queue%d", i);
	}

	return TEST_SUCCESS;
}

static int
test_eventdev_queue_attr_nb_atomic_flows(void)
{
	int i, ret;
	struct rte_event_dev_info info;
	struct rte_event_queue_conf qconf;
	uint32_t nb_atomic_flows;

	ret = rte_event_dev_info_get(TEST_DEV_ID, &info);
	TEST_ASSERT_SUCCESS(ret, "Failed to get event dev info");

	uint32_t queue_count;
	TEST_ASSERT_SUCCESS(rte_event_dev_attr_get(TEST_DEV_ID,
			    RTE_EVENT_DEV_ATTR_QUEUE_COUNT, &queue_count),
			    "Queue count get failed");

	ret = rte_event_queue_default_conf_get(TEST_DEV_ID, 0, &qconf);
	TEST_ASSERT_SUCCESS(ret, "Failed to get queue 0's def conf");

	if (qconf.nb_atomic_flows == 0)
		/* Assume PMD doesn't support atomic flows, return early */
		return -ENOTSUP;

	qconf.schedule_type = RTE_SCHED_TYPE_ATOMIC;

	for (i = 0; i < (int)queue_count; i++) {
		ret = rte_event_queue_setup(TEST_DEV_ID, i, &qconf);
		TEST_ASSERT_SUCCESS(ret, "Failed to setup queue%d", i);
	}

	for (i = 0; i < (int)queue_count; i++) {
		TEST_ASSERT_SUCCESS(rte_event_queue_attr_get(TEST_DEV_ID, i,
				    RTE_EVENT_QUEUE_ATTR_NB_ATOMIC_FLOWS,
				    &nb_atomic_flows),
				    "Queue nb_atomic_flows get failed");

		TEST_ASSERT_EQUAL(nb_atomic_flows, qconf.nb_atomic_flows,
				  "Wrong atomic flows value for queue%d", i);
	}

	return TEST_SUCCESS;
}

static int
test_eventdev_queue_attr_nb_atomic_order_sequences(void)
{
	int i, ret;
	struct rte_event_dev_info info;
	struct rte_event_queue_conf qconf;
	uint32_t nb_atomic_order_sequences;

	ret = rte_event_dev_info_get(TEST_DEV_ID, &info);
	TEST_ASSERT_SUCCESS(ret, "Failed to get event dev info");

	uint32_t queue_count;
	TEST_ASSERT_SUCCESS(rte_event_dev_attr_get(TEST_DEV_ID,
			    RTE_EVENT_DEV_ATTR_QUEUE_COUNT, &queue_count),
			    "Queue count get failed");

	ret = rte_event_queue_default_conf_get(TEST_DEV_ID, 0, &qconf);
	TEST_ASSERT_SUCCESS(ret, "Failed to get queue 0's def conf");

	if (qconf.nb_atomic_order_sequences == 0)
		/* Assume PMD doesn't support reordering */
		return -ENOTSUP;

	qconf.schedule_type = RTE_SCHED_TYPE_ORDERED;

	for (i = 0; i < (int)queue_count; i++) {
		ret = rte_event_queue_setup(TEST_DEV_ID, i, &qconf);
		TEST_ASSERT_SUCCESS(ret, "Failed to setup queue%d", i);
	}

	for (i = 0; i < (int)queue_count; i++) {
		TEST_ASSERT_SUCCESS(rte_event_queue_attr_get(TEST_DEV_ID, i,
			    RTE_EVENT_QUEUE_ATTR_NB_ATOMIC_ORDER_SEQUENCES,
			    &nb_atomic_order_sequences),
			    "Queue nb_atomic_order_sequencess get failed");

		TEST_ASSERT_EQUAL(nb_atomic_order_sequences,
				  qconf.nb_atomic_order_sequences,
				  "Wrong atomic order sequences value for queue%d",
				  i);
	}

	return TEST_SUCCESS;
}

static int
test_eventdev_queue_attr_event_queue_cfg(void)
{
	int i, ret;
	struct rte_event_dev_info info;
	struct rte_event_queue_conf qconf;
	uint32_t event_queue_cfg;

	ret = rte_event_dev_info_get(TEST_DEV_ID, &info);
	TEST_ASSERT_SUCCESS(ret, "Failed to get event dev info");

	uint32_t queue_count;
	TEST_ASSERT_SUCCESS(rte_event_dev_attr_get(TEST_DEV_ID,
			    RTE_EVENT_DEV_ATTR_QUEUE_COUNT, &queue_count),
			    "Queue count get failed");

	ret = rte_event_queue_default_conf_get(TEST_DEV_ID, 0, &qconf);
	TEST_ASSERT_SUCCESS(ret, "Failed to get queue0 def conf");

	qconf.event_queue_cfg = RTE_EVENT_QUEUE_CFG_SINGLE_LINK;

	for (i = 0; i < (int)queue_count; i++) {
		ret = rte_event_queue_setup(TEST_DEV_ID, i, &qconf);
		TEST_ASSERT_SUCCESS(ret, "Failed to setup queue%d", i);
	}

	for (i = 0; i < (int)queue_count; i++) {
		TEST_ASSERT_SUCCESS(rte_event_queue_attr_get(TEST_DEV_ID, i,
				    RTE_EVENT_QUEUE_ATTR_EVENT_QUEUE_CFG,
				    &event_queue_cfg),
				    "Queue event_queue_cfg get failed");

		TEST_ASSERT_EQUAL(event_queue_cfg, qconf.event_queue_cfg,
				  "Wrong event_queue_cfg value for queue%d",
				  i);
	}

	return TEST_SUCCESS;
}

static int
test_eventdev_port_default_conf_get(void)
{
	int i, ret;
	struct rte_event_port_conf pconf;

	ret = rte_event_port_default_conf_get(TEST_DEV_ID, 0, NULL);
	TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);

	uint32_t port_count;
	TEST_ASSERT_SUCCESS(rte_event_dev_attr_get(TEST_DEV_ID,
				RTE_EVENT_DEV_ATTR_PORT_COUNT,
				&port_count), "Port count get failed");

	ret = rte_event_port_default_conf_get(TEST_DEV_ID,
			port_count + 1, NULL);
	TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);

	for (i = 0; i < (int)port_count; i++) {
		ret = rte_event_port_default_conf_get(TEST_DEV_ID, i,
							&pconf);
		TEST_ASSERT_SUCCESS(ret, "Failed to get port%d info", i);
	}

	return TEST_SUCCESS;
}

static int
test_eventdev_port_setup(void)
{
	int i, ret;
	struct rte_event_dev_info info;
	struct rte_event_port_conf pconf;

	ret = rte_event_dev_info_get(TEST_DEV_ID, &info);
	TEST_ASSERT_SUCCESS(ret, "Failed to get event dev info");

	/* Negative cases */
	ret = rte_event_port_default_conf_get(TEST_DEV_ID, 0, &pconf);
	TEST_ASSERT_SUCCESS(ret, "Failed to get port0 info");
	pconf.new_event_threshold = info.max_num_events + 1;
	ret = rte_event_port_setup(TEST_DEV_ID, 0, &pconf);
	TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);

	pconf.new_event_threshold = info.max_num_events;
	pconf.dequeue_depth = info.max_event_port_dequeue_depth + 1;
	ret = rte_event_port_setup(TEST_DEV_ID, 0, &pconf);
	TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);

	pconf.dequeue_depth = info.max_event_port_dequeue_depth;
	pconf.enqueue_depth = info.max_event_port_enqueue_depth + 1;
	ret = rte_event_port_setup(TEST_DEV_ID, 0, &pconf);
	TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);

	if (!(info.event_dev_cap &
	      RTE_EVENT_DEV_CAP_IMPLICIT_RELEASE_DISABLE)) {
		pconf.enqueue_depth = info.max_event_port_enqueue_depth;
		pconf.event_port_cfg = RTE_EVENT_PORT_CFG_DISABLE_IMPL_REL;
		ret = rte_event_port_setup(TEST_DEV_ID, 0, &pconf);
		TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);
		pconf.event_port_cfg = 0;
	}

	ret = rte_event_port_setup(TEST_DEV_ID, info.max_event_ports,
					&pconf);
	TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);

	/* Positive case */
	ret = rte_event_port_default_conf_get(TEST_DEV_ID, 0, &pconf);
	TEST_ASSERT_SUCCESS(ret, "Failed to get port0 info");
	ret = rte_event_port_setup(TEST_DEV_ID, 0, &pconf);
	TEST_ASSERT_SUCCESS(ret, "Failed to setup port0");

	uint32_t port_count;
	TEST_ASSERT_SUCCESS(rte_event_dev_attr_get(TEST_DEV_ID,
				RTE_EVENT_DEV_ATTR_PORT_COUNT,
				&port_count), "Port count get failed");

	for (i = 0; i < (int)port_count; i++) {
		ret = rte_event_port_setup(TEST_DEV_ID, i, NULL);
		TEST_ASSERT_SUCCESS(ret, "Failed to setup port%d", i);
	}

	return TEST_SUCCESS;
}

static int
test_eventdev_port_attr_dequeue_depth(void)
{
	int ret;
	struct rte_event_dev_info info;
	struct rte_event_port_conf pconf;

	ret = rte_event_dev_info_get(TEST_DEV_ID, &info);
	TEST_ASSERT_SUCCESS(ret, "Failed to get event dev info");

	ret = rte_event_port_default_conf_get(TEST_DEV_ID, 0, &pconf);
	TEST_ASSERT_SUCCESS(ret, "Failed to get port0 info");
	ret = rte_event_port_setup(TEST_DEV_ID, 0, &pconf);
	TEST_ASSERT_SUCCESS(ret, "Failed to setup port0");

	uint32_t value;
	TEST_ASSERT_EQUAL(rte_event_port_attr_get(TEST_DEV_ID, 0,
			RTE_EVENT_PORT_ATTR_DEQ_DEPTH, &value),
			0, "Call to get port dequeue depth failed");
	TEST_ASSERT_EQUAL(value, pconf.dequeue_depth,
			"Wrong port dequeue depth");

	return TEST_SUCCESS;
}

static int
test_eventdev_port_attr_enqueue_depth(void)
{
	int ret;
	struct rte_event_dev_info info;
	struct rte_event_port_conf pconf;

	ret = rte_event_dev_info_get(TEST_DEV_ID, &info);
	TEST_ASSERT_SUCCESS(ret, "Failed to get event dev info");

	ret = rte_event_port_default_conf_get(TEST_DEV_ID, 0, &pconf);
	TEST_ASSERT_SUCCESS(ret, "Failed to get port0 info");
	ret = rte_event_port_setup(TEST_DEV_ID, 0, &pconf);
	TEST_ASSERT_SUCCESS(ret, "Failed to setup port0");

	uint32_t value;
	TEST_ASSERT_EQUAL(rte_event_port_attr_get(TEST_DEV_ID, 0,
			RTE_EVENT_PORT_ATTR_ENQ_DEPTH, &value),
			0, "Call to get port enqueue depth failed");
	TEST_ASSERT_EQUAL(value, pconf.enqueue_depth,
			"Wrong port enqueue depth");

	return TEST_SUCCESS;
}

static int
test_eventdev_port_attr_new_event_threshold(void)
{
	int ret;
	struct rte_event_dev_info info;
	struct rte_event_port_conf pconf;

	ret = rte_event_dev_info_get(TEST_DEV_ID, &info);
	TEST_ASSERT_SUCCESS(ret, "Failed to get event dev info");

	ret = rte_event_port_default_conf_get(TEST_DEV_ID, 0, &pconf);
	TEST_ASSERT_SUCCESS(ret, "Failed to get port0 info");
	ret = rte_event_port_setup(TEST_DEV_ID, 0, &pconf);
	TEST_ASSERT_SUCCESS(ret, "Failed to setup port0");

	uint32_t value;
	TEST_ASSERT_EQUAL(rte_event_port_attr_get(TEST_DEV_ID, 0,
			RTE_EVENT_PORT_ATTR_NEW_EVENT_THRESHOLD, &value),
			0, "Call to get port new event threshold failed");
	TEST_ASSERT_EQUAL((int32_t) value, pconf.new_event_threshold,
			"Wrong port new event threshold");

	return TEST_SUCCESS;
}

static int
test_eventdev_port_count(void)
{
	int ret;
	struct rte_event_dev_info info;

	ret = rte_event_dev_info_get(TEST_DEV_ID, &info);
	TEST_ASSERT_SUCCESS(ret, "Failed to get event dev info");

	uint32_t port_count;
	TEST_ASSERT_SUCCESS(rte_event_dev_attr_get(TEST_DEV_ID,
				RTE_EVENT_DEV_ATTR_PORT_COUNT,
				&port_count), "Port count get failed");
	TEST_ASSERT_EQUAL(port_count, info.max_event_ports, "Wrong port count");

	return TEST_SUCCESS;
}

static int
test_eventdev_timeout_ticks(void)
{
	int ret;
	uint64_t timeout_ticks;

	ret = rte_event_dequeue_timeout_ticks(TEST_DEV_ID, 100, &timeout_ticks);
	if (ret != -ENOTSUP)
		TEST_ASSERT_SUCCESS(ret, "Fail to get timeout_ticks");

	return ret;
}


static int
test_eventdev_start_stop(void)
{
	int i, ret;

	ret = eventdev_configure_setup();
	TEST_ASSERT_SUCCESS(ret, "Failed to configure eventdev");

	uint32_t queue_count;
	TEST_ASSERT_SUCCESS(rte_event_dev_attr_get(TEST_DEV_ID,
			    RTE_EVENT_DEV_ATTR_QUEUE_COUNT, &queue_count),
			    "Queue count get failed");
	for (i = 0; i < (int)queue_count; i++) {
		ret = rte_event_queue_setup(TEST_DEV_ID, i, NULL);
		TEST_ASSERT_SUCCESS(ret, "Failed to setup queue%d", i);
	}

	uint32_t port_count;
	TEST_ASSERT_SUCCESS(rte_event_dev_attr_get(TEST_DEV_ID,
				RTE_EVENT_DEV_ATTR_PORT_COUNT,
				&port_count), "Port count get failed");

	for (i = 0; i < (int)port_count; i++) {
		ret = rte_event_port_setup(TEST_DEV_ID, i, NULL);
		TEST_ASSERT_SUCCESS(ret, "Failed to setup port%d", i);
	}

	ret = rte_event_port_link(TEST_DEV_ID, 0, NULL, NULL, 0);
	TEST_ASSERT(ret == (int)queue_count, "Failed to link port, device %d",
		    TEST_DEV_ID);

	ret = rte_event_dev_start(TEST_DEV_ID);
	TEST_ASSERT_SUCCESS(ret, "Failed to start device%d", TEST_DEV_ID);

	rte_event_dev_stop(TEST_DEV_ID);
	return TEST_SUCCESS;
}


static int
eventdev_setup_device(void)
{
	int i, ret;

	ret = eventdev_configure_setup();
	TEST_ASSERT_SUCCESS(ret, "Failed to configure eventdev");

	uint32_t queue_count;
	TEST_ASSERT_SUCCESS(rte_event_dev_attr_get(TEST_DEV_ID,
			    RTE_EVENT_DEV_ATTR_QUEUE_COUNT, &queue_count),
			    "Queue count get failed");
	for (i = 0; i < (int)queue_count; i++) {
		ret = rte_event_queue_setup(TEST_DEV_ID, i, NULL);
		TEST_ASSERT_SUCCESS(ret, "Failed to setup queue%d", i);
	}

	uint32_t port_count;
	TEST_ASSERT_SUCCESS(rte_event_dev_attr_get(TEST_DEV_ID,
				RTE_EVENT_DEV_ATTR_PORT_COUNT,
				&port_count), "Port count get failed");

	for (i = 0; i < (int)port_count; i++) {
		ret = rte_event_port_setup(TEST_DEV_ID, i, NULL);
		TEST_ASSERT_SUCCESS(ret, "Failed to setup port%d", i);
	}

	ret = rte_event_port_link(TEST_DEV_ID, 0, NULL, NULL, 0);
	TEST_ASSERT(ret == (int)queue_count, "Failed to link port, device %d",
		    TEST_DEV_ID);

	ret = rte_event_dev_start(TEST_DEV_ID);
	TEST_ASSERT_SUCCESS(ret, "Failed to start device%d", TEST_DEV_ID);

	return TEST_SUCCESS;
}

static void
eventdev_stop_device(void)
{
	rte_event_dev_stop(TEST_DEV_ID);
}

static int
test_eventdev_link(void)
{
	int ret, nb_queues, i;
	uint8_t queues[RTE_EVENT_MAX_QUEUES_PER_DEV];
	uint8_t priorities[RTE_EVENT_MAX_QUEUES_PER_DEV];

	ret = rte_event_port_link(TEST_DEV_ID, 0, NULL, NULL, 0);
	TEST_ASSERT(ret >= 0, "Failed to link with NULL device%d",
				 TEST_DEV_ID);

	uint32_t queue_count;
	TEST_ASSERT_SUCCESS(rte_event_dev_attr_get(TEST_DEV_ID,
			    RTE_EVENT_DEV_ATTR_QUEUE_COUNT, &queue_count),
			    "Queue count get failed");
	nb_queues = queue_count;
	for (i = 0; i < nb_queues; i++) {
		queues[i] = i;
		priorities[i] = RTE_EVENT_DEV_PRIORITY_NORMAL;
	}

	ret = rte_event_port_link(TEST_DEV_ID, 0, queues,
					priorities, nb_queues);
	TEST_ASSERT(ret == nb_queues, "Failed to link(device%d) ret=%d",
				 TEST_DEV_ID, ret);
	return TEST_SUCCESS;
}

static int
test_eventdev_unlink(void)
{
	int ret, nb_queues, i;
	uint8_t queues[RTE_EVENT_MAX_QUEUES_PER_DEV];

	ret = rte_event_port_unlink(TEST_DEV_ID, 0, NULL, 0);
	TEST_ASSERT(ret >= 0, "Failed to unlink with NULL device%d",
				 TEST_DEV_ID);

	uint32_t queue_count;
	TEST_ASSERT_SUCCESS(rte_event_dev_attr_get(TEST_DEV_ID,
			    RTE_EVENT_DEV_ATTR_QUEUE_COUNT, &queue_count),
			    "Queue count get failed");
	nb_queues = queue_count;
	for (i = 0; i < nb_queues; i++)
		queues[i] = i;

	ret = rte_event_port_link(TEST_DEV_ID, 0, NULL, NULL, 0);
	TEST_ASSERT(ret >= 0, "Failed to link with NULL device%d",
				 TEST_DEV_ID);

	ret = rte_event_port_unlink(TEST_DEV_ID, 0, queues, nb_queues);
	TEST_ASSERT(ret == nb_queues, "Failed to unlink(device%d) ret=%d",
				 TEST_DEV_ID, ret);
	return TEST_SUCCESS;
}

static int
test_eventdev_link_get(void)
{
	int ret, i;
	uint8_t queues[RTE_EVENT_MAX_QUEUES_PER_DEV];
	uint8_t priorities[RTE_EVENT_MAX_QUEUES_PER_DEV];

	/* link all queues */
	ret = rte_event_port_link(TEST_DEV_ID, 0, NULL, NULL, 0);
	TEST_ASSERT(ret >= 0, "Failed to link with NULL device%d",
				 TEST_DEV_ID);

	uint32_t queue_count;
	TEST_ASSERT_SUCCESS(rte_event_dev_attr_get(TEST_DEV_ID,
			    RTE_EVENT_DEV_ATTR_QUEUE_COUNT, &queue_count),
			    "Queue count get failed");
	const int nb_queues = queue_count;
	for (i = 0; i < nb_queues; i++)
		queues[i] = i;

	ret = rte_event_port_unlink(TEST_DEV_ID, 0, queues, nb_queues);
	TEST_ASSERT(ret == nb_queues, "Failed to unlink(device%d) ret=%d",
				 TEST_DEV_ID, ret);

	ret = rte_event_port_links_get(TEST_DEV_ID, 0, queues, priorities);
	TEST_ASSERT(ret == 0, "(%d)Wrong link get=%d", TEST_DEV_ID, ret);

	/* link all queues and get the links */
	for (i = 0; i < nb_queues; i++) {
		queues[i] = i;
		priorities[i] = RTE_EVENT_DEV_PRIORITY_NORMAL;
	}
	ret = rte_event_port_link(TEST_DEV_ID, 0, queues, priorities,
					 nb_queues);
	TEST_ASSERT(ret == nb_queues, "Failed to link(device%d) ret=%d",
				 TEST_DEV_ID, ret);
	ret = rte_event_port_links_get(TEST_DEV_ID, 0, queues, priorities);
	TEST_ASSERT(ret == nb_queues, "(%d)Wrong link get ret=%d expected=%d",
				 TEST_DEV_ID, ret, nb_queues);
	/* unlink all*/
	ret = rte_event_port_unlink(TEST_DEV_ID, 0, NULL, 0);
	TEST_ASSERT(ret == nb_queues, "Failed to unlink(device%d) ret=%d",
				 TEST_DEV_ID, ret);
	/* link just one queue */
	queues[0] = 0;
	priorities[0] = RTE_EVENT_DEV_PRIORITY_NORMAL;

	ret = rte_event_port_link(TEST_DEV_ID, 0, queues, priorities, 1);
	TEST_ASSERT(ret == 1, "Failed to link(device%d) ret=%d",
				 TEST_DEV_ID, ret);
	ret = rte_event_port_links_get(TEST_DEV_ID, 0, queues, priorities);
	TEST_ASSERT(ret == 1, "(%d)Wrong link get ret=%d expected=%d",
					TEST_DEV_ID, ret, 1);
	/* unlink the queue */
	ret = rte_event_port_unlink(TEST_DEV_ID, 0, NULL, 0);
	TEST_ASSERT(ret == 1, "Failed to unlink(device%d) ret=%d",
				 TEST_DEV_ID, ret);

	/* 4links and 2 unlinks */
	if (nb_queues >= 4) {
		for (i = 0; i < 4; i++) {
			queues[i] = i;
			priorities[i] = 0x40;
		}
		ret = rte_event_port_link(TEST_DEV_ID, 0, queues, priorities,
						4);
		TEST_ASSERT(ret == 4, "Failed to link(device%d) ret=%d",
					 TEST_DEV_ID, ret);

		for (i = 0; i < 2; i++)
			queues[i] = i;

		ret = rte_event_port_unlink(TEST_DEV_ID, 0, queues, 2);
		TEST_ASSERT(ret == 2, "Failed to unlink(device%d) ret=%d",
					 TEST_DEV_ID, ret);
		ret = rte_event_port_links_get(TEST_DEV_ID, 0,
						queues, priorities);
		TEST_ASSERT(ret == 2, "(%d)Wrong link get ret=%d expected=%d",
						TEST_DEV_ID, ret, 2);
		TEST_ASSERT(queues[0] == 2, "ret=%d expected=%d", ret, 2);
		TEST_ASSERT(priorities[0] == 0x40, "ret=%d expected=%d",
							ret, 0x40);
		TEST_ASSERT(queues[1] == 3, "ret=%d expected=%d", ret, 3);
		TEST_ASSERT(priorities[1] == 0x40, "ret=%d expected=%d",
					ret, 0x40);
	}

	return TEST_SUCCESS;
}

static int
test_eventdev_profile_switch(void)
{
#define MAX_RETRIES   4
	uint8_t priorities[RTE_EVENT_MAX_QUEUES_PER_DEV];
	uint8_t queues[RTE_EVENT_MAX_QUEUES_PER_DEV];
	struct rte_event_queue_conf qcfg;
	struct rte_event_port_conf pcfg;
	struct rte_event_dev_info info;
	struct rte_event ev;
	uint8_t q, re;
	int rc;

	rte_event_dev_info_get(TEST_DEV_ID, &info);

	if (info.max_profiles_per_port <= 1)
		return TEST_SKIPPED;

	if (info.max_event_queues <= 1)
		return TEST_SKIPPED;

	rc = rte_event_port_default_conf_get(TEST_DEV_ID, 0, &pcfg);
	TEST_ASSERT_SUCCESS(rc, "Failed to get port0 default config");
	rc = rte_event_port_setup(TEST_DEV_ID, 0, &pcfg);
	TEST_ASSERT_SUCCESS(rc, "Failed to setup port0");

	rc = rte_event_queue_default_conf_get(TEST_DEV_ID, 0, &qcfg);
	TEST_ASSERT_SUCCESS(rc, "Failed to get queue0 default config");
	rc = rte_event_queue_setup(TEST_DEV_ID, 0, &qcfg);
	TEST_ASSERT_SUCCESS(rc, "Failed to setup queue0");

	q = 0;
	rc = rte_event_port_profile_links_set(TEST_DEV_ID, 0, &q, NULL, 1, 0);
	TEST_ASSERT(rc == 1, "Failed to link queue 0 to port 0 with profile 0");
	q = 1;
	rc = rte_event_port_profile_links_set(TEST_DEV_ID, 0, &q, NULL, 1, 1);
	TEST_ASSERT(rc == 1, "Failed to link queue 1 to port 0 with profile 1");

	rc = rte_event_port_profile_links_get(TEST_DEV_ID, 0, queues, priorities, 0);
	TEST_ASSERT(rc == 1, "Failed to links");
	TEST_ASSERT(queues[0] == 0, "Invalid queue found in link");

	rc = rte_event_port_profile_links_get(TEST_DEV_ID, 0, queues, priorities, 1);
	TEST_ASSERT(rc == 1, "Failed to links");
	TEST_ASSERT(queues[0] == 1, "Invalid queue found in link");

	rc = rte_event_dev_start(TEST_DEV_ID);
	TEST_ASSERT_SUCCESS(rc, "Failed to start event device");

	ev.event_type = RTE_EVENT_TYPE_CPU;
	ev.queue_id = 0;
	ev.op = RTE_EVENT_OP_NEW;
	ev.flow_id = 0;
	ev.u64 = 0xBADF00D0;
	rc = rte_event_enqueue_burst(TEST_DEV_ID, 0, &ev, 1);
	TEST_ASSERT(rc == 1, "Failed to enqueue event");
	ev.queue_id = 1;
	ev.flow_id = 1;
	rc = rte_event_enqueue_burst(TEST_DEV_ID, 0, &ev, 1);
	TEST_ASSERT(rc == 1, "Failed to enqueue event");

	ev.event = 0;
	ev.u64 = 0;

	rc = rte_event_port_profile_switch(TEST_DEV_ID, 0, 1);
	TEST_ASSERT_SUCCESS(rc, "Failed to change profile");

	re = MAX_RETRIES;
	while (re--) {
		rc = rte_event_dequeue_burst(TEST_DEV_ID, 0, &ev, 1, 0);
		printf("rc %d\n", rc);
		if (rc)
			break;
	}

	TEST_ASSERT(rc == 1, "Failed to dequeue event from profile 1");
	TEST_ASSERT(ev.flow_id == 1, "Incorrect flow identifier from profile 1");
	TEST_ASSERT(ev.queue_id == 1, "Incorrect queue identifier from profile 1");

	re = MAX_RETRIES;
	while (re--) {
		rc = rte_event_dequeue_burst(TEST_DEV_ID, 0, &ev, 1, 0);
		TEST_ASSERT(rc == 0, "Unexpected event dequeued from active profile");
	}

	rc = rte_event_port_profile_switch(TEST_DEV_ID, 0, 0);
	TEST_ASSERT_SUCCESS(rc, "Failed to change profile");

	re = MAX_RETRIES;
	while (re--) {
		rc = rte_event_dequeue_burst(TEST_DEV_ID, 0, &ev, 1, 0);
		if (rc)
			break;
	}

	TEST_ASSERT(rc == 1, "Failed to dequeue event from profile 1");
	TEST_ASSERT(ev.flow_id == 0, "Incorrect flow identifier from profile 0");
	TEST_ASSERT(ev.queue_id == 0, "Incorrect queue identifier from profile 0");

	re = MAX_RETRIES;
	while (re--) {
		rc = rte_event_dequeue_burst(TEST_DEV_ID, 0, &ev, 1, 0);
		TEST_ASSERT(rc == 0, "Unexpected event dequeued from active profile");
	}

	q = 0;
	rc = rte_event_port_profile_unlink(TEST_DEV_ID, 0, &q, 1, 0);
	TEST_ASSERT(rc == 1, "Failed to unlink queue 0 to port 0 with profile 0");
	q = 1;
	rc = rte_event_port_profile_unlink(TEST_DEV_ID, 0, &q, 1, 1);
	TEST_ASSERT(rc == 1, "Failed to unlink queue 1 to port 0 with profile 1");

	return TEST_SUCCESS;
}

static int
test_eventdev_close(void)
{
	rte_event_dev_stop(TEST_DEV_ID);
	return rte_event_dev_close(TEST_DEV_ID);
}

static struct unit_test_suite eventdev_common_testsuite  = {
	.suite_name = "eventdev common code unit test suite",
	.setup = testsuite_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {
		TEST_CASE_ST(NULL, NULL,
			test_eventdev_count),
		TEST_CASE_ST(NULL, NULL,
			test_eventdev_get_dev_id),
		TEST_CASE_ST(NULL, NULL,
			test_eventdev_socket_id),
		TEST_CASE_ST(NULL, NULL,
			test_eventdev_info_get),
		TEST_CASE_ST(NULL, NULL,
			test_eventdev_configure),
		TEST_CASE_ST(eventdev_configure_setup, NULL,
			test_eventdev_queue_default_conf_get),
		TEST_CASE_ST(eventdev_configure_setup, NULL,
			test_eventdev_queue_setup),
		TEST_CASE_ST(eventdev_configure_setup, NULL,
			test_eventdev_queue_count),
		TEST_CASE_ST(eventdev_configure_setup, NULL,
			test_eventdev_queue_attr_priority),
		TEST_CASE_ST(eventdev_configure_setup, eventdev_stop_device,
			test_eventdev_queue_attr_priority_runtime),
		TEST_CASE_ST(eventdev_configure_setup, NULL,
			test_eventdev_queue_attr_weight_runtime),
		TEST_CASE_ST(eventdev_configure_setup, NULL,
			test_eventdev_queue_attr_affinity_runtime),
		TEST_CASE_ST(eventdev_configure_setup, NULL,
			test_eventdev_queue_attr_nb_atomic_flows),
		TEST_CASE_ST(eventdev_configure_setup, NULL,
			test_eventdev_queue_attr_nb_atomic_order_sequences),
		TEST_CASE_ST(eventdev_configure_setup, NULL,
			test_eventdev_queue_attr_event_queue_cfg),
		TEST_CASE_ST(eventdev_configure_setup, NULL,
			test_eventdev_port_default_conf_get),
		TEST_CASE_ST(eventdev_configure_setup, NULL,
			test_eventdev_port_setup),
		TEST_CASE_ST(eventdev_configure_setup, NULL,
			test_eventdev_port_attr_dequeue_depth),
		TEST_CASE_ST(eventdev_configure_setup, NULL,
			test_eventdev_port_attr_enqueue_depth),
		TEST_CASE_ST(eventdev_configure_setup, NULL,
			test_eventdev_port_attr_new_event_threshold),
		TEST_CASE_ST(eventdev_configure_setup, NULL,
			test_eventdev_port_count),
		TEST_CASE_ST(eventdev_configure_setup, NULL,
			test_eventdev_timeout_ticks),
		TEST_CASE_ST(NULL, NULL,
			test_eventdev_start_stop),
		TEST_CASE_ST(eventdev_configure_setup, eventdev_stop_device,
			test_eventdev_profile_switch),
		TEST_CASE_ST(eventdev_setup_device, eventdev_stop_device,
			test_eventdev_link),
		TEST_CASE_ST(eventdev_setup_device, eventdev_stop_device,
			test_eventdev_unlink),
		TEST_CASE_ST(eventdev_setup_device, eventdev_stop_device,
			test_eventdev_link_get),
		TEST_CASE_ST(eventdev_setup_device, NULL,
			test_eventdev_close),
		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static int
test_eventdev_common(void)
{
	return unit_test_suite_runner(&eventdev_common_testsuite);
}

static int
test_eventdev_selftest_impl(const char *pmd, const char *opts)
{
	int ret = 0;

	if (rte_event_dev_get_dev_id(pmd) == -ENODEV)
		ret = rte_vdev_init(pmd, opts);
	if (ret)
		return TEST_SKIPPED;

	return rte_event_dev_selftest(rte_event_dev_get_dev_id(pmd));
}

static int
test_eventdev_selftest_sw(void)
{
	return test_eventdev_selftest_impl("event_sw", "");
}

static int
test_eventdev_selftest_octeontx(void)
{
	return test_eventdev_selftest_impl("event_octeontx", "");
}

static int
test_eventdev_selftest_dpaa2(void)
{
	return test_eventdev_selftest_impl("event_dpaa2", "");
}

static int
test_eventdev_selftest_dlb2(void)
{
	return test_eventdev_selftest_impl("dlb2_event", "");
}

static int
test_eventdev_selftest_cn9k(void)
{
	return test_eventdev_selftest_impl("event_cn9k", "");
}

static int
test_eventdev_selftest_cn10k(void)
{
	return test_eventdev_selftest_impl("event_cn10k", "");
}

#endif /* !RTE_EXEC_ENV_WINDOWS */

REGISTER_FAST_TEST(eventdev_common_autotest, true, true, test_eventdev_common);

#ifndef RTE_EXEC_ENV_WINDOWS
REGISTER_FAST_TEST(eventdev_selftest_sw, true, true, test_eventdev_selftest_sw);
REGISTER_DRIVER_TEST(eventdev_selftest_octeontx, test_eventdev_selftest_octeontx);
REGISTER_DRIVER_TEST(eventdev_selftest_dpaa2, test_eventdev_selftest_dpaa2);
REGISTER_DRIVER_TEST(eventdev_selftest_dlb2, test_eventdev_selftest_dlb2);
REGISTER_DRIVER_TEST(eventdev_selftest_cn9k, test_eventdev_selftest_cn9k);
REGISTER_DRIVER_TEST(eventdev_selftest_cn10k, test_eventdev_selftest_cn10k);

#endif /* !RTE_EXEC_ENV_WINDOWS */
