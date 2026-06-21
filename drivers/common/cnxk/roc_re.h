/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2026 Marvell.
 */

#ifndef __ROC_RE_H__
#define __ROC_RE_H__

/* RE ML opcodes */
#define ROC_RE_MAJOR_OP_MLKEM        0x1A
#define ROC_RE_MAJOR_OP_MLDSA        0x1B
#define ROC_RE_MINOR_OP_MLKEM_KEYGEN 0x00
#define ROC_RE_MINOR_OP_MLKEM_ENCAP  0x01
#define ROC_RE_MINOR_OP_MLKEM_DECAP  0x02
#define ROC_RE_MINOR_OP_MLDSA_KEYGEN 0x00
#define ROC_RE_MINOR_OP_MLDSA_SIGN   0x01
#define ROC_RE_MINOR_OP_MLDSA_VERIFY 0x02

/* ML-KEM param2 fields */
#define ROC_RE_ML_KEM_PARAM2_INMSG_BIT  4
#define ROC_RE_ML_KEM_PARAM2_INSEED_BIT 5

/* ML-DSA param2 fields */
#define ROC_RE_ML_DSA_PARAM2_SIGN_BIT   4
#define ROC_RE_ML_DSA_PARAM2_SEED_BIT   5
#define ROC_RE_ML_DSA_PARAM2_CTXN_BIT   8

/* ML-DSA minor op fields */
#define ROC_RE_ML_DSA_MINOR_SIGN_TYPE_BIT 2
#define ROC_RE_ML_DSA_MINOR_MSG_TYPE_BIT  4

#endif /* __ROC_RE_H__ */
