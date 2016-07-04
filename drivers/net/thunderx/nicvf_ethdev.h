/*
 *   BSD LICENSE
 *
 *   Copyright (C) Cavium networks Ltd. 2016.
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
 *     * Neither the name of Cavium networks nor the names of its
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

#ifndef __THUNDERX_NICVF_ETHDEV_H__
#define __THUNDERX_NICVF_ETHDEV_H__

#include <rte_ethdev.h>

#define THUNDERX_NICVF_PMD_VERSION      "1.0"
#define THUNDERX_REG_BYTES		8

#define NICVF_INTR_POLL_INTERVAL_MS	50
#define NICVF_HALF_DUPLEX		0x00
#define NICVF_FULL_DUPLEX		0x01
#define NICVF_UNKNOWN_DUPLEX		0xff

#define NICVF_RSS_OFFLOAD_PASS1 ( \
	ETH_RSS_PORT | \
	ETH_RSS_IPV4 | \
	ETH_RSS_NONFRAG_IPV4_TCP | \
	ETH_RSS_NONFRAG_IPV4_UDP | \
	ETH_RSS_IPV6 | \
	ETH_RSS_NONFRAG_IPV6_TCP | \
	ETH_RSS_NONFRAG_IPV6_UDP)

#define NICVF_RSS_OFFLOAD_TUNNEL ( \
	ETH_RSS_VXLAN | \
	ETH_RSS_GENEVE | \
	ETH_RSS_NVGRE)

#define NICVF_DEFAULT_RX_FREE_THRESH    224
#define NICVF_DEFAULT_TX_FREE_THRESH    224
#define NICVF_TX_FREE_MPOOL_THRESH      16
#define NICVF_MAX_RX_FREE_THRESH        1024
#define NICVF_MAX_TX_FREE_THRESH        1024

#define VLAN_TAG_SIZE                   4	/* 802.3ac tag */

static inline struct nicvf *
nicvf_pmd_priv(struct rte_eth_dev *eth_dev)
{
	return eth_dev->data->dev_private;
}

static inline uint64_t
nicvf_mempool_phy_offset(struct rte_mempool *mp)
{
	struct rte_mempool_memhdr *hdr;

	hdr = STAILQ_FIRST(&mp->mem_list);
	assert(hdr != NULL);
	return (uint64_t)((uintptr_t)hdr->addr - hdr->phys_addr);
}

static inline uint16_t
nicvf_mbuff_meta_length(struct rte_mbuf *mbuf)
{
	return (uint16_t)((uintptr_t)mbuf->buf_addr - (uintptr_t)mbuf);
}

/*
 * Simple phy2virt functions assuming mbufs are in a single huge page
 * V = P + offset
 * P = V - offset
 */
static inline uintptr_t
nicvf_mbuff_phy2virt(phys_addr_t phy, uint64_t mbuf_phys_off)
{
	return (uintptr_t)(phy + mbuf_phys_off);
}

static inline uintptr_t
nicvf_mbuff_virt2phy(uintptr_t virt, uint64_t mbuf_phys_off)
{
	return (phys_addr_t)(virt - mbuf_phys_off);
}

#endif /* __THUNDERX_NICVF_ETHDEV_H__  */
