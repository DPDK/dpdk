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

#include <openssl/aes.h>

#include <rte_common.h>
#include <rte_config.h>
#include <rte_hexdump.h>
#include <rte_cryptodev.h>
#include <rte_cryptodev_pmd.h>
#include <rte_dev.h>
#include <rte_malloc.h>
#include <rte_cpuflags.h>

#include "aesni_gcm_pmd_private.h"

/**
 * Global static parameter used to create a unique name for each AES-NI multi
 * buffer crypto device.
 */
static unsigned unique_name_id;

static inline int
create_unique_device_name(char *name, size_t size)
{
	int ret;

	if (name == NULL)
		return -EINVAL;

	ret = snprintf(name, size, "%s_%u", RTE_STR(CRYPTODEV_NAME_AESNI_GCM_PMD),
			unique_name_id++);
	if (ret < 0)
		return ret;
	return 0;
}

static int
aesni_gcm_calculate_hash_sub_key(uint8_t *hsubkey, unsigned hsubkey_length,
		uint8_t *aeskey, unsigned aeskey_length)
{
	uint8_t key[aeskey_length] __rte_aligned(16);
	AES_KEY enc_key;

	if (hsubkey_length % 16 != 0 && aeskey_length % 16 != 0)
		return -EFAULT;

	memcpy(key, aeskey, aeskey_length);

	if (AES_set_encrypt_key(key, aeskey_length << 3, &enc_key) != 0)
		return -EFAULT;

	AES_encrypt(hsubkey, hsubkey, &enc_key);

	return 0;
}

/** Get xform chain order */
static int
aesni_gcm_get_mode(const struct rte_crypto_sym_xform *xform)
{
	/*
	 * GCM only supports authenticated encryption or authenticated
	 * decryption, all other options are invalid, so we must have exactly
	 * 2 xform structs chained together
	 */
	if (xform->next == NULL || xform->next->next != NULL)
		return -1;

	if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER &&
			xform->next->type == RTE_CRYPTO_SYM_XFORM_AUTH) {
		return AESNI_GCM_OP_AUTHENTICATED_ENCRYPTION;
	}

	if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH &&
			xform->next->type == RTE_CRYPTO_SYM_XFORM_CIPHER) {
		return AESNI_GCM_OP_AUTHENTICATED_DECRYPTION;
	}

	return -1;
}

/** Parse crypto xform chain and set private session parameters */
int
aesni_gcm_set_session_parameters(const struct aesni_gcm_ops *gcm_ops,
		struct aesni_gcm_session *sess,
		const struct rte_crypto_sym_xform *xform)
{
	const struct rte_crypto_sym_xform *auth_xform = NULL;
	const struct rte_crypto_sym_xform *cipher_xform = NULL;

	uint8_t hsubkey[16] __rte_aligned(16) = { 0 };

	/* Select Crypto operation - hash then cipher / cipher then hash */
	switch (aesni_gcm_get_mode(xform)) {
	case AESNI_GCM_OP_AUTHENTICATED_ENCRYPTION:
		sess->op = AESNI_GCM_OP_AUTHENTICATED_ENCRYPTION;

		cipher_xform = xform;
		auth_xform = xform->next;
		break;
	case AESNI_GCM_OP_AUTHENTICATED_DECRYPTION:
		sess->op = AESNI_GCM_OP_AUTHENTICATED_DECRYPTION;

		auth_xform = xform;
		cipher_xform = xform->next;
		break;
	default:
		GCM_LOG_ERR("Unsupported operation chain order parameter");
		return -EINVAL;
	}

	/* We only support AES GCM */
	if (cipher_xform->cipher.algo != RTE_CRYPTO_CIPHER_AES_GCM &&
			auth_xform->auth.algo != RTE_CRYPTO_AUTH_AES_GCM)
		return -EINVAL;

	/* Select cipher direction */
	if (sess->op == AESNI_GCM_OP_AUTHENTICATED_ENCRYPTION &&
			cipher_xform->cipher.op !=
					RTE_CRYPTO_CIPHER_OP_ENCRYPT) {
		GCM_LOG_ERR("xform chain (CIPHER/AUTH) and cipher operation "
				"(DECRYPT) specified are an invalid selection");
		return -EINVAL;
	} else if (sess->op == AESNI_GCM_OP_AUTHENTICATED_DECRYPTION &&
			cipher_xform->cipher.op !=
					RTE_CRYPTO_CIPHER_OP_DECRYPT) {
		GCM_LOG_ERR("xform chain (AUTH/CIPHER) and cipher operation "
				"(ENCRYPT) specified are an invalid selection");
		return -EINVAL;
	}

	/* Expand GCM AES128 key */
	(*gcm_ops->aux.keyexp.aes128_enc)(cipher_xform->cipher.key.data,
			sess->gdata.expanded_keys);

	/* Calculate hash sub key here */
	aesni_gcm_calculate_hash_sub_key(hsubkey, sizeof(hsubkey),
			cipher_xform->cipher.key.data,
			cipher_xform->cipher.key.length);

	/* Calculate GCM pre-compute */
	(*gcm_ops->gcm.precomp)(&sess->gdata, hsubkey);

	return 0;
}

/** Get gcm session */
static struct aesni_gcm_session *
aesni_gcm_get_session(struct aesni_gcm_qp *qp, struct rte_crypto_sym_op *op)
{
	struct aesni_gcm_session *sess = NULL;

	if (op->sess_type == RTE_CRYPTO_SYM_OP_WITH_SESSION) {
		if (unlikely(op->session->dev_type
					!= RTE_CRYPTODEV_AESNI_GCM_PMD))
			return sess;

		sess = (struct aesni_gcm_session *)op->session->_private;
	} else  {
		void *_sess;

		if (rte_mempool_get(qp->sess_mp, &_sess))
			return sess;

		sess = (struct aesni_gcm_session *)
			((struct rte_cryptodev_session *)_sess)->_private;

		if (unlikely(aesni_gcm_set_session_parameters(qp->ops,
				sess, op->xform) != 0)) {
			rte_mempool_put(qp->sess_mp, _sess);
			sess = NULL;
		}
	}
	return sess;
}

/**
 * Process a crypto operation and complete a JOB_AES_HMAC job structure for
 * submission to the multi buffer library for processing.
 *
 * @param	qp		queue pair
 * @param	op		symmetric crypto operation
 * @param	session		GCM session
 *
 * @return
 *
 */
static int
process_gcm_crypto_op(struct aesni_gcm_qp *qp, struct rte_crypto_sym_op *op,
		struct aesni_gcm_session *session)
{
	uint8_t *src, *dst;
	struct rte_mbuf *m = op->m_src;

	src = rte_pktmbuf_mtod(m, uint8_t *) + op->cipher.data.offset;
	dst = op->m_dst ?
			rte_pktmbuf_mtod_offset(op->m_dst, uint8_t *,
					op->cipher.data.offset) :
			rte_pktmbuf_mtod_offset(m, uint8_t *,
					op->cipher.data.offset);

	/* sanity checks */
	if (op->cipher.iv.length != 16 && op->cipher.iv.length != 0) {
		GCM_LOG_ERR("iv");
		return -1;
	}

	if (op->auth.aad.length != 12 && op->auth.aad.length != 8 &&
			op->auth.aad.length != 0) {
		GCM_LOG_ERR("iv");
		return -1;
	}

	if (op->auth.digest.length != 16 &&
			op->auth.digest.length != 12 &&
			op->auth.digest.length != 8 &&
			op->auth.digest.length != 0) {
		GCM_LOG_ERR("iv");
		return -1;
	}

	if (session->op == AESNI_GCM_OP_AUTHENTICATED_ENCRYPTION) {

		(*qp->ops->gcm.enc)(&session->gdata, dst, src,
				(uint64_t)op->cipher.data.length,
				op->cipher.iv.data,
				op->auth.aad.data,
				(uint64_t)op->auth.aad.length,
				op->auth.digest.data,
				(uint64_t)op->auth.digest.length);
	} else if (session->op == AESNI_GCM_OP_AUTHENTICATED_DECRYPTION) {
		uint8_t *auth_tag = (uint8_t *)rte_pktmbuf_append(m,
				op->auth.digest.length);

		if (!auth_tag) {
			GCM_LOG_ERR("iv");
			return -1;
		}

		(*qp->ops->gcm.dec)(&session->gdata, dst, src,
				(uint64_t)op->cipher.data.length,
				op->cipher.iv.data,
				op->auth.aad.data,
				(uint64_t)op->auth.aad.length,
				auth_tag,
				(uint64_t)op->auth.digest.length);
	} else {
		GCM_LOG_ERR("iv");
		return -1;
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
post_process_gcm_crypto_op(struct rte_crypto_op *op)
{
	struct rte_mbuf *m = op->sym->m_dst ? op->sym->m_dst : op->sym->m_src;

	struct aesni_gcm_session *session =
		(struct aesni_gcm_session *)op->sym->session->_private;

	op->status = RTE_CRYPTO_OP_STATUS_SUCCESS;

	/* Verify digest if required */
	if (session->op == AESNI_GCM_OP_AUTHENTICATED_DECRYPTION) {

		uint8_t *tag = rte_pktmbuf_mtod_offset(m, uint8_t *,
				m->data_len - op->sym->auth.digest.length);

#ifdef RTE_LIBRTE_PMD_AESNI_GCM_DEBUG
		rte_hexdump(stdout, "auth tag (orig):",
				op->sym->auth.digest.data, op->sym->auth.digest.length);
		rte_hexdump(stdout, "auth tag (calc):",
				tag, op->sym->auth.digest.length);
#endif

		if (memcmp(tag, op->sym->auth.digest.data,
				op->sym->auth.digest.length) != 0)
			op->status = RTE_CRYPTO_OP_STATUS_AUTH_FAILED;

		/* trim area used for digest from mbuf */
		rte_pktmbuf_trim(m, op->sym->auth.digest.length);
	}
}

/**
 * Process a completed GCM request
 *
 * @param qp		Queue Pair to process
 * @param job		JOB_AES_HMAC job
 *
 * @return
 * - Number of processed jobs
 */
static void
handle_completed_gcm_crypto_op(struct aesni_gcm_qp *qp,
		struct rte_crypto_op *op)
{
	post_process_gcm_crypto_op(op);

	/* Free session if a session-less crypto op */
	if (op->sym->sess_type == RTE_CRYPTO_SYM_OP_SESSIONLESS) {
		rte_mempool_put(qp->sess_mp, op->sym->session);
		op->sym->session = NULL;
	}

	rte_ring_enqueue(qp->processed_pkts, (void *)op);
}

static uint16_t
aesni_gcm_pmd_enqueue_burst(void *queue_pair,
		struct rte_crypto_op **ops, uint16_t nb_ops)
{
	struct aesni_gcm_session *sess;
	struct aesni_gcm_qp *qp = queue_pair;

	int i, retval = 0;

	for (i = 0; i < nb_ops; i++) {

		sess = aesni_gcm_get_session(qp, ops[i]->sym);
		if (unlikely(sess == NULL)) {
			ops[i]->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
			qp->qp_stats.enqueue_err_count++;
			break;
		}

		retval = process_gcm_crypto_op(qp, ops[i]->sym, sess);
		if (retval < 0) {
			ops[i]->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
			qp->qp_stats.enqueue_err_count++;
			break;
		}

		handle_completed_gcm_crypto_op(qp, ops[i]);

		qp->qp_stats.enqueued_count++;
	}
	return i;
}

static uint16_t
aesni_gcm_pmd_dequeue_burst(void *queue_pair,
		struct rte_crypto_op **ops, uint16_t nb_ops)
{
	struct aesni_gcm_qp *qp = queue_pair;

	unsigned nb_dequeued;

	nb_dequeued = rte_ring_dequeue_burst(qp->processed_pkts,
			(void **)ops, nb_ops);
	qp->qp_stats.dequeued_count += nb_dequeued;

	return nb_dequeued;
}

static int aesni_gcm_uninit(const char *name);

static int
aesni_gcm_create(const char *name,
		struct rte_crypto_vdev_init_params *init_params)
{
	struct rte_cryptodev *dev;
	char crypto_dev_name[RTE_CRYPTODEV_NAME_MAX_LEN];
	struct aesni_gcm_private *internals;
	enum aesni_gcm_vector_mode vector_mode;

	/* Check CPU for support for AES instruction set */
	if (!rte_cpu_get_flag_enabled(RTE_CPUFLAG_AES)) {
		GCM_LOG_ERR("AES instructions not supported by CPU");
		return -EFAULT;
	}

	/* Check CPU for supported vector instruction set */
	if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX2))
		vector_mode = RTE_AESNI_GCM_AVX2;
	else if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX))
		vector_mode = RTE_AESNI_GCM_AVX;
	else if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_SSE4_1))
		vector_mode = RTE_AESNI_GCM_SSE;
	else {
		GCM_LOG_ERR("Vector instructions are not supported by CPU");
		return -EFAULT;
	}

	/* create a unique device name */
	if (create_unique_device_name(crypto_dev_name,
			RTE_CRYPTODEV_NAME_MAX_LEN) != 0) {
		GCM_LOG_ERR("failed to create unique cryptodev name");
		return -EINVAL;
	}


	dev = rte_cryptodev_pmd_virtual_dev_init(crypto_dev_name,
			sizeof(struct aesni_gcm_private), init_params->socket_id);
	if (dev == NULL) {
		GCM_LOG_ERR("failed to create cryptodev vdev");
		goto init_error;
	}

	dev->dev_type = RTE_CRYPTODEV_AESNI_GCM_PMD;
	dev->dev_ops = rte_aesni_gcm_pmd_ops;

	/* register rx/tx burst functions for data path */
	dev->dequeue_burst = aesni_gcm_pmd_dequeue_burst;
	dev->enqueue_burst = aesni_gcm_pmd_enqueue_burst;

	dev->feature_flags = RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO |
			RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING |
			RTE_CRYPTODEV_FF_CPU_AESNI;

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

	/* Set vector instructions mode supported */
	internals = dev->data->dev_private;

	internals->vector_mode = vector_mode;

	internals->max_nb_queue_pairs = init_params->max_nb_queue_pairs;
	internals->max_nb_sessions = init_params->max_nb_sessions;

	return 0;

init_error:
	GCM_LOG_ERR("driver %s: create failed", name);

	aesni_gcm_uninit(crypto_dev_name);
	return -EFAULT;
}

static int
aesni_gcm_init(const char *name, const char *input_args)
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

	return aesni_gcm_create(name, &init_params);
}

static int
aesni_gcm_uninit(const char *name)
{
	if (name == NULL)
		return -EINVAL;

	GCM_LOG_INFO("Closing AESNI crypto device %s on numa socket %u\n",
			name, rte_socket_id());

	return 0;
}

static struct rte_driver aesni_gcm_pmd_drv = {
	.type = PMD_VDEV,
	.init = aesni_gcm_init,
	.uninit = aesni_gcm_uninit
};

PMD_REGISTER_DRIVER(aesni_gcm_pmd_drv, CRYPTODEV_NAME_AESNI_GCM_PMD);
DRIVER_REGISTER_PARAM_STRING(CRYPTODEV_NAME_AESNI_GCM_PMD,
	"max_nb_queue_pairs=<int> "
	"max_nb_sessions=<int> "
	"socket_id=<int>");
