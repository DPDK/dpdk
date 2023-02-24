/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef __CN9K_IPSEC_LA_OPS_H__
#define __CN9K_IPSEC_LA_OPS_H__

#include <rte_crypto_sym.h>
#include <rte_esp.h>
#include <rte_security.h>

#include "roc_ie.h"

#include "cn9k_ipsec.h"
#include "cnxk_cryptodev_ops.h"
#include "cnxk_security_ar.h"
#include "cnxk_sg.h"

static __rte_always_inline int32_t
ipsec_po_out_rlen_get(struct cn9k_sec_session *sess, uint32_t plen, struct rte_mbuf *m_src)
{
	uint32_t enc_payload_len;
	int adj_len = 0;

	if (sess->sa.out_sa.common_sa.ctl.ipsec_mode == ROC_IE_SA_MODE_TRANSPORT) {
		adj_len = ROC_CPT_TUNNEL_IPV4_HDR_LEN;

		uintptr_t data = (uintptr_t)m_src->buf_addr + m_src->data_off;
		struct rte_ipv4_hdr *ip = (struct rte_ipv4_hdr *)data;

		if (unlikely(ip->version != IPVERSION)) {
			struct rte_ipv6_hdr *ip6 = (struct rte_ipv6_hdr *)ip;
			uint8_t *nxt_hdr = (uint8_t *)ip6;
			uint8_t dest_op_cnt = 0;
			int nh = ip6->proto;

			PLT_ASSERT(ip->version == 6);

			adj_len = ROC_CPT_TUNNEL_IPV6_HDR_LEN;
			nxt_hdr += ROC_CPT_TUNNEL_IPV6_HDR_LEN;
			while (nh != -EINVAL) {
				size_t ext_len = 0;

				nh = rte_ipv6_get_next_ext(nxt_hdr, nh, &ext_len);
				/* With multiple dest ops headers, the ESP hdr will be before
				 * the 2nd dest ops and after the first dest ops header
				 */
				if ((nh == IPPROTO_DSTOPTS) && dest_op_cnt)
					break;
				else if (nh == IPPROTO_DSTOPTS)
					dest_op_cnt++;
				adj_len += ext_len;
				nxt_hdr += ext_len;
			}
		}
	}

	enc_payload_len =
		RTE_ALIGN_CEIL(plen + sess->rlens.roundup_len - adj_len, sess->rlens.roundup_byte);

	return sess->custom_hdr_len + sess->rlens.partial_len + enc_payload_len + adj_len;
}

static __rte_always_inline int
process_outb_sa(struct cpt_qp_meta_info *m_info, struct rte_crypto_op *cop,
		struct cn9k_sec_session *sess, struct cpt_inst_s *inst,
		struct cpt_inflight_req *infl_req)
{
	const unsigned int hdr_len = sess->custom_hdr_len;
	struct rte_crypto_sym_op *sym_op = cop->sym;
	struct rte_mbuf *m_src = sym_op->m_src;
	uint32_t dlen, rlen, pkt_len, seq_lo;
	uint16_t data_off = m_src->data_off;
	struct roc_ie_on_outb_hdr *hdr;
	int32_t extend_tail;
	uint64_t esn;

	pkt_len = rte_pktmbuf_pkt_len(m_src);
	dlen = pkt_len + hdr_len;
	rlen = ipsec_po_out_rlen_get(sess, pkt_len, m_src);

	extend_tail = rlen - dlen;
	pkt_len += extend_tail;

	if (likely(m_src->next == NULL)) {
		if (unlikely(extend_tail > rte_pktmbuf_tailroom(m_src))) {
			plt_dp_err("Not enough tail room (required: %d, available: %d)",
				   extend_tail, rte_pktmbuf_tailroom(m_src));
			return -ENOMEM;
		}

		if (unlikely(hdr_len > data_off)) {
			plt_dp_err("Not enough head room (required: %d, available: %d)", hdr_len,
				   rte_pktmbuf_headroom(m_src));
			return -ENOMEM;
		}

		m_src->data_len = pkt_len;

		hdr = PLT_PTR_ADD(m_src->buf_addr, data_off - hdr_len);

		inst->dptr = PLT_U64_CAST(hdr);
		inst->w4.u64 = sess->inst.w4 | dlen;
	} else {
		struct roc_sglist_comp *scatter_comp, *gather_comp;
		uint32_t g_size_bytes, s_size_bytes;
		struct rte_mbuf *last_seg;
		uint8_t *in_buffer;
		void *m_data;
		int i;

		last_seg = rte_pktmbuf_lastseg(m_src);

		if (unlikely(extend_tail > rte_pktmbuf_tailroom(last_seg))) {
			plt_dp_err("Not enough tail room (required: %d, available: %d)",
				   extend_tail, rte_pktmbuf_tailroom(last_seg));
			return -ENOMEM;
		}

		m_data = alloc_op_meta(NULL, m_info->mlen, m_info->pool, infl_req);
		if (unlikely(m_data == NULL)) {
			plt_dp_err("Error allocating meta buffer for request");
			return -ENOMEM;
		}

		hdr = m_data;

		m_data = (uint8_t *)m_data + hdr_len;
		in_buffer = m_data;

		((uint16_t *)in_buffer)[0] = 0;
		((uint16_t *)in_buffer)[1] = 0;

		/*
		 * Input Gather List
		 */
		i = 0;
		gather_comp = (struct roc_sglist_comp *)((uint8_t *)m_data + 8);

		i = fill_sg_comp(gather_comp, i, (uint64_t)hdr, hdr_len);
		i = fill_ipsec_sg_comp_from_pkt(gather_comp, i, m_src);
		((uint16_t *)in_buffer)[2] = rte_cpu_to_be_16(i);

		g_size_bytes = ((i + 3) / 4) * sizeof(struct roc_sglist_comp);

		/*
		 * output Scatter List
		 */
		last_seg->data_len += extend_tail;

		i = 0;
		scatter_comp = (struct roc_sglist_comp *)((uint8_t *)gather_comp + g_size_bytes);

		i = fill_sg_comp(scatter_comp, i, (uint64_t)hdr, hdr_len);
		i = fill_ipsec_sg_comp_from_pkt(scatter_comp, i, m_src);
		((uint16_t *)in_buffer)[3] = rte_cpu_to_be_16(i);

		s_size_bytes = ((i + 3) / 4) * sizeof(struct roc_sglist_comp);

		dlen = g_size_bytes + s_size_bytes + ROC_SG_LIST_HDR_SIZE;

		inst->dptr = (uint64_t)in_buffer;

		inst->w4.u64 = sess->inst.w4 | dlen;
		inst->w4.s.opcode_major |= (uint64_t)ROC_DMA_MODE_SG;
	}

#ifdef LA_IPSEC_DEBUG
	if (sess->inst.w4 & ROC_IE_ON_PER_PKT_IV) {
		memcpy(&hdr->iv[0],
		       rte_crypto_op_ctod_offset(cop, uint8_t *, sess->cipher_iv_off),
		       sess->cipher_iv_len);
	}
#endif

	m_src->pkt_len = pkt_len;
	esn = ++sess->esn;

	/* Set ESN seq hi */
	hdr->esn = rte_cpu_to_be_32(esn >> 32);

	/* Set ESN seq lo */
	seq_lo = rte_cpu_to_be_32(esn & (BIT_ULL(32) - 1));
	hdr->seq = seq_lo;

	/* Set IPID same as seq_lo */
	hdr->ip_id = seq_lo;

	/* Prepare CPT instruction */
	inst->w7.u64 = sess->inst.w7;

	return 0;
}

static __rte_always_inline int
process_inb_sa(struct cpt_qp_meta_info *m_info, struct rte_crypto_op *cop,
	       struct cn9k_sec_session *sess, struct cpt_inst_s *inst,
	       struct cpt_inflight_req *infl_req)
{
	const unsigned int hdr_len = ROC_IE_ON_INB_RPTR_HDR;
	struct rte_crypto_sym_op *sym_op = cop->sym;
	struct rte_mbuf *m_src = sym_op->m_src;
	struct roc_ie_on_inb_hdr *hdr;
	uint32_t dlen;

	infl_req->op_flags |= CPT_OP_FLAGS_IPSEC_DIR_INBOUND;
	if (likely(m_src->next == NULL)) {
		dlen = rte_pktmbuf_pkt_len(m_src);
		inst->dptr = rte_pktmbuf_mtod(m_src, uint64_t);
		inst->w4.u64 = sess->inst.w4 | dlen;
	} else {
		struct roc_sglist_comp *scatter_comp, *gather_comp;
		uint32_t g_size_bytes, s_size_bytes;
		uint8_t *in_buffer;
		void *m_data;
		int i;

		m_data = alloc_op_meta(NULL, m_info->mlen, m_info->pool, infl_req);
		if (unlikely(m_data == NULL)) {
			plt_dp_err("Error allocating meta buffer for request");
			return -ENOMEM;
		}

		hdr = m_data;

		m_data = (uint8_t *)m_data + hdr_len;
		in_buffer = m_data;

		((uint16_t *)in_buffer)[0] = 0;
		((uint16_t *)in_buffer)[1] = 0;

		/*
		 * Input Gather List
		 */
		i = 0;
		gather_comp = (struct roc_sglist_comp *)((uint8_t *)m_data + 8);
		i = fill_ipsec_sg_comp_from_pkt(gather_comp, i, m_src);
		((uint16_t *)in_buffer)[2] = rte_cpu_to_be_16(i);

		g_size_bytes = ((i + 3) / 4) * sizeof(struct roc_sglist_comp);

		/*
		 * Output Scatter List
		 */
		i = 0;
		scatter_comp = (struct roc_sglist_comp *)((uint8_t *)gather_comp + g_size_bytes);
		i = fill_sg_comp(scatter_comp, i, (uint64_t)hdr, hdr_len);
		i = fill_ipsec_sg_comp_from_pkt(scatter_comp, i, m_src);
		((uint16_t *)in_buffer)[3] = rte_cpu_to_be_16(i);

		s_size_bytes = ((i + 3) / 4) * sizeof(struct roc_sglist_comp);

		dlen = g_size_bytes + s_size_bytes + ROC_SG_LIST_HDR_SIZE;

		inst->dptr = (uint64_t)in_buffer;
		inst->w4.u64 = sess->inst.w4 | dlen;
		inst->w4.s.opcode_major |= (uint64_t)ROC_DMA_MODE_SG;
	}

	/* Prepare CPT instruction */
	inst->w7.u64 = sess->inst.w7;

	if (unlikely(sess->replay_win_sz))
		infl_req->op_flags |= CPT_OP_FLAGS_IPSEC_INB_REPLAY;

	return 0;
}
#endif /* __CN9K_IPSEC_LA_OPS_H__ */
