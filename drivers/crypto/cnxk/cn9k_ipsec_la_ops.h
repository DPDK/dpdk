/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef __CN9K_IPSEC_LA_OPS_H__
#define __CN9K_IPSEC_LA_OPS_H__

#include <rte_crypto_sym.h>
#include <rte_security.h>

#include "cn9k_ipsec.h"

static __rte_always_inline int32_t
ipsec_po_out_rlen_get(struct cn9k_ipsec_sa *sa, uint32_t plen)
{
	uint32_t enc_payload_len;

	enc_payload_len = RTE_ALIGN_CEIL(plen + sa->rlens.roundup_len,
					 sa->rlens.roundup_byte);

	return sa->rlens.partial_len + enc_payload_len;
}

static __rte_always_inline int
process_outb_sa(struct rte_crypto_op *cop, struct cn9k_ipsec_sa *sa,
		struct cpt_inst_s *inst)
{
	const unsigned int hdr_len = sizeof(struct roc_ie_on_outb_hdr);
	struct rte_crypto_sym_op *sym_op = cop->sym;
	struct rte_mbuf *m_src = sym_op->m_src;
	uint32_t dlen, rlen, extend_tail;
	struct roc_ie_on_outb_sa *out_sa;
	struct roc_ie_on_outb_hdr *hdr;

	out_sa = &sa->out_sa;

	dlen = rte_pktmbuf_pkt_len(m_src) + hdr_len;
	rlen = ipsec_po_out_rlen_get(sa, dlen - hdr_len);

	extend_tail = rlen - dlen;
	if (unlikely(extend_tail > rte_pktmbuf_tailroom(m_src))) {
		plt_dp_err("Not enough tail room");
		return -ENOMEM;
	}

	m_src->data_len += extend_tail;
	m_src->pkt_len += extend_tail;

	hdr = (struct roc_ie_on_outb_hdr *)rte_pktmbuf_prepend(m_src, hdr_len);
	if (unlikely(hdr == NULL)) {
		plt_dp_err("Not enough head room");
		return -ENOMEM;
	}

	memcpy(&hdr->iv[0],
	       rte_crypto_op_ctod_offset(cop, uint8_t *, sa->cipher_iv_off),
	       sa->cipher_iv_len);
	hdr->seq = rte_cpu_to_be_32(sa->seq_lo);
	hdr->ip_id = rte_cpu_to_be_32(sa->ip_id);

	out_sa->common_sa.esn_hi = sa->seq_hi;

	sa->ip_id++;
	sa->esn++;

	/* Prepare CPT instruction */
	inst->w4.u64 = sa->inst.w4 | dlen;
	inst->dptr = rte_pktmbuf_iova(m_src);
	inst->rptr = inst->dptr;
	inst->w7.u64 = sa->inst.w7;

	return 0;
}

static __rte_always_inline int
process_inb_sa(struct rte_crypto_op *cop, struct cn9k_ipsec_sa *sa,
	       struct cpt_inst_s *inst)
{
	struct rte_crypto_sym_op *sym_op = cop->sym;
	struct rte_mbuf *m_src = sym_op->m_src;

	/* Prepare CPT instruction */
	inst->w4.u64 = sa->inst.w4 | rte_pktmbuf_pkt_len(m_src);
	inst->dptr = rte_pktmbuf_iova(m_src);
	inst->rptr = inst->dptr;
	inst->w7.u64 = sa->inst.w7;

	return 0;
}
#endif /* __CN9K_IPSEC_LA_OPS_H__ */
