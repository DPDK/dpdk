/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Corigine, Inc.
 * All rights reserved.
 */

#include <stdbool.h>

#include <rte_common.h>

#include "nfp_rxtx_vec.h"

bool __rte_weak
nfp_net_get_avx2_supported(void)
{
	return false;
}
