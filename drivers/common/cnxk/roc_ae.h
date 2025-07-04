/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef __ROC_AE_H__
#define __ROC_AE_H__

#include "roc_platform.h"

/* AE opcodes */
#define ROC_AE_MAJOR_OP_RANDOM	     0x32
#define ROC_AE_MAJOR_OP_MODEX	     0x03
#define ROC_AE_MAJOR_OP_EC	     0x04
#define ROC_AE_MAJOR_OP_ECC	     0x05
#define ROC_AE_MAJOR_OP_EDDSA	     0x0A
#define ROC_AE_MINOR_OP_RANDOM	     0x00
#define ROC_AE_MINOR_OP_MODEX	     0x01
#define ROC_AE_MINOR_OP_PKCS_ENC     0x02
#define ROC_AE_MINOR_OP_PKCS_ENC_CRT 0x03
#define ROC_AE_MINOR_OP_PKCS_DEC     0x04
#define ROC_AE_MINOR_OP_PKCS_DEC_CRT 0x05
#define ROC_AE_MINOR_OP_MODEX_CRT    0x06
#define ROC_AE_MINOR_OP_EC_SIGN      0x01
#define ROC_AE_MINOR_OP_EC_VERIFY    0x02
#define ROC_AE_MINOR_OP_ECC_UMP	     0x03
#define ROC_AE_MINOR_OP_ECC_FPM	     0x04
#define ROC_AE_MINOR_OP_ED_SIGN      0x00
#define ROC_AE_MINOR_OP_ED_VERIFY    0x01
#define ROC_AE_MINOR_OP_ED_KEYGEN    0x02

/**
 * Enumeration roc_ae_ec_id
 *
 * Enumerates supported elliptic curves
 */
typedef enum {
	ROC_AE_EC_ID_P192 = 0,
	ROC_AE_EC_ID_P224 = 1,
	ROC_AE_EC_ID_P256 = 2,
	ROC_AE_EC_ID_P384 = 3,
	ROC_AE_EC_ID_P521 = 4,
	ROC_AE_EC_ID_P160 = 5,
	ROC_AE_EC_ID_P320 = 6,
	ROC_AE_EC_ID_P512 = 7,
	ROC_AE_EC_ID_SM2  = 8,
	ROC_AE_EC_ID_ED25519 = 9,
	ROC_AE_EC_ID_ED448 = 10,
	ROC_AE_EC_ID_PMAX
} roc_ae_ec_id;

/* EC param1 fields */
#define ROC_AE_EC_PARAM1_ECDSA     (0 << 7)
#define ROC_AE_EC_PARAM1_SM2       (1 << 7)
#define ROC_AE_EC_PARAM1_NIST      (0 << 6)
#define ROC_AE_EC_PARAM1_NONNIST   (1 << 6)
#define ROC_AE_ED_PARAM1_25519     (1 << 1)
#define ROC_AE_ED_PARAM1_448       (1 << 3)
#define ROC_AE_ED_PARAM1_KEYGEN_BIT      4
#define ROC_AE_EC_PARAM1_PH_BIT          5

typedef enum {
	ROC_AE_ERR_ECC_PAI = 0x0b,
	ROC_AE_ERR_ECC_POINT_NOT_ON_CURVE = 0x11
} roc_ae_error_code;

#define ROC_AE_EC_DATA_MAX 66

/* Prime and order fields of built-in elliptic curves */
struct roc_ae_ec_group {
	struct {
		/* P521 maximum length */
		uint8_t data[ROC_AE_EC_DATA_MAX];
		unsigned int length;
	} prime;

	struct {
		/* P521 maximum length */
		uint8_t data[ROC_AE_EC_DATA_MAX];
		unsigned int length;
	} order;

	struct {
		/* P521 maximum length */
		uint8_t data[ROC_AE_EC_DATA_MAX];
		unsigned int length;
	} consta;

	struct {
		/* P521 maximum length */
		uint8_t data[ROC_AE_EC_DATA_MAX];
		unsigned int length;
	} constb;
};

struct roc_ae_ec_ctx {
	/* Prime length defined by microcode for EC operations */
	uint8_t curveid;

	/* Private key */
	struct {
		uint8_t data[ROC_AE_EC_DATA_MAX];
		unsigned int length;
	} pkey;

	/* Public key */
	struct {
		struct {
			uint8_t data[ROC_AE_EC_DATA_MAX];
			unsigned int length;
		} x;
		struct {
			uint8_t data[ROC_AE_EC_DATA_MAX];
			unsigned int length;
		} y;
	} q;
};

/* Buffer pointer */
struct roc_ae_buf_ptr {
	void *vaddr;
};

int __roc_api roc_ae_ec_grp_get(struct roc_ae_ec_group **tbl);
void __roc_api roc_ae_ec_grp_put(void);
#endif /* __ROC_AE_H__ */
