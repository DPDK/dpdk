/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021-2024 Advanced Micro Devices, Inc.
 */

#include <rte_cryptodev.h>

#include "ionic_crypto.h"

static const struct rte_cryptodev_capabilities iocpt_sym_caps[] = {
	{	/* AES GCM */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AEAD,
			{.aead = {
				.algo = RTE_CRYPTO_AEAD_AES_GCM,
				.block_size = 16,
				.key_size = {
					.min = 16,
					.max = 32,
					.increment = 16
				},
				.digest_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				},
				.aad_size = {
					.min = 0,
					.max = 1024,
					.increment = 1
				},
				.iv_size = {
					.min = 12,
					.max = 12,
					.increment = 0
				}
			}, }
		}, }
	},
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

static const struct rte_cryptodev_capabilities iocpt_asym_caps[] = {
	/* None */
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

const struct rte_cryptodev_capabilities *
iocpt_get_caps(uint64_t flags)
{
	if (flags & RTE_CRYPTODEV_FF_ASYMMETRIC_CRYPTO)
		return iocpt_asym_caps;
	else
		return iocpt_sym_caps;
}
