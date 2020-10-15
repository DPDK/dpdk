/* SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright (c) 2016 Freescale Semiconductor, Inc. All rights reserved.
 *   Copyright 2016,2020 NXP
 *
 */

#ifndef _DPAA2_SEC_PMD_PRIVATE_H_
#define _DPAA2_SEC_PMD_PRIVATE_H_

#ifdef RTE_LIB_SECURITY
#include <rte_security_driver.h>
#endif

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

#define DPAA2_IPv6_DEFAULT_VTC_FLOW	0x60000000

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

#ifdef RTE_LIB_SECURITY
/*
 * The structure is to be filled by user for PDCP Protocol
 */
struct dpaa2_pdcp_ctxt {
	enum rte_security_pdcp_domain domain; /*!< Data/Control mode*/
	int8_t bearer;	/*!< PDCP bearer ID */
	int8_t pkt_dir;/*!< PDCP Frame Direction 0:UL 1:DL*/
	int8_t hfn_ovd;/*!< Overwrite HFN per packet*/
	uint8_t sn_size;	/*!< Sequence number size, 5/7/12/15/18 */
	uint32_t hfn_ovd_offset;/*!< offset from rte_crypto_op at which
				 * per packet hfn is stored
				 */
	uint32_t hfn;	/*!< Hyper Frame Number */
	uint32_t hfn_threshold;	/*!< HFN Threashold for key renegotiation */
};
#endif
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
	union {
		struct {
			struct {
				uint16_t length; /**< IV length in bytes */
				uint16_t offset; /**< IV offset in bytes */
			} iv;
			uint16_t digest_length;
			uint8_t status;
			union {
				struct dpaa2_sec_aead_ctxt aead_ctxt;
			} ext_params;
		};
#ifdef RTE_LIB_SECURITY
		struct dpaa2_pdcp_ctxt pdcp;
#endif
	};
} dpaa2_sec_session;

static const struct rte_cryptodev_capabilities dpaa2_sec_capabilities[] = {
	/* Symmetric capabilities */
	{	/* NULL (AUTH) */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_NULL,
				.block_size = 1,
				.key_size = {
					.min = 0,
					.max = 0,
					.increment = 0
				},
				.digest_size = {
					.min = 0,
					.max = 0,
					.increment = 0
				},
				.iv_size = { 0 }
			}, },
		}, },
	},
	{	/* MD5 */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_MD5,
				.block_size = 64,
				.key_size = {
					.min = 0,
					.max = 0,
					.increment = 0
				},
				.digest_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				},
				.iv_size = { 0 }
			}, }
		}, }
	},
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
	{	/* SHA1 */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA1,
				.block_size = 64,
				.key_size = {
					.min = 0,
					.max = 0,
					.increment = 0
				},
				.digest_size = {
					.min = 20,
					.max = 20,
					.increment = 0
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
	{	/* SHA224 */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA224,
				.block_size = 64,
					.key_size = {
					.min = 0,
					.max = 0,
					.increment = 0
				},
				.digest_size = {
					.min = 28,
					.max = 28,
					.increment = 0
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
	{	/* SHA256 */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA256,
				.block_size = 64,
				.key_size = {
					.min = 0,
					.max = 0,
					.increment = 0
				},
				.digest_size = {
					.min = 32,
					.max = 32,
					.increment = 0
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
	{	/* SHA384 */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA384,
				.block_size = 64,
				.key_size = {
					.min = 0,
					.max = 0,
					.increment = 0
				},
				.digest_size = {
					.min = 48,
					.max = 48,
					.increment = 0
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
	{	/* SHA512 */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA512,
				.block_size = 128,
				.key_size = {
					.min = 0,
					.max = 0,
					.increment = 0
				},
				.digest_size = {
					.min = 64,
					.max = 64,
					.increment = 0
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
	{	/* NULL (CIPHER) */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_NULL,
				.block_size = 1,
				.key_size = {
					.min = 0,
					.max = 0,
					.increment = 0
				},
				.iv_size = {
					.min = 0,
					.max = 0,
					.increment = 0
				}
			}, },
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
	{	/* DES CBC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_DES_CBC,
				.block_size = 8,
				.key_size = {
					.min = 8,
					.max = 8,
					.increment = 0
				},
				.iv_size = {
					.min = 8,
					.max = 8,
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
	{	/* SNOW 3G (UIA2) */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SNOW3G_UIA2,
				.block_size = 16,
				.key_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				},
				.digest_size = {
					.min = 4,
					.max = 4,
					.increment = 0
				},
				.iv_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				}
			}, }
		}, }
	},
	{	/* SNOW 3G (UEA2) */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_SNOW3G_UEA2,
				.block_size = 16,
				.key_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				},
				.iv_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				}
			}, }
		}, }
	},
	{	/* ZUC (EEA3) */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_ZUC_EEA3,
				.block_size = 16,
				.key_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				},
				.iv_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				}
			}, }
		}, }
	},
	{	/* ZUC (EIA3) */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_ZUC_EIA3,
				.block_size = 16,
				.key_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				},
				.digest_size = {
					.min = 4,
					.max = 4,
					.increment = 0
				},
				.iv_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				}
			}, }
		}, }
	},
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

#ifdef RTE_LIB_SECURITY

static const struct rte_cryptodev_capabilities dpaa2_pdcp_capabilities[] = {
	{	/* SNOW 3G (UIA2) */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SNOW3G_UIA2,
				.block_size = 16,
				.key_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				},
				.digest_size = {
					.min = 4,
					.max = 4,
					.increment = 0
				},
				.iv_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				}
			}, }
		}, }
	},
	{	/* SNOW 3G (UEA2) */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_SNOW3G_UEA2,
				.block_size = 16,
				.key_size = {
					.min = 16,
					.max = 16,
					.increment = 0
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
	{	/* NULL (AUTH) */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_NULL,
				.block_size = 1,
				.key_size = {
					.min = 0,
					.max = 0,
					.increment = 0
				},
				.digest_size = {
					.min = 0,
					.max = 0,
					.increment = 0
				},
				.iv_size = { 0 }
			}, },
		}, },
	},
	{	/* NULL (CIPHER) */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_NULL,
				.block_size = 1,
				.key_size = {
					.min = 0,
					.max = 0,
					.increment = 0
				},
				.iv_size = {
					.min = 0,
					.max = 0,
					.increment = 0
				}
			}, },
		}, }
	},
	{	/* ZUC (EEA3) */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_ZUC_EEA3,
				.block_size = 16,
				.key_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				},
				.iv_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				}
			}, }
		}, }
	},
	{	/* ZUC (EIA3) */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_ZUC_EIA3,
				.block_size = 16,
				.key_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				},
				.digest_size = {
					.min = 4,
					.max = 4,
					.increment = 0
				},
				.iv_size = {
					.min = 16,
					.max = 16,
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
			.options = { 0 },
			.replay_win_sz_max = 128
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
			.options = { 0 },
			.replay_win_sz_max = 128
		},
		.crypto_capabilities = dpaa2_sec_capabilities
	},
	{ /* PDCP Lookaside Protocol offload Data */
		.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_PDCP,
		.pdcp = {
			.domain = RTE_SECURITY_PDCP_MODE_DATA,
			.capa_flags = 0
		},
		.crypto_capabilities = dpaa2_pdcp_capabilities
	},
	{ /* PDCP Lookaside Protocol offload Control */
		.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_PDCP,
		.pdcp = {
			.domain = RTE_SECURITY_PDCP_MODE_CONTROL,
			.capa_flags = 0
		},
		.crypto_capabilities = dpaa2_pdcp_capabilities
	},
	{
		.action = RTE_SECURITY_ACTION_TYPE_NONE
	}
};
#endif
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

#endif /* _DPAA2_SEC_PMD_PRIVATE_H_ */
