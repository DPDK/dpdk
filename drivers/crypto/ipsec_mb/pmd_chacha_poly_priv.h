/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

#ifndef _PMD_CHACHA_POLY_PRIV_H_
#define _PMD_CHACHA_POLY_PRIV_H_

#include "ipsec_mb_private.h"

#define CHACHA20_POLY1305_DIGEST_LENGTH 16

static const
struct rte_cryptodev_capabilities chacha20_poly1305_capabilities[] = {
	{/* CHACHA20-POLY1305 */
	    .op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
	    {.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AEAD,
		    {.aead = {
				.algo = RTE_CRYPTO_AEAD_CHACHA20_POLY1305,
				.block_size = 64,
				.key_size = {
					.min = 32,
					.max = 32,
					.increment = 0},
				.digest_size = {
					.min = 16,
					.max = 16,
					.increment = 0},
				.aad_size = {
					.min = 0,
					.max = 240,
					.increment = 1},
				.iv_size = {
					.min = 12,
					.max = 12,
					.increment = 0},
			    },
			}
		},}
	},
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

uint8_t pmd_driver_id_chacha20_poly1305;

#endif /* _PMD_CHACHA_POLY_PRIV_H_ */
