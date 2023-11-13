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


static struct rte_cryptodev_capabilities qat_sym_crypto_legacy_caps_gen3[] = {
	QAT_SYM_CIPHER_CAP(3DES_CBC,
		CAP_SET(block_size, 8),
		CAP_RNG(key_size, 8, 24, 8), CAP_RNG(iv_size, 8, 8, 0)),
	QAT_SYM_CIPHER_CAP(DES_CBC,
		CAP_SET(block_size, 8),
		CAP_RNG(key_size, 8, 24, 8), CAP_RNG(iv_size, 8, 8, 0)),
	QAT_SYM_CIPHER_CAP(3DES_CTR,
		CAP_SET(block_size, 8),
		CAP_RNG(key_size, 16, 24, 8), CAP_RNG(iv_size, 8, 8, 0)),
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
	QAT_SYM_AUTH_CAP(MD5_HMAC,
		CAP_SET(block_size, 64),
		CAP_RNG(key_size, 1, 64, 1), CAP_RNG(digest_size, 1, 16, 1),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_CIPHER_CAP(DES_DOCSISBPI,
		CAP_SET(block_size, 8),
		CAP_RNG(key_size, 8, 8, 0), CAP_RNG(iv_size, 8, 8, 0)),
	QAT_SYM_PLAIN_AUTH_CAP(SHA3_224,
		CAP_SET(block_size, 144),
		CAP_RNG(digest_size, 28, 28, 0)),
	QAT_SYM_CIPHER_CAP(SM4_ECB,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 16, 0), CAP_RNG(iv_size, 0, 0, 0))
};

static struct rte_cryptodev_capabilities qat_sym_crypto_caps_gen3[] = {
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
	QAT_SYM_AUTH_CAP(AES_CMAC,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 16, 0), CAP_RNG(digest_size, 4, 16, 4),
			CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
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
	QAT_SYM_PLAIN_AUTH_CAP(SHA3_256,
		CAP_SET(block_size, 136),
		CAP_RNG(digest_size, 32, 32, 0)),
	QAT_SYM_PLAIN_AUTH_CAP(SHA3_384,
		CAP_SET(block_size, 104),
		CAP_RNG(digest_size, 48, 48, 0)),
	QAT_SYM_PLAIN_AUTH_CAP(SHA3_512,
		CAP_SET(block_size, 72),
		CAP_RNG(digest_size, 64, 64, 0)),
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
	QAT_SYM_AUTH_CAP(SNOW3G_UIA2,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 16, 0), CAP_RNG(digest_size, 4, 4, 0),
		CAP_RNG_ZERO(aad_size), CAP_RNG(iv_size, 16, 16, 0)),
	QAT_SYM_AUTH_CAP(KASUMI_F9,
		CAP_SET(block_size, 8),
		CAP_RNG(key_size, 16, 16, 0), CAP_RNG(digest_size, 4, 4, 0),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_AUTH_CAP(NULL,
		CAP_SET(block_size, 1),
		CAP_RNG_ZERO(key_size), CAP_RNG_ZERO(digest_size),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_CIPHER_CAP(AES_CBC,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 32, 8), CAP_RNG(iv_size, 16, 16, 0)),
	QAT_SYM_CIPHER_CAP(AES_CTR,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 32, 8), CAP_RNG(iv_size, 16, 16, 0)),
	QAT_SYM_CIPHER_CAP(AES_XTS,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 32, 64, 32), CAP_RNG(iv_size, 16, 16, 0)),
	QAT_SYM_CIPHER_CAP(AES_DOCSISBPI,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 32, 16), CAP_RNG(iv_size, 16, 16, 0)),
	QAT_SYM_CIPHER_CAP(SNOW3G_UEA2,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 16, 0), CAP_RNG(iv_size, 16, 16, 0)),
	QAT_SYM_CIPHER_CAP(KASUMI_F8,
		CAP_SET(block_size, 8),
		CAP_RNG(key_size, 16, 16, 0), CAP_RNG(iv_size, 8, 8, 0)),
	QAT_SYM_CIPHER_CAP(NULL,
		CAP_SET(block_size, 1),
		CAP_RNG_ZERO(key_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_CIPHER_CAP(ZUC_EEA3,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 16, 0), CAP_RNG(iv_size, 16, 16, 0)),
	QAT_SYM_AUTH_CAP(ZUC_EIA3,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 16, 0), CAP_RNG(digest_size, 4, 4, 0),
		CAP_RNG_ZERO(aad_size), CAP_RNG(iv_size, 16, 16, 0)),
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

static int
check_cipher_capa(const struct rte_cryptodev_capabilities *cap,
		enum rte_crypto_cipher_algorithm algo)
{
	if (cap->op != RTE_CRYPTO_OP_TYPE_SYMMETRIC)
		return 0;
	if (cap->sym.xform_type != RTE_CRYPTO_SYM_XFORM_CIPHER)
		return 0;
	if (cap->sym.cipher.algo != algo)
		return 0;
	return 1;
}

static int
check_auth_capa(const struct rte_cryptodev_capabilities *cap,
		enum rte_crypto_auth_algorithm algo)
{
	if (cap->op != RTE_CRYPTO_OP_TYPE_SYMMETRIC)
		return 0;
	if (cap->sym.xform_type != RTE_CRYPTO_SYM_XFORM_AUTH)
		return 0;
	if (cap->sym.auth.algo != algo)
		return 0;
	return 1;
}

static int
qat_sym_crypto_cap_get_gen3(struct qat_cryptodev_private *internals,
			const char *capa_memz_name, const uint16_t slice_map)
{

	uint32_t i, iter = 0;
	uint32_t curr_capa = 0;
	uint32_t capa_num, legacy_capa_num;
	uint32_t size = sizeof(qat_sym_crypto_caps_gen3);
	uint32_t legacy_size = sizeof(qat_sym_crypto_legacy_caps_gen3);
	capa_num = size/sizeof(struct rte_cryptodev_capabilities);
	legacy_capa_num = legacy_size/sizeof(struct rte_cryptodev_capabilities);

	if (unlikely(qat_legacy_capa))
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

	if (unlikely(qat_legacy_capa)) {
		capabilities = qat_sym_crypto_legacy_caps_gen3;
		capa_num += legacy_capa_num;
	} else {
		capabilities = qat_sym_crypto_caps_gen3;
	}

	for (i = 0; i < capa_num; i++, iter++) {
		if (unlikely(qat_legacy_capa) && (i == legacy_capa_num)) {
			capabilities = qat_sym_crypto_caps_gen3;
			addr += curr_capa;
			curr_capa = 0;
			iter = 0;
		}

		if (slice_map & ICP_ACCEL_MASK_SM4_SLICE && (
			check_cipher_capa(&capabilities[iter],
				RTE_CRYPTO_CIPHER_SM4_ECB) ||
			check_cipher_capa(&capabilities[iter],
				RTE_CRYPTO_CIPHER_SM4_CBC) ||
			check_cipher_capa(&capabilities[iter],
				RTE_CRYPTO_CIPHER_SM4_CTR))) {
			continue;
		}
		if (slice_map & ICP_ACCEL_MASK_SM3_SLICE && (
			check_auth_capa(&capabilities[iter],
				RTE_CRYPTO_AUTH_SM3) ||
			check_auth_capa(&capabilities[iter],
				RTE_CRYPTO_AUTH_SM3_HMAC))) {
			continue;
		}
		memcpy(addr + curr_capa, capabilities + iter,
			sizeof(struct rte_cryptodev_capabilities));
		curr_capa++;
	}
	internals->qat_dev_capabilities = internals->capa_mz->addr;

	return 0;
}

static __rte_always_inline void
enqueue_one_aead_job_gen3(struct qat_sym_session *ctx,
	struct icp_qat_fw_la_bulk_req *req,
	struct rte_crypto_va_iova_ptr *iv,
	struct rte_crypto_va_iova_ptr *digest,
	struct rte_crypto_va_iova_ptr *aad,
	union rte_crypto_sym_ofs ofs, uint32_t data_len)
{
	if (ctx->is_single_pass) {
		struct icp_qat_fw_la_cipher_req_params *cipher_param =
			(void *)&req->serv_specif_rqpars;

		/* QAT GEN3 uses single pass to treat AEAD as
		 * cipher operation
		 */
		cipher_param = (void *)&req->serv_specif_rqpars;

		qat_set_cipher_iv(cipher_param, iv, ctx->cipher_iv.length, req);
		cipher_param->cipher_offset = ofs.ofs.cipher.head;
		cipher_param->cipher_length = data_len - ofs.ofs.cipher.head -
				ofs.ofs.cipher.tail;

		cipher_param->spc_aad_addr = aad->iova;
		cipher_param->spc_auth_res_addr = digest->iova;

		return;
	}

	enqueue_one_aead_job_gen1(ctx, req, iv, digest, aad, ofs, data_len);
}

static __rte_always_inline void
enqueue_one_auth_job_gen3(struct qat_sym_session *ctx,
	struct qat_sym_op_cookie *cookie,
	struct icp_qat_fw_la_bulk_req *req,
	struct rte_crypto_va_iova_ptr *digest,
	struct rte_crypto_va_iova_ptr *auth_iv,
	union rte_crypto_sym_ofs ofs, uint32_t data_len)
{
	struct icp_qat_fw_cipher_cd_ctrl_hdr *cipher_cd_ctrl;
	struct icp_qat_fw_la_cipher_req_params *cipher_param;
	uint32_t ver_key_offset;
	uint32_t auth_data_len = data_len - ofs.ofs.auth.head -
			ofs.ofs.auth.tail;

	if (!ctx->is_single_pass_gmac ||
			(auth_data_len > QAT_AES_GMAC_SPC_MAX_SIZE)) {
		enqueue_one_auth_job_gen1(ctx, req, digest, auth_iv, ofs,
				data_len);
		return;
	}

	cipher_cd_ctrl = (void *) &req->cd_ctrl;
	cipher_param = (void *)&req->serv_specif_rqpars;
	ver_key_offset = sizeof(struct icp_qat_hw_auth_setup) +
			ICP_QAT_HW_GALOIS_128_STATE1_SZ +
			ICP_QAT_HW_GALOIS_H_SZ + ICP_QAT_HW_GALOIS_LEN_A_SZ +
			ICP_QAT_HW_GALOIS_E_CTR0_SZ +
			sizeof(struct icp_qat_hw_cipher_config);

	if (ctx->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_GALOIS_128 ||
		ctx->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_GALOIS_64) {
		/* AES-GMAC */
		qat_set_cipher_iv(cipher_param, auth_iv, ctx->auth_iv.length,
				req);
	}

	/* Fill separate Content Descriptor for this op */
	rte_memcpy(cookie->opt.spc_gmac.cd_cipher.key,
			ctx->auth_op == ICP_QAT_HW_AUTH_GENERATE ?
				ctx->cd.cipher.key :
				RTE_PTR_ADD(&ctx->cd, ver_key_offset),
			ctx->auth_key_length);
	cookie->opt.spc_gmac.cd_cipher.cipher_config.val =
			ICP_QAT_HW_CIPHER_CONFIG_BUILD(
				ICP_QAT_HW_CIPHER_AEAD_MODE,
				ctx->qat_cipher_alg,
				ICP_QAT_HW_CIPHER_NO_CONVERT,
				(ctx->auth_op == ICP_QAT_HW_AUTH_GENERATE ?
					ICP_QAT_HW_CIPHER_ENCRYPT :
					ICP_QAT_HW_CIPHER_DECRYPT));
	QAT_FIELD_SET(cookie->opt.spc_gmac.cd_cipher.cipher_config.val,
			ctx->digest_length,
			QAT_CIPHER_AEAD_HASH_CMP_LEN_BITPOS,
			QAT_CIPHER_AEAD_HASH_CMP_LEN_MASK);
	cookie->opt.spc_gmac.cd_cipher.cipher_config.reserved =
			ICP_QAT_HW_CIPHER_CONFIG_BUILD_UPPER(auth_data_len);

	/* Update the request */
	req->cd_pars.u.s.content_desc_addr =
			cookie->opt.spc_gmac.cd_phys_addr;
	req->cd_pars.u.s.content_desc_params_sz = RTE_ALIGN_CEIL(
			sizeof(struct icp_qat_hw_cipher_config) +
			ctx->auth_key_length, 8) >> 3;
	req->comn_mid.src_length = data_len;
	req->comn_mid.dst_length = 0;

	cipher_param->spc_aad_addr = 0;
	cipher_param->spc_auth_res_addr = digest->iova;
	cipher_param->spc_aad_sz = auth_data_len;
	cipher_param->reserved = 0;
	cipher_param->spc_auth_res_sz = ctx->digest_length;

	req->comn_hdr.service_cmd_id = ICP_QAT_FW_LA_CMD_CIPHER;
	cipher_cd_ctrl->cipher_cfg_offset = 0;
	ICP_QAT_FW_COMN_CURR_ID_SET(cipher_cd_ctrl, ICP_QAT_FW_SLICE_CIPHER);
	ICP_QAT_FW_COMN_NEXT_ID_SET(cipher_cd_ctrl, ICP_QAT_FW_SLICE_DRAM_WR);
	ICP_QAT_FW_LA_SINGLE_PASS_PROTO_FLAG_SET(
			req->comn_hdr.serv_specif_flags,
			ICP_QAT_FW_LA_SINGLE_PASS_PROTO);
	ICP_QAT_FW_LA_PROTO_SET(
			req->comn_hdr.serv_specif_flags,
			ICP_QAT_FW_LA_NO_PROTO);
}

static int
qat_sym_build_op_aead_gen3(void *in_op, struct qat_sym_session *ctx,
		uint8_t *out_msg, void *op_cookie)
{
	register struct icp_qat_fw_la_bulk_req *req;
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

	req = (struct icp_qat_fw_la_bulk_req *)out_msg;
	rte_mov128((uint8_t *)req, (const uint8_t *)&(ctx->fw_req));

	ofs.raw = qat_sym_convert_op_to_vec_aead(op, ctx, &in_sgl, &out_sgl,
			&cipher_iv, &aad, &digest);
	if (unlikely(ofs.raw == UINT64_MAX)) {
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		return -EINVAL;
	}

	total_len = qat_sym_build_req_set_data(req, in_op, cookie,
			in_sgl.vec, in_sgl.num, out_sgl.vec, out_sgl.num);
	if (unlikely(total_len < 0)) {
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		return -EINVAL;
	}

	enqueue_one_aead_job_gen3(ctx, req, &cipher_iv, &digest, &aad, ofs,
		total_len);

	qat_sym_debug_log_dump(req, ctx, in_sgl.vec, in_sgl.num, &cipher_iv,
			NULL, &aad, &digest);

	return 0;
}

static int
qat_sym_build_op_auth_gen3(void *in_op, struct qat_sym_session *ctx,
		uint8_t *out_msg, void *op_cookie)
{
	register struct icp_qat_fw_la_bulk_req *req;
	struct rte_crypto_op *op = in_op;
	struct qat_sym_op_cookie *cookie = op_cookie;
	struct rte_crypto_sgl in_sgl, out_sgl;
	struct rte_crypto_vec in_vec[QAT_SYM_SGL_MAX_NUMBER],
			out_vec[QAT_SYM_SGL_MAX_NUMBER];
	struct rte_crypto_va_iova_ptr auth_iv;
	struct rte_crypto_va_iova_ptr digest;
	union rte_crypto_sym_ofs ofs;
	int32_t total_len;

	in_sgl.vec = in_vec;
	out_sgl.vec = out_vec;

	req = (struct icp_qat_fw_la_bulk_req *)out_msg;
	rte_mov128((uint8_t *)req, (const uint8_t *)&(ctx->fw_req));

	ofs.raw = qat_sym_convert_op_to_vec_auth(op, ctx, &in_sgl, &out_sgl,
			NULL, &auth_iv, &digest, op_cookie);
	if (unlikely(ofs.raw == UINT64_MAX)) {
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		return -EINVAL;
	}

	total_len = qat_sym_build_req_set_data(req, in_op, cookie,
			in_sgl.vec, in_sgl.num, out_sgl.vec, out_sgl.num);
	if (unlikely(total_len < 0)) {
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		return -EINVAL;
	}

	enqueue_one_auth_job_gen3(ctx, cookie, req, &digest, &auth_iv,
			ofs, total_len);

	qat_sym_debug_log_dump(req, ctx, in_sgl.vec, in_sgl.num, NULL,
			&auth_iv, NULL, &digest);

	return 0;
}

static int
qat_sym_crypto_set_session_gen3(void *cdev __rte_unused, void *session)
{
	struct qat_sym_session *ctx = session;
	enum rte_proc_type_t proc_type = rte_eal_process_type();
	int ret;

	if (proc_type == RTE_PROC_AUTO || proc_type == RTE_PROC_INVALID)
		return -EINVAL;

	ret = qat_sym_crypto_set_session_gen1(cdev, session);
	/* special single pass build request for GEN3 */
	if (ctx->is_single_pass)
		ctx->build_request[proc_type] = qat_sym_build_op_aead_gen3;
	else if (ctx->is_single_pass_gmac)
		ctx->build_request[proc_type] = qat_sym_build_op_auth_gen3;

	if (ret == -ENOTSUP) {
		/* GEN1 returning -ENOTSUP as it cannot handle some mixed algo,
		 * this is addressed by GEN3
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
qat_sym_dp_enqueue_single_aead_gen3(void *qp_data, uint8_t *drv_ctx,
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

	enqueue_one_aead_job_gen3(ctx, req, iv, digest, aad, ofs,
		(uint32_t)data_len);

	dp_ctx->tail = tail;
	dp_ctx->cached_enqueue++;

	qat_sym_debug_log_dump(req, ctx, data, n_data_vecs, iv,
			NULL, aad, digest);

	return 0;
}

static uint32_t
qat_sym_dp_enqueue_aead_jobs_gen3(void *qp_data, uint8_t *drv_ctx,
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

		req  = (struct icp_qat_fw_la_bulk_req *)(
			(uint8_t *)tx_queue->base_addr + tail);
		rte_mov128((uint8_t *)req, (const uint8_t *)&(ctx->fw_req));

		if (vec->dest_sgl) {
			data_len = qat_sym_build_req_set_data(req,
				user_data[i], cookie,
				vec->src_sgl[i].vec, vec->src_sgl[i].num,
				vec->dest_sgl[i].vec, vec->dest_sgl[i].num);
		} else {
			data_len = qat_sym_build_req_set_data(req,
				user_data[i], cookie,
				vec->src_sgl[i].vec,
				vec->src_sgl[i].num, NULL, 0);
		}

		if (unlikely(data_len < 0))
			break;

		enqueue_one_aead_job_gen3(ctx, req, &vec->iv[i],
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

static int
qat_sym_dp_enqueue_single_auth_gen3(void *qp_data, uint8_t *drv_ctx,
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
	struct qat_sym_op_cookie *cookie;
	struct qat_sym_session *ctx = dp_ctx->session;
	struct icp_qat_fw_la_bulk_req *req;
	int32_t data_len;
	uint32_t tail = dp_ctx->tail;
	struct rte_crypto_va_iova_ptr null_digest;
	struct rte_crypto_va_iova_ptr *job_digest = digest;

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

	if (ctx->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_NULL) {
		null_digest.iova = cookie->digest_null_phys_addr;
		job_digest = &null_digest;
	}

	enqueue_one_auth_job_gen3(ctx, cookie, req, job_digest, auth_iv, ofs,
			(uint32_t)data_len);

	dp_ctx->tail = tail;
	dp_ctx->cached_enqueue++;

	return 0;
}

static uint32_t
qat_sym_dp_enqueue_auth_jobs_gen3(void *qp_data, uint8_t *drv_ctx,
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
	struct rte_crypto_va_iova_ptr null_digest;
	struct rte_crypto_va_iova_ptr *job_digest = NULL;

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

		req  = (struct icp_qat_fw_la_bulk_req *)(
			(uint8_t *)tx_queue->base_addr + tail);
		rte_mov128((uint8_t *)req, (const uint8_t *)&(ctx->fw_req));

		if (vec->dest_sgl) {
			data_len = qat_sym_build_req_set_data(req,
				user_data[i], cookie,
				vec->src_sgl[i].vec, vec->src_sgl[i].num,
				vec->dest_sgl[i].vec, vec->dest_sgl[i].num);
		} else {
			data_len = qat_sym_build_req_set_data(req,
				user_data[i], cookie,
				vec->src_sgl[i].vec,
				vec->src_sgl[i].num, NULL, 0);
		}

		if (unlikely(data_len < 0))
			break;
		if (ctx->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_NULL) {
			null_digest.iova = cookie->digest_null_phys_addr;
			job_digest = &null_digest;
		} else
			job_digest = &vec->digest[i];

		enqueue_one_auth_job_gen3(ctx, cookie, req, job_digest,
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

static int
qat_sym_configure_raw_dp_ctx_gen3(void *_raw_dp_ctx, void *_ctx)
{
	struct rte_crypto_raw_dp_ctx *raw_dp_ctx = _raw_dp_ctx;
	struct qat_sym_session *ctx = _ctx;
	int ret;

	ret = qat_sym_configure_raw_dp_ctx_gen1(_raw_dp_ctx, _ctx);
	if (ret < 0)
		return ret;

	if (ctx->is_single_pass) {
		raw_dp_ctx->enqueue_burst = qat_sym_dp_enqueue_aead_jobs_gen3;
		raw_dp_ctx->enqueue = qat_sym_dp_enqueue_single_aead_gen3;
	} else if (ctx->is_single_pass_gmac) {
		raw_dp_ctx->enqueue_burst = qat_sym_dp_enqueue_auth_jobs_gen3;
		raw_dp_ctx->enqueue = qat_sym_dp_enqueue_single_auth_gen3;
	}

	return 0;
}


RTE_INIT(qat_sym_crypto_gen3_init)
{
	qat_sym_gen_dev_ops[QAT_GEN3].cryptodev_ops = &qat_sym_crypto_ops_gen1;
	qat_sym_gen_dev_ops[QAT_GEN3].get_capabilities =
			qat_sym_crypto_cap_get_gen3;
	qat_sym_gen_dev_ops[QAT_GEN3].get_feature_flags =
			qat_sym_crypto_feature_flags_get_gen1;
	qat_sym_gen_dev_ops[QAT_GEN3].set_session =
			qat_sym_crypto_set_session_gen3;
	qat_sym_gen_dev_ops[QAT_GEN3].set_raw_dp_ctx =
			qat_sym_configure_raw_dp_ctx_gen3;
	qat_sym_gen_dev_ops[QAT_GEN3].create_security_ctx =
			qat_sym_create_security_gen1;
}

RTE_INIT(qat_asym_crypto_gen3_init)
{
	qat_asym_gen_dev_ops[QAT_GEN3].cryptodev_ops =
			&qat_asym_crypto_ops_gen1;
	qat_asym_gen_dev_ops[QAT_GEN3].get_capabilities =
			qat_asym_crypto_cap_get_gen1;
	qat_asym_gen_dev_ops[QAT_GEN3].get_feature_flags =
			qat_asym_crypto_feature_flags_get_gen1;
	qat_asym_gen_dev_ops[QAT_GEN3].set_session =
			qat_asym_crypto_set_session_gen1;
}
