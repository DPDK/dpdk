/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#ifndef __OTX2_IPSEC_PO_H__
#define __OTX2_IPSEC_PO_H__

#include <rte_crypto_sym.h>
#include <rte_ip.h>
#include <rte_security.h>

#define OTX2_IPSEC_PO_AES_GCM_INB_CTX_LEN    0x09
#define OTX2_IPSEC_PO_AES_GCM_OUTB_CTX_LEN   0x28

#define OTX2_IPSEC_PO_MAX_INB_CTX_LEN    0x22
#define OTX2_IPSEC_PO_MAX_OUTB_CTX_LEN   0x38

#define OTX2_IPSEC_PO_WRITE_IPSEC_OUTB     0x20
#define OTX2_IPSEC_PO_WRITE_IPSEC_INB      0x21
#define OTX2_IPSEC_PO_PROCESS_IPSEC_OUTB   0x23
#define OTX2_IPSEC_PO_PROCESS_IPSEC_INB    0x24

#define OTX2_IPSEC_PO_INB_RPTR_HDR         0x8

enum otx2_ipsec_po_comp_e {
	OTX2_IPSEC_PO_CC_SUCCESS = 0x00,
	OTX2_IPSEC_PO_CC_AUTH_UNSUPPORTED = 0xB0,
	OTX2_IPSEC_PO_CC_ENCRYPT_UNSUPPORTED = 0xB1,
};

enum {
	OTX2_IPSEC_PO_SA_DIRECTION_INBOUND = 0,
	OTX2_IPSEC_PO_SA_DIRECTION_OUTBOUND = 1,
};

enum {
	OTX2_IPSEC_PO_SA_IP_VERSION_4 = 0,
	OTX2_IPSEC_PO_SA_IP_VERSION_6 = 1,
};

enum {
	OTX2_IPSEC_PO_SA_MODE_TRANSPORT = 0,
	OTX2_IPSEC_PO_SA_MODE_TUNNEL = 1,
};

enum {
	OTX2_IPSEC_PO_SA_PROTOCOL_AH = 0,
	OTX2_IPSEC_PO_SA_PROTOCOL_ESP = 1,
};

enum {
	OTX2_IPSEC_PO_SA_AES_KEY_LEN_128 = 1,
	OTX2_IPSEC_PO_SA_AES_KEY_LEN_192 = 2,
	OTX2_IPSEC_PO_SA_AES_KEY_LEN_256 = 3,
};

enum {
	OTX2_IPSEC_PO_SA_ENC_NULL = 0,
	OTX2_IPSEC_PO_SA_ENC_DES_CBC = 1,
	OTX2_IPSEC_PO_SA_ENC_3DES_CBC = 2,
	OTX2_IPSEC_PO_SA_ENC_AES_CBC = 3,
	OTX2_IPSEC_PO_SA_ENC_AES_CTR = 4,
	OTX2_IPSEC_PO_SA_ENC_AES_GCM = 5,
	OTX2_IPSEC_PO_SA_ENC_AES_CCM = 6,
};

enum {
	OTX2_IPSEC_PO_SA_AUTH_NULL = 0,
	OTX2_IPSEC_PO_SA_AUTH_MD5 = 1,
	OTX2_IPSEC_PO_SA_AUTH_SHA1 = 2,
	OTX2_IPSEC_PO_SA_AUTH_SHA2_224 = 3,
	OTX2_IPSEC_PO_SA_AUTH_SHA2_256 = 4,
	OTX2_IPSEC_PO_SA_AUTH_SHA2_384 = 5,
	OTX2_IPSEC_PO_SA_AUTH_SHA2_512 = 6,
	OTX2_IPSEC_PO_SA_AUTH_AES_GMAC = 7,
	OTX2_IPSEC_PO_SA_AUTH_AES_XCBC_128 = 8,
};

enum {
	OTX2_IPSEC_PO_SA_FRAG_POST = 0,
	OTX2_IPSEC_PO_SA_FRAG_PRE = 1,
};

enum {
	OTX2_IPSEC_PO_SA_ENCAP_NONE = 0,
	OTX2_IPSEC_PO_SA_ENCAP_UDP = 1,
};

struct otx2_ipsec_po_out_hdr {
	uint32_t ip_id;
	uint32_t seq;
	uint8_t iv[16];
};

union otx2_ipsec_po_bit_perfect_iv {
	uint8_t aes_iv[16];
	uint8_t des_iv[8];
	struct {
		uint8_t nonce[4];
		uint8_t iv[8];
		uint8_t counter[4];
	} gcm;
};

struct otx2_ipsec_po_traffic_selector {
	rte_be16_t src_port[2];
	rte_be16_t dst_port[2];
	RTE_STD_C11
	union {
		struct {
			rte_be32_t src_addr[2];
			rte_be32_t dst_addr[2];
		} ipv4;
		struct {
			uint8_t src_addr[32];
			uint8_t dst_addr[32];
		} ipv6;
	};
};

struct otx2_ipsec_po_sa_ctl {
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

struct otx2_ipsec_po_in_sa {
	/* w0 */
	struct otx2_ipsec_po_sa_ctl ctl;

	/* w1-w4 */
	uint8_t cipher_key[32];

	/* w5-w6 */
	union otx2_ipsec_po_bit_perfect_iv iv;

	/* w7 */
	uint32_t esn_hi;
	uint32_t esn_low;

	/* w8 */
	uint8_t udp_encap[8];

	/* w9-w23 */
	struct {
		uint8_t hmac_key[48];
		struct otx2_ipsec_po_traffic_selector selector;
	} aes_gcm;
};

struct otx2_ipsec_po_ip_template {
	RTE_STD_C11
	union {
		uint8_t raw[252];
		struct rte_ipv4_hdr ipv4_hdr;
		struct rte_ipv6_hdr ipv6_hdr;
	};
};

struct otx2_ipsec_po_out_sa {
	/* w0 */
	struct otx2_ipsec_po_sa_ctl ctl;

	/* w1-w4 */
	uint8_t cipher_key[32];

	/* w5-w6 */
	union otx2_ipsec_po_bit_perfect_iv iv;

	/* w7 */
	uint32_t esn_hi;
	uint32_t esn_low;

	/* w8-w39 */
	struct otx2_ipsec_po_ip_template template;
	uint16_t udp_src;
	uint16_t udp_dst;
};

static inline int
ipsec_po_xform_cipher_verify(struct rte_crypto_sym_xform *xform)
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
ipsec_po_xform_auth_verify(struct rte_crypto_sym_xform *xform)
{
	uint16_t keylen = xform->auth.key.length;

	if (xform->auth.algo == RTE_CRYPTO_AUTH_SHA1_HMAC) {
		if (keylen >= 20 && keylen <= 64)
			return 0;
	}

	return -ENOTSUP;
}

static inline int
ipsec_po_xform_aead_verify(struct rte_security_ipsec_xform *ipsec,
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
ipsec_po_xform_verify(struct rte_security_ipsec_xform *ipsec,
		      struct rte_crypto_sym_xform *xform)
{
	struct rte_crypto_sym_xform *auth_xform, *cipher_xform;
	int ret;

	if (xform->type == RTE_CRYPTO_SYM_XFORM_AEAD)
		return ipsec_po_xform_aead_verify(ipsec, xform);

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

	ret = ipsec_po_xform_cipher_verify(cipher_xform);
	if (ret)
		return ret;

	ret = ipsec_po_xform_auth_verify(auth_xform);
	if (ret)
		return ret;

	return 0;
}

static inline int
ipsec_po_sa_ctl_set(struct rte_security_ipsec_xform *ipsec,
		    struct rte_crypto_sym_xform *xform,
		    struct otx2_ipsec_po_sa_ctl *ctl)
{
	struct rte_crypto_sym_xform *cipher_xform, *auth_xform;
	int aes_key_len;

	if (ipsec->direction == RTE_SECURITY_IPSEC_SA_DIR_EGRESS) {
		ctl->direction = OTX2_IPSEC_PO_SA_DIRECTION_OUTBOUND;
		cipher_xform = xform;
		auth_xform = xform->next;
	} else if (ipsec->direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS) {
		ctl->direction = OTX2_IPSEC_PO_SA_DIRECTION_INBOUND;
		auth_xform = xform;
		cipher_xform = xform->next;
	} else {
		return -EINVAL;
	}

	if (ipsec->mode == RTE_SECURITY_IPSEC_SA_MODE_TUNNEL) {
		if (ipsec->tunnel.type == RTE_SECURITY_IPSEC_TUNNEL_IPV4)
			ctl->outer_ip_ver = OTX2_IPSEC_PO_SA_IP_VERSION_4;
		else if (ipsec->tunnel.type == RTE_SECURITY_IPSEC_TUNNEL_IPV6)
			ctl->outer_ip_ver = OTX2_IPSEC_PO_SA_IP_VERSION_6;
		else
			return -EINVAL;
	}

	ctl->inner_ip_ver = ctl->outer_ip_ver;

	if (ipsec->mode == RTE_SECURITY_IPSEC_SA_MODE_TRANSPORT)
		ctl->ipsec_mode = OTX2_IPSEC_PO_SA_MODE_TRANSPORT;
	else if (ipsec->mode == RTE_SECURITY_IPSEC_SA_MODE_TUNNEL)
		ctl->ipsec_mode = OTX2_IPSEC_PO_SA_MODE_TUNNEL;
	else
		return -EINVAL;

	if (ipsec->proto == RTE_SECURITY_IPSEC_SA_PROTO_AH)
		ctl->ipsec_proto = OTX2_IPSEC_PO_SA_PROTOCOL_AH;
	else if (ipsec->proto == RTE_SECURITY_IPSEC_SA_PROTO_ESP)
		ctl->ipsec_proto = OTX2_IPSEC_PO_SA_PROTOCOL_ESP;
	else
		return -EINVAL;

	if (xform->type == RTE_CRYPTO_SYM_XFORM_AEAD) {
		if (xform->aead.algo == RTE_CRYPTO_AEAD_AES_GCM) {
			ctl->enc_type = OTX2_IPSEC_PO_SA_ENC_AES_GCM;
			aes_key_len = xform->aead.key.length;
		} else {
			return -ENOTSUP;
		}
	} else if (cipher_xform->cipher.algo == RTE_CRYPTO_CIPHER_AES_CBC) {
		ctl->enc_type = OTX2_IPSEC_PO_SA_ENC_AES_CCM;
		aes_key_len = xform->cipher.key.length;
	} else {
		return -ENOTSUP;
	}


	switch (aes_key_len) {
	case 16:
		ctl->aes_key_len = OTX2_IPSEC_PO_SA_AES_KEY_LEN_128;
		break;
	case 24:
		ctl->aes_key_len = OTX2_IPSEC_PO_SA_AES_KEY_LEN_192;
		break;
	case 32:
		ctl->aes_key_len = OTX2_IPSEC_PO_SA_AES_KEY_LEN_256;
		break;
	default:
		return -EINVAL;
	}

	if (xform->type != RTE_CRYPTO_SYM_XFORM_AEAD) {
		switch (auth_xform->auth.algo) {
		case RTE_CRYPTO_AUTH_NULL:
			ctl->auth_type = OTX2_IPSEC_PO_SA_AUTH_NULL;
			break;
		case RTE_CRYPTO_AUTH_MD5_HMAC:
			ctl->auth_type = OTX2_IPSEC_PO_SA_AUTH_MD5;
			break;
		case RTE_CRYPTO_AUTH_SHA1_HMAC:
			ctl->auth_type = OTX2_IPSEC_PO_SA_AUTH_SHA1;
			break;
		case RTE_CRYPTO_AUTH_SHA224_HMAC:
			ctl->auth_type = OTX2_IPSEC_PO_SA_AUTH_SHA2_224;
			break;
		case RTE_CRYPTO_AUTH_SHA256_HMAC:
			ctl->auth_type = OTX2_IPSEC_PO_SA_AUTH_SHA2_256;
			break;
		case RTE_CRYPTO_AUTH_SHA384_HMAC:
			ctl->auth_type = OTX2_IPSEC_PO_SA_AUTH_SHA2_384;
			break;
		case RTE_CRYPTO_AUTH_SHA512_HMAC:
			ctl->auth_type = OTX2_IPSEC_PO_SA_AUTH_SHA2_512;
			break;
		case RTE_CRYPTO_AUTH_AES_GMAC:
			ctl->auth_type = OTX2_IPSEC_PO_SA_AUTH_AES_GMAC;
			break;
		case RTE_CRYPTO_AUTH_AES_XCBC_MAC:
			ctl->auth_type = OTX2_IPSEC_PO_SA_AUTH_AES_XCBC_128;
			break;
		default:
			return -ENOTSUP;
		}
	}

	if (ipsec->options.esn)
		ctl->esn_en = 1;

	if (ipsec->options.udp_encap == 1)
		ctl->encap_type = OTX2_IPSEC_PO_SA_ENCAP_UDP;

	ctl->spi = rte_cpu_to_be_32(ipsec->spi);
	ctl->valid = 1;

	return 0;
}

#endif /* __OTX2_IPSEC_PO_H__ */
