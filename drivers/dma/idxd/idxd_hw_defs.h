/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021 Intel Corporation
 */

#ifndef _IDXD_HW_DEFS_H_
#define _IDXD_HW_DEFS_H_

#define IDXD_COMP_STATUS_INCOMPLETE        0
#define IDXD_COMP_STATUS_SUCCESS           1
#define IDXD_COMP_STATUS_INVALID_OPCODE 0x10
#define IDXD_COMP_STATUS_INVALID_SIZE   0x13
#define IDXD_COMP_STATUS_SKIPPED        0xFF /* not official IDXD error, needed as placeholder */

/**
 * Completion record structure written back by DSA
 */
struct idxd_completion {
	uint8_t status;
	uint8_t result;
	/* 16-bits pad here */
	uint32_t completed_size; /* data length, or descriptors for batch */

	rte_iova_t fault_address;
	uint32_t invalid_flags;
} __rte_aligned(32);

/*** Definitions for Intel(R) Data Streaming Accelerator  ***/

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
enum rte_idxd_wqcfg {
	wq_size_idx,       /* size is in first 32-bit value */
	wq_threshold_idx,  /* WQ threshold second 32-bits */
	wq_mode_idx,       /* WQ mode and other flags */
	wq_sizes_idx,      /* WQ transfer and batch sizes */
	wq_occ_int_idx,    /* WQ occupancy interrupt handle */
	wq_occ_limit_idx,  /* WQ occupancy limit */
	wq_state_idx,      /* WQ state and occupancy state */
};

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

#endif
