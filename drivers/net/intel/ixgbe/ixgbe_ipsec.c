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

		priv->tx_sa_tbl[i] = (struct ixgbe_crypto_tx_sa_table){0};
	}
}

void
ixgbe_crypto_install_rx_sa(struct rte_eth_dev_data *dev_data,
		const struct ixgbe_crypto_rx_sa *sa)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev_data->dev_private);
	uint8_t key[16];

	ixgbe_crypto_write_rx_ip(hw, sa->ip_idx, &sa->dst_ip, true);
	ixgbe_crypto_write_rx_spi(hw, sa->sa_idx, sa->spi, sa->ip_idx, true);
	memcpy(key, sa->key, sizeof(key));
	ixgbe_crypto_write_rx_key(hw, sa->sa_idx, (const uint8_t *)key,
			sa->salt, sa->mode, true);
	rte_memzero_explicit(key, sizeof(key));
}

void
ixgbe_crypto_uninstall_rx_sa(struct rte_eth_dev_data *dev_data,
		const struct ixgbe_crypto_rx_sa *sa, bool clear_ip)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev_data->dev_private);
	const uint8_t key[16] = {0};

	ixgbe_crypto_write_rx_spi(hw, sa->sa_idx, 0, 0, false);
	ixgbe_crypto_write_rx_key(hw, sa->sa_idx, key, 0, 0, false);
	if (clear_ip) {
		const struct ipaddr ip = {0};

		ixgbe_crypto_write_rx_ip(hw, sa->ip_idx, &ip, false);
	}
}

static int
ixgbe_crypto_add_tx_sa(struct ixgbe_crypto_session *ic_session)
{
	struct rte_eth_dev_data *dev_data = ic_session->dev_data;
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev_data->dev_private);
	struct ixgbe_ipsec *priv = IXGBE_DEV_PRIVATE_TO_IPSEC(dev_data->dev_private);
	struct ixgbe_crypto_tx_sa_table *txsa;
	uint8_t key[16];
	int sa_index = -1;
	int i;

	for (i = 0; i < IPSEC_MAX_SA_COUNT; i++) {
		if (priv->tx_sa_tbl[i].used == 0) {
			sa_index = i;
			break;
		}
	}
	if (sa_index < 0) {
		PMD_DRV_LOG(ERR, "No free entry left in the Tx SA table");
		return -ENOSPC;
	}
	txsa = &priv->tx_sa_tbl[sa_index];
	txsa->spi = ic_session->spi;
	txsa->used = 1;
	ic_session->sa_index = sa_index;

	memcpy(key, ic_session->key, ic_session->key_len);
	ixgbe_crypto_write_tx_key(hw, sa_index, (const uint8_t *)key,
			ic_session->salt, true);
	rte_memzero_explicit(key, sizeof(key));

	return 0;
}

static int
ixgbe_crypto_remove_tx_sa(struct ixgbe_crypto_session *ic_session)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(ic_session->dev_data->dev_private);
	struct ixgbe_ipsec *priv =
			IXGBE_DEV_PRIVATE_TO_IPSEC(ic_session->dev_data->dev_private);
	struct ixgbe_crypto_tx_sa_table *txsa;
	const uint8_t key[16] = {0};

	if (ic_session->sa_index >= IPSEC_MAX_SA_COUNT)
		return -ENOENT;
	txsa = &priv->tx_sa_tbl[ic_session->sa_index];
	if (txsa->used == 0 || txsa->spi != ic_session->spi)
		return -ENOENT;

	ixgbe_crypto_write_tx_key(hw, ic_session->sa_index, key, 0, false);
	*txsa = (struct ixgbe_crypto_tx_sa_table){0};

	return 0;
}

static int
ixgbe_crypto_session_state_acquire(struct ixgbe_crypto_session *ic_session)
{
	uint16_t expected;

	expected = rte_atomic_load_explicit(&ic_session->refcnt,
			rte_memory_order_acquire);
	while (expected != IXGBE_SECURITY_SESSION_DESTROYING) {
		if (expected >= IXGBE_SECURITY_SESSION_REFCNT_MAX)
			return -ENOSPC;
		if (rte_atomic_compare_exchange_strong_explicit(&ic_session->refcnt,
				&expected, expected + 1,
				rte_memory_order_acq_rel, rte_memory_order_acquire))
			return 0;
	}

	return -EBUSY;
}

static int
ixgbe_crypto_session_state_release(struct ixgbe_crypto_session *ic_session)
{
	uint16_t expected;

	expected = rte_atomic_load_explicit(&ic_session->refcnt,
			rte_memory_order_acquire);
	while (expected != IXGBE_SECURITY_SESSION_DESTROYING) {
		if (expected == IXGBE_SECURITY_SESSION_UNBOUND)
			return -EINVAL;
		if (rte_atomic_compare_exchange_strong_explicit(&ic_session->refcnt,
				&expected, expected - 1,
				rte_memory_order_release, rte_memory_order_relaxed))
			return 0;
	}

	return -EINVAL;
}

int
ixgbe_crypto_session_acquire(struct rte_security_session *session)
{
	struct ixgbe_crypto_session *ic_session = SECURITY_GET_SESS_PRIV(session);

	return ixgbe_crypto_session_state_acquire(ic_session);
}

int
ixgbe_crypto_session_release(struct rte_security_session *session)
{
	struct ixgbe_crypto_session *ic_session = SECURITY_GET_SESS_PRIV(session);

	return ixgbe_crypto_session_state_release(ic_session);
}

static int
ixgbe_crypto_session_disable(struct rte_security_session *session)
{
	struct ixgbe_crypto_session *ic_session = SECURITY_GET_SESS_PRIV(session);
	uint16_t expected;

	/*
	 * Rx sessions must have no flow references before removal. Tx sessions
	 * retain the reference acquired when their SA was allocated.
	 */
	expected = ic_session->op == IXGBE_OP_AUTHENTICATED_DECRYPTION ?
			IXGBE_SECURITY_SESSION_UNBOUND : 1;
	if (!rte_atomic_compare_exchange_strong_explicit(&ic_session->refcnt,
			&expected, IXGBE_SECURITY_SESSION_DESTROYING,
			rte_memory_order_acq_rel, rte_memory_order_acquire))
		return -EBUSY;
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
	int ret;

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

	memcpy(ic_session->key, aead_xform->key.data, sizeof(ic_session->key));
	ic_session->key_len = aead_xform->key.length;
	memcpy(&ic_session->salt,
	       &aead_xform->key.data[aead_xform->key.length], 4);
	ic_session->spi = conf->ipsec.spi;
	ic_session->dev_data = eth_dev->data;

	rte_atomic_store_explicit(&ic_session->refcnt,
			IXGBE_SECURITY_SESSION_UNBOUND,
			rte_memory_order_relaxed);

	/* only handle tx, as rx sessions are created by rte_flow */
	if (ic_session->op == IXGBE_OP_AUTHENTICATED_ENCRYPTION) {
		if (ixgbe_crypto_add_tx_sa(ic_session)) {
			PMD_DRV_LOG(ERR, "Failed to add SA");
			rte_memzero_explicit(ic_session, sizeof(*ic_session));
			return -EPERM;
		}
		ret = ixgbe_crypto_session_state_acquire(ic_session);
		if (ret != 0) {
			ixgbe_crypto_remove_tx_sa(ic_session);
			rte_memzero_explicit(ic_session, sizeof(*ic_session));
			return ret;
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
	int ret;

	if (eth_dev->data != ic_session->dev_data) {
		PMD_DRV_LOG(ERR, "Session not bound to this device");
		return -ENODEV;
	}

	if (ixgbe_crypto_session_disable(session) < 0) {
		PMD_DRV_LOG(ERR, "Session is still in use");
		return -EBUSY;
	}

	if (ic_session->op == IXGBE_OP_AUTHENTICATED_ENCRYPTION) {
		ret = ixgbe_crypto_remove_tx_sa(ic_session);
		if (ret != 0) {
			PMD_DRV_LOG(ERR, "Failed to remove session");
			/* set refcnt back to 1 to re-enable the session */
			rte_atomic_store_explicit(&ic_session->refcnt, 1,
					rte_memory_order_release);
			return ret;
		}
	}

	rte_memzero_explicit(ic_session, sizeof(*ic_session));
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
