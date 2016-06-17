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

#ifndef __THUNDERX_NICVF_RXTX_H__
#define __THUNDERX_NICVF_RXTX_H__

#include <rte_byteorder.h>
#include <rte_ethdev.h>

#define NICVF_TX_OFFLOAD_MASK (PKT_TX_IP_CKSUM | PKT_TX_L4_MASK)

#ifndef __hot
#define __hot	__attribute__((hot))
#endif

#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
static inline uint16_t __attribute__((const))
nicvf_frag_num(uint16_t i)
{
	return (i & ~3) + 3 - (i & 3);
}

static inline void __hot
fill_sq_desc_gather(union sq_entry_t *entry, struct rte_mbuf *pkt)
{
	/* Local variable sqe to avoid read from sq desc memory*/
	union sq_entry_t sqe;

	/* Fill the SQ gather entry */
	sqe.buff[0] = 0; sqe.buff[1] = 0;
	sqe.gather.subdesc_type = SQ_DESC_TYPE_GATHER;
	sqe.gather.ld_type = NIC_SEND_LD_TYPE_E_LDT;
	sqe.gather.size = pkt->data_len;
	sqe.gather.addr = rte_mbuf_data_dma_addr(pkt);

	entry->buff[0] = sqe.buff[0];
	entry->buff[1] = sqe.buff[1];
}

#else

static inline uint16_t __attribute__((const))
nicvf_frag_num(uint16_t i)
{
	return i;
}

static inline void __hot
fill_sq_desc_gather(union sq_entry_t *entry, struct rte_mbuf *pkt)
{
	entry->buff[0] = (uint64_t)SQ_DESC_TYPE_GATHER << 60 |
			 (uint64_t)NIC_SEND_LD_TYPE_E_LDT << 58 |
			 pkt->data_len;
	entry->buff[1] = rte_mbuf_data_dma_addr(pkt);
}
#endif

uint32_t nicvf_dev_rx_queue_count(struct rte_eth_dev *dev, uint16_t queue_idx);
uint32_t nicvf_dev_rbdr_refill(struct rte_eth_dev *dev, uint16_t queue_idx);

uint16_t nicvf_recv_pkts(void *rxq, struct rte_mbuf **rx_pkts, uint16_t pkts);
uint16_t nicvf_recv_pkts_multiseg(void *rx_queue, struct rte_mbuf **rx_pkts,
				  uint16_t nb_pkts);

uint16_t nicvf_xmit_pkts(void *txq, struct rte_mbuf **tx_pkts, uint16_t pkts);
uint16_t nicvf_xmit_pkts_multiseg(void *txq, struct rte_mbuf **tx_pkts,
				  uint16_t pkts);

void nicvf_single_pool_free_xmited_buffers(struct nicvf_txq *sq);
void nicvf_multi_pool_free_xmited_buffers(struct nicvf_txq *sq);

#endif /* __THUNDERX_NICVF_RXTX_H__  */
