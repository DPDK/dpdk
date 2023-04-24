/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Marvell.
 */
#ifndef _CNXK_EP_VF_H_
#define _CNXK_EP_VF_H_

#include <rte_io.h>

#define CNXK_CONFIG_XPANSION_BAR             0x38
#define CNXK_CONFIG_PCIE_CAP                 0x70
#define CNXK_CONFIG_PCIE_DEVCAP              0x74
#define CNXK_CONFIG_PCIE_DEVCTL              0x78
#define CNXK_CONFIG_PCIE_LINKCAP             0x7C
#define CNXK_CONFIG_PCIE_LINKCTL             0x80
#define CNXK_CONFIG_PCIE_SLOTCAP             0x84
#define CNXK_CONFIG_PCIE_SLOTCTL             0x88
#define CNXK_CONFIG_PCIE_FLTMSK              0x720

#define CNXK_EP_RING_OFFSET                    (0x1ULL << 17)

#define CNXK_EP_R_IN_CONTROL_START          0x10000
#define CNXK_EP_R_IN_ENABLE_START           0x10010
#define CNXK_EP_R_IN_INSTR_BADDR_START      0x10020
#define CNXK_EP_R_IN_INSTR_RSIZE_START      0x10030
#define CNXK_EP_R_IN_INSTR_DBELL_START      0x10040
#define CNXK_EP_R_IN_CNTS_START             0x10050
#define CNXK_EP_R_IN_INT_LEVELS_START       0x10060
#define CNXK_EP_R_IN_PKT_CNT_START          0x10080
#define CNXK_EP_R_IN_BYTE_CNT_START         0x10090
#define CNXK_EP_R_IN_CNTS_ISM_START         0x10520

#define CNXK_EP_R_IN_CONTROL(ring)             \
	(CNXK_EP_R_IN_CONTROL_START + ((ring) * CNXK_EP_RING_OFFSET))

#define CNXK_EP_R_IN_ENABLE(ring)              \
	(CNXK_EP_R_IN_ENABLE_START + ((ring) * CNXK_EP_RING_OFFSET))

#define CNXK_EP_R_IN_INSTR_BADDR(ring)          \
	(CNXK_EP_R_IN_INSTR_BADDR_START + ((ring) * CNXK_EP_RING_OFFSET))

#define CNXK_EP_R_IN_INSTR_RSIZE(ring)          \
	(CNXK_EP_R_IN_INSTR_RSIZE_START + ((ring) * CNXK_EP_RING_OFFSET))

#define CNXK_EP_R_IN_INSTR_DBELL(ring)          \
	(CNXK_EP_R_IN_INSTR_DBELL_START + ((ring) * CNXK_EP_RING_OFFSET))

#define CNXK_EP_R_IN_CNTS(ring)                \
	(CNXK_EP_R_IN_CNTS_START + ((ring) * CNXK_EP_RING_OFFSET))

#define CNXK_EP_R_IN_INT_LEVELS(ring)          \
	(CNXK_EP_R_IN_INT_LEVELS_START + ((ring) * CNXK_EP_RING_OFFSET))

#define CNXK_EP_R_IN_PKT_CNT(ring)             \
	(CNXK_EP_R_IN_PKT_CNT_START + ((ring) * CNXK_EP_RING_OFFSET))

#define CNXK_EP_R_IN_BYTE_CNT(ring)            \
	(CNXK_EP_R_IN_BYTE_CNT_START +  ((ring) * CNXK_EP_RING_OFFSET))

#define CNXK_EP_R_IN_CNTS_ISM(ring)            \
	(CNXK_EP_R_IN_CNTS_ISM_START + ((ring) * CNXK_EP_RING_OFFSET))

/** Rings per Virtual Function **/
#define CNXK_EP_R_IN_CTL_RPVF_MASK	(0xF)
#define	CNXK_EP_R_IN_CTL_RPVF_POS	(48)

/* Number of instructions to be read in one MAC read request.
 * setting to Max value(4)
 */
#define CNXK_EP_R_IN_CTL_IDLE		(0x1ULL << 28)
#define CNXK_EP_R_IN_CTL_RDSIZE		(0x3ULL << 25)
#define CNXK_EP_R_IN_CTL_IS_64B		(0x1ULL << 24)
#define CNXK_EP_R_IN_CTL_D_NSR		(0x1ULL << 8)
#define CNXK_EP_R_IN_CTL_D_ROR		(0x1ULL << 5)
#define CNXK_EP_R_IN_CTL_NSR		(0x1ULL << 3)
#define CNXK_EP_R_IN_CTL_ROR		(0x1ULL << 0)
#define CNXK_EP_R_IN_CTL_ESR             (0x1ull << 1)

#define CNXK_EP_R_IN_CTL_MASK                    \
	(CNXK_EP_R_IN_CTL_RDSIZE                 \
	| CNXK_EP_R_IN_CTL_IS_64B)

#define CNXK_EP_R_OUT_CNTS_START           0x10100
#define CNXK_EP_R_OUT_INT_LEVELS_START     0x10110
#define CNXK_EP_R_OUT_SLIST_BADDR_START    0x10120
#define CNXK_EP_R_OUT_SLIST_RSIZE_START    0x10130
#define CNXK_EP_R_OUT_SLIST_DBELL_START    0x10140
#define CNXK_EP_R_OUT_CONTROL_START        0x10150
/* WMARK need to be set; New in CN10K */
#define CNXK_EP_R_OUT_WMARK_START          0x10160
#define CNXK_EP_R_OUT_ENABLE_START         0x10170
#define CNXK_EP_R_OUT_PKT_CNT_START        0x10180
#define CNXK_EP_R_OUT_BYTE_CNT_START       0x10190
#define CNXK_EP_R_OUT_CNTS_ISM_START       0x10510

#define CNXK_EP_R_MBOX_PF_VF_DATA_START    0x10210
#define CNXK_EP_R_MBOX_VF_PF_DATA_START    0x10230
#define CNXK_EP_R_MBOX_PF_VF_INT_START     0x10220

#define CNXK_EP_R_OUT_CNTS(ring)                \
	(CNXK_EP_R_OUT_CNTS_START + ((ring) * CNXK_EP_RING_OFFSET))

#define CNXK_EP_R_OUT_INT_LEVELS(ring)          \
	(CNXK_EP_R_OUT_INT_LEVELS_START + ((ring) * CNXK_EP_RING_OFFSET))

#define CNXK_EP_R_OUT_SLIST_BADDR(ring)          \
	(CNXK_EP_R_OUT_SLIST_BADDR_START + ((ring) * CNXK_EP_RING_OFFSET))

#define CNXK_EP_R_OUT_SLIST_RSIZE(ring)          \
	(CNXK_EP_R_OUT_SLIST_RSIZE_START + ((ring) * CNXK_EP_RING_OFFSET))

#define CNXK_EP_R_OUT_SLIST_DBELL(ring)          \
	(CNXK_EP_R_OUT_SLIST_DBELL_START + ((ring) * CNXK_EP_RING_OFFSET))

#define CNXK_EP_R_OUT_CONTROL(ring)              \
	(CNXK_EP_R_OUT_CONTROL_START + ((ring) * CNXK_EP_RING_OFFSET))

#define CNXK_EP_R_OUT_ENABLE(ring)               \
	(CNXK_EP_R_OUT_ENABLE_START + ((ring) * CNXK_EP_RING_OFFSET))

#define CNXK_EP_R_OUT_WMARK(ring)                \
	(CNXK_EP_R_OUT_WMARK_START + ((ring) * CNXK_EP_RING_OFFSET))

#define CNXK_EP_R_OUT_PKT_CNT(ring)              \
	(CNXK_EP_R_OUT_PKT_CNT_START + ((ring) * CNXK_EP_RING_OFFSET))

#define CNXK_EP_R_OUT_BYTE_CNT(ring)             \
	(CNXK_EP_R_OUT_BYTE_CNT_START + ((ring) * CNXK_EP_RING_OFFSET))

#define CNXK_EP_R_OUT_CNTS_ISM(ring)             \
	(CNXK_EP_R_OUT_CNTS_ISM_START + ((ring) * CNXK_EP_RING_OFFSET))

#define CNXK_EP_R_MBOX_VF_PF_DATA(ring)          \
	(CNXK_EP_R_MBOX_VF_PF_DATA_START + ((ring) * CNXK_EP_RING_OFFSET))

#define CNXK_EP_R_MBOX_PF_VF_INT(ring)           \
	(CNXK_EP_R_MBOX_PF_VF_INT_START + ((ring) * CNXK_EP_RING_OFFSET))

/*------------------ R_OUT Masks ----------------*/
#define CNXK_EP_R_OUT_INT_LEVELS_BMODE       (1ULL << 63)
#define CNXK_EP_R_OUT_INT_LEVELS_TIMET       (32)

#define CNXK_EP_R_OUT_CTL_IDLE               (1ULL << 40)
#define CNXK_EP_R_OUT_CTL_ES_I         (1ull << 34)
#define CNXK_EP_R_OUT_CTL_NSR_I              (1ULL << 33)
#define CNXK_EP_R_OUT_CTL_ROR_I              (1ULL << 32)
#define CNXK_EP_R_OUT_CTL_ES_D         (1ull << 30)
#define CNXK_EP_R_OUT_CTL_NSR_D              (1ULL << 29)
#define CNXK_EP_R_OUT_CTL_ROR_D              (1ULL << 28)
#define CNXK_EP_R_OUT_CTL_ES_P         (1ull << 26)
#define CNXK_EP_R_OUT_CTL_NSR_P              (1ULL << 25)
#define CNXK_EP_R_OUT_CTL_ROR_P              (1ULL << 24)
#define CNXK_EP_R_OUT_CTL_IMODE              (1ULL << 23)

#define PCI_DEVID_CN10KA_EP_NET_VF		0xB903
#define PCI_DEVID_CNF10KA_EP_NET_VF		0xBA03
#define PCI_DEVID_CNF10KB_EP_NET_VF		0xBC03
#define PCI_DEVID_CN10KB_EP_NET_VF		0xBD03

int
cnxk_ep_vf_setup_device(struct otx_ep_device *sdpvf);

struct cnxk_ep_instr_64B {
	/* Pointer where the input data is available. */
	uint64_t dptr;

	/* OTX_EP Instruction Header. */
	union otx_ep_instr_ih ih;

	/** Pointer where the response for a RAW mode packet
	 * will be written by OCTEON TX.
	 */
	uint64_t rptr;

	/* Input Request Header. */
	union otx_ep_instr_irh irh;

	/* Additional headers available in a 64-byte instruction. */
	uint64_t exhdr[4];
};

#define CNXK_EP_IQ_ISM_OFFSET(queue)    (RTE_CACHE_LINE_SIZE * (queue) + 4)
#define CNXK_EP_OQ_ISM_OFFSET(queue)    (RTE_CACHE_LINE_SIZE * (queue))
#define CNXK_EP_ISM_EN                  (0x1)
#define CNXK_EP_ISM_MSIX_DIS            (0x2)

#endif /*_CNXK_EP_VF_H_ */
