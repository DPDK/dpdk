/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef __OTX2_SSOW_HW_H__
#define __OTX2_SSOW_HW_H__

/* Register offsets */

#define SSOW_AF_RVU_LF_HWS_CFG_DEBUG    (0x10ull)
#define SSOW_AF_LF_HWS_RST              (0x30ull)
#define SSOW_PRIV_LFX_HWS_CFG(a)        (0x1000ull | (uint64_t)(a) << 3)
#define SSOW_PRIV_LFX_HWS_INT_CFG(a)    (0x2000ull | (uint64_t)(a) << 3)
#define SSOW_AF_SCRATCH_WS              (0x100000ull)
#define SSOW_AF_SCRATCH_GW              (0x200000ull)
#define SSOW_AF_SCRATCH_AW              (0x300000ull)

#define SSOW_LF_GWS_LINKS               (0x10ull)
#define SSOW_LF_GWS_PENDWQP             (0x40ull)
#define SSOW_LF_GWS_PENDSTATE           (0x50ull)
#define SSOW_LF_GWS_NW_TIM              (0x70ull)
#define SSOW_LF_GWS_GRPMSK_CHG          (0x80ull)
#define SSOW_LF_GWS_INT                 (0x100ull)
#define SSOW_LF_GWS_INT_W1S             (0x108ull)
#define SSOW_LF_GWS_INT_ENA_W1S         (0x110ull)
#define SSOW_LF_GWS_INT_ENA_W1C         (0x118ull)
#define SSOW_LF_GWS_TAG                 (0x200ull)
#define SSOW_LF_GWS_WQP                 (0x210ull)
#define SSOW_LF_GWS_SWTP                (0x220ull)
#define SSOW_LF_GWS_PENDTAG             (0x230ull)
#define SSOW_LF_GWS_OP_ALLOC_WE         (0x400ull)
#define SSOW_LF_GWS_OP_GET_WORK         (0x600ull)
#define SSOW_LF_GWS_OP_SWTAG_FLUSH      (0x800ull)
#define SSOW_LF_GWS_OP_SWTAG_UNTAG      (0x810ull)
#define SSOW_LF_GWS_OP_SWTP_CLR         (0x820ull)
#define SSOW_LF_GWS_OP_UPD_WQP_GRP0     (0x830ull)
#define SSOW_LF_GWS_OP_UPD_WQP_GRP1     (0x838ull)
#define SSOW_LF_GWS_OP_DESCHED          (0x880ull)
#define SSOW_LF_GWS_OP_DESCHED_NOSCH    (0x8c0ull)
#define SSOW_LF_GWS_OP_SWTAG_DESCHED    (0x980ull)
#define SSOW_LF_GWS_OP_SWTAG_NOSCHED    (0x9c0ull)
#define SSOW_LF_GWS_OP_CLR_NSCHED0      (0xa00ull)
#define SSOW_LF_GWS_OP_CLR_NSCHED1      (0xa08ull)
#define SSOW_LF_GWS_OP_SWTP_SET         (0xc00ull)
#define SSOW_LF_GWS_OP_SWTAG_NORM       (0xc10ull)
#define SSOW_LF_GWS_OP_SWTAG_FULL0      (0xc20ull)
#define SSOW_LF_GWS_OP_SWTAG_FULL1      (0xc28ull)
#define SSOW_LF_GWS_OP_GWC_INVAL        (0xe00ull)


/* Enum offsets */

#define SSOW_LF_INT_VEC_IOP    (0x0ull)


#endif /* __OTX2_SSOW_HW_H__ */
