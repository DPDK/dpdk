/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#include <rte_malloc.h>
#include <rte_bitmap.h>

#include "sxe2_ethdev.h"
#include "sxe2_security.h"
#include "sxe2_ipsec.h"
#include "sxe2_cmd_chnl.h"
#include "sxe2_common_log.h"

bool sxe2_ipsec_supported(struct sxe2_adapter *adapter)
{
	uint64_t cap = adapter->cap_flags;

	return !!(cap & SXE2_DEV_CAPS_OFFLOAD_IPSEC);
}

bool sxe2_ipsec_valid_tx_offloads(uint64_t offloads)
{
	bool ret = true;
	uint64_t tso_features = 0;
	uint64_t cksum_features = 0;

	if (offloads & RTE_ETH_TX_OFFLOAD_SECURITY) {
		tso_features = RTE_ETH_TX_OFFLOAD_TCP_TSO |
			RTE_ETH_TX_OFFLOAD_UDP_TSO |
			RTE_ETH_TX_OFFLOAD_VXLAN_TNL_TSO |
			RTE_ETH_TX_OFFLOAD_GRE_TNL_TSO |
			RTE_ETH_TX_OFFLOAD_IPIP_TNL_TSO |
			RTE_ETH_TX_OFFLOAD_GENEVE_TNL_TSO;
		if (offloads & tso_features) {
			PMD_LOG_ERR(DRV, "Security offload is not compatible with TSO offload.");
			ret = false;
			goto l_end;
		}

		cksum_features = RTE_ETH_TX_OFFLOAD_IPV4_CKSUM |
			RTE_ETH_TX_OFFLOAD_UDP_CKSUM |
			RTE_ETH_TX_OFFLOAD_TCP_CKSUM |
			RTE_ETH_TX_OFFLOAD_SCTP_CKSUM |
			RTE_ETH_TX_OFFLOAD_OUTER_IPV4_CKSUM |
			RTE_ETH_TX_OFFLOAD_OUTER_UDP_CKSUM;
		if (offloads & cksum_features) {
			PMD_LOG_ERR(DRV, "Security offload is not compatible with checksum offload.");
			ret = false;
			goto l_end;
		}

		if (offloads & (RTE_ETH_TX_OFFLOAD_VLAN_INSERT | RTE_ETH_TX_OFFLOAD_QINQ_INSERT)) {
			PMD_LOG_ERR(DRV, "Security offload is not compatible with vlan offload.");
			ret = false;
			goto l_end;
		}
	}

l_end:
	return ret;
}

bool sxe2_ipsec_valid_rx_offloads(uint64_t offloads)
{
	bool ret = true;

	if (offloads & RTE_ETH_RX_OFFLOAD_SECURITY) {
		if (offloads & RTE_ETH_RX_OFFLOAD_TCP_LRO) {
			PMD_LOG_ERR(DRV, "Security offload is not compatible with LRO offload.");
			ret = false;
			goto l_end;
		}

		if (offloads & RTE_ETH_RX_OFFLOAD_CHECKSUM) {
			PMD_LOG_ERR(DRV, "Security offload is not compatible with checksum offload.");
			ret = false;
			goto l_end;
		}

		if (offloads & RTE_ETH_RX_OFFLOAD_KEEP_CRC) {
			PMD_LOG_ERR(DRV, "Security offload is not compatible with keep CRC offload.");
			ret = false;
			goto l_end;
		}

		if (offloads & RTE_ETH_RX_OFFLOAD_VLAN) {
			PMD_LOG_ERR(DRV, "Security offload is not compatible with vlan offload.");
			ret = false;
			goto l_end;
		}
	}

l_end:
	return ret;
}

static int32_t sxe2_ipsec_bitmap_mem_init(struct rte_bitmap **d_bmp, void **d_mem, uint32_t bits)
{
	struct rte_bitmap *bmp = NULL;
	uint32_t bmp_size           = 0;
	void *mem              = NULL;
	int32_t ret                = -1;

	bmp_size = rte_bitmap_get_memory_footprint(bits);

	mem = rte_zmalloc("ipsec bitmap", bmp_size, RTE_CACHE_LINE_SIZE);
	if (mem == NULL) {
		PMD_LOG_ERR(DRV, "Alloc ipsec bitmap memory failed.");
		ret = -ENOMEM;
		goto l_end;
	}

	bmp = rte_bitmap_init(bits, mem, bmp_size);
	if (bmp == NULL) {
		PMD_LOG_ERR(DRV, "Failed to init ipsec bitmap.");
		rte_free(mem);
		ret = -ENOMEM;
		goto l_end;
	}

	*d_bmp = bmp;
	*d_mem = mem;

	ret = 0;

l_end:
	return ret;
}

static int32_t sxe2_ipsec_bitmap_init(struct sxe2_security_ctx *sxe2_sctx)
{
	int32_t ret  = -1;

	ret = sxe2_ipsec_bitmap_mem_init(&sxe2_sctx->ipsec_ctx.bmp.tx_sa_bmp,
			&sxe2_sctx->ipsec_ctx.bmp.tx_sa_mem, sxe2_sctx->ipsec_ctx.max_tx_sa);
	if (ret)
		goto l_end;

	ret = sxe2_ipsec_bitmap_mem_init(&sxe2_sctx->ipsec_ctx.bmp.rx_sa_bmp,
			&sxe2_sctx->ipsec_ctx.bmp.rx_sa_mem, sxe2_sctx->ipsec_ctx.max_rx_sa);
	if (ret) {
		rte_free(sxe2_sctx->ipsec_ctx.bmp.tx_sa_mem);
		sxe2_sctx->ipsec_ctx.bmp.tx_sa_mem = NULL;
		goto l_end;
	}

	ret = sxe2_ipsec_bitmap_mem_init(&sxe2_sctx->ipsec_ctx.bmp.rx_tcam_bmp,
			&sxe2_sctx->ipsec_ctx.bmp.rx_tcam_mem, sxe2_sctx->ipsec_ctx.max_tcam);
	if (ret) {
		rte_free(sxe2_sctx->ipsec_ctx.bmp.tx_sa_mem);
		rte_free(sxe2_sctx->ipsec_ctx.bmp.rx_sa_mem);
		sxe2_sctx->ipsec_ctx.bmp.tx_sa_mem = NULL;
		sxe2_sctx->ipsec_ctx.bmp.rx_sa_mem = NULL;
		goto l_end;
	}

	ret = sxe2_ipsec_bitmap_mem_init(&sxe2_sctx->ipsec_ctx.bmp.rx_udp_bmp,
			&sxe2_sctx->ipsec_ctx.bmp.rx_udp_mem, sxe2_sctx->ipsec_ctx.max_udp_group);
	if (ret) {
		rte_free(sxe2_sctx->ipsec_ctx.bmp.tx_sa_mem);
		rte_free(sxe2_sctx->ipsec_ctx.bmp.rx_sa_mem);
		rte_free(sxe2_sctx->ipsec_ctx.bmp.rx_tcam_mem);
		sxe2_sctx->ipsec_ctx.bmp.tx_sa_mem = NULL;
		sxe2_sctx->ipsec_ctx.bmp.rx_sa_mem = NULL;
		sxe2_sctx->ipsec_ctx.bmp.rx_tcam_mem = NULL;
		goto l_end;
	}

l_end:
	return ret;
}

static uint16_t sxe2_ipsec_id_alloc(struct rte_bitmap *bmp, uint16_t bits)
{
	uint16_t i = 0;
	uint16_t index = 0XFFFF;

	for (i = 0; i < bits; i++) {
		if (!rte_bitmap_get(bmp, i)) {
			index = i;
			rte_bitmap_set(bmp, i);
			break;
		}
	}

	return index;
}

static void sxe2_ipsec_id_free(struct rte_bitmap *bmp, uint16_t pos)
{
	rte_bitmap_clear(bmp, pos);
}

static struct rte_cryptodev_symmetric_capability *
sxe2_ipsec_cipher_cap_get(struct rte_cryptodev_capabilities *crypto_cap,
			enum rte_crypto_cipher_algorithm algo)
{
	struct rte_cryptodev_symmetric_capability *capability = NULL;
	uint8_t index                                              = 0;

	for (index = 0; index < SXE2_IPSEC_CAP_MAX; index++) {
		if (crypto_cap[index].sym.xform_type == RTE_CRYPTO_SYM_XFORM_CIPHER &&
			crypto_cap[index].sym.cipher.algo == algo) {
			capability = &crypto_cap[index].sym;
			goto l_end;
		}
	}

l_end:
	return capability;
}

static struct rte_cryptodev_symmetric_capability *
sxe2_ipsec_auth_cap_get(struct rte_cryptodev_capabilities *crypto_cap,
			enum rte_crypto_auth_algorithm algo)
{
	struct rte_cryptodev_symmetric_capability *capability = NULL;
	uint8_t index                                              = 0;

	for (index = 0; index < SXE2_IPSEC_CAP_MAX; index++) {
		if (crypto_cap[index].sym.xform_type == RTE_CRYPTO_SYM_XFORM_AUTH &&
			crypto_cap[index].sym.auth.algo == algo) {
			capability = &crypto_cap[index].sym;
			goto l_end;
		}
	}

l_end:
	return capability;
}

static bool sxe2_security_valid_key(uint16_t src_key, uint16_t max_key,
				    uint16_t min_key, uint16_t increment)
{
	bool is_valid = false;

	if (src_key > SXE2_IPSEC_MAX_KEY_LEN) {
		is_valid = false;
		goto l_end;
	}

	if (src_key < min_key || src_key > max_key) {
		is_valid = false;
		goto l_end;
	}

	if (increment == 0) {
		is_valid = true;
		goto l_end;
	}

	if ((uint16_t)(src_key - min_key) % increment) {
		is_valid = false;
		goto l_end;
	}

	is_valid = true;

l_end:
	return is_valid;
}

static int32_t
sxe2_ipsec_valid_cipher(enum rte_crypto_cipher_operation cipher_op,
			struct rte_cryptodev_capabilities *crypto_cap,
			struct rte_crypto_sym_xform *xform)
{
	const struct rte_cryptodev_symmetric_capability *capability = NULL;
	uint16_t src_key                                = 0;
	uint16_t max_key                                = 0;
	uint16_t min_key                                = 0;
	uint16_t increment                              = 0;
	int32_t ret                                    = -1;

	if (xform->cipher.op != cipher_op) {
		PMD_LOG_ERR(DRV, "Invalid cipher direction specified");
		ret = -EINVAL;
		goto l_end;
	}

	capability = sxe2_ipsec_cipher_cap_get(crypto_cap, xform->cipher.algo);
	if (!capability) {
		PMD_LOG_ERR(DRV, "Invalid cipher algo specified");
		ret = -EINVAL;
		goto l_end;
	}

	src_key = xform->cipher.key.length;
	min_key = capability->cipher.key_size.min;
	max_key = capability->cipher.key_size.max;
	increment = capability->cipher.key_size.increment;
	if (!sxe2_security_valid_key(src_key, max_key, min_key, increment)) {
		PMD_LOG_ERR(DRV, "Invalid cipher key size specified");
		ret = -EINVAL;
		goto l_end;
	}

	ret = 0;

l_end:
	return ret;
}

static int32_t
sxe2_ipsec_valid_auth(enum rte_crypto_auth_operation auth_op,
		      struct rte_cryptodev_capabilities *crypto_cap,
		      struct rte_crypto_sym_xform *xform)
{
	const struct rte_cryptodev_symmetric_capability *capability = NULL;
	uint16_t src_key                                = 0;
	uint16_t max_key                                = 0;
	uint16_t min_key                                = 0;
	uint16_t increment                              = 0;
	int32_t ret                                    = -1;

	if (xform->auth.op != auth_op) {
		PMD_LOG_ERR(DRV, "Invalid auth direction specified");
		ret = -EINVAL;
		goto l_end;
	}

	capability = sxe2_ipsec_auth_cap_get(crypto_cap, xform->auth.algo);
	if (!capability) {
		PMD_LOG_ERR(DRV, "Invalid auth algo specified");
		ret = -EINVAL;
		goto l_end;
	}

	src_key = xform->auth.key.length;
	min_key = capability->auth.key_size.min;
	max_key = capability->auth.key_size.max;
	increment = capability->auth.key_size.increment;
	if (!sxe2_security_valid_key(src_key, max_key, min_key, increment)) {
		PMD_LOG_ERR(DRV, "Invalid auth key size specified");
		ret = -EINVAL;
		goto l_end;
	}

	ret = 0;

l_end:
	return ret;
}

static bool
sxe2_ipsec_valid_algo(enum rte_crypto_auth_algorithm auth_algo,
		      enum rte_crypto_cipher_algorithm cipher_algo)
{
	bool ret = false;

	if ((cipher_algo == SXE2_RTE_CRYPTO_CIPHER_AES_CBC &&
		 auth_algo == SXE2_RTE_CRYPTO_AUTH_SHA256_HMAC) ||
		(cipher_algo == SXE2_RTE_RTE_CRYPTO_CIPHER_SM4_CBC &&
		 auth_algo == SXE2_RTE_CRYPTO_AUTH_SM3_HMAC)) {
		ret = true;
		goto l_end;
	}

l_end:
	return ret;
}

static enum sxe2_ipsec_algorithm
sxe2_ipsec_algo_gen(enum rte_crypto_cipher_algorithm cipher_algo)
{
	enum sxe2_ipsec_algorithm algo = SXE2_IPSEC_ALGO_INVALID;

	if (cipher_algo == SXE2_RTE_CRYPTO_CIPHER_AES_CBC)
		algo = SXE2_IPSEC_ALGO_AES_CBC_AND_SHA256_128_HMAC;
	else if (cipher_algo == SXE2_RTE_RTE_CRYPTO_CIPHER_SM4_CBC)
		algo = SXE2_IPSEC_ALGO_SM4_CBC_AND_SM3_96_HMAC;

	return algo;
}

static int32_t
	sxe2_ipsec_valid_xform(struct sxe2_security_ctx *sxe2_sctx,
			       struct rte_security_session_conf *conf)
{
	struct rte_crypto_sym_xform *xform = NULL;
	struct rte_cryptodev_capabilities *crypto_cap =
		sxe2_sctx->sxe2_capabilities[SXE2_SECURITY_PROTOCOL_IPSEC].crypto_capabilities;
	enum rte_crypto_auth_algorithm auth_algo = RTE_CRYPTO_AUTH_NULL;
	enum rte_crypto_cipher_algorithm cipher_algo = RTE_CRYPTO_CIPHER_NULL;
	int32_t ret = -1;

	if (conf->ipsec.direction == RTE_SECURITY_IPSEC_SA_DIR_EGRESS &&
		conf->crypto_xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER) {
		xform = conf->crypto_xform;
		cipher_algo = xform->cipher.algo;
		ret = sxe2_ipsec_valid_cipher(RTE_CRYPTO_CIPHER_OP_ENCRYPT,
					      crypto_cap, xform);
		if (ret)
			goto l_end;

		if (conf->crypto_xform->next) {
			if (conf->crypto_xform->next->type == RTE_CRYPTO_SYM_XFORM_AUTH) {
				auth_algo = conf->crypto_xform->next->auth.algo;
				if (!sxe2_ipsec_valid_algo(auth_algo, cipher_algo)) {
					PMD_LOG_ERR(DRV, "Invalid algo group.");
					ret = -EINVAL;
					goto l_end;
				}
				xform = conf->crypto_xform->next;
				ret = sxe2_ipsec_valid_auth(RTE_CRYPTO_AUTH_OP_GENERATE,
									crypto_cap, xform);
				if (ret)
					goto l_end;
			} else {
				PMD_LOG_ERR(DRV, "Encrypt direction next xform only verify.");
				ret = -EINVAL;
				goto l_end;
			}
		}
	} else if (conf->ipsec.direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS &&
		conf->crypto_xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER) {
		xform = conf->crypto_xform;
		ret = sxe2_ipsec_valid_cipher(RTE_CRYPTO_CIPHER_OP_DECRYPT,
										crypto_cap, xform);
		if (ret)
			goto l_end;

	} else if (conf->ipsec.direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS &&
		conf->crypto_xform->type == RTE_CRYPTO_SYM_XFORM_AUTH) {
		xform = conf->crypto_xform;
		ret = sxe2_ipsec_valid_auth(RTE_CRYPTO_AUTH_OP_VERIFY, crypto_cap, xform);
		if (ret)
			goto l_end;

		if (conf->crypto_xform->next &&
			conf->crypto_xform->next->type == RTE_CRYPTO_SYM_XFORM_CIPHER) {
			auth_algo = conf->crypto_xform->auth.algo;
			cipher_algo = conf->crypto_xform->next->cipher.algo;
			if (!sxe2_ipsec_valid_algo(auth_algo, cipher_algo)) {
				PMD_LOG_ERR(DRV, "Invalid algo group.");
				ret = -EINVAL;
				goto l_end;
			}
			xform = conf->crypto_xform->next;
			ret = sxe2_ipsec_valid_cipher(RTE_CRYPTO_CIPHER_OP_DECRYPT,
										crypto_cap, xform);
			if (ret)
				goto l_end;
		} else {
			PMD_LOG_ERR(DRV, "Not support decrypt direction only verify, but not decrypt.");
			ret = -EINVAL;
			goto l_end;
		}
	} else {
		PMD_LOG_ERR(DRV, "Encrypt/decrypt xform invalid.");
		ret = -EINVAL;
		goto l_end;
	}

	ret = 0;

l_end:
	return ret;
}

static int32_t
sxe2_ipsec_valid_udp(struct rte_security_session_conf *conf)
{
	int32_t ret = -1;
	uint16_t sport = conf->ipsec.udp.sport;
	uint16_t dport = conf->ipsec.udp.dport;

	if (conf->ipsec.options.udp_encap == 0) {
		ret = 0;
		goto l_end;
	}

	if (sport == 0 && dport == 0) {
		PMD_LOG_ERR(DRV, "Invalid udp port, cannot be zero.");
		ret = -1;
		goto l_end;
	}

	if (sport != 0 && dport != 0 && sport != dport) {
		PMD_LOG_ERR(DRV, "Invalid udp port, if sport and dport is not zero, must be equal.");
		ret = -1;
		goto l_end;
	}

	ret = 0;

l_end:
	return ret;
}

static int32_t
sxe2_ipsec_session_conf_valid(struct sxe2_security_ctx *sxe2_sctx,
			      struct rte_security_session_conf *conf)
{
	int32_t ret = -1;

	if (sxe2_sctx == NULL) {
		PMD_LOG_ERR(DRV, "Invalid  security ctx.");
		ret = -EINVAL;
		goto l_end;
	}

	if (conf->action_type !=
		sxe2_sctx->sxe2_capabilities[SXE2_SECURITY_PROTOCOL_IPSEC].action) {
		PMD_LOG_ERR(DRV, "Invalid action specified");
		ret = -EINVAL;
		goto l_end;
	}

	if (conf->ipsec.mode !=
		sxe2_sctx->sxe2_capabilities[SXE2_SECURITY_PROTOCOL_IPSEC].ipsec.mode) {
		PMD_LOG_ERR(DRV, "Invalid IPsec mode specified");
		ret = -EINVAL;
		goto l_end;
	}

	if (conf->ipsec.proto !=
	    sxe2_sctx->sxe2_capabilities[SXE2_SECURITY_PROTOCOL_IPSEC].ipsec.proto) {
		PMD_LOG_ERR(DRV, "Invalid IPsec protocol specified");
		ret = -EINVAL;
		goto l_end;
	}

	if (conf->ipsec.options.esn) {
		PMD_LOG_ERR(DRV, "Not support esn.");
		ret = -EINVAL;
		goto l_end;
	}

	if (conf->ipsec.direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS &&
		conf->ipsec.spi == 0) {
		PMD_LOG_ERR(DRV, "spi cannot be zero.");
		ret = -EINVAL;
		goto l_end;
	}

	if (conf->crypto_xform == NULL) {
		PMD_LOG_ERR(DRV, "Invalid ipsec xform specified");
		ret = -EINVAL;
		goto l_end;
	}

	ret = sxe2_ipsec_valid_udp(conf);
	if (ret)
		goto l_end;

	ret = sxe2_ipsec_valid_xform(sxe2_sctx, conf);
	if (ret)
		goto l_end;

l_end:
	return ret;
}

static void
sxe2_ipsec_session_save(struct sxe2_security_ctx *sxe2_sctx,
			struct rte_security_session_conf *conf,
			struct sxe2_security_session *sxe2_sess, uint16_t sa_id, uint16_t index)
{
	enum rte_crypto_cipher_algorithm cipher_algo   = RTE_CRYPTO_CIPHER_NULL;

	sxe2_sess->adapter = sxe2_sctx->adapter;
	sxe2_sess->direction = conf->ipsec.direction;
	sxe2_sess->protocol = conf->protocol;
	sxe2_sess->mode = conf->ipsec.mode;
	sxe2_sess->sa_proto = conf->ipsec.proto;
	sxe2_sess->sa.spi = conf->ipsec.spi;
	sxe2_sess->sa.hw_idx = sa_id;
	sxe2_sess->sa.sw_idx = index;

	if (conf->ipsec.options.esn) {
		sxe2_sess->esn.enabled = true;
		sxe2_sess->esn.value = conf->ipsec.esn.value;
	}

	if (sxe2_sess->mode == RTE_SECURITY_IPSEC_SA_MODE_TUNNEL)
		sxe2_sess->type = conf->ipsec.tunnel.type;

	if (conf->ipsec.options.udp_encap) {
		sxe2_sess->udp_cap.enabled = true;
		memcpy(&sxe2_sess->udp_cap.value, &conf->ipsec.udp,
			sizeof(struct rte_security_ipsec_udp_param));
	}

	sxe2_sess->pkt_metadata_template.sa_idx = sa_id;
	sxe2_sess->pkt_metadata_template.ol_flags |= SXE2_IPSEC_OL_FLAGS_IS_TUN;
	sxe2_sess->pkt_metadata_template.ol_flags |= SXE2_IPSEC_OL_FLAGS_IS_ESP;

	if (conf->ipsec.direction == RTE_SECURITY_IPSEC_SA_DIR_EGRESS &&
		conf->crypto_xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER) {
		cipher_algo = conf->crypto_xform->cipher.algo;
		sxe2_sess->pkt_metadata_template.algo = sxe2_ipsec_algo_gen(cipher_algo);
		if (conf->crypto_xform->next)
			sxe2_sess->pkt_metadata_template.mode = SXE2_IPSEC_MODE_ENC_AND_AUTH;
		else
			sxe2_sess->pkt_metadata_template.mode = SXE2_IPSEC_MODE_ONLY_ENCRYPT;
	}

	PMD_LOG_INFO(DRV,
		"Save security info to session ctx, said:%u, spi:%u, mode:%u, algo:%u",
		sa_id, sxe2_sess->sa.spi,
		sxe2_sess->pkt_metadata_template.mode,
		sxe2_sess->pkt_metadata_template.algo);
}

static void
sxe2_ipsec_tx_sa_fill(struct sxe2_ipsec_tx_sa *tx_sa,
		      struct rte_security_session_conf *conf)
{
	uint8_t *dst = NULL;
	uint8_t len  = 0;

	memcpy(&tx_sa->xform, &conf->ipsec, sizeof(struct rte_security_ipsec_xform));

	if (conf->crypto_xform->next)
		tx_sa->mode = SXE2_IPSEC_MODE_ENC_AND_AUTH;
	else
		tx_sa->mode = SXE2_IPSEC_MODE_ONLY_ENCRYPT;

	if (conf->crypto_xform->cipher.algo == SXE2_RTE_RTE_CRYPTO_CIPHER_SM4_CBC)
		tx_sa->algo = SXE2_IPSEC_ALGO_SM4_CBC_AND_SM3_96_HMAC;
	else
		tx_sa->algo = SXE2_IPSEC_ALGO_AES_CBC_AND_SHA256_128_HMAC;

	dst = tx_sa->enc_key;
	len = conf->crypto_xform->cipher.key.length;
	memcpy(dst, conf->crypto_xform->cipher.key.data, len);

	if (conf->crypto_xform->next) {
		dst = tx_sa->auth_key;
		len = conf->crypto_xform->next->auth.key.length;
		memcpy(dst, conf->crypto_xform->next->auth.key.data, len);
	}
}

static int32_t
sxe2_ipsec_tx_sa_add(struct sxe2_security_ctx *sxe2_sctx,
		     struct rte_security_session_conf *conf,
		     struct sxe2_security_session *sxe2_sess)
{
	struct sxe2_ipsec_tx_sa *tx_sa = NULL;
	struct rte_bitmap *bmp          = sxe2_sctx->ipsec_ctx.bmp.tx_sa_bmp;
	uint16_t bits                        = sxe2_sctx->ipsec_ctx.max_tx_sa;
	uint16_t index                       = 0xFFFF;
	int32_t ret                         = -1;

	rte_spinlock_lock(&sxe2_sctx->security_lock);
	index = sxe2_ipsec_id_alloc(bmp, bits);
	rte_spinlock_unlock(&sxe2_sctx->security_lock);
	if (index == 0xFFFF) {
		PMD_LOG_ERR(DRV, "Failed to allocate ipsec tx sa index.");
		ret = -ENOMEM;
		goto l_end;
	}
	tx_sa = &sxe2_sctx->ipsec_ctx.tx_sa[index];

	sxe2_ipsec_tx_sa_fill(tx_sa, conf);

	ret = sxe2_drv_ipsec_txsa_add(sxe2_sctx->adapter, tx_sa);
	if (ret) {
		PMD_LOG_ERR(DRV, "Failed to add tx sa.");
		ret = -EIO;
		rte_spinlock_lock(&sxe2_sctx->security_lock);
		sxe2_ipsec_id_free(bmp, index);
		rte_spinlock_unlock(&sxe2_sctx->security_lock);
		goto l_end;
	}

	sxe2_ipsec_session_save(sxe2_sctx, conf, sxe2_sess, tx_sa->hw_sa_id, tx_sa->id);

	PMD_LOG_INFO(DRV, "Add tx sa success, tx sa id: %u, index: %u.",
		tx_sa->hw_sa_id, tx_sa->id);

l_end:
	return ret;
}

static uint16_t
sxe2_ipsec_tcam_id_find(struct sxe2_ipsec_rx_tcam *rx_tcam,
			struct rte_security_ipsec_tunnel_param tunnel, uint16_t len)
{
	struct sxe2_ipsec_rx_tcam *per = NULL;
	uint16_t tcam_id = 0XFFFF;
	uint16_t i       = 0;

	for (i = 0; i < len; i++) {
		per = &rx_tcam[i];
		if (per->ip_addr.type == tunnel.type) {
			if (tunnel.type == RTE_SECURITY_IPSEC_TUNNEL_IPV4 &&
			per->ip_addr.dst_ipv4 == (uint32_t)tunnel.ipv4.dst_ip.s_addr) {
				tcam_id = i;
				goto l_end;
			}
			if (tunnel.type == RTE_SECURITY_IPSEC_TUNNEL_IPV6) {
				if (!memcmp(&tunnel.ipv6, &per->ip_addr.dst_ipv6,
				sizeof(tunnel.ipv6))) {
					tcam_id = i;
					goto l_end;
				}
			}
		}
	}

l_end:
	return tcam_id;
}

static uint16_t
sxe2_ipsec_group_id_find(struct sxe2_ipsec_rx_udp_group *rx_udp_group,
			 uint16_t udp_port, uint8_t sport_en, uint8_t dport_en, uint16_t len)
{
	struct sxe2_ipsec_rx_udp_group *per = NULL;
	uint16_t group_id = 0XFFFF;
	uint16_t i;

	for (i = 0; i < len; i++) {
		per = &rx_udp_group[i];
		if (per->udp_port == udp_port && per->sport_en == sport_en &&
			per->dport_en == dport_en) {
			group_id = i;
			goto l_end;
		}
	}

l_end:
	return group_id;
}

static void
sxe2_ipsec_rx_sa_fill(struct sxe2_ipsec_rx_sa *rx_sa,
		      struct rte_security_session_conf *conf)
{
	uint8_t *dst = NULL;
	uint8_t len = 0;

	memcpy(&rx_sa->xform, &conf->ipsec, sizeof(struct rte_security_ipsec_xform));

	if (conf->crypto_xform->next)
		rx_sa->mode = SXE2_IPSEC_MODE_ENC_AND_AUTH;
	else
		rx_sa->mode = SXE2_IPSEC_MODE_ONLY_ENCRYPT;

	if (conf->crypto_xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER) {
		if (conf->crypto_xform->cipher.algo == SXE2_RTE_RTE_CRYPTO_CIPHER_SM4_CBC)
			rx_sa->algo = SXE2_IPSEC_ALGO_SM4_CBC_AND_SM3_96_HMAC;
		else
			rx_sa->algo = SXE2_IPSEC_ALGO_AES_CBC_AND_SHA256_128_HMAC;
	} else {
		if (conf->crypto_xform->auth.algo == SXE2_RTE_CRYPTO_AUTH_SM3_HMAC)
			rx_sa->algo = SXE2_IPSEC_ALGO_SM4_CBC_AND_SM3_96_HMAC;
		else
			rx_sa->algo = SXE2_IPSEC_ALGO_AES_CBC_AND_SHA256_128_HMAC;
	}

	if (conf->crypto_xform->next) {
		dst = rx_sa->auth_key;
		len = conf->crypto_xform->auth.key.length;
		memcpy(dst, conf->crypto_xform->auth.key.data, len);

		dst = rx_sa->enc_key;
		len = conf->crypto_xform->next->cipher.key.length;
		memcpy(dst, conf->crypto_xform->next->cipher.key.data, len);
	} else {
		dst = rx_sa->enc_key;
		len = conf->crypto_xform->cipher.key.length;
		memcpy(dst, conf->crypto_xform->cipher.key.data, len);
	}

	rx_sa->spi = conf->ipsec.spi;
}

static int32_t
sxe2_ipsec_rx_tcam_fill(struct sxe2_security_ctx *sxe2_sctx, uint16_t *tcam_id,
			struct rte_security_session_conf *conf)
{
	int32_t ret = -1;
	uint16_t len = sxe2_sctx->ipsec_ctx.max_tcam;
	struct sxe2_ipsec_rx_tcam *rx_tcam = NULL;

	*tcam_id = sxe2_ipsec_tcam_id_find(sxe2_sctx->ipsec_ctx.rx_tcam,
			conf->ipsec.tunnel, len);
	if (*tcam_id == 0XFFFF) {
		*tcam_id = sxe2_ipsec_id_alloc(sxe2_sctx->ipsec_ctx.bmp.rx_tcam_bmp, len);
		if (*tcam_id == 0xFFFF) {
			ret = -ENOMEM;
			goto l_end;
		}
		rx_tcam = &sxe2_sctx->ipsec_ctx.rx_tcam[*tcam_id];

		rx_tcam->ip_addr.type = conf->ipsec.tunnel.type;
		if (rx_tcam->ip_addr.type == RTE_SECURITY_IPSEC_TUNNEL_IPV4) {
			rx_tcam->ip_addr.dst_ipv4 = (uint32_t)conf->ipsec.tunnel.ipv4.dst_ip.s_addr;
		} else {
			memcpy(&rx_tcam->ip_addr.dst_ipv6, &conf->ipsec.tunnel.ipv6.dst_addr,
				sizeof(rx_tcam->ip_addr.dst_ipv6));
		}
	} else {
		rx_tcam = &sxe2_sctx->ipsec_ctx.rx_tcam[*tcam_id];
	}
	rx_tcam->ref_cnt++;
	ret = 0;

l_end:
	return ret;
}

static int32_t
sxe2_ipsec_rx_udp_group_fill(struct sxe2_security_ctx *sxe2_sctx, uint16_t *udp_group_id,
			     struct rte_security_session_conf *conf)
{
	int32_t ret = -1;
	uint16_t len = sxe2_sctx->ipsec_ctx.max_udp_group;
	struct sxe2_ipsec_rx_udp_group *rx_udp_group = NULL;
	uint8_t sport_en = 0;
	uint8_t dport_en = 0;
	uint16_t udp_port = 0;

	if (!conf->ipsec.options.udp_encap) {
		ret = 0;
		goto l_end;
	}

	if (conf->ipsec.udp.sport) {
		sport_en = 1;
		udp_port = conf->ipsec.udp.sport;
	} else {
		sport_en = 0;
	}
	if (conf->ipsec.udp.dport) {
		dport_en = 1;
		udp_port = conf->ipsec.udp.dport;
	} else {
		dport_en = 0;
	}

	*udp_group_id = sxe2_ipsec_group_id_find(sxe2_sctx->ipsec_ctx.rx_udp_group,
			udp_port, sport_en, dport_en, len);
	if (*udp_group_id == 0XFFFF) {
		*udp_group_id = sxe2_ipsec_id_alloc(sxe2_sctx->ipsec_ctx.bmp.rx_udp_bmp, len);
		if (*udp_group_id == 0xFFFF) {
			ret = -ENOMEM;
			goto l_end;
		}
		rx_udp_group = &sxe2_sctx->ipsec_ctx.rx_udp_group[*udp_group_id];
		rx_udp_group->sport_en = sport_en;
		rx_udp_group->dport_en = dport_en;
		rx_udp_group->udp_port = udp_port;
	} else {
		rx_udp_group = &sxe2_sctx->ipsec_ctx.rx_udp_group[*udp_group_id];
	}
	rx_udp_group->ref_cnt++;
	ret = 0;

l_end:
	return ret;
}

static int32_t
sxe2_ipsec_rx_sa_add(struct sxe2_security_ctx *sxe2_sctx,
		     struct rte_security_session_conf *conf,
		     struct sxe2_security_session *sxe2_sess)
{
	struct sxe2_ipsec_rx_tcam *rx_tcam = NULL;
	struct sxe2_ipsec_rx_sa *rx_sa     = NULL;
	struct sxe2_ipsec_rx_udp_group *rx_udp_group = NULL;
	struct rte_bitmap *rx_sa_bmp        = sxe2_sctx->ipsec_ctx.bmp.rx_sa_bmp;
	struct rte_bitmap *rx_tcam_bmp      = sxe2_sctx->ipsec_ctx.bmp.rx_tcam_bmp;
	uint16_t sa_bits                         = sxe2_sctx->ipsec_ctx.max_rx_sa;
	uint16_t sa_id                           = 0xFFFF;
	uint16_t tcam_id                         = 0xFFFF;
	uint16_t udp_group_id                    = 0xFFFF;
	int32_t ret                             = -1;

	rte_spinlock_lock(&sxe2_sctx->security_lock);
	sa_id = sxe2_ipsec_id_alloc(rx_sa_bmp, sa_bits);
	if (sa_id == 0xFFFF) {
		PMD_LOG_ERR(DRV, "Failed to allocate ipsec rx sa index.");
		ret = -ENOMEM;
		goto l_end;
	}
	rx_sa = &sxe2_sctx->ipsec_ctx.rx_sa[sa_id];
	sxe2_ipsec_rx_sa_fill(rx_sa, conf);

	ret = sxe2_ipsec_rx_tcam_fill(sxe2_sctx, &tcam_id, conf);
	if (ret) {
		PMD_LOG_ERR(DRV, "Failed to allocate ipsec rx tcam index.");
		sxe2_ipsec_id_free(rx_sa_bmp, sa_id);
		goto l_end;
	}
	rx_sa->tcam_id = tcam_id;
	rx_tcam = &sxe2_sctx->ipsec_ctx.rx_tcam[tcam_id];

	ret = sxe2_ipsec_rx_udp_group_fill(sxe2_sctx, &udp_group_id, conf);
	if (ret) {
		PMD_LOG_ERR(DRV, "Failed to allocate ipsec rx udp group index.");
		sxe2_ipsec_id_free(rx_sa_bmp, sa_id);
		sxe2_ipsec_id_free(rx_tcam_bmp, tcam_id);
		goto l_end;
	}

	if (udp_group_id != 0XFFFF) {
		rx_sa->udp_group_id = (uint8_t)udp_group_id;
		rx_udp_group = &sxe2_sctx->ipsec_ctx.rx_udp_group[udp_group_id];
	} else {
		rx_sa->udp_group_id = 0XFF;
	}

	ret = sxe2_drv_ipsec_rxsa_add(sxe2_sctx->adapter, rx_sa, rx_tcam, rx_udp_group);
	if (ret) {
		PMD_LOG_ERR(DRV, "Failed to add rx sa.");
		sxe2_ipsec_id_free(rx_sa_bmp, sa_id);
		rx_tcam->ref_cnt--;
		if (rx_tcam->ref_cnt == 0)
			sxe2_ipsec_id_free(rx_tcam_bmp, tcam_id);

		if (rx_udp_group != NULL) {
			rx_udp_group->ref_cnt--;
			if (rx_udp_group->ref_cnt == 0)
				sxe2_ipsec_id_free(sxe2_sctx->ipsec_ctx.bmp.rx_udp_bmp,
						   udp_group_id);
		}

		ret = -EIO;
		goto l_end;
	}

	sxe2_ipsec_session_save(sxe2_sctx, conf, sxe2_sess, rx_sa->hw_sa_id, rx_sa->id);

	PMD_LOG_INFO(DRV, "Add rx sa success, rx sa id: %u, rx ip id: %u, group id: %u, index: %u.",
				rx_sa->hw_sa_id, rx_sa->hw_ip_id, rx_sa->udp_group_id, rx_sa->id);

l_end:
	rte_spinlock_unlock(&sxe2_sctx->security_lock);
	return ret;
}

static int32_t
sxe2_ipsec_hw_table_add(struct sxe2_security_ctx *sxe2_sctx,
			struct rte_security_session_conf *conf,
			struct sxe2_security_session *sxe2_sess)
{
	int32_t ret = -1;

	switch (conf->ipsec.direction) {
	case RTE_SECURITY_IPSEC_SA_DIR_EGRESS:
		ret = sxe2_ipsec_tx_sa_add(sxe2_sctx, conf, sxe2_sess);
		break;
	case RTE_SECURITY_IPSEC_SA_DIR_INGRESS:
		ret = sxe2_ipsec_rx_sa_add(sxe2_sctx, conf, sxe2_sess);
		break;
	default:
		PMD_LOG_ERR(DRV, "Invalid sa direction.");
		ret = -EINVAL;
		break;
	}

	return ret;
}

int sxe2_ipsec_session_create(void *device,
			      struct rte_security_session_conf *conf,
			      struct sxe2_security_session *sxe2_sess)
{
	struct rte_eth_dev *eth_dev = (struct rte_eth_dev *)device;
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(eth_dev);
	struct sxe2_security_ctx *sxe2_sctx = &adapter->security_ctx;
	int32_t ret = -1;

	ret = sxe2_ipsec_session_conf_valid(sxe2_sctx, conf);
	if (ret) {
		PMD_LOG_ERR(DRV, "Input ipsec session conf invalid.");
		goto l_end;
	}

	ret = sxe2_ipsec_hw_table_add(sxe2_sctx, conf, sxe2_sess);
	if (ret)
		goto l_end;

l_end:
	return ret;
}

static int32_t
sxe2_ipsec_tx_sa_delete(struct sxe2_security_ctx *sxe2_sctx,
			struct sxe2_security_session *sxe2_sess)
{
	struct sxe2_ipsec_tx_sa *tx_sa = NULL;
	uint16_t sa_id = sxe2_sess->sa.hw_idx;
	uint16_t sw_sa_id = sxe2_sess->sa.sw_idx;
	int32_t ret   = -1;

	if (sw_sa_id >= sxe2_sctx->ipsec_ctx.max_tx_sa) {
		ret = 0;
		PMD_LOG_WARN(DRV, "invalid sw sa id: %u.", sw_sa_id);
		goto l_end;
	}

	if (!rte_bitmap_get(sxe2_sctx->ipsec_ctx.bmp.tx_sa_bmp, sw_sa_id)) {
		ret = 0;
		PMD_LOG_WARN(DRV, "bitmap not set, index: %u.", sw_sa_id);
		goto l_end;
	}

	tx_sa = &sxe2_sctx->ipsec_ctx.tx_sa[sw_sa_id];

	if (tx_sa->hw_sa_id != sa_id) {
		ret = 0;
		PMD_LOG_WARN(DRV, "invalid hw sa id: %u != %u.", sa_id, tx_sa->hw_sa_id);
		goto l_end;
	}

	ret = sxe2_drv_ipsec_txsa_delete(sxe2_sctx->adapter, sa_id);
	if (ret)
		goto l_end;

	rte_spinlock_lock(&sxe2_sctx->security_lock);
	sxe2_ipsec_id_free(sxe2_sctx->ipsec_ctx.bmp.tx_sa_bmp, sw_sa_id);
	rte_spinlock_unlock(&sxe2_sctx->security_lock);

l_end:
	return ret;
}

static int32_t
sxe2_ipsec_rx_sa_delete(struct sxe2_security_ctx *sxe2_sctx,
			struct sxe2_security_session *sxe2_sess)
{
	struct sxe2_ipsec_rx_udp_group *rx_udp = NULL;
	struct sxe2_ipsec_rx_tcam *rx_tcam = NULL;
	struct sxe2_ipsec_rx_sa *rx_sa = NULL;
	uint16_t sa_id                            = sxe2_sess->sa.hw_idx;
	uint16_t sw_sa_id                         = sxe2_sess->sa.sw_idx;
	int32_t ret                              = -1;

	if (sw_sa_id >= sxe2_sctx->ipsec_ctx.max_rx_sa) {
		ret = 0;
		PMD_LOG_WARN(DRV, "invalid sw sa id: %u.", sw_sa_id);
		goto l_end;
	}

	if (!rte_bitmap_get(sxe2_sctx->ipsec_ctx.bmp.rx_sa_bmp, sw_sa_id)) {
		ret = 0;
		PMD_LOG_INFO(DRV, "bitmap not set, id: %u.", sw_sa_id);
		goto l_end;
	}

	rx_sa = &sxe2_sctx->ipsec_ctx.rx_sa[sw_sa_id];

	if (rx_sa->hw_sa_id != sa_id) {
		ret = 0;
		PMD_LOG_WARN(DRV, "invalid hw sa id: %u != %u.", sa_id, rx_sa->hw_sa_id);
		goto l_end;
	}

	ret = sxe2_drv_ipsec_rxsa_delete(sxe2_sctx->adapter, rx_sa);
	if (ret)
		goto l_end;

	rte_spinlock_lock(&sxe2_sctx->security_lock);
	sxe2_ipsec_id_free(sxe2_sctx->ipsec_ctx.bmp.rx_sa_bmp, sw_sa_id);

	rx_tcam = &sxe2_sctx->ipsec_ctx.rx_tcam[rx_sa->tcam_id];
	rx_tcam->ref_cnt--;
	if (rx_tcam->ref_cnt == 0)
		sxe2_ipsec_id_free(sxe2_sctx->ipsec_ctx.bmp.rx_tcam_bmp, rx_sa->tcam_id);

	if (rx_sa->udp_group_id == 0xFF) {
		PMD_LOG_INFO(DRV, "Not need to release udp group resource.");
		rte_spinlock_unlock(&sxe2_sctx->security_lock);
		goto l_end;
	}
	rx_udp = &sxe2_sctx->ipsec_ctx.rx_udp_group[rx_sa->udp_group_id];
	rx_udp->ref_cnt--;
	if (rx_udp->ref_cnt == 0)
		sxe2_ipsec_id_free(sxe2_sctx->ipsec_ctx.bmp.rx_udp_bmp, rx_sa->udp_group_id);
	rte_spinlock_unlock(&sxe2_sctx->security_lock);

l_end:
	return ret;
}

static int32_t
sxe2_ipsec_hw_table_delete(struct sxe2_security_ctx *sxe2_sctx,
			   struct sxe2_security_session *sxe2_sess)
{
	int32_t ret = -1;

	switch (sxe2_sess->direction) {
	case RTE_SECURITY_IPSEC_SA_DIR_EGRESS:
		ret = sxe2_ipsec_tx_sa_delete(sxe2_sctx, sxe2_sess);
		break;
	case RTE_SECURITY_IPSEC_SA_DIR_INGRESS:
		ret = sxe2_ipsec_rx_sa_delete(sxe2_sctx, sxe2_sess);
		break;
	default:
		PMD_LOG_ERR(DRV, "Invalid sa direction.");
		ret = -EINVAL;
		break;
	}

	return ret;
}

int sxe2_ipsec_session_destroy(void *device, struct rte_security_session *session)
{
	struct rte_eth_dev *eth_dev = (struct rte_eth_dev *)device;
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(eth_dev);
	struct sxe2_security_ctx *sxe2_sctx = &adapter->security_ctx;
	struct sxe2_security_session *sxe2_sess = NULL;
	sxe2_sess = SECURITY_GET_SESS_PRIV(session);
	int32_t ret = -1;

	if (unlikely(sxe2_sess == NULL || sxe2_sess->adapter != adapter)) {
		PMD_LOG_ERR(DRV, "Invalid device adapter.");
		ret = -EINVAL;
		goto l_end;
	}

	ret = sxe2_ipsec_hw_table_delete(sxe2_sctx, sxe2_sess);
	if (ret) {
		ret = -EIO;
		PMD_LOG_ERR(DRV, "Failed to delete ipsec hw tables.");
		goto l_end;
	}

	memset(sxe2_sess, 0, sizeof(struct sxe2_security_session));

	PMD_LOG_INFO(DRV, "Delete ipsec session success, sa_id: %u, spi: %u.",
			sxe2_sess->sa.hw_idx, sxe2_sess->sa.spi);

l_end:
	return ret;
}

int sxe2_ipsec_pkt_metadata_set(void *device, struct rte_security_session *session,
				struct rte_mbuf *m, void *params)
{
	struct rte_eth_dev *eth_dev = (struct rte_eth_dev *)device;
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(eth_dev);
	struct sxe2_security_ctx *sxe2_sctx = &adapter->security_ctx;
	struct sxe2_security_session *sxe2_sess = NULL;
	struct sxe2_ipsec_pkt_metadata *md             = NULL;
	uint16_t offset                                      = 0;
	int32_t ret                                         = -1;

	sxe2_sess = SECURITY_GET_SESS_PRIV(session);
	if (unlikely(sxe2_sess == NULL || sxe2_sess->adapter != adapter)) {
		PMD_LOG_ERR(DRV, "Invalid parameters.");
		ret = -EINVAL;
		goto l_end;
	}

	offset = ((struct sxe2_ipsec_metadata_params *)params)->esp_header_offset;
	if (offset <= IPSEC_ESP_OFFSET_MIN || offset >= IPSEC_ESP_OFFSET_MAX) {
		PMD_LOG_ERR(DRV, "Invalid esp header offset.");
		ret = -EINVAL;
		goto l_end;
	}

	md = RTE_MBUF_DYNFIELD(m, sxe2_sctx->ipsec_ctx.md_offset, struct sxe2_ipsec_pkt_metadata *);

	memcpy(md, &sxe2_sess->pkt_metadata_template, sizeof(struct sxe2_ipsec_pkt_metadata));
	md->esp_head_offset = offset;

	PMD_LOG_INFO(DRV, "ipsec metadata set, offset:%u, said:%u, mode:%u, algo:%u.", offset,
		sxe2_sess->pkt_metadata_template.sa_idx, sxe2_sess->pkt_metadata_template.mode,
		sxe2_sess->pkt_metadata_template.algo);

	ret = 0;

l_end:
	return ret;
}

int sxe2_ipsec_pkt_md_offset_get(struct sxe2_adapter *adapter)
{
	return adapter->security_ctx.ipsec_ctx.md_offset;
}

static void sxe2_ipsec_enc_aes_cbc_fill(struct rte_cryptodev_capabilities *cap)
{
	cap->sym.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER;

	cap->sym.cipher.algo = SXE2_RTE_CRYPTO_CIPHER_AES_CBC;

	cap->sym.cipher.block_size = SXE2_SECURITY_BLOCK_SIZE_16;

	cap->sym.cipher.key_size.min = SXE2_IPSEC_AES_KEY_MIN;
	cap->sym.cipher.key_size.max = SXE2_IPSEC_AES_KEY_MAX;
	cap->sym.cipher.key_size.increment = SXE2_IPSEC_AES_KEY_INC;

	cap->sym.cipher.iv_size.min = SXE2_IPSEC_AES_IV_MIN;
	cap->sym.cipher.iv_size.max = SXE2_IPSEC_AES_IV_MAX;
	cap->sym.cipher.iv_size.increment = SXE2_IPSEC_AES_IV_INC;

	cap->sym.cipher.dataunit_set |= RTE_CRYPTO_CIPHER_DATA_UNIT_LEN_512_BYTES;
}

static void sxe2_ipsec_enc_sm4_cbc_fill(struct rte_cryptodev_capabilities *cap)
{
	cap->sym.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER;

	cap->sym.cipher.algo = SXE2_RTE_RTE_CRYPTO_CIPHER_SM4_CBC;

	cap->sym.cipher.block_size = SXE2_SECURITY_BLOCK_SIZE_16;

	cap->sym.cipher.key_size.min = SXE2_IPSEC_SM4_KEY_MIN;
	cap->sym.cipher.key_size.max = SXE2_IPSEC_SM4_KEY_MAX;
	cap->sym.cipher.key_size.increment = SXE2_IPSEC_SM4_KEY_INC;

	cap->sym.cipher.iv_size.min = SXE2_IPSEC_SM4_IV_MIN;
	cap->sym.cipher.iv_size.max = SXE2_IPSEC_SM4_IV_MAX;
	cap->sym.cipher.iv_size.increment = SXE2_IPSEC_SM4_IV_INC;

	cap->sym.cipher.dataunit_set |= RTE_CRYPTO_CIPHER_DATA_UNIT_LEN_512_BYTES;
}

static void sxe2_ipsec_auth_sha_hmac_fill(struct rte_cryptodev_capabilities *cap)
{
	cap->sym.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH;

	cap->sym.auth.algo = SXE2_RTE_CRYPTO_AUTH_SHA256_HMAC;

	cap->sym.auth.block_size = SXE2_SECURITY_BLOCK_SIZE_64;

	cap->sym.auth.key_size.min = SXE2_IPSEC_SHA_KEY_MIN;
	cap->sym.auth.key_size.max = SXE2_IPSEC_SHA_KEY_MAX;
	cap->sym.auth.key_size.increment = SXE2_IPSEC_SHA_KEY_INC;

	cap->sym.auth.iv_size.min = SXE2_IPSEC_SHA_IV_MIN;
	cap->sym.auth.iv_size.max = SXE2_IPSEC_SHA_IV_MAX;
	cap->sym.auth.iv_size.increment = SXE2_IPSEC_SHA_IV_INC;

	cap->sym.auth.digest_size.min = SXE2_IPSEC_SHA_DIGEST_MIN;
	cap->sym.auth.digest_size.max = SXE2_IPSEC_SHA_DIGEST_MAX;
	cap->sym.auth.digest_size.increment = SXE2_IPSEC_SHA_DIGEST_INC;

	cap->sym.auth.aad_size.min = SXE2_IPSEC_AAD_MIN;
	cap->sym.auth.aad_size.max = SXE2_IPSEC_AAD_MAX;
	cap->sym.auth.aad_size.increment = SXE2_IPSEC_AAD_INC;
}

static void sxe2_ipsec_auth_sm3_hmac_fill(struct rte_cryptodev_capabilities *cap)
{
	cap->sym.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH;

	cap->sym.auth.algo = SXE2_RTE_CRYPTO_AUTH_SM3_HMAC;

	cap->sym.auth.block_size = SXE2_SECURITY_BLOCK_SIZE_64;

	cap->sym.auth.key_size.min = SXE2_IPSEC_SM3_KEY_MIN;
	cap->sym.auth.key_size.max = SXE2_IPSEC_SM3_KEY_MAX;
	cap->sym.auth.key_size.increment = SXE2_IPSEC_SM3_KEY_INC;

	cap->sym.auth.iv_size.min = SXE2_IPSEC_SM3_IV_MIN;
	cap->sym.auth.iv_size.max = SXE2_IPSEC_SM3_IV_MAX;
	cap->sym.auth.iv_size.increment = SXE2_IPSEC_SM3_IV_INC;

	cap->sym.auth.digest_size.min = SXE2_IPSEC_SM3_DIGEST_MIN;
	cap->sym.auth.digest_size.max = SXE2_IPSEC_SM3_DIGEST_MAX;
	cap->sym.auth.digest_size.increment = SXE2_IPSEC_SM3_DIGEST_INC;

	cap->sym.auth.aad_size.min = SXE2_IPSEC_AAD_MIN;
	cap->sym.auth.aad_size.max = SXE2_IPSEC_AAD_MAX;
	cap->sym.auth.aad_size.increment = SXE2_IPSEC_AAD_INC;
}

static int32_t
sxe2_ipsec_capabilities_init(struct sxe2_security_ctx *sxe2_sctx)
{
	struct rte_cryptodev_capabilities *capabilities = NULL;
	struct sxe2_security_capabilities *sxe2_cap   =
			&sxe2_sctx->sxe2_capabilities[SXE2_SECURITY_PROTOCOL_IPSEC];
	int32_t ret                                         = -1;
	uint8_t index                                        = 0;

	sxe2_cap->action = RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO;
	sxe2_cap->ipsec.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP;
	sxe2_cap->ipsec.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL;
	sxe2_cap->ipsec.options.stats = 1;

	capabilities = rte_zmalloc("security_caps",
				sizeof(struct rte_cryptodev_capabilities) * SXE2_IPSEC_CAP_MAX, 0);
	if (capabilities == NULL) {
		ret = -ENOMEM;
		goto l_end;
	}

	for (index = 0; index < SXE2_IPSEC_CAP_MAX; index++) {
		capabilities[index].op = RTE_CRYPTO_OP_TYPE_SYMMETRIC;
		switch (index) {
		case SXE2_IPSEC_CAP_ENC_AES_CBC:
			sxe2_ipsec_enc_aes_cbc_fill(&capabilities[index]);
			break;
		case SXE2_IPSEC_CAP_ENC_SM4_CBC:
			sxe2_ipsec_enc_sm4_cbc_fill(&capabilities[index]);
			break;
		case SXE2_IPSEC_CAP_AUTH_SHA256_HMAC:
			sxe2_ipsec_auth_sha_hmac_fill(&capabilities[index]);
			break;
		case SXE2_IPSEC_CAP_AUTH_SM3_HMAC:
			sxe2_ipsec_auth_sm3_hmac_fill(&capabilities[index]);
			break;
		default:
			break;
		}
	}

	sxe2_cap->crypto_capabilities = capabilities;
	ret = 0;

l_end:
	return ret;
}

static void
sxe2_ipsec_tx_sa_init(struct sxe2_ipsec_tx_sa *tx_sa, uint16_t len)
{
	struct sxe2_ipsec_tx_sa *per = NULL;
	uint16_t i;

	memset(tx_sa, 0, sizeof(struct sxe2_ipsec_tx_sa) * len);
	for (i = 0; i < len; i++) {
		per = &tx_sa[i];
		per->id = i;
	}
}

static void
sxe2_ipsec_rx_sa_init(struct sxe2_ipsec_rx_sa *rx_sa, uint16_t len)
{
	struct sxe2_ipsec_rx_sa *per = NULL;
	uint16_t i;

	memset(rx_sa, 0, sizeof(struct sxe2_ipsec_rx_sa) * len);
	for (i = 0; i < len; i++) {
		per = &rx_sa[i];
		per->id = i;
	}
}

static void
sxe2_ipsec_rx_tcam_init(struct sxe2_ipsec_rx_tcam *rx_tcam, uint16_t len)
{
	struct sxe2_ipsec_rx_tcam *per = NULL;
	uint16_t i;

	memset(rx_tcam, 0, sizeof(struct sxe2_ipsec_rx_tcam) * len);
	for (i = 0; i < len; i++) {
		per = &rx_tcam[i];
		per->id = i;
	}
}

static void
sxe2_ipsec_rx_udp_group_init(struct sxe2_ipsec_rx_udp_group *rx_udp_group, uint16_t len)
{
	struct sxe2_ipsec_rx_udp_group *per = NULL;
	uint16_t i;

	memset(rx_udp_group, 0, sizeof(struct sxe2_ipsec_rx_udp_group) * len);
	for (i = 0; i < len; i++) {
		per = &rx_udp_group[i];
		per->id = i;
	}
}

static int32_t
sxe2_ipsec_hw_table_init(struct sxe2_security_ctx *sxe2_sctx)
{
	struct sxe2_ipsec_tx_sa *tx_sa = NULL;
	struct sxe2_ipsec_rx_sa *rx_sa = NULL;
	struct sxe2_ipsec_rx_tcam *rx_tcam = NULL;
	struct sxe2_ipsec_rx_udp_group *rx_udp_group = NULL;
	uint16_t max_tx_sa = sxe2_sctx->ipsec_ctx.max_tx_sa;
	uint16_t max_rx_sa = sxe2_sctx->ipsec_ctx.max_rx_sa;
	uint16_t max_tcam  = sxe2_sctx->ipsec_ctx.max_tcam;
	uint16_t max_udp_group  = sxe2_sctx->ipsec_ctx.max_udp_group;
	int32_t ret       = -1;

	tx_sa = rte_zmalloc("sxe2_ipsec_tx_sa", sizeof(struct sxe2_ipsec_tx_sa) * max_tx_sa, 0);
	if (tx_sa == NULL) {
		ret = -ENOMEM;
		goto l_end;
	}
	sxe2_ipsec_tx_sa_init(tx_sa, max_tx_sa);
	sxe2_sctx->ipsec_ctx.tx_sa = tx_sa;

	rx_sa = rte_zmalloc("sxe2_ipsec_rx_sa", sizeof(struct sxe2_ipsec_rx_sa) * max_rx_sa, 0);
	if (rx_sa == NULL) {
		ret = -ENOMEM;
		goto l_end;
	}
	sxe2_ipsec_rx_sa_init(rx_sa, max_rx_sa);
	sxe2_sctx->ipsec_ctx.rx_sa = rx_sa;

	rx_tcam = rte_zmalloc("sxe2_ipsec_rx_tcam",
				sizeof(struct sxe2_ipsec_rx_tcam) * max_tcam, 0);
	if (rx_tcam == NULL) {
		ret = -ENOMEM;
		goto l_end;
	}
	sxe2_ipsec_rx_tcam_init(rx_tcam, max_tcam);
	sxe2_sctx->ipsec_ctx.rx_tcam = rx_tcam;

	rx_udp_group = rte_zmalloc("sxe2_ipsec_rx_udp_group",
				sizeof(struct sxe2_ipsec_rx_udp_group) * max_udp_group, 0);
	if (rx_udp_group == NULL) {
		ret = -ENOMEM;
		goto l_end;
	}
	sxe2_ipsec_rx_udp_group_init(rx_udp_group, max_udp_group);
	sxe2_sctx->ipsec_ctx.rx_udp_group = rx_udp_group;

	ret = 0;

l_end:
	if (ret) {
		if (tx_sa != NULL) {
			rte_free(tx_sa);
			sxe2_sctx->ipsec_ctx.tx_sa = NULL;
		}
		if (rx_sa != NULL) {
			rte_free(rx_sa);
			sxe2_sctx->ipsec_ctx.rx_sa = NULL;
		}
		if (rx_tcam != NULL) {
			rte_free(rx_tcam);
			sxe2_sctx->ipsec_ctx.rx_tcam = NULL;
		}
		if (rx_udp_group != NULL) {
			rte_free(rx_udp_group);
			sxe2_sctx->ipsec_ctx.rx_udp_group = NULL;
		}
	}
	return ret;
}

int32_t sxe2_ipsec_init(struct sxe2_adapter *adapter)
{
	struct sxe2_security_ctx *sxe2_sctx = &adapter->security_ctx;
	struct sxe2_security_capabilities *sxe2_cap = NULL;
	int32_t ret                               = -1;
	struct rte_mbuf_dynfield pkt_md_dynfield = {
	.name = "sxe2_ipsec_pkt_metadata",
		.size = sizeof(struct sxe2_ipsec_pkt_metadata),
		.align = alignof(struct sxe2_ipsec_pkt_metadata)
	};

	PMD_LOG_INFO(INIT, "Init ipsec.");

	sxe2_sctx->ipsec_ctx.md_offset = rte_mbuf_dynfield_register(&pkt_md_dynfield);
	if (sxe2_sctx->ipsec_ctx.md_offset < 0) {
		PMD_LOG_ERR(INIT, "Failed to register ipsec mbuf dynamic field.");
		ret = -EIO;
		goto l_end;
	}

	ret = sxe2_ipsec_capabilities_init(sxe2_sctx);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to init ipsec capabilities.");
		goto l_end;
	}

	ret = sxe2_drv_ipsec_get_capa(adapter);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to get ipsec capabilities.");
		goto l_caps_free;
	}

	ret = sxe2_ipsec_bitmap_init(sxe2_sctx);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to init ipsec bitmap.");
		goto l_caps_free;
	}

	ret = sxe2_ipsec_hw_table_init(sxe2_sctx);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to init ipsec hw table.");
		goto l_bitmap_free;
	}

	goto l_end;

l_bitmap_free:

	if (sxe2_sctx->ipsec_ctx.bmp.tx_sa_mem != NULL) {
		rte_free(sxe2_sctx->ipsec_ctx.bmp.tx_sa_mem);
		sxe2_sctx->ipsec_ctx.bmp.tx_sa_mem = NULL;
	}
	if (sxe2_sctx->ipsec_ctx.bmp.rx_sa_mem != NULL) {
		rte_free(sxe2_sctx->ipsec_ctx.bmp.rx_sa_mem);
		sxe2_sctx->ipsec_ctx.bmp.rx_sa_mem = NULL;
	}
	if (sxe2_sctx->ipsec_ctx.bmp.rx_tcam_mem != NULL) {
		rte_free(sxe2_sctx->ipsec_ctx.bmp.rx_tcam_mem);
		sxe2_sctx->ipsec_ctx.bmp.rx_tcam_mem = NULL;
	}
	if (sxe2_sctx->ipsec_ctx.bmp.rx_udp_mem != NULL) {
		rte_free(sxe2_sctx->ipsec_ctx.bmp.rx_udp_mem);
		sxe2_sctx->ipsec_ctx.bmp.rx_udp_mem = NULL;
	}
l_caps_free:
	sxe2_cap = &sxe2_sctx->sxe2_capabilities[SXE2_SECURITY_PROTOCOL_IPSEC];
	if (sxe2_cap->crypto_capabilities != NULL) {
		rte_free(sxe2_cap->crypto_capabilities);
		sxe2_cap->crypto_capabilities = NULL;
	}
l_end:
	return ret;
}

void sxe2_ipsec_uinit(struct sxe2_adapter *adapter)
{
	struct sxe2_security_ctx *sxe2_sctx = &adapter->security_ctx;
	struct sxe2_security_capabilities *sxe2_cap   =
			&sxe2_sctx->sxe2_capabilities[SXE2_SECURITY_PROTOCOL_IPSEC];
	struct sxe2_ipsec_tx_sa *tx_sa = sxe2_sctx->ipsec_ctx.tx_sa;
	struct sxe2_ipsec_rx_sa *rx_sa = sxe2_sctx->ipsec_ctx.rx_sa;
	struct sxe2_ipsec_rx_tcam *rx_tcam = sxe2_sctx->ipsec_ctx.rx_tcam;
	struct sxe2_ipsec_rx_udp_group *rx_udp_group = sxe2_sctx->ipsec_ctx.rx_udp_group;

	PMD_LOG_INFO(INIT, "Uinit ipsec.");

	(void)sxe2_drv_ipsec_resource_clear(adapter);

	if (sxe2_sctx->ipsec_ctx.bmp.tx_sa_mem != NULL) {
		rte_free(sxe2_sctx->ipsec_ctx.bmp.tx_sa_mem);
		sxe2_sctx->ipsec_ctx.bmp.tx_sa_mem = NULL;
	}
	if (sxe2_sctx->ipsec_ctx.bmp.rx_sa_mem != NULL) {
		rte_free(sxe2_sctx->ipsec_ctx.bmp.rx_sa_mem);
		sxe2_sctx->ipsec_ctx.bmp.rx_sa_mem = NULL;
	}
	if (sxe2_sctx->ipsec_ctx.bmp.rx_tcam_mem != NULL) {
		rte_free(sxe2_sctx->ipsec_ctx.bmp.rx_tcam_mem);
		sxe2_sctx->ipsec_ctx.bmp.rx_tcam_mem = NULL;
	}
	if (sxe2_sctx->ipsec_ctx.bmp.rx_udp_mem != NULL) {
		rte_free(sxe2_sctx->ipsec_ctx.bmp.rx_udp_mem);
		sxe2_sctx->ipsec_ctx.bmp.rx_udp_mem = NULL;
	}

	if (tx_sa != NULL) {
		rte_free(tx_sa);
		sxe2_sctx->ipsec_ctx.tx_sa = NULL;
	}
	if (rx_sa != NULL) {
		rte_free(rx_sa);
		sxe2_sctx->ipsec_ctx.rx_sa = NULL;
	}
	if (rx_tcam != NULL) {
		rte_free(rx_tcam);
		sxe2_sctx->ipsec_ctx.rx_tcam = NULL;
	}
	if (rx_udp_group != NULL) {
		rte_free(rx_udp_group);
		sxe2_sctx->ipsec_ctx.rx_udp_group = NULL;
	}

	if (sxe2_cap->crypto_capabilities != NULL) {
		rte_free(sxe2_cap->crypto_capabilities);
		sxe2_cap->crypto_capabilities = NULL;
	}
}
