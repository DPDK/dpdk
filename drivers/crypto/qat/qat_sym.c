/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2018 Intel Corporation
 */

#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_hexdump.h>
#include <rte_crypto_sym.h>
#include <rte_bus_pci.h>
#include <rte_byteorder.h>

#include <openssl/evp.h>

#include "qat_logs.h"
#include "qat_sym_session.h"
#include "qat_sym.h"
#include "qat_qp.h"
#include "adf_transport_access_macros.h"
#include "qat_device.h"

#define BYTE_LENGTH    8
/* bpi is only used for partial blocks of DES and AES
 * so AES block len can be assumed as max len for iv, src and dst
 */
#define BPI_MAX_ENCR_IV_LEN ICP_QAT_HW_AES_BLK_SZ

/** Encrypt a single partial block
 *  Depends on openssl libcrypto
 *  Uses ECB+XOR to do CFB encryption, same result, more performant
 */
static inline int
bpi_cipher_encrypt(uint8_t *src, uint8_t *dst,
		uint8_t *iv, int ivlen, int srclen,
		void *bpi_ctx)
{
	EVP_CIPHER_CTX *ctx = (EVP_CIPHER_CTX *)bpi_ctx;
	int encrypted_ivlen;
	uint8_t encrypted_iv[BPI_MAX_ENCR_IV_LEN];
	uint8_t *encr = encrypted_iv;

	/* ECB method: encrypt the IV, then XOR this with plaintext */
	if (EVP_EncryptUpdate(ctx, encrypted_iv, &encrypted_ivlen, iv, ivlen)
								<= 0)
		goto cipher_encrypt_err;

	for (; srclen != 0; --srclen, ++dst, ++src, ++encr)
		*dst = *src ^ *encr;

	return 0;

cipher_encrypt_err:
	PMD_DRV_LOG(ERR, "libcrypto ECB cipher encrypt failed");
	return -EINVAL;
}

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
	PMD_DRV_LOG(ERR, "libcrypto ECB cipher decrypt for BPI IV failed");
	return -EINVAL;
}

/** Creates a context in either AES or DES in ECB mode
 *  Depends on openssl libcrypto
 */

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

#ifdef RTE_LIBRTE_PMD_QAT_DEBUG_TX
		rte_hexdump(stdout, "BPI: src before pre-process:", last_block,
			last_block_len);
		if (sym_op->m_dst != NULL)
			rte_hexdump(stdout, "BPI: dst before pre-process:", dst,
				last_block_len);
#endif
		bpi_cipher_decrypt(last_block, dst, iv, block_len,
				last_block_len, ctx->bpi_ctx);
#ifdef RTE_LIBRTE_PMD_QAT_DEBUG_TX
		rte_hexdump(stdout, "BPI: src after pre-process:", last_block,
			last_block_len);
		if (sym_op->m_dst != NULL)
			rte_hexdump(stdout, "BPI: dst after pre-process:", dst,
				last_block_len);
#endif
	}

	return sym_op->cipher.data.length - last_block_len;
}

static inline uint32_t
qat_bpicipher_postprocess(struct qat_sym_session *ctx,
				struct rte_crypto_op *op)
{
	int block_len = qat_cipher_get_block_size(ctx->qat_cipher_alg);
	struct rte_crypto_sym_op *sym_op = op->sym;
	uint8_t last_block_len = block_len > 0 ?
			sym_op->cipher.data.length % block_len : 0;

	if (last_block_len > 0 &&
			ctx->qat_dir == ICP_QAT_HW_CIPHER_ENCRYPT) {

		/* Encrypt last block */
		uint8_t *last_block, *dst, *iv;
		uint32_t last_block_offset;

		last_block_offset = sym_op->cipher.data.offset +
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
			iv = dst - block_len;
		else
			/* runt block, i.e. less than one full block */
			iv = rte_crypto_op_ctod_offset(op, uint8_t *,
					ctx->cipher_iv.offset);

#ifdef RTE_LIBRTE_PMD_QAT_DEBUG_RX
		rte_hexdump(stdout, "BPI: src before post-process:", last_block,
			last_block_len);
		if (sym_op->m_dst != NULL)
			rte_hexdump(stdout, "BPI: dst before post-process:",
					dst, last_block_len);
#endif
		bpi_cipher_encrypt(last_block, dst, iv, block_len,
				last_block_len, ctx->bpi_ctx);
#ifdef RTE_LIBRTE_PMD_QAT_DEBUG_RX
		rte_hexdump(stdout, "BPI: src after post-process:", last_block,
			last_block_len);
		if (sym_op->m_dst != NULL)
			rte_hexdump(stdout, "BPI: dst after post-process:", dst,
				last_block_len);
#endif
	}
	return sym_op->cipher.data.length - last_block_len;
}

uint16_t
qat_sym_pmd_enqueue_op_burst(void *qp, struct rte_crypto_op **ops,
		uint16_t nb_ops)
{
	return qat_enqueue_op_burst(qp, (void **)ops, nb_ops);
}

int
qat_sym_process_response(void **op, uint8_t *resp,
		__rte_unused void *op_cookie,
		__rte_unused enum qat_device_gen qat_dev_gen)
{

	struct icp_qat_fw_comn_resp *resp_msg =
			(struct icp_qat_fw_comn_resp *)resp;
	struct rte_crypto_op *rx_op = (struct rte_crypto_op *)(uintptr_t)
			(resp_msg->opaque_data);

#ifdef RTE_LIBRTE_PMD_QAT_DEBUG_RX
	rte_hexdump(stdout, "qat_response:", (uint8_t *)resp_msg,
			sizeof(struct icp_qat_fw_comn_resp));
#endif

	if (ICP_QAT_FW_COMN_STATUS_FLAG_OK !=
			ICP_QAT_FW_COMN_RESP_CRYPTO_STAT_GET(
			resp_msg->comn_hdr.comn_status)) {

		rx_op->status = RTE_CRYPTO_OP_STATUS_AUTH_FAILED;
	} else {
		struct qat_sym_session *sess = (struct qat_sym_session *)
						get_session_private_data(
						rx_op->sym->session,
						cryptodev_qat_driver_id);

		if (sess->bpi_ctx)
			qat_bpicipher_postprocess(sess, rx_op);
		rx_op->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
	}
	*op = (void *)rx_op;

	return 0;
}


uint16_t
qat_sym_pmd_dequeue_op_burst(void *qp, struct rte_crypto_op **ops,
		uint16_t nb_ops)
{
	return qat_dequeue_op_burst(qp, (void **)ops, nb_ops);
}

static inline int
qat_sgl_fill_array(struct rte_mbuf *buf, uint64_t buff_start,
		struct qat_alg_buf_list *list, uint32_t data_len)
{
	int nr = 1;

	uint32_t buf_len = rte_pktmbuf_iova(buf) -
			buff_start + rte_pktmbuf_data_len(buf);

	list->bufers[0].addr = buff_start;
	list->bufers[0].resrvd = 0;
	list->bufers[0].len = buf_len;

	if (data_len <= buf_len) {
		list->num_bufs = nr;
		list->bufers[0].len = data_len;
		return 0;
	}

	buf = buf->next;
	while (buf) {
		if (unlikely(nr == QAT_SGL_MAX_NUMBER)) {
			PMD_DRV_LOG(ERR, "QAT PMD exceeded size of QAT SGL"
					" entry(%u)",
					QAT_SGL_MAX_NUMBER);
			return -EINVAL;
		}

		list->bufers[nr].len = rte_pktmbuf_data_len(buf);
		list->bufers[nr].resrvd = 0;
		list->bufers[nr].addr = rte_pktmbuf_iova(buf);

		buf_len += list->bufers[nr].len;
		buf = buf->next;

		if (buf_len > data_len) {
			list->bufers[nr].len -=
				buf_len - data_len;
			buf = NULL;
		}
		++nr;
	}
	list->num_bufs = nr;

	return 0;
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
	struct rte_crypto_op *op = (struct rte_crypto_op *)in_op;
	struct qat_sym_op_cookie *cookie =
				(struct qat_sym_op_cookie *)op_cookie;

#ifdef RTE_LIBRTE_PMD_QAT_DEBUG_TX
	if (unlikely(op->type != RTE_CRYPTO_OP_TYPE_SYMMETRIC)) {
		PMD_DRV_LOG(ERR, "QAT PMD only supports symmetric crypto "
				"operation requests, op (%p) is not a "
				"symmetric operation.", op);
		return -EINVAL;
	}
#endif
	if (unlikely(op->sess_type == RTE_CRYPTO_OP_SESSIONLESS)) {
		PMD_DRV_LOG(ERR, "QAT PMD only supports session oriented"
				" requests, op (%p) is sessionless.", op);
		return -EINVAL;
	}

	ctx = (struct qat_sym_session *)get_session_private_data(
			op->sym->session, cryptodev_qat_driver_id);

	if (unlikely(ctx == NULL)) {
		PMD_DRV_LOG(ERR, "Session was not created for this device");
		return -EINVAL;
	}

	if (unlikely(ctx->min_qat_dev_gen > qat_dev_gen)) {
		PMD_DRV_LOG(ERR, "Session alg not supported on this device gen");
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
				(cipher_param->cipher_length % BYTE_LENGTH != 0)
				 || (cipher_param->cipher_offset
							% BYTE_LENGTH != 0))) {
				PMD_DRV_LOG(ERR,
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
			if (unlikely((auth_param->auth_off % BYTE_LENGTH != 0)
				|| (auth_param->auth_len % BYTE_LENGTH != 0))) {
				PMD_DRV_LOG(ERR,
		"For SNOW3G/KASUMI/ZUC, QAT PMD only supports byte aligned values");
				op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
				return -EINVAL;
			}
			auth_ofs = op->sym->auth.data.offset >> 3;
			auth_len = op->sym->auth.data.length >> 3;

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
		ret = qat_sgl_fill_array(op->sym->m_src, src_buf_start,
				&cookie->qat_sgl_list_src,
				qat_req->comn_mid.src_length);
		if (ret) {
			PMD_DRV_LOG(ERR, "QAT PMD Cannot fill sgl array");
			return ret;
		}

		if (likely(op->sym->m_dst == NULL))
			qat_req->comn_mid.dest_data_addr =
				qat_req->comn_mid.src_data_addr =
				cookie->qat_sgl_src_phys_addr;
		else {
			ret = qat_sgl_fill_array(op->sym->m_dst,
					dst_buf_start,
					&cookie->qat_sgl_list_dst,
						qat_req->comn_mid.dst_length);

			if (ret) {
				PMD_DRV_LOG(ERR, "QAT PMD Cannot "
						"fill sgl array");
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
	}

#ifdef RTE_LIBRTE_PMD_QAT_DEBUG_TX
	rte_hexdump(stdout, "qat_req:", qat_req,
			sizeof(struct icp_qat_fw_la_bulk_req));
	rte_hexdump(stdout, "src_data:",
			rte_pktmbuf_mtod(op->sym->m_src, uint8_t*),
			rte_pktmbuf_data_len(op->sym->m_src));
	if (do_cipher) {
		uint8_t *cipher_iv_ptr = rte_crypto_op_ctod_offset(op,
						uint8_t *,
						ctx->cipher_iv.offset);
		rte_hexdump(stdout, "cipher iv:", cipher_iv_ptr,
				ctx->cipher_iv.length);
	}

	if (do_auth) {
		if (ctx->auth_iv.length) {
			uint8_t *auth_iv_ptr = rte_crypto_op_ctod_offset(op,
							uint8_t *,
							ctx->auth_iv.offset);
			rte_hexdump(stdout, "auth iv:", auth_iv_ptr,
						ctx->auth_iv.length);
		}
		rte_hexdump(stdout, "digest:", op->sym->auth.digest.data,
				ctx->digest_length);
	}

	if (do_aead) {
		rte_hexdump(stdout, "digest:", op->sym->aead.digest.data,
				ctx->digest_length);
		rte_hexdump(stdout, "aad:", op->sym->aead.aad.data,
				ctx->aad_len);
	}
#endif
	return 0;
}


void qat_sym_stats_get(struct rte_cryptodev *dev,
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
		stats->dequeued_count += qp[i]->stats.dequeued_count;
		stats->enqueue_err_count += qp[i]->stats.enqueue_err_count;
		stats->dequeue_err_count += qp[i]->stats.dequeue_err_count;
	}
}

void qat_sym_stats_reset(struct rte_cryptodev *dev)
{
	int i;
	struct qat_qp **qp = (struct qat_qp **)(dev->data->queue_pairs);

	PMD_INIT_FUNC_TRACE();
	for (i = 0; i < dev->data->nb_queue_pairs; i++)
		memset(&(qp[i]->stats), 0, sizeof(qp[i]->stats));
	PMD_DRV_LOG(DEBUG, "QAT crypto: stats cleared");
}

int qat_sym_qp_release(struct rte_cryptodev *dev, uint16_t queue_pair_id)
{
	PMD_DRV_LOG(DEBUG, "Release sym qp %u on device %d",
				queue_pair_id, dev->data->dev_id);

	return qat_qp_release((struct qat_qp **)
			&(dev->data->queue_pairs[queue_pair_id]));
}

int qat_sym_qp_setup(struct rte_cryptodev *dev, uint16_t qp_id,
	const struct rte_cryptodev_qp_conf *qp_conf,
	int socket_id, struct rte_mempool *session_pool __rte_unused)
{
	struct qat_qp *qp;
	int ret = 0;
	uint32_t i;
	struct qat_qp_config qat_qp_conf;

	struct qat_qp **qp_addr =
			(struct qat_qp **)&(dev->data->queue_pairs[qp_id]);
	struct qat_pmd_private *qat_private = dev->data->dev_private;
	const struct qat_qp_hw_data *sym_hw_qps =
			qp_gen_config[qat_private->qat_dev_gen]
				      .qp_hw_data[QAT_SERVICE_SYMMETRIC];
	const struct qat_qp_hw_data *qp_hw_data = sym_hw_qps + qp_id;

	/* If qp is already in use free ring memory and qp metadata. */
	if (*qp_addr != NULL) {
		ret = qat_sym_qp_release(dev, qp_id);
		if (ret < 0)
			return ret;
	}
	if (qp_id >= qat_qps_per_service(sym_hw_qps, QAT_SERVICE_SYMMETRIC)) {
		PMD_DRV_LOG(ERR, "qp_id %u invalid for this device", qp_id);
		return -EINVAL;
	}

	qat_qp_conf.hw = qp_hw_data;
	qat_qp_conf.build_request = qat_sym_build_request;
	qat_qp_conf.process_response = qat_sym_process_response;
	qat_qp_conf.cookie_size = sizeof(struct qat_sym_op_cookie);
	qat_qp_conf.nb_descriptors = qp_conf->nb_descriptors;
	qat_qp_conf.socket_id = socket_id;
	qat_qp_conf.service_str = "sym";

	ret = qat_qp_setup(qat_private, qp_addr, qp_id, &qat_qp_conf);
	if (ret != 0)
		return ret;

	qp = (struct qat_qp *)*qp_addr;

	for (i = 0; i < qp->nb_descriptors; i++) {

		struct qat_sym_op_cookie *sql_cookie =
				qp->op_cookies[i];

		sql_cookie->qat_sgl_src_phys_addr =
				rte_mempool_virt2iova(sql_cookie) +
				offsetof(struct qat_sym_op_cookie,
				qat_sgl_list_src);

		sql_cookie->qat_sgl_dst_phys_addr =
				rte_mempool_virt2iova(sql_cookie) +
				offsetof(struct qat_sym_op_cookie,
				qat_sgl_list_dst);
	}

	return ret;
}
