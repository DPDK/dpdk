/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2026 Marvell.
 */
#ifndef _CN20K_EP_VF_H_
#define _CN20K_EP_VF_H_

#include <rte_io.h>
#include <rte_bitops.h>

#include "otx_ep_common.h"

#define CN20K_MAX_RINGS_PER_VF     (8)

#define CN20K_EP_R_IN_CONTROL_START        0x40000
#define CN20K_EP_R_IN_ENABLE_START         0x40008
#define CN20K_EP_R_IN_INSTR_BADDR_START    0x40010
#define CN20K_EP_R_IN_INSTR_RSIZE_START    0x40018
#define CN20K_EP_R_IN_INSTR_DBELL_START    0x40020
#define CN20K_EP_R_IN_CNTS_START           0x40030
#define CN20K_EP_R_IN_INT_LEVELS_START     0x40040
#define CN20K_EP_R_IN_CNTS_ISM_START       0x40050
#define CN20K_EP_R_IN_PKT_CNT_START        0x40460
#define CN20K_EP_R_IN_BYTE_CNT_START       0x40470
#define CN20K_EP_R_ERR_TYPE_START          0x10400

#define CN20K_EP_RING_OFFSET              (0x1ULL << 12)

#define CN20K_EP_R_ERR_TYPE(ring)                 \
	(CN20K_EP_R_ERR_TYPE_START + ((ring) * CN20K_EP_RING_OFFSET))

#define CN20K_EP_R_IN_CONTROL(ring)          \
	(CN20K_EP_R_IN_CONTROL_START + ((ring) * CN20K_EP_RING_OFFSET))

#define CN20K_EP_R_IN_ENABLE(ring)          \
	(CN20K_EP_R_IN_ENABLE_START + ((ring) * CN20K_EP_RING_OFFSET))

#define CN20K_EP_R_IN_INSTR_BADDR(ring)          \
	(CN20K_EP_R_IN_INSTR_BADDR_START + ((ring) * CN20K_EP_RING_OFFSET))

#define CN20K_EP_R_IN_INSTR_RSIZE(ring)          \
	(CN20K_EP_R_IN_INSTR_RSIZE_START + ((ring) * CN20K_EP_RING_OFFSET))

#define CN20K_EP_R_IN_INSTR_DBELL(ring)          \
	(CN20K_EP_R_IN_INSTR_DBELL_START + ((ring) * CN20K_EP_RING_OFFSET))

#define CN20K_EP_R_IN_CNTS(ring)          \
	(CN20K_EP_R_IN_CNTS_START + ((ring) * CN20K_EP_RING_OFFSET))

#define CN20K_EP_R_IN_INT_LEVELS(ring)          \
	(CN20K_EP_R_IN_INT_LEVELS_START + ((ring) * CN20K_EP_RING_OFFSET))

#define CN20K_EP_R_IN_CNTS_ISM(ring)          \
	(CN20K_EP_R_IN_CNTS_ISM_START + ((ring) * CN20K_EP_RING_OFFSET))

#define    CN20K_EP_R_OUT_CNTS_START            0x40130
#define    CN20K_EP_R_OUT_INT_LEVELS_START      0x40140
#define    CN20K_EP_R_OUT_CNTS_ISM_START        0x40150
#define    CN20K_EP_R_OUT_SLIST_BADDR_START     0x40110
#define    CN20K_EP_R_OUT_SLIST_RSIZE_START     0x40118
#define    CN20K_EP_R_OUT_SLIST_DBELL_START     0x40120
#define    CN20K_EP_R_OUT_CONTROL_START         0x40100
#define    CN20K_EP_R_OUT_WMARK_START           0x40128
#define    CN20K_EP_R_OUT_ENABLE_START          0x40108
#define    CN20K_EP_R_OUT_PKT_CNT_START         0x40560
#define    CN20K_EP_R_OUT_BYTE_CNT_START        0x40570

#define CN20K_EP_R_OUT_CONTROL(ring)          \
	(CN20K_EP_R_OUT_CONTROL_START + ((ring) * CN20K_EP_RING_OFFSET))

#define CN20K_EP_R_OUT_ENABLE(ring)          \
	(CN20K_EP_R_OUT_ENABLE_START + ((ring) * CN20K_EP_RING_OFFSET))

#define CN20K_EP_R_OUT_SLIST_BADDR(ring)          \
	(CN20K_EP_R_OUT_SLIST_BADDR_START + ((ring) * CN20K_EP_RING_OFFSET))

#define CN20K_EP_R_OUT_SLIST_RSIZE(ring)          \
	(CN20K_EP_R_OUT_SLIST_RSIZE_START + ((ring) * CN20K_EP_RING_OFFSET))

#define CN20K_EP_R_OUT_SLIST_DBELL(ring)          \
	(CN20K_EP_R_OUT_SLIST_DBELL_START + ((ring) * CN20K_EP_RING_OFFSET))

#define CN20K_EP_R_OUT_WMARK(ring)          \
	(CN20K_EP_R_OUT_WMARK_START + ((ring) * CN20K_EP_RING_OFFSET))

#define CN20K_EP_R_OUT_CNTS(ring)          \
	(CN20K_EP_R_OUT_CNTS_START + ((ring) * CN20K_EP_RING_OFFSET))

#define CN20K_EP_R_OUT_INT_LEVELS(ring)          \
	(CN20K_EP_R_OUT_INT_LEVELS_START + ((ring) * CN20K_EP_RING_OFFSET))

#define CN20K_EP_R_OUT_CNTS_ISM(ring)          \
	(CN20K_EP_R_OUT_CNTS_ISM_START + ((ring) * CN20K_EP_RING_OFFSET))

#define CN20K_EP_R_OUT_PKT_CNT(ring)          \
	(CN20K_EP_R_OUT_PKT_CNT_START + ((ring) * CN20K_EP_RING_OFFSET))

#define CN20K_EP_R_OUT_BYTE_CNT(ring)          \
	(CN20K_EP_R_OUT_BYTE_CNT_START + ((ring) * CN20K_EP_RING_OFFSET))

#define PCI_DEVID_CN20KA_EP_NET_VF		0xC203
#define PCI_DEVID_CNF20KA_EP_NET_VF		0xCA03

#ifndef BIT_ULL
#define BIT_ULL(n) RTE_BIT64(n)
#endif

int
cn20k_ep_vf_setup_device(struct otx_ep_device *sdpvf);

/* ##################### CN20K Mailbox Registers ########################## */
/* CN20K uses a completely different mailbox architecture compared to CN9X/CN10X
 * - Uses command/data/control register interface
 * - Shared DMA memory buffer (allocated by PF in system RAM)
 * - VF accesses via indirect CMD registers with offset/data/control
 * - PF accesses directly via pointers to DMA memory
 */

/* VF Write Command Registers - for VF_PF communication */
#define CN20K_SDP_RMT_VFX_MBOX_WR_CMD_CTL       0x24040
#define CN20K_SDP_RMT_VFX_MBOX_WR_CMD_OFFSET    0x24048
#define CN20K_SDP_RMT_VFX_MBOX_WR_CMD_DATA      0x24050

/* VF Read Command Registers - for PF_VF communication */
#define CN20K_SDP_RMT_VFX_MBOX_RD_CMD_CTL       0x24060
#define CN20K_SDP_RMT_VFX_MBOX_RD_CMD_OFFSET    0x24068
#define CN20K_SDP_RMT_VFX_MBOX_RD_CMD_DATA      0x24070

/* VF Mailbox Interrupt Registers */
#define CN20K_SDP_RMT_VFX_MBOX_RINT             0x24020
#define CN20K_SDP_RMT_VFX_MBOX_RINT_W1S         0x24028
#define CN20K_SDP_RMT_VFX_MBOX_RINT_ENA_W1C     0x24030
#define CN20K_SDP_RMT_VFX_MBOX_RINT_ENA_W1S     0x24038

/* VF Send Interrupt to PF */
#define CN20K_SDP_RMT_VFX_MBOX_SEND_INT         0x24000

/* Direct Mailbox Data Access (8K registers * 8 bytes = 64KB) */
#define CN20K_SDP_RMT_VFX_MBOX_DATA_START       0x30000
#define CN20K_SDP_RMT_VFX_MBOX_DATA(offset)     \
	(CN20K_SDP_RMT_VFX_MBOX_DATA_START + ((offset) * 8))

/* Command Control Bit Definitions */
#define CN20K_MBOX_WR_CMD_DONE      BIT_ULL(0)
#define CN20K_MBOX_WR_CMD_ERR       BIT_ULL(1)
#define CN20K_MBOX_WR_CMD_OUT       BIT_ULL(2)
#define CN20K_MBOX_WR_COMMIT        BIT_ULL(3)
#define CN20K_MBOX_WR_DROP          BIT_ULL(4)

#define CN20K_MBOX_RD_CMD_DONE      BIT_ULL(0)
#define CN20K_MBOX_RD_CMD_ERR       BIT_ULL(1)
#define CN20K_MBOX_RD_CMD_OUT       BIT_ULL(2)

/* Mailbox Interrupt Bits */
#define CN20K_MBOX_INTR             BIT_ULL(0)

/* Mailbox Memory Layout (64KB per VF, same as PFAF) */
#define CN20K_VF_MBOX_SIZE          (64 * 1024)
#define CN20K_MBOX_DOWN_RX_START    0
#define CN20K_MBOX_DOWN_RX_SIZE     (46 * 1024)
#define CN20K_MBOX_DOWN_TX_START    (CN20K_MBOX_DOWN_RX_START + CN20K_MBOX_DOWN_RX_SIZE)
#define CN20K_MBOX_DOWN_TX_SIZE     (16 * 1024)
#define CN20K_MBOX_UP_RX_START      (CN20K_MBOX_DOWN_TX_START + CN20K_MBOX_DOWN_TX_SIZE)
#define CN20K_MBOX_UP_RX_SIZE       (1 * 1024)
#define CN20K_MBOX_UP_TX_START      (CN20K_MBOX_UP_RX_START + CN20K_MBOX_UP_RX_SIZE)
#define CN20K_MBOX_UP_TX_SIZE       (1 * 1024)

/* Non-IOQ Interrupt Configuration */
#define CN20K_VF_NUM_NON_IOQ_INTR   16    /* Vectors 0-15 for non-IOQ (control path) */
#define CN20K_VF_IOQ_INTR_BASE      16    /* IOQ interrupts start at vector 16 (data path) */

/* Non-IOQ Interrupt Vector Assignments */
#define CN20K_VF_NON_IOQ_INTR_MBOX  0     /* Vector 0: Mailbox (control path) */
/* Vectors 1-15: Reserved for other non-IOQ interrupts (errors, etc.) */

#endif /*_CN20K_EP_VF_H_ */
