/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2018 Intel Corporation
 */

#include <rte_common.h>
#include <rte_hexdump.h>
#include <rte_cryptodev.h>
#include <rte_cryptodev_pmd.h>
#include <rte_bus_vdev.h>
#include <rte_malloc.h>
#include <rte_cpuflags.h>

#include "snow3g_pmd_private.h"

#define SNOW3G_IV_LENGTH 16
#define SNOW3G_MAX_BURST 8
#define BYTE_LEN 8

static uint8_t cryptodev_driver_id;

/** Get xform chain order. */
static enum snow3g_operation
snow3g_get_mode(const struct rte_crypto_sym_xform *xform)
{
	if (xform == NULL)
		return SNOW3G_OP_NOT_SUPPORTED;

	if (xform->next)
		if (xform->next->next != NULL)
			return SNOW3G_OP_NOT_SUPPORTED;

	if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH) {
		if (xform->next == NULL)
			return SNOW3G_OP_ONLY_AUTH;
		else if (xform->next->type == RTE_CRYPTO_SYM_XFORM_CIPHER)
			return SNOW3G_OP_AUTH_CIPHER;
		else
			return SNOW3G_OP_NOT_SUPPORTED;
	}

	if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER) {
		if (xform->next == NULL)
			return SNOW3G_OP_ONLY_CIPHER;
		else if (xform->next->type == RTE_CRYPTO_SYM_XFORM_AUTH)
			return SNOW3G_OP_CIPHER_AUTH;
		else
			return SNOW3G_OP_NOT_SUPPORTED;
	}

	return SNOW3G_OP_NOT_SUPPORTED;
}


/** Parse crypto xform chain and set private session parameters. */
int
snow3g_set_session_parameters(MB_MGR *mgr, struct snow3g_session *sess,
		const struct rte_crypto_sym_xform *xform)
{
	const struct rte_crypto_sym_xform *auth_xform = NULL;
	const struct rte_crypto_sym_xform *cipher_xform = NULL;
	enum snow3g_operation mode;

	/* Select Crypto operation - hash then cipher / cipher then hash */
	mode = snow3g_get_mode(xform);

	switch (mode) {
	case SNOW3G_OP_CIPHER_AUTH:
		auth_xform = xform->next;

		/* Fall-through */
	case SNOW3G_OP_ONLY_CIPHER:
		cipher_xform = xform;
		break;
	case SNOW3G_OP_AUTH_CIPHER:
		cipher_xform = xform->next;
		/* Fall-through */
	case SNOW3G_OP_ONLY_AUTH:
		auth_xform = xform;
		break;
	case SNOW3G_OP_NOT_SUPPORTED:
	default:
		SNOW3G_LOG(ERR, "Unsupported operation chain order parameter");
		return -ENOTSUP;
	}

	if (cipher_xform) {
		/* Only SNOW 3G UEA2 supported */
		if (cipher_xform->cipher.algo != RTE_CRYPTO_CIPHER_SNOW3G_UEA2)
			return -ENOTSUP;

		if (cipher_xform->cipher.iv.length != SNOW3G_IV_LENGTH) {
			SNOW3G_LOG(ERR, "Wrong IV length");
			return -EINVAL;
		}
		if (cipher_xform->cipher.key.length > SNOW3G_MAX_KEY_SIZE) {
			SNOW3G_LOG(ERR, "Not enough memory to store the key");
			return -ENOMEM;
		}

		sess->cipher_iv_offset = cipher_xform->cipher.iv.offset;

		/* Initialize key */
		IMB_SNOW3G_INIT_KEY_SCHED(mgr, cipher_xform->cipher.key.data,
					&sess->pKeySched_cipher);
	}

	if (auth_xform) {
		/* Only SNOW 3G UIA2 supported */
		if (auth_xform->auth.algo != RTE_CRYPTO_AUTH_SNOW3G_UIA2)
			return -ENOTSUP;

		if (auth_xform->auth.digest_length != SNOW3G_DIGEST_LENGTH) {
			SNOW3G_LOG(ERR, "Wrong digest length");
			return -EINVAL;
		}
		if (auth_xform->auth.key.length > SNOW3G_MAX_KEY_SIZE) {
			SNOW3G_LOG(ERR, "Not enough memory to store the key");
			return -ENOMEM;
		}

		sess->auth_op = auth_xform->auth.op;

		if (auth_xform->auth.iv.length != SNOW3G_IV_LENGTH) {
			SNOW3G_LOG(ERR, "Wrong IV length");
			return -EINVAL;
		}
		sess->auth_iv_offset = auth_xform->auth.iv.offset;

		/* Initialize key */
		IMB_SNOW3G_INIT_KEY_SCHED(mgr, auth_xform->auth.key.data,
					&sess->pKeySched_hash);
	}

	sess->op = mode;

	return 0;
}

/** Get SNOW 3G session. */
static struct snow3g_session *
snow3g_get_session(struct snow3g_qp *qp, struct rte_crypto_op *op)
{
	struct snow3g_session *sess = NULL;

	if (op->sess_type == RTE_CRYPTO_OP_WITH_SESSION) {
		if (likely(op->sym->session != NULL))
			sess = (struct snow3g_session *)
					get_sym_session_private_data(
					op->sym->session,
					cryptodev_driver_id);
	} else {
		void *_sess = NULL;
		void *_sess_private_data = NULL;

		if (rte_mempool_get(qp->sess_mp, (void **)&_sess))
			return NULL;

		if (rte_mempool_get(qp->sess_mp_priv,
				(void **)&_sess_private_data))
			return NULL;

		sess = (struct snow3g_session *)_sess_private_data;

		if (unlikely(snow3g_set_session_parameters(qp->mgr, sess,
				op->sym->xform) != 0)) {
			rte_mempool_put(qp->sess_mp, _sess);
			rte_mempool_put(qp->sess_mp_priv, _sess_private_data);
			sess = NULL;
		}
		op->sym->session = (struct rte_cryptodev_sym_session *)_sess;
		set_sym_session_private_data(op->sym->session,
				cryptodev_driver_id, _sess_private_data);
	}

	if (unlikely(sess == NULL))
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_SESSION;


	return sess;
}

/** Encrypt/decrypt mbufs with same cipher key. */
static uint8_t
process_snow3g_cipher_op(struct snow3g_qp *qp, struct rte_crypto_op **ops,
		struct snow3g_session *session,
		uint8_t num_ops)
{
	unsigned i;
	uint8_t processed_ops = 0;
	const void *src[SNOW3G_MAX_BURST];
	void *dst[SNOW3G_MAX_BURST];
	const void *iv[SNOW3G_MAX_BURST];
	uint32_t num_bytes[SNOW3G_MAX_BURST];

	for (i = 0; i < num_ops; i++) {
		src[i] = rte_pktmbuf_mtod(ops[i]->sym->m_src, uint8_t *) +
				(ops[i]->sym->cipher.data.offset >> 3);
		dst[i] = ops[i]->sym->m_dst ?
			rte_pktmbuf_mtod(ops[i]->sym->m_dst, uint8_t *) +
				(ops[i]->sym->cipher.data.offset >> 3) :
			rte_pktmbuf_mtod(ops[i]->sym->m_src, uint8_t *) +
				(ops[i]->sym->cipher.data.offset >> 3);
		iv[i] = rte_crypto_op_ctod_offset(ops[i], uint8_t *,
				session->cipher_iv_offset);
		num_bytes[i] = ops[i]->sym->cipher.data.length >> 3;

		processed_ops++;
	}

	IMB_SNOW3G_F8_N_BUFFER(qp->mgr, &session->pKeySched_cipher, iv,
			src, dst, num_bytes, processed_ops);

	return processed_ops;
}

/** Encrypt/decrypt mbuf (bit level function). */
static uint8_t
process_snow3g_cipher_op_bit(struct snow3g_qp *qp,
		struct rte_crypto_op *op,
		struct snow3g_session *session)
{
	uint8_t *src, *dst;
	uint8_t *iv;
	uint32_t length_in_bits, offset_in_bits;

	offset_in_bits = op->sym->cipher.data.offset;
	src = rte_pktmbuf_mtod(op->sym->m_src, uint8_t *);
	if (op->sym->m_dst == NULL) {
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		SNOW3G_LOG(ERR, "bit-level in-place not supported\n");
		return 0;
	}
	dst = rte_pktmbuf_mtod(op->sym->m_dst, uint8_t *);
	iv = rte_crypto_op_ctod_offset(op, uint8_t *,
				session->cipher_iv_offset);
	length_in_bits = op->sym->cipher.data.length;

	IMB_SNOW3G_F8_1_BUFFER_BIT(qp->mgr, &session->pKeySched_cipher, iv,
			src, dst, length_in_bits, offset_in_bits);

	return 1;
}

/** Generate/verify hash from mbufs with same hash key. */
static int
process_snow3g_hash_op(struct snow3g_qp *qp, struct rte_crypto_op **ops,
		struct snow3g_session *session,
		uint8_t num_ops)
{
	unsigned i;
	uint8_t processed_ops = 0;
	uint8_t *src, *dst;
	uint32_t length_in_bits;
	uint8_t *iv;

	for (i = 0; i < num_ops; i++) {
		/* Data must be byte aligned */
		if ((ops[i]->sym->auth.data.offset % BYTE_LEN) != 0) {
			ops[i]->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
			SNOW3G_LOG(ERR, "Offset");
			break;
		}

		length_in_bits = ops[i]->sym->auth.data.length;

		src = rte_pktmbuf_mtod(ops[i]->sym->m_src, uint8_t *) +
				(ops[i]->sym->auth.data.offset >> 3);
		iv = rte_crypto_op_ctod_offset(ops[i], uint8_t *,
				session->auth_iv_offset);

		if (session->auth_op == RTE_CRYPTO_AUTH_OP_VERIFY) {
			dst = qp->temp_digest;

			IMB_SNOW3G_F9_1_BUFFER(qp->mgr,
					&session->pKeySched_hash,
					iv, src, length_in_bits, dst);
			/* Verify digest. */
			if (memcmp(dst, ops[i]->sym->auth.digest.data,
					SNOW3G_DIGEST_LENGTH) != 0)
				ops[i]->status = RTE_CRYPTO_OP_STATUS_AUTH_FAILED;
		} else  {
			dst = ops[i]->sym->auth.digest.data;

			IMB_SNOW3G_F9_1_BUFFER(qp->mgr,
					&session->pKeySched_hash,
					iv, src, length_in_bits, dst);
		}
		processed_ops++;
	}

	return processed_ops;
}

/** Process a batch of crypto ops which shares the same session. */
static int
process_ops(struct rte_crypto_op **ops, struct snow3g_session *session,
		struct snow3g_qp *qp, uint8_t num_ops,
		uint16_t *accumulated_enqueued_ops)
{
	unsigned i;
	unsigned enqueued_ops, processed_ops;

#ifdef RTE_LIBRTE_PMD_SNOW3G_DEBUG
	for (i = 0; i < num_ops; i++) {
		if (!rte_pktmbuf_is_contiguous(ops[i]->sym->m_src) ||
				(ops[i]->sym->m_dst != NULL &&
				!rte_pktmbuf_is_contiguous(
						ops[i]->sym->m_dst))) {
			SNOW3G_LOG(ERR, "PMD supports only contiguous mbufs, "
				"op (%p) provides noncontiguous mbuf as "
				"source/destination buffer.\n", ops[i]);
			ops[i]->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
			return 0;
		}
	}
#endif

	switch (session->op) {
	case SNOW3G_OP_ONLY_CIPHER:
		processed_ops = process_snow3g_cipher_op(qp, ops,
				session, num_ops);
		break;
	case SNOW3G_OP_ONLY_AUTH:
		processed_ops = process_snow3g_hash_op(qp, ops, session,
				num_ops);
		break;
	case SNOW3G_OP_CIPHER_AUTH:
		processed_ops = process_snow3g_cipher_op(qp, ops, session,
				num_ops);
		process_snow3g_hash_op(qp, ops, session, processed_ops);
		break;
	case SNOW3G_OP_AUTH_CIPHER:
		processed_ops = process_snow3g_hash_op(qp, ops, session,
				num_ops);
		process_snow3g_cipher_op(qp, ops, session, processed_ops);
		break;
	default:
		/* Operation not supported. */
		processed_ops = 0;
	}

	for (i = 0; i < num_ops; i++) {
		/*
		 * If there was no error/authentication failure,
		 * change status to successful.
		 */
		if (ops[i]->status == RTE_CRYPTO_OP_STATUS_NOT_PROCESSED)
			ops[i]->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
		/* Free session if a session-less crypto op. */
		if (ops[i]->sess_type == RTE_CRYPTO_OP_SESSIONLESS) {
			memset(session, 0, sizeof(struct snow3g_session));
			memset(ops[i]->sym->session, 0,
			rte_cryptodev_sym_get_existing_header_session_size(
					ops[i]->sym->session));
			rte_mempool_put(qp->sess_mp_priv, session);
			rte_mempool_put(qp->sess_mp, ops[i]->sym->session);
			ops[i]->sym->session = NULL;
		}
	}

	enqueued_ops = rte_ring_enqueue_burst(qp->processed_ops,
			(void **)ops, processed_ops, NULL);
	qp->qp_stats.enqueued_count += enqueued_ops;
	*accumulated_enqueued_ops += enqueued_ops;

	return enqueued_ops;
}

/** Process a crypto op with length/offset in bits. */
static int
process_op_bit(struct rte_crypto_op *op, struct snow3g_session *session,
		struct snow3g_qp *qp, uint16_t *accumulated_enqueued_ops)
{
	unsigned enqueued_op, processed_op;

	switch (session->op) {
	case SNOW3G_OP_ONLY_CIPHER:
		processed_op = process_snow3g_cipher_op_bit(qp, op,
				session);
		break;
	case SNOW3G_OP_ONLY_AUTH:
		processed_op = process_snow3g_hash_op(qp, &op, session, 1);
		break;
	case SNOW3G_OP_CIPHER_AUTH:
		processed_op = process_snow3g_cipher_op_bit(qp, op, session);
		if (processed_op == 1)
			process_snow3g_hash_op(qp, &op, session, 1);
		break;
	case SNOW3G_OP_AUTH_CIPHER:
		processed_op = process_snow3g_hash_op(qp, &op, session, 1);
		if (processed_op == 1)
			process_snow3g_cipher_op_bit(qp, op, session);
		break;
	default:
		/* Operation not supported. */
		processed_op = 0;
	}

	/*
	 * If there was no error/authentication failure,
	 * change status to successful.
	 */
	if (op->status == RTE_CRYPTO_OP_STATUS_NOT_PROCESSED)
		op->status = RTE_CRYPTO_OP_STATUS_SUCCESS;

	/* Free session if a session-less crypto op. */
	if (op->sess_type == RTE_CRYPTO_OP_SESSIONLESS) {
		memset(op->sym->session, 0, sizeof(struct snow3g_session));
		rte_cryptodev_sym_session_free(op->sym->session);
		op->sym->session = NULL;
	}

	enqueued_op = rte_ring_enqueue_burst(qp->processed_ops,
			(void **)&op, processed_op, NULL);
	qp->qp_stats.enqueued_count += enqueued_op;
	*accumulated_enqueued_ops += enqueued_op;

	return enqueued_op;
}

static uint16_t
snow3g_pmd_enqueue_burst(void *queue_pair, struct rte_crypto_op **ops,
		uint16_t nb_ops)
{
	struct rte_crypto_op *c_ops[SNOW3G_MAX_BURST];
	struct rte_crypto_op *curr_c_op;

	struct snow3g_session *prev_sess = NULL, *curr_sess = NULL;
	struct snow3g_qp *qp = queue_pair;
	unsigned i;
	uint8_t burst_size = 0;
	uint16_t enqueued_ops = 0;
	uint8_t processed_ops;

	for (i = 0; i < nb_ops; i++) {
		curr_c_op = ops[i];

		/* Set status as enqueued (not processed yet) by default. */
		curr_c_op->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;

		curr_sess = snow3g_get_session(qp, curr_c_op);
		if (unlikely(curr_sess == NULL ||
				curr_sess->op == SNOW3G_OP_NOT_SUPPORTED)) {
			curr_c_op->status =
					RTE_CRYPTO_OP_STATUS_INVALID_SESSION;
			break;
		}

		/* If length/offset is at bit-level, process this buffer alone. */
		if (((curr_c_op->sym->cipher.data.length % BYTE_LEN) != 0)
				|| ((curr_c_op->sym->cipher.data.offset
					% BYTE_LEN) != 0)) {
			/* Process the ops of the previous session. */
			if (prev_sess != NULL) {
				processed_ops = process_ops(c_ops, prev_sess,
				qp, burst_size, &enqueued_ops);
				if (processed_ops < burst_size) {
					burst_size = 0;
					break;
				}

				burst_size = 0;
				prev_sess = NULL;
			}

			processed_ops = process_op_bit(curr_c_op, curr_sess,
							qp, &enqueued_ops);
			if (processed_ops != 1)
				break;

			continue;
		}

		/* Batch ops that share the same session. */
		if (prev_sess == NULL) {
			prev_sess = curr_sess;
			c_ops[burst_size++] = curr_c_op;
		} else if (curr_sess == prev_sess) {
			c_ops[burst_size++] = curr_c_op;
			/*
			 * When there are enough ops to process in a batch,
			 * process them, and start a new batch.
			 */
			if (burst_size == SNOW3G_MAX_BURST) {
				processed_ops = process_ops(c_ops, prev_sess,
						qp, burst_size, &enqueued_ops);
				if (processed_ops < burst_size) {
					burst_size = 0;
					break;
				}

				burst_size = 0;
				prev_sess = NULL;
			}
		} else {
			/*
			 * Different session, process the ops
			 * of the previous session.
			 */
			processed_ops = process_ops(c_ops, prev_sess,
					qp, burst_size, &enqueued_ops);
			if (processed_ops < burst_size) {
				burst_size = 0;
				break;
			}

			burst_size = 0;
			prev_sess = curr_sess;

			c_ops[burst_size++] = curr_c_op;
		}
	}

	if (burst_size != 0) {
		/* Process the crypto ops of the last session. */
		processed_ops = process_ops(c_ops, prev_sess,
				qp, burst_size, &enqueued_ops);
	}

	qp->qp_stats.enqueue_err_count += nb_ops - enqueued_ops;
	return enqueued_ops;
}

static uint16_t
snow3g_pmd_dequeue_burst(void *queue_pair,
		struct rte_crypto_op **c_ops, uint16_t nb_ops)
{
	struct snow3g_qp *qp = queue_pair;

	unsigned nb_dequeued;

	nb_dequeued = rte_ring_dequeue_burst(qp->processed_ops,
			(void **)c_ops, nb_ops, NULL);
	qp->qp_stats.dequeued_count += nb_dequeued;

	return nb_dequeued;
}

static int cryptodev_snow3g_remove(struct rte_vdev_device *vdev);

static int
cryptodev_snow3g_create(const char *name,
			struct rte_vdev_device *vdev,
			struct rte_cryptodev_pmd_init_params *init_params)
{
	struct rte_cryptodev *dev;
	struct snow3g_private *internals;
	MB_MGR *mgr;

	dev = rte_cryptodev_pmd_create(name, &vdev->device, init_params);
	if (dev == NULL) {
		SNOW3G_LOG(ERR, "failed to create cryptodev vdev");
		goto init_error;
	}

	dev->driver_id = cryptodev_driver_id;
	dev->dev_ops = rte_snow3g_pmd_ops;

	/* Register RX/TX burst functions for data path. */
	dev->dequeue_burst = snow3g_pmd_dequeue_burst;
	dev->enqueue_burst = snow3g_pmd_enqueue_burst;

	dev->feature_flags = RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO |
			RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING |
			RTE_CRYPTODEV_FF_NON_BYTE_ALIGNED_DATA |
			RTE_CRYPTODEV_FF_SYM_SESSIONLESS |
			RTE_CRYPTODEV_FF_OOP_LB_IN_LB_OUT;

	mgr = alloc_mb_mgr(0);
	if (mgr == NULL)
		return -ENOMEM;

	if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX2)) {
		dev->feature_flags |= RTE_CRYPTODEV_FF_CPU_AVX2;
		init_mb_mgr_avx2(mgr);
	} else if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX)) {
		dev->feature_flags |= RTE_CRYPTODEV_FF_CPU_AVX;
		init_mb_mgr_avx(mgr);
	} else {
		dev->feature_flags |= RTE_CRYPTODEV_FF_CPU_SSE;
		init_mb_mgr_sse(mgr);
	}

	internals = dev->data->dev_private;
	internals->mgr = mgr;

	internals->max_nb_queue_pairs = init_params->max_nb_queue_pairs;

	return 0;
init_error:
	SNOW3G_LOG(ERR, "driver %s: cryptodev_snow3g_create failed",
			init_params->name);

	cryptodev_snow3g_remove(vdev);
	return -EFAULT;
}

static int
cryptodev_snow3g_probe(struct rte_vdev_device *vdev)
{
	struct rte_cryptodev_pmd_init_params init_params = {
		"",
		sizeof(struct snow3g_private),
		rte_socket_id(),
		RTE_CRYPTODEV_PMD_DEFAULT_MAX_NB_QUEUE_PAIRS
	};
	const char *name;
	const char *input_args;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;
	input_args = rte_vdev_device_args(vdev);

	rte_cryptodev_pmd_parse_input_args(&init_params, input_args);

	return cryptodev_snow3g_create(name, vdev, &init_params);
}

static int
cryptodev_snow3g_remove(struct rte_vdev_device *vdev)
{
	struct rte_cryptodev *cryptodev;
	const char *name;
	struct snow3g_private *internals;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	cryptodev = rte_cryptodev_pmd_get_named_dev(name);
	if (cryptodev == NULL)
		return -ENODEV;

	internals = cryptodev->data->dev_private;

	free_mb_mgr(internals->mgr);

	return rte_cryptodev_pmd_destroy(cryptodev);
}

static struct rte_vdev_driver cryptodev_snow3g_pmd_drv = {
	.probe = cryptodev_snow3g_probe,
	.remove = cryptodev_snow3g_remove
};

static struct cryptodev_driver snow3g_crypto_drv;

RTE_PMD_REGISTER_VDEV(CRYPTODEV_NAME_SNOW3G_PMD, cryptodev_snow3g_pmd_drv);
RTE_PMD_REGISTER_ALIAS(CRYPTODEV_NAME_SNOW3G_PMD, cryptodev_snow3g_pmd);
RTE_PMD_REGISTER_PARAM_STRING(CRYPTODEV_NAME_SNOW3G_PMD,
	"max_nb_queue_pairs=<int> "
	"socket_id=<int>");
RTE_PMD_REGISTER_CRYPTO_DRIVER(snow3g_crypto_drv,
		cryptodev_snow3g_pmd_drv.driver, cryptodev_driver_id);
RTE_LOG_REGISTER(snow3g_logtype_driver, pmd.crypto.snow3g, INFO);
