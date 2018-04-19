/* SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright (c) 2016 Freescale Semiconductor, Inc. All rights reserved.
 *   Copyright 2016 NXP
 *
 */

#ifndef _RTE_DPAA2_SEC_PMD_PRIVATE_H_
#define _RTE_DPAA2_SEC_PMD_PRIVATE_H_

#define CRYPTODEV_NAME_DPAA2_SEC_PMD	crypto_dpaa2_sec
/**< NXP DPAA2 - SEC PMD device name */

#define MAX_QUEUES		64
#define MAX_DESC_SIZE		64
/** private data structure for each DPAA2_SEC device */
struct dpaa2_sec_dev_private {
	void *mc_portal; /**< MC Portal for configuring this device */
	void *hw; /**< Hardware handle for this device.Used by NADK framework */
	struct rte_mempool *fle_pool; /* per device memory pool for FLE */
	int32_t hw_id; /**< An unique ID of this device instance */
	int32_t vfio_fd; /**< File descriptor received via VFIO */
	uint16_t token; /**< Token required by DPxxx objects */
	unsigned int max_nb_queue_pairs;
	/**< Max number of queue pairs supported by device */
	unsigned int max_nb_sessions;
	/**< Max number of sessions supported by device */
};

struct dpaa2_sec_qp {
	struct dpaa2_queue rx_vq;
	struct dpaa2_queue tx_vq;
};

enum shr_desc_type {
	DESC_UPDATE,
	DESC_FINAL,
	DESC_INITFINAL,
};

#define DIR_ENC                 1
#define DIR_DEC                 0

#define DPAA2_SET_FLC_EWS(flc)  (flc->word1_bits23_16 |= 0x1)
#define DPAA2_SET_FLC_RSC(flc)  (flc->word1_bits31_24 |= 0x1)
#define DPAA2_SET_FLC_REUSE_BS(flc) (flc->mode_bits |= 0x8000)
#define DPAA2_SET_FLC_REUSE_FF(flc) (flc->mode_bits |= 0x2000)

/* SEC Flow Context Descriptor */
struct sec_flow_context {
	/* word 0 */
	uint16_t word0_sdid;		/* 11-0  SDID */
	uint16_t word0_res;		/* 31-12 reserved */

	/* word 1 */
	uint8_t word1_sdl;		/* 5-0 SDL */
					/* 7-6 reserved */

	uint8_t word1_bits_15_8;        /* 11-8 CRID */
					/* 14-12 reserved */
					/* 15 CRJD */

	uint8_t word1_bits23_16;	/* 16  EWS */
					/* 17 DAC */
					/* 18,19,20 ? */
					/* 23-21 reserved */

	uint8_t word1_bits31_24;	/* 24 RSC */
					/* 25 RBMT */
					/* 31-26 reserved */

	/* word 2  RFLC[31-0] */
	uint32_t word2_rflc_31_0;

	/* word 3  RFLC[63-32] */
	uint32_t word3_rflc_63_32;

	/* word 4 */
	uint16_t word4_iicid;		/* 15-0  IICID */
	uint16_t word4_oicid;		/* 31-16 OICID */

	/* word 5 */
	uint32_t word5_ofqid:24;		/* 23-0 OFQID */
	uint32_t word5_31_24:8;
					/* 24 OSC */
					/* 25 OBMT */
					/* 29-26 reserved */
					/* 31-30 ICR */

	/* word 6 */
	uint32_t word6_oflc_31_0;

	/* word 7 */
	uint32_t word7_oflc_63_32;

	/* Word 8-15 storage profiles */
	uint16_t dl;			/**<  DataLength(correction) */
	uint16_t reserved;		/**< reserved */
	uint16_t dhr;			/**< DataHeadRoom(correction) */
	uint16_t mode_bits;		/**< mode bits */
	uint16_t bpv0;			/**< buffer pool0 valid */
	uint16_t bpid0;			/**< Bypass Memory Translation */
	uint16_t bpv1;			/**< buffer pool1 valid */
	uint16_t bpid1;			/**< Bypass Memory Translation */
	uint64_t word_12_15[2];		/**< word 12-15 are reserved */
};

struct sec_flc_desc {
	struct sec_flow_context flc;
	uint32_t desc[MAX_DESC_SIZE];
};

struct ctxt_priv {
	struct rte_mempool *fle_pool; /* per device memory pool for FLE */
	struct sec_flc_desc flc_desc[0];
};

enum dpaa2_sec_op_type {
	DPAA2_SEC_NONE,  /*!< No Cipher operations*/
	DPAA2_SEC_CIPHER,/*!< CIPHER operations */
	DPAA2_SEC_AUTH,  /*!< Authentication Operations */
	DPAA2_SEC_AEAD,  /*!< AEAD (AES-GCM/CCM) type operations */
	DPAA2_SEC_CIPHER_HASH,  /*!< Authenticated Encryption with
				 * associated data
				 */
	DPAA2_SEC_HASH_CIPHER,  /*!< Encryption with Authenticated
				 * associated data
				 */
	DPAA2_SEC_IPSEC, /*!< IPSEC protocol operations*/
	DPAA2_SEC_PDCP,  /*!< PDCP protocol operations*/
	DPAA2_SEC_PKC,   /*!< Public Key Cryptographic Operations */
	DPAA2_SEC_MAX
};

struct dpaa2_sec_aead_ctxt {
	uint16_t auth_only_len; /*!< Length of data for Auth only */
	uint8_t auth_cipher_text;       /**< Authenticate/cipher ordering */
};

typedef struct dpaa2_sec_session_entry {
	void *ctxt;
	uint8_t ctxt_type;
	uint8_t dir;         /*!< Operation Direction */
	enum rte_crypto_cipher_algorithm cipher_alg; /*!< Cipher Algorithm*/
	enum rte_crypto_auth_algorithm auth_alg; /*!< Authentication Algorithm*/
	enum rte_crypto_aead_algorithm aead_alg; /*!< AEAD Algorithm*/
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
		uint16_t length; /**< IV length in bytes */
		uint16_t offset; /**< IV offset in bytes */
	} iv;
	uint16_t digest_length;
	uint8_t status;
	union {
		struct dpaa2_sec_aead_ctxt aead_ctxt;
	} ext_params;
} dpaa2_sec_session;

static const struct rte_cryptodev_capabilities dpaa2_sec_capabilities[] = {
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
				},
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

static const struct rte_security_capability dpaa2_sec_security_cap[] = {
	{ /* IPsec Lookaside Protocol offload ESP Transport Egress */
		.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
		.ipsec = {
			.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
			.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL,
			.direction = RTE_SECURITY_IPSEC_SA_DIR_EGRESS,
			.options = { 0 }
		},
		.crypto_capabilities = dpaa2_sec_capabilities
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
		.crypto_capabilities = dpaa2_sec_capabilities
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

#endif /* _RTE_DPAA2_SEC_PMD_PRIVATE_H_ */
