/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#ifndef _FPGA_5GNR_FEC_H_
#define _FPGA_5GNR_FEC_H_

#include <stdint.h>
#include <stdbool.h>

/* Helper macro for logging */
#define rte_bbdev_log(level, fmt, ...) \
	rte_log(RTE_LOG_ ## level, fpga_5gnr_fec_logtype, fmt "\n", \
		##__VA_ARGS__)

#ifdef RTE_LIBRTE_BBDEV_DEBUG
#define rte_bbdev_log_debug(fmt, ...) \
		rte_bbdev_log(DEBUG, "fpga_5gnr_fec: " fmt, \
		##__VA_ARGS__)
#else
#define rte_bbdev_log_debug(fmt, ...)
#endif

/* FPGA 5GNR FEC driver names */
#define FPGA_5GNR_FEC_PF_DRIVER_NAME intel_fpga_5gnr_fec_pf
#define FPGA_5GNR_FEC_VF_DRIVER_NAME intel_fpga_5gnr_fec_vf

/* FPGA 5GNR FEC PCI vendor & device IDs */
#define FPGA_5GNR_FEC_VENDOR_ID (0x8086)
#define FPGA_5GNR_FEC_PF_DEVICE_ID (0x0D8F)
#define FPGA_5GNR_FEC_VF_DEVICE_ID (0x0D90)

/* Align DMA descriptors to 256 bytes - cache-aligned */
#define FPGA_RING_DESC_ENTRY_LENGTH (8)
/* Ring size is in 256 bits (32 bytes) units */
#define FPGA_RING_DESC_LEN_UNIT_BYTES (32)
/* Maximum size of queue */
#define FPGA_RING_MAX_SIZE (1024)
#define FPGA_FLR_TIMEOUT_UNIT (16.384)

#define FPGA_NUM_UL_QUEUES (32)
#define FPGA_NUM_DL_QUEUES (32)
#define FPGA_TOTAL_NUM_QUEUES (FPGA_NUM_UL_QUEUES + FPGA_NUM_DL_QUEUES)
#define FPGA_NUM_INTR_VEC (FPGA_TOTAL_NUM_QUEUES - RTE_INTR_VEC_RXTX_OFFSET)

#define FPGA_INVALID_HW_QUEUE_ID (0xFFFFFFFF)

#define FPGA_QUEUE_FLUSH_TIMEOUT_US (1000)
#define FPGA_HARQ_RDY_TIMEOUT (10)
#define FPGA_TIMEOUT_CHECK_INTERVAL (5)
#define FPGA_DDR_OVERFLOW (0x10)

#define FPGA_5GNR_FEC_DDR_WR_DATA_LEN_IN_BYTES 8
#define FPGA_5GNR_FEC_DDR_RD_DATA_LEN_IN_BYTES 8

/* Constants from K0 computation from 3GPP 38.212 Table 5.4.2.1-2 */
#define N_ZC_1 66 /* N = 66 Zc for BG 1 */
#define N_ZC_2 50 /* N = 50 Zc for BG 2 */
#define K0_1_1 17 /* K0 fraction numerator for rv 1 and BG 1 */
#define K0_1_2 13 /* K0 fraction numerator for rv 1 and BG 2 */
#define K0_2_1 33 /* K0 fraction numerator for rv 2 and BG 1 */
#define K0_2_2 25 /* K0 fraction numerator for rv 2 and BG 2 */
#define K0_3_1 56 /* K0 fraction numerator for rv 3 and BG 1 */
#define K0_3_2 43 /* K0 fraction numerator for rv 3 and BG 2 */

/* FPGA 5GNR FEC Register mapping on BAR0 */
enum {
	FPGA_5GNR_FEC_VERSION_ID = 0x00000000, /* len: 4B */
	FPGA_5GNR_FEC_CONFIGURATION = 0x00000004, /* len: 2B */
	FPGA_5GNR_FEC_QUEUE_PF_VF_MAP_DONE = 0x00000008, /* len: 1B */
	FPGA_5GNR_FEC_LOAD_BALANCE_FACTOR = 0x0000000a, /* len: 2B */
	FPGA_5GNR_FEC_RING_DESC_LEN = 0x0000000c, /* len: 2B */
	FPGA_5GNR_FEC_FLR_TIME_OUT = 0x0000000e, /* len: 2B */
	FPGA_5GNR_FEC_VFQ_FLUSH_STATUS_LW = 0x00000018, /* len: 4B */
	FPGA_5GNR_FEC_VFQ_FLUSH_STATUS_HI = 0x0000001c, /* len: 4B */
	FPGA_5GNR_FEC_QUEUE_MAP = 0x00000040, /* len: 256B */
	FPGA_5GNR_FEC_RING_CTRL_REGS = 0x00000200, /* len: 2048B */
	FPGA_5GNR_FEC_DDR4_WR_ADDR_REGS = 0x00000A00, /* len: 4B */
	FPGA_5GNR_FEC_DDR4_WR_DATA_REGS = 0x00000A08, /* len: 8B */
	FPGA_5GNR_FEC_DDR4_WR_DONE_REGS = 0x00000A10, /* len: 1B */
	FPGA_5GNR_FEC_DDR4_RD_ADDR_REGS = 0x00000A18, /* len: 4B */
	FPGA_5GNR_FEC_DDR4_RD_DONE_REGS = 0x00000A20, /* len: 1B */
	FPGA_5GNR_FEC_DDR4_RD_RDY_REGS = 0x00000A28, /* len: 1B */
	FPGA_5GNR_FEC_DDR4_RD_DATA_REGS = 0x00000A30, /* len: 8B */
	FPGA_5GNR_FEC_DDR4_ADDR_RDY_REGS = 0x00000A38, /* len: 1B */
	FPGA_5GNR_FEC_HARQ_BUF_SIZE_RDY_REGS = 0x00000A40, /* len: 1B */
	FPGA_5GNR_FEC_HARQ_BUF_SIZE_REGS = 0x00000A48  /* len: 4B */
};

/* FPGA 5GNR FEC Ring Control Registers */
enum {
	FPGA_5GNR_FEC_RING_HEAD_ADDR = 0x00000008,
	FPGA_5GNR_FEC_RING_SIZE = 0x00000010,
	FPGA_5GNR_FEC_RING_MISC = 0x00000014,
	FPGA_5GNR_FEC_RING_ENABLE = 0x00000015,
	FPGA_5GNR_FEC_RING_FLUSH_QUEUE_EN = 0x00000016,
	FPGA_5GNR_FEC_RING_SHADOW_TAIL = 0x00000018,
	FPGA_5GNR_FEC_RING_HEAD_POINT = 0x0000001C
};

/* FPGA 5GNR FEC DESCRIPTOR ERROR */
enum {
	DESC_ERR_NO_ERR = 0x0,
	DESC_ERR_K_P_OUT_OF_RANGE = 0x1,
	DESC_ERR_Z_C_NOT_LEGAL = 0x2,
	DESC_ERR_DESC_OFFSET_ERR = 0x3,
	DESC_ERR_DESC_READ_FAIL = 0x8,
	DESC_ERR_DESC_READ_TIMEOUT = 0x9,
	DESC_ERR_DESC_READ_TLP_POISONED = 0xA,
	DESC_ERR_CB_READ_FAIL = 0xC,
	DESC_ERR_CB_READ_TIMEOUT = 0xD,
	DESC_ERR_CB_READ_TLP_POISONED = 0xE,
	DESC_ERR_HBSTORE_ERR = 0xF
};


/* FPGA 5GNR FEC DMA Encoding Request Descriptor */
struct __rte_packed fpga_dma_enc_desc {
	uint32_t done:1,
		rsrvd0:7,
		error:4,
		rsrvd1:4,
		num_null:10,
		rsrvd2:6;
	uint32_t ncb:15,
		rsrvd3:1,
		k0:16;
	uint32_t irq_en:1,
		crc_en:1,
		rsrvd4:1,
		qm_idx:3,
		bg_idx:1,
		zc:9,
		desc_idx:10,
		rsrvd5:6;
	uint16_t rm_e;
	uint16_t k_;
	uint32_t out_addr_lw;
	uint32_t out_addr_hi;
	uint32_t in_addr_lw;
	uint32_t in_addr_hi;

	union {
		struct {
			/* Virtual addresses used to retrieve SW context info */
			void *op_addr;
			/* Stores information about total number of Code Blocks
			 * in currently processed Transport Block
			 */
			uint64_t cbs_in_op;
		};

		uint8_t sw_ctxt[FPGA_RING_DESC_LEN_UNIT_BYTES *
					(FPGA_RING_DESC_ENTRY_LENGTH - 1)];
	};
};


/* FPGA 5GNR DPC FEC DMA Decoding Request Descriptor */
struct __rte_packed fpga_dma_dec_desc {
	uint32_t done:1,
		iter:5,
		et_pass:1,
		crcb_pass:1,
		error:4,
		qm_idx:3,
		max_iter:5,
		bg_idx:1,
		rsrvd0:1,
		harqin_en:1,
		zc:9;
	uint32_t hbstroe_offset:22,
		num_null:10;
	uint32_t irq_en:1,
		ncb:15,
		desc_idx:10,
		drop_crc24b:1,
		crc24b_ind:1,
		rv:2,
		et_dis:1,
		rsrvd2:1;
	uint32_t harq_input_length:16,
		rm_e:16;/*the inbound data byte length*/
	uint32_t out_addr_lw;
	uint32_t out_addr_hi;
	uint32_t in_addr_lw;
	uint32_t in_addr_hi;

	union {
		struct {
			/* Virtual addresses used to retrieve SW context info */
			void *op_addr;
			/* Stores information about total number of Code Blocks
			 * in currently processed Transport Block
			 */
			uint8_t cbs_in_op;
		};

		uint32_t sw_ctxt[8 * (FPGA_RING_DESC_ENTRY_LENGTH - 1)];
	};
};

/* FPGA 5GNR DMA Descriptor */
union fpga_dma_desc {
	struct fpga_dma_enc_desc enc_req;
	struct fpga_dma_dec_desc dec_req;
};

/* FPGA 5GNR FEC Ring Control Register */
struct __rte_packed fpga_ring_ctrl_reg {
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

};

/* Private data structure for each FPGA FEC device */
struct fpga_5gnr_fec_device {
	/** Base address of MMIO registers (BAR0) */
	void *mmio_base;
	/** Base address of memory for sw rings */
	void *sw_rings;
	/** Physical address of sw_rings */
	rte_iova_t sw_rings_phys;
	/** Number of bytes available for each queue in device. */
	uint32_t sw_ring_size;
	/** Max number of entries available for each queue in device */
	uint32_t sw_ring_max_depth;
	/** Base address of response tail pointer buffer */
	uint32_t *tail_ptrs;
	/** Physical address of tail pointers */
	rte_iova_t tail_ptr_phys;
	/** Queues flush completion flag */
	uint64_t *flush_queue_status;
	/* Bitmap capturing which Queues are bound to the PF/VF */
	uint64_t q_bound_bit_map;
	/* Bitmap capturing which Queues have already been assigned */
	uint64_t q_assigned_bit_map;
	/** True if this is a PF FPGA FEC device */
	bool pf_device;
};

/* Structure associated with each queue. */
struct __rte_cache_aligned fpga_queue {
	struct fpga_ring_ctrl_reg ring_ctrl_reg;  /* Ring Control Register */
	union fpga_dma_desc *ring_addr;  /* Virtual address of software ring */
	uint64_t *ring_head_addr;  /* Virtual address of completion_head */
	uint64_t shadow_completion_head; /* Shadow completion head value */
	uint16_t head_free_desc;  /* Ring head */
	uint16_t tail;  /* Ring tail */
	/* Mask used to wrap enqueued descriptors on the sw ring */
	uint32_t sw_ring_wrap_mask;
	uint32_t irq_enable;  /* Enable ops dequeue interrupts if set to 1 */
	uint8_t q_idx;  /* Queue index */
	struct fpga_5gnr_fec_device *d;
	/* MMIO register of shadow_tail used to enqueue descriptors */
	void *shadow_tail_addr;
};

/* Write to 16 bit MMIO register address */
static inline void
mmio_write_16(void *addr, uint16_t value)
{
	*((volatile uint16_t *)(addr)) = rte_cpu_to_le_16(value);
}

/* Write to 32 bit MMIO register address */
static inline void
mmio_write_32(void *addr, uint32_t value)
{
	*((volatile uint32_t *)(addr)) = rte_cpu_to_le_32(value);
}

/* Write to 64 bit MMIO register address */
static inline void
mmio_write_64(void *addr, uint64_t value)
{
	*((volatile uint64_t *)(addr)) = rte_cpu_to_le_64(value);
}

/* Write a 8 bit register of a FPGA 5GNR FEC device */
static inline void
fpga_reg_write_8(void *mmio_base, uint32_t offset, uint8_t payload)
{
	void *reg_addr = RTE_PTR_ADD(mmio_base, offset);
	*((volatile uint8_t *)(reg_addr)) = payload;
}

/* Write a 16 bit register of a FPGA 5GNR FEC device */
static inline void
fpga_reg_write_16(void *mmio_base, uint32_t offset, uint16_t payload)
{
	void *reg_addr = RTE_PTR_ADD(mmio_base, offset);
	mmio_write_16(reg_addr, payload);
}

/* Write a 32 bit register of a FPGA 5GNR FEC device */
static inline void
fpga_reg_write_32(void *mmio_base, uint32_t offset, uint32_t payload)
{
	void *reg_addr = RTE_PTR_ADD(mmio_base, offset);
	mmio_write_32(reg_addr, payload);
}

/* Write a 64 bit register of a FPGA 5GNR FEC device */
static inline void
fpga_reg_write_64(void *mmio_base, uint32_t offset, uint64_t payload)
{
	void *reg_addr = RTE_PTR_ADD(mmio_base, offset);
	mmio_write_64(reg_addr, payload);
}

/* Write a ring control register of a FPGA 5GNR FEC device */
static inline void
fpga_ring_reg_write(void *mmio_base, uint32_t offset,
		struct fpga_ring_ctrl_reg payload)
{
	fpga_reg_write_64(mmio_base, offset, payload.ring_base_addr);
	fpga_reg_write_64(mmio_base, offset + FPGA_5GNR_FEC_RING_HEAD_ADDR,
			payload.ring_head_addr);
	fpga_reg_write_16(mmio_base, offset + FPGA_5GNR_FEC_RING_SIZE,
			payload.ring_size);
	fpga_reg_write_16(mmio_base, offset + FPGA_5GNR_FEC_RING_HEAD_POINT,
			payload.head_point);
	fpga_reg_write_8(mmio_base, offset + FPGA_5GNR_FEC_RING_FLUSH_QUEUE_EN,
			payload.flush_queue_en);
	fpga_reg_write_16(mmio_base, offset + FPGA_5GNR_FEC_RING_SHADOW_TAIL,
			payload.shadow_tail);
	fpga_reg_write_8(mmio_base, offset + FPGA_5GNR_FEC_RING_MISC,
			payload.misc);
	fpga_reg_write_8(mmio_base, offset + FPGA_5GNR_FEC_RING_ENABLE,
			payload.enable);
}

/* Read a register of FPGA 5GNR FEC device */
static inline uint32_t
fpga_reg_read_32(void *mmio_base, uint32_t offset)
{
	void *reg_addr = RTE_PTR_ADD(mmio_base, offset);
	uint32_t ret = *((volatile uint32_t *)(reg_addr));
	return rte_le_to_cpu_32(ret);
}

#ifdef RTE_LIBRTE_BBDEV_DEBUG

/* Read a register of FPGA 5GNR FEC device */
static inline uint16_t
fpga_reg_read_16(void *mmio_base, uint32_t offset)
{
	void *reg_addr = RTE_PTR_ADD(mmio_base, offset);
	uint16_t ret = *((volatile uint16_t *)(reg_addr));
	return rte_le_to_cpu_16(ret);
}

#endif

/* Read a register of FPGA 5GNR FEC device */
static inline uint8_t
fpga_reg_read_8(void *mmio_base, uint32_t offset)
{
	void *reg_addr = RTE_PTR_ADD(mmio_base, offset);
	return *((volatile uint8_t *)(reg_addr));
}

/* Read a register of FPGA 5GNR FEC device */
static inline uint64_t
fpga_reg_read_64(void *mmio_base, uint32_t offset)
{
	void *reg_addr = RTE_PTR_ADD(mmio_base, offset);
	uint64_t ret = *((volatile uint64_t *)(reg_addr));
	return rte_le_to_cpu_64(ret);
}

#endif /* _FPGA_5GNR_FEC_H_ */
