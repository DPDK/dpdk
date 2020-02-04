/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#ifndef __OTX2_IPSEC_FP_H__
#define __OTX2_IPSEC_FP_H__

#include <rte_crypto_sym.h>
#include <rte_security.h>

enum {
	OTX2_IPSEC_FP_SA_DIRECTION_INBOUND = 0,
	OTX2_IPSEC_FP_SA_DIRECTION_OUTBOUND = 1,
};

enum {
	OTX2_IPSEC_FP_SA_IP_VERSION_4 = 0,
	OTX2_IPSEC_FP_SA_IP_VERSION_6 = 1,
};

enum {
	OTX2_IPSEC_FP_SA_MODE_TRANSPORT = 0,
	OTX2_IPSEC_FP_SA_MODE_TUNNEL = 1,
};

enum {
	OTX2_IPSEC_FP_SA_PROTOCOL_AH = 0,
	OTX2_IPSEC_FP_SA_PROTOCOL_ESP = 1,
};

enum {
	OTX2_IPSEC_FP_SA_AES_KEY_LEN_128 = 1,
	OTX2_IPSEC_FP_SA_AES_KEY_LEN_192 = 2,
	OTX2_IPSEC_FP_SA_AES_KEY_LEN_256 = 3,
};

enum {
	OTX2_IPSEC_FP_SA_ENC_NULL = 0,
	OTX2_IPSEC_FP_SA_ENC_DES_CBC = 1,
	OTX2_IPSEC_FP_SA_ENC_3DES_CBC = 2,
	OTX2_IPSEC_FP_SA_ENC_AES_CBC = 3,
	OTX2_IPSEC_FP_SA_ENC_AES_CTR = 4,
	OTX2_IPSEC_FP_SA_ENC_AES_GCM = 5,
	OTX2_IPSEC_FP_SA_ENC_AES_CCM = 6,
};

enum {
	OTX2_IPSEC_FP_SA_AUTH_NULL = 0,
	OTX2_IPSEC_FP_SA_AUTH_MD5 = 1,
	OTX2_IPSEC_FP_SA_AUTH_SHA1 = 2,
	OTX2_IPSEC_FP_SA_AUTH_SHA2_224 = 3,
	OTX2_IPSEC_FP_SA_AUTH_SHA2_256 = 4,
	OTX2_IPSEC_FP_SA_AUTH_SHA2_384 = 5,
	OTX2_IPSEC_FP_SA_AUTH_SHA2_512 = 6,
	OTX2_IPSEC_FP_SA_AUTH_AES_GMAC = 7,
	OTX2_IPSEC_FP_SA_AUTH_AES_XCBC_128 = 8,
};

enum {
	OTX2_IPSEC_FP_SA_FRAG_POST = 0,
	OTX2_IPSEC_FP_SA_FRAG_PRE = 1,
};

enum {
	OTX2_IPSEC_FP_SA_ENCAP_NONE = 0,
	OTX2_IPSEC_FP_SA_ENCAP_UDP = 1,
};

struct otx2_ipsec_fp_sa_ctl {
	rte_be32_t spi          : 32;
	uint64_t exp_proto_inter_frag : 8;
	uint64_t rsvd_42_40   : 3;
	uint64_t esn_en       : 1;
	uint64_t rsvd_45_44   : 2;
	uint64_t encap_type   : 2;
	uint64_t enc_type     : 3;
	uint64_t rsvd_48      : 1;
	uint64_t auth_type    : 4;
	uint64_t valid        : 1;
	uint64_t direction    : 1;
	uint64_t outer_ip_ver : 1;
	uint64_t inner_ip_ver : 1;
	uint64_t ipsec_mode   : 1;
	uint64_t ipsec_proto  : 1;
	uint64_t aes_key_len  : 2;
};

struct otx2_ipsec_fp_out_sa {
	/* w0 */
	struct otx2_ipsec_fp_sa_ctl ctl;

	/* w1 */
	uint8_t nonce[4];
	uint16_t udp_src;
	uint16_t udp_dst;

	/* w2 */
	uint32_t ip_src;
	uint32_t ip_dst;

	/* w3-w6 */
	uint8_t cipher_key[32];

	/* w7-w12 */
	uint8_t hmac_key[48];
};

struct otx2_ipsec_fp_in_sa {
	/* w0 */
	struct otx2_ipsec_fp_sa_ctl ctl;

	/* w1 */
	uint8_t nonce[4]; /* Only for AES-GCM */
	uint32_t unused;

	/* w2 */
	uint32_t esn_low;
	uint32_t esn_hi;

	/* w3-w6 */
	uint8_t cipher_key[32];

	/* w7-w12 */
	uint8_t hmac_key[48];

	RTE_STD_C11
	union {
		void *userdata;
		uint64_t udata64;
	};

	uint64_t reserved1;
	uint64_t reserved2;
};

static inline int
ipsec_fp_xform_cipher_verify(struct rte_crypto_sym_xform *xform)
{
	if (xform->cipher.algo == RTE_CRYPTO_CIPHER_AES_CBC) {
		switch (xform->cipher.key.length) {
		case 16:
		case 24:
		case 32:
			break;
		default:
			return -ENOTSUP;
		}
		return 0;
	}

	return -ENOTSUP;
}

static inline int
ipsec_fp_xform_auth_verify(struct rte_crypto_sym_xform *xform)
{
	uint16_t keylen = xform->auth.key.length;

	if (xform->auth.algo == RTE_CRYPTO_AUTH_SHA1_HMAC) {
		if (keylen >= 20 && keylen <= 64)
			return 0;
	}

	return -ENOTSUP;
}

static inline int
ipsec_fp_xform_aead_verify(struct rte_security_ipsec_xform *ipsec,
			   struct rte_crypto_sym_xform *xform)
{
	if (ipsec->direction == RTE_SECURITY_IPSEC_SA_DIR_EGRESS &&
	    xform->aead.op != RTE_CRYPTO_AEAD_OP_ENCRYPT)
		return -EINVAL;

	if (ipsec->direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS &&
	    xform->aead.op != RTE_CRYPTO_AEAD_OP_DECRYPT)
		return -EINVAL;

	if (xform->aead.algo == RTE_CRYPTO_AEAD_AES_GCM) {
		switch (xform->aead.key.length) {
		case 16:
		case 24:
		case 32:
			break;
		default:
			return -EINVAL;
		}
		return 0;
	}

	return -ENOTSUP;
}

static inline int
ipsec_fp_xform_verify(struct rte_security_ipsec_xform *ipsec,
		      struct rte_crypto_sym_xform *xform)
{
	struct rte_crypto_sym_xform *auth_xform, *cipher_xform;
	int ret;

	if (xform->type == RTE_CRYPTO_SYM_XFORM_AEAD)
		return ipsec_fp_xform_aead_verify(ipsec, xform);

	if (xform->next == NULL)
		return -EINVAL;

	if (ipsec->direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS) {
		/* Ingress */
		if (xform->type != RTE_CRYPTO_SYM_XFORM_AUTH ||
		    xform->next->type != RTE_CRYPTO_SYM_XFORM_CIPHER)
			return -EINVAL;
		auth_xform = xform;
		cipher_xform = xform->next;
	} else {
		/* Egress */
		if (xform->type != RTE_CRYPTO_SYM_XFORM_CIPHER ||
		    xform->next->type != RTE_CRYPTO_SYM_XFORM_AUTH)
			return -EINVAL;
		cipher_xform = xform;
		auth_xform = xform->next;
	}

	ret = ipsec_fp_xform_cipher_verify(cipher_xform);
	if (ret)
		return ret;

	ret = ipsec_fp_xform_auth_verify(auth_xform);
	if (ret)
		return ret;

	return 0;
}

static inline int
ipsec_fp_sa_ctl_set(struct rte_security_ipsec_xform *ipsec,
		    struct rte_crypto_sym_xform *xform,
		    struct otx2_ipsec_fp_sa_ctl *ctl)
{
	struct rte_crypto_sym_xform *cipher_xform, *auth_xform;
	int aes_key_len;

	if (ipsec->direction == RTE_SECURITY_IPSEC_SA_DIR_EGRESS) {
		ctl->direction = OTX2_IPSEC_FP_SA_DIRECTION_OUTBOUND;
		cipher_xform = xform;
		auth_xform = xform->next;
	} else if (ipsec->direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS) {
		ctl->direction = OTX2_IPSEC_FP_SA_DIRECTION_INBOUND;
		auth_xform = xform;
		cipher_xform = xform->next;
	} else {
		return -EINVAL;
	}

	if (ipsec->mode == RTE_SECURITY_IPSEC_SA_MODE_TUNNEL) {
		if (ipsec->tunnel.type == RTE_SECURITY_IPSEC_TUNNEL_IPV4)
			ctl->outer_ip_ver = OTX2_IPSEC_FP_SA_IP_VERSION_4;
		else if (ipsec->tunnel.type == RTE_SECURITY_IPSEC_TUNNEL_IPV6)
			ctl->outer_ip_ver = OTX2_IPSEC_FP_SA_IP_VERSION_6;
		else
			return -EINVAL;
	}

	ctl->inner_ip_ver = OTX2_IPSEC_FP_SA_IP_VERSION_4;

	if (ipsec->mode == RTE_SECURITY_IPSEC_SA_MODE_TRANSPORT)
		ctl->ipsec_mode = OTX2_IPSEC_FP_SA_MODE_TRANSPORT;
	else if (ipsec->mode == RTE_SECURITY_IPSEC_SA_MODE_TUNNEL)
		ctl->ipsec_mode = OTX2_IPSEC_FP_SA_MODE_TUNNEL;
	else
		return -EINVAL;

	if (ipsec->proto == RTE_SECURITY_IPSEC_SA_PROTO_AH)
		ctl->ipsec_proto = OTX2_IPSEC_FP_SA_PROTOCOL_AH;
	else if (ipsec->proto == RTE_SECURITY_IPSEC_SA_PROTO_ESP)
		ctl->ipsec_proto = OTX2_IPSEC_FP_SA_PROTOCOL_ESP;
	else
		return -EINVAL;

	if (xform->type == RTE_CRYPTO_SYM_XFORM_AEAD) {
		if (xform->aead.algo == RTE_CRYPTO_AEAD_AES_GCM) {
			ctl->enc_type = OTX2_IPSEC_FP_SA_ENC_AES_GCM;
			aes_key_len = xform->aead.key.length;
		} else {
			return -ENOTSUP;
		}
	} else if (cipher_xform->cipher.algo == RTE_CRYPTO_CIPHER_AES_CBC) {
		ctl->enc_type = OTX2_IPSEC_FP_SA_ENC_AES_CBC;
		aes_key_len = cipher_xform->cipher.key.length;
	} else {
		return -ENOTSUP;
	}

	switch (aes_key_len) {
	case 16:
		ctl->aes_key_len = OTX2_IPSEC_FP_SA_AES_KEY_LEN_128;
		break;
	case 24:
		ctl->aes_key_len = OTX2_IPSEC_FP_SA_AES_KEY_LEN_192;
		break;
	case 32:
		ctl->aes_key_len = OTX2_IPSEC_FP_SA_AES_KEY_LEN_256;
		break;
	default:
		return -EINVAL;
	}

	if (xform->type != RTE_CRYPTO_SYM_XFORM_AEAD) {
		switch (auth_xform->auth.algo) {
		case RTE_CRYPTO_AUTH_NULL:
			ctl->auth_type = OTX2_IPSEC_FP_SA_AUTH_NULL;
			break;
		case RTE_CRYPTO_AUTH_MD5_HMAC:
			ctl->auth_type = OTX2_IPSEC_FP_SA_AUTH_MD5;
			break;
		case RTE_CRYPTO_AUTH_SHA1_HMAC:
			ctl->auth_type = OTX2_IPSEC_FP_SA_AUTH_SHA1;
			break;
		case RTE_CRYPTO_AUTH_SHA224_HMAC:
			ctl->auth_type = OTX2_IPSEC_FP_SA_AUTH_SHA2_224;
			break;
		case RTE_CRYPTO_AUTH_SHA256_HMAC:
			ctl->auth_type = OTX2_IPSEC_FP_SA_AUTH_SHA2_256;
			break;
		case RTE_CRYPTO_AUTH_SHA384_HMAC:
			ctl->auth_type = OTX2_IPSEC_FP_SA_AUTH_SHA2_384;
			break;
		case RTE_CRYPTO_AUTH_SHA512_HMAC:
			ctl->auth_type = OTX2_IPSEC_FP_SA_AUTH_SHA2_512;
			break;
		case RTE_CRYPTO_AUTH_AES_GMAC:
			ctl->auth_type = OTX2_IPSEC_FP_SA_AUTH_AES_GMAC;
			break;
		case RTE_CRYPTO_AUTH_AES_XCBC_MAC:
			ctl->auth_type = OTX2_IPSEC_FP_SA_AUTH_AES_XCBC_128;
			break;
		default:
			return -ENOTSUP;
		}
	}

	if (ipsec->options.esn == 1)
		ctl->esn_en = 1;

	ctl->spi = rte_cpu_to_be_32(ipsec->spi);
	ctl->valid = 1;

	return 0;
}

#endif /* __OTX2_IPSEC_FP_H__ */
