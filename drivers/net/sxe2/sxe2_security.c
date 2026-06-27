/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#include <rte_malloc.h>

#include "sxe2_ethdev.h"
#include "sxe2_security.h"
#include "sxe2_ipsec.h"
#include "sxe2_common_log.h"

static unsigned int
sxe2_security_session_size_get(void *device __rte_unused)
{
	return sizeof(struct sxe2_security_session);
}

static int
sxe2_security_session_create(void *device,
			     struct rte_security_session_conf *conf,
			     struct rte_security_session *session)
{
	int32_t ret = -1;
	struct sxe2_security_session *sxe2_sess = NULL;
	sxe2_sess = SECURITY_GET_SESS_PRIV(session);

	switch (conf->protocol) {
	case RTE_SECURITY_PROTOCOL_IPSEC:
		ret = sxe2_ipsec_session_create(device, conf, sxe2_sess);
		break;
	default:
		PMD_LOG_ERR(DRV, "Invalid security protocol.");
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int
sxe2_security_session_destroy(void *device, struct rte_security_session *session)
{
	int32_t ret = -1;
	struct sxe2_security_session *sxe2_sess = NULL;
	sxe2_sess = SECURITY_GET_SESS_PRIV(session);

	switch (sxe2_sess->protocol) {
	case RTE_SECURITY_PROTOCOL_IPSEC:
		ret = sxe2_ipsec_session_destroy(device, session);
		break;
	default:
		PMD_LOG_ERR(DRV, "Invalid security protocol.");
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int
sxe2_security_pkt_metadata_set(void *device,
			       struct rte_security_session *session,
			       struct rte_mbuf *m, void *params)
{
	struct sxe2_security_session *sxe2_sess = NULL;
	sxe2_sess = SECURITY_GET_SESS_PRIV(session);
	int32_t ret = -1;

	switch (sxe2_sess->protocol) {
	case RTE_SECURITY_PROTOCOL_IPSEC:
		ret = sxe2_ipsec_pkt_metadata_set(device, session, m, params);
		break;
	default:
		PMD_LOG_ERR(DRV, "Invalid security protocol.");
		ret = -EINVAL;
		break;
	}

	return ret;
}

static const struct rte_security_capability *
sxe2_security_capabilities_get(void *device __rte_unused)
{
	static const struct rte_cryptodev_capabilities
	ipsec_crypto_capabilities[] = {
		{
			.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
			{.sym = {
				.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
				{.cipher = {
					.algo = SXE2_RTE_CRYPTO_CIPHER_AES_CBC,
					.block_size = SXE2_SECURITY_BLOCK_SIZE_16,
					.key_size = {
						.min = SXE2_IPSEC_AES_KEY_MIN,
						.max = SXE2_IPSEC_AES_KEY_MAX,
						.increment = SXE2_IPSEC_AES_KEY_INC
					},
					.iv_size = {
						.min = SXE2_IPSEC_AES_IV_MIN,
						.max = SXE2_IPSEC_AES_IV_MAX,
						.increment = SXE2_IPSEC_AES_IV_INC
					},
					.dataunit_set = RTE_CRYPTO_CIPHER_DATA_UNIT_LEN_512_BYTES,
				}, }
			}, }
		},
		{
			.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
			{.sym = {
				.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
				{.cipher = {
					.algo = SXE2_RTE_RTE_CRYPTO_CIPHER_SM4_CBC,
					.block_size = SXE2_SECURITY_BLOCK_SIZE_16,
					.key_size = {
						.min = SXE2_IPSEC_SM4_KEY_MIN,
						.max = SXE2_IPSEC_SM4_KEY_MAX,
						.increment = SXE2_IPSEC_SM4_KEY_INC
					},
					.iv_size = {
						.min = SXE2_IPSEC_SM4_IV_MIN,
						.max = SXE2_IPSEC_SM4_IV_MAX,
						.increment = SXE2_IPSEC_SM4_IV_INC
					},
					.dataunit_set = RTE_CRYPTO_CIPHER_DATA_UNIT_LEN_512_BYTES,
				}, }
			}, }
		},
		{
			.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
			{.sym = {
				.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
				{.auth = {
					.algo = SXE2_RTE_CRYPTO_AUTH_SHA256_HMAC,
					.block_size = SXE2_SECURITY_BLOCK_SIZE_64,
					.key_size = {
						.min = SXE2_IPSEC_SHA_KEY_MIN,
						.max = SXE2_IPSEC_SHA_KEY_MAX,
						.increment = SXE2_IPSEC_SHA_KEY_INC
					},
					.digest_size = {
						.min = SXE2_IPSEC_SHA_DIGEST_MIN,
						.max = SXE2_IPSEC_SHA_DIGEST_MAX,
						.increment = SXE2_IPSEC_SHA_DIGEST_INC
					},
					.iv_size = {
						.min = SXE2_IPSEC_SHA_IV_MIN,
						.max = SXE2_IPSEC_SHA_IV_MAX,
						.increment = SXE2_IPSEC_SHA_IV_INC
					},
					.aad_size = {
						.min = SXE2_IPSEC_AAD_MIN,
						.max = SXE2_IPSEC_AAD_MAX,
						.increment = SXE2_IPSEC_AAD_INC
					}
				}, }
			}, }
		},
		{
			.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
			{.sym = {
				.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
				{.auth = {
					.algo = SXE2_RTE_CRYPTO_AUTH_SM3_HMAC,
					.block_size = SXE2_SECURITY_BLOCK_SIZE_64,
					.key_size = {
						.min = SXE2_IPSEC_SM3_KEY_MIN,
						.max = SXE2_IPSEC_SM3_KEY_MAX,
						.increment = SXE2_IPSEC_SM3_KEY_INC
					},
					.digest_size = {
						.min = SXE2_IPSEC_SM3_DIGEST_MIN,
						.max = SXE2_IPSEC_SM3_DIGEST_MAX,
						.increment = SXE2_IPSEC_SM3_DIGEST_INC
					},
					.iv_size = {
						.min = SXE2_IPSEC_SM3_IV_MIN,
						.max = SXE2_IPSEC_SM3_IV_MAX,
						.increment = SXE2_IPSEC_SM3_IV_INC
					},
					.aad_size = {
						.min = SXE2_IPSEC_AAD_MIN,
						.max = SXE2_IPSEC_AAD_MAX,
						.increment = SXE2_IPSEC_AAD_INC
					}
				}, }
			}, }
		},
		{
			.op = RTE_CRYPTO_OP_TYPE_UNDEFINED,
			{.sym = {
				.xform_type = RTE_CRYPTO_SYM_XFORM_NOT_SPECIFIED
			}, }
		}
	};

	static const struct rte_security_capability
	sxe2_security_capabilities[] = {
		{
			.action = RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO,
			.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
			{.ipsec = {
				.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
				.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL,
				.direction = RTE_SECURITY_IPSEC_SA_DIR_EGRESS,
				.options = {
					.esn = 0,
					.udp_encap = 1,
					.copy_dscp = 0,
					.copy_flabel = 0,
					.copy_df = 0,
					.dec_ttl = 0,
					.ecn = 0,
					.stats = 1,
					.iv_gen_disable = 0,
					.tunnel_hdr_verify = 1,
					.udp_ports_verify = 1,
					.ip_csum_enable = 0,
					.l4_csum_enable = 0,
					.ip_reassembly_en = 0,
					.ingress_oop = 0
			} } },
			.crypto_capabilities = ipsec_crypto_capabilities,
			.ol_flags = RTE_SECURITY_TX_OLOAD_NEED_MDATA
		},
		{
			.action = RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO,
			.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
			{.ipsec = {
				.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
				.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL,
				.direction = RTE_SECURITY_IPSEC_SA_DIR_INGRESS,
				.options = {
					.esn = 0,
					.udp_encap = 1,
					.copy_dscp = 0,
					.copy_flabel = 0,
					.copy_df = 0,
					.dec_ttl = 0,
					.ecn = 0,
					.stats = 1,
					.iv_gen_disable = 0,
					.tunnel_hdr_verify = 1,
					.udp_ports_verify = 1,
					.ip_csum_enable = 0,
					.l4_csum_enable = 0,
					.ip_reassembly_en = 0,
					.ingress_oop = 0
			} } },
			.crypto_capabilities = ipsec_crypto_capabilities,
			.ol_flags = 0
		},
		{
			.action = RTE_SECURITY_ACTION_TYPE_NONE
		}
	};

	return sxe2_security_capabilities;
}

static struct rte_security_ops sxe2_security_ops = {
	.session_get_size		= sxe2_security_session_size_get,
	.session_create			= sxe2_security_session_create,
	.session_destroy		= sxe2_security_session_destroy,
	.set_pkt_metadata		= sxe2_security_pkt_metadata_set,
	.capabilities_get		= sxe2_security_capabilities_get,
};

int32_t sxe2_security_init(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct rte_security_ctx *sctx = NULL;
	struct sxe2_security_ctx *sxe2_sctx = &adapter->security_ctx;
	int32_t ret = -1;

	if (!sxe2_ipsec_supported(adapter)) {
		ret = 0;
		PMD_LOG_INFO(INIT, "Not support security feature.");
		goto l_end;
	}

	PMD_LOG_INFO(INIT, "Init security feature.");

	sctx = rte_zmalloc("security_ctx", sizeof(struct rte_security_ctx), 0);
	if (sctx == NULL) {
		ret = -ENOMEM;
		goto l_end;
	}

	sctx->device = dev;
	sctx->ops = &sxe2_security_ops;
	sctx->sess_cnt = 0;
	sctx->flags = 0;
	dev->security_ctx = (void *)sctx;

	rte_spinlock_init(&sxe2_sctx->security_lock);
	sxe2_sctx->adapter = adapter;

	if (sxe2_ipsec_supported(adapter)) {
		ret = sxe2_ipsec_init(adapter);
		if (ret) {
			rte_free(sctx);
			sctx = NULL;
			dev->security_ctx = NULL;
			goto l_end;
		}
	}

	ret = 0;

l_end:
	return ret;
}

void sxe2_security_uinit(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct rte_security_ctx *sctx = dev->security_ctx;

	if (!sxe2_ipsec_supported(adapter)) {
		PMD_LOG_INFO(INIT, "Not support security feature.");
		goto l_end;
	}

	PMD_LOG_INFO(INIT, "Uinit security feature.");

	if (sctx != NULL) {
		rte_free(sctx);
		sctx = NULL;
	}

	sxe2_ipsec_uinit(adapter);

l_end:
	return;
}
