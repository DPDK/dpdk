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
#define SDP_VF_R_OUT_CTL_IDLE         (0x1ull << 40)
#define SDP_VF_R_OUT_CTL_ES_I         (0x1ull << 34)
#define SDP_VF_R_OUT_CTL_NSR_I        (0x1ull << 33)
#define SDP_VF_R_OUT_CTL_ROR_I        (0x1ull << 32)
#define SDP_VF_R_OUT_CTL_ES_D         (0x1ull << 30)
#define SDP_VF_R_OUT_CTL_NSR_D        (0x1ull << 29)
#define SDP_VF_R_OUT_CTL_ROR_D        (0x1ull << 28)
#define SDP_VF_R_OUT_CTL_ES_P         (0x1ull << 26)
#define SDP_VF_R_OUT_CTL_NSR_P        (0x1ull << 25)
#define SDP_VF_R_OUT_CTL_ROR_P        (0x1ull << 24)
#define SDP_VF_R_OUT_CTL_IMODE        (0x1ull << 23)
#define SDP_VF_R_OUT_CNTS_OUT_INT     (0x1ull << 62)
#define SDP_VF_R_OUT_CNTS_IN_INT      (0x1ull << 61)
#define SDP_VF_R_IN_CNTS_OUT_INT      (0x1ull << 62)

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
#define SDP_VF_R_IN_CNTS_ISM_START        (0x10520)

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

#define SDP_VF_R_IN_CNTS_ISM(ring)          \
	(SDP_VF_R_IN_CNTS_ISM_START + (SDP_VF_RING_OFFSET * (ring)))

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
#define SDP_VF_R_OUT_CNTS_ISM_START          (0x10510)

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

#define SDP_VF_R_OUT_CNTS_ISM(ring)   \
	(SDP_VF_R_OUT_CNTS_ISM_START + (SDP_VF_RING_OFFSET * (ring)))

/* SDP VF IQ Masks */
#define SDP_VF_R_IN_CTL_RPVF_MASK       (0xF)
#define	SDP_VF_R_IN_CTL_RPVF_POS        (48)

/* IO Access */
#define otx2_read64(addr) rte_read64_relaxed((void *)(addr))
#define otx2_write64(val, addr) rte_write64_relaxed((val), (void *)(addr))

#define PCI_DEVID_CN9K_EP_NET_VF		0xB203 /* OCTEON 9 EP mode */
#define PCI_DEVID_CN98XX_EP_NET_VF		0xB103
#define PCI_DEVID_CNF95N_EP_NET_VF		0xB403
#define PCI_DEVID_CNF95O_EP_NET_VF		0xB603

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

#define OTX2_EP_IQ_ISM_OFFSET(queue)   (RTE_CACHE_LINE_SIZE * (queue) + 4)
#define OTX2_EP_OQ_ISM_OFFSET(queue)   (RTE_CACHE_LINE_SIZE * (queue))
#define OTX2_EP_ISM_EN                 (0x1)
#define OTX2_EP_ISM_MSIX_DIS           (0x2)
#define OTX2_EP_MAX_RX_PKT_LEN         (16384)

union out_int_lvl_t {
	uint64_t d64;
	struct {
		uint64_t cnt:32;
		uint64_t timet:22;
		uint64_t max_len:7;
		uint64_t max_len_en:1;
		uint64_t time_cnt_en:1;
		uint64_t bmode:1;
	} s;
};

union out_cnts_t {
	uint64_t d64;
	struct {
		uint64_t cnt:32;
		uint64_t timer:22;
		uint64_t rsvd:5;
		uint64_t resend:1;
		uint64_t mbox_int:1;
		uint64_t in_int:1;
		uint64_t out_int:1;
		uint64_t send_ism:1;
	} s;
};

#define OTX2_EP_64B_INSTR_SIZE	(sizeof(otx2_ep_instr_64B))

#define NIX_MAX_HW_FRS			9212
#define NIX_MAX_VTAG_INS		2
#define NIX_MAX_VTAG_ACT_SIZE		(4 * NIX_MAX_VTAG_INS)
#define NIX_MAX_FRS	\
	(NIX_MAX_HW_FRS + RTE_ETHER_CRC_LEN - NIX_MAX_VTAG_ACT_SIZE)

#define CN93XX_INTR_R_OUT_INT        (1ULL << 62)
#define CN93XX_INTR_R_IN_INT         (1ULL << 61)
#endif /*_OTX2_EP_VF_H_ */
