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

/* Initialization Function */
static void
fpga_5gnr_fec_init(struct rte_bbdev *dev, struct rte_pci_driver *drv)
{
	struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI(dev->device);

	dev->dev_ops = &fpga_ops;

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
