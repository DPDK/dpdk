/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 HiSilicon Limited.
 */

#ifndef HNS3_REGS_H
#define HNS3_REGS_H

#include <ethdev_driver.h>
#include <rte_dev_info.h>

/* bar registers for cmdq */
#define HNS3_CMDQ_TX_ADDR_L_REG		0x27000
#define HNS3_CMDQ_TX_ADDR_H_REG		0x27004
#define HNS3_CMDQ_TX_DEPTH_REG		0x27008
#define HNS3_CMDQ_TX_TAIL_REG		0x27010
#define HNS3_CMDQ_TX_HEAD_REG		0x27014
#define HNS3_CMDQ_RX_ADDR_L_REG		0x27018
#define HNS3_CMDQ_RX_ADDR_H_REG		0x2701c
#define HNS3_CMDQ_RX_DEPTH_REG		0x27020
#define HNS3_CMDQ_RX_TAIL_REG		0x27024
#define HNS3_CMDQ_RX_HEAD_REG		0x27028
#define HNS3_CMDQ_INTR_STS_REG		0x27104
#define HNS3_CMDQ_INTR_EN_REG		0x27108
#define HNS3_CMDQ_INTR_GEN_REG		0x2710C

/* Vector0 interrupt CMDQ event source register(RW) */
#define HNS3_VECTOR0_CMDQ_SRC_REG	0x27100
/* Vector0 interrupt CMDQ event status register(RO) */
#define HNS3_VECTOR0_CMDQ_STAT_REG	0x27104

#define HNS3_VECTOR0_OTHER_INT_STS_REG	0x20800

#define HNS3_RAS_PF_OTHER_INT_STS_REG	0x20B00
#define HNS3_RAS_REG_NFE_MASK		0xFF00

#define HNS3_MISC_VECTOR_REG_BASE	0x20400
#define HNS3_VECTOR0_OTER_EN_REG	0x20600
#define HNS3_MISC_RESET_STS_REG		0x20700
#define HNS3_GLOBAL_RESET_REG		0x20A00
#define HNS3_FUN_RST_ING		0x20C00
#define HNS3_GRO_EN_REG			0x28000

#define HNS3_RPU_DROP_CNT_REG		0x28004
#define HNS3_RXD_ADV_LAYOUT_EN_REG	0x28008

/* Vector0 register bits for reset */
#define HNS3_VECTOR0_FUNCRESET_INT_B	0
#define HNS3_VECTOR0_GLOBALRESET_INT_B	5
#define HNS3_VECTOR0_CORERESET_INT_B	6
#define HNS3_VECTOR0_IMPRESET_INT_B	7

/* CMDQ register bits for RX event(=MBX event) */
#define HNS3_VECTOR0_RX_CMDQ_INT_B	1
#define HNS3_VECTOR0_REG_MSIX_MASK	0x1FF00
/* RST register bits for RESET event */
#define HNS3_VECTOR0_RST_INT_B	2

#define HNS3_VF_RST_ING			0x07008
#define HNS3_VF_RST_ING_BIT		BIT(16)

/* bar registers for rcb */
#define HNS3_RING_RX_BASEADDR_L_REG		0x00000
#define HNS3_RING_RX_BASEADDR_H_REG		0x00004
#define HNS3_RING_RX_BD_NUM_REG			0x00008
#define HNS3_RING_RX_BD_LEN_REG			0x0000C
#define HNS3_RING_RX_MERGE_EN_REG		0x00014
#define HNS3_RING_RX_TAIL_REG			0x00018
#define HNS3_RING_RX_HEAD_REG			0x0001C
#define HNS3_RING_RX_FBDNUM_REG			0x00020
#define HNS3_RING_RX_OFFSET_REG			0x00024
#define HNS3_RING_RX_FBD_OFFSET_REG		0x00028
#define HNS3_RING_RX_PKTNUM_RECORD_REG		0x0002C
#define HNS3_RING_RX_STASH_REG			0x00030
#define HNS3_RING_RX_BD_ERR_REG			0x00034

#define HNS3_RING_TX_BASEADDR_L_REG		0x00040
#define HNS3_RING_TX_BASEADDR_H_REG		0x00044
#define HNS3_RING_TX_BD_NUM_REG			0x00048
#define HNS3_RING_TX_PRIORITY_REG		0x0004C
#define HNS3_RING_TX_TC_REG			0x00050
#define HNS3_RING_TX_MERGE_EN_REG		0x00054
#define HNS3_RING_TX_TAIL_REG			0x00058
#define HNS3_RING_TX_HEAD_REG			0x0005C
#define HNS3_RING_TX_FBDNUM_REG			0x00060
#define HNS3_RING_TX_OFFSET_REG			0x00064
#define HNS3_RING_TX_EBD_NUM_REG		0x00068
#define HNS3_RING_TX_PKTNUM_RECORD_REG		0x0006C
#define HNS3_RING_TX_EBD_OFFSET_REG		0x00070
#define HNS3_RING_TX_BD_ERR_REG			0x00074

#define HNS3_RING_EN_REG			0x00090
#define HNS3_RING_RX_EN_REG			0x00098
#define HNS3_RING_TX_EN_REG			0x000d4

#define HNS3_RING_EN_B				0

#define HNS3_TQP_REG_OFFSET			0x80000
#define HNS3_TQP_REG_SIZE			0x200

#define HNS3_TQP_EXT_REG_OFFSET			0x100
#define HNS3_MIN_EXTEND_QUEUE_ID		1024

/* bar registers for tqp interrupt */
#define HNS3_TQP_INTR_REG_BASE			0x20000
#define HNS3_TQP_INTR_EXT_REG_BASE		0x30000
#define HNS3_TQP_INTR_CTRL_REG			0
#define HNS3_TQP_INTR_GL0_REG			0x100
#define HNS3_TQP_INTR_GL1_REG			0x200
#define HNS3_TQP_INTR_GL2_REG			0x300
#define HNS3_TQP_INTR_RL_REG			0x900
#define HNS3_TQP_INTR_TX_QL_REG			0xe00
#define HNS3_TQP_INTR_RX_QL_REG			0xf00
#define HNS3_TQP_INTR_RL_EN_B			6

#define HNS3_MIN_EXT_TQP_INTR_ID		64
#define HNS3_TQP_INTR_LOW_ORDER_OFFSET		0x4
#define HNS3_TQP_INTR_HIGH_ORDER_OFFSET		0x1000

#define HNS3_TQP_INTR_GL_MAX			0x1FE0
#define HNS3_TQP_INTR_GL_DEFAULT		20
#define HNS3_TQP_INTR_GL_UNIT_1US		BIT(31)
#define HNS3_TQP_INTR_RL_MAX			0xEC
#define HNS3_TQP_INTR_RL_ENABLE_MASK		0x40
#define HNS3_TQP_INTR_RL_DEFAULT		0
#define HNS3_TQP_INTR_QL_DEFAULT		0

/* gl_usec convert to hardware count, as writing each 1 represents 2us */
#define HNS3_GL_USEC_TO_REG(gl_usec)		((gl_usec) >> 1)
/* rl_usec convert to hardware count, as writing each 1 represents 4us */
#define HNS3_RL_USEC_TO_REG(rl_usec)		((rl_usec) >> 2)

int hns3_get_regs(struct rte_eth_dev *eth_dev, struct rte_dev_reg_info *regs);
#endif /* HNS3_REGS_H */
