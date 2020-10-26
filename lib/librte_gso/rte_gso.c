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
#include "gso_udp4.h"

#define ILLEGAL_UDP_GSO_CTX(ctx) \
	((((ctx)->gso_types & DEV_TX_OFFLOAD_UDP_TSO) == 0) || \
	 (ctx)->gso_size < RTE_GSO_UDP_SEG_SIZE_MIN)

#define ILLEGAL_TCP_GSO_CTX(ctx) \
	((((ctx)->gso_types & (DEV_TX_OFFLOAD_TCP_TSO | \
		DEV_TX_OFFLOAD_VXLAN_TNL_TSO | \
		DEV_TX_OFFLOAD_GRE_TNL_TSO)) == 0) || \
		(ctx)->gso_size < RTE_GSO_SEG_SIZE_MIN)

int
rte_gso_segment(struct rte_mbuf *pkt,
		const struct rte_gso_ctx *gso_ctx,
		struct rte_mbuf **pkts_out,
		uint16_t nb_pkts_out)
{
	struct rte_mempool *direct_pool, *indirect_pool;
	uint64_t ol_flags;
	uint16_t gso_size;
	uint8_t ipid_delta;
	int ret = 1;

	if (pkt == NULL || pkts_out == NULL || gso_ctx == NULL ||
			nb_pkts_out < 1 ||
			(ILLEGAL_UDP_GSO_CTX(gso_ctx) &&
			 ILLEGAL_TCP_GSO_CTX(gso_ctx)))
		return -EINVAL;

	if (gso_ctx->gso_size >= pkt->pkt_len) {
		pkt->ol_flags &= (~(PKT_TX_TCP_SEG | PKT_TX_UDP_SEG));
		return 0;
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
	} else if (IS_IPV4_UDP(pkt->ol_flags) &&
			(gso_ctx->gso_types & DEV_TX_OFFLOAD_UDP_TSO)) {
		pkt->ol_flags &= (~PKT_TX_UDP_SEG);
		ret = gso_udp4_segment(pkt, gso_size, direct_pool,
				indirect_pool, pkts_out, nb_pkts_out);
	} else {
		/* unsupported packet, skip */
		RTE_LOG(DEBUG, GSO, "Unsupported packet type\n");
		ret = 0;
	}

	if (ret < 0) {
		/* Revert the ol_flags in the event of failure. */
		pkt->ol_flags = ol_flags;
	}

	return ret;
}
