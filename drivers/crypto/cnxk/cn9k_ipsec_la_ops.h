/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef __CN9K_IPSEC_LA_OPS_H__
#define __CN9K_IPSEC_LA_OPS_H__

#include <rte_crypto_sym.h>
#include <rte_esp.h>
#include <rte_security.h>

#include "cn9k_ipsec.h"
#include "cnxk_security_ar.h"

static __rte_always_inline int32_t
ipsec_po_out_rlen_get(struct cn9k_ipsec_sa *sa, uint32_t plen)
{
	uint32_t enc_payload_len;

	enc_payload_len = RTE_ALIGN_CEIL(plen + sa->rlens.roundup_len,
					 sa->rlens.roundup_byte);

	return sa->custom_hdr_len + sa->rlens.partial_len + enc_payload_len;
}

static __rte_always_inline int
ipsec_antireplay_check(struct cn9k_ipsec_sa *sa, uint32_t win_sz,
		       struct rte_mbuf *m)
{
	uint32_t esn_low = 0, esn_hi = 0, seql = 0, seqh = 0;
	struct roc_ie_on_common_sa *common_sa;
	struct roc_ie_on_inb_sa *in_sa;
	struct roc_ie_on_sa_ctl *ctl;
	uint64_t seq_in_sa, seq = 0;
	struct rte_esp_hdr *esp;
	uint8_t esn;
	int ret;

	in_sa = &sa->in_sa;
	common_sa = &in_sa->common_sa;
	ctl = &common_sa->ctl;

	esn = ctl->esn_en;
	esn_low = rte_be_to_cpu_32(common_sa->seq_t.tl);
	esn_hi = rte_be_to_cpu_32(common_sa->seq_t.th);

	esp = rte_pktmbuf_mtod_offset(m, void *, sizeof(struct rte_ipv4_hdr));
	seql = rte_be_to_cpu_32(esp->seq);

	if (!esn) {
		seq = (uint64_t)seql;
	} else {
		seqh = cnxk_on_anti_replay_get_seqh(win_sz, seql, esn_hi,
						    esn_low);
		seq = ((uint64_t)seqh << 32) | seql;
	}

	if (unlikely(seq == 0))
		return IPSEC_ANTI_REPLAY_FAILED;

	ret = cnxk_on_anti_replay_check(seq, &sa->ar, win_sz);
	if (esn && !ret) {
		seq_in_sa = ((uint64_t)esn_hi << 32) | esn_low;
		if (seq > seq_in_sa) {
			common_sa->seq_t.tl = rte_cpu_to_be_32(seql);
			common_sa->seq_t.th = rte_cpu_to_be_32(seqh);
		}
	}

	return ret;
}

static __rte_always_inline int
process_outb_sa(struct rte_crypto_op *cop, struct cn9k_ipsec_sa *sa,
		struct cpt_inst_s *inst)
{
	const unsigned int hdr_len = sa->custom_hdr_len;
	struct rte_crypto_sym_op *sym_op = cop->sym;
	struct rte_mbuf *m_src = sym_op->m_src;
	uint32_t dlen, rlen, pkt_len, seq_lo;
	uint16_t data_off = m_src->data_off;
	struct roc_ie_on_outb_hdr *hdr;
	int32_t extend_tail;
	uint64_t esn;

	pkt_len = rte_pktmbuf_pkt_len(m_src);
	dlen = pkt_len + hdr_len;
	rlen = ipsec_po_out_rlen_get(sa, pkt_len);

	extend_tail = rlen - dlen;
	if (unlikely(extend_tail > rte_pktmbuf_tailroom(m_src))) {
		plt_dp_err("Not enough tail room (required: %d, available: %d)",
			   extend_tail, rte_pktmbuf_tailroom(m_src));
		return -ENOMEM;
	}

	if (unlikely(hdr_len > data_off)) {
		plt_dp_err("Not enough head room (required: %d, available: %d)",
			   hdr_len, rte_pktmbuf_headroom(m_src));
		return -ENOMEM;
	}

	pkt_len += extend_tail;

	m_src->data_len = pkt_len;
	m_src->pkt_len = pkt_len;

	hdr = PLT_PTR_ADD(m_src->buf_addr, data_off - hdr_len);

#ifdef LA_IPSEC_DEBUG
	if (sa->inst.w4 & ROC_IE_ON_PER_PKT_IV) {
		memcpy(&hdr->iv[0],
		       rte_crypto_op_ctod_offset(cop, uint8_t *,
						 sa->cipher_iv_off),
		       sa->cipher_iv_len);
	}
#endif

	esn = ++sa->esn;

	/* Set ESN seq hi */
	hdr->esn = rte_cpu_to_be_32(esn >> 32);

	/* Set ESN seq lo */
	seq_lo = rte_cpu_to_be_32(esn & (BIT_ULL(32) - 1));
	hdr->seq = seq_lo;

	/* Set IPID same as seq_lo */
	hdr->ip_id = seq_lo;

	/* Prepare CPT instruction */
	inst->w4.u64 = sa->inst.w4 | dlen;
	inst->dptr = PLT_U64_CAST(hdr);
	inst->rptr = PLT_U64_CAST(hdr);
	inst->w7.u64 = sa->inst.w7;

	return 0;
}

static __rte_always_inline void
process_inb_sa(struct rte_crypto_op *cop, struct cn9k_ipsec_sa *sa,
	       struct cpt_inst_s *inst)
{
	struct rte_crypto_sym_op *sym_op = cop->sym;
	struct rte_mbuf *m_src = sym_op->m_src;
	int ret;

	if (sa->replay_win_sz) {
		ret = ipsec_antireplay_check(sa, sa->replay_win_sz, m_src);
		if (unlikely(ret)) {
			/* Use PASSTHROUGH op for failed antireplay packet */
			inst->w4.u64 = 0;
			inst->w4.s.opcode_major = ROC_SE_MAJOR_OP_MISC;
			inst->w4.s.opcode_minor =
				ROC_SE_MISC_MINOR_OP_PASSTHROUGH;
			inst->w4.s.param1 = 1;
			/* Send out completion code only */
			inst->w4.s.param2 =
				(ROC_IE_ON_SWCC_ANTI_REPLAY << 8) | 0x1;
			inst->w4.s.dlen = 1;
			inst->dptr = rte_pktmbuf_iova(m_src);
			inst->rptr = inst->dptr;
			inst->w7.u64 = sa->inst.w7;
			return;
		}
	}

	/* Prepare CPT instruction */
	inst->w4.u64 = sa->inst.w4 | rte_pktmbuf_pkt_len(m_src);
	inst->dptr = inst->rptr = rte_pktmbuf_iova(m_src);
	inst->w7.u64 = sa->inst.w7;
}
#endif /* __CN9K_IPSEC_LA_OPS_H__ */
