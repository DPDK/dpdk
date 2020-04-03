/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include "otx2_ethdev.h"

#define nix_dump(fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)
#define NIX_REG_INFO(reg) {reg, #reg}
#define NIX_REG_NAME_SZ 48

struct nix_lf_reg_info {
	uint32_t offset;
	const char *name;
};

static const struct
nix_lf_reg_info nix_lf_reg[] = {
	NIX_REG_INFO(NIX_LF_RX_SECRETX(0)),
	NIX_REG_INFO(NIX_LF_RX_SECRETX(1)),
	NIX_REG_INFO(NIX_LF_RX_SECRETX(2)),
	NIX_REG_INFO(NIX_LF_RX_SECRETX(3)),
	NIX_REG_INFO(NIX_LF_RX_SECRETX(4)),
	NIX_REG_INFO(NIX_LF_RX_SECRETX(5)),
	NIX_REG_INFO(NIX_LF_CFG),
	NIX_REG_INFO(NIX_LF_GINT),
	NIX_REG_INFO(NIX_LF_GINT_W1S),
	NIX_REG_INFO(NIX_LF_GINT_ENA_W1C),
	NIX_REG_INFO(NIX_LF_GINT_ENA_W1S),
	NIX_REG_INFO(NIX_LF_ERR_INT),
	NIX_REG_INFO(NIX_LF_ERR_INT_W1S),
	NIX_REG_INFO(NIX_LF_ERR_INT_ENA_W1C),
	NIX_REG_INFO(NIX_LF_ERR_INT_ENA_W1S),
	NIX_REG_INFO(NIX_LF_RAS),
	NIX_REG_INFO(NIX_LF_RAS_W1S),
	NIX_REG_INFO(NIX_LF_RAS_ENA_W1C),
	NIX_REG_INFO(NIX_LF_RAS_ENA_W1S),
	NIX_REG_INFO(NIX_LF_SQ_OP_ERR_DBG),
	NIX_REG_INFO(NIX_LF_MNQ_ERR_DBG),
	NIX_REG_INFO(NIX_LF_SEND_ERR_DBG),
};

static int
nix_lf_get_reg_count(struct otx2_eth_dev *dev)
{
	int reg_count = 0;

	reg_count = RTE_DIM(nix_lf_reg);
	/* NIX_LF_TX_STATX */
	reg_count += dev->lf_tx_stats;
	/* NIX_LF_RX_STATX */
	reg_count += dev->lf_rx_stats;
	/* NIX_LF_QINTX_CNT*/
	reg_count += dev->qints;
	/* NIX_LF_QINTX_INT */
	reg_count += dev->qints;
	/* NIX_LF_QINTX_ENA_W1S */
	reg_count += dev->qints;
	/* NIX_LF_QINTX_ENA_W1C */
	reg_count += dev->qints;
	/* NIX_LF_CINTX_CNT */
	reg_count += dev->cints;
	/* NIX_LF_CINTX_WAIT */
	reg_count += dev->cints;
	/* NIX_LF_CINTX_INT */
	reg_count += dev->cints;
	/* NIX_LF_CINTX_INT_W1S */
	reg_count += dev->cints;
	/* NIX_LF_CINTX_ENA_W1S */
	reg_count += dev->cints;
	/* NIX_LF_CINTX_ENA_W1C */
	reg_count += dev->cints;

	return reg_count;
}

int
otx2_nix_reg_dump(struct otx2_eth_dev *dev, uint64_t *data)
{
	uintptr_t nix_lf_base = dev->base;
	bool dump_stdout;
	uint64_t reg;
	uint32_t i;

	dump_stdout = data ? 0 : 1;

	for (i = 0; i < RTE_DIM(nix_lf_reg); i++) {
		reg = otx2_read64(nix_lf_base + nix_lf_reg[i].offset);
		if (dump_stdout && reg)
			nix_dump("%32s = 0x%" PRIx64,
				 nix_lf_reg[i].name, reg);
		if (data)
			*data++ = reg;
	}

	/* NIX_LF_TX_STATX */
	for (i = 0; i < dev->lf_tx_stats; i++) {
		reg = otx2_read64(nix_lf_base + NIX_LF_TX_STATX(i));
		if (dump_stdout && reg)
			nix_dump("%32s_%d = 0x%" PRIx64,
				 "NIX_LF_TX_STATX", i, reg);
		if (data)
			*data++ = reg;
	}

	/* NIX_LF_RX_STATX */
	for (i = 0; i < dev->lf_rx_stats; i++) {
		reg = otx2_read64(nix_lf_base + NIX_LF_RX_STATX(i));
		if (dump_stdout && reg)
			nix_dump("%32s_%d = 0x%" PRIx64,
				 "NIX_LF_RX_STATX", i, reg);
		if (data)
			*data++ = reg;
	}

	/* NIX_LF_QINTX_CNT*/
	for (i = 0; i < dev->qints; i++) {
		reg = otx2_read64(nix_lf_base + NIX_LF_QINTX_CNT(i));
		if (dump_stdout && reg)
			nix_dump("%32s_%d = 0x%" PRIx64,
				 "NIX_LF_QINTX_CNT", i, reg);
		if (data)
			*data++ = reg;
	}

	/* NIX_LF_QINTX_INT */
	for (i = 0; i < dev->qints; i++) {
		reg = otx2_read64(nix_lf_base + NIX_LF_QINTX_INT(i));
		if (dump_stdout && reg)
			nix_dump("%32s_%d = 0x%" PRIx64,
				 "NIX_LF_QINTX_INT", i, reg);
		if (data)
			*data++ = reg;
	}

	/* NIX_LF_QINTX_ENA_W1S */
	for (i = 0; i < dev->qints; i++) {
		reg = otx2_read64(nix_lf_base + NIX_LF_QINTX_ENA_W1S(i));
		if (dump_stdout && reg)
			nix_dump("%32s_%d = 0x%" PRIx64,
				 "NIX_LF_QINTX_ENA_W1S", i, reg);
		if (data)
			*data++ = reg;
	}

	/* NIX_LF_QINTX_ENA_W1C */
	for (i = 0; i < dev->qints; i++) {
		reg = otx2_read64(nix_lf_base + NIX_LF_QINTX_ENA_W1C(i));
		if (dump_stdout && reg)
			nix_dump("%32s_%d = 0x%" PRIx64,
				 "NIX_LF_QINTX_ENA_W1C", i, reg);
		if (data)
			*data++ = reg;
	}

	/* NIX_LF_CINTX_CNT */
	for (i = 0; i < dev->cints; i++) {
		reg = otx2_read64(nix_lf_base + NIX_LF_CINTX_CNT(i));
		if (dump_stdout && reg)
			nix_dump("%32s_%d = 0x%" PRIx64,
				 "NIX_LF_CINTX_CNT", i, reg);
		if (data)
			*data++ = reg;
	}

	/* NIX_LF_CINTX_WAIT */
	for (i = 0; i < dev->cints; i++) {
		reg = otx2_read64(nix_lf_base + NIX_LF_CINTX_WAIT(i));
		if (dump_stdout && reg)
			nix_dump("%32s_%d = 0x%" PRIx64,
				 "NIX_LF_CINTX_WAIT", i, reg);
		if (data)
			*data++ = reg;
	}

	/* NIX_LF_CINTX_INT */
	for (i = 0; i < dev->cints; i++) {
		reg = otx2_read64(nix_lf_base + NIX_LF_CINTX_INT(i));
		if (dump_stdout && reg)
			nix_dump("%32s_%d = 0x%" PRIx64,
				 "NIX_LF_CINTX_INT", i, reg);
		if (data)
			*data++ = reg;
	}

	/* NIX_LF_CINTX_INT_W1S */
	for (i = 0; i < dev->cints; i++) {
		reg = otx2_read64(nix_lf_base + NIX_LF_CINTX_INT_W1S(i));
		if (dump_stdout && reg)
			nix_dump("%32s_%d = 0x%" PRIx64,
				 "NIX_LF_CINTX_INT_W1S", i, reg);
		if (data)
			*data++ = reg;
	}

	/* NIX_LF_CINTX_ENA_W1S */
	for (i = 0; i < dev->cints; i++) {
		reg = otx2_read64(nix_lf_base + NIX_LF_CINTX_ENA_W1S(i));
		if (dump_stdout && reg)
			nix_dump("%32s_%d = 0x%" PRIx64,
				 "NIX_LF_CINTX_ENA_W1S", i, reg);
		if (data)
			*data++ = reg;
	}

	/* NIX_LF_CINTX_ENA_W1C */
	for (i = 0; i < dev->cints; i++) {
		reg = otx2_read64(nix_lf_base + NIX_LF_CINTX_ENA_W1C(i));
		if (dump_stdout && reg)
			nix_dump("%32s_%d = 0x%" PRIx64,
				 "NIX_LF_CINTX_ENA_W1C", i, reg);
		if (data)
			*data++ = reg;
	}
	return 0;
}

int
otx2_nix_dev_get_reg(struct rte_eth_dev *eth_dev, struct rte_dev_reg_info *regs)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	uint64_t *data = regs->data;

	if (data == NULL) {
		regs->length = nix_lf_get_reg_count(dev);
		regs->width = 8;
		return 0;
	}

	if (!regs->length ||
	    regs->length == (uint32_t)nix_lf_get_reg_count(dev)) {
		otx2_nix_reg_dump(dev, data);
		return 0;
	}

	return -ENOTSUP;
}

static inline void
nix_lf_sq_dump(__otx2_io struct nix_sq_ctx_s *ctx)
{
	nix_dump("W0: sqe_way_mask \t\t%d\nW0: cq \t\t\t\t%d",
		 ctx->sqe_way_mask, ctx->cq);
	nix_dump("W0: sdp_mcast \t\t\t%d\nW0: substream \t\t\t0x%03x",
		 ctx->sdp_mcast, ctx->substream);
	nix_dump("W0: qint_idx \t\t\t%d\nW0: ena \t\t\t%d\n",
		 ctx->qint_idx, ctx->ena);

	nix_dump("W1: sqb_count \t\t\t%d\nW1: default_chan \t\t%d",
		 ctx->sqb_count, ctx->default_chan);
	nix_dump("W1: smq_rr_quantum \t\t%d\nW1: sso_ena \t\t\t%d",
		 ctx->smq_rr_quantum, ctx->sso_ena);
	nix_dump("W1: xoff \t\t\t%d\nW1: cq_ena \t\t\t%d\nW1: smq\t\t\t\t%d\n",
		 ctx->xoff, ctx->cq_ena, ctx->smq);

	nix_dump("W2: sqe_stype \t\t\t%d\nW2: sq_int_ena \t\t\t%d",
		 ctx->sqe_stype, ctx->sq_int_ena);
	nix_dump("W2: sq_int  \t\t\t%d\nW2: sqb_aura \t\t\t%d",
		 ctx->sq_int, ctx->sqb_aura);
	nix_dump("W2: smq_rr_count \t\t%d\n",  ctx->smq_rr_count);

	nix_dump("W3: smq_next_sq_vld\t\t%d\nW3: smq_pend\t\t\t%d",
		 ctx->smq_next_sq_vld, ctx->smq_pend);
	nix_dump("W3: smenq_next_sqb_vld  \t%d\nW3: head_offset\t\t\t%d",
		 ctx->smenq_next_sqb_vld, ctx->head_offset);
	nix_dump("W3: smenq_offset\t\t%d\nW3: tail_offset \t\t%d",
		 ctx->smenq_offset, ctx->tail_offset);
	nix_dump("W3: smq_lso_segnum \t\t%d\nW3: smq_next_sq \t\t%d",
		 ctx->smq_lso_segnum, ctx->smq_next_sq);
	nix_dump("W3: mnq_dis \t\t\t%d\nW3: lmt_dis \t\t\t%d",
		 ctx->mnq_dis, ctx->lmt_dis);
	nix_dump("W3: cq_limit\t\t\t%d\nW3: max_sqe_size\t\t%d\n",
		 ctx->cq_limit, ctx->max_sqe_size);

	nix_dump("W4: next_sqb \t\t\t0x%" PRIx64 "", ctx->next_sqb);
	nix_dump("W5: tail_sqb \t\t\t0x%" PRIx64 "", ctx->tail_sqb);
	nix_dump("W6: smenq_sqb \t\t\t0x%" PRIx64 "", ctx->smenq_sqb);
	nix_dump("W7: smenq_next_sqb \t\t0x%" PRIx64 "", ctx->smenq_next_sqb);
	nix_dump("W8: head_sqb \t\t\t0x%" PRIx64 "", ctx->head_sqb);

	nix_dump("W9: vfi_lso_vld \t\t%d\nW9: vfi_lso_vlan1_ins_ena\t%d",
		 ctx->vfi_lso_vld, ctx->vfi_lso_vlan1_ins_ena);
	nix_dump("W9: vfi_lso_vlan0_ins_ena\t%d\nW9: vfi_lso_mps\t\t\t%d",
		 ctx->vfi_lso_vlan0_ins_ena, ctx->vfi_lso_mps);
	nix_dump("W9: vfi_lso_sb \t\t\t%d\nW9: vfi_lso_sizem1\t\t%d",
		 ctx->vfi_lso_sb, ctx->vfi_lso_sizem1);
	nix_dump("W9: vfi_lso_total\t\t%d", ctx->vfi_lso_total);

	nix_dump("W10: scm_lso_rem \t\t0x%" PRIx64 "",
		 (uint64_t)ctx->scm_lso_rem);
	nix_dump("W11: octs \t\t\t0x%" PRIx64 "", (uint64_t)ctx->octs);
	nix_dump("W12: pkts \t\t\t0x%" PRIx64 "", (uint64_t)ctx->pkts);
	nix_dump("W14: dropped_octs \t\t0x%" PRIx64 "",
		 (uint64_t)ctx->drop_octs);
	nix_dump("W15: dropped_pkts \t\t0x%" PRIx64 "",
		 (uint64_t)ctx->drop_pkts);
}

static inline void
nix_lf_rq_dump(__otx2_io struct nix_rq_ctx_s *ctx)
{
	nix_dump("W0: wqe_aura \t\t\t%d\nW0: substream \t\t\t0x%03x",
		 ctx->wqe_aura, ctx->substream);
	nix_dump("W0: cq \t\t\t\t%d\nW0: ena_wqwd \t\t\t%d",
		 ctx->cq, ctx->ena_wqwd);
	nix_dump("W0: ipsech_ena \t\t\t%d\nW0: sso_ena \t\t\t%d",
		 ctx->ipsech_ena, ctx->sso_ena);
	nix_dump("W0: ena \t\t\t%d\n", ctx->ena);

	nix_dump("W1: lpb_drop_ena \t\t%d\nW1: spb_drop_ena \t\t%d",
		 ctx->lpb_drop_ena, ctx->spb_drop_ena);
	nix_dump("W1: xqe_drop_ena \t\t%d\nW1: wqe_caching \t\t%d",
		 ctx->xqe_drop_ena, ctx->wqe_caching);
	nix_dump("W1: pb_caching \t\t\t%d\nW1: sso_tt \t\t\t%d",
		 ctx->pb_caching, ctx->sso_tt);
	nix_dump("W1: sso_grp \t\t\t%d\nW1: lpb_aura \t\t\t%d",
		 ctx->sso_grp, ctx->lpb_aura);
	nix_dump("W1: spb_aura \t\t\t%d\n", ctx->spb_aura);

	nix_dump("W2: xqe_hdr_split \t\t%d\nW2: xqe_imm_copy \t\t%d",
		 ctx->xqe_hdr_split, ctx->xqe_imm_copy);
	nix_dump("W2: xqe_imm_size \t\t%d\nW2: later_skip \t\t\t%d",
		 ctx->xqe_imm_size, ctx->later_skip);
	nix_dump("W2: first_skip \t\t\t%d\nW2: lpb_sizem1 \t\t\t%d",
		 ctx->first_skip, ctx->lpb_sizem1);
	nix_dump("W2: spb_ena \t\t\t%d\nW2: wqe_skip \t\t\t%d",
		 ctx->spb_ena, ctx->wqe_skip);
	nix_dump("W2: spb_sizem1 \t\t\t%d\n", ctx->spb_sizem1);

	nix_dump("W3: spb_pool_pass \t\t%d\nW3: spb_pool_drop \t\t%d",
		 ctx->spb_pool_pass, ctx->spb_pool_drop);
	nix_dump("W3: spb_aura_pass \t\t%d\nW3: spb_aura_drop \t\t%d",
		 ctx->spb_aura_pass, ctx->spb_aura_drop);
	nix_dump("W3: wqe_pool_pass \t\t%d\nW3: wqe_pool_drop \t\t%d",
		 ctx->wqe_pool_pass, ctx->wqe_pool_drop);
	nix_dump("W3: xqe_pass \t\t\t%d\nW3: xqe_drop \t\t\t%d\n",
		 ctx->xqe_pass, ctx->xqe_drop);

	nix_dump("W4: qint_idx \t\t\t%d\nW4: rq_int_ena \t\t\t%d",
		 ctx->qint_idx, ctx->rq_int_ena);
	nix_dump("W4: rq_int \t\t\t%d\nW4: lpb_pool_pass \t\t%d",
		 ctx->rq_int, ctx->lpb_pool_pass);
	nix_dump("W4: lpb_pool_drop \t\t%d\nW4: lpb_aura_pass \t\t%d",
		 ctx->lpb_pool_drop, ctx->lpb_aura_pass);
	nix_dump("W4: lpb_aura_drop \t\t%d\n", ctx->lpb_aura_drop);

	nix_dump("W5: flow_tagw \t\t\t%d\nW5: bad_utag \t\t\t%d",
		 ctx->flow_tagw, ctx->bad_utag);
	nix_dump("W5: good_utag \t\t\t%d\nW5: ltag \t\t\t%d\n",
		 ctx->good_utag, ctx->ltag);

	nix_dump("W6: octs \t\t\t0x%" PRIx64 "", (uint64_t)ctx->octs);
	nix_dump("W7: pkts \t\t\t0x%" PRIx64 "", (uint64_t)ctx->pkts);
	nix_dump("W8: drop_octs \t\t\t0x%" PRIx64 "", (uint64_t)ctx->drop_octs);
	nix_dump("W9: drop_pkts \t\t\t0x%" PRIx64 "", (uint64_t)ctx->drop_pkts);
	nix_dump("W10: re_pkts \t\t\t0x%" PRIx64 "\n", (uint64_t)ctx->re_pkts);
}

static inline void
nix_lf_cq_dump(__otx2_io struct nix_cq_ctx_s *ctx)
{
	nix_dump("W0: base \t\t\t0x%" PRIx64 "\n", ctx->base);

	nix_dump("W1: wrptr \t\t\t%" PRIx64 "", (uint64_t)ctx->wrptr);
	nix_dump("W1: avg_con \t\t\t%d\nW1: cint_idx \t\t\t%d",
		 ctx->avg_con, ctx->cint_idx);
	nix_dump("W1: cq_err \t\t\t%d\nW1: qint_idx \t\t\t%d",
		 ctx->cq_err, ctx->qint_idx);
	nix_dump("W1: bpid  \t\t\t%d\nW1: bp_ena \t\t\t%d\n",
		 ctx->bpid, ctx->bp_ena);

	nix_dump("W2: update_time \t\t%d\nW2: avg_level \t\t\t%d",
		 ctx->update_time, ctx->avg_level);
	nix_dump("W2: head \t\t\t%d\nW2: tail \t\t\t%d\n",
		 ctx->head, ctx->tail);

	nix_dump("W3: cq_err_int_ena \t\t%d\nW3: cq_err_int \t\t\t%d",
		 ctx->cq_err_int_ena, ctx->cq_err_int);
	nix_dump("W3: qsize \t\t\t%d\nW3: caching \t\t\t%d",
		 ctx->qsize, ctx->caching);
	nix_dump("W3: substream \t\t\t0x%03x\nW3: ena \t\t\t%d",
		 ctx->substream, ctx->ena);
	nix_dump("W3: drop_ena \t\t\t%d\nW3: drop \t\t\t%d",
		 ctx->drop_ena, ctx->drop);
	nix_dump("W3: bp \t\t\t\t%d\n", ctx->bp);
}

int
otx2_nix_queues_ctx_dump(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	int rc, q, rq = eth_dev->data->nb_rx_queues;
	int sq = eth_dev->data->nb_tx_queues;
	struct otx2_mbox *mbox = dev->mbox;
	struct npa_aq_enq_rsp *npa_rsp;
	struct npa_aq_enq_req *npa_aq;
	struct otx2_npa_lf *npa_lf;
	struct nix_aq_enq_rsp *rsp;
	struct nix_aq_enq_req *aq;

	npa_lf = otx2_npa_lf_obj_get();

	for (q = 0; q < rq; q++) {
		aq = otx2_mbox_alloc_msg_nix_aq_enq(mbox);
		aq->qidx = q;
		aq->ctype = NIX_AQ_CTYPE_CQ;
		aq->op = NIX_AQ_INSTOP_READ;

		rc = otx2_mbox_process_msg(mbox, (void *)&rsp);
		if (rc) {
			otx2_err("Failed to get cq context");
			goto fail;
		}
		nix_dump("============== port=%d cq=%d ===============",
			 eth_dev->data->port_id, q);
		nix_lf_cq_dump(&rsp->cq);
	}

	for (q = 0; q < rq; q++) {
		aq = otx2_mbox_alloc_msg_nix_aq_enq(mbox);
		aq->qidx = q;
		aq->ctype = NIX_AQ_CTYPE_RQ;
		aq->op = NIX_AQ_INSTOP_READ;

		rc = otx2_mbox_process_msg(mbox, (void **)&rsp);
		if (rc) {
			otx2_err("Failed to get rq context");
			goto fail;
		}
		nix_dump("============== port=%d rq=%d ===============",
			 eth_dev->data->port_id, q);
		nix_lf_rq_dump(&rsp->rq);
	}
	for (q = 0; q < sq; q++) {
		aq = otx2_mbox_alloc_msg_nix_aq_enq(mbox);
		aq->qidx = q;
		aq->ctype = NIX_AQ_CTYPE_SQ;
		aq->op = NIX_AQ_INSTOP_READ;

		rc = otx2_mbox_process_msg(mbox, (void *)&rsp);
		if (rc) {
			otx2_err("Failed to get sq context");
			goto fail;
		}
		nix_dump("============== port=%d sq=%d ===============",
			 eth_dev->data->port_id, q);
		nix_lf_sq_dump(&rsp->sq);

		if (!npa_lf) {
			otx2_err("NPA LF doesn't exist");
			continue;
		}

		/* Dump SQB Aura minimal info */
		npa_aq = otx2_mbox_alloc_msg_npa_aq_enq(npa_lf->mbox);
		npa_aq->aura_id = rsp->sq.sqb_aura;
		npa_aq->ctype = NPA_AQ_CTYPE_AURA;
		npa_aq->op = NPA_AQ_INSTOP_READ;

		rc = otx2_mbox_process_msg(npa_lf->mbox, (void *)&npa_rsp);
		if (rc) {
			otx2_err("Failed to get sq's sqb_aura context");
			continue;
		}

		nix_dump("\nSQB Aura W0: Pool addr\t\t0x%"PRIx64"",
			 npa_rsp->aura.pool_addr);
		nix_dump("SQB Aura W1: ena\t\t\t%d",
			 npa_rsp->aura.ena);
		nix_dump("SQB Aura W2: count\t\t%"PRIx64"",
			 (uint64_t)npa_rsp->aura.count);
		nix_dump("SQB Aura W3: limit\t\t%"PRIx64"",
			 (uint64_t)npa_rsp->aura.limit);
		nix_dump("SQB Aura W3: fc_ena\t\t%d",
			 npa_rsp->aura.fc_ena);
		nix_dump("SQB Aura W4: fc_addr\t\t0x%"PRIx64"\n",
			 npa_rsp->aura.fc_addr);
	}

fail:
	return rc;
}

/* Dumps struct nix_cqe_hdr_s and struct nix_rx_parse_s */
void
otx2_nix_cqe_dump(const struct nix_cqe_hdr_s *cq)
{
	const struct nix_rx_parse_s *rx =
		 (const struct nix_rx_parse_s *)((const uint64_t *)cq + 1);

	nix_dump("tag \t\t0x%x\tq \t\t%d\t\tnode \t\t%d\tcqe_type \t%d",
		 cq->tag, cq->q, cq->node, cq->cqe_type);

	nix_dump("W0: chan \t%d\t\tdesc_sizem1 \t%d",
		 rx->chan, rx->desc_sizem1);
	nix_dump("W0: imm_copy \t%d\t\texpress \t%d",
		 rx->imm_copy, rx->express);
	nix_dump("W0: wqwd \t%d\t\terrlev \t\t%d\t\terrcode \t%d",
		 rx->wqwd, rx->errlev, rx->errcode);
	nix_dump("W0: latype \t%d\t\tlbtype \t\t%d\t\tlctype \t\t%d",
		 rx->latype, rx->lbtype, rx->lctype);
	nix_dump("W0: ldtype \t%d\t\tletype \t\t%d\t\tlftype \t\t%d",
		 rx->ldtype, rx->letype, rx->lftype);
	nix_dump("W0: lgtype \t%d \t\tlhtype \t\t%d",
		 rx->lgtype, rx->lhtype);

	nix_dump("W1: pkt_lenm1 \t%d", rx->pkt_lenm1);
	nix_dump("W1: l2m \t%d\t\tl2b \t\t%d\t\tl3m \t\t%d\tl3b \t\t%d",
		 rx->l2m, rx->l2b, rx->l3m, rx->l3b);
	nix_dump("W1: vtag0_valid %d\t\tvtag0_gone \t%d",
		 rx->vtag0_valid, rx->vtag0_gone);
	nix_dump("W1: vtag1_valid %d\t\tvtag1_gone \t%d",
		 rx->vtag1_valid, rx->vtag1_gone);
	nix_dump("W1: pkind \t%d", rx->pkind);
	nix_dump("W1: vtag0_tci \t%d\t\tvtag1_tci \t%d",
		 rx->vtag0_tci, rx->vtag1_tci);

	nix_dump("W2: laflags \t%d\t\tlbflags\t\t%d\t\tlcflags \t%d",
		 rx->laflags, rx->lbflags, rx->lcflags);
	nix_dump("W2: ldflags \t%d\t\tleflags\t\t%d\t\tlfflags \t%d",
		 rx->ldflags, rx->leflags, rx->lfflags);
	nix_dump("W2: lgflags \t%d\t\tlhflags \t%d",
		 rx->lgflags, rx->lhflags);

	nix_dump("W3: eoh_ptr \t%d\t\twqe_aura \t%d\t\tpb_aura \t%d",
		 rx->eoh_ptr, rx->wqe_aura, rx->pb_aura);
	nix_dump("W3: match_id \t%d", rx->match_id);

	nix_dump("W4: laptr \t%d\t\tlbptr \t\t%d\t\tlcptr \t\t%d",
		 rx->laptr, rx->lbptr, rx->lcptr);
	nix_dump("W4: ldptr \t%d\t\tleptr \t\t%d\t\tlfptr \t\t%d",
		 rx->ldptr, rx->leptr, rx->lfptr);
	nix_dump("W4: lgptr \t%d\t\tlhptr \t\t%d", rx->lgptr, rx->lhptr);

	nix_dump("W5: vtag0_ptr \t%d\t\tvtag1_ptr \t%d\t\tflow_key_alg \t%d",
		 rx->vtag0_ptr, rx->vtag1_ptr, rx->flow_key_alg);
}

static uint8_t
prepare_nix_tm_reg_dump(uint16_t hw_lvl, uint16_t schq, uint16_t link,
			uint64_t *reg, char regstr[][NIX_REG_NAME_SZ])
{
	uint8_t k = 0;

	switch (hw_lvl) {
	case NIX_TXSCH_LVL_SMQ:
		reg[k] = NIX_AF_SMQX_CFG(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ,
			 "NIX_AF_SMQ[%u]_CFG", schq);

		reg[k] = NIX_AF_MDQX_PARENT(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ,
			 "NIX_AF_MDQ[%u]_PARENT", schq);

		reg[k] = NIX_AF_MDQX_SCHEDULE(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ,
			 "NIX_AF_MDQ[%u]_SCHEDULE", schq);

		reg[k] = NIX_AF_MDQX_PIR(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ,
			 "NIX_AF_MDQ[%u]_PIR", schq);

		reg[k] = NIX_AF_MDQX_CIR(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ,
			 "NIX_AF_MDQ[%u]_CIR", schq);

		reg[k] = NIX_AF_MDQX_SHAPE(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ,
			 "NIX_AF_MDQ[%u]_SHAPE", schq);

		reg[k] = NIX_AF_MDQX_SW_XOFF(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ,
			 "NIX_AF_MDQ[%u]_SW_XOFF", schq);
		break;
	case NIX_TXSCH_LVL_TL4:
		reg[k] = NIX_AF_TL4X_PARENT(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ,
			 "NIX_AF_TL4[%u]_PARENT", schq);

		reg[k] = NIX_AF_TL4X_TOPOLOGY(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ,
			 "NIX_AF_TL4[%u]_TOPOLOGY", schq);

		reg[k] = NIX_AF_TL4X_SDP_LINK_CFG(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ,
			 "NIX_AF_TL4[%u]_SDP_LINK_CFG", schq);

		reg[k] = NIX_AF_TL4X_SCHEDULE(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ,
			 "NIX_AF_TL4[%u]_SCHEDULE", schq);

		reg[k] = NIX_AF_TL4X_PIR(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ,
			 "NIX_AF_TL4[%u]_PIR", schq);

		reg[k] = NIX_AF_TL4X_CIR(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ,
			 "NIX_AF_TL4[%u]_CIR", schq);

		reg[k] = NIX_AF_TL4X_SHAPE(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ,
			 "NIX_AF_TL4[%u]_SHAPE", schq);

		reg[k] = NIX_AF_TL4X_SW_XOFF(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ,
			 "NIX_AF_TL4[%u]_SW_XOFF", schq);
		break;
	case NIX_TXSCH_LVL_TL3:
		reg[k] = NIX_AF_TL3X_PARENT(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ,
			 "NIX_AF_TL3[%u]_PARENT", schq);

		reg[k] = NIX_AF_TL3X_TOPOLOGY(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ,
			 "NIX_AF_TL3[%u]_TOPOLOGY", schq);

		reg[k] = NIX_AF_TL3_TL2X_LINKX_CFG(schq, link);
		snprintf(regstr[k++], NIX_REG_NAME_SZ,
			 "NIX_AF_TL3_TL2[%u]_LINK[%u]_CFG", schq, link);

		reg[k] = NIX_AF_TL3X_SCHEDULE(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ,
			 "NIX_AF_TL3[%u]_SCHEDULE", schq);

		reg[k] = NIX_AF_TL3X_PIR(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ,
			 "NIX_AF_TL3[%u]_PIR", schq);

		reg[k] = NIX_AF_TL3X_CIR(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ,
			 "NIX_AF_TL3[%u]_CIR", schq);

		reg[k] = NIX_AF_TL3X_SHAPE(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ,
			 "NIX_AF_TL3[%u]_SHAPE", schq);

		reg[k] = NIX_AF_TL3X_SW_XOFF(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ,
			 "NIX_AF_TL3[%u]_SW_XOFF", schq);
		break;
	case NIX_TXSCH_LVL_TL2:
		reg[k] = NIX_AF_TL2X_PARENT(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ,
			 "NIX_AF_TL2[%u]_PARENT", schq);

		reg[k] = NIX_AF_TL2X_TOPOLOGY(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ,
			 "NIX_AF_TL2[%u]_TOPOLOGY", schq);

		reg[k] = NIX_AF_TL3_TL2X_LINKX_CFG(schq, link);
		snprintf(regstr[k++], NIX_REG_NAME_SZ,
			 "NIX_AF_TL3_TL2[%u]_LINK[%u]_CFG", schq, link);

		reg[k] = NIX_AF_TL2X_SCHEDULE(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ,
			 "NIX_AF_TL2[%u]_SCHEDULE", schq);

		reg[k] = NIX_AF_TL2X_PIR(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ,
			 "NIX_AF_TL2[%u]_PIR", schq);

		reg[k] = NIX_AF_TL2X_CIR(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ,
			 "NIX_AF_TL2[%u]_CIR", schq);

		reg[k] = NIX_AF_TL2X_SHAPE(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ,
			 "NIX_AF_TL2[%u]_SHAPE", schq);

		reg[k] = NIX_AF_TL2X_SW_XOFF(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ,
			 "NIX_AF_TL2[%u]_SW_XOFF", schq);
		break;
	case NIX_TXSCH_LVL_TL1:

		reg[k] = NIX_AF_TL1X_TOPOLOGY(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ,
			 "NIX_AF_TL1[%u]_TOPOLOGY", schq);

		reg[k] = NIX_AF_TL1X_SCHEDULE(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ,
			 "NIX_AF_TL1[%u]_SCHEDULE", schq);

		reg[k] = NIX_AF_TL1X_CIR(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ,
			 "NIX_AF_TL1[%u]_CIR", schq);

		reg[k] = NIX_AF_TL1X_SW_XOFF(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ,
			 "NIX_AF_TL1[%u]_SW_XOFF", schq);

		reg[k] = NIX_AF_TL1X_DROPPED_PACKETS(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ,
			 "NIX_AF_TL1[%u]_DROPPED_PACKETS", schq);
		break;
	default:
		break;
	}

	if (k > MAX_REGS_PER_MBOX_MSG) {
		nix_dump("\t!!!NIX TM Registers request overflow!!!");
		return 0;
	}
	return k;
}

/* Dump TM hierarchy and registers */
void
otx2_nix_tm_dump(struct otx2_eth_dev *dev)
{
	char regstr[MAX_REGS_PER_MBOX_MSG * 2][NIX_REG_NAME_SZ];
	struct otx2_nix_tm_node *tm_node, *root_node, *parent;
	uint64_t reg[MAX_REGS_PER_MBOX_MSG * 2];
	struct nix_txschq_config *req;
	const char *lvlstr, *parent_lvlstr;
	struct nix_txschq_config *rsp;
	uint32_t schq, parent_schq;
	int hw_lvl, j, k, rc;

	nix_dump("===TM hierarchy and registers dump of %s===",
		 dev->eth_dev->data->name);

	root_node = NULL;

	for (hw_lvl = 0; hw_lvl <= NIX_TXSCH_LVL_CNT; hw_lvl++) {
		TAILQ_FOREACH(tm_node, &dev->node_list, node) {
			if (tm_node->hw_lvl != hw_lvl)
				continue;

			parent = tm_node->parent;
			if (hw_lvl == NIX_TXSCH_LVL_CNT) {
				lvlstr = "SQ";
				schq = tm_node->id;
			} else {
				lvlstr = nix_hwlvl2str(tm_node->hw_lvl);
				schq = tm_node->hw_id;
			}

			if (parent) {
				parent_schq = parent->hw_id;
				parent_lvlstr =
					nix_hwlvl2str(parent->hw_lvl);
			} else if (tm_node->hw_lvl == NIX_TXSCH_LVL_TL1) {
				parent_schq = otx2_nix_get_link(dev);
				parent_lvlstr = "LINK";
			} else {
				parent_schq = tm_node->parent_hw_id;
				parent_lvlstr =
					nix_hwlvl2str(tm_node->hw_lvl + 1);
			}

			nix_dump("%s_%d->%s_%d", lvlstr, schq,
				 parent_lvlstr, parent_schq);

			if (!(tm_node->flags & NIX_TM_NODE_HWRES))
				continue;

			/* Need to dump TL1 when root is TL2 */
			if (tm_node->hw_lvl == dev->otx2_tm_root_lvl)
				root_node = tm_node;

			/* Dump registers only when HWRES is present */
			k = prepare_nix_tm_reg_dump(tm_node->hw_lvl, schq,
						    otx2_nix_get_link(dev), reg,
						    regstr);
			if (!k)
				continue;

			req = otx2_mbox_alloc_msg_nix_txschq_cfg(dev->mbox);
			req->read = 1;
			req->lvl = tm_node->hw_lvl;
			req->num_regs = k;
			otx2_mbox_memcpy(req->reg, reg, sizeof(uint64_t) * k);
			rc = otx2_mbox_process_msg(dev->mbox, (void **)&rsp);
			if (!rc) {
				for (j = 0; j < k; j++)
					nix_dump("\t%s=0x%016"PRIx64,
						 regstr[j], rsp->regval[j]);
			} else {
				nix_dump("\t!!!Failed to dump registers!!!");
			}
		}
		nix_dump("\n");
	}

	/* Dump TL1 node data when root level is TL2 */
	if (root_node && root_node->hw_lvl == NIX_TXSCH_LVL_TL2) {
		k = prepare_nix_tm_reg_dump(NIX_TXSCH_LVL_TL1,
					    root_node->parent_hw_id,
					    otx2_nix_get_link(dev),
					    reg, regstr);
		if (!k)
			return;


		req = otx2_mbox_alloc_msg_nix_txschq_cfg(dev->mbox);
		req->read = 1;
		req->lvl = NIX_TXSCH_LVL_TL1;
		req->num_regs = k;
		otx2_mbox_memcpy(req->reg, reg, sizeof(uint64_t) * k);
		rc = otx2_mbox_process_msg(dev->mbox, (void **)&rsp);
		if (!rc) {
			for (j = 0; j < k; j++)
				nix_dump("\t%s=0x%016"PRIx64,
					 regstr[j], rsp->regval[j]);
		} else {
			nix_dump("\t!!!Failed to dump registers!!!");
		}
	}

	otx2_nix_queues_ctx_dump(dev->eth_dev);
}
