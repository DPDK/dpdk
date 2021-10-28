/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#ifndef _OTX2_EP_VF_H_
#define _OTX2_EP_VF_H_

#include <rte_io.h>

#define SDP_VF_R_IN_CTL_IDLE            (0x1ull << 28)
#define SDP_VF_R_IN_CTL_RDSIZE          (0x3ull << 25) /* Setting to max(4) */
#define SDP_VF_R_IN_CTL_IS_64B          (0x1ull << 24)
#define SDP_VF_R_IN_CTL_ESR             (0x1ull << 1)

#define SDP_VF_BUSY_LOOP_COUNT      (10000)

/* SDP VF OQ Masks */
#define SDP_VF_R_OUT_CTL_IDLE         (1ull << 40)
#define SDP_VF_R_OUT_CTL_ES_I         (1ull << 34)
#define SDP_VF_R_OUT_CTL_NSR_I        (1ull << 33)
#define SDP_VF_R_OUT_CTL_ROR_I        (1ull << 32)
#define SDP_VF_R_OUT_CTL_ES_D         (1ull << 30)
#define SDP_VF_R_OUT_CTL_NSR_D        (1ull << 29)
#define SDP_VF_R_OUT_CTL_ROR_D        (1ull << 28)
#define SDP_VF_R_OUT_CTL_ES_P         (1ull << 26)
#define SDP_VF_R_OUT_CTL_NSR_P        (1ull << 25)
#define SDP_VF_R_OUT_CTL_ROR_P        (1ull << 24)
#define SDP_VF_R_OUT_CTL_IMODE        (1ull << 23)

/* SDP VF Register definitions */
#define SDP_VF_RING_OFFSET                (0x1ull << 17)

/* SDP VF IQ Registers */
#define SDP_VF_R_IN_CONTROL_START         (0x10000)
#define SDP_VF_R_IN_ENABLE_START          (0x10010)
#define SDP_VF_R_IN_INSTR_BADDR_START     (0x10020)
#define SDP_VF_R_IN_INSTR_RSIZE_START     (0x10030)
#define SDP_VF_R_IN_INSTR_DBELL_START     (0x10040)
#define SDP_VF_R_IN_CNTS_START            (0x10050)
#define SDP_VF_R_IN_INT_LEVELS_START      (0x10060)
#define SDP_VF_R_IN_PKT_CNT_START         (0x10080)
#define SDP_VF_R_IN_BYTE_CNT_START        (0x10090)

#define SDP_VF_R_IN_CONTROL(ring)  \
	(SDP_VF_R_IN_CONTROL_START + ((ring) * SDP_VF_RING_OFFSET))

#define SDP_VF_R_IN_ENABLE(ring)   \
	(SDP_VF_R_IN_ENABLE_START + ((ring) * SDP_VF_RING_OFFSET))

#define SDP_VF_R_IN_INSTR_BADDR(ring)   \
	(SDP_VF_R_IN_INSTR_BADDR_START + ((ring) * SDP_VF_RING_OFFSET))

#define SDP_VF_R_IN_INSTR_RSIZE(ring)   \
	(SDP_VF_R_IN_INSTR_RSIZE_START + ((ring) * SDP_VF_RING_OFFSET))

#define SDP_VF_R_IN_INSTR_DBELL(ring)   \
	(SDP_VF_R_IN_INSTR_DBELL_START + ((ring) * SDP_VF_RING_OFFSET))

#define SDP_VF_R_IN_CNTS(ring)          \
	(SDP_VF_R_IN_CNTS_START + ((ring) * SDP_VF_RING_OFFSET))

#define SDP_VF_R_IN_INT_LEVELS(ring)    \
	(SDP_VF_R_IN_INT_LEVELS_START + ((ring) * SDP_VF_RING_OFFSET))

#define SDP_VF_R_IN_PKT_CNT(ring)       \
	(SDP_VF_R_IN_PKT_CNT_START + ((ring) * SDP_VF_RING_OFFSET))

#define SDP_VF_R_IN_BYTE_CNT(ring)          \
	(SDP_VF_R_IN_BYTE_CNT_START + ((ring) * SDP_VF_RING_OFFSET))

/* SDP VF OQ Registers */
#define SDP_VF_R_OUT_CNTS_START              (0x10100)
#define SDP_VF_R_OUT_INT_LEVELS_START        (0x10110)
#define SDP_VF_R_OUT_SLIST_BADDR_START       (0x10120)
#define SDP_VF_R_OUT_SLIST_RSIZE_START       (0x10130)
#define SDP_VF_R_OUT_SLIST_DBELL_START       (0x10140)
#define SDP_VF_R_OUT_CONTROL_START           (0x10150)
#define SDP_VF_R_OUT_ENABLE_START            (0x10160)
#define SDP_VF_R_OUT_PKT_CNT_START           (0x10180)
#define SDP_VF_R_OUT_BYTE_CNT_START          (0x10190)

#define SDP_VF_R_OUT_CONTROL(ring)    \
	(SDP_VF_R_OUT_CONTROL_START + ((ring) * SDP_VF_RING_OFFSET))

#define SDP_VF_R_OUT_ENABLE(ring)     \
	(SDP_VF_R_OUT_ENABLE_START + ((ring) * SDP_VF_RING_OFFSET))

#define SDP_VF_R_OUT_SLIST_BADDR(ring)  \
	(SDP_VF_R_OUT_SLIST_BADDR_START + ((ring) * SDP_VF_RING_OFFSET))

#define SDP_VF_R_OUT_SLIST_RSIZE(ring)  \
	(SDP_VF_R_OUT_SLIST_RSIZE_START + ((ring) * SDP_VF_RING_OFFSET))

#define SDP_VF_R_OUT_SLIST_DBELL(ring)  \
	(SDP_VF_R_OUT_SLIST_DBELL_START + ((ring) * SDP_VF_RING_OFFSET))

#define SDP_VF_R_OUT_CNTS(ring)   \
	(SDP_VF_R_OUT_CNTS_START + ((ring) * SDP_VF_RING_OFFSET))

#define SDP_VF_R_OUT_INT_LEVELS(ring)   \
	(SDP_VF_R_OUT_INT_LEVELS_START + ((ring) * SDP_VF_RING_OFFSET))

#define SDP_VF_R_OUT_PKT_CNT(ring)   \
	(SDP_VF_R_OUT_PKT_CNT_START + ((ring) * SDP_VF_RING_OFFSET))

#define SDP_VF_R_OUT_BYTE_CNT(ring)   \
	(SDP_VF_R_OUT_BYTE_CNT_START + ((ring) * SDP_VF_RING_OFFSET))

/* SDP VF IQ Masks */
#define SDP_VF_R_IN_CTL_RPVF_MASK       (0xF)
#define	SDP_VF_R_IN_CTL_RPVF_POS        (48)

/* IO Access */
#define otx2_read64(addr) rte_read64_relaxed((void *)(addr))
#define otx2_write64(val, addr) rte_write64_relaxed((val), (void *)(addr))

#define PCI_DEVID_OCTEONTX2_EP_NET_VF		0xB203 /* OCTEON TX2 EP mode */
#define PCI_DEVID_CN98XX_EP_NET_VF		0xB103

int
otx2_ep_vf_setup_device(struct otx_ep_device *sdpvf);

struct otx2_ep_instr_64B {
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

#endif /*_OTX2_EP_VF_H_ */

