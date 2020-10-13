/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2020 Marvell International Ltd.
 */

#ifndef _OTX2_REGEXDEV_HW_ACCESS_H_
#define _OTX2_REGEXDEV_HW_ACCESS_H_

#include <stdint.h>

#include "otx2_regexdev.h"

/* REE instruction queue length */
#define OTX2_REE_IQ_LEN			(1 << 13)

#define OTX2_REE_DEFAULT_CMD_QLEN	OTX2_REE_IQ_LEN

/* Status register bits */
#define OTX2_REE_STATUS_PMI_EOJ_BIT		(1 << 14)
#define OTX2_REE_STATUS_PMI_SOJ_BIT		(1 << 13)
#define OTX2_REE_STATUS_MP_CNT_DET_BIT		(1 << 7)
#define OTX2_REE_STATUS_MM_CNT_DET_BIT		(1 << 6)
#define OTX2_REE_STATUS_ML_CNT_DET_BIT		(1 << 5)
#define OTX2_REE_STATUS_MST_CNT_DET_BIT		(1 << 4)
#define OTX2_REE_STATUS_MPT_CNT_DET_BIT		(1 << 3)

/* Register offsets */
/* REE LF registers */
#define OTX2_REE_LF_DONE_INT		0x120ull
#define OTX2_REE_LF_DONE_INT_W1S	0x130ull
#define OTX2_REE_LF_DONE_INT_ENA_W1S	0x138ull
#define OTX2_REE_LF_DONE_INT_ENA_W1C	0x140ull
#define OTX2_REE_LF_MISC_INT		0x300ull
#define OTX2_REE_LF_MISC_INT_W1S	0x310ull
#define OTX2_REE_LF_MISC_INT_ENA_W1S	0x320ull
#define OTX2_REE_LF_MISC_INT_ENA_W1C	0x330ull
#define OTX2_REE_LF_ENA			0x10ull
#define OTX2_REE_LF_SBUF_ADDR		0x20ull
#define OTX2_REE_LF_DONE		0x100ull
#define OTX2_REE_LF_DONE_ACK		0x110ull
#define OTX2_REE_LF_DONE_WAIT		0x148ull
#define OTX2_REE_LF_DOORBELL		0x400ull
#define OTX2_REE_LF_OUTSTAND_JOB	0x410ull

/* BAR 0 */
#define OTX2_REE_AF_QUE_SBUF_CTL(a)	(0x1200ull | (uint64_t)(a) << 3)
#define OTX2_REE_PRIV_LF_CFG(a)		(0x41000ull | (uint64_t)(a) << 3)

#define OTX2_REE_LF_BAR2(vf, q_id) \
		((vf)->otx2_dev.bar2 + \
		 (((vf)->block_address << 20) | ((q_id) << 12)))


#define OTX2_REE_QUEUE_HI_PRIO 0x1

enum ree_desc_type_e {
	REE_TYPE_JOB_DESC    = 0x0,
	REE_TYPE_RESULT_DESC = 0x1,
	REE_TYPE_ENUM_LAST   = 0x2
};

union otx2_ree_priv_lf_cfg {
	uint64_t u;
	struct {
		uint64_t slot                        : 8;
		uint64_t pf_func                     : 16;
		uint64_t reserved_24_62              : 39;
		uint64_t ena                         : 1;
	} s;
};


union otx2_ree_lf_sbuf_addr {
	uint64_t u;
	struct {
		uint64_t off                         : 7;
		uint64_t ptr                         : 46;
		uint64_t reserved_53_63              : 11;
	} s;
};

union otx2_ree_lf_ena {
	uint64_t u;
	struct {
		uint64_t ena                         : 1;
		uint64_t reserved_1_63               : 63;
	} s;
};

union otx2_ree_af_reexm_max_match {
	uint64_t u;
	struct {
		uint64_t max                         : 8;
		uint64_t reserved_8_63               : 56;
	} s;
};

union otx2_ree_lf_done {
	uint64_t u;
	struct {
		uint64_t done                        : 20;
		uint64_t reserved_20_63              : 44;
	} s;
};

union otx2_ree_inst {
	uint64_t u[8];
	struct  {
		uint64_t doneint                     :  1;
		uint64_t reserved_1_3                :  3;
		uint64_t dg                          :  1;
		uint64_t reserved_5_7                :  3;
		uint64_t ooj                         :  1;
		uint64_t reserved_9_15               :  7;
		uint64_t reserved_16_63              : 48;
		uint64_t inp_ptr_addr                : 64;
		uint64_t inp_ptr_ctl                 : 64;
		uint64_t res_ptr_addr                : 64;
		uint64_t wq_ptr                      : 64;
		uint64_t tag                         : 32;
		uint64_t tt                          :  2;
		uint64_t ggrp                        : 10;
		uint64_t reserved_364_383            : 20;
		uint64_t reserved_384_391            :  8;
		uint64_t ree_job_id                  : 24;
		uint64_t ree_job_ctrl                : 16;
		uint64_t ree_job_length              : 15;
		uint64_t reserved_447_447            :  1;
		uint64_t ree_job_subset_id_0         : 16;
		uint64_t ree_job_subset_id_1         : 16;
		uint64_t ree_job_subset_id_2         : 16;
		uint64_t ree_job_subset_id_3         : 16;
	} cn98xx;
};

union otx2_ree_res_status {
	uint64_t u;
	struct {
		uint64_t job_type                    :  3;
		uint64_t mpt_cnt_det                 :  1;
		uint64_t mst_cnt_det                 :  1;
		uint64_t ml_cnt_det                  :  1;
		uint64_t mm_cnt_det                  :  1;
		uint64_t mp_cnt_det                  :  1;
		uint64_t mode                        :  2;
		uint64_t reserved_10_11              :  2;
		uint64_t reserved_12_12              :  1;
		uint64_t pmi_soj                     :  1;
		uint64_t pmi_eoj                     :  1;
		uint64_t reserved_15_15              :  1;
		uint64_t reserved_16_63              : 48;
	} s;
};

union otx2_ree_res {
	uint64_t u[8];
	struct ree_res_s_98 {
		uint64_t done			:  1;
		uint64_t hwjid			:  7;
		uint64_t ree_res_job_id		: 24;
		uint64_t ree_res_status		: 16;
		uint64_t ree_res_dmcnt		:  8;
		uint64_t ree_res_mcnt		:  8;
		uint64_t ree_meta_ptcnt		: 16;
		uint64_t ree_meta_icnt		: 16;
		uint64_t ree_meta_lcnt		: 16;
		uint64_t ree_pmi_min_byte_ptr	: 16;
		uint64_t ree_err		:  1;
		uint64_t reserved_129_190	: 62;
		uint64_t doneint		:  1;
		uint64_t reserved_192_255	: 64;
		uint64_t reserved_256_319	: 64;
		uint64_t reserved_320_383	: 64;
		uint64_t reserved_384_447	: 64;
		uint64_t reserved_448_511	: 64;
	} s;
};

union otx2_ree_match {
	uint64_t u;
	struct {
		uint64_t ree_rule_id                 : 32;
		uint64_t start_ptr                   : 14;
		uint64_t reserved_46_47              :  2;
		uint64_t match_length                : 15;
		uint64_t reserved_63_63              :  1;
	} s;
};

void otx2_ree_err_intr_unregister(const struct rte_regexdev *dev);

int otx2_ree_err_intr_register(const struct rte_regexdev *dev);

int otx2_ree_iq_enable(const struct rte_regexdev *dev,
		       const struct otx2_ree_qp *qp,
		       uint8_t pri, uint32_t size_div128);

void otx2_ree_iq_disable(struct otx2_ree_qp *qp);

int otx2_ree_max_matches_get(const struct rte_regexdev *dev,
			     uint8_t *max_matches);

#endif /* _OTX2_REGEXDEV_HW_ACCESS_H_ */
