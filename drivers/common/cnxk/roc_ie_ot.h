/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Marvell.
 */

#ifndef __ROC_IE_OT_H__
#define __ROC_IE_OT_H__

/* CN10K IPSEC opcodes */
#define ROC_IE_OT_MAJOR_OP_PROCESS_OUTBOUND_IPSEC 0x28UL
#define ROC_IE_OT_MAJOR_OP_PROCESS_INBOUND_IPSEC  0x29UL

#define ROC_IE_OT_MAJOR_OP_WRITE_SA 0x01UL
#define ROC_IE_OT_MINOR_OP_WRITE_SA 0x09UL

#define ROC_IE_OT_CTX_ILEN 2
/* PKIND to be used for CPT Meta parsing */
#define ROC_IE_OT_CPT_PKIND	  58
#define ROC_IE_OT_SA_CTX_HDR_SIZE 1

enum roc_ie_ot_ucc_ipsec {
	ROC_IE_OT_UCC_SUCCESS = 0x00,
	ROC_IE_OT_UCC_ERR_SA_INVAL = 0xb0,
	ROC_IE_OT_UCC_ERR_SA_EXPIRED = 0xb1,
	ROC_IE_OT_UCC_ERR_SA_OVERFLOW = 0xb2,
	ROC_IE_OT_UCC_ERR_SA_ESP_BAD_ALGO = 0xb3,
	ROC_IE_OT_UCC_ERR_SA_AH_BAD_ALGO = 0xb4,
	ROC_IE_OT_UCC_ERR_SA_BAD_CTX = 0xb5,
	ROC_IE_OT_UCC_SA_CTX_FLAG_MISMATCH = 0xb6,
	ROC_IE_OT_UCC_ERR_AOP_IPSEC = 0xb7,
	ROC_IE_OT_UCC_ERR_PKT_IP = 0xb8,
	ROC_IE_OT_UCC_ERR_PKT_IP6_BAD_EXT = 0xb9,
	ROC_IE_OT_UCC_ERR_PKT_IP6_HBH = 0xba,
	ROC_IE_OT_UCC_ERR_PKT_IP6_BIGEXT = 0xbb,
	ROC_IE_OT_UCC_ERR_PKT_IP_ULP = 0xbc,
	ROC_IE_OT_UCC_ERR_PKT_SA_MISMATCH = 0xbd,
	ROC_IE_OT_UCC_ERR_PKT_SPI_MISMATCH = 0xbe,
	ROC_IE_OT_UCC_ERR_PKT_ESP_BADPAD = 0xbf,
	ROC_IE_OT_UCC_ERR_PKT_BADICV = 0xc0,
	ROC_IE_OT_UCC_ERR_PKT_REPLAY_SEQ = 0xc1,
	ROC_IE_OT_UCC_ERR_PKT_BADNH = 0xc2,
	ROC_IE_OT_UCC_ERR_PKT_SA_PORT_MISMATCH = 0xc3,
	ROC_IE_OT_UCC_ERR_PKT_BAD_DLEN = 0xc4,
	ROC_IE_OT_UCC_ERR_SA_ESP_BAD_KEYS = 0xc5,
	ROC_IE_OT_UCC_ERR_SA_AH_BAD_KEYS = 0xc6,
	ROC_IE_OT_UCC_ERR_SA_BAD_IP = 0xc7,
	ROC_IE_OT_UCC_ERR_PKT_IP_FRAG = 0xc8,
	ROC_IE_OT_UCC_ERR_PKT_REPLAY_WINDOW = 0xc9,
	ROC_IE_OT_UCC_SUCCESS_SA_SOFTEXP_FIRST = 0xf0,
	ROC_IE_OT_UCC_SUCCESS_PKT_IP_BADCSUM = 0xf1,
	ROC_IE_OT_UCC_SUCCESS_SA_SOFTEXP_AGAIN = 0xf2,
	ROC_IE_OT_UCC_SUCCESS_PKT_L4_GOODCSUM = 0xf3,
	ROC_IE_OT_UCC_SUCCESS_PKT_L4_BADCSUM = 0xf4,
	ROC_IE_OT_UCC_SUCCESS_PKT_UDPESP_NZCSUM = 0xf5,
	ROC_IE_OT_UCC_SUCCESS_PKT_UDP_ZEROCSUM = 0xf6,
	ROC_IE_OT_UCC_SUCCESS_PKT_IP_GOODCSUM = 0xf7,
};

enum {
	ROC_IE_OT_SA_AR_WIN_DISABLED = 0,
	ROC_IE_OT_SA_AR_WIN_64 = 1,
	ROC_IE_OT_SA_AR_WIN_128 = 2,
	ROC_IE_OT_SA_AR_WIN_256 = 3,
	ROC_IE_OT_SA_AR_WIN_512 = 4,
	ROC_IE_OT_SA_AR_WIN_1024 = 5,
	ROC_IE_OT_SA_AR_WIN_2048 = 6,
	ROC_IE_OT_SA_AR_WIN_4096 = 7,
};

enum {
	ROC_IE_OT_SA_PKT_FMT_FULL = 0,
	ROC_IE_OT_SA_PKT_FMT_META = 1,
};

enum {
	ROC_IE_OT_SA_PKT_OUTPUT_DECRYPTED = 0,
	ROC_IE_OT_SA_PKT_OUTPUT_NO_FRAG = 1,
	ROC_IE_OT_SA_PKT_OUTPUT_HW_BASED_DEFRAG = 2,
	ROC_IE_OT_SA_PKT_OUTPUT_UCODE_BASED_DEFRAG = 3,
};

enum {
	ROC_IE_OT_SA_DEFRAG_ALL = 0,
	ROC_IE_OT_SA_DEFRAG_IN_ORDER = 1,
	ROC_IE_OT_SA_DEFRAG_IN_REV_ORDER = 2,
};

enum {
	ROC_IE_OT_SA_IV_SRC_DEFAULT = 0,
	ROC_IE_OT_SA_IV_SRC_ENC_CTR = 1,
	ROC_IE_OT_SA_IV_SRC_FROM_SA = 2,
};

enum {
	ROC_IE_OT_SA_COPY_FROM_SA = 0,
	ROC_IE_OT_SA_COPY_FROM_INNER_IP_HDR = 1,
};

enum {
	ROC_IE_OT_SA_INNER_PKT_IP_CSUM_ENABLE = 0,
	ROC_IE_OT_SA_INNER_PKT_IP_CSUM_DISABLE = 1,
};

enum {
	ROC_IE_OT_SA_INNER_PKT_L4_CSUM_ENABLE = 0,
	ROC_IE_OT_SA_INNER_PKT_L4_CSUM_DISABLE = 1,
};

enum {
	ROC_IE_OT_SA_ENC_NULL = 0,
	ROC_IE_OT_SA_ENC_3DES_CBC = 2,
	ROC_IE_OT_SA_ENC_AES_CBC = 3,
	ROC_IE_OT_SA_ENC_AES_CTR = 4,
	ROC_IE_OT_SA_ENC_AES_GCM = 5,
	ROC_IE_OT_SA_ENC_AES_CCM = 6,
};

enum {
	ROC_IE_OT_SA_AUTH_NULL = 0,
	ROC_IE_OT_SA_AUTH_SHA1 = 2,
	ROC_IE_OT_SA_AUTH_SHA2_256 = 4,
	ROC_IE_OT_SA_AUTH_SHA2_384 = 5,
	ROC_IE_OT_SA_AUTH_SHA2_512 = 6,
	ROC_IE_OT_SA_AUTH_AES_GMAC = 7,
	ROC_IE_OT_SA_AUTH_AES_XCBC_128 = 8,
};

enum {
	ROC_IE_OT_SA_ENCAP_NONE = 0,
	ROC_IE_OT_SA_ENCAP_UDP = 1,
	ROC_IE_OT_SA_ENCAP_TCP = 2,
};

enum {
	ROC_IE_OT_SA_LIFE_UNIT_OCTETS = 0,
	ROC_IE_OT_SA_LIFE_UNIT_PKTS = 1,
};

enum {
	ROC_IE_OT_SA_IP_HDR_VERIFY_DISABLED = 0,
	ROC_IE_OT_SA_IP_HDR_VERIFY_DST_ADDR = 1,
	ROC_IE_OT_SA_IP_HDR_VERIFY_SRC_DST_ADDR = 2,
};

enum {
	ROC_IE_OT_REAS_STS_SUCCESS = 0,
	ROC_IE_OT_REAS_STS_TIMEOUT = 1,
	ROC_IE_OT_REAS_STS_EVICT = 2,
	ROC_IE_OT_REAS_STS_BAD_ORDER = 3,
	ROC_IE_OT_REAS_STS_TOO_MANY = 4,
	ROC_IE_OT_REAS_STS_HSH_EVICT = 5,
	ROC_IE_OT_REAS_STS_OVERLAP = 6,
	ROC_IE_OT_REAS_STS_ZOMBIE = 7,
	ROC_IE_OT_REAS_STS_L3P_ERR = 8,
	ROC_IE_OT_REAS_STS_MAX = 9
};

enum {
	ROC_IE_OT_ERR_CTL_MODE_NONE = 0,
	ROC_IE_OT_ERR_CTL_MODE_CLEAR = 1,
	ROC_IE_OT_ERR_CTL_MODE_RING = 2,
};

/* Context units in bytes */
#define ROC_CTX_UNIT_8B		  8
#define ROC_CTX_UNIT_128B	  128
#define ROC_CTX_MAX_CKEY_LEN	  32
#define ROC_CTX_MAX_OPAD_IPAD_LEN 128

/* Anti reply window size supported */
#define ROC_AR_WIN_SIZE_MIN	   64
#define ROC_AR_WIN_SIZE_MAX	   4096
#define ROC_LOG_MIN_AR_WIN_SIZE_M1 5

/* u64 array size to fit anti replay window bits */
#define ROC_AR_WINBITS_SZ                                                      \
	(PLT_ALIGN_CEIL(ROC_AR_WIN_SIZE_MAX, BITS_PER_LONG_LONG) /             \
	 BITS_PER_LONG_LONG)

#define ROC_IPSEC_ERR_RING_MAX_ENTRY 65536

union roc_ot_ipsec_err_ring_head {
	uint64_t u64;
	struct {
		uint16_t tail_pos;
		uint16_t tail_gen;
		uint16_t head_pos;
		uint16_t head_gen;
	} s;
};

union roc_ot_ipsec_err_ring_entry {
	uint64_t u64;
	struct {
		uint64_t data0 : 44;
		uint64_t data1 : 9;
		uint64_t rsvd : 3;
		uint64_t comp_code : 8;
	} s;
};

/* Common bit fields between inbound and outbound SA */
union roc_ot_ipsec_sa_word2 {
	struct {
		uint64_t valid : 1;
		uint64_t dir : 1;
		uint64_t outer_ip_ver : 1;
		uint64_t rsvd0 : 1;
		uint64_t mode : 1;
		uint64_t protocol : 1;
		uint64_t aes_key_len : 2;

		uint64_t enc_type : 3;
		uint64_t life_unit : 1;
		uint64_t auth_type : 4;

		uint64_t encap_type : 2;
		uint64_t et_ovrwr_ddr_en : 1;
		uint64_t esn_en : 1;
		uint64_t tport_l4_incr_csum : 1;
		uint64_t ip_hdr_verify : 2;
		uint64_t udp_ports_verify : 1;

		uint64_t rsvd2 : 7;
		uint64_t async_mode : 1;

		uint64_t spi : 32;
	} s;
	uint64_t u64;
};

PLT_STATIC_ASSERT(sizeof(union roc_ot_ipsec_sa_word2) == 1 * sizeof(uint64_t));

union roc_ot_ipsec_outer_ip_hdr {
	struct {
		uint32_t dst_addr;
		uint32_t src_addr;
	} ipv4;
	struct {
		uint8_t src_addr[16];
		uint8_t dst_addr[16];
	} ipv6;
};

struct roc_ot_ipsec_inb_ctx_update_reg {
	uint64_t ar_base;
	uint64_t ar_valid_mask;
	uint64_t hard_life;
	uint64_t soft_life;
	uint64_t mib_octs;
	uint64_t mib_pkts;
	uint64_t ar_winbits[ROC_AR_WINBITS_SZ];
};

union roc_ot_ipsec_outb_iv {
	uint64_t u64[2];
	uint8_t iv_dbg[16];
	struct {
		uint8_t iv_dbg1[4];
		uint8_t salt[4];

		uint32_t rsvd;
		uint8_t iv_dbg2[4];
	} s;
};

struct roc_ot_ipsec_outb_ctx_update_reg {
	union {
		struct {
			uint64_t reserved_0_2 : 3;
			uint64_t address : 57;
			uint64_t mode : 4;
		} s;
		uint64_t u64;
	} err_ctl;

	uint64_t esn_val;
	uint64_t hard_life;
	uint64_t soft_life;
	uint64_t mib_octs;
	uint64_t mib_pkts;
};

union roc_ot_ipsec_outb_param1 {
	uint16_t u16;
	struct {
		uint16_t l4_csum_disable : 1;
		uint16_t ip_csum_disable : 1;
		uint16_t ttl_or_hop_limit : 1;
		uint16_t dummy_pkt : 1;
		uint16_t rfc_or_override_mode : 1;
		uint16_t reserved_5_15 : 11;
	} s;
};

union roc_ot_ipsec_inb_param1 {
	uint16_t u16;
	struct {
		uint16_t l4_csum_disable : 1;
		uint16_t ip_csum_disable : 1;
		uint16_t esp_trailer_disable : 1;
		uint16_t reserved_3_15 : 13;
	} s;
};

struct roc_ot_ipsec_inb_sa {
	/* Word0 */
	union {
		struct {
			uint64_t ar_win : 3;
			uint64_t hard_life_dec : 1;
			uint64_t soft_life_dec : 1;
			uint64_t count_glb_octets : 1;
			uint64_t count_glb_pkts : 1;
			uint64_t count_mib_bytes : 1;

			uint64_t count_mib_pkts : 1;
			uint64_t hw_ctx_off : 7;

			uint64_t ctx_id : 16;

			uint64_t orig_pkt_fabs : 1;
			uint64_t orig_pkt_free : 1;
			uint64_t pkind : 6;

			uint64_t rsvd0 : 1;
			uint64_t et_ovrwr : 1;
			uint64_t pkt_output : 2;
			uint64_t pkt_format : 1;
			uint64_t defrag_opt : 2;
			uint64_t x2p_dst : 1;

			uint64_t ctx_push_size : 7;
			uint64_t rsvd1 : 1;

			uint64_t ctx_hdr_size : 2;
			uint64_t aop_valid : 1;
			uint64_t rsvd2 : 1;
			uint64_t ctx_size : 4;
		} s;
		uint64_t u64;
	} w0;

	/* Word1 */
	union {
		struct {
			uint64_t orig_pkt_aura : 20;
			uint64_t rsvd3 : 4;
			uint64_t orig_pkt_foff : 8;
			uint64_t cookie : 32;
		} s;
		uint64_t u64;
	} w1;

	/* Word 2 */
	union {
		struct {
			uint64_t valid : 1;
			uint64_t dir : 1;
			uint64_t outer_ip_ver : 1;
			uint64_t rsvd4 : 1;
			uint64_t ipsec_mode : 1;
			uint64_t ipsec_protocol : 1;
			uint64_t aes_key_len : 2;

			uint64_t enc_type : 3;
			uint64_t life_unit : 1;
			uint64_t auth_type : 4;

			uint64_t encap_type : 2;
			uint64_t et_ovrwr_ddr_en : 1;
			uint64_t esn_en : 1;
			uint64_t tport_l4_incr_csum : 1;
			uint64_t ip_hdr_verify : 2;
			uint64_t udp_ports_verify : 1;

			uint64_t l3hdr_on_err : 1;
			uint64_t rsvd6 : 6;
			uint64_t async_mode : 1;

			uint64_t spi : 32;
		} s;
		uint64_t u64;
	} w2;

	/* Word3 */
	uint64_t rsvd7;

	/* Word4 - Word7 */
	uint8_t cipher_key[ROC_CTX_MAX_CKEY_LEN];

	/* Word8 - Word9 */
	union {
		struct {
			uint32_t rsvd8;
			uint8_t salt[4];
		} s;
		uint64_t u64;
	} w8;
	uint64_t rsvd9;

	/* Word10 */
	union {
		struct {
			uint64_t rsvd10 : 32;
			uint64_t udp_src_port : 16;
			uint64_t udp_dst_port : 16;
		} s;
		uint64_t u64;
	} w10;

	/* Word11 - Word14 */
	union roc_ot_ipsec_outer_ip_hdr outer_hdr;

	/* Word15 - Word30 */
	uint8_t hmac_opad_ipad[ROC_CTX_MAX_OPAD_IPAD_LEN];

	/* Word31 - Word100 */
	struct roc_ot_ipsec_inb_ctx_update_reg ctx;
};

PLT_STATIC_ASSERT(offsetof(struct roc_ot_ipsec_inb_sa, w1) ==
		  1 * sizeof(uint64_t));
PLT_STATIC_ASSERT(offsetof(struct roc_ot_ipsec_inb_sa, w2) ==
		  2 * sizeof(uint64_t));
PLT_STATIC_ASSERT(offsetof(struct roc_ot_ipsec_inb_sa, cipher_key) ==
		  4 * sizeof(uint64_t));
PLT_STATIC_ASSERT(offsetof(struct roc_ot_ipsec_inb_sa, w8) ==
		  8 * sizeof(uint64_t));
PLT_STATIC_ASSERT(offsetof(struct roc_ot_ipsec_inb_sa, w10) ==
		  10 * sizeof(uint64_t));
PLT_STATIC_ASSERT(offsetof(struct roc_ot_ipsec_inb_sa, outer_hdr) ==
		  11 * sizeof(uint64_t));
PLT_STATIC_ASSERT(offsetof(struct roc_ot_ipsec_inb_sa, hmac_opad_ipad) ==
		  15 * sizeof(uint64_t));
PLT_STATIC_ASSERT(offsetof(struct roc_ot_ipsec_inb_sa, ctx) ==
		  31 * sizeof(uint64_t));

struct roc_ot_ipsec_outb_sa {
	/* Word0 */
	union {
		struct {
			uint64_t esn_en : 1;
			uint64_t ip_id : 1;
			uint64_t rsvd0 : 1;
			uint64_t hard_life_dec : 1;
			uint64_t soft_life_dec : 1;
			uint64_t count_glb_octets : 1;
			uint64_t count_glb_pkts : 1;
			uint64_t count_mib_bytes : 1;

			uint64_t count_mib_pkts : 1;
			uint64_t hw_ctx_off : 7;

			uint64_t ctx_id : 16;
			uint64_t rsvd1 : 16;

			uint64_t ctx_push_size : 7;
			uint64_t rsvd2 : 1;

			uint64_t ctx_hdr_size : 2;
			uint64_t aop_valid : 1;
			uint64_t rsvd3 : 1;
			uint64_t ctx_size : 4;
		} s;
		uint64_t u64;
	} w0;

	/* Word1 */
	union {
		struct {
			uint64_t rsvd4 : 32;
			uint64_t cookie : 32;
		} s;
		uint64_t u64;
	} w1;

	/* Word 2 */
	union {
		struct {
			uint64_t valid : 1;
			uint64_t dir : 1;
			uint64_t outer_ip_ver : 1;
			uint64_t rsvd5 : 1;
			uint64_t ipsec_mode : 1;
			uint64_t ipsec_protocol : 1;
			uint64_t aes_key_len : 2;

			uint64_t enc_type : 3;
			uint64_t life_unit : 1;
			uint64_t auth_type : 4;

			uint64_t encap_type : 2;
			uint64_t ipv4_df_src_or_ipv6_flw_lbl_src : 1;
			uint64_t dscp_src : 1;
			uint64_t iv_src : 2;
			uint64_t ipid_gen : 1;
			uint64_t rsvd6 : 1;

			uint64_t rsvd7 : 7;
			uint64_t async_mode : 1;

			uint64_t spi : 32;
		} s;
		uint64_t u64;
	} w2;

	/* Word3 */
	uint64_t rsvd8;

	/* Word4 - Word7 */
	uint8_t cipher_key[ROC_CTX_MAX_CKEY_LEN];

	/* Word8 - Word9 */
	union roc_ot_ipsec_outb_iv iv;

	/* Word10 */
	union {
		struct {
			uint64_t rsvd9 : 4;
			uint64_t ipv4_df_or_ipv6_flw_lbl : 20;

			uint64_t dscp : 6;
			uint64_t rsvd10 : 2;

			uint64_t udp_dst_port : 16;

			uint64_t udp_src_port : 16;
		} s;
		uint64_t u64;
	} w10;

	/* Word11 - Word14 */
	union roc_ot_ipsec_outer_ip_hdr outer_hdr;

	/* Word15 - Word30 */
	uint8_t hmac_opad_ipad[ROC_CTX_MAX_OPAD_IPAD_LEN];

	/* Word31 - Word36 */
	struct roc_ot_ipsec_outb_ctx_update_reg ctx;
};

PLT_STATIC_ASSERT(offsetof(struct roc_ot_ipsec_outb_sa, w1) ==
		  1 * sizeof(uint64_t));
PLT_STATIC_ASSERT(offsetof(struct roc_ot_ipsec_outb_sa, w2) ==
		  2 * sizeof(uint64_t));
PLT_STATIC_ASSERT(offsetof(struct roc_ot_ipsec_outb_sa, cipher_key) ==
		  4 * sizeof(uint64_t));
PLT_STATIC_ASSERT(offsetof(struct roc_ot_ipsec_outb_sa, iv) ==
		  8 * sizeof(uint64_t));
PLT_STATIC_ASSERT(offsetof(struct roc_ot_ipsec_outb_sa, w10) ==
		  10 * sizeof(uint64_t));
PLT_STATIC_ASSERT(offsetof(struct roc_ot_ipsec_outb_sa, outer_hdr) ==
		  11 * sizeof(uint64_t));
PLT_STATIC_ASSERT(offsetof(struct roc_ot_ipsec_outb_sa, hmac_opad_ipad) ==
		  15 * sizeof(uint64_t));
PLT_STATIC_ASSERT(offsetof(struct roc_ot_ipsec_outb_sa, ctx) ==
		  31 * sizeof(uint64_t));

void __roc_api roc_ot_ipsec_inb_sa_init(struct roc_ot_ipsec_inb_sa *sa,
					bool is_inline);
void __roc_api roc_ot_ipsec_outb_sa_init(struct roc_ot_ipsec_outb_sa *sa);
#endif /* __ROC_IE_OT_H__ */
