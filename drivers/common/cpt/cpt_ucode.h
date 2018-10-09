/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium, Inc
 */

#ifndef _CPT_UCODE_H_
#define _CPT_UCODE_H_

#include "cpt_mcode_defines.h"

/*
 * This file defines functions that are interfaces to microcode spec.
 *
 */

static __rte_always_inline int
cpt_is_algo_supported(struct rte_crypto_sym_xform *xform)
{
	/*
	 * Microcode only supports the following combination.
	 * Encryption followed by authentication
	 * Authentication followed by decryption
	 */
	if (xform->next) {
		if ((xform->type == RTE_CRYPTO_SYM_XFORM_AUTH) &&
		    (xform->next->type == RTE_CRYPTO_SYM_XFORM_CIPHER) &&
		    (xform->next->cipher.op == RTE_CRYPTO_CIPHER_OP_ENCRYPT)) {
			/* Unsupported as of now by microcode */
			CPT_LOG_DP_ERR("Unsupported combination");
			return -1;
		}
		if ((xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER) &&
		    (xform->next->type == RTE_CRYPTO_SYM_XFORM_AUTH) &&
		    (xform->cipher.op == RTE_CRYPTO_CIPHER_OP_DECRYPT)) {
			/* For GMAC auth there is no cipher operation */
			if (xform->aead.algo != RTE_CRYPTO_AEAD_AES_GCM ||
			    xform->next->auth.algo !=
			    RTE_CRYPTO_AUTH_AES_GMAC) {
				/* Unsupported as of now by microcode */
				CPT_LOG_DP_ERR("Unsupported combination");
				return -1;
			}
		}
	}
	return 0;
}

#endif /*_CPT_UCODE_H_ */
