/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Corigine, Inc.
 * All rights reserved.
 */

#ifndef __NFP_NFDK_VEC_H__
#define __NFP_NFDK_VEC_H__

#include <stdbool.h>

#include <rte_mbuf_core.h>

#include "../nfp_net_common.h"
#include "nfp_nfdk.h"

static inline bool
nfp_net_nfdk_is_simple_packet(struct rte_mbuf *pkt,
		struct nfp_net_hw *hw)
{
	if (pkt->data_len > NFDK_TX_MAX_DATA_PER_HEAD)
		return false;

	if ((hw->super.cap & NFP_NET_CFG_CTRL_LSO_ANY) == 0)
		return true;

	if ((pkt->ol_flags & RTE_MBUF_F_TX_TCP_SEG) == 0)
		return true;

	return false;
}

uint16_t nfp_net_nfdk_vec_avx2_xmit_pkts(void *tx_queue,
		struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts);

#endif /* __NFP_NFDK_VEC_H__ */
