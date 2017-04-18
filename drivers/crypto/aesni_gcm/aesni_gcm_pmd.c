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
#include <rte_vdev.h>
#include <rte_malloc.h>
#include <rte_cpuflags.h>
#include <rte_byteorder.h>

#include "aesni_gcm_pmd_private.h"

/** GCM encode functions pointer table */
static const struct aesni_gcm_ops aesni_gcm_enc[] = {
		[AESNI_GCM_KEY_128] = {
				aesni_gcm128_init,
				aesni_gcm128_enc_update,
				aesni_gcm128_enc_finalize
		},
		[AESNI_GCM_KEY_256] = {
				aesni_gcm256_init,
				aesni_gcm256_enc_update,
				aesni_gcm256_enc_finalize
		}
};

/** GCM decode functions pointer table */
static const struct aesni_gcm_ops aesni_gcm_dec[] = {
		[AESNI_GCM_KEY_128] = {
				aesni_gcm128_init,
				aesni_gcm128_dec_update,
				aesni_gcm128_dec_finalize
		},
		[AESNI_GCM_KEY_256] = {
				aesni_gcm256_init,
				aesni_gcm256_dec_update,
				aesni_gcm256_dec_finalize
		}
};

/** Parse crypto xform chain and set private session parameters */
int
aesni_gcm_set_session_parameters(struct aesni_gcm_session *sess,
		const struct rte_crypto_sym_xform *xform)
{
	const struct rte_crypto_sym_xform *auth_xform;
	const struct rte_crypto_sym_xform *cipher_xform;

	if (xform->next == NULL || xform->next->next != NULL) {
		GCM_LOG_ERR("Two and only two chained xform required");
		return -EINVAL;
	}

	if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER &&
			xform->next->type == RTE_CRYPTO_SYM_XFORM_AUTH) {
		auth_xform = xform->next;
		cipher_xform = xform;
	} else if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH &&
			xform->next->type == RTE_CRYPTO_SYM_XFORM_CIPHER) {
		auth_xform = xform;
		cipher_xform = xform->next;
	} else {
		GCM_LOG_ERR("Cipher and auth xform required");
		return -EINVAL;
	}

	if (!(cipher_xform->cipher.algo == RTE_CRYPTO_CIPHER_AES_GCM &&
		(auth_xform->auth.algo == RTE_CRYPTO_AUTH_AES_GCM ||
			auth_xform->auth.algo == RTE_CRYPTO_AUTH_AES_GMAC))) {
		GCM_LOG_ERR("We only support AES GCM and AES GMAC");
		return -EINVAL;
	}

	/* Select Crypto operation */
	if (cipher_xform->cipher.op == RTE_CRYPTO_CIPHER_OP_ENCRYPT &&
			auth_xform->auth.op == RTE_CRYPTO_AUTH_OP_GENERATE)
		sess->op = AESNI_GCM_OP_AUTHENTICATED_ENCRYPTION;
	else if (cipher_xform->cipher.op == RTE_CRYPTO_CIPHER_OP_DECRYPT &&
			auth_xform->auth.op == RTE_CRYPTO_AUTH_OP_VERIFY)
		sess->op = AESNI_GCM_OP_AUTHENTICATED_DECRYPTION;
	else {
		GCM_LOG_ERR("Cipher/Auth operations: Encrypt/Generate or"
				" Decrypt/Verify are valid only");
		return -EINVAL;
	}

	/* Check key length and calculate GCM pre-compute. */
	switch (cipher_xform->cipher.key.length) {
	case 16:
		aesni_gcm128_pre(cipher_xform->cipher.key.data, &sess->gdata);
		sess->key = AESNI_GCM_KEY_128;

		break;
	case 32:
		aesni_gcm256_pre(cipher_xform->cipher.key.data, &sess->gdata);
		sess->key = AESNI_GCM_KEY_256;

		break;
	default:
		GCM_LOG_ERR("Unsupported cipher key length");
		return -EINVAL;
	}

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
			((struct rte_cryptodev_sym_session *)_sess)->_private;

		if (unlikely(aesni_gcm_set_session_parameters(sess,
				op->xform) != 0)) {
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
process_gcm_crypto_op(struct rte_crypto_sym_op *op,
		struct aesni_gcm_session *session)
{
	uint8_t *src, *dst;
	struct rte_mbuf *m_src = op->m_src;
	uint32_t offset = op->cipher.data.offset;
	uint32_t part_len, total_len, data_len;

	RTE_ASSERT(m_src != NULL);

	while (offset >= m_src->data_len) {
		offset -= m_src->data_len;
		m_src = m_src->next;

		RTE_ASSERT(m_src != NULL);
	}

	data_len = m_src->data_len - offset;
	part_len = (data_len < op->cipher.data.length) ? data_len :
			op->cipher.data.length;

	/* Destination buffer is required when segmented source buffer */
	RTE_ASSERT((part_len == op->cipher.data.length) ||
			((part_len != op->cipher.data.length) &&
					(op->m_dst != NULL)));
	/* Segmented destination buffer is not supported */
	RTE_ASSERT((op->m_dst == NULL) ||
			((op->m_dst != NULL) &&
					rte_pktmbuf_is_contiguous(op->m_dst)));


	dst = op->m_dst ?
			rte_pktmbuf_mtod_offset(op->m_dst, uint8_t *,
					op->cipher.data.offset) :
			rte_pktmbuf_mtod_offset(op->m_src, uint8_t *,
					op->cipher.data.offset);

	src = rte_pktmbuf_mtod_offset(m_src, uint8_t *, offset);

	/* sanity checks */
	if (op->cipher.iv.length != 16 && op->cipher.iv.length != 12 &&
			op->cipher.iv.length != 0) {
		GCM_LOG_ERR("iv");
		return -1;
	}

	/*
	 * GCM working in 12B IV mode => 16B pre-counter block we need
	 * to set BE LSB to 1, driver expects that 16B is allocated
	 */
	if (op->cipher.iv.length == 12) {
		uint32_t *iv_padd = (uint32_t *)&op->cipher.iv.data[12];
		*iv_padd = rte_bswap32(1);
	}

	if (op->auth.digest.length != 16 &&
			op->auth.digest.length != 12 &&
			op->auth.digest.length != 8) {
		GCM_LOG_ERR("digest");
		return -1;
	}

	if (session->op == AESNI_GCM_OP_AUTHENTICATED_ENCRYPTION) {

		aesni_gcm_enc[session->key].init(&session->gdata,
				op->cipher.iv.data,
				op->auth.aad.data,
				(uint64_t)op->auth.aad.length);

		aesni_gcm_enc[session->key].update(&session->gdata, dst, src,
				(uint64_t)part_len);
		total_len = op->cipher.data.length - part_len;

		while (total_len) {
			dst += part_len;
			m_src = m_src->next;

			RTE_ASSERT(m_src != NULL);

			src = rte_pktmbuf_mtod(m_src, uint8_t *);
			part_len = (m_src->data_len < total_len) ?
					m_src->data_len : total_len;

			aesni_gcm_enc[session->key].update(&session->gdata,
					dst, src,
					(uint64_t)part_len);
			total_len -= part_len;
		}

		aesni_gcm_enc[session->key].finalize(&session->gdata,
				op->auth.digest.data,
				(uint64_t)op->auth.digest.length);
	} else { /* session->op == AESNI_GCM_OP_AUTHENTICATED_DECRYPTION */
		uint8_t *auth_tag = (uint8_t *)rte_pktmbuf_append(op->m_dst ?
				op->m_dst : op->m_src,
				op->auth.digest.length);

		if (!auth_tag) {
			GCM_LOG_ERR("auth_tag");
			return -1;
		}

		aesni_gcm_dec[session->key].init(&session->gdata,
				op->cipher.iv.data,
				op->auth.aad.data,
				(uint64_t)op->auth.aad.length);

		aesni_gcm_dec[session->key].update(&session->gdata, dst, src,
				(uint64_t)part_len);
		total_len = op->cipher.data.length - part_len;

		while (total_len) {
			dst += part_len;
			m_src = m_src->next;

			RTE_ASSERT(m_src != NULL);

			src = rte_pktmbuf_mtod(m_src, uint8_t *);
			part_len = (m_src->data_len < total_len) ?
					m_src->data_len : total_len;

			aesni_gcm_dec[session->key].update(&session->gdata,
					dst, src,
					(uint64_t)part_len);
			total_len -= part_len;
		}

		aesni_gcm_dec[session->key].finalize(&session->gdata,
				auth_tag,
				(uint64_t)op->auth.digest.length);
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

		sess = aesni_gcm_get_session(qp, ops[i]->sym);
		if (unlikely(sess == NULL)) {
			ops[i]->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
			qp->qp_stats.dequeue_err_count++;
			break;
		}

		retval = process_gcm_crypto_op(ops[i]->sym, sess);
		if (retval < 0) {
			ops[i]->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
			qp->qp_stats.dequeue_err_count++;
			break;
		}

		handle_completed_gcm_crypto_op(qp, ops[i]);
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
		struct rte_crypto_vdev_init_params *init_params)
{
	struct rte_cryptodev *dev;
	struct aesni_gcm_private *internals;

	if (init_params->name[0] == '\0')
		snprintf(init_params->name, sizeof(init_params->name),
				"%s", name);

	/* Check CPU for support for AES instruction set */
	if (!rte_cpu_get_flag_enabled(RTE_CPUFLAG_AES)) {
		GCM_LOG_ERR("AES instructions not supported by CPU");
		return -EFAULT;
	}

	dev = rte_cryptodev_pmd_virtual_dev_init(init_params->name,
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
			RTE_CRYPTODEV_FF_CPU_AESNI |
			RTE_CRYPTODEV_FF_MBUF_SCATTER_GATHER;

	internals = dev->data->dev_private;

	internals->max_nb_queue_pairs = init_params->max_nb_queue_pairs;
	internals->max_nb_sessions = init_params->max_nb_sessions;

	return 0;

init_error:
	GCM_LOG_ERR("driver %s: create failed", init_params->name);

	aesni_gcm_remove(vdev);
	return -EFAULT;
}

static int
aesni_gcm_probe(struct rte_vdev_device *vdev)
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

	return aesni_gcm_create(name, vdev, &init_params);
}

static int
aesni_gcm_remove(struct rte_vdev_device *vdev)
{
	const char *name;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	GCM_LOG_INFO("Closing AESNI crypto device %s on numa socket %u\n",
			name, rte_socket_id());

	return 0;
}

static struct rte_vdev_driver aesni_gcm_pmd_drv = {
	.probe = aesni_gcm_probe,
	.remove = aesni_gcm_remove
};

RTE_PMD_REGISTER_VDEV(CRYPTODEV_NAME_AESNI_GCM_PMD, aesni_gcm_pmd_drv);
RTE_PMD_REGISTER_ALIAS(CRYPTODEV_NAME_AESNI_GCM_PMD, cryptodev_aesni_gcm_pmd);
RTE_PMD_REGISTER_PARAM_STRING(CRYPTODEV_NAME_AESNI_GCM_PMD,
	"max_nb_queue_pairs=<int> "
	"max_nb_sessions=<int> "
	"socket_id=<int>");
