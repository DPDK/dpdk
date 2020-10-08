/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2019 Marvell International Ltd.
 */

#ifndef _OTX2_CRYPTODEV_HW_ACCESS_H_
#define _OTX2_CRYPTODEV_HW_ACCESS_H_

#include <stdint.h>

#include <rte_cryptodev.h>
#include <rte_memory.h>

#include "cpt_common.h"
#include "cpt_hw_types.h"
#include "cpt_mcode_defines.h"

#include "otx2_dev.h"
#include "otx2_cryptodev_qp.h"

/* CPT instruction queue length */
#define OTX2_CPT_IQ_LEN			8200

#define OTX2_CPT_DEFAULT_CMD_QLEN	OTX2_CPT_IQ_LEN

/* Mask which selects all engine groups */
#define OTX2_CPT_ENG_GRPS_MASK		0xFF

/* Register offsets */

/* LMT LF registers */
#define OTX2_LMT_LF_LMTLINE(a)		(0x0ull | (uint64_t)(a) << 3)

/* CPT LF registers */
#define OTX2_CPT_LF_CTL			0x10ull
#define OTX2_CPT_LF_INPROG		0x40ull
#define OTX2_CPT_LF_MISC_INT		0xb0ull
#define OTX2_CPT_LF_MISC_INT_ENA_W1S	0xd0ull
#define OTX2_CPT_LF_MISC_INT_ENA_W1C	0xe0ull
#define OTX2_CPT_LF_Q_BASE		0xf0ull
#define OTX2_CPT_LF_Q_SIZE		0x100ull
#define OTX2_CPT_LF_Q_GRP_PTR		0x120ull
#define OTX2_CPT_LF_NQ(a)		(0x400ull | (uint64_t)(a) << 3)

#define OTX2_CPT_AF_LF_CTL(a)		(0x27000ull | (uint64_t)(a) << 3)
#define OTX2_CPT_AF_LF_CTL2(a)		(0x29000ull | (uint64_t)(a) << 3)

#define OTX2_CPT_LF_BAR2(vf, q_id) \
		((vf)->otx2_dev.bar2 + \
		 ((RVU_BLOCK_ADDR_CPT0 << 20) | ((q_id) << 12)))

#define OTX2_CPT_QUEUE_HI_PRIO 0x1

union otx2_cpt_lf_ctl {
	uint64_t u;
	struct {
		uint64_t ena                         : 1;
		uint64_t fc_ena                      : 1;
		uint64_t fc_up_crossing              : 1;
		uint64_t reserved_3_3                : 1;
		uint64_t fc_hyst_bits                : 4;
		uint64_t reserved_8_63               : 56;
	} s;
};

union otx2_cpt_lf_inprog {
	uint64_t u;
	struct {
		uint64_t inflight                    : 9;
		uint64_t reserved_9_15               : 7;
		uint64_t eena                        : 1;
		uint64_t grp_drp                     : 1;
		uint64_t reserved_18_30              : 13;
		uint64_t grb_partial                 : 1;
		uint64_t grb_cnt                     : 8;
		uint64_t gwb_cnt                     : 8;
		uint64_t reserved_48_63              : 16;
	} s;
};

union otx2_cpt_lf_q_base {
	uint64_t u;
	struct {
		uint64_t fault                       : 1;
		uint64_t stopped                     : 1;
		uint64_t reserved_2_6                : 5;
		uint64_t addr                        : 46;
		uint64_t reserved_53_63              : 11;
	} s;
};

union otx2_cpt_lf_q_size {
	uint64_t u;
	struct {
		uint64_t size_div40                  : 15;
		uint64_t reserved_15_63              : 49;
	} s;
};

union otx2_cpt_af_lf_ctl {
	uint64_t u;
	struct {
		uint64_t pri                         : 1;
		uint64_t reserved_1_8                : 8;
		uint64_t pf_func_inst                : 1;
		uint64_t cont_err                    : 1;
		uint64_t reserved_11_15              : 5;
		uint64_t nixtx_en                    : 1;
		uint64_t reserved_17_47              : 31;
		uint64_t grp                         : 8;
		uint64_t reserved_56_63              : 8;
	} s;
};

union otx2_cpt_af_lf_ctl2 {
	uint64_t u;
	struct {
		uint64_t exe_no_swap                 : 1;
		uint64_t exe_ldwb                    : 1;
		uint64_t reserved_2_31               : 30;
		uint64_t sso_pf_func                 : 16;
		uint64_t nix_pf_func                 : 16;
	} s;
};

union otx2_cpt_lf_q_grp_ptr {
	uint64_t u;
	struct {
		uint64_t dq_ptr                      : 15;
		uint64_t reserved_31_15              : 17;
		uint64_t nq_ptr                      : 15;
		uint64_t reserved_47_62              : 16;
		uint64_t xq_xor                      : 1;
	} s;
};

/*
 * Enumeration cpt_9x_comp_e
 *
 * CPT 9X Completion Enumeration
 * Enumerates the values of CPT_RES_S[COMPCODE].
 */
enum cpt_9x_comp_e {
	CPT_9X_COMP_E_NOTDONE = 0x00,
	CPT_9X_COMP_E_GOOD = 0x01,
	CPT_9X_COMP_E_FAULT = 0x02,
	CPT_9X_COMP_E_HWERR = 0x04,
	CPT_9X_COMP_E_INSTERR = 0x05,
	CPT_9X_COMP_E_LAST_ENTRY = 0x06
};

void otx2_cpt_err_intr_unregister(const struct rte_cryptodev *dev);

int otx2_cpt_err_intr_register(const struct rte_cryptodev *dev);

int otx2_cpt_iq_enable(const struct rte_cryptodev *dev,
		       const struct otx2_cpt_qp *qp, uint8_t grp_mask,
		       uint8_t pri, uint32_t size_div40);

void otx2_cpt_iq_disable(struct otx2_cpt_qp *qp);

#endif /* _OTX2_CRYPTODEV_HW_ACCESS_H_ */
