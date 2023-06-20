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

#define MIXED_CRYPTO_MIN_FW_VER 0x04090000

static struct rte_cryptodev_capabilities qat_sym_crypto_legacy_caps_gen2[] = {
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
};

static struct rte_cryptodev_capabilities qat_sym_crypto_caps_gen2[] = {
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
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

static int
qat_sym_crypto_qp_setup_gen2(struct rte_cryptodev *dev, uint16_t qp_id,
		const struct rte_cryptodev_qp_conf *qp_conf, int socket_id)
{
	struct qat_cryptodev_private *qat_sym_private = dev->data->dev_private;
	struct qat_qp *qp;
	int ret;

	if (qat_cryptodev_qp_setup(dev, qp_id, qp_conf, socket_id)) {
		QAT_LOG(DEBUG, "QAT qp setup failed");
		return -1;
	}

	qp = qat_sym_private->qat_dev->qps_in_use[QAT_SERVICE_SYMMETRIC][qp_id];
	ret = qat_cq_get_fw_version(qp);
	if (ret < 0) {
		qat_cryptodev_qp_release(dev, qp_id);
		return ret;
	}

	if (ret != 0)
		QAT_LOG(DEBUG, "QAT firmware version: %d.%d.%d",
				(ret >> 24) & 0xff,
				(ret >> 16) & 0xff,
				(ret >> 8) & 0xff);
	else
		QAT_LOG(DEBUG, "unknown QAT firmware version");

	/* set capabilities based on the fw version */
	qat_sym_private->internal_capabilities |= QAT_SYM_CAP_VALID |
			((ret >= MIXED_CRYPTO_MIN_FW_VER) ?
					QAT_SYM_CAP_MIXED_CRYPTO : 0);
	return 0;
}

void
qat_sym_session_set_ext_hash_flags_gen2(struct qat_sym_session *session,
		uint8_t hash_flag)
{
	struct icp_qat_fw_comn_req_hdr *header = &session->fw_req.comn_hdr;
	struct icp_qat_fw_cipher_auth_cd_ctrl_hdr *cd_ctrl =
			(struct icp_qat_fw_cipher_auth_cd_ctrl_hdr *)
			session->fw_req.cd_ctrl.content_desc_ctrl_lw;

	/* Set the Use Extended Protocol Flags bit in LW 1 */
	QAT_FIELD_SET(header->comn_req_flags,
			QAT_COMN_EXT_FLAGS_USED,
			QAT_COMN_EXT_FLAGS_BITPOS,
			QAT_COMN_EXT_FLAGS_MASK);

	/* Set Hash Flags in LW 28 */
	cd_ctrl->hash_flags |= hash_flag;

	/* Set proto flags in LW 1 */
	switch (session->qat_cipher_alg) {
	case ICP_QAT_HW_CIPHER_ALGO_SNOW_3G_UEA2:
		ICP_QAT_FW_LA_PROTO_SET(header->serv_specif_flags,
				ICP_QAT_FW_LA_SNOW_3G_PROTO);
		ICP_QAT_FW_LA_ZUC_3G_PROTO_FLAG_SET(
				header->serv_specif_flags, 0);
		break;
	case ICP_QAT_HW_CIPHER_ALGO_ZUC_3G_128_EEA3:
		ICP_QAT_FW_LA_PROTO_SET(header->serv_specif_flags,
				ICP_QAT_FW_LA_NO_PROTO);
		ICP_QAT_FW_LA_ZUC_3G_PROTO_FLAG_SET(
				header->serv_specif_flags,
				ICP_QAT_FW_LA_ZUC_3G_PROTO);
		break;
	default:
		ICP_QAT_FW_LA_PROTO_SET(header->serv_specif_flags,
				ICP_QAT_FW_LA_NO_PROTO);
		ICP_QAT_FW_LA_ZUC_3G_PROTO_FLAG_SET(
				header->serv_specif_flags, 0);
		break;
	}
}

static int
qat_sym_crypto_set_session_gen2(void *cdev, void *session)
{
	struct rte_cryptodev *dev = cdev;
	struct qat_sym_session *ctx = session;
	const struct qat_cryptodev_private *qat_private =
			dev->data->dev_private;
	int ret;

	ret = qat_sym_crypto_set_session_gen1(cdev, session);
	if (ret == -ENOTSUP) {
		/* GEN1 returning -ENOTSUP as it cannot handle some mixed algo,
		 * but some are not supported by GEN2, so checking here
		 */
		if ((qat_private->internal_capabilities &
				QAT_SYM_CAP_MIXED_CRYPTO) == 0)
			return -ENOTSUP;

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

struct rte_cryptodev_ops qat_sym_crypto_ops_gen2 = {

	/* Device related operations */
	.dev_configure		= qat_cryptodev_config,
	.dev_start		= qat_cryptodev_start,
	.dev_stop		= qat_cryptodev_stop,
	.dev_close		= qat_cryptodev_close,
	.dev_infos_get		= qat_cryptodev_info_get,

	.stats_get		= qat_cryptodev_stats_get,
	.stats_reset		= qat_cryptodev_stats_reset,
	.queue_pair_setup	= qat_sym_crypto_qp_setup_gen2,
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
qat_sym_crypto_cap_get_gen2(struct qat_cryptodev_private *internals,
			const char *capa_memz_name,
			const uint16_t __rte_unused slice_map)
{
	uint32_t legacy_capa_num;
	uint32_t size = sizeof(qat_sym_crypto_caps_gen2);
	uint32_t legacy_size = sizeof(qat_sym_crypto_legacy_caps_gen2);
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
		capabilities = qat_sym_crypto_legacy_caps_gen2;
		memcpy(addr, capabilities, legacy_size);
		addr += legacy_capa_num;
	}
	capabilities = qat_sym_crypto_caps_gen2;
	memcpy(addr, capabilities, sizeof(qat_sym_crypto_caps_gen2));
	internals->qat_dev_capabilities = internals->capa_mz->addr;

	return 0;
}

RTE_INIT(qat_sym_crypto_gen2_init)
{
	qat_sym_gen_dev_ops[QAT_GEN2].cryptodev_ops = &qat_sym_crypto_ops_gen2;
	qat_sym_gen_dev_ops[QAT_GEN2].get_capabilities =
			qat_sym_crypto_cap_get_gen2;
	qat_sym_gen_dev_ops[QAT_GEN2].set_session =
			qat_sym_crypto_set_session_gen2;
	qat_sym_gen_dev_ops[QAT_GEN2].set_raw_dp_ctx =
			qat_sym_configure_raw_dp_ctx_gen1;
	qat_sym_gen_dev_ops[QAT_GEN2].get_feature_flags =
			qat_sym_crypto_feature_flags_get_gen1;
	qat_sym_gen_dev_ops[QAT_GEN2].create_security_ctx =
			qat_sym_create_security_gen1;
}

RTE_INIT(qat_asym_crypto_gen2_init)
{
	qat_asym_gen_dev_ops[QAT_GEN2].cryptodev_ops =
			&qat_asym_crypto_ops_gen1;
	qat_asym_gen_dev_ops[QAT_GEN2].get_capabilities =
			qat_asym_crypto_cap_get_gen1;
	qat_asym_gen_dev_ops[QAT_GEN2].get_feature_flags =
			qat_asym_crypto_feature_flags_get_gen1;
	qat_asym_gen_dev_ops[QAT_GEN2].set_session =
			qat_asym_crypto_set_session_gen1;
}
