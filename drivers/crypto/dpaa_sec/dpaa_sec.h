/* SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright 2016 NXP
 *
 */

#ifndef _DPAA_SEC_H_
#define _DPAA_SEC_H_

#define CRYPTODEV_NAME_DPAA_SEC_PMD	crypto_dpaa_sec
/**< NXP DPAA - SEC PMD device name */

#define NUM_POOL_CHANNELS	4
#define DPAA_SEC_BURST		7
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
#define RTE_DPAA_SEC_PMD_MAX_NB_SESSIONS 2048

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

typedef struct dpaa_sec_session_entry {
	uint8_t dir;         /*!< Operation Direction */
	enum rte_crypto_cipher_algorithm cipher_alg; /*!< Cipher Algorithm*/
	enum rte_crypto_auth_algorithm auth_alg; /*!< Authentication Algorithm*/
	enum rte_crypto_aead_algorithm aead_alg; /*!< AEAD Algorithm*/
	enum rte_security_session_protocol proto_alg; /*!< Security Algorithm*/
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
	struct ipsec_encap_pdb encap_pdb;
	struct ip ip4_hdr;
	struct ipsec_decap_pdb decap_pdb;
	struct dpaa_sec_qp *qp;
	struct qman_fq *inq;
	struct sec_cdb cdb;	/**< cmd block associated with qp */
	struct rte_mempool *ctx_pool; /* session mempool for dpaa_sec_op_ctx */
} dpaa_sec_session;

struct dpaa_sec_qp {
	struct dpaa_sec_dev_private *internals;
	struct qman_fq outq;
	int rx_pkts;
	int rx_errs;
	int tx_pkts;
	int tx_errs;
};

#define RTE_DPAA_MAX_NB_SEC_QPS 2
#define RTE_DPAA_MAX_RX_QUEUE RTE_DPAA_SEC_PMD_MAX_NB_SESSIONS
#define DPAA_MAX_DEQUEUE_NUM_FRAMES 63

/* internal sec queue interface */
struct dpaa_sec_dev_private {
	void *sec_hw;
	struct rte_mempool *ctx_pool; /* per dev mempool for dpaa_sec_op_ctx */
	struct dpaa_sec_qp qps[RTE_DPAA_MAX_NB_SEC_QPS]; /* i/o queue for sec */
	struct qman_fq inq[RTE_DPAA_MAX_RX_QUEUE];
	unsigned char inq_attach[RTE_DPAA_MAX_RX_QUEUE];
	unsigned int max_nb_queue_pairs;
	unsigned int max_nb_sessions;
	rte_spinlock_t lock;
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
	int64_t vtop_offset;
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
					.min = 1,
					.max = 16,
					.increment = 1
				},
				.iv_size = { 0 }
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
					.min = 1,
					.max = 20,
					.increment = 1
				},
				.iv_size = { 0 }
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
					.min = 1,
					.max = 28,
					.increment = 1
				},
				.iv_size = { 0 }
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
					.min = 1,
					.max = 32,
					.increment = 1
				},
				.iv_size = { 0 }
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
					.min = 1,
					.max = 48,
					.increment = 1
				},
				.iv_size = { 0 }
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
					.min = 1,
					.max = 64,
					.increment = 1
				},
				.iv_size = { 0 }
			}, }
		}, }
	},
	{	/* AES GCM */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AEAD,
			{.aead = {
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

static const struct rte_security_capability dpaa_sec_security_cap[] = {
	{ /* IPsec Lookaside Protocol offload ESP Transport Egress */
		.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
		.ipsec = {
			.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
			.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL,
			.direction = RTE_SECURITY_IPSEC_SA_DIR_EGRESS,
			.options = { 0 }
		},
		.crypto_capabilities = dpaa_sec_capabilities
	},
	{ /* IPsec Lookaside Protocol offload ESP Tunnel Ingress */
		.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
		.ipsec = {
			.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
			.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL,
			.direction = RTE_SECURITY_IPSEC_SA_DIR_INGRESS,
			.options = { 0 }
		},
		.crypto_capabilities = dpaa_sec_capabilities
	},
	{
		.action = RTE_SECURITY_ACTION_TYPE_NONE
	}
};

/**
 * Checksum
 *
 * @param buffer calculate chksum for buffer
 * @param len    buffer length
 *
 * @return checksum value in host cpu order
 */
static inline uint16_t
calc_chksum(void *buffer, int len)
{
	uint16_t *buf = (uint16_t *)buffer;
	uint32_t sum = 0;
	uint16_t result;

	for (sum = 0; len > 1; len -= 2)
		sum += *buf++;

	if (len == 1)
		sum += *(unsigned char *)buf;

	sum = (sum >> 16) + (sum & 0xFFFF);
	sum += (sum >> 16);
	result = ~sum;

	return  result;
}

#endif /* _DPAA_SEC_H_ */
