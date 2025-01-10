/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021-2024 NXP
 */

#ifndef _DPAA_QDMA_H_
#define _DPAA_QDMA_H_

#include <rte_io.h>

#ifndef BIT
#define BIT(nr)		(1UL << (nr))
#endif

#define RETRIES	5

#ifndef GENMASK
#define BITS_PER_LONG	(__SIZEOF_LONG__ * 8)
#define GENMASK(h, l) \
		(((~0UL) << (l)) & (~0UL >> (BITS_PER_LONG - 1 - (h))))
#endif

#define QDMA_CTRL_REGION_OFFSET 0
#define QDMA_CTRL_REGION_SIZE 0x10000
#define QDMA_STATUS_REGION_OFFSET \
	(QDMA_CTRL_REGION_OFFSET + QDMA_CTRL_REGION_SIZE)
#define QDMA_STATUS_REGION_SIZE 0x10000

#define DPAA_QDMA_FLAGS_INDEX RTE_BIT64(63)
#define DPAA_QDMA_COPY_IDX_OFFSET 8
#define DPAA_QDMA_SG_IDX_ADDR_ALIGN \
	RTE_BIT64(DPAA_QDMA_COPY_IDX_OFFSET)
#define DPAA_QDMA_SG_IDX_ADDR_MASK \
	(DPAA_QDMA_SG_IDX_ADDR_ALIGN - 1)

#define FSL_QDMA_DMR			0x0
#define FSL_QDMA_DSR			0x4
#define FSL_QDMA_DEDR			0xe04
#define FSL_QDMA_DECFDW0R		0xe10
#define FSL_QDMA_DECFDW1R		0xe14
#define FSL_QDMA_DECFDW2R		0xe18
#define FSL_QDMA_DECFDW3R		0xe1c
#define FSL_QDMA_DECFQIDR		0xe30
#define FSL_QDMA_DECBR			0xe34

#define FSL_QDMA_BCQMR(x)		(0xc0 + 0x100 * (x))
#define FSL_QDMA_BCQSR(x)		(0xc4 + 0x100 * (x))
#define FSL_QDMA_BCQEDPA_SADDR(x)	(0xc8 + 0x100 * (x))
#define FSL_QDMA_BCQDPA_SADDR(x)	(0xcc + 0x100 * (x))
#define FSL_QDMA_BCQEEPA_SADDR(x)	(0xd0 + 0x100 * (x))
#define FSL_QDMA_BCQEPA_SADDR(x)	(0xd4 + 0x100 * (x))
#define FSL_QDMA_BCQIER(x)		(0xe0 + 0x100 * (x))
#define FSL_QDMA_BCQIDR(x)		(0xe4 + 0x100 * (x))

#define FSL_QDMA_SQEDPAR		0x808
#define FSL_QDMA_SQDPAR			0x80c
#define FSL_QDMA_SQEEPAR		0x810
#define FSL_QDMA_SQEPAR			0x814
#define FSL_QDMA_BSQMR			0x800
#define FSL_QDMA_BSQSR			0x804
#define FSL_QDMA_BSQICR			0x828
#define FSL_QDMA_CQIER			0xa10
#define FSL_QDMA_SQCCMR			0xa20

#define FSL_QDMA_SQCCMR_ENTER_WM	0x200000

#define FSL_QDMA_QUEUE_MAX		8

#define FSL_QDMA_BCQMR_EN		0x80000000
#define FSL_QDMA_BCQMR_EI		0x40000000

#define FSL_QDMA_BCQMR_CD_THLD(x)	((x) << 20)
#define FSL_QDMA_BCQMR_CQ_SIZE(x)	((x) << 16)

#define FSL_QDMA_BCQSR_QF_XOFF_BE	0x1000100

#define FSL_QDMA_BSQMR_EN		0x80000000
#define FSL_QDMA_BSQMR_CQ_SIZE(x)	((x) << 16)
#define FSL_QDMA_BSQMR_DI		0xc0

#define FSL_QDMA_BSQSR_QE_BE		0x200

#define FSL_QDMA_DMR_DQD		0x40000000
#define FSL_QDMA_DSR_DB			0x80000000

#define FSL_QDMA_CIRCULAR_DESC_SIZE_MIN	64
#define FSL_QDMA_CIRCULAR_DESC_SIZE_MAX	16384
#define FSL_QDMA_QUEUE_NUM_MAX		8

#define FSL_QDMA_COMP_SG_FORMAT		0x1

#define FSL_QDMA_CMD_RWTTYPE		0x4
#define FSL_QDMA_CMD_LWC		0x2

#define FSL_QDMA_CMD_SS_ERR050757_LEN 128

/* qdma engine attribute */
#define QDMA_QUEUE_SIZE FSL_QDMA_CIRCULAR_DESC_SIZE_MIN
#define QDMA_STATUS_SIZE QDMA_QUEUE_SIZE
#define QDMA_CCSR_BASE 0x8380000
#define QDMA_BLOCK_OFFSET 0x10000
#define QDMA_BLOCKS 4
#define QDMA_QUEUES 8
#define QDMA_QUEUE_CR_WM 32

#define QDMA_BIG_ENDIAN			1
#ifdef QDMA_BIG_ENDIAN
#define QDMA_IN(addr)		be32_to_cpu(rte_read32(addr))
#define QDMA_OUT(addr, val)	rte_write32(be32_to_cpu(val), addr)
#define QDMA_IN_BE(addr)	rte_read32(addr)
#define QDMA_OUT_BE(addr, val)	rte_write32(val, addr)
#else
#define QDMA_IN(addr)		rte_read32(addr)
#define QDMA_OUT(addr, val)	rte_write32(val, addr)
#define QDMA_IN_BE(addr)	be32_to_cpu(rte_write32(addr))
#define QDMA_OUT_BE(addr, val)	rte_write32(be32_to_cpu(val), addr)
#endif

#define FSL_QDMA_BLOCK_BASE_OFFSET(fsl_qdma_engine, x)			\
	(((fsl_qdma_engine)->block_offset) * (x))

/* qDMA Command Descriptor Formats */
struct __rte_packed_begin fsl_qdma_comp_cmd_desc {
	uint8_t status;
	uint32_t rsv0:22;
	uint32_t ser:1;
	uint32_t rsv1:21;
	uint32_t offset:9;
	uint32_t format:3;
	uint32_t addr_lo;
	uint8_t addr_hi;
	uint16_t rsv3;
	uint8_t queue:3;
	uint8_t rsv4:3;
	uint8_t dd:2;
} __rte_packed_end;

struct __rte_packed_begin fsl_qdma_comp_sg_desc {
	uint32_t offset:13;
	uint32_t rsv0:19;
	uint32_t length:30;
	uint32_t final:1;
	uint32_t extion:1;
	uint32_t addr_lo;
	uint8_t addr_hi;
	uint32_t rsv1:24;
} __rte_packed_end;

struct __rte_packed_begin fsl_qdma_sdf {
	uint32_t rsv0;
	uint32_t ssd:12;
	uint32_t sss:12;
	uint32_t rsv1:8;
	uint32_t rsv2;

	uint32_t rsv3:17;
	uint32_t prefetch:1;
	uint32_t rsv4:1;
	uint32_t ssen:1;
	uint32_t rthrotl:4;
	uint32_t sqos:3;
	uint32_t ns:1;
	uint32_t srttype:4;
} __rte_packed_end;

struct __rte_packed_begin fsl_qdma_ddf {
	uint32_t rsv0;
	uint32_t dsd:12;
	uint32_t dss:12;
	uint32_t rsv1:8;
	uint32_t rsv2;

	uint16_t rsv3;
	uint32_t lwc:2;
	uint32_t rsv4:1;
	uint32_t dsen:1;
	uint32_t wthrotl:4;
	uint32_t dqos:3;
	uint32_t ns:1;
	uint32_t dwttype:4;
} __rte_packed_end;

struct fsl_qdma_df {
	struct fsl_qdma_sdf sdf;
	struct fsl_qdma_ddf ddf;
};

#define FSL_QDMA_SG_MAX_ENTRY 64
#define FSL_QDMA_MAX_DESC_NUM (FSL_QDMA_SG_MAX_ENTRY * QDMA_QUEUE_SIZE)
struct __rte_packed_begin fsl_qdma_cmpd_ft {
	struct fsl_qdma_comp_sg_desc desc_buf;
	struct fsl_qdma_comp_sg_desc desc_sbuf;
	struct fsl_qdma_comp_sg_desc desc_dbuf;
	uint64_t cache_align[2];
	struct fsl_qdma_comp_sg_desc desc_ssge[FSL_QDMA_SG_MAX_ENTRY];
	struct fsl_qdma_comp_sg_desc desc_dsge[FSL_QDMA_SG_MAX_ENTRY];
	struct fsl_qdma_df df;
	uint64_t phy_ssge;
	uint64_t phy_dsge;
	uint64_t phy_df;
} __rte_packed_end;

#define FSL_QDMA_ERR_REG_STATUS_OFFSET 0xe00

struct fsl_qdma_dedr_reg {
	uint32_t me:1;
	uint32_t rsv0:1;
	uint32_t rte:1;
	uint32_t wte:1;
	uint32_t cde:1;
	uint32_t sde:1;
	uint32_t dde:1;
	uint32_t ere:1;
	uint32_t rsv1:24;
};

struct fsl_qdma_deccqidr_reg {
	uint32_t rsv:27;
	uint32_t block:2;
	uint32_t queue:3;
};

#define FSL_QDMA_DECCD_ERR_NUM \
	(sizeof(struct fsl_qdma_comp_cmd_desc) / sizeof(uint32_t))

struct fsl_qdma_err_reg {
	uint32_t deier;
	union {
		rte_be32_t dedr_be;
		struct fsl_qdma_dedr_reg dedr;
	};
	uint32_t rsv0[2];
	union {
		rte_le32_t deccd_le[FSL_QDMA_DECCD_ERR_NUM];
		struct fsl_qdma_comp_cmd_desc err_cmd;
	};
	uint32_t rsv1[4];
	union {
		rte_be32_t deccqidr_be;
		struct fsl_qdma_deccqidr_reg deccqidr;
	};
	rte_be32_t decbr;
};

#define DPAA_QDMA_IDXADDR_FROM_SG_FLAG(flag) \
	((void *)(uintptr_t)((flag) - ((flag) & DPAA_QDMA_SG_IDX_ADDR_MASK)))

#define DPAA_QDMA_IDX_FROM_FLAG(flag) \
	((flag) >> DPAA_QDMA_COPY_IDX_OFFSET)

struct fsl_qdma_desc {
	rte_iova_t src;
	rte_iova_t dst;
	uint64_t flag;
	uint64_t len;
};

struct fsl_qdma_queue {
	int used;
	struct fsl_qdma_cmpd_ft **ft;
	uint16_t ci;
	struct rte_ring *complete_burst;
	struct rte_ring *complete_desc;
	struct rte_ring *complete_pool;
	uint16_t n_cq;
	uint8_t block_id;
	uint8_t queue_id;
	uint8_t channel_id;
	void *block_vir;
	uint32_t le_cqmr;
	struct fsl_qdma_comp_cmd_desc *cq;
	uint16_t desc_in_hw[QDMA_QUEUE_SIZE];
	struct rte_dma_stats stats;
	struct fsl_qdma_desc *pending_desc;
	uint16_t pending_max;
	uint16_t pending_start;
	uint16_t pending_num;
	uint16_t complete_start;
	dma_addr_t bus_addr;
	void *engine;
};

struct fsl_qdma_status_queue {
	uint16_t n_cq;
	uint16_t complete;
	uint8_t block_id;
	void *block_vir;
	struct fsl_qdma_comp_cmd_desc *cq;
	struct rte_dma_stats stats;
	dma_addr_t bus_addr;
	void *engine;
};

struct fsl_qdma_engine {
	void *reg_base;
	void *ctrl_base;
	void *status_base;
	void *block_base;
	uint32_t n_queues;
	uint8_t block_queues[QDMA_BLOCKS];
	struct fsl_qdma_queue cmd_queues[QDMA_BLOCKS][QDMA_QUEUES];
	struct fsl_qdma_status_queue stat_queues[QDMA_BLOCKS];
	struct fsl_qdma_queue *chan[QDMA_BLOCKS * QDMA_QUEUES];
	uint32_t num_blocks;
	int block_offset;
	int is_silent;
};

#endif /* _DPAA_QDMA_H_ */
