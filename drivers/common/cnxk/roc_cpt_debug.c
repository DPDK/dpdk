/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

void
roc_cpt_parse_hdr_dump(const struct cpt_parse_hdr_s *cpth)
{
	plt_print("CPT_PARSE \t0x%p:", cpth);

	/* W0 */
	plt_print("W0: cookie \t0x%x\t\tmatch_id \t0x%04x\t\terr_sum \t%u \t",
		  cpth->w0.cookie, cpth->w0.match_id, cpth->w0.err_sum);
	plt_print("W0: reas_sts \t0x%x\t\tet_owr \t%u\t\tpkt_fmt \t%u \t",
		  cpth->w0.reas_sts, cpth->w0.et_owr, cpth->w0.pkt_fmt);
	plt_print("W0: pad_len \t%u\t\tnum_frags \t%u\t\tpkt_out \t%u \t",
		  cpth->w0.pad_len, cpth->w0.num_frags, cpth->w0.pkt_out);

	/* W1 */
	plt_print("W1: wqe_ptr \t0x%016lx\t", cpth->wqe_ptr);

	/* W2 */
	plt_print("W2: frag_age \t0x%x\t\torig_pf_func \t0x%04x",
		  cpth->w2.frag_age, cpth->w2.orig_pf_func);
	plt_print("W2: il3_off \t0x%x\t\tfi_pad \t0x%x\t\tfi_offset \t0x%x \t",
		  cpth->w2.il3_off, cpth->w2.fi_pad, cpth->w2.fi_offset);

	/* W3 */
	plt_print("W3: hw_ccode \t0x%x\t\tuc_ccode \t0x%x\t\tspi \t0x%08x",
		  cpth->w3.hw_ccode, cpth->w3.uc_ccode, cpth->w3.spi);

	/* W4 */
	plt_print("W4: esn \t%" PRIx64 " \t OR frag1_wqe_ptr \t0x%" PRIx64,
		  cpth->esn, cpth->frag1_wqe_ptr);
}

static int
cpt_af_reg_read(struct roc_cpt *roc_cpt, uint64_t reg, uint64_t *val)
{
	struct cpt *cpt = roc_cpt_to_cpt_priv(roc_cpt);
	struct cpt_rd_wr_reg_msg *msg;
	struct dev *dev = &cpt->dev;
	int ret;

	msg = mbox_alloc_msg_cpt_rd_wr_register(dev->mbox);
	if (msg == NULL)
		return -EIO;

	msg->hdr.pcifunc = dev->pf_func;

	msg->is_write = 0;
	msg->reg_offset = reg;
	msg->ret_val = val;

	ret = mbox_process_msg(dev->mbox, (void *)&msg);
	if (ret)
		return -EIO;

	*val = msg->val;

	return 0;
}

static int
cpt_sts_print(struct roc_cpt *roc_cpt)
{
	struct cpt *cpt = roc_cpt_to_cpt_priv(roc_cpt);
	struct dev *dev = &cpt->dev;
	struct cpt_sts_req *req;
	struct cpt_sts_rsp *rsp;
	int ret;

	req = mbox_alloc_msg_cpt_sts_get(dev->mbox);
	if (req == NULL)
		return -EIO;

	req->blkaddr = 0;
	ret = mbox_process_msg(dev->mbox, (void *)&rsp);
	if (ret)
		return -EIO;

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

	return 0;
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
