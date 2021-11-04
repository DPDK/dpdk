/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2021 Intel Corporation
 */

#include <rte_cryptodev.h>
#include <cryptodev_pmd.h>
#include "qat_sym_session.h"
#include "qat_sym.h"
#include "qat_asym.h"
#include "qat_crypto.h"
#include "qat_crypto_pmd_gens.h"

#define MIXED_CRYPTO_MIN_FW_VER 0x04090000

static struct rte_cryptodev_capabilities qat_sym_crypto_caps_gen2[] = {
	QAT_SYM_PLAIN_AUTH_CAP(SHA1,
		CAP_SET(block_size, 64),
		CAP_RNG(digest_size, 1, 20, 1)),
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
	QAT_SYM_AUTH_CAP(SHA224,
		CAP_SET(block_size, 64),
		CAP_RNG_ZERO(key_size), CAP_RNG(digest_size, 1, 28, 1),
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
	QAT_SYM_AUTH_CAP(SHA1_HMAC,
		CAP_SET(block_size, 64),
		CAP_RNG(key_size, 1, 64, 1), CAP_RNG(digest_size, 1, 20, 1),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_AUTH_CAP(SHA224_HMAC,
		CAP_SET(block_size, 64),
		CAP_RNG(key_size, 1, 64, 1), CAP_RNG(digest_size, 1, 28, 1),
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
	QAT_SYM_AUTH_CAP(MD5_HMAC,
		CAP_SET(block_size, 64),
		CAP_RNG(key_size, 1, 64, 1), CAP_RNG(digest_size, 1, 16, 1),
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
	QAT_SYM_CIPHER_CAP(3DES_CBC,
		CAP_SET(block_size, 8),
		CAP_RNG(key_size, 8, 24, 8), CAP_RNG(iv_size, 8, 8, 0)),
	QAT_SYM_CIPHER_CAP(3DES_CTR,
		CAP_SET(block_size, 8),
		CAP_RNG(key_size, 16, 24, 8), CAP_RNG(iv_size, 8, 8, 0)),
	QAT_SYM_CIPHER_CAP(DES_CBC,
		CAP_SET(block_size, 8),
		CAP_RNG(key_size, 8, 24, 8), CAP_RNG(iv_size, 8, 8, 0)),
	QAT_SYM_CIPHER_CAP(DES_DOCSISBPI,
		CAP_SET(block_size, 8),
		CAP_RNG(key_size, 8, 8, 0), CAP_RNG(iv_size, 8, 8, 0)),
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
	qat_sym_private->internal_capabilities = QAT_SYM_CAP_VALID |
			((ret >= MIXED_CRYPTO_MIN_FW_VER) ?
					QAT_SYM_CAP_MIXED_CRYPTO : 0);
	return 0;
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

static struct qat_capabilities_info
qat_sym_crypto_cap_get_gen2(struct qat_pci_device *qat_dev __rte_unused)
{
	struct qat_capabilities_info capa_info;
	capa_info.data = qat_sym_crypto_caps_gen2;
	capa_info.size = sizeof(qat_sym_crypto_caps_gen2);
	return capa_info;
}

RTE_INIT(qat_sym_crypto_gen2_init)
{
	qat_sym_gen_dev_ops[QAT_GEN2].cryptodev_ops = &qat_sym_crypto_ops_gen2;
	qat_sym_gen_dev_ops[QAT_GEN2].get_capabilities =
			qat_sym_crypto_cap_get_gen2;
	qat_sym_gen_dev_ops[QAT_GEN2].get_feature_flags =
			qat_sym_crypto_feature_flags_get_gen1;

#ifdef RTE_LIB_SECURITY
	qat_sym_gen_dev_ops[QAT_GEN2].create_security_ctx =
			qat_sym_create_security_gen1;
#endif
}

RTE_INIT(qat_asym_crypto_gen2_init)
{
	qat_asym_gen_dev_ops[QAT_GEN2].cryptodev_ops =
			&qat_asym_crypto_ops_gen1;
	qat_asym_gen_dev_ops[QAT_GEN2].get_capabilities =
			qat_asym_crypto_cap_get_gen1;
	qat_asym_gen_dev_ops[QAT_GEN2].get_feature_flags =
			qat_asym_crypto_feature_flags_get_gen1;
}
