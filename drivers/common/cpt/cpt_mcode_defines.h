/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium, Inc
 */

#ifndef _CPT_MCODE_DEFINES_H_
#define _CPT_MCODE_DEFINES_H_

#include <rte_byteorder.h>
#include <rte_memory.h>

/*
 * This file defines macros and structures according to microcode spec
 *
 */

#define CPT_BYTE_16		16
#define CPT_BYTE_24		24
#define CPT_BYTE_32		32
#define CPT_MAX_SG_IN_OUT_CNT	32
#define CPT_MAX_SG_CNT		(CPT_MAX_SG_IN_OUT_CNT/2)

#define COMPLETION_CODE_SIZE	8
#define COMPLETION_CODE_INIT	0

#define SG_LIST_HDR_SIZE	(8u)
#define SG_ENTRY_SIZE		sizeof(sg_comp_t)

/* #define CPT_ALWAYS_USE_SG_MODE */
#define CPT_ALWAYS_USE_SEPARATE_BUF

typedef struct sglist_comp {
	union {
		uint64_t len;
		struct {
			uint16_t len[4];
		} s;
	} u;
	uint64_t ptr[4];
} sg_comp_t;

struct cpt_sess_misc {
	/** CPT opcode */
	uint16_t cpt_op:4;
	/** ZUC, SNOW3G &  KASUMI flags */
	uint16_t zsk_flag:4;
	/** Flag for AES GCM */
	uint16_t aes_gcm:1;
	/** Flag for AES CTR */
	uint16_t aes_ctr:1;
	/** Flag for NULL cipher/auth */
	uint16_t is_null:1;
	/** Flag for GMAC */
	uint16_t is_gmac:1;
	/** AAD length */
	uint16_t aad_length;
	/** MAC len in bytes */
	uint8_t mac_len;
	/** IV length in bytes */
	uint8_t iv_length;
	/** Auth IV length in bytes */
	uint8_t auth_iv_length;
	/** Reserved field */
	uint8_t rsvd1;
	/** IV offset in bytes */
	uint16_t iv_offset;
	/** Auth IV offset in bytes */
	uint16_t auth_iv_offset;
	/** Salt */
	uint32_t salt;
	/** Context DMA address */
	phys_addr_t ctx_dma_addr;
};

typedef union {
	uint64_t flags;
	struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		uint64_t enc_cipher   : 4;
		uint64_t reserved1    : 1;
		uint64_t aes_key      : 2;
		uint64_t iv_source    : 1;
		uint64_t hash_type    : 4;
		uint64_t reserved2    : 3;
		uint64_t auth_input_type : 1;
		uint64_t mac_len      : 8;
		uint64_t reserved3    : 8;
		uint64_t encr_offset  : 16;
		uint64_t iv_offset    : 8;
		uint64_t auth_offset  : 8;
#else
		uint64_t auth_offset  : 8;
		uint64_t iv_offset    : 8;
		uint64_t encr_offset  : 16;
		uint64_t reserved3    : 8;
		uint64_t mac_len      : 8;
		uint64_t auth_input_type : 1;
		uint64_t reserved2    : 3;
		uint64_t hash_type    : 4;
		uint64_t iv_source    : 1;
		uint64_t aes_key      : 2;
		uint64_t reserved1    : 1;
		uint64_t enc_cipher   : 4;
#endif
	} e;
} encr_ctrl_t;

typedef struct {
	encr_ctrl_t enc_ctrl;
	uint8_t  encr_key[32];
	uint8_t  encr_iv[16];
} mc_enc_context_t;

typedef struct {
	uint8_t  ipad[64];
	uint8_t  opad[64];
} mc_fc_hmac_context_t;

typedef struct {
	mc_enc_context_t     enc;
	mc_fc_hmac_context_t hmac;
} mc_fc_context_t;

typedef struct {
	uint8_t encr_auth_iv[16];
	uint8_t ci_key[16];
	uint8_t zuc_const[32];
} mc_zuc_snow3g_ctx_t;

typedef struct {
	uint8_t reg_A[8];
	uint8_t ci_key[16];
} mc_kasumi_ctx_t;

struct cpt_ctx {
	/* Below fields are accessed by sw */
	uint64_t enc_cipher	:8;
	uint64_t hash_type	:8;
	uint64_t mac_len	:8;
	uint64_t auth_key_len	:8;
	uint64_t fc_type	:4;
	uint64_t hmac		:1;
	uint64_t zsk_flags	:3;
	uint64_t k_ecb		:1;
	uint64_t snow3g		:1;
	uint64_t rsvd		:22;
	/* Below fields are accessed by hardware */
	union {
		mc_fc_context_t fctx;
		mc_zuc_snow3g_ctx_t zs_ctx;
		mc_kasumi_ctx_t k_ctx;
	};
	uint8_t  auth_key[64];
};

#define CPT_P_ENC_CTRL(fctx)  fctx->enc.enc_ctrl.e

#endif /* _CPT_MCODE_DEFINES_H_ */
