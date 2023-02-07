/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#ifndef __ML_HW_H__
#define __ML_HW_H__

#include <stdint.h>

/* Constants */
#define ML_ANBX_NR 0x3

/* Base offsets */
#define ML_MLAB_BLK_OFFSET 0x20000000 /* CNF10KB */
#define ML_AXI_START_ADDR  0x800000000

/* MLW register offsets / ML_PF_BAR0 */
#define ML_CFG			 0x10000
#define ML_MLR_BASE		 0x10008
#define ML_AXI_BRIDGE_CTRL(a)	 (0x10020 | (uint64_t)(a) << 3)
#define ML_JOB_MGR_CTRL		 0x10060
#define ML_CORE_INT_LO		 0x10140
#define ML_CORE_INT_HI		 0x10160
#define ML_JCMDQ_IN(a)		 (0x11000 | (uint64_t)(a) << 3) /* CN10KA */
#define ML_JCMDQ_STATUS		 0x11010			/* CN10KA */
#define ML_STGX_STATUS(a)	 (0x11020 | (uint64_t)(a) << 3) /* CNF10KB */
#define ML_STG_CONTROL		 0x11100			/* CNF10KB */
#define ML_PNB_CMD_TYPE		 0x113a0			/* CNF10KB */
#define ML_SCRATCH(a)		 (0x14000 | (uint64_t)(a) << 3)
#define ML_ANBX_BACKP_DISABLE(a) (0x18000 | (uint64_t)(a) << 12) /* CN10KA */
#define ML_ANBX_NCBI_P_OVR(a)	 (0x18010 | (uint64_t)(a) << 12) /* CN10KA */
#define ML_ANBX_NCBI_NP_OVR(a)	 (0x18020 | (uint64_t)(a) << 12) /* CN10KA */

/* MLIP configuration register offsets / ML_PF_BAR0 */
#define ML_SW_RST_CTRL		      0x12084000
#define ML_A35_0_RST_VECTOR_BASE_W(a) (0x12084014 + (a) * (0x04))
#define ML_A35_1_RST_VECTOR_BASE_W(a) (0x1208401c + (a) * (0x04))

/* MLW scratch register offsets */
#define ML_SCRATCH_WORK_PTR	      (ML_SCRATCH(0))
#define ML_SCRATCH_FW_CTRL	      (ML_SCRATCH(1))
#define ML_SCRATCH_DBG_BUFFER_HEAD_C0 (ML_SCRATCH(2))
#define ML_SCRATCH_DBG_BUFFER_TAIL_C0 (ML_SCRATCH(3))
#define ML_SCRATCH_DBG_BUFFER_HEAD_C1 (ML_SCRATCH(4))
#define ML_SCRATCH_DBG_BUFFER_TAIL_C1 (ML_SCRATCH(5))
#define ML_SCRATCH_EXCEPTION_SP_C0    (ML_SCRATCH(6))
#define ML_SCRATCH_EXCEPTION_SP_C1    (ML_SCRATCH(7))

/* ML job completion structure */
struct ml_jce_s {
	/* WORD 0 */
	union ml_jce_w0 {
		struct {
			uint64_t rsvd_0_3 : 4;

			/* Reserved for future architecture */
			uint64_t ggrp_h : 2;

			/* Tag type */
			uint64_t ttype : 2;

			/* Physical function number */
			uint64_t pf_func : 16;

			/* Unused [7] + Guest Group [6:0] */
			uint64_t ggrp : 8;

			/* Tag */
			uint64_t tag : 32;
		} s;
		uint64_t u64;
	} w0;

	/* WORD 1 */
	union ml_jce_w1 {
		struct {
			/* Work queue pointer */
			uint64_t wqp : 53;
			uint64_t rsvd_53_63 : 11;

		} s;
		uint64_t u64;
	} w1;
};

/* ML job command structure */
struct ml_job_cmd_s {
	/* WORD 0 */
	union ml_job_cmd_w0 {
		struct {
			uint64_t rsvd_0_63;
		} s;
		uint64_t u64;
	} w0;

	/* WORD 1 */
	union ml_job_cmd_w1 {
		struct {
			/* Job pointer */
			uint64_t jobptr : 53;
			uint64_t rsvd_53_63 : 11;
		} s;
		uint64_t u64;
	} w1;
};

/* ML A35 0 RST vector base structure */
union ml_a35_0_rst_vector_base_s {
	struct {
		/* Base address */
		uint64_t addr : 37;
		uint64_t rsvd_37_63 : 27;
	} s;

	struct {
		/* WORD 0 */
		uint32_t w0;

		/* WORD 1 */
		uint32_t w1;
	} w;

	uint64_t u64;
};

/* ML A35 1 RST vector base structure */
union ml_a35_1_rst_vector_base_s {
	struct {
		/* Base address */
		uint64_t addr : 37;
		uint64_t rsvd_37_63 : 27;
	} s;

	struct {
		/* WORD 0 */
		uint32_t w0;

		/* WORD 1 */
		uint32_t w1;
	} w;

	uint64_t u64;
};

/* Work pointer scratch register */
union ml_scratch_work_ptr_s {
	struct {
		/* Work pointer */
		uint64_t work_ptr : 37;
		uint64_t rsvd_37_63 : 27;
	} s;
	uint64_t u64;
};

/* Firmware control scratch register */
union ml_scratch_fw_ctrl_s {
	struct {
		uint64_t rsvd_0_15 : 16;

		/* Valid job bit */
		uint64_t valid : 1;

		/* Done status bit */
		uint64_t done : 1;
		uint64_t rsvd_18_63 : 46;
	} s;
	uint64_t u64;
};

#endif /* __ML_HW_H__ */
