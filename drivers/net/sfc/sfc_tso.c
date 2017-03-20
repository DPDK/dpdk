/*-
 *   BSD LICENSE
 *
 * Copyright (c) 2016-2017 Solarflare Communications Inc.
 * All rights reserved.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <rte_ip.h>
#include <rte_tcp.h>

#include "sfc.h"
#include "sfc_debug.h"
#include "sfc_tx.h"
#include "sfc_ev.h"

/** Standard TSO header length */
#define SFC_TSOH_STD_LEN        256

/** The number of TSO option descriptors that precede the packet descriptors */
#define SFC_TSO_OPDESCS_IDX_SHIFT	2

int
sfc_efx_tso_alloc_tsoh_objs(struct sfc_efx_tx_sw_desc *sw_ring,
			    unsigned int txq_entries, unsigned int socket_id)
{
	unsigned int i;

	for (i = 0; i < txq_entries; ++i) {
		sw_ring[i].tsoh = rte_malloc_socket("sfc-efx-txq-tsoh-obj",
						    SFC_TSOH_STD_LEN,
						    RTE_CACHE_LINE_SIZE,
						    socket_id);
		if (sw_ring[i].tsoh == NULL)
			goto fail_alloc_tsoh_objs;
	}

	return 0;

fail_alloc_tsoh_objs:
	while (i > 0)
		rte_free(sw_ring[--i].tsoh);

	return ENOMEM;
}

void
sfc_efx_tso_free_tsoh_objs(struct sfc_efx_tx_sw_desc *sw_ring,
			   unsigned int txq_entries)
{
	unsigned int i;

	for (i = 0; i < txq_entries; ++i) {
		rte_free(sw_ring[i].tsoh);
		sw_ring[i].tsoh = NULL;
	}
}

static void
sfc_efx_tso_prepare_header(struct sfc_efx_txq *txq, struct rte_mbuf **in_seg,
			   size_t *in_off, unsigned int idx, size_t bytes_left)
{
	struct rte_mbuf *m = *in_seg;
	size_t bytes_to_copy = 0;
	uint8_t *tsoh = txq->sw_ring[idx & txq->ptr_mask].tsoh;

	do {
		bytes_to_copy = MIN(bytes_left, m->data_len);

		rte_memcpy(tsoh, rte_pktmbuf_mtod(m, uint8_t *),
			   bytes_to_copy);

		bytes_left -= bytes_to_copy;
		tsoh += bytes_to_copy;

		if (bytes_left > 0) {
			m = m->next;
			SFC_ASSERT(m != NULL);
		}
	} while (bytes_left > 0);

	if (bytes_to_copy == m->data_len) {
		*in_seg = m->next;
		*in_off = 0;
	} else {
		*in_seg = m;
		*in_off = bytes_to_copy;
	}
}

int
sfc_efx_tso_do(struct sfc_efx_txq *txq, unsigned int idx,
	       struct rte_mbuf **in_seg, size_t *in_off, efx_desc_t **pend,
	       unsigned int *pkt_descs, size_t *pkt_len)
{
	uint8_t *tsoh;
	const struct tcp_hdr *th;
	efsys_dma_addr_t header_paddr;
	uint16_t packet_id;
	uint32_t sent_seq;
	struct rte_mbuf *m = *in_seg;
	size_t nh_off = m->l2_len; /* IP header offset */
	size_t tcph_off = m->l2_len + m->l3_len; /* TCP header offset */
	size_t header_len = m->l2_len + m->l3_len + m->l4_len;
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(txq->evq->sa->nic);

	idx += SFC_TSO_OPDESCS_IDX_SHIFT;

	/* Packets which have too big headers should be discarded */
	if (unlikely(header_len > SFC_TSOH_STD_LEN))
		return EMSGSIZE;

	/*
	 * The TCP header must start at most 208 bytes into the frame.
	 * If it starts later than this then the NIC won't realise
	 * it's a TCP packet and TSO edits won't be applied
	 */
	if (unlikely(tcph_off > encp->enc_tx_tso_tcp_header_offset_limit))
		return EMSGSIZE;

	header_paddr = rte_pktmbuf_mtophys(m);

	/*
	 * Sometimes headers may be split across multiple mbufs. In such cases
	 * we need to glue those pieces and store them in some temporary place.
	 * Also, packet headers must be contiguous in memory, so that
	 * they can be referred to with a single DMA descriptor. EF10 has no
	 * limitations on address boundaries crossing by DMA descriptor data.
	 */
	if (m->data_len < header_len) {
		sfc_efx_tso_prepare_header(txq, in_seg, in_off, idx,
					   header_len);
		tsoh = txq->sw_ring[idx & txq->ptr_mask].tsoh;

		header_paddr = rte_malloc_virt2phy((void *)tsoh);
	} else {
		if (m->data_len == header_len) {
			*in_off = 0;
			*in_seg = m->next;
		} else {
			*in_off = header_len;
		}

		tsoh = rte_pktmbuf_mtod(m, uint8_t *);
	}

	/* Handle IP header */
	if (m->ol_flags & PKT_TX_IPV4) {
		const struct ipv4_hdr *iphe4;

		iphe4 = (const struct ipv4_hdr *)(tsoh + nh_off);
		rte_memcpy(&packet_id, &iphe4->packet_id, sizeof(uint16_t));
		packet_id = rte_be_to_cpu_16(packet_id);
	} else if (m->ol_flags & PKT_TX_IPV6) {
		packet_id = 0;
	} else {
		return EINVAL;
	}

	/* Handle TCP header */
	th = (const struct tcp_hdr *)(tsoh + tcph_off);

	rte_memcpy(&sent_seq, &th->sent_seq, sizeof(uint32_t));
	sent_seq = rte_be_to_cpu_32(sent_seq);

	efx_tx_qdesc_tso2_create(txq->common, packet_id, sent_seq, m->tso_segsz,
				 *pend, EFX_TX_FATSOV2_OPT_NDESCS);

	*pend += EFX_TX_FATSOV2_OPT_NDESCS;
	*pkt_descs += EFX_TX_FATSOV2_OPT_NDESCS;

	efx_tx_qdesc_dma_create(txq->common, header_paddr, header_len,
				B_FALSE, (*pend)++);
	(*pkt_descs)++;
	*pkt_len -= header_len;

	return 0;
}
