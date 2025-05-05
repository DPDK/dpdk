/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Napatech A/S
 */

#ifndef _HW_MOD_FLM_V25_H_
#define _HW_MOD_FLM_V25_H_

#include <stdint.h>

/* SCRUB record constraints */
#define SCRUB_T_MAX 0xEF/* MAX encoded timeout value, approx. 127 years */
#define SCRUB_DEL                                                                                 \
	0	/* Indicates if flow should be deleted upon timeout. If DEL=0 then the flow is \
		 * marked as aged rather than being deleted. \
		 */
#define SCRUB_INF                                                                                 \
	1	/* Indicates if a flow info record should be generated upon timeout. \
		 * If INF=1 a flow info record will be generated even for a stateless flow. \
		 */

struct flm_v25_control_s {
	uint32_t enable;
	uint32_t init;
	uint32_t lds;
	uint32_t lfs;
	uint32_t lis;
	uint32_t uds;
	uint32_t uis;
	uint32_t rds;
	uint32_t ris;
	uint32_t pds;
	uint32_t pis;
	uint32_t crcwr;
	uint32_t crcrd;
	uint32_t rbl;
	uint32_t eab;
	uint32_t split_sdram_usage;
};

struct flm_v25_status_s {
	uint32_t calib_success;
	uint32_t calib_fail;
	uint32_t initdone;
	uint32_t idle;
	uint32_t critical;
	uint32_t panic;
	uint32_t crcerr;
	uint32_t eft_bp;
	uint32_t cache_buf_critical;
};

struct flm_v25_load_bin_s {
	uint32_t bin;
};

struct flm_v25_load_pps_s {
	uint32_t pps;
};

struct flm_v25_load_lps_s {
	uint32_t lps;
};

struct flm_v25_load_aps_s {
	uint32_t aps;
};

struct flm_v25_prio_s {
	uint32_t limit0;
	uint32_t ft0;
	uint32_t limit1;
	uint32_t ft1;
	uint32_t limit2;
	uint32_t ft2;
	uint32_t limit3;
	uint32_t ft3;
};

struct flm_v25_pst_s {
	uint32_t bp;
	uint32_t pp;
	uint32_t tp;
};

struct flm_v25_rcp_s {
	uint32_t lookup;
	uint32_t qw0_dyn;
	uint32_t qw0_ofs;
	uint32_t qw0_sel;
	uint32_t qw4_dyn;
	uint32_t qw4_ofs;
	uint32_t sw8_dyn;
	uint32_t sw8_ofs;
	uint32_t sw8_sel;
	uint32_t sw9_dyn;
	uint32_t sw9_ofs;
	uint32_t mask[10];
	uint32_t kid;
	uint32_t opn;
	uint32_t ipn;
	uint32_t byt_dyn;
	uint32_t byt_ofs;
	uint32_t txplm;
	uint32_t auto_ipv4_mask;
};

struct flm_v25_buf_ctrl_s {
	uint32_t lrn_free;
	uint32_t inf_avail;
	uint32_t sta_avail;
};

struct flm_v25_stat_lrn_done_s {
	uint32_t cnt;
};

struct flm_v25_stat_lrn_ignore_s {
	uint32_t cnt;
};

struct flm_v25_stat_lrn_fail_s {
	uint32_t cnt;
};

struct flm_v25_stat_unl_done_s {
	uint32_t cnt;
};

struct flm_v25_stat_unl_ignore_s {
	uint32_t cnt;
};

struct flm_v25_stat_rel_done_s {
	uint32_t cnt;
};

struct flm_v25_stat_rel_ignore_s {
	uint32_t cnt;
};

struct flm_v25_stat_aul_done_s {
	uint32_t cnt;
};

struct flm_v25_stat_aul_ignore_s {
	uint32_t cnt;
};

struct flm_v25_stat_aul_fail_s {
	uint32_t cnt;
};

struct flm_v25_stat_tul_done_s {
	uint32_t cnt;
};

struct flm_v25_stat_flows_s {
	uint32_t cnt;
};

struct flm_v25_stat_prb_done_s {
	uint32_t cnt;
};

struct flm_v25_stat_prb_ignore_s {
	uint32_t cnt;
};

struct flm_v25_stat_sta_done_s {
	uint32_t cnt;
};

struct flm_v25_stat_inf_done_s {
	uint32_t cnt;
};

struct flm_v25_stat_inf_skip_s {
	uint32_t cnt;
};

struct flm_v25_stat_pck_hit_s {
	uint32_t cnt;
};

struct flm_v25_stat_pck_miss_s {
	uint32_t cnt;
};

struct flm_v25_stat_pck_unh_s {
	uint32_t cnt;
};

struct flm_v25_stat_pck_dis_s {
	uint32_t cnt;
};

struct flm_v25_stat_csh_hit_s {
	uint32_t cnt;
};

struct flm_v25_stat_csh_miss_s {
	uint32_t cnt;
};

struct flm_v25_stat_csh_unh_s {
	uint32_t cnt;
};

struct flm_v25_stat_cuc_start_s {
	uint32_t cnt;
};

struct flm_v25_stat_cuc_move_s {
	uint32_t cnt;
};

struct flm_v25_scan_s {
	uint32_t i;
};

struct flm_v25_scrub_s {
	uint8_t t;
	uint8_t r;
	uint8_t del;
	uint8_t inf;
};

struct __rte_packed_begin flm_v25_lrn_data_s {
	uint32_t sw9;	/* 31:0 (32) */
	uint32_t sw8;	/* 63:32 (32) */
	uint32_t qw4[4];/* 191:64 (128) */
	uint32_t qw0[4];/* 319:192 (128) */
	uint8_t prot;	/* 327:320 (8) */
	uint8_t kid;	/* 335:328 (8) */
	uint32_t nat_ip;/* 367:336 (32) */
	uint32_t teid;	/* 399:368 (32) */
	uint16_t nat_port;	/* 415:400 (16) */
	uint16_t rate;	/* 431:416 (16) */
	uint16_t size;	/* 447:432 (16) */
	uint32_t color;	/* 479:448 (32) */
	uint32_t adj;	/* 511:480 (32) */
	uint32_t id;	/* 543:512 (32) */
	uint16_t fill;	/* 559:544 (16) */

	/*
	 * Bit fields
	 */
	uint64_t ft : 4;/* 563:560 (4) */
	uint64_t ft_mbr : 4;	/* 567:564 (4) */
	uint64_t ft_miss : 4;	/* 571:568 (4) */
	uint64_t mbr_idx1 : 28;	/* 599:572 (28) */
	uint64_t mbr_idx2 : 28;	/* 627:600 (28) */
	uint64_t mbr_idx3 : 28;	/* 655:628 (28) */
	uint64_t mbr_idx4 : 28;	/* 683:656 (28) */
	uint64_t vol_idx : 3;	/* 686:684 (3) */
	uint64_t stat_prof : 4;	/* 690:687 (4) */
	uint64_t prio : 2;	/* 692:691 (2) */
	uint64_t ent : 1;	/* 693:693 (1) */
	uint64_t op : 4;/* 697:694 (4) */
	uint64_t dscp : 6;	/* 703:698 (6) */
	uint64_t qfi : 6;	/* 709:704 (6) */
	uint64_t rqi : 1;	/* 710:710 (1) */
	uint64_t nat_en : 1;	/* 711:711 (1) */
	uint64_t scrub_prof : 4;/* 715:712 (4) */
	uint64_t nofi : 1;	/* 716:716 (1) */
	uint64_t pad : 50;	/* 766:717 (50) */
	uint64_t eor : 1;	/* 767:767 (1) */
} __rte_packed_end;

struct __rte_packed_begin flm_v25_inf_data_s {
	uint64_t bytes;
	uint64_t packets;
	uint64_t ts;
	uint32_t id;
	uint64_t cause : 3;
	uint64_t pad : 60;
	uint64_t eor : 1;
} __rte_packed_end;

struct __rte_packed_begin flm_v25_sta_data_s {
	uint32_t id;
	uint64_t lds : 1;
	uint64_t lfs : 1;
	uint64_t lis : 1;
	uint64_t uds : 1;
	uint64_t uis : 1;
	uint64_t rds : 1;
	uint64_t ris : 1;
	uint64_t pds : 1;
	uint64_t pis : 1;
	uint64_t pad : 54;
	uint64_t eor : 1;
} __rte_packed_end;

struct hw_mod_flm_v25_s {
	struct flm_v25_control_s *control;
	struct flm_v25_status_s *status;
	struct flm_v25_load_bin_s *load_bin;
	struct flm_v25_load_pps_s *load_pps;
	struct flm_v25_load_lps_s *load_lps;
	struct flm_v25_load_aps_s *load_aps;
	struct flm_v25_prio_s *prio;
	struct flm_v25_pst_s *pst;
	struct flm_v25_rcp_s *rcp;
	struct flm_v25_buf_ctrl_s *buf_ctrl;
	/* lrn_data is not handled by struct */
	/* inf_data is not handled by struct */
	/* sta_data is not handled by struct */
	struct flm_v25_stat_lrn_done_s *lrn_done;
	struct flm_v25_stat_lrn_ignore_s *lrn_ignore;
	struct flm_v25_stat_lrn_fail_s *lrn_fail;
	struct flm_v25_stat_unl_done_s *unl_done;
	struct flm_v25_stat_unl_ignore_s *unl_ignore;
	struct flm_v25_stat_rel_done_s *rel_done;
	struct flm_v25_stat_rel_ignore_s *rel_ignore;
	struct flm_v25_stat_aul_done_s *aul_done;
	struct flm_v25_stat_aul_ignore_s *aul_ignore;
	struct flm_v25_stat_aul_fail_s *aul_fail;
	struct flm_v25_stat_tul_done_s *tul_done;
	struct flm_v25_stat_flows_s *flows;
	struct flm_v25_stat_prb_done_s *prb_done;
	struct flm_v25_stat_prb_ignore_s *prb_ignore;
	struct flm_v25_stat_sta_done_s *sta_done;
	struct flm_v25_stat_inf_done_s *inf_done;
	struct flm_v25_stat_inf_skip_s *inf_skip;
	struct flm_v25_stat_pck_hit_s *pck_hit;
	struct flm_v25_stat_pck_miss_s *pck_miss;
	struct flm_v25_stat_pck_unh_s *pck_unh;
	struct flm_v25_stat_pck_dis_s *pck_dis;
	struct flm_v25_stat_csh_hit_s *csh_hit;
	struct flm_v25_stat_csh_miss_s *csh_miss;
	struct flm_v25_stat_csh_unh_s *csh_unh;
	struct flm_v25_stat_cuc_start_s *cuc_start;
	struct flm_v25_stat_cuc_move_s *cuc_move;
	struct flm_v25_scan_s *scan;
	struct flm_v25_scrub_s *scrub;
};

#endif	/* _HW_MOD_FLM_V25_H_ */
