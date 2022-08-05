/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
/**
 * DPI device HW definitions.
 */
#ifndef DEV_DPI_HW_H
#define DEV_DPI_HW_H

#include <stdint.h>

/* DPI VF register offsets from VF_BAR0 */
#define DPI_VDMA_EN	   (0x0)
#define DPI_VDMA_REQQ_CTL  (0x8)
#define DPI_VDMA_DBELL	   (0x10)
#define DPI_VDMA_SADDR	   (0x18)
#define DPI_VDMA_COUNTS	   (0x20)
#define DPI_VDMA_NADDR	   (0x28)
#define DPI_VDMA_IWBUSY	   (0x30)
#define DPI_VDMA_CNT	   (0x38)
#define DPI_VF_INT	   (0x100)
#define DPI_VF_INT_W1S	   (0x108)
#define DPI_VF_INT_ENA_W1C (0x110)
#define DPI_VF_INT_ENA_W1S (0x118)

/**
 * Enumeration dpi_hdr_xtype_e
 *
 * DPI Transfer Type Enumeration
 * Enumerates the pointer type in DPI_DMA_INSTR_HDR_S[XTYPE].
 */
#define DPI_XTYPE_OUTBOUND	(0)
#define DPI_XTYPE_INBOUND	(1)
#define DPI_XTYPE_INTERNAL_ONLY (2)
#define DPI_XTYPE_EXTERNAL_ONLY (3)
#define DPI_HDR_XTYPE_MASK	0x3

#define DPI_HDR_PT_ZBW_CA	0x0
#define DPI_HDR_PT_ZBW_NC	0x1
#define DPI_HDR_PT_WQP		0x2
#define DPI_HDR_PT_WQP_NOSTATUS	0x0
#define DPI_HDR_PT_WQP_STATUSCA	0x1
#define DPI_HDR_PT_WQP_STATUSNC	0x3
#define DPI_HDR_PT_CNT		0x3
#define DPI_HDR_PT_MASK		0x3

#define DPI_HDR_TT_MASK		0x3
#define DPI_HDR_GRP_MASK	0x3FF
#define DPI_HDR_FUNC_MASK	0xFFFF

/* Big endian data bit position in DMA local pointer */
#define DPI_LPTR_BED_BIT_POS (60)

#define DPI_MIN_CMD_SIZE 8
#define DPI_MAX_CMD_SIZE 64

/**
 * Structure dpi_instr_hdr_s for CN9K
 *
 * DPI DMA Instruction Header Format
 */
union dpi_instr_hdr_s {
	uint64_t u[4];
	struct dpi_cn9k_instr_hdr_s_s {
		uint64_t tag : 32;
		uint64_t tt : 2;
		uint64_t grp : 10;
		uint64_t reserved_44_47 : 4;
		uint64_t nfst : 4;
		uint64_t reserved_52_53 : 2;
		uint64_t nlst : 4;
		uint64_t reserved_58_63 : 6;
		/* Word 0 - End */
		uint64_t aura : 20;
		uint64_t func : 16;
		uint64_t pt : 2;
		uint64_t reserved_102 : 1;
		uint64_t pvfe : 1;
		uint64_t fl : 1;
		uint64_t ii : 1;
		uint64_t fi : 1;
		uint64_t ca : 1;
		uint64_t csel : 1;
		uint64_t reserved_109_111 : 3;
		uint64_t xtype : 2;
		uint64_t reserved_114_119 : 6;
		uint64_t fport : 2;
		uint64_t reserved_122_123 : 2;
		uint64_t lport : 2;
		uint64_t reserved_126_127 : 2;
		/* Word 1 - End */
		uint64_t ptr : 64;
		/* Word 2 - End */
		uint64_t reserved_192_255 : 64;
		/* Word 3 - End */
	} cn9k;

	struct dpi_cn10k_instr_hdr_s_s {
		uint64_t nfst : 4;
		uint64_t reserved_4_5 : 2;
		uint64_t nlst : 4;
		uint64_t reserved_10_11 : 2;
		uint64_t pvfe : 1;
		uint64_t reserved_13 : 1;
		uint64_t func : 16;
		uint64_t aura : 20;
		uint64_t xtype : 2;
		uint64_t reserved_52_53 : 2;
		uint64_t pt : 2;
		uint64_t fport : 2;
		uint64_t reserved_58_59 : 2;
		uint64_t lport : 2;
		uint64_t reserved_62_63 : 2;
		/* Word 0 - End */
		uint64_t ptr : 64;
		/* Word 1 - End */
		uint64_t tag : 32;
		uint64_t tt : 2;
		uint64_t grp : 10;
		uint64_t reserved_172_173 : 2;
		uint64_t fl : 1;
		uint64_t ii : 1;
		uint64_t fi : 1;
		uint64_t ca : 1;
		uint64_t csel : 1;
		uint64_t reserved_179_191 : 3;
		/* Word 2 - End */
		uint64_t reserved_192_255 : 64;
		/* Word 3 - End */
	} cn10k;
};

#endif /*__DEV_DPI_HW_H__*/
