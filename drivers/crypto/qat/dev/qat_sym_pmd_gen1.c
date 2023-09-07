/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2022 Intel Corporation
 */

#include <rte_cryptodev.h>
#include <rte_security_driver.h>

#include "adf_transport_access_macros.h"
#include "icp_qat_fw.h"
#include "icp_qat_fw_la.h"

#include "qat_sym.h"
#include "qat_sym_session.h"
#include "qat_crypto.h"
#include "qat_crypto_pmd_gens.h"

static struct rte_cryptodev_capabilities qat_sym_crypto_legacy_caps_gen1[] = {
	QAT_SYM_CIPHER_CAP(DES_CBC,
		CAP_SET(block_size, 8),
		CAP_RNG(key_size, 8, 24, 8), CAP_RNG(iv_size, 8, 8, 0)),
	QAT_SYM_CIPHER_CAP(3DES_CBC,
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
	QAT_SYM_AUTH_CAP(SHA1_HMAC,
		CAP_SET(block_size, 64),
		CAP_RNG(key_size, 1, 64, 1), CAP_RNG(digest_size, 1, 20, 1),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_AUTH_CAP(SHA224_HMAC,
		CAP_SET(block_size, 64),
		CAP_RNG(key_size, 1, 64, 1), CAP_RNG(digest_size, 1, 28, 1),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_AUTH_CAP(MD5_HMAC,
		CAP_SET(block_size, 64),
		CAP_RNG(key_size, 1, 64, 1), CAP_RNG(digest_size, 1, 16, 1),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_CIPHER_CAP(DES_DOCSISBPI,
		CAP_SET(block_size, 8),
		CAP_RNG(key_size, 8, 8, 0), CAP_RNG(iv_size, 8, 8, 0)),
};

static struct rte_cryptodev_capabilities qat_sym_crypto_caps_gen1[] = {
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
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

struct rte_cryptodev_ops qat_sym_crypto_ops_gen1 = {

	/* Device related operations */
	.dev_configure		= qat_cryptodev_config,
	.dev_start		= qat_cryptodev_start,
	.dev_stop		= qat_cryptodev_stop,
	.dev_close		= qat_cryptodev_close,
	.dev_infos_get		= qat_cryptodev_info_get,

	.stats_get		= qat_cryptodev_stats_get,
	.stats_reset		= qat_cryptodev_stats_reset,
	.queue_pair_setup	= qat_cryptodev_qp_setup,
	.queue_pair_release	= qat_cryptodev_qp_release,

	/* Crypto related operations */
	.sym_session_get_size	= qat_sym_session_get_private_size,
	.sym_session_configure	= qat_sym_session_configure,
	.sym_session_clear	= qat_sym_session_clear,

	/* Raw data-path API related operations */
	.sym_get_raw_dp_ctx_size = qat_sym_get_dp_ctx_size,
	.sym_configure_raw_dp_ctx = qat_sym_configure_dp_ctx,
};

static int
qat_sym_crypto_cap_get_gen1(struct qat_cryptodev_private *internals,
			const char *capa_memz_name,
			const uint16_t __rte_unused slice_map)
{

	uint32_t legacy_capa_num;
	uint32_t size = sizeof(qat_sym_crypto_caps_gen1);
	uint32_t legacy_size = sizeof(qat_sym_crypto_legacy_caps_gen1);
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
		capabilities = qat_sym_crypto_legacy_caps_gen1;
		memcpy(addr, capabilities, legacy_size);
		addr += legacy_capa_num;
	}
	capabilities = qat_sym_crypto_caps_gen1;
	memcpy(addr, capabilities, sizeof(qat_sym_crypto_caps_gen1));
	internals->qat_dev_capabilities = internals->capa_mz->addr;

	return 0;
}

uint64_t
qat_sym_crypto_feature_flags_get_gen1(
	struct qat_pci_device *qat_dev __rte_unused)
{
	uint64_t feature_flags = RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO |
			RTE_CRYPTODEV_FF_HW_ACCELERATED |
			RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING |
			RTE_CRYPTODEV_FF_IN_PLACE_SGL |
			RTE_CRYPTODEV_FF_OOP_SGL_IN_SGL_OUT |
			RTE_CRYPTODEV_FF_OOP_SGL_IN_LB_OUT |
			RTE_CRYPTODEV_FF_OOP_LB_IN_SGL_OUT |
			RTE_CRYPTODEV_FF_OOP_LB_IN_LB_OUT |
			RTE_CRYPTODEV_FF_DIGEST_ENCRYPTED |
			RTE_CRYPTODEV_FF_SYM_RAW_DP;

	return feature_flags;
}

int
qat_sym_build_op_cipher_gen1(void *in_op, struct qat_sym_session *ctx,
		uint8_t *out_msg, void *op_cookie)
{
	register struct icp_qat_fw_la_bulk_req *req;
	struct rte_crypto_op *op = in_op;
	struct qat_sym_op_cookie *cookie = op_cookie;
	struct rte_crypto_sgl in_sgl, out_sgl;
	struct rte_crypto_vec in_vec[QAT_SYM_SGL_MAX_NUMBER],
			out_vec[QAT_SYM_SGL_MAX_NUMBER];
	struct rte_crypto_va_iova_ptr cipher_iv;
	union rte_crypto_sym_ofs ofs;
	int32_t total_len;

	in_sgl.vec = in_vec;
	out_sgl.vec = out_vec;

	req = (struct icp_qat_fw_la_bulk_req *)out_msg;
	rte_mov128((uint8_t *)req, (const uint8_t *)&(ctx->fw_req));

	ofs.raw = qat_sym_convert_op_to_vec_cipher(op, ctx, &in_sgl, &out_sgl,
			&cipher_iv, NULL, NULL);
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

	enqueue_one_cipher_job_gen1(ctx, req, &cipher_iv, ofs, total_len);

	qat_sym_debug_log_dump(req, ctx, in_sgl.vec, in_sgl.num, &cipher_iv,
			NULL, NULL, NULL);

	return 0;
}

int
qat_sym_build_op_auth_gen1(void *in_op, struct qat_sym_session *ctx,
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

	enqueue_one_auth_job_gen1(ctx, req, &digest, &auth_iv, ofs,
			total_len);

	qat_sym_debug_log_dump(req, ctx, in_sgl.vec, in_sgl.num, NULL,
			&auth_iv, NULL, &digest);

	return 0;
}

int
qat_sym_build_op_aead_gen1(void *in_op, struct qat_sym_session *ctx,
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

	enqueue_one_aead_job_gen1(ctx, req, &cipher_iv, &digest, &aad, ofs,
		total_len);

	qat_sym_debug_log_dump(req, ctx, in_sgl.vec, in_sgl.num, &cipher_iv,
			NULL, &aad, &digest);

	return 0;
}

int
qat_sym_build_op_chain_gen1(void *in_op, struct qat_sym_session *ctx,
		uint8_t *out_msg, void *op_cookie)
{
	register struct icp_qat_fw_la_bulk_req *req;
	struct rte_crypto_op *op = in_op;
	struct qat_sym_op_cookie *cookie = op_cookie;
	struct rte_crypto_sgl in_sgl = {0}, out_sgl = {0};
	struct rte_crypto_vec in_vec[QAT_SYM_SGL_MAX_NUMBER],
			out_vec[QAT_SYM_SGL_MAX_NUMBER];
	struct rte_crypto_va_iova_ptr cipher_iv;
	struct rte_crypto_va_iova_ptr auth_iv;
	struct rte_crypto_va_iova_ptr digest;
	union rte_crypto_sym_ofs ofs;
	int32_t total_len;

	in_sgl.vec = in_vec;
	out_sgl.vec = out_vec;

	req = (struct icp_qat_fw_la_bulk_req *)out_msg;
	rte_mov128((uint8_t *)req, (const uint8_t *)&(ctx->fw_req));

	ofs.raw = qat_sym_convert_op_to_vec_chain(op, ctx, &in_sgl, &out_sgl,
			&cipher_iv, &auth_iv, &digest, cookie);
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

	enqueue_one_chain_job_gen1(ctx, req, in_sgl.vec, in_sgl.num,
			out_sgl.vec, out_sgl.num, &cipher_iv, &digest, &auth_iv,
			ofs, total_len);

	qat_sym_debug_log_dump(req, ctx, in_sgl.vec, in_sgl.num, &cipher_iv,
			&auth_iv, NULL, &digest);

	return 0;
}

#define QAT_SECURITY_SYM_CAPABILITIES					\
	{	/* AES DOCSIS BPI */					\
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,			\
		{.sym = {						\
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,	\
			{.cipher = {					\
				.algo = RTE_CRYPTO_CIPHER_AES_DOCSISBPI,\
				.block_size = 16,			\
				.key_size = {				\
					.min = 16,			\
					.max = 32,			\
					.increment = 16			\
				},					\
				.iv_size = {				\
					.min = 16,			\
					.max = 16,			\
					.increment = 0			\
				}					\
			}, }						\
		}, }							\
	}

#define QAT_SECURITY_CAPABILITIES(sym)					\
	[0] = {	/* DOCSIS Uplink */					\
		.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,	\
		.protocol = RTE_SECURITY_PROTOCOL_DOCSIS,		\
		.docsis = {						\
			.direction = RTE_SECURITY_DOCSIS_UPLINK		\
		},							\
		.crypto_capabilities = (sym)				\
	},								\
	[1] = {	/* DOCSIS Downlink */					\
		.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,	\
		.protocol = RTE_SECURITY_PROTOCOL_DOCSIS,		\
		.docsis = {						\
			.direction = RTE_SECURITY_DOCSIS_DOWNLINK	\
		},							\
		.crypto_capabilities = (sym)				\
	}

static const struct rte_cryptodev_capabilities
					qat_security_sym_capabilities[] = {
	QAT_SECURITY_SYM_CAPABILITIES,
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

static const struct rte_security_capability qat_security_capabilities_gen1[] = {
	QAT_SECURITY_CAPABILITIES(qat_security_sym_capabilities),
	{
		.action = RTE_SECURITY_ACTION_TYPE_NONE
	}
};

static const struct rte_security_capability *
qat_security_cap_get_gen1(void *dev __rte_unused)
{
	return qat_security_capabilities_gen1;
}

struct rte_security_ops security_qat_ops_gen1 = {
		.session_create = qat_security_session_create,
		.session_update = NULL,
		.session_get_size = qat_security_session_get_size,
		.session_stats_get = NULL,
		.session_destroy = qat_security_session_destroy,
		.set_pkt_metadata = NULL,
		.capabilities_get = qat_security_cap_get_gen1
};

void *
qat_sym_create_security_gen1(void *cryptodev)
{
	struct rte_security_ctx *security_instance;

	security_instance = rte_malloc(NULL, sizeof(struct rte_security_ctx),
			RTE_CACHE_LINE_SIZE);
	if (security_instance == NULL)
		return NULL;

	security_instance->device = cryptodev;
	security_instance->ops = &security_qat_ops_gen1;
	security_instance->sess_cnt = 0;

	return (void *)security_instance;
}

int
qat_sym_dp_enqueue_single_cipher_gen1(void *qp_data, uint8_t *drv_ctx,
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
	struct qat_sym_op_cookie *cookie;
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

	enqueue_one_cipher_job_gen1(ctx, req, iv, ofs, (uint32_t)data_len);

	qat_sym_debug_log_dump(req, ctx, data, n_data_vecs, iv,
			NULL, NULL, NULL);

	dp_ctx->tail = tail;
	dp_ctx->cached_enqueue++;

	return 0;
}

uint32_t
qat_sym_dp_enqueue_cipher_jobs_gen1(void *qp_data, uint8_t *drv_ctx,
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
		enqueue_one_cipher_job_gen1(ctx, req, &vec->iv[i], ofs,
			(uint32_t)data_len);
		tail = (tail + tx_queue->msg_size) & tx_queue->modulo_mask;

		qat_sym_debug_log_dump(req, ctx, vec->src_sgl[i].vec,
				vec->src_sgl[i].num, &vec->iv[i],
				NULL, NULL, NULL);
	}

	if (unlikely(i < n))
		qat_sym_dp_fill_vec_status(vec->status + i, -1, n - i);

	dp_ctx->tail = tail;
	dp_ctx->cached_enqueue += i;
	*status = 0;
	return i;
}

int
qat_sym_dp_enqueue_single_auth_gen1(void *qp_data, uint8_t *drv_ctx,
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

	enqueue_one_auth_job_gen1(ctx, req, job_digest, auth_iv, ofs,
		(uint32_t)data_len);

	dp_ctx->tail = tail;
	dp_ctx->cached_enqueue++;

	qat_sym_debug_log_dump(req, ctx, data, n_data_vecs, NULL,
			auth_iv, NULL, digest);

	return 0;
}

uint32_t
qat_sym_dp_enqueue_auth_jobs_gen1(void *qp_data, uint8_t *drv_ctx,
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

		enqueue_one_auth_job_gen1(ctx, req, job_digest,
			&vec->auth_iv[i], ofs, (uint32_t)data_len);
		tail = (tail + tx_queue->msg_size) & tx_queue->modulo_mask;

		qat_sym_debug_log_dump(req, ctx, vec->src_sgl[i].vec,
				vec->src_sgl[i].num, NULL, &vec->auth_iv[i],
				NULL, &vec->digest[i]);
	}

	if (unlikely(i < n))
		qat_sym_dp_fill_vec_status(vec->status + i, -1, n - i);

	dp_ctx->tail = tail;
	dp_ctx->cached_enqueue += i;
	*status = 0;
	return i;
}

int
qat_sym_dp_enqueue_single_chain_gen1(void *qp_data, uint8_t *drv_ctx,
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

	if (unlikely(enqueue_one_chain_job_gen1(ctx, req, data, n_data_vecs,
			NULL, 0, cipher_iv, job_digest, auth_iv, ofs,
			(uint32_t)data_len)))
		return -1;

	dp_ctx->tail = tail;
	dp_ctx->cached_enqueue++;


	qat_sym_debug_log_dump(req, ctx, data, n_data_vecs, cipher_iv,
			auth_iv, NULL, digest);

	return 0;
}

uint32_t
qat_sym_dp_enqueue_chain_jobs_gen1(void *qp_data, uint8_t *drv_ctx,
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
	struct rte_crypto_va_iova_ptr *job_digest;

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

		if (unlikely(enqueue_one_chain_job_gen1(ctx, req,
				vec->src_sgl[i].vec, vec->src_sgl[i].num,
				NULL, 0,
				&vec->iv[i], job_digest,
				&vec->auth_iv[i], ofs, (uint32_t)data_len)))
			break;

		tail = (tail + tx_queue->msg_size) & tx_queue->modulo_mask;

		qat_sym_debug_log_dump(req, ctx, vec->src_sgl[i].vec,
				vec->src_sgl[i].num, &vec->iv[i],
				&vec->auth_iv[i],
				NULL, &vec->digest[i]);
	}

	if (unlikely(i < n))
		qat_sym_dp_fill_vec_status(vec->status + i, -1, n - i);

	dp_ctx->tail = tail;
	dp_ctx->cached_enqueue += i;
	*status = 0;
	return i;
}

int
qat_sym_dp_enqueue_single_aead_gen1(void *qp_data, uint8_t *drv_ctx,
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

	enqueue_one_aead_job_gen1(ctx, req, iv, digest, aad, ofs,
		(uint32_t)data_len);

	dp_ctx->tail = tail;
	dp_ctx->cached_enqueue++;

	qat_sym_debug_log_dump(req, ctx, data, n_data_vecs, iv,
			NULL, aad, digest);

	return 0;
}

uint32_t
qat_sym_dp_enqueue_aead_jobs_gen1(void *qp_data, uint8_t *drv_ctx,
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

		enqueue_one_aead_job_gen1(ctx, req, &vec->iv[i],
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


uint32_t
qat_sym_dp_dequeue_burst_gen1(void *qp_data, uint8_t *drv_ctx,
	rte_cryptodev_raw_get_dequeue_count_t get_dequeue_count,
	uint32_t max_nb_to_dequeue,
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
	if (get_dequeue_count) {
		n = get_dequeue_count(resp_opaque);
		if (unlikely(n == 0))
			return 0;
	} else {
		if (unlikely(max_nb_to_dequeue == 0))
			return 0;
		n = max_nb_to_dequeue;
	}

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

void *
qat_sym_dp_dequeue_single_gen1(void *qp_data, uint8_t *drv_ctx,
	int *dequeue_status, enum rte_crypto_op_status *op_status)
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

int
qat_sym_dp_enqueue_done_gen1(void *qp_data, uint8_t *drv_ctx, uint32_t n)
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

int
qat_sym_dp_dequeue_done_gen1(void *qp_data, uint8_t *drv_ctx, uint32_t n)
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
qat_sym_configure_raw_dp_ctx_gen1(void *_raw_dp_ctx, void *_ctx)
{
	struct rte_crypto_raw_dp_ctx *raw_dp_ctx = _raw_dp_ctx;
	struct qat_sym_session *ctx = _ctx;

	raw_dp_ctx->enqueue_done = qat_sym_dp_enqueue_done_gen1;
	raw_dp_ctx->dequeue_burst = qat_sym_dp_dequeue_burst_gen1;
	raw_dp_ctx->dequeue = qat_sym_dp_dequeue_single_gen1;
	raw_dp_ctx->dequeue_done = qat_sym_dp_dequeue_done_gen1;

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

	return 0;
}

int
qat_sym_crypto_set_session_gen1(void *cryptodev __rte_unused, void *session)
{
	struct qat_sym_session *ctx = session;
	qat_sym_build_request_t build_request = NULL;
	enum rte_proc_type_t proc_type = rte_eal_process_type();
	int handle_mixed = 0;

	if (proc_type == RTE_PROC_AUTO || proc_type == RTE_PROC_INVALID)
		return -EINVAL;

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
			/* do_aead = 1; */
			build_request = qat_sym_build_op_aead_gen1;
		} else {
			/* do_auth = 1; do_cipher = 1; */
			build_request = qat_sym_build_op_chain_gen1;
			handle_mixed = 1;
		}
	} else if (ctx->qat_cmd == ICP_QAT_FW_LA_CMD_AUTH || ctx->is_gmac) {
		/* do_auth = 1; do_cipher = 0;*/
		build_request = qat_sym_build_op_auth_gen1;
	} else if (ctx->qat_cmd == ICP_QAT_FW_LA_CMD_CIPHER) {
		/* do_auth = 0; do_cipher = 1; */
		build_request = qat_sym_build_op_cipher_gen1;
	} else if (ctx->qat_cmd == ICP_QAT_FW_LA_CMD_CIPHER_CRC) {
		/* do_auth = 1; do_cipher = 1; */
		build_request = qat_sym_build_op_chain_gen1;
		handle_mixed = 1;
	}

	if (build_request)
		ctx->build_request[proc_type] = build_request;
	else
		return -EINVAL;

	/* no more work if not mixed op */
	if (!handle_mixed)
		return 0;

	/* Check none supported algs if mixed */
	if (ctx->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_ZUC_3G_128_EIA3 &&
			ctx->qat_cipher_alg !=
			ICP_QAT_HW_CIPHER_ALGO_ZUC_3G_128_EEA3) {
		return -ENOTSUP;
	} else if (ctx->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_SNOW_3G_UIA2 &&
			ctx->qat_cipher_alg !=
			ICP_QAT_HW_CIPHER_ALGO_SNOW_3G_UEA2) {
		return -ENOTSUP;
	} else if ((ctx->aes_cmac ||
			ctx->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_NULL) &&
			(ctx->qat_cipher_alg ==
			ICP_QAT_HW_CIPHER_ALGO_SNOW_3G_UEA2 ||
			ctx->qat_cipher_alg ==
			ICP_QAT_HW_CIPHER_ALGO_ZUC_3G_128_EEA3)) {
		return -ENOTSUP;
	}

	return 0;
}

RTE_INIT(qat_sym_crypto_gen1_init)
{
	qat_sym_gen_dev_ops[QAT_GEN1].cryptodev_ops = &qat_sym_crypto_ops_gen1;
	qat_sym_gen_dev_ops[QAT_GEN1].get_capabilities =
			qat_sym_crypto_cap_get_gen1;
	qat_sym_gen_dev_ops[QAT_GEN1].set_session =
			qat_sym_crypto_set_session_gen1;
	qat_sym_gen_dev_ops[QAT_GEN1].set_raw_dp_ctx =
			qat_sym_configure_raw_dp_ctx_gen1;
	qat_sym_gen_dev_ops[QAT_GEN1].get_feature_flags =
			qat_sym_crypto_feature_flags_get_gen1;
	qat_sym_gen_dev_ops[QAT_GEN1].create_security_ctx =
			qat_sym_create_security_gen1;
}
