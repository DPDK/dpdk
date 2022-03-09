/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2021 Intel Corporation
 */

#include "pmd_aesni_gcm_priv.h"

static void
aesni_gcm_set_ops(struct aesni_gcm_ops *ops, IMB_MGR *mb_mgr)
{
	/* Set 128 bit function pointers. */
	ops[GCM_KEY_128].pre = mb_mgr->gcm128_pre;
	ops[GCM_KEY_128].init = mb_mgr->gcm128_init;

	ops[GCM_KEY_128].enc = mb_mgr->gcm128_enc;
	ops[GCM_KEY_128].update_enc = mb_mgr->gcm128_enc_update;
	ops[GCM_KEY_128].finalize_enc = mb_mgr->gcm128_enc_finalize;

	ops[GCM_KEY_128].dec = mb_mgr->gcm128_dec;
	ops[GCM_KEY_128].update_dec = mb_mgr->gcm128_dec_update;
	ops[GCM_KEY_128].finalize_dec = mb_mgr->gcm128_dec_finalize;

	ops[GCM_KEY_128].gmac_init = mb_mgr->gmac128_init;
	ops[GCM_KEY_128].gmac_update = mb_mgr->gmac128_update;
	ops[GCM_KEY_128].gmac_finalize = mb_mgr->gmac128_finalize;

	/* Set 192 bit function pointers. */
	ops[GCM_KEY_192].pre = mb_mgr->gcm192_pre;
	ops[GCM_KEY_192].init = mb_mgr->gcm192_init;

	ops[GCM_KEY_192].enc = mb_mgr->gcm192_enc;
	ops[GCM_KEY_192].update_enc = mb_mgr->gcm192_enc_update;
	ops[GCM_KEY_192].finalize_enc = mb_mgr->gcm192_enc_finalize;

	ops[GCM_KEY_192].dec = mb_mgr->gcm192_dec;
	ops[GCM_KEY_192].update_dec = mb_mgr->gcm192_dec_update;
	ops[GCM_KEY_192].finalize_dec = mb_mgr->gcm192_dec_finalize;

	ops[GCM_KEY_192].gmac_init = mb_mgr->gmac192_init;
	ops[GCM_KEY_192].gmac_update = mb_mgr->gmac192_update;
	ops[GCM_KEY_192].gmac_finalize = mb_mgr->gmac192_finalize;

	/* Set 256 bit function pointers. */
	ops[GCM_KEY_256].pre = mb_mgr->gcm256_pre;
	ops[GCM_KEY_256].init = mb_mgr->gcm256_init;

	ops[GCM_KEY_256].enc = mb_mgr->gcm256_enc;
	ops[GCM_KEY_256].update_enc = mb_mgr->gcm256_enc_update;
	ops[GCM_KEY_256].finalize_enc = mb_mgr->gcm256_enc_finalize;

	ops[GCM_KEY_256].dec = mb_mgr->gcm256_dec;
	ops[GCM_KEY_256].update_dec = mb_mgr->gcm256_dec_update;
	ops[GCM_KEY_256].finalize_dec = mb_mgr->gcm256_dec_finalize;

	ops[GCM_KEY_256].gmac_init = mb_mgr->gmac256_init;
	ops[GCM_KEY_256].gmac_update = mb_mgr->gmac256_update;
	ops[GCM_KEY_256].gmac_finalize = mb_mgr->gmac256_finalize;
}

static int
aesni_gcm_session_configure(IMB_MGR *mb_mgr, void *session,
			    const struct rte_crypto_sym_xform *xform)
{
	struct aesni_gcm_session *sess = session;
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

	/**< GCM key type */

	sess->op = mode;

	switch (sess->op) {
	case IPSEC_MB_OP_HASH_GEN_ONLY:
	case IPSEC_MB_OP_HASH_VERIFY_ONLY:
		/* AES-GMAC
		 * auth_xform = xform;
		 */
		if (auth_xform->auth.algo != RTE_CRYPTO_AUTH_AES_GMAC) {
			IPSEC_MB_LOG(ERR,
	"Only AES GMAC is supported as an authentication only algorithm");
			ret = -ENOTSUP;
			goto error_exit;
		}
		/* Set IV parameters */
		sess->iv.offset = auth_xform->auth.iv.offset;
		sess->iv.length = auth_xform->auth.iv.length;
		key_length = auth_xform->auth.key.length;
		key = auth_xform->auth.key.data;
		sess->req_digest_length =
		    RTE_MIN(auth_xform->auth.digest_length,
				DIGEST_LENGTH_MAX);
		break;
	case IPSEC_MB_OP_AEAD_AUTHENTICATED_ENCRYPT:
	case IPSEC_MB_OP_AEAD_AUTHENTICATED_DECRYPT:
		/* AES-GCM
		 * aead_xform = xform;
		 */

		if (aead_xform->aead.algo != RTE_CRYPTO_AEAD_AES_GCM) {
			IPSEC_MB_LOG(ERR,
			"The only combined operation supported is AES GCM");
			ret = -ENOTSUP;
			goto error_exit;
		}
		/* Set IV parameters */
		sess->iv.offset = aead_xform->aead.iv.offset;
		sess->iv.length = aead_xform->aead.iv.length;
		key_length = aead_xform->aead.key.length;
		key = aead_xform->aead.key.data;
		sess->aad_length = aead_xform->aead.aad_length;
		sess->req_digest_length =
			RTE_MIN(aead_xform->aead.digest_length,
				DIGEST_LENGTH_MAX);
		break;
	default:
		IPSEC_MB_LOG(
		    ERR, "Wrong xform type, has to be AEAD or authentication");
		ret = -ENOTSUP;
		goto error_exit;
	}

	/* Check key length, and calculate GCM pre-compute. */
	switch (key_length) {
	case 16:
		sess->key_length = GCM_KEY_128;
		mb_mgr->gcm128_pre(key, &sess->gdata_key);
		break;
	case 24:
		sess->key_length = GCM_KEY_192;
		mb_mgr->gcm192_pre(key, &sess->gdata_key);
		break;
	case 32:
		sess->key_length = GCM_KEY_256;
		mb_mgr->gcm256_pre(key, &sess->gdata_key);
		break;
	default:
		IPSEC_MB_LOG(ERR, "Invalid key length");
		ret = -EINVAL;
		goto error_exit;
	}

	/* Digest check */
	if (sess->req_digest_length > 16) {
		IPSEC_MB_LOG(ERR, "Invalid digest length");
		ret = -EINVAL;
		goto error_exit;
	}
	/*
	 * If size requested is different, generate the full digest
	 * (16 bytes) in a temporary location and then memcpy
	 * the requested number of bytes.
	 */
	if (sess->req_digest_length < 4)
		sess->gen_digest_length = 16;
	else
		sess->gen_digest_length = sess->req_digest_length;

error_exit:
	return ret;
}

/**
 * Process a completed job and return rte_mbuf which job processed
 *
 * @param job	IMB_JOB job to process
 *
 * @return
 * - Returns processed mbuf which is trimmed of output digest used in
 * verification of supplied digest in the case of a HASH_CIPHER operation
 * - Returns NULL on invalid job
 */
static void
post_process_gcm_crypto_op(struct ipsec_mb_qp *qp,
		struct rte_crypto_op *op,
		struct aesni_gcm_session *session)
{
	struct aesni_gcm_qp_data *qp_data = ipsec_mb_get_qp_private_data(qp);

	op->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
	/* Verify digest if required */
	if (session->op == IPSEC_MB_OP_AEAD_AUTHENTICATED_DECRYPT ||
			session->op == IPSEC_MB_OP_HASH_VERIFY_ONLY) {
		uint8_t *digest;

		uint8_t *tag = qp_data->temp_digest;

		if (session->op == IPSEC_MB_OP_HASH_VERIFY_ONLY)
			digest = op->sym->auth.digest.data;
		else
			digest = op->sym->aead.digest.data;

#ifdef RTE_LIBRTE_PMD_AESNI_GCM_DEBUG
		rte_hexdump(stdout, "auth tag (orig):",
				digest, session->req_digest_length);
		rte_hexdump(stdout, "auth tag (calc):",
				tag, session->req_digest_length);
#endif

		if (memcmp(tag, digest,	session->req_digest_length) != 0)
			op->status = RTE_CRYPTO_OP_STATUS_AUTH_FAILED;
	} else {
		if (session->req_digest_length != session->gen_digest_length) {
			if (session->op ==
				IPSEC_MB_OP_AEAD_AUTHENTICATED_ENCRYPT)
				memcpy(op->sym->aead.digest.data,
					qp_data->temp_digest,
					session->req_digest_length);
			else
				memcpy(op->sym->auth.digest.data,
					qp_data->temp_digest,
					session->req_digest_length);
		}
	}
}

/**
 * Process a completed GCM request
 *
 * @param qp		Queue Pair to process
 * @param op		Crypto operation
 * @param sess		AESNI-GCM session
 *
 */
static void
handle_completed_gcm_crypto_op(struct ipsec_mb_qp *qp,
		struct rte_crypto_op *op,
		struct aesni_gcm_session *sess)
{
	post_process_gcm_crypto_op(qp, op, sess);

	/* Free session if a session-less crypto op */
	if (op->sess_type == RTE_CRYPTO_OP_SESSIONLESS) {
		memset(sess, 0, sizeof(struct aesni_gcm_session));
		memset(op->sym->session, 0,
			rte_cryptodev_sym_get_existing_header_session_size(
				op->sym->session));
		rte_mempool_put(qp->sess_mp_priv, sess);
		rte_mempool_put(qp->sess_mp, op->sym->session);
		op->sym->session = NULL;
	}
}

/**
 * Process a crypto operation, calling
 * the GCM API from the multi buffer library.
 *
 * @param	qp		queue pair
 * @param	op		symmetric crypto operation
 * @param	session		GCM session
 *
 * @return
 *  0 on success
 */
static int
process_gcm_crypto_op(struct ipsec_mb_qp *qp, struct rte_crypto_op *op,
		struct aesni_gcm_session *session)
{
	struct aesni_gcm_qp_data *qp_data = ipsec_mb_get_qp_private_data(qp);
	uint8_t *src, *dst;
	uint8_t *iv_ptr;
	struct rte_crypto_sym_op *sym_op = op->sym;
	struct rte_mbuf *m_src = sym_op->m_src;
	uint32_t offset, data_offset, data_length;
	uint32_t part_len, total_len, data_len;
	uint8_t *tag;
	unsigned int oop = 0;
	struct aesni_gcm_ops *ops = &qp_data->ops[session->key_length];

	if (session->op == IPSEC_MB_OP_AEAD_AUTHENTICATED_ENCRYPT ||
			session->op == IPSEC_MB_OP_AEAD_AUTHENTICATED_DECRYPT) {
		offset = sym_op->aead.data.offset;
		data_offset = offset;
		data_length = sym_op->aead.data.length;
	} else {
		offset = sym_op->auth.data.offset;
		data_offset = offset;
		data_length = sym_op->auth.data.length;
	}

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

	RTE_ASSERT((sym_op->m_dst == NULL) ||
			((sym_op->m_dst != NULL) &&
				rte_pktmbuf_is_contiguous(sym_op->m_dst)));

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

	if (session->op == IPSEC_MB_OP_AEAD_AUTHENTICATED_ENCRYPT) {
		ops->init(&session->gdata_key, &qp_data->gcm_ctx_data, iv_ptr,
				sym_op->aead.aad.data,
				(uint64_t)session->aad_length);

		ops->update_enc(&session->gdata_key, &qp_data->gcm_ctx_data,
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

			ops->update_enc(&session->gdata_key,
					&qp_data->gcm_ctx_data,
					dst, src, (uint64_t)part_len);
			total_len -= part_len;
		}

		if (session->req_digest_length != session->gen_digest_length)
			tag = qp_data->temp_digest;
		else
			tag = sym_op->aead.digest.data;

		ops->finalize_enc(&session->gdata_key, &qp_data->gcm_ctx_data,
				tag, session->gen_digest_length);
	} else if (session->op == IPSEC_MB_OP_AEAD_AUTHENTICATED_DECRYPT) {
		ops->init(&session->gdata_key, &qp_data->gcm_ctx_data, iv_ptr,
				sym_op->aead.aad.data,
				(uint64_t)session->aad_length);

		ops->update_dec(&session->gdata_key, &qp_data->gcm_ctx_data,
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

			ops->update_dec(&session->gdata_key,
					&qp_data->gcm_ctx_data,
					dst, src, (uint64_t)part_len);
			total_len -= part_len;
		}

		tag = qp_data->temp_digest;
		ops->finalize_dec(&session->gdata_key, &qp_data->gcm_ctx_data,
				tag, session->gen_digest_length);
	} else if (session->op == IPSEC_MB_OP_HASH_GEN_ONLY) {
		ops->gmac_init(&session->gdata_key, &qp_data->gcm_ctx_data,
				iv_ptr, session->iv.length);

		ops->gmac_update(&session->gdata_key, &qp_data->gcm_ctx_data,
				src, (uint64_t)part_len);
		total_len = data_length - part_len;

		while (total_len) {
			m_src = m_src->next;

			RTE_ASSERT(m_src != NULL);

			src = rte_pktmbuf_mtod(m_src, uint8_t *);
			part_len = (m_src->data_len < total_len) ?
					m_src->data_len : total_len;

			ops->gmac_update(&session->gdata_key,
					&qp_data->gcm_ctx_data, src,
					(uint64_t)part_len);
			total_len -= part_len;
		}

		if (session->req_digest_length != session->gen_digest_length)
			tag = qp_data->temp_digest;
		else
			tag = sym_op->auth.digest.data;

		ops->gmac_finalize(&session->gdata_key, &qp_data->gcm_ctx_data,
				tag, session->gen_digest_length);
	} else { /* IPSEC_MB_OP_HASH_VERIFY_ONLY */
		ops->gmac_init(&session->gdata_key, &qp_data->gcm_ctx_data,
				iv_ptr, session->iv.length);

		ops->gmac_update(&session->gdata_key, &qp_data->gcm_ctx_data,
				src, (uint64_t)part_len);
		total_len = data_length - part_len;

		while (total_len) {
			m_src = m_src->next;

			RTE_ASSERT(m_src != NULL);

			src = rte_pktmbuf_mtod(m_src, uint8_t *);
			part_len = (m_src->data_len < total_len) ?
					m_src->data_len : total_len;

			ops->gmac_update(&session->gdata_key,
					&qp_data->gcm_ctx_data, src,
					(uint64_t)part_len);
			total_len -= part_len;
		}

		tag = qp_data->temp_digest;

		ops->gmac_finalize(&session->gdata_key, &qp_data->gcm_ctx_data,
				tag, session->gen_digest_length);
	}
	return 0;
}

/** Get gcm session */
static inline struct aesni_gcm_session *
aesni_gcm_get_session(struct ipsec_mb_qp *qp,
	     struct rte_crypto_op *op)
{
	struct aesni_gcm_session *sess = NULL;
	uint32_t driver_id =
	    ipsec_mb_get_driver_id(IPSEC_MB_PMD_TYPE_AESNI_GCM);
	struct rte_crypto_sym_op *sym_op = op->sym;

	if (op->sess_type == RTE_CRYPTO_OP_WITH_SESSION) {
		if (likely(sym_op->session != NULL))
			sess = (struct aesni_gcm_session *)
			    get_sym_session_private_data(sym_op->session,
							 driver_id);
	} else {
		void *_sess;
		void *_sess_private_data = NULL;

		if (rte_mempool_get(qp->sess_mp, (void **)&_sess))
			return NULL;

		if (rte_mempool_get(qp->sess_mp_priv,
				(void **)&_sess_private_data))
			return NULL;

		sess = (struct aesni_gcm_session *)_sess_private_data;

		if (unlikely(aesni_gcm_session_configure(qp->mb_mgr,
				 _sess_private_data, sym_op->xform) != 0)) {
			rte_mempool_put(qp->sess_mp, _sess);
			rte_mempool_put(qp->sess_mp_priv, _sess_private_data);
			sess = NULL;
		}
		sym_op->session = (struct rte_cryptodev_sym_session *)_sess;
		set_sym_session_private_data(sym_op->session, driver_id,
					     _sess_private_data);
	}

	if (unlikely(sess == NULL))
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_SESSION;

	return sess;
}

static uint16_t
aesni_gcm_pmd_dequeue_burst(void *queue_pair,
		struct rte_crypto_op **ops, uint16_t nb_ops)
{
	struct aesni_gcm_session *sess;
	struct ipsec_mb_qp *qp = queue_pair;

	int retval = 0;
	unsigned int i, nb_dequeued;

	nb_dequeued = rte_ring_dequeue_burst(qp->ingress_queue,
			(void **)ops, nb_ops, NULL);

	for (i = 0; i < nb_dequeued; i++) {

		sess = aesni_gcm_get_session(qp, ops[i]);
		if (unlikely(sess == NULL)) {
			ops[i]->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
			qp->stats.dequeue_err_count++;
			break;
		}

		retval = process_gcm_crypto_op(qp, ops[i], sess);
		if (retval < 0) {
			ops[i]->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
			qp->stats.dequeue_err_count++;
			break;
		}

		handle_completed_gcm_crypto_op(qp, ops[i], sess);
	}

	qp->stats.dequeued_count += i;

	return i;
}

static inline void
aesni_gcm_fill_error_code(struct rte_crypto_sym_vec *vec,
			  int32_t errnum)
{
	uint32_t i;

	for (i = 0; i < vec->num; i++)
		vec->status[i] = errnum;
}

static inline int32_t
aesni_gcm_sgl_op_finalize_encryption(const struct aesni_gcm_session *s,
				     struct gcm_context_data *gdata_ctx,
				     uint8_t *digest, struct aesni_gcm_ops ops)
{
	if (s->req_digest_length != s->gen_digest_length) {
		uint8_t tmpdigest[s->gen_digest_length];

		ops.finalize_enc(&s->gdata_key, gdata_ctx, tmpdigest,
				s->gen_digest_length);
		memcpy(digest, tmpdigest, s->req_digest_length);
	} else {
		ops.finalize_enc(&s->gdata_key, gdata_ctx, digest,
				s->gen_digest_length);
	}

	return 0;
}

static inline int32_t
aesni_gcm_sgl_op_finalize_decryption(const struct aesni_gcm_session *s,
				     struct gcm_context_data *gdata_ctx,
				     uint8_t *digest, struct aesni_gcm_ops ops)
{
	uint8_t tmpdigest[s->gen_digest_length];

	ops.finalize_dec(&s->gdata_key, gdata_ctx, tmpdigest,
			s->gen_digest_length);

	return memcmp(digest, tmpdigest, s->req_digest_length) == 0 ? 0
								    : EBADMSG;
}

static inline void
aesni_gcm_process_gcm_sgl_op(const struct aesni_gcm_session *s,
			     struct gcm_context_data *gdata_ctx,
			     struct rte_crypto_sgl *sgl, void *iv, void *aad,
			     struct aesni_gcm_ops ops)
{
	uint32_t i;

	/* init crypto operation */
	ops.init(&s->gdata_key, gdata_ctx, iv, aad,
		    (uint64_t)s->aad_length);

	/* update with sgl data */
	for (i = 0; i < sgl->num; i++) {
		struct rte_crypto_vec *vec = &sgl->vec[i];

		switch (s->op) {
		case IPSEC_MB_OP_AEAD_AUTHENTICATED_ENCRYPT:
			ops.update_enc(&s->gdata_key, gdata_ctx,
			      vec->base, vec->base, vec->len);
			break;
		case IPSEC_MB_OP_AEAD_AUTHENTICATED_DECRYPT:
			ops.update_dec(&s->gdata_key, gdata_ctx,
			      vec->base, vec->base, vec->len);
			break;
		default:
			IPSEC_MB_LOG(ERR, "Invalid session op");
			break;
		}

	}
}

static inline void
aesni_gcm_process_gmac_sgl_op(const struct aesni_gcm_session *s,
			      struct gcm_context_data *gdata_ctx,
			      struct rte_crypto_sgl *sgl, void *iv,
			      struct aesni_gcm_ops ops)
{
	ops.init(&s->gdata_key, gdata_ctx, iv, sgl->vec[0].base,
		    sgl->vec[0].len);
}

static inline uint32_t
aesni_gcm_sgl_encrypt(struct aesni_gcm_session *s,
		      struct gcm_context_data *gdata_ctx,
		      struct rte_crypto_sym_vec *vec,
		      struct aesni_gcm_ops ops)
{
	uint32_t i, processed;

	processed = 0;
	for (i = 0; i < vec->num; ++i) {
		aesni_gcm_process_gcm_sgl_op(s, gdata_ctx, &vec->src_sgl[i],
					     vec->iv[i].va, vec->aad[i].va,
					     ops);
		vec->status[i] = aesni_gcm_sgl_op_finalize_encryption(
		    s, gdata_ctx, vec->digest[i].va, ops);
		processed += (vec->status[i] == 0);
	}

	return processed;
}

static inline uint32_t
aesni_gcm_sgl_decrypt(struct aesni_gcm_session *s,
		      struct gcm_context_data *gdata_ctx,
		      struct rte_crypto_sym_vec *vec,
		      struct aesni_gcm_ops ops)
{
	uint32_t i, processed;

	processed = 0;
	for (i = 0; i < vec->num; ++i) {
		aesni_gcm_process_gcm_sgl_op(s, gdata_ctx, &vec->src_sgl[i],
					     vec->iv[i].va, vec->aad[i].va,
					     ops);
		vec->status[i] = aesni_gcm_sgl_op_finalize_decryption(
		    s, gdata_ctx, vec->digest[i].va, ops);
		processed += (vec->status[i] == 0);
	}

	return processed;
}

static inline uint32_t
aesni_gmac_sgl_generate(struct aesni_gcm_session *s,
			struct gcm_context_data *gdata_ctx,
			struct rte_crypto_sym_vec *vec,
			struct aesni_gcm_ops ops)
{
	uint32_t i, processed;

	processed = 0;
	for (i = 0; i < vec->num; ++i) {
		if (vec->src_sgl[i].num != 1) {
			vec->status[i] = ENOTSUP;
			continue;
		}

		aesni_gcm_process_gmac_sgl_op(s, gdata_ctx, &vec->src_sgl[i],
					      vec->iv[i].va, ops);
		vec->status[i] = aesni_gcm_sgl_op_finalize_encryption(
		    s, gdata_ctx, vec->digest[i].va, ops);
		processed += (vec->status[i] == 0);
	}

	return processed;
}

static inline uint32_t
aesni_gmac_sgl_verify(struct aesni_gcm_session *s,
		      struct gcm_context_data *gdata_ctx,
		      struct rte_crypto_sym_vec *vec,
		      struct aesni_gcm_ops ops)
{
	uint32_t i, processed;

	processed = 0;
	for (i = 0; i < vec->num; ++i) {
		if (vec->src_sgl[i].num != 1) {
			vec->status[i] = ENOTSUP;
			continue;
		}

		aesni_gcm_process_gmac_sgl_op(s, gdata_ctx, &vec->src_sgl[i],
					      vec->iv[i].va, ops);
		vec->status[i] = aesni_gcm_sgl_op_finalize_decryption(
		    s, gdata_ctx, vec->digest[i].va, ops);
		processed += (vec->status[i] == 0);
	}

	return processed;
}

/** Process CPU crypto bulk operations */
static uint32_t
aesni_gcm_process_bulk(struct rte_cryptodev *dev,
			struct rte_cryptodev_sym_session *sess,
			__rte_unused union rte_crypto_sym_ofs ofs,
			struct rte_crypto_sym_vec *vec)
{
	struct aesni_gcm_session *s;
	struct gcm_context_data gdata_ctx;
	IMB_MGR *mb_mgr;

	s = (struct aesni_gcm_session *) get_sym_session_private_data(sess,
		dev->driver_id);
	if (unlikely(s == NULL)) {
		aesni_gcm_fill_error_code(vec, EINVAL);
		return 0;
	}

	/* get per-thread MB MGR, create one if needed */
	mb_mgr = get_per_thread_mb_mgr();
	if (unlikely(mb_mgr == NULL))
		return 0;

	/* Check if function pointers have been set for this thread ops. */
	if (unlikely(RTE_PER_LCORE(gcm_ops)[s->key_length].init == NULL))
		aesni_gcm_set_ops(RTE_PER_LCORE(gcm_ops), mb_mgr);

	switch (s->op) {
	case IPSEC_MB_OP_AEAD_AUTHENTICATED_ENCRYPT:
		return aesni_gcm_sgl_encrypt(s, &gdata_ctx, vec,
				RTE_PER_LCORE(gcm_ops)[s->key_length]);
	case IPSEC_MB_OP_AEAD_AUTHENTICATED_DECRYPT:
		return aesni_gcm_sgl_decrypt(s, &gdata_ctx, vec,
				RTE_PER_LCORE(gcm_ops)[s->key_length]);
	case IPSEC_MB_OP_HASH_GEN_ONLY:
		return aesni_gmac_sgl_generate(s, &gdata_ctx, vec,
				RTE_PER_LCORE(gcm_ops)[s->key_length]);
	case IPSEC_MB_OP_HASH_VERIFY_ONLY:
		return aesni_gmac_sgl_verify(s, &gdata_ctx, vec,
				RTE_PER_LCORE(gcm_ops)[s->key_length]);
	default:
		aesni_gcm_fill_error_code(vec, EINVAL);
		return 0;
	}
}

static int
aesni_gcm_qp_setup(struct rte_cryptodev *dev, uint16_t qp_id,
				const struct rte_cryptodev_qp_conf *qp_conf,
				int socket_id)
{
	int ret = ipsec_mb_qp_setup(dev, qp_id, qp_conf, socket_id);
	if (ret < 0)
		return ret;

	struct ipsec_mb_qp *qp = dev->data->queue_pairs[qp_id];
	struct aesni_gcm_qp_data *qp_data = ipsec_mb_get_qp_private_data(qp);
	aesni_gcm_set_ops(qp_data->ops, qp->mb_mgr);
	return 0;
}

struct rte_cryptodev_ops aesni_gcm_pmd_ops = {
	.dev_configure = ipsec_mb_config,
	.dev_start = ipsec_mb_start,
	.dev_stop = ipsec_mb_stop,
	.dev_close = ipsec_mb_close,

	.stats_get = ipsec_mb_stats_get,
	.stats_reset = ipsec_mb_stats_reset,

	.dev_infos_get = ipsec_mb_info_get,

	.queue_pair_setup = aesni_gcm_qp_setup,
	.queue_pair_release = ipsec_mb_qp_release,

	.sym_cpu_process = aesni_gcm_process_bulk,

	.sym_session_get_size = ipsec_mb_sym_session_get_size,
	.sym_session_configure = ipsec_mb_sym_session_configure,
	.sym_session_clear = ipsec_mb_sym_session_clear
};

static int
aesni_gcm_probe(struct rte_vdev_device *vdev)
{
	return ipsec_mb_create(vdev, IPSEC_MB_PMD_TYPE_AESNI_GCM);
}

static struct rte_vdev_driver cryptodev_aesni_gcm_pmd_drv = {
	.probe = aesni_gcm_probe,
	.remove = ipsec_mb_remove
};

static struct cryptodev_driver aesni_gcm_crypto_drv;

RTE_PMD_REGISTER_VDEV(CRYPTODEV_NAME_AESNI_GCM_PMD,
		      cryptodev_aesni_gcm_pmd_drv);
RTE_PMD_REGISTER_ALIAS(CRYPTODEV_NAME_AESNI_GCM_PMD, cryptodev_aesni_gcm_pmd);
RTE_PMD_REGISTER_PARAM_STRING(CRYPTODEV_NAME_AESNI_GCM_PMD,
			      "max_nb_queue_pairs=<int> socket_id=<int>");
RTE_PMD_REGISTER_CRYPTO_DRIVER(aesni_gcm_crypto_drv,
			       cryptodev_aesni_gcm_pmd_drv.driver,
			       pmd_driver_id_aesni_gcm);

/* Constructor function to register aesni-gcm PMD */
RTE_INIT(ipsec_mb_register_aesni_gcm)
{
	struct ipsec_mb_internals *aesni_gcm_data =
		&ipsec_mb_pmds[IPSEC_MB_PMD_TYPE_AESNI_GCM];

	aesni_gcm_data->caps = aesni_gcm_capabilities;
	aesni_gcm_data->dequeue_burst = aesni_gcm_pmd_dequeue_burst;
	aesni_gcm_data->feature_flags =
		RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO |
		RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING |
		RTE_CRYPTODEV_FF_IN_PLACE_SGL |
		RTE_CRYPTODEV_FF_OOP_SGL_IN_LB_OUT |
		RTE_CRYPTODEV_FF_OOP_LB_IN_LB_OUT |
		RTE_CRYPTODEV_FF_SYM_CPU_CRYPTO |
		RTE_CRYPTODEV_FF_SYM_SESSIONLESS;
	aesni_gcm_data->internals_priv_size = 0;
	aesni_gcm_data->ops = &aesni_gcm_pmd_ops;
	aesni_gcm_data->qp_priv_size = sizeof(struct aesni_gcm_qp_data);
	aesni_gcm_data->queue_pair_configure = NULL;
	aesni_gcm_data->session_configure = aesni_gcm_session_configure;
	aesni_gcm_data->session_priv_size = sizeof(struct aesni_gcm_session);
}
