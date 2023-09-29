/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Marvell.
 */

#include "test.h"
#include <string.h>
#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_random.h>

#ifdef RTE_EXEC_ENV_WINDOWS
static int
test_event_dma_adapter(void)
{
	printf("event_dma_adapter not supported on Windows, skipping test\n");
	return TEST_SKIPPED;
}

#else

#include <rte_bus_vdev.h>
#include <rte_dmadev.h>
#include <rte_eventdev.h>
#include <rte_event_dma_adapter.h>
#include <rte_service.h>

#define NUM_MBUFS                 (8191)
#define MBUF_CACHE_SIZE           (256)
#define TEST_APP_PORT_ID           0
#define TEST_APP_EV_QUEUE_ID       0
#define TEST_APP_EV_PRIORITY       0
#define TEST_APP_EV_FLOWID         0xAABB
#define TEST_DMA_EV_QUEUE_ID       1
#define TEST_ADAPTER_ID            0
#define TEST_DMA_DEV_ID            0
#define TEST_DMA_VCHAN_ID          0
#define PACKET_LENGTH              1024
#define NB_TEST_PORTS              1
#define NB_TEST_QUEUES             2
#define NUM_CORES                  2
#define DMA_OP_POOL_SIZE           128
#define TEST_MAX_OP                32
#define TEST_RINGSIZE              512

#define MBUF_SIZE                  (RTE_PKTMBUF_HEADROOM + PACKET_LENGTH)

/* Handle log statements in same manner as test macros */
#define LOG_DBG(...)    RTE_LOG(DEBUG, EAL, __VA_ARGS__)

struct event_dma_adapter_test_params {
	struct rte_mempool *src_mbuf_pool;
	struct rte_mempool *dst_mbuf_pool;
	struct rte_mempool *op_mpool;
	uint8_t dma_event_port_id;
	uint8_t internal_port_op_fwd;
};

struct rte_event dma_response_info = {
	.queue_id = TEST_APP_EV_QUEUE_ID,
	.sched_type = RTE_SCHED_TYPE_ATOMIC,
	.flow_id = TEST_APP_EV_FLOWID,
	.priority = TEST_APP_EV_PRIORITY
};

static struct event_dma_adapter_test_params params;
static uint8_t dma_adapter_setup_done;
static uint32_t slcore_id;
static int evdev;

static int
send_recv_ev(struct rte_event *ev)
{
	struct rte_event recv_ev[TEST_MAX_OP];
	uint16_t nb_enqueued = 0;
	int i = 0;

	if (params.internal_port_op_fwd) {
		nb_enqueued = rte_event_dma_adapter_enqueue(evdev, TEST_APP_PORT_ID, ev,
							    TEST_MAX_OP);
	} else {
		while (nb_enqueued < TEST_MAX_OP) {
			nb_enqueued += rte_event_enqueue_burst(evdev, TEST_APP_PORT_ID,
							       &ev[nb_enqueued], TEST_MAX_OP -
							       nb_enqueued);
		}
	}

	TEST_ASSERT_EQUAL(nb_enqueued, TEST_MAX_OP, "Failed to send event to dma adapter\n");

	while (i < TEST_MAX_OP) {
		if (rte_event_dequeue_burst(evdev, TEST_APP_PORT_ID, &recv_ev[i], 1, 0) != 1)
			continue;
		i++;
	}

	TEST_ASSERT_EQUAL(i, TEST_MAX_OP, "Test failed. Failed to dequeue events.\n");

	return TEST_SUCCESS;
}

static int
test_dma_adapter_stats(void)
{
	struct rte_event_dma_adapter_stats stats;

	rte_event_dma_adapter_stats_get(TEST_ADAPTER_ID, &stats);
	printf(" +------------------------------------------------------+\n");
	printf(" + DMA adapter stats for instance %u:\n", TEST_ADAPTER_ID);
	printf(" + Event port poll count         0x%" PRIx64 "\n",
		stats.event_poll_count);
	printf(" + Event dequeue count           0x%" PRIx64 "\n",
		stats.event_deq_count);
	printf(" + DMA dev enqueue count         0x%" PRIx64 "\n",
		stats.dma_enq_count);
	printf(" + DMA dev enqueue failed count  0x%" PRIx64 "\n",
		stats.dma_enq_fail_count);
	printf(" + DMA dev dequeue count         0x%" PRIx64 "\n",
		stats.dma_deq_count);
	printf(" + Event enqueue count           0x%" PRIx64 "\n",
		stats.event_enq_count);
	printf(" + Event enqueue retry count     0x%" PRIx64 "\n",
		stats.event_enq_retry_count);
	printf(" + Event enqueue fail count      0x%" PRIx64 "\n",
		stats.event_enq_fail_count);
	printf(" +------------------------------------------------------+\n");

	rte_event_dma_adapter_stats_reset(TEST_ADAPTER_ID);
	return TEST_SUCCESS;
}

static int
test_dma_adapter_params(void)
{
	struct rte_event_dma_adapter_runtime_params out_params;
	struct rte_event_dma_adapter_runtime_params in_params;
	struct rte_event event;
	uint32_t cap;
	int err, rc;

	err = rte_event_dma_adapter_caps_get(evdev, TEST_DMA_DEV_ID, &cap);
	TEST_ASSERT_SUCCESS(err, "Failed to get adapter capabilities\n");

	if (cap & RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_VCHAN_EV_BIND) {
		err = rte_event_dma_adapter_vchan_add(TEST_ADAPTER_ID, TEST_DMA_DEV_ID,
							    TEST_DMA_VCHAN_ID, &event);
	} else
		err = rte_event_dma_adapter_vchan_add(TEST_ADAPTER_ID, TEST_DMA_DEV_ID,
							    TEST_DMA_VCHAN_ID, NULL);

	TEST_ASSERT_SUCCESS(err, "Failed to add vchan\n");

	err = rte_event_dma_adapter_runtime_params_init(&in_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	err = rte_event_dma_adapter_runtime_params_init(&out_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	/* Case 1: Get the default value of mbufs processed by adapter */
	err = rte_event_dma_adapter_runtime_params_get(TEST_ADAPTER_ID, &out_params);
	if (err == -ENOTSUP) {
		rc = TEST_SKIPPED;
		goto vchan_del;
	}
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	/* Case 2: Set max_nb = 32 (=BATCH_SEIZE) */
	in_params.max_nb = 32;

	err = rte_event_dma_adapter_runtime_params_set(TEST_ADAPTER_ID, &in_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_dma_adapter_runtime_params_get(TEST_ADAPTER_ID, &out_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	TEST_ASSERT(in_params.max_nb == out_params.max_nb, "Expected %u got %u",
		    in_params.max_nb, out_params.max_nb);

	/* Case 3: Set max_nb = 192 */
	in_params.max_nb = 192;

	err = rte_event_dma_adapter_runtime_params_set(TEST_ADAPTER_ID, &in_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_dma_adapter_runtime_params_get(TEST_ADAPTER_ID, &out_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	TEST_ASSERT(in_params.max_nb == out_params.max_nb, "Expected %u got %u",
		    in_params.max_nb, out_params.max_nb);

	/* Case 4: Set max_nb = 256 */
	in_params.max_nb = 256;

	err = rte_event_dma_adapter_runtime_params_set(TEST_ADAPTER_ID, &in_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_dma_adapter_runtime_params_get(TEST_ADAPTER_ID, &out_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	TEST_ASSERT(in_params.max_nb == out_params.max_nb, "Expected %u got %u",
		    in_params.max_nb, out_params.max_nb);

	/* Case 5: Set max_nb = 30(<BATCH_SIZE) */
	in_params.max_nb = 30;

	err = rte_event_dma_adapter_runtime_params_set(TEST_ADAPTER_ID, &in_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_dma_adapter_runtime_params_get(TEST_ADAPTER_ID, &out_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	TEST_ASSERT(in_params.max_nb == out_params.max_nb, "Expected %u got %u",
		    in_params.max_nb, out_params.max_nb);

	/* Case 6: Set max_nb = 512 */
	in_params.max_nb = 512;

	err = rte_event_dma_adapter_runtime_params_set(TEST_ADAPTER_ID, &in_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_dma_adapter_runtime_params_get(TEST_ADAPTER_ID, &out_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	TEST_ASSERT(in_params.max_nb == out_params.max_nb, "Expected %u got %u",
		    in_params.max_nb, out_params.max_nb);

	rc = TEST_SUCCESS;
vchan_del:
	err = rte_event_dma_adapter_vchan_del(TEST_ADAPTER_ID, TEST_DMA_DEV_ID,
						    TEST_DMA_VCHAN_ID);
	TEST_ASSERT_SUCCESS(err, "Failed to delete vchan\n");

	return rc;
}

static int
test_op_forward_mode(void)
{
	struct rte_mbuf *src_mbuf[TEST_MAX_OP];
	struct rte_mbuf *dst_mbuf[TEST_MAX_OP];
	struct rte_event_dma_adapter_op *op;
	struct rte_event ev[TEST_MAX_OP];
	struct rte_event response_info;
	int ret, i;

	ret = rte_pktmbuf_alloc_bulk(params.src_mbuf_pool, src_mbuf, TEST_MAX_OP);
	TEST_ASSERT_SUCCESS(ret, "alloc src mbufs failed.\n");

	ret = rte_pktmbuf_alloc_bulk(params.dst_mbuf_pool, dst_mbuf, TEST_MAX_OP);
	TEST_ASSERT_SUCCESS(ret, "alloc dst mbufs failed.\n");

	for (i = 0; i < TEST_MAX_OP; i++) {
		memset(rte_pktmbuf_mtod(src_mbuf[i], void *), rte_rand(), PACKET_LENGTH);
		memset(rte_pktmbuf_mtod(dst_mbuf[i], void *), 0, PACKET_LENGTH);
	}

	for (i = 0; i < TEST_MAX_OP; i++) {
		rte_mempool_get(params.op_mpool, (void **)&op);
		TEST_ASSERT_NOT_NULL(op, "Failed to allocate dma operation struct\n");

		op->src_seg = rte_malloc(NULL, sizeof(struct rte_dma_sge), 0);
		op->dst_seg = rte_malloc(NULL, sizeof(struct rte_dma_sge), 0);

		/* Update Op */
		op->src_seg->addr = rte_pktmbuf_iova(src_mbuf[i]);
		op->dst_seg->addr = rte_pktmbuf_iova(dst_mbuf[i]);
		op->src_seg->length = PACKET_LENGTH;
		op->dst_seg->length = PACKET_LENGTH;
		op->nb_src = 1;
		op->nb_dst = 1;
		op->flags = RTE_DMA_OP_FLAG_SUBMIT;
		op->op_mp = params.op_mpool;
		op->dma_dev_id = TEST_DMA_DEV_ID;
		op->vchan = TEST_DMA_VCHAN_ID;

		response_info.event = dma_response_info.event;
		rte_memcpy((uint8_t *)op + sizeof(struct rte_event_dma_adapter_op), &response_info,
			   sizeof(struct rte_event));

		/* Fill in event info and update event_ptr with rte_event_dma_adapter_op */
		memset(&ev[i], 0, sizeof(struct rte_event));
		ev[i].event = 0;
		ev[i].event_type = RTE_EVENT_TYPE_DMADEV;
		ev[i].queue_id = TEST_DMA_EV_QUEUE_ID;
		ev[i].sched_type = RTE_SCHED_TYPE_ATOMIC;
		ev[i].flow_id = 0xAABB;
		ev[i].event_ptr = op;
	}

	ret = send_recv_ev(ev);
	TEST_ASSERT_SUCCESS(ret, "Failed to send/receive event to dma adapter\n");

	test_dma_adapter_stats();

	for (i = 0; i < TEST_MAX_OP; i++) {
		op = ev[i].event_ptr;
		ret = memcmp(rte_pktmbuf_mtod(src_mbuf[i], void *),
			     rte_pktmbuf_mtod(dst_mbuf[i], void *), PACKET_LENGTH);

		TEST_ASSERT_EQUAL(ret, 0, "Data mismatch for dma adapter\n");

		rte_free(op->src_seg);
		rte_free(op->dst_seg);
		rte_mempool_put(op->op_mp, op);
	}

	rte_pktmbuf_free_bulk(src_mbuf, TEST_MAX_OP);
	rte_pktmbuf_free_bulk(dst_mbuf, TEST_MAX_OP);

	return TEST_SUCCESS;
}

static int
map_adapter_service_core(void)
{
	uint32_t adapter_service_id;
	int ret;

	if (rte_event_dma_adapter_service_id_get(TEST_ADAPTER_ID, &adapter_service_id) == 0) {
		uint32_t core_list[NUM_CORES];

		ret = rte_service_lcore_list(core_list, NUM_CORES);
		TEST_ASSERT(ret >= 0, "Failed to get service core list!");

		if (core_list[0] != slcore_id) {
			TEST_ASSERT_SUCCESS(rte_service_lcore_add(slcore_id),
						"Failed to add service core");
			TEST_ASSERT_SUCCESS(rte_service_lcore_start(slcore_id),
						"Failed to start service core");
		}

		TEST_ASSERT_SUCCESS(rte_service_map_lcore_set(
					adapter_service_id, slcore_id, 1),
					"Failed to map adapter service");
	}

	return TEST_SUCCESS;
}

static int
test_with_op_forward_mode(void)
{
	uint32_t cap;
	int ret;

	ret = rte_event_dma_adapter_caps_get(evdev, TEST_DMA_DEV_ID, &cap);
	TEST_ASSERT_SUCCESS(ret, "Failed to get adapter capabilities\n");

	if (!(cap & RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_OP_FWD) &&
			!(cap & RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_OP_NEW))
		map_adapter_service_core();
	else {
		if (!(cap & RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_OP_FWD))
			return TEST_SKIPPED;
	}

	TEST_ASSERT_SUCCESS(rte_event_dma_adapter_start(TEST_ADAPTER_ID),
			"Failed to start event dma adapter");

	ret = test_op_forward_mode();
	TEST_ASSERT_SUCCESS(ret, "DMA - FORWARD mode test failed\n");
	return TEST_SUCCESS;
}

static int
configure_dmadev(void)
{
	const struct rte_dma_conf conf = { .nb_vchans = 1};
	const struct rte_dma_vchan_conf qconf = {
		.direction = RTE_DMA_DIR_MEM_TO_MEM,
		.nb_desc = TEST_RINGSIZE,
	};
	struct rte_dma_info info;
	unsigned int elt_size;
	int ret;

	ret = rte_dma_count_avail();
	RTE_TEST_ASSERT_FAIL(ret, "No dma devices found!\n");

	ret = rte_dma_info_get(TEST_DMA_DEV_ID, &info);
	TEST_ASSERT_SUCCESS(ret, "Error with rte_dma_info_get()\n");

	if (info.max_vchans < 1)
		RTE_LOG(ERR, USER1, "Error, no channels available on device id %u\n",
				TEST_DMA_DEV_ID);

	if (rte_dma_configure(TEST_DMA_DEV_ID, &conf) != 0)
		RTE_LOG(ERR, USER1, "Error with rte_dma_configure()\n");

	if (rte_dma_vchan_setup(TEST_DMA_DEV_ID, TEST_DMA_VCHAN_ID, &qconf) < 0)
		RTE_LOG(ERR, USER1, "Error with vchan configuration\n");

	ret = rte_dma_info_get(TEST_DMA_DEV_ID, &info);
	if (ret != 0 || info.nb_vchans != 1)
		RTE_LOG(ERR, USER1, "Error, no configured vhcan reported on device id %u\n",
				TEST_DMA_DEV_ID);

	params.src_mbuf_pool = rte_pktmbuf_pool_create("DMA_ADAPTER_SRC_MBUFPOOL", NUM_MBUFS,
						       MBUF_CACHE_SIZE, 0, MBUF_SIZE,
						       rte_socket_id());
	RTE_TEST_ASSERT_NOT_NULL(params.src_mbuf_pool, "Can't create DMA_SRC_MBUFPOOL\n");

	params.dst_mbuf_pool = rte_pktmbuf_pool_create("DMA_ADAPTER_DST_MBUFPOOL", NUM_MBUFS,
						       MBUF_CACHE_SIZE, 0, MBUF_SIZE,
						       rte_socket_id());
	RTE_TEST_ASSERT_NOT_NULL(params.dst_mbuf_pool, "Can't create DMA_DST_MBUFPOOL\n");

	elt_size = sizeof(struct rte_event_dma_adapter_op) + sizeof(struct rte_event);
	params.op_mpool = rte_mempool_create("EVENT_DMA_OP_POOL", DMA_OP_POOL_SIZE, elt_size, 0,
					     0, NULL, NULL, NULL, NULL, rte_socket_id(), 0);
	RTE_TEST_ASSERT_NOT_NULL(params.op_mpool, "Can't create DMA_OP_POOL\n");

	return TEST_SUCCESS;
}

static inline void
evdev_set_conf_values(struct rte_event_dev_config *dev_conf, struct rte_event_dev_info *info)
{
	memset(dev_conf, 0, sizeof(struct rte_event_dev_config));
	dev_conf->dequeue_timeout_ns = info->min_dequeue_timeout_ns;
	dev_conf->nb_event_ports = NB_TEST_PORTS;
	dev_conf->nb_event_queues = NB_TEST_QUEUES;
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
configure_eventdev(void)
{
	struct rte_event_queue_conf queue_conf;
	struct rte_event_dev_config devconf;
	struct rte_event_dev_info info;
	uint32_t queue_count;
	uint32_t port_count;
	uint8_t qid;
	int ret;

	if (!rte_event_dev_count()) {
		/* If there is no hardware eventdev, or no software vdev was
		 * specified on the command line, create an instance of
		 * event_sw.
		 */
		LOG_DBG("Failed to find a valid event device... "
				"testing with event_sw device\n");
		TEST_ASSERT_SUCCESS(rte_vdev_init("event_sw0", NULL),
				"Error creating eventdev");
		evdev = rte_event_dev_get_dev_id("event_sw0");
	}

	ret = rte_event_dev_info_get(evdev, &info);
	TEST_ASSERT_SUCCESS(ret, "Failed to get event dev info\n");

	evdev_set_conf_values(&devconf, &info);

	ret = rte_event_dev_configure(evdev, &devconf);
	TEST_ASSERT_SUCCESS(ret, "Failed to configure eventdev\n");

	/* Set up event queue */
	ret = rte_event_dev_attr_get(evdev, RTE_EVENT_DEV_ATTR_QUEUE_COUNT, &queue_count);
	TEST_ASSERT_SUCCESS(ret, "Queue count get failed\n");
	TEST_ASSERT_EQUAL(queue_count, 2, "Unexpected queue count\n");

	qid = TEST_APP_EV_QUEUE_ID;
	ret = rte_event_queue_setup(evdev, qid, NULL);
	TEST_ASSERT_SUCCESS(ret, "Failed to setup queue=%d\n", qid);

	queue_conf.nb_atomic_flows = info.max_event_queue_flows;
	queue_conf.nb_atomic_order_sequences = 32;
	queue_conf.schedule_type = RTE_SCHED_TYPE_ATOMIC;
	queue_conf.priority = RTE_EVENT_DEV_PRIORITY_HIGHEST;
	queue_conf.event_queue_cfg = RTE_EVENT_QUEUE_CFG_SINGLE_LINK;

	qid = TEST_DMA_EV_QUEUE_ID;
	ret = rte_event_queue_setup(evdev, qid, &queue_conf);
	TEST_ASSERT_SUCCESS(ret, "Failed to setup queue=%u\n", qid);

	/* Set up event port */
	ret = rte_event_dev_attr_get(evdev, RTE_EVENT_DEV_ATTR_PORT_COUNT,
			&port_count);
	TEST_ASSERT_SUCCESS(ret, "Port count get failed\n");
	TEST_ASSERT_EQUAL(port_count, 1, "Unexpected port count\n");

	ret = rte_event_port_setup(evdev, TEST_APP_PORT_ID, NULL);
	TEST_ASSERT_SUCCESS(ret, "Failed to setup port=%d\n",
			TEST_APP_PORT_ID);

	qid = TEST_APP_EV_QUEUE_ID;
	ret = rte_event_port_link(evdev, TEST_APP_PORT_ID, &qid, NULL, 1);
	TEST_ASSERT(ret >= 0, "Failed to link queue port=%d\n",
			TEST_APP_PORT_ID);

	return TEST_SUCCESS;
}

static void
test_dma_adapter_free(void)
{
	rte_event_dma_adapter_free(TEST_ADAPTER_ID);
}

static int
test_dma_adapter_create(void)
{
	struct rte_event_dev_info evdev_info = {0};
	struct rte_event_port_conf conf = {0};
	int ret;

	ret = rte_event_dev_info_get(evdev, &evdev_info);
	TEST_ASSERT_SUCCESS(ret, "Failed to create event dma adapter\n");

	conf.new_event_threshold = evdev_info.max_num_events;
	conf.dequeue_depth = evdev_info.max_event_port_dequeue_depth;
	conf.enqueue_depth = evdev_info.max_event_port_enqueue_depth;

	/* Create adapter with default port creation callback */
	ret = rte_event_dma_adapter_create(TEST_ADAPTER_ID, evdev, &conf, 0);
	TEST_ASSERT_SUCCESS(ret, "Failed to create event dma adapter\n");

	return TEST_SUCCESS;
}

static int
test_dma_adapter_vchan_add_del(void)
{
	struct rte_event event;
	uint32_t cap;
	int ret;

	ret = rte_event_dma_adapter_caps_get(evdev, TEST_DMA_DEV_ID, &cap);
	TEST_ASSERT_SUCCESS(ret, "Failed to get adapter capabilities\n");

	if (cap & RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_VCHAN_EV_BIND) {
		ret = rte_event_dma_adapter_vchan_add(TEST_ADAPTER_ID, TEST_DMA_DEV_ID,
							    TEST_DMA_VCHAN_ID, &event);
	} else
		ret = rte_event_dma_adapter_vchan_add(TEST_ADAPTER_ID, TEST_DMA_DEV_ID,
							    TEST_DMA_VCHAN_ID, NULL);

	TEST_ASSERT_SUCCESS(ret, "Failed to create add vchan\n");

	ret = rte_event_dma_adapter_vchan_del(TEST_ADAPTER_ID, TEST_DMA_DEV_ID,
						    TEST_DMA_VCHAN_ID);
	TEST_ASSERT_SUCCESS(ret, "Failed to delete vchan\n");

	return TEST_SUCCESS;
}

static int
configure_event_dma_adapter(enum rte_event_dma_adapter_mode mode)
{
	struct rte_event_dev_info evdev_info = {0};
	struct rte_event_port_conf conf = {0};
	struct rte_event event;
	uint32_t cap;
	int ret;

	ret = rte_event_dma_adapter_caps_get(evdev, TEST_DMA_DEV_ID, &cap);
	TEST_ASSERT_SUCCESS(ret, "Failed to get adapter capabilities\n");

	/* Skip mode and capability mismatch check for SW eventdev */
	if (!(cap & RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_OP_NEW) &&
			!(cap & RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_OP_FWD) &&
			!(cap & RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_VCHAN_EV_BIND))
		goto adapter_create;

	if (mode == RTE_EVENT_DMA_ADAPTER_OP_FORWARD) {
		if (cap & RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_OP_FWD)
			params.internal_port_op_fwd = 1;
		else
			return -ENOTSUP;
	}

adapter_create:
	ret = rte_event_dev_info_get(evdev, &evdev_info);
	TEST_ASSERT_SUCCESS(ret, "Failed to create event dma adapter\n");

	conf.new_event_threshold = evdev_info.max_num_events;
	conf.dequeue_depth = evdev_info.max_event_port_dequeue_depth;
	conf.enqueue_depth = evdev_info.max_event_port_enqueue_depth;

	/* Create adapter with default port creation callback */
	ret = rte_event_dma_adapter_create(TEST_ADAPTER_ID, evdev, &conf, mode);
	TEST_ASSERT_SUCCESS(ret, "Failed to create event dma adapter\n");

	if (cap & RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_VCHAN_EV_BIND) {
		ret = rte_event_dma_adapter_vchan_add(TEST_ADAPTER_ID, TEST_DMA_DEV_ID,
							    TEST_DMA_VCHAN_ID, &event);
	} else
		ret = rte_event_dma_adapter_vchan_add(TEST_ADAPTER_ID, TEST_DMA_DEV_ID,
							    TEST_DMA_VCHAN_ID, NULL);

	TEST_ASSERT_SUCCESS(ret, "Failed to add vchan\n");

	if (!params.internal_port_op_fwd) {
		ret = rte_event_dma_adapter_event_port_get(TEST_ADAPTER_ID,
							   &params.dma_event_port_id);
		TEST_ASSERT_SUCCESS(ret, "Failed to get event port\n");
	}

	return TEST_SUCCESS;
}

static void
test_dma_adapter_stop(void)
{
	uint32_t evdev_service_id, adapter_service_id;

	/* retrieve service ids & stop services */
	if (rte_event_dma_adapter_service_id_get(TEST_ADAPTER_ID,
				&adapter_service_id) == 0) {
		rte_service_runstate_set(adapter_service_id, 0);
		rte_service_lcore_stop(slcore_id);
		rte_service_lcore_del(slcore_id);
		rte_event_dma_adapter_stop(TEST_ADAPTER_ID);
	}

	if (rte_event_dev_service_id_get(evdev, &evdev_service_id) == 0) {
		rte_service_runstate_set(evdev_service_id, 0);
		rte_service_lcore_stop(slcore_id);
		rte_service_lcore_del(slcore_id);
		rte_dma_stop(TEST_DMA_DEV_ID);
		rte_event_dev_stop(evdev);
	} else {
		rte_dma_stop(TEST_DMA_DEV_ID);
		rte_event_dev_stop(evdev);
	}
}

static int
test_dma_adapter_conf(enum rte_event_dma_adapter_mode mode)
{
	uint32_t evdev_service_id;
	uint8_t qid;
	int ret;

	if (!dma_adapter_setup_done) {
		ret = configure_event_dma_adapter(mode);
		if (ret)
			return ret;
		if (!params.internal_port_op_fwd) {
			qid = TEST_DMA_EV_QUEUE_ID;
			ret = rte_event_port_link(evdev,
					params.dma_event_port_id, &qid, NULL, 1);
			TEST_ASSERT(ret >= 0, "Failed to link queue %d "
					"port=%u\n", qid,
					params.dma_event_port_id);
		}
		dma_adapter_setup_done = 1;
	}

	/* retrieve service ids */
	if (rte_event_dev_service_id_get(evdev, &evdev_service_id) == 0) {
		/* add a service core and start it */
		TEST_ASSERT_SUCCESS(rte_service_lcore_add(slcore_id),
				"Failed to add service core");
		TEST_ASSERT_SUCCESS(rte_service_lcore_start(slcore_id),
				"Failed to start service core");

		/* map services to it */
		TEST_ASSERT_SUCCESS(rte_service_map_lcore_set(evdev_service_id,
					slcore_id, 1), "Failed to map evdev service");

		/* set services to running */
		TEST_ASSERT_SUCCESS(rte_service_runstate_set(evdev_service_id,
					1), "Failed to start evdev service");
	}

	/* start the eventdev */
	TEST_ASSERT_SUCCESS(rte_event_dev_start(evdev),
			"Failed to start event device");

	/* start the dma dev */
	TEST_ASSERT_SUCCESS(rte_dma_start(TEST_DMA_DEV_ID),
			"Failed to start dma device");

	return TEST_SUCCESS;
}

static int
test_dma_adapter_conf_op_forward_mode(void)
{
	enum rte_event_dma_adapter_mode mode;

	mode = RTE_EVENT_DMA_ADAPTER_OP_FORWARD;

	return test_dma_adapter_conf(mode);
}

static int
testsuite_setup(void)
{
	int ret;

	slcore_id = rte_get_next_lcore(-1, 1, 0);
	TEST_ASSERT_NOT_EQUAL(slcore_id, RTE_MAX_LCORE, "At least 2 lcores "
			"are required to run this autotest\n");

	/* Setup and start event device. */
	ret = configure_eventdev();
	TEST_ASSERT_SUCCESS(ret, "Failed to setup eventdev\n");

	/* Setup and start dma device. */
	ret = configure_dmadev();
	TEST_ASSERT_SUCCESS(ret, "dmadev initialization failed\n");

	return TEST_SUCCESS;
}

static void
dma_adapter_teardown(void)
{
	int ret;

	ret = rte_event_dma_adapter_stop(TEST_ADAPTER_ID);
	if (ret < 0)
		RTE_LOG(ERR, USER1, "Failed to stop adapter!");

	ret = rte_event_dma_adapter_vchan_del(TEST_ADAPTER_ID, TEST_DMA_DEV_ID,
						    TEST_DMA_VCHAN_ID);
	if (ret < 0)
		RTE_LOG(ERR, USER1, "Failed to delete vchan!");

	ret = rte_event_dma_adapter_free(TEST_ADAPTER_ID);
	if (ret < 0)
		RTE_LOG(ERR, USER1, "Failed to free adapter!");

	dma_adapter_setup_done = 0;
}

static void
dma_teardown(void)
{
	/* Free mbuf mempool */
	if (params.src_mbuf_pool != NULL) {
		RTE_LOG(DEBUG, USER1, "DMA_ADAPTER_SRC_MBUFPOOL count %u\n",
				rte_mempool_avail_count(params.src_mbuf_pool));
		rte_mempool_free(params.src_mbuf_pool);
		params.src_mbuf_pool = NULL;
	}

	if (params.dst_mbuf_pool != NULL) {
		RTE_LOG(DEBUG, USER1, "DMA_ADAPTER_DST_MBUFPOOL count %u\n",
				rte_mempool_avail_count(params.dst_mbuf_pool));
		rte_mempool_free(params.dst_mbuf_pool);
		params.dst_mbuf_pool = NULL;
	}

	/* Free ops mempool */
	if (params.op_mpool != NULL) {
		RTE_LOG(DEBUG, USER1, "EVENT_DMA_OP_POOL count %u\n",
				rte_mempool_avail_count(params.op_mpool));
		rte_mempool_free(params.op_mpool);
		params.op_mpool = NULL;
	}
}

static void
eventdev_teardown(void)
{
	rte_event_dev_stop(evdev);
}

static void
testsuite_teardown(void)
{
	dma_adapter_teardown();
	dma_teardown();
	eventdev_teardown();
}

static struct unit_test_suite functional_testsuite = {
	.suite_name = "Event dma adapter test suite",
	.setup = testsuite_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {

		TEST_CASE_ST(NULL, test_dma_adapter_free, test_dma_adapter_create),

		TEST_CASE_ST(test_dma_adapter_create, test_dma_adapter_free,
			     test_dma_adapter_vchan_add_del),

		TEST_CASE_ST(test_dma_adapter_create, test_dma_adapter_free,
			     test_dma_adapter_stats),

		TEST_CASE_ST(test_dma_adapter_create, test_dma_adapter_free,
			     test_dma_adapter_params),

		TEST_CASE_ST(test_dma_adapter_conf_op_forward_mode, test_dma_adapter_stop,
			     test_with_op_forward_mode),

		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static int
test_event_dma_adapter(void)
{
	return unit_test_suite_runner(&functional_testsuite);
}

#endif /* !RTE_EXEC_ENV_WINDOWS */

REGISTER_DRIVER_TEST(event_dma_adapter_autotest, test_event_dma_adapter);
