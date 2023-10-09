/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef __CPT_HW_H__
#define __CPT_HW_H__

#include "roc_platform.h"

/* Register offsets */

#define CPT_COMP_NOT_DONE (0x0ull)
#define CPT_COMP_GOOD	  (0x1ull)
#define CPT_COMP_FAULT	  (0x2ull)
#define CPT_COMP_SWERR	  (0x3ull)
#define CPT_COMP_HWERR	  (0x4ull)
#define CPT_COMP_INSTERR  (0x5ull)
#define CPT_COMP_WARN	  (0x6ull) /* [CN10K, .) */

#define CPT_COMP_HWGOOD_MASK ((1U << CPT_COMP_WARN) | (1U << CPT_COMP_GOOD))

#define CPT_LF_INT_VEC_MISC	(0x0ull)
#define CPT_LF_INT_VEC_DONE	(0x1ull)
#define CPT_LF_CTL		(0x10ull)
#define CPT_LF_DONE_WAIT	(0x30ull)
#define CPT_LF_INPROG		(0x40ull)
#define CPT_LF_DONE		(0x50ull)
#define CPT_LF_DONE_ACK		(0x60ull)
#define CPT_LF_DONE_INT_ENA_W1S (0x90ull)
#define CPT_LF_DONE_INT_ENA_W1C (0xa0ull)
#define CPT_LF_MISC_INT		(0xb0ull)
#define CPT_LF_MISC_INT_W1S	(0xc0ull)
#define CPT_LF_MISC_INT_ENA_W1S (0xd0ull)
#define CPT_LF_MISC_INT_ENA_W1C (0xe0ull)
#define CPT_LF_Q_BASE		(0xf0ull)
#define CPT_LF_Q_SIZE		(0x100ull)
#define CPT_LF_Q_INST_PTR	(0x110ull)
#define CPT_LF_Q_GRP_PTR	(0x120ull)
#define CPT_LF_NQX(a)		(0x400ull | (uint64_t)(a) << 3)
#define CPT_LF_CTX_CTL		(0x500ull)
#define CPT_LF_CTX_FLUSH	(0x510ull)
#define CPT_LF_CTX_ERR		(0x520ull)
#define CPT_LF_CTX_ENC_BYTE_CNT (0x530ull)
#define CPT_LF_CTX_ENC_PKT_CNT	(0x540ull)
#define CPT_LF_CTX_DEC_BYTE_CNT (0x550ull)
#define CPT_LF_CTX_DEC_PKT_CNT	(0x560ull)
#define CPT_LF_CTX_RELOAD	(0x570ull)

#define CPT_AF_LFX_CTL(a)  (0x27000ull | (uint64_t)(a) << 3)
#define CPT_AF_LFX_CTL2(a) (0x29000ull | (uint64_t)(a) << 3)

enum cpt_eng_type {
	CPT_ENG_TYPE_AE = 1,
	CPT_ENG_TYPE_SE = 2,
	CPT_ENG_TYPE_IE = 3,
	CPT_MAX_ENG_TYPES,
};

/* Structures definitions */

/* CPT HW capabilities */
union cpt_eng_caps {
	uint64_t __io u;
	struct {
		uint64_t __io reserved_0_4 : 5;
		uint64_t __io mul : 1;
		uint64_t __io sha1_sha2 : 1;
		uint64_t __io chacha20 : 1;
		uint64_t __io zuc_snow3g : 1;
		uint64_t __io sha3 : 1;
		uint64_t __io aes : 1;
		uint64_t __io kasumi : 1;
		uint64_t __io des : 1;
		uint64_t __io crc : 1;
		uint64_t __io mmul : 1;
		uint64_t __io reserved_15_20 : 6;
		uint64_t __io sm3 : 1;
		uint64_t __io sm4 : 1;
		uint64_t __io reserved_23_34 : 12;
		uint64_t __io sg_ver2 : 1;
		uint64_t __io sm2 : 1;
		uint64_t __io pdcp_chain_zuc256 : 1;
		uint64_t __io reserved_38_63 : 26;
	};
};

union cpt_lf_ctl {
	uint64_t u;
	struct cpt_lf_ctl_s {
		uint64_t ena : 1;
		uint64_t fc_ena : 1;
		uint64_t fc_up_crossing : 1;
		uint64_t reserved_3_3 : 1;
		uint64_t fc_hyst_bits : 4;
		uint64_t reserved_8_63 : 56;
	} s;
};

union cpt_lf_ctx_flush {
	uint64_t u;
	struct {
		uint64_t cptr : 46;
		uint64_t inval : 1;
		uint64_t reserved_47_63 : 17;
	} s;
};

union cpt_lf_ctx_err {
	uint64_t u;
	struct {
		uint64_t flush_st_flt : 1;
		uint64_t busy_flr : 1;
		uint64_t busy_sw_flush : 1;
		uint64_t reload_faulted : 1;
		uint64_t reserved_4_63 : 1;
	} s;
};

union cpt_lf_ctx_reload {
	uint64_t u;
	struct {
		uint64_t cptr : 46;
		uint64_t reserved_46_63 : 18;
	} s;
};

union cpt_lf_inprog {
	uint64_t u;
	struct cpt_lf_inprog_s {
		uint64_t inflight : 9;
		uint64_t reserved_9_15 : 7;
		uint64_t eena : 1;
		uint64_t grp_drp : 1;
		uint64_t reserved_18_30 : 13;
		uint64_t grb_partial : 1;
		uint64_t grb_cnt : 8;
		uint64_t gwb_cnt : 8;
		uint64_t reserved_48_63 : 16;
	} s;
};

union cpt_lf_q_inst_ptr {
	uint64_t u;
	struct cpt_lf_q_inst_ptr_s {
		uint64_t dq_ptr : 20;
		uint64_t reserved_20_31 : 12;
		uint64_t nq_ptr : 20;
		uint64_t reserved_52_62 : 11;
		uint64_t xq_xor : 1;
	} s;
};

union cpt_lf_q_base {
	uint64_t u;
	struct cpt_lf_q_base_s {
		uint64_t fault : 1;
		uint64_t stopped : 1;
		uint64_t reserved_2_6 : 5;
		uint64_t addr : 46;
		uint64_t reserved_53_63 : 11;
	} s;
};

union cpt_lf_q_size {
	uint64_t u;
	struct cpt_lf_q_size_s {
		uint64_t size_div40 : 15;
		uint64_t reserved_15_63 : 49;
	} s;
};

union cpt_lf_misc_int {
	uint64_t u;
	struct cpt_lf_misc_int_s {
		uint64_t reserved_0_0 : 1;
		uint64_t nqerr : 1;
		uint64_t irde : 1;
		uint64_t nwrp : 1;
		uint64_t reserved_4_4 : 1;
		uint64_t hwerr : 1;
		uint64_t fault : 1;
		uint64_t reserved_7_63 : 57;
	} s;
};

union cpt_lf_q_grp_ptr {
	uint64_t u;
	struct {
		uint64_t dq_ptr : 15;
		uint64_t reserved_31_15 : 17;
		uint64_t nq_ptr : 15;
		uint64_t reserved_47_62 : 16;
		uint64_t xq_xor : 1;
	} s;
};

union cpt_inst_w4 {
	uint64_t u64;
	struct {
		uint64_t dlen : 16;
		uint64_t param2 : 16;
		uint64_t param1 : 16;
		uint64_t opcode_major : 8;
		uint64_t opcode_minor : 8;
	} s;
};

union cpt_inst_w5 {
	uint64_t u64;
	struct {
		uint64_t dptr : 60;
		uint64_t gather_sz : 4;
	} s;
};

union cpt_inst_w6 {
	uint64_t u64;
	struct {
		uint64_t rptr : 60;
		uint64_t scatter_sz : 4;
	} s;
};

union cpt_inst_w7 {
	uint64_t u64;
	struct {
		uint64_t cptr : 60;
		uint64_t ctx_val : 1;
		uint64_t egrp : 3;
	} s;
};

struct cpt_inst_s {
	union cpt_inst_w0 {
		struct {
			uint64_t nixtxl : 3;
			uint64_t doneint : 1;
			uint64_t nixtx_addr : 60;
		} s;
		uint64_t u64;
	} w0;

	uint64_t res_addr;

	union cpt_inst_w2 {
		struct {
			uint64_t tag : 32;
			uint64_t tt : 2;
			uint64_t grp : 10;
			uint64_t reserved_172_175 : 4;
			uint64_t rvu_pf_func : 16;
		} s;
		uint64_t u64;
	} w2;

	union cpt_inst_w3 {
		struct {
			uint64_t qord : 1;
			uint64_t reserved_194_193 : 2;
			uint64_t wqe_ptr : 61;
		} s;
		uint64_t u64;
	} w3;

	union cpt_inst_w4 w4;

	union {
		union cpt_inst_w5 w5;
		uint64_t dptr;
	};

	union {
		union cpt_inst_w6 w6;
		uint64_t rptr;
	};

	union cpt_inst_w7 w7;
};

union cpt_res_s {
	struct cpt_cn10k_res_s {
		uint64_t compcode : 7;
		uint64_t doneint : 1;
		uint64_t uc_compcode : 8;
		uint64_t rlen : 16;
		uint64_t spi : 32;

		uint64_t esn;
	} cn10k;

	struct cpt_cn9k_res_s {
		uint64_t compcode : 8;
		uint64_t uc_compcode : 8;
		uint64_t doneint : 1;
		uint64_t reserved_17_63 : 47;

		uint64_t reserved_64_127;
	} cn9k;

	uint64_t u64[2];
};

/* [CN10K, .) */
struct cpt_parse_hdr_s {
	/* WORD 0 */
	union {
		uint64_t u64;
		struct {
			uint8_t pad_len : 3;
			uint8_t num_frags : 3;
			uint8_t pkt_out : 2;

			uint8_t err_sum : 1;
			uint8_t reas_sts : 4;
			uint8_t reserved_53 : 1;
			uint8_t et_owr : 1;
			uint8_t pkt_fmt : 1;

			uint16_t match_id : 16;

			uint32_t cookie : 32;
		};
	} w0;

	/* WORD 1 */
	uint64_t wqe_ptr;

	/* WORD 2 */
	union {
		uint64_t u64;
		struct {
			uint8_t fi_pad : 3;
			uint8_t fi_offset : 5;
			uint8_t il3_off;
			uint16_t orig_pf_func;
			uint16_t reserved_145_160;
			uint16_t frag_age;
		};
	} w2;

	/* WORD 3 */
	union {
		uint64_t u64;
		struct {
			uint32_t spi;
			uint16_t reserved_209_224;
			uint8_t uc_ccode;
			uint8_t hw_ccode;
		};
	} w3;

	/* WORD 4 */
	union {
		uint64_t u64;
		uint64_t esn;
		uint64_t frag1_wqe_ptr;
	};
};

union cpt_frag_info {
	uint16_t info;
	struct {
		uint16_t f_off : 13;
		uint16_t f_mf : 1;
		uint16_t f_rsv : 2;
	};
};

struct cpt_frag_info_s {
	/* WORD 0 */
	union {
		uint64_t u64;
		struct {
			/* CPT HW swaps each 8B word implicitly */
			union cpt_frag_info f0;
			union cpt_frag_info f1;
			union cpt_frag_info f2;
			union cpt_frag_info f3;
		};
	} w0;

	/* WORD 1 */
	union {
		uint64_t u64;
		struct {
			/* CPT HW swaps each 8B word implicitly */
			uint16_t frag_size0;
			uint16_t frag_size1;
			uint16_t frag_size2;
			uint16_t frag_size3;
		};
	} w1;
};

union cpt_fc_write_s {
	struct {
		uint32_t qsize;
		uint32_t reserved_32_63;
		uint64_t reserved_64_127;
	} s;
	uint64_t u64[2];
};

#endif /* __CPT_HW_H__ */
