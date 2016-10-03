/*-
 *   BSD LICENSE
 *
 *   Copyright 2016 6WIND S.A.
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

#ifndef _RTE_NET_PTYPE_H_
#define _RTE_NET_PTYPE_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Structure containing header lengths associated to a packet, filled
 * by rte_net_get_ptype().
 */
struct rte_net_hdr_lens {
	uint8_t l2_len;
	uint8_t l3_len;
	uint8_t l4_len;
	uint8_t tunnel_len;
	uint8_t inner_l2_len;
	uint8_t inner_l3_len;
	uint8_t inner_l4_len;
};

/**
 * Parse an Ethernet packet to get its packet type.
 *
 * This function parses the network headers in mbuf data and return its
 * packet type.
 *
 * If it is provided by the user, it also fills a rte_net_hdr_lens
 * structure that contains the lengths of the parsed network
 * headers. Each length field is valid only if the associated packet
 * type is set. For instance, hdr_lens->l2_len is valid only if
 * (retval & RTE_PTYPE_L2_MASK) != RTE_PTYPE_UNKNOWN.
 *
 * Supported packet types are:
 *   L2: Ether, Vlan, QinQ
 *   L3: IPv4, IPv6
 *   L4: TCP, UDP, SCTP
 *   Tunnels: IPv4, IPv6, Gre, Nvgre
 *
 * @param m
 *   The packet mbuf to be parsed.
 * @param hdr_lens
 *   A pointer to a structure where the header lengths will be returned,
 *   or NULL.
 * @param layers
 *   List of layers to parse. The function will stop at the first
 *   empty layer. Examples:
 *   - To parse all known layers, use RTE_PTYPE_ALL_MASK.
 *   - To parse only L2 and L3, use RTE_PTYPE_L2_MASK | RTE_PTYPE_L3_MASK
 * @return
 *   The packet type of the packet.
 */
uint32_t rte_net_get_ptype(const struct rte_mbuf *m,
	struct rte_net_hdr_lens *hdr_lens, uint32_t layers);

#ifdef __cplusplus
}
#endif


#endif /* _RTE_NET_PTYPE_H_ */
