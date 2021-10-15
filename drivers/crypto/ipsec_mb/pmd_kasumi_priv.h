/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2021 Intel Corporation
 */

#ifndef _PMD_KASUMI_PRIV_H_
#define _PMD_KASUMI_PRIV_H_

#include "ipsec_mb_private.h"

#define KASUMI_KEY_LENGTH 16
#define KASUMI_IV_LENGTH 8
#define KASUMI_MAX_BURST 4
#define BYTE_LEN 8
#define KASUMI_DIGEST_LENGTH 4

uint8_t pmd_driver_id_kasumi;

static const struct rte_cryptodev_capabilities kasumi_capabilities[] = {
	{	/* KASUMI (F9) */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_KASUMI_F9,
				.block_size = 8,
				.key_size = {
					.min = KASUMI_KEY_LENGTH,
					.max = KASUMI_KEY_LENGTH,
					.increment = 0
				},
				.digest_size = {
					.min = KASUMI_DIGEST_LENGTH,
					.max = KASUMI_DIGEST_LENGTH,
					.increment = 0
				},
				.iv_size = { 0 }
			}, }
		}, }
	},
	{	/* KASUMI (F8) */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_KASUMI_F8,
				.block_size = 8,
				.key_size = {
					.min = KASUMI_KEY_LENGTH,
					.max = KASUMI_KEY_LENGTH,
					.increment = 0
				},
				.iv_size = {
					.min = KASUMI_IV_LENGTH,
					.max = KASUMI_IV_LENGTH,
					.increment = 0
				}
			}, }
		}, }
	},
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

/** KASUMI private session structure */
struct kasumi_session {
	/* Keys have to be 16-byte aligned */
	kasumi_key_sched_t pKeySched_cipher;
	kasumi_key_sched_t pKeySched_hash;
	enum ipsec_mb_operation op;
	enum rte_crypto_auth_operation auth_op;
	uint16_t cipher_iv_offset;
} __rte_cache_aligned;

struct kasumi_qp_data {
	uint8_t temp_digest[KASUMI_DIGEST_LENGTH];
	/* *< Buffers used to store the digest generated
	 * by the driver when verifying a digest provided
	 * by the user (using authentication verify operation)
	 */
};

#endif /* _PMD_KASUMI_PRIV_H_ */
