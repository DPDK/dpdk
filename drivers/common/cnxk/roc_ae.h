/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef __ROC_AE_H__
#define __ROC_AE_H__

/* AE opcodes */
#define ROC_AE_MAJOR_OP_MODEX	     0x03
#define ROC_AE_MAJOR_OP_ECDSA	     0x04
#define ROC_AE_MAJOR_OP_ECC	     0x05
#define ROC_AE_MINOR_OP_MODEX	     0x01
#define ROC_AE_MINOR_OP_PKCS_ENC     0x02
#define ROC_AE_MINOR_OP_PKCS_ENC_CRT 0x03
#define ROC_AE_MINOR_OP_PKCS_DEC     0x04
#define ROC_AE_MINOR_OP_PKCS_DEC_CRT 0x05
#define ROC_AE_MINOR_OP_MODEX_CRT    0x06
#define ROC_AE_MINOR_OP_ECDSA_SIGN   0x01
#define ROC_AE_MINOR_OP_ECDSA_VERIFY 0x02
#define ROC_AE_MINOR_OP_ECC_UMP	     0x03

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
	ROC_AE_EC_ID_PMAX = 8
} roc_ae_ec_id;

/* Prime and order fields of built-in elliptic curves */
struct roc_ae_ec_group {
	struct {
		/* P521 maximum length */
		uint8_t data[66];
		unsigned int length;
	} prime;

	struct {
		/* P521 maximum length */
		uint8_t data[66];
		unsigned int length;
	} order;

	struct {
		/* P521 maximum length */
		uint8_t data[66];
		unsigned int length;
	} consta;

	struct {
		/* P521 maximum length */
		uint8_t data[66];
		unsigned int length;
	} constb;
};

struct roc_ae_ec_ctx {
	/* Prime length defined by microcode for EC operations */
	uint8_t curveid;
};

/* Buffer pointer */
struct roc_ae_buf_ptr {
	void *vaddr;
};

int __roc_api roc_ae_ec_grp_get(struct roc_ae_ec_group **tbl);
void __roc_api roc_ae_ec_grp_put(void);
#endif /* __ROC_AE_H__ */
