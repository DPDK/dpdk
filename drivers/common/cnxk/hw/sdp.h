/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef __SDP_HW_H_
#define __SDP_HW_H_

/* SDP VF IOQs */
#define SDP_MIN_RINGS_PER_VF (1)
#define SDP_MAX_RINGS_PER_VF (8)

/* SDP VF IQ configuration */
#define SDP_VF_MAX_IQ_DESCRIPTORS (512)
#define SDP_VF_MIN_IQ_DESCRIPTORS (128)

#define SDP_VF_DB_MIN	      (1)
#define SDP_VF_DB_TIMEOUT     (1)
#define SDP_VF_INTR_THRESHOLD (0xFFFFFFFF)

#define SDP_VF_64BYTE_INSTR (64)
#define SDP_VF_32BYTE_INSTR (32)

/* SDP VF OQ configuration */
#define SDP_VF_MAX_OQ_DESCRIPTORS (512)
#define SDP_VF_MIN_OQ_DESCRIPTORS (128)
#define SDP_VF_OQ_BUF_SIZE	  (2048)
#define SDP_VF_OQ_REFIL_THRESHOLD (16)

#define SDP_VF_OQ_INFOPTR_MODE (1)
#define SDP_VF_OQ_BUFPTR_MODE  (0)

#define SDP_VF_OQ_INTR_PKT   (1)
#define SDP_VF_OQ_INTR_TIME  (10)
#define SDP_VF_CFG_IO_QUEUES SDP_MAX_RINGS_PER_VF

/* Wait time in milliseconds for FLR */
#define SDP_VF_PCI_FLR_WAIT    (100)
#define SDP_VF_BUSY_LOOP_COUNT (10000)

#define SDP_VF_MAX_IO_QUEUES SDP_MAX_RINGS_PER_VF
#define SDP_VF_MIN_IO_QUEUES SDP_MIN_RINGS_PER_VF

/* SDP VF IOQs per rawdev */
#define SDP_VF_MAX_IOQS_PER_RAWDEV     SDP_VF_MAX_IO_QUEUES
#define SDP_VF_DEFAULT_IOQS_PER_RAWDEV SDP_VF_MIN_IO_QUEUES

/* SDP VF Register definitions */
#define SDP_VF_RING_OFFSET (0x1ull << 17)

/* SDP VF IQ Registers */
#define SDP_VF_R_IN_CONTROL_START     (0x10000)
#define SDP_VF_R_IN_ENABLE_START      (0x10010)
#define SDP_VF_R_IN_INSTR_BADDR_START (0x10020)
#define SDP_VF_R_IN_INSTR_RSIZE_START (0x10030)
#define SDP_VF_R_IN_INSTR_DBELL_START (0x10040)
#define SDP_VF_R_IN_CNTS_START	      (0x10050)
#define SDP_VF_R_IN_INT_LEVELS_START  (0x10060)
#define SDP_VF_R_IN_PKT_CNT_START     (0x10080)
#define SDP_VF_R_IN_BYTE_CNT_START    (0x10090)

#define SDP_VF_R_IN_CONTROL(ring)                                              \
	(SDP_VF_R_IN_CONTROL_START + (SDP_VF_RING_OFFSET * (ring)))

#define SDP_VF_R_IN_ENABLE(ring)                                               \
	(SDP_VF_R_IN_ENABLE_START + (SDP_VF_RING_OFFSET * (ring)))

#define SDP_VF_R_IN_INSTR_BADDR(ring)                                          \
	(SDP_VF_R_IN_INSTR_BADDR_START + (SDP_VF_RING_OFFSET * (ring)))

#define SDP_VF_R_IN_INSTR_RSIZE(ring)                                          \
	(SDP_VF_R_IN_INSTR_RSIZE_START + (SDP_VF_RING_OFFSET * (ring)))

#define SDP_VF_R_IN_INSTR_DBELL(ring)                                          \
	(SDP_VF_R_IN_INSTR_DBELL_START + (SDP_VF_RING_OFFSET * (ring)))

#define SDP_VF_R_IN_CNTS(ring)                                                 \
	(SDP_VF_R_IN_CNTS_START + (SDP_VF_RING_OFFSET * (ring)))

#define SDP_VF_R_IN_INT_LEVELS(ring)                                           \
	(SDP_VF_R_IN_INT_LEVELS_START + (SDP_VF_RING_OFFSET * (ring)))

#define SDP_VF_R_IN_PKT_CNT(ring)                                              \
	(SDP_VF_R_IN_PKT_CNT_START + (SDP_VF_RING_OFFSET * (ring)))

#define SDP_VF_R_IN_BYTE_CNT(ring)                                             \
	(SDP_VF_R_IN_BYTE_CNT_START + (SDP_VF_RING_OFFSET * (ring)))

/* SDP VF IQ Masks */
#define SDP_VF_R_IN_CTL_RPVF_MASK (0xF)
#define SDP_VF_R_IN_CTL_RPVF_POS  (48)

#define SDP_VF_R_IN_CTL_IDLE   (0x1ull << 28)
#define SDP_VF_R_IN_CTL_RDSIZE (0x3ull << 25) /* Setting to max(4) */
#define SDP_VF_R_IN_CTL_IS_64B (0x1ull << 24)
#define SDP_VF_R_IN_CTL_D_NSR  (0x1ull << 8)
#define SDP_VF_R_IN_CTL_D_ESR  (0x1ull << 6)
#define SDP_VF_R_IN_CTL_D_ROR  (0x1ull << 5)
#define SDP_VF_R_IN_CTL_NSR    (0x1ull << 3)
#define SDP_VF_R_IN_CTL_ESR    (0x1ull << 1)
#define SDP_VF_R_IN_CTL_ROR    (0x1ull << 0)

#define SDP_VF_R_IN_CTL_MASK (SDP_VF_R_IN_CTL_RDSIZE | SDP_VF_R_IN_CTL_IS_64B)

/* SDP VF OQ Registers */
#define SDP_VF_R_OUT_CNTS_START	       (0x10100)
#define SDP_VF_R_OUT_INT_LEVELS_START  (0x10110)
#define SDP_VF_R_OUT_SLIST_BADDR_START (0x10120)
#define SDP_VF_R_OUT_SLIST_RSIZE_START (0x10130)
#define SDP_VF_R_OUT_SLIST_DBELL_START (0x10140)
#define SDP_VF_R_OUT_CONTROL_START     (0x10150)
#define SDP_VF_R_OUT_ENABLE_START      (0x10160)
#define SDP_VF_R_OUT_PKT_CNT_START     (0x10180)
#define SDP_VF_R_OUT_BYTE_CNT_START    (0x10190)

#define SDP_VF_R_OUT_CONTROL(ring)                                             \
	(SDP_VF_R_OUT_CONTROL_START + (SDP_VF_RING_OFFSET * (ring)))

#define SDP_VF_R_OUT_ENABLE(ring)                                              \
	(SDP_VF_R_OUT_ENABLE_START + (SDP_VF_RING_OFFSET * (ring)))

#define SDP_VF_R_OUT_SLIST_BADDR(ring)                                         \
	(SDP_VF_R_OUT_SLIST_BADDR_START + (SDP_VF_RING_OFFSET * (ring)))

#define SDP_VF_R_OUT_SLIST_RSIZE(ring)                                         \
	(SDP_VF_R_OUT_SLIST_RSIZE_START + (SDP_VF_RING_OFFSET * (ring)))

#define SDP_VF_R_OUT_SLIST_DBELL(ring)                                         \
	(SDP_VF_R_OUT_SLIST_DBELL_START + (SDP_VF_RING_OFFSET * (ring)))

#define SDP_VF_R_OUT_CNTS(ring)                                                \
	(SDP_VF_R_OUT_CNTS_START + (SDP_VF_RING_OFFSET * (ring)))

#define SDP_VF_R_OUT_INT_LEVELS(ring)                                          \
	(SDP_VF_R_OUT_INT_LEVELS_START + (SDP_VF_RING_OFFSET * (ring)))

#define SDP_VF_R_OUT_PKT_CNT(ring)                                             \
	(SDP_VF_R_OUT_PKT_CNT_START + (SDP_VF_RING_OFFSET * (ring)))

#define SDP_VF_R_OUT_BYTE_CNT(ring)                                            \
	(SDP_VF_R_OUT_BYTE_CNT_START + (SDP_VF_RING_OFFSET * (ring)))

/* SDP VF OQ Masks */
#define SDP_VF_R_OUT_CTL_IDLE  (1ull << 40)
#define SDP_VF_R_OUT_CTL_ES_I  (1ull << 34)
#define SDP_VF_R_OUT_CTL_NSR_I (1ull << 33)
#define SDP_VF_R_OUT_CTL_ROR_I (1ull << 32)
#define SDP_VF_R_OUT_CTL_ES_D  (1ull << 30)
#define SDP_VF_R_OUT_CTL_NSR_D (1ull << 29)
#define SDP_VF_R_OUT_CTL_ROR_D (1ull << 28)
#define SDP_VF_R_OUT_CTL_ES_P  (1ull << 26)
#define SDP_VF_R_OUT_CTL_NSR_P (1ull << 25)
#define SDP_VF_R_OUT_CTL_ROR_P (1ull << 24)
#define SDP_VF_R_OUT_CTL_IMODE (1ull << 23)

#define SDP_VF_R_OUT_INT_LEVELS_BMODE (1ull << 63)
#define SDP_VF_R_OUT_INT_LEVELS_TIMET (32)

/* SDP Instruction Header */
struct sdp_instr_ih {
	/* Data Len */
	uint64_t tlen : 16;

	/* Reserved1 */
	uint64_t rsvd1 : 20;

	/* PKIND for SDP */
	uint64_t pkind : 6;

	/* Front Data size */
	uint64_t fsz : 6;

	/* No. of entries in gather list */
	uint64_t gsz : 14;

	/* Gather indicator */
	uint64_t gather : 1;

	/* Reserved2 */
	uint64_t rsvd2 : 1;
} __plt_packed;

#endif /* __SDP_HW_H_  */
