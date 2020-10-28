/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) Intel Corporation
 */

/**
 * \file
 * I/OAT specification definitions
 *
 * Taken from ioat_spec.h from SPDK project, with prefix renames and
 * other minor changes.
 */

#ifndef RTE_IOAT_SPEC_H
#define RTE_IOAT_SPEC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define RTE_IOAT_PCI_CHANERR_INT_OFFSET	0x180

#define RTE_IOAT_INTRCTRL_MASTER_INT_EN	0x01

#define RTE_IOAT_VER_3_0                0x30
#define RTE_IOAT_VER_3_3                0x33

/* DMA Channel Registers */
#define RTE_IOAT_CHANCTRL_CHANNEL_PRIORITY_MASK	0xF000
#define RTE_IOAT_CHANCTRL_COMPL_DCA_EN		0x0200
#define RTE_IOAT_CHANCTRL_CHANNEL_IN_USE		0x0100
#define RTE_IOAT_CHANCTRL_DESCRIPTOR_ADDR_SNOOP_CONTROL	0x0020
#define RTE_IOAT_CHANCTRL_ERR_INT_EN		0x0010
#define RTE_IOAT_CHANCTRL_ANY_ERR_ABORT_EN		0x0008
#define RTE_IOAT_CHANCTRL_ERR_COMPLETION_EN		0x0004
#define RTE_IOAT_CHANCTRL_INT_REARM			0x0001

/* DMA Channel Capabilities */
#define	RTE_IOAT_DMACAP_PB		(1 << 0)
#define	RTE_IOAT_DMACAP_DCA		(1 << 4)
#define	RTE_IOAT_DMACAP_BFILL		(1 << 6)
#define	RTE_IOAT_DMACAP_XOR		(1 << 8)
#define	RTE_IOAT_DMACAP_PQ		(1 << 9)
#define	RTE_IOAT_DMACAP_DMA_DIF	(1 << 10)

struct rte_ioat_registers {
	uint8_t		chancnt;
	uint8_t		xfercap;
	uint8_t		genctrl;
	uint8_t		intrctrl;
	uint32_t	attnstatus;
	uint8_t		cbver;		/* 0x08 */
	uint8_t		reserved4[0x3]; /* 0x09 */
	uint16_t	intrdelay;	/* 0x0C */
	uint16_t	cs_status;	/* 0x0E */
	uint32_t	dmacapability;	/* 0x10 */
	uint8_t		reserved5[0x6C]; /* 0x14 */
	uint16_t	chanctrl;	/* 0x80 */
	uint8_t		reserved6[0x2];	/* 0x82 */
	uint8_t		chancmd;	/* 0x84 */
	uint8_t		reserved3[1];	/* 0x85 */
	uint16_t	dmacount;	/* 0x86 */
	uint64_t	chansts;	/* 0x88 */
	uint64_t	chainaddr;	/* 0x90 */
	uint64_t	chancmp;	/* 0x98 */
	uint8_t		reserved2[0x8];	/* 0xA0 */
	uint32_t	chanerr;	/* 0xA8 */
	uint32_t	chanerrmask;	/* 0xAC */
} __rte_packed;

#define RTE_IOAT_CHANCMD_RESET			0x20
#define RTE_IOAT_CHANCMD_SUSPEND		0x04

#define RTE_IOAT_CHANSTS_STATUS		0x7ULL
#define RTE_IOAT_CHANSTS_ACTIVE		0x0
#define RTE_IOAT_CHANSTS_IDLE			0x1
#define RTE_IOAT_CHANSTS_SUSPENDED		0x2
#define RTE_IOAT_CHANSTS_HALTED		0x3
#define RTE_IOAT_CHANSTS_ARMED			0x4

#define RTE_IOAT_CHANSTS_UNAFFILIATED_ERROR	0x8ULL
#define RTE_IOAT_CHANSTS_SOFT_ERROR		0x10ULL

#define RTE_IOAT_CHANSTS_COMPLETED_DESCRIPTOR_MASK	(~0x3FULL)

#define RTE_IOAT_CHANCMP_ALIGN			8	/* CHANCMP address must be 64-bit aligned */

struct rte_ioat_dma_hw_desc {
	uint32_t size;
	union {
		uint32_t control_raw;
		struct {
			uint32_t int_enable: 1;
			uint32_t src_snoop_disable: 1;
			uint32_t dest_snoop_disable: 1;
			uint32_t completion_update: 1;
			uint32_t fence: 1;
			uint32_t null: 1;
			uint32_t src_page_break: 1;
			uint32_t dest_page_break: 1;
			uint32_t bundle: 1;
			uint32_t dest_dca: 1;
			uint32_t hint: 1;
			uint32_t reserved: 13;
#define RTE_IOAT_OP_COPY 0x00
			uint32_t op: 8;
		} control;
	} u;
	uint64_t src_addr;
	uint64_t dest_addr;
	uint64_t next;
	uint64_t reserved;
	uint64_t reserved2;
	uint64_t user1;
	uint64_t user2;
};

struct rte_ioat_fill_hw_desc {
	uint32_t size;
	union {
		uint32_t control_raw;
		struct {
			uint32_t int_enable: 1;
			uint32_t reserved: 1;
			uint32_t dest_snoop_disable: 1;
			uint32_t completion_update: 1;
			uint32_t fence: 1;
			uint32_t reserved2: 2;
			uint32_t dest_page_break: 1;
			uint32_t bundle: 1;
			uint32_t reserved3: 15;
#define RTE_IOAT_OP_FILL 0x01
			uint32_t op: 8;
		} control;
	} u;
	uint64_t src_data;
	uint64_t dest_addr;
	uint64_t next;
	uint64_t reserved;
	uint64_t next_dest_addr;
	uint64_t user1;
	uint64_t user2;
};

struct rte_ioat_xor_hw_desc {
	uint32_t size;
	union {
		uint32_t control_raw;
		struct {
			uint32_t int_enable: 1;
			uint32_t src_snoop_disable: 1;
			uint32_t dest_snoop_disable: 1;
			uint32_t completion_update: 1;
			uint32_t fence: 1;
			uint32_t src_count: 3;
			uint32_t bundle: 1;
			uint32_t dest_dca: 1;
			uint32_t hint: 1;
			uint32_t reserved: 13;
#define RTE_IOAT_OP_XOR 0x87
#define RTE_IOAT_OP_XOR_VAL 0x88
			uint32_t op: 8;
		} control;
	} u;
	uint64_t src_addr;
	uint64_t dest_addr;
	uint64_t next;
	uint64_t src_addr2;
	uint64_t src_addr3;
	uint64_t src_addr4;
	uint64_t src_addr5;
};

struct rte_ioat_xor_ext_hw_desc {
	uint64_t src_addr6;
	uint64_t src_addr7;
	uint64_t src_addr8;
	uint64_t next;
	uint64_t reserved[4];
};

struct rte_ioat_pq_hw_desc {
	uint32_t size;
	union {
		uint32_t control_raw;
		struct {
			uint32_t int_enable: 1;
			uint32_t src_snoop_disable: 1;
			uint32_t dest_snoop_disable: 1;
			uint32_t completion_update: 1;
			uint32_t fence: 1;
			uint32_t src_count: 3;
			uint32_t bundle: 1;
			uint32_t dest_dca: 1;
			uint32_t hint: 1;
			uint32_t p_disable: 1;
			uint32_t q_disable: 1;
			uint32_t reserved: 11;
#define RTE_IOAT_OP_PQ 0x89
#define RTE_IOAT_OP_PQ_VAL 0x8a
			uint32_t op: 8;
		} control;
	} u;
	uint64_t src_addr;
	uint64_t p_addr;
	uint64_t next;
	uint64_t src_addr2;
	uint64_t src_addr3;
	uint8_t  coef[8];
	uint64_t q_addr;
};

struct rte_ioat_pq_ext_hw_desc {
	uint64_t src_addr4;
	uint64_t src_addr5;
	uint64_t src_addr6;
	uint64_t next;
	uint64_t src_addr7;
	uint64_t src_addr8;
	uint64_t reserved[2];
};

struct rte_ioat_pq_update_hw_desc {
	uint32_t size;
	union {
		uint32_t control_raw;
		struct {
			uint32_t int_enable: 1;
			uint32_t src_snoop_disable: 1;
			uint32_t dest_snoop_disable: 1;
			uint32_t completion_update: 1;
			uint32_t fence: 1;
			uint32_t src_cnt: 3;
			uint32_t bundle: 1;
			uint32_t dest_dca: 1;
			uint32_t hint: 1;
			uint32_t p_disable: 1;
			uint32_t q_disable: 1;
			uint32_t reserved: 3;
			uint32_t coef: 8;
#define RTE_IOAT_OP_PQ_UP 0x8b
			uint32_t op: 8;
		} control;
	} u;
	uint64_t src_addr;
	uint64_t p_addr;
	uint64_t next;
	uint64_t src_addr2;
	uint64_t p_src;
	uint64_t q_src;
	uint64_t q_addr;
};

struct rte_ioat_raw_hw_desc {
	uint64_t field[8];
};

union rte_ioat_hw_desc {
	struct rte_ioat_raw_hw_desc raw;
	struct rte_ioat_generic_hw_desc generic;
	struct rte_ioat_dma_hw_desc dma;
	struct rte_ioat_fill_hw_desc fill;
	struct rte_ioat_xor_hw_desc xor_desc;
	struct rte_ioat_xor_ext_hw_desc xor_ext;
	struct rte_ioat_pq_hw_desc pq;
	struct rte_ioat_pq_ext_hw_desc pq_ext;
	struct rte_ioat_pq_update_hw_desc pq_update;
};

/*** Definitions for Intel(R) Data Streaming Accelerator Follow ***/

#define IDXD_CMD_SHIFT 20
enum rte_idxd_cmds {
	idxd_enable_dev = 1,
	idxd_disable_dev,
	idxd_drain_all,
	idxd_abort_all,
	idxd_reset_device,
	idxd_enable_wq,
	idxd_disable_wq,
	idxd_drain_wq,
	idxd_abort_wq,
	idxd_reset_wq,
};

/* General bar0 registers */
struct rte_idxd_bar0 {
	uint32_t __rte_cache_aligned version;    /* offset 0x00 */
	uint64_t __rte_aligned(0x10) gencap;     /* offset 0x10 */
	uint64_t __rte_aligned(0x10) wqcap;      /* offset 0x20 */
	uint64_t __rte_aligned(0x10) grpcap;     /* offset 0x30 */
	uint64_t __rte_aligned(0x08) engcap;     /* offset 0x38 */
	uint64_t __rte_aligned(0x10) opcap;      /* offset 0x40 */
	uint64_t __rte_aligned(0x20) offsets[2]; /* offset 0x60 */
	uint32_t __rte_aligned(0x20) gencfg;     /* offset 0x80 */
	uint32_t __rte_aligned(0x08) genctrl;    /* offset 0x88 */
	uint32_t __rte_aligned(0x10) gensts;     /* offset 0x90 */
	uint32_t __rte_aligned(0x08) intcause;   /* offset 0x98 */
	uint32_t __rte_aligned(0x10) cmd;        /* offset 0xA0 */
	uint32_t __rte_aligned(0x08) cmdstatus;  /* offset 0xA8 */
	uint64_t __rte_aligned(0x20) swerror[4]; /* offset 0xC0 */
};

/* workqueue config is provided by array of uint32_t. */
#define WQ_SIZE_IDX      0 /* size is in first 32-bit value */
#define WQ_THRESHOLD_IDX 1 /* WQ threshold second 32-bits */
#define WQ_MODE_IDX      2 /* WQ mode and other flags */
#define WQ_SIZES_IDX     3 /* WQ transfer and batch sizes */
#define WQ_OCC_INT_IDX   4 /* WQ occupancy interrupt handle */
#define WQ_OCC_LIMIT_IDX 5 /* WQ occupancy limit */
#define WQ_STATE_IDX     6 /* WQ state and occupancy state */

#define WQ_MODE_SHARED    0
#define WQ_MODE_DEDICATED 1
#define WQ_PRIORITY_SHIFT 4
#define WQ_BATCH_SZ_SHIFT 5
#define WQ_STATE_SHIFT 30
#define WQ_STATE_MASK 0x3

struct rte_idxd_grpcfg {
	uint64_t grpwqcfg[4]  __rte_cache_aligned; /* 64-byte register set */
	uint64_t grpengcfg;  /* offset 32 */
	uint32_t grpflags;   /* offset 40 */
};

#define GENSTS_DEV_STATE_MASK 0x03
#define CMDSTATUS_ACTIVE_SHIFT 31
#define CMDSTATUS_ACTIVE_MASK (1 << 31)
#define CMDSTATUS_ERR_MASK 0xFF

#ifdef __cplusplus
}
#endif

#endif /* RTE_IOAT_SPEC_H */
