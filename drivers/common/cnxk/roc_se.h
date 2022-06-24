/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef __ROC_SE_H__
#define __ROC_SE_H__

/* SE opcodes */
#define ROC_SE_MAJOR_OP_FC	      0x33
#define ROC_SE_FC_MINOR_OP_ENCRYPT    0x0
#define ROC_SE_FC_MINOR_OP_DECRYPT    0x1
#define ROC_SE_FC_MINOR_OP_HMAC_FIRST 0x10

#define ROC_SE_MAJOR_OP_HASH	   0x34
#define ROC_SE_MAJOR_OP_HMAC	   0x35
#define ROC_SE_MAJOR_OP_ZUC_SNOW3G 0x37
#define ROC_SE_MAJOR_OP_KASUMI	   0x38
#define ROC_SE_MAJOR_OP_MISC	   0x01

#define ROC_SE_MAX_AAD_SIZE 64
#define ROC_SE_MAX_MAC_LEN  64

#define ROC_SE_OFF_CTRL_LEN 8
#define ROC_SE_DMA_MODE	    (1 << 7)

#define ROC_SE_MAX_SG_IN_OUT_CNT 32
#define ROC_SE_MAX_SG_CNT	 (ROC_SE_MAX_SG_IN_OUT_CNT / 2)

#define ROC_SE_SG_LIST_HDR_SIZE (8u)
#define ROC_SE_SG_ENTRY_SIZE	sizeof(struct roc_se_sglist_comp)

#define ROC_SE_ZS_EA 0x1
#define ROC_SE_ZS_IA 0x2
#define ROC_SE_K_F8  0x4
#define ROC_SE_K_F9  0x8

#define ROC_SE_FC_GEN	 0x1
#define ROC_SE_PDCP	 0x2
#define ROC_SE_KASUMI	 0x3
#define ROC_SE_HASH_HMAC 0x4

#define ROC_SE_OP_CIPHER_ENCRYPT 0x1
#define ROC_SE_OP_CIPHER_DECRYPT 0x2
#define ROC_SE_OP_CIPHER_MASK                                                  \
	(ROC_SE_OP_CIPHER_ENCRYPT | ROC_SE_OP_CIPHER_DECRYPT)

#define ROC_SE_OP_AUTH_VERIFY	0x4
#define ROC_SE_OP_AUTH_GENERATE 0x8
#define ROC_SE_OP_AUTH_MASK                                                    \
	(ROC_SE_OP_AUTH_VERIFY | ROC_SE_OP_AUTH_GENERATE)

#define ROC_SE_OP_ENCODE (ROC_SE_OP_CIPHER_ENCRYPT | ROC_SE_OP_AUTH_GENERATE)
#define ROC_SE_OP_DECODE (ROC_SE_OP_CIPHER_DECRYPT | ROC_SE_OP_AUTH_VERIFY)

#define ROC_SE_ALWAYS_USE_SEPARATE_BUF

/*
 * Parameters for Flexi Crypto
 * requests
 */
#define ROC_SE_VALID_AAD_BUF	       0x01
#define ROC_SE_VALID_MAC_BUF	       0x02
#define ROC_SE_VALID_IV_BUF	       0x04
#define ROC_SE_SINGLE_BUF_INPLACE      0x08
#define ROC_SE_SINGLE_BUF_HEADROOM     0x10

#define ROC_SE_ENCR_IV_OFFSET(__d_offs) (((__d_offs) >> 32) & 0xffff)
#define ROC_SE_ENCR_OFFSET(__d_offs)	(((__d_offs) >> 16) & 0xffff)
#define ROC_SE_AUTH_OFFSET(__d_offs)	((__d_offs) & 0xffff)
#define ROC_SE_ENCR_DLEN(__d_lens)	((__d_lens) >> 32)
#define ROC_SE_AUTH_DLEN(__d_lens)	((__d_lens) & 0xffffffff)

typedef enum { ROC_SE_FROM_CTX = 0, ROC_SE_FROM_DPTR = 1 } roc_se_input_type;

typedef enum {
	ROC_SE_MD5_TYPE = 1,
	ROC_SE_SHA1_TYPE = 2,
	ROC_SE_SHA2_SHA224 = 3,
	ROC_SE_SHA2_SHA256 = 4,
	ROC_SE_SHA2_SHA384 = 5,
	ROC_SE_SHA2_SHA512 = 6,
	ROC_SE_GMAC_TYPE = 7,
	ROC_SE_POLY1305 = 8,
	ROC_SE_SHA3_SHA224 = 10,
	ROC_SE_SHA3_SHA256 = 11,
	ROC_SE_SHA3_SHA384 = 12,
	ROC_SE_SHA3_SHA512 = 13,
	ROC_SE_SHA3_SHAKE256 = 14,
	ROC_SE_SHA3_SHAKE512 = 15,

	/* These are only for software use */
	ROC_SE_ZUC_EIA3 = 0x90,
	ROC_SE_SNOW3G_UIA2 = 0x91,
	ROC_SE_AES_CMAC_EIA2 = 0x92,
	ROC_SE_KASUMI_F9_CBC = 0x93,
	ROC_SE_KASUMI_F9_ECB = 0x94,
} roc_se_auth_type;

typedef enum {
	/* To support passthrough */
	ROC_SE_PASSTHROUGH = 0x0,
	/*
	 * These are defined by MC for Flexi crypto
	 * for field of 4 bits
	 */
	ROC_SE_DES3_CBC = 0x1,
	ROC_SE_DES3_ECB = 0x2,
	ROC_SE_AES_CBC = 0x3,
	ROC_SE_AES_ECB = 0x4,
	ROC_SE_AES_CFB = 0x5,
	ROC_SE_AES_CTR = 0x6,
	ROC_SE_AES_GCM = 0x7,
	ROC_SE_AES_XTS = 0x8,
	ROC_SE_CHACHA20 = 0x9,

	/* These are only for software use */
	ROC_SE_ZUC_EEA3 = 0x90,
	ROC_SE_SNOW3G_UEA2 = 0x91,
	ROC_SE_AES_CTR_EEA2 = 0x92,
	ROC_SE_KASUMI_F8_CBC = 0x93,
	ROC_SE_KASUMI_F8_ECB = 0x94,
} roc_se_cipher_type;

typedef enum {
	/* Microcode errors */
	ROC_SE_NO_ERR = 0x00,
	ROC_SE_ERR_OPCODE_UNSUPPORTED = 0x01,

	/* SCATTER GATHER */
	ROC_SE_ERR_SCATTER_GATHER_WRITE_LENGTH = 0x02,
	ROC_SE_ERR_SCATTER_GATHER_LIST = 0x03,
	ROC_SE_ERR_SCATTER_GATHER_NOT_SUPPORTED = 0x04,

	/* SE GC */
	ROC_SE_ERR_GC_LENGTH_INVALID = 0x41,
	ROC_SE_ERR_GC_RANDOM_LEN_INVALID = 0x42,
	ROC_SE_ERR_GC_DATA_LEN_INVALID = 0x43,
	ROC_SE_ERR_GC_DRBG_TYPE_INVALID = 0x44,
	ROC_SE_ERR_GC_CTX_LEN_INVALID = 0x45,
	ROC_SE_ERR_GC_CIPHER_UNSUPPORTED = 0x46,
	ROC_SE_ERR_GC_AUTH_UNSUPPORTED = 0x47,
	ROC_SE_ERR_GC_OFFSET_INVALID = 0x48,
	ROC_SE_ERR_GC_HASH_MODE_UNSUPPORTED = 0x49,
	ROC_SE_ERR_GC_DRBG_ENTROPY_LEN_INVALID = 0x4a,
	ROC_SE_ERR_GC_DRBG_ADDNL_LEN_INVALID = 0x4b,
	ROC_SE_ERR_GC_ICV_MISCOMPARE = 0x4c,
	ROC_SE_ERR_GC_DATA_UNALIGNED = 0x4d,

	/* API Layer */
	ROC_SE_ERR_REQ_PENDING = 0xfe,
	ROC_SE_ERR_REQ_TIMEOUT = 0xff,

} roc_se_error_code;

typedef enum {
	ROC_SE_AES_128_BIT = 0x1,
	ROC_SE_AES_192_BIT = 0x2,
	ROC_SE_AES_256_BIT = 0x3
} roc_se_aes_type;

typedef enum {
	ROC_SE_PDCP_MAC_LEN_32_BIT = 0x1,
	ROC_SE_PDCP_MAC_LEN_64_BIT = 0x2,
	ROC_SE_PDCP_MAC_LEN_128_BIT = 0x3
} roc_se_pdcp_mac_len_type;

struct roc_se_sglist_comp {
	union {
		uint64_t len;
		struct {
			uint16_t len[4];
		} s;
	} u;
	uint64_t ptr[4];
};

struct roc_se_enc_context {
	uint64_t iv_source : 1;
	uint64_t aes_key : 2;
	uint64_t rsvd_60 : 1;
	uint64_t enc_cipher : 4;
	uint64_t auth_input_type : 1;
	uint64_t rsvd_52_54 : 3;
	uint64_t hash_type : 4;
	uint64_t mac_len : 8;
	uint64_t rsvd_39_0 : 40;
	uint8_t encr_key[32];
	uint8_t encr_iv[16];
};

struct roc_se_hmac_context {
	uint8_t ipad[64];
	uint8_t opad[64];
};

struct roc_se_context {
	struct roc_se_enc_context enc;
	struct roc_se_hmac_context hmac;
};

struct roc_se_otk_zuc_ctx {
	union {
		uint64_t u64;
		struct {
			uint64_t rsvd_56 : 57;
			uint64_t mac_len : 2;
			uint64_t key_len : 2;
			uint64_t lfsr_state : 1;
			uint64_t alg_type : 2;
		} s;
	} w0;
	uint8_t ci_key[32];
	uint8_t encr_auth_iv[24];
	uint8_t zuc_const[32];
};

struct roc_se_onk_zuc_ctx {
	uint8_t encr_auth_iv[16];
	uint8_t ci_key[16];
	uint8_t zuc_const[32];
};

struct roc_se_zuc_snow3g_ctx {
	union {
		struct roc_se_onk_zuc_ctx onk_ctx;
		struct roc_se_otk_zuc_ctx otk_ctx;
	} zuc;
};

struct roc_se_kasumi_ctx {
	uint8_t reg_A[8];
	uint8_t ci_key[16];
};

/* Buffer pointer */
struct roc_se_buf_ptr {
	void *vaddr;
	uint32_t size;
	uint32_t resv;
};

/* IOV Pointer */
struct roc_se_iov_ptr {
	int buf_cnt;
	struct roc_se_buf_ptr bufs[0];
};

struct roc_se_fc_params {
	/* 0th cache line */
	union {
		struct roc_se_buf_ptr bufs[1];
		struct {
			struct roc_se_iov_ptr *src_iov;
			struct roc_se_iov_ptr *dst_iov;
		};
	};
	void *iv_buf;
	void *auth_iv_buf;
	struct roc_se_buf_ptr meta_buf;
	struct roc_se_buf_ptr ctx_buf;
	uint32_t rsvd2;
	uint8_t rsvd3;
	uint8_t iv_ovr;
	uint8_t cipher_iv_len;
	uint8_t auth_iv_len;

	/* 1st cache line */
	struct roc_se_buf_ptr aad_buf __plt_cache_aligned;
	struct roc_se_buf_ptr mac_buf;
};

PLT_STATIC_ASSERT((offsetof(struct roc_se_fc_params, aad_buf) % 128) == 0);

#define ROC_SE_PDCP_ALG_TYPE_ZUC     0
#define ROC_SE_PDCP_ALG_TYPE_SNOW3G  1
#define ROC_SE_PDCP_ALG_TYPE_AES_CTR 2

struct roc_se_ctx {
	/* Below fields are accessed by sw */
	uint64_t enc_cipher : 8;
	uint64_t hash_type : 8;
	uint64_t mac_len : 8;
	uint64_t auth_key_len : 8;
	uint64_t fc_type : 4;
	uint64_t hmac : 1;
	uint64_t zsk_flags : 3;
	uint64_t k_ecb : 1;
	uint64_t pdcp_alg_type : 2;
	uint64_t rsvd : 21;
	union cpt_inst_w4 template_w4;
	/* Below fields are accessed by hardware */
	union {
		struct roc_se_context fctx;
		struct roc_se_zuc_snow3g_ctx zs_ctx;
		struct roc_se_kasumi_ctx k_ctx;
	} se_ctx;
	uint8_t *auth_key;
};

static inline void
roc_se_zuc_bytes_swap(uint8_t *arr, int len)
{
	int start, end;
	uint8_t tmp;

	if (len <= 0)
		return;

	start = 0;
	end = len - 1;

	while (start < end) {
		tmp = arr[start];
		arr[start] = arr[end];
		arr[end] = tmp;
		start++;
		end--;
	}
}

int __roc_api roc_se_auth_key_set(struct roc_se_ctx *se_ctx,
				  roc_se_auth_type type, const uint8_t *key,
				  uint16_t key_len, uint16_t mac_len);

int __roc_api roc_se_ciph_key_set(struct roc_se_ctx *se_ctx,
				  roc_se_cipher_type type, const uint8_t *key,
				  uint16_t key_len, uint8_t *salt);

void __roc_api roc_se_ctx_swap(struct roc_se_ctx *se_ctx);

#endif /* __ROC_SE_H__ */
