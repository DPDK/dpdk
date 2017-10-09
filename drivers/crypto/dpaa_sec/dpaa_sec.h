/*-
 *   BSD LICENSE
 *
 *   Copyright 2016 NXP.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of NXP nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _DPAA_SEC_H_
#define _DPAA_SEC_H_

#define NUM_POOL_CHANNELS	4
#define DPAA_SEC_BURST		32
#define DPAA_SEC_ALG_UNSUPPORT	(-1)
#define TDES_CBC_IV_LEN		8
#define AES_CBC_IV_LEN		16
#define AES_CTR_IV_LEN		16
#define AES_GCM_IV_LEN		12

/* Minimum job descriptor consists of a oneword job descriptor HEADER and
 * a pointer to the shared descriptor.
 */
#define MIN_JOB_DESC_SIZE	(CAAM_CMD_SZ + CAAM_PTR_SZ)
/* CTX_POOL_NUM_BUFS is set as per the ipsec-secgw application */
#define CTX_POOL_NUM_BUFS	32000
#define CTX_POOL_BUF_SIZE	sizeof(struct dpaa_sec_op_ctx)
#define CTX_POOL_CACHE_SIZE	512

#define DIR_ENC                 1
#define DIR_DEC                 0

enum dpaa_sec_op_type {
	DPAA_SEC_NONE,  /*!< No Cipher operations*/
	DPAA_SEC_CIPHER,/*!< CIPHER operations */
	DPAA_SEC_AUTH,  /*!< Authentication Operations */
	DPAA_SEC_AEAD,  /*!< Authenticated Encryption with associated data */
	DPAA_SEC_IPSEC, /*!< IPSEC protocol operations*/
	DPAA_SEC_PDCP,  /*!< PDCP protocol operations*/
	DPAA_SEC_PKC,   /*!< Public Key Cryptographic Operations */
	DPAA_SEC_MAX
};

typedef struct dpaa_sec_session_entry {
	uint8_t dir;         /*!< Operation Direction */
	enum rte_crypto_cipher_algorithm cipher_alg; /*!< Cipher Algorithm*/
	enum rte_crypto_auth_algorithm auth_alg; /*!< Authentication Algorithm*/
	enum rte_crypto_aead_algorithm aead_alg; /*!< Authentication Algorithm*/
	union {
		struct {
			uint8_t *data;	/**< pointer to key data */
			size_t length;	/**< key length in bytes */
		} aead_key;
		struct {
			struct {
				uint8_t *data;	/**< pointer to key data */
				size_t length;	/**< key length in bytes */
			} cipher_key;
			struct {
				uint8_t *data;	/**< pointer to key data */
				size_t length;	/**< key length in bytes */
			} auth_key;
		};
	};
	struct {
		uint16_t length;
		uint16_t offset;
	} iv;	/**< Initialisation vector parameters */
	uint16_t auth_only_len; /*!< Length of data for Auth only */
	uint32_t digest_length;
	struct dpaa_sec_qp *qp;
	struct rte_mempool *ctx_pool; /* session mempool for dpaa_sec_op_ctx */
} dpaa_sec_session;

#define DPAA_SEC_MAX_DESC_SIZE  64
/* code or cmd block to caam */
struct sec_cdb {
	struct {
		union {
			uint32_t word;
			struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
				uint16_t rsvd63_48;
				unsigned int rsvd47_39:9;
				unsigned int idlen:7;
#else
				unsigned int idlen:7;
				unsigned int rsvd47_39:9;
				uint16_t rsvd63_48;
#endif
			} field;
		} __packed hi;

		union {
			uint32_t word;
			struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
				unsigned int rsvd31_30:2;
				unsigned int fsgt:1;
				unsigned int lng:1;
				unsigned int offset:2;
				unsigned int abs:1;
				unsigned int add_buf:1;
				uint8_t pool_id;
				uint16_t pool_buffer_size;
#else
				uint16_t pool_buffer_size;
				uint8_t pool_id;
				unsigned int add_buf:1;
				unsigned int abs:1;
				unsigned int offset:2;
				unsigned int lng:1;
				unsigned int fsgt:1;
				unsigned int rsvd31_30:2;
#endif
			} field;
		} __packed lo;
	} __packed sh_hdr;

	uint32_t sh_desc[DPAA_SEC_MAX_DESC_SIZE];
};

struct dpaa_sec_qp {
	struct dpaa_sec_dev_private *internals;
	struct sec_cdb cdb;		/* cmd block associated with qp */
	dpaa_sec_session *ses;		/* session associated with qp */
	struct qman_fq inq;
	struct qman_fq outq;
	int rx_pkts;
	int rx_errs;
	int tx_pkts;
	int tx_errs;
};

#define RTE_MAX_NB_SEC_QPS RTE_DPAA_SEC_PMD_MAX_NB_SESSIONS
/* internal sec queue interface */
struct dpaa_sec_dev_private {
	void *sec_hw;
	struct rte_mempool *ctx_pool; /* per dev mempool for dpaa_sec_op_ctx */
	struct dpaa_sec_qp qps[RTE_MAX_NB_SEC_QPS]; /* i/o queue for sec */
	unsigned int max_nb_queue_pairs;
	unsigned int max_nb_sessions;
};

#define MAX_SG_ENTRIES		16
#define SG_CACHELINE_0		0
#define SG_CACHELINE_1		4
#define SG_CACHELINE_2		8
#define SG_CACHELINE_3		12
struct dpaa_sec_job {
	/* sg[0] output, sg[1] input, others are possible sub frames */
	struct qm_sg_entry sg[MAX_SG_ENTRIES];
};

#define DPAA_MAX_NB_MAX_DIGEST	32
struct dpaa_sec_op_ctx {
	struct dpaa_sec_job job;
	struct rte_crypto_op *op;
	struct rte_mempool *ctx_pool; /* mempool pointer for dpaa_sec_op_ctx */
	uint32_t fd_status;
	uint8_t digest[DPAA_MAX_NB_MAX_DIGEST];
};

static const struct rte_cryptodev_capabilities dpaa_sec_capabilities[] = {
	{	/* MD5 HMAC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_MD5_HMAC,
				.block_size = 64,
				.key_size = {
					.min = 1,
					.max = 64,
					.increment = 1
				},
				.digest_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				},
			}, }
		}, }
	},
	{	/* SHA1 HMAC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA1_HMAC,
				.block_size = 64,
				.key_size = {
					.min = 1,
					.max = 64,
					.increment = 1
				},
				.digest_size = {
					.min = 20,
					.max = 20,
					.increment = 0
				},
			}, }
		}, }
	},
	{	/* SHA224 HMAC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA224_HMAC,
				.block_size = 64,
				.key_size = {
					.min = 1,
					.max = 64,
					.increment = 1
				},
				.digest_size = {
					.min = 28,
					.max = 28,
					.increment = 0
				},
			}, }
		}, }
	},
	{	/* SHA256 HMAC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA256_HMAC,
				.block_size = 64,
				.key_size = {
					.min = 1,
					.max = 64,
					.increment = 1
				},
				.digest_size = {
					.min = 32,
					.max = 32,
					.increment = 0
				},
			}, }
		}, }
	},
	{	/* SHA384 HMAC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA384_HMAC,
				.block_size = 128,
				.key_size = {
					.min = 1,
					.max = 128,
					.increment = 1
				},
				.digest_size = {
					.min = 48,
					.max = 48,
					.increment = 0
				},
			}, }
		}, }
	},
	{	/* SHA512 HMAC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA512_HMAC,
				.block_size = 128,
				.key_size = {
					.min = 1,
					.max = 128,
					.increment = 1
				},
				.digest_size = {
					.min = 64,
					.max = 64,
					.increment = 0
				},
			}, }
		}, }
	},
	{	/* AES GCM */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AEAD,
			{.auth = {
				.algo = RTE_CRYPTO_AEAD_AES_GCM,
				.block_size = 16,
				.key_size = {
					.min = 16,
					.max = 32,
					.increment = 8
				},
				.digest_size = {
					.min = 8,
					.max = 16,
					.increment = 4
				},
				.aad_size = {
					.min = 0,
					.max = 240,
					.increment = 1
				},
				.iv_size = {
					.min = 12,
					.max = 12,
					.increment = 0
				},
			}, }
		}, }
	},
	{	/* AES CBC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_AES_CBC,
				.block_size = 16,
				.key_size = {
					.min = 16,
					.max = 32,
					.increment = 8
				},
				.iv_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				}
			}, }
		}, }
	},
	{	/* AES CTR */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_AES_CTR,
				.block_size = 16,
				.key_size = {
					.min = 16,
					.max = 32,
					.increment = 8
				},
				.iv_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				}
			}, }
		}, }
	},
	{	/* 3DES CBC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_3DES_CBC,
				.block_size = 8,
				.key_size = {
					.min = 16,
					.max = 24,
					.increment = 8
				},
				.iv_size = {
					.min = 8,
					.max = 8,
					.increment = 0
				}
			}, }
		}, }
	},

	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

#endif /* _DPAA_SEC_H_ */
