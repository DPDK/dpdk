/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef _NITROX_SYM_CTX_H_
#define _NITROX_SYM_CTX_H_

#include <stdbool.h>

#include <rte_crypto.h>

#define AES_MAX_KEY_SIZE 32
#define AES_BLOCK_SIZE 16
#define AES_GCM_SALT_SIZE 4

enum nitrox_chain {
	NITROX_CHAIN_CIPHER_ONLY,
	NITROX_CHAIN_CIPHER_AUTH,
	NITROX_CHAIN_AUTH_CIPHER,
	NITROX_CHAIN_COMBINED,
	NITROX_CHAIN_NOT_SUPPORTED
};

enum nitrox_op {
	NITROX_OP_ENCRYPT,
	NITROX_OP_DECRYPT,
};

struct crypto_keys {
	uint8_t key[AES_MAX_KEY_SIZE];
	uint8_t iv[AES_BLOCK_SIZE];
};

struct auth_keys {
	uint8_t ipad[64];
	uint8_t opad[64];
};

struct flexi_crypto_context {
	union {
		uint64_t flags;
		struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
			uint64_t cipher_type : 4;
			uint64_t reserved_59 : 1;
			uint64_t aes_keylen : 2;
			uint64_t iv_source : 1;
			uint64_t hash_type : 4;
			uint64_t reserved_49_51 : 3;
			uint64_t auth_input_type : 1;
			uint64_t mac_len : 8;
			uint64_t reserved_0_39 : 40;
#else
			uint64_t reserved_0_39 : 40;
			uint64_t mac_len : 8;
			uint64_t auth_input_type : 1;
			uint64_t reserved_49_51 : 3;
			uint64_t hash_type : 4;
			uint64_t iv_source : 1;
			uint64_t aes_keylen : 2;
			uint64_t reserved_59 : 1;
			uint64_t cipher_type : 4;
#endif
		} w0;
	};
	struct crypto_keys crypto;
	struct auth_keys auth;
};

struct nitrox_crypto_ctx {
	struct flexi_crypto_context fctx;
	enum nitrox_chain nitrox_chain;
	enum rte_crypto_aead_algorithm aead_algo;
	struct {
		uint16_t offset;
		uint16_t length;
	} iv;
	rte_iova_t iova;
	uint8_t salt[AES_GCM_SALT_SIZE];
	uint16_t digest_length;
	uint16_t aad_length;
	uint8_t opcode;
	uint8_t req_op;
};

#endif /* _NITROX_SYM_CTX_H_ */
