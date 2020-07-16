/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#ifndef __OTX2_ETHDEV_SEC_TX_H__
#define __OTX2_ETHDEV_SEC_TX_H__

#include <rte_security.h>
#include <rte_mbuf.h>

#include "otx2_ethdev_sec.h"
#include "otx2_security.h"

struct otx2_ipsec_fp_out_hdr {
	uint32_t ip_id;
	uint32_t seq;
	uint8_t iv[16];
};

static __rte_always_inline int32_t
otx2_ipsec_fp_out_rlen_get(struct otx2_sec_session_ipsec_ip *sess,
			   uint32_t plen)
{
	uint32_t enc_payload_len;

	enc_payload_len = RTE_ALIGN_CEIL(plen + sess->roundup_len,
			sess->roundup_byte);

	return sess->partial_len + enc_payload_len;
}

static __rte_always_inline void
otx2_ssogws_head_wait(struct otx2_ssogws *ws);

static __rte_always_inline int
otx2_sec_event_tx(struct otx2_ssogws *ws, struct rte_event *ev,
		  struct rte_mbuf *m, const struct otx2_eth_txq *txq,
		  const uint32_t offload_flags)
{
	uint32_t dlen, rlen, desc_headroom, extend_head, extend_tail;
	struct otx2_sec_session_ipsec_ip *sess;
	struct otx2_ipsec_fp_out_hdr *hdr;
	struct otx2_ipsec_fp_out_sa *sa;
	uint64_t data_addr, desc_addr;
	struct otx2_sec_session *priv;
	struct otx2_cpt_inst_s inst;
	uint64_t lmt_status;
	char *data;

	struct desc {
		struct otx2_cpt_res cpt_res __rte_aligned(OTX2_CPT_RES_ALIGN);
		struct nix_send_hdr_s nix_hdr
				__rte_aligned(OTX2_NIX_SEND_DESC_ALIGN);
		union nix_send_sg_s nix_sg;
		struct nix_iova_s nix_iova;
	} *sd;

	priv = get_sec_session_private_data((void *)(m->udata64));
	sess = &priv->ipsec.ip;
	sa = &sess->out_sa;

	RTE_ASSERT(sess->cpt_lmtline != NULL);
	RTE_ASSERT(!(offload_flags & (NIX_TX_OFFLOAD_MBUF_NOFF_F |
				      NIX_TX_OFFLOAD_VLAN_QINQ_F)));

	dlen = rte_pktmbuf_pkt_len(m) + sizeof(*hdr) - RTE_ETHER_HDR_LEN;
	rlen = otx2_ipsec_fp_out_rlen_get(sess, dlen - sizeof(*hdr));

	RTE_BUILD_BUG_ON(OTX2_CPT_RES_ALIGN % OTX2_NIX_SEND_DESC_ALIGN);
	RTE_BUILD_BUG_ON(sizeof(sd->cpt_res) % OTX2_NIX_SEND_DESC_ALIGN);

	extend_head = sizeof(*hdr);
	extend_tail = rlen - dlen;

	desc_headroom = (OTX2_CPT_RES_ALIGN - 1) + sizeof(*sd);

	if (unlikely(!rte_pktmbuf_is_contiguous(m)) ||
	    unlikely(rte_pktmbuf_headroom(m) < extend_head + desc_headroom) ||
	    unlikely(rte_pktmbuf_tailroom(m) < extend_tail)) {
		goto drop;
	}

	/*
	 * Extend mbuf data to point to the expected packet buffer for NIX.
	 * This includes the Ethernet header followed by the encrypted IPsec
	 * payload
	 */
	rte_pktmbuf_append(m, extend_tail);
	data = rte_pktmbuf_prepend(m, extend_head);
	data_addr = rte_pktmbuf_mtophys(m);

	/*
	 * Move the Ethernet header, to insert otx2_ipsec_fp_out_hdr prior
	 * to the IP header
	 */
	memcpy(data, data + sizeof(*hdr), RTE_ETHER_HDR_LEN);

	hdr = (struct otx2_ipsec_fp_out_hdr *)(data + RTE_ETHER_HDR_LEN);

	if (sa->ctl.enc_type == OTX2_IPSEC_FP_SA_ENC_AES_GCM) {
		/* AES-128-GCM */
		memcpy(hdr->iv, &sa->nonce, 4);
		memset(hdr->iv + 4, 0, 12); //TODO: make it random
	} else {
		/* AES-128-[CBC] + [SHA1] */
		memset(hdr->iv, 0, 16); //TODO: make it random
	}

	/* Keep CPT result and NIX send descriptors in headroom */
	sd = (void *)RTE_PTR_ALIGN(data - desc_headroom, OTX2_CPT_RES_ALIGN);
	desc_addr = data_addr - RTE_PTR_DIFF(data, sd);

	/* Prepare CPT instruction */

	inst.nixtx_addr = (desc_addr + offsetof(struct desc, nix_hdr)) >> 4;
	inst.doneint = 0;
	inst.nixtxl = 1;
	inst.res_addr = desc_addr + offsetof(struct desc, cpt_res);
	inst.u64[2] = 0;
	inst.u64[3] = 0;
	inst.wqe_ptr = desc_addr >> 3;	/* FIXME: Handle errors */
	inst.qord = 1;
	inst.opcode = OTX2_CPT_OP_INLINE_IPSEC_OUTB;
	inst.dlen = dlen;
	inst.dptr = data_addr + RTE_ETHER_HDR_LEN;
	inst.u64[7] = sess->inst_w7;

	/* First word contains 8 bit completion code & 8 bit uc comp code */
	sd->cpt_res.u16[0] = 0;

	/* Prepare NIX send descriptors for output expected from CPT */

	sd->nix_hdr.w0.u = 0;
	sd->nix_hdr.w1.u = 0;
	sd->nix_hdr.w0.sq = txq->sq;
	sd->nix_hdr.w0.sizem1 = 1;
	sd->nix_hdr.w0.total = rte_pktmbuf_data_len(m);
	sd->nix_hdr.w0.aura = npa_lf_aura_handle_to_aura(m->pool->pool_id);

	sd->nix_sg.u = 0;
	sd->nix_sg.subdc = NIX_SUBDC_SG;
	sd->nix_sg.ld_type = NIX_SENDLDTYPE_LDD;
	sd->nix_sg.segs = 1;
	sd->nix_sg.seg1_size = rte_pktmbuf_data_len(m);

	sd->nix_iova.addr = rte_mbuf_data_iova(m);

	/* Mark mempool object as "put" since it is freed by NIX */
	__mempool_check_cookies(m->pool, (void **)&m, 1, 0);

	if (!ev->sched_type)
		otx2_ssogws_head_wait(ws);

	inst.param1 = sess->esn_hi >> 16;
	inst.param2 = sess->esn_hi & 0xffff;

	hdr->seq = rte_cpu_to_be_32(sess->seq);
	hdr->ip_id = rte_cpu_to_be_32(sess->ip_id);

	sess->ip_id++;
	sess->esn++;

	rte_cio_wmb();

	do {
		otx2_lmt_mov(sess->cpt_lmtline, &inst, 2);
		lmt_status = otx2_lmt_submit(sess->cpt_nq_reg);
	} while (lmt_status == 0);

	return 1;

drop:
	if (offload_flags & NIX_TX_OFFLOAD_MBUF_NOFF_F) {
		/* Don't free if reference count > 1 */
		if (rte_pktmbuf_prefree_seg(m) == NULL)
			return 0;
	}
	rte_pktmbuf_free(m);
	return 0;
}

#endif /* __OTX2_ETHDEV_SEC_TX_H__ */
