/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include <ethdev_pci.h>
#include <rte_security_driver.h>
#include <rte_cryptodev.h>

#include "base/txgbe.h"
#include "txgbe_ethdev.h"
#include "txgbe_ipsec.h"

#define CMP_IP(a, b) (\
	(a).ipv6[0] == (b).ipv6[0] && \
	(a).ipv6[1] == (b).ipv6[1] && \
	(a).ipv6[2] == (b).ipv6[2] && \
	(a).ipv6[3] == (b).ipv6[3])

static void
txgbe_crypto_clear_ipsec_tables(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_ipsec *priv = TXGBE_DEV_IPSEC(dev);
	int i = 0;

	/* clear Rx IP table*/
	for (i = 0; i < IPSEC_MAX_RX_IP_COUNT; i++) {
		uint16_t index = i << 3;
		uint32_t reg_val = TXGBE_IPSRXIDX_WRITE |
				TXGBE_IPSRXIDX_TB_IP | index;
		wr32(hw, TXGBE_IPSRXADDR(0), 0);
		wr32(hw, TXGBE_IPSRXADDR(1), 0);
		wr32(hw, TXGBE_IPSRXADDR(2), 0);
		wr32(hw, TXGBE_IPSRXADDR(3), 0);
		wr32w(hw, TXGBE_IPSRXIDX, reg_val, TXGBE_IPSRXIDX_WRITE, 1000);
	}

	/* clear Rx SPI and Rx/Tx SA tables*/
	for (i = 0; i < IPSEC_MAX_SA_COUNT; i++) {
		uint32_t index = i << 3;
		uint32_t reg_val = TXGBE_IPSRXIDX_WRITE |
				TXGBE_IPSRXIDX_TB_SPI | index;
		wr32(hw, TXGBE_IPSRXSPI, 0);
		wr32(hw, TXGBE_IPSRXADDRIDX, 0);
		wr32w(hw, TXGBE_IPSRXIDX, reg_val, TXGBE_IPSRXIDX_WRITE, 1000);
		reg_val = TXGBE_IPSRXIDX_WRITE | TXGBE_IPSRXIDX_TB_KEY | index;
		wr32(hw, TXGBE_IPSRXKEY(0), 0);
		wr32(hw, TXGBE_IPSRXKEY(1), 0);
		wr32(hw, TXGBE_IPSRXKEY(2), 0);
		wr32(hw, TXGBE_IPSRXKEY(3), 0);
		wr32(hw, TXGBE_IPSRXSALT, 0);
		wr32(hw, TXGBE_IPSRXMODE, 0);
		wr32w(hw, TXGBE_IPSRXIDX, reg_val, TXGBE_IPSRXIDX_WRITE, 1000);
		reg_val = TXGBE_IPSTXIDX_WRITE | index;
		wr32(hw, TXGBE_IPSTXKEY(0), 0);
		wr32(hw, TXGBE_IPSTXKEY(1), 0);
		wr32(hw, TXGBE_IPSTXKEY(2), 0);
		wr32(hw, TXGBE_IPSTXKEY(3), 0);
		wr32(hw, TXGBE_IPSTXSALT, 0);
		wr32w(hw, TXGBE_IPSTXIDX, reg_val, TXGBE_IPSTXIDX_WRITE, 1000);
	}

	memset(priv->rx_ip_tbl, 0, sizeof(priv->rx_ip_tbl));
	memset(priv->rx_sa_tbl, 0, sizeof(priv->rx_sa_tbl));
	memset(priv->tx_sa_tbl, 0, sizeof(priv->tx_sa_tbl));
}

static int
txgbe_crypto_add_sa(struct txgbe_crypto_session *ic_session)
{
	struct rte_eth_dev *dev = ic_session->dev;
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_ipsec *priv = TXGBE_DEV_IPSEC(dev);
	uint32_t reg_val;
	int sa_index = -1;

	if (ic_session->op == TXGBE_OP_AUTHENTICATED_DECRYPTION) {
		int i, ip_index = -1;
		uint8_t *key;

		/* Find a match in the IP table*/
		for (i = 0; i < IPSEC_MAX_RX_IP_COUNT; i++) {
			if (CMP_IP(priv->rx_ip_tbl[i].ip,
				   ic_session->dst_ip)) {
				ip_index = i;
				break;
			}
		}
		/* If no match, find a free entry in the IP table*/
		if (ip_index < 0) {
			for (i = 0; i < IPSEC_MAX_RX_IP_COUNT; i++) {
				if (priv->rx_ip_tbl[i].ref_count == 0) {
					ip_index = i;
					break;
				}
			}
		}

		/* Fail if no match and no free entries*/
		if (ip_index < 0) {
			PMD_DRV_LOG(ERR,
				    "No free entry left in the Rx IP table\n");
			return -1;
		}

		/* Find a free entry in the SA table*/
		for (i = 0; i < IPSEC_MAX_SA_COUNT; i++) {
			if (priv->rx_sa_tbl[i].used == 0) {
				sa_index = i;
				break;
			}
		}
		/* Fail if no free entries*/
		if (sa_index < 0) {
			PMD_DRV_LOG(ERR,
				    "No free entry left in the Rx SA table\n");
			return -1;
		}

		priv->rx_ip_tbl[ip_index].ip.ipv6[0] =
				ic_session->dst_ip.ipv6[0];
		priv->rx_ip_tbl[ip_index].ip.ipv6[1] =
				ic_session->dst_ip.ipv6[1];
		priv->rx_ip_tbl[ip_index].ip.ipv6[2] =
				ic_session->dst_ip.ipv6[2];
		priv->rx_ip_tbl[ip_index].ip.ipv6[3] =
				ic_session->dst_ip.ipv6[3];
		priv->rx_ip_tbl[ip_index].ref_count++;

		priv->rx_sa_tbl[sa_index].spi = ic_session->spi;
		priv->rx_sa_tbl[sa_index].ip_index = ip_index;
		priv->rx_sa_tbl[sa_index].mode = IPSRXMOD_VALID;
		if (ic_session->op == TXGBE_OP_AUTHENTICATED_DECRYPTION)
			priv->rx_sa_tbl[sa_index].mode |=
					(IPSRXMOD_PROTO | IPSRXMOD_DECRYPT);
		if (ic_session->dst_ip.type == IPv6) {
			priv->rx_sa_tbl[sa_index].mode |= IPSRXMOD_IPV6;
			priv->rx_ip_tbl[ip_index].ip.type = IPv6;
		} else if (ic_session->dst_ip.type == IPv4) {
			priv->rx_ip_tbl[ip_index].ip.type = IPv4;
		}
		priv->rx_sa_tbl[sa_index].used = 1;

		/* write IP table entry*/
		reg_val = TXGBE_IPSRXIDX_ENA | TXGBE_IPSRXIDX_WRITE |
				TXGBE_IPSRXIDX_TB_IP | (ip_index << 3);
		if (priv->rx_ip_tbl[ip_index].ip.type == IPv4) {
			uint32_t ipv4 = priv->rx_ip_tbl[ip_index].ip.ipv4;
			wr32(hw, TXGBE_IPSRXADDR(0), rte_cpu_to_be_32(ipv4));
			wr32(hw, TXGBE_IPSRXADDR(1), 0);
			wr32(hw, TXGBE_IPSRXADDR(2), 0);
			wr32(hw, TXGBE_IPSRXADDR(3), 0);
		} else {
			wr32(hw, TXGBE_IPSRXADDR(0),
					priv->rx_ip_tbl[ip_index].ip.ipv6[0]);
			wr32(hw, TXGBE_IPSRXADDR(1),
					priv->rx_ip_tbl[ip_index].ip.ipv6[1]);
			wr32(hw, TXGBE_IPSRXADDR(2),
					priv->rx_ip_tbl[ip_index].ip.ipv6[2]);
			wr32(hw, TXGBE_IPSRXADDR(3),
					priv->rx_ip_tbl[ip_index].ip.ipv6[3]);
		}
		wr32w(hw, TXGBE_IPSRXIDX, reg_val, TXGBE_IPSRXIDX_WRITE, 1000);

		/* write SPI table entry*/
		reg_val = TXGBE_IPSRXIDX_ENA | TXGBE_IPSRXIDX_WRITE |
				TXGBE_IPSRXIDX_TB_SPI | (sa_index << 3);
		wr32(hw, TXGBE_IPSRXSPI,
				priv->rx_sa_tbl[sa_index].spi);
		wr32(hw, TXGBE_IPSRXADDRIDX,
				priv->rx_sa_tbl[sa_index].ip_index);
		wr32w(hw, TXGBE_IPSRXIDX, reg_val, TXGBE_IPSRXIDX_WRITE, 1000);

		/* write Key table entry*/
		key = malloc(ic_session->key_len);
		if (!key)
			return -ENOMEM;

		memcpy(key, ic_session->key, ic_session->key_len);

		reg_val = TXGBE_IPSRXIDX_ENA | TXGBE_IPSRXIDX_WRITE |
				TXGBE_IPSRXIDX_TB_KEY | (sa_index << 3);
		wr32(hw, TXGBE_IPSRXKEY(0),
			rte_cpu_to_be_32(*(uint32_t *)&key[12]));
		wr32(hw, TXGBE_IPSRXKEY(1),
			rte_cpu_to_be_32(*(uint32_t *)&key[8]));
		wr32(hw, TXGBE_IPSRXKEY(2),
			rte_cpu_to_be_32(*(uint32_t *)&key[4]));
		wr32(hw, TXGBE_IPSRXKEY(3),
			rte_cpu_to_be_32(*(uint32_t *)&key[0]));
		wr32(hw, TXGBE_IPSRXSALT,
				rte_cpu_to_be_32(ic_session->salt));
		wr32(hw, TXGBE_IPSRXMODE,
				priv->rx_sa_tbl[sa_index].mode);
		wr32w(hw, TXGBE_IPSRXIDX, reg_val, TXGBE_IPSRXIDX_WRITE, 1000);

		free(key);
	} else { /* sess->dir == RTE_CRYPTO_OUTBOUND */
		uint8_t *key;
		int i;

		/* Find a free entry in the SA table*/
		for (i = 0; i < IPSEC_MAX_SA_COUNT; i++) {
			if (priv->tx_sa_tbl[i].used == 0) {
				sa_index = i;
				break;
			}
		}
		/* Fail if no free entries*/
		if (sa_index < 0) {
			PMD_DRV_LOG(ERR,
				    "No free entry left in the Tx SA table\n");
			return -1;
		}

		priv->tx_sa_tbl[sa_index].spi =
			rte_cpu_to_be_32(ic_session->spi);
		priv->tx_sa_tbl[i].used = 1;
		ic_session->sa_index = sa_index;

		key = malloc(ic_session->key_len);
		if (!key)
			return -ENOMEM;

		memcpy(key, ic_session->key, ic_session->key_len);

		/* write Key table entry*/
		reg_val = TXGBE_IPSRXIDX_ENA |
			TXGBE_IPSRXIDX_WRITE | (sa_index << 3);
		wr32(hw, TXGBE_IPSTXKEY(0),
			rte_cpu_to_be_32(*(uint32_t *)&key[12]));
		wr32(hw, TXGBE_IPSTXKEY(1),
			rte_cpu_to_be_32(*(uint32_t *)&key[8]));
		wr32(hw, TXGBE_IPSTXKEY(2),
			rte_cpu_to_be_32(*(uint32_t *)&key[4]));
		wr32(hw, TXGBE_IPSTXKEY(3),
			rte_cpu_to_be_32(*(uint32_t *)&key[0]));
		wr32(hw, TXGBE_IPSTXSALT,
				rte_cpu_to_be_32(ic_session->salt));
		wr32w(hw, TXGBE_IPSTXIDX, reg_val, TXGBE_IPSTXIDX_WRITE, 1000);

		free(key);
	}

	return 0;
}

static int
txgbe_crypto_remove_sa(struct rte_eth_dev *dev,
		       struct txgbe_crypto_session *ic_session)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_ipsec *priv = TXGBE_DEV_IPSEC(dev);
	uint32_t reg_val;
	int sa_index = -1;

	if (ic_session->op == TXGBE_OP_AUTHENTICATED_DECRYPTION) {
		int i, ip_index = -1;

		/* Find a match in the IP table*/
		for (i = 0; i < IPSEC_MAX_RX_IP_COUNT; i++) {
			if (CMP_IP(priv->rx_ip_tbl[i].ip, ic_session->dst_ip)) {
				ip_index = i;
				break;
			}
		}

		/* Fail if no match*/
		if (ip_index < 0) {
			PMD_DRV_LOG(ERR,
				    "Entry not found in the Rx IP table\n");
			return -1;
		}

		/* Find a free entry in the SA table*/
		for (i = 0; i < IPSEC_MAX_SA_COUNT; i++) {
			if (priv->rx_sa_tbl[i].spi ==
				  rte_cpu_to_be_32(ic_session->spi)) {
				sa_index = i;
				break;
			}
		}
		/* Fail if no match*/
		if (sa_index < 0) {
			PMD_DRV_LOG(ERR,
				    "Entry not found in the Rx SA table\n");
			return -1;
		}

		/* Disable and clear Rx SPI and key table entries */
		reg_val = TXGBE_IPSRXIDX_WRITE |
			TXGBE_IPSRXIDX_TB_SPI | (sa_index << 3);
		wr32(hw, TXGBE_IPSRXSPI, 0);
		wr32(hw, TXGBE_IPSRXADDRIDX, 0);
		wr32w(hw, TXGBE_IPSRXIDX, reg_val, TXGBE_IPSRXIDX_WRITE, 1000);
		reg_val = TXGBE_IPSRXIDX_WRITE |
			TXGBE_IPSRXIDX_TB_KEY | (sa_index << 3);
		wr32(hw, TXGBE_IPSRXKEY(0), 0);
		wr32(hw, TXGBE_IPSRXKEY(1), 0);
		wr32(hw, TXGBE_IPSRXKEY(2), 0);
		wr32(hw, TXGBE_IPSRXKEY(3), 0);
		wr32(hw, TXGBE_IPSRXSALT, 0);
		wr32(hw, TXGBE_IPSRXMODE, 0);
		wr32w(hw, TXGBE_IPSRXIDX, reg_val, TXGBE_IPSRXIDX_WRITE, 1000);
		priv->rx_sa_tbl[sa_index].used = 0;

		/* If last used then clear the IP table entry*/
		priv->rx_ip_tbl[ip_index].ref_count--;
		if (priv->rx_ip_tbl[ip_index].ref_count == 0) {
			reg_val = TXGBE_IPSRXIDX_WRITE | TXGBE_IPSRXIDX_TB_IP |
					(ip_index << 3);
			wr32(hw, TXGBE_IPSRXADDR(0), 0);
			wr32(hw, TXGBE_IPSRXADDR(1), 0);
			wr32(hw, TXGBE_IPSRXADDR(2), 0);
			wr32(hw, TXGBE_IPSRXADDR(3), 0);
		}
	} else { /* session->dir == RTE_CRYPTO_OUTBOUND */
		int i;

		/* Find a match in the SA table*/
		for (i = 0; i < IPSEC_MAX_SA_COUNT; i++) {
			if (priv->tx_sa_tbl[i].spi ==
				    rte_cpu_to_be_32(ic_session->spi)) {
				sa_index = i;
				break;
			}
		}
		/* Fail if no match entries*/
		if (sa_index < 0) {
			PMD_DRV_LOG(ERR,
				    "Entry not found in the Tx SA table\n");
			return -1;
		}
		reg_val = TXGBE_IPSRXIDX_WRITE | (sa_index << 3);
		wr32(hw, TXGBE_IPSTXKEY(0), 0);
		wr32(hw, TXGBE_IPSTXKEY(1), 0);
		wr32(hw, TXGBE_IPSTXKEY(2), 0);
		wr32(hw, TXGBE_IPSTXKEY(3), 0);
		wr32(hw, TXGBE_IPSTXSALT, 0);
		wr32w(hw, TXGBE_IPSTXIDX, reg_val, TXGBE_IPSTXIDX_WRITE, 1000);

		priv->tx_sa_tbl[sa_index].used = 0;
	}

	return 0;
}

static int
txgbe_crypto_create_session(void *device,
		struct rte_security_session_conf *conf,
		struct rte_security_session *session)
{
	struct rte_eth_dev *eth_dev = (struct rte_eth_dev *)device;
	struct txgbe_crypto_session *ic_session = SECURITY_GET_SESS_PRIV(session);
	struct rte_crypto_aead_xform *aead_xform;
	struct rte_eth_conf *dev_conf = &eth_dev->data->dev_conf;

	if (conf->crypto_xform->type != RTE_CRYPTO_SYM_XFORM_AEAD ||
			conf->crypto_xform->aead.algo !=
					RTE_CRYPTO_AEAD_AES_GCM) {
		PMD_DRV_LOG(ERR, "Unsupported crypto transformation mode\n");
		return -ENOTSUP;
	}
	aead_xform = &conf->crypto_xform->aead;

	if (conf->ipsec.direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS) {
		if (dev_conf->rxmode.offloads & RTE_ETH_RX_OFFLOAD_SECURITY) {
			ic_session->op = TXGBE_OP_AUTHENTICATED_DECRYPTION;
		} else {
			PMD_DRV_LOG(ERR, "IPsec decryption not enabled\n");
			return -ENOTSUP;
		}
	} else {
		if (dev_conf->txmode.offloads & RTE_ETH_TX_OFFLOAD_SECURITY) {
			ic_session->op = TXGBE_OP_AUTHENTICATED_ENCRYPTION;
		} else {
			PMD_DRV_LOG(ERR, "IPsec encryption not enabled\n");
			return -ENOTSUP;
		}
	}

	ic_session->key = aead_xform->key.data;
	ic_session->key_len = aead_xform->key.length;
	memcpy(&ic_session->salt,
	       &aead_xform->key.data[aead_xform->key.length], 4);
	ic_session->spi = conf->ipsec.spi;
	ic_session->dev = eth_dev;

	if (ic_session->op == TXGBE_OP_AUTHENTICATED_ENCRYPTION) {
		if (txgbe_crypto_add_sa(ic_session)) {
			PMD_DRV_LOG(ERR, "Failed to add SA\n");
			return -EPERM;
		}
	}

	return 0;
}

static unsigned int
txgbe_crypto_session_get_size(__rte_unused void *device)
{
	return sizeof(struct txgbe_crypto_session);
}

static int
txgbe_crypto_remove_session(void *device,
		struct rte_security_session *session)
{
	struct rte_eth_dev *eth_dev = device;
	struct txgbe_crypto_session *ic_session = SECURITY_GET_SESS_PRIV(session);

	if (eth_dev != ic_session->dev) {
		PMD_DRV_LOG(ERR, "Session not bound to this device\n");
		return -ENODEV;
	}

	if (txgbe_crypto_remove_sa(eth_dev, ic_session)) {
		PMD_DRV_LOG(ERR, "Failed to remove session\n");
		return -EFAULT;
	}

	memset(ic_session, 0, sizeof(struct txgbe_crypto_session));

	return 0;
}

static inline uint8_t
txgbe_crypto_compute_pad_len(struct rte_mbuf *m)
{
	if (m->nb_segs == 1) {
		/* 16 bytes ICV + 2 bytes ESP trailer + payload padding size
		 * payload padding size is stored at <pkt_len - 18>
		 */
		uint8_t *esp_pad_len = rte_pktmbuf_mtod_offset(m, uint8_t *,
					rte_pktmbuf_pkt_len(m) -
					(ESP_TRAILER_SIZE + ESP_ICV_SIZE));
		return *esp_pad_len + ESP_TRAILER_SIZE + ESP_ICV_SIZE;
	}
	return 0;
}

static int
txgbe_crypto_update_mb(void *device __rte_unused,
		struct rte_security_session *session,
		       struct rte_mbuf *m, void *params __rte_unused)
{
	struct txgbe_crypto_session *ic_session = SECURITY_GET_SESS_PRIV(session);

	if (ic_session->op == TXGBE_OP_AUTHENTICATED_ENCRYPTION) {
		union txgbe_crypto_tx_desc_md *mdata =
			(union txgbe_crypto_tx_desc_md *)
				rte_security_dynfield(m);
		mdata->enc = 1;
		mdata->sa_idx = ic_session->sa_index;
		mdata->pad_len = txgbe_crypto_compute_pad_len(m);
	}
	return 0;
}

static const struct rte_security_capability *
txgbe_crypto_capabilities_get(void *device __rte_unused)
{
	static const struct rte_cryptodev_capabilities
	aes_gcm_gmac_crypto_capabilities[] = {
		{	/* AES GMAC (128-bit) */
			.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
			{.sym = {
				.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
				{.auth = {
					.algo = RTE_CRYPTO_AUTH_AES_GMAC,
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
					.iv_size = {
						.min = 12,
						.max = 12,
						.increment = 0
					}
				}, }
			}, }
		},
		{	/* AES GCM (128-bit) */
			.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
			{.sym = {
				.xform_type = RTE_CRYPTO_SYM_XFORM_AEAD,
				{.aead = {
					.algo = RTE_CRYPTO_AEAD_AES_GCM,
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
					.aad_size = {
						.min = 0,
						.max = 65535,
						.increment = 1
					},
					.iv_size = {
						.min = 12,
						.max = 12,
						.increment = 0
					}
				}, }
			}, }
		},
		{
			.op = RTE_CRYPTO_OP_TYPE_UNDEFINED,
			{.sym = {
				.xform_type = RTE_CRYPTO_SYM_XFORM_NOT_SPECIFIED
			}, }
		},
	};

	static const struct rte_security_capability
	txgbe_security_capabilities[] = {
		{ /* IPsec Inline Crypto ESP Transport Egress */
			.action = RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO,
			.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
			{.ipsec = {
				.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
				.mode = RTE_SECURITY_IPSEC_SA_MODE_TRANSPORT,
				.direction = RTE_SECURITY_IPSEC_SA_DIR_EGRESS,
				.options = { 0 }
			} },
			.crypto_capabilities = aes_gcm_gmac_crypto_capabilities,
			.ol_flags = RTE_SECURITY_TX_OLOAD_NEED_MDATA
		},
		{ /* IPsec Inline Crypto ESP Transport Ingress */
			.action = RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO,
			.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
			{.ipsec = {
				.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
				.mode = RTE_SECURITY_IPSEC_SA_MODE_TRANSPORT,
				.direction = RTE_SECURITY_IPSEC_SA_DIR_INGRESS,
				.options = { 0 }
			} },
			.crypto_capabilities = aes_gcm_gmac_crypto_capabilities,
			.ol_flags = 0
		},
		{ /* IPsec Inline Crypto ESP Tunnel Egress */
			.action = RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO,
			.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
			{.ipsec = {
				.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
				.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL,
				.direction = RTE_SECURITY_IPSEC_SA_DIR_EGRESS,
				.options = { 0 }
			} },
			.crypto_capabilities = aes_gcm_gmac_crypto_capabilities,
			.ol_flags = RTE_SECURITY_TX_OLOAD_NEED_MDATA
		},
		{ /* IPsec Inline Crypto ESP Tunnel Ingress */
			.action = RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO,
			.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
			{.ipsec = {
				.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
				.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL,
				.direction = RTE_SECURITY_IPSEC_SA_DIR_INGRESS,
				.options = { 0 }
			} },
			.crypto_capabilities = aes_gcm_gmac_crypto_capabilities,
			.ol_flags = 0
		},
		{
			.action = RTE_SECURITY_ACTION_TYPE_NONE
		}
	};

	return txgbe_security_capabilities;
}

int
txgbe_crypto_enable_ipsec(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint32_t reg;
	uint64_t rx_offloads;
	uint64_t tx_offloads;

	rx_offloads = dev->data->dev_conf.rxmode.offloads;
	tx_offloads = dev->data->dev_conf.txmode.offloads;

	/* sanity checks */
	if (rx_offloads & RTE_ETH_RX_OFFLOAD_TCP_LRO) {
		PMD_DRV_LOG(ERR, "RSC and IPsec not supported");
		return -1;
	}
	if (rx_offloads & RTE_ETH_RX_OFFLOAD_KEEP_CRC) {
		PMD_DRV_LOG(ERR, "HW CRC strip needs to be enabled for IPsec");
		return -1;
	}

	/* Set TXGBE_SECTXBUFFAF to 0x14 as required in the datasheet*/
	wr32(hw, TXGBE_SECTXBUFAF, 0x14);

	/* IFG needs to be set to 3 when we are using security. Otherwise a Tx
	 * hang will occur with heavy traffic.
	 */
	reg = rd32(hw, TXGBE_SECTXIFG);
	reg = (reg & ~TXGBE_SECTXIFG_MIN_MASK) | TXGBE_SECTXIFG_MIN(0x3);
	wr32(hw, TXGBE_SECTXIFG, reg);

	reg = rd32(hw, TXGBE_SECRXCTL);
	reg |= TXGBE_SECRXCTL_CRCSTRIP;
	wr32(hw, TXGBE_SECRXCTL, reg);

	if (rx_offloads & RTE_ETH_RX_OFFLOAD_SECURITY) {
		wr32m(hw, TXGBE_SECRXCTL, TXGBE_SECRXCTL_ODSA, 0);
		reg = rd32m(hw, TXGBE_SECRXCTL, TXGBE_SECRXCTL_ODSA);
		if (reg != 0) {
			PMD_DRV_LOG(ERR, "Error enabling Rx Crypto");
			return -1;
		}
	}
	if (tx_offloads & RTE_ETH_TX_OFFLOAD_SECURITY) {
		wr32(hw, TXGBE_SECTXCTL, TXGBE_SECTXCTL_STFWD);
		reg = rd32(hw, TXGBE_SECTXCTL);
		if (reg != TXGBE_SECTXCTL_STFWD) {
			PMD_DRV_LOG(ERR, "Error enabling Rx Crypto");
			return -1;
		}
	}

	txgbe_crypto_clear_ipsec_tables(dev);

	return 0;
}

int
txgbe_crypto_add_ingress_sa_from_flow(const void *sess,
				      const void *ip_spec,
				      uint8_t is_ipv6)
{
	/**
	 * FIXME Updating the session priv data when the session is const.
	 * Typecasting done here is wrong and the implementation need to be corrected.
	 */
	struct txgbe_crypto_session *ic_session = (void *)(uintptr_t)
			((const struct rte_security_session *)sess)->driver_priv_data;

	if (ic_session->op == TXGBE_OP_AUTHENTICATED_DECRYPTION) {
		if (is_ipv6) {
			const struct rte_flow_item_ipv6 *ipv6 = ip_spec;
			ic_session->src_ip.type = IPv6;
			ic_session->dst_ip.type = IPv6;
			rte_memcpy(ic_session->src_ip.ipv6,
				   ipv6->hdr.src_addr, 16);
			rte_memcpy(ic_session->dst_ip.ipv6,
				   ipv6->hdr.dst_addr, 16);
		} else {
			const struct rte_flow_item_ipv4 *ipv4 = ip_spec;
			ic_session->src_ip.type = IPv4;
			ic_session->dst_ip.type = IPv4;
			ic_session->src_ip.ipv4 = ipv4->hdr.src_addr;
			ic_session->dst_ip.ipv4 = ipv4->hdr.dst_addr;
		}
		return txgbe_crypto_add_sa(ic_session);
	}

	return 0;
}

static struct rte_security_ops txgbe_security_ops = {
	.session_create = txgbe_crypto_create_session,
	.session_get_size = txgbe_crypto_session_get_size,
	.session_destroy = txgbe_crypto_remove_session,
	.set_pkt_metadata = txgbe_crypto_update_mb,
	.capabilities_get = txgbe_crypto_capabilities_get
};

static int
txgbe_crypto_capable(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint32_t reg_i, reg, capable = 1;
	/* test if rx crypto can be enabled and then write back initial value*/
	reg_i = rd32(hw, TXGBE_SECRXCTL);
	wr32m(hw, TXGBE_SECRXCTL, TXGBE_SECRXCTL_ODSA, 0);
	reg = rd32m(hw, TXGBE_SECRXCTL, TXGBE_SECRXCTL_ODSA);
	if (reg != 0)
		capable = 0;
	wr32(hw, TXGBE_SECRXCTL, reg_i);
	return capable;
}

int
txgbe_ipsec_ctx_create(struct rte_eth_dev *dev)
{
	struct rte_security_ctx *ctx = NULL;

	if (txgbe_crypto_capable(dev)) {
		ctx = rte_malloc("rte_security_instances_ops",
				 sizeof(struct rte_security_ctx), 0);
		if (ctx) {
			ctx->device = (void *)dev;
			ctx->ops = &txgbe_security_ops;
			ctx->sess_cnt = 0;
			dev->security_ctx = ctx;
		} else {
			return -ENOMEM;
		}
	}
	if (rte_security_dynfield_register() < 0)
		return -rte_errno;
	return 0;
}
