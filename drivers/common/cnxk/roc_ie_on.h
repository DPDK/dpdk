/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef __ROC_IE_ON_H__
#define __ROC_IE_ON_H__

/* CN9K IPsec LA */

/* CN9K IPsec LA opcodes */
#define ROC_IE_ON_MAJOR_OP_WRITE_IPSEC_OUTBOUND	  0x20
#define ROC_IE_ON_MAJOR_OP_WRITE_IPSEC_INBOUND	  0x21
#define ROC_IE_ON_MAJOR_OP_PROCESS_OUTBOUND_IPSEC 0x23
#define ROC_IE_ON_MAJOR_OP_PROCESS_INBOUND_IPSEC  0x24

/* Ucode completion codes */
enum roc_ie_on_ucc_ipsec {
	ROC_IE_ON_UCC_SUCCESS = 0,
	ROC_IE_ON_AUTH_UNSUPPORTED = 0xB0,
	ROC_IE_ON_ENCRYPT_UNSUPPORTED = 0xB1,
};

/* Helper macros */
#define ROC_IE_ON_PER_PKT_IV   BIT(11)
#define ROC_IE_ON_INB_RPTR_HDR 0x8

enum {
	ROC_IE_ON_SA_ENC_NULL = 0,
	ROC_IE_ON_SA_ENC_DES_CBC = 1,
	ROC_IE_ON_SA_ENC_3DES_CBC = 2,
	ROC_IE_ON_SA_ENC_AES_CBC = 3,
	ROC_IE_ON_SA_ENC_AES_CTR = 4,
	ROC_IE_ON_SA_ENC_AES_GCM = 5,
	ROC_IE_ON_SA_ENC_AES_CCM = 6,
};

enum {
	ROC_IE_ON_SA_AUTH_NULL = 0,
	ROC_IE_ON_SA_AUTH_MD5 = 1,
	ROC_IE_ON_SA_AUTH_SHA1 = 2,
	ROC_IE_ON_SA_AUTH_SHA2_224 = 3,
	ROC_IE_ON_SA_AUTH_SHA2_256 = 4,
	ROC_IE_ON_SA_AUTH_SHA2_384 = 5,
	ROC_IE_ON_SA_AUTH_SHA2_512 = 6,
	ROC_IE_ON_SA_AUTH_AES_GMAC = 7,
	ROC_IE_ON_SA_AUTH_AES_XCBC_128 = 8,
};

enum {
	ROC_IE_ON_SA_FRAG_POST = 0,
	ROC_IE_ON_SA_FRAG_PRE = 1,
};

enum {
	ROC_IE_ON_SA_ENCAP_NONE = 0,
	ROC_IE_ON_SA_ENCAP_UDP = 1,
};

struct roc_ie_on_outb_hdr {
	uint32_t ip_id;
	uint32_t seq;
	uint8_t iv[16];
};

union roc_ie_on_bit_perfect_iv {
	uint8_t aes_iv[16];
	uint8_t des_iv[8];
	struct {
		uint8_t nonce[4];
		uint8_t iv[8];
		uint8_t counter[4];
	} gcm;
};

struct roc_ie_on_traffic_selector {
	uint16_t src_port[2];
	uint16_t dst_port[2];
	union {
		struct {
			uint32_t src_addr[2];
			uint32_t dst_addr[2];
		} ipv4;
		struct {
			uint8_t src_addr[32];
			uint8_t dst_addr[32];
		} ipv6;
	};
};

struct roc_ie_on_ip_template {
	union {
		struct {
			uint8_t ipv4_hdr[20];
			uint16_t udp_src;
			uint16_t udp_dst;
		} ip4;
		struct {
			uint8_t ipv6_hdr[40];
			uint16_t udp_src;
			uint16_t udp_dst;
		} ip6;
	};
};

struct roc_ie_on_sa_ctl {
	uint64_t spi : 32;
	uint64_t exp_proto_inter_frag : 8;
	uint64_t copy_df : 1;
	uint64_t frag_type : 1;
	uint64_t explicit_iv_en : 1;
	uint64_t esn_en : 1;
	uint64_t rsvd_45_44 : 2;
	uint64_t encap_type : 2;
	uint64_t enc_type : 3;
	uint64_t rsvd_48 : 1;
	uint64_t auth_type : 4;
	uint64_t valid : 1;
	uint64_t direction : 1;
	uint64_t outer_ip_ver : 1;
	uint64_t inner_ip_ver : 1;
	uint64_t ipsec_mode : 1;
	uint64_t ipsec_proto : 1;
	uint64_t aes_key_len : 2;
};

struct roc_ie_on_common_sa {
	/* w0 */
	struct roc_ie_on_sa_ctl ctl;

	/* w1-w4 */
	uint8_t cipher_key[32];

	/* w5-w6 */
	union roc_ie_on_bit_perfect_iv iv;

	/* w7 */
	uint32_t esn_hi;
	uint32_t esn_low;
};

struct roc_ie_on_outb_sa {
	/* w0 - w7 */
	struct roc_ie_on_common_sa common_sa;

	/* w8-w55 */
	union {
		struct {
			struct roc_ie_on_ip_template template;
		} aes_gcm;
		struct {
			uint8_t hmac_key[24];
			uint8_t unused[24];
			struct roc_ie_on_ip_template template;
		} sha1;
		struct {
			uint8_t hmac_key[64];
			uint8_t hmac_iv[64];
			struct roc_ie_on_ip_template template;
		} sha2;
	};
};

struct roc_ie_on_inb_sa {
	/* w0 - w7 */
	struct roc_ie_on_common_sa common_sa;

	/* w8 */
	uint8_t udp_encap[8];

	/* w9-w33 */
	union {
		struct {
			uint8_t hmac_key[48];
			struct roc_ie_on_traffic_selector selector;
		} sha1_or_gcm;
		struct {
			uint8_t hmac_key[64];
			uint8_t hmac_iv[64];
			struct roc_ie_on_traffic_selector selector;
		} sha2;
	};
};

/* CN9K IPsec FP */

/* CN9K IPsec FP opcodes */
#define ROC_IE_ONF_MAJOR_OP_PROCESS_OUTBOUND_IPSEC 0x25UL
#define ROC_IE_ONF_MAJOR_OP_PROCESS_INBOUND_IPSEC  0x26UL

/* Ucode completion codes */
#define ROC_IE_ONF_UCC_SUCCESS 0

struct roc_ie_onf_sa_ctl {
	uint32_t spi;
	uint64_t exp_proto_inter_frag : 8;
	uint64_t rsvd_41_40 : 2;
	/* Disable SPI, SEQ data in RPTR for Inbound inline */
	uint64_t spi_seq_dis : 1;
	uint64_t esn_en : 1;
	uint64_t rsvd_44_45 : 2;
	uint64_t encap_type : 2;
	uint64_t enc_type : 3;
	uint64_t rsvd_48 : 1;
	uint64_t auth_type : 4;
	uint64_t valid : 1;
	uint64_t direction : 1;
	uint64_t outer_ip_ver : 1;
	uint64_t inner_ip_ver : 1;
	uint64_t ipsec_mode : 1;
	uint64_t ipsec_proto : 1;
	uint64_t aes_key_len : 2;
};

struct roc_onf_ipsec_outb_sa {
	/* w0 */
	struct roc_ie_onf_sa_ctl ctl;

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

struct roc_onf_ipsec_inb_sa {
	/* w0 */
	struct roc_ie_onf_sa_ctl ctl;

	/* w1 */
	uint8_t nonce[4]; /* Only for AES-GCM */
	uint32_t unused;

	/* w2 */
	uint32_t esn_hi;
	uint32_t esn_low;

	/* w3-w6 */
	uint8_t cipher_key[32];

	/* w7-w12 */
	uint8_t hmac_key[48];
};

#define ROC_ONF_IPSEC_INB_MAX_L2_SZ	  32UL
#define ROC_ONF_IPSEC_OUTB_MAX_L2_SZ	  30UL
#define ROC_ONF_IPSEC_OUTB_MAX_L2_INFO_SZ (ROC_ONF_IPSEC_OUTB_MAX_L2_SZ + 2)

#define ROC_ONF_IPSEC_INB_RES_OFF    80
#define ROC_ONF_IPSEC_INB_SPI_SEQ_SZ 16

struct roc_onf_ipsec_outb_hdr {
	uint32_t ip_id;
	uint32_t seq;
	uint8_t iv[16];
};

#endif /* __ROC_IE_ON_H__ */
