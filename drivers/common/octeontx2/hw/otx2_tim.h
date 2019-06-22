/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef __OTX2_TIM_HW_H__
#define __OTX2_TIM_HW_H__

/* TIM */
#define TIM_AF_CONST                    (0x90)
#define TIM_PRIV_LFX_CFG(a)             (0x20000 | (a) << 3)
#define TIM_PRIV_LFX_INT_CFG(a)         (0x24000 | (a) << 3)
#define TIM_AF_RVU_LF_CFG_DEBUG         (0x30000)
#define TIM_AF_BLK_RST                  (0x10)
#define TIM_AF_LF_RST                   (0x20)
#define TIM_AF_BLK_RST                  (0x10)
#define TIM_AF_RINGX_GMCTL(a)           (0x2000 | (a) << 3)
#define TIM_AF_RINGX_CTL0(a)            (0x4000 | (a) << 3)
#define TIM_AF_RINGX_CTL1(a)            (0x6000 | (a) << 3)
#define TIM_AF_RINGX_CTL2(a)            (0x8000 | (a) << 3)
#define TIM_AF_FLAGS_REG                (0x80)
#define TIM_AF_FLAGS_REG_ENA_TIM        BIT_ULL(0)
#define TIM_AF_RINGX_CTL1_ENA           BIT_ULL(47)
#define TIM_AF_RINGX_CTL1_RCF_BUSY      BIT_ULL(50)
#define TIM_AF_RINGX_CLT1_CLK_10NS      (0)
#define TIM_AF_RINGX_CLT1_CLK_GPIO      (1)
#define TIM_AF_RINGX_CLT1_CLK_GTI       (2)
#define TIM_AF_RINGX_CLT1_CLK_PTP       (3)

/* ENUMS */

#define TIM_LF_INT_VEC_NRSPERR_INT	(0x0ull)
#define TIM_LF_INT_VEC_RAS_INT		(0x1ull)

#endif /* __OTX2_TIM_HW_H__ */
