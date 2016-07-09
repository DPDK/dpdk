/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016 Intel Corporation. All rights reserved.
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
#include <rte_dev.h>
#include <rte_malloc.h>
#include <rte_cpuflags.h>

#include "rte_snow3g_pmd_private.h"

#define SNOW3G_IV_LENGTH 16
#define SNOW3G_DIGEST_LENGTH 4
#define SNOW3G_MAX_BURST 8
#define BYTE_LEN 8

/**
 * Global static parameter used to create a unique name for each SNOW 3G
 * crypto device.
 */
static unsigned unique_name_id;

static inline int
create_unique_device_name(char *name, size_t size)
{
	int ret;

	if (name == NULL)
		return -EINVAL;

	ret = snprintf(name, size, "%s_%u", RTE_STR(CRYPTODEV_NAME_SNOW3G_PMD),
			unique_name_id++);
	if (ret < 0)
		return ret;
	return 0;
}

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
snow3g_set_session_parameters(struct snow3g_session *sess,
		const struct rte_crypto_sym_xform *xform)
{
	const struct rte_crypto_sym_xform *auth_xform = NULL;
	const struct rte_crypto_sym_xform *cipher_xform = NULL;
	int mode;

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
	}

	if (mode == SNOW3G_OP_NOT_SUPPORTED) {
		SNOW3G_LOG_ERR("Unsupported operation chain order parameter");
		return -EINVAL;
	}

	if (cipher_xform) {
		/* Only SNOW 3G UEA2 supported */
		if (cipher_xform->cipher.algo != RTE_CRYPTO_CIPHER_SNOW3G_UEA2)
			return -EINVAL;
		/* Initialize key */
		sso_snow3g_init_key_sched(xform->cipher.key.data,
				&sess->pKeySched_cipher);
	}

	if (auth_xform) {
		/* Only SNOW 3G UIA2 supported */
		if (auth_xform->auth.algo != RTE_CRYPTO_AUTH_SNOW3G_UIA2)
			return -EINVAL;
		sess->auth_op = auth_xform->auth.op;
		/* Initialize key */
		sso_snow3g_init_key_sched(xform->auth.key.data,
				&sess->pKeySched_hash);
	}


	sess->op = mode;

	return 0;
}

/** Get SNOW 3G session. */
static struct snow3g_session *
snow3g_get_session(struct snow3g_qp *qp, struct rte_crypto_op *op)
{
	struct snow3g_session *sess;

	if (op->sym->sess_type == RTE_CRYPTO_SYM_OP_WITH_SESSION) {
		if (unlikely(op->sym->session->dev_type !=
				RTE_CRYPTODEV_SNOW3G_PMD))
			return NULL;

		sess = (struct snow3g_session *)op->sym->session->_private;
	} else  {
		struct rte_cryptodev_session *c_sess = NULL;

		if (rte_mempool_get(qp->sess_mp, (void **)&c_sess))
			return NULL;

		sess = (struct snow3g_session *)c_sess->_private;

		if (unlikely(snow3g_set_session_parameters(sess,
				op->sym->xform) != 0))
			return NULL;
	}

	return sess;
}

/** Encrypt/decrypt mbufs with same cipher key. */
static uint8_t
process_snow3g_cipher_op(struct rte_crypto_op **ops,
		struct snow3g_session *session,
		uint8_t num_ops)
{
	unsigned i;
	uint8_t processed_ops = 0;
	uint8_t *src[SNOW3G_MAX_BURST], *dst[SNOW3G_MAX_BURST];
	uint8_t *IV[SNOW3G_MAX_BURST];
	uint32_t num_bytes[SNOW3G_MAX_BURST];

	for (i = 0; i < num_ops; i++) {
		/* Sanity checks. */
		if (unlikely(ops[i]->sym->cipher.iv.length != SNOW3G_IV_LENGTH)) {
			ops[i]->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
			SNOW3G_LOG_ERR("iv");
			break;
		}

		src[i] = rte_pktmbuf_mtod(ops[i]->sym->m_src, uint8_t *) +
				(ops[i]->sym->cipher.data.offset >> 3);
		dst[i] = ops[i]->sym->m_dst ?
			rte_pktmbuf_mtod(ops[i]->sym->m_dst, uint8_t *) +
				(ops[i]->sym->cipher.data.offset >> 3) :
			rte_pktmbuf_mtod(ops[i]->sym->m_src, uint8_t *) +
				(ops[i]->sym->cipher.data.offset >> 3);
		IV[i] = ops[i]->sym->cipher.iv.data;
		num_bytes[i] = ops[i]->sym->cipher.data.length >> 3;

		processed_ops++;
	}

	sso_snow3g_f8_n_buffer(&session->pKeySched_cipher, IV, src, dst,
			num_bytes, processed_ops);

	return processed_ops;
}

/** Encrypt/decrypt mbuf (bit level function). */
static uint8_t
process_snow3g_cipher_op_bit(struct rte_crypto_op *op,
		struct snow3g_session *session)
{
	uint8_t *src, *dst;
	uint8_t *IV;
	uint32_t length_in_bits, offset_in_bits;

	/* Sanity checks. */
	if (unlikely(op->sym->cipher.iv.length != SNOW3G_IV_LENGTH)) {
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		SNOW3G_LOG_ERR("iv");
		return 0;
	}

	offset_in_bits = op->sym->cipher.data.offset;
	src = rte_pktmbuf_mtod(op->sym->m_src, uint8_t *);
	if (op->sym->m_dst == NULL) {
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		SNOW3G_LOG_ERR("bit-level in-place not supported\n");
		return 0;
	}
	dst = rte_pktmbuf_mtod(op->sym->m_dst, uint8_t *);
	IV = op->sym->cipher.iv.data;
	length_in_bits = op->sym->cipher.data.length;

	sso_snow3g_f8_1_buffer_bit(&session->pKeySched_cipher, IV,
			src, dst, length_in_bits, offset_in_bits);

	return 1;
}

/** Generate/verify hash from mbufs with same hash key. */
static int
process_snow3g_hash_op(struct rte_crypto_op **ops,
		struct snow3g_session *session,
		uint8_t num_ops)
{
	unsigned i;
	uint8_t processed_ops = 0;
	uint8_t *src, *dst;
	uint32_t length_in_bits;

	for (i = 0; i < num_ops; i++) {
		if (unlikely(ops[i]->sym->auth.aad.length != SNOW3G_IV_LENGTH)) {
			ops[i]->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
			SNOW3G_LOG_ERR("aad");
			break;
		}

		if (unlikely(ops[i]->sym->auth.digest.length != SNOW3G_DIGEST_LENGTH)) {
			ops[i]->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
			SNOW3G_LOG_ERR("digest");
			break;
		}

		/* Data must be byte aligned */
		if ((ops[i]->sym->auth.data.offset % BYTE_LEN) != 0) {
			ops[i]->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
			SNOW3G_LOG_ERR("Offset");
			break;
		}

		length_in_bits = ops[i]->sym->auth.data.length;

		src = rte_pktmbuf_mtod(ops[i]->sym->m_src, uint8_t *) +
				(ops[i]->sym->auth.data.offset >> 3);

		if (session->auth_op == RTE_CRYPTO_AUTH_OP_VERIFY) {
			dst = (uint8_t *)rte_pktmbuf_append(ops[i]->sym->m_src,
					ops[i]->sym->auth.digest.length);

			sso_snow3g_f9_1_buffer(&session->pKeySched_hash,
					ops[i]->sym->auth.aad.data, src,
					length_in_bits,	dst);
			/* Verify digest. */
			if (memcmp(dst, ops[i]->sym->auth.digest.data,
					ops[i]->sym->auth.digest.length) != 0)
				ops[i]->status = RTE_CRYPTO_OP_STATUS_AUTH_FAILED;

			/* Trim area used for digest from mbuf. */
			rte_pktmbuf_trim(ops[i]->sym->m_src,
					ops[i]->sym->auth.digest.length);
		} else  {
			dst = ops[i]->sym->auth.digest.data;

			sso_snow3g_f9_1_buffer(&session->pKeySched_hash,
					ops[i]->sym->auth.aad.data, src,
					length_in_bits, dst);
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

	switch (session->op) {
	case SNOW3G_OP_ONLY_CIPHER:
		processed_ops = process_snow3g_cipher_op(ops,
				session, num_ops);
		break;
	case SNOW3G_OP_ONLY_AUTH:
		processed_ops = process_snow3g_hash_op(ops, session,
				num_ops);
		break;
	case SNOW3G_OP_CIPHER_AUTH:
		processed_ops = process_snow3g_cipher_op(ops, session,
				num_ops);
		process_snow3g_hash_op(ops, session, processed_ops);
		break;
	case SNOW3G_OP_AUTH_CIPHER:
		processed_ops = process_snow3g_hash_op(ops, session,
				num_ops);
		process_snow3g_cipher_op(ops, session, processed_ops);
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
		if (ops[i]->sym->sess_type == RTE_CRYPTO_SYM_OP_SESSIONLESS) {
			rte_mempool_put(qp->sess_mp, ops[i]->sym->session);
			ops[i]->sym->session = NULL;
		}
	}

	enqueued_ops = rte_ring_enqueue_burst(qp->processed_ops,
			(void **)ops, processed_ops);
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
		processed_op = process_snow3g_cipher_op_bit(op,
				session);
		break;
	case SNOW3G_OP_ONLY_AUTH:
		processed_op = process_snow3g_hash_op(&op, session, 1);
		break;
	case SNOW3G_OP_CIPHER_AUTH:
		processed_op = process_snow3g_cipher_op_bit(op, session);
		if (processed_op == 1)
			process_snow3g_hash_op(&op, session, 1);
		break;
	case SNOW3G_OP_AUTH_CIPHER:
		processed_op = process_snow3g_hash_op(&op, session, 1);
		if (processed_op == 1)
			process_snow3g_cipher_op_bit(op, session);
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
	if (op->sym->sess_type == RTE_CRYPTO_SYM_OP_SESSIONLESS) {
		rte_mempool_put(qp->sess_mp, op->sym->session);
		op->sym->session = NULL;
	}

	enqueued_op = rte_ring_enqueue_burst(qp->processed_ops,
			(void **)&op, processed_op);
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
			(void **)c_ops, nb_ops);
	qp->qp_stats.dequeued_count += nb_dequeued;

	return nb_dequeued;
}

static int cryptodev_snow3g_uninit(const char *name);

static int
cryptodev_snow3g_create(const char *name,
		struct rte_crypto_vdev_init_params *init_params)
{
	struct rte_cryptodev *dev;
	char crypto_dev_name[RTE_CRYPTODEV_NAME_MAX_LEN];
	struct snow3g_private *internals;
	uint64_t cpu_flags = 0;

	/* Check CPU for supported vector instruction set */
	if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_SSE4_1))
		cpu_flags |= RTE_CRYPTODEV_FF_CPU_SSE;
	else {
		SNOW3G_LOG_ERR("Vector instructions are not supported by CPU");
		return -EFAULT;
	}


	/* Create a unique device name. */
	if (create_unique_device_name(crypto_dev_name,
			RTE_CRYPTODEV_NAME_MAX_LEN) != 0) {
		SNOW3G_LOG_ERR("failed to create unique cryptodev name");
		return -EINVAL;
	}

	dev = rte_cryptodev_pmd_virtual_dev_init(crypto_dev_name,
			sizeof(struct snow3g_private), init_params->socket_id);
	if (dev == NULL) {
		SNOW3G_LOG_ERR("failed to create cryptodev vdev");
		goto init_error;
	}

	dev->dev_type = RTE_CRYPTODEV_SNOW3G_PMD;
	dev->dev_ops = rte_snow3g_pmd_ops;

	/* Register RX/TX burst functions for data path. */
	dev->dequeue_burst = snow3g_pmd_dequeue_burst;
	dev->enqueue_burst = snow3g_pmd_enqueue_burst;

	dev->feature_flags = RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO |
			RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING |
			cpu_flags;

	internals = dev->data->dev_private;

	internals->max_nb_queue_pairs = init_params->max_nb_queue_pairs;
	internals->max_nb_sessions = init_params->max_nb_sessions;

	return 0;
init_error:
	SNOW3G_LOG_ERR("driver %s: cryptodev_snow3g_create failed", name);

	cryptodev_snow3g_uninit(crypto_dev_name);
	return -EFAULT;
}

static int
cryptodev_snow3g_init(const char *name,
		const char *input_args)
{
	struct rte_crypto_vdev_init_params init_params = {
		RTE_CRYPTODEV_VDEV_DEFAULT_MAX_NB_QUEUE_PAIRS,
		RTE_CRYPTODEV_VDEV_DEFAULT_MAX_NB_SESSIONS,
		rte_socket_id()
	};

	rte_cryptodev_parse_vdev_init_params(&init_params, input_args);

	RTE_LOG(INFO, PMD, "Initialising %s on NUMA node %d\n", name,
			init_params.socket_id);
	RTE_LOG(INFO, PMD, "  Max number of queue pairs = %d\n",
			init_params.max_nb_queue_pairs);
	RTE_LOG(INFO, PMD, "  Max number of sessions = %d\n",
			init_params.max_nb_sessions);

	return cryptodev_snow3g_create(name, &init_params);
}

static int
cryptodev_snow3g_uninit(const char *name)
{
	if (name == NULL)
		return -EINVAL;

	RTE_LOG(INFO, PMD, "Closing SNOW3G crypto device %s"
			" on numa socket %u\n",
			name, rte_socket_id());

	return 0;
}

static struct rte_driver cryptodev_snow3g_pmd_drv = {
	.type = PMD_VDEV,
	.init = cryptodev_snow3g_init,
	.uninit = cryptodev_snow3g_uninit
};

PMD_REGISTER_DRIVER(cryptodev_snow3g_pmd_drv, CRYPTODEV_NAME_SNOW3G_PMD);
DRIVER_REGISTER_PARAM_STRING(CRYPTODEV_NAME_SNOW3G_PMD,
	"max_nb_queue_pairs=<int> "
	"max_nb_sessions=<int> "
	"socket_id=<int>");
