/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Broadcom
 * All rights reserved.
 */

#ifndef _BCMFS_SYM_ENGINE_H_
#define _BCMFS_SYM_ENGINE_H_

#include <rte_crypto_sym.h>

#include "bcmfs_dev_msg.h"
#include "bcmfs_sym_defs.h"
#include "bcmfs_sym_req.h"

/* structure to hold element's arrtibutes */
struct fsattr {
	void *va;
	uint64_t pa;
	uint64_t sz;
};

#define fsattr_va(__ptr)      ((__ptr)->va)
#define fsattr_pa(__ptr)      ((__ptr)->pa)
#define fsattr_sz(__ptr)      ((__ptr)->sz)

/*
 *  Macros for Crypto h/w constraints
 */

#define BCMFS_CRYPTO_AES_BLOCK_SIZE	16
#define BCMFS_CRYPTO_AES_MIN_KEY_SIZE	16
#define BCMFS_CRYPTO_AES_MAX_KEY_SIZE	32

#define BCMFS_CRYPTO_DES_BLOCK_SIZE	8
#define BCMFS_CRYPTO_DES_KEY_SIZE	8

#define BCMFS_CRYPTO_3DES_BLOCK_SIZE	8
#define BCMFS_CRYPTO_3DES_KEY_SIZE	(3 * 8)

#define BCMFS_CRYPTO_MD5_DIGEST_SIZE	16
#define BCMFS_CRYPTO_MD5_BLOCK_SIZE	64

#define BCMFS_CRYPTO_SHA1_DIGEST_SIZE	20
#define BCMFS_CRYPTO_SHA1_BLOCK_SIZE	64

#define BCMFS_CRYPTO_SHA224_DIGEST_SIZE	28
#define BCMFS_CRYPTO_SHA224_BLOCK_SIZE	64

#define BCMFS_CRYPTO_SHA256_DIGEST_SIZE	32
#define BCMFS_CRYPTO_SHA256_BLOCK_SIZE	64

#define BCMFS_CRYPTO_SHA384_DIGEST_SIZE	48
#define BCMFS_CRYPTO_SHA384_BLOCK_SIZE	128

#define BCMFS_CRYPTO_SHA512_DIGEST_SIZE	64
#define BCMFS_CRYPTO_SHA512_BLOCK_SIZE	128

#define BCMFS_CRYPTO_SHA3_224_DIGEST_SIZE	(224 / 8)
#define BCMFS_CRYPTO_SHA3_224_BLOCK_SIZE	(200 - 2 * \
					BCMFS_CRYPTO_SHA3_224_DIGEST_SIZE)

#define BCMFS_CRYPTO_SHA3_256_DIGEST_SIZE	(256 / 8)
#define BCMFS_CRYPTO_SHA3_256_BLOCK_SIZE	(200 - 2 * \
					BCMFS_CRYPTO_SHA3_256_DIGEST_SIZE)

#define BCMFS_CRYPTO_SHA3_384_DIGEST_SIZE	(384 / 8)
#define BCMFS_CRYPTO_SHA3_384_BLOCK_SIZE	(200 - 2 * \
					BCMFS_CRYPTO_SHA3_384_DIGEST_SIZE)

#define BCMFS_CRYPTO_SHA3_512_DIGEST_SIZE	(512 / 8)
#define BCMFS_CRYPTO_SHA3_512_BLOCK_SIZE	(200 - 2 * \
					BCMFS_CRYPTO_SHA3_512_DIGEST_SIZE)

enum bcmfs_crypto_aes_cipher_key {
	BCMFS_CRYPTO_AES128 = 16,
	BCMFS_CRYPTO_AES192 = 24,
	BCMFS_CRYPTO_AES256 = 32,
};

int
bcmfs_crypto_build_cipher_req(struct bcmfs_sym_request *req,
			      enum rte_crypto_cipher_algorithm c_algo,
			      enum rte_crypto_cipher_operation cop,
			      struct fsattr *src, struct fsattr *dst,
			      struct fsattr *key, struct fsattr *iv);

int
bcmfs_crypto_build_auth_req(struct bcmfs_sym_request *req,
			    enum rte_crypto_auth_algorithm a_algo,
			    enum rte_crypto_auth_operation aop,
			    struct fsattr *src, struct fsattr *dst,
			    struct fsattr *mac, struct fsattr *key,
			    struct fsattr *iv);

int
bcmfs_crypto_build_chain_request(struct bcmfs_sym_request *req,
				 enum rte_crypto_cipher_algorithm c_algo,
				 enum rte_crypto_cipher_operation cop,
				 enum rte_crypto_auth_algorithm a_algo,
				 enum rte_crypto_auth_operation aop,
				 struct fsattr *src, struct fsattr *dst,
				 struct fsattr *cipher_key,
				 struct fsattr *auth_key,
				 struct fsattr *iv, struct fsattr *aad,
				 struct fsattr *digest, bool cipher_first);

int
bcmfs_crypto_build_aead_request(struct bcmfs_sym_request *req,
				enum rte_crypto_aead_algorithm ae_algo,
				enum rte_crypto_aead_operation aeop,
				struct fsattr *src, struct fsattr *dst,
				struct fsattr *key, struct fsattr *iv,
				struct fsattr *aad, struct fsattr *digest);

#endif /* _BCMFS_SYM_ENGINE_H_ */
