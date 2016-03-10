/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2016 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef __INCLUDE_PIPELINE_ACTIONS_COMMON_H__
#define __INCLUDE_PIPELINE_ACTIONS_COMMON_H__

#include <stdint.h>

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_mbuf.h>
#include <rte_pipeline.h>

#define PIPELINE_PORT_IN_AH(f_ah, f_pkt_work, f_pkt4_work)		\
static int								\
f_ah(									\
	__rte_unused struct rte_pipeline *p,				\
	struct rte_mbuf **pkts,						\
	uint32_t n_pkts,						\
	void *arg)							\
{									\
	uint32_t i;							\
									\
	for (i = 0; i < (n_pkts & (~0x3LLU)); i += 4)			\
		f_pkt4_work(&pkts[i], arg);				\
									\
	for ( ; i < n_pkts; i++)					\
		f_pkt_work(pkts[i], arg);				\
									\
	return 0;							\
}

#define PIPELINE_PORT_IN_AH_HIJACK_ALL(f_ah, f_pkt_work, f_pkt4_work) \
static int								\
f_ah(									\
	struct rte_pipeline *p,				\
	struct rte_mbuf **pkts,					\
	uint32_t n_pkts,						\
	void *arg)						\
{									\
	uint64_t pkt_mask = RTE_LEN2MASK(n_pkts, uint64_t);	\
	uint32_t i;							\
									\
	rte_pipeline_ah_packet_hijack(p, pkt_mask);	\
									\
	for (i = 0; i < (n_pkts & (~0x3LLU)); i += 4)	\
		f_pkt4_work(&pkts[i], arg);				\
									\
	for ( ; i < n_pkts; i++)				\
		f_pkt_work(pkts[i], arg);			\
									\
	return 0;							\
}

#define PIPELINE_TABLE_AH_HIT(f_ah, f_pkt_work, f_pkt4_work)		\
static int								\
f_ah(									\
	__rte_unused struct rte_pipeline *p,				\
	struct rte_mbuf **pkts,						\
	uint64_t pkts_in_mask,						\
	struct rte_pipeline_table_entry **entries,			\
	void *arg)							\
{									\
	if ((pkts_in_mask & (pkts_in_mask + 1)) == 0) {			\
		uint64_t n_pkts = __builtin_popcountll(pkts_in_mask);	\
		uint32_t i;						\
									\
		for (i = 0; i < (n_pkts & (~0x3LLU)); i += 4)		\
			f_pkt4_work(&pkts[i], &entries[i], arg);	\
									\
		for ( ; i < n_pkts; i++)				\
			f_pkt_work(pkts[i], entries[i], arg);		\
	} else								\
		for ( ; pkts_in_mask; ) {				\
			uint32_t pos = __builtin_ctzll(pkts_in_mask);	\
			uint64_t pkt_mask = 1LLU << pos;		\
									\
			pkts_in_mask &= ~pkt_mask;			\
			f_pkt_work(pkts[pos], entries[pos], arg);	\
		}							\
									\
	return 0;							\
}

#define PIPELINE_TABLE_AH_MISS(f_ah, f_pkt_work, f_pkt4_work)		\
static int								\
f_ah(									\
	__rte_unused struct rte_pipeline *p,				\
	struct rte_mbuf **pkts,						\
	uint64_t pkts_in_mask,						\
	struct rte_pipeline_table_entry *entry,				\
	void *arg)							\
{									\
	if ((pkts_in_mask & (pkts_in_mask + 1)) == 0) {			\
		uint64_t n_pkts = __builtin_popcountll(pkts_in_mask);	\
		uint32_t i;						\
									\
		for (i = 0; i < (n_pkts & (~0x3LLU)); i += 4)		\
			f_pkt4_work(&pkts[i], entry, arg);		\
									\
		for ( ; i < n_pkts; i++)				\
			f_pkt_work(pkts[i], entry, arg);		\
	} else								\
		for ( ; pkts_in_mask; ) {				\
			uint32_t pos = __builtin_ctzll(pkts_in_mask);	\
			uint64_t pkt_mask = 1LLU << pos;		\
									\
			pkts_in_mask &= ~pkt_mask;			\
			f_pkt_work(pkts[pos], entry, arg);		\
		}							\
									\
	return 0;							\
}

#define PIPELINE_TABLE_AH_HIT_DROP_TIME(f_ah, f_pkt_work, f_pkt4_work)	\
static int								\
f_ah(									\
	struct rte_pipeline *p,						\
	struct rte_mbuf **pkts,						\
	uint64_t pkts_mask,						\
	struct rte_pipeline_table_entry **entries,			\
	void *arg)							\
{									\
	uint64_t pkts_in_mask = pkts_mask;				\
	uint64_t pkts_out_mask = pkts_mask;				\
	uint64_t time = rte_rdtsc();					\
									\
	if ((pkts_in_mask & (pkts_in_mask + 1)) == 0) {			\
		uint64_t n_pkts = __builtin_popcountll(pkts_in_mask);	\
		uint32_t i;						\
									\
		for (i = 0; i < (n_pkts & (~0x3LLU)); i += 4) {		\
			uint64_t mask = f_pkt4_work(&pkts[i],		\
				&entries[i], arg, time);		\
			pkts_out_mask ^= mask << i;			\
		}							\
									\
		for ( ; i < n_pkts; i++) {				\
			uint64_t mask = f_pkt_work(pkts[i],		\
				entries[i], arg, time);			\
			pkts_out_mask ^= mask << i;			\
		}							\
	} else								\
		for ( ; pkts_in_mask; ) {				\
			uint32_t pos = __builtin_ctzll(pkts_in_mask);	\
			uint64_t pkt_mask = 1LLU << pos;		\
			uint64_t mask = f_pkt_work(pkts[pos],		\
				entries[pos], arg, time);		\
									\
			pkts_in_mask &= ~pkt_mask;			\
			pkts_out_mask ^= mask << pos;			\
		}							\
									\
	rte_pipeline_ah_packet_drop(p, pkts_out_mask ^ pkts_mask);	\
									\
	return 0;							\
}

#define PIPELINE_TABLE_AH_MISS_DROP_TIME(f_ah, f_pkt_work, f_pkt4_work)	\
static int								\
f_ah(									\
	struct rte_pipeline *p,						\
	struct rte_mbuf **pkts,						\
	uint64_t pkts_mask,						\
	struct rte_pipeline_table_entry *entry,				\
	void *arg)							\
{									\
	uint64_t pkts_in_mask = pkts_mask;				\
	uint64_t pkts_out_mask = pkts_mask;				\
	uint64_t time = rte_rdtsc();					\
									\
	if ((pkts_in_mask & (pkts_in_mask + 1)) == 0) {			\
		uint64_t n_pkts = __builtin_popcountll(pkts_in_mask);	\
		uint32_t i;						\
									\
		for (i = 0; i < (n_pkts & (~0x3LLU)); i += 4) {		\
			uint64_t mask = f_pkt4_work(&pkts[i],		\
				entry, arg, time);			\
			pkts_out_mask ^= mask << i;			\
		}							\
									\
		for ( ; i < n_pkts; i++) {				\
			uint64_t mask = f_pkt_work(pkts[i], entry, arg, time);\
			pkts_out_mask ^= mask << i;			\
		}							\
	} else								\
		for ( ; pkts_in_mask; ) {				\
			uint32_t pos = __builtin_ctzll(pkts_in_mask);	\
			uint64_t pkt_mask = 1LLU << pos;		\
			uint64_t mask = f_pkt_work(pkts[pos],		\
				entry, arg, time);		\
									\
			pkts_in_mask &= ~pkt_mask;			\
			pkts_out_mask ^= mask << pos;			\
		}							\
									\
	rte_pipeline_ah_packet_drop(p, pkts_out_mask ^ pkts_mask);	\
									\
	return 0;							\
}

#endif
