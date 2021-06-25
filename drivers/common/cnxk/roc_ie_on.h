/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef __ROC_IE_ON_H__
#define __ROC_IE_ON_H__

/* CN9K IPSEC LA opcodes */
#define ROC_IE_ONL_MAJOR_OP_WRITE_IPSEC_OUTBOUND   0x20
#define ROC_IE_ONL_MAJOR_OP_WRITE_IPSEC_INBOUND	   0x21
#define ROC_IE_ONL_MAJOR_OP_PROCESS_OUTBOUND_IPSEC 0x23
#define ROC_IE_ONL_MAJOR_OP_PROCESS_INBOUND_IPSEC  0x24

/* CN9K IPSEC FP opcodes */
#define ROC_IE_ONF_MAJOR_OP_PROCESS_OUTBOUND_IPSEC 0x25UL
#define ROC_IE_ONF_MAJOR_OP_PROCESS_INBOUND_IPSEC  0x26UL

/* Ucode completion codes */
#define ROC_IE_ONF_UCC_SUCCESS 0

enum {
	ROC_IE_ON_SA_DIR_INBOUND = 0,
	ROC_IE_ON_SA_DIR_OUTBOUND = 1,
};

enum {
	ROC_IE_ON_SA_IP_VERSION_4 = 0,
	ROC_IE_ON_SA_IP_VERSION_6 = 1,
};

enum {
	ROC_IE_ON_SA_MODE_TRANSPORT = 0,
	ROC_IE_ON_SA_MODE_TUNNEL = 1,
};

enum {
	ROC_IE_ON_SA_PROTOCOL_AH = 0,
	ROC_IE_ON_SA_PROTOCOL_ESP = 1,
};

enum {
	ROC_IE_ON_SA_AES_KEY_LEN_128 = 1,
	ROC_IE_ON_SA_AES_KEY_LEN_192 = 2,
	ROC_IE_ON_SA_AES_KEY_LEN_256 = 3,
};

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
