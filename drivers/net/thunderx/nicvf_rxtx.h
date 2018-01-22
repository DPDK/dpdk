/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Cavium, Inc
 */

#ifndef __THUNDERX_NICVF_RXTX_H__
#define __THUNDERX_NICVF_RXTX_H__

#include <rte_byteorder.h>
#include <rte_ethdev_driver.h>

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
	sqe.gather.addr = rte_mbuf_data_iova(pkt);

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
	entry->buff[1] = rte_mbuf_data_iova(pkt);
}
#endif

static inline void
nicvf_mbuff_init_update(struct rte_mbuf *pkt, const uint64_t mbuf_init,
				uint16_t apad)
{
	union mbuf_initializer init = {.value = mbuf_init};
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
	init.fields.data_off += apad;
#else
	init.value += apad;
#endif
	*(uint64_t *)(&pkt->rearm_data) = init.value;
}

static inline void
nicvf_mbuff_init_mseg_update(struct rte_mbuf *pkt, const uint64_t mbuf_init,
						uint16_t apad, uint16_t nb_segs)
{
	union mbuf_initializer init = {.value = mbuf_init};
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
	init.fields.data_off += apad;
#else
	init.value += apad;
#endif
	init.fields.nb_segs = nb_segs;
	*(uint64_t *)(&pkt->rearm_data) = init.value;
}

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
