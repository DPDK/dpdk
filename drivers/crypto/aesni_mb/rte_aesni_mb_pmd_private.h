/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2016 Intel Corporation
 */

#ifndef _RTE_AESNI_MB_PMD_PRIVATE_H_
#define _RTE_AESNI_MB_PMD_PRIVATE_H_

#include "aesni_mb_ops.h"

#define CRYPTODEV_NAME_AESNI_MB_PMD	crypto_aesni_mb
/**< AES-NI Multi buffer PMD device name */

#define MB_LOG_ERR(fmt, args...) \
	RTE_LOG(ERR, CRYPTODEV, "[%s] %s() line %u: " fmt "\n",  \
			RTE_STR(CRYPTODEV_NAME_AESNI_MB_PMD), \
			__func__, __LINE__, ## args)

#ifdef RTE_LIBRTE_AESNI_MB_DEBUG
#define MB_LOG_INFO(fmt, args...) \
	RTE_LOG(INFO, CRYPTODEV, "[%s] %s() line %u: " fmt "\n", \
			CRYPTODEV_NAME_AESNI_MB_PMD, \
			__func__, __LINE__, ## args)

#define MB_LOG_DBG(fmt, args...) \
	RTE_LOG(DEBUG, CRYPTODEV, "[%s] %s() line %u: " fmt "\n", \
			CRYPTODEV_NAME_AESNI_MB_PMD, \
			__func__, __LINE__, ## args)
#else
#define MB_LOG_INFO(fmt, args...)
#define MB_LOG_DBG(fmt, args...)
#endif

#define HMAC_IPAD_VALUE			(0x36)
#define HMAC_OPAD_VALUE			(0x5C)

/* Maximum length for digest (SHA-512 truncated needs 32 bytes) */
#define DIGEST_LENGTH_MAX 32
static const unsigned auth_blocksize[] = {
		[MD5]		= 64,
		[SHA1]		= 64,
		[SHA_224]	= 64,
		[SHA_256]	= 64,
		[SHA_384]	= 128,
		[SHA_512]	= 128,
		[AES_XCBC]	= 16,
		[AES_CCM]	= 16,
};

/**
 * Get the blocksize in bytes for a specified authentication algorithm
 *
 * @Note: this function will not return a valid value for a non-valid
 * authentication algorithm
 */
static inline unsigned
get_auth_algo_blocksize(JOB_HASH_ALG algo)
{
	return auth_blocksize[algo];
}

static const unsigned auth_truncated_digest_byte_lengths[] = {
		[MD5]		= 12,
		[SHA1]		= 12,
		[SHA_224]	= 14,
		[SHA_256]	= 16,
		[SHA_384]	= 24,
		[SHA_512]	= 32,
		[AES_XCBC]	= 12,
		[AES_CMAC]	= 16,
		[AES_CCM]	= 8,
		[NULL_HASH]	= 0
};

/**
 * Get the IPsec specified truncated length in bytes of the HMAC digest for a
 * specified authentication algorithm
 *
 * @Note: this function will not return a valid value for a non-valid
 * authentication algorithm
 */
static inline unsigned
get_truncated_digest_byte_length(JOB_HASH_ALG algo)
{
	return auth_truncated_digest_byte_lengths[algo];
}

static const unsigned auth_digest_byte_lengths[] = {
		[MD5]		= 16,
		[SHA1]		= 20,
		[SHA_224]	= 28,
		[SHA_256]	= 32,
		[SHA_384]	= 48,
		[SHA_512]	= 64,
		[AES_XCBC]	= 16,
		[AES_CMAC]	= 16,
		[NULL_HASH]		= 0
};

/**
 * Get the output digest size in bytes for a specified authentication algorithm
 *
 * @Note: this function will not return a valid value for a non-valid
 * authentication algorithm
 */
static inline unsigned
get_digest_byte_length(JOB_HASH_ALG algo)
{
	return auth_digest_byte_lengths[algo];
}

enum aesni_mb_operation {
	AESNI_MB_OP_HASH_CIPHER,
	AESNI_MB_OP_CIPHER_HASH,
	AESNI_MB_OP_HASH_ONLY,
	AESNI_MB_OP_CIPHER_ONLY,
	AESNI_MB_OP_AEAD_HASH_CIPHER,
	AESNI_MB_OP_AEAD_CIPHER_HASH,
	AESNI_MB_OP_NOT_SUPPORTED
};

/** private data structure for each virtual AESNI device */
struct aesni_mb_private {
	enum aesni_mb_vector_mode vector_mode;
	/**< CPU vector instruction set mode */
	unsigned max_nb_queue_pairs;
	/**< Max number of queue pairs supported by device */
	unsigned max_nb_sessions;
	/**< Max number of sessions supported by device */
};

/** AESNI Multi buffer queue pair */
struct aesni_mb_qp {
	uint16_t id;
	/**< Queue Pair Identifier */
	char name[RTE_CRYPTODEV_NAME_MAX_LEN];
	/**< Unique Queue Pair Name */
	const struct aesni_mb_op_fns *op_fns;
	/**< Vector mode dependent pointer table of the multi-buffer APIs */
	MB_MGR mb_mgr;
	/**< Multi-buffer instance */
	struct rte_ring *ingress_queue;
       /**< Ring for placing operations ready for processing */
	struct rte_mempool *sess_mp;
	/**< Session Mempool */
	struct rte_cryptodev_stats stats;
	/**< Queue pair statistics */
	uint8_t digest_idx;
	/**< Index of the next slot to be used in temp_digests,
	 * to store the digest for a given operation
	 */
	uint8_t temp_digests[MAX_JOBS][DIGEST_LENGTH_MAX];
	/**< Buffers used to store the digest generated
	 * by the driver when verifying a digest provided
	 * by the user (using authentication verify operation)
	 */
} __rte_cache_aligned;

/** AES-NI multi-buffer private session structure */
struct aesni_mb_session {
	JOB_CHAIN_ORDER chain_order;
	struct {
		uint16_t length;
		uint16_t offset;
	} iv;
	/**< IV parameters */

	/** Cipher Parameters */
	struct {
		/** Cipher direction - encrypt / decrypt */
		JOB_CIPHER_DIRECTION direction;
		/** Cipher mode - CBC / Counter */
		JOB_CIPHER_MODE mode;

		uint64_t key_length_in_bytes;

		struct {
			uint32_t encode[60] __rte_aligned(16);
			/**< encode key */
			uint32_t decode[60] __rte_aligned(16);
			/**< decode key */
		} expanded_aes_keys;
		/**< Expanded AES keys - Allocating space to
		 * contain the maximum expanded key size which
		 * is 240 bytes for 256 bit AES, calculate by:
		 * ((key size (bytes)) *
		 * ((number of rounds) + 1))
		 */
	} cipher;

	/** Authentication Parameters */
	struct {
		JOB_HASH_ALG algo; /**< Authentication Algorithm */
		enum rte_crypto_auth_operation operation;
		/**< auth operation generate or verify */
		union {
			struct {
				uint8_t inner[128] __rte_aligned(16);
				/**< inner pad */
				uint8_t outer[128] __rte_aligned(16);
				/**< outer pad */
			} pads;
			/**< HMAC Authentication pads -
			 * allocating space for the maximum pad
			 * size supported which is 128 bytes for
			 * SHA512
			 */

			struct {
			    uint32_t k1_expanded[44] __rte_aligned(16);
			    /**< k1 (expanded key). */
			    uint8_t k2[16] __rte_aligned(16);
			    /**< k2. */
			    uint8_t k3[16] __rte_aligned(16);
			    /**< k3. */
			} xcbc;

			struct {
				uint32_t expkey[60] __rte_aligned(16);
						    /**< k1 (expanded key). */
				uint32_t skey1[4] __rte_aligned(16);
						    /**< k2. */
				uint32_t skey2[4] __rte_aligned(16);
						    /**< k3. */
			} cmac;
			/**< Expanded XCBC authentication keys */
		};
	/** digest size */
	uint16_t digest_len;

	} auth;
	struct {
		/** AAD data length */
		uint16_t aad_len;
	} aead;
} __rte_cache_aligned;


/**
 *
 */
extern int
aesni_mb_set_session_parameters(const struct aesni_mb_op_fns *mb_ops,
		struct aesni_mb_session *sess,
		const struct rte_crypto_sym_xform *xform);


/** device specific operations function pointer structure */
extern struct rte_cryptodev_ops *rte_aesni_mb_pmd_ops;



#endif /* _RTE_AESNI_MB_PMD_PRIVATE_H_ */
