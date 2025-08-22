/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium Networks
 * Copyright (c) 2019 Intel Corporation
 */

#include <rte_bus_vdev.h>
#include <rte_common.h>
#include <rte_hexdump.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_pause.h>

#include <rte_cryptodev.h>
#include <rte_crypto.h>

#include "test_cryptodev.h"
#include "test_cryptodev_dh_test_vectors.h"
#include "test_cryptodev_dsa_test_vectors.h"
#include "test_cryptodev_ecdh_test_vectors.h"
#include "test_cryptodev_ecdsa_test_vectors.h"
#include "test_cryptodev_ecpm_test_vectors.h"
#include "test_cryptodev_eddsa_test_vectors.h"
#include "test_cryptodev_mod_test_vectors.h"
#include "test_cryptodev_rsa_test_vectors.h"
#include "test_cryptodev_sm2_test_vectors.h"
#include "test_cryptodev_asym_util.h"
#include "test.h"

#define TEST_NUM_BUFS 10
#define TEST_NUM_SESSIONS 4

#ifndef TEST_DATA_SIZE
	#define TEST_DATA_SIZE 4096
#endif
#define ASYM_TEST_MSG_LEN 256
#define TEST_VECTOR_SIZE 256
#define DEQ_TIMEOUT 10000

static int gbl_driver_id;
static struct crypto_testsuite_params_asym {
	struct rte_mempool *op_mpool;
	struct rte_mempool *session_mpool;
	struct rte_cryptodev_config conf;
	struct rte_cryptodev_qp_conf qp_conf;
	uint8_t valid_devs[RTE_CRYPTO_MAX_DEVS];
	uint8_t valid_dev_count;
} testsuite_params, *params = &testsuite_params;

static struct ut_args {
	void *sess;
	struct rte_crypto_op *op;
	struct rte_crypto_op *result_op;
} _args, *self = &_args;

static int
queue_ops_rsa_sign_verify(void *sess)
{
	struct crypto_testsuite_params_asym *ts_params = &testsuite_params;
	struct rte_mempool *op_mpool = ts_params->op_mpool;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_crypto_op *op, *result_op;
	struct rte_crypto_asym_op *asym_op;
	uint8_t output_buf[TEST_DATA_SIZE];
	int status;

	/* Set up crypto op data structure */
	op = rte_crypto_op_alloc(op_mpool, RTE_CRYPTO_OP_TYPE_ASYMMETRIC);
	if (!op) {
		RTE_LOG(ERR, USER1, "Failed to allocate asymmetric crypto "
			"operation struct\n");
		return TEST_FAILED;
	}

	asym_op = op->asym;

	/* Compute sign on the test vector */
	asym_op->rsa.op_type = RTE_CRYPTO_ASYM_OP_SIGN;

	asym_op->rsa.message.data = rsaplaintext.data;
	asym_op->rsa.message.length = rsaplaintext.len;
	asym_op->rsa.sign.length = RTE_DIM(rsa_n);
	asym_op->rsa.sign.data = output_buf;

	debug_hexdump(stdout, "message", asym_op->rsa.message.data,
		      asym_op->rsa.message.length);

	/* Attach asymmetric crypto session to crypto operations */
	rte_crypto_op_attach_asym_session(op, sess);

	RTE_LOG(DEBUG, USER1, "Process ASYM operation\n");

	/* Process crypto operation */
	if (rte_cryptodev_enqueue_burst(dev_id, 0, &op, 1) != 1) {
		RTE_LOG(ERR, USER1, "Error sending packet for sign\n");
		status = TEST_FAILED;
		goto error_exit;
	}

	while (rte_cryptodev_dequeue_burst(dev_id, 0, &result_op, 1) == 0)
		rte_pause();

	if (result_op == NULL) {
		RTE_LOG(ERR, USER1, "Failed to process sign op\n");
		status = TEST_FAILED;
		goto error_exit;
	}

	debug_hexdump(stdout, "signed message", asym_op->rsa.sign.data,
		      asym_op->rsa.sign.length);
	asym_op = result_op->asym;

	/* Verify sign */
	asym_op->rsa.op_type = RTE_CRYPTO_ASYM_OP_VERIFY;

	/* Process crypto operation */
	if (rte_cryptodev_enqueue_burst(dev_id, 0, &op, 1) != 1) {
		RTE_LOG(ERR, USER1, "Error sending packet for verify\n");
		status = TEST_FAILED;
		goto error_exit;
	}

	while (rte_cryptodev_dequeue_burst(dev_id, 0, &result_op, 1) == 0)
		rte_pause();

	if (result_op == NULL) {
		RTE_LOG(ERR, USER1, "Failed to process verify op\n");
		status = TEST_FAILED;
		goto error_exit;
	}

	if (result_op->status != RTE_CRYPTO_OP_STATUS_SUCCESS) {
		RTE_LOG(ERR, USER1, "Failed to process sign-verify op\n");
		status = TEST_FAILED;
		goto error_exit;
	}

	/* Negative test */
	result_op->asym->rsa.sign.data[0] ^= 0xff;
	if (rte_cryptodev_enqueue_burst(dev_id, 0, &result_op, 1) != 1) {
		RTE_LOG(ERR, USER1, "Error sending packet for verify\n");
		status = TEST_FAILED;
		goto error_exit;
	}

	while (rte_cryptodev_dequeue_burst(dev_id, 0, &result_op, 1) == 0)
		rte_pause();

	if (result_op == NULL) {
		RTE_LOG(ERR, USER1, "Failed to process verify op\n");
		status = TEST_FAILED;
		goto error_exit;
	}

	if (result_op->status != RTE_CRYPTO_OP_STATUS_ERROR) {
		RTE_LOG(ERR, USER1, "Failed to process sign-verify op\n");
		status = TEST_FAILED;
		goto error_exit;
	}

	status = TEST_SUCCESS;
error_exit:

	rte_crypto_op_free(op);

	return status;
}

static int
queue_ops_rsa_enc_dec(void *sess)
{
	struct crypto_testsuite_params_asym *ts_params = &testsuite_params;
	struct rte_mempool *op_mpool = ts_params->op_mpool;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_crypto_op *op, *result_op;
	struct rte_crypto_asym_op *asym_op;
	uint8_t cipher_buf[TEST_DATA_SIZE] = {0};
	uint8_t msg_buf[TEST_DATA_SIZE] = {0};
	int ret, status;

	memcpy(msg_buf, rsaplaintext.data, rsaplaintext.len);

	/* Set up crypto op data structure */
	op = rte_crypto_op_alloc(op_mpool, RTE_CRYPTO_OP_TYPE_ASYMMETRIC);
	if (!op) {
		RTE_LOG(ERR, USER1, "Failed to allocate asymmetric crypto "
			"operation struct\n");
		return TEST_FAILED;
	}

	asym_op = op->asym;

	/* Compute encryption on the test vector */
	asym_op->rsa.op_type = RTE_CRYPTO_ASYM_OP_ENCRYPT;

	asym_op->rsa.message.data = msg_buf;
	asym_op->rsa.cipher.data = cipher_buf;
	asym_op->rsa.cipher.length = RTE_DIM(rsa_n);
	asym_op->rsa.message.length = rsaplaintext.len;

	debug_hexdump(stdout, "message", asym_op->rsa.message.data,
		      asym_op->rsa.message.length);

	/* Attach asymmetric crypto session to crypto operations */
	rte_crypto_op_attach_asym_session(op, sess);

	RTE_LOG(DEBUG, USER1, "Process ASYM operation\n");

	/* Process crypto operation */
	if (rte_cryptodev_enqueue_burst(dev_id, 0, &op, 1) != 1) {
		RTE_LOG(ERR, USER1, "Error sending packet for encryption\n");
		status = TEST_FAILED;
		goto error_exit;
	}

	while (rte_cryptodev_dequeue_burst(dev_id, 0, &result_op, 1) == 0)
		rte_pause();

	if (result_op == NULL) {
		RTE_LOG(ERR, USER1, "Failed to process encryption op\n");
		status = TEST_FAILED;
		goto error_exit;
	}
	debug_hexdump(stdout, "encrypted message", asym_op->rsa.cipher.data,
		      asym_op->rsa.cipher.length);

	/* Use the resulted output as decryption Input vector*/
	asym_op = result_op->asym;
	asym_op->rsa.message.length = RTE_DIM(rsa_n);
	asym_op->rsa.op_type = RTE_CRYPTO_ASYM_OP_DECRYPT;
	memset(asym_op->rsa.message.data, 0, asym_op->rsa.message.length);

	/* Process crypto operation */
	if (rte_cryptodev_enqueue_burst(dev_id, 0, &op, 1) != 1) {
		RTE_LOG(ERR, USER1, "Error sending packet for decryption\n");
		status = TEST_FAILED;
		goto error_exit;
	}

	while (rte_cryptodev_dequeue_burst(dev_id, 0, &result_op, 1) == 0)
		rte_pause();

	if (result_op == NULL) {
		RTE_LOG(ERR, USER1, "Failed to process decryption op\n");
		status = TEST_FAILED;
		goto error_exit;
	}

	if (result_op->status != RTE_CRYPTO_OP_STATUS_SUCCESS) {
		RTE_LOG(ERR, USER1, "Expected crypto op to succeed\n");
		status = TEST_FAILED;
		goto error_exit;
	}

	ret = rsa_verify(&rsaplaintext, result_op);
	if (ret) {
		status = TEST_FAILED;
		goto error_exit;
	}

	status = TEST_SUCCESS;
error_exit:

	rte_crypto_op_free(op);

	return status;
}

static int
test_rsa_sign_verify(void)
{
	struct crypto_testsuite_params_asym *ts_params = &testsuite_params;
	struct rte_mempool *sess_mpool = ts_params->session_mpool;
	struct rte_cryptodev_asym_capability_idx idx;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_crypto_asym_xform xform;
	void *sess = NULL;
	struct rte_cryptodev_info dev_info;
	int ret, status = TEST_SUCCESS;

	/* Check RSA capability */
	idx.type = RTE_CRYPTO_ASYM_XFORM_RSA;
	if (rte_cryptodev_asym_capability_get(dev_id, &idx) == NULL)
		return -ENOTSUP;

	/* Test case supports op with exponent key only,
	 * Check in PMD feature flag for RSA exponent key type support.
	 */
	rte_cryptodev_info_get(dev_id, &dev_info);
	if (!(dev_info.feature_flags &
				RTE_CRYPTODEV_FF_RSA_PRIV_OP_KEY_EXP)) {
		RTE_LOG(INFO, USER1, "Device doesn't support sign op with "
			"exponent key type. Test Skipped\n");
		return TEST_SKIPPED;
	}

	memcpy(&xform, &rsa_xform, sizeof(rsa_xform));
	xform.rsa.key_type = RTE_RSA_KEY_TYPE_EXP;

	ret = rte_cryptodev_asym_session_create(dev_id, &xform, sess_mpool, &sess);

	if (ret < 0) {
		RTE_LOG(ERR, USER1, "Session creation failed for "
			"sign_verify\n");
		status = (ret == -ENOTSUP) ? TEST_SKIPPED : TEST_FAILED;
		goto error_exit;
	}

	status = queue_ops_rsa_sign_verify(sess);

error_exit:
	rte_cryptodev_asym_session_free(dev_id, sess);

	TEST_ASSERT_EQUAL(status, 0, "Test failed");

	return status;
}

static int
test_rsa_enc_dec(void)
{
	struct crypto_testsuite_params_asym *ts_params = &testsuite_params;
	struct rte_mempool *sess_mpool = ts_params->session_mpool;
	struct rte_cryptodev_asym_capability_idx idx;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_crypto_asym_xform xform;
	void *sess = NULL;
	struct rte_cryptodev_info dev_info;
	int ret, status = TEST_SUCCESS;

	/* Check RSA capability */
	idx.type = RTE_CRYPTO_ASYM_XFORM_RSA;
	if (rte_cryptodev_asym_capability_get(dev_id, &idx) == NULL)
		return -ENOTSUP;

	/* Test case supports op with exponent key only,
	 * Check in PMD feature flag for RSA exponent key type support.
	 */
	rte_cryptodev_info_get(dev_id, &dev_info);
	if (!(dev_info.feature_flags &
				RTE_CRYPTODEV_FF_RSA_PRIV_OP_KEY_EXP)) {
		RTE_LOG(INFO, USER1, "Device doesn't support decrypt op with "
			"exponent key type. Test skipped\n");
		return TEST_SKIPPED;
	}

	memcpy(&xform, &rsa_xform, sizeof(rsa_xform));
	xform.rsa.key_type = RTE_RSA_KEY_TYPE_EXP;

	ret = rte_cryptodev_asym_session_create(dev_id, &xform, sess_mpool, &sess);

	if (ret < 0) {
		RTE_LOG(ERR, USER1, "Session creation failed for enc_dec\n");
		status = (ret == -ENOTSUP) ? TEST_SKIPPED : TEST_FAILED;
		goto error_exit;
	}

	status = queue_ops_rsa_enc_dec(sess);

error_exit:

	rte_cryptodev_asym_session_free(dev_id, sess);

	TEST_ASSERT_EQUAL(status, 0, "Test failed");

	return status;
}

static int
test_rsa_sign_verify_crt(void)
{
	struct crypto_testsuite_params_asym *ts_params = &testsuite_params;
	struct rte_mempool *sess_mpool = ts_params->session_mpool;
	struct rte_cryptodev_asym_capability_idx idx;
	uint8_t dev_id = ts_params->valid_devs[0];
	void *sess = NULL;
	struct rte_cryptodev_info dev_info;
	int ret, status = TEST_SUCCESS;

	/* Check RSA capability */
	idx.type = RTE_CRYPTO_ASYM_XFORM_RSA;
	if (rte_cryptodev_asym_capability_get(dev_id, &idx) == NULL)
		return -ENOTSUP;

	/* Test case supports op with quintuple format key only,
	 * Check im PMD feature flag for RSA quintuple key type support.
	 */
	rte_cryptodev_info_get(dev_id, &dev_info);
	if (!(dev_info.feature_flags & RTE_CRYPTODEV_FF_RSA_PRIV_OP_KEY_QT)) {
		RTE_LOG(INFO, USER1, "Device doesn't support sign op with "
			"quintuple key type. Test skipped\n");
		return TEST_SKIPPED;
	}

	ret = rte_cryptodev_asym_session_create(dev_id, &rsa_xform, sess_mpool, &sess);

	if (ret < 0) {
		RTE_LOG(ERR, USER1, "Session creation failed for "
			"sign_verify_crt\n");
		status = (ret == -ENOTSUP) ? TEST_SKIPPED : TEST_FAILED;
		goto error_exit;
	}

	status = queue_ops_rsa_sign_verify(sess);

error_exit:

	rte_cryptodev_asym_session_free(dev_id, sess);

	TEST_ASSERT_EQUAL(status, 0, "Test failed");

	return status;
}

static int
test_rsa_enc_dec_crt(void)
{
	struct crypto_testsuite_params_asym *ts_params = &testsuite_params;
	struct rte_mempool *sess_mpool = ts_params->session_mpool;
	struct rte_cryptodev_asym_capability_idx idx;
	uint8_t dev_id = ts_params->valid_devs[0];
	void *sess = NULL;
	struct rte_cryptodev_info dev_info;
	int ret, status = TEST_SUCCESS;

	/* Check RSA capability */
	idx.type = RTE_CRYPTO_ASYM_XFORM_RSA;
	if (rte_cryptodev_asym_capability_get(dev_id, &idx) == NULL)
		return -ENOTSUP;

	/* Test case supports op with quintuple format key only,
	 * Check in PMD feature flag for RSA quintuple key type support.
	 */
	rte_cryptodev_info_get(dev_id, &dev_info);
	if (!(dev_info.feature_flags & RTE_CRYPTODEV_FF_RSA_PRIV_OP_KEY_QT)) {
		RTE_LOG(INFO, USER1, "Device doesn't support decrypt op with "
			"quintuple key type. Test skipped\n");
		return TEST_SKIPPED;
	}

	ret = rte_cryptodev_asym_session_create(dev_id, &rsa_xform, sess_mpool, &sess);

	if (ret < 0) {
		RTE_LOG(ERR, USER1, "Session creation failed for "
			"enc_dec_crt\n");
		status = (ret == -ENOTSUP) ? TEST_SKIPPED : TEST_FAILED;
		goto error_exit;
	}

	status = queue_ops_rsa_enc_dec(sess);

error_exit:

	rte_cryptodev_asym_session_free(dev_id, sess);

	TEST_ASSERT_EQUAL(status, 0, "Test failed");

	return status;
}

static int
testsuite_setup(void)
{
	struct crypto_testsuite_params_asym *ts_params = &testsuite_params;
	uint8_t valid_devs[RTE_CRYPTO_MAX_DEVS];
	struct rte_cryptodev_info info;
	int ret, dev_id = -1;
	uint32_t i, nb_devs;
	uint16_t qp_id;

	memset(ts_params, 0, sizeof(*ts_params));

	/* Device, op pool and session configuration for asymmetric crypto. 8< */
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

	/* Get list of valid crypto devs */
	nb_devs = rte_cryptodev_devices_get(
				rte_cryptodev_driver_name_get(gbl_driver_id),
				valid_devs, RTE_CRYPTO_MAX_DEVS);
	if (nb_devs < 1) {
		RTE_LOG(ERR, USER1, "No crypto devices found?\n");
		return TEST_SKIPPED;
	}

	/*
	 * Get first valid asymmetric device found in test suite param and
	 * break
	 */
	for (i = 0; i < nb_devs ; i++) {
		rte_cryptodev_info_get(valid_devs[i], &info);
		if (info.feature_flags & RTE_CRYPTODEV_FF_ASYMMETRIC_CRYPTO) {
			dev_id = ts_params->valid_devs[0] = valid_devs[i];
			break;
		}
	}

	if (dev_id == -1) {
		RTE_LOG(ERR, USER1, "Device doesn't support asymmetric. "
			"Test skipped.\n");
		return TEST_FAILED;
	}

	/* Set valid device count */
	ts_params->valid_dev_count = nb_devs;

	/* configure device with num qp */
	ts_params->conf.nb_queue_pairs = info.max_nb_queue_pairs;
	ts_params->conf.socket_id = SOCKET_ID_ANY;
	ts_params->conf.ff_disable = RTE_CRYPTODEV_FF_SECURITY |
			RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO;
	TEST_ASSERT_SUCCESS(rte_cryptodev_configure(dev_id,
			&ts_params->conf),
			"Failed to configure cryptodev %u with %u qps",
			dev_id, ts_params->conf.nb_queue_pairs);

	/* configure qp */
	ts_params->qp_conf.nb_descriptors = DEFAULT_NUM_OPS_INFLIGHT;
	ts_params->qp_conf.mp_session = ts_params->session_mpool;
	for (qp_id = 0; qp_id < info.max_nb_queue_pairs; qp_id++) {
		TEST_ASSERT_SUCCESS(rte_cryptodev_queue_pair_setup(
			dev_id, qp_id, &ts_params->qp_conf,
			rte_cryptodev_socket_id(dev_id)),
			"Failed to setup queue pair %u on cryptodev %u ASYM",
			qp_id, dev_id);
	}

	ts_params->session_mpool = rte_cryptodev_asym_session_pool_create(
			"test_asym_sess_mp", TEST_NUM_SESSIONS, 0, 0,
			SOCKET_ID_ANY);

	TEST_ASSERT_NOT_NULL(ts_params->session_mpool,
			"session mempool allocation failed");
	/* >8 End of device, op pool and session configuration for asymmetric crypto section. */
	return TEST_SUCCESS;
}

static void
testsuite_teardown(void)
{
	struct crypto_testsuite_params_asym *ts_params = &testsuite_params;

	/* Reset device */
	ts_params->qp_conf.mp_session = NULL;
	ts_params->conf.ff_disable = 0;
	if (rte_cryptodev_configure(ts_params->valid_devs[0], &ts_params->conf))
		RTE_LOG(DEBUG, USER1, "Could not reset cryptodev\n");

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
ut_setup_asym(void)
{
	struct crypto_testsuite_params_asym *ts_params = &testsuite_params;
	uint16_t qp_id;

	memset(self, 0, sizeof(*self));
	self->op = rte_crypto_op_alloc(params->op_mpool,
			RTE_CRYPTO_OP_TYPE_ASYMMETRIC);
	TEST_ASSERT_NOT_NULL(self->op,
		"Failed to allocate asymmetric crypto operation struct"
	);

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
			rte_cryptodev_socket_id(ts_params->valid_devs[0])),
			"Failed to setup queue pair %u on cryptodev %u",
			qp_id, ts_params->valid_devs[0]);
	}

	/* Start the device */
	TEST_ASSERT_SUCCESS(rte_cryptodev_start(ts_params->valid_devs[0]),
						"Failed to start cryptodev %u",
						ts_params->valid_devs[0]);

	return TEST_SUCCESS;
}

static void
ut_teardown_asym(void)
{
	struct crypto_testsuite_params_asym *ts_params = &testsuite_params;
	uint8_t dev_id = ts_params->valid_devs[0];

	if (self->sess != NULL)
		rte_cryptodev_asym_session_free(dev_id, self->sess);
	rte_crypto_op_free(self->op);
	self->sess = NULL;
	self->op = NULL;
	self->result_op = NULL;

	/* Stop the device */
	rte_cryptodev_stop(ts_params->valid_devs[0]);
}

static inline void print_asym_capa(
		const struct rte_cryptodev_asymmetric_xform_capability *capa)
{
	int i = 0;

	printf("\nxform type: %s\n===================\n",
			rte_cryptodev_asym_get_xform_string(capa->xform_type));
	printf("operation supported -");

	for (i = 0; i < RTE_CRYPTO_ASYM_OP_LIST_END; i++) {
		/* check supported operations */
		if (rte_cryptodev_asym_xform_capability_check_optype(capa, i)) {
			if (capa->xform_type == RTE_CRYPTO_ASYM_XFORM_DH)
				printf(" %s", rte_crypto_asym_ke_strings[i]);
			else
				printf(" %s", rte_crypto_asym_op_strings[i]);
		}
	}
	switch (capa->xform_type) {
	case RTE_CRYPTO_ASYM_XFORM_RSA:
	case RTE_CRYPTO_ASYM_XFORM_MODINV:
	case RTE_CRYPTO_ASYM_XFORM_MODEX:
	case RTE_CRYPTO_ASYM_XFORM_DH:
	case RTE_CRYPTO_ASYM_XFORM_DSA:
		printf(" modlen: min %d max %d increment %d",
				capa->modlen.min,
				capa->modlen.max,
				capa->modlen.increment);
	break;
	case RTE_CRYPTO_ASYM_XFORM_ECDSA:
	case RTE_CRYPTO_ASYM_XFORM_ECPM:
	case RTE_CRYPTO_ASYM_XFORM_SM2:
	default:
		break;
	}
	printf("\n");
}

static int
test_capability(void)
{
	struct crypto_testsuite_params_asym *ts_params = &testsuite_params;
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
		return TEST_SKIPPED;
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
			TEST_ASSERT_NOT_NULL(capa, "Failed to get asymmetric capability");
			print_asym_capa(capa);
			}
	}
	return TEST_SUCCESS;
}

static int
test_dh_gen_shared_sec(struct rte_crypto_asym_xform *xfrm)
{
	struct crypto_testsuite_params_asym *ts_params = &testsuite_params;
	struct rte_mempool *op_mpool = ts_params->op_mpool;
	struct rte_mempool *sess_mpool = ts_params->session_mpool;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_crypto_asym_op *asym_op = NULL;
	struct rte_crypto_op *op = NULL, *result_op = NULL;
	void *sess = NULL;
	int ret, status = TEST_SUCCESS;
	uint8_t output[TEST_DH_MOD_LEN];
	struct rte_crypto_asym_xform xform = *xfrm;
	uint8_t peer[] = "01234567890123456789012345678901234567890123456789";

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
	xform.next = NULL;
	asym_op->dh.ke_type = RTE_CRYPTO_ASYM_KE_SHARED_SECRET_COMPUTE;
	asym_op->dh.priv_key.data = dh_test_params.priv_key.data;
	asym_op->dh.priv_key.length = dh_test_params.priv_key.length;
	asym_op->dh.pub_key.data = (uint8_t *)peer;
	asym_op->dh.pub_key.length = sizeof(peer);
	asym_op->dh.shared_secret.data = output;
	asym_op->dh.shared_secret.length = sizeof(output);

	ret = rte_cryptodev_asym_session_create(dev_id, &xform, sess_mpool, &sess);
	if (ret < 0) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Session creation failed");
		status = (ret == -ENOTSUP) ? TEST_SKIPPED : TEST_FAILED;
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
	if (sess != NULL)
		rte_cryptodev_asym_session_free(dev_id, sess);
	rte_crypto_op_free(op);
	return status;
}

static int
test_dh_gen_priv_key(struct rte_crypto_asym_xform *xfrm)
{
	struct crypto_testsuite_params_asym *ts_params = &testsuite_params;
	struct rte_mempool *op_mpool = ts_params->op_mpool;
	struct rte_mempool *sess_mpool = ts_params->session_mpool;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_crypto_asym_op *asym_op = NULL;
	struct rte_crypto_op *op = NULL, *result_op = NULL;
	void *sess = NULL;
	int ret, status = TEST_SUCCESS;
	uint8_t output[TEST_DH_MOD_LEN];
	struct rte_crypto_asym_xform xform = *xfrm;

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
	xform.next = NULL;
	asym_op->dh.ke_type = RTE_CRYPTO_ASYM_KE_PRIV_KEY_GENERATE;
	asym_op->dh.priv_key.data = output;
	asym_op->dh.priv_key.length = sizeof(output);

	ret = rte_cryptodev_asym_session_create(dev_id, &xform, sess_mpool, &sess);
	if (ret < 0) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Session creation failed");
		status = (ret == -ENOTSUP) ? TEST_SKIPPED : TEST_FAILED;
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
	if (sess != NULL)
		rte_cryptodev_asym_session_free(dev_id, sess);
	rte_crypto_op_free(op);

	return status;
}


static int
test_dh_gen_pub_key(struct rte_crypto_asym_xform *xfrm)
{
	struct crypto_testsuite_params_asym *ts_params = &testsuite_params;
	struct rte_mempool *op_mpool = ts_params->op_mpool;
	struct rte_mempool *sess_mpool = ts_params->session_mpool;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_crypto_asym_op *asym_op = NULL;
	struct rte_crypto_op *op = NULL, *result_op = NULL;
	void *sess = NULL;
	int ret, status = TEST_SUCCESS;
	uint8_t output[TEST_DH_MOD_LEN];
	struct rte_crypto_asym_xform xform = *xfrm;

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
	xform.next = NULL;

	asym_op->dh.ke_type = RTE_CRYPTO_ASYM_KE_PUB_KEY_GENERATE;
	asym_op->dh.pub_key.data = output;
	asym_op->dh.pub_key.length = sizeof(output);
	/* load pre-defined private key */
	asym_op->dh.priv_key.data = rte_malloc(NULL,
					dh_test_params.priv_key.length,
					0);
	asym_op->dh.priv_key = dh_test_params.priv_key;

	ret = rte_cryptodev_asym_session_create(dev_id, &xform, sess_mpool, &sess);
	if (ret < 0) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Session creation failed");
		status = (ret == -ENOTSUP) ? TEST_SKIPPED : TEST_FAILED;
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
	if (sess != NULL)
		rte_cryptodev_asym_session_free(dev_id, sess);
	rte_crypto_op_free(op);

	return status;
}

static int
test_dh_gen_kp(struct rte_crypto_asym_xform *xfrm)
{
	struct crypto_testsuite_params_asym *ts_params = &testsuite_params;
	struct rte_mempool *op_mpool = ts_params->op_mpool;
	struct rte_mempool *sess_mpool = ts_params->session_mpool;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_crypto_asym_op *asym_op = NULL;
	struct rte_crypto_op *op = NULL, *result_op = NULL;
	void *sess = NULL;
	int ret, status = TEST_SUCCESS;
	uint8_t out_pub_key[TEST_DH_MOD_LEN];
	uint8_t out_prv_key[TEST_DH_MOD_LEN];
	struct rte_crypto_asym_xform pub_key_xform;
	struct rte_crypto_asym_xform xform = *xfrm;

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
	 */
	pub_key_xform.xform_type = RTE_CRYPTO_ASYM_XFORM_DH;
	xform.next = &pub_key_xform;

	asym_op->dh.ke_type = RTE_CRYPTO_ASYM_KE_PUB_KEY_GENERATE;
	asym_op->dh.pub_key.data = out_pub_key;
	asym_op->dh.pub_key.length = sizeof(out_pub_key);
	asym_op->dh.priv_key.data = out_prv_key;
	asym_op->dh.priv_key.length = 0;

	ret = rte_cryptodev_asym_session_create(dev_id, &xform, sess_mpool, &sess);
	if (ret < 0) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Session creation failed");
		status = (ret == -ENOTSUP) ? TEST_SKIPPED : TEST_FAILED;
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
	if (sess != NULL)
		rte_cryptodev_asym_session_free(dev_id, sess);
	rte_crypto_op_free(op);

	return status;
}

static int
test_mod_inv(void)
{
	struct crypto_testsuite_params_asym *ts_params = &testsuite_params;
	struct rte_mempool *op_mpool = ts_params->op_mpool;
	struct rte_mempool *sess_mpool = ts_params->session_mpool;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_crypto_asym_op *asym_op = NULL;
	struct rte_crypto_op *op = NULL, *result_op = NULL;
	void *sess = NULL;
	int status = TEST_SUCCESS;
	struct rte_cryptodev_asym_capability_idx cap_idx;
	const struct rte_cryptodev_asymmetric_xform_capability *capability;
	uint8_t input[TEST_DATA_SIZE] = {0};
	int ret = 0;
	uint8_t result[sizeof(mod_p)] = { 0 };

	if (rte_cryptodev_asym_get_xform_enum(
		&modinv_xform.xform_type, "modinv") < 0) {
		RTE_LOG(ERR, USER1,
				 "Invalid ASYM algorithm specified\n");
		return -1;
	}

	cap_idx.type = modinv_xform.xform_type;
	capability = rte_cryptodev_asym_capability_get(dev_id,
					&cap_idx);

	if (capability == NULL) {
		RTE_LOG(INFO, USER1,
			"Device doesn't support MOD INV. Test Skipped\n");
		return TEST_SKIPPED;
	}

	if (rte_cryptodev_asym_xform_capability_check_modlen(
		capability,
		modinv_xform.modinv.modulus.length)) {
		RTE_LOG(ERR, USER1,
				 "Invalid MODULUS length specified\n");
				return TEST_SKIPPED;
		}

	ret = rte_cryptodev_asym_session_create(dev_id, &modinv_xform, sess_mpool, &sess);
	if (ret < 0) {
		RTE_LOG(ERR, USER1, "line %u "
				"FAILED: %s", __LINE__,
				"Session creation failed");
		status = (ret == -ENOTSUP) ? TEST_SKIPPED : TEST_FAILED;
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
	asym_op->modinv.result.data = result;
	asym_op->modinv.result.length = sizeof(result);

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
	if (sess)
		rte_cryptodev_asym_session_free(dev_id, sess);

	rte_crypto_op_free(op);

	TEST_ASSERT_EQUAL(status, 0, "Test failed");

	return status;
}

static int
test_mod_exp(void)
{
	struct crypto_testsuite_params_asym *ts_params = &testsuite_params;
	struct rte_mempool *op_mpool = ts_params->op_mpool;
	struct rte_mempool *sess_mpool = ts_params->session_mpool;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_crypto_asym_op *asym_op = NULL;
	struct rte_crypto_op *op = NULL, *result_op = NULL;
	void *sess = NULL;
	int status = TEST_SUCCESS;
	struct rte_cryptodev_asym_capability_idx cap_idx;
	const struct rte_cryptodev_asymmetric_xform_capability *capability;
	uint8_t input[TEST_DATA_SIZE] = {0};
	int ret = 0;
	uint8_t result[sizeof(mod_p)] = { 0 };

	if (rte_cryptodev_asym_get_xform_enum(&modex_xform.xform_type,
		"modexp")
		< 0) {
		RTE_LOG(ERR, USER1,
				"Invalid ASYM algorithm specified\n");
		return -1;
	}

	/* check for modlen capability */
	cap_idx.type = modex_xform.xform_type;
	capability = rte_cryptodev_asym_capability_get(dev_id, &cap_idx);

	if (capability == NULL) {
		RTE_LOG(INFO, USER1,
			"Device doesn't support MOD EXP. Test Skipped\n");
		return TEST_SKIPPED;
	}

	if (rte_cryptodev_asym_xform_capability_check_modlen(
			capability, modex_xform.modex.modulus.length)) {
		RTE_LOG(ERR, USER1,
				"Invalid MODULUS length specified\n");
				return TEST_SKIPPED;
		}

	/* Create op, create session, and process packets. 8< */
	op = rte_crypto_op_alloc(op_mpool, RTE_CRYPTO_OP_TYPE_ASYMMETRIC);
	if (!op) {
		RTE_LOG(ERR, USER1,
			"line %u FAILED: %s",
			__LINE__, "Failed to allocate asymmetric crypto "
			"operation struct");
		status = TEST_FAILED;
		goto error_exit;
	}

	ret = rte_cryptodev_asym_session_create(dev_id, &modex_xform, sess_mpool, &sess);
	if (ret < 0) {
		RTE_LOG(ERR, USER1,
				 "line %u "
				"FAILED: %s", __LINE__,
				"Session creation failed");
		status = (ret == -ENOTSUP) ? TEST_SKIPPED : TEST_FAILED;
		goto error_exit;
	}

	asym_op = op->asym;
	memcpy(input, base, sizeof(base));
	asym_op->modex.base.data = input;
	asym_op->modex.base.length = sizeof(base);
	asym_op->modex.result.data = result;
	asym_op->modex.result.length = sizeof(result);
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
	/* >8 End of create op, create session, and process packets section. */
	ret = verify_modexp(mod_exp, result_op);
	if (ret) {
		RTE_LOG(ERR, USER1,
			 "operation verification failed\n");
		status = TEST_FAILED;
	}

error_exit:
	if (sess != NULL)
		rte_cryptodev_asym_session_free(dev_id, sess);

	rte_crypto_op_free(op);

	TEST_ASSERT_EQUAL(status, 0, "Test failed");

	return status;
}

static int
test_dh_key_generation(void)
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
test_dsa_sign(struct rte_crypto_dsa_op_param *dsa_op)
{
	struct crypto_testsuite_params_asym *ts_params = &testsuite_params;
	struct rte_mempool *op_mpool = ts_params->op_mpool;
	struct rte_mempool *sess_mpool = ts_params->session_mpool;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_crypto_asym_op *asym_op = NULL;
	struct rte_crypto_op *op = NULL, *result_op = NULL;
	void *sess = NULL;
	int status = TEST_SUCCESS;
	int ret;

	ret = rte_cryptodev_asym_session_create(dev_id, &dsa_xform, sess_mpool, &sess);
	if (ret < 0) {
		RTE_LOG(ERR, USER1,
				 "line %u FAILED: %s", __LINE__,
				"Session creation failed");
		status = (ret == -ENOTSUP) ? TEST_SKIPPED : TEST_FAILED;
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
	asym_op->dsa = *dsa_op;

	debug_hexdump(stdout, "p: ", dsa_xform.dsa.p.data,
			dsa_xform.dsa.p.length);
	debug_hexdump(stdout, "q: ", dsa_xform.dsa.q.data,
			dsa_xform.dsa.q.length);
	debug_hexdump(stdout, "g: ", dsa_xform.dsa.g.data,
			dsa_xform.dsa.g.length);
	debug_hexdump(stdout, "priv_key: ", dsa_xform.dsa.x.data,
			dsa_xform.dsa.x.length);

	/* attach asymmetric crypto session to crypto operations */
	rte_crypto_op_attach_asym_session(op, sess);
	asym_op->dsa.op_type = RTE_CRYPTO_ASYM_OP_SIGN;
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
	dsa_op->r.length = asym_op->dsa.r.length;
	dsa_op->s.length = asym_op->dsa.s.length;

	debug_hexdump(stdout, "r:",
			asym_op->dsa.r.data, asym_op->dsa.r.length);
	debug_hexdump(stdout, "s:",
			asym_op->dsa.s.data, asym_op->dsa.s.length);
error_exit:
	if (sess != NULL)
		rte_cryptodev_asym_session_free(dev_id, sess);
	rte_crypto_op_free(op);
	return status;
}

static int
test_dsa_verify(struct rte_crypto_dsa_op_param *dsa_op)
{
	struct crypto_testsuite_params_asym *ts_params = &testsuite_params;
	struct rte_mempool *op_mpool = ts_params->op_mpool;
	struct rte_mempool *sess_mpool = ts_params->session_mpool;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_crypto_asym_op *asym_op = NULL;
	struct rte_crypto_op *op = NULL, *result_op = NULL;
	void *sess = NULL;
	int status = TEST_SUCCESS;
	int ret;

	ret = rte_cryptodev_asym_session_create(dev_id, &dsa_xform, sess_mpool, &sess);
	if (ret < 0) {
		RTE_LOG(ERR, USER1,
				 "line %u FAILED: %s", __LINE__,
				"Session creation failed");
		status = (ret == -ENOTSUP) ? TEST_SKIPPED : TEST_FAILED;
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
	asym_op->dsa = *dsa_op;

	debug_hexdump(stdout, "p: ", dsa_xform.dsa.p.data,
			dsa_xform.dsa.p.length);
	debug_hexdump(stdout, "q: ", dsa_xform.dsa.q.data,
			dsa_xform.dsa.q.length);
	debug_hexdump(stdout, "g: ", dsa_xform.dsa.g.data,
			dsa_xform.dsa.g.length);

	/* attach asymmetric crypto session to crypto operations */
	rte_crypto_op_attach_asym_session(op, sess);

	debug_hexdump(stdout, "r:",
			asym_op->dsa.r.data, asym_op->dsa.r.length);
	debug_hexdump(stdout, "s:",
			asym_op->dsa.s.data, asym_op->dsa.s.length);

	RTE_LOG(DEBUG, USER1, "Process ASYM verify operation");
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
	if (sess != NULL)
		rte_cryptodev_asym_session_free(dev_id, sess);
	rte_crypto_op_free(op);
	return status;
}

static int
test_dsa(void)
{
	int status;
	uint8_t r[TEST_DH_MOD_LEN];
	uint8_t s[TEST_DH_MOD_LEN];
	struct rte_crypto_dsa_op_param dsa_op;
	uint8_t dgst[] = "35d81554afaad2cf18f3a1770d5fedc4ea5be344";

	dsa_op.message.data = dgst;
	dsa_op.message.length = sizeof(dgst);
	dsa_op.r.data = r;
	dsa_op.s.data = s;
	dsa_op.r.length = sizeof(r);
	dsa_op.s.length = sizeof(s);

	status = test_dsa_sign(&dsa_op);
	TEST_ASSERT_EQUAL(status, 0, "DSA sign test failed");
	status = test_dsa_verify(&dsa_op);
	TEST_ASSERT_EQUAL(status, 0, "DSA verify test failed");
	return status;
}

static int
test_ecdsa_sign_verify(enum curve curve_id)
{
	struct crypto_testsuite_params_asym *ts_params = &testsuite_params;
	struct rte_mempool *sess_mpool = ts_params->session_mpool;
	struct rte_mempool *op_mpool = ts_params->op_mpool;
	struct crypto_testsuite_ecdsa_params input_params;
	void *sess = NULL;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_crypto_op *result_op = NULL;
	uint8_t output_buf_r[TEST_DATA_SIZE];
	uint8_t output_buf_s[TEST_DATA_SIZE];
	struct rte_crypto_asym_xform xform;
	struct rte_crypto_asym_op *asym_op;
	struct rte_cryptodev_info dev_info;
	struct rte_crypto_op *op = NULL;
	int ret, status = TEST_SUCCESS;

	switch (curve_id) {
	case SECP192R1:
		input_params = ecdsa_param_secp192r1;
		break;
	case SECP224R1:
		input_params = ecdsa_param_secp224r1;
		break;
	case SECP256R1:
		input_params = ecdsa_param_secp256r1;
		break;
	case SECP384R1:
		input_params = ecdsa_param_secp384r1;
		break;
	case SECP521R1:
		input_params = ecdsa_param_secp521r1;
		break;
	case SECP521R1_UA:
		input_params = ecdsa_param_secp521r1_ua;
		break;
	default:
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Unsupported curve id\n");
		status = TEST_FAILED;
		goto exit;
	}

	rte_cryptodev_info_get(dev_id, &dev_info);

	/* Setup crypto op data structure */
	op = rte_crypto_op_alloc(op_mpool, RTE_CRYPTO_OP_TYPE_ASYMMETRIC);
	if (op == NULL) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Failed to allocate asymmetric crypto "
				"operation struct\n");
		status = TEST_FAILED;
		goto exit;
	}
	asym_op = op->asym;

	/* Setup asym xform */
	xform.next = NULL;
	xform.xform_type = RTE_CRYPTO_ASYM_XFORM_ECDSA;
	xform.ec.curve_id = input_params.curve;
	xform.ec.pkey.data = input_params.pkey.data;
	xform.ec.pkey.length = input_params.pkey.length;
	xform.ec.q.x.data = input_params.pubkey_qx.data;
	xform.ec.q.x.length = input_params.pubkey_qx.length;
	xform.ec.q.y.data = input_params.pubkey_qy.data;
	xform.ec.q.y.length = input_params.pubkey_qy.length;

	ret = rte_cryptodev_asym_session_create(dev_id, &xform, sess_mpool, &sess);
	if (ret < 0) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Session creation failed\n");
		status = (ret == -ENOTSUP) ? TEST_SKIPPED : TEST_FAILED;
		goto exit;
	}

	/* Attach asymmetric crypto session to crypto operations */
	rte_crypto_op_attach_asym_session(op, sess);

	/* Compute sign */

	/* Populate op with operational details */
	op->asym->ecdsa.op_type = RTE_CRYPTO_ASYM_OP_SIGN;
	op->asym->ecdsa.message.data = input_params.digest.data;
	op->asym->ecdsa.message.length = input_params.digest.length;
	op->asym->ecdsa.k.data = input_params.scalar.data;
	op->asym->ecdsa.k.length = input_params.scalar.length;

	/* Init out buf */
	op->asym->ecdsa.r.data = output_buf_r;
	op->asym->ecdsa.s.data = output_buf_s;

	RTE_LOG(DEBUG, USER1, "Process ASYM operation\n");

	/* Process crypto operation */
	if (rte_cryptodev_enqueue_burst(dev_id, 0, &op, 1) != 1) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Error sending packet for operation\n");
		status = TEST_FAILED;
		goto exit;
	}

	while (rte_cryptodev_dequeue_burst(dev_id, 0, &result_op, 1) == 0)
		rte_pause();

	if (result_op == NULL) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Failed to process asym crypto op\n");
		status = TEST_FAILED;
		goto exit;
	}

	if (result_op->status != RTE_CRYPTO_OP_STATUS_SUCCESS) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Failed to process asym crypto op\n");
		status = TEST_FAILED;
		goto exit;
	}

	asym_op = result_op->asym;

	debug_hexdump(stdout, "r:",
			asym_op->ecdsa.r.data, asym_op->ecdsa.r.length);
	debug_hexdump(stdout, "s:",
			asym_op->ecdsa.s.data, asym_op->ecdsa.s.length);

	ret = verify_ecdsa_sign(input_params.sign_r.data,
				input_params.sign_s.data, result_op);
	if (ret) {
		status = TEST_FAILED;
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"ECDSA sign failed.\n");
		goto exit;
	}

	/* Verify sign */

	/* Populate op with operational details */
	op->asym->ecdsa.op_type = RTE_CRYPTO_ASYM_OP_VERIFY;
	op->asym->ecdsa.r.data = asym_op->ecdsa.r.data;
	op->asym->ecdsa.r.length = asym_op->ecdsa.r.length;
	op->asym->ecdsa.s.data = asym_op->ecdsa.s.data;
	op->asym->ecdsa.s.length = asym_op->ecdsa.s.length;

	/* Enqueue sign result for verify */
	if (rte_cryptodev_enqueue_burst(dev_id, 0, &op, 1) != 1) {
		status = TEST_FAILED;
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Error sending packet for operation\n");
		goto exit;
	}

	while (rte_cryptodev_dequeue_burst(dev_id, 0, &result_op, 1) == 0)
		rte_pause();

	if (result_op == NULL) {
		status = TEST_FAILED;
		goto exit;
	}
	if (result_op->status != RTE_CRYPTO_OP_STATUS_SUCCESS) {
		status = TEST_FAILED;
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"ECDSA verify failed.\n");
		goto exit;
	}

exit:
	if (sess != NULL)
		rte_cryptodev_asym_session_free(dev_id, sess);
	rte_crypto_op_free(op);
	return status;
};

static int
test_ecdsa_sign_verify_all_curve(void)
{
	int status, overall_status = TEST_SUCCESS;
	enum curve curve_id;
	int test_index = 0;
	const char *msg;

	for (curve_id = SECP192R1; curve_id < END_OF_CURVE_LIST; curve_id++) {
		if (curve_id == ED25519 || curve_id == ED448 ||
		    curve_id == ECGROUP19 || curve_id == ECGROUP20 ||
		    curve_id == ECGROUP21)
			continue;

		status = test_ecdsa_sign_verify(curve_id);
		if (status == TEST_SUCCESS) {
			msg = "succeeded";
		} else {
			msg = "failed";
			overall_status = status;
		}
		printf("  %u) TestCase Sign/Veriy Curve %s  %s\n",
		       test_index ++, curve[curve_id], msg);
	}
	return overall_status;
}

static int
test_ecpm(enum curve curve_id)
{
	struct crypto_testsuite_params_asym *ts_params = &testsuite_params;
	struct rte_mempool *sess_mpool = ts_params->session_mpool;
	struct rte_mempool *op_mpool = ts_params->op_mpool;
	struct crypto_testsuite_ecpm_params input_params;
	void *sess = NULL;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_crypto_op *result_op = NULL;
	uint8_t output_buf_x[TEST_DATA_SIZE];
	uint8_t output_buf_y[TEST_DATA_SIZE];
	struct rte_crypto_asym_xform xform;
	struct rte_crypto_asym_op *asym_op;
	struct rte_cryptodev_info dev_info;
	struct rte_crypto_op *op = NULL;
	int ret, status = TEST_SUCCESS;

	switch (curve_id) {
	case SECP192R1:
		input_params = ecpm_param_secp192r1;
		break;
	case SECP224R1:
		input_params = ecpm_param_secp224r1;
		break;
	case SECP256R1:
		input_params = ecpm_param_secp256r1;
		break;
	case SECP384R1:
		input_params = ecpm_param_secp384r1;
		break;
	case SECP521R1:
		input_params = ecpm_param_secp521r1;
		break;
	default:
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Unsupported curve id\n");
		status = TEST_FAILED;
		goto exit;
	}

	rte_cryptodev_info_get(dev_id, &dev_info);

	/* Setup crypto op data structure */
	op = rte_crypto_op_alloc(op_mpool, RTE_CRYPTO_OP_TYPE_ASYMMETRIC);
	if (op == NULL) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Failed to allocate asymmetric crypto "
				"operation struct\n");
		status = TEST_FAILED;
		goto exit;
	}
	asym_op = op->asym;

	/* Setup asym xform */
	xform.next = NULL;
	xform.xform_type = RTE_CRYPTO_ASYM_XFORM_ECPM;
	xform.ec.curve_id = input_params.curve;

	ret = rte_cryptodev_asym_session_create(dev_id, &xform, sess_mpool, &sess);
	if (ret < 0) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Session creation failed\n");
		status = (ret == -ENOTSUP) ? TEST_SKIPPED : TEST_FAILED;
		goto exit;
	}

	/* Attach asymmetric crypto session to crypto operations */
	rte_crypto_op_attach_asym_session(op, sess);

	/* Populate op with operational details */
	op->asym->ecpm.p.x.data = input_params.gen_x.data;
	op->asym->ecpm.p.x.length = input_params.gen_x.length;
	op->asym->ecpm.p.y.data = input_params.gen_y.data;
	op->asym->ecpm.p.y.length = input_params.gen_y.length;
	op->asym->ecpm.scalar.data = input_params.privkey.data;
	op->asym->ecpm.scalar.length = input_params.privkey.length;

	/* Init out buf */
	op->asym->ecpm.r.x.data = output_buf_x;
	op->asym->ecpm.r.y.data = output_buf_y;

	RTE_LOG(DEBUG, USER1, "Process ASYM operation\n");

	/* Process crypto operation */
	if (rte_cryptodev_enqueue_burst(dev_id, 0, &op, 1) != 1) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Error sending packet for operation\n");
		status = TEST_FAILED;
		goto exit;
	}

	while (rte_cryptodev_dequeue_burst(dev_id, 0, &result_op, 1) == 0)
		rte_pause();

	if (result_op == NULL) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Failed to process asym crypto op\n");
		status = TEST_FAILED;
		goto exit;
	}

	if (result_op->status != RTE_CRYPTO_OP_STATUS_SUCCESS) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Failed to process asym crypto op\n");
		status = TEST_FAILED;
		goto exit;
	}

	asym_op = result_op->asym;

	debug_hexdump(stdout, "r x:",
			asym_op->ecpm.r.x.data, asym_op->ecpm.r.x.length);
	debug_hexdump(stdout, "r y:",
			asym_op->ecpm.r.y.data, asym_op->ecpm.r.y.length);

	ret = verify_ecpm(input_params.pubkey_x.data,
				input_params.pubkey_y.data, result_op);
	if (ret) {
		status = TEST_FAILED;
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"EC Point Multiplication failed.\n");
		goto exit;
	}

exit:
	if (sess != NULL)
		rte_cryptodev_asym_session_free(dev_id, sess);
	rte_crypto_op_free(op);
	return status;
}

static int
test_ecpm_all_curve(void)
{
	int status, overall_status = TEST_SUCCESS;
	enum curve curve_id;
	int test_index = 0;
	const char *msg;

	for (curve_id = SECP192R1; curve_id < END_OF_CURVE_LIST; curve_id++) {
		if (curve_id == SECP521R1_UA || curve_id == ECGROUP19 ||
		    curve_id == ECGROUP20 || curve_id == ECGROUP21 ||
		    curve_id == ED25519 || curve_id == ED448)
			continue;

		status = test_ecpm(curve_id);
		if (status == TEST_SUCCESS) {
			msg = "succeeded";
		} else {
			msg = "failed";
			overall_status = status;
		}
		printf("  %u) TestCase EC Point Mul Curve %s  %s\n",
		       test_index ++, curve[curve_id], msg);
	}
	return overall_status;
}

static int
test_ecdh_priv_key_generate(enum curve curve_id)
{
	struct crypto_testsuite_params_asym *ts_params = &testsuite_params;
	const struct rte_cryptodev_asymmetric_xform_capability *capa;
	struct rte_mempool *sess_mpool = ts_params->session_mpool;
	struct rte_mempool *op_mpool = ts_params->op_mpool;
	struct rte_cryptodev_asym_capability_idx idx;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_crypto_asym_xform xform = {0};
	struct rte_crypto_op *result_op = NULL;
	uint8_t output_buf[TEST_DATA_SIZE];
	struct rte_crypto_asym_op *asym_op;
	struct rte_crypto_op *op = NULL;
	int ret, status = TEST_SUCCESS;
	uint16_t output_buflen = 0;
	void *sess = NULL;
	int curve;

	/* Check ECDH capability */
	idx.type = RTE_CRYPTO_ASYM_XFORM_ECDH;
	capa = rte_cryptodev_asym_capability_get(dev_id, &idx);
	if (capa == NULL)
		return -ENOTSUP;

	if (!(capa->op_types & (1 <<  RTE_CRYPTO_ASYM_KE_PRIV_KEY_GENERATE)))
		return -ENOTSUP;

	switch (curve_id) {
	case SECP192R1:
		curve = RTE_CRYPTO_EC_GROUP_SECP192R1;
		output_buflen = 24;
		break;
	case SECP224R1:
		curve = RTE_CRYPTO_EC_GROUP_SECP224R1;
		output_buflen = 28;
		break;
	case SECP256R1:
		curve = RTE_CRYPTO_EC_GROUP_SECP256R1;
		output_buflen = 32;
		break;
	case SECP384R1:
		curve = RTE_CRYPTO_EC_GROUP_SECP384R1;
		output_buflen = 48;
		break;
	case SECP521R1:
		curve = RTE_CRYPTO_EC_GROUP_SECP521R1;
		output_buflen = 66;
		break;
	default:
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Unsupported curve id\n");
		status = TEST_FAILED;
		goto exit;
	}

	/* Setup crypto op data structure */
	op = rte_crypto_op_alloc(op_mpool, RTE_CRYPTO_OP_TYPE_ASYMMETRIC);
	if (op == NULL) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Failed to allocate asymmetric crypto "
				"operation struct\n");
		status = TEST_FAILED;
		goto exit;
	}
	asym_op = op->asym;

	/* Setup asym xform */
	xform.next = NULL;
	xform.xform_type = RTE_CRYPTO_ASYM_XFORM_ECDH;
	xform.ec.curve_id = curve;

	ret = rte_cryptodev_asym_session_create(dev_id, &xform, sess_mpool, &sess);
	if (ret < 0) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Session creation failed\n");
		status = (ret == -ENOTSUP) ? TEST_SKIPPED : TEST_FAILED;
		goto exit;
	}

	/* Attach asymmetric crypto session to crypto operations */
	rte_crypto_op_attach_asym_session(op, sess);

	/* Populate op with operational details */
	asym_op->ecdh.ke_type = RTE_CRYPTO_ASYM_KE_PRIV_KEY_GENERATE;

	/* Init out buf */
	asym_op->ecdh.priv_key.data = output_buf;
	asym_op->ecdh.priv_key.length = output_buflen;

	RTE_LOG(DEBUG, USER1, "Process ASYM operation\n");

	/* Process crypto operation */
	if (rte_cryptodev_enqueue_burst(dev_id, 0, &op, 1) != 1) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Error sending packet for operation\n");
		status = TEST_FAILED;
		goto exit;
	}

	while (rte_cryptodev_dequeue_burst(dev_id, 0, &result_op, 1) == 0)
		rte_pause();

	if (result_op == NULL) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Failed to process asym crypto op\n");
		status = TEST_FAILED;
		goto exit;
	}

	if (result_op->status != RTE_CRYPTO_OP_STATUS_SUCCESS) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Failed to process asym crypto op\n");
		status = TEST_FAILED;
		goto exit;
	}

	asym_op = result_op->asym;

	debug_hexdump(stdout, "priv_key:",
		asym_op->ecdh.priv_key.data, asym_op->ecdh.priv_key.length);

exit:
	if (sess != NULL)
		rte_cryptodev_asym_session_free(dev_id, sess);
	rte_crypto_op_free(op);
	return status;
}

static int
test_ecdh_pub_key_generate(enum curve curve_id)
{
	struct crypto_testsuite_params_asym *ts_params = &testsuite_params;
	const struct rte_cryptodev_asymmetric_xform_capability *capa;
	struct rte_mempool *sess_mpool = ts_params->session_mpool;
	struct rte_mempool *op_mpool = ts_params->op_mpool;
	struct crypto_testsuite_ecdh_params input_params;
	struct rte_cryptodev_asym_capability_idx idx;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_crypto_asym_xform xform = {0};
	struct rte_crypto_op *result_op = NULL;
	uint8_t output_buf_x[TEST_DATA_SIZE];
	uint8_t output_buf_y[TEST_DATA_SIZE];
	struct rte_crypto_asym_op *asym_op;
	struct rte_crypto_op *op = NULL;
	int ret, status = TEST_SUCCESS;
	void *sess = NULL;

	/* Check ECDH capability */
	idx.type = RTE_CRYPTO_ASYM_XFORM_ECDH;
	capa = rte_cryptodev_asym_capability_get(dev_id, &idx);
	if (capa == NULL)
		return TEST_SKIPPED;

	if (!(capa->op_types & (1 <<  RTE_CRYPTO_ASYM_KE_PUB_KEY_GENERATE)))
		return TEST_SKIPPED;

	if (curve_id == ED25519 || curve_id == ED448) {
		/* Check EdDSA capability */
		idx.type = RTE_CRYPTO_ASYM_XFORM_EDDSA;
		capa = rte_cryptodev_asym_capability_get(dev_id, &idx);
		if (capa == NULL)
			return TEST_SKIPPED;
	}

	switch (curve_id) {
	case SECP192R1:
		input_params = ecdh_param_secp192r1;
		break;
	case SECP224R1:
		input_params = ecdh_param_secp224r1;
		break;
	case SECP256R1:
		input_params = ecdh_param_secp256r1;
		break;
	case SECP384R1:
		input_params = ecdh_param_secp384r1;
		break;
	case SECP521R1:
		input_params = ecdh_param_secp521r1;
		break;
	case ECGROUP19:
		input_params = ecdh_param_group19;
		break;
	case ECGROUP20:
		input_params = ecdh_param_group20;
		break;
	case ECGROUP21:
		input_params = ecdh_param_group21;
		break;
	case ED25519:
		input_params = ecdh_param_ed25519;
		break;
	case ED448:
		input_params = ecdh_param_ed448;
		break;
	default:
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Unsupported curve id\n");
		status = TEST_FAILED;
		goto exit;
	}

	debug_hexdump(stdout, "pkey:",
		input_params.pkey_A.data, input_params.pkey_A.length);

	/* Setup crypto op data structure */
	op = rte_crypto_op_alloc(op_mpool, RTE_CRYPTO_OP_TYPE_ASYMMETRIC);
	if (op == NULL) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Failed to allocate asymmetric crypto "
				"operation struct\n");
		status = TEST_FAILED;
		goto exit;
	}
	asym_op = op->asym;

	/* Setup asym xform */
	xform.next = NULL;
	xform.xform_type = RTE_CRYPTO_ASYM_XFORM_ECDH;
	xform.ec.curve_id = input_params.curve;
	xform.ec.pkey.data = input_params.pkey_A.data;
	xform.ec.pkey.length = input_params.pkey_A.length;

	ret = rte_cryptodev_asym_session_create(dev_id, &xform, sess_mpool, &sess);
	if (ret < 0) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Session creation failed\n");
		status = (ret == -ENOTSUP) ? TEST_SKIPPED : TEST_FAILED;
		goto exit;
	}

	/* Attach asymmetric crypto session to crypto operations */
	rte_crypto_op_attach_asym_session(op, sess);

	/* Populate op with operational details */
	asym_op->ecdh.ke_type = RTE_CRYPTO_ASYM_KE_PUB_KEY_GENERATE;
	if (curve_id == ED25519 || curve_id == ED448)
		asym_op->flags |= RTE_CRYPTO_ASYM_FLAG_PUB_KEY_COMPRESSED;

	/* Init out buf */
	asym_op->ecdh.pub_key.x.data = output_buf_x;
	if (curve_id == ED25519 || curve_id == ED448)
		asym_op->ecdh.pub_key.y.data = NULL;
	else
		asym_op->ecdh.pub_key.y.data = output_buf_y;

	RTE_LOG(DEBUG, USER1, "Process ASYM operation\n");

	/* Process crypto operation */
	if (rte_cryptodev_enqueue_burst(dev_id, 0, &op, 1) != 1) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Error sending packet for operation\n");
		status = TEST_FAILED;
		goto exit;
	}

	while (rte_cryptodev_dequeue_burst(dev_id, 0, &result_op, 1) == 0)
		rte_pause();

	if (result_op == NULL) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Failed to process asym crypto op\n");
		status = TEST_FAILED;
		goto exit;
	}

	if (result_op->status != RTE_CRYPTO_OP_STATUS_SUCCESS) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Failed to process asym crypto op\n");
		status = TEST_FAILED;
		goto exit;
	}

	asym_op = result_op->asym;

	debug_hexdump(stdout, "qx:",
		asym_op->ecdh.pub_key.x.data, asym_op->ecdh.pub_key.x.length);
	debug_hexdump(stdout, "qy:",
		asym_op->ecdh.pub_key.y.data, asym_op->ecdh.pub_key.y.length);

	if (curve_id == ED25519 || curve_id == ED448)
		ret = memcmp(input_params.pubkey_qA_x.data, result_op->asym->ecdh.pub_key.x.data,
			   result_op->asym->ecdh.pub_key.x.length);
	else
		ret = verify_ecdh_secret(input_params.pubkey_qA_x.data,
				input_params.pubkey_qA_y.data, result_op);

	if (ret) {
		status = TEST_FAILED;
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"ECDH public key generation failed.\n");
		goto exit;
	}

exit:
	if (sess != NULL)
		rte_cryptodev_asym_session_free(dev_id, sess);
	rte_crypto_op_free(op);
	return status;
}

static int
test_ecdh_pub_key_verify(enum curve curve_id)
{
	struct crypto_testsuite_params_asym *ts_params = &testsuite_params;
	const struct rte_cryptodev_asymmetric_xform_capability *capa;
	struct rte_mempool *sess_mpool = ts_params->session_mpool;
	struct rte_mempool *op_mpool = ts_params->op_mpool;
	struct crypto_testsuite_ecdh_params input_params;
	struct rte_cryptodev_asym_capability_idx idx;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_crypto_asym_xform xform = {0};
	struct rte_crypto_op *result_op = NULL;
	struct rte_crypto_asym_op *asym_op;
	struct rte_crypto_op *op = NULL;
	int ret, status = TEST_SUCCESS;
	void *sess = NULL;

	/* Check ECDH capability */
	idx.type = RTE_CRYPTO_ASYM_XFORM_ECDH;
	capa = rte_cryptodev_asym_capability_get(dev_id, &idx);
	if (capa == NULL)
		return -ENOTSUP;

	if (!(capa->op_types & (1 <<  RTE_CRYPTO_ASYM_KE_PUB_KEY_VERIFY)))
		return -ENOTSUP;

	switch (curve_id) {
	case SECP192R1:
		input_params = ecdh_param_secp192r1;
		break;
	case SECP224R1:
		input_params = ecdh_param_secp224r1;
		break;
	case SECP256R1:
		input_params = ecdh_param_secp256r1;
		break;
	case SECP384R1:
		input_params = ecdh_param_secp384r1;
		break;
	case SECP521R1:
		input_params = ecdh_param_secp521r1;
		break;
	case ECGROUP19:
		input_params = ecdh_param_group19;
		break;
	case ECGROUP20:
		input_params = ecdh_param_group20;
		break;
	case ECGROUP21:
		input_params = ecdh_param_group21;
		break;
	default:
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Unsupported curve id\n");
		status = TEST_FAILED;
		goto exit;
	}

	debug_hexdump(stdout, "qx:",
		input_params.pubkey_qA_x.data, input_params.pubkey_qA_x.length);
	debug_hexdump(stdout, "qy:",
		input_params.pubkey_qA_y.data, input_params.pubkey_qA_y.length);

	/* Setup crypto op data structure */
	op = rte_crypto_op_alloc(op_mpool, RTE_CRYPTO_OP_TYPE_ASYMMETRIC);
	if (op == NULL) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Failed to allocate asymmetric crypto "
				"operation struct\n");
		status = TEST_FAILED;
		goto exit;
	}
	asym_op = op->asym;

	/* Setup asym xform */
	xform.next = NULL;
	xform.xform_type = RTE_CRYPTO_ASYM_XFORM_ECDH;
	xform.ec.curve_id = input_params.curve;

	ret = rte_cryptodev_asym_session_create(dev_id, &xform, sess_mpool, &sess);
	if (ret < 0) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Session creation failed\n");
		status = (ret == -ENOTSUP) ? TEST_SKIPPED : TEST_FAILED;
		goto exit;
	}

	/* Attach asymmetric crypto session to crypto operations */
	rte_crypto_op_attach_asym_session(op, sess);

	/* Populate op with operational details */
	asym_op->ecdh.ke_type = RTE_CRYPTO_ASYM_KE_PUB_KEY_VERIFY;
	asym_op->ecdh.pub_key.x.data = input_params.pubkey_qA_x.data;
	asym_op->ecdh.pub_key.x.length = input_params.pubkey_qA_x.length;
	asym_op->ecdh.pub_key.y.data = input_params.pubkey_qA_y.data;
	asym_op->ecdh.pub_key.y.length = input_params.pubkey_qA_y.length;

	RTE_LOG(DEBUG, USER1, "Process ASYM operation\n");

	/* Process crypto operation */
	if (rte_cryptodev_enqueue_burst(dev_id, 0, &op, 1) != 1) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Error sending packet for operation\n");
		status = TEST_FAILED;
		goto exit;
	}

	while (rte_cryptodev_dequeue_burst(dev_id, 0, &result_op, 1) == 0)
		rte_pause();

	if (result_op == NULL) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Failed to process asym crypto op\n");
		status = TEST_FAILED;
		goto exit;
	}

	if (result_op->status != RTE_CRYPTO_OP_STATUS_SUCCESS) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Failed to process asym crypto op\n");
		status = TEST_FAILED;
		goto exit;
	}

exit:
	if (sess != NULL)
		rte_cryptodev_asym_session_free(dev_id, sess);
	rte_crypto_op_free(op);
	return status;
}

static int
test_ecdh_shared_secret(enum curve curve_id)
{
	struct crypto_testsuite_params_asym *ts_params = &testsuite_params;
	const struct rte_cryptodev_asymmetric_xform_capability *capa;
	struct rte_mempool *sess_mpool = ts_params->session_mpool;
	struct rte_mempool *op_mpool = ts_params->op_mpool;
	struct crypto_testsuite_ecdh_params input_params;
	struct rte_cryptodev_asym_capability_idx idx;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_crypto_asym_xform xform = {0};
	struct rte_crypto_op *result_op = NULL;
	uint8_t output_buf_x[TEST_DATA_SIZE];
	uint8_t output_buf_y[TEST_DATA_SIZE];
	struct rte_crypto_asym_op *asym_op;
	struct rte_crypto_op *op = NULL;
	int ret, status = TEST_SUCCESS;
	void *sess = NULL;

	/* Check ECDH capability */
	idx.type = RTE_CRYPTO_ASYM_XFORM_ECDH;
	capa = rte_cryptodev_asym_capability_get(dev_id, &idx);
	if (capa == NULL)
		return -ENOTSUP;

	if (!(capa->op_types & (1 <<  RTE_CRYPTO_ASYM_KE_SHARED_SECRET_COMPUTE)))
		return -ENOTSUP;

	switch (curve_id) {
	case SECP192R1:
		input_params = ecdh_param_secp192r1;
		break;
	case SECP224R1:
		input_params = ecdh_param_secp224r1;
		break;
	case SECP256R1:
		input_params = ecdh_param_secp256r1;
		break;
	case SECP384R1:
		input_params = ecdh_param_secp384r1;
		break;
	case SECP521R1:
		input_params = ecdh_param_secp521r1;
		break;
	case ECGROUP19:
		input_params = ecdh_param_group19;
		break;
	case ECGROUP20:
		input_params = ecdh_param_group20;
		break;
	case ECGROUP21:
		input_params = ecdh_param_group21;
		break;
	default:
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Unsupported curve id\n");
		status = TEST_FAILED;
		goto exit;
	}

	/* zA = dA.QB */
	debug_hexdump(stdout, "pkey:",
		input_params.pkey_A.data, input_params.pkey_A.length);
	debug_hexdump(stdout, "qx:",
		input_params.pubkey_qB_x.data, input_params.pubkey_qB_x.length);
	debug_hexdump(stdout, "qy:",
		input_params.pubkey_qB_y.data, input_params.pubkey_qB_y.length);

	/* Setup crypto op data structure */
	op = rte_crypto_op_alloc(op_mpool, RTE_CRYPTO_OP_TYPE_ASYMMETRIC);
	if (op == NULL) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Failed to allocate asymmetric crypto "
				"operation struct\n");
		status = TEST_FAILED;
		goto exit;
	}
	asym_op = op->asym;

	/* Setup asym xform */
	xform.next = NULL;
	xform.xform_type = RTE_CRYPTO_ASYM_XFORM_ECDH;
	xform.ec.curve_id = input_params.curve;
	xform.ec.pkey.data = input_params.pkey_A.data;
	xform.ec.pkey.length = input_params.pkey_A.length;
	xform.ec.q.x.data = input_params.pubkey_qB_x.data;
	xform.ec.q.x.length = input_params.pubkey_qB_x.length;
	xform.ec.q.y.data = input_params.pubkey_qB_y.data;
	xform.ec.q.y.length = input_params.pubkey_qB_y.length;

	ret = rte_cryptodev_asym_session_create(dev_id, &xform, sess_mpool, &sess);
	if (ret < 0) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Session creation failed\n");
		status = (ret == -ENOTSUP) ? TEST_SKIPPED : TEST_FAILED;
		goto exit;
	}

	/* Attach asymmetric crypto session to crypto operations */
	rte_crypto_op_attach_asym_session(op, sess);

	/* Populate op with operational details */
	asym_op->ecdh.ke_type = RTE_CRYPTO_ASYM_KE_SHARED_SECRET_COMPUTE;

	/* Init out buf */
	asym_op->ecdh.shared_secret.x.data = output_buf_x;
	asym_op->ecdh.shared_secret.y.data = output_buf_y;

	RTE_LOG(DEBUG, USER1, "Process ASYM operation\n");

	/* Process crypto operation */
	if (rte_cryptodev_enqueue_burst(dev_id, 0, &op, 1) != 1) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Error sending packet for operation\n");
		status = TEST_FAILED;
		goto exit;
	}

	while (rte_cryptodev_dequeue_burst(dev_id, 0, &result_op, 1) == 0)
		rte_pause();

	if (result_op == NULL) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Failed to process asym crypto op\n");
		status = TEST_FAILED;
		goto exit;
	}

	if (result_op->status != RTE_CRYPTO_OP_STATUS_SUCCESS) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Failed to process asym crypto op\n");
		status = TEST_FAILED;
		goto exit;
	}

	asym_op = result_op->asym;

	debug_hexdump(stdout, "secret_x:",
		asym_op->ecdh.shared_secret.x.data, asym_op->ecdh.shared_secret.x.length);
	debug_hexdump(stdout, "secret_y:",
		asym_op->ecdh.shared_secret.y.data, asym_op->ecdh.shared_secret.y.length);

	ret = verify_ecdh_secret(input_params.secret_x.data,
				input_params.secret_y.data, result_op);
	if (ret) {
		status = TEST_FAILED;
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"ECDH shared secret compute failed.\n");
		goto exit;
	}

	if (sess != NULL)
		rte_cryptodev_asym_session_free(dev_id, sess);
	rte_crypto_op_free(op);

	/* zB = dB.QA */
	debug_hexdump(stdout, "pkey:",
		input_params.pkey_B.data, input_params.pkey_B.length);
	debug_hexdump(stdout, "qx:",
		input_params.pubkey_qA_x.data, input_params.pubkey_qA_x.length);
	debug_hexdump(stdout, "qy:",
		input_params.pubkey_qA_y.data, input_params.pubkey_qA_y.length);

	/* Setup crypto op data structure */
	op = rte_crypto_op_alloc(op_mpool, RTE_CRYPTO_OP_TYPE_ASYMMETRIC);
	if (op == NULL) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Failed to allocate asymmetric crypto "
				"operation struct\n");
		status = TEST_FAILED;
		goto exit;
	}
	asym_op = op->asym;

	/* Setup asym xform */
	xform.next = NULL;
	xform.xform_type = RTE_CRYPTO_ASYM_XFORM_ECDH;
	xform.ec.curve_id = input_params.curve;
	xform.ec.pkey.data = input_params.pkey_B.data;
	xform.ec.pkey.length = input_params.pkey_B.length;
	xform.ec.q.x.data = input_params.pubkey_qA_x.data;
	xform.ec.q.x.length = input_params.pubkey_qA_x.length;
	xform.ec.q.y.data = input_params.pubkey_qA_y.data;
	xform.ec.q.y.length = input_params.pubkey_qA_y.length;

	ret = rte_cryptodev_asym_session_create(dev_id, &xform, sess_mpool, &sess);
	if (ret < 0) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Session creation failed\n");
		status = (ret == -ENOTSUP) ? TEST_SKIPPED : TEST_FAILED;
		goto exit;
	}

	/* Attach asymmetric crypto session to crypto operations */
	rte_crypto_op_attach_asym_session(op, sess);

	/* Populate op with operational details */
	asym_op->ecdh.ke_type = RTE_CRYPTO_ASYM_KE_SHARED_SECRET_COMPUTE;

	/* Init out buf */
	asym_op->ecdh.shared_secret.x.data = output_buf_x;
	asym_op->ecdh.shared_secret.y.data = output_buf_y;

	RTE_LOG(DEBUG, USER1, "Process ASYM operation\n");

	/* Process crypto operation */
	if (rte_cryptodev_enqueue_burst(dev_id, 0, &op, 1) != 1) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Error sending packet for operation\n");
		status = TEST_FAILED;
		goto exit;
	}

	while (rte_cryptodev_dequeue_burst(dev_id, 0, &result_op, 1) == 0)
		rte_pause();

	if (result_op == NULL) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Failed to process asym crypto op\n");
		status = TEST_FAILED;
		goto exit;
	}

	if (result_op->status != RTE_CRYPTO_OP_STATUS_SUCCESS) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Failed to process asym crypto op\n");
		status = TEST_FAILED;
		goto exit;
	}

	asym_op = result_op->asym;

	debug_hexdump(stdout, "secret_x:",
			asym_op->ecdh.shared_secret.x.data, asym_op->ecdh.shared_secret.x.length);
	debug_hexdump(stdout, "secret_y:",
			asym_op->ecdh.shared_secret.y.data, asym_op->ecdh.shared_secret.y.length);

	ret = verify_ecdh_secret(input_params.secret_x.data,
				input_params.secret_y.data, result_op);
	if (ret) {
		status = TEST_FAILED;
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"ECDH shared secret compute failed.\n");
		goto exit;
	}

exit:
	if (sess != NULL)
		rte_cryptodev_asym_session_free(dev_id, sess);
	rte_crypto_op_free(op);
	return status;
}

static int
test_ecdh_all_curve(void)
{
	int status, overall_status = TEST_SUCCESS;
	enum curve curve_id;
	int test_index = 0;
	const char *msg;

	for (curve_id = SECP192R1; curve_id < END_OF_CURVE_LIST; curve_id++) {
		if (curve_id == SECP521R1_UA || curve_id == ECGROUP19 ||
		    curve_id == ECGROUP20 || curve_id == ECGROUP21 ||
		    curve_id == ED25519 || curve_id == ED448)
			continue;

		status = test_ecdh_priv_key_generate(curve_id);
		if (status == TEST_SUCCESS) {
			msg = "succeeded";
		} else {
			msg = "failed";
			overall_status = status;
		}
		printf("  %u) TestCase ECDH private key generation for Curve %s %s\n",
		       test_index ++, curve[curve_id], msg);
	}

	for (curve_id = SECP192R1; curve_id < END_OF_CURVE_LIST; curve_id++) {
		if (curve_id == SECP521R1_UA)
			continue;

		status = test_ecdh_pub_key_generate(curve_id);
		if (status == TEST_SUCCESS) {
			msg = "succeeded";
		} else if (status == TEST_SKIPPED) {
			msg = "skipped";
		} else {
			msg = "failed";
			overall_status = status;
		}
		printf("  %u) TestCase ECDH public key generation for Curve %s %s\n",
		       test_index ++, curve[curve_id], msg);
	}

	for (curve_id = SECP192R1; curve_id < END_OF_CURVE_LIST; curve_id++) {
		if (curve_id == SECP521R1_UA || curve_id == ED25519 || curve_id == ED448)
			continue;

		status = test_ecdh_pub_key_verify(curve_id);
		if (status == TEST_SUCCESS) {
			msg = "succeeded";
		} else {
			msg = "failed";
			overall_status = status;
		}
		printf("  %u) TestCase ECDH public key verification for Curve %s %s\n",
		       test_index ++, curve[curve_id], msg);
	}

	for (curve_id = SECP192R1; curve_id < END_OF_CURVE_LIST; curve_id++) {
		if (curve_id == SECP521R1_UA || curve_id == ED25519 || curve_id == ED448)
			continue;

		status = test_ecdh_shared_secret(curve_id);
		if (status == TEST_SUCCESS) {
			msg = "succeeded";
		} else {
			msg = "failed";
			overall_status = status;
		}
		printf("  %u) TestCase ECDH shared secret compute for Curve %s %s\n",
		       test_index ++, curve[curve_id], msg);
	}

	return overall_status;
}

static int
test_sm2_sign(void)
{
	struct crypto_testsuite_params_asym *ts_params = &testsuite_params;
	struct crypto_testsuite_sm2_params input_params = sm2_param_fp256;
	const struct rte_cryptodev_asymmetric_xform_capability *capa;
	struct rte_mempool *sess_mpool = ts_params->session_mpool;
	struct rte_mempool *op_mpool = ts_params->op_mpool;
	struct rte_cryptodev_asym_capability_idx idx;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_crypto_op *result_op = NULL;
	uint8_t output_buf_r[TEST_DATA_SIZE];
	uint8_t output_buf_s[TEST_DATA_SIZE];
	struct rte_crypto_asym_xform xform;
	struct rte_crypto_asym_op *asym_op;
	struct rte_crypto_op *op = NULL;
	int ret, status = TEST_SUCCESS;
	void *sess = NULL;

	/* Check SM2 capability */
	idx.type = RTE_CRYPTO_ASYM_XFORM_SM2;
	capa = rte_cryptodev_asym_capability_get(dev_id, &idx);
	if (capa == NULL)
		return -ENOTSUP;

	/* Setup crypto op data structure */
	op = rte_crypto_op_alloc(op_mpool, RTE_CRYPTO_OP_TYPE_ASYMMETRIC);
	if (op == NULL) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Failed to allocate asymmetric crypto "
				"operation struct\n");
		status = TEST_FAILED;
		goto exit;
	}

	asym_op = op->asym;

	/* Setup asym xform */
	xform.next = NULL;
	xform.xform_type = RTE_CRYPTO_ASYM_XFORM_SM2;
	xform.ec.curve_id = input_params.curve;
	xform.ec.pkey.data = input_params.pkey.data;
	xform.ec.pkey.length = input_params.pkey.length;
	xform.ec.q.x.data = input_params.pubkey_qx.data;
	xform.ec.q.x.length = input_params.pubkey_qx.length;
	xform.ec.q.y.data = input_params.pubkey_qy.data;
	xform.ec.q.y.length = input_params.pubkey_qy.length;

	ret = rte_cryptodev_asym_session_create(dev_id, &xform, sess_mpool, &sess);
	if (ret < 0) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Session creation failed\n");
		status = (ret == -ENOTSUP) ? TEST_SKIPPED : TEST_FAILED;
		goto exit;
	}

	/* Attach asymmetric crypto session to crypto operations */
	rte_crypto_op_attach_asym_session(op, sess);

	/* Compute sign */

	/* Populate op with operational details */
	asym_op->sm2.op_type = RTE_CRYPTO_ASYM_OP_SIGN;
	if (rte_cryptodev_asym_xform_capability_check_opcap(capa,
			RTE_CRYPTO_ASYM_OP_SIGN, RTE_CRYPTO_SM2_PH))
		asym_op->sm2.hash = RTE_CRYPTO_AUTH_SM3;
	else
		asym_op->sm2.hash = RTE_CRYPTO_AUTH_NULL;

	if (asym_op->sm2.hash == RTE_CRYPTO_AUTH_SM3) {
		asym_op->sm2.message.data = input_params.message.data;
		asym_op->sm2.message.length = input_params.message.length;
		asym_op->sm2.id.data = input_params.id.data;
		asym_op->sm2.id.length = input_params.id.length;
	} else {
		asym_op->sm2.message.data = input_params.digest.data;
		asym_op->sm2.message.length = input_params.digest.length;
		asym_op->sm2.id.data = NULL;
		asym_op->sm2.id.length = 0;
	}

	if (rte_cryptodev_asym_xform_capability_check_opcap(capa,
			RTE_CRYPTO_ASYM_OP_ENCRYPT, RTE_CRYPTO_SM2_RNG)) {
		asym_op->sm2.k.data = NULL;
		asym_op->sm2.k.length = 0;
	} else {
		asym_op->sm2.k.data = input_params.k.data;
		asym_op->sm2.k.length = input_params.k.length;
	}

	/* Init out buf */
	asym_op->sm2.r.data = output_buf_r;
	asym_op->sm2.s.data = output_buf_s;

	RTE_LOG(DEBUG, USER1, "Process ASYM operation\n");

	/* Process crypto operation */
	if (rte_cryptodev_enqueue_burst(dev_id, 0, &op, 1) != 1) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Error sending packet for operation\n");
		status = TEST_FAILED;
		goto exit;
	}

	while (rte_cryptodev_dequeue_burst(dev_id, 0, &result_op, 1) == 0)
		rte_pause();

	if (result_op == NULL) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Failed to process asym crypto op\n");
		status = TEST_FAILED;
		goto exit;
	}

	if (result_op->status != RTE_CRYPTO_OP_STATUS_SUCCESS) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Failed to process asym crypto op\n");
		status = TEST_FAILED;
		goto exit;
	}

	asym_op = result_op->asym;

	debug_hexdump(stdout, "r:",
			asym_op->sm2.r.data, asym_op->sm2.r.length);
	debug_hexdump(stdout, "s:",
			asym_op->sm2.s.data, asym_op->sm2.s.length);

	if (!rte_cryptodev_asym_xform_capability_check_opcap(capa,
			RTE_CRYPTO_ASYM_OP_SIGN, RTE_CRYPTO_SM2_RNG)) {
		/* Verify sign (by comparison). */
		if (memcmp(input_params.sign_r.data, asym_op->sm2.r.data,
				   asym_op->sm2.r.length) != 0) {
			status = TEST_FAILED;
			RTE_LOG(ERR, USER1,
					"line %u FAILED: %s", __LINE__,
					"SM2 sign failed.\n");
			goto exit;
		}
		if (memcmp(input_params.sign_s.data, asym_op->sm2.s.data,
				   asym_op->sm2.s.length) != 0) {
			status = TEST_FAILED;
			RTE_LOG(ERR, USER1,
					"line %u FAILED: %s", __LINE__,
					"SM2 sign failed.\n");
			goto exit;
		}
	} else {
		/* Verify sign (in roundtrip).
		 * Due to random number used per message, sign op
		 * would produce different output for same message
		 * every time. Hence, we can't have expected output
		 * to match, instead reverse op to verify.
		 */

		/* Populate op with operational details */
		asym_op->sm2.op_type = RTE_CRYPTO_ASYM_OP_VERIFY;

		/* Enqueue sign result for verify */
		if (rte_cryptodev_enqueue_burst(dev_id, 0, &op, 1) != 1) {
			status = TEST_FAILED;
			RTE_LOG(ERR, USER1,
					"line %u FAILED: %s", __LINE__,
					"Error sending packet for operation\n");
			goto exit;
		}

		while (rte_cryptodev_dequeue_burst(dev_id, 0, &result_op, 1) == 0)
			rte_pause();

		if (result_op == NULL) {
			status = TEST_FAILED;
			goto exit;
		}
		if (result_op->status != RTE_CRYPTO_OP_STATUS_SUCCESS) {
			status = TEST_FAILED;
			RTE_LOG(ERR, USER1,
					"line %u FAILED: %s", __LINE__,
					"SM2 verify failed.\n");
			goto exit;
		}
	}

exit:
	if (sess != NULL)
		rte_cryptodev_asym_session_free(dev_id, sess);
	rte_crypto_op_free(op);
	return status;
};

static int
test_sm2_verify(void)
{
	struct crypto_testsuite_params_asym *ts_params = &testsuite_params;
	struct crypto_testsuite_sm2_params input_params = sm2_param_fp256;
	const struct rte_cryptodev_asymmetric_xform_capability *capa;
	struct rte_mempool *sess_mpool = ts_params->session_mpool;
	struct rte_mempool *op_mpool = ts_params->op_mpool;
	struct rte_cryptodev_asym_capability_idx idx;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_crypto_op *result_op = NULL;
	struct rte_crypto_asym_xform xform;
	struct rte_crypto_asym_op *asym_op;
	struct rte_crypto_op *op = NULL;
	int ret, status = TEST_SUCCESS;
	void *sess = NULL;

	/* Check SM2 capability */
	idx.type = RTE_CRYPTO_ASYM_XFORM_SM2;
	capa = rte_cryptodev_asym_capability_get(dev_id, &idx);
	if (capa == NULL)
		return -ENOTSUP;

	/* Setup crypto op data structure */
	op = rte_crypto_op_alloc(op_mpool, RTE_CRYPTO_OP_TYPE_ASYMMETRIC);
	if (op == NULL) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Failed to allocate asymmetric crypto "
				"operation struct\n");
		status = TEST_FAILED;
		goto exit;
	}

	asym_op = op->asym;

	/* Setup asym xform */
	xform.next = NULL;
	xform.xform_type = RTE_CRYPTO_ASYM_XFORM_SM2;
	xform.ec.curve_id = input_params.curve;
	xform.ec.pkey.data = input_params.pkey.data;
	xform.ec.pkey.length = input_params.pkey.length;
	xform.ec.q.x.data = input_params.pubkey_qx.data;
	xform.ec.q.x.length = input_params.pubkey_qx.length;
	xform.ec.q.y.data = input_params.pubkey_qy.data;
	xform.ec.q.y.length = input_params.pubkey_qy.length;

	ret = rte_cryptodev_asym_session_create(dev_id, &xform, sess_mpool, &sess);
	if (ret < 0) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Session creation failed\n");
		status = (ret == -ENOTSUP) ? TEST_SKIPPED : TEST_FAILED;
		goto exit;
	}

	/* Attach asymmetric crypto session to crypto operations */
	rte_crypto_op_attach_asym_session(op, sess);

	/* Verify given sign */

	/* Populate op with operational details */
	asym_op->sm2.op_type = RTE_CRYPTO_ASYM_OP_VERIFY;

	if (rte_cryptodev_asym_xform_capability_check_opcap(capa,
			RTE_CRYPTO_ASYM_OP_VERIFY, RTE_CRYPTO_SM2_PH))
		asym_op->sm2.hash = RTE_CRYPTO_AUTH_SM3;
	else
		asym_op->sm2.hash = RTE_CRYPTO_AUTH_NULL;

	if (asym_op->sm2.hash == RTE_CRYPTO_AUTH_SM3) {
		asym_op->sm2.message.data = input_params.message.data;
		asym_op->sm2.message.length = input_params.message.length;
		asym_op->sm2.id.data = input_params.id.data;
		asym_op->sm2.id.length = input_params.id.length;
	} else {
		asym_op->sm2.message.data = input_params.digest.data;
		asym_op->sm2.message.length = input_params.digest.length;
		asym_op->sm2.id.data = NULL;
		asym_op->sm2.id.length = 0;
	}

	asym_op->sm2.r.data = input_params.sign_r.data;
	asym_op->sm2.r.length = input_params.sign_r.length;
	asym_op->sm2.s.data = input_params.sign_s.data;
	asym_op->sm2.s.length = input_params.sign_s.length;

	RTE_LOG(DEBUG, USER1, "Process ASYM operation\n");

	/* Process crypto operation */
	if (rte_cryptodev_enqueue_burst(dev_id, 0, &op, 1) != 1) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Error sending packet for operation\n");
		status = TEST_FAILED;
		goto exit;
	}

	while (rte_cryptodev_dequeue_burst(dev_id, 0, &result_op, 1) == 0)
		rte_pause();

	if (result_op == NULL) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Failed to process asym crypto op\n");
		status = TEST_FAILED;
		goto exit;
	}

	if (result_op->status != RTE_CRYPTO_OP_STATUS_SUCCESS) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Failed to process asym crypto op\n");
		status = TEST_FAILED;
		goto exit;
	}

exit:
	if (sess != NULL)
		rte_cryptodev_asym_session_free(dev_id, sess);
	rte_crypto_op_free(op);
	return status;
};

static int
test_sm2_enc(void)
{
	struct crypto_testsuite_params_asym *ts_params = &testsuite_params;
	struct crypto_testsuite_sm2_params input_params = sm2_param_fp256;
	const struct rte_cryptodev_asymmetric_xform_capability *capa;
	struct rte_mempool *sess_mpool = ts_params->session_mpool;
	struct rte_mempool *op_mpool = ts_params->op_mpool;
	uint8_t output_buf[TEST_DATA_SIZE], *pbuf = NULL;
	struct rte_cryptodev_asym_capability_idx idx;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_crypto_op *result_op = NULL;
	struct rte_crypto_asym_xform xform;
	struct rte_crypto_asym_op *asym_op;
	struct rte_crypto_op *op = NULL;
	int ret, status = TEST_SUCCESS;
	void *sess = NULL;

	/* Check SM2 capability */
	idx.type = RTE_CRYPTO_ASYM_XFORM_SM2;
	capa = rte_cryptodev_asym_capability_get(dev_id, &idx);
	if (capa == NULL)
		return -ENOTSUP;

	/* Setup crypto op data structure */
	op = rte_crypto_op_alloc(op_mpool, RTE_CRYPTO_OP_TYPE_ASYMMETRIC);
	if (op == NULL) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Failed to allocate asymmetric crypto "
				"operation struct\n");
		status = TEST_FAILED;
		goto exit;
	}
	asym_op = op->asym;

	/* Setup asym xform */
	xform.next = NULL;
	xform.xform_type = RTE_CRYPTO_ASYM_XFORM_SM2;
	xform.ec.curve_id = input_params.curve;
	xform.ec.pkey.data = input_params.pkey.data;
	xform.ec.pkey.length = input_params.pkey.length;
	xform.ec.q.x.data = input_params.pubkey_qx.data;
	xform.ec.q.x.length = input_params.pubkey_qx.length;
	xform.ec.q.y.data = input_params.pubkey_qy.data;
	xform.ec.q.y.length = input_params.pubkey_qy.length;

	ret = rte_cryptodev_asym_session_create(dev_id, &xform, sess_mpool, &sess);
	if (ret < 0) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Session creation failed\n");
		status = (ret == -ENOTSUP) ? TEST_SKIPPED : TEST_FAILED;
		goto exit;
	}

	/* Attach asymmetric crypto session to crypto operations */
	rte_crypto_op_attach_asym_session(op, sess);

	/* Compute encrypt */

	/* Populate op with operational details */
	asym_op->sm2.op_type = RTE_CRYPTO_ASYM_OP_ENCRYPT;
	if (rte_cryptodev_asym_xform_capability_check_opcap(capa,
			RTE_CRYPTO_ASYM_OP_ENCRYPT, RTE_CRYPTO_SM2_PH))
		asym_op->sm2.hash = RTE_CRYPTO_AUTH_SM3;
	else
		asym_op->sm2.hash = RTE_CRYPTO_AUTH_NULL;

	asym_op->sm2.message.data = input_params.message.data;
	asym_op->sm2.message.length = input_params.message.length;

	if (rte_cryptodev_asym_xform_capability_check_opcap(capa,
			RTE_CRYPTO_ASYM_OP_ENCRYPT, RTE_CRYPTO_SM2_RNG)) {
		asym_op->sm2.k.data = NULL;
		asym_op->sm2.k.length = 0;
	} else {
		asym_op->sm2.k.data = input_params.k.data;
		asym_op->sm2.k.length = input_params.k.length;
	}

	/* Init out buf */
	asym_op->sm2.cipher.data = output_buf;

	RTE_LOG(DEBUG, USER1, "Process ASYM operation\n");

	/* Process crypto operation */
	if (rte_cryptodev_enqueue_burst(dev_id, 0, &op, 1) != 1) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Error sending packet for operation\n");
		status = TEST_FAILED;
		goto exit;
	}

	while (rte_cryptodev_dequeue_burst(dev_id, 0, &result_op, 1) == 0)
		rte_pause();

	if (result_op == NULL) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Failed to process asym crypto op\n");
		status = TEST_FAILED;
		goto exit;
	}

	if (result_op->status != RTE_CRYPTO_OP_STATUS_SUCCESS) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Failed to process asym crypto op\n");
		status = TEST_FAILED;
		goto exit;
	}

	asym_op = result_op->asym;

	debug_hexdump(stdout, "cipher:",
			asym_op->sm2.cipher.data, asym_op->sm2.cipher.length);

	if (!rte_cryptodev_asym_xform_capability_check_opcap(capa,
			RTE_CRYPTO_ASYM_OP_ENCRYPT, RTE_CRYPTO_SM2_RNG)) {
		if (memcmp(input_params.cipher.data, asym_op->sm2.cipher.data,
				   asym_op->sm2.cipher.length) != 0) {
			status = TEST_FAILED;
			RTE_LOG(ERR, USER1, "line %u FAILED: %s", __LINE__,
					"SM2 encrypt failed.\n");
			goto exit;
		}
	} else {
		/* Verify cipher (in roundtrip).
		 * Due to random number used per message, encrypt op
		 * would produce different output for same message
		 * every time. Hence, we can't have expected output
		 * to match, instead reverse op to decrypt.
		 */

		/* Populate op with operational details */
		op->asym->sm2.op_type = RTE_CRYPTO_ASYM_OP_DECRYPT;
		pbuf = rte_malloc(NULL, TEST_DATA_SIZE, 0);
		op->asym->sm2.message.data = pbuf;
		op->asym->sm2.message.length = TEST_DATA_SIZE;

		/* Enqueue cipher result for decrypt */
		if (rte_cryptodev_enqueue_burst(dev_id, 0, &op, 1) != 1) {
			status = TEST_FAILED;
			RTE_LOG(ERR, USER1,
					"line %u FAILED: %s", __LINE__,
					"Error sending packet for operation\n");
			goto exit;
		}

		while (rte_cryptodev_dequeue_burst(dev_id, 0, &result_op, 1) == 0)
			rte_pause();

		if (result_op == NULL) {
			status = TEST_FAILED;
			goto exit;
		}
		if (result_op->status != RTE_CRYPTO_OP_STATUS_SUCCESS) {
			status = TEST_FAILED;
			RTE_LOG(ERR, USER1,
					"line %u FAILED: %s", __LINE__,
					"SM2 encrypt failed.\n");
			goto exit;
		}

		asym_op = result_op->asym;
		if (memcmp(input_params.message.data, asym_op->sm2.message.data,
			       asym_op->sm2.message.length) != 0) {
			status = TEST_FAILED;
			RTE_LOG(ERR, USER1, "line %u FAILED: %s", __LINE__,
					"SM2 encrypt failed.\n");
			goto exit;
		}
	}
exit:
	rte_free(pbuf);

	if (sess != NULL)
		rte_cryptodev_asym_session_free(dev_id, sess);
	rte_crypto_op_free(op);
	return status;
};

static int
test_sm2_dec(void)
{
	struct crypto_testsuite_params_asym *ts_params = &testsuite_params;
	struct crypto_testsuite_sm2_params input_params = sm2_param_fp256;
	const struct rte_cryptodev_asymmetric_xform_capability *capa;
	struct rte_mempool *sess_mpool = ts_params->session_mpool;
	struct rte_mempool *op_mpool = ts_params->op_mpool;
	struct rte_cryptodev_asym_capability_idx idx;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_crypto_op *result_op = NULL;
	uint8_t output_buf_m[TEST_DATA_SIZE];
	struct rte_crypto_asym_xform xform;
	struct rte_crypto_asym_op *asym_op;
	struct rte_crypto_op *op = NULL;
	int ret, status = TEST_SUCCESS;
	void *sess = NULL;

	/* Check SM2 capability */
	idx.type = RTE_CRYPTO_ASYM_XFORM_SM2;
	capa = rte_cryptodev_asym_capability_get(dev_id, &idx);
	if (capa == NULL)
		return -ENOTSUP;

	/* Setup crypto op data structure */
	op = rte_crypto_op_alloc(op_mpool, RTE_CRYPTO_OP_TYPE_ASYMMETRIC);
	if (op == NULL) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Failed to allocate asymmetric crypto "
				"operation struct\n");
		status = TEST_FAILED;
		goto exit;
	}
	asym_op = op->asym;

	/* Setup asym xform */
	xform.next = NULL;
	xform.xform_type = RTE_CRYPTO_ASYM_XFORM_SM2;
	xform.ec.curve_id = input_params.curve;
	xform.ec.pkey.data = input_params.pkey.data;
	xform.ec.pkey.length = input_params.pkey.length;
	xform.ec.q.x.data = input_params.pubkey_qx.data;
	xform.ec.q.x.length = input_params.pubkey_qx.length;
	xform.ec.q.y.data = input_params.pubkey_qy.data;
	xform.ec.q.y.length = input_params.pubkey_qy.length;

	ret = rte_cryptodev_asym_session_create(dev_id, &xform, sess_mpool, &sess);
	if (ret < 0) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Session creation failed\n");
		status = (ret == -ENOTSUP) ? TEST_SKIPPED : TEST_FAILED;
		goto exit;
	}

	/* Attach asymmetric crypto session to crypto operations */
	rte_crypto_op_attach_asym_session(op, sess);

	/* Compute decrypt */

	/* Populate op with operational details */
	asym_op->sm2.op_type = RTE_CRYPTO_ASYM_OP_DECRYPT;
	if (rte_cryptodev_asym_xform_capability_check_opcap(capa,
			RTE_CRYPTO_ASYM_OP_DECRYPT, RTE_CRYPTO_SM2_PH))
		asym_op->sm2.hash = RTE_CRYPTO_AUTH_SM3;
	else
		asym_op->sm2.hash = RTE_CRYPTO_AUTH_NULL;

	asym_op->sm2.cipher.data = input_params.cipher.data;
	asym_op->sm2.cipher.length = input_params.cipher.length;

	/* Init out buf */
	asym_op->sm2.message.data = output_buf_m;
	asym_op->sm2.message.length = RTE_DIM(output_buf_m);

	RTE_LOG(DEBUG, USER1, "Process ASYM operation\n");

	/* Process crypto operation */
	if (rte_cryptodev_enqueue_burst(dev_id, 0, &op, 1) != 1) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Error sending packet for operation\n");
		status = TEST_FAILED;
		goto exit;
	}

	while (rte_cryptodev_dequeue_burst(dev_id, 0, &result_op, 1) == 0)
		rte_pause();

	if (result_op == NULL) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Failed to process asym crypto op\n");
		status = TEST_FAILED;
		goto exit;
	}

	if (result_op->status != RTE_CRYPTO_OP_STATUS_SUCCESS) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Failed to process asym crypto op\n");
		status = TEST_FAILED;
		goto exit;
	}

	asym_op = result_op->asym;

	debug_hexdump(stdout, "message:",
			asym_op->sm2.message.data, asym_op->sm2.message.length);

	if (memcmp(input_params.message.data, asym_op->sm2.message.data,
			op->asym->sm2.message.length)) {
		status = TEST_FAILED;
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"SM2 decrypt failed.\n");
		goto exit;
	}
exit:
	if (sess != NULL)
		rte_cryptodev_asym_session_free(dev_id, sess);
	rte_crypto_op_free(op);
	return status;
};

static int
test_eddsa_sign(struct crypto_testsuite_eddsa_params *input_params)
{
	struct crypto_testsuite_params_asym *ts_params = &testsuite_params;
	enum rte_crypto_edward_instance instance = input_params->instance;
	struct rte_mempool *sess_mpool = ts_params->session_mpool;
	struct rte_mempool *op_mpool = ts_params->op_mpool;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_crypto_op *result_op = NULL;
	uint8_t output_buf_r[TEST_DATA_SIZE];
	struct rte_crypto_asym_xform xform;
	struct rte_crypto_asym_op *asym_op;
	struct rte_crypto_op *op = NULL;
	int ret, status = TEST_FAILED;
	void *sess = NULL;
	bool ctx = false;

	if (instance == RTE_CRYPTO_EDCURVE_25519CTX)
		ctx = true;

	/* Setup crypto op data structure */
	op = rte_crypto_op_alloc(op_mpool, RTE_CRYPTO_OP_TYPE_ASYMMETRIC);
	if (op == NULL) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Failed to allocate asymmetric crypto "
				"operation struct\n");
		status = TEST_FAILED;
		goto exit;
	}

	asym_op = op->asym;

	/* Setup asym xform */
	xform.next = NULL;
	xform.xform_type = RTE_CRYPTO_ASYM_XFORM_EDDSA;
	xform.ec.curve_id = input_params->curve;
	xform.ec.pkey.data = input_params->pkey.data;
	xform.ec.pkey.length = input_params->pkey.length;
	xform.ec.q.x.data = input_params->pubkey.data;
	xform.ec.q.x.length = input_params->pubkey.length;

	ret = rte_cryptodev_asym_session_create(dev_id, &xform, sess_mpool, &sess);
	if (ret < 0) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Session creation failed\n");
		status = (ret == -ENOTSUP) ? TEST_SKIPPED : TEST_FAILED;
		goto exit;
	}

	/* Attach asymmetric crypto session to crypto operations */
	rte_crypto_op_attach_asym_session(op, sess);

	/* Compute sign */

	/* Populate op with operational details */
	asym_op->eddsa.op_type = RTE_CRYPTO_ASYM_OP_SIGN;
	asym_op->eddsa.instance = input_params->instance;
	asym_op->eddsa.message.data = input_params->message.data;
	asym_op->eddsa.message.length = input_params->message.length;
	asym_op->eddsa.context.length = 0;
	if (ctx) {
		asym_op->eddsa.context.data = input_params->context.data;
		asym_op->eddsa.context.length = input_params->context.length;
	}

	/* Init out buf */
	asym_op->eddsa.sign.data = output_buf_r;

	RTE_LOG(DEBUG, USER1, "Process ASYM operation\n");

	/* Process crypto operation */
	if (rte_cryptodev_enqueue_burst(dev_id, 0, &op, 1) != 1) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Error sending packet for operation\n");
		goto exit;
	}

	while (rte_cryptodev_dequeue_burst(dev_id, 0, &result_op, 1) == 0)
		rte_pause();

	if (result_op == NULL) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Failed to process asym crypto op\n");
		goto exit;
	}

	if (result_op->status != RTE_CRYPTO_OP_STATUS_SUCCESS) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Failed to process asym crypto op\n");
		goto exit;
	}

	asym_op = result_op->asym;

	debug_hexdump(stdout, "sign:",
			asym_op->eddsa.sign.data, asym_op->eddsa.sign.length);

	/* Verify sign (by comparison). */
	if (memcmp(input_params->sign.data, asym_op->eddsa.sign.data,
			   asym_op->eddsa.sign.length) != 0) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"EdDSA sign failed.\n");
		goto exit;
	}

	status = TEST_SUCCESS;
exit:
	if (sess != NULL)
		rte_cryptodev_asym_session_free(dev_id, sess);
	rte_crypto_op_free(op);
	return status;
};

static int
test_eddsa_verify(struct crypto_testsuite_eddsa_params *input_params)
{
	struct crypto_testsuite_params_asym *ts_params = &testsuite_params;
	enum rte_crypto_edward_instance instance = input_params->instance;
	struct rte_mempool *sess_mpool = ts_params->session_mpool;
	struct rte_mempool *op_mpool = ts_params->op_mpool;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_crypto_op *result_op = NULL;
	struct rte_crypto_asym_xform xform;
	struct rte_crypto_asym_op *asym_op;
	struct rte_crypto_op *op = NULL;
	int ret, status = TEST_FAILED;
	void *sess = NULL;
	bool ctx = false;

	if (instance == RTE_CRYPTO_EDCURVE_25519CTX)
		ctx = true;

	/* Setup crypto op data structure */
	op = rte_crypto_op_alloc(op_mpool, RTE_CRYPTO_OP_TYPE_ASYMMETRIC);
	if (op == NULL) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Failed to allocate asymmetric crypto "
				"operation struct\n");
		goto exit;
	}

	asym_op = op->asym;

	/* Setup asym xform */
	xform.next = NULL;
	xform.xform_type = RTE_CRYPTO_ASYM_XFORM_EDDSA;
	xform.ec.curve_id = input_params->curve;
	xform.ec.pkey.length = 0;
	xform.ec.q.x.data = input_params->pubkey.data;
	xform.ec.q.x.length = input_params->pubkey.length;

	ret = rte_cryptodev_asym_session_create(dev_id, &xform, sess_mpool, &sess);
	if (ret < 0) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Session creation failed\n");
		status = (ret == -ENOTSUP) ? TEST_SKIPPED : TEST_FAILED;
		goto exit;
	}

	/* Attach asymmetric crypto session to crypto operations */
	rte_crypto_op_attach_asym_session(op, sess);

	/* Compute sign */

	/* Populate op with operational details */
	asym_op->eddsa.op_type = RTE_CRYPTO_ASYM_OP_VERIFY;
	asym_op->eddsa.instance = input_params->instance;
	asym_op->eddsa.message.data = input_params->message.data;
	asym_op->eddsa.message.length = input_params->message.length;
	asym_op->eddsa.context.length = 0;
	if (ctx) {
		asym_op->eddsa.context.data = input_params->context.data;
		asym_op->eddsa.context.length = input_params->context.length;
	}

	asym_op->eddsa.sign.data = input_params->sign.data;
	asym_op->eddsa.sign.length = input_params->sign.length;

	RTE_LOG(DEBUG, USER1, "Process ASYM operation\n");

	/* Process crypto operation */
	if (rte_cryptodev_enqueue_burst(dev_id, 0, &op, 1) != 1) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Error sending packet for operation\n");
		goto exit;
	}

	while (rte_cryptodev_dequeue_burst(dev_id, 0, &result_op, 1) == 0)
		rte_pause();

	if (result_op == NULL) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Failed to process asym crypto op\n");
		goto exit;
	}

	if (result_op->status != RTE_CRYPTO_OP_STATUS_SUCCESS) {
		RTE_LOG(ERR, USER1,
				"line %u FAILED: %s", __LINE__,
				"Failed to process asym crypto op\n");
		goto exit;
	}

	status = TEST_SUCCESS;
exit:
	if (sess != NULL)
		rte_cryptodev_asym_session_free(dev_id, sess);
	rte_crypto_op_free(op);
	return status;
};

static int
test_eddsa_sign_verify_all_curve(void)
{
	struct crypto_testsuite_params_asym *ts_params = &testsuite_params;
	const struct rte_cryptodev_asymmetric_xform_capability *capa;
	struct crypto_testsuite_eddsa_params input_params;
	struct rte_cryptodev_asym_capability_idx idx;
	int status, overall_status = TEST_SUCCESS;
	uint8_t dev_id = ts_params->valid_devs[0];
	uint8_t i, tc = 0;
	const char *msg;

	/* Check EdDSA capability */
	idx.type = RTE_CRYPTO_ASYM_XFORM_EDDSA;
	capa = rte_cryptodev_asym_capability_get(dev_id, &idx);
	if (capa == NULL)
		return TEST_SKIPPED;

	/* Sign tests */
	for (i = 0; i < RTE_DIM(eddsa_test_params); i++) {
		memcpy(&input_params, &eddsa_test_params[i],
				sizeof(input_params));
		status = test_eddsa_sign(&input_params);
		if (status == TEST_SUCCESS) {
			msg = "succeeded";
		} else {
			msg = "failed";
			overall_status = status;
		}
		printf("  %u) TestCase Sign %s  %s\n",
		       tc++, input_params.description, msg);
	}

	/* Verify tests */
	for (i = 0; i < RTE_DIM(eddsa_test_params); i++) {
		memcpy(&input_params, &eddsa_test_params[i],
				sizeof(input_params));
		status = test_eddsa_verify(&input_params);
		if (status == TEST_SUCCESS) {
			msg = "succeeded";
		} else {
			msg = "failed";
			overall_status = status;
		}
		printf("  %u) TestCase Verify %s  %s\n",
		       tc++, input_params.description, msg);
	}

	/* Negative tests */
	memcpy(&input_params, &eddsa_test_params[1],
			sizeof(input_params));
	input_params.pubkey.data[0] ^= 0x01;

	status = test_eddsa_sign(&input_params);
	if (status == TEST_FAILED) {
		msg = "succeeded";
	} else {
		msg = "failed";
		overall_status = status;
	}
	printf("  %u) TestCase Negative Sign %s  %s\n",
			tc++, input_params.description, msg);

	status = test_eddsa_verify(&input_params);
	if (status == TEST_FAILED) {
		msg = "succeeded";
	} else {
		msg = "failed";
		overall_status = status;
	}
	printf("  %u) TestCase Negative Verify %s  %s\n",
			tc++, input_params.description, msg);

	return overall_status;
}

static int send_one(void)
{
	int ticks = 0;

	if (rte_cryptodev_enqueue_burst(params->valid_devs[0], 0,
			&self->op, 1) != 1) {
		RTE_LOG(ERR, USER1,
			"line %u FAILED: Error sending packet for operation on device %d",
			__LINE__, params->valid_devs[0]);
		return TEST_FAILED;
	}
	while (rte_cryptodev_dequeue_burst(params->valid_devs[0], 0,
			&self->result_op, 1) == 0) {
		rte_delay_ms(1);
		ticks++;
		if (ticks >= DEQ_TIMEOUT) {
			RTE_LOG(ERR, USER1,
				"line %u FAILED: Cannot dequeue the crypto op on device %d",
				__LINE__, params->valid_devs[0]);
			return TEST_FAILED;
		}
	}
	TEST_ASSERT_NOT_NULL(self->result_op,
		"Failed to process asym crypto op");
	TEST_ASSERT_SUCCESS(self->result_op->status,
		"Failed to process asym crypto op, error status received");
	return TEST_SUCCESS;
}

static int
modular_cmpeq(const uint8_t *a, size_t a_len, const uint8_t *b, size_t b_len)
{
	const uint8_t *new_a, *new_b;
	size_t i, j;

	/* Strip leading NUL bytes */
	for (i = 0; i < a_len; i++)
		if (a[i] != 0)
			break;

	for (j = 0; j < b_len; j++)
		if (b[j] != 0)
			break;

	if (a_len - i != b_len - j)
		return 1;

	new_a = &a[i];
	new_b = &b[j];
	if (memcmp(new_a, new_b, a_len - i))
		return 1;

	return 0;
}

static int
modular_exponentiation(const void *test_data)
{
	const struct modex_test_data *vector = test_data;
	uint8_t input[TEST_DATA_SIZE] = { 0 };
	uint8_t exponent[TEST_DATA_SIZE] = { 0 };
	uint8_t modulus[TEST_DATA_SIZE] = { 0 };
	uint8_t result[TEST_DATA_SIZE] = { 0 };
	struct rte_crypto_asym_xform xform = { };
	const uint8_t dev_id = params->valid_devs[0];

	memcpy(input, vector->base.data, vector->base.len);
	memcpy(exponent, vector->exponent.data, vector->exponent.len);
	memcpy(modulus, vector->modulus.data, vector->modulus.len);

	xform.xform_type = RTE_CRYPTO_ASYM_XFORM_MODEX;
	xform.modex.exponent.data = exponent;
	xform.modex.exponent.length = vector->exponent.len;
	xform.modex.modulus.data = modulus;
	xform.modex.modulus.length = vector->modulus.len;

	if (rte_cryptodev_asym_session_create(dev_id, &xform,
			params->session_mpool, &self->sess) < 0) {
		RTE_LOG(ERR, USER1, "line %u FAILED: Session creation failed",
			__LINE__);
		return TEST_FAILED;
	}
	rte_crypto_op_attach_asym_session(self->op, self->sess);
	self->op->asym->modex.base.data = input;
	self->op->asym->modex.base.length = vector->base.len;
	self->op->asym->modex.result.data = result;

	TEST_ASSERT_SUCCESS(send_one(),
		"Failed to process crypto op");
	TEST_ASSERT_SUCCESS(modular_cmpeq(vector->reminder.data, vector->reminder.len,
			self->result_op->asym->modex.result.data,
			self->result_op->asym->modex.result.length),
			"operation verification failed\n");

	return TEST_SUCCESS;
}

static int
modular_multiplicative_inverse(const void *test_data)
{
	const struct modinv_test_data *vector = test_data;
	uint8_t input[TEST_DATA_SIZE] = { 0 };
	uint8_t modulus[TEST_DATA_SIZE] = { 0 };
	uint8_t result[TEST_DATA_SIZE] = { 0 };
	struct rte_crypto_asym_xform xform = { };
	const uint8_t dev_id = params->valid_devs[0];

	memcpy(input, vector->base.data, vector->base.len);
	memcpy(modulus, vector->modulus.data, vector->modulus.len);
	xform.xform_type = RTE_CRYPTO_ASYM_XFORM_MODINV;
	xform.modex.modulus.data = modulus;
	xform.modex.modulus.length = vector->modulus.len;
	if (rte_cryptodev_asym_session_create(dev_id, &xform,
			params->session_mpool, &self->sess) < 0) {
		RTE_LOG(ERR, USER1, "line %u FAILED: Session creation failed",
			__LINE__);
		return TEST_FAILED;
	}
	rte_crypto_op_attach_asym_session(self->op, self->sess);

	self->op->asym->modinv.base.data = input;
	self->op->asym->modinv.base.length = vector->base.len;
	self->op->asym->modinv.result.data = result;
	self->op->asym->modinv.result.length = vector->modulus.len;

	TEST_ASSERT_SUCCESS(send_one(),
		"Failed to process crypto op");
	TEST_ASSERT_BUFFERS_ARE_EQUAL(vector->inverse.data,
		self->result_op->asym->modinv.result.data,
		self->result_op->asym->modinv.result.length,
		"Incorrect reminder\n");

	return TEST_SUCCESS;
}

#define SET_RSA_PARAM(arg, vector, coef) \
	uint8_t coef[TEST_DATA_SIZE] = { }; \
	memcpy(coef, vector->coef.data, vector->coef.len); \
	arg.coef.data = coef; \
	arg.coef.length = vector->coef.len

#define SET_RSA_PARAM_QT(arg, vector, coef) \
	uint8_t coef[TEST_DATA_SIZE] = { }; \
	memcpy(coef, vector->coef.data, vector->coef.len); \
	arg.qt.coef.data = coef; \
	arg.qt.coef.length = vector->coef.len

static int
rsa_encrypt(const struct rsa_test_data_2 *vector, uint8_t *cipher_buf)
{
	self->result_op = NULL;
	/* Compute encryption on the test vector */
	self->op->asym->rsa.op_type = RTE_CRYPTO_ASYM_OP_ENCRYPT;
	self->op->asym->rsa.cipher.data = cipher_buf;
	self->op->asym->rsa.cipher.length = 0;
	SET_RSA_PARAM(self->op->asym->rsa, vector, message);

	rte_crypto_op_attach_asym_session(self->op, self->sess);
	TEST_ASSERT_SUCCESS(send_one(),
		"Failed to process crypto op (Enryption)");

	return 0;
}

static int
rsa_decrypt(const struct rsa_test_data_2 *vector, uint8_t *plaintext,
		const int use_op)
{
	uint8_t cipher[TEST_DATA_SIZE] = { 0 };

	if (use_op == 0) {
		memcpy(cipher, vector->cipher.data, vector->cipher.len);
		self->op->asym->rsa.cipher.data = cipher;
		self->op->asym->rsa.cipher.length = vector->cipher.len;
	}
	self->result_op = NULL;
	self->op->asym->rsa.message.data = plaintext;
	self->op->asym->rsa.message.length = 0;
	self->op->asym->rsa.op_type = RTE_CRYPTO_ASYM_OP_DECRYPT;
	rte_crypto_op_attach_asym_session(self->op, self->sess);
	TEST_ASSERT_SUCCESS(send_one(),
		"Failed to process crypto op (Decryption)");
	return 0;
}

static int
rsa_init_session(struct rte_crypto_asym_xform *xform)
{
	const uint8_t dev_id = params->valid_devs[0];
	struct rte_cryptodev_info dev_info;
	int ret = 0;

	xform->xform_type = RTE_CRYPTO_ASYM_XFORM_RSA;

	rte_cryptodev_info_get(dev_id, &dev_info);
	if (!(dev_info.feature_flags & RTE_CRYPTODEV_FF_RSA_PRIV_OP_KEY_QT)) {
		RTE_LOG(INFO, USER1,
			"Device doesn't support decrypt op with quintuple key type. Test skipped\n");
		return TEST_SKIPPED;
	}
	ret = rte_cryptodev_asym_session_create(dev_id, xform,
		params->session_mpool, &self->sess);
	if (ret < 0) {
		RTE_LOG(ERR, USER1,
			"Session creation failed for enc_dec_crt\n");
		return (ret == -ENOTSUP) ? TEST_SKIPPED : TEST_FAILED;
	}
	return 0;
}

static int
kat_rsa_encrypt(const void *data)
{
	uint8_t cipher_buf[TEST_DATA_SIZE] = {0};
	const struct rsa_test_data_2 *vector = data;
	struct rte_crypto_asym_xform xform = { };

	SET_RSA_PARAM(xform.rsa, vector, n);
	SET_RSA_PARAM(xform.rsa, vector, e);
	SET_RSA_PARAM(xform.rsa, vector, d);
	xform.rsa.padding.type = vector->padding;
	xform.rsa.key_type = RTE_RSA_KEY_TYPE_EXP;
	int ret = rsa_init_session(&xform);

	if (ret) {
		RTE_LOG(ERR, USER1, "Failed to init session for RSA\n");
		return ret;
	}
	TEST_ASSERT_SUCCESS(rsa_encrypt(vector, cipher_buf),
		"RSA: Failed to encrypt");
	TEST_ASSERT_BUFFERS_ARE_EQUAL(vector->cipher.data,
		self->result_op->asym->rsa.cipher.data,
		self->result_op->asym->rsa.cipher.length,
		"operation verification failed\n");
	return 0;
}

static int
kat_rsa_encrypt_crt(const void *data)
{
	uint8_t cipher_buf[TEST_DATA_SIZE] = {0};
	const struct rsa_test_data_2 *vector = data;
	struct rte_crypto_asym_xform xform = { };

	SET_RSA_PARAM(xform.rsa, vector, n);
	SET_RSA_PARAM(xform.rsa, vector, e);
	SET_RSA_PARAM_QT(xform.rsa, vector, p);
	SET_RSA_PARAM_QT(xform.rsa, vector, q);
	SET_RSA_PARAM_QT(xform.rsa, vector, dP);
	SET_RSA_PARAM_QT(xform.rsa, vector, dQ);
	SET_RSA_PARAM_QT(xform.rsa, vector, qInv);
	xform.rsa.padding.type = vector->padding;
	xform.rsa.key_type = RTE_RSA_KEY_TYPE_QT;
	int ret = rsa_init_session(&xform);
	if (ret) {
		RTE_LOG(ERR, USER1, "Failed to init session for RSA\n");
		return ret;
	}
	TEST_ASSERT_SUCCESS(rsa_encrypt(vector, cipher_buf),
		"RSA: Failed to encrypt");
	TEST_ASSERT_BUFFERS_ARE_EQUAL(vector->cipher.data,
		self->result_op->asym->rsa.cipher.data,
		self->result_op->asym->rsa.cipher.length,
		"operation verification failed\n");
	return 0;
}

static int
kat_rsa_decrypt(const void *data)
{
	uint8_t message[TEST_DATA_SIZE] = {0};
	const struct rsa_test_data_2 *vector = data;
	struct rte_crypto_asym_xform xform = { };

	SET_RSA_PARAM(xform.rsa, vector, n);
	SET_RSA_PARAM(xform.rsa, vector, e);
	SET_RSA_PARAM(xform.rsa, vector, d);
	xform.rsa.padding.type = vector->padding;
	xform.rsa.key_type = RTE_RSA_KEY_TYPE_EXP;
	int ret = rsa_init_session(&xform);

	if (ret) {
		RTE_LOG(ERR, USER1, "Failed to init session for RSA\n");
		return ret;
	}
	TEST_ASSERT_SUCCESS(rsa_decrypt(vector, message, 0),
		"RSA: Failed to encrypt");
	TEST_ASSERT_BUFFERS_ARE_EQUAL(vector->message.data,
		self->result_op->asym->rsa.message.data,
		self->result_op->asym->rsa.message.length,
		"operation verification failed\n");
	return 0;
}

static int
kat_rsa_decrypt_crt(const void *data)
{
	uint8_t message[TEST_DATA_SIZE] = {0};
	const struct rsa_test_data_2 *vector = data;
	struct rte_crypto_asym_xform xform = { };

	SET_RSA_PARAM(xform.rsa, vector, n);
	SET_RSA_PARAM(xform.rsa, vector, e);
	SET_RSA_PARAM_QT(xform.rsa, vector, p);
	SET_RSA_PARAM_QT(xform.rsa, vector, q);
	SET_RSA_PARAM_QT(xform.rsa, vector, dP);
	SET_RSA_PARAM_QT(xform.rsa, vector, dQ);
	SET_RSA_PARAM_QT(xform.rsa, vector, qInv);
	xform.rsa.padding.type = vector->padding;
	xform.rsa.key_type = RTE_RSA_KEY_TYPE_QT;
	int ret = rsa_init_session(&xform);
	if (ret) {
		RTE_LOG(ERR, USER1, "Failed to init session for RSA\n");
		return ret;
	}
	TEST_ASSERT_SUCCESS(rsa_decrypt(vector, message, 0),
		"RSA: Failed to encrypt");
	TEST_ASSERT_BUFFERS_ARE_EQUAL(vector->message.data,
		self->result_op->asym->rsa.message.data,
		self->result_op->asym->rsa.message.length,
		"operation verification failed\n");
	return 0;
}

static int
test_sm2_partial_encryption(const void *data)
{
	struct rte_crypto_asym_xform xform = { 0 };
	const uint8_t dev_id = params->valid_devs[0];
	const struct crypto_testsuite_sm2_params *test_vector = data;
	uint8_t result_C1_x1[TEST_DATA_SIZE] = { 0 };
	uint8_t result_C1_y1[TEST_DATA_SIZE] = { 0 };
	uint8_t result_kP_x1[TEST_DATA_SIZE] = { 0 };
	uint8_t result_kP_y1[TEST_DATA_SIZE] = { 0 };
	struct rte_cryptodev_asym_capability_idx idx;
	const struct rte_cryptodev_asymmetric_xform_capability *capa;

	idx.type = RTE_CRYPTO_ASYM_XFORM_SM2;
	capa = rte_cryptodev_asym_capability_get(dev_id, &idx);
	if (capa == NULL)
		return TEST_SKIPPED;
	if (!rte_cryptodev_asym_xform_capability_check_opcap(capa,
			RTE_CRYPTO_ASYM_OP_ENCRYPT, RTE_CRYPTO_SM2_PARTIAL)) {
		return TEST_SKIPPED;
	}

	xform.xform_type = RTE_CRYPTO_ASYM_XFORM_SM2;
	xform.ec.curve_id = RTE_CRYPTO_EC_GROUP_SM2;
	xform.ec.q = test_vector->pubkey;
	self->op->asym->sm2.op_type = RTE_CRYPTO_ASYM_OP_ENCRYPT;
	self->op->asym->sm2.k = test_vector->k;
	if (rte_cryptodev_asym_session_create(dev_id, &xform,
			params->session_mpool, &self->sess) < 0) {
		RTE_LOG(ERR, USER1, "line %u FAILED: Session creation failed",
			__LINE__);
		return TEST_FAILED;
	}
	rte_crypto_op_attach_asym_session(self->op, self->sess);

	self->op->asym->sm2.c1.x.data = result_C1_x1;
	self->op->asym->sm2.c1.y.data = result_C1_y1;
	self->op->asym->sm2.kp.x.data = result_kP_x1;
	self->op->asym->sm2.kp.y.data = result_kP_y1;
	TEST_ASSERT_SUCCESS(send_one(),
		"Failed to process crypto op");

	debug_hexdump(stdout, "C1[x]", self->op->asym->sm2.c1.x.data,
		self->op->asym->sm2.c1.x.length);
	debug_hexdump(stdout, "C1[y]", self->op->asym->sm2.c1.y.data,
		self->op->asym->sm2.c1.y.length);
	debug_hexdump(stdout, "kP[x]", self->op->asym->sm2.kp.x.data,
		self->op->asym->sm2.kp.x.length);
	debug_hexdump(stdout, "kP[y]", self->op->asym->sm2.kp.y.data,
		self->op->asym->sm2.kp.y.length);

	TEST_ASSERT_BUFFERS_ARE_EQUAL(test_vector->C1.x.data,
		self->op->asym->sm2.c1.x.data,
		test_vector->C1.x.length,
		"Incorrect value of C1[x]\n");
	TEST_ASSERT_BUFFERS_ARE_EQUAL(test_vector->C1.y.data,
		self->op->asym->sm2.c1.y.data,
		test_vector->C1.y.length,
		"Incorrect value of C1[y]\n");
	TEST_ASSERT_BUFFERS_ARE_EQUAL(test_vector->kP.x.data,
		self->op->asym->sm2.kp.x.data,
		test_vector->kP.x.length,
		"Incorrect value of kP[x]\n");
	TEST_ASSERT_BUFFERS_ARE_EQUAL(test_vector->kP.y.data,
		self->op->asym->sm2.kp.y.data,
		test_vector->kP.y.length,
		"Incorrect value of kP[y]\n");

	return TEST_SUCCESS;
}

static int
test_sm2_partial_decryption(const void *data)
{
	struct rte_crypto_asym_xform xform = {};
	const uint8_t dev_id = params->valid_devs[0];
	const struct crypto_testsuite_sm2_params *test_vector = data;
	uint8_t result_kP_x1[TEST_DATA_SIZE] = { 0 };
	uint8_t result_kP_y1[TEST_DATA_SIZE] = { 0 };
	struct rte_cryptodev_asym_capability_idx idx;
	const struct rte_cryptodev_asymmetric_xform_capability *capa;

	idx.type = RTE_CRYPTO_ASYM_XFORM_SM2;
	capa = rte_cryptodev_asym_capability_get(dev_id, &idx);
	if (capa == NULL)
		return TEST_SKIPPED;
	if (!rte_cryptodev_asym_xform_capability_check_opcap(capa,
			RTE_CRYPTO_ASYM_OP_DECRYPT, RTE_CRYPTO_SM2_PARTIAL)) {
		return TEST_SKIPPED;
	}

	xform.xform_type = RTE_CRYPTO_ASYM_XFORM_SM2;
	xform.ec.pkey = test_vector->pkey;
	self->op->asym->sm2.op_type = RTE_CRYPTO_ASYM_OP_DECRYPT;
	self->op->asym->sm2.c1 = test_vector->C1;

	if (rte_cryptodev_asym_session_create(dev_id, &xform,
			params->session_mpool, &self->sess) < 0) {
		RTE_LOG(ERR, USER1, "line %u FAILED: Session creation failed",
			__LINE__);
		return TEST_FAILED;
	}
	rte_crypto_op_attach_asym_session(self->op, self->sess);

	self->op->asym->sm2.kp.x.data = result_kP_x1;
	self->op->asym->sm2.kp.y.data = result_kP_y1;
	TEST_ASSERT_SUCCESS(send_one(),
		"Failed to process crypto op");

	debug_hexdump(stdout, "kP[x]", self->op->asym->sm2.kp.x.data,
		self->op->asym->sm2.c1.x.length);
	debug_hexdump(stdout, "kP[y]", self->op->asym->sm2.kp.y.data,
		self->op->asym->sm2.c1.y.length);

	TEST_ASSERT_BUFFERS_ARE_EQUAL(test_vector->kP.x.data,
		self->op->asym->sm2.kp.x.data,
		test_vector->kP.x.length,
		"Incorrect value of kP[x]\n");
	TEST_ASSERT_BUFFERS_ARE_EQUAL(test_vector->kP.y.data,
		self->op->asym->sm2.kp.y.data,
		test_vector->kP.y.length,
		"Incorrect value of kP[y]\n");

	return 0;
}

static struct unit_test_suite cryptodev_openssl_asym_testsuite  = {
	.suite_name = "Crypto Device OPENSSL ASYM Unit Test Suite",
	.setup = testsuite_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {
		TEST_CASE_ST(ut_setup_asym, ut_teardown_asym, test_capability),
		TEST_CASE_ST(ut_setup_asym, ut_teardown_asym, test_dsa),
		TEST_CASE_ST(ut_setup_asym, ut_teardown_asym,
				test_dh_key_generation),
		TEST_CASE_ST(ut_setup_asym, ut_teardown_asym, test_sm2_sign),
		TEST_CASE_ST(ut_setup_asym, ut_teardown_asym, test_sm2_verify),
		TEST_CASE_ST(ut_setup_asym, ut_teardown_asym, test_sm2_enc),
		TEST_CASE_ST(ut_setup_asym, ut_teardown_asym, test_sm2_dec),
		TEST_CASE_ST(ut_setup_asym, ut_teardown_asym, test_rsa_enc_dec),
		TEST_CASE_ST(ut_setup_asym, ut_teardown_asym,
				test_rsa_sign_verify),
		TEST_CASE_ST(ut_setup_asym, ut_teardown_asym,
				test_rsa_enc_dec_crt),
		TEST_CASE_ST(ut_setup_asym, ut_teardown_asym,
				test_rsa_sign_verify_crt),
		TEST_CASE_ST(ut_setup_asym, ut_teardown_asym, test_mod_inv),
		TEST_CASE_ST(ut_setup_asym, ut_teardown_asym, test_mod_exp),
		TEST_CASE_NAMED_WITH_DATA(
			"Modex test for zero padding",
			ut_setup_asym, ut_teardown_asym,
			modular_exponentiation, &modex_test_cases[0]),
		TEST_CASE_NAMED_WITH_DATA(
			"Modex test for zero padding (2)",
			ut_setup_asym, ut_teardown_asym,
			modular_exponentiation, &modex_test_cases[1]),
		TEST_CASE_NAMED_WITH_DATA(
			"Modex Group 5 test",
			ut_setup_asym, ut_teardown_asym,
			modular_exponentiation, &modex_group_test_cases[0]),
		TEST_CASE_NAMED_WITH_DATA(
			"Modex Group 14 test",
			ut_setup_asym, ut_teardown_asym,
			modular_exponentiation, &modex_group_test_cases[1]),
		TEST_CASE_NAMED_WITH_DATA(
			"Modex Group 15 test",
			ut_setup_asym, ut_teardown_asym,
			modular_exponentiation, &modex_group_test_cases[2]),
		TEST_CASE_NAMED_WITH_DATA(
			"Modex Group 16 test",
			ut_setup_asym, ut_teardown_asym,
			modular_exponentiation, &modex_group_test_cases[3]),
		TEST_CASE_NAMED_WITH_DATA(
			"Modex Group 17 test",
			ut_setup_asym, ut_teardown_asym,
			modular_exponentiation, &modex_group_test_cases[4]),
		TEST_CASE_NAMED_WITH_DATA(
			"Modex Group 18 test",
			ut_setup_asym, ut_teardown_asym,
			modular_exponentiation, &modex_group_test_cases[5]),
		TEST_CASE_NAMED_WITH_DATA(
			"Modex Group 24 test",
			ut_setup_asym, ut_teardown_asym,
			modular_exponentiation, &modex_group_test_cases[6]),
		TEST_CASE_NAMED_WITH_DATA(
			"Modex Group 24 subgroup test",
			ut_setup_asym, ut_teardown_asym,
			modular_exponentiation, &modex_group_test_cases[7]),
		TEST_CASE_ST(ut_setup_asym, ut_teardown_asym, test_eddsa_sign_verify_all_curve),
		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static struct unit_test_suite cryptodev_qat_asym_testsuite  = {
	.suite_name = "Crypto Device QAT ASYM Unit Test Suite",
	.setup = testsuite_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {
		TEST_CASE_NAMED_WITH_DATA(
			"SM2 encryption - test case 1",
			ut_setup_asym, ut_teardown_asym,
			test_sm2_partial_encryption, &sm2_enc_hw_t1),
		TEST_CASE_NAMED_WITH_DATA(
			"SM2 decryption - test case 1",
			ut_setup_asym, ut_teardown_asym,
			test_sm2_partial_decryption, &sm2_enc_hw_t1),
		TEST_CASE_NAMED_WITH_DATA(
			"Modular Exponentiation (mod=128, base=20, exp=3, res=128)",
			ut_setup_asym, ut_teardown_asym,
			modular_exponentiation, &modex_test_case_m128_b20_e3),
		/* Modular Multiplicative Inverse */
		TEST_CASE_NAMED_WITH_DATA(
			"Modular Inverse (mod=128, base=20, exp=3, inv=128)",
			ut_setup_asym, ut_teardown_asym,
			modular_multiplicative_inverse, &modinv_test_case),
		/* RSA EXP */
		TEST_CASE_NAMED_WITH_DATA(
			"RSA Encryption (n=128, pt=20, e=3) EXP, Padding: NONE",
			ut_setup_asym, ut_teardown_asym,
			kat_rsa_encrypt, &rsa_vector_128_20_3_none),
		TEST_CASE_NAMED_WITH_DATA(
			"RSA Decryption (n=128, pt=20, e=3) EXP, Padding: NONE",
			ut_setup_asym, ut_teardown_asym,
			kat_rsa_decrypt, &rsa_vector_128_20_3_none),
		/* RSA CRT */
		TEST_CASE_NAMED_WITH_DATA(
			"RSA Encryption (n=128, pt=20, e=3) CRT, Padding: NONE",
			ut_setup_asym, ut_teardown_asym,
			kat_rsa_encrypt_crt, &rsa_vector_128_20_3_none),
		TEST_CASE_NAMED_WITH_DATA(
			"RSA Decryption (n=128, pt=20, e=3) CRT, Padding: NONE",
			ut_setup_asym, ut_teardown_asym,
			kat_rsa_decrypt_crt, &rsa_vector_128_20_3_none),
		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static struct unit_test_suite cryptodev_octeontx_asym_testsuite  = {
	.suite_name = "Crypto Device OCTEONTX ASYM Unit Test Suite",
	.setup = testsuite_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {
		TEST_CASE_ST(ut_setup_asym, ut_teardown_asym, test_capability),
		TEST_CASE_ST(ut_setup_asym, ut_teardown_asym,
				test_rsa_enc_dec_crt),
		TEST_CASE_ST(ut_setup_asym, ut_teardown_asym,
				test_rsa_sign_verify_crt),
		TEST_CASE_ST(ut_setup_asym, ut_teardown_asym, test_rsa_enc_dec),
		TEST_CASE_ST(ut_setup_asym, ut_teardown_asym,
				test_rsa_sign_verify),
		TEST_CASE_ST(ut_setup_asym, ut_teardown_asym, test_mod_exp),
		TEST_CASE_NAMED_WITH_DATA(
			"Modex test for zero padding",
			ut_setup_asym, ut_teardown_asym,
			modular_exponentiation, &modex_test_cases[0]),
		TEST_CASE_NAMED_WITH_DATA(
			"Modex test for zero padding (2)",
			ut_setup_asym, ut_teardown_asym,
			modular_exponentiation, &modex_test_cases[1]),
		TEST_CASE_NAMED_WITH_DATA(
			"Modex Group 5 test",
			ut_setup_asym, ut_teardown_asym,
			modular_exponentiation, &modex_group_test_cases[0]),
		TEST_CASE_NAMED_WITH_DATA(
			"Modex Group 14 test",
			ut_setup_asym, ut_teardown_asym,
			modular_exponentiation, &modex_group_test_cases[1]),
		TEST_CASE_NAMED_WITH_DATA(
			"Modex Group 15 test",
			ut_setup_asym, ut_teardown_asym,
			modular_exponentiation, &modex_group_test_cases[2]),
		TEST_CASE_NAMED_WITH_DATA(
			"Modex Group 16 test",
			ut_setup_asym, ut_teardown_asym,
			modular_exponentiation, &modex_group_test_cases[3]),
		TEST_CASE_NAMED_WITH_DATA(
			"Modex Group 17 test",
			ut_setup_asym, ut_teardown_asym,
			modular_exponentiation, &modex_group_test_cases[4]),
		TEST_CASE_NAMED_WITH_DATA(
			"Modex Group 18 test",
			ut_setup_asym, ut_teardown_asym,
			modular_exponentiation, &modex_group_test_cases[5]),
		TEST_CASE_NAMED_WITH_DATA(
			"Modex Group 24 test",
			ut_setup_asym, ut_teardown_asym,
			modular_exponentiation, &modex_group_test_cases[6]),
		TEST_CASE_NAMED_WITH_DATA(
			"Modex Group 24 subgroup test",
			ut_setup_asym, ut_teardown_asym,
			modular_exponentiation, &modex_group_test_cases[7]),
		TEST_CASE_ST(ut_setup_asym, ut_teardown_asym,
			     test_ecdsa_sign_verify_all_curve),
		TEST_CASE_ST(ut_setup_asym, ut_teardown_asym, test_sm2_sign),
		TEST_CASE_ST(ut_setup_asym, ut_teardown_asym, test_sm2_verify),
		TEST_CASE_ST(ut_setup_asym, ut_teardown_asym,
				test_ecdh_all_curve),
		TEST_CASE_ST(ut_setup_asym, ut_teardown_asym,
				test_ecpm_all_curve),
		TEST_CASE_ST(ut_setup_asym, ut_teardown_asym, test_eddsa_sign_verify_all_curve),
		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static struct unit_test_suite cryptodev_virtio_asym_testsuite  = {
	.suite_name = "Crypto Device VIRTIO ASYM Unit Test Suite",
	.setup = testsuite_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {
		TEST_CASE_ST(ut_setup_asym, ut_teardown_asym, test_capability),
		TEST_CASE_ST(ut_setup_asym, ut_teardown_asym,
				test_rsa_sign_verify_crt),
		TEST_CASE_ST(ut_setup_asym, ut_teardown_asym, test_rsa_enc_dec_crt),
		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static int
test_cryptodev_openssl_asym(void)
{
	gbl_driver_id = rte_cryptodev_driver_id_get(
			RTE_STR(CRYPTODEV_NAME_OPENSSL_PMD));

	if (gbl_driver_id == -1) {
		RTE_LOG(ERR, USER1, "OPENSSL PMD must be loaded.\n");
		return TEST_SKIPPED;
	}

	return unit_test_suite_runner(&cryptodev_openssl_asym_testsuite);
}

static int
test_cryptodev_qat_asym(void)
{
	gbl_driver_id = rte_cryptodev_driver_id_get(
			RTE_STR(CRYPTODEV_NAME_QAT_ASYM_PMD));

	if (gbl_driver_id == -1) {
		RTE_LOG(ERR, USER1, "QAT PMD must be loaded.\n");
		return TEST_SKIPPED;
	}

	return unit_test_suite_runner(&cryptodev_qat_asym_testsuite);
}

static int
test_cryptodev_octeontx_asym(void)
{
	gbl_driver_id = rte_cryptodev_driver_id_get(
			RTE_STR(CRYPTODEV_NAME_OCTEONTX_SYM_PMD));
	if (gbl_driver_id == -1) {
		RTE_LOG(ERR, USER1, "OCTEONTX PMD must be loaded.\n");
		return TEST_SKIPPED;
	}
	return unit_test_suite_runner(&cryptodev_octeontx_asym_testsuite);
}

static int
test_cryptodev_cn9k_asym(void)
{
	gbl_driver_id = rte_cryptodev_driver_id_get(
			RTE_STR(CRYPTODEV_NAME_CN9K_PMD));
	if (gbl_driver_id == -1) {
		RTE_LOG(ERR, USER1, "CN9K PMD must be loaded.\n");
		return TEST_SKIPPED;
	}

	/* Use test suite registered for crypto_octeontx PMD */
	return unit_test_suite_runner(&cryptodev_octeontx_asym_testsuite);
}

static int
test_cryptodev_cn10k_asym(void)
{
	gbl_driver_id = rte_cryptodev_driver_id_get(
			RTE_STR(CRYPTODEV_NAME_CN10K_PMD));
	if (gbl_driver_id == -1) {
		RTE_LOG(ERR, USER1, "CN10K PMD must be loaded.\n");
		return TEST_SKIPPED;
	}

	/* Use test suite registered for crypto_octeontx PMD */
	return unit_test_suite_runner(&cryptodev_octeontx_asym_testsuite);
}

static int
test_cryptodev_virtio_asym(void)
{
	gbl_driver_id = rte_cryptodev_driver_id_get(
			RTE_STR(CRYPTODEV_NAME_VIRTIO_PMD));
	if (gbl_driver_id == -1) {
		RTE_LOG(ERR, USER1, "virtio PMD must be loaded.\n");
		return TEST_SKIPPED;
	}

	/* Use test suite registered for crypto_virtio PMD */
	return unit_test_suite_runner(&cryptodev_virtio_asym_testsuite);
}

static int
test_cryptodev_virtio_user_asym(void)
{
	gbl_driver_id = rte_cryptodev_driver_id_get(
			RTE_STR(CRYPTODEV_NAME_VIRTIO_USER_PMD));
	if (gbl_driver_id == -1) {
		RTE_LOG(ERR, USER1, "virtio user PMD must be loaded.\n");
		return TEST_SKIPPED;
	}

	/* Use test suite registered for crypto_virtio_user PMD */
	return unit_test_suite_runner(&cryptodev_virtio_asym_testsuite);
}

REGISTER_DRIVER_TEST(cryptodev_openssl_asym_autotest, test_cryptodev_openssl_asym);
REGISTER_DRIVER_TEST(cryptodev_qat_asym_autotest, test_cryptodev_qat_asym);
REGISTER_DRIVER_TEST(cryptodev_octeontx_asym_autotest, test_cryptodev_octeontx_asym);
REGISTER_DRIVER_TEST(cryptodev_cn9k_asym_autotest, test_cryptodev_cn9k_asym);
REGISTER_DRIVER_TEST(cryptodev_cn10k_asym_autotest, test_cryptodev_cn10k_asym);
REGISTER_DRIVER_TEST(cryptodev_virtio_asym_autotest, test_cryptodev_virtio_asym);
REGISTER_DRIVER_TEST(cryptodev_virtio_user_asym_autotest, test_cryptodev_virtio_user_asym);
