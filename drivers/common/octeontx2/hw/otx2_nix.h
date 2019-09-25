/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef __OTX2_NIX_HW_H__
#define __OTX2_NIX_HW_H__

/* Register offsets */

#define NIX_AF_CFG                              (0x0ull)
#define NIX_AF_STATUS                           (0x10ull)
#define NIX_AF_NDC_CFG                          (0x18ull)
#define NIX_AF_CONST                            (0x20ull)
#define NIX_AF_CONST1                           (0x28ull)
#define NIX_AF_CONST2                           (0x30ull)
#define NIX_AF_CONST3                           (0x38ull)
#define NIX_AF_SQ_CONST                         (0x40ull)
#define NIX_AF_CQ_CONST                         (0x48ull)
#define NIX_AF_RQ_CONST                         (0x50ull)
#define NIX_AF_PSE_CONST                        (0x60ull)
#define NIX_AF_TL1_CONST                        (0x70ull)
#define NIX_AF_TL2_CONST                        (0x78ull)
#define NIX_AF_TL3_CONST                        (0x80ull)
#define NIX_AF_TL4_CONST                        (0x88ull)
#define NIX_AF_MDQ_CONST                        (0x90ull)
#define NIX_AF_MC_MIRROR_CONST                  (0x98ull)
#define NIX_AF_LSO_CFG                          (0xa8ull)
#define NIX_AF_BLK_RST                          (0xb0ull)
#define NIX_AF_TX_TSTMP_CFG                     (0xc0ull)
#define NIX_AF_RX_CFG                           (0xd0ull)
#define NIX_AF_AVG_DELAY                        (0xe0ull)
#define NIX_AF_CINT_DELAY                       (0xf0ull)
#define NIX_AF_RX_MCAST_BASE                    (0x100ull)
#define NIX_AF_RX_MCAST_CFG                     (0x110ull)
#define NIX_AF_RX_MCAST_BUF_BASE                (0x120ull)
#define NIX_AF_RX_MCAST_BUF_CFG                 (0x130ull)
#define NIX_AF_RX_MIRROR_BUF_BASE               (0x140ull)
#define NIX_AF_RX_MIRROR_BUF_CFG                (0x148ull)
#define NIX_AF_LF_RST                           (0x150ull)
#define NIX_AF_GEN_INT                          (0x160ull)
#define NIX_AF_GEN_INT_W1S                      (0x168ull)
#define NIX_AF_GEN_INT_ENA_W1S                  (0x170ull)
#define NIX_AF_GEN_INT_ENA_W1C                  (0x178ull)
#define NIX_AF_ERR_INT                          (0x180ull)
#define NIX_AF_ERR_INT_W1S                      (0x188ull)
#define NIX_AF_ERR_INT_ENA_W1S                  (0x190ull)
#define NIX_AF_ERR_INT_ENA_W1C                  (0x198ull)
#define NIX_AF_RAS                              (0x1a0ull)
#define NIX_AF_RAS_W1S                          (0x1a8ull)
#define NIX_AF_RAS_ENA_W1S                      (0x1b0ull)
#define NIX_AF_RAS_ENA_W1C                      (0x1b8ull)
#define NIX_AF_RVU_INT                          (0x1c0ull)
#define NIX_AF_RVU_INT_W1S                      (0x1c8ull)
#define NIX_AF_RVU_INT_ENA_W1S                  (0x1d0ull)
#define NIX_AF_RVU_INT_ENA_W1C                  (0x1d8ull)
#define NIX_AF_TCP_TIMER                        (0x1e0ull)
#define NIX_AF_RX_DEF_OL2                       (0x200ull)
#define NIX_AF_RX_DEF_OIP4                      (0x210ull)
#define NIX_AF_RX_DEF_IIP4                      (0x220ull)
#define NIX_AF_RX_DEF_OIP6                      (0x230ull)
#define NIX_AF_RX_DEF_IIP6                      (0x240ull)
#define NIX_AF_RX_DEF_OTCP                      (0x250ull)
#define NIX_AF_RX_DEF_ITCP                      (0x260ull)
#define NIX_AF_RX_DEF_OUDP                      (0x270ull)
#define NIX_AF_RX_DEF_IUDP                      (0x280ull)
#define NIX_AF_RX_DEF_OSCTP                     (0x290ull)
#define NIX_AF_RX_DEF_ISCTP                     (0x2a0ull)
#define NIX_AF_RX_DEF_IPSECX(a)                 (0x2b0ull | (uint64_t)(a) << 3)
#define NIX_AF_RX_IPSEC_GEN_CFG                 (0x300ull)
#define NIX_AF_RX_CPTX_INST_QSEL(a)             (0x320ull | (uint64_t)(a) << 3)
#define NIX_AF_RX_CPTX_CREDIT(a)                (0x360ull | (uint64_t)(a) << 3)
#define NIX_AF_NDC_RX_SYNC                      (0x3e0ull)
#define NIX_AF_NDC_TX_SYNC                      (0x3f0ull)
#define NIX_AF_AQ_CFG                           (0x400ull)
#define NIX_AF_AQ_BASE                          (0x410ull)
#define NIX_AF_AQ_STATUS                        (0x420ull)
#define NIX_AF_AQ_DOOR                          (0x430ull)
#define NIX_AF_AQ_DONE_WAIT                     (0x440ull)
#define NIX_AF_AQ_DONE                          (0x450ull)
#define NIX_AF_AQ_DONE_ACK                      (0x460ull)
#define NIX_AF_AQ_DONE_TIMER                    (0x470ull)
#define NIX_AF_AQ_DONE_ENA_W1S                  (0x490ull)
#define NIX_AF_AQ_DONE_ENA_W1C                  (0x498ull)
#define NIX_AF_RX_LINKX_CFG(a)                  (0x540ull | (uint64_t)(a) << 16)
#define NIX_AF_RX_SW_SYNC                       (0x550ull)
#define NIX_AF_RX_LINKX_WRR_CFG(a)              (0x560ull | (uint64_t)(a) << 16)
#define NIX_AF_EXPR_TX_FIFO_STATUS              (0x640ull)
#define NIX_AF_NORM_TX_FIFO_STATUS              (0x648ull)
#define NIX_AF_SDP_TX_FIFO_STATUS               (0x650ull)
#define NIX_AF_TX_NPC_CAPTURE_CONFIG            (0x660ull)
#define NIX_AF_TX_NPC_CAPTURE_INFO              (0x668ull)
#define NIX_AF_TX_NPC_CAPTURE_RESPX(a)          (0x680ull | (uint64_t)(a) << 3)
#define NIX_AF_SEB_ACTIVE_CYCLES_PCX(a)         (0x6c0ull | (uint64_t)(a) << 3)
#define NIX_AF_SMQX_CFG(a)                      (0x700ull | (uint64_t)(a) << 16)
#define NIX_AF_SMQX_HEAD(a)                     (0x710ull | (uint64_t)(a) << 16)
#define NIX_AF_SMQX_TAIL(a)                     (0x720ull | (uint64_t)(a) << 16)
#define NIX_AF_SMQX_STATUS(a)                   (0x730ull | (uint64_t)(a) << 16)
#define NIX_AF_SMQX_NXT_HEAD(a)                 (0x740ull | (uint64_t)(a) << 16)
#define NIX_AF_SQM_ACTIVE_CYCLES_PC             (0x770ull)
#define NIX_AF_PSE_CHANNEL_LEVEL                (0x800ull)
#define NIX_AF_PSE_SHAPER_CFG                   (0x810ull)
#define NIX_AF_PSE_ACTIVE_CYCLES_PC             (0x8c0ull)
#define NIX_AF_MARK_FORMATX_CTL(a)              (0x900ull | (uint64_t)(a) << 18)
#define NIX_AF_TX_LINKX_NORM_CREDIT(a)          (0xa00ull | (uint64_t)(a) << 16)
#define NIX_AF_TX_LINKX_EXPR_CREDIT(a)          (0xa10ull | (uint64_t)(a) << 16)
#define NIX_AF_TX_LINKX_SW_XOFF(a)              (0xa20ull | (uint64_t)(a) << 16)
#define NIX_AF_TX_LINKX_HW_XOFF(a)              (0xa30ull | (uint64_t)(a) << 16)
#define NIX_AF_SDP_LINK_CREDIT                  (0xa40ull)
#define NIX_AF_SDP_SW_XOFFX(a)                  (0xa60ull | (uint64_t)(a) << 3)
#define NIX_AF_SDP_HW_XOFFX(a)                  (0xac0ull | (uint64_t)(a) << 3)
#define NIX_AF_TL4X_BP_STATUS(a)                (0xb00ull | (uint64_t)(a) << 16)
#define NIX_AF_TL4X_SDP_LINK_CFG(a)             (0xb10ull | (uint64_t)(a) << 16)
#define NIX_AF_TL1X_SCHEDULE(a)                 (0xc00ull | (uint64_t)(a) << 16)
#define NIX_AF_TL1X_SHAPE(a)                    (0xc10ull | (uint64_t)(a) << 16)
#define NIX_AF_TL1X_CIR(a)                      (0xc20ull | (uint64_t)(a) << 16)
#define NIX_AF_TL1X_SHAPE_STATE(a)              (0xc50ull | (uint64_t)(a) << 16)
#define NIX_AF_TL1X_SW_XOFF(a)                  (0xc70ull | (uint64_t)(a) << 16)
#define NIX_AF_TL1X_TOPOLOGY(a)                 (0xc80ull | (uint64_t)(a) << 16)
#define NIX_AF_TL1X_MD_DEBUG0(a)                (0xcc0ull | (uint64_t)(a) << 16)
#define NIX_AF_TL1X_MD_DEBUG1(a)                (0xcc8ull | (uint64_t)(a) << 16)
#define NIX_AF_TL1X_MD_DEBUG2(a)                (0xcd0ull | (uint64_t)(a) << 16)
#define NIX_AF_TL1X_MD_DEBUG3(a)                (0xcd8ull | (uint64_t)(a) << 16)
#define NIX_AF_TL1X_DROPPED_PACKETS(a)          (0xd20ull | (uint64_t)(a) << 16)
#define NIX_AF_TL1X_DROPPED_BYTES(a)            (0xd30ull | (uint64_t)(a) << 16)
#define NIX_AF_TL1X_RED_PACKETS(a)              (0xd40ull | (uint64_t)(a) << 16)
#define NIX_AF_TL1X_RED_BYTES(a)                (0xd50ull | (uint64_t)(a) << 16)
#define NIX_AF_TL1X_YELLOW_PACKETS(a)           (0xd60ull | (uint64_t)(a) << 16)
#define NIX_AF_TL1X_YELLOW_BYTES(a)             (0xd70ull | (uint64_t)(a) << 16)
#define NIX_AF_TL1X_GREEN_PACKETS(a)            (0xd80ull | (uint64_t)(a) << 16)
#define NIX_AF_TL1X_GREEN_BYTES(a)              (0xd90ull | (uint64_t)(a) << 16)
#define NIX_AF_TL2X_SCHEDULE(a)                 (0xe00ull | (uint64_t)(a) << 16)
#define NIX_AF_TL2X_SHAPE(a)                    (0xe10ull | (uint64_t)(a) << 16)
#define NIX_AF_TL2X_CIR(a)                      (0xe20ull | (uint64_t)(a) << 16)
#define NIX_AF_TL2X_PIR(a)                      (0xe30ull | (uint64_t)(a) << 16)
#define NIX_AF_TL2X_SCHED_STATE(a)              (0xe40ull | (uint64_t)(a) << 16)
#define NIX_AF_TL2X_SHAPE_STATE(a)              (0xe50ull | (uint64_t)(a) << 16)
#define NIX_AF_TL2X_SW_XOFF(a)                  (0xe70ull | (uint64_t)(a) << 16)
#define NIX_AF_TL2X_TOPOLOGY(a)                 (0xe80ull | (uint64_t)(a) << 16)
#define NIX_AF_TL2X_PARENT(a)                   (0xe88ull | (uint64_t)(a) << 16)
#define NIX_AF_TL2X_MD_DEBUG0(a)                (0xec0ull | (uint64_t)(a) << 16)
#define NIX_AF_TL2X_MD_DEBUG1(a)                (0xec8ull | (uint64_t)(a) << 16)
#define NIX_AF_TL2X_MD_DEBUG2(a)                (0xed0ull | (uint64_t)(a) << 16)
#define NIX_AF_TL2X_MD_DEBUG3(a)                (0xed8ull | (uint64_t)(a) << 16)
#define NIX_AF_TL3X_SCHEDULE(a)                 \
	(0x1000ull | (uint64_t)(a) << 16)
#define NIX_AF_TL3X_SHAPE(a)                    \
	(0x1010ull | (uint64_t)(a) << 16)
#define NIX_AF_TL3X_CIR(a)                      \
	(0x1020ull | (uint64_t)(a) << 16)
#define NIX_AF_TL3X_PIR(a)                      \
	(0x1030ull | (uint64_t)(a) << 16)
#define NIX_AF_TL3X_SCHED_STATE(a)              \
	(0x1040ull | (uint64_t)(a) << 16)
#define NIX_AF_TL3X_SHAPE_STATE(a)              \
	(0x1050ull | (uint64_t)(a) << 16)
#define NIX_AF_TL3X_SW_XOFF(a)                  \
	(0x1070ull | (uint64_t)(a) << 16)
#define NIX_AF_TL3X_TOPOLOGY(a)                 \
	(0x1080ull | (uint64_t)(a) << 16)
#define NIX_AF_TL3X_PARENT(a)                   \
	(0x1088ull | (uint64_t)(a) << 16)
#define NIX_AF_TL3X_MD_DEBUG0(a)                \
	(0x10c0ull | (uint64_t)(a) << 16)
#define NIX_AF_TL3X_MD_DEBUG1(a)                \
	(0x10c8ull | (uint64_t)(a) << 16)
#define NIX_AF_TL3X_MD_DEBUG2(a)                \
	(0x10d0ull | (uint64_t)(a) << 16)
#define NIX_AF_TL3X_MD_DEBUG3(a)                \
	(0x10d8ull | (uint64_t)(a) << 16)
#define NIX_AF_TL4X_SCHEDULE(a)                 \
	(0x1200ull | (uint64_t)(a) << 16)
#define NIX_AF_TL4X_SHAPE(a)                    \
	(0x1210ull | (uint64_t)(a) << 16)
#define NIX_AF_TL4X_CIR(a)                      \
	(0x1220ull | (uint64_t)(a) << 16)
#define NIX_AF_TL4X_PIR(a)                      \
	(0x1230ull | (uint64_t)(a) << 16)
#define NIX_AF_TL4X_SCHED_STATE(a)              \
	(0x1240ull | (uint64_t)(a) << 16)
#define NIX_AF_TL4X_SHAPE_STATE(a)              \
	(0x1250ull | (uint64_t)(a) << 16)
#define NIX_AF_TL4X_SW_XOFF(a)                  \
	(0x1270ull | (uint64_t)(a) << 16)
#define NIX_AF_TL4X_TOPOLOGY(a)                 \
	(0x1280ull | (uint64_t)(a) << 16)
#define NIX_AF_TL4X_PARENT(a)                   \
	(0x1288ull | (uint64_t)(a) << 16)
#define NIX_AF_TL4X_MD_DEBUG0(a)                \
	(0x12c0ull | (uint64_t)(a) << 16)
#define NIX_AF_TL4X_MD_DEBUG1(a)                \
	(0x12c8ull | (uint64_t)(a) << 16)
#define NIX_AF_TL4X_MD_DEBUG2(a)                \
	(0x12d0ull | (uint64_t)(a) << 16)
#define NIX_AF_TL4X_MD_DEBUG3(a)                \
	(0x12d8ull | (uint64_t)(a) << 16)
#define NIX_AF_MDQX_SCHEDULE(a)                 \
	(0x1400ull | (uint64_t)(a) << 16)
#define NIX_AF_MDQX_SHAPE(a)                    \
	(0x1410ull | (uint64_t)(a) << 16)
#define NIX_AF_MDQX_CIR(a)                      \
	(0x1420ull | (uint64_t)(a) << 16)
#define NIX_AF_MDQX_PIR(a)                      \
	(0x1430ull | (uint64_t)(a) << 16)
#define NIX_AF_MDQX_SCHED_STATE(a)              \
	(0x1440ull | (uint64_t)(a) << 16)
#define NIX_AF_MDQX_SHAPE_STATE(a)              \
	(0x1450ull | (uint64_t)(a) << 16)
#define NIX_AF_MDQX_SW_XOFF(a)                  \
	(0x1470ull | (uint64_t)(a) << 16)
#define NIX_AF_MDQX_PARENT(a)                   \
	(0x1480ull | (uint64_t)(a) << 16)
#define NIX_AF_MDQX_MD_DEBUG(a)                 \
	(0x14c0ull | (uint64_t)(a) << 16)
#define NIX_AF_TL3_TL2X_CFG(a)                  \
	(0x1600ull | (uint64_t)(a) << 16)
#define NIX_AF_TL3_TL2X_BP_STATUS(a)            \
	(0x1610ull | (uint64_t)(a) << 16)
#define NIX_AF_TL3_TL2X_LINKX_CFG(a, b)         \
	(0x1700ull | (uint64_t)(a) << 16 | (uint64_t)(b) << 3)
#define NIX_AF_RX_FLOW_KEY_ALGX_FIELDX(a, b)    \
	(0x1800ull | (uint64_t)(a) << 18 | (uint64_t)(b) << 3)
#define NIX_AF_TX_MCASTX(a)                     \
	(0x1900ull | (uint64_t)(a) << 15)
#define NIX_AF_TX_VTAG_DEFX_CTL(a)              \
	(0x1a00ull | (uint64_t)(a) << 16)
#define NIX_AF_TX_VTAG_DEFX_DATA(a)             \
	(0x1a10ull | (uint64_t)(a) << 16)
#define NIX_AF_RX_BPIDX_STATUS(a)               \
	(0x1a20ull | (uint64_t)(a) << 17)
#define NIX_AF_RX_CHANX_CFG(a)                  \
	(0x1a30ull | (uint64_t)(a) << 15)
#define NIX_AF_CINT_TIMERX(a)                   \
	(0x1a40ull | (uint64_t)(a) << 18)
#define NIX_AF_LSO_FORMATX_FIELDX(a, b)         \
	(0x1b00ull | (uint64_t)(a) << 16 | (uint64_t)(b) << 3)
#define NIX_AF_LFX_CFG(a)                       \
	(0x4000ull | (uint64_t)(a) << 17)
#define NIX_AF_LFX_SQS_CFG(a)                   \
	(0x4020ull | (uint64_t)(a) << 17)
#define NIX_AF_LFX_TX_CFG2(a)                   \
	(0x4028ull | (uint64_t)(a) << 17)
#define NIX_AF_LFX_SQS_BASE(a)                  \
	(0x4030ull | (uint64_t)(a) << 17)
#define NIX_AF_LFX_RQS_CFG(a)                   \
	(0x4040ull | (uint64_t)(a) << 17)
#define NIX_AF_LFX_RQS_BASE(a)                  \
	(0x4050ull | (uint64_t)(a) << 17)
#define NIX_AF_LFX_CQS_CFG(a)                   \
	(0x4060ull | (uint64_t)(a) << 17)
#define NIX_AF_LFX_CQS_BASE(a)                  \
	(0x4070ull | (uint64_t)(a) << 17)
#define NIX_AF_LFX_TX_CFG(a)                    \
	(0x4080ull | (uint64_t)(a) << 17)
#define NIX_AF_LFX_TX_PARSE_CFG(a)              \
	(0x4090ull | (uint64_t)(a) << 17)
#define NIX_AF_LFX_RX_CFG(a)                    \
	(0x40a0ull | (uint64_t)(a) << 17)
#define NIX_AF_LFX_RSS_CFG(a)                   \
	(0x40c0ull | (uint64_t)(a) << 17)
#define NIX_AF_LFX_RSS_BASE(a)                  \
	(0x40d0ull | (uint64_t)(a) << 17)
#define NIX_AF_LFX_QINTS_CFG(a)                 \
	(0x4100ull | (uint64_t)(a) << 17)
#define NIX_AF_LFX_QINTS_BASE(a)                \
	(0x4110ull | (uint64_t)(a) << 17)
#define NIX_AF_LFX_CINTS_CFG(a)                 \
	(0x4120ull | (uint64_t)(a) << 17)
#define NIX_AF_LFX_CINTS_BASE(a)                \
	(0x4130ull | (uint64_t)(a) << 17)
#define NIX_AF_LFX_RX_IPSEC_CFG0(a)             \
	(0x4140ull | (uint64_t)(a) << 17)
#define NIX_AF_LFX_RX_IPSEC_CFG1(a)             \
	(0x4148ull | (uint64_t)(a) << 17)
#define NIX_AF_LFX_RX_IPSEC_DYNO_CFG(a)         \
	(0x4150ull | (uint64_t)(a) << 17)
#define NIX_AF_LFX_RX_IPSEC_DYNO_BASE(a)        \
	(0x4158ull | (uint64_t)(a) << 17)
#define NIX_AF_LFX_RX_IPSEC_SA_BASE(a)          \
	(0x4170ull | (uint64_t)(a) << 17)
#define NIX_AF_LFX_TX_STATUS(a)                 \
	(0x4180ull | (uint64_t)(a) << 17)
#define NIX_AF_LFX_RX_VTAG_TYPEX(a, b)          \
	(0x4200ull | (uint64_t)(a) << 17 | (uint64_t)(b) << 3)
#define NIX_AF_LFX_LOCKX(a, b)                  \
	(0x4300ull | (uint64_t)(a) << 17 | (uint64_t)(b) << 3)
#define NIX_AF_LFX_TX_STATX(a, b)               \
	(0x4400ull | (uint64_t)(a) << 17 | (uint64_t)(b) << 3)
#define NIX_AF_LFX_RX_STATX(a, b)               \
	(0x4500ull | (uint64_t)(a) << 17 | (uint64_t)(b) << 3)
#define NIX_AF_LFX_RSS_GRPX(a, b)               \
	(0x4600ull | (uint64_t)(a) << 17 | (uint64_t)(b) << 3)
#define NIX_AF_RX_NPC_MC_RCV                    (0x4700ull)
#define NIX_AF_RX_NPC_MC_DROP                   (0x4710ull)
#define NIX_AF_RX_NPC_MIRROR_RCV                (0x4720ull)
#define NIX_AF_RX_NPC_MIRROR_DROP               (0x4730ull)
#define NIX_AF_RX_ACTIVE_CYCLES_PCX(a)          \
	(0x4800ull | (uint64_t)(a) << 16)
#define NIX_PRIV_AF_INT_CFG                     (0x8000000ull)
#define NIX_PRIV_LFX_CFG(a)                     \
	(0x8000010ull | (uint64_t)(a) << 8)
#define NIX_PRIV_LFX_INT_CFG(a)                 \
	(0x8000020ull | (uint64_t)(a) << 8)
#define NIX_AF_RVU_LF_CFG_DEBUG                 (0x8000030ull)

#define NIX_LF_RX_SECRETX(a)                    (0x0ull | (uint64_t)(a) << 3)
#define NIX_LF_CFG                              (0x100ull)
#define NIX_LF_GINT                             (0x200ull)
#define NIX_LF_GINT_W1S                         (0x208ull)
#define NIX_LF_GINT_ENA_W1C                     (0x210ull)
#define NIX_LF_GINT_ENA_W1S                     (0x218ull)
#define NIX_LF_ERR_INT                          (0x220ull)
#define NIX_LF_ERR_INT_W1S                      (0x228ull)
#define NIX_LF_ERR_INT_ENA_W1C                  (0x230ull)
#define NIX_LF_ERR_INT_ENA_W1S                  (0x238ull)
#define NIX_LF_RAS                              (0x240ull)
#define NIX_LF_RAS_W1S                          (0x248ull)
#define NIX_LF_RAS_ENA_W1C                      (0x250ull)
#define NIX_LF_RAS_ENA_W1S                      (0x258ull)
#define NIX_LF_SQ_OP_ERR_DBG                    (0x260ull)
#define NIX_LF_MNQ_ERR_DBG                      (0x270ull)
#define NIX_LF_SEND_ERR_DBG                     (0x280ull)
#define NIX_LF_TX_STATX(a)                      (0x300ull | (uint64_t)(a) << 3)
#define NIX_LF_RX_STATX(a)                      (0x400ull | (uint64_t)(a) << 3)
#define NIX_LF_OP_SENDX(a)                      (0x800ull | (uint64_t)(a) << 3)
#define NIX_LF_RQ_OP_INT                        (0x900ull)
#define NIX_LF_RQ_OP_OCTS                       (0x910ull)
#define NIX_LF_RQ_OP_PKTS                       (0x920ull)
#define NIX_LF_RQ_OP_DROP_OCTS                  (0x930ull)
#define NIX_LF_RQ_OP_DROP_PKTS                  (0x940ull)
#define NIX_LF_RQ_OP_RE_PKTS                    (0x950ull)
#define NIX_LF_OP_IPSEC_DYNO_CNT                (0x980ull)
#define NIX_LF_SQ_OP_INT                        (0xa00ull)
#define NIX_LF_SQ_OP_OCTS                       (0xa10ull)
#define NIX_LF_SQ_OP_PKTS                       (0xa20ull)
#define NIX_LF_SQ_OP_STATUS                     (0xa30ull)
#define NIX_LF_SQ_OP_DROP_OCTS                  (0xa40ull)
#define NIX_LF_SQ_OP_DROP_PKTS                  (0xa50ull)
#define NIX_LF_CQ_OP_INT                        (0xb00ull)
#define NIX_LF_CQ_OP_DOOR                       (0xb30ull)
#define NIX_LF_CQ_OP_STATUS                     (0xb40ull)
#define NIX_LF_QINTX_CNT(a)                     (0xc00ull | (uint64_t)(a) << 12)
#define NIX_LF_QINTX_INT(a)                     (0xc10ull | (uint64_t)(a) << 12)
#define NIX_LF_QINTX_ENA_W1S(a)                 (0xc20ull | (uint64_t)(a) << 12)
#define NIX_LF_QINTX_ENA_W1C(a)                 (0xc30ull | (uint64_t)(a) << 12)
#define NIX_LF_CINTX_CNT(a)                     (0xd00ull | (uint64_t)(a) << 12)
#define NIX_LF_CINTX_WAIT(a)                    (0xd10ull | (uint64_t)(a) << 12)
#define NIX_LF_CINTX_INT(a)                     (0xd20ull | (uint64_t)(a) << 12)
#define NIX_LF_CINTX_INT_W1S(a)                 (0xd30ull | (uint64_t)(a) << 12)
#define NIX_LF_CINTX_ENA_W1S(a)                 (0xd40ull | (uint64_t)(a) << 12)
#define NIX_LF_CINTX_ENA_W1C(a)                 (0xd50ull | (uint64_t)(a) << 12)


/* Enum offsets */

#define NIX_TX_VTAGOP_NOP                     (0x0ull)
#define NIX_TX_VTAGOP_INSERT                  (0x1ull)
#define NIX_TX_VTAGOP_REPLACE                 (0x2ull)

#define NIX_TX_ACTIONOP_DROP                  (0x0ull)
#define NIX_TX_ACTIONOP_UCAST_DEFAULT         (0x1ull)
#define NIX_TX_ACTIONOP_UCAST_CHAN            (0x2ull)
#define NIX_TX_ACTIONOP_MCAST                 (0x3ull)
#define NIX_TX_ACTIONOP_DROP_VIOL             (0x5ull)

#define NIX_INTF_RX                           (0x0ull)
#define NIX_INTF_TX                           (0x1ull)

#define NIX_TXLAYER_OL3                       (0x0ull)
#define NIX_TXLAYER_OL4                       (0x1ull)
#define NIX_TXLAYER_IL3                       (0x2ull)
#define NIX_TXLAYER_IL4                       (0x3ull)

#define NIX_SUBDC_NOP                         (0x0ull)
#define NIX_SUBDC_EXT                         (0x1ull)
#define NIX_SUBDC_CRC                         (0x2ull)
#define NIX_SUBDC_IMM                         (0x3ull)
#define NIX_SUBDC_SG                          (0x4ull)
#define NIX_SUBDC_MEM                         (0x5ull)
#define NIX_SUBDC_JUMP                        (0x6ull)
#define NIX_SUBDC_WORK                        (0x7ull)
#define NIX_SUBDC_SOD                         (0xfull)

#define NIX_STYPE_STF                         (0x0ull)
#define NIX_STYPE_STT                         (0x1ull)
#define NIX_STYPE_STP                         (0x2ull)

#define NIX_STAT_LF_TX_TX_UCAST               (0x0ull)
#define NIX_STAT_LF_TX_TX_BCAST               (0x1ull)
#define NIX_STAT_LF_TX_TX_MCAST               (0x2ull)
#define NIX_STAT_LF_TX_TX_DROP                (0x3ull)
#define NIX_STAT_LF_TX_TX_OCTS                (0x4ull)

#define NIX_STAT_LF_RX_RX_OCTS                (0x0ull)
#define NIX_STAT_LF_RX_RX_UCAST               (0x1ull)
#define NIX_STAT_LF_RX_RX_BCAST               (0x2ull)
#define NIX_STAT_LF_RX_RX_MCAST               (0x3ull)
#define NIX_STAT_LF_RX_RX_DROP                (0x4ull)
#define NIX_STAT_LF_RX_RX_DROP_OCTS           (0x5ull)
#define NIX_STAT_LF_RX_RX_FCS                 (0x6ull)
#define NIX_STAT_LF_RX_RX_ERR                 (0x7ull)
#define NIX_STAT_LF_RX_RX_DRP_BCAST           (0x8ull)
#define NIX_STAT_LF_RX_RX_DRP_MCAST           (0x9ull)
#define NIX_STAT_LF_RX_RX_DRP_L3BCAST         (0xaull)
#define NIX_STAT_LF_RX_RX_DRP_L3MCAST         (0xbull)

#define NIX_SQOPERR_SQ_OOR                    (0x0ull)
#define NIX_SQOPERR_SQ_CTX_FAULT              (0x1ull)
#define NIX_SQOPERR_SQ_CTX_POISON             (0x2ull)
#define NIX_SQOPERR_SQ_DISABLED               (0x3ull)
#define NIX_SQOPERR_MAX_SQE_SIZE_ERR          (0x4ull)
#define NIX_SQOPERR_SQE_OFLOW                 (0x5ull)
#define NIX_SQOPERR_SQB_NULL                  (0x6ull)
#define NIX_SQOPERR_SQB_FAULT                 (0x7ull)

#define NIX_XQESZ_W64                         (0x0ull)
#define NIX_XQESZ_W16                         (0x1ull)

#define NIX_VTAGSIZE_T4                       (0x0ull)
#define NIX_VTAGSIZE_T8                       (0x1ull)

#define NIX_RX_ACTIONOP_DROP                  (0x0ull)
#define NIX_RX_ACTIONOP_UCAST                 (0x1ull)
#define NIX_RX_ACTIONOP_UCAST_IPSEC           (0x2ull)
#define NIX_RX_ACTIONOP_MCAST                 (0x3ull)
#define NIX_RX_ACTIONOP_RSS                   (0x4ull)
#define NIX_RX_ACTIONOP_PF_FUNC_DROP          (0x5ull)
#define NIX_RX_ACTIONOP_MIRROR                (0x6ull)

#define NIX_RX_VTAGACTION_VTAG0_RELPTR        (0x0ull)
#define NIX_RX_VTAGACTION_VTAG1_RELPTR        (0x4ull)
#define NIX_RX_VTAGACTION_VTAG_VALID          (0x1ull)
#define NIX_TX_VTAGACTION_VTAG0_RELPTR        \
	(sizeof(struct nix_inst_hdr_s) + 2 * 6)
#define NIX_TX_VTAGACTION_VTAG1_RELPTR        \
	(sizeof(struct nix_inst_hdr_s) + 2 * 6 + 4)
#define NIX_RQINT_DROP                        (0x0ull)
#define NIX_RQINT_RED                         (0x1ull)
#define NIX_RQINT_R2                          (0x2ull)
#define NIX_RQINT_R3                          (0x3ull)
#define NIX_RQINT_R4                          (0x4ull)
#define NIX_RQINT_R5                          (0x5ull)
#define NIX_RQINT_R6                          (0x6ull)
#define NIX_RQINT_R7                          (0x7ull)

#define NIX_MAXSQESZ_W16                      (0x0ull)
#define NIX_MAXSQESZ_W8                       (0x1ull)

#define NIX_LSOALG_NOP                        (0x0ull)
#define NIX_LSOALG_ADD_SEGNUM                 (0x1ull)
#define NIX_LSOALG_ADD_PAYLEN                 (0x2ull)
#define NIX_LSOALG_ADD_OFFSET                 (0x3ull)
#define NIX_LSOALG_TCP_FLAGS                  (0x4ull)

#define NIX_MNQERR_SQ_CTX_FAULT               (0x0ull)
#define NIX_MNQERR_SQ_CTX_POISON              (0x1ull)
#define NIX_MNQERR_SQB_FAULT                  (0x2ull)
#define NIX_MNQERR_SQB_POISON                 (0x3ull)
#define NIX_MNQERR_TOTAL_ERR                  (0x4ull)
#define NIX_MNQERR_LSO_ERR                    (0x5ull)
#define NIX_MNQERR_CQ_QUERY_ERR               (0x6ull)
#define NIX_MNQERR_MAX_SQE_SIZE_ERR           (0x7ull)
#define NIX_MNQERR_MAXLEN_ERR                 (0x8ull)
#define NIX_MNQERR_SQE_SIZEM1_ZERO            (0x9ull)

#define NIX_MDTYPE_RSVD                       (0x0ull)
#define NIX_MDTYPE_FLUSH                      (0x1ull)
#define NIX_MDTYPE_PMD                        (0x2ull)

#define NIX_NDC_TX_PORT_LMT                   (0x0ull)
#define NIX_NDC_TX_PORT_ENQ                   (0x1ull)
#define NIX_NDC_TX_PORT_MNQ                   (0x2ull)
#define NIX_NDC_TX_PORT_DEQ                   (0x3ull)
#define NIX_NDC_TX_PORT_DMA                   (0x4ull)
#define NIX_NDC_TX_PORT_XQE                   (0x5ull)

#define NIX_NDC_RX_PORT_AQ                    (0x0ull)
#define NIX_NDC_RX_PORT_CQ                    (0x1ull)
#define NIX_NDC_RX_PORT_CINT                  (0x2ull)
#define NIX_NDC_RX_PORT_MC                    (0x3ull)
#define NIX_NDC_RX_PORT_PKT                   (0x4ull)
#define NIX_NDC_RX_PORT_RQ                    (0x5ull)

#define NIX_RE_OPCODE_RE_NONE                 (0x0ull)
#define NIX_RE_OPCODE_RE_PARTIAL              (0x1ull)
#define NIX_RE_OPCODE_RE_JABBER               (0x2ull)
#define NIX_RE_OPCODE_RE_FCS                  (0x7ull)
#define NIX_RE_OPCODE_RE_FCS_RCV              (0x8ull)
#define NIX_RE_OPCODE_RE_TERMINATE            (0x9ull)
#define NIX_RE_OPCODE_RE_RX_CTL               (0xbull)
#define NIX_RE_OPCODE_RE_SKIP                 (0xcull)
#define NIX_RE_OPCODE_RE_DMAPKT               (0xfull)
#define NIX_RE_OPCODE_UNDERSIZE               (0x10ull)
#define NIX_RE_OPCODE_OVERSIZE                (0x11ull)
#define NIX_RE_OPCODE_OL2_LENMISM             (0x12ull)

#define NIX_REDALG_STD                        (0x0ull)
#define NIX_REDALG_SEND                       (0x1ull)
#define NIX_REDALG_STALL                      (0x2ull)
#define NIX_REDALG_DISCARD                    (0x3ull)

#define NIX_RX_MCOP_RQ                        (0x0ull)
#define NIX_RX_MCOP_RSS                       (0x1ull)

#define NIX_RX_PERRCODE_NPC_RESULT_ERR        (0x2ull)
#define NIX_RX_PERRCODE_MCAST_FAULT           (0x4ull)
#define NIX_RX_PERRCODE_MIRROR_FAULT          (0x5ull)
#define NIX_RX_PERRCODE_MCAST_POISON          (0x6ull)
#define NIX_RX_PERRCODE_MIRROR_POISON         (0x7ull)
#define NIX_RX_PERRCODE_DATA_FAULT            (0x8ull)
#define NIX_RX_PERRCODE_MEMOUT                (0x9ull)
#define NIX_RX_PERRCODE_BUFS_OFLOW            (0xaull)
#define NIX_RX_PERRCODE_OL3_LEN               (0x10ull)
#define NIX_RX_PERRCODE_OL4_LEN               (0x11ull)
#define NIX_RX_PERRCODE_OL4_CHK               (0x12ull)
#define NIX_RX_PERRCODE_OL4_PORT              (0x13ull)
#define NIX_RX_PERRCODE_IL3_LEN               (0x20ull)
#define NIX_RX_PERRCODE_IL4_LEN               (0x21ull)
#define NIX_RX_PERRCODE_IL4_CHK               (0x22ull)
#define NIX_RX_PERRCODE_IL4_PORT              (0x23ull)

#define NIX_SENDCRCALG_CRC32                  (0x0ull)
#define NIX_SENDCRCALG_CRC32C                 (0x1ull)
#define NIX_SENDCRCALG_ONES16                 (0x2ull)

#define NIX_SENDL3TYPE_NONE                   (0x0ull)
#define NIX_SENDL3TYPE_IP4                    (0x2ull)
#define NIX_SENDL3TYPE_IP4_CKSUM              (0x3ull)
#define NIX_SENDL3TYPE_IP6                    (0x4ull)

#define NIX_SENDL4TYPE_NONE                   (0x0ull)
#define NIX_SENDL4TYPE_TCP_CKSUM              (0x1ull)
#define NIX_SENDL4TYPE_SCTP_CKSUM             (0x2ull)
#define NIX_SENDL4TYPE_UDP_CKSUM              (0x3ull)

#define NIX_SENDLDTYPE_LDD                    (0x0ull)
#define NIX_SENDLDTYPE_LDT                    (0x1ull)
#define NIX_SENDLDTYPE_LDWB                   (0x2ull)

#define NIX_SENDMEMALG_SET                    (0x0ull)
#define NIX_SENDMEMALG_SETTSTMP               (0x1ull)
#define NIX_SENDMEMALG_SETRSLT                (0x2ull)
#define NIX_SENDMEMALG_ADD                    (0x8ull)
#define NIX_SENDMEMALG_SUB                    (0x9ull)
#define NIX_SENDMEMALG_ADDLEN                 (0xaull)
#define NIX_SENDMEMALG_SUBLEN                 (0xbull)
#define NIX_SENDMEMALG_ADDMBUF                (0xcull)
#define NIX_SENDMEMALG_SUBMBUF                (0xdull)

#define NIX_SENDMEMDSZ_B64                    (0x0ull)
#define NIX_SENDMEMDSZ_B32                    (0x1ull)
#define NIX_SENDMEMDSZ_B16                    (0x2ull)
#define NIX_SENDMEMDSZ_B8                     (0x3ull)

#define NIX_SEND_STATUS_GOOD                  (0x0ull)
#define NIX_SEND_STATUS_SQ_CTX_FAULT          (0x1ull)
#define NIX_SEND_STATUS_SQ_CTX_POISON         (0x2ull)
#define NIX_SEND_STATUS_SQB_FAULT             (0x3ull)
#define NIX_SEND_STATUS_SQB_POISON            (0x4ull)
#define NIX_SEND_STATUS_SEND_HDR_ERR          (0x5ull)
#define NIX_SEND_STATUS_SEND_EXT_ERR          (0x6ull)
#define NIX_SEND_STATUS_JUMP_FAULT            (0x7ull)
#define NIX_SEND_STATUS_JUMP_POISON           (0x8ull)
#define NIX_SEND_STATUS_SEND_CRC_ERR          (0x10ull)
#define NIX_SEND_STATUS_SEND_IMM_ERR          (0x11ull)
#define NIX_SEND_STATUS_SEND_SG_ERR           (0x12ull)
#define NIX_SEND_STATUS_SEND_MEM_ERR          (0x13ull)
#define NIX_SEND_STATUS_INVALID_SUBDC         (0x14ull)
#define NIX_SEND_STATUS_SUBDC_ORDER_ERR       (0x15ull)
#define NIX_SEND_STATUS_DATA_FAULT            (0x16ull)
#define NIX_SEND_STATUS_DATA_POISON           (0x17ull)
#define NIX_SEND_STATUS_NPC_DROP_ACTION       (0x20ull)
#define NIX_SEND_STATUS_LOCK_VIOL             (0x21ull)
#define NIX_SEND_STATUS_NPC_UCAST_CHAN_ERR    (0x22ull)
#define NIX_SEND_STATUS_NPC_MCAST_CHAN_ERR    (0x23ull)
#define NIX_SEND_STATUS_NPC_MCAST_ABORT       (0x24ull)
#define NIX_SEND_STATUS_NPC_VTAG_PTR_ERR      (0x25ull)
#define NIX_SEND_STATUS_NPC_VTAG_SIZE_ERR     (0x26ull)
#define NIX_SEND_STATUS_SEND_MEM_FAULT        (0x27ull)

#define NIX_SQINT_LMT_ERR                     (0x0ull)
#define NIX_SQINT_MNQ_ERR                     (0x1ull)
#define NIX_SQINT_SEND_ERR                    (0x2ull)
#define NIX_SQINT_SQB_ALLOC_FAIL              (0x3ull)

#define NIX_XQE_TYPE_INVALID                  (0x0ull)
#define NIX_XQE_TYPE_RX                       (0x1ull)
#define NIX_XQE_TYPE_RX_IPSECS                (0x2ull)
#define NIX_XQE_TYPE_RX_IPSECH                (0x3ull)
#define NIX_XQE_TYPE_RX_IPSECD                (0x4ull)
#define NIX_XQE_TYPE_SEND                     (0x8ull)

#define NIX_AQ_COMP_NOTDONE                   (0x0ull)
#define NIX_AQ_COMP_GOOD                      (0x1ull)
#define NIX_AQ_COMP_SWERR                     (0x2ull)
#define NIX_AQ_COMP_CTX_POISON                (0x3ull)
#define NIX_AQ_COMP_CTX_FAULT                 (0x4ull)
#define NIX_AQ_COMP_LOCKERR                   (0x5ull)
#define NIX_AQ_COMP_SQB_ALLOC_FAIL            (0x6ull)

#define NIX_AF_INT_VEC_RVU                    (0x0ull)
#define NIX_AF_INT_VEC_GEN                    (0x1ull)
#define NIX_AF_INT_VEC_AQ_DONE                (0x2ull)
#define NIX_AF_INT_VEC_AF_ERR                 (0x3ull)
#define NIX_AF_INT_VEC_POISON                 (0x4ull)

#define NIX_AQINT_GEN_RX_MCAST_DROP           (0x0ull)
#define NIX_AQINT_GEN_RX_MIRROR_DROP          (0x1ull)
#define NIX_AQINT_GEN_TL1_DRAIN               (0x3ull)
#define NIX_AQINT_GEN_SMQ_FLUSH_DONE          (0x4ull)

#define NIX_AQ_INSTOP_NOP                     (0x0ull)
#define NIX_AQ_INSTOP_INIT                    (0x1ull)
#define NIX_AQ_INSTOP_WRITE                   (0x2ull)
#define NIX_AQ_INSTOP_READ                    (0x3ull)
#define NIX_AQ_INSTOP_LOCK                    (0x4ull)
#define NIX_AQ_INSTOP_UNLOCK                  (0x5ull)

#define NIX_AQ_CTYPE_RQ                       (0x0ull)
#define NIX_AQ_CTYPE_SQ                       (0x1ull)
#define NIX_AQ_CTYPE_CQ                       (0x2ull)
#define NIX_AQ_CTYPE_MCE                      (0x3ull)
#define NIX_AQ_CTYPE_RSS                      (0x4ull)
#define NIX_AQ_CTYPE_DYNO                     (0x5ull)

#define NIX_COLORRESULT_GREEN                 (0x0ull)
#define NIX_COLORRESULT_YELLOW                (0x1ull)
#define NIX_COLORRESULT_RED_SEND              (0x2ull)
#define NIX_COLORRESULT_RED_DROP              (0x3ull)

#define NIX_CHAN_LBKX_CHX(a, b)               \
	(0x000ull | ((uint64_t)(a) << 8) | (uint64_t)(b))
#define NIX_CHAN_R4                           (0x400ull)
#define NIX_CHAN_R5                           (0x500ull)
#define NIX_CHAN_R6                           (0x600ull)
#define NIX_CHAN_SDP_CH_END                   (0x7ffull)
#define NIX_CHAN_SDP_CH_START                 (0x700ull)
#define NIX_CHAN_CGXX_LMACX_CHX(a, b, c)      \
	(0x800ull | ((uint64_t)(a) << 8) | ((uint64_t)(b) << 4) | \
	(uint64_t)(c))

#define NIX_INTF_SDP                          (0x4ull)
#define NIX_INTF_CGX0                         (0x0ull)
#define NIX_INTF_CGX1                         (0x1ull)
#define NIX_INTF_CGX2                         (0x2ull)
#define NIX_INTF_LBK0                         (0x3ull)

#define NIX_CQERRINT_DOOR_ERR                 (0x0ull)
#define NIX_CQERRINT_WR_FULL                  (0x1ull)
#define NIX_CQERRINT_CQE_FAULT                (0x2ull)

#define NIX_LF_INT_VEC_GINT                   (0x80ull)
#define NIX_LF_INT_VEC_ERR_INT                (0x81ull)
#define NIX_LF_INT_VEC_POISON                 (0x82ull)
#define NIX_LF_INT_VEC_QINT_END               (0x3full)
#define NIX_LF_INT_VEC_QINT_START             (0x0ull)
#define NIX_LF_INT_VEC_CINT_END               (0x7full)
#define NIX_LF_INT_VEC_CINT_START             (0x40ull)

/* Enums definitions */

/* Structures definitions */

/* NIX admin queue instruction structure */
struct nix_aq_inst_s {
	uint64_t op         : 4;
	uint64_t ctype      : 4;
	uint64_t lf         : 7;
	uint64_t rsvd_23_15 : 9;
	uint64_t cindex     : 20;
	uint64_t rsvd_62_44 : 19;
	uint64_t doneint    : 1;
	uint64_t res_addr   : 64;    /* W1 */
};

/* NIX admin queue result structure */
struct nix_aq_res_s {
	uint64_t op          : 4;
	uint64_t ctype       : 4;
	uint64_t compcode    : 8;
	uint64_t doneint     : 1;
	uint64_t rsvd_63_17  : 47;
	uint64_t rsvd_127_64 : 64;   /* W1 */
};

/* NIX completion interrupt context hardware structure */
struct nix_cint_hw_s {
	uint64_t ecount       : 32;
	uint64_t qcount       : 16;
	uint64_t intr         : 1;
	uint64_t ena          : 1;
	uint64_t timer_idx    : 8;
	uint64_t rsvd_63_58   : 6;
	uint64_t ecount_wait  : 32;
	uint64_t qcount_wait  : 16;
	uint64_t time_wait    : 8;
	uint64_t rsvd_127_120 : 8;
};

/* NIX completion queue entry header structure */
struct nix_cqe_hdr_s {
	uint64_t tag        : 32;
	uint64_t q          : 20;
	uint64_t rsvd_57_52 : 6;
	uint64_t node       : 2;
	uint64_t cqe_type   : 4;
};

/* NIX completion queue context structure */
struct nix_cq_ctx_s {
	uint64_t base           : 64;/* W0 */
	uint64_t rsvd_67_64     : 4;
	uint64_t bp_ena         : 1;
	uint64_t rsvd_71_69     : 3;
	uint64_t bpid           : 9;
	uint64_t rsvd_83_81     : 3;
	uint64_t qint_idx       : 7;
	uint64_t cq_err         : 1;
	uint64_t cint_idx       : 7;
	uint64_t avg_con        : 9;
	uint64_t wrptr          : 20;
	uint64_t tail           : 20;
	uint64_t head           : 20;
	uint64_t avg_level      : 8;
	uint64_t update_time    : 16;
	uint64_t bp             : 8;
	uint64_t drop           : 8;
	uint64_t drop_ena       : 1;
	uint64_t ena            : 1;
	uint64_t rsvd_211_210   : 2;
	uint64_t substream      : 20;
	uint64_t caching        : 1;
	uint64_t rsvd_235_233   : 3;
	uint64_t qsize          : 4;
	uint64_t cq_err_int     : 8;
	uint64_t cq_err_int_ena : 8;
};

/* NIX instruction header structure */
struct nix_inst_hdr_s {
	uint64_t pf_func    : 16;
	uint64_t sq         : 20;
	uint64_t rsvd_63_36 : 28;
};

/* NIX i/o virtual address structure */
struct nix_iova_s {
	uint64_t addr : 64;          /* W0 */
};

/* NIX IPsec dynamic ordering counter structure */
struct nix_ipsec_dyno_s {
	uint32_t count : 32;         /* W0 */
};

/* NIX memory value structure */
struct nix_mem_result_s {
	uint64_t v         : 1;
	uint64_t color     : 2;
	uint64_t rsvd_63_3 : 61;
};

/* NIX statistics operation write data structure */
struct nix_op_q_wdata_s {
	uint64_t rsvd_31_0  : 32;
	uint64_t q          : 20;
	uint64_t rsvd_63_52 : 12;
};

/* NIX queue interrupt context hardware structure */
struct nix_qint_hw_s {
	uint32_t count      : 22;
	uint32_t rsvd_30_22 : 9;
	uint32_t ena        : 1;
};

/* NIX receive queue context structure */
struct nix_rq_ctx_hw_s {
	uint64_t ena           : 1;
	uint64_t sso_ena       : 1;
	uint64_t ipsech_ena    : 1;
	uint64_t ena_wqwd      : 1;
	uint64_t cq            : 20;
	uint64_t substream     : 20;
	uint64_t wqe_aura      : 20;
	uint64_t spb_aura      : 20;
	uint64_t lpb_aura      : 20;
	uint64_t sso_grp       : 10;
	uint64_t sso_tt        : 2;
	uint64_t pb_caching    : 2;
	uint64_t wqe_caching   : 1;
	uint64_t xqe_drop_ena  : 1;
	uint64_t spb_drop_ena  : 1;
	uint64_t lpb_drop_ena  : 1;
	uint64_t wqe_skip      : 2;
	uint64_t rsvd_127_124  : 4;
	uint64_t rsvd_139_128  : 12;
	uint64_t spb_sizem1    : 6;
	uint64_t rsvd_150_146  : 5;
	uint64_t spb_ena       : 1;
	uint64_t lpb_sizem1    : 12;
	uint64_t first_skip    : 7;
	uint64_t rsvd_171      : 1;
	uint64_t later_skip    : 6;
	uint64_t xqe_imm_size  : 6;
	uint64_t rsvd_189_184  : 6;
	uint64_t xqe_imm_copy  : 1;
	uint64_t xqe_hdr_split : 1;
	uint64_t xqe_drop      : 8;
	uint64_t xqe_pass      : 8;
	uint64_t wqe_pool_drop : 8;
	uint64_t wqe_pool_pass : 8;
	uint64_t spb_aura_drop : 8;
	uint64_t spb_aura_pass : 8;
	uint64_t spb_pool_drop : 8;
	uint64_t spb_pool_pass : 8;
	uint64_t lpb_aura_drop : 8;
	uint64_t lpb_aura_pass : 8;
	uint64_t lpb_pool_drop : 8;
	uint64_t lpb_pool_pass : 8;
	uint64_t rsvd_319_288  : 32;
	uint64_t ltag          : 24;
	uint64_t good_utag     : 8;
	uint64_t bad_utag      : 8;
	uint64_t flow_tagw     : 6;
	uint64_t rsvd_383_366  : 18;
	uint64_t octs          : 48;
	uint64_t rsvd_447_432  : 16;
	uint64_t pkts          : 48;
	uint64_t rsvd_511_496  : 16;
	uint64_t drop_octs     : 48;
	uint64_t rsvd_575_560  : 16;
	uint64_t drop_pkts     : 48;
	uint64_t rsvd_639_624  : 16;
	uint64_t re_pkts       : 48;
	uint64_t rsvd_702_688  : 15;
	uint64_t ena_copy      : 1;
	uint64_t rsvd_739_704  : 36;
	uint64_t rq_int        : 8;
	uint64_t rq_int_ena    : 8;
	uint64_t qint_idx      : 7;
	uint64_t rsvd_767_763  : 5;
	uint64_t rsvd_831_768  : 64;/* W12 */
	uint64_t rsvd_895_832  : 64;/* W13 */
	uint64_t rsvd_959_896  : 64;/* W14 */
	uint64_t rsvd_1023_960 : 64;/* W15 */
};

/* NIX receive queue context structure */
struct nix_rq_ctx_s {
	uint64_t ena           : 1;
	uint64_t sso_ena       : 1;
	uint64_t ipsech_ena    : 1;
	uint64_t ena_wqwd      : 1;
	uint64_t cq            : 20;
	uint64_t substream     : 20;
	uint64_t wqe_aura      : 20;
	uint64_t spb_aura      : 20;
	uint64_t lpb_aura      : 20;
	uint64_t sso_grp       : 10;
	uint64_t sso_tt        : 2;
	uint64_t pb_caching    : 2;
	uint64_t wqe_caching   : 1;
	uint64_t xqe_drop_ena  : 1;
	uint64_t spb_drop_ena  : 1;
	uint64_t lpb_drop_ena  : 1;
	uint64_t rsvd_127_122  : 6;
	uint64_t rsvd_139_128  : 12;
	uint64_t spb_sizem1    : 6;
	uint64_t wqe_skip      : 2;
	uint64_t rsvd_150_148  : 3;
	uint64_t spb_ena       : 1;
	uint64_t lpb_sizem1    : 12;
	uint64_t first_skip    : 7;
	uint64_t rsvd_171      : 1;
	uint64_t later_skip    : 6;
	uint64_t xqe_imm_size  : 6;
	uint64_t rsvd_189_184  : 6;
	uint64_t xqe_imm_copy  : 1;
	uint64_t xqe_hdr_split : 1;
	uint64_t xqe_drop      : 8;
	uint64_t xqe_pass      : 8;
	uint64_t wqe_pool_drop : 8;
	uint64_t wqe_pool_pass : 8;
	uint64_t spb_aura_drop : 8;
	uint64_t spb_aura_pass : 8;
	uint64_t spb_pool_drop : 8;
	uint64_t spb_pool_pass : 8;
	uint64_t lpb_aura_drop : 8;
	uint64_t lpb_aura_pass : 8;
	uint64_t lpb_pool_drop : 8;
	uint64_t lpb_pool_pass : 8;
	uint64_t rsvd_291_288  : 4;
	uint64_t rq_int        : 8;
	uint64_t rq_int_ena    : 8;
	uint64_t qint_idx      : 7;
	uint64_t rsvd_319_315  : 5;
	uint64_t ltag          : 24;
	uint64_t good_utag     : 8;
	uint64_t bad_utag      : 8;
	uint64_t flow_tagw     : 6;
	uint64_t rsvd_383_366  : 18;
	uint64_t octs          : 48;
	uint64_t rsvd_447_432  : 16;
	uint64_t pkts          : 48;
	uint64_t rsvd_511_496  : 16;
	uint64_t drop_octs     : 48;
	uint64_t rsvd_575_560  : 16;
	uint64_t drop_pkts     : 48;
	uint64_t rsvd_639_624  : 16;
	uint64_t re_pkts       : 48;
	uint64_t rsvd_703_688  : 16;
	uint64_t rsvd_767_704  : 64;/* W11 */
	uint64_t rsvd_831_768  : 64;/* W12 */
	uint64_t rsvd_895_832  : 64;/* W13 */
	uint64_t rsvd_959_896  : 64;/* W14 */
	uint64_t rsvd_1023_960 : 64;/* W15 */
};

/* NIX receive side scaling entry structure */
struct nix_rsse_s {
	uint32_t rq         : 20;
	uint32_t rsvd_31_20 : 12;
};

/* NIX receive action structure */
struct nix_rx_action_s {
	uint64_t op           : 4;
	uint64_t pf_func      : 16;
	uint64_t index        : 20;
	uint64_t match_id     : 16;
	uint64_t flow_key_alg : 5;
	uint64_t rsvd_63_61   : 3;
};

/* NIX receive immediate sub descriptor structure */
struct nix_rx_imm_s {
	uint64_t size       : 16;
	uint64_t apad       : 3;
	uint64_t rsvd_59_19 : 41;
	uint64_t subdc      : 4;
};

/* NIX receive multicast/mirror entry structure */
struct nix_rx_mce_s {
	uint64_t op         : 2;
	uint64_t rsvd_2     : 1;
	uint64_t eol        : 1;
	uint64_t index      : 20;
	uint64_t rsvd_31_24 : 8;
	uint64_t pf_func    : 16;
	uint64_t next       : 16;
};

/* NIX receive parse structure */
struct nix_rx_parse_s {
	uint64_t chan         : 12;
	uint64_t desc_sizem1  : 5;
	uint64_t imm_copy     : 1;
	uint64_t express      : 1;
	uint64_t wqwd         : 1;
	uint64_t errlev       : 4;
	uint64_t errcode      : 8;
	uint64_t latype       : 4;
	uint64_t lbtype       : 4;
	uint64_t lctype       : 4;
	uint64_t ldtype       : 4;
	uint64_t letype       : 4;
	uint64_t lftype       : 4;
	uint64_t lgtype       : 4;
	uint64_t lhtype       : 4;
	uint64_t pkt_lenm1    : 16;
	uint64_t l2m          : 1;
	uint64_t l2b          : 1;
	uint64_t l3m          : 1;
	uint64_t l3b          : 1;
	uint64_t vtag0_valid  : 1;
	uint64_t vtag0_gone   : 1;
	uint64_t vtag1_valid  : 1;
	uint64_t vtag1_gone   : 1;
	uint64_t pkind        : 6;
	uint64_t rsvd_95_94   : 2;
	uint64_t vtag0_tci    : 16;
	uint64_t vtag1_tci    : 16;
	uint64_t laflags      : 8;
	uint64_t lbflags      : 8;
	uint64_t lcflags      : 8;
	uint64_t ldflags      : 8;
	uint64_t leflags      : 8;
	uint64_t lfflags      : 8;
	uint64_t lgflags      : 8;
	uint64_t lhflags      : 8;
	uint64_t eoh_ptr      : 8;
	uint64_t wqe_aura     : 20;
	uint64_t pb_aura      : 20;
	uint64_t match_id     : 16;
	uint64_t laptr        : 8;
	uint64_t lbptr        : 8;
	uint64_t lcptr        : 8;
	uint64_t ldptr        : 8;
	uint64_t leptr        : 8;
	uint64_t lfptr        : 8;
	uint64_t lgptr        : 8;
	uint64_t lhptr        : 8;
	uint64_t vtag0_ptr    : 8;
	uint64_t vtag1_ptr    : 8;
	uint64_t flow_key_alg : 5;
	uint64_t rsvd_383_341 : 43;
	uint64_t rsvd_447_384 : 64;  /* W6 */
};

/* NIX receive scatter/gather sub descriptor structure */
struct nix_rx_sg_s {
	uint64_t seg1_size  : 16;
	uint64_t seg2_size  : 16;
	uint64_t seg3_size  : 16;
	uint64_t segs       : 2;
	uint64_t rsvd_59_50 : 10;
	uint64_t subdc      : 4;
};

/* NIX receive vtag action structure */
struct nix_rx_vtag_action_s {
	uint64_t vtag0_relptr : 8;
	uint64_t vtag0_lid    : 3;
	uint64_t rsvd_11      : 1;
	uint64_t vtag0_type   : 3;
	uint64_t vtag0_valid  : 1;
	uint64_t rsvd_31_16   : 16;
	uint64_t vtag1_relptr : 8;
	uint64_t vtag1_lid    : 3;
	uint64_t rsvd_43      : 1;
	uint64_t vtag1_type   : 3;
	uint64_t vtag1_valid  : 1;
	uint64_t rsvd_63_48   : 16;
};

/* NIX send completion structure */
struct nix_send_comp_s {
	uint64_t status     : 8;
	uint64_t sqe_id     : 16;
	uint64_t rsvd_63_24 : 40;
};

/* NIX send CRC sub descriptor structure */
struct nix_send_crc_s {
	uint64_t size        : 16;
	uint64_t start       : 16;
	uint64_t insert      : 16;
	uint64_t rsvd_57_48  : 10;
	uint64_t alg         : 2;
	uint64_t subdc       : 4;
	uint64_t iv          : 32;
	uint64_t rsvd_127_96 : 32;
};

/* NIX send extended header sub descriptor structure */
RTE_STD_C11
union nix_send_ext_w0_u {
	uint64_t u;
	struct {
		uint64_t lso_mps       : 14;
		uint64_t lso           : 1;
		uint64_t tstmp         : 1;
		uint64_t lso_sb        : 8;
		uint64_t lso_format    : 5;
		uint64_t rsvd_31_29    : 3;
		uint64_t shp_chg       : 9;
		uint64_t shp_dis       : 1;
		uint64_t shp_ra        : 2;
		uint64_t markptr       : 8;
		uint64_t markform      : 7;
		uint64_t mark_en       : 1;
		uint64_t subdc         : 4;
	};
};

RTE_STD_C11
union nix_send_ext_w1_u {
	uint64_t u;
	struct {
		uint64_t vlan0_ins_ptr : 8;
		uint64_t vlan0_ins_tci : 16;
		uint64_t vlan1_ins_ptr : 8;
		uint64_t vlan1_ins_tci : 16;
		uint64_t vlan0_ins_ena : 1;
		uint64_t vlan1_ins_ena : 1;
		uint64_t rsvd_127_114  : 14;
	};
};

struct nix_send_ext_s {
	union nix_send_ext_w0_u w0;
	union nix_send_ext_w1_u w1;
};

/* NIX send header sub descriptor structure */
RTE_STD_C11
union nix_send_hdr_w0_u {
	uint64_t u;
	struct {
		uint64_t total   : 18;
		uint64_t rsvd_18 : 1;
		uint64_t df      : 1;
		uint64_t aura    : 20;
		uint64_t sizem1  : 3;
		uint64_t pnc     : 1;
		uint64_t sq      : 20;
	};
};

RTE_STD_C11
union nix_send_hdr_w1_u {
	uint64_t u;
	struct {
		uint64_t ol3ptr  : 8;
		uint64_t ol4ptr  : 8;
		uint64_t il3ptr  : 8;
		uint64_t il4ptr  : 8;
		uint64_t ol3type : 4;
		uint64_t ol4type : 4;
		uint64_t il3type : 4;
		uint64_t il4type : 4;
		uint64_t sqe_id  : 16;
	};
};

struct nix_send_hdr_s {
	union nix_send_hdr_w0_u w0;
	union nix_send_hdr_w1_u w1;
};

/* NIX send immediate sub descriptor structure */
struct nix_send_imm_s {
	uint64_t size       : 16;
	uint64_t apad       : 3;
	uint64_t rsvd_59_19 : 41;
	uint64_t subdc      : 4;
};

/* NIX send jump sub descriptor structure */
struct nix_send_jump_s {
	uint64_t sizem1     : 7;
	uint64_t rsvd_13_7  : 7;
	uint64_t ld_type    : 2;
	uint64_t aura       : 20;
	uint64_t rsvd_58_36 : 23;
	uint64_t f          : 1;
	uint64_t subdc      : 4;
	uint64_t addr       : 64;    /* W1 */
};

/* NIX send memory sub descriptor structure */
struct nix_send_mem_s {
	uint64_t offset     : 16;
	uint64_t rsvd_52_16 : 37;
	uint64_t wmem       : 1;
	uint64_t dsz        : 2;
	uint64_t alg        : 4;
	uint64_t subdc      : 4;
	uint64_t addr       : 64;    /* W1 */
};

/* NIX send scatter/gather sub descriptor structure */
RTE_STD_C11
union nix_send_sg_s {
	uint64_t u;
	struct {
		uint64_t seg1_size  : 16;
		uint64_t seg2_size  : 16;
		uint64_t seg3_size  : 16;
		uint64_t segs       : 2;
		uint64_t rsvd_54_50 : 5;
		uint64_t i1         : 1;
		uint64_t i2         : 1;
		uint64_t i3         : 1;
		uint64_t ld_type    : 2;
		uint64_t subdc      : 4;
	};
};

/* NIX send work sub descriptor structure */
struct nix_send_work_s {
	uint64_t tag        : 32;
	uint64_t tt         : 2;
	uint64_t grp        : 10;
	uint64_t rsvd_59_44 : 16;
	uint64_t subdc      : 4;
	uint64_t addr       : 64;    /* W1 */
};

/* NIX sq context hardware structure */
struct nix_sq_ctx_hw_s {
	uint64_t ena                   : 1;
	uint64_t substream             : 20;
	uint64_t max_sqe_size          : 2;
	uint64_t sqe_way_mask          : 16;
	uint64_t sqb_aura              : 20;
	uint64_t gbl_rsvd1             : 5;
	uint64_t cq_id                 : 20;
	uint64_t cq_ena                : 1;
	uint64_t qint_idx              : 6;
	uint64_t gbl_rsvd2             : 1;
	uint64_t sq_int                : 8;
	uint64_t sq_int_ena            : 8;
	uint64_t xoff                  : 1;
	uint64_t sqe_stype             : 2;
	uint64_t gbl_rsvd              : 17;
	uint64_t head_sqb              : 64;/* W2 */
	uint64_t head_offset           : 6;
	uint64_t sqb_dequeue_count     : 16;
	uint64_t default_chan          : 12;
	uint64_t sdp_mcast             : 1;
	uint64_t sso_ena               : 1;
	uint64_t dse_rsvd1             : 28;
	uint64_t sqb_enqueue_count     : 16;
	uint64_t tail_offset           : 6;
	uint64_t lmt_dis               : 1;
	uint64_t smq_rr_quantum        : 24;
	uint64_t dnq_rsvd1             : 17;
	uint64_t tail_sqb              : 64;/* W5 */
	uint64_t next_sqb              : 64;/* W6 */
	uint64_t mnq_dis               : 1;
	uint64_t smq                   : 9;
	uint64_t smq_pend              : 1;
	uint64_t smq_next_sq           : 20;
	uint64_t smq_next_sq_vld       : 1;
	uint64_t scm1_rsvd2            : 32;
	uint64_t smenq_sqb             : 64;/* W8 */
	uint64_t smenq_offset          : 6;
	uint64_t cq_limit              : 8;
	uint64_t smq_rr_count          : 25;
	uint64_t scm_lso_rem           : 18;
	uint64_t scm_dq_rsvd0          : 7;
	uint64_t smq_lso_segnum        : 8;
	uint64_t vfi_lso_total         : 18;
	uint64_t vfi_lso_sizem1        : 3;
	uint64_t vfi_lso_sb            : 8;
	uint64_t vfi_lso_mps           : 14;
	uint64_t vfi_lso_vlan0_ins_ena : 1;
	uint64_t vfi_lso_vlan1_ins_ena : 1;
	uint64_t vfi_lso_vld           : 1;
	uint64_t smenq_next_sqb_vld    : 1;
	uint64_t scm_dq_rsvd1          : 9;
	uint64_t smenq_next_sqb        : 64;/* W11 */
	uint64_t seb_rsvd1             : 64;/* W12 */
	uint64_t drop_pkts             : 48;
	uint64_t drop_octs_lsw         : 16;
	uint64_t drop_octs_msw         : 32;
	uint64_t pkts_lsw              : 32;
	uint64_t pkts_msw              : 16;
	uint64_t octs                  : 48;
};

/* NIX send queue context structure */
struct nix_sq_ctx_s {
	uint64_t ena                   : 1;
	uint64_t qint_idx              : 6;
	uint64_t substream             : 20;
	uint64_t sdp_mcast             : 1;
	uint64_t cq                    : 20;
	uint64_t sqe_way_mask          : 16;
	uint64_t smq                   : 9;
	uint64_t cq_ena                : 1;
	uint64_t xoff                  : 1;
	uint64_t sso_ena               : 1;
	uint64_t smq_rr_quantum        : 24;
	uint64_t default_chan          : 12;
	uint64_t sqb_count             : 16;
	uint64_t smq_rr_count          : 25;
	uint64_t sqb_aura              : 20;
	uint64_t sq_int                : 8;
	uint64_t sq_int_ena            : 8;
	uint64_t sqe_stype             : 2;
	uint64_t rsvd_191              : 1;
	uint64_t max_sqe_size          : 2;
	uint64_t cq_limit              : 8;
	uint64_t lmt_dis               : 1;
	uint64_t mnq_dis               : 1;
	uint64_t smq_next_sq           : 20;
	uint64_t smq_lso_segnum        : 8;
	uint64_t tail_offset           : 6;
	uint64_t smenq_offset          : 6;
	uint64_t head_offset           : 6;
	uint64_t smenq_next_sqb_vld    : 1;
	uint64_t smq_pend              : 1;
	uint64_t smq_next_sq_vld       : 1;
	uint64_t rsvd_255_253          : 3;
	uint64_t next_sqb              : 64;/* W4 */
	uint64_t tail_sqb              : 64;/* W5 */
	uint64_t smenq_sqb             : 64;/* W6 */
	uint64_t smenq_next_sqb        : 64;/* W7 */
	uint64_t head_sqb              : 64;/* W8 */
	uint64_t rsvd_583_576          : 8;
	uint64_t vfi_lso_total         : 18;
	uint64_t vfi_lso_sizem1        : 3;
	uint64_t vfi_lso_sb            : 8;
	uint64_t vfi_lso_mps           : 14;
	uint64_t vfi_lso_vlan0_ins_ena : 1;
	uint64_t vfi_lso_vlan1_ins_ena : 1;
	uint64_t vfi_lso_vld           : 1;
	uint64_t rsvd_639_630          : 10;
	uint64_t scm_lso_rem           : 18;
	uint64_t rsvd_703_658          : 46;
	uint64_t octs                  : 48;
	uint64_t rsvd_767_752          : 16;
	uint64_t pkts                  : 48;
	uint64_t rsvd_831_816          : 16;
	uint64_t rsvd_895_832          : 64;/* W13 */
	uint64_t drop_octs             : 48;
	uint64_t rsvd_959_944          : 16;
	uint64_t drop_pkts             : 48;
	uint64_t rsvd_1023_1008        : 16;
};

/* NIX transmit action structure */
struct nix_tx_action_s {
	uint64_t op         : 4;
	uint64_t rsvd_11_4  : 8;
	uint64_t index      : 20;
	uint64_t match_id   : 16;
	uint64_t rsvd_63_48 : 16;
};

/* NIX transmit vtag action structure */
struct nix_tx_vtag_action_s {
	uint64_t vtag0_relptr : 8;
	uint64_t vtag0_lid    : 3;
	uint64_t rsvd_11      : 1;
	uint64_t vtag0_op     : 2;
	uint64_t rsvd_15_14   : 2;
	uint64_t vtag0_def    : 10;
	uint64_t rsvd_31_26   : 6;
	uint64_t vtag1_relptr : 8;
	uint64_t vtag1_lid    : 3;
	uint64_t rsvd_43      : 1;
	uint64_t vtag1_op     : 2;
	uint64_t rsvd_47_46   : 2;
	uint64_t vtag1_def    : 10;
	uint64_t rsvd_63_58   : 6;
};

/* NIX work queue entry header structure */
struct nix_wqe_hdr_s {
	uint64_t tag      : 32;
	uint64_t tt       : 2;
	uint64_t grp      : 10;
	uint64_t node     : 2;
	uint64_t q        : 14;
	uint64_t wqe_type : 4;
};

/* NIX Rx flow key algorithm field structure */
struct nix_rx_flowkey_alg {
	uint64_t key_offset      :6;
	uint64_t ln_mask     :1;
	uint64_t fn_mask     :1;
	uint64_t hdr_offset      :8;
	uint64_t bytesm1     :5;
	uint64_t lid         :3;
	uint64_t reserved_24_24  :1;
	uint64_t ena         :1;
	uint64_t sel_chan        :1;
	uint64_t ltype_mask      :4;
	uint64_t ltype_match     :4;
	uint64_t reserved_35_63  :29;
};

/* NIX LSO format field structure */
struct nix_lso_format {
	uint64_t offset      : 8;
	uint64_t layer       : 2;
	uint64_t rsvd_10_11  : 2;
	uint64_t sizem1      : 2;
	uint64_t rsvd_14_15  : 2;
	uint64_t alg         : 3;
	uint64_t rsvd_19_63  : 45;
};

#define NIX_LSO_FIELD_MAX	(8)
#define NIX_LSO_FIELD_ALG_MASK	GENMASK(18, 16)
#define NIX_LSO_FIELD_SZ_MASK	GENMASK(13, 12)
#define NIX_LSO_FIELD_LY_MASK	GENMASK(9, 8)
#define NIX_LSO_FIELD_OFF_MASK	GENMASK(7, 0)

#define NIX_LSO_FIELD_MASK			\
		(NIX_LSO_FIELD_OFF_MASK |	\
		 NIX_LSO_FIELD_LY_MASK |	\
		 NIX_LSO_FIELD_SZ_MASK |	\
		 NIX_LSO_FIELD_ALG_MASK)

#endif /* __OTX2_NIX_HW_H__ */
