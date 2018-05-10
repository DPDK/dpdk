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

#define AES_BLOCK_SIZE 16
#define CMAC_PAD_VALUE 0x80
#define CTR_NONCE_SIZE 4
#define CTR_IV_SIZE 8
#define CCP_SHA3_CTX_SIZE 200

/**Macro helpers for CCP command creation*/
#define	CCP_AES_SIZE(p)		((p)->aes.size)
#define	CCP_AES_ENCRYPT(p)	((p)->aes.encrypt)
#define	CCP_AES_MODE(p)		((p)->aes.mode)
#define	CCP_AES_TYPE(p)		((p)->aes.type)
#define	CCP_DES_ENCRYPT(p)	((p)->des.encrypt)
#define	CCP_DES_MODE(p)		((p)->des.mode)
#define	CCP_DES_TYPE(p)		((p)->des.type)
#define	CCP_SHA_TYPE(p)		((p)->sha.type)
#define	CCP_PT_BYTESWAP(p)	((p)->pt.byteswap)
#define	CCP_PT_BITWISE(p)	((p)->pt.bitwise)

/* HMAC */
#define HMAC_IPAD_VALUE 0x36
#define HMAC_OPAD_VALUE 0x5c

/* MD5 */
#define MD5_DIGEST_SIZE         16
#define MD5_BLOCK_SIZE          64

/* SHA */
#define SHA_COMMON_DIGEST_SIZE	32
#define SHA1_DIGEST_SIZE        20
#define SHA1_BLOCK_SIZE         64

#define SHA224_DIGEST_SIZE      28
#define SHA224_BLOCK_SIZE       64
#define SHA3_224_BLOCK_SIZE     144

#define SHA256_DIGEST_SIZE      32
#define SHA256_BLOCK_SIZE       64
#define SHA3_256_BLOCK_SIZE     136

#define SHA384_DIGEST_SIZE      48
#define SHA384_BLOCK_SIZE       128
#define SHA3_384_BLOCK_SIZE	104

#define SHA512_DIGEST_SIZE      64
#define SHA512_BLOCK_SIZE       128
#define SHA3_512_BLOCK_SIZE     72

/* Maximum length for digest */
#define DIGEST_LENGTH_MAX	64

/* SHA LSB intialiazation values */

#define SHA1_H0		0x67452301UL
#define SHA1_H1		0xefcdab89UL
#define SHA1_H2		0x98badcfeUL
#define SHA1_H3		0x10325476UL
#define SHA1_H4		0xc3d2e1f0UL

#define SHA224_H0	0xc1059ed8UL
#define SHA224_H1	0x367cd507UL
#define SHA224_H2	0x3070dd17UL
#define SHA224_H3	0xf70e5939UL
#define SHA224_H4	0xffc00b31UL
#define SHA224_H5	0x68581511UL
#define SHA224_H6	0x64f98fa7UL
#define SHA224_H7	0xbefa4fa4UL

#define SHA256_H0	0x6a09e667UL
#define SHA256_H1	0xbb67ae85UL
#define SHA256_H2	0x3c6ef372UL
#define SHA256_H3	0xa54ff53aUL
#define SHA256_H4	0x510e527fUL
#define SHA256_H5	0x9b05688cUL
#define SHA256_H6	0x1f83d9abUL
#define SHA256_H7	0x5be0cd19UL

#define SHA384_H0	0xcbbb9d5dc1059ed8ULL
#define SHA384_H1	0x629a292a367cd507ULL
#define SHA384_H2	0x9159015a3070dd17ULL
#define SHA384_H3	0x152fecd8f70e5939ULL
#define SHA384_H4	0x67332667ffc00b31ULL
#define SHA384_H5	0x8eb44a8768581511ULL
#define SHA384_H6	0xdb0c2e0d64f98fa7ULL
#define SHA384_H7	0x47b5481dbefa4fa4ULL

#define SHA512_H0	0x6a09e667f3bcc908ULL
#define SHA512_H1	0xbb67ae8584caa73bULL
#define SHA512_H2	0x3c6ef372fe94f82bULL
#define SHA512_H3	0xa54ff53a5f1d36f1ULL
#define SHA512_H4	0x510e527fade682d1ULL
#define SHA512_H5	0x9b05688c2b3e6c1fULL
#define SHA512_H6	0x1f83d9abfb41bd6bULL
#define SHA512_H7	0x5be0cd19137e2179ULL

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
	CCP_AUTH_ALGO_MD5_HMAC,
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
	bool auth_opt;
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
struct ccp_private;

/**
 * Set and validate CCP crypto session parameters
 *
 * @param sess ccp private session
 * @param xform crypto xform for this session
 * @return 0 on success otherwise -1
 */
int ccp_set_session_parameters(struct ccp_session *sess,
			       const struct rte_crypto_sym_xform *xform,
			       struct ccp_private *internals);

/**
 * Find count of slots
 *
 * @param session CCP private session
 * @return count of free slots available
 */
int ccp_compute_slot_count(struct ccp_session *session);

/**
 * process crypto ops to be enqueued
 *
 * @param qp CCP crypto queue-pair
 * @param op crypto ops table
 * @param cmd_q CCP cmd queue
 * @param nb_ops No. of ops to be submitted
 * @return 0 on success otherwise -1
 */
int process_ops_to_enqueue(struct ccp_qp *qp,
			   struct rte_crypto_op **op,
			   struct ccp_queue *cmd_q,
			   uint16_t nb_ops,
			   int slots_req);

/**
 * process crypto ops to be dequeued
 *
 * @param qp CCP crypto queue-pair
 * @param op crypto ops table
 * @param nb_ops requested no. of ops
 * @return 0 on success otherwise -1
 */
int process_ops_to_dequeue(struct ccp_qp *qp,
			   struct rte_crypto_op **op,
			   uint16_t nb_ops);


/**
 * Apis for SHA3 partial hash generation
 * @param data_in buffer pointer on which phash is applied
 * @param data_out phash result in ccp be format is written
 */
int partial_hash_sha3_224(uint8_t *data_in,
			  uint8_t *data_out);

int partial_hash_sha3_256(uint8_t *data_in,
			  uint8_t *data_out);

int partial_hash_sha3_384(uint8_t *data_in,
			  uint8_t *data_out);

int partial_hash_sha3_512(uint8_t *data_in,
			  uint8_t *data_out);

#endif /* _CCP_CRYPTO_H_ */
