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

#endif
