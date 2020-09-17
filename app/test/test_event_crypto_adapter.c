/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation.
 * All rights reserved.
 */

#include <string.h>
#include <rte_common.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_cryptodev.h>
#include <rte_eventdev.h>
#include <rte_bus_vdev.h>
#include <rte_service.h>
#include <rte_event_crypto_adapter.h>
#include "test.h"

#define PKT_TRACE                  0
#define NUM                        1
#define DEFAULT_NUM_XFORMS        (2)
#define NUM_MBUFS                 (8191)
#define MBUF_CACHE_SIZE           (256)
#define MAXIMUM_IV_LENGTH         (16)
#define DEFAULT_NUM_OPS_INFLIGHT  (128)
#define MAX_NB_SESSIONS            4
#define TEST_APP_PORT_ID           0
#define TEST_APP_EV_QUEUE_ID       0
#define TEST_APP_EV_PRIORITY       0
#define TEST_APP_EV_FLOWID         0xAABB
#define TEST_CRYPTO_EV_QUEUE_ID    1
#define TEST_ADAPTER_ID            0
#define TEST_CDEV_ID               0
#define TEST_CDEV_QP_ID            0
#define PACKET_LENGTH              64
#define NB_TEST_PORTS              1
#define NB_TEST_QUEUES             2
#define NUM_CORES                  1
#define CRYPTODEV_NAME_NULL_PMD    crypto_null

#define MBUF_SIZE              (sizeof(struct rte_mbuf) + \
				RTE_PKTMBUF_HEADROOM + PACKET_LENGTH)
#define IV_OFFSET              (sizeof(struct rte_crypto_op) + \
				sizeof(struct rte_crypto_sym_op) + \
				DEFAULT_NUM_XFORMS * \
				sizeof(struct rte_crypto_sym_xform))

/* Handle log statements in same manner as test macros */
#define LOG_DBG(...)    RTE_LOG(DEBUG, EAL, __VA_ARGS__)

static const uint8_t text_64B[] = {
	0x05, 0x15, 0x77, 0x32, 0xc9, 0x66, 0x91, 0x50,
	0x93, 0x9f, 0xbb, 0x4e, 0x2e, 0x5a, 0x02, 0xd0,
	0x2d, 0x9d, 0x31, 0x5d, 0xc8, 0x9e, 0x86, 0x36,
	0x54, 0x5c, 0x50, 0xe8, 0x75, 0x54, 0x74, 0x5e,
	0xd5, 0xa2, 0x84, 0x21, 0x2d, 0xc5, 0xf8, 0x1c,
	0x55, 0x1a, 0xba, 0x91, 0xce, 0xb5, 0xa3, 0x1e,
	0x31, 0xbf, 0xe9, 0xa1, 0x97, 0x5c, 0x2b, 0xd6,
	0x57, 0xa5, 0x9f, 0xab, 0xbd, 0xb0, 0x9b, 0x9c
};

struct event_crypto_adapter_test_params {
	struct rte_mempool *mbuf_pool;
	struct rte_mempool *op_mpool;
	struct rte_mempool *session_mpool;
	struct rte_mempool *session_priv_mpool;
	struct rte_cryptodev_config *config;
	uint8_t crypto_event_port_id;
};

struct rte_event response_info = {
	.queue_id = TEST_APP_EV_QUEUE_ID,
	.sched_type = RTE_SCHED_TYPE_ATOMIC,
	.flow_id = TEST_APP_EV_FLOWID,
	.priority = TEST_APP_EV_PRIORITY
};

struct rte_event_crypto_request request_info = {
	.cdev_id = TEST_CDEV_ID,
	.queue_pair_id = TEST_CDEV_QP_ID
};

static struct event_crypto_adapter_test_params params;
static uint8_t crypto_adapter_setup_done;
static uint32_t slcore_id;
static int evdev;

static struct rte_mbuf *
alloc_fill_mbuf(struct rte_mempool *mpool, const uint8_t *data,
		size_t len, uint8_t blocksize)
{
	struct rte_mbuf *m = rte_pktmbuf_alloc(mpool);
	size_t t_len = len - (blocksize ? (len % blocksize) : 0);

	if (m) {
		char *dst = rte_pktmbuf_append(m, t_len);

		if (!dst) {
			rte_pktmbuf_free(m);
			return NULL;
		}

		rte_memcpy(dst, (const void *)data, t_len);
	}
	return m;
}

static int
send_recv_ev(struct rte_event *ev)
{
	struct rte_crypto_op *op;
	struct rte_event recv_ev;
	int ret;

	ret = rte_event_enqueue_burst(evdev, TEST_APP_PORT_ID, ev, NUM);
	TEST_ASSERT_EQUAL(ret, NUM,
			  "Failed to send event to crypto adapter\n");

	while (rte_event_dequeue_burst(evdev,
			TEST_APP_PORT_ID, &recv_ev, NUM, 0) == 0)
		rte_pause();

	op = recv_ev.event_ptr;
#if PKT_TRACE
	struct rte_mbuf *m = op->sym->m_src;
	rte_pktmbuf_dump(stdout, m, rte_pktmbuf_pkt_len(m));
#endif
	rte_pktmbuf_free(op->sym->m_src);
	rte_crypto_op_free(op);

	return TEST_SUCCESS;
}

static int
test_crypto_adapter_stats(void)
{
	struct rte_event_crypto_adapter_stats stats;

	rte_event_crypto_adapter_stats_get(TEST_ADAPTER_ID, &stats);
	printf(" +------------------------------------------------------+\n");
	printf(" + Crypto adapter stats for instance %u:\n", TEST_ADAPTER_ID);
	printf(" + Event port poll count          %" PRIx64 "\n",
		stats.event_poll_count);
	printf(" + Event dequeue count            %" PRIx64 "\n",
		stats.event_deq_count);
	printf(" + Cryptodev enqueue count        %" PRIx64 "\n",
		stats.crypto_enq_count);
	printf(" + Cryptodev enqueue failed count %" PRIx64 "\n",
		stats.crypto_enq_fail);
	printf(" + Cryptodev dequeue count        %" PRIx64 "\n",
		stats.crypto_deq_count);
	printf(" + Event enqueue count            %" PRIx64 "\n",
		stats.event_enq_count);
	printf(" + Event enqueue retry count      %" PRIx64 "\n",
		stats.event_enq_retry_count);
	printf(" + Event enqueue fail count       %" PRIx64 "\n",
		stats.event_enq_fail_count);
	printf(" +------------------------------------------------------+\n");

	rte_event_crypto_adapter_stats_reset(TEST_ADAPTER_ID);
	return TEST_SUCCESS;
}

static int
test_op_forward_mode(uint8_t session_less)
{
	struct rte_crypto_sym_xform cipher_xform;
	struct rte_cryptodev_sym_session *sess;
	union rte_event_crypto_metadata m_data;
	struct rte_crypto_sym_op *sym_op;
	struct rte_crypto_op *op;
	struct rte_mbuf *m;
	struct rte_event ev;
	uint32_t cap;
	int ret;

	memset(&m_data, 0, sizeof(m_data));

	m = alloc_fill_mbuf(params.mbuf_pool, text_64B, PACKET_LENGTH, 0);
	TEST_ASSERT_NOT_NULL(m, "Failed to allocate mbuf!\n");
#if PKT_TRACE
	rte_pktmbuf_dump(stdout, m, rte_pktmbuf_pkt_len(m));
#endif
	/* Setup Cipher Parameters */
	cipher_xform.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	cipher_xform.next = NULL;
	cipher_xform.cipher.algo = RTE_CRYPTO_CIPHER_NULL;

	op = rte_crypto_op_alloc(params.op_mpool,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	TEST_ASSERT_NOT_NULL(op,
		"Failed to allocate symmetric crypto operation struct\n");

	sym_op = op->sym;

	if (!session_less) {
		sess = rte_cryptodev_sym_session_create(
				params.session_mpool);
		TEST_ASSERT_NOT_NULL(sess, "Session creation failed\n");

		/* Create Crypto session*/
		ret = rte_cryptodev_sym_session_init(TEST_CDEV_ID, sess,
				&cipher_xform, params.session_priv_mpool);
		TEST_ASSERT_SUCCESS(ret, "Failed to init session\n");

		ret = rte_event_crypto_adapter_caps_get(TEST_ADAPTER_ID,
							evdev, &cap);
		TEST_ASSERT_SUCCESS(ret, "Failed to get adapter capabilities\n");

		if (cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_SESSION_PRIVATE_DATA) {
			/* Fill in private user data information */
			rte_memcpy(&m_data.response_info, &response_info,
				sizeof(response_info));
			rte_memcpy(&m_data.request_info, &request_info,
				sizeof(request_info));
			rte_cryptodev_sym_session_set_user_data(sess,
						&m_data, sizeof(m_data));
		}

		rte_crypto_op_attach_sym_session(op, sess);
	} else {
		struct rte_crypto_sym_xform *first_xform;

		rte_crypto_op_sym_xforms_alloc(op, NUM);
		op->sess_type = RTE_CRYPTO_OP_SESSIONLESS;
		first_xform = &cipher_xform;
		sym_op->xform = first_xform;
		uint32_t len = IV_OFFSET + MAXIMUM_IV_LENGTH +
				(sizeof(struct rte_crypto_sym_xform) * 2);
		op->private_data_offset = len;
		/* Fill in private data information */
		rte_memcpy(&m_data.response_info, &response_info,
			   sizeof(response_info));
		rte_memcpy(&m_data.request_info, &request_info,
			   sizeof(request_info));
		rte_memcpy((uint8_t *)op + len, &m_data, sizeof(m_data));
	}

	sym_op->m_src = m;
	sym_op->cipher.data.offset = 0;
	sym_op->cipher.data.length = PACKET_LENGTH;

	/* Fill in event info and update event_ptr with rte_crypto_op */
	memset(&ev, 0, sizeof(ev));
	ev.queue_id = TEST_CRYPTO_EV_QUEUE_ID;
	ev.sched_type = RTE_SCHED_TYPE_ATOMIC;
	ev.flow_id = 0xAABB;
	ev.event_ptr = op;

	ret = send_recv_ev(&ev);
	TEST_ASSERT_SUCCESS(ret, "Failed to send/receive event to "
				"crypto adapter\n");

	test_crypto_adapter_stats();

	return TEST_SUCCESS;
}

static int
map_adapter_service_core(void)
{
	uint32_t adapter_service_id;
	int ret;

	if (rte_event_crypto_adapter_service_id_get(TEST_ADAPTER_ID,
						&adapter_service_id) == 0) {
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
test_sessionless_with_op_forward_mode(void)
{
	uint32_t cap;
	int ret;

	ret = rte_event_crypto_adapter_caps_get(TEST_ADAPTER_ID, evdev, &cap);
	TEST_ASSERT_SUCCESS(ret, "Failed to get adapter capabilities\n");

	if (!(cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_FWD) &&
	    !(cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_NEW))
		map_adapter_service_core();
	else {
		if (!(cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_FWD))
			return TEST_SKIPPED;
	}

	TEST_ASSERT_SUCCESS(rte_event_crypto_adapter_start(TEST_ADAPTER_ID),
				"Failed to start event crypto adapter");

	ret = test_op_forward_mode(1);
	TEST_ASSERT_SUCCESS(ret, "Sessionless - FORWARD mode test failed\n");
	return TEST_SUCCESS;
}

static int
test_session_with_op_forward_mode(void)
{
	uint32_t cap;
	int ret;

	ret = rte_event_crypto_adapter_caps_get(TEST_ADAPTER_ID, evdev, &cap);
	TEST_ASSERT_SUCCESS(ret, "Failed to get adapter capabilities\n");

	if (!(cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_FWD) &&
	    !(cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_NEW))
		map_adapter_service_core();
	else {
		if (!(cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_FWD))
			return TEST_SKIPPED;
	}

	TEST_ASSERT_SUCCESS(rte_event_crypto_adapter_start(TEST_ADAPTER_ID
				), "Failed to start event crypto adapter");

	ret = test_op_forward_mode(0);
	TEST_ASSERT_SUCCESS(ret, "Session based - FORWARD mode test failed\n");
	return TEST_SUCCESS;
}

static int
send_op_recv_ev(struct rte_crypto_op *op)
{
	struct rte_crypto_op *recv_op;
	struct rte_event ev;
	int ret;

	ret = rte_cryptodev_enqueue_burst(TEST_CDEV_ID, TEST_CDEV_QP_ID,
					  &op, NUM);
	TEST_ASSERT_EQUAL(ret, NUM, "Failed to enqueue to cryptodev\n");
	memset(&ev, 0, sizeof(ev));

	while (rte_event_dequeue_burst(evdev,
		TEST_APP_PORT_ID, &ev, NUM, 0) == 0)
		rte_pause();

	recv_op = ev.event_ptr;
#if PKT_TRACE
	struct rte_mbuf *m = recv_op->sym->m_src;
	rte_pktmbuf_dump(stdout, m, rte_pktmbuf_pkt_len(m));
#endif
	rte_pktmbuf_free(recv_op->sym->m_src);
	rte_crypto_op_free(recv_op);

	return TEST_SUCCESS;
}

static int
test_op_new_mode(uint8_t session_less)
{
	struct rte_crypto_sym_xform cipher_xform;
	struct rte_cryptodev_sym_session *sess;
	union rte_event_crypto_metadata m_data;
	struct rte_crypto_sym_op *sym_op;
	struct rte_crypto_op *op;
	struct rte_mbuf *m;
	uint32_t cap;
	int ret;

	memset(&m_data, 0, sizeof(m_data));

	m = alloc_fill_mbuf(params.mbuf_pool, text_64B, PACKET_LENGTH, 0);
	TEST_ASSERT_NOT_NULL(m, "Failed to allocate mbuf!\n");
#if PKT_TRACE
	rte_pktmbuf_dump(stdout, m, rte_pktmbuf_pkt_len(m));
#endif
	/* Setup Cipher Parameters */
	cipher_xform.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	cipher_xform.next = NULL;
	cipher_xform.cipher.algo = RTE_CRYPTO_CIPHER_NULL;

	op = rte_crypto_op_alloc(params.op_mpool,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	TEST_ASSERT_NOT_NULL(op, "Failed to allocate crypto_op!\n");

	sym_op = op->sym;

	if (!session_less) {
		sess = rte_cryptodev_sym_session_create(
				params.session_mpool);
		TEST_ASSERT_NOT_NULL(sess, "Session creation failed\n");

		ret = rte_event_crypto_adapter_caps_get(TEST_ADAPTER_ID,
							evdev, &cap);
		TEST_ASSERT_SUCCESS(ret, "Failed to get adapter capabilities\n");

		if (cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_SESSION_PRIVATE_DATA) {
			/* Fill in private user data information */
			rte_memcpy(&m_data.response_info, &response_info,
				   sizeof(m_data));
			rte_cryptodev_sym_session_set_user_data(sess,
						&m_data, sizeof(m_data));
		}
		ret = rte_cryptodev_sym_session_init(TEST_CDEV_ID, sess,
				&cipher_xform, params.session_priv_mpool);
		TEST_ASSERT_SUCCESS(ret, "Failed to init session\n");

		rte_crypto_op_attach_sym_session(op, sess);
	} else {
		struct rte_crypto_sym_xform *first_xform;

		rte_crypto_op_sym_xforms_alloc(op, NUM);
		op->sess_type = RTE_CRYPTO_OP_SESSIONLESS;
		first_xform = &cipher_xform;
		sym_op->xform = first_xform;
		uint32_t len = IV_OFFSET + MAXIMUM_IV_LENGTH +
				(sizeof(struct rte_crypto_sym_xform) * 2);
		op->private_data_offset = len;
		/* Fill in private data information */
		rte_memcpy(&m_data.response_info, &response_info,
			   sizeof(m_data));
		rte_memcpy((uint8_t *)op + len, &m_data, sizeof(m_data));
	}

	sym_op->m_src = m;
	sym_op->cipher.data.offset = 0;
	sym_op->cipher.data.length = PACKET_LENGTH;

	ret = send_op_recv_ev(op);
	TEST_ASSERT_SUCCESS(ret, "Failed to enqueue op to cryptodev\n");

	test_crypto_adapter_stats();

	return TEST_SUCCESS;
}

static int
test_sessionless_with_op_new_mode(void)
{
	uint32_t cap;
	int ret;

	ret = rte_event_crypto_adapter_caps_get(TEST_ADAPTER_ID, evdev, &cap);
	TEST_ASSERT_SUCCESS(ret, "Failed to get adapter capabilities\n");

	if (!(cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_FWD) &&
	    !(cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_NEW))
		map_adapter_service_core();
	else {
		if (!(cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_NEW))
			return TEST_SKIPPED;
	}

	/* start the event crypto adapter */
	TEST_ASSERT_SUCCESS(rte_event_crypto_adapter_start(TEST_ADAPTER_ID),
				"Failed to start event crypto adapter");

	ret = test_op_new_mode(1);
	TEST_ASSERT_SUCCESS(ret, "Sessionless - NEW mode test failed\n");
	return TEST_SUCCESS;
}

static int
test_session_with_op_new_mode(void)
{
	uint32_t cap;
	int ret;

	ret = rte_event_crypto_adapter_caps_get(TEST_ADAPTER_ID, evdev, &cap);
	TEST_ASSERT_SUCCESS(ret, "Failed to get adapter capabilities\n");

	if (!(cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_FWD) &&
	    !(cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_NEW))
		map_adapter_service_core();
	else {
		if (!(cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_NEW))
			return TEST_SKIPPED;
	}

	TEST_ASSERT_SUCCESS(rte_event_crypto_adapter_start(TEST_ADAPTER_ID),
				"Failed to start event crypto adapter");

	ret = test_op_new_mode(0);
	TEST_ASSERT_SUCCESS(ret, "Session based - NEW mode test failed\n");
	return TEST_SUCCESS;
}

static int
configure_cryptodev(void)
{
	struct rte_cryptodev_qp_conf qp_conf;
	struct rte_cryptodev_config conf;
	struct rte_cryptodev_info info;
	unsigned int session_size;
	uint8_t nb_devs;
	int ret;

	params.mbuf_pool = rte_pktmbuf_pool_create(
			"CRYPTO_ADAPTER_MBUFPOOL",
			NUM_MBUFS, MBUF_CACHE_SIZE, 0, MBUF_SIZE,
			rte_socket_id());
	if (params.mbuf_pool == NULL) {
		RTE_LOG(ERR, USER1, "Can't create CRYPTO_MBUFPOOL\n");
		return TEST_FAILED;
	}

	params.op_mpool = rte_crypto_op_pool_create(
			"EVENT_CRYPTO_SYM_OP_POOL",
			RTE_CRYPTO_OP_TYPE_SYMMETRIC,
			NUM_MBUFS, MBUF_CACHE_SIZE,
			DEFAULT_NUM_XFORMS *
			sizeof(struct rte_crypto_sym_xform) +
			MAXIMUM_IV_LENGTH,
			rte_socket_id());
	if (params.op_mpool == NULL) {
		RTE_LOG(ERR, USER1, "Can't create CRYPTO_OP_POOL\n");
		return TEST_FAILED;
	}

	/* Create a NULL crypto device */
	nb_devs = rte_cryptodev_device_count_by_driver(
			rte_cryptodev_driver_id_get(
			RTE_STR(CRYPTODEV_NAME_NULL_PMD)));
	if (!nb_devs) {
		ret = rte_vdev_init(
			RTE_STR(CRYPTODEV_NAME_NULL_PMD), NULL);

		TEST_ASSERT(ret == 0, "Failed to create pmd:%s instance\n",
			    RTE_STR(CRYPTODEV_NAME_NULL_PMD));
	}

	nb_devs = rte_cryptodev_count();
	if (!nb_devs) {
		RTE_LOG(ERR, USER1, "No crypto devices found!\n");
		return TEST_FAILED;
	}

	/*
	 * Create mempool with maximum number of sessions * 2,
	 * to include the session headers & private data
	 */
	session_size = rte_cryptodev_sym_get_private_session_size(TEST_CDEV_ID);
	session_size += sizeof(union rte_event_crypto_metadata);

	params.session_mpool = rte_cryptodev_sym_session_pool_create(
			"CRYPTO_ADAPTER_SESSION_MP",
			MAX_NB_SESSIONS, 0, 0,
			sizeof(union rte_event_crypto_metadata),
			SOCKET_ID_ANY);
	TEST_ASSERT_NOT_NULL(params.session_mpool,
			"session mempool allocation failed\n");

	params.session_priv_mpool = rte_mempool_create(
				"CRYPTO_AD_SESS_MP_PRIV",
				MAX_NB_SESSIONS,
				session_size,
				0, 0, NULL, NULL, NULL,
				NULL, SOCKET_ID_ANY,
				0);
	TEST_ASSERT_NOT_NULL(params.session_priv_mpool,
			"session mempool allocation failed\n");

	rte_cryptodev_info_get(TEST_CDEV_ID, &info);
	conf.nb_queue_pairs = info.max_nb_queue_pairs;
	conf.socket_id = SOCKET_ID_ANY;
	conf.ff_disable = RTE_CRYPTODEV_FF_SECURITY;

	TEST_ASSERT_SUCCESS(rte_cryptodev_configure(TEST_CDEV_ID, &conf),
			"Failed to configure cryptodev %u with %u qps\n",
			TEST_CDEV_ID, conf.nb_queue_pairs);

	qp_conf.nb_descriptors = DEFAULT_NUM_OPS_INFLIGHT;
	qp_conf.mp_session = params.session_mpool;
	qp_conf.mp_session_private = params.session_priv_mpool;

	TEST_ASSERT_SUCCESS(rte_cryptodev_queue_pair_setup(
			TEST_CDEV_ID, TEST_CDEV_QP_ID, &qp_conf,
			rte_cryptodev_socket_id(TEST_CDEV_ID)),
			"Failed to setup queue pair %u on cryptodev %u\n",
			TEST_CDEV_QP_ID, TEST_CDEV_ID);

	return TEST_SUCCESS;
}

static inline void
evdev_set_conf_values(struct rte_event_dev_config *dev_conf,
			struct rte_event_dev_info *info)
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
	int ret;
	uint8_t qid;

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
	ret = rte_event_dev_attr_get(evdev, RTE_EVENT_DEV_ATTR_QUEUE_COUNT,
					&queue_count);
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

	qid = TEST_CRYPTO_EV_QUEUE_ID;
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
test_crypto_adapter_free(void)
{
	rte_event_crypto_adapter_free(TEST_ADAPTER_ID);
}

static int
test_crypto_adapter_create(void)
{
	struct rte_event_port_conf conf = {
		.dequeue_depth = 8,
		.enqueue_depth = 8,
		.new_event_threshold = 1200,
	};
	int ret;

	/* Create adapter with default port creation callback */
	ret = rte_event_crypto_adapter_create(TEST_ADAPTER_ID,
					      TEST_CDEV_ID,
					      &conf, 0);
	TEST_ASSERT_SUCCESS(ret, "Failed to create event crypto adapter\n");

	return TEST_SUCCESS;
}

static int
test_crypto_adapter_qp_add_del(void)
{
	uint32_t cap;
	int ret;

	ret = rte_event_crypto_adapter_caps_get(TEST_ADAPTER_ID, evdev, &cap);
	TEST_ASSERT_SUCCESS(ret, "Failed to get adapter capabilities\n");

	if (cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_QP_EV_BIND) {
		ret = rte_event_crypto_adapter_queue_pair_add(TEST_ADAPTER_ID,
				TEST_CDEV_ID, TEST_CDEV_QP_ID, &response_info);
	} else
		ret = rte_event_crypto_adapter_queue_pair_add(TEST_ADAPTER_ID,
					TEST_CDEV_ID, TEST_CDEV_QP_ID, NULL);

	TEST_ASSERT_SUCCESS(ret, "Failed to create add queue pair\n");

	ret = rte_event_crypto_adapter_queue_pair_del(TEST_ADAPTER_ID,
					TEST_CDEV_ID, TEST_CDEV_QP_ID);
	TEST_ASSERT_SUCCESS(ret, "Failed to delete add queue pair\n");

	return TEST_SUCCESS;
}

static int
configure_event_crypto_adapter(enum rte_event_crypto_adapter_mode mode)
{
	struct rte_event_port_conf conf = {
		.dequeue_depth = 8,
		.enqueue_depth = 8,
		.new_event_threshold = 1200,
	};

	uint32_t cap;
	int ret;

	/* Create adapter with default port creation callback */
	ret = rte_event_crypto_adapter_create(TEST_ADAPTER_ID,
					      TEST_CDEV_ID,
					      &conf, mode);
	TEST_ASSERT_SUCCESS(ret, "Failed to create event crypto adapter\n");

	ret = rte_event_crypto_adapter_caps_get(TEST_ADAPTER_ID, evdev, &cap);
	TEST_ASSERT_SUCCESS(ret, "Failed to get adapter capabilities\n");

	if (cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_QP_EV_BIND) {
		ret = rte_event_crypto_adapter_queue_pair_add(TEST_ADAPTER_ID,
				TEST_CDEV_ID, TEST_CDEV_QP_ID, &response_info);
	} else
		ret = rte_event_crypto_adapter_queue_pair_add(TEST_ADAPTER_ID,
				TEST_CDEV_ID, TEST_CDEV_QP_ID, NULL);

	TEST_ASSERT_SUCCESS(ret, "Failed to add queue pair\n");

	ret = rte_event_crypto_adapter_event_port_get(TEST_ADAPTER_ID,
				&params.crypto_event_port_id);
	TEST_ASSERT_SUCCESS(ret, "Failed to get event port\n");

	return TEST_SUCCESS;
}

static void
test_crypto_adapter_stop(void)
{
	uint32_t evdev_service_id, adapter_service_id;

	/* retrieve service ids & stop services */
	if (rte_event_crypto_adapter_service_id_get(TEST_ADAPTER_ID,
						&adapter_service_id) == 0) {
		rte_service_runstate_set(adapter_service_id, 0);
		rte_service_lcore_stop(slcore_id);
		rte_service_lcore_del(slcore_id);
		rte_event_crypto_adapter_stop(TEST_ADAPTER_ID);
	}

	if (rte_event_dev_service_id_get(evdev, &evdev_service_id) == 0) {
		rte_service_runstate_set(evdev_service_id, 0);
		rte_service_lcore_stop(slcore_id);
		rte_service_lcore_del(slcore_id);
		rte_event_dev_stop(evdev);
	}
}

static int
test_crypto_adapter_conf(enum rte_event_crypto_adapter_mode mode)
{
	uint32_t evdev_service_id;
	uint8_t qid;
	int ret;

	if (!crypto_adapter_setup_done) {
		ret = configure_event_crypto_adapter(mode);
		if (!ret) {
			qid = TEST_CRYPTO_EV_QUEUE_ID;
			ret = rte_event_port_link(evdev,
				params.crypto_event_port_id, &qid, NULL, 1);
			TEST_ASSERT(ret >= 0, "Failed to link queue %d "
					"port=%u\n", qid,
					params.crypto_event_port_id);
		}
		crypto_adapter_setup_done = 1;
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

	return TEST_SUCCESS;
}

static int
test_crypto_adapter_conf_op_forward_mode(void)
{
	enum rte_event_crypto_adapter_mode mode;

	mode = RTE_EVENT_CRYPTO_ADAPTER_OP_FORWARD;
	TEST_ASSERT_SUCCESS(test_crypto_adapter_conf(mode),
				"Failed to config crypto adapter");

	return TEST_SUCCESS;
}

static int
test_crypto_adapter_conf_op_new_mode(void)
{
	enum rte_event_crypto_adapter_mode mode;

	mode = RTE_EVENT_CRYPTO_ADAPTER_OP_NEW;
	TEST_ASSERT_SUCCESS(test_crypto_adapter_conf(mode),
				"Failed to config crypto adapter");

	return TEST_SUCCESS;
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

	/* Setup and start crypto device. */
	ret = configure_cryptodev();
	TEST_ASSERT_SUCCESS(ret, "cryptodev initialization failed\n");

	return TEST_SUCCESS;
}

static void
crypto_teardown(void)
{
	/* Free mbuf mempool */
	if (params.mbuf_pool != NULL) {
		RTE_LOG(DEBUG, USER1, "CRYPTO_ADAPTER_MBUFPOOL count %u\n",
		rte_mempool_avail_count(params.mbuf_pool));
		rte_mempool_free(params.mbuf_pool);
		params.mbuf_pool = NULL;
	}

	/* Free session mempool */
	if (params.session_mpool != NULL) {
		RTE_LOG(DEBUG, USER1, "CRYPTO_ADAPTER_SESSION_MP count %u\n",
		rte_mempool_avail_count(params.session_mpool));
		rte_mempool_free(params.session_mpool);
		params.session_mpool = NULL;
	}
	if (params.session_priv_mpool != NULL) {
		rte_mempool_avail_count(params.session_priv_mpool);
		rte_mempool_free(params.session_priv_mpool);
		params.session_priv_mpool = NULL;
	}

	/* Free ops mempool */
	if (params.op_mpool != NULL) {
		RTE_LOG(DEBUG, USER1, "EVENT_CRYPTO_SYM_OP_POOL count %u\n",
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
	crypto_teardown();
	eventdev_teardown();
}

static struct unit_test_suite functional_testsuite = {
	.suite_name = "Event crypto adapter test suite",
	.setup = testsuite_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {

		TEST_CASE_ST(NULL, test_crypto_adapter_free,
				test_crypto_adapter_create),

		TEST_CASE_ST(test_crypto_adapter_create,
				test_crypto_adapter_free,
				test_crypto_adapter_qp_add_del),

		TEST_CASE_ST(test_crypto_adapter_create,
				test_crypto_adapter_free,
				test_crypto_adapter_stats),

		TEST_CASE_ST(test_crypto_adapter_conf_op_forward_mode,
				test_crypto_adapter_stop,
				test_session_with_op_forward_mode),

		TEST_CASE_ST(test_crypto_adapter_conf_op_forward_mode,
				test_crypto_adapter_stop,
				test_sessionless_with_op_forward_mode),

		TEST_CASE_ST(test_crypto_adapter_conf_op_new_mode,
				test_crypto_adapter_stop,
				test_session_with_op_new_mode),

		TEST_CASE_ST(test_crypto_adapter_conf_op_new_mode,
				test_crypto_adapter_stop,
				test_sessionless_with_op_new_mode),

		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static int
test_event_crypto_adapter(void)
{
	return unit_test_suite_runner(&functional_testsuite);
}

REGISTER_TEST_COMMAND(event_crypto_adapter_autotest,
		test_event_crypto_adapter);
