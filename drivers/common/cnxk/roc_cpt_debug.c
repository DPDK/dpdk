/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

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

static void
cpt_lf_print(struct roc_cpt_lf *lf)
{
	uint64_t reg_val;

	reg_val = plt_read64(lf->rbase + CPT_LF_CTX_ENC_BYTE_CNT);
	plt_print("    Encrypted byte count:\t%" PRIu64, reg_val);

	reg_val = plt_read64(lf->rbase + CPT_LF_CTX_ENC_PKT_CNT);
	plt_print("    Encrypted packet count:\t%" PRIu64, reg_val);

	reg_val = plt_read64(lf->rbase + CPT_LF_CTX_DEC_BYTE_CNT);
	plt_print("    Decrypted byte count:\t%" PRIu64, reg_val);

	reg_val = plt_read64(lf->rbase + CPT_LF_CTX_ENC_PKT_CNT);
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

		plt_print("Count registers for CPT LF%d:", lf_id);
		cpt_lf_print(lf);
	}

	return 0;
}
