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

/* DPI CN20K LF register offsets from VF_BAR2 */
#define DPI_LF_CTL		    (0ull)
#define DPI_LF_RINGX_CFG(x)	    ((0x20ull | (uint64_t)(x) << 3))
#define DPI_LF_RINGX_BASE(x)	    ((0x30ull | (uint64_t)(x) << 3))
#define DPI_LF_RINGX_RIDX(x)	    ((0x40ull | (uint64_t)(x) << 3))
#define DPI_LF_RINGX_WIDX(x)	    ((0x50ull | (uint64_t)(x) << 3))
#define DPI_LF_RINGX_RST(x)	    ((0x70ull | (uint64_t)(x) << 3))
#define DPI_LF_RINGX_ISTAT(x)	    ((0x80ull | (uint64_t)(x) << 3))
#define DPI_LF_RINGX_CMPL(x)	    ((0x90ull | (uint64_t)(x) << 3))
#define DPI_LF_RINGX_INT	    (0x100ull)
#define DPI_LF_RINGX_INT_W1S	    (0x108ull)
#define DPI_LF_RINGX_INT_ENA_W1C    (0x110ull)
#define DPI_LF_RINGX_INT_ENA_W1S    (0x118ull)
#define DPI_LF_RINGX_ERR_STAT(x)    ((0x120ull | (uint64_t)(x) << 3))
#define DPI_LF_RINGX_ERR	    (0x200ull)
#define DPI_LF_RINGX_ERR_W1S	    (0x208ull)
#define DPI_LF_RINGX_ERR_ENA_W1C    (0x210ull)
#define DPI_LF_RINGX_ERR_ENA_W1S    (0x218ull)
#define DPI_LF_RINGX_DMA_CNT(x)	    ((0x220ull | (uint64_t)(x) << 3))
#define DPI_LF_RINGX_DMA_BCNT(x)    ((0x230ull | (uint64_t)(x) << 3))

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

#define DPI_CMD_SIZE_64B  64
#define DPI_CMD_SIZE_128B 128

#define DPI_CMD_VLD_BIT BIT_ULL(63)

#define DPI_LF_QCFG_QEN	  BIT_ULL(63)
#define DPI_LF_QCFG_ISIZE BIT(11)
#define DPI_LF_QUEUE_RST  BIT(0)

#define DPI_LF_QIDX_WRAP_MASK 0x8000
#define DPI_LF_QSIZE_MASK     0xFF
#define DPI_LF_QIDX_MASK      0xFFF
#define DPI_LF_QSIZE_SHIFT    56

#define DPI_Q_RIDX(x) ((x)&0xFFF)
#define DPI_Q_WIDX(x) ((x)&0xFFF)

#define DPI_Q_RIDX_WRAP(x) (((x) >> 15) & 0x1)
#define DPI_Q_WIDX_WRAP(x) (((x) >> 15) & 0x1)

/**
 * Structure dpi_instr_hdr_s for CN9K
 *
 * DPI DMA Instruction Header Format
 */
union dpi_instr_hdr_s {
	uint64_t u[4];
	struct dpi_cn9k_instr_hdr_s {
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

	struct dpi_cn10k_instr_hdr_s {
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

	struct dpi_cn20k_instr_hdr_s {
		uint64_t nfst : 3;
		uint64_t reserved_3 : 1;
		uint64_t nlst : 3;
		uint64_t reserved_7 : 1;
		uint64_t msix_int : 1;
		uint64_t ct : 3;
		uint64_t chan : 14;
		uint64_t reserved_26_29 : 4;
		uint64_t aura : 20;
		uint64_t xt : 2;
		uint64_t ivec : 9;
		uint64_t fe : 1;
		uint64_t reserved_62 : 1;
		uint64_t vld : 1;
		/* Word 0 - End */
		uint64_t ptr : 64;
		/* Word 1 - End */
		uint64_t tag : 32;
		uint64_t tt : 2;
		uint64_t grp : 10;
		uint64_t reserved_107_127 : 20;
		/* Word 2 - End */
		uint64_t reserved_128_191 : 64;
		/* Word 3 - End */
	} cn20k;
};

#endif /*__DEV_DPI_HW_H__*/
