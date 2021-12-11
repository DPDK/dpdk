/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef __REE_HW_H__
#define __REE_HW_H__

/* REE instruction queue length */
#define REE_IQ_LEN (1 << 13)

#define REE_DEFAULT_CMD_QLEN REE_IQ_LEN

/* Status register bits */
#define REE_STATUS_PMI_EOJ_BIT	   BIT_ULL(14)
#define REE_STATUS_PMI_SOJ_BIT	   BIT_ULL(13)
#define REE_STATUS_MP_CNT_DET_BIT  BIT_ULL(7)
#define REE_STATUS_MM_CNT_DET_BIT  BIT_ULL(6)
#define REE_STATUS_ML_CNT_DET_BIT  BIT_ULL(5)
#define REE_STATUS_MST_CNT_DET_BIT BIT_ULL(4)
#define REE_STATUS_MPT_CNT_DET_BIT BIT_ULL(3)

/* Register offsets */
/* REE LF registers */
#define REE_LF_DONE_INT		0x120ull
#define REE_LF_DONE_INT_W1S	0x130ull
#define REE_LF_DONE_INT_ENA_W1S 0x138ull
#define REE_LF_DONE_INT_ENA_W1C 0x140ull
#define REE_LF_MISC_INT		0x300ull
#define REE_LF_MISC_INT_W1S	0x310ull
#define REE_LF_MISC_INT_ENA_W1S 0x320ull
#define REE_LF_MISC_INT_ENA_W1C 0x330ull
#define REE_LF_ENA		0x10ull
#define REE_LF_SBUF_ADDR	0x20ull
#define REE_LF_DONE		0x100ull
#define REE_LF_DONE_ACK		0x110ull
#define REE_LF_DONE_WAIT	0x148ull
#define REE_LF_DOORBELL		0x400ull
#define REE_LF_OUTSTAND_JOB	0x410ull

/* BAR 0 */
#define REE_AF_REEXM_MAX_MATCH (0x80c8ull)
#define REE_AF_QUE_SBUF_CTL(a) (0x1200ull | (uint64_t)(a) << 3)
#define REE_PRIV_LF_CFG(a)     (0x41000ull | (uint64_t)(a) << 3)

#define REE_AF_QUEX_GMCTL(a) (0x800 | (a) << 3)

#define REE_AF_INT_VEC_RAS	(0x0ull)
#define REE_AF_INT_VEC_RVU	(0x1ull)
#define REE_AF_INT_VEC_QUE_DONE (0x2ull)
#define REE_AF_INT_VEC_AQ	(0x3ull)


#define REE_LF_INT_VEC_QUE_DONE (0x0ull)
#define REE_LF_INT_VEC_MISC	(0x1ull)

#define REE_LF_SBUF_ADDR_OFF_MASK GENMASK_ULL(6, 0)
#define REE_LF_SBUF_ADDR_PTR_MASK GENMASK_ULL(52, 7)

#define REE_LF_ENA_ENA_MASK BIT_ULL(0)

#define REE_LF_BAR2(vf, q_id)                                                  \
	((vf)->dev->bar2 + (((vf)->block_address << 20) | ((q_id) << 12)))

#define REE_QUEUE_HI_PRIO 0x1

enum ree_desc_type_e {
	REE_TYPE_JOB_DESC = 0x0,
	REE_TYPE_RESULT_DESC = 0x1,
	REE_TYPE_ENUM_LAST = 0x2
};

union ree_res_status {
	uint64_t u;
	struct {
		uint64_t job_type : 3;
		uint64_t mpt_cnt_det : 1;
		uint64_t mst_cnt_det : 1;
		uint64_t ml_cnt_det : 1;
		uint64_t mm_cnt_det : 1;
		uint64_t mp_cnt_det : 1;
		uint64_t mode : 2;
		uint64_t reserved_10_11 : 2;
		uint64_t reserved_12_12 : 1;
		uint64_t pmi_soj : 1;
		uint64_t pmi_eoj : 1;
		uint64_t reserved_15_15 : 1;
		uint64_t reserved_16_63 : 48;
	} s;
};

union ree_res {
	uint64_t u[8];
	struct ree_res_s_98 {
		uint64_t done : 1;
		uint64_t hwjid : 7;
		uint64_t ree_res_job_id : 24;
		uint64_t ree_res_status : 16;
		uint64_t ree_res_dmcnt : 8;
		uint64_t ree_res_mcnt : 8;
		uint64_t ree_meta_ptcnt : 16;
		uint64_t ree_meta_icnt : 16;
		uint64_t ree_meta_lcnt : 16;
		uint64_t ree_pmi_min_byte_ptr : 16;
		uint64_t ree_err : 1;
		uint64_t reserved_129_190 : 62;
		uint64_t doneint : 1;
		uint64_t reserved_192_255 : 64;
		uint64_t reserved_256_319 : 64;
		uint64_t reserved_320_383 : 64;
		uint64_t reserved_384_447 : 64;
		uint64_t reserved_448_511 : 64;
	} s;
};

union ree_match {
	uint64_t u;
	struct {
		uint64_t ree_rule_id : 32;
		uint64_t start_ptr : 14;
		uint64_t reserved_46_47 : 2;
		uint64_t match_length : 15;
		uint64_t reserved_63_6 : 1;
	} s;
};

#endif /* __REE_HW_H__ */
