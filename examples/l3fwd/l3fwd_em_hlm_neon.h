/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2018 Intel Corporation.
 * Copyright(c) 2017-2018 Linaro Limited.
 */

#ifndef __L3FWD_EM_HLM_NEON_H__
#define __L3FWD_EM_HLM_NEON_H__

#include <arm_neon.h>

static inline void
get_ipv4_5tuple(struct rte_mbuf *m0, int32x4_t mask0,
		union ipv4_5tuple_host *key)
{
	int32x4_t tmpdata0 = vld1q_s32(rte_pktmbuf_mtod_offset(m0, int32_t *,
				sizeof(struct ether_hdr) +
				offsetof(struct ipv4_hdr, time_to_live)));

	key->xmm = vandq_s32(tmpdata0, mask0);
}

static inline void
get_ipv6_5tuple(struct rte_mbuf *m0, int32x4_t mask0,
		int32x4_t mask1, union ipv6_5tuple_host *key)
{
	int32x4_t tmpdata0 = vld1q_s32(
			rte_pktmbuf_mtod_offset(m0, int *,
				sizeof(struct ether_hdr) +
				offsetof(struct ipv6_hdr, payload_len)));

	int32x4_t tmpdata1 = vld1q_s32(
			rte_pktmbuf_mtod_offset(m0, int *,
				sizeof(struct ether_hdr) +
				offsetof(struct ipv6_hdr, payload_len) + 8));

	int32x4_t tmpdata2 = vld1q_s32(
			rte_pktmbuf_mtod_offset(m0, int *,
				sizeof(struct ether_hdr) +
				offsetof(struct ipv6_hdr, payload_len) + 16));

	key->xmm[0] = vandq_s32(tmpdata0, mask0);
	key->xmm[1] = tmpdata1;
	key->xmm[2] = vandq_s32(tmpdata2, mask1);
}
#endif /* __L3FWD_EM_HLM_NEON_H__ */
