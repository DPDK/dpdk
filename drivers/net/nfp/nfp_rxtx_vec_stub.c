/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Corigine, Inc.
 * All rights reserved.
 */

#include <stdbool.h>

#include <rte_common.h>
#include <rte_mbuf_core.h>

#include "nfp_rxtx_vec.h"

bool
nfp_net_get_avx2_supported(void)
{
	return false;
}

uint16_t
nfp_net_vec_avx2_recv_pkts(__rte_unused void *rx_queue,
		__rte_unused struct rte_mbuf **rx_pkts,
		__rte_unused uint16_t nb_pkts)
{
	return 0;
}
