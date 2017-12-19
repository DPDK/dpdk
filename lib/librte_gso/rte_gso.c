/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <errno.h>

#include <rte_log.h>
#include <rte_ethdev.h>

#include "rte_gso.h"
#include "gso_common.h"
#include "gso_tcp4.h"
#include "gso_tunnel_tcp4.h"

int
rte_gso_segment(struct rte_mbuf *pkt,
		const struct rte_gso_ctx *gso_ctx,
		struct rte_mbuf **pkts_out,
		uint16_t nb_pkts_out)
{
	struct rte_mempool *direct_pool, *indirect_pool;
	struct rte_mbuf *pkt_seg;
	uint64_t ol_flags;
	uint16_t gso_size;
	uint8_t ipid_delta;
	int ret = 1;

	if (pkt == NULL || pkts_out == NULL || gso_ctx == NULL ||
			nb_pkts_out < 1 ||
			gso_ctx->gso_size < RTE_GSO_SEG_SIZE_MIN ||
			((gso_ctx->gso_types & (DEV_TX_OFFLOAD_TCP_TSO |
			DEV_TX_OFFLOAD_VXLAN_TNL_TSO |
			DEV_TX_OFFLOAD_GRE_TNL_TSO)) == 0))
		return -EINVAL;

	if (gso_ctx->gso_size >= pkt->pkt_len) {
		pkt->ol_flags &= (~PKT_TX_TCP_SEG);
		pkts_out[0] = pkt;
		return 1;
	}

	direct_pool = gso_ctx->direct_pool;
	indirect_pool = gso_ctx->indirect_pool;
	gso_size = gso_ctx->gso_size;
	ipid_delta = (gso_ctx->flag != RTE_GSO_FLAG_IPID_FIXED);
	ol_flags = pkt->ol_flags;

	if ((IS_IPV4_VXLAN_TCP4(pkt->ol_flags) &&
			(gso_ctx->gso_types & DEV_TX_OFFLOAD_VXLAN_TNL_TSO)) ||
			((IS_IPV4_GRE_TCP4(pkt->ol_flags) &&
			 (gso_ctx->gso_types & DEV_TX_OFFLOAD_GRE_TNL_TSO)))) {
		pkt->ol_flags &= (~PKT_TX_TCP_SEG);
		ret = gso_tunnel_tcp4_segment(pkt, gso_size, ipid_delta,
				direct_pool, indirect_pool,
				pkts_out, nb_pkts_out);
	} else if (IS_IPV4_TCP(pkt->ol_flags) &&
			(gso_ctx->gso_types & DEV_TX_OFFLOAD_TCP_TSO)) {
		pkt->ol_flags &= (~PKT_TX_TCP_SEG);
		ret = gso_tcp4_segment(pkt, gso_size, ipid_delta,
				direct_pool, indirect_pool,
				pkts_out, nb_pkts_out);
	} else {
		/* unsupported packet, skip */
		pkts_out[0] = pkt;
		RTE_LOG(DEBUG, GSO, "Unsupported packet type\n");
		return 1;
	}

	if (ret > 1) {
		pkt_seg = pkt;
		while (pkt_seg) {
			rte_mbuf_refcnt_update(pkt_seg, -1);
			pkt_seg = pkt_seg->next;
		}
	} else if (ret < 0) {
		/* Revert the ol_flags in the event of failure. */
		pkt->ol_flags = ol_flags;
	}

	return ret;
}
