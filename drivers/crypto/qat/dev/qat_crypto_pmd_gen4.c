/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2022 Intel Corporation
 */

#include <rte_cryptodev.h>
#include <cryptodev_pmd.h>
#include "qat_sym_session.h"
#include "qat_sym.h"
#include "qat_asym.h"
#include "qat_crypto.h"
#include "qat_crypto_pmd_gens.h"
#include "adf_transport_access_macros_gen4vf.h"


static struct rte_cryptodev_capabilities qat_sym_crypto_legacy_caps_gen4[] = {
	QAT_SYM_PLAIN_AUTH_CAP(SHA1,
		CAP_SET(block_size, 64),
		CAP_RNG(digest_size, 1, 20, 1)),
	QAT_SYM_AUTH_CAP(SHA224,
		CAP_SET(block_size, 64),
		CAP_RNG_ZERO(key_size), CAP_RNG(digest_size, 1, 28, 1),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_AUTH_CAP(SHA224_HMAC,
		CAP_SET(block_size, 64),
		CAP_RNG(key_size, 1, 64, 1), CAP_RNG(digest_size, 1, 28, 1),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_AUTH_CAP(SHA1_HMAC,
		CAP_SET(block_size, 64),
		CAP_RNG(key_size, 1, 64, 1), CAP_RNG(digest_size, 1, 20, 1),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_CIPHER_CAP(SM4_ECB,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 16, 0), CAP_RNG(iv_size, 0, 0, 0)),
};

static struct rte_cryptodev_capabilities qat_sym_crypto_caps_gen4[] = {
	QAT_SYM_CIPHER_CAP(AES_CBC,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 32, 8), CAP_RNG(iv_size, 16, 16, 0)),
	QAT_SYM_AUTH_CAP(SHA256_HMAC,
		CAP_SET(block_size, 64),
		CAP_RNG(key_size, 1, 64, 1), CAP_RNG(digest_size, 1, 32, 1),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_AUTH_CAP(SHA384_HMAC,
		CAP_SET(block_size, 128),
		CAP_RNG(key_size, 1, 128, 1), CAP_RNG(digest_size, 1, 48, 1),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_AUTH_CAP(SHA512_HMAC,
		CAP_SET(block_size, 128),
		CAP_RNG(key_size, 1, 128, 1), CAP_RNG(digest_size, 1, 64, 1),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_AUTH_CAP(AES_XCBC_MAC,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 16, 0), CAP_RNG(digest_size, 12, 12, 0),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_AUTH_CAP(AES_CMAC,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 16, 0), CAP_RNG(digest_size, 4, 16, 4),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_CIPHER_CAP(AES_DOCSISBPI,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 32, 16), CAP_RNG(iv_size, 16, 16, 0)),
	QAT_SYM_AUTH_CAP(NULL,
		CAP_SET(block_size, 1),
		CAP_RNG_ZERO(key_size), CAP_RNG_ZERO(digest_size),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_CIPHER_CAP(NULL,
		CAP_SET(block_size, 1),
		CAP_RNG_ZERO(key_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_AUTH_CAP(SHA256,
		CAP_SET(block_size, 64),
		CAP_RNG_ZERO(key_size), CAP_RNG(digest_size, 1, 32, 1),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_AUTH_CAP(SHA384,
		CAP_SET(block_size, 128),
		CAP_RNG_ZERO(key_size), CAP_RNG(digest_size, 1, 48, 1),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_AUTH_CAP(SHA512,
		CAP_SET(block_size, 128),
		CAP_RNG_ZERO(key_size), CAP_RNG(digest_size, 1, 64, 1),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_CIPHER_CAP(AES_CTR,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 32, 8), CAP_RNG(iv_size, 16, 16, 0)),
	QAT_SYM_AEAD_CAP(AES_GCM,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 32, 8), CAP_RNG(digest_size, 8, 16, 4),
		CAP_RNG(aad_size, 0, 240, 1), CAP_RNG(iv_size, 0, 12, 12)),
	QAT_SYM_AEAD_CAP(AES_CCM,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 16, 0), CAP_RNG(digest_size, 4, 16, 2),
		CAP_RNG(aad_size, 0, 224, 1), CAP_RNG(iv_size, 7, 13, 1)),
	QAT_SYM_AUTH_CAP(AES_GMAC,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 32, 8), CAP_RNG(digest_size, 8, 16, 4),
		CAP_RNG_ZERO(aad_size), CAP_RNG(iv_size, 0, 12, 12)),
	QAT_SYM_AEAD_CAP(CHACHA20_POLY1305,
		CAP_SET(block_size, 64),
		CAP_RNG(key_size, 32, 32, 0),
		CAP_RNG(digest_size, 16, 16, 0),
		CAP_RNG(aad_size, 0, 240, 1), CAP_RNG(iv_size, 12, 12, 0)),
	QAT_SYM_CIPHER_CAP(SM4_CBC,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 16, 0), CAP_RNG(iv_size, 16, 16, 0)),
	QAT_SYM_CIPHER_CAP(SM4_CTR,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 16, 0), CAP_RNG(iv_size, 16, 16, 0)),
	QAT_SYM_PLAIN_AUTH_CAP(SM3,
		CAP_SET(block_size, 64),
		CAP_RNG(digest_size, 32, 32, 0)),
	QAT_SYM_AUTH_CAP(SM3_HMAC,
		CAP_SET(block_size, 64),
		CAP_RNG(key_size, 16, 64, 4), CAP_RNG(digest_size, 32, 32, 0),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

static struct rte_cryptodev_capabilities qat_asym_crypto_caps_gen4[] = {
	QAT_ASYM_CAP(MODEX,
		0, 1, 512, 1),
	QAT_ASYM_CAP(MODINV,
		0, 1, 512, 1),
	QAT_ASYM_CAP(RSA,
			((1 << RTE_CRYPTO_ASYM_OP_SIGN) |
			(1 << RTE_CRYPTO_ASYM_OP_VERIFY) |
			(1 << RTE_CRYPTO_ASYM_OP_ENCRYPT) |
			(1 << RTE_CRYPTO_ASYM_OP_DECRYPT)),
			64, 512, 64),
	{	/* SM2 */
		.op = RTE_CRYPTO_OP_TYPE_ASYMMETRIC,
		{.asym = {
			.xform_capa = {
				.xform_type = RTE_CRYPTO_ASYM_XFORM_SM2,
				.op_types =
				((1 << RTE_CRYPTO_ASYM_OP_SIGN) |
				 (1 << RTE_CRYPTO_ASYM_OP_VERIFY) |
				 (1 << RTE_CRYPTO_ASYM_OP_ENCRYPT) |
				 (1 << RTE_CRYPTO_ASYM_OP_DECRYPT)),
				.op_capa = {
					[RTE_CRYPTO_ASYM_OP_ENCRYPT] = (1 << RTE_CRYPTO_SM2_PARTIAL),
					[RTE_CRYPTO_ASYM_OP_DECRYPT] = (1 << RTE_CRYPTO_SM2_PARTIAL),
				},
			},
		}
		}
	},
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

static int
qat_sym_crypto_cap_get_gen4(struct qat_cryptodev_private *internals,
			const char *capa_memz_name,
			const uint16_t __rte_unused slice_map)
{
	uint32_t legacy_capa_num;
	uint32_t size = sizeof(qat_sym_crypto_caps_gen4);
	uint32_t legacy_size = sizeof(qat_sym_crypto_legacy_caps_gen4);
	legacy_capa_num = legacy_size/sizeof(struct rte_cryptodev_capabilities);

	if (unlikely(internals->qat_dev->options.legacy_alg))
		size = size + legacy_size;

	internals->capa_mz = rte_memzone_lookup(capa_memz_name);
	if (internals->capa_mz == NULL) {
		internals->capa_mz = rte_memzone_reserve(capa_memz_name,
				size, rte_socket_id(), 0);
		if (internals->capa_mz == NULL) {
			QAT_LOG(DEBUG,
				"Error allocating memzone for capabilities");
			return -1;
		}
	}

	struct rte_cryptodev_capabilities *addr =
			(struct rte_cryptodev_capabilities *)
				internals->capa_mz->addr;

	struct rte_cryptodev_capabilities *capabilities;

	if (unlikely(internals->qat_dev->options.legacy_alg)) {
		capabilities = qat_sym_crypto_legacy_caps_gen4;
		memcpy(addr, capabilities, legacy_size);
		addr += legacy_capa_num;
	}
	capabilities = qat_sym_crypto_caps_gen4;
	memcpy(addr, capabilities, sizeof(qat_sym_crypto_caps_gen4));
	internals->qat_dev_capabilities = internals->capa_mz->addr;

	return 0;
}

static int
qat_asym_crypto_cap_get_gen4(struct qat_cryptodev_private *internals,
			const char *capa_memz_name,
			const uint16_t __rte_unused slice_map)
{
	const uint32_t size = sizeof(qat_asym_crypto_caps_gen4);
	uint32_t i;

	internals->capa_mz = rte_memzone_lookup(capa_memz_name);
	if (internals->capa_mz == NULL) {
		internals->capa_mz = rte_memzone_reserve(capa_memz_name,
				size, rte_socket_id(), 0);
		if (internals->capa_mz == NULL) {
			QAT_LOG(DEBUG,
				"Error allocating memzone for capabilities");
			return -1;
		}
	}

	struct rte_cryptodev_capabilities *addr =
			(struct rte_cryptodev_capabilities *)
				internals->capa_mz->addr;
	const struct rte_cryptodev_capabilities *capabilities =
		qat_asym_crypto_caps_gen4;
	const uint32_t capa_num =
		size / sizeof(struct rte_cryptodev_capabilities);
	uint32_t curr_capa = 0;

	for (i = 0; i < capa_num; i++) {
		memcpy(addr + curr_capa, capabilities + i,
			sizeof(struct rte_cryptodev_capabilities));
		curr_capa++;
	}
	internals->qat_dev_capabilities = internals->capa_mz->addr;

	return 0;
}

static __rte_always_inline void
enqueue_one_aead_job_gen4(struct qat_sym_session *ctx,
	struct icp_qat_fw_la_bulk_req *req,
	struct rte_crypto_va_iova_ptr *iv,
	struct rte_crypto_va_iova_ptr *digest,
	struct rte_crypto_va_iova_ptr *aad,
	union rte_crypto_sym_ofs ofs, uint32_t data_len)
{
	if (ctx->is_single_pass && ctx->is_ucs) {
		struct icp_qat_fw_la_cipher_20_req_params *cipher_param_20 =
			(void *)&req->serv_specif_rqpars;
		struct icp_qat_fw_la_cipher_req_params *cipher_param =
			(void *)&req->serv_specif_rqpars;

		/* QAT GEN4 uses single pass to treat AEAD as cipher
		 * operation
		 */
		qat_set_cipher_iv(cipher_param, iv, ctx->cipher_iv.length,
				req);
		cipher_param->cipher_offset = ofs.ofs.cipher.head;
		cipher_param->cipher_length = data_len -
				ofs.ofs.cipher.head - ofs.ofs.cipher.tail;

		cipher_param_20->spc_aad_addr = aad->iova;
		cipher_param_20->spc_auth_res_addr = digest->iova;

		return;
	}

	enqueue_one_aead_job_gen1(ctx, req, iv, digest, aad, ofs, data_len);
}

static int
qat_sym_build_op_aead_gen4(void *in_op, struct qat_sym_session *ctx,
		uint8_t *out_msg, void *op_cookie)
{
	register struct icp_qat_fw_la_bulk_req *qat_req;
	struct rte_crypto_op *op = in_op;
	struct qat_sym_op_cookie *cookie = op_cookie;
	struct rte_crypto_sgl in_sgl, out_sgl;
	struct rte_crypto_vec in_vec[QAT_SYM_SGL_MAX_NUMBER],
			out_vec[QAT_SYM_SGL_MAX_NUMBER];
	struct rte_crypto_va_iova_ptr cipher_iv;
	struct rte_crypto_va_iova_ptr aad;
	struct rte_crypto_va_iova_ptr digest;
	union rte_crypto_sym_ofs ofs;
	int32_t total_len;

	in_sgl.vec = in_vec;
	out_sgl.vec = out_vec;

	qat_req = (struct icp_qat_fw_la_bulk_req *)out_msg;
	rte_mov128((uint8_t *)qat_req, (const uint8_t *)&(ctx->fw_req));

	ofs.raw = qat_sym_convert_op_to_vec_aead(op, ctx, &in_sgl, &out_sgl,
			&cipher_iv, &aad, &digest);
	if (unlikely(ofs.raw == UINT64_MAX)) {
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		return -EINVAL;
	}

	total_len = qat_sym_build_req_set_data(qat_req, in_op, cookie,
			in_sgl.vec, in_sgl.num, out_sgl.vec, out_sgl.num);
	if (unlikely(total_len < 0)) {
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		return -EINVAL;
	}

	enqueue_one_aead_job_gen4(ctx, qat_req, &cipher_iv, &digest, &aad, ofs,
		total_len);

	qat_sym_debug_log_dump(qat_req, ctx, in_sgl.vec, in_sgl.num, &cipher_iv,
			NULL, &aad, &digest);

	return 0;
}

int
qat_sym_dp_enqueue_done_gen4(void *qp_data, uint8_t *drv_ctx, uint32_t n)
{
	struct qat_qp *qp = qp_data;
	struct qat_queue *tx_queue = &qp->tx_q;
	struct qat_sym_dp_ctx *dp_ctx = (void *)drv_ctx;

	if (unlikely(dp_ctx->cached_enqueue != n))
		return -1;

	qp->enqueued += n;
	qp->stats.enqueued_count += n;

	tx_queue->tail = dp_ctx->tail;

	WRITE_CSR_RING_TAIL_GEN4VF(qp->mmap_bar_addr,
		tx_queue->hw_bundle_number,
		tx_queue->hw_queue_number, tx_queue->tail);

	tx_queue->csr_tail = tx_queue->tail;
	dp_ctx->cached_enqueue = 0;

	return 0;
}

int
qat_sym_dp_dequeue_done_gen4(void *qp_data, uint8_t *drv_ctx, uint32_t n)
{
	struct qat_qp *qp = qp_data;
	struct qat_queue *rx_queue = &qp->rx_q;
	struct qat_sym_dp_ctx *dp_ctx = (void *)drv_ctx;

	if (unlikely(dp_ctx->cached_dequeue != n))
		return -1;

	rx_queue->head = dp_ctx->head;
	rx_queue->nb_processed_responses += n;
	qp->dequeued += n;
	qp->stats.dequeued_count += n;
	if (rx_queue->nb_processed_responses > QAT_CSR_HEAD_WRITE_THRESH) {
		uint32_t old_head, new_head;
		uint32_t max_head;

		old_head = rx_queue->csr_head;
		new_head = rx_queue->head;
		max_head = qp->nb_descriptors * rx_queue->msg_size;

		/* write out free descriptors */
		void *cur_desc = (uint8_t *)rx_queue->base_addr + old_head;

		if (new_head < old_head) {
			memset(cur_desc, ADF_RING_EMPTY_SIG_BYTE,
					max_head - old_head);
			memset(rx_queue->base_addr, ADF_RING_EMPTY_SIG_BYTE,
					new_head);
		} else {
			memset(cur_desc, ADF_RING_EMPTY_SIG_BYTE, new_head -
					old_head);
		}
		rx_queue->nb_processed_responses = 0;
		rx_queue->csr_head = new_head;

		/* write current head to CSR */
		WRITE_CSR_RING_HEAD_GEN4VF(qp->mmap_bar_addr,
			rx_queue->hw_bundle_number, rx_queue->hw_queue_number,
			new_head);
	}

	dp_ctx->cached_dequeue = 0;
	return 0;
}

int
qat_sym_crypto_set_session_gen4(void *cdev, void *session)
{
	struct qat_sym_session *ctx = session;
	enum rte_proc_type_t proc_type = rte_eal_process_type();
	int ret;

	if (proc_type == RTE_PROC_AUTO || proc_type == RTE_PROC_INVALID)
		return -EINVAL;

	ret = qat_sym_crypto_set_session_gen1(cdev, session);
	/* special single pass build request for GEN4 */
	if (ctx->is_single_pass && ctx->is_ucs)
		ctx->build_request[proc_type] = qat_sym_build_op_aead_gen4;

	if (ret == -ENOTSUP) {
		/* GEN1 returning -ENOTSUP as it cannot handle some mixed algo,
		 * this is addressed by GEN4
		 */
		if (ctx->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_ZUC_3G_128_EIA3 &&
				ctx->qat_cipher_alg !=
				ICP_QAT_HW_CIPHER_ALGO_ZUC_3G_128_EEA3) {
			qat_sym_session_set_ext_hash_flags_gen2(ctx,
				1 << ICP_QAT_FW_AUTH_HDR_FLAG_ZUC_EIA3_BITPOS);
		} else if (ctx->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_SNOW_3G_UIA2 &&
				ctx->qat_cipher_alg !=
				ICP_QAT_HW_CIPHER_ALGO_SNOW_3G_UEA2) {
			qat_sym_session_set_ext_hash_flags_gen2(ctx,
				1 << ICP_QAT_FW_AUTH_HDR_FLAG_SNOW3G_UIA2_BITPOS);
		} else if ((ctx->aes_cmac ||
				ctx->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_NULL) &&
				(ctx->qat_cipher_alg ==
				ICP_QAT_HW_CIPHER_ALGO_SNOW_3G_UEA2 ||
				ctx->qat_cipher_alg ==
				ICP_QAT_HW_CIPHER_ALGO_ZUC_3G_128_EEA3)) {
			qat_sym_session_set_ext_hash_flags_gen2(ctx, 0);
		}

		ret = 0;
	}

	return ret;
}

static int
qat_sym_dp_enqueue_single_aead_gen4(void *qp_data, uint8_t *drv_ctx,
	struct rte_crypto_vec *data, uint16_t n_data_vecs,
	union rte_crypto_sym_ofs ofs,
	struct rte_crypto_va_iova_ptr *iv,
	struct rte_crypto_va_iova_ptr *digest,
	struct rte_crypto_va_iova_ptr *aad,
	void *user_data)
{
	struct qat_qp *qp = qp_data;
	struct qat_sym_dp_ctx *dp_ctx = (void *)drv_ctx;
	struct qat_queue *tx_queue = &qp->tx_q;
	struct qat_sym_op_cookie *cookie;
	struct qat_sym_session *ctx = dp_ctx->session;
	struct icp_qat_fw_la_bulk_req *req;

	int32_t data_len;
	uint32_t tail = dp_ctx->tail;

	req = (struct icp_qat_fw_la_bulk_req *)(
		(uint8_t *)tx_queue->base_addr + tail);
	cookie = qp->op_cookies[tail >> tx_queue->trailz];
	tail = (tail + tx_queue->msg_size) & tx_queue->modulo_mask;
	rte_mov128((uint8_t *)req, (const uint8_t *)&(ctx->fw_req));
	rte_prefetch0((uint8_t *)tx_queue->base_addr + tail);
	data_len = qat_sym_build_req_set_data(req, user_data, cookie,
			data, n_data_vecs, NULL, 0);
	if (unlikely(data_len < 0))
		return -1;

	enqueue_one_aead_job_gen4(ctx, req, iv, digest, aad, ofs,
		(uint32_t)data_len);

	dp_ctx->tail = tail;
	dp_ctx->cached_enqueue++;

	qat_sym_debug_log_dump(req, ctx, data, n_data_vecs, iv,
			NULL, aad, digest);

	return 0;
}

static uint32_t
qat_sym_dp_enqueue_aead_jobs_gen4(void *qp_data, uint8_t *drv_ctx,
	struct rte_crypto_sym_vec *vec, union rte_crypto_sym_ofs ofs,
	void *user_data[], int *status)
{
	struct qat_qp *qp = qp_data;
	struct qat_sym_dp_ctx *dp_ctx = (void *)drv_ctx;
	struct qat_queue *tx_queue = &qp->tx_q;
	struct qat_sym_session *ctx = dp_ctx->session;
	uint32_t i, n;
	uint32_t tail;
	struct icp_qat_fw_la_bulk_req *req;
	int32_t data_len;

	n = QAT_SYM_DP_GET_MAX_ENQ(qp, dp_ctx->cached_enqueue, vec->num);
	if (unlikely(n == 0)) {
		qat_sym_dp_fill_vec_status(vec->status, -1, vec->num);
		*status = 0;
		return 0;
	}

	tail = dp_ctx->tail;

	for (i = 0; i < n; i++) {
		struct qat_sym_op_cookie *cookie =
			qp->op_cookies[tail >> tx_queue->trailz];
		int error = 0;

		req  = (struct icp_qat_fw_la_bulk_req *)(
			(uint8_t *)tx_queue->base_addr + tail);
		rte_mov128((uint8_t *)req, (const uint8_t *)&(ctx->fw_req));

		if (vec->dest_sgl) {
			data_len = qat_reqs_mid_set(&error, req, cookie, user_data[i],
					&vec->src_sgl[i], &vec->dest_sgl[i], ofs);
			/* In oop there is no offset, src/dst addresses are moved
			 * to avoid overwriting the dst header
			 */
			ofs.ofs.cipher.head = 0;
		} else {
			data_len = qat_sym_build_req_set_data(req,
				user_data[i], cookie,
				vec->src_sgl[i].vec,
				vec->src_sgl[i].num, NULL, 0);
		}

		if (unlikely(data_len < 0) || error)
			break;

		enqueue_one_aead_job_gen4(ctx, req, &vec->iv[i],
				&vec->digest[i], &vec->aad[i], ofs,
				(uint32_t)data_len);

		tail = (tail + tx_queue->msg_size) & tx_queue->modulo_mask;

		qat_sym_debug_log_dump(req, ctx, vec->src_sgl[i].vec,
				vec->src_sgl[i].num, &vec->iv[i], NULL,
				&vec->aad[i], &vec->digest[i]);
	}

	if (unlikely(i < n))
		qat_sym_dp_fill_vec_status(vec->status + i, -1, n - i);

	dp_ctx->tail = tail;
	dp_ctx->cached_enqueue += i;
	*status = 0;
	return i;
}

int
qat_sym_configure_raw_dp_ctx_gen4(void *_raw_dp_ctx, void *_ctx)
{
	struct rte_crypto_raw_dp_ctx *raw_dp_ctx = _raw_dp_ctx;
	struct qat_sym_session *ctx = _ctx;

	raw_dp_ctx->enqueue_done = qat_sym_dp_enqueue_done_gen4;
	raw_dp_ctx->dequeue_burst = qat_sym_dp_dequeue_burst_gen1;
	raw_dp_ctx->dequeue = qat_sym_dp_dequeue_single_gen1;
	raw_dp_ctx->dequeue_done = qat_sym_dp_dequeue_done_gen4;

	if ((ctx->qat_cmd == ICP_QAT_FW_LA_CMD_HASH_CIPHER ||
			ctx->qat_cmd == ICP_QAT_FW_LA_CMD_CIPHER_HASH) &&
			!ctx->is_gmac) {
		/* AES-GCM or AES-CCM */
		if (ctx->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_GALOIS_128 ||
			ctx->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_GALOIS_64 ||
			(ctx->qat_cipher_alg == ICP_QAT_HW_CIPHER_ALGO_AES128
			&& ctx->qat_mode == ICP_QAT_HW_CIPHER_CTR_MODE
			&& ctx->qat_hash_alg ==
					ICP_QAT_HW_AUTH_ALGO_AES_CBC_MAC)) {
			raw_dp_ctx->enqueue_burst =
					qat_sym_dp_enqueue_aead_jobs_gen1;
			raw_dp_ctx->enqueue =
					qat_sym_dp_enqueue_single_aead_gen1;
		} else {
			raw_dp_ctx->enqueue_burst =
					qat_sym_dp_enqueue_chain_jobs_gen1;
			raw_dp_ctx->enqueue =
					qat_sym_dp_enqueue_single_chain_gen1;
		}
	} else if (ctx->qat_cmd == ICP_QAT_FW_LA_CMD_AUTH || ctx->is_gmac) {
		raw_dp_ctx->enqueue_burst = qat_sym_dp_enqueue_auth_jobs_gen1;
		raw_dp_ctx->enqueue = qat_sym_dp_enqueue_single_auth_gen1;
	} else if (ctx->qat_cmd == ICP_QAT_FW_LA_CMD_CIPHER) {
		if (ctx->qat_mode == ICP_QAT_HW_CIPHER_AEAD_MODE ||
			ctx->qat_cipher_alg ==
				ICP_QAT_HW_CIPHER_ALGO_CHACHA20_POLY1305) {
			raw_dp_ctx->enqueue_burst =
					qat_sym_dp_enqueue_aead_jobs_gen1;
			raw_dp_ctx->enqueue =
					qat_sym_dp_enqueue_single_aead_gen1;
		} else {
			raw_dp_ctx->enqueue_burst =
					qat_sym_dp_enqueue_cipher_jobs_gen1;
			raw_dp_ctx->enqueue =
					qat_sym_dp_enqueue_single_cipher_gen1;
		}
	} else
		return -1;

	if (ctx->is_single_pass && ctx->is_ucs) {
		raw_dp_ctx->enqueue_burst = qat_sym_dp_enqueue_aead_jobs_gen4;
		raw_dp_ctx->enqueue = qat_sym_dp_enqueue_single_aead_gen4;
	}

	return 0;
}

RTE_INIT(qat_sym_crypto_gen4_init)
{
	qat_sym_gen_dev_ops[QAT_VQAT].cryptodev_ops =
		qat_sym_gen_dev_ops[QAT_GEN4].cryptodev_ops = &qat_sym_crypto_ops_gen1;
	qat_sym_gen_dev_ops[QAT_VQAT].get_capabilities =
		qat_sym_gen_dev_ops[QAT_GEN4].get_capabilities =
			qat_sym_crypto_cap_get_gen4;
	qat_sym_gen_dev_ops[QAT_VQAT].set_session =
		qat_sym_gen_dev_ops[QAT_GEN4].set_session =
			qat_sym_crypto_set_session_gen4;
	qat_sym_gen_dev_ops[QAT_GEN4].set_raw_dp_ctx =
			qat_sym_configure_raw_dp_ctx_gen4;
	qat_sym_gen_dev_ops[QAT_VQAT].get_feature_flags =
		qat_sym_gen_dev_ops[QAT_GEN4].get_feature_flags =
			qat_sym_crypto_feature_flags_get_gen1;
	qat_sym_gen_dev_ops[QAT_GEN4].create_security_ctx =
			qat_sym_create_security_gen1;
}

RTE_INIT(qat_asym_crypto_gen4_init)
{
	qat_asym_gen_dev_ops[QAT_VQAT].cryptodev_ops =
		qat_asym_gen_dev_ops[QAT_GEN4].cryptodev_ops =
			&qat_asym_crypto_ops_gen1;
	qat_asym_gen_dev_ops[QAT_VQAT].get_capabilities =
		qat_asym_gen_dev_ops[QAT_GEN4].get_capabilities =
			qat_asym_crypto_cap_get_gen4;
	qat_asym_gen_dev_ops[QAT_VQAT].get_feature_flags =
		qat_asym_gen_dev_ops[QAT_GEN4].get_feature_flags =
			qat_asym_crypto_feature_flags_get_gen1;
	qat_asym_gen_dev_ops[QAT_VQAT].set_session =
		qat_asym_gen_dev_ops[QAT_GEN4].set_session =
			qat_asym_crypto_set_session_gen1;
}
