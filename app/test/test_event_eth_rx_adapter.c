/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */
#include <string.h>
#include <rte_common.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_eventdev.h>
#include <rte_bus_vdev.h>

#include <rte_event_eth_rx_adapter.h>

#include "test.h"

#define MAX_NUM_RX_QUEUE	64
#define NB_MBUFS		(8192 * num_ports * MAX_NUM_RX_QUEUE)
#define MBUF_CACHE_SIZE		512
#define MBUF_PRIV_SIZE		0
#define TEST_INST_ID		0
#define TEST_DEV_ID		0
#define TEST_ETHDEV_ID		0

struct event_eth_rx_adapter_test_params {
	struct rte_mempool *mp;
	uint16_t rx_rings, tx_rings;
	uint32_t caps;
	int rx_intr_port_inited;
	uint16_t rx_intr_port;
};

static struct event_eth_rx_adapter_test_params default_params;

static inline int
port_init_common(uint16_t port, const struct rte_eth_conf *port_conf,
		struct rte_mempool *mp)
{
	const uint16_t rx_ring_size = 512, tx_ring_size = 512;
	int retval;
	uint16_t q;
	struct rte_eth_dev_info dev_info;

	if (!rte_eth_dev_is_valid_port(port))
		return -1;

	retval = rte_eth_dev_configure(port, 0, 0, port_conf);

	retval = rte_eth_dev_info_get(port, &dev_info);
	if (retval != 0)
		return retval;

	default_params.rx_rings = RTE_MIN(dev_info.max_rx_queues,
					MAX_NUM_RX_QUEUE);
	default_params.tx_rings = 1;

	/* Configure the Ethernet device. */
	retval = rte_eth_dev_configure(port, default_params.rx_rings,
				default_params.tx_rings, port_conf);
	if (retval != 0)
		return retval;

	for (q = 0; q < default_params.rx_rings; q++) {
		retval = rte_eth_rx_queue_setup(port, q, rx_ring_size,
				rte_eth_dev_socket_id(port), NULL, mp);
		if (retval < 0)
			return retval;
	}

	/* Allocate and set up 1 TX queue per Ethernet port. */
	for (q = 0; q < default_params.tx_rings; q++) {
		retval = rte_eth_tx_queue_setup(port, q, tx_ring_size,
				rte_eth_dev_socket_id(port), NULL);
		if (retval < 0)
			return retval;
	}

	/* Start the Ethernet port. */
	retval = rte_eth_dev_start(port);
	if (retval < 0)
		return retval;

	/* Display the port MAC address. */
	struct rte_ether_addr addr;
	retval = rte_eth_macaddr_get(port, &addr);
	if (retval < 0)
		return retval;
	printf("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
			   " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
			(unsigned int)port,
			addr.addr_bytes[0], addr.addr_bytes[1],
			addr.addr_bytes[2], addr.addr_bytes[3],
			addr.addr_bytes[4], addr.addr_bytes[5]);

	/* Enable RX in promiscuous mode for the Ethernet device. */
	retval = rte_eth_promiscuous_enable(port);
	if (retval != 0)
		return retval;

	return 0;
}

static inline int
port_init_rx_intr(uint16_t port, struct rte_mempool *mp)
{
	static const struct rte_eth_conf port_conf_default = {
		.rxmode = {
			.mq_mode = ETH_MQ_RX_NONE,
		},
		.intr_conf = {
			.rxq = 1,
		},
	};

	return port_init_common(port, &port_conf_default, mp);
}

static inline int
port_init(uint16_t port, struct rte_mempool *mp)
{
	static const struct rte_eth_conf port_conf_default = {
		.rxmode = {
			.mq_mode = ETH_MQ_RX_NONE,
		},
	};

	return port_init_common(port, &port_conf_default, mp);
}

static int
init_port_rx_intr(int num_ports)
{
	int retval;
	uint16_t portid;
	int err;

	default_params.mp = rte_pktmbuf_pool_create("packet_pool",
						   NB_MBUFS,
						   MBUF_CACHE_SIZE,
						   MBUF_PRIV_SIZE,
						   RTE_MBUF_DEFAULT_BUF_SIZE,
						   rte_socket_id());
	if (!default_params.mp)
		return -ENOMEM;

	RTE_ETH_FOREACH_DEV(portid) {
		retval = port_init_rx_intr(portid, default_params.mp);
		if (retval)
			continue;
		err = rte_event_eth_rx_adapter_caps_get(TEST_DEV_ID, portid,
							&default_params.caps);
		if (err)
			continue;
		if (!(default_params.caps &
			RTE_EVENT_ETH_RX_ADAPTER_CAP_INTERNAL_PORT)) {
			default_params.rx_intr_port_inited = 1;
			default_params.rx_intr_port = portid;
			return 0;
		}
		rte_eth_dev_stop(portid);
	}
	return 0;
}

static int
init_ports(int num_ports)
{
	uint16_t portid;
	int retval;

	struct rte_mempool *ptr = rte_mempool_lookup("packet_pool");

	if (ptr == NULL)
		default_params.mp = rte_pktmbuf_pool_create("packet_pool",
						NB_MBUFS,
						MBUF_CACHE_SIZE,
						MBUF_PRIV_SIZE,
						RTE_MBUF_DEFAULT_BUF_SIZE,
						rte_socket_id());
	else
		default_params.mp = ptr;

	if (!default_params.mp)
		return -ENOMEM;

	RTE_ETH_FOREACH_DEV(portid) {
		retval = port_init(portid, default_params.mp);
		if (retval)
			return retval;
	}

	return 0;
}

static int
testsuite_setup(void)
{
	int err;
	uint8_t count;
	struct rte_event_dev_info dev_info;

	count = rte_event_dev_count();
	if (!count) {
		printf("Failed to find a valid event device,"
			" testing with event_skeleton device\n");
		rte_vdev_init("event_skeleton", NULL);
	}

	struct rte_event_dev_config config = {
			.nb_event_queues = 1,
			.nb_event_ports = 1,
	};

	err = rte_event_dev_info_get(TEST_DEV_ID, &dev_info);
	config.nb_event_queue_flows = dev_info.max_event_queue_flows;
	config.nb_event_port_dequeue_depth =
			dev_info.max_event_port_dequeue_depth;
	config.nb_event_port_enqueue_depth =
			dev_info.max_event_port_enqueue_depth;
	config.nb_events_limit =
			dev_info.max_num_events;
	err = rte_event_dev_configure(TEST_DEV_ID, &config);
	TEST_ASSERT(err == 0, "Event device initialization failed err %d\n",
			err);

	/*
	 * eth devices like octeontx use event device to receive packets
	 * so rte_eth_dev_start invokes rte_event_dev_start internally, so
	 * call init_ports after rte_event_dev_configure
	 */
	err = init_ports(rte_eth_dev_count_total());
	TEST_ASSERT(err == 0, "Port initialization failed err %d\n", err);

	err = rte_event_eth_rx_adapter_caps_get(TEST_DEV_ID, TEST_ETHDEV_ID,
						&default_params.caps);
	TEST_ASSERT(err == 0, "Failed to get adapter cap err %d\n",
			err);

	return err;
}

static int
testsuite_setup_rx_intr(void)
{
	int err;
	uint8_t count;
	struct rte_event_dev_info dev_info;

	count = rte_event_dev_count();
	if (!count) {
		printf("Failed to find a valid event device,"
			" testing with event_skeleton device\n");
		rte_vdev_init("event_skeleton", NULL);
	}

	struct rte_event_dev_config config = {
		.nb_event_queues = 1,
		.nb_event_ports = 1,
	};

	err = rte_event_dev_info_get(TEST_DEV_ID, &dev_info);
	config.nb_event_queue_flows = dev_info.max_event_queue_flows;
	config.nb_event_port_dequeue_depth =
			dev_info.max_event_port_dequeue_depth;
	config.nb_event_port_enqueue_depth =
			dev_info.max_event_port_enqueue_depth;
	config.nb_events_limit =
			dev_info.max_num_events;

	err = rte_event_dev_configure(TEST_DEV_ID, &config);
	TEST_ASSERT(err == 0, "Event device initialization failed err %d\n",
			err);

	/*
	 * eth devices like octeontx use event device to receive packets
	 * so rte_eth_dev_start invokes rte_event_dev_start internally, so
	 * call init_ports after rte_event_dev_configure
	 */
	err = init_port_rx_intr(rte_eth_dev_count_total());
	TEST_ASSERT(err == 0, "Port initialization failed err %d\n", err);

	if (!default_params.rx_intr_port_inited)
		return 0;

	err = rte_event_eth_rx_adapter_caps_get(TEST_DEV_ID,
						default_params.rx_intr_port,
						&default_params.caps);
	TEST_ASSERT(err == 0, "Failed to get adapter cap err %d\n", err);

	return err;
}

static void
testsuite_teardown(void)
{
	uint32_t i;
	RTE_ETH_FOREACH_DEV(i)
		rte_eth_dev_stop(i);

	rte_mempool_free(default_params.mp);
}

static void
testsuite_teardown_rx_intr(void)
{
	if (!default_params.rx_intr_port_inited)
		return;

	rte_eth_dev_stop(default_params.rx_intr_port);
	rte_mempool_free(default_params.mp);
}

static int
adapter_create(void)
{
	int err;
	struct rte_event_dev_info dev_info;
	struct rte_event_port_conf rx_p_conf;

	memset(&rx_p_conf, 0, sizeof(rx_p_conf));

	err = rte_event_dev_info_get(TEST_DEV_ID, &dev_info);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	rx_p_conf.new_event_threshold = dev_info.max_num_events;
	rx_p_conf.dequeue_depth = dev_info.max_event_port_dequeue_depth;
	rx_p_conf.enqueue_depth = dev_info.max_event_port_enqueue_depth;
	err = rte_event_eth_rx_adapter_create(TEST_INST_ID, TEST_DEV_ID,
					&rx_p_conf);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	return err;
}

static void
adapter_free(void)
{
	rte_event_eth_rx_adapter_free(TEST_INST_ID);
}

static int
adapter_create_free(void)
{
	int err;

	struct rte_event_port_conf rx_p_conf = {
			.dequeue_depth = 8,
			.enqueue_depth = 8,
			.new_event_threshold = 1200,
	};

	err = rte_event_eth_rx_adapter_create(TEST_INST_ID, TEST_DEV_ID,
					NULL);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	err = rte_event_eth_rx_adapter_create(TEST_INST_ID, TEST_DEV_ID,
					&rx_p_conf);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_rx_adapter_create(TEST_INST_ID,
					TEST_DEV_ID, &rx_p_conf);
	TEST_ASSERT(err == -EEXIST, "Expected -EEXIST %d got %d", -EEXIST, err);

	err = rte_event_eth_rx_adapter_free(TEST_INST_ID);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_rx_adapter_free(TEST_INST_ID);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL %d got %d", -EINVAL, err);

	err = rte_event_eth_rx_adapter_free(1);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL %d got %d", -EINVAL, err);

	return TEST_SUCCESS;
}

static int
adapter_queue_add_del(void)
{
	int err;
	struct rte_event ev;
	uint32_t cap;

	struct rte_event_eth_rx_adapter_queue_conf queue_config;

	err = rte_event_eth_rx_adapter_caps_get(TEST_DEV_ID, TEST_ETHDEV_ID,
					 &cap);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	ev.queue_id = 0;
	ev.sched_type = RTE_SCHED_TYPE_ATOMIC;
	ev.priority = 0;

	queue_config.rx_queue_flags = 0;
	if (cap & RTE_EVENT_ETH_RX_ADAPTER_CAP_OVERRIDE_FLOW_ID) {
		ev.flow_id = 1;
		queue_config.rx_queue_flags =
			RTE_EVENT_ETH_RX_ADAPTER_QUEUE_FLOW_ID_VALID;
	}
	queue_config.ev = ev;
	queue_config.servicing_weight = 1;

	err = rte_event_eth_rx_adapter_queue_add(TEST_INST_ID,
						rte_eth_dev_count_total(),
						-1, &queue_config);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	if (cap & RTE_EVENT_ETH_RX_ADAPTER_CAP_MULTI_EVENTQ) {
		err = rte_event_eth_rx_adapter_queue_add(TEST_INST_ID,
							TEST_ETHDEV_ID, 0,
							&queue_config);
		TEST_ASSERT(err == 0, "Expected 0 got %d", err);

		err = rte_event_eth_rx_adapter_queue_del(TEST_INST_ID,
							TEST_ETHDEV_ID, 0);
		TEST_ASSERT(err == 0, "Expected 0 got %d", err);

		err = rte_event_eth_rx_adapter_queue_add(TEST_INST_ID,
							TEST_ETHDEV_ID,
							-1,
							&queue_config);
		TEST_ASSERT(err == 0, "Expected 0 got %d", err);

		err = rte_event_eth_rx_adapter_queue_del(TEST_INST_ID,
							TEST_ETHDEV_ID,
							-1);
		TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	} else {
		err = rte_event_eth_rx_adapter_queue_add(TEST_INST_ID,
							TEST_ETHDEV_ID,
							0,
							&queue_config);
		TEST_ASSERT(err == -EINVAL, "Expected EINVAL got %d", err);

		err = rte_event_eth_rx_adapter_queue_add(TEST_INST_ID,
							TEST_ETHDEV_ID, -1,
							&queue_config);
		TEST_ASSERT(err == 0, "Expected 0 got %d", err);

		err = rte_event_eth_rx_adapter_queue_del(TEST_INST_ID,
							TEST_ETHDEV_ID, 0);
		TEST_ASSERT(err == 0, "Expected 0 got %d", err);

		err = rte_event_eth_rx_adapter_queue_del(TEST_INST_ID,
							TEST_ETHDEV_ID, -1);
		TEST_ASSERT(err == 0, "Expected 0 got %d", err);

		err = rte_event_eth_rx_adapter_queue_del(TEST_INST_ID,
							TEST_ETHDEV_ID, -1);
		TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	}

	err = rte_event_eth_rx_adapter_queue_add(1, TEST_ETHDEV_ID, -1,
						&queue_config);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	err = rte_event_eth_rx_adapter_queue_del(1, TEST_ETHDEV_ID, -1);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	return TEST_SUCCESS;
}

static int
adapter_multi_eth_add_del(void)
{
	int err;
	struct rte_event ev;

	uint16_t port_index, drv_id = 0;
	char driver_name[50];

	struct rte_event_eth_rx_adapter_queue_conf queue_config;

	ev.queue_id = 0;
	ev.sched_type = RTE_SCHED_TYPE_ATOMIC;
	ev.priority = 0;

	queue_config.rx_queue_flags = 0;
	queue_config.ev = ev;
	queue_config.servicing_weight = 1;

	/* stop eth devices for existing */
	port_index = 0;
	for (; port_index < rte_eth_dev_count_total(); port_index += 1)
		rte_eth_dev_stop(port_index);

	/* add the max port for rx_adapter */
	port_index = rte_eth_dev_count_total();
	for (; port_index < RTE_MAX_ETHPORTS; port_index += 1) {
		snprintf(driver_name, sizeof(driver_name), "%s%u", "net_null",
				drv_id);
		err = rte_vdev_init(driver_name, NULL);
		TEST_ASSERT(err == 0, "Failed driver %s got %d",
		driver_name, err);
		drv_id += 1;
	}

	err = init_ports(rte_eth_dev_count_total());
	TEST_ASSERT(err == 0, "Port initialization failed err %d\n", err);

	/* eth_rx_adapter_queue_add for n ports */
	port_index = 0;
	for (; port_index < rte_eth_dev_count_total(); port_index += 1) {
		err = rte_event_eth_rx_adapter_queue_add(TEST_INST_ID,
				port_index, -1,
				&queue_config);
		TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	}

	/* eth_rx_adapter_queue_del n ports */
	port_index = 0;
	for (; port_index < rte_eth_dev_count_total(); port_index += 1) {
		err = rte_event_eth_rx_adapter_queue_del(TEST_INST_ID,
				port_index, -1);
		TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	}

	return TEST_SUCCESS;
}

static int
adapter_intr_queue_add_del(void)
{
	int err;
	struct rte_event ev;
	uint32_t cap;
	uint16_t eth_port;
	struct rte_event_eth_rx_adapter_queue_conf queue_config;

	if (!default_params.rx_intr_port_inited)
		return 0;

	eth_port = default_params.rx_intr_port;
	err = rte_event_eth_rx_adapter_caps_get(TEST_DEV_ID, eth_port, &cap);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	ev.queue_id = 0;
	ev.sched_type = RTE_SCHED_TYPE_ATOMIC;
	ev.priority = 0;

	queue_config.rx_queue_flags = 0;
	queue_config.ev = ev;

	/* weight = 0 => interrupt mode */
	queue_config.servicing_weight = 0;

	if (cap & RTE_EVENT_ETH_RX_ADAPTER_CAP_MULTI_EVENTQ) {
		/* add queue 0 */
		err = rte_event_eth_rx_adapter_queue_add(TEST_INST_ID,
							TEST_ETHDEV_ID, 0,
							&queue_config);
		TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	}

	/* add all queues */
	queue_config.servicing_weight = 0;
	err = rte_event_eth_rx_adapter_queue_add(TEST_INST_ID,
						TEST_ETHDEV_ID,
						-1,
						&queue_config);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	if (cap & RTE_EVENT_ETH_RX_ADAPTER_CAP_MULTI_EVENTQ) {
		/* del queue 0 */
		err = rte_event_eth_rx_adapter_queue_del(TEST_INST_ID,
							TEST_ETHDEV_ID,
							0);
		TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	}

	/* del remaining queues */
	err = rte_event_eth_rx_adapter_queue_del(TEST_INST_ID,
						TEST_ETHDEV_ID,
						-1);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	/* add all queues */
	queue_config.servicing_weight = 0;
	err = rte_event_eth_rx_adapter_queue_add(TEST_INST_ID,
						TEST_ETHDEV_ID,
						-1,
						&queue_config);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	/* intr -> poll mode queue */
	queue_config.servicing_weight = 1;

	if (cap & RTE_EVENT_ETH_RX_ADAPTER_CAP_MULTI_EVENTQ) {
		err = rte_event_eth_rx_adapter_queue_add(TEST_INST_ID,
							TEST_ETHDEV_ID,
							0,
							&queue_config);
		TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	}

	err = rte_event_eth_rx_adapter_queue_add(TEST_INST_ID,
						TEST_ETHDEV_ID,
						-1,
						 &queue_config);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	/* del queues */
	err = rte_event_eth_rx_adapter_queue_del(TEST_INST_ID,
						TEST_ETHDEV_ID,
						-1);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	return TEST_SUCCESS;
}

static int
adapter_start_stop(void)
{
	int err;
	struct rte_event ev;

	ev.queue_id = 0;
	ev.sched_type = RTE_SCHED_TYPE_ATOMIC;
	ev.priority = 0;

	struct rte_event_eth_rx_adapter_queue_conf queue_config;

	queue_config.rx_queue_flags = 0;
	if (default_params.caps &
		RTE_EVENT_ETH_RX_ADAPTER_CAP_OVERRIDE_FLOW_ID) {
		ev.flow_id = 1;
		queue_config.rx_queue_flags =
			RTE_EVENT_ETH_RX_ADAPTER_QUEUE_FLOW_ID_VALID;
	}

	queue_config.ev = ev;
	queue_config.servicing_weight = 1;

	err = rte_event_eth_rx_adapter_queue_add(TEST_INST_ID, TEST_ETHDEV_ID,
					-1, &queue_config);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_rx_adapter_start(TEST_INST_ID);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_rx_adapter_stop(TEST_INST_ID);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_rx_adapter_queue_del(TEST_INST_ID, TEST_ETHDEV_ID,
						-1);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_rx_adapter_start(TEST_INST_ID);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_rx_adapter_stop(TEST_INST_ID);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_rx_adapter_start(1);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	err = rte_event_eth_rx_adapter_stop(1);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	return TEST_SUCCESS;
}

static int
adapter_stats(void)
{
	int err;
	struct rte_event_eth_rx_adapter_stats stats;

	err = rte_event_eth_rx_adapter_stats_get(TEST_INST_ID, NULL);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	err = rte_event_eth_rx_adapter_stats_get(TEST_INST_ID, &stats);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_rx_adapter_stats_get(1, &stats);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	return TEST_SUCCESS;
}

static struct unit_test_suite event_eth_rx_tests = {
	.suite_name = "rx event eth adapter test suite",
	.setup = testsuite_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {
		TEST_CASE_ST(NULL, NULL, adapter_create_free),
		TEST_CASE_ST(adapter_create, adapter_free,
					adapter_queue_add_del),
		TEST_CASE_ST(adapter_create, adapter_free,
					adapter_multi_eth_add_del),
		TEST_CASE_ST(adapter_create, adapter_free, adapter_start_stop),
		TEST_CASE_ST(adapter_create, adapter_free, adapter_stats),
		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static struct unit_test_suite event_eth_rx_intr_tests = {
	.suite_name = "rx event eth adapter test suite",
	.setup = testsuite_setup_rx_intr,
	.teardown = testsuite_teardown_rx_intr,
	.unit_test_cases = {
		TEST_CASE_ST(adapter_create, adapter_free,
			adapter_intr_queue_add_del),
		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static int
test_event_eth_rx_adapter_common(void)
{
	return unit_test_suite_runner(&event_eth_rx_tests);
}

static int
test_event_eth_rx_intr_adapter_common(void)
{
	return unit_test_suite_runner(&event_eth_rx_intr_tests);
}

REGISTER_TEST_COMMAND(event_eth_rx_adapter_autotest,
		test_event_eth_rx_adapter_common);
REGISTER_TEST_COMMAND(event_eth_rx_intr_adapter_autotest,
		test_event_eth_rx_intr_adapter_common);
