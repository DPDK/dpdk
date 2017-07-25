/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016-2017 Intel Corporation. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
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
#include <rte_config.h>
#include <rte_hexdump.h>
#include <rte_cryptodev.h>
#include <rte_cryptodev_pmd.h>
#include <rte_cryptodev_vdev.h>
#include <rte_vdev.h>
#include <rte_malloc.h>
#include <rte_cpuflags.h>

#include "rte_zuc_pmd_private.h"

#define ZUC_DIGEST_LENGTH 4
#define ZUC_MAX_BURST 8
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
		ZUC_LOG_ERR("Unsupported operation chain order parameter");
		return -ENOTSUP;
	}

	if (cipher_xform) {
		/* Only ZUC EEA3 supported */
		if (cipher_xform->cipher.algo != RTE_CRYPTO_CIPHER_ZUC_EEA3)
			return -ENOTSUP;

		if (cipher_xform->cipher.iv.length != ZUC_IV_KEY_LENGTH) {
			ZUC_LOG_ERR("Wrong IV length");
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
			ZUC_LOG_ERR("Wrong digest length");
			return -EINVAL;
		}

		sess->auth_op = auth_xform->auth.op;

		if (auth_xform->auth.iv.length != ZUC_IV_KEY_LENGTH) {
			ZUC_LOG_ERR("Wrong IV length");
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
			sess = (struct zuc_session *)get_session_private_data(
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
		set_session_private_data(op->sym->session, cryptodev_driver_id,
			_sess_private_data);
	}

	if (unlikely(sess == NULL))
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_SESSION;


	return sess;
}

/** Encrypt/decrypt mbufs with same cipher key. */
static uint8_t
process_zuc_cipher_op(struct rte_crypto_op **ops,
		struct zuc_session *session,
		uint8_t num_ops)
{
	unsigned i;
	uint8_t processed_ops = 0;
	uint8_t *src[ZUC_MAX_BURST], *dst[ZUC_MAX_BURST];
	uint8_t *iv[ZUC_MAX_BURST];
	uint32_t num_bytes[ZUC_MAX_BURST];
	uint8_t *cipher_keys[ZUC_MAX_BURST];

	for (i = 0; i < num_ops; i++) {
		if (((ops[i]->sym->cipher.data.length % BYTE_LEN) != 0)
				|| ((ops[i]->sym->cipher.data.offset
					% BYTE_LEN) != 0)) {
			ops[i]->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
			ZUC_LOG_ERR("Data Length or offset");
			break;
		}

#ifdef RTE_LIBRTE_PMD_ZUC_DEBUG
		if (!rte_pktmbuf_is_contiguous(ops[i]->sym->m_src) ||
				(ops[i]->sym->m_dst != NULL &&
				!rte_pktmbuf_is_contiguous(
						ops[i]->sym->m_dst))) {
			ZUC_LOG_ERR("PMD supports only contiguous mbufs, "
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
				session->cipher_iv_offset);
		num_bytes[i] = ops[i]->sym->cipher.data.length >> 3;

		cipher_keys[i] = session->pKey_cipher;

		processed_ops++;
	}

	sso_zuc_eea3_n_buffer(cipher_keys, iv, src, dst,
			num_bytes, processed_ops);

	return processed_ops;
}

/** Generate/verify hash from mbufs with same hash key. */
static int
process_zuc_hash_op(struct rte_crypto_op **ops,
		struct zuc_session *session,
		uint8_t num_ops)
{
	unsigned i;
	uint8_t processed_ops = 0;
	uint8_t *src;
	uint32_t *dst;
	uint32_t length_in_bits;
	uint8_t *iv;

	for (i = 0; i < num_ops; i++) {
		/* Data must be byte aligned */
		if ((ops[i]->sym->auth.data.offset % BYTE_LEN) != 0) {
			ops[i]->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
			ZUC_LOG_ERR("Offset");
			break;
		}

		length_in_bits = ops[i]->sym->auth.data.length;

		src = rte_pktmbuf_mtod(ops[i]->sym->m_src, uint8_t *) +
				(ops[i]->sym->auth.data.offset >> 3);
		iv = rte_crypto_op_ctod_offset(ops[i], uint8_t *,
				session->auth_iv_offset);

		if (session->auth_op == RTE_CRYPTO_AUTH_OP_VERIFY) {
			dst = (uint32_t *)rte_pktmbuf_append(ops[i]->sym->m_src,
					ZUC_DIGEST_LENGTH);

			sso_zuc_eia3_1_buffer(session->pKey_hash,
					iv, src,
					length_in_bits,	dst);
			/* Verify digest. */
			if (memcmp(dst, ops[i]->sym->auth.digest.data,
					ZUC_DIGEST_LENGTH) != 0)
				ops[i]->status = RTE_CRYPTO_OP_STATUS_AUTH_FAILED;

			/* Trim area used for digest from mbuf. */
			rte_pktmbuf_trim(ops[i]->sym->m_src,
					ZUC_DIGEST_LENGTH);
		} else  {
			dst = (uint32_t *)ops[i]->sym->auth.digest.data;

			sso_zuc_eia3_1_buffer(session->pKey_hash,
					iv, src,
					length_in_bits, dst);
		}
		processed_ops++;
	}

	return processed_ops;
}

/** Process a batch of crypto ops which shares the same session. */
static int
process_ops(struct rte_crypto_op **ops, struct zuc_session *session,
		struct zuc_qp *qp, uint8_t num_ops,
		uint16_t *accumulated_enqueued_ops)
{
	unsigned i;
	unsigned enqueued_ops, processed_ops;

	switch (session->op) {
	case ZUC_OP_ONLY_CIPHER:
		processed_ops = process_zuc_cipher_op(ops,
				session, num_ops);
		break;
	case ZUC_OP_ONLY_AUTH:
		processed_ops = process_zuc_hash_op(ops, session,
				num_ops);
		break;
	case ZUC_OP_CIPHER_AUTH:
		processed_ops = process_zuc_cipher_op(ops, session,
				num_ops);
		process_zuc_hash_op(ops, session, processed_ops);
		break;
	case ZUC_OP_AUTH_CIPHER:
		processed_ops = process_zuc_hash_op(ops, session,
				num_ops);
		process_zuc_cipher_op(ops, session, processed_ops);
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
			memset(session, 0, sizeof(struct zuc_session));
			memset(ops[i]->sym->session, 0,
					rte_cryptodev_get_header_session_size());
			rte_mempool_put(qp->sess_mp, session);
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

	struct zuc_session *prev_sess = NULL, *curr_sess = NULL;
	struct zuc_qp *qp = queue_pair;
	unsigned i;
	uint8_t burst_size = 0;
	uint16_t enqueued_ops = 0;
	uint8_t processed_ops;

	for (i = 0; i < nb_ops; i++) {
		curr_c_op = ops[i];

		/* Set status as enqueued (not processed yet) by default. */
		curr_c_op->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;

		curr_sess = zuc_get_session(qp, curr_c_op);
		if (unlikely(curr_sess == NULL ||
				curr_sess->op == ZUC_OP_NOT_SUPPORTED)) {
			curr_c_op->status =
					RTE_CRYPTO_OP_STATUS_INVALID_SESSION;
			break;
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
			if (burst_size == ZUC_MAX_BURST) {
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
		struct rte_crypto_vdev_init_params *init_params)
{
	struct rte_cryptodev *dev;
	struct zuc_private *internals;
	uint64_t cpu_flags = RTE_CRYPTODEV_FF_CPU_SSE;

	if (init_params->name[0] == '\0')
		snprintf(init_params->name, sizeof(init_params->name),
				"%s", name);

	dev = rte_cryptodev_vdev_pmd_init(init_params->name,
			sizeof(struct zuc_private), init_params->socket_id,
			vdev);
	if (dev == NULL) {
		ZUC_LOG_ERR("failed to create cryptodev vdev");
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
	internals->max_nb_sessions = init_params->max_nb_sessions;

	return 0;
init_error:
	ZUC_LOG_ERR("driver %s: cryptodev_zuc_create failed",
			init_params->name);

	cryptodev_zuc_remove(vdev);
	return -EFAULT;
}

static int
cryptodev_zuc_probe(struct rte_vdev_device *vdev)
{
	struct rte_crypto_vdev_init_params init_params = {
		RTE_CRYPTODEV_VDEV_DEFAULT_MAX_NB_QUEUE_PAIRS,
		RTE_CRYPTODEV_VDEV_DEFAULT_MAX_NB_SESSIONS,
		rte_socket_id(),
		{0}
	};
	const char *name;
	const char *input_args;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;
	input_args = rte_vdev_device_args(vdev);

	rte_cryptodev_vdev_parse_init_params(&init_params, input_args);

	RTE_LOG(INFO, PMD, "Initialising %s on NUMA node %d\n", name,
			init_params.socket_id);
	if (init_params.name[0] != '\0')
		RTE_LOG(INFO, PMD, "  User defined name = %s\n",
			init_params.name);
	RTE_LOG(INFO, PMD, "  Max number of queue pairs = %d\n",
			init_params.max_nb_queue_pairs);
	RTE_LOG(INFO, PMD, "  Max number of sessions = %d\n",
			init_params.max_nb_sessions);

	return cryptodev_zuc_create(name, vdev, &init_params);
}

static int
cryptodev_zuc_remove(struct rte_vdev_device *vdev)
{
	const char *name;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	RTE_LOG(INFO, PMD, "Closing ZUC crypto device %s"
			" on numa socket %u\n",
			name, rte_socket_id());

	return 0;
}

static struct rte_vdev_driver cryptodev_zuc_pmd_drv = {
	.probe = cryptodev_zuc_probe,
	.remove = cryptodev_zuc_remove
};

RTE_PMD_REGISTER_VDEV(CRYPTODEV_NAME_ZUC_PMD, cryptodev_zuc_pmd_drv);
RTE_PMD_REGISTER_PARAM_STRING(CRYPTODEV_NAME_ZUC_PMD,
	"max_nb_queue_pairs=<int> "
	"max_nb_sessions=<int> "
	"socket_id=<int>");
RTE_PMD_REGISTER_CRYPTO_DRIVER(cryptodev_zuc_pmd_drv, cryptodev_driver_id);
