/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#ifndef __CN10K_TLS_OPS_H__
#define __CN10K_TLS_OPS_H__

#include <rte_crypto_sym.h>
#include <rte_security.h>

#include "roc_ie.h"

#include "cn10k_cryptodev.h"
#include "cn10k_cryptodev_sec.h"
#include "cnxk_cryptodev.h"
#include "cnxk_cryptodev_ops.h"
#include "cnxk_sg.h"

static __rte_always_inline int
process_tls_write(struct roc_cpt_lf *lf, struct rte_crypto_op *cop, struct cn10k_sec_session *sess,
		  struct cpt_qp_meta_info *m_info, struct cpt_inflight_req *infl_req,
		  struct cpt_inst_s *inst, const bool is_sg_ver2)
{
	struct cn10k_tls_opt tls_opt = sess->tls_opt;
	struct rte_crypto_sym_op *sym_op = cop->sym;
#ifdef LA_IPSEC_DEBUG
	struct roc_ie_ot_tls_write_sa *write_sa;
#endif
	struct rte_mbuf *m_src = sym_op->m_src;
	struct rte_mbuf *m_dst = sym_op->m_dst;
	uint32_t pad_len, pad_bytes;
	struct rte_mbuf *last_seg;
	union cpt_inst_w4 w4;
	void *m_data = NULL;
	uint8_t *in_buffer;

	pad_bytes = (cop->aux_flags * 8) > 0xff ? 0xff : (cop->aux_flags * 8);
	pad_len = (pad_bytes >> tls_opt.pad_shift) * tls_opt.enable_padding;

#ifdef LA_IPSEC_DEBUG
	write_sa = &sess->tls_rec.write_sa;
	if (write_sa->w2.s.iv_at_cptr == ROC_IE_OT_TLS_IV_SRC_FROM_SA) {

		uint8_t *iv = PLT_PTR_ADD(write_sa->cipher_key, 32);

		if (write_sa->w2.s.cipher_select == ROC_IE_OT_TLS_CIPHER_AES_GCM) {
			uint32_t *tmp;

			/* For GCM, the IV and salt format will be like below:
			 * iv[0-3]: lower bytes of IV in BE format.
			 * iv[4-7]: salt / nonce.
			 * iv[12-15]: upper bytes of IV in BE format.
			 */
			memcpy(iv, rte_crypto_op_ctod_offset(cop, uint8_t *, sess->iv_offset), 4);
			tmp = (uint32_t *)iv;
			*tmp = rte_be_to_cpu_32(*tmp);

			memcpy(iv + 12,
			       rte_crypto_op_ctod_offset(cop, uint8_t *, sess->iv_offset + 4), 4);
			tmp = (uint32_t *)(iv + 12);
			*tmp = rte_be_to_cpu_32(*tmp);
		} else if (write_sa->w2.s.cipher_select == ROC_IE_OT_TLS_CIPHER_AES_CBC) {
			uint64_t *tmp;

			memcpy(iv, rte_crypto_op_ctod_offset(cop, uint8_t *, sess->iv_offset), 16);
			tmp = (uint64_t *)iv;
			*tmp = rte_be_to_cpu_64(*tmp);
			tmp = (uint64_t *)(iv + 8);
			*tmp = rte_be_to_cpu_64(*tmp);
		} else if (write_sa->w2.s.cipher_select == ROC_IE_OT_TLS_CIPHER_3DES) {
			uint64_t *tmp;

			memcpy(iv, rte_crypto_op_ctod_offset(cop, uint8_t *, sess->iv_offset), 8);
			tmp = (uint64_t *)iv;
			*tmp = rte_be_to_cpu_64(*tmp);
		}

		/* Trigger CTX reload to fetch new data from DRAM */
		roc_cpt_lf_ctx_reload(lf, write_sa);
		rte_delay_ms(1);
	}
#else
	RTE_SET_USED(lf);
#endif
	/* Single buffer direct mode */
	if (likely(m_src->next == NULL)) {
		void *vaddr;

		if (unlikely(rte_pktmbuf_tailroom(m_src) < sess->max_extended_len)) {
			plt_dp_err("Not enough tail room");
			return -ENOMEM;
		}

		vaddr = rte_pktmbuf_mtod(m_src, void *);
		inst->dptr = (uint64_t)vaddr;
		inst->rptr = (uint64_t)vaddr;

		w4.u64 = sess->inst.w4;
		w4.s.param1 = m_src->data_len;
		w4.s.dlen = m_src->data_len;

		w4.s.param2 = cop->param1.tls_record.content_type;
		w4.s.opcode_minor = pad_len;

		inst->w4.u64 = w4.u64;
	} else if (is_sg_ver2 == false) {
		struct roc_sglist_comp *scatter_comp, *gather_comp;
		uint32_t g_size_bytes, s_size_bytes;
		uint32_t dlen;
		int i;

		last_seg = rte_pktmbuf_lastseg(m_src);

		if (unlikely(rte_pktmbuf_tailroom(last_seg) < sess->max_extended_len)) {
			plt_dp_err("Not enough tail room (required: %d, available: %d)",
				   sess->max_extended_len, rte_pktmbuf_tailroom(last_seg));
			return -ENOMEM;
		}

		if (unlikely(m_src->nb_segs > ROC_SG1_MAX_PTRS)) {
			plt_dp_err("Exceeds max supported components. Reduce segments");
			return -1;
		}

		m_data = alloc_op_meta(NULL, m_info->mlen, m_info->pool, infl_req);
		if (unlikely(m_data == NULL)) {
			plt_dp_err("Error allocating meta buffer for request");
			return -ENOMEM;
		}

		in_buffer = (uint8_t *)m_data;
		((uint16_t *)in_buffer)[0] = 0;
		((uint16_t *)in_buffer)[1] = 0;

		/* Input Gather List */
		i = 0;
		gather_comp = (struct roc_sglist_comp *)((uint8_t *)in_buffer + 8);

		i = fill_sg_comp_from_pkt(gather_comp, i, m_src);
		((uint16_t *)in_buffer)[2] = rte_cpu_to_be_16(i);

		g_size_bytes = ((i + 3) / 4) * sizeof(struct roc_sglist_comp);

		/* Output Scatter List */
		last_seg->data_len += sess->max_extended_len + pad_bytes;
		i = 0;
		scatter_comp = (struct roc_sglist_comp *)((uint8_t *)gather_comp + g_size_bytes);

		i = fill_sg_comp_from_pkt(scatter_comp, i, m_src);
		((uint16_t *)in_buffer)[3] = rte_cpu_to_be_16(i);

		s_size_bytes = ((i + 3) / 4) * sizeof(struct roc_sglist_comp);

		dlen = g_size_bytes + s_size_bytes + ROC_SG_LIST_HDR_SIZE;

		inst->dptr = (uint64_t)in_buffer;
		inst->rptr = (uint64_t)in_buffer;

		w4.u64 = sess->inst.w4;
		w4.s.dlen = dlen;
		w4.s.param1 = rte_pktmbuf_pkt_len(m_src);
		w4.s.param2 = cop->param1.tls_record.content_type;
		w4.s.opcode_major |= (uint64_t)ROC_DMA_MODE_SG;
		w4.s.opcode_minor = pad_len;

		inst->w4.u64 = w4.u64;
	} else {
		struct roc_sg2list_comp *scatter_comp, *gather_comp;
		union cpt_inst_w5 cpt_inst_w5;
		union cpt_inst_w6 cpt_inst_w6;
		uint32_t g_size_bytes;
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

		in_buffer = (uint8_t *)m_data;
		/* Input Gather List */
		i = 0;
		gather_comp = (struct roc_sg2list_comp *)((uint8_t *)in_buffer);
		i = fill_sg2_comp_from_pkt(gather_comp, i, m_src);

		cpt_inst_w5.s.gather_sz = ((i + 2) / 3);
		g_size_bytes = ((i + 2) / 3) * sizeof(struct roc_sg2list_comp);

		/* Output Scatter List */
		last_seg->data_len += sess->max_extended_len + pad_bytes;
		i = 0;
		scatter_comp = (struct roc_sg2list_comp *)((uint8_t *)gather_comp + g_size_bytes);

		if (m_dst == NULL)
			m_dst = m_src;
		i = fill_sg2_comp_from_pkt(scatter_comp, i, m_dst);

		cpt_inst_w6.s.scatter_sz = ((i + 2) / 3);

		cpt_inst_w5.s.dptr = (uint64_t)gather_comp;
		cpt_inst_w6.s.rptr = (uint64_t)scatter_comp;

		inst->w5.u64 = cpt_inst_w5.u64;
		inst->w6.u64 = cpt_inst_w6.u64;
		w4.u64 = sess->inst.w4;
		w4.s.dlen = rte_pktmbuf_pkt_len(m_src);
		w4.s.opcode_major &= (~(ROC_IE_OT_INPLACE_BIT));
		w4.s.opcode_minor = pad_len;
		w4.s.param1 = w4.s.dlen;
		w4.s.param2 = cop->param1.tls_record.content_type;
		inst->w4.u64 = w4.u64;
	}

	return 0;
}

static __rte_always_inline int
process_tls_read(struct rte_crypto_op *cop, struct cn10k_sec_session *sess,
		 struct cpt_qp_meta_info *m_info, struct cpt_inflight_req *infl_req,
		 struct cpt_inst_s *inst, const bool is_sg_ver2)
{
	struct rte_crypto_sym_op *sym_op = cop->sym;
	struct rte_mbuf *m_src = sym_op->m_src;
	struct rte_mbuf *m_dst = sym_op->m_dst;
	union cpt_inst_w4 w4;
	uint8_t *in_buffer;
	void *m_data;

	if (likely(m_src->next == NULL)) {
		void *vaddr;

		vaddr = rte_pktmbuf_mtod(m_src, void *);

		inst->dptr = (uint64_t)vaddr;
		inst->rptr = (uint64_t)vaddr;

		w4.u64 = sess->inst.w4;
		w4.s.dlen = m_src->data_len;
		w4.s.param1 = m_src->data_len;
		inst->w4.u64 = w4.u64;
	} else if (is_sg_ver2 == false) {
		struct roc_sglist_comp *scatter_comp, *gather_comp;
		int tail_len = sess->tls_opt.tail_fetch_len * 16;
		int pkt_len = rte_pktmbuf_pkt_len(m_src);
		uint32_t g_size_bytes, s_size_bytes;
		uint16_t *sg_hdr;
		uint32_t dlen;
		int i;

		if (unlikely(m_src->nb_segs > ROC_SG1_MAX_PTRS)) {
			plt_dp_err("Exceeds max supported components. Reduce segments");
			return -1;
		}

		m_data = alloc_op_meta(NULL, m_info->mlen, m_info->pool, infl_req);
		if (unlikely(m_data == NULL)) {
			plt_dp_err("Error allocating meta buffer for request");
			return -ENOMEM;
		}

		/* Input Gather List */
		in_buffer = (uint8_t *)m_data;
		sg_hdr = (uint16_t *)(in_buffer + 32);
		gather_comp = (struct roc_sglist_comp *)((uint8_t *)sg_hdr + 8);
		i = 0;
		/* Add the last blocks as first gather component for tail fetch. */
		if (tail_len) {
			const uint8_t *output;

			output = rte_pktmbuf_read(m_src, pkt_len - tail_len, tail_len, in_buffer);
			if (output != in_buffer)
				rte_memcpy(in_buffer, output, tail_len);
			i = fill_sg_comp(gather_comp, i, (uint64_t)in_buffer, tail_len);
		}

		sg_hdr[0] = 0;
		sg_hdr[1] = 0;
		i = fill_sg_comp_from_pkt(gather_comp, i, m_src);
		sg_hdr[2] = rte_cpu_to_be_16(i);

		g_size_bytes = ((i + 3) / 4) * sizeof(struct roc_sglist_comp);

		i = 0;
		scatter_comp = (struct roc_sglist_comp *)((uint8_t *)gather_comp + g_size_bytes);

		i = fill_sg_comp_from_pkt(scatter_comp, i, m_src);
		sg_hdr[3] = rte_cpu_to_be_16(i);

		s_size_bytes = ((i + 3) / 4) * sizeof(struct roc_sglist_comp);

		dlen = g_size_bytes + s_size_bytes + ROC_SG_LIST_HDR_SIZE;

		inst->dptr = (uint64_t)in_buffer;
		inst->rptr = (uint64_t)in_buffer;

		w4.u64 = sess->inst.w4;
		w4.s.dlen = dlen;
		w4.s.opcode_major |= (uint64_t)ROC_DMA_MODE_SG;
		w4.s.param1 = pkt_len;
		inst->w4.u64 = w4.u64;
	} else {
		struct roc_sg2list_comp *scatter_comp, *gather_comp;
		int tail_len = sess->tls_opt.tail_fetch_len * 16;
		int pkt_len = rte_pktmbuf_pkt_len(m_src);
		union cpt_inst_w5 cpt_inst_w5;
		union cpt_inst_w6 cpt_inst_w6;
		uint32_t g_size_bytes;
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

		in_buffer = (uint8_t *)m_data;
		/* Input Gather List */
		i = 0;

		/* First 32 bytes in m_data are rsvd for tail fetch.
		 * SG list start from 32 byte onwards.
		 */
		gather_comp = (struct roc_sg2list_comp *)((uint8_t *)(in_buffer + 32));

		/* Add the last blocks as first gather component for tail fetch. */
		if (tail_len) {
			const uint8_t *output;

			output = rte_pktmbuf_read(m_src, pkt_len - tail_len, tail_len, in_buffer);
			if (output != in_buffer)
				rte_memcpy(in_buffer, output, tail_len);
			i = fill_sg2_comp(gather_comp, i, (uint64_t)in_buffer, tail_len);
		}

		i = fill_sg2_comp_from_pkt(gather_comp, i, m_src);

		cpt_inst_w5.s.gather_sz = ((i + 2) / 3);
		g_size_bytes = ((i + 2) / 3) * sizeof(struct roc_sg2list_comp);

		i = 0;
		scatter_comp = (struct roc_sg2list_comp *)((uint8_t *)gather_comp + g_size_bytes);

		if (m_dst == NULL)
			m_dst = m_src;
		i = fill_sg2_comp_from_pkt(scatter_comp, i, m_dst);

		cpt_inst_w6.s.scatter_sz = ((i + 2) / 3);

		cpt_inst_w5.s.dptr = (uint64_t)gather_comp;
		cpt_inst_w6.s.rptr = (uint64_t)scatter_comp;

		inst->w5.u64 = cpt_inst_w5.u64;
		inst->w6.u64 = cpt_inst_w6.u64;
		w4.u64 = sess->inst.w4;
		w4.s.dlen = pkt_len + tail_len;
		w4.s.param1 = w4.s.dlen;
		w4.s.opcode_major &= (~(ROC_IE_OT_INPLACE_BIT));
		inst->w4.u64 = w4.u64;
	}

	return 0;
}
#endif /* __CN10K_TLS_OPS_H__ */
