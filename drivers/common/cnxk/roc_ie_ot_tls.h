/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#ifndef __ROC_IE_OT_TLS_H__
#define __ROC_IE_OT_TLS_H__

#include "roc_platform.h"

#define ROC_IE_OT_TLS_CTX_ILEN		     1
#define ROC_IE_OT_TLS_CTX_HDR_SIZE	     1
#define ROC_IE_OT_TLS_AR_WIN_SIZE_MAX	     4096
#define ROC_IE_OT_TLS_LOG_MIN_AR_WIN_SIZE_M1 5

/* u64 array size to fit anti replay window bits */
#define ROC_IE_OT_TLS_AR_WINBITS_SZ                                                                \
	(PLT_ALIGN_CEIL(ROC_IE_OT_TLS_AR_WIN_SIZE_MAX, BITS_PER_LONG_LONG) / BITS_PER_LONG_LONG)

/* CN10K TLS opcodes */
#define ROC_IE_OT_TLS_MAJOR_OP_RECORD_ENC   0x16UL
#define ROC_IE_OT_TLS_MAJOR_OP_RECORD_DEC   0x17UL
#define ROC_IE_OT_TLS13_MAJOR_OP_RECORD_ENC 0x18UL
#define ROC_IE_OT_TLS13_MAJOR_OP_RECORD_DEC 0x19UL

#define ROC_IE_OT_TLS_CTX_MAX_OPAD_IPAD_LEN 128
#define ROC_IE_OT_TLS_CTX_MAX_KEY_IV_LEN    48
#define ROC_IE_OT_TLS_CTX_MAX_IV_LEN	    16

enum roc_ie_ot_tls_mac_type {
	ROC_IE_OT_TLS_MAC_MD5 = 1,
	ROC_IE_OT_TLS_MAC_SHA1 = 2,
	ROC_IE_OT_TLS_MAC_SHA2_256 = 4,
	ROC_IE_OT_TLS_MAC_SHA2_384 = 5,
	ROC_IE_OT_TLS_MAC_SHA2_512 = 6,
};

enum roc_ie_ot_tls_cipher_type {
	ROC_IE_OT_TLS_CIPHER_3DES = 1,
	ROC_IE_OT_TLS_CIPHER_AES_CBC = 3,
	ROC_IE_OT_TLS_CIPHER_AES_GCM = 7,
	ROC_IE_OT_TLS_CIPHER_AES_CCM = 10,
	ROC_IE_OT_TLS_CIPHER_CHACHA_POLY = 9,
};

enum roc_ie_ot_tls_ver {
	ROC_IE_OT_TLS_VERSION_TLS_12 = 1,
	ROC_IE_OT_TLS_VERSION_DTLS_12 = 2,
	ROC_IE_OT_TLS_VERSION_TLS_13 = 3,
};

enum roc_ie_ot_tls_aes_key_len {
	ROC_IE_OT_TLS_AES_KEY_LEN_128 = 1,
	ROC_IE_OT_TLS_AES_KEY_LEN_256 = 3,
};

enum {
	ROC_IE_OT_TLS_IV_SRC_DEFAULT = 0,
	ROC_IE_OT_TLS_IV_SRC_FROM_SA = 1,
};

struct roc_ie_ot_tls_read_ctx_update_reg {
	uint64_t ar_base;
	uint64_t ar_valid_mask;
	uint64_t hard_life;
	uint64_t soft_life;
	uint64_t mib_octs;
	uint64_t mib_pkts;
	uint64_t ar_winbits[ROC_IE_OT_TLS_AR_WINBITS_SZ];
};

struct roc_ie_ot_tls_1_3_read_ctx_update_reg {
	uint64_t rsvd0;
	uint64_t ar_valid_mask;
	uint64_t hard_life;
	uint64_t soft_life;
	uint64_t mib_octs;
	uint64_t mib_pkts;
	uint64_t rsvd1;
};

union roc_ie_ot_tls_param2 {
	uint16_t u16;
	struct {
		uint8_t msg_type;
		uint8_t rsvd;
	} s;
};

struct roc_ie_ot_tls_read_sa {
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
	uint64_t w1_rsvd3;

	/* Word2 */
	union {
		struct {
			uint64_t version_select : 4;
			uint64_t aes_key_len : 2;
			uint64_t cipher_select : 4;
			uint64_t mac_select : 4;
			uint64_t rsvd4 : 50;
		} s;
		uint64_t u64;
	} w2;

	/* Word3 */
	uint64_t w3_rsvd5;

	/* Word4 - Word9 */
	uint8_t cipher_key[ROC_IE_OT_TLS_CTX_MAX_KEY_IV_LEN];

	union {
		struct {
			/* Word10 - Word16 */
			struct roc_ie_ot_tls_1_3_read_ctx_update_reg ctx;
		} tls_13;

		struct {
			/* Word10 - Word25 */
			uint8_t opad_ipad[ROC_IE_OT_TLS_CTX_MAX_OPAD_IPAD_LEN];

			/* Word26 - Word95 */
			struct roc_ie_ot_tls_read_ctx_update_reg ctx;
		} tls_12;
	};
};

struct roc_ie_ot_tls_write_sa {
	/* Word0 */
	union {
		struct {
			uint64_t rsvd0 : 3;
			uint64_t hard_life_dec : 1;
			uint64_t soft_life_dec : 1;
			uint64_t count_glb_octets : 1;
			uint64_t count_glb_pkts : 1;
			uint64_t count_mib_bytes : 1;

			uint64_t count_mib_pkts : 1;
			uint64_t hw_ctx_off : 7;

			uint64_t rsvd1 : 32;

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
	uint64_t w1_rsvd4;

	/* Word2 */
	union {
		struct {
			uint64_t version_select : 4;
			uint64_t aes_key_len : 2;
			uint64_t cipher_select : 4;
			uint64_t mac_select : 4;
			uint64_t iv_at_cptr : 1;
			uint64_t rsvd5 : 49;
		} s;
		uint64_t u64;
	} w2;

	/* Word3 */
	uint64_t w3_rsvd6;

	/* Word4 - Word9 */
	uint8_t cipher_key[ROC_IE_OT_TLS_CTX_MAX_KEY_IV_LEN];

	union {
		struct {
			/* Word10 */
			uint64_t w10_rsvd7;

			uint64_t seq_num;
		} tls_13;

		struct {
			/* Word10 - Word25 */
			uint8_t opad_ipad[ROC_IE_OT_TLS_CTX_MAX_OPAD_IPAD_LEN];

			/* Word26 */
			uint64_t w26_rsvd7;

			/* Word27 */
			uint64_t seq_num;
		} tls_12;
	};
};
#endif /* __ROC_IE_OT_TLS_H__ */
