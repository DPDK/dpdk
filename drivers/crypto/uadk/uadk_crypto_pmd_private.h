/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2022-2023 Huawei Technologies Co.,Ltd. All rights reserved.
 * Copyright 2022-2023 Linaro ltd.
 */

#ifndef _UADK_CRYPTO_PMD_PRIVATE_H_
#define _UADK_CRYPTO_PMD_PRIVATE_H_

/* Maximum length for digest (SHA-512 needs 64 bytes) */
#define DIGEST_LENGTH_MAX 64

struct __rte_cache_aligned uadk_qp {
	/* Ring for placing process packets */
	struct rte_ring *processed_pkts;
	/* Queue pair statistics */
	struct rte_cryptodev_stats qp_stats;
	/* Queue Pair Identifier */
	uint16_t id;
	/* Unique Queue Pair Name */
	char name[RTE_CRYPTODEV_NAME_MAX_LEN];
	/* Buffer used to store the digest generated
	 * by the driver when verifying a digest provided
	 * by the user (using authentication verify operation)
	 */
	uint8_t temp_digest[DIGEST_LENGTH_MAX];
};

enum uadk_chain_order {
	UADK_CHAIN_ONLY_CIPHER,
	UADK_CHAIN_ONLY_AUTH,
	UADK_CHAIN_CIPHER_AUTH,
	UADK_CHAIN_AUTH_CIPHER,
	UADK_CHAIN_NOT_SUPPORTED
};

struct __rte_cache_aligned uadk_crypto_session {
	handle_t handle_cipher;
	handle_t handle_digest;
	enum uadk_chain_order chain_order;

	/* IV parameters */
	struct {
		uint16_t length;
		uint16_t offset;
	} iv;

	/* Cipher Parameters */
	struct {
		enum rte_crypto_cipher_operation direction;
		struct wd_cipher_req req;
	} cipher;

	/* Authentication Parameters */
	struct {
		struct wd_digest_req req;
		enum rte_crypto_auth_operation operation;
		uint16_t digest_length;
	} auth;
};

enum uadk_crypto_version {
	UADK_CRYPTO_V2,
	UADK_CRYPTO_V3,
};

struct __rte_cache_aligned uadk_crypto_priv {
	bool env_cipher_init;
	bool env_auth_init;
	enum uadk_crypto_version version;
	unsigned int max_nb_qpairs;
};

extern int uadk_crypto_logtype;
#define RTE_LOGTYPE_UADK_CRYPTO uadk_crypto_logtype

#define UADK_LOG(level, ...)  \
	RTE_LOG_LINE_PREFIX(level, UADK_CRYPTO, "%s() line %u: ", \
		__func__ RTE_LOG_COMMA __LINE__, __VA_ARGS__)

#endif /* _UADK_CRYPTO_PMD_PRIVATE_H_ */
