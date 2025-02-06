/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Corigine, Inc.
 * All rights reserved.
 */

#include "nfp_nfdk_vec.h"

uint16_t
nfp_net_nfdk_vec_avx2_xmit_pkts(__rte_unused void *tx_queue,
		__rte_unused struct rte_mbuf **tx_pkts,
		__rte_unused uint16_t nb_pkts)
{
	return 0;
}
