/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"


#define nix_dump(file, fmt, ...) do {                                           \
	if ((file) == NULL)							\
		plt_dump(fmt, ##__VA_ARGS__);					\
	else                                                                    \
		fprintf(file, fmt "\n", ##__VA_ARGS__);                         \
} while (0)

#define NIX_REG_INFO(reg)                                                      \
	{                                                                      \
		reg, #reg                                                      \
	}
#define NIX_REG_NAME_SZ 48

#define nix_dump_no_nl(file, fmt, ...) do {                                     \
	if ((file) == NULL)                                                     \
		plt_dump_no_nl(fmt, ##__VA_ARGS__);				\
	else                                                                    \
		fprintf(file, fmt, ##__VA_ARGS__);                              \
} while (0)


struct nix_lf_reg_info {
	uint32_t offset;
	const char *name;
};

static const struct nix_lf_reg_info nix_lf_reg[] = {
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

static void
nix_bitmap_dump(struct plt_bitmap *bmp, FILE *file)
{
	uint32_t pos = 0, start_pos;
	uint64_t slab = 0;
	int i;

	plt_bitmap_scan_init(bmp);
	if (!plt_bitmap_scan(bmp, &pos, &slab))
		return;

	start_pos = pos;

	nix_dump_no_nl(file, "  \t\t[");
	do {
		if (!slab)
			break;
		i = 0;

		for (i = 0; i < 64; i++)
			if (slab & (1ULL << i))
				nix_dump_no_nl(file, "%d, ", i);

		if (!plt_bitmap_scan(bmp, &pos, &slab))
			break;
	} while (start_pos != pos);
	nix_dump_no_nl(file, " ]");
}

int
roc_nix_lf_get_reg_count(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	int reg_count;

	if (roc_nix == NULL)
		return NIX_ERR_PARAM;

	reg_count = PLT_DIM(nix_lf_reg);
	/* NIX_LF_TX_STATX */
	reg_count += nix->lf_tx_stats;
	/* NIX_LF_RX_STATX */
	reg_count += nix->lf_rx_stats;
	/* NIX_LF_QINTX_CNT*/
	reg_count += nix->qints;
	/* NIX_LF_QINTX_INT */
	reg_count += nix->qints;
	/* NIX_LF_QINTX_ENA_W1S */
	reg_count += nix->qints;
	/* NIX_LF_QINTX_ENA_W1C */
	reg_count += nix->qints;
	/* NIX_LF_CINTX_CNT */
	reg_count += nix->cints;
	/* NIX_LF_CINTX_WAIT */
	reg_count += nix->cints;
	/* NIX_LF_CINTX_INT */
	reg_count += nix->cints;
	/* NIX_LF_CINTX_INT_W1S */
	reg_count += nix->cints;
	/* NIX_LF_CINTX_ENA_W1S */
	reg_count += nix->cints;
	/* NIX_LF_CINTX_ENA_W1C */
	reg_count += nix->cints;

	return reg_count;
}

int
nix_lf_gen_reg_dump(uintptr_t nix_lf_base, uint64_t *data)
{
	FILE *file = NULL;
	bool dump_stdout;
	uint64_t reg;
	uint32_t i;

	dump_stdout = data ? 0 : 1;

	for (i = 0; i < PLT_DIM(nix_lf_reg); i++) {
		reg = plt_read64(nix_lf_base + nix_lf_reg[i].offset);
		if (dump_stdout && reg)
			nix_dump(file, "%32s = 0x%" PRIx64, nix_lf_reg[i].name, reg);
		if (data)
			*data++ = reg;
	}

	return i;
}

int
nix_lf_stat_reg_dump(uintptr_t nix_lf_base, uint64_t *data, uint8_t lf_tx_stats,
		     uint8_t lf_rx_stats)
{
	uint32_t i, count = 0;
	FILE *file = NULL;
	bool dump_stdout;
	uint64_t reg;

	dump_stdout = data ? 0 : 1;

	/* NIX_LF_TX_STATX */
	for (i = 0; i < lf_tx_stats; i++) {
		reg = plt_read64(nix_lf_base + NIX_LF_TX_STATX(i));
		if (dump_stdout && reg)
			nix_dump(file, "%32s_%d = 0x%" PRIx64, "NIX_LF_TX_STATX", i,
				 reg);
		if (data)
			*data++ = reg;
	}
	count += i;

	/* NIX_LF_RX_STATX */
	for (i = 0; i < lf_rx_stats; i++) {
		reg = plt_read64(nix_lf_base + NIX_LF_RX_STATX(i));
		if (dump_stdout && reg)
			nix_dump(file, "%32s_%d = 0x%" PRIx64, "NIX_LF_RX_STATX", i,
				 reg);
		if (data)
			*data++ = reg;
	}

	return count + i;
}

int
nix_lf_int_reg_dump(uintptr_t nix_lf_base, uint64_t *data, uint16_t qints,
		    uint16_t cints)
{
	uint32_t i, count = 0;
	FILE *file = NULL;
	bool dump_stdout;
	uint64_t reg;

	dump_stdout = data ? 0 : 1;

	/* NIX_LF_QINTX_CNT*/
	for (i = 0; i < qints; i++) {
		reg = plt_read64(nix_lf_base + NIX_LF_QINTX_CNT(i));
		if (dump_stdout && reg)
			nix_dump(file, "%32s_%d = 0x%" PRIx64, "NIX_LF_QINTX_CNT", i,
				 reg);
		if (data)
			*data++ = reg;
	}
	count += i;

	/* NIX_LF_QINTX_INT */
	for (i = 0; i < qints; i++) {
		reg = plt_read64(nix_lf_base + NIX_LF_QINTX_INT(i));
		if (dump_stdout && reg)
			nix_dump(file, "%32s_%d = 0x%" PRIx64, "NIX_LF_QINTX_INT", i,
				 reg);
		if (data)
			*data++ = reg;
	}
	count += i;

	/* NIX_LF_QINTX_ENA_W1S */
	for (i = 0; i < qints; i++) {
		reg = plt_read64(nix_lf_base + NIX_LF_QINTX_ENA_W1S(i));
		if (dump_stdout && reg)
			nix_dump(file, "%32s_%d = 0x%" PRIx64, "NIX_LF_QINTX_ENA_W1S",
				 i, reg);
		if (data)
			*data++ = reg;
	}
	count += i;

	/* NIX_LF_QINTX_ENA_W1C */
	for (i = 0; i < qints; i++) {
		reg = plt_read64(nix_lf_base + NIX_LF_QINTX_ENA_W1C(i));
		if (dump_stdout && reg)
			nix_dump(file, "%32s_%d = 0x%" PRIx64, "NIX_LF_QINTX_ENA_W1C",
				 i, reg);
		if (data)
			*data++ = reg;
	}
	count += i;

	/* NIX_LF_CINTX_CNT */
	for (i = 0; i < cints; i++) {
		reg = plt_read64(nix_lf_base + NIX_LF_CINTX_CNT(i));
		if (dump_stdout && reg)
			nix_dump(file, "%32s_%d = 0x%" PRIx64, "NIX_LF_CINTX_CNT", i,
				 reg);
		if (data)
			*data++ = reg;
	}
	count += i;

	/* NIX_LF_CINTX_WAIT */
	for (i = 0; i < cints; i++) {
		reg = plt_read64(nix_lf_base + NIX_LF_CINTX_WAIT(i));
		if (dump_stdout && reg)
			nix_dump(file, "%32s_%d = 0x%" PRIx64, "NIX_LF_CINTX_WAIT", i,
				 reg);
		if (data)
			*data++ = reg;
	}
	count += i;

	/* NIX_LF_CINTX_INT */
	for (i = 0; i < cints; i++) {
		reg = plt_read64(nix_lf_base + NIX_LF_CINTX_INT(i));
		if (dump_stdout && reg)
			nix_dump(file, "%32s_%d = 0x%" PRIx64, "NIX_LF_CINTX_INT", i,
				 reg);
		if (data)
			*data++ = reg;
	}
	count += i;

	/* NIX_LF_CINTX_INT_W1S */
	for (i = 0; i < cints; i++) {
		reg = plt_read64(nix_lf_base + NIX_LF_CINTX_INT_W1S(i));
		if (dump_stdout && reg)
			nix_dump(file, "%32s_%d = 0x%" PRIx64, "NIX_LF_CINTX_INT_W1S",
				 i, reg);
		if (data)
			*data++ = reg;
	}
	count += i;

	/* NIX_LF_CINTX_ENA_W1S */
	for (i = 0; i < cints; i++) {
		reg = plt_read64(nix_lf_base + NIX_LF_CINTX_ENA_W1S(i));
		if (dump_stdout && reg)
			nix_dump(file, "%32s_%d = 0x%" PRIx64, "NIX_LF_CINTX_ENA_W1S",
				 i, reg);
		if (data)
			*data++ = reg;
	}
	count += i;

	/* NIX_LF_CINTX_ENA_W1C */
	for (i = 0; i < cints; i++) {
		reg = plt_read64(nix_lf_base + NIX_LF_CINTX_ENA_W1C(i));
		if (dump_stdout && reg)
			nix_dump(file, "%32s_%d = 0x%" PRIx64, "NIX_LF_CINTX_ENA_W1C",
				 i, reg);
		if (data)
			*data++ = reg;
	}

	return count + i;
}

int
roc_nix_lf_reg_dump(struct roc_nix *roc_nix, uint64_t *data)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	bool dump_stdout = data ? 0 : 1;
	uintptr_t nix_base;
	uint32_t i;

	if (roc_nix == NULL)
		return NIX_ERR_PARAM;

	nix_base = nix->base;
	/* General registers */
	i = nix_lf_gen_reg_dump(nix_base, data);

	/* Rx, Tx stat registers */
	i += nix_lf_stat_reg_dump(nix_base, dump_stdout ? NULL : &data[i],
				  nix->lf_tx_stats, nix->lf_rx_stats);

	/* Intr registers */
	i += nix_lf_int_reg_dump(nix_base, dump_stdout ? NULL : &data[i],
				 nix->qints, nix->cints);

	return 0;
}

int
nix_q_ctx_get(struct dev *dev, uint8_t ctype, uint16_t qid, __io void **ctx_p)
{
	struct mbox *mbox = mbox_get(dev->mbox);
	int rc;

	if (roc_model_is_cn9k()) {
		struct nix_aq_enq_rsp *rsp;
		struct nix_aq_enq_req *aq;

		aq = mbox_alloc_msg_nix_aq_enq(mbox);
		if (!aq) {
			rc = -ENOSPC;
			goto exit;
		}

		aq->qidx = qid;
		aq->ctype = ctype;
		aq->op = NIX_AQ_INSTOP_READ;

		rc = mbox_process_msg(mbox, (void *)&rsp);
		if (rc)
			goto exit;
		if (ctype == NIX_AQ_CTYPE_RQ)
			*ctx_p = &rsp->rq;
		else if (ctype == NIX_AQ_CTYPE_SQ)
			*ctx_p = &rsp->sq;
		else
			*ctx_p = &rsp->cq;
	} else {
		struct nix_cn10k_aq_enq_rsp *rsp;
		struct nix_cn10k_aq_enq_req *aq;

		aq = mbox_alloc_msg_nix_cn10k_aq_enq(mbox);
		if (!aq) {
			rc = -ENOSPC;
			goto exit;
		}

		aq->qidx = qid;
		aq->ctype = ctype;
		aq->op = NIX_AQ_INSTOP_READ;

		rc = mbox_process_msg(mbox, (void *)&rsp);
		if (rc)
			goto exit;

		if (ctype == NIX_AQ_CTYPE_RQ)
			*ctx_p = &rsp->rq;
		else if (ctype == NIX_AQ_CTYPE_SQ)
			*ctx_p = &rsp->sq;
		else
			*ctx_p = &rsp->cq;
	}
	rc = 0;
exit:
	mbox_put(mbox);
	return rc;
}

static inline void
nix_cn9k_lf_sq_dump(__io struct nix_sq_ctx_s *ctx, uint32_t *sqb_aura_p, FILE *file)
{
	nix_dump(file, "W0: sqe_way_mask \t\t%d\nW0: cq \t\t\t\t%d",
		 ctx->sqe_way_mask, ctx->cq);
	nix_dump(file, "W0: sdp_mcast \t\t\t%d\nW0: substream \t\t\t0x%03x",
		 ctx->sdp_mcast, ctx->substream);
	nix_dump(file, "W0: qint_idx \t\t\t%d\nW0: ena \t\t\t%d\n", ctx->qint_idx,
		 ctx->ena);

	nix_dump(file, "W1: sqb_count \t\t\t%d\nW1: default_chan \t\t%d",
		 ctx->sqb_count, ctx->default_chan);
	nix_dump(file, "W1: smq_rr_quantum \t\t%d\nW1: sso_ena \t\t\t%d",
		 ctx->smq_rr_quantum, ctx->sso_ena);
	nix_dump(file, "W1: xoff \t\t\t%d\nW1: cq_ena \t\t\t%d\nW1: smq\t\t\t\t%d\n",
		 ctx->xoff, ctx->cq_ena, ctx->smq);

	nix_dump(file, "W2: sqe_stype \t\t\t%d\nW2: sq_int_ena \t\t\t%d",
		 ctx->sqe_stype, ctx->sq_int_ena);
	nix_dump(file, "W2: sq_int  \t\t\t%d\nW2: sqb_aura \t\t\t%d", ctx->sq_int,
		 ctx->sqb_aura);
	nix_dump(file, "W2: smq_rr_count \t\t%d\n", ctx->smq_rr_count);

	nix_dump(file, "W3: smq_next_sq_vld\t\t%d\nW3: smq_pend\t\t\t%d",
		 ctx->smq_next_sq_vld, ctx->smq_pend);
	nix_dump(file, "W3: smenq_next_sqb_vld  \t%d\nW3: head_offset\t\t\t%d",
		 ctx->smenq_next_sqb_vld, ctx->head_offset);
	nix_dump(file, "W3: smenq_offset\t\t%d\nW3: tail_offset \t\t%d",
		 ctx->smenq_offset, ctx->tail_offset);
	nix_dump(file, "W3: smq_lso_segnum \t\t%d\nW3: smq_next_sq \t\t%d",
		 ctx->smq_lso_segnum, ctx->smq_next_sq);
	nix_dump(file, "W3: mnq_dis \t\t\t%d\nW3: lmt_dis \t\t\t%d", ctx->mnq_dis,
		 ctx->lmt_dis);
	nix_dump(file, "W3: cq_limit\t\t\t%d\nW3: max_sqe_size\t\t%d\n",
		 ctx->cq_limit, ctx->max_sqe_size);

	nix_dump(file, "W4: next_sqb \t\t\t0x%" PRIx64 "", ctx->next_sqb);
	nix_dump(file, "W5: tail_sqb \t\t\t0x%" PRIx64 "", ctx->tail_sqb);
	nix_dump(file, "W6: smenq_sqb \t\t\t0x%" PRIx64 "", ctx->smenq_sqb);
	nix_dump(file, "W7: smenq_next_sqb \t\t0x%" PRIx64 "", ctx->smenq_next_sqb);
	nix_dump(file, "W8: head_sqb \t\t\t0x%" PRIx64 "", ctx->head_sqb);

	nix_dump(file, "W9: vfi_lso_vld \t\t%d\nW9: vfi_lso_vlan1_ins_ena\t%d",
		 ctx->vfi_lso_vld, ctx->vfi_lso_vlan1_ins_ena);
	nix_dump(file, "W9: vfi_lso_vlan0_ins_ena\t%d\nW9: vfi_lso_mps\t\t\t%d",
		 ctx->vfi_lso_vlan0_ins_ena, ctx->vfi_lso_mps);
	nix_dump(file, "W9: vfi_lso_sb \t\t\t%d\nW9: vfi_lso_sizem1\t\t%d",
		 ctx->vfi_lso_sb, ctx->vfi_lso_sizem1);
	nix_dump(file, "W9: vfi_lso_total\t\t%d", ctx->vfi_lso_total);

	nix_dump(file, "W10: scm_lso_rem \t\t0x%" PRIx64 "",
		 (uint64_t)ctx->scm_lso_rem);
	nix_dump(file, "W11: octs \t\t\t0x%" PRIx64 "", (uint64_t)ctx->octs);
	nix_dump(file, "W12: pkts \t\t\t0x%" PRIx64 "", (uint64_t)ctx->pkts);
	nix_dump(file, "W14: dropped_octs \t\t0x%" PRIx64 "",
		 (uint64_t)ctx->drop_octs);
	nix_dump(file, "W15: dropped_pkts \t\t0x%" PRIx64 "",
		 (uint64_t)ctx->drop_pkts);

	*sqb_aura_p = ctx->sqb_aura;
}

static inline void
nix_lf_sq_dump(__io struct nix_cn10k_sq_ctx_s *ctx, uint32_t *sqb_aura_p, FILE *file)
{
	nix_dump(file, "W0: sqe_way_mask \t\t%d\nW0: cq \t\t\t\t%d",
		 ctx->sqe_way_mask, ctx->cq);
	nix_dump(file, "W0: sdp_mcast \t\t\t%d\nW0: substream \t\t\t0x%03x",
		 ctx->sdp_mcast, ctx->substream);
	nix_dump(file, "W0: qint_idx \t\t\t%d\nW0: ena \t\t\t%d\n", ctx->qint_idx,
		 ctx->ena);

	nix_dump(file, "W1: sqb_count \t\t\t%d\nW1: default_chan \t\t%d",
		 ctx->sqb_count, ctx->default_chan);
	nix_dump(file, "W1: smq_rr_weight \t\t%d\nW1: sso_ena \t\t\t%d",
		 ctx->smq_rr_weight, ctx->sso_ena);
	nix_dump(file, "W1: xoff \t\t\t%d\nW1: cq_ena \t\t\t%d\nW1: smq\t\t\t\t%d\n",
		 ctx->xoff, ctx->cq_ena, ctx->smq);

	nix_dump(file, "W2: sqe_stype \t\t\t%d\nW2: sq_int_ena \t\t\t%d",
		 ctx->sqe_stype, ctx->sq_int_ena);
	nix_dump(file, "W2: sq_int  \t\t\t%d\nW2: sqb_aura \t\t\t%d", ctx->sq_int,
		 ctx->sqb_aura);
	nix_dump(file, "W2: smq_rr_count[ub:lb] \t\t%x:%x\n", ctx->smq_rr_count_ub,
		 ctx->smq_rr_count_lb);

	nix_dump(file, "W3: smq_next_sq_vld\t\t%d\nW3: smq_pend\t\t\t%d",
		 ctx->smq_next_sq_vld, ctx->smq_pend);
	nix_dump(file, "W3: smenq_next_sqb_vld  \t%d\nW3: head_offset\t\t\t%d",
		 ctx->smenq_next_sqb_vld, ctx->head_offset);
	nix_dump(file, "W3: smenq_offset\t\t%d\nW3: tail_offset \t\t%d",
		 ctx->smenq_offset, ctx->tail_offset);
	nix_dump(file, "W3: smq_lso_segnum \t\t%d\nW3: smq_next_sq \t\t%d",
		 ctx->smq_lso_segnum, ctx->smq_next_sq);
	nix_dump(file, "W3: mnq_dis \t\t\t%d\nW3: lmt_dis \t\t\t%d", ctx->mnq_dis,
		 ctx->lmt_dis);
	nix_dump(file, "W3: cq_limit\t\t\t%d\nW3: max_sqe_size\t\t%d\n",
		 ctx->cq_limit, ctx->max_sqe_size);

	nix_dump(file, "W4: next_sqb \t\t\t0x%" PRIx64 "", ctx->next_sqb);
	nix_dump(file, "W5: tail_sqb \t\t\t0x%" PRIx64 "", ctx->tail_sqb);
	nix_dump(file, "W6: smenq_sqb \t\t\t0x%" PRIx64 "", ctx->smenq_sqb);
	nix_dump(file, "W7: smenq_next_sqb \t\t0x%" PRIx64 "", ctx->smenq_next_sqb);
	nix_dump(file, "W8: head_sqb \t\t\t0x%" PRIx64 "", ctx->head_sqb);

	nix_dump(file, "W9: vfi_lso_vld \t\t%d\nW9: vfi_lso_vlan1_ins_ena\t%d", ctx->vfi_lso_vld,
		 ctx->vfi_lso_vlan1_ins_ena);
	nix_dump(file, "W9: vfi_lso_vlan0_ins_ena\t%d\nW9: vfi_lso_mps\t\t\t%d",
		 ctx->vfi_lso_vlan0_ins_ena, ctx->vfi_lso_mps);
	nix_dump(file, "W9: vfi_lso_sb \t\t\t%d\nW9: vfi_lso_sizem1\t\t%d", ctx->vfi_lso_sb,
		 ctx->vfi_lso_sizem1);
	nix_dump(file, "W9: vfi_lso_total\t\t%d", ctx->vfi_lso_total);

	nix_dump(file, "W10: scm_lso_rem \t\t0x%" PRIx64 "", (uint64_t)ctx->scm_lso_rem);
	nix_dump(file, "W11: octs \t\t\t0x%" PRIx64 "", (uint64_t)ctx->octs);
	nix_dump(file, "W12: pkts \t\t\t0x%" PRIx64 "", (uint64_t)ctx->pkts);
	nix_dump(file, "W13: aged_drop_pkts \t\t\t0x%" PRIx64 "", (uint64_t)ctx->aged_drop_pkts);
	nix_dump(file, "W13: aged_drop_octs \t\t\t0x%" PRIx64 "", (uint64_t)ctx->aged_drop_octs);
	nix_dump(file, "W14: dropped_octs \t\t0x%" PRIx64 "", (uint64_t)ctx->drop_octs);
	nix_dump(file, "W15: dropped_pkts \t\t0x%" PRIx64 "", (uint64_t)ctx->drop_pkts);

	*sqb_aura_p = ctx->sqb_aura;
}

static inline void
nix_cn9k_lf_rq_dump(__io struct nix_rq_ctx_s *ctx, FILE *file)
{
	nix_dump(file, "W0: wqe_aura \t\t\t%d\nW0: substream \t\t\t0x%03x",
		 ctx->wqe_aura, ctx->substream);
	nix_dump(file, "W0: cq \t\t\t\t%d\nW0: ena_wqwd \t\t\t%d", ctx->cq,
		 ctx->ena_wqwd);
	nix_dump(file, "W0: ipsech_ena \t\t\t%d\nW0: sso_ena \t\t\t%d",
		 ctx->ipsech_ena, ctx->sso_ena);
	nix_dump(file, "W0: ena \t\t\t%d\n", ctx->ena);

	nix_dump(file, "W1: lpb_drop_ena \t\t%d\nW1: spb_drop_ena \t\t%d",
		 ctx->lpb_drop_ena, ctx->spb_drop_ena);
	nix_dump(file, "W1: xqe_drop_ena \t\t%d\nW1: wqe_caching \t\t%d",
		 ctx->xqe_drop_ena, ctx->wqe_caching);
	nix_dump(file, "W1: pb_caching \t\t\t%d\nW1: sso_tt \t\t\t%d",
		 ctx->pb_caching, ctx->sso_tt);
	nix_dump(file, "W1: sso_grp \t\t\t%d\nW1: lpb_aura \t\t\t%d", ctx->sso_grp,
		 ctx->lpb_aura);
	nix_dump(file, "W1: spb_aura \t\t\t%d\n", ctx->spb_aura);

	nix_dump(file, "W2: xqe_hdr_split \t\t%d\nW2: xqe_imm_copy \t\t%d",
		 ctx->xqe_hdr_split, ctx->xqe_imm_copy);
	nix_dump(file, "W2: xqe_imm_size \t\t%d\nW2: later_skip \t\t\t%d",
		 ctx->xqe_imm_size, ctx->later_skip);
	nix_dump(file, "W2: first_skip \t\t\t%d\nW2: lpb_sizem1 \t\t\t%d",
		 ctx->first_skip, ctx->lpb_sizem1);
	nix_dump(file, "W2: spb_ena \t\t\t%d\nW2: wqe_skip \t\t\t%d", ctx->spb_ena,
		 ctx->wqe_skip);
	nix_dump(file, "W2: spb_sizem1 \t\t\t%d\n", ctx->spb_sizem1);

	nix_dump(file, "W3: spb_pool_pass \t\t%d\nW3: spb_pool_drop \t\t%d",
		 ctx->spb_pool_pass, ctx->spb_pool_drop);
	nix_dump(file, "W3: spb_aura_pass \t\t%d\nW3: spb_aura_drop \t\t%d",
		 ctx->spb_aura_pass, ctx->spb_aura_drop);
	nix_dump(file, "W3: wqe_pool_pass \t\t%d\nW3: wqe_pool_drop \t\t%d",
		 ctx->wqe_pool_pass, ctx->wqe_pool_drop);
	nix_dump(file, "W3: xqe_pass \t\t\t%d\nW3: xqe_drop \t\t\t%d\n",
		 ctx->xqe_pass, ctx->xqe_drop);

	nix_dump(file, "W4: qint_idx \t\t\t%d\nW4: rq_int_ena \t\t\t%d",
		 ctx->qint_idx, ctx->rq_int_ena);
	nix_dump(file, "W4: rq_int \t\t\t%d\nW4: lpb_pool_pass \t\t%d", ctx->rq_int,
		 ctx->lpb_pool_pass);
	nix_dump(file, "W4: lpb_pool_drop \t\t%d\nW4: lpb_aura_pass \t\t%d",
		 ctx->lpb_pool_drop, ctx->lpb_aura_pass);
	nix_dump(file, "W4: lpb_aura_drop \t\t%d\n", ctx->lpb_aura_drop);

	nix_dump(file, "W5: flow_tagw \t\t\t%d\nW5: bad_utag \t\t\t%d",
		 ctx->flow_tagw, ctx->bad_utag);
	nix_dump(file, "W5: good_utag \t\t\t%d\nW5: ltag \t\t\t%d\n", ctx->good_utag,
		 ctx->ltag);

	nix_dump(file, "W6: octs \t\t\t0x%" PRIx64 "", (uint64_t)ctx->octs);
	nix_dump(file, "W7: pkts \t\t\t0x%" PRIx64 "", (uint64_t)ctx->pkts);
	nix_dump(file, "W8: drop_octs \t\t\t0x%" PRIx64 "", (uint64_t)ctx->drop_octs);
	nix_dump(file, "W9: drop_pkts \t\t\t0x%" PRIx64 "", (uint64_t)ctx->drop_pkts);
	nix_dump(file, "W10: re_pkts \t\t\t0x%" PRIx64 "\n", (uint64_t)ctx->re_pkts);
}

void
nix_lf_rq_dump(__io struct nix_cn10k_rq_ctx_s *ctx, FILE *file)
{
	nix_dump(file, "W0: wqe_aura \t\t\t%d\nW0: len_ol3_dis \t\t\t%d",
		 ctx->wqe_aura, ctx->len_ol3_dis);
	nix_dump(file, "W0: len_ol4_dis \t\t\t%d\nW0: len_il3_dis \t\t\t%d",
		 ctx->len_ol4_dis, ctx->len_il3_dis);
	nix_dump(file, "W0: len_il4_dis \t\t\t%d\nW0: csum_ol4_dis \t\t\t%d",
		 ctx->len_il4_dis, ctx->csum_ol4_dis);
	nix_dump(file, "W0: csum_ol3_dis \t\t\t%d\nW0: lenerr_dis \t\t\t%d",
		 ctx->csum_ol4_dis, ctx->lenerr_dis);
	nix_dump(file, "W0: cq \t\t\t\t%d\nW0: ena_wqwd \t\t\t%d", ctx->cq,
		 ctx->ena_wqwd);
	nix_dump(file, "W0: ipsech_ena \t\t\t%d\nW0: sso_ena \t\t\t%d",
		 ctx->ipsech_ena, ctx->sso_ena);
	nix_dump(file, "W0: ena \t\t\t%d\n", ctx->ena);

	nix_dump(file, "W1: chi_ena \t\t%d\nW1: ipsecd_drop_en \t\t%d", ctx->chi_ena,
		 ctx->ipsecd_drop_en);
	nix_dump(file, "W1: pb_stashing \t\t\t%d", ctx->pb_stashing);
	nix_dump(file, "W1: lpb_drop_ena \t\t%d\nW1: spb_drop_ena \t\t%d",
		 ctx->lpb_drop_ena, ctx->spb_drop_ena);
	nix_dump(file, "W1: xqe_drop_ena \t\t%d\nW1: wqe_caching \t\t%d",
		 ctx->xqe_drop_ena, ctx->wqe_caching);
	nix_dump(file, "W1: pb_caching \t\t\t%d\nW1: sso_tt \t\t\t%d",
		 ctx->pb_caching, ctx->sso_tt);
	nix_dump(file, "W1: sso_grp \t\t\t%d\nW1: lpb_aura \t\t\t%d", ctx->sso_grp,
		 ctx->lpb_aura);
	nix_dump(file, "W1: spb_aura \t\t\t%d\n", ctx->spb_aura);

	nix_dump(file, "W2: xqe_hdr_split \t\t%d\nW2: xqe_imm_copy \t\t%d",
		 ctx->xqe_hdr_split, ctx->xqe_imm_copy);
	nix_dump(file, "W2: xqe_imm_size \t\t%d\nW2: later_skip \t\t\t%d",
		 ctx->xqe_imm_size, ctx->later_skip);
	nix_dump(file, "W2: first_skip \t\t\t%d\nW2: lpb_sizem1 \t\t\t%d",
		 ctx->first_skip, ctx->lpb_sizem1);
	nix_dump(file, "W2: spb_ena \t\t\t%d\nW2: wqe_skip \t\t\t%d", ctx->spb_ena,
		 ctx->wqe_skip);
	nix_dump(file, "W2: spb_sizem1 \t\t\t%d\nW2: policer_ena \t\t\t%d",
		 ctx->spb_sizem1, ctx->policer_ena);
	nix_dump(file, "W2: band_prof_id \t\t\t%d", ctx->band_prof_id);

	nix_dump(file, "W3: spb_pool_pass \t\t%d\nW3: spb_pool_drop \t\t%d",
		 ctx->spb_pool_pass, ctx->spb_pool_drop);
	nix_dump(file, "W3: spb_aura_pass \t\t%d\nW3: spb_aura_drop \t\t%d",
		 ctx->spb_aura_pass, ctx->spb_aura_drop);
	nix_dump(file, "W3: wqe_pool_pass \t\t%d\nW3: wqe_pool_drop \t\t%d",
		 ctx->wqe_pool_pass, ctx->wqe_pool_drop);
	nix_dump(file, "W3: xqe_pass \t\t\t%d\nW3: xqe_drop \t\t\t%d\n",
		 ctx->xqe_pass, ctx->xqe_drop);

	nix_dump(file, "W4: qint_idx \t\t\t%d\nW4: rq_int_ena \t\t\t%d",
		 ctx->qint_idx, ctx->rq_int_ena);
	nix_dump(file, "W4: rq_int \t\t\t%d\nW4: lpb_pool_pass \t\t%d", ctx->rq_int,
		 ctx->lpb_pool_pass);
	nix_dump(file, "W4: lpb_pool_drop \t\t%d\nW4: lpb_aura_pass \t\t%d",
		 ctx->lpb_pool_drop, ctx->lpb_aura_pass);
	nix_dump(file, "W4: lpb_aura_drop \t\t%d\n", ctx->lpb_aura_drop);

	nix_dump(file, "W5: vwqe_skip \t\t\t%d\nW5: max_vsize_exp \t\t\t%d",
		 ctx->vwqe_skip, ctx->max_vsize_exp);
	nix_dump(file, "W5: vtime_wait \t\t\t%d\nW5: vwqe_ena \t\t\t%d",
		 ctx->vtime_wait, ctx->max_vsize_exp);
	nix_dump(file, "W5: ipsec_vwqe \t\t\t%d", ctx->ipsec_vwqe);
	nix_dump(file, "W5: flow_tagw \t\t\t%d\nW5: bad_utag \t\t\t%d",
		 ctx->flow_tagw, ctx->bad_utag);
	nix_dump(file, "W5: good_utag \t\t\t%d\nW5: ltag \t\t\t%d\n", ctx->good_utag,
		 ctx->ltag);

	nix_dump(file, "W6: octs \t\t\t0x%" PRIx64 "", (uint64_t)ctx->octs);
	nix_dump(file, "W7: pkts \t\t\t0x%" PRIx64 "", (uint64_t)ctx->pkts);
	nix_dump(file, "W8: drop_octs \t\t\t0x%" PRIx64 "", (uint64_t)ctx->drop_octs);
	nix_dump(file, "W9: drop_pkts \t\t\t0x%" PRIx64 "", (uint64_t)ctx->drop_pkts);
	nix_dump(file, "W10: re_pkts \t\t\t0x%" PRIx64 "\n", (uint64_t)ctx->re_pkts);
}

static inline void
nix_lf_cq_dump(__io struct nix_cq_ctx_s *ctx, FILE *file)
{
	nix_dump(file, "W0: base \t\t\t0x%" PRIx64 "\n", ctx->base);

	nix_dump(file, "W1: wrptr \t\t\t%" PRIx64 "", (uint64_t)ctx->wrptr);
	nix_dump(file, "W1: avg_con \t\t\t%d\nW1: cint_idx \t\t\t%d", ctx->avg_con,
		 ctx->cint_idx);
	nix_dump(file, "W1: cq_err \t\t\t%d\nW1: qint_idx \t\t\t%d", ctx->cq_err,
		 ctx->qint_idx);
	nix_dump(file, "W1: bpid  \t\t\t%d\nW1: bp_ena \t\t\t%d\n", ctx->bpid,
		 ctx->bp_ena);
	nix_dump(file,
		 "W1: lbpid_high \t\t\t0x%03x\nW1: lbpid_med \t\t\t0x%03x\n"
		 "W1: lbpid_low \t\t\t0x%03x\n(W1: lbpid) \t\t\t0x%03x\n",
		 ctx->lbpid_high, ctx->lbpid_med, ctx->lbpid_low,
		 ctx->lbpid_high << 6 | ctx->lbpid_med << 3 | ctx->lbpid_low);
	nix_dump(file, "W1: lbp_ena \t\t\t\t%d\n", ctx->lbp_ena);

	nix_dump(file, "W2: update_time \t\t%d\nW2: avg_level \t\t\t%d",
		 ctx->update_time, ctx->avg_level);
	nix_dump(file, "W2: head \t\t\t%d\nW2: tail \t\t\t%d\n", ctx->head,
		 ctx->tail);

	nix_dump(file, "W3: cq_err_int_ena \t\t%d\nW3: cq_err_int \t\t\t%d",
		 ctx->cq_err_int_ena, ctx->cq_err_int);
	nix_dump(file, "W3: qsize \t\t\t%d\nW3: caching \t\t\t%d", ctx->qsize,
		 ctx->caching);
	nix_dump(file, "W3: lbp_frac \t\t\t%d\n", ctx->lbp_frac);
	nix_dump(file, "W3: substream \t\t\t0x%03x\nW3: cpt_drop_err_en \t\t\t%d\n",
		 ctx->substream, ctx->cpt_drop_err_en);
	nix_dump(file, "W3: ena \t\t\t%d\n", ctx->ena);
	nix_dump(file, "W3: drop_ena \t\t\t%d\nW3: drop \t\t\t%d", ctx->drop_ena,
		 ctx->drop);
	nix_dump(file, "W3: bp \t\t\t\t%d\n", ctx->bp);
}

int
roc_nix_queues_ctx_dump(struct roc_nix *roc_nix, FILE *file)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	int rc = -1, q, rq = nix->nb_rx_queues;
	struct npa_aq_enq_rsp *npa_rsp;
	struct npa_aq_enq_req *npa_aq;
	struct dev *dev = &nix->dev;
	int sq = nix->nb_tx_queues;
	struct roc_nix_rq *inl_rq;
	struct npa_lf *npa_lf;
	volatile void *ctx;
	uint32_t sqb_aura;

	npa_lf = idev_npa_obj_get();
	if (npa_lf == NULL)
		return NPA_ERR_DEVICE_NOT_BOUNDED;

	for (q = 0; q < rq; q++) {
		rc = nix_q_ctx_get(dev, NIX_AQ_CTYPE_CQ, q, &ctx);
		if (rc) {
			plt_err("Failed to get cq context");
			goto fail;
		}
		nix_dump(file, "============== port=%d cq=%d ===============",
			 roc_nix->port_id, q);
		nix_lf_cq_dump(ctx, file);
	}

	for (q = 0; q < rq; q++) {
		rc = nix_q_ctx_get(dev, NIX_AQ_CTYPE_RQ, q, &ctx);
		if (rc) {
			plt_err("Failed to get rq context");
			goto fail;
		}
		nix_dump(file, "============== port=%d rq=%d ===============",
			 roc_nix->port_id, q);
		if (roc_model_is_cn9k())
			nix_cn9k_lf_rq_dump(ctx, file);
		else
			nix_lf_rq_dump(ctx, file);
	}

	/* Dump inline dev RQ for this port */
	inl_rq = roc_nix_inl_dev_rq(roc_nix);
	if (inl_rq) {
		struct idev_cfg *idev = idev_get_cfg();
		struct nix_inl_dev *inl_dev = idev->nix_inl_dev;

		rc = nix_q_ctx_get(&inl_dev->dev, NIX_AQ_CTYPE_RQ, inl_rq->qid, &ctx);
		if (rc) {
			plt_err("Failed to get rq context");
			goto fail;
		}
		nix_dump(file, "============== port=%d inl_rq=%d ===============", roc_nix->port_id,
			 inl_rq->qid);
		if (roc_model_is_cn9k())
			nix_cn9k_lf_rq_dump(ctx, file);
		else
			nix_lf_rq_dump(ctx, file);
	}

	for (q = 0; q < sq; q++) {
		rc = nix_q_ctx_get(dev, NIX_AQ_CTYPE_SQ, q, &ctx);
		if (rc) {
			plt_err("Failed to get sq context");
			goto fail;
		}
		nix_dump(file, "============== port=%d sq=%d ===============",
			 roc_nix->port_id, q);
		if (roc_model_is_cn9k())
			nix_cn9k_lf_sq_dump(ctx, &sqb_aura, file);
		else
			nix_lf_sq_dump(ctx, &sqb_aura, file);

		if (!npa_lf) {
			plt_err("NPA LF does not exist");
			continue;
		}

		/* Dump SQB Aura minimal info */
		npa_aq = mbox_alloc_msg_npa_aq_enq(mbox_get(npa_lf->mbox));
		if (npa_aq == NULL) {
			rc = -ENOSPC;
			mbox_put(npa_lf->mbox);
			goto fail;
		}
		npa_aq->aura_id = sqb_aura;
		npa_aq->ctype = NPA_AQ_CTYPE_AURA;
		npa_aq->op = NPA_AQ_INSTOP_READ;

		rc = mbox_process_msg(npa_lf->mbox, (void *)&npa_rsp);
		mbox_put(npa_lf->mbox);
		if (rc) {
			plt_err("Failed to get sq's sqb_aura context");
			continue;
		}

		nix_dump(file, "\nSQB Aura W0: Pool addr\t\t0x%" PRIx64 "",
			 npa_rsp->aura.pool_addr);
		nix_dump(file, "SQB Aura W1: ena\t\t\t%d", npa_rsp->aura.ena);
		nix_dump(file, "SQB Aura W2: count\t\t%" PRIx64 "",
			 (uint64_t)npa_rsp->aura.count);
		nix_dump(file, "SQB Aura W3: limit\t\t%" PRIx64 "",
			 (uint64_t)npa_rsp->aura.limit);
		nix_dump(file, "SQB Aura W3: fc_ena\t\t%d", npa_rsp->aura.fc_ena);
		nix_dump(file, "SQB Aura W4: fc_addr\t\t0x%" PRIx64 "\n",
			 npa_rsp->aura.fc_addr);
	}

fail:
	return rc;
}

/* Dumps struct nix_cqe_hdr_s and union nix_rx_parse_u */
void
roc_nix_cqe_dump(const struct nix_cqe_hdr_s *cq)
{
	FILE *file = NULL;
	const union nix_rx_parse_u *rx =
		(const union nix_rx_parse_u *)((const uint64_t *)cq + 1);
	const uint64_t *sgs = (const uint64_t *)(rx + 1);
	int i;

	nix_dump(file, "tag \t\t0x%x\tq \t\t%d\t\tnode \t\t%d\tcqe_type \t%d",
		 cq->tag, cq->q, cq->node, cq->cqe_type);

	nix_dump(file, "W0: chan \t0x%x\t\tdesc_sizem1 \t%d", rx->chan,
		 rx->desc_sizem1);
	nix_dump(file, "W0: imm_copy \t%d\t\texpress \t%d", rx->imm_copy,
		 rx->express);
	nix_dump(file, "W0: wqwd \t%d\t\terrlev \t\t%d\t\terrcode \t%d", rx->wqwd,
		 rx->errlev, rx->errcode);
	nix_dump(file, "W0: latype \t%d\t\tlbtype \t\t%d\t\tlctype \t\t%d",
		 rx->latype, rx->lbtype, rx->lctype);
	nix_dump(file, "W0: ldtype \t%d\t\tletype \t\t%d\t\tlftype \t\t%d",
		 rx->ldtype, rx->letype, rx->lftype);
	nix_dump(file, "W0: lgtype \t%d \t\tlhtype \t\t%d", rx->lgtype, rx->lhtype);

	nix_dump(file, "W1: pkt_lenm1 \t%d", rx->pkt_lenm1);
	nix_dump(file, "W1: l2m \t%d\t\tl2b \t\t%d\t\tl3m \t\t%d\tl3b \t\t%d",
		 rx->l2m, rx->l2b, rx->l3m, rx->l3b);
	nix_dump(file, "W1: vtag0_valid %d\t\tvtag0_gone \t%d", rx->vtag0_valid,
		 rx->vtag0_gone);
	nix_dump(file, "W1: vtag1_valid %d\t\tvtag1_gone \t%d", rx->vtag1_valid,
		 rx->vtag1_gone);
	nix_dump(file, "W1: pkind \t%d", rx->pkind);
	nix_dump(file, "W1: vtag0_tci \t%d\t\tvtag1_tci \t%d", rx->vtag0_tci,
		 rx->vtag1_tci);

	nix_dump(file, "W2: laflags \t%d\t\tlbflags\t\t%d\t\tlcflags \t%d",
		 rx->laflags, rx->lbflags, rx->lcflags);
	nix_dump(file, "W2: ldflags \t%d\t\tleflags\t\t%d\t\tlfflags \t%d",
		 rx->ldflags, rx->leflags, rx->lfflags);
	nix_dump(file, "W2: lgflags \t%d\t\tlhflags \t%d", rx->lgflags, rx->lhflags);

	nix_dump(file, "W3: eoh_ptr \t%d\t\twqe_aura \t%d\t\tpb_aura \t%d",
		 rx->eoh_ptr, rx->wqe_aura, rx->pb_aura);
	nix_dump(file, "W3: match_id \t%d", rx->match_id);

	nix_dump(file, "W4: laptr \t%d\t\tlbptr \t\t%d\t\tlcptr \t\t%d", rx->laptr,
		 rx->lbptr, rx->lcptr);
	nix_dump(file, "W4: ldptr \t%d\t\tleptr \t\t%d\t\tlfptr \t\t%d", rx->ldptr,
		 rx->leptr, rx->lfptr);
	nix_dump(file, "W4: lgptr \t%d\t\tlhptr \t\t%d", rx->lgptr, rx->lhptr);

	nix_dump(file, "W5: vtag0_ptr \t%d\t\tvtag1_ptr \t%d\t\tflow_key_alg \t%d",
		 rx->vtag0_ptr, rx->vtag1_ptr, rx->flow_key_alg);

	for (i = 0; i < (rx->desc_sizem1 + 1) << 1; i++)
		nix_dump(file, "sg[%u] = %p", i, (void *)sgs[i]);
}

void
roc_nix_rq_dump(struct roc_nix_rq *rq, FILE *file)
{
	nix_dump(file, "nix_rq@%p", rq);
	nix_dump(file, "  qid = %d", rq->qid);
	nix_dump(file, "  aura_handle = 0x%" PRIx64 "", rq->aura_handle);
	nix_dump(file, "  ipsec_ena = %d", rq->ipsech_ena);
	nix_dump(file, "  first_skip = %d", rq->first_skip);
	nix_dump(file, "  later_skip = %d", rq->later_skip);
	nix_dump(file, "  lpb_size = %d", rq->lpb_size);
	nix_dump(file, "  sso_ena = %d", rq->sso_ena);
	nix_dump(file, "  tag_mask = %d", rq->tag_mask);
	nix_dump(file, "  flow_tag_width = %d", rq->flow_tag_width);
	nix_dump(file, "  tt = %d", rq->tt);
	nix_dump(file, "  hwgrp = %d", rq->hwgrp);
	nix_dump(file, "  vwqe_ena = %d", rq->vwqe_ena);
	nix_dump(file, "  vwqe_first_skip = %d", rq->vwqe_first_skip);
	nix_dump(file, "  vwqe_max_sz_exp = %d", rq->vwqe_max_sz_exp);
	nix_dump(file, "  vwqe_wait_tmo = %ld", rq->vwqe_wait_tmo);
	nix_dump(file, "  vwqe_aura_handle = %ld", rq->vwqe_aura_handle);
	nix_dump(file, "  roc_nix = %p", rq->roc_nix);
	nix_dump(file, "  inl_dev_refs = %d", rq->inl_dev_refs);
	nix_dump(file, "  tc = %d", rq->tc);
}

void
roc_nix_cq_dump(struct roc_nix_cq *cq, FILE *file)
{
	nix_dump(file, "nix_cq@%p", cq);
	nix_dump(file, "  qid = %d", cq->qid);
	nix_dump(file, "  qnb_desc = %d", cq->nb_desc);
	nix_dump(file, "  roc_nix = %p", cq->roc_nix);
	nix_dump(file, "  door = 0x%" PRIx64 "", cq->door);
	nix_dump(file, "  status = %p", cq->status);
	nix_dump(file, "  wdata = 0x%" PRIx64 "", cq->wdata);
	nix_dump(file, "  desc_base = %p", cq->desc_base);
	nix_dump(file, "  qmask = 0x%" PRIx32 "", cq->qmask);
}

void
roc_nix_sq_dump(struct roc_nix_sq *sq, FILE *file)
{
	nix_dump(file, "nix_sq@%p", sq);
	nix_dump(file, "  qid = %d", sq->qid);
	nix_dump(file, "  max_sqe_sz = %d", sq->max_sqe_sz);
	nix_dump(file, "  nb_desc = %d", sq->nb_desc);
	nix_dump(file, "  sqes_per_sqb_log2 = %d", sq->sqes_per_sqb_log2);
	nix_dump(file, "  roc_nix= %p", sq->roc_nix);
	nix_dump(file, "  aura_handle = 0x%" PRIx64 "", sq->aura_handle);
	nix_dump(file, "  nb_sqb_bufs_adj = %d", sq->nb_sqb_bufs_adj);
	nix_dump(file, "  nb_sqb_bufs = %d", sq->nb_sqb_bufs);
	nix_dump(file, "  io_addr = 0x%" PRIx64 "", sq->io_addr);
	nix_dump(file, "  lmt_addr = %p", sq->lmt_addr);
	nix_dump(file, "  sqe_mem = %p", sq->sqe_mem);
	nix_dump(file, "  fc = %p", sq->fc);
	nix_dump(file, "  tc = %d", sq->tc);
};

static uint8_t
nix_tm_reg_dump_prep(uint16_t hw_lvl, uint16_t schq, uint16_t link,
		     uint64_t *reg, char regstr[][NIX_REG_NAME_SZ])
{
	FILE *file = NULL;
	uint8_t k = 0;

	switch (hw_lvl) {
	case NIX_TXSCH_LVL_SMQ:
		reg[k] = NIX_AF_SMQX_CFG(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ, "NIX_AF_SMQ[%u]_CFG",
			 schq);

		reg[k] = NIX_AF_MDQX_PARENT(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ, "NIX_AF_MDQ[%u]_PARENT",
			 schq);

		reg[k] = NIX_AF_MDQX_SCHEDULE(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ,
			 "NIX_AF_MDQ[%u]_SCHEDULE", schq);

		reg[k] = NIX_AF_MDQX_PIR(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ, "NIX_AF_MDQ[%u]_PIR",
			 schq);

		reg[k] = NIX_AF_MDQX_CIR(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ, "NIX_AF_MDQ[%u]_CIR",
			 schq);

		reg[k] = NIX_AF_MDQX_SHAPE(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ, "NIX_AF_MDQ[%u]_SHAPE",
			 schq);

		reg[k] = NIX_AF_MDQX_SW_XOFF(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ, "NIX_AF_MDQ[%u]_SW_XOFF",
			 schq);
		break;
	case NIX_TXSCH_LVL_TL4:
		reg[k] = NIX_AF_TL4X_PARENT(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ, "NIX_AF_TL4[%u]_PARENT",
			 schq);

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
		snprintf(regstr[k++], NIX_REG_NAME_SZ, "NIX_AF_TL4[%u]_PIR",
			 schq);

		reg[k] = NIX_AF_TL4X_CIR(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ, "NIX_AF_TL4[%u]_CIR",
			 schq);

		reg[k] = NIX_AF_TL4X_SHAPE(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ, "NIX_AF_TL4[%u]_SHAPE",
			 schq);

		reg[k] = NIX_AF_TL4X_SW_XOFF(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ, "NIX_AF_TL4[%u]_SW_XOFF",
			 schq);
		break;
	case NIX_TXSCH_LVL_TL3:
		reg[k] = NIX_AF_TL3X_PARENT(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ, "NIX_AF_TL3[%u]_PARENT",
			 schq);

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
		snprintf(regstr[k++], NIX_REG_NAME_SZ, "NIX_AF_TL3[%u]_PIR",
			 schq);

		reg[k] = NIX_AF_TL3X_CIR(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ, "NIX_AF_TL3[%u]_CIR",
			 schq);

		reg[k] = NIX_AF_TL3X_SHAPE(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ, "NIX_AF_TL3[%u]_SHAPE",
			 schq);

		reg[k] = NIX_AF_TL3X_SW_XOFF(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ, "NIX_AF_TL3[%u]_SW_XOFF",
			 schq);
		break;
	case NIX_TXSCH_LVL_TL2:
		reg[k] = NIX_AF_TL2X_PARENT(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ, "NIX_AF_TL2[%u]_PARENT",
			 schq);

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
		snprintf(regstr[k++], NIX_REG_NAME_SZ, "NIX_AF_TL2[%u]_PIR",
			 schq);

		reg[k] = NIX_AF_TL2X_CIR(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ, "NIX_AF_TL2[%u]_CIR",
			 schq);

		reg[k] = NIX_AF_TL2X_SHAPE(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ, "NIX_AF_TL2[%u]_SHAPE",
			 schq);

		reg[k] = NIX_AF_TL2X_SW_XOFF(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ, "NIX_AF_TL2[%u]_SW_XOFF",
			 schq);
		break;
	case NIX_TXSCH_LVL_TL1:

		reg[k] = NIX_AF_TL1X_TOPOLOGY(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ,
			 "NIX_AF_TL1[%u]_TOPOLOGY", schq);

		reg[k] = NIX_AF_TL1X_SCHEDULE(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ,
			 "NIX_AF_TL1[%u]_SCHEDULE", schq);

		reg[k] = NIX_AF_TL1X_CIR(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ, "NIX_AF_TL1[%u]_CIR",
			 schq);

		reg[k] = NIX_AF_TL1X_SW_XOFF(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ, "NIX_AF_TL1[%u]_SW_XOFF",
			 schq);

		reg[k] = NIX_AF_TL1X_DROPPED_PACKETS(schq);
		snprintf(regstr[k++], NIX_REG_NAME_SZ,
			 "NIX_AF_TL1[%u]_DROPPED_PACKETS", schq);
		break;
	default:
		break;
	}

	if (k > MAX_REGS_PER_MBOX_MSG) {
		nix_dump(file, "\t!!!NIX TM Registers request overflow!!!");
		return 0;
	}
	return k;
}

static void
nix_tm_dump_lvl(struct nix *nix, struct nix_tm_node_list *list, uint8_t hw_lvl)
{
	char regstr[MAX_REGS_PER_MBOX_MSG * 2][NIX_REG_NAME_SZ];
	uint64_t reg[MAX_REGS_PER_MBOX_MSG * 2];
	struct mbox *mbox = (&nix->dev)->mbox;
	struct nix_txschq_config *req, *rsp;
	const char *lvlstr, *parent_lvlstr;
	struct nix_tm_node *node, *parent;
	struct nix_tm_node *root = NULL;
	uint32_t schq, parent_schq;
	bool found = false;
	FILE *file = NULL;
	uint8_t j, k, rc;

	TAILQ_FOREACH(node, list, node) {
		if (node->hw_lvl != hw_lvl)
			continue;

		found = true;
		parent = node->parent;
		if (hw_lvl == NIX_TXSCH_LVL_CNT) {
			lvlstr = "SQ";
			schq = node->id;
		} else {
			lvlstr = nix_tm_hwlvl2str(node->hw_lvl);
			schq = node->hw_id;
		}

		if (parent) {
			parent_schq = parent->hw_id;
			parent_lvlstr = nix_tm_hwlvl2str(parent->hw_lvl);
		} else if (node->hw_lvl == NIX_TXSCH_LVL_TL1) {
			parent_schq = nix->tx_link;
			parent_lvlstr = "LINK";
		} else {
			parent_schq = node->parent_hw_id;
			parent_lvlstr = nix_tm_hwlvl2str(node->hw_lvl + 1);
		}

		nix_dump(file, "\t(%p%s) %s_%d->%s_%d", node,
			 node->child_realloc ? "[CR]" : "", lvlstr, schq,
			 parent_lvlstr, parent_schq);

		if (!(node->flags & NIX_TM_NODE_HWRES))
			continue;

		/* Need to dump TL1 when root is TL2 */
		if (node->hw_lvl == nix->tm_root_lvl)
			root = node;

		/* Dump registers only when HWRES is present */
		k = nix_tm_reg_dump_prep(node->hw_lvl, schq, nix->tx_link, reg,
					 regstr);
		if (!k)
			continue;

		req = mbox_alloc_msg_nix_txschq_cfg(mbox_get(mbox));
		req->read = 1;
		req->lvl = node->hw_lvl;
		req->num_regs = k;
		mbox_memcpy(req->reg, reg, sizeof(uint64_t) * k);
		rc = mbox_process_msg(mbox, (void **)&rsp);
		if (!rc) {
			for (j = 0; j < k; j++)
				nix_dump(file, "\t\t%s=0x%016" PRIx64, regstr[j],
					 rsp->regval[j]);
		} else {
			nix_dump(file, "\t!!!Failed to dump registers!!!");
		}
		mbox_put(mbox);
	}

	if (found)
		nix_dump(file, "\n");

	/* Dump TL1 node data when root level is TL2 */
	if (root && root->hw_lvl == NIX_TXSCH_LVL_TL2) {
		k = nix_tm_reg_dump_prep(NIX_TXSCH_LVL_TL1, root->parent_hw_id,
					 nix->tx_link, reg, regstr);
		if (!k)
			return;

		req = mbox_alloc_msg_nix_txschq_cfg(mbox_get(mbox));
		req->read = 1;
		req->lvl = NIX_TXSCH_LVL_TL1;
		req->num_regs = k;
		mbox_memcpy(req->reg, reg, sizeof(uint64_t) * k);
		rc = mbox_process_msg(mbox, (void **)&rsp);
		if (!rc) {
			for (j = 0; j < k; j++)
				nix_dump(file, "\t\t%s=0x%016" PRIx64, regstr[j],
					 rsp->regval[j]);
		} else {
			nix_dump(file, "\t!!!Failed to dump registers!!!");
		}
		mbox_put(mbox);
		nix_dump(file, "\n");
	}
}

void
roc_nix_tm_dump(struct roc_nix *roc_nix, FILE *file)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;
	uint8_t hw_lvl, i;

	nix_dump(file, "===TM hierarchy and registers dump of %s (pf:vf) (%d:%d)===",
		 nix->pci_dev->name, dev_get_pf(dev->pf_func),
		 dev_get_vf(dev->pf_func));

	/* Dump all trees */
	for (i = 0; i < ROC_NIX_TM_TREE_MAX; i++) {
		nix_dump(file, "\tTM %s:", nix_tm_tree2str(i));
		for (hw_lvl = 0; hw_lvl <= NIX_TXSCH_LVL_CNT; hw_lvl++)
			nix_tm_dump_lvl(nix, &nix->trees[i], hw_lvl);
	}

	/* Dump unused resources */
	nix_dump(file, "\tTM unused resources:");
	hw_lvl = NIX_TXSCH_LVL_SMQ;
	for (; hw_lvl < NIX_TXSCH_LVL_CNT; hw_lvl++) {
		nix_dump(file, "\t\ttxschq        %7s num = %d",
			 nix_tm_hwlvl2str(hw_lvl),
			 nix_tm_resource_avail(nix, hw_lvl, false));

		nix_bitmap_dump(nix->schq_bmp[hw_lvl], file);
		nix_dump(file, "\n");

		nix_dump(file, "\t\ttxschq_contig %7s num = %d",
			 nix_tm_hwlvl2str(hw_lvl),
			 nix_tm_resource_avail(nix, hw_lvl, true));
		nix_bitmap_dump(nix->schq_contig_bmp[hw_lvl], file);
		nix_dump(file, "\n");
	}
}

void
roc_nix_dump(struct roc_nix *roc_nix, FILE *file)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;
	int i;

	nix_dump(file, "nix@%p", nix);
	nix_dump(file, "  pf = %d", dev_get_pf(dev->pf_func));
	nix_dump(file, "  vf = %d", dev_get_vf(dev->pf_func));
	nix_dump(file, "  bar2 = 0x%" PRIx64, dev->bar2);
	nix_dump(file, "  bar4 = 0x%" PRIx64, dev->bar4);
	nix_dump(file, "  port_id = %d", roc_nix->port_id);
	nix_dump(file, "  rss_tag_as_xor = %d", roc_nix->rss_tag_as_xor);
	nix_dump(file, "  rss_tag_as_xor = %d", roc_nix->max_sqb_count);
	nix_dump(file, "  outb_nb_desc = %u", roc_nix->outb_nb_desc);

	nix_dump(file, "  \tpci_dev = %p", nix->pci_dev);
	nix_dump(file, "  \tbase = 0x%" PRIxPTR "", nix->base);
	nix_dump(file, "  \tlmt_base = 0x%" PRIxPTR "", nix->lmt_base);
	nix_dump(file, "  \treta_size = %d", nix->reta_sz);
	nix_dump(file, "  \ttx_chan_base = %d", nix->tx_chan_base);
	nix_dump(file, "  \trx_chan_base = %d", nix->rx_chan_base);
	nix_dump(file, "  \tnb_rx_queues = %d", nix->nb_rx_queues);
	nix_dump(file, "  \tnb_tx_queues = %d", nix->nb_tx_queues);
	nix_dump(file, "  \tlso_tsov6_idx = %d", nix->lso_tsov6_idx);
	nix_dump(file, "  \tlso_tsov4_idx = %d", nix->lso_tsov4_idx);
	nix_dump(file, "  \tlso_udp_tun_v4v4 = %d",
		 nix->lso_udp_tun_idx[ROC_NIX_LSO_TUN_V4V4]);
	nix_dump(file, "  \tlso_udp_tun_v4v6 = %d",
		 nix->lso_udp_tun_idx[ROC_NIX_LSO_TUN_V4V6]);
	nix_dump(file, "  \tlso_udp_tun_v6v4 = %d",
		 nix->lso_udp_tun_idx[ROC_NIX_LSO_TUN_V6V4]);
	nix_dump(file, "  \tlso_udp_tun_v6v6 = %d",
		 nix->lso_udp_tun_idx[ROC_NIX_LSO_TUN_V6V6]);
	nix_dump(file, "  \tlso_tun_v4v4 = %d",
		 nix->lso_tun_idx[ROC_NIX_LSO_TUN_V4V4]);
	nix_dump(file, "  \tlso_tun_v4v6 = %d",
		 nix->lso_tun_idx[ROC_NIX_LSO_TUN_V4V6]);
	nix_dump(file, "  \tlso_tun_v6v4 = %d",
		 nix->lso_tun_idx[ROC_NIX_LSO_TUN_V6V4]);
	nix_dump(file, "  \tlso_tun_v6v6 = %d",
		 nix->lso_tun_idx[ROC_NIX_LSO_TUN_V6V6]);
	nix_dump(file, "  \tlf_rx_stats = %d", nix->lf_rx_stats);
	nix_dump(file, "  \tlf_tx_stats = %d", nix->lf_tx_stats);
	nix_dump(file, "  \trx_chan_cnt = %d", nix->rx_chan_cnt);
	nix_dump(file, "  \ttx_chan_cnt = %d", nix->tx_chan_cnt);
	nix_dump(file, "  \tcgx_links = %d", nix->cgx_links);
	nix_dump(file, "  \tlbk_links = %d", nix->lbk_links);
	nix_dump(file, "  \tsdp_links = %d", nix->sdp_links);
	nix_dump(file, "  \ttx_link = %d", nix->tx_link);
	nix_dump(file, "  \tsqb_size = %d", nix->sqb_size);
	nix_dump(file, "  \tmsixoff = %d", nix->msixoff);
	for (i = 0; i < nix->nb_cpt_lf; i++)
		nix_dump(file, "  \tcpt_msixoff[%d] = %d", i, nix->cpt_msixoff[i]);
	nix_dump(file, "  \tcints = %d", nix->cints);
	nix_dump(file, "  \tqints = %d", nix->qints);
	nix_dump(file, "  \tsdp_link = %d", nix->sdp_link);
	nix_dump(file, "  \tptp_en = %d", nix->ptp_en);
	nix_dump(file, "  \trss_alg_idx = %d", nix->rss_alg_idx);
	nix_dump(file, "  \ttx_pause = %d", nix->tx_pause);
	nix_dump(file, "  \tinl_inb_ena = %d", nix->inl_inb_ena);
	nix_dump(file, "  \tinl_outb_ena = %d", nix->inl_outb_ena);
	nix_dump(file, "  \tinb_sa_base = 0x%p", nix->inb_sa_base);
	nix_dump(file, "  \tinb_sa_sz = %" PRIu64, nix->inb_sa_sz);
	nix_dump(file, "  \toutb_sa_base = 0x%p", nix->outb_sa_base);
	nix_dump(file, "  \toutb_sa_sz = %" PRIu64, nix->outb_sa_sz);
	nix_dump(file, "  \toutb_err_sso_pffunc = 0x%x", nix->outb_err_sso_pffunc);
	nix_dump(file, "  \tcpt_lf_base = 0x%p", nix->cpt_lf_base);
	nix_dump(file, "  \tnb_cpt_lf = %d", nix->nb_cpt_lf);
	nix_dump(file, "  \tinb_inl_dev = %d", nix->inb_inl_dev);

}

void
roc_nix_inl_dev_dump(struct roc_nix_inl_dev *roc_inl_dev, FILE *file)
{
	struct idev_cfg *idev = idev_get_cfg();
	struct nix_inl_dev *inl_dev = NULL;
	struct dev *dev = NULL;
	int i;

	if (roc_inl_dev) {
		inl_dev = (struct nix_inl_dev *)&roc_inl_dev->reserved;
	} else {
		if (idev && idev->nix_inl_dev)
			inl_dev = idev->nix_inl_dev;
		else
			return;
	}

	dev = &inl_dev->dev;
	nix_dump(file, "nix_inl_dev@%p", inl_dev);
	nix_dump(file, "  pf = %d", dev_get_pf(dev->pf_func));
	nix_dump(file, "  vf = %d", dev_get_vf(dev->pf_func));
	nix_dump(file, "  bar2 = 0x%" PRIx64, dev->bar2);
	nix_dump(file, "  bar4 = 0x%" PRIx64, dev->bar4);

	nix_dump(file, "  \tpci_dev = %p", inl_dev->pci_dev);
	nix_dump(file, "  \tnix_base = 0x%" PRIxPTR "", inl_dev->nix_base);
	nix_dump(file, "  \tsso_base = 0x%" PRIxPTR "", inl_dev->sso_base);
	nix_dump(file, "  \tssow_base = 0x%" PRIxPTR "", inl_dev->ssow_base);
	nix_dump(file, "  \tnix_msixoff = %d", inl_dev->nix_msixoff);
	nix_dump(file, "  \tsso_msixoff = %d", inl_dev->sso_msixoff);
	nix_dump(file, "  \tssow_msixoff = %d", inl_dev->ssow_msixoff);
	nix_dump(file, "  \tnix_cints = %d", inl_dev->cints);
	nix_dump(file, "  \tnix_qints = %d", inl_dev->qints);
	nix_dump(file, "  \tinb_sa_base = 0x%p", inl_dev->inb_sa_base);
	nix_dump(file, "  \tinb_sa_sz = %d", inl_dev->inb_sa_sz);
	nix_dump(file, "  \txaq_buf_size = %u", inl_dev->xaq_buf_size);
	nix_dump(file, "  \txae_waes = %u", inl_dev->xae_waes);
	nix_dump(file, "  \tiue = %u", inl_dev->iue);
	nix_dump(file, "  \txaq_aura = 0x%" PRIx64, inl_dev->xaq.aura_handle);
	nix_dump(file, "  \txaq_mem = 0x%p", inl_dev->xaq.mem);

	nix_dump(file, "  \tinl_dev_rq:");
	for (i = 0; i < inl_dev->nb_rqs; i++)
		roc_nix_rq_dump(&inl_dev->rqs[i], file);
}

void
roc_nix_inl_outb_cpt_lfs_dump(struct roc_nix *roc_nix, FILE *file)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct roc_cpt_lf *lf_base = nix->cpt_lf_base;
	int i;

	nix_dump(file, "nix@%p", nix);
	for (i = 0; i < nix->nb_cpt_lf; i++) {
		nix_dump(file, "NIX inline dev outbound CPT LFs:");
		cpt_lf_print(&lf_base[i]);
	}
}
