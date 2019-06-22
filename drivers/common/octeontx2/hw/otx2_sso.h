/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef __OTX2_SSO_HW_H__
#define __OTX2_SSO_HW_H__

/* Register offsets */

#define SSO_AF_CONST                        (0x1000ull)
#define SSO_AF_CONST1                       (0x1008ull)
#define SSO_AF_WQ_INT_PC                    (0x1020ull)
#define SSO_AF_NOS_CNT                      (0x1050ull)
#define SSO_AF_AW_WE                        (0x1080ull)
#define SSO_AF_WS_CFG                       (0x1088ull)
#define SSO_AF_GWE_CFG                      (0x1098ull)
#define SSO_AF_GWE_RANDOM                   (0x10b0ull)
#define SSO_AF_LF_HWGRP_RST                 (0x10e0ull)
#define SSO_AF_AW_CFG                       (0x10f0ull)
#define SSO_AF_BLK_RST                      (0x10f8ull)
#define SSO_AF_ACTIVE_CYCLES0               (0x1100ull)
#define SSO_AF_ACTIVE_CYCLES1               (0x1108ull)
#define SSO_AF_ACTIVE_CYCLES2               (0x1110ull)
#define SSO_AF_ERR0                         (0x1220ull)
#define SSO_AF_ERR0_W1S                     (0x1228ull)
#define SSO_AF_ERR0_ENA_W1C                 (0x1230ull)
#define SSO_AF_ERR0_ENA_W1S                 (0x1238ull)
#define SSO_AF_ERR2                         (0x1260ull)
#define SSO_AF_ERR2_W1S                     (0x1268ull)
#define SSO_AF_ERR2_ENA_W1C                 (0x1270ull)
#define SSO_AF_ERR2_ENA_W1S                 (0x1278ull)
#define SSO_AF_UNMAP_INFO                   (0x12f0ull)
#define SSO_AF_UNMAP_INFO2                  (0x1300ull)
#define SSO_AF_UNMAP_INFO3                  (0x1310ull)
#define SSO_AF_RAS                          (0x1420ull)
#define SSO_AF_RAS_W1S                      (0x1430ull)
#define SSO_AF_RAS_ENA_W1C                  (0x1460ull)
#define SSO_AF_RAS_ENA_W1S                  (0x1470ull)
#define SSO_AF_AW_INP_CTL                   (0x2070ull)
#define SSO_AF_AW_ADD                       (0x2080ull)
#define SSO_AF_AW_READ_ARB                  (0x2090ull)
#define SSO_AF_XAQ_REQ_PC                   (0x20b0ull)
#define SSO_AF_XAQ_LATENCY_PC               (0x20b8ull)
#define SSO_AF_TAQ_CNT                      (0x20c0ull)
#define SSO_AF_TAQ_ADD                      (0x20e0ull)
#define SSO_AF_POISONX(a)                   (0x2100ull | (uint64_t)(a) << 3)
#define SSO_AF_POISONX_W1S(a)               (0x2200ull | (uint64_t)(a) << 3)
#define SSO_PRIV_AF_INT_CFG                 (0x3000ull)
#define SSO_AF_RVU_LF_CFG_DEBUG             (0x3800ull)
#define SSO_PRIV_LFX_HWGRP_CFG(a)           (0x10000ull | (uint64_t)(a) << 3)
#define SSO_PRIV_LFX_HWGRP_INT_CFG(a)       (0x20000ull | (uint64_t)(a) << 3)
#define SSO_AF_IU_ACCNTX_CFG(a)             (0x50000ull | (uint64_t)(a) << 3)
#define SSO_AF_IU_ACCNTX_RST(a)             (0x60000ull | (uint64_t)(a) << 3)
#define SSO_AF_XAQX_HEAD_PTR(a)             (0x80000ull | (uint64_t)(a) << 3)
#define SSO_AF_XAQX_TAIL_PTR(a)             (0x90000ull | (uint64_t)(a) << 3)
#define SSO_AF_XAQX_HEAD_NEXT(a)            (0xa0000ull | (uint64_t)(a) << 3)
#define SSO_AF_XAQX_TAIL_NEXT(a)            (0xb0000ull | (uint64_t)(a) << 3)
#define SSO_AF_TIAQX_STATUS(a)              (0xc0000ull | (uint64_t)(a) << 3)
#define SSO_AF_TOAQX_STATUS(a)              (0xd0000ull | (uint64_t)(a) << 3)
#define SSO_AF_XAQX_GMCTL(a)                (0xe0000ull | (uint64_t)(a) << 3)
#define SSO_AF_HWGRPX_IAQ_THR(a)            (0x200000ull | (uint64_t)(a) << 12)
#define SSO_AF_HWGRPX_TAQ_THR(a)            (0x200010ull | (uint64_t)(a) << 12)
#define SSO_AF_HWGRPX_PRI(a)                (0x200020ull | (uint64_t)(a) << 12)
#define SSO_AF_HWGRPX_WS_PC(a)              (0x200050ull | (uint64_t)(a) << 12)
#define SSO_AF_HWGRPX_EXT_PC(a)             (0x200060ull | (uint64_t)(a) << 12)
#define SSO_AF_HWGRPX_WA_PC(a)              (0x200070ull | (uint64_t)(a) << 12)
#define SSO_AF_HWGRPX_TS_PC(a)              (0x200080ull | (uint64_t)(a) << 12)
#define SSO_AF_HWGRPX_DS_PC(a)              (0x200090ull | (uint64_t)(a) << 12)
#define SSO_AF_HWGRPX_DQ_PC(a)              (0x2000A0ull | (uint64_t)(a) << 12)
#define SSO_AF_HWGRPX_PAGE_CNT(a)           (0x200100ull | (uint64_t)(a) << 12)
#define SSO_AF_HWGRPX_AW_STATUS(a)          (0x200110ull | (uint64_t)(a) << 12)
#define SSO_AF_HWGRPX_AW_CFG(a)             (0x200120ull | (uint64_t)(a) << 12)
#define SSO_AF_HWGRPX_AW_TAGSPACE(a)        (0x200130ull | (uint64_t)(a) << 12)
#define SSO_AF_HWGRPX_XAQ_AURA(a)           (0x200140ull | (uint64_t)(a) << 12)
#define SSO_AF_HWGRPX_XAQ_LIMIT(a)          (0x200220ull | (uint64_t)(a) << 12)
#define SSO_AF_HWGRPX_IU_ACCNT(a)           (0x200230ull | (uint64_t)(a) << 12)
#define SSO_AF_HWSX_ARB(a)                  (0x400100ull | (uint64_t)(a) << 12)
#define SSO_AF_HWSX_INV(a)                  (0x400180ull | (uint64_t)(a) << 12)
#define SSO_AF_HWSX_GMCTL(a)                (0x400200ull | (uint64_t)(a) << 12)
#define SSO_AF_HWSX_SX_GRPMSKX(a, b, c)     \
	(0x400400ull | (uint64_t)(a) << 12 | (uint64_t)(b) << 5 | \
	(uint64_t)(c) << 3)
#define SSO_AF_IPL_FREEX(a)                 (0x800000ull | (uint64_t)(a) << 3)
#define SSO_AF_IPL_IAQX(a)                  (0x840000ull | (uint64_t)(a) << 3)
#define SSO_AF_IPL_DESCHEDX(a)              (0x860000ull | (uint64_t)(a) << 3)
#define SSO_AF_IPL_CONFX(a)                 (0x880000ull | (uint64_t)(a) << 3)
#define SSO_AF_NPA_DIGESTX(a)               (0x900000ull | (uint64_t)(a) << 3)
#define SSO_AF_NPA_DIGESTX_W1S(a)           (0x900100ull | (uint64_t)(a) << 3)
#define SSO_AF_BFP_DIGESTX(a)               (0x900200ull | (uint64_t)(a) << 3)
#define SSO_AF_BFP_DIGESTX_W1S(a)           (0x900300ull | (uint64_t)(a) << 3)
#define SSO_AF_BFPN_DIGESTX(a)              (0x900400ull | (uint64_t)(a) << 3)
#define SSO_AF_BFPN_DIGESTX_W1S(a)          (0x900500ull | (uint64_t)(a) << 3)
#define SSO_AF_GRPDIS_DIGESTX(a)            (0x900600ull | (uint64_t)(a) << 3)
#define SSO_AF_GRPDIS_DIGESTX_W1S(a)        (0x900700ull | (uint64_t)(a) << 3)
#define SSO_AF_AWEMPTY_DIGESTX(a)           (0x900800ull | (uint64_t)(a) << 3)
#define SSO_AF_AWEMPTY_DIGESTX_W1S(a)       (0x900900ull | (uint64_t)(a) << 3)
#define SSO_AF_WQP0_DIGESTX(a)              (0x900a00ull | (uint64_t)(a) << 3)
#define SSO_AF_WQP0_DIGESTX_W1S(a)          (0x900b00ull | (uint64_t)(a) << 3)
#define SSO_AF_AW_DROPPED_DIGESTX(a)        (0x900c00ull | (uint64_t)(a) << 3)
#define SSO_AF_AW_DROPPED_DIGESTX_W1S(a)    (0x900d00ull | (uint64_t)(a) << 3)
#define SSO_AF_QCTLDIS_DIGESTX(a)           (0x900e00ull | (uint64_t)(a) << 3)
#define SSO_AF_QCTLDIS_DIGESTX_W1S(a)       (0x900f00ull | (uint64_t)(a) << 3)
#define SSO_AF_XAQDIS_DIGESTX(a)            (0x901000ull | (uint64_t)(a) << 3)
#define SSO_AF_XAQDIS_DIGESTX_W1S(a)        (0x901100ull | (uint64_t)(a) << 3)
#define SSO_AF_FLR_AQ_DIGESTX(a)            (0x901200ull | (uint64_t)(a) << 3)
#define SSO_AF_FLR_AQ_DIGESTX_W1S(a)        (0x901300ull | (uint64_t)(a) << 3)
#define SSO_AF_WS_GMULTI_DIGESTX(a)         (0x902000ull | (uint64_t)(a) << 3)
#define SSO_AF_WS_GMULTI_DIGESTX_W1S(a)     (0x902100ull | (uint64_t)(a) << 3)
#define SSO_AF_WS_GUNMAP_DIGESTX(a)         (0x902200ull | (uint64_t)(a) << 3)
#define SSO_AF_WS_GUNMAP_DIGESTX_W1S(a)     (0x902300ull | (uint64_t)(a) << 3)
#define SSO_AF_WS_AWE_DIGESTX(a)            (0x902400ull | (uint64_t)(a) << 3)
#define SSO_AF_WS_AWE_DIGESTX_W1S(a)        (0x902500ull | (uint64_t)(a) << 3)
#define SSO_AF_WS_GWI_DIGESTX(a)            (0x902600ull | (uint64_t)(a) << 3)
#define SSO_AF_WS_GWI_DIGESTX_W1S(a)        (0x902700ull | (uint64_t)(a) << 3)
#define SSO_AF_WS_NE_DIGESTX(a)             (0x902800ull | (uint64_t)(a) << 3)
#define SSO_AF_WS_NE_DIGESTX_W1S(a)         (0x902900ull | (uint64_t)(a) << 3)
#define SSO_AF_IENTX_TAG(a)                 (0xa00000ull | (uint64_t)(a) << 3)
#define SSO_AF_IENTX_GRP(a)                 (0xa20000ull | (uint64_t)(a) << 3)
#define SSO_AF_IENTX_PENDTAG(a)             (0xa40000ull | (uint64_t)(a) << 3)
#define SSO_AF_IENTX_LINKS(a)               (0xa60000ull | (uint64_t)(a) << 3)
#define SSO_AF_IENTX_QLINKS(a)              (0xa80000ull | (uint64_t)(a) << 3)
#define SSO_AF_IENTX_WQP(a)                 (0xaa0000ull | (uint64_t)(a) << 3)
#define SSO_AF_TAQX_LINK(a)                 (0xc00000ull | (uint64_t)(a) << 3)
#define SSO_AF_TAQX_WAEX_TAG(a, b)          \
	(0xe00000ull | (uint64_t)(a) << 8 | (uint64_t)(b) << 4)
#define SSO_AF_TAQX_WAEX_WQP(a, b)          \
	(0xe00008ull | (uint64_t)(a) << 8 | (uint64_t)(b) << 4)

#define SSO_LF_GGRP_OP_ADD_WORK0            (0x0ull)
#define SSO_LF_GGRP_OP_ADD_WORK1            (0x8ull)
#define SSO_LF_GGRP_QCTL                    (0x20ull)
#define SSO_LF_GGRP_EXE_DIS                 (0x80ull)
#define SSO_LF_GGRP_INT                     (0x100ull)
#define SSO_LF_GGRP_INT_W1S                 (0x108ull)
#define SSO_LF_GGRP_INT_ENA_W1S             (0x110ull)
#define SSO_LF_GGRP_INT_ENA_W1C             (0x118ull)
#define SSO_LF_GGRP_INT_THR                 (0x140ull)
#define SSO_LF_GGRP_INT_CNT                 (0x180ull)
#define SSO_LF_GGRP_XAQ_CNT                 (0x1b0ull)
#define SSO_LF_GGRP_AQ_CNT                  (0x1c0ull)
#define SSO_LF_GGRP_AQ_THR                  (0x1e0ull)
#define SSO_LF_GGRP_MISC_CNT                (0x200ull)

#define SSO_AF_IAQ_FREE_CNT_MASK        0x3FFFull
#define SSO_AF_IAQ_RSVD_FREE_MASK       0x3FFFull
#define SSO_AF_IAQ_RSVD_FREE_SHIFT      16
#define SSO_AF_IAQ_FREE_CNT_MAX         SSO_AF_IAQ_FREE_CNT_MASK
#define SSO_AF_AW_ADD_RSVD_FREE_MASK    0x3FFFull
#define SSO_AF_AW_ADD_RSVD_FREE_SHIFT   16
#define SSO_HWGRP_IAQ_MAX_THR_MASK      0x3FFFull
#define SSO_HWGRP_IAQ_RSVD_THR_MASK     0x3FFFull
#define SSO_HWGRP_IAQ_MAX_THR_SHIFT     32
#define SSO_HWGRP_IAQ_RSVD_THR          0x2

#define SSO_AF_TAQ_FREE_CNT_MASK        0x7FFull
#define SSO_AF_TAQ_RSVD_FREE_MASK       0x7FFull
#define SSO_AF_TAQ_RSVD_FREE_SHIFT      16
#define SSO_AF_TAQ_FREE_CNT_MAX         SSO_AF_TAQ_FREE_CNT_MASK
#define SSO_AF_TAQ_ADD_RSVD_FREE_MASK   0x1FFFull
#define SSO_AF_TAQ_ADD_RSVD_FREE_SHIFT  16
#define SSO_HWGRP_TAQ_MAX_THR_MASK      0x7FFull
#define SSO_HWGRP_TAQ_RSVD_THR_MASK     0x7FFull
#define SSO_HWGRP_TAQ_MAX_THR_SHIFT     32
#define SSO_HWGRP_TAQ_RSVD_THR          0x3

#define SSO_HWGRP_PRI_AFF_MASK          0xFull
#define SSO_HWGRP_PRI_AFF_SHIFT         8
#define SSO_HWGRP_PRI_WGT_MASK          0x3Full
#define SSO_HWGRP_PRI_WGT_SHIFT         16
#define SSO_HWGRP_PRI_WGT_LEFT_MASK     0x3Full
#define SSO_HWGRP_PRI_WGT_LEFT_SHIFT    24

#define SSO_HWGRP_AW_CFG_RWEN           BIT_ULL(0)
#define SSO_HWGRP_AW_CFG_LDWB           BIT_ULL(1)
#define SSO_HWGRP_AW_CFG_LDT            BIT_ULL(2)
#define SSO_HWGRP_AW_CFG_STT            BIT_ULL(3)
#define SSO_HWGRP_AW_CFG_XAQ_BYP_DIS    BIT_ULL(4)

#define SSO_HWGRP_AW_STS_TPTR_VLD       BIT_ULL(8)
#define SSO_HWGRP_AW_STS_NPA_FETCH      BIT_ULL(9)
#define SSO_HWGRP_AW_STS_XAQ_BUFSC_MASK 0x7ull
#define SSO_HWGRP_AW_STS_INIT_STS       0x18ull

/* Enum offsets */

#define SSO_LF_INT_VEC_GRP     (0x0ull)

#define SSO_AF_INT_VEC_ERR0    (0x0ull)
#define SSO_AF_INT_VEC_ERR2    (0x1ull)
#define SSO_AF_INT_VEC_RAS     (0x2ull)

#define SSO_WA_IOBN            (0x0ull)
#define SSO_WA_NIXRX           (0x1ull)
#define SSO_WA_CPT             (0x2ull)
#define SSO_WA_ADDWQ           (0x3ull)
#define SSO_WA_DPI             (0x4ull)
#define SSO_WA_NIXTX           (0x5ull)
#define SSO_WA_TIM             (0x6ull)
#define SSO_WA_ZIP             (0x7ull)

#define SSO_TT_ORDERED         (0x0ull)
#define SSO_TT_ATOMIC          (0x1ull)
#define SSO_TT_UNTAGGED        (0x2ull)
#define SSO_TT_EMPTY           (0x3ull)


/* Structures definitions */

#endif /* __OTX2_SSO_HW_H__ */
