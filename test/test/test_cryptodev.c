/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2015-2017 Intel Corporation. All rights reserved.
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

#ifdef RTE_LIBRTE_PMD_CRYPTO_SCHEDULER
#include <rte_cryptodev_scheduler.h>
#include <rte_cryptodev_scheduler_operations.h>
#endif

#include "test.h"
#include "test_cryptodev.h"

#include "test_cryptodev_blockcipher.h"
#include "test_cryptodev_aes_test_vectors.h"
#include "test_cryptodev_des_test_vectors.h"
#include "test_cryptodev_hash_test_vectors.h"
#include "test_cryptodev_kasumi_test_vectors.h"
#include "test_cryptodev_kasumi_hash_test_vectors.h"
#include "test_cryptodev_snow3g_test_vectors.h"
#include "test_cryptodev_snow3g_hash_test_vectors.h"
#include "test_cryptodev_zuc_test_vectors.h"
#include "test_cryptodev_gcm_test_vectors.h"
#include "test_cryptodev_hmac_test_vectors.h"

static enum rte_cryptodev_type gbl_cryptodev_type;

struct crypto_testsuite_params {
	struct rte_mempool *mbuf_pool;
	struct rte_mempool *large_mbuf_pool;
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
		struct crypto_unittest_params *ut_params, uint8_t *cipher_key,
		uint8_t *hmac_key);

static int
test_AES_CBC_HMAC_SHA512_decrypt_perform(struct rte_cryptodev_sym_session *sess,
		struct crypto_unittest_params *ut_params,
		struct crypto_testsuite_params *ts_param,
		const uint8_t *cipher,
		const uint8_t *digest,
		const uint8_t *iv);

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
	uint32_t i = 0, nb_devs, dev_id;
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

	ts_params->large_mbuf_pool = rte_mempool_lookup(
			"CRYPTO_LARGE_MBUFPOOL");
	if (ts_params->large_mbuf_pool == NULL) {
		/* Not already created so create */
		ts_params->large_mbuf_pool = rte_pktmbuf_pool_create(
				"CRYPTO_LARGE_MBUFPOOL",
				1, 0, 0, UINT16_MAX,
				rte_socket_id());
		if (ts_params->large_mbuf_pool == NULL) {
			RTE_LOG(ERR, USER1,
				"Can't create CRYPTO_LARGE_MBUFPOOL\n");
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

	/* Create an AESNI MB device if required */
	if (gbl_cryptodev_type == RTE_CRYPTODEV_AESNI_MB_PMD) {
#ifndef RTE_LIBRTE_PMD_AESNI_MB
		RTE_LOG(ERR, USER1, "CONFIG_RTE_LIBRTE_PMD_AESNI_MB must be"
			" enabled in config file to run this testsuite.\n");
		return TEST_FAILED;
#endif
		nb_devs = rte_cryptodev_count_devtype(
				RTE_CRYPTODEV_AESNI_MB_PMD);
		if (nb_devs < 1) {
			ret = rte_vdev_init(
				RTE_STR(CRYPTODEV_NAME_AESNI_MB_PMD), NULL);

			TEST_ASSERT(ret == 0,
				"Failed to create instance of"
				" pmd : %s",
				RTE_STR(CRYPTODEV_NAME_AESNI_MB_PMD));
		}
	}

	/* Create an AESNI GCM device if required */
	if (gbl_cryptodev_type == RTE_CRYPTODEV_AESNI_GCM_PMD) {
#ifndef RTE_LIBRTE_PMD_AESNI_GCM
		RTE_LOG(ERR, USER1, "CONFIG_RTE_LIBRTE_PMD_AESNI_GCM must be"
			" enabled in config file to run this testsuite.\n");
		return TEST_FAILED;
#endif
		nb_devs = rte_cryptodev_count_devtype(
				RTE_CRYPTODEV_AESNI_GCM_PMD);
		if (nb_devs < 1) {
			TEST_ASSERT_SUCCESS(rte_vdev_init(
				RTE_STR(CRYPTODEV_NAME_AESNI_GCM_PMD), NULL),
				"Failed to create instance of"
				" pmd : %s",
				RTE_STR(CRYPTODEV_NAME_AESNI_GCM_PMD));
		}
	}

	/* Create a SNOW 3G device if required */
	if (gbl_cryptodev_type == RTE_CRYPTODEV_SNOW3G_PMD) {
#ifndef RTE_LIBRTE_PMD_SNOW3G
		RTE_LOG(ERR, USER1, "CONFIG_RTE_LIBRTE_PMD_SNOW3G must be"
			" enabled in config file to run this testsuite.\n");
		return TEST_FAILED;
#endif
		nb_devs = rte_cryptodev_count_devtype(RTE_CRYPTODEV_SNOW3G_PMD);
		if (nb_devs < 1) {
			TEST_ASSERT_SUCCESS(rte_vdev_init(
				RTE_STR(CRYPTODEV_NAME_SNOW3G_PMD), NULL),
				"Failed to create instance of"
				" pmd : %s",
				RTE_STR(CRYPTODEV_NAME_SNOW3G_PMD));
		}
	}

	/* Create a KASUMI device if required */
	if (gbl_cryptodev_type == RTE_CRYPTODEV_KASUMI_PMD) {
#ifndef RTE_LIBRTE_PMD_KASUMI
		RTE_LOG(ERR, USER1, "CONFIG_RTE_LIBRTE_PMD_KASUMI must be"
			" enabled in config file to run this testsuite.\n");
		return TEST_FAILED;
#endif
		nb_devs = rte_cryptodev_count_devtype(RTE_CRYPTODEV_KASUMI_PMD);
		if (nb_devs < 1) {
			TEST_ASSERT_SUCCESS(rte_vdev_init(
				RTE_STR(CRYPTODEV_NAME_KASUMI_PMD), NULL),
				"Failed to create instance of"
				" pmd : %s",
				RTE_STR(CRYPTODEV_NAME_KASUMI_PMD));
		}
	}

	/* Create a ZUC device if required */
	if (gbl_cryptodev_type == RTE_CRYPTODEV_ZUC_PMD) {
#ifndef RTE_LIBRTE_PMD_ZUC
		RTE_LOG(ERR, USER1, "CONFIG_RTE_LIBRTE_PMD_ZUC must be"
			" enabled in config file to run this testsuite.\n");
		return TEST_FAILED;
#endif
		nb_devs = rte_cryptodev_count_devtype(RTE_CRYPTODEV_ZUC_PMD);
		if (nb_devs < 1) {
			TEST_ASSERT_SUCCESS(rte_vdev_init(
				RTE_STR(CRYPTODEV_NAME_ZUC_PMD), NULL),
				"Failed to create instance of"
				" pmd : %s",
				RTE_STR(CRYPTODEV_NAME_ZUC_PMD));
		}
	}

	/* Create a NULL device if required */
	if (gbl_cryptodev_type == RTE_CRYPTODEV_NULL_PMD) {
#ifndef RTE_LIBRTE_PMD_NULL_CRYPTO
		RTE_LOG(ERR, USER1, "CONFIG_RTE_LIBRTE_PMD_NULL_CRYPTO must be"
			" enabled in config file to run this testsuite.\n");
		return TEST_FAILED;
#endif
		nb_devs = rte_cryptodev_count_devtype(
				RTE_CRYPTODEV_NULL_PMD);
		if (nb_devs < 1) {
			ret = rte_vdev_init(
				RTE_STR(CRYPTODEV_NAME_NULL_PMD), NULL);

			TEST_ASSERT(ret == 0,
				"Failed to create instance of"
				" pmd : %s",
				RTE_STR(CRYPTODEV_NAME_NULL_PMD));
		}
	}

	/* Create an OPENSSL device if required */
	if (gbl_cryptodev_type == RTE_CRYPTODEV_OPENSSL_PMD) {
#ifndef RTE_LIBRTE_PMD_OPENSSL
		RTE_LOG(ERR, USER1, "CONFIG_RTE_LIBRTE_PMD_OPENSSL must be"
			" enabled in config file to run this testsuite.\n");
		return TEST_FAILED;
#endif
		nb_devs = rte_cryptodev_count_devtype(
				RTE_CRYPTODEV_OPENSSL_PMD);
		if (nb_devs < 1) {
			ret = rte_vdev_init(
				RTE_STR(CRYPTODEV_NAME_OPENSSL_PMD),
				NULL);

			TEST_ASSERT(ret == 0, "Failed to create "
				"instance of pmd : %s",
				RTE_STR(CRYPTODEV_NAME_OPENSSL_PMD));
		}
	}

	/* Create a ARMv8 device if required */
	if (gbl_cryptodev_type == RTE_CRYPTODEV_ARMV8_PMD) {
#ifndef RTE_LIBRTE_PMD_ARMV8_CRYPTO
		RTE_LOG(ERR, USER1, "CONFIG_RTE_LIBRTE_PMD_ARMV8_CRYPTO must be"
			" enabled in config file to run this testsuite.\n");
		return TEST_FAILED;
#endif
		nb_devs = rte_cryptodev_count_devtype(
				RTE_CRYPTODEV_ARMV8_PMD);
		if (nb_devs < 1) {
			ret = rte_vdev_init(
				RTE_STR(CRYPTODEV_NAME_ARMV8_PMD),
				NULL);

			TEST_ASSERT(ret == 0, "Failed to create "
				"instance of pmd : %s",
				RTE_STR(CRYPTODEV_NAME_ARMV8_PMD));
		}
	}

#ifdef RTE_LIBRTE_PMD_CRYPTO_SCHEDULER
	if (gbl_cryptodev_type == RTE_CRYPTODEV_SCHEDULER_PMD) {

#ifndef RTE_LIBRTE_PMD_AESNI_MB
		RTE_LOG(ERR, USER1, "CONFIG_RTE_LIBRTE_PMD_AESNI_MB must be"
			" enabled in config file to run this testsuite.\n");
		return TEST_FAILED;
#endif
		nb_devs = rte_cryptodev_count_devtype(
				RTE_CRYPTODEV_SCHEDULER_PMD);
		if (nb_devs < 1) {
			ret = rte_vdev_init(
				RTE_STR(CRYPTODEV_NAME_SCHEDULER_PMD),
				NULL);

			TEST_ASSERT(ret == 0,
				"Failed to create instance %u of"
				" pmd : %s",
				i, RTE_STR(CRYPTODEV_NAME_SCHEDULER_PMD));
		}
	}
#endif /* RTE_LIBRTE_PMD_CRYPTO_SCHEDULER */

#ifndef RTE_LIBRTE_PMD_QAT
	if (gbl_cryptodev_type == RTE_CRYPTODEV_QAT_SYM_PMD) {
		RTE_LOG(ERR, USER1, "CONFIG_RTE_LIBRTE_PMD_QAT must be enabled "
				"in config file to run this testsuite.\n");
		return TEST_FAILED;
	}
#endif

	nb_devs = rte_cryptodev_count();
	if (nb_devs < 1) {
		RTE_LOG(ERR, USER1, "No crypto devices found?\n");
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

	dev_id = ts_params->valid_devs[0];

	rte_cryptodev_info_get(dev_id, &info);

	ts_params->conf.nb_queue_pairs = info.max_nb_queue_pairs;
	ts_params->conf.socket_id = SOCKET_ID_ANY;
	ts_params->conf.session_mp.nb_objs = info.sym.max_nb_sessions;

	TEST_ASSERT_SUCCESS(rte_cryptodev_configure(dev_id,
			&ts_params->conf),
			"Failed to configure cryptodev %u with %u qps",
			dev_id, ts_params->conf.nb_queue_pairs);

	ts_params->qp_conf.nb_descriptors = DEFAULT_NUM_OPS_INFLIGHT;

	for (qp_id = 0; qp_id < info.max_nb_queue_pairs; qp_id++) {
		TEST_ASSERT_SUCCESS(rte_cryptodev_queue_pair_setup(
			dev_id, qp_id, &ts_params->qp_conf,
			rte_cryptodev_socket_id(dev_id)),
			"Failed to setup queue pair %u on cryptodev %u",
			qp_id, dev_id);
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
	ts_params->conf.socket_id = SOCKET_ID_ANY;
	ts_params->conf.session_mp.nb_objs = DEFAULT_NUM_OPS_INFLIGHT;

	TEST_ASSERT_SUCCESS(rte_cryptodev_configure(ts_params->valid_devs[0],
			&ts_params->conf),
			"Failed to configure cryptodev %u",
			ts_params->valid_devs[0]);

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
	uint16_t orig_nb_qps = ts_params->conf.nb_queue_pairs;

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

	/* revert to original testsuite value */
	ts_params->conf.nb_queue_pairs = orig_nb_qps;

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


/* Multisession Vector context Test */
/*Begin Session 0 */
static uint8_t ms_aes_cbc_key0[] = {
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

static uint8_t ms_aes_cbc_iv0[] = {
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

static const uint8_t ms_aes_cbc_cipher0[] = {
		0x3C, 0xE4, 0xEE, 0x42, 0xB6, 0x9B, 0xC3, 0x38,
		0x5F, 0xAD, 0x54, 0xDC, 0xA8, 0x32, 0x81, 0xDC,
		0x7A, 0x6F, 0x85, 0x58, 0x07, 0x35, 0xED, 0xEB,
		0xAD, 0x79, 0x79, 0x96, 0xD3, 0x0E, 0xA6, 0xD9,
		0xAA, 0x86, 0xA4, 0x8F, 0xB5, 0xD6, 0x6E, 0x6D,
		0x0C, 0x91, 0x2F, 0xC4, 0x67, 0x98, 0x0E, 0xC4,
		0x8D, 0x83, 0x68, 0x69, 0xC4, 0xD3, 0x94, 0x34,
		0xC4, 0x5D, 0x60, 0x55, 0x22, 0x87, 0x8F, 0x6F,
		0x17, 0x8E, 0x75, 0xE4, 0x02, 0xF5, 0x1B, 0x99,
		0xC8, 0x39, 0xA9, 0xAB, 0x23, 0x91, 0x12, 0xED,
		0x08, 0xE7, 0xD9, 0x25, 0x89, 0x24, 0x4F, 0x8D,
		0x68, 0xF3, 0x10, 0x39, 0x0A, 0xEE, 0x45, 0x24,
		0xDF, 0x7A, 0x9D, 0x00, 0x25, 0xE5, 0x35, 0x71,
		0x4E, 0x40, 0x59, 0x6F, 0x0A, 0x13, 0xB3, 0x72,
		0x1D, 0x98, 0x63, 0x94, 0x89, 0xA5, 0x39, 0x8E,
		0xD3, 0x9C, 0x8A, 0x7F, 0x71, 0x2F, 0xC7, 0xCD,
		0x81, 0x05, 0xDC, 0xC0, 0x8D, 0xCE, 0x6D, 0x18,
		0x30, 0xC4, 0x72, 0x51, 0xF0, 0x27, 0xC8, 0xF6,
		0x60, 0x5B, 0x7C, 0xB2, 0xE3, 0x49, 0x0C, 0x29,
		0xC6, 0x9F, 0x39, 0x57, 0x80, 0x55, 0x24, 0x2C,
		0x9B, 0x0F, 0x5A, 0xB3, 0x89, 0x55, 0x31, 0x96,
		0x0D, 0xCD, 0xF6, 0x51, 0x03, 0x2D, 0x89, 0x26,
		0x74, 0x44, 0xD6, 0xE8, 0xDC, 0xEA, 0x44, 0x55,
		0x64, 0x71, 0x9C, 0x9F, 0x5D, 0xBA, 0x39, 0x46,
		0xA8, 0x17, 0xA1, 0x9C, 0x52, 0x9D, 0xBC, 0x6B,
		0x4A, 0x98, 0xE6, 0xEA, 0x33, 0xEC, 0x58, 0xB4,
		0x43, 0xF0, 0x32, 0x45, 0xA4, 0xC1, 0x55, 0xB7,
		0x5D, 0xB5, 0x59, 0xB2, 0xE3, 0x96, 0xFF, 0xA5,
		0xAF, 0xE1, 0x86, 0x1B, 0x42, 0xE6, 0x3B, 0xA0,
		0x90, 0x4A, 0xE8, 0x8C, 0x21, 0x7F, 0x36, 0x1E,
		0x5B, 0x65, 0x25, 0xD1, 0xC1, 0x5A, 0xCA, 0x3D,
		0x10, 0xED, 0x2D, 0x79, 0xD0, 0x0F, 0x58, 0x44,
		0x69, 0x81, 0xF5, 0xD4, 0xC9, 0x0F, 0x90, 0x76,
		0x1F, 0x54, 0xD2, 0xD5, 0x97, 0xCE, 0x2C, 0xE3,
		0xEF, 0xF4, 0xB7, 0xC6, 0x3A, 0x87, 0x7F, 0x83,
		0x2A, 0xAF, 0xCD, 0x90, 0x12, 0xA7, 0x7D, 0x85,
		0x1D, 0x62, 0xD3, 0x85, 0x25, 0x05, 0xDB, 0x45,
		0x92, 0xA3, 0xF6, 0xA2, 0xA8, 0x41, 0xE4, 0x25,
		0x86, 0x87, 0x67, 0x24, 0xEC, 0x89, 0x23, 0x2A,
		0x9B, 0x20, 0x4D, 0x93, 0xEE, 0xE2, 0x2E, 0xC1,
		0x0B, 0x15, 0x33, 0xCF, 0x00, 0xD1, 0x1A, 0xDA,
		0x93, 0xFD, 0x28, 0x21, 0x5B, 0xCF, 0xD1, 0xF3,
		0x5A, 0x81, 0xBA, 0x82, 0x5E, 0x2F, 0x61, 0xB4,
		0x05, 0x71, 0xB5, 0xF4, 0x39, 0x3C, 0x1F, 0x60,
		0x00, 0x7A, 0xC4, 0xF8, 0x35, 0x20, 0x6C, 0x3A,
		0xCC, 0x03, 0x8F, 0x7B, 0xA2, 0xB6, 0x65, 0x8A,
		0xB6, 0x5F, 0xFD, 0x25, 0xD3, 0x5F, 0x92, 0xF9,
		0xAE, 0x17, 0x9B, 0x5E, 0x6E, 0x9A, 0xE4, 0x55,
		0x10, 0x25, 0x07, 0xA4, 0xAF, 0x21, 0x69, 0x13,
		0xD8, 0xFA, 0x31, 0xED, 0xF7, 0xA7, 0xA7, 0x3B,
		0xB8, 0x96, 0x8E, 0x10, 0x86, 0x74, 0xD8, 0xB1,
		0x34, 0x9E, 0x9B, 0x6A, 0x26, 0xA8, 0xD4, 0xD0,
		0xB5, 0xF6, 0xDE, 0xE7, 0xCA, 0x06, 0xDC, 0xA3,
		0x6F, 0xEE, 0x6B, 0x1E, 0xB5, 0x30, 0x99, 0x23,
		0xF9, 0x76, 0xF0, 0xA0, 0xCF, 0x3B, 0x94, 0x7B,
		0x19, 0x8D, 0xA5, 0x0C, 0x18, 0xA6, 0x1D, 0x07,
		0x89, 0xBE, 0x5B, 0x61, 0xE5, 0xF1, 0x42, 0xDB,
		0xD4, 0x2E, 0x02, 0x1F, 0xCE, 0xEF, 0x92, 0xB1,
		0x1B, 0x56, 0x50, 0xF2, 0x16, 0xE5, 0xE7, 0x4F,
		0xFD, 0xBB, 0x3E, 0xD2, 0xFC, 0x3C, 0xC6, 0x0F,
		0xF9, 0x12, 0x4E, 0xCB, 0x1E, 0x0C, 0x15, 0x84,
		0x2A, 0x14, 0x8A, 0x02, 0xE4, 0x7E, 0x95, 0x5B,
		0x86, 0xDB, 0x9B, 0x62, 0x5B, 0x19, 0xD2, 0x17,
		0xFA, 0x13, 0xBB, 0x6B, 0x3F, 0x45, 0x9F, 0xBF
};


static  uint8_t ms_hmac_key0[] = {
		0xFF, 0x1A, 0x7D, 0x3D, 0xF5, 0x82, 0x80, 0xF1,
		0xF1, 0x35, 0x5C, 0x3B, 0xDD, 0x9A, 0x65, 0xBA,
		0x58, 0x34, 0x85, 0x65, 0x1C, 0x42, 0x50, 0x76,
		0x9A, 0xAF, 0x88, 0x1B, 0xB6, 0x8F, 0xF8, 0x60,
		0xA2, 0x5A, 0x7F, 0x3F, 0xF4, 0x72, 0x70, 0xF1,
		0xF5, 0x35, 0x4C, 0x3B, 0xDD, 0x90, 0x65, 0xB0,
		0x47, 0x3A, 0x75, 0x61, 0x5C, 0xA2, 0x10, 0x76,
		0x9A, 0xAF, 0x77, 0x5B, 0xB6, 0x7F, 0xF7, 0x60
};

static const uint8_t ms_hmac_digest0[] = {
		0x43, 0x52, 0xED, 0x34, 0xAB, 0x36, 0xB2, 0x51,
		0xFB, 0xA3, 0xA6, 0x7C, 0x38, 0xFC, 0x42, 0x8F,
		0x57, 0x64, 0xAB, 0x81, 0xA7, 0x89, 0xB7, 0x6C,
		0xA0, 0xDC, 0xB9, 0x4D, 0xC4, 0x30, 0xF9, 0xD4,
		0x10, 0x82, 0x55, 0xD0, 0xAB, 0x32, 0xFB, 0x56,
		0x0D, 0xE4, 0x68, 0x3D, 0x76, 0xD0, 0x7B, 0xE4,
		0xA6, 0x2C, 0x34, 0x9E, 0x8C, 0x41, 0xF8, 0x23,
		0x28, 0x1B, 0x3A, 0x90, 0x26, 0x34, 0x47, 0x90
		};

/* End Session 0 */
/* Begin session 1 */

static  uint8_t ms_aes_cbc_key1[] = {
		0xf1, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
		0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

static  uint8_t ms_aes_cbc_iv1[] = {
	0xf1, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

static const uint8_t ms_aes_cbc_cipher1[] = {
		0x5A, 0x7A, 0x67, 0x5D, 0xB8, 0xE1, 0xDC, 0x71,
		0x39, 0xA8, 0x74, 0x93, 0x9C, 0x4C, 0xFE, 0x23,
		0x61, 0xCD, 0xA4, 0xB3, 0xD9, 0xCE, 0x99, 0x09,
		0x2A, 0x23, 0xF3, 0x29, 0xBF, 0x4C, 0xB4, 0x6A,
		0x1B, 0x6B, 0x73, 0x4D, 0x48, 0x0C, 0xCF, 0x6C,
		0x5E, 0x34, 0x9E, 0x7F, 0xBC, 0x8F, 0xCC, 0x8F,
		0x75, 0x1D, 0x3D, 0x77, 0x10, 0x76, 0xC8, 0xB9,
		0x99, 0x6F, 0xD6, 0x56, 0x75, 0xA9, 0xB2, 0x66,
		0xC2, 0x24, 0x2B, 0x9C, 0xFE, 0x40, 0x8E, 0x43,
		0x20, 0x97, 0x1B, 0xFA, 0xD0, 0xCF, 0x04, 0xAB,
		0xBB, 0xF6, 0x5D, 0xF5, 0xA0, 0x19, 0x7C, 0x23,
		0x5D, 0x80, 0x8C, 0x49, 0xF6, 0x76, 0x88, 0x29,
		0x27, 0x4C, 0x59, 0x2B, 0x43, 0xA6, 0xB2, 0x26,
		0x27, 0x78, 0xBE, 0x1B, 0xE1, 0x4F, 0x5A, 0x1F,
		0xFC, 0x68, 0x08, 0xE7, 0xC4, 0xD1, 0x34, 0x68,
		0xB7, 0x13, 0x14, 0x41, 0x62, 0x6B, 0x1F, 0x77,
		0x0C, 0x68, 0x1D, 0x0D, 0xED, 0x89, 0xAA, 0xD8,
		0x97, 0x02, 0xBA, 0x5E, 0xD4, 0x84, 0x25, 0x97,
		0x03, 0xA5, 0xA6, 0x13, 0x66, 0x02, 0xF4, 0xC3,
		0xF3, 0xD3, 0xCC, 0x95, 0xC3, 0x87, 0x46, 0x90,
		0x1F, 0x6E, 0x14, 0xA8, 0x00, 0xF2, 0x6F, 0xD5,
		0xA1, 0xAD, 0xD5, 0x40, 0xA2, 0x0F, 0x32, 0x7E,
		0x99, 0xA3, 0xF5, 0x53, 0xC3, 0x26, 0xA1, 0x45,
		0x01, 0x88, 0x57, 0x84, 0x3E, 0x7B, 0x4E, 0x0B,
		0x3C, 0xB5, 0x3E, 0x9E, 0xE9, 0x78, 0x77, 0xC5,
		0xC0, 0x89, 0xA8, 0xF8, 0xF1, 0xA5, 0x2D, 0x5D,
		0xF9, 0xC6, 0xFB, 0xCB, 0x05, 0x23, 0xBD, 0x6E,
		0x5E, 0x14, 0xC6, 0x57, 0x73, 0xCF, 0x98, 0xBD,
		0x10, 0x8B, 0x18, 0xA6, 0x01, 0x5B, 0x13, 0xAE,
		0x8E, 0xDE, 0x1F, 0xB5, 0xB7, 0x40, 0x6C, 0xC1,
		0x1E, 0xA1, 0x19, 0x20, 0x9E, 0x95, 0xE0, 0x2F,
		0x1C, 0xF5, 0xD9, 0xD0, 0x2B, 0x1E, 0x82, 0x25,
		0x62, 0xB4, 0xEB, 0xA1, 0x1F, 0xCE, 0x44, 0xA1,
		0xCB, 0x92, 0x01, 0x6B, 0xE4, 0x26, 0x23, 0xE3,
		0xC5, 0x67, 0x35, 0x55, 0xDA, 0xE5, 0x27, 0xEE,
		0x8D, 0x12, 0x84, 0xB7, 0xBA, 0xA7, 0x1C, 0xD6,
		0x32, 0x3F, 0x67, 0xED, 0xFB, 0x5B, 0x8B, 0x52,
		0x46, 0x8C, 0xF9, 0x69, 0xCD, 0xAE, 0x79, 0xAA,
		0x37, 0x78, 0x49, 0xEB, 0xC6, 0x8E, 0x76, 0x63,
		0x84, 0xFF, 0x9D, 0x22, 0x99, 0x51, 0xB7, 0x5E,
		0x83, 0x4C, 0x8B, 0xDF, 0x5A, 0x07, 0xCC, 0xBA,
		0x42, 0xA5, 0x98, 0xB6, 0x47, 0x0E, 0x66, 0xEB,
		0x23, 0x0E, 0xBA, 0x44, 0xA8, 0xAA, 0x20, 0x71,
		0x79, 0x9C, 0x77, 0x5F, 0xF5, 0xFE, 0xEC, 0xEF,
		0xC6, 0x64, 0x3D, 0x84, 0xD0, 0x2B, 0xA7, 0x0A,
		0xC3, 0x72, 0x5B, 0x9C, 0xFA, 0xA8, 0x87, 0x95,
		0x94, 0x11, 0x38, 0xA7, 0x1E, 0x58, 0xE3, 0x73,
		0xC6, 0xC9, 0xD1, 0x7B, 0x92, 0xDB, 0x0F, 0x49,
		0x74, 0xC2, 0xA2, 0x0E, 0x35, 0x57, 0xAC, 0xDB,
		0x9A, 0x1C, 0xCF, 0x5A, 0x32, 0x3E, 0x26, 0x9B,
		0xEC, 0xB3, 0xEF, 0x9C, 0xFE, 0xBE, 0x52, 0xAC,
		0xB1, 0x29, 0xDD, 0xFD, 0x07, 0xE2, 0xEE, 0xED,
		0xE4, 0x46, 0x37, 0xFE, 0xD1, 0xDC, 0xCD, 0x02,
		0xF9, 0x31, 0xB0, 0xFB, 0x36, 0xB7, 0x34, 0xA4,
		0x76, 0xE8, 0x57, 0xBF, 0x99, 0x92, 0xC7, 0xAF,
		0x98, 0x10, 0xE2, 0x70, 0xCA, 0xC9, 0x2B, 0x82,
		0x06, 0x96, 0x88, 0x0D, 0xB3, 0xAC, 0x9E, 0x6D,
		0x43, 0xBC, 0x5B, 0x31, 0xCF, 0x65, 0x8D, 0xA6,
		0xC7, 0xFE, 0x73, 0xE1, 0x54, 0xF7, 0x10, 0xF9,
		0x86, 0xF7, 0xDF, 0xA1, 0xA1, 0xD8, 0xAE, 0x35,
		0xB3, 0x90, 0xDC, 0x6F, 0x43, 0x7A, 0x8B, 0xE0,
		0xFE, 0x8F, 0x33, 0x4D, 0x29, 0x6C, 0x45, 0x53,
		0x73, 0xDD, 0x21, 0x0B, 0x85, 0x30, 0xB5, 0xA5,
		0xF3, 0x5D, 0xEC, 0x79, 0x61, 0x9D, 0x9E, 0xB3

};

static uint8_t ms_hmac_key1[] = {
		0xFE, 0x1A, 0x7D, 0x3D, 0xF5, 0x82, 0x80, 0xF1,
		0xF1, 0x35, 0x5C, 0x3B, 0xDD, 0x9A, 0x65, 0xBA,
		0x58, 0x34, 0x85, 0x65, 0x1C, 0x42, 0x50, 0x76,
		0x9A, 0xAF, 0x88, 0x1B, 0xB6, 0x8F, 0xF8, 0x60,
		0xA2, 0x5A, 0x7F, 0x3F, 0xF4, 0x72, 0x70, 0xF1,
		0xF5, 0x35, 0x4C, 0x3B, 0xDD, 0x90, 0x65, 0xB0,
		0x47, 0x3A, 0x75, 0x61, 0x5C, 0xA2, 0x10, 0x76,
		0x9A, 0xAF, 0x77, 0x5B, 0xB6, 0x7F, 0xF7, 0x60
};

static const uint8_t ms_hmac_digest1[] = {
		0xCE, 0x6E, 0x5F, 0x77, 0x96, 0x9A, 0xB1, 0x69,
		0x2D, 0x5E, 0xF3, 0x2F, 0x32, 0x10, 0xCB, 0x50,
		0x0E, 0x09, 0x56, 0x25, 0x07, 0x34, 0xC9, 0x20,
		0xEC, 0x13, 0x43, 0x23, 0x5C, 0x08, 0x8B, 0xCD,
		0xDC, 0x86, 0x8C, 0xEE, 0x0A, 0x95, 0x2E, 0xB9,
		0x8C, 0x7B, 0x02, 0x7A, 0xD4, 0xE1, 0x49, 0xB4,
		0x45, 0xB5, 0x52, 0x37, 0xC6, 0xFF, 0xFE, 0xAA,
		0x0A, 0x87, 0xB8, 0x51, 0xF9, 0x2A, 0x01, 0x8F
};
/* End Session 1  */
/* Begin Session 2 */
static  uint8_t ms_aes_cbc_key2[] = {
		0xff, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
		0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

static  uint8_t ms_aes_cbc_iv2[] = {
		0xff, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
		0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

static const uint8_t ms_aes_cbc_cipher2[] = {
		0xBB, 0x3C, 0x68, 0x25, 0xFD, 0xB6, 0xA2, 0x91,
		0x20, 0x56, 0xF6, 0x30, 0x35, 0xFC, 0x9E, 0x97,
		0xF2, 0x90, 0xFC, 0x7E, 0x3E, 0x0A, 0x75, 0xC8,
		0x4C, 0xF2, 0x2D, 0xAC, 0xD3, 0x93, 0xF0, 0xC5,
		0x14, 0x88, 0x8A, 0x23, 0xC2, 0x59, 0x9A, 0x98,
		0x4B, 0xD5, 0x2C, 0xDA, 0x43, 0xA9, 0x34, 0x69,
		0x7C, 0x6D, 0xDB, 0xDC, 0xCB, 0xC0, 0xA0, 0x09,
		0xA7, 0x86, 0x16, 0x4B, 0xBF, 0xA8, 0xB6, 0xCF,
		0x7F, 0x74, 0x1F, 0x22, 0xF0, 0xF6, 0xBB, 0x44,
		0x8B, 0x4C, 0x9E, 0x23, 0xF8, 0x9F, 0xFC, 0x5B,
		0x9E, 0x9C, 0x2A, 0x79, 0x30, 0x8F, 0xBF, 0xA9,
		0x68, 0xA1, 0x20, 0x71, 0x7C, 0x77, 0x22, 0x34,
		0x07, 0xCD, 0xC6, 0xF6, 0x50, 0x0A, 0x08, 0x99,
		0x17, 0x98, 0xE3, 0x93, 0x8A, 0xB0, 0xEE, 0xDF,
		0xC2, 0xBA, 0x3B, 0x44, 0x73, 0xDF, 0xDD, 0xDC,
		0x14, 0x4D, 0x3B, 0xBB, 0x5E, 0x58, 0xC1, 0x26,
		0xA7, 0xAE, 0x47, 0xF3, 0x24, 0x6D, 0x4F, 0xD3,
		0x6E, 0x3E, 0x33, 0xE6, 0x7F, 0xCA, 0x50, 0xAF,
		0x5D, 0x3D, 0xA0, 0xDD, 0xC9, 0xF3, 0x30, 0xD3,
		0x6E, 0x8B, 0x2E, 0x12, 0x24, 0x34, 0xF0, 0xD3,
		0xC7, 0x8D, 0x23, 0x29, 0xAA, 0x05, 0xE1, 0xFA,
		0x2E, 0xF6, 0x8D, 0x37, 0x86, 0xC0, 0x6D, 0x13,
		0x2D, 0x98, 0xF3, 0x52, 0x39, 0x22, 0xCE, 0x38,
		0xC2, 0x1A, 0x72, 0xED, 0xFB, 0xCC, 0xE4, 0x71,
		0x5A, 0x0C, 0x0D, 0x09, 0xF8, 0xE8, 0x1B, 0xBC,
		0x53, 0xC8, 0xD8, 0x8F, 0xE5, 0x98, 0x5A, 0xB1,
		0x06, 0xA6, 0x5B, 0xE6, 0xA2, 0x88, 0x21, 0x9E,
		0x36, 0xC0, 0x34, 0xF9, 0xFB, 0x3B, 0x0A, 0x22,
		0x00, 0x00, 0x39, 0x48, 0x8D, 0x23, 0x74, 0x62,
		0x72, 0x91, 0xE6, 0x36, 0xAA, 0x77, 0x9C, 0x72,
		0x9D, 0xA8, 0xC3, 0xA9, 0xD5, 0x44, 0x72, 0xA6,
		0xB9, 0x28, 0x8F, 0x64, 0x4C, 0x8A, 0x64, 0xE6,
		0x4E, 0xFA, 0xEF, 0x87, 0xDE, 0x7B, 0x22, 0x44,
		0xB0, 0xDF, 0x2E, 0x5F, 0x0B, 0xA5, 0xF2, 0x24,
		0x07, 0x5C, 0x2D, 0x39, 0xB7, 0x3D, 0x8A, 0xE5,
		0x0E, 0x9D, 0x4E, 0x50, 0xED, 0x03, 0x99, 0x8E,
		0xF0, 0x06, 0x55, 0x4E, 0xA2, 0x24, 0xE7, 0x17,
		0x46, 0xDF, 0x6C, 0xCD, 0xC6, 0x44, 0xE8, 0xF9,
		0xB9, 0x1B, 0x36, 0xF6, 0x7F, 0x10, 0xA4, 0x7D,
		0x90, 0xBD, 0xE4, 0xAA, 0xD6, 0x9E, 0x18, 0x9D,
		0x22, 0x35, 0xD6, 0x55, 0x54, 0xAA, 0xF7, 0x22,
		0xA3, 0x3E, 0xEF, 0xC8, 0xA2, 0x34, 0x8D, 0xA9,
		0x37, 0x63, 0xA6, 0xC3, 0x57, 0xCB, 0x0C, 0x49,
		0x7D, 0x02, 0xBE, 0xAA, 0x13, 0x75, 0xB7, 0x4E,
		0x52, 0x62, 0xA5, 0xC2, 0x33, 0xC7, 0x6C, 0x1B,
		0xF6, 0x34, 0xF6, 0x09, 0xA5, 0x0C, 0xC7, 0xA2,
		0x61, 0x48, 0x62, 0x7D, 0x17, 0x15, 0xE3, 0x95,
		0xC8, 0x63, 0xD2, 0xA4, 0x43, 0xA9, 0x49, 0x07,
		0xB2, 0x3B, 0x2B, 0x62, 0x7D, 0xCB, 0x51, 0xB3,
		0x25, 0x33, 0x47, 0x0E, 0x14, 0x67, 0xDC, 0x6A,
		0x9B, 0x51, 0xAC, 0x9D, 0x8F, 0xA2, 0x2B, 0x57,
		0x8C, 0x5C, 0x5F, 0x76, 0x23, 0x92, 0x0F, 0x84,
		0x46, 0x0E, 0x40, 0x85, 0x38, 0x60, 0xFA, 0x61,
		0x20, 0xC5, 0xE3, 0xF1, 0x70, 0xAC, 0x1B, 0xBF,
		0xC4, 0x2B, 0xC5, 0x67, 0xD1, 0x43, 0xC5, 0x17,
		0x74, 0x71, 0x69, 0x6F, 0x82, 0x89, 0x19, 0x8A,
		0x70, 0x43, 0x92, 0x01, 0xC4, 0x63, 0x7E, 0xB1,
		0x59, 0x4E, 0xCD, 0xEA, 0x93, 0xA4, 0x52, 0x53,
		0x9B, 0x61, 0x5B, 0xD2, 0x3E, 0x19, 0x39, 0xB7,
		0x32, 0xEA, 0x8E, 0xF8, 0x1D, 0x76, 0x5C, 0xB2,
		0x73, 0x2D, 0x91, 0xC0, 0x18, 0xED, 0x25, 0x2A,
		0x53, 0x64, 0xF0, 0x92, 0x31, 0x55, 0x21, 0xA8,
		0x24, 0xA9, 0xD1, 0x02, 0xF6, 0x6C, 0x2B, 0x70,
		0xA9, 0x59, 0xC1, 0xD6, 0xC3, 0x57, 0x5B, 0x92
};

static  uint8_t ms_hmac_key2[] = {
		0xFC, 0x1A, 0x7D, 0x3D, 0xF5, 0x82, 0x80, 0xF1,
		0xF1, 0x35, 0x5C, 0x3B, 0xDD, 0x9A, 0x65, 0xBA,
		0x58, 0x34, 0x85, 0x65, 0x1C, 0x42, 0x50, 0x76,
		0x9A, 0xAF, 0x88, 0x1B, 0xB6, 0x8F, 0xF8, 0x60,
		0xA2, 0x5A, 0x7F, 0x3F, 0xF4, 0x72, 0x70, 0xF1,
		0xF5, 0x35, 0x4C, 0x3B, 0xDD, 0x90, 0x65, 0xB0,
		0x47, 0x3A, 0x75, 0x61, 0x5C, 0xA2, 0x10, 0x76,
		0x9A, 0xAF, 0x77, 0x5B, 0xB6, 0x7F, 0xF7, 0x60
};

static const uint8_t ms_hmac_digest2[] = {
		0xA5, 0x0F, 0x9C, 0xFB, 0x08, 0x62, 0x59, 0xFF,
		0x80, 0x2F, 0xEB, 0x4B, 0xE1, 0x46, 0x21, 0xD6,
		0x02, 0x98, 0xF2, 0x8E, 0xF4, 0xEC, 0xD4, 0x77,
		0x86, 0x4C, 0x31, 0x28, 0xC8, 0x25, 0x80, 0x27,
		0x3A, 0x72, 0x5D, 0x6A, 0x56, 0x8A, 0xD3, 0x82,
		0xB0, 0xEC, 0x31, 0x6D, 0x8B, 0x6B, 0xB4, 0x24,
		0xE7, 0x62, 0xC1, 0x52, 0xBC, 0x14, 0x1B, 0x8E,
		0xEC, 0x9A, 0xF1, 0x47, 0x80, 0xD2, 0xB0, 0x59
};

/* End Session 2 */


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
		struct crypto_unittest_params *ut_params,
		uint8_t *cipher_key,
		uint8_t *hmac_key);

static int
test_AES_CBC_HMAC_SHA512_decrypt_perform(struct rte_cryptodev_sym_session *sess,
		struct crypto_unittest_params *ut_params,
		struct crypto_testsuite_params *ts_params,
		const uint8_t *cipher,
		const uint8_t *digest,
		const uint8_t *iv);


static int
test_AES_CBC_HMAC_SHA512_decrypt_create_session_params(
		struct crypto_unittest_params *ut_params,
		uint8_t *cipher_key,
		uint8_t *hmac_key)
{

	/* Setup Cipher Parameters */
	ut_params->cipher_xform.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	ut_params->cipher_xform.next = NULL;

	ut_params->cipher_xform.cipher.algo = RTE_CRYPTO_CIPHER_AES_CBC;
	ut_params->cipher_xform.cipher.op = RTE_CRYPTO_CIPHER_OP_DECRYPT;
	ut_params->cipher_xform.cipher.key.data = cipher_key;
	ut_params->cipher_xform.cipher.key.length = CIPHER_KEY_LENGTH_AES_CBC;

	/* Setup HMAC Parameters */
	ut_params->auth_xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	ut_params->auth_xform.next = &ut_params->cipher_xform;

	ut_params->auth_xform.auth.op = RTE_CRYPTO_AUTH_OP_VERIFY;
	ut_params->auth_xform.auth.algo = RTE_CRYPTO_AUTH_SHA512_HMAC;
	ut_params->auth_xform.auth.key.data = hmac_key;
	ut_params->auth_xform.auth.key.length = HMAC_KEY_LENGTH_SHA512;
	ut_params->auth_xform.auth.digest_length = DIGEST_BYTE_LENGTH_SHA512;

	return TEST_SUCCESS;
}


static int
test_AES_CBC_HMAC_SHA512_decrypt_perform(struct rte_cryptodev_sym_session *sess,
		struct crypto_unittest_params *ut_params,
		struct crypto_testsuite_params *ts_params,
		const uint8_t *cipher,
		const uint8_t *digest,
		const uint8_t *iv)
{
	/* Generate test mbuf data and digest */
	ut_params->ibuf = setup_test_string(ts_params->mbuf_pool,
			(const char *)
			cipher,
			QUOTE_512_BYTES, 0);

	ut_params->digest = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
			DIGEST_BYTE_LENGTH_SHA512);
	TEST_ASSERT_NOT_NULL(ut_params->digest, "no room to append digest");

	rte_memcpy(ut_params->digest,
			digest,
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

	rte_memcpy(sym_op->cipher.iv.data, iv,
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
test_AES_cipheronly_mb_all(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	int status;

	status = test_blockcipher_all_tests(ts_params->mbuf_pool,
		ts_params->op_mpool, ts_params->valid_devs[0],
		RTE_CRYPTODEV_AESNI_MB_PMD,
		BLKCIPHER_AES_CIPHERONLY_TYPE);

	TEST_ASSERT_EQUAL(status, 0, "Test failed");

	return TEST_SUCCESS;
}

static int
test_AES_docsis_mb_all(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	int status;

	status = test_blockcipher_all_tests(ts_params->mbuf_pool,
		ts_params->op_mpool, ts_params->valid_devs[0],
		RTE_CRYPTODEV_AESNI_MB_PMD,
		BLKCIPHER_AES_DOCSIS_TYPE);

	TEST_ASSERT_EQUAL(status, 0, "Test failed");

	return TEST_SUCCESS;
}

static int
test_AES_docsis_qat_all(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	int status;

	status = test_blockcipher_all_tests(ts_params->mbuf_pool,
		ts_params->op_mpool, ts_params->valid_devs[0],
		RTE_CRYPTODEV_QAT_SYM_PMD,
		BLKCIPHER_AES_DOCSIS_TYPE);

	TEST_ASSERT_EQUAL(status, 0, "Test failed");

	return TEST_SUCCESS;
}

static int
test_DES_docsis_qat_all(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	int status;

	status = test_blockcipher_all_tests(ts_params->mbuf_pool,
		ts_params->op_mpool, ts_params->valid_devs[0],
		RTE_CRYPTODEV_QAT_SYM_PMD,
		BLKCIPHER_DES_DOCSIS_TYPE);

	TEST_ASSERT_EQUAL(status, 0, "Test failed");

	return TEST_SUCCESS;
}

static int
test_authonly_mb_all(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	int status;

	status = test_blockcipher_all_tests(ts_params->mbuf_pool,
		ts_params->op_mpool, ts_params->valid_devs[0],
		RTE_CRYPTODEV_AESNI_MB_PMD,
		BLKCIPHER_AUTHONLY_TYPE);

	TEST_ASSERT_EQUAL(status, 0, "Test failed");

	return TEST_SUCCESS;
}

static int
test_AES_chain_mb_all(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	int status;

	status = test_blockcipher_all_tests(ts_params->mbuf_pool,
		ts_params->op_mpool, ts_params->valid_devs[0],
		RTE_CRYPTODEV_AESNI_MB_PMD,
		BLKCIPHER_AES_CHAIN_TYPE);

	TEST_ASSERT_EQUAL(status, 0, "Test failed");

	return TEST_SUCCESS;
}

#ifdef RTE_LIBRTE_PMD_CRYPTO_SCHEDULER

static int
test_AES_cipheronly_scheduler_all(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	int status;

	status = test_blockcipher_all_tests(ts_params->mbuf_pool,
		ts_params->op_mpool, ts_params->valid_devs[0],
		RTE_CRYPTODEV_SCHEDULER_PMD,
		BLKCIPHER_AES_CIPHERONLY_TYPE);

	TEST_ASSERT_EQUAL(status, 0, "Test failed");

	return TEST_SUCCESS;
}

static int
test_AES_chain_scheduler_all(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	int status;

	status = test_blockcipher_all_tests(ts_params->mbuf_pool,
		ts_params->op_mpool, ts_params->valid_devs[0],
		RTE_CRYPTODEV_SCHEDULER_PMD,
		BLKCIPHER_AES_CHAIN_TYPE);

	TEST_ASSERT_EQUAL(status, 0, "Test failed");

	return TEST_SUCCESS;
}

static int
test_authonly_scheduler_all(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	int status;

	status = test_blockcipher_all_tests(ts_params->mbuf_pool,
		ts_params->op_mpool, ts_params->valid_devs[0],
		RTE_CRYPTODEV_SCHEDULER_PMD,
		BLKCIPHER_AUTHONLY_TYPE);

	TEST_ASSERT_EQUAL(status, 0, "Test failed");

	return TEST_SUCCESS;
}

#endif /* RTE_LIBRTE_PMD_CRYPTO_SCHEDULER */

static int
test_AES_chain_openssl_all(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	int status;

	status = test_blockcipher_all_tests(ts_params->mbuf_pool,
		ts_params->op_mpool, ts_params->valid_devs[0],
		RTE_CRYPTODEV_OPENSSL_PMD,
		BLKCIPHER_AES_CHAIN_TYPE);

	TEST_ASSERT_EQUAL(status, 0, "Test failed");

	return TEST_SUCCESS;
}

static int
test_AES_cipheronly_openssl_all(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	int status;

	status = test_blockcipher_all_tests(ts_params->mbuf_pool,
		ts_params->op_mpool, ts_params->valid_devs[0],
		RTE_CRYPTODEV_OPENSSL_PMD,
		BLKCIPHER_AES_CIPHERONLY_TYPE);

	TEST_ASSERT_EQUAL(status, 0, "Test failed");

	return TEST_SUCCESS;
}

static int
test_AES_chain_qat_all(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	int status;

	status = test_blockcipher_all_tests(ts_params->mbuf_pool,
		ts_params->op_mpool, ts_params->valid_devs[0],
		RTE_CRYPTODEV_QAT_SYM_PMD,
		BLKCIPHER_AES_CHAIN_TYPE);

	TEST_ASSERT_EQUAL(status, 0, "Test failed");

	return TEST_SUCCESS;
}

static int
test_AES_cipheronly_qat_all(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	int status;

	status = test_blockcipher_all_tests(ts_params->mbuf_pool,
		ts_params->op_mpool, ts_params->valid_devs[0],
		RTE_CRYPTODEV_QAT_SYM_PMD,
		BLKCIPHER_AES_CIPHERONLY_TYPE);

	TEST_ASSERT_EQUAL(status, 0, "Test failed");

	return TEST_SUCCESS;
}

static int
test_AES_chain_dpaa2_sec_all(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	int status;

	status = test_blockcipher_all_tests(ts_params->mbuf_pool,
		ts_params->op_mpool, ts_params->valid_devs[0],
		RTE_CRYPTODEV_DPAA2_SEC_PMD,
		BLKCIPHER_AES_CHAIN_TYPE);

	TEST_ASSERT_EQUAL(status, 0, "Test failed");

	return TEST_SUCCESS;
}

static int
test_AES_cipheronly_dpaa2_sec_all(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	int status;

	status = test_blockcipher_all_tests(ts_params->mbuf_pool,
		ts_params->op_mpool, ts_params->valid_devs[0],
		RTE_CRYPTODEV_DPAA2_SEC_PMD,
		BLKCIPHER_AES_CIPHERONLY_TYPE);

	TEST_ASSERT_EQUAL(status, 0, "Test failed");

	return TEST_SUCCESS;
}

static int
test_authonly_openssl_all(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	int status;

	status = test_blockcipher_all_tests(ts_params->mbuf_pool,
		ts_params->op_mpool, ts_params->valid_devs[0],
		RTE_CRYPTODEV_OPENSSL_PMD,
		BLKCIPHER_AUTHONLY_TYPE);

	TEST_ASSERT_EQUAL(status, 0, "Test failed");

	return TEST_SUCCESS;
}

static int
test_AES_chain_armv8_all(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	int status;

	status = test_blockcipher_all_tests(ts_params->mbuf_pool,
		ts_params->op_mpool, ts_params->valid_devs[0],
		RTE_CRYPTODEV_ARMV8_PMD,
		BLKCIPHER_AES_CHAIN_TYPE);

	TEST_ASSERT_EQUAL(status, 0, "Test failed");

	return TEST_SUCCESS;
}

/* ***** SNOW 3G Tests ***** */
static int
create_wireless_algo_hash_session(uint8_t dev_id,
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
create_wireless_algo_cipher_session(uint8_t dev_id,
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
create_wireless_algo_cipher_operation(const uint8_t *iv, const unsigned iv_len,
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
create_wireless_algo_cipher_operation_oop(const uint8_t *iv, const uint8_t iv_len,
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

	/* For OOP operation both buffers must have the same size */
	if (ut_params->obuf)
		rte_pktmbuf_prepend(ut_params->obuf, iv_pad_len);

	memset(sym_op->cipher.iv.data, 0, iv_pad_len);
	sym_op->cipher.iv.phys_addr = rte_pktmbuf_mtophys(ut_params->ibuf);
	sym_op->cipher.iv.length = iv_pad_len;

	rte_memcpy(sym_op->cipher.iv.data, iv, iv_len);
	sym_op->cipher.data.length = cipher_len;
	sym_op->cipher.data.offset = cipher_offset;
	return 0;
}

static int
create_wireless_algo_cipher_auth_session(uint8_t dev_id,
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
create_wireless_cipher_auth_session(uint8_t dev_id,
		enum rte_crypto_cipher_operation cipher_op,
		enum rte_crypto_auth_operation auth_op,
		enum rte_crypto_auth_algorithm auth_algo,
		enum rte_crypto_cipher_algorithm cipher_algo,
		const struct wireless_test_data *tdata)
{
	const uint8_t key_len = tdata->key.len;
	uint8_t cipher_auth_key[key_len];

	struct crypto_unittest_params *ut_params = &unittest_params;
	const uint8_t *key = tdata->key.data;
	const uint8_t aad_len = tdata->aad.len;
	const uint8_t auth_len = tdata->digest.len;

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
create_zuc_cipher_auth_encrypt_generate_session(uint8_t dev_id,
		const struct wireless_test_data *tdata)
{
	return create_wireless_cipher_auth_session(dev_id,
		RTE_CRYPTO_CIPHER_OP_ENCRYPT,
		RTE_CRYPTO_AUTH_OP_GENERATE, RTE_CRYPTO_AUTH_ZUC_EIA3,
		RTE_CRYPTO_CIPHER_ZUC_EEA3, tdata);
}

static int
create_wireless_algo_auth_cipher_session(uint8_t dev_id,
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
create_wireless_algo_hash_operation(const uint8_t *auth_tag,
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
	*   block size (8 bytes for KASUMI and 16 bytes for SNOW 3G).
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
create_wireless_cipher_hash_operation(const struct wireless_test_data *tdata,
	enum rte_crypto_auth_operation op,
	enum rte_crypto_auth_algorithm auth_algo,
	enum rte_crypto_cipher_algorithm cipher_algo)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	const uint8_t *auth_tag = tdata->digest.data;
	const unsigned int auth_tag_len = tdata->digest.len;
	const uint8_t *aad = tdata->aad.data;
	const uint8_t aad_len = tdata->aad.len;
	unsigned int plaintext_len = ceil_byte_length(tdata->plaintext.len);
	unsigned int data_pad_len = RTE_ALIGN_CEIL(plaintext_len, 16);

	const uint8_t *iv = tdata->iv.data;
	const uint8_t iv_len = tdata->iv.len;
	const unsigned int cipher_len = tdata->validCipherLenInBits.len;
	const unsigned int cipher_offset =
		tdata->validCipherOffsetLenInBits.len;
	const unsigned int auth_len = tdata->validAuthLenInBits.len;
	const unsigned int auth_offset = tdata->validAuthOffsetLenInBits.len;

	unsigned int iv_pad_len = 0;
	unsigned int aad_buffer_len;

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
	ut_params->digest = sym_op->auth.digest.data;
	sym_op->auth.digest.phys_addr = rte_pktmbuf_mtophys_offset(
			ut_params->ibuf, data_pad_len);
	sym_op->auth.digest.length = auth_tag_len;
	if (op == RTE_CRYPTO_AUTH_OP_GENERATE)
		memset(sym_op->auth.digest.data, 0, auth_tag_len);
	else
		rte_memcpy(sym_op->auth.digest.data, auth_tag, auth_tag_len);

	TEST_HEXDUMP(stdout, "digest:",
		sym_op->auth.digest.data,
		sym_op->auth.digest.length);

	/* aad */
	/*
	* Always allocate the aad up to the block size.
	* The cryptodev API calls out -
	*  - the array must be big enough to hold the AAD, plus any
	*   space to round this up to the nearest multiple of the
	*   block size (8 bytes for KASUMI and 16 bytes for SNOW 3G).
	*/
	if (auth_algo == RTE_CRYPTO_AUTH_KASUMI_F9)
		aad_buffer_len = ALIGN_POW2_ROUNDUP(aad_len, 8);
	else
		aad_buffer_len = ALIGN_POW2_ROUNDUP(aad_len, 16);
	sym_op->auth.aad.data =
		(uint8_t *)rte_pktmbuf_prepend(
			ut_params->ibuf, aad_buffer_len);
	TEST_ASSERT_NOT_NULL(sym_op->auth.aad.data,
			"no room to prepend aad");
	sym_op->auth.aad.phys_addr = rte_pktmbuf_mtophys(
			ut_params->ibuf);
	sym_op->auth.aad.length = aad_len;
	memset(sym_op->auth.aad.data, 0, aad_buffer_len);
	rte_memcpy(sym_op->auth.aad.data, aad, aad_len);
	TEST_HEXDUMP(stdout, "aad:", sym_op->auth.aad.data, aad_len);

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
	sym_op->cipher.data.offset = cipher_offset + auth_offset;
	sym_op->auth.data.length = auth_len;
	sym_op->auth.data.offset = auth_offset + cipher_offset;

	return 0;
}

static int
create_zuc_cipher_hash_generate_operation(
		const struct wireless_test_data *tdata)
{
	return create_wireless_cipher_hash_operation(tdata,
		RTE_CRYPTO_AUTH_OP_GENERATE,
		RTE_CRYPTO_AUTH_ZUC_EIA3,
		RTE_CRYPTO_CIPHER_ZUC_EEA3);
}

static int
create_wireless_algo_cipher_hash_operation(const uint8_t *auth_tag,
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

	/* digest */
	sym_op->auth.digest.data = (uint8_t *)rte_pktmbuf_append(
			ut_params->ibuf, auth_tag_len);

	TEST_ASSERT_NOT_NULL(sym_op->auth.digest.data,
			"no room to append auth tag");
	ut_params->digest = sym_op->auth.digest.data;
	sym_op->auth.digest.phys_addr = rte_pktmbuf_mtophys_offset(
			ut_params->ibuf, data_pad_len);
	sym_op->auth.digest.length = auth_tag_len;
	if (op == RTE_CRYPTO_AUTH_OP_GENERATE)
		memset(sym_op->auth.digest.data, 0, auth_tag_len);
	else
		rte_memcpy(sym_op->auth.digest.data, auth_tag, auth_tag_len);

	TEST_HEXDUMP(stdout, "digest:",
		sym_op->auth.digest.data,
		sym_op->auth.digest.length);

	/* aad */
	/*
	* Always allocate the aad up to the block size.
	* The cryptodev API calls out -
	*  - the array must be big enough to hold the AAD, plus any
	*   space to round this up to the nearest multiple of the
	*   block size (8 bytes for KASUMI and 16 bytes for SNOW 3G).
	*/
	if (auth_algo == RTE_CRYPTO_AUTH_KASUMI_F9)
		aad_buffer_len = ALIGN_POW2_ROUNDUP(aad_len, 8);
	else
		aad_buffer_len = ALIGN_POW2_ROUNDUP(aad_len, 16);
	sym_op->auth.aad.data =
		(uint8_t *)rte_pktmbuf_prepend(
			ut_params->ibuf, aad_buffer_len);
	TEST_ASSERT_NOT_NULL(sym_op->auth.aad.data,
			"no room to prepend aad");
	sym_op->auth.aad.phys_addr = rte_pktmbuf_mtophys(
			ut_params->ibuf);
	sym_op->auth.aad.length = aad_len;
	memset(sym_op->auth.aad.data, 0, aad_buffer_len);
	rte_memcpy(sym_op->auth.aad.data, aad, aad_len);
	TEST_HEXDUMP(stdout, "aad:", sym_op->auth.aad.data, aad_len);

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
	sym_op->cipher.data.offset = cipher_offset + auth_offset;
	sym_op->auth.data.length = auth_len;
	sym_op->auth.data.offset = auth_offset + cipher_offset;

	return 0;
}

static int
create_wireless_algo_auth_cipher_operation(const unsigned auth_tag_len,
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

	/* Create SNOW 3G session */
	retval = create_wireless_algo_hash_session(ts_params->valid_devs[0],
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

	/* Create SNOW 3G operation */
	retval = create_wireless_algo_hash_operation(NULL, tdata->digest.len,
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
	"SNOW 3G Generated auth tag not as expected");

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

	/* Create SNOW 3G session */
	retval = create_wireless_algo_hash_session(ts_params->valid_devs[0],
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

	/* Create SNOW 3G operation */
	retval = create_wireless_algo_hash_operation(tdata->digest.data,
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
	retval = create_wireless_algo_hash_session(ts_params->valid_devs[0],
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
	retval = create_wireless_algo_hash_operation(NULL, tdata->digest.len,
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
	retval = create_wireless_algo_hash_session(ts_params->valid_devs[0],
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
	retval = create_wireless_algo_hash_operation(tdata->digest.data,
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
test_kasumi_hash_generate_test_case_6(void)
{
	return test_kasumi_authentication(&kasumi_hash_test_case_6);
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
	retval = create_wireless_algo_cipher_session(ts_params->valid_devs[0],
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
	retval = create_wireless_algo_cipher_operation(tdata->iv.data, tdata->iv.len,
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
test_kasumi_encryption_sgl(const struct kasumi_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;

	unsigned int plaintext_pad_len;
	unsigned int plaintext_len;

	uint8_t buffer[10000];
	const uint8_t *ciphertext;

	struct rte_cryptodev_info dev_info;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	if (!(dev_info.feature_flags & RTE_CRYPTODEV_FF_MBUF_SCATTER_GATHER)) {
		printf("Device doesn't support scatter-gather. "
				"Test Skipped.\n");
		return 0;
	}

	/* Create KASUMI session */
	retval = create_wireless_algo_cipher_session(ts_params->valid_devs[0],
					RTE_CRYPTO_CIPHER_OP_ENCRYPT,
					RTE_CRYPTO_CIPHER_KASUMI_F8,
					tdata->key.data, tdata->key.len);
	if (retval < 0)
		return retval;

	plaintext_len = ceil_byte_length(tdata->plaintext.len);


	/* Append data which is padded to a multiple */
	/* of the algorithms block size */
	plaintext_pad_len = RTE_ALIGN_CEIL(plaintext_len, 8);

	ut_params->ibuf = create_segmented_mbuf(ts_params->mbuf_pool,
			plaintext_pad_len, 10, 0);

	pktmbuf_write(ut_params->ibuf, 0, plaintext_len, tdata->plaintext.data);

	/* Create KASUMI operation */
	retval = create_wireless_algo_cipher_operation(tdata->iv.data,
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
		ciphertext = rte_pktmbuf_read(ut_params->obuf, tdata->iv.len,
				plaintext_len, buffer);
	else
		ciphertext = rte_pktmbuf_read(ut_params->ibuf, tdata->iv.len,
				plaintext_len, buffer);

	/* Validate obuf */
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
	retval = create_wireless_algo_cipher_session(ts_params->valid_devs[0],
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
	retval = create_wireless_algo_cipher_operation_oop(tdata->iv.data,
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
test_kasumi_encryption_oop_sgl(const struct kasumi_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;
	unsigned int plaintext_pad_len;
	unsigned int plaintext_len;

	const uint8_t *ciphertext;
	uint8_t buffer[2048];

	struct rte_cryptodev_info dev_info;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	if (!(dev_info.feature_flags & RTE_CRYPTODEV_FF_MBUF_SCATTER_GATHER)) {
		printf("Device doesn't support scatter-gather. "
				"Test Skipped.\n");
		return 0;
	}

	/* Create KASUMI session */
	retval = create_wireless_algo_cipher_session(ts_params->valid_devs[0],
					RTE_CRYPTO_CIPHER_OP_ENCRYPT,
					RTE_CRYPTO_CIPHER_KASUMI_F8,
					tdata->key.data, tdata->key.len);
	if (retval < 0)
		return retval;

	plaintext_len = ceil_byte_length(tdata->plaintext.len);
	/* Append data which is padded to a multiple */
	/* of the algorithms block size */
	plaintext_pad_len = RTE_ALIGN_CEIL(plaintext_len, 8);

	ut_params->ibuf = create_segmented_mbuf(ts_params->mbuf_pool,
			plaintext_pad_len, 10, 0);
	ut_params->obuf = create_segmented_mbuf(ts_params->mbuf_pool,
			plaintext_pad_len, 3, 0);

	/* Append data which is padded to a multiple */
	/* of the algorithms block size */
	pktmbuf_write(ut_params->ibuf, 0, plaintext_len, tdata->plaintext.data);

	/* Create KASUMI operation */
	retval = create_wireless_algo_cipher_operation_oop(tdata->iv.data,
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
		ciphertext = rte_pktmbuf_read(ut_params->obuf, tdata->iv.len,
				plaintext_pad_len, buffer);
	else
		ciphertext = rte_pktmbuf_read(ut_params->ibuf, tdata->iv.len,
				plaintext_pad_len, buffer);

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
	retval = create_wireless_algo_cipher_session(ts_params->valid_devs[0],
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
	retval = create_wireless_algo_cipher_operation_oop(tdata->iv.data,
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
	retval = create_wireless_algo_cipher_session(ts_params->valid_devs[0],
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
	retval = create_wireless_algo_cipher_operation(tdata->iv.data,
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

	/* Create SNOW 3G session */
	retval = create_wireless_algo_cipher_session(ts_params->valid_devs[0],
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

	/* Create SNOW 3G operation */
	retval = create_wireless_algo_cipher_operation(tdata->iv.data, tdata->iv.len,
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
		"SNOW 3G Ciphertext data not as expected");
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

	/* Create SNOW 3G session */
	retval = create_wireless_algo_cipher_session(ts_params->valid_devs[0],
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

	/* Create SNOW 3G operation */
	retval = create_wireless_algo_cipher_operation_oop(tdata->iv.data,
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
		"SNOW 3G Ciphertext data not as expected");
	return 0;
}

static int
test_snow3g_encryption_oop_sgl(const struct snow3g_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;
	unsigned int plaintext_pad_len;
	unsigned int plaintext_len;
	uint8_t buffer[10000];
	const uint8_t *ciphertext;

	struct rte_cryptodev_info dev_info;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	if (!(dev_info.feature_flags & RTE_CRYPTODEV_FF_MBUF_SCATTER_GATHER)) {
		printf("Device doesn't support scatter-gather. "
				"Test Skipped.\n");
		return 0;
	}

	/* Create SNOW 3G session */
	retval = create_wireless_algo_cipher_session(ts_params->valid_devs[0],
					RTE_CRYPTO_CIPHER_OP_ENCRYPT,
					RTE_CRYPTO_CIPHER_SNOW3G_UEA2,
					tdata->key.data, tdata->key.len);
	if (retval < 0)
		return retval;

	plaintext_len = ceil_byte_length(tdata->plaintext.len);
	/* Append data which is padded to a multiple of */
	/* the algorithms block size */
	plaintext_pad_len = RTE_ALIGN_CEIL(plaintext_len, 16);

	ut_params->ibuf = create_segmented_mbuf(ts_params->mbuf_pool,
			plaintext_pad_len, 10, 0);
	ut_params->obuf = create_segmented_mbuf(ts_params->mbuf_pool,
			plaintext_pad_len, 3, 0);

	TEST_ASSERT_NOT_NULL(ut_params->ibuf,
			"Failed to allocate input buffer in mempool");
	TEST_ASSERT_NOT_NULL(ut_params->obuf,
			"Failed to allocate output buffer in mempool");

	pktmbuf_write(ut_params->ibuf, 0, plaintext_len, tdata->plaintext.data);

	/* Create SNOW 3G operation */
	retval = create_wireless_algo_cipher_operation_oop(tdata->iv.data,
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
		ciphertext = rte_pktmbuf_read(ut_params->obuf, tdata->iv.len,
				plaintext_len, buffer);
	else
		ciphertext = rte_pktmbuf_read(ut_params->ibuf, tdata->iv.len,
				plaintext_len, buffer);

	TEST_HEXDUMP(stdout, "ciphertext:", ciphertext, plaintext_len);

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(
		ciphertext,
		tdata->ciphertext.data,
		tdata->validDataLenInBits.len,
		"SNOW 3G Ciphertext data not as expected");

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

	/* Create SNOW 3G session */
	retval = create_wireless_algo_cipher_session(ts_params->valid_devs[0],
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
	/* Create SNOW 3G operation */
	retval = create_wireless_algo_cipher_operation_oop(tdata->iv.data,
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
		"SNOW 3G Ciphertext data not as expected");
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

	/* Create SNOW 3G session */
	retval = create_wireless_algo_cipher_session(ts_params->valid_devs[0],
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

	/* Create SNOW 3G operation */
	retval = create_wireless_algo_cipher_operation(tdata->iv.data, tdata->iv.len,
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
				"SNOW 3G Plaintext data not as expected");
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

	/* Create SNOW 3G session */
	retval = create_wireless_algo_cipher_session(ts_params->valid_devs[0],
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

	/* Create SNOW 3G operation */
	retval = create_wireless_algo_cipher_operation_oop(tdata->iv.data,
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
				"SNOW 3G Plaintext data not as expected");
	return 0;
}

static int
test_zuc_cipher_auth(const struct wireless_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;

	uint8_t *plaintext, *ciphertext;
	unsigned int plaintext_pad_len;
	unsigned int plaintext_len;

	struct rte_cryptodev_sym_capability_idx cap_idx;

	/* Check if device supports ZUC EEA3 */
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	cap_idx.algo.cipher = RTE_CRYPTO_CIPHER_ZUC_EEA3;

	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return -ENOTSUP;

	/* Check if device supports ZUC EIA3 */
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	cap_idx.algo.auth = RTE_CRYPTO_AUTH_ZUC_EIA3;

	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return -ENOTSUP;

	/* Create ZUC session */
	retval = create_zuc_cipher_auth_encrypt_generate_session(
			ts_params->valid_devs[0],
			tdata);
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

	/* Create ZUC operation */
	retval = create_zuc_cipher_hash_generate_operation(tdata);
	if (retval < 0)
		return retval;

	ut_params->op = process_crypto_request(ts_params->valid_devs[0],
			ut_params->op);
	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");
	ut_params->obuf = ut_params->op->sym->m_src;
	if (ut_params->obuf)
		ciphertext = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *)
				+ tdata->iv.len + tdata->aad.len;
	else
		ciphertext = plaintext;

	TEST_HEXDUMP(stdout, "ciphertext:", ciphertext, plaintext_len);
	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(
			ciphertext,
			tdata->ciphertext.data,
			tdata->validDataLenInBits.len,
			"ZUC Ciphertext data not as expected");

	ut_params->digest = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *)
	    + plaintext_pad_len + tdata->aad.len + tdata->iv.len;

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			ut_params->digest,
			tdata->digest.data,
			4,
			"ZUC Generated auth tag not as expected");
	return 0;
}

static int
test_snow3g_cipher_auth(const struct snow3g_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;

	uint8_t *plaintext, *ciphertext;
	unsigned plaintext_pad_len;
	unsigned plaintext_len;

	/* Create SNOW 3G session */
	retval = create_wireless_algo_cipher_auth_session(ts_params->valid_devs[0],
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

	/* Create SNOW 3G operation */
	retval = create_wireless_algo_cipher_hash_operation(tdata->digest.data,
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
				+ tdata->iv.len + tdata->aad.len;
	else
		ciphertext = plaintext;

	TEST_HEXDUMP(stdout, "ciphertext:", ciphertext, plaintext_len);
	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(
			ciphertext,
			tdata->ciphertext.data,
			tdata->validDataLenInBits.len,
			"SNOW 3G Ciphertext data not as expected");

	ut_params->digest = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *)
	    + plaintext_pad_len + tdata->aad.len + tdata->iv.len;

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			ut_params->digest,
			tdata->digest.data,
			DIGEST_BYTE_LENGTH_SNOW3G_UIA2,
			"SNOW 3G Generated auth tag not as expected");
	return 0;
}
static int
test_snow3g_auth_cipher(const struct snow3g_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;

	uint8_t *plaintext, *ciphertext;
	unsigned plaintext_pad_len;
	unsigned plaintext_len;

	/* Create SNOW 3G session */
	retval = create_wireless_algo_auth_cipher_session(ts_params->valid_devs[0],
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

	/* Create SNOW 3G operation */
	retval = create_wireless_algo_auth_cipher_operation(
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
		"SNOW 3G Ciphertext data not as expected");

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL(
		ut_params->digest,
		tdata->digest.data,
		DIGEST_BYTE_LENGTH_SNOW3G_UIA2,
		"SNOW 3G Generated auth tag not as expected");
	return 0;
}

static int
test_kasumi_auth_cipher(const struct kasumi_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;

	uint8_t *plaintext, *ciphertext;
	unsigned plaintext_pad_len;
	unsigned plaintext_len;

	/* Create KASUMI session */
	retval = create_wireless_algo_auth_cipher_session(
			ts_params->valid_devs[0],
			RTE_CRYPTO_CIPHER_OP_ENCRYPT,
			RTE_CRYPTO_AUTH_OP_GENERATE,
			RTE_CRYPTO_AUTH_KASUMI_F9,
			RTE_CRYPTO_CIPHER_KASUMI_F8,
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

	/* Create KASUMI operation */
	retval = create_wireless_algo_auth_cipher_operation(tdata->digest.len,
				tdata->iv.data, tdata->iv.len,
				tdata->aad.data, tdata->aad.len,
				plaintext_pad_len,
				tdata->validCipherLenInBits.len,
				tdata->validCipherOffsetLenInBits.len,
				tdata->validAuthLenInBits.len,
				tdata->validAuthOffsetLenInBits.len,
				RTE_CRYPTO_AUTH_KASUMI_F9,
				RTE_CRYPTO_CIPHER_KASUMI_F8
				);

	if (retval < 0)
		return retval;

	ut_params->op = process_crypto_request(ts_params->valid_devs[0],
			ut_params->op);
	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");
	ut_params->obuf = ut_params->op->sym->m_src;
	if (ut_params->obuf)
		ciphertext = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *)
				+ tdata->iv.len + tdata->aad.len;
	else
		ciphertext = plaintext;

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(
			ciphertext,
			tdata->ciphertext.data,
			tdata->validCipherLenInBits.len,
			"KASUMI Ciphertext data not as expected");
	ut_params->digest = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *)
	    + plaintext_pad_len + tdata->aad.len + tdata->iv.len;

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			ut_params->digest,
			tdata->digest.data,
			DIGEST_BYTE_LENGTH_KASUMI_F9,
			"KASUMI Generated auth tag not as expected");
	return 0;
}

static int
test_kasumi_cipher_auth(const struct kasumi_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;

	uint8_t *plaintext, *ciphertext;
	unsigned plaintext_pad_len;
	unsigned plaintext_len;

	/* Create KASUMI session */
	retval = create_wireless_algo_cipher_auth_session(
			ts_params->valid_devs[0],
			RTE_CRYPTO_CIPHER_OP_ENCRYPT,
			RTE_CRYPTO_AUTH_OP_GENERATE,
			RTE_CRYPTO_AUTH_KASUMI_F9,
			RTE_CRYPTO_CIPHER_KASUMI_F8,
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

	/* Create KASUMI operation */
	retval = create_wireless_algo_cipher_hash_operation(tdata->digest.data,
				tdata->digest.len, tdata->aad.data,
				tdata->aad.len,
				plaintext_pad_len, RTE_CRYPTO_AUTH_OP_GENERATE,
				RTE_CRYPTO_AUTH_KASUMI_F9,
				RTE_CRYPTO_CIPHER_KASUMI_F8,
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
				+ tdata->aad.len + tdata->iv.len;
	else
		ciphertext = plaintext;

	ut_params->digest = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *)
			+ plaintext_pad_len + tdata->aad.len + tdata->iv.len;

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(
		ciphertext,
		tdata->ciphertext.data,
		tdata->validCipherLenInBits.len,
		"KASUMI Ciphertext data not as expected");

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL(
		ut_params->digest,
		tdata->digest.data,
		DIGEST_BYTE_LENGTH_SNOW3G_UIA2,
		"KASUMI Generated auth tag not as expected");
	return 0;
}

static int
test_zuc_encryption(const struct wireless_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;
	uint8_t *plaintext, *ciphertext;
	unsigned plaintext_pad_len;
	unsigned plaintext_len;

	struct rte_cryptodev_sym_capability_idx cap_idx;

	/* Check if device supports ZUC EEA3 */
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	cap_idx.algo.cipher = RTE_CRYPTO_CIPHER_ZUC_EEA3;

	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return -ENOTSUP;

	/* Create ZUC session */
	retval = create_wireless_algo_cipher_session(ts_params->valid_devs[0],
					RTE_CRYPTO_CIPHER_OP_ENCRYPT,
					RTE_CRYPTO_CIPHER_ZUC_EEA3,
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

	/* Create ZUC operation */
	retval = create_wireless_algo_cipher_operation(tdata->iv.data, tdata->iv.len,
					tdata->plaintext.len,
					tdata->validCipherOffsetLenInBits.len,
					RTE_CRYPTO_CIPHER_ZUC_EEA3);
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
		"ZUC Ciphertext data not as expected");
	return 0;
}

static int
test_zuc_encryption_sgl(const struct wireless_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;

	unsigned int plaintext_pad_len;
	unsigned int plaintext_len;
	const uint8_t *ciphertext;
	uint8_t ciphertext_buffer[2048];
	struct rte_cryptodev_info dev_info;

	struct rte_cryptodev_sym_capability_idx cap_idx;

	/* Check if device supports ZUC EEA3 */
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	cap_idx.algo.cipher = RTE_CRYPTO_CIPHER_ZUC_EEA3;

	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return -ENOTSUP;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	if (!(dev_info.feature_flags & RTE_CRYPTODEV_FF_MBUF_SCATTER_GATHER)) {
		printf("Device doesn't support scatter-gather. "
				"Test Skipped.\n");
		return -ENOTSUP;
	}

	plaintext_len = ceil_byte_length(tdata->plaintext.len);

	/* Append data which is padded to a multiple */
	/* of the algorithms block size */
	plaintext_pad_len = RTE_ALIGN_CEIL(plaintext_len, 8);

	ut_params->ibuf = create_segmented_mbuf(ts_params->mbuf_pool,
			plaintext_pad_len, 10, 0);

	pktmbuf_write(ut_params->ibuf, 0, plaintext_len,
			tdata->plaintext.data);

	/* Create ZUC session */
	retval = create_wireless_algo_cipher_session(ts_params->valid_devs[0],
			RTE_CRYPTO_CIPHER_OP_ENCRYPT,
			RTE_CRYPTO_CIPHER_ZUC_EEA3,
			tdata->key.data, tdata->key.len);
	if (retval < 0)
		return retval;

	/* Clear mbuf payload */

	pktmbuf_write(ut_params->ibuf, 0, plaintext_len, tdata->plaintext.data);

	/* Create ZUC operation */
	retval = create_wireless_algo_cipher_operation(tdata->iv.data,
			tdata->iv.len, tdata->plaintext.len,
			tdata->validCipherOffsetLenInBits.len,
			RTE_CRYPTO_CIPHER_ZUC_EEA3);
	if (retval < 0)
		return retval;

	ut_params->op = process_crypto_request(ts_params->valid_devs[0],
						ut_params->op);
	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");

	ut_params->obuf = ut_params->op->sym->m_dst;
	if (ut_params->obuf)
		ciphertext = rte_pktmbuf_read(ut_params->obuf,
			tdata->iv.len, plaintext_len, ciphertext_buffer);
	else
		ciphertext = rte_pktmbuf_read(ut_params->ibuf,
			tdata->iv.len, plaintext_len, ciphertext_buffer);

	/* Validate obuf */
	TEST_HEXDUMP(stdout, "ciphertext:", ciphertext, plaintext_len);

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(
		ciphertext,
		tdata->ciphertext.data,
		tdata->validCipherLenInBits.len,
		"ZUC Ciphertext data not as expected");

	return 0;
}

static int
test_zuc_authentication(const struct wireless_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;
	unsigned plaintext_pad_len;
	unsigned plaintext_len;
	uint8_t *plaintext;

	struct rte_cryptodev_sym_capability_idx cap_idx;

	/* Check if device supports ZUC EIA3 */
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	cap_idx.algo.auth = RTE_CRYPTO_AUTH_ZUC_EIA3;

	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return -ENOTSUP;

	/* Create ZUC session */
	retval = create_wireless_algo_hash_session(ts_params->valid_devs[0],
			tdata->key.data, tdata->key.len,
			tdata->aad.len, tdata->digest.len,
			RTE_CRYPTO_AUTH_OP_GENERATE,
			RTE_CRYPTO_AUTH_ZUC_EIA3);
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

	/* Create ZUC operation */
	retval = create_wireless_algo_hash_operation(NULL, tdata->digest.len,
			tdata->aad.data, tdata->aad.len,
			plaintext_pad_len, RTE_CRYPTO_AUTH_OP_GENERATE,
			RTE_CRYPTO_AUTH_ZUC_EIA3,
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
	"ZUC Generated auth tag not as expected");

	return 0;
}

static int
test_kasumi_encryption_test_case_1(void)
{
	return test_kasumi_encryption(&kasumi_test_case_1);
}

static int
test_kasumi_encryption_test_case_1_sgl(void)
{
	return test_kasumi_encryption_sgl(&kasumi_test_case_1);
}

static int
test_kasumi_encryption_test_case_1_oop(void)
{
	return test_kasumi_encryption_oop(&kasumi_test_case_1);
}

static int
test_kasumi_encryption_test_case_1_oop_sgl(void)
{
	return test_kasumi_encryption_oop_sgl(&kasumi_test_case_1);
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
test_snow3g_encryption_test_case_1_oop_sgl(void)
{
	return test_snow3g_encryption_oop_sgl(&snow3g_test_case_1);
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
test_snow3g_cipher_auth_test_case_1(void)
{
	return test_snow3g_cipher_auth(&snow3g_test_case_3);
}

static int
test_snow3g_auth_cipher_test_case_1(void)
{
	return test_snow3g_auth_cipher(&snow3g_test_case_6);
}

static int
test_kasumi_auth_cipher_test_case_1(void)
{
	return test_kasumi_auth_cipher(&kasumi_test_case_3);
}

static int
test_kasumi_cipher_auth_test_case_1(void)
{
	return test_kasumi_cipher_auth(&kasumi_test_case_6);
}

static int
test_zuc_encryption_test_case_1(void)
{
	return test_zuc_encryption(&zuc_test_case_cipher_193b);
}

static int
test_zuc_encryption_test_case_2(void)
{
	return test_zuc_encryption(&zuc_test_case_cipher_800b);
}

static int
test_zuc_encryption_test_case_3(void)
{
	return test_zuc_encryption(&zuc_test_case_cipher_1570b);
}

static int
test_zuc_encryption_test_case_4(void)
{
	return test_zuc_encryption(&zuc_test_case_cipher_2798b);
}

static int
test_zuc_encryption_test_case_5(void)
{
	return test_zuc_encryption(&zuc_test_case_cipher_4019b);
}

static int
test_zuc_encryption_test_case_6_sgl(void)
{
	return test_zuc_encryption_sgl(&zuc_test_case_cipher_193b);
}

static int
test_zuc_hash_generate_test_case_1(void)
{
	return test_zuc_authentication(&zuc_test_case_auth_1b);
}

static int
test_zuc_hash_generate_test_case_2(void)
{
	return test_zuc_authentication(&zuc_test_case_auth_90b);
}

static int
test_zuc_hash_generate_test_case_3(void)
{
	return test_zuc_authentication(&zuc_test_case_auth_577b);
}

static int
test_zuc_hash_generate_test_case_4(void)
{
	return test_zuc_authentication(&zuc_test_case_auth_2079b);
}

static int
test_zuc_hash_generate_test_case_5(void)
{
	return test_zuc_authentication(&zuc_test_auth_5670b);
}

static int
test_zuc_hash_generate_test_case_6(void)
{
	return test_zuc_authentication(&zuc_test_case_auth_128b);
}

static int
test_zuc_hash_generate_test_case_7(void)
{
	return test_zuc_authentication(&zuc_test_case_auth_2080b);
}

static int
test_zuc_hash_generate_test_case_8(void)
{
	return test_zuc_authentication(&zuc_test_case_auth_584b);
}

static int
test_zuc_cipher_auth_test_case_1(void)
{
	return test_zuc_cipher_auth(&zuc_test_case_cipher_200b_auth_200b);
}

static int
test_zuc_cipher_auth_test_case_2(void)
{
	return test_zuc_cipher_auth(&zuc_test_case_cipher_800b_auth_120b);
}

static int
test_3DES_chain_qat_all(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	int status;

	status = test_blockcipher_all_tests(ts_params->mbuf_pool,
		ts_params->op_mpool, ts_params->valid_devs[0],
		RTE_CRYPTODEV_QAT_SYM_PMD,
		BLKCIPHER_3DES_CHAIN_TYPE);

	TEST_ASSERT_EQUAL(status, 0, "Test failed");

	return TEST_SUCCESS;
}

static int
test_DES_cipheronly_qat_all(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	int status;

	status = test_blockcipher_all_tests(ts_params->mbuf_pool,
		ts_params->op_mpool, ts_params->valid_devs[0],
		RTE_CRYPTODEV_QAT_SYM_PMD,
		BLKCIPHER_DES_CIPHERONLY_TYPE);

	TEST_ASSERT_EQUAL(status, 0, "Test failed");

	return TEST_SUCCESS;
}

static int
test_DES_docsis_openssl_all(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	int status;

	status = test_blockcipher_all_tests(ts_params->mbuf_pool,
		ts_params->op_mpool, ts_params->valid_devs[0],
		RTE_CRYPTODEV_OPENSSL_PMD,
		BLKCIPHER_DES_DOCSIS_TYPE);

	TEST_ASSERT_EQUAL(status, 0, "Test failed");

	return TEST_SUCCESS;
}

static int
test_3DES_chain_dpaa2_sec_all(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	int status;

	status = test_blockcipher_all_tests(ts_params->mbuf_pool,
		ts_params->op_mpool, ts_params->valid_devs[0],
		RTE_CRYPTODEV_DPAA2_SEC_PMD,
		BLKCIPHER_3DES_CHAIN_TYPE);

	TEST_ASSERT_EQUAL(status, 0, "Test failed");

	return TEST_SUCCESS;
}

static int
test_3DES_cipheronly_dpaa2_sec_all(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	int status;

	status = test_blockcipher_all_tests(ts_params->mbuf_pool,
		ts_params->op_mpool, ts_params->valid_devs[0],
		RTE_CRYPTODEV_DPAA2_SEC_PMD,
		BLKCIPHER_3DES_CIPHERONLY_TYPE);

	TEST_ASSERT_EQUAL(status, 0, "Test failed");

	return TEST_SUCCESS;
}

static int
test_3DES_cipheronly_qat_all(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	int status;

	status = test_blockcipher_all_tests(ts_params->mbuf_pool,
		ts_params->op_mpool, ts_params->valid_devs[0],
		RTE_CRYPTODEV_QAT_SYM_PMD,
		BLKCIPHER_3DES_CIPHERONLY_TYPE);

	TEST_ASSERT_EQUAL(status, 0, "Test failed");

	return TEST_SUCCESS;
}

static int
test_3DES_chain_openssl_all(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	int status;

	status = test_blockcipher_all_tests(ts_params->mbuf_pool,
		ts_params->op_mpool, ts_params->valid_devs[0],
		RTE_CRYPTODEV_OPENSSL_PMD,
		BLKCIPHER_3DES_CHAIN_TYPE);

	TEST_ASSERT_EQUAL(status, 0, "Test failed");

	return TEST_SUCCESS;
}

static int
test_3DES_cipheronly_openssl_all(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	int status;

	status = test_blockcipher_all_tests(ts_params->mbuf_pool,
		ts_params->op_mpool, ts_params->valid_devs[0],
		RTE_CRYPTODEV_OPENSSL_PMD,
		BLKCIPHER_3DES_CIPHERONLY_TYPE);

	TEST_ASSERT_EQUAL(status, 0, "Test failed");

	return TEST_SUCCESS;
}

/* ***** AES-GCM Tests ***** */

static int
create_gcm_session(uint8_t dev_id, enum rte_crypto_cipher_operation op,
		const uint8_t *key, const uint8_t key_len,
		const uint8_t aad_len, const uint8_t auth_len,
		enum rte_crypto_auth_operation auth_op)
{
	uint8_t cipher_key[key_len];

	struct crypto_unittest_params *ut_params = &unittest_params;

	memcpy(cipher_key, key, key_len);

	/* Setup Cipher Parameters */
	ut_params->cipher_xform.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	ut_params->cipher_xform.next = NULL;

	ut_params->cipher_xform.cipher.algo = RTE_CRYPTO_CIPHER_AES_GCM;
	ut_params->auth_xform.auth.op = auth_op;
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
create_gcm_xforms(struct rte_crypto_op *op,
		enum rte_crypto_cipher_operation cipher_op,
		uint8_t *key, const uint8_t key_len,
		const uint8_t aad_len, const uint8_t auth_len,
		enum rte_crypto_auth_operation auth_op)
{
	TEST_ASSERT_NOT_NULL(rte_crypto_op_sym_xforms_alloc(op, 2),
			"failed to allocate space for crypto transforms");

	struct rte_crypto_sym_op *sym_op = op->sym;

	/* Setup Cipher Parameters */
	sym_op->xform->type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	sym_op->xform->cipher.algo = RTE_CRYPTO_CIPHER_AES_GCM;
	sym_op->xform->cipher.op = cipher_op;
	sym_op->xform->cipher.key.data = key;
	sym_op->xform->cipher.key.length = key_len;

	TEST_HEXDUMP(stdout, "key:", key, key_len);

	/* Setup Authentication Parameters */
	sym_op->xform->next->type = RTE_CRYPTO_SYM_XFORM_AUTH;
	sym_op->xform->next->auth.algo = RTE_CRYPTO_AUTH_AES_GCM;
	sym_op->xform->next->auth.op = auth_op;
	sym_op->xform->next->auth.digest_length = auth_len;
	sym_op->xform->next->auth.add_auth_data_length = aad_len;
	sym_op->xform->next->auth.key.length = 0;
	sym_op->xform->next->auth.key.data = NULL;
	sym_op->xform->next->next = NULL;

	return 0;
}

static int
create_gcm_operation(enum rte_crypto_cipher_operation op,
		const struct gcm_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	uint8_t *plaintext, *ciphertext;
	unsigned int iv_pad_len, aad_pad_len, plaintext_pad_len;

	/* Generate Crypto op data structure */
	ut_params->op = rte_crypto_op_alloc(ts_params->op_mpool,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	TEST_ASSERT_NOT_NULL(ut_params->op,
			"Failed to allocate symmetric crypto operation struct");

	struct rte_crypto_sym_op *sym_op = ut_params->op->sym;

	/* Append aad data */
	aad_pad_len = RTE_ALIGN_CEIL(tdata->aad.len, 16);
	sym_op->auth.aad.data = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
			aad_pad_len);
	TEST_ASSERT_NOT_NULL(sym_op->auth.aad.data,
			"no room to append aad");

	sym_op->auth.aad.length = tdata->aad.len;
	sym_op->auth.aad.phys_addr =
			rte_pktmbuf_mtophys(ut_params->ibuf);
	memcpy(sym_op->auth.aad.data, tdata->aad.data, tdata->aad.len);
	TEST_HEXDUMP(stdout, "aad:", sym_op->auth.aad.data,
		sym_op->auth.aad.length);

	/* Prepend iv */
	iv_pad_len = RTE_ALIGN_CEIL(tdata->iv.len, 16);
	sym_op->cipher.iv.data = (uint8_t *)rte_pktmbuf_prepend(
			ut_params->ibuf, iv_pad_len);
	TEST_ASSERT_NOT_NULL(sym_op->cipher.iv.data, "no room to prepend iv");

	memset(sym_op->cipher.iv.data, 0, iv_pad_len);
	sym_op->cipher.iv.phys_addr = rte_pktmbuf_mtophys(ut_params->ibuf);
	sym_op->cipher.iv.length = tdata->iv.len;

	rte_memcpy(sym_op->cipher.iv.data, tdata->iv.data, tdata->iv.len);
	TEST_HEXDUMP(stdout, "iv:", sym_op->cipher.iv.data,
		sym_op->cipher.iv.length);

	/* Append plaintext/ciphertext */
	if (op == RTE_CRYPTO_CIPHER_OP_ENCRYPT) {
		plaintext_pad_len = RTE_ALIGN_CEIL(tdata->plaintext.len, 16);
		plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				plaintext_pad_len);
		TEST_ASSERT_NOT_NULL(plaintext, "no room to append plaintext");

		memcpy(plaintext, tdata->plaintext.data, tdata->plaintext.len);
		TEST_HEXDUMP(stdout, "plaintext:", plaintext,
				tdata->plaintext.len);

		if (ut_params->obuf) {
			ciphertext = (uint8_t *)rte_pktmbuf_append(
					ut_params->obuf,
					plaintext_pad_len + aad_pad_len +
					iv_pad_len);
			TEST_ASSERT_NOT_NULL(ciphertext,
					"no room to append ciphertext");

			memset(ciphertext + aad_pad_len + iv_pad_len, 0,
					tdata->ciphertext.len);
		}
	} else {
		plaintext_pad_len = RTE_ALIGN_CEIL(tdata->ciphertext.len, 16);
		ciphertext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				plaintext_pad_len);
		TEST_ASSERT_NOT_NULL(ciphertext,
				"no room to append ciphertext");

		memcpy(ciphertext, tdata->ciphertext.data,
				tdata->ciphertext.len);
		TEST_HEXDUMP(stdout, "ciphertext:", ciphertext,
				tdata->ciphertext.len);

		if (ut_params->obuf) {
			plaintext = (uint8_t *)rte_pktmbuf_append(
					ut_params->obuf,
					plaintext_pad_len + aad_pad_len +
					iv_pad_len);
			TEST_ASSERT_NOT_NULL(plaintext,
					"no room to append plaintext");

			memset(plaintext + aad_pad_len + iv_pad_len, 0,
					tdata->plaintext.len);
		}
	}

	/* Append digest data */
	if (op == RTE_CRYPTO_CIPHER_OP_ENCRYPT) {
		sym_op->auth.digest.data = (uint8_t *)rte_pktmbuf_append(
				ut_params->obuf ? ut_params->obuf :
						ut_params->ibuf,
						tdata->auth_tag.len);
		TEST_ASSERT_NOT_NULL(sym_op->auth.digest.data,
				"no room to append digest");
		memset(sym_op->auth.digest.data, 0, tdata->auth_tag.len);
		sym_op->auth.digest.phys_addr = rte_pktmbuf_mtophys_offset(
				ut_params->obuf ? ut_params->obuf :
						ut_params->ibuf,
						plaintext_pad_len +
						aad_pad_len + iv_pad_len);
		sym_op->auth.digest.length = tdata->auth_tag.len;
	} else {
		sym_op->auth.digest.data = (uint8_t *)rte_pktmbuf_append(
				ut_params->ibuf, tdata->auth_tag.len);
		TEST_ASSERT_NOT_NULL(sym_op->auth.digest.data,
				"no room to append digest");
		sym_op->auth.digest.phys_addr = rte_pktmbuf_mtophys_offset(
				ut_params->ibuf,
				plaintext_pad_len + aad_pad_len + iv_pad_len);
		sym_op->auth.digest.length = tdata->auth_tag.len;

		rte_memcpy(sym_op->auth.digest.data, tdata->auth_tag.data,
			tdata->auth_tag.len);
		TEST_HEXDUMP(stdout, "digest:",
			sym_op->auth.digest.data,
			sym_op->auth.digest.length);
	}

	sym_op->cipher.data.length = tdata->plaintext.len;
	sym_op->cipher.data.offset = aad_pad_len + iv_pad_len;

	sym_op->auth.data.length = tdata->plaintext.len;
	sym_op->auth.data.offset = aad_pad_len + iv_pad_len;

	return 0;
}

static int
test_mb_AES_GCM_authenticated_encryption(const struct gcm_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;
	uint8_t *ciphertext, *auth_tag;
	uint16_t plaintext_pad_len;
	uint32_t i;

	/* Create GCM session */
	retval = create_gcm_session(ts_params->valid_devs[0],
			RTE_CRYPTO_CIPHER_OP_ENCRYPT,
			tdata->key.data, tdata->key.len,
			tdata->aad.len, tdata->auth_tag.len,
			RTE_CRYPTO_AUTH_OP_GENERATE);
	if (retval < 0)
		return retval;

	if (tdata->aad.len > MBUF_SIZE) {
		ut_params->ibuf = rte_pktmbuf_alloc(ts_params->large_mbuf_pool);
		/* Populate full size of add data */
		for (i = 32; i < GCM_MAX_AAD_LENGTH; i += 32)
			memcpy(&tdata->aad.data[i], &tdata->aad.data[0], 32);
	} else
		ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	/* clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->ibuf));

	/* Create GCM operation */
	retval = create_gcm_operation(RTE_CRYPTO_CIPHER_OP_ENCRYPT, tdata);
	if (retval < 0)
		return retval;

	rte_crypto_op_attach_sym_session(ut_params->op, ut_params->sess);

	ut_params->op->sym->m_src = ut_params->ibuf;

	/* Process crypto operation */
	TEST_ASSERT_NOT_NULL(process_crypto_request(ts_params->valid_devs[0],
			ut_params->op), "failed to process sym crypto op");

	TEST_ASSERT_EQUAL(ut_params->op->status, RTE_CRYPTO_OP_STATUS_SUCCESS,
			"crypto op processing failed");

	plaintext_pad_len = RTE_ALIGN_CEIL(tdata->plaintext.len, 16);

	if (ut_params->op->sym->m_dst) {
		ciphertext = rte_pktmbuf_mtod(ut_params->op->sym->m_dst,
				uint8_t *);
		auth_tag = rte_pktmbuf_mtod_offset(ut_params->op->sym->m_dst,
				uint8_t *, plaintext_pad_len);
	} else {
		ciphertext = rte_pktmbuf_mtod_offset(ut_params->op->sym->m_src,
				uint8_t *,
				ut_params->op->sym->cipher.data.offset);
		auth_tag = ciphertext + plaintext_pad_len;
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
test_mb_AES_GCM_auth_encryption_test_case_256_1(void)
{
	return test_mb_AES_GCM_authenticated_encryption(&gcm_test_case_256_1);
}

static int
test_mb_AES_GCM_auth_encryption_test_case_256_2(void)
{
	return test_mb_AES_GCM_authenticated_encryption(&gcm_test_case_256_2);
}

static int
test_mb_AES_GCM_auth_encryption_test_case_256_3(void)
{
	return test_mb_AES_GCM_authenticated_encryption(&gcm_test_case_256_3);
}

static int
test_mb_AES_GCM_auth_encryption_test_case_256_4(void)
{
	return test_mb_AES_GCM_authenticated_encryption(&gcm_test_case_256_4);
}

static int
test_mb_AES_GCM_auth_encryption_test_case_256_5(void)
{
	return test_mb_AES_GCM_authenticated_encryption(&gcm_test_case_256_5);
}

static int
test_mb_AES_GCM_auth_encryption_test_case_256_6(void)
{
	return test_mb_AES_GCM_authenticated_encryption(&gcm_test_case_256_6);
}

static int
test_mb_AES_GCM_auth_encryption_test_case_256_7(void)
{
	return test_mb_AES_GCM_authenticated_encryption(&gcm_test_case_256_7);
}

static int
test_mb_AES_GCM_auth_encryption_test_case_aad_1(void)
{
	return test_mb_AES_GCM_authenticated_encryption(&gcm_test_case_aad_1);
}

static int
test_mb_AES_GCM_auth_encryption_test_case_aad_2(void)
{
	return test_mb_AES_GCM_authenticated_encryption(&gcm_test_case_aad_2);
}

static int
test_mb_AES_GCM_authenticated_decryption(const struct gcm_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;
	uint8_t *plaintext;
	uint32_t i;

	/* Create GCM session */
	retval = create_gcm_session(ts_params->valid_devs[0],
			RTE_CRYPTO_CIPHER_OP_DECRYPT,
			tdata->key.data, tdata->key.len,
			tdata->aad.len, tdata->auth_tag.len,
			RTE_CRYPTO_AUTH_OP_VERIFY);
	if (retval < 0)
		return retval;

	/* alloc mbuf and set payload */
	if (tdata->aad.len > MBUF_SIZE) {
		ut_params->ibuf = rte_pktmbuf_alloc(ts_params->large_mbuf_pool);
		/* Populate full size of add data */
		for (i = 32; i < GCM_MAX_AAD_LENGTH; i += 32)
			memcpy(&tdata->aad.data[i], &tdata->aad.data[0], 32);
	} else
		ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->ibuf));

	/* Create GCM operation */
	retval = create_gcm_operation(RTE_CRYPTO_CIPHER_OP_DECRYPT, tdata);
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
		plaintext = rte_pktmbuf_mtod_offset(ut_params->op->sym->m_src,
				uint8_t *,
				ut_params->op->sym->cipher.data.offset);

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
test_mb_AES_GCM_auth_decryption_test_case_256_1(void)
{
	return test_mb_AES_GCM_authenticated_decryption(&gcm_test_case_256_1);
}

static int
test_mb_AES_GCM_auth_decryption_test_case_256_2(void)
{
	return test_mb_AES_GCM_authenticated_decryption(&gcm_test_case_256_2);
}

static int
test_mb_AES_GCM_auth_decryption_test_case_256_3(void)
{
	return test_mb_AES_GCM_authenticated_decryption(&gcm_test_case_256_3);
}

static int
test_mb_AES_GCM_auth_decryption_test_case_256_4(void)
{
	return test_mb_AES_GCM_authenticated_decryption(&gcm_test_case_256_4);
}

static int
test_mb_AES_GCM_auth_decryption_test_case_256_5(void)
{
	return test_mb_AES_GCM_authenticated_decryption(&gcm_test_case_256_5);
}

static int
test_mb_AES_GCM_auth_decryption_test_case_256_6(void)
{
	return test_mb_AES_GCM_authenticated_decryption(&gcm_test_case_256_6);
}

static int
test_mb_AES_GCM_auth_decryption_test_case_256_7(void)
{
	return test_mb_AES_GCM_authenticated_decryption(&gcm_test_case_256_7);
}

static int
test_mb_AES_GCM_auth_decryption_test_case_aad_1(void)
{
	return test_mb_AES_GCM_authenticated_decryption(&gcm_test_case_aad_1);
}

static int
test_mb_AES_GCM_auth_decryption_test_case_aad_2(void)
{
	return test_mb_AES_GCM_authenticated_decryption(&gcm_test_case_aad_2);
}

static int
test_AES_GCM_authenticated_encryption_oop(const struct gcm_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;
	uint8_t *ciphertext, *auth_tag;
	uint16_t plaintext_pad_len;

	/* Create GCM session */
	retval = create_gcm_session(ts_params->valid_devs[0],
			RTE_CRYPTO_CIPHER_OP_ENCRYPT,
			tdata->key.data, tdata->key.len,
			tdata->aad.len, tdata->auth_tag.len,
			RTE_CRYPTO_AUTH_OP_GENERATE);
	if (retval < 0)
		return retval;

	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);
	ut_params->obuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	/* clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->ibuf));
	memset(rte_pktmbuf_mtod(ut_params->obuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->obuf));

	/* Create GCM operation */
	retval = create_gcm_operation(RTE_CRYPTO_CIPHER_OP_ENCRYPT, tdata);
	if (retval < 0)
		return retval;

	rte_crypto_op_attach_sym_session(ut_params->op, ut_params->sess);

	ut_params->op->sym->m_src = ut_params->ibuf;
	ut_params->op->sym->m_dst = ut_params->obuf;

	/* Process crypto operation */
	TEST_ASSERT_NOT_NULL(process_crypto_request(ts_params->valid_devs[0],
			ut_params->op), "failed to process sym crypto op");

	TEST_ASSERT_EQUAL(ut_params->op->status, RTE_CRYPTO_OP_STATUS_SUCCESS,
			"crypto op processing failed");

	plaintext_pad_len = RTE_ALIGN_CEIL(tdata->plaintext.len, 16);

	ciphertext = rte_pktmbuf_mtod_offset(ut_params->obuf, uint8_t *,
			ut_params->op->sym->cipher.data.offset);
	auth_tag = ciphertext + plaintext_pad_len;

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
test_mb_AES_GCM_authenticated_encryption_oop(void)
{
	return test_AES_GCM_authenticated_encryption_oop(&gcm_test_case_5);
}

static int
test_AES_GCM_authenticated_decryption_oop(const struct gcm_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;
	uint8_t *plaintext;

	/* Create GCM session */
	retval = create_gcm_session(ts_params->valid_devs[0],
			RTE_CRYPTO_CIPHER_OP_DECRYPT,
			tdata->key.data, tdata->key.len,
			tdata->aad.len, tdata->auth_tag.len,
			RTE_CRYPTO_AUTH_OP_VERIFY);
	if (retval < 0)
		return retval;

	/* alloc mbuf and set payload */
	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);
	ut_params->obuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->ibuf));
	memset(rte_pktmbuf_mtod(ut_params->obuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->obuf));

	/* Create GCM operation */
	retval = create_gcm_operation(RTE_CRYPTO_CIPHER_OP_DECRYPT, tdata);
	if (retval < 0)
		return retval;

	rte_crypto_op_attach_sym_session(ut_params->op, ut_params->sess);

	ut_params->op->sym->m_src = ut_params->ibuf;
	ut_params->op->sym->m_dst = ut_params->obuf;

	/* Process crypto operation */
	TEST_ASSERT_NOT_NULL(process_crypto_request(ts_params->valid_devs[0],
			ut_params->op), "failed to process sym crypto op");

	TEST_ASSERT_EQUAL(ut_params->op->status, RTE_CRYPTO_OP_STATUS_SUCCESS,
			"crypto op processing failed");

	plaintext = rte_pktmbuf_mtod_offset(ut_params->obuf, uint8_t *,
			ut_params->op->sym->cipher.data.offset);

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
test_mb_AES_GCM_authenticated_decryption_oop(void)
{
	return test_AES_GCM_authenticated_decryption_oop(&gcm_test_case_5);
}

static int
test_AES_GCM_authenticated_encryption_sessionless(
		const struct gcm_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;
	uint8_t *ciphertext, *auth_tag;
	uint16_t plaintext_pad_len;
	uint8_t key[tdata->key.len + 1];

	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	/* clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->ibuf));

	/* Create GCM operation */
	retval = create_gcm_operation(RTE_CRYPTO_CIPHER_OP_ENCRYPT, tdata);
	if (retval < 0)
		return retval;

	/* Create GCM xforms */
	memcpy(key, tdata->key.data, tdata->key.len);
	retval = create_gcm_xforms(ut_params->op,
			RTE_CRYPTO_CIPHER_OP_ENCRYPT,
			key, tdata->key.len,
			tdata->aad.len, tdata->auth_tag.len,
			RTE_CRYPTO_AUTH_OP_GENERATE);
	if (retval < 0)
		return retval;

	ut_params->op->sym->m_src = ut_params->ibuf;

	TEST_ASSERT_EQUAL(ut_params->op->sym->sess_type,
			RTE_CRYPTO_SYM_OP_SESSIONLESS,
			"crypto op session type not sessionless");

	/* Process crypto operation */
	TEST_ASSERT_NOT_NULL(process_crypto_request(ts_params->valid_devs[0],
			ut_params->op), "failed to process sym crypto op");

	TEST_ASSERT_NOT_NULL(ut_params->op, "failed crypto process");

	TEST_ASSERT_EQUAL(ut_params->op->status, RTE_CRYPTO_OP_STATUS_SUCCESS,
			"crypto op status not success");

	plaintext_pad_len = RTE_ALIGN_CEIL(tdata->plaintext.len, 16);

	ciphertext = rte_pktmbuf_mtod_offset(ut_params->ibuf, uint8_t *,
			ut_params->op->sym->cipher.data.offset);
	auth_tag = ciphertext + plaintext_pad_len;

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
test_mb_AES_GCM_authenticated_encryption_sessionless(void)
{
	return test_AES_GCM_authenticated_encryption_sessionless(
			&gcm_test_case_5);
}

static int
test_AES_GCM_authenticated_decryption_sessionless(
		const struct gcm_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;
	uint8_t *plaintext;
	uint8_t key[tdata->key.len + 1];

	/* alloc mbuf and set payload */
	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->ibuf));

	/* Create GCM operation */
	retval = create_gcm_operation(RTE_CRYPTO_CIPHER_OP_DECRYPT, tdata);
	if (retval < 0)
		return retval;

	/* Create GCM xforms */
	memcpy(key, tdata->key.data, tdata->key.len);
	retval = create_gcm_xforms(ut_params->op,
			RTE_CRYPTO_CIPHER_OP_DECRYPT,
			key, tdata->key.len,
			tdata->aad.len, tdata->auth_tag.len,
			RTE_CRYPTO_AUTH_OP_VERIFY);
	if (retval < 0)
		return retval;

	ut_params->op->sym->m_src = ut_params->ibuf;

	TEST_ASSERT_EQUAL(ut_params->op->sym->sess_type,
			RTE_CRYPTO_SYM_OP_SESSIONLESS,
			"crypto op session type not sessionless");

	/* Process crypto operation */
	TEST_ASSERT_NOT_NULL(process_crypto_request(ts_params->valid_devs[0],
			ut_params->op), "failed to process sym crypto op");

	TEST_ASSERT_NOT_NULL(ut_params->op, "failed crypto process");

	TEST_ASSERT_EQUAL(ut_params->op->status, RTE_CRYPTO_OP_STATUS_SUCCESS,
			"crypto op status not success");

	plaintext = rte_pktmbuf_mtod_offset(ut_params->ibuf, uint8_t *,
			ut_params->op->sym->cipher.data.offset);

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
test_mb_AES_GCM_authenticated_decryption_sessionless(void)
{
	return test_AES_GCM_authenticated_decryption_sessionless(
			&gcm_test_case_5);
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

static int MD5_HMAC_create_session(struct crypto_testsuite_params *ts_params,
				   struct crypto_unittest_params *ut_params,
				   enum rte_crypto_auth_operation op,
				   const struct HMAC_MD5_vector *test_case)
{
	uint8_t key[64];

	memcpy(key, test_case->key.data, test_case->key.len);

	ut_params->auth_xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	ut_params->auth_xform.next = NULL;
	ut_params->auth_xform.auth.op = op;

	ut_params->auth_xform.auth.algo = RTE_CRYPTO_AUTH_MD5_HMAC;

	ut_params->auth_xform.auth.digest_length = MD5_DIGEST_LEN;
	ut_params->auth_xform.auth.add_auth_data_length = 0;
	ut_params->auth_xform.auth.key.length = test_case->key.len;
	ut_params->auth_xform.auth.key.data = key;

	ut_params->sess = rte_cryptodev_sym_session_create(
		ts_params->valid_devs[0], &ut_params->auth_xform);

	if (ut_params->sess == NULL)
		return TEST_FAILED;

	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->ibuf));

	return 0;
}

static int MD5_HMAC_create_op(struct crypto_unittest_params *ut_params,
			      const struct HMAC_MD5_vector *test_case,
			      uint8_t **plaintext)
{
	uint16_t plaintext_pad_len;

	struct rte_crypto_sym_op *sym_op = ut_params->op->sym;

	plaintext_pad_len = RTE_ALIGN_CEIL(test_case->plaintext.len,
				16);

	*plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
			plaintext_pad_len);
	memcpy(*plaintext, test_case->plaintext.data,
			test_case->plaintext.len);

	sym_op->auth.digest.data = (uint8_t *)rte_pktmbuf_append(
			ut_params->ibuf, MD5_DIGEST_LEN);
	TEST_ASSERT_NOT_NULL(sym_op->auth.digest.data,
			"no room to append digest");
	sym_op->auth.digest.phys_addr = rte_pktmbuf_mtophys_offset(
			ut_params->ibuf, plaintext_pad_len);
	sym_op->auth.digest.length = MD5_DIGEST_LEN;

	if (ut_params->auth_xform.auth.op == RTE_CRYPTO_AUTH_OP_VERIFY) {
		rte_memcpy(sym_op->auth.digest.data, test_case->auth_tag.data,
			   test_case->auth_tag.len);
	}

	sym_op->auth.data.offset = 0;
	sym_op->auth.data.length = test_case->plaintext.len;

	rte_crypto_op_attach_sym_session(ut_params->op, ut_params->sess);
	ut_params->op->sym->m_src = ut_params->ibuf;

	return 0;
}

static int
test_MD5_HMAC_generate(const struct HMAC_MD5_vector *test_case)
{
	uint16_t plaintext_pad_len;
	uint8_t *plaintext, *auth_tag;

	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	if (MD5_HMAC_create_session(ts_params, ut_params,
			RTE_CRYPTO_AUTH_OP_GENERATE, test_case))
		return TEST_FAILED;

	/* Generate Crypto op data structure */
	ut_params->op = rte_crypto_op_alloc(ts_params->op_mpool,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	TEST_ASSERT_NOT_NULL(ut_params->op,
			"Failed to allocate symmetric crypto operation struct");

	plaintext_pad_len = RTE_ALIGN_CEIL(test_case->plaintext.len,
				16);

	if (MD5_HMAC_create_op(ut_params, test_case, &plaintext))
		return TEST_FAILED;

	TEST_ASSERT_NOT_NULL(process_crypto_request(ts_params->valid_devs[0],
			ut_params->op), "failed to process sym crypto op");

	TEST_ASSERT_EQUAL(ut_params->op->status, RTE_CRYPTO_OP_STATUS_SUCCESS,
			"crypto op processing failed");

	if (ut_params->op->sym->m_dst) {
		auth_tag = rte_pktmbuf_mtod_offset(ut_params->op->sym->m_dst,
				uint8_t *, plaintext_pad_len);
	} else {
		auth_tag = plaintext + plaintext_pad_len;
	}

	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			auth_tag,
			test_case->auth_tag.data,
			test_case->auth_tag.len,
			"HMAC_MD5 generated tag not as expected");

	return TEST_SUCCESS;
}

static int
test_MD5_HMAC_verify(const struct HMAC_MD5_vector *test_case)
{
	uint8_t *plaintext;

	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	if (MD5_HMAC_create_session(ts_params, ut_params,
			RTE_CRYPTO_AUTH_OP_VERIFY, test_case)) {
		return TEST_FAILED;
	}

	/* Generate Crypto op data structure */
	ut_params->op = rte_crypto_op_alloc(ts_params->op_mpool,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	TEST_ASSERT_NOT_NULL(ut_params->op,
			"Failed to allocate symmetric crypto operation struct");

	if (MD5_HMAC_create_op(ut_params, test_case, &plaintext))
		return TEST_FAILED;

	TEST_ASSERT_NOT_NULL(process_crypto_request(ts_params->valid_devs[0],
			ut_params->op), "failed to process sym crypto op");

	TEST_ASSERT_EQUAL(ut_params->op->status, RTE_CRYPTO_OP_STATUS_SUCCESS,
			"HMAC_MD5 crypto op processing failed");

	return TEST_SUCCESS;
}

static int
test_MD5_HMAC_generate_case_1(void)
{
	return test_MD5_HMAC_generate(&HMAC_MD5_test_case_1);
}

static int
test_MD5_HMAC_verify_case_1(void)
{
	return test_MD5_HMAC_verify(&HMAC_MD5_test_case_1);
}

static int
test_MD5_HMAC_generate_case_2(void)
{
	return test_MD5_HMAC_generate(&HMAC_MD5_test_case_2);
}

static int
test_MD5_HMAC_verify_case_2(void)
{
	return test_MD5_HMAC_verify(&HMAC_MD5_test_case_2);
}

static int
test_multi_session(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	struct rte_cryptodev_info dev_info;
	struct rte_cryptodev_sym_session **sessions;

	uint16_t i;

	test_AES_CBC_HMAC_SHA512_decrypt_create_session_params(ut_params,
			aes_cbc_key, hmac_sha512_key);


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
		TEST_ASSERT_SUCCESS( test_AES_CBC_HMAC_SHA512_decrypt_perform(
			sessions[i],
			ut_params,
			ts_params,
			catch_22_quote_2_512_bytes_AES_CBC_ciphertext,
			catch_22_quote_2_512_bytes_AES_CBC_HMAC_SHA512_digest,
			aes_cbc_iv),
			"Failed to perform decrypt on request number %u.", i);
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

struct multi_session_params {
	struct crypto_unittest_params ut_params;
	uint8_t *cipher_key;
	uint8_t *hmac_key;
	const uint8_t *cipher;
	const uint8_t *digest;
	uint8_t *iv;
};

#define MB_SESSION_NUMBER 3

static int
test_multi_session_random_usage(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct rte_cryptodev_info dev_info;
	struct rte_cryptodev_sym_session **sessions;
	uint32_t i, j;
	struct multi_session_params ut_paramz[] = {

		{
			.cipher_key = ms_aes_cbc_key0,
			.hmac_key = ms_hmac_key0,
			.cipher = ms_aes_cbc_cipher0,
			.digest = ms_hmac_digest0,
			.iv = ms_aes_cbc_iv0
		},
		{
			.cipher_key = ms_aes_cbc_key1,
			.hmac_key = ms_hmac_key1,
			.cipher = ms_aes_cbc_cipher1,
			.digest = ms_hmac_digest1,
			.iv = ms_aes_cbc_iv1
		},
		{
			.cipher_key = ms_aes_cbc_key2,
			.hmac_key = ms_hmac_key2,
			.cipher = ms_aes_cbc_cipher2,
			.digest = ms_hmac_digest2,
			.iv = ms_aes_cbc_iv2
		},

	};

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);

	sessions = rte_malloc(NULL,
			(sizeof(struct rte_cryptodev_sym_session *)
					* dev_info.sym.max_nb_sessions) + 1, 0);

	for (i = 0; i < MB_SESSION_NUMBER; i++) {
		rte_memcpy(&ut_paramz[i].ut_params, &testsuite_params,
				sizeof(struct crypto_unittest_params));

		test_AES_CBC_HMAC_SHA512_decrypt_create_session_params(
				&ut_paramz[i].ut_params,
				ut_paramz[i].cipher_key, ut_paramz[i].hmac_key);

		/* Create multiple crypto sessions*/
		sessions[i] = rte_cryptodev_sym_session_create(
				ts_params->valid_devs[0],
				&ut_paramz[i].ut_params.auth_xform);

		TEST_ASSERT_NOT_NULL(sessions[i],
				"Session creation failed at session number %u",
				i);

	}

	srand(time(NULL));
	for (i = 0; i < 40000; i++) {

		j = rand() % MB_SESSION_NUMBER;

		TEST_ASSERT_SUCCESS(
			test_AES_CBC_HMAC_SHA512_decrypt_perform(
					sessions[j],
					&ut_paramz[j].ut_params,
					ts_params, ut_paramz[j].cipher,
					ut_paramz[j].digest,
					ut_paramz[j].iv),
			"Failed to perform decrypt on request number %u.", i);

		if (ut_paramz[j].ut_params.op)
			rte_crypto_op_free(ut_paramz[j].ut_params.op);

		/*
		 * free mbuf - both obuf and ibuf are usually the same,
		 * so check if they point at the same address is necessary,
		 * to avoid freeing the mbuf twice.
		 */
		if (ut_paramz[j].ut_params.obuf) {
			rte_pktmbuf_free(ut_paramz[j].ut_params.obuf);
			if (ut_paramz[j].ut_params.ibuf
					== ut_paramz[j].ut_params.obuf)
				ut_paramz[j].ut_params.ibuf = 0;
			ut_paramz[j].ut_params.obuf = 0;
		}
		if (ut_paramz[j].ut_params.ibuf) {
			rte_pktmbuf_free(ut_paramz[j].ut_params.ibuf);
			ut_paramz[j].ut_params.ibuf = 0;
		}
	}

	for (i = 0; i < MB_SESSION_NUMBER; i++)
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

static void
generate_gmac_large_plaintext(uint8_t *data)
{
	uint16_t i;

	for (i = 32; i < GMAC_LARGE_PLAINTEXT_LENGTH; i += 32)
		memcpy(&data[i], &data[0], 32);
}

static int
create_gmac_operation(enum rte_crypto_auth_operation op,
		const struct gmac_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;
	struct rte_crypto_sym_op *sym_op;

	unsigned iv_pad_len;
	unsigned aad_pad_len;

	iv_pad_len = RTE_ALIGN_CEIL(tdata->iv.len, 16);
	aad_pad_len = RTE_ALIGN_CEIL(tdata->aad.len, 16);

	/*
	 * Runtime generate the large plain text instead of use hard code
	 * plain text vector. It is done to avoid create huge source file
	 * with the test vector.
	 */
	if (tdata->aad.len == GMAC_LARGE_PLAINTEXT_LENGTH)
		generate_gmac_large_plaintext(tdata->aad.data);

	/* Generate Crypto op data structure */
	ut_params->op = rte_crypto_op_alloc(ts_params->op_mpool,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	TEST_ASSERT_NOT_NULL(ut_params->op,
			"Failed to allocate symmetric crypto operation struct");

	sym_op = ut_params->op->sym;
	sym_op->auth.aad.data = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
			aad_pad_len);
	TEST_ASSERT_NOT_NULL(sym_op->auth.aad.data,
			"no room to append aad");

	sym_op->auth.aad.length = tdata->aad.len;
	sym_op->auth.aad.phys_addr =
			rte_pktmbuf_mtophys(ut_params->ibuf);
	memcpy(sym_op->auth.aad.data, tdata->aad.data, tdata->aad.len);

	sym_op->auth.digest.data = (uint8_t *)rte_pktmbuf_append(
			ut_params->ibuf, tdata->gmac_tag.len);
	TEST_ASSERT_NOT_NULL(sym_op->auth.digest.data,
			"no room to append digest");

	sym_op->auth.digest.phys_addr = rte_pktmbuf_mtophys_offset(
			ut_params->ibuf, aad_pad_len);
	sym_op->auth.digest.length = tdata->gmac_tag.len;

	if (op == RTE_CRYPTO_AUTH_OP_VERIFY) {
		rte_memcpy(sym_op->auth.digest.data, tdata->gmac_tag.data,
				tdata->gmac_tag.len);
		TEST_HEXDUMP(stdout, "digest:",
				sym_op->auth.digest.data,
				sym_op->auth.digest.length);
	}

	sym_op->cipher.iv.data = (uint8_t *)rte_pktmbuf_prepend(
			ut_params->ibuf, iv_pad_len);
	TEST_ASSERT_NOT_NULL(sym_op->cipher.iv.data, "no room to prepend iv");

	memset(sym_op->cipher.iv.data, 0, iv_pad_len);
	sym_op->cipher.iv.phys_addr = rte_pktmbuf_mtophys(ut_params->ibuf);
	sym_op->cipher.iv.length = tdata->iv.len;

	rte_memcpy(sym_op->cipher.iv.data, tdata->iv.data, tdata->iv.len);

	TEST_HEXDUMP(stdout, "iv:", sym_op->cipher.iv.data, iv_pad_len);

	sym_op->cipher.data.length = 0;
	sym_op->cipher.data.offset = 0;

	sym_op->auth.data.offset = 0;
	sym_op->auth.data.length = 0;

	return 0;
}

static int create_gmac_session(uint8_t dev_id,
		enum rte_crypto_cipher_operation op,
		const struct gmac_test_data *tdata,
		enum rte_crypto_auth_operation auth_op)
{
	uint8_t cipher_key[tdata->key.len];

	struct crypto_unittest_params *ut_params = &unittest_params;

	memcpy(cipher_key, tdata->key.data, tdata->key.len);

	/* For GMAC we setup cipher parameters */
	ut_params->cipher_xform.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	ut_params->cipher_xform.next = NULL;
	ut_params->cipher_xform.cipher.algo = RTE_CRYPTO_CIPHER_AES_GCM;
	ut_params->cipher_xform.cipher.op = op;
	ut_params->cipher_xform.cipher.key.data = cipher_key;
	ut_params->cipher_xform.cipher.key.length = tdata->key.len;

	ut_params->auth_xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	ut_params->auth_xform.next = NULL;

	ut_params->auth_xform.auth.algo = RTE_CRYPTO_AUTH_AES_GMAC;
	ut_params->auth_xform.auth.op = auth_op;
	ut_params->auth_xform.auth.digest_length = tdata->gmac_tag.len;
	ut_params->auth_xform.auth.add_auth_data_length = 0;
	ut_params->auth_xform.auth.key.length = 0;
	ut_params->auth_xform.auth.key.data = NULL;

	ut_params->cipher_xform.next = &ut_params->auth_xform;

	ut_params->sess = rte_cryptodev_sym_session_create(dev_id,
			&ut_params->cipher_xform);

	TEST_ASSERT_NOT_NULL(ut_params->sess, "Session creation failed");

	return 0;
}

static int
test_AES_GMAC_authentication(const struct gmac_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;

	uint8_t *auth_tag, *p;
	uint16_t aad_pad_len;

	TEST_ASSERT_NOT_EQUAL(tdata->gmac_tag.len, 0,
			      "No GMAC length in the source data");

	retval = create_gmac_session(ts_params->valid_devs[0],
			RTE_CRYPTO_CIPHER_OP_ENCRYPT,
			tdata, RTE_CRYPTO_AUTH_OP_GENERATE);

	if (retval < 0)
		return retval;

	if (tdata->aad.len > MBUF_SIZE)
		ut_params->ibuf = rte_pktmbuf_alloc(ts_params->large_mbuf_pool);
	else
		ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);
	TEST_ASSERT_NOT_NULL(ut_params->ibuf,
			"Failed to allocate input buffer in mempool");

	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->ibuf));

	aad_pad_len = RTE_ALIGN_CEIL(tdata->aad.len, 16);

	p = rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *);

	retval = create_gmac_operation(RTE_CRYPTO_AUTH_OP_GENERATE,
			tdata);

	if (retval < 0)
		return retval;

	rte_crypto_op_attach_sym_session(ut_params->op, ut_params->sess);

	ut_params->op->sym->m_src = ut_params->ibuf;

	TEST_ASSERT_NOT_NULL(process_crypto_request(ts_params->valid_devs[0],
			ut_params->op), "failed to process sym crypto op");

	TEST_ASSERT_EQUAL(ut_params->op->status, RTE_CRYPTO_OP_STATUS_SUCCESS,
			"crypto op processing failed");

	if (ut_params->op->sym->m_dst) {
		auth_tag = rte_pktmbuf_mtod_offset(ut_params->op->sym->m_dst,
				uint8_t *, aad_pad_len);
	} else {
		auth_tag = p + aad_pad_len;
	}

	TEST_HEXDUMP(stdout, "auth tag:", auth_tag, tdata->gmac_tag.len);

	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			auth_tag,
			tdata->gmac_tag.data,
			tdata->gmac_tag.len,
			"GMAC Generated auth tag not as expected");

	return 0;
}

static int
test_AES_GMAC_authentication_test_case_1(void)
{
	return test_AES_GMAC_authentication(&gmac_test_case_1);
}

static int
test_AES_GMAC_authentication_test_case_2(void)
{
	return test_AES_GMAC_authentication(&gmac_test_case_2);
}

static int
test_AES_GMAC_authentication_test_case_3(void)
{
	return test_AES_GMAC_authentication(&gmac_test_case_3);
}

static int
test_AES_GMAC_authentication_test_case_4(void)
{
	return test_AES_GMAC_authentication(&gmac_test_case_4);
}

static int
test_AES_GMAC_authentication_verify(const struct gmac_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;
	int retval;

	TEST_ASSERT_NOT_EQUAL(tdata->gmac_tag.len, 0,
			      "No GMAC length in the source data");

	retval = create_gmac_session(ts_params->valid_devs[0],
			RTE_CRYPTO_CIPHER_OP_DECRYPT,
			tdata, RTE_CRYPTO_AUTH_OP_VERIFY);

	if (retval < 0)
		return retval;

	if (tdata->aad.len > MBUF_SIZE)
		ut_params->ibuf = rte_pktmbuf_alloc(ts_params->large_mbuf_pool);
	else
		ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);
	TEST_ASSERT_NOT_NULL(ut_params->ibuf,
			"Failed to allocate input buffer in mempool");

	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->ibuf));

	retval = create_gmac_operation(RTE_CRYPTO_AUTH_OP_VERIFY,
			tdata);

	if (retval < 0)
		return retval;

	rte_crypto_op_attach_sym_session(ut_params->op, ut_params->sess);

	ut_params->op->sym->m_src = ut_params->ibuf;

	TEST_ASSERT_NOT_NULL(process_crypto_request(ts_params->valid_devs[0],
			ut_params->op), "failed to process sym crypto op");

	TEST_ASSERT_EQUAL(ut_params->op->status, RTE_CRYPTO_OP_STATUS_SUCCESS,
			"crypto op processing failed");

	return 0;

}

static int
test_AES_GMAC_authentication_verify_test_case_1(void)
{
	return test_AES_GMAC_authentication_verify(&gmac_test_case_1);
}

static int
test_AES_GMAC_authentication_verify_test_case_2(void)
{
	return test_AES_GMAC_authentication_verify(&gmac_test_case_2);
}

static int
test_AES_GMAC_authentication_verify_test_case_3(void)
{
	return test_AES_GMAC_authentication_verify(&gmac_test_case_3);
}

static int
test_AES_GMAC_authentication_verify_test_case_4(void)
{
	return test_AES_GMAC_authentication_verify(&gmac_test_case_4);
}

struct test_crypto_vector {
	enum rte_crypto_cipher_algorithm crypto_algo;

	struct {
		uint8_t data[64];
		unsigned int len;
	} cipher_key;

	struct {
		uint8_t data[64];
		unsigned int len;
	} iv;

	struct {
		const uint8_t *data;
		unsigned int len;
	} plaintext;

	struct {
		const uint8_t *data;
		unsigned int len;
	} ciphertext;

	enum rte_crypto_auth_algorithm auth_algo;

	struct {
		uint8_t data[128];
		unsigned int len;
	} auth_key;

	struct {
		const uint8_t *data;
		unsigned int len;
	} aad;

	struct {
		uint8_t data[128];
		unsigned int len;
	} digest;
};

static const struct test_crypto_vector
hmac_sha1_test_crypto_vector = {
	.auth_algo = RTE_CRYPTO_AUTH_SHA1_HMAC,
	.plaintext = {
		.data = plaintext_hash,
		.len = 512
	},
	.auth_key = {
		.data = {
			0xF8, 0x2A, 0xC7, 0x54, 0xDB, 0x96, 0x18, 0xAA,
			0xC3, 0xA1, 0x53, 0xF6, 0x1F, 0x17, 0x60, 0xBD,
			0xDE, 0xF4, 0xDE, 0xAD
		},
		.len = 20
	},
	.digest = {
		.data = {
			0xC4, 0xB7, 0x0E, 0x6B, 0xDE, 0xD1, 0xE7, 0x77,
			0x7E, 0x2E, 0x8F, 0xFC, 0x48, 0x39, 0x46, 0x17,
			0x3F, 0x91, 0x64, 0x59
		},
		.len = 20
	}
};

static const struct test_crypto_vector
aes128_gmac_test_vector = {
	.auth_algo = RTE_CRYPTO_AUTH_AES_GMAC,
	.crypto_algo = RTE_CRYPTO_CIPHER_AES_GCM,
	.aad = {
		.data = plaintext_hash,
		.len = 512
	},
	.iv = {
		.data = {
			0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
			0x08, 0x09, 0x0A, 0x0B
		},
		.len = 12
	},
	.cipher_key = {
		.data = {
			0x42, 0x1A, 0x7D, 0x3D, 0xF5, 0x82, 0x80, 0xF1,
			0xF1, 0x35, 0x5C, 0x3B, 0xDD, 0x9A, 0x65, 0xBA
		},
		.len = 16
	},
	.digest = {
		.data = {
			0xCA, 0x00, 0x99, 0x8B, 0x30, 0x7E, 0x74, 0x56,
			0x32, 0xA7, 0x87, 0xB5, 0xE9, 0xB2, 0x34, 0x5A
		},
		.len = 16
	}
};

static const struct test_crypto_vector
aes128cbc_hmac_sha1_test_vector = {
	.crypto_algo = RTE_CRYPTO_CIPHER_AES_CBC,
	.cipher_key = {
		.data = {
			0xE4, 0x23, 0x33, 0x8A, 0x35, 0x64, 0x61, 0xE2,
			0x49, 0x03, 0xDD, 0xC6, 0xB8, 0xCA, 0x55, 0x7A
		},
		.len = 16
	},
	.iv = {
		.data = {
			0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
			0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
		},
		.len = 16
	},
	.plaintext = {
		.data = plaintext_hash,
		.len = 512
	},
	.ciphertext = {
		.data = ciphertext512_aes128cbc,
		.len = 512
	},
	.auth_algo = RTE_CRYPTO_AUTH_SHA1_HMAC,
	.auth_key = {
		.data = {
			0xF8, 0x2A, 0xC7, 0x54, 0xDB, 0x96, 0x18, 0xAA,
			0xC3, 0xA1, 0x53, 0xF6, 0x1F, 0x17, 0x60, 0xBD,
			0xDE, 0xF4, 0xDE, 0xAD
		},
		.len = 20
	},
	.digest = {
		.data = {
			0x9A, 0x4F, 0x88, 0x1B, 0xB6, 0x8F, 0xD8, 0x60,
			0x42, 0x1A, 0x7D, 0x3D, 0xF5, 0x82, 0x80, 0xF1,
			0x18, 0x8C, 0x1D, 0x32
		},
		.len = 20
	}
};

static void
data_corruption(uint8_t *data)
{
	data[0] += 1;
}

static void
tag_corruption(uint8_t *data, unsigned int tag_offset)
{
	data[tag_offset] += 1;
}

static int
create_auth_session(struct crypto_unittest_params *ut_params,
		uint8_t dev_id,
		const struct test_crypto_vector *reference,
		enum rte_crypto_auth_operation auth_op)
{
	uint8_t auth_key[reference->auth_key.len + 1];

	memcpy(auth_key, reference->auth_key.data, reference->auth_key.len);

	/* Setup Authentication Parameters */
	ut_params->auth_xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	ut_params->auth_xform.auth.op = auth_op;
	ut_params->auth_xform.next = NULL;
	ut_params->auth_xform.auth.algo = reference->auth_algo;
	ut_params->auth_xform.auth.key.length = reference->auth_key.len;
	ut_params->auth_xform.auth.key.data = auth_key;
	ut_params->auth_xform.auth.digest_length = reference->digest.len;
	ut_params->auth_xform.auth.add_auth_data_length = reference->aad.len;

	/* Create Crypto session*/
	ut_params->sess = rte_cryptodev_sym_session_create(dev_id,
				&ut_params->auth_xform);

	TEST_ASSERT_NOT_NULL(ut_params->sess, "Session creation failed");

	return 0;
}

static int
create_auth_cipher_session(struct crypto_unittest_params *ut_params,
		uint8_t dev_id,
		const struct test_crypto_vector *reference,
		enum rte_crypto_auth_operation auth_op,
		enum rte_crypto_cipher_operation cipher_op)
{
	uint8_t cipher_key[reference->cipher_key.len + 1];
	uint8_t auth_key[reference->auth_key.len + 1];

	memcpy(cipher_key, reference->cipher_key.data,
			reference->cipher_key.len);
	memcpy(auth_key, reference->auth_key.data, reference->auth_key.len);

	/* Setup Authentication Parameters */
	ut_params->auth_xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	ut_params->auth_xform.auth.op = auth_op;
	ut_params->auth_xform.next = &ut_params->cipher_xform;
	ut_params->auth_xform.auth.algo = reference->auth_algo;
	ut_params->auth_xform.auth.key.length = reference->auth_key.len;
	ut_params->auth_xform.auth.key.data = auth_key;
	ut_params->auth_xform.auth.digest_length = reference->digest.len;
	ut_params->auth_xform.auth.add_auth_data_length = reference->aad.len;

	/* Setup Cipher Parameters */
	ut_params->cipher_xform.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	ut_params->cipher_xform.next = NULL;
	ut_params->cipher_xform.cipher.algo = reference->crypto_algo;
	ut_params->cipher_xform.cipher.op = cipher_op;
	ut_params->cipher_xform.cipher.key.data = cipher_key;
	ut_params->cipher_xform.cipher.key.length = reference->cipher_key.len;

	/* Create Crypto session*/
	ut_params->sess = rte_cryptodev_sym_session_create(dev_id,
				&ut_params->auth_xform);

	TEST_ASSERT_NOT_NULL(ut_params->sess, "Session creation failed");

	return 0;
}

static int
create_auth_operation(struct crypto_testsuite_params *ts_params,
		struct crypto_unittest_params *ut_params,
		const struct test_crypto_vector *reference,
		unsigned int auth_generate)
{
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
			ut_params->ibuf, reference->digest.len);

	TEST_ASSERT_NOT_NULL(sym_op->auth.digest.data,
			"no room to append auth tag");

	sym_op->auth.digest.phys_addr = rte_pktmbuf_mtophys_offset(
			ut_params->ibuf, reference->plaintext.len);
	sym_op->auth.digest.length = reference->digest.len;

	if (auth_generate)
		memset(sym_op->auth.digest.data, 0, reference->digest.len);
	else
		memcpy(sym_op->auth.digest.data,
				reference->digest.data,
				reference->digest.len);

	TEST_HEXDUMP(stdout, "digest:",
			sym_op->auth.digest.data,
			sym_op->auth.digest.length);

	sym_op->auth.data.length = reference->plaintext.len;
	sym_op->auth.data.offset = 0;

	return 0;
}

static int
create_auth_GMAC_operation(struct crypto_testsuite_params *ts_params,
		struct crypto_unittest_params *ut_params,
		const struct test_crypto_vector *reference,
		unsigned int auth_generate)
{
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
	sym_op->auth.aad.data = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
			reference->aad.len);
	TEST_ASSERT_NOT_NULL(sym_op->auth.aad.data, "no room to append AAD");
	memcpy(sym_op->auth.aad.data, reference->aad.data, reference->aad.len);

	TEST_HEXDUMP(stdout, "AAD:", sym_op->auth.aad.data, reference->aad.len);

	sym_op->auth.aad.phys_addr = rte_pktmbuf_mtophys(ut_params->ibuf);
	sym_op->auth.aad.length = reference->aad.len;

	/* digest */
	sym_op->auth.digest.data = (uint8_t *)rte_pktmbuf_append(
			ut_params->ibuf, reference->digest.len);

	TEST_ASSERT_NOT_NULL(sym_op->auth.digest.data,
			"no room to append auth tag");

	sym_op->auth.digest.phys_addr = rte_pktmbuf_mtophys_offset(
			ut_params->ibuf, reference->ciphertext.len);
	sym_op->auth.digest.length = reference->digest.len;

	if (auth_generate)
		memset(sym_op->auth.digest.data, 0, reference->digest.len);
	else
		memcpy(sym_op->auth.digest.data,
				reference->digest.data,
				reference->digest.len);

	TEST_HEXDUMP(stdout, "digest:",
			sym_op->auth.digest.data,
			sym_op->auth.digest.length);

	sym_op->cipher.iv.data = (uint8_t *)rte_pktmbuf_prepend(
		ut_params->ibuf, reference->iv.len);
	TEST_ASSERT_NOT_NULL(sym_op->cipher.iv.data, "no room to prepend iv");

	sym_op->cipher.iv.phys_addr = rte_pktmbuf_mtophys(ut_params->ibuf);
	sym_op->cipher.iv.length = reference->iv.len;

	memcpy(sym_op->cipher.iv.data, reference->iv.data, reference->iv.len);

	sym_op->cipher.data.length = 0;
	sym_op->cipher.data.offset = 0;

	sym_op->auth.data.length = 0;
	sym_op->auth.data.offset = 0;

	return 0;
}

static int
create_cipher_auth_operation(struct crypto_testsuite_params *ts_params,
		struct crypto_unittest_params *ut_params,
		const struct test_crypto_vector *reference,
		unsigned int auth_generate)
{
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
			ut_params->ibuf, reference->digest.len);

	TEST_ASSERT_NOT_NULL(sym_op->auth.digest.data,
			"no room to append auth tag");

	sym_op->auth.digest.phys_addr = rte_pktmbuf_mtophys_offset(
			ut_params->ibuf, reference->ciphertext.len);
	sym_op->auth.digest.length = reference->digest.len;

	if (auth_generate)
		memset(sym_op->auth.digest.data, 0, reference->digest.len);
	else
		memcpy(sym_op->auth.digest.data,
				reference->digest.data,
				reference->digest.len);

	TEST_HEXDUMP(stdout, "digest:",
			sym_op->auth.digest.data,
			sym_op->auth.digest.length);

	sym_op->cipher.iv.data = (uint8_t *)rte_pktmbuf_prepend(
		ut_params->ibuf, reference->iv.len);
	TEST_ASSERT_NOT_NULL(sym_op->cipher.iv.data, "no room to prepend iv");

	sym_op->cipher.iv.phys_addr = rte_pktmbuf_mtophys(ut_params->ibuf);
	sym_op->cipher.iv.length = reference->iv.len;

	memcpy(sym_op->cipher.iv.data, reference->iv.data, reference->iv.len);

	sym_op->cipher.data.length = reference->ciphertext.len;
	sym_op->cipher.data.offset = reference->iv.len;

	sym_op->auth.data.length = reference->ciphertext.len;
	sym_op->auth.data.offset = reference->iv.len;

	return 0;
}

static int
create_auth_verify_operation(struct crypto_testsuite_params *ts_params,
		struct crypto_unittest_params *ut_params,
		const struct test_crypto_vector *reference)
{
	return create_auth_operation(ts_params, ut_params, reference, 0);
}

static int
create_auth_verify_GMAC_operation(
		struct crypto_testsuite_params *ts_params,
		struct crypto_unittest_params *ut_params,
		const struct test_crypto_vector *reference)
{
	return create_auth_GMAC_operation(ts_params, ut_params, reference, 0);
}

static int
create_cipher_auth_verify_operation(struct crypto_testsuite_params *ts_params,
		struct crypto_unittest_params *ut_params,
		const struct test_crypto_vector *reference)
{
	return create_cipher_auth_operation(ts_params, ut_params, reference, 0);
}

static int
test_authentication_verify_fail_when_data_corruption(
		struct crypto_testsuite_params *ts_params,
		struct crypto_unittest_params *ut_params,
		const struct test_crypto_vector *reference,
		unsigned int data_corrupted)
{
	int retval;

	uint8_t *plaintext;

	/* Create session */
	retval = create_auth_session(ut_params,
			ts_params->valid_devs[0],
			reference,
			RTE_CRYPTO_AUTH_OP_VERIFY);
	if (retval < 0)
		return retval;

	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);
	TEST_ASSERT_NOT_NULL(ut_params->ibuf,
			"Failed to allocate input buffer in mempool");

	/* clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->ibuf));

	plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
			reference->plaintext.len);
	TEST_ASSERT_NOT_NULL(plaintext, "no room to append plaintext");
	memcpy(plaintext, reference->plaintext.data, reference->plaintext.len);

	TEST_HEXDUMP(stdout, "plaintext:", plaintext, reference->plaintext.len);

	/* Create operation */
	retval = create_auth_verify_operation(ts_params, ut_params, reference);

	if (retval < 0)
		return retval;

	if (data_corrupted)
		data_corruption(plaintext);
	else
		tag_corruption(plaintext, reference->plaintext.len);

	ut_params->op = process_crypto_request(ts_params->valid_devs[0],
			ut_params->op);
	TEST_ASSERT_NOT_NULL(ut_params->op, "failed crypto process");
	TEST_ASSERT_EQUAL(ut_params->op->status,
			RTE_CRYPTO_OP_STATUS_AUTH_FAILED,
			"authentication not failed");

	ut_params->obuf = ut_params->op->sym->m_src;
	TEST_ASSERT_NOT_NULL(ut_params->obuf, "failed to retrieve obuf");

	return 0;
}

static int
test_authentication_verify_GMAC_fail_when_corruption(
		struct crypto_testsuite_params *ts_params,
		struct crypto_unittest_params *ut_params,
		const struct test_crypto_vector *reference,
		unsigned int data_corrupted)
{
	int retval;

	/* Create session */
	retval = create_auth_cipher_session(ut_params,
			ts_params->valid_devs[0],
			reference,
			RTE_CRYPTO_AUTH_OP_VERIFY,
			RTE_CRYPTO_CIPHER_OP_DECRYPT);
	if (retval < 0)
		return retval;

	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);
	TEST_ASSERT_NOT_NULL(ut_params->ibuf,
			"Failed to allocate input buffer in mempool");

	/* clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->ibuf));

	/* Create operation */
	retval = create_auth_verify_GMAC_operation(ts_params,
			ut_params,
			reference);

	if (retval < 0)
		return retval;

	if (data_corrupted)
		data_corruption(ut_params->op->sym->auth.aad.data);
	else
		tag_corruption(ut_params->op->sym->auth.aad.data,
				reference->aad.len);

	ut_params->op = process_crypto_request(ts_params->valid_devs[0],
			ut_params->op);
	TEST_ASSERT_NOT_NULL(ut_params->op, "failed crypto process");
	TEST_ASSERT_EQUAL(ut_params->op->status,
			RTE_CRYPTO_OP_STATUS_AUTH_FAILED,
			"authentication not failed");

	ut_params->obuf = ut_params->op->sym->m_src;
	TEST_ASSERT_NOT_NULL(ut_params->obuf, "failed to retrieve obuf");

	return 0;
}

static int
test_authenticated_decryption_fail_when_corruption(
		struct crypto_testsuite_params *ts_params,
		struct crypto_unittest_params *ut_params,
		const struct test_crypto_vector *reference,
		unsigned int data_corrupted)
{
	int retval;

	uint8_t *ciphertext;

	/* Create session */
	retval = create_auth_cipher_session(ut_params,
			ts_params->valid_devs[0],
			reference,
			RTE_CRYPTO_AUTH_OP_VERIFY,
			RTE_CRYPTO_CIPHER_OP_DECRYPT);
	if (retval < 0)
		return retval;

	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);
	TEST_ASSERT_NOT_NULL(ut_params->ibuf,
			"Failed to allocate input buffer in mempool");

	/* clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->ibuf));

	ciphertext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
			reference->ciphertext.len);
	TEST_ASSERT_NOT_NULL(ciphertext, "no room to append ciphertext");
	memcpy(ciphertext, reference->ciphertext.data,
			reference->ciphertext.len);

	/* Create operation */
	retval = create_cipher_auth_verify_operation(ts_params,
			ut_params,
			reference);

	if (retval < 0)
		return retval;

	if (data_corrupted)
		data_corruption(ciphertext);
	else
		tag_corruption(ciphertext, reference->ciphertext.len);

	ut_params->op = process_crypto_request(ts_params->valid_devs[0],
			ut_params->op);

	TEST_ASSERT_NOT_NULL(ut_params->op, "failed crypto process");
	TEST_ASSERT_EQUAL(ut_params->op->status,
			RTE_CRYPTO_OP_STATUS_AUTH_FAILED,
			"authentication not failed");

	ut_params->obuf = ut_params->op->sym->m_src;
	TEST_ASSERT_NOT_NULL(ut_params->obuf, "failed to retrieve obuf");

	return 0;
}

static int
create_gcm_operation_SGL(enum rte_crypto_cipher_operation op,
		const struct gcm_test_data *tdata,
		void *digest_mem, uint64_t digest_phys)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	const unsigned int auth_tag_len = tdata->auth_tag.len;
	const unsigned int iv_len = tdata->iv.len;
	const unsigned int aad_len = tdata->aad.len;

	unsigned int iv_pad_len = 0;

	/* Generate Crypto op data structure */
	ut_params->op = rte_crypto_op_alloc(ts_params->op_mpool,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	TEST_ASSERT_NOT_NULL(ut_params->op,
		"Failed to allocate symmetric crypto operation struct");

	struct rte_crypto_sym_op *sym_op = ut_params->op->sym;

	sym_op->auth.digest.data = digest_mem;

	TEST_ASSERT_NOT_NULL(sym_op->auth.digest.data,
			"no room to append digest");

	sym_op->auth.digest.phys_addr = digest_phys;
	sym_op->auth.digest.length = auth_tag_len;

	if (op == RTE_CRYPTO_CIPHER_OP_DECRYPT) {
		rte_memcpy(sym_op->auth.digest.data, tdata->auth_tag.data,
				auth_tag_len);
		TEST_HEXDUMP(stdout, "digest:",
				sym_op->auth.digest.data,
				sym_op->auth.digest.length);
	}

	iv_pad_len = RTE_ALIGN_CEIL(iv_len, 16);

	sym_op->cipher.iv.data = (uint8_t *)rte_pktmbuf_prepend(
			ut_params->ibuf, iv_pad_len);

	TEST_ASSERT_NOT_NULL(sym_op->cipher.iv.data,
			"no room to prepend iv");

	memset(sym_op->cipher.iv.data, 0, iv_pad_len);
	sym_op->cipher.iv.phys_addr = rte_pktmbuf_mtophys(ut_params->ibuf);
	sym_op->cipher.iv.length = iv_len;

	rte_memcpy(sym_op->cipher.iv.data, tdata->iv.data, iv_pad_len);

	sym_op->auth.aad.data = (uint8_t *)rte_pktmbuf_prepend(
			ut_params->ibuf, aad_len);
	TEST_ASSERT_NOT_NULL(sym_op->auth.aad.data,
			"no room to prepend aad");
	sym_op->auth.aad.phys_addr = rte_pktmbuf_mtophys(
			ut_params->ibuf);
	sym_op->auth.aad.length = aad_len;

	memset(sym_op->auth.aad.data, 0, aad_len);
	rte_memcpy(sym_op->auth.aad.data, tdata->aad.data, aad_len);

	TEST_HEXDUMP(stdout, "iv:", sym_op->cipher.iv.data, iv_pad_len);
	TEST_HEXDUMP(stdout, "aad:",
			sym_op->auth.aad.data, aad_len);

	sym_op->cipher.data.length = tdata->plaintext.len;
	sym_op->cipher.data.offset = aad_len + iv_pad_len;

	sym_op->auth.data.offset = aad_len + iv_pad_len;
	sym_op->auth.data.length = tdata->plaintext.len;

	return 0;
}

#define SGL_MAX_NO	16

static int
test_AES_GCM_authenticated_encryption_SGL(const struct gcm_test_data *tdata,
		const int oop, uint32_t fragsz, uint32_t fragsz_oop)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;
	struct rte_mbuf *buf, *buf_oop = NULL, *buf_last_oop = NULL;
	int retval;
	int to_trn = 0;
	int to_trn_tbl[SGL_MAX_NO];
	int segs = 1;
	unsigned int trn_data = 0;
	uint8_t *plaintext, *ciphertext, *auth_tag;

	if (fragsz > tdata->plaintext.len)
		fragsz = tdata->plaintext.len;

	uint16_t plaintext_len = fragsz;
	uint16_t frag_size_oop = fragsz_oop ? fragsz_oop : fragsz;

	if (fragsz_oop > tdata->plaintext.len)
		frag_size_oop = tdata->plaintext.len;

	int ecx = 0;
	void *digest_mem = NULL;

	uint32_t prepend_len = ALIGN_POW2_ROUNDUP(tdata->iv.len, 16)
			+ tdata->aad.len;

	if (tdata->plaintext.len % fragsz != 0) {
		if (tdata->plaintext.len / fragsz + 1 > SGL_MAX_NO)
			return 1;
	}	else {
		if (tdata->plaintext.len / fragsz > SGL_MAX_NO)
			return 1;
	}

	/*
	 * For out-op-place we need to alloc another mbuf
	 */
	if (oop) {
		ut_params->obuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);
		rte_pktmbuf_append(ut_params->obuf,
				frag_size_oop + prepend_len);
		buf_oop = ut_params->obuf;
	}

	/* Create GCM session */
	retval = create_gcm_session(ts_params->valid_devs[0],
			RTE_CRYPTO_CIPHER_OP_ENCRYPT,
			tdata->key.data, tdata->key.len,
			tdata->aad.len, tdata->auth_tag.len,
			RTE_CRYPTO_AUTH_OP_GENERATE);
	if (retval < 0)
		return retval;

	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	/* clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->ibuf));

	plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
			plaintext_len);

	memcpy(plaintext, tdata->plaintext.data, plaintext_len);

	trn_data += plaintext_len;

	buf = ut_params->ibuf;

	/*
	 * Loop until no more fragments
	 */

	while (trn_data < tdata->plaintext.len) {
		++segs;
		to_trn = (tdata->plaintext.len - trn_data < fragsz) ?
				(tdata->plaintext.len - trn_data) : fragsz;

		to_trn_tbl[ecx++] = to_trn;

		buf->next = rte_pktmbuf_alloc(ts_params->mbuf_pool);
		buf = buf->next;

		memset(rte_pktmbuf_mtod(buf, uint8_t *), 0,
				rte_pktmbuf_tailroom(buf));

		/* OOP */
		if (oop && !fragsz_oop) {
			buf_last_oop = buf_oop->next =
					rte_pktmbuf_alloc(ts_params->mbuf_pool);
			buf_oop = buf_oop->next;
			memset(rte_pktmbuf_mtod(buf_oop, uint8_t *),
					0, rte_pktmbuf_tailroom(buf_oop));
			rte_pktmbuf_append(buf_oop, to_trn);
		}

		plaintext = (uint8_t *)rte_pktmbuf_append(buf,
				to_trn);

		memcpy(plaintext, tdata->plaintext.data + trn_data,
				to_trn);
		trn_data += to_trn;
		if (trn_data  == tdata->plaintext.len) {
			if (oop) {
				if (!fragsz_oop)
					digest_mem = rte_pktmbuf_append(buf_oop,
						tdata->auth_tag.len);
			} else
				digest_mem = (uint8_t *)rte_pktmbuf_append(buf,
					tdata->auth_tag.len);
		}
	}

	uint64_t digest_phys = 0;

	ut_params->ibuf->nb_segs = segs;

	segs = 1;
	if (fragsz_oop && oop) {
		to_trn = 0;
		ecx = 0;

		if (frag_size_oop == tdata->plaintext.len) {
			digest_mem = rte_pktmbuf_append(ut_params->obuf,
				tdata->auth_tag.len);

			digest_phys = rte_pktmbuf_mtophys_offset(
					ut_params->obuf,
					tdata->plaintext.len + prepend_len);
		}

		trn_data = frag_size_oop;
		while (trn_data < tdata->plaintext.len) {
			++segs;
			to_trn =
				(tdata->plaintext.len - trn_data <
						frag_size_oop) ?
				(tdata->plaintext.len - trn_data) :
						frag_size_oop;

			to_trn_tbl[ecx++] = to_trn;

			buf_last_oop = buf_oop->next =
					rte_pktmbuf_alloc(ts_params->mbuf_pool);
			buf_oop = buf_oop->next;
			memset(rte_pktmbuf_mtod(buf_oop, uint8_t *),
					0, rte_pktmbuf_tailroom(buf_oop));
			rte_pktmbuf_append(buf_oop, to_trn);

			trn_data += to_trn;

			if (trn_data  == tdata->plaintext.len) {
				digest_mem = rte_pktmbuf_append(buf_oop,
					tdata->auth_tag.len);
			}
		}

		ut_params->obuf->nb_segs = segs;
	}

	/*
	 * Place digest at the end of the last buffer
	 */
	if (!digest_phys)
		digest_phys = rte_pktmbuf_mtophys(buf) + to_trn;
	if (oop && buf_last_oop)
		digest_phys = rte_pktmbuf_mtophys(buf_last_oop) + to_trn;

	if (!digest_mem && !oop) {
		digest_mem = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				+ tdata->auth_tag.len);
		digest_phys = rte_pktmbuf_mtophys_offset(ut_params->ibuf,
				tdata->plaintext.len);
	}

	/* Create GCM opertaion */
	retval = create_gcm_operation_SGL(RTE_CRYPTO_CIPHER_OP_ENCRYPT,
			tdata, digest_mem, digest_phys);

	if (retval < 0)
		return retval;

	rte_crypto_op_attach_sym_session(ut_params->op, ut_params->sess);

	ut_params->op->sym->m_src = ut_params->ibuf;
	if (oop)
		ut_params->op->sym->m_dst = ut_params->obuf;

	/* Process crypto operation */
	TEST_ASSERT_NOT_NULL(process_crypto_request(ts_params->valid_devs[0],
			ut_params->op), "failed to process sym crypto op");

	TEST_ASSERT_EQUAL(ut_params->op->status, RTE_CRYPTO_OP_STATUS_SUCCESS,
			"crypto op processing failed");


	ciphertext = rte_pktmbuf_mtod_offset(ut_params->op->sym->m_src,
			uint8_t *, prepend_len);
	if (oop) {
		ciphertext = rte_pktmbuf_mtod_offset(ut_params->op->sym->m_dst,
				uint8_t *, prepend_len);
	}

	if (fragsz_oop)
		fragsz = fragsz_oop;

	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			ciphertext,
			tdata->ciphertext.data,
			fragsz,
			"GCM Ciphertext data not as expected");

	buf = ut_params->op->sym->m_src->next;
	if (oop)
		buf = ut_params->op->sym->m_dst->next;

	unsigned int off = fragsz;

	ecx = 0;
	while (buf) {
		ciphertext = rte_pktmbuf_mtod(buf,
				uint8_t *);

		TEST_ASSERT_BUFFERS_ARE_EQUAL(
				ciphertext,
				tdata->ciphertext.data + off,
				to_trn_tbl[ecx],
				"GCM Ciphertext data not as expected");

		off += to_trn_tbl[ecx++];
		buf = buf->next;
	}

	auth_tag = digest_mem;
	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			auth_tag,
			tdata->auth_tag.data,
			tdata->auth_tag.len,
			"GCM Generated auth tag not as expected");

	return 0;
}

#define IN_PLACE	0
#define OUT_OF_PLACE	1

static int
test_AES_GCM_auth_encrypt_SGL_out_of_place_400B_400B(void)
{
	return test_AES_GCM_authenticated_encryption_SGL(
			&gcm_test_case_SGL_1, OUT_OF_PLACE, 400, 400);
}

static int
test_AES_GCM_auth_encrypt_SGL_out_of_place_1500B_2000B(void)
{
	return test_AES_GCM_authenticated_encryption_SGL(
			&gcm_test_case_SGL_1, OUT_OF_PLACE, 1500, 2000);
}

static int
test_AES_GCM_auth_encrypt_SGL_out_of_place_400B_1seg(void)
{
	return test_AES_GCM_authenticated_encryption_SGL(
			&gcm_test_case_8, OUT_OF_PLACE, 400,
			gcm_test_case_8.plaintext.len);
}

static int
test_AES_GCM_auth_encrypt_SGL_in_place_1500B(void)
{

	return test_AES_GCM_authenticated_encryption_SGL(
			&gcm_test_case_SGL_1, IN_PLACE, 1500, 0);
}

static int
test_authentication_verify_fail_when_data_corrupted(
		struct crypto_testsuite_params *ts_params,
		struct crypto_unittest_params *ut_params,
		const struct test_crypto_vector *reference)
{
	return test_authentication_verify_fail_when_data_corruption(
			ts_params, ut_params, reference, 1);
}

static int
test_authentication_verify_fail_when_tag_corrupted(
		struct crypto_testsuite_params *ts_params,
		struct crypto_unittest_params *ut_params,
		const struct test_crypto_vector *reference)
{
	return test_authentication_verify_fail_when_data_corruption(
			ts_params, ut_params, reference, 0);
}

static int
test_authentication_verify_GMAC_fail_when_data_corrupted(
		struct crypto_testsuite_params *ts_params,
		struct crypto_unittest_params *ut_params,
		const struct test_crypto_vector *reference)
{
	return test_authentication_verify_GMAC_fail_when_corruption(
			ts_params, ut_params, reference, 1);
}

static int
test_authentication_verify_GMAC_fail_when_tag_corrupted(
		struct crypto_testsuite_params *ts_params,
		struct crypto_unittest_params *ut_params,
		const struct test_crypto_vector *reference)
{
	return test_authentication_verify_GMAC_fail_when_corruption(
			ts_params, ut_params, reference, 0);
}

static int
test_authenticated_decryption_fail_when_data_corrupted(
		struct crypto_testsuite_params *ts_params,
		struct crypto_unittest_params *ut_params,
		const struct test_crypto_vector *reference)
{
	return test_authenticated_decryption_fail_when_corruption(
			ts_params, ut_params, reference, 1);
}

static int
test_authenticated_decryption_fail_when_tag_corrupted(
		struct crypto_testsuite_params *ts_params,
		struct crypto_unittest_params *ut_params,
		const struct test_crypto_vector *reference)
{
	return test_authenticated_decryption_fail_when_corruption(
			ts_params, ut_params, reference, 0);
}

static int
authentication_verify_HMAC_SHA1_fail_data_corrupt(void)
{
	return test_authentication_verify_fail_when_data_corrupted(
			&testsuite_params, &unittest_params,
			&hmac_sha1_test_crypto_vector);
}

static int
authentication_verify_HMAC_SHA1_fail_tag_corrupt(void)
{
	return test_authentication_verify_fail_when_tag_corrupted(
			&testsuite_params, &unittest_params,
			&hmac_sha1_test_crypto_vector);
}

static int
authentication_verify_AES128_GMAC_fail_data_corrupt(void)
{
	return test_authentication_verify_GMAC_fail_when_data_corrupted(
			&testsuite_params, &unittest_params,
			&aes128_gmac_test_vector);
}

static int
authentication_verify_AES128_GMAC_fail_tag_corrupt(void)
{
	return test_authentication_verify_GMAC_fail_when_tag_corrupted(
			&testsuite_params, &unittest_params,
			&aes128_gmac_test_vector);
}

static int
auth_decryption_AES128CBC_HMAC_SHA1_fail_data_corrupt(void)
{
	return test_authenticated_decryption_fail_when_data_corrupted(
			&testsuite_params,
			&unittest_params,
			&aes128cbc_hmac_sha1_test_vector);
}

static int
auth_decryption_AES128CBC_HMAC_SHA1_fail_tag_corrupt(void)
{
	return test_authenticated_decryption_fail_when_tag_corrupted(
			&testsuite_params,
			&unittest_params,
			&aes128cbc_hmac_sha1_test_vector);
}

#ifdef RTE_LIBRTE_PMD_CRYPTO_SCHEDULER

/* global AESNI slave IDs for the scheduler test */
uint8_t aesni_ids[2];

static int
test_scheduler_attach_slave_op(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	uint8_t sched_id = ts_params->valid_devs[0];
	uint32_t nb_devs, i, nb_devs_attached = 0;
	int ret;
	char vdev_name[32];

	/* create 2 AESNI_MB if necessary */
	nb_devs = rte_cryptodev_count_devtype(
			RTE_CRYPTODEV_AESNI_MB_PMD);
	if (nb_devs < 2) {
		for (i = nb_devs; i < 2; i++) {
			snprintf(vdev_name, sizeof(vdev_name), "%s_%u",
					RTE_STR(CRYPTODEV_NAME_AESNI_MB_PMD),
					i);
			ret = rte_vdev_init(vdev_name, NULL);

			TEST_ASSERT(ret == 0,
				"Failed to create instance %u of"
				" pmd : %s",
				i, RTE_STR(CRYPTODEV_NAME_AESNI_MB_PMD));
		}
	}

	/* attach 2 AESNI_MB cdevs */
	for (i = 0; i < rte_cryptodev_count() && nb_devs_attached < 2;
			i++) {
		struct rte_cryptodev_info info;

		rte_cryptodev_info_get(i, &info);
		if (info.dev_type != RTE_CRYPTODEV_AESNI_MB_PMD)
			continue;

		ret = rte_cryptodev_scheduler_slave_attach(sched_id,
				(uint8_t)i);

		TEST_ASSERT(ret == 0,
			"Failed to attach device %u of pmd : %s", i,
			RTE_STR(CRYPTODEV_NAME_AESNI_MB_PMD));

		aesni_ids[nb_devs_attached] = (uint8_t)i;

		nb_devs_attached++;
	}

	return 0;
}

static int
test_scheduler_detach_slave_op(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	uint8_t sched_id = ts_params->valid_devs[0];
	uint32_t i;
	int ret;

	for (i = 0; i < 2; i++) {
		ret = rte_cryptodev_scheduler_slave_detach(sched_id,
				aesni_ids[i]);
		TEST_ASSERT(ret == 0,
			"Failed to detach device %u", aesni_ids[i]);
	}

	return 0;
}

static int
test_scheduler_mode_op(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	uint8_t sched_id = ts_params->valid_devs[0];
	struct rte_cryptodev_scheduler_ops op = {0};
	struct rte_cryptodev_scheduler dummy_scheduler = {
		.description = "dummy scheduler to test mode",
		.name = "dummy scheduler",
		.mode = CDEV_SCHED_MODE_USERDEFINED,
		.ops = &op
	};
	int ret;

	/* set user defined mode */
	ret = rte_cryptodev_scheduler_load_user_scheduler(sched_id,
			&dummy_scheduler);
	TEST_ASSERT(ret == 0,
		"Failed to set cdev %u to user defined mode", sched_id);

	/* set round robin mode */
	ret = rte_cryptodev_scheduler_mode_set(sched_id,
			CDEV_SCHED_MODE_ROUNDROBIN);
	TEST_ASSERT(ret == 0,
		"Failed to set cdev %u to round-robin mode", sched_id);
	TEST_ASSERT(rte_cryptodev_scheduler_mode_get(sched_id) ==
			CDEV_SCHED_MODE_ROUNDROBIN, "Scheduling Mode "
					"not match");

	return 0;
}

static struct unit_test_suite cryptodev_scheduler_testsuite  = {
	.suite_name = "Crypto Device Scheduler Unit Test Suite",
	.setup = testsuite_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {
		TEST_CASE_ST(NULL, NULL, test_scheduler_attach_slave_op),
		TEST_CASE_ST(NULL, NULL, test_scheduler_mode_op),
		TEST_CASE_ST(ut_setup, ut_teardown,
				test_AES_chain_scheduler_all),
		TEST_CASE_ST(ut_setup, ut_teardown,
				test_AES_cipheronly_scheduler_all),
		TEST_CASE_ST(ut_setup, ut_teardown,
				test_authonly_scheduler_all),
		TEST_CASE_ST(NULL, NULL, test_scheduler_detach_slave_op),
		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

#endif /* RTE_LIBRTE_PMD_CRYPTO_SCHEDULER */

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

		TEST_CASE_ST(ut_setup, ut_teardown, test_AES_chain_qat_all),
		TEST_CASE_ST(ut_setup, ut_teardown,
						test_AES_cipheronly_qat_all),
		TEST_CASE_ST(ut_setup, ut_teardown, test_3DES_chain_qat_all),
		TEST_CASE_ST(ut_setup, ut_teardown,
						test_3DES_cipheronly_qat_all),
		TEST_CASE_ST(ut_setup, ut_teardown,
						test_DES_cipheronly_qat_all),
		TEST_CASE_ST(ut_setup, ut_teardown,
						test_AES_docsis_qat_all),
		TEST_CASE_ST(ut_setup, ut_teardown,
						test_DES_docsis_qat_all),
		TEST_CASE_ST(ut_setup, ut_teardown, test_stats),

		/** AES GCM Authenticated Encryption */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_encrypt_SGL_in_place_1500B),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_encrypt_SGL_out_of_place_400B_400B),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_encrypt_SGL_out_of_place_1500B_2000B),
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

		/** AES GMAC Authentication */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GMAC_authentication_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GMAC_authentication_verify_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GMAC_authentication_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GMAC_authentication_verify_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GMAC_authentication_test_case_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GMAC_authentication_verify_test_case_3),

		/** SNOW 3G encrypt only (UEA2) */
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

		/** SNOW 3G decrypt only (UEA2) */
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
			test_snow3g_cipher_auth_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_auth_cipher_test_case_1),

		/** ZUC encrypt only (EEA3) */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_encryption_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_encryption_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_encryption_test_case_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_encryption_test_case_4),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_encryption_test_case_5),

		/** ZUC authenticate (EIA3) */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_hash_generate_test_case_6),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_hash_generate_test_case_7),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_hash_generate_test_case_8),

		/** ZUC alg-chain (EEA3/EIA3) */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_cipher_auth_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_cipher_auth_test_case_2),

		/** HMAC_MD5 Authentication */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_MD5_HMAC_generate_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_MD5_HMAC_verify_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_MD5_HMAC_generate_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_MD5_HMAC_verify_case_2),

		/** NULL tests */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_null_auth_only_operation),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_null_cipher_only_operation),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_null_cipher_auth_operation),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_null_auth_cipher_operation),

		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_hash_generate_test_case_6),

		/** KASUMI tests */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_encryption_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_encryption_test_case_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_auth_cipher_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_cipher_auth_test_case_1),

		/** Negative tests */
		TEST_CASE_ST(ut_setup, ut_teardown,
			authentication_verify_HMAC_SHA1_fail_data_corrupt),
		TEST_CASE_ST(ut_setup, ut_teardown,
			authentication_verify_HMAC_SHA1_fail_tag_corrupt),
		TEST_CASE_ST(ut_setup, ut_teardown,
			authentication_verify_AES128_GMAC_fail_data_corrupt),
		TEST_CASE_ST(ut_setup, ut_teardown,
			authentication_verify_AES128_GMAC_fail_tag_corrupt),
		TEST_CASE_ST(ut_setup, ut_teardown,
			auth_decryption_AES128CBC_HMAC_SHA1_fail_data_corrupt),
		TEST_CASE_ST(ut_setup, ut_teardown,
			auth_decryption_AES128CBC_HMAC_SHA1_fail_tag_corrupt),

		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static struct unit_test_suite cryptodev_aesni_mb_testsuite  = {
	.suite_name = "Crypto Device AESNI MB Unit Test Suite",
	.setup = testsuite_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {
		TEST_CASE_ST(ut_setup, ut_teardown, test_AES_chain_mb_all),
		TEST_CASE_ST(ut_setup, ut_teardown, test_AES_cipheronly_mb_all),
		TEST_CASE_ST(ut_setup, ut_teardown, test_AES_docsis_mb_all),
		TEST_CASE_ST(ut_setup, ut_teardown, test_authonly_mb_all),

		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static struct unit_test_suite cryptodev_openssl_testsuite  = {
	.suite_name = "Crypto Device OPENSSL Unit Test Suite",
	.setup = testsuite_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {
		TEST_CASE_ST(ut_setup, ut_teardown, test_multi_session),
		TEST_CASE_ST(ut_setup, ut_teardown,
				test_multi_session_random_usage),
		TEST_CASE_ST(ut_setup, ut_teardown,
				test_AES_chain_openssl_all),
		TEST_CASE_ST(ut_setup, ut_teardown,
				test_AES_cipheronly_openssl_all),
		TEST_CASE_ST(ut_setup, ut_teardown,
				test_3DES_chain_openssl_all),
		TEST_CASE_ST(ut_setup, ut_teardown,
				test_3DES_cipheronly_openssl_all),
		TEST_CASE_ST(ut_setup, ut_teardown,
				test_DES_docsis_openssl_all),
		TEST_CASE_ST(ut_setup, ut_teardown,
				test_authonly_openssl_all),

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

		/** AES GMAC Authentication */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GMAC_authentication_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GMAC_authentication_verify_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GMAC_authentication_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GMAC_authentication_verify_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GMAC_authentication_test_case_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GMAC_authentication_verify_test_case_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GMAC_authentication_test_case_4),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GMAC_authentication_verify_test_case_4),

		/** Scatter-Gather */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_encrypt_SGL_out_of_place_400B_1seg),

		/** Negative tests */
		TEST_CASE_ST(ut_setup, ut_teardown,
			authentication_verify_HMAC_SHA1_fail_data_corrupt),
		TEST_CASE_ST(ut_setup, ut_teardown,
			authentication_verify_HMAC_SHA1_fail_tag_corrupt),
		TEST_CASE_ST(ut_setup, ut_teardown,
			authentication_verify_AES128_GMAC_fail_data_corrupt),
		TEST_CASE_ST(ut_setup, ut_teardown,
			authentication_verify_AES128_GMAC_fail_tag_corrupt),
		TEST_CASE_ST(ut_setup, ut_teardown,
			auth_decryption_AES128CBC_HMAC_SHA1_fail_data_corrupt),
		TEST_CASE_ST(ut_setup, ut_teardown,
			auth_decryption_AES128CBC_HMAC_SHA1_fail_tag_corrupt),

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

		/** AES GCM Authenticated Encryption 256 bits key */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_auth_encryption_test_case_256_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_auth_encryption_test_case_256_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_auth_encryption_test_case_256_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_auth_encryption_test_case_256_4),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_auth_encryption_test_case_256_5),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_auth_encryption_test_case_256_6),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_auth_encryption_test_case_256_7),

		/** AES GCM Authenticated Decryption 256 bits key */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_auth_decryption_test_case_256_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_auth_decryption_test_case_256_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_auth_decryption_test_case_256_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_auth_decryption_test_case_256_4),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_auth_decryption_test_case_256_5),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_auth_decryption_test_case_256_6),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_auth_decryption_test_case_256_7),

		/** AES GCM Authenticated Encryption big aad size */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_auth_encryption_test_case_aad_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_auth_encryption_test_case_aad_2),

		/** AES GCM Authenticated Decryption big aad size */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_auth_decryption_test_case_aad_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_auth_decryption_test_case_aad_2),

		/** AES GMAC Authentication */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GMAC_authentication_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GMAC_authentication_verify_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GMAC_authentication_test_case_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GMAC_authentication_verify_test_case_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GMAC_authentication_test_case_4),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GMAC_authentication_verify_test_case_4),

		/** Negative tests */
		TEST_CASE_ST(ut_setup, ut_teardown,
			authentication_verify_AES128_GMAC_fail_data_corrupt),
		TEST_CASE_ST(ut_setup, ut_teardown,
			authentication_verify_AES128_GMAC_fail_tag_corrupt),

		/** Out of place tests */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_authenticated_encryption_oop),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_authenticated_decryption_oop),

		/** Session-less tests */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_authenticated_encryption_sessionless),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_mb_AES_GCM_authenticated_decryption_sessionless),

		/** Scatter-Gather */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_encrypt_SGL_out_of_place_400B_1seg),

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
			test_kasumi_encryption_test_case_1_sgl),
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
			test_kasumi_encryption_test_case_1_oop_sgl),


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
			test_kasumi_hash_generate_test_case_6),
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
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_auth_cipher_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_cipher_auth_test_case_1),
		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};
static struct unit_test_suite cryptodev_sw_snow3g_testsuite  = {
	.suite_name = "Crypto Device SW SNOW 3G Unit Test Suite",
	.setup = testsuite_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {
		/** SNOW 3G encrypt only (UEA2) */
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
				test_snow3g_encryption_test_case_1_oop_sgl),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_decryption_test_case_1_oop),

		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_encryption_test_case_1_offset_oop),

		/** SNOW 3G decrypt only (UEA2) */
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
			test_snow3g_cipher_auth_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_auth_cipher_test_case_1),

		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static struct unit_test_suite cryptodev_sw_zuc_testsuite  = {
	.suite_name = "Crypto Device SW ZUC Unit Test Suite",
	.setup = testsuite_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {
		/** ZUC encrypt only (EEA3) */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_encryption_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_encryption_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_encryption_test_case_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_encryption_test_case_4),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_encryption_test_case_5),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_hash_generate_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_hash_generate_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_hash_generate_test_case_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_hash_generate_test_case_4),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_hash_generate_test_case_5),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_encryption_test_case_6_sgl),
		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static struct unit_test_suite cryptodev_dpaa2_sec_testsuite  = {
	.suite_name = "Crypto DPAA2_SEC Unit Test Suite",
	.setup = testsuite_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {
		TEST_CASE_ST(ut_setup, ut_teardown,
			     test_device_configure_invalid_dev_id),
		TEST_CASE_ST(ut_setup, ut_teardown,
			     test_multi_session),

		TEST_CASE_ST(ut_setup, ut_teardown,
			     test_AES_chain_dpaa2_sec_all),
		TEST_CASE_ST(ut_setup, ut_teardown,
			     test_3DES_chain_dpaa2_sec_all),
		TEST_CASE_ST(ut_setup, ut_teardown,
			     test_AES_cipheronly_dpaa2_sec_all),
		TEST_CASE_ST(ut_setup, ut_teardown,
			     test_3DES_cipheronly_dpaa2_sec_all),

		/** HMAC_MD5 Authentication */
		TEST_CASE_ST(ut_setup, ut_teardown,
			     test_MD5_HMAC_generate_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			     test_MD5_HMAC_verify_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			     test_MD5_HMAC_generate_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			     test_MD5_HMAC_verify_case_2),

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

static struct unit_test_suite cryptodev_armv8_testsuite  = {
	.suite_name = "Crypto Device ARMv8 Unit Test Suite",
	.setup = testsuite_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {
		TEST_CASE_ST(ut_setup, ut_teardown, test_AES_chain_armv8_all),

		/** Negative tests */
		TEST_CASE_ST(ut_setup, ut_teardown,
			auth_decryption_AES128CBC_HMAC_SHA1_fail_data_corrupt),
		TEST_CASE_ST(ut_setup, ut_teardown,
			auth_decryption_AES128CBC_HMAC_SHA1_fail_tag_corrupt),

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
test_cryptodev_openssl(void)
{
	gbl_cryptodev_type = RTE_CRYPTODEV_OPENSSL_PMD;

	return unit_test_suite_runner(&cryptodev_openssl_testsuite);
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

static int
test_cryptodev_sw_zuc(void /*argv __rte_unused, int argc __rte_unused*/)
{
	gbl_cryptodev_type = RTE_CRYPTODEV_ZUC_PMD;

	return unit_test_suite_runner(&cryptodev_sw_zuc_testsuite);
}

static int
test_cryptodev_armv8(void)
{
	gbl_cryptodev_type = RTE_CRYPTODEV_ARMV8_PMD;

	return unit_test_suite_runner(&cryptodev_armv8_testsuite);
}

#ifdef RTE_LIBRTE_PMD_CRYPTO_SCHEDULER

static int
test_cryptodev_scheduler(void /*argv __rte_unused, int argc __rte_unused*/)
{
	gbl_cryptodev_type = RTE_CRYPTODEV_SCHEDULER_PMD;
	return unit_test_suite_runner(&cryptodev_scheduler_testsuite);
}

REGISTER_TEST_COMMAND(cryptodev_scheduler_autotest, test_cryptodev_scheduler);

#endif

static int
test_cryptodev_dpaa2_sec(void /*argv __rte_unused, int argc __rte_unused*/)
{
	gbl_cryptodev_type = RTE_CRYPTODEV_DPAA2_SEC_PMD;
	return unit_test_suite_runner(&cryptodev_dpaa2_sec_testsuite);
}

REGISTER_TEST_COMMAND(cryptodev_qat_autotest, test_cryptodev_qat);
REGISTER_TEST_COMMAND(cryptodev_aesni_mb_autotest, test_cryptodev_aesni_mb);
REGISTER_TEST_COMMAND(cryptodev_openssl_autotest, test_cryptodev_openssl);
REGISTER_TEST_COMMAND(cryptodev_aesni_gcm_autotest, test_cryptodev_aesni_gcm);
REGISTER_TEST_COMMAND(cryptodev_null_autotest, test_cryptodev_null);
REGISTER_TEST_COMMAND(cryptodev_sw_snow3g_autotest, test_cryptodev_sw_snow3g);
REGISTER_TEST_COMMAND(cryptodev_sw_kasumi_autotest, test_cryptodev_sw_kasumi);
REGISTER_TEST_COMMAND(cryptodev_sw_zuc_autotest, test_cryptodev_sw_zuc);
REGISTER_TEST_COMMAND(cryptodev_sw_armv8_autotest, test_cryptodev_armv8);
REGISTER_TEST_COMMAND(cryptodev_dpaa2_sec_autotest, test_cryptodev_dpaa2_sec);
