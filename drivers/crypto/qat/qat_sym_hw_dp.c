/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include <rte_cryptodev_pmd.h>

#include "adf_transport_access_macros.h"
#include "icp_qat_fw.h"
#include "icp_qat_fw_la.h"

#include "qat_sym.h"
#include "qat_sym_pmd.h"
#include "qat_sym_session.h"
#include "qat_qp.h"

struct qat_sym_dp_ctx {
	struct qat_sym_session *session;
	uint32_t tail;
	uint32_t head;
	uint16_t cached_enqueue;
	uint16_t cached_dequeue;
};

static __rte_always_inline int32_t
qat_sym_dp_parse_data_vec(struct qat_qp *qp, struct icp_qat_fw_la_bulk_req *req,
		struct rte_crypto_vec *data, uint16_t n_data_vecs)
{
	struct qat_queue *tx_queue;
	struct qat_sym_op_cookie *cookie;
	struct qat_sgl *list;
	uint32_t i;
	uint32_t total_len;

	if (likely(n_data_vecs == 1)) {
		req->comn_mid.src_data_addr = req->comn_mid.dest_data_addr =
			data[0].iova;
		req->comn_mid.src_length = req->comn_mid.dst_length =
			data[0].len;
		return data[0].len;
	}

	if (n_data_vecs == 0 || n_data_vecs > QAT_SYM_SGL_MAX_NUMBER)
		return -1;

	total_len = 0;
	tx_queue = &qp->tx_q;

	ICP_QAT_FW_COMN_PTR_TYPE_SET(req->comn_hdr.comn_req_flags,
			QAT_COMN_PTR_TYPE_SGL);
	cookie = qp->op_cookies[tx_queue->tail >> tx_queue->trailz];
	list = (struct qat_sgl *)&cookie->qat_sgl_src;

	for (i = 0; i < n_data_vecs; i++) {
		list->buffers[i].len = data[i].len;
		list->buffers[i].resrvd = 0;
		list->buffers[i].addr = data[i].iova;
		if (total_len + data[i].len > UINT32_MAX) {
			QAT_DP_LOG(ERR, "Message too long");
			return -1;
		}
		total_len += data[i].len;
	}

	list->num_bufs = i;
	req->comn_mid.src_data_addr = req->comn_mid.dest_data_addr =
			cookie->qat_sgl_src_phys_addr;
	req->comn_mid.src_length = req->comn_mid.dst_length = 0;
	return total_len;
}

static __rte_always_inline void
set_cipher_iv(struct icp_qat_fw_la_cipher_req_params *cipher_param,
		struct rte_crypto_va_iova_ptr *iv_ptr, uint32_t iv_len,
		struct icp_qat_fw_la_bulk_req *qat_req)
{
	/* copy IV into request if it fits */
	if (iv_len <= sizeof(cipher_param->u.cipher_IV_array))
		rte_memcpy(cipher_param->u.cipher_IV_array, iv_ptr->va,
				iv_len);
	else {
		ICP_QAT_FW_LA_CIPH_IV_FLD_FLAG_SET(
				qat_req->comn_hdr.serv_specif_flags,
				ICP_QAT_FW_CIPH_IV_64BIT_PTR);
		cipher_param->u.s.cipher_IV_ptr = iv_ptr->iova;
	}
}

#define QAT_SYM_DP_IS_RESP_SUCCESS(resp) \
	(ICP_QAT_FW_COMN_STATUS_FLAG_OK == \
	ICP_QAT_FW_COMN_RESP_CRYPTO_STAT_GET(resp->comn_hdr.comn_status))

static __rte_always_inline void
qat_sym_dp_fill_vec_status(int32_t *sta, int status, uint32_t n)
{
	uint32_t i;

	for (i = 0; i < n; i++)
		sta[i] = status;
}

#define QAT_SYM_DP_GET_MAX_ENQ(q, c, n) \
	RTE_MIN((q->max_inflights - q->enqueued + q->dequeued - c), n)

static __rte_always_inline void
enqueue_one_aead_job(struct qat_sym_session *ctx,
	struct icp_qat_fw_la_bulk_req *req,
	struct rte_crypto_va_iova_ptr *iv,
	struct rte_crypto_va_iova_ptr *digest,
	struct rte_crypto_va_iova_ptr *aad,
	union rte_crypto_sym_ofs ofs, uint32_t data_len)
{
	struct icp_qat_fw_la_cipher_req_params *cipher_param =
		(void *)&req->serv_specif_rqpars;
	struct icp_qat_fw_la_auth_req_params *auth_param =
		(void *)((uint8_t *)&req->serv_specif_rqpars +
		ICP_QAT_FW_HASH_REQUEST_PARAMETERS_OFFSET);
	uint8_t *aad_data;
	uint8_t aad_ccm_real_len;
	uint8_t aad_len_field_sz;
	uint32_t msg_len_be;
	rte_iova_t aad_iova = 0;
	uint8_t q;

	switch (ctx->qat_hash_alg) {
	case ICP_QAT_HW_AUTH_ALGO_GALOIS_128:
	case ICP_QAT_HW_AUTH_ALGO_GALOIS_64:
		ICP_QAT_FW_LA_GCM_IV_LEN_FLAG_SET(
			req->comn_hdr.serv_specif_flags,
				ICP_QAT_FW_LA_GCM_IV_LEN_12_OCTETS);
		rte_memcpy(cipher_param->u.cipher_IV_array, iv->va,
				ctx->cipher_iv.length);
		aad_iova = aad->iova;
		break;
	case ICP_QAT_HW_AUTH_ALGO_AES_CBC_MAC:
		aad_data = aad->va;
		aad_iova = aad->iova;
		aad_ccm_real_len = 0;
		aad_len_field_sz = 0;
		msg_len_be = rte_bswap32((uint32_t)data_len -
				ofs.ofs.cipher.head);

		if (ctx->aad_len > ICP_QAT_HW_CCM_AAD_DATA_OFFSET) {
			aad_len_field_sz = ICP_QAT_HW_CCM_AAD_LEN_INFO;
			aad_ccm_real_len = ctx->aad_len -
				ICP_QAT_HW_CCM_AAD_B0_LEN -
				ICP_QAT_HW_CCM_AAD_LEN_INFO;
		} else {
			aad_data = iv->va;
			aad_iova = iv->iova;
		}

		q = ICP_QAT_HW_CCM_NQ_CONST - ctx->cipher_iv.length;
		aad_data[0] = ICP_QAT_HW_CCM_BUILD_B0_FLAGS(
			aad_len_field_sz, ctx->digest_length, q);
		if (q > ICP_QAT_HW_CCM_MSG_LEN_MAX_FIELD_SIZE) {
			memcpy(aad_data	+ ctx->cipher_iv.length +
				ICP_QAT_HW_CCM_NONCE_OFFSET + (q -
				ICP_QAT_HW_CCM_MSG_LEN_MAX_FIELD_SIZE),
				(uint8_t *)&msg_len_be,
				ICP_QAT_HW_CCM_MSG_LEN_MAX_FIELD_SIZE);
		} else {
			memcpy(aad_data	+ ctx->cipher_iv.length +
				ICP_QAT_HW_CCM_NONCE_OFFSET,
				(uint8_t *)&msg_len_be +
				(ICP_QAT_HW_CCM_MSG_LEN_MAX_FIELD_SIZE
				- q), q);
		}

		if (aad_len_field_sz > 0) {
			*(uint16_t *)&aad_data[ICP_QAT_HW_CCM_AAD_B0_LEN] =
				rte_bswap16(aad_ccm_real_len);

			if ((aad_ccm_real_len + aad_len_field_sz)
				% ICP_QAT_HW_CCM_AAD_B0_LEN) {
				uint8_t pad_len = 0;
				uint8_t pad_idx = 0;

				pad_len = ICP_QAT_HW_CCM_AAD_B0_LEN -
					((aad_ccm_real_len +
					aad_len_field_sz) %
					ICP_QAT_HW_CCM_AAD_B0_LEN);
				pad_idx = ICP_QAT_HW_CCM_AAD_B0_LEN +
					aad_ccm_real_len +
					aad_len_field_sz;
				memset(&aad_data[pad_idx], 0, pad_len);
			}
		}

		rte_memcpy(((uint8_t *)cipher_param->u.cipher_IV_array)
			+ ICP_QAT_HW_CCM_NONCE_OFFSET,
			(uint8_t *)iv->va +
			ICP_QAT_HW_CCM_NONCE_OFFSET, ctx->cipher_iv.length);
		*(uint8_t *)&cipher_param->u.cipher_IV_array[0] =
			q - ICP_QAT_HW_CCM_NONCE_OFFSET;

		rte_memcpy((uint8_t *)aad->va +
				ICP_QAT_HW_CCM_NONCE_OFFSET,
			(uint8_t *)iv->va + ICP_QAT_HW_CCM_NONCE_OFFSET,
			ctx->cipher_iv.length);
		break;
	default:
		break;
	}

	cipher_param->cipher_offset = ofs.ofs.cipher.head;
	cipher_param->cipher_length = data_len - ofs.ofs.cipher.head -
			ofs.ofs.cipher.tail;
	auth_param->auth_off = ofs.ofs.cipher.head;
	auth_param->auth_len = cipher_param->cipher_length;
	auth_param->auth_res_addr = digest->iova;
	auth_param->u1.aad_adr = aad_iova;

	if (ctx->is_single_pass) {
		cipher_param->spc_aad_addr = aad_iova;
		cipher_param->spc_auth_res_addr = digest->iova;
	}
}

static __rte_always_inline int
qat_sym_dp_enqueue_single_aead(void *qp_data, uint8_t *drv_ctx,
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
	struct qat_sym_session *ctx = dp_ctx->session;
	struct icp_qat_fw_la_bulk_req *req;
	int32_t data_len;
	uint32_t tail = dp_ctx->tail;

	req = (struct icp_qat_fw_la_bulk_req *)(
		(uint8_t *)tx_queue->base_addr + tail);
	tail = (tail + tx_queue->msg_size) & tx_queue->modulo_mask;
	rte_mov128((uint8_t *)req, (const uint8_t *)&(ctx->fw_req));
	rte_prefetch0((uint8_t *)tx_queue->base_addr + tail);
	data_len = qat_sym_dp_parse_data_vec(qp, req, data, n_data_vecs);
	if (unlikely(data_len < 0))
		return -1;
	req->comn_mid.opaque_data = (uint64_t)(uintptr_t)user_data;

	enqueue_one_aead_job(ctx, req, iv, digest, aad, ofs,
		(uint32_t)data_len);

	dp_ctx->tail = tail;
	dp_ctx->cached_enqueue++;

	return 0;
}

static __rte_always_inline uint32_t
qat_sym_dp_enqueue_aead_jobs(void *qp_data, uint8_t *drv_ctx,
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
		req  = (struct icp_qat_fw_la_bulk_req *)(
			(uint8_t *)tx_queue->base_addr + tail);
		rte_mov128((uint8_t *)req, (const uint8_t *)&(ctx->fw_req));

		data_len = qat_sym_dp_parse_data_vec(qp, req, vec->sgl[i].vec,
			vec->sgl[i].num);
		if (unlikely(data_len < 0))
			break;
		req->comn_mid.opaque_data = (uint64_t)(uintptr_t)user_data[i];
		enqueue_one_aead_job(ctx, req, &vec->iv[i], &vec->digest[i],
			&vec->aad[i], ofs, (uint32_t)data_len);
		tail = (tail + tx_queue->msg_size) & tx_queue->modulo_mask;
	}

	if (unlikely(i < n))
		qat_sym_dp_fill_vec_status(vec->status + i, -1, n - i);

	dp_ctx->tail = tail;
	dp_ctx->cached_enqueue += i;
	*status = 0;
	return i;
}

static __rte_always_inline void
enqueue_one_cipher_job(struct qat_sym_session *ctx,
	struct icp_qat_fw_la_bulk_req *req,
	struct rte_crypto_va_iova_ptr *iv,
	union rte_crypto_sym_ofs ofs, uint32_t data_len)
{
	struct icp_qat_fw_la_cipher_req_params *cipher_param;

	cipher_param = (void *)&req->serv_specif_rqpars;

	/* cipher IV */
	set_cipher_iv(cipher_param, iv, ctx->cipher_iv.length, req);
	cipher_param->cipher_offset = ofs.ofs.cipher.head;
	cipher_param->cipher_length = data_len - ofs.ofs.cipher.head -
			ofs.ofs.cipher.tail;
}

static __rte_always_inline int
qat_sym_dp_enqueue_single_cipher(void *qp_data, uint8_t *drv_ctx,
	struct rte_crypto_vec *data, uint16_t n_data_vecs,
	union rte_crypto_sym_ofs ofs,
	struct rte_crypto_va_iova_ptr *iv,
	struct rte_crypto_va_iova_ptr *digest __rte_unused,
	struct rte_crypto_va_iova_ptr *aad __rte_unused,
	void *user_data)
{
	struct qat_qp *qp = qp_data;
	struct qat_sym_dp_ctx *dp_ctx = (void *)drv_ctx;
	struct qat_queue *tx_queue = &qp->tx_q;
	struct qat_sym_session *ctx = dp_ctx->session;
	struct icp_qat_fw_la_bulk_req *req;
	int32_t data_len;
	uint32_t tail = dp_ctx->tail;

	req = (struct icp_qat_fw_la_bulk_req *)(
		(uint8_t *)tx_queue->base_addr + tail);
	tail = (tail + tx_queue->msg_size) & tx_queue->modulo_mask;
	rte_mov128((uint8_t *)req, (const uint8_t *)&(ctx->fw_req));
	rte_prefetch0((uint8_t *)tx_queue->base_addr + tail);
	data_len = qat_sym_dp_parse_data_vec(qp, req, data, n_data_vecs);
	if (unlikely(data_len < 0))
		return -1;
	req->comn_mid.opaque_data = (uint64_t)(uintptr_t)user_data;

	enqueue_one_cipher_job(ctx, req, iv, ofs, (uint32_t)data_len);

	dp_ctx->tail = tail;
	dp_ctx->cached_enqueue++;

	return 0;
}

static __rte_always_inline uint32_t
qat_sym_dp_enqueue_cipher_jobs(void *qp_data, uint8_t *drv_ctx,
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
		req  = (struct icp_qat_fw_la_bulk_req *)(
			(uint8_t *)tx_queue->base_addr + tail);
		rte_mov128((uint8_t *)req, (const uint8_t *)&(ctx->fw_req));

		data_len = qat_sym_dp_parse_data_vec(qp, req, vec->sgl[i].vec,
			vec->sgl[i].num);
		if (unlikely(data_len < 0))
			break;
		req->comn_mid.opaque_data = (uint64_t)(uintptr_t)user_data[i];
		enqueue_one_cipher_job(ctx, req, &vec->iv[i], ofs,
			(uint32_t)data_len);
		tail = (tail + tx_queue->msg_size) & tx_queue->modulo_mask;
	}

	if (unlikely(i < n))
		qat_sym_dp_fill_vec_status(vec->status + i, -1, n - i);

	dp_ctx->tail = tail;
	dp_ctx->cached_enqueue += i;
	*status = 0;
	return i;
}

static __rte_always_inline void
enqueue_one_auth_job(struct qat_sym_session *ctx,
	struct icp_qat_fw_la_bulk_req *req,
	struct rte_crypto_va_iova_ptr *digest,
	struct rte_crypto_va_iova_ptr *auth_iv,
	union rte_crypto_sym_ofs ofs, uint32_t data_len)
{
	struct icp_qat_fw_la_cipher_req_params *cipher_param;
	struct icp_qat_fw_la_auth_req_params *auth_param;

	cipher_param = (void *)&req->serv_specif_rqpars;
	auth_param = (void *)((uint8_t *)cipher_param +
			ICP_QAT_FW_HASH_REQUEST_PARAMETERS_OFFSET);

	auth_param->auth_off = ofs.ofs.auth.head;
	auth_param->auth_len = data_len - ofs.ofs.auth.head -
			ofs.ofs.auth.tail;
	auth_param->auth_res_addr = digest->iova;

	switch (ctx->qat_hash_alg) {
	case ICP_QAT_HW_AUTH_ALGO_SNOW_3G_UIA2:
	case ICP_QAT_HW_AUTH_ALGO_KASUMI_F9:
	case ICP_QAT_HW_AUTH_ALGO_ZUC_3G_128_EIA3:
		auth_param->u1.aad_adr = auth_iv->iova;
		break;
	case ICP_QAT_HW_AUTH_ALGO_GALOIS_128:
	case ICP_QAT_HW_AUTH_ALGO_GALOIS_64:
		ICP_QAT_FW_LA_GCM_IV_LEN_FLAG_SET(
			req->comn_hdr.serv_specif_flags,
				ICP_QAT_FW_LA_GCM_IV_LEN_12_OCTETS);
		rte_memcpy(cipher_param->u.cipher_IV_array, auth_iv->va,
				ctx->auth_iv.length);
		break;
	default:
		break;
	}
}

static __rte_always_inline int
qat_sym_dp_enqueue_single_auth(void *qp_data, uint8_t *drv_ctx,
	struct rte_crypto_vec *data, uint16_t n_data_vecs,
	union rte_crypto_sym_ofs ofs,
	struct rte_crypto_va_iova_ptr *iv __rte_unused,
	struct rte_crypto_va_iova_ptr *digest,
	struct rte_crypto_va_iova_ptr *auth_iv,
	void *user_data)
{
	struct qat_qp *qp = qp_data;
	struct qat_sym_dp_ctx *dp_ctx = (void *)drv_ctx;
	struct qat_queue *tx_queue = &qp->tx_q;
	struct qat_sym_session *ctx = dp_ctx->session;
	struct icp_qat_fw_la_bulk_req *req;
	int32_t data_len;
	uint32_t tail = dp_ctx->tail;

	req = (struct icp_qat_fw_la_bulk_req *)(
		(uint8_t *)tx_queue->base_addr + tail);
	tail = (tail + tx_queue->msg_size) & tx_queue->modulo_mask;
	rte_mov128((uint8_t *)req, (const uint8_t *)&(ctx->fw_req));
	rte_prefetch0((uint8_t *)tx_queue->base_addr + tail);
	data_len = qat_sym_dp_parse_data_vec(qp, req, data, n_data_vecs);
	if (unlikely(data_len < 0))
		return -1;
	req->comn_mid.opaque_data = (uint64_t)(uintptr_t)user_data;

	enqueue_one_auth_job(ctx, req, digest, auth_iv, ofs,
			(uint32_t)data_len);

	dp_ctx->tail = tail;
	dp_ctx->cached_enqueue++;

	return 0;
}

static __rte_always_inline uint32_t
qat_sym_dp_enqueue_auth_jobs(void *qp_data, uint8_t *drv_ctx,
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
		req  = (struct icp_qat_fw_la_bulk_req *)(
			(uint8_t *)tx_queue->base_addr + tail);
		rte_mov128((uint8_t *)req, (const uint8_t *)&(ctx->fw_req));

		data_len = qat_sym_dp_parse_data_vec(qp, req, vec->sgl[i].vec,
			vec->sgl[i].num);
		if (unlikely(data_len < 0))
			break;
		req->comn_mid.opaque_data = (uint64_t)(uintptr_t)user_data[i];
		enqueue_one_auth_job(ctx, req, &vec->digest[i],
			&vec->auth_iv[i], ofs, (uint32_t)data_len);
		tail = (tail + tx_queue->msg_size) & tx_queue->modulo_mask;
	}

	if (unlikely(i < n))
		qat_sym_dp_fill_vec_status(vec->status + i, -1, n - i);

	dp_ctx->tail = tail;
	dp_ctx->cached_enqueue += i;
	*status = 0;
	return i;
}

static __rte_always_inline int
enqueue_one_chain_job(struct qat_sym_session *ctx,
	struct icp_qat_fw_la_bulk_req *req,
	struct rte_crypto_vec *data,
	uint16_t n_data_vecs,
	struct rte_crypto_va_iova_ptr *cipher_iv,
	struct rte_crypto_va_iova_ptr *digest,
	struct rte_crypto_va_iova_ptr *auth_iv,
	union rte_crypto_sym_ofs ofs, uint32_t data_len)
{
	struct icp_qat_fw_la_cipher_req_params *cipher_param;
	struct icp_qat_fw_la_auth_req_params *auth_param;
	rte_iova_t auth_iova_end;
	int32_t cipher_len, auth_len;

	cipher_param = (void *)&req->serv_specif_rqpars;
	auth_param = (void *)((uint8_t *)cipher_param +
			ICP_QAT_FW_HASH_REQUEST_PARAMETERS_OFFSET);

	cipher_len = data_len - ofs.ofs.cipher.head -
			ofs.ofs.cipher.tail;
	auth_len = data_len - ofs.ofs.auth.head - ofs.ofs.auth.tail;

	if (unlikely(cipher_len < 0 || auth_len < 0))
		return -1;

	cipher_param->cipher_offset = ofs.ofs.cipher.head;
	cipher_param->cipher_length = cipher_len;
	set_cipher_iv(cipher_param, cipher_iv, ctx->cipher_iv.length, req);

	auth_param->auth_off = ofs.ofs.auth.head;
	auth_param->auth_len = auth_len;
	auth_param->auth_res_addr = digest->iova;

	switch (ctx->qat_hash_alg) {
	case ICP_QAT_HW_AUTH_ALGO_SNOW_3G_UIA2:
	case ICP_QAT_HW_AUTH_ALGO_KASUMI_F9:
	case ICP_QAT_HW_AUTH_ALGO_ZUC_3G_128_EIA3:
		auth_param->u1.aad_adr = auth_iv->iova;
		break;
	case ICP_QAT_HW_AUTH_ALGO_GALOIS_128:
	case ICP_QAT_HW_AUTH_ALGO_GALOIS_64:
		break;
	default:
		break;
	}

	if (unlikely(n_data_vecs > 1)) {
		int auth_end_get = 0, i = n_data_vecs - 1;
		struct rte_crypto_vec *cvec = &data[0];
		uint32_t len;

		len = data_len - ofs.ofs.auth.tail;

		while (i >= 0 && len > 0) {
			if (cvec->len >= len) {
				auth_iova_end = cvec->iova + len;
				len = 0;
				auth_end_get = 1;
				break;
			}
			len -= cvec->len;
			i--;
			cvec++;
		}

		if (unlikely(auth_end_get == 0))
			return -1;
	} else
		auth_iova_end = data[0].iova + auth_param->auth_off +
			auth_param->auth_len;

	/* Then check if digest-encrypted conditions are met */
	if ((auth_param->auth_off + auth_param->auth_len <
		cipher_param->cipher_offset +
		cipher_param->cipher_length) &&
		(digest->iova == auth_iova_end)) {
		/* Handle partial digest encryption */
		if (cipher_param->cipher_offset +
				cipher_param->cipher_length <
				auth_param->auth_off +
				auth_param->auth_len +
				ctx->digest_length)
			req->comn_mid.dst_length =
				req->comn_mid.src_length =
				auth_param->auth_off +
				auth_param->auth_len +
				ctx->digest_length;
		struct icp_qat_fw_comn_req_hdr *header =
			&req->comn_hdr;
		ICP_QAT_FW_LA_DIGEST_IN_BUFFER_SET(
			header->serv_specif_flags,
			ICP_QAT_FW_LA_DIGEST_IN_BUFFER);
	}

	return 0;
}

static __rte_always_inline int
qat_sym_dp_enqueue_single_chain(void *qp_data, uint8_t *drv_ctx,
	struct rte_crypto_vec *data, uint16_t n_data_vecs,
	union rte_crypto_sym_ofs ofs,
	struct rte_crypto_va_iova_ptr *cipher_iv,
	struct rte_crypto_va_iova_ptr *digest,
	struct rte_crypto_va_iova_ptr *auth_iv,
	void *user_data)
{
	struct qat_qp *qp = qp_data;
	struct qat_sym_dp_ctx *dp_ctx = (void *)drv_ctx;
	struct qat_queue *tx_queue = &qp->tx_q;
	struct qat_sym_session *ctx = dp_ctx->session;
	struct icp_qat_fw_la_bulk_req *req;
	int32_t data_len;
	uint32_t tail = dp_ctx->tail;

	req = (struct icp_qat_fw_la_bulk_req *)(
		(uint8_t *)tx_queue->base_addr + tail);
	tail = (tail + tx_queue->msg_size) & tx_queue->modulo_mask;
	rte_mov128((uint8_t *)req, (const uint8_t *)&(ctx->fw_req));
	rte_prefetch0((uint8_t *)tx_queue->base_addr + tail);
	data_len = qat_sym_dp_parse_data_vec(qp, req, data, n_data_vecs);
	if (unlikely(data_len < 0))
		return -1;
	req->comn_mid.opaque_data = (uint64_t)(uintptr_t)user_data;

	if (unlikely(enqueue_one_chain_job(ctx, req, data, n_data_vecs,
			cipher_iv, digest, auth_iv, ofs, (uint32_t)data_len)))
		return -1;

	dp_ctx->tail = tail;
	dp_ctx->cached_enqueue++;

	return 0;
}

static __rte_always_inline uint32_t
qat_sym_dp_enqueue_chain_jobs(void *qp_data, uint8_t *drv_ctx,
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
		req  = (struct icp_qat_fw_la_bulk_req *)(
			(uint8_t *)tx_queue->base_addr + tail);
		rte_mov128((uint8_t *)req, (const uint8_t *)&(ctx->fw_req));

		data_len = qat_sym_dp_parse_data_vec(qp, req, vec->sgl[i].vec,
			vec->sgl[i].num);
		if (unlikely(data_len < 0))
			break;
		req->comn_mid.opaque_data = (uint64_t)(uintptr_t)user_data[i];
		if (unlikely(enqueue_one_chain_job(ctx, req, vec->sgl[i].vec,
			vec->sgl[i].num, &vec->iv[i], &vec->digest[i],
				&vec->auth_iv[i], ofs, (uint32_t)data_len)))
			break;

		tail = (tail + tx_queue->msg_size) & tx_queue->modulo_mask;
	}

	if (unlikely(i < n))
		qat_sym_dp_fill_vec_status(vec->status + i, -1, n - i);

	dp_ctx->tail = tail;
	dp_ctx->cached_enqueue += i;
	*status = 0;
	return i;
}

static __rte_always_inline uint32_t
qat_sym_dp_dequeue_burst(void *qp_data, uint8_t *drv_ctx,
	rte_cryptodev_raw_get_dequeue_count_t get_dequeue_count,
	rte_cryptodev_raw_post_dequeue_t post_dequeue,
	void **out_user_data, uint8_t is_user_data_array,
	uint32_t *n_success_jobs, int *return_status)
{
	struct qat_qp *qp = qp_data;
	struct qat_sym_dp_ctx *dp_ctx = (void *)drv_ctx;
	struct qat_queue *rx_queue = &qp->rx_q;
	struct icp_qat_fw_comn_resp *resp;
	void *resp_opaque;
	uint32_t i, n, inflight;
	uint32_t head;
	uint8_t status;

	*n_success_jobs = 0;
	*return_status = 0;
	head = dp_ctx->head;

	inflight = qp->enqueued - qp->dequeued;
	if (unlikely(inflight == 0))
		return 0;

	resp = (struct icp_qat_fw_comn_resp *)((uint8_t *)rx_queue->base_addr +
			head);
	/* no operation ready */
	if (unlikely(*(uint32_t *)resp == ADF_RING_EMPTY_SIG))
		return 0;

	resp_opaque = (void *)(uintptr_t)resp->opaque_data;
	/* get the dequeue count */
	n = get_dequeue_count(resp_opaque);
	if (unlikely(n == 0))
		return 0;

	out_user_data[0] = resp_opaque;
	status = QAT_SYM_DP_IS_RESP_SUCCESS(resp);
	post_dequeue(resp_opaque, 0, status);
	*n_success_jobs += status;

	head = (head + rx_queue->msg_size) & rx_queue->modulo_mask;

	/* we already finished dequeue when n == 1 */
	if (unlikely(n == 1)) {
		i = 1;
		goto end_deq;
	}

	if (is_user_data_array) {
		for (i = 1; i < n; i++) {
			resp = (struct icp_qat_fw_comn_resp *)(
				(uint8_t *)rx_queue->base_addr + head);
			if (unlikely(*(uint32_t *)resp ==
					ADF_RING_EMPTY_SIG))
				goto end_deq;
			out_user_data[i] = (void *)(uintptr_t)resp->opaque_data;
			status = QAT_SYM_DP_IS_RESP_SUCCESS(resp);
			*n_success_jobs += status;
			post_dequeue(out_user_data[i], i, status);
			head = (head + rx_queue->msg_size) &
					rx_queue->modulo_mask;
		}

		goto end_deq;
	}

	/* opaque is not array */
	for (i = 1; i < n; i++) {
		resp = (struct icp_qat_fw_comn_resp *)(
			(uint8_t *)rx_queue->base_addr + head);
		status = QAT_SYM_DP_IS_RESP_SUCCESS(resp);
		if (unlikely(*(uint32_t *)resp == ADF_RING_EMPTY_SIG))
			goto end_deq;
		head = (head + rx_queue->msg_size) &
				rx_queue->modulo_mask;
		post_dequeue(resp_opaque, i, status);
		*n_success_jobs += status;
	}

end_deq:
	dp_ctx->head = head;
	dp_ctx->cached_dequeue += i;
	return i;
}

static __rte_always_inline void *
qat_sym_dp_dequeue(void *qp_data, uint8_t *drv_ctx, int *dequeue_status,
		enum rte_crypto_op_status *op_status)
{
	struct qat_qp *qp = qp_data;
	struct qat_sym_dp_ctx *dp_ctx = (void *)drv_ctx;
	struct qat_queue *rx_queue = &qp->rx_q;
	register struct icp_qat_fw_comn_resp *resp;

	resp = (struct icp_qat_fw_comn_resp *)((uint8_t *)rx_queue->base_addr +
			dp_ctx->head);

	if (unlikely(*(uint32_t *)resp == ADF_RING_EMPTY_SIG))
		return NULL;

	dp_ctx->head = (dp_ctx->head + rx_queue->msg_size) &
			rx_queue->modulo_mask;
	dp_ctx->cached_dequeue++;

	*op_status = QAT_SYM_DP_IS_RESP_SUCCESS(resp) ?
			RTE_CRYPTO_OP_STATUS_SUCCESS :
			RTE_CRYPTO_OP_STATUS_AUTH_FAILED;
	*dequeue_status = 0;
	return (void *)(uintptr_t)resp->opaque_data;
}

static __rte_always_inline int
qat_sym_dp_kick_tail(void *qp_data, uint8_t *drv_ctx, uint32_t n)
{
	struct qat_qp *qp = qp_data;
	struct qat_queue *tx_queue = &qp->tx_q;
	struct qat_sym_dp_ctx *dp_ctx = (void *)drv_ctx;

	if (unlikely(dp_ctx->cached_enqueue != n))
		return -1;

	qp->enqueued += n;
	qp->stats.enqueued_count += n;

	tx_queue->tail = dp_ctx->tail;

	WRITE_CSR_RING_TAIL(qp->mmap_bar_addr,
			tx_queue->hw_bundle_number,
			tx_queue->hw_queue_number, tx_queue->tail);
	tx_queue->csr_tail = tx_queue->tail;
	dp_ctx->cached_enqueue = 0;

	return 0;
}

static __rte_always_inline int
qat_sym_dp_update_head(void *qp_data, uint8_t *drv_ctx, uint32_t n)
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
		WRITE_CSR_RING_HEAD(qp->mmap_bar_addr,
			rx_queue->hw_bundle_number, rx_queue->hw_queue_number,
			new_head);
	}

	dp_ctx->cached_dequeue = 0;
	return 0;
}

int
qat_sym_configure_dp_ctx(struct rte_cryptodev *dev, uint16_t qp_id,
	struct rte_crypto_raw_dp_ctx *raw_dp_ctx,
	enum rte_crypto_op_sess_type sess_type,
	union rte_cryptodev_session_ctx session_ctx, uint8_t is_update)
{
	struct qat_qp *qp;
	struct qat_sym_session *ctx;
	struct qat_sym_dp_ctx *dp_ctx;

	qp = dev->data->queue_pairs[qp_id];
	dp_ctx = (struct qat_sym_dp_ctx *)raw_dp_ctx->drv_ctx_data;

	if (!is_update) {
		memset(raw_dp_ctx, 0, sizeof(*raw_dp_ctx) +
				sizeof(struct qat_sym_dp_ctx));
		raw_dp_ctx->qp_data = dev->data->queue_pairs[qp_id];
		dp_ctx->tail = qp->tx_q.tail;
		dp_ctx->head = qp->rx_q.head;
		dp_ctx->cached_enqueue = dp_ctx->cached_dequeue = 0;
	}

	if (sess_type != RTE_CRYPTO_OP_WITH_SESSION)
		return -EINVAL;

	ctx = (struct qat_sym_session *)get_sym_session_private_data(
			session_ctx.crypto_sess, qat_sym_driver_id);

	dp_ctx->session = ctx;

	raw_dp_ctx->enqueue_done = qat_sym_dp_kick_tail;
	raw_dp_ctx->dequeue_burst = qat_sym_dp_dequeue_burst;
	raw_dp_ctx->dequeue = qat_sym_dp_dequeue;
	raw_dp_ctx->dequeue_done = qat_sym_dp_update_head;

	if (ctx->qat_cmd == ICP_QAT_FW_LA_CMD_HASH_CIPHER ||
			ctx->qat_cmd == ICP_QAT_FW_LA_CMD_CIPHER_HASH) {
		/* AES-GCM or AES-CCM */
		if (ctx->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_GALOIS_128 ||
			ctx->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_GALOIS_64 ||
			(ctx->qat_cipher_alg == ICP_QAT_HW_CIPHER_ALGO_AES128
			&& ctx->qat_mode == ICP_QAT_HW_CIPHER_CTR_MODE
			&& ctx->qat_hash_alg ==
					ICP_QAT_HW_AUTH_ALGO_AES_CBC_MAC)) {
			raw_dp_ctx->enqueue_burst =
					qat_sym_dp_enqueue_aead_jobs;
			raw_dp_ctx->enqueue = qat_sym_dp_enqueue_single_aead;
		} else {
			raw_dp_ctx->enqueue_burst =
					qat_sym_dp_enqueue_chain_jobs;
			raw_dp_ctx->enqueue = qat_sym_dp_enqueue_single_chain;
		}
	} else if (ctx->qat_cmd == ICP_QAT_FW_LA_CMD_AUTH) {
		raw_dp_ctx->enqueue_burst = qat_sym_dp_enqueue_auth_jobs;
		raw_dp_ctx->enqueue = qat_sym_dp_enqueue_single_auth;
	} else if (ctx->qat_cmd == ICP_QAT_FW_LA_CMD_CIPHER) {
		raw_dp_ctx->enqueue_burst = qat_sym_dp_enqueue_cipher_jobs;
		raw_dp_ctx->enqueue = qat_sym_dp_enqueue_single_cipher;
	} else
		return -1;

	return 0;
}

int
qat_sym_get_dp_ctx_size(__rte_unused struct rte_cryptodev *dev)
{
	return sizeof(struct qat_sym_dp_ctx);
}
