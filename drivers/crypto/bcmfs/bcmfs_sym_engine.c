/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Broadcom.
 * All rights reserved.
 */

#include <stdbool.h>
#include <string.h>

#include <rte_common.h>
#include <rte_cryptodev.h>
#include <rte_crypto_sym.h>

#include "bcmfs_logs.h"
#include "bcmfs_sym_defs.h"
#include "bcmfs_dev_msg.h"
#include "bcmfs_sym_req.h"
#include "bcmfs_sym_engine.h"

enum spu2_cipher_type {
	SPU2_CIPHER_TYPE_NONE = 0x0,
	SPU2_CIPHER_TYPE_AES128 = 0x1,
	SPU2_CIPHER_TYPE_AES192 = 0x2,
	SPU2_CIPHER_TYPE_AES256 = 0x3,
	SPU2_CIPHER_TYPE_DES = 0x4,
	SPU2_CIPHER_TYPE_3DES = 0x5,
	SPU2_CIPHER_TYPE_LAST
};

enum spu2_cipher_mode {
	SPU2_CIPHER_MODE_ECB = 0x0,
	SPU2_CIPHER_MODE_CBC = 0x1,
	SPU2_CIPHER_MODE_CTR = 0x2,
	SPU2_CIPHER_MODE_CFB = 0x3,
	SPU2_CIPHER_MODE_OFB = 0x4,
	SPU2_CIPHER_MODE_XTS = 0x5,
	SPU2_CIPHER_MODE_CCM = 0x6,
	SPU2_CIPHER_MODE_GCM = 0x7,
	SPU2_CIPHER_MODE_LAST
};

enum spu2_hash_type {
	SPU2_HASH_TYPE_NONE = 0x0,
	SPU2_HASH_TYPE_AES128 = 0x1,
	SPU2_HASH_TYPE_AES192 = 0x2,
	SPU2_HASH_TYPE_AES256 = 0x3,
	SPU2_HASH_TYPE_MD5 = 0x6,
	SPU2_HASH_TYPE_SHA1 = 0x7,
	SPU2_HASH_TYPE_SHA224 = 0x8,
	SPU2_HASH_TYPE_SHA256 = 0x9,
	SPU2_HASH_TYPE_SHA384 = 0xa,
	SPU2_HASH_TYPE_SHA512 = 0xb,
	SPU2_HASH_TYPE_SHA512_224 = 0xc,
	SPU2_HASH_TYPE_SHA512_256 = 0xd,
	SPU2_HASH_TYPE_SHA3_224 = 0xe,
	SPU2_HASH_TYPE_SHA3_256 = 0xf,
	SPU2_HASH_TYPE_SHA3_384 = 0x10,
	SPU2_HASH_TYPE_SHA3_512 = 0x11,
	SPU2_HASH_TYPE_LAST
};

enum spu2_hash_mode {
	SPU2_HASH_MODE_CMAC = 0x0,
	SPU2_HASH_MODE_CBC_MAC = 0x1,
	SPU2_HASH_MODE_XCBC_MAC = 0x2,
	SPU2_HASH_MODE_HMAC = 0x3,
	SPU2_HASH_MODE_RABIN = 0x4,
	SPU2_HASH_MODE_CCM = 0x5,
	SPU2_HASH_MODE_GCM = 0x6,
	SPU2_HASH_MODE_RESERVED = 0x7,
	SPU2_HASH_MODE_LAST
};

enum spu2_proto_sel {
	SPU2_PROTO_RESV = 0,
	SPU2_MACSEC_SECTAG8_ECB = 1,
	SPU2_MACSEC_SECTAG8_SCB = 2,
	SPU2_MACSEC_SECTAG16 = 3,
	SPU2_MACSEC_SECTAG16_8_XPN = 4,
	SPU2_IPSEC = 5,
	SPU2_IPSEC_ESN = 6,
	SPU2_TLS_CIPHER = 7,
	SPU2_TLS_AEAD = 8,
	SPU2_DTLS_CIPHER = 9,
	SPU2_DTLS_AEAD = 10
};

/* SPU2 response size */
#define SPU2_STATUS_LEN			2

/* Metadata settings in response */
enum spu2_ret_md_opts {
	SPU2_RET_NO_MD = 0,		/* return no metadata */
	SPU2_RET_FMD_OMD = 1,		/* return both FMD and OMD */
	SPU2_RET_FMD_ONLY = 2,		/* return only FMD */
	SPU2_RET_FMD_OMD_IV = 3,	/* return FMD and OMD with just IVs */
};

/* FMD ctrl0 field masks */
#define SPU2_CIPH_ENCRYPT_EN            0x1 /* 0: decrypt, 1: encrypt */
#define SPU2_CIPH_TYPE_SHIFT              4
#define SPU2_CIPH_MODE                0xF00 /* one of spu2_cipher_mode */
#define SPU2_CIPH_MODE_SHIFT              8
#define SPU2_CFB_MASK                0x7000 /* cipher feedback mask */
#define SPU2_CFB_MASK_SHIFT              12
#define SPU2_PROTO_SEL             0xF00000 /* MACsec, IPsec, TLS... */
#define SPU2_PROTO_SEL_SHIFT             20
#define SPU2_HASH_FIRST           0x1000000 /* 1: hash input is input pkt
					     * data
					     */
#define SPU2_CHK_TAG              0x2000000 /* 1: check digest provided */
#define SPU2_HASH_TYPE          0x1F0000000 /* one of spu2_hash_type */
#define SPU2_HASH_TYPE_SHIFT             28
#define SPU2_HASH_MODE         0xF000000000 /* one of spu2_hash_mode */
#define SPU2_HASH_MODE_SHIFT             36
#define SPU2_CIPH_PAD_EN     0x100000000000 /* 1: Add pad to end of payload for
					     *    enc
					     */
#define SPU2_CIPH_PAD      0xFF000000000000 /* cipher pad value */
#define SPU2_CIPH_PAD_SHIFT              48

/* FMD ctrl1 field masks */
#define SPU2_TAG_LOC                    0x1 /* 1: end of payload, 0: undef */
#define SPU2_HAS_FR_DATA                0x2 /* 1: msg has frame data */
#define SPU2_HAS_AAD1                   0x4 /* 1: msg has AAD1 field */
#define SPU2_HAS_NAAD                   0x8 /* 1: msg has NAAD field */
#define SPU2_HAS_AAD2                  0x10 /* 1: msg has AAD2 field */
#define SPU2_HAS_ESN                   0x20 /* 1: msg has ESN field */
#define SPU2_HASH_KEY_LEN            0xFF00 /* len of hash key in bytes.
					     * HMAC only.
					     */
#define SPU2_HASH_KEY_LEN_SHIFT           8
#define SPU2_CIPH_KEY_LEN         0xFF00000 /* len of cipher key in bytes */
#define SPU2_CIPH_KEY_LEN_SHIFT          20
#define SPU2_GENIV               0x10000000 /* 1: hw generates IV */
#define SPU2_HASH_IV             0x20000000 /* 1: IV incl in hash */
#define SPU2_RET_IV              0x40000000 /* 1: return IV in output msg
					     *    b4 payload
					     */
#define SPU2_RET_IV_LEN         0xF00000000 /* length in bytes of IV returned.
					     * 0 = 16 bytes
					     */
#define SPU2_RET_IV_LEN_SHIFT            32
#define SPU2_IV_OFFSET         0xF000000000 /* gen IV offset */
#define SPU2_IV_OFFSET_SHIFT             36
#define SPU2_IV_LEN          0x1F0000000000 /* length of input IV in bytes */
#define SPU2_IV_LEN_SHIFT                40
#define SPU2_HASH_TAG_LEN  0x7F000000000000 /* hash tag length in bytes */
#define SPU2_HASH_TAG_LEN_SHIFT          48
#define SPU2_RETURN_MD    0x300000000000000 /* return metadata */
#define SPU2_RETURN_MD_SHIFT             56
#define SPU2_RETURN_FD    0x400000000000000
#define SPU2_RETURN_AAD1  0x800000000000000
#define SPU2_RETURN_NAAD 0x1000000000000000
#define SPU2_RETURN_AAD2 0x2000000000000000
#define SPU2_RETURN_PAY  0x4000000000000000 /* return payload */

/* FMD ctrl2 field masks */
#define SPU2_AAD1_OFFSET              0xFFF /* byte offset of AAD1 field */
#define SPU2_AAD1_LEN               0xFF000 /* length of AAD1 in bytes */
#define SPU2_AAD1_LEN_SHIFT              12
#define SPU2_AAD2_OFFSET         0xFFF00000 /* byte offset of AAD2 field */
#define SPU2_AAD2_OFFSET_SHIFT           20
#define SPU2_PL_OFFSET   0xFFFFFFFF00000000 /* payload offset from AAD2 */
#define SPU2_PL_OFFSET_SHIFT             32

/* FMD ctrl3 field masks */
#define SPU2_PL_LEN              0xFFFFFFFF /* payload length in bytes */
#define SPU2_TLS_LEN         0xFFFF00000000 /* TLS encrypt: cipher len
					     * TLS decrypt: compressed len
					     */
#define SPU2_TLS_LEN_SHIFT               32

/*
 * Max value that can be represented in the Payload Length field of the
 * ctrl3 word of FMD.
 */
#define SPU2_MAX_PAYLOAD  SPU2_PL_LEN

#define SPU2_VAL_NONE	0

/* CCM B_0 field definitions, common for SPU-M and SPU2 */
#define CCM_B0_ADATA		0x40
#define CCM_B0_ADATA_SHIFT	   6
#define CCM_B0_M_PRIME		0x38
#define CCM_B0_M_PRIME_SHIFT	   3
#define CCM_B0_L_PRIME		0x07
#define CCM_B0_L_PRIME_SHIFT	   0
#define CCM_ESP_L_VALUE		   4

static uint16_t
spu2_cipher_type_xlate(enum rte_crypto_cipher_algorithm cipher_alg,
		       enum spu2_cipher_type *spu2_type,
		       struct fsattr *key)
{
	int ret = 0;
	int key_size = fsattr_sz(key);

	if (cipher_alg == RTE_CRYPTO_CIPHER_AES_XTS)
		key_size = key_size / 2;

	switch (key_size) {
	case BCMFS_CRYPTO_AES128:
		*spu2_type = SPU2_CIPHER_TYPE_AES128;
		break;
	case BCMFS_CRYPTO_AES192:
		*spu2_type = SPU2_CIPHER_TYPE_AES192;
		break;
	case BCMFS_CRYPTO_AES256:
		*spu2_type = SPU2_CIPHER_TYPE_AES256;
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int
spu2_hash_xlate(enum rte_crypto_auth_algorithm auth_alg,
		struct fsattr *key,
		enum spu2_hash_type *spu2_type,
		enum spu2_hash_mode *spu2_mode)
{
	*spu2_mode = 0;

	switch (auth_alg) {
	case RTE_CRYPTO_AUTH_NULL:
		*spu2_type = SPU2_HASH_TYPE_NONE;
		break;
	case RTE_CRYPTO_AUTH_MD5:
		*spu2_type = SPU2_HASH_TYPE_MD5;
		break;
	case RTE_CRYPTO_AUTH_MD5_HMAC:
		*spu2_type = SPU2_HASH_TYPE_MD5;
		*spu2_mode = SPU2_HASH_MODE_HMAC;
		break;
	case RTE_CRYPTO_AUTH_SHA1:
		*spu2_type = SPU2_HASH_TYPE_SHA1;
		break;
	case RTE_CRYPTO_AUTH_SHA1_HMAC:
		*spu2_type = SPU2_HASH_TYPE_SHA1;
		*spu2_mode = SPU2_HASH_MODE_HMAC;
		break;
	case RTE_CRYPTO_AUTH_SHA224:
		*spu2_type = SPU2_HASH_TYPE_SHA224;
		break;
	case RTE_CRYPTO_AUTH_SHA224_HMAC:
		*spu2_type = SPU2_HASH_TYPE_SHA224;
		*spu2_mode = SPU2_HASH_MODE_HMAC;
		break;
	case RTE_CRYPTO_AUTH_SHA256:
		*spu2_type = SPU2_HASH_TYPE_SHA256;
		break;
	case RTE_CRYPTO_AUTH_SHA256_HMAC:
		*spu2_type = SPU2_HASH_TYPE_SHA256;
		*spu2_mode = SPU2_HASH_MODE_HMAC;
		break;
	case RTE_CRYPTO_AUTH_SHA384:
		*spu2_type = SPU2_HASH_TYPE_SHA384;
		break;
	case RTE_CRYPTO_AUTH_SHA384_HMAC:
		*spu2_type = SPU2_HASH_TYPE_SHA384;
		*spu2_mode = SPU2_HASH_MODE_HMAC;
		break;
	case RTE_CRYPTO_AUTH_SHA512:
		*spu2_type = SPU2_HASH_TYPE_SHA512;
		break;
	case RTE_CRYPTO_AUTH_SHA512_HMAC:
		*spu2_type = SPU2_HASH_TYPE_SHA512;
		*spu2_mode = SPU2_HASH_MODE_HMAC;
		break;
	case RTE_CRYPTO_AUTH_SHA3_224:
		*spu2_type = SPU2_HASH_TYPE_SHA3_224;
		break;
	case RTE_CRYPTO_AUTH_SHA3_224_HMAC:
		*spu2_type = SPU2_HASH_TYPE_SHA3_224;
		*spu2_mode = SPU2_HASH_MODE_HMAC;
		break;
	case RTE_CRYPTO_AUTH_SHA3_256:
		*spu2_type = SPU2_HASH_TYPE_SHA3_256;
		break;
	case RTE_CRYPTO_AUTH_SHA3_256_HMAC:
		*spu2_type = SPU2_HASH_TYPE_SHA3_256;
		*spu2_mode = SPU2_HASH_MODE_HMAC;
		break;
	case RTE_CRYPTO_AUTH_SHA3_384:
		*spu2_type = SPU2_HASH_TYPE_SHA3_384;
		break;
	case RTE_CRYPTO_AUTH_SHA3_384_HMAC:
		*spu2_type = SPU2_HASH_TYPE_SHA3_384;
		*spu2_mode = SPU2_HASH_MODE_HMAC;
		break;
	case RTE_CRYPTO_AUTH_SHA3_512:
		*spu2_type = SPU2_HASH_TYPE_SHA3_512;
		break;
	case RTE_CRYPTO_AUTH_SHA3_512_HMAC:
		*spu2_type = SPU2_HASH_TYPE_SHA3_512;
		*spu2_mode = SPU2_HASH_MODE_HMAC;
		break;
	case RTE_CRYPTO_AUTH_AES_XCBC_MAC:
		*spu2_mode = SPU2_HASH_MODE_XCBC_MAC;
		switch (fsattr_sz(key)) {
		case BCMFS_CRYPTO_AES128:
			*spu2_type = SPU2_HASH_TYPE_AES128;
			break;
		case BCMFS_CRYPTO_AES192:
			*spu2_type = SPU2_HASH_TYPE_AES192;
			break;
		case BCMFS_CRYPTO_AES256:
			*spu2_type = SPU2_HASH_TYPE_AES256;
			break;
		default:
			return -EINVAL;
		}
		break;
	case RTE_CRYPTO_AUTH_AES_CMAC:
		*spu2_mode = SPU2_HASH_MODE_CMAC;
		switch (fsattr_sz(key)) {
		case BCMFS_CRYPTO_AES128:
			*spu2_type = SPU2_HASH_TYPE_AES128;
			break;
		case BCMFS_CRYPTO_AES192:
			*spu2_type = SPU2_HASH_TYPE_AES192;
			break;
		case BCMFS_CRYPTO_AES256:
			*spu2_type = SPU2_HASH_TYPE_AES256;
			break;
		default:
			return -EINVAL;
		}
		break;
	case RTE_CRYPTO_AUTH_AES_GMAC:
		*spu2_mode = SPU2_HASH_MODE_GCM;
		switch (fsattr_sz(key)) {
		case BCMFS_CRYPTO_AES128:
			*spu2_type = SPU2_HASH_TYPE_AES128;
			break;
		case BCMFS_CRYPTO_AES192:
			*spu2_type = SPU2_HASH_TYPE_AES192;
			break;
		case BCMFS_CRYPTO_AES256:
			*spu2_type = SPU2_HASH_TYPE_AES256;
			break;
		default:
			return -EINVAL;
		}
		break;
	case RTE_CRYPTO_AUTH_AES_CBC_MAC:
		*spu2_mode = SPU2_HASH_MODE_CBC_MAC;
		switch (fsattr_sz(key)) {
		case BCMFS_CRYPTO_AES128:
			*spu2_type = SPU2_HASH_TYPE_AES128;
			break;
		case BCMFS_CRYPTO_AES192:
			*spu2_type = SPU2_HASH_TYPE_AES192;
			break;
		case BCMFS_CRYPTO_AES256:
			*spu2_type = SPU2_HASH_TYPE_AES256;
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int
spu2_cipher_xlate(enum rte_crypto_cipher_algorithm cipher_alg,
		  struct fsattr *key,
		  enum spu2_cipher_type *spu2_type,
		  enum spu2_cipher_mode *spu2_mode)
{
	int ret = 0;

	switch (cipher_alg) {
	case RTE_CRYPTO_CIPHER_NULL:
		*spu2_type = SPU2_CIPHER_TYPE_NONE;
		break;
	case RTE_CRYPTO_CIPHER_DES_CBC:
		*spu2_mode =  SPU2_CIPHER_MODE_CBC;
		*spu2_type = SPU2_CIPHER_TYPE_DES;
		break;
	case RTE_CRYPTO_CIPHER_3DES_ECB:
		*spu2_mode =  SPU2_CIPHER_MODE_ECB;
		*spu2_type = SPU2_CIPHER_TYPE_3DES;
		break;
	case RTE_CRYPTO_CIPHER_3DES_CBC:
		*spu2_mode =  SPU2_CIPHER_MODE_CBC;
		*spu2_type = SPU2_CIPHER_TYPE_3DES;
		break;
	case RTE_CRYPTO_CIPHER_AES_CBC:
		*spu2_mode =  SPU2_CIPHER_MODE_CBC;
		ret = spu2_cipher_type_xlate(cipher_alg, spu2_type, key);
		break;
	case RTE_CRYPTO_CIPHER_AES_ECB:
		*spu2_mode =  SPU2_CIPHER_MODE_ECB;
		ret = spu2_cipher_type_xlate(cipher_alg, spu2_type, key);
		break;
	case RTE_CRYPTO_CIPHER_AES_CTR:
		*spu2_mode =  SPU2_CIPHER_MODE_CTR;
		ret = spu2_cipher_type_xlate(cipher_alg, spu2_type, key);
		break;
	case RTE_CRYPTO_CIPHER_AES_XTS:
		*spu2_mode =  SPU2_CIPHER_MODE_XTS;
		ret = spu2_cipher_type_xlate(cipher_alg, spu2_type, key);
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static void
spu2_fmd_ctrl0_write(struct spu2_fmd *fmd,
		     bool is_inbound, bool auth_first,
		     enum spu2_proto_sel protocol,
		     enum spu2_cipher_type cipher_type,
		     enum spu2_cipher_mode cipher_mode,
		     enum spu2_hash_type auth_type,
		     enum spu2_hash_mode auth_mode)
{
	uint64_t ctrl0 = 0;

	if (cipher_type != SPU2_CIPHER_TYPE_NONE && !is_inbound)
		ctrl0 |= SPU2_CIPH_ENCRYPT_EN;

	ctrl0 |= ((uint64_t)cipher_type << SPU2_CIPH_TYPE_SHIFT) |
		  ((uint64_t)cipher_mode << SPU2_CIPH_MODE_SHIFT);

	if (protocol != SPU2_PROTO_RESV)
		ctrl0 |= (uint64_t)protocol << SPU2_PROTO_SEL_SHIFT;

	if (auth_first)
		ctrl0 |= SPU2_HASH_FIRST;

	if (is_inbound && auth_type != SPU2_HASH_TYPE_NONE)
		ctrl0 |= SPU2_CHK_TAG;

	ctrl0 |= (((uint64_t)auth_type << SPU2_HASH_TYPE_SHIFT) |
		  ((uint64_t)auth_mode << SPU2_HASH_MODE_SHIFT));

	fmd->ctrl0 = ctrl0;

#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
	BCMFS_DP_HEXDUMP_LOG(DEBUG, "ctrl0:", &fmd->ctrl0, sizeof(uint64_t));
#endif
}

static void
spu2_fmd_ctrl1_write(struct spu2_fmd *fmd, bool is_inbound,
		     uint64_t assoc_size, uint64_t auth_key_len,
		     uint64_t cipher_key_len, bool gen_iv, bool hash_iv,
		     bool return_iv, uint64_t ret_iv_len,
		     uint64_t ret_iv_offset, uint64_t cipher_iv_len,
		     uint64_t digest_size, bool return_payload, bool return_md)
{
	uint64_t ctrl1 = 0;

	if (is_inbound && digest_size != 0)
		ctrl1 |= SPU2_TAG_LOC;

	if (assoc_size != 0)
		ctrl1 |= SPU2_HAS_AAD2;

	if (auth_key_len != 0)
		ctrl1 |= ((auth_key_len << SPU2_HASH_KEY_LEN_SHIFT) &
			  SPU2_HASH_KEY_LEN);

	if (cipher_key_len != 0)
		ctrl1 |= ((cipher_key_len << SPU2_CIPH_KEY_LEN_SHIFT) &
			  SPU2_CIPH_KEY_LEN);

	if (gen_iv)
		ctrl1 |= SPU2_GENIV;

	if (hash_iv)
		ctrl1 |= SPU2_HASH_IV;

	if (return_iv) {
		ctrl1 |= SPU2_RET_IV;
		ctrl1 |= ret_iv_len << SPU2_RET_IV_LEN_SHIFT;
		ctrl1 |= ret_iv_offset << SPU2_IV_OFFSET_SHIFT;
	}

	ctrl1 |= ((cipher_iv_len << SPU2_IV_LEN_SHIFT) & SPU2_IV_LEN);

	if (digest_size != 0) {
		ctrl1 |= ((digest_size << SPU2_HASH_TAG_LEN_SHIFT) &
			  SPU2_HASH_TAG_LEN);
	}

	/*
	 * Let's ask for the output pkt to include FMD, but don't need to
	 * get keys and IVs back in OMD.
	 */
	if (return_md)
		ctrl1 |= ((uint64_t)SPU2_RET_FMD_ONLY << SPU2_RETURN_MD_SHIFT);
	else
		ctrl1 |= ((uint64_t)SPU2_RET_NO_MD << SPU2_RETURN_MD_SHIFT);

	/* Crypto API does not get assoc data back. So no need for AAD2. */

	if (return_payload)
		ctrl1 |= SPU2_RETURN_PAY;

	fmd->ctrl1 = ctrl1;

#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
	BCMFS_DP_HEXDUMP_LOG(DEBUG, "ctrl1:", &fmd->ctrl1, sizeof(uint64_t));
#endif
}

static void
spu2_fmd_ctrl2_write(struct spu2_fmd *fmd, uint64_t cipher_offset,
		     uint64_t auth_key_len __rte_unused,
		     uint64_t auth_iv_len  __rte_unused,
		     uint64_t cipher_key_len  __rte_unused,
		     uint64_t cipher_iv_len  __rte_unused)
{
	uint64_t aad1_offset;
	uint64_t aad2_offset;
	uint16_t aad1_len = 0;
	uint64_t payload_offset;

	/* AAD1 offset is from start of FD. FD length always 0. */
	aad1_offset = 0;

	aad2_offset = aad1_offset;
	payload_offset = cipher_offset;
	fmd->ctrl2 = aad1_offset |
		     (aad1_len << SPU2_AAD1_LEN_SHIFT) |
		     (aad2_offset << SPU2_AAD2_OFFSET_SHIFT) |
		     (payload_offset << SPU2_PL_OFFSET_SHIFT);

#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
	BCMFS_DP_HEXDUMP_LOG(DEBUG, "ctrl2:", &fmd->ctrl2, sizeof(uint64_t));
#endif
}

static void
spu2_fmd_ctrl3_write(struct spu2_fmd *fmd, uint64_t payload_len)
{
	fmd->ctrl3 = payload_len & SPU2_PL_LEN;

#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
	BCMFS_DP_HEXDUMP_LOG(DEBUG, "ctrl3:", &fmd->ctrl3, sizeof(uint64_t));
#endif
}

int
bcmfs_crypto_build_auth_req(struct bcmfs_sym_request *sreq,
			    enum rte_crypto_auth_algorithm a_alg,
			    enum rte_crypto_auth_operation auth_op,
			    struct fsattr *src, struct fsattr *dst,
			    struct fsattr *mac, struct fsattr *auth_key,
			    struct fsattr *iv)
{
	int ret;
	uint64_t dst_size;
	int src_index = 0;
	struct spu2_fmd *fmd;
	uint64_t payload_len;
	uint32_t src_msg_len = 0;
	enum spu2_hash_mode spu2_auth_mode;
	enum spu2_hash_type spu2_auth_type = SPU2_HASH_TYPE_NONE;
	uint64_t iv_size = (iv != NULL) ? fsattr_sz(iv) : 0;
	uint64_t auth_ksize = (auth_key != NULL) ? fsattr_sz(auth_key) : 0;
	bool is_inbound = (auth_op == RTE_CRYPTO_AUTH_OP_VERIFY);

	if (src == NULL)
		return -EINVAL;

	payload_len = fsattr_sz(src);
	if (!payload_len) {
		BCMFS_DP_LOG(ERR, "null payload not supported");
		return -EINVAL;
	}

	/* one of dst or mac should not be NULL */
	if (dst == NULL && mac == NULL)
		return -EINVAL;

	if (auth_op == RTE_CRYPTO_AUTH_OP_GENERATE && dst != NULL)
		dst_size = fsattr_sz(dst);
	else if (auth_op == RTE_CRYPTO_AUTH_OP_VERIFY && mac != NULL)
		dst_size = fsattr_sz(mac);
	else
		return -EINVAL;

	/* spu2 hash algorithm and hash algorithm mode */
	ret = spu2_hash_xlate(a_alg, auth_key, &spu2_auth_type,
			      &spu2_auth_mode);
	if (ret)
		return -EINVAL;

	fmd  = &sreq->fmd;

	spu2_fmd_ctrl0_write(fmd, is_inbound, SPU2_VAL_NONE,
			     SPU2_PROTO_RESV, SPU2_VAL_NONE,
			     SPU2_VAL_NONE, spu2_auth_type, spu2_auth_mode);

	spu2_fmd_ctrl1_write(fmd, is_inbound, SPU2_VAL_NONE,
			     auth_ksize, SPU2_VAL_NONE, false,
			     false, SPU2_VAL_NONE, SPU2_VAL_NONE,
			     SPU2_VAL_NONE, iv_size,
			     dst_size, SPU2_VAL_NONE, SPU2_VAL_NONE);

	memset(&fmd->ctrl2, 0, sizeof(uint64_t));

	spu2_fmd_ctrl3_write(fmd, fsattr_sz(src));

	/* FMD */
	sreq->msgs.srcs_addr[src_index] = sreq->fptr;
	src_msg_len += sizeof(*fmd);

	/* Start of OMD */
	if (auth_ksize != 0) {
		memcpy((uint8_t *)fmd + src_msg_len, fsattr_va(auth_key),
		       auth_ksize);
		src_msg_len += auth_ksize;
	}

	if (iv_size != 0) {
		memcpy((uint8_t *)fmd + src_msg_len, fsattr_va(iv),
		       iv_size);
		src_msg_len += iv_size;
	} /* End of OMD */

	sreq->msgs.srcs_len[src_index] = src_msg_len;
	src_index++;

	sreq->msgs.srcs_addr[src_index] = fsattr_pa(src);
	sreq->msgs.srcs_len[src_index] = fsattr_sz(src);
	src_index++;

	/*
	 * In case of authentication verify operation, use input mac data to
	 * SPU2 engine.
	 */
	if (auth_op == RTE_CRYPTO_AUTH_OP_VERIFY && mac != NULL) {
		sreq->msgs.srcs_addr[src_index] = fsattr_pa(mac);
		sreq->msgs.srcs_len[src_index] = fsattr_sz(mac);
		src_index++;
	}
	sreq->msgs.srcs_count = src_index;

	/*
	 * Output packet contains actual output from SPU2 and
	 * the status packet, so the dsts_count is always 2  below.
	 */
	if (auth_op == RTE_CRYPTO_AUTH_OP_GENERATE) {
		sreq->msgs.dsts_addr[0] = fsattr_pa(dst);
		sreq->msgs.dsts_len[0] = fsattr_sz(dst);
	} else {
		/*
		 * In case of authentication verify operation, provide dummy
		 * location to SPU2 engine to generate hash. This is needed
		 * because SPU2 generates hash even in case of verify operation.
		 */
		sreq->msgs.dsts_addr[0] = sreq->dptr;
		sreq->msgs.dsts_len[0] = fsattr_sz(mac);
	}

	sreq->msgs.dsts_addr[1] = sreq->rptr;
	sreq->msgs.dsts_len[1] = SPU2_STATUS_LEN;
	sreq->msgs.dsts_count = 2;

	return 0;
}

int
bcmfs_crypto_build_cipher_req(struct bcmfs_sym_request *sreq,
			      enum rte_crypto_cipher_algorithm calgo,
			      enum rte_crypto_cipher_operation cipher_op,
			      struct fsattr *src, struct fsattr *dst,
			      struct fsattr *cipher_key, struct fsattr *iv)
{
	int ret = 0;
	int src_index = 0;
	struct spu2_fmd *fmd;
	uint32_t src_msg_len = 0;
	enum spu2_cipher_mode spu2_ciph_mode = 0;
	enum spu2_cipher_type spu2_ciph_type = SPU2_CIPHER_TYPE_NONE;
	bool is_inbound = (cipher_op == RTE_CRYPTO_CIPHER_OP_DECRYPT);

	if (src == NULL || dst == NULL || iv == NULL)
		return -EINVAL;

	fmd  = &sreq->fmd;

	/* spu2 cipher algorithm and cipher algorithm mode */
	ret = spu2_cipher_xlate(calgo, cipher_key,
				&spu2_ciph_type, &spu2_ciph_mode);
	if (ret)
		return -EINVAL;

	spu2_fmd_ctrl0_write(fmd, is_inbound, SPU2_VAL_NONE,
			     SPU2_PROTO_RESV, spu2_ciph_type, spu2_ciph_mode,
			     SPU2_VAL_NONE, SPU2_VAL_NONE);

	spu2_fmd_ctrl1_write(fmd, SPU2_VAL_NONE, SPU2_VAL_NONE, SPU2_VAL_NONE,
			     fsattr_sz(cipher_key), false, false,
			     SPU2_VAL_NONE, SPU2_VAL_NONE, SPU2_VAL_NONE,
			     fsattr_sz(iv), SPU2_VAL_NONE, SPU2_VAL_NONE,
			     SPU2_VAL_NONE);

	/* Nothing for FMD2 */
	memset(&fmd->ctrl2, 0, sizeof(uint64_t));

	spu2_fmd_ctrl3_write(fmd, fsattr_sz(src));

	/* FMD */
	sreq->msgs.srcs_addr[src_index] = sreq->fptr;
	src_msg_len += sizeof(*fmd);

	/* Start of OMD */
	if (cipher_key != NULL && fsattr_sz(cipher_key) != 0) {
		uint8_t *cipher_buf = (uint8_t *)fmd + src_msg_len;
		if (calgo == RTE_CRYPTO_CIPHER_AES_XTS) {
			uint32_t xts_keylen = fsattr_sz(cipher_key) / 2;
			memcpy(cipher_buf,
			       (uint8_t *)fsattr_va(cipher_key) + xts_keylen,
			       xts_keylen);
			memcpy(cipher_buf + xts_keylen,
			       fsattr_va(cipher_key), xts_keylen);
		} else {
			memcpy(cipher_buf, fsattr_va(cipher_key),
			       fsattr_sz(cipher_key));
		}

		src_msg_len += fsattr_sz(cipher_key);
	}

	if (iv != NULL && fsattr_sz(iv) != 0) {
		memcpy((uint8_t *)fmd + src_msg_len,
		       fsattr_va(iv), fsattr_sz(iv));
		src_msg_len +=  fsattr_sz(iv);
	} /* End of OMD */

	sreq->msgs.srcs_len[src_index] = src_msg_len;
	src_index++;

	sreq->msgs.srcs_addr[src_index] = fsattr_pa(src);
	sreq->msgs.srcs_len[src_index] = fsattr_sz(src);
	src_index++;
	sreq->msgs.srcs_count = src_index;

	/**
	 * Output packet contains actual output from SPU2 and
	 * the status packet, so the dsts_count is always 2  below.
	 */
	sreq->msgs.dsts_addr[0] = fsattr_pa(dst);
	sreq->msgs.dsts_len[0] = fsattr_sz(dst);

	sreq->msgs.dsts_addr[1] = sreq->rptr;
	sreq->msgs.dsts_len[1] = SPU2_STATUS_LEN;
	sreq->msgs.dsts_count = 2;

	return 0;
}

int
bcmfs_crypto_build_chain_request(struct bcmfs_sym_request *sreq,
				 enum rte_crypto_cipher_algorithm cipher_alg,
				 enum rte_crypto_cipher_operation cipher_op __rte_unused,
				 enum rte_crypto_auth_algorithm auth_alg,
				 enum rte_crypto_auth_operation auth_op,
				 struct fsattr *src, struct fsattr *dst,
				 struct fsattr *cipher_key,
				 struct fsattr *auth_key,
				 struct fsattr *iv, struct fsattr *aad,
				 struct fsattr *digest, bool cipher_first)
{
	int ret = 0;
	int src_index = 0;
	int dst_index = 0;
	bool auth_first = 0;
	struct spu2_fmd *fmd;
	uint64_t payload_len;
	uint32_t src_msg_len = 0;
	enum spu2_cipher_mode spu2_ciph_mode = 0;
	enum spu2_hash_mode spu2_auth_mode = 0;
	enum spu2_cipher_type spu2_ciph_type = SPU2_CIPHER_TYPE_NONE;
	uint64_t auth_ksize = (auth_key != NULL) ?
				fsattr_sz(auth_key) : 0;
	uint64_t cipher_ksize = (cipher_key != NULL) ?
					fsattr_sz(cipher_key) : 0;
	uint64_t iv_size = (iv != NULL) ? fsattr_sz(iv) : 0;
	uint64_t digest_size = (digest != NULL) ?
					fsattr_sz(digest) : 0;
	uint64_t aad_size = (aad != NULL) ?
				fsattr_sz(aad) : 0;
	enum spu2_hash_type spu2_auth_type = SPU2_HASH_TYPE_NONE;
	bool is_inbound = (auth_op == RTE_CRYPTO_AUTH_OP_VERIFY);

	if (src == NULL)
		return -EINVAL;

	payload_len = fsattr_sz(src);
	if (!payload_len) {
		BCMFS_DP_LOG(ERR, "null payload not supported");
		return -EINVAL;
	}

	/* spu2 hash algorithm and hash algorithm mode */
	ret = spu2_hash_xlate(auth_alg, auth_key, &spu2_auth_type,
			      &spu2_auth_mode);
	if (ret)
		return -EINVAL;

	/* spu2 cipher algorithm and cipher algorithm mode */
	ret = spu2_cipher_xlate(cipher_alg, cipher_key, &spu2_ciph_type,
				&spu2_ciph_mode);
	if (ret) {
		BCMFS_DP_LOG(ERR, "cipher xlate error");
		return -EINVAL;
	}

	auth_first = cipher_first ? 0 : 1;

	fmd  = &sreq->fmd;

	spu2_fmd_ctrl0_write(fmd, is_inbound, auth_first, SPU2_PROTO_RESV,
			     spu2_ciph_type, spu2_ciph_mode,
			     spu2_auth_type, spu2_auth_mode);

	spu2_fmd_ctrl1_write(fmd, is_inbound, aad_size, auth_ksize,
			     cipher_ksize, false, false, SPU2_VAL_NONE,
			     SPU2_VAL_NONE, SPU2_VAL_NONE, iv_size,
			     digest_size, false, SPU2_VAL_NONE);

	spu2_fmd_ctrl2_write(fmd, aad_size, auth_ksize, 0,
			     cipher_ksize, iv_size);

	spu2_fmd_ctrl3_write(fmd, payload_len);

	/* FMD */
	sreq->msgs.srcs_addr[src_index] = sreq->fptr;
	src_msg_len += sizeof(*fmd);

	/* Start of OMD */
	if (auth_ksize != 0) {
		memcpy((uint8_t *)fmd + src_msg_len,
		       fsattr_va(auth_key), auth_ksize);
		src_msg_len += auth_ksize;
#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
	BCMFS_DP_HEXDUMP_LOG(DEBUG, "auth key:", fsattr_va(auth_key),
			     auth_ksize);
#endif
	}

	if (cipher_ksize != 0) {
		memcpy((uint8_t *)fmd + src_msg_len,
		       fsattr_va(cipher_key), cipher_ksize);
		src_msg_len += cipher_ksize;

#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
	BCMFS_DP_HEXDUMP_LOG(DEBUG, "cipher key:", fsattr_va(cipher_key),
			     cipher_ksize);
#endif
	}

	if (iv_size != 0) {
		memcpy((uint8_t *)fmd + src_msg_len,
		       fsattr_va(iv), iv_size);
		src_msg_len += iv_size;
#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
		BCMFS_DP_HEXDUMP_LOG(DEBUG, "iv key:", fsattr_va(iv),
				     iv_size);
#endif
	} /* End of OMD */

	sreq->msgs.srcs_len[src_index] = src_msg_len;

	if (aad_size != 0) {
		if (fsattr_sz(aad) < BCMFS_AAD_THRESH_LEN) {
			memcpy((uint8_t *)fmd + src_msg_len, fsattr_va(aad), aad_size);
			sreq->msgs.srcs_len[src_index] += aad_size;
		} else {
			src_index++;
			sreq->msgs.srcs_addr[src_index] = fsattr_pa(aad);
			sreq->msgs.srcs_len[src_index] = aad_size;
		}
#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
		BCMFS_DP_HEXDUMP_LOG(DEBUG, "aad :", fsattr_va(aad),
				     aad_size);
#endif
	}

	src_index++;

	sreq->msgs.srcs_addr[src_index] = fsattr_pa(src);
	sreq->msgs.srcs_len[src_index] = fsattr_sz(src);
	src_index++;

	if (auth_op == RTE_CRYPTO_AUTH_OP_VERIFY && digest != NULL &&
	    fsattr_sz(digest) != 0) {
		sreq->msgs.srcs_addr[src_index] = fsattr_pa(digest);
		sreq->msgs.srcs_len[src_index] = fsattr_sz(digest);
		src_index++;
	}
	sreq->msgs.srcs_count = src_index;

	if (dst != NULL) {
		sreq->msgs.dsts_addr[dst_index] = fsattr_pa(dst);
		sreq->msgs.dsts_len[dst_index] = fsattr_sz(dst);
		dst_index++;
	}

	if (auth_op == RTE_CRYPTO_AUTH_OP_VERIFY) {
		/*
		 * In case of decryption digest data is generated by
		 * SPU2 engine  but application doesn't need digest
		 * as such. So program dummy location to capture
		 * digest data
		 */
		if (digest_size != 0) {
			sreq->msgs.dsts_addr[dst_index] =
				sreq->dptr;
			sreq->msgs.dsts_len[dst_index] =
				fsattr_sz(digest);
			dst_index++;
		}
	} else {
		if (digest_size != 0) {
			sreq->msgs.dsts_addr[dst_index] =
				fsattr_pa(digest);
			sreq->msgs.dsts_len[dst_index] =
				fsattr_sz(digest);
			dst_index++;
		}
	}

	sreq->msgs.dsts_addr[dst_index] = sreq->rptr;
	sreq->msgs.dsts_len[dst_index] = SPU2_STATUS_LEN;
	dst_index++;
	sreq->msgs.dsts_count = dst_index;

	return 0;
}

static void
bcmfs_crypto_ccm_update_iv(uint8_t *ivbuf,
			   uint64_t *ivlen, bool is_esp)
{
	int L;  /* size of length field, in bytes */

	/*
	 * In RFC4309 mode, L is fixed at 4 bytes; otherwise, IV from
	 * testmgr contains (L-1) in bottom 3 bits of first byte,
	 * per RFC 3610.
	 */
	if (is_esp)
		L = CCM_ESP_L_VALUE;
	else
		L = ((ivbuf[0] & CCM_B0_L_PRIME) >>
		      CCM_B0_L_PRIME_SHIFT) + 1;

	/* SPU2 doesn't want these length bytes nor the first byte... */
	*ivlen -= (1 + L);
	memmove(ivbuf, &ivbuf[1], *ivlen);
}

int
bcmfs_crypto_build_aead_request(struct bcmfs_sym_request *sreq,
				enum rte_crypto_aead_algorithm ae_algo,
				enum rte_crypto_aead_operation aeop,
				struct fsattr *src, struct fsattr *dst,
				struct fsattr *key, struct fsattr *iv,
				struct fsattr *aad, struct fsattr *digest)
{
	int src_index = 0;
	int dst_index = 0;
	bool auth_first = 0;
	struct spu2_fmd *fmd;
	uint64_t payload_len;
	uint32_t src_msg_len = 0;
	uint8_t iv_buf[BCMFS_MAX_IV_SIZE];
	enum spu2_cipher_mode spu2_ciph_mode = 0;
	enum spu2_hash_mode spu2_auth_mode = 0;
	enum spu2_cipher_type spu2_ciph_type = SPU2_CIPHER_TYPE_NONE;
	enum spu2_hash_type spu2_auth_type = SPU2_HASH_TYPE_NONE;
	uint64_t ksize = (key != NULL) ? fsattr_sz(key) : 0;
	uint64_t iv_size = (iv != NULL) ? fsattr_sz(iv) : 0;
	uint64_t aad_size = (aad != NULL) ? fsattr_sz(aad) : 0;
	uint64_t digest_size = (digest != NULL) ?
				fsattr_sz(digest) : 0;
	bool is_inbound = (aeop == RTE_CRYPTO_AEAD_OP_DECRYPT);

	if (src == NULL)
		return -EINVAL;

	payload_len = fsattr_sz(src);
	if (!payload_len) {
		BCMFS_DP_LOG(ERR, "null payload not supported");
		return -EINVAL;
	}

	switch (ksize) {
	case BCMFS_CRYPTO_AES128:
		spu2_auth_type = SPU2_HASH_TYPE_AES128;
		spu2_ciph_type = SPU2_CIPHER_TYPE_AES128;
		break;
	case BCMFS_CRYPTO_AES192:
		spu2_auth_type = SPU2_HASH_TYPE_AES192;
		spu2_ciph_type = SPU2_CIPHER_TYPE_AES192;
		break;
	case BCMFS_CRYPTO_AES256:
		spu2_auth_type = SPU2_HASH_TYPE_AES256;
		spu2_ciph_type = SPU2_CIPHER_TYPE_AES256;
		break;
	default:
		return -EINVAL;
	}

	if (ae_algo == RTE_CRYPTO_AEAD_AES_GCM) {
		spu2_auth_mode = SPU2_HASH_MODE_GCM;
		spu2_ciph_mode = SPU2_CIPHER_MODE_GCM;
		/*
		 * SPU2 needs in total 12 bytes of IV
		 * ie IV of 8 bytes(random number) and 4 bytes of salt.
		 */
		if (fsattr_sz(iv) > 12)
			iv_size = 12;

		/*
		 * On SPU 2, aes gcm cipher first on encrypt, auth first on
		 * decrypt
		 */

		auth_first = (aeop == RTE_CRYPTO_AEAD_OP_ENCRYPT) ?
				0 : 1;
	}

	if (iv_size != 0)
		memcpy(iv_buf, fsattr_va(iv), iv_size);

	if (ae_algo == RTE_CRYPTO_AEAD_AES_CCM) {
		spu2_auth_mode = SPU2_HASH_MODE_CCM;
		spu2_ciph_mode = SPU2_CIPHER_MODE_CCM;
		if (iv_size != 0)  {
			memcpy(iv_buf, fsattr_va(iv),
			       iv_size);
			bcmfs_crypto_ccm_update_iv(iv_buf, &iv_size, false);
		}

		/* opposite for ccm (auth 1st on encrypt) */
		auth_first = (aeop == RTE_CRYPTO_AEAD_OP_ENCRYPT) ?
			      0 : 1;
	}

	fmd  = &sreq->fmd;

	spu2_fmd_ctrl0_write(fmd, is_inbound, auth_first, SPU2_PROTO_RESV,
			     spu2_ciph_type, spu2_ciph_mode,
			     spu2_auth_type, spu2_auth_mode);

	spu2_fmd_ctrl1_write(fmd, is_inbound, aad_size, 0,
			     ksize, false, false, SPU2_VAL_NONE,
			     SPU2_VAL_NONE, SPU2_VAL_NONE, iv_size,
			     digest_size, false, SPU2_VAL_NONE);

	spu2_fmd_ctrl2_write(fmd, aad_size, 0, 0,
			     ksize, iv_size);

	spu2_fmd_ctrl3_write(fmd, payload_len);

	/* FMD */
	sreq->msgs.srcs_addr[src_index] = sreq->fptr;
	src_msg_len += sizeof(*fmd);

	if (ksize) {
		memcpy((uint8_t *)fmd + src_msg_len,
		       fsattr_va(key), ksize);
		src_msg_len += ksize;

#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
	BCMFS_DP_HEXDUMP_LOG(DEBUG, "cipher key:", fsattr_va(key),
			     ksize);
#endif
	}

	if (iv_size) {
		memcpy((uint8_t *)fmd + src_msg_len, iv_buf, iv_size);
		src_msg_len += iv_size;

#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
		BCMFS_DP_HEXDUMP_LOG(DEBUG, "iv key:", fsattr_va(iv),
				     fsattr_sz(iv));
#endif
	} /* End of OMD */

	sreq->msgs.srcs_len[src_index] = src_msg_len;

	if (aad_size != 0) {
		if (aad_size < BCMFS_AAD_THRESH_LEN) {
			memcpy((uint8_t *)fmd + src_msg_len, fsattr_va(aad), aad_size);
			sreq->msgs.srcs_len[src_index] += aad_size;
		} else {
			src_index++;
			sreq->msgs.srcs_addr[src_index] = fsattr_pa(aad);
			sreq->msgs.srcs_len[src_index] = aad_size;
		}
#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
		BCMFS_DP_HEXDUMP_LOG(DEBUG, "aad :", fsattr_va(aad),
				     aad_size);
#endif
	}

	src_index++;

	sreq->msgs.srcs_addr[src_index] = fsattr_pa(src);
	sreq->msgs.srcs_len[src_index] = fsattr_sz(src);
	src_index++;

	if (aeop == RTE_CRYPTO_AEAD_OP_DECRYPT && digest != NULL &&
	    fsattr_sz(digest) != 0) {
		sreq->msgs.srcs_addr[src_index] = fsattr_pa(digest);
		sreq->msgs.srcs_len[src_index] = fsattr_sz(digest);
		src_index++;
	}
	sreq->msgs.srcs_count = src_index;

	if (dst != NULL) {
		sreq->msgs.dsts_addr[dst_index] = fsattr_pa(dst);
		sreq->msgs.dsts_len[dst_index] = fsattr_sz(dst);
		dst_index++;
	}

	if (aeop == RTE_CRYPTO_AEAD_OP_DECRYPT) {
		/*
		 * In case of decryption digest data is generated by
		 * SPU2 engine but application doesn't need digest
		 * as such. So program dummy location to capture
		 * digest data
		 */
		if (digest_size != 0) {
			sreq->msgs.dsts_addr[dst_index] =
				sreq->dptr;
			sreq->msgs.dsts_len[dst_index] =
				digest_size;
			dst_index++;
		}
	} else {
		if (digest_size != 0) {
			sreq->msgs.dsts_addr[dst_index] =
				fsattr_pa(digest);
			sreq->msgs.dsts_len[dst_index] =
				digest_size;
			dst_index++;
		}
	}

	sreq->msgs.dsts_addr[dst_index] = sreq->rptr;
	sreq->msgs.dsts_len[dst_index] = SPU2_STATUS_LEN;
	dst_index++;
	sreq->msgs.dsts_count = dst_index;

	return 0;
}
