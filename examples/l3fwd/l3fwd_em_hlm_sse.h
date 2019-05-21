/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Intel Corporation
 */

#ifndef __L3FWD_EM_HLM_SSE_H__
#define __L3FWD_EM_HLM_SSE_H__

#include "l3fwd_sse.h"

static __rte_always_inline void
get_ipv4_5tuple(struct rte_mbuf *m0, __m128i mask0,
		union ipv4_5tuple_host *key)
{
	 __m128i tmpdata0 = _mm_loadu_si128(
			rte_pktmbuf_mtod_offset(m0, __m128i *,
				sizeof(struct rte_ether_hdr) +
				offsetof(struct rte_ipv4_hdr, time_to_live)));

	key->xmm = _mm_and_si128(tmpdata0, mask0);
}

static inline void
get_ipv6_5tuple(struct rte_mbuf *m0, __m128i mask0,
		__m128i mask1, union ipv6_5tuple_host *key)
{
	__m128i tmpdata0 = _mm_loadu_si128(
			rte_pktmbuf_mtod_offset(m0, __m128i *,
				sizeof(struct rte_ether_hdr) +
				offsetof(struct rte_ipv6_hdr, payload_len)));

	__m128i tmpdata1 = _mm_loadu_si128(
			rte_pktmbuf_mtod_offset(m0, __m128i *,
				sizeof(struct rte_ether_hdr) +
				offsetof(struct rte_ipv6_hdr, payload_len) +
				sizeof(__m128i)));

	__m128i tmpdata2 = _mm_loadu_si128(
			rte_pktmbuf_mtod_offset(m0, __m128i *,
				sizeof(struct rte_ether_hdr) +
				offsetof(struct rte_ipv6_hdr, payload_len) +
				sizeof(__m128i) + sizeof(__m128i)));

	key->xmm[0] = _mm_and_si128(tmpdata0, mask0);
	key->xmm[1] = tmpdata1;
	key->xmm[2] = _mm_and_si128(tmpdata2, mask1);
}
#endif /* __L3FWD_EM_SSE_HLM_H__ */
