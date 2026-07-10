/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2026 Advanced Micro Devices, Inc. All rights reserved.
 */

#ifndef AE4DMA_HW_DEFS_H
#define AE4DMA_HW_DEFS_H

#include <stdint.h>

#include <rte_bitops.h>

/* ae4dma device details */
#define AMD_VENDOR_ID	0x1022
#define AE4DMA_DEVICE_ID	0x149b
#define AE4DMA_PCIE_BAR 0

/*
 * An AE4DMA engine has 16 DMA queues. Each queue supports up to 32
 * descriptors (31 outstanding).
 */
#define AE4DMA_MAX_HW_QUEUES        16
#define AE4DMA_QUEUE_START_INDEX    0
#define AE4DMA_CMD_QUEUE_ENABLE		0x1
#define AE4DMA_CMD_QUEUE_DISABLE	0x0

/* Common to all queues */
#define AE4DMA_COMMON_CONFIG_OFFSET 0x00

#define AE4DMA_DISABLE_INTR 0x01

/* Descriptor status */
enum ae4dma_dma_status {
	AE4DMA_DMA_DESC_SUBMITTED = 0,
	AE4DMA_DMA_DESC_VALIDATED = 1,
	AE4DMA_DMA_DESC_PROCESSED = 2,
	AE4DMA_DMA_DESC_COMPLETED = 3,
	AE4DMA_DMA_DESC_ERROR = 4,
};

/* Descriptor error-code */
enum ae4dma_dma_err {
	AE4DMA_DMA_ERR_NO_ERR = 0,
	AE4DMA_DMA_ERR_INV_HEADER = 1,
	AE4DMA_DMA_ERR_INV_STATUS = 2,
	AE4DMA_DMA_ERR_INV_LEN = 3,
	AE4DMA_DMA_ERR_INV_SRC = 4,
	AE4DMA_DMA_ERR_INV_DST = 5,
	AE4DMA_DMA_ERR_INV_ALIGN = 6,
	AE4DMA_DMA_ERR_UNKNOWN = 7,
};

/* HW Queue status */
enum ae4dma_hwqueue_status {
	AE4DMA_HWQUEUE_EMPTY = 0,
	AE4DMA_HWQUEUE_FULL = 1,
	AE4DMA_HWQUEUE_NOT_EMPTY = 4,
};
/*
 * descriptor for AE4DMA commands
 * 8 32-bit words:
 * word 0: source memory type; destination memory type ; control bits
 * word 1: desc_id; error code; status
 * word 2: length
 * word 3: reserved
 * word 4: upper 32 bits of source pointer
 * word 5: low 32 bits of source pointer
 * word 6: upper 32 bits of destination pointer
 * word 7: low 32 bits of destination pointer
 */

/* AE4DMA Descriptor - DWORD0 - Controls bits: Reserved for future use */
#define AE4DMA_DWORD0_STOP_ON_COMPLETION	RTE_BIT32(0)
#define AE4DMA_DWORD0_INTERRUPT_ON_COMPLETION	RTE_BIT32(1)
#define AE4DMA_DWORD0_START_OF_MESSAGE		RTE_BIT32(3)
#define AE4DMA_DWORD0_END_OF_MESSAGE		RTE_BIT32(4)
#define AE4DMA_DWORD0_DESTINATION_MEMORY_TYPE	RTE_GENMASK64(5, 4)
#define AE4DMA_DWORD0_SOURCE_MEMEORY_TYPE	RTE_GENMASK64(7, 6)

#define AE4DMA_DWORD0_DESTINATION_MEMORY_TYPE_MEMORY    (0x0)
#define AE4DMA_DWORD0_DESTINATION_MEMORY_TYPE_IOMEMORY  RTE_BIT32(4)
#define AE4DMA_DWORD0_SOURCE_MEMEORY_TYPE_MEMORY    (0x0)
#define AE4DMA_DWORD0_SOURCE_MEMEORY_TYPE_IOMEMORY  RTE_BIT32(6)

struct ae4dma_desc_dword0 {
	uint8_t byte0;
	uint8_t byte1;
	uint16_t timestamp;
};

struct ae4dma_desc_dword1 {
	uint8_t status;
	uint8_t err_code;
	uint16_t desc_id;
};

struct ae4dma_desc {
	struct ae4dma_desc_dword0 dw0;
	struct ae4dma_desc_dword1 dw1;
	uint32_t length;
	uint32_t reserved;
	uint32_t src_lo;
	uint32_t src_hi;
	uint32_t dst_lo;
	uint32_t dst_hi;
};

/*
 * Registers for each queue :4 bytes length
 * Effective address : offset + reg
 */
struct ae4dma_hwq_regs {
	union {
		uint32_t control_raw;
		struct {
			uint32_t queue_enable: 1;
			uint32_t reserved_internal: 31;
		} control;
	} control_reg;

	union {
		uint32_t status_raw;
		struct {
			uint32_t reserved0: 1;
			/* 0–empty, 1–full, 2–stopped, 3–error , 4–Not Empty */
			uint32_t queue_status: 2;
			uint32_t reserved1: 21;
			uint32_t interrupt_type: 4;
			uint32_t reserved2: 4;
		} status;
	} status_reg;

	uint32_t max_idx;
	uint32_t read_idx;
	uint32_t write_idx;

	union {
		uint32_t intr_status_raw;
		struct {
			uint32_t intr_status: 1;
			uint32_t reserved: 31;
		} intr_status;
	} intr_status_reg;

	uint32_t qbase_lo;
	uint32_t qbase_hi;

};

#endif /* AE4DMA_HW_DEFS_H */
