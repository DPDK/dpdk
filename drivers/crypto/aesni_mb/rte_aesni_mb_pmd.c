/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2015-2016 Intel Corporation. All rights reserved.
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
#include <rte_hexdump.h>
#include <rte_cryptodev.h>
#include <rte_cryptodev_pmd.h>
#include <rte_vdev.h>
#include <rte_malloc.h>
#include <rte_cpuflags.h>

#include "rte_aesni_mb_pmd_private.h"

typedef void (*hash_one_block_t)(const void *data, void *digest);
typedef void (*aes_keyexp_t)(const void *key, void *enc_exp_keys, void *dec_exp_keys);

/**
 * Calculate the authentication pre-computes
 *
 * @param one_block_hash	Function pointer to calculate digest on ipad/opad
 * @param ipad			Inner pad output byte array
 * @param opad			Outer pad output byte array
 * @param hkey			Authentication key
 * @param hkey_len		Authentication key length
 * @param blocksize		Block size of selected hash algo
 */
static void
calculate_auth_precomputes(hash_one_block_t one_block_hash,
		uint8_t *ipad, uint8_t *opad,
		uint8_t *hkey, uint16_t hkey_len,
		uint16_t blocksize)
{
	unsigned i, length;

	uint8_t ipad_buf[blocksize] __rte_aligned(16);
	uint8_t opad_buf[blocksize] __rte_aligned(16);

	/* Setup inner and outer pads */
	memset(ipad_buf, HMAC_IPAD_VALUE, blocksize);
	memset(opad_buf, HMAC_OPAD_VALUE, blocksize);

	/* XOR hash key with inner and outer pads */
	length = hkey_len > blocksize ? blocksize : hkey_len;

	for (i = 0; i < length; i++) {
		ipad_buf[i] ^= hkey[i];
		opad_buf[i] ^= hkey[i];
	}

	/* Compute partial hashes */
	(*one_block_hash)(ipad_buf, ipad);
	(*one_block_hash)(opad_buf, opad);

	/* Clean up stack */
	memset(ipad_buf, 0, blocksize);
	memset(opad_buf, 0, blocksize);
}

/** Get xform chain order */
static enum aesni_mb_operation
aesni_mb_get_chain_order(const struct rte_crypto_sym_xform *xform)
{
	if (xform == NULL)
		return AESNI_MB_OP_NOT_SUPPORTED;

	if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER) {
		if (xform->next == NULL)
			return AESNI_MB_OP_CIPHER_ONLY;
		if (xform->next->type == RTE_CRYPTO_SYM_XFORM_AUTH)
			return AESNI_MB_OP_CIPHER_HASH;
	}

	if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH) {
		if (xform->next == NULL)
			return AESNI_MB_OP_HASH_ONLY;
		if (xform->next->type == RTE_CRYPTO_SYM_XFORM_CIPHER)
			return AESNI_MB_OP_HASH_CIPHER;
	}

	return AESNI_MB_OP_NOT_SUPPORTED;
}

/** Set session authentication parameters */
static int
aesni_mb_set_session_auth_parameters(const struct aesni_mb_op_fns *mb_ops,
		struct aesni_mb_session *sess,
		const struct rte_crypto_sym_xform *xform)
{
	hash_one_block_t hash_oneblock_fn;

	if (xform == NULL) {
		sess->auth.algo = NULL_HASH;
		return 0;
	}

	if (xform->type != RTE_CRYPTO_SYM_XFORM_AUTH) {
		MB_LOG_ERR("Crypto xform struct not of type auth");
		return -1;
	}

	/* Select auth generate/verify */
	sess->auth.operation = xform->auth.op;

	/* Set Authentication Parameters */
	if (xform->auth.algo == RTE_CRYPTO_AUTH_AES_XCBC_MAC) {
		sess->auth.algo = AES_XCBC;
		(*mb_ops->aux.keyexp.aes_xcbc)(xform->auth.key.data,
				sess->auth.xcbc.k1_expanded,
				sess->auth.xcbc.k2, sess->auth.xcbc.k3);
		return 0;
	}

	switch (xform->auth.algo) {
	case RTE_CRYPTO_AUTH_MD5_HMAC:
		sess->auth.algo = MD5;
		hash_oneblock_fn = mb_ops->aux.one_block.md5;
		break;
	case RTE_CRYPTO_AUTH_SHA1_HMAC:
		sess->auth.algo = SHA1;
		hash_oneblock_fn = mb_ops->aux.one_block.sha1;
		break;
	case RTE_CRYPTO_AUTH_SHA224_HMAC:
		sess->auth.algo = SHA_224;
		hash_oneblock_fn = mb_ops->aux.one_block.sha224;
		break;
	case RTE_CRYPTO_AUTH_SHA256_HMAC:
		sess->auth.algo = SHA_256;
		hash_oneblock_fn = mb_ops->aux.one_block.sha256;
		break;
	case RTE_CRYPTO_AUTH_SHA384_HMAC:
		sess->auth.algo = SHA_384;
		hash_oneblock_fn = mb_ops->aux.one_block.sha384;
		break;
	case RTE_CRYPTO_AUTH_SHA512_HMAC:
		sess->auth.algo = SHA_512;
		hash_oneblock_fn = mb_ops->aux.one_block.sha512;
		break;
	default:
		MB_LOG_ERR("Unsupported authentication algorithm selection");
		return -1;
	}

	/* Calculate Authentication precomputes */
	calculate_auth_precomputes(hash_oneblock_fn,
			sess->auth.pads.inner, sess->auth.pads.outer,
			xform->auth.key.data,
			xform->auth.key.length,
			get_auth_algo_blocksize(sess->auth.algo));

	return 0;
}

/** Set session cipher parameters */
static int
aesni_mb_set_session_cipher_parameters(const struct aesni_mb_op_fns *mb_ops,
		struct aesni_mb_session *sess,
		const struct rte_crypto_sym_xform *xform)
{
	aes_keyexp_t aes_keyexp_fn;

	if (xform == NULL) {
		sess->cipher.mode = NULL_CIPHER;
		return 0;
	}

	if (xform->type != RTE_CRYPTO_SYM_XFORM_CIPHER) {
		MB_LOG_ERR("Crypto xform struct not of type cipher");
		return -1;
	}

	/* Select cipher direction */
	switch (xform->cipher.op) {
	case RTE_CRYPTO_CIPHER_OP_ENCRYPT:
		sess->cipher.direction = ENCRYPT;
		break;
	case RTE_CRYPTO_CIPHER_OP_DECRYPT:
		sess->cipher.direction = DECRYPT;
		break;
	default:
		MB_LOG_ERR("Unsupported cipher operation parameter");
		return -1;
	}

	/* Select cipher mode */
	switch (xform->cipher.algo) {
	case RTE_CRYPTO_CIPHER_AES_CBC:
		sess->cipher.mode = CBC;
		break;
	case RTE_CRYPTO_CIPHER_AES_CTR:
		sess->cipher.mode = CNTR;
		break;
	case RTE_CRYPTO_CIPHER_AES_DOCSISBPI:
		sess->cipher.mode = DOCSIS_SEC_BPI;
		break;
	default:
		MB_LOG_ERR("Unsupported cipher mode parameter");
		return -1;
	}

	/* Check key length and choose key expansion function */
	switch (xform->cipher.key.length) {
	case AES_128_BYTES:
		sess->cipher.key_length_in_bytes = AES_128_BYTES;
		aes_keyexp_fn = mb_ops->aux.keyexp.aes128;
		break;
	case AES_192_BYTES:
		sess->cipher.key_length_in_bytes = AES_192_BYTES;
		aes_keyexp_fn = mb_ops->aux.keyexp.aes192;
		break;
	case AES_256_BYTES:
		sess->cipher.key_length_in_bytes = AES_256_BYTES;
		aes_keyexp_fn = mb_ops->aux.keyexp.aes256;
		break;
	default:
		MB_LOG_ERR("Unsupported cipher key length");
		return -1;
	}

	/* Expanded cipher keys */
	(*aes_keyexp_fn)(xform->cipher.key.data,
			sess->cipher.expanded_aes_keys.encode,
			sess->cipher.expanded_aes_keys.decode);

	return 0;
}

/** Parse crypto xform chain and set private session parameters */
int
aesni_mb_set_session_parameters(const struct aesni_mb_op_fns *mb_ops,
		struct aesni_mb_session *sess,
		const struct rte_crypto_sym_xform *xform)
{
	const struct rte_crypto_sym_xform *auth_xform = NULL;
	const struct rte_crypto_sym_xform *cipher_xform = NULL;

	/* Select Crypto operation - hash then cipher / cipher then hash */
	switch (aesni_mb_get_chain_order(xform)) {
	case AESNI_MB_OP_HASH_CIPHER:
		sess->chain_order = HASH_CIPHER;
		auth_xform = xform;
		cipher_xform = xform->next;
		break;
	case AESNI_MB_OP_CIPHER_HASH:
		sess->chain_order = CIPHER_HASH;
		auth_xform = xform->next;
		cipher_xform = xform;
		break;
	case AESNI_MB_OP_HASH_ONLY:
		sess->chain_order = HASH_CIPHER;
		auth_xform = xform;
		cipher_xform = NULL;
		break;
	case AESNI_MB_OP_CIPHER_ONLY:
		/*
		 * Multi buffer library operates only at two modes,
		 * CIPHER_HASH and HASH_CIPHER. When doing ciphering only,
		 * chain order depends on cipher operation: encryption is always
		 * the first operation and decryption the last one.
		 */
		if (xform->cipher.op == RTE_CRYPTO_CIPHER_OP_ENCRYPT)
			sess->chain_order = CIPHER_HASH;
		else
			sess->chain_order = HASH_CIPHER;
		auth_xform = NULL;
		cipher_xform = xform;
		break;
	case AESNI_MB_OP_NOT_SUPPORTED:
	default:
		MB_LOG_ERR("Unsupported operation chain order parameter");
		return -1;
	}

	if (aesni_mb_set_session_auth_parameters(mb_ops, sess, auth_xform)) {
		MB_LOG_ERR("Invalid/unsupported authentication parameters");
		return -1;
	}

	if (aesni_mb_set_session_cipher_parameters(mb_ops, sess,
			cipher_xform)) {
		MB_LOG_ERR("Invalid/unsupported cipher parameters");
		return -1;
	}
	return 0;
}

/**
 * burst enqueue, place crypto operations on ingress queue for processing.
 *
 * @param __qp         Queue Pair to process
 * @param ops          Crypto operations for processing
 * @param nb_ops       Number of crypto operations for processing
 *
 * @return
 * - Number of crypto operations enqueued
 */
static uint16_t
aesni_mb_pmd_enqueue_burst(void *__qp, struct rte_crypto_op **ops,
		uint16_t nb_ops)
{
	struct aesni_mb_qp *qp = __qp;

	unsigned int nb_enqueued;

	nb_enqueued = rte_ring_enqueue_burst(qp->ingress_queue,
			(void **)ops, nb_ops, NULL);

	qp->stats.enqueued_count += nb_enqueued;

	return nb_enqueued;
}

/** Get multi buffer session */
static inline struct aesni_mb_session *
get_session(struct aesni_mb_qp *qp, struct rte_crypto_op *op)
{
	struct aesni_mb_session *sess = NULL;

	if (op->sym->sess_type == RTE_CRYPTO_SYM_OP_WITH_SESSION) {
		if (unlikely(op->sym->session->dev_type !=
				RTE_CRYPTODEV_AESNI_MB_PMD)) {
			return NULL;
		}

		sess = (struct aesni_mb_session *)op->sym->session->_private;
	} else  {
		void *_sess = NULL;

		if (rte_mempool_get(qp->sess_mp, (void **)&_sess))
			return NULL;

		sess = (struct aesni_mb_session *)
			((struct rte_cryptodev_sym_session *)_sess)->_private;

		if (unlikely(aesni_mb_set_session_parameters(qp->op_fns,
				sess, op->sym->xform) != 0)) {
			rte_mempool_put(qp->sess_mp, _sess);
			sess = NULL;
		}
		op->sym->session = (struct rte_cryptodev_sym_session *)_sess;
	}

	return sess;
}

/**
 * Process a crypto operation and complete a JOB_AES_HMAC job structure for
 * submission to the multi buffer library for processing.
 *
 * @param	qp	queue pair
 * @param	job	JOB_AES_HMAC structure to fill
 * @param	m	mbuf to process
 *
 * @return
 * - Completed JOB_AES_HMAC structure pointer on success
 * - NULL pointer if completion of JOB_AES_HMAC structure isn't possible
 */
static inline int
set_mb_job_params(JOB_AES_HMAC *job, struct aesni_mb_qp *qp,
		struct rte_crypto_op *op)
{
	struct rte_mbuf *m_src = op->sym->m_src, *m_dst;
	struct aesni_mb_session *session;
	uint16_t m_offset = 0;

	session = get_session(qp, op);
	if (session == NULL) {
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_SESSION;
		return -1;
	}
	op->status = RTE_CRYPTO_OP_STATUS_ENQUEUED;

	/* Set crypto operation */
	job->chain_order = session->chain_order;

	/* Set cipher parameters */
	job->cipher_direction = session->cipher.direction;
	job->cipher_mode = session->cipher.mode;

	job->aes_key_len_in_bytes = session->cipher.key_length_in_bytes;
	job->aes_enc_key_expanded = session->cipher.expanded_aes_keys.encode;
	job->aes_dec_key_expanded = session->cipher.expanded_aes_keys.decode;


	/* Set authentication parameters */
	job->hash_alg = session->auth.algo;
	if (job->hash_alg == AES_XCBC) {
		job->_k1_expanded = session->auth.xcbc.k1_expanded;
		job->_k2 = session->auth.xcbc.k2;
		job->_k3 = session->auth.xcbc.k3;
	} else {
		job->hashed_auth_key_xor_ipad = session->auth.pads.inner;
		job->hashed_auth_key_xor_opad = session->auth.pads.outer;
	}

	/* Mutable crypto operation parameters */
	if (op->sym->m_dst) {
		m_src = m_dst = op->sym->m_dst;

		/* append space for output data to mbuf */
		char *odata = rte_pktmbuf_append(m_dst,
				rte_pktmbuf_data_len(op->sym->m_src));
		if (odata == NULL) {
			MB_LOG_ERR("failed to allocate space in destination "
					"mbuf for source data");
			op->status = RTE_CRYPTO_OP_STATUS_ERROR;
			return -1;
		}

		memcpy(odata, rte_pktmbuf_mtod(op->sym->m_src, void*),
				rte_pktmbuf_data_len(op->sym->m_src));
	} else {
		m_dst = m_src;
		m_offset = op->sym->cipher.data.offset;
	}

	/* Set digest output location */
	if (job->hash_alg != NULL_HASH &&
			session->auth.operation == RTE_CRYPTO_AUTH_OP_VERIFY) {
		job->auth_tag_output = (uint8_t *)rte_pktmbuf_append(m_dst,
				get_digest_byte_length(job->hash_alg));

		if (job->auth_tag_output == NULL) {
			MB_LOG_ERR("failed to allocate space in output mbuf "
					"for temp digest");
			op->status = RTE_CRYPTO_OP_STATUS_ERROR;
			return -1;
		}

		memset(job->auth_tag_output, 0,
				sizeof(get_digest_byte_length(job->hash_alg)));

	} else {
		job->auth_tag_output = op->sym->auth.digest.data;
	}

	/*
	 * Multi-buffer library current only support returning a truncated
	 * digest length as specified in the relevant IPsec RFCs
	 */
	job->auth_tag_output_len_in_bytes =
			get_truncated_digest_byte_length(job->hash_alg);

	/* Set IV parameters */
	job->iv = op->sym->cipher.iv.data;
	job->iv_len_in_bytes = op->sym->cipher.iv.length;

	/* Data  Parameter */
	job->src = rte_pktmbuf_mtod(m_src, uint8_t *);
	job->dst = rte_pktmbuf_mtod_offset(m_dst, uint8_t *, m_offset);

	job->cipher_start_src_offset_in_bytes = op->sym->cipher.data.offset;
	job->msg_len_to_cipher_in_bytes = op->sym->cipher.data.length;

	job->hash_start_src_offset_in_bytes = op->sym->auth.data.offset;
	job->msg_len_to_hash_in_bytes = op->sym->auth.data.length;

	/* Set user data to be crypto operation data struct */
	job->user_data = op;
	job->user_data2 = m_dst;

	return 0;
}

static inline void
verify_digest(JOB_AES_HMAC *job, struct rte_crypto_op *op) {
	struct rte_mbuf *m_dst = (struct rte_mbuf *)job->user_data2;

	RTE_ASSERT(m_dst == NULL);

	/* Verify digest if required */
	if (memcmp(job->auth_tag_output, op->sym->auth.digest.data,
			job->auth_tag_output_len_in_bytes) != 0)
		op->status = RTE_CRYPTO_OP_STATUS_AUTH_FAILED;

	/* trim area used for digest from mbuf */
	rte_pktmbuf_trim(m_dst,	get_digest_byte_length(job->hash_alg));
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
static inline struct rte_crypto_op *
post_process_mb_job(struct aesni_mb_qp *qp, JOB_AES_HMAC *job)
{
	struct rte_crypto_op *op = (struct rte_crypto_op *)job->user_data;

	struct aesni_mb_session *sess;

	RTE_ASSERT(op == NULL);

	if (unlikely(op->status == RTE_CRYPTO_OP_STATUS_ENQUEUED)) {
		switch (job->status) {
		case STS_COMPLETED:
			op->status = RTE_CRYPTO_OP_STATUS_SUCCESS;

			if (job->hash_alg != NULL_HASH) {
				sess = (struct aesni_mb_session *)
						op->sym->session->_private;

				if (sess->auth.operation ==
						RTE_CRYPTO_AUTH_OP_VERIFY)
					verify_digest(job, op);
			}
			break;
		default:
			op->status = RTE_CRYPTO_OP_STATUS_ERROR;
		}
	}

	/* Free session if a session-less crypto op */
	if (op->sym->sess_type == RTE_CRYPTO_SYM_OP_SESSIONLESS) {
		rte_mempool_put(qp->sess_mp, op->sym->session);
		op->sym->session = NULL;
	}

	return op;
}

/**
 * Process a completed JOB_AES_HMAC job and keep processing jobs until
 * get_completed_job return NULL
 *
 * @param qp		Queue Pair to process
 * @param job		JOB_AES_HMAC job
 *
 * @return
 * - Number of processed jobs
 */
static unsigned
handle_completed_jobs(struct aesni_mb_qp *qp, JOB_AES_HMAC *job,
		struct rte_crypto_op **ops, uint16_t nb_ops)
{
	struct rte_crypto_op *op = NULL;
	unsigned processed_jobs = 0;

	while (job != NULL && processed_jobs < nb_ops) {
		op = post_process_mb_job(qp, job);

		if (op) {
			ops[processed_jobs++] = op;
			qp->stats.dequeued_count++;
		} else {
			qp->stats.dequeue_err_count++;
			break;
		}

		job = (*qp->op_fns->job.get_completed_job)(&qp->mb_mgr);
	}

	return processed_jobs;
}

static inline uint16_t
flush_mb_mgr(struct aesni_mb_qp *qp, struct rte_crypto_op **ops,
		uint16_t nb_ops)
{
	int processed_ops = 0;

	/* Flush the remaining jobs */
	JOB_AES_HMAC *job = (*qp->op_fns->job.flush_job)(&qp->mb_mgr);

	if (job)
		processed_ops += handle_completed_jobs(qp, job,
				&ops[processed_ops], nb_ops - processed_ops);

	return processed_ops;
}

static inline JOB_AES_HMAC *
set_job_null_op(JOB_AES_HMAC *job)
{
	job->chain_order = HASH_CIPHER;
	job->cipher_mode = NULL_CIPHER;
	job->hash_alg = NULL_HASH;
	job->cipher_direction = DECRYPT;

	return job;
}

static uint16_t
aesni_mb_pmd_dequeue_burst(void *queue_pair, struct rte_crypto_op **ops,
		uint16_t nb_ops)
{
	struct aesni_mb_qp *qp = queue_pair;

	struct rte_crypto_op *op;
	JOB_AES_HMAC *job;

	int retval, processed_jobs = 0;

	do {
		/* Get next operation to process from ingress queue */
		retval = rte_ring_dequeue(qp->ingress_queue, (void **)&op);
		if (retval < 0)
			break;

		/* Get next free mb job struct from mb manager */
		job = (*qp->op_fns->job.get_next)(&qp->mb_mgr);
		if (unlikely(job == NULL)) {
			/* if no free mb job structs we need to flush mb_mgr */
			processed_jobs += flush_mb_mgr(qp,
					&ops[processed_jobs],
					(nb_ops - processed_jobs) - 1);

			job = (*qp->op_fns->job.get_next)(&qp->mb_mgr);
		}

		retval = set_mb_job_params(job, qp, op);
		if (unlikely(retval != 0)) {
			qp->stats.dequeue_err_count++;
			set_job_null_op(job);
		}

		/* Submit job to multi-buffer for processing */
		job = (*qp->op_fns->job.submit)(&qp->mb_mgr);

		/*
		 * If submit returns a processed job then handle it,
		 * before submitting subsequent jobs
		 */
		if (job)
			processed_jobs += handle_completed_jobs(qp, job,
					&ops[processed_jobs],
					nb_ops - processed_jobs);

	} while (processed_jobs < nb_ops);

	if (processed_jobs < 1)
		processed_jobs += flush_mb_mgr(qp,
				&ops[processed_jobs],
				nb_ops - processed_jobs);

	return processed_jobs;
}

static int cryptodev_aesni_mb_remove(struct rte_vdev_device *vdev);

static int
cryptodev_aesni_mb_create(const char *name,
			struct rte_vdev_device *vdev,
			struct rte_crypto_vdev_init_params *init_params)
{
	struct rte_cryptodev *dev;
	struct aesni_mb_private *internals;
	enum aesni_mb_vector_mode vector_mode;

	if (init_params->name[0] == '\0')
		snprintf(init_params->name, sizeof(init_params->name),
				"%s", name);

	/* Check CPU for supported vector instruction set */
	if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX512F))
		vector_mode = RTE_AESNI_MB_AVX512;
	else if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX2))
		vector_mode = RTE_AESNI_MB_AVX2;
	else if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX))
		vector_mode = RTE_AESNI_MB_AVX;
	else if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_SSE4_1))
		vector_mode = RTE_AESNI_MB_SSE;
	else {
		MB_LOG_ERR("Vector instructions are not supported by CPU");
		return -EFAULT;
	}

	dev = rte_cryptodev_pmd_virtual_dev_init(init_params->name,
			sizeof(struct aesni_mb_private), init_params->socket_id);
	if (dev == NULL) {
		MB_LOG_ERR("failed to create cryptodev vdev");
		goto init_error;
	}

	dev->dev_type = RTE_CRYPTODEV_AESNI_MB_PMD;
	dev->dev_ops = rte_aesni_mb_pmd_ops;

	/* register rx/tx burst functions for data path */
	dev->dequeue_burst = aesni_mb_pmd_dequeue_burst;
	dev->enqueue_burst = aesni_mb_pmd_enqueue_burst;

	dev->feature_flags = RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO |
			RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING |
			RTE_CRYPTODEV_FF_CPU_AESNI;

	switch (vector_mode) {
	case RTE_AESNI_MB_SSE:
		dev->feature_flags |= RTE_CRYPTODEV_FF_CPU_SSE;
		break;
	case RTE_AESNI_MB_AVX:
		dev->feature_flags |= RTE_CRYPTODEV_FF_CPU_AVX;
		break;
	case RTE_AESNI_MB_AVX2:
		dev->feature_flags |= RTE_CRYPTODEV_FF_CPU_AVX2;
		break;
	case RTE_AESNI_MB_AVX512:
		dev->feature_flags |= RTE_CRYPTODEV_FF_CPU_AVX512;
		break;
	default:
		break;
	}

	/* Set vector instructions mode supported */
	internals = dev->data->dev_private;

	internals->vector_mode = vector_mode;
	internals->max_nb_queue_pairs = init_params->max_nb_queue_pairs;
	internals->max_nb_sessions = init_params->max_nb_sessions;

	return 0;
init_error:
	MB_LOG_ERR("driver %s: cryptodev_aesni_create failed",
			init_params->name);

	cryptodev_aesni_mb_remove(vdev);
	return -EFAULT;
}

static int
cryptodev_aesni_mb_probe(struct rte_vdev_device *vdev)
{
	struct rte_crypto_vdev_init_params init_params = {
		RTE_CRYPTODEV_VDEV_DEFAULT_MAX_NB_QUEUE_PAIRS,
		RTE_CRYPTODEV_VDEV_DEFAULT_MAX_NB_SESSIONS,
		rte_socket_id(),
		""
	};
	const char *name;
	const char *input_args;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;
	input_args = rte_vdev_device_args(vdev);
	rte_cryptodev_parse_vdev_init_params(&init_params, input_args);

	RTE_LOG(INFO, PMD, "Initialising %s on NUMA node %d\n", name,
			init_params.socket_id);
	if (init_params.name[0] != '\0')
		RTE_LOG(INFO, PMD, "  User defined name = %s\n",
			init_params.name);
	RTE_LOG(INFO, PMD, "  Max number of queue pairs = %d\n",
			init_params.max_nb_queue_pairs);
	RTE_LOG(INFO, PMD, "  Max number of sessions = %d\n",
			init_params.max_nb_sessions);

	return cryptodev_aesni_mb_create(name, vdev, &init_params);
}

static int
cryptodev_aesni_mb_remove(struct rte_vdev_device *vdev)
{
	const char *name;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	RTE_LOG(INFO, PMD, "Closing AESNI crypto device %s on numa socket %u\n",
			name, rte_socket_id());

	return 0;
}

static struct rte_vdev_driver cryptodev_aesni_mb_pmd_drv = {
	.probe = cryptodev_aesni_mb_probe,
	.remove = cryptodev_aesni_mb_remove
};

RTE_PMD_REGISTER_VDEV(CRYPTODEV_NAME_AESNI_MB_PMD, cryptodev_aesni_mb_pmd_drv);
RTE_PMD_REGISTER_ALIAS(CRYPTODEV_NAME_AESNI_MB_PMD, cryptodev_aesni_mb_pmd);
RTE_PMD_REGISTER_PARAM_STRING(CRYPTODEV_NAME_AESNI_MB_PMD,
	"max_nb_queue_pairs=<int> "
	"max_nb_sessions=<int> "
	"socket_id=<int>");
