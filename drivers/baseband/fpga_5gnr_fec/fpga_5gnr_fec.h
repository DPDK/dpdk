/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#ifndef _FPGA_5GNR_FEC_H_
#define _FPGA_5GNR_FEC_H_

#include <stdint.h>
#include <stdbool.h>

#include "agx100_pmd.h"
#include "vc_5gnr_pmd.h"

extern int fpga_5gnr_fec_logtype;
#define RTE_LOGTYPE_FPGA_5GNR_FEC fpga_5gnr_fec_logtype

/* Helper macro for logging */
#define rte_bbdev_log(level, ...) \
	RTE_LOG_LINE(level, FPGA_5GNR_FEC, __VA_ARGS__)

#ifdef RTE_LIBRTE_BBDEV_DEBUG
#define rte_bbdev_log_debug(...) \
	rte_bbdev_log(DEBUG, __VA_ARGS__)
#else
#define rte_bbdev_log_debug(...)
#endif

/* FPGA 5GNR FEC driver names */
#define FPGA_5GNR_FEC_PF_DRIVER_NAME intel_fpga_5gnr_fec_pf
#define FPGA_5GNR_FEC_VF_DRIVER_NAME intel_fpga_5gnr_fec_vf

#define FPGA_5GNR_INVALID_HW_QUEUE_ID (0xFFFFFFFF)
#define FPGA_5GNR_QUEUE_FLUSH_TIMEOUT_US (1000)
#define FPGA_5GNR_HARQ_RDY_TIMEOUT (10)
#define FPGA_5GNR_TIMEOUT_CHECK_INTERVAL (5)
#define FPGA_5GNR_DDR_OVERFLOW (0x10)
#define FPGA_5GNR_DDR_WR_DATA_LEN_IN_BYTES 8
#define FPGA_5GNR_DDR_RD_DATA_LEN_IN_BYTES 8
/* Align DMA descriptors to 256 bytes - cache-aligned. */
#define FPGA_5GNR_RING_DESC_ENTRY_LENGTH (8)
/* Maximum size of queue. */
#define FPGA_5GNR_RING_MAX_SIZE (1024)

#define VC_5GNR_FPGA_VARIANT	0
#define AGX100_FPGA_VARIANT	1

/* Constants from K0 computation from 3GPP 38.212 Table 5.4.2.1-2 */
#define N_ZC_1 66 /* N = 66 Zc for BG 1 */
#define N_ZC_2 50 /* N = 50 Zc for BG 2 */
#define K0_1_1 17 /* K0 fraction numerator for rv 1 and BG 1 */
#define K0_1_2 13 /* K0 fraction numerator for rv 1 and BG 2 */
#define K0_2_1 33 /* K0 fraction numerator for rv 2 and BG 1 */
#define K0_2_2 25 /* K0 fraction numerator for rv 2 and BG 2 */
#define K0_3_1 56 /* K0 fraction numerator for rv 3 and BG 1 */
#define K0_3_2 43 /* K0 fraction numerator for rv 3 and BG 2 */

/* FPGA 5GNR Ring Control Registers. */
enum {
	FPGA_5GNR_FEC_RING_HEAD_ADDR = 0x00000008,
	FPGA_5GNR_FEC_RING_SIZE = 0x00000010,
	FPGA_5GNR_FEC_RING_MISC = 0x00000014,
	FPGA_5GNR_FEC_RING_ENABLE = 0x00000015,
	FPGA_5GNR_FEC_RING_FLUSH_QUEUE_EN = 0x00000016,
	FPGA_5GNR_FEC_RING_SHADOW_TAIL = 0x00000018,
	FPGA_5GNR_FEC_RING_HEAD_POINT = 0x0000001C
};

/* VC 5GNR and AGX100 common register mapping on BAR0. */
enum {
	FPGA_5GNR_FEC_VERSION_ID = 0x00000000, /**< len: 4B. */
	FPGA_5GNR_FEC_QUEUE_PF_VF_MAP_DONE = 0x00000008, /**< len: 1B. */
	FPGA_5GNR_FEC_LOAD_BALANCE_FACTOR = 0x0000000A, /**< len: 2B. */
	FPGA_5GNR_FEC_RING_DESC_LEN = 0x0000000C, /**< len: 2B. */
	FPGA_5GNR_FEC_VFQ_FLUSH_STATUS_LW = 0x00000018, /**< len: 4B. */
	FPGA_5GNR_FEC_VFQ_FLUSH_STATUS_HI = 0x0000001C, /**< len: 4B. */
	FPGA_5GNR_FEC_RING_CTRL_REGS = 0x00000200, /**< len: 2048B. */
	FPGA_5GNR_FEC_DDR4_WR_ADDR_REGS = 0x00000A00, /**< len: 4B. */
	FPGA_5GNR_FEC_DDR4_WR_DATA_REGS = 0x00000A08, /**< len: 8B. */
	FPGA_5GNR_FEC_DDR4_WR_DONE_REGS = 0x00000A10, /**< len: 1B. */
	FPGA_5GNR_FEC_DDR4_RD_ADDR_REGS = 0x00000A18, /**< len: 4B. */
	FPGA_5GNR_FEC_DDR4_RD_DONE_REGS = 0x00000A20, /**< len: 1B. */
	FPGA_5GNR_FEC_DDR4_RD_RDY_REGS = 0x00000A28, /**< len: 1B. */
	FPGA_5GNR_FEC_DDR4_RD_DATA_REGS = 0x00000A30, /**< len: 8B. */
	FPGA_5GNR_FEC_DDR4_ADDR_RDY_REGS = 0x00000A38, /**< len: 1B. */
	FPGA_5GNR_FEC_HARQ_BUF_SIZE_RDY_REGS = 0x00000A40, /**< len: 1B. */
	FPGA_5GNR_FEC_HARQ_BUF_SIZE_REGS = 0x00000A48, /**< len: 4B. */
	FPGA_5GNR_FEC_MUTEX = 0x00000A60, /**< len: 4B. */
	FPGA_5GNR_FEC_MUTEX_RESET = 0x00000A68  /**< len: 4B. */
};

/* FPGA 5GNR Ring Control Register. */
struct __rte_packed_begin fpga_5gnr_ring_ctrl_reg {
	uint64_t ring_base_addr;
	uint64_t ring_head_addr;
	uint16_t ring_size:11;
	uint16_t rsrvd0;
	union { /* Miscellaneous register */
		uint8_t misc;
		uint8_t max_ul_dec:5,
			max_ul_dec_en:1,
			rsrvd1:2;
	};
	uint8_t enable;
	uint8_t flush_queue_en;
	uint8_t rsrvd2;
	uint16_t shadow_tail;
	uint16_t rsrvd3;
	uint16_t head_point;
	uint16_t rsrvd4;
} __rte_packed_end;

/* Private data structure for each FPGA 5GNR device. */
struct fpga_5gnr_fec_device {
	/** Base address of MMIO registers (BAR0). */
	void *mmio_base;
	/** Base address of memory for sw rings. */
	void *sw_rings;
	/** Physical address of sw_rings. */
	rte_iova_t sw_rings_phys;
	/** Number of bytes available for each queue in device. */
	uint32_t sw_ring_size;
	/** Max number of entries available for each queue in device. */
	uint32_t sw_ring_max_depth;
	/** Base address of response tail pointer buffer. */
	uint32_t *tail_ptrs;
	/** Physical address of tail pointers. */
	rte_iova_t tail_ptr_phys;
	/** Queues flush completion flag. */
	uint64_t *flush_queue_status;
	/** Bitmap capturing which Queues are bound to the PF/VF. */
	uint64_t q_bound_bit_map;
	/** Bitmap capturing which Queues have already been assigned. */
	uint64_t q_assigned_bit_map;
	/** True if this is a PF FPGA 5GNR device. */
	bool pf_device;
	/** Maximum number of possible queues for this device. */
	uint8_t total_num_queues;
	/** FPGA Variant. VC_5GNR_FPGA_VARIANT = 0; AGX100_FPGA_VARIANT = 1. */
	uint8_t fpga_variant;
};

/** Structure associated with each queue. */
struct __rte_cache_aligned fpga_5gnr_queue {
	struct fpga_5gnr_ring_ctrl_reg ring_ctrl_reg;  /**< Ring Control Register */
	union {
		/** Virtual address of VC 5GNR software ring. */
		union vc_5gnr_dma_desc *vc_5gnr_ring_addr;
		/** Virtual address of AGX100 software ring. */
		union agx100_dma_desc *agx100_ring_addr;
	};
	uint64_t *ring_head_addr;  /* Virtual address of completion_head */
	uint64_t shadow_completion_head; /* Shadow completion head value */
	uint16_t head_free_desc;  /* Ring head */
	uint16_t tail;  /* Ring tail */
	/* Mask used to wrap enqueued descriptors on the sw ring */
	uint32_t sw_ring_wrap_mask;
	uint32_t irq_enable;  /* Enable ops dequeue interrupts if set to 1 */
	uint8_t q_idx;  /* Queue index */
	/** uuid used for MUTEX acquision for DDR */
	uint16_t ddr_mutex_uuid;
	struct fpga_5gnr_fec_device *d;
	/* MMIO register of shadow_tail used to enqueue descriptors */
	void *shadow_tail_addr;
};

/* Write to 16 bit MMIO register address. */
static inline void
mmio_write_16(void *addr, uint16_t value)
{
	*((volatile uint16_t *)(addr)) = rte_cpu_to_le_16(value);
}

/* Write to 32 bit MMIO register address. */
static inline void
mmio_write_32(void *addr, uint32_t value)
{
	*((volatile uint32_t *)(addr)) = rte_cpu_to_le_32(value);
}

/* Write to 64 bit MMIO register address. */
static inline void
mmio_write_64(void *addr, uint64_t value)
{
	*((volatile uint64_t *)(addr)) = rte_cpu_to_le_64(value);
}

/* Write a 8 bit register of a FPGA 5GNR device. */
static inline void
fpga_5gnr_reg_write_8(void *mmio_base, uint32_t offset, uint8_t payload)
{
	void *reg_addr = RTE_PTR_ADD(mmio_base, offset);
	*((volatile uint8_t *)(reg_addr)) = payload;
}

/* Write a 16 bit register of a FPGA 5GNR device. */
static inline void
fpga_5gnr_reg_write_16(void *mmio_base, uint32_t offset, uint16_t payload)
{
	void *reg_addr = RTE_PTR_ADD(mmio_base, offset);
	mmio_write_16(reg_addr, payload);
}

/* Write a 32 bit register of a FPGA 5GNR device. */
static inline void
fpga_5gnr_reg_write_32(void *mmio_base, uint32_t offset, uint32_t payload)
{
	void *reg_addr = RTE_PTR_ADD(mmio_base, offset);
	mmio_write_32(reg_addr, payload);
}

/* Write a 64 bit register of a FPGA 5GNR device. */
static inline void
fpga_5gnr_reg_write_64(void *mmio_base, uint32_t offset, uint64_t payload)
{
	void *reg_addr = RTE_PTR_ADD(mmio_base, offset);
	mmio_write_64(reg_addr, payload);
}

/* Write a ring control register of a FPGA 5GNR device. */
static inline void
fpga_ring_reg_write(void *mmio_base, uint32_t offset, struct fpga_5gnr_ring_ctrl_reg payload)
{
	fpga_5gnr_reg_write_64(mmio_base, offset, payload.ring_base_addr);
	fpga_5gnr_reg_write_64(mmio_base, offset + FPGA_5GNR_FEC_RING_HEAD_ADDR,
			payload.ring_head_addr);
	fpga_5gnr_reg_write_16(mmio_base, offset + FPGA_5GNR_FEC_RING_SIZE, payload.ring_size);
	fpga_5gnr_reg_write_16(mmio_base, offset + FPGA_5GNR_FEC_RING_HEAD_POINT,
			payload.head_point);
	fpga_5gnr_reg_write_8(mmio_base, offset + FPGA_5GNR_FEC_RING_FLUSH_QUEUE_EN,
			payload.flush_queue_en);
	fpga_5gnr_reg_write_16(mmio_base, offset + FPGA_5GNR_FEC_RING_SHADOW_TAIL,
			payload.shadow_tail);
	fpga_5gnr_reg_write_8(mmio_base, offset + FPGA_5GNR_FEC_RING_MISC, payload.misc);
	fpga_5gnr_reg_write_8(mmio_base, offset + FPGA_5GNR_FEC_RING_ENABLE, payload.enable);
}

/* Read a register of FPGA 5GNR device. */
static inline uint32_t
fpga_5gnr_reg_read_32(void *mmio_base, uint32_t offset)
{
	void *reg_addr = RTE_PTR_ADD(mmio_base, offset);
	uint32_t ret = *((volatile uint32_t *)(reg_addr));
	return rte_le_to_cpu_32(ret);
}

#ifdef RTE_LIBRTE_BBDEV_DEBUG

/* Read a register of FPGA 5GNR device. */
static inline uint16_t
fpga_5gnr_reg_read_16(void *mmio_base, uint32_t offset)
{
	void *reg_addr = RTE_PTR_ADD(mmio_base, offset);
	uint16_t ret = *((volatile uint16_t *)(reg_addr));
	return rte_le_to_cpu_16(ret);
}

#endif

/* Read a register of FPGA 5GNR device. */
static inline uint8_t
fpga_5gnr_reg_read_8(void *mmio_base, uint32_t offset)
{
	void *reg_addr = RTE_PTR_ADD(mmio_base, offset);
	return *((volatile uint8_t *)(reg_addr));
}

/* Read a register of FPGA 5GNR device. */
static inline uint64_t
fpga_5gnr_reg_read_64(void *mmio_base, uint32_t offset)
{
	void *reg_addr = RTE_PTR_ADD(mmio_base, offset);
	uint64_t ret = *((volatile uint64_t *)(reg_addr));
	return rte_le_to_cpu_64(ret);
}

#endif /* _FPGA_5GNR_FEC_H_ */
