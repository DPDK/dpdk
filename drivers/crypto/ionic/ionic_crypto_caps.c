/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021-2024 Advanced Micro Devices, Inc.
 */

#include <rte_cryptodev.h>

#include "ionic_crypto.h"

static const struct rte_cryptodev_capabilities iocpt_sym_caps[] = {
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
