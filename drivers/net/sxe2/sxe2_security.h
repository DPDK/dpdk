/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#ifndef __SXE2_SECURITY_H__
#define __SXE2_SECURITY_H__

#include <rte_security.h>
#include <rte_cryptodev.h>
#include <rte_security_driver.h>

#include "sxe2_ipsec.h"

#define SXE2_DEV_TO_SECURITY(eth) \
	((struct rte_security_ctx *)(((struct rte_eth_dev *)eth)->security_ctx))

#define SXE2_RTE_CRYPTO_CIPHER_AES_CBC   (RTE_CRYPTO_CIPHER_AES_CBC)

#define SXE2_RTE_RTE_CRYPTO_CIPHER_SM4_CBC   (RTE_CRYPTO_CIPHER_SM4_CBC)

#define SXE2_RTE_CRYPTO_AUTH_SHA256_HMAC  (RTE_CRYPTO_AUTH_SHA256_HMAC)

#define SXE2_RTE_CRYPTO_AUTH_SM3_HMAC   (RTE_CRYPTO_AUTH_SM3_HMAC)

enum sxe2_security_protocol {
	SXE2_SECURITY_PROTOCOL_IPSEC       = 0,
	SXE2_SECURITY_PROTOCOL_MAX         = 1,
};

enum sxe2_security_xform {
	SXE2_SECURITY_IPSEC_EN       = 0,
	SXE2_SECURITY_IPSEC_DE       = 1,
	SXE2_SECURITY_NUM_MAX        = 2,
};

enum sxe2_security_block_size {
	SXE2_SECURITY_BLOCK_SIZE_16        = 16,
	SXE2_SECURITY_BLOCK_SIZE_64        = 64,
};

struct sxe2_security_ipsec_caps {
	enum rte_security_ipsec_sa_protocol   proto;
	enum rte_security_ipsec_sa_mode       mode;
	struct rte_security_ipsec_sa_options  options;
};

struct sxe2_security_capabilities {
	struct rte_cryptodev_capabilities     *crypto_capabilities;
	enum rte_security_session_action_type action;
	struct sxe2_security_ipsec_caps      ipsec;
};

struct sxe2_security_session {
	struct sxe2_adapter                   *adapter;
	struct sxe2_ipsec_pkt_metadata        pkt_metadata_template;
	struct sxe2_ipsec_security_sa         sa;
	struct sxe2_ipsec_esn                 esn;
	struct sxe2_ipsec_udp                 udp_cap;
	enum rte_security_session_protocol     protocol;
	enum rte_security_ipsec_sa_direction   direction;
	enum rte_security_ipsec_sa_mode        mode;
	enum rte_security_ipsec_sa_protocol    sa_proto;
	enum rte_security_ipsec_tunnel_type    type;
};

struct sxe2_security_ctx {
	struct sxe2_adapter                 *adapter;
	struct sxe2_security_capabilities   sxe2_capabilities[SXE2_SECURITY_PROTOCOL_MAX];
	struct sxe2_ipsec_ctx               ipsec_ctx;
	rte_spinlock_t                       security_lock;
};

int32_t sxe2_security_init(struct rte_eth_dev *dev);

void sxe2_security_uinit(struct rte_eth_dev *dev);

#endif /* __SXE2_SECURITY_H__ */
