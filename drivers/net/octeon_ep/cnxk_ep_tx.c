/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#include "cnxk_ep_vf.h"
#include "otx_ep_rxtx.h"

static uint32_t
cnxk_vf_update_read_index(struct otx_ep_instr_queue *iq)
{
	uint32_t val;

	/* Batch subtractions from the HW counter to reduce PCIe traffic
	 * This adds an extra local variable, but almost halves the
	 * number of PCIe writes.
	 */
	val = __atomic_load_n(iq->inst_cnt_ism, __ATOMIC_RELAXED);
	iq->inst_cnt += val - iq->inst_cnt_ism_prev;
	iq->inst_cnt_ism_prev = val;

	if (val > (uint32_t)(1 << 31)) {
		/* Only subtract the packet count in the HW counter
		 * when count above halfway to saturation.
		 */
		rte_write64((uint64_t)val, iq->inst_cnt_reg);
		rte_mb();

		rte_write64(OTX2_SDP_REQUEST_ISM, iq->inst_cnt_reg);
		while (__atomic_load_n(iq->inst_cnt_ism, __ATOMIC_RELAXED) >= val) {
			rte_write64(OTX2_SDP_REQUEST_ISM, iq->inst_cnt_reg);
			rte_mb();
		}

		iq->inst_cnt_ism_prev = 0;
	}
	rte_write64(OTX2_SDP_REQUEST_ISM, iq->inst_cnt_reg);

	/* Modulo of the new index with the IQ size will give us
	 * the new index.
	 */
	return iq->inst_cnt & (iq->nb_desc - 1);
}

static inline void
cnxk_ep_flush_iq(struct otx_ep_instr_queue *iq)
{
	uint32_t instr_processed = 0;
	uint32_t cnt = 0;

	iq->otx_read_index = cnxk_vf_update_read_index(iq);

	if (unlikely(iq->flush_index == iq->otx_read_index))
		return;

	if (iq->flush_index < iq->otx_read_index) {
		instr_processed = iq->otx_read_index - iq->flush_index;
		rte_pktmbuf_free_bulk(&iq->mbuf_list[iq->flush_index], instr_processed);
		iq->flush_index = otx_ep_incr_index(iq->flush_index, instr_processed, iq->nb_desc);
	} else {
		cnt = iq->nb_desc - iq->flush_index;
		rte_pktmbuf_free_bulk(&iq->mbuf_list[iq->flush_index], cnt);
		iq->flush_index = otx_ep_incr_index(iq->flush_index, cnt, iq->nb_desc);

		instr_processed = iq->otx_read_index;
		rte_pktmbuf_free_bulk(&iq->mbuf_list[iq->flush_index], instr_processed);
		iq->flush_index = otx_ep_incr_index(iq->flush_index, instr_processed, iq->nb_desc);

		instr_processed += cnt;
	}

	iq->stats.instr_processed = instr_processed;
	iq->instr_pending -= instr_processed;
}

static inline void
set_sg_size(struct otx_ep_sg_entry *sg_entry, uint16_t size, uint32_t pos)
{
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
	sg_entry->u.size[pos] = size;
#elif RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	sg_entry->u.size[(OTX_EP_NUM_SG_PTRS - 1) - pos] = size;
#endif
}

static __rte_always_inline void
cnxk_ep_xmit_pkts_scalar(struct rte_mbuf **tx_pkts, struct otx_ep_instr_queue *iq, uint16_t nb_pkts)
{
	struct cnxk_ep_instr_32B *iqcmd;
	struct rte_mbuf *m;
	uint32_t pkt_len;
	uint32_t tx_bytes = 0;
	uint32_t write_idx = iq->host_write_index;
	uint16_t pkts, nb_desc = iq->nb_desc;
	uint8_t desc_size = iq->desc_size;

	for (pkts = 0; pkts < nb_pkts; pkts++) {
		m = tx_pkts[pkts];
		iq->mbuf_list[write_idx] = m;
		pkt_len = rte_pktmbuf_data_len(m);

		iqcmd = (struct cnxk_ep_instr_32B *)(iq->base_addr + (write_idx * desc_size));
		iqcmd->ih.u64 = iq->partial_ih | pkt_len;
		iqcmd->dptr = rte_mbuf_data_iova(m); /*dptr*/
		tx_bytes += pkt_len;

		/* Increment the host write index */
		write_idx = otx_ep_incr_index(write_idx, 1, nb_desc);
	}
	iq->host_write_index = write_idx;

	/* ring dbell */
	rte_io_wmb();
	rte_write64(pkts, iq->doorbell_reg);
	iq->instr_pending += pkts;
	iq->stats.tx_pkts += pkts;
	iq->stats.tx_bytes += tx_bytes;
}

static __rte_always_inline uint16_t
cnxk_ep_xmit_pkts_scalar_mseg(struct rte_mbuf **tx_pkts, struct otx_ep_instr_queue *iq,
			      uint16_t nb_pkts)
{
	uint16_t frags, num_sg, mask = OTX_EP_NUM_SG_PTRS - 1;
	struct otx_ep_buf_free_info *finfo;
	struct cnxk_ep_instr_32B *iqcmd;
	struct rte_mbuf *m;
	uint32_t pkt_len, tx_bytes = 0;
	uint32_t write_idx = iq->host_write_index;
	uint16_t pkts, nb_desc = iq->nb_desc;
	uint8_t desc_size = iq->desc_size;

	for (pkts = 0; pkts < nb_pkts; pkts++) {
		uint16_t j = 0;

		m = tx_pkts[pkts];
		frags = m->nb_segs;

		pkt_len = rte_pktmbuf_pkt_len(m);
		num_sg = (frags + mask) / OTX_EP_NUM_SG_PTRS;

		if (unlikely(pkt_len > OTX_EP_MAX_PKT_SZ && num_sg > OTX_EP_MAX_SG_LISTS)) {
			otx_ep_err("Failed to xmit the pkt, pkt_len is higher or pkt has more segments\n");
			goto exit;
		}

		finfo = &iq->req_list[write_idx].finfo;

		iq->mbuf_list[write_idx] = m;
		iqcmd = (struct cnxk_ep_instr_32B *)(iq->base_addr + (write_idx * desc_size));
		iqcmd->dptr = rte_mem_virt2iova(finfo->g.sg);
		iqcmd->ih.u64 = iq->partial_ih | (1ULL << 62) | ((uint64_t)frags << 48) | pkt_len;

		while (frags--) {
			finfo->g.sg[(j >> 2)].ptr[(j & mask)] = rte_mbuf_data_iova(m);
			set_sg_size(&finfo->g.sg[(j >> 2)], m->data_len, (j & mask));
			j++;
			m = m->next;
		}

		/* Increment the host write index */
		write_idx = otx_ep_incr_index(write_idx, 1, nb_desc);
		tx_bytes += pkt_len;
	}
exit:
	iq->host_write_index = write_idx;

	/* ring dbell */
	rte_io_wmb();
	rte_write64(pkts, iq->doorbell_reg);
	iq->instr_pending += pkts;
	iq->stats.tx_pkts += pkts;
	iq->stats.tx_bytes += tx_bytes;

	return pkts;
}

uint16_t __rte_noinline __rte_hot
cnxk_ep_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct otx_ep_instr_queue *iq = (struct otx_ep_instr_queue *)tx_queue;
	uint16_t pkts;

	pkts = RTE_MIN(nb_pkts, iq->nb_desc - iq->instr_pending);

	cnxk_ep_xmit_pkts_scalar(tx_pkts, iq, pkts);

	if (iq->instr_pending >= OTX_EP_MAX_INSTR)
		cnxk_ep_flush_iq(iq);

	/* Return no# of instructions posted successfully. */
	return pkts;
}

uint16_t __rte_noinline __rte_hot
cnxk_ep_xmit_pkts_mseg(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct otx_ep_instr_queue *iq = (struct otx_ep_instr_queue *)tx_queue;
	uint16_t pkts;

	pkts = RTE_MIN(nb_pkts, iq->nb_desc - iq->instr_pending);

	pkts = cnxk_ep_xmit_pkts_scalar_mseg(tx_pkts, iq, pkts);

	if (iq->instr_pending >= OTX_EP_MAX_INSTR)
		cnxk_ep_flush_iq(iq);

	/* Return no# of instructions posted successfully. */
	return pkts;
}
