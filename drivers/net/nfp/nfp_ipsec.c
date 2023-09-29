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

#include "nfp_common.h"
#include "nfp_ctrl.h"
#include "nfp_logs.h"
#include "nfp_rxtx.h"

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
		.crypto_capabilities = nfp_crypto_caps
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
		.crypto_capabilities = nfp_crypto_caps,
		.ol_flags = RTE_SECURITY_TX_OLOAD_NEED_MDATA
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
		.crypto_capabilities = nfp_crypto_caps
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
		.crypto_capabilities = nfp_crypto_caps,
		.ol_flags = RTE_SECURITY_TX_OLOAD_NEED_MDATA
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
		.crypto_capabilities = nfp_crypto_caps
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
		.crypto_capabilities = nfp_crypto_caps,
		.ol_flags = RTE_SECURITY_TX_OLOAD_NEED_MDATA
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
		.crypto_capabilities = nfp_crypto_caps
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
		.crypto_capabilities = nfp_crypto_caps,
		.ol_flags = RTE_SECURITY_TX_OLOAD_NEED_MDATA
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

static int
nfp_ipsec_cfg_cmd_issue(struct nfp_net_hw *hw,
		struct nfp_ipsec_msg *msg)
{
	int ret;
	uint32_t i;
	uint32_t msg_size;

	msg_size = RTE_DIM(msg->raw);
	msg->rsp = NFP_IPSEC_CFG_MSG_OK;

	for (i = 0; i < msg_size; i++)
		nn_cfg_writel(hw, NFP_NET_CFG_MBOX_VAL + 4 * i, msg->raw[i]);

	ret = nfp_net_mbox_reconfig(hw, NFP_NET_CFG_MBOX_CMD_IPSEC);
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
		msg->raw[i] = nn_cfg_readl(hw, NFP_NET_CFG_MBOX_VAL + 4 * i);

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
	struct nfp_net_hw *hw;
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
	hw = NFP_NET_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);

	ret = nfp_ipsec_cfg_cmd_issue(hw, &msg);
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

static const struct rte_security_ops nfp_security_ops = {
	.session_get_size = nfp_security_session_get_size,
	.session_stats_get = nfp_security_session_get_stats,
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
	struct nfp_net_hw *hw;
	struct nfp_net_ipsec_data *data;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	cap_extend = nn_cfg_readl(hw, NFP_NET_CFG_CAP_WORD1);
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
	hw->ipsec_data = data;

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
	if (dev->security_ctx != NULL)
		rte_free(dev->security_ctx);
}

void
nfp_ipsec_uninit(struct rte_eth_dev *dev)
{
	uint16_t i;
	uint32_t cap_extend;
	struct nfp_net_hw *hw;
	struct nfp_ipsec_session *priv_session;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	cap_extend = nn_cfg_readl(hw, NFP_NET_CFG_CAP_WORD1);
	if ((cap_extend & NFP_NET_CFG_CTRL_IPSEC) == 0) {
		PMD_INIT_LOG(INFO, "Unsupported IPsec extend capability");
		return;
	}

	nfp_ipsec_ctx_destroy(dev);

	if (hw->ipsec_data == NULL) {
		PMD_INIT_LOG(INFO, "IPsec data is NULL!");
		return;
	}

	for (i = 0; i < NFP_NET_IPSEC_MAX_SA_CNT; i++) {
		priv_session = hw->ipsec_data->sa_entries[i];
		if (priv_session != NULL)
			memset(priv_session, 0, sizeof(struct nfp_ipsec_session));
	}

	rte_free(hw->ipsec_data);
}

