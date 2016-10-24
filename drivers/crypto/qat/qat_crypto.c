/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2015-2016 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *	 * Redistributions of source code must retain the above copyright
 *	   notice, this list of conditions and the following disclaimer.
 *	 * Redistributions in binary form must reproduce the above copyright
 *	   notice, this list of conditions and the following disclaimer in
 *	   the documentation and/or other materials provided with the
 *	   distribution.
 *	 * Neither the name of Intel Corporation nor the names of its
 *	   contributors may be used to endorse or promote products derived
 *	   from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/queue.h>
#include <stdarg.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_tailq.h>
#include <rte_ether.h>
#include <rte_malloc.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_string_fns.h>
#include <rte_spinlock.h>
#include <rte_hexdump.h>

#include "qat_logs.h"
#include "qat_algs.h"
#include "qat_crypto.h"
#include "adf_transport_access_macros.h"

#define BYTE_LENGTH    8

static const struct rte_cryptodev_capabilities qat_pmd_capabilities[] = {
	{	/* SHA1 HMAC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA1_HMAC,
				.block_size = 64,
				.key_size = {
					.min = 64,
					.max = 64,
					.increment = 0
				},
				.digest_size = {
					.min = 20,
					.max = 20,
					.increment = 0
				},
				.aad_size = { 0 }
			}, }
		}, }
	},
	{	/* SHA224 HMAC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA224_HMAC,
				.block_size = 64,
					.key_size = {
					.min = 64,
					.max = 64,
					.increment = 0
				},
				.digest_size = {
					.min = 28,
					.max = 28,
					.increment = 0
				},
				.aad_size = { 0 }
			}, }
		}, }
	},
	{	/* SHA256 HMAC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA256_HMAC,
				.block_size = 64,
				.key_size = {
					.min = 64,
					.max = 64,
					.increment = 0
				},
				.digest_size = {
					.min = 32,
					.max = 32,
					.increment = 0
				},
				.aad_size = { 0 }
			}, }
		}, }
	},
	{	/* SHA384 HMAC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA384_HMAC,
				.block_size = 64,
				.key_size = {
					.min = 128,
					.max = 128,
					.increment = 0
				},
				.digest_size = {
					.min = 48,
					.max = 48,
					.increment = 0
					},
				.aad_size = { 0 }
			}, }
		}, }
	},
	{	/* SHA512 HMAC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA512_HMAC,
				.block_size = 128,
				.key_size = {
					.min = 128,
					.max = 128,
					.increment = 0
				},
				.digest_size = {
					.min = 64,
					.max = 64,
					.increment = 0
				},
				.aad_size = { 0 }
			}, }
		}, }
	},
	{	/* MD5 HMAC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_MD5_HMAC,
				.block_size = 64,
				.key_size = {
					.min = 8,
					.max = 64,
					.increment = 8
				},
				.digest_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				},
				.aad_size = { 0 }
			}, }
		}, }
	},
	{	/* AES XCBC MAC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_AES_XCBC_MAC,
				.block_size = 16,
				.key_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				},
				.digest_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				},
				.aad_size = { 0 }
			}, }
		}, }
	},
	{	/* AES GCM (AUTH) */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_AES_GCM,
				.block_size = 16,
				.key_size = {
					.min = 16,
					.max = 32,
					.increment = 8
				},
				.digest_size = {
					.min = 8,
					.max = 16,
					.increment = 4
				},
				.aad_size = {
					.min = 8,
					.max = 12,
					.increment = 4
				}
			}, }
		}, }
	},
	{	/* AES GMAC (AUTH) */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_AES_GMAC,
				.block_size = 16,
				.key_size = {
					.min = 16,
					.max = 32,
					.increment = 8
				},
				.digest_size = {
					.min = 8,
					.max = 16,
					.increment = 4
				},
				.aad_size = {
					.min = 1,
					.max = 65535,
					.increment = 1
				}
			}, }
		}, }
	},
	{	/* SNOW 3G (UIA2) */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SNOW3G_UIA2,
				.block_size = 16,
				.key_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				},
				.digest_size = {
					.min = 4,
					.max = 4,
					.increment = 0
				},
				.aad_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				}
			}, }
		}, }
	},
	{	/* AES GCM (CIPHER) */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_AES_GCM,
				.block_size = 16,
				.key_size = {
					.min = 16,
					.max = 32,
					.increment = 8
				},
				.iv_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				}
			}, }
		}, }
	},
	{	/* AES CBC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_AES_CBC,
				.block_size = 16,
				.key_size = {
					.min = 16,
					.max = 32,
					.increment = 8
				},
				.iv_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				}
			}, }
		}, }
	},
	{	/* SNOW 3G (UEA2) */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_SNOW3G_UEA2,
				.block_size = 16,
				.key_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				},
				.iv_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				}
			}, }
		}, }
	},
	{	/* AES CTR */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_AES_CTR,
				.block_size = 16,
				.key_size = {
					.min = 16,
					.max = 32,
					.increment = 8
				},
				.iv_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				}
			}, }
		}, }
	},
	{	/* NULL (AUTH) */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_NULL,
				.block_size = 1,
				.key_size = {
					.min = 0,
					.max = 0,
					.increment = 0
				},
				.digest_size = {
					.min = 0,
					.max = 0,
					.increment = 0
				},
				.aad_size = { 0 }
			}, },
		}, },
	},
	{	/* NULL (CIPHER) */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_NULL,
				.block_size = 1,
				.key_size = {
					.min = 0,
					.max = 0,
					.increment = 0
				},
				.iv_size = {
					.min = 0,
					.max = 0,
					.increment = 0
				}
			}, },
		}, }
	},
	{       /* KASUMI (F8) */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_KASUMI_F8,
				.block_size = 8,
				.key_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				},
				.iv_size = {
					.min = 8,
					.max = 8,
					.increment = 0
				}
			}, }
		}, }
	},
	{       /* KASUMI (F9) */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_KASUMI_F9,
				.block_size = 8,
				.key_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				},
				.digest_size = {
					.min = 4,
					.max = 4,
					.increment = 0
				},
				.aad_size = {
					.min = 8,
					.max = 8,
					.increment = 0
				}
			}, }
		}, }
	},
	{	/* 3DES CBC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_3DES_CBC,
				.block_size = 8,
				.key_size = {
					.min = 16,
					.max = 24,
					.increment = 8
				},
				.iv_size = {
					.min = 8,
					.max = 8,
					.increment = 0
				}
			}, }
		}, }
	},
	{	/* 3DES CTR */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_3DES_CTR,
				.block_size = 8,
				.key_size = {
					.min = 16,
					.max = 24,
					.increment = 8
				},
				.iv_size = {
					.min = 8,
					.max = 8,
					.increment = 0
				}
			}, }
		}, }
	},
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

static inline uint32_t
adf_modulo(uint32_t data, uint32_t shift);

static inline int
qat_write_hw_desc_entry(struct rte_crypto_op *op, uint8_t *out_msg);

void qat_crypto_sym_clear_session(struct rte_cryptodev *dev,
		void *session)
{
	struct qat_session *sess = session;
	phys_addr_t cd_paddr;

	PMD_INIT_FUNC_TRACE();
	if (session) {
		cd_paddr = sess->cd_paddr;
		memset(sess, 0, qat_crypto_sym_get_session_private_size(dev));
		sess->cd_paddr = cd_paddr;
	} else
		PMD_DRV_LOG(ERR, "NULL session");
}

static int
qat_get_cmd_id(const struct rte_crypto_sym_xform *xform)
{
	/* Cipher Only */
	if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER && xform->next == NULL)
		return ICP_QAT_FW_LA_CMD_CIPHER;

	/* Authentication Only */
	if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH && xform->next == NULL)
		return ICP_QAT_FW_LA_CMD_AUTH;

	if (xform->next == NULL)
		return -1;

	/* Cipher then Authenticate */
	if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER &&
			xform->next->type == RTE_CRYPTO_SYM_XFORM_AUTH)
		return ICP_QAT_FW_LA_CMD_CIPHER_HASH;

	/* Authenticate then Cipher */
	if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH &&
			xform->next->type == RTE_CRYPTO_SYM_XFORM_CIPHER)
		return ICP_QAT_FW_LA_CMD_HASH_CIPHER;

	return -1;
}

static struct rte_crypto_auth_xform *
qat_get_auth_xform(struct rte_crypto_sym_xform *xform)
{
	do {
		if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH)
			return &xform->auth;

		xform = xform->next;
	} while (xform);

	return NULL;
}

static struct rte_crypto_cipher_xform *
qat_get_cipher_xform(struct rte_crypto_sym_xform *xform)
{
	do {
		if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER)
			return &xform->cipher;

		xform = xform->next;
	} while (xform);

	return NULL;
}
void *
qat_crypto_sym_configure_session_cipher(struct rte_cryptodev *dev,
		struct rte_crypto_sym_xform *xform, void *session_private)
{
	struct qat_pmd_private *internals = dev->data->dev_private;

	struct qat_session *session = session_private;

	struct rte_crypto_cipher_xform *cipher_xform = NULL;

	/* Get cipher xform from crypto xform chain */
	cipher_xform = qat_get_cipher_xform(xform);

	switch (cipher_xform->algo) {
	case RTE_CRYPTO_CIPHER_AES_CBC:
		if (qat_alg_validate_aes_key(cipher_xform->key.length,
				&session->qat_cipher_alg) != 0) {
			PMD_DRV_LOG(ERR, "Invalid AES cipher key size");
			goto error_out;
		}
		session->qat_mode = ICP_QAT_HW_CIPHER_CBC_MODE;
		break;
	case RTE_CRYPTO_CIPHER_AES_GCM:
		if (qat_alg_validate_aes_key(cipher_xform->key.length,
				&session->qat_cipher_alg) != 0) {
			PMD_DRV_LOG(ERR, "Invalid AES cipher key size");
			goto error_out;
		}
		session->qat_mode = ICP_QAT_HW_CIPHER_CTR_MODE;
		break;
	case RTE_CRYPTO_CIPHER_AES_CTR:
		if (qat_alg_validate_aes_key(cipher_xform->key.length,
				&session->qat_cipher_alg) != 0) {
			PMD_DRV_LOG(ERR, "Invalid AES cipher key size");
			goto error_out;
		}
		session->qat_mode = ICP_QAT_HW_CIPHER_CTR_MODE;
		break;
	case RTE_CRYPTO_CIPHER_SNOW3G_UEA2:
		if (qat_alg_validate_snow3g_key(cipher_xform->key.length,
					&session->qat_cipher_alg) != 0) {
			PMD_DRV_LOG(ERR, "Invalid SNOW 3G cipher key size");
			goto error_out;
		}
		session->qat_mode = ICP_QAT_HW_CIPHER_ECB_MODE;
		break;
	case RTE_CRYPTO_CIPHER_NULL:
		session->qat_mode = ICP_QAT_HW_CIPHER_ECB_MODE;
		break;
	case RTE_CRYPTO_CIPHER_KASUMI_F8:
		if (qat_alg_validate_kasumi_key(cipher_xform->key.length,
					&session->qat_cipher_alg) != 0) {
			PMD_DRV_LOG(ERR, "Invalid KASUMI cipher key size");
			goto error_out;
		}
		session->qat_mode = ICP_QAT_HW_CIPHER_F8_MODE;
		break;
	case RTE_CRYPTO_CIPHER_3DES_CBC:
		if (qat_alg_validate_3des_key(cipher_xform->key.length,
				&session->qat_cipher_alg) != 0) {
			PMD_DRV_LOG(ERR, "Invalid 3DES cipher key size");
			goto error_out;
		}
		session->qat_mode = ICP_QAT_HW_CIPHER_CBC_MODE;
		break;
	case RTE_CRYPTO_CIPHER_3DES_CTR:
		if (qat_alg_validate_3des_key(cipher_xform->key.length,
				&session->qat_cipher_alg) != 0) {
			PMD_DRV_LOG(ERR, "Invalid 3DES cipher key size");
			goto error_out;
		}
		session->qat_mode = ICP_QAT_HW_CIPHER_CTR_MODE;
		break;
	case RTE_CRYPTO_CIPHER_3DES_ECB:
	case RTE_CRYPTO_CIPHER_AES_ECB:
	case RTE_CRYPTO_CIPHER_AES_CCM:
	case RTE_CRYPTO_CIPHER_AES_F8:
	case RTE_CRYPTO_CIPHER_AES_XTS:
	case RTE_CRYPTO_CIPHER_ARC4:
	case RTE_CRYPTO_CIPHER_ZUC_EEA3:
		PMD_DRV_LOG(ERR, "Crypto QAT PMD: Unsupported Cipher alg %u",
				cipher_xform->algo);
		goto error_out;
	default:
		PMD_DRV_LOG(ERR, "Crypto: Undefined Cipher specified %u\n",
				cipher_xform->algo);
		goto error_out;
	}

	if (cipher_xform->op == RTE_CRYPTO_CIPHER_OP_ENCRYPT)
		session->qat_dir = ICP_QAT_HW_CIPHER_ENCRYPT;
	else
		session->qat_dir = ICP_QAT_HW_CIPHER_DECRYPT;

	if (qat_alg_aead_session_create_content_desc_cipher(session,
						cipher_xform->key.data,
						cipher_xform->key.length))
		goto error_out;

	return session;

error_out:
	rte_mempool_put(internals->sess_mp, session);
	return NULL;
}


void *
qat_crypto_sym_configure_session(struct rte_cryptodev *dev,
		struct rte_crypto_sym_xform *xform, void *session_private)
{
	struct qat_pmd_private *internals = dev->data->dev_private;

	struct qat_session *session = session_private;

	int qat_cmd_id;

	PMD_INIT_FUNC_TRACE();

	/* Get requested QAT command id */
	qat_cmd_id = qat_get_cmd_id(xform);
	if (qat_cmd_id < 0 || qat_cmd_id >= ICP_QAT_FW_LA_CMD_DELIMITER) {
		PMD_DRV_LOG(ERR, "Unsupported xform chain requested");
		goto error_out;
	}
	session->qat_cmd = (enum icp_qat_fw_la_cmd_id)qat_cmd_id;
	switch (session->qat_cmd) {
	case ICP_QAT_FW_LA_CMD_CIPHER:
	session = qat_crypto_sym_configure_session_cipher(dev, xform, session);
		break;
	case ICP_QAT_FW_LA_CMD_AUTH:
	session = qat_crypto_sym_configure_session_auth(dev, xform, session);
		break;
	case ICP_QAT_FW_LA_CMD_CIPHER_HASH:
	session = qat_crypto_sym_configure_session_cipher(dev, xform, session);
	session = qat_crypto_sym_configure_session_auth(dev, xform, session);
		break;
	case ICP_QAT_FW_LA_CMD_HASH_CIPHER:
	session = qat_crypto_sym_configure_session_auth(dev, xform, session);
	session = qat_crypto_sym_configure_session_cipher(dev, xform, session);
		break;
	case ICP_QAT_FW_LA_CMD_TRNG_GET_RANDOM:
	case ICP_QAT_FW_LA_CMD_TRNG_TEST:
	case ICP_QAT_FW_LA_CMD_SSL3_KEY_DERIVE:
	case ICP_QAT_FW_LA_CMD_TLS_V1_1_KEY_DERIVE:
	case ICP_QAT_FW_LA_CMD_TLS_V1_2_KEY_DERIVE:
	case ICP_QAT_FW_LA_CMD_MGF1:
	case ICP_QAT_FW_LA_CMD_AUTH_PRE_COMP:
	case ICP_QAT_FW_LA_CMD_CIPHER_PRE_COMP:
	case ICP_QAT_FW_LA_CMD_DELIMITER:
	PMD_DRV_LOG(ERR, "Unsupported Service %u",
		session->qat_cmd);
		goto error_out;
	default:
	PMD_DRV_LOG(ERR, "Unsupported Service %u",
		session->qat_cmd);
		goto error_out;
	}
	return session;

error_out:
	rte_mempool_put(internals->sess_mp, session);
	return NULL;
}

struct qat_session *
qat_crypto_sym_configure_session_auth(struct rte_cryptodev *dev,
				struct rte_crypto_sym_xform *xform,
				struct qat_session *session_private)
{

	struct qat_pmd_private *internals = dev->data->dev_private;
	struct qat_session *session = session_private;
	struct rte_crypto_auth_xform *auth_xform = NULL;
	struct rte_crypto_cipher_xform *cipher_xform = NULL;
	auth_xform = qat_get_auth_xform(xform);

	switch (auth_xform->algo) {
	case RTE_CRYPTO_AUTH_SHA1_HMAC:
		session->qat_hash_alg = ICP_QAT_HW_AUTH_ALGO_SHA1;
		break;
	case RTE_CRYPTO_AUTH_SHA224_HMAC:
		session->qat_hash_alg = ICP_QAT_HW_AUTH_ALGO_SHA224;
		break;
	case RTE_CRYPTO_AUTH_SHA256_HMAC:
		session->qat_hash_alg = ICP_QAT_HW_AUTH_ALGO_SHA256;
		break;
	case RTE_CRYPTO_AUTH_SHA384_HMAC:
		session->qat_hash_alg = ICP_QAT_HW_AUTH_ALGO_SHA384;
		break;
	case RTE_CRYPTO_AUTH_SHA512_HMAC:
		session->qat_hash_alg = ICP_QAT_HW_AUTH_ALGO_SHA512;
		break;
	case RTE_CRYPTO_AUTH_AES_XCBC_MAC:
		session->qat_hash_alg = ICP_QAT_HW_AUTH_ALGO_AES_XCBC_MAC;
		break;
	case RTE_CRYPTO_AUTH_AES_GCM:
		session->qat_hash_alg = ICP_QAT_HW_AUTH_ALGO_GALOIS_128;
		break;
	case RTE_CRYPTO_AUTH_AES_GMAC:
		session->qat_hash_alg = ICP_QAT_HW_AUTH_ALGO_GALOIS_128;
		break;
	case RTE_CRYPTO_AUTH_SNOW3G_UIA2:
		session->qat_hash_alg = ICP_QAT_HW_AUTH_ALGO_SNOW_3G_UIA2;
		break;
	case RTE_CRYPTO_AUTH_MD5_HMAC:
		session->qat_hash_alg = ICP_QAT_HW_AUTH_ALGO_MD5;
		break;
	case RTE_CRYPTO_AUTH_NULL:
		session->qat_hash_alg = ICP_QAT_HW_AUTH_ALGO_NULL;
		break;
	case RTE_CRYPTO_AUTH_KASUMI_F9:
		session->qat_hash_alg = ICP_QAT_HW_AUTH_ALGO_KASUMI_F9;
		break;
	case RTE_CRYPTO_AUTH_SHA1:
	case RTE_CRYPTO_AUTH_SHA256:
	case RTE_CRYPTO_AUTH_SHA512:
	case RTE_CRYPTO_AUTH_SHA224:
	case RTE_CRYPTO_AUTH_SHA384:
	case RTE_CRYPTO_AUTH_MD5:
	case RTE_CRYPTO_AUTH_AES_CCM:
	case RTE_CRYPTO_AUTH_AES_CMAC:
	case RTE_CRYPTO_AUTH_AES_CBC_MAC:
	case RTE_CRYPTO_AUTH_ZUC_EIA3:
		PMD_DRV_LOG(ERR, "Crypto: Unsupported hash alg %u",
				auth_xform->algo);
		goto error_out;
	default:
		PMD_DRV_LOG(ERR, "Crypto: Undefined Hash algo %u specified",
				auth_xform->algo);
		goto error_out;
	}
	cipher_xform = qat_get_cipher_xform(xform);

	if ((session->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_GALOIS_128) ||
			(session->qat_hash_alg ==
				ICP_QAT_HW_AUTH_ALGO_GALOIS_64))  {
		if (qat_alg_aead_session_create_content_desc_auth(session,
				cipher_xform->key.data,
				cipher_xform->key.length,
				auth_xform->add_auth_data_length,
				auth_xform->digest_length,
				auth_xform->op))
			goto error_out;
	} else {
		if (qat_alg_aead_session_create_content_desc_auth(session,
				auth_xform->key.data,
				auth_xform->key.length,
				auth_xform->add_auth_data_length,
				auth_xform->digest_length,
				auth_xform->op))
			goto error_out;
	}
	return session;

error_out:
	if (internals->sess_mp != NULL)
		rte_mempool_put(internals->sess_mp, session);
	return NULL;
}

unsigned qat_crypto_sym_get_session_private_size(
		struct rte_cryptodev *dev __rte_unused)
{
	return RTE_ALIGN_CEIL(sizeof(struct qat_session), 8);
}


uint16_t
qat_pmd_enqueue_op_burst(void *qp, struct rte_crypto_op **ops,
		uint16_t nb_ops)
{
	register struct qat_queue *queue;
	struct qat_qp *tmp_qp = (struct qat_qp *)qp;
	register uint32_t nb_ops_sent = 0;
	register struct rte_crypto_op **cur_op = ops;
	register int ret;
	uint16_t nb_ops_possible = nb_ops;
	register uint8_t *base_addr;
	register uint32_t tail;
	int overflow;

	if (unlikely(nb_ops == 0))
		return 0;

	/* read params used a lot in main loop into registers */
	queue = &(tmp_qp->tx_q);
	base_addr = (uint8_t *)queue->base_addr;
	tail = queue->tail;

	/* Find how many can actually fit on the ring */
	overflow = rte_atomic16_add_return(&tmp_qp->inflights16, nb_ops)
				- queue->max_inflights;
	if (overflow > 0) {
		rte_atomic16_sub(&tmp_qp->inflights16, overflow);
		nb_ops_possible = nb_ops - overflow;
		if (nb_ops_possible == 0)
			return 0;
	}

	while (nb_ops_sent != nb_ops_possible) {
		ret = qat_write_hw_desc_entry(*cur_op, base_addr + tail);
		if (ret != 0) {
			tmp_qp->stats.enqueue_err_count++;
			if (nb_ops_sent == 0)
				return 0;
			goto kick_tail;
		}

		tail = adf_modulo(tail + queue->msg_size, queue->modulo);
		nb_ops_sent++;
		cur_op++;
	}
kick_tail:
	WRITE_CSR_RING_TAIL(tmp_qp->mmap_bar_addr, queue->hw_bundle_number,
			queue->hw_queue_number, tail);
	queue->tail = tail;
	tmp_qp->stats.enqueued_count += nb_ops_sent;
	return nb_ops_sent;
}

uint16_t
qat_pmd_dequeue_op_burst(void *qp, struct rte_crypto_op **ops,
		uint16_t nb_ops)
{
	struct qat_queue *queue;
	struct qat_qp *tmp_qp = (struct qat_qp *)qp;
	uint32_t msg_counter = 0;
	struct rte_crypto_op *rx_op;
	struct icp_qat_fw_comn_resp *resp_msg;

	queue = &(tmp_qp->rx_q);
	resp_msg = (struct icp_qat_fw_comn_resp *)
			((uint8_t *)queue->base_addr + queue->head);

	while (*(uint32_t *)resp_msg != ADF_RING_EMPTY_SIG &&
			msg_counter != nb_ops) {
		rx_op = (struct rte_crypto_op *)(uintptr_t)
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
			rx_op->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
		}
		*(uint32_t *)resp_msg = ADF_RING_EMPTY_SIG;
		queue->head = adf_modulo(queue->head +
				queue->msg_size,
				ADF_RING_SIZE_MODULO(queue->queue_size));
		resp_msg = (struct icp_qat_fw_comn_resp *)
					((uint8_t *)queue->base_addr +
							queue->head);
		*ops = rx_op;
		ops++;
		msg_counter++;
	}
	if (msg_counter > 0) {
		WRITE_CSR_RING_HEAD(tmp_qp->mmap_bar_addr,
					queue->hw_bundle_number,
					queue->hw_queue_number, queue->head);
		rte_atomic16_sub(&tmp_qp->inflights16, msg_counter);
		tmp_qp->stats.dequeued_count += msg_counter;
	}
	return msg_counter;
}

static inline int
qat_write_hw_desc_entry(struct rte_crypto_op *op, uint8_t *out_msg)
{
	struct qat_session *ctx;
	struct icp_qat_fw_la_cipher_req_params *cipher_param;
	struct icp_qat_fw_la_auth_req_params *auth_param;
	register struct icp_qat_fw_la_bulk_req *qat_req;
	uint8_t do_auth = 0, do_cipher = 0;
	uint32_t cipher_len = 0, cipher_ofs = 0;
	uint32_t auth_len = 0, auth_ofs = 0;
	uint32_t min_ofs = 0;
	uint32_t digest_appended = 1;
	uint64_t buf_start = 0;


#ifdef RTE_LIBRTE_PMD_QAT_DEBUG_TX
	if (unlikely(op->type != RTE_CRYPTO_OP_TYPE_SYMMETRIC)) {
		PMD_DRV_LOG(ERR, "QAT PMD only supports symmetric crypto "
				"operation requests, op (%p) is not a "
				"symmetric operation.", op);
		return -EINVAL;
	}
#endif
	if (unlikely(op->sym->sess_type == RTE_CRYPTO_SYM_OP_SESSIONLESS)) {
		PMD_DRV_LOG(ERR, "QAT PMD only supports session oriented"
				" requests, op (%p) is sessionless.", op);
		return -EINVAL;
	}

	if (unlikely(op->sym->session->dev_type != RTE_CRYPTODEV_QAT_SYM_PMD)) {
		PMD_DRV_LOG(ERR, "Session was not created for this device");
		return -EINVAL;
	}

	ctx = (struct qat_session *)op->sym->session->_private;
	qat_req = (struct icp_qat_fw_la_bulk_req *)out_msg;
	rte_mov128((uint8_t *)qat_req, (const uint8_t *)&(ctx->fw_req));
	qat_req->comn_mid.opaque_data = (uint64_t)(uintptr_t)op;
	cipher_param = (void *)&qat_req->serv_specif_rqpars;
	auth_param = (void *)((uint8_t *)cipher_param + sizeof(*cipher_param));

	if (ctx->qat_cmd == ICP_QAT_FW_LA_CMD_HASH_CIPHER ||
		ctx->qat_cmd == ICP_QAT_FW_LA_CMD_CIPHER_HASH) {
		do_auth = 1;
		do_cipher = 1;
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
			ctx->qat_cipher_alg == ICP_QAT_HW_CIPHER_ALGO_KASUMI) {

			if (unlikely(
				(cipher_param->cipher_length % BYTE_LENGTH != 0)
				 || (cipher_param->cipher_offset
							% BYTE_LENGTH != 0))) {
				PMD_DRV_LOG(ERR,
		  "SNOW3G/KASUMI in QAT PMD only supports byte aligned values");
				op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
				return -EINVAL;
			}
			cipher_len = op->sym->cipher.data.length >> 3;
			cipher_ofs = op->sym->cipher.data.offset >> 3;

		} else {
			cipher_len = op->sym->cipher.data.length;
			cipher_ofs = op->sym->cipher.data.offset;
		}

		/* copy IV into request if it fits */
		if (op->sym->cipher.iv.length && (op->sym->cipher.iv.length <=
				sizeof(cipher_param->u.cipher_IV_array))) {
			rte_memcpy(cipher_param->u.cipher_IV_array,
					op->sym->cipher.iv.data,
					op->sym->cipher.iv.length);
		} else {
			ICP_QAT_FW_LA_CIPH_IV_FLD_FLAG_SET(
					qat_req->comn_hdr.serv_specif_flags,
					ICP_QAT_FW_CIPH_IV_64BIT_PTR);
			cipher_param->u.s.cipher_IV_ptr =
					op->sym->cipher.iv.phys_addr;
		}
		min_ofs = cipher_ofs;
	}

	if (do_auth) {

		if (ctx->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_SNOW_3G_UIA2 ||
			ctx->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_KASUMI_F9) {
			if (unlikely((auth_param->auth_off % BYTE_LENGTH != 0)
				|| (auth_param->auth_len % BYTE_LENGTH != 0))) {
				PMD_DRV_LOG(ERR,
		"For SNOW3G/KASUMI, QAT PMD only supports byte aligned values");
				op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
				return -EINVAL;
			}
			auth_ofs = op->sym->auth.data.offset >> 3;
			auth_len = op->sym->auth.data.length >> 3;

			if (ctx->qat_hash_alg ==
					ICP_QAT_HW_AUTH_ALGO_KASUMI_F9) {
				if (do_cipher) {
					auth_len = auth_len + auth_ofs + 1 -
						ICP_QAT_HW_KASUMI_BLK_SZ;
					auth_ofs = ICP_QAT_HW_KASUMI_BLK_SZ;
				} else {
					auth_len = auth_len + auth_ofs + 1;
					auth_ofs = 0;
				}
			}

		} else {
			auth_ofs = op->sym->auth.data.offset;
			auth_len = op->sym->auth.data.length;
		}
		min_ofs = auth_ofs;

		if (op->sym->auth.digest.phys_addr) {
			ICP_QAT_FW_LA_DIGEST_IN_BUFFER_SET(
					qat_req->comn_hdr.serv_specif_flags,
					ICP_QAT_FW_LA_NO_DIGEST_IN_BUFFER);
			auth_param->auth_res_addr =
					op->sym->auth.digest.phys_addr;
			digest_appended = 0;
		}

		auth_param->u1.aad_adr = op->sym->auth.aad.phys_addr;

	}

	/* adjust for chain case */
	if (do_cipher && do_auth)
		min_ofs = cipher_ofs < auth_ofs ? cipher_ofs : auth_ofs;


	/* Start DMA at nearest aligned address below min_ofs */
	#define QAT_64_BTYE_ALIGN_MASK (~0x3f)
	buf_start = rte_pktmbuf_mtophys_offset(op->sym->m_src, min_ofs) &
							QAT_64_BTYE_ALIGN_MASK;

	if (unlikely((rte_pktmbuf_mtophys(op->sym->m_src)
			- rte_pktmbuf_headroom(op->sym->m_src)) > buf_start)) {
		/* alignment has pushed addr ahead of start of mbuf
		 * so revert and take the performance hit
		 */
		buf_start = rte_pktmbuf_mtophys(op->sym->m_src);
	}

	qat_req->comn_mid.dest_data_addr =
		qat_req->comn_mid.src_data_addr = buf_start;

	if (do_cipher) {
		cipher_param->cipher_offset =
					(uint32_t)rte_pktmbuf_mtophys_offset(
					op->sym->m_src, cipher_ofs) - buf_start;
		cipher_param->cipher_length = cipher_len;
	} else {
		cipher_param->cipher_offset = 0;
		cipher_param->cipher_length = 0;
	}
	if (do_auth) {
		auth_param->auth_off = (uint32_t)rte_pktmbuf_mtophys_offset(
					op->sym->m_src, auth_ofs) - buf_start;
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

	if (do_auth && digest_appended) {
		if (ctx->auth_op == ICP_QAT_HW_AUTH_GENERATE)
			qat_req->comn_mid.dst_length
					+= op->sym->auth.digest.length;
		else
			qat_req->comn_mid.src_length
				+= op->sym->auth.digest.length;
	}

	/* out-of-place operation (OOP) */
	if (unlikely(op->sym->m_dst != NULL)) {

		if (do_auth)
			qat_req->comn_mid.dest_data_addr =
				rte_pktmbuf_mtophys_offset(op->sym->m_dst,
						auth_ofs)
						- auth_param->auth_off;
		else
			qat_req->comn_mid.dest_data_addr =
				rte_pktmbuf_mtophys_offset(op->sym->m_dst,
						cipher_ofs)
						- cipher_param->cipher_offset;
	}

	if (ctx->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_GALOIS_128 ||
			ctx->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_GALOIS_64) {
		if (op->sym->cipher.iv.length == 12) {
			/*
			 * For GCM a 12 bit IV is allowed,
			 * but we need to inform the f/w
			 */
			ICP_QAT_FW_LA_GCM_IV_LEN_FLAG_SET(
				qat_req->comn_hdr.serv_specif_flags,
				ICP_QAT_FW_LA_GCM_IV_LEN_12_OCTETS);
		}
		if (op->sym->cipher.data.length == 0) {
			/*
			 * GMAC
			 */
			qat_req->comn_mid.dest_data_addr =
				qat_req->comn_mid.src_data_addr =
						op->sym->auth.aad.phys_addr;
			qat_req->comn_mid.dst_length =
				qat_req->comn_mid.src_length =
					rte_pktmbuf_data_len(op->sym->m_src);
			cipher_param->cipher_length = 0;
			cipher_param->cipher_offset = 0;
			auth_param->u1.aad_adr = 0;
			auth_param->auth_len = op->sym->auth.aad.length;
			auth_param->auth_off = op->sym->auth.data.offset;
			auth_param->u2.aad_sz = 0;
		}
	}


#ifdef RTE_LIBRTE_PMD_QAT_DEBUG_TX
	rte_hexdump(stdout, "qat_req:", qat_req,
			sizeof(struct icp_qat_fw_la_bulk_req));
	rte_hexdump(stdout, "src_data:",
			rte_pktmbuf_mtod(op->sym->m_src, uint8_t*),
			rte_pktmbuf_data_len(op->sym->m_src));
	rte_hexdump(stdout, "iv:", op->sym->cipher.iv.data,
			op->sym->cipher.iv.length);
	rte_hexdump(stdout, "digest:", op->sym->auth.digest.data,
			op->sym->auth.digest.length);
	rte_hexdump(stdout, "aad:", op->sym->auth.aad.data,
			op->sym->auth.aad.length);
#endif
	return 0;
}

static inline uint32_t adf_modulo(uint32_t data, uint32_t shift)
{
	uint32_t div = data >> shift;
	uint32_t mult = div << shift;

	return data - mult;
}

void qat_crypto_sym_session_init(struct rte_mempool *mp, void *sym_sess)
{
	struct rte_cryptodev_sym_session *sess = sym_sess;
	struct qat_session *s = (void *)sess->_private;

	PMD_INIT_FUNC_TRACE();
	s->cd_paddr = rte_mempool_virt2phy(mp, sess) +
		offsetof(struct qat_session, cd) +
		offsetof(struct rte_cryptodev_sym_session, _private);
}

int qat_dev_config(__rte_unused struct rte_cryptodev *dev)
{
	PMD_INIT_FUNC_TRACE();
	return -ENOTSUP;
}

int qat_dev_start(__rte_unused struct rte_cryptodev *dev)
{
	PMD_INIT_FUNC_TRACE();
	return 0;
}

void qat_dev_stop(__rte_unused struct rte_cryptodev *dev)
{
	PMD_INIT_FUNC_TRACE();
}

int qat_dev_close(struct rte_cryptodev *dev)
{
	int i, ret;

	PMD_INIT_FUNC_TRACE();

	for (i = 0; i < dev->data->nb_queue_pairs; i++) {
		ret = qat_crypto_sym_qp_release(dev, i);
		if (ret < 0)
			return ret;
	}

	return 0;
}

void qat_dev_info_get(__rte_unused struct rte_cryptodev *dev,
				struct rte_cryptodev_info *info)
{
	struct qat_pmd_private *internals = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();
	if (info != NULL) {
		info->max_nb_queue_pairs =
				ADF_NUM_SYM_QPS_PER_BUNDLE *
				ADF_NUM_BUNDLES_PER_DEV;
		info->feature_flags = dev->feature_flags;
		info->capabilities = qat_pmd_capabilities;
		info->sym.max_nb_sessions = internals->max_nb_sessions;
		info->dev_type = RTE_CRYPTODEV_QAT_SYM_PMD;
	}
}

void qat_crypto_sym_stats_get(struct rte_cryptodev *dev,
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
		stats->dequeued_count += qp[i]->stats.enqueued_count;
		stats->enqueue_err_count += qp[i]->stats.enqueue_err_count;
		stats->dequeue_err_count += qp[i]->stats.enqueue_err_count;
	}
}

void qat_crypto_sym_stats_reset(struct rte_cryptodev *dev)
{
	int i;
	struct qat_qp **qp = (struct qat_qp **)(dev->data->queue_pairs);

	PMD_INIT_FUNC_TRACE();
	for (i = 0; i < dev->data->nb_queue_pairs; i++)
		memset(&(qp[i]->stats), 0, sizeof(qp[i]->stats));
	PMD_DRV_LOG(DEBUG, "QAT crypto: stats cleared");
}
