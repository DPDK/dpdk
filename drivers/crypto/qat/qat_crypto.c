/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *	 * Redistributions of source code must retain the above copyright
 *	   notice, this list of conditions and the following disclaimer.
 *	 * Redistributions in binary form must reproduce the above copyright
 *	   notice, this list of conditions and the following disclaimer in
 *	   the documentation and/or other materials provided with the
 *	   distribution.
 *	 * Neither the name of Intel Corporation nor the names of its
 *	   contributors may be used to endorse or promote products derived
 *	   from this software without specific prior written permission.
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

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/queue.h>
#include <stdarg.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_tailq.h>
#include <rte_ether.h>
#include <rte_malloc.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_string_fns.h>
#include <rte_spinlock.h>
#include <rte_mbuf_offload.h>
#include <rte_hexdump.h>

#include "qat_logs.h"
#include "qat_algs.h"
#include "qat_crypto.h"
#include "adf_transport_access_macros.h"


static inline uint32_t
adf_modulo(uint32_t data, uint32_t shift);

static inline int
qat_alg_write_mbuf_entry(struct rte_mbuf *mbuf, uint8_t *out_msg);

void qat_crypto_sym_clear_session(struct rte_cryptodev *dev,
		void *session)
{
	struct qat_session *sess = session;
	phys_addr_t cd_paddr = sess->cd_paddr;

	PMD_INIT_FUNC_TRACE();
	if (session) {
		memset(sess, 0, qat_crypto_sym_get_session_private_size(dev));

		sess->cd_paddr = cd_paddr;
	}
}

static int
qat_get_cmd_id(const struct rte_crypto_xform *xform)
{
	if (xform->next == NULL)
		return -1;

	/* Cipher Only */
	if (xform->type == RTE_CRYPTO_XFORM_CIPHER && xform->next == NULL)
		return -1; /* return ICP_QAT_FW_LA_CMD_CIPHER; */

	/* Authentication Only */
	if (xform->type == RTE_CRYPTO_XFORM_AUTH && xform->next == NULL)
		return -1; /* return ICP_QAT_FW_LA_CMD_AUTH; */

	/* Cipher then Authenticate */
	if (xform->type == RTE_CRYPTO_XFORM_CIPHER &&
			xform->next->type == RTE_CRYPTO_XFORM_AUTH)
		return ICP_QAT_FW_LA_CMD_CIPHER_HASH;

	/* Authenticate then Cipher */
	if (xform->type == RTE_CRYPTO_XFORM_AUTH &&
			xform->next->type == RTE_CRYPTO_XFORM_CIPHER)
		return ICP_QAT_FW_LA_CMD_HASH_CIPHER;

	return -1;
}

static struct rte_crypto_auth_xform *
qat_get_auth_xform(struct rte_crypto_xform *xform)
{
	do {
		if (xform->type == RTE_CRYPTO_XFORM_AUTH)
			return &xform->auth;

		xform = xform->next;
	} while (xform);

	return NULL;
}

static struct rte_crypto_cipher_xform *
qat_get_cipher_xform(struct rte_crypto_xform *xform)
{
	do {
		if (xform->type == RTE_CRYPTO_XFORM_CIPHER)
			return &xform->cipher;

		xform = xform->next;
	} while (xform);

	return NULL;
}


void *
qat_crypto_sym_configure_session(struct rte_cryptodev *dev,
		struct rte_crypto_xform *xform, void *session_private)
{
	struct qat_pmd_private *internals = dev->data->dev_private;

	struct qat_session *session = session_private;

	struct rte_crypto_auth_xform *auth_xform = NULL;
	struct rte_crypto_cipher_xform *cipher_xform = NULL;

	int qat_cmd_id;

	PMD_INIT_FUNC_TRACE();

	/* Get requested QAT command id */
	qat_cmd_id = qat_get_cmd_id(xform);
	if (qat_cmd_id < 0 || qat_cmd_id >= ICP_QAT_FW_LA_CMD_DELIMITER) {
		PMD_DRV_LOG(ERR, "Unsupported xform chain requested");
		goto error_out;
	}
	session->qat_cmd = (enum icp_qat_fw_la_cmd_id)qat_cmd_id;

	/* Get cipher xform from crypto xform chain */
	cipher_xform = qat_get_cipher_xform(xform);

	switch (cipher_xform->algo) {
	case RTE_CRYPTO_CIPHER_AES_CBC:
		if (qat_alg_validate_aes_key(cipher_xform->key.length,
				&session->qat_cipher_alg) != 0) {
			PMD_DRV_LOG(ERR, "Invalid AES cipher key size");
			goto error_out;
		}
		session->qat_mode = ICP_QAT_HW_CIPHER_CBC_MODE;
		break;
	case RTE_CRYPTO_CIPHER_AES_GCM:
		if (qat_alg_validate_aes_key(cipher_xform->key.length,
				&session->qat_cipher_alg) != 0) {
			PMD_DRV_LOG(ERR, "Invalid AES cipher key size");
			goto error_out;
		}
		session->qat_mode = ICP_QAT_HW_CIPHER_CTR_MODE;
		break;
	case RTE_CRYPTO_CIPHER_NULL:
	case RTE_CRYPTO_CIPHER_3DES_ECB:
	case RTE_CRYPTO_CIPHER_3DES_CBC:
	case RTE_CRYPTO_CIPHER_AES_ECB:
	case RTE_CRYPTO_CIPHER_AES_CTR:
	case RTE_CRYPTO_CIPHER_AES_CCM:
	case RTE_CRYPTO_CIPHER_KASUMI_F8:
		PMD_DRV_LOG(ERR, "Crypto: Unsupported Cipher alg %u",
				cipher_xform->algo);
		goto error_out;
	default:
		PMD_DRV_LOG(ERR, "Crypto: Undefined Cipher specified %u\n",
				cipher_xform->algo);
		goto error_out;
	}

	if (cipher_xform->op == RTE_CRYPTO_CIPHER_OP_ENCRYPT)
		session->qat_dir = ICP_QAT_HW_CIPHER_ENCRYPT;
	else
		session->qat_dir = ICP_QAT_HW_CIPHER_DECRYPT;


	/* Get authentication xform from Crypto xform chain */
	auth_xform = qat_get_auth_xform(xform);

	switch (auth_xform->algo) {
	case RTE_CRYPTO_AUTH_SHA1_HMAC:
		session->qat_hash_alg = ICP_QAT_HW_AUTH_ALGO_SHA1;
		break;
	case RTE_CRYPTO_AUTH_SHA256_HMAC:
		session->qat_hash_alg = ICP_QAT_HW_AUTH_ALGO_SHA256;
		break;
	case RTE_CRYPTO_AUTH_SHA512_HMAC:
		session->qat_hash_alg = ICP_QAT_HW_AUTH_ALGO_SHA512;
		break;
	case RTE_CRYPTO_AUTH_AES_XCBC_MAC:
		session->qat_hash_alg = ICP_QAT_HW_AUTH_ALGO_AES_XCBC_MAC;
		break;
	case RTE_CRYPTO_AUTH_AES_GCM:
	case RTE_CRYPTO_AUTH_AES_GMAC:
		session->qat_hash_alg = ICP_QAT_HW_AUTH_ALGO_GALOIS_128;
		break;
	case RTE_CRYPTO_AUTH_NULL:
	case RTE_CRYPTO_AUTH_SHA1:
	case RTE_CRYPTO_AUTH_SHA256:
	case RTE_CRYPTO_AUTH_SHA512:
	case RTE_CRYPTO_AUTH_SHA224:
	case RTE_CRYPTO_AUTH_SHA224_HMAC:
	case RTE_CRYPTO_AUTH_SHA384:
	case RTE_CRYPTO_AUTH_SHA384_HMAC:
	case RTE_CRYPTO_AUTH_MD5:
	case RTE_CRYPTO_AUTH_MD5_HMAC:
	case RTE_CRYPTO_AUTH_AES_CCM:
	case RTE_CRYPTO_AUTH_KASUMI_F9:
	case RTE_CRYPTO_AUTH_SNOW3G_UIA2:
	case RTE_CRYPTO_AUTH_AES_CMAC:
	case RTE_CRYPTO_AUTH_AES_CBC_MAC:
	case RTE_CRYPTO_AUTH_ZUC_EIA3:
		PMD_DRV_LOG(ERR, "Crypto: Unsupported hash alg %u",
				auth_xform->algo);
		goto error_out;
	default:
		PMD_DRV_LOG(ERR, "Crypto: Undefined Hash algo %u specified",
				auth_xform->algo);
		goto error_out;
	}

	if (qat_alg_aead_session_create_content_desc(session,
		cipher_xform->key.data,
		cipher_xform->key.length,
		auth_xform->key.data,
		auth_xform->key.length,
		auth_xform->add_auth_data_length,
		auth_xform->digest_length))
		goto error_out;

	return (struct rte_cryptodev_session *)session;

error_out:
	rte_mempool_put(internals->sess_mp, session);
	return NULL;
}

unsigned qat_crypto_sym_get_session_private_size(
		struct rte_cryptodev *dev __rte_unused)
{
	return RTE_ALIGN_CEIL(sizeof(struct qat_session), 8);
}


uint16_t qat_crypto_pkt_tx_burst(void *qp, struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts)
{
	register struct qat_queue *queue;
	struct qat_qp *tmp_qp = (struct qat_qp *)qp;
	register uint32_t nb_pkts_sent = 0;
	register struct rte_mbuf **cur_tx_pkt = tx_pkts;
	register int ret;
	uint16_t nb_pkts_possible = nb_pkts;
	register uint8_t *base_addr;
	register uint32_t tail;
	int overflow;

	/* read params used a lot in main loop into registers */
	queue = &(tmp_qp->tx_q);
	base_addr = (uint8_t *)queue->base_addr;
	tail = queue->tail;

	/* Find how many can actually fit on the ring */
	overflow = (rte_atomic16_add_return(&tmp_qp->inflights16, nb_pkts)
				- queue->max_inflights);
	if (overflow > 0) {
		rte_atomic16_sub(&tmp_qp->inflights16, overflow);
		nb_pkts_possible = nb_pkts - overflow;
		if (nb_pkts_possible == 0)
			return 0;
	}

	while (nb_pkts_sent != nb_pkts_possible) {

		ret = qat_alg_write_mbuf_entry(*cur_tx_pkt,
			base_addr + tail);
		if (ret != 0) {
			tmp_qp->stats.enqueue_err_count++;
			if (nb_pkts_sent == 0)
				return 0;
			goto kick_tail;
		}

		tail = adf_modulo(tail + queue->msg_size, queue->modulo);
		nb_pkts_sent++;
		cur_tx_pkt++;
	}
kick_tail:
	WRITE_CSR_RING_TAIL(tmp_qp->mmap_bar_addr, queue->hw_bundle_number,
			queue->hw_queue_number, tail);
	queue->tail = tail;
	tmp_qp->stats.enqueued_count += nb_pkts_sent;
	return nb_pkts_sent;
}

uint16_t
qat_crypto_pkt_rx_burst(void *qp, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct rte_mbuf_offload *ol;
	struct qat_queue *queue;
	struct qat_qp *tmp_qp = (struct qat_qp *)qp;
	uint32_t msg_counter = 0;
	struct rte_mbuf *rx_mbuf;
	struct icp_qat_fw_comn_resp *resp_msg;

	queue = &(tmp_qp->rx_q);
	resp_msg = (struct icp_qat_fw_comn_resp *)
			((uint8_t *)queue->base_addr + queue->head);

	while (*(uint32_t *)resp_msg != ADF_RING_EMPTY_SIG &&
			msg_counter != nb_pkts) {
		rx_mbuf = (struct rte_mbuf *)(resp_msg->opaque_data);
		ol = rte_pktmbuf_offload_get(rx_mbuf, RTE_PKTMBUF_OL_CRYPTO);

		if (ICP_QAT_FW_COMN_STATUS_FLAG_OK !=
				ICP_QAT_FW_COMN_RESP_CRYPTO_STAT_GET(
					resp_msg->comn_hdr.comn_status)) {
			ol->op.crypto.status = RTE_CRYPTO_OP_STATUS_AUTH_FAILED;
		} else {
			ol->op.crypto.status = RTE_CRYPTO_OP_STATUS_SUCCESS;
		}
		*(uint32_t *)resp_msg = ADF_RING_EMPTY_SIG;
		queue->head = adf_modulo(queue->head +
				queue->msg_size,
				ADF_RING_SIZE_MODULO(queue->queue_size));
		resp_msg = (struct icp_qat_fw_comn_resp *)
					((uint8_t *)queue->base_addr +
							queue->head);

		*rx_pkts = rx_mbuf;
		rx_pkts++;
		msg_counter++;
	}
	if (msg_counter > 0) {
		WRITE_CSR_RING_HEAD(tmp_qp->mmap_bar_addr,
					queue->hw_bundle_number,
					queue->hw_queue_number, queue->head);
		rte_atomic16_sub(&tmp_qp->inflights16, msg_counter);
		tmp_qp->stats.dequeued_count += msg_counter;
	}
	return msg_counter;
}

static inline int
qat_alg_write_mbuf_entry(struct rte_mbuf *mbuf, uint8_t *out_msg)
{
	struct rte_mbuf_offload *ol;

	struct qat_session *ctx;
	struct icp_qat_fw_la_cipher_req_params *cipher_param;
	struct icp_qat_fw_la_auth_req_params *auth_param;
	register struct icp_qat_fw_la_bulk_req *qat_req;

	ol = rte_pktmbuf_offload_get(mbuf, RTE_PKTMBUF_OL_CRYPTO);
	if (unlikely(ol == NULL)) {
		PMD_DRV_LOG(ERR, "No valid crypto off-load operation attached "
				"to (%p) mbuf.", mbuf);
		return -EINVAL;
	}

	if (unlikely(ol->op.crypto.type == RTE_CRYPTO_OP_SESSIONLESS)) {
		PMD_DRV_LOG(ERR, "QAT PMD only supports session oriented"
				" requests mbuf (%p) is sessionless.", mbuf);
		return -EINVAL;
	}

	if (unlikely(ol->op.crypto.session->type != RTE_CRYPTODEV_QAT_PMD)) {
		PMD_DRV_LOG(ERR, "Session was not created for this device");
		return -EINVAL;
	}

	ctx = (struct qat_session *)ol->op.crypto.session->_private;
	qat_req = (struct icp_qat_fw_la_bulk_req *)out_msg;
	*qat_req = ctx->fw_req;
	qat_req->comn_mid.opaque_data = (uint64_t)mbuf;

	/*
	 * The following code assumes:
	 * - single entry buffer.
	 * - always in place.
	 */
	qat_req->comn_mid.dst_length =
			qat_req->comn_mid.src_length = mbuf->data_len;
	qat_req->comn_mid.dest_data_addr =
			qat_req->comn_mid.src_data_addr =
					rte_pktmbuf_mtophys(mbuf);

	cipher_param = (void *)&qat_req->serv_specif_rqpars;
	auth_param = (void *)((uint8_t *)cipher_param + sizeof(*cipher_param));

	cipher_param->cipher_length = ol->op.crypto.data.to_cipher.length;
	cipher_param->cipher_offset = ol->op.crypto.data.to_cipher.offset;
	if (ol->op.crypto.iv.length &&
		(ol->op.crypto.iv.length <=
				sizeof(cipher_param->u.cipher_IV_array))) {
		rte_memcpy(cipher_param->u.cipher_IV_array,
				ol->op.crypto.iv.data, ol->op.crypto.iv.length);
	} else {
		ICP_QAT_FW_LA_CIPH_IV_FLD_FLAG_SET(
				qat_req->comn_hdr.serv_specif_flags,
				ICP_QAT_FW_CIPH_IV_64BIT_PTR);
		cipher_param->u.s.cipher_IV_ptr = ol->op.crypto.iv.phys_addr;
	}
	if (ol->op.crypto.digest.phys_addr) {
		ICP_QAT_FW_LA_DIGEST_IN_BUFFER_SET(
				qat_req->comn_hdr.serv_specif_flags,
				ICP_QAT_FW_LA_NO_DIGEST_IN_BUFFER);
		auth_param->auth_res_addr = ol->op.crypto.digest.phys_addr;
	}
	auth_param->auth_off = ol->op.crypto.data.to_hash.offset;
	auth_param->auth_len = ol->op.crypto.data.to_hash.length;
	auth_param->u1.aad_adr = ol->op.crypto.additional_auth.phys_addr;

	/* (GCM) aad length(240 max) will be at this location after precompute */
	if (ctx->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_GALOIS_128 ||
		ctx->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_GALOIS_64) {
		auth_param->u2.aad_sz =
		ALIGN_POW2_ROUNDUP(ctx->cd.hash.sha.state1[
					ICP_QAT_HW_GALOIS_128_STATE1_SZ +
					ICP_QAT_HW_GALOIS_H_SZ + 3], 16);
	}
	auth_param->hash_state_sz = (auth_param->u2.aad_sz) >> 3;

#ifdef RTE_LIBRTE_PMD_QAT_DEBUG_DRIVER
	rte_hexdump(stdout, "qat_req:", qat_req,
			sizeof(struct icp_qat_fw_la_bulk_req));
#endif
	return 0;
}

static inline uint32_t adf_modulo(uint32_t data, uint32_t shift)
{
	uint32_t div = data >> shift;
	uint32_t mult = div << shift;

	return data - mult;
}

void qat_crypto_sym_session_init(struct rte_mempool *mp, void *priv_sess)
{
	struct qat_session *s = priv_sess;

	PMD_INIT_FUNC_TRACE();
	s->cd_paddr = rte_mempool_virt2phy(mp, &s->cd);
}

int qat_dev_config(__rte_unused struct rte_cryptodev *dev)
{
	PMD_INIT_FUNC_TRACE();
	return -ENOTSUP;
}

int qat_dev_start(__rte_unused struct rte_cryptodev *dev)
{
	PMD_INIT_FUNC_TRACE();
	return 0;
}

void qat_dev_stop(__rte_unused struct rte_cryptodev *dev)
{
	PMD_INIT_FUNC_TRACE();
}

int qat_dev_close(struct rte_cryptodev *dev)
{
	int i, ret;

	PMD_INIT_FUNC_TRACE();

	for (i = 0; i < dev->data->nb_queue_pairs; i++) {
		ret = qat_crypto_sym_qp_release(dev, i);
		if (ret < 0)
			return ret;
	}

	return 0;
}

void qat_dev_info_get(__rte_unused struct rte_cryptodev *dev,
				struct rte_cryptodev_info *info)
{
	struct qat_pmd_private *internals = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();
	if (info != NULL) {
		info->max_nb_queue_pairs =
				ADF_NUM_SYM_QPS_PER_BUNDLE *
				ADF_NUM_BUNDLES_PER_DEV;

		info->max_nb_sessions = internals->max_nb_sessions;
		info->dev_type = RTE_CRYPTODEV_QAT_PMD;
	}
}

void qat_crypto_sym_stats_get(struct rte_cryptodev *dev,
		struct rte_cryptodev_stats *stats)
{
	int i;
	struct qat_qp **qp = (struct qat_qp **)(dev->data->queue_pairs);

	PMD_INIT_FUNC_TRACE();
	if (stats == NULL) {
		PMD_DRV_LOG(ERR, "invalid stats ptr NULL");
		return;
	}
	for (i = 0; i < dev->data->nb_queue_pairs; i++) {
		if (qp[i] == NULL) {
			PMD_DRV_LOG(DEBUG, "Uninitialised queue pair");
			continue;
		}

		stats->enqueued_count += qp[i]->stats.enqueued_count;
		stats->dequeued_count += qp[i]->stats.enqueued_count;
		stats->enqueue_err_count += qp[i]->stats.enqueue_err_count;
		stats->dequeue_err_count += qp[i]->stats.enqueue_err_count;
	}
}

void qat_crypto_sym_stats_reset(struct rte_cryptodev *dev)
{
	int i;
	struct qat_qp **qp = (struct qat_qp **)(dev->data->queue_pairs);

	PMD_INIT_FUNC_TRACE();
	for (i = 0; i < dev->data->nb_queue_pairs; i++)
		memset(&(qp[i]->stats), 0, sizeof(qp[i]->stats));
	PMD_DRV_LOG(DEBUG, "QAT crypto: stats cleared");
}
