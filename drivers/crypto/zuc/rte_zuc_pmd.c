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

#include "rte_zuc_pmd_private.h"
#define ZUC_MAX_BURST 4
#define BYTE_LEN 8

static uint8_t cryptodev_driver_id;

/** Get xform chain order. */
static enum zuc_operation
zuc_get_mode(const struct rte_crypto_sym_xform *xform)
{
	if (xform == NULL)
		return ZUC_OP_NOT_SUPPORTED;

	if (xform->next)
		if (xform->next->next != NULL)
			return ZUC_OP_NOT_SUPPORTED;

	if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH) {
		if (xform->next == NULL)
			return ZUC_OP_ONLY_AUTH;
		else if (xform->next->type == RTE_CRYPTO_SYM_XFORM_CIPHER)
			return ZUC_OP_AUTH_CIPHER;
		else
			return ZUC_OP_NOT_SUPPORTED;
	}

	if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER) {
		if (xform->next == NULL)
			return ZUC_OP_ONLY_CIPHER;
		else if (xform->next->type == RTE_CRYPTO_SYM_XFORM_AUTH)
			return ZUC_OP_CIPHER_AUTH;
		else
			return ZUC_OP_NOT_SUPPORTED;
	}

	return ZUC_OP_NOT_SUPPORTED;
}


/** Parse crypto xform chain and set private session parameters. */
int
zuc_set_session_parameters(struct zuc_session *sess,
		const struct rte_crypto_sym_xform *xform)
{
	const struct rte_crypto_sym_xform *auth_xform = NULL;
	const struct rte_crypto_sym_xform *cipher_xform = NULL;
	enum zuc_operation mode;

	/* Select Crypto operation - hash then cipher / cipher then hash */
	mode = zuc_get_mode(xform);

	switch (mode) {
	case ZUC_OP_CIPHER_AUTH:
		auth_xform = xform->next;

		/* Fall-through */
	case ZUC_OP_ONLY_CIPHER:
		cipher_xform = xform;
		break;
	case ZUC_OP_AUTH_CIPHER:
		cipher_xform = xform->next;
		/* Fall-through */
	case ZUC_OP_ONLY_AUTH:
		auth_xform = xform;
		break;
	case ZUC_OP_NOT_SUPPORTED:
	default:
		ZUC_LOG(ERR, "Unsupported operation chain order parameter");
		return -ENOTSUP;
	}

	if (cipher_xform) {
		/* Only ZUC EEA3 supported */
		if (cipher_xform->cipher.algo != RTE_CRYPTO_CIPHER_ZUC_EEA3)
			return -ENOTSUP;

		if (cipher_xform->cipher.iv.length != ZUC_IV_KEY_LENGTH) {
			ZUC_LOG(ERR, "Wrong IV length");
			return -EINVAL;
		}
		sess->cipher_iv_offset = cipher_xform->cipher.iv.offset;

		/* Copy the key */
		memcpy(sess->pKey_cipher, cipher_xform->cipher.key.data,
				ZUC_IV_KEY_LENGTH);
	}

	if (auth_xform) {
		/* Only ZUC EIA3 supported */
		if (auth_xform->auth.algo != RTE_CRYPTO_AUTH_ZUC_EIA3)
			return -ENOTSUP;

		if (auth_xform->auth.digest_length != ZUC_DIGEST_LENGTH) {
			ZUC_LOG(ERR, "Wrong digest length");
			return -EINVAL;
		}

		sess->auth_op = auth_xform->auth.op;

		if (auth_xform->auth.iv.length != ZUC_IV_KEY_LENGTH) {
			ZUC_LOG(ERR, "Wrong IV length");
			return -EINVAL;
		}
		sess->auth_iv_offset = auth_xform->auth.iv.offset;

		/* Copy the key */
		memcpy(sess->pKey_hash, auth_xform->auth.key.data,
				ZUC_IV_KEY_LENGTH);
	}


	sess->op = mode;

	return 0;
}

/** Get ZUC session. */
static struct zuc_session *
zuc_get_session(struct zuc_qp *qp, struct rte_crypto_op *op)
{
	struct zuc_session *sess = NULL;

	if (op->sess_type == RTE_CRYPTO_OP_WITH_SESSION) {
		if (likely(op->sym->session != NULL))
			sess = (struct zuc_session *)get_sym_session_private_data(
					op->sym->session,
					cryptodev_driver_id);
	} else {
		void *_sess = NULL;
		void *_sess_private_data = NULL;

		if (rte_mempool_get(qp->sess_mp, (void **)&_sess))
			return NULL;

		if (rte_mempool_get(qp->sess_mp, (void **)&_sess_private_data))
			return NULL;

		sess = (struct zuc_session *)_sess_private_data;

		if (unlikely(zuc_set_session_parameters(sess,
				op->sym->xform) != 0)) {
			rte_mempool_put(qp->sess_mp, _sess);
			rte_mempool_put(qp->sess_mp, _sess_private_data);
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

/** Encrypt/decrypt mbufs. */
static uint8_t
process_zuc_cipher_op(struct rte_crypto_op **ops,
		struct zuc_session **sessions,
		uint8_t num_ops)
{
	unsigned i;
	uint8_t processed_ops = 0;
	uint8_t *src[ZUC_MAX_BURST], *dst[ZUC_MAX_BURST];
	uint8_t *iv[ZUC_MAX_BURST];
	uint32_t num_bytes[ZUC_MAX_BURST];
	uint8_t *cipher_keys[ZUC_MAX_BURST];
	struct zuc_session *sess;

	for (i = 0; i < num_ops; i++) {
		if (((ops[i]->sym->cipher.data.length % BYTE_LEN) != 0)
				|| ((ops[i]->sym->cipher.data.offset
					% BYTE_LEN) != 0)) {
			ops[i]->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
			ZUC_LOG(ERR, "Data Length or offset");
			break;
		}

		sess = sessions[i];

#ifdef RTE_LIBRTE_PMD_ZUC_DEBUG
		if (!rte_pktmbuf_is_contiguous(ops[i]->sym->m_src) ||
				(ops[i]->sym->m_dst != NULL &&
				!rte_pktmbuf_is_contiguous(
						ops[i]->sym->m_dst))) {
			ZUC_LOG(ERR, "PMD supports only contiguous mbufs, "
				"op (%p) provides noncontiguous mbuf as "
				"source/destination buffer.\n", ops[i]);
			ops[i]->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
			break;
		}
#endif

		src[i] = rte_pktmbuf_mtod(ops[i]->sym->m_src, uint8_t *) +
				(ops[i]->sym->cipher.data.offset >> 3);
		dst[i] = ops[i]->sym->m_dst ?
			rte_pktmbuf_mtod(ops[i]->sym->m_dst, uint8_t *) +
				(ops[i]->sym->cipher.data.offset >> 3) :
			rte_pktmbuf_mtod(ops[i]->sym->m_src, uint8_t *) +
				(ops[i]->sym->cipher.data.offset >> 3);
		iv[i] = rte_crypto_op_ctod_offset(ops[i], uint8_t *,
				sess->cipher_iv_offset);
		num_bytes[i] = ops[i]->sym->cipher.data.length >> 3;

		cipher_keys[i] = sess->pKey_cipher;

		processed_ops++;
	}

	sso_zuc_eea3_n_buffer(cipher_keys, iv, src, dst,
			num_bytes, processed_ops);

	return processed_ops;
}

/** Generate/verify hash from mbufs. */
static int
process_zuc_hash_op(struct zuc_qp *qp, struct rte_crypto_op **ops,
		struct zuc_session **sessions,
		uint8_t num_ops)
{
	unsigned i;
	uint8_t processed_ops = 0;
	uint8_t *src;
	uint32_t *dst;
	uint32_t length_in_bits;
	uint8_t *iv;
	struct zuc_session *sess;

	for (i = 0; i < num_ops; i++) {
		/* Data must be byte aligned */
		if ((ops[i]->sym->auth.data.offset % BYTE_LEN) != 0) {
			ops[i]->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
			ZUC_LOG(ERR, "Offset");
			break;
		}

		sess = sessions[i];

		length_in_bits = ops[i]->sym->auth.data.length;

		src = rte_pktmbuf_mtod(ops[i]->sym->m_src, uint8_t *) +
				(ops[i]->sym->auth.data.offset >> 3);
		iv = rte_crypto_op_ctod_offset(ops[i], uint8_t *,
				sess->auth_iv_offset);

		if (sess->auth_op == RTE_CRYPTO_AUTH_OP_VERIFY) {
			dst = (uint32_t *)qp->temp_digest;

			sso_zuc_eia3_1_buffer(sess->pKey_hash,
					iv, src,
					length_in_bits,	dst);
			/* Verify digest. */
			if (memcmp(dst, ops[i]->sym->auth.digest.data,
					ZUC_DIGEST_LENGTH) != 0)
				ops[i]->status = RTE_CRYPTO_OP_STATUS_AUTH_FAILED;
		} else  {
			dst = (uint32_t *)ops[i]->sym->auth.digest.data;

			sso_zuc_eia3_1_buffer(sess->pKey_hash,
					iv, src,
					length_in_bits, dst);
		}
		processed_ops++;
	}

	return processed_ops;
}

/** Process a batch of crypto ops which shares the same operation type. */
static int
process_ops(struct rte_crypto_op **ops, enum zuc_operation op_type,
		struct zuc_session **sessions,
		struct zuc_qp *qp, uint8_t num_ops,
		uint16_t *accumulated_enqueued_ops)
{
	unsigned i;
	unsigned enqueued_ops, processed_ops;

	switch (op_type) {
	case ZUC_OP_ONLY_CIPHER:
		processed_ops = process_zuc_cipher_op(ops,
				sessions, num_ops);
		break;
	case ZUC_OP_ONLY_AUTH:
		processed_ops = process_zuc_hash_op(qp, ops, sessions,
				num_ops);
		break;
	case ZUC_OP_CIPHER_AUTH:
		processed_ops = process_zuc_cipher_op(ops, sessions,
				num_ops);
		process_zuc_hash_op(qp, ops, sessions, processed_ops);
		break;
	case ZUC_OP_AUTH_CIPHER:
		processed_ops = process_zuc_hash_op(qp, ops, sessions,
				num_ops);
		process_zuc_cipher_op(ops, sessions, processed_ops);
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
			memset(sessions[i], 0, sizeof(struct zuc_session));
			memset(ops[i]->sym->session, 0,
					rte_cryptodev_sym_get_header_session_size());
			rte_mempool_put(qp->sess_mp, sessions[i]);
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

static uint16_t
zuc_pmd_enqueue_burst(void *queue_pair, struct rte_crypto_op **ops,
		uint16_t nb_ops)
{
	struct rte_crypto_op *c_ops[ZUC_MAX_BURST];
	struct rte_crypto_op *curr_c_op;

	struct zuc_session *curr_sess;
	struct zuc_session *sessions[ZUC_MAX_BURST];
	enum zuc_operation prev_zuc_op = ZUC_OP_NOT_SUPPORTED;
	enum zuc_operation curr_zuc_op;
	struct zuc_qp *qp = queue_pair;
	unsigned i;
	uint8_t burst_size = 0;
	uint16_t enqueued_ops = 0;
	uint8_t processed_ops;

	for (i = 0; i < nb_ops; i++) {
		curr_c_op = ops[i];

		curr_sess = zuc_get_session(qp, curr_c_op);
		if (unlikely(curr_sess == NULL)) {
			curr_c_op->status =
					RTE_CRYPTO_OP_STATUS_INVALID_SESSION;
			break;
		}

		curr_zuc_op = curr_sess->op;

		/*
		 * Batch ops that share the same operation type
		 * (cipher only, auth only...).
		 */
		if (burst_size == 0) {
			prev_zuc_op = curr_zuc_op;
			c_ops[0] = curr_c_op;
			sessions[0] = curr_sess;
			burst_size++;
		} else if (curr_zuc_op == prev_zuc_op) {
			c_ops[burst_size] = curr_c_op;
			sessions[burst_size] = curr_sess;
			burst_size++;
			/*
			 * When there are enough ops to process in a batch,
			 * process them, and start a new batch.
			 */
			if (burst_size == ZUC_MAX_BURST) {
				processed_ops = process_ops(c_ops, curr_zuc_op,
						sessions, qp, burst_size,
						&enqueued_ops);
				if (processed_ops < burst_size) {
					burst_size = 0;
					break;
				}

				burst_size = 0;
			}
		} else {
			/*
			 * Different operation type, process the ops
			 * of the previous type.
			 */
			processed_ops = process_ops(c_ops, prev_zuc_op,
					sessions, qp, burst_size,
					&enqueued_ops);
			if (processed_ops < burst_size) {
				burst_size = 0;
				break;
			}

			burst_size = 0;
			prev_zuc_op = curr_zuc_op;

			c_ops[0] = curr_c_op;
			sessions[0] = curr_sess;
			burst_size++;
		}
	}

	if (burst_size != 0) {
		/* Process the crypto ops of the last operation type. */
		processed_ops = process_ops(c_ops, prev_zuc_op,
				sessions, qp, burst_size,
				&enqueued_ops);
	}

	qp->qp_stats.enqueue_err_count += nb_ops - enqueued_ops;
	return enqueued_ops;
}

static uint16_t
zuc_pmd_dequeue_burst(void *queue_pair,
		struct rte_crypto_op **c_ops, uint16_t nb_ops)
{
	struct zuc_qp *qp = queue_pair;

	unsigned nb_dequeued;

	nb_dequeued = rte_ring_dequeue_burst(qp->processed_ops,
			(void **)c_ops, nb_ops, NULL);
	qp->qp_stats.dequeued_count += nb_dequeued;

	return nb_dequeued;
}

static int cryptodev_zuc_remove(struct rte_vdev_device *vdev);

static int
cryptodev_zuc_create(const char *name,
		struct rte_vdev_device *vdev,
		struct rte_cryptodev_pmd_init_params *init_params)
{
	struct rte_cryptodev *dev;
	struct zuc_private *internals;
	uint64_t cpu_flags = RTE_CRYPTODEV_FF_CPU_SSE;


	dev = rte_cryptodev_pmd_create(name, &vdev->device, init_params);
	if (dev == NULL) {
		ZUC_LOG(ERR, "failed to create cryptodev vdev");
		goto init_error;
	}

	dev->driver_id = cryptodev_driver_id;
	dev->dev_ops = rte_zuc_pmd_ops;

	/* Register RX/TX burst functions for data path. */
	dev->dequeue_burst = zuc_pmd_dequeue_burst;
	dev->enqueue_burst = zuc_pmd_enqueue_burst;

	dev->feature_flags = RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO |
			RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING |
			cpu_flags;

	internals = dev->data->dev_private;

	internals->max_nb_queue_pairs = init_params->max_nb_queue_pairs;

	return 0;
init_error:
	ZUC_LOG(ERR, "driver %s: failed",
			init_params->name);

	cryptodev_zuc_remove(vdev);
	return -EFAULT;
}

static int
cryptodev_zuc_probe(struct rte_vdev_device *vdev)
{
	struct rte_cryptodev_pmd_init_params init_params = {
		"",
		sizeof(struct zuc_private),
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

	return cryptodev_zuc_create(name, vdev, &init_params);
}

static int
cryptodev_zuc_remove(struct rte_vdev_device *vdev)
{

	struct rte_cryptodev *cryptodev;
	const char *name;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	cryptodev = rte_cryptodev_pmd_get_named_dev(name);
	if (cryptodev == NULL)
		return -ENODEV;

	return rte_cryptodev_pmd_destroy(cryptodev);
}

static struct rte_vdev_driver cryptodev_zuc_pmd_drv = {
	.probe = cryptodev_zuc_probe,
	.remove = cryptodev_zuc_remove
};

static struct cryptodev_driver zuc_crypto_drv;

RTE_PMD_REGISTER_VDEV(CRYPTODEV_NAME_ZUC_PMD, cryptodev_zuc_pmd_drv);
RTE_PMD_REGISTER_PARAM_STRING(CRYPTODEV_NAME_ZUC_PMD,
	"max_nb_queue_pairs=<int> "
	"socket_id=<int>");
RTE_PMD_REGISTER_CRYPTO_DRIVER(zuc_crypto_drv, cryptodev_zuc_pmd_drv.driver,
		cryptodev_driver_id);

RTE_INIT(zuc_init_log)
{
	zuc_logtype_driver = rte_log_register("pmd.crypto.zuc");
}
