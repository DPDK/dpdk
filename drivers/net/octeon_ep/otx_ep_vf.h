/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#ifndef _OTX_EP_VF_H_
#define _OTX_EP_VF_H_

#define OTX_EP_RING_OFFSET                (0x1ull << 17)

/* OTX_EP VF IQ Registers */
#define OTX_EP_R_IN_CONTROL_START         (0x10000)
#define OTX_EP_R_IN_ENABLE_START          (0x10010)
#define OTX_EP_R_IN_INSTR_BADDR_START     (0x10020)
#define OTX_EP_R_IN_INSTR_RSIZE_START     (0x10030)
#define OTX_EP_R_IN_INSTR_DBELL_START     (0x10040)
#define OTX_EP_R_IN_CNTS_START            (0x10050)
#define OTX_EP_R_IN_INT_LEVELS_START      (0x10060)

#define OTX_EP_R_IN_CONTROL(ring)  \
	(OTX_EP_R_IN_CONTROL_START + ((ring) * OTX_EP_RING_OFFSET))

#define OTX_EP_R_IN_ENABLE(ring)   \
	(OTX_EP_R_IN_ENABLE_START + ((ring) * OTX_EP_RING_OFFSET))

#define OTX_EP_R_IN_INSTR_BADDR(ring)   \
	(OTX_EP_R_IN_INSTR_BADDR_START + ((ring) * OTX_EP_RING_OFFSET))

#define OTX_EP_R_IN_INSTR_RSIZE(ring)   \
	(OTX_EP_R_IN_INSTR_RSIZE_START + ((ring) * OTX_EP_RING_OFFSET))

#define OTX_EP_R_IN_INSTR_DBELL(ring)   \
	(OTX_EP_R_IN_INSTR_DBELL_START + ((ring) * OTX_EP_RING_OFFSET))

#define OTX_EP_R_IN_CNTS(ring)          \
	(OTX_EP_R_IN_CNTS_START + ((ring) * OTX_EP_RING_OFFSET))

#define OTX_EP_R_IN_INT_LEVELS(ring)    \
	(OTX_EP_R_IN_INT_LEVELS_START + ((ring) * OTX_EP_RING_OFFSET))

/* OTX_EP VF IQ Masks */
#define OTX_EP_R_IN_CTL_RPVF_MASK       (0xF)
#define	OTX_EP_R_IN_CTL_RPVF_POS        (48)

#define OTX_EP_R_IN_CTL_IDLE            (0x1ull << 28)
#define OTX_EP_R_IN_CTL_RDSIZE          (0x3ull << 25) /* Setting to max(4) */
#define OTX_EP_R_IN_CTL_IS_64B          (0x1ull << 24)
#define OTX_EP_R_IN_CTL_ESR             (0x1ull << 1)
/* OTX_EP VF OQ Registers */
#define OTX_EP_R_OUT_CNTS_START              (0x10100)
#define OTX_EP_R_OUT_INT_LEVELS_START        (0x10110)
#define OTX_EP_R_OUT_SLIST_BADDR_START       (0x10120)
#define OTX_EP_R_OUT_SLIST_RSIZE_START       (0x10130)
#define OTX_EP_R_OUT_SLIST_DBELL_START       (0x10140)
#define OTX_EP_R_OUT_CONTROL_START           (0x10150)
#define OTX_EP_R_OUT_ENABLE_START            (0x10160)

#define OTX_EP_R_OUT_CONTROL(ring)    \
	(OTX_EP_R_OUT_CONTROL_START + ((ring) * OTX_EP_RING_OFFSET))

#define OTX_EP_R_OUT_ENABLE(ring)     \
	(OTX_EP_R_OUT_ENABLE_START + ((ring) * OTX_EP_RING_OFFSET))

#define OTX_EP_R_OUT_SLIST_BADDR(ring)  \
	(OTX_EP_R_OUT_SLIST_BADDR_START + ((ring) * OTX_EP_RING_OFFSET))

#define OTX_EP_R_OUT_SLIST_RSIZE(ring)  \
	(OTX_EP_R_OUT_SLIST_RSIZE_START + ((ring) * OTX_EP_RING_OFFSET))

#define OTX_EP_R_OUT_SLIST_DBELL(ring)  \
	(OTX_EP_R_OUT_SLIST_DBELL_START + ((ring) * OTX_EP_RING_OFFSET))

#define OTX_EP_R_OUT_CNTS(ring)   \
	(OTX_EP_R_OUT_CNTS_START + ((ring) * OTX_EP_RING_OFFSET))

#define OTX_EP_R_OUT_INT_LEVELS(ring)   \
	(OTX_EP_R_OUT_INT_LEVELS_START + ((ring) * OTX_EP_RING_OFFSET))

/* OTX_EP VF OQ Masks */

#define OTX_EP_R_OUT_CTL_IDLE         (1ull << 36)
#define OTX_EP_R_OUT_CTL_ES_I         (1ull << 34)
#define OTX_EP_R_OUT_CTL_NSR_I        (1ull << 33)
#define OTX_EP_R_OUT_CTL_ROR_I        (1ull << 32)
#define OTX_EP_R_OUT_CTL_ES_D         (1ull << 30)
#define OTX_EP_R_OUT_CTL_NSR_D        (1ull << 29)
#define OTX_EP_R_OUT_CTL_ROR_D        (1ull << 28)
#define OTX_EP_R_OUT_CTL_ES_P         (1ull << 26)
#define OTX_EP_R_OUT_CTL_NSR_P        (1ull << 25)
#define OTX_EP_R_OUT_CTL_ROR_P        (1ull << 24)
#define OTX_EP_R_OUT_CTL_IMODE        (1ull << 23)

#define PCI_DEVID_OCTEONTX_EP_VF 0xa303

/* this is a static value set by SLI PF driver in octeon
 * No handshake is available
 * Change this if changing the value in SLI PF driver
 */
#define SDP_GBL_WMARK 0x100


/* Optional PKI Instruction Header(PKI IH) */
typedef union {
	uint64_t u64;
	struct {
		/** Tag Value */
		uint64_t tag:32;

		/** QPG Value */
		uint64_t qpg:11;

		/** Reserved1 */
		uint64_t reserved1:2;

		/** Tag type */
		uint64_t tagtype:2;

		/** Use Tag Type */
		uint64_t utt:1;

		/** Skip Length */
		uint64_t sl:8;

		/** Parse Mode */
		uint64_t pm:3;

		/** Reserved2 */
		uint64_t reserved2:1;

		/** Use QPG */
		uint64_t uqpg:1;

		/** Use Tag */
		uint64_t utag:1;

		/** Raw mode indicator 1 = RAW */
		uint64_t raw:1;

		/** Wider bit */
		uint64_t w:1;
	} s;
} otx_ep_instr_pki_ih3_t;


/* OTX_EP 64B instruction format */
struct otx_ep_instr_64B {
	/* Pointer where the input data is available. */
	uint64_t dptr;

	/* OTX_EP Instruction Header. */
	union otx_ep_instr_ih ih;

	/* PKI Optional Instruction Header. */
	otx_ep_instr_pki_ih3_t pki_ih3;

	/** Pointer where the response for a RAW mode packet
	 * will be written by OCTEON TX.
	 */
	uint64_t rptr;

	/* Input Request Header. */
	union otx_ep_instr_irh irh;

	/* Additional headers available in a 64-byte instruction. */
	uint64_t exhdr[3];
};

int
otx_ep_vf_setup_device(struct otx_ep_device *otx_ep);
#endif /*_OTX_EP_VF_H_ */
