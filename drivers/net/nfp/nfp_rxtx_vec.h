/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Corigine, Inc.
 * All rights reserved.
 */

#ifndef __NFP_RXTX_VEC_AVX2_H__
#define __NFP_RXTX_VEC_AVX2_H__

#include <stdbool.h>

bool nfp_net_get_avx2_supported(void);

uint16_t nfp_net_vec_avx2_recv_pkts(void *rx_queue,
		struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts);

#endif /* __NFP_RXTX_VEC_AVX2_H__ */
