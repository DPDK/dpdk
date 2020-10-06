/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include <unistd.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_dev.h>
#include <rte_malloc.h>
#include <rte_mempool.h>
#include <rte_errno.h>
#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <rte_byteorder.h>
#ifdef RTE_BBDEV_OFFLOAD_COST
#include <rte_cycles.h>
#endif

#include <rte_bbdev.h>
#include <rte_bbdev_pmd.h>

#include "fpga_lte_fec.h"

#ifdef RTE_LIBRTE_BBDEV_DEBUG
RTE_LOG_REGISTER(fpga_lte_fec_logtype, pmd.bb.fpga_lte_fec, DEBUG);
#else
RTE_LOG_REGISTER(fpga_lte_fec_logtype, pmd.bb.fpga_lte_fec, NOTICE);
#endif

/* Helper macro for logging */
#define rte_bbdev_log(level, fmt, ...) \
	rte_log(RTE_LOG_ ## level, fpga_lte_fec_logtype, fmt "\n", \
		##__VA_ARGS__)

#ifdef RTE_LIBRTE_BBDEV_DEBUG
#define rte_bbdev_log_debug(fmt, ...) \
		rte_bbdev_log(DEBUG, "fpga_lte_fec: " fmt, \
		##__VA_ARGS__)
#else
#define rte_bbdev_log_debug(fmt, ...)
#endif

/* FPGA LTE FEC driver names */
#define FPGA_LTE_FEC_PF_DRIVER_NAME intel_fpga_lte_fec_pf
#define FPGA_LTE_FEC_VF_DRIVER_NAME intel_fpga_lte_fec_vf

/* FPGA LTE FEC PCI vendor & device IDs */
#define FPGA_LTE_FEC_VENDOR_ID (0x1172)
#define FPGA_LTE_FEC_PF_DEVICE_ID (0x5052)
#define FPGA_LTE_FEC_VF_DEVICE_ID (0x5050)

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
#define FPGA_TIMEOUT_CHECK_INTERVAL (5)

/* FPGA LTE FEC Register mapping on BAR0 */
enum {
	FPGA_LTE_FEC_VERSION_ID = 0x00000000, /* len: 4B */
	FPGA_LTE_FEC_CONFIGURATION = 0x00000004, /* len: 2B */
	FPGA_LTE_FEC_QUEUE_PF_VF_MAP_DONE = 0x00000008, /* len: 1B */
	FPGA_LTE_FEC_LOAD_BALANCE_FACTOR = 0x0000000a, /* len: 2B */
	FPGA_LTE_FEC_RING_DESC_LEN = 0x0000000c, /* len: 2B */
	FPGA_LTE_FEC_FLR_TIME_OUT = 0x0000000e, /* len: 2B */
	FPGA_LTE_FEC_VFQ_FLUSH_STATUS_LW = 0x00000018, /* len: 4B */
	FPGA_LTE_FEC_VFQ_FLUSH_STATUS_HI = 0x0000001c, /* len: 4B */
	FPGA_LTE_FEC_VF0_DEBUG = 0x00000020, /* len: 4B */
	FPGA_LTE_FEC_VF1_DEBUG = 0x00000024, /* len: 4B */
	FPGA_LTE_FEC_VF2_DEBUG = 0x00000028, /* len: 4B */
	FPGA_LTE_FEC_VF3_DEBUG = 0x0000002c, /* len: 4B */
	FPGA_LTE_FEC_VF4_DEBUG = 0x00000030, /* len: 4B */
	FPGA_LTE_FEC_VF5_DEBUG = 0x00000034, /* len: 4B */
	FPGA_LTE_FEC_VF6_DEBUG = 0x00000038, /* len: 4B */
	FPGA_LTE_FEC_VF7_DEBUG = 0x0000003c, /* len: 4B */
	FPGA_LTE_FEC_QUEUE_MAP = 0x00000040, /* len: 256B */
	FPGA_LTE_FEC_RING_CTRL_REGS = 0x00000200  /* len: 2048B */
};

/* FPGA LTE FEC Ring Control Registers */
enum {
	FPGA_LTE_FEC_RING_HEAD_ADDR = 0x00000008,
	FPGA_LTE_FEC_RING_SIZE = 0x00000010,
	FPGA_LTE_FEC_RING_MISC = 0x00000014,
	FPGA_LTE_FEC_RING_ENABLE = 0x00000015,
	FPGA_LTE_FEC_RING_FLUSH_QUEUE_EN = 0x00000016,
	FPGA_LTE_FEC_RING_SHADOW_TAIL = 0x00000018,
	FPGA_LTE_FEC_RING_HEAD_POINT = 0x0000001C
};

/* FPGA LTE FEC DESCRIPTOR ERROR */
enum {
	DESC_ERR_NO_ERR = 0x0,
	DESC_ERR_K_OUT_OF_RANGE = 0x1,
	DESC_ERR_K_NOT_NORMAL = 0x2,
	DESC_ERR_KPAI_NOT_NORMAL = 0x3,
	DESC_ERR_DESC_OFFSET_ERR = 0x4,
	DESC_ERR_DESC_READ_FAIL = 0x8,
	DESC_ERR_DESC_READ_TIMEOUT = 0x9,
	DESC_ERR_DESC_READ_TLP_POISONED = 0xA,
	DESC_ERR_CB_READ_FAIL = 0xC,
	DESC_ERR_CB_READ_TIMEOUT = 0xD,
	DESC_ERR_CB_READ_TLP_POISONED = 0xE
};

/* FPGA LTE FEC DMA Encoding Request Descriptor */
struct __rte_packed fpga_dma_enc_desc {
	uint32_t done:1,
		rsrvd0:11,
		error:4,
		rsrvd1:16;
	uint32_t ncb:16,
		rsrvd2:14,
		rv:2;
	uint32_t bypass_rm:1,
		irq_en:1,
		crc_en:1,
		rsrvd3:13,
		offset:10,
		rsrvd4:6;
	uint16_t e;
	uint16_t k;
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

/* FPGA LTE FEC DMA Decoding Request Descriptor */
struct __rte_packed fpga_dma_dec_desc {
	uint32_t done:1,
		iter:5,
		rsrvd0:2,
		crc_pass:1,
		rsrvd1:3,
		error:4,
		crc_type:1,
		rsrvd2:7,
		max_iter:5,
		rsrvd3:3;
	uint32_t rsrvd4;
	uint32_t bypass_rm:1,
		irq_en:1,
		drop_crc:1,
		rsrvd5:13,
		offset:10,
		rsrvd6:6;
	uint16_t k;
	uint16_t in_len;
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

/* FPGA LTE DMA Descriptor */
union fpga_dma_desc {
	struct fpga_dma_enc_desc enc_req;
	struct fpga_dma_dec_desc dec_req;
};

/* FPGA LTE FEC Ring Control Register */
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
struct fpga_lte_fec_device {
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
	struct fpga_lte_fec_device *d;
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

/* Write a 8 bit register of a FPGA LTE FEC device */
static inline void
fpga_reg_write_8(void *mmio_base, uint32_t offset, uint8_t payload)
{
	void *reg_addr = RTE_PTR_ADD(mmio_base, offset);
	*((volatile uint8_t *)(reg_addr)) = payload;
}

/* Write a 16 bit register of a FPGA LTE FEC device */
static inline void
fpga_reg_write_16(void *mmio_base, uint32_t offset, uint16_t payload)
{
	void *reg_addr = RTE_PTR_ADD(mmio_base, offset);
	mmio_write_16(reg_addr, payload);
}

/* Write a 32 bit register of a FPGA LTE FEC device */
static inline void
fpga_reg_write_32(void *mmio_base, uint32_t offset, uint32_t payload)
{
	void *reg_addr = RTE_PTR_ADD(mmio_base, offset);
	mmio_write_32(reg_addr, payload);
}

/* Write a 64 bit register of a FPGA LTE FEC device */
static inline void
fpga_reg_write_64(void *mmio_base, uint32_t offset, uint64_t payload)
{
	void *reg_addr = RTE_PTR_ADD(mmio_base, offset);
	mmio_write_64(reg_addr, payload);
}

/* Write a ring control register of a FPGA LTE FEC device */
static inline void
fpga_ring_reg_write(void *mmio_base, uint32_t offset,
		struct fpga_ring_ctrl_reg payload)
{
	fpga_reg_write_64(mmio_base, offset, payload.ring_base_addr);
	fpga_reg_write_64(mmio_base, offset + FPGA_LTE_FEC_RING_HEAD_ADDR,
			payload.ring_head_addr);
	fpga_reg_write_16(mmio_base, offset + FPGA_LTE_FEC_RING_SIZE,
			payload.ring_size);
	fpga_reg_write_16(mmio_base, offset + FPGA_LTE_FEC_RING_HEAD_POINT,
			payload.head_point);
	fpga_reg_write_8(mmio_base, offset + FPGA_LTE_FEC_RING_FLUSH_QUEUE_EN,
			payload.flush_queue_en);
	fpga_reg_write_16(mmio_base, offset + FPGA_LTE_FEC_RING_SHADOW_TAIL,
			payload.shadow_tail);
	fpga_reg_write_8(mmio_base, offset + FPGA_LTE_FEC_RING_MISC,
			payload.misc);
	fpga_reg_write_8(mmio_base, offset + FPGA_LTE_FEC_RING_ENABLE,
			payload.enable);
}

/* Read a register of FPGA LTE FEC device */
static uint32_t
fpga_reg_read_32(void *mmio_base, uint32_t offset)
{
	void *reg_addr = RTE_PTR_ADD(mmio_base, offset);
	uint32_t ret = *((volatile uint32_t *)(reg_addr));
	return rte_le_to_cpu_32(ret);
}

#ifdef RTE_LIBRTE_BBDEV_DEBUG
/* Read a register of FPGA LTE FEC device */
static uint8_t
fpga_reg_read_8(void *mmio_base, uint32_t offset)
{
	void *reg_addr = RTE_PTR_ADD(mmio_base, offset);
	return *((volatile uint8_t *)(reg_addr));
}

/* Read a register of FPGA LTE FEC device */
static uint16_t
fpga_reg_read_16(void *mmio_base, uint32_t offset)
{
	void *reg_addr = RTE_PTR_ADD(mmio_base, offset);
	uint16_t ret = *((volatile uint16_t *)(reg_addr));
	return rte_le_to_cpu_16(ret);
}

/* Read a register of FPGA LTE FEC device */
static uint64_t
fpga_reg_read_64(void *mmio_base, uint32_t offset)
{
	void *reg_addr = RTE_PTR_ADD(mmio_base, offset);
	uint64_t ret = *((volatile uint64_t *)(reg_addr));
	return rte_le_to_cpu_64(ret);
}

/* Read Ring Control Register of FPGA LTE FEC device */
static inline void
print_ring_reg_debug_info(void *mmio_base, uint32_t offset)
{
	rte_bbdev_log_debug(
		"FPGA MMIO base address @ %p | Ring Control Register @ offset = 0x%08"
		PRIx32, mmio_base, offset);
	rte_bbdev_log_debug(
		"RING_BASE_ADDR = 0x%016"PRIx64,
		fpga_reg_read_64(mmio_base, offset));
	rte_bbdev_log_debug(
		"RING_HEAD_ADDR = 0x%016"PRIx64,
		fpga_reg_read_64(mmio_base, offset +
				FPGA_LTE_FEC_RING_HEAD_ADDR));
	rte_bbdev_log_debug(
		"RING_SIZE = 0x%04"PRIx16,
		fpga_reg_read_16(mmio_base, offset +
				FPGA_LTE_FEC_RING_SIZE));
	rte_bbdev_log_debug(
		"RING_MISC = 0x%02"PRIx8,
		fpga_reg_read_8(mmio_base, offset +
				FPGA_LTE_FEC_RING_MISC));
	rte_bbdev_log_debug(
		"RING_ENABLE = 0x%02"PRIx8,
		fpga_reg_read_8(mmio_base, offset +
				FPGA_LTE_FEC_RING_ENABLE));
	rte_bbdev_log_debug(
		"RING_FLUSH_QUEUE_EN = 0x%02"PRIx8,
		fpga_reg_read_8(mmio_base, offset +
				FPGA_LTE_FEC_RING_FLUSH_QUEUE_EN));
	rte_bbdev_log_debug(
		"RING_SHADOW_TAIL = 0x%04"PRIx16,
		fpga_reg_read_16(mmio_base, offset +
				FPGA_LTE_FEC_RING_SHADOW_TAIL));
	rte_bbdev_log_debug(
		"RING_HEAD_POINT = 0x%04"PRIx16,
		fpga_reg_read_16(mmio_base, offset +
				FPGA_LTE_FEC_RING_HEAD_POINT));
}

/* Read Static Register of FPGA LTE FEC device */
static inline void
print_static_reg_debug_info(void *mmio_base)
{
	uint16_t config = fpga_reg_read_16(mmio_base,
			FPGA_LTE_FEC_CONFIGURATION);
	uint8_t qmap_done = fpga_reg_read_8(mmio_base,
			FPGA_LTE_FEC_QUEUE_PF_VF_MAP_DONE);
	uint16_t lb_factor = fpga_reg_read_16(mmio_base,
			FPGA_LTE_FEC_LOAD_BALANCE_FACTOR);
	uint16_t ring_desc_len = fpga_reg_read_16(mmio_base,
			FPGA_LTE_FEC_RING_DESC_LEN);
	uint16_t flr_time_out = fpga_reg_read_16(mmio_base,
			FPGA_LTE_FEC_FLR_TIME_OUT);

	rte_bbdev_log_debug("UL.DL Weights = %u.%u",
			((uint8_t)config), ((uint8_t)(config >> 8)));
	rte_bbdev_log_debug("UL.DL Load Balance = %u.%u",
			((uint8_t)lb_factor), ((uint8_t)(lb_factor >> 8)));
	rte_bbdev_log_debug("Queue-PF/VF Mapping Table = %s",
			(qmap_done > 0) ? "READY" : "NOT-READY");
	rte_bbdev_log_debug("Ring Descriptor Size = %u bytes",
			ring_desc_len*FPGA_RING_DESC_LEN_UNIT_BYTES);
	rte_bbdev_log_debug("FLR Timeout = %f usec",
			(float)flr_time_out*FPGA_FLR_TIMEOUT_UNIT);
}

/* Print decode DMA Descriptor of FPGA LTE FEC device */
static void
print_dma_dec_desc_debug_info(union fpga_dma_desc *desc)
{
	rte_bbdev_log_debug("DMA response desc %p\n"
		"\t-- done(%"PRIu32") | iter(%"PRIu32") | crc_pass(%"PRIu32")"
		" | error (%"PRIu32") | crc_type(%"PRIu32")\n"
		"\t-- max_iter(%"PRIu32") | bypass_rm(%"PRIu32") | "
		"irq_en (%"PRIu32") | drop_crc(%"PRIu32") | offset(%"PRIu32")\n"
		"\t-- k(%"PRIu32") | in_len (%"PRIu16") | op_add(%p)\n"
		"\t-- cbs_in_op(%"PRIu32") | in_add (0x%08"PRIx32"%08"PRIx32") | "
		"out_add (0x%08"PRIx32"%08"PRIx32")",
		desc,
		(uint32_t)desc->dec_req.done,
		(uint32_t)desc->dec_req.iter,
		(uint32_t)desc->dec_req.crc_pass,
		(uint32_t)desc->dec_req.error,
		(uint32_t)desc->dec_req.crc_type,
		(uint32_t)desc->dec_req.max_iter,
		(uint32_t)desc->dec_req.bypass_rm,
		(uint32_t)desc->dec_req.irq_en,
		(uint32_t)desc->dec_req.drop_crc,
		(uint32_t)desc->dec_req.offset,
		(uint32_t)desc->dec_req.k,
		(uint16_t)desc->dec_req.in_len,
		desc->dec_req.op_addr,
		(uint32_t)desc->dec_req.cbs_in_op,
		(uint32_t)desc->dec_req.in_addr_hi,
		(uint32_t)desc->dec_req.in_addr_lw,
		(uint32_t)desc->dec_req.out_addr_hi,
		(uint32_t)desc->dec_req.out_addr_lw);
}
#endif

static int
fpga_setup_queues(struct rte_bbdev *dev, uint16_t num_queues, int socket_id)
{
	/* Number of queues bound to a PF/VF */
	uint32_t hw_q_num = 0;
	uint32_t ring_size, payload, address, q_id, offset;
	rte_iova_t phys_addr;
	struct fpga_ring_ctrl_reg ring_reg;
	struct fpga_lte_fec_device *fpga_dev = dev->data->dev_private;

	address = FPGA_LTE_FEC_QUEUE_PF_VF_MAP_DONE;
	if (!(fpga_reg_read_32(fpga_dev->mmio_base, address) & 0x1)) {
		rte_bbdev_log(ERR,
				"Queue-PF/VF mapping is not set! Was PF configured for device (%s) ?",
				dev->data->name);
		return -EPERM;
	}

	/* Clear queue registers structure */
	memset(&ring_reg, 0, sizeof(struct fpga_ring_ctrl_reg));

	/* Scan queue map.
	 * If a queue is valid and mapped to a calling PF/VF the read value is
	 * replaced with a queue ID and if it's not then
	 * FPGA_INVALID_HW_QUEUE_ID is returned.
	 */
	for (q_id = 0; q_id < FPGA_TOTAL_NUM_QUEUES; ++q_id) {
		uint32_t hw_q_id = fpga_reg_read_32(fpga_dev->mmio_base,
				FPGA_LTE_FEC_QUEUE_MAP + (q_id << 2));

		rte_bbdev_log_debug("%s: queue ID: %u, registry queue ID: %u",
				dev->device->name, q_id, hw_q_id);

		if (hw_q_id != FPGA_INVALID_HW_QUEUE_ID) {
			fpga_dev->q_bound_bit_map |= (1ULL << q_id);
			/* Clear queue register of found queue */
			offset = FPGA_LTE_FEC_RING_CTRL_REGS +
				(sizeof(struct fpga_ring_ctrl_reg) * q_id);
			fpga_ring_reg_write(fpga_dev->mmio_base,
					offset, ring_reg);
			++hw_q_num;
		}
	}
	if (hw_q_num == 0) {
		rte_bbdev_log(ERR,
			"No HW queues assigned to this device. Probably this is a VF configured for PF mode. Check device configuration!");
		return -ENODEV;
	}

	if (num_queues > hw_q_num) {
		rte_bbdev_log(ERR,
			"Not enough queues for device %s! Requested: %u, available: %u",
			dev->device->name, num_queues, hw_q_num);
		return -EINVAL;
	}

	ring_size = FPGA_RING_MAX_SIZE * sizeof(struct fpga_dma_dec_desc);

	/* Enforce 32 byte alignment */
	RTE_BUILD_BUG_ON((RTE_CACHE_LINE_SIZE % 32) != 0);

	/* Allocate memory for SW descriptor rings */
	fpga_dev->sw_rings = rte_zmalloc_socket(dev->device->driver->name,
			num_queues * ring_size, RTE_CACHE_LINE_SIZE,
			socket_id);
	if (fpga_dev->sw_rings == NULL) {
		rte_bbdev_log(ERR,
				"Failed to allocate memory for %s:%u sw_rings",
				dev->device->driver->name, dev->data->dev_id);
		return -ENOMEM;
	}

	fpga_dev->sw_rings_phys = rte_malloc_virt2iova(fpga_dev->sw_rings);
	fpga_dev->sw_ring_size = ring_size;
	fpga_dev->sw_ring_max_depth = FPGA_RING_MAX_SIZE;

	/* Allocate memory for ring flush status */
	fpga_dev->flush_queue_status = rte_zmalloc_socket(NULL,
			sizeof(uint64_t), RTE_CACHE_LINE_SIZE, socket_id);
	if (fpga_dev->flush_queue_status == NULL) {
		rte_bbdev_log(ERR,
				"Failed to allocate memory for %s:%u flush_queue_status",
				dev->device->driver->name, dev->data->dev_id);
		return -ENOMEM;
	}

	/* Set the flush status address registers */
	phys_addr = rte_malloc_virt2iova(fpga_dev->flush_queue_status);

	address = FPGA_LTE_FEC_VFQ_FLUSH_STATUS_LW;
	payload = (uint32_t)(phys_addr);
	fpga_reg_write_32(fpga_dev->mmio_base, address, payload);

	address = FPGA_LTE_FEC_VFQ_FLUSH_STATUS_HI;
	payload = (uint32_t)(phys_addr >> 32);
	fpga_reg_write_32(fpga_dev->mmio_base, address, payload);

	return 0;
}

static int
fpga_dev_close(struct rte_bbdev *dev)
{
	struct fpga_lte_fec_device *fpga_dev = dev->data->dev_private;

	rte_free(fpga_dev->sw_rings);
	rte_free(fpga_dev->flush_queue_status);

	return 0;
}

static void
fpga_dev_info_get(struct rte_bbdev *dev,
		struct rte_bbdev_driver_info *dev_info)
{
	struct fpga_lte_fec_device *d = dev->data->dev_private;
	uint32_t q_id = 0;

	/* TODO RTE_BBDEV_TURBO_NEG_LLR_1_BIT_IN and numbers of buffers are set
	 * to temporary values as they are required by test application while
	 * validation phase.
	 */
	static const struct rte_bbdev_op_cap bbdev_capabilities[] = {
		{
			.type = RTE_BBDEV_OP_TURBO_DEC,
			.cap.turbo_dec = {
				.capability_flags =
					RTE_BBDEV_TURBO_CRC_TYPE_24B |
					RTE_BBDEV_TURBO_SUBBLOCK_DEINTERLEAVE |
					RTE_BBDEV_TURBO_DEC_INTERRUPTS |
					RTE_BBDEV_TURBO_NEG_LLR_1_BIT_IN |
					RTE_BBDEV_TURBO_DEC_TB_CRC_24B_KEEP,
				.max_llr_modulus = INT8_MAX,
				.num_buffers_src =
						RTE_BBDEV_TURBO_MAX_CODE_BLOCKS,
				.num_buffers_hard_out =
					RTE_BBDEV_TURBO_MAX_CODE_BLOCKS,
				.num_buffers_soft_out = 0
			}
		},
		{
			.type = RTE_BBDEV_OP_TURBO_ENC,
			.cap.turbo_enc = {
				.capability_flags =
					RTE_BBDEV_TURBO_CRC_24B_ATTACH |
					RTE_BBDEV_TURBO_RATE_MATCH |
					RTE_BBDEV_TURBO_ENC_INTERRUPTS,
				.num_buffers_src =
						RTE_BBDEV_TURBO_MAX_CODE_BLOCKS,
				.num_buffers_dst =
						RTE_BBDEV_TURBO_MAX_CODE_BLOCKS
			}
		},
		RTE_BBDEV_END_OF_CAPABILITIES_LIST()
	};

	static struct rte_bbdev_queue_conf default_queue_conf;
	default_queue_conf.socket = dev->data->socket_id;
	default_queue_conf.queue_size = FPGA_RING_MAX_SIZE;


	dev_info->driver_name = dev->device->driver->name;
	dev_info->queue_size_lim = FPGA_RING_MAX_SIZE;
	dev_info->hardware_accelerated = true;
	dev_info->min_alignment = 64;
	dev_info->default_queue_conf = default_queue_conf;
	dev_info->capabilities = bbdev_capabilities;
	dev_info->cpu_flag_reqs = NULL;

	/* Calculates number of queues assigned to device */
	dev_info->max_num_queues = 0;
	for (q_id = 0; q_id < FPGA_TOTAL_NUM_QUEUES; ++q_id) {
		uint32_t hw_q_id = fpga_reg_read_32(d->mmio_base,
				FPGA_LTE_FEC_QUEUE_MAP + (q_id << 2));
		if (hw_q_id != FPGA_INVALID_HW_QUEUE_ID)
			dev_info->max_num_queues++;
	}
}

/**
 * Find index of queue bound to current PF/VF which is unassigned. Return -1
 * when there is no available queue
 */
static int
fpga_find_free_queue_idx(struct rte_bbdev *dev,
		const struct rte_bbdev_queue_conf *conf)
{
	struct fpga_lte_fec_device *d = dev->data->dev_private;
	uint64_t q_idx;
	uint8_t i = 0;
	uint8_t range = FPGA_TOTAL_NUM_QUEUES >> 1;

	if (conf->op_type == RTE_BBDEV_OP_TURBO_ENC) {
		i = FPGA_NUM_DL_QUEUES;
		range = FPGA_TOTAL_NUM_QUEUES;
	}

	for (; i < range; ++i) {
		q_idx = 1ULL << i;
		/* Check if index of queue is bound to current PF/VF */
		if (d->q_bound_bit_map & q_idx)
			/* Check if found queue was not already assigned */
			if (!(d->q_assigned_bit_map & q_idx)) {
				d->q_assigned_bit_map |= q_idx;
				return i;
			}
	}

	rte_bbdev_log(INFO, "Failed to find free queue on %s", dev->data->name);

	return -1;
}

static int
fpga_queue_setup(struct rte_bbdev *dev, uint16_t queue_id,
		const struct rte_bbdev_queue_conf *conf)
{
	uint32_t address, ring_offset;
	struct fpga_lte_fec_device *d = dev->data->dev_private;
	struct fpga_queue *q;
	int8_t q_idx;

	/* Check if there is a free queue to assign */
	q_idx = fpga_find_free_queue_idx(dev, conf);
	if (q_idx == -1)
		return -1;

	/* Allocate the queue data structure. */
	q = rte_zmalloc_socket(dev->device->driver->name, sizeof(*q),
			RTE_CACHE_LINE_SIZE, conf->socket);
	if (q == NULL) {
		/* Mark queue as un-assigned */
		d->q_assigned_bit_map &= (0xFFFFFFFF - (1ULL << q_idx));
		rte_bbdev_log(ERR, "Failed to allocate queue memory");
		return -ENOMEM;
	}

	q->d = d;
	q->q_idx = q_idx;

	/* Set ring_base_addr */
	q->ring_addr = RTE_PTR_ADD(d->sw_rings, (d->sw_ring_size * queue_id));
	q->ring_ctrl_reg.ring_base_addr = d->sw_rings_phys +
			(d->sw_ring_size * queue_id);

	/* Allocate memory for Completion Head variable*/
	q->ring_head_addr = rte_zmalloc_socket(dev->device->driver->name,
			sizeof(uint64_t), RTE_CACHE_LINE_SIZE, conf->socket);
	if (q->ring_head_addr == NULL) {
		/* Mark queue as un-assigned */
		d->q_assigned_bit_map &= (0xFFFFFFFF - (1ULL << q_idx));
		rte_free(q);
		rte_bbdev_log(ERR,
				"Failed to allocate memory for %s:%u completion_head",
				dev->device->driver->name, dev->data->dev_id);
		return -ENOMEM;
	}
	/* Set ring_head_addr */
	q->ring_ctrl_reg.ring_head_addr =
			rte_malloc_virt2iova(q->ring_head_addr);

	/* Clear shadow_completion_head */
	q->shadow_completion_head = 0;

	/* Set ring_size */
	if (conf->queue_size > FPGA_RING_MAX_SIZE) {
		/* Mark queue as un-assigned */
		d->q_assigned_bit_map &= (0xFFFFFFFF - (1ULL << q_idx));
		rte_free(q->ring_head_addr);
		rte_free(q);
		rte_bbdev_log(ERR,
				"Size of queue is too big %d (MAX: %d ) for %s:%u",
				conf->queue_size, FPGA_RING_MAX_SIZE,
				dev->device->driver->name, dev->data->dev_id);
		return -EINVAL;
	}
	q->ring_ctrl_reg.ring_size = conf->queue_size;

	/* Set Miscellaneous FPGA register*/
	/* Max iteration number for TTI mitigation - todo */
	q->ring_ctrl_reg.max_ul_dec = 0;
	/* Enable max iteration number for TTI - todo */
	q->ring_ctrl_reg.max_ul_dec_en = 0;

	/* Enable the ring */
	q->ring_ctrl_reg.enable = 1;

	/* Set FPGA head_point and tail registers */
	q->ring_ctrl_reg.head_point = q->tail = 0;

	/* Set FPGA shadow_tail register */
	q->ring_ctrl_reg.shadow_tail = q->tail;

	/* Calculates the ring offset for found queue */
	ring_offset = FPGA_LTE_FEC_RING_CTRL_REGS +
			(sizeof(struct fpga_ring_ctrl_reg) * q_idx);

	/* Set FPGA Ring Control Registers */
	fpga_ring_reg_write(d->mmio_base, ring_offset, q->ring_ctrl_reg);

	/* Store MMIO register of shadow_tail */
	address = ring_offset + FPGA_LTE_FEC_RING_SHADOW_TAIL;
	q->shadow_tail_addr = RTE_PTR_ADD(d->mmio_base, address);

	q->head_free_desc = q->tail;

	/* Set wrap mask */
	q->sw_ring_wrap_mask = conf->queue_size - 1;

	rte_bbdev_log_debug("Setup dev%u q%u: queue_idx=%u",
			dev->data->dev_id, queue_id, q->q_idx);

	dev->data->queues[queue_id].queue_private = q;

	rte_bbdev_log_debug("BBDEV queue[%d] set up for FPGA queue[%d]",
			queue_id, q_idx);

#ifdef RTE_LIBRTE_BBDEV_DEBUG
	/* Read FPGA Ring Control Registers after configuration*/
	print_ring_reg_debug_info(d->mmio_base, ring_offset);
#endif
	return 0;
}

static int
fpga_queue_release(struct rte_bbdev *dev, uint16_t queue_id)
{
	struct fpga_lte_fec_device *d = dev->data->dev_private;
	struct fpga_queue *q = dev->data->queues[queue_id].queue_private;
	struct fpga_ring_ctrl_reg ring_reg;
	uint32_t offset;

	rte_bbdev_log_debug("FPGA Queue[%d] released", queue_id);

	if (q != NULL) {
		memset(&ring_reg, 0, sizeof(struct fpga_ring_ctrl_reg));
		offset = FPGA_LTE_FEC_RING_CTRL_REGS +
			(sizeof(struct fpga_ring_ctrl_reg) * q->q_idx);
		/* Disable queue */
		fpga_reg_write_8(d->mmio_base,
				offset + FPGA_LTE_FEC_RING_ENABLE, 0x00);
		/* Clear queue registers */
		fpga_ring_reg_write(d->mmio_base, offset, ring_reg);

		/* Mark the Queue as un-assigned */
		d->q_assigned_bit_map &= (0xFFFFFFFF - (1ULL << q->q_idx));
		rte_free(q->ring_head_addr);
		rte_free(q);
		dev->data->queues[queue_id].queue_private = NULL;
	}

	return 0;
}

/* Function starts a device queue. */
static int
fpga_queue_start(struct rte_bbdev *dev, uint16_t queue_id)
{
	struct fpga_lte_fec_device *d = dev->data->dev_private;
#ifdef RTE_LIBRTE_BBDEV_DEBUG
	if (d == NULL) {
		rte_bbdev_log(ERR, "Invalid device pointer");
		return -1;
	}
#endif
	struct fpga_queue *q = dev->data->queues[queue_id].queue_private;
	uint32_t offset = FPGA_LTE_FEC_RING_CTRL_REGS +
			(sizeof(struct fpga_ring_ctrl_reg) * q->q_idx);
	uint8_t enable = 0x01;
	uint16_t zero = 0x0000;

	/* Clear queue head and tail variables */
	q->tail = q->head_free_desc = 0;

	/* Clear FPGA head_point and tail registers */
	fpga_reg_write_16(d->mmio_base, offset + FPGA_LTE_FEC_RING_HEAD_POINT,
			zero);
	fpga_reg_write_16(d->mmio_base, offset + FPGA_LTE_FEC_RING_SHADOW_TAIL,
			zero);

	/* Enable queue */
	fpga_reg_write_8(d->mmio_base, offset + FPGA_LTE_FEC_RING_ENABLE,
			enable);

	rte_bbdev_log_debug("FPGA Queue[%d] started", queue_id);
	return 0;
}

/* Function stops a device queue. */
static int
fpga_queue_stop(struct rte_bbdev *dev, uint16_t queue_id)
{
	struct fpga_lte_fec_device *d = dev->data->dev_private;
#ifdef RTE_LIBRTE_BBDEV_DEBUG
	if (d == NULL) {
		rte_bbdev_log(ERR, "Invalid device pointer");
		return -1;
	}
#endif
	struct fpga_queue *q = dev->data->queues[queue_id].queue_private;
	uint32_t offset = FPGA_LTE_FEC_RING_CTRL_REGS +
			(sizeof(struct fpga_ring_ctrl_reg) * q->q_idx);
	uint8_t payload = 0x01;
	uint8_t counter = 0;
	uint8_t timeout = FPGA_QUEUE_FLUSH_TIMEOUT_US /
			FPGA_TIMEOUT_CHECK_INTERVAL;

	/* Set flush_queue_en bit to trigger queue flushing */
	fpga_reg_write_8(d->mmio_base,
			offset + FPGA_LTE_FEC_RING_FLUSH_QUEUE_EN, payload);

	/** Check if queue flush is completed.
	 * FPGA will update the completion flag after queue flushing is
	 * completed. If completion flag is not updated within 1ms it is
	 * considered as a failure.
	 */
	while (!(*((volatile uint8_t *)d->flush_queue_status + q->q_idx) & payload)) {
		if (counter > timeout) {
			rte_bbdev_log(ERR, "FPGA Queue Flush failed for queue %d",
					queue_id);
			return -1;
		}
		usleep(FPGA_TIMEOUT_CHECK_INTERVAL);
		counter++;
	}

	/* Disable queue */
	payload = 0x00;
	fpga_reg_write_8(d->mmio_base, offset + FPGA_LTE_FEC_RING_ENABLE,
			payload);

	rte_bbdev_log_debug("FPGA Queue[%d] stopped", queue_id);
	return 0;
}

static inline uint16_t
get_queue_id(struct rte_bbdev_data *data, uint8_t q_idx)
{
	uint16_t queue_id;

	for (queue_id = 0; queue_id < data->num_queues; ++queue_id) {
		struct fpga_queue *q = data->queues[queue_id].queue_private;
		if (q != NULL && q->q_idx == q_idx)
			return queue_id;
	}

	return -1;
}

/* Interrupt handler triggered by FPGA dev for handling specific interrupt */
static void
fpga_dev_interrupt_handler(void *cb_arg)
{
	struct rte_bbdev *dev = cb_arg;
	struct fpga_lte_fec_device *fpga_dev = dev->data->dev_private;
	struct fpga_queue *q;
	uint64_t ring_head;
	uint64_t q_idx;
	uint16_t queue_id;
	uint8_t i;

	/* Scan queue assigned to this device */
	for (i = 0; i < FPGA_TOTAL_NUM_QUEUES; ++i) {
		q_idx = 1ULL << i;
		if (fpga_dev->q_bound_bit_map & q_idx) {
			queue_id = get_queue_id(dev->data, i);
			if (queue_id == (uint16_t) -1)
				continue;

			/* Check if completion head was changed */
			q = dev->data->queues[queue_id].queue_private;
			ring_head = *q->ring_head_addr;
			if (q->shadow_completion_head != ring_head &&
				q->irq_enable == 1) {
				q->shadow_completion_head = ring_head;
				rte_bbdev_pmd_callback_process(
						dev,
						RTE_BBDEV_EVENT_DEQUEUE,
						&queue_id);
			}
		}
	}
}

static int
fpga_queue_intr_enable(struct rte_bbdev *dev, uint16_t queue_id)
{
	struct fpga_queue *q = dev->data->queues[queue_id].queue_private;

	if (!rte_intr_cap_multiple(dev->intr_handle))
		return -ENOTSUP;

	q->irq_enable = 1;

	return 0;
}

static int
fpga_queue_intr_disable(struct rte_bbdev *dev, uint16_t queue_id)
{
	struct fpga_queue *q = dev->data->queues[queue_id].queue_private;
	q->irq_enable = 0;

	return 0;
}

static int
fpga_intr_enable(struct rte_bbdev *dev)
{
	int ret;
	uint8_t i;

	if (!rte_intr_cap_multiple(dev->intr_handle)) {
		rte_bbdev_log(ERR, "Multiple intr vector is not supported by FPGA (%s)",
				dev->data->name);
		return -ENOTSUP;
	}

	/* Create event file descriptors for each of 64 queue. Event fds will be
	 * mapped to FPGA IRQs in rte_intr_enable(). This is a 1:1 mapping where
	 * the IRQ number is a direct translation to the queue number.
	 *
	 * 63 (FPGA_NUM_INTR_VEC) event fds are created as rte_intr_enable()
	 * mapped the first IRQ to already created interrupt event file
	 * descriptor (intr_handle->fd).
	 */
	if (rte_intr_efd_enable(dev->intr_handle, FPGA_NUM_INTR_VEC)) {
		rte_bbdev_log(ERR, "Failed to create fds for %u queues",
				dev->data->num_queues);
		return -1;
	}

	/* TODO Each event file descriptor is overwritten by interrupt event
	 * file descriptor. That descriptor is added to epoll observed list.
	 * It ensures that callback function assigned to that descriptor will
	 * invoked when any FPGA queue issues interrupt.
	 */
	for (i = 0; i < FPGA_NUM_INTR_VEC; ++i)
		dev->intr_handle->efds[i] = dev->intr_handle->fd;

	if (!dev->intr_handle->intr_vec) {
		dev->intr_handle->intr_vec = rte_zmalloc("intr_vec",
				dev->data->num_queues * sizeof(int), 0);
		if (!dev->intr_handle->intr_vec) {
			rte_bbdev_log(ERR, "Failed to allocate %u vectors",
					dev->data->num_queues);
			return -ENOMEM;
		}
	}

	ret = rte_intr_enable(dev->intr_handle);
	if (ret < 0) {
		rte_bbdev_log(ERR,
				"Couldn't enable interrupts for device: %s",
				dev->data->name);
		return ret;
	}

	ret = rte_intr_callback_register(dev->intr_handle,
			fpga_dev_interrupt_handler, dev);
	if (ret < 0) {
		rte_bbdev_log(ERR,
				"Couldn't register interrupt callback for device: %s",
				dev->data->name);
		return ret;
	}

	return 0;
}

static const struct rte_bbdev_ops fpga_ops = {
	.setup_queues = fpga_setup_queues,
	.intr_enable = fpga_intr_enable,
	.close = fpga_dev_close,
	.info_get = fpga_dev_info_get,
	.queue_setup = fpga_queue_setup,
	.queue_stop = fpga_queue_stop,
	.queue_start = fpga_queue_start,
	.queue_release = fpga_queue_release,
	.queue_intr_enable = fpga_queue_intr_enable,
	.queue_intr_disable = fpga_queue_intr_disable
};

static inline void
fpga_dma_enqueue(struct fpga_queue *q, uint16_t num_desc,
		struct rte_bbdev_stats *queue_stats)
{
#ifdef RTE_BBDEV_OFFLOAD_COST
	uint64_t start_time = 0;
	queue_stats->acc_offload_cycles = 0;
#else
	RTE_SET_USED(queue_stats);
#endif

	/* Update tail and shadow_tail register */
	q->tail = (q->tail + num_desc) & q->sw_ring_wrap_mask;

	rte_wmb();

#ifdef RTE_BBDEV_OFFLOAD_COST
	/* Start time measurement for enqueue function offload. */
	start_time = rte_rdtsc_precise();
#endif
	mmio_write_16(q->shadow_tail_addr, q->tail);

#ifdef RTE_BBDEV_OFFLOAD_COST
	rte_wmb();
	queue_stats->acc_offload_cycles += rte_rdtsc_precise() - start_time;
#endif
}

/* Calculates number of CBs in processed encoder TB based on 'r' and input
 * length.
 */
static inline uint8_t
get_num_cbs_in_op_enc(struct rte_bbdev_op_turbo_enc *turbo_enc)
{
	uint8_t c, c_neg, r, crc24_bits = 0;
	uint16_t k, k_neg, k_pos;
	uint8_t cbs_in_op = 0;
	int32_t length;

	length = turbo_enc->input.length;
	r = turbo_enc->tb_params.r;
	c = turbo_enc->tb_params.c;
	c_neg = turbo_enc->tb_params.c_neg;
	k_neg = turbo_enc->tb_params.k_neg;
	k_pos = turbo_enc->tb_params.k_pos;
	crc24_bits = 24;
	while (length > 0 && r < c) {
		k = (r < c_neg) ? k_neg : k_pos;
		length -= (k - crc24_bits) >> 3;
		r++;
		cbs_in_op++;
	}

	return cbs_in_op;
}

/* Calculates number of CBs in processed decoder TB based on 'r' and input
 * length.
 */
static inline uint16_t
get_num_cbs_in_op_dec(struct rte_bbdev_op_turbo_dec *turbo_dec)
{
	uint8_t c, c_neg, r = 0;
	uint16_t kw, k, k_neg, k_pos, cbs_in_op = 0;
	int32_t length;

	length = turbo_dec->input.length;
	r = turbo_dec->tb_params.r;
	c = turbo_dec->tb_params.c;
	c_neg = turbo_dec->tb_params.c_neg;
	k_neg = turbo_dec->tb_params.k_neg;
	k_pos = turbo_dec->tb_params.k_pos;
	while (length > 0 && r < c) {
		k = (r < c_neg) ? k_neg : k_pos;
		kw = RTE_ALIGN_CEIL(k + 4, 32) * 3;
		length -= kw;
		r++;
		cbs_in_op++;
	}

	return cbs_in_op;
}

/* Read flag value 0/1/ from bitmap */
static inline bool
check_bit(uint32_t bitmap, uint32_t bitmask)
{
	return bitmap & bitmask;
}

/* Print an error if a descriptor error has occurred.
 *  Return 0 on success, 1 on failure
 */
static inline int
check_desc_error(uint32_t error_code) {
	switch (error_code) {
	case DESC_ERR_NO_ERR:
		return 0;
	case DESC_ERR_K_OUT_OF_RANGE:
		rte_bbdev_log(ERR, "Block_size_k is out of range (k<40 or k>6144)");
		break;
	case DESC_ERR_K_NOT_NORMAL:
		rte_bbdev_log(ERR, "Block_size_k is not a normal value within normal range");
		break;
	case DESC_ERR_KPAI_NOT_NORMAL:
		rte_bbdev_log(ERR, "Three_kpai is not a normal value for UL only");
		break;
	case DESC_ERR_DESC_OFFSET_ERR:
		rte_bbdev_log(ERR, "Queue offset does not meet the expectation in the FPGA");
		break;
	case (DESC_ERR_K_OUT_OF_RANGE | DESC_ERR_DESC_OFFSET_ERR):
		rte_bbdev_log(ERR, "Block_size_k is out of range (k<40 or k>6144) and queue offset error");
		break;
	case (DESC_ERR_K_NOT_NORMAL | DESC_ERR_DESC_OFFSET_ERR):
		rte_bbdev_log(ERR, "Block_size_k is not a normal value within normal range and queue offset error");
		break;
	case (DESC_ERR_KPAI_NOT_NORMAL | DESC_ERR_DESC_OFFSET_ERR):
		rte_bbdev_log(ERR, "Three_kpai is not a normal value for UL only and queue offset error");
		break;
	case DESC_ERR_DESC_READ_FAIL:
		rte_bbdev_log(ERR, "Unsuccessful completion for descriptor read");
		break;
	case DESC_ERR_DESC_READ_TIMEOUT:
		rte_bbdev_log(ERR, "Descriptor read time-out");
		break;
	case DESC_ERR_DESC_READ_TLP_POISONED:
		rte_bbdev_log(ERR, "Descriptor read TLP poisoned");
		break;
	case DESC_ERR_CB_READ_FAIL:
		rte_bbdev_log(ERR, "Unsuccessful completion for code block");
		break;
	case DESC_ERR_CB_READ_TIMEOUT:
		rte_bbdev_log(ERR, "Code block read time-out");
		break;
	case DESC_ERR_CB_READ_TLP_POISONED:
		rte_bbdev_log(ERR, "Code block read TLP poisoned");
		break;
	default:
		rte_bbdev_log(ERR, "Descriptor error unknown error code %u",
				error_code);
		break;
	}
	return 1;
}

/**
 * Set DMA descriptor for encode operation (1 Code Block)
 *
 * @param op
 *   Pointer to a single encode operation.
 * @param desc
 *   Pointer to DMA descriptor.
 * @param input
 *   Pointer to pointer to input data which will be decoded.
 * @param k
 *   K value (length of input in bits).
 * @param e
 *   E value (length of output in bits).
 * @param ncb
 *   Ncb value (size of the soft buffer).
 * @param out_length
 *   Length of output buffer
 * @param in_offset
 *   Input offset in rte_mbuf structure. It is used for calculating the point
 *   where data is starting.
 * @param out_offset
 *   Output offset in rte_mbuf structure. It is used for calculating the point
 *   where hard output data will be stored.
 * @param cbs_in_op
 *   Number of CBs contained in one operation.
 */
static inline int
fpga_dma_desc_te_fill(struct rte_bbdev_enc_op *op,
		struct fpga_dma_enc_desc *desc, struct rte_mbuf *input,
		struct rte_mbuf *output, uint16_t k, uint16_t e, uint16_t ncb,
		uint32_t in_offset, uint32_t out_offset, uint16_t desc_offset,
		uint8_t cbs_in_op)

{
	/* reset */
	desc->done = 0;
	desc->crc_en = check_bit(op->turbo_enc.op_flags,
		RTE_BBDEV_TURBO_CRC_24B_ATTACH);
	desc->bypass_rm = !check_bit(op->turbo_enc.op_flags,
		RTE_BBDEV_TURBO_RATE_MATCH);
	desc->k = k;
	desc->e = e;
	desc->ncb = ncb;
	desc->rv = op->turbo_enc.rv_index;
	desc->offset = desc_offset;
	/* Set inbound data buffer address */
	desc->in_addr_hi = (uint32_t)(
			rte_pktmbuf_iova_offset(input, in_offset) >> 32);
	desc->in_addr_lw = (uint32_t)(
			rte_pktmbuf_iova_offset(input, in_offset));

	desc->out_addr_hi = (uint32_t)(
			rte_pktmbuf_iova_offset(output, out_offset) >> 32);
	desc->out_addr_lw = (uint32_t)(
			rte_pktmbuf_iova_offset(output, out_offset));

	/* Save software context needed for dequeue */
	desc->op_addr = op;

	/* Set total number of CBs in an op */
	desc->cbs_in_op = cbs_in_op;

	return 0;
}

/**
 * Set DMA descriptor for encode operation (1 Code Block)
 *
 * @param op
 *   Pointer to a single encode operation.
 * @param desc
 *   Pointer to DMA descriptor.
 * @param input
 *   Pointer to pointer to input data which will be decoded.
 * @param in_length
 *   Length of an input.
 * @param k
 *   K value (length of an output in bits).
 * @param in_offset
 *   Input offset in rte_mbuf structure. It is used for calculating the point
 *   where data is starting.
 * @param out_offset
 *   Output offset in rte_mbuf structure. It is used for calculating the point
 *   where hard output data will be stored.
 * @param cbs_in_op
 *   Number of CBs contained in one operation.
 */
static inline int
fpga_dma_desc_td_fill(struct rte_bbdev_dec_op *op,
		struct fpga_dma_dec_desc *desc, struct rte_mbuf *input,
		struct rte_mbuf *output, uint16_t in_length, uint16_t k,
		uint32_t in_offset, uint32_t out_offset, uint16_t desc_offset,
		uint8_t cbs_in_op)
{
	/* reset */
	desc->done = 0;
	/* Set inbound data buffer address */
	desc->in_addr_hi = (uint32_t)(
			rte_pktmbuf_iova_offset(input, in_offset) >> 32);
	desc->in_addr_lw = (uint32_t)(
			rte_pktmbuf_iova_offset(input, in_offset));
	desc->in_len = in_length;
	desc->k = k;
	desc->crc_type = !check_bit(op->turbo_dec.op_flags,
			RTE_BBDEV_TURBO_CRC_TYPE_24B);
	if ((op->turbo_dec.code_block_mode == 0)
		&& !check_bit(op->turbo_dec.op_flags,
		RTE_BBDEV_TURBO_DEC_TB_CRC_24B_KEEP))
		desc->drop_crc = 1;
	desc->max_iter = op->turbo_dec.iter_max * 2;
	desc->offset = desc_offset;
	desc->out_addr_hi = (uint32_t)(
			rte_pktmbuf_iova_offset(output, out_offset) >> 32);
	desc->out_addr_lw = (uint32_t)(
			rte_pktmbuf_iova_offset(output, out_offset));

	/* Save software context needed for dequeue */
	desc->op_addr = op;

	/* Set total number of CBs in an op */
	desc->cbs_in_op = cbs_in_op;

	return 0;
}

#ifdef RTE_LIBRTE_BBDEV_DEBUG
/* Validates turbo encoder parameters */
static int
validate_enc_op(struct rte_bbdev_enc_op *op)
{
	struct rte_bbdev_op_turbo_enc *turbo_enc = &op->turbo_enc;
	struct rte_bbdev_op_enc_turbo_cb_params *cb = NULL;
	struct rte_bbdev_op_enc_turbo_tb_params *tb = NULL;
	uint16_t kw, kw_neg, kw_pos;

	if (turbo_enc->input.length >
			RTE_BBDEV_TURBO_MAX_TB_SIZE >> 3) {
		rte_bbdev_log(ERR, "TB size (%u) is too big, max: %d",
				turbo_enc->input.length,
				RTE_BBDEV_TURBO_MAX_TB_SIZE);
		op->status = 1 << RTE_BBDEV_DATA_ERROR;
		return -1;
	}

	if (op->mempool == NULL) {
		rte_bbdev_log(ERR, "Invalid mempool pointer");
		return -1;
	}
	if (turbo_enc->input.data == NULL) {
		rte_bbdev_log(ERR, "Invalid input pointer");
		return -1;
	}
	if (turbo_enc->output.data == NULL) {
		rte_bbdev_log(ERR, "Invalid output pointer");
		return -1;
	}
	if (turbo_enc->rv_index > 3) {
		rte_bbdev_log(ERR,
				"rv_index (%u) is out of range 0 <= value <= 3",
				turbo_enc->rv_index);
		return -1;
	}
	if (turbo_enc->code_block_mode != 0 &&
			turbo_enc->code_block_mode != 1) {
		rte_bbdev_log(ERR,
				"code_block_mode (%u) is out of range 0 <= value <= 1",
				turbo_enc->code_block_mode);
		return -1;
	}

	if (turbo_enc->code_block_mode == 0) {
		tb = &turbo_enc->tb_params;
		if ((tb->k_neg < RTE_BBDEV_TURBO_MIN_CB_SIZE
				|| tb->k_neg > RTE_BBDEV_TURBO_MAX_CB_SIZE)
				&& tb->c_neg > 0) {
			rte_bbdev_log(ERR,
					"k_neg (%u) is out of range %u <= value <= %u",
					tb->k_neg, RTE_BBDEV_TURBO_MIN_CB_SIZE,
					RTE_BBDEV_TURBO_MAX_CB_SIZE);
			return -1;
		}
		if (tb->k_pos < RTE_BBDEV_TURBO_MIN_CB_SIZE
				|| tb->k_pos > RTE_BBDEV_TURBO_MAX_CB_SIZE) {
			rte_bbdev_log(ERR,
					"k_pos (%u) is out of range %u <= value <= %u",
					tb->k_pos, RTE_BBDEV_TURBO_MIN_CB_SIZE,
					RTE_BBDEV_TURBO_MAX_CB_SIZE);
			return -1;
		}
		if (tb->c_neg > (RTE_BBDEV_TURBO_MAX_CODE_BLOCKS - 1))
			rte_bbdev_log(ERR,
					"c_neg (%u) is out of range 0 <= value <= %u",
					tb->c_neg,
					RTE_BBDEV_TURBO_MAX_CODE_BLOCKS - 1);
		if (tb->c < 1 || tb->c > RTE_BBDEV_TURBO_MAX_CODE_BLOCKS) {
			rte_bbdev_log(ERR,
					"c (%u) is out of range 1 <= value <= %u",
					tb->c, RTE_BBDEV_TURBO_MAX_CODE_BLOCKS);
			return -1;
		}
		if (tb->cab > tb->c) {
			rte_bbdev_log(ERR,
					"cab (%u) is greater than c (%u)",
					tb->cab, tb->c);
			return -1;
		}
		if ((tb->ea < RTE_BBDEV_TURBO_MIN_CB_SIZE || (tb->ea % 2))
				&& tb->r < tb->cab) {
			rte_bbdev_log(ERR,
					"ea (%u) is less than %u or it is not even",
					tb->ea, RTE_BBDEV_TURBO_MIN_CB_SIZE);
			return -1;
		}
		if ((tb->eb < RTE_BBDEV_TURBO_MIN_CB_SIZE || (tb->eb % 2))
				&& tb->c > tb->cab) {
			rte_bbdev_log(ERR,
					"eb (%u) is less than %u or it is not even",
					tb->eb, RTE_BBDEV_TURBO_MIN_CB_SIZE);
			return -1;
		}

		kw_neg = 3 * RTE_ALIGN_CEIL(tb->k_neg + 4,
					RTE_BBDEV_TURBO_C_SUBBLOCK);
		if (tb->ncb_neg < tb->k_neg || tb->ncb_neg > kw_neg) {
			rte_bbdev_log(ERR,
					"ncb_neg (%u) is out of range (%u) k_neg <= value <= (%u) kw_neg",
					tb->ncb_neg, tb->k_neg, kw_neg);
			return -1;
		}

		kw_pos = 3 * RTE_ALIGN_CEIL(tb->k_pos + 4,
					RTE_BBDEV_TURBO_C_SUBBLOCK);
		if (tb->ncb_pos < tb->k_pos || tb->ncb_pos > kw_pos) {
			rte_bbdev_log(ERR,
					"ncb_pos (%u) is out of range (%u) k_pos <= value <= (%u) kw_pos",
					tb->ncb_pos, tb->k_pos, kw_pos);
			return -1;
		}
		if (tb->r > (tb->c - 1)) {
			rte_bbdev_log(ERR,
					"r (%u) is greater than c - 1 (%u)",
					tb->r, tb->c - 1);
			return -1;
		}
	} else {
		cb = &turbo_enc->cb_params;
		if (cb->k < RTE_BBDEV_TURBO_MIN_CB_SIZE
				|| cb->k > RTE_BBDEV_TURBO_MAX_CB_SIZE) {
			rte_bbdev_log(ERR,
					"k (%u) is out of range %u <= value <= %u",
					cb->k, RTE_BBDEV_TURBO_MIN_CB_SIZE,
					RTE_BBDEV_TURBO_MAX_CB_SIZE);
			return -1;
		}

		if (cb->e < RTE_BBDEV_TURBO_MIN_CB_SIZE || (cb->e % 2)) {
			rte_bbdev_log(ERR,
					"e (%u) is less than %u or it is not even",
					cb->e, RTE_BBDEV_TURBO_MIN_CB_SIZE);
			return -1;
		}

		kw = RTE_ALIGN_CEIL(cb->k + 4, RTE_BBDEV_TURBO_C_SUBBLOCK) * 3;
		if (cb->ncb < cb->k || cb->ncb > kw) {
			rte_bbdev_log(ERR,
					"ncb (%u) is out of range (%u) k <= value <= (%u) kw",
					cb->ncb, cb->k, kw);
			return -1;
		}
	}

	return 0;
}
#endif

static inline char *
mbuf_append(struct rte_mbuf *m_head, struct rte_mbuf *m, uint16_t len)
{
	if (unlikely(len > rte_pktmbuf_tailroom(m)))
		return NULL;

	char *tail = (char *)m->buf_addr + m->data_off + m->data_len;
	m->data_len = (uint16_t)(m->data_len + len);
	m_head->pkt_len  = (m_head->pkt_len + len);
	return tail;
}

static inline int
enqueue_enc_one_op_cb(struct fpga_queue *q, struct rte_bbdev_enc_op *op,
		uint16_t desc_offset)
{
	union fpga_dma_desc *desc;
	struct rte_mbuf *input;
	struct rte_mbuf *output;
	int ret;
	uint16_t k, e, ncb, ring_offset;
	uint32_t total_left, in_length, out_length, in_offset, out_offset;

#ifdef RTE_LIBRTE_BBDEV_DEBUG
	/* Validate op structure */
	if (validate_enc_op(op) == -1) {
		rte_bbdev_log(ERR, "Turbo encoder validation failed");
		return -EINVAL;
	}
#endif

	input = op->turbo_enc.input.data;
	output = op->turbo_enc.output.data;
	in_offset = op->turbo_enc.input.offset;
	out_offset = op->turbo_enc.output.offset;
	total_left = op->turbo_enc.input.length;
	k = op->turbo_enc.cb_params.k;
	e = op->turbo_enc.cb_params.e;
	ncb = op->turbo_enc.cb_params.ncb;

	if (check_bit(op->turbo_enc.op_flags, RTE_BBDEV_TURBO_CRC_24B_ATTACH))
		in_length = ((k - 24) >> 3);
	else
		in_length = k >> 3;

	if (check_bit(op->turbo_enc.op_flags, RTE_BBDEV_TURBO_RATE_MATCH))
		out_length = (e + 7) >> 3;
	else
		out_length = (k >> 3) * 3 + 2;

	mbuf_append(output, output, out_length);

	/* Offset into the ring */
	ring_offset = ((q->tail + desc_offset) & q->sw_ring_wrap_mask);
	/* Setup DMA Descriptor */
	desc = q->ring_addr + ring_offset;

	ret = fpga_dma_desc_te_fill(op, &desc->enc_req, input, output, k, e,
			ncb, in_offset, out_offset, ring_offset, 1);
	if (unlikely(ret < 0))
		return ret;

	/* Update lengths */
	total_left -= in_length;
	op->turbo_enc.output.length += out_length;

	if (total_left > 0) {
		rte_bbdev_log(ERR,
			"Mismatch between mbuf length and included CB sizes: mbuf len %u, cb len %u",
				total_left, in_length);
		return -1;
	}

	return 1;
}

static inline int
enqueue_enc_one_op_tb(struct fpga_queue *q, struct rte_bbdev_enc_op *op,
		uint16_t desc_offset, uint8_t cbs_in_op)
{
	union fpga_dma_desc *desc;
	struct rte_mbuf *input, *output_head, *output;
	int ret;
	uint8_t r, c, crc24_bits = 0;
	uint16_t k, e, ncb, ring_offset;
	uint32_t mbuf_total_left, in_length, out_length, in_offset, out_offset;
	uint32_t seg_total_left;
	uint16_t current_enqueued_cbs = 0;

#ifdef RTE_LIBRTE_BBDEV_DEBUG
	/* Validate op structure */
	if (validate_enc_op(op) == -1) {
		rte_bbdev_log(ERR, "Turbo encoder validation failed");
		return -EINVAL;
	}
#endif

	input = op->turbo_enc.input.data;
	output_head = output = op->turbo_enc.output.data;
	in_offset = op->turbo_enc.input.offset;
	out_offset = op->turbo_enc.output.offset;
	mbuf_total_left = op->turbo_enc.input.length;

	c = op->turbo_enc.tb_params.c;
	r = op->turbo_enc.tb_params.r;

	if (check_bit(op->turbo_enc.op_flags, RTE_BBDEV_TURBO_CRC_24B_ATTACH))
		crc24_bits = 24;

	while (mbuf_total_left > 0 && r < c && input != NULL) {
		seg_total_left = rte_pktmbuf_data_len(input) - in_offset;

		e = (r < op->turbo_enc.tb_params.cab) ?
				op->turbo_enc.tb_params.ea :
				op->turbo_enc.tb_params.eb;
		k = (r < op->turbo_enc.tb_params.c_neg) ?
				op->turbo_enc.tb_params.k_neg :
				op->turbo_enc.tb_params.k_pos;
		ncb = (r < op->turbo_enc.tb_params.c_neg) ?
				op->turbo_enc.tb_params.ncb_neg :
				op->turbo_enc.tb_params.ncb_pos;

		in_length = ((k - crc24_bits) >> 3);

		if (check_bit(op->turbo_enc.op_flags,
			RTE_BBDEV_TURBO_RATE_MATCH))
			out_length = (e + 7) >> 3;
		else
			out_length = (k >> 3) * 3 + 2;

		mbuf_append(output_head, output, out_length);

		/* Setup DMA Descriptor */
		ring_offset = ((q->tail + desc_offset) & q->sw_ring_wrap_mask);
		desc = q->ring_addr + ring_offset;
		ret = fpga_dma_desc_te_fill(op, &desc->enc_req, input, output,
				k, e, ncb, in_offset, out_offset, ring_offset,
				cbs_in_op);
		if (unlikely(ret < 0))
			return ret;

		rte_bbdev_log_debug("DMA request desc %p", desc);

		/* Update lengths */
		op->turbo_enc.output.length += out_length;
		mbuf_total_left -= in_length;

		/* Update offsets */
		if (seg_total_left == in_length) {
			/* Go to the next mbuf */
			input = input->next;
			output = output->next;
			in_offset = 0;
			out_offset = 0;
		} else {
			in_offset += in_length;
			out_offset += out_length;
		}

		r++;
		desc_offset++;
		current_enqueued_cbs++;
	}

	if (mbuf_total_left > 0) {
		rte_bbdev_log(ERR,
				"Some date still left for processing: mbuf_total_left = %u",
				mbuf_total_left);
		return -1;
	}

	return current_enqueued_cbs;
}

#ifdef RTE_LIBRTE_BBDEV_DEBUG
/* Validates turbo decoder parameters */
static int
validate_dec_op(struct rte_bbdev_dec_op *op)
{
	struct rte_bbdev_op_turbo_dec *turbo_dec = &op->turbo_dec;
	struct rte_bbdev_op_dec_turbo_cb_params *cb = NULL;
	struct rte_bbdev_op_dec_turbo_tb_params *tb = NULL;

	if (op->mempool == NULL) {
		rte_bbdev_log(ERR, "Invalid mempool pointer");
		return -1;
	}
	if (turbo_dec->input.data == NULL) {
		rte_bbdev_log(ERR, "Invalid input pointer");
		return -1;
	}
	if (turbo_dec->hard_output.data == NULL) {
		rte_bbdev_log(ERR, "Invalid hard_output pointer");
		return -1;
	}
	if (turbo_dec->rv_index > 3) {
		rte_bbdev_log(ERR,
				"rv_index (%u) is out of range 0 <= value <= 3",
				turbo_dec->rv_index);
		return -1;
	}
	if (turbo_dec->iter_min < 1) {
		rte_bbdev_log(ERR,
				"iter_min (%u) is less than 1",
				turbo_dec->iter_min);
		return -1;
	}
	if (turbo_dec->iter_max <= 2) {
		rte_bbdev_log(ERR,
				"iter_max (%u) is less than or equal to 2",
				turbo_dec->iter_max);
		return -1;
	}
	if (turbo_dec->iter_min > turbo_dec->iter_max) {
		rte_bbdev_log(ERR,
				"iter_min (%u) is greater than iter_max (%u)",
				turbo_dec->iter_min, turbo_dec->iter_max);
		return -1;
	}
	if (turbo_dec->code_block_mode != 0 &&
			turbo_dec->code_block_mode != 1) {
		rte_bbdev_log(ERR,
				"code_block_mode (%u) is out of range 0 <= value <= 1",
				turbo_dec->code_block_mode);
		return -1;
	}

	if (turbo_dec->code_block_mode == 0) {

		if ((turbo_dec->op_flags &
			RTE_BBDEV_TURBO_DEC_TB_CRC_24B_KEEP) &&
			!(turbo_dec->op_flags & RTE_BBDEV_TURBO_CRC_TYPE_24B)) {
			rte_bbdev_log(ERR,
				"RTE_BBDEV_TURBO_DEC_TB_CRC_24B_KEEP should accompany RTE_BBDEV_TURBO_CRC_TYPE_24B");
			return -1;
		}

		tb = &turbo_dec->tb_params;
		if ((tb->k_neg < RTE_BBDEV_TURBO_MIN_CB_SIZE
				|| tb->k_neg > RTE_BBDEV_TURBO_MAX_CB_SIZE)
				&& tb->c_neg > 0) {
			rte_bbdev_log(ERR,
					"k_neg (%u) is out of range %u <= value <= %u",
					tb->k_neg, RTE_BBDEV_TURBO_MIN_CB_SIZE,
					RTE_BBDEV_TURBO_MAX_CB_SIZE);
			return -1;
		}
		if ((tb->k_pos < RTE_BBDEV_TURBO_MIN_CB_SIZE
				|| tb->k_pos > RTE_BBDEV_TURBO_MAX_CB_SIZE)
				&& tb->c > tb->c_neg) {
			rte_bbdev_log(ERR,
					"k_pos (%u) is out of range %u <= value <= %u",
					tb->k_pos, RTE_BBDEV_TURBO_MIN_CB_SIZE,
					RTE_BBDEV_TURBO_MAX_CB_SIZE);
			return -1;
		}
		if (tb->c_neg > (RTE_BBDEV_TURBO_MAX_CODE_BLOCKS - 1))
			rte_bbdev_log(ERR,
					"c_neg (%u) is out of range 0 <= value <= %u",
					tb->c_neg,
					RTE_BBDEV_TURBO_MAX_CODE_BLOCKS - 1);
		if (tb->c < 1 || tb->c > RTE_BBDEV_TURBO_MAX_CODE_BLOCKS) {
			rte_bbdev_log(ERR,
					"c (%u) is out of range 1 <= value <= %u",
					tb->c, RTE_BBDEV_TURBO_MAX_CODE_BLOCKS);
			return -1;
		}
		if (tb->cab > tb->c) {
			rte_bbdev_log(ERR,
					"cab (%u) is greater than c (%u)",
					tb->cab, tb->c);
			return -1;
		}
	} else {

		if (turbo_dec->op_flags & RTE_BBDEV_TURBO_DEC_TB_CRC_24B_KEEP) {
			rte_bbdev_log(ERR,
					"RTE_BBDEV_TURBO_DEC_TB_CRC_24B_KEEP is invalid in CB-mode");
			return -1;
		}

		cb = &turbo_dec->cb_params;
		if (cb->k < RTE_BBDEV_TURBO_MIN_CB_SIZE
				|| cb->k > RTE_BBDEV_TURBO_MAX_CB_SIZE) {
			rte_bbdev_log(ERR,
					"k (%u) is out of range %u <= value <= %u",
					cb->k, RTE_BBDEV_TURBO_MIN_CB_SIZE,
					RTE_BBDEV_TURBO_MAX_CB_SIZE);
			return -1;
		}
	}

	return 0;
}
#endif

static inline int
enqueue_dec_one_op_cb(struct fpga_queue *q, struct rte_bbdev_dec_op *op,
		uint16_t desc_offset)
{
	union fpga_dma_desc *desc;
	struct rte_mbuf *input;
	struct rte_mbuf *output;
	int ret;
	uint16_t k, kw, ring_offset;
	uint32_t total_left, in_length, out_length, in_offset, out_offset;

#ifdef RTE_LIBRTE_BBDEV_DEBUG
	/* Validate op structure */
	if (validate_dec_op(op) == -1) {
		rte_bbdev_log(ERR, "Turbo decoder validation failed");
		return -EINVAL;
	}
#endif

	input = op->turbo_dec.input.data;
	output = op->turbo_dec.hard_output.data;
	total_left = op->turbo_dec.input.length;
	in_offset = op->turbo_dec.input.offset;
	out_offset = op->turbo_dec.hard_output.offset;

	k = op->turbo_dec.cb_params.k;
	kw = RTE_ALIGN_CEIL(k + 4, 32) * 3;
	in_length = kw;
	out_length = k >> 3;

	mbuf_append(output, output, out_length);

	/* Setup DMA Descriptor */
	ring_offset = ((q->tail + desc_offset) & q->sw_ring_wrap_mask);
	desc = q->ring_addr + ring_offset;
	ret = fpga_dma_desc_td_fill(op, &desc->dec_req, input, output,
			in_length, k, in_offset, out_offset, ring_offset, 1);
	if (unlikely(ret < 0))
		return ret;

#ifdef RTE_LIBRTE_BBDEV_DEBUG
	print_dma_dec_desc_debug_info(desc);
#endif

	/* Update lengths */
	total_left -= in_length;
	op->turbo_dec.hard_output.length += out_length;

	if (total_left > 0) {
		rte_bbdev_log(ERR,
				"Mismatch between mbuf length and included CB sizes: mbuf len %u, cb len %u",
				total_left, in_length);
		return -1;
	}

	return 1;
}


static inline int
enqueue_dec_one_op_tb(struct fpga_queue *q, struct rte_bbdev_dec_op *op,
		uint16_t desc_offset, uint8_t cbs_in_op)
{
	union fpga_dma_desc *desc;
	struct rte_mbuf *input, *output_head, *output;
	int ret;
	uint8_t r, c;
	uint16_t k, kw, in_length, out_length, ring_offset;
	uint32_t mbuf_total_left, seg_total_left, in_offset, out_offset;
	uint16_t current_enqueued_cbs = 0;
	uint16_t crc24_overlap = 0;

#ifdef RTE_LIBRTE_BBDEV_DEBUG
	/* Validate op structure */
	if (validate_dec_op(op) == -1) {
		rte_bbdev_log(ERR, "Turbo decoder validation failed");
		return -EINVAL;
	}
#endif

	input = op->turbo_dec.input.data;
	output_head = output = op->turbo_dec.hard_output.data;
	mbuf_total_left = op->turbo_dec.input.length;
	in_offset = op->turbo_dec.input.offset;
	out_offset = op->turbo_dec.hard_output.offset;

	if (!check_bit(op->turbo_dec.op_flags,
		RTE_BBDEV_TURBO_DEC_TB_CRC_24B_KEEP))
		crc24_overlap = 24;

	c = op->turbo_dec.tb_params.c;
	r = op->turbo_dec.tb_params.r;

	while (mbuf_total_left > 0 && r < c && input != NULL) {
		seg_total_left = rte_pktmbuf_data_len(input) - in_offset;
		k = (r < op->turbo_dec.tb_params.c_neg) ?
				op->turbo_dec.tb_params.k_neg :
				op->turbo_dec.tb_params.k_pos;
		kw = RTE_ALIGN_CEIL(k + 4, 32) * 3;

		in_length = kw;
		out_length = (k - crc24_overlap) >> 3;

		mbuf_append(output_head, output, out_length);

		if (seg_total_left < in_length) {
			rte_bbdev_log(ERR,
					"Partial CB found in a TB. FPGA Driver doesn't support scatter-gather operations!");
			return -1;
		}

		/* Setup DMA Descriptor */
		ring_offset = ((q->tail + desc_offset) & q->sw_ring_wrap_mask);
		desc = q->ring_addr + ring_offset;
		ret = fpga_dma_desc_td_fill(op, &desc->dec_req, input, output,
				in_length, k, in_offset, out_offset,
				ring_offset, cbs_in_op);
		if (unlikely(ret < 0))
			return ret;

		/* Update lengths */
		ret = rte_pktmbuf_trim(op->turbo_dec.hard_output.data,
				(crc24_overlap >> 3));
#ifdef RTE_LIBRTE_BBDEV_DEBUG
		if (ret < 0) {
			rte_bbdev_log(ERR,
					"The length to remove is greater than the length of the last segment");
			return -EINVAL;
		}
#endif
		op->turbo_dec.hard_output.length += out_length;
		mbuf_total_left -= in_length;

		/* Update offsets */
		if (seg_total_left == in_length) {
			/* Go to the next mbuf */
			input = input->next;
			output = output->next;
			in_offset = 0;
			out_offset = 0;
		} else {
			in_offset += in_length;
			out_offset += out_length;
		}

		r++;
		desc_offset++;
		current_enqueued_cbs++;
	}

	if (mbuf_total_left > 0) {
		rte_bbdev_log(ERR,
				"Some date still left for processing: mbuf_total_left = %u",
				mbuf_total_left);
		return -1;
	}

	return current_enqueued_cbs;
}

static uint16_t
fpga_enqueue_enc(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_enc_op **ops, uint16_t num)
{
	uint8_t cbs_in_op;
	uint16_t i, total_enqueued_cbs = 0;
	int32_t avail;
	int enqueued_cbs;
	struct fpga_queue *q = q_data->queue_private;
	union fpga_dma_desc *desc;

	/* Check if queue is not full */
	if (unlikely(((q->tail + 1) & q->sw_ring_wrap_mask) ==
			q->head_free_desc))
		return 0;

	/* Calculates available space */
	avail = (q->head_free_desc > q->tail) ?
		q->head_free_desc - q->tail - 1 :
		q->ring_ctrl_reg.ring_size + q->head_free_desc - q->tail - 1;

	for (i = 0; i < num; ++i) {
		if (ops[i]->turbo_enc.code_block_mode == 0) {
			cbs_in_op = get_num_cbs_in_op_enc(&ops[i]->turbo_enc);
			/* Check if there is available space for further
			 * processing
			 */
			if (unlikely(avail - cbs_in_op < 0))
				break;
			avail -= cbs_in_op;
			enqueued_cbs = enqueue_enc_one_op_tb(q, ops[i],
					total_enqueued_cbs, cbs_in_op);
		} else {
			/* Check if there is available space for further
			 * processing
			 */
			if (unlikely(avail - 1 < 0))
				break;
			avail -= 1;
			enqueued_cbs = enqueue_enc_one_op_cb(q, ops[i],
					total_enqueued_cbs);
		}

		if (enqueued_cbs < 0)
			break;

		total_enqueued_cbs += enqueued_cbs;

		rte_bbdev_log_debug("enqueuing enc ops [%d/%d] | head %d | tail %d",
				total_enqueued_cbs, num,
				q->head_free_desc, q->tail);
	}

	/* Set interrupt bit for last CB in enqueued ops. FPGA issues interrupt
	 * only when all previous CBs were already processed.
	 */
	desc = q->ring_addr + ((q->tail + total_enqueued_cbs - 1)
			& q->sw_ring_wrap_mask);
	desc->enc_req.irq_en = q->irq_enable;

	fpga_dma_enqueue(q, total_enqueued_cbs, &q_data->queue_stats);

	/* Update stats */
	q_data->queue_stats.enqueued_count += i;
	q_data->queue_stats.enqueue_err_count += num - i;

	return i;
}

static uint16_t
fpga_enqueue_dec(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_dec_op **ops, uint16_t num)
{
	uint8_t cbs_in_op;
	uint16_t i, total_enqueued_cbs = 0;
	int32_t avail;
	int enqueued_cbs;
	struct fpga_queue *q = q_data->queue_private;
	union fpga_dma_desc *desc;

	/* Check if queue is not full */
	if (unlikely(((q->tail + 1) & q->sw_ring_wrap_mask) ==
			q->head_free_desc))
		return 0;

	/* Calculates available space */
	avail = (q->head_free_desc > q->tail) ?
		q->head_free_desc - q->tail - 1 :
		q->ring_ctrl_reg.ring_size + q->head_free_desc - q->tail - 1;

	for (i = 0; i < num; ++i) {
		if (ops[i]->turbo_dec.code_block_mode == 0) {
			cbs_in_op = get_num_cbs_in_op_dec(&ops[i]->turbo_dec);
			/* Check if there is available space for further
			 * processing
			 */
			if (unlikely(avail - cbs_in_op < 0))
				break;
			avail -= cbs_in_op;
			enqueued_cbs = enqueue_dec_one_op_tb(q, ops[i],
					total_enqueued_cbs, cbs_in_op);
		} else {
			/* Check if there is available space for further
			 * processing
			 */
			if (unlikely(avail - 1 < 0))
				break;
			avail -= 1;
			enqueued_cbs = enqueue_dec_one_op_cb(q, ops[i],
					total_enqueued_cbs);
		}

		if (enqueued_cbs < 0)
			break;

		total_enqueued_cbs += enqueued_cbs;

		rte_bbdev_log_debug("enqueuing dec ops [%d/%d] | head %d | tail %d",
				total_enqueued_cbs, num,
				q->head_free_desc, q->tail);
	}

	/* Set interrupt bit for last CB in enqueued ops. FPGA issues interrupt
	 * only when all previous CBs were already processed.
	 */
	desc = q->ring_addr + ((q->tail + total_enqueued_cbs - 1)
			& q->sw_ring_wrap_mask);
	desc->dec_req.irq_en = q->irq_enable;

	fpga_dma_enqueue(q, total_enqueued_cbs, &q_data->queue_stats);

	/* Update stats */
	q_data->queue_stats.enqueued_count += i;
	q_data->queue_stats.enqueue_err_count += num - i;

	return i;
}

static inline int
dequeue_enc_one_op_cb(struct fpga_queue *q, struct rte_bbdev_enc_op **op,
		uint16_t desc_offset)
{
	union fpga_dma_desc *desc;
	int desc_error = 0;

	/* Set current desc */
	desc = q->ring_addr + ((q->head_free_desc + desc_offset)
			& q->sw_ring_wrap_mask);

	/*check if done */
	if (desc->enc_req.done == 0)
		return -1;

	/* make sure the response is read atomically */
	rte_smp_rmb();

	rte_bbdev_log_debug("DMA response desc %p", desc);

	*op = desc->enc_req.op_addr;
	/* Check the decriptor error field, return 1 on error */
	desc_error = check_desc_error(desc->enc_req.error);
	(*op)->status = desc_error << RTE_BBDEV_DATA_ERROR;

	return 1;
}

static inline int
dequeue_enc_one_op_tb(struct fpga_queue *q, struct rte_bbdev_enc_op **op,
		uint16_t desc_offset)
{
	union fpga_dma_desc *desc;
	uint8_t cbs_in_op, cb_idx;
	int desc_error = 0;
	int status = 0;

	/* Set descriptor */
	desc = q->ring_addr + ((q->head_free_desc + desc_offset)
			& q->sw_ring_wrap_mask);

	/* Verify if done bit is set */
	if (desc->enc_req.done == 0)
		return -1;

	/* Make sure the response is read atomically */
	rte_smp_rmb();

	/* Verify if done bit in all CBs is set */
	cbs_in_op = desc->enc_req.cbs_in_op;
	for (cb_idx = 1; cb_idx < cbs_in_op; ++cb_idx) {
		desc = q->ring_addr + ((q->head_free_desc + desc_offset +
				cb_idx) & q->sw_ring_wrap_mask);
		if (desc->enc_req.done == 0)
			return -1;
	}

	/* Make sure the response is read atomically */
	rte_smp_rmb();

	for (cb_idx = 0; cb_idx < cbs_in_op; ++cb_idx) {
		desc = q->ring_addr + ((q->head_free_desc + desc_offset +
				cb_idx) & q->sw_ring_wrap_mask);
		/* Check the decriptor error field, return 1 on error */
		desc_error = check_desc_error(desc->enc_req.error);
		status |=  desc_error << RTE_BBDEV_DATA_ERROR;
		rte_bbdev_log_debug("DMA response desc %p", desc);
	}

	*op = desc->enc_req.op_addr;
	(*op)->status = status;
	return cbs_in_op;
}

static inline int
dequeue_dec_one_op_cb(struct fpga_queue *q, struct rte_bbdev_dec_op **op,
		uint16_t desc_offset)
{
	union fpga_dma_desc *desc;
	int desc_error = 0;
	/* Set descriptor */
	desc = q->ring_addr + ((q->head_free_desc + desc_offset)
			& q->sw_ring_wrap_mask);

	/* Verify done bit is set */
	if (desc->dec_req.done == 0)
		return -1;

	/* make sure the response is read atomically */
	rte_smp_rmb();

#ifdef RTE_LIBRTE_BBDEV_DEBUG
	print_dma_dec_desc_debug_info(desc);

#endif

	*op = desc->dec_req.op_addr;
	/* FPGA reports in half-iterations, from 0 to 31. get ceiling */
	(*op)->turbo_dec.iter_count = (desc->dec_req.iter + 2) >> 1;
	/* crc_pass = 0 when decoder fails */
	(*op)->status = !(desc->dec_req.crc_pass) << RTE_BBDEV_CRC_ERROR;
	/* Check the decriptor error field, return 1 on error */
	desc_error = check_desc_error(desc->enc_req.error);
	(*op)->status |= desc_error << RTE_BBDEV_DATA_ERROR;
	return 1;
}

static inline int
dequeue_dec_one_op_tb(struct fpga_queue *q, struct rte_bbdev_dec_op **op,
		uint16_t desc_offset)
{
	union fpga_dma_desc *desc;
	uint8_t cbs_in_op, cb_idx, iter_count = 0;
	int status = 0;
	int  desc_error = 0;
	/* Set descriptor */
	desc = q->ring_addr + ((q->head_free_desc + desc_offset)
			& q->sw_ring_wrap_mask);

	/* Verify if done bit is set */
	if (desc->dec_req.done == 0)
		return -1;

	/* Make sure the response is read atomically */
	rte_smp_rmb();

	/* Verify if done bit in all CBs is set */
	cbs_in_op = desc->dec_req.cbs_in_op;
	for (cb_idx = 1; cb_idx < cbs_in_op; ++cb_idx) {
		desc = q->ring_addr + ((q->head_free_desc + desc_offset +
				cb_idx) & q->sw_ring_wrap_mask);
		if (desc->dec_req.done == 0)
			return -1;
	}

	/* Make sure the response is read atomically */
	rte_smp_rmb();

	for (cb_idx = 0; cb_idx < cbs_in_op; ++cb_idx) {
		desc = q->ring_addr + ((q->head_free_desc + desc_offset +
				cb_idx) & q->sw_ring_wrap_mask);
		/* get max iter_count for all CBs in op */
		iter_count = RTE_MAX(iter_count, (uint8_t) desc->dec_req.iter);
		/* crc_pass = 0 when decoder fails, one fails all */
		status |= !(desc->dec_req.crc_pass) << RTE_BBDEV_CRC_ERROR;
		/* Check the decriptor error field, return 1 on error */
		desc_error = check_desc_error(desc->enc_req.error);
		status |= desc_error << RTE_BBDEV_DATA_ERROR;
		rte_bbdev_log_debug("DMA response desc %p", desc);
	}

	*op = desc->dec_req.op_addr;

	/* FPGA reports in half-iterations, get ceiling */
	(*op)->turbo_dec.iter_count = (iter_count + 2) >> 1;
	(*op)->status = status;
	return cbs_in_op;
}

static uint16_t
fpga_dequeue_enc(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_enc_op **ops, uint16_t num)
{
	struct fpga_queue *q = q_data->queue_private;
	uint32_t avail = (q->tail - q->head_free_desc) & q->sw_ring_wrap_mask;
	uint16_t i;
	uint16_t dequeued_cbs = 0;
	struct rte_bbdev_enc_op *op;
	int ret;

	for (i = 0; (i < num) && (dequeued_cbs < avail); ++i) {
		op = (q->ring_addr + ((q->head_free_desc + dequeued_cbs)
			& q->sw_ring_wrap_mask))->enc_req.op_addr;
		if (op->turbo_enc.code_block_mode == 0)
			ret = dequeue_enc_one_op_tb(q, &ops[i], dequeued_cbs);
		else
			ret = dequeue_enc_one_op_cb(q, &ops[i], dequeued_cbs);

		if (ret < 0)
			break;

		dequeued_cbs += ret;

		rte_bbdev_log_debug("dequeuing enc ops [%d/%d] | head %d | tail %d",
				dequeued_cbs, num, q->head_free_desc, q->tail);
	}

	/* Update head */
	q->head_free_desc = (q->head_free_desc + dequeued_cbs) &
			q->sw_ring_wrap_mask;

	/* Update stats */
	q_data->queue_stats.dequeued_count += i;

	return i;
}

static uint16_t
fpga_dequeue_dec(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_dec_op **ops, uint16_t num)
{
	struct fpga_queue *q = q_data->queue_private;
	uint32_t avail = (q->tail - q->head_free_desc) & q->sw_ring_wrap_mask;
	uint16_t i;
	uint16_t dequeued_cbs = 0;
	struct rte_bbdev_dec_op *op;
	int ret;

	for (i = 0; (i < num) && (dequeued_cbs < avail); ++i) {
		op = (q->ring_addr + ((q->head_free_desc + dequeued_cbs)
			& q->sw_ring_wrap_mask))->dec_req.op_addr;
		if (op->turbo_dec.code_block_mode == 0)
			ret = dequeue_dec_one_op_tb(q, &ops[i], dequeued_cbs);
		else
			ret = dequeue_dec_one_op_cb(q, &ops[i], dequeued_cbs);

		if (ret < 0)
			break;

		dequeued_cbs += ret;

		rte_bbdev_log_debug("dequeuing dec ops [%d/%d] | head %d | tail %d",
				dequeued_cbs, num, q->head_free_desc, q->tail);
	}

	/* Update head */
	q->head_free_desc = (q->head_free_desc + dequeued_cbs) &
			q->sw_ring_wrap_mask;

	/* Update stats */
	q_data->queue_stats.dequeued_count += i;

	return i;
}

/* Initialization Function */
static void
fpga_lte_fec_init(struct rte_bbdev *dev, struct rte_pci_driver *drv)
{
	struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI(dev->device);

	dev->dev_ops = &fpga_ops;
	dev->enqueue_enc_ops = fpga_enqueue_enc;
	dev->enqueue_dec_ops = fpga_enqueue_dec;
	dev->dequeue_enc_ops = fpga_dequeue_enc;
	dev->dequeue_dec_ops = fpga_dequeue_dec;

	((struct fpga_lte_fec_device *) dev->data->dev_private)->pf_device =
			!strcmp(drv->driver.name,
					RTE_STR(FPGA_LTE_FEC_PF_DRIVER_NAME));
	((struct fpga_lte_fec_device *) dev->data->dev_private)->mmio_base =
			pci_dev->mem_resource[0].addr;

	rte_bbdev_log_debug(
			"Init device %s [%s] @ virtaddr %p phyaddr %#"PRIx64,
			drv->driver.name, dev->data->name,
			(void *)pci_dev->mem_resource[0].addr,
			pci_dev->mem_resource[0].phys_addr);
}

static int
fpga_lte_fec_probe(struct rte_pci_driver *pci_drv,
	struct rte_pci_device *pci_dev)
{
	struct rte_bbdev *bbdev = NULL;
	char dev_name[RTE_BBDEV_NAME_MAX_LEN];

	if (pci_dev == NULL) {
		rte_bbdev_log(ERR, "NULL PCI device");
		return -EINVAL;
	}

	rte_pci_device_name(&pci_dev->addr, dev_name, sizeof(dev_name));

	/* Allocate memory to be used privately by drivers */
	bbdev = rte_bbdev_allocate(pci_dev->device.name);
	if (bbdev == NULL)
		return -ENODEV;

	/* allocate device private memory */
	bbdev->data->dev_private = rte_zmalloc_socket(dev_name,
			sizeof(struct fpga_lte_fec_device), RTE_CACHE_LINE_SIZE,
			pci_dev->device.numa_node);

	if (bbdev->data->dev_private == NULL) {
		rte_bbdev_log(CRIT,
				"Allocate of %zu bytes for device \"%s\" failed",
				sizeof(struct fpga_lte_fec_device), dev_name);
				rte_bbdev_release(bbdev);
			return -ENOMEM;
	}

	/* Fill HW specific part of device structure */
	bbdev->device = &pci_dev->device;
	bbdev->intr_handle = &pci_dev->intr_handle;
	bbdev->data->socket_id = pci_dev->device.numa_node;

	/* Invoke FEC FPGA device initialization function */
	fpga_lte_fec_init(bbdev, pci_drv);

	rte_bbdev_log_debug("bbdev id = %u [%s]",
			bbdev->data->dev_id, dev_name);

	struct fpga_lte_fec_device *d = bbdev->data->dev_private;
	uint32_t version_id = fpga_reg_read_32(d->mmio_base,
			FPGA_LTE_FEC_VERSION_ID);
	rte_bbdev_log(INFO, "FEC FPGA RTL v%u.%u",
		((uint16_t)(version_id >> 16)), ((uint16_t)version_id));

#ifdef RTE_LIBRTE_BBDEV_DEBUG
	if (!strcmp(pci_drv->driver.name,
			RTE_STR(FPGA_LTE_FEC_PF_DRIVER_NAME)))
		print_static_reg_debug_info(d->mmio_base);
#endif
	return 0;
}

static int
fpga_lte_fec_remove(struct rte_pci_device *pci_dev)
{
	struct rte_bbdev *bbdev;
	int ret;
	uint8_t dev_id;

	if (pci_dev == NULL)
		return -EINVAL;

	/* Find device */
	bbdev = rte_bbdev_get_named_dev(pci_dev->device.name);
	if (bbdev == NULL) {
		rte_bbdev_log(CRIT,
				"Couldn't find HW dev \"%s\" to uninitialise it",
				pci_dev->device.name);
		return -ENODEV;
	}
	dev_id = bbdev->data->dev_id;

	/* free device private memory before close */
	rte_free(bbdev->data->dev_private);

	/* Close device */
	ret = rte_bbdev_close(dev_id);
	if (ret < 0)
		rte_bbdev_log(ERR,
				"Device %i failed to close during uninit: %i",
				dev_id, ret);

	/* release bbdev from library */
	ret = rte_bbdev_release(bbdev);
	if (ret)
		rte_bbdev_log(ERR, "Device %i failed to uninit: %i", dev_id,
				ret);

	rte_bbdev_log_debug("Destroyed bbdev = %u", dev_id);

	return 0;
}

static inline void
set_default_fpga_conf(struct rte_fpga_lte_fec_conf *def_conf)
{
	/* clear default configuration before initialization */
	memset(def_conf, 0, sizeof(struct rte_fpga_lte_fec_conf));
	/* Set pf mode to true */
	def_conf->pf_mode_en = true;

	/* Set ratio between UL and DL to 1:1 (unit of weight is 3 CBs) */
	def_conf->ul_bandwidth = 3;
	def_conf->dl_bandwidth = 3;

	/* Set Load Balance Factor to 64 */
	def_conf->dl_load_balance = 64;
	def_conf->ul_load_balance = 64;
}

/* Initial configuration of FPGA LTE FEC device */
int
rte_fpga_lte_fec_configure(const char *dev_name,
		const struct rte_fpga_lte_fec_conf *conf)
{
	uint32_t payload_32, address;
	uint16_t payload_16;
	uint8_t payload_8;
	uint16_t q_id, vf_id, total_q_id, total_ul_q_id, total_dl_q_id;
	struct rte_bbdev *bbdev = rte_bbdev_get_named_dev(dev_name);
	struct rte_fpga_lte_fec_conf def_conf;

	if (bbdev == NULL) {
		rte_bbdev_log(ERR,
				"Invalid dev_name (%s), or device is not yet initialised",
				dev_name);
		return -ENODEV;
	}

	struct fpga_lte_fec_device *d = bbdev->data->dev_private;

	if (conf == NULL) {
		rte_bbdev_log(ERR,
				"FPGA Configuration was not provided. Default configuration will be loaded.");
		set_default_fpga_conf(&def_conf);
		conf = &def_conf;
	}

	/*
	 * Configure UL:DL ratio.
	 * [7:0]: UL weight
	 * [15:8]: DL weight
	 */
	payload_16 = (conf->dl_bandwidth << 8) | conf->ul_bandwidth;
	address = FPGA_LTE_FEC_CONFIGURATION;
	fpga_reg_write_16(d->mmio_base, address, payload_16);

	/* Clear all queues registers */
	payload_32 = FPGA_INVALID_HW_QUEUE_ID;
	for (q_id = 0; q_id < FPGA_TOTAL_NUM_QUEUES; ++q_id) {
		address = (q_id << 2) + FPGA_LTE_FEC_QUEUE_MAP;
		fpga_reg_write_32(d->mmio_base, address, payload_32);
	}

	/*
	 * If PF mode is enabled allocate all queues for PF only.
	 *
	 * For VF mode each VF can have different number of UL and DL queues.
	 * Total number of queues to configure cannot exceed FPGA
	 * capabilities - 64 queues - 32 queues for UL and 32 queues for DL.
	 * Queues mapping is done according to configuration:
	 *
	 * UL queues:
	 * |                Q_ID              | VF_ID |
	 * |                 0                |   0   |
	 * |                ...               |   0   |
	 * | conf->vf_dl_queues_number[0] - 1 |   0   |
	 * | conf->vf_dl_queues_number[0]     |   1   |
	 * |                ...               |   1   |
	 * | conf->vf_dl_queues_number[1] - 1 |   1   |
	 * |                ...               |  ...  |
	 * | conf->vf_dl_queues_number[7] - 1 |   7   |
	 *
	 * DL queues:
	 * |                Q_ID              | VF_ID |
	 * |                 32               |   0   |
	 * |                ...               |   0   |
	 * | conf->vf_ul_queues_number[0] - 1 |   0   |
	 * | conf->vf_ul_queues_number[0]     |   1   |
	 * |                ...               |   1   |
	 * | conf->vf_ul_queues_number[1] - 1 |   1   |
	 * |                ...               |  ...  |
	 * | conf->vf_ul_queues_number[7] - 1 |   7   |
	 *
	 * Example of configuration:
	 * conf->vf_ul_queues_number[0] = 4;  -> 4 UL queues for VF0
	 * conf->vf_dl_queues_number[0] = 4;  -> 4 DL queues for VF0
	 * conf->vf_ul_queues_number[1] = 2;  -> 2 UL queues for VF1
	 * conf->vf_dl_queues_number[1] = 2;  -> 2 DL queues for VF1
	 *
	 * UL:
	 * | Q_ID | VF_ID |
	 * |   0  |   0   |
	 * |   1  |   0   |
	 * |   2  |   0   |
	 * |   3  |   0   |
	 * |   4  |   1   |
	 * |   5  |   1   |
	 *
	 * DL:
	 * | Q_ID | VF_ID |
	 * |  32  |   0   |
	 * |  33  |   0   |
	 * |  34  |   0   |
	 * |  35  |   0   |
	 * |  36  |   1   |
	 * |  37  |   1   |
	 */
	if (conf->pf_mode_en) {
		payload_32 = 0x1;
		for (q_id = 0; q_id < FPGA_TOTAL_NUM_QUEUES; ++q_id) {
			address = (q_id << 2) + FPGA_LTE_FEC_QUEUE_MAP;
			fpga_reg_write_32(d->mmio_base, address, payload_32);
		}
	} else {
		/* Calculate total number of UL and DL queues to configure */
		total_ul_q_id = total_dl_q_id = 0;
		for (vf_id = 0; vf_id < FPGA_LTE_FEC_NUM_VFS; ++vf_id) {
			total_ul_q_id += conf->vf_ul_queues_number[vf_id];
			total_dl_q_id += conf->vf_dl_queues_number[vf_id];
		}
		total_q_id = total_dl_q_id + total_ul_q_id;
		/*
		 * Check if total number of queues to configure does not exceed
		 * FPGA capabilities (64 queues - 32 UL and 32 DL queues)
		 */
		if ((total_ul_q_id > FPGA_NUM_UL_QUEUES) ||
			(total_dl_q_id > FPGA_NUM_DL_QUEUES) ||
			(total_q_id > FPGA_TOTAL_NUM_QUEUES)) {
			rte_bbdev_log(ERR,
					"FPGA Configuration failed. Too many queues to configure: UL_Q %u, DL_Q %u, FPGA_Q %u",
					total_ul_q_id, total_dl_q_id,
					FPGA_TOTAL_NUM_QUEUES);
			return -EINVAL;
		}
		total_ul_q_id = 0;
		for (vf_id = 0; vf_id < FPGA_LTE_FEC_NUM_VFS; ++vf_id) {
			for (q_id = 0; q_id < conf->vf_ul_queues_number[vf_id];
					++q_id, ++total_ul_q_id) {
				address = (total_ul_q_id << 2) +
						FPGA_LTE_FEC_QUEUE_MAP;
				payload_32 = ((0x80 + vf_id) << 16) | 0x1;
				fpga_reg_write_32(d->mmio_base, address,
						payload_32);
			}
		}
		total_dl_q_id = 0;
		for (vf_id = 0; vf_id < FPGA_LTE_FEC_NUM_VFS; ++vf_id) {
			for (q_id = 0; q_id < conf->vf_dl_queues_number[vf_id];
					++q_id, ++total_dl_q_id) {
				address = ((total_dl_q_id + FPGA_NUM_UL_QUEUES)
						<< 2) + FPGA_LTE_FEC_QUEUE_MAP;
				payload_32 = ((0x80 + vf_id) << 16) | 0x1;
				fpga_reg_write_32(d->mmio_base, address,
						payload_32);
			}
		}
	}

	/* Setting Load Balance Factor */
	payload_16 = (conf->dl_load_balance << 8) | (conf->ul_load_balance);
	address = FPGA_LTE_FEC_LOAD_BALANCE_FACTOR;
	fpga_reg_write_16(d->mmio_base, address, payload_16);

	/* Setting length of ring descriptor entry */
	payload_16 = FPGA_RING_DESC_ENTRY_LENGTH;
	address = FPGA_LTE_FEC_RING_DESC_LEN;
	fpga_reg_write_16(d->mmio_base, address, payload_16);

	/* Setting FLR timeout value */
	payload_16 = conf->flr_time_out;
	address = FPGA_LTE_FEC_FLR_TIME_OUT;
	fpga_reg_write_16(d->mmio_base, address, payload_16);

	/* Queue PF/VF mapping table is ready */
	payload_8 = 0x1;
	address = FPGA_LTE_FEC_QUEUE_PF_VF_MAP_DONE;
	fpga_reg_write_8(d->mmio_base, address, payload_8);

	rte_bbdev_log_debug("PF FPGA LTE FEC configuration complete for %s",
			dev_name);

#ifdef RTE_LIBRTE_BBDEV_DEBUG
	print_static_reg_debug_info(d->mmio_base);
#endif
	return 0;
}

/* FPGA LTE FEC PCI PF address map */
static struct rte_pci_id pci_id_fpga_lte_fec_pf_map[] = {
	{
		RTE_PCI_DEVICE(FPGA_LTE_FEC_VENDOR_ID,
				FPGA_LTE_FEC_PF_DEVICE_ID)
	},
	{.device_id = 0},
};

static struct rte_pci_driver fpga_lte_fec_pci_pf_driver = {
	.probe = fpga_lte_fec_probe,
	.remove = fpga_lte_fec_remove,
	.id_table = pci_id_fpga_lte_fec_pf_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING
};

/* FPGA LTE FEC PCI VF address map */
static struct rte_pci_id pci_id_fpga_lte_fec_vf_map[] = {
	{
		RTE_PCI_DEVICE(FPGA_LTE_FEC_VENDOR_ID,
				FPGA_LTE_FEC_VF_DEVICE_ID)
	},
	{.device_id = 0},
};

static struct rte_pci_driver fpga_lte_fec_pci_vf_driver = {
	.probe = fpga_lte_fec_probe,
	.remove = fpga_lte_fec_remove,
	.id_table = pci_id_fpga_lte_fec_vf_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING
};


RTE_PMD_REGISTER_PCI(FPGA_LTE_FEC_PF_DRIVER_NAME, fpga_lte_fec_pci_pf_driver);
RTE_PMD_REGISTER_PCI_TABLE(FPGA_LTE_FEC_PF_DRIVER_NAME,
		pci_id_fpga_lte_fec_pf_map);
RTE_PMD_REGISTER_PCI(FPGA_LTE_FEC_VF_DRIVER_NAME, fpga_lte_fec_pci_vf_driver);
RTE_PMD_REGISTER_PCI_TABLE(FPGA_LTE_FEC_VF_DRIVER_NAME,
		pci_id_fpga_lte_fec_vf_map);
