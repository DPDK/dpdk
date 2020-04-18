/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
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

#include "fpga_5gnr_fec.h"

/* 5GNR SW PMD logging ID */
static int fpga_5gnr_fec_logtype;

static int
fpga_setup_queues(struct rte_bbdev *dev, uint16_t num_queues, int socket_id)
{
	/* Number of queues bound to a PF/VF */
	uint32_t hw_q_num = 0;
	uint32_t ring_size, payload, address, q_id, offset;
	rte_iova_t phys_addr;
	struct fpga_ring_ctrl_reg ring_reg;
	struct fpga_5gnr_fec_device *fpga_dev = dev->data->dev_private;

	address = FPGA_5GNR_FEC_QUEUE_PF_VF_MAP_DONE;
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
				FPGA_5GNR_FEC_QUEUE_MAP + (q_id << 2));

		rte_bbdev_log_debug("%s: queue ID: %u, registry queue ID: %u",
				dev->device->name, q_id, hw_q_id);

		if (hw_q_id != FPGA_INVALID_HW_QUEUE_ID) {
			fpga_dev->q_bound_bit_map |= (1ULL << q_id);
			/* Clear queue register of found queue */
			offset = FPGA_5GNR_FEC_RING_CTRL_REGS +
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

	address = FPGA_5GNR_FEC_VFQ_FLUSH_STATUS_LW;
	payload = (uint32_t)(phys_addr);
	fpga_reg_write_32(fpga_dev->mmio_base, address, payload);

	address = FPGA_5GNR_FEC_VFQ_FLUSH_STATUS_HI;
	payload = (uint32_t)(phys_addr >> 32);
	fpga_reg_write_32(fpga_dev->mmio_base, address, payload);

	return 0;
}

static int
fpga_dev_close(struct rte_bbdev *dev)
{
	struct fpga_5gnr_fec_device *fpga_dev = dev->data->dev_private;

	rte_free(fpga_dev->sw_rings);
	rte_free(fpga_dev->flush_queue_status);

	return 0;
}

static void
fpga_dev_info_get(struct rte_bbdev *dev,
		struct rte_bbdev_driver_info *dev_info)
{
	struct fpga_5gnr_fec_device *d = dev->data->dev_private;
	uint32_t q_id = 0;

	static const struct rte_bbdev_op_cap bbdev_capabilities[] = {
		{
			.type   = RTE_BBDEV_OP_LDPC_ENC,
			.cap.ldpc_enc = {
				.capability_flags =
						RTE_BBDEV_LDPC_RATE_MATCH |
						RTE_BBDEV_LDPC_ENC_INTERRUPTS |
						RTE_BBDEV_LDPC_CRC_24B_ATTACH,
				.num_buffers_src =
						RTE_BBDEV_LDPC_MAX_CODE_BLOCKS,
				.num_buffers_dst =
						RTE_BBDEV_LDPC_MAX_CODE_BLOCKS,
			}
		},
		{
		.type   = RTE_BBDEV_OP_LDPC_DEC,
		.cap.ldpc_dec = {
			.capability_flags =
				RTE_BBDEV_LDPC_CRC_TYPE_24B_CHECK |
				RTE_BBDEV_LDPC_CRC_TYPE_24B_DROP |
				RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE |
				RTE_BBDEV_LDPC_HQ_COMBINE_OUT_ENABLE |
				RTE_BBDEV_LDPC_ITERATION_STOP_ENABLE |
				RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_IN_ENABLE |
				RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_OUT_ENABLE |
				RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_FILLERS,
			.llr_size = 6,
			.llr_decimals = 2,
			.num_buffers_src =
					RTE_BBDEV_LDPC_MAX_CODE_BLOCKS,
			.num_buffers_hard_out =
					RTE_BBDEV_LDPC_MAX_CODE_BLOCKS,
			.num_buffers_soft_out = 0,
		}
		},
		RTE_BBDEV_END_OF_CAPABILITIES_LIST()
	};

	/* Check the HARQ DDR size available */
	uint8_t timeout_counter = 0;
	uint32_t harq_buf_ready = fpga_reg_read_32(d->mmio_base,
			FPGA_5GNR_FEC_HARQ_BUF_SIZE_RDY_REGS);
	while (harq_buf_ready != 1) {
		usleep(FPGA_TIMEOUT_CHECK_INTERVAL);
		timeout_counter++;
		harq_buf_ready = fpga_reg_read_32(d->mmio_base,
				FPGA_5GNR_FEC_HARQ_BUF_SIZE_RDY_REGS);
		if (timeout_counter > FPGA_HARQ_RDY_TIMEOUT) {
			rte_bbdev_log(ERR, "HARQ Buffer not ready %d",
					harq_buf_ready);
			harq_buf_ready = 1;
		}
	}
	uint32_t harq_buf_size = fpga_reg_read_32(d->mmio_base,
			FPGA_5GNR_FEC_HARQ_BUF_SIZE_REGS);

	static struct rte_bbdev_queue_conf default_queue_conf;
	default_queue_conf.socket = dev->data->socket_id;
	default_queue_conf.queue_size = FPGA_RING_MAX_SIZE;

	dev_info->driver_name = dev->device->driver->name;
	dev_info->queue_size_lim = FPGA_RING_MAX_SIZE;
	dev_info->hardware_accelerated = true;
	dev_info->min_alignment = 64;
	dev_info->harq_buffer_size = (harq_buf_size >> 10) + 1;
	dev_info->default_queue_conf = default_queue_conf;
	dev_info->capabilities = bbdev_capabilities;
	dev_info->cpu_flag_reqs = NULL;

	/* Calculates number of queues assigned to device */
	dev_info->max_num_queues = 0;
	for (q_id = 0; q_id < FPGA_TOTAL_NUM_QUEUES; ++q_id) {
		uint32_t hw_q_id = fpga_reg_read_32(d->mmio_base,
				FPGA_5GNR_FEC_QUEUE_MAP + (q_id << 2));
		if (hw_q_id != FPGA_INVALID_HW_QUEUE_ID)
			dev_info->max_num_queues++;
	}
}

/**
 * Find index of queue bound to current PF/VF which is unassigned. Return -1
 * when there is no available queue
 */
static inline int
fpga_find_free_queue_idx(struct rte_bbdev *dev,
		const struct rte_bbdev_queue_conf *conf)
{
	struct fpga_5gnr_fec_device *d = dev->data->dev_private;
	uint64_t q_idx;
	uint8_t i = 0;
	uint8_t range = FPGA_TOTAL_NUM_QUEUES >> 1;

	if (conf->op_type == RTE_BBDEV_OP_LDPC_ENC) {
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
	struct fpga_5gnr_fec_device *d = dev->data->dev_private;
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
	ring_offset = FPGA_5GNR_FEC_RING_CTRL_REGS +
			(sizeof(struct fpga_ring_ctrl_reg) * q_idx);

	/* Set FPGA Ring Control Registers */
	fpga_ring_reg_write(d->mmio_base, ring_offset, q->ring_ctrl_reg);

	/* Store MMIO register of shadow_tail */
	address = ring_offset + FPGA_5GNR_FEC_RING_SHADOW_TAIL;
	q->shadow_tail_addr = RTE_PTR_ADD(d->mmio_base, address);

	q->head_free_desc = q->tail;

	/* Set wrap mask */
	q->sw_ring_wrap_mask = conf->queue_size - 1;

	rte_bbdev_log_debug("Setup dev%u q%u: queue_idx=%u",
			dev->data->dev_id, queue_id, q->q_idx);

	dev->data->queues[queue_id].queue_private = q;

	rte_bbdev_log_debug("BBDEV queue[%d] set up for FPGA queue[%d]",
			queue_id, q_idx);

	return 0;
}

static int
fpga_queue_release(struct rte_bbdev *dev, uint16_t queue_id)
{
	struct fpga_5gnr_fec_device *d = dev->data->dev_private;
	struct fpga_queue *q = dev->data->queues[queue_id].queue_private;
	struct fpga_ring_ctrl_reg ring_reg;
	uint32_t offset;

	rte_bbdev_log_debug("FPGA Queue[%d] released", queue_id);

	if (q != NULL) {
		memset(&ring_reg, 0, sizeof(struct fpga_ring_ctrl_reg));
		offset = FPGA_5GNR_FEC_RING_CTRL_REGS +
			(sizeof(struct fpga_ring_ctrl_reg) * q->q_idx);
		/* Disable queue */
		fpga_reg_write_8(d->mmio_base,
				offset + FPGA_5GNR_FEC_RING_ENABLE, 0x00);
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
	struct fpga_5gnr_fec_device *d = dev->data->dev_private;
#ifdef RTE_LIBRTE_BBDEV_DEBUG
	if (d == NULL) {
		rte_bbdev_log(ERR, "Invalid device pointer");
		return -1;
	}
#endif
	struct fpga_queue *q = dev->data->queues[queue_id].queue_private;
	uint32_t offset = FPGA_5GNR_FEC_RING_CTRL_REGS +
			(sizeof(struct fpga_ring_ctrl_reg) * q->q_idx);
	uint8_t enable = 0x01;
	uint16_t zero = 0x0000;

	/* Clear queue head and tail variables */
	q->tail = q->head_free_desc = 0;

	/* Clear FPGA head_point and tail registers */
	fpga_reg_write_16(d->mmio_base, offset + FPGA_5GNR_FEC_RING_HEAD_POINT,
			zero);
	fpga_reg_write_16(d->mmio_base, offset + FPGA_5GNR_FEC_RING_SHADOW_TAIL,
			zero);

	/* Enable queue */
	fpga_reg_write_8(d->mmio_base, offset + FPGA_5GNR_FEC_RING_ENABLE,
			enable);

	rte_bbdev_log_debug("FPGA Queue[%d] started", queue_id);
	return 0;
}

/* Function stops a device queue. */
static int
fpga_queue_stop(struct rte_bbdev *dev, uint16_t queue_id)
{
	struct fpga_5gnr_fec_device *d = dev->data->dev_private;
#ifdef RTE_LIBRTE_BBDEV_DEBUG
	if (d == NULL) {
		rte_bbdev_log(ERR, "Invalid device pointer");
		return -1;
	}
#endif
	struct fpga_queue *q = dev->data->queues[queue_id].queue_private;
	uint32_t offset = FPGA_5GNR_FEC_RING_CTRL_REGS +
			(sizeof(struct fpga_ring_ctrl_reg) * q->q_idx);
	uint8_t payload = 0x01;
	uint8_t counter = 0;
	uint8_t timeout = FPGA_QUEUE_FLUSH_TIMEOUT_US /
			FPGA_TIMEOUT_CHECK_INTERVAL;

	/* Set flush_queue_en bit to trigger queue flushing */
	fpga_reg_write_8(d->mmio_base,
			offset + FPGA_5GNR_FEC_RING_FLUSH_QUEUE_EN, payload);

	/** Check if queue flush is completed.
	 * FPGA will update the completion flag after queue flushing is
	 * completed. If completion flag is not updated within 1ms it is
	 * considered as a failure.
	 */
	while (!(*((volatile uint8_t *)d->flush_queue_status + q->q_idx)
			& payload)) {
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
	fpga_reg_write_8(d->mmio_base, offset + FPGA_5GNR_FEC_RING_ENABLE,
			payload);

	rte_bbdev_log_debug("FPGA Queue[%d] stopped", queue_id);
	return 0;
}

static const struct rte_bbdev_ops fpga_ops = {
	.setup_queues = fpga_setup_queues,
	.close = fpga_dev_close,
	.info_get = fpga_dev_info_get,
	.queue_setup = fpga_queue_setup,
	.queue_stop = fpga_queue_stop,
	.queue_start = fpga_queue_start,
	.queue_release = fpga_queue_release,
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

/* Read flag value 0/1/ from bitmap */
static inline bool
check_bit(uint32_t bitmap, uint32_t bitmask)
{
	return bitmap & bitmask;
}

/* Compute value of k0.
 * Based on 3GPP 38.212 Table 5.4.2.1-2
 * Starting position of different redundancy versions, k0
 */
static inline uint16_t
get_k0(uint16_t n_cb, uint16_t z_c, uint8_t bg, uint8_t rv_index)
{
	if (rv_index == 0)
		return 0;
	uint16_t n = (bg == 1 ? N_ZC_1 : N_ZC_2) * z_c;
	if (n_cb == n) {
		if (rv_index == 1)
			return (bg == 1 ? K0_1_1 : K0_1_2) * z_c;
		else if (rv_index == 2)
			return (bg == 1 ? K0_2_1 : K0_2_2) * z_c;
		else
			return (bg == 1 ? K0_3_1 : K0_3_2) * z_c;
	}
	/* LBRM case - includes a division by N */
	if (rv_index == 1)
		return (((bg == 1 ? K0_1_1 : K0_1_2) * n_cb)
				/ n) * z_c;
	else if (rv_index == 2)
		return (((bg == 1 ? K0_2_1 : K0_2_2) * n_cb)
				/ n) * z_c;
	else
		return (((bg == 1 ? K0_3_1 : K0_3_2) * n_cb)
				/ n) * z_c;
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
		struct rte_mbuf *output, uint16_t k_,  uint16_t e,
		uint32_t in_offset, uint32_t out_offset, uint16_t desc_offset,
		uint8_t cbs_in_op)
{
	/* reset */
	desc->done = 0;
	desc->error = 0;
	desc->k_ = k_;
	desc->rm_e = e;
	desc->desc_idx = desc_offset;
	desc->zc = op->ldpc_enc.z_c;
	desc->bg_idx = op->ldpc_enc.basegraph - 1;
	desc->qm_idx = op->ldpc_enc.q_m / 2;
	desc->crc_en = check_bit(op->ldpc_enc.op_flags,
			RTE_BBDEV_LDPC_CRC_24B_ATTACH);
	desc->irq_en = 0;
	desc->k0 = get_k0(op->ldpc_enc.n_cb, op->ldpc_enc.z_c,
			op->ldpc_enc.basegraph, op->ldpc_enc.rv_index);
	desc->ncb = op->ldpc_enc.n_cb;
	desc->num_null = op->ldpc_enc.n_filler;
	/* Set inbound data buffer address */
	desc->in_addr_hi = (uint32_t)(
			rte_pktmbuf_mtophys_offset(input, in_offset) >> 32);
	desc->in_addr_lw = (uint32_t)(
			rte_pktmbuf_mtophys_offset(input, in_offset));

	desc->out_addr_hi = (uint32_t)(
			rte_pktmbuf_mtophys_offset(output, out_offset) >> 32);
	desc->out_addr_lw = (uint32_t)(
			rte_pktmbuf_mtophys_offset(output, out_offset));
	/* Save software context needed for dequeue */
	desc->op_addr = op;
	/* Set total number of CBs in an op */
	desc->cbs_in_op = cbs_in_op;
	return 0;
}

/**
 * Set DMA descriptor for decode operation (1 Code Block)
 *
 * @param op
 *   Pointer to a single encode operation.
 * @param desc
 *   Pointer to DMA descriptor.
 * @param input
 *   Pointer to pointer to input data which will be decoded.
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
fpga_dma_desc_ld_fill(struct rte_bbdev_dec_op *op,
		struct fpga_dma_dec_desc *desc,
		struct rte_mbuf *input,	struct rte_mbuf *output,
		uint16_t harq_in_length,
		uint32_t in_offset, uint32_t out_offset,
		uint32_t harq_offset,
		uint16_t desc_offset,
		uint8_t cbs_in_op)
{
	/* reset */
	desc->done = 0;
	desc->error = 0;
	/* Set inbound data buffer address */
	desc->in_addr_hi = (uint32_t)(
			rte_pktmbuf_mtophys_offset(input, in_offset) >> 32);
	desc->in_addr_lw = (uint32_t)(
			rte_pktmbuf_mtophys_offset(input, in_offset));
	desc->rm_e = op->ldpc_dec.cb_params.e;
	desc->harq_input_length = harq_in_length;
	desc->et_dis = !check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_ITERATION_STOP_ENABLE);
	desc->rv = op->ldpc_dec.rv_index;
	desc->crc24b_ind = check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_CRC_TYPE_24B_CHECK);
	desc->drop_crc24b = check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_CRC_TYPE_24B_DROP);
	desc->desc_idx = desc_offset;
	desc->ncb = op->ldpc_dec.n_cb;
	desc->num_null = op->ldpc_dec.n_filler;
	desc->hbstroe_offset = harq_offset >> 10;
	desc->zc = op->ldpc_dec.z_c;
	desc->harqin_en = check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE);
	desc->bg_idx = op->ldpc_dec.basegraph - 1;
	desc->max_iter = op->ldpc_dec.iter_max;
	desc->qm_idx = op->ldpc_dec.q_m / 2;
	desc->out_addr_hi = (uint32_t)(
			rte_pktmbuf_mtophys_offset(output, out_offset) >> 32);
	desc->out_addr_lw = (uint32_t)(
			rte_pktmbuf_mtophys_offset(output, out_offset));
	/* Save software context needed for dequeue */
	desc->op_addr = op;
	/* Set total number of CBs in an op */
	desc->cbs_in_op = cbs_in_op;

	return 0;
}

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
enqueue_ldpc_enc_one_op_cb(struct fpga_queue *q, struct rte_bbdev_enc_op *op,
		uint16_t desc_offset)
{
	union fpga_dma_desc *desc;
	int ret;
	uint8_t c, crc24_bits = 0;
	struct rte_bbdev_op_ldpc_enc *enc = &op->ldpc_enc;
	uint16_t in_offset = enc->input.offset;
	uint16_t out_offset = enc->output.offset;
	struct rte_mbuf *m_in = enc->input.data;
	struct rte_mbuf *m_out = enc->output.data;
	struct rte_mbuf *m_out_head = enc->output.data;
	uint32_t in_length, out_length, e;
	uint16_t total_left = enc->input.length;
	uint16_t ring_offset;
	uint16_t K, k_;

	/* Clear op status */
	op->status = 0;

	if (m_in == NULL || m_out == NULL) {
		rte_bbdev_log(ERR, "Invalid mbuf pointer");
		op->status = 1 << RTE_BBDEV_DATA_ERROR;
		return -EINVAL;
	}

	if (enc->op_flags & RTE_BBDEV_LDPC_CRC_24B_ATTACH)
		crc24_bits = 24;

	if (enc->code_block_mode == 0) {
		/* For Transport Block mode */
		/* FIXME */
		c = enc->tb_params.c;
		e = enc->tb_params.ea;
	} else { /* For Code Block mode */
		c = 1;
		e = enc->cb_params.e;
	}

	/* Update total_left */
	K = (enc->basegraph == 1 ? 22 : 10) * enc->z_c;
	k_ = K - enc->n_filler;
	in_length = (k_ - crc24_bits) >> 3;
	out_length = (e + 7) >> 3;

	total_left = rte_pktmbuf_data_len(m_in) - in_offset;

	/* Update offsets */
	if (total_left != in_length) {
		op->status |= 1 << RTE_BBDEV_DATA_ERROR;
		rte_bbdev_log(ERR,
				"Mismatch between mbuf length and included CBs sizes %d",
				total_left);
	}

	mbuf_append(m_out_head, m_out, out_length);

	/* Offset into the ring */
	ring_offset = ((q->tail + desc_offset) & q->sw_ring_wrap_mask);
	/* Setup DMA Descriptor */
	desc = q->ring_addr + ring_offset;

	ret = fpga_dma_desc_te_fill(op, &desc->enc_req, m_in, m_out,
			k_, e, in_offset, out_offset, ring_offset, c);
	if (unlikely(ret < 0))
		return ret;

	/* Update lengths */
	total_left -= in_length;
	op->ldpc_enc.output.length += out_length;

	if (total_left > 0) {
		rte_bbdev_log(ERR,
			"Mismatch between mbuf length and included CB sizes: mbuf len %u, cb len %u",
				total_left, in_length);
		return -1;
	}

	return 1;
}

static inline int
enqueue_ldpc_dec_one_op_cb(struct fpga_queue *q, struct rte_bbdev_dec_op *op,
		uint16_t desc_offset)
{
	union fpga_dma_desc *desc;
	int ret;
	uint16_t ring_offset;
	uint8_t c;
	uint16_t e, in_length, out_length, k0, l, seg_total_left, sys_cols;
	uint16_t K, parity_offset, harq_in_length = 0, harq_out_length = 0;
	uint16_t crc24_overlap = 0;
	struct rte_bbdev_op_ldpc_dec *dec = &op->ldpc_dec;
	struct rte_mbuf *m_in = dec->input.data;
	struct rte_mbuf *m_out = dec->hard_output.data;
	struct rte_mbuf *m_out_head = dec->hard_output.data;
	uint16_t in_offset = dec->input.offset;
	uint16_t out_offset = dec->hard_output.offset;
	uint32_t harq_offset = 0;

	/* Clear op status */
	op->status = 0;

	/* Setup DMA Descriptor */
	ring_offset = ((q->tail + desc_offset) & q->sw_ring_wrap_mask);
	desc = q->ring_addr + ring_offset;

	if (m_in == NULL || m_out == NULL) {
		rte_bbdev_log(ERR, "Invalid mbuf pointer");
		op->status = 1 << RTE_BBDEV_DATA_ERROR;
		return -1;
	}

	c = 1;
	e = dec->cb_params.e;

	if (check_bit(dec->op_flags, RTE_BBDEV_LDPC_CRC_TYPE_24B_DROP))
		crc24_overlap = 24;

	sys_cols = (dec->basegraph == 1) ? 22 : 10;
	K = sys_cols * dec->z_c;
	parity_offset = K - 2 * dec->z_c;

	out_length = ((K - crc24_overlap - dec->n_filler) >> 3);
	in_length = e;
	seg_total_left = dec->input.length;

	if (check_bit(dec->op_flags, RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE)) {
		harq_in_length = RTE_MIN(dec->harq_combined_input.length,
				(uint32_t)dec->n_cb);
	}

	if (check_bit(dec->op_flags, RTE_BBDEV_LDPC_HQ_COMBINE_OUT_ENABLE)) {
		k0 = get_k0(dec->n_cb, dec->z_c,
				dec->basegraph, dec->rv_index);
		if (k0 > parity_offset)
			l = k0 + e;
		else
			l = k0 + e + dec->n_filler;
		harq_out_length = RTE_MIN(RTE_MAX(harq_in_length, l),
				dec->n_cb - dec->n_filler);
		dec->harq_combined_output.length = harq_out_length;
	}

	mbuf_append(m_out_head, m_out, out_length);
	if (check_bit(dec->op_flags, RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE))
		harq_offset = dec->harq_combined_input.offset;
	else if (check_bit(dec->op_flags, RTE_BBDEV_LDPC_HQ_COMBINE_OUT_ENABLE))
		harq_offset = dec->harq_combined_output.offset;

	if ((harq_offset & 0x3FF) > 0) {
		rte_bbdev_log(ERR, "Invalid HARQ offset %d", harq_offset);
		op->status = 1 << RTE_BBDEV_DATA_ERROR;
		return -1;
	}

	ret = fpga_dma_desc_ld_fill(op, &desc->dec_req, m_in, m_out,
		harq_in_length, in_offset, out_offset, harq_offset,
		ring_offset, c);
	if (unlikely(ret < 0))
		return ret;
	/* Update lengths */
	seg_total_left -= in_length;
	op->ldpc_dec.hard_output.length += out_length;
	if (seg_total_left > 0) {
		rte_bbdev_log(ERR,
				"Mismatch between mbuf length and included CB sizes: mbuf len %u, cb len %u",
				seg_total_left, in_length);
		return -1;
	}

	return 1;
}

static uint16_t
fpga_enqueue_ldpc_enc(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_enc_op **ops, uint16_t num)
{
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

		/* Check if there is available space for further
		 * processing
		 */
		if (unlikely(avail - 1 < 0))
			break;
		avail -= 1;
		enqueued_cbs = enqueue_ldpc_enc_one_op_cb(q, ops[i],
				total_enqueued_cbs);

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
fpga_enqueue_ldpc_dec(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_dec_op **ops, uint16_t num)
{
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

		/* Check if there is available space for further
		 * processing
		 */
		if (unlikely(avail - 1 < 0))
			break;
		avail -= 1;
		enqueued_cbs = enqueue_ldpc_dec_one_op_cb(q, ops[i],
				total_enqueued_cbs);

		if (enqueued_cbs < 0)
			break;

		total_enqueued_cbs += enqueued_cbs;

		rte_bbdev_log_debug("enqueuing dec ops [%d/%d] | head %d | tail %d",
				total_enqueued_cbs, num,
				q->head_free_desc, q->tail);
	}

	/* Update stats */
	q_data->queue_stats.enqueued_count += i;
	q_data->queue_stats.enqueue_err_count += num - i;

	/* Set interrupt bit for last CB in enqueued ops. FPGA issues interrupt
	 * only when all previous CBs were already processed.
	 */
	desc = q->ring_addr + ((q->tail + total_enqueued_cbs - 1)
			& q->sw_ring_wrap_mask);
	desc->enc_req.irq_en = q->irq_enable;
	fpga_dma_enqueue(q, total_enqueued_cbs, &q_data->queue_stats);
	return i;
}


static inline int
dequeue_ldpc_enc_one_op_cb(struct fpga_queue *q,
		struct rte_bbdev_enc_op **op __rte_unused,
		uint16_t desc_offset)
{
	union fpga_dma_desc *desc;

	/* Set current desc */
	desc = q->ring_addr + ((q->head_free_desc + desc_offset)
			& q->sw_ring_wrap_mask);

	/*check if done */
	if (desc->enc_req.done == 0)
		return -1;

	/* make sure the response is read atomically */
	rte_smp_rmb();

	rte_bbdev_log_debug("DMA response desc %p", desc);

	return 1;
}


static inline int
dequeue_ldpc_dec_one_op_cb(struct fpga_queue *q, struct rte_bbdev_dec_op **op,
		uint16_t desc_offset)
{
	union fpga_dma_desc *desc;

	/* Set descriptor */
	desc = q->ring_addr + ((q->head_free_desc + desc_offset)
			& q->sw_ring_wrap_mask);

	/* Verify done bit is set */
	if (desc->dec_req.done == 0)
		return -1;

	/* make sure the response is read atomically */
	rte_smp_rmb();

	*op = desc->dec_req.op_addr;

	if (check_bit((*op)->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_LOOPBACK)) {
		(*op)->status = 0;
		return 1;
	}

	/* FPGA reports iterations based on round-up minus 1 */
	(*op)->ldpc_dec.iter_count = desc->dec_req.iter + 1;
	/* CRC Check criteria */
	if (desc->dec_req.crc24b_ind && !(desc->dec_req.crcb_pass))
		(*op)->status = 1 << RTE_BBDEV_CRC_ERROR;
	/* et_pass = 0 when decoder fails */
	(*op)->status |= !(desc->dec_req.et_pass) << RTE_BBDEV_SYNDROME_ERROR;
	return 1;
}

static uint16_t
fpga_dequeue_ldpc_enc(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_enc_op **ops, uint16_t num)
{
	struct fpga_queue *q = q_data->queue_private;
	uint32_t avail = (q->tail - q->head_free_desc) & q->sw_ring_wrap_mask;
	uint16_t i;
	uint16_t dequeued_cbs = 0;
	int ret;

	for (i = 0; (i < num) && (dequeued_cbs < avail); ++i) {
		ret = dequeue_ldpc_enc_one_op_cb(q, &ops[i], dequeued_cbs);

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
fpga_dequeue_ldpc_dec(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_dec_op **ops, uint16_t num)
{
	struct fpga_queue *q = q_data->queue_private;
	uint32_t avail = (q->tail - q->head_free_desc) & q->sw_ring_wrap_mask;
	uint16_t i;
	uint16_t dequeued_cbs = 0;
	int ret;

	for (i = 0; (i < num) && (dequeued_cbs < avail); ++i) {
		ret = dequeue_ldpc_dec_one_op_cb(q, &ops[i], dequeued_cbs);

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
fpga_5gnr_fec_init(struct rte_bbdev *dev, struct rte_pci_driver *drv)
{
	struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI(dev->device);

	dev->dev_ops = &fpga_ops;
	dev->enqueue_ldpc_enc_ops = fpga_enqueue_ldpc_enc;
	dev->enqueue_ldpc_dec_ops = fpga_enqueue_ldpc_dec;
	dev->dequeue_ldpc_enc_ops = fpga_dequeue_ldpc_enc;
	dev->dequeue_ldpc_dec_ops = fpga_dequeue_ldpc_dec;

	((struct fpga_5gnr_fec_device *) dev->data->dev_private)->pf_device =
			!strcmp(drv->driver.name,
					RTE_STR(FPGA_5GNR_FEC_PF_DRIVER_NAME));
	((struct fpga_5gnr_fec_device *) dev->data->dev_private)->mmio_base =
			pci_dev->mem_resource[0].addr;

	rte_bbdev_log_debug(
			"Init device %s [%s] @ virtaddr %p phyaddr %#"PRIx64,
			dev->device->driver->name, dev->data->name,
			(void *)pci_dev->mem_resource[0].addr,
			pci_dev->mem_resource[0].phys_addr);
}

static int
fpga_5gnr_fec_probe(struct rte_pci_driver *pci_drv,
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
			sizeof(struct fpga_5gnr_fec_device),
			RTE_CACHE_LINE_SIZE,
			pci_dev->device.numa_node);

	if (bbdev->data->dev_private == NULL) {
		rte_bbdev_log(CRIT,
				"Allocate of %zu bytes for device \"%s\" failed",
				sizeof(struct fpga_5gnr_fec_device), dev_name);
				rte_bbdev_release(bbdev);
			return -ENOMEM;
	}

	/* Fill HW specific part of device structure */
	bbdev->device = &pci_dev->device;
	bbdev->intr_handle = &pci_dev->intr_handle;
	bbdev->data->socket_id = pci_dev->device.numa_node;

	/* Invoke FEC FPGA device initialization function */
	fpga_5gnr_fec_init(bbdev, pci_drv);

	rte_bbdev_log_debug("bbdev id = %u [%s]",
			bbdev->data->dev_id, dev_name);

	return 0;
}

static int
fpga_5gnr_fec_remove(struct rte_pci_device *pci_dev)
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

/* FPGA 5GNR FEC PCI PF address map */
static struct rte_pci_id pci_id_fpga_5gnr_fec_pf_map[] = {
	{
		RTE_PCI_DEVICE(FPGA_5GNR_FEC_VENDOR_ID,
				FPGA_5GNR_FEC_PF_DEVICE_ID)
	},
	{.device_id = 0},
};

static struct rte_pci_driver fpga_5gnr_fec_pci_pf_driver = {
	.probe = fpga_5gnr_fec_probe,
	.remove = fpga_5gnr_fec_remove,
	.id_table = pci_id_fpga_5gnr_fec_pf_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING
};

/* FPGA 5GNR FEC PCI VF address map */
static struct rte_pci_id pci_id_fpga_5gnr_fec_vf_map[] = {
	{
		RTE_PCI_DEVICE(FPGA_5GNR_FEC_VENDOR_ID,
				FPGA_5GNR_FEC_VF_DEVICE_ID)
	},
	{.device_id = 0},
};

static struct rte_pci_driver fpga_5gnr_fec_pci_vf_driver = {
	.probe = fpga_5gnr_fec_probe,
	.remove = fpga_5gnr_fec_remove,
	.id_table = pci_id_fpga_5gnr_fec_vf_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING
};


RTE_PMD_REGISTER_PCI(FPGA_5GNR_FEC_PF_DRIVER_NAME, fpga_5gnr_fec_pci_pf_driver);
RTE_PMD_REGISTER_PCI_TABLE(FPGA_5GNR_FEC_PF_DRIVER_NAME,
		pci_id_fpga_5gnr_fec_pf_map);
RTE_PMD_REGISTER_PCI(FPGA_5GNR_FEC_VF_DRIVER_NAME, fpga_5gnr_fec_pci_vf_driver);
RTE_PMD_REGISTER_PCI_TABLE(FPGA_5GNR_FEC_VF_DRIVER_NAME,
		pci_id_fpga_5gnr_fec_vf_map);

RTE_INIT(fpga_5gnr_fec_init_log)
{
	fpga_5gnr_fec_logtype = rte_log_register("pmd.bb.fpga_5gnr_fec");
	if (fpga_5gnr_fec_logtype >= 0)
#ifdef RTE_LIBRTE_BBDEV_DEBUG
		rte_log_set_level(fpga_5gnr_fec_logtype, RTE_LOG_DEBUG);
#else
		rte_log_set_level(fpga_5gnr_fec_logtype, RTE_LOG_NOTICE);
#endif
}
