/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Intel Corporation
 */

#include <rte_cryptodev.h>
#include <cryptodev_pmd.h>
#include "qat_sym_session.h"
#include "qat_sym.h"
#include "qat_asym.h"
#include "qat_crypto.h"
#include "qat_crypto_pmd_gens.h"


static struct rte_cryptodev_capabilities qat_sym_crypto_legacy_caps_gen6[] = {
	QAT_SYM_PLAIN_AUTH_CAP(SHA1,
		CAP_SET(block_size, 64),
		CAP_RNG(digest_size, 1, 20, 1)),
	QAT_SYM_AUTH_CAP(SHA224,
		CAP_SET(block_size, 64),
		CAP_RNG_ZERO(key_size), CAP_RNG(digest_size, 1, 28, 1),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
};

static struct rte_cryptodev_capabilities qat_sym_crypto_caps_gen6[] = {
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
		CAP_RNG(key_size, 16, 32, 16), CAP_RNG(iv_size, 16, 16, 0)),
	QAT_SYM_AEAD_CAP(AES_GCM,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 32, 16), CAP_RNG(digest_size, 8, 16, 4),
		CAP_RNG(aad_size, 0, 240, 1), CAP_RNG(iv_size, 12, 12, 12)),
	QAT_SYM_AEAD_CAP(CHACHA20_POLY1305,
		CAP_SET(block_size, 64),
		CAP_RNG(key_size, 32, 32, 0),
		CAP_RNG(digest_size, 16, 16, 0),
		CAP_RNG(aad_size, 0, 240, 1), CAP_RNG(iv_size, 12, 12, 0)),
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

static int
qat_sym_crypto_cap_get_gen6(struct qat_cryptodev_private *internals,
			const char *capa_memz_name,
			const uint16_t __rte_unused slice_map)
{
	uint32_t caps_size = sizeof(qat_sym_crypto_caps_gen6);
	uint32_t legacy_caps_size = sizeof(qat_sym_crypto_legacy_caps_gen6);
	uint32_t size = caps_size;
	uintptr_t cp_addr;

	if (internals->qat_dev->options.legacy_alg)
		size = size + legacy_caps_size;

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
	cp_addr = (uintptr_t)(internals->capa_mz->addr);

	if (internals->qat_dev->options.legacy_alg) {
		memcpy((void *)cp_addr, qat_sym_crypto_legacy_caps_gen6, legacy_caps_size);
		cp_addr += legacy_caps_size;
	}

	memcpy((void *)cp_addr, qat_sym_crypto_caps_gen6, caps_size);
	internals->qat_dev_capabilities = internals->capa_mz->addr;

	return 0;
}

static int get_block_size(enum icp_qat_hw_auth_algo qat_hash_alg)
{
	switch (qat_hash_alg) {
	case ICP_QAT_HW_AUTH_ALGO_SHA1:
	case ICP_QAT_HW_AUTH_ALGO_SHA224:
	case ICP_QAT_HW_AUTH_ALGO_SHA256:
		return QAT_SHA_CBLOCK;
	case ICP_QAT_HW_AUTH_ALGO_SHA384:
	case ICP_QAT_HW_AUTH_ALGO_SHA512:
		return QAT_SHA512_CBLOCK;
	default:
		return -EFAULT;
	};
	return -EFAULT;
}

int
qat_sym_cd_auth_set_gen6(struct qat_sym_session *cdesc,
		const uint8_t *authkey,
		uint32_t authkeylen,
		uint32_t aad_length __rte_unused,
		uint32_t digestsize,
		unsigned int operation)
{
	struct icp_qat_hw_auth_setup *hash = NULL;
	struct icp_qat_fw_la_bulk_req *req_tmpl = &cdesc->fw_req;
	struct icp_qat_fw_comn_req_hdr_cd_pars *cd_pars = &req_tmpl->cd_pars;
	void *ptr = &req_tmpl->cd_ctrl;
	struct icp_qat_fw_auth_cd_ctrl_hdr *hash_cd_ctrl = ptr;
	struct icp_qat_fw_la_auth_req_params *auth_param =
		(struct icp_qat_fw_la_auth_req_params *)
		((char *)&req_tmpl->serv_specif_rqpars +
		ICP_QAT_FW_HASH_REQUEST_PARAMETERS_OFFSET);
	enum icp_qat_hw_auth_mode auth_mode =
			(cdesc->auth_mode == ICP_QAT_HW_AUTH_MODE0) ?
			ICP_QAT_HW_AUTH_MODE0 : ICP_QAT_HW_AUTH_MODE2;
	uint16_t state1_size = 0, state2_size = 0, cd_extra_size = 0;
	uint16_t hash_offset;

	if (cdesc->qat_cmd == ICP_QAT_FW_LA_CMD_AUTH) {
		ICP_QAT_FW_COMN_CURR_ID_SET(hash_cd_ctrl,
					ICP_QAT_FW_SLICE_AUTH);
		cdesc->cd_cur_ptr = (uint8_t *)&cdesc->cd;
	} else if (cdesc->qat_cmd != ICP_QAT_FW_LA_CMD_CIPHER_HASH) {
		QAT_LOG(ERR, "Invalid param, must be a hash command.");
		return -EFAULT;
	}

	if (operation == RTE_CRYPTO_AUTH_OP_VERIFY)
		cdesc->auth_op = ICP_QAT_HW_AUTH_VERIFY;
	else
		cdesc->auth_op = ICP_QAT_HW_AUTH_GENERATE;

	/*
	 * Setup the inner hash config
	 */
	hash_offset = cdesc->cd_cur_ptr-((uint8_t *)&cdesc->cd);
	hash = (struct icp_qat_hw_auth_setup *)cdesc->cd_cur_ptr;
	hash->auth_config.reserved = 0;
	if (cdesc->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_NULL)
		hash->auth_config.config =
			ICP_QAT_HW_AUTH_CONFIG_BUILD(auth_mode,
				cdesc->qat_hash_alg, 4);
	else
		hash->auth_config.config =
			ICP_QAT_HW_AUTH_CONFIG_BUILD(auth_mode,
				cdesc->qat_hash_alg, digestsize);

	if (cdesc->auth_mode == ICP_QAT_HW_AUTH_MODE0) {
		hash->auth_counter.counter = 0;
	} else {
		int block_size = get_block_size(cdesc->qat_hash_alg);
		if (block_size < 0)
			return block_size;
		hash->auth_counter.counter = rte_bswap32(block_size);
	}

	hash_cd_ctrl->hash_cfg_offset = hash_offset >> 3;
	cdesc->cd_cur_ptr += sizeof(struct icp_qat_hw_auth_setup);
	switch (cdesc->qat_hash_alg) {
	case ICP_QAT_HW_AUTH_ALGO_SHA1:
	case ICP_QAT_HW_AUTH_ALGO_SHA224:
		break;
	case ICP_QAT_HW_AUTH_ALGO_SHA256:
		/* HMAC */
		if (cdesc->auth_mode != ICP_QAT_HW_AUTH_MODE0) {
			struct icp_qat_fw_la_cipher_20_req_params *cipher_param_20 =
				(void *)&req_tmpl->serv_specif_rqpars;
			rte_memcpy(cdesc->cd_cur_ptr, authkey, authkeylen);
			state2_size = 0;
			state1_size = authkeylen;
			ICP_QAT_FW_HASH_FLAG_MODE2_SET(
				hash_cd_ctrl->hash_flags, QAT_FW_LA_MODE2);
			cipher_param_20->spc_aad_addr = cdesc->cd_paddr +
				(cdesc->cd_cur_ptr - (uint8_t *)&cdesc->cd);
			cipher_param_20->hmac_sz = ICP_QAT_HW_SHA256_STATE2_SZ * 2;
		}
		break;
	case ICP_QAT_HW_AUTH_ALGO_SHA384:
		/* HMAC */
		if (cdesc->auth_mode != ICP_QAT_HW_AUTH_MODE0) {
			struct icp_qat_fw_la_cipher_20_req_params *cipher_param_20 =
				(void *)&req_tmpl->serv_specif_rqpars;
			rte_memcpy(cdesc->cd_cur_ptr, authkey, authkeylen);
			state2_size = 0;
			state1_size = authkeylen;
			ICP_QAT_FW_HASH_FLAG_MODE2_SET(
				hash_cd_ctrl->hash_flags, QAT_FW_LA_MODE2);
			cipher_param_20->spc_aad_addr = cdesc->cd_paddr +
				(cdesc->cd_cur_ptr - (uint8_t *)&cdesc->cd);
			cipher_param_20->hmac_sz = ICP_QAT_HW_SHA384_STATE2_SZ * 2;
		}
		break;
	case ICP_QAT_HW_AUTH_ALGO_SHA512:
		/* HMAC */
		if (cdesc->auth_mode != ICP_QAT_HW_AUTH_MODE0) {
			struct icp_qat_fw_la_cipher_20_req_params *cipher_param_20 =
				(void *)&req_tmpl->serv_specif_rqpars;
			rte_memcpy(cdesc->cd_cur_ptr, authkey, authkeylen);
			state2_size = 0;
			state1_size = authkeylen;
			ICP_QAT_FW_HASH_FLAG_MODE2_SET(
				hash_cd_ctrl->hash_flags, QAT_FW_LA_MODE2);
			cipher_param_20->spc_aad_addr = cdesc->cd_paddr +
				(cdesc->cd_cur_ptr - (uint8_t *)&cdesc->cd);
			cipher_param_20->hmac_sz = ICP_QAT_HW_SHA512_STATE2_SZ * 2;
		}
		break;
	case ICP_QAT_HW_AUTH_ALGO_NULL:
		break;
	default:
		QAT_LOG(ERR, "Invalid HASH alg %u", cdesc->qat_hash_alg);
		return -EFAULT;
	}

	/* Auth CD config setup */
	hash_cd_ctrl->hash_flags |= ICP_QAT_FW_AUTH_HDR_FLAG_NO_NESTED;
	hash_cd_ctrl->inner_state1_sz = state1_size;
	if (cdesc->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_NULL) {
		hash_cd_ctrl->inner_res_sz = 4;
		hash_cd_ctrl->final_sz = 4;
		auth_param->auth_res_sz = 4;
	} else {
		hash_cd_ctrl->inner_res_sz = digestsize;
		hash_cd_ctrl->final_sz = digestsize;
		auth_param->auth_res_sz = digestsize;
	}

	hash_cd_ctrl->inner_state2_sz  = state2_size;
	hash_cd_ctrl->inner_state2_offset = hash_cd_ctrl->hash_cfg_offset +
			((sizeof(struct icp_qat_hw_auth_setup) +
			 RTE_ALIGN_CEIL(hash_cd_ctrl->inner_state1_sz, 8))
					>> 3);
	cdesc->cd_cur_ptr += state1_size + state2_size + cd_extra_size;
	cd_pars->u.s.content_desc_addr = cdesc->cd_paddr;
	cd_pars->u.s.content_desc_params_sz = 1;
	return 0;
}

static int
qat_sym_build_op_auth_gen6(void *in_op, struct qat_sym_session *ctx,
	uint8_t *out_msg, void *op_cookie)
{
	struct rte_crypto_op *op = in_op;
	if (unlikely(op->sym->m_dst != NULL &&
		op->sym->m_dst != op->sym->m_src)) {
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		return -EINVAL;
	}
	return qat_sym_build_op_auth_gen1(in_op, ctx, out_msg, op_cookie);
}

static int
qat_sym_crypto_set_session_gen6(void *cdev, void *session)
{
	struct qat_sym_session *ctx = session;
	enum rte_proc_type_t proc_type = rte_eal_process_type();
	int ret;

	if (proc_type == RTE_PROC_AUTO || proc_type == RTE_PROC_INVALID)
		return -EINVAL;

	ret = qat_sym_crypto_set_session_gen4(cdev, session);

	if (ret == 0) {
		if (ctx->build_request[proc_type] ==
			qat_sym_build_op_chain_gen1) {
			/* Gen 6 does not support chaining of symmetric crypto ops. */
			QAT_LOG(ERR, "Chained ops not supported");
			return -ENOTSUP;
		} else if (ctx->build_request[proc_type] ==
			qat_sym_build_op_auth_gen1) {
			/* Gen 6 does not support OOP auth ops. */
			ctx->build_request[proc_type] = qat_sym_build_op_auth_gen6;
		}
	}

	return ret;
}

static uint32_t
qat_sym_dp_enqueue_auth_jobs_gen6(void *qp_data, uint8_t *drv_ctx,
		struct rte_crypto_sym_vec *vec, union rte_crypto_sym_ofs ofs,
		void *user_data[], int *status)
{
	if (vec->dest_sgl != NULL) {
		qat_sym_dp_fill_vec_status(vec->status, ENOTSUP, vec->num);
		*status = 0;
		return 0;
	}

	return qat_sym_dp_enqueue_auth_jobs_gen1(qp_data, drv_ctx, vec, ofs,
			user_data, status);
}

static int
qat_sym_configure_raw_dp_ctx_gen6(void *raw_dp_ctx, void *ctx)
{
	struct qat_sym_session *session = ctx;
	struct rte_crypto_raw_dp_ctx *raw_ctx = raw_dp_ctx;
	int ret;

	ret = qat_sym_configure_raw_dp_ctx_gen4(raw_dp_ctx, ctx);
	if (ret != 0)
		return ret;

	if (session->qat_cmd == ICP_QAT_FW_LA_CMD_AUTH)
		raw_ctx->enqueue_burst = qat_sym_dp_enqueue_auth_jobs_gen6;

	return 0;
}

static uint64_t
qat_sym_crypto_feature_flags_get_gen6(struct qat_pci_device *qat_dev)
{
	/* Gen 6 does not support chaining of symmetric crypto ops. */
	return qat_sym_crypto_feature_flags_get_gen1(qat_dev) &
				~RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING;
}

RTE_INIT(qat_sym_crypto_gen6_init)
{
	qat_sym_gen_dev_ops[QAT_GEN6].cryptodev_ops = &qat_sym_crypto_ops_gen1;
	qat_sym_gen_dev_ops[QAT_GEN6].get_capabilities =
			qat_sym_crypto_cap_get_gen6;
	qat_sym_gen_dev_ops[QAT_GEN6].set_session =
			qat_sym_crypto_set_session_gen6;
	qat_sym_gen_dev_ops[QAT_GEN6].set_raw_dp_ctx =
			qat_sym_configure_raw_dp_ctx_gen6;
	qat_sym_gen_dev_ops[QAT_GEN6].get_feature_flags =
			qat_sym_crypto_feature_flags_get_gen6;
	qat_sym_gen_dev_ops[QAT_GEN6].create_security_ctx =
			qat_sym_create_security_gen1;
}

#ifdef BUILD_QAT_ASYM
RTE_INIT(qat_asym_crypto_gen6_init)
{
	qat_asym_gen_dev_ops[QAT_GEN6].cryptodev_ops =
			&qat_asym_crypto_ops_gen1;
	qat_asym_gen_dev_ops[QAT_GEN6].get_capabilities =
			qat_asym_crypto_cap_get_gen1;
	qat_asym_gen_dev_ops[QAT_GEN6].get_feature_flags =
			qat_asym_crypto_feature_flags_get_gen1;
	qat_asym_gen_dev_ops[QAT_GEN6].set_session =
			qat_asym_crypto_set_session_gen1;
}
#endif
