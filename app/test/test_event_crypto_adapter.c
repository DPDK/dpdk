/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation.
 * All rights reserved.
 */

#include "test.h"
#include <string.h>
#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_cryptodev.h>

#ifdef RTE_EXEC_ENV_WINDOWS
static int
test_event_crypto_adapter(void)
{
	printf("event_crypto_adapter not supported on Windows, skipping test\n");
	return TEST_SKIPPED;
}

#else

#include <rte_eventdev.h>
#include <rte_bus_vdev.h>
#include <rte_service.h>
#include <rte_event_crypto_adapter.h>

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
#define DATA_SIZE		512
struct modex_test_data {
	enum rte_crypto_asym_xform_type xform_type;
	struct {
		uint8_t data[DATA_SIZE];
		uint16_t len;
	} base;
	struct {
		uint8_t data[DATA_SIZE];
		uint16_t len;
	} exponent;
	struct {
		uint8_t data[DATA_SIZE];
		uint16_t len;
	} modulus;
	struct {
		uint8_t data[DATA_SIZE];
		uint16_t len;
	} reminder;
	uint16_t result_len;
};

static struct
modex_test_data modex_test_case = {
	.xform_type = RTE_CRYPTO_ASYM_XFORM_MODEX,
	.base = {
		.data = {
			0xF8, 0xBA, 0x1A, 0x55, 0xD0, 0x2F, 0x85,
			0xAE, 0x96, 0x7B, 0xB6, 0x2F, 0xB6, 0xCD,
			0xA8, 0xEB, 0x7E, 0x78, 0xA0, 0x50
		},
		.len = 20,
	},
	.exponent = {
		.data = {
			0x01, 0x00, 0x01
		},
		.len = 3,
	},
	.reminder = {
		.data = {
			0x2C, 0x60, 0x75, 0x45, 0x98, 0x9D, 0xE0, 0x72,
			0xA0, 0x9D, 0x3A, 0x9E, 0x03, 0x38, 0x73, 0x3C,
			0x31, 0x83, 0x04, 0xFE, 0x75, 0x43, 0xE6, 0x17,
			0x5C, 0x01, 0x29, 0x51, 0x69, 0x33, 0x62, 0x2D,
			0x78, 0xBE, 0xAE, 0xC4, 0xBC, 0xDE, 0x7E, 0x2C,
			0x77, 0x84, 0xF2, 0xC5, 0x14, 0xB5, 0x2F, 0xF7,
			0xC5, 0x94, 0xEF, 0x86, 0x75, 0x75, 0xB5, 0x11,
			0xE5, 0x0E, 0x0A, 0x29, 0x76, 0xE2, 0xEA, 0x32,
			0x0E, 0x43, 0x77, 0x7E, 0x2C, 0x27, 0xAC, 0x3B,
			0x86, 0xA5, 0xDB, 0xC9, 0x48, 0x40, 0xE8, 0x99,
			0x9A, 0x0A, 0x3D, 0xD6, 0x74, 0xFA, 0x2E, 0x2E,
			0x5B, 0xAF, 0x8C, 0x99, 0x44, 0x2A, 0x67, 0x38,
			0x27, 0x41, 0x59, 0x9D, 0xB8, 0x51, 0xC9, 0xF7,
			0x43, 0x61, 0x31, 0x6E, 0xF1, 0x25, 0x38, 0x7F,
			0xAE, 0xC6, 0xD0, 0xBB, 0x29, 0x76, 0x3F, 0x46,
			0x2E, 0x1B, 0xE4, 0x67, 0x71, 0xE3, 0x87, 0x5A
		},
		.len = 128,
	},
	.modulus = {
		.data = {
			0xb3, 0xa1, 0xaf, 0xb7, 0x13, 0x08, 0x00, 0x0a,
			0x35, 0xdc, 0x2b, 0x20, 0x8d, 0xa1, 0xb5, 0xce,
			0x47, 0x8a, 0xc3, 0x80, 0xf4, 0x7d, 0x4a, 0xa2,
			0x62, 0xfd, 0x61, 0x7f, 0xb5, 0xa8, 0xde, 0x0a,
			0x17, 0x97, 0xa0, 0xbf, 0xdf, 0x56, 0x5a, 0x3d,
			0x51, 0x56, 0x4f, 0x70, 0x70, 0x3f, 0x63, 0x6a,
			0x44, 0x5b, 0xad, 0x84, 0x0d, 0x3f, 0x27, 0x6e,
			0x3b, 0x34, 0x91, 0x60, 0x14, 0xb9, 0xaa, 0x72,
			0xfd, 0xa3, 0x64, 0xd2, 0x03, 0xa7, 0x53, 0x87,
			0x9e, 0x88, 0x0b, 0xc1, 0x14, 0x93, 0x1a, 0x62,
			0xff, 0xb1, 0x5d, 0x74, 0xcd, 0x59, 0x63, 0x18,
			0x11, 0x3d, 0x4f, 0xba, 0x75, 0xd4, 0x33, 0x4e,
			0x23, 0x6b, 0x7b, 0x57, 0x44, 0xe1, 0xd3, 0x03,
			0x13, 0xa6, 0xf0, 0x8b, 0x60, 0xb0, 0x9e, 0xee,
			0x75, 0x08, 0x9d, 0x71, 0x63, 0x13, 0xcb, 0xa6,
			0x81, 0x92, 0x14, 0x03, 0x22, 0x2d, 0xde, 0x55
		},
		.len = 128,
	},
	.result_len = 128,
};

struct event_crypto_adapter_test_params {
	struct rte_mempool *mbuf_pool;
	struct rte_mempool *op_mpool;
	struct rte_mempool *asym_op_mpool;
	struct rte_mempool *session_mpool;
	struct rte_mempool *asym_sess_mpool;
	struct rte_cryptodev_config *config;
	uint8_t crypto_event_port_id;
	uint8_t internal_port_op_fwd;
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

	if (params.internal_port_op_fwd)
		ret = rte_event_crypto_adapter_enqueue(evdev, TEST_APP_PORT_ID,
						       ev, NUM);
	else
		ret = rte_event_enqueue_burst(evdev, TEST_APP_PORT_ID, ev, NUM);
	TEST_ASSERT_EQUAL(ret, NUM, "Failed to send event to crypto adapter\n");

	while (rte_event_dequeue_burst(evdev,
			TEST_APP_PORT_ID, &recv_ev, NUM, 0) == 0)
		rte_pause();

	op = recv_ev.event_ptr;
	if (op->type == RTE_CRYPTO_OP_TYPE_SYMMETRIC) {
#if PKT_TRACE
		struct rte_mbuf *m = op->sym->m_src;
		rte_pktmbuf_dump(stdout, m, rte_pktmbuf_pkt_len(m));
#endif
		rte_pktmbuf_free(op->sym->m_src);
	} else {
		uint8_t *data_expected = NULL, *data_received = NULL;
		uint32_t data_size;

		data_expected = modex_test_case.reminder.data;
		data_received = op->asym->modex.result.data;
		data_size = op->asym->modex.result.length;
		ret = memcmp(data_expected, data_received, data_size);
		TEST_ASSERT_EQUAL(ret, 0,
				"Data mismatch for asym crypto adapter\n");
		rte_free(op->asym->modex.result.data);
	}
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
test_crypto_adapter_params(void)
{
	int err, rc;
	struct rte_event_crypto_adapter_runtime_params in_params;
	struct rte_event_crypto_adapter_runtime_params out_params;
	uint32_t cap;
	struct rte_event_crypto_adapter_queue_conf queue_conf = {
		.ev = response_info,
	};

	err = rte_event_crypto_adapter_caps_get(evdev, TEST_CDEV_ID, &cap);
	TEST_ASSERT_SUCCESS(err, "Failed to get adapter capabilities\n");

	if (cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_QP_EV_BIND) {
		err = rte_event_crypto_adapter_queue_pair_add(TEST_ADAPTER_ID,
				TEST_CDEV_ID, TEST_CDEV_QP_ID, &queue_conf);
	} else
		err = rte_event_crypto_adapter_queue_pair_add(TEST_ADAPTER_ID,
					TEST_CDEV_ID, TEST_CDEV_QP_ID, NULL);

	TEST_ASSERT_SUCCESS(err, "Failed to add queue pair\n");

	err = rte_event_crypto_adapter_runtime_params_init(&in_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	err = rte_event_crypto_adapter_runtime_params_init(&out_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	/* Case 1: Get the default value of mbufs processed by adapter */
	err = rte_event_crypto_adapter_runtime_params_get(TEST_ADAPTER_ID,
							  &out_params);
	if (err == -ENOTSUP) {
		rc = TEST_SKIPPED;
		goto queue_pair_del;
	}
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	/* Case 2: Set max_nb = 32 (=BATCH_SEIZE) */
	in_params.max_nb = 32;

	err = rte_event_crypto_adapter_runtime_params_set(TEST_ADAPTER_ID,
							  &in_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_crypto_adapter_runtime_params_get(TEST_ADAPTER_ID,
							  &out_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	TEST_ASSERT(in_params.max_nb == out_params.max_nb, "Expected %u got %u",
		    in_params.max_nb, out_params.max_nb);

	/* Case 3: Set max_nb = 192 */
	in_params.max_nb = 192;

	err = rte_event_crypto_adapter_runtime_params_set(TEST_ADAPTER_ID,
							  &in_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_crypto_adapter_runtime_params_get(TEST_ADAPTER_ID,
							  &out_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	TEST_ASSERT(in_params.max_nb == out_params.max_nb, "Expected %u got %u",
		    in_params.max_nb, out_params.max_nb);

	/* Case 4: Set max_nb = 256 */
	in_params.max_nb = 256;

	err = rte_event_crypto_adapter_runtime_params_set(TEST_ADAPTER_ID,
							  &in_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_crypto_adapter_runtime_params_get(TEST_ADAPTER_ID,
							  &out_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	TEST_ASSERT(in_params.max_nb == out_params.max_nb, "Expected %u got %u",
		    in_params.max_nb, out_params.max_nb);

	/* Case 5: Set max_nb = 30(<BATCH_SIZE) */
	in_params.max_nb = 30;

	err = rte_event_crypto_adapter_runtime_params_set(TEST_ADAPTER_ID,
							  &in_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_crypto_adapter_runtime_params_get(TEST_ADAPTER_ID,
							  &out_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	TEST_ASSERT(in_params.max_nb == out_params.max_nb, "Expected %u got %u",
		    in_params.max_nb, out_params.max_nb);

	/* Case 6: Set max_nb = 512 */
	in_params.max_nb = 512;

	err = rte_event_crypto_adapter_runtime_params_set(TEST_ADAPTER_ID,
							  &in_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_crypto_adapter_runtime_params_get(TEST_ADAPTER_ID,
							  &out_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	TEST_ASSERT(in_params.max_nb == out_params.max_nb, "Expected %u got %u",
		    in_params.max_nb, out_params.max_nb);

	rc = TEST_SUCCESS;
queue_pair_del:
	err = rte_event_crypto_adapter_queue_pair_del(TEST_ADAPTER_ID,
					TEST_CDEV_ID, TEST_CDEV_QP_ID);
	TEST_ASSERT_SUCCESS(err, "Failed to delete add queue pair\n");

	return rc;
}

static int
test_op_forward_mode(uint8_t session_less)
{
	struct rte_crypto_sym_xform cipher_xform;
	union rte_event_crypto_metadata m_data;
	struct rte_crypto_sym_op *sym_op;
	struct rte_crypto_op *op;
	struct rte_mbuf *m;
	struct rte_event ev;
	uint32_t cap;
	void *sess;
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
	cipher_xform.cipher.op = RTE_CRYPTO_CIPHER_OP_ENCRYPT;

	op = rte_crypto_op_alloc(params.op_mpool,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	TEST_ASSERT_NOT_NULL(op,
		"Failed to allocate symmetric crypto operation struct\n");

	sym_op = op->sym;

	if (!session_less) {
		sess = rte_cryptodev_sym_session_create(TEST_CDEV_ID,
				&cipher_xform, params.session_mpool);
		TEST_ASSERT_NOT_NULL(sess, "Session creation failed\n");

		ret = rte_event_crypto_adapter_caps_get(evdev, TEST_CDEV_ID,
							&cap);
		TEST_ASSERT_SUCCESS(ret, "Failed to get adapter capabilities\n");

		if (cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_SESSION_PRIVATE_DATA) {
			/* Fill in private user data information */
			m_data.request_info.cdev_id = request_info.cdev_id;
			m_data.request_info.queue_pair_id =
				request_info.queue_pair_id;
			m_data.response_info.event = response_info.event;
			rte_cryptodev_session_event_mdata_set(TEST_CDEV_ID,
					sess, RTE_CRYPTO_OP_TYPE_SYMMETRIC,
					RTE_CRYPTO_OP_WITH_SESSION,
					&m_data, sizeof(m_data));
		}

		rte_crypto_op_attach_sym_session(op, sess);
	} else {
		struct rte_crypto_sym_xform *first_xform;

		rte_crypto_op_sym_xforms_alloc(op, NUM);
		op->sess_type = RTE_CRYPTO_OP_SESSIONLESS;
		first_xform = &cipher_xform;
		sym_op->xform = first_xform;
		uint32_t len = IV_OFFSET + MAXIMUM_IV_LENGTH;
		op->private_data_offset = len;
		/* Fill in private data information */
		m_data.request_info.cdev_id = request_info.cdev_id;
		m_data.request_info.queue_pair_id = request_info.queue_pair_id;
		m_data.response_info.event = response_info.event;
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

	ret = rte_event_crypto_adapter_caps_get(evdev, TEST_CDEV_ID, &cap);
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

	ret = rte_event_crypto_adapter_caps_get(evdev, TEST_CDEV_ID, &cap);
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
test_asym_op_forward_mode(uint8_t session_less)
{
	const struct rte_cryptodev_asymmetric_xform_capability *capability;
	struct rte_cryptodev_asym_capability_idx cap_idx;
	struct rte_crypto_asym_xform xform_tc;
	union rte_event_crypto_metadata m_data;
	struct rte_cryptodev_info dev_info;
	struct rte_crypto_asym_op *asym_op;
	struct rte_crypto_op *op;
	uint8_t input[4096] = {0};
	uint8_t *result = NULL;
	struct rte_event ev;
	void *sess = NULL;
	uint32_t cap;
	int ret;

	memset(&m_data, 0, sizeof(m_data));

	rte_cryptodev_info_get(TEST_CDEV_ID, &dev_info);
	if (session_less && !(dev_info.feature_flags &
				RTE_CRYPTODEV_FF_ASYM_SESSIONLESS)) {
		RTE_LOG(INFO, USER1,
			"Device doesn't support Asym sessionless ops. Test Skipped\n");
		return TEST_SKIPPED;
	}
	/* Setup Cipher Parameters */
	xform_tc.next = NULL;
	xform_tc.xform_type = RTE_CRYPTO_ASYM_XFORM_MODEX;
	cap_idx.type = xform_tc.xform_type;
	capability = rte_cryptodev_asym_capability_get(TEST_CDEV_ID, &cap_idx);

	if (capability == NULL) {
		RTE_LOG(INFO, USER1,
			"Device doesn't support MODEX. Test Skipped\n");
		return TEST_SKIPPED;
	}

	op = rte_crypto_op_alloc(params.asym_op_mpool,
			RTE_CRYPTO_OP_TYPE_ASYMMETRIC);
	TEST_ASSERT_NOT_NULL(op,
		"Failed to allocate asymmetric crypto operation struct\n");

	asym_op = op->asym;

	result = rte_zmalloc(NULL, modex_test_case.result_len, 0);
	xform_tc.modex.modulus.data = modex_test_case.modulus.data;
	xform_tc.modex.modulus.length = modex_test_case.modulus.len;
	xform_tc.modex.exponent.data = modex_test_case.exponent.data;
	xform_tc.modex.exponent.length = modex_test_case.exponent.len;
	memcpy(input, modex_test_case.base.data,
		modex_test_case.base.len);
	asym_op->modex.base.data = input;
	asym_op->modex.base.length = modex_test_case.base.len;
	asym_op->modex.result.data = result;
	asym_op->modex.result.length = modex_test_case.result_len;
	if (rte_cryptodev_asym_xform_capability_check_modlen(capability,
			xform_tc.modex.modulus.length)) {
		RTE_LOG(INFO, USER1,
			"line %u FAILED: %s", __LINE__,
			"Invalid MODULUS length specified");
		return TEST_FAILED;
	}

	if (!session_less) {
		/* Create Crypto session*/
		ret = rte_cryptodev_asym_session_create(TEST_CDEV_ID,
				&xform_tc, params.asym_sess_mpool, &sess);
		TEST_ASSERT_SUCCESS(ret, "Failed to init session\n");

		ret = rte_event_crypto_adapter_caps_get(evdev, TEST_CDEV_ID,
							&cap);
		TEST_ASSERT_SUCCESS(ret, "Failed to get adapter capabilities\n");

		if (cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_SESSION_PRIVATE_DATA) {
			/* Fill in private user data information */
			m_data.request_info.cdev_id = request_info.cdev_id;
			m_data.request_info.queue_pair_id =
				request_info.queue_pair_id;
			m_data.response_info.event = response_info.event;
			rte_cryptodev_session_event_mdata_set(TEST_CDEV_ID,
					sess, RTE_CRYPTO_OP_TYPE_ASYMMETRIC,
					RTE_CRYPTO_OP_WITH_SESSION,
					&m_data, sizeof(m_data));
		}

		rte_crypto_op_attach_asym_session(op, sess);
	} else {
		op->sess_type = RTE_CRYPTO_OP_SESSIONLESS;
		asym_op->xform = &xform_tc;
		op->private_data_offset = (sizeof(struct rte_crypto_op) +
				sizeof(struct rte_crypto_asym_op) +
				DEFAULT_NUM_XFORMS *
				sizeof(struct rte_crypto_asym_xform));
		/* Fill in private data information */
		m_data.request_info.cdev_id = request_info.cdev_id;
		m_data.request_info.queue_pair_id = request_info.queue_pair_id;
		m_data.response_info.event = response_info.event;
		rte_memcpy((uint8_t *)op + op->private_data_offset,
				&m_data, sizeof(m_data));
	}
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
test_asym_sessionless_with_op_forward_mode(void)
{
	uint32_t cap;
	int ret;

	ret = rte_event_crypto_adapter_caps_get(evdev, TEST_CDEV_ID, &cap);
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

	return test_asym_op_forward_mode(1);
}

static int
test_asym_session_with_op_forward_mode(void)
{
	uint32_t cap;
	int ret;

	ret = rte_event_crypto_adapter_caps_get(evdev, TEST_CDEV_ID, &cap);
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

	return test_asym_op_forward_mode(0);
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
	if (recv_op->type == RTE_CRYPTO_OP_TYPE_SYMMETRIC) {
#if PKT_TRACE
		struct rte_mbuf *m = recv_op->sym->m_src;
		rte_pktmbuf_dump(stdout, m, rte_pktmbuf_pkt_len(m));
#endif
		rte_pktmbuf_free(recv_op->sym->m_src);
	} else {
		uint8_t *data_expected = NULL, *data_received = NULL;
		uint32_t data_size;

		data_expected = modex_test_case.reminder.data;
		data_received = op->asym->modex.result.data;
		data_size = op->asym->modex.result.length;
		ret = memcmp(data_expected, data_received, data_size);
		TEST_ASSERT_EQUAL(ret, 0,
				"Data mismatch for asym crypto adapter\n");
		rte_free(op->asym->modex.result.data);
	}
	rte_crypto_op_free(recv_op);

	return TEST_SUCCESS;
}

static int
test_op_new_mode(uint8_t session_less)
{
	struct rte_crypto_sym_xform cipher_xform;
	union rte_event_crypto_metadata m_data;
	struct rte_crypto_sym_op *sym_op;
	struct rte_crypto_op *op;
	struct rte_mbuf *m;
	uint32_t cap;
	void *sess;
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
	cipher_xform.cipher.op = RTE_CRYPTO_CIPHER_OP_ENCRYPT;

	op = rte_crypto_op_alloc(params.op_mpool,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	TEST_ASSERT_NOT_NULL(op, "Failed to allocate crypto_op!\n");

	sym_op = op->sym;

	if (!session_less) {
		sess = rte_cryptodev_sym_session_create(TEST_CDEV_ID,
				&cipher_xform, params.session_mpool);
		TEST_ASSERT_NOT_NULL(sess, "Session creation failed\n");

		ret = rte_event_crypto_adapter_caps_get(evdev, TEST_CDEV_ID,
							&cap);
		TEST_ASSERT_SUCCESS(ret, "Failed to get adapter capabilities\n");

		if (cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_SESSION_PRIVATE_DATA) {
			/* Fill in private user data information */
			m_data.response_info.event = response_info.event;
			rte_cryptodev_session_event_mdata_set(TEST_CDEV_ID,
					sess, RTE_CRYPTO_OP_TYPE_SYMMETRIC,
					RTE_CRYPTO_OP_WITH_SESSION,
					&m_data, sizeof(m_data));
		}

		rte_crypto_op_attach_sym_session(op, sess);
	} else {
		struct rte_crypto_sym_xform *first_xform;

		rte_crypto_op_sym_xforms_alloc(op, NUM);
		op->sess_type = RTE_CRYPTO_OP_SESSIONLESS;
		first_xform = &cipher_xform;
		sym_op->xform = first_xform;
		uint32_t len = IV_OFFSET + MAXIMUM_IV_LENGTH;
		op->private_data_offset = len;
		/* Fill in private data information */
		m_data.response_info.event = response_info.event;
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

	ret = rte_event_crypto_adapter_caps_get(evdev, TEST_CDEV_ID, &cap);
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

	ret = rte_event_crypto_adapter_caps_get(evdev, TEST_CDEV_ID, &cap);
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
test_asym_op_new_mode(uint8_t session_less)
{
	const struct rte_cryptodev_asymmetric_xform_capability *capability;
	struct rte_cryptodev_asym_capability_idx cap_idx;
	struct rte_crypto_asym_xform xform_tc;
	union rte_event_crypto_metadata m_data;
	struct rte_cryptodev_info dev_info;
	struct rte_crypto_asym_op *asym_op;
	struct rte_crypto_op *op;
	uint8_t input[4096] = {0};
	uint8_t *result = NULL;
	void *sess = NULL;
	uint32_t cap;
	int ret;

	memset(&m_data, 0, sizeof(m_data));

	rte_cryptodev_info_get(TEST_CDEV_ID, &dev_info);
	if (session_less && !(dev_info.feature_flags &
				RTE_CRYPTODEV_FF_ASYM_SESSIONLESS)) {
		RTE_LOG(INFO, USER1,
			"Device doesn't support Asym sessionless ops. Test Skipped\n");
		return TEST_SKIPPED;
	}
	/* Setup Cipher Parameters */
	xform_tc.next = NULL;
	xform_tc.xform_type = RTE_CRYPTO_ASYM_XFORM_MODEX;
	cap_idx.type = xform_tc.xform_type;
	capability = rte_cryptodev_asym_capability_get(TEST_CDEV_ID, &cap_idx);

	if (capability == NULL) {
		RTE_LOG(INFO, USER1,
			"Device doesn't support MODEX. Test Skipped\n");
		return TEST_SKIPPED;
	}

	op = rte_crypto_op_alloc(params.asym_op_mpool,
			RTE_CRYPTO_OP_TYPE_ASYMMETRIC);
	TEST_ASSERT_NOT_NULL(op, "Failed to allocate asym crypto_op!\n");

	asym_op = op->asym;

	result = rte_zmalloc(NULL, modex_test_case.result_len, 0);
	xform_tc.modex.modulus.data = modex_test_case.modulus.data;
	xform_tc.modex.modulus.length = modex_test_case.modulus.len;
	xform_tc.modex.exponent.data = modex_test_case.exponent.data;
	xform_tc.modex.exponent.length = modex_test_case.exponent.len;
	memcpy(input, modex_test_case.base.data,
		modex_test_case.base.len);
	asym_op->modex.base.data = input;
	asym_op->modex.base.length = modex_test_case.base.len;
	asym_op->modex.result.data = result;
	asym_op->modex.result.length = modex_test_case.result_len;
	if (rte_cryptodev_asym_xform_capability_check_modlen(capability,
			xform_tc.modex.modulus.length)) {
		RTE_LOG(INFO, USER1,
			"line %u FAILED: %s", __LINE__,
			"Invalid MODULUS length specified");
		return TEST_FAILED;
	}

	if (!session_less) {
		ret = rte_cryptodev_asym_session_create(TEST_CDEV_ID,
				&xform_tc, params.asym_sess_mpool, &sess);
		TEST_ASSERT_NOT_NULL(sess, "Session creation failed\n");
		TEST_ASSERT_SUCCESS(ret, "Failed to init session\n");

		ret = rte_event_crypto_adapter_caps_get(evdev, TEST_CDEV_ID,
							&cap);
		TEST_ASSERT_SUCCESS(ret, "Failed to get adapter capabilities\n");

		if (cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_SESSION_PRIVATE_DATA) {
			/* Fill in private user data information */
			m_data.response_info.event = response_info.event;
			rte_cryptodev_session_event_mdata_set(TEST_CDEV_ID,
					sess, RTE_CRYPTO_OP_TYPE_ASYMMETRIC,
					RTE_CRYPTO_OP_WITH_SESSION,
					&m_data, sizeof(m_data));
		}

		rte_crypto_op_attach_asym_session(op, sess);
	} else {
		op->sess_type = RTE_CRYPTO_OP_SESSIONLESS;
		asym_op->xform = &xform_tc;
		op->private_data_offset = (sizeof(struct rte_crypto_op) +
				sizeof(struct rte_crypto_asym_op) +
				DEFAULT_NUM_XFORMS *
				sizeof(struct rte_crypto_asym_xform));
		/* Fill in private data information */
		m_data.response_info.event = response_info.event;
		rte_memcpy((uint8_t *)op + op->private_data_offset,
				&m_data, sizeof(m_data));
	}

	ret = send_op_recv_ev(op);
	TEST_ASSERT_SUCCESS(ret, "Failed to enqueue op to cryptodev\n");

	test_crypto_adapter_stats();

	return TEST_SUCCESS;
}

static int
test_asym_sessionless_with_op_new_mode(void)
{
	uint32_t cap;
	int ret;

	ret = rte_event_crypto_adapter_caps_get(evdev, TEST_CDEV_ID, &cap);
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

	return test_asym_op_new_mode(1);
}

static int
test_asym_session_with_op_new_mode(void)
{
	uint32_t cap;
	int ret;

	ret = rte_event_crypto_adapter_caps_get(evdev, TEST_CDEV_ID, &cap);
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

	return test_asym_op_new_mode(0);
}

static int
configure_cryptodev(void)
{
	const struct rte_cryptodev_capabilities *capability;
	struct rte_cryptodev_qp_conf qp_conf;
	struct rte_cryptodev_config conf;
	struct rte_cryptodev_info info;
	unsigned int session_size;
	unsigned int i = 0;
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
			MAXIMUM_IV_LENGTH +
			sizeof(union rte_event_crypto_metadata),
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
			MAX_NB_SESSIONS, session_size, 0,
			sizeof(union rte_event_crypto_metadata),
			SOCKET_ID_ANY);
	TEST_ASSERT_NOT_NULL(params.session_mpool,
			"session mempool allocation failed\n");

	rte_cryptodev_info_get(TEST_CDEV_ID, &info);

	while ((capability = &info.capabilities[i++])->op !=
			RTE_CRYPTO_OP_TYPE_UNDEFINED) {
		if (capability->op == RTE_CRYPTO_OP_TYPE_ASYMMETRIC) {
			params.asym_op_mpool = rte_crypto_op_pool_create(
				"EVENT_CRYPTO_ASYM_OP_POOL",
				RTE_CRYPTO_OP_TYPE_ASYMMETRIC,
				NUM_MBUFS, MBUF_CACHE_SIZE,
				(DEFAULT_NUM_XFORMS *
				sizeof(struct rte_crypto_asym_xform)) +
				sizeof(union rte_event_crypto_metadata),
				rte_socket_id());
			TEST_ASSERT_NOT_NULL(params.asym_op_mpool,
					"Can't create CRYPTO_ASYM_OP_POOL\n");

			params.asym_sess_mpool =
				rte_cryptodev_asym_session_pool_create(
					"CRYPTO_AD_ASYM_SESS_MP",
					MAX_NB_SESSIONS, 0,
					sizeof(union rte_event_crypto_metadata),
					SOCKET_ID_ANY);
			TEST_ASSERT_NOT_NULL(params.asym_sess_mpool,
				"asym session mempool allocation failed\n");
			break;
		}
	}

	conf.nb_queue_pairs = info.max_nb_queue_pairs;
	conf.socket_id = SOCKET_ID_ANY;
	conf.ff_disable = RTE_CRYPTODEV_FF_SECURITY;

	TEST_ASSERT_SUCCESS(rte_cryptodev_configure(TEST_CDEV_ID, &conf),
			"Failed to configure cryptodev %u with %u qps\n",
			TEST_CDEV_ID, conf.nb_queue_pairs);

	qp_conf.nb_descriptors = DEFAULT_NUM_OPS_INFLIGHT;
	qp_conf.mp_session = params.session_mpool;

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
					      evdev,
					      &conf, 0);
	TEST_ASSERT_SUCCESS(ret, "Failed to create event crypto adapter\n");

	return TEST_SUCCESS;
}

static int
test_crypto_adapter_qp_add_del(void)
{
	struct rte_event_crypto_adapter_queue_conf queue_conf = {
		.ev = response_info,
	};

	uint32_t cap;
	int ret;

	ret = rte_event_crypto_adapter_caps_get(evdev, TEST_CDEV_ID, &cap);
	TEST_ASSERT_SUCCESS(ret, "Failed to get adapter capabilities\n");

	if (cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_QP_EV_BIND) {
		ret = rte_event_crypto_adapter_queue_pair_add(TEST_ADAPTER_ID,
				TEST_CDEV_ID, TEST_CDEV_QP_ID, &queue_conf);
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

	struct rte_event_crypto_adapter_queue_conf queue_conf = {
		.ev = response_info,
	};

	uint32_t cap;
	int ret;

	ret = rte_event_crypto_adapter_caps_get(evdev, TEST_CDEV_ID, &cap);
	TEST_ASSERT_SUCCESS(ret, "Failed to get adapter capabilities\n");

	/* Skip mode and capability mismatch check for SW eventdev */
	if (!(cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_NEW) &&
	    !(cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_FWD) &&
	    !(cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_QP_EV_BIND))
		goto adapter_create;

	if (mode == RTE_EVENT_CRYPTO_ADAPTER_OP_FORWARD) {
		if (cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_FWD)
			params.internal_port_op_fwd = 1;
		else
			return -ENOTSUP;
	}

	if ((mode == RTE_EVENT_CRYPTO_ADAPTER_OP_NEW) &&
	    !(cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_NEW))
		return -ENOTSUP;

adapter_create:
	/* Create adapter with default port creation callback */
	ret = rte_event_crypto_adapter_create(TEST_ADAPTER_ID,
					      evdev,
					      &conf, mode);
	TEST_ASSERT_SUCCESS(ret, "Failed to create event crypto adapter\n");

	if (cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_QP_EV_BIND) {
		ret = rte_event_crypto_adapter_queue_pair_add(TEST_ADAPTER_ID,
				TEST_CDEV_ID, TEST_CDEV_QP_ID, &queue_conf);
	} else
		ret = rte_event_crypto_adapter_queue_pair_add(TEST_ADAPTER_ID,
				TEST_CDEV_ID, TEST_CDEV_QP_ID, NULL);

	TEST_ASSERT_SUCCESS(ret, "Failed to add queue pair\n");

	if (!params.internal_port_op_fwd) {
		ret = rte_event_crypto_adapter_event_port_get(TEST_ADAPTER_ID,
						&params.crypto_event_port_id);
		TEST_ASSERT_SUCCESS(ret, "Failed to get event port\n");
	}

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
		rte_cryptodev_stop(TEST_CDEV_ID);
		rte_event_dev_stop(evdev);
	} else {
		rte_cryptodev_stop(TEST_CDEV_ID);
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
		if (ret)
			return ret;
		if (!params.internal_port_op_fwd) {
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

	/* start the cryptodev */
	TEST_ASSERT_SUCCESS(rte_cryptodev_start(TEST_CDEV_ID),
				"Failed to start crypto device");

	return TEST_SUCCESS;
}

static int
test_crypto_adapter_conf_op_forward_mode(void)
{
	enum rte_event_crypto_adapter_mode mode;

	mode = RTE_EVENT_CRYPTO_ADAPTER_OP_FORWARD;

	return test_crypto_adapter_conf(mode);
}

static int
test_crypto_adapter_conf_op_new_mode(void)
{
	enum rte_event_crypto_adapter_mode mode;

	mode = RTE_EVENT_CRYPTO_ADAPTER_OP_NEW;

	return test_crypto_adapter_conf(mode);
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
crypto_adapter_teardown(void)
{
	int ret;

	ret = rte_event_crypto_adapter_stop(TEST_ADAPTER_ID);
	if (ret < 0)
		RTE_LOG(ERR, USER1, "Failed to stop adapter!");

	ret = rte_event_crypto_adapter_queue_pair_del(TEST_ADAPTER_ID,
					TEST_CDEV_ID, TEST_CDEV_QP_ID);
	if (ret < 0)
		RTE_LOG(ERR, USER1, "Failed to delete queue pair!");

	ret = rte_event_crypto_adapter_free(TEST_ADAPTER_ID);
	if (ret < 0)
		RTE_LOG(ERR, USER1, "Failed to free adapter!");

	crypto_adapter_setup_done = 0;
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

	/* Free asym session mempool */
	if (params.asym_sess_mpool != NULL) {
		RTE_LOG(DEBUG, USER1, "CRYPTO_AD_ASYM_SESS_MP count %u\n",
		rte_mempool_avail_count(params.asym_sess_mpool));
		rte_mempool_free(params.asym_sess_mpool);
		params.asym_sess_mpool = NULL;
	}
	/* Free asym ops mempool */
	if (params.asym_op_mpool != NULL) {
		RTE_LOG(DEBUG, USER1, "EVENT_CRYPTO_ASYM_OP_POOL count %u\n",
		rte_mempool_avail_count(params.asym_op_mpool));
		rte_mempool_free(params.asym_op_mpool);
		params.asym_op_mpool = NULL;
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
	crypto_adapter_teardown();
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

		TEST_CASE_ST(test_crypto_adapter_create,
				test_crypto_adapter_free,
				test_crypto_adapter_params),

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

		TEST_CASE_ST(test_crypto_adapter_conf_op_forward_mode,
				test_crypto_adapter_stop,
				test_asym_session_with_op_forward_mode),

		TEST_CASE_ST(test_crypto_adapter_conf_op_forward_mode,
				test_crypto_adapter_stop,
				test_asym_sessionless_with_op_forward_mode),

		TEST_CASE_ST(test_crypto_adapter_conf_op_new_mode,
				test_crypto_adapter_stop,
				test_asym_session_with_op_new_mode),

		TEST_CASE_ST(test_crypto_adapter_conf_op_new_mode,
				test_crypto_adapter_stop,
				test_asym_sessionless_with_op_new_mode),

		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static int
test_event_crypto_adapter(void)
{
	return unit_test_suite_runner(&functional_testsuite);
}

#endif /* !RTE_EXEC_ENV_WINDOWS */

REGISTER_TEST_COMMAND(event_crypto_adapter_autotest,
		test_event_crypto_adapter);
