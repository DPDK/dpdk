/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2022 Intel Corporation
 */

#ifndef _QAT_SYM_H_
#define _QAT_SYM_H_

#include <cryptodev_pmd.h>
#ifdef RTE_LIB_SECURITY
#include <rte_net_crc.h>
#endif

#ifdef BUILD_QAT_SYM
#include <openssl/evp.h>

#include "qat_common.h"
#include "qat_sym_session.h"
#include "qat_crypto.h"
#include "qat_logs.h"

#define BYTE_LENGTH    8
/* bpi is only used for partial blocks of DES and AES
 * so AES block len can be assumed as max len for iv, src and dst
 */
#define BPI_MAX_ENCR_IV_LEN ICP_QAT_HW_AES_BLK_SZ

/** Intel(R) QAT Symmetric Crypto PMD name */
#define CRYPTODEV_NAME_QAT_SYM_PMD	crypto_qat

/* Internal capabilities */
#define QAT_SYM_CAP_MIXED_CRYPTO	(1 << 0)
#define QAT_SYM_CAP_VALID		(1 << 31)

/**
 * Macro to add a sym capability
 * helper function to add an sym capability
 * <n: name> <b: block size> <k: key size> <d: digest size>
 * <a: aad_size> <i: iv_size>
 **/
#define QAT_SYM_PLAIN_AUTH_CAP(n, b, d)					\
	{								\
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,			\
		{.sym = {						\
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,	\
			{.auth = {					\
				.algo = RTE_CRYPTO_AUTH_##n,		\
				b, d					\
			}, }						\
		}, }							\
	}

#define QAT_SYM_AUTH_CAP(n, b, k, d, a, i)				\
	{								\
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,			\
		{.sym = {						\
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,	\
			{.auth = {					\
				.algo = RTE_CRYPTO_AUTH_##n,		\
				b, k, d, a, i				\
			}, }						\
		}, }							\
	}

#define QAT_SYM_AEAD_CAP(n, b, k, d, a, i)				\
	{								\
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,			\
		{.sym = {						\
			.xform_type = RTE_CRYPTO_SYM_XFORM_AEAD,	\
			{.aead = {					\
				.algo = RTE_CRYPTO_AEAD_##n,		\
				b, k, d, a, i				\
			}, }						\
		}, }							\
	}

#define QAT_SYM_CIPHER_CAP(n, b, k, i)					\
	{								\
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,			\
		{.sym = {						\
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,	\
			{.cipher = {					\
				.algo = RTE_CRYPTO_CIPHER_##n,		\
				b, k, i					\
			}, }						\
		}, }							\
	}

/*
 * Maximum number of SGL entries
 */
#define QAT_SYM_SGL_MAX_NUMBER	16

/* Maximum data length for single pass GMAC: 2^14-1 */
#define QAT_AES_GMAC_SPC_MAX_SIZE 16383

struct qat_sym_session;

struct qat_sym_sgl {
	qat_sgl_hdr;
	struct qat_flat_buf buffers[QAT_SYM_SGL_MAX_NUMBER];
} __rte_packed __rte_cache_aligned;

struct qat_sym_op_cookie {
	struct qat_sym_sgl qat_sgl_src;
	struct qat_sym_sgl qat_sgl_dst;
	phys_addr_t qat_sgl_src_phys_addr;
	phys_addr_t qat_sgl_dst_phys_addr;
	union {
		/* Used for Single-Pass AES-GMAC only */
		struct {
			struct icp_qat_hw_cipher_algo_blk cd_cipher
					__rte_packed __rte_cache_aligned;
			phys_addr_t cd_phys_addr;
		} spc_gmac;
	} opt;
};

struct qat_sym_dp_ctx {
	struct qat_sym_session *session;
	uint32_t tail;
	uint32_t head;
	uint16_t cached_enqueue;
	uint16_t cached_dequeue;
};

uint16_t
qat_sym_enqueue_burst(void *qp, struct rte_crypto_op **ops,
		uint16_t nb_ops);

uint16_t
qat_sym_dequeue_burst(void *qp, struct rte_crypto_op **ops,
		uint16_t nb_ops);

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
	QAT_DP_LOG(ERR, "libcrypto ECB cipher encrypt failed");
	return -EINVAL;
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

#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
		QAT_DP_HEXDUMP_LOG(DEBUG, "BPI: src before post-process:",
			last_block, last_block_len);
		if (sym_op->m_dst != NULL)
			QAT_DP_HEXDUMP_LOG(DEBUG,
				"BPI: dst before post-process:",
				dst, last_block_len);
#endif
		bpi_cipher_encrypt(last_block, dst, iv, block_len,
				last_block_len, ctx->bpi_ctx);
#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
		QAT_DP_HEXDUMP_LOG(DEBUG, "BPI: src after post-process:",
				last_block, last_block_len);
		if (sym_op->m_dst != NULL)
			QAT_DP_HEXDUMP_LOG(DEBUG,
				"BPI: dst after post-process:",
				dst, last_block_len);
#endif
	}
	return sym_op->cipher.data.length - last_block_len;
}

#ifdef RTE_LIB_SECURITY
static inline void
qat_crc_verify(struct qat_sym_session *ctx, struct rte_crypto_op *op)
{
	struct rte_crypto_sym_op *sym_op = op->sym;
	uint32_t crc_data_ofs, crc_data_len, crc;
	uint8_t *crc_data;

	if (ctx->qat_dir == ICP_QAT_HW_CIPHER_DECRYPT &&
			sym_op->auth.data.length != 0) {

		crc_data_ofs = sym_op->auth.data.offset;
		crc_data_len = sym_op->auth.data.length;
		crc_data = rte_pktmbuf_mtod_offset(sym_op->m_src, uint8_t *,
				crc_data_ofs);

		crc = rte_net_crc_calc(crc_data, crc_data_len,
				RTE_NET_CRC32_ETH);

		if (crc != *(uint32_t *)(crc_data + crc_data_len))
			op->status = RTE_CRYPTO_OP_STATUS_AUTH_FAILED;
	}
}

static inline void
qat_crc_generate(struct qat_sym_session *ctx,
			struct rte_crypto_op *op)
{
	struct rte_crypto_sym_op *sym_op = op->sym;
	uint32_t *crc, crc_data_len;
	uint8_t *crc_data;

	if (ctx->qat_dir == ICP_QAT_HW_CIPHER_ENCRYPT &&
			sym_op->auth.data.length != 0 &&
			sym_op->m_src->nb_segs == 1) {

		crc_data_len = sym_op->auth.data.length;
		crc_data = rte_pktmbuf_mtod_offset(sym_op->m_src, uint8_t *,
				sym_op->auth.data.offset);
		crc = (uint32_t *)(crc_data + crc_data_len);
		*crc = rte_net_crc_calc(crc_data, crc_data_len,
				RTE_NET_CRC32_ETH);
	}
}

static inline void
qat_sym_preprocess_requests(void **ops, uint16_t nb_ops)
{
	struct rte_crypto_op *op;
	struct qat_sym_session *ctx;
	uint16_t i;

	for (i = 0; i < nb_ops; i++) {
		op = (struct rte_crypto_op *)ops[i];

		if (op->sess_type == RTE_CRYPTO_OP_SECURITY_SESSION) {
			ctx = (struct qat_sym_session *)
				get_sec_session_private_data(
					op->sym->sec_session);

			if (ctx == NULL || ctx->bpi_ctx == NULL)
				continue;

			qat_crc_generate(ctx, op);
		}
	}
}
#endif

static __rte_always_inline int
qat_sym_process_response(void **op, uint8_t *resp, void *op_cookie,
		uint64_t *dequeue_err_count __rte_unused)
{
	struct icp_qat_fw_comn_resp *resp_msg =
			(struct icp_qat_fw_comn_resp *)resp;
	struct rte_crypto_op *rx_op = (struct rte_crypto_op *)(uintptr_t)
			(resp_msg->opaque_data);
	struct qat_sym_session *sess;
	uint8_t is_docsis_sec;

#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
	QAT_DP_HEXDUMP_LOG(DEBUG, "qat_response:", (uint8_t *)resp_msg,
			sizeof(struct icp_qat_fw_comn_resp));
#endif

#ifdef RTE_LIB_SECURITY
	if (rx_op->sess_type == RTE_CRYPTO_OP_SECURITY_SESSION) {
		/*
		 * Assuming at this point that if it's a security
		 * op, that this is for DOCSIS
		 */
		sess = (struct qat_sym_session *)
				get_sec_session_private_data(
				rx_op->sym->sec_session);
		is_docsis_sec = 1;
	} else
#endif
	{
		sess = (struct qat_sym_session *)
				get_sym_session_private_data(
				rx_op->sym->session,
				qat_sym_driver_id);
		is_docsis_sec = 0;
	}

	if (ICP_QAT_FW_COMN_STATUS_FLAG_OK !=
			ICP_QAT_FW_COMN_RESP_CRYPTO_STAT_GET(
			resp_msg->comn_hdr.comn_status)) {

		rx_op->status = RTE_CRYPTO_OP_STATUS_AUTH_FAILED;
	} else {
		rx_op->status = RTE_CRYPTO_OP_STATUS_SUCCESS;

		if (sess->bpi_ctx) {
			qat_bpicipher_postprocess(sess, rx_op);
#ifdef RTE_LIB_SECURITY
			if (is_docsis_sec)
				qat_crc_verify(sess, rx_op);
#endif
		}
	}

	if (sess->is_single_pass_gmac) {
		struct qat_sym_op_cookie *cookie =
				(struct qat_sym_op_cookie *) op_cookie;
		memset(cookie->opt.spc_gmac.cd_cipher.key, 0,
				sess->auth_key_length);
	}

	*op = (void *)rx_op;

	/*
	 * return 1 as dequeue op only move on to the next op
	 * if one was ready to return to API
	 */
	return 1;
}

int
qat_sym_configure_dp_ctx(struct rte_cryptodev *dev, uint16_t qp_id,
	struct rte_crypto_raw_dp_ctx *raw_dp_ctx,
	enum rte_crypto_op_sess_type sess_type,
	union rte_cryptodev_session_ctx session_ctx, uint8_t is_update);

int
qat_sym_get_dp_ctx_size(struct rte_cryptodev *dev);

void
qat_sym_init_op_cookie(void *cookie);

#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
static __rte_always_inline void
qat_sym_debug_log_dump(struct icp_qat_fw_la_bulk_req *qat_req,
		struct qat_sym_session *ctx,
		struct rte_crypto_vec *vec, uint32_t vec_len,
		struct rte_crypto_va_iova_ptr *cipher_iv,
		struct rte_crypto_va_iova_ptr *auth_iv,
		struct rte_crypto_va_iova_ptr *aad,
		struct rte_crypto_va_iova_ptr *digest)
{
	uint32_t i;

	QAT_DP_HEXDUMP_LOG(DEBUG, "qat_req:", qat_req,
			sizeof(struct icp_qat_fw_la_bulk_req));
	for (i = 0; i < vec_len; i++)
		QAT_DP_HEXDUMP_LOG(DEBUG, "src_data:", vec[i].base, vec[i].len);
	if (cipher_iv && ctx->cipher_iv.length > 0)
		QAT_DP_HEXDUMP_LOG(DEBUG, "cipher iv:", cipher_iv->va,
				ctx->cipher_iv.length);
	if (auth_iv && ctx->auth_iv.length > 0)
		QAT_DP_HEXDUMP_LOG(DEBUG, "auth iv:", auth_iv->va,
				ctx->auth_iv.length);
	if (aad && ctx->aad_len > 0)
		QAT_DP_HEXDUMP_LOG(DEBUG, "aad:", aad->va,
				ctx->aad_len);
	if (digest && ctx->digest_length > 0)
		QAT_DP_HEXDUMP_LOG(DEBUG, "digest:", digest->va,
				ctx->digest_length);
}
#else
static __rte_always_inline void
qat_sym_debug_log_dump(struct icp_qat_fw_la_bulk_req *qat_req __rte_unused,
		struct qat_sym_session *ctx __rte_unused,
		struct rte_crypto_vec *vec __rte_unused,
		uint32_t vec_len __rte_unused,
		struct rte_crypto_va_iova_ptr *cipher_iv __rte_unused,
		struct rte_crypto_va_iova_ptr *auth_iv __rte_unused,
		struct rte_crypto_va_iova_ptr *aad __rte_unused,
		struct rte_crypto_va_iova_ptr *digest __rte_unused)
{
}
#endif

#else

static inline void
qat_sym_preprocess_requests(void **ops __rte_unused,
				uint16_t nb_ops __rte_unused)
{
}

static inline void
qat_sym_process_response(void **op __rte_unused, uint8_t *resp __rte_unused,
	void *op_cookie __rte_unused)
{
}

#endif /* BUILD_QAT_SYM */
#endif /* _QAT_SYM_H_ */
