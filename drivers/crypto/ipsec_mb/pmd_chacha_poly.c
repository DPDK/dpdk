/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

#include "pmd_chacha_poly_priv.h"

/** Parse crypto xform chain and set private session parameters. */
static int
chacha20_poly1305_session_configure(IMB_MGR * mb_mgr __rte_unused,
		void *priv_sess, const struct rte_crypto_sym_xform *xform)
{
	struct chacha20_poly1305_session *sess = priv_sess;
	const struct rte_crypto_sym_xform *auth_xform;
	const struct rte_crypto_sym_xform *cipher_xform;
	const struct rte_crypto_sym_xform *aead_xform;

	uint8_t key_length;
	const uint8_t *key;
	enum ipsec_mb_operation mode;
	int ret = 0;

	ret = ipsec_mb_parse_xform(xform, &mode, &auth_xform,
				&cipher_xform, &aead_xform);
	if (ret)
		return ret;

	sess->op = mode;

	switch (sess->op) {
	case IPSEC_MB_OP_AEAD_AUTHENTICATED_ENCRYPT:
	case IPSEC_MB_OP_AEAD_AUTHENTICATED_DECRYPT:
		if (aead_xform->aead.algo !=
				RTE_CRYPTO_AEAD_CHACHA20_POLY1305) {
			IPSEC_MB_LOG(ERR,
			"The only combined operation supported is CHACHA20 POLY1305");
			ret = -ENOTSUP;
			goto error_exit;
		}
		/* Set IV parameters */
		sess->iv.offset = aead_xform->aead.iv.offset;
		sess->iv.length = aead_xform->aead.iv.length;
		key_length = aead_xform->aead.key.length;
		key = aead_xform->aead.key.data;
		sess->aad_length = aead_xform->aead.aad_length;
		sess->req_digest_length = aead_xform->aead.digest_length;
		break;
	default:
		IPSEC_MB_LOG(
		    ERR, "Wrong xform type, has to be AEAD or authentication");
		ret = -ENOTSUP;
		goto error_exit;
	}

	/* IV check */
	if (sess->iv.length != CHACHA20_POLY1305_IV_LENGTH &&
		sess->iv.length != 0) {
		IPSEC_MB_LOG(ERR, "Wrong IV length");
		ret = -EINVAL;
		goto error_exit;
	}

	/* Check key length */
	if (key_length != CHACHA20_POLY1305_KEY_SIZE) {
		IPSEC_MB_LOG(ERR, "Invalid key length");
		ret = -EINVAL;
		goto error_exit;
	} else {
		memcpy(sess->key, key, CHACHA20_POLY1305_KEY_SIZE);
	}

	/* Digest check */
	if (sess->req_digest_length !=  CHACHA20_POLY1305_DIGEST_LENGTH) {
		IPSEC_MB_LOG(ERR, "Invalid digest length");
		ret = -EINVAL;
		goto error_exit;
	} else {
		sess->gen_digest_length = CHACHA20_POLY1305_DIGEST_LENGTH;
	}

error_exit:
	return ret;
}

/**
 * Process a crypto operation, calling
 * the direct chacha poly API from the multi buffer library.
 *
 * @param	qp		queue pair
 * @param	op		symmetric crypto operation
 * @param	session		chacha poly session
 *
 * @return
 * - Return 0 if success
 */
static int
chacha20_poly1305_crypto_op(struct ipsec_mb_qp *qp, struct rte_crypto_op *op,
		struct chacha20_poly1305_session *session)
{
	struct chacha20_poly1305_qp_data *qp_data =
					ipsec_mb_get_qp_private_data(qp);
	uint8_t *src, *dst;
	uint8_t *iv_ptr;
	struct rte_crypto_sym_op *sym_op = op->sym;
	struct rte_mbuf *m_src = sym_op->m_src;
	uint32_t offset, data_offset, data_length;
	uint32_t part_len, data_len;
	int total_len;
	uint8_t *tag;
	unsigned int oop = 0;

	offset = sym_op->aead.data.offset;
	data_offset = offset;
	data_length = sym_op->aead.data.length;
	RTE_ASSERT(m_src != NULL);

	while (offset >= m_src->data_len && data_length != 0) {
		offset -= m_src->data_len;
		m_src = m_src->next;

		RTE_ASSERT(m_src != NULL);
	}

	src = rte_pktmbuf_mtod_offset(m_src, uint8_t *, offset);

	data_len = m_src->data_len - offset;
	part_len = (data_len < data_length) ? data_len :
			data_length;

	/* In-place */
	if (sym_op->m_dst == NULL || (sym_op->m_dst == sym_op->m_src))
		dst = src;
	/* Out-of-place */
	else {
		oop = 1;
		/* Segmented destination buffer is not supported
		 * if operation is Out-of-place
		 */
		RTE_ASSERT(rte_pktmbuf_is_contiguous(sym_op->m_dst));
		dst = rte_pktmbuf_mtod_offset(sym_op->m_dst, uint8_t *,
					data_offset);
	}

	iv_ptr = rte_crypto_op_ctod_offset(op, uint8_t *,
				session->iv.offset);

	IMB_CHACHA20_POLY1305_INIT(qp->mb_mgr, session->key,
				&qp_data->chacha20_poly1305_ctx_data,
				iv_ptr,	sym_op->aead.aad.data,
				(uint64_t)session->aad_length);

	if (session->op == IPSEC_MB_OP_AEAD_AUTHENTICATED_ENCRYPT) {
		IMB_CHACHA20_POLY1305_ENC_UPDATE(qp->mb_mgr,
				session->key,
				&qp_data->chacha20_poly1305_ctx_data,
				dst, src, (uint64_t)part_len);
		total_len = data_length - part_len;

		while (total_len) {
			m_src = m_src->next;
			RTE_ASSERT(m_src != NULL);

			src = rte_pktmbuf_mtod(m_src, uint8_t *);
			if (oop)
				dst += part_len;
			else
				dst = src;
			part_len = (m_src->data_len < total_len) ?
					m_src->data_len : total_len;

			if (dst == NULL || src == NULL) {
				IPSEC_MB_LOG(ERR, "Invalid src or dst input");
				return -EINVAL;
			}
			IMB_CHACHA20_POLY1305_ENC_UPDATE(qp->mb_mgr,
					session->key,
					&qp_data->chacha20_poly1305_ctx_data,
					dst, src, (uint64_t)part_len);
			total_len -= part_len;
			if (total_len < 0) {
				IPSEC_MB_LOG(ERR, "Invalid part len");
				return -EINVAL;
			}
		}

		tag = sym_op->aead.digest.data;
		IMB_CHACHA20_POLY1305_ENC_FINALIZE(qp->mb_mgr,
					&qp_data->chacha20_poly1305_ctx_data,
					tag, session->gen_digest_length);

	} else {
		IMB_CHACHA20_POLY1305_DEC_UPDATE(qp->mb_mgr,
					session->key,
					&qp_data->chacha20_poly1305_ctx_data,
					dst, src, (uint64_t)part_len);

		total_len = data_length - part_len;

		while (total_len) {
			m_src = m_src->next;

			RTE_ASSERT(m_src != NULL);

			src = rte_pktmbuf_mtod(m_src, uint8_t *);
			if (oop)
				dst += part_len;
			else
				dst = src;
			part_len = (m_src->data_len < total_len) ?
					m_src->data_len : total_len;

			if (dst == NULL || src == NULL) {
				IPSEC_MB_LOG(ERR, "Invalid src or dst input");
				return -EINVAL;
			}
			IMB_CHACHA20_POLY1305_DEC_UPDATE(qp->mb_mgr,
					session->key,
					&qp_data->chacha20_poly1305_ctx_data,
					dst, src, (uint64_t)part_len);
			total_len -= part_len;
			if (total_len < 0) {
				IPSEC_MB_LOG(ERR, "Invalid part len");
				return -EINVAL;
			}
		}

		tag = qp_data->temp_digest;
		IMB_CHACHA20_POLY1305_DEC_FINALIZE(qp->mb_mgr,
					&qp_data->chacha20_poly1305_ctx_data,
					tag, session->gen_digest_length);
	}

	return 0;
}

/**
 * Process a completed chacha poly op
 *
 * @param qp		Queue Pair to process
 * @param op		Crypto operation
 * @param sess		Crypto session
 *
 * @return
 * - void
 */
static void
post_process_chacha20_poly1305_crypto_op(struct ipsec_mb_qp *qp,
		struct rte_crypto_op *op,
		struct chacha20_poly1305_session *session)
{
	struct chacha20_poly1305_qp_data *qp_data =
					ipsec_mb_get_qp_private_data(qp);

	op->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
	/* Verify digest if required */
	if (session->op == IPSEC_MB_OP_AEAD_AUTHENTICATED_DECRYPT ||
			session->op == IPSEC_MB_OP_HASH_VERIFY_ONLY) {
		uint8_t *digest = op->sym->aead.digest.data;
		uint8_t *tag = qp_data->temp_digest;

#ifdef RTE_LIBRTE_PMD_CHACHA20_POLY1305_DEBUG
		rte_hexdump(stdout, "auth tag (orig):",
				digest, session->req_digest_length);
		rte_hexdump(stdout, "auth tag (calc):",
				tag, session->req_digest_length);
#endif
		if (memcmp(tag, digest,	session->req_digest_length) != 0)
			op->status = RTE_CRYPTO_OP_STATUS_AUTH_FAILED;

	}

}

/**
 * Process a completed Chacha20_poly1305 request
 *
 * @param qp		Queue Pair to process
 * @param op		Crypto operation
 * @param sess		Crypto session
 *
 * @return
 * - void
 */
static void
handle_completed_chacha20_poly1305_crypto_op(struct ipsec_mb_qp *qp,
		struct rte_crypto_op *op,
		struct chacha20_poly1305_session *sess)
{
	post_process_chacha20_poly1305_crypto_op(qp, op, sess);

	/* Free session if a session-less crypto op */
	if (op->sess_type == RTE_CRYPTO_OP_SESSIONLESS) {
		memset(sess, 0, sizeof(struct chacha20_poly1305_session));
		memset(op->sym->session, 0,
			rte_cryptodev_sym_get_existing_header_session_size(
				op->sym->session));
		rte_mempool_put(qp->sess_mp_priv, sess);
		rte_mempool_put(qp->sess_mp, op->sym->session);
		op->sym->session = NULL;
	}
}

static uint16_t
chacha20_poly1305_pmd_dequeue_burst(void *queue_pair,
		struct rte_crypto_op **ops, uint16_t nb_ops)
{
	struct chacha20_poly1305_session *sess;
	struct ipsec_mb_qp *qp = queue_pair;

	int retval = 0;
	unsigned int i = 0, nb_dequeued;

	nb_dequeued = rte_ring_dequeue_burst(qp->ingress_queue,
			(void **)ops, nb_ops, NULL);

	for (i = 0; i < nb_dequeued; i++) {

		sess = ipsec_mb_get_session_private(qp, ops[i]);
		if (unlikely(sess == NULL)) {
			ops[i]->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
			qp->stats.dequeue_err_count++;
			break;
		}

		retval = chacha20_poly1305_crypto_op(qp, ops[i], sess);
		if (retval < 0) {
			ops[i]->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
			qp->stats.dequeue_err_count++;
			break;
		}

		handle_completed_chacha20_poly1305_crypto_op(qp, ops[i], sess);
	}

	qp->stats.dequeued_count += i;

	return i;
}

struct rte_cryptodev_ops chacha20_poly1305_pmd_ops = {
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

struct rte_cryptodev_ops *rte_chacha20_poly1305_pmd_ops =
						&chacha20_poly1305_pmd_ops;

static int
chacha20_poly1305_probe(struct rte_vdev_device *vdev)
{
	return ipsec_mb_create(vdev, IPSEC_MB_PMD_TYPE_CHACHA20_POLY1305);
}

static struct rte_vdev_driver cryptodev_chacha20_poly1305_pmd_drv = {
	.probe = chacha20_poly1305_probe,
	.remove = ipsec_mb_remove
};

static struct cryptodev_driver chacha20_poly1305_crypto_drv;

RTE_PMD_REGISTER_VDEV(CRYPTODEV_NAME_CHACHA20_POLY1305_PMD,
					cryptodev_chacha20_poly1305_pmd_drv);
RTE_PMD_REGISTER_ALIAS(CRYPTODEV_NAME_CHACHA20_POLY1305_PMD,
					cryptodev_chacha20_poly1305_pmd);
RTE_PMD_REGISTER_PARAM_STRING(CRYPTODEV_NAME_CHACHA20_POLY1305_PMD,
			       "max_nb_queue_pairs=<int> socket_id=<int>");
RTE_PMD_REGISTER_CRYPTO_DRIVER(chacha20_poly1305_crypto_drv,
				cryptodev_chacha20_poly1305_pmd_drv.driver,
				pmd_driver_id_chacha20_poly1305);

/* Constructor function to register chacha20_poly1305 PMD */
RTE_INIT(ipsec_mb_register_chacha20_poly1305)
{
	struct ipsec_mb_internals *chacha_poly_data
		= &ipsec_mb_pmds[IPSEC_MB_PMD_TYPE_CHACHA20_POLY1305];

	chacha_poly_data->caps = chacha20_poly1305_capabilities;
	chacha_poly_data->dequeue_burst = chacha20_poly1305_pmd_dequeue_burst;
	chacha_poly_data->feature_flags =
		RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO |
		RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING |
		RTE_CRYPTODEV_FF_IN_PLACE_SGL |
		RTE_CRYPTODEV_FF_OOP_SGL_IN_LB_OUT |
		RTE_CRYPTODEV_FF_OOP_LB_IN_LB_OUT |
		RTE_CRYPTODEV_FF_SYM_CPU_CRYPTO |
		RTE_CRYPTODEV_FF_SYM_SESSIONLESS;
	chacha_poly_data->internals_priv_size = 0;
	chacha_poly_data->ops = &chacha20_poly1305_pmd_ops;
	chacha_poly_data->qp_priv_size =
			sizeof(struct chacha20_poly1305_qp_data);
	chacha_poly_data->session_configure =
			chacha20_poly1305_session_configure;
	chacha_poly_data->session_priv_size =
			sizeof(struct chacha20_poly1305_session);
}
