/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2015 Freescale Semiconductor, Inc.
 * Copyright 2018-2020 NXP
 */

#include "compat.h"
#include <fsl_qbman_debug.h>
#include "qbman_portal.h"

/* QBMan portal management command code */
#define QBMAN_BP_QUERY            0x32
#define QBMAN_FQ_QUERY            0x44
#define QBMAN_FQ_QUERY_NP         0x45
#define QBMAN_WQ_QUERY            0x47
#define QBMAN_CGR_QUERY           0x51
#define QBMAN_WRED_QUERY          0x54
#define QBMAN_CGR_STAT_QUERY      0x55
#define QBMAN_CGR_STAT_QUERY_CLR  0x56

struct qbman_bp_query_desc {
	uint8_t verb;
	uint8_t reserved;
	uint16_t bpid;
	uint8_t reserved2[60];
};

#define QB_BP_STATE_SHIFT  24
#define QB_BP_VA_SHIFT     1
#define QB_BP_VA_MASK      0x2
#define QB_BP_WAE_SHIFT    2
#define QB_BP_WAE_MASK     0x4
#define QB_BP_PL_SHIFT     15
#define QB_BP_PL_MASK      0x8000
#define QB_BP_ICID_MASK    0x7FFF

int qbman_bp_query(struct qbman_swp *s, uint32_t bpid,
		   struct qbman_bp_query_rslt *r)
{
	struct qbman_bp_query_desc *p;

	/* Start the management command */
	p = (struct qbman_bp_query_desc *)qbman_swp_mc_start(s);
	if (!p)
		return -EBUSY;

	/* Encode the caller-provided attributes */
	p->bpid = bpid;

	/* Complete the management command */
	*r = *(struct qbman_bp_query_rslt *)qbman_swp_mc_complete(s, p,
						 QBMAN_BP_QUERY);
	if (!r) {
		pr_err("qbman: Query BPID %d failed, no response\n",
			bpid);
		return -EIO;
	}

	/* Decode the outcome */
	QBMAN_BUG_ON((r->verb & QBMAN_RESPONSE_VERB_MASK) != QBMAN_BP_QUERY);

	/* Determine success or failure */
	if (r->rslt != QBMAN_MC_RSLT_OK) {
		pr_err("Query of BPID 0x%x failed, code=0x%02x\n", bpid,
								r->rslt);
		return -EIO;
	}

	return 0;
}

int qbman_bp_get_bdi(struct qbman_bp_query_rslt *r)
{
	return r->bdi & 1;
}

int qbman_bp_get_va(struct qbman_bp_query_rslt *r)
{
	return (r->bdi & QB_BP_VA_MASK) >> QB_BP_VA_MASK;
}

int qbman_bp_get_wae(struct qbman_bp_query_rslt *r)
{
	return (r->bdi & QB_BP_WAE_MASK) >> QB_BP_WAE_SHIFT;
}

static uint16_t qbman_bp_thresh_to_value(uint16_t val)
{
	return (val & 0xff) << ((val & 0xf00) >> 8);
}

uint16_t qbman_bp_get_swdet(struct qbman_bp_query_rslt  *r)
{

	return qbman_bp_thresh_to_value(r->swdet);
}

uint16_t qbman_bp_get_swdxt(struct qbman_bp_query_rslt  *r)
{
	return qbman_bp_thresh_to_value(r->swdxt);
}

uint16_t qbman_bp_get_hwdet(struct qbman_bp_query_rslt  *r)
{
	return qbman_bp_thresh_to_value(r->hwdet);
}

uint16_t qbman_bp_get_hwdxt(struct qbman_bp_query_rslt  *r)
{
	return qbman_bp_thresh_to_value(r->hwdxt);
}

uint16_t qbman_bp_get_swset(struct qbman_bp_query_rslt  *r)
{
	return qbman_bp_thresh_to_value(r->swset);
}

uint16_t qbman_bp_get_swsxt(struct qbman_bp_query_rslt  *r)
{

	return qbman_bp_thresh_to_value(r->swsxt);
}

uint16_t qbman_bp_get_vbpid(struct qbman_bp_query_rslt  *r)
{
	return r->vbpid;
}

uint16_t qbman_bp_get_icid(struct qbman_bp_query_rslt  *r)
{
	return r->icid & QB_BP_ICID_MASK;
}

int qbman_bp_get_pl(struct qbman_bp_query_rslt  *r)
{
	return (r->icid & QB_BP_PL_MASK) >> QB_BP_PL_SHIFT;
}

uint64_t qbman_bp_get_bpscn_addr(struct qbman_bp_query_rslt  *r)
{
	return r->bpscn_addr;
}

uint64_t qbman_bp_get_bpscn_ctx(struct qbman_bp_query_rslt  *r)
{
	return r->bpscn_ctx;
}

uint16_t qbman_bp_get_hw_targ(struct qbman_bp_query_rslt  *r)
{
	return r->hw_targ;
}

int qbman_bp_has_free_bufs(struct qbman_bp_query_rslt  *r)
{
	return !(int)(r->state & 0x1);
}

int qbman_bp_is_depleted(struct qbman_bp_query_rslt  *r)
{
	return (int)((r->state & 0x2) >> 1);
}

int qbman_bp_is_surplus(struct qbman_bp_query_rslt  *r)
{
	return (int)((r->state & 0x4) >> 2);
}

uint32_t qbman_bp_num_free_bufs(struct qbman_bp_query_rslt  *r)
{
	return r->fill;
}

uint32_t qbman_bp_get_hdptr(struct qbman_bp_query_rslt  *r)
{
	return r->hdptr;
}

uint32_t qbman_bp_get_sdcnt(struct qbman_bp_query_rslt  *r)
{
	return r->sdcnt;
}

uint32_t qbman_bp_get_hdcnt(struct qbman_bp_query_rslt  *r)
{
	return r->hdcnt;
}

uint32_t qbman_bp_get_sscnt(struct qbman_bp_query_rslt  *r)
{
	return r->sscnt;
}

struct qbman_fq_query_desc {
	uint8_t verb;
	uint8_t reserved[3];
	uint32_t fqid;
	uint8_t reserved2[56];
};

/* FQ query function for programmable fields */
int qbman_fq_query(struct qbman_swp *s, uint32_t fqid,
		   struct qbman_fq_query_rslt *r)
{
	struct qbman_fq_query_desc *p;

	p = (struct qbman_fq_query_desc *)qbman_swp_mc_start(s);
	if (!p)
		return -EBUSY;

	p->fqid = fqid;
	*r = *(struct qbman_fq_query_rslt *)qbman_swp_mc_complete(s, p,
					  QBMAN_FQ_QUERY);
	if (!r) {
		pr_err("qbman: Query FQID %d failed, no response\n",
			fqid);
		return -EIO;
	}

	/* Decode the outcome */
	QBMAN_BUG_ON((r->verb & QBMAN_RESPONSE_VERB_MASK) != QBMAN_FQ_QUERY);

	/* Determine success or failure */
	if (r->rslt != QBMAN_MC_RSLT_OK) {
		pr_err("Query of FQID 0x%x failed, code=0x%02x\n",
		       fqid, r->rslt);
		return -EIO;
	}

	return 0;
}

uint8_t qbman_fq_attr_get_fqctrl(struct qbman_fq_query_rslt *r)
{
	return r->fq_ctrl;
}

uint16_t qbman_fq_attr_get_cgrid(struct qbman_fq_query_rslt *r)
{
	return r->cgid;
}

uint16_t qbman_fq_attr_get_destwq(struct qbman_fq_query_rslt *r)
{
	return r->dest_wq;
}

static uint16_t qbman_thresh_to_value(uint16_t val)
{
	return ((val & 0x1FE0) >> 5) << (val & 0x1F);
}

uint16_t qbman_fq_attr_get_tdthresh(struct qbman_fq_query_rslt *r)
{
	return qbman_thresh_to_value(r->td_thresh);
}

int qbman_fq_attr_get_oa_ics(struct qbman_fq_query_rslt *r)
{
	return (int)(r->oal_oac >> 14) & 0x1;
}

int qbman_fq_attr_get_oa_cgr(struct qbman_fq_query_rslt *r)
{
	return (int)(r->oal_oac >> 15);
}

uint16_t qbman_fq_attr_get_oal(struct qbman_fq_query_rslt *r)
{
	return (r->oal_oac & 0x0FFF);
}

int qbman_fq_attr_get_bdi(struct qbman_fq_query_rslt *r)
{
	return (r->mctl & 0x1);
}

int qbman_fq_attr_get_ff(struct qbman_fq_query_rslt *r)
{
	return (r->mctl & 0x2) >> 1;
}

int qbman_fq_attr_get_va(struct qbman_fq_query_rslt *r)
{
	return (r->mctl & 0x4) >> 2;
}

int qbman_fq_attr_get_ps(struct qbman_fq_query_rslt *r)
{
	return (r->mctl & 0x8) >> 3;
}

int qbman_fq_attr_get_pps(struct qbman_fq_query_rslt *r)
{
	return (r->mctl & 0x30) >> 4;
}

uint16_t qbman_fq_attr_get_icid(struct qbman_fq_query_rslt *r)
{
	return r->icid & 0x7FFF;
}

int qbman_fq_attr_get_pl(struct qbman_fq_query_rslt *r)
{
	return (int)((r->icid & 0x8000) >> 15);
}

uint32_t qbman_fq_attr_get_vfqid(struct qbman_fq_query_rslt *r)
{
	return r->vfqid & 0x00FFFFFF;
}

uint32_t qbman_fq_attr_get_erfqid(struct qbman_fq_query_rslt *r)
{
	return r->fqid_er & 0x00FFFFFF;
}

uint16_t qbman_fq_attr_get_opridsz(struct qbman_fq_query_rslt *r)
{
	return r->opridsz;
}

int qbman_fq_query_state(struct qbman_swp *s, uint32_t fqid,
			 struct qbman_fq_query_np_rslt *r)
{
	struct qbman_fq_query_desc *p;
	struct qbman_fq_query_np_rslt *var;

	p = (struct qbman_fq_query_desc *)qbman_swp_mc_start(s);
	if (!p)
		return -EBUSY;

	p->fqid = fqid;
	var = qbman_swp_mc_complete(s, p, QBMAN_FQ_QUERY_NP);
	if (!var) {
		pr_err("qbman: Query FQID %d NP fields failed, no response\n",
		       fqid);
		return -EIO;
	}
	*r = *var;

	/* Decode the outcome */
	QBMAN_BUG_ON((r->verb & QBMAN_RESPONSE_VERB_MASK) != QBMAN_FQ_QUERY_NP);

	/* Determine success or failure */
	if (r->rslt != QBMAN_MC_RSLT_OK) {
		pr_err("Query NP fields of FQID 0x%x failed, code=0x%02x\n",
		       fqid, r->rslt);
		return -EIO;
	}

	return 0;
}

uint8_t qbman_fq_state_schedstate(const struct qbman_fq_query_np_rslt *r)
{
	return r->st1 & 0x7;
}

int qbman_fq_state_force_eligible(const struct qbman_fq_query_np_rslt *r)
{
	return (int)((r->st1 & 0x8) >> 3);
}

int qbman_fq_state_xoff(const struct qbman_fq_query_np_rslt *r)
{
	return (int)((r->st1 & 0x10) >> 4);
}

int qbman_fq_state_retirement_pending(const struct qbman_fq_query_np_rslt *r)
{
	return (int)((r->st1 & 0x20) >> 5);
}

int qbman_fq_state_overflow_error(const struct qbman_fq_query_np_rslt *r)
{
	return (int)((r->st1 & 0x40) >> 6);
}

uint32_t qbman_fq_state_frame_count(const struct qbman_fq_query_np_rslt *r)
{
	return (r->frm_cnt & 0x00FFFFFF);
}

uint32_t qbman_fq_state_byte_count(const struct qbman_fq_query_np_rslt *r)
{
	return r->byte_cnt;
}

/* Query CGR */
struct qbman_cgr_query_desc {
	uint8_t verb;
	uint8_t reserved;
	uint16_t cgid;
	uint8_t reserved2[60];
};

int qbman_cgr_query(struct qbman_swp *s, uint32_t cgid,
		    struct qbman_cgr_query_rslt *r)
{
	struct qbman_cgr_query_desc *p;

	p = (struct qbman_cgr_query_desc *)qbman_swp_mc_start(s);
	if (!p)
		return -EBUSY;

	p->cgid = cgid;
	*r = *(struct qbman_cgr_query_rslt *)qbman_swp_mc_complete(s, p,
							QBMAN_CGR_QUERY);
	if (!r) {
		pr_err("qbman: Query CGID %d failed, no response\n",
			cgid);
		return -EIO;
	}

	/* Decode the outcome */
	QBMAN_BUG_ON((r->verb & QBMAN_RESPONSE_VERB_MASK) != QBMAN_CGR_QUERY);

	/* Determine success or failure */
	if (r->rslt != QBMAN_MC_RSLT_OK) {
		pr_err("Query CGID 0x%x failed,code=0x%02x\n", cgid, r->rslt);
		return -EIO;
	}

	return 0;
}

int qbman_cgr_get_cscn_wq_en_enter(struct qbman_cgr_query_rslt *r)
{
	return (int)(r->ctl1 & 0x1);
}

int qbman_cgr_get_cscn_wq_en_exit(struct qbman_cgr_query_rslt *r)
{
	return (int)((r->ctl1 & 0x2) >> 1);
}

int qbman_cgr_get_cscn_wq_icd(struct qbman_cgr_query_rslt *r)
{
	return (int)((r->ctl1 & 0x4) >> 2);
}

uint8_t qbman_cgr_get_mode(struct qbman_cgr_query_rslt *r)
{
	return r->mode & 0x3;
}

int qbman_cgr_get_rej_cnt_mode(struct qbman_cgr_query_rslt *r)
{
	return (int)((r->mode & 0x4) >> 2);
}

int qbman_cgr_get_cscn_bdi(struct qbman_cgr_query_rslt *r)
{
	return (int)((r->mode & 0x8) >> 3);
}

uint16_t qbman_cgr_attr_get_cs_thres(struct qbman_cgr_query_rslt *r)
{
	return qbman_thresh_to_value(r->cs_thres);
}

uint16_t qbman_cgr_attr_get_cs_thres_x(struct qbman_cgr_query_rslt *r)
{
	return qbman_thresh_to_value(r->cs_thres_x);
}

uint16_t qbman_cgr_attr_get_td_thres(struct qbman_cgr_query_rslt *r)
{
	return qbman_thresh_to_value(r->td_thres);
}

int qbman_cgr_wred_query(struct qbman_swp *s, uint32_t cgid,
			struct qbman_wred_query_rslt *r)
{
	struct qbman_cgr_query_desc *p;

	p = (struct qbman_cgr_query_desc *)qbman_swp_mc_start(s);
	if (!p)
		return -EBUSY;

	p->cgid = cgid;
	*r = *(struct qbman_wred_query_rslt *)qbman_swp_mc_complete(s, p,
							QBMAN_WRED_QUERY);
	if (!r) {
		pr_err("qbman: Query CGID WRED %d failed, no response\n",
			cgid);
		return -EIO;
	}

	/* Decode the outcome */
	QBMAN_BUG_ON((r->verb & QBMAN_RESPONSE_VERB_MASK) != QBMAN_WRED_QUERY);

	/* Determine success or failure */
	if (r->rslt != QBMAN_MC_RSLT_OK) {
		pr_err("Query CGID WRED 0x%x failed,code=0x%02x\n",
							 cgid, r->rslt);
		return -EIO;
	}

	return 0;
}

int qbman_cgr_attr_wred_get_edp(struct qbman_wred_query_rslt *r, uint32_t idx)
{
	return (int)(r->edp[idx] & 1);
}

uint32_t qbman_cgr_attr_wred_get_parm_dp(struct qbman_wred_query_rslt *r,
					 uint32_t idx)
{
	return r->wred_parm_dp[idx];
}

void qbman_cgr_attr_wred_dp_decompose(uint32_t dp, uint64_t *minth,
				      uint64_t *maxth, uint8_t *maxp)
{
	uint8_t ma, mn, step_i, step_s, pn;

	ma = (uint8_t)(dp >> 24);
	mn = (uint8_t)(dp >> 19) & 0x1f;
	step_i = (uint8_t)(dp >> 11);
	step_s = (uint8_t)(dp >> 6) & 0x1f;
	pn = (uint8_t)dp & 0x3f;

	*maxp = (uint8_t)(((pn<<2) * 100)/256);

	if (mn == 0)
		*maxth = ma;
	else
		*maxth = ((ma+256) * (1<<(mn-1)));

	if (step_s == 0)
		*minth = *maxth - step_i;
	else
		*minth = *maxth - (256 + step_i) * (1<<(step_s - 1));
}

/* Query CGR/CCGR/CQ statistics */
struct qbman_cgr_statistics_query_desc {
	uint8_t verb;
	uint8_t reserved;
	uint16_t cgid;
	uint8_t reserved1;
	uint8_t ct;
	uint8_t reserved2[58];
};

struct qbman_cgr_statistics_query_rslt {
	uint8_t verb;
	uint8_t rslt;
	uint8_t reserved[14];
	uint64_t frm_cnt;
	uint64_t byte_cnt;
	uint32_t reserved2[8];
};

static int qbman_cgr_statistics_query(struct qbman_swp *s, uint32_t cgid,
				      int clear, uint32_t command_type,
				      uint64_t *frame_cnt, uint64_t *byte_cnt)
{
	struct qbman_cgr_statistics_query_desc *p;
	struct qbman_cgr_statistics_query_rslt *r;
	uint32_t query_verb;

	p = (struct qbman_cgr_statistics_query_desc *)qbman_swp_mc_start(s);
	if (!p)
		return -EBUSY;

	p->cgid = cgid;
	if (command_type < 2)
		p->ct = command_type;
	query_verb = clear ?
			QBMAN_CGR_STAT_QUERY_CLR : QBMAN_CGR_STAT_QUERY;
	r = (struct qbman_cgr_statistics_query_rslt *)qbman_swp_mc_complete(s,
							p, query_verb);
	if (!r) {
		pr_err("qbman: Query CGID %d statistics failed, no response\n",
			cgid);
		return -EIO;
	}

	/* Decode the outcome */
	QBMAN_BUG_ON((r->verb & QBMAN_RESPONSE_VERB_MASK) != query_verb);

	/* Determine success or failure */
	if (r->rslt != QBMAN_MC_RSLT_OK) {
		pr_err("Query statistics of CGID 0x%x failed, code=0x%02x\n",
						cgid, r->rslt);
		return -EIO;
	}

	if (*frame_cnt)
		*frame_cnt = r->frm_cnt & 0xFFFFFFFFFFllu;
	if (*byte_cnt)
		*byte_cnt = r->byte_cnt & 0xFFFFFFFFFFllu;

	return 0;
}

int qbman_cgr_reject_statistics(struct qbman_swp *s, uint32_t cgid, int clear,
				uint64_t *frame_cnt, uint64_t *byte_cnt)
{
	return qbman_cgr_statistics_query(s, cgid, clear, 0xff,
					  frame_cnt, byte_cnt);
}

int qbman_ccgr_reject_statistics(struct qbman_swp *s, uint32_t cgid, int clear,
				 uint64_t *frame_cnt, uint64_t *byte_cnt)
{
	return qbman_cgr_statistics_query(s, cgid, clear, 1,
					  frame_cnt, byte_cnt);
}

int qbman_cq_dequeue_statistics(struct qbman_swp *s, uint32_t cgid, int clear,
				uint64_t *frame_cnt, uint64_t *byte_cnt)
{
	return qbman_cgr_statistics_query(s, cgid, clear, 0,
					  frame_cnt, byte_cnt);
}

/* WQ Chan Query */
struct qbman_wqchan_query_desc {
	uint8_t verb;
	uint8_t reserved;
	uint16_t chid;
	uint8_t reserved2[60];
};

int qbman_wqchan_query(struct qbman_swp *s, uint16_t chanid,
		       struct qbman_wqchan_query_rslt *r)
{
	struct qbman_wqchan_query_desc *p;

	/* Start the management command */
	p = (struct qbman_wqchan_query_desc *)qbman_swp_mc_start(s);
	if (!p)
		return -EBUSY;

	/* Encode the caller-provided attributes */
	p->chid = chanid;

	/* Complete the management command */
	*r = *(struct qbman_wqchan_query_rslt *)qbman_swp_mc_complete(s, p,
							QBMAN_WQ_QUERY);
	if (!r) {
		pr_err("qbman: Query WQ Channel %d failed, no response\n",
			chanid);
		return -EIO;
	}

	/* Decode the outcome */
	QBMAN_BUG_ON((r->verb & QBMAN_RESPONSE_VERB_MASK) != QBMAN_WQ_QUERY);

	/* Determine success or failure */
	if (r->rslt != QBMAN_MC_RSLT_OK) {
		pr_err("Query of WQCHAN 0x%x failed, code=0x%02x\n",
		       chanid, r->rslt);
		return -EIO;
	}

	return 0;
}

uint32_t qbman_wqchan_attr_get_wqlen(struct qbman_wqchan_query_rslt *r, int wq)
{
	return r->wq_len[wq] & 0x00FFFFFF;
}

uint64_t qbman_wqchan_attr_get_cdan_ctx(struct qbman_wqchan_query_rslt *r)
{
	return r->cdan_ctx;
}

uint16_t qbman_wqchan_attr_get_cdan_wqid(struct qbman_wqchan_query_rslt *r)
{
	return r->cdan_wqid;
}

uint8_t qbman_wqchan_attr_get_ctrl(struct qbman_wqchan_query_rslt *r)
{
	return r->ctrl;
}

uint16_t qbman_wqchan_attr_get_chanid(struct qbman_wqchan_query_rslt *r)
{
	return r->chid;
}
