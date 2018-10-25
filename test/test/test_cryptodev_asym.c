/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium Networks
 */

#include <rte_bus_vdev.h>
#include <rte_common.h>
#include <rte_hexdump.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_pause.h>

#include <rte_cryptodev.h>
#include <rte_cryptodev_pmd.h>
#include <rte_crypto.h>

#include "test_cryptodev.h"
#include "test_cryptodev_dh_test_vectors.h"
#include "test_cryptodev_dsa_test_vectors.h"
#include "test_cryptodev_mod_test_vectors.h"
#include "test_cryptodev_rsa_test_vectors.h"
#include "test_cryptodev_asym_util.h"
#include "test.h"

#define TEST_NUM_BUFS 10
#define TEST_NUM_SESSIONS 4

static int gbl_driver_id;
struct crypto_testsuite_params {
	struct rte_mempool *op_mpool;
	struct rte_mempool *session_mpool;
	struct rte_cryptodev_config conf;
	struct rte_cryptodev_qp_conf qp_conf;
	uint8_t valid_devs[RTE_CRYPTO_MAX_DEVS];
	uint8_t valid_dev_count;
};

struct crypto_unittest_params {
	struct rte_cryptodev_asym_session *sess;
	struct rte_crypto_op *op;
};

static struct crypto_testsuite_params testsuite_params = { NULL };

static int
test_rsa_sign_verify(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct rte_mempool *op_mpool = ts_params->op_mpool;
	struct rte_mempool *sess_mpool = ts_params->session_mpool;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_crypto_asym_op *asym_op = NULL;
	struct rte_crypto_op *op = NULL, *result_op = NULL;
	struct rte_cryptodev_asym_session *sess = NULL;
	int status = TEST_SUCCESS;
	uint8_t output_buf[TEST_DATA_SIZE] = {0};
	uint8_t input_buf[TEST_DATA_SIZE] = {0};

	sess = rte_cryptodev_asym_session_create(sess_mpool);

	if (!sess) {
		RTE_LOG(ERR, USER1, "line %u "
				"FAILED: %s", __LINE__,
				"Session creation failed");
		status = TEST_FAILED;
		goto error_exit;
	}

	if (rte_cryptodev_asym_session_init(dev_id, sess, &rsa_xform,
				sess_mpool) < 0) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s",
				__LINE__, "unabled to config sym session");
		status = TEST_FAILED;
		goto error_exit;
	}

	/* set up crypto op data structure */
	op = rte_crypto_op_alloc(op_mpool, RTE_CRYPTO_OP_TYPE_ASYMMETRIC);
	if (!op) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s",
				__LINE__,
				"Failed to allocate asymmetric crypto "
				"operation struct");
		status = TEST_FAILED;
		goto error_exit;
	}

	asym_op = op->asym;
	/* Compute sign on the test vector */
	asym_op->rsa.op_type = RTE_CRYPTO_ASYM_OP_SIGN;

	memcpy(input_buf, &rsaplaintext.data,
			rsaplaintext.len);
	asym_op->rsa.message.data = input_buf;
	asym_op->rsa.message.length = rsaplaintext.len;
	asym_op->rsa.sign.data = output_buf;
	asym_op->rsa.pad = RTE_CRYPTO_RSA_PKCS1_V1_5_BT1;

	debug_hexdump(stdout, "message", asym_op->rsa.message.data,
			asym_op->rsa.message.length);

	/* attach asymmetric crypto session to crypto operations */
	rte_crypto_op_attach_asym_session(op, sess);

	RTE_LOG(DEBUG, USER1, "Process ASYM operation");

	/* Process crypto operation */
	if (rte_cryptodev_enqueue_burst(dev_id, 0, &op, 1) != 1) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s",
				__LINE__, "Error sending packet for operation");
		status = TEST_FAILED;
		goto error_exit;
	}

	while (rte_cryptodev_dequeue_burst(dev_id, 0, &result_op, 1) == 0)
		rte_pause();

	if (result_op == NULL) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s",
				__LINE__, "Failed to process asym crypto op");
		status = TEST_FAILED;
		goto error_exit;
	}
	debug_hexdump(stdout, "signed message", asym_op->rsa.sign.data,
			asym_op->rsa.sign.length);
	asym_op = result_op->asym;

	/* Verify sign */
	asym_op->rsa.op_type = RTE_CRYPTO_ASYM_OP_VERIFY;
	asym_op->rsa.pad = RTE_CRYPTO_RSA_PKCS1_V1_5_BT2;

	/* Process crypto operation */
	if (rte_cryptodev_enqueue_burst(dev_id, 0, &op, 1) != 1) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s",
				__LINE__, "Error sending packet for operation");
		status = TEST_FAILED;
		goto error_exit;
	}

	while (rte_cryptodev_dequeue_burst(dev_id, 0, &result_op, 1) == 0)
		rte_pause();

	if (result_op == NULL) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s",
				__LINE__, "Failed to process asym crypto op");
		status = TEST_FAILED;
		goto error_exit;
	}
	status = TEST_SUCCESS;
	if (result_op->status != RTE_CRYPTO_OP_STATUS_SUCCESS) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s",
				__LINE__, "Failed to process asym crypto op");
		status = TEST_FAILED;
		goto error_exit;
	}

error_exit:

	if (sess) {
		rte_cryptodev_asym_session_clear(dev_id, sess);
		rte_cryptodev_asym_session_free(sess);
	}

	if (op)
		rte_crypto_op_free(op);

	TEST_ASSERT_EQUAL(status, 0, "Test failed");

	return status;
}

static int
test_rsa_enc_dec(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct rte_mempool *op_mpool = ts_params->op_mpool;
	struct rte_mempool *sess_mpool = ts_params->session_mpool;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_crypto_asym_op *asym_op = NULL;
	struct rte_crypto_op *op = NULL, *result_op = NULL;
	struct rte_cryptodev_asym_session *sess = NULL;
	int status = TEST_SUCCESS;
	uint8_t input_buf[TEST_DATA_SIZE] = {0};

	sess = rte_cryptodev_asym_session_create(sess_mpool);

	if (!sess) {
		RTE_LOG(ERR, USER1, "line %u "
				"FAILED: %s", __LINE__,
				"Session creation failed");
		status = TEST_FAILED;
		goto error_exit;
	}

	if (rte_cryptodev_asym_session_init(dev_id, sess, &rsa_xform,
				sess_mpool) < 0) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s",
				__LINE__, "unabled to config sym session");
		status = TEST_FAILED;
		goto error_exit;
	}

	/* set up crypto op data structure */
	op = rte_crypto_op_alloc(op_mpool, RTE_CRYPTO_OP_TYPE_ASYMMETRIC);
	if (!op) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s",
				__LINE__,
				"Failed to allocate asymmetric crypto "
				"operation struct");
		status = TEST_FAILED;
		goto error_exit;
	}

	asym_op = op->asym;
	/*Compute encryption on the test vector */
	asym_op->rsa.op_type = RTE_CRYPTO_ASYM_OP_ENCRYPT;

	memcpy(input_buf, rsaplaintext.data,
			rsaplaintext.len);
	asym_op->rsa.message.data = input_buf;
	asym_op->rsa.message.length = rsaplaintext.len;
	asym_op->rsa.pad = RTE_CRYPTO_RSA_PKCS1_V1_5_BT2;

	debug_hexdump(stdout, "message", asym_op->rsa.message.data,
			asym_op->rsa.message.length);

	/* attach asymmetric crypto session to crypto operations */
	rte_crypto_op_attach_asym_session(op, sess);

	RTE_LOG(DEBUG, USER1, "Process ASYM operation");

	/* Process crypto operation */
	if (rte_cryptodev_enqueue_burst(dev_id, 0, &op, 1) != 1) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s",
				__LINE__, "Error sending packet for operation");
		status = TEST_FAILED;
		goto error_exit;
	}

	while (rte_cryptodev_dequeue_burst(dev_id, 0, &result_op, 1) == 0)
		rte_pause();

	if (result_op == NULL) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s",
				__LINE__, "Failed to process asym crypto op");
		status = TEST_FAILED;
		goto error_exit;
	}
	debug_hexdump(stdout, "encrypted message", asym_op->rsa.message.data,
			asym_op->rsa.message.length);
	/* Use the resulted output as decryption Input vector*/
	asym_op = result_op->asym;
	asym_op->rsa.op_type = RTE_CRYPTO_ASYM_OP_DECRYPT;
	asym_op->rsa.pad = RTE_CRYPTO_RSA_PKCS1_V1_5_BT1;

	/* Process crypto operation */
	if (rte_cryptodev_enqueue_burst(dev_id, 0, &op, 1) != 1) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s",
				__LINE__, "Error sending packet for operation");
		status = TEST_FAILED;
		goto error_exit;
	}

	while (rte_cryptodev_dequeue_burst(dev_id, 0, &result_op, 1) == 0)
		rte_pause();

	if (result_op == NULL) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s",
				__LINE__, "Failed to process asym crypto op");
		status = TEST_FAILED;
		goto error_exit;
	}
	status = TEST_SUCCESS;
	int ret = 0;
	ret = rsa_verify(&rsaplaintext, result_op);
	if (ret)
		status = TEST_FAILED;

error_exit:

	if (sess) {
		rte_cryptodev_asym_session_clear(dev_id, sess);
		rte_cryptodev_asym_session_free(sess);
	}

	if (op)
		rte_crypto_op_free(op);

	TEST_ASSERT_EQUAL(status, 0, "Test failed");

	return status;
}

static int
testsuite_setup(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct rte_cryptodev_info info;
	uint32_t i = 0, nb_devs, dev_id;
	int ret;
	uint16_t qp_id;

	memset(ts_params, 0, sizeof(*ts_params));

	ts_params->op_mpool = rte_crypto_op_pool_create(
			"CRYPTO_ASYM_OP_POOL",
			RTE_CRYPTO_OP_TYPE_ASYMMETRIC,
			TEST_NUM_BUFS, 0,
			0,
			rte_socket_id());
	if (ts_params->op_mpool == NULL) {
		RTE_LOG(ERR, USER1, "Can't create ASYM_CRYPTO_OP_POOL\n");
		return TEST_FAILED;
	}

	/* Create an OPENSSL device if required */
	if (gbl_driver_id == rte_cryptodev_driver_id_get(
			RTE_STR(CRYPTODEV_NAME_OPENSSL_PMD))) {
		nb_devs = rte_cryptodev_device_count_by_driver(
				rte_cryptodev_driver_id_get(
				RTE_STR(CRYPTODEV_NAME_OPENSSL_PMD)));
		if (nb_devs < 1) {
			ret = rte_vdev_init(
				RTE_STR(CRYPTODEV_NAME_OPENSSL_PMD),
				NULL);

			TEST_ASSERT(ret == 0, "Failed to create "
				"instance of pmd : %s",
				RTE_STR(CRYPTODEV_NAME_OPENSSL_PMD));
		}
	}

	nb_devs = rte_cryptodev_count();
	if (nb_devs < 1) {
		RTE_LOG(ERR, USER1, "No crypto devices found?\n");
		return TEST_FAILED;
	}

	/* Create list of valid crypto devs */
	for (i = 0; i < nb_devs; i++) {
		rte_cryptodev_info_get(i, &info);
		if (info.driver_id == gbl_driver_id)
			ts_params->valid_devs[ts_params->valid_dev_count++] = i;
	}

	if (ts_params->valid_dev_count < 1)
		return TEST_FAILED;

	/* Set up all the qps on the first of the valid devices found */

	dev_id = ts_params->valid_devs[0];

	rte_cryptodev_info_get(dev_id, &info);

	/* check if device support asymmetric, skip if not */
	if (!(info.feature_flags &
				RTE_CRYPTODEV_FF_ASYMMETRIC_CRYPTO)) {
		RTE_LOG(ERR, USER1, "Device doesn't support asymmetric. "
				"Test Skipped.\n");
		return TEST_FAILED;
	}

	/* configure device with num qp */
	ts_params->conf.nb_queue_pairs = info.max_nb_queue_pairs;
	ts_params->conf.socket_id = SOCKET_ID_ANY;
	TEST_ASSERT_SUCCESS(rte_cryptodev_configure(dev_id,
			&ts_params->conf),
			"Failed to configure cryptodev %u with %u qps",
			dev_id, ts_params->conf.nb_queue_pairs);

	/* configure qp */
	ts_params->qp_conf.nb_descriptors = DEFAULT_NUM_OPS_INFLIGHT;
	for (qp_id = 0; qp_id < info.max_nb_queue_pairs; qp_id++) {
		TEST_ASSERT_SUCCESS(rte_cryptodev_queue_pair_setup(
			dev_id, qp_id, &ts_params->qp_conf,
			rte_cryptodev_socket_id(dev_id),
			ts_params->session_mpool),
			"Failed to setup queue pair %u on cryptodev %u ASYM",
			qp_id, dev_id);
	}

	/* setup asym session pool */
	unsigned int session_size =
		rte_cryptodev_asym_get_private_session_size(dev_id);
	/*
	 * Create mempool with TEST_NUM_SESSIONS * 2,
	 * to include the session headers
	 */
	ts_params->session_mpool = rte_mempool_create(
				"test_asym_sess_mp",
				TEST_NUM_SESSIONS * 2,
				session_size,
				0, 0, NULL, NULL, NULL,
				NULL, SOCKET_ID_ANY,
				0);

	TEST_ASSERT_NOT_NULL(ts_params->session_mpool,
			"session mempool allocation failed");

	return TEST_SUCCESS;
}

static void
testsuite_teardown(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;

	if (ts_params->op_mpool != NULL) {
		RTE_LOG(DEBUG, USER1, "CRYPTO_OP_POOL count %u\n",
		rte_mempool_avail_count(ts_params->op_mpool));
	}

	/* Free session mempools */
	if (ts_params->session_mpool != NULL) {
		rte_mempool_free(ts_params->session_mpool);
		ts_params->session_mpool = NULL;
	}
}

static int
ut_setup(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;

	uint16_t qp_id;

	/* Reconfigure device to default parameters */
	ts_params->conf.socket_id = SOCKET_ID_ANY;

	TEST_ASSERT_SUCCESS(rte_cryptodev_configure(ts_params->valid_devs[0],
			&ts_params->conf),
			"Failed to configure cryptodev %u",
			ts_params->valid_devs[0]);

	for (qp_id = 0; qp_id < ts_params->conf.nb_queue_pairs ; qp_id++) {
		TEST_ASSERT_SUCCESS(rte_cryptodev_queue_pair_setup(
			ts_params->valid_devs[0], qp_id,
			&ts_params->qp_conf,
			rte_cryptodev_socket_id(ts_params->valid_devs[0]),
			ts_params->session_mpool),
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
	struct rte_cryptodev_stats stats;

	rte_cryptodev_stats_get(ts_params->valid_devs[0], &stats);

	/* Stop the device */
	rte_cryptodev_stop(ts_params->valid_devs[0]);
}

static inline void print_asym_capa(
		const struct rte_cryptodev_asymmetric_xform_capability *capa)
{
	int i = 0;

	printf("\nxform type: %s\n===================\n",
			rte_crypto_asym_xform_strings[capa->xform_type]);
	printf("operation supported -");

	for (i = 0; i < RTE_CRYPTO_ASYM_OP_LIST_END; i++) {
		/* check supported operations */
		if (rte_cryptodev_asym_xform_capability_check_optype(capa, i))
			printf(" %s",
					rte_crypto_asym_op_strings[i]);
		}
		switch (capa->xform_type) {
		case RTE_CRYPTO_ASYM_XFORM_RSA:
		case RTE_CRYPTO_ASYM_XFORM_MODINV:
		case RTE_CRYPTO_ASYM_XFORM_MODEX:
		case RTE_CRYPTO_ASYM_XFORM_DH:
		case RTE_CRYPTO_ASYM_XFORM_DSA:
			printf(" modlen: min %d max %d increment %d\n",
					capa->modlen.min,
					capa->modlen.max,
					capa->modlen.increment);
		break;
		default:
			break;
		}
}

static int
test_capability(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_cryptodev_info dev_info;
	const struct rte_cryptodev_capabilities *dev_capa;
	int i = 0;
	struct rte_cryptodev_asym_capability_idx idx;
	const struct rte_cryptodev_asymmetric_xform_capability *capa;

	rte_cryptodev_info_get(dev_id, &dev_info);
	if (!(dev_info.feature_flags &
				RTE_CRYPTODEV_FF_ASYMMETRIC_CRYPTO)) {
		RTE_LOG(INFO, USER1,
				"Device doesn't support asymmetric. Test Skipped\n");
		return TEST_SUCCESS;
	}

	/* print xform capability */
	for (i = 0;
		dev_info.capabilities[i].op != RTE_CRYPTO_OP_TYPE_UNDEFINED;
		i++) {
		dev_capa = &(dev_info.capabilities[i]);
		if (dev_info.capabilities[i].op ==
				RTE_CRYPTO_OP_TYPE_ASYMMETRIC) {
			idx.type = dev_capa->asym.xform_capa.xform_type;

			capa = rte_cryptodev_asym_capability_get(dev_id,
				(const struct
				rte_cryptodev_asym_capability_idx *) &idx);
			print_asym_capa(capa);
			}
	}
	return TEST_SUCCESS;
}

static int
test_dh_gen_shared_sec(struct rte_crypto_asym_xform *xfrm)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct rte_mempool *op_mpool = ts_params->op_mpool;
	struct rte_mempool *sess_mpool = ts_params->session_mpool;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_crypto_asym_op *asym_op = NULL;
	struct rte_crypto_op *op = NULL, *result_op = NULL;
	struct rte_cryptodev_asym_session *sess = NULL;
	int status = TEST_SUCCESS;
	uint8_t output[TEST_DH_MOD_LEN];
	struct rte_crypto_asym_xform xform = *xfrm;
	uint8_t peer[] = "01234567890123456789012345678901234567890123456789";

	sess = rte_cryptodev_asym_session_create(sess_mpool);
	if (sess == NULL) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Session creation failed");
		status = TEST_FAILED;
		goto error_exit;
	}
	/* set up crypto op data structure */
	op = rte_crypto_op_alloc(op_mpool, RTE_CRYPTO_OP_TYPE_ASYMMETRIC);
	if (!op) {
		RTE_LOG(ERR, USER1,
			"line %u FAILED: %s",
			__LINE__, "Failed to allocate asymmetric crypto "
			"operation struct");
		status = TEST_FAILED;
		goto error_exit;
	}
	asym_op = op->asym;

	/* Setup a xform and op to generate private key only */
	xform.dh.type = RTE_CRYPTO_ASYM_OP_SHARED_SECRET_COMPUTE;
	xform.next = NULL;
	asym_op->dh.priv_key.data = dh_test_params.priv_key.data;
	asym_op->dh.priv_key.length = dh_test_params.priv_key.length;
	asym_op->dh.pub_key.data = (uint8_t *)peer;
	asym_op->dh.pub_key.length = sizeof(peer);
	asym_op->dh.shared_secret.data = output;
	asym_op->dh.shared_secret.length = sizeof(output);

	if (rte_cryptodev_asym_session_init(dev_id, sess, &xform,
			sess_mpool) < 0) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s",
				__LINE__, "unabled to config sym session");
		status = TEST_FAILED;
		goto error_exit;
	}

	/* attach asymmetric crypto session to crypto operations */
	rte_crypto_op_attach_asym_session(op, sess);

	RTE_LOG(DEBUG, USER1, "Process ASYM operation");

	/* Process crypto operation */
	if (rte_cryptodev_enqueue_burst(dev_id, 0, &op, 1) != 1) {
		RTE_LOG(ERR, USER1,
			"line %u FAILED: %s",
			__LINE__, "Error sending packet for operation");
		status = TEST_FAILED;
		goto error_exit;
	}

	while (rte_cryptodev_dequeue_burst(dev_id, 0, &result_op, 1) == 0)
		rte_pause();

	if (result_op == NULL) {
		RTE_LOG(ERR, USER1,
			"line %u FAILED: %s",
			__LINE__, "Failed to process asym crypto op");
		status = TEST_FAILED;
		goto error_exit;
	}

	debug_hexdump(stdout, "shared secret:",
			asym_op->dh.shared_secret.data,
			asym_op->dh.shared_secret.length);

error_exit:
	if (sess != NULL) {
		rte_cryptodev_asym_session_clear(dev_id, sess);
		rte_cryptodev_asym_session_free(sess);
	}
	if (op != NULL)
		rte_crypto_op_free(op);
	return status;
}

static int
test_dh_gen_priv_key(struct rte_crypto_asym_xform *xfrm)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct rte_mempool *op_mpool = ts_params->op_mpool;
	struct rte_mempool *sess_mpool = ts_params->session_mpool;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_crypto_asym_op *asym_op = NULL;
	struct rte_crypto_op *op = NULL, *result_op = NULL;
	struct rte_cryptodev_asym_session *sess = NULL;
	int status = TEST_SUCCESS;
	uint8_t output[TEST_DH_MOD_LEN];
	struct rte_crypto_asym_xform xform = *xfrm;

	sess = rte_cryptodev_asym_session_create(sess_mpool);
	if (sess == NULL) {
		RTE_LOG(ERR, USER1,
				 "line %u FAILED: %s", __LINE__,
				"Session creation failed");
		status = TEST_FAILED;
		goto error_exit;
	}
	/* set up crypto op data structure */
	op = rte_crypto_op_alloc(op_mpool, RTE_CRYPTO_OP_TYPE_ASYMMETRIC);
	if (!op) {
		RTE_LOG(ERR, USER1,
			"line %u FAILED: %s",
			__LINE__, "Failed to allocate asymmetric crypto "
			"operation struct");
		status = TEST_FAILED;
		goto error_exit;
	}
	asym_op = op->asym;

	/* Setup a xform and op to generate private key only */
	xform.dh.type = RTE_CRYPTO_ASYM_OP_PRIVATE_KEY_GENERATE;
	xform.next = NULL;
	asym_op->dh.priv_key.data = output;
	asym_op->dh.priv_key.length = sizeof(output);

	if (rte_cryptodev_asym_session_init(dev_id, sess, &xform,
			sess_mpool) < 0) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s",
				__LINE__, "unabled to config sym session");
		status = TEST_FAILED;
		goto error_exit;
	}

	/* attach asymmetric crypto session to crypto operations */
	rte_crypto_op_attach_asym_session(op, sess);

	RTE_LOG(DEBUG, USER1, "Process ASYM operation");

	/* Process crypto operation */
	if (rte_cryptodev_enqueue_burst(dev_id, 0, &op, 1) != 1) {
		RTE_LOG(ERR, USER1,
			"line %u FAILED: %s",
			__LINE__, "Error sending packet for operation");
		status = TEST_FAILED;
		goto error_exit;
	}

	while (rte_cryptodev_dequeue_burst(dev_id, 0, &result_op, 1) == 0)
		rte_pause();

	if (result_op == NULL) {
		RTE_LOG(ERR, USER1,
			"line %u FAILED: %s",
			__LINE__, "Failed to process asym crypto op");
		status = TEST_FAILED;
		goto error_exit;
	}

	debug_hexdump(stdout, "private key:",
			asym_op->dh.priv_key.data,
			asym_op->dh.priv_key.length);


error_exit:
	if (sess != NULL) {
		rte_cryptodev_asym_session_clear(dev_id, sess);
		rte_cryptodev_asym_session_free(sess);
	}
	if (op != NULL)
		rte_crypto_op_free(op);

	return status;
}


static int
test_dh_gen_pub_key(struct rte_crypto_asym_xform *xfrm)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct rte_mempool *op_mpool = ts_params->op_mpool;
	struct rte_mempool *sess_mpool = ts_params->session_mpool;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_crypto_asym_op *asym_op = NULL;
	struct rte_crypto_op *op = NULL, *result_op = NULL;
	struct rte_cryptodev_asym_session *sess = NULL;
	int status = TEST_SUCCESS;
	uint8_t output[TEST_DH_MOD_LEN];
	struct rte_crypto_asym_xform xform = *xfrm;

	sess = rte_cryptodev_asym_session_create(sess_mpool);
	if (sess == NULL) {
		RTE_LOG(ERR, USER1,
				 "line %u FAILED: %s", __LINE__,
				"Session creation failed");
		status = TEST_FAILED;
		goto error_exit;
	}
	/* set up crypto op data structure */
	op = rte_crypto_op_alloc(op_mpool, RTE_CRYPTO_OP_TYPE_ASYMMETRIC);
	if (!op) {
		RTE_LOG(ERR, USER1,
			"line %u FAILED: %s",
			__LINE__, "Failed to allocate asymmetric crypto "
			"operation struct");
		status = TEST_FAILED;
		goto error_exit;
	}
	asym_op = op->asym;
	/* Setup a xform chain to generate public key
	 * using test private key
	 *
	 */
	xform.dh.type = RTE_CRYPTO_ASYM_OP_PUBLIC_KEY_GENERATE;
	xform.next = NULL;

	asym_op->dh.pub_key.data = output;
	asym_op->dh.pub_key.length = sizeof(output);
	/* load pre-defined private key */
	asym_op->dh.priv_key.data = rte_malloc(NULL,
					dh_test_params.priv_key.length,
					0);
	asym_op->dh.priv_key = dh_test_params.priv_key;

	if (rte_cryptodev_asym_session_init(dev_id, sess, &xform,
			sess_mpool) < 0) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s",
				__LINE__, "unabled to config sym session");
		status = TEST_FAILED;
		goto error_exit;
	}

	/* attach asymmetric crypto session to crypto operations */
	rte_crypto_op_attach_asym_session(op, sess);

	RTE_LOG(DEBUG, USER1, "Process ASYM operation");

	/* Process crypto operation */
	if (rte_cryptodev_enqueue_burst(dev_id, 0, &op, 1) != 1) {
		RTE_LOG(ERR, USER1,
			"line %u FAILED: %s",
			__LINE__, "Error sending packet for operation");
		status = TEST_FAILED;
		goto error_exit;
	}

	while (rte_cryptodev_dequeue_burst(dev_id, 0, &result_op, 1) == 0)
		rte_pause();

	if (result_op == NULL) {
		RTE_LOG(ERR, USER1,
			"line %u FAILED: %s",
			__LINE__, "Failed to process asym crypto op");
		status = TEST_FAILED;
		goto error_exit;
	}

	debug_hexdump(stdout, "pub key:",
			asym_op->dh.pub_key.data, asym_op->dh.pub_key.length);

	debug_hexdump(stdout, "priv key:",
			asym_op->dh.priv_key.data, asym_op->dh.priv_key.length);

error_exit:
	if (sess != NULL) {
		rte_cryptodev_asym_session_clear(dev_id, sess);
		rte_cryptodev_asym_session_free(sess);
	}
	if (op != NULL)
		rte_crypto_op_free(op);

	return status;
}

static int
test_dh_gen_kp(struct rte_crypto_asym_xform *xfrm)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct rte_mempool *op_mpool = ts_params->op_mpool;
	struct rte_mempool *sess_mpool = ts_params->session_mpool;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_crypto_asym_op *asym_op = NULL;
	struct rte_crypto_op *op = NULL, *result_op = NULL;
	struct rte_cryptodev_asym_session *sess = NULL;
	int status = TEST_SUCCESS;
	uint8_t out_pub_key[TEST_DH_MOD_LEN];
	uint8_t out_prv_key[TEST_DH_MOD_LEN];
	struct rte_crypto_asym_xform pub_key_xform;
	struct rte_crypto_asym_xform xform = *xfrm;

	sess = rte_cryptodev_asym_session_create(sess_mpool);
	if (sess == NULL) {
		RTE_LOG(ERR, USER1,
				 "line %u FAILED: %s", __LINE__,
				"Session creation failed");
		status = TEST_FAILED;
		goto error_exit;
	}

	/* set up crypto op data structure */
	op = rte_crypto_op_alloc(op_mpool, RTE_CRYPTO_OP_TYPE_ASYMMETRIC);
	if (!op) {
		RTE_LOG(ERR, USER1,
			"line %u FAILED: %s",
			__LINE__, "Failed to allocate asymmetric crypto "
			"operation struct");
		status = TEST_FAILED;
		goto error_exit;
	}
	asym_op = op->asym;
	/* Setup a xform chain to generate
	 * private key first followed by
	 * public key
	 */xform.dh.type = RTE_CRYPTO_ASYM_OP_PRIVATE_KEY_GENERATE;
	pub_key_xform.xform_type = RTE_CRYPTO_ASYM_XFORM_DH;
	pub_key_xform.dh.type = RTE_CRYPTO_ASYM_OP_PUBLIC_KEY_GENERATE;
	xform.next = &pub_key_xform;

	asym_op->dh.pub_key.data = out_pub_key;
	asym_op->dh.pub_key.length = sizeof(out_pub_key);
	asym_op->dh.priv_key.data = out_prv_key;
	asym_op->dh.priv_key.length = sizeof(out_prv_key);
	if (rte_cryptodev_asym_session_init(dev_id, sess, &xform,
			sess_mpool) < 0) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s",
				__LINE__, "unabled to config sym session");
		status = TEST_FAILED;
		goto error_exit;
	}

	/* attach asymmetric crypto session to crypto operations */
	rte_crypto_op_attach_asym_session(op, sess);

	RTE_LOG(DEBUG, USER1, "Process ASYM operation");

	/* Process crypto operation */
	if (rte_cryptodev_enqueue_burst(dev_id, 0, &op, 1) != 1) {
		RTE_LOG(ERR, USER1,
			"line %u FAILED: %s",
			__LINE__, "Error sending packet for operation");
		status = TEST_FAILED;
		goto error_exit;
	}

	while (rte_cryptodev_dequeue_burst(dev_id, 0, &result_op, 1) == 0)
		rte_pause();

	if (result_op == NULL) {
		RTE_LOG(ERR, USER1,
			"line %u FAILED: %s",
			__LINE__, "Failed to process asym crypto op");
		status = TEST_FAILED;
		goto error_exit;
	}
	debug_hexdump(stdout, "priv key:",
			out_prv_key, asym_op->dh.priv_key.length);
	debug_hexdump(stdout, "pub key:",
			out_pub_key, asym_op->dh.pub_key.length);

error_exit:
	if (sess != NULL) {
		rte_cryptodev_asym_session_clear(dev_id, sess);
		rte_cryptodev_asym_session_free(sess);
	}
	if (op != NULL)
		rte_crypto_op_free(op);

	return status;
}

static int
test_mod_inv(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct rte_mempool *op_mpool = ts_params->op_mpool;
	struct rte_mempool *sess_mpool = ts_params->session_mpool;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_crypto_asym_op *asym_op = NULL;
	struct rte_crypto_op *op = NULL, *result_op = NULL;
	struct rte_cryptodev_asym_session *sess = NULL;
	int status = TEST_SUCCESS;
	struct rte_cryptodev_asym_capability_idx cap_idx;
	const struct rte_cryptodev_asymmetric_xform_capability *capability;
	uint8_t input[TEST_DATA_SIZE] = {0};
	int ret = 0;

	if (rte_cryptodev_asym_get_xform_enum(
		&modinv_xform.xform_type, "modinv") < 0) {
		RTE_LOG(ERR, USER1,
				 "Invalid ASYNC algorithm specified\n");
		return -1;
	}

	cap_idx.type = modinv_xform.xform_type;
	capability = rte_cryptodev_asym_capability_get(dev_id,
					&cap_idx);

	if (rte_cryptodev_asym_xform_capability_check_modlen(
		capability,
		modinv_xform.modinv.modulus.length)) {
		RTE_LOG(ERR, USER1,
				 "Invalid MODULOUS length specified\n");
				return -1;
		}

	sess = rte_cryptodev_asym_session_create(sess_mpool);
	if (!sess) {
		RTE_LOG(ERR, USER1, "line %u "
				"FAILED: %s", __LINE__,
				"Session creation failed");
		status = TEST_FAILED;
		goto error_exit;
	}

	if (rte_cryptodev_asym_session_init(dev_id, sess, &modinv_xform,
			sess_mpool) < 0) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s",
				__LINE__, "unabled to config sym session");
		status = TEST_FAILED;
		goto error_exit;
	}

	/* generate crypto op data structure */
	op = rte_crypto_op_alloc(op_mpool, RTE_CRYPTO_OP_TYPE_ASYMMETRIC);
	if (!op) {
		RTE_LOG(ERR, USER1,
			"line %u FAILED: %s",
			__LINE__, "Failed to allocate asymmetric crypto "
			"operation struct");
		status = TEST_FAILED;
		goto error_exit;
	}

	asym_op = op->asym;
	memcpy(input, base, sizeof(base));
	asym_op->modinv.base.data = input;
	asym_op->modinv.base.length = sizeof(base);

	/* attach asymmetric crypto session to crypto operations */
	rte_crypto_op_attach_asym_session(op, sess);

	RTE_LOG(DEBUG, USER1, "Process ASYM operation");

	/* Process crypto operation */
	if (rte_cryptodev_enqueue_burst(dev_id, 0, &op, 1) != 1) {
		RTE_LOG(ERR, USER1,
			"line %u FAILED: %s",
			__LINE__, "Error sending packet for operation");
		status = TEST_FAILED;
		goto error_exit;
	}

	while (rte_cryptodev_dequeue_burst(dev_id, 0, &result_op, 1) == 0)
		rte_pause();

	if (result_op == NULL) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s",
				__LINE__, "Failed to process asym crypto op");
		status = TEST_FAILED;
		goto error_exit;
	}

	ret = verify_modinv(mod_inv, result_op);
	if (ret) {
		RTE_LOG(ERR, USER1,
			 "operation verification failed\n");
		status = TEST_FAILED;
	}

error_exit:
	if (sess) {
		rte_cryptodev_asym_session_clear(dev_id, sess);
		rte_cryptodev_asym_session_free(sess);
	}

	if (op)
		rte_crypto_op_free(op);

	TEST_ASSERT_EQUAL(status, 0, "Test failed");

	return status;
}

static int
test_mod_exp(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct rte_mempool *op_mpool = ts_params->op_mpool;
	struct rte_mempool *sess_mpool = ts_params->session_mpool;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_crypto_asym_op *asym_op = NULL;
	struct rte_crypto_op *op = NULL, *result_op = NULL;
	struct rte_cryptodev_asym_session *sess = NULL;
	int status = TEST_SUCCESS;
	struct rte_cryptodev_asym_capability_idx cap_idx;
	const struct rte_cryptodev_asymmetric_xform_capability *capability;
	uint8_t input[TEST_DATA_SIZE] = {0};
	int ret = 0;

	if (rte_cryptodev_asym_get_xform_enum(&modex_xform.xform_type,
		"modexp")
		< 0) {
		RTE_LOG(ERR, USER1,
				"Invalid ASYNC algorithm specified\n");
		return -1;
	}

	/* check for modlen capability */
	cap_idx.type = modex_xform.xform_type;
	capability = rte_cryptodev_asym_capability_get(dev_id, &cap_idx);

	if (rte_cryptodev_asym_xform_capability_check_modlen(
			capability, modex_xform.modex.modulus.length)) {
		RTE_LOG(ERR, USER1,
				"Invalid MODULOUS length specified\n");
				return -1;
		}

	/* generate crypto op data structure */
	op = rte_crypto_op_alloc(op_mpool, RTE_CRYPTO_OP_TYPE_ASYMMETRIC);
	if (!op) {
		RTE_LOG(ERR, USER1,
			"line %u FAILED: %s",
			__LINE__, "Failed to allocate asymmetric crypto "
			"operation struct");
		status = TEST_FAILED;
		goto error_exit;
	}

	sess = rte_cryptodev_asym_session_create(sess_mpool);
	if (!sess) {
		RTE_LOG(ERR, USER1,
				 "line %u "
				"FAILED: %s", __LINE__,
				"Session creation failed");
		status = TEST_FAILED;
		goto error_exit;
	}

	if (rte_cryptodev_asym_session_init(dev_id, sess, &modex_xform,
			sess_mpool) < 0) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s",
				__LINE__, "unabled to config sym session");
		status = TEST_FAILED;
		goto error_exit;
	}

	asym_op = op->asym;
	memcpy(input, base, sizeof(base));
	asym_op->modex.base.data = input;
	asym_op->modex.base.length = sizeof(base);
	/* attach asymmetric crypto session to crypto operations */
	rte_crypto_op_attach_asym_session(op, sess);

	RTE_LOG(DEBUG, USER1, "Process ASYM operation");
	/* Process crypto operation */
	if (rte_cryptodev_enqueue_burst(dev_id, 0, &op, 1) != 1) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s",
				__LINE__, "Error sending packet for operation");
		status = TEST_FAILED;
		goto error_exit;
	}

	while (rte_cryptodev_dequeue_burst(dev_id, 0, &result_op, 1) == 0)
		rte_pause();

	if (result_op == NULL) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s",
				__LINE__, "Failed to process asym crypto op");
		status = TEST_FAILED;
		goto error_exit;
	}

	ret = verify_modexp(mod_exp, result_op);
	if (ret) {
		RTE_LOG(ERR, USER1,
			 "operation verification failed\n");
		status = TEST_FAILED;
	}

error_exit:
	if (sess != NULL) {
		rte_cryptodev_asym_session_clear(dev_id, sess);
		rte_cryptodev_asym_session_free(sess);
	}

	if (op != NULL)
		rte_crypto_op_free(op);

	TEST_ASSERT_EQUAL(status, 0, "Test failed");

	return status;
}

static int
test_dh_keygenration(void)
{
	int status;

	debug_hexdump(stdout, "p:", dh_xform.dh.p.data, dh_xform.dh.p.length);
	debug_hexdump(stdout, "g:", dh_xform.dh.g.data, dh_xform.dh.g.length);
	debug_hexdump(stdout, "priv_key:", dh_test_params.priv_key.data,
			dh_test_params.priv_key.length);

	RTE_LOG(INFO, USER1,
		"Test Public and Private key pair generation\n");

	status = test_dh_gen_kp(&dh_xform);
	TEST_ASSERT_EQUAL(status, 0, "Test failed");

	RTE_LOG(INFO, USER1,
		"Test Public Key Generation using pre-defined priv key\n");

	status = test_dh_gen_pub_key(&dh_xform);
	TEST_ASSERT_EQUAL(status, 0, "Test failed");

	RTE_LOG(INFO, USER1,
		"Test Private Key Generation only\n");

	status = test_dh_gen_priv_key(&dh_xform);
	TEST_ASSERT_EQUAL(status, 0, "Test failed");

	RTE_LOG(INFO, USER1,
		"Test shared secret compute\n");

	status = test_dh_gen_shared_sec(&dh_xform);
	TEST_ASSERT_EQUAL(status, 0, "Test failed");

	return status;
}

static int
test_dsa_sign(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct rte_mempool *op_mpool = ts_params->op_mpool;
	struct rte_mempool *sess_mpool = ts_params->session_mpool;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_crypto_asym_op *asym_op = NULL;
	struct rte_crypto_op *op = NULL, *result_op = NULL;
	struct rte_cryptodev_asym_session *sess = NULL;
	int status = TEST_SUCCESS;
	uint8_t r[TEST_DH_MOD_LEN];
	uint8_t s[TEST_DH_MOD_LEN];
	uint8_t dgst[] = "35d81554afaad2cf18f3a1770d5fedc4ea5be344";

	sess = rte_cryptodev_asym_session_create(sess_mpool);
	if (sess == NULL) {
		RTE_LOG(ERR, USER1,
				 "line %u FAILED: %s", __LINE__,
				"Session creation failed");
		status = TEST_FAILED;
		goto error_exit;
	}
	/* set up crypto op data structure */
	op = rte_crypto_op_alloc(op_mpool, RTE_CRYPTO_OP_TYPE_ASYMMETRIC);
	if (!op) {
		RTE_LOG(ERR, USER1,
			"line %u FAILED: %s",
			__LINE__, "Failed to allocate asymmetric crypto "
			"operation struct");
		status = TEST_FAILED;
		goto error_exit;
	}
	asym_op = op->asym;

	debug_hexdump(stdout, "p: ", dsa_xform.dsa.p.data,
			dsa_xform.dsa.p.length);
	debug_hexdump(stdout, "q: ", dsa_xform.dsa.q.data,
			dsa_xform.dsa.q.length);
	debug_hexdump(stdout, "g: ", dsa_xform.dsa.g.data,
			dsa_xform.dsa.g.length);
	debug_hexdump(stdout, "priv_key: ", dsa_xform.dsa.x.data,
			dsa_xform.dsa.x.length);

	if (rte_cryptodev_asym_session_init(dev_id, sess, &dsa_xform,
				sess_mpool) < 0) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s",
				__LINE__, "unabled to config sym session");
		status = TEST_FAILED;
		goto error_exit;
	}

	/* attach asymmetric crypto session to crypto operations */
	rte_crypto_op_attach_asym_session(op, sess);
	asym_op->dsa.op_type = RTE_CRYPTO_ASYM_OP_SIGN;
	asym_op->dsa.message.data = dgst;
	asym_op->dsa.message.length = sizeof(dgst);
	asym_op->dsa.r.length = sizeof(r);
	asym_op->dsa.r.data = r;
	asym_op->dsa.s.length = sizeof(s);
	asym_op->dsa.s.data = s;

	RTE_LOG(DEBUG, USER1, "Process ASYM operation");

	/* Process crypto operation */
	if (rte_cryptodev_enqueue_burst(dev_id, 0, &op, 1) != 1) {
		RTE_LOG(ERR, USER1,
			"line %u FAILED: %s",
			__LINE__, "Error sending packet for operation");
		status = TEST_FAILED;
		goto error_exit;
	}

	while (rte_cryptodev_dequeue_burst(dev_id, 0, &result_op, 1) == 0)
		rte_pause();

	if (result_op == NULL) {
		RTE_LOG(ERR, USER1,
			"line %u FAILED: %s",
			__LINE__, "Failed to process asym crypto op");
		status = TEST_FAILED;
		goto error_exit;
	}

	asym_op = result_op->asym;

	debug_hexdump(stdout, "r:",
			asym_op->dsa.r.data, asym_op->dsa.r.length);
	debug_hexdump(stdout, "s:",
			asym_op->dsa.s.data, asym_op->dsa.s.length);

	/* Test PMD DSA sign verification using signer public key */
	asym_op->dsa.op_type = RTE_CRYPTO_ASYM_OP_VERIFY;

	/* copy signer public key */
	asym_op->dsa.y.data = dsa_test_params.y.data;
	asym_op->dsa.y.length = dsa_test_params.y.length;

	/* Process crypto operation */
	if (rte_cryptodev_enqueue_burst(dev_id, 0, &op, 1) != 1) {
		RTE_LOG(ERR, USER1,
			"line %u FAILED: %s",
			__LINE__, "Error sending packet for operation");
		status = TEST_FAILED;
		goto error_exit;
	}

	while (rte_cryptodev_dequeue_burst(dev_id, 0, &result_op, 1) == 0)
		rte_pause();

	if (result_op == NULL) {
		RTE_LOG(ERR, USER1,
			"line %u FAILED: %s",
			__LINE__, "Failed to process asym crypto op");
		status = TEST_FAILED;
		goto error_exit;
	}

	if (result_op->status != RTE_CRYPTO_OP_STATUS_SUCCESS) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s",
				__LINE__, "Failed to process asym crypto op");
		status = TEST_FAILED;
	}
error_exit:
	if (sess != NULL) {
		rte_cryptodev_asym_session_clear(dev_id, sess);
		rte_cryptodev_asym_session_free(sess);
	}
	if (op != NULL)
		rte_crypto_op_free(op);
	return status;
}

static int
test_dsa(void)
{
	int status;
	status = test_dsa_sign();
	TEST_ASSERT_EQUAL(status, 0, "Test failed");
	return status;
}


static struct unit_test_suite cryptodev_openssl_asym_testsuite  = {
	.suite_name = "Crypto Device OPENSSL ASYM Unit Test Suite",
	.setup = testsuite_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {
		TEST_CASE_ST(ut_setup, ut_teardown, test_capability),
		TEST_CASE_ST(ut_setup, ut_teardown, test_dsa),
		TEST_CASE_ST(ut_setup, ut_teardown, test_dh_keygenration),
		TEST_CASE_ST(ut_setup, ut_teardown, test_rsa_enc_dec),
		TEST_CASE_ST(ut_setup, ut_teardown, test_rsa_sign_verify),
		TEST_CASE_ST(ut_setup, ut_teardown, test_mod_inv),
		TEST_CASE_ST(ut_setup, ut_teardown, test_mod_exp),
		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static int
test_cryptodev_openssl_asym(void)
{
	gbl_driver_id = rte_cryptodev_driver_id_get(
			RTE_STR(CRYPTODEV_NAME_OPENSSL_PMD));

	if (gbl_driver_id == -1) {
		RTE_LOG(ERR, USER1, "OPENSSL PMD must be loaded. Check if "
				"CONFIG_RTE_LIBRTE_PMD_OPENSSL is enabled "
				"in config file to run this testsuite.\n");
		return TEST_FAILED;
	}

	return unit_test_suite_runner(&cryptodev_openssl_asym_testsuite);
}

REGISTER_TEST_COMMAND(cryptodev_openssl_asym_autotest,
					  test_cryptodev_openssl_asym);
