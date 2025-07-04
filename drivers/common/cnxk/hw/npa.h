/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef __NPA_HW_H__
#define __NPA_HW_H__

/* Register offsets */

#define NPA_AF_BLK_RST		     (0x0ull)
#define NPA_AF_CONST		     (0x10ull)
#define NPA_AF_CONST1		     (0x18ull)
#define NPA_AF_LF_RST		     (0x20ull)
#define NPA_AF_GEN_CFG		     (0x30ull)
#define NPA_AF_NDC_CFG		     (0x40ull)
#define NPA_AF_NDC_SYNC		     (0x50ull)
#define NPA_AF_INP_CTL		     (0xd0ull)
#define NPA_AF_ACTIVE_CYCLES_PC	     (0xf0ull)
#define NPA_AF_AVG_DELAY	     (0x100ull)
#define NPA_AF_GEN_INT		     (0x140ull)
#define NPA_AF_GEN_INT_W1S	     (0x148ull)
#define NPA_AF_GEN_INT_ENA_W1S	     (0x150ull)
#define NPA_AF_GEN_INT_ENA_W1C	     (0x158ull)
#define NPA_AF_RVU_INT		     (0x160ull)
#define NPA_AF_RVU_INT_W1S	     (0x168ull)
#define NPA_AF_RVU_INT_ENA_W1S	     (0x170ull)
#define NPA_AF_RVU_INT_ENA_W1C	     (0x178ull)
#define NPA_AF_ERR_INT		     (0x180ull)
#define NPA_AF_ERR_INT_W1S	     (0x188ull)
#define NPA_AF_ERR_INT_ENA_W1S	     (0x190ull)
#define NPA_AF_ERR_INT_ENA_W1C	     (0x198ull)
#define NPA_AF_RAS		     (0x1a0ull)
#define NPA_AF_RAS_W1S		     (0x1a8ull)
#define NPA_AF_RAS_ENA_W1S	     (0x1b0ull)
#define NPA_AF_RAS_ENA_W1C	     (0x1b8ull)
#define NPA_AF_AQ_CFG		     (0x600ull)
#define NPA_AF_AQ_BASE		     (0x610ull)
#define NPA_AF_AQ_STATUS	     (0x620ull)
#define NPA_AF_AQ_DOOR		     (0x630ull)
#define NPA_AF_AQ_DONE_WAIT	     (0x640ull)
#define NPA_AF_AQ_DONE		     (0x650ull)
#define NPA_AF_AQ_DONE_ACK	     (0x660ull)
#define NPA_AF_AQ_DONE_TIMER	     (0x670ull)
#define NPA_AF_AQ_DONE_INT	     (0x680ull)
#define NPA_AF_AQ_DONE_ENA_W1S	     (0x690ull)
#define NPA_AF_AQ_DONE_ENA_W1C	     (0x698ull)
#define NPA_AF_BATCH_CTL	     (0x6a0ull) /* [CN10K, .) */
#define NPA_AF_BATCH_ACCEPT_CTL	     (0x6a8ull) /* [CN10K, .) */
#define NPA_AF_BATCH_ERR_DATA0	     (0x6c0ull) /* [CN10K, .) */
#define NPA_AF_BATCH_ERR_DATA1	     (0x6c8ull) /* [CN10K, .) */
#define NPA_AF_LFX_AURAS_CFG(a)	     (0x4000ull | (uint64_t)(a) << 18)
#define NPA_AF_LFX_LOC_AURAS_BASE(a) (0x4010ull | (uint64_t)(a) << 18)
#define NPA_AF_LFX_QINTS_CFG(a)	     (0x4100ull | (uint64_t)(a) << 18)
#define NPA_AF_LFX_QINTS_BASE(a)     (0x4110ull | (uint64_t)(a) << 18)
#define NPA_PRIV_AF_INT_CFG	     (0x10000ull)
#define NPA_PRIV_LFX_CFG(a)	     (0x10010ull | (uint64_t)(a) << 8)
#define NPA_PRIV_LFX_INT_CFG(a)	     (0x10020ull | (uint64_t)(a) << 8)
#define NPA_AF_RVU_LF_CFG_DEBUG	     (0x10030ull)
#define NPA_AF_DTX_FILTER_CTL	     (0x10040ull)

#define NPA_LF_AURA_OP_ALLOCX(a)  (0x10ull | (uint64_t)(a) << 3)
#define NPA_LF_AURA_OP_FREE0	  (0x20ull)
#define NPA_LF_AURA_OP_FREE1	  (0x28ull)
#define NPA_LF_AURA_OP_CNT	  (0x30ull)
#define NPA_LF_AURA_OP_LIMIT	  (0x50ull)
#define NPA_LF_AURA_OP_INT	  (0x60ull)
#define NPA_LF_AURA_OP_THRESH	  (0x70ull)
#define NPA_LF_POOL_OP_PC	  (0x100ull)
#define NPA_LF_POOL_OP_AVAILABLE  (0x110ull)
#define NPA_LF_POOL_OP_PTR_START0 (0x120ull)
#define NPA_LF_POOL_OP_PTR_START1 (0x128ull)
#define NPA_LF_POOL_OP_PTR_END0	  (0x130ull)
#define NPA_LF_POOL_OP_PTR_END1	  (0x138ull)
#define NPA_LF_POOL_OP_INT	  (0x160ull)
#define NPA_LF_POOL_OP_THRESH	  (0x170ull)
#define NPA_LF_ERR_INT		  (0x200ull)
#define NPA_LF_ERR_INT_W1S	  (0x208ull)
#define NPA_LF_ERR_INT_ENA_W1C	  (0x210ull)
#define NPA_LF_ERR_INT_ENA_W1S	  (0x218ull)
#define NPA_LF_RAS		  (0x220ull)
#define NPA_LF_RAS_W1S		  (0x228ull)
#define NPA_LF_RAS_ENA_W1C	  (0x230ull)
#define NPA_LF_RAS_ENA_W1S	  (0x238ull)
#define NPA_LF_QINTX_CNT(a)	  (0x300ull | (uint64_t)(a) << 12)
#define NPA_LF_QINTX_INT(a)	  (0x310ull | (uint64_t)(a) << 12)
#define NPA_LF_QINTX_ENA_W1S(a)	  (0x320ull | (uint64_t)(a) << 12)
#define NPA_LF_QINTX_ENA_W1C(a)	  (0x330ull | (uint64_t)(a) << 12)
#define NPA_LF_AURA_BATCH_ALLOC	  (0x340ull) /* [CN10K, .) */
#define NPA_LF_AURA_BATCH_FREE0	  (0x400ull) /* [CN10K, .) */
#define NPA_LF_AURA_BATCH_FREEX(a)                                             \
	(0x400ull | (uint64_t)(a) << 3) /* [CN10K, .) */

/* Enum offsets */

#define NPA_AF_BATCH_FAIL_BATCH_PASS	   (0x0ull) /* [CN10K, .) */
#define NPA_AF_BATCH_FAIL_BATCH_CNT_OOR	   (0x1ull) /* [CN10K, .) */
#define NPA_AF_BATCH_FAIL_BATCH_STORE_FAIL (0x2ull) /* [CN10K, .) */

#define NPA_AQ_COMP_NOTDONE    (0x0ull)
#define NPA_AQ_COMP_GOOD       (0x1ull)
#define NPA_AQ_COMP_SWERR      (0x2ull)
#define NPA_AQ_COMP_CTX_POISON (0x3ull)
#define NPA_AQ_COMP_CTX_FAULT  (0x4ull)
#define NPA_AQ_COMP_LOCKERR    (0x5ull)

#define NPA_AF_INT_VEC_RVU     (0x0ull)
#define NPA_AF_INT_VEC_GEN     (0x1ull)
#define NPA_AF_INT_VEC_AQ_DONE (0x2ull)
#define NPA_AF_INT_VEC_AF_ERR  (0x3ull)
#define NPA_AF_INT_VEC_POISON  (0x4ull)

#define NPA_AQ_INSTOP_NOP    (0x0ull)
#define NPA_AQ_INSTOP_INIT   (0x1ull)
#define NPA_AQ_INSTOP_WRITE  (0x2ull)
#define NPA_AQ_INSTOP_READ   (0x3ull)
#define NPA_AQ_INSTOP_LOCK   (0x4ull)
#define NPA_AQ_INSTOP_UNLOCK (0x5ull)

#define NPA_AQ_CTYPE_AURA (0x0ull)
#define NPA_AQ_CTYPE_POOL (0x1ull)

#define NPA_BPINTF_NIX0_RX (0x0ull)
#define NPA_BPINTF_NIX1_RX (0x1ull)

#define NPA_AURA_ERR_INT_AURA_FREE_UNDER (0x0ull)
#define NPA_AURA_ERR_INT_AURA_ADD_OVER	 (0x1ull)
#define NPA_AURA_ERR_INT_AURA_ADD_UNDER	 (0x2ull)
#define NPA_AURA_ERR_INT_POOL_DIS	 (0x3ull)
#define NPA_AURA_ERR_INT_R4		 (0x4ull)
#define NPA_AURA_ERR_INT_R5		 (0x5ull)
#define NPA_AURA_ERR_INT_R6		 (0x6ull)
#define NPA_AURA_ERR_INT_R7		 (0x7ull)

#define NPA_LF_INT_VEC_ERR_INT	  (0x40ull)
#define NPA_LF_INT_VEC_POISON	  (0x41ull)
#define NPA_LF_INT_VEC_QINT_END	  (0x3full)
#define NPA_LF_INT_VEC_QINT_START (0x0ull)

#define NPA_INPQ_SSO	      (0x4ull)
#define NPA_INPQ_TIM	      (0x5ull)
#define NPA_INPQ_DPI	      (0x6ull)
#define NPA_INPQ_AURA_OP      (0xeull)
#define NPA_INPQ_INTERNAL_RSV (0xfull)
#define NPA_INPQ_NIX0_RX      (0x0ull)
#define NPA_INPQ_NIX1_RX      (0x2ull)
#define NPA_INPQ_NIX0_TX      (0x1ull)
#define NPA_INPQ_NIX1_TX      (0x3ull)
#define NPA_INPQ_R_END	      (0xdull)
#define NPA_INPQ_R_START      (0x7ull)

#define NPA_POOL_ERR_INT_OVFLS (0x0ull)
#define NPA_POOL_ERR_INT_RANGE (0x1ull)
#define NPA_POOL_ERR_INT_PERR  (0x2ull)
#define NPA_POOL_ERR_INT_R3    (0x3ull)
#define NPA_POOL_ERR_INT_R4    (0x4ull)
#define NPA_POOL_ERR_INT_R5    (0x5ull)
#define NPA_POOL_ERR_INT_R6    (0x6ull)
#define NPA_POOL_ERR_INT_R7    (0x7ull)

#define NPA_NDC0_PORT_AURA0  (0x0ull)
#define NPA_NDC0_PORT_AURA1  (0x1ull)
#define NPA_NDC0_PORT_POOL0  (0x2ull)
#define NPA_NDC0_PORT_POOL1  (0x3ull)
#define NPA_NDC0_PORT_STACK0 (0x4ull)
#define NPA_NDC0_PORT_STACK1 (0x5ull)

#define NPA_LF_ERR_INT_AURA_DIS	   (0x0ull)
#define NPA_LF_ERR_INT_AURA_OOR	   (0x1ull)
#define NPA_LF_ERR_INT_AURA_FAULT  (0xcull)
#define NPA_LF_ERR_INT_POOL_FAULT  (0xdull)
#define NPA_LF_ERR_INT_STACK_FAULT (0xeull)
#define NPA_LF_ERR_INT_QINT_FAULT  (0xfull)

#define NPA_BATCH_ALLOC_RESULT_ACCEPTED	   (0x0ull) /* [CN10K, .) */
#define NPA_BATCH_ALLOC_RESULT_WAIT	   (0x1ull) /* [CN10K, .) */
#define NPA_BATCH_ALLOC_RESULT_ERR	   (0x2ull) /* [CN10K, .) */
#define NPA_BATCH_ALLOC_RESULT_NOCORE	   (0x3ull) /* [CN10K, .) */
#define NPA_BATCH_ALLOC_RESULT_NOCORE_WAIT (0x4ull) /* [CN10K, .) */

#define NPA_BATCH_ALLOC_CCODE_INVAL    (0x0ull) /* [CN10K, .) */
#define NPA_BATCH_ALLOC_CCODE_VAL      (0x1ull) /* [CN10K, .) */
#define NPA_BATCH_ALLOC_CCODE_VAL_NULL (0x2ull) /* [CN10K, .) */

#define NPA_INPQ_ENAS_REMOTE_PORT   (0x0ull)   /* [CN10K, .) */
#define NPA_INPQ_ENAS_RESP_DISABLE  (0x702ull) /* [CN10K, .) */
#define NPA_INPQ_ENAS_NOTIF_DISABLE (0x7cfull) /* [CN10K, .) */

/* Structures definitions */

/* NPA admin queue instruction structure */
struct npa_aq_inst_s {
	uint64_t op : 4;
	uint64_t ctype : 4;
	uint64_t lf : 9;
	uint64_t rsvd_23_17 : 7;
	uint64_t cindex : 20;
	uint64_t rsvd_62_44 : 19;
	uint64_t doneint : 1;
	uint64_t res_addr : 64; /* W1 */
};

/* NPA admin queue result structure */
struct npa_aq_res_s {
	uint64_t op : 4;
	uint64_t ctype : 4;
	uint64_t compcode : 8;
	uint64_t doneint : 1;
	uint64_t rsvd_63_17 : 47;
	uint64_t rsvd_127_64 : 64; /* W1 */
};

/* NPA aura operation write data structure */
struct npa_aura_op_wdata_s {
	uint64_t aura : 20;
	uint64_t rsvd_62_20 : 43;
	uint64_t drop : 1;
};

/* NPA aura context structure [CN9K, CN10K] */
struct npa_aura_s {
	uint64_t pool_addr : 64; /* W0 */
	uint64_t ena : 1; /* W1 */
	uint64_t rsvd_66_65 : 2;
	uint64_t pool_caching : 1;
	uint64_t pool_way_mask : 16;
	uint64_t avg_con : 9;
	uint64_t rsvd_93 : 1;
	uint64_t pool_drop_ena : 1;
	uint64_t aura_drop_ena : 1;
	uint64_t bp_ena : 2;
	uint64_t rsvd_103_98 : 6;
	uint64_t aura_drop : 8;
	uint64_t shift : 6;
	uint64_t rsvd_119_118 : 2;
	uint64_t avg_level : 8;
	uint64_t count : 36; /* W2 */
	uint64_t rsvd_167_164 : 4;
	uint64_t nix0_bpid : 9;
	uint64_t rsvd_179_177 : 3;
	uint64_t nix1_bpid : 9;
	uint64_t rsvd_191_189 : 3;
	uint64_t limit : 36; /* W3 */
	uint64_t rsvd_231_228 : 4;
	uint64_t bp : 8;
	uint64_t rsvd_242_240 : 3;
	uint64_t fc_be : 1; /* [CN10K] */
	uint64_t fc_ena : 1;
	uint64_t fc_up_crossing : 1;
	uint64_t fc_stype : 2;
	uint64_t fc_hyst_bits : 4;
	uint64_t rsvd_255_252 : 4;
	uint64_t fc_addr : 64; /* W4 */
	uint64_t pool_drop : 8; /* W5 */
	uint64_t update_time : 16;
	uint64_t err_int : 8;
	uint64_t err_int_ena : 8;
	uint64_t thresh_int : 1;
	uint64_t thresh_int_ena : 1;
	uint64_t thresh_up : 1;
	uint64_t rsvd_363 : 1;
	uint64_t thresh_qint_idx : 7;
	uint64_t rsvd_371 : 1;
	uint64_t err_qint_idx : 7;
	uint64_t rsvd_383_379 : 5;
	uint64_t thresh : 36; /* W6 */
	uint64_t rsvd_423_420 : 4;
	uint64_t fc_msh_dst : 11; /* [CN10K] */
	uint64_t rsvd_447_435 : 13;
	uint64_t rsvd_511_448 : 64; /* W7 */
};

/* NPA pool context structure [CN9K, CN10K] */
struct npa_pool_s {
	uint64_t stack_base : 64; /* W0 */
	uint64_t ena : 1; /* W1 */
	uint64_t nat_align : 1;
	uint64_t rsvd_67_66 : 2;
	uint64_t stack_caching : 1;
	uint64_t rsvd_71_69 : 3;
	uint64_t stack_way_mask : 16;
	uint64_t buf_offset : 12;
	uint64_t rsvd_103_100 : 4;
	uint64_t buf_size : 11;
	uint64_t rsvd_127_115 : 13;
	uint64_t stack_max_pages : 32; /* W2 */
	uint64_t stack_pages : 32;
	uint64_t op_pc : 48; /* W3 */
	uint64_t rsvd_255_240 : 16;
	uint64_t stack_offset : 4; /* W4 */
	uint64_t rsvd_263_260 : 4;
	uint64_t shift : 6;
	uint64_t rsvd_271_270 : 2;
	uint64_t avg_level : 8;
	uint64_t avg_con : 9;
	uint64_t fc_ena : 1;
	uint64_t fc_stype : 2;
	uint64_t fc_hyst_bits : 4;
	uint64_t fc_up_crossing : 1;
	uint64_t fc_be : 1; /* [CN10K] */
	uint64_t rsvd_299_298 : 2;
	uint64_t update_time : 16;
	uint64_t rsvd_319_316 : 4;
	uint64_t fc_addr : 64;	 /* W5 */
	uint64_t ptr_start : 64; /* W6 */
	uint64_t ptr_end : 64;	 /* W7 */
	uint64_t rsvd_535_512 : 24; /* W8 */
	uint64_t err_int : 8;
	uint64_t err_int_ena : 8;
	uint64_t thresh_int : 1;
	uint64_t thresh_int_ena : 1;
	uint64_t thresh_up : 1;
	uint64_t rsvd_555 : 1;
	uint64_t thresh_qint_idx : 7;
	uint64_t rsvd_563 : 1;
	uint64_t err_qint_idx : 7;
	uint64_t rsvd_575_571 : 5;
	uint64_t thresh : 36; /* W9 */
	uint64_t rsvd_615_612 : 4;
	uint64_t fc_msh_dst : 11; /* [CN10K] */
	uint64_t rsvd_639_627 : 13;
	uint64_t rsvd_703_640 : 64;  /* W10 */
	uint64_t rsvd_767_704 : 64;  /* W11 */
	uint64_t rsvd_831_768 : 64;  /* W12 */
	uint64_t rsvd_895_832 : 64;  /* W13 */
	uint64_t rsvd_959_896 : 64;  /* W14 */
	uint64_t rsvd_1023_960 : 64; /* W15 */
};

/* NPA aura context structure [CN20K] */
struct npa_cn20k_aura_s {
	uint64_t pool_addr : 64; /* W0 */
	uint64_t ena : 1;   /* W1 */
	uint64_t rsvd_66_65 : 2;
	uint64_t pool_caching : 1;
	uint64_t rsvd_68 : 16;
	uint64_t avg_con : 9;
	uint64_t rsvd_93 : 1;
	uint64_t pool_drop_ena : 1;
	uint64_t aura_drop_ena : 1;
	uint64_t bp_ena : 1;
	uint64_t rsvd_103_97 : 7;
	uint64_t aura_drop : 8;
	uint64_t shift : 6;
	uint64_t rsvd_119_118 : 2;
	uint64_t avg_level : 8;
	uint64_t count : 36; /* W2 */
	uint64_t rsvd_167_164 : 4;
	uint64_t bpid : 12;
	uint64_t rsvd_191_180 : 12;
	uint64_t limit : 36; /* W3 */
	uint64_t rsvd_231_228 : 4;
	uint64_t bp : 7;
	uint64_t rsvd_243_239 : 5;
	uint64_t fc_ena : 1;
	uint64_t fc_up_crossing : 1;
	uint64_t fc_stype : 2;
	uint64_t fc_hyst_bits : 4;
	uint64_t rsvd_255_252 : 4;
	uint64_t fc_addr : 64;  /* W4 */
	uint64_t pool_drop : 8; /* W5 */
	uint64_t update_time : 16;
	uint64_t err_int : 8;
	uint64_t err_int_ena : 8;
	uint64_t thresh_int : 1;
	uint64_t thresh_int_ena : 1;
	uint64_t thresh_up : 1;
	uint64_t rsvd_363 : 1;
	uint64_t thresh_qint_idx : 7;
	uint64_t rsvd_371 : 1;
	uint64_t err_qint_idx : 7;
	uint64_t rsvd_383_379 : 5;
	uint64_t thresh : 36; /* W6*/
	uint64_t rsvd_423_420 : 4;
	uint64_t fc_msh_dst : 11;
	uint64_t rsvd_438_435 : 4;
	uint64_t op_dpc_ena : 1;
	uint64_t op_dpc_set : 6;
	uint64_t stream_ctx : 1;
	uint64_t unified_ctx : 1;
	uint64_t rsvd_511_448 : 64; /* W7 */
};

/* NPA pool context structure [CN20K] */
struct npa_cn20k_pool_s {
	uint64_t stack_base : 64; /* W0 */
	uint64_t ena : 1; /* W1 */
	uint64_t nat_align : 1;
	uint64_t rsvd_67_66 : 2;
	uint64_t stack_caching : 1;
	uint64_t rsvd_87_69 : 19;
	uint64_t buf_offset : 12;
	uint64_t rsvd_103_100 : 4;
	uint64_t buf_size : 12;
	uint64_t rsvd_119_116 : 4;
	uint64_t ref_cnt_prof : 3;
	uint64_t rsvd_127_123 : 5;
	uint64_t stack_max_pages : 32; /* W2 */
	uint64_t stack_pages : 32;
	uint64_t bp_0 : 7; /* W3 */
	uint64_t bp_1 : 7;
	uint64_t bp_2 : 7;
	uint64_t bp_3 : 7;
	uint64_t bp_4 : 7;
	uint64_t bp_5 : 7;
	uint64_t bp_6 : 7;
	uint64_t bp_7 : 7;
	uint64_t bp_ena_0 : 1;
	uint64_t bp_ena_1 : 1;
	uint64_t bp_ena_2 : 1;
	uint64_t bp_ena_3 : 1;
	uint64_t bp_ena_4 : 1;
	uint64_t bp_ena_5 : 1;
	uint64_t bp_ena_6 : 1;
	uint64_t bp_ena_7 : 1;
	uint64_t stack_offset : 4; /* W4 */
	uint64_t rsvd_263_260 : 4;
	uint64_t shift : 6;
	uint64_t rsvd_271_270 : 2;
	uint64_t avg_level : 8;
	uint64_t avg_con : 9;
	uint64_t fc_ena : 1;
	uint64_t fc_stype : 2;
	uint64_t fc_hyst_bits : 4;
	uint64_t fc_up_crossing : 1;
	uint64_t rsvd_299_297 : 3;
	uint64_t update_time : 16;
	uint64_t rsvd_319_316 : 4;
	uint64_t fc_addr : 64;   /* W5 */
	uint64_t ptr_start : 64; /* W6 */
	uint64_t ptr_end : 64;   /* W7 */
	uint64_t bpid_0 : 12; /* W8 */
	uint64_t rsvd_535_524 : 12;
	uint64_t err_int : 8;
	uint64_t err_int_ena : 8;
	uint64_t thresh_int : 1;
	uint64_t thresh_int_ena : 1;
	uint64_t thresh_up : 1;
	uint64_t rsvd_555 : 1;
	uint64_t thresh_qint_idx : 7;
	uint64_t rsvd_563 : 1;
	uint64_t err_qint_idx : 7;
	uint64_t rsvd_575_571 : 5;
	uint64_t thresh : 36; /* W9 */
	uint64_t rsvd_615_612 : 4;
	uint64_t fc_msh_dst : 11;
	uint64_t rsvd_630_627 : 4;
	uint64_t op_dpc_ena : 1;
	uint64_t op_dpc_set : 6;
	uint64_t stream_ctx : 1;
	uint64_t rsvd_639 : 1;
	uint64_t rsvd_703_640 : 64;  /* W10 */
	uint64_t rsvd_767_704 : 64;  /* W11 */
	uint64_t rsvd_831_768 : 64;  /* W12 */
	uint64_t rsvd_895_832 : 64;  /* W13 */
	uint64_t rsvd_959_896 : 64;  /* W14 */
	uint64_t rsvd_1023_960 : 64; /* W15 */
};

/* NPA queue interrupt context hardware structure */
struct npa_qint_hw_s {
	uint32_t count : 22;
	uint32_t rsvd_30_22 : 9;
	uint32_t ena : 1;
};

/* NPA batch allocate compare hardware structure */
struct npa_batch_alloc_compare_s {
	uint64_t aura : 20;
	uint64_t rsvd_31_20 : 12;
	uint64_t count : 10;
	uint64_t rsvd_47_42 : 6;
	uint64_t stype : 2;
	uint64_t rsvd_61_50 : 12;
	uint64_t dis_wait : 1;
	uint64_t drop : 1;
};

/* NPA batch alloc dma write status structure */
struct npa_batch_alloc_status_s {
	uint8_t count : 5;
	uint8_t ccode : 2;
	uint8_t rsvd_7_7 : 1;
};

typedef enum {
	ALLOC_RESULT_ACCEPTED = 0,
	ALLOC_RESULT_WAIT = 1,
	ALLOC_RESULT_ERR = 2,
	ALLOC_RESULT_NOCORE = 3,
	ALLOC_RESULT_NOCORE_WAIT = 4,
} npa_batch_alloc_result_e;

typedef enum {
	ALLOC_CCODE_INVAL = 0,
	ALLOC_CCODE_VAL = 1,
	ALLOC_CCODE_VAL_NULL = 2,
} npa_batch_alloc_ccode_e;

typedef enum {
	ALLOC_STYPE_STF = 0,
	ALLOC_STYPE_STT = 1,
	ALLOC_STYPE_STP = 2,
	ALLOC_STYPE_STSTP = 3,
} npa_batch_alloc_stype_e;

#endif /* __NPA_HW_H__ */
