/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2026 NXP
 */

/*
 * Hardware register definitions for the NXP i.MX95 eDMA5 controller.
 * The block has a Management Page (MP) region followed by per-channel register
 * windows (base offset 0x10000, stride 0x8000, 64 channels, 64-byte TCD64).
 * All register accesses are little-endian.
 */

#ifndef IMX_EDMA5_HW_H
#define IMX_EDMA5_HW_H

#include <stdint.h>

/* Number of DMA channels implemented on i.MX95 eDMA5. */
#define IMX_EDMA5_MAX_CHANNELS		64

/* Per-channel register window geometry. */
#define IMX_EDMA5_CHAN_BASE_OFF		0x10000u
#define IMX_EDMA5_CHAN_STRIDE		0x8000u

/*
 * Management Page (MP) registers (offsets from register window base).
 * Only the fields used by this driver are documented here.
 */
#define IMX_EDMA5_MP_CSR		0x0000u	/* Management control */
#define IMX_EDMA5_MP_ES			0x0004u	/* Management error status */
#define IMX_EDMA5_MP_INT_LOW		0x0008u	/* Interrupt request (chan 0-31) */
#define IMX_EDMA5_MP_INT_HIGH		0x000Cu	/* Interrupt request (chan 32-63) */
#define IMX_EDMA5_MP_HRS_LOW		0x0010u	/* Hardware request status low */
#define IMX_EDMA5_MP_HRS_HIGH		0x0014u	/* Hardware request status high */

/* MP_CSR bit fields. */
#define IMX_EDMA5_MP_CSR_EDBG		(1u << 1)  /* Enable debug */
#define IMX_EDMA5_MP_CSR_ERCA		(1u << 2)  /* Enable round-robin arb */
#define IMX_EDMA5_MP_CSR_HAE		(1u << 4)  /* Halt after error */
#define IMX_EDMA5_MP_CSR_GCLC		(1u << 6)  /* Global clock control */
#define IMX_EDMA5_MP_CSR_GMRC		(1u << 7)  /* Global master ID replic */

/* MP_ES: valid bit indicates a logged error is present. */
#define IMX_EDMA5_MP_ES_VLD		(1u << 31)

/*
 * Per-channel control registers (offsets from a channel window base).
 * Layout matches the eDMA4/eDMA5 fsl_edma3_ch_reg structure.
 */
#define IMX_EDMA5_CH_CSR		0x00u	/* Channel control/status */
#define IMX_EDMA5_CH_ES			0x04u	/* Channel error status */
#define IMX_EDMA5_CH_INT		0x08u	/* Channel interrupt status */
#define IMX_EDMA5_CH_SBR		0x0Cu	/* System bus register */
#define IMX_EDMA5_CH_PRI		0x10u	/* Channel priority */
#define IMX_EDMA5_CH_MUX		0x14u	/* Channel multiplexor (source) */
#define IMX_EDMA5_CH_MATTR		0x18u	/* Memory attributes */

/* Channel window offset of the TCD (Transfer Control Descriptor). */
#define IMX_EDMA5_CH_TCD_OFF		0x20u

/* CH_CSR bit fields. */
#define IMX_EDMA5_CH_CSR_ERQ		(1u << 0)  /* Enable hardware request */
#define IMX_EDMA5_CH_CSR_EARQ		(1u << 1)  /* Enable async hw request */
#define IMX_EDMA5_CH_CSR_EEI		(1u << 2)  /* Enable error interrupt */
#define IMX_EDMA5_CH_CSR_DONE		(1u << 30) /* Channel done (w1c) */
#define IMX_EDMA5_CH_CSR_ACTIVE		(1u << 31) /* Channel active */

/* CH_ES: valid bit indicates a logged channel error. */
#define IMX_EDMA5_CH_ES_ERR		(1u << 31)

/* CH_INT: write 1 to clear the channel interrupt request. */
#define IMX_EDMA5_CH_INT_INT		(1u << 0)

/* CH_SBR: read/write privileged/secure attributes for bus mastering. */
#define IMX_EDMA5_CH_SBR_RD		(1u << 22)
#define IMX_EDMA5_CH_SBR_WR		(1u << 21)

/*
 * TCD64 field offsets, relative to the channel TCD base
 * (channel window base + IMX_EDMA5_CH_TCD_OFF).
 */
#define IMX_EDMA5_TCD_SADDR		0x00u
#define IMX_EDMA5_TCD_SOFF		0x08u
#define IMX_EDMA5_TCD_ATTR		0x0Au
#define IMX_EDMA5_TCD_NBYTES		0x0Cu
#define IMX_EDMA5_TCD_SLAST		0x10u
#define IMX_EDMA5_TCD_DADDR		0x18u
#define IMX_EDMA5_TCD_DLAST_SGA		0x20u
#define IMX_EDMA5_TCD_DOFF		0x28u
#define IMX_EDMA5_TCD_CITER		0x2Au
#define IMX_EDMA5_TCD_CSR		0x2Cu
#define IMX_EDMA5_TCD_BITER		0x2Eu

/* TCD ATTR sub-fields: transfer size is encoded as log2(bytes). GET_* extract. */
#define IMX_EDMA5_TCD_ATTR_DSIZE(x)	(((x) & 0x7u))
#define IMX_EDMA5_TCD_ATTR_SSIZE(x)	(((x) & 0x7u) << 8)
#define IMX_EDMA5_TCD_ATTR_GET_DSIZE(x)	((x) & 0x7u)
#define IMX_EDMA5_TCD_ATTR_GET_SSIZE(x)	(((x) >> 8) & 0x7u)

/* Transfer size encodings for ATTR SSIZE/DSIZE (log2 of bytes). */
#define IMX_EDMA5_TCD_SIZE_1B		0u
#define IMX_EDMA5_TCD_SIZE_2B		1u
#define IMX_EDMA5_TCD_SIZE_4B		2u
#define IMX_EDMA5_TCD_SIZE_8B		3u
#define IMX_EDMA5_TCD_SIZE_16B		4u
#define IMX_EDMA5_TCD_SIZE_32B		5u
#define IMX_EDMA5_TCD_SIZE_64B		6u

/* Major iteration count field mask (15-bit CITER/BITER). */
#define IMX_EDMA5_TCD_ITER_MASK		0x7FFFu

/* TCD CSR bit fields. */
#define IMX_EDMA5_TCD_CSR_START		(1u << 0)  /* Software start */
#define IMX_EDMA5_TCD_CSR_INT_MAJOR	(1u << 1)  /* Interrupt on major done */
#define IMX_EDMA5_TCD_CSR_INT_HALF	(1u << 2)  /* Interrupt on half done */
#define IMX_EDMA5_TCD_CSR_D_REQ		(1u << 3)  /* Disable request on done */
#define IMX_EDMA5_TCD_CSR_E_SG		(1u << 4)  /* Enable scatter-gather */
#define IMX_EDMA5_TCD_CSR_E_LINK	(1u << 5)  /* Enable channel linking */
#define IMX_EDMA5_TCD_CSR_ACTIVE	(1u << 6)  /* Channel active */
#define IMX_EDMA5_TCD_CSR_DONE		(1u << 7)  /* Channel done */

/*
 * In-memory 64-bit Transfer Control Descriptor. The field order and offsets
 * match the register TCD64 layout above; all fields are little-endian. The
 * descriptor must be 32-byte aligned and, on this non-coherent SoC, cleaned
 * from the CPU cache before the transfer is started.
 */
struct __rte_aligned(32) imx_edma5_hw_tcd64 {
	uint64_t saddr;		/* 0x00 source address */
	uint16_t soff;		/* 0x08 source offset */
	uint16_t attr;		/* 0x0A transfer attributes */
	uint32_t nbytes;	/* 0x0C minor loop byte count */
	uint64_t slast;		/* 0x10 last source adjustment */
	uint64_t daddr;		/* 0x18 destination address */
	uint64_t dlast_sga;	/* 0x20 next TCD address (scatter-gather) */
	uint16_t doff;		/* 0x28 destination offset */
	uint16_t citer;		/* 0x2A current major iteration count */
	uint16_t csr;		/* 0x2C control and status */
	uint16_t biter;		/* 0x2E starting major iteration count */
	/* Pad to 64 bytes total; the type is 32-byte aligned for TCD fetches. */
	uint8_t  reserved[16];
};

#endif /* IMX_EDMA5_HW_H */
