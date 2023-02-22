/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2021 Xilinx, Inc.
 * Copyright(c) 2016-2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#include <rte_ip.h>
#include <rte_tcp.h>

#include "sfc.h"
#include "sfc_debug.h"
#include "sfc_tx.h"
#include "sfc_ev.h"
#include "sfc_tso.h"

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

unsigned int
sfc_tso_prepare_header(uint8_t *tsoh, size_t header_len,
		       struct rte_mbuf **in_seg, size_t *in_off)
{
	struct rte_mbuf *m = *in_seg;
	size_t bytes_to_copy = 0;
	size_t bytes_left = header_len;
	unsigned int segments_copied = 0;

	do {
		bytes_to_copy = MIN(bytes_left, m->data_len);

		rte_memcpy(tsoh, rte_pktmbuf_mtod(m, uint8_t *),
			   bytes_to_copy);

		bytes_left -= bytes_to_copy;
		tsoh += bytes_to_copy;

		if (bytes_left > 0) {
			m = m->next;
			SFC_ASSERT(m != NULL);
			segments_copied++;
		}
	} while (bytes_left > 0);

	if (bytes_to_copy == m->data_len) {
		*in_seg = m->next;
		*in_off = 0;
		segments_copied++;
	} else {
		*in_seg = m;
		*in_off = bytes_to_copy;
	}

	return segments_copied;
}

int
sfc_efx_tso_do(struct sfc_efx_txq *txq, unsigned int idx,
	       struct rte_mbuf **in_seg, size_t *in_off, efx_desc_t **pend,
	       unsigned int *pkt_descs, size_t *pkt_len)
{
	uint8_t *tsoh;
	const struct rte_tcp_hdr *th;
	efsys_dma_addr_t header_paddr;
	uint16_t packet_id = 0;
	uint32_t sent_seq;
	struct rte_mbuf *m = *in_seg;
	size_t nh_off = m->l2_len; /* IP header offset */
	size_t tcph_off = m->l2_len + m->l3_len; /* TCP header offset */
	size_t header_len = m->l2_len + m->l3_len + m->l4_len;

	idx += SFC_EF10_TSO_OPT_DESCS_NUM;

	header_paddr = rte_pktmbuf_iova(m);

	/*
	 * Sometimes headers may be split across multiple mbufs. In such cases
	 * we need to glue those pieces and store them in some temporary place.
	 * Also, packet headers must be contiguous in memory, so that
	 * they can be referred to with a single DMA descriptor. EF10 has no
	 * limitations on address boundaries crossing by DMA descriptor data.
	 */
	if (m->data_len < header_len) {
		/*
		 * Discard a packet if header linearization is needed but
		 * the header is too big.
		 * Duplicate Tx prepare check here to avoid spoil of
		 * memory if Tx prepare is skipped.
		 */
		if (unlikely(header_len > SFC_TSOH_STD_LEN))
			return EMSGSIZE;

		tsoh = txq->sw_ring[idx & txq->ptr_mask].tsoh;
		sfc_tso_prepare_header(tsoh, header_len, in_seg, in_off);

		header_paddr = rte_malloc_virt2iova((void *)tsoh);
	} else {
		if (m->data_len == header_len) {
			*in_off = 0;
			*in_seg = m->next;
		} else {
			*in_off = header_len;
		}

		tsoh = rte_pktmbuf_mtod(m, uint8_t *);
	}

	/*
	 * 8000-series EF10 hardware requires that innermost IP length
	 * be greater than or equal to the value which each segment is
	 * supposed to have; otherwise, TCP checksum will be incorrect.
	 */
	sfc_tso_innermost_ip_fix_len(m, tsoh, nh_off);

	/*
	 * Handle IP header. Tx prepare has debug-only checks that offload flags
	 * are correctly filled in TSO mbuf. Use zero IPID if there is no
	 * IPv4 flag. If the packet is still IPv4, HW will simply start from
	 * zero IPID.
	 */
	if (m->ol_flags & RTE_MBUF_F_TX_IPV4)
		packet_id = sfc_tso_ip4_get_ipid(tsoh, nh_off);

	/* Handle TCP header */
	th = (const struct rte_tcp_hdr *)(tsoh + tcph_off);

	rte_memcpy(&sent_seq, &th->sent_seq, sizeof(uint32_t));
	sent_seq = rte_be_to_cpu_32(sent_seq);

	efx_tx_qdesc_tso2_create(txq->common, packet_id, 0, sent_seq,
				 m->tso_segsz,
				 *pend, EFX_TX_FATSOV2_OPT_NDESCS);

	*pend += EFX_TX_FATSOV2_OPT_NDESCS;
	*pkt_descs += EFX_TX_FATSOV2_OPT_NDESCS;

	efx_tx_qdesc_dma_create(txq->common, header_paddr, header_len,
				B_FALSE, (*pend)++);
	(*pkt_descs)++;
	*pkt_len -= header_len;

	return 0;
}
