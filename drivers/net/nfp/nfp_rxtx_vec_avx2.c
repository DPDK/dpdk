/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Corigine, Inc.
 * All rights reserved.
 */

#include <stdbool.h>

#include <rte_cpuflags.h>
#include <rte_vect.h>

#include "nfp_rxtx_vec.h"

bool
nfp_net_get_avx2_supported(void)
{
	if (rte_vect_get_max_simd_bitwidth() >= RTE_VECT_SIMD_256 &&
			rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX2) == 1)
		return true;

	return false;
}
