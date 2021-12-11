/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef __RVU_HW_H__
#define __RVU_HW_H__

/* Register offsets */

#define RVU_AF_MSIXTR_BASE     (0x10ull)
#define RVU_AF_BLK_RST	       (0x30ull)
#define RVU_AF_PF_BAR4_ADDR    (0x40ull)
#define RVU_AF_RAS	       (0x100ull)
#define RVU_AF_RAS_W1S	       (0x108ull)
#define RVU_AF_RAS_ENA_W1S     (0x110ull)
#define RVU_AF_RAS_ENA_W1C     (0x118ull)
#define RVU_AF_GEN_INT	       (0x120ull)
#define RVU_AF_GEN_INT_W1S     (0x128ull)
#define RVU_AF_GEN_INT_ENA_W1S (0x130ull)
#define RVU_AF_GEN_INT_ENA_W1C (0x138ull)
#define RVU_AF_AFPFX_MBOXX(a, b)                                               \
	(0x2000ull | (uint64_t)(a) << 4 | (uint64_t)(b) << 3)
#define RVU_AF_PFME_STATUS	     (0x2800ull)
#define RVU_AF_PFTRPEND		     (0x2810ull)
#define RVU_AF_PFTRPEND_W1S	     (0x2820ull)
#define RVU_AF_PF_RST		     (0x2840ull)
#define RVU_AF_HWVF_RST		     (0x2850ull)
#define RVU_AF_PFAF_MBOX_INT	     (0x2880ull)
#define RVU_AF_PFAF_MBOX_INT_W1S     (0x2888ull)
#define RVU_AF_PFAF_MBOX_INT_ENA_W1S (0x2890ull)
#define RVU_AF_PFAF_MBOX_INT_ENA_W1C (0x2898ull)
#define RVU_AF_PFFLR_INT	     (0x28a0ull)
#define RVU_AF_PFFLR_INT_W1S	     (0x28a8ull)
#define RVU_AF_PFFLR_INT_ENA_W1S     (0x28b0ull)
#define RVU_AF_PFFLR_INT_ENA_W1C     (0x28b8ull)
#define RVU_AF_PFME_INT		     (0x28c0ull)
#define RVU_AF_PFME_INT_W1S	     (0x28c8ull)
#define RVU_AF_PFME_INT_ENA_W1S	     (0x28d0ull)
#define RVU_AF_PFME_INT_ENA_W1C	     (0x28d8ull)
#define RVU_PRIV_CONST		     (0x8000000ull)
#define RVU_PRIV_GEN_CFG	     (0x8000010ull)
#define RVU_PRIV_CLK_CFG	     (0x8000020ull)
#define RVU_PRIV_ACTIVE_PC	     (0x8000030ull)
#define RVU_PRIV_PFX_CFG(a)	     (0x8000100ull | (uint64_t)(a) << 16)
#define RVU_PRIV_PFX_MSIX_CFG(a)     (0x8000110ull | (uint64_t)(a) << 16)
#define RVU_PRIV_PFX_ID_CFG(a)	     (0x8000120ull | (uint64_t)(a) << 16)
#define RVU_PRIV_PFX_INT_CFG(a)	     (0x8000200ull | (uint64_t)(a) << 16)
#define RVU_PRIV_PFX_NIXX_CFG(a, b)                                            \
	(0x8000300ull | (uint64_t)(a) << 16 | (uint64_t)(b) << 3)
#define RVU_PRIV_PFX_NPA_CFG(a)	 (0x8000310ull | (uint64_t)(a) << 16)
#define RVU_PRIV_PFX_SSO_CFG(a)	 (0x8000320ull | (uint64_t)(a) << 16)
#define RVU_PRIV_PFX_SSOW_CFG(a) (0x8000330ull | (uint64_t)(a) << 16)
#define RVU_PRIV_PFX_TIM_CFG(a)	 (0x8000340ull | (uint64_t)(a) << 16)
#define RVU_PRIV_PFX_CPTX_CFG(a, b)                                            \
	(0x8000350ull | (uint64_t)(a) << 16 | (uint64_t)(b) << 3)
#define RVU_PRIV_BLOCK_TYPEX_REV(a) (0x8000400ull | (uint64_t)(a) << 3)
#define RVU_PRIV_HWVFX_INT_CFG(a)   (0x8001280ull | (uint64_t)(a) << 16)
#define RVU_PRIV_HWVFX_NIXX_CFG(a, b)                                          \
	(0x8001300ull | (uint64_t)(a) << 16 | (uint64_t)(b) << 3)
#define RVU_PRIV_HWVFX_NPA_CFG(a)  (0x8001310ull | (uint64_t)(a) << 16)
#define RVU_PRIV_HWVFX_SSO_CFG(a)  (0x8001320ull | (uint64_t)(a) << 16)
#define RVU_PRIV_HWVFX_SSOW_CFG(a) (0x8001330ull | (uint64_t)(a) << 16)
#define RVU_PRIV_HWVFX_TIM_CFG(a)  (0x8001340ull | (uint64_t)(a) << 16)
#define RVU_PRIV_HWVFX_CPTX_CFG(a, b)                                          \
	(0x8001350ull | (uint64_t)(a) << 16 | (uint64_t)(b) << 3)

#define RVU_PF_VFX_PFVF_MBOXX(a, b)                                            \
	(0x0ull | (uint64_t)(a) << 12 | (uint64_t)(b) << 3)
#define RVU_PF_VF_BAR4_ADDR		 (0x10ull)
#define RVU_PF_BLOCK_ADDRX_DISC(a)	 (0x200ull | (uint64_t)(a) << 3)
#define RVU_PF_VFME_STATUSX(a)		 (0x800ull | (uint64_t)(a) << 3)
#define RVU_PF_VFTRPENDX(a)		 (0x820ull | (uint64_t)(a) << 3)
#define RVU_PF_VFTRPEND_W1SX(a)		 (0x840ull | (uint64_t)(a) << 3)
#define RVU_PF_VFPF_MBOX_INTX(a)	 (0x880ull | (uint64_t)(a) << 3)
#define RVU_PF_VFPF_MBOX_INT_W1SX(a)	 (0x8a0ull | (uint64_t)(a) << 3)
#define RVU_PF_VFPF_MBOX_INT_ENA_W1SX(a) (0x8c0ull | (uint64_t)(a) << 3)
#define RVU_PF_VFPF_MBOX_INT_ENA_W1CX(a) (0x8e0ull | (uint64_t)(a) << 3)
#define RVU_PF_VFFLR_INTX(a)		 (0x900ull | (uint64_t)(a) << 3)
#define RVU_PF_VFFLR_INT_W1SX(a)	 (0x920ull | (uint64_t)(a) << 3)
#define RVU_PF_VFFLR_INT_ENA_W1SX(a)	 (0x940ull | (uint64_t)(a) << 3)
#define RVU_PF_VFFLR_INT_ENA_W1CX(a)	 (0x960ull | (uint64_t)(a) << 3)
#define RVU_PF_VFME_INTX(a)		 (0x980ull | (uint64_t)(a) << 3)
#define RVU_PF_VFME_INT_W1SX(a)		 (0x9a0ull | (uint64_t)(a) << 3)
#define RVU_PF_VFME_INT_ENA_W1SX(a)	 (0x9c0ull | (uint64_t)(a) << 3)
#define RVU_PF_VFME_INT_ENA_W1CX(a)	 (0x9e0ull | (uint64_t)(a) << 3)
#define RVU_PF_PFAF_MBOXX(a)		 (0xc00ull | (uint64_t)(a) << 3)
#define RVU_PF_INT			 (0xc20ull)
#define RVU_PF_INT_W1S			 (0xc28ull)
#define RVU_PF_INT_ENA_W1S		 (0xc30ull)
#define RVU_PF_INT_ENA_W1C		 (0xc38ull)
#define RVU_PF_MSIX_VECX_ADDR(a)	 (0x80000ull | (uint64_t)(a) << 4)
#define RVU_PF_MSIX_VECX_CTL(a)		 (0x80008ull | (uint64_t)(a) << 4)
#define RVU_PF_MSIX_PBAX(a)		 (0xf0000ull | (uint64_t)(a) << 3)
#define RVU_VF_VFPF_MBOXX(a)		 (0x0ull | (uint64_t)(a) << 3)
#define RVU_VF_INT			 (0x20ull)
#define RVU_VF_INT_W1S			 (0x28ull)
#define RVU_VF_INT_ENA_W1S		 (0x30ull)
#define RVU_VF_INT_ENA_W1C		 (0x38ull)
#define RVU_VF_BLOCK_ADDRX_DISC(a)	 (0x200ull | (uint64_t)(a) << 3)
#define RVU_VF_MSIX_VECX_ADDR(a)	 (0x80000ull | (uint64_t)(a) << 4)
#define RVU_VF_MSIX_VECX_CTL(a)		 (0x80008ull | (uint64_t)(a) << 4)
#define RVU_VF_MBOX_REGION		 (0xc0000ull) /* [CN10K, .) */
#define RVU_VF_MSIX_PBAX(a)		 (0xf0000ull | (uint64_t)(a) << 3)

/* Enum offsets */

#define RVU_BAR_RVU_PF_END_BAR0	  (0x84f000000000ull)
#define RVU_BAR_RVU_PF_START_BAR0 (0x840000000000ull)
#define RVU_BAR_RVU_PFX_FUNCX_BAR2(a, b)                                       \
	(0x840200000000ull | ((uint64_t)(a) << 36) | ((uint64_t)(b) << 25))

#define RVU_AF_INT_VEC_POISON (0x0ull)
#define RVU_AF_INT_VEC_PFFLR  (0x1ull)
#define RVU_AF_INT_VEC_PFME   (0x2ull)
#define RVU_AF_INT_VEC_GEN    (0x3ull)
#define RVU_AF_INT_VEC_MBOX   (0x4ull)

#define RVU_BLOCK_TYPE_RVUM (0x0ull)
#define RVU_BLOCK_TYPE_LMT  (0x2ull)
#define RVU_BLOCK_TYPE_NIX  (0x3ull)
#define RVU_BLOCK_TYPE_NPA  (0x4ull)
#define RVU_BLOCK_TYPE_NPC  (0x5ull)
#define RVU_BLOCK_TYPE_SSO  (0x6ull)
#define RVU_BLOCK_TYPE_SSOW (0x7ull)
#define RVU_BLOCK_TYPE_TIM  (0x8ull)
#define RVU_BLOCK_TYPE_CPT  (0x9ull)
#define RVU_BLOCK_TYPE_NDC  (0xaull)
#define RVU_BLOCK_TYPE_DDF  (0xbull)
#define RVU_BLOCK_TYPE_ZIP  (0xcull)
#define RVU_BLOCK_TYPE_RAD  (0xdull)
#define RVU_BLOCK_TYPE_DFA  (0xeull)
#define RVU_BLOCK_TYPE_HNA  (0xfull)
#define RVU_BLOCK_TYPE_REE  (0xeull)

#define RVU_BLOCK_ADDR_RVUM    (0x0ull)
#define RVU_BLOCK_ADDR_LMT     (0x1ull)
#define RVU_BLOCK_ADDR_NPA     (0x3ull)
#define RVU_BLOCK_ADDR_NIX0    (0x4ull)
#define RVU_BLOCK_ADDR_NIX1    (0x5ull)
#define RVU_BLOCK_ADDR_NPC     (0x6ull)
#define RVU_BLOCK_ADDR_SSO     (0x7ull)
#define RVU_BLOCK_ADDR_SSOW    (0x8ull)
#define RVU_BLOCK_ADDR_TIM     (0x9ull)
#define RVU_BLOCK_ADDR_CPT0    (0xaull)
#define RVU_BLOCK_ADDR_CPT1    (0xbull)
#define RVU_BLOCK_ADDR_NDC0    (0xcull)
#define RVU_BLOCK_ADDR_NDC1    (0xdull)
#define RVU_BLOCK_ADDR_NDC2    (0xeull)
#define RVU_BLOCK_ADDR_R_END   (0x1full)
#define RVU_BLOCK_ADDR_R_START (0x14ull)
#define RVU_BLOCK_ADDR_REE0    (0x14ull)
#define RVU_BLOCK_ADDR_REE1    (0x15ull)

#define RVU_VF_INT_VEC_MBOX (0x0ull)

#define RVU_PF_INT_VEC_AFPF_MBOX  (0x6ull)
#define RVU_PF_INT_VEC_VFFLR0	  (0x0ull)
#define RVU_PF_INT_VEC_VFFLR1	  (0x1ull)
#define RVU_PF_INT_VEC_VFME0	  (0x2ull)
#define RVU_PF_INT_VEC_VFME1	  (0x3ull)
#define RVU_PF_INT_VEC_VFPF_MBOX0 (0x4ull)
#define RVU_PF_INT_VEC_VFPF_MBOX1 (0x5ull)

#define AF_BAR2_ALIASX_SIZE (0x100000ull)

#define TIM_AF_BAR2_SEL	 (0x9000000ull)
#define SSO_AF_BAR2_SEL	 (0x9000000ull)
#define NIX_AF_BAR2_SEL	 (0x9000000ull)
#define SSOW_AF_BAR2_SEL (0x9000000ull)
#define NPA_AF_BAR2_SEL	 (0x9000000ull)
#define CPT_AF_BAR2_SEL	 (0x9000000ull)
#define RVU_AF_BAR2_SEL	 (0x9000000ull)
#define REE_AF_BAR2_SEL	 (0x9000000ull)

#define AF_BAR2_ALIASX(a, b)                                                   \
	(0x9100000ull | (uint64_t)(a) << 12 | (uint64_t)(b))
#define TIM_AF_BAR2_ALIASX(a, b)  AF_BAR2_ALIASX(a, b)
#define SSO_AF_BAR2_ALIASX(a, b)  AF_BAR2_ALIASX(a, b)
#define NIX_AF_BAR2_ALIASX(a, b)  AF_BAR2_ALIASX(0, b)
#define SSOW_AF_BAR2_ALIASX(a, b) AF_BAR2_ALIASX(a, b)
#define NPA_AF_BAR2_ALIASX(a, b)  AF_BAR2_ALIASX(0, b)
#define CPT_AF_BAR2_ALIASX(a, b)  AF_BAR2_ALIASX(a, b)
#define RVU_AF_BAR2_ALIASX(a, b)  AF_BAR2_ALIASX(a, b)
#define REE_AF_BAR2_ALIASX(a, b)  AF_BAR2_ALIASX(a, b)

/* Structures definitions */

/* RVU admin function register address structure */
struct rvu_af_addr_s {
	uint64_t addr : 28;
	uint64_t block : 5;
	uint64_t rsvd_63_33 : 31;
};

/* RVU function-unique address structure */
struct rvu_func_addr_s {
	uint32_t addr : 12;
	uint32_t lf_slot : 8;
	uint32_t block : 5;
	uint32_t rsvd_31_25 : 7;
};

/* RVU msi-x vector structure */
struct rvu_msix_vec_s {
	uint64_t addr : 64; /* W0 */
	uint64_t data : 32;
	uint64_t mask : 1;
	uint64_t pend : 1;
	uint64_t rsvd_127_98 : 30;
};

/* RVU pf function identification structure */
struct rvu_pf_func_s {
	uint16_t func : 10;
	uint16_t pf : 6;
};

#define RVU_CN9K_LMT_SLOT_MAX  256ULL
#define RVU_CN9K_LMT_SLOT_MASK (RVU_CN9K_LMT_SLOT_MAX - 1)

#define RVU_LMT_SZ 128ULL

/* 2048 LMT lines in BAR4 [CN10k, .) */
#define RVU_LMT_LINE_MAX       2048
#define RVU_LMT_LINE_BURST_MAX (uint16_t)32 /* [CN10K, .) */

#endif /* __RVU_HW_H__ */
