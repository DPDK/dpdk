/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2015-2016 Intel Corporation. All rights reserved.
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
 *	 * Neither the name of Intel Corporation nor the names of its
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

#include <rte_crypto.h>
#include <rte_cryptodev.h>
#include <rte_cryptodev_pmd.h>

#include "test.h"
#include "test_cryptodev.h"

#include "test_cryptodev_aes.h"
#include "test_cryptodev_kasumi_test_vectors.h"
#include "test_cryptodev_kasumi_hash_test_vectors.h"
#include "test_cryptodev_snow3g_test_vectors.h"
#include "test_cryptodev_snow3g_hash_test_vectors.h"
#include "test_cryptodev_gcm_test_vectors.h"

static enum rte_cryptodev_type gbl_cryptodev_type;

struct crypto_testsuite_params {
	struct rte_mempool *mbuf_pool;
	struct rte_mempool *op_mpool;
	struct rte_cryptodev_config conf;
	struct rte_cryptodev_qp_conf qp_conf;

	uint8_t valid_devs[RTE_CRYPTO_MAX_DEVS];
	uint8_t valid_dev_count;
};

struct crypto_unittest_params {
	struct rte_crypto_sym_xform cipher_xform;
	struct rte_crypto_sym_xform auth_xform;

	struct rte_cryptodev_sym_session *sess;

	struct rte_crypto_op *op;

	struct rte_mbuf *obuf, *ibuf;

	uint8_t *digest;
};

#define ALIGN_POW2_ROUNDUP(num, align) \
	(((num) + (align) - 1) & ~((align) - 1))

/*
 * Forward declarations.
 */
static int
test_AES_CBC_HMAC_SHA512_decrypt_create_session_params(
		struct crypto_unittest_params *ut_params);

static int
test_AES_CBC_HMAC_SHA512_decrypt_perform(struct rte_cryptodev_sym_session *sess,
		struct crypto_unittest_params *ut_params,
		struct crypto_testsuite_params *ts_param);

static struct rte_mbuf *
setup_test_string(struct rte_mempool *mpool,
		const char *string, size_t len, uint8_t blocksize)
{
	struct rte_mbuf *m = rte_pktmbuf_alloc(mpool);
	size_t t_len = len - (blocksize ? (len % blocksize) : 0);

	memset(m->buf_addr, 0, m->buf_len);
	if (m) {
		char *dst = rte_pktmbuf_append(m, t_len);

		if (!dst) {
			rte_pktmbuf_free(m);
			return NULL;
		}
		if (string != NULL)
			rte_memcpy(dst, string, t_len);
		else
			memset(dst, 0, t_len);
	}

	return m;
}

/* Get number of bytes in X bits (rounding up) */
static uint32_t
ceil_byte_length(uint32_t num_bits)
{
	if (num_bits % 8)
		return ((num_bits >> 3) + 1);
	else
		return (num_bits >> 3);
}

static struct rte_crypto_op *
process_crypto_request(uint8_t dev_id, struct rte_crypto_op *op)
{
	if (rte_cryptodev_enqueue_burst(dev_id, 0, &op, 1) != 1) {
		printf("Error sending packet for encryption");
		return NULL;
	}

	op = NULL;

	while (rte_cryptodev_dequeue_burst(dev_id, 0, &op, 1) == 0)
		rte_pause();

	return op;
}

static struct crypto_testsuite_params testsuite_params = { NULL };
static struct crypto_unittest_params unittest_params;

static int
testsuite_setup(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct rte_cryptodev_info info;
	unsigned i, nb_devs, dev_id;
	int ret;
	uint16_t qp_id;

	memset(ts_params, 0, sizeof(*ts_params));

	ts_params->mbuf_pool = rte_mempool_lookup("CRYPTO_MBUFPOOL");
	if (ts_params->mbuf_pool == NULL) {
		/* Not already created so create */
		ts_params->mbuf_pool = rte_pktmbuf_pool_create(
				"CRYPTO_MBUFPOOL",
				NUM_MBUFS, MBUF_CACHE_SIZE, 0, MBUF_SIZE,
				rte_socket_id());
		if (ts_params->mbuf_pool == NULL) {
			RTE_LOG(ERR, USER1, "Can't create CRYPTO_MBUFPOOL\n");
			return TEST_FAILED;
		}
	}

	ts_params->op_mpool = rte_crypto_op_pool_create(
			"MBUF_CRYPTO_SYM_OP_POOL",
			RTE_CRYPTO_OP_TYPE_SYMMETRIC,
			NUM_MBUFS, MBUF_CACHE_SIZE,
			DEFAULT_NUM_XFORMS *
			sizeof(struct rte_crypto_sym_xform),
			rte_socket_id());
	if (ts_params->op_mpool == NULL) {
		RTE_LOG(ERR, USER1, "Can't create CRYPTO_OP_POOL\n");
		return TEST_FAILED;
	}

	/* Create 2 AESNI MB devices if required */
	if (gbl_cryptodev_type == RTE_CRYPTODEV_AESNI_MB_PMD) {
		nb_devs = rte_cryptodev_count_devtype(
				RTE_CRYPTODEV_AESNI_MB_PMD);
		if (nb_devs < 2) {
			for (i = nb_devs; i < 2; i++) {
				ret = rte_eal_vdev_init(
					RTE_STR(CRYPTODEV_NAME_AESNI_MB_PMD), NULL);

				TEST_ASSERT(ret == 0,
					"Failed to create instance %u of"
					" pmd : %s",
					i, RTE_STR(CRYPTODEV_NAME_AESNI_MB_PMD));
			}
		}
	}

	/* Create 2 AESNI GCM devices if required */
	if (gbl_cryptodev_type == RTE_CRYPTODEV_AESNI_GCM_PMD) {
		nb_devs = rte_cryptodev_count_devtype(
				RTE_CRYPTODEV_AESNI_GCM_PMD);
		if (nb_devs < 2) {
			for (i = nb_devs; i < 2; i++) {
				TEST_ASSERT_SUCCESS(rte_eal_vdev_init(
					RTE_STR(CRYPTODEV_NAME_AESNI_GCM_PMD), NULL),
					"Failed to create instance %u of"
					" pmd : %s",
					i, RTE_STR(CRYPTODEV_NAME_AESNI_GCM_PMD));
			}
		}
	}

	/* Create 2 Snow3G devices if required */
	if (gbl_cryptodev_type == RTE_CRYPTODEV_SNOW3G_PMD) {
		nb_devs = rte_cryptodev_count_devtype(RTE_CRYPTODEV_SNOW3G_PMD);
		if (nb_devs < 2) {
			for (i = nb_devs; i < 2; i++) {
				TEST_ASSERT_SUCCESS(rte_eal_vdev_init(
					RTE_STR(CRYPTODEV_NAME_SNOW3G_PMD), NULL),
					"Failed to create instance %u of"
					" pmd : %s",
					i, RTE_STR(CRYPTODEV_NAME_SNOW3G_PMD));
			}
		}
	}

	/* Create 2 KASUMI devices if required */
	if (gbl_cryptodev_type == RTE_CRYPTODEV_KASUMI_PMD) {
		nb_devs = rte_cryptodev_count_devtype(RTE_CRYPTODEV_KASUMI_PMD);
		if (nb_devs < 2) {
			for (i = nb_devs; i < 2; i++) {
				TEST_ASSERT_SUCCESS(rte_eal_vdev_init(
					RTE_STR(CRYPTODEV_NAME_KASUMI_PMD), NULL),
					"Failed to create instance %u of"
					" pmd : %s",
					i, RTE_STR(CRYPTODEV_NAME_KASUMI_PMD));
			}
		}
	}

	/* Create 2 NULL devices if required */
	if (gbl_cryptodev_type == RTE_CRYPTODEV_NULL_PMD) {
		nb_devs = rte_cryptodev_count_devtype(
				RTE_CRYPTODEV_NULL_PMD);
		if (nb_devs < 2) {
			for (i = nb_devs; i < 2; i++) {
				int dev_id = rte_eal_vdev_init(
					RTE_STR(CRYPTODEV_NAME_NULL_PMD), NULL);

				TEST_ASSERT(dev_id >= 0,
					"Failed to create instance %u of"
					" pmd : %s",
					i, RTE_STR(CRYPTODEV_NAME_NULL_PMD));
			}
		}
	}

	nb_devs = rte_cryptodev_count();
	if (nb_devs < 1) {
		RTE_LOG(ERR, USER1, "No crypto devices found?");
		return TEST_FAILED;
	}

	/* Create list of valid crypto devs */
	for (i = 0; i < nb_devs; i++) {
		rte_cryptodev_info_get(i, &info);
		if (info.dev_type == gbl_cryptodev_type)
			ts_params->valid_devs[ts_params->valid_dev_count++] = i;
	}

	if (ts_params->valid_dev_count < 1)
		return TEST_FAILED;

	/* Set up all the qps on the first of the valid devices found */
	for (i = 0; i < 1; i++) {
		dev_id = ts_params->valid_devs[i];

		rte_cryptodev_info_get(dev_id, &info);

		/*
		 * Since we can't free and re-allocate queue memory always set
		 * the queues on this device up to max size first so enough
		 * memory is allocated for any later re-configures needed by
		 * other tests
		 */

		ts_params->conf.nb_queue_pairs = info.max_nb_queue_pairs;
		ts_params->conf.socket_id = SOCKET_ID_ANY;
		ts_params->conf.session_mp.nb_objs = info.sym.max_nb_sessions;

		TEST_ASSERT_SUCCESS(rte_cryptodev_configure(dev_id,
				&ts_params->conf),
				"Failed to configure cryptodev %u with %u qps",
				dev_id, ts_params->conf.nb_queue_pairs);

		ts_params->qp_conf.nb_descriptors = MAX_NUM_OPS_INFLIGHT;

		for (qp_id = 0; qp_id < info.max_nb_queue_pairs; qp_id++) {
			TEST_ASSERT_SUCCESS(rte_cryptodev_queue_pair_setup(
					dev_id, qp_id, &ts_params->qp_conf,
					rte_cryptodev_socket_id(dev_id)),
					"Failed to setup queue pair %u on "
					"cryptodev %u",
					qp_id, dev_id);
		}
	}

	return TEST_SUCCESS;
}

static void
testsuite_teardown(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;

	if (ts_params->mbuf_pool != NULL) {
		RTE_LOG(DEBUG, USER1, "CRYPTO_MBUFPOOL count %u\n",
		rte_mempool_avail_count(ts_params->mbuf_pool));
	}

	if (ts_params->op_mpool != NULL) {
		RTE_LOG(DEBUG, USER1, "CRYPTO_OP_POOL count %u\n",
		rte_mempool_avail_count(ts_params->op_mpool));
	}

}

static int
ut_setup(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	uint16_t qp_id;

	/* Clear unit test parameters before running test */
	memset(ut_params, 0, sizeof(*ut_params));

	/* Reconfigure device to default parameters */
	ts_params->conf.nb_queue_pairs = DEFAULT_NUM_QPS_PER_QAT_DEVICE;
	ts_params->conf.socket_id = SOCKET_ID_ANY;
	ts_params->conf.session_mp.nb_objs =
			(gbl_cryptodev_type == RTE_CRYPTODEV_QAT_SYM_PMD) ?
					DEFAULT_NUM_OPS_INFLIGHT :
					DEFAULT_NUM_OPS_INFLIGHT;

	TEST_ASSERT_SUCCESS(rte_cryptodev_configure(ts_params->valid_devs[0],
			&ts_params->conf),
			"Failed to configure cryptodev %u",
			ts_params->valid_devs[0]);

	/*
	 * Now reconfigure queues to size we actually want to use in this
	 * test suite.
	 */
	ts_params->qp_conf.nb_descriptors = DEFAULT_NUM_OPS_INFLIGHT;

	for (qp_id = 0; qp_id < ts_params->conf.nb_queue_pairs ; qp_id++) {
		TEST_ASSERT_SUCCESS(rte_cryptodev_queue_pair_setup(
			ts_params->valid_devs[0], qp_id,
			&ts_params->qp_conf,
			rte_cryptodev_socket_id(ts_params->valid_devs[0])),
			"Failed to setup queue pair %u on cryptodev %u",
			qp_id, ts_params->valid_devs[0]);
	}


	rte_cryptodev_stats_reset(ts_params->valid_devs[0]);

	/* Start the device */
	TEST_ASSERT_SUCCESS(rte_cryptodev_start(ts_params->valid_devs[0]),
			"Failed to start cryptodev %u",
			ts_params->valid_devs[0]);

	return TEST_SUCCESS;
}

static void
ut_teardown(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;
	struct rte_cryptodev_stats stats;

	/* free crypto session structure */
	if (ut_params->sess) {
		rte_cryptodev_sym_session_free(ts_params->valid_devs[0],
				ut_params->sess);
		ut_params->sess = NULL;
	}

	/* free crypto operation structure */
	if (ut_params->op)
		rte_crypto_op_free(ut_params->op);

	/*
	 * free mbuf - both obuf and ibuf are usually the same,
	 * so check if they point at the same address is necessary,
	 * to avoid freeing the mbuf twice.
	 */
	if (ut_params->obuf) {
		rte_pktmbuf_free(ut_params->obuf);
		if (ut_params->ibuf == ut_params->obuf)
			ut_params->ibuf = 0;
		ut_params->obuf = 0;
	}
	if (ut_params->ibuf) {
		rte_pktmbuf_free(ut_params->ibuf);
		ut_params->ibuf = 0;
	}

	if (ts_params->mbuf_pool != NULL)
		RTE_LOG(DEBUG, USER1, "CRYPTO_MBUFPOOL count %u\n",
			rte_mempool_avail_count(ts_params->mbuf_pool));

	rte_cryptodev_stats_get(ts_params->valid_devs[0], &stats);

	/* Stop the device */
	rte_cryptodev_stop(ts_params->valid_devs[0]);
}

static int
test_device_configure_invalid_dev_id(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	uint16_t dev_id, num_devs = 0;

	TEST_ASSERT((num_devs = rte_cryptodev_count()) >= 1,
			"Need at least %d devices for test", 1);

	/* valid dev_id values */
	dev_id = ts_params->valid_devs[ts_params->valid_dev_count - 1];

	/* Stop the device in case it's started so it can be configured */
	rte_cryptodev_stop(ts_params->valid_devs[dev_id]);

	TEST_ASSERT_SUCCESS(rte_cryptodev_configure(dev_id, &ts_params->conf),
			"Failed test for rte_cryptodev_configure: "
			"invalid dev_num %u", dev_id);

	/* invalid dev_id values */
	dev_id = num_devs;

	TEST_ASSERT_FAIL(rte_cryptodev_configure(dev_id, &ts_params->conf),
			"Failed test for rte_cryptodev_configure: "
			"invalid dev_num %u", dev_id);

	dev_id = 0xff;

	TEST_ASSERT_FAIL(rte_cryptodev_configure(dev_id, &ts_params->conf),
			"Failed test for rte_cryptodev_configure:"
			"invalid dev_num %u", dev_id);

	return TEST_SUCCESS;
}

static int
test_device_configure_invalid_queue_pair_ids(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;

	/* Stop the device in case it's started so it can be configured */
	rte_cryptodev_stop(ts_params->valid_devs[0]);

	/* valid - one queue pairs */
	ts_params->conf.nb_queue_pairs = 1;

	TEST_ASSERT_SUCCESS(rte_cryptodev_configure(ts_params->valid_devs[0],
			&ts_params->conf),
			"Failed to configure cryptodev: dev_id %u, qp_id %u",
			ts_params->valid_devs[0], ts_params->conf.nb_queue_pairs);


	/* valid - max value queue pairs */
	ts_params->conf.nb_queue_pairs = MAX_NUM_QPS_PER_QAT_DEVICE;

	TEST_ASSERT_SUCCESS(rte_cryptodev_configure(ts_params->valid_devs[0],
			&ts_params->conf),
			"Failed to configure cryptodev: dev_id %u, qp_id %u",
			ts_params->valid_devs[0], ts_params->conf.nb_queue_pairs);


	/* invalid - zero queue pairs */
	ts_params->conf.nb_queue_pairs = 0;

	TEST_ASSERT_FAIL(rte_cryptodev_configure(ts_params->valid_devs[0],
			&ts_params->conf),
			"Failed test for rte_cryptodev_configure, dev_id %u,"
			" invalid qps: %u",
			ts_params->valid_devs[0],
			ts_params->conf.nb_queue_pairs);


	/* invalid - max value supported by field queue pairs */
	ts_params->conf.nb_queue_pairs = UINT16_MAX;

	TEST_ASSERT_FAIL(rte_cryptodev_configure(ts_params->valid_devs[0],
			&ts_params->conf),
			"Failed test for rte_cryptodev_configure, dev_id %u,"
			" invalid qps: %u",
			ts_params->valid_devs[0],
			ts_params->conf.nb_queue_pairs);


	/* invalid - max value + 1 queue pairs */
	ts_params->conf.nb_queue_pairs = MAX_NUM_QPS_PER_QAT_DEVICE + 1;

	TEST_ASSERT_FAIL(rte_cryptodev_configure(ts_params->valid_devs[0],
			&ts_params->conf),
			"Failed test for rte_cryptodev_configure, dev_id %u,"
			" invalid qps: %u",
			ts_params->valid_devs[0],
			ts_params->conf.nb_queue_pairs);

	return TEST_SUCCESS;
}

static int
test_queue_pair_descriptor_setup(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct rte_cryptodev_info dev_info;
	struct rte_cryptodev_qp_conf qp_conf = {
		.nb_descriptors = MAX_NUM_OPS_INFLIGHT
	};

	uint16_t qp_id;

	/* Stop the device in case it's started so it can be configured */
	rte_cryptodev_stop(ts_params->valid_devs[0]);


	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);

	ts_params->conf.session_mp.nb_objs = dev_info.sym.max_nb_sessions;

	TEST_ASSERT_SUCCESS(rte_cryptodev_configure(ts_params->valid_devs[0],
			&ts_params->conf), "Failed to configure cryptodev %u",
			ts_params->valid_devs[0]);


	/*
	 * Test various ring sizes on this device. memzones can't be
	 * freed so are re-used if ring is released and re-created.
	 */
	qp_conf.nb_descriptors = MIN_NUM_OPS_INFLIGHT; /* min size*/

	for (qp_id = 0; qp_id < ts_params->conf.nb_queue_pairs; qp_id++) {
		TEST_ASSERT_SUCCESS(rte_cryptodev_queue_pair_setup(
				ts_params->valid_devs[0], qp_id, &qp_conf,
				rte_cryptodev_socket_id(
						ts_params->valid_devs[0])),
				"Failed test for "
				"rte_cryptodev_queue_pair_setup: num_inflights "
				"%u on qp %u on cryptodev %u",
				qp_conf.nb_descriptors, qp_id,
				ts_params->valid_devs[0]);
	}

	qp_conf.nb_descriptors = (uint32_t)(MAX_NUM_OPS_INFLIGHT / 2);

	for (qp_id = 0; qp_id < ts_params->conf.nb_queue_pairs; qp_id++) {
		TEST_ASSERT_SUCCESS(rte_cryptodev_queue_pair_setup(
				ts_params->valid_devs[0], qp_id, &qp_conf,
				rte_cryptodev_socket_id(
						ts_params->valid_devs[0])),
				"Failed test for"
				" rte_cryptodev_queue_pair_setup: num_inflights"
				" %u on qp %u on cryptodev %u",
				qp_conf.nb_descriptors, qp_id,
				ts_params->valid_devs[0]);
	}

	qp_conf.nb_descriptors = MAX_NUM_OPS_INFLIGHT; /* valid */

	for (qp_id = 0; qp_id < ts_params->conf.nb_queue_pairs; qp_id++) {
		TEST_ASSERT_SUCCESS(rte_cryptodev_queue_pair_setup(
				ts_params->valid_devs[0], qp_id, &qp_conf,
				rte_cryptodev_socket_id(
						ts_params->valid_devs[0])),
				"Failed test for "
				"rte_cryptodev_queue_pair_setup: num_inflights"
				" %u on qp %u on cryptodev %u",
				qp_conf.nb_descriptors, qp_id,
				ts_params->valid_devs[0]);
	}

	/* invalid number of descriptors - max supported + 2 */
	qp_conf.nb_descriptors = MAX_NUM_OPS_INFLIGHT + 2;

	for (qp_id = 0; qp_id < ts_params->conf.nb_queue_pairs; qp_id++) {
		TEST_ASSERT_FAIL(rte_cryptodev_queue_pair_setup(
				ts_params->valid_devs[0], qp_id, &qp_conf,
				rte_cryptodev_socket_id(
						ts_params->valid_devs[0])),
				"Unexpectedly passed test for "
				"rte_cryptodev_queue_pair_setup:"
				"num_inflights %u on qp %u on cryptodev %u",
				qp_conf.nb_descriptors, qp_id,
				ts_params->valid_devs[0]);
	}

	/* invalid number of descriptors - max value of parameter */
	qp_conf.nb_descriptors = UINT32_MAX-1;

	for (qp_id = 0; qp_id < ts_params->conf.nb_queue_pairs; qp_id++) {
		TEST_ASSERT_FAIL(rte_cryptodev_queue_pair_setup(
				ts_params->valid_devs[0], qp_id, &qp_conf,
				rte_cryptodev_socket_id(
						ts_params->valid_devs[0])),
				"Unexpectedly passed test for "
				"rte_cryptodev_queue_pair_setup:"
				"num_inflights %u on qp %u on cryptodev %u",
				qp_conf.nb_descriptors, qp_id,
				ts_params->valid_devs[0]);
	}

	qp_conf.nb_descriptors = DEFAULT_NUM_OPS_INFLIGHT;

	for (qp_id = 0; qp_id < ts_params->conf.nb_queue_pairs; qp_id++) {
		TEST_ASSERT_SUCCESS(rte_cryptodev_queue_pair_setup(
				ts_params->valid_devs[0], qp_id, &qp_conf,
				rte_cryptodev_socket_id(
						ts_params->valid_devs[0])),
				"Failed test for"
				" rte_cryptodev_queue_pair_setup:"
				"num_inflights %u on qp %u on cryptodev %u",
				qp_conf.nb_descriptors, qp_id,
				ts_params->valid_devs[0]);
	}

	/* invalid number of descriptors - max supported + 1 */
	qp_conf.nb_descriptors = DEFAULT_NUM_OPS_INFLIGHT + 1;

	for (qp_id = 0; qp_id < ts_params->conf.nb_queue_pairs; qp_id++) {
		TEST_ASSERT_FAIL(rte_cryptodev_queue_pair_setup(
				ts_params->valid_devs[0], qp_id, &qp_conf,
				rte_cryptodev_socket_id(
						ts_params->valid_devs[0])),
				"Unexpectedly passed test for "
				"rte_cryptodev_queue_pair_setup:"
				"num_inflights %u on qp %u on cryptodev %u",
				qp_conf.nb_descriptors, qp_id,
				ts_params->valid_devs[0]);
	}

	/* test invalid queue pair id */
	qp_conf.nb_descriptors = DEFAULT_NUM_OPS_INFLIGHT;	/*valid */

	qp_id = DEFAULT_NUM_QPS_PER_QAT_DEVICE;		/*invalid */

	TEST_ASSERT_FAIL(rte_cryptodev_queue_pair_setup(
			ts_params->valid_devs[0],
			qp_id, &qp_conf,
			rte_cryptodev_socket_id(ts_params->valid_devs[0])),
			"Failed test for rte_cryptodev_queue_pair_setup:"
			"invalid qp %u on cryptodev %u",
			qp_id, ts_params->valid_devs[0]);

	qp_id = 0xffff; /*invalid*/

	TEST_ASSERT_FAIL(rte_cryptodev_queue_pair_setup(
			ts_params->valid_devs[0],
			qp_id, &qp_conf,
			rte_cryptodev_socket_id(ts_params->valid_devs[0])),
			"Failed test for rte_cryptodev_queue_pair_setup:"
			"invalid qp %u on cryptodev %u",
			qp_id, ts_params->valid_devs[0]);

	return TEST_SUCCESS;
}

/* ***** Plaintext data for tests ***** */

const char catch_22_quote_1[] =
		"There was only one catch and that was Catch-22, which "
		"specified that a concern for one's safety in the face of "
		"dangers that were real and immediate was the process of a "
		"rational mind. Orr was crazy and could be grounded. All he "
		"had to do was ask; and as soon as he did, he would no longer "
		"be crazy and would have to fly more missions. Orr would be "
		"crazy to fly more missions and sane if he didn't, but if he "
		"was sane he had to fly them. If he flew them he was crazy "
		"and didn't have to; but if he didn't want to he was sane and "
		"had to. Yossarian was moved very deeply by the absolute "
		"simplicity of this clause of Catch-22 and let out a "
		"respectful whistle. \"That's some catch, that Catch-22\", he "
		"observed. \"It's the best there is,\" Doc Daneeka agreed.";

const char catch_22_quote[] =
		"What a lousy earth! He wondered how many people were "
		"destitute that same night even in his own prosperous country, "
		"how many homes were shanties, how many husbands were drunk "
		"and wives socked, and how many children were bullied, abused, "
		"or abandoned. How many families hungered for food they could "
		"not afford to buy? How many hearts were broken? How many "
		"suicides would take place that same night, how many people "
		"would go insane? How many cockroaches and landlords would "
		"triumph? How many winners were losers, successes failures, "
		"and rich men poor men? How many wise guys were stupid? How "
		"many happy endings were unhappy endings? How many honest men "
		"were liars, brave men cowards, loyal men traitors, how many "
		"sainted men were corrupt, how many people in positions of "
		"trust had sold their souls to bodyguards, how many had never "
		"had souls? How many straight-and-narrow paths were crooked "
		"paths? How many best families were worst families and how "
		"many good people were bad people? When you added them all up "
		"and then subtracted, you might be left with only the children, "
		"and perhaps with Albert Einstein and an old violinist or "
		"sculptor somewhere.";

#define QUOTE_480_BYTES		(480)
#define QUOTE_512_BYTES		(512)
#define QUOTE_768_BYTES		(768)
#define QUOTE_1024_BYTES	(1024)



/* ***** SHA1 Hash Tests ***** */

#define HMAC_KEY_LENGTH_SHA1	(DIGEST_BYTE_LENGTH_SHA1)

static uint8_t hmac_sha1_key[] = {
	0xF8, 0x2A, 0xC7, 0x54, 0xDB, 0x96, 0x18, 0xAA,
	0xC3, 0xA1, 0x53, 0xF6, 0x1F, 0x17, 0x60, 0xBD,
	0xDE, 0xF4, 0xDE, 0xAD };

/* ***** SHA224 Hash Tests ***** */

#define HMAC_KEY_LENGTH_SHA224	(DIGEST_BYTE_LENGTH_SHA224)


/* ***** AES-CBC Cipher Tests ***** */

#define CIPHER_KEY_LENGTH_AES_CBC	(16)
#define CIPHER_IV_LENGTH_AES_CBC	(CIPHER_KEY_LENGTH_AES_CBC)

static uint8_t aes_cbc_key[] = {
	0xE4, 0x23, 0x33, 0x8A, 0x35, 0x64, 0x61, 0xE2,
	0x49, 0x03, 0xDD, 0xC6, 0xB8, 0xCA, 0x55, 0x7A };

static uint8_t aes_cbc_iv[] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f };


/* ***** AES-CBC / HMAC-SHA1 Hash Tests ***** */

static const uint8_t catch_22_quote_2_512_bytes_AES_CBC_ciphertext[] = {
	0x8B, 0x4D, 0xDA, 0x1B, 0xCF, 0x04, 0xA0, 0x31,
	0xB4, 0xBF, 0xBD, 0x68, 0x43, 0x20, 0x7E, 0x76,
	0xB1, 0x96, 0x8B, 0xA2, 0x7C, 0xA2, 0x83, 0x9E,
	0x39, 0x5A, 0x2F, 0x7E, 0x92, 0xB4, 0x48, 0x1A,
	0x3F, 0x6B, 0x5D, 0xDF, 0x52, 0x85, 0x5F, 0x8E,
	0x42, 0x3C, 0xFB, 0xE9, 0x1A, 0x24, 0xD6, 0x08,
	0xDD, 0xFD, 0x16, 0xFB, 0xE9, 0x55, 0xEF, 0xF0,
	0xA0, 0x8D, 0x13, 0xAB, 0x81, 0xC6, 0x90, 0x01,
	0xB5, 0x18, 0x84, 0xB3, 0xF6, 0xE6, 0x11, 0x57,
	0xD6, 0x71, 0xC6, 0x3C, 0x3F, 0x2F, 0x33, 0xEE,
	0x24, 0x42, 0x6E, 0xAC, 0x0B, 0xCA, 0xEC, 0xF9,
	0x84, 0xF8, 0x22, 0xAA, 0x60, 0xF0, 0x32, 0xA9,
	0x75, 0x75, 0x3B, 0xCB, 0x70, 0x21, 0x0A, 0x8D,
	0x0F, 0xE0, 0xC4, 0x78, 0x2B, 0xF8, 0x97, 0xE3,
	0xE4, 0x26, 0x4B, 0x29, 0xDA, 0x88, 0xCD, 0x46,
	0xEC, 0xAA, 0xF9, 0x7F, 0xF1, 0x15, 0xEA, 0xC3,
	0x87, 0xE6, 0x31, 0xF2, 0xCF, 0xDE, 0x4D, 0x80,
	0x70, 0x91, 0x7E, 0x0C, 0xF7, 0x26, 0x3A, 0x92,
	0x4F, 0x18, 0x83, 0xC0, 0x8F, 0x59, 0x01, 0xA5,
	0x88, 0xD1, 0xDB, 0x26, 0x71, 0x27, 0x16, 0xF5,
	0xEE, 0x10, 0x82, 0xAC, 0x68, 0x26, 0x9B, 0xE2,
	0x6D, 0xD8, 0x9A, 0x80, 0xDF, 0x04, 0x31, 0xD5,
	0xF1, 0x35, 0x5C, 0x3B, 0xDD, 0x9A, 0x65, 0xBA,
	0x58, 0x34, 0x85, 0x61, 0x1C, 0x42, 0x10, 0x76,
	0x73, 0x02, 0x42, 0xC9, 0x23, 0x18, 0x8E, 0xB4,
	0x6F, 0xB4, 0xA3, 0x54, 0x6E, 0x88, 0x3B, 0x62,
	0x7C, 0x02, 0x8D, 0x4C, 0x9F, 0xC8, 0x45, 0xF4,
	0xC9, 0xDE, 0x4F, 0xEB, 0x22, 0x83, 0x1B, 0xE4,
	0x49, 0x37, 0xE4, 0xAD, 0xE7, 0xCD, 0x21, 0x54,
	0xBC, 0x1C, 0xC2, 0x04, 0x97, 0xB4, 0x10, 0x61,
	0xF0, 0xE4, 0xEF, 0x27, 0x63, 0x3A, 0xDA, 0x91,
	0x41, 0x25, 0x62, 0x1C, 0x5C, 0xB6, 0x38, 0x4A,
	0x88, 0x71, 0x59, 0x5A, 0x8D, 0xA0, 0x09, 0xAF,
	0x72, 0x94, 0xD7, 0x79, 0x5C, 0x60, 0x7C, 0x8F,
	0x4C, 0xF5, 0xD9, 0xA1, 0x39, 0x6D, 0x81, 0x28,
	0xEF, 0x13, 0x28, 0xDF, 0xF5, 0x3E, 0xF7, 0x8E,
	0x09, 0x9C, 0x78, 0x18, 0x79, 0xB8, 0x68, 0xD7,
	0xA8, 0x29, 0x62, 0xAD, 0xDE, 0xE1, 0x61, 0x76,
	0x1B, 0x05, 0x16, 0xCD, 0xBF, 0x02, 0x8E, 0xA6,
	0x43, 0x6E, 0x92, 0x55, 0x4F, 0x60, 0x9C, 0x03,
	0xB8, 0x4F, 0xA3, 0x02, 0xAC, 0xA8, 0xA7, 0x0C,
	0x1E, 0xB5, 0x6B, 0xF8, 0xC8, 0x4D, 0xDE, 0xD2,
	0xB0, 0x29, 0x6E, 0x40, 0xE6, 0xD6, 0xC9, 0xE6,
	0xB9, 0x0F, 0xB6, 0x63, 0xF5, 0xAA, 0x2B, 0x96,
	0xA7, 0x16, 0xAC, 0x4E, 0x0A, 0x33, 0x1C, 0xA6,
	0xE6, 0xBD, 0x8A, 0xCF, 0x40, 0xA9, 0xB2, 0xFA,
	0x63, 0x27, 0xFD, 0x9B, 0xD9, 0xFC, 0xD5, 0x87,
	0x8D, 0x4C, 0xB6, 0xA4, 0xCB, 0xE7, 0x74, 0x55,
	0xF4, 0xFB, 0x41, 0x25, 0xB5, 0x4B, 0x0A, 0x1B,
	0xB1, 0xD6, 0xB7, 0xD9, 0x47, 0x2A, 0xC3, 0x98,
	0x6A, 0xC4, 0x03, 0x73, 0x1F, 0x93, 0x6E, 0x53,
	0x19, 0x25, 0x64, 0x15, 0x83, 0xF9, 0x73, 0x2A,
	0x74, 0xB4, 0x93, 0x69, 0xC4, 0x72, 0xFC, 0x26,
	0xA2, 0x9F, 0x43, 0x45, 0xDD, 0xB9, 0xEF, 0x36,
	0xC8, 0x3A, 0xCD, 0x99, 0x9B, 0x54, 0x1A, 0x36,
	0xC1, 0x59, 0xF8, 0x98, 0xA8, 0xCC, 0x28, 0x0D,
	0x73, 0x4C, 0xEE, 0x98, 0xCB, 0x7C, 0x58, 0x7E,
	0x20, 0x75, 0x1E, 0xB7, 0xC9, 0xF8, 0xF2, 0x0E,
	0x63, 0x9E, 0x05, 0x78, 0x1A, 0xB6, 0xA8, 0x7A,
	0xF9, 0x98, 0x6A, 0xA6, 0x46, 0x84, 0x2E, 0xF6,
	0x4B, 0xDC, 0x9B, 0x8F, 0x9B, 0x8F, 0xEE, 0xB4,
	0xAA, 0x3F, 0xEE, 0xC0, 0x37, 0x27, 0x76, 0xC7,
	0x95, 0xBB, 0x26, 0x74, 0x69, 0x12, 0x7F, 0xF1,
	0xBB, 0xFF, 0xAE, 0xB5, 0x99, 0x6E, 0xCB, 0x0C
};

static const uint8_t catch_22_quote_2_512_bytes_AES_CBC_HMAC_SHA1_digest[] = {
	0x9a, 0x4f, 0x88, 0x1b, 0xb6, 0x8f, 0xd8, 0x60,
	0x42, 0x1a, 0x7d, 0x3d, 0xf5, 0x82, 0x80, 0xf1,
	0x18, 0x8c, 0x1d, 0x32
};


static int
test_AES_CBC_HMAC_SHA1_encrypt_digest(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	/* Generate test mbuf data and space for digest */
	ut_params->ibuf = setup_test_string(ts_params->mbuf_pool,
			catch_22_quote,	QUOTE_512_BYTES, 0);

	ut_params->digest = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
			DIGEST_BYTE_LENGTH_SHA1);
	TEST_ASSERT_NOT_NULL(ut_params->digest, "no room to append digest");

	/* Setup Cipher Parameters */
	ut_params->cipher_xform.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	ut_params->cipher_xform.next = &ut_params->auth_xform;

	ut_params->cipher_xform.cipher.algo = RTE_CRYPTO_CIPHER_AES_CBC;
	ut_params->cipher_xform.cipher.op = RTE_CRYPTO_CIPHER_OP_ENCRYPT;
	ut_params->cipher_xform.cipher.key.data = aes_cbc_key;
	ut_params->cipher_xform.cipher.key.length = CIPHER_KEY_LENGTH_AES_CBC;

	/* Setup HMAC Parameters */
	ut_params->auth_xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;

	ut_params->auth_xform.next = NULL;

	ut_params->auth_xform.auth.op = RTE_CRYPTO_AUTH_OP_GENERATE;
	ut_params->auth_xform.auth.algo = RTE_CRYPTO_AUTH_SHA1_HMAC;
	ut_params->auth_xform.auth.key.length = HMAC_KEY_LENGTH_SHA1;
	ut_params->auth_xform.auth.key.data = hmac_sha1_key;
	ut_params->auth_xform.auth.digest_length = DIGEST_BYTE_LENGTH_SHA1;

	/* Create crypto session*/
	ut_params->sess = rte_cryptodev_sym_session_create(
			ts_params->valid_devs[0],
			&ut_params->cipher_xform);
	TEST_ASSERT_NOT_NULL(ut_params->sess, "Session creation failed");

	/* Generate crypto op data structure */
	ut_params->op = rte_crypto_op_alloc(ts_params->op_mpool,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	TEST_ASSERT_NOT_NULL(ut_params->op,
			"Failed to allocate symmetric crypto operation struct");

	rte_crypto_op_attach_sym_session(ut_params->op, ut_params->sess);

	struct rte_crypto_sym_op *sym_op = ut_params->op->sym;

	/* set crypto operation source mbuf */
	sym_op->m_src = ut_params->ibuf;

	/* Set crypto operation authentication parameters */
	sym_op->auth.digest.data = ut_params->digest;
	sym_op->auth.digest.phys_addr = rte_pktmbuf_mtophys_offset(
			ut_params->ibuf, QUOTE_512_BYTES);
	sym_op->auth.digest.length = DIGEST_BYTE_LENGTH_SHA1;

	sym_op->auth.data.offset = CIPHER_IV_LENGTH_AES_CBC;
	sym_op->auth.data.length = QUOTE_512_BYTES;

	/* Set crypto operation cipher parameters */
	sym_op->cipher.iv.data = (uint8_t *)rte_pktmbuf_prepend(ut_params->ibuf,
			CIPHER_IV_LENGTH_AES_CBC);
	sym_op->cipher.iv.phys_addr = rte_pktmbuf_mtophys(ut_params->ibuf);
	sym_op->cipher.iv.length = CIPHER_IV_LENGTH_AES_CBC;

	rte_memcpy(sym_op->cipher.iv.data, aes_cbc_iv,
			CIPHER_IV_LENGTH_AES_CBC);

	sym_op->cipher.data.offset = CIPHER_IV_LENGTH_AES_CBC;
	sym_op->cipher.data.length = QUOTE_512_BYTES;

	/* Process crypto operation */
	TEST_ASSERT_NOT_NULL(process_crypto_request(ts_params->valid_devs[0],
			ut_params->op), "failed to process sym crypto op");

	TEST_ASSERT_EQUAL(ut_params->op->status, RTE_CRYPTO_OP_STATUS_SUCCESS,
			"crypto op processing failed");

	/* Validate obuf */
	uint8_t *ciphertext = rte_pktmbuf_mtod_offset(ut_params->op->sym->m_src,
			uint8_t *, CIPHER_IV_LENGTH_AES_CBC);

	TEST_ASSERT_BUFFERS_ARE_EQUAL(ciphertext,
			catch_22_quote_2_512_bytes_AES_CBC_ciphertext,
			QUOTE_512_BYTES,
			"ciphertext data not as expected");

	uint8_t *digest = ciphertext + QUOTE_512_BYTES;

	TEST_ASSERT_BUFFERS_ARE_EQUAL(digest,
			catch_22_quote_2_512_bytes_AES_CBC_HMAC_SHA1_digest,
			gbl_cryptodev_type == RTE_CRYPTODEV_AESNI_MB_PMD ?
					TRUNCATED_DIGEST_BYTE_LENGTH_SHA1 :
					DIGEST_BYTE_LENGTH_SHA1,
			"Generated digest data not as expected");

	return TEST_SUCCESS;
}

/* ***** AES-CBC / HMAC-SHA512 Hash Tests ***** */

#define HMAC_KEY_LENGTH_SHA512  (DIGEST_BYTE_LENGTH_SHA512)

static uint8_t hmac_sha512_key[] = {
	0x42, 0x1a, 0x7d, 0x3d, 0xf5, 0x82, 0x80, 0xf1,
	0xF1, 0x35, 0x5C, 0x3B, 0xDD, 0x9A, 0x65, 0xBA,
	0x58, 0x34, 0x85, 0x65, 0x1C, 0x42, 0x50, 0x76,
	0x9a, 0xaf, 0x88, 0x1b, 0xb6, 0x8f, 0xf8, 0x60,
	0xa2, 0x5a, 0x7f, 0x3f, 0xf4, 0x72, 0x70, 0xf1,
	0xF5, 0x35, 0x4C, 0x3B, 0xDD, 0x90, 0x65, 0xB0,
	0x47, 0x3a, 0x75, 0x61, 0x5C, 0xa2, 0x10, 0x76,
	0x9a, 0xaf, 0x77, 0x5b, 0xb6, 0x7f, 0xf7, 0x60 };

static const uint8_t catch_22_quote_2_512_bytes_AES_CBC_HMAC_SHA512_digest[] = {
	0x5D, 0x54, 0x66, 0xC1, 0x6E, 0xBC, 0x04, 0xB8,
	0x46, 0xB8, 0x08, 0x6E, 0xE0, 0xF0, 0x43, 0x48,
	0x37, 0x96, 0x9C, 0xC6, 0x9C, 0xC2, 0x1E, 0xE8,
	0xF2, 0x0C, 0x0B, 0xEF, 0x86, 0xA2, 0xE3, 0x70,
	0x95, 0xC8, 0xB3, 0x06, 0x47, 0xA9, 0x90, 0xE8,
	0xA0, 0xC6, 0x72, 0x69, 0x05, 0xC0, 0x0D, 0x0E,
	0x21, 0x96, 0x65, 0x93, 0x74, 0x43, 0x2A, 0x1D,
	0x2E, 0xBF, 0xC2, 0xC2, 0xEE, 0xCC, 0x2F, 0x0A };



static int
test_AES_CBC_HMAC_SHA512_decrypt_create_session_params(
		struct crypto_unittest_params *ut_params);

static int
test_AES_CBC_HMAC_SHA512_decrypt_perform(struct rte_cryptodev_sym_session *sess,
		struct crypto_unittest_params *ut_params,
		struct crypto_testsuite_params *ts_params);


static int
test_AES_CBC_HMAC_SHA512_decrypt_create_session_params(
		struct crypto_unittest_params *ut_params)
{

	/* Setup Cipher Parameters */
	ut_params->cipher_xform.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	ut_params->cipher_xform.next = NULL;

	ut_params->cipher_xform.cipher.algo = RTE_CRYPTO_CIPHER_AES_CBC;
	ut_params->cipher_xform.cipher.op = RTE_CRYPTO_CIPHER_OP_DECRYPT;
	ut_params->cipher_xform.cipher.key.data = aes_cbc_key;
	ut_params->cipher_xform.cipher.key.length = CIPHER_KEY_LENGTH_AES_CBC;

	/* Setup HMAC Parameters */
	ut_params->auth_xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	ut_params->auth_xform.next = &ut_params->cipher_xform;

	ut_params->auth_xform.auth.op = RTE_CRYPTO_AUTH_OP_VERIFY;
	ut_params->auth_xform.auth.algo = RTE_CRYPTO_AUTH_SHA512_HMAC;
	ut_params->auth_xform.auth.key.data = hmac_sha512_key;
	ut_params->auth_xform.auth.key.length = HMAC_KEY_LENGTH_SHA512;
	ut_params->auth_xform.auth.digest_length = DIGEST_BYTE_LENGTH_SHA512;

	return TEST_SUCCESS;
}


static int
test_AES_CBC_HMAC_SHA512_decrypt_perform(struct rte_cryptodev_sym_session *sess,
		struct crypto_unittest_params *ut_params,
		struct crypto_testsuite_params *ts_params)
{
	/* Generate test mbuf data and digest */
	ut_params->ibuf = setup_test_string(ts_params->mbuf_pool,
			(const char *)
			catch_22_quote_2_512_bytes_AES_CBC_ciphertext,
			QUOTE_512_BYTES, 0);

	ut_params->digest = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
			DIGEST_BYTE_LENGTH_SHA512);
	TEST_ASSERT_NOT_NULL(ut_params->digest, "no room to append digest");

	rte_memcpy(ut_params->digest,
			catch_22_quote_2_512_bytes_AES_CBC_HMAC_SHA512_digest,
			DIGEST_BYTE_LENGTH_SHA512);

	/* Generate Crypto op data structure */
	ut_params->op = rte_crypto_op_alloc(ts_params->op_mpool,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	TEST_ASSERT_NOT_NULL(ut_params->op,
			"Failed to allocate symmetric crypto operation struct");

	rte_crypto_op_attach_sym_session(ut_params->op, sess);

	struct rte_crypto_sym_op *sym_op = ut_params->op->sym;

	/* set crypto operation source mbuf */
	sym_op->m_src = ut_params->ibuf;

	sym_op->auth.digest.data = ut_params->digest;
	sym_op->auth.digest.phys_addr = rte_pktmbuf_mtophys_offset(
			ut_params->ibuf, QUOTE_512_BYTES);
	sym_op->auth.digest.length = DIGEST_BYTE_LENGTH_SHA512;

	sym_op->auth.data.offset = CIPHER_IV_LENGTH_AES_CBC;
	sym_op->auth.data.length = QUOTE_512_BYTES;

	sym_op->cipher.iv.data = (uint8_t *)rte_pktmbuf_prepend(
			ut_params->ibuf, CIPHER_IV_LENGTH_AES_CBC);
	sym_op->cipher.iv.phys_addr = rte_pktmbuf_mtophys_offset(
			ut_params->ibuf, 0);
	sym_op->cipher.iv.length = CIPHER_IV_LENGTH_AES_CBC;

	rte_memcpy(sym_op->cipher.iv.data, aes_cbc_iv,
			CIPHER_IV_LENGTH_AES_CBC);

	sym_op->cipher.data.offset = CIPHER_IV_LENGTH_AES_CBC;
	sym_op->cipher.data.length = QUOTE_512_BYTES;

	/* Process crypto operation */
	TEST_ASSERT_NOT_NULL(process_crypto_request(ts_params->valid_devs[0],
			ut_params->op), "failed to process sym crypto op");

	TEST_ASSERT_EQUAL(ut_params->op->status, RTE_CRYPTO_OP_STATUS_SUCCESS,
			"crypto op processing failed");

	ut_params->obuf = ut_params->op->sym->m_src;

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			rte_pktmbuf_mtod(ut_params->obuf, uint8_t *) +
			CIPHER_IV_LENGTH_AES_CBC, catch_22_quote,
			QUOTE_512_BYTES,
			"Plaintext data not as expected");

	/* Validate obuf */
	TEST_ASSERT_EQUAL(ut_params->op->status, RTE_CRYPTO_OP_STATUS_SUCCESS,
			"Digest verification failed");

	return TEST_SUCCESS;
}

static int
test_AES_mb_all(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	int status;

	status = test_AES_all_tests(ts_params->mbuf_pool,
		ts_params->op_mpool, ts_params->valid_devs[0],
		RTE_CRYPTODEV_AESNI_MB_PMD);

	TEST_ASSERT_EQUAL(status, 0, "Test failed");

	return TEST_SUCCESS;
}

static int
test_AES_qat_all(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	int status;

	status = test_AES_all_tests(ts_params->mbuf_pool,
		ts_params->op_mpool, ts_params->valid_devs[0],
		RTE_CRYPTODEV_QAT_SYM_PMD);

	TEST_ASSERT_EQUAL(status, 0, "Test failed");

	return TEST_SUCCESS;
}

/* ***** Snow3G Tests ***** */
static int
create_snow3g_kasumi_hash_session(uint8_t dev_id,
	const uint8_t *key, const uint8_t key_len,
	const uint8_t aad_len, const uint8_t auth_len,
	enum rte_crypto_auth_operation op,
	enum rte_crypto_auth_algorithm algo)
{
	uint8_t hash_key[key_len];

	struct crypto_unittest_params *ut_params = &unittest_params;

	memcpy(hash_key, key, key_len);

	TEST_HEXDUMP(stdout, "key:", key, key_len);

	/* Setup Authentication Parameters */
	ut_params->auth_xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	ut_params->auth_xform.next = NULL;

	ut_params->auth_xform.auth.op = op;
	ut_params->auth_xform.auth.algo = algo;
	ut_params->auth_xform.auth.key.length = key_len;
	ut_params->auth_xform.auth.key.data = hash_key;
	ut_params->auth_xform.auth.digest_length = auth_len;
	ut_params->auth_xform.auth.add_auth_data_length = aad_len;
	ut_params->sess = rte_cryptodev_sym_session_create(dev_id,
				&ut_params->auth_xform);
	TEST_ASSERT_NOT_NULL(ut_params->sess, "Session creation failed");
	return 0;
}

static int
create_snow3g_kasumi_cipher_session(uint8_t dev_id,
			enum rte_crypto_cipher_operation op,
			enum rte_crypto_cipher_algorithm algo,
			const uint8_t *key, const uint8_t key_len)
{
	uint8_t cipher_key[key_len];

	struct crypto_unittest_params *ut_params = &unittest_params;

	memcpy(cipher_key, key, key_len);

	/* Setup Cipher Parameters */
	ut_params->cipher_xform.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	ut_params->cipher_xform.next = NULL;

	ut_params->cipher_xform.cipher.algo = algo;
	ut_params->cipher_xform.cipher.op = op;
	ut_params->cipher_xform.cipher.key.data = cipher_key;
	ut_params->cipher_xform.cipher.key.length = key_len;

	TEST_HEXDUMP(stdout, "key:", key, key_len);

	/* Create Crypto session */
	ut_params->sess = rte_cryptodev_sym_session_create(dev_id,
						&ut_params->
						cipher_xform);
	TEST_ASSERT_NOT_NULL(ut_params->sess, "Session creation failed");
	return 0;
}

static int
create_snow3g_kasumi_cipher_operation(const uint8_t *iv, const unsigned iv_len,
			const unsigned cipher_len,
			const unsigned cipher_offset,
			enum rte_crypto_cipher_algorithm algo)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;
	unsigned iv_pad_len = 0;

	/* Generate Crypto op data structure */
	ut_params->op = rte_crypto_op_alloc(ts_params->op_mpool,
				RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	TEST_ASSERT_NOT_NULL(ut_params->op,
				"Failed to allocate pktmbuf offload");

	/* Set crypto operation data parameters */
	rte_crypto_op_attach_sym_session(ut_params->op, ut_params->sess);

	struct rte_crypto_sym_op *sym_op = ut_params->op->sym;

	/* set crypto operation source mbuf */
	sym_op->m_src = ut_params->ibuf;

	/* iv */
	if (algo == RTE_CRYPTO_CIPHER_KASUMI_F8)
		iv_pad_len = RTE_ALIGN_CEIL(iv_len, 8);
	else
		iv_pad_len = RTE_ALIGN_CEIL(iv_len, 16);

	sym_op->cipher.iv.data = (uint8_t *)rte_pktmbuf_prepend(ut_params->ibuf
			, iv_pad_len);

	TEST_ASSERT_NOT_NULL(sym_op->cipher.iv.data, "no room to prepend iv");

	memset(sym_op->cipher.iv.data, 0, iv_pad_len);
	sym_op->cipher.iv.phys_addr = rte_pktmbuf_mtophys(ut_params->ibuf);
	sym_op->cipher.iv.length = iv_pad_len;

	rte_memcpy(sym_op->cipher.iv.data, iv, iv_len);
	sym_op->cipher.data.length = cipher_len;
	sym_op->cipher.data.offset = cipher_offset;
	return 0;
}

static int
create_snow3g_kasumi_cipher_operation_oop(const uint8_t *iv, const uint8_t iv_len,
			const unsigned cipher_len,
			const unsigned cipher_offset,
			enum rte_crypto_cipher_algorithm algo)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;
	unsigned iv_pad_len = 0;

	/* Generate Crypto op data structure */
	ut_params->op = rte_crypto_op_alloc(ts_params->op_mpool,
				RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	TEST_ASSERT_NOT_NULL(ut_params->op,
				"Failed to allocate pktmbuf offload");

	/* Set crypto operation data parameters */
	rte_crypto_op_attach_sym_session(ut_params->op, ut_params->sess);

	struct rte_crypto_sym_op *sym_op = ut_params->op->sym;

	/* set crypto operation source mbuf */
	sym_op->m_src = ut_params->ibuf;
	sym_op->m_dst = ut_params->obuf;

	/* iv */
	if (algo == RTE_CRYPTO_CIPHER_KASUMI_F8)
		iv_pad_len = RTE_ALIGN_CEIL(iv_len, 8);
	else
		iv_pad_len = RTE_ALIGN_CEIL(iv_len, 16);
	sym_op->cipher.iv.data = (uint8_t *)rte_pktmbuf_prepend(ut_params->ibuf,
					iv_pad_len);

	TEST_ASSERT_NOT_NULL(sym_op->cipher.iv.data, "no room to prepend iv");

	memset(sym_op->cipher.iv.data, 0, iv_pad_len);
	sym_op->cipher.iv.phys_addr = rte_pktmbuf_mtophys(ut_params->ibuf);
	sym_op->cipher.iv.length = iv_pad_len;

	rte_memcpy(sym_op->cipher.iv.data, iv, iv_len);
	sym_op->cipher.data.length = cipher_len;
	sym_op->cipher.data.offset = cipher_offset;
	return 0;
}

static int
create_snow3g_kasumi_cipher_auth_session(uint8_t dev_id,
		enum rte_crypto_cipher_operation cipher_op,
		enum rte_crypto_auth_operation auth_op,
		enum rte_crypto_auth_algorithm auth_algo,
		enum rte_crypto_cipher_algorithm cipher_algo,
		const uint8_t *key, const uint8_t key_len,
		const uint8_t aad_len, const uint8_t auth_len)

{
	uint8_t cipher_auth_key[key_len];

	struct crypto_unittest_params *ut_params = &unittest_params;

	memcpy(cipher_auth_key, key, key_len);

	/* Setup Authentication Parameters */
	ut_params->auth_xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	ut_params->auth_xform.next = NULL;

	ut_params->auth_xform.auth.op = auth_op;
	ut_params->auth_xform.auth.algo = auth_algo;
	ut_params->auth_xform.auth.key.length = key_len;
	/* Hash key = cipher key */
	ut_params->auth_xform.auth.key.data = cipher_auth_key;
	ut_params->auth_xform.auth.digest_length = auth_len;
	ut_params->auth_xform.auth.add_auth_data_length = aad_len;

	/* Setup Cipher Parameters */
	ut_params->cipher_xform.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	ut_params->cipher_xform.next = &ut_params->auth_xform;

	ut_params->cipher_xform.cipher.algo = cipher_algo;
	ut_params->cipher_xform.cipher.op = cipher_op;
	ut_params->cipher_xform.cipher.key.data = cipher_auth_key;
	ut_params->cipher_xform.cipher.key.length = key_len;

	TEST_HEXDUMP(stdout, "key:", key, key_len);

	/* Create Crypto session*/
	ut_params->sess = rte_cryptodev_sym_session_create(dev_id,
				&ut_params->cipher_xform);

	TEST_ASSERT_NOT_NULL(ut_params->sess, "Session creation failed");
	return 0;
}

static int
create_snow3g_kasumi_auth_cipher_session(uint8_t dev_id,
		enum rte_crypto_cipher_operation cipher_op,
		enum rte_crypto_auth_operation auth_op,
		enum rte_crypto_auth_algorithm auth_algo,
		enum rte_crypto_cipher_algorithm cipher_algo,
		const uint8_t *key, const uint8_t key_len,
		const uint8_t aad_len, const uint8_t auth_len)
{
	uint8_t auth_cipher_key[key_len];

	struct crypto_unittest_params *ut_params = &unittest_params;

	memcpy(auth_cipher_key, key, key_len);

	/* Setup Authentication Parameters */
	ut_params->auth_xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	ut_params->auth_xform.auth.op = auth_op;
	ut_params->auth_xform.next = &ut_params->cipher_xform;
	ut_params->auth_xform.auth.algo = auth_algo;
	ut_params->auth_xform.auth.key.length = key_len;
	ut_params->auth_xform.auth.key.data = auth_cipher_key;
	ut_params->auth_xform.auth.digest_length = auth_len;
	ut_params->auth_xform.auth.add_auth_data_length = aad_len;

	/* Setup Cipher Parameters */
	ut_params->cipher_xform.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	ut_params->cipher_xform.next = NULL;
	ut_params->cipher_xform.cipher.algo = cipher_algo;
	ut_params->cipher_xform.cipher.op = cipher_op;
	ut_params->cipher_xform.cipher.key.data = auth_cipher_key;
	ut_params->cipher_xform.cipher.key.length = key_len;

	TEST_HEXDUMP(stdout, "key:", key, key_len);

	/* Create Crypto session*/
	ut_params->sess = rte_cryptodev_sym_session_create(dev_id,
				&ut_params->auth_xform);

	TEST_ASSERT_NOT_NULL(ut_params->sess, "Session creation failed");

	return 0;
}

static int
create_snow3g_kasumi_hash_operation(const uint8_t *auth_tag,
		const unsigned auth_tag_len,
		const uint8_t *aad, const unsigned aad_len,
		unsigned data_pad_len,
		enum rte_crypto_auth_operation op,
		enum rte_crypto_auth_algorithm algo,
		const unsigned auth_len, const unsigned auth_offset)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;

	struct crypto_unittest_params *ut_params = &unittest_params;

	unsigned aad_buffer_len;

	/* Generate Crypto op data structure */
	ut_params->op = rte_crypto_op_alloc(ts_params->op_mpool,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	TEST_ASSERT_NOT_NULL(ut_params->op,
		"Failed to allocate pktmbuf offload");

	/* Set crypto operation data parameters */
	rte_crypto_op_attach_sym_session(ut_params->op, ut_params->sess);

	struct rte_crypto_sym_op *sym_op = ut_params->op->sym;

	/* set crypto operation source mbuf */
	sym_op->m_src = ut_params->ibuf;

	/* aad */
	/*
	* Always allocate the aad up to the block size.
	* The cryptodev API calls out -
	*  - the array must be big enough to hold the AAD, plus any
	*   space to round this up to the nearest multiple of the
	*   block size (8 bytes for KASUMI and 16 bytes for SNOW3G).
	*/
	if (algo == RTE_CRYPTO_AUTH_KASUMI_F9)
		aad_buffer_len = ALIGN_POW2_ROUNDUP(aad_len, 8);
	else
		aad_buffer_len = ALIGN_POW2_ROUNDUP(aad_len, 16);
	sym_op->auth.aad.data = (uint8_t *)rte_pktmbuf_prepend(
			ut_params->ibuf, aad_buffer_len);
	TEST_ASSERT_NOT_NULL(sym_op->auth.aad.data,
					"no room to prepend aad");
	sym_op->auth.aad.phys_addr = rte_pktmbuf_mtophys(
			ut_params->ibuf);
	sym_op->auth.aad.length = aad_len;

	memset(sym_op->auth.aad.data, 0, aad_buffer_len);
	rte_memcpy(sym_op->auth.aad.data, aad, aad_len);

	TEST_HEXDUMP(stdout, "aad:",
			sym_op->auth.aad.data, aad_len);

	/* digest */
	sym_op->auth.digest.data = (uint8_t *)rte_pktmbuf_append(
					ut_params->ibuf, auth_tag_len);

	TEST_ASSERT_NOT_NULL(sym_op->auth.digest.data,
				"no room to append auth tag");
	ut_params->digest = sym_op->auth.digest.data;
	sym_op->auth.digest.phys_addr = rte_pktmbuf_mtophys_offset(
			ut_params->ibuf, data_pad_len + aad_len);
	sym_op->auth.digest.length = auth_tag_len;
	if (op == RTE_CRYPTO_AUTH_OP_GENERATE)
		memset(sym_op->auth.digest.data, 0, auth_tag_len);
	else
		rte_memcpy(sym_op->auth.digest.data, auth_tag, auth_tag_len);

	TEST_HEXDUMP(stdout, "digest:",
		sym_op->auth.digest.data,
		sym_op->auth.digest.length);

	sym_op->auth.data.length = auth_len;
	sym_op->auth.data.offset = auth_offset;

	return 0;
}

static int
create_snow3g_kasumi_cipher_hash_operation(const uint8_t *auth_tag,
		const unsigned auth_tag_len,
		const uint8_t *aad, const uint8_t aad_len,
		unsigned data_pad_len,
		enum rte_crypto_auth_operation op,
		enum rte_crypto_auth_algorithm auth_algo,
		enum rte_crypto_cipher_algorithm cipher_algo,
		const uint8_t *iv, const uint8_t iv_len,
		const unsigned cipher_len, const unsigned cipher_offset,
		const unsigned auth_len, const unsigned auth_offset)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	unsigned iv_pad_len = 0;
	unsigned aad_buffer_len;

	/* Generate Crypto op data structure */
	ut_params->op = rte_crypto_op_alloc(ts_params->op_mpool,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	TEST_ASSERT_NOT_NULL(ut_params->op,
			"Failed to allocate pktmbuf offload");
	/* Set crypto operation data parameters */
	rte_crypto_op_attach_sym_session(ut_params->op, ut_params->sess);

	struct rte_crypto_sym_op *sym_op = ut_params->op->sym;

	/* set crypto operation source mbuf */
	sym_op->m_src = ut_params->ibuf;


	/* iv */
	if (cipher_algo == RTE_CRYPTO_CIPHER_KASUMI_F8)
		iv_pad_len = RTE_ALIGN_CEIL(iv_len, 8);
	else
		iv_pad_len = RTE_ALIGN_CEIL(iv_len, 16);

	sym_op->cipher.iv.data = (uint8_t *)rte_pktmbuf_prepend(
		ut_params->ibuf, iv_pad_len);
	TEST_ASSERT_NOT_NULL(sym_op->cipher.iv.data, "no room to prepend iv");

	memset(sym_op->cipher.iv.data, 0, iv_pad_len);
	sym_op->cipher.iv.phys_addr = rte_pktmbuf_mtophys(ut_params->ibuf);
	sym_op->cipher.iv.length = iv_pad_len;

	rte_memcpy(sym_op->cipher.iv.data, iv, iv_len);

	sym_op->cipher.data.length = cipher_len;
	sym_op->cipher.data.offset = cipher_offset;

	/* aad */
	/*
	* Always allocate the aad up to the block size.
	* The cryptodev API calls out -
	*  - the array must be big enough to hold the AAD, plus any
	*   space to round this up to the nearest multiple of the
	*   block size (8 bytes for KASUMI and 16 bytes for SNOW3G).
	*/
	if (auth_algo == RTE_CRYPTO_AUTH_KASUMI_F9)
		aad_buffer_len = ALIGN_POW2_ROUNDUP(aad_len, 8);
	else
		aad_buffer_len = ALIGN_POW2_ROUNDUP(aad_len, 16);

	sym_op->auth.aad.data =
			(uint8_t *)rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *);
	TEST_ASSERT_NOT_NULL(sym_op->auth.aad.data,
			"no room to prepend aad");
	sym_op->auth.aad.phys_addr = rte_pktmbuf_mtophys(
			ut_params->ibuf);
	sym_op->auth.aad.length = aad_len;

	memset(sym_op->auth.aad.data, 0, aad_buffer_len);
	rte_memcpy(sym_op->auth.aad.data, aad, aad_len);

	TEST_HEXDUMP(stdout, "aad:",
			sym_op->auth.aad.data, aad_len);

	/* digest */
	sym_op->auth.digest.data = (uint8_t *)rte_pktmbuf_append(
			ut_params->ibuf, auth_tag_len);

	TEST_ASSERT_NOT_NULL(sym_op->auth.digest.data,
			"no room to append auth tag");
	ut_params->digest = sym_op->auth.digest.data;
	sym_op->auth.digest.phys_addr = rte_pktmbuf_mtophys_offset(
			ut_params->ibuf, data_pad_len + aad_len);
	sym_op->auth.digest.length = auth_tag_len;
	if (op == RTE_CRYPTO_AUTH_OP_GENERATE)
		memset(sym_op->auth.digest.data, 0, auth_tag_len);
	else
		rte_memcpy(sym_op->auth.digest.data, auth_tag, auth_tag_len);

	TEST_HEXDUMP(stdout, "digest:",
		sym_op->auth.digest.data,
		sym_op->auth.digest.length);

	sym_op->auth.data.length = auth_len;
	sym_op->auth.data.offset = auth_offset;

	return 0;
}

static int
create_snow3g_kasumi_auth_cipher_operation(const unsigned auth_tag_len,
		const uint8_t *iv, const uint8_t iv_len,
		const uint8_t *aad, const uint8_t aad_len,
		unsigned data_pad_len,
		const unsigned cipher_len, const unsigned cipher_offset,
		const unsigned auth_len, const unsigned auth_offset,
		enum rte_crypto_auth_algorithm auth_algo,
		enum rte_crypto_cipher_algorithm cipher_algo)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	unsigned iv_pad_len = 0;
	unsigned aad_buffer_len = 0;

	/* Generate Crypto op data structure */
	ut_params->op = rte_crypto_op_alloc(ts_params->op_mpool,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	TEST_ASSERT_NOT_NULL(ut_params->op,
			"Failed to allocate pktmbuf offload");

	/* Set crypto operation data parameters */
	rte_crypto_op_attach_sym_session(ut_params->op, ut_params->sess);

	struct rte_crypto_sym_op *sym_op = ut_params->op->sym;

	/* set crypto operation source mbuf */
	sym_op->m_src = ut_params->ibuf;

	/* digest */
	sym_op->auth.digest.data = (uint8_t *)rte_pktmbuf_append(
			ut_params->ibuf, auth_tag_len);

	TEST_ASSERT_NOT_NULL(sym_op->auth.digest.data,
			"no room to append auth tag");

	sym_op->auth.digest.phys_addr = rte_pktmbuf_mtophys_offset(
			ut_params->ibuf, data_pad_len);
	sym_op->auth.digest.length = auth_tag_len;

	memset(sym_op->auth.digest.data, 0, auth_tag_len);

	TEST_HEXDUMP(stdout, "digest:",
			sym_op->auth.digest.data,
			sym_op->auth.digest.length);

	/* iv */
	if (cipher_algo == RTE_CRYPTO_CIPHER_KASUMI_F8)
		iv_pad_len = RTE_ALIGN_CEIL(iv_len, 8);
	else
		iv_pad_len = RTE_ALIGN_CEIL(iv_len, 16);

	sym_op->cipher.iv.data = (uint8_t *)rte_pktmbuf_prepend(
		ut_params->ibuf, iv_pad_len);
	TEST_ASSERT_NOT_NULL(sym_op->cipher.iv.data, "no room to prepend iv");

	memset(sym_op->cipher.iv.data, 0, iv_pad_len);
	sym_op->cipher.iv.phys_addr = rte_pktmbuf_mtophys(ut_params->ibuf);
	sym_op->cipher.iv.length = iv_pad_len;

	rte_memcpy(sym_op->cipher.iv.data, iv, iv_len);

	/* aad */
	/*
	* Always allocate the aad up to the block size.
	* The cryptodev API calls out -
	*  - the array must be big enough to hold the AAD, plus any
	*   space to round this up to the nearest multiple of the
	*   block size (8 bytes for KASUMI 16 bytes).
	*/
	if (auth_algo == RTE_CRYPTO_AUTH_KASUMI_F9)
		aad_buffer_len = ALIGN_POW2_ROUNDUP(aad_len, 8);
	else
		aad_buffer_len = ALIGN_POW2_ROUNDUP(aad_len, 16);

	sym_op->auth.aad.data = (uint8_t *)rte_pktmbuf_prepend(
	ut_params->ibuf, aad_buffer_len);
	TEST_ASSERT_NOT_NULL(sym_op->auth.aad.data,
				"no room to prepend aad");
	sym_op->auth.aad.phys_addr = rte_pktmbuf_mtophys(
				ut_params->ibuf);
	sym_op->auth.aad.length = aad_len;

	memset(sym_op->auth.aad.data, 0, aad_buffer_len);
	rte_memcpy(sym_op->auth.aad.data, aad, aad_len);

	TEST_HEXDUMP(stdout, "aad:",
			sym_op->auth.aad.data, aad_len);

	sym_op->cipher.data.length = cipher_len;
	sym_op->cipher.data.offset = auth_offset + cipher_offset;

	sym_op->auth.data.length = auth_len;
	sym_op->auth.data.offset = auth_offset + cipher_offset;

	return 0;
}

static int
test_snow3g_authentication(const struct snow3g_hash_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;
	unsigned plaintext_pad_len;
	unsigned plaintext_len;
	uint8_t *plaintext;

	/* Create SNOW3G session */
	retval = create_snow3g_kasumi_hash_session(ts_params->valid_devs[0],
			tdata->key.data, tdata->key.len,
			tdata->aad.len, tdata->digest.len,
			RTE_CRYPTO_AUTH_OP_GENERATE,
			RTE_CRYPTO_AUTH_SNOW3G_UIA2);
	if (retval < 0)
		return retval;

	/* alloc mbuf and set payload */
	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
	rte_pktmbuf_tailroom(ut_params->ibuf));

	plaintext_len = ceil_byte_length(tdata->plaintext.len);
	/* Append data which is padded to a multiple of */
	/* the algorithms block size */
	plaintext_pad_len = RTE_ALIGN_CEIL(plaintext_len, 16);
	plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				plaintext_pad_len);
	memcpy(plaintext, tdata->plaintext.data, plaintext_len);

	/* Create SNOW3G operation */
	retval = create_snow3g_kasumi_hash_operation(NULL, tdata->digest.len,
			tdata->aad.data, tdata->aad.len,
			plaintext_pad_len, RTE_CRYPTO_AUTH_OP_GENERATE,
			RTE_CRYPTO_AUTH_SNOW3G_UIA2,
			tdata->validAuthLenInBits.len,
			tdata->validAuthOffsetLenInBits.len);
	if (retval < 0)
		return retval;

	ut_params->op = process_crypto_request(ts_params->valid_devs[0],
				ut_params->op);
	ut_params->obuf = ut_params->op->sym->m_src;
	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");
	ut_params->digest = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *)
			+ plaintext_pad_len + tdata->aad.len;

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL(
	ut_params->digest,
	tdata->digest.data,
	DIGEST_BYTE_LENGTH_SNOW3G_UIA2,
	"Snow3G Generated auth tag not as expected");

	return 0;
}

static int
test_snow3g_authentication_verify(const struct snow3g_hash_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;
	unsigned plaintext_pad_len;
	unsigned plaintext_len;
	uint8_t *plaintext;

	/* Create SNOW3G session */
	retval = create_snow3g_kasumi_hash_session(ts_params->valid_devs[0],
				tdata->key.data, tdata->key.len,
				tdata->aad.len, tdata->digest.len,
				RTE_CRYPTO_AUTH_OP_VERIFY,
				RTE_CRYPTO_AUTH_SNOW3G_UIA2);
	if (retval < 0)
		return retval;
	/* alloc mbuf and set payload */
	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
	rte_pktmbuf_tailroom(ut_params->ibuf));

	plaintext_len = ceil_byte_length(tdata->plaintext.len);
	/* Append data which is padded to a multiple of */
	/* the algorithms block size */
	plaintext_pad_len = RTE_ALIGN_CEIL(plaintext_len, 16);
	plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				plaintext_pad_len);
	memcpy(plaintext, tdata->plaintext.data, plaintext_len);

	/* Create SNOW3G operation */
	retval = create_snow3g_kasumi_hash_operation(tdata->digest.data,
			tdata->digest.len,
			tdata->aad.data, tdata->aad.len,
			plaintext_pad_len,
			RTE_CRYPTO_AUTH_OP_VERIFY,
			RTE_CRYPTO_AUTH_SNOW3G_UIA2,
			tdata->validAuthLenInBits.len,
			tdata->validAuthOffsetLenInBits.len);
	if (retval < 0)
		return retval;

	ut_params->op = process_crypto_request(ts_params->valid_devs[0],
				ut_params->op);
	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");
	ut_params->obuf = ut_params->op->sym->m_src;
	ut_params->digest = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *)
				+ plaintext_pad_len + tdata->aad.len;

	/* Validate obuf */
	if (ut_params->op->status == RTE_CRYPTO_OP_STATUS_SUCCESS)
		return 0;
	else
		return -1;

	return 0;
}

static int
test_kasumi_authentication(const struct kasumi_hash_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;
	unsigned plaintext_pad_len;
	unsigned plaintext_len;
	uint8_t *plaintext;

	/* Create KASUMI session */
	retval = create_snow3g_kasumi_hash_session(ts_params->valid_devs[0],
			tdata->key.data, tdata->key.len,
			tdata->aad.len, tdata->digest.len,
			RTE_CRYPTO_AUTH_OP_GENERATE,
			RTE_CRYPTO_AUTH_KASUMI_F9);
	if (retval < 0)
		return retval;

	/* alloc mbuf and set payload */
	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
	rte_pktmbuf_tailroom(ut_params->ibuf));

	plaintext_len = ceil_byte_length(tdata->plaintext.len);
	/* Append data which is padded to a multiple of */
	/* the algorithms block size */
	plaintext_pad_len = RTE_ALIGN_CEIL(plaintext_len, 8);
	plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				plaintext_pad_len);
	memcpy(plaintext, tdata->plaintext.data, plaintext_len);

	/* Create KASUMI operation */
	retval = create_snow3g_kasumi_hash_operation(NULL, tdata->digest.len,
			tdata->aad.data, tdata->aad.len,
			plaintext_pad_len, RTE_CRYPTO_AUTH_OP_GENERATE,
			RTE_CRYPTO_AUTH_KASUMI_F9,
			tdata->validAuthLenInBits.len,
			tdata->validAuthOffsetLenInBits.len);
	if (retval < 0)
		return retval;

	ut_params->op = process_crypto_request(ts_params->valid_devs[0],
				ut_params->op);
	ut_params->obuf = ut_params->op->sym->m_src;
	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");
	ut_params->digest = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *)
			+ plaintext_pad_len + ALIGN_POW2_ROUNDUP(tdata->aad.len, 8);

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL(
	ut_params->digest,
	tdata->digest.data,
	DIGEST_BYTE_LENGTH_KASUMI_F9,
	"KASUMI Generated auth tag not as expected");

	return 0;
}

static int
test_kasumi_authentication_verify(const struct kasumi_hash_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;
	unsigned plaintext_pad_len;
	unsigned plaintext_len;
	uint8_t *plaintext;

	/* Create KASUMI session */
	retval = create_snow3g_kasumi_hash_session(ts_params->valid_devs[0],
				tdata->key.data, tdata->key.len,
				tdata->aad.len, tdata->digest.len,
				RTE_CRYPTO_AUTH_OP_VERIFY,
				RTE_CRYPTO_AUTH_KASUMI_F9);
	if (retval < 0)
		return retval;
	/* alloc mbuf and set payload */
	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
	rte_pktmbuf_tailroom(ut_params->ibuf));

	plaintext_len = ceil_byte_length(tdata->plaintext.len);
	/* Append data which is padded to a multiple */
	/* of the algorithms block size */
	plaintext_pad_len = RTE_ALIGN_CEIL(plaintext_len, 8);
	plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				plaintext_pad_len);
	memcpy(plaintext, tdata->plaintext.data, plaintext_len);

	/* Create KASUMI operation */
	retval = create_snow3g_kasumi_hash_operation(tdata->digest.data,
			tdata->digest.len,
			tdata->aad.data, tdata->aad.len,
			plaintext_pad_len,
			RTE_CRYPTO_AUTH_OP_VERIFY,
			RTE_CRYPTO_AUTH_KASUMI_F9,
			tdata->validAuthLenInBits.len,
			tdata->validAuthOffsetLenInBits.len);
	if (retval < 0)
		return retval;

	ut_params->op = process_crypto_request(ts_params->valid_devs[0],
				ut_params->op);
	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");
	ut_params->obuf = ut_params->op->sym->m_src;
	ut_params->digest = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *)
				+ plaintext_pad_len + tdata->aad.len;

	/* Validate obuf */
	if (ut_params->op->status == RTE_CRYPTO_OP_STATUS_SUCCESS)
		return 0;
	else
		return -1;

	return 0;
}

static int
test_snow3g_hash_generate_test_case_1(void)
{
	return test_snow3g_authentication(&snow3g_hash_test_case_1);
}

static int
test_snow3g_hash_generate_test_case_2(void)
{
	return test_snow3g_authentication(&snow3g_hash_test_case_2);
}

static int
test_snow3g_hash_generate_test_case_3(void)
{
	return test_snow3g_authentication(&snow3g_hash_test_case_3);
}

static int
test_snow3g_hash_generate_test_case_4(void)
{
	return test_snow3g_authentication(&snow3g_hash_test_case_4);
}

static int
test_snow3g_hash_generate_test_case_5(void)
{
	return test_snow3g_authentication(&snow3g_hash_test_case_5);
}

static int
test_snow3g_hash_generate_test_case_6(void)
{
	return test_snow3g_authentication(&snow3g_hash_test_case_6);
}

static int
test_snow3g_hash_verify_test_case_1(void)
{
	return test_snow3g_authentication_verify(&snow3g_hash_test_case_1);

}

static int
test_snow3g_hash_verify_test_case_2(void)
{
	return test_snow3g_authentication_verify(&snow3g_hash_test_case_2);
}

static int
test_snow3g_hash_verify_test_case_3(void)
{
	return test_snow3g_authentication_verify(&snow3g_hash_test_case_3);
}

static int
test_snow3g_hash_verify_test_case_4(void)
{
	return test_snow3g_authentication_verify(&snow3g_hash_test_case_4);
}

static int
test_snow3g_hash_verify_test_case_5(void)
{
	return test_snow3g_authentication_verify(&snow3g_hash_test_case_5);
}

static int
test_snow3g_hash_verify_test_case_6(void)
{
	return test_snow3g_authentication_verify(&snow3g_hash_test_case_6);
}

static int
test_kasumi_hash_generate_test_case_1(void)
{
	return test_kasumi_authentication(&kasumi_hash_test_case_1);
}

static int
test_kasumi_hash_generate_test_case_2(void)
{
	return test_kasumi_authentication(&kasumi_hash_test_case_2);
}

static int
test_kasumi_hash_generate_test_case_3(void)
{
	return test_kasumi_authentication(&kasumi_hash_test_case_3);
}

static int
test_kasumi_hash_generate_test_case_4(void)
{
	return test_kasumi_authentication(&kasumi_hash_test_case_4);
}

static int
test_kasumi_hash_generate_test_case_5(void)
{
	return test_kasumi_authentication(&kasumi_hash_test_case_5);
}

static int
test_kasumi_hash_verify_test_case_1(void)
{
	return test_kasumi_authentication_verify(&kasumi_hash_test_case_1);
}

static int
test_kasumi_hash_verify_test_case_2(void)
{
	return test_kasumi_authentication_verify(&kasumi_hash_test_case_2);
}

static int
test_kasumi_hash_verify_test_case_3(void)
{
	return test_kasumi_authentication_verify(&kasumi_hash_test_case_3);
}

static int
test_kasumi_hash_verify_test_case_4(void)
{
	return test_kasumi_authentication_verify(&kasumi_hash_test_case_4);
}

static int
test_kasumi_hash_verify_test_case_5(void)
{
	return test_kasumi_authentication_verify(&kasumi_hash_test_case_5);
}

static int
test_kasumi_encryption(const struct kasumi_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;
	uint8_t *plaintext, *ciphertext;
	unsigned plaintext_pad_len;
	unsigned plaintext_len;

	/* Create KASUMI session */
	retval = create_snow3g_kasumi_cipher_session(ts_params->valid_devs[0],
					RTE_CRYPTO_CIPHER_OP_ENCRYPT,
					RTE_CRYPTO_CIPHER_KASUMI_F8,
					tdata->key.data, tdata->key.len);
	if (retval < 0)
		return retval;

	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	/* Clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
	       rte_pktmbuf_tailroom(ut_params->ibuf));

	plaintext_len = ceil_byte_length(tdata->plaintext.len);
	/* Append data which is padded to a multiple */
	/* of the algorithms block size */
	plaintext_pad_len = RTE_ALIGN_CEIL(plaintext_len, 8);
	plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				plaintext_pad_len);
	memcpy(plaintext, tdata->plaintext.data, plaintext_len);

	TEST_HEXDUMP(stdout, "plaintext:", plaintext, plaintext_len);

	/* Create KASUMI operation */
	retval = create_snow3g_kasumi_cipher_operation(tdata->iv.data, tdata->iv.len,
					tdata->plaintext.len,
					tdata->validCipherOffsetLenInBits.len,
					RTE_CRYPTO_CIPHER_KASUMI_F8);
	if (retval < 0)
		return retval;

	ut_params->op = process_crypto_request(ts_params->valid_devs[0],
						ut_params->op);
	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");

	ut_params->obuf = ut_params->op->sym->m_dst;
	if (ut_params->obuf)
		ciphertext = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *)
				+ tdata->iv.len;
	else
		ciphertext = plaintext;

	TEST_HEXDUMP(stdout, "ciphertext:", ciphertext, plaintext_len);

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(
		ciphertext,
		tdata->ciphertext.data,
		tdata->validCipherLenInBits.len,
		"KASUMI Ciphertext data not as expected");
	return 0;
}

static int
test_kasumi_encryption_oop(const struct kasumi_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;
	uint8_t *plaintext, *ciphertext;
	unsigned plaintext_pad_len;
	unsigned plaintext_len;

	/* Create KASUMI session */
	retval = create_snow3g_kasumi_cipher_session(ts_params->valid_devs[0],
					RTE_CRYPTO_CIPHER_OP_ENCRYPT,
					RTE_CRYPTO_CIPHER_KASUMI_F8,
					tdata->key.data, tdata->key.len);
	if (retval < 0)
		return retval;

	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);
	ut_params->obuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	/* Clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
	       rte_pktmbuf_tailroom(ut_params->ibuf));

	plaintext_len = ceil_byte_length(tdata->plaintext.len);
	/* Append data which is padded to a multiple */
	/* of the algorithms block size */
	plaintext_pad_len = RTE_ALIGN_CEIL(plaintext_len, 8);
	plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				plaintext_pad_len);
	rte_pktmbuf_append(ut_params->obuf, plaintext_pad_len);
	memcpy(plaintext, tdata->plaintext.data, plaintext_len);

	TEST_HEXDUMP(stdout, "plaintext:", plaintext, plaintext_len);

	/* Create KASUMI operation */
	retval = create_snow3g_kasumi_cipher_operation_oop(tdata->iv.data,
					tdata->iv.len,
					tdata->plaintext.len,
					tdata->validCipherOffsetLenInBits.len,
					RTE_CRYPTO_CIPHER_KASUMI_F8);
	if (retval < 0)
		return retval;

	ut_params->op = process_crypto_request(ts_params->valid_devs[0],
						ut_params->op);
	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");

	ut_params->obuf = ut_params->op->sym->m_dst;
	if (ut_params->obuf)
		ciphertext = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *)
				+ tdata->iv.len;
	else
		ciphertext = plaintext;

	TEST_HEXDUMP(stdout, "ciphertext:", ciphertext, plaintext_len);

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(
		ciphertext,
		tdata->ciphertext.data,
		tdata->validCipherLenInBits.len,
		"KASUMI Ciphertext data not as expected");
	return 0;
}

static int
test_kasumi_decryption_oop(const struct kasumi_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;
	uint8_t *ciphertext, *plaintext;
	unsigned ciphertext_pad_len;
	unsigned ciphertext_len;

	/* Create KASUMI session */
	retval = create_snow3g_kasumi_cipher_session(ts_params->valid_devs[0],
					RTE_CRYPTO_CIPHER_OP_DECRYPT,
					RTE_CRYPTO_CIPHER_KASUMI_F8,
					tdata->key.data, tdata->key.len);
	if (retval < 0)
		return retval;

	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);
	ut_params->obuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	/* Clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
	       rte_pktmbuf_tailroom(ut_params->ibuf));

	ciphertext_len = ceil_byte_length(tdata->ciphertext.len);
	/* Append data which is padded to a multiple */
	/* of the algorithms block size */
	ciphertext_pad_len = RTE_ALIGN_CEIL(ciphertext_len, 8);
	ciphertext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				ciphertext_pad_len);
	rte_pktmbuf_append(ut_params->obuf, ciphertext_pad_len);
	memcpy(ciphertext, tdata->ciphertext.data, ciphertext_len);

	TEST_HEXDUMP(stdout, "ciphertext:", ciphertext, ciphertext_len);

	/* Create KASUMI operation */
	retval = create_snow3g_kasumi_cipher_operation_oop(tdata->iv.data,
					tdata->iv.len,
					tdata->ciphertext.len,
					tdata->validCipherOffsetLenInBits.len,
					RTE_CRYPTO_CIPHER_KASUMI_F8);
	if (retval < 0)
		return retval;

	ut_params->op = process_crypto_request(ts_params->valid_devs[0],
						ut_params->op);
	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");

	ut_params->obuf = ut_params->op->sym->m_dst;
	if (ut_params->obuf)
		plaintext = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *)
				+ tdata->iv.len;
	else
		plaintext = ciphertext;

	TEST_HEXDUMP(stdout, "plaintext:", plaintext, ciphertext_len);

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(
		plaintext,
		tdata->plaintext.data,
		tdata->validCipherLenInBits.len,
		"KASUMI Plaintext data not as expected");
	return 0;
}

static int
test_kasumi_decryption(const struct kasumi_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;
	uint8_t *ciphertext, *plaintext;
	unsigned ciphertext_pad_len;
	unsigned ciphertext_len;

	/* Create KASUMI session */
	retval = create_snow3g_kasumi_cipher_session(ts_params->valid_devs[0],
					RTE_CRYPTO_CIPHER_OP_DECRYPT,
					RTE_CRYPTO_CIPHER_KASUMI_F8,
					tdata->key.data, tdata->key.len);
	if (retval < 0)
		return retval;

	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	/* Clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
	       rte_pktmbuf_tailroom(ut_params->ibuf));

	ciphertext_len = ceil_byte_length(tdata->ciphertext.len);
	/* Append data which is padded to a multiple */
	/* of the algorithms block size */
	ciphertext_pad_len = RTE_ALIGN_CEIL(ciphertext_len, 8);
	ciphertext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				ciphertext_pad_len);
	memcpy(ciphertext, tdata->ciphertext.data, ciphertext_len);

	TEST_HEXDUMP(stdout, "ciphertext:", ciphertext, ciphertext_len);

	/* Create KASUMI operation */
	retval = create_snow3g_kasumi_cipher_operation(tdata->iv.data,
					tdata->iv.len,
					tdata->ciphertext.len,
					tdata->validCipherOffsetLenInBits.len,
					RTE_CRYPTO_CIPHER_KASUMI_F8);
	if (retval < 0)
		return retval;

	ut_params->op = process_crypto_request(ts_params->valid_devs[0],
						ut_params->op);
	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");

	ut_params->obuf = ut_params->op->sym->m_dst;
	if (ut_params->obuf)
		plaintext = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *)
				+ tdata->iv.len;
	else
		plaintext = ciphertext;

	TEST_HEXDUMP(stdout, "plaintext:", plaintext, ciphertext_len);

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(
		plaintext,
		tdata->plaintext.data,
		tdata->validCipherLenInBits.len,
		"KASUMI Plaintext data not as expected");
	return 0;
}

static int
test_snow3g_encryption(const struct snow3g_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;
	uint8_t *plaintext, *ciphertext;
	unsigned plaintext_pad_len;
	unsigned plaintext_len;

	/* Create SNOW3G session */
	retval = create_snow3g_kasumi_cipher_session(ts_params->valid_devs[0],
					RTE_CRYPTO_CIPHER_OP_ENCRYPT,
					RTE_CRYPTO_CIPHER_SNOW3G_UEA2,
					tdata->key.data, tdata->key.len);
	if (retval < 0)
		return retval;

	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	/* Clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
	       rte_pktmbuf_tailroom(ut_params->ibuf));

	plaintext_len = ceil_byte_length(tdata->plaintext.len);
	/* Append data which is padded to a multiple of */
	/* the algorithms block size */
	plaintext_pad_len = RTE_ALIGN_CEIL(plaintext_len, 16);
	plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				plaintext_pad_len);
	memcpy(plaintext, tdata->plaintext.data, plaintext_len);

	TEST_HEXDUMP(stdout, "plaintext:", plaintext, plaintext_len);

	/* Create SNOW3G operation */
	retval = create_snow3g_kasumi_cipher_operation(tdata->iv.data, tdata->iv.len,
					tdata->validCipherLenInBits.len,
					tdata->validCipherOffsetLenInBits.len,
					RTE_CRYPTO_CIPHER_SNOW3G_UEA2);
	if (retval < 0)
		return retval;

	ut_params->op = process_crypto_request(ts_params->valid_devs[0],
						ut_params->op);
	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");

	ut_params->obuf = ut_params->op->sym->m_dst;
	if (ut_params->obuf)
		ciphertext = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *)
				+ tdata->iv.len;
	else
		ciphertext = plaintext;

	TEST_HEXDUMP(stdout, "ciphertext:", ciphertext, plaintext_len);

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(
		ciphertext,
		tdata->ciphertext.data,
		tdata->validDataLenInBits.len,
		"Snow3G Ciphertext data not as expected");
	return 0;
}


static int
test_snow3g_encryption_oop(const struct snow3g_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;
	uint8_t *plaintext, *ciphertext;

	int retval;
	unsigned plaintext_pad_len;
	unsigned plaintext_len;

	/* Create SNOW3G session */
	retval = create_snow3g_kasumi_cipher_session(ts_params->valid_devs[0],
					RTE_CRYPTO_CIPHER_OP_ENCRYPT,
					RTE_CRYPTO_CIPHER_SNOW3G_UEA2,
					tdata->key.data, tdata->key.len);
	if (retval < 0)
		return retval;

	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);
	ut_params->obuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	TEST_ASSERT_NOT_NULL(ut_params->ibuf,
			"Failed to allocate input buffer in mempool");
	TEST_ASSERT_NOT_NULL(ut_params->obuf,
			"Failed to allocate output buffer in mempool");

	/* Clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
	       rte_pktmbuf_tailroom(ut_params->ibuf));

	plaintext_len = ceil_byte_length(tdata->plaintext.len);
	/* Append data which is padded to a multiple of */
	/* the algorithms block size */
	plaintext_pad_len = RTE_ALIGN_CEIL(plaintext_len, 16);
	plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				plaintext_pad_len);
	rte_pktmbuf_append(ut_params->obuf, plaintext_pad_len);
	memcpy(plaintext, tdata->plaintext.data, plaintext_len);

	TEST_HEXDUMP(stdout, "plaintext:", plaintext, plaintext_len);

	/* Create SNOW3G operation */
	retval = create_snow3g_kasumi_cipher_operation_oop(tdata->iv.data,
					tdata->iv.len,
					tdata->validCipherLenInBits.len,
					tdata->validCipherOffsetLenInBits.len,
					RTE_CRYPTO_CIPHER_SNOW3G_UEA2);
	if (retval < 0)
		return retval;

	ut_params->op = process_crypto_request(ts_params->valid_devs[0],
						ut_params->op);
	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");

	ut_params->obuf = ut_params->op->sym->m_dst;
	if (ut_params->obuf)
		ciphertext = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *)
				+ tdata->iv.len;
	else
		ciphertext = plaintext;

	TEST_HEXDUMP(stdout, "ciphertext:", ciphertext, plaintext_len);

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(
		ciphertext,
		tdata->ciphertext.data,
		tdata->validDataLenInBits.len,
		"Snow3G Ciphertext data not as expected");
	return 0;
}

/* Shift right a buffer by "offset" bits, "offset" < 8 */
static void
buffer_shift_right(uint8_t *buffer, uint32_t length, uint8_t offset)
{
	uint8_t curr_byte, prev_byte;
	uint32_t length_in_bytes = ceil_byte_length(length + offset);
	uint8_t lower_byte_mask = (1 << offset) - 1;
	unsigned i;

	prev_byte = buffer[0];
	buffer[0] >>= offset;

	for (i = 1; i < length_in_bytes; i++) {
		curr_byte = buffer[i];
		buffer[i] = ((prev_byte & lower_byte_mask) << (8 - offset)) |
				(curr_byte >> offset);
		prev_byte = curr_byte;
	}
}

static int
test_snow3g_encryption_offset_oop(const struct snow3g_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;
	uint8_t *plaintext, *ciphertext;
	int retval;
	uint32_t plaintext_len;
	uint32_t plaintext_pad_len;
	uint8_t extra_offset = 4;
	uint8_t *expected_ciphertext_shifted;

	/* Create SNOW3G session */
	retval = create_snow3g_kasumi_cipher_session(ts_params->valid_devs[0],
					RTE_CRYPTO_CIPHER_OP_ENCRYPT,
					RTE_CRYPTO_CIPHER_SNOW3G_UEA2,
					tdata->key.data, tdata->key.len);
	if (retval < 0)
		return retval;

	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);
	ut_params->obuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	TEST_ASSERT_NOT_NULL(ut_params->ibuf,
			"Failed to allocate input buffer in mempool");
	TEST_ASSERT_NOT_NULL(ut_params->obuf,
			"Failed to allocate output buffer in mempool");

	/* Clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
	       rte_pktmbuf_tailroom(ut_params->ibuf));

	plaintext_len = ceil_byte_length(tdata->plaintext.len + extra_offset);
	/*
	 * Append data which is padded to a
	 * multiple of the algorithms block size
	 */
	plaintext_pad_len = RTE_ALIGN_CEIL(plaintext_len, 16);

	plaintext = (uint8_t *) rte_pktmbuf_append(ut_params->ibuf,
						plaintext_pad_len);

	rte_pktmbuf_append(ut_params->obuf, plaintext_pad_len);

	memcpy(plaintext, tdata->plaintext.data, (tdata->plaintext.len >> 3));
	buffer_shift_right(plaintext, tdata->plaintext.len, extra_offset);

#ifdef RTE_APP_TEST_DEBUG
	rte_hexdump(stdout, "plaintext:", plaintext, tdata->plaintext.len);
#endif
	/* Create SNOW3G operation */
	retval = create_snow3g_kasumi_cipher_operation_oop(tdata->iv.data,
					tdata->iv.len,
					tdata->validCipherLenInBits.len,
					tdata->validCipherOffsetLenInBits.len +
					extra_offset,
					RTE_CRYPTO_CIPHER_SNOW3G_UEA2);
	if (retval < 0)
		return retval;

	ut_params->op = process_crypto_request(ts_params->valid_devs[0],
						ut_params->op);
	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");

	ut_params->obuf = ut_params->op->sym->m_dst;
	if (ut_params->obuf)
		ciphertext = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *)
				+ tdata->iv.len;
	else
		ciphertext = plaintext;

#ifdef RTE_APP_TEST_DEBUG
	rte_hexdump(stdout, "ciphertext:", ciphertext, plaintext_len);
#endif

	expected_ciphertext_shifted = rte_malloc(NULL,
			ceil_byte_length(plaintext_len + extra_offset), 0);

	TEST_ASSERT_NOT_NULL(expected_ciphertext_shifted,
			"failed to reserve memory for ciphertext shifted\n");

	memcpy(expected_ciphertext_shifted, tdata->ciphertext.data,
			ceil_byte_length(tdata->ciphertext.len));
	buffer_shift_right(expected_ciphertext_shifted, tdata->ciphertext.len,
			extra_offset);
	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT_OFFSET(
		ciphertext,
		expected_ciphertext_shifted,
		tdata->validDataLenInBits.len,
		extra_offset,
		"Snow3G Ciphertext data not as expected");
	return 0;
}

static int test_snow3g_decryption(const struct snow3g_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;

	uint8_t *plaintext, *ciphertext;
	unsigned ciphertext_pad_len;
	unsigned ciphertext_len;

	/* Create SNOW3G session */
	retval = create_snow3g_kasumi_cipher_session(ts_params->valid_devs[0],
					RTE_CRYPTO_CIPHER_OP_DECRYPT,
					RTE_CRYPTO_CIPHER_SNOW3G_UEA2,
					tdata->key.data, tdata->key.len);
	if (retval < 0)
		return retval;

	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	/* Clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
	       rte_pktmbuf_tailroom(ut_params->ibuf));

	ciphertext_len = ceil_byte_length(tdata->ciphertext.len);
	/* Append data which is padded to a multiple of */
	/* the algorithms block size */
	ciphertext_pad_len = RTE_ALIGN_CEIL(ciphertext_len, 16);
	ciphertext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				ciphertext_pad_len);
	memcpy(ciphertext, tdata->ciphertext.data, ciphertext_len);

	TEST_HEXDUMP(stdout, "ciphertext:", ciphertext, ciphertext_len);

	/* Create SNOW3G operation */
	retval = create_snow3g_kasumi_cipher_operation(tdata->iv.data, tdata->iv.len,
					tdata->validCipherLenInBits.len,
					tdata->validCipherOffsetLenInBits.len,
					RTE_CRYPTO_CIPHER_SNOW3G_UEA2);
	if (retval < 0)
		return retval;

	ut_params->op = process_crypto_request(ts_params->valid_devs[0],
						ut_params->op);
	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");
	ut_params->obuf = ut_params->op->sym->m_dst;
	if (ut_params->obuf)
		plaintext = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *)
				+ tdata->iv.len;
	else
		plaintext = ciphertext;

	TEST_HEXDUMP(stdout, "plaintext:", plaintext, ciphertext_len);

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(plaintext,
				tdata->plaintext.data,
				tdata->validDataLenInBits.len,
				"Snow3G Plaintext data not as expected");
	return 0;
}

static int test_snow3g_decryption_oop(const struct snow3g_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;

	uint8_t *plaintext, *ciphertext;
	unsigned ciphertext_pad_len;
	unsigned ciphertext_len;

	/* Create SNOW3G session */
	retval = create_snow3g_kasumi_cipher_session(ts_params->valid_devs[0],
					RTE_CRYPTO_CIPHER_OP_DECRYPT,
					RTE_CRYPTO_CIPHER_SNOW3G_UEA2,
					tdata->key.data, tdata->key.len);
	if (retval < 0)
		return retval;

	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);
	ut_params->obuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	TEST_ASSERT_NOT_NULL(ut_params->ibuf,
			"Failed to allocate input buffer");
	TEST_ASSERT_NOT_NULL(ut_params->obuf,
			"Failed to allocate output buffer");

	/* Clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
	       rte_pktmbuf_tailroom(ut_params->ibuf));

	memset(rte_pktmbuf_mtod(ut_params->obuf, uint8_t *), 0,
		       rte_pktmbuf_tailroom(ut_params->obuf));

	ciphertext_len = ceil_byte_length(tdata->ciphertext.len);
	/* Append data which is padded to a multiple of */
	/* the algorithms block size */
	ciphertext_pad_len = RTE_ALIGN_CEIL(ciphertext_len, 16);
	ciphertext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				ciphertext_pad_len);
	rte_pktmbuf_append(ut_params->obuf, ciphertext_pad_len);
	memcpy(ciphertext, tdata->ciphertext.data, ciphertext_len);

	TEST_HEXDUMP(stdout, "ciphertext:", ciphertext, ciphertext_len);

	/* Create SNOW3G operation */
	retval = create_snow3g_kasumi_cipher_operation_oop(tdata->iv.data,
					tdata->iv.len,
					tdata->validCipherLenInBits.len,
					tdata->validCipherOffsetLenInBits.len,
					RTE_CRYPTO_CIPHER_SNOW3G_UEA2);
	if (retval < 0)
		return retval;

	ut_params->op = process_crypto_request(ts_params->valid_devs[0],
						ut_params->op);
	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");
	ut_params->obuf = ut_params->op->sym->m_dst;
	if (ut_params->obuf)
		plaintext = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *)
				+ tdata->iv.len;
	else
		plaintext = ciphertext;

	TEST_HEXDUMP(stdout, "plaintext:", plaintext, ciphertext_len);

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(plaintext,
				tdata->plaintext.data,
				tdata->validDataLenInBits.len,
				"Snow3G Plaintext data not as expected");
	return 0;
}

static int
test_snow3g_authenticated_encryption(const struct snow3g_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;

	uint8_t *plaintext, *ciphertext;
	unsigned plaintext_pad_len;
	unsigned plaintext_len;

	/* Create SNOW3G session */
	retval = create_snow3g_kasumi_cipher_auth_session(ts_params->valid_devs[0],
			RTE_CRYPTO_CIPHER_OP_ENCRYPT,
			RTE_CRYPTO_AUTH_OP_GENERATE,
			RTE_CRYPTO_AUTH_SNOW3G_UIA2,
			RTE_CRYPTO_CIPHER_SNOW3G_UEA2,
			tdata->key.data, tdata->key.len,
			tdata->aad.len, tdata->digest.len);
	if (retval < 0)
		return retval;
	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	/* clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->ibuf));

	plaintext_len = ceil_byte_length(tdata->plaintext.len);
	/* Append data which is padded to a multiple of */
	/* the algorithms block size */
	plaintext_pad_len = RTE_ALIGN_CEIL(plaintext_len, 16);
	plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				plaintext_pad_len);
	memcpy(plaintext, tdata->plaintext.data, plaintext_len);

	TEST_HEXDUMP(stdout, "plaintext:", plaintext, plaintext_len);

	/* Create SNOW3G operation */
	retval = create_snow3g_kasumi_cipher_hash_operation(tdata->digest.data,
			tdata->digest.len, tdata->aad.data,
			tdata->aad.len, /*tdata->plaintext.len,*/
			plaintext_pad_len, RTE_CRYPTO_AUTH_OP_GENERATE,
			RTE_CRYPTO_AUTH_SNOW3G_UIA2,
			RTE_CRYPTO_CIPHER_SNOW3G_UEA2,
			tdata->iv.data, tdata->iv.len,
			tdata->validCipherLenInBits.len,
			tdata->validCipherOffsetLenInBits.len,
			tdata->validAuthLenInBits.len,
			tdata->validAuthOffsetLenInBits.len
			);
	if (retval < 0)
		return retval;

	ut_params->op = process_crypto_request(ts_params->valid_devs[0],
			ut_params->op);
	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");
	ut_params->obuf = ut_params->op->sym->m_src;
	if (ut_params->obuf)
		ciphertext = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *)
				+ tdata->iv.len;
	else
		ciphertext = plaintext;

	TEST_HEXDUMP(stdout, "ciphertext:", ciphertext, plaintext_len);

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(
			ciphertext,
			tdata->ciphertext.data,
			tdata->validDataLenInBits.len,
			"Snow3G Ciphertext data not as expected");

	ut_params->digest = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *)
	    + plaintext_pad_len + tdata->aad.len;

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			ut_params->digest,
			tdata->digest.data,
			DIGEST_BYTE_LENGTH_SNOW3G_UIA2,
			"Snow3G Generated auth tag not as expected");
	return 0;
}
static int
test_snow3g_encrypted_authentication(const struct snow3g_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;

	uint8_t *plaintext, *ciphertext;
	unsigned plaintext_pad_len;
	unsigned plaintext_len;

	/* Create SNOW3G session */
	retval = create_snow3g_kasumi_auth_cipher_session(ts_params->valid_devs[0],
			RTE_CRYPTO_CIPHER_OP_ENCRYPT,
			RTE_CRYPTO_AUTH_OP_GENERATE,
			RTE_CRYPTO_AUTH_SNOW3G_UIA2,
			RTE_CRYPTO_CIPHER_SNOW3G_UEA2,
			tdata->key.data, tdata->key.len,
			tdata->aad.len, tdata->digest.len);
	if (retval < 0)
		return retval;

	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	/* clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->ibuf));

	plaintext_len = ceil_byte_length(tdata->plaintext.len);
	/* Append data which is padded to a multiple of */
	/* the algorithms block size */
	plaintext_pad_len = RTE_ALIGN_CEIL(plaintext_len, 16);
	plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				plaintext_pad_len);
	memcpy(plaintext, tdata->plaintext.data, plaintext_len);

	TEST_HEXDUMP(stdout, "plaintext:", plaintext, plaintext_len);

	/* Create SNOW3G operation */
	retval = create_snow3g_kasumi_auth_cipher_operation(
		tdata->digest.len,
		tdata->iv.data, tdata->iv.len,
		tdata->aad.data, tdata->aad.len,
		plaintext_pad_len,
		tdata->validCipherLenInBits.len,
		tdata->validCipherOffsetLenInBits.len,
		tdata->validAuthLenInBits.len,
		tdata->validAuthOffsetLenInBits.len,
		RTE_CRYPTO_AUTH_SNOW3G_UIA2,
		RTE_CRYPTO_CIPHER_SNOW3G_UEA2
	);

	if (retval < 0)
		return retval;

	ut_params->op = process_crypto_request(ts_params->valid_devs[0],
			ut_params->op);
	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");
	ut_params->obuf = ut_params->op->sym->m_src;
	if (ut_params->obuf)
		ciphertext = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *)
				+ tdata->aad.len + tdata->iv.len;
	else
		ciphertext = plaintext;

	ut_params->digest = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *)
			+ plaintext_pad_len + tdata->aad.len + tdata->iv.len;
	TEST_HEXDUMP(stdout, "ciphertext:", ciphertext, plaintext_len);

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(
		ciphertext,
		tdata->ciphertext.data,
		tdata->validDataLenInBits.len,
		"Snow3G Ciphertext data not as expected");

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL(
		ut_params->digest,
		tdata->digest.data,
		DIGEST_BYTE_LENGTH_SNOW3G_UIA2,
		"Snow3G Generated auth tag not as expected");
	return 0;
}

static int
test_kasumi_encryption_test_case_1(void)
{
	return test_kasumi_encryption(&kasumi_test_case_1);
}

static int
test_kasumi_encryption_test_case_1_oop(void)
{
	return test_kasumi_encryption_oop(&kasumi_test_case_1);
}

static int
test_kasumi_encryption_test_case_2(void)
{
	return test_kasumi_encryption(&kasumi_test_case_2);
}

static int
test_kasumi_encryption_test_case_3(void)
{
	return test_kasumi_encryption(&kasumi_test_case_3);
}

static int
test_kasumi_encryption_test_case_4(void)
{
	return test_kasumi_encryption(&kasumi_test_case_4);
}

static int
test_kasumi_encryption_test_case_5(void)
{
	return test_kasumi_encryption(&kasumi_test_case_5);
}

static int
test_kasumi_decryption_test_case_1(void)
{
	return test_kasumi_decryption(&kasumi_test_case_1);
}

static int
test_kasumi_decryption_test_case_1_oop(void)
{
	return test_kasumi_decryption_oop(&kasumi_test_case_1);
}

static int
test_kasumi_decryption_test_case_2(void)
{
	return test_kasumi_decryption(&kasumi_test_case_2);
}

static int
test_kasumi_decryption_test_case_3(void)
{
	return test_kasumi_decryption(&kasumi_test_case_3);
}

static int
test_kasumi_decryption_test_case_4(void)
{
	return test_kasumi_decryption(&kasumi_test_case_4);
}

static int
test_kasumi_decryption_test_case_5(void)
{
	return test_kasumi_decryption(&kasumi_test_case_5);
}
static int
test_snow3g_encryption_test_case_1(void)
{
	return test_snow3g_encryption(&snow3g_test_case_1);
}

static int
test_snow3g_encryption_test_case_1_oop(void)
{
	return test_snow3g_encryption_oop(&snow3g_test_case_1);
}

static int
test_snow3g_encryption_test_case_1_offset_oop(void)
{
	return test_snow3g_encryption_offset_oop(&snow3g_test_case_1);
}

static int
test_snow3g_encryption_test_case_2(void)
{
	return test_snow3g_encryption(&snow3g_test_case_2);
}

static int
test_snow3g_encryption_test_case_3(void)
{
	return test_snow3g_encryption(&snow3g_test_case_3);
}

static int
test_snow3g_encryption_test_case_4(void)
{
	return test_snow3g_encryption(&snow3g_test_case_4);
}

static int
test_snow3g_encryption_test_case_5(void)
{
	return test_snow3g_encryption(&snow3g_test_case_5);
}

static int
test_snow3g_decryption_test_case_1(void)
{
	return test_snow3g_decryption(&snow3g_test_case_1);
}

static int
test_snow3g_decryption_test_case_1_oop(void)
{
	return test_snow3g_decryption_oop(&snow3g_test_case_1);
}

static int
test_snow3g_decryption_test_case_2(void)
{
	return test_snow3g_decryption(&snow3g_test_case_2);
}

static int
test_snow3g_decryption_test_case_3(void)
{
	return test_snow3g_decryption(&snow3g_test_case_3);
}

static int
test_snow3g_decryption_test_case_4(void)
{
	return test_snow3g_decryption(&snow3g_test_case_4);
}

static int
test_snow3g_decryption_test_case_5(void)
{
	return test_snow3g_decryption(&snow3g_test_case_5);
}
static int
test_snow3g_authenticated_encryption_test_case_1(void)
{
	return test_snow3g_authenticated_encryption(&snow3g_test_case_3);
}

static int
test_snow3g_encrypted_authentication_test_case_1(void)
{
	return test_snow3g_encrypted_authentication(&snow3g_test_case_6);
}

/* ***** AES-GCM Tests ***** */

static int
create_gcm_session(uint8_t dev_id, enum rte_crypto_cipher_operation op,
		const uint8_t *key, const uint8_t key_len,
		const uint8_t aad_len, const uint8_t auth_len)
{
	uint8_t cipher_key[key_len];

	struct crypto_unittest_params *ut_params = &unittest_params;


	memcpy(cipher_key, key, key_len);

	/* Setup Cipher Parameters */
	ut_params->cipher_xform.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	ut_params->cipher_xform.next = NULL;

	ut_params->cipher_xform.cipher.algo = RTE_CRYPTO_CIPHER_AES_GCM;
	ut_params->auth_xform.auth.op = RTE_CRYPTO_AUTH_OP_GENERATE;
	ut_params->cipher_xform.cipher.op = op;
	ut_params->cipher_xform.cipher.key.data = cipher_key;
	ut_params->cipher_xform.cipher.key.length = key_len;

	TEST_HEXDUMP(stdout, "key:", key, key_len);

	/* Setup Authentication Parameters */
	ut_params->auth_xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	ut_params->auth_xform.next = NULL;

	ut_params->auth_xform.auth.algo = RTE_CRYPTO_AUTH_AES_GCM;

	ut_params->auth_xform.auth.digest_length = auth_len;
	ut_params->auth_xform.auth.add_auth_data_length = aad_len;
	ut_params->auth_xform.auth.key.length = 0;
	ut_params->auth_xform.auth.key.data = NULL;

	if (op == RTE_CRYPTO_CIPHER_OP_ENCRYPT) {
		ut_params->cipher_xform.next = &ut_params->auth_xform;

		/* Create Crypto session*/
		ut_params->sess = rte_cryptodev_sym_session_create(dev_id,
				&ut_params->cipher_xform);
	} else {/* Create Crypto session*/
		ut_params->auth_xform.next = &ut_params->cipher_xform;
		ut_params->sess = rte_cryptodev_sym_session_create(dev_id,
				&ut_params->auth_xform);
	}

	TEST_ASSERT_NOT_NULL(ut_params->sess, "Session creation failed");

	return 0;
}

static int
create_gcm_operation(enum rte_crypto_cipher_operation op,
		const uint8_t *auth_tag, const unsigned auth_tag_len,
		const uint8_t *iv, const unsigned iv_len,
		const uint8_t *aad, const unsigned aad_len,
		const unsigned data_len, unsigned data_pad_len)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	unsigned iv_pad_len = 0, aad_buffer_len;

	/* Generate Crypto op data structure */
	ut_params->op = rte_crypto_op_alloc(ts_params->op_mpool,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	TEST_ASSERT_NOT_NULL(ut_params->op,
			"Failed to allocate symmetric crypto operation struct");

	struct rte_crypto_sym_op *sym_op = ut_params->op->sym;



	sym_op->auth.digest.data = (uint8_t *)rte_pktmbuf_append(
			ut_params->ibuf, auth_tag_len);
	TEST_ASSERT_NOT_NULL(sym_op->auth.digest.data,
			"no room to append digest");
	sym_op->auth.digest.phys_addr = rte_pktmbuf_mtophys_offset(
			ut_params->ibuf, data_pad_len);
	sym_op->auth.digest.length = auth_tag_len;

	if (op == RTE_CRYPTO_CIPHER_OP_DECRYPT) {
		rte_memcpy(sym_op->auth.digest.data, auth_tag, auth_tag_len);
		TEST_HEXDUMP(stdout, "digest:",
				sym_op->auth.digest.data,
				sym_op->auth.digest.length);
	}

	/* iv */
	iv_pad_len = RTE_ALIGN_CEIL(iv_len, 16);

	sym_op->cipher.iv.data = (uint8_t *)rte_pktmbuf_prepend(
			ut_params->ibuf, iv_pad_len);
	TEST_ASSERT_NOT_NULL(sym_op->cipher.iv.data, "no room to prepend iv");

	memset(sym_op->cipher.iv.data, 0, iv_pad_len);
	sym_op->cipher.iv.phys_addr = rte_pktmbuf_mtophys(ut_params->ibuf);
	sym_op->cipher.iv.length = iv_pad_len;

	rte_memcpy(sym_op->cipher.iv.data, iv, iv_len);

	/* CalcY0 */
	if (iv_len != 16)
		sym_op->cipher.iv.data[15] = 1;

	/*
	 * Always allocate the aad up to the block size.
	 * The cryptodev API calls out -
	 *  - the array must be big enough to hold the AAD, plus any
	 *   space to round this up to the nearest multiple of the
	 *   block size (16 bytes).
	 */
	aad_buffer_len = ALIGN_POW2_ROUNDUP(aad_len, 16);

	sym_op->auth.aad.data = (uint8_t *)rte_pktmbuf_prepend(
			ut_params->ibuf, aad_buffer_len);
	TEST_ASSERT_NOT_NULL(sym_op->auth.aad.data,
			"no room to prepend aad");
	sym_op->auth.aad.phys_addr = rte_pktmbuf_mtophys(
			ut_params->ibuf);
	sym_op->auth.aad.length = aad_len;

	memset(sym_op->auth.aad.data, 0, aad_buffer_len);
	rte_memcpy(sym_op->auth.aad.data, aad, aad_len);

	TEST_HEXDUMP(stdout, "iv:", sym_op->cipher.iv.data, iv_pad_len);
	TEST_HEXDUMP(stdout, "aad:",
			sym_op->auth.aad.data, aad_len);

	sym_op->cipher.data.length = data_len;
	sym_op->cipher.data.offset = aad_buffer_len + iv_pad_len;

	sym_op->auth.data.offset = aad_buffer_len + iv_pad_len;
	sym_op->auth.data.length = data_len;

	return 0;
}

static int
test_mb_AES_GCM_authenticated_encryption(const struct gcm_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;

	uint8_t *plaintext, *ciphertext, *auth_tag;
	uint16_t plaintext_pad_len;

	/* Create GCM session */
	retval = create_gcm_session(ts_params->valid_devs[0],
			RTE_CRYPTO_CIPHER_OP_ENCRYPT,
			tdata->key.data, tdata->key.len,
			tdata->aad.len, tdata->auth_tag.len);
	if (retval < 0)
		return retval;


	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	/* clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->ibuf));

	/*
	 * Append data which is padded to a multiple
	 * of the algorithms block size
	 */
	plaintext_pad_len = RTE_ALIGN_CEIL(tdata->plaintext.len, 16);

	plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
			plaintext_pad_len);
	memcpy(plaintext, tdata->plaintext.data, tdata->plaintext.len);

	TEST_HEXDUMP(stdout, "plaintext:", plaintext, tdata->plaintext.len);

	/* Create GCM opertaion */
	retval = create_gcm_operation(RTE_CRYPTO_CIPHER_OP_ENCRYPT,
			tdata->auth_tag.data, tdata->auth_tag.len,
			tdata->iv.data, tdata->iv.len,
			tdata->aad.data, tdata->aad.len,
			tdata->plaintext.len, plaintext_pad_len);
	if (retval < 0)
		return retval;

	rte_crypto_op_attach_sym_session(ut_params->op, ut_params->sess);

	ut_params->op->sym->m_src = ut_params->ibuf;

	/* Process crypto operation */
	TEST_ASSERT_NOT_NULL(process_crypto_request(ts_params->valid_devs[0],
			ut_params->op), "failed to process sym crypto op");

	TEST_ASSERT_EQUAL(ut_params->op->status, RTE_CRYPTO_OP_STATUS_SUCCESS,
			"crypto op processing failed");

	if (ut_params->op->sym->m_dst) {
		ciphertext = rte_pktmbuf_mtod(ut_params->op->sym->m_dst,
				uint8_t *);
		auth_tag = rte_pktmbuf_mtod_offset(ut_params->op->sym->m_dst,
				uint8_t *, plaintext_pad_len);
	} else {
		ciphertext = plaintext;
		auth_tag = plaintext + plaintext_pad_len;
	}

	TEST_HEXDUMP(stdout, "ciphertext:", ciphertext, tdata->ciphertext.len);
	TEST_HEXDUMP(stdout, "auth tag:", auth_tag, tdata->auth_tag.len);

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			ciphertext,
			tdata->ciphertext.data,
			tdata->ciphertext.len,
			"GCM Ciphertext data not as expected");

	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			auth_tag,
			tdata->auth_tag.data,
			tdata->auth_tag.len,
			"GCM Generated auth tag not as expected");

	return 0;

}

static int
test_mb_AES_GCM_authenticated_encryption_test_case_1(void)
{
	return test_mb_AES_GCM_authenticated_encryption(&gcm_test_case_1);
}

static int
test_mb_AES_GCM_authenticated_encryption_test_case_2(void)
{
	return test_mb_AES_GCM_authenticated_encryption(&gcm_test_case_2);
}

static int
test_mb_AES_GCM_authenticated_encryption_test_case_3(void)
{
	return test_mb_AES_GCM_authenticated_encryption(&gcm_test_case_3);
}

static int
test_mb_AES_GCM_authenticated_encryption_test_case_4(void)
{
	return test_mb_AES_GCM_authenticated_encryption(&gcm_test_case_4);
}

static int
test_mb_AES_GCM_authenticated_encryption_test_case_5(void)
{
	return test_mb_AES_GCM_authenticated_encryption(&gcm_test_case_5);
}

static int
test_mb_AES_GCM_authenticated_encryption_test_case_6(void)
{
	return test_mb_AES_GCM_authenticated_encryption(&gcm_test_case_6);
}

static int
test_mb_AES_GCM_authenticated_encryption_test_case_7(void)
{
	return test_mb_AES_GCM_authenticated_encryption(&gcm_test_case_7);
}

static int
test_mb_AES_GCM_authenticated_decryption(const struct gcm_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;

	uint8_t *plaintext, *ciphertext;
	uint16_t ciphertext_pad_len;

	/* Create GCM session */
	retval = create_gcm_session(ts_params->valid_devs[0],
			RTE_CRYPTO_CIPHER_OP_DECRYPT,
			tdata->key.data, tdata->key.len,
			tdata->aad.len, tdata->auth_tag.len);
	if (retval < 0)
		return retval;


	/* alloc mbuf and set payload */
	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->ibuf));

	ciphertext_pad_len = RTE_ALIGN_CEIL(tdata->ciphertext.len, 16);

	ciphertext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
			ciphertext_pad_len);
	memcpy(ciphertext, tdata->ciphertext.data, tdata->ciphertext.len);

	TEST_HEXDUMP(stdout, "ciphertext:", ciphertext, tdata->ciphertext.len);

	/* Create GCM opertaion */
	retval = create_gcm_operation(RTE_CRYPTO_CIPHER_OP_DECRYPT,
			tdata->auth_tag.data, tdata->auth_tag.len,
			tdata->iv.data, tdata->iv.len,
			tdata->aad.data, tdata->aad.len,
			tdata->ciphertext.len, ciphertext_pad_len);
	if (retval < 0)
		return retval;


	rte_crypto_op_attach_sym_session(ut_params->op, ut_params->sess);

	ut_params->op->sym->m_src = ut_params->ibuf;

	/* Process crypto operation */
	TEST_ASSERT_NOT_NULL(process_crypto_request(ts_params->valid_devs[0],
			ut_params->op), "failed to process sym crypto op");

	TEST_ASSERT_EQUAL(ut_params->op->status, RTE_CRYPTO_OP_STATUS_SUCCESS,
			"crypto op processing failed");

	if (ut_params->op->sym->m_dst)
		plaintext = rte_pktmbuf_mtod(ut_params->op->sym->m_dst,
				uint8_t *);
	else
		plaintext = ciphertext;

	TEST_HEXDUMP(stdout, "plaintext:", plaintext, tdata->ciphertext.len);

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			plaintext,
			tdata->plaintext.data,
			tdata->plaintext.len,
			"GCM plaintext data not as expected");

	TEST_ASSERT_EQUAL(ut_params->op->status,
			RTE_CRYPTO_OP_STATUS_SUCCESS,
			"GCM authentication failed");
	return 0;
}

static int
test_mb_AES_GCM_authenticated_decryption_test_case_1(void)
{
	return test_mb_AES_GCM_authenticated_decryption(&gcm_test_case_1);
}

static int
test_mb_AES_GCM_authenticated_decryption_test_case_2(void)
{
	return test_mb_AES_GCM_authenticated_decryption(&gcm_test_case_2);
}

static int
test_mb_AES_GCM_authenticated_decryption_test_case_3(void)
{
	return test_mb_AES_GCM_authenticated_decryption(&gcm_test_case_3);
}

static int
test_mb_AES_GCM_authenticated_decryption_test_case_4(void)
{
	return test_mb_AES_GCM_authenticated_decryption(&gcm_test_case_4);
}

static int
test_mb_AES_GCM_authenticated_decryption_test_case_5(void)
{
	return test_mb_AES_GCM_authenticated_decryption(&gcm_test_case_5);
}

static int
test_mb_AES_GCM_authenticated_decryption_test_case_6(void)
{
	return test_mb_AES_GCM_authenticated_decryption(&gcm_test_case_6);
}

static int
test_mb_AES_GCM_authenticated_decryption_test_case_7(void)
{
	return test_mb_AES_GCM_authenticated_decryption(&gcm_test_case_7);
}

static int
test_stats(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct rte_cryptodev_stats stats;
	struct rte_cryptodev *dev;
	cryptodev_stats_get_t temp_pfn;

	rte_cryptodev_stats_reset(ts_params->valid_devs[0]);
	TEST_ASSERT((rte_cryptodev_stats_get(ts_params->valid_devs[0] + 600,
			&stats) == -ENODEV),
		"rte_cryptodev_stats_get invalid dev failed");
	TEST_ASSERT((rte_cryptodev_stats_get(ts_params->valid_devs[0], 0) != 0),
		"rte_cryptodev_stats_get invalid Param failed");
	dev = &rte_cryptodevs[ts_params->valid_devs[0]];
	temp_pfn = dev->dev_ops->stats_get;
	dev->dev_ops->stats_get = (cryptodev_stats_get_t)0;
	TEST_ASSERT((rte_cryptodev_stats_get(ts_params->valid_devs[0], &stats)
			== -ENOTSUP),
		"rte_cryptodev_stats_get invalid Param failed");
	dev->dev_ops->stats_get = temp_pfn;

	/* Test expected values */
	ut_setup();
	test_AES_CBC_HMAC_SHA1_encrypt_digest();
	ut_teardown();
	TEST_ASSERT_SUCCESS(rte_cryptodev_stats_get(ts_params->valid_devs[0],
			&stats),
		"rte_cryptodev_stats_get failed");
	TEST_ASSERT((stats.enqueued_count == 1),
		"rte_cryptodev_stats_get returned unexpected enqueued stat");
	TEST_ASSERT((stats.dequeued_count == 1),
		"rte_cryptodev_stats_get returned unexpected enqueued stat");
	TEST_ASSERT((stats.enqueue_err_count == 0),
		"rte_cryptodev_stats_get returned unexpected enqueued stat");
	TEST_ASSERT((stats.dequeue_err_count == 0),
		"rte_cryptodev_stats_get returned unexpected enqueued stat");

	/* invalid device but should ignore and not reset device stats*/
	rte_cryptodev_stats_reset(ts_params->valid_devs[0] + 300);
	TEST_ASSERT_SUCCESS(rte_cryptodev_stats_get(ts_params->valid_devs[0],
			&stats),
		"rte_cryptodev_stats_get failed");
	TEST_ASSERT((stats.enqueued_count == 1),
		"rte_cryptodev_stats_get returned unexpected enqueued stat");

	/* check that a valid reset clears stats */
	rte_cryptodev_stats_reset(ts_params->valid_devs[0]);
	TEST_ASSERT_SUCCESS(rte_cryptodev_stats_get(ts_params->valid_devs[0],
			&stats),
					  "rte_cryptodev_stats_get failed");
	TEST_ASSERT((stats.enqueued_count == 0),
		"rte_cryptodev_stats_get returned unexpected enqueued stat");
	TEST_ASSERT((stats.dequeued_count == 0),
		"rte_cryptodev_stats_get returned unexpected enqueued stat");

	return TEST_SUCCESS;
}


static int
test_multi_session(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	struct rte_cryptodev_info dev_info;
	struct rte_cryptodev_sym_session **sessions;

	uint16_t i;

	test_AES_CBC_HMAC_SHA512_decrypt_create_session_params(ut_params);


	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);

	sessions = rte_malloc(NULL,
			(sizeof(struct rte_cryptodev_sym_session *) *
			dev_info.sym.max_nb_sessions) + 1, 0);

	/* Create multiple crypto sessions*/
	for (i = 0; i < dev_info.sym.max_nb_sessions; i++) {
		sessions[i] = rte_cryptodev_sym_session_create(
				ts_params->valid_devs[0],
			&ut_params->auth_xform);
		TEST_ASSERT_NOT_NULL(sessions[i],
				"Session creation failed at session number %u",
				i);

		/* Attempt to send a request on each session */
		TEST_ASSERT_SUCCESS(test_AES_CBC_HMAC_SHA512_decrypt_perform(
				sessions[i], ut_params, ts_params),
				"Failed to perform decrypt on request "
				"number %u.", i);
		/* free crypto operation structure */
		if (ut_params->op)
			rte_crypto_op_free(ut_params->op);

		/*
		 * free mbuf - both obuf and ibuf are usually the same,
		 * so check if they point at the same address is necessary,
		 * to avoid freeing the mbuf twice.
		 */
		if (ut_params->obuf) {
			rte_pktmbuf_free(ut_params->obuf);
			if (ut_params->ibuf == ut_params->obuf)
				ut_params->ibuf = 0;
			ut_params->obuf = 0;
		}
		if (ut_params->ibuf) {
			rte_pktmbuf_free(ut_params->ibuf);
			ut_params->ibuf = 0;
		}
	}

	/* Next session create should fail */
	sessions[i] = rte_cryptodev_sym_session_create(ts_params->valid_devs[0],
			&ut_params->auth_xform);
	TEST_ASSERT_NULL(sessions[i],
			"Session creation succeeded unexpectedly!");

	for (i = 0; i < dev_info.sym.max_nb_sessions; i++)
		rte_cryptodev_sym_session_free(ts_params->valid_devs[0],
				sessions[i]);

	rte_free(sessions);

	return TEST_SUCCESS;
}

static int
test_null_cipher_only_operation(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	/* Generate test mbuf data and space for digest */
	ut_params->ibuf = setup_test_string(ts_params->mbuf_pool,
			catch_22_quote, QUOTE_512_BYTES, 0);

	/* Setup Cipher Parameters */
	ut_params->cipher_xform.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	ut_params->cipher_xform.next = NULL;

	ut_params->cipher_xform.cipher.algo = RTE_CRYPTO_CIPHER_NULL;
	ut_params->cipher_xform.cipher.op = RTE_CRYPTO_CIPHER_OP_ENCRYPT;

	/* Create Crypto session*/
	ut_params->sess = rte_cryptodev_sym_session_create(
			ts_params->valid_devs[0], &ut_params->cipher_xform);
	TEST_ASSERT_NOT_NULL(ut_params->sess, "Session creation failed");

	/* Generate Crypto op data structure */
	ut_params->op = rte_crypto_op_alloc(ts_params->op_mpool,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	TEST_ASSERT_NOT_NULL(ut_params->op,
			"Failed to allocate symmetric crypto operation struct");

	/* Set crypto operation data parameters */
	rte_crypto_op_attach_sym_session(ut_params->op, ut_params->sess);

	struct rte_crypto_sym_op *sym_op = ut_params->op->sym;

	/* set crypto operation source mbuf */
	sym_op->m_src = ut_params->ibuf;

	sym_op->cipher.data.offset = 0;
	sym_op->cipher.data.length = QUOTE_512_BYTES;

	/* Process crypto operation */
	ut_params->op = process_crypto_request(ts_params->valid_devs[0],
			ut_params->op);
	TEST_ASSERT_NOT_NULL(ut_params->op, "no crypto operation returned");

	TEST_ASSERT_EQUAL(ut_params->op->status, RTE_CRYPTO_OP_STATUS_SUCCESS,
			"crypto operation processing failed");

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			rte_pktmbuf_mtod(ut_params->op->sym->m_src, uint8_t *),
			catch_22_quote,
			QUOTE_512_BYTES,
			"Ciphertext data not as expected");

	return TEST_SUCCESS;
}

static int
test_null_auth_only_operation(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	/* Generate test mbuf data and space for digest */
	ut_params->ibuf = setup_test_string(ts_params->mbuf_pool,
			catch_22_quote, QUOTE_512_BYTES, 0);

	/* Setup HMAC Parameters */
	ut_params->auth_xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	ut_params->auth_xform.next = NULL;

	ut_params->auth_xform.auth.algo = RTE_CRYPTO_AUTH_NULL;
	ut_params->auth_xform.auth.op = RTE_CRYPTO_AUTH_OP_GENERATE;

	/* Create Crypto session*/
	ut_params->sess = rte_cryptodev_sym_session_create(
			ts_params->valid_devs[0], &ut_params->auth_xform);
	TEST_ASSERT_NOT_NULL(ut_params->sess, "Session creation failed");

	/* Generate Crypto op data structure */
	ut_params->op = rte_crypto_op_alloc(ts_params->op_mpool,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	TEST_ASSERT_NOT_NULL(ut_params->op,
			"Failed to allocate symmetric crypto operation struct");

	/* Set crypto operation data parameters */
	rte_crypto_op_attach_sym_session(ut_params->op, ut_params->sess);

	struct rte_crypto_sym_op *sym_op = ut_params->op->sym;

	sym_op->m_src = ut_params->ibuf;

	sym_op->auth.data.offset = 0;
	sym_op->auth.data.length = QUOTE_512_BYTES;

	/* Process crypto operation */
	ut_params->op = process_crypto_request(ts_params->valid_devs[0],
			ut_params->op);
	TEST_ASSERT_NOT_NULL(ut_params->op, "no crypto operation returned");

	TEST_ASSERT_EQUAL(ut_params->op->status, RTE_CRYPTO_OP_STATUS_SUCCESS,
			"crypto operation processing failed");

	return TEST_SUCCESS;
}

static int
test_null_cipher_auth_operation(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	/* Generate test mbuf data and space for digest */
	ut_params->ibuf = setup_test_string(ts_params->mbuf_pool,
			catch_22_quote, QUOTE_512_BYTES, 0);

	/* Setup Cipher Parameters */
	ut_params->cipher_xform.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	ut_params->cipher_xform.next = &ut_params->auth_xform;

	ut_params->cipher_xform.cipher.algo = RTE_CRYPTO_CIPHER_NULL;
	ut_params->cipher_xform.cipher.op = RTE_CRYPTO_CIPHER_OP_ENCRYPT;

	/* Setup HMAC Parameters */
	ut_params->auth_xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	ut_params->auth_xform.next = NULL;

	ut_params->auth_xform.auth.algo = RTE_CRYPTO_AUTH_NULL;
	ut_params->auth_xform.auth.op = RTE_CRYPTO_AUTH_OP_GENERATE;

	/* Create Crypto session*/
	ut_params->sess = rte_cryptodev_sym_session_create(
			ts_params->valid_devs[0], &ut_params->cipher_xform);
	TEST_ASSERT_NOT_NULL(ut_params->sess, "Session creation failed");

	/* Generate Crypto op data structure */
	ut_params->op = rte_crypto_op_alloc(ts_params->op_mpool,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	TEST_ASSERT_NOT_NULL(ut_params->op,
			"Failed to allocate symmetric crypto operation struct");

	/* Set crypto operation data parameters */
	rte_crypto_op_attach_sym_session(ut_params->op, ut_params->sess);

	struct rte_crypto_sym_op *sym_op = ut_params->op->sym;

	sym_op->m_src = ut_params->ibuf;

	sym_op->cipher.data.offset = 0;
	sym_op->cipher.data.length = QUOTE_512_BYTES;

	sym_op->auth.data.offset = 0;
	sym_op->auth.data.length = QUOTE_512_BYTES;

	/* Process crypto operation */
	ut_params->op = process_crypto_request(ts_params->valid_devs[0],
			ut_params->op);
	TEST_ASSERT_NOT_NULL(ut_params->op, "no crypto operation returned");

	TEST_ASSERT_EQUAL(ut_params->op->status, RTE_CRYPTO_OP_STATUS_SUCCESS,
			"crypto operation processing failed");

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			rte_pktmbuf_mtod(ut_params->op->sym->m_src, uint8_t *),
			catch_22_quote,
			QUOTE_512_BYTES,
			"Ciphertext data not as expected");

	return TEST_SUCCESS;
}

static int
test_null_auth_cipher_operation(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	/* Generate test mbuf data and space for digest */
	ut_params->ibuf = setup_test_string(ts_params->mbuf_pool,
			catch_22_quote, QUOTE_512_BYTES, 0);

	/* Setup Cipher Parameters */
	ut_params->cipher_xform.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	ut_params->cipher_xform.next = NULL;

	ut_params->cipher_xform.cipher.algo = RTE_CRYPTO_CIPHER_NULL;
	ut_params->cipher_xform.cipher.op = RTE_CRYPTO_CIPHER_OP_ENCRYPT;

	/* Setup HMAC Parameters */
	ut_params->auth_xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	ut_params->auth_xform.next = &ut_params->cipher_xform;

	ut_params->auth_xform.auth.algo = RTE_CRYPTO_AUTH_NULL;
	ut_params->auth_xform.auth.op = RTE_CRYPTO_AUTH_OP_GENERATE;

	/* Create Crypto session*/
	ut_params->sess = rte_cryptodev_sym_session_create(
			ts_params->valid_devs[0], &ut_params->cipher_xform);
	TEST_ASSERT_NOT_NULL(ut_params->sess, "Session creation failed");

	/* Generate Crypto op data structure */
	ut_params->op = rte_crypto_op_alloc(ts_params->op_mpool,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	TEST_ASSERT_NOT_NULL(ut_params->op,
			"Failed to allocate symmetric crypto operation struct");

	/* Set crypto operation data parameters */
	rte_crypto_op_attach_sym_session(ut_params->op, ut_params->sess);

	struct rte_crypto_sym_op *sym_op = ut_params->op->sym;

	sym_op->m_src = ut_params->ibuf;

	sym_op->cipher.data.offset = 0;
	sym_op->cipher.data.length = QUOTE_512_BYTES;

	sym_op->auth.data.offset = 0;
	sym_op->auth.data.length = QUOTE_512_BYTES;

	/* Process crypto operation */
	ut_params->op = process_crypto_request(ts_params->valid_devs[0],
			ut_params->op);
	TEST_ASSERT_NOT_NULL(ut_params->op, "no crypto operation returned");

	TEST_ASSERT_EQUAL(ut_params->op->status, RTE_CRYPTO_OP_STATUS_SUCCESS,
			"crypto operation processing failed");

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			rte_pktmbuf_mtod(ut_params->op->sym->m_src, uint8_t *),
			catch_22_quote,
			QUOTE_512_BYTES,
			"Ciphertext data not as expected");

	return TEST_SUCCESS;
}


static int
test_null_invalid_operation(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	/* Setup Cipher Parameters */
	ut_params->cipher_xform.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	ut_params->cipher_xform.next = NULL;

	ut_params->cipher_xform.cipher.algo = RTE_CRYPTO_CIPHER_AES_CBC;
	ut_params->cipher_xform.cipher.op = RTE_CRYPTO_CIPHER_OP_ENCRYPT;

	/* Create Crypto session*/
	ut_params->sess = rte_cryptodev_sym_session_create(
			ts_params->valid_devs[0], &ut_params->cipher_xform);
	TEST_ASSERT_NULL(ut_params->sess,
			"Session creation succeeded unexpectedly");


	/* Setup HMAC Parameters */
	ut_params->auth_xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	ut_params->auth_xform.next = NULL;

	ut_params->auth_xform.auth.algo = RTE_CRYPTO_AUTH_SHA1_HMAC;
	ut_params->auth_xform.auth.op = RTE_CRYPTO_AUTH_OP_GENERATE;

	/* Create Crypto session*/
	ut_params->sess = rte_cryptodev_sym_session_create(
			ts_params->valid_devs[0], &ut_params->auth_xform);
	TEST_ASSERT_NULL(ut_params->sess,
			"Session creation succeeded unexpectedly");

	return TEST_SUCCESS;
}


#define NULL_BURST_LENGTH (32)

static int
test_null_burst_operation(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	unsigned i, burst_len = NULL_BURST_LENGTH;

	struct rte_crypto_op *burst[NULL_BURST_LENGTH] = { NULL };
	struct rte_crypto_op *burst_dequeued[NULL_BURST_LENGTH] = { NULL };

	/* Setup Cipher Parameters */
	ut_params->cipher_xform.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	ut_params->cipher_xform.next = &ut_params->auth_xform;

	ut_params->cipher_xform.cipher.algo = RTE_CRYPTO_CIPHER_NULL;
	ut_params->cipher_xform.cipher.op = RTE_CRYPTO_CIPHER_OP_ENCRYPT;

	/* Setup HMAC Parameters */
	ut_params->auth_xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	ut_params->auth_xform.next = NULL;

	ut_params->auth_xform.auth.algo = RTE_CRYPTO_AUTH_NULL;
	ut_params->auth_xform.auth.op = RTE_CRYPTO_AUTH_OP_GENERATE;

	/* Create Crypto session*/
	ut_params->sess = rte_cryptodev_sym_session_create(
			ts_params->valid_devs[0], &ut_params->cipher_xform);
	TEST_ASSERT_NOT_NULL(ut_params->sess, "Session creation failed");

	TEST_ASSERT_EQUAL(rte_crypto_op_bulk_alloc(ts_params->op_mpool,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC, burst, burst_len),
			burst_len, "failed to generate burst of crypto ops");

	/* Generate an operation for each mbuf in burst */
	for (i = 0; i < burst_len; i++) {
		struct rte_mbuf *m = rte_pktmbuf_alloc(ts_params->mbuf_pool);

		TEST_ASSERT_NOT_NULL(m, "Failed to allocate mbuf");

		unsigned *data = (unsigned *)rte_pktmbuf_append(m,
				sizeof(unsigned));
		*data = i;

		rte_crypto_op_attach_sym_session(burst[i], ut_params->sess);

		burst[i]->sym->m_src = m;
	}

	/* Process crypto operation */
	TEST_ASSERT_EQUAL(rte_cryptodev_enqueue_burst(ts_params->valid_devs[0],
			0, burst, burst_len),
			burst_len,
			"Error enqueuing burst");

	TEST_ASSERT_EQUAL(rte_cryptodev_dequeue_burst(ts_params->valid_devs[0],
			0, burst_dequeued, burst_len),
			burst_len,
			"Error dequeuing burst");


	for (i = 0; i < burst_len; i++) {
		TEST_ASSERT_EQUAL(
			*rte_pktmbuf_mtod(burst[i]->sym->m_src, uint32_t *),
			*rte_pktmbuf_mtod(burst_dequeued[i]->sym->m_src,
					uint32_t *),
			"data not as expected");

		rte_pktmbuf_free(burst[i]->sym->m_src);
		rte_crypto_op_free(burst[i]);
	}

	return TEST_SUCCESS;
}




static struct unit_test_suite cryptodev_qat_testsuite  = {
	.suite_name = "Crypto QAT Unit Test Suite",
	.setup = testsuite_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {
		TEST_CASE_ST(ut_setup, ut_teardown,
				test_device_configure_invalid_dev_id),
		TEST_CASE_ST(ut_setup, ut_teardown,
				test_device_configure_invalid_queue_pair_ids),
		TEST_CASE_ST(ut_setup, ut_teardown,
				test_queue_pair_descriptor_setup),
		TEST_CASE_ST(ut_setup, ut_teardown,
				test_multi_session),

		TEST_CASE_ST(ut_setup, ut_teardown, test_AES_qat_all),
		TEST_CASE_ST(ut_setup, ut_teardown, test_stats),

		/** AES GCM Authenticated Encryption */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_authenticated_encryption_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_authenticated_encryption_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_authenticated_encryption_test_case_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_authenticated_encryption_test_case_4),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_authenticated_encryption_test_case_5),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_authenticated_encryption_test_case_6),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_authenticated_encryption_test_case_7),

		/** AES GCM Authenticated Decryption */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_authenticated_decryption_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_authenticated_decryption_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_authenticated_decryption_test_case_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_authenticated_decryption_test_case_4),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_authenticated_decryption_test_case_5),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_authenticated_decryption_test_case_6),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_authenticated_decryption_test_case_7),

		/** Snow3G encrypt only (UEA2) */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_encryption_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_encryption_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_encryption_test_case_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_encryption_test_case_4),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_encryption_test_case_5),

		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_encryption_test_case_1_oop),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_decryption_test_case_1_oop),

		/** Snow3G decrypt only (UEA2) */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_decryption_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_decryption_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_decryption_test_case_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_decryption_test_case_4),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_decryption_test_case_5),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_hash_generate_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_hash_generate_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_hash_generate_test_case_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_hash_verify_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_hash_verify_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_hash_verify_test_case_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_authenticated_encryption_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_encrypted_authentication_test_case_1),
		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static struct unit_test_suite cryptodev_aesni_mb_testsuite  = {
	.suite_name = "Crypto Device AESNI MB Unit Test Suite",
	.setup = testsuite_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {
		TEST_CASE_ST(ut_setup, ut_teardown, test_AES_mb_all),

		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static struct unit_test_suite cryptodev_aesni_gcm_testsuite  = {
	.suite_name = "Crypto Device AESNI GCM Unit Test Suite",
	.setup = testsuite_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {
		/** AES GCM Authenticated Encryption */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_authenticated_encryption_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_authenticated_encryption_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_authenticated_encryption_test_case_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_authenticated_encryption_test_case_4),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_authenticated_encryption_test_case_5),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_authenticated_encryption_test_case_6),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_authenticated_encryption_test_case_7),

		/** AES GCM Authenticated Decryption */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_authenticated_decryption_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_authenticated_decryption_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_authenticated_decryption_test_case_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_authenticated_decryption_test_case_4),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_authenticated_decryption_test_case_5),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_authenticated_decryption_test_case_6),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_authenticated_decryption_test_case_7),

		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static struct unit_test_suite cryptodev_sw_kasumi_testsuite  = {
	.suite_name = "Crypto Device SW KASUMI Unit Test Suite",
	.setup = testsuite_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {
		/** KASUMI encrypt only (UEA1) */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_encryption_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_encryption_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_encryption_test_case_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_encryption_test_case_4),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_encryption_test_case_5),
		/** KASUMI decrypt only (UEA1) */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_decryption_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_decryption_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_decryption_test_case_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_decryption_test_case_4),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_decryption_test_case_5),

		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_encryption_test_case_1_oop),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_decryption_test_case_1_oop),

		/** KASUMI hash only (UIA1) */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_hash_generate_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_hash_generate_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_hash_generate_test_case_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_hash_generate_test_case_4),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_hash_generate_test_case_5),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_hash_verify_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_hash_verify_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_hash_verify_test_case_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_hash_verify_test_case_4),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_hash_verify_test_case_5),

		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};
static struct unit_test_suite cryptodev_sw_snow3g_testsuite  = {
	.suite_name = "Crypto Device SW Snow3G Unit Test Suite",
	.setup = testsuite_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {
		/** Snow3G encrypt only (UEA2) */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_encryption_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_encryption_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_encryption_test_case_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_encryption_test_case_4),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_encryption_test_case_5),

		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_encryption_test_case_1_oop),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_decryption_test_case_1_oop),

		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_encryption_test_case_1_offset_oop),

		/** Snow3G decrypt only (UEA2) */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_decryption_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_decryption_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_decryption_test_case_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_decryption_test_case_4),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_decryption_test_case_5),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_hash_generate_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_hash_generate_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_hash_generate_test_case_3),
		/* Tests with buffers which length is not byte-aligned */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_hash_generate_test_case_4),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_hash_generate_test_case_5),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_hash_generate_test_case_6),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_hash_verify_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_hash_verify_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_hash_verify_test_case_3),
		/* Tests with buffers which length is not byte-aligned */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_hash_verify_test_case_4),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_hash_verify_test_case_5),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_hash_verify_test_case_6),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_authenticated_encryption_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_encrypted_authentication_test_case_1),

		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static struct unit_test_suite cryptodev_null_testsuite  = {
	.suite_name = "Crypto Device NULL Unit Test Suite",
	.setup = testsuite_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_null_auth_only_operation),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_null_cipher_only_operation),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_null_cipher_auth_operation),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_null_auth_cipher_operation),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_null_invalid_operation),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_null_burst_operation),

		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static int
test_cryptodev_qat(void /*argv __rte_unused, int argc __rte_unused*/)
{
	gbl_cryptodev_type = RTE_CRYPTODEV_QAT_SYM_PMD;
	return unit_test_suite_runner(&cryptodev_qat_testsuite);
}

static int
test_cryptodev_aesni_mb(void /*argv __rte_unused, int argc __rte_unused*/)
{
	gbl_cryptodev_type = RTE_CRYPTODEV_AESNI_MB_PMD;

	return unit_test_suite_runner(&cryptodev_aesni_mb_testsuite);
}

static int
test_cryptodev_aesni_gcm(void)
{
	gbl_cryptodev_type = RTE_CRYPTODEV_AESNI_GCM_PMD;

	return unit_test_suite_runner(&cryptodev_aesni_gcm_testsuite);
}

static int
test_cryptodev_null(void)
{
	gbl_cryptodev_type = RTE_CRYPTODEV_NULL_PMD;

	return unit_test_suite_runner(&cryptodev_null_testsuite);
}

static int
test_cryptodev_sw_snow3g(void /*argv __rte_unused, int argc __rte_unused*/)
{
	gbl_cryptodev_type = RTE_CRYPTODEV_SNOW3G_PMD;

	return unit_test_suite_runner(&cryptodev_sw_snow3g_testsuite);
}

static int
test_cryptodev_sw_kasumi(void /*argv __rte_unused, int argc __rte_unused*/)
{
	gbl_cryptodev_type = RTE_CRYPTODEV_KASUMI_PMD;

	return unit_test_suite_runner(&cryptodev_sw_kasumi_testsuite);
}

REGISTER_TEST_COMMAND(cryptodev_qat_autotest, test_cryptodev_qat);
REGISTER_TEST_COMMAND(cryptodev_aesni_mb_autotest, test_cryptodev_aesni_mb);
REGISTER_TEST_COMMAND(cryptodev_aesni_gcm_autotest, test_cryptodev_aesni_gcm);
REGISTER_TEST_COMMAND(cryptodev_null_autotest, test_cryptodev_null);
REGISTER_TEST_COMMAND(cryptodev_sw_snow3g_autotest, test_cryptodev_sw_snow3g);
REGISTER_TEST_COMMAND(cryptodev_sw_kasumi_autotest, test_cryptodev_sw_kasumi);
