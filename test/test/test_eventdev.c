/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016 Cavium networks. All rights reserved.
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

#include <rte_common.h>
#include <rte_hexdump.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_eventdev.h>
#include <rte_dev.h>

#include "test.h"

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
		return rte_eal_vdev_init("event_skeleton", NULL);
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
min_dequeue_limit(struct rte_event_dev_config *dev_conf,
		  struct rte_event_dev_info *info)
{
	dev_conf->dequeue_timeout_ns = info->min_dequeue_timeout_ns - 1;
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
		test_ethdev_config_run(&dev_conf, &info, min_dequeue_limit),
		 "Config negative test failed");
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
	TEST_ASSERT_EQUAL(-EINVAL,
		test_ethdev_config_run(&dev_conf, &info,
			max_event_port_dequeue_depth),
			 "Config negative test failed");
	TEST_ASSERT_EQUAL(-EINVAL,
		test_ethdev_config_run(&dev_conf, &info,
		max_event_port_enqueue_depth),
		 "Config negative test failed");

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

	for (i = 0; i < rte_event_queue_count(TEST_DEV_ID); i++) {
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
	qconf.event_queue_cfg =	(RTE_EVENT_QUEUE_CFG_ALL_TYPES &
		 RTE_EVENT_QUEUE_CFG_TYPE_MASK);
	qconf.nb_atomic_flows = info.max_event_queue_flows + 1;
	ret = rte_event_queue_setup(TEST_DEV_ID, 0, &qconf);
	TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);

	qconf.nb_atomic_flows = info.max_event_queue_flows;
	qconf.event_queue_cfg =	(RTE_EVENT_QUEUE_CFG_ORDERED_ONLY &
		 RTE_EVENT_QUEUE_CFG_TYPE_MASK);
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


	for (i = 0; i < rte_event_queue_count(TEST_DEV_ID); i++) {
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

	TEST_ASSERT_EQUAL(rte_event_queue_count(TEST_DEV_ID),
		 info.max_event_queues, "Wrong queue count");

	return TEST_SUCCESS;
}

static int
test_eventdev_queue_priority(void)
{
	int i, ret;
	struct rte_event_dev_info info;
	struct rte_event_queue_conf qconf;
	uint8_t priority;

	ret = rte_event_dev_info_get(TEST_DEV_ID, &info);
	TEST_ASSERT_SUCCESS(ret, "Failed to get event dev info");

	for (i = 0; i < rte_event_queue_count(TEST_DEV_ID); i++) {
		ret = rte_event_queue_default_conf_get(TEST_DEV_ID, i,
					&qconf);
		TEST_ASSERT_SUCCESS(ret, "Failed to get queue%d def conf", i);
		qconf.priority = i %  RTE_EVENT_DEV_PRIORITY_LOWEST;
		ret = rte_event_queue_setup(TEST_DEV_ID, i, &qconf);
		TEST_ASSERT_SUCCESS(ret, "Failed to setup queue%d", i);
	}

	for (i = 0; i < rte_event_queue_count(TEST_DEV_ID); i++) {
		priority =  rte_event_queue_priority(TEST_DEV_ID, i);
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
test_eventdev_port_default_conf_get(void)
{
	int i, ret;
	struct rte_event_port_conf pconf;

	ret = rte_event_port_default_conf_get(TEST_DEV_ID, 0, NULL);
	TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);

	ret = rte_event_port_default_conf_get(TEST_DEV_ID,
			rte_event_port_count(TEST_DEV_ID) + 1, NULL);
	TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);

	for (i = 0; i < rte_event_port_count(TEST_DEV_ID); i++) {
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

	ret = rte_event_port_setup(TEST_DEV_ID, info.max_event_ports,
					&pconf);
	TEST_ASSERT(ret == -EINVAL, "Expected -EINVAL, %d", ret);

	/* Positive case */
	ret = rte_event_port_default_conf_get(TEST_DEV_ID, 0, &pconf);
	TEST_ASSERT_SUCCESS(ret, "Failed to get port0 info");
	ret = rte_event_port_setup(TEST_DEV_ID, 0, &pconf);
	TEST_ASSERT_SUCCESS(ret, "Failed to setup port0");


	for (i = 0; i < rte_event_port_count(TEST_DEV_ID); i++) {
		ret = rte_event_port_setup(TEST_DEV_ID, i, NULL);
		TEST_ASSERT_SUCCESS(ret, "Failed to setup port%d", i);
	}

	return TEST_SUCCESS;
}

static int
test_eventdev_dequeue_depth(void)
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

	TEST_ASSERT_EQUAL(rte_event_port_dequeue_depth(TEST_DEV_ID, 0),
		 pconf.dequeue_depth, "Wrong port dequeue depth");

	return TEST_SUCCESS;
}

static int
test_eventdev_enqueue_depth(void)
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

	TEST_ASSERT_EQUAL(rte_event_port_enqueue_depth(TEST_DEV_ID, 0),
		 pconf.enqueue_depth, "Wrong port enqueue depth");

	return TEST_SUCCESS;
}

static int
test_eventdev_port_count(void)
{
	int ret;
	struct rte_event_dev_info info;

	ret = rte_event_dev_info_get(TEST_DEV_ID, &info);
	TEST_ASSERT_SUCCESS(ret, "Failed to get event dev info");

	TEST_ASSERT_EQUAL(rte_event_port_count(TEST_DEV_ID),
		 info.max_event_ports, "Wrong port count");

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

	for (i = 0; i < rte_event_queue_count(TEST_DEV_ID); i++) {
		ret = rte_event_queue_setup(TEST_DEV_ID, i, NULL);
		TEST_ASSERT_SUCCESS(ret, "Failed to setup queue%d", i);
	}

	for (i = 0; i < rte_event_port_count(TEST_DEV_ID); i++) {
		ret = rte_event_port_setup(TEST_DEV_ID, i, NULL);
		TEST_ASSERT_SUCCESS(ret, "Failed to setup port%d", i);
	}

	ret = rte_event_port_link(TEST_DEV_ID, 0, NULL, NULL, 0);
	TEST_ASSERT(ret == rte_event_queue_count(TEST_DEV_ID),
			"Failed to link port, device %d", TEST_DEV_ID);

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

	for (i = 0; i < rte_event_queue_count(TEST_DEV_ID); i++) {
		ret = rte_event_queue_setup(TEST_DEV_ID, i, NULL);
		TEST_ASSERT_SUCCESS(ret, "Failed to setup queue%d", i);
	}

	for (i = 0; i < rte_event_port_count(TEST_DEV_ID); i++) {
		ret = rte_event_port_setup(TEST_DEV_ID, i, NULL);
		TEST_ASSERT_SUCCESS(ret, "Failed to setup port%d", i);
	}

	ret = rte_event_port_link(TEST_DEV_ID, 0, NULL, NULL, 0);
	TEST_ASSERT(ret == rte_event_queue_count(TEST_DEV_ID),
			"Failed to link port, device %d", TEST_DEV_ID);

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

	nb_queues = rte_event_queue_count(TEST_DEV_ID);
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

	nb_queues = rte_event_queue_count(TEST_DEV_ID);
	for (i = 0; i < nb_queues; i++)
		queues[i] = i;


	ret = rte_event_port_unlink(TEST_DEV_ID, 0, queues, nb_queues);
	TEST_ASSERT(ret == nb_queues, "Failed to unlink(device%d) ret=%d",
				 TEST_DEV_ID, ret);
	return TEST_SUCCESS;
}

static int
test_eventdev_link_get(void)
{
	int ret, nb_queues, i;
	uint8_t queues[RTE_EVENT_MAX_QUEUES_PER_DEV];
	uint8_t priorities[RTE_EVENT_MAX_QUEUES_PER_DEV];

	/* link all queues */
	ret = rte_event_port_link(TEST_DEV_ID, 0, NULL, NULL, 0);
	TEST_ASSERT(ret >= 0, "Failed to link with NULL device%d",
				 TEST_DEV_ID);

	nb_queues = rte_event_queue_count(TEST_DEV_ID);
	for (i = 0; i < nb_queues; i++)
		queues[i] = i;

	ret = rte_event_port_unlink(TEST_DEV_ID, 0, queues, nb_queues);
	TEST_ASSERT(ret == nb_queues, "Failed to unlink(device%d) ret=%d",
				 TEST_DEV_ID, ret);

	ret = rte_event_port_links_get(TEST_DEV_ID, 0, queues, priorities);
	TEST_ASSERT(ret == 0, "(%d)Wrong link get=%d", TEST_DEV_ID, ret);

	/* link all queues and get the links */
	nb_queues = rte_event_queue_count(TEST_DEV_ID);
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
	/* unlink all*/
	ret = rte_event_port_unlink(TEST_DEV_ID, 0, NULL, 0);
	TEST_ASSERT(ret == nb_queues, "Failed to unlink(device%d) ret=%d",
				 TEST_DEV_ID, ret);
	/* 4links and 2 unlinks */
	nb_queues = rte_event_queue_count(TEST_DEV_ID);
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
			test_eventdev_queue_priority),
		TEST_CASE_ST(eventdev_configure_setup, NULL,
			test_eventdev_port_default_conf_get),
		TEST_CASE_ST(eventdev_configure_setup, NULL,
			test_eventdev_port_setup),
		TEST_CASE_ST(eventdev_configure_setup, NULL,
			test_eventdev_dequeue_depth),
		TEST_CASE_ST(eventdev_configure_setup, NULL,
			test_eventdev_enqueue_depth),
		TEST_CASE_ST(eventdev_configure_setup, NULL,
			test_eventdev_port_count),
		TEST_CASE_ST(eventdev_configure_setup, NULL,
			test_eventdev_timeout_ticks),
		TEST_CASE_ST(NULL, NULL,
			test_eventdev_start_stop),
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

REGISTER_TEST_COMMAND(eventdev_common_autotest, test_eventdev_common);
