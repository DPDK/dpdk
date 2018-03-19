/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright(c) 2018 Advanced Micro Devices, Inc. All rights reserved.
 */

#ifndef _CCP_CRYPTO_H_
#define _CCP_CRYPTO_H_

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <rte_atomic.h>
#include <rte_byteorder.h>
#include <rte_io.h>
#include <rte_pci.h>
#include <rte_spinlock.h>
#include <rte_crypto_sym.h>
#include <rte_cryptodev.h>

#include "ccp_dev.h"

#define CCP_SHA3_CTX_SIZE 200
/**
 * CCP supported AES modes
 */
enum ccp_aes_mode {
	CCP_AES_MODE_ECB = 0,
	CCP_AES_MODE_CBC,
	CCP_AES_MODE_OFB,
	CCP_AES_MODE_CFB,
	CCP_AES_MODE_CTR,
	CCP_AES_MODE_CMAC,
	CCP_AES_MODE_GHASH,
	CCP_AES_MODE_GCTR,
	CCP_AES_MODE__LAST,
};

/**
 * CCP AES GHASH mode
 */
enum ccp_aes_ghash_mode {
	CCP_AES_MODE_GHASH_AAD = 0,
	CCP_AES_MODE_GHASH_FINAL
};

/**
 * CCP supported AES types
 */
enum ccp_aes_type {
	CCP_AES_TYPE_128 = 0,
	CCP_AES_TYPE_192,
	CCP_AES_TYPE_256,
	CCP_AES_TYPE__LAST,
};

/***** 3DES engine *****/

/**
 * CCP supported DES/3DES modes
 */
enum ccp_des_mode {
	CCP_DES_MODE_ECB = 0, /* Not supported */
	CCP_DES_MODE_CBC,
	CCP_DES_MODE_CFB,
};

/**
 * CCP supported DES types
 */
enum ccp_des_type {
	CCP_DES_TYPE_128 = 0,	/* 112 + 16 parity */
	CCP_DES_TYPE_192,	/* 168 + 24 parity */
	CCP_DES_TYPE__LAST,
};

/***** SHA engine *****/

/**
 * ccp_sha_type - type of SHA operation
 *
 * @CCP_SHA_TYPE_1: SHA-1 operation
 * @CCP_SHA_TYPE_224: SHA-224 operation
 * @CCP_SHA_TYPE_256: SHA-256 operation
 */
enum ccp_sha_type {
	CCP_SHA_TYPE_1 = 1,
	CCP_SHA_TYPE_224,
	CCP_SHA_TYPE_256,
	CCP_SHA_TYPE_384,
	CCP_SHA_TYPE_512,
	CCP_SHA_TYPE_RSVD1,
	CCP_SHA_TYPE_RSVD2,
	CCP_SHA3_TYPE_224,
	CCP_SHA3_TYPE_256,
	CCP_SHA3_TYPE_384,
	CCP_SHA3_TYPE_512,
	CCP_SHA_TYPE__LAST,
};

/**
 * CCP supported cipher algorithms
 */
enum ccp_cipher_algo {
	CCP_CIPHER_ALGO_AES_CBC = 0,
	CCP_CIPHER_ALGO_AES_ECB,
	CCP_CIPHER_ALGO_AES_CTR,
	CCP_CIPHER_ALGO_AES_GCM,
	CCP_CIPHER_ALGO_3DES_CBC,
};

/**
 * CCP cipher operation type
 */
enum ccp_cipher_dir {
	CCP_CIPHER_DIR_DECRYPT = 0,
	CCP_CIPHER_DIR_ENCRYPT = 1,
};

/**
 * CCP supported hash algorithms
 */
enum ccp_hash_algo {
	CCP_AUTH_ALGO_SHA1 = 0,
	CCP_AUTH_ALGO_SHA1_HMAC,
	CCP_AUTH_ALGO_SHA224,
	CCP_AUTH_ALGO_SHA224_HMAC,
	CCP_AUTH_ALGO_SHA3_224,
	CCP_AUTH_ALGO_SHA3_224_HMAC,
	CCP_AUTH_ALGO_SHA256,
	CCP_AUTH_ALGO_SHA256_HMAC,
	CCP_AUTH_ALGO_SHA3_256,
	CCP_AUTH_ALGO_SHA3_256_HMAC,
	CCP_AUTH_ALGO_SHA384,
	CCP_AUTH_ALGO_SHA384_HMAC,
	CCP_AUTH_ALGO_SHA3_384,
	CCP_AUTH_ALGO_SHA3_384_HMAC,
	CCP_AUTH_ALGO_SHA512,
	CCP_AUTH_ALGO_SHA512_HMAC,
	CCP_AUTH_ALGO_SHA3_512,
	CCP_AUTH_ALGO_SHA3_512_HMAC,
	CCP_AUTH_ALGO_AES_CMAC,
	CCP_AUTH_ALGO_AES_GCM,
#ifdef RTE_LIBRTE_PMD_CCP_CPU_AUTH
	CCP_AUTH_ALGO_MD5_HMAC,
#endif
};

/**
 * CCP hash operation type
 */
enum ccp_hash_op {
	CCP_AUTH_OP_GENERATE = 0,
	CCP_AUTH_OP_VERIFY = 1,
};

/* CCP crypto private session structure */
struct ccp_session {
	enum ccp_cmd_order cmd_id;
	/**< chain order mode */
	struct {
		uint16_t length;
		uint16_t offset;
	} iv;
	/**< IV parameters */
	struct {
		enum ccp_cipher_algo  algo;
		enum ccp_engine  engine;
		union {
			enum ccp_aes_mode aes_mode;
			enum ccp_des_mode des_mode;
		} um;
		union {
			enum ccp_aes_type aes_type;
			enum ccp_des_type des_type;
		} ut;
		enum ccp_cipher_dir dir;
		uint64_t key_length;
		/**< max cipher key size 256 bits */
		uint8_t key[32];
		/**ccp key format*/
		uint8_t key_ccp[32];
		phys_addr_t key_phys;
		/**AES-ctr nonce(4) iv(8) ctr*/
		uint8_t nonce[32];
		phys_addr_t nonce_phys;
	} cipher;
	/**< Cipher Parameters */

	struct {
		enum ccp_hash_algo algo;
		enum ccp_engine  engine;
		union {
			enum ccp_aes_mode aes_mode;
		} um;
		union {
			enum ccp_sha_type sha_type;
			enum ccp_aes_type aes_type;
		} ut;
		enum ccp_hash_op op;
		uint64_t key_length;
		/**< max hash key size 144 bytes (struct capabilties) */
		uint8_t key[144];
		/**< max be key size of AES is 32*/
		uint8_t key_ccp[32];
		phys_addr_t key_phys;
		uint64_t digest_length;
		void *ctx;
		int ctx_len;
		int offset;
		int block_size;
		/**< Buffer to store  Software generated precomute values*/
		/**< For HMAC H(ipad ^ key) and H(opad ^ key) */
		/**< For CMAC K1 IV and K2 IV*/
		uint8_t pre_compute[2 * CCP_SHA3_CTX_SIZE];
		/**< SHA3 initial ctx all zeros*/
		uint8_t sha3_ctx[200];
		int aad_length;
	} auth;
	/**< Authentication Parameters */
	enum rte_crypto_aead_algorithm aead_algo;
	/**< AEAD Algorithm */

	uint32_t reserved;
} __rte_cache_aligned;

extern uint8_t ccp_cryptodev_driver_id;

struct ccp_qp;

/**
 * Set and validate CCP crypto session parameters
 *
 * @param sess ccp private session
 * @param xform crypto xform for this session
 * @return 0 on success otherwise -1
 */
int ccp_set_session_parameters(struct ccp_session *sess,
			       const struct rte_crypto_sym_xform *xform);

#endif /* _CCP_CRYPTO_H_ */
