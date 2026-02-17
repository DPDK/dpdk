/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

#define cpt_dump(file, fmt, ...)                                                                   \
	do {                                                                                       \
		if ((file) == NULL)                                                                \
			plt_dump(fmt, ##__VA_ARGS__);                                              \
		else                                                                               \
			fprintf(file, fmt "\n", ##__VA_ARGS__);                                    \
	} while (0)

static inline void
cpt_cnxk_parse_hdr_dump(FILE *file, const struct cpt_parse_hdr_s *cpth)
{
	struct cpt_frag_info_s *frag_info;
	struct cpt_rxc_sg_s *rxc_sg;
	uint32_t offset;
	int i;

	cpt_dump(file, "CPT_PARSE \t0x%p:", cpth);

	/* W0 */
	cpt_dump(file, "W0: cookie \t0x%x\t\tmatch_id \t0x%04x \t", cpth->w0.cookie,
		 cpth->w0.match_id);
	cpt_dump(file, "W0: err_sum \t%u \t", cpth->w0.err_sum);
	cpt_dump(file, "W0: reas_sts \t0x%x\t\tet_owr \t%u\t\tpkt_fmt \t%u \t", cpth->w0.reas_sts,
		 cpth->w0.et_owr, cpth->w0.pkt_fmt);
	cpt_dump(file, "W0: pad_len \t%u\t\tnum_frags \t%u\t\tpkt_out \t%u \t", cpth->w0.pad_len,
		 cpth->w0.num_frags, cpth->w0.pkt_out);

	/* W1 */
	cpt_dump(file, "W1: wqe_ptr \t0x%016lx\t", cpth->wqe_ptr);

	/* W2 */
	cpt_dump(file, "W2: pkt_inline \t0x%x\t\torig_pkt_aura \t0x%x", cpth->w2.pkt_inline,
		 cpth->w2.orig_pkt_aura);
	cpt_dump(file, "W2: il3_off \t0x%x\t\tptr_pad \t0x%x \t", cpth->w2.il3_off,
		 cpth->w2.ptr_pad);
	cpt_dump(file, "W2: ptr_offset \t0x%x \t", cpth->w2.ptr_offset);

	/* W3 */
	cpt_dump(file, "W3: hw_ccode \t0x%x\t\tuc_ccode \t0x%x\t\tfrag_age \t0x%04x",
		 cpth->w3.hw_ccode, cpth->w3.uc_ccode, cpth->w3.frag_age);
	cpt_dump(file, "W3: pf_func \t0x%x\t\trlen \t0x%x \t", cpth->w3.pf_func, cpth->w3.rlen);

	/* W4 */
	cpt_dump(file, "W4: l4_chksum \t0x%x\t\tl4_chksum_type \t0x%x", cpth->w4.l4_chksum,
		 cpth->w4.l4_chksum_type);
	cpt_dump(file, "W4: channel \t0x%x\t\tsctr_size \t0x%08x\t\tgthr_size \t0x%08x",
		 cpth->w4.channel, cpth->w4.sctr_size, cpth->w4.gthr_size);

	/* offset of 0 implies 256B, otherwise it implies offset*32B */
	offset = cpth->w2.ptr_offset;
	offset = (((offset - 1) & 0x7) + 1) * 32;
	frag_info = PLT_PTR_ADD(cpth, offset);

	if (cpth->w0.num_frags > 0) {
		cpt_dump(file, "CPT Fraginfo_0 \t%p:", frag_info);

		/* W0 */
		cpt_dump(file, "W0: f0.info \t0x%x", frag_info->w0.f0.info);
		cpt_dump(file, "W0: f1.info \t0x%x", frag_info->w0.f1.info);
		cpt_dump(file, "W0: f2.info \t0x%x", frag_info->w0.f2.info);
		cpt_dump(file, "W0: f3.info \t0x%x", frag_info->w0.f3.info);

		/* W1 */
		cpt_dump(file, "W1: frag_size0 \t0x%x", frag_info->w1.frag_size0);
		cpt_dump(file, "W1: frag_size1 \t0x%x", frag_info->w1.frag_size1);
		cpt_dump(file, "W1: frag_size2 \t0x%x", frag_info->w1.frag_size2);
		cpt_dump(file, "W1: frag_size3 \t0x%x", frag_info->w1.frag_size3);

		frag_info++;
	}

	if (cpth->w0.num_frags > 4) {
		cpt_dump(file, "CPT Fraginfo_1 \t%p:", frag_info);

		/* W0 */
		cpt_dump(file, "W0: f4.info \t0x%x", frag_info->w0.f0.info);
		cpt_dump(file, "W0: f5.info \t0x%x", frag_info->w0.f1.info);
		cpt_dump(file, "W0: f6.info \t0x%x", frag_info->w0.f2.info);
		cpt_dump(file, "W0: f7.info \t0x%x", frag_info->w0.f3.info);

		/* W1 */
		cpt_dump(file, "W1: frag_size4 \t0x%x", frag_info->w1.frag_size0);
		cpt_dump(file, "W1: frag_size5 \t0x%x", frag_info->w1.frag_size1);
		cpt_dump(file, "W1: frag_size6 \t0x%x", frag_info->w1.frag_size2);
		cpt_dump(file, "W1: frag_size7 \t0x%x", frag_info->w1.frag_size3);

		frag_info++;
	}

	rxc_sg = (struct cpt_rxc_sg_s *)frag_info;
	for (i = 0; i < cpth->w4.sctr_size; i++) {
		cpt_dump(file, "CPT RXC SC SGS \t%p:", rxc_sg);
		cpt_dump(file, "W0: seg1_size \t0x%x\t\tseg2_size \t0x%x\t\tseg3_size \t0x%04x",
			 rxc_sg->w0.seg1_size, rxc_sg->w0.seg2_size, rxc_sg->w0.seg3_size);
		cpt_dump(file, "W0: segs \t0x%x\t\tnxt_fst_frag \t0x%x\t\tblk_sz \t0x%x",
			 rxc_sg->w0.segs, rxc_sg->w0.nxt_fst_frag, rxc_sg->w0.blk_sz);
		cpt_dump(file, "W1: seg1_ptr \t0x%" PRIx64, rxc_sg->seg1_ptr);
		cpt_dump(file, "W2: seg2_ptr \t0x%" PRIx64, rxc_sg->seg2_ptr);
		cpt_dump(file, "W3: seg3_ptr \t0x%" PRIx64, rxc_sg->seg3_ptr);

		rxc_sg++;
	}

	for (i = 0; i < cpth->w4.gthr_size; i++) {
		cpt_dump(file, "CPT RXC GT SGS \t0x%p:", rxc_sg);
		cpt_dump(file, "W0: seg1_size \t0x%x\t\tseg2_size \t0x%x\t\tseg3_size \t0x%04x",
			 rxc_sg->w0.seg1_size, rxc_sg->w0.seg2_size, rxc_sg->w0.seg3_size);
		cpt_dump(file, "W0: segs \t0x%x\t\tnxt_fst_frag \t0x%x\t\tblk_sz \t0x%x",
			 rxc_sg->w0.segs, rxc_sg->w0.nxt_fst_frag, rxc_sg->w0.blk_sz);
		cpt_dump(file, "W1: seg1_ptr \t0x%" PRIx64, rxc_sg->seg1_ptr);
		cpt_dump(file, "W2: seg2_ptr \t0x%" PRIx64, rxc_sg->seg2_ptr);
		cpt_dump(file, "W3: seg3_ptr \t0x%" PRIx64, rxc_sg->seg3_ptr);

		rxc_sg++;
	}
}

static inline void
cpt_cn10k_parse_hdr_dump(FILE *file, const struct cpt_cn10k_parse_hdr_s *cpth)
{
	struct cpt_cn10k_frag_info_s *frag_info;
	uint32_t offset;
	uint64_t *slot;

	cpt_dump(file, "CPT_PARSE \t0x%p:", cpth);

	/* W0 */
	cpt_dump(file, "W0: cookie \t0x%x\t\tmatch_id \t0x%04x \t",
		  cpth->w0.cookie, cpth->w0.match_id);
	cpt_dump(file, "W0: err_sum \t%u \t", cpth->w0.err_sum);
	cpt_dump(file, "W0: reas_sts \t0x%x\t\tet_owr \t%u\t\tpkt_fmt \t%u \t",
		  cpth->w0.reas_sts, cpth->w0.et_owr, cpth->w0.pkt_fmt);
	cpt_dump(file, "W0: pad_len \t%u\t\tnum_frags \t%u\t\tpkt_out \t%u \t",
		  cpth->w0.pad_len, cpth->w0.num_frags, cpth->w0.pkt_out);

	/* W1 */
	cpt_dump(file, "W1: wqe_ptr \t0x%016lx\t",
			plt_be_to_cpu_64(cpth->wqe_ptr));

	/* W2 */
	cpt_dump(file, "W2: frag_age \t0x%x\t\torig_pf_func \t0x%04x",
		  cpth->w2.frag_age, cpth->w2.orig_pf_func);
	cpt_dump(file, "W2: il3_off \t0x%x\t\tfi_pad \t0x%x \t",
		  cpth->w2.il3_off, cpth->w2.fi_pad);
	cpt_dump(file, "W2: fi_offset \t0x%x \t", cpth->w2.fi_offset);

	/* W3 */
	cpt_dump(file, "W3: hw_ccode \t0x%x\t\tuc_ccode \t0x%x\t\tspi \t0x%08x",
		  cpth->w3.hw_ccode, cpth->w3.uc_ccode, cpth->w3.spi);

	/* W4 */
	cpt_dump(file, "W4: esn \t%" PRIx64 " \t OR frag1_wqe_ptr \t0x%" PRIx64,
		  cpth->esn, plt_be_to_cpu_64(cpth->frag1_wqe_ptr));

	/* offset of 0 implies 256B, otherwise it implies offset*8B */
	offset = cpth->w2.fi_offset;
	offset = (((offset - 1) & 0x1f) + 1) * 8;
	frag_info = PLT_PTR_ADD(cpth, offset);

	cpt_dump(file, "CPT Fraginfo \t0x%p:", frag_info);

	/* W0 */
	cpt_dump(file, "W0: f0.info \t0x%x", frag_info->w0.f0.info);
	cpt_dump(file, "W0: f1.info \t0x%x", frag_info->w0.f1.info);
	cpt_dump(file, "W0: f2.info \t0x%x", frag_info->w0.f2.info);
	cpt_dump(file, "W0: f3.info \t0x%x", frag_info->w0.f3.info);

	/* W1 */
	cpt_dump(file, "W1: frag_size0 \t0x%x", frag_info->w1.frag_size0);
	cpt_dump(file, "W1: frag_size1 \t0x%x", frag_info->w1.frag_size1);
	cpt_dump(file, "W1: frag_size2 \t0x%x", frag_info->w1.frag_size2);
	cpt_dump(file, "W1: frag_size3 \t0x%x", frag_info->w1.frag_size3);

	slot = (uint64_t *)(frag_info + 1);
	cpt_dump(file, "Frag Slot2:  WQE ptr \t%p", (void *)plt_be_to_cpu_64(slot[0]));
	cpt_dump(file, "Frag Slot3:  WQE ptr \t%p", (void *)plt_be_to_cpu_64(slot[1]));
}

void
roc_cpt_parse_hdr_dump(FILE *file, const union cpt_parse_hdr_u *cpth)
{
	if (roc_model_is_cn10k())
		cpt_cn10k_parse_hdr_dump(file, &cpth->cn10k);
	else
		cpt_cnxk_parse_hdr_dump(file, &cpth->s);
}

static int
cpt_af_reg_read(struct roc_cpt *roc_cpt, uint64_t reg, uint64_t *val)
{
	struct cpt *cpt = roc_cpt_to_cpt_priv(roc_cpt);
	struct cpt_rd_wr_reg_msg *msg;
	struct dev *dev = &cpt->dev;
	struct mbox *mbox = mbox_get(dev->mbox);
	int ret;

	msg = mbox_alloc_msg_cpt_rd_wr_register(mbox);
	if (msg == NULL) {
		ret = -EIO;
		goto exit;
	}

	msg->hdr.pcifunc = dev->pf_func;

	msg->is_write = 0;
	msg->reg_offset = reg;
	msg->ret_val = val;

	ret = mbox_process_msg(dev->mbox, (void *)&msg);
	if (ret) {
		ret =  -EIO;
		goto exit;
	}

	*val = msg->val;

	ret = 0;
exit:
	mbox_put(mbox);
	return ret;
}

static int
cpt_sts_print(struct roc_cpt *roc_cpt)
{
	struct cpt *cpt = roc_cpt_to_cpt_priv(roc_cpt);
	struct dev *dev = &cpt->dev;
	struct cpt_sts_req *req;
	struct cpt_sts_rsp *rsp;
	struct mbox *mbox = mbox_get(dev->mbox);
	int ret;

	req = mbox_alloc_msg_cpt_sts_get(mbox);
	if (req == NULL) {
		ret = -EIO;
		goto exit;
	}

	req->blkaddr = 0;
	ret = mbox_process_msg(dev->mbox, (void *)&rsp);
	if (ret) {
		ret = -EIO;
		goto exit;
	}

	plt_print("    %s:\t0x%016" PRIx64, "inst_req_pc", rsp->inst_req_pc);
	plt_print("    %s:\t0x%016" PRIx64, "inst_lat_pc", rsp->inst_lat_pc);
	plt_print("    %s:\t\t0x%016" PRIx64, "rd_req_pc", rsp->rd_req_pc);
	plt_print("    %s:\t\t0x%016" PRIx64, "rd_lat_pc", rsp->rd_lat_pc);
	plt_print("    %s:\t\t0x%016" PRIx64, "rd_uc_pc", rsp->rd_uc_pc);
	plt_print("    %s:\t0x%016" PRIx64, "active_cycles_pc",
		  rsp->active_cycles_pc);
	plt_print("    %s:\t\t0x%016" PRIx64, "ctx_mis_pc", rsp->ctx_mis_pc);
	plt_print("    %s:\t\t0x%016" PRIx64, "ctx_hit_pc", rsp->ctx_hit_pc);
	plt_print("    %s:\t\t0x%016" PRIx64, "ctx_aop_pc", rsp->ctx_aop_pc);
	plt_print("    %s:\t0x%016" PRIx64, "ctx_aop_lat_pc",
		  rsp->ctx_aop_lat_pc);
	plt_print("    %s:\t0x%016" PRIx64, "ctx_ifetch_pc",
		  rsp->ctx_ifetch_pc);
	plt_print("    %s:\t0x%016" PRIx64, "ctx_ifetch_lat_pc",
		  rsp->ctx_ifetch_lat_pc);
	plt_print("    %s:\t0x%016" PRIx64, "ctx_ffetch_pc",
		  rsp->ctx_ffetch_pc);
	plt_print("    %s:\t0x%016" PRIx64, "ctx_ffetch_lat_pc",
		  rsp->ctx_ffetch_lat_pc);
	plt_print("    %s:\t0x%016" PRIx64, "ctx_wback_pc", rsp->ctx_wback_pc);
	plt_print("    %s:\t0x%016" PRIx64, "ctx_wback_lat_pc",
		  rsp->ctx_wback_lat_pc);
	plt_print("    %s:\t\t0x%016" PRIx64, "ctx_psh_pc", rsp->ctx_psh_pc);
	plt_print("    %s:\t0x%016" PRIx64, "ctx_psh_lat_pc",
		  rsp->ctx_psh_lat_pc);
	plt_print("    %s:\t\t0x%016" PRIx64, "ctx_err", rsp->ctx_err);
	plt_print("    %s:\t\t0x%016" PRIx64, "ctx_enc_id", rsp->ctx_enc_id);
	plt_print("    %s:\t0x%016" PRIx64, "ctx_flush_timer",
		  rsp->ctx_flush_timer);
	plt_print("    %s:\t\t0x%016" PRIx64, "rxc_time", rsp->rxc_time);
	plt_print("    %s:\t0x%016" PRIx64, "rxc_time_cfg", rsp->rxc_time_cfg);
	plt_print("    %s:\t0x%016" PRIx64, "rxc_active_sts",
		  rsp->rxc_active_sts);
	plt_print("    %s:\t0x%016" PRIx64, "rxc_zombie_sts",
		  rsp->rxc_zombie_sts);
	plt_print("    %s:\t0x%016" PRIx64, "rxc_dfrg", rsp->rxc_dfrg);
	plt_print("    %s:\t0x%016" PRIx64, "x2p_link_cfg0",
		  rsp->x2p_link_cfg0);
	plt_print("    %s:\t0x%016" PRIx64, "x2p_link_cfg1",
		  rsp->x2p_link_cfg1);
	plt_print("    %s:\t0x%016" PRIx64, "busy_sts_ae", rsp->busy_sts_ae);
	plt_print("    %s:\t0x%016" PRIx64, "free_sts_ae", rsp->free_sts_ae);
	plt_print("    %s:\t0x%016" PRIx64, "busy_sts_se", rsp->busy_sts_se);
	plt_print("    %s:\t0x%016" PRIx64, "free_sts_se", rsp->free_sts_se);
	plt_print("    %s:\t0x%016" PRIx64, "busy_sts_ie", rsp->busy_sts_ie);
	plt_print("    %s:\t0x%016" PRIx64, "free_sts_ie", rsp->free_sts_ie);
	plt_print("    %s:\t0x%016" PRIx64, "exe_err_info", rsp->exe_err_info);
	plt_print("    %s:\t\t0x%016" PRIx64, "cptclk_cnt", rsp->cptclk_cnt);
	plt_print("    %s:\t\t0x%016" PRIx64, "diag", rsp->diag);

	ret = 0;
exit:
	mbox_put(mbox);
	return ret;
}

int
roc_cpt_afs_print(struct roc_cpt *roc_cpt)
{
	uint64_t reg_val;

	plt_print("CPT AF registers:");

	if (cpt_af_reg_read(roc_cpt, CPT_AF_LFX_CTL(0), &reg_val))
		return -EIO;

	plt_print("    CPT_AF_LF0_CTL:\t0x%016" PRIx64, reg_val);

	if (cpt_af_reg_read(roc_cpt, CPT_AF_LFX_CTL2(0), &reg_val))
		return -EIO;

	plt_print("    CPT_AF_LF0_CTL2:\t0x%016" PRIx64, reg_val);

	cpt_sts_print(roc_cpt);

	return 0;
}

void
cpt_lf_print(struct roc_cpt_lf *lf)
{
	uint64_t reg_val;

	reg_val = plt_read64(lf->rbase + CPT_LF_Q_BASE);
	plt_print("    CPT_LF_Q_BASE:\t%016lx", reg_val);

	reg_val = plt_read64(lf->rbase + CPT_LF_Q_SIZE);
	plt_print("    CPT_LF_Q_SIZE:\t%016lx", reg_val);

	reg_val = plt_read64(lf->rbase + CPT_LF_Q_INST_PTR);
	plt_print("    CPT_LF_Q_INST_PTR:\t%016lx", reg_val);

	reg_val = plt_read64(lf->rbase + CPT_LF_Q_GRP_PTR);
	plt_print("    CPT_LF_Q_GRP_PTR:\t%016lx", reg_val);

	reg_val = plt_read64(lf->rbase + CPT_LF_CTL);
	plt_print("    CPT_LF_CTL:\t%016lx", reg_val);

	reg_val = plt_read64(lf->rbase + CPT_LF_MISC_INT_ENA_W1S);
	plt_print("    CPT_LF_MISC_INT_ENA_W1S:\t%016lx", reg_val);

	reg_val = plt_read64(lf->rbase + CPT_LF_MISC_INT);
	plt_print("    CPT_LF_MISC_INT:\t%016lx", reg_val);

	reg_val = plt_read64(lf->rbase + CPT_LF_INPROG);
	plt_print("    CPT_LF_INPROG:\t%016lx", reg_val);

	if (roc_model_is_cn9k())
		return;

	plt_print("Count registers for CPT LF%d:", lf->lf_id);

	reg_val = plt_read64(lf->rbase + CPT_LF_CTX_ENC_BYTE_CNT);
	plt_print("    Encrypted byte count:\t%" PRIu64, reg_val);

	reg_val = plt_read64(lf->rbase + CPT_LF_CTX_ENC_PKT_CNT);
	plt_print("    Encrypted packet count:\t%" PRIu64, reg_val);

	reg_val = plt_read64(lf->rbase + CPT_LF_CTX_DEC_BYTE_CNT);
	plt_print("    Decrypted byte count:\t%" PRIu64, reg_val);

	reg_val = plt_read64(lf->rbase + CPT_LF_CTX_DEC_PKT_CNT);
	plt_print("    Decrypted packet count:\t%" PRIu64, reg_val);
}

int
roc_cpt_lfs_print(struct roc_cpt *roc_cpt)
{
	struct cpt *cpt = roc_cpt_to_cpt_priv(roc_cpt);
	struct roc_cpt_lf *lf;
	int lf_id;

	if (cpt == NULL)
		return -EINVAL;

	for (lf_id = 0; lf_id < roc_cpt->nb_lf; lf_id++) {
		lf = roc_cpt->lf[lf_id];
		if (lf == NULL)
			continue;

		cpt_lf_print(lf);
	}

	return 0;
}
