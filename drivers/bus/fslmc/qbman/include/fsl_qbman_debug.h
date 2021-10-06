/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2015 Freescale Semiconductor, Inc.
 * Copyright 2018-2020 NXP
 */
#ifndef _FSL_QBMAN_DEBUG_H
#define _FSL_QBMAN_DEBUG_H

#include <rte_compat.h>

struct qbman_swp;
/* Buffer pool query commands */
struct qbman_bp_query_rslt {
	uint8_t verb;
	uint8_t rslt;
	uint8_t reserved[4];
	uint8_t bdi;
	uint8_t state;
	uint32_t fill;
	uint32_t hdptr;
	uint16_t swdet;
	uint16_t swdxt;
	uint16_t hwdet;
	uint16_t hwdxt;
	uint16_t swset;
	uint16_t swsxt;
	uint16_t vbpid;
	uint16_t icid;
	uint64_t bpscn_addr;
	uint64_t bpscn_ctx;
	uint16_t hw_targ;
	uint8_t dbe;
	uint8_t reserved2;
	uint8_t sdcnt;
	uint8_t hdcnt;
	uint8_t sscnt;
	uint8_t reserved3[9];
};

int qbman_bp_query(struct qbman_swp *s, uint32_t bpid,
		   struct qbman_bp_query_rslt *r);
int qbman_bp_get_bdi(struct qbman_bp_query_rslt *r);
int qbman_bp_get_va(struct qbman_bp_query_rslt *r);
int qbman_bp_get_wae(struct qbman_bp_query_rslt *r);
uint16_t qbman_bp_get_swdet(struct qbman_bp_query_rslt  *r);
uint16_t qbman_bp_get_swdxt(struct qbman_bp_query_rslt  *r);
uint16_t qbman_bp_get_hwdet(struct qbman_bp_query_rslt  *r);
uint16_t qbman_bp_get_hwdxt(struct qbman_bp_query_rslt  *r);
uint16_t qbman_bp_get_swset(struct qbman_bp_query_rslt  *r);
uint16_t qbman_bp_get_swsxt(struct qbman_bp_query_rslt  *r);
uint16_t qbman_bp_get_vbpid(struct qbman_bp_query_rslt  *r);
uint16_t qbman_bp_get_icid(struct qbman_bp_query_rslt  *r);
int qbman_bp_get_pl(struct qbman_bp_query_rslt  *r);
uint64_t qbman_bp_get_bpscn_addr(struct qbman_bp_query_rslt  *r);
uint64_t qbman_bp_get_bpscn_ctx(struct qbman_bp_query_rslt  *r);
uint16_t qbman_bp_get_hw_targ(struct qbman_bp_query_rslt  *r);
int qbman_bp_has_free_bufs(struct qbman_bp_query_rslt  *r);
uint32_t qbman_bp_num_free_bufs(struct qbman_bp_query_rslt  *r);
int qbman_bp_is_depleted(struct qbman_bp_query_rslt  *r);
int qbman_bp_is_surplus(struct qbman_bp_query_rslt  *r);
uint32_t qbman_bp_get_hdptr(struct qbman_bp_query_rslt  *r);
uint32_t qbman_bp_get_sdcnt(struct qbman_bp_query_rslt  *r);
uint32_t qbman_bp_get_hdcnt(struct qbman_bp_query_rslt  *r);
uint32_t qbman_bp_get_sscnt(struct qbman_bp_query_rslt  *r);

/* FQ query function for programmable fields */
struct qbman_fq_query_rslt {
	uint8_t verb;
	uint8_t rslt;
	uint8_t reserved[8];
	uint16_t cgid;
	uint16_t dest_wq;
	uint8_t reserved2;
	uint8_t fq_ctrl;
	uint16_t ics_cred;
	uint16_t td_thresh;
	uint16_t oal_oac;
	uint8_t reserved3;
	uint8_t mctl;
	uint64_t fqd_ctx;
	uint16_t icid;
	uint16_t reserved4;
	uint32_t vfqid;
	uint32_t fqid_er;
	uint16_t opridsz;
	uint8_t reserved5[18];
};

int qbman_fq_query(struct qbman_swp *s, uint32_t fqid,
		   struct qbman_fq_query_rslt *r);
uint8_t qbman_fq_attr_get_fqctrl(struct qbman_fq_query_rslt *r);
uint16_t qbman_fq_attr_get_cgrid(struct qbman_fq_query_rslt *r);
uint16_t qbman_fq_attr_get_destwq(struct qbman_fq_query_rslt *r);
uint16_t qbman_fq_attr_get_tdthresh(struct qbman_fq_query_rslt *r);
int qbman_fq_attr_get_oa_ics(struct qbman_fq_query_rslt *r);
int qbman_fq_attr_get_oa_cgr(struct qbman_fq_query_rslt *r);
uint16_t qbman_fq_attr_get_oal(struct qbman_fq_query_rslt *r);
int qbman_fq_attr_get_bdi(struct qbman_fq_query_rslt *r);
int qbman_fq_attr_get_ff(struct qbman_fq_query_rslt *r);
int qbman_fq_attr_get_va(struct qbman_fq_query_rslt *r);
int qbman_fq_attr_get_ps(struct qbman_fq_query_rslt *r);
int qbman_fq_attr_get_pps(struct qbman_fq_query_rslt *r);
uint16_t qbman_fq_attr_get_icid(struct qbman_fq_query_rslt *r);
int qbman_fq_attr_get_pl(struct qbman_fq_query_rslt *r);
uint32_t qbman_fq_attr_get_vfqid(struct qbman_fq_query_rslt *r);
uint32_t qbman_fq_attr_get_erfqid(struct qbman_fq_query_rslt *r);
uint16_t qbman_fq_attr_get_opridsz(struct qbman_fq_query_rslt *r);

/* FQ query command for non-programmable fields*/
enum qbman_fq_schedstate_e {
	qbman_fq_schedstate_oos = 0,
	qbman_fq_schedstate_retired,
	qbman_fq_schedstate_tentatively_scheduled,
	qbman_fq_schedstate_truly_scheduled,
	qbman_fq_schedstate_parked,
	qbman_fq_schedstate_held_active,
};

struct qbman_fq_query_np_rslt {
uint8_t verb;
	uint8_t rslt;
	uint8_t st1;
	uint8_t st2;
	uint8_t reserved[2];
	uint16_t od1_sfdr;
	uint16_t od2_sfdr;
	uint16_t od3_sfdr;
	uint16_t ra1_sfdr;
	uint16_t ra2_sfdr;
	uint32_t pfdr_hptr;
	uint32_t pfdr_tptr;
	uint32_t frm_cnt;
	uint32_t byte_cnt;
	uint16_t ics_surp;
	uint8_t is;
	uint8_t reserved2[29];
};

__rte_internal
int qbman_fq_query_state(struct qbman_swp *s, uint32_t fqid,
			 struct qbman_fq_query_np_rslt *r);
uint8_t qbman_fq_state_schedstate(const struct qbman_fq_query_np_rslt *r);
int qbman_fq_state_force_eligible(const struct qbman_fq_query_np_rslt *r);
int qbman_fq_state_xoff(const struct qbman_fq_query_np_rslt *r);
int qbman_fq_state_retirement_pending(const struct qbman_fq_query_np_rslt *r);
int qbman_fq_state_overflow_error(const struct qbman_fq_query_np_rslt *r);
__rte_internal
uint32_t qbman_fq_state_frame_count(const struct qbman_fq_query_np_rslt *r);
uint32_t qbman_fq_state_byte_count(const struct qbman_fq_query_np_rslt *r);

/* CGR query */
struct qbman_cgr_query_rslt {
	uint8_t verb;
	uint8_t rslt;
	uint8_t reserved[6];
	uint8_t ctl1;
	uint8_t reserved1;
	uint16_t oal;
	uint16_t reserved2;
	uint8_t mode;
	uint8_t ctl2;
	uint8_t iwc;
	uint8_t tdc;
	uint16_t cs_thres;
	uint16_t cs_thres_x;
	uint16_t td_thres;
	uint16_t cscn_tdcp;
	uint16_t cscn_wqid;
	uint16_t cscn_vcgid;
	uint16_t cg_icid;
	uint64_t cg_wr_addr;
	uint64_t cscn_ctx;
	uint64_t i_cnt;
	uint64_t a_cnt;
};

int qbman_cgr_query(struct qbman_swp *s, uint32_t cgid,
		    struct qbman_cgr_query_rslt *r);
int qbman_cgr_get_cscn_wq_en_enter(struct qbman_cgr_query_rslt *r);
int qbman_cgr_get_cscn_wq_en_exit(struct qbman_cgr_query_rslt *r);
int qbman_cgr_get_cscn_wq_icd(struct qbman_cgr_query_rslt *r);
uint8_t qbman_cgr_get_mode(struct qbman_cgr_query_rslt *r);
int qbman_cgr_get_rej_cnt_mode(struct qbman_cgr_query_rslt *r);
int qbman_cgr_get_cscn_bdi(struct qbman_cgr_query_rslt *r);
uint16_t qbman_cgr_attr_get_cs_thres(struct qbman_cgr_query_rslt *r);
uint16_t qbman_cgr_attr_get_cs_thres_x(struct qbman_cgr_query_rslt *r);
uint16_t qbman_cgr_attr_get_td_thres(struct qbman_cgr_query_rslt *r);

/* WRED query */
struct qbman_wred_query_rslt {
	uint8_t verb;
	uint8_t rslt;
	uint8_t reserved[6];
	uint8_t edp[7];
	uint8_t reserved1;
	uint32_t wred_parm_dp[7];
	uint8_t reserved2[20];
};

int qbman_cgr_wred_query(struct qbman_swp *s, uint32_t cgid,
			 struct qbman_wred_query_rslt *r);
int qbman_cgr_attr_wred_get_edp(struct qbman_wred_query_rslt *r, uint32_t idx);
void qbman_cgr_attr_wred_dp_decompose(uint32_t dp, uint64_t *minth,
				      uint64_t *maxth, uint8_t *maxp);
uint32_t qbman_cgr_attr_wred_get_parm_dp(struct qbman_wred_query_rslt *r,
					 uint32_t idx);

/* CGR/CCGR/CQ statistics query */
int qbman_cgr_reject_statistics(struct qbman_swp *s, uint32_t cgid, int clear,
				uint64_t *frame_cnt, uint64_t *byte_cnt);
int qbman_ccgr_reject_statistics(struct qbman_swp *s, uint32_t cgid, int clear,
				 uint64_t *frame_cnt, uint64_t *byte_cnt);
int qbman_cq_dequeue_statistics(struct qbman_swp *s, uint32_t cgid, int clear,
				uint64_t *frame_cnt, uint64_t *byte_cnt);

/* Query Work Queue Channel */
struct qbman_wqchan_query_rslt {
	uint8_t verb;
	uint8_t rslt;
	uint16_t chid;
	uint8_t reserved;
	uint8_t ctrl;
	uint16_t cdan_wqid;
	uint64_t cdan_ctx;
	uint32_t reserved2[4];
	uint32_t wq_len[8];
};

int qbman_wqchan_query(struct qbman_swp *s, uint16_t chanid,
		       struct qbman_wqchan_query_rslt *r);
uint32_t qbman_wqchan_attr_get_wqlen(struct qbman_wqchan_query_rslt *r, int wq);
uint64_t qbman_wqchan_attr_get_cdan_ctx(struct qbman_wqchan_query_rslt *r);
uint16_t qbman_wqchan_attr_get_cdan_wqid(struct qbman_wqchan_query_rslt *r);
uint8_t qbman_wqchan_attr_get_ctrl(struct qbman_wqchan_query_rslt *r);
uint16_t qbman_wqchan_attr_get_chanid(struct qbman_wqchan_query_rslt *r);
#endif /* !_FSL_QBMAN_DEBUG_H */
