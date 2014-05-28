/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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

#ifndef _RTE_IP_FRAG_H__
#define _RTE_IP_FRAG_H__

/**
 * @file
 * RTE IPv4 Fragmentation
 *
 * Implementation of IPv4 fragmentation.
 *
 */

/**
 * IPv4 fragmentation.
 *
 * This function implements the fragmentation of IPv4 packets.
 *
 * @param pkt_in
 *   The input packet.
 * @param pkts_out
 *   Array storing the output fragments.
 * @param nb_pkts_out
 *   Number of fragments.
 * @param mtu_size
 *   Size in bytes of the Maximum Transfer Unit (MTU) for the outgoing IPv4
 *   datagrams. This value includes the size of the IPv4 header.
 * @param pool_direct
 *   MBUF pool used for allocating direct buffers for the output fragments.
 * @param pool_indirect
 *   MBUF pool used for allocating indirect buffers for the output fragments.
 * @return
 *   Upon successful completion - number of output fragments placed
 *   in the pkts_out array.
 *   Otherwise - (-1) * errno.
 */
int32_t rte_ipv4_fragmentation(struct rte_mbuf *pkt_in,
	struct rte_mbuf **pkts_out,
	uint16_t nb_pkts_out,
	uint16_t mtu_size,
	struct rte_mempool *pool_direct,
	struct rte_mempool *pool_indirect);

#endif
