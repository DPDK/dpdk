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
ipsec_po_out_rlen_get(struct cn9k_sec_session *sess, uint32_t plen)
{
	uint32_t enc_payload_len;
	int adj_len = 0;

	if (sess->sa.out_sa.common_sa.ctl.ipsec_mode == ROC_IE_SA_MODE_TRANSPORT)
		adj_len = ROC_CPT_TUNNEL_IPV4_HDR_LEN;

	enc_payload_len =
		RTE_ALIGN_CEIL(plen + sess->rlens.roundup_len - adj_len, sess->rlens.roundup_byte);

	return sess->custom_hdr_len + sess->rlens.partial_len + enc_payload_len + adj_len;
}

static __rte_always_inline int
process_outb_sa(struct rte_crypto_op *cop, struct cn9k_sec_session *sess, struct cpt_inst_s *inst)
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
	rlen = ipsec_po_out_rlen_get(sess, pkt_len);

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
	if (sess->inst.w4 & ROC_IE_ON_PER_PKT_IV) {
		memcpy(&hdr->iv[0],
		       rte_crypto_op_ctod_offset(cop, uint8_t *, sess->cipher_iv_off),
		       sess->cipher_iv_len);
	}
#endif

	esn = ++sess->esn;

	/* Set ESN seq hi */
	hdr->esn = rte_cpu_to_be_32(esn >> 32);

	/* Set ESN seq lo */
	seq_lo = rte_cpu_to_be_32(esn & (BIT_ULL(32) - 1));
	hdr->seq = seq_lo;

	/* Set IPID same as seq_lo */
	hdr->ip_id = seq_lo;

	/* Prepare CPT instruction */
	inst->w4.u64 = sess->inst.w4 | dlen;
	inst->dptr = PLT_U64_CAST(hdr);
	inst->w7.u64 = sess->inst.w7;

	return 0;
}

static __rte_always_inline void
process_inb_sa(struct rte_crypto_op *cop, struct cn9k_sec_session *sess, struct cpt_inst_s *inst)
{
	struct rte_crypto_sym_op *sym_op = cop->sym;
	struct rte_mbuf *m_src = sym_op->m_src;

	/* Prepare CPT instruction */
	inst->w4.u64 = sess->inst.w4 | rte_pktmbuf_pkt_len(m_src);
	inst->dptr = rte_pktmbuf_mtod(m_src, uint64_t);
	inst->w7.u64 = sess->inst.w7;
}
#endif /* __CN9K_IPSEC_LA_OPS_H__ */
