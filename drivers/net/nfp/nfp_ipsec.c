/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Corigine Systems, Inc.
 * All rights reserved.
 */

#include "nfp_ipsec.h"

#include <rte_cryptodev.h>
#include <rte_malloc.h>
#include <rte_security_driver.h>

#include <ethdev_driver.h>
#include <ethdev_pci.h>

#include "nfp_logs.h"
#include "nfp_net_common.h"
#include "nfp_net_ctrl.h"
#include "nfp_rxtx.h"

#define NFP_UDP_ESP_PORT            4500

static const struct rte_cryptodev_capabilities nfp_crypto_caps[] = {
	{
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			.auth = {
				.algo = RTE_CRYPTO_AUTH_MD5_HMAC,
				.block_size = 64,
				.key_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				},
				.digest_size = {
					.min = 12,
					.max = 16,
					.increment = 4
				},
			},
		},
	},
	{
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA1_HMAC,
				.block_size = 64,
				.key_size = {
					.min = 20,
					.max = 64,
					.increment = 1
				},
				.digest_size = {
					.min = 10,
					.max = 12,
					.increment = 2
				},
			},
		},
	},
	{
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA256_HMAC,
				.block_size = 64,
				.key_size = {
					.min = 32,
					.max = 32,
					.increment = 0
				},
				.digest_size = {
					.min = 12,
					.max = 16,
					.increment = 4
				},
			},
		},
	},
	{
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA384_HMAC,
				.block_size = 128,
				.key_size = {
					.min = 48,
					.max = 48,
					.increment = 0
				},
				.digest_size = {
					.min = 12,
					.max = 24,
					.increment = 12
				},
			},
		},
	},
	{
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA512_HMAC,
				.block_size = 128,
				.key_size = {
					.min = 64,
					.max = 64,
					.increment = 1
				},
				.digest_size = {
					.min = 12,
					.max = 32,
					.increment = 4
				},
			},
		},
	},
	{
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			.cipher = {
				.algo = RTE_CRYPTO_CIPHER_3DES_CBC,
				.block_size = 8,
				.key_size = {
					.min = 24,
					.max = 24,
					.increment = 0
				},
				.iv_size = {
					.min = 8,
					.max = 16,
					.increment = 8
				},
			},
		},
	},
	{
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			.cipher = {
				.algo = RTE_CRYPTO_CIPHER_AES_CBC,
				.block_size = 16,
				.key_size = {
					.min = 16,
					.max = 32,
					.increment = 8
				},
				.iv_size = {
					.min = 8,
					.max = 16,
					.increment = 8
				},
			},
		},
	},
	{
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AEAD,
			.aead = {
				.algo = RTE_CRYPTO_AEAD_AES_GCM,
				.block_size = 16,
				.key_size = {
					.min = 16,
					.max = 32,
					.increment = 8
				},
				.digest_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				},
				.aad_size = {
					.min = 0,
					.max = 1024,
					.increment = 1
				},
				.iv_size = {
					.min = 8,
					.max = 16,
					.increment = 4
				}
			},
		},
	},
	{
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AEAD,
			.aead = {
				.algo = RTE_CRYPTO_AEAD_CHACHA20_POLY1305,
				.block_size = 16,
				.key_size = {
					.min = 32,
					.max = 32,
					.increment = 0
				},
				.digest_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				},
				.aad_size = {
					.min = 0,
					.max = 1024,
					.increment = 1
				},
				.iv_size = {
					.min = 8,
					.max = 16,
					.increment = 4
				}
			},
		},
	},
	{
		.op = RTE_CRYPTO_OP_TYPE_UNDEFINED,
		.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_NOT_SPECIFIED
		},
	}
};

static const struct rte_security_capability nfp_security_caps[] = {
	{ /* IPsec Inline Crypto Tunnel Egress */
		.action = RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO,
		.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
		.ipsec = {
			.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL,
			.direction = RTE_SECURITY_IPSEC_SA_DIR_EGRESS,
			.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
			.options = {
				.udp_encap = 1,
				.stats = 1,
				.esn = 1
				}
		},
		.crypto_capabilities = nfp_crypto_caps,
		.ol_flags = RTE_SECURITY_TX_OLOAD_NEED_MDATA
	},
	{ /* IPsec Inline Crypto Tunnel Ingress */
		.action = RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO,
		.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
		.ipsec = {
			.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL,
			.direction = RTE_SECURITY_IPSEC_SA_DIR_INGRESS,
			.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
			.options = {
				.udp_encap = 1,
				.stats = 1,
				.esn = 1
				}
		},
		.crypto_capabilities = nfp_crypto_caps
	},
	{ /* IPsec Inline Crypto Transport Egress */
		.action = RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO,
		.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
		.ipsec = {
			.mode = RTE_SECURITY_IPSEC_SA_MODE_TRANSPORT,
			.direction = RTE_SECURITY_IPSEC_SA_DIR_EGRESS,
			.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
			.options = {
				.udp_encap = 1,
				.stats = 1,
				.esn = 1
				}
		},
		.crypto_capabilities = nfp_crypto_caps,
		.ol_flags = RTE_SECURITY_TX_OLOAD_NEED_MDATA
	},
	{ /* IPsec Inline Crypto Transport Ingress */
		.action = RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO,
		.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
		.ipsec = {
			.mode = RTE_SECURITY_IPSEC_SA_MODE_TRANSPORT,
			.direction = RTE_SECURITY_IPSEC_SA_DIR_INGRESS,
			.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
			.options = {
				.udp_encap = 1,
				.stats = 1,
				.esn = 1
				}
		},
		.crypto_capabilities = nfp_crypto_caps
	},
	{ /* IPsec Inline Protocol Tunnel Egress */
		.action = RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
		.ipsec = {
			.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL,
			.direction = RTE_SECURITY_IPSEC_SA_DIR_EGRESS,
			.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
			.options = {
				.udp_encap = 1,
				.stats = 1,
				.esn = 1
				}
		},
		.crypto_capabilities = nfp_crypto_caps,
		.ol_flags = RTE_SECURITY_TX_OLOAD_NEED_MDATA
	},
	{ /* IPsec Inline Protocol Tunnel Ingress */
		.action = RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
		.ipsec = {
			.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL,
			.direction = RTE_SECURITY_IPSEC_SA_DIR_INGRESS,
			.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
			.options = {
				.udp_encap = 1,
				.stats = 1,
				.esn = 1
				}
		},
		.crypto_capabilities = nfp_crypto_caps
	},
	{ /* IPsec Inline Protocol Transport Egress */
		.action = RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
		.ipsec = {
			.mode = RTE_SECURITY_IPSEC_SA_MODE_TRANSPORT,
			.direction = RTE_SECURITY_IPSEC_SA_DIR_EGRESS,
			.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
			.options = {
				.udp_encap = 1,
				.stats = 1,
				.esn = 1
				}
		},
		.crypto_capabilities = nfp_crypto_caps,
		.ol_flags = RTE_SECURITY_TX_OLOAD_NEED_MDATA
	},
	{ /* IPsec Inline Protocol Transport Ingress */
		.action = RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
		.ipsec = {
			.mode = RTE_SECURITY_IPSEC_SA_MODE_TRANSPORT,
			.direction = RTE_SECURITY_IPSEC_SA_DIR_INGRESS,
			.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
			.options = {
				.udp_encap = 1,
				.stats = 1,
				.esn = 1
				}
		},
		.crypto_capabilities = nfp_crypto_caps
	},
	{
		.action = RTE_SECURITY_ACTION_TYPE_NONE
	}
};

/* IPsec config message cmd codes */
enum nfp_ipsec_cfg_msg_cmd_codes {
	NFP_IPSEC_CFG_MSG_ADD_SA,       /**< Add a new SA */
	NFP_IPSEC_CFG_MSG_INV_SA,       /**< Invalidate an existing SA */
	NFP_IPSEC_CFG_MSG_MODIFY_SA,    /**< Modify an existing SA */
	NFP_IPSEC_CFG_MSG_GET_SA_STATS, /**< Report SA counters, flags, etc. */
	NFP_IPSEC_CFG_MSG_GET_SEQ_NUMS, /**< Allocate sequence numbers */
	NFP_IPSEC_CFG_MSG_LAST
};

enum nfp_ipsec_cfg_msg_rsp_codes {
	NFP_IPSEC_CFG_MSG_OK,
	NFP_IPSEC_CFG_MSG_FAILED,
	NFP_IPSEC_CFG_MSG_SA_VALID,
	NFP_IPSEC_CFG_MSG_SA_HASH_ADD_FAILED,
	NFP_IPSEC_CFG_MSG_SA_HASH_DEL_FAILED,
	NFP_IPSEC_CFG_MSG_SA_INVALID_CMD
};

enum nfp_ipsec_mode {
	NFP_IPSEC_MODE_TRANSPORT,
	NFP_IPSEC_MODE_TUNNEL,
};

enum nfp_ipsec_protocol {
	NFP_IPSEC_PROTOCOL_AH,
	NFP_IPSEC_PROTOCOL_ESP,
};

/* Cipher modes */
enum nfp_ipsec_cimode {
	NFP_IPSEC_CIMODE_ECB,
	NFP_IPSEC_CIMODE_CBC,
	NFP_IPSEC_CIMODE_CFB,
	NFP_IPSEC_CIMODE_OFB,
	NFP_IPSEC_CIMODE_CTR,
};

/* Hash types */
enum nfp_ipsec_hash_type {
	NFP_IPSEC_HASH_NONE,
	NFP_IPSEC_HASH_MD5_96,
	NFP_IPSEC_HASH_SHA1_96,
	NFP_IPSEC_HASH_SHA256_96,
	NFP_IPSEC_HASH_SHA384_96,
	NFP_IPSEC_HASH_SHA512_96,
	NFP_IPSEC_HASH_MD5_128,
	NFP_IPSEC_HASH_SHA1_80,
	NFP_IPSEC_HASH_SHA256_128,
	NFP_IPSEC_HASH_SHA384_192,
	NFP_IPSEC_HASH_SHA512_256,
	NFP_IPSEC_HASH_GF128_128,
	NFP_IPSEC_HASH_POLY1305_128,
};

/* Cipher types */
enum nfp_ipsec_cipher_type {
	NFP_IPSEC_CIPHER_NULL,
	NFP_IPSEC_CIPHER_3DES,
	NFP_IPSEC_CIPHER_AES128,
	NFP_IPSEC_CIPHER_AES192,
	NFP_IPSEC_CIPHER_AES256,
	NFP_IPSEC_CIPHER_AES128_NULL,
	NFP_IPSEC_CIPHER_AES192_NULL,
	NFP_IPSEC_CIPHER_AES256_NULL,
	NFP_IPSEC_CIPHER_CHACHA20,
};

/* Don't Fragment types */
enum nfp_ipsec_df_type {
	NFP_IPSEC_DF_CLEAR,
	NFP_IPSEC_DF_SET,
	NFP_IPSEC_DF_COPY,
};

static int
nfp_ipsec_cfg_cmd_issue(struct nfp_net_hw *net_hw,
		struct nfp_ipsec_msg *msg)
{
	int ret;
	uint32_t i;
	uint32_t msg_size;

	msg_size = RTE_DIM(msg->raw);
	msg->rsp = NFP_IPSEC_CFG_MSG_OK;

	for (i = 0; i < msg_size; i++)
		nn_cfg_writel(&net_hw->super, NFP_NET_CFG_MBOX_VAL + 4 * i, msg->raw[i]);

	ret = nfp_net_mbox_reconfig(net_hw, NFP_NET_CFG_MBOX_CMD_IPSEC);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to IPsec reconfig mbox");
		return ret;
	}

	/*
	 * Not all commands and callers make use of response message data. But
	 * leave this up to the caller and always read and store the full
	 * response. One example where the data is needed is for statistics.
	 */
	for (i = 0; i < msg_size; i++)
		msg->raw[i] = nn_cfg_readl(&net_hw->super, NFP_NET_CFG_MBOX_VAL + 4 * i);

	switch (msg->rsp) {
	case NFP_IPSEC_CFG_MSG_OK:
		ret = 0;
		break;
	case NFP_IPSEC_CFG_MSG_SA_INVALID_CMD:
		ret = -EINVAL;
		break;
	case NFP_IPSEC_CFG_MSG_SA_VALID:
		ret = -EEXIST;
		break;
	case NFP_IPSEC_CFG_MSG_FAILED:
		/* FALLTHROUGH */
	case NFP_IPSEC_CFG_MSG_SA_HASH_ADD_FAILED:
		/* FALLTHROUGH */
	case NFP_IPSEC_CFG_MSG_SA_HASH_DEL_FAILED:
		ret = -EIO;
		break;
	default:
		ret = -EDOM;
	}

	return ret;
}

/**
 * Get valid SA index from SA table
 *
 * @param data
 *   SA table pointer
 * @param sa_idx
 *   SA table index pointer
 *
 * @return
 *   Negative number on full or repeat, 0 on success
 *
 * Note: multiple sockets may create same SA session.
 */
static void
nfp_get_sa_entry(struct nfp_net_ipsec_data *data,
		int *sa_idx)
{
	uint32_t i;

	for (i = 0; i < NFP_NET_IPSEC_MAX_SA_CNT; i++) {
		if (data->sa_entries[i] == NULL) {
			*sa_idx = i;
			break;
		}
	}
}

static void
nfp_aesgcm_iv_update(struct ipsec_add_sa *cfg,
		uint16_t iv_len,
		const char *iv_string)
{
	int i;
	char *save;
	char *iv_b;
	char *iv_str;
	uint8_t *cfg_iv;

	iv_str = strdup(iv_string);
	cfg_iv = (uint8_t *)cfg->aesgcm_fields.iv;

	for (i = 0; i < iv_len; i++) {
		iv_b = strtok_r(i ? NULL : iv_str, ",", &save);
		if (iv_b == NULL)
			break;

		cfg_iv[i] = strtoul(iv_b, NULL, 0);
	}

	*(uint32_t *)cfg_iv = rte_be_to_cpu_32(*(uint32_t *)cfg_iv);
	*(uint32_t *)&cfg_iv[4] = rte_be_to_cpu_32(*(uint32_t *)&cfg_iv[4]);

	free(iv_str);
}

static int
set_aes_keylen(uint32_t key_length,
		struct ipsec_add_sa *cfg)
{
	switch (key_length << 3) {
	case 128:
		cfg->ctrl_word.cipher = NFP_IPSEC_CIPHER_AES128;
		break;
	case 192:
		cfg->ctrl_word.cipher = NFP_IPSEC_CIPHER_AES192;
		break;
	case 256:
		cfg->ctrl_word.cipher = NFP_IPSEC_CIPHER_AES256;
		break;
	default:
		PMD_DRV_LOG(ERR, "AES cipher key length is illegal!");
		return -EINVAL;
	}

	return 0;
}

/* Map rte_security_session_conf aead algo to NFP aead algo */
static int
nfp_aead_map(struct rte_eth_dev *eth_dev,
		struct rte_crypto_aead_xform *aead,
		uint32_t key_length,
		struct ipsec_add_sa *cfg)
{
	int ret;
	uint32_t i;
	uint32_t index;
	uint16_t iv_len;
	uint32_t offset;
	uint32_t device_id;
	const char *iv_str;
	const uint32_t *key;
	struct nfp_net_hw *net_hw;

	net_hw = eth_dev->data->dev_private;
	device_id = net_hw->device_id;
	offset = 0;

	switch (aead->algo) {
	case RTE_CRYPTO_AEAD_AES_GCM:
		if (aead->digest_length != 16) {
			PMD_DRV_LOG(ERR, "ICV must be 128bit with RTE_CRYPTO_AEAD_AES_GCM!");
			return -EINVAL;
		}

		cfg->ctrl_word.cimode = NFP_IPSEC_CIMODE_CTR;
		cfg->ctrl_word.hash = NFP_IPSEC_HASH_GF128_128;

		ret = set_aes_keylen(key_length, cfg);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "Failed to set AES_GCM key length!");
			return -EINVAL;
		}

		break;
	case RTE_CRYPTO_AEAD_CHACHA20_POLY1305:
		if (device_id != PCI_DEVICE_ID_NFP3800_PF_NIC) {
			PMD_DRV_LOG(ERR, "Unsupported aead CHACHA20_POLY1305 algorithm!");
			return -EINVAL;
		}

		if (aead->digest_length != 16) {
			PMD_DRV_LOG(ERR, "ICV must be 128bit with RTE_CRYPTO_AEAD_CHACHA20_POLY1305");
			return -EINVAL;
		}

		/* Aead->alg_key_len includes 32-bit salt */
		if (key_length != 32) {
			PMD_DRV_LOG(ERR, "Unsupported CHACHA20 key length");
			return -EINVAL;
		}

		/* The CHACHA20's mode is not configured */
		cfg->ctrl_word.hash = NFP_IPSEC_HASH_POLY1305_128;
		cfg->ctrl_word.cipher = NFP_IPSEC_CIPHER_CHACHA20;
		break;
	default:
		PMD_DRV_LOG(ERR, "Unsupported aead algorithm!");
		return -EINVAL;
	}

	key = (const uint32_t *)(aead->key.data);

	/*
	 * The CHACHA20's key order needs to be adjusted based on hardware design.
	 * Unadjusted order: {K0, K1, K2, K3, K4, K5, K6, K7}
	 * Adjusted order: {K4, K5, K6, K7, K0, K1, K2, K3}
	 */
	if (aead->algo == RTE_CRYPTO_AEAD_CHACHA20_POLY1305)
		offset = key_length / sizeof(cfg->cipher_key[0]) << 1;

	for (i = 0; i < key_length / sizeof(cfg->cipher_key[0]); i++) {
		index = (i + offset) % (key_length / sizeof(cfg->cipher_key[0]));
		cfg->cipher_key[index] = rte_cpu_to_be_32(*key++);
	}

	/*
	 * The iv of the FW is equal to ESN by default. Reading the
	 * iv of the configuration information is not supported.
	 */
	iv_str = getenv("ETH_SEC_IV_OVR");
	if (iv_str != NULL) {
		iv_len = aead->iv.length;
		nfp_aesgcm_iv_update(cfg, iv_len, iv_str);
	}

	return 0;
}

/* Map rte_security_session_conf cipher algo to NFP cipher algo */
static int
nfp_cipher_map(struct rte_eth_dev *eth_dev,
		struct rte_crypto_cipher_xform *cipher,
		uint32_t key_length,
		struct ipsec_add_sa *cfg)
{
	int ret;
	uint32_t i;
	uint32_t device_id;
	const uint32_t *key;
	struct nfp_net_hw *net_hw;

	net_hw = eth_dev->data->dev_private;
	device_id = net_hw->device_id;

	switch (cipher->algo) {
	case RTE_CRYPTO_CIPHER_NULL:
		cfg->ctrl_word.cimode = NFP_IPSEC_CIMODE_CBC;
		cfg->ctrl_word.cipher = NFP_IPSEC_CIPHER_NULL;
		break;
	case RTE_CRYPTO_CIPHER_3DES_CBC:
		if (device_id == PCI_DEVICE_ID_NFP3800_PF_NIC) {
			PMD_DRV_LOG(ERR, "Unsupported 3DESCBC encryption algorithm!");
			return -EINVAL;
		}

		cfg->ctrl_word.cimode = NFP_IPSEC_CIMODE_CBC;
		cfg->ctrl_word.cipher = NFP_IPSEC_CIPHER_3DES;
		break;
	case RTE_CRYPTO_CIPHER_AES_CBC:
		cfg->ctrl_word.cimode = NFP_IPSEC_CIMODE_CBC;
		ret = set_aes_keylen(key_length, cfg);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "Failed to set cipher key length!");
			return -EINVAL;
		}

		break;
	default:
		PMD_DRV_LOG(ERR, "Unsupported cipher alg!");
		return -EINVAL;
	}

	key = (const uint32_t  *)(cipher->key.data);
	if (key_length > sizeof(cfg->cipher_key)) {
		PMD_DRV_LOG(ERR, "Insufficient space for offloaded key");
		return -EINVAL;
	}

	for (i = 0; i < key_length / sizeof(cfg->cipher_key[0]); i++)
		cfg->cipher_key[i] = rte_cpu_to_be_32(*key++);

	return 0;
}

static void
set_md5hmac(struct ipsec_add_sa *cfg,
		uint32_t *digest_length)
{
	switch (*digest_length) {
	case 96:
		cfg->ctrl_word.hash = NFP_IPSEC_HASH_MD5_96;
		break;
	case 128:
		cfg->ctrl_word.hash = NFP_IPSEC_HASH_MD5_128;
		break;
	default:
		*digest_length = 0;
	}
}

static void
set_sha1hmac(struct ipsec_add_sa *cfg,
		uint32_t *digest_length)
{
	switch (*digest_length) {
	case 96:
		cfg->ctrl_word.hash = NFP_IPSEC_HASH_SHA1_96;
		break;
	case 80:
		cfg->ctrl_word.hash = NFP_IPSEC_HASH_SHA1_80;
		break;
	default:
		*digest_length = 0;
	}
}

static void
set_sha2_256hmac(struct ipsec_add_sa *cfg,
		uint32_t *digest_length)
{
	switch (*digest_length) {
	case 96:
		cfg->ctrl_word.hash = NFP_IPSEC_HASH_SHA256_96;
		break;
	case 128:
		cfg->ctrl_word.hash = NFP_IPSEC_HASH_SHA256_128;
		break;
	default:
		*digest_length = 0;
	}
}

static void
set_sha2_384hmac(struct ipsec_add_sa *cfg,
		uint32_t *digest_length)
{
	switch (*digest_length) {
	case 96:
		cfg->ctrl_word.hash = NFP_IPSEC_HASH_SHA384_96;
		break;
	case 192:
		cfg->ctrl_word.hash = NFP_IPSEC_HASH_SHA384_192;
		break;
	default:
		*digest_length = 0;
	}
}

static void
set_sha2_512hmac(struct ipsec_add_sa *cfg,
		uint32_t *digest_length)
{
	switch (*digest_length) {
	case 96:
		cfg->ctrl_word.hash = NFP_IPSEC_HASH_SHA512_96;
		break;
	case 256:
		cfg->ctrl_word.hash = NFP_IPSEC_HASH_SHA512_256;
		break;
	default:
		*digest_length = 0;
	}
}

/* Map rte_security_session_conf auth algo to NFP auth algo */
static int
nfp_auth_map(struct rte_eth_dev *eth_dev,
		struct rte_crypto_auth_xform *auth,
		uint32_t digest_length,
		struct ipsec_add_sa *cfg)
{
	uint32_t i;
	uint8_t key_length;
	uint32_t device_id;
	const uint32_t *key;
	struct nfp_net_hw *net_hw;

	if (digest_length == 0) {
		PMD_DRV_LOG(ERR, "Auth digest length is illegal!");
		return -EINVAL;
	}

	net_hw = eth_dev->data->dev_private;
	device_id = net_hw->device_id;
	digest_length = digest_length << 3;

	switch (auth->algo) {
	case RTE_CRYPTO_AUTH_NULL:
		cfg->ctrl_word.hash = NFP_IPSEC_HASH_NONE;
		digest_length = 1;
		break;
	case RTE_CRYPTO_AUTH_MD5_HMAC:
		if (device_id == PCI_DEVICE_ID_NFP3800_PF_NIC) {
			PMD_DRV_LOG(ERR, "Unsupported MD5HMAC authentication algorithm!");
			return -EINVAL;
		}

		set_md5hmac(cfg, &digest_length);
		break;
	case RTE_CRYPTO_AUTH_SHA1_HMAC:
		set_sha1hmac(cfg, &digest_length);
		break;
	case RTE_CRYPTO_AUTH_SHA256_HMAC:
		set_sha2_256hmac(cfg, &digest_length);
		break;
	case RTE_CRYPTO_AUTH_SHA384_HMAC:
		set_sha2_384hmac(cfg, &digest_length);
		break;
	case RTE_CRYPTO_AUTH_SHA512_HMAC:
		set_sha2_512hmac(cfg, &digest_length);
		break;
	default:
		PMD_DRV_LOG(ERR, "Unsupported auth alg!");
		return -EINVAL;
	}

	if (digest_length == 0) {
		PMD_DRV_LOG(ERR, "Unsupported authentication algorithm digest length");
		return -EINVAL;
	}

	key = (const uint32_t *)(auth->key.data);
	key_length = auth->key.length;
	if (key_length > sizeof(cfg->auth_key)) {
		PMD_DRV_LOG(ERR, "Insufficient space for offloaded auth key!");
		return -EINVAL;
	}

	for (i = 0; i < key_length / sizeof(cfg->auth_key[0]); i++)
		cfg->auth_key[i] = rte_cpu_to_be_32(*key++);

	return 0;
}

static int
nfp_crypto_msg_build(struct rte_eth_dev *eth_dev,
		struct rte_security_session_conf *conf,
		struct nfp_ipsec_msg *msg)
{
	int ret;
	struct ipsec_add_sa *cfg;
	struct rte_crypto_sym_xform *cur;
	struct rte_crypto_sym_xform *next;
	enum rte_security_ipsec_sa_direction direction;

	cur = conf->crypto_xform;
	if (cur == NULL) {
		PMD_DRV_LOG(ERR, "Unsupported crypto_xform is NULL!");
		return -EINVAL;
	}

	next = cur->next;
	direction = conf->ipsec.direction;
	cfg = &msg->cfg_add_sa;

	switch (cur->type) {
	case RTE_CRYPTO_SYM_XFORM_AEAD:
		/* Aead transforms can be used for either inbound/outbound IPsec SAs */
		if (next != NULL) {
			PMD_DRV_LOG(ERR, "Next crypto_xform type should be NULL!");
			return -EINVAL;
		}

		ret = nfp_aead_map(eth_dev, &cur->aead, cur->aead.key.length, cfg);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "Failed to map aead alg!");
			return ret;
		}

		cfg->aesgcm_fields.salt = rte_cpu_to_be_32(conf->ipsec.salt);
		break;
	case RTE_CRYPTO_SYM_XFORM_AUTH:
		/* Only support Auth + Cipher for inbound */
		if (direction != RTE_SECURITY_IPSEC_SA_DIR_INGRESS) {
			PMD_DRV_LOG(ERR, "Direction should be INGRESS, but it is not!");
			return -EINVAL;
		}

		if (next == NULL || next->type != RTE_CRYPTO_SYM_XFORM_CIPHER) {
			PMD_DRV_LOG(ERR, "Next crypto_xfrm should be cipher, but it is not!");
			return -EINVAL;
		}

		ret = nfp_auth_map(eth_dev, &cur->auth, cur->auth.digest_length, cfg);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "Failed to map auth alg!");
			return ret;
		}

		ret = nfp_cipher_map(eth_dev, &next->cipher, next->cipher.key.length, cfg);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "Failed to map cipher alg!");
			return ret;
		}

		break;
	case RTE_CRYPTO_SYM_XFORM_CIPHER:
		/* Only support Cipher + Auth for outbound */
		if (direction != RTE_SECURITY_IPSEC_SA_DIR_EGRESS) {
			PMD_DRV_LOG(ERR, "Direction should be EGRESS, but it is not!");
			return -EINVAL;
		}

		if (next == NULL || next->type != RTE_CRYPTO_SYM_XFORM_AUTH) {
			PMD_DRV_LOG(ERR, "Next crypto_xfrm should be auth, but it is not!");
			return -EINVAL;
		}

		ret = nfp_cipher_map(eth_dev, &cur->cipher, cur->cipher.key.length, cfg);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "Failed to map cipher alg!");
			return ret;
		}

		ret = nfp_auth_map(eth_dev, &next->auth, next->auth.digest_length, cfg);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "Failed to map auth alg!");
			return ret;
		}

		break;
	default:
		PMD_DRV_LOG(ERR, "Unsupported crypto_xform type!");
		return -EINVAL;
	}

	return 0;
}

static int
nfp_ipsec_msg_build(struct rte_eth_dev *eth_dev,
		struct rte_security_session_conf *conf,
		struct nfp_ipsec_msg *msg)
{
	int ret;
	struct ipsec_add_sa *cfg;
	enum rte_security_ipsec_tunnel_type type;

	cfg = &msg->cfg_add_sa;
	cfg->spi = conf->ipsec.spi;
	cfg->pmtu_limit = 0xffff;

	/*
	 * UDP encapsulation
	 *
	 * 1: Do UDP encapsulation/decapsulation
	 * 0: No UDP encapsulation
	 */
	if (conf->ipsec.options.udp_encap == 1) {
		cfg->udp_enable = 1;
		cfg->natt_dst_port = NFP_UDP_ESP_PORT;
		cfg->natt_src_port = NFP_UDP_ESP_PORT;
	}

	if (conf->ipsec.options.copy_df == 1)
		cfg->df_ctrl = NFP_IPSEC_DF_COPY;
	else if (conf->ipsec.tunnel.ipv4.df != 0)
		cfg->df_ctrl = NFP_IPSEC_DF_SET;
	else
		cfg->df_ctrl = NFP_IPSEC_DF_CLEAR;

	switch (conf->action_type) {
	case RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO:
		cfg->ctrl_word.encap_dsbl = 1;
		break;
	case RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL:
		cfg->ctrl_word.encap_dsbl = 0;
		break;
	default:
		PMD_DRV_LOG(ERR, "Unsupported IPsec action for offload, action: %d",
				conf->action_type);
		return -EINVAL;
	}

	switch (conf->ipsec.proto) {
	case RTE_SECURITY_IPSEC_SA_PROTO_ESP:
		cfg->ctrl_word.proto = NFP_IPSEC_PROTOCOL_ESP;
		break;
	case RTE_SECURITY_IPSEC_SA_PROTO_AH:
		cfg->ctrl_word.proto = NFP_IPSEC_PROTOCOL_AH;
		break;
	default:
		PMD_DRV_LOG(ERR, "Unsupported IPsec protocol for offload, protocol: %d",
				conf->ipsec.proto);
		return -EINVAL;
	}

	switch (conf->ipsec.mode) {
	case RTE_SECURITY_IPSEC_SA_MODE_TUNNEL:
		type = conf->ipsec.tunnel.type;
		cfg->ctrl_word.mode = NFP_IPSEC_MODE_TUNNEL;
		if (type == RTE_SECURITY_IPSEC_TUNNEL_IPV4) {
			cfg->src_ip.v4 = conf->ipsec.tunnel.ipv4.src_ip;
			cfg->dst_ip.v4 = conf->ipsec.tunnel.ipv4.dst_ip;
			cfg->ipv6 = 0;
		} else if (type == RTE_SECURITY_IPSEC_TUNNEL_IPV6) {
			cfg->src_ip.v6 = conf->ipsec.tunnel.ipv6.src_addr;
			cfg->dst_ip.v6 = conf->ipsec.tunnel.ipv6.dst_addr;
			cfg->ipv6 = 1;
		} else {
			PMD_DRV_LOG(ERR, "Unsupported address family!");
			return -EINVAL;
		}

		break;
	case RTE_SECURITY_IPSEC_SA_MODE_TRANSPORT:
		type = conf->ipsec.tunnel.type;
		cfg->ctrl_word.mode = NFP_IPSEC_MODE_TRANSPORT;
		if (type == RTE_SECURITY_IPSEC_TUNNEL_IPV4) {
			memset(&cfg->src_ip, 0, sizeof(cfg->src_ip));
			cfg->ipv6 = 0;
		} else if (type == RTE_SECURITY_IPSEC_TUNNEL_IPV6) {
			memset(&cfg->src_ip, 0, sizeof(cfg->src_ip));
			cfg->ipv6 = 1;
		} else {
			PMD_DRV_LOG(ERR, "Unsupported address family!");
			return -EINVAL;
		}

		break;
	default:
		PMD_DRV_LOG(ERR, "Unsupported IPsec mode for offload, mode: %d",
				conf->ipsec.mode);
		return -EINVAL;
	}

	ret = nfp_crypto_msg_build(eth_dev, conf, msg);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to build auth/crypto/aead msg!");
		return ret;
	}

	return 0;
}

static int
nfp_crypto_create_session(void *device,
		struct rte_security_session_conf *conf,
		struct rte_security_session *session)
{
	int ret;
	int sa_idx;
	struct nfp_net_hw *net_hw;
	struct nfp_ipsec_msg msg;
	struct rte_eth_dev *eth_dev;
	struct nfp_ipsec_session *priv_session;

	/* Only support IPsec at present */
	if (conf->protocol != RTE_SECURITY_PROTOCOL_IPSEC) {
		PMD_DRV_LOG(ERR, "Unsupported non-IPsec offload!");
		return -EINVAL;
	}

	sa_idx = -1;
	eth_dev = device;
	priv_session = SECURITY_GET_SESS_PRIV(session);
	net_hw = eth_dev->data->dev_private;

	if (net_hw->ipsec_data->sa_free_cnt == 0) {
		PMD_DRV_LOG(ERR, "No space in SA table, spi: %d", conf->ipsec.spi);
		return -EINVAL;
	}

	nfp_get_sa_entry(net_hw->ipsec_data, &sa_idx);

	if (sa_idx < 0) {
		PMD_DRV_LOG(ERR, "Failed to get SA entry!");
		return -EINVAL;
	}

	memset(&msg, 0, sizeof(msg));
	ret = nfp_ipsec_msg_build(eth_dev, conf, &msg);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to build IPsec msg!");
		return -EINVAL;
	}

	msg.cmd = NFP_IPSEC_CFG_MSG_ADD_SA;
	msg.sa_idx = sa_idx;
	ret = nfp_ipsec_cfg_cmd_issue(net_hw, &msg);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to add SA to nic");
		return -EINVAL;
	}

	priv_session->action = conf->action_type;
	priv_session->ipsec = conf->ipsec;
	priv_session->msg = msg.cfg_add_sa;
	priv_session->sa_index = sa_idx;
	priv_session->dev = eth_dev;
	priv_session->user_data = conf->userdata;

	net_hw->ipsec_data->sa_free_cnt--;
	net_hw->ipsec_data->sa_entries[sa_idx] = priv_session;

	return 0;
}

static int
nfp_crypto_update_session(void *device __rte_unused,
		struct rte_security_session *session,
		struct rte_security_session_conf *conf)
{
	struct nfp_ipsec_session *priv_session;

	priv_session = SECURITY_GET_SESS_PRIV(session);
	if (priv_session == NULL)
		return -EINVAL;

	/* Update IPsec ESN value */
	if (priv_session->msg.ctrl_word.ext_seq != 0 && conf->ipsec.options.esn != 0) {
		/*
		 * Store in nfp_ipsec_session for outbound SA for use
		 * in nfp_security_set_pkt_metadata() function.
		 */
		priv_session->ipsec.esn.hi = conf->ipsec.esn.hi;
		priv_session->ipsec.esn.low = conf->ipsec.esn.low;
	}

	return 0;
}

static int
nfp_security_set_pkt_metadata(void *device,
		struct rte_security_session *session,
		struct rte_mbuf *m,
		void *params)
{
	int offset;
	uint64_t *sqn;
	struct nfp_net_hw *net_hw;
	struct rte_eth_dev *eth_dev;
	struct nfp_ipsec_session *priv_session;

	sqn = params;
	eth_dev = device;
	priv_session = SECURITY_GET_SESS_PRIV(session);
	net_hw = eth_dev->data->dev_private;

	if (priv_session->ipsec.direction == RTE_SECURITY_IPSEC_SA_DIR_EGRESS) {
		struct nfp_tx_ipsec_desc_msg *desc_md;

		offset = net_hw->ipsec_data->pkt_dynfield_offset;
		desc_md = RTE_MBUF_DYNFIELD(m, offset, struct nfp_tx_ipsec_desc_msg *);

		if (priv_session->msg.ctrl_word.ext_seq != 0 && sqn != NULL) {
			desc_md->esn.low = rte_cpu_to_be_32(*sqn);
			desc_md->esn.hi = rte_cpu_to_be_32(*sqn >> 32);
		} else if (priv_session->msg.ctrl_word.ext_seq != 0) {
			desc_md->esn.low = rte_cpu_to_be_32(priv_session->ipsec.esn.low);
			desc_md->esn.hi = rte_cpu_to_be_32(priv_session->ipsec.esn.hi);
		} else {
			desc_md->esn.low = rte_cpu_to_be_32(priv_session->ipsec.esn.value);
			desc_md->esn.hi = 0;
		}

		desc_md->enc = 1;
		desc_md->sa_idx = rte_cpu_to_be_32(priv_session->sa_index);
	}

	return 0;
}

/**
 * Get discards packet statistics for each SA
 *
 * The sa_discard_stats contains the statistics of discards packets
 * of an SA. This function calculates the sum total of discarded packets.
 *
 * @param errors
 *   The value is SA discards packet sum total
 * @param sa_discard_stats
 *   The struct is SA discards packet Statistics
 */
static void
nfp_get_errorstats(uint64_t *errors,
		struct ipsec_discard_stats *sa_discard_stats)
{
	uint32_t i;
	uint32_t len;
	uint32_t *perror;

	perror = &sa_discard_stats->discards_auth;
	len = sizeof(struct ipsec_discard_stats) / sizeof(uint32_t);

	for (i = 0; i < len; i++)
		*errors += *perror++;

	*errors -= sa_discard_stats->ipv4_id_counter;
}

static int
nfp_security_session_get_stats(void *device,
		struct rte_security_session *session,
		struct rte_security_stats *stats)
{
	int ret;
	struct nfp_net_hw *net_hw;
	struct nfp_ipsec_msg msg;
	struct rte_eth_dev *eth_dev;
	struct ipsec_get_sa_stats *cfg_s;
	struct rte_security_ipsec_stats *ips_s;
	struct nfp_ipsec_session *priv_session;
	enum rte_security_ipsec_sa_direction direction;

	eth_dev = device;
	priv_session = SECURITY_GET_SESS_PRIV(session);
	memset(&msg, 0, sizeof(msg));
	msg.cmd = NFP_IPSEC_CFG_MSG_GET_SA_STATS;
	msg.sa_idx = priv_session->sa_index;
	net_hw = eth_dev->data->dev_private;

	ret = nfp_ipsec_cfg_cmd_issue(net_hw, &msg);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to get SA stats");
		return ret;
	}

	cfg_s = &msg.cfg_get_stats;
	direction = priv_session->ipsec.direction;
	memset(stats, 0, sizeof(struct rte_security_stats)); /* Start with zeros */
	stats->protocol = RTE_SECURITY_PROTOCOL_IPSEC;
	ips_s = &stats->ipsec;

	/* Only display SA if any counters are non-zero */
	if (cfg_s->lifetime_byte_count != 0 || cfg_s->pkt_count != 0) {
		if (direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS) {
			ips_s->ipackets = cfg_s->pkt_count;
			ips_s->ibytes = cfg_s->lifetime_byte_count;
			nfp_get_errorstats(&ips_s->ierrors, &cfg_s->sa_discard_stats);
		} else {
			ips_s->opackets = cfg_s->pkt_count;
			ips_s->obytes = cfg_s->lifetime_byte_count;
			nfp_get_errorstats(&ips_s->oerrors, &cfg_s->sa_discard_stats);
		}
	}

	return 0;
}

static const struct rte_security_capability *
nfp_crypto_capabilities_get(void *device __rte_unused)
{
	return nfp_security_caps;
}

static uint32_t
nfp_security_session_get_size(void *device __rte_unused)
{
	return sizeof(struct nfp_ipsec_session);
}

static int
nfp_crypto_remove_sa(struct rte_eth_dev *eth_dev,
		struct nfp_ipsec_session *priv_session)
{
	int ret;
	uint32_t sa_index;
	struct nfp_net_hw *net_hw;
	struct nfp_ipsec_msg cfg;

	sa_index = priv_session->sa_index;
	net_hw = eth_dev->data->dev_private;

	cfg.cmd = NFP_IPSEC_CFG_MSG_INV_SA;
	cfg.sa_idx = sa_index;
	ret = nfp_ipsec_cfg_cmd_issue(net_hw, &cfg);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to remove SA!");
		return -EINVAL;
	}

	net_hw->ipsec_data->sa_free_cnt++;
	net_hw->ipsec_data->sa_entries[sa_index] = NULL;

	return 0;
}

static int
nfp_crypto_remove_session(void *device,
		struct rte_security_session *session)
{
	int ret;
	struct rte_eth_dev *eth_dev;
	struct nfp_ipsec_session *priv_session;

	eth_dev = device;
	priv_session = SECURITY_GET_SESS_PRIV(session);
	if (eth_dev != priv_session->dev) {
		PMD_DRV_LOG(ERR, "Session not bound to this device");
		return -ENODEV;
	}

	ret = nfp_crypto_remove_sa(eth_dev, priv_session);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to remove session");
		return -EFAULT;
	}

	memset(priv_session, 0, sizeof(struct nfp_ipsec_session));

	return 0;
}

static const struct rte_security_ops nfp_security_ops = {
	.session_create = nfp_crypto_create_session,
	.session_update = nfp_crypto_update_session,
	.session_get_size = nfp_security_session_get_size,
	.session_stats_get = nfp_security_session_get_stats,
	.session_destroy = nfp_crypto_remove_session,
	.set_pkt_metadata = nfp_security_set_pkt_metadata,
	.capabilities_get = nfp_crypto_capabilities_get,
};

static int
nfp_ipsec_ctx_create(struct rte_eth_dev *dev,
		struct nfp_net_ipsec_data *data)
{
	struct rte_security_ctx *ctx;
	static const struct rte_mbuf_dynfield pkt_md_dynfield = {
		.name = "nfp_ipsec_crypto_pkt_metadata",
		.size = sizeof(struct nfp_tx_ipsec_desc_msg),
		.align = __alignof__(struct nfp_tx_ipsec_desc_msg),
	};

	ctx = rte_zmalloc("security_ctx",
			sizeof(struct rte_security_ctx), 0);
	if (ctx == NULL) {
		PMD_INIT_LOG(ERR, "Failed to malloc security_ctx");
		return -ENOMEM;
	}

	ctx->device = dev;
	ctx->ops = &nfp_security_ops;
	ctx->sess_cnt = 0;
	dev->security_ctx = ctx;

	data->pkt_dynfield_offset = rte_mbuf_dynfield_register(&pkt_md_dynfield);
	if (data->pkt_dynfield_offset < 0) {
		PMD_INIT_LOG(ERR, "Failed to register mbuf esn_dynfield");
		return -ENOMEM;
	}

	return 0;
}

int
nfp_ipsec_init(struct rte_eth_dev *dev)
{
	int ret;
	uint32_t cap_extend;
	struct nfp_net_hw *net_hw;
	struct nfp_net_ipsec_data *data;

	net_hw = dev->data->dev_private;

	cap_extend = net_hw->super.cap_ext;
	if ((cap_extend & NFP_NET_CFG_CTRL_IPSEC) == 0) {
		PMD_INIT_LOG(INFO, "Unsupported IPsec extend capability");
		return 0;
	}

	data = rte_zmalloc("ipsec_data", sizeof(struct nfp_net_ipsec_data), 0);
	if (data == NULL) {
		PMD_INIT_LOG(ERR, "Failed to malloc ipsec_data");
		return -ENOMEM;
	}

	data->pkt_dynfield_offset = -1;
	data->sa_free_cnt = NFP_NET_IPSEC_MAX_SA_CNT;
	net_hw->ipsec_data = data;

	ret = nfp_ipsec_ctx_create(dev, data);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Failed to create IPsec ctx");
		goto ipsec_cleanup;
	}

	return 0;

ipsec_cleanup:
	nfp_ipsec_uninit(dev);

	return ret;
}

static void
nfp_ipsec_ctx_destroy(struct rte_eth_dev *dev)
{
	rte_free(dev->security_ctx);
}

void
nfp_ipsec_uninit(struct rte_eth_dev *dev)
{
	uint16_t i;
	uint32_t cap_extend;
	struct nfp_net_hw *net_hw;
	struct nfp_ipsec_session *priv_session;

	net_hw = dev->data->dev_private;

	cap_extend = net_hw->super.cap_ext;
	if ((cap_extend & NFP_NET_CFG_CTRL_IPSEC) == 0) {
		PMD_INIT_LOG(INFO, "Unsupported IPsec extend capability");
		return;
	}

	nfp_ipsec_ctx_destroy(dev);

	if (net_hw->ipsec_data == NULL) {
		PMD_INIT_LOG(INFO, "IPsec data is NULL!");
		return;
	}

	for (i = 0; i < NFP_NET_IPSEC_MAX_SA_CNT; i++) {
		priv_session = net_hw->ipsec_data->sa_entries[i];
		if (priv_session != NULL)
			memset(priv_session, 0, sizeof(struct nfp_ipsec_session));
	}

	rte_free(net_hw->ipsec_data);
}

