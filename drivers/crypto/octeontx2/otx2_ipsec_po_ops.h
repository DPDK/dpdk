
/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef __OTX2_IPSEC_PO_OPS_H__
#define __OTX2_IPSEC_PO_OPS_H__

#include <rte_crypto_sym.h>
#include <rte_security.h>

#include "otx2_cryptodev.h"
#include "otx2_security.h"

static __rte_always_inline int32_t
otx2_ipsec_po_out_rlen_get(struct otx2_sec_session_ipsec_lp *sess,
			   uint32_t plen)
{
	uint32_t enc_payload_len;

	enc_payload_len = RTE_ALIGN_CEIL(plen + sess->roundup_len,
			sess->roundup_byte);

	return sess->partial_len + enc_payload_len;
}

static __rte_always_inline struct cpt_request_info *
alloc_request_struct(char *maddr, void *cop, int mdata_len)
{
	struct cpt_request_info *req;
	struct cpt_meta_info *meta;
	uint8_t *resp_addr;
	uintptr_t *op;

	meta = (void *)RTE_PTR_ALIGN((uint8_t *)maddr, 16);

	op = (uintptr_t *)meta->deq_op_info;
	req = &meta->cpt_req;
	resp_addr = (uint8_t *)&meta->cpt_res;

	req->completion_addr = (uint64_t *)((uint8_t *)resp_addr);
	*req->completion_addr = COMPLETION_CODE_INIT;
	req->comp_baddr = rte_mem_virt2iova(resp_addr);
	req->op = op;

	op[0] = (uintptr_t)((uint64_t)meta | 1ull);
	op[1] = (uintptr_t)cop;
	op[2] = (uintptr_t)req;
	op[3] = mdata_len;

	return req;
}

static __rte_always_inline int
process_outb_sa(struct rte_crypto_op *cop,
	       struct otx2_sec_session_ipsec_lp *sess,
	       struct cpt_qp_meta_info *m_info, void **prep_req)
{
	uint32_t dlen, rlen, extend_head, extend_tail;
	struct rte_crypto_sym_op *sym_op = cop->sym;
	struct rte_mbuf *m_src = sym_op->m_src;
	struct otx2_ipsec_po_sa_ctl *ctl_wrd;
	struct cpt_request_info *req = NULL;
	struct otx2_ipsec_po_out_hdr *hdr;
	struct otx2_ipsec_po_out_sa *sa;
	int hdr_len, mdata_len, ret = 0;
	vq_cmd_word0_t word0;
	char *mdata, *data;

	sa = &sess->out_sa;
	ctl_wrd = &sa->ctl;
	hdr_len = sizeof(*hdr);

	dlen = rte_pktmbuf_pkt_len(m_src) + hdr_len;
	rlen = otx2_ipsec_po_out_rlen_get(sess, dlen - hdr_len);

	extend_head = hdr_len + RTE_ETHER_HDR_LEN;
	extend_tail = rlen - dlen;
	mdata_len = m_info->lb_mlen + 8;

	mdata = rte_pktmbuf_append(m_src, extend_tail + mdata_len);
	if (unlikely(mdata == NULL)) {
		otx2_err("Not enough tail room\n");
		ret = -ENOMEM;
		goto exit;
	}

	mdata += extend_tail; /* mdata follows encrypted data */
	req = alloc_request_struct(mdata, (void *)cop, mdata_len);

	data = rte_pktmbuf_prepend(m_src, extend_head);
	if (unlikely(data == NULL)) {
		otx2_err("Not enough head room\n");
		ret = -ENOMEM;
		goto exit;
	}

	/*
	 * Move the Ethernet header, to insert otx2_ipsec_po_out_hdr prior
	 * to the IP header
	 */
	memcpy(data, data + hdr_len, RTE_ETHER_HDR_LEN);

	hdr = (struct otx2_ipsec_po_out_hdr *)rte_pktmbuf_adj(m_src,
							RTE_ETHER_HDR_LEN);

	if (ctl_wrd->enc_type == OTX2_IPSEC_FP_SA_ENC_AES_GCM) {
		memcpy(&hdr->iv[0], &sa->iv.gcm.nonce, 4);
		memcpy(&hdr->iv[4], rte_crypto_op_ctod_offset(cop, uint8_t *,
			sess->iv_offset), sess->iv_length);
	} else if (ctl_wrd->auth_type == OTX2_IPSEC_FP_SA_ENC_AES_CBC) {
		memcpy(&hdr->iv[0], rte_crypto_op_ctod_offset(cop, uint8_t *,
			sess->iv_offset), sess->iv_length);
	}

	/* Prepare CPT instruction */
	word0.u64 = sess->ucmd_w0;
	word0.s.dlen = dlen;

	req->ist.ei0 = word0.u64;
	req->ist.ei1 = rte_pktmbuf_iova(m_src);
	req->ist.ei2 = req->ist.ei1;
	req->ist.ei3 = sess->ucmd_w3;

	hdr->seq = rte_cpu_to_be_32(sess->seq_lo);
	hdr->ip_id = rte_cpu_to_be_32(sess->ip_id);

	sess->ip_id++;
	sess->esn++;

exit:
	*prep_req = req;

	return ret;
}

static __rte_always_inline int
process_inb_sa(struct rte_crypto_op *cop,
	      struct otx2_sec_session_ipsec_lp *sess,
	      struct cpt_qp_meta_info *m_info, void **prep_req)
{
	struct rte_crypto_sym_op *sym_op = cop->sym;
	struct rte_mbuf *m_src = sym_op->m_src;
	struct cpt_request_info *req = NULL;
	int mdata_len, ret = 0;
	vq_cmd_word0_t word0;
	uint32_t dlen;
	char *mdata;

	dlen = rte_pktmbuf_pkt_len(m_src);
	mdata_len = m_info->lb_mlen + 8;

	mdata = rte_pktmbuf_append(m_src, mdata_len);
	if (unlikely(mdata == NULL)) {
		otx2_err("Not enough tail room\n");
		ret = -ENOMEM;
		goto exit;
	}

	req = alloc_request_struct(mdata, (void *)cop, mdata_len);

	/* Prepare CPT instruction */
	word0.u64 = sess->ucmd_w0;
	word0.s.dlen   = dlen;

	req->ist.ei0 = word0.u64;
	req->ist.ei1 = rte_pktmbuf_iova(m_src);
	req->ist.ei2 = req->ist.ei1;
	req->ist.ei3 = sess->ucmd_w3;

exit:
	*prep_req = req;
	return ret;
}
#endif /* __OTX2_IPSEC_PO_OPS_H__ */
