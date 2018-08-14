/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */

#include <rte_common.h>
#include <rte_hexdump.h>
#include <rte_cryptodev.h>
#include <rte_cryptodev_pmd.h>
#include <rte_bus_vdev.h>
#include <rte_malloc.h>
#include <rte_cpuflags.h>
#include <rte_byteorder.h>

#include "aesni_gcm_pmd_private.h"

static uint8_t cryptodev_driver_id;

/** Parse crypto xform chain and set private session parameters */
int
aesni_gcm_set_session_parameters(const struct aesni_gcm_ops *gcm_ops,
		struct aesni_gcm_session *sess,
		const struct rte_crypto_sym_xform *xform)
{
	const struct rte_crypto_sym_xform *auth_xform;
	const struct rte_crypto_sym_xform *aead_xform;
	uint8_t key_length;
	uint8_t *key;

	/* AES-GMAC */
	if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH) {
		auth_xform = xform;
		if (auth_xform->auth.algo != RTE_CRYPTO_AUTH_AES_GMAC) {
			AESNI_GCM_LOG(ERR, "Only AES GMAC is supported as an "
				"authentication only algorithm");
			return -ENOTSUP;
		}
		/* Set IV parameters */
		sess->iv.offset = auth_xform->auth.iv.offset;
		sess->iv.length = auth_xform->auth.iv.length;

		/* Select Crypto operation */
		if (auth_xform->auth.op == RTE_CRYPTO_AUTH_OP_GENERATE)
			sess->op = AESNI_GMAC_OP_GENERATE;
		else
			sess->op = AESNI_GMAC_OP_VERIFY;

		key_length = auth_xform->auth.key.length;
		key = auth_xform->auth.key.data;
		sess->req_digest_length = auth_xform->auth.digest_length;

	/* AES-GCM */
	} else if (xform->type == RTE_CRYPTO_SYM_XFORM_AEAD) {
		aead_xform = xform;

		if (aead_xform->aead.algo != RTE_CRYPTO_AEAD_AES_GCM) {
			AESNI_GCM_LOG(ERR, "The only combined operation "
						"supported is AES GCM");
			return -ENOTSUP;
		}

		/* Set IV parameters */
		sess->iv.offset = aead_xform->aead.iv.offset;
		sess->iv.length = aead_xform->aead.iv.length;

		/* Select Crypto operation */
		if (aead_xform->aead.op == RTE_CRYPTO_AEAD_OP_ENCRYPT)
			sess->op = AESNI_GCM_OP_AUTHENTICATED_ENCRYPTION;
		else
			sess->op = AESNI_GCM_OP_AUTHENTICATED_DECRYPTION;

		key_length = aead_xform->aead.key.length;
		key = aead_xform->aead.key.data;

		sess->aad_length = aead_xform->aead.aad_length;
		sess->req_digest_length = aead_xform->aead.digest_length;
	} else {
		AESNI_GCM_LOG(ERR, "Wrong xform type, has to be AEAD or authentication");
		return -ENOTSUP;
	}


	/* IV check */
	if (sess->iv.length != 16 && sess->iv.length != 12 &&
			sess->iv.length != 0) {
		AESNI_GCM_LOG(ERR, "Wrong IV length");
		return -EINVAL;
	}

	/* Check key length and calculate GCM pre-compute. */
	switch (key_length) {
	case 16:
		sess->key = AESNI_GCM_KEY_128;
		break;
	case 24:
		sess->key = AESNI_GCM_KEY_192;
		break;
	case 32:
		sess->key = AESNI_GCM_KEY_256;
		break;
	default:
		AESNI_GCM_LOG(ERR, "Invalid key length");
		return -EINVAL;
	}

	gcm_ops[sess->key].precomp(key, &sess->gdata_key);

	/* Digest check */
	if (sess->req_digest_length > 16) {
		AESNI_GCM_LOG(ERR, "Invalid digest length");
		return -EINVAL;
	}
	/*
	 * Multi-buffer lib supports digest sizes from 4 to 16 bytes
	 * in version 0.50 and sizes of 8, 12 and 16 bytes,
	 * in version 0.49.
	 * If size requested is different, generate the full digest
	 * (16 bytes) in a temporary location and then memcpy
	 * the requested number of bytes.
	 */
#if IMB_VERSION_NUM >= IMB_VERSION(0, 50, 0)
	if (sess->req_digest_length < 4)
#else
	if (sess->req_digest_length != 16 &&
			sess->req_digest_length != 12 &&
			sess->req_digest_length != 8)
#endif
		sess->gen_digest_length = 16;
	else
		sess->gen_digest_length = sess->req_digest_length;

	return 0;
}

/** Get gcm session */
static struct aesni_gcm_session *
aesni_gcm_get_session(struct aesni_gcm_qp *qp, struct rte_crypto_op *op)
{
	struct aesni_gcm_session *sess = NULL;
	struct rte_crypto_sym_op *sym_op = op->sym;

	if (op->sess_type == RTE_CRYPTO_OP_WITH_SESSION) {
		if (likely(sym_op->session != NULL))
			sess = (struct aesni_gcm_session *)
					get_sym_session_private_data(
					sym_op->session,
					cryptodev_driver_id);
	} else  {
		void *_sess;
		void *_sess_private_data = NULL;

		if (rte_mempool_get(qp->sess_mp, (void **)&_sess))
			return NULL;

		if (rte_mempool_get(qp->sess_mp, (void **)&_sess_private_data))
			return NULL;

		sess = (struct aesni_gcm_session *)_sess_private_data;

		if (unlikely(aesni_gcm_set_session_parameters(qp->ops,
				sess, sym_op->xform) != 0)) {
			rte_mempool_put(qp->sess_mp, _sess);
			rte_mempool_put(qp->sess_mp, _sess_private_data);
			sess = NULL;
		}
		sym_op->session = (struct rte_cryptodev_sym_session *)_sess;
		set_sym_session_private_data(sym_op->session,
				cryptodev_driver_id, _sess_private_data);
	}

	if (unlikely(sess == NULL))
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_SESSION;

	return sess;
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
 *
 */
static int
process_gcm_crypto_op(struct aesni_gcm_qp *qp, struct rte_crypto_op *op,
		struct aesni_gcm_session *session)
{
	uint8_t *src, *dst;
	uint8_t *iv_ptr;
	struct rte_crypto_sym_op *sym_op = op->sym;
	struct rte_mbuf *m_src = sym_op->m_src;
	uint32_t offset, data_offset, data_length;
	uint32_t part_len, total_len, data_len;
	uint8_t *tag;

	if (session->op == AESNI_GCM_OP_AUTHENTICATED_ENCRYPTION ||
			session->op == AESNI_GCM_OP_AUTHENTICATED_DECRYPTION) {
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

	data_len = m_src->data_len - offset;
	part_len = (data_len < data_length) ? data_len :
			data_length;

	/* Destination buffer is required when segmented source buffer */
	RTE_ASSERT((part_len == data_length) ||
			((part_len != data_length) &&
					(sym_op->m_dst != NULL)));
	/* Segmented destination buffer is not supported */
	RTE_ASSERT((sym_op->m_dst == NULL) ||
			((sym_op->m_dst != NULL) &&
					rte_pktmbuf_is_contiguous(sym_op->m_dst)));


	dst = sym_op->m_dst ?
			rte_pktmbuf_mtod_offset(sym_op->m_dst, uint8_t *,
					data_offset) :
			rte_pktmbuf_mtod_offset(sym_op->m_src, uint8_t *,
					data_offset);

	src = rte_pktmbuf_mtod_offset(m_src, uint8_t *, offset);

	iv_ptr = rte_crypto_op_ctod_offset(op, uint8_t *,
				session->iv.offset);

	if (session->op == AESNI_GCM_OP_AUTHENTICATED_ENCRYPTION) {
		qp->ops[session->key].init(&session->gdata_key,
				&qp->gdata_ctx,
				iv_ptr,
				sym_op->aead.aad.data,
				(uint64_t)session->aad_length);

		qp->ops[session->key].update_enc(&session->gdata_key,
				&qp->gdata_ctx, dst, src,
				(uint64_t)part_len);
		total_len = data_length - part_len;

		while (total_len) {
			dst += part_len;
			m_src = m_src->next;

			RTE_ASSERT(m_src != NULL);

			src = rte_pktmbuf_mtod(m_src, uint8_t *);
			part_len = (m_src->data_len < total_len) ?
					m_src->data_len : total_len;

			qp->ops[session->key].update_enc(&session->gdata_key,
					&qp->gdata_ctx, dst, src,
					(uint64_t)part_len);
			total_len -= part_len;
		}

		if (session->req_digest_length != session->gen_digest_length)
			tag = qp->temp_digest;
		else
			tag = sym_op->aead.digest.data;

		qp->ops[session->key].finalize(&session->gdata_key,
				&qp->gdata_ctx,
				tag,
				session->gen_digest_length);
	} else if (session->op == AESNI_GCM_OP_AUTHENTICATED_DECRYPTION) {
		qp->ops[session->key].init(&session->gdata_key,
				&qp->gdata_ctx,
				iv_ptr,
				sym_op->aead.aad.data,
				(uint64_t)session->aad_length);

		qp->ops[session->key].update_dec(&session->gdata_key,
				&qp->gdata_ctx, dst, src,
				(uint64_t)part_len);
		total_len = data_length - part_len;

		while (total_len) {
			dst += part_len;
			m_src = m_src->next;

			RTE_ASSERT(m_src != NULL);

			src = rte_pktmbuf_mtod(m_src, uint8_t *);
			part_len = (m_src->data_len < total_len) ?
					m_src->data_len : total_len;

			qp->ops[session->key].update_dec(&session->gdata_key,
					&qp->gdata_ctx,
					dst, src,
					(uint64_t)part_len);
			total_len -= part_len;
		}

		tag = qp->temp_digest;
		qp->ops[session->key].finalize(&session->gdata_key,
				&qp->gdata_ctx,
				tag,
				session->gen_digest_length);
	} else if (session->op == AESNI_GMAC_OP_GENERATE) {
		qp->ops[session->key].init(&session->gdata_key,
				&qp->gdata_ctx,
				iv_ptr,
				src,
				(uint64_t)data_length);
		if (session->req_digest_length != session->gen_digest_length)
			tag = qp->temp_digest;
		else
			tag = sym_op->auth.digest.data;
		qp->ops[session->key].finalize(&session->gdata_key,
				&qp->gdata_ctx,
				tag,
				session->gen_digest_length);
	} else { /* AESNI_GMAC_OP_VERIFY */
		qp->ops[session->key].init(&session->gdata_key,
				&qp->gdata_ctx,
				iv_ptr,
				src,
				(uint64_t)data_length);

		/*
		 * Generate always 16 bytes and later compare only
		 * the bytes passed.
		 */
		tag = qp->temp_digest;
		qp->ops[session->key].finalize(&session->gdata_key,
				&qp->gdata_ctx,
				tag,
				session->gen_digest_length);
	}

	return 0;
}

/**
 * Process a completed job and return rte_mbuf which job processed
 *
 * @param job	JOB_AES_HMAC job to process
 *
 * @return
 * - Returns processed mbuf which is trimmed of output digest used in
 * verification of supplied digest in the case of a HASH_CIPHER operation
 * - Returns NULL on invalid job
 */
static void
post_process_gcm_crypto_op(struct aesni_gcm_qp *qp,
		struct rte_crypto_op *op,
		struct aesni_gcm_session *session)
{
	op->status = RTE_CRYPTO_OP_STATUS_SUCCESS;

	/* Verify digest if required */
	if (session->op == AESNI_GCM_OP_AUTHENTICATED_DECRYPTION ||
			session->op == AESNI_GMAC_OP_VERIFY) {
		uint8_t *digest;

		uint8_t *tag = qp->temp_digest;

		if (session->op == AESNI_GMAC_OP_VERIFY)
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
			if (session->op == AESNI_GCM_OP_AUTHENTICATED_ENCRYPTION)
				memcpy(op->sym->aead.digest.data, qp->temp_digest,
						session->req_digest_length);
			else
				memcpy(op->sym->auth.digest.data, qp->temp_digest,
						session->req_digest_length);
		}
	}
}

/**
 * Process a completed GCM request
 *
 * @param qp		Queue Pair to process
 * @param op		Crypto operation
 * @param job		JOB_AES_HMAC job
 *
 * @return
 * - Number of processed jobs
 */
static void
handle_completed_gcm_crypto_op(struct aesni_gcm_qp *qp,
		struct rte_crypto_op *op,
		struct aesni_gcm_session *sess)
{
	post_process_gcm_crypto_op(qp, op, sess);

	/* Free session if a session-less crypto op */
	if (op->sess_type == RTE_CRYPTO_OP_SESSIONLESS) {
		memset(sess, 0, sizeof(struct aesni_gcm_session));
		memset(op->sym->session, 0,
				rte_cryptodev_sym_get_header_session_size());
		rte_mempool_put(qp->sess_mp, sess);
		rte_mempool_put(qp->sess_mp, op->sym->session);
		op->sym->session = NULL;
	}
}

static uint16_t
aesni_gcm_pmd_dequeue_burst(void *queue_pair,
		struct rte_crypto_op **ops, uint16_t nb_ops)
{
	struct aesni_gcm_session *sess;
	struct aesni_gcm_qp *qp = queue_pair;

	int retval = 0;
	unsigned int i, nb_dequeued;

	nb_dequeued = rte_ring_dequeue_burst(qp->processed_pkts,
			(void **)ops, nb_ops, NULL);

	for (i = 0; i < nb_dequeued; i++) {

		sess = aesni_gcm_get_session(qp, ops[i]);
		if (unlikely(sess == NULL)) {
			ops[i]->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
			qp->qp_stats.dequeue_err_count++;
			break;
		}

		retval = process_gcm_crypto_op(qp, ops[i], sess);
		if (retval < 0) {
			ops[i]->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
			qp->qp_stats.dequeue_err_count++;
			break;
		}

		handle_completed_gcm_crypto_op(qp, ops[i], sess);
	}

	qp->qp_stats.dequeued_count += i;

	return i;
}

static uint16_t
aesni_gcm_pmd_enqueue_burst(void *queue_pair,
		struct rte_crypto_op **ops, uint16_t nb_ops)
{
	struct aesni_gcm_qp *qp = queue_pair;

	unsigned int nb_enqueued;

	nb_enqueued = rte_ring_enqueue_burst(qp->processed_pkts,
			(void **)ops, nb_ops, NULL);
	qp->qp_stats.enqueued_count += nb_enqueued;

	return nb_enqueued;
}

static int aesni_gcm_remove(struct rte_vdev_device *vdev);

static int
aesni_gcm_create(const char *name,
		struct rte_vdev_device *vdev,
		struct rte_cryptodev_pmd_init_params *init_params)
{
	struct rte_cryptodev *dev;
	struct aesni_gcm_private *internals;
	enum aesni_gcm_vector_mode vector_mode;

	/* Check CPU for support for AES instruction set */
	if (!rte_cpu_get_flag_enabled(RTE_CPUFLAG_AES)) {
		AESNI_GCM_LOG(ERR, "AES instructions not supported by CPU");
		return -EFAULT;
	}
	dev = rte_cryptodev_pmd_create(name, &vdev->device, init_params);
	if (dev == NULL) {
		AESNI_GCM_LOG(ERR, "driver %s: create failed",
			init_params->name);
		return -ENODEV;
	}

	/* Check CPU for supported vector instruction set */
	if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX2))
		vector_mode = RTE_AESNI_GCM_AVX2;
	else if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX))
		vector_mode = RTE_AESNI_GCM_AVX;
	else
		vector_mode = RTE_AESNI_GCM_SSE;

	dev->driver_id = cryptodev_driver_id;
	dev->dev_ops = rte_aesni_gcm_pmd_ops;

	/* register rx/tx burst functions for data path */
	dev->dequeue_burst = aesni_gcm_pmd_dequeue_burst;
	dev->enqueue_burst = aesni_gcm_pmd_enqueue_burst;

	dev->feature_flags = RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO |
			RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING |
			RTE_CRYPTODEV_FF_CPU_AESNI |
			RTE_CRYPTODEV_FF_OOP_SGL_IN_LB_OUT |
			RTE_CRYPTODEV_FF_OOP_LB_IN_LB_OUT;

	switch (vector_mode) {
	case RTE_AESNI_GCM_SSE:
		dev->feature_flags |= RTE_CRYPTODEV_FF_CPU_SSE;
		break;
	case RTE_AESNI_GCM_AVX:
		dev->feature_flags |= RTE_CRYPTODEV_FF_CPU_AVX;
		break;
	case RTE_AESNI_GCM_AVX2:
		dev->feature_flags |= RTE_CRYPTODEV_FF_CPU_AVX2;
		break;
	default:
		break;
	}

	internals = dev->data->dev_private;

	internals->vector_mode = vector_mode;

	internals->max_nb_queue_pairs = init_params->max_nb_queue_pairs;

#if IMB_VERSION_NUM >= IMB_VERSION(0, 50, 0)
	AESNI_GCM_LOG(INFO, "IPSec Multi-buffer library version used: %s\n",
			imb_get_version_str());
#else
	AESNI_GCM_LOG(INFO, "IPSec Multi-buffer library version used: 0.49.0\n");
#endif

	return 0;
}

static int
aesni_gcm_probe(struct rte_vdev_device *vdev)
{
	struct rte_cryptodev_pmd_init_params init_params = {
		"",
		sizeof(struct aesni_gcm_private),
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

	return aesni_gcm_create(name, vdev, &init_params);
}

static int
aesni_gcm_remove(struct rte_vdev_device *vdev)
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

static struct rte_vdev_driver aesni_gcm_pmd_drv = {
	.probe = aesni_gcm_probe,
	.remove = aesni_gcm_remove
};

static struct cryptodev_driver aesni_gcm_crypto_drv;

RTE_PMD_REGISTER_VDEV(CRYPTODEV_NAME_AESNI_GCM_PMD, aesni_gcm_pmd_drv);
RTE_PMD_REGISTER_ALIAS(CRYPTODEV_NAME_AESNI_GCM_PMD, cryptodev_aesni_gcm_pmd);
RTE_PMD_REGISTER_PARAM_STRING(CRYPTODEV_NAME_AESNI_GCM_PMD,
	"max_nb_queue_pairs=<int> "
	"socket_id=<int>");
RTE_PMD_REGISTER_CRYPTO_DRIVER(aesni_gcm_crypto_drv, aesni_gcm_pmd_drv.driver,
		cryptodev_driver_id);


RTE_INIT(aesni_gcm_init_log)
{
	aesni_gcm_logtype_driver = rte_log_register("pmd.crypto.aesni_gcm");
}
