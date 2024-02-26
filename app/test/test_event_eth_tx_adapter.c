/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <string.h>

#include <rte_bus_vdev.h>
#include <rte_common.h>
#include <rte_ethdev.h>
#include <rte_eth_ring.h>
#include <rte_eventdev.h>
#include <rte_event_eth_tx_adapter.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_service.h>

#include "test.h"

#ifdef RTE_EXEC_ENV_WINDOWS
static int
test_event_eth_tx_adapter_common(void)
{
	printf("event_eth_tx_adapter not supported on Windows, skipping test\n");
	return TEST_SKIPPED;
}

#else

#define MAX_NUM_QUEUE		RTE_PMD_RING_MAX_RX_RINGS
#define TEST_INST_ID		0
#define TEST_DEV_ID		0
#define TEST_ETH_QUEUE_ID	0
#define SOCKET0			0
#define RING_SIZE		256
#define ETH_NAME_LEN		32
#define NUM_ETH_PAIR		1
#define NUM_ETH_DEV		(2 * NUM_ETH_PAIR)
#define NB_MBUF			512
#define PAIR_PORT_INDEX(p)	((p) + NUM_ETH_PAIR)
#define PORT(p)			default_params.port[(p)]
#define TEST_ETHDEV_ID		PORT(0)
#define TEST_ETHDEV_PAIR_ID	PORT(PAIR_PORT_INDEX(0))
#define DEFAULT_FLUSH_THRESHOLD 1024
#define TXA_NB_TX_WORK_DEFAULT  128

#define EDEV_RETRY		0xffff

struct event_eth_tx_adapter_test_params {
	struct rte_mempool *mp;
	uint16_t rx_rings, tx_rings;
	struct rte_ring *r[NUM_ETH_DEV][MAX_NUM_QUEUE];
	int port[NUM_ETH_DEV];
};

static int event_dev_delete;
static struct event_eth_tx_adapter_test_params default_params;
static uint64_t eid = ~0ULL;
static uint32_t tid;

static inline int
port_init_common(uint16_t port, const struct rte_eth_conf *port_conf,
		struct rte_mempool *mp)
{
	const uint16_t rx_ring_size = RING_SIZE, tx_ring_size = RING_SIZE;
	int retval;
	uint16_t q;

	if (!rte_eth_dev_is_valid_port(port))
		return -1;

	default_params.rx_rings = MAX_NUM_QUEUE;
	default_params.tx_rings = MAX_NUM_QUEUE;

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
			(unsigned int)port, RTE_ETHER_ADDR_BYTES(&addr));

	/* Enable RX in promiscuous mode for the Ethernet device. */
	retval = rte_eth_promiscuous_enable(port);
	if (retval != 0)
		return retval;

	return 0;
}

static inline int
port_init(uint16_t port, struct rte_mempool *mp)
{
	struct rte_eth_conf conf = { 0 };
	return port_init_common(port, &conf, mp);
}

#define RING_NAME_LEN	20
#define DEV_NAME_LEN	20

static int
init_ports(void)
{
	char ring_name[ETH_NAME_LEN];
	unsigned int i, j;
	struct rte_ring * const *c1;
	struct rte_ring * const *c2;
	int err;

	if (!default_params.mp)
		default_params.mp = rte_pktmbuf_pool_create("mbuf_pool",
			NB_MBUF, 32,
			0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

	if (!default_params.mp)
		return -ENOMEM;

	for (i = 0; i < NUM_ETH_DEV; i++) {
		for (j = 0; j < MAX_NUM_QUEUE; j++) {
			snprintf(ring_name, sizeof(ring_name), "R%u%u", i, j);
			default_params.r[i][j] = rte_ring_create(ring_name,
						RING_SIZE,
						SOCKET0,
						RING_F_SP_ENQ | RING_F_SC_DEQ);
			TEST_ASSERT((default_params.r[i][j] != NULL),
				"Failed to allocate ring");
		}
	}

	/*
	 * To create two pseudo-Ethernet ports where the traffic is
	 * switched between them, that is, traffic sent to port 1 is
	 * read back from port 2 and vice-versa
	 */
	for (i = 0; i < NUM_ETH_PAIR; i++) {
		char dev_name[DEV_NAME_LEN];
		int p;

		c1 = default_params.r[i];
		c2 = default_params.r[PAIR_PORT_INDEX(i)];

		snprintf(dev_name, DEV_NAME_LEN, "%u-%u", i, i + NUM_ETH_PAIR);
		p = rte_eth_from_rings(dev_name, c1, MAX_NUM_QUEUE,
				 c2, MAX_NUM_QUEUE, SOCKET0);
		TEST_ASSERT(p >= 0, "Port creation failed %s", dev_name);
		err = port_init(p, default_params.mp);
		TEST_ASSERT(err == 0, "Port init failed %s", dev_name);
		default_params.port[i] = p;

		snprintf(dev_name, DEV_NAME_LEN, "%u-%u",  i + NUM_ETH_PAIR, i);
		p = rte_eth_from_rings(dev_name, c2, MAX_NUM_QUEUE,
				c1, MAX_NUM_QUEUE, SOCKET0);
		TEST_ASSERT(p > 0, "Port creation failed %s", dev_name);
		err = port_init(p, default_params.mp);
		TEST_ASSERT(err == 0, "Port init failed %s", dev_name);
		default_params.port[PAIR_PORT_INDEX(i)] = p;
	}

	return 0;
}

static void
deinit_ports(void)
{
	uint16_t i, j;
	char name[ETH_NAME_LEN];

	for (i = 0; i < RTE_DIM(default_params.port); i++) {
		rte_eth_dev_stop(default_params.port[i]);
		rte_eth_dev_get_name_by_port(default_params.port[i], name);
		rte_vdev_uninit(name);
		for (j = 0; j < RTE_DIM(default_params.r[i]); j++)
			rte_ring_free(default_params.r[i][j]);
	}
}

static int
testsuite_setup(void)
{
	const char *vdev_name = "event_sw0";

	int err = init_ports();
	TEST_ASSERT(err == 0, "Port initialization failed err %d\n", err);

	if (rte_event_dev_count() == 0) {
		printf("Failed to find a valid event device,"
			" testing with event_sw0 device\n");
		err = rte_vdev_init(vdev_name, NULL);
		TEST_ASSERT(err == 0, "vdev %s creation failed  %d\n",
			vdev_name, err);
		event_dev_delete = 1;
	}
	return err;
}

#define DEVICE_ID_SIZE 64

static void
testsuite_teardown(void)
{
	deinit_ports();
	rte_mempool_free(default_params.mp);
	default_params.mp = NULL;
	if (event_dev_delete)
		rte_vdev_uninit("event_sw0");
}

static int
tx_adapter_create(void)
{
	int err;
	struct rte_event_dev_info dev_info;
	struct rte_event_port_conf tx_p_conf;
	uint8_t priority;
	uint8_t queue_id;

	struct rte_event_dev_config config = {
			.nb_event_queues = 1,
			.nb_event_ports = 1,
	};

	struct rte_event_queue_conf wkr_q_conf = {
			.schedule_type = RTE_SCHED_TYPE_ORDERED,
			.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
			.nb_atomic_flows = 1024,
			.nb_atomic_order_sequences = 1024,
	};

	memset(&tx_p_conf, 0, sizeof(tx_p_conf));
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

	queue_id = 0;
	err = rte_event_queue_setup(TEST_DEV_ID, 0, &wkr_q_conf);
	TEST_ASSERT(err == 0, "Event queue setup failed %d\n", err);

	err = rte_event_port_setup(TEST_DEV_ID, 0, NULL);
	TEST_ASSERT(err == 0, "Event port setup failed %d\n", err);

	priority = RTE_EVENT_DEV_PRIORITY_LOWEST;
	err = rte_event_port_link(TEST_DEV_ID, 0, &queue_id, &priority, 1);
	TEST_ASSERT(err == 1, "Error linking port %s\n",
		rte_strerror(rte_errno));
	err = rte_event_dev_info_get(TEST_DEV_ID, &dev_info);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	tx_p_conf.new_event_threshold = dev_info.max_num_events;
	tx_p_conf.dequeue_depth = dev_info.max_event_port_dequeue_depth;
	tx_p_conf.enqueue_depth = dev_info.max_event_port_enqueue_depth;
	err = rte_event_eth_tx_adapter_create(TEST_INST_ID, TEST_DEV_ID,
					&tx_p_conf);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	return err;
}

static void
tx_adapter_free(void)
{
	rte_event_eth_tx_adapter_free(TEST_INST_ID);
}

static int
tx_adapter_create_free(void)
{
	int err;
	struct rte_event_dev_info dev_info;
	struct rte_event_port_conf tx_p_conf;

	err = rte_event_dev_info_get(TEST_DEV_ID, &dev_info);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	tx_p_conf.new_event_threshold = dev_info.max_num_events;
	tx_p_conf.dequeue_depth = dev_info.max_event_port_dequeue_depth;
	tx_p_conf.enqueue_depth = dev_info.max_event_port_enqueue_depth;

	err = rte_event_eth_tx_adapter_create(TEST_INST_ID, TEST_DEV_ID,
					NULL);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	err = rte_event_eth_tx_adapter_create(TEST_INST_ID, TEST_DEV_ID,
					&tx_p_conf);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_tx_adapter_create(TEST_INST_ID,
					TEST_DEV_ID, &tx_p_conf);
	TEST_ASSERT(err == -EEXIST, "Expected -EEXIST %d got %d", -EEXIST, err);

	err = rte_event_eth_tx_adapter_free(TEST_INST_ID);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_tx_adapter_free(TEST_INST_ID);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL %d got %d", -EINVAL, err);

	err = rte_event_eth_tx_adapter_free(1);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL %d got %d", -EINVAL, err);

	return TEST_SUCCESS;
}

static int
tx_adapter_queue_add_del(void)
{
	int err;
	uint32_t cap;

	err = rte_event_eth_tx_adapter_caps_get(TEST_DEV_ID, TEST_ETHDEV_ID,
					 &cap);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);


	err = rte_event_eth_tx_adapter_queue_add(TEST_INST_ID,
						rte_eth_dev_count_total(),
						-1);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	err = rte_event_eth_tx_adapter_queue_add(TEST_INST_ID,
						TEST_ETHDEV_ID,
						0);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_tx_adapter_queue_add(TEST_INST_ID,
						TEST_ETHDEV_ID,
						-1);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_tx_adapter_queue_del(TEST_INST_ID,
						TEST_ETHDEV_ID,
						0);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_tx_adapter_queue_del(TEST_INST_ID,
						TEST_ETHDEV_ID,
						-1);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_tx_adapter_queue_del(TEST_INST_ID,
						TEST_ETHDEV_ID,
						-1);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_tx_adapter_queue_add(1, TEST_ETHDEV_ID, -1);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	err = rte_event_eth_tx_adapter_queue_del(1, TEST_ETHDEV_ID, -1);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	return TEST_SUCCESS;
}

static int
tx_adapter_start_stop(void)
{
	int err;

	err = rte_event_eth_tx_adapter_queue_add(TEST_INST_ID, TEST_ETHDEV_ID,
						-1);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_tx_adapter_start(TEST_INST_ID);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_tx_adapter_stop(TEST_INST_ID);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_tx_adapter_queue_del(TEST_INST_ID, TEST_ETHDEV_ID,
						-1);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_tx_adapter_start(TEST_INST_ID);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_tx_adapter_stop(TEST_INST_ID);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_tx_adapter_start(1);

	err = rte_event_eth_tx_adapter_stop(1);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	return TEST_SUCCESS;
}


static int
tx_adapter_single(uint16_t port, uint16_t tx_queue_id,
		struct rte_mbuf *m, uint8_t qid,
		uint8_t sched_type)
{
	struct rte_event event;
	struct rte_mbuf *r;
	int ret;
	unsigned int l;

	event.queue_id = qid;
	event.op = RTE_EVENT_OP_NEW;
	event.event_type = RTE_EVENT_TYPE_CPU;
	event.sched_type = sched_type;
	event.mbuf = m;

	m->port = port;
	rte_event_eth_tx_adapter_txq_set(m, tx_queue_id);

	l = 0;
	while (rte_event_enqueue_burst(TEST_DEV_ID, 0, &event, 1) != 1) {
		l++;
		if (l > EDEV_RETRY)
			break;
	}

	TEST_ASSERT(l < EDEV_RETRY, "Unable to enqueue to eventdev");
	l = 0;
	while (l++ < EDEV_RETRY) {

		if (eid != ~0ULL) {
			ret = rte_service_run_iter_on_app_lcore(eid, 0);
			TEST_ASSERT(ret == 0, "failed to run service %d", ret);
		}

		ret = rte_service_run_iter_on_app_lcore(tid, 0);
		TEST_ASSERT(ret == 0, "failed to run service %d", ret);

		if (rte_eth_rx_burst(TEST_ETHDEV_PAIR_ID, tx_queue_id,
				&r, 1)) {
			TEST_ASSERT_EQUAL(r, m, "mbuf comparison failed"
					" expected %p received %p", m, r);
			return 0;
		}
	}

	TEST_ASSERT(0, "Failed to receive packet");
	return -1;
}

static int
tx_adapter_service(void)
{
	struct rte_event_eth_tx_adapter_stats stats;
	uint32_t i;
	int err;
	uint8_t ev_port, ev_qid;
	struct rte_mbuf  bufs[RING_SIZE];
	struct rte_mbuf *pbufs[RING_SIZE];
	struct rte_event_dev_info dev_info;
	struct rte_event_dev_config dev_conf;
	struct rte_event_queue_conf qconf;
	uint32_t qcnt, pcnt;
	uint16_t q;
	int internal_port;
	uint32_t cap;

	/* Initialize mbufs */
	for (i = 0; i < RING_SIZE; i++)
		rte_pktmbuf_reset(&bufs[i]);

	memset(&dev_conf, 0, sizeof(dev_conf));
	err = rte_event_eth_tx_adapter_caps_get(TEST_DEV_ID, TEST_ETHDEV_ID,
						&cap);
	TEST_ASSERT(err == 0, "Failed to get adapter cap err %d\n", err);

	internal_port = !!(cap & RTE_EVENT_ETH_TX_ADAPTER_CAP_INTERNAL_PORT);
	if (internal_port)
		return TEST_SUCCESS;

	err = rte_event_eth_tx_adapter_queue_add(TEST_INST_ID, TEST_ETHDEV_ID,
						-1);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_tx_adapter_event_port_get(TEST_INST_ID,
						&ev_port);
	TEST_ASSERT_SUCCESS(err, "Failed to get event port %d", err);

	err = rte_event_dev_attr_get(TEST_DEV_ID, RTE_EVENT_DEV_ATTR_PORT_COUNT,
					&pcnt);
	TEST_ASSERT_SUCCESS(err, "Port count get failed");

	err = rte_event_dev_attr_get(TEST_DEV_ID,
				RTE_EVENT_DEV_ATTR_QUEUE_COUNT, &qcnt);
	TEST_ASSERT_SUCCESS(err, "Queue count get failed");

	err = rte_event_dev_info_get(TEST_DEV_ID, &dev_info);
	TEST_ASSERT_SUCCESS(err, "Dev info failed");

	dev_conf.nb_event_queue_flows = dev_info.max_event_queue_flows;
	dev_conf.nb_event_port_dequeue_depth =
			dev_info.max_event_port_dequeue_depth;
	dev_conf.nb_event_port_enqueue_depth =
			dev_info.max_event_port_enqueue_depth;
	dev_conf.nb_events_limit =
			dev_info.max_num_events;
	dev_conf.nb_event_queues = qcnt + 1;
	dev_conf.nb_event_ports = pcnt;
	err = rte_event_dev_configure(TEST_DEV_ID, &dev_conf);
	TEST_ASSERT(err == 0, "Event device initialization failed err %d\n",
			err);

	ev_qid = qcnt;
	qconf.nb_atomic_flows = dev_info.max_event_queue_flows;
	qconf.nb_atomic_order_sequences = 32;
	qconf.schedule_type = RTE_SCHED_TYPE_ATOMIC;
	qconf.priority = RTE_EVENT_DEV_PRIORITY_HIGHEST;
	qconf.event_queue_cfg = RTE_EVENT_QUEUE_CFG_SINGLE_LINK;
	err = rte_event_queue_setup(TEST_DEV_ID, ev_qid, &qconf);
	TEST_ASSERT_SUCCESS(err, "Failed to setup queue %u", ev_qid);

	/*
	 * Setup ports again so that the newly added queue is visible
	 * to them
	 */
	for (i = 0; i < pcnt; i++) {

		int n_links;
		uint8_t queues[RTE_EVENT_MAX_QUEUES_PER_DEV];
		uint8_t priorities[RTE_EVENT_MAX_QUEUES_PER_DEV];

		if (i == ev_port)
			continue;

		n_links = rte_event_port_links_get(TEST_DEV_ID, i, queues,
						priorities);
		TEST_ASSERT(n_links > 0, "Failed to get port links %d\n",
			n_links);
		err = rte_event_port_setup(TEST_DEV_ID, i, NULL);
		TEST_ASSERT(err == 0, "Failed to setup port err %d\n", err);
		err = rte_event_port_link(TEST_DEV_ID, i, queues, priorities,
					n_links);
		TEST_ASSERT(n_links == err, "Failed to link all queues"
			" err %s\n", rte_strerror(rte_errno));
	}

	err = rte_event_port_link(TEST_DEV_ID, ev_port, &ev_qid, NULL, 1);
	TEST_ASSERT(err == 1, "Failed to link queue port %u",
		    ev_port);

	err = rte_event_eth_tx_adapter_start(TEST_INST_ID);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	if (!(dev_info.event_dev_cap & RTE_EVENT_DEV_CAP_DISTRIBUTED_SCHED)) {
		err = rte_event_dev_service_id_get(0, (uint32_t *)&eid);
		TEST_ASSERT(err == 0, "Expected 0 got %d", err);

		err = rte_service_runstate_set(eid, 1);
		TEST_ASSERT(err == 0, "Expected 0 got %d", err);

		err = rte_service_set_runstate_mapped_check(eid, 0);
		TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	}

	err = rte_event_eth_tx_adapter_service_id_get(TEST_INST_ID, &tid);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_service_runstate_set(tid, 1);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_service_set_runstate_mapped_check(tid, 0);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_dev_start(TEST_DEV_ID);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	for (q = 0; q < MAX_NUM_QUEUE; q++) {
		for (i = 0; i < RING_SIZE; i++)
			pbufs[i] = &bufs[i];
		for (i = 0; i < RING_SIZE; i++) {
			pbufs[i] = &bufs[i];
			err = tx_adapter_single(TEST_ETHDEV_ID, q, pbufs[i],
						ev_qid,
						RTE_SCHED_TYPE_ORDERED);
			TEST_ASSERT(err == 0, "Expected 0 got %d", err);
		}
		for (i = 0; i < RING_SIZE; i++) {
			TEST_ASSERT_EQUAL(pbufs[i], &bufs[i],
				"Error: received data does not match"
				" that transmitted");
		}
	}

	err = rte_event_eth_tx_adapter_stats_get(TEST_INST_ID, NULL);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	err = rte_event_eth_tx_adapter_stats_get(TEST_INST_ID, &stats);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	TEST_ASSERT_EQUAL(stats.tx_packets, MAX_NUM_QUEUE * RING_SIZE,
			"stats.tx_packets expected %u got %"PRIu64,
			MAX_NUM_QUEUE * RING_SIZE,
			stats.tx_packets);

	err = rte_event_eth_tx_adapter_stats_reset(TEST_INST_ID);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_tx_adapter_stats_get(TEST_INST_ID, &stats);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	TEST_ASSERT_EQUAL(stats.tx_packets, 0,
			"stats.tx_packets expected %u got %"PRIu64,
			0,
			stats.tx_packets);

	err = rte_event_eth_tx_adapter_stats_get(1, &stats);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	err = rte_event_eth_tx_adapter_queue_del(TEST_INST_ID, TEST_ETHDEV_ID,
						-1);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_tx_adapter_free(TEST_INST_ID);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	rte_event_dev_stop(TEST_DEV_ID);

	return TEST_SUCCESS;
}

static int
tx_adapter_instance_get(void)
{
	int err;
	uint8_t inst_id;
	uint16_t eth_dev_id;
	struct rte_eth_dev_info dev_info;

	/* Case 1: Test without configuring eth */
	err = rte_event_eth_tx_adapter_instance_get(TEST_ETHDEV_ID,
						    TEST_ETH_QUEUE_ID,
						    &inst_id);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	/* Case 2: Test with wrong eth port */
	eth_dev_id = rte_eth_dev_count_total() + 1;
	err = rte_event_eth_tx_adapter_instance_get(eth_dev_id,
						    TEST_ETH_QUEUE_ID,
						    &inst_id);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	/* Case 3: Test with wrong tx queue */
	err = rte_eth_dev_info_get(TEST_ETHDEV_ID, &dev_info);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_tx_adapter_instance_get(TEST_ETHDEV_ID,
						    dev_info.max_tx_queues + 1,
						    &inst_id);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	/* Case 4: Test with right instance, port & rxq */
	/* Add queue to tx adapter */
	err = rte_event_eth_tx_adapter_queue_add(TEST_INST_ID,
						 TEST_ETHDEV_ID,
						 TEST_ETH_QUEUE_ID);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_tx_adapter_instance_get(TEST_ETHDEV_ID,
						    TEST_ETH_QUEUE_ID,
						    &inst_id);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	TEST_ASSERT(inst_id == TEST_INST_ID, "Expected %d got %d",
		    TEST_INST_ID, err);

	/* Add another queue to tx adapter */
	err = rte_event_eth_tx_adapter_queue_add(TEST_INST_ID,
						 TEST_ETHDEV_ID,
						 TEST_ETH_QUEUE_ID + 1);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_tx_adapter_instance_get(TEST_ETHDEV_ID,
						    TEST_ETH_QUEUE_ID + 1,
						    &inst_id);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	TEST_ASSERT(inst_id == TEST_INST_ID, "Expected %d got %d",
		    TEST_INST_ID, err);

	/* Case 5: Test with right instance, port & wrong rxq */
	err = rte_event_eth_tx_adapter_instance_get(TEST_ETHDEV_ID,
						    TEST_ETH_QUEUE_ID + 2,
						    &inst_id);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	/* Delete all queues from the Tx adapter */
	err = rte_event_eth_tx_adapter_queue_del(TEST_INST_ID,
						 TEST_ETHDEV_ID,
						 -1);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	return TEST_SUCCESS;
}

static int
tx_adapter_queue_start_stop(void)
{
	int err;
	uint16_t eth_dev_id;
	struct rte_eth_dev_info dev_info;

	/* Case 1: Test without adding eth Tx queue */
	err = rte_event_eth_tx_adapter_queue_start(TEST_ETHDEV_ID,
						    TEST_ETH_QUEUE_ID);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	err = rte_event_eth_tx_adapter_queue_stop(TEST_ETHDEV_ID,
						    TEST_ETH_QUEUE_ID);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	/* Case 2: Test with wrong eth port */
	eth_dev_id = rte_eth_dev_count_total() + 1;
	err = rte_event_eth_tx_adapter_queue_start(eth_dev_id,
						    TEST_ETH_QUEUE_ID);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	err = rte_event_eth_tx_adapter_queue_stop(eth_dev_id,
						    TEST_ETH_QUEUE_ID);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	/* Case 3: Test with wrong tx queue */
	err = rte_eth_dev_info_get(TEST_ETHDEV_ID, &dev_info);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_tx_adapter_queue_start(TEST_ETHDEV_ID,
						    dev_info.max_tx_queues + 1);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	err = rte_event_eth_tx_adapter_queue_stop(TEST_ETHDEV_ID,
						    dev_info.max_tx_queues + 1);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	/* Case 4: Test with right instance, port & rxq */
	/* Add queue to tx adapter */
	err = rte_event_eth_tx_adapter_queue_add(TEST_INST_ID,
						 TEST_ETHDEV_ID,
						 TEST_ETH_QUEUE_ID);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_tx_adapter_queue_stop(TEST_ETHDEV_ID,
						    TEST_ETH_QUEUE_ID);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_tx_adapter_queue_start(TEST_ETHDEV_ID,
						    TEST_ETH_QUEUE_ID);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	/* Add another queue to tx adapter */
	err = rte_event_eth_tx_adapter_queue_add(TEST_INST_ID,
						 TEST_ETHDEV_ID,
						 TEST_ETH_QUEUE_ID + 1);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_tx_adapter_queue_stop(TEST_ETHDEV_ID,
						    TEST_ETH_QUEUE_ID + 1);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	err = rte_event_eth_tx_adapter_queue_start(TEST_ETHDEV_ID,
						    TEST_ETH_QUEUE_ID + 1);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	/* Case 5: Test with right instance, port & wrong rxq */
	err = rte_event_eth_tx_adapter_queue_stop(TEST_ETHDEV_ID,
						    TEST_ETH_QUEUE_ID + 2);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	err = rte_event_eth_tx_adapter_queue_start(TEST_ETHDEV_ID,
						    TEST_ETH_QUEUE_ID + 2);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	/* Delete all queues from the Tx adapter */
	err = rte_event_eth_tx_adapter_queue_del(TEST_INST_ID,
						 TEST_ETHDEV_ID,
						 -1);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	return TEST_SUCCESS;
}

static int
tx_adapter_set_get_params(void)
{
	int err, rc;
	struct rte_event_eth_tx_adapter_runtime_params in_params;
	struct rte_event_eth_tx_adapter_runtime_params out_params;

	err = rte_event_eth_tx_adapter_queue_add(TEST_INST_ID,
						 TEST_ETHDEV_ID,
						 0);
	if (err == -ENOTSUP) {
		rc = TEST_SKIPPED;
		goto skip;
	}
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_tx_adapter_runtime_params_init(&in_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	err = rte_event_eth_tx_adapter_runtime_params_init(&out_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	/* Case 1: Get the default values of adapter */
	err = rte_event_eth_tx_adapter_runtime_params_get(TEST_INST_ID,
							  &out_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	TEST_ASSERT(out_params.flush_threshold == DEFAULT_FLUSH_THRESHOLD,
		    "Expected %u got %u",
		    DEFAULT_FLUSH_THRESHOLD, out_params.flush_threshold);
	TEST_ASSERT(out_params.max_nb_tx == TXA_NB_TX_WORK_DEFAULT,
		    "Expected %u got %u",
		    TXA_NB_TX_WORK_DEFAULT, out_params.max_nb_tx);

	/* Case 2: Set max_nb_tx = 32 (=TXA_BATCH_SEIZE) */
	in_params.max_nb_tx = 32;
	in_params.flush_threshold = DEFAULT_FLUSH_THRESHOLD;

	err = rte_event_eth_tx_adapter_runtime_params_set(TEST_INST_ID,
							  &in_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_tx_adapter_runtime_params_get(TEST_INST_ID,
							  &out_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	TEST_ASSERT(in_params.max_nb_tx == out_params.max_nb_tx,
		    "Expected %u got %u",
		    in_params.max_nb_tx, out_params.max_nb_tx);
	TEST_ASSERT(in_params.flush_threshold == out_params.flush_threshold,
		    "Expected %u got %u",
		    in_params.flush_threshold, out_params.flush_threshold);

	/* Case 3: Set max_nb_tx = 192 */
	in_params.max_nb_tx = 192;

	err = rte_event_eth_tx_adapter_runtime_params_set(TEST_INST_ID,
							  &in_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_tx_adapter_runtime_params_get(TEST_INST_ID,
							  &out_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	TEST_ASSERT(in_params.max_nb_tx == out_params.max_nb_tx,
		    "Expected %u got %u",
		    in_params.max_nb_tx, out_params.max_nb_tx);

	/* Case 4: Set max_nb_tx = 256 */
	in_params.max_nb_tx = 256;

	err = rte_event_eth_tx_adapter_runtime_params_set(TEST_INST_ID,
							  &in_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_tx_adapter_runtime_params_get(TEST_INST_ID,
							  &out_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	TEST_ASSERT(in_params.max_nb_tx == out_params.max_nb_tx,
		    "Expected %u got %u",
		    in_params.max_nb_tx, out_params.max_nb_tx);

	/* Case 5: Set max_nb_tx = 30(<TXA_BATCH_SIZE) */
	in_params.max_nb_tx = 30;

	err = rte_event_eth_tx_adapter_runtime_params_set(TEST_INST_ID,
							  &in_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_tx_adapter_runtime_params_get(TEST_INST_ID,
							  &out_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	TEST_ASSERT(in_params.max_nb_tx == out_params.max_nb_tx,
		    "Expected %u got %u",
		    in_params.max_nb_tx, out_params.max_nb_tx);

	/* Case 6: Set max_nb_tx = 512 */
	in_params.max_nb_tx = 512;

	err = rte_event_eth_tx_adapter_runtime_params_set(TEST_INST_ID,
							  &in_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_tx_adapter_runtime_params_get(TEST_INST_ID,
							  &out_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	TEST_ASSERT(in_params.max_nb_tx == out_params.max_nb_tx,
		    "Expected %u got %u",
		    in_params.max_nb_tx, out_params.max_nb_tx);

	/* Case 7: Set flush_threshold = 10 */
	in_params.max_nb_tx = 128;
	in_params.flush_threshold = 10;

	err = rte_event_eth_tx_adapter_runtime_params_set(TEST_INST_ID,
							  &in_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_tx_adapter_runtime_params_get(TEST_INST_ID,
							  &out_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	TEST_ASSERT(in_params.max_nb_tx == out_params.max_nb_tx,
		    "Expected %u got %u",
		    in_params.max_nb_tx, out_params.max_nb_tx);
	TEST_ASSERT(in_params.flush_threshold == out_params.flush_threshold,
		    "Expected %u got %u",
		    in_params.flush_threshold, out_params.flush_threshold);
	rc = TEST_SUCCESS;
skip:
	err = rte_event_eth_tx_adapter_queue_del(TEST_INST_ID,
						 TEST_ETHDEV_ID,
						 0);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	return rc;
}

static int
tx_adapter_dynamic_device(void)
{
	uint16_t port_id = rte_eth_dev_count_avail();
	const char *null_dev[2] = { "eth_null0", "eth_null1" };
	struct rte_eth_conf dev_conf;
	int ret;
	size_t i;

	memset(&dev_conf, 0, sizeof(dev_conf));
	for (i = 0; i < RTE_DIM(null_dev); i++) {
		ret = rte_vdev_init(null_dev[i], NULL);
		TEST_ASSERT_SUCCESS(ret, "%s Port creation failed %d",
				null_dev[i], ret);

		if (i == 0) {
			ret = tx_adapter_create();
			TEST_ASSERT_SUCCESS(ret, "Adapter create failed %d",
					ret);
		}

		ret = rte_eth_dev_configure(port_id + i, MAX_NUM_QUEUE,
					MAX_NUM_QUEUE, &dev_conf);
		TEST_ASSERT_SUCCESS(ret, "Failed to configure device %d", ret);

		ret = rte_event_eth_tx_adapter_queue_add(TEST_INST_ID,
							port_id + i, 0);
		TEST_ASSERT_SUCCESS(ret, "Failed to add queues %d", ret);

	}

	for (i = 0; i < RTE_DIM(null_dev); i++) {
		ret = rte_event_eth_tx_adapter_queue_del(TEST_INST_ID,
							port_id + i, -1);
		TEST_ASSERT_SUCCESS(ret, "Failed to delete queues %d", ret);
	}

	tx_adapter_free();

	for (i = 0; i < RTE_DIM(null_dev); i++)
		rte_vdev_uninit(null_dev[i]);

	return TEST_SUCCESS;
}

static struct unit_test_suite event_eth_tx_tests = {
	.setup = testsuite_setup,
	.teardown = testsuite_teardown,
	.suite_name = "tx event eth adapter test suite",
	.unit_test_cases = {
		TEST_CASE_ST(NULL, NULL, tx_adapter_create_free),
		TEST_CASE_ST(tx_adapter_create, tx_adapter_free,
					tx_adapter_queue_add_del),
		TEST_CASE_ST(tx_adapter_create, tx_adapter_free,
					tx_adapter_start_stop),
		TEST_CASE_ST(tx_adapter_create, tx_adapter_free,
					tx_adapter_service),
		TEST_CASE_ST(tx_adapter_create, tx_adapter_free,
					tx_adapter_instance_get),
		TEST_CASE_ST(tx_adapter_create, tx_adapter_free,
					tx_adapter_queue_start_stop),
		TEST_CASE_ST(tx_adapter_create, tx_adapter_free,
					tx_adapter_set_get_params),
		TEST_CASE_ST(NULL, NULL, tx_adapter_dynamic_device),
		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static int
test_event_eth_tx_adapter_common(void)
{
	return unit_test_suite_runner(&event_eth_tx_tests);
}

#endif /* !RTE_EXEC_ENV_WINDOWS */

REGISTER_FAST_TEST(event_eth_tx_adapter_autotest, false, true, test_event_eth_tx_adapter_common);
