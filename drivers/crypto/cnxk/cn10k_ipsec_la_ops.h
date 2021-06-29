/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef __CN10K_IPSEC_LA_OPS_H__
#define __CN10K_IPSEC_LA_OPS_H__

#include <rte_crypto_sym.h>
#include <rte_security.h>

#include "cn10k_cryptodev.h"
#include "cn10k_ipsec.h"
#include "cnxk_cryptodev.h"

static __rte_always_inline int32_t
ipsec_po_out_rlen_get(struct cn10k_ipsec_sa *sess, uint32_t plen)
{
	uint32_t enc_payload_len;

	enc_payload_len =
		RTE_ALIGN_CEIL(plen + sess->roundup_len, sess->roundup_byte);

	return sess->partial_len + enc_payload_len;
}

static __rte_always_inline int
process_outb_sa(struct rte_crypto_op *cop, struct cn10k_ipsec_sa *sess,
		struct cpt_inst_s *inst)
{
	struct rte_crypto_sym_op *sym_op = cop->sym;
	struct rte_mbuf *m_src = sym_op->m_src;
	uint32_t dlen, rlen, extend_tail;
	char *mdata;

	dlen = rte_pktmbuf_pkt_len(m_src);
	rlen = ipsec_po_out_rlen_get(sess, dlen);

	extend_tail = rlen - dlen;

	mdata = rte_pktmbuf_append(m_src, extend_tail);
	if (unlikely(mdata == NULL)) {
		plt_dp_err("Not enough tail room");
		return -ENOMEM;
	}

	/* Prepare CPT instruction */
	inst->w4.u64 = sess->inst.w4;
	inst->w4.s.dlen = dlen;
	inst->dptr = rte_pktmbuf_iova(m_src);
	inst->rptr = inst->dptr;

	return 0;
}

static __rte_always_inline int
process_inb_sa(struct rte_crypto_op *cop, struct cn10k_ipsec_sa *sa,
	       struct cpt_inst_s *inst)
{
	struct rte_crypto_sym_op *sym_op = cop->sym;
	struct rte_mbuf *m_src = sym_op->m_src;
	uint32_t dlen;

	dlen = rte_pktmbuf_pkt_len(m_src);

	/* Prepare CPT instruction */
	inst->w4.u64 = sa->inst.w4;
	inst->w4.s.dlen = dlen;
	inst->dptr = rte_pktmbuf_iova(m_src);
	inst->rptr = inst->dptr;

	return 0;
}

#endif /* __CN10K_IPSEC_LA_OPS_H__ */
