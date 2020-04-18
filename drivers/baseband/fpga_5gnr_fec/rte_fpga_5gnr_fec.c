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
fpga_dev_close(struct rte_bbdev *dev __rte_unused)
{
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

static const struct rte_bbdev_ops fpga_ops = {
	.close = fpga_dev_close,
	.info_get = fpga_dev_info_get,
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
