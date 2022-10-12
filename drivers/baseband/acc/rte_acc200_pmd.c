/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#include <unistd.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_dev.h>
#include <rte_malloc.h>
#include <rte_mempool.h>
#include <rte_byteorder.h>
#include <rte_errno.h>
#include <rte_branch_prediction.h>
#include <rte_hexdump.h>
#include <rte_pci.h>
#include <rte_bus_pci.h>
#ifdef RTE_BBDEV_OFFLOAD_COST
#include <rte_cycles.h>
#endif

#include <rte_bbdev.h>
#include <rte_bbdev_pmd.h>
#include "acc200_pmd.h"

#ifdef RTE_LIBRTE_BBDEV_DEBUG
RTE_LOG_REGISTER_DEFAULT(acc200_logtype, DEBUG);
#else
RTE_LOG_REGISTER_DEFAULT(acc200_logtype, NOTICE);
#endif

/* Calculate the offset of the enqueue register. */
static inline uint32_t
queue_offset(bool pf_device, uint8_t vf_id, uint8_t qgrp_id, uint16_t aq_id)
{
	if (pf_device)
		return ((vf_id << 12) + (qgrp_id << 7) + (aq_id << 3) +
				HWPfQmgrIngressAq);
	else
		return ((qgrp_id << 7) + (aq_id << 3) +
				HWVfQmgrIngressAq);
}

enum {UL_4G = 0, UL_5G, DL_4G, DL_5G, FFT, NUM_ACC};

/* Return the queue topology for a Queue Group Index. */
static inline void
qtopFromAcc(struct rte_acc_queue_topology **qtop, int acc_enum, struct rte_acc_conf *acc_conf)
{
	struct rte_acc_queue_topology *p_qtop;
	p_qtop = NULL;

	switch (acc_enum) {
	case UL_4G:
		p_qtop = &(acc_conf->q_ul_4g);
		break;
	case UL_5G:
		p_qtop = &(acc_conf->q_ul_5g);
		break;
	case DL_4G:
		p_qtop = &(acc_conf->q_dl_4g);
		break;
	case DL_5G:
		p_qtop = &(acc_conf->q_dl_5g);
		break;
	case FFT:
		p_qtop = &(acc_conf->q_fft);
		break;
	default:
		/* NOTREACHED. */
		rte_bbdev_log(ERR, "Unexpected error evaluating %s using %d", __func__, acc_enum);
		break;
	}
	*qtop = p_qtop;
}

static void
initQTop(struct rte_acc_conf *acc_conf)
{
	acc_conf->q_ul_4g.num_aqs_per_groups = 0;
	acc_conf->q_ul_4g.num_qgroups = 0;
	acc_conf->q_ul_4g.first_qgroup_index = -1;
	acc_conf->q_ul_5g.num_aqs_per_groups = 0;
	acc_conf->q_ul_5g.num_qgroups = 0;
	acc_conf->q_ul_5g.first_qgroup_index = -1;
	acc_conf->q_dl_4g.num_aqs_per_groups = 0;
	acc_conf->q_dl_4g.num_qgroups = 0;
	acc_conf->q_dl_4g.first_qgroup_index = -1;
	acc_conf->q_dl_5g.num_aqs_per_groups = 0;
	acc_conf->q_dl_5g.num_qgroups = 0;
	acc_conf->q_dl_5g.first_qgroup_index = -1;
	acc_conf->q_fft.num_aqs_per_groups = 0;
	acc_conf->q_fft.num_qgroups = 0;
	acc_conf->q_fft.first_qgroup_index = -1;
}

static inline void
updateQtop(uint8_t acc, uint8_t qg, struct rte_acc_conf *acc_conf, struct acc_device *d) {
	uint32_t reg;
	struct rte_acc_queue_topology *q_top = NULL;
	uint16_t aq;

	qtopFromAcc(&q_top, acc, acc_conf);
	if (unlikely(q_top == NULL))
		return;
	q_top->num_qgroups++;
	if (q_top->first_qgroup_index == -1) {
		q_top->first_qgroup_index = qg;
		/* Can be optimized to assume all are enabled by default. */
		reg = acc_reg_read(d, queue_offset(d->pf_device, 0, qg, ACC200_NUM_AQS - 1));
		if (reg & ACC_QUEUE_ENABLE) {
			q_top->num_aqs_per_groups = ACC200_NUM_AQS;
			return;
		}
		q_top->num_aqs_per_groups = 0;
		for (aq = 0; aq < ACC200_NUM_AQS; aq++) {
			reg = acc_reg_read(d, queue_offset(d->pf_device, 0, qg, aq));
			if (reg & ACC_QUEUE_ENABLE)
				q_top->num_aqs_per_groups++;
		}
	}
}

/* Fetch configuration enabled for the PF/VF using MMIO Read (slow). */
static inline void
fetch_acc200_config(struct rte_bbdev *dev)
{
	struct acc_device *d = dev->data->dev_private;
	struct rte_acc_conf *acc_conf = &d->acc_conf;
	const struct acc200_registry_addr *reg_addr;
	uint8_t acc, qg;
	uint32_t reg_aq, reg_len0, reg_len1, reg0, reg1;
	uint32_t reg_mode, idx;
	struct rte_acc_queue_topology *q_top = NULL;
	int qman_func_id[ACC200_NUM_ACCS] = {ACC_ACCMAP_0, ACC_ACCMAP_1,
			ACC_ACCMAP_2, ACC_ACCMAP_3, ACC_ACCMAP_4};

	/* No need to retrieve the configuration is already done. */
	if (d->configured)
		return;

	/* Choose correct registry addresses for the device type. */
	if (d->pf_device)
		reg_addr = &pf_reg_addr;
	else
		reg_addr = &vf_reg_addr;

	d->ddr_size = 0;

	/* Single VF Bundle by VF. */
	acc_conf->num_vf_bundles = 1;
	initQTop(acc_conf);

	reg0 = acc_reg_read(d, reg_addr->qman_group_func);
	reg1 = acc_reg_read(d, reg_addr->qman_group_func + 4);
	for (qg = 0; qg < ACC200_NUM_QGRPS; qg++) {
		reg_aq = acc_reg_read(d, queue_offset(d->pf_device, 0, qg, 0));
		if (reg_aq & ACC_QUEUE_ENABLE) {
			if (qg < ACC_NUM_QGRPS_PER_WORD)
				idx = (reg0 >> (qg * 4)) & 0x7;
			else
				idx = (reg1 >> ((qg -
					ACC_NUM_QGRPS_PER_WORD) * 4)) & 0x7;
			if (idx < ACC200_NUM_ACCS) {
				acc = qman_func_id[idx];
				updateQtop(acc, qg, acc_conf, d);
			}
		}
	}

	/* Check the depth of the AQs. */
	reg_len0 = acc_reg_read(d, reg_addr->depth_log0_offset);
	reg_len1 = acc_reg_read(d, reg_addr->depth_log1_offset);
	for (acc = 0; acc < NUM_ACC; acc++) {
		qtopFromAcc(&q_top, acc, acc_conf);
		if (q_top->first_qgroup_index < ACC_NUM_QGRPS_PER_WORD)
			q_top->aq_depth_log2 = (reg_len0 >> (q_top->first_qgroup_index * 4)) & 0xF;
		else
			q_top->aq_depth_log2 = (reg_len1 >> ((q_top->first_qgroup_index -
					ACC_NUM_QGRPS_PER_WORD) * 4)) & 0xF;
	}

	/* Read PF mode. */
	if (d->pf_device) {
		reg_mode = acc_reg_read(d, HWPfHiPfMode);
		acc_conf->pf_mode_en = (reg_mode == ACC_PF_VAL) ? 1 : 0;
	} else {
		reg_mode = acc_reg_read(d, reg_addr->hi_mode);
		acc_conf->pf_mode_en = reg_mode & 1;
	}

	rte_bbdev_log_debug(
			"%s Config LLR SIGN IN/OUT %s %s QG %u %u %u %u %u AQ %u %u %u %u %u Len %u %u %u %u %u\n",
			(d->pf_device) ? "PF" : "VF",
			(acc_conf->input_pos_llr_1_bit) ? "POS" : "NEG",
			(acc_conf->output_pos_llr_1_bit) ? "POS" : "NEG",
			acc_conf->q_ul_4g.num_qgroups,
			acc_conf->q_dl_4g.num_qgroups,
			acc_conf->q_ul_5g.num_qgroups,
			acc_conf->q_dl_5g.num_qgroups,
			acc_conf->q_fft.num_qgroups,
			acc_conf->q_ul_4g.num_aqs_per_groups,
			acc_conf->q_dl_4g.num_aqs_per_groups,
			acc_conf->q_ul_5g.num_aqs_per_groups,
			acc_conf->q_dl_5g.num_aqs_per_groups,
			acc_conf->q_fft.num_aqs_per_groups,
			acc_conf->q_ul_4g.aq_depth_log2,
			acc_conf->q_dl_4g.aq_depth_log2,
			acc_conf->q_ul_5g.aq_depth_log2,
			acc_conf->q_dl_5g.aq_depth_log2,
			acc_conf->q_fft.aq_depth_log2);
}

/* Free memory used for software rings. */
static int
acc200_dev_close(struct rte_bbdev *dev)
{
	RTE_SET_USED(dev);
	/* Ensure all in flight HW transactions are completed. */
	usleep(ACC_LONG_WAIT);
	return 0;
}

/* Get ACC200 device info. */
static void
acc200_dev_info_get(struct rte_bbdev *dev,
		struct rte_bbdev_driver_info *dev_info)
{
	struct acc_device *d = dev->data->dev_private;
	int i;
	static const struct rte_bbdev_op_cap bbdev_capabilities[] = {
		RTE_BBDEV_END_OF_CAPABILITIES_LIST()
	};

	static struct rte_bbdev_queue_conf default_queue_conf;
	default_queue_conf.socket = dev->data->socket_id;
	default_queue_conf.queue_size = ACC_MAX_QUEUE_DEPTH;

	dev_info->driver_name = dev->device->driver->name;

	/* Read and save the populated config from ACC200 registers. */
	fetch_acc200_config(dev);

	/* Exposed number of queues. */
	dev_info->num_queues[RTE_BBDEV_OP_NONE] = 0;
	dev_info->num_queues[RTE_BBDEV_OP_TURBO_DEC] = 0;
	dev_info->num_queues[RTE_BBDEV_OP_TURBO_ENC] = 0;
	dev_info->num_queues[RTE_BBDEV_OP_LDPC_DEC] = 0;
	dev_info->num_queues[RTE_BBDEV_OP_LDPC_ENC] = 0;
	dev_info->num_queues[RTE_BBDEV_OP_FFT] = 0;
	dev_info->queue_priority[RTE_BBDEV_OP_TURBO_DEC] = 0;
	dev_info->queue_priority[RTE_BBDEV_OP_TURBO_ENC] = 0;
	dev_info->queue_priority[RTE_BBDEV_OP_LDPC_DEC] = 0;
	dev_info->queue_priority[RTE_BBDEV_OP_LDPC_ENC] = 0;
	dev_info->queue_priority[RTE_BBDEV_OP_FFT] = 0;
	dev_info->max_num_queues = 0;
	for (i = RTE_BBDEV_OP_NONE; i <= RTE_BBDEV_OP_FFT; i++)
		dev_info->max_num_queues += dev_info->num_queues[i];
	dev_info->queue_size_lim = ACC_MAX_QUEUE_DEPTH;
	dev_info->hardware_accelerated = true;
	dev_info->max_dl_queue_priority =
			d->acc_conf.q_dl_4g.num_qgroups - 1;
	dev_info->max_ul_queue_priority =
			d->acc_conf.q_ul_4g.num_qgroups - 1;
	dev_info->default_queue_conf = default_queue_conf;
	dev_info->cpu_flag_reqs = NULL;
	dev_info->min_alignment = 1;
	dev_info->capabilities = bbdev_capabilities;
	dev_info->harq_buffer_size = 0;
}

static const struct rte_bbdev_ops acc200_bbdev_ops = {
	.close = acc200_dev_close,
	.info_get = acc200_dev_info_get,
};

/* ACC200 PCI PF address map. */
static struct rte_pci_id pci_id_acc200_pf_map[] = {
	{
		RTE_PCI_DEVICE(RTE_ACC200_VENDOR_ID, RTE_ACC200_PF_DEVICE_ID)
	},
	{.device_id = 0},
};

/* ACC200 PCI VF address map. */
static struct rte_pci_id pci_id_acc200_vf_map[] = {
	{
		RTE_PCI_DEVICE(RTE_ACC200_VENDOR_ID, RTE_ACC200_VF_DEVICE_ID)
	},
	{.device_id = 0},
};

/* Initialization Function. */
static void
acc200_bbdev_init(struct rte_bbdev *dev, struct rte_pci_driver *drv)
{
	struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI(dev->device);

	dev->dev_ops = &acc200_bbdev_ops;

	((struct acc_device *) dev->data->dev_private)->pf_device =
			!strcmp(drv->driver.name,
					RTE_STR(ACC200PF_DRIVER_NAME));
	((struct acc_device *) dev->data->dev_private)->mmio_base =
			pci_dev->mem_resource[0].addr;

	rte_bbdev_log_debug("Init device %s [%s] @ vaddr %p paddr %#"PRIx64"",
			drv->driver.name, dev->data->name,
			(void *)pci_dev->mem_resource[0].addr,
			pci_dev->mem_resource[0].phys_addr);
}

static int acc200_pci_probe(struct rte_pci_driver *pci_drv,
	struct rte_pci_device *pci_dev)
{
	struct rte_bbdev *bbdev = NULL;
	char dev_name[RTE_BBDEV_NAME_MAX_LEN];

	if (pci_dev == NULL) {
		rte_bbdev_log(ERR, "NULL PCI device");
		return -EINVAL;
	}

	rte_pci_device_name(&pci_dev->addr, dev_name, sizeof(dev_name));

	/* Allocate memory to be used privately by drivers. */
	bbdev = rte_bbdev_allocate(pci_dev->device.name);
	if (bbdev == NULL)
		return -ENODEV;

	/* allocate device private memory. */
	bbdev->data->dev_private = rte_zmalloc_socket(dev_name,
			sizeof(struct acc_device), RTE_CACHE_LINE_SIZE,
			pci_dev->device.numa_node);

	if (bbdev->data->dev_private == NULL) {
		rte_bbdev_log(CRIT,
				"Allocate of %zu bytes for device \"%s\" failed",
				sizeof(struct acc_device), dev_name);
				rte_bbdev_release(bbdev);
			return -ENOMEM;
	}

	/* Fill HW specific part of device structure. */
	bbdev->device = &pci_dev->device;
	bbdev->intr_handle = pci_dev->intr_handle;
	bbdev->data->socket_id = pci_dev->device.numa_node;

	/* Invoke ACC200 device initialization function. */
	acc200_bbdev_init(bbdev, pci_drv);

	rte_bbdev_log_debug("Initialised bbdev %s (id = %u)",
			dev_name, bbdev->data->dev_id);
	return 0;
}

static struct rte_pci_driver acc200_pci_pf_driver = {
		.probe = acc200_pci_probe,
		.remove = acc_pci_remove,
		.id_table = pci_id_acc200_pf_map,
		.drv_flags = RTE_PCI_DRV_NEED_MAPPING
};

static struct rte_pci_driver acc200_pci_vf_driver = {
		.probe = acc200_pci_probe,
		.remove = acc_pci_remove,
		.id_table = pci_id_acc200_vf_map,
		.drv_flags = RTE_PCI_DRV_NEED_MAPPING
};

RTE_PMD_REGISTER_PCI(ACC200PF_DRIVER_NAME, acc200_pci_pf_driver);
RTE_PMD_REGISTER_PCI_TABLE(ACC200PF_DRIVER_NAME, pci_id_acc200_pf_map);
RTE_PMD_REGISTER_PCI(ACC200VF_DRIVER_NAME, acc200_pci_vf_driver);
RTE_PMD_REGISTER_PCI_TABLE(ACC200VF_DRIVER_NAME, pci_id_acc200_vf_map);
