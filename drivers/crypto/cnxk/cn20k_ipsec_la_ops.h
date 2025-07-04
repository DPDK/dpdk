/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2025 Marvell.
 */

#ifndef __CN20K_IPSEC_LA_OPS_H__
#define __CN20K_IPSEC_LA_OPS_H__

#include <rte_crypto_sym.h>
#include <rte_security.h>

#include "roc_ie.h"

#include "cn20k_cryptodev.h"
#include "cn20k_ipsec.h"
#include "cnxk_cryptodev.h"
#include "cnxk_cryptodev_ops.h"
#include "cnxk_sg.h"

static inline void
ipsec_po_sa_iv_set(struct cn20k_sec_session *sess, struct rte_crypto_op *cop)
{
	uint64_t *iv = &sess->sa.out_sa.iv.u64[0];
	uint64_t *tmp_iv;

	memcpy(iv, rte_crypto_op_ctod_offset(cop, uint8_t *, sess->iv_offset), 16);
	tmp_iv = (uint64_t *)iv;
	*tmp_iv = rte_be_to_cpu_64(*tmp_iv);

	tmp_iv = (uint64_t *)(iv + 1);
	*tmp_iv = rte_be_to_cpu_64(*tmp_iv);
}

static inline void
ipsec_po_sa_aes_8b_iv_set(struct cn20k_sec_session *sess, struct rte_crypto_op *cop)
{
	uint8_t *iv = &sess->sa.out_sa.iv.s.iv_dbg1[0];
	uint32_t *tmp_iv;

	memcpy(iv, rte_crypto_op_ctod_offset(cop, uint8_t *, sess->iv_offset), 4);
	tmp_iv = (uint32_t *)iv;
	*tmp_iv = rte_be_to_cpu_32(*tmp_iv);

	iv = &sess->sa.out_sa.iv.s.iv_dbg2[0];
	memcpy(iv, rte_crypto_op_ctod_offset(cop, uint8_t *, sess->iv_offset + 4), 4);
	tmp_iv = (uint32_t *)iv;
	*tmp_iv = rte_be_to_cpu_32(*tmp_iv);
}

static __rte_always_inline int
process_outb_sa(struct roc_cpt_lf *lf, struct rte_crypto_op *cop, struct cn20k_sec_session *sess,
		struct cpt_qp_meta_info *m_info, struct cpt_inflight_req *infl_req,
		struct cpt_inst_s *inst)
{
	struct rte_crypto_sym_op *sym_op = cop->sym;
	struct rte_mbuf *m_src = sym_op->m_src;
	uint64_t inst_w4_u64 = sess->inst.w4;
	uint64_t dptr;

	RTE_SET_USED(lf);

#ifdef LA_IPSEC_DEBUG
	if (sess->sa.out_sa.w2.s.iv_src == ROC_IE_OW_SA_IV_SRC_FROM_SA) {
		if (sess->sa.out_sa.w2.s.enc_type == ROC_IE_SA_ENC_AES_GCM ||
		    sess->sa.out_sa.w2.s.enc_type == ROC_IE_SA_ENC_AES_CCM ||
		    sess->sa.out_sa.w2.s.auth_type == ROC_IE_SA_AUTH_AES_GMAC ||
		    sess->sa.out_sa.w2.s.enc_type == ROC_IE_SA_ENC_AES_CTR)
			ipsec_po_sa_aes_8b_iv_set(sess, cop);
		else
			ipsec_po_sa_iv_set(sess, cop);
	}

	/* Trigger CTX reload to fetch new data from DRAM */
	roc_cpt_lf_ctx_reload(lf, &sess->sa.out_sa);
	rte_delay_ms(1);
#endif
	const uint64_t ol_flags = m_src->ol_flags;

	inst_w4_u64 &= ~(((uint64_t)(!!(ol_flags & RTE_MBUF_F_TX_IP_CKSUM)) << 33) |
			 ((uint64_t)(!!(ol_flags & RTE_MBUF_F_TX_L4_MASK)) << 32));

	if (likely(m_src->next == NULL)) {
		if (unlikely(rte_pktmbuf_tailroom(m_src) < sess->max_extended_len)) {
			plt_dp_err("Not enough tail room");
			return -ENOMEM;
		}

		/* Prepare CPT instruction */
		inst->w4.u64 = inst_w4_u64 | rte_pktmbuf_pkt_len(m_src);
		dptr = rte_pktmbuf_mtod(m_src, uint64_t);
		inst->dptr = dptr;
	} else {
		struct roc_sg2list_comp *scatter_comp, *gather_comp;
		union cpt_inst_w5 cpt_inst_w5;
		union cpt_inst_w6 cpt_inst_w6;
		struct rte_mbuf *last_seg;
		uint32_t g_size_bytes;
		void *m_data;
		int i;

		last_seg = rte_pktmbuf_lastseg(m_src);

		if (unlikely(rte_pktmbuf_tailroom(last_seg) < sess->max_extended_len)) {
			plt_dp_err("Not enough tail room (required: %d, available: %d)",
				   sess->max_extended_len, rte_pktmbuf_tailroom(last_seg));
			return -ENOMEM;
		}

		if (unlikely(m_src->nb_segs > ROC_SG2_MAX_PTRS)) {
			plt_dp_err("Exceeds max supported components. Reduce segments");
			return -1;
		}

		m_data = alloc_op_meta(NULL, m_info->mlen, m_info->pool, infl_req);
		if (unlikely(m_data == NULL)) {
			plt_dp_err("Error allocating meta buffer for request");
			return -ENOMEM;
		}

		/* Input Gather List */
		i = 0;
		gather_comp = (struct roc_sg2list_comp *)((uint8_t *)m_data);

		i = fill_sg2_comp_from_pkt(gather_comp, i, m_src);

		cpt_inst_w5.s.gather_sz = ((i + 2) / 3);
		g_size_bytes = ((i + 2) / 3) * sizeof(struct roc_sg2list_comp);

		/* Output Scatter List */
		last_seg->data_len += sess->max_extended_len;

		i = 0;
		scatter_comp = (struct roc_sg2list_comp *)((uint8_t *)gather_comp + g_size_bytes);

		i = fill_sg2_comp_from_pkt(scatter_comp, i, m_src);

		cpt_inst_w6.s.scatter_sz = ((i + 2) / 3);

		cpt_inst_w5.s.dptr = (uint64_t)gather_comp;
		cpt_inst_w6.s.rptr = (uint64_t)scatter_comp;

		inst->w5.u64 = cpt_inst_w5.u64;
		inst->w6.u64 = cpt_inst_w6.u64;
		inst->w4.u64 = sess->inst.w4 | rte_pktmbuf_pkt_len(m_src);
		inst->w4.s.opcode_major &= (~(ROC_IE_OW_INPLACE_BIT));
	}

	return 0;
}

static __rte_always_inline int
process_inb_sa(struct rte_crypto_op *cop, struct cn20k_sec_session *sess, struct cpt_inst_s *inst,
	       struct cpt_qp_meta_info *m_info, struct cpt_inflight_req *infl_req)
{
	struct rte_crypto_sym_op *sym_op = cop->sym;
	struct rte_mbuf *m_src = sym_op->m_src;
	uint64_t dptr;

	if (likely(m_src->next == NULL)) {
		/* Prepare CPT instruction */
		inst->w4.u64 = sess->inst.w4 | rte_pktmbuf_pkt_len(m_src);
		dptr = rte_pktmbuf_mtod(m_src, uint64_t);
		inst->dptr = dptr;
		m_src->ol_flags |= (uint64_t)sess->ipsec.ip_csum;
	} else {
		struct roc_sg2list_comp *scatter_comp, *gather_comp;
		union cpt_inst_w5 cpt_inst_w5;
		union cpt_inst_w6 cpt_inst_w6;
		uint32_t g_size_bytes;
		void *m_data;
		int i;

		if (unlikely(m_src->nb_segs > ROC_SG2_MAX_PTRS)) {
			plt_dp_err("Exceeds max supported components. Reduce segments");
			return -1;
		}

		m_data = alloc_op_meta(NULL, m_info->mlen, m_info->pool, infl_req);
		if (unlikely(m_data == NULL)) {
			plt_dp_err("Error allocating meta buffer for request");
			return -ENOMEM;
		}

		/* Input Gather List */
		i = 0;
		gather_comp = (struct roc_sg2list_comp *)((uint8_t *)m_data);

		i = fill_sg2_comp_from_pkt(gather_comp, i, m_src);

		cpt_inst_w5.s.gather_sz = ((i + 2) / 3);
		g_size_bytes = ((i + 2) / 3) * sizeof(struct roc_sg2list_comp);

		/* Output Scatter List */
		i = 0;
		scatter_comp = (struct roc_sg2list_comp *)((uint8_t *)gather_comp + g_size_bytes);
		i = fill_sg2_comp_from_pkt(scatter_comp, i, m_src);

		cpt_inst_w6.s.scatter_sz = ((i + 2) / 3);

		cpt_inst_w5.s.dptr = (uint64_t)gather_comp;
		cpt_inst_w6.s.rptr = (uint64_t)scatter_comp;

		inst->w5.u64 = cpt_inst_w5.u64;
		inst->w6.u64 = cpt_inst_w6.u64;
		inst->w4.u64 = sess->inst.w4 | rte_pktmbuf_pkt_len(m_src);
		inst->w4.s.opcode_major &= (~(ROC_IE_OW_INPLACE_BIT));
	}
	return 0;
}

#endif /* __CN20K_IPSEC_LA_OPS_H__ */
