/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include <rte_cryptodev.h>
#include <rte_ethdev.h>
#include <rte_security_driver.h>
#include <rte_security.h>

#include "iavf.h"
#include "iavf_rxtx.h"
#include "iavf_log.h"
#include "iavf_generic_flow.h"

#include "iavf_ipsec_crypto.h"
#include "iavf_ipsec_crypto_capabilities.h"

/**
 * iAVF IPsec Crypto Security Context
 */
struct iavf_security_ctx {
	struct iavf_adapter *adapter;
	int pkt_md_offset;
	struct rte_cryptodev_capabilities *crypto_capabilities;
};

/**
 * iAVF IPsec Crypto Security Session Parameters
 */
struct iavf_security_session {
	struct iavf_adapter *adapter;

	enum rte_security_ipsec_sa_mode mode;
	enum rte_security_ipsec_tunnel_type type;
	enum rte_security_ipsec_sa_direction direction;

	struct {
		uint32_t spi; /* Security Parameter Index */
		uint32_t hw_idx; /* SA Index in hardware table */
	} sa;

	struct {
		uint8_t enabled :1;
		union {
			uint64_t value;
			struct {
				uint32_t hi;
				uint32_t low;
			};
		};
	} esn;

	struct {
		uint8_t enabled :1;
	} udp_encap;

	size_t iv_sz;
	size_t icv_sz;
	size_t block_sz;

	struct iavf_ipsec_crypto_pkt_metadata pkt_metadata_template;
};
/**
 *  IV Length field in IPsec Tx Desc uses the following encoding:
 *
 *  0B - 0
 *  4B - 1
 *  8B - 2
 *  16B - 3
 *
 * but we also need the IV Length for TSO to correctly calculate the total
 * header length so placing it in the upper 6-bits here for easier retrieval.
 */
static inline uint8_t
calc_ipsec_desc_iv_len_field(uint16_t iv_sz)
{
	uint8_t iv_length = IAVF_IPSEC_IV_LEN_NONE;

	switch (iv_sz) {
	case 4:
		iv_length = IAVF_IPSEC_IV_LEN_DW;
		break;
	case 8:
		iv_length = IAVF_IPSEC_IV_LEN_DDW;
		break;
	case 16:
		iv_length = IAVF_IPSEC_IV_LEN_QDW;
		break;
	}

	return (iv_sz << 2) | iv_length;
}

static unsigned int
iavf_ipsec_crypto_session_size_get(void *device __rte_unused)
{
	return sizeof(struct iavf_security_session);
}

static const struct rte_cryptodev_symmetric_capability *
get_capability(struct iavf_security_ctx *iavf_sctx __rte_unused,
	uint32_t algo, uint32_t type)
{
	const struct rte_cryptodev_capabilities *capability;
	int i = 0;

	capability = &iavf_crypto_capabilities[i];

	while (capability->op != RTE_CRYPTO_OP_TYPE_UNDEFINED) {
		if (capability->op == RTE_CRYPTO_OP_TYPE_SYMMETRIC &&
			(uint32_t)capability->sym.xform_type == type &&
			(uint32_t)capability->sym.cipher.algo == algo)
			return &capability->sym;
		/** try next capability */
		capability = &iavf_crypto_capabilities[i++];
	}

	return NULL;
}

static const struct rte_cryptodev_symmetric_capability *
get_auth_capability(struct iavf_security_ctx *iavf_sctx,
	enum rte_crypto_auth_algorithm algo)
{
	return get_capability(iavf_sctx, algo, RTE_CRYPTO_SYM_XFORM_AUTH);
}

static const struct rte_cryptodev_symmetric_capability *
get_cipher_capability(struct iavf_security_ctx *iavf_sctx,
	enum rte_crypto_cipher_algorithm algo)
{
	return get_capability(iavf_sctx, algo, RTE_CRYPTO_SYM_XFORM_CIPHER);
}
static const struct rte_cryptodev_symmetric_capability *
get_aead_capability(struct iavf_security_ctx *iavf_sctx,
	enum rte_crypto_aead_algorithm algo)
{
	return get_capability(iavf_sctx, algo, RTE_CRYPTO_SYM_XFORM_AEAD);
}

static uint16_t
get_cipher_blocksize(struct iavf_security_ctx *iavf_sctx,
	enum rte_crypto_cipher_algorithm algo)
{
	const struct rte_cryptodev_symmetric_capability *capability;

	capability = get_cipher_capability(iavf_sctx, algo);
	if (capability == NULL)
		return 0;

	return capability->cipher.block_size;
}

static uint16_t
get_aead_blocksize(struct iavf_security_ctx *iavf_sctx,
	enum rte_crypto_aead_algorithm algo)
{
	const struct rte_cryptodev_symmetric_capability *capability;

	capability = get_aead_capability(iavf_sctx, algo);
	if (capability == NULL)
		return 0;

	return capability->cipher.block_size;
}

static uint16_t
get_auth_blocksize(struct iavf_security_ctx *iavf_sctx,
	enum rte_crypto_auth_algorithm algo)
{
	const struct rte_cryptodev_symmetric_capability *capability;

	capability = get_auth_capability(iavf_sctx, algo);
	if (capability == NULL)
		return 0;

	return capability->auth.block_size;
}

static uint8_t
calc_context_desc_cipherblock_sz(size_t len)
{
	switch (len) {
	case 8:
		return 0x2;
	case 16:
		return 0x3;
	default:
		return 0x0;
	}
}

static int
valid_length(uint32_t len, uint32_t min, uint32_t max, uint32_t increment)
{
	if (len < min || len > max)
		return false;

	if (increment == 0)
		return true;

	if ((len - min) % increment)
		return false;

	/* make sure it fits in the key array */
	if (len > VIRTCHNL_IPSEC_MAX_KEY_LEN)
		return false;

	return true;
}

static int
valid_auth_xform(struct iavf_security_ctx *iavf_sctx,
	struct rte_crypto_auth_xform *auth)
{
	const struct rte_cryptodev_symmetric_capability *capability;

	capability = get_auth_capability(iavf_sctx, auth->algo);
	if (capability == NULL)
		return false;

	/* verify key size */
	if (!valid_length(auth->key.length,
		capability->auth.key_size.min,
		capability->auth.key_size.max,
		capability->aead.key_size.increment))
		return false;

	return true;
}

static int
valid_cipher_xform(struct iavf_security_ctx *iavf_sctx,
	struct rte_crypto_cipher_xform *cipher)
{
	const struct rte_cryptodev_symmetric_capability *capability;

	capability = get_cipher_capability(iavf_sctx, cipher->algo);
	if (capability == NULL)
		return false;

	/* verify key size */
	if (!valid_length(cipher->key.length,
		capability->cipher.key_size.min,
		capability->cipher.key_size.max,
		capability->cipher.key_size.increment))
		return false;

	return true;
}

static int
valid_aead_xform(struct iavf_security_ctx *iavf_sctx,
	struct rte_crypto_aead_xform *aead)
{
	const struct rte_cryptodev_symmetric_capability *capability;

	capability = get_aead_capability(iavf_sctx, aead->algo);
	if (capability == NULL)
		return false;

	/* verify key size */
	if (!valid_length(aead->key.length,
		capability->aead.key_size.min,
		capability->aead.key_size.max,
		capability->aead.key_size.increment))
		return false;

	return true;
}

static int
iavf_ipsec_crypto_session_validate_conf(struct iavf_security_ctx *iavf_sctx,
	struct rte_security_session_conf *conf)
{
	/** validate security action/protocol selection */
	if (conf->action_type != RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO ||
		conf->protocol != RTE_SECURITY_PROTOCOL_IPSEC) {
		PMD_DRV_LOG(ERR, "Invalid action / protocol specified");
		return -EINVAL;
	}

	/** validate IPsec protocol selection */
	if (conf->ipsec.proto != RTE_SECURITY_IPSEC_SA_PROTO_ESP) {
		PMD_DRV_LOG(ERR, "Invalid IPsec protocol specified");
		return -EINVAL;
	}

	/** validate selected options */
	if (conf->ipsec.options.copy_dscp ||
		conf->ipsec.options.copy_flabel ||
		conf->ipsec.options.copy_df ||
		conf->ipsec.options.dec_ttl ||
		conf->ipsec.options.ecn ||
		conf->ipsec.options.stats) {
		PMD_DRV_LOG(ERR, "Invalid IPsec option specified");
		return -EINVAL;
	}

	/**
	 * Validate crypto xforms parameters.
	 *
	 * AEAD transforms can be used for either inbound/outbound IPsec SAs,
	 * for non-AEAD crypto transforms we explicitly only support CIPHER/AUTH
	 * for outbound and AUTH/CIPHER chained transforms for inbound IPsec.
	 */
	if (conf->crypto_xform->type == RTE_CRYPTO_SYM_XFORM_AEAD) {
		if (!valid_aead_xform(iavf_sctx, &conf->crypto_xform->aead)) {
			PMD_DRV_LOG(ERR, "Invalid IPsec option specified");
			return -EINVAL;
		}
	} else if (conf->ipsec.direction == RTE_SECURITY_IPSEC_SA_DIR_EGRESS &&
		conf->crypto_xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER &&
		conf->crypto_xform->next &&
		conf->crypto_xform->next->type == RTE_CRYPTO_SYM_XFORM_AUTH) {
		if (!valid_cipher_xform(iavf_sctx,
				&conf->crypto_xform->cipher)) {
			PMD_DRV_LOG(ERR, "Invalid IPsec option specified");
			return -EINVAL;
		}

		if (!valid_auth_xform(iavf_sctx,
				&conf->crypto_xform->next->auth)) {
			PMD_DRV_LOG(ERR, "Invalid IPsec option specified");
			return -EINVAL;
		}
	} else if (conf->ipsec.direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS &&
		conf->crypto_xform->type == RTE_CRYPTO_SYM_XFORM_AUTH &&
		conf->crypto_xform->next &&
		conf->crypto_xform->next->type == RTE_CRYPTO_SYM_XFORM_CIPHER) {
		if (!valid_auth_xform(iavf_sctx, &conf->crypto_xform->auth)) {
			PMD_DRV_LOG(ERR, "Invalid IPsec option specified");
			return -EINVAL;
		}

		if (!valid_cipher_xform(iavf_sctx,
				&conf->crypto_xform->next->cipher)) {
			PMD_DRV_LOG(ERR, "Invalid IPsec option specified");
			return -EINVAL;
		}
	}

	return 0;
}

static void
sa_add_set_aead_params(struct virtchnl_ipsec_crypto_cfg_item *cfg,
	struct rte_crypto_aead_xform *aead, uint32_t salt)
{
	cfg->crypto_type = VIRTCHNL_AEAD;

	switch (aead->algo) {
	case RTE_CRYPTO_AEAD_AES_CCM:
		cfg->algo_type = VIRTCHNL_AES_CCM; break;
	case RTE_CRYPTO_AEAD_AES_GCM:
		cfg->algo_type = VIRTCHNL_AES_GCM; break;
	case RTE_CRYPTO_AEAD_CHACHA20_POLY1305:
		cfg->algo_type = VIRTCHNL_CHACHA20_POLY1305; break;
	default:
		PMD_DRV_LOG(ERR, "Invalid AEAD parameters");
		break;
	}

	cfg->key_len = aead->key.length;
	cfg->iv_len = sizeof(uint64_t); /* iv.length includes salt len */
	cfg->digest_len = aead->digest_length;
	cfg->salt = salt;

	memcpy(cfg->key_data, aead->key.data, cfg->key_len);
}

static void
sa_add_set_cipher_params(struct virtchnl_ipsec_crypto_cfg_item *cfg,
	struct rte_crypto_cipher_xform *cipher, uint32_t salt)
{
	cfg->crypto_type = VIRTCHNL_CIPHER;

	switch (cipher->algo) {
	case RTE_CRYPTO_CIPHER_AES_CBC:
		cfg->algo_type = VIRTCHNL_AES_CBC; break;
	case RTE_CRYPTO_CIPHER_3DES_CBC:
		cfg->algo_type = VIRTCHNL_3DES_CBC; break;
	case RTE_CRYPTO_CIPHER_NULL:
		cfg->algo_type = VIRTCHNL_CIPHER_NO_ALG; break;
	case RTE_CRYPTO_CIPHER_AES_CTR:
		cfg->algo_type = VIRTCHNL_AES_CTR;
		cfg->salt = salt;
		break;
	default:
		PMD_DRV_LOG(ERR, "Invalid cipher parameters");
		break;
	}

	cfg->key_len = cipher->key.length;
	cfg->iv_len = cipher->iv.length;
	cfg->salt = salt;

	memcpy(cfg->key_data, cipher->key.data, cfg->key_len);
}

static void
sa_add_set_auth_params(struct virtchnl_ipsec_crypto_cfg_item *cfg,
	struct rte_crypto_auth_xform *auth, uint32_t salt)
{
	cfg->crypto_type = VIRTCHNL_AUTH;

	switch (auth->algo) {
	case RTE_CRYPTO_AUTH_NULL:
		cfg->algo_type = VIRTCHNL_HASH_NO_ALG; break;
	case RTE_CRYPTO_AUTH_AES_CBC_MAC:
		cfg->algo_type = VIRTCHNL_AES_CBC_MAC; break;
	case RTE_CRYPTO_AUTH_AES_CMAC:
		cfg->algo_type = VIRTCHNL_AES_CMAC; break;
	case RTE_CRYPTO_AUTH_AES_XCBC_MAC:
		cfg->algo_type = VIRTCHNL_AES_XCBC_MAC; break;
	case RTE_CRYPTO_AUTH_MD5_HMAC:
		cfg->algo_type = VIRTCHNL_MD5_HMAC; break;
	case RTE_CRYPTO_AUTH_SHA1_HMAC:
		cfg->algo_type = VIRTCHNL_SHA1_HMAC; break;
	case RTE_CRYPTO_AUTH_SHA224_HMAC:
		cfg->algo_type = VIRTCHNL_SHA224_HMAC; break;
	case RTE_CRYPTO_AUTH_SHA256_HMAC:
		cfg->algo_type = VIRTCHNL_SHA256_HMAC; break;
	case RTE_CRYPTO_AUTH_SHA384_HMAC:
		cfg->algo_type = VIRTCHNL_SHA384_HMAC; break;
	case RTE_CRYPTO_AUTH_SHA512_HMAC:
		cfg->algo_type = VIRTCHNL_SHA512_HMAC; break;
	case RTE_CRYPTO_AUTH_AES_GMAC:
		cfg->algo_type = VIRTCHNL_AES_GMAC;
		cfg->salt = salt;
		break;
	default:
		PMD_DRV_LOG(ERR, "Invalid auth parameters");
		break;
	}

	cfg->key_len = auth->key.length;
	/* special case for RTE_CRYPTO_AUTH_AES_GMAC */
	if (auth->algo == RTE_CRYPTO_AUTH_AES_GMAC)
		cfg->iv_len = sizeof(uint64_t); /* iv.length includes salt */
	else
		cfg->iv_len = auth->iv.length;
	cfg->digest_len = auth->digest_length;

	memcpy(cfg->key_data, auth->key.data, cfg->key_len);
}

/**
 * Send SA add virtual channel request to Inline IPsec driver.
 *
 * Inline IPsec driver expects SPI and destination IP address to be in host
 * order, but DPDK APIs are network order, therefore we need to do a htonl
 * conversion of these parameters.
 */
static uint32_t
iavf_ipsec_crypto_security_association_add(struct iavf_adapter *adapter,
	struct rte_security_session_conf *conf)
{
	struct inline_ipsec_msg *request = NULL, *response = NULL;
	struct virtchnl_ipsec_sa_cfg *sa_cfg;
	size_t request_len, response_len;

	int rc;

	request_len = sizeof(struct inline_ipsec_msg) +
			sizeof(struct virtchnl_ipsec_sa_cfg);

	request = rte_malloc("iavf-sad-add-request", request_len, 0);
	if (request == NULL) {
		rc = -ENOMEM;
		goto update_cleanup;
	}

	response_len = sizeof(struct inline_ipsec_msg) +
			sizeof(struct virtchnl_ipsec_sa_cfg_resp);
	response = rte_malloc("iavf-sad-add-response", response_len, 0);
	if (response == NULL) {
		rc = -ENOMEM;
		goto update_cleanup;
	}

	/* set msg header params */
	request->ipsec_opcode = INLINE_IPSEC_OP_SA_CREATE;
	request->req_id = (uint16_t)0xDEADBEEF;

	/* set SA configuration params */
	sa_cfg = (struct virtchnl_ipsec_sa_cfg *)(request + 1);

	sa_cfg->spi = conf->ipsec.spi;
	sa_cfg->virtchnl_protocol_type = VIRTCHNL_PROTO_ESP;
	sa_cfg->virtchnl_direction =
		conf->ipsec.direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS ?
			VIRTCHNL_DIR_INGRESS : VIRTCHNL_DIR_EGRESS;

	if (conf->ipsec.options.esn) {
		sa_cfg->esn_enabled = 1;
		sa_cfg->esn_hi = conf->ipsec.esn.hi;
		sa_cfg->esn_low = conf->ipsec.esn.low;
	}

	if (conf->ipsec.options.udp_encap)
		sa_cfg->udp_encap_enabled = 1;

	/* Set outer IP params */
	if (conf->ipsec.tunnel.type == RTE_SECURITY_IPSEC_TUNNEL_IPV4) {
		sa_cfg->virtchnl_ip_type = VIRTCHNL_IPV4;

		*((uint32_t *)sa_cfg->dst_addr)	=
			htonl(conf->ipsec.tunnel.ipv4.dst_ip.s_addr);
	} else {
		uint32_t *v6_dst_addr =
			(uint32_t *)conf->ipsec.tunnel.ipv6.dst_addr.s6_addr;

		sa_cfg->virtchnl_ip_type = VIRTCHNL_IPV6;

		((uint32_t *)sa_cfg->dst_addr)[0] = htonl(v6_dst_addr[0]);
		((uint32_t *)sa_cfg->dst_addr)[1] = htonl(v6_dst_addr[1]);
		((uint32_t *)sa_cfg->dst_addr)[2] = htonl(v6_dst_addr[2]);
		((uint32_t *)sa_cfg->dst_addr)[3] = htonl(v6_dst_addr[3]);
	}

	/* set crypto params */
	if (conf->crypto_xform->type == RTE_CRYPTO_SYM_XFORM_AEAD) {
		sa_add_set_aead_params(&sa_cfg->crypto_cfg.items[0],
			&conf->crypto_xform->aead, conf->ipsec.salt);

	} else if (conf->crypto_xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER) {
		sa_add_set_cipher_params(&sa_cfg->crypto_cfg.items[0],
			&conf->crypto_xform->cipher, conf->ipsec.salt);
		sa_add_set_auth_params(&sa_cfg->crypto_cfg.items[1],
			&conf->crypto_xform->next->auth, conf->ipsec.salt);

	} else if (conf->crypto_xform->type == RTE_CRYPTO_SYM_XFORM_AUTH) {
		sa_add_set_auth_params(&sa_cfg->crypto_cfg.items[0],
			&conf->crypto_xform->auth, conf->ipsec.salt);
		if (conf->crypto_xform->auth.algo != RTE_CRYPTO_AUTH_AES_GMAC)
			sa_add_set_cipher_params(&sa_cfg->crypto_cfg.items[1],
			&conf->crypto_xform->next->cipher, conf->ipsec.salt);
	}

	/* send virtual channel request to add SA to hardware database */
	rc = iavf_ipsec_crypto_request(adapter,
			(uint8_t *)request, request_len,
			(uint8_t *)response, response_len);
	if (rc)
		goto update_cleanup;

	/* verify response id */
	if (response->ipsec_opcode != request->ipsec_opcode ||
		response->req_id != request->req_id)
		rc = -EFAULT;
	else
		rc = response->ipsec_data.sa_cfg_resp->sa_handle;
update_cleanup:
	rte_free(response);
	rte_free(request);

	return rc;
}

static void
set_pkt_metadata_template(struct iavf_ipsec_crypto_pkt_metadata *template,
	struct iavf_security_session *sess)
{
	template->sa_idx = sess->sa.hw_idx;

	if (sess->udp_encap.enabled)
		template->ol_flags = IAVF_IPSEC_CRYPTO_OL_FLAGS_NATT;

	if (sess->esn.enabled)
		template->ol_flags = IAVF_IPSEC_CRYPTO_OL_FLAGS_ESN;

	template->len_iv = calc_ipsec_desc_iv_len_field(sess->iv_sz);
	template->ctx_desc_ipsec_params =
			calc_context_desc_cipherblock_sz(sess->block_sz) |
			((uint8_t)(sess->icv_sz >> 2) << 3);
}

static void
set_session_parameter(struct iavf_security_ctx *iavf_sctx,
	struct iavf_security_session *sess,
	struct rte_security_session_conf *conf, uint32_t sa_idx)
{
	sess->adapter = iavf_sctx->adapter;

	sess->mode = conf->ipsec.mode;
	sess->direction = conf->ipsec.direction;

	if (sess->mode == RTE_SECURITY_IPSEC_SA_MODE_TUNNEL)
		sess->type = conf->ipsec.tunnel.type;

	sess->sa.spi = conf->ipsec.spi;
	sess->sa.hw_idx = sa_idx;

	if (conf->ipsec.options.esn) {
		sess->esn.enabled = 1;
		sess->esn.value = conf->ipsec.esn.value;
	}

	if (conf->ipsec.options.udp_encap)
		sess->udp_encap.enabled = 1;

	if (conf->crypto_xform->type == RTE_CRYPTO_SYM_XFORM_AEAD) {
		sess->block_sz = get_aead_blocksize(iavf_sctx,
			conf->crypto_xform->aead.algo);
		sess->iv_sz = sizeof(uint64_t); /* iv.length includes salt */
		sess->icv_sz = conf->crypto_xform->aead.digest_length;
	} else if (conf->crypto_xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER) {
		sess->block_sz = get_cipher_blocksize(iavf_sctx,
			conf->crypto_xform->cipher.algo);
		sess->iv_sz = conf->crypto_xform->cipher.iv.length;
		sess->icv_sz = conf->crypto_xform->next->auth.digest_length;
	} else if (conf->crypto_xform->type == RTE_CRYPTO_SYM_XFORM_AUTH) {
		if (conf->crypto_xform->auth.algo == RTE_CRYPTO_AUTH_AES_GMAC) {
			sess->block_sz = get_auth_blocksize(iavf_sctx,
				conf->crypto_xform->auth.algo);
			sess->iv_sz = sizeof(uint64_t); /* iv len inc. salt */
			sess->icv_sz = conf->crypto_xform->auth.digest_length;
		} else {
			sess->block_sz = get_cipher_blocksize(iavf_sctx,
				conf->crypto_xform->next->cipher.algo);
			sess->iv_sz =
				conf->crypto_xform->next->cipher.iv.length;
			sess->icv_sz = conf->crypto_xform->auth.digest_length;
		}
	}

	set_pkt_metadata_template(&sess->pkt_metadata_template, sess);
}

/**
 * Create IPsec Security Association for inline IPsec Crypto offload.
 *
 * 1. validate session configuration parameters
 * 2. allocate session memory from mempool
 * 3. add SA to hardware database
 * 4. set session parameters
 * 5. create packet metadata template for datapath
 */
static int
iavf_ipsec_crypto_session_create(void *device,
				 struct rte_security_session_conf *conf,
				 struct rte_security_session *session)
{
	struct rte_eth_dev *ethdev = device;
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(ethdev->data->dev_private);
	struct iavf_security_ctx *iavf_sctx = adapter->security_ctx;
	struct iavf_security_session *iavf_session = SECURITY_GET_SESS_PRIV(session);
	int sa_idx;
	int ret = 0;

	/* validate that all SA parameters are valid for device */
	ret = iavf_ipsec_crypto_session_validate_conf(iavf_sctx, conf);
	if (ret)
		return ret;

	/* add SA to hardware database */
	sa_idx = iavf_ipsec_crypto_security_association_add(adapter, conf);
	if (sa_idx < 0) {
		PMD_DRV_LOG(ERR,
			"Failed to add SA (spi: %d, mode: %s, direction: %s)",
			conf->ipsec.spi,
			conf->ipsec.mode ==
				RTE_SECURITY_IPSEC_SA_MODE_TRANSPORT ?
				"transport" : "tunnel",
			conf->ipsec.direction ==
				RTE_SECURITY_IPSEC_SA_DIR_INGRESS ?
				"inbound" : "outbound");

		return -EFAULT;
	}

	/* save data plane required session parameters */
	set_session_parameter(iavf_sctx, iavf_session, conf, sa_idx);

	return 0;
}

/**
 * Check if valid ipsec crypto action.
 * SPI must be non-zero and SPI in session must match SPI value
 * passed into function.
 *
 * returns: 0 if invalid session or SPI value equal zero
 * returns: 1 if valid
 */
uint32_t
iavf_ipsec_crypto_action_valid(struct rte_eth_dev *ethdev,
	const struct rte_security_session *session, uint32_t spi)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(ethdev->data->dev_private);
	const struct iavf_security_session *sess = (const void *)session->driver_priv_data;

	/* verify we have a valid session and that it belong to this adapter */
	if (unlikely(sess == NULL || sess->adapter != adapter))
		return false;

	/* SPI value must be non-zero and must match flow SPI*/
	if (spi == 0 || (htonl(sess->sa.spi) != spi))
		return false;

	return true;
}

/**
 * Send virtual channel security policy add request to IES driver.
 *
 * IES driver expects SPI and destination IP address to be in host
 * order, but DPDK APIs are network order, therefore we need to do a htonl
 * conversion of these parameters.
 */
int
iavf_ipsec_crypto_inbound_security_policy_add(struct iavf_adapter *adapter,
	uint32_t esp_spi,
	uint8_t is_v4,
	rte_be32_t v4_dst_addr,
	uint8_t *v6_dst_addr,
	uint8_t drop,
	bool is_udp,
	uint16_t udp_port)
{
	struct inline_ipsec_msg *request = NULL, *response = NULL;
	size_t request_len, response_len;
	int rc = 0;

	request_len = sizeof(struct inline_ipsec_msg) +
			sizeof(struct virtchnl_ipsec_sp_cfg);
	request = rte_malloc("iavf-inbound-security-policy-add-request",
				request_len, 0);
	if (request == NULL) {
		rc = -ENOMEM;
		goto update_cleanup;
	}

	/* set msg header params */
	request->ipsec_opcode = INLINE_IPSEC_OP_SP_CREATE;
	request->req_id = (uint16_t)0xDEADBEEF;

	/* ESP SPI */
	request->ipsec_data.sp_cfg->spi = htonl(esp_spi);

	/* Destination IP  */
	if (is_v4) {
		request->ipsec_data.sp_cfg->table_id =
				VIRTCHNL_IPSEC_INBOUND_SPD_TBL_IPV4;
		request->ipsec_data.sp_cfg->dip[0] = htonl(v4_dst_addr);
	} else {
		request->ipsec_data.sp_cfg->table_id =
				VIRTCHNL_IPSEC_INBOUND_SPD_TBL_IPV6;
		request->ipsec_data.sp_cfg->dip[0] =
				htonl(((uint32_t *)v6_dst_addr)[0]);
		request->ipsec_data.sp_cfg->dip[1] =
				htonl(((uint32_t *)v6_dst_addr)[1]);
		request->ipsec_data.sp_cfg->dip[2] =
				htonl(((uint32_t *)v6_dst_addr)[2]);
		request->ipsec_data.sp_cfg->dip[3] =
				htonl(((uint32_t *)v6_dst_addr)[3]);
	}

	request->ipsec_data.sp_cfg->drop = drop;

	/** Traffic Class/Congestion Domain currently not support */
	request->ipsec_data.sp_cfg->set_tc = 0;
	request->ipsec_data.sp_cfg->cgd = 0;
	request->ipsec_data.sp_cfg->is_udp = is_udp;
	request->ipsec_data.sp_cfg->udp_port = htons(udp_port);

	response_len = sizeof(struct inline_ipsec_msg) +
			sizeof(struct virtchnl_ipsec_sp_cfg_resp);
	response = rte_malloc("iavf-inbound-security-policy-add-response",
				response_len, 0);
	if (response == NULL) {
		rc = -ENOMEM;
		goto update_cleanup;
	}

	/* send virtual channel request to add SA to hardware database */
	rc = iavf_ipsec_crypto_request(adapter,
			(uint8_t *)request, request_len,
			(uint8_t *)response, response_len);
	if (rc)
		goto update_cleanup;

	/* verify response */
	if (response->ipsec_opcode != request->ipsec_opcode ||
		response->req_id != request->req_id)
		rc = -EFAULT;
	else
		rc = response->ipsec_data.sp_cfg_resp->rule_id;

update_cleanup:
	rte_free(request);
	rte_free(response);

	return rc;
}

static uint32_t
iavf_ipsec_crypto_sa_update_esn(struct iavf_adapter *adapter,
	struct iavf_security_session *sess)
{
	struct inline_ipsec_msg *request = NULL, *response = NULL;
	size_t request_len, response_len;
	int rc = 0;

	request_len = sizeof(struct inline_ipsec_msg) +
			sizeof(struct virtchnl_ipsec_sa_update);
	request = rte_malloc("iavf-sa-update-request", request_len, 0);
	if (request == NULL) {
		rc = -ENOMEM;
		goto update_cleanup;
	}

	response_len = sizeof(struct inline_ipsec_msg) +
			sizeof(struct virtchnl_ipsec_resp);
	response = rte_malloc("iavf-sa-update-response", response_len, 0);
	if (response == NULL) {
		rc = -ENOMEM;
		goto update_cleanup;
	}

	/* set msg header params */
	request->ipsec_opcode = INLINE_IPSEC_OP_SA_UPDATE;
	request->req_id = (uint16_t)0xDEADBEEF;

	/* set request params */
	request->ipsec_data.sa_update->sa_index = sess->sa.hw_idx;
	request->ipsec_data.sa_update->esn_hi = sess->esn.hi;
	request->ipsec_data.sa_update->esn_low = sess->esn.low;

	/* send virtual channel request to add SA to hardware database */
	rc = iavf_ipsec_crypto_request(adapter,
			(uint8_t *)request, request_len,
			(uint8_t *)response, response_len);
	if (rc)
		goto update_cleanup;

	/* verify response */
	if (response->ipsec_opcode != request->ipsec_opcode ||
		response->req_id != request->req_id)
		rc = -EFAULT;
	else
		rc = response->ipsec_data.ipsec_resp->resp;

update_cleanup:
	rte_free(request);
	rte_free(response);

	return rc;
}

static int
iavf_ipsec_crypto_session_update(void *device,
		struct rte_security_session *session,
		struct rte_security_session_conf *conf)
{
	struct iavf_adapter *adapter = NULL;
	struct iavf_security_session *iavf_sess = NULL;
	struct rte_eth_dev *eth_dev = (struct rte_eth_dev *)device;
	int rc = 0;

	adapter = IAVF_DEV_PRIVATE_TO_ADAPTER(eth_dev->data->dev_private);
	iavf_sess = SECURITY_GET_SESS_PRIV(session);

	/* verify we have a valid session and that it belong to this adapter */
	if (unlikely(iavf_sess == NULL || iavf_sess->adapter != adapter))
		return -EINVAL;

	/* update esn hi 32-bits */
	if (iavf_sess->esn.enabled && conf->ipsec.options.esn) {
		/**
		 * Update ESN in hardware for inbound SA. Store in
		 * iavf_security_session for outbound SA for use
		 * in *iavf_ipsec_crypto_pkt_metadata_set* function.
		 */
		iavf_sess->esn.hi = conf->ipsec.esn.hi;
		iavf_sess->esn.low = conf->ipsec.esn.low;
		if (iavf_sess->direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS)
			rc = iavf_ipsec_crypto_sa_update_esn(adapter,
					iavf_sess);

	}

	return rc;
}

static int
iavf_ipsec_crypto_session_stats_get(void *device __rte_unused,
		struct rte_security_session *session __rte_unused,
		struct rte_security_stats *stats __rte_unused)
{
	return -EOPNOTSUPP;
}

int
iavf_ipsec_crypto_security_policy_delete(struct iavf_adapter *adapter,
	uint8_t is_v4, uint32_t flow_id)
{
	struct inline_ipsec_msg *request = NULL, *response = NULL;
	size_t request_len, response_len;
	int rc = 0;

	request_len = sizeof(struct inline_ipsec_msg) +
			sizeof(struct virtchnl_ipsec_sp_destroy);
	request = rte_malloc("iavf-sp-del-request", request_len, 0);
	if (request == NULL) {
		rc = -ENOMEM;
		goto update_cleanup;
	}

	response_len = sizeof(struct inline_ipsec_msg) +
			sizeof(struct virtchnl_ipsec_resp);
	response = rte_malloc("iavf-sp-del-response", response_len, 0);
	if (response == NULL) {
		rc = -ENOMEM;
		goto update_cleanup;
	}

	/* set msg header params */
	request->ipsec_opcode = INLINE_IPSEC_OP_SP_DESTROY;
	request->req_id = (uint16_t)0xDEADBEEF;

	/* set security policy params */
	request->ipsec_data.sp_destroy->table_id = is_v4 ?
			VIRTCHNL_IPSEC_INBOUND_SPD_TBL_IPV4 :
			VIRTCHNL_IPSEC_INBOUND_SPD_TBL_IPV6;
	request->ipsec_data.sp_destroy->rule_id = flow_id;

	/* send virtual channel request to add SA to hardware database */
	rc = iavf_ipsec_crypto_request(adapter,
			(uint8_t *)request, request_len,
			(uint8_t *)response, response_len);
	if (rc)
		goto update_cleanup;

	/* verify response */
	if (response->ipsec_opcode != request->ipsec_opcode ||
		response->req_id != request->req_id)
		rc = -EFAULT;
	else
		return response->ipsec_data.ipsec_status->status;

update_cleanup:
	rte_free(request);
	rte_free(response);

	return rc;
}

static uint32_t
iavf_ipsec_crypto_sa_del(struct iavf_adapter *adapter,
	struct iavf_security_session *sess)
{
	struct inline_ipsec_msg *request = NULL, *response = NULL;
	size_t request_len, response_len;

	int rc = 0;

	request_len = sizeof(struct inline_ipsec_msg) +
			sizeof(struct virtchnl_ipsec_sa_destroy);

	request = rte_malloc("iavf-sa-del-request", request_len, 0);
	if (request == NULL) {
		rc = -ENOMEM;
		goto update_cleanup;
	}

	response_len = sizeof(struct inline_ipsec_msg) +
			sizeof(struct virtchnl_ipsec_resp);

	response = rte_malloc("iavf-sa-del-response", response_len, 0);
	if (response == NULL) {
		rc = -ENOMEM;
		goto update_cleanup;
	}

	/* set msg header params */
	request->ipsec_opcode = INLINE_IPSEC_OP_SA_DESTROY;
	request->req_id = (uint16_t)0xDEADBEEF;

	/**
	 * SA delete supports deletion of 1-8 specified SA's or if the flag
	 * field is zero, all SA's associated with VF will be deleted.
	 */
	if (sess) {
		request->ipsec_data.sa_destroy->flag = 0x1;
		request->ipsec_data.sa_destroy->sa_index[0] = sess->sa.hw_idx;
	} else {
		request->ipsec_data.sa_destroy->flag = 0x0;
	}

	/* send virtual channel request to add SA to hardware database */
	rc = iavf_ipsec_crypto_request(adapter,
			(uint8_t *)request, request_len,
			(uint8_t *)response, response_len);
	if (rc)
		goto update_cleanup;

	/* verify response */
	if (response->ipsec_opcode != request->ipsec_opcode ||
		response->req_id != request->req_id)
		rc = -EFAULT;

	/**
	 * Delete status will be the same bitmask as sa_destroy request flag if
	 * deletes successful
	 */
	if (request->ipsec_data.sa_destroy->flag !=
			response->ipsec_data.ipsec_status->status)
		rc = -EFAULT;

update_cleanup:
	rte_free(response);
	rte_free(request);

	return rc;
}

static int
iavf_ipsec_crypto_session_destroy(void *device,
		struct rte_security_session *session)
{
	struct iavf_adapter *adapter = NULL;
	struct iavf_security_session *iavf_sess = NULL;
	struct rte_eth_dev *eth_dev = (struct rte_eth_dev *)device;
	int ret;

	adapter = IAVF_DEV_PRIVATE_TO_ADAPTER(eth_dev->data->dev_private);
	iavf_sess = SECURITY_GET_SESS_PRIV(session);

	/* verify we have a valid session and that it belong to this adapter */
	if (unlikely(iavf_sess == NULL || iavf_sess->adapter != adapter))
		return -EINVAL;

	ret = iavf_ipsec_crypto_sa_del(adapter, iavf_sess);
	memset(iavf_sess, 0, sizeof(struct iavf_security_session));
	return ret;
}

/**
 * Get ESP trailer from packet as well as calculate the total ESP trailer
 * length, which include padding, ESP trailer footer and the ICV
 */
static inline struct rte_esp_tail *
iavf_ipsec_crypto_get_esp_trailer(struct rte_mbuf *m,
	struct iavf_security_session *s, uint16_t *esp_trailer_length)
{
	struct rte_esp_tail *esp_trailer;

	uint16_t length = sizeof(struct rte_esp_tail) + s->icv_sz;
	uint16_t offset = 0;

	/**
	 * The ICV will not be present in TSO packets as this is appended by
	 * hardware during segment generation
	 */
	if (m->ol_flags & (RTE_MBUF_F_TX_TCP_SEG | RTE_MBUF_F_TX_UDP_SEG))
		length -=  s->icv_sz;

	*esp_trailer_length = length;

	/**
	 * Calculate offset in packet to ESP trailer header, this should be
	 * total packet length less the size of the ESP trailer plus the ICV
	 * length if it is present
	 */
	offset = rte_pktmbuf_pkt_len(m) - length;

	if (m->nb_segs > 1) {
		/* find segment which esp trailer is located */
		while (m->data_len < offset) {
			offset -= m->data_len;
			m = m->next;
		}
	}

	esp_trailer = rte_pktmbuf_mtod_offset(m, struct rte_esp_tail *, offset);

	*esp_trailer_length += esp_trailer->pad_len;

	return esp_trailer;
}

static inline uint16_t
iavf_ipsec_crypto_compute_l4_payload_length(struct rte_mbuf *m,
	struct iavf_security_session *s, uint16_t esp_tlen)
{
	uint16_t ol2_len = m->l2_len;	/* MAC + VLAN */
	uint16_t ol3_len = 0;		/* ipv4/6 + ext hdrs */
	uint16_t ol4_len = 0;		/* UDP NATT */
	uint16_t l3_len = 0;		/* IPv4/6 + ext hdrs */
	uint16_t l4_len = 0;		/* TCP/UDP/STCP hdrs */
	uint16_t esp_hlen = sizeof(struct rte_esp_hdr) + s->iv_sz;

	if (s->mode == RTE_SECURITY_IPSEC_SA_MODE_TUNNEL)
		ol3_len = m->outer_l3_len;
		/**<
		 * application provided l3len assumed to include length of
		 * ipv4/6 hdr + ext hdrs
		 */

	if (s->udp_encap.enabled) {
		ol4_len = sizeof(struct rte_udp_hdr);
		l3_len = m->l3_len - ol4_len;
		l4_len = l3_len;
	} else {
		l3_len = m->l3_len;
		l4_len = m->l4_len;
	}

	return rte_pktmbuf_pkt_len(m) - (ol2_len + ol3_len + ol4_len +
			esp_hlen + l3_len + l4_len + esp_tlen);
}

static int
iavf_ipsec_crypto_pkt_metadata_set(void *device,
			 struct rte_security_session *session,
			 struct rte_mbuf *m, void *params)
{
	struct rte_eth_dev *ethdev = device;
	struct iavf_adapter *adapter =
			IAVF_DEV_PRIVATE_TO_ADAPTER(ethdev->data->dev_private);
	struct iavf_security_ctx *iavf_sctx = adapter->security_ctx;
	struct iavf_security_session *iavf_sess = SECURITY_GET_SESS_PRIV(session);
	struct iavf_ipsec_crypto_pkt_metadata *md;
	struct rte_esp_tail *esp_tail;
	uint64_t *sqn = params;
	uint16_t esp_trailer_length;

	/* Check we have valid session and is associated with this device */
	if (unlikely(iavf_sess == NULL || iavf_sess->adapter != adapter))
		return -EINVAL;

	/* Get dynamic metadata location from mbuf */
	md = RTE_MBUF_DYNFIELD(m, iavf_sctx->pkt_md_offset,
		struct iavf_ipsec_crypto_pkt_metadata *);

	/* Set immutable metadata values from session template */
	memcpy(md, &iavf_sess->pkt_metadata_template,
		sizeof(struct iavf_ipsec_crypto_pkt_metadata));

	esp_tail = iavf_ipsec_crypto_get_esp_trailer(m, iavf_sess,
			&esp_trailer_length);

	/* Set per packet mutable metadata values */
	md->esp_trailer_len = esp_trailer_length;
	md->l4_payload_len = iavf_ipsec_crypto_compute_l4_payload_length(m,
				iavf_sess, esp_trailer_length);
	md->next_proto = esp_tail->next_proto;

	/* If Extended SN in use set the upper 32-bits in metadata */
	if (iavf_sess->esn.enabled && sqn != NULL)
		md->esn = (uint32_t)(*sqn >> 32);

	return 0;
}

static int
iavf_ipsec_crypto_device_capabilities_get(struct iavf_adapter *adapter,
		struct virtchnl_ipsec_cap *capability)
{
	/* Perform pf-vf comms */
	struct inline_ipsec_msg *request = NULL, *response = NULL;
	size_t request_len, response_len;
	int rc;

	request_len = sizeof(struct inline_ipsec_msg);

	request = rte_malloc("iavf-device-capability-request", request_len, 0);
	if (request == NULL) {
		rc = -ENOMEM;
		goto update_cleanup;
	}

	response_len = sizeof(struct inline_ipsec_msg) +
			sizeof(struct virtchnl_ipsec_cap);
	response = rte_malloc("iavf-device-capability-response",
			response_len, 0);
	if (response == NULL) {
		rc = -ENOMEM;
		goto update_cleanup;
	}

	/* set msg header params */
	request->ipsec_opcode = INLINE_IPSEC_OP_GET_CAP;
	request->req_id = (uint16_t)0xDEADBEEF;

	/* send virtual channel request to add SA to hardware database */
	rc = iavf_ipsec_crypto_request(adapter,
			(uint8_t *)request, request_len,
			(uint8_t *)response, response_len);
	if (rc)
		goto update_cleanup;

	/* verify response id */
	if (response->ipsec_opcode != request->ipsec_opcode ||
		response->req_id != request->req_id){
		rc = -EFAULT;
		goto update_cleanup;
	}
	memcpy(capability, response->ipsec_data.ipsec_cap, sizeof(*capability));

update_cleanup:
	rte_free(response);
	rte_free(request);

	return rc;
}

enum rte_crypto_auth_algorithm auth_maptbl[] = {
	/* Hash Algorithm */
	[VIRTCHNL_HASH_NO_ALG] = RTE_CRYPTO_AUTH_NULL,
	[VIRTCHNL_AES_CBC_MAC] = RTE_CRYPTO_AUTH_AES_CBC_MAC,
	[VIRTCHNL_AES_CMAC] = RTE_CRYPTO_AUTH_AES_CMAC,
	[VIRTCHNL_AES_GMAC] = RTE_CRYPTO_AUTH_AES_GMAC,
	[VIRTCHNL_AES_XCBC_MAC] = RTE_CRYPTO_AUTH_AES_XCBC_MAC,
	[VIRTCHNL_MD5_HMAC] = RTE_CRYPTO_AUTH_MD5_HMAC,
	[VIRTCHNL_SHA1_HMAC] = RTE_CRYPTO_AUTH_SHA1_HMAC,
	[VIRTCHNL_SHA224_HMAC] = RTE_CRYPTO_AUTH_SHA224_HMAC,
	[VIRTCHNL_SHA256_HMAC] = RTE_CRYPTO_AUTH_SHA256_HMAC,
	[VIRTCHNL_SHA384_HMAC] = RTE_CRYPTO_AUTH_SHA384_HMAC,
	[VIRTCHNL_SHA512_HMAC] = RTE_CRYPTO_AUTH_SHA512_HMAC,
	[VIRTCHNL_SHA3_224_HMAC] = RTE_CRYPTO_AUTH_SHA3_224_HMAC,
	[VIRTCHNL_SHA3_256_HMAC] = RTE_CRYPTO_AUTH_SHA3_256_HMAC,
	[VIRTCHNL_SHA3_384_HMAC] = RTE_CRYPTO_AUTH_SHA3_384_HMAC,
	[VIRTCHNL_SHA3_512_HMAC] = RTE_CRYPTO_AUTH_SHA3_512_HMAC,
};

static void
update_auth_capabilities(struct rte_cryptodev_capabilities *scap,
		struct virtchnl_algo_cap *acap,
		const struct rte_cryptodev_symmetric_capability *symcap)
{
	struct rte_cryptodev_symmetric_capability *capability = &scap->sym;

	scap->op = RTE_CRYPTO_OP_TYPE_SYMMETRIC;

	capability->xform_type = RTE_CRYPTO_SYM_XFORM_AUTH;

	capability->auth.algo = auth_maptbl[acap->algo_type];
	capability->auth.block_size = acap->block_size;

	capability->auth.key_size.min = acap->min_key_size;
	capability->auth.key_size.max = acap->max_key_size;
	capability->auth.key_size.increment = acap->inc_key_size;

	capability->auth.digest_size.min = acap->min_digest_size;
	capability->auth.digest_size.max = acap->max_digest_size;
	capability->auth.digest_size.increment = acap->inc_digest_size;

	if (symcap) {
		capability->auth.iv_size.min = symcap->auth.iv_size.min;
		capability->auth.iv_size.max = symcap->auth.iv_size.max;
		capability->auth.iv_size.increment =
				symcap->auth.iv_size.increment;
	} else {
		capability->auth.iv_size.min = 0;
		capability->auth.iv_size.max = 65535;
		capability->auth.iv_size.increment = 1;
	}
}

enum rte_crypto_cipher_algorithm cipher_maptbl[] = {
	/* Cipher Algorithm */
	[VIRTCHNL_CIPHER_NO_ALG] = RTE_CRYPTO_CIPHER_NULL,
	[VIRTCHNL_3DES_CBC] = RTE_CRYPTO_CIPHER_3DES_CBC,
	[VIRTCHNL_AES_CBC] = RTE_CRYPTO_CIPHER_AES_CBC,
	[VIRTCHNL_AES_CTR] = RTE_CRYPTO_CIPHER_AES_CTR,
};

static void
update_cipher_capabilities(struct rte_cryptodev_capabilities *scap,
	struct virtchnl_algo_cap *acap,
	const struct rte_cryptodev_symmetric_capability *symcap)
{
	struct rte_cryptodev_symmetric_capability *capability = &scap->sym;

	scap->op = RTE_CRYPTO_OP_TYPE_SYMMETRIC;

	capability->xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER;

	capability->cipher.algo = cipher_maptbl[acap->algo_type];

	capability->cipher.block_size = acap->block_size;

	capability->cipher.key_size.min = acap->min_key_size;
	capability->cipher.key_size.max = acap->max_key_size;
	capability->cipher.key_size.increment = acap->inc_key_size;

	if (symcap) {
		capability->cipher.iv_size.min = symcap->cipher.iv_size.min;
		capability->cipher.iv_size.max = symcap->cipher.iv_size.max;
		capability->cipher.iv_size.increment =
				symcap->cipher.iv_size.increment;

	} else {
		capability->cipher.iv_size.min = 0;
		capability->cipher.iv_size.max = 65535;
		capability->cipher.iv_size.increment = 1;
	}
}

enum rte_crypto_aead_algorithm aead_maptbl[] = {
	/* AEAD Algorithm */
	[VIRTCHNL_AES_CCM] = RTE_CRYPTO_AEAD_AES_CCM,
	[VIRTCHNL_AES_GCM] = RTE_CRYPTO_AEAD_AES_GCM,
	[VIRTCHNL_CHACHA20_POLY1305] = RTE_CRYPTO_AEAD_CHACHA20_POLY1305,
};

static void
update_aead_capabilities(struct rte_cryptodev_capabilities *scap,
	struct virtchnl_algo_cap *acap,
	const struct rte_cryptodev_symmetric_capability *symcap __rte_unused)
{
	struct rte_cryptodev_symmetric_capability *capability = &scap->sym;

	scap->op = RTE_CRYPTO_OP_TYPE_SYMMETRIC;

	capability->xform_type = RTE_CRYPTO_SYM_XFORM_AEAD;

	capability->aead.algo = aead_maptbl[acap->algo_type];

	capability->aead.block_size = acap->block_size;

	capability->aead.key_size.min = acap->min_key_size;
	capability->aead.key_size.max = acap->max_key_size;
	capability->aead.key_size.increment = acap->inc_key_size;

	/* remove constrains for aead and iv length */
	capability->aead.aad_size.min = 0;
	capability->aead.aad_size.max = 65535;
	capability->aead.aad_size.increment = 1;

	capability->aead.iv_size.min = 0;
	capability->aead.iv_size.max = 65535;
	capability->aead.iv_size.increment = 1;

	capability->aead.digest_size.min = acap->min_digest_size;
	capability->aead.digest_size.max = acap->max_digest_size;
	capability->aead.digest_size.increment = acap->inc_digest_size;
}

/**
 * Dynamically set crypto capabilities based on virtchannel IPsec
 * capabilities structure.
 */
int
iavf_ipsec_crypto_set_security_capabililites(struct iavf_security_ctx
		*iavf_sctx, struct virtchnl_ipsec_cap *vch_cap)
{
	struct rte_cryptodev_capabilities *capabilities;
	const struct rte_cryptodev_symmetric_capability *symcap;
	int i, j, number_of_capabilities = 0, ci = 0;

	/* Count the total number of crypto algorithms supported */
	for (i = 0; i < VIRTCHNL_IPSEC_MAX_CRYPTO_CAP_NUM; i++)
		number_of_capabilities += vch_cap->cap[i].algo_cap_num;

	/**
	 * Allocate cryptodev capabilities structure for
	 * *number_of_capabilities* items plus one item to null terminate the
	 * array
	 */
	capabilities = rte_zmalloc("crypto_cap",
		sizeof(struct rte_cryptodev_capabilities) *
		(number_of_capabilities + 1), 0);
	if (!capabilities)
		return -ENOMEM;
	capabilities[number_of_capabilities].op = RTE_CRYPTO_OP_TYPE_UNDEFINED;

	/**
	 * Iterate over each virtchnl crypto capability by crypto type and
	 * algorithm.
	 */
	for (i = 0; i < VIRTCHNL_IPSEC_MAX_CRYPTO_CAP_NUM; i++) {
		for (j = 0; j < vch_cap->cap[i].algo_cap_num; j++, ci++) {
			switch (vch_cap->cap[i].crypto_type) {
			case VIRTCHNL_AUTH:
				symcap = get_auth_capability(iavf_sctx,
					capabilities[ci].sym.auth.algo);
				update_auth_capabilities(&capabilities[ci],
					&vch_cap->cap[i].algo_cap_list[j],
					symcap);
				break;
			case VIRTCHNL_CIPHER:
				symcap = get_cipher_capability(iavf_sctx,
					capabilities[ci].sym.cipher.algo);
				update_cipher_capabilities(&capabilities[ci],
					&vch_cap->cap[i].algo_cap_list[j],
					symcap);
				break;
			case VIRTCHNL_AEAD:
				symcap = get_aead_capability(iavf_sctx,
					capabilities[ci].sym.aead.algo);
				update_aead_capabilities(&capabilities[ci],
					&vch_cap->cap[i].algo_cap_list[j],
					symcap);
				break;
			default:
				capabilities[ci].op =
						RTE_CRYPTO_OP_TYPE_UNDEFINED;
				break;
			}
		}
	}

	iavf_sctx->crypto_capabilities = capabilities;
	return 0;
}

/**
 * Get security capabilities for device
 */
static const struct rte_security_capability *
iavf_ipsec_crypto_capabilities_get(void *device)
{
	struct rte_eth_dev *eth_dev = (struct rte_eth_dev *)device;
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(eth_dev->data->dev_private);
	struct iavf_security_ctx *iavf_sctx = adapter->security_ctx;
	unsigned int i;

	static struct rte_security_capability iavf_security_capabilities[] = {
		{ /* IPsec Inline Crypto ESP Tunnel Egress */
			.action = RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO,
			.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
			.ipsec = {
				.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
				.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL,
				.direction = RTE_SECURITY_IPSEC_SA_DIR_EGRESS,
				.options = { .udp_encap = 1,
						.stats = 1, .esn = 1 },
			},
			.ol_flags = RTE_SECURITY_TX_OLOAD_NEED_MDATA
		},
		{ /* IPsec Inline Crypto ESP Tunnel Ingress */
			.action = RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO,
			.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
			.ipsec = {
				.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
				.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL,
				.direction = RTE_SECURITY_IPSEC_SA_DIR_INGRESS,
				.options = { .udp_encap = 1,
						.stats = 1, .esn = 1 },
			},
			.ol_flags = 0
		},
		{ /* IPsec Inline Crypto ESP Transport Egress */
			.action = RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO,
			.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
			.ipsec = {
				.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
				.mode = RTE_SECURITY_IPSEC_SA_MODE_TRANSPORT,
				.direction = RTE_SECURITY_IPSEC_SA_DIR_EGRESS,
				.options = { .udp_encap = 1, .stats = 1,
						.esn = 1 },
			},
			.ol_flags = RTE_SECURITY_TX_OLOAD_NEED_MDATA
		},
		{ /* IPsec Inline Crypto ESP Transport Ingress */
			.action = RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO,
			.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
			.ipsec = {
				.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
				.mode = RTE_SECURITY_IPSEC_SA_MODE_TRANSPORT,
				.direction = RTE_SECURITY_IPSEC_SA_DIR_INGRESS,
				.options = { .udp_encap = 1, .stats = 1,
						.esn = 1 }
			},
			.ol_flags = 0
		},
		{
			.action = RTE_SECURITY_ACTION_TYPE_NONE
		}
	};

	/**
	 * Update the security capabilities struct with the runtime discovered
	 * crypto capabilities, except for last element of the array which is
	 * the null termination
	 */
	for (i = 0; i < ((sizeof(iavf_security_capabilities) /
			sizeof(iavf_security_capabilities[0])) - 1); i++) {
		iavf_security_capabilities[i].crypto_capabilities =
			iavf_sctx->crypto_capabilities;
	}

	return iavf_security_capabilities;
}

static struct rte_security_ops iavf_ipsec_crypto_ops = {
	.session_get_size		= iavf_ipsec_crypto_session_size_get,
	.session_create			= iavf_ipsec_crypto_session_create,
	.session_update			= iavf_ipsec_crypto_session_update,
	.session_stats_get		= iavf_ipsec_crypto_session_stats_get,
	.session_destroy		= iavf_ipsec_crypto_session_destroy,
	.set_pkt_metadata		= iavf_ipsec_crypto_pkt_metadata_set,
	.capabilities_get		= iavf_ipsec_crypto_capabilities_get,
};

int
iavf_security_ctx_create(struct iavf_adapter *adapter)
{
	struct rte_security_ctx *sctx;

	sctx = rte_malloc("security_ctx", sizeof(struct rte_security_ctx), 0);
	if (sctx == NULL)
		return -ENOMEM;

	sctx->device = adapter->vf.eth_dev;
	sctx->ops = &iavf_ipsec_crypto_ops;
	sctx->sess_cnt = 0;

	adapter->vf.eth_dev->security_ctx = sctx;

	if (adapter->security_ctx == NULL) {
		adapter->security_ctx = rte_malloc("iavf_security_ctx",
				sizeof(struct iavf_security_ctx), 0);
		if (adapter->security_ctx == NULL) {
			rte_free(adapter->vf.eth_dev->security_ctx);
			adapter->vf.eth_dev->security_ctx = NULL;
			return -ENOMEM;
		}
	}

	return 0;
}

int
iavf_security_init(struct iavf_adapter *adapter)
{
	struct iavf_security_ctx *iavf_sctx = adapter->security_ctx;
	struct rte_mbuf_dynfield pkt_md_dynfield = {
		.name = "iavf_ipsec_crypto_pkt_metadata",
		.size = sizeof(struct iavf_ipsec_crypto_pkt_metadata),
		.align = __alignof__(struct iavf_ipsec_crypto_pkt_metadata)
	};
	struct virtchnl_ipsec_cap capabilities;
	int rc;

	iavf_sctx->adapter = adapter;

	iavf_sctx->pkt_md_offset = rte_mbuf_dynfield_register(&pkt_md_dynfield);
	if (iavf_sctx->pkt_md_offset < 0)
		return iavf_sctx->pkt_md_offset;

	/* Get device capabilities from Inline IPsec driver over PF-VF comms */
	rc = iavf_ipsec_crypto_device_capabilities_get(adapter, &capabilities);
	if (rc)
		return rc;

	return	iavf_ipsec_crypto_set_security_capabililites(iavf_sctx,
			&capabilities);
}

int
iavf_security_get_pkt_md_offset(struct iavf_adapter *adapter)
{
	struct iavf_security_ctx *iavf_sctx = adapter->security_ctx;

	return iavf_sctx->pkt_md_offset;
}

int
iavf_security_ctx_destroy(struct iavf_adapter *adapter)
{
	struct rte_security_ctx *sctx  = adapter->vf.eth_dev->security_ctx;
	struct iavf_security_ctx *iavf_sctx = adapter->security_ctx;

	if (iavf_sctx == NULL)
		return -ENODEV;

	/* free and reset security data structures */
	rte_free(iavf_sctx);
	rte_free(sctx);

	adapter->security_ctx = NULL;
	adapter->vf.eth_dev->security_ctx = NULL;

	return 0;
}

static int
iavf_ipsec_crypto_status_get(struct iavf_adapter *adapter,
		struct virtchnl_ipsec_status *status)
{
	/* Perform pf-vf comms */
	struct inline_ipsec_msg *request = NULL, *response = NULL;
	size_t request_len, response_len;
	int rc;

	request_len = sizeof(struct inline_ipsec_msg);

	request = rte_malloc("iavf-device-status-request", request_len, 0);
	if (request == NULL) {
		rc = -ENOMEM;
		goto update_cleanup;
	}

	response_len = sizeof(struct inline_ipsec_msg) +
			sizeof(struct virtchnl_ipsec_cap);
	response = rte_malloc("iavf-device-status-response",
			response_len, 0);
	if (response == NULL) {
		rc = -ENOMEM;
		goto update_cleanup;
	}

	/* set msg header params */
	request->ipsec_opcode = INLINE_IPSEC_OP_GET_STATUS;
	request->req_id = (uint16_t)0xDEADBEEF;

	/* send virtual channel request to add SA to hardware database */
	rc = iavf_ipsec_crypto_request(adapter,
			(uint8_t *)request, request_len,
			(uint8_t *)response, response_len);
	if (rc)
		goto update_cleanup;

	/* verify response id */
	if (response->ipsec_opcode != request->ipsec_opcode ||
		response->req_id != request->req_id){
		rc = -EFAULT;
		goto update_cleanup;
	}
	memcpy(status, response->ipsec_data.ipsec_status, sizeof(*status));

update_cleanup:
	rte_free(response);
	rte_free(request);

	return rc;
}


int
iavf_ipsec_crypto_supported(struct iavf_adapter *adapter)
{
	struct virtchnl_vf_resource *resources = adapter->vf.vf_res;
	int crypto_supported = false;

	/** Capability check for IPsec Crypto */
	if (resources && (resources->vf_cap_flags &
		VIRTCHNL_VF_OFFLOAD_INLINE_IPSEC_CRYPTO)) {
		struct virtchnl_ipsec_status status;
		int rc = iavf_ipsec_crypto_status_get(adapter, &status);
		if (rc == 0 && status.status == INLINE_IPSEC_STATUS_AVAILABLE)
			crypto_supported = true;
	}

	/* Clear the VF flag to return faster next call */
	if (resources && !crypto_supported)
		resources->vf_cap_flags &=
				~(VIRTCHNL_VF_OFFLOAD_INLINE_IPSEC_CRYPTO);

	return crypto_supported;
}

#define IAVF_IPSEC_INSET_ESP (\
	IAVF_INSET_ESP_SPI)

#define IAVF_IPSEC_INSET_AH (\
	IAVF_INSET_AH_SPI)

#define IAVF_IPSEC_INSET_IPV4_NATT_ESP (\
	IAVF_INSET_IPV4_SRC | IAVF_INSET_IPV4_DST | \
	IAVF_INSET_ESP_SPI)

#define IAVF_IPSEC_INSET_IPV6_NATT_ESP (\
	IAVF_INSET_IPV6_SRC | IAVF_INSET_IPV6_DST | \
	IAVF_INSET_ESP_SPI)

enum iavf_ipsec_flow_pt_type {
	IAVF_PATTERN_ESP = 1,
	IAVF_PATTERN_AH,
	IAVF_PATTERN_UDP_ESP,
};
enum iavf_ipsec_flow_pt_ip_ver {
	IAVF_PATTERN_IPV4 = 1,
	IAVF_PATTERN_IPV6,
};

#define IAVF_PATTERN(t, ipt) ((void *)((t) | ((ipt) << 4)))
#define IAVF_PATTERN_TYPE(pt) ((pt) & 0x0F)
#define IAVF_PATTERN_IP_V(pt) ((pt) >> 4)

static struct iavf_pattern_match_item iavf_ipsec_flow_pattern[] = {
	{iavf_pattern_eth_ipv4_esp,	IAVF_IPSEC_INSET_ESP,
			IAVF_PATTERN(IAVF_PATTERN_ESP, IAVF_PATTERN_IPV4)},
	{iavf_pattern_eth_ipv6_esp,	IAVF_IPSEC_INSET_ESP,
			IAVF_PATTERN(IAVF_PATTERN_ESP, IAVF_PATTERN_IPV6)},
	{iavf_pattern_eth_ipv4_ah,	IAVF_IPSEC_INSET_AH,
			IAVF_PATTERN(IAVF_PATTERN_AH, IAVF_PATTERN_IPV4)},
	{iavf_pattern_eth_ipv6_ah,	IAVF_IPSEC_INSET_AH,
			IAVF_PATTERN(IAVF_PATTERN_AH, IAVF_PATTERN_IPV6)},
	{iavf_pattern_eth_ipv4_udp_esp,	IAVF_IPSEC_INSET_IPV4_NATT_ESP,
			IAVF_PATTERN(IAVF_PATTERN_UDP_ESP, IAVF_PATTERN_IPV4)},
	{iavf_pattern_eth_ipv6_udp_esp,	IAVF_IPSEC_INSET_IPV6_NATT_ESP,
			IAVF_PATTERN(IAVF_PATTERN_UDP_ESP, IAVF_PATTERN_IPV6)},
};

struct iavf_ipsec_flow_item {
	uint64_t id;
	uint8_t is_ipv4;
	uint32_t spi;
	struct rte_ether_hdr eth_hdr;
	union {
		struct rte_ipv4_hdr ipv4_hdr;
		struct rte_ipv6_hdr ipv6_hdr;
	};
	struct rte_udp_hdr udp_hdr;
	uint8_t is_udp;
};

static void
parse_eth_item(const struct rte_flow_item_eth *item,
		struct rte_ether_hdr *eth)
{
	memcpy(eth->src_addr.addr_bytes,
			item->hdr.src_addr.addr_bytes, sizeof(eth->src_addr));
	memcpy(eth->dst_addr.addr_bytes,
			item->hdr.dst_addr.addr_bytes, sizeof(eth->dst_addr));
}

static void
parse_ipv4_item(const struct rte_flow_item_ipv4 *item,
		struct rte_ipv4_hdr *ipv4)
{
	ipv4->src_addr = item->hdr.src_addr;
	ipv4->dst_addr = item->hdr.dst_addr;
}

static void
parse_ipv6_item(const struct rte_flow_item_ipv6 *item,
		struct rte_ipv6_hdr *ipv6)
{
	memcpy(ipv6->src_addr, item->hdr.src_addr, 16);
	memcpy(ipv6->dst_addr, item->hdr.dst_addr, 16);
}

static void
parse_udp_item(const struct rte_flow_item_udp *item, struct rte_udp_hdr *udp)
{
	udp->dst_port = item->hdr.dst_port;
	udp->src_port = item->hdr.src_port;
}

static int
has_security_action(const struct rte_flow_action actions[],
	const void **session)
{
	/* only {SECURITY; END} supported */
	if (actions[0].type == RTE_FLOW_ACTION_TYPE_SECURITY &&
		actions[1].type == RTE_FLOW_ACTION_TYPE_END) {
		*session = actions[0].conf;
		return true;
	}
	return false;
}

static struct iavf_ipsec_flow_item *
iavf_ipsec_flow_item_parse(struct rte_eth_dev *ethdev,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		uint32_t type)
{
	const void *session;
	struct iavf_ipsec_flow_item
		*ipsec_flow = rte_malloc("security-flow-rule",
		sizeof(struct iavf_ipsec_flow_item), 0);
	enum iavf_ipsec_flow_pt_type p_type = IAVF_PATTERN_TYPE(type);
	enum iavf_ipsec_flow_pt_ip_ver p_ip_type = IAVF_PATTERN_IP_V(type);

	if (ipsec_flow == NULL)
		return NULL;

	ipsec_flow->is_ipv4 = (p_ip_type == IAVF_PATTERN_IPV4);

	if (pattern[0].spec)
		parse_eth_item((const struct rte_flow_item_eth *)
				pattern[0].spec, &ipsec_flow->eth_hdr);

	switch (p_type) {
	case IAVF_PATTERN_ESP:
		if (ipsec_flow->is_ipv4) {
			parse_ipv4_item((const struct rte_flow_item_ipv4 *)
					pattern[1].spec,
					&ipsec_flow->ipv4_hdr);
		} else {
			parse_ipv6_item((const struct rte_flow_item_ipv6 *)
					pattern[1].spec,
					&ipsec_flow->ipv6_hdr);
		}
		ipsec_flow->spi =
			((const struct rte_flow_item_esp *)
					pattern[2].spec)->hdr.spi;
		break;
	case IAVF_PATTERN_AH:
		if (ipsec_flow->is_ipv4) {
			parse_ipv4_item((const struct rte_flow_item_ipv4 *)
					pattern[1].spec,
					&ipsec_flow->ipv4_hdr);
		} else {
			parse_ipv6_item((const struct rte_flow_item_ipv6 *)
					pattern[1].spec,
					&ipsec_flow->ipv6_hdr);
		}
		ipsec_flow->spi =
			((const struct rte_flow_item_ah *)
					pattern[2].spec)->spi;
		break;
	case IAVF_PATTERN_UDP_ESP:
		if (ipsec_flow->is_ipv4) {
			parse_ipv4_item((const struct rte_flow_item_ipv4 *)
					pattern[1].spec,
					&ipsec_flow->ipv4_hdr);
		} else {
			parse_ipv6_item((const struct rte_flow_item_ipv6 *)
					pattern[1].spec,
					&ipsec_flow->ipv6_hdr);
		}
		parse_udp_item((const struct rte_flow_item_udp *)
				pattern[2].spec,
			&ipsec_flow->udp_hdr);
		ipsec_flow->is_udp = true;
		ipsec_flow->spi =
			((const struct rte_flow_item_esp *)
					pattern[3].spec)->hdr.spi;
		break;
	default:
		goto flow_cleanup;
	}

	if (!has_security_action(actions, &session))
		goto flow_cleanup;

	if (!iavf_ipsec_crypto_action_valid(ethdev, session,
			ipsec_flow->spi))
		goto flow_cleanup;

	return ipsec_flow;

flow_cleanup:
	rte_free(ipsec_flow);
	return NULL;
}


static struct iavf_flow_parser iavf_ipsec_flow_parser;

static int
iavf_ipsec_flow_init(struct iavf_adapter *ad)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(ad);
	struct iavf_flow_parser *parser;

	if (!vf->vf_res)
		return -EINVAL;

	if (vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_INLINE_IPSEC_CRYPTO)
		parser = &iavf_ipsec_flow_parser;
	else
		return -ENOTSUP;

	return iavf_register_parser(parser, ad);
}

static void
iavf_ipsec_flow_uninit(struct iavf_adapter *ad)
{
	iavf_unregister_parser(&iavf_ipsec_flow_parser, ad);
}

static int
iavf_ipsec_flow_create(struct iavf_adapter *ad,
		struct rte_flow *flow,
		void *meta,
		struct rte_flow_error *error)
{
	struct iavf_ipsec_flow_item *ipsec_flow = meta;
	int flow_id = -1;
	if (!ipsec_flow) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"NULL rule.");
		return -rte_errno;
	}

	if (ipsec_flow->is_ipv4) {
		flow_id = iavf_ipsec_crypto_inbound_security_policy_add(ad,
			ipsec_flow->spi,
			1,
			ipsec_flow->ipv4_hdr.dst_addr,
			NULL,
			0,
			ipsec_flow->is_udp,
			ipsec_flow->udp_hdr.dst_port);
	} else {
		flow_id = iavf_ipsec_crypto_inbound_security_policy_add(ad,
			ipsec_flow->spi,
			0,
			0,
			ipsec_flow->ipv6_hdr.dst_addr,
			0,
			ipsec_flow->is_udp,
			ipsec_flow->udp_hdr.dst_port);
	}

	if (flow_id < 1) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				"Failed to add SA.");
		return -rte_errno;
	}

	ipsec_flow->id = flow_id;
	flow->rule = ipsec_flow;

	return 0;
}

static int
iavf_ipsec_flow_destroy(struct iavf_adapter *ad,
		struct rte_flow *flow,
		struct rte_flow_error *error)
{
	struct iavf_ipsec_flow_item *ipsec_flow = flow->rule;
	if (!ipsec_flow) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"NULL rule.");
		return -rte_errno;
	}

	iavf_ipsec_crypto_security_policy_delete(ad,
			ipsec_flow->is_ipv4, ipsec_flow->id);
	rte_free(ipsec_flow);
	return 0;
}

static struct iavf_flow_engine iavf_ipsec_flow_engine = {
	.init = iavf_ipsec_flow_init,
	.uninit = iavf_ipsec_flow_uninit,
	.create = iavf_ipsec_flow_create,
	.destroy = iavf_ipsec_flow_destroy,
	.type = IAVF_FLOW_ENGINE_IPSEC_CRYPTO,
};

static int
iavf_ipsec_flow_parse(struct iavf_adapter *ad,
		      struct iavf_pattern_match_item *array,
		      uint32_t array_len,
		      const struct rte_flow_item pattern[],
		      const struct rte_flow_action actions[],
		      uint32_t priority,
		      void **meta,
		      struct rte_flow_error *error)
{
	struct iavf_pattern_match_item *item = NULL;
	int ret = -1;

	if (priority >= 1)
		return -rte_errno;

	item = iavf_search_pattern_match_item(pattern, array, array_len, error);
	if (item && item->meta) {
		uint32_t type = (uint64_t)(item->meta);
		struct iavf_ipsec_flow_item *fi =
				iavf_ipsec_flow_item_parse(ad->vf.eth_dev,
						pattern, actions, type);
		if (fi && meta) {
			*meta = fi;
			ret = 0;
		}
	}
	return ret;
}

static struct iavf_flow_parser iavf_ipsec_flow_parser = {
	.engine = &iavf_ipsec_flow_engine,
	.array = iavf_ipsec_flow_pattern,
	.array_len = RTE_DIM(iavf_ipsec_flow_pattern),
	.parse_pattern_action = iavf_ipsec_flow_parse,
	.stage = IAVF_FLOW_STAGE_IPSEC_CRYPTO,
};

RTE_INIT(iavf_ipsec_flow_engine_register)
{
	iavf_register_flow_engine(&iavf_ipsec_flow_engine);
}
