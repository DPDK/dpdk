/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2021 Intel Corporation
 */

#include <bus_vdev_driver.h>
#include <rte_common.h>
#include <rte_cpuflags.h>
#include <rte_cryptodev.h>
#include <rte_hexdump.h>
#include <rte_malloc.h>

#include "pmd_kasumi_priv.h"

/** Parse crypto xform chain and set private session parameters. */
static int
kasumi_session_configure(IMB_MGR *mgr, void *priv_sess,
			  const struct rte_crypto_sym_xform *xform)
{
	const struct rte_crypto_sym_xform *auth_xform = NULL;
	const struct rte_crypto_sym_xform *cipher_xform = NULL;
	enum ipsec_mb_operation mode;
	struct kasumi_session *sess = (struct kasumi_session *)priv_sess;
	/* Select Crypto operation - hash then cipher / cipher then hash */
	int ret = ipsec_mb_parse_xform(xform, &mode, &auth_xform,
				&cipher_xform, NULL);

	if (ret)
		return ret;

	if (cipher_xform) {
		/* Only KASUMI F8 supported */
		if (cipher_xform->cipher.algo != RTE_CRYPTO_CIPHER_KASUMI_F8) {
			IPSEC_MB_LOG(ERR, "Unsupported cipher algorithm ");
			return -ENOTSUP;
		}

		sess->cipher_iv_offset = cipher_xform->cipher.iv.offset;
		if (cipher_xform->cipher.iv.length != KASUMI_IV_LENGTH) {
			IPSEC_MB_LOG(ERR, "Wrong IV length");
			return -EINVAL;
		}

		/* Initialize key */
		IMB_KASUMI_INIT_F8_KEY_SCHED(mgr,
					      cipher_xform->cipher.key.data,
					      &sess->pKeySched_cipher);
	}

	if (auth_xform) {
		/* Only KASUMI F9 supported */
		if (auth_xform->auth.algo != RTE_CRYPTO_AUTH_KASUMI_F9) {
			IPSEC_MB_LOG(ERR, "Unsupported authentication");
			return -ENOTSUP;
		}

		if (auth_xform->auth.digest_length != KASUMI_DIGEST_LENGTH) {
			IPSEC_MB_LOG(ERR, "Wrong digest length");
			return -EINVAL;
		}

		sess->auth_op = auth_xform->auth.op;

		/* Initialize key */
		IMB_KASUMI_INIT_F9_KEY_SCHED(mgr, auth_xform->auth.key.data,
					      &sess->pKeySched_hash);
	}

	sess->op = mode;
	return ret;
}

/** Encrypt/decrypt mbufs with same cipher key. */
static uint8_t
process_kasumi_cipher_op(struct ipsec_mb_qp *qp, struct rte_crypto_op **ops,
			  struct kasumi_session *session, uint8_t num_ops)
{
	unsigned int i;
	uint8_t processed_ops = 0;
	const void *src[num_ops];
	void *dst[num_ops];
	uint8_t *iv_ptr;
	uint64_t iv[num_ops];
	uint32_t num_bytes[num_ops];

	for (i = 0; i < num_ops; i++) {
		src[i] = rte_pktmbuf_mtod(ops[i]->sym->m_src, uint8_t *)
			 + (ops[i]->sym->cipher.data.offset >> 3);
		dst[i] = ops[i]->sym->m_dst
			     ? rte_pktmbuf_mtod(ops[i]->sym->m_dst, uint8_t *)
				   + (ops[i]->sym->cipher.data.offset >> 3)
			     : rte_pktmbuf_mtod(ops[i]->sym->m_src, uint8_t *)
				   + (ops[i]->sym->cipher.data.offset >> 3);
		iv_ptr = rte_crypto_op_ctod_offset(ops[i], uint8_t *,
						    session->cipher_iv_offset);
		iv[i] = *((uint64_t *)(iv_ptr));
		num_bytes[i] = ops[i]->sym->cipher.data.length >> 3;

		processed_ops++;
	}

	if (processed_ops != 0)
		IMB_KASUMI_F8_N_BUFFER(qp->mb_mgr, &session->pKeySched_cipher,
					iv, src, dst, num_bytes,
					processed_ops);

	return processed_ops;
}

/** Encrypt/decrypt mbuf (bit level function). */
static uint8_t
process_kasumi_cipher_op_bit(struct ipsec_mb_qp *qp, struct rte_crypto_op *op,
			      struct kasumi_session *session)
{
	uint8_t *src, *dst;
	uint8_t *iv_ptr;
	uint64_t iv;
	uint32_t length_in_bits, offset_in_bits;

	offset_in_bits = op->sym->cipher.data.offset;
	src = rte_pktmbuf_mtod(op->sym->m_src, uint8_t *);
	if (op->sym->m_dst == NULL)
		dst = src;
	else
		dst = rte_pktmbuf_mtod(op->sym->m_dst, uint8_t *);
	iv_ptr = rte_crypto_op_ctod_offset(op, uint8_t *,
					    session->cipher_iv_offset);
	iv = *((uint64_t *)(iv_ptr));
	length_in_bits = op->sym->cipher.data.length;

	IMB_KASUMI_F8_1_BUFFER_BIT(qp->mb_mgr, &session->pKeySched_cipher, iv,
				    src, dst, length_in_bits, offset_in_bits);

	return 1;
}

/** Generate/verify hash from mbufs with same hash key. */
static int
process_kasumi_hash_op(struct ipsec_mb_qp *qp, struct rte_crypto_op **ops,
			struct kasumi_session *session, uint8_t num_ops)
{
	unsigned int i;
	uint8_t processed_ops = 0;
	uint8_t *src, *dst;
	uint32_t length_in_bits;
	uint32_t num_bytes;
	struct kasumi_qp_data *qp_data = ipsec_mb_get_qp_private_data(qp);

	for (i = 0; i < num_ops; i++) {
		/* Data must be byte aligned */
		if ((ops[i]->sym->auth.data.offset % BYTE_LEN) != 0) {
			ops[i]->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
			IPSEC_MB_LOG(ERR, "Invalid Offset");
			break;
		}

		length_in_bits = ops[i]->sym->auth.data.length;

		src = rte_pktmbuf_mtod(ops[i]->sym->m_src, uint8_t *)
		      + (ops[i]->sym->auth.data.offset >> 3);
		/* Direction from next bit after end of message */
		num_bytes = length_in_bits >> 3;

		if (session->auth_op == RTE_CRYPTO_AUTH_OP_VERIFY) {
			dst = qp_data->temp_digest;
			IMB_KASUMI_F9_1_BUFFER(qp->mb_mgr,
						&session->pKeySched_hash, src,
						num_bytes, dst);

			/* Verify digest. */
			if (memcmp(dst, ops[i]->sym->auth.digest.data,
				    KASUMI_DIGEST_LENGTH)
			    != 0)
				ops[i]->status
				    = RTE_CRYPTO_OP_STATUS_AUTH_FAILED;
		} else {
			dst = ops[i]->sym->auth.digest.data;

			IMB_KASUMI_F9_1_BUFFER(qp->mb_mgr,
						&session->pKeySched_hash, src,
						num_bytes, dst);
		}
		processed_ops++;
	}

	return processed_ops;
}

/** Process a batch of crypto ops which shares the same session. */
static int
process_ops(struct rte_crypto_op **ops, struct kasumi_session *session,
		struct ipsec_mb_qp *qp, uint8_t num_ops)
{
	unsigned int i;
	unsigned int processed_ops;

	switch (session->op) {
	case IPSEC_MB_OP_ENCRYPT_ONLY:
	case IPSEC_MB_OP_DECRYPT_ONLY:
		processed_ops
		    = process_kasumi_cipher_op(qp, ops, session, num_ops);
		break;
	case IPSEC_MB_OP_HASH_GEN_ONLY:
	case IPSEC_MB_OP_HASH_VERIFY_ONLY:
		processed_ops
		    = process_kasumi_hash_op(qp, ops, session, num_ops);
		break;
	case IPSEC_MB_OP_ENCRYPT_THEN_HASH_GEN:
	case IPSEC_MB_OP_DECRYPT_THEN_HASH_VERIFY:
		processed_ops
		    = process_kasumi_cipher_op(qp, ops, session, num_ops);
		process_kasumi_hash_op(qp, ops, session, processed_ops);
		break;
	case IPSEC_MB_OP_HASH_VERIFY_THEN_DECRYPT:
	case IPSEC_MB_OP_HASH_GEN_THEN_ENCRYPT:
		processed_ops
		    = process_kasumi_hash_op(qp, ops, session, num_ops);
		process_kasumi_cipher_op(qp, ops, session, processed_ops);
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
			memset(session, 0, sizeof(struct kasumi_session));
			rte_mempool_put(qp->sess_mp, ops[i]->sym->session);
			ops[i]->sym->session = NULL;
		}
	}
	return processed_ops;
}

/** Process a crypto op with length/offset in bits. */
static int
process_op_bit(struct rte_crypto_op *op, struct kasumi_session *session,
		struct ipsec_mb_qp *qp)
{
	unsigned int processed_op;

	switch (session->op) {
		/* case KASUMI_OP_ONLY_CIPHER: */
	case IPSEC_MB_OP_ENCRYPT_ONLY:
	case IPSEC_MB_OP_DECRYPT_ONLY:
		processed_op = process_kasumi_cipher_op_bit(qp, op, session);
		break;
	/* case KASUMI_OP_ONLY_AUTH: */
	case IPSEC_MB_OP_HASH_GEN_ONLY:
	case IPSEC_MB_OP_HASH_VERIFY_ONLY:
		processed_op = process_kasumi_hash_op(qp, &op, session, 1);
		break;
	/* case KASUMI_OP_CIPHER_AUTH: */
	case IPSEC_MB_OP_ENCRYPT_THEN_HASH_GEN:
		processed_op = process_kasumi_cipher_op_bit(qp, op, session);
		if (processed_op == 1)
			process_kasumi_hash_op(qp, &op, session, 1);
		break;
	/* case KASUMI_OP_AUTH_CIPHER: */
	case IPSEC_MB_OP_HASH_VERIFY_THEN_DECRYPT:
		processed_op = process_kasumi_hash_op(qp, &op, session, 1);
		if (processed_op == 1)
			process_kasumi_cipher_op_bit(qp, op, session);
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
		memset(CRYPTODEV_GET_SYM_SESS_PRIV(op->sym->session), 0,
			sizeof(struct kasumi_session));
		rte_mempool_put(qp->sess_mp, (void *)op->sym->session);
		op->sym->session = NULL;
	}
	return processed_op;
}

static uint16_t
kasumi_pmd_dequeue_burst(void *queue_pair, struct rte_crypto_op **ops,
			  uint16_t nb_ops)
{
	struct rte_crypto_op *c_ops[nb_ops];
	struct rte_crypto_op *curr_c_op = NULL;

	struct kasumi_session *prev_sess = NULL, *curr_sess = NULL;
	struct ipsec_mb_qp *qp = queue_pair;
	unsigned int i;
	uint8_t burst_size = 0;
	uint8_t processed_ops;
	unsigned int nb_dequeued;

	nb_dequeued = rte_ring_dequeue_burst(qp->ingress_queue,
					      (void **)ops, nb_ops, NULL);
	for (i = 0; i < nb_dequeued; i++) {
		curr_c_op = ops[i];

#ifdef RTE_LIBRTE_PMD_KASUMI_DEBUG
		if (!rte_pktmbuf_is_contiguous(curr_c_op->sym->m_src)
		    || (curr_c_op->sym->m_dst != NULL
			&& !rte_pktmbuf_is_contiguous(
			    curr_c_op->sym->m_dst))) {
			IPSEC_MB_LOG(ERR,
				      "PMD supports only contiguous mbufs, op (%p) provides noncontiguous mbuf as source/destination buffer.",
				      curr_c_op);
			curr_c_op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
			break;
		}
#endif

		/* Set status as enqueued (not processed yet) by default. */
		curr_c_op->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;

		curr_sess = (struct kasumi_session *)
			ipsec_mb_get_session_private(qp, curr_c_op);
		if (unlikely(curr_sess == NULL
			      || curr_sess->op == IPSEC_MB_OP_NOT_SUPPORTED)) {
			curr_c_op->status
			    = RTE_CRYPTO_OP_STATUS_INVALID_SESSION;
			break;
		}

		/* If length/offset is at bit-level, process this buffer alone.
		 */
		if (((curr_c_op->sym->cipher.data.length % BYTE_LEN) != 0)
		    || ((ops[i]->sym->cipher.data.offset % BYTE_LEN) != 0)) {
			/* Process the ops of the previous session. */
			if (prev_sess != NULL) {
				processed_ops = process_ops(c_ops, prev_sess,
						qp, burst_size);
				if (processed_ops < burst_size) {
					burst_size = 0;
					break;
				}

				burst_size = 0;
				prev_sess = NULL;
			}

			processed_ops = process_op_bit(curr_c_op,
					curr_sess, qp);
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
			if (burst_size == KASUMI_MAX_BURST) {
				processed_ops = process_ops(c_ops, prev_sess,
						qp, burst_size);
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
			processed_ops = process_ops(c_ops, prev_sess, qp,
					burst_size);
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
		processed_ops = process_ops(c_ops, prev_sess, qp, burst_size);
	}

	qp->stats.dequeued_count += i;
	return i;
}

struct rte_cryptodev_ops kasumi_pmd_ops = {
	.dev_configure = ipsec_mb_config,
	.dev_start = ipsec_mb_start,
	.dev_stop = ipsec_mb_stop,
	.dev_close = ipsec_mb_close,

	.stats_get = ipsec_mb_stats_get,
	.stats_reset = ipsec_mb_stats_reset,

	.dev_infos_get = ipsec_mb_info_get,

	.queue_pair_setup = ipsec_mb_qp_setup,
	.queue_pair_release = ipsec_mb_qp_release,

	.sym_session_get_size = ipsec_mb_sym_session_get_size,
	.sym_session_configure = ipsec_mb_sym_session_configure,
	.sym_session_clear = ipsec_mb_sym_session_clear
};

struct rte_cryptodev_ops *rte_kasumi_pmd_ops = &kasumi_pmd_ops;

static int
kasumi_probe(struct rte_vdev_device *vdev)
{
	return ipsec_mb_create(vdev, IPSEC_MB_PMD_TYPE_KASUMI);
}

static struct rte_vdev_driver cryptodev_kasumi_pmd_drv = {
	.probe = kasumi_probe,
	.remove = ipsec_mb_remove
};

static struct cryptodev_driver kasumi_crypto_drv;

RTE_PMD_REGISTER_VDEV(CRYPTODEV_NAME_KASUMI_PMD, cryptodev_kasumi_pmd_drv);
RTE_PMD_REGISTER_ALIAS(CRYPTODEV_NAME_KASUMI_PMD, cryptodev_kasumi_pmd);
RTE_PMD_REGISTER_PARAM_STRING(CRYPTODEV_NAME_KASUMI_PMD,
			       "max_nb_queue_pairs=<int> socket_id=<int>");
RTE_PMD_REGISTER_CRYPTO_DRIVER(kasumi_crypto_drv,
				cryptodev_kasumi_pmd_drv.driver,
				pmd_driver_id_kasumi);

/* Constructor function to register kasumi PMD */
RTE_INIT(ipsec_mb_register_kasumi)
{
	struct ipsec_mb_internals *kasumi_data
	    = &ipsec_mb_pmds[IPSEC_MB_PMD_TYPE_KASUMI];

	kasumi_data->caps = kasumi_capabilities;
	kasumi_data->dequeue_burst = kasumi_pmd_dequeue_burst;
	kasumi_data->feature_flags = RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO
				| RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING
				| RTE_CRYPTODEV_FF_NON_BYTE_ALIGNED_DATA
				| RTE_CRYPTODEV_FF_OOP_SGL_IN_LB_OUT
				| RTE_CRYPTODEV_FF_SYM_SESSIONLESS
				| RTE_CRYPTODEV_FF_OOP_LB_IN_LB_OUT;
	kasumi_data->internals_priv_size = 0;
	kasumi_data->ops = &kasumi_pmd_ops;
	kasumi_data->qp_priv_size = sizeof(struct kasumi_qp_data);
	kasumi_data->session_configure = kasumi_session_configure;
	kasumi_data->session_priv_size = sizeof(struct kasumi_session);
}
