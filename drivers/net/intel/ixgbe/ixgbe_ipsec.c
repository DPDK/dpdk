/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include <ethdev_driver.h>
#include <ethdev_pci.h>
#include <rte_ip.h>
#include <rte_jhash.h>
#include <rte_security_driver.h>
#include <rte_cryptodev.h>
#include <rte_flow.h>

#include "base/ixgbe_type.h"
#include "base/ixgbe_api.h"
#include "ixgbe_ethdev.h"
#include "ixgbe_ipsec.h"

#define IXGBE_REGISTER_POLL_WAIT_5_MS  5

#define IXGBE_WAIT_RREAD \
	IXGBE_WRITE_REG_THEN_POLL_MASK(hw, IXGBE_IPSRXIDX, reg_val, \
	IPSRXIDX_READ, IXGBE_REGISTER_POLL_WAIT_5_MS)
#define IXGBE_WAIT_RWRITE \
	IXGBE_WRITE_REG_THEN_POLL_MASK(hw, IXGBE_IPSRXIDX, reg_val, \
	IPSRXIDX_WRITE, IXGBE_REGISTER_POLL_WAIT_5_MS)
#define IXGBE_WAIT_TREAD \
	IXGBE_WRITE_REG_THEN_POLL_MASK(hw, IXGBE_IPSTXIDX, reg_val, \
	IPSRXIDX_READ, IXGBE_REGISTER_POLL_WAIT_5_MS)
#define IXGBE_WAIT_TWRITE \
	IXGBE_WRITE_REG_THEN_POLL_MASK(hw, IXGBE_IPSTXIDX, reg_val, \
	IPSRXIDX_WRITE, IXGBE_REGISTER_POLL_WAIT_5_MS)

#define CMP_IP(a, b) (\
	(a).ipv6[0] == (b).ipv6[0] && \
	(a).ipv6[1] == (b).ipv6[1] && \
	(a).ipv6[2] == (b).ipv6[2] && \
	(a).ipv6[3] == (b).ipv6[3])

static inline void
ixgbe_crypto_write_rx_ip(struct ixgbe_hw *hw, uint32_t idx,
		const struct ipaddr *ip, bool enable)
{
	uint32_t reg_val = IPSRXIDX_WRITE | IPSRXIDX_TABLE_IP | (idx << 3);
	uint32_t addr[4] = {0};

	if (enable)
		reg_val |= IPSRXIDX_RX_EN;

	if (ip->type == IPv4)
		/* only write last 4 bytes */
		addr[3] = ip->ipv4;
	else
		memcpy(addr, ip->ipv6, sizeof(addr));

	IXGBE_WRITE_REG(hw, IXGBE_IPSRXIPADDR(0), addr[0]);
	IXGBE_WRITE_REG(hw, IXGBE_IPSRXIPADDR(1), addr[1]);
	IXGBE_WRITE_REG(hw, IXGBE_IPSRXIPADDR(2), addr[2]);
	IXGBE_WRITE_REG(hw, IXGBE_IPSRXIPADDR(3), addr[3]);
	IXGBE_WAIT_RWRITE;
}

static inline void
ixgbe_crypto_write_rx_spi(struct ixgbe_hw *hw, uint32_t idx,
		uint32_t spi, uint32_t ip_idx, bool enable)
{
	uint32_t reg_val = IPSRXIDX_WRITE | IPSRXIDX_TABLE_SPI | (idx << 3);

	if (enable)
		reg_val |= IPSRXIDX_RX_EN;

	IXGBE_WRITE_REG(hw, IXGBE_IPSRXSPI, rte_cpu_to_be_32(spi));
	IXGBE_WRITE_REG(hw, IXGBE_IPSRXIPIDX, ip_idx);
	IXGBE_WAIT_RWRITE;
}

static inline void
ixgbe_crypto_write_rx_key(struct ixgbe_hw *hw, uint32_t idx,
		const uint8_t *key, uint32_t salt, uint32_t mode, bool enable)
{
	uint32_t reg_val = IPSRXIDX_WRITE | IPSRXIDX_TABLE_KEY | (idx << 3);

	if (enable)
		reg_val |= IPSRXIDX_RX_EN;

	IXGBE_WRITE_REG(hw, IXGBE_IPSRXKEY(0),
		rte_cpu_to_be_32(*(const uint32_t *)&key[12]));
	IXGBE_WRITE_REG(hw, IXGBE_IPSRXKEY(1),
		rte_cpu_to_be_32(*(const uint32_t *)&key[8]));
	IXGBE_WRITE_REG(hw, IXGBE_IPSRXKEY(2),
		rte_cpu_to_be_32(*(const uint32_t *)&key[4]));
	IXGBE_WRITE_REG(hw, IXGBE_IPSRXKEY(3),
		rte_cpu_to_be_32(*(const uint32_t *)&key[0]));
	IXGBE_WRITE_REG(hw, IXGBE_IPSRXSALT, rte_cpu_to_be_32(salt));
	IXGBE_WRITE_REG(hw, IXGBE_IPSRXMOD, mode);
	IXGBE_WAIT_RWRITE;
}

static inline void
ixgbe_crypto_write_tx_key(struct ixgbe_hw *hw, uint32_t idx,
		const uint8_t *key, uint32_t salt, bool enable)
{
	uint32_t reg_val = IPSRXIDX_WRITE | (idx << 3);

	if (enable)
		reg_val |= IPSRXIDX_TX_EN;

	IXGBE_WRITE_REG(hw, IXGBE_IPSTXKEY(0),
		rte_cpu_to_be_32(*(const uint32_t *)&key[12]));
	IXGBE_WRITE_REG(hw, IXGBE_IPSTXKEY(1),
		rte_cpu_to_be_32(*(const uint32_t *)&key[8]));
	IXGBE_WRITE_REG(hw, IXGBE_IPSTXKEY(2),
		rte_cpu_to_be_32(*(const uint32_t *)&key[4]));
	IXGBE_WRITE_REG(hw, IXGBE_IPSTXKEY(3),
		rte_cpu_to_be_32(*(const uint32_t *)&key[0]));
	IXGBE_WRITE_REG(hw, IXGBE_IPSTXSALT, rte_cpu_to_be_32(salt));
	IXGBE_WAIT_TWRITE;
}

static void
ixgbe_crypto_clear_ipsec_tables(struct rte_eth_dev *dev)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ixgbe_ipsec *priv = IXGBE_DEV_PRIVATE_TO_IPSEC(
				dev->data->dev_private);
	const struct ipaddr ip = {0};
	const uint8_t key[16] = {0};
	int i = 0;

	/* clear Rx IP table*/
	for (i = 0; i < IPSEC_MAX_RX_IP_COUNT; i++)
		ixgbe_crypto_write_rx_ip(hw, i, &ip, false);

	/* clear Rx SPI and Rx/Tx SA tables*/
	for (i = 0; i < IPSEC_MAX_SA_COUNT; i++) {
		ixgbe_crypto_write_rx_spi(hw, i, 0, 0, false);
		ixgbe_crypto_write_rx_key(hw, i, key, 0, 0, false);
		ixgbe_crypto_write_tx_key(hw, i, key, 0, false);
	}

	memset(priv->rx_ip_tbl, 0, sizeof(priv->rx_ip_tbl));
	memset(priv->rx_sa_tbl, 0, sizeof(priv->rx_sa_tbl));
	memset(priv->tx_sa_tbl, 0, sizeof(priv->tx_sa_tbl));
}

static int
ixgbe_crypto_add_sa(struct ixgbe_crypto_session *ic_session)
{
	struct rte_eth_dev_data *dev_data = ic_session->dev_data;
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev_data->dev_private);
	struct ixgbe_ipsec *priv = IXGBE_DEV_PRIVATE_TO_IPSEC(dev_data->dev_private);
	int i, sa_index = -1;
	uint8_t key[16] = {0};

	if (ic_session->op == IXGBE_OP_AUTHENTICATED_DECRYPTION) {
		struct ixgbe_crypto_rx_ip_table *rxip;
		struct ixgbe_crypto_rx_sa_table *rxsa;
		int ip_index = -1, free_index = -1;

		/* Find a match in the IP table*/
		for (i = 0; i < IPSEC_MAX_RX_IP_COUNT; i++) {
			if (CMP_IP(priv->rx_ip_tbl[i].ip,
				   ic_session->dst_ip)) {
				ip_index = i;
				break;
			}
			if (free_index == -1 && priv->rx_ip_tbl[i].ref_count == 0)
				free_index = i;
		}
		/* If no match, find a free entry in the IP table*/
		if (ip_index < 0)
			ip_index = free_index;

		/* Fail if no match and no free entries*/
		if (ip_index < 0) {
			PMD_DRV_LOG(ERR, "No free entry left in the Rx IP table");
			return -ENOSPC;
		}
		rxip = &priv->rx_ip_tbl[ip_index];

		/* Find a free entry in the SA table*/
		for (i = 0; i < IPSEC_MAX_SA_COUNT; i++) {
			if (priv->rx_sa_tbl[i].used == 0) {
				sa_index = i;
				break;
			}
		}
		/* Fail if no free entries*/
		if (sa_index < 0) {
			PMD_DRV_LOG(ERR, "No free entry left in the Rx SA table");
			return -ENOSPC;
		}
		rxsa = &priv->rx_sa_tbl[sa_index];

		rxip->ref_count++;
		memcpy(&rxip->ip, &ic_session->dst_ip, sizeof(rxip->ip));

		rxsa->spi = ic_session->spi;
		rxsa->ip_index = ip_index;
		rxsa->mode = IPSRXMOD_VALID | IPSRXMOD_PROTO | IPSRXMOD_DECRYPT;
		if (ic_session->dst_ip.type == IPv6)
			rxsa->mode |= IPSRXMOD_IPV6;

		rxsa->used = 1;

		/* write IP table entry*/
		ixgbe_crypto_write_rx_ip(hw, ip_index, &rxip->ip, true);

		/* write SPI table entry*/
		ixgbe_crypto_write_rx_spi(hw, sa_index, rxsa->spi, ip_index, true);

		/* write Key table entry*/
		memcpy(key, ic_session->key, ic_session->key_len);

		ixgbe_crypto_write_rx_key(hw, sa_index, key,
				ic_session->salt, rxsa->mode, true);

		rte_memzero_explicit(key, sizeof(key));

	} else { /* sess->dir == RTE_CRYPTO_OUTBOUND */
		struct ixgbe_crypto_tx_sa_table *txsa;

		/* Find a free entry in the SA table*/
		for (i = 0; i < IPSEC_MAX_SA_COUNT; i++) {
			if (priv->tx_sa_tbl[i].used == 0) {
				sa_index = i;
				break;
			}
		}
		/* Fail if no free entries*/
		if (sa_index < 0) {
			PMD_DRV_LOG(ERR, "No free entry left in the Tx SA table");
			return -ENOSPC;
		}
		txsa = &priv->tx_sa_tbl[sa_index];

		txsa->spi = ic_session->spi;
		txsa->used = 1;
		ic_session->sa_index = sa_index;

		memcpy(key, ic_session->key, ic_session->key_len);

		/* write Key table entry*/
		ixgbe_crypto_write_tx_key(hw, sa_index, key, ic_session->salt, true);

		rte_memzero_explicit(key, sizeof(key));
	}

	return 0;
}

static int
ixgbe_crypto_remove_sa(struct ixgbe_crypto_session *ic_session)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(ic_session->dev_data->dev_private);
	struct ixgbe_ipsec *priv =
			IXGBE_DEV_PRIVATE_TO_IPSEC(ic_session->dev_data->dev_private);
	const uint8_t key[16] = {0};
	int i, sa_index = -1;

	if (ic_session->op == IXGBE_OP_AUTHENTICATED_DECRYPTION) {
		struct ixgbe_crypto_rx_ip_table *rxip;
		struct ixgbe_crypto_rx_sa_table *rxsa;
		int ip_index = -1;

		/* Find a match in the IP table*/
		for (i = 0; i < IPSEC_MAX_RX_IP_COUNT; i++) {
			if (CMP_IP(priv->rx_ip_tbl[i].ip, ic_session->dst_ip)) {
				ip_index = i;
				break;
			}
		}

		/* Fail if no match*/
		if (ip_index < 0) {
			PMD_DRV_LOG(ERR, "Entry not found in the Rx IP table");
			return -ENOENT;
		}
		rxip = &priv->rx_ip_tbl[ip_index];

		/* Find a free entry in the SA table*/
		for (i = 0; i < IPSEC_MAX_SA_COUNT; i++) {
			if (priv->rx_sa_tbl[i].spi == ic_session->spi) {
				sa_index = i;
				break;
			}
		}
		/* Fail if no match*/
		if (sa_index < 0) {
			PMD_DRV_LOG(ERR, "Entry not found in the Rx SA table");
			return -ENOENT;
		}
		rxsa = &priv->rx_sa_tbl[sa_index];

		/* Disable and clear Rx SPI and key table entries*/
		ixgbe_crypto_write_rx_spi(hw, sa_index, 0, 0, false);
		ixgbe_crypto_write_rx_key(hw, sa_index, key, 0, 0, false);

		/* Clear the SA table entry*/
		*rxsa = (struct ixgbe_crypto_rx_sa_table){0};

		/* If last used then clear the IP table entry*/
		rxip->ref_count--;
		if (rxip->ref_count == 0) {
			const struct ipaddr ip = {0};
			ixgbe_crypto_write_rx_ip(hw, ip_index, &ip, false);
			*rxip = (struct ixgbe_crypto_rx_ip_table){0};
		}
	} else { /* session->dir == RTE_CRYPTO_OUTBOUND */
		struct ixgbe_crypto_tx_sa_table *txsa;

		/* Find a match in the SA table*/
		for (i = 0; i < IPSEC_MAX_SA_COUNT; i++) {
			if (priv->tx_sa_tbl[i].spi == ic_session->spi) {
				sa_index = i;
				break;
			}
		}
		/* Fail if no match entries*/
		if (sa_index < 0) {
			PMD_DRV_LOG(ERR, "Entry not found in the Tx SA table");
			return -ENOENT;
		}
		txsa = &priv->tx_sa_tbl[sa_index];

		ixgbe_crypto_write_tx_key(hw, sa_index, key, 0, false);
		*txsa = (struct ixgbe_crypto_tx_sa_table){0};
	}

	return 0;
}

static int
ixgbe_crypto_create_session(void *device,
		struct rte_security_session_conf *conf,
		struct rte_security_session *session)
{
	struct rte_eth_dev *eth_dev = (struct rte_eth_dev *)device;
	struct ixgbe_crypto_session *ic_session = SECURITY_GET_SESS_PRIV(session);
	struct rte_crypto_aead_xform *aead_xform;
	struct rte_eth_conf *dev_conf = &eth_dev->data->dev_conf;

	if (conf->crypto_xform->type != RTE_CRYPTO_SYM_XFORM_AEAD ||
			conf->crypto_xform->aead.algo !=
					RTE_CRYPTO_AEAD_AES_GCM) {
		PMD_DRV_LOG(ERR, "Unsupported crypto transformation mode");
		return -ENOTSUP;
	}
	aead_xform = &conf->crypto_xform->aead;

	/* Only 16-byte keys are supported. */
	if (aead_xform->key.length != 16) {
		PMD_DRV_LOG(ERR, "Unsupported key length %u", aead_xform->key.length);
		return -ENOTSUP;
	}

	if (conf->ipsec.direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS) {
		if (dev_conf->rxmode.offloads & RTE_ETH_RX_OFFLOAD_SECURITY) {
			ic_session->op = IXGBE_OP_AUTHENTICATED_DECRYPTION;
		} else {
			PMD_DRV_LOG(ERR, "IPsec decryption not enabled");
			return -ENOTSUP;
		}
	} else {
		if (dev_conf->txmode.offloads & RTE_ETH_TX_OFFLOAD_SECURITY) {
			ic_session->op = IXGBE_OP_AUTHENTICATED_ENCRYPTION;
		} else {
			PMD_DRV_LOG(ERR, "IPsec encryption not enabled");
			return -ENOTSUP;
		}
	}

	ic_session->key = aead_xform->key.data;
	ic_session->key_len = aead_xform->key.length;
	memcpy(&ic_session->salt,
	       &aead_xform->key.data[aead_xform->key.length], 4);
	ic_session->spi = conf->ipsec.spi;
	ic_session->dev_data = eth_dev->data;

	if (ic_session->op == IXGBE_OP_AUTHENTICATED_ENCRYPTION) {
		if (ixgbe_crypto_add_sa(ic_session)) {
			PMD_DRV_LOG(ERR, "Failed to add SA");
			return -EPERM;
		}
	}

	return 0;
}

static unsigned int
ixgbe_crypto_session_get_size(__rte_unused void *device)
{
	return sizeof(struct ixgbe_crypto_session);
}

static int
ixgbe_crypto_remove_session(void *device,
		struct rte_security_session *session)
{
	struct rte_eth_dev *eth_dev = device;
	struct ixgbe_crypto_session *ic_session = SECURITY_GET_SESS_PRIV(session);

	if (eth_dev->data != ic_session->dev_data) {
		PMD_DRV_LOG(ERR, "Session not bound to this device");
		return -ENODEV;
	}

	if (ixgbe_crypto_remove_sa(ic_session)) {
		PMD_DRV_LOG(ERR, "Failed to remove session");
		return -EFAULT;
	}

	memset(ic_session, 0, sizeof(struct ixgbe_crypto_session));
	return 0;
}

static inline uint8_t
ixgbe_crypto_compute_pad_len(struct rte_mbuf *m)
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
ixgbe_crypto_update_mb(void *device __rte_unused,
		struct rte_security_session *session,
		       struct rte_mbuf *m, void *params __rte_unused)
{
	struct ixgbe_crypto_session *ic_session = SECURITY_GET_SESS_PRIV(session);
	if (ic_session->op == IXGBE_OP_AUTHENTICATED_ENCRYPTION) {
		union ixgbe_crypto_tx_desc_md *mdata =
			(union ixgbe_crypto_tx_desc_md *)
				rte_security_dynfield(m);
		mdata->enc = 1;
		mdata->sa_idx = ic_session->sa_index;
		mdata->pad_len = ixgbe_crypto_compute_pad_len(m);
	}
	return 0;
}


static const struct rte_security_capability *
ixgbe_crypto_capabilities_get(void *device __rte_unused)
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
	ixgbe_security_capabilities[] = {
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

	return ixgbe_security_capabilities;
}


int
ixgbe_crypto_enable_ipsec(struct rte_eth_dev *dev)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
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


	/* Set IXGBE_SECTXBUFFAF to 0x15 as required in the datasheet*/
	IXGBE_WRITE_REG(hw, IXGBE_SECTXBUFFAF, 0x15);

	/* IFG needs to be set to 3 when we are using security. Otherwise a Tx
	 * hang will occur with heavy traffic.
	 */
	reg = IXGBE_READ_REG(hw, IXGBE_SECTXMINIFG);
	reg = (reg & 0xFFFFFFF0) | 0x3;
	IXGBE_WRITE_REG(hw, IXGBE_SECTXMINIFG, reg);

	reg  = IXGBE_READ_REG(hw, IXGBE_HLREG0);
	reg |= IXGBE_HLREG0_TXCRCEN | IXGBE_HLREG0_RXCRCSTRP;
	IXGBE_WRITE_REG(hw, IXGBE_HLREG0, reg);

	if (rx_offloads & RTE_ETH_RX_OFFLOAD_SECURITY) {
		IXGBE_WRITE_REG(hw, IXGBE_SECRXCTRL, 0);
		reg = IXGBE_READ_REG(hw, IXGBE_SECRXCTRL);
		if (reg != 0) {
			PMD_DRV_LOG(ERR, "Error enabling Rx Crypto");
			return -1;
		}
	}
	if (tx_offloads & RTE_ETH_TX_OFFLOAD_SECURITY) {
		IXGBE_WRITE_REG(hw, IXGBE_SECTXCTRL,
				IXGBE_SECTXCTRL_STORE_FORWARD);
		reg = IXGBE_READ_REG(hw, IXGBE_SECTXCTRL);
		if (reg != IXGBE_SECTXCTRL_STORE_FORWARD) {
			PMD_DRV_LOG(ERR, "Error enabling Rx Crypto");
			return -1;
		}
	}

	ixgbe_crypto_clear_ipsec_tables(dev);

	return 0;
}

int
ixgbe_crypto_add_ingress_sa_from_flow(struct rte_security_session *sess,
		const struct ip_spec *spec)
{
	struct ixgbe_crypto_session *ic_session = SECURITY_GET_SESS_PRIV(sess);

	if (ic_session->op == IXGBE_OP_AUTHENTICATED_DECRYPTION) {
		if (spec->is_ipv6) {
			const struct rte_flow_item_ipv6 *ipv6 = &spec->spec.ipv6;
			ic_session->src_ip.type = IPv6;
			ic_session->dst_ip.type = IPv6;
			memcpy(ic_session->src_ip.ipv6,
				   &ipv6->hdr.src_addr, 16);
			memcpy(ic_session->dst_ip.ipv6,
				   &ipv6->hdr.dst_addr, 16);
		} else {
			const struct rte_flow_item_ipv4 *ipv4 = &spec->spec.ipv4;
			ic_session->src_ip.type = IPv4;
			ic_session->dst_ip.type = IPv4;
			ic_session->src_ip.ipv4 = ipv4->hdr.src_addr;
			ic_session->dst_ip.ipv4 = ipv4->hdr.dst_addr;
		}
		return ixgbe_crypto_add_sa(ic_session);
	}

	return 0;
}

static struct rte_security_ops ixgbe_security_ops = {
	.session_create = ixgbe_crypto_create_session,
	.session_update = NULL,
	.session_get_size = ixgbe_crypto_session_get_size,
	.session_stats_get = NULL,
	.session_destroy = ixgbe_crypto_remove_session,
	.set_pkt_metadata = ixgbe_crypto_update_mb,
	.capabilities_get = ixgbe_crypto_capabilities_get
};

static int
ixgbe_crypto_capable(struct rte_eth_dev *dev)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t reg_i, reg, capable = 1;
	/* test if rx crypto can be enabled and then write back initial value*/
	reg_i = IXGBE_READ_REG(hw, IXGBE_SECRXCTRL);
	IXGBE_WRITE_REG(hw, IXGBE_SECRXCTRL, 0);
	reg = IXGBE_READ_REG(hw, IXGBE_SECRXCTRL);
	if (reg != 0)
		capable = 0;
	IXGBE_WRITE_REG(hw, IXGBE_SECRXCTRL, reg_i);
	return capable;
}

int
ixgbe_ipsec_ctx_create(struct rte_eth_dev *dev)
{
	struct rte_security_ctx *ctx = NULL;

	if (ixgbe_crypto_capable(dev)) {
		ctx = rte_malloc("rte_security_instances_ops",
				 sizeof(struct rte_security_ctx), 0);
		if (ctx) {
			ctx->device = (void *)dev;
			ctx->ops = &ixgbe_security_ops;
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
