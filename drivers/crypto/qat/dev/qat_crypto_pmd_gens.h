/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2022 Intel Corporation
 */

#ifndef _QAT_CRYPTO_PMD_GENS_H_
#define _QAT_CRYPTO_PMD_GENS_H_

#include <rte_cryptodev.h>
#include "qat_crypto.h"
#include "qat_sym_session.h"
#include "qat_sym.h"

#define QAT_SYM_DP_GET_MAX_ENQ(q, c, n) \
	RTE_MIN((q->max_inflights - q->enqueued + q->dequeued - c), n)

#define QAT_SYM_DP_IS_RESP_SUCCESS(resp) \
	(ICP_QAT_FW_COMN_STATUS_FLAG_OK == \
	ICP_QAT_FW_COMN_RESP_CRYPTO_STAT_GET(resp->comn_hdr.comn_status))

static __rte_always_inline int
op_bpi_cipher_decrypt(uint8_t *src, uint8_t *dst,
		uint8_t *iv, int ivlen, int srclen,
		void *bpi_ctx)
{
	EVP_CIPHER_CTX *ctx = (EVP_CIPHER_CTX *)bpi_ctx;
	int encrypted_ivlen;
	uint8_t encrypted_iv[BPI_MAX_ENCR_IV_LEN];
	uint8_t *encr = encrypted_iv;

	/* ECB method: encrypt (not decrypt!) the IV, then XOR with plaintext */
	if (EVP_EncryptUpdate(ctx, encrypted_iv, &encrypted_ivlen, iv, ivlen)
								<= 0)
		goto cipher_decrypt_err;

	for (; srclen != 0; --srclen, ++dst, ++src, ++encr)
		*dst = *src ^ *encr;

	return 0;

cipher_decrypt_err:
	QAT_DP_LOG(ERR, "libcrypto ECB cipher decrypt for BPI IV failed");
	return -EINVAL;
}

static __rte_always_inline uint32_t
qat_bpicipher_preprocess(struct qat_sym_session *ctx,
				struct rte_crypto_op *op)
{
	int block_len = qat_cipher_get_block_size(ctx->qat_cipher_alg);
	struct rte_crypto_sym_op *sym_op = op->sym;
	uint8_t last_block_len = block_len > 0 ?
			sym_op->cipher.data.length % block_len : 0;

	if (last_block_len && ctx->qat_dir == ICP_QAT_HW_CIPHER_DECRYPT) {
		/* Decrypt last block */
		uint8_t *last_block, *dst, *iv;
		uint32_t last_block_offset = sym_op->cipher.data.offset +
				sym_op->cipher.data.length - last_block_len;
		last_block = (uint8_t *) rte_pktmbuf_mtod_offset(sym_op->m_src,
				uint8_t *, last_block_offset);

		if (unlikely((sym_op->m_dst != NULL)
				&& (sym_op->m_dst != sym_op->m_src)))
			/* out-of-place operation (OOP) */
			dst = (uint8_t *) rte_pktmbuf_mtod_offset(sym_op->m_dst,
						uint8_t *, last_block_offset);
		else
			dst = last_block;

		if (last_block_len < sym_op->cipher.data.length)
			/* use previous block ciphertext as IV */
			iv = last_block - block_len;
		else
			/* runt block, i.e. less than one full block */
			iv = rte_crypto_op_ctod_offset(op, uint8_t *,
					ctx->cipher_iv.offset);

#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
		QAT_DP_HEXDUMP_LOG(DEBUG, "BPI: src before pre-process:",
			last_block, last_block_len);
		if (sym_op->m_dst != NULL)
			QAT_DP_HEXDUMP_LOG(DEBUG, "BPI: dst before pre-process:",
			dst, last_block_len);
#endif
#ifdef RTE_QAT_OPENSSL
		op_bpi_cipher_decrypt(last_block, dst, iv, block_len,
				last_block_len, ctx->bpi_ctx);
#else
		bpi_cipher_ipsec(last_block, dst, iv, last_block_len, ctx->expkey,
			ctx->mb_mgr, ctx->docsis_key_len);
#endif
#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
		QAT_DP_HEXDUMP_LOG(DEBUG, "BPI: src after pre-process:",
			last_block, last_block_len);
		if (sym_op->m_dst != NULL)
			QAT_DP_HEXDUMP_LOG(DEBUG, "BPI: dst after pre-process:",
			dst, last_block_len);
#endif
	}

	return sym_op->cipher.data.length - last_block_len;
}

static __rte_always_inline int
qat_auth_is_len_in_bits(struct qat_sym_session *ctx,
		struct rte_crypto_op *op)
{
	if (ctx->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_SNOW_3G_UIA2 ||
		ctx->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_KASUMI_F9 ||
		ctx->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_ZUC_3G_128_EIA3) {
		if (unlikely((op->sym->auth.data.offset % BYTE_LENGTH != 0) ||
				(op->sym->auth.data.length % BYTE_LENGTH != 0)))
			return -EINVAL;
		return 1;
	}
	return 0;
}

static __rte_always_inline int
qat_cipher_is_len_in_bits(struct qat_sym_session *ctx,
		struct rte_crypto_op *op)
{
	if (ctx->qat_cipher_alg == ICP_QAT_HW_CIPHER_ALGO_SNOW_3G_UEA2 ||
		ctx->qat_cipher_alg == ICP_QAT_HW_CIPHER_ALGO_KASUMI ||
		ctx->qat_cipher_alg == ICP_QAT_HW_CIPHER_ALGO_ZUC_3G_128_EEA3) {
		if (unlikely((op->sym->cipher.data.length % BYTE_LENGTH != 0) ||
			((op->sym->cipher.data.offset %
			BYTE_LENGTH) != 0)))
			return -EINVAL;
		return 1;
	}
	return 0;
}

static __rte_always_inline int32_t
qat_sym_build_req_set_data(struct icp_qat_fw_la_bulk_req *req,
		void *opaque, struct qat_sym_op_cookie *cookie,
		struct rte_crypto_vec *src_vec, uint16_t n_src,
		struct rte_crypto_vec *dst_vec, uint16_t n_dst)
{
	struct qat_sgl *list;
	uint32_t i;
	uint32_t tl_src = 0, total_len_src, total_len_dst;
	uint64_t src_data_start = 0, dst_data_start = 0;
	int is_sgl = n_src > 1 || n_dst > 1;

	if (unlikely(n_src < 1 || n_src > QAT_SYM_SGL_MAX_NUMBER ||
			n_dst > QAT_SYM_SGL_MAX_NUMBER))
		return -1;

	if (likely(!is_sgl)) {
		src_data_start = src_vec[0].iova;
		tl_src = total_len_src =
				src_vec[0].len;
		if (unlikely(n_dst)) { /* oop */
			total_len_dst = dst_vec[0].len;

			dst_data_start = dst_vec[0].iova;
			if (unlikely(total_len_src != total_len_dst))
				return -EINVAL;
		} else {
			dst_data_start = src_data_start;
			total_len_dst = tl_src;
		}
	} else { /* sgl */
		total_len_dst = total_len_src = 0;

		ICP_QAT_FW_COMN_PTR_TYPE_SET(req->comn_hdr.comn_req_flags,
			QAT_COMN_PTR_TYPE_SGL);

		list = (struct qat_sgl *)&cookie->qat_sgl_src;
		for (i = 0; i < n_src; i++) {
			list->buffers[i].len = src_vec[i].len;
			list->buffers[i].resrvd = 0;
			list->buffers[i].addr = src_vec[i].iova;
			if (tl_src + src_vec[i].len > UINT32_MAX) {
				QAT_DP_LOG(ERR, "Message too long");
				return -1;
			}
			tl_src += src_vec[i].len;
		}

		list->num_bufs = i;
		src_data_start = cookie->qat_sgl_src_phys_addr;

		if (unlikely(n_dst > 0)) { /* oop sgl */
			uint32_t tl_dst = 0;

			list = (struct qat_sgl *)&cookie->qat_sgl_dst;

			for (i = 0; i < n_dst; i++) {
				list->buffers[i].len = dst_vec[i].len;
				list->buffers[i].resrvd = 0;
				list->buffers[i].addr = dst_vec[i].iova;
				if (tl_dst + dst_vec[i].len > UINT32_MAX) {
					QAT_DP_LOG(ERR, "Message too long");
					return -ENOTSUP;
				}

				tl_dst += dst_vec[i].len;
			}

			if (tl_src != tl_dst)
				return -EINVAL;
			list->num_bufs = i;
			dst_data_start = cookie->qat_sgl_dst_phys_addr;
		} else
			dst_data_start = src_data_start;
	}

	req->comn_mid.src_data_addr = src_data_start;
	req->comn_mid.dest_data_addr = dst_data_start;
	req->comn_mid.src_length = total_len_src;
	req->comn_mid.dst_length = total_len_dst;
	req->comn_mid.opaque_data = (uintptr_t)opaque;

	return tl_src;
}

static __rte_always_inline uint64_t
qat_sym_convert_op_to_vec_cipher(struct rte_crypto_op *op,
		struct qat_sym_session *ctx,
		struct rte_crypto_sgl *in_sgl, struct rte_crypto_sgl *out_sgl,
		struct rte_crypto_va_iova_ptr *cipher_iv,
		struct rte_crypto_va_iova_ptr *auth_iv_or_aad __rte_unused,
		struct rte_crypto_va_iova_ptr *digest __rte_unused)
{
	uint32_t cipher_len = 0, cipher_ofs = 0;
	int n_src = 0;
	int ret;

	ret = qat_cipher_is_len_in_bits(ctx, op);
	switch (ret) {
	case 1:
		cipher_len = op->sym->cipher.data.length >> 3;
		cipher_ofs = op->sym->cipher.data.offset >> 3;
		break;
	case 0:

#ifdef RTE_QAT_OPENSSL
		if (ctx->bpi_ctx) {
#else
		if (ctx->mb_mgr) {
#endif
			/* DOCSIS - only send complete blocks to device.
			 * Process any partial block using CFB mode.
			 * Even if 0 complete blocks, still send this to device
			 * to get into rx queue for post-process and dequeuing
			 */
			cipher_len = qat_bpicipher_preprocess(ctx, op);
			cipher_ofs = op->sym->cipher.data.offset;
		} else {
			cipher_len = op->sym->cipher.data.length;
			cipher_ofs = op->sym->cipher.data.offset;
		}
		break;
	default:
		QAT_DP_LOG(ERR,
	  "SNOW3G/KASUMI/ZUC in QAT PMD only supports byte aligned values");
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		return UINT64_MAX;
	}

	cipher_iv->va = rte_crypto_op_ctod_offset(op, void *,
			ctx->cipher_iv.offset);
	cipher_iv->iova = rte_crypto_op_ctophys_offset(op,
			ctx->cipher_iv.offset);

	n_src = rte_crypto_mbuf_to_vec(op->sym->m_src, cipher_ofs,
			cipher_len, in_sgl->vec, QAT_SYM_SGL_MAX_NUMBER);
	if (n_src < 0 || n_src > op->sym->m_src->nb_segs) {
		op->status = RTE_CRYPTO_OP_STATUS_ERROR;
		return UINT64_MAX;
	}

	in_sgl->num = n_src;

	/* Out-Of-Place operation */
	if (unlikely((op->sym->m_dst != NULL) &&
			(op->sym->m_dst != op->sym->m_src))) {
		int n_dst = rte_crypto_mbuf_to_vec(op->sym->m_dst, cipher_ofs,
				cipher_len, out_sgl->vec,
				QAT_SYM_SGL_MAX_NUMBER);

		if ((n_dst < 0) || (n_dst > op->sym->m_dst->nb_segs)) {
			op->status = RTE_CRYPTO_OP_STATUS_ERROR;
			return UINT64_MAX;
		}

		out_sgl->num = n_dst;
	} else
		out_sgl->num = 0;

	return 0;
}

static __rte_always_inline uint64_t
qat_sym_convert_op_to_vec_auth(struct rte_crypto_op *op,
		struct qat_sym_session *ctx,
		struct rte_crypto_sgl *in_sgl, struct rte_crypto_sgl *out_sgl,
		struct rte_crypto_va_iova_ptr *cipher_iv __rte_unused,
		struct rte_crypto_va_iova_ptr *auth_iv,
		struct rte_crypto_va_iova_ptr *digest)
{
	uint32_t auth_ofs = 0, auth_len = 0;
	int n_src, ret;

	ret = qat_auth_is_len_in_bits(ctx, op);
	switch (ret) {
	case 1:
		auth_ofs = op->sym->auth.data.offset >> 3;
		auth_len = op->sym->auth.data.length >> 3;
		auth_iv->va = rte_crypto_op_ctod_offset(op, void *,
				ctx->auth_iv.offset);
		auth_iv->iova = rte_crypto_op_ctophys_offset(op,
				ctx->auth_iv.offset);
		break;
	case 0:
		if (ctx->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_GALOIS_128 ||
			ctx->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_GALOIS_64) {
			/* AES-GMAC */
			auth_ofs = op->sym->auth.data.offset;
			auth_len = op->sym->auth.data.length;
			auth_iv->va = rte_crypto_op_ctod_offset(op, void *,
					ctx->auth_iv.offset);
			auth_iv->iova = rte_crypto_op_ctophys_offset(op,
					ctx->auth_iv.offset);
		} else {
			auth_ofs = op->sym->auth.data.offset;
			auth_len = op->sym->auth.data.length;
			auth_iv->va = NULL;
			auth_iv->iova = 0;
		}
		break;
	default:
		QAT_DP_LOG(ERR,
	"For SNOW3G/KASUMI/ZUC, QAT PMD only supports byte aligned values");
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		return UINT64_MAX;
	}

	n_src = rte_crypto_mbuf_to_vec(op->sym->m_src, auth_ofs,
			auth_len, in_sgl->vec,
			QAT_SYM_SGL_MAX_NUMBER);
	if (n_src < 0 || n_src > op->sym->m_src->nb_segs) {
		op->status = RTE_CRYPTO_OP_STATUS_ERROR;
		return UINT64_MAX;
	}

	in_sgl->num = n_src;

	/* Out-Of-Place operation */
	if (unlikely((op->sym->m_dst != NULL) &&
			(op->sym->m_dst != op->sym->m_src))) {
		int n_dst = rte_crypto_mbuf_to_vec(op->sym->m_dst, auth_ofs,
				auth_len, out_sgl->vec,
				QAT_SYM_SGL_MAX_NUMBER);

		if ((n_dst < 0) || (n_dst > op->sym->m_dst->nb_segs)) {
			op->status = RTE_CRYPTO_OP_STATUS_ERROR;
			return UINT64_MAX;
		}
		out_sgl->num = n_dst;
	} else
		out_sgl->num = 0;

	digest->va = (void *)op->sym->auth.digest.data;
	digest->iova = op->sym->auth.digest.phys_addr;

	return 0;
}

static __rte_always_inline uint64_t
qat_sym_convert_op_to_vec_chain(struct rte_crypto_op *op,
		struct qat_sym_session *ctx,
		struct rte_crypto_sgl *in_sgl, struct rte_crypto_sgl *out_sgl,
		struct rte_crypto_va_iova_ptr *cipher_iv,
		struct rte_crypto_va_iova_ptr *auth_iv_or_aad,
		struct rte_crypto_va_iova_ptr *digest)
{
	union rte_crypto_sym_ofs ofs;
	uint32_t max_len = 0;
	uint32_t cipher_len = 0, cipher_ofs = 0;
	uint32_t auth_len = 0, auth_ofs = 0;
	int is_oop = (op->sym->m_dst != NULL) &&
			(op->sym->m_dst != op->sym->m_src);
	int is_sgl = op->sym->m_src->nb_segs > 1;
	int is_bpi = 0;
	int n_src;
	int ret;

	if (unlikely(is_oop))
		is_sgl |= op->sym->m_dst->nb_segs > 1;

	cipher_iv->va = rte_crypto_op_ctod_offset(op, void *,
			ctx->cipher_iv.offset);
	cipher_iv->iova = rte_crypto_op_ctophys_offset(op,
			ctx->cipher_iv.offset);
	auth_iv_or_aad->va = rte_crypto_op_ctod_offset(op, void *,
			ctx->auth_iv.offset);
	auth_iv_or_aad->iova = rte_crypto_op_ctophys_offset(op,
			ctx->auth_iv.offset);
	digest->va = (void *)op->sym->auth.digest.data;
	digest->iova = op->sym->auth.digest.phys_addr;

	ret = qat_cipher_is_len_in_bits(ctx, op);
	switch (ret) {
	case 1:
		cipher_len = op->sym->cipher.data.length >> 3;
		cipher_ofs = op->sym->cipher.data.offset >> 3;
		break;
	case 0:
#ifdef RTE_QAT_OPENSSL
		if (ctx->bpi_ctx) {
#else
		if (ctx->mb_mgr) {
#endif
			cipher_len = qat_bpicipher_preprocess(ctx, op);
			cipher_ofs = op->sym->cipher.data.offset;
			is_bpi = 1;
		} else {
			cipher_len = op->sym->cipher.data.length;
			cipher_ofs = op->sym->cipher.data.offset;
		}
		break;
	default:
		QAT_DP_LOG(ERR,
	"For SNOW3G/KASUMI/ZUC, QAT PMD only supports byte aligned values");
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		return -EINVAL;
	}

	ret = qat_auth_is_len_in_bits(ctx, op);
	switch (ret) {
	case 1:
		auth_len = op->sym->auth.data.length >> 3;
		auth_ofs = op->sym->auth.data.offset >> 3;
		break;
	case 0:
		auth_len = op->sym->auth.data.length;
		auth_ofs = op->sym->auth.data.offset;
		break;
	default:
		QAT_DP_LOG(ERR,
	"For SNOW3G/KASUMI/ZUC, QAT PMD only supports byte aligned values");
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		return -EINVAL;
	}

	max_len = RTE_MAX(cipher_ofs + cipher_len, auth_ofs + auth_len);

	/* digest in buffer check. Needed only for wireless algos
	 * or combined cipher-crc operations
	 */
	if (ret == 1 || is_bpi) {
		/* Handle digest-encrypted cases, i.e.
		 * auth-gen-then-cipher-encrypt and
		 * cipher-decrypt-then-auth-verify
		 */
		uint64_t auth_end_iova;

		if (unlikely(is_sgl)) {
			uint32_t remaining_off = auth_ofs + auth_len;
			struct rte_mbuf *sgl_buf = (is_oop ? op->sym->m_dst :
				op->sym->m_src);

			while (remaining_off >= rte_pktmbuf_data_len(sgl_buf)
					&& sgl_buf->next != NULL) {
				remaining_off -= rte_pktmbuf_data_len(sgl_buf);
				sgl_buf = sgl_buf->next;
			}

			auth_end_iova = (uint64_t)rte_pktmbuf_iova_offset(
				sgl_buf, remaining_off);
		} else
			auth_end_iova = (is_oop ?
				rte_pktmbuf_iova(op->sym->m_dst) :
				rte_pktmbuf_iova(op->sym->m_src)) + auth_ofs +
					auth_len;

		/* Then check if digest-encrypted conditions are met */
		if (((auth_ofs + auth_len < cipher_ofs + cipher_len) &&
				(digest->iova == auth_end_iova)) ||
#ifdef RTE_QAT_OPENSSL
				ctx->bpi_ctx)
#else
				ctx->mb_mgr)
#endif
			max_len = RTE_MAX(max_len, auth_ofs + auth_len +
					ctx->digest_length);
	}

	/* Passing 0 as cipher & auth offsets are assigned into ofs later */
	n_src = rte_crypto_mbuf_to_vec(op->sym->m_src, 0, max_len,
			in_sgl->vec, QAT_SYM_SGL_MAX_NUMBER);
	if (unlikely(n_src < 0 || n_src > op->sym->m_src->nb_segs)) {
		op->status = RTE_CRYPTO_OP_STATUS_ERROR;
		return -1;
	}
	in_sgl->num = n_src;

	if (unlikely((op->sym->m_dst != NULL) &&
			(op->sym->m_dst != op->sym->m_src))) {
		int n_dst = rte_crypto_mbuf_to_vec(op->sym->m_dst, 0,
				max_len, out_sgl->vec, QAT_SYM_SGL_MAX_NUMBER);

		if (n_dst < 0 || n_dst > op->sym->m_dst->nb_segs) {
			op->status = RTE_CRYPTO_OP_STATUS_ERROR;
			return -1;
		}
		out_sgl->num = n_dst;
	} else
		out_sgl->num = 0;

	ofs.ofs.cipher.head = cipher_ofs;
	ofs.ofs.cipher.tail = max_len - cipher_ofs - cipher_len;
	ofs.ofs.auth.head = auth_ofs;
	ofs.ofs.auth.tail = max_len - auth_ofs - auth_len;

	return ofs.raw;
}

static __rte_always_inline uint64_t
qat_sym_convert_op_to_vec_aead(struct rte_crypto_op *op,
		struct qat_sym_session *ctx,
		struct rte_crypto_sgl *in_sgl, struct rte_crypto_sgl *out_sgl,
		struct rte_crypto_va_iova_ptr *cipher_iv,
		struct rte_crypto_va_iova_ptr *auth_iv_or_aad,
		struct rte_crypto_va_iova_ptr *digest)
{
	uint32_t cipher_len = 0, cipher_ofs = 0;
	int32_t n_src = 0;

	cipher_iv->va = rte_crypto_op_ctod_offset(op, void *,
			ctx->cipher_iv.offset);
	cipher_iv->iova = rte_crypto_op_ctophys_offset(op,
			ctx->cipher_iv.offset);
	auth_iv_or_aad->va = (void *)op->sym->aead.aad.data;
	auth_iv_or_aad->iova = op->sym->aead.aad.phys_addr;
	digest->va = (void *)op->sym->aead.digest.data;
	digest->iova = op->sym->aead.digest.phys_addr;

	cipher_len = op->sym->aead.data.length;
	cipher_ofs = op->sym->aead.data.offset;

	n_src = rte_crypto_mbuf_to_vec(op->sym->m_src, cipher_ofs, cipher_len,
			in_sgl->vec, QAT_SYM_SGL_MAX_NUMBER);
	if (n_src < 0 || n_src > op->sym->m_src->nb_segs) {
		op->status = RTE_CRYPTO_OP_STATUS_ERROR;
		return UINT64_MAX;
	}
	in_sgl->num = n_src;

	/* Out-Of-Place operation */
	if (unlikely((op->sym->m_dst != NULL) &&
			(op->sym->m_dst != op->sym->m_src))) {
		int n_dst = rte_crypto_mbuf_to_vec(op->sym->m_dst, cipher_ofs,
				cipher_len, out_sgl->vec,
				QAT_SYM_SGL_MAX_NUMBER);
		if (n_dst < 0 || n_dst > op->sym->m_dst->nb_segs) {
			op->status = RTE_CRYPTO_OP_STATUS_ERROR;
			return UINT64_MAX;
		}

		out_sgl->num = n_dst;
	} else
		out_sgl->num = 0;

	return 0;
}

static __rte_always_inline void
qat_set_cipher_iv(struct icp_qat_fw_la_cipher_req_params *cipher_param,
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

static __rte_always_inline void
qat_sym_dp_fill_vec_status(int32_t *sta, int status, uint32_t n)
{
	uint32_t i;

	for (i = 0; i < n; i++)
		sta[i] = status;
}

static __rte_always_inline void
enqueue_one_cipher_job_gen1(struct qat_sym_session *ctx,
	struct icp_qat_fw_la_bulk_req *req,
	struct rte_crypto_va_iova_ptr *iv,
	union rte_crypto_sym_ofs ofs, uint32_t data_len)
{
	struct icp_qat_fw_la_cipher_req_params *cipher_param;

	cipher_param = (void *)&req->serv_specif_rqpars;

	/* cipher IV */
	qat_set_cipher_iv(cipher_param, iv, ctx->cipher_iv.length, req);
	cipher_param->cipher_offset = ofs.ofs.cipher.head;
	cipher_param->cipher_length = data_len - ofs.ofs.cipher.head -
			ofs.ofs.cipher.tail;
}

static __rte_always_inline void
enqueue_one_auth_job_gen1(struct qat_sym_session *ctx,
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
	case ICP_QAT_HW_AUTH_ALGO_SM3:
		if (ctx->auth_mode == ICP_QAT_HW_AUTH_MODE0)
			auth_param->u1.aad_adr = 0;
		else
			auth_param->u1.aad_adr = ctx->prefix_paddr;
		break;
	default:
		break;
	}
}

static __rte_always_inline int
enqueue_one_chain_job_gen1(struct qat_sym_session *ctx,
	struct icp_qat_fw_la_bulk_req *req,
	struct rte_crypto_vec *src_vec,
	uint16_t n_src_vecs,
	struct rte_crypto_vec *dst_vec,
	uint16_t n_dst_vecs,
	struct rte_crypto_va_iova_ptr *cipher_iv,
	struct rte_crypto_va_iova_ptr *digest,
	struct rte_crypto_va_iova_ptr *auth_iv,
	union rte_crypto_sym_ofs ofs, uint32_t data_len)
{
	struct icp_qat_fw_la_cipher_req_params *cipher_param;
	struct icp_qat_fw_la_auth_req_params *auth_param;
	struct rte_crypto_vec *cvec = n_dst_vecs > 0 ?
			dst_vec : src_vec;
	rte_iova_t auth_iova_end;
	int cipher_len, auth_len;
	int is_sgl = n_src_vecs > 1 || n_dst_vecs > 1;

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
	qat_set_cipher_iv(cipher_param, cipher_iv, ctx->cipher_iv.length, req);

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
	case ICP_QAT_HW_AUTH_ALGO_SM3:
		if (ctx->auth_mode == ICP_QAT_HW_AUTH_MODE0)
			auth_param->u1.aad_adr = 0;
		else
			auth_param->u1.aad_adr = ctx->prefix_paddr;
		break;
	default:
		break;
	}

	if (unlikely(is_sgl)) {
		/* sgl */
		int i = n_dst_vecs ? n_dst_vecs : n_src_vecs;
		uint32_t remaining_off = data_len - ofs.ofs.auth.tail;

		while (remaining_off >= cvec->len && i >= 1) {
			i--;
			remaining_off -= cvec->len;
			if (i)
				cvec++;
		}

		auth_iova_end = cvec->iova + remaining_off;
	} else
		auth_iova_end = cvec[0].iova + auth_param->auth_off +
			auth_param->auth_len;

	/* Then check if digest-encrypted conditions are met */
	if (((auth_param->auth_off + auth_param->auth_len <
		cipher_param->cipher_offset + cipher_param->cipher_length) &&
			(digest->iova == auth_iova_end)) ||
#ifdef RTE_QAT_OPENSSL
			ctx->bpi_ctx) {
#else
			ctx->mb_mgr) {
#endif
		/* Handle partial digest encryption */
		if (cipher_param->cipher_offset + cipher_param->cipher_length <
			auth_param->auth_off + auth_param->auth_len +
				ctx->digest_length && !is_sgl)
			req->comn_mid.dst_length = req->comn_mid.src_length =
				auth_param->auth_off + auth_param->auth_len +
					ctx->digest_length;
		struct icp_qat_fw_comn_req_hdr *header = &req->comn_hdr;
		ICP_QAT_FW_LA_DIGEST_IN_BUFFER_SET(header->serv_specif_flags,
			ICP_QAT_FW_LA_DIGEST_IN_BUFFER);
	}

	return 0;
}

static __rte_always_inline void
enqueue_one_aead_job_gen1(struct qat_sym_session *ctx,
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
}

extern struct rte_cryptodev_ops qat_sym_crypto_ops_gen1;
extern struct rte_cryptodev_ops qat_asym_crypto_ops_gen1;

/* -----------------GEN 1 sym crypto op data path APIs ---------------- */
int
qat_sym_build_op_cipher_gen1(void *in_op, struct qat_sym_session *ctx,
	uint8_t *out_msg, void *op_cookie);

int
qat_sym_build_op_auth_gen1(void *in_op, struct qat_sym_session *ctx,
		uint8_t *out_msg, void *op_cookie);

int
qat_sym_build_op_aead_gen1(void *in_op, struct qat_sym_session *ctx,
		uint8_t *out_msg, void *op_cookie);

int
qat_sym_build_op_chain_gen1(void *in_op, struct qat_sym_session *ctx,
		uint8_t *out_msg, void *op_cookie);

/* -----------------GEN 1 sym crypto raw data path APIs ---------------- */
int
qat_sym_dp_enqueue_single_cipher_gen1(void *qp_data, uint8_t *drv_ctx,
	struct rte_crypto_vec *data, uint16_t n_data_vecs,
	union rte_crypto_sym_ofs ofs,
	struct rte_crypto_va_iova_ptr *iv,
	struct rte_crypto_va_iova_ptr *digest __rte_unused,
	struct rte_crypto_va_iova_ptr *aad __rte_unused,
	void *user_data);

uint32_t
qat_sym_dp_enqueue_cipher_jobs_gen1(void *qp_data, uint8_t *drv_ctx,
	struct rte_crypto_sym_vec *vec, union rte_crypto_sym_ofs ofs,
	void *user_data[], int *status);

int
qat_sym_dp_enqueue_single_auth_gen1(void *qp_data, uint8_t *drv_ctx,
	struct rte_crypto_vec *data, uint16_t n_data_vecs,
	union rte_crypto_sym_ofs ofs,
	struct rte_crypto_va_iova_ptr *iv __rte_unused,
	struct rte_crypto_va_iova_ptr *digest,
	struct rte_crypto_va_iova_ptr *auth_iv,
	void *user_data);

uint32_t
qat_sym_dp_enqueue_auth_jobs_gen1(void *qp_data, uint8_t *drv_ctx,
	struct rte_crypto_sym_vec *vec, union rte_crypto_sym_ofs ofs,
	void *user_data[], int *status);

int
qat_sym_dp_enqueue_single_chain_gen1(void *qp_data, uint8_t *drv_ctx,
	struct rte_crypto_vec *data, uint16_t n_data_vecs,
	union rte_crypto_sym_ofs ofs,
	struct rte_crypto_va_iova_ptr *cipher_iv,
	struct rte_crypto_va_iova_ptr *digest,
	struct rte_crypto_va_iova_ptr *auth_iv,
	void *user_data);

uint32_t
qat_sym_dp_enqueue_chain_jobs_gen1(void *qp_data, uint8_t *drv_ctx,
	struct rte_crypto_sym_vec *vec, union rte_crypto_sym_ofs ofs,
	void *user_data[], int *status);

int
qat_sym_dp_enqueue_single_aead_gen1(void *qp_data, uint8_t *drv_ctx,
	struct rte_crypto_vec *data, uint16_t n_data_vecs,
	union rte_crypto_sym_ofs ofs,
	struct rte_crypto_va_iova_ptr *iv,
	struct rte_crypto_va_iova_ptr *digest,
	struct rte_crypto_va_iova_ptr *aad,
	void *user_data);

uint32_t
qat_sym_dp_enqueue_aead_jobs_gen1(void *qp_data, uint8_t *drv_ctx,
	struct rte_crypto_sym_vec *vec, union rte_crypto_sym_ofs ofs,
	void *user_data[], int *status);

void *
qat_sym_dp_dequeue_single_gen1(void *qp_data, uint8_t *drv_ctx,
	int *dequeue_status, enum rte_crypto_op_status *op_status);

uint32_t
qat_sym_dp_dequeue_burst_gen1(void *qp_data, uint8_t *drv_ctx,
	rte_cryptodev_raw_get_dequeue_count_t get_dequeue_count,
	uint32_t max_nb_to_dequeue,
	rte_cryptodev_raw_post_dequeue_t post_dequeue,
	void **out_user_data, uint8_t is_user_data_array,
	uint32_t *n_success_jobs, int *return_status);

int
qat_sym_dp_enqueue_done_gen1(void *qp_data, uint8_t *drv_ctx, uint32_t n);

int
qat_sym_dp_dequeue_done_gen1(void *qp_data, uint8_t *drv_ctx, uint32_t n);

int
qat_sym_configure_raw_dp_ctx_gen1(void *_raw_dp_ctx, void *_ctx);

/* -----------------GENx control path APIs ---------------- */
uint64_t
qat_sym_crypto_feature_flags_get_gen1(struct qat_pci_device *qat_dev);

int
qat_sym_crypto_set_session_gen1(void *cryptodev, void *session);

void
qat_sym_session_set_ext_hash_flags_gen2(struct qat_sym_session *session,
		uint8_t hash_flag);

int
qat_asym_crypto_cap_get_gen1(struct qat_cryptodev_private *internals,
			const char *capa_memz_name, const uint16_t slice_map);

uint64_t
qat_asym_crypto_feature_flags_get_gen1(struct qat_pci_device *qat_dev);

int
qat_asym_crypto_set_session_gen1(void *cryptodev, void *session);

extern struct rte_security_ops security_qat_ops_gen1;

void *
qat_sym_create_security_gen1(void *cryptodev);

#endif
