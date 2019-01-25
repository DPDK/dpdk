/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2018 Intel Corporation
 */

#include <openssl/evp.h>

#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_crypto_sym.h>
#include <rte_bus_pci.h>
#include <rte_byteorder.h>

#include "qat_sym.h"

/** Decrypt a single partial block
 *  Depends on openssl libcrypto
 *  Uses ECB+XOR to do CFB encryption, same result, more performant
 */
static inline int
bpi_cipher_decrypt(uint8_t *src, uint8_t *dst,
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


static inline uint32_t
qat_bpicipher_preprocess(struct qat_sym_session *ctx,
				struct rte_crypto_op *op)
{
	int block_len = qat_cipher_get_block_size(ctx->qat_cipher_alg);
	struct rte_crypto_sym_op *sym_op = op->sym;
	uint8_t last_block_len = block_len > 0 ?
			sym_op->cipher.data.length % block_len : 0;

	if (last_block_len &&
			ctx->qat_dir == ICP_QAT_HW_CIPHER_DECRYPT) {

		/* Decrypt last block */
		uint8_t *last_block, *dst, *iv;
		uint32_t last_block_offset = sym_op->cipher.data.offset +
				sym_op->cipher.data.length - last_block_len;
		last_block = (uint8_t *) rte_pktmbuf_mtod_offset(sym_op->m_src,
				uint8_t *, last_block_offset);

		if (unlikely(sym_op->m_dst != NULL))
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
			QAT_DP_HEXDUMP_LOG(DEBUG, "BPI:dst before pre-process:",
			dst, last_block_len);
#endif
		bpi_cipher_decrypt(last_block, dst, iv, block_len,
				last_block_len, ctx->bpi_ctx);
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

static inline void
set_cipher_iv(uint16_t iv_length, uint16_t iv_offset,
		struct icp_qat_fw_la_cipher_req_params *cipher_param,
		struct rte_crypto_op *op,
		struct icp_qat_fw_la_bulk_req *qat_req)
{
	/* copy IV into request if it fits */
	if (iv_length <= sizeof(cipher_param->u.cipher_IV_array)) {
		rte_memcpy(cipher_param->u.cipher_IV_array,
				rte_crypto_op_ctod_offset(op, uint8_t *,
					iv_offset),
				iv_length);
	} else {
		ICP_QAT_FW_LA_CIPH_IV_FLD_FLAG_SET(
				qat_req->comn_hdr.serv_specif_flags,
				ICP_QAT_FW_CIPH_IV_64BIT_PTR);
		cipher_param->u.s.cipher_IV_ptr =
				rte_crypto_op_ctophys_offset(op,
					iv_offset);
	}
}

/** Set IV for CCM is special case, 0th byte is set to q-1
 *  where q is padding of nonce in 16 byte block
 */
static inline void
set_cipher_iv_ccm(uint16_t iv_length, uint16_t iv_offset,
		struct icp_qat_fw_la_cipher_req_params *cipher_param,
		struct rte_crypto_op *op, uint8_t q, uint8_t aad_len_field_sz)
{
	rte_memcpy(((uint8_t *)cipher_param->u.cipher_IV_array) +
			ICP_QAT_HW_CCM_NONCE_OFFSET,
			rte_crypto_op_ctod_offset(op, uint8_t *,
				iv_offset) + ICP_QAT_HW_CCM_NONCE_OFFSET,
			iv_length);
	*(uint8_t *)&cipher_param->u.cipher_IV_array[0] =
			q - ICP_QAT_HW_CCM_NONCE_OFFSET;

	if (aad_len_field_sz)
		rte_memcpy(&op->sym->aead.aad.data[ICP_QAT_HW_CCM_NONCE_OFFSET],
			rte_crypto_op_ctod_offset(op, uint8_t *,
				iv_offset) + ICP_QAT_HW_CCM_NONCE_OFFSET,
			iv_length);
}

int
qat_sym_build_request(void *in_op, uint8_t *out_msg,
		void *op_cookie, enum qat_device_gen qat_dev_gen)
{
	int ret = 0;
	struct qat_sym_session *ctx;
	struct icp_qat_fw_la_cipher_req_params *cipher_param;
	struct icp_qat_fw_la_auth_req_params *auth_param;
	register struct icp_qat_fw_la_bulk_req *qat_req;
	uint8_t do_auth = 0, do_cipher = 0, do_aead = 0;
	uint32_t cipher_len = 0, cipher_ofs = 0;
	uint32_t auth_len = 0, auth_ofs = 0;
	uint32_t min_ofs = 0;
	uint64_t src_buf_start = 0, dst_buf_start = 0;
	uint8_t do_sgl = 0;
	uint8_t wireless_auth = 0, in_place = 1;
	struct rte_crypto_op *op = (struct rte_crypto_op *)in_op;
	struct qat_sym_op_cookie *cookie =
				(struct qat_sym_op_cookie *)op_cookie;

	if (unlikely(op->type != RTE_CRYPTO_OP_TYPE_SYMMETRIC)) {
		QAT_DP_LOG(ERR, "QAT PMD only supports symmetric crypto "
				"operation requests, op (%p) is not a "
				"symmetric operation.", op);
		return -EINVAL;
	}

	if (unlikely(op->sess_type == RTE_CRYPTO_OP_SESSIONLESS)) {
		QAT_DP_LOG(ERR, "QAT PMD only supports session oriented"
				" requests, op (%p) is sessionless.", op);
		return -EINVAL;
	}

	ctx = (struct qat_sym_session *)get_sym_session_private_data(
			op->sym->session, cryptodev_qat_driver_id);

	if (unlikely(ctx == NULL)) {
		QAT_DP_LOG(ERR, "Session was not created for this device");
		return -EINVAL;
	}

	if (unlikely(ctx->min_qat_dev_gen > qat_dev_gen)) {
		QAT_DP_LOG(ERR, "Session alg not supported on this device gen");
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_SESSION;
		return -EINVAL;
	}

	qat_req = (struct icp_qat_fw_la_bulk_req *)out_msg;
	rte_mov128((uint8_t *)qat_req, (const uint8_t *)&(ctx->fw_req));
	qat_req->comn_mid.opaque_data = (uint64_t)(uintptr_t)op;
	cipher_param = (void *)&qat_req->serv_specif_rqpars;
	auth_param = (void *)((uint8_t *)cipher_param + sizeof(*cipher_param));

	if (ctx->qat_cmd == ICP_QAT_FW_LA_CMD_HASH_CIPHER ||
			ctx->qat_cmd == ICP_QAT_FW_LA_CMD_CIPHER_HASH) {
		/* AES-GCM or AES-CCM */
		if (ctx->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_GALOIS_128 ||
			ctx->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_GALOIS_64 ||
			(ctx->qat_cipher_alg == ICP_QAT_HW_CIPHER_ALGO_AES128
			&& ctx->qat_mode == ICP_QAT_HW_CIPHER_CTR_MODE
			&& ctx->qat_hash_alg ==
					ICP_QAT_HW_AUTH_ALGO_AES_CBC_MAC)) {
			do_aead = 1;
		} else {
			do_auth = 1;
			do_cipher = 1;
		}
	} else if (ctx->qat_cmd == ICP_QAT_FW_LA_CMD_AUTH) {
		do_auth = 1;
		do_cipher = 0;
	} else if (ctx->qat_cmd == ICP_QAT_FW_LA_CMD_CIPHER) {
		do_auth = 0;
		do_cipher = 1;
	}

	if (do_cipher) {

		if (ctx->qat_cipher_alg ==
					 ICP_QAT_HW_CIPHER_ALGO_SNOW_3G_UEA2 ||
			ctx->qat_cipher_alg == ICP_QAT_HW_CIPHER_ALGO_KASUMI ||
			ctx->qat_cipher_alg ==
				ICP_QAT_HW_CIPHER_ALGO_ZUC_3G_128_EEA3) {

			if (unlikely(
			    (op->sym->cipher.data.length % BYTE_LENGTH != 0) ||
			    (op->sym->cipher.data.offset % BYTE_LENGTH != 0))) {
				QAT_DP_LOG(ERR,
		  "SNOW3G/KASUMI/ZUC in QAT PMD only supports byte aligned values");
				op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
				return -EINVAL;
			}
			cipher_len = op->sym->cipher.data.length >> 3;
			cipher_ofs = op->sym->cipher.data.offset >> 3;

		} else if (ctx->bpi_ctx) {
			/* DOCSIS - only send complete blocks to device
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

		set_cipher_iv(ctx->cipher_iv.length, ctx->cipher_iv.offset,
				cipher_param, op, qat_req);
		min_ofs = cipher_ofs;
	}

	if (do_auth) {

		if (ctx->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_SNOW_3G_UIA2 ||
			ctx->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_KASUMI_F9 ||
			ctx->qat_hash_alg ==
				ICP_QAT_HW_AUTH_ALGO_ZUC_3G_128_EIA3) {
			if (unlikely(
			    (op->sym->auth.data.offset % BYTE_LENGTH != 0) ||
			    (op->sym->auth.data.length % BYTE_LENGTH != 0))) {
				QAT_DP_LOG(ERR,
		"For SNOW3G/KASUMI/ZUC, QAT PMD only supports byte aligned values");
				op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
				return -EINVAL;
			}
			auth_ofs = op->sym->auth.data.offset >> 3;
			auth_len = op->sym->auth.data.length >> 3;
			wireless_auth = 1;

			auth_param->u1.aad_adr =
					rte_crypto_op_ctophys_offset(op,
							ctx->auth_iv.offset);

		} else if (ctx->qat_hash_alg ==
					ICP_QAT_HW_AUTH_ALGO_GALOIS_128 ||
				ctx->qat_hash_alg ==
					ICP_QAT_HW_AUTH_ALGO_GALOIS_64) {
			/* AES-GMAC */
			set_cipher_iv(ctx->auth_iv.length,
				ctx->auth_iv.offset,
				cipher_param, op, qat_req);
			auth_ofs = op->sym->auth.data.offset;
			auth_len = op->sym->auth.data.length;

			auth_param->u1.aad_adr = 0;
			auth_param->u2.aad_sz = 0;

			/*
			 * If len(iv)==12B fw computes J0
			 */
			if (ctx->auth_iv.length == 12) {
				ICP_QAT_FW_LA_GCM_IV_LEN_FLAG_SET(
					qat_req->comn_hdr.serv_specif_flags,
					ICP_QAT_FW_LA_GCM_IV_LEN_12_OCTETS);

			}
		} else {
			auth_ofs = op->sym->auth.data.offset;
			auth_len = op->sym->auth.data.length;

		}
		min_ofs = auth_ofs;

		if (likely(ctx->qat_hash_alg != ICP_QAT_HW_AUTH_ALGO_NULL))
			auth_param->auth_res_addr =
					op->sym->auth.digest.phys_addr;

	}

	if (do_aead) {
		/*
		 * This address may used for setting AAD physical pointer
		 * into IV offset from op
		 */
		rte_iova_t aad_phys_addr_aead = op->sym->aead.aad.phys_addr;
		if (ctx->qat_hash_alg ==
				ICP_QAT_HW_AUTH_ALGO_GALOIS_128 ||
				ctx->qat_hash_alg ==
					ICP_QAT_HW_AUTH_ALGO_GALOIS_64) {
			/*
			 * If len(iv)==12B fw computes J0
			 */
			if (ctx->cipher_iv.length == 12) {
				ICP_QAT_FW_LA_GCM_IV_LEN_FLAG_SET(
					qat_req->comn_hdr.serv_specif_flags,
					ICP_QAT_FW_LA_GCM_IV_LEN_12_OCTETS);
			}
			set_cipher_iv(ctx->cipher_iv.length,
					ctx->cipher_iv.offset,
					cipher_param, op, qat_req);

		} else if (ctx->qat_hash_alg ==
				ICP_QAT_HW_AUTH_ALGO_AES_CBC_MAC) {

			/* In case of AES-CCM this may point to user selected
			 * memory or iv offset in cypto_op
			 */
			uint8_t *aad_data = op->sym->aead.aad.data;
			/* This is true AAD length, it not includes 18 bytes of
			 * preceding data
			 */
			uint8_t aad_ccm_real_len = 0;
			uint8_t aad_len_field_sz = 0;
			uint32_t msg_len_be =
					rte_bswap32(op->sym->aead.data.length);

			if (ctx->aad_len > ICP_QAT_HW_CCM_AAD_DATA_OFFSET) {
				aad_len_field_sz = ICP_QAT_HW_CCM_AAD_LEN_INFO;
				aad_ccm_real_len = ctx->aad_len -
					ICP_QAT_HW_CCM_AAD_B0_LEN -
					ICP_QAT_HW_CCM_AAD_LEN_INFO;
			} else {
				/*
				 * aad_len not greater than 18, so no actual aad
				 *  data, then use IV after op for B0 block
				 */
				aad_data = rte_crypto_op_ctod_offset(op,
						uint8_t *,
						ctx->cipher_iv.offset);
				aad_phys_addr_aead =
						rte_crypto_op_ctophys_offset(op,
							ctx->cipher_iv.offset);
			}

			uint8_t q = ICP_QAT_HW_CCM_NQ_CONST -
							ctx->cipher_iv.length;

			aad_data[0] = ICP_QAT_HW_CCM_BUILD_B0_FLAGS(
							aad_len_field_sz,
							ctx->digest_length, q);

			if (q > ICP_QAT_HW_CCM_MSG_LEN_MAX_FIELD_SIZE) {
				memcpy(aad_data	+ ctx->cipher_iv.length +
				    ICP_QAT_HW_CCM_NONCE_OFFSET +
				    (q - ICP_QAT_HW_CCM_MSG_LEN_MAX_FIELD_SIZE),
				    (uint8_t *)&msg_len_be,
				    ICP_QAT_HW_CCM_MSG_LEN_MAX_FIELD_SIZE);
			} else {
				memcpy(aad_data	+ ctx->cipher_iv.length +
				    ICP_QAT_HW_CCM_NONCE_OFFSET,
				    (uint8_t *)&msg_len_be
				    + (ICP_QAT_HW_CCM_MSG_LEN_MAX_FIELD_SIZE
				    - q), q);
			}

			if (aad_len_field_sz > 0) {
				*(uint16_t *)&aad_data[ICP_QAT_HW_CCM_AAD_B0_LEN]
						= rte_bswap16(aad_ccm_real_len);

				if ((aad_ccm_real_len + aad_len_field_sz)
						% ICP_QAT_HW_CCM_AAD_B0_LEN) {
					uint8_t pad_len = 0;
					uint8_t pad_idx = 0;

					pad_len = ICP_QAT_HW_CCM_AAD_B0_LEN -
					((aad_ccm_real_len + aad_len_field_sz) %
						ICP_QAT_HW_CCM_AAD_B0_LEN);
					pad_idx = ICP_QAT_HW_CCM_AAD_B0_LEN +
					    aad_ccm_real_len + aad_len_field_sz;
					memset(&aad_data[pad_idx],
							0, pad_len);
				}

			}

			set_cipher_iv_ccm(ctx->cipher_iv.length,
					ctx->cipher_iv.offset,
					cipher_param, op, q,
					aad_len_field_sz);

		}

		cipher_len = op->sym->aead.data.length;
		cipher_ofs = op->sym->aead.data.offset;
		auth_len = op->sym->aead.data.length;
		auth_ofs = op->sym->aead.data.offset;

		auth_param->u1.aad_adr = aad_phys_addr_aead;
		auth_param->auth_res_addr = op->sym->aead.digest.phys_addr;
		min_ofs = op->sym->aead.data.offset;
	}

	if (op->sym->m_src->next || (op->sym->m_dst && op->sym->m_dst->next))
		do_sgl = 1;

	/* adjust for chain case */
	if (do_cipher && do_auth)
		min_ofs = cipher_ofs < auth_ofs ? cipher_ofs : auth_ofs;

	if (unlikely(min_ofs >= rte_pktmbuf_data_len(op->sym->m_src) && do_sgl))
		min_ofs = 0;

	if (unlikely(op->sym->m_dst != NULL)) {
		/* Out-of-place operation (OOP)
		 * Don't align DMA start. DMA the minimum data-set
		 * so as not to overwrite data in dest buffer
		 */
		in_place = 0;
		src_buf_start =
			rte_pktmbuf_iova_offset(op->sym->m_src, min_ofs);
		dst_buf_start =
			rte_pktmbuf_iova_offset(op->sym->m_dst, min_ofs);

	} else {
		/* In-place operation
		 * Start DMA at nearest aligned address below min_ofs
		 */
		src_buf_start =
			rte_pktmbuf_iova_offset(op->sym->m_src, min_ofs)
						& QAT_64_BTYE_ALIGN_MASK;

		if (unlikely((rte_pktmbuf_iova(op->sym->m_src) -
					rte_pktmbuf_headroom(op->sym->m_src))
							> src_buf_start)) {
			/* alignment has pushed addr ahead of start of mbuf
			 * so revert and take the performance hit
			 */
			src_buf_start =
				rte_pktmbuf_iova_offset(op->sym->m_src,
								min_ofs);
		}
		dst_buf_start = src_buf_start;
	}

	if (do_cipher || do_aead) {
		cipher_param->cipher_offset =
				(uint32_t)rte_pktmbuf_iova_offset(
				op->sym->m_src, cipher_ofs) - src_buf_start;
		cipher_param->cipher_length = cipher_len;
	} else {
		cipher_param->cipher_offset = 0;
		cipher_param->cipher_length = 0;
	}

	if (do_auth || do_aead) {
		auth_param->auth_off = (uint32_t)rte_pktmbuf_iova_offset(
				op->sym->m_src, auth_ofs) - src_buf_start;
		auth_param->auth_len = auth_len;
	} else {
		auth_param->auth_off = 0;
		auth_param->auth_len = 0;
	}

	qat_req->comn_mid.dst_length =
		qat_req->comn_mid.src_length =
		(cipher_param->cipher_offset + cipher_param->cipher_length)
		> (auth_param->auth_off + auth_param->auth_len) ?
		(cipher_param->cipher_offset + cipher_param->cipher_length)
		: (auth_param->auth_off + auth_param->auth_len);

	if (do_sgl) {

		ICP_QAT_FW_COMN_PTR_TYPE_SET(qat_req->comn_hdr.comn_req_flags,
				QAT_COMN_PTR_TYPE_SGL);
		ret = qat_sgl_fill_array(op->sym->m_src,
		   (int64_t)(src_buf_start - rte_pktmbuf_iova(op->sym->m_src)),
		   &cookie->qat_sgl_src,
		   qat_req->comn_mid.src_length,
		   QAT_SYM_SGL_MAX_NUMBER);

		if (unlikely(ret)) {
			QAT_DP_LOG(ERR, "QAT PMD Cannot fill sgl array");
			return ret;
		}

		if (likely(op->sym->m_dst == NULL))
			qat_req->comn_mid.dest_data_addr =
				qat_req->comn_mid.src_data_addr =
				cookie->qat_sgl_src_phys_addr;
		else {
			ret = qat_sgl_fill_array(op->sym->m_dst,
				(int64_t)(dst_buf_start -
					  rte_pktmbuf_iova(op->sym->m_dst)),
				 &cookie->qat_sgl_dst,
				 qat_req->comn_mid.dst_length,
				 QAT_SYM_SGL_MAX_NUMBER);

			if (unlikely(ret)) {
				QAT_DP_LOG(ERR, "QAT PMD can't fill sgl array");
				return ret;
			}

			qat_req->comn_mid.src_data_addr =
				cookie->qat_sgl_src_phys_addr;
			qat_req->comn_mid.dest_data_addr =
					cookie->qat_sgl_dst_phys_addr;
		}
	} else {
		qat_req->comn_mid.src_data_addr = src_buf_start;
		qat_req->comn_mid.dest_data_addr = dst_buf_start;
		/* handle case of auth-gen-then-cipher with digest encrypted */
		if (wireless_auth && in_place &&
		    (op->sym->auth.digest.phys_addr ==
				src_buf_start + auth_ofs + auth_len) &&
		    (auth_ofs + auth_len + ctx->digest_length <=
				cipher_ofs + cipher_len)) {
			struct icp_qat_fw_comn_req_hdr *header =
						&qat_req->comn_hdr;
			ICP_QAT_FW_LA_DIGEST_IN_BUFFER_SET(
				header->serv_specif_flags,
				ICP_QAT_FW_LA_DIGEST_IN_BUFFER);
		}
	}

#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
	QAT_DP_HEXDUMP_LOG(DEBUG, "qat_req:", qat_req,
			sizeof(struct icp_qat_fw_la_bulk_req));
	QAT_DP_HEXDUMP_LOG(DEBUG, "src_data:",
			rte_pktmbuf_mtod(op->sym->m_src, uint8_t*),
			rte_pktmbuf_data_len(op->sym->m_src));
	if (do_cipher) {
		uint8_t *cipher_iv_ptr = rte_crypto_op_ctod_offset(op,
						uint8_t *,
						ctx->cipher_iv.offset);
		QAT_DP_HEXDUMP_LOG(DEBUG, "cipher iv:", cipher_iv_ptr,
				ctx->cipher_iv.length);
	}

	if (do_auth) {
		if (ctx->auth_iv.length) {
			uint8_t *auth_iv_ptr = rte_crypto_op_ctod_offset(op,
							uint8_t *,
							ctx->auth_iv.offset);
			QAT_DP_HEXDUMP_LOG(DEBUG, "auth iv:", auth_iv_ptr,
						ctx->auth_iv.length);
		}
		QAT_DP_HEXDUMP_LOG(DEBUG, "digest:", op->sym->auth.digest.data,
				ctx->digest_length);
	}

	if (do_aead) {
		QAT_DP_HEXDUMP_LOG(DEBUG, "digest:", op->sym->aead.digest.data,
				ctx->digest_length);
		QAT_DP_HEXDUMP_LOG(DEBUG, "aad:", op->sym->aead.aad.data,
				ctx->aad_len);
	}
#endif
	return 0;
}
