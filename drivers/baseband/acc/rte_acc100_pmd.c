/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include <unistd.h>

#include <rte_common.h>
#include <rte_log.h>
#include <dev_driver.h>
#include <rte_malloc.h>
#include <rte_byteorder.h>
#include <rte_errno.h>
#include <rte_branch_prediction.h>
#include <rte_hexdump.h>
#include <rte_pci.h>
#include <bus_pci_driver.h>
#include <rte_cycles.h>

#include <rte_bbdev.h>
#include <rte_bbdev_pmd.h>
#include "acc100_pmd.h"
#include "acc101_pmd.h"
#include "vrb_cfg.h"

#ifdef RTE_LIBRTE_BBDEV_DEBUG
RTE_LOG_REGISTER_DEFAULT(acc100_logtype, DEBUG);
#else
RTE_LOG_REGISTER_DEFAULT(acc100_logtype, NOTICE);
#endif

/* Calculate the offset of the enqueue register */
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

enum {UL_4G = 0, UL_5G, DL_4G, DL_5G, NUM_ACC};

/* Return the accelerator enum for a Queue Group Index */
static inline int
accFromQgid(int qg_idx, const struct rte_acc_conf *acc_conf)
{
	int accQg[ACC100_NUM_QGRPS];
	int NumQGroupsPerFn[NUM_ACC];
	int acc, qgIdx, qgIndex = 0;
	for (qgIdx = 0; qgIdx < ACC100_NUM_QGRPS; qgIdx++)
		accQg[qgIdx] = 0;
	NumQGroupsPerFn[UL_4G] = acc_conf->q_ul_4g.num_qgroups;
	NumQGroupsPerFn[UL_5G] = acc_conf->q_ul_5g.num_qgroups;
	NumQGroupsPerFn[DL_4G] = acc_conf->q_dl_4g.num_qgroups;
	NumQGroupsPerFn[DL_5G] = acc_conf->q_dl_5g.num_qgroups;
	for (acc = UL_4G;  acc < NUM_ACC; acc++)
		for (qgIdx = 0; qgIdx < NumQGroupsPerFn[acc]; qgIdx++)
			accQg[qgIndex++] = acc;
	acc = accQg[qg_idx];
	return acc;
}

/* Return the queue topology for a Queue Group Index */
static inline void
qtopFromAcc(struct rte_acc_queue_topology **qtop, int acc_enum,
		struct rte_acc_conf *acc_conf)
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
	default:
		/* NOTREACHED */
		rte_bbdev_log(ERR, "Unexpected error evaluating qtopFromAcc");
		break;
	}
	*qtop = p_qtop;
}

/* Return the AQ depth for a Queue Group Index */
static inline int
aqDepth(int qg_idx, struct rte_acc_conf *acc_conf)
{
	struct rte_acc_queue_topology *q_top = NULL;
	int acc_enum = accFromQgid(qg_idx, acc_conf);
	qtopFromAcc(&q_top, acc_enum, acc_conf);
	if (unlikely(q_top == NULL))
		return 1;
	return RTE_MAX(1, q_top->aq_depth_log2);
}

/* Return the AQ depth for a Queue Group Index */
static inline int
aqNum(int qg_idx, struct rte_acc_conf *acc_conf)
{
	struct rte_acc_queue_topology *q_top = NULL;
	int acc_enum = accFromQgid(qg_idx, acc_conf);
	qtopFromAcc(&q_top, acc_enum, acc_conf);
	if (unlikely(q_top == NULL))
		return 0;
	return q_top->num_aqs_per_groups;
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
}

static inline void
updateQtop(uint8_t acc, uint8_t qg, struct rte_acc_conf *acc_conf,
		struct acc_device *d) {
	uint32_t reg;
	struct rte_acc_queue_topology *q_top = NULL;
	qtopFromAcc(&q_top, acc, acc_conf);
	if (unlikely(q_top == NULL))
		return;
	uint16_t aq;
	q_top->num_qgroups++;
	if (q_top->first_qgroup_index == -1) {
		q_top->first_qgroup_index = qg;
		/* Can be optimized to assume all are enabled by default */
		reg = acc_reg_read(d, queue_offset(d->pf_device,
				0, qg, ACC100_NUM_AQS - 1));
		if (reg & ACC_QUEUE_ENABLE) {
			q_top->num_aqs_per_groups = ACC100_NUM_AQS;
			return;
		}
		q_top->num_aqs_per_groups = 0;
		for (aq = 0; aq < ACC100_NUM_AQS; aq++) {
			reg = acc_reg_read(d, queue_offset(d->pf_device,
					0, qg, aq));
			if (reg & ACC_QUEUE_ENABLE)
				q_top->num_aqs_per_groups++;
		}
	}
}

/* Fetch configuration enabled for the PF/VF using MMIO Read (slow) */
static inline void
fetch_acc100_config(struct rte_bbdev *dev)
{
	struct acc_device *d = dev->data->dev_private;
	struct rte_acc_conf *acc_conf = &d->acc_conf;
	const struct acc100_registry_addr *reg_addr;
	uint8_t acc, qg;
	uint32_t reg, reg_aq, reg_len0, reg_len1;
	uint32_t reg_mode;

	/* No need to retrieve the configuration is already done */
	if (d->configured)
		return;

	/* Choose correct registry addresses for the device type */
	if (d->pf_device)
		reg_addr = &pf_reg_addr;
	else
		reg_addr = &vf_reg_addr;

	d->ddr_size = (1 + acc_reg_read(d, reg_addr->ddr_range)) << 10;

	/* Single VF Bundle by VF */
	acc_conf->num_vf_bundles = 1;
	initQTop(acc_conf);

	struct rte_acc_queue_topology *q_top = NULL;
	int qman_func_id[ACC100_NUM_ACCS] = {ACC_ACCMAP_0, ACC_ACCMAP_1,
			ACC_ACCMAP_2, ACC_ACCMAP_3, ACC_ACCMAP_4};
	reg = acc_reg_read(d, reg_addr->qman_group_func);
	for (qg = 0; qg < ACC_NUM_QGRPS_PER_WORD; qg++) {
		reg_aq = acc_reg_read(d,
				queue_offset(d->pf_device, 0, qg, 0));
		if (reg_aq & ACC_QUEUE_ENABLE) {
			uint32_t idx = (reg >> (qg * 4)) & 0x7;
			if (idx < ACC100_NUM_ACCS) {
				acc = qman_func_id[idx];
				updateQtop(acc, qg, acc_conf, d);
			}
		}
	}

	/* Check the depth of the AQs*/
	reg_len0 = acc_reg_read(d, reg_addr->depth_log0_offset);
	reg_len1 = acc_reg_read(d, reg_addr->depth_log1_offset);
	for (acc = 0; acc < NUM_ACC; acc++) {
		qtopFromAcc(&q_top, acc, acc_conf);
		if (q_top->first_qgroup_index < ACC_NUM_QGRPS_PER_WORD)
			q_top->aq_depth_log2 = (reg_len0 >>
					(q_top->first_qgroup_index * 4))
					& 0xF;
		else
			q_top->aq_depth_log2 = (reg_len1 >>
					((q_top->first_qgroup_index -
					ACC_NUM_QGRPS_PER_WORD) * 4))
					& 0xF;
	}

	/* Read PF mode */
	if (d->pf_device) {
		reg_mode = acc_reg_read(d, HWPfHiPfMode);
		acc_conf->pf_mode_en = (reg_mode == ACC_PF_VAL) ? 1 : 0;
	}

	rte_bbdev_log_debug(
			"%s Config LLR SIGN IN/OUT %s %s QG %u %u %u %u AQ %u %u %u %u Len %u %u %u %u\n",
			(d->pf_device) ? "PF" : "VF",
			(acc_conf->input_pos_llr_1_bit) ? "POS" : "NEG",
			(acc_conf->output_pos_llr_1_bit) ? "POS" : "NEG",
			acc_conf->q_ul_4g.num_qgroups,
			acc_conf->q_dl_4g.num_qgroups,
			acc_conf->q_ul_5g.num_qgroups,
			acc_conf->q_dl_5g.num_qgroups,
			acc_conf->q_ul_4g.num_aqs_per_groups,
			acc_conf->q_dl_4g.num_aqs_per_groups,
			acc_conf->q_ul_5g.num_aqs_per_groups,
			acc_conf->q_dl_5g.num_aqs_per_groups,
			acc_conf->q_ul_4g.aq_depth_log2,
			acc_conf->q_dl_4g.aq_depth_log2,
			acc_conf->q_ul_5g.aq_depth_log2,
			acc_conf->q_dl_5g.aq_depth_log2);
}

/* Checks PF Info Ring to find the interrupt cause and handles it accordingly */
static inline void
acc100_check_ir(struct acc_device *acc100_dev)
{
	volatile union acc_info_ring_data *ring_data;
	uint16_t info_ring_head = acc100_dev->info_ring_head;
	if (acc100_dev->info_ring == NULL)
		return;

	ring_data = acc100_dev->info_ring + (acc100_dev->info_ring_head &
			ACC_INFO_RING_MASK);

	while (ring_data->valid) {
		if ((ring_data->int_nb < ACC100_PF_INT_DMA_DL_DESC_IRQ) || (
				ring_data->int_nb >
				ACC100_PF_INT_DMA_DL5G_DESC_IRQ)) {
			rte_bbdev_log(WARNING, "InfoRing: ITR:%d Info:0x%x",
					ring_data->int_nb, ring_data->detailed_info);
			/* Initialize Info Ring entry and move forward */
			ring_data->val = 0;
		}
		info_ring_head++;
		ring_data = acc100_dev->info_ring +
				(info_ring_head & ACC_INFO_RING_MASK);
	}
}

/* Checks PF Info Ring to find the interrupt cause and handles it accordingly */
static inline void
acc100_pf_interrupt_handler(struct rte_bbdev *dev)
{
	struct acc_device *acc100_dev = dev->data->dev_private;
	volatile union acc_info_ring_data *ring_data;
	struct acc_deq_intr_details deq_intr_det;

	ring_data = acc100_dev->info_ring + (acc100_dev->info_ring_head &
			ACC_INFO_RING_MASK);

	while (ring_data->valid) {

		rte_bbdev_log_debug(
				"ACC100 PF Interrupt received, Info Ring data: 0x%x",
				ring_data->val);

		switch (ring_data->int_nb) {
		case ACC100_PF_INT_DMA_DL_DESC_IRQ:
		case ACC100_PF_INT_DMA_UL_DESC_IRQ:
		case ACC100_PF_INT_DMA_UL5G_DESC_IRQ:
		case ACC100_PF_INT_DMA_DL5G_DESC_IRQ:
			deq_intr_det.queue_id = get_queue_id_from_ring_info(
					dev->data, *ring_data);
			if (deq_intr_det.queue_id == UINT16_MAX) {
				rte_bbdev_log(ERR,
						"Couldn't find queue: aq_id: %u, qg_id: %u, vf_id: %u",
						ring_data->aq_id,
						ring_data->qg_id,
						ring_data->vf_id);
				return;
			}
			rte_bbdev_pmd_callback_process(dev,
					RTE_BBDEV_EVENT_DEQUEUE, &deq_intr_det);
			break;
		default:
			rte_bbdev_pmd_callback_process(dev,
					RTE_BBDEV_EVENT_ERROR, NULL);
			break;
		}

		/* Initialize Info Ring entry and move forward */
		ring_data->val = 0;
		++acc100_dev->info_ring_head;
		ring_data = acc100_dev->info_ring +
				(acc100_dev->info_ring_head &
				ACC_INFO_RING_MASK);
	}
}

/* Checks VF Info Ring to find the interrupt cause and handles it accordingly */
static inline void
acc100_vf_interrupt_handler(struct rte_bbdev *dev)
{
	struct acc_device *acc100_dev = dev->data->dev_private;
	volatile union acc_info_ring_data *ring_data;
	struct acc_deq_intr_details deq_intr_det;

	ring_data = acc100_dev->info_ring + (acc100_dev->info_ring_head &
			ACC_INFO_RING_MASK);

	while (ring_data->valid) {

		rte_bbdev_log_debug(
				"ACC100 VF Interrupt received, Info Ring data: 0x%x",
				ring_data->val);

		switch (ring_data->int_nb) {
		case ACC100_VF_INT_DMA_DL_DESC_IRQ:
		case ACC100_VF_INT_DMA_UL_DESC_IRQ:
		case ACC100_VF_INT_DMA_UL5G_DESC_IRQ:
		case ACC100_VF_INT_DMA_DL5G_DESC_IRQ:
			/* VFs are not aware of their vf_id - it's set to 0 in
			 * queue structures.
			 */
			ring_data->vf_id = 0;
			deq_intr_det.queue_id = get_queue_id_from_ring_info(
					dev->data, *ring_data);
			if (deq_intr_det.queue_id == UINT16_MAX) {
				rte_bbdev_log(ERR,
						"Couldn't find queue: aq_id: %u, qg_id: %u",
						ring_data->aq_id,
						ring_data->qg_id);
				return;
			}
			rte_bbdev_pmd_callback_process(dev,
					RTE_BBDEV_EVENT_DEQUEUE, &deq_intr_det);
			break;
		default:
			rte_bbdev_pmd_callback_process(dev,
					RTE_BBDEV_EVENT_ERROR, NULL);
			break;
		}

		/* Initialize Info Ring entry and move forward */
		ring_data->valid = 0;
		++acc100_dev->info_ring_head;
		ring_data = acc100_dev->info_ring + (acc100_dev->info_ring_head
				& ACC_INFO_RING_MASK);
	}
}

/* Interrupt handler triggered by ACC100 dev for handling specific interrupt */
static void
acc100_dev_interrupt_handler(void *cb_arg)
{
	struct rte_bbdev *dev = cb_arg;
	struct acc_device *acc100_dev = dev->data->dev_private;

	/* Read info ring */
	if (acc100_dev->pf_device)
		acc100_pf_interrupt_handler(dev);
	else
		acc100_vf_interrupt_handler(dev);
}

/* Allocate and setup inforing */
static int
allocate_info_ring(struct rte_bbdev *dev)
{
	struct acc_device *d = dev->data->dev_private;
	const struct acc100_registry_addr *reg_addr;
	rte_iova_t info_ring_iova;
	uint32_t phys_low, phys_high;

	if (d->info_ring != NULL)
		return 0; /* Already configured */

	/* Choose correct registry addresses for the device type */
	if (d->pf_device)
		reg_addr = &pf_reg_addr;
	else
		reg_addr = &vf_reg_addr;
	/* Allocate InfoRing */
	d->info_ring = rte_zmalloc_socket("Info Ring",
		ACC_INFO_RING_NUM_ENTRIES *
		sizeof(*d->info_ring), RTE_CACHE_LINE_SIZE,
		dev->data->socket_id);
	if (d->info_ring == NULL) {
		rte_bbdev_log(ERR,
				"Failed to allocate Info Ring for %s:%u",
				dev->device->driver->name,
				dev->data->dev_id);
		return -ENOMEM;
	}
	info_ring_iova = rte_malloc_virt2iova(d->info_ring);

	/* Setup Info Ring */
	phys_high = (uint32_t)(info_ring_iova >> 32);
	phys_low  = (uint32_t)(info_ring_iova);
	acc_reg_write(d, reg_addr->info_ring_hi, phys_high);
	acc_reg_write(d, reg_addr->info_ring_lo, phys_low);
	acc_reg_write(d, reg_addr->info_ring_en, ACC100_REG_IRQ_EN_ALL);
	d->info_ring_head = (acc_reg_read(d, reg_addr->info_ring_ptr) &
			0xFFF) / sizeof(union acc_info_ring_data);
	return 0;
}


/* Allocate 64MB memory used for all software rings */
static int
acc100_setup_queues(struct rte_bbdev *dev, uint16_t num_queues, int socket_id)
{
	uint32_t phys_low, phys_high, value;
	struct acc_device *d = dev->data->dev_private;
	const struct acc100_registry_addr *reg_addr;
	int ret;

	if (d->pf_device && !d->acc_conf.pf_mode_en) {
		rte_bbdev_log(NOTICE,
				"%s has PF mode disabled. This PF can't be used.",
				dev->data->name);
		return -ENODEV;
	}

	alloc_sw_rings_min_mem(dev, d, num_queues, socket_id);

	/* If minimal memory space approach failed, then allocate
	 * the 2 * 64MB block for the sw rings
	 */
	if (d->sw_rings == NULL)
		alloc_2x64mb_sw_rings_mem(dev, d, socket_id);

	if (d->sw_rings == NULL) {
		rte_bbdev_log(NOTICE,
				"Failure allocating sw_rings memory");
		return -ENODEV;
	}

	/* Configure ACC100 with the base address for DMA descriptor rings
	 * Same descriptor rings used for UL and DL DMA Engines
	 * Note : Assuming only VF0 bundle is used for PF mode
	 */
	phys_high = (uint32_t)(d->sw_rings_iova >> 32);
	phys_low  = (uint32_t)(d->sw_rings_iova & ~(ACC_SIZE_64MBYTE-1));

	/* Choose correct registry addresses for the device type */
	if (d->pf_device)
		reg_addr = &pf_reg_addr;
	else
		reg_addr = &vf_reg_addr;

	/* Read the populated cfg from ACC100 registers */
	fetch_acc100_config(dev);

	for (value = 0; value <= 2; value++) {
		acc_reg_write(d, reg_addr->pmon_ctrl_a, value);
		acc_reg_write(d, reg_addr->pmon_ctrl_b, value);
	}

	/* Release AXI from PF */
	if (d->pf_device)
		acc_reg_write(d, HWPfDmaAxiControl, 1);

	acc_reg_write(d, reg_addr->dma_ring_ul5g_hi, phys_high);
	acc_reg_write(d, reg_addr->dma_ring_ul5g_lo, phys_low);
	acc_reg_write(d, reg_addr->dma_ring_dl5g_hi, phys_high);
	acc_reg_write(d, reg_addr->dma_ring_dl5g_lo, phys_low);
	acc_reg_write(d, reg_addr->dma_ring_ul4g_hi, phys_high);
	acc_reg_write(d, reg_addr->dma_ring_ul4g_lo, phys_low);
	acc_reg_write(d, reg_addr->dma_ring_dl4g_hi, phys_high);
	acc_reg_write(d, reg_addr->dma_ring_dl4g_lo, phys_low);

	/*
	 * Configure Ring Size to the max queue ring size
	 * (used for wrapping purpose)
	 */
	value = log2_basic(d->sw_ring_size / 64);
	acc_reg_write(d, reg_addr->ring_size, value);

	/* Configure tail pointer for use when SDONE enabled */
	if (d->tail_ptrs == NULL)
		d->tail_ptrs = rte_zmalloc_socket(
			dev->device->driver->name,
			ACC100_NUM_QGRPS * ACC100_NUM_AQS * sizeof(uint32_t),
			RTE_CACHE_LINE_SIZE, socket_id);
	if (d->tail_ptrs == NULL) {
		rte_bbdev_log(ERR, "Failed to allocate tail ptr for %s:%u",
				dev->device->driver->name,
				dev->data->dev_id);
		ret = -ENOMEM;
		goto free_sw_rings;
	}
	d->tail_ptr_iova = rte_malloc_virt2iova(d->tail_ptrs);

	phys_high = (uint32_t)(d->tail_ptr_iova >> 32);
	phys_low  = (uint32_t)(d->tail_ptr_iova);
	acc_reg_write(d, reg_addr->tail_ptrs_ul5g_hi, phys_high);
	acc_reg_write(d, reg_addr->tail_ptrs_ul5g_lo, phys_low);
	acc_reg_write(d, reg_addr->tail_ptrs_dl5g_hi, phys_high);
	acc_reg_write(d, reg_addr->tail_ptrs_dl5g_lo, phys_low);
	acc_reg_write(d, reg_addr->tail_ptrs_ul4g_hi, phys_high);
	acc_reg_write(d, reg_addr->tail_ptrs_ul4g_lo, phys_low);
	acc_reg_write(d, reg_addr->tail_ptrs_dl4g_hi, phys_high);
	acc_reg_write(d, reg_addr->tail_ptrs_dl4g_lo, phys_low);

	ret = allocate_info_ring(dev);
	if (ret < 0) {
		rte_bbdev_log(ERR, "Failed to allocate info_ring for %s:%u",
				dev->device->driver->name,
				dev->data->dev_id);
		/* Continue */
	}

	if (d->harq_layout == NULL)
		d->harq_layout = rte_zmalloc_socket("HARQ Layout",
			ACC_HARQ_LAYOUT * sizeof(*d->harq_layout),
			RTE_CACHE_LINE_SIZE, dev->data->socket_id);
	if (d->harq_layout == NULL) {
		rte_bbdev_log(ERR, "Failed to allocate harq_layout for %s:%u",
				dev->device->driver->name,
				dev->data->dev_id);
		ret = -ENOMEM;
		goto free_tail_ptrs;
	}

	/* Mark as configured properly */
	d->configured = true;

	rte_bbdev_log_debug(
			"ACC100 (%s) configured  sw_rings = %p, sw_rings_iova = %#"
			PRIx64, dev->data->name, d->sw_rings, d->sw_rings_iova);
	return 0;

free_tail_ptrs:
	rte_free(d->tail_ptrs);
	d->tail_ptrs = NULL;
free_sw_rings:
	rte_free(d->sw_rings_base);
	d->sw_rings = NULL;

	return ret;
}

static int
acc100_intr_enable(struct rte_bbdev *dev)
{
	int ret;
	struct acc_device *d = dev->data->dev_private;

	/* Only MSI are currently supported */
	if (rte_intr_type_get(dev->intr_handle) == RTE_INTR_HANDLE_VFIO_MSI ||
			rte_intr_type_get(dev->intr_handle) == RTE_INTR_HANDLE_UIO) {

		ret = allocate_info_ring(dev);
		if (ret < 0) {
			rte_bbdev_log(ERR,
					"Couldn't allocate info ring for device: %s",
					dev->data->name);
			return ret;
		}

		ret = rte_intr_enable(dev->intr_handle);
		if (ret < 0) {
			rte_bbdev_log(ERR,
					"Couldn't enable interrupts for device: %s",
					dev->data->name);
			rte_free(d->info_ring);
			return ret;
		}
		ret = rte_intr_callback_register(dev->intr_handle,
				acc100_dev_interrupt_handler, dev);
		if (ret < 0) {
			rte_bbdev_log(ERR,
					"Couldn't register interrupt callback for device: %s",
					dev->data->name);
			rte_free(d->info_ring);
			return ret;
		}

		return 0;
	}

	rte_bbdev_log(ERR, "ACC100 (%s) supports only VFIO MSI interrupts",
			dev->data->name);
	return -ENOTSUP;
}

/* Free memory used for software rings */
static int
acc100_dev_close(struct rte_bbdev *dev)
{
	struct acc_device *d = dev->data->dev_private;
	acc100_check_ir(d);
	if (d->sw_rings_base != NULL) {
		rte_free(d->tail_ptrs);
		rte_free(d->info_ring);
		rte_free(d->sw_rings_base);
		rte_free(d->harq_layout);
		d->sw_rings_base = NULL;
		d->tail_ptrs = NULL;
		d->info_ring = NULL;
		d->harq_layout = NULL;
	}
	/* Ensure all in flight HW transactions are completed */
	usleep(ACC_LONG_WAIT);
	return 0;
}

/**
 * Report a ACC100 queue index which is free
 * Return 0 to 16k for a valid queue_idx or -1 when no queue is available
 * Note : Only supporting VF0 Bundle for PF mode
 */
static int
acc100_find_free_queue_idx(struct rte_bbdev *dev,
		const struct rte_bbdev_queue_conf *conf)
{
	struct acc_device *d = dev->data->dev_private;
	int op_2_acc[5] = {0, UL_4G, DL_4G, UL_5G, DL_5G};
	int acc = op_2_acc[conf->op_type];
	struct rte_acc_queue_topology *qtop = NULL;

	qtopFromAcc(&qtop, acc, &(d->acc_conf));
	if (qtop == NULL)
		return -1;
	/* Identify matching QGroup Index which are sorted in priority order */
	uint16_t group_idx = qtop->first_qgroup_index;
	group_idx += conf->priority;
	if (group_idx >= ACC100_NUM_QGRPS ||
			conf->priority >= qtop->num_qgroups) {
		rte_bbdev_log(INFO, "Invalid Priority on %s, priority %u",
				dev->data->name, conf->priority);
		return -1;
	}
	/* Find a free AQ_idx  */
	uint64_t aq_idx;
	for (aq_idx = 0; aq_idx < qtop->num_aqs_per_groups; aq_idx++) {
		if (((d->q_assigned_bit_map[group_idx] >> aq_idx) & 0x1) == 0) {
			/* Mark the Queue as assigned */
			d->q_assigned_bit_map[group_idx] |= (1ULL << aq_idx);
			/* Report the AQ Index */
			return (group_idx << ACC100_GRP_ID_SHIFT) + aq_idx;
		}
	}
	rte_bbdev_log(INFO, "Failed to find free queue on %s, priority %u",
			dev->data->name, conf->priority);
	return -1;
}

/* Setup ACC100 queue */
static int
acc100_queue_setup(struct rte_bbdev *dev, uint16_t queue_id,
		const struct rte_bbdev_queue_conf *conf)
{
	struct acc_device *d = dev->data->dev_private;
	struct acc_queue *q;
	int16_t q_idx;
	int ret;

	if (d == NULL) {
		rte_bbdev_log(ERR, "Undefined device");
		return -ENODEV;
	}
	/* Allocate the queue data structure. */
	q = rte_zmalloc_socket(dev->device->driver->name, sizeof(*q),
			RTE_CACHE_LINE_SIZE, conf->socket);
	if (q == NULL) {
		rte_bbdev_log(ERR, "Failed to allocate queue memory");
		return -ENOMEM;
	}

	q->d = d;
	q->ring_addr = RTE_PTR_ADD(d->sw_rings, (d->sw_ring_size * queue_id));
	q->ring_addr_iova = d->sw_rings_iova + (d->sw_ring_size * queue_id);

	/* Prepare the Ring with default descriptor format */
	union acc_dma_desc *desc = NULL;
	unsigned int desc_idx, b_idx;
	int fcw_len = (conf->op_type == RTE_BBDEV_OP_LDPC_ENC ?
		ACC_FCW_LE_BLEN : (conf->op_type == RTE_BBDEV_OP_TURBO_DEC ?
		ACC_FCW_TD_BLEN : ACC_FCW_LD_BLEN));

	for (desc_idx = 0; desc_idx < d->sw_ring_max_depth; desc_idx++) {
		desc = q->ring_addr + desc_idx;
		desc->req.word0 = ACC_DMA_DESC_TYPE;
		desc->req.word1 = 0; /**< Timestamp */
		desc->req.word2 = 0;
		desc->req.word3 = 0;
		uint64_t fcw_offset = (desc_idx << 8) + ACC_DESC_FCW_OFFSET;
		desc->req.data_ptrs[0].address = q->ring_addr_iova + fcw_offset;
		desc->req.data_ptrs[0].blen = fcw_len;
		desc->req.data_ptrs[0].blkid = ACC_DMA_BLKID_FCW;
		desc->req.data_ptrs[0].last = 0;
		desc->req.data_ptrs[0].dma_ext = 0;
		for (b_idx = 1; b_idx < ACC_DMA_MAX_NUM_POINTERS - 1;
				b_idx++) {
			desc->req.data_ptrs[b_idx].blkid = ACC_DMA_BLKID_IN;
			desc->req.data_ptrs[b_idx].last = 1;
			desc->req.data_ptrs[b_idx].dma_ext = 0;
			b_idx++;
			desc->req.data_ptrs[b_idx].blkid =
					ACC_DMA_BLKID_OUT_ENC;
			desc->req.data_ptrs[b_idx].last = 1;
			desc->req.data_ptrs[b_idx].dma_ext = 0;
		}
		/* Preset some fields of LDPC FCW */
		desc->req.fcw_ld.FCWversion = ACC_FCW_VER;
		desc->req.fcw_ld.gain_i = 1;
		desc->req.fcw_ld.gain_h = 1;
	}

	q->lb_in = rte_zmalloc_socket(dev->device->driver->name,
			RTE_CACHE_LINE_SIZE,
			RTE_CACHE_LINE_SIZE, conf->socket);
	if (q->lb_in == NULL) {
		rte_bbdev_log(ERR, "Failed to allocate lb_in memory");
		ret = -ENOMEM;
		goto free_q;
	}
	q->lb_in_addr_iova = rte_malloc_virt2iova(q->lb_in);
	q->lb_out = rte_zmalloc_socket(dev->device->driver->name,
			RTE_CACHE_LINE_SIZE,
			RTE_CACHE_LINE_SIZE, conf->socket);
	if (q->lb_out == NULL) {
		rte_bbdev_log(ERR, "Failed to allocate lb_out memory");
		ret = -ENOMEM;
		goto free_lb_in;
	}
	q->lb_out_addr_iova = rte_malloc_virt2iova(q->lb_out);
	q->companion_ring_addr = rte_zmalloc_socket(dev->device->driver->name,
			d->sw_ring_max_depth * sizeof(*q->companion_ring_addr),
			RTE_CACHE_LINE_SIZE, conf->socket);
	if (q->companion_ring_addr == NULL) {
		rte_bbdev_log(ERR, "Failed to allocate companion_ring memory");
		ret = -ENOMEM;
		goto free_lb_out;
	}
	q->derm_buffer = rte_zmalloc_socket(dev->device->driver->name,
			RTE_BBDEV_TURBO_MAX_CB_SIZE * 10,
			RTE_CACHE_LINE_SIZE, conf->socket);
	if (q->derm_buffer == NULL) {
		rte_bbdev_log(ERR, "Failed to allocate derm_buffer memory");
		ret = -ENOMEM;
		goto free_companion_ring_addr;
	}

	/*
	 * Software queue ring wraps synchronously with the HW when it reaches
	 * the boundary of the maximum allocated queue size, no matter what the
	 * sw queue size is. This wrapping is guarded by setting the wrap_mask
	 * to represent the maximum queue size as allocated at the time when
	 * the device has been setup (in configure()).
	 *
	 * The queue depth is set to the queue size value (conf->queue_size).
	 * This limits the occupancy of the queue at any point of time, so that
	 * the queue does not get swamped with enqueue requests.
	 */
	q->sw_ring_depth = conf->queue_size;
	q->sw_ring_wrap_mask = d->sw_ring_max_depth - 1;

	q->op_type = conf->op_type;

	q_idx = acc100_find_free_queue_idx(dev, conf);
	if (q_idx == -1) {
		ret = -EINVAL;
		goto free_derm_buffer;
	}

	q->qgrp_id = (q_idx >> ACC100_GRP_ID_SHIFT) & 0xF;
	q->vf_id = (q_idx >> ACC100_VF_ID_SHIFT)  & 0x3F;
	q->aq_id = q_idx & 0xF;
	q->aq_depth = 0;
	if (conf->op_type ==  RTE_BBDEV_OP_TURBO_DEC)
		q->aq_depth = (1 << d->acc_conf.q_ul_4g.aq_depth_log2);
	else if (conf->op_type ==  RTE_BBDEV_OP_TURBO_ENC)
		q->aq_depth = (1 << d->acc_conf.q_dl_4g.aq_depth_log2);
	else if (conf->op_type ==  RTE_BBDEV_OP_LDPC_DEC)
		q->aq_depth = (1 << d->acc_conf.q_ul_5g.aq_depth_log2);
	else if (conf->op_type ==  RTE_BBDEV_OP_LDPC_ENC)
		q->aq_depth = (1 << d->acc_conf.q_dl_5g.aq_depth_log2);

	q->mmio_reg_enqueue = RTE_PTR_ADD(d->mmio_base,
			queue_offset(d->pf_device,
					q->vf_id, q->qgrp_id, q->aq_id));

	rte_bbdev_log_debug(
			"Setup dev%u q%u: qgrp_id=%u, vf_id=%u, aq_id=%u, aq_depth=%u, mmio_reg_enqueue=%p",
			dev->data->dev_id, queue_id, q->qgrp_id, q->vf_id,
			q->aq_id, q->aq_depth, q->mmio_reg_enqueue);

	dev->data->queues[queue_id].queue_private = q;
	return 0;

free_derm_buffer:
	rte_free(q->derm_buffer);
	q->derm_buffer = NULL;
free_companion_ring_addr:
	rte_free(q->companion_ring_addr);
	q->companion_ring_addr = NULL;
free_lb_out:
	rte_free(q->lb_out);
	q->lb_out = NULL;
free_lb_in:
	rte_free(q->lb_in);
	q->lb_in = NULL;
free_q:
	rte_free(q);
	q = NULL;

	return ret;
}

static inline void
acc100_print_op(struct rte_bbdev_dec_op *op, enum rte_bbdev_op_type op_type,
		uint16_t index)
{
	if (op == NULL)
		return;
	if (op_type == RTE_BBDEV_OP_LDPC_DEC)
		rte_bbdev_log(DEBUG,
			"  Op 5GUL %d %d %d %d %d %d %d %d %d %d %d %d",
			index,
			op->ldpc_dec.basegraph, op->ldpc_dec.z_c,
			op->ldpc_dec.n_cb, op->ldpc_dec.q_m,
			op->ldpc_dec.n_filler, op->ldpc_dec.cb_params.e,
			op->ldpc_dec.op_flags, op->ldpc_dec.rv_index,
			op->ldpc_dec.iter_max, op->ldpc_dec.iter_count,
			op->ldpc_dec.harq_combined_input.length
			);
	else if (op_type == RTE_BBDEV_OP_LDPC_ENC) {
		struct rte_bbdev_enc_op *op_dl = (struct rte_bbdev_enc_op *) op;
		rte_bbdev_log(DEBUG,
			"  Op 5GDL %d %d %d %d %d %d %d %d %d",
			index,
			op_dl->ldpc_enc.basegraph, op_dl->ldpc_enc.z_c,
			op_dl->ldpc_enc.n_cb, op_dl->ldpc_enc.q_m,
			op_dl->ldpc_enc.n_filler, op_dl->ldpc_enc.cb_params.e,
			op_dl->ldpc_enc.op_flags, op_dl->ldpc_enc.rv_index
			);
	}
}

static int
acc100_queue_stop(struct rte_bbdev *dev, uint16_t queue_id)
{
	struct acc_queue *q;
	struct rte_bbdev_dec_op *op;
	uint16_t i;

	q = dev->data->queues[queue_id].queue_private;
	rte_bbdev_log(INFO, "Queue Stop %d H/T/D %d %d %x OpType %d",
			queue_id, q->sw_ring_head, q->sw_ring_tail,
			q->sw_ring_depth, q->op_type);
	for (i = 0; i < q->sw_ring_depth; ++i) {
		op = (q->ring_addr + i)->req.op_addr;
		acc100_print_op(op, q->op_type, i);
	}
	/* ignore all operations in flight and clear counters */
	q->sw_ring_tail = q->sw_ring_head;
	q->aq_enqueued = 0;
	q->aq_dequeued = 0;
	dev->data->queues[queue_id].queue_stats.enqueued_count = 0;
	dev->data->queues[queue_id].queue_stats.dequeued_count = 0;
	dev->data->queues[queue_id].queue_stats.enqueue_err_count = 0;
	dev->data->queues[queue_id].queue_stats.dequeue_err_count = 0;
	dev->data->queues[queue_id].queue_stats.enqueue_warn_count = 0;
	dev->data->queues[queue_id].queue_stats.dequeue_warn_count = 0;

	return 0;
}

/* Release ACC100 queue */
static int
acc100_queue_release(struct rte_bbdev *dev, uint16_t q_id)
{
	struct acc_device *d = dev->data->dev_private;
	struct acc_queue *q = dev->data->queues[q_id].queue_private;

	if (q != NULL) {
		/* Mark the Queue as un-assigned */
		d->q_assigned_bit_map[q->qgrp_id] &= (~0ULL - (1 << (uint64_t) q->aq_id));
		rte_free(q->derm_buffer);
		rte_free(q->companion_ring_addr);
		rte_free(q->lb_in);
		rte_free(q->lb_out);
		rte_free(q);
		dev->data->queues[q_id].queue_private = NULL;
	}

	return 0;
}

/* Get ACC100 device info */
static void
acc100_dev_info_get(struct rte_bbdev *dev,
		struct rte_bbdev_driver_info *dev_info)
{
	struct acc_device *d = dev->data->dev_private;
	int i;
	static const struct rte_bbdev_op_cap bbdev_capabilities[] = {
		{
			.type = RTE_BBDEV_OP_TURBO_DEC,
			.cap.turbo_dec = {
				.capability_flags =
					RTE_BBDEV_TURBO_SUBBLOCK_DEINTERLEAVE |
					RTE_BBDEV_TURBO_CRC_TYPE_24B |
					RTE_BBDEV_TURBO_HALF_ITERATION_EVEN |
					RTE_BBDEV_TURBO_EARLY_TERMINATION |
					RTE_BBDEV_TURBO_DEC_INTERRUPTS |
					RTE_BBDEV_TURBO_NEG_LLR_1_BIT_IN |
					RTE_BBDEV_TURBO_DEC_TB_CRC_24B_KEEP |
					RTE_BBDEV_TURBO_DEC_CRC_24B_DROP |
					RTE_BBDEV_TURBO_DEC_SCATTER_GATHER,
				.max_llr_modulus = INT8_MAX,
				.num_buffers_src =
						RTE_BBDEV_TURBO_MAX_CODE_BLOCKS,
				.num_buffers_hard_out =
						RTE_BBDEV_TURBO_MAX_CODE_BLOCKS,
				.num_buffers_soft_out =
						RTE_BBDEV_TURBO_MAX_CODE_BLOCKS,
			}
		},
		{
			.type = RTE_BBDEV_OP_TURBO_ENC,
			.cap.turbo_enc = {
				.capability_flags =
					RTE_BBDEV_TURBO_CRC_24B_ATTACH |
					RTE_BBDEV_TURBO_RV_INDEX_BYPASS |
					RTE_BBDEV_TURBO_RATE_MATCH |
					RTE_BBDEV_TURBO_ENC_INTERRUPTS |
					RTE_BBDEV_TURBO_ENC_SCATTER_GATHER,
				.num_buffers_src =
						RTE_BBDEV_TURBO_MAX_CODE_BLOCKS,
				.num_buffers_dst =
						RTE_BBDEV_TURBO_MAX_CODE_BLOCKS,
			}
		},
		{
			.type   = RTE_BBDEV_OP_LDPC_ENC,
			.cap.ldpc_enc = {
				.capability_flags =
					RTE_BBDEV_LDPC_RATE_MATCH |
					RTE_BBDEV_LDPC_CRC_24B_ATTACH |
					RTE_BBDEV_LDPC_INTERLEAVER_BYPASS |
					RTE_BBDEV_LDPC_ENC_INTERRUPTS,
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
#ifdef ACC100_EXT_MEM
				RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_LOOPBACK |
				RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_IN_ENABLE |
				RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_OUT_ENABLE |
#endif
				RTE_BBDEV_LDPC_ITERATION_STOP_ENABLE |
				RTE_BBDEV_LDPC_DEINTERLEAVER_BYPASS |
				RTE_BBDEV_LDPC_DECODE_BYPASS |
				RTE_BBDEV_LDPC_DEC_SCATTER_GATHER |
				RTE_BBDEV_LDPC_HARQ_6BIT_COMPRESSION |
				RTE_BBDEV_LDPC_LLR_COMPRESSION |
				RTE_BBDEV_LDPC_DEC_INTERRUPTS,
			.llr_size = 8,
			.llr_decimals = 1,
			.num_buffers_src =
					RTE_BBDEV_LDPC_MAX_CODE_BLOCKS,
			.num_buffers_hard_out =
					RTE_BBDEV_LDPC_MAX_CODE_BLOCKS,
			.num_buffers_soft_out = 0,
			}
		},
		RTE_BBDEV_END_OF_CAPABILITIES_LIST()
	};

	static struct rte_bbdev_queue_conf default_queue_conf;
	default_queue_conf.socket = dev->data->socket_id;
	default_queue_conf.queue_size = ACC_MAX_QUEUE_DEPTH;

	dev_info->driver_name = dev->device->driver->name;

	/* Read and save the populated config from ACC100 registers */
	fetch_acc100_config(dev);
	/* Check the status of device */
	dev_info->device_status = RTE_BBDEV_DEV_NOT_SUPPORTED;

	/* Expose number of queues */
	dev_info->num_queues[RTE_BBDEV_OP_NONE] = 0;
	dev_info->num_queues[RTE_BBDEV_OP_TURBO_DEC] = d->acc_conf.q_ul_4g.num_aqs_per_groups *
			d->acc_conf.q_ul_4g.num_qgroups;
	dev_info->num_queues[RTE_BBDEV_OP_TURBO_ENC] = d->acc_conf.q_dl_4g.num_aqs_per_groups *
			d->acc_conf.q_dl_4g.num_qgroups;
	dev_info->num_queues[RTE_BBDEV_OP_LDPC_DEC] = d->acc_conf.q_ul_5g.num_aqs_per_groups *
			d->acc_conf.q_ul_5g.num_qgroups;
	dev_info->num_queues[RTE_BBDEV_OP_LDPC_ENC] = d->acc_conf.q_dl_5g.num_aqs_per_groups *
			d->acc_conf.q_dl_5g.num_qgroups;
	dev_info->num_queues[RTE_BBDEV_OP_FFT] = 0;
	dev_info->queue_priority[RTE_BBDEV_OP_TURBO_DEC] = d->acc_conf.q_ul_4g.num_qgroups;
	dev_info->queue_priority[RTE_BBDEV_OP_TURBO_ENC] = d->acc_conf.q_dl_4g.num_qgroups;
	dev_info->queue_priority[RTE_BBDEV_OP_LDPC_DEC] = d->acc_conf.q_ul_5g.num_qgroups;
	dev_info->queue_priority[RTE_BBDEV_OP_LDPC_ENC] = d->acc_conf.q_dl_5g.num_qgroups;
	dev_info->max_num_queues = 0;
	for (i = RTE_BBDEV_OP_NONE; i <= RTE_BBDEV_OP_LDPC_ENC; i++)
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
#ifdef ACC100_EXT_MEM
	dev_info->harq_buffer_size = d->ddr_size;
#else
	dev_info->harq_buffer_size = 0;
#endif
	dev_info->data_endianness = RTE_LITTLE_ENDIAN;
	acc100_check_ir(d);
}

static int
acc100_queue_intr_enable(struct rte_bbdev *dev, uint16_t queue_id)
{
	struct acc_queue *q = dev->data->queues[queue_id].queue_private;

	if (rte_intr_type_get(dev->intr_handle) != RTE_INTR_HANDLE_VFIO_MSI &&
			rte_intr_type_get(dev->intr_handle) != RTE_INTR_HANDLE_UIO)
		return -ENOTSUP;

	q->irq_enable = 1;
	return 0;
}

static int
acc100_queue_intr_disable(struct rte_bbdev *dev, uint16_t queue_id)
{
	struct acc_queue *q = dev->data->queues[queue_id].queue_private;

	if (rte_intr_type_get(dev->intr_handle) != RTE_INTR_HANDLE_VFIO_MSI &&
			rte_intr_type_get(dev->intr_handle) != RTE_INTR_HANDLE_UIO)
		return -ENOTSUP;

	q->irq_enable = 0;
	return 0;
}

static const struct rte_bbdev_ops acc100_bbdev_ops = {
	.setup_queues = acc100_setup_queues,
	.intr_enable = acc100_intr_enable,
	.close = acc100_dev_close,
	.info_get = acc100_dev_info_get,
	.queue_setup = acc100_queue_setup,
	.queue_release = acc100_queue_release,
	.queue_stop = acc100_queue_stop,
	.queue_intr_enable = acc100_queue_intr_enable,
	.queue_intr_disable = acc100_queue_intr_disable
};

/* ACC100 PCI PF address map */
static struct rte_pci_id pci_id_acc100_pf_map[] = {
	{
		RTE_PCI_DEVICE(ACC100_VENDOR_ID, ACC100_PF_DEVICE_ID),
	},
	{
		RTE_PCI_DEVICE(ACC101_VENDOR_ID, ACC101_PF_DEVICE_ID),
	},
	{.device_id = 0},
};

/* ACC100 PCI VF address map */
static struct rte_pci_id pci_id_acc100_vf_map[] = {
	{
		RTE_PCI_DEVICE(ACC100_VENDOR_ID, ACC100_VF_DEVICE_ID),
	},
	{
		RTE_PCI_DEVICE(ACC101_VENDOR_ID, ACC101_VF_DEVICE_ID),
	},
	{.device_id = 0},
};


/* Fill in a frame control word for turbo decoding. */
static inline void
acc100_fcw_td_fill(const struct rte_bbdev_dec_op *op, struct acc_fcw_td *fcw)
{
	/* Note : Early termination is always enabled for 4GUL */
	fcw->fcw_ver = 1;
	if (op->turbo_dec.code_block_mode == RTE_BBDEV_TRANSPORT_BLOCK)
		fcw->k_pos = op->turbo_dec.tb_params.k_pos;
	else
		fcw->k_pos = op->turbo_dec.cb_params.k;
	fcw->turbo_crc_type = check_bit(op->turbo_dec.op_flags,
			RTE_BBDEV_TURBO_CRC_TYPE_24B);
	fcw->bypass_sb_deint = 0;
	fcw->raw_decoder_input_on = 0;
	fcw->max_iter = op->turbo_dec.iter_max;
	fcw->half_iter_on = !check_bit(op->turbo_dec.op_flags,
			RTE_BBDEV_TURBO_HALF_ITERATION_EVEN);
}

static inline bool
is_acc100(struct acc_queue *q)
{
	return (q->d->device_variant == ACC100_VARIANT);
}

#ifndef RTE_LIBRTE_BBDEV_SKIP_VALIDATE
static inline bool
validate_op_required(struct acc_queue *q)
{
	return is_acc100(q);
}
#endif

/* Fill in a frame control word for LDPC decoding. */
static inline void
acc100_fcw_ld_fill(struct rte_bbdev_dec_op *op, struct acc_fcw_ld *fcw,
		union acc_harq_layout_data *harq_layout)
{
	uint16_t harq_out_length, harq_in_length, ncb_p, k0_p, parity_offset;
	uint32_t harq_index;
	uint32_t l;
	bool harq_prun = false;
	uint32_t max_hc_in;

	fcw->qm = op->ldpc_dec.q_m;
	fcw->nfiller = op->ldpc_dec.n_filler;
	fcw->BG = (op->ldpc_dec.basegraph - 1);
	fcw->Zc = op->ldpc_dec.z_c;
	fcw->ncb = op->ldpc_dec.n_cb;
	fcw->k0 = get_k0(fcw->ncb, fcw->Zc, op->ldpc_dec.basegraph,
			op->ldpc_dec.rv_index);
	if (op->ldpc_dec.code_block_mode == RTE_BBDEV_CODE_BLOCK)
		fcw->rm_e = op->ldpc_dec.cb_params.e;
	else
		fcw->rm_e = (op->ldpc_dec.tb_params.r <
				op->ldpc_dec.tb_params.cab) ?
						op->ldpc_dec.tb_params.ea :
						op->ldpc_dec.tb_params.eb;

	if (unlikely(check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE) &&
			(op->ldpc_dec.harq_combined_input.length == 0))) {
		rte_bbdev_log(WARNING, "Null HARQ input size provided");
		/* Disable HARQ input in that case to carry forward. */
		op->ldpc_dec.op_flags ^= RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE;
	}

	fcw->hcin_en = check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE);
	fcw->hcout_en = check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_HQ_COMBINE_OUT_ENABLE);
	fcw->crc_select = check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_CRC_TYPE_24B_CHECK);
	fcw->bypass_dec = check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_DECODE_BYPASS);
	fcw->bypass_intlv = check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_DEINTERLEAVER_BYPASS);
	if (op->ldpc_dec.q_m == 1) {
		fcw->bypass_intlv = 1;
		fcw->qm = 2;
	}
	fcw->hcin_decomp_mode = check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_HARQ_6BIT_COMPRESSION);
	fcw->hcout_comp_mode = check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_HARQ_6BIT_COMPRESSION);
	fcw->llr_pack_mode = check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_LLR_COMPRESSION);
	harq_index = hq_index(op->ldpc_dec.harq_combined_output.offset);
#ifdef ACC100_EXT_MEM
	/* Limit cases when HARQ pruning is valid */
	harq_prun = ((op->ldpc_dec.harq_combined_output.offset %
			ACC_HARQ_OFFSET) == 0) &&
			(op->ldpc_dec.harq_combined_output.offset <= UINT16_MAX
			* ACC_HARQ_OFFSET);
#endif
	if (fcw->hcin_en > 0) {
		harq_in_length = op->ldpc_dec.harq_combined_input.length;
		if (fcw->hcin_decomp_mode > 0)
			harq_in_length = harq_in_length * 8 / 6;

		harq_in_length = RTE_MIN(harq_in_length, op->ldpc_dec.n_cb
				- op->ldpc_dec.n_filler);

		/* Alignment on next 64B - Already enforced from HC output */
		harq_in_length = RTE_ALIGN_FLOOR(harq_in_length, ACC_HARQ_ALIGN_64B);

		/* Stronger alignment requirement when in decompression mode */
		if (fcw->hcin_decomp_mode > 0)
			harq_in_length = RTE_ALIGN_FLOOR(harq_in_length, ACC100_HARQ_ALIGN_COMP);

		if ((harq_layout[harq_index].offset > 0) && harq_prun) {
			rte_bbdev_log_debug("HARQ IN offset unexpected for now\n");
			fcw->hcin_size0 = harq_layout[harq_index].size0;
			fcw->hcin_offset = harq_layout[harq_index].offset;
			fcw->hcin_size1 = harq_in_length - harq_layout[harq_index].offset;
		} else {
			fcw->hcin_size0 = harq_in_length;
			fcw->hcin_offset = 0;
			fcw->hcin_size1 = 0;
		}
	} else {
		fcw->hcin_size0 = 0;
		fcw->hcin_offset = 0;
		fcw->hcin_size1 = 0;
	}

	/* Enforce additional check on FCW validity */
	max_hc_in = RTE_ALIGN_CEIL(fcw->ncb - fcw->nfiller, ACC_HARQ_ALIGN_64B);
	if ((fcw->hcin_size0 > max_hc_in) ||
			(fcw->hcin_size1 + fcw->hcin_offset > max_hc_in) ||
			((fcw->hcin_size0 > fcw->hcin_offset) &&
			(fcw->hcin_size1 != 0))) {
		rte_bbdev_log(ERR, " Invalid FCW : HCIn %d %d %d, Ncb %d F %d",
				fcw->hcin_size0, fcw->hcin_size1,
				fcw->hcin_offset,
				fcw->ncb, fcw->nfiller);
		/* Disable HARQ input in that case to carry forward */
		op->ldpc_dec.op_flags ^= RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE;
		fcw->hcin_en = 0;
	}

	fcw->itmax = op->ldpc_dec.iter_max;
	fcw->itstop = check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_ITERATION_STOP_ENABLE);
	fcw->synd_precoder = fcw->itstop;
	/*
	 * These are all implicitly set
	 * fcw->synd_post = 0;
	 * fcw->so_en = 0;
	 * fcw->so_bypass_rm = 0;
	 * fcw->so_bypass_intlv = 0;
	 * fcw->dec_convllr = 0;
	 * fcw->hcout_convllr = 0;
	 * fcw->hcout_size1 = 0;
	 * fcw->so_it = 0;
	 * fcw->hcout_offset = 0;
	 * fcw->negstop_th = 0;
	 * fcw->negstop_it = 0;
	 * fcw->negstop_en = 0;
	 * fcw->gain_i = 1;
	 * fcw->gain_h = 1;
	 */
	if (fcw->hcout_en > 0) {
		parity_offset = (op->ldpc_dec.basegraph == 1 ? 20 : 8)
			* op->ldpc_dec.z_c - op->ldpc_dec.n_filler;
		k0_p = (fcw->k0 > parity_offset) ? fcw->k0 - op->ldpc_dec.n_filler : fcw->k0;
		ncb_p = fcw->ncb - op->ldpc_dec.n_filler;
		l = RTE_MIN(k0_p + fcw->rm_e, INT16_MAX);
		harq_out_length = (uint16_t) fcw->hcin_size0;
		harq_out_length = RTE_MAX(harq_out_length, l);

		/* Stronger alignment when in compression mode */
		if (fcw->hcout_comp_mode > 0)
			harq_out_length = RTE_ALIGN_CEIL(harq_out_length, ACC100_HARQ_ALIGN_COMP);

		/* Cannot exceed the pruned Ncb circular buffer */
		harq_out_length = RTE_MIN(harq_out_length, ncb_p);

		/* Alignment on next 64B */
		harq_out_length = RTE_ALIGN_CEIL(harq_out_length, ACC_HARQ_ALIGN_64B);

		/* Stronger alignment when in compression mode enforced again */
		if (fcw->hcout_comp_mode > 0)
			harq_out_length = RTE_ALIGN_FLOOR(harq_out_length, ACC100_HARQ_ALIGN_COMP);

		if ((k0_p > fcw->hcin_size0 + ACC_HARQ_OFFSET_THRESHOLD) && harq_prun) {
			fcw->hcout_size0 = (uint16_t) fcw->hcin_size0;
			fcw->hcout_offset = k0_p & 0xFFC0;
			fcw->hcout_size1 = harq_out_length - fcw->hcout_offset;
		} else {
			fcw->hcout_size0 = harq_out_length;
			fcw->hcout_size1 = 0;
			fcw->hcout_offset = 0;
		}

		if (fcw->hcout_size0 == 0) {
			rte_bbdev_log(ERR, " Invalid FCW : HCout %d",
				fcw->hcout_size0);
			op->ldpc_dec.op_flags ^= RTE_BBDEV_LDPC_HQ_COMBINE_OUT_ENABLE;
			fcw->hcout_en = 0;
		}

		harq_layout[harq_index].offset = fcw->hcout_offset;
		harq_layout[harq_index].size0 = fcw->hcout_size0;
	} else {
		fcw->hcout_size0 = 0;
		fcw->hcout_size1 = 0;
		fcw->hcout_offset = 0;
	}
}

/* Fill in a frame control word for LDPC decoding for ACC101 */
static inline void
acc101_fcw_ld_fill(struct rte_bbdev_dec_op *op, struct acc_fcw_ld *fcw,
		union acc_harq_layout_data *harq_layout)
{
	uint16_t harq_out_length, harq_in_length, ncb_p, k0_p, parity_offset;
	uint32_t harq_index;
	uint32_t l;

	fcw->qm = op->ldpc_dec.q_m;
	fcw->nfiller = op->ldpc_dec.n_filler;
	fcw->BG = (op->ldpc_dec.basegraph - 1);
	fcw->Zc = op->ldpc_dec.z_c;
	fcw->ncb = op->ldpc_dec.n_cb;
	fcw->k0 = get_k0(fcw->ncb, fcw->Zc, op->ldpc_dec.basegraph,
			op->ldpc_dec.rv_index);
	if (op->ldpc_dec.code_block_mode == RTE_BBDEV_CODE_BLOCK)
		fcw->rm_e = op->ldpc_dec.cb_params.e;
	else
		fcw->rm_e = (op->ldpc_dec.tb_params.r <
				op->ldpc_dec.tb_params.cab) ?
						op->ldpc_dec.tb_params.ea :
						op->ldpc_dec.tb_params.eb;

	if (unlikely(check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE) &&
			(op->ldpc_dec.harq_combined_input.length == 0))) {
		rte_bbdev_log(WARNING, "Null HARQ input size provided");
		/* Disable HARQ input in that case to carry forward */
		op->ldpc_dec.op_flags ^= RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE;
	}
	if (unlikely(fcw->rm_e == 0)) {
		rte_bbdev_log(WARNING, "Null E input provided");
		fcw->rm_e = 2;
	}

	fcw->hcin_en = check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE);
	fcw->hcout_en = check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_HQ_COMBINE_OUT_ENABLE);
	fcw->crc_select = check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_CRC_TYPE_24B_CHECK);
	fcw->bypass_dec = check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_DECODE_BYPASS);
	fcw->bypass_intlv = check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_DEINTERLEAVER_BYPASS);
	if (op->ldpc_dec.q_m == 1) {
		fcw->bypass_intlv = 1;
		fcw->qm = 2;
	}
	fcw->hcin_decomp_mode = check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_HARQ_6BIT_COMPRESSION);
	fcw->hcout_comp_mode = check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_HARQ_6BIT_COMPRESSION);
	fcw->llr_pack_mode = check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_LLR_COMPRESSION);
	harq_index = hq_index(op->ldpc_dec.harq_combined_output.offset);
	if (fcw->hcin_en > 0) {
		harq_in_length = op->ldpc_dec.harq_combined_input.length;
		if (fcw->hcin_decomp_mode > 0)
			harq_in_length = harq_in_length * 8 / 6;
		harq_in_length = RTE_MIN(harq_in_length, op->ldpc_dec.n_cb
				- op->ldpc_dec.n_filler);
		/* Alignment on next 64B - Already enforced from HC output */
		harq_in_length = RTE_ALIGN_FLOOR(harq_in_length, ACC_HARQ_ALIGN_64B);
		fcw->hcin_size0 = harq_in_length;
		fcw->hcin_offset = 0;
		fcw->hcin_size1 = 0;
	} else {
		fcw->hcin_size0 = 0;
		fcw->hcin_offset = 0;
		fcw->hcin_size1 = 0;
	}

	fcw->itmax = op->ldpc_dec.iter_max;
	fcw->itstop = check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_ITERATION_STOP_ENABLE);
	fcw->synd_precoder = fcw->itstop;
	/*
	 * These are all implicitly set
	 * fcw->synd_post = 0;
	 * fcw->so_en = 0;
	 * fcw->so_bypass_rm = 0;
	 * fcw->so_bypass_intlv = 0;
	 * fcw->dec_convllr = 0;
	 * fcw->hcout_convllr = 0;
	 * fcw->hcout_size1 = 0;
	 * fcw->so_it = 0;
	 * fcw->hcout_offset = 0;
	 * fcw->negstop_th = 0;
	 * fcw->negstop_it = 0;
	 * fcw->negstop_en = 0;
	 * fcw->gain_i = 1;
	 * fcw->gain_h = 1;
	 */
	if (fcw->hcout_en > 0) {
		parity_offset = (op->ldpc_dec.basegraph == 1 ? 20 : 8)
			* op->ldpc_dec.z_c - op->ldpc_dec.n_filler;
		k0_p = (fcw->k0 > parity_offset) ?
				fcw->k0 - op->ldpc_dec.n_filler : fcw->k0;
		ncb_p = fcw->ncb - op->ldpc_dec.n_filler;
		l = RTE_MIN(k0_p + fcw->rm_e, INT16_MAX);
		harq_out_length = (uint16_t) fcw->hcin_size0;
		harq_out_length = RTE_MAX(harq_out_length, l);
		/* Cannot exceed the pruned Ncb circular buffer */
		harq_out_length = RTE_MIN(harq_out_length, ncb_p);
		/* Alignment on next 64B */
		harq_out_length = RTE_ALIGN_CEIL(harq_out_length, ACC_HARQ_ALIGN_64B);
		fcw->hcout_size0 = harq_out_length;
		fcw->hcout_size1 = 0;
		fcw->hcout_offset = 0;
		harq_layout[harq_index].offset = fcw->hcout_offset;
		harq_layout[harq_index].size0 = fcw->hcout_size0;
	} else {
		fcw->hcout_size0 = 0;
		fcw->hcout_size1 = 0;
		fcw->hcout_offset = 0;
	}
}

/* May need to pad LDPC Encoder input to avoid small beat for ACC100. */
static inline uint16_t
pad_le_in(uint16_t blen, struct acc_queue *q)
{
	uint16_t last_beat;

	if (!is_acc100(q))
		return blen;

	last_beat = blen % 64;
	if ((last_beat > 0) && (last_beat <= 8))
		blen += 8;

	return blen;
}

static inline int
acc100_dma_desc_le_fill(struct rte_bbdev_enc_op *op,
		struct acc_dma_req_desc *desc, struct rte_mbuf **input,
		struct rte_mbuf *output, uint32_t *in_offset,
		uint32_t *out_offset, uint32_t *out_length,
		uint32_t *mbuf_total_left, uint32_t *seg_total_left, struct acc_queue *q)
{
	int next_triplet = 1; /* FCW already done */
	uint16_t K, in_length_in_bits, in_length_in_bytes;
	struct rte_bbdev_op_ldpc_enc *enc = &op->ldpc_enc;

	acc_header_init(desc);

	K = (enc->basegraph == 1 ? 22 : 10) * enc->z_c;
	in_length_in_bits = K - enc->n_filler;
	if (enc->op_flags & RTE_BBDEV_LDPC_CRC_24B_ATTACH)
		in_length_in_bits -= 24;
	in_length_in_bytes = in_length_in_bits >> 3;

	if (unlikely((*mbuf_total_left == 0) ||
			(*mbuf_total_left < in_length_in_bytes))) {
		rte_bbdev_log(ERR,
				"Mismatch between mbuf length and included CB sizes: mbuf len %u, cb len %u",
				*mbuf_total_left, in_length_in_bytes);
		return -1;
	}

	next_triplet = acc_dma_fill_blk_type_in(desc, input, in_offset,
			pad_le_in(in_length_in_bytes, q), seg_total_left, next_triplet, false);
	if (unlikely(next_triplet < 0)) {
		rte_bbdev_log(ERR,
				"Mismatch between data to process and mbuf data length in bbdev_op: %p",
				op);
		return -1;
	}
	desc->data_ptrs[next_triplet - 1].last = 1;
	desc->m2dlen = next_triplet;
	*mbuf_total_left -= in_length_in_bytes;

	/* Set output length */
	/* Integer round up division by 8 */
	*out_length = (enc->cb_params.e + 7) >> 3;

	next_triplet = acc_dma_fill_blk_type(desc, output, *out_offset,
			*out_length, next_triplet, ACC_DMA_BLKID_OUT_ENC);
	op->ldpc_enc.output.length += *out_length;
	*out_offset += *out_length;
	desc->data_ptrs[next_triplet - 1].last = 1;
	desc->data_ptrs[next_triplet - 1].dma_ext = 0;
	desc->d2mlen = next_triplet - desc->m2dlen;

	desc->op_addr = op;

	return 0;
}

static inline int
acc100_dma_desc_td_fill(struct rte_bbdev_dec_op *op,
		struct acc_dma_req_desc *desc, struct rte_mbuf **input,
		struct rte_mbuf *h_output, struct rte_mbuf *s_output,
		uint32_t *in_offset, uint32_t *h_out_offset,
		uint32_t *s_out_offset, uint32_t *h_out_length,
		uint32_t *s_out_length, uint32_t *mbuf_total_left,
		uint32_t *seg_total_left, uint8_t r)
{
	int next_triplet = 1; /* FCW already done */
	uint16_t k;
	uint16_t crc24_overlap = 0;
	uint32_t e, kw;

	desc->word0 = ACC_DMA_DESC_TYPE;
	desc->word1 = 0; /**< Timestamp could be disabled */
	desc->word2 = 0;
	desc->word3 = 0;
	desc->numCBs = 1;

	if (op->turbo_dec.code_block_mode == RTE_BBDEV_TRANSPORT_BLOCK) {
		k = (r < op->turbo_dec.tb_params.c_neg)
			? op->turbo_dec.tb_params.k_neg
			: op->turbo_dec.tb_params.k_pos;
		e = (r < op->turbo_dec.tb_params.cab)
			? op->turbo_dec.tb_params.ea
			: op->turbo_dec.tb_params.eb;
	} else {
		k = op->turbo_dec.cb_params.k;
		e = op->turbo_dec.cb_params.e;
	}

	if ((op->turbo_dec.code_block_mode == RTE_BBDEV_TRANSPORT_BLOCK)
			&& !check_bit(op->turbo_dec.op_flags,
			RTE_BBDEV_TURBO_DEC_TB_CRC_24B_KEEP))
		crc24_overlap = 24;
	if ((op->turbo_dec.code_block_mode == RTE_BBDEV_CODE_BLOCK)
			&& check_bit(op->turbo_dec.op_flags,
			RTE_BBDEV_TURBO_DEC_CRC_24B_DROP))
		crc24_overlap = 24;

	/* Calculates circular buffer size.
	 * According to 3gpp 36.212 section 5.1.4.2
	 *   Kw = 3 * Kpi,
	 * where:
	 *   Kpi = nCol * nRow
	 * where nCol is 32 and nRow can be calculated from:
	 *   D =< nCol * nRow
	 * where D is the size of each output from turbo encoder block (k + 4).
	 */
	kw = RTE_ALIGN_CEIL(k + 4, 32) * 3;

	if (unlikely((*mbuf_total_left == 0) || (*mbuf_total_left < kw))) {
		rte_bbdev_log(ERR,
				"Mismatch between mbuf length and included CB sizes: mbuf len %u, cb len %u",
				*mbuf_total_left, kw);
		return -1;
	}

	next_triplet = acc_dma_fill_blk_type_in(desc, input, in_offset, kw,
			seg_total_left, next_triplet,
			check_bit(op->turbo_dec.op_flags,
			RTE_BBDEV_TURBO_DEC_SCATTER_GATHER));
	if (unlikely(next_triplet < 0)) {
		rte_bbdev_log(ERR,
				"Mismatch between data to process and mbuf data length in bbdev_op: %p",
				op);
		return -1;
	}
	desc->data_ptrs[next_triplet - 1].last = 1;
	desc->m2dlen = next_triplet;
	*mbuf_total_left -= kw;

	next_triplet = acc_dma_fill_blk_type(
			desc, h_output, *h_out_offset,
			(k - crc24_overlap) >> 3, next_triplet,
			ACC_DMA_BLKID_OUT_HARD);
	if (unlikely(next_triplet < 0)) {
		rte_bbdev_log(ERR,
				"Mismatch between data to process and mbuf data length in bbdev_op: %p",
				op);
		return -1;
	}

	*h_out_length = ((k - crc24_overlap) >> 3);
	op->turbo_dec.hard_output.length += *h_out_length;
	*h_out_offset += *h_out_length;

	/* Soft output */
	if (check_bit(op->turbo_dec.op_flags, RTE_BBDEV_TURBO_SOFT_OUTPUT)) {
		if (op->turbo_dec.soft_output.data == 0) {
			rte_bbdev_log(ERR, "Soft output is not defined");
			return -1;
		}
		if (check_bit(op->turbo_dec.op_flags,
				RTE_BBDEV_TURBO_EQUALIZER))
			*s_out_length = e;
		else
			*s_out_length = (k * 3) + 12;

		next_triplet = acc_dma_fill_blk_type(desc, s_output,
				*s_out_offset, *s_out_length, next_triplet,
				ACC_DMA_BLKID_OUT_SOFT);
		if (unlikely(next_triplet < 0)) {
			rte_bbdev_log(ERR,
					"Mismatch between data to process and mbuf data length in bbdev_op: %p",
					op);
			return -1;
		}

		op->turbo_dec.soft_output.length += *s_out_length;
		*s_out_offset += *s_out_length;
	}

	desc->data_ptrs[next_triplet - 1].last = 1;
	desc->d2mlen = next_triplet - desc->m2dlen;

	desc->op_addr = op;

	return 0;
}

static inline int
acc100_dma_desc_ld_fill(struct rte_bbdev_dec_op *op,
		struct acc_dma_req_desc *desc,
		struct rte_mbuf **input, struct rte_mbuf *h_output,
		uint32_t *in_offset, uint32_t *h_out_offset,
		uint32_t *h_out_length, uint32_t *mbuf_total_left,
		uint32_t *seg_total_left,
		struct acc_fcw_ld *fcw)
{
	struct rte_bbdev_op_ldpc_dec *dec = &op->ldpc_dec;
	int next_triplet = 1; /* FCW already done */
	uint32_t input_length;
	uint16_t output_length, crc24_overlap = 0;
	uint16_t sys_cols, K, h_p_size, h_np_size;
	bool h_comp = check_bit(dec->op_flags,
			RTE_BBDEV_LDPC_HARQ_6BIT_COMPRESSION);

	acc_header_init(desc);

	if (check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_CRC_TYPE_24B_DROP))
		crc24_overlap = 24;

	/* Compute some LDPC BG lengths */
	input_length = fcw->rm_e;
	if (check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_LLR_COMPRESSION))
		input_length = (input_length * 3 + 3) / 4;
	sys_cols = (dec->basegraph == 1) ? 22 : 10;
	K = sys_cols * dec->z_c;
	output_length = K - dec->n_filler - crc24_overlap;

	if (unlikely((*mbuf_total_left == 0) ||
			(*mbuf_total_left < input_length))) {
		rte_bbdev_log(ERR,
				"Mismatch between mbuf length and included CB sizes: mbuf len %u, cb len %u",
				*mbuf_total_left, input_length);
		return -1;
	}

	next_triplet = acc_dma_fill_blk_type_in(desc, input,
			in_offset, input_length,
			seg_total_left, next_triplet,
			check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_DEC_SCATTER_GATHER));

	if (unlikely(next_triplet < 0)) {
		rte_bbdev_log(ERR,
				"Mismatch between data to process and mbuf data length in bbdev_op: %p",
				op);
		return -1;
	}

	if (check_bit(op->ldpc_dec.op_flags,
				RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE)) {
		h_p_size = fcw->hcin_size0 + fcw->hcin_size1;
		if (h_comp)
			h_p_size = (h_p_size * 3 + 3) / 4;
		desc->data_ptrs[next_triplet].address =
				dec->harq_combined_input.offset;
		desc->data_ptrs[next_triplet].blen = h_p_size;
		desc->data_ptrs[next_triplet].blkid = ACC_DMA_BLKID_IN_HARQ;
		desc->data_ptrs[next_triplet].dma_ext = 1;
#ifndef ACC100_EXT_MEM
		acc_dma_fill_blk_type(
				desc,
				op->ldpc_dec.harq_combined_input.data,
				op->ldpc_dec.harq_combined_input.offset,
				h_p_size,
				next_triplet,
				ACC_DMA_BLKID_IN_HARQ);
#endif
		next_triplet++;
	}

	desc->data_ptrs[next_triplet - 1].last = 1;
	desc->m2dlen = next_triplet;
	*mbuf_total_left -= input_length;

	next_triplet = acc_dma_fill_blk_type(desc, h_output,
			*h_out_offset, output_length >> 3, next_triplet,
			ACC_DMA_BLKID_OUT_HARD);

	if (check_bit(op->ldpc_dec.op_flags,
				RTE_BBDEV_LDPC_HQ_COMBINE_OUT_ENABLE)) {
		/* Pruned size of the HARQ */
		h_p_size = fcw->hcout_size0 + fcw->hcout_size1;
		/* Non-Pruned size of the HARQ */
		h_np_size = fcw->hcout_offset > 0 ?
				fcw->hcout_offset + fcw->hcout_size1 :
				h_p_size;
		if (h_comp) {
			h_np_size = (h_np_size * 3 + 3) / 4;
			h_p_size = (h_p_size * 3 + 3) / 4;
		}
		dec->harq_combined_output.length = h_np_size;
		desc->data_ptrs[next_triplet].address =
				dec->harq_combined_output.offset;
		desc->data_ptrs[next_triplet].blen = h_p_size;
		desc->data_ptrs[next_triplet].blkid = ACC_DMA_BLKID_OUT_HARQ;
		desc->data_ptrs[next_triplet].dma_ext = 1;
#ifndef ACC100_EXT_MEM
		acc_dma_fill_blk_type(
				desc,
				dec->harq_combined_output.data,
				dec->harq_combined_output.offset,
				h_p_size,
				next_triplet,
				ACC_DMA_BLKID_OUT_HARQ);
#endif
		next_triplet++;
	}

	*h_out_length = output_length >> 3;
	dec->hard_output.length += *h_out_length;
	*h_out_offset += *h_out_length;
	desc->data_ptrs[next_triplet - 1].last = 1;
	desc->d2mlen = next_triplet - desc->m2dlen;

	desc->op_addr = op;

	return 0;
}

static inline void
acc100_dma_desc_ld_update(struct rte_bbdev_dec_op *op,
		struct acc_dma_req_desc *desc,
		struct rte_mbuf *input, struct rte_mbuf *h_output,
		uint32_t *in_offset, uint32_t *h_out_offset,
		uint32_t *h_out_length,
		union acc_harq_layout_data *harq_layout)
{
	int next_triplet = 1; /* FCW already done */
	desc->data_ptrs[next_triplet].address =
			rte_pktmbuf_iova_offset(input, *in_offset);
	next_triplet++;

	if (check_bit(op->ldpc_dec.op_flags,
				RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE)) {
		struct rte_bbdev_op_data hi = op->ldpc_dec.harq_combined_input;
		desc->data_ptrs[next_triplet].address = hi.offset;
#ifndef ACC100_EXT_MEM
		desc->data_ptrs[next_triplet].address =
				rte_pktmbuf_iova_offset(hi.data, hi.offset);
#endif
		next_triplet++;
	}

	desc->data_ptrs[next_triplet].address =
			rte_pktmbuf_iova_offset(h_output, *h_out_offset);
	*h_out_length = desc->data_ptrs[next_triplet].blen;
	next_triplet++;

	if (check_bit(op->ldpc_dec.op_flags, RTE_BBDEV_LDPC_HQ_COMBINE_OUT_ENABLE)) {
		struct rte_bbdev_dec_op *prev_op;
		uint32_t harq_idx, prev_harq_idx;
		desc->data_ptrs[next_triplet].address = op->ldpc_dec.harq_combined_output.offset;
		/* Adjust based on previous operation */
		prev_op = desc->op_addr;
		op->ldpc_dec.harq_combined_output.length =
				prev_op->ldpc_dec.harq_combined_output.length;
		harq_idx = hq_index(op->ldpc_dec.harq_combined_output.offset);
		prev_harq_idx = hq_index(prev_op->ldpc_dec.harq_combined_output.offset);
		harq_layout[harq_idx].val = harq_layout[prev_harq_idx].val;
#ifndef ACC100_EXT_MEM
		struct rte_bbdev_op_data ho =
				op->ldpc_dec.harq_combined_output;
		desc->data_ptrs[next_triplet].address =
				rte_pktmbuf_iova_offset(ho.data, ho.offset);
#endif
		next_triplet++;
	}

	op->ldpc_dec.hard_output.length += *h_out_length;
	desc->op_addr = op;
}

#ifndef RTE_LIBRTE_BBDEV_SKIP_VALIDATE
/* Validates turbo encoder parameters */
static inline int
validate_enc_op(struct rte_bbdev_enc_op *op, struct acc_queue *q)
{
	struct rte_bbdev_op_turbo_enc *turbo_enc = &op->turbo_enc;
	struct rte_bbdev_op_enc_turbo_cb_params *cb = NULL;
	struct rte_bbdev_op_enc_turbo_tb_params *tb = NULL;
	uint16_t kw, kw_neg, kw_pos;

	if (!validate_op_required(q))
		return 0;

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
	if (turbo_enc->code_block_mode != RTE_BBDEV_TRANSPORT_BLOCK &&
			turbo_enc->code_block_mode != RTE_BBDEV_CODE_BLOCK) {
		rte_bbdev_log(ERR,
				"code_block_mode (%u) is out of range 0 <= value <= 1",
				turbo_enc->code_block_mode);
		return -1;
	}

	if (unlikely(turbo_enc->input.length == 0)) {
		rte_bbdev_log(ERR, "input length null");
		return -1;
	}

	if (turbo_enc->code_block_mode == RTE_BBDEV_TRANSPORT_BLOCK) {
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
		if (unlikely(tb->c_neg > 0)) {
			rte_bbdev_log(ERR,
					"c_neg (%u) expected to be null",
					tb->c_neg);
			return -1;
		}
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
/* Validates LDPC encoder parameters */
static inline int
validate_ldpc_enc_op(struct rte_bbdev_enc_op *op, struct acc_queue *q)
{
	struct rte_bbdev_op_ldpc_enc *ldpc_enc = &op->ldpc_enc;
	int K, N, q_m, crc24;

	if (!validate_op_required(q))
		return 0;

	if (ldpc_enc->input.data == NULL) {
		rte_bbdev_log(ERR, "Invalid input pointer");
		return -1;
	}
	if (ldpc_enc->output.data == NULL) {
		rte_bbdev_log(ERR, "Invalid output pointer");
		return -1;
	}
	if (ldpc_enc->input.length == 0) {
		rte_bbdev_log(ERR, "CB size (%u) is null", ldpc_enc->input.length);
		return -1;
	}
	if ((ldpc_enc->basegraph > 2) || (ldpc_enc->basegraph == 0)) {
		rte_bbdev_log(ERR, "BG (%u) is out of range 1 <= value <= 2", ldpc_enc->basegraph);
		return -1;
	}
	if (ldpc_enc->rv_index > 3) {
		rte_bbdev_log(ERR,
				"rv_index (%u) is out of range 0 <= value <= 3",
				ldpc_enc->rv_index);
		return -1;
	}
	if (ldpc_enc->code_block_mode > RTE_BBDEV_CODE_BLOCK) {
		rte_bbdev_log(ERR,
				"code_block_mode (%u) is out of range 0 <= value <= 1",
				ldpc_enc->code_block_mode);
		return -1;
	}
	if (ldpc_enc->z_c > ACC_MAX_ZC) {
		rte_bbdev_log(ERR, "Zc (%u) is out of range", ldpc_enc->z_c);
		return -1;
	}

	K = (ldpc_enc->basegraph == 1 ? 22 : 10) * ldpc_enc->z_c;
	N = (ldpc_enc->basegraph == 1 ? ACC_N_ZC_1 : ACC_N_ZC_2) * ldpc_enc->z_c;
	q_m = ldpc_enc->q_m;
	crc24 = 0;

	if (check_bit(op->ldpc_enc.op_flags,
			RTE_BBDEV_LDPC_CRC_24A_ATTACH) ||
			check_bit(op->ldpc_enc.op_flags,
			RTE_BBDEV_LDPC_CRC_24B_ATTACH))
		crc24 = 24;
	if ((K - ldpc_enc->n_filler) % 8 > 0) {
		rte_bbdev_log(ERR, "K - F not byte aligned %u", K - ldpc_enc->n_filler);
		return -1;
	}
	if (ldpc_enc->n_filler > (K - 2 * ldpc_enc->z_c)) {
		rte_bbdev_log(ERR, "K - F invalid %u %u", K, ldpc_enc->n_filler);
		return -1;
	}
	if ((ldpc_enc->n_cb > N) || (ldpc_enc->n_cb <= K)) {
		rte_bbdev_log(ERR, "Ncb (%u) is out of range K  %d N %d", ldpc_enc->n_cb, K, N);
		return -1;
	}
	if (!check_bit(op->ldpc_enc.op_flags,
			RTE_BBDEV_LDPC_INTERLEAVER_BYPASS) &&
			((q_m == 0) || ((q_m > 2) && ((q_m % 2) == 1))
			|| (q_m > 8))) {
		rte_bbdev_log(ERR, "Qm (%u) is out of range", ldpc_enc->q_m);
		return -1;
	}
	if (ldpc_enc->code_block_mode == RTE_BBDEV_CODE_BLOCK) {
		if (ldpc_enc->cb_params.e == 0) {
			rte_bbdev_log(ERR, "E is null");
			return -1;
		}
		if (q_m > 0) {
			if (ldpc_enc->cb_params.e % q_m > 0) {
				rte_bbdev_log(ERR, "E not multiple of qm %d", q_m);
				return -1;
			}
		}
		if ((ldpc_enc->z_c <= 11) && (ldpc_enc->cb_params.e > 3456)) {
			rte_bbdev_log(ERR, "E too large for small block");
			return -1;
		}
		if (ldpc_enc->input.length >
			RTE_BBDEV_LDPC_MAX_CB_SIZE >> 3) {
			rte_bbdev_log(ERR, "CB size (%u) is too big, max: %d",
					ldpc_enc->input.length,
					RTE_BBDEV_LDPC_MAX_CB_SIZE);
		return -1;
		}
		if (K < (int) (ldpc_enc->input.length * 8 + ldpc_enc->n_filler) + crc24) {
			rte_bbdev_log(ERR,
					"K and F not matching input size %u %u %u",
					K, ldpc_enc->n_filler,
					ldpc_enc->input.length);
			return -1;
		}
	} else {
		if ((ldpc_enc->tb_params.c == 0) ||
				(ldpc_enc->tb_params.ea == 0) ||
				(ldpc_enc->tb_params.eb == 0)) {
			rte_bbdev_log(ERR, "TB parameter is null");
			return -1;
		}
		if (q_m > 0) {
			if ((ldpc_enc->tb_params.ea % q_m > 0) ||
				(ldpc_enc->tb_params.eb % q_m > 0)) {
				rte_bbdev_log(ERR, "E not multiple of qm %d", q_m);
				return -1;
			}
		}
		if ((ldpc_enc->z_c <= 11) && (RTE_MAX(ldpc_enc->tb_params.ea,
				ldpc_enc->tb_params.eb) > 3456)) {
			rte_bbdev_log(ERR, "E too large for small block");
			return -1;
		}
	}
	return 0;
}

/* Validates LDPC decoder parameters */
static inline int
validate_ldpc_dec_op(struct rte_bbdev_dec_op *op, struct acc_queue *q)
{
	struct rte_bbdev_op_ldpc_dec *ldpc_dec = &op->ldpc_dec;
	int K, N, q_m;
	uint32_t min_harq_input;

	if (!validate_op_required(q))
		return 0;

	if (ldpc_dec->input.data == NULL) {
		rte_bbdev_log(ERR, "Invalid input pointer");
		return -1;
	}
	if (ldpc_dec->hard_output.data == NULL) {
		rte_bbdev_log(ERR, "Invalid output pointer");
		return -1;
	}
	if (ldpc_dec->input.length == 0) {
		rte_bbdev_log(ERR, "input is null");
		return -1;
	}
	if ((ldpc_dec->basegraph > 2) || (ldpc_dec->basegraph == 0)) {
		rte_bbdev_log(ERR, "BG (%u) is out of range 1 <= value <= 2", ldpc_dec->basegraph);
		return -1;
	}
	if (ldpc_dec->iter_max == 0) {
		rte_bbdev_log(ERR, "iter_max (%u) is equal to 0", ldpc_dec->iter_max);
		return -1;
	}
	if (ldpc_dec->rv_index > 3) {
		rte_bbdev_log(ERR,
				"rv_index (%u) is out of range 0 <= value <= 3",
				ldpc_dec->rv_index);
		return -1;
	}
	if (ldpc_dec->code_block_mode > RTE_BBDEV_CODE_BLOCK) {
		rte_bbdev_log(ERR,
				"code_block_mode (%u) is out of range 0 <= value <= 1",
				ldpc_dec->code_block_mode);
		return -1;
	}
	/* Check Zc is valid value. */
	if ((ldpc_dec->z_c > ACC_MAX_ZC) || (ldpc_dec->z_c < 2)) {
		rte_bbdev_log(ERR, "Zc (%u) is out of range", ldpc_dec->z_c);
		return -1;
	}
	if (ldpc_dec->z_c > 256) {
		if ((ldpc_dec->z_c % 32) != 0) {
			rte_bbdev_log(ERR, "Invalid Zc %d", ldpc_dec->z_c);
			return -1;
		}
	} else if (ldpc_dec->z_c > 128) {
		if ((ldpc_dec->z_c % 16) != 0) {
			rte_bbdev_log(ERR, "Invalid Zc %d", ldpc_dec->z_c);
			return -1;
		}
	} else if (ldpc_dec->z_c > 64) {
		if ((ldpc_dec->z_c % 8) != 0) {
			rte_bbdev_log(ERR, "Invalid Zc %d", ldpc_dec->z_c);
			return -1;
		}
	} else if (ldpc_dec->z_c > 32) {
		if ((ldpc_dec->z_c % 4) != 0) {
			rte_bbdev_log(ERR, "Invalid Zc %d", ldpc_dec->z_c);
			return -1;
		}
	} else if (ldpc_dec->z_c > 16) {
		if ((ldpc_dec->z_c % 2) != 0) {
			rte_bbdev_log(ERR, "Invalid Zc %d", ldpc_dec->z_c);
			return -1;
		}
	}

	K = (ldpc_dec->basegraph == 1 ? 22 : 10) * ldpc_dec->z_c;
	N = (ldpc_dec->basegraph == 1 ? ACC_N_ZC_1 : ACC_N_ZC_2) * ldpc_dec->z_c;
	q_m = ldpc_dec->q_m;

	if (ldpc_dec->n_filler >= K - 2 * ldpc_dec->z_c) {
		rte_bbdev_log(ERR, "K and F are not compatible %u %u", K, ldpc_dec->n_filler);
		return -1;
	}
	if ((ldpc_dec->n_cb > N) || (ldpc_dec->n_cb <= K)) {
		rte_bbdev_log(ERR, "Ncb (%u) is out of range K  %d N %d", ldpc_dec->n_cb, K, N);
		return -1;
	}
	if (((q_m == 0) || ((q_m > 2) && ((q_m % 2) == 1))
			|| (q_m > 8))) {
		rte_bbdev_log(ERR, "Qm (%u) is out of range", ldpc_dec->q_m);
		return -1;
	}
	if (ldpc_dec->code_block_mode == RTE_BBDEV_CODE_BLOCK) {
		if (ldpc_dec->cb_params.e == 0) {
			rte_bbdev_log(ERR, "E is null");
			return -1;
		}
		if (ldpc_dec->cb_params.e % q_m > 0) {
			rte_bbdev_log(ERR, "E not multiple of qm %d", q_m);
			return -1;
		}
		if (ldpc_dec->cb_params.e > 512 * ldpc_dec->z_c) {
			rte_bbdev_log(ERR, "E too high");
			return -1;
		}
	} else {
		if ((ldpc_dec->tb_params.c == 0) ||
				(ldpc_dec->tb_params.ea == 0) ||
				(ldpc_dec->tb_params.eb == 0)) {
			rte_bbdev_log(ERR, "TB parameter is null");
			return -1;
		}
		if ((ldpc_dec->tb_params.ea % q_m > 0) ||
				(ldpc_dec->tb_params.eb % q_m > 0)) {
			rte_bbdev_log(ERR, "E not multiple of qm %d", q_m);
			return -1;
		}
		if ((ldpc_dec->tb_params.ea > 512 * ldpc_dec->z_c) ||
				(ldpc_dec->tb_params.eb > 512 * ldpc_dec->z_c)) {
			rte_bbdev_log(ERR, "E too high");
			return -1;
		}
	}
	if (check_bit(op->ldpc_dec.op_flags, RTE_BBDEV_LDPC_DECODE_BYPASS)) {
		rte_bbdev_log(ERR, "Avoid LDPC Decode bypass");
		return -1;
	}

	/* Avoid HARQ compression for small block size */
	if ((check_bit(op->ldpc_dec.op_flags, RTE_BBDEV_LDPC_HARQ_6BIT_COMPRESSION)) && (K < 2048))
		op->ldpc_dec.op_flags ^= RTE_BBDEV_LDPC_HARQ_6BIT_COMPRESSION;

	min_harq_input = check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_HARQ_6BIT_COMPRESSION) ? 256 : 64;
	if (check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE) &&
			ldpc_dec->harq_combined_input.length <
			min_harq_input) {
		rte_bbdev_log(ERR, "HARQ input size is too small %d < %d",
			ldpc_dec->harq_combined_input.length,
			min_harq_input);
		return -1;
	}

	/* Enforce in-range HARQ input size */
	if (check_bit(op->ldpc_dec.op_flags, RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE)) {
		uint32_t max_harq_input = RTE_ALIGN_CEIL(ldpc_dec->n_cb - ldpc_dec->n_filler, 64);

		if (check_bit(op->ldpc_dec.op_flags, RTE_BBDEV_LDPC_HARQ_6BIT_COMPRESSION))
			max_harq_input = max_harq_input * 3 / 4;

		if (ldpc_dec->harq_combined_input.length > max_harq_input) {
			rte_bbdev_log(ERR,
					"HARQ input size out of range %d > %d, Ncb %d F %d K %d N %d",
					ldpc_dec->harq_combined_input.length,
					max_harq_input, ldpc_dec->n_cb,
					ldpc_dec->n_filler, K, N);
			/* Fallback to flush HARQ combine */
			ldpc_dec->harq_combined_input.length = 0;

			if (check_bit(op->ldpc_dec.op_flags, RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE))
				op->ldpc_dec.op_flags ^= RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE;
		}
	}

#ifdef ACC100_EXT_MEM
	/* Enforce in-range HARQ offset */
	if (check_bit(op->ldpc_dec.op_flags, RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE)) {
		if ((op->ldpc_dec.harq_combined_input.offset >> 10) >= q->d->ddr_size) {
			rte_bbdev_log(ERR,
				"HARQin offset out of range %d > %d",
				op->ldpc_dec.harq_combined_input.offset,
				q->d->ddr_size);
			return -1;
		}
		if ((op->ldpc_dec.harq_combined_input.offset & 0x3FF) > 0) {
			rte_bbdev_log(ERR,
				"HARQin offset not aligned on 1kB %d",
				op->ldpc_dec.harq_combined_input.offset);
			return -1;
		}
	}
	if (check_bit(op->ldpc_dec.op_flags, RTE_BBDEV_LDPC_HQ_COMBINE_OUT_ENABLE)) {
		if ((op->ldpc_dec.harq_combined_output.offset >> 10) >= q->d->ddr_size) {
			rte_bbdev_log(ERR,
				"HARQout offset out of range %d > %d",
				op->ldpc_dec.harq_combined_output.offset,
				q->d->ddr_size);
			return -1;
		}
		if ((op->ldpc_dec.harq_combined_output.offset & 0x3FF) > 0) {
			rte_bbdev_log(ERR,
				"HARQout offset not aligned on 1kB %d",
				op->ldpc_dec.harq_combined_output.offset);
			return -1;
		}
	}
#endif

	return 0;
}
#endif

/* Enqueue one encode operations for ACC100 device in CB mode */
static inline int
enqueue_enc_one_op_cb(struct acc_queue *q, struct rte_bbdev_enc_op *op,
		uint16_t total_enqueued_cbs)
{
	union acc_dma_desc *desc = NULL;
	int ret;
	uint32_t in_offset, out_offset, out_length, mbuf_total_left,
		seg_total_left;
	struct rte_mbuf *input, *output_head, *output;

#ifndef RTE_LIBRTE_BBDEV_SKIP_VALIDATE
	/* Validate op structure */
	if (validate_enc_op(op, q) == -1) {
		rte_bbdev_log(ERR, "Turbo encoder validation rejected");
		return -EINVAL;
	}
#endif

	desc = acc_desc(q, total_enqueued_cbs);
	acc_fcw_te_fill(op, &desc->req.fcw_te);

	input = op->turbo_enc.input.data;
	output_head = output = op->turbo_enc.output.data;
	in_offset = op->turbo_enc.input.offset;
	out_offset = op->turbo_enc.output.offset;
	out_length = 0;
	mbuf_total_left = op->turbo_enc.input.length;
	seg_total_left = rte_pktmbuf_data_len(op->turbo_enc.input.data)
			- in_offset;

	ret = acc_dma_desc_te_fill(op, &desc->req, &input, output,
			&in_offset, &out_offset, &out_length, &mbuf_total_left,
			&seg_total_left, 0);

	if (unlikely(ret < 0))
		return ret;

	mbuf_append(output_head, output, out_length);

#ifdef RTE_LIBRTE_BBDEV_DEBUG
	rte_memdump(stderr, "FCW", &desc->req.fcw_te,
			sizeof(desc->req.fcw_te) - 8);
	rte_memdump(stderr, "Req Desc.", desc, sizeof(*desc));
	if (check_mbuf_total_left(mbuf_total_left) != 0)
		return -EINVAL;
#endif
	/* One CB (one op) was successfully prepared to enqueue */
	return 1;
}

/* Enqueue one encode operations for ACC100 device in CB mode */
static inline int
enqueue_ldpc_enc_n_op_cb(struct acc_queue *q, struct rte_bbdev_enc_op **ops,
		uint16_t total_enqueued_descs, int16_t num)
{
	union acc_dma_desc *desc = NULL;
	uint32_t out_length;
	struct rte_mbuf *output_head, *output;
	int i, next_triplet;
	uint16_t  in_length_in_bytes;
	struct rte_bbdev_op_ldpc_enc *enc = &ops[0]->ldpc_enc;

#ifndef RTE_LIBRTE_BBDEV_SKIP_VALIDATE
	/* Validate op structure */
	if (validate_ldpc_enc_op(ops[0], q) == -1) {
		rte_bbdev_log(ERR, "LDPC encoder validation rejected");
		return -EINVAL;
	}
#endif

	desc = acc_desc(q, total_enqueued_descs);
	acc_fcw_le_fill(ops[0], &desc->req.fcw_le, num, 0);

	/** This could be done at polling */
	acc_header_init(&desc->req);
	desc->req.numCBs = num;

	in_length_in_bytes = pad_le_in(ops[0]->ldpc_enc.input.data->data_len, q);
	out_length = (enc->cb_params.e + 7) >> 3;
	desc->req.m2dlen = 1 + num;
	desc->req.d2mlen = num;
	next_triplet = 1;

	for (i = 0; i < num; i++) {
		desc->req.data_ptrs[next_triplet].address =
			rte_pktmbuf_iova_offset(ops[i]->ldpc_enc.input.data, 0);
		desc->req.data_ptrs[next_triplet].blen = in_length_in_bytes;
		next_triplet++;
		desc->req.data_ptrs[next_triplet].address =
				rte_pktmbuf_iova_offset(
				ops[i]->ldpc_enc.output.data, 0);
		desc->req.data_ptrs[next_triplet].blen = out_length;
		next_triplet++;
		ops[i]->ldpc_enc.output.length = out_length;
		output_head = output = ops[i]->ldpc_enc.output.data;
		mbuf_append(output_head, output, out_length);
		output->data_len = out_length;
	}

	desc->req.op_addr = ops[0];
	/* Keep track of pointers even when multiplexed in single descriptor */
	struct acc_ptrs *context_ptrs = q->companion_ring_addr
			+ acc_desc_idx(q, total_enqueued_descs);
	for (i = 0; i < num; i++)
		context_ptrs->ptr[i].op_addr = ops[i];

#ifdef RTE_LIBRTE_BBDEV_DEBUG
	rte_memdump(stderr, "FCW", &desc->req.fcw_le,
			sizeof(desc->req.fcw_le) - 8);
	rte_memdump(stderr, "Req Desc.", desc, sizeof(*desc));
#endif

	/* One CB (one op) was successfully prepared to enqueue */
	return num;
}

/* Enqueue one encode operations for ACC100 device for a partial TB
 * all codes blocks have same configuration multiplexed on the same descriptor.
 */
static inline void
enqueue_ldpc_enc_part_tb(struct acc_queue *q, struct rte_bbdev_enc_op *op,
		uint16_t total_enqueued_descs, int16_t num_cbs, uint32_t e,
		uint16_t in_len_bytes, uint32_t out_len_bytes, uint32_t *in_offset,
		uint32_t *out_offset)
{
	union acc_dma_desc *desc = NULL;
	struct rte_mbuf *output_head, *output;
	int i, next_triplet;
	struct rte_bbdev_op_ldpc_enc *enc = &op->ldpc_enc;

	desc = acc_desc(q, total_enqueued_descs);
	acc_fcw_le_fill(op, &desc->req.fcw_le, num_cbs, e);

	/* This could be done at polling. */
	acc_header_init(&desc->req);
	desc->req.numCBs = num_cbs;

	desc->req.m2dlen = 1 + num_cbs;
	desc->req.d2mlen = num_cbs;
	next_triplet = 1;

	for (i = 0; i < num_cbs; i++) {
		desc->req.data_ptrs[next_triplet].address =
			rte_pktmbuf_iova_offset(enc->input.data, *in_offset);
		*in_offset += in_len_bytes;
		desc->req.data_ptrs[next_triplet].blen = in_len_bytes;
		next_triplet++;
		desc->req.data_ptrs[next_triplet].address =
			rte_pktmbuf_iova_offset(enc->output.data, *out_offset);
		*out_offset += out_len_bytes;
		desc->req.data_ptrs[next_triplet].blen = out_len_bytes;
		next_triplet++;
		enc->output.length += out_len_bytes;
		output_head = output = enc->output.data;
		mbuf_append(output_head, output, out_len_bytes);
	}

#ifdef RTE_LIBRTE_BBDEV_DEBUG
	rte_memdump(stderr, "FCW", &desc->req.fcw_le,
			sizeof(desc->req.fcw_le) - 8);
	rte_memdump(stderr, "Req Desc.", desc, sizeof(*desc));
#endif

}

/* Enqueue one encode operations for ACC100 device in CB mode */
static inline int
enqueue_ldpc_enc_one_op_cb(struct acc_queue *q, struct rte_bbdev_enc_op *op,
		uint16_t total_enqueued_cbs)
{
	union acc_dma_desc *desc = NULL;
	int ret;
	uint32_t in_offset, out_offset, out_length, mbuf_total_left,
		seg_total_left;
	struct rte_mbuf *input, *output_head, *output;

#ifndef RTE_LIBRTE_BBDEV_SKIP_VALIDATE
	/* Validate op structure */
	if (validate_ldpc_enc_op(op, q) == -1) {
		rte_bbdev_log(ERR, "LDPC encoder validation rejected");
		return -EINVAL;
	}
#endif

	desc = acc_desc(q, total_enqueued_cbs);
	acc_fcw_le_fill(op, &desc->req.fcw_le, 1, 0);

	input = op->ldpc_enc.input.data;
	output_head = output = op->ldpc_enc.output.data;
	in_offset = op->ldpc_enc.input.offset;
	out_offset = op->ldpc_enc.output.offset;
	out_length = 0;
	mbuf_total_left = op->ldpc_enc.input.length;
	seg_total_left = rte_pktmbuf_data_len(op->ldpc_enc.input.data)
			- in_offset;

	ret = acc100_dma_desc_le_fill(op, &desc->req, &input, output,
			&in_offset, &out_offset, &out_length, &mbuf_total_left,
			&seg_total_left, q);

	if (unlikely(ret < 0))
		return ret;

	mbuf_append(output_head, output, out_length);

#ifdef RTE_LIBRTE_BBDEV_DEBUG
	rte_memdump(stderr, "FCW", &desc->req.fcw_le,
			sizeof(desc->req.fcw_le) - 8);
	rte_memdump(stderr, "Req Desc.", desc, sizeof(*desc));

	if (check_mbuf_total_left(mbuf_total_left) != 0)
		return -EINVAL;
#endif
	/* One CB (one op) was successfully prepared to enqueue */
	return 1;
}


/* Enqueue one encode operations for ACC100 device in TB mode. */
static inline int
enqueue_enc_one_op_tb(struct acc_queue *q, struct rte_bbdev_enc_op *op,
		uint16_t total_enqueued_cbs, uint8_t cbs_in_tb)
{
	union acc_dma_desc *desc = NULL;
	int ret;
	uint8_t r, c;
	uint32_t in_offset, out_offset, out_length, mbuf_total_left,
		seg_total_left;
	struct rte_mbuf *input, *output_head, *output;
	uint16_t desc_idx, current_enqueued_cbs = 0;
	uint64_t fcw_offset;

#ifndef RTE_LIBRTE_BBDEV_SKIP_VALIDATE
	/* Validate op structure */
	if (validate_enc_op(op, q) == -1) {
		rte_bbdev_log(ERR, "Turbo encoder validation rejected");
		return -EINVAL;
	}
#endif

	desc_idx = acc_desc_idx(q, total_enqueued_cbs);
	desc = q->ring_addr + desc_idx;
	fcw_offset = (desc_idx << 8) + ACC_DESC_FCW_OFFSET;
	acc_fcw_te_fill(op, &desc->req.fcw_te);

	input = op->turbo_enc.input.data;
	output_head = output = op->turbo_enc.output.data;
	in_offset = op->turbo_enc.input.offset;
	out_offset = op->turbo_enc.output.offset;
	out_length = 0;
	mbuf_total_left = op->turbo_enc.input.length;

	c = op->turbo_enc.tb_params.c;
	r = op->turbo_enc.tb_params.r;

	while (mbuf_total_left > 0 && r < c) {
		if (unlikely(input == NULL)) {
			rte_bbdev_log(ERR, "Not enough input segment");
			return -EINVAL;
		}
		seg_total_left = rte_pktmbuf_data_len(input) - in_offset;
		/* Set up DMA descriptor */
		desc = acc_desc(q, total_enqueued_cbs);
		desc->req.data_ptrs[0].address = q->ring_addr_iova + fcw_offset;
		desc->req.data_ptrs[0].blen = ACC_FCW_TE_BLEN;

		ret = acc_dma_desc_te_fill(op, &desc->req, &input, output,
				&in_offset, &out_offset, &out_length,
				&mbuf_total_left, &seg_total_left, r);
		if (unlikely(ret < 0))
			return ret;
		mbuf_append(output_head, output, out_length);

		/* Set total number of CBs in TB */
		desc->req.cbs_in_tb = cbs_in_tb;
#ifdef RTE_LIBRTE_BBDEV_DEBUG
		rte_memdump(stderr, "FCW", &desc->req.fcw_te,
				sizeof(desc->req.fcw_te) - 8);
		rte_memdump(stderr, "Req Desc.", desc, sizeof(*desc));
#endif

		if (seg_total_left == 0) {
			/* Go to the next mbuf */
			input = input->next;
			in_offset = 0;
			output = output->next;
			out_offset = 0;
		}

		total_enqueued_cbs++;
		current_enqueued_cbs++;
		r++;
	}

#ifdef RTE_LIBRTE_BBDEV_DEBUG
	if (check_mbuf_total_left(mbuf_total_left) != 0)
		return -EINVAL;
#endif

	/* Set SDone on last CB descriptor for TB mode. */
	desc->req.sdone_enable = 1;

	return current_enqueued_cbs;
}

/* Enqueue one encode operations for ACC100 device in TB mode.
 * returns the number of descs used.
 */
static inline int
enqueue_ldpc_enc_one_op_tb(struct acc_queue *q, struct rte_bbdev_enc_op *op,
		uint16_t enq_descs, uint8_t cbs_in_tb)
{
#ifndef RTE_LIBRTE_BBDEV_SKIP_VALIDATE
	if (validate_ldpc_enc_op(op, q) == -1) {
		rte_bbdev_log(ERR, "LDPC encoder validation rejected");
		return -EINVAL;
	}
#endif
	uint8_t num_a, num_b;
	uint8_t r = op->ldpc_enc.tb_params.r;
	uint8_t cab =  op->ldpc_enc.tb_params.cab;
	union acc_dma_desc *desc;
	uint16_t init_enq_descs = enq_descs;
	uint16_t input_len_B = ((op->ldpc_enc.basegraph == 1 ? 22 : 10) *
			op->ldpc_enc.z_c - op->ldpc_enc.n_filler) >> 3;
	uint32_t in_offset = 0, out_offset = 0;
	uint16_t return_descs;

	if (check_bit(op->ldpc_enc.op_flags, RTE_BBDEV_LDPC_CRC_24B_ATTACH))
		input_len_B -= 3;

	if (r < cab) {
		num_a = cab - r;
		num_b = cbs_in_tb - cab;
	} else {
		num_a = 0;
		num_b = cbs_in_tb - r;
	}

	while (num_a > 0) {
		uint32_t e = op->ldpc_enc.tb_params.ea;
		uint32_t out_len_bytes = (e + 7) >> 3;
		uint8_t enq = RTE_MIN(num_a, ACC_MUX_5GDL_DESC);
		num_a -= enq;
		enqueue_ldpc_enc_part_tb(q, op, enq_descs, enq, e, input_len_B,
				out_len_bytes, &in_offset, &out_offset);
		enq_descs++;
	}
	while (num_b > 0) {
		uint32_t e = op->ldpc_enc.tb_params.eb;
		uint32_t out_len_bytes = (e + 7) >> 3;
		uint8_t enq = RTE_MIN(num_b, ACC_MUX_5GDL_DESC);
		num_b -= enq;
		enqueue_ldpc_enc_part_tb(q, op, enq_descs, enq, e, input_len_B,
				out_len_bytes, &in_offset, &out_offset);
		enq_descs++;
	}

	return_descs = enq_descs - init_enq_descs;
	/* Keep total number of CBs in first TB. */
	desc = acc_desc(q, init_enq_descs);
	desc->req.cbs_in_tb = return_descs; /** Actual number of descriptors. */
	desc->req.op_addr = op;

	/* Set SDone on last CB descriptor for TB mode. */
	desc = acc_desc(q, enq_descs - 1);
	desc->req.sdone_enable = 1;
	desc->req.op_addr = op;

	return return_descs;
}

#ifndef RTE_LIBRTE_BBDEV_SKIP_VALIDATE
/* Validates turbo decoder parameters */
static inline int
validate_dec_op(struct rte_bbdev_dec_op *op, struct acc_queue *q)
{
	struct rte_bbdev_op_turbo_dec *turbo_dec = &op->turbo_dec;
	struct rte_bbdev_op_dec_turbo_cb_params *cb = NULL;
	struct rte_bbdev_op_dec_turbo_tb_params *tb = NULL;

	if (!validate_op_required(q))
		return 0;

	if (turbo_dec->input.data == NULL) {
		rte_bbdev_log(ERR, "Invalid input pointer");
		return -1;
	}
	if (turbo_dec->hard_output.data == NULL) {
		rte_bbdev_log(ERR, "Invalid hard_output pointer");
		return -1;
	}
	if (check_bit(turbo_dec->op_flags, RTE_BBDEV_TURBO_SOFT_OUTPUT) &&
			turbo_dec->soft_output.data == NULL) {
		rte_bbdev_log(ERR, "Invalid soft_output pointer");
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
	if (turbo_dec->code_block_mode != RTE_BBDEV_TRANSPORT_BLOCK &&
			turbo_dec->code_block_mode != RTE_BBDEV_CODE_BLOCK) {
		rte_bbdev_log(ERR,
				"code_block_mode (%u) is out of range 0 <= value <= 1",
				turbo_dec->code_block_mode);
		return -1;
	}

	if (unlikely(turbo_dec->input.length == 0)) {
		rte_bbdev_log(ERR, "input length null");
		return -1;
	}

	if (turbo_dec->code_block_mode == RTE_BBDEV_TRANSPORT_BLOCK) {
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
		if (unlikely(tb->c_neg > (RTE_BBDEV_TURBO_MAX_CODE_BLOCKS - 1))) {
			rte_bbdev_log(ERR,
					"c_neg (%u) is out of range 0 <= value <= %u",
					tb->c_neg,
					RTE_BBDEV_TURBO_MAX_CODE_BLOCKS - 1);
					return -1;
		}
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
		if (check_bit(turbo_dec->op_flags, RTE_BBDEV_TURBO_EQUALIZER) &&
				(tb->ea < RTE_BBDEV_TURBO_MIN_CB_SIZE
						|| (tb->ea % 2))
				&& tb->cab > 0) {
			rte_bbdev_log(ERR,
					"ea (%u) is less than %u or it is not even",
					tb->ea, RTE_BBDEV_TURBO_MIN_CB_SIZE);
			return -1;
		}
		if (check_bit(turbo_dec->op_flags, RTE_BBDEV_TURBO_EQUALIZER) &&
				(tb->eb < RTE_BBDEV_TURBO_MIN_CB_SIZE
						|| (tb->eb % 2))
				&& tb->c > tb->cab) {
			rte_bbdev_log(ERR,
					"eb (%u) is less than %u or it is not even",
					tb->eb, RTE_BBDEV_TURBO_MIN_CB_SIZE);
		}
	} else {
		cb = &turbo_dec->cb_params;
		if (cb->k < RTE_BBDEV_TURBO_MIN_CB_SIZE
				|| cb->k > RTE_BBDEV_TURBO_MAX_CB_SIZE) {
			rte_bbdev_log(ERR,
					"k (%u) is out of range %u <= value <= %u",
					cb->k, RTE_BBDEV_TURBO_MIN_CB_SIZE,
					RTE_BBDEV_TURBO_MAX_CB_SIZE);
			return -1;
		}
		if (check_bit(turbo_dec->op_flags, RTE_BBDEV_TURBO_EQUALIZER) &&
				(cb->e < RTE_BBDEV_TURBO_MIN_CB_SIZE ||
				(cb->e % 2))) {
			rte_bbdev_log(ERR,
					"e (%u) is less than %u or it is not even",
					cb->e, RTE_BBDEV_TURBO_MIN_CB_SIZE);
			return -1;
		}
	}

	return 0;
}
#endif

/** Enqueue one decode operations for ACC100 device in CB mode */
static inline int
enqueue_dec_one_op_cb(struct acc_queue *q, struct rte_bbdev_dec_op *op,
		uint16_t total_enqueued_cbs)
{
	union acc_dma_desc *desc = NULL;
	int ret;
	uint32_t in_offset, h_out_offset, s_out_offset, s_out_length,
		h_out_length, mbuf_total_left, seg_total_left;
	struct rte_mbuf *input, *h_output_head, *h_output,
		*s_output_head, *s_output;

#ifndef RTE_LIBRTE_BBDEV_SKIP_VALIDATE
	/* Validate op structure */
	if (validate_dec_op(op, q) == -1) {
		rte_bbdev_log(ERR, "Turbo decoder validation rejected");
		return -EINVAL;
	}
#endif

	desc = acc_desc(q, total_enqueued_cbs);
	acc100_fcw_td_fill(op, &desc->req.fcw_td);

	input = op->turbo_dec.input.data;
	h_output_head = h_output = op->turbo_dec.hard_output.data;
	s_output_head = s_output = op->turbo_dec.soft_output.data;
	in_offset = op->turbo_dec.input.offset;
	h_out_offset = op->turbo_dec.hard_output.offset;
	s_out_offset = op->turbo_dec.soft_output.offset;
	h_out_length = s_out_length = 0;
	mbuf_total_left = op->turbo_dec.input.length;
	seg_total_left = rte_pktmbuf_data_len(input) - in_offset;

#ifdef RTE_LIBRTE_BBDEV_DEBUG
	if (unlikely(input == NULL)) {
		rte_bbdev_log(ERR, "Invalid mbuf pointer");
		return -EFAULT;
	}
#endif

	/* Set up DMA descriptor */
	desc = acc_desc(q, total_enqueued_cbs);

	ret = acc100_dma_desc_td_fill(op, &desc->req, &input, h_output,
			s_output, &in_offset, &h_out_offset, &s_out_offset,
			&h_out_length, &s_out_length, &mbuf_total_left,
			&seg_total_left, 0);

	if (unlikely(ret < 0))
		return ret;

	/* Hard output */
	mbuf_append(h_output_head, h_output, h_out_length);

	/* Soft output */
	if (check_bit(op->turbo_dec.op_flags, RTE_BBDEV_TURBO_SOFT_OUTPUT))
		mbuf_append(s_output_head, s_output, s_out_length);

#ifdef RTE_LIBRTE_BBDEV_DEBUG
	rte_memdump(stderr, "FCW", &desc->req.fcw_td,
			sizeof(desc->req.fcw_td) - 8);
	rte_memdump(stderr, "Req Desc.", desc, sizeof(*desc));
	if (check_mbuf_total_left(mbuf_total_left) != 0)
		return -EINVAL;
#endif

	/* One CB (one op) was successfully prepared to enqueue */
	return 1;
}

static inline int
harq_loopback(struct acc_queue *q, struct rte_bbdev_dec_op *op,
		uint16_t total_enqueued_cbs) {
	struct acc_fcw_ld *fcw;
	union acc_dma_desc *desc;
	int next_triplet = 1;
	struct rte_mbuf *hq_output_head, *hq_output;
	uint16_t harq_dma_length_in, harq_dma_length_out;
	uint16_t harq_in_length = op->ldpc_dec.harq_combined_input.length;
	bool ddr_mem_in;
	union acc_harq_layout_data *harq_layout;
	uint32_t harq_index;

	if (harq_in_length == 0) {
		rte_bbdev_log(ERR, "Loopback of invalid null size\n");
		return -EINVAL;
	}

	int h_comp = check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_HARQ_6BIT_COMPRESSION
			) ? 1 : 0;
	if (h_comp == 1) {
		harq_in_length = harq_in_length * 8 / 6;
		harq_in_length = RTE_ALIGN(harq_in_length, 64);
		harq_dma_length_in = harq_in_length * 6 / 8;
	} else {
		harq_in_length = RTE_ALIGN(harq_in_length, 64);
		harq_dma_length_in = harq_in_length;
	}
	harq_dma_length_out = harq_dma_length_in;

	ddr_mem_in = check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_IN_ENABLE);
	harq_layout = q->d->harq_layout;
	harq_index = hq_index(ddr_mem_in ?
			op->ldpc_dec.harq_combined_input.offset :
			op->ldpc_dec.harq_combined_output.offset);

	desc = acc_desc(q, total_enqueued_cbs);
	fcw = &desc->req.fcw_ld;
	/* Set the FCW from loopback into DDR */
	memset(fcw, 0, sizeof(struct acc_fcw_ld));
	fcw->FCWversion = ACC_FCW_VER;
	fcw->qm = 2;
	fcw->Zc = ACC_MAX_ZC;
	if (harq_in_length < 16 * ACC_N_ZC_1)
		fcw->Zc = 16;
	fcw->ncb = fcw->Zc * ACC_N_ZC_1;
	fcw->rm_e = 2;
	fcw->hcin_en = 1;
	fcw->hcout_en = 1;

	rte_bbdev_log(DEBUG, "Loopback IN %d Index %d offset %d length %d %d\n",
			ddr_mem_in, harq_index,
			harq_layout[harq_index].offset, harq_in_length,
			harq_dma_length_in);

	if (ddr_mem_in && (harq_layout[harq_index].offset > 0)) {
		fcw->hcin_size0 = harq_layout[harq_index].size0;
		fcw->hcin_offset = harq_layout[harq_index].offset;
		fcw->hcin_size1 = harq_in_length - fcw->hcin_offset;
		harq_dma_length_in = (fcw->hcin_size0 + fcw->hcin_size1);
		if (h_comp == 1)
			harq_dma_length_in = harq_dma_length_in * 6 / 8;
	} else {
		fcw->hcin_size0 = harq_in_length;
	}
	harq_layout[harq_index].val = 0;
	rte_bbdev_log(DEBUG, "Loopback FCW Config %d %d %d\n",
			fcw->hcin_size0, fcw->hcin_offset, fcw->hcin_size1);
	fcw->hcout_size0 = harq_in_length;
	fcw->hcin_decomp_mode = h_comp;
	fcw->hcout_comp_mode = h_comp;
	fcw->gain_i = 1;
	fcw->gain_h = 1;

	/* Set the prefix of descriptor. This could be done at polling */
	acc_header_init(&desc->req);

	/* Null LLR input for Decoder */
	desc->req.data_ptrs[next_triplet].address =
			q->lb_in_addr_iova;
	desc->req.data_ptrs[next_triplet].blen = 2;
	desc->req.data_ptrs[next_triplet].blkid = ACC_DMA_BLKID_IN;
	desc->req.data_ptrs[next_triplet].last = 0;
	desc->req.data_ptrs[next_triplet].dma_ext = 0;
	next_triplet++;

	/* HARQ Combine input from either Memory interface */
	if (!ddr_mem_in) {
		next_triplet = acc_dma_fill_blk_type(&desc->req,
				op->ldpc_dec.harq_combined_input.data,
				op->ldpc_dec.harq_combined_input.offset,
				harq_dma_length_in,
				next_triplet,
				ACC_DMA_BLKID_IN_HARQ);
	} else {
		desc->req.data_ptrs[next_triplet].address =
				op->ldpc_dec.harq_combined_input.offset;
		desc->req.data_ptrs[next_triplet].blen =
				harq_dma_length_in;
		desc->req.data_ptrs[next_triplet].blkid =
				ACC_DMA_BLKID_IN_HARQ;
		desc->req.data_ptrs[next_triplet].dma_ext = 1;
		next_triplet++;
	}
	desc->req.data_ptrs[next_triplet - 1].last = 1;
	desc->req.m2dlen = next_triplet;

	/* Dropped decoder hard output */
	desc->req.data_ptrs[next_triplet].address =
			q->lb_out_addr_iova;
	desc->req.data_ptrs[next_triplet].blen = ACC_BYTES_IN_WORD;
	desc->req.data_ptrs[next_triplet].blkid = ACC_DMA_BLKID_OUT_HARD;
	desc->req.data_ptrs[next_triplet].last = 0;
	desc->req.data_ptrs[next_triplet].dma_ext = 0;
	next_triplet++;

	/* HARQ Combine output to either Memory interface */
	if (check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_OUT_ENABLE
			)) {
		desc->req.data_ptrs[next_triplet].address =
				op->ldpc_dec.harq_combined_output.offset;
		desc->req.data_ptrs[next_triplet].blen =
				harq_dma_length_out;
		desc->req.data_ptrs[next_triplet].blkid =
				ACC_DMA_BLKID_OUT_HARQ;
		desc->req.data_ptrs[next_triplet].dma_ext = 1;
		next_triplet++;
	} else {
		hq_output_head = op->ldpc_dec.harq_combined_output.data;
		hq_output = op->ldpc_dec.harq_combined_output.data;
		next_triplet = acc_dma_fill_blk_type(
				&desc->req,
				op->ldpc_dec.harq_combined_output.data,
				op->ldpc_dec.harq_combined_output.offset,
				harq_dma_length_out,
				next_triplet,
				ACC_DMA_BLKID_OUT_HARQ);
		/* HARQ output */
		mbuf_append(hq_output_head, hq_output, harq_dma_length_out);
		op->ldpc_dec.harq_combined_output.length =
				harq_dma_length_out;
	}
	desc->req.data_ptrs[next_triplet - 1].last = 1;
	desc->req.d2mlen = next_triplet - desc->req.m2dlen;
	desc->req.op_addr = op;

	/* One CB (one op) was successfully prepared to enqueue */
	return 1;
}

/* Assess whether a work around is recommended for the deRM corner cases */
static inline bool
derm_workaround_recommended(struct rte_bbdev_op_ldpc_dec *ldpc_dec, struct acc_queue *q)
{
	if (!is_acc100(q))
		return false;
	int32_t e = ldpc_dec->cb_params.e;
	int q_m = ldpc_dec->q_m;
	int z_c = ldpc_dec->z_c;
	int K = (ldpc_dec->basegraph == 1 ? ACC_K_ZC_1 : ACC_K_ZC_2) * z_c;
	bool recommended = false;

	if (ldpc_dec->basegraph == 1) {
		if ((q_m == 4) && (z_c >= 320) && (e * ACC_LIM_31 > K * 64))
			recommended = true;
		else if ((e * ACC_LIM_21 > K * 64))
			recommended = true;
	} else {
		if (q_m <= 2) {
			if ((z_c >= 208) && (e * ACC_LIM_09 > K * 64))
				recommended = true;
			else if ((z_c < 208) && (e * ACC_LIM_03 > K * 64))
				recommended = true;
		} else if (e * ACC_LIM_14 > K * 64)
			recommended = true;
	}

	return recommended;
}

/** Enqueue one decode operations for ACC100 device in CB mode */
static inline int
enqueue_ldpc_dec_one_op_cb(struct acc_queue *q, struct rte_bbdev_dec_op *op,
		uint16_t total_enqueued_cbs, bool same_op)
{
	int ret;
	if (unlikely(check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_LOOPBACK))) {
		ret = harq_loopback(q, op, total_enqueued_cbs);
		return ret;
	}

#ifndef RTE_LIBRTE_BBDEV_SKIP_VALIDATE
	/* Validate op structure */
	if (validate_ldpc_dec_op(op, q) == -1) {
		rte_bbdev_log(ERR, "LDPC decoder validation rejected");
		return -EINVAL;
	}
#endif
	union acc_dma_desc *desc;
	desc = acc_desc(q, total_enqueued_cbs);
	struct rte_mbuf *input, *h_output_head, *h_output;
	uint32_t in_offset, h_out_offset, mbuf_total_left, h_out_length = 0;
	input = op->ldpc_dec.input.data;
	h_output_head = h_output = op->ldpc_dec.hard_output.data;
	in_offset = op->ldpc_dec.input.offset;
	h_out_offset = op->ldpc_dec.hard_output.offset;
	mbuf_total_left = op->ldpc_dec.input.length;
#ifdef RTE_LIBRTE_BBDEV_DEBUG
	if (unlikely(input == NULL)) {
		rte_bbdev_log(ERR, "Invalid mbuf pointer");
		return -EFAULT;
	}
#endif
	union acc_harq_layout_data *harq_layout = q->d->harq_layout;

	if (same_op) {
		union acc_dma_desc *prev_desc;
		prev_desc = acc_desc(q, total_enqueued_cbs - 1);
		uint8_t *prev_ptr = (uint8_t *) prev_desc;
		uint8_t *new_ptr = (uint8_t *) desc;
		/* Copy first 4 words and BDESCs */
		rte_memcpy(new_ptr, prev_ptr, ACC_5GUL_SIZE_0);
		rte_memcpy(new_ptr + ACC_5GUL_OFFSET_0,
				prev_ptr + ACC_5GUL_OFFSET_0,
				ACC_5GUL_SIZE_1);
		desc->req.op_addr = prev_desc->req.op_addr;
		/* Copy FCW */
		rte_memcpy(new_ptr + ACC_DESC_FCW_OFFSET,
				prev_ptr + ACC_DESC_FCW_OFFSET,
				ACC_FCW_LD_BLEN);
		acc100_dma_desc_ld_update(op, &desc->req, input, h_output,
				&in_offset, &h_out_offset,
				&h_out_length, harq_layout);
	} else {
		struct acc_fcw_ld *fcw;
		uint32_t seg_total_left;

		if (derm_workaround_recommended(&op->ldpc_dec, q))
			rte_bbdev_log(INFO, "Corner case may require deRM pre-processing");

		fcw = &desc->req.fcw_ld;
		q->d->fcw_ld_fill(op, fcw, harq_layout);

		/* Special handling when using mbuf or not */
		if (check_bit(op->ldpc_dec.op_flags, RTE_BBDEV_LDPC_DEC_SCATTER_GATHER))
			seg_total_left = rte_pktmbuf_data_len(input) - in_offset;
		else
			seg_total_left = fcw->rm_e;

		ret = acc100_dma_desc_ld_fill(op, &desc->req, &input, h_output,
				&in_offset, &h_out_offset,
				&h_out_length, &mbuf_total_left,
				&seg_total_left, fcw);
		if (unlikely(ret < 0))
			return ret;
	}

	/* Hard output */
	mbuf_append(h_output_head, h_output, h_out_length);
#ifndef ACC100_EXT_MEM
	if (op->ldpc_dec.harq_combined_output.length > 0) {
		/* Push the HARQ output into host memory */
		struct rte_mbuf *hq_output_head, *hq_output;
		hq_output_head = op->ldpc_dec.harq_combined_output.data;
		hq_output = op->ldpc_dec.harq_combined_output.data;
		mbuf_append(hq_output_head, hq_output,
				op->ldpc_dec.harq_combined_output.length);
	}
#endif

#ifdef RTE_LIBRTE_BBDEV_DEBUG
	rte_memdump(stderr, "FCW", &desc->req.fcw_ld,
			sizeof(desc->req.fcw_ld));
	rte_memdump(stderr, "Req Desc.", desc, sizeof(*desc));
#endif

	/* One CB (one op) was successfully prepared to enqueue */
	return 1;
}


/* Enqueue one decode operations for ACC100 device in TB mode */
static inline int
enqueue_ldpc_dec_one_op_tb(struct acc_queue *q, struct rte_bbdev_dec_op *op,
		uint16_t total_enqueued_cbs, uint8_t cbs_in_tb)
{
	union acc_dma_desc *desc = NULL;
	union acc_dma_desc *desc_first = NULL;
	int ret;
	uint8_t r, c;
	uint32_t in_offset, h_out_offset,
		h_out_length, mbuf_total_left, seg_total_left;
	struct rte_mbuf *input, *h_output_head, *h_output;
	uint16_t desc_idx, current_enqueued_cbs = 0;
	uint64_t fcw_offset;
	union acc_harq_layout_data *harq_layout;

#ifndef RTE_LIBRTE_BBDEV_SKIP_VALIDATE
	/* Validate op structure */
	if (validate_ldpc_dec_op(op, q) == -1) {
		rte_bbdev_log(ERR, "LDPC decoder validation rejected");
		return -EINVAL;
	}
#endif

	desc_idx = acc_desc_idx(q, total_enqueued_cbs);
	desc = q->ring_addr + desc_idx;
	desc_first = desc;
	fcw_offset = (desc_idx << 8) + ACC_DESC_FCW_OFFSET;
	harq_layout = q->d->harq_layout;
	q->d->fcw_ld_fill(op, &desc->req.fcw_ld, harq_layout);

	input = op->ldpc_dec.input.data;
	h_output_head = h_output = op->ldpc_dec.hard_output.data;
	in_offset = op->ldpc_dec.input.offset;
	h_out_offset = op->ldpc_dec.hard_output.offset;
	h_out_length = 0;
	mbuf_total_left = op->ldpc_dec.input.length;
	c = op->ldpc_dec.tb_params.c;
	r = op->ldpc_dec.tb_params.r;

	while (mbuf_total_left > 0 && r < c) {
		if (check_bit(op->ldpc_dec.op_flags, RTE_BBDEV_LDPC_DEC_SCATTER_GATHER))
			seg_total_left = rte_pktmbuf_data_len(input) - in_offset;
		else
			seg_total_left = op->ldpc_dec.input.length;
		/* Set up DMA descriptor */
		desc = acc_desc(q, total_enqueued_cbs);
		desc->req.data_ptrs[0].address = q->ring_addr_iova + fcw_offset;
		desc->req.data_ptrs[0].blen = ACC_FCW_LD_BLEN;
		rte_memcpy(&desc->req.fcw_ld, &desc_first->req.fcw_ld, ACC_FCW_LD_BLEN);
		ret = acc100_dma_desc_ld_fill(op, &desc->req, &input,
				h_output, &in_offset, &h_out_offset,
				&h_out_length,
				&mbuf_total_left, &seg_total_left,
				&desc->req.fcw_ld);

		if (unlikely(ret < 0))
			return ret;

		/* Hard output */
		mbuf_append(h_output_head, h_output, h_out_length);

		/* Set total number of CBs in TB */
		desc->req.cbs_in_tb = cbs_in_tb;
#ifdef RTE_LIBRTE_BBDEV_DEBUG
		rte_memdump(stderr, "FCW", &desc->req.fcw_td,
				sizeof(desc->req.fcw_td) - 8);
		rte_memdump(stderr, "Req Desc.", desc, sizeof(*desc));
#endif

		if (check_bit(op->ldpc_dec.op_flags,
				RTE_BBDEV_LDPC_DEC_SCATTER_GATHER)
				&& (seg_total_left == 0)) {
			/* Go to the next mbuf */
			input = input->next;
			in_offset = 0;
			h_output = h_output->next;
			h_out_offset = 0;
		}
		total_enqueued_cbs++;
		current_enqueued_cbs++;
		r++;
	}

#ifdef RTE_LIBRTE_BBDEV_DEBUG
	if (check_mbuf_total_left(mbuf_total_left) != 0)
		return -EINVAL;
#endif
	/* Set SDone on last CB descriptor for TB mode */
	desc->req.sdone_enable = 1;

	return current_enqueued_cbs;
}

/* Enqueue one decode operations for ACC100 device in TB mode */
static inline int
enqueue_dec_one_op_tb(struct acc_queue *q, struct rte_bbdev_dec_op *op,
		uint16_t total_enqueued_cbs, uint8_t cbs_in_tb)
{
	union acc_dma_desc *desc = NULL;
	int ret;
	uint8_t r, c;
	uint32_t in_offset, h_out_offset, s_out_offset, s_out_length,
		h_out_length, mbuf_total_left, seg_total_left;
	struct rte_mbuf *input, *h_output_head, *h_output,
		*s_output_head, *s_output;
	uint16_t current_enqueued_cbs = 0;
	uint64_t fcw_offset;

#ifndef RTE_LIBRTE_BBDEV_SKIP_VALIDATE
	/* Validate op structure */
	if (cbs_in_tb == 0) {
		rte_bbdev_log(ERR, "Turbo decoder invalid number of CBs");
		return -EINVAL;
	}
	if (validate_dec_op(op, q) == -1) {
		rte_bbdev_log(ERR, "Turbo decoder validation rejected");
		return -EINVAL;
	}
#endif

	desc = acc_desc(q, total_enqueued_cbs);
	fcw_offset = (acc_desc_idx(q, total_enqueued_cbs) << 8) + ACC_DESC_FCW_OFFSET;
	acc100_fcw_td_fill(op, &desc->req.fcw_td);

	input = op->turbo_dec.input.data;
	h_output_head = h_output = op->turbo_dec.hard_output.data;
	s_output_head = s_output = op->turbo_dec.soft_output.data;
	in_offset = op->turbo_dec.input.offset;
	h_out_offset = op->turbo_dec.hard_output.offset;
	s_out_offset = op->turbo_dec.soft_output.offset;
	h_out_length = s_out_length = 0;
	mbuf_total_left = op->turbo_dec.input.length;
	c = op->turbo_dec.tb_params.c;
	r = op->turbo_dec.tb_params.r;

	while (mbuf_total_left > 0 && r < c) {

		seg_total_left = rte_pktmbuf_data_len(input) - in_offset;

		/* Set up DMA descriptor */
		desc = acc_desc(q, total_enqueued_cbs);
		desc->req.data_ptrs[0].address = q->ring_addr_iova + fcw_offset;
		desc->req.data_ptrs[0].blen = ACC_FCW_TD_BLEN;
		ret = acc100_dma_desc_td_fill(op, &desc->req, &input,
				h_output, s_output, &in_offset, &h_out_offset,
				&s_out_offset, &h_out_length, &s_out_length,
				&mbuf_total_left, &seg_total_left, r);

		if (unlikely(ret < 0))
			return ret;

		/* Hard output */
		mbuf_append(h_output_head, h_output, h_out_length);

		/* Soft output */
		if (check_bit(op->turbo_dec.op_flags,
				RTE_BBDEV_TURBO_SOFT_OUTPUT))
			mbuf_append(s_output_head, s_output, s_out_length);

		/* Set total number of CBs in TB */
		desc->req.cbs_in_tb = cbs_in_tb;
#ifdef RTE_LIBRTE_BBDEV_DEBUG
		rte_memdump(stderr, "FCW", &desc->req.fcw_td,
				sizeof(desc->req.fcw_td) - 8);
		rte_memdump(stderr, "Req Desc.", desc, sizeof(*desc));
#endif

		if (seg_total_left == 0) {
			/* Go to the next mbuf */
			input = input->next;
			in_offset = 0;
			h_output = h_output->next;
			h_out_offset = 0;

			if (check_bit(op->turbo_dec.op_flags,
					RTE_BBDEV_TURBO_SOFT_OUTPUT)) {
				s_output = s_output->next;
				s_out_offset = 0;
			}
		}

		total_enqueued_cbs++;
		current_enqueued_cbs++;
		r++;
	}

#ifdef RTE_LIBRTE_BBDEV_DEBUG
	if (check_mbuf_total_left(mbuf_total_left) != 0)
		return -EINVAL;
#endif
	/* Set SDone on last CB descriptor for TB mode */
	desc->req.sdone_enable = 1;

	return current_enqueued_cbs;
}

/* Enqueue encode operations for ACC100 device in CB mode. */
static uint16_t
acc100_enqueue_enc_cb(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_enc_op **ops, uint16_t num)
{
	struct acc_queue *q = q_data->queue_private;
	int32_t avail = acc_ring_avail_enq(q);
	uint16_t i;
	int ret;

	for (i = 0; i < num; ++i) {
		/* Check if there are available space for further processing */
		if (unlikely(avail - 1 < 0)) {
			acc_enqueue_ring_full(q_data);
			break;
		}
		avail -= 1;

		ret = enqueue_enc_one_op_cb(q, ops[i], i);
		if (ret < 0) {
			acc_enqueue_invalid(q_data);
			break;
		}
	}

	if (unlikely(i == 0))
		return 0; /* Nothing to enqueue */

	acc_dma_enqueue(q, i, &q_data->queue_stats);

	/* Update stats */
	q_data->queue_stats.enqueued_count += i;
	q_data->queue_stats.enqueue_err_count += num - i;
	return i;
}

/** Enqueue encode operations for ACC100 device in CB mode. */
static inline uint16_t
acc100_enqueue_ldpc_enc_cb(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_enc_op **ops, uint16_t num)
{
	struct acc_queue *q = q_data->queue_private;
	int32_t avail = acc_ring_avail_enq(q);
	uint16_t i = 0;
	int ret, desc_idx = 0;
	int16_t enq, left = num;

	while (left > 0) {
		if (unlikely(avail < 1)) {
			acc_enqueue_ring_full(q_data);
			break;
		}
		avail--;
		enq = RTE_MIN(left, ACC_MUX_5GDL_DESC);
		enq = check_mux(&ops[i], enq);
		if (enq > 1) {
			ret = enqueue_ldpc_enc_n_op_cb(q, &ops[i], desc_idx, enq);
			if (ret < 0) {
				acc_enqueue_invalid(q_data);
				break;
			}
			i += enq;
		} else {
			ret = enqueue_ldpc_enc_one_op_cb(q, ops[i], desc_idx);
			if (ret < 0) {
				acc_enqueue_invalid(q_data);
				break;
			}
			i++;
		}
		desc_idx++;
		left = num - i;
	}

	if (unlikely(i == 0))
		return 0; /* Nothing to enqueue */

	acc_dma_enqueue(q, desc_idx, &q_data->queue_stats);

	/* Update stats */
	q_data->queue_stats.enqueued_count += i;
	q_data->queue_stats.enqueue_err_count += num - i;

	return i;
}

/* Enqueue encode operations for ACC100 device in TB mode. */
static uint16_t
acc100_enqueue_enc_tb(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_enc_op **ops, uint16_t num)
{
	struct acc_queue *q = q_data->queue_private;
	int32_t avail = acc_ring_avail_enq(q);
	uint16_t i, enqueued_cbs = 0;
	uint8_t cbs_in_tb;
	int ret;

	for (i = 0; i < num; ++i) {
		cbs_in_tb = get_num_cbs_in_tb_enc(&ops[i]->turbo_enc);
		/* Check if there are available space for further processing */
		if (unlikely(avail - cbs_in_tb < 0)) {
			acc_enqueue_ring_full(q_data);
			break;
		}
		avail -= cbs_in_tb;

		ret = enqueue_enc_one_op_tb(q, ops[i], enqueued_cbs, cbs_in_tb);
		if (ret < 0) {
			acc_enqueue_invalid(q_data);
			break;
		}
		enqueued_cbs += ret;
	}
	if (unlikely(enqueued_cbs == 0))
		return 0; /* Nothing to enqueue */

	acc_dma_enqueue(q, enqueued_cbs, &q_data->queue_stats);

	/* Update stats */
	q_data->queue_stats.enqueued_count += i;
	q_data->queue_stats.enqueue_err_count += num - i;

	return i;
}

/* Enqueue LDPC encode operations for ACC100 device in TB mode. */
static uint16_t
acc100_enqueue_ldpc_enc_tb(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_enc_op **ops, uint16_t num)
{
	struct acc_queue *q = q_data->queue_private;
	int32_t avail = acc_ring_avail_enq(q);
	uint16_t i, enqueued_descs = 0;
	uint8_t cbs_in_tb;
	int descs_used;

	for (i = 0; i < num; ++i) {
		cbs_in_tb = get_num_cbs_in_tb_ldpc_enc(&ops[i]->ldpc_enc);
		/* Check if there are available space for further processing. */
		if (unlikely(avail - cbs_in_tb < 0)) {
			acc_enqueue_ring_full(q_data);
			break;
		}
		descs_used = enqueue_ldpc_enc_one_op_tb(q, ops[i], enqueued_descs, cbs_in_tb);
		if (descs_used < 0) {
			acc_enqueue_invalid(q_data);
			break;
		}
		enqueued_descs += descs_used;
		avail -= descs_used;
	}
	if (unlikely(enqueued_descs == 0))
		return 0; /* Nothing to enqueue. */

	acc_dma_enqueue(q, enqueued_descs, &q_data->queue_stats);

	/* Update stats. */
	q_data->queue_stats.enqueued_count += i;
	q_data->queue_stats.enqueue_err_count += num - i;

	return i;
}

/* Enqueue encode operations for ACC100 device. */
static uint16_t
acc100_enqueue_enc(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_enc_op **ops, uint16_t num)
{
	int32_t aq_avail = acc_aq_avail(q_data, num);
	if (unlikely((aq_avail <= 0) || (num == 0)))
		return 0;
	if (ops[0]->turbo_enc.code_block_mode == RTE_BBDEV_TRANSPORT_BLOCK)
		return acc100_enqueue_enc_tb(q_data, ops, num);
	else
		return acc100_enqueue_enc_cb(q_data, ops, num);
}

/* Enqueue encode operations for ACC100 device. */
static uint16_t
acc100_enqueue_ldpc_enc(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_enc_op **ops, uint16_t num)
{
	int32_t aq_avail = acc_aq_avail(q_data, num);
	if (unlikely((aq_avail <= 0) || (num == 0)))
		return 0;
	if (ops[0]->ldpc_enc.code_block_mode == RTE_BBDEV_TRANSPORT_BLOCK)
		return acc100_enqueue_ldpc_enc_tb(q_data, ops, num);
	else
		return acc100_enqueue_ldpc_enc_cb(q_data, ops, num);
}


/* Enqueue decode operations for ACC100 device in CB mode */
static uint16_t
acc100_enqueue_dec_cb(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_dec_op **ops, uint16_t num)
{
	struct acc_queue *q = q_data->queue_private;
	int32_t avail = acc_ring_avail_enq(q);
	uint16_t i;
	int ret;

	for (i = 0; i < num; ++i) {
		/* Check if there are available space for further processing */
		if (unlikely(avail - 1 < 0)) {
			acc_enqueue_ring_full(q_data);
			break;
		}
		avail -= 1;

		ret = enqueue_dec_one_op_cb(q, ops[i], i);
		if (ret < 0) {
			acc_enqueue_invalid(q_data);
			break;
		}
	}

	if (unlikely(i == 0))
		return 0; /* Nothing to enqueue */

	acc_dma_enqueue(q, i, &q_data->queue_stats);

	/* Update stats */
	q_data->queue_stats.enqueued_count += i;
	q_data->queue_stats.enqueue_err_count += num - i;

	return i;
}

/* Enqueue decode operations for ACC100 device in TB mode */
static uint16_t
acc100_enqueue_ldpc_dec_tb(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_dec_op **ops, uint16_t num)
{
	struct acc_queue *q = q_data->queue_private;
	int32_t avail = acc_ring_avail_enq(q);
	uint16_t i, enqueued_cbs = 0;
	uint8_t cbs_in_tb;
	int ret;

	for (i = 0; i < num; ++i) {
		cbs_in_tb = get_num_cbs_in_tb_ldpc_dec(&ops[i]->ldpc_dec);
		/* Check if there are available space for further processing */
		if (unlikely(avail - cbs_in_tb < 0))
			break;
		avail -= cbs_in_tb;

		ret = enqueue_ldpc_dec_one_op_tb(q, ops[i],
				enqueued_cbs, cbs_in_tb);
		if (ret < 0) {
			acc_enqueue_invalid(q_data);
			break;
		}
		enqueued_cbs += ret;
	}
	if (unlikely(enqueued_cbs == 0))
		return 0; /* Nothing to enqueue */

	acc_dma_enqueue(q, enqueued_cbs, &q_data->queue_stats);

	/* Update stats */
	q_data->queue_stats.enqueued_count += i;
	q_data->queue_stats.enqueue_err_count += num - i;
	return i;
}

/* Enqueue decode operations for ACC100 device in CB mode */
static uint16_t
acc100_enqueue_ldpc_dec_cb(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_dec_op **ops, uint16_t num)
{
	struct acc_queue *q = q_data->queue_private;
	int32_t avail = acc_ring_avail_enq(q);
	uint16_t i;
	int ret;
	bool same_op = false;
	for (i = 0; i < num; ++i) {
		/* Check if there are available space for further processing */
		if (unlikely(avail < 1)) {
			acc_enqueue_ring_full(q_data);
			break;
		}
		avail -= 1;

		if (i > 0)
			same_op = cmp_ldpc_dec_op(&ops[i-1]);
		rte_bbdev_log(INFO, "Op %d %d %d %d %d %d %d %d %d %d %d %d\n",
			i, ops[i]->ldpc_dec.op_flags, ops[i]->ldpc_dec.rv_index,
			ops[i]->ldpc_dec.iter_max, ops[i]->ldpc_dec.iter_count,
			ops[i]->ldpc_dec.basegraph, ops[i]->ldpc_dec.z_c,
			ops[i]->ldpc_dec.n_cb, ops[i]->ldpc_dec.q_m,
			ops[i]->ldpc_dec.n_filler, ops[i]->ldpc_dec.cb_params.e,
			same_op);
		ret = enqueue_ldpc_dec_one_op_cb(q, ops[i], i, same_op);
		if (ret < 0) {
			acc_enqueue_invalid(q_data);
			break;
		}
	}

	if (unlikely(i == 0))
		return 0; /* Nothing to enqueue */

	acc_dma_enqueue(q, i, &q_data->queue_stats);

	/* Update stats */
	q_data->queue_stats.enqueued_count += i;
	q_data->queue_stats.enqueue_err_count += num - i;
	return i;
}


/* Enqueue decode operations for ACC100 device in TB mode */
static uint16_t
acc100_enqueue_dec_tb(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_dec_op **ops, uint16_t num)
{
	struct acc_queue *q = q_data->queue_private;
	int32_t avail = acc_ring_avail_enq(q);
	uint16_t i, enqueued_cbs = 0;
	uint8_t cbs_in_tb;
	int ret;

	for (i = 0; i < num; ++i) {
		cbs_in_tb = get_num_cbs_in_tb_dec(&ops[i]->turbo_dec);
		/* Check if there are available space for further processing */
		if (unlikely(avail - cbs_in_tb < 0)) {
			acc_enqueue_ring_full(q_data);
			break;
		}
		avail -= cbs_in_tb;

		ret = enqueue_dec_one_op_tb(q, ops[i], enqueued_cbs, cbs_in_tb);
		if (ret < 0) {
			acc_enqueue_invalid(q_data);
			break;
		}
		enqueued_cbs += ret;
	}

	acc_dma_enqueue(q, enqueued_cbs, &q_data->queue_stats);

	/* Update stats */
	q_data->queue_stats.enqueued_count += i;
	q_data->queue_stats.enqueue_err_count += num - i;

	return i;
}

/* Enqueue decode operations for ACC100 device. */
static uint16_t
acc100_enqueue_dec(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_dec_op **ops, uint16_t num)
{
	int32_t aq_avail = acc_aq_avail(q_data, num);

	if (unlikely((aq_avail <= 0) || (num == 0)))
		return 0;

	if (ops[0]->turbo_dec.code_block_mode == RTE_BBDEV_TRANSPORT_BLOCK)
		return acc100_enqueue_dec_tb(q_data, ops, num);
	else
		return acc100_enqueue_dec_cb(q_data, ops, num);
}

/* Enqueue decode operations for ACC100 device. */
static uint16_t
acc100_enqueue_ldpc_dec(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_dec_op **ops, uint16_t num)
{
	int32_t aq_avail = acc_aq_avail(q_data, num);

	if (unlikely((aq_avail <= 0) || (num == 0)))
		return 0;

	if (ops[0]->ldpc_dec.code_block_mode == RTE_BBDEV_TRANSPORT_BLOCK)
		return acc100_enqueue_ldpc_dec_tb(q_data, ops, num);
	else
		return acc100_enqueue_ldpc_dec_cb(q_data, ops, num);
}

/* Dequeue one encode operations from ACC100 device in CB mode */
static inline int
dequeue_enc_one_op_cb(struct acc_queue *q, struct rte_bbdev_enc_op **ref_op,
		uint16_t *dequeued_ops, uint32_t *aq_dequeued,
		uint16_t *dequeued_descs)
{
	union acc_dma_desc *desc, atom_desc;
	union acc_dma_rsp_desc rsp;
	struct rte_bbdev_enc_op *op;
	int i;
	uint16_t desc_idx;

	desc_idx = acc_desc_idx_tail(q, *dequeued_descs);
	desc = q->ring_addr + desc_idx;
	atom_desc.atom_hdr = __atomic_load_n((uint64_t *)desc,
			__ATOMIC_RELAXED);

	/* Check fdone bit */
	if (!(atom_desc.rsp.val & ACC_FDONE))
		return -1;

	rsp.val = atom_desc.rsp.val;
	rte_bbdev_log_debug("Resp. desc %p: %x num %d\n", desc, rsp.val, desc->req.numCBs);

	/* Dequeue */
	op = desc->req.op_addr;

	/* Clearing status, it will be set based on response */
	op->status = 0;
	op->status |= ((rsp.dma_err) ? (1 << RTE_BBDEV_DRV_ERROR) : 0);
	op->status |= ((rsp.fcw_err) ? (1 << RTE_BBDEV_DRV_ERROR) : 0);

	if (desc->req.last_desc_in_batch) {
		(*aq_dequeued)++;
		desc->req.last_desc_in_batch = 0;
	}
	desc->rsp.val = ACC_DMA_DESC_TYPE;
	desc->rsp.add_info_0 = 0; /*Reserved bits */
	desc->rsp.add_info_1 = 0; /*Reserved bits */

	ref_op[0] = op;
	struct acc_ptrs *context_ptrs = q->companion_ring_addr + desc_idx;
	for (i = 1 ; i < desc->req.numCBs; i++)
		ref_op[i] = context_ptrs->ptr[i].op_addr;

	/* One CB (op) was successfully dequeued */
	/* One op was successfully dequeued */
	(*dequeued_descs)++;
	*dequeued_ops += desc->req.numCBs;

	/* One CB (op) was successfully dequeued */
	return desc->req.numCBs;
}

/* Dequeue one LDPC encode operations from ACC100 device in TB mode
 * That operation may cover multiple descriptors
 */
static inline int
dequeue_enc_one_op_tb(struct acc_queue *q, struct rte_bbdev_enc_op **ref_op,
		uint16_t *dequeued_ops, uint32_t *aq_dequeued,
		uint16_t *dequeued_descs)
{
	union acc_dma_desc *desc, *last_desc, atom_desc;
	union acc_dma_rsp_desc rsp;
	struct rte_bbdev_enc_op *op;
	uint8_t i = 0;
	uint16_t current_dequeued_descs = 0, descs_in_tb;

	desc = acc_desc_tail(q, *dequeued_descs);
	atom_desc.atom_hdr = __atomic_load_n((uint64_t *)desc,
			__ATOMIC_RELAXED);

	/* Check fdone bit */
	if (!(atom_desc.rsp.val & ACC_FDONE))
		return -1;

	/* Get number of CBs in dequeued TB */
	descs_in_tb = desc->req.cbs_in_tb;
	/* Get last CB */
	last_desc = acc_desc_tail(q, *dequeued_descs + descs_in_tb - 1);
	/* Check if last CB in TB is ready to dequeue (and thus
	 * the whole TB) - checking sdone bit. If not return.
	 */
	atom_desc.atom_hdr = __atomic_load_n((uint64_t *)last_desc,
			__ATOMIC_RELAXED);
	if (!(atom_desc.rsp.val & ACC_SDONE))
		return -1;

	/* Dequeue */
	op = desc->req.op_addr;

	/* Clearing status, it will be set based on response */
	op->status = 0;

	while (i < descs_in_tb) {
		desc = acc_desc_tail(q, *dequeued_descs);
		atom_desc.atom_hdr = __atomic_load_n((uint64_t *)desc,
				__ATOMIC_RELAXED);
		rsp.val = atom_desc.rsp.val;
		rte_bbdev_log_debug("Resp. desc %p: %x descs %d cbs %d\n",
				desc, rsp.val, descs_in_tb, desc->req.numCBs);

		op->status |= ((rsp.dma_err) ? (1 << RTE_BBDEV_DRV_ERROR) : 0);
		op->status |= ((rsp.fcw_err) ? (1 << RTE_BBDEV_DRV_ERROR) : 0);

		if (desc->req.last_desc_in_batch) {
			(*aq_dequeued)++;
			desc->req.last_desc_in_batch = 0;
		}
		desc->rsp.val = ACC_DMA_DESC_TYPE;
		desc->rsp.add_info_0 = 0;
		desc->rsp.add_info_1 = 0;
		(*dequeued_descs)++;
		current_dequeued_descs++;
		i++;
	}

	*ref_op = op;

	(*dequeued_ops)++;
	return current_dequeued_descs;
}

/* Dequeue one decode operation from ACC100 device in CB mode */
static inline int
dequeue_dec_one_op_cb(struct rte_bbdev_queue_data *q_data,
		struct acc_queue *q, struct rte_bbdev_dec_op **ref_op,
		uint16_t dequeued_cbs, uint32_t *aq_dequeued)
{
	union acc_dma_desc *desc, atom_desc;
	union acc_dma_rsp_desc rsp;
	struct rte_bbdev_dec_op *op;

	desc = acc_desc_tail(q, dequeued_cbs);
	atom_desc.atom_hdr = __atomic_load_n((uint64_t *)desc,
			__ATOMIC_RELAXED);

	/* Check fdone bit */
	if (!(atom_desc.rsp.val & ACC_FDONE))
		return -1;

	rsp.val = atom_desc.rsp.val;
	rte_bbdev_log_debug("Resp. desc %p: %x", desc, rsp.val);

	/* Dequeue */
	op = desc->req.op_addr;

	/* Clearing status, it will be set based on response */
	op->status = 0;
	op->status |= ((rsp.input_err)
			? (1 << RTE_BBDEV_DATA_ERROR) : 0);
	op->status |= ((rsp.dma_err) ? (1 << RTE_BBDEV_DRV_ERROR) : 0);
	op->status |= ((rsp.fcw_err) ? (1 << RTE_BBDEV_DRV_ERROR) : 0);
	if (op->status != 0) {
		q_data->queue_stats.dequeue_err_count++;
		acc100_check_ir(q->d);
	}

	/* CRC invalid if error exists */
	if (!op->status)
		op->status |= rsp.crc_status << RTE_BBDEV_CRC_ERROR;
	op->turbo_dec.iter_count = (uint8_t) rsp.iter_cnt / 2;
	/* Check if this is the last desc in batch (Atomic Queue) */
	if (desc->req.last_desc_in_batch) {
		(*aq_dequeued)++;
		desc->req.last_desc_in_batch = 0;
	}
	desc->rsp.val = ACC_DMA_DESC_TYPE;
	desc->rsp.add_info_0 = 0;
	desc->rsp.add_info_1 = 0;
	*ref_op = op;

	/* One CB (op) was successfully dequeued */
	return 1;
}

/* Dequeue one decode operations from ACC100 device in CB mode */
static inline int
dequeue_ldpc_dec_one_op_cb(struct rte_bbdev_queue_data *q_data,
		struct acc_queue *q, struct rte_bbdev_dec_op **ref_op,
		uint16_t dequeued_cbs, uint32_t *aq_dequeued)
{
	union acc_dma_desc *desc, atom_desc;
	union acc_dma_rsp_desc rsp;
	struct rte_bbdev_dec_op *op;

	desc = acc_desc_tail(q, dequeued_cbs);
	atom_desc.atom_hdr = __atomic_load_n((uint64_t *)desc,
			__ATOMIC_RELAXED);

	/* Check fdone bit */
	if (!(atom_desc.rsp.val & ACC_FDONE))
		return -1;

	rsp.val = atom_desc.rsp.val;
	rte_bbdev_log_debug("Resp. desc %p: %x\n", desc, rsp.val);

	/* Dequeue */
	op = desc->req.op_addr;

	/* Clearing status, it will be set based on response */
	op->status = 0;
	op->status |= rsp.input_err << RTE_BBDEV_DATA_ERROR;
	op->status |= rsp.dma_err << RTE_BBDEV_DRV_ERROR;
	op->status |= rsp.fcw_err << RTE_BBDEV_DRV_ERROR;
	if (op->status != 0)
		q_data->queue_stats.dequeue_err_count++;

	op->status |= rsp.crc_status << RTE_BBDEV_CRC_ERROR;
	if (op->ldpc_dec.hard_output.length > 0 && !rsp.synd_ok)
		op->status |= 1 << RTE_BBDEV_SYNDROME_ERROR;
	op->ldpc_dec.iter_count = (uint8_t) rsp.iter_cnt;

	if (op->status & (1 << RTE_BBDEV_DRV_ERROR))
		acc100_check_ir(q->d);

	/* Check if this is the last desc in batch (Atomic Queue) */
	if (desc->req.last_desc_in_batch) {
		(*aq_dequeued)++;
		desc->req.last_desc_in_batch = 0;
	}

	desc->rsp.val = ACC_DMA_DESC_TYPE;
	desc->rsp.add_info_0 = 0;
	desc->rsp.add_info_1 = 0;

	*ref_op = op;

	/* One CB (op) was successfully dequeued */
	return 1;
}

/* Dequeue one decode operations from ACC100 device in TB mode. */
static inline int
dequeue_dec_one_op_tb(struct acc_queue *q, struct rte_bbdev_dec_op **ref_op,
		uint16_t dequeued_cbs, uint32_t *aq_dequeued)
{
	union acc_dma_desc *desc, *last_desc, atom_desc;
	union acc_dma_rsp_desc rsp;
	struct rte_bbdev_dec_op *op;
	uint8_t cbs_in_tb = 1, cb_idx = 0;

	desc = acc_desc_tail(q, dequeued_cbs);
	atom_desc.atom_hdr = __atomic_load_n((uint64_t *)desc,
			__ATOMIC_RELAXED);

	/* Check fdone bit */
	if (!(atom_desc.rsp.val & ACC_FDONE))
		return -1;

	/* Dequeue */
	op = desc->req.op_addr;

	/* Get number of CBs in dequeued TB */
	cbs_in_tb = desc->req.cbs_in_tb;
	/* Get last CB */
	last_desc = acc_desc_tail(q, dequeued_cbs + cbs_in_tb - 1);
	/* Check if last CB in TB is ready to dequeue (and thus
	 * the whole TB) - checking sdone bit. If not return.
	 */
	atom_desc.atom_hdr = __atomic_load_n((uint64_t *)last_desc,
			__ATOMIC_RELAXED);
	if (!(atom_desc.rsp.val & ACC_SDONE))
		return -1;

	/* Clearing status, it will be set based on response */
	op->status = 0;

	/* Read remaining CBs if exists */
	while (cb_idx < cbs_in_tb) {
		desc = acc_desc_tail(q, dequeued_cbs);
		atom_desc.atom_hdr = __atomic_load_n((uint64_t *)desc,
				__ATOMIC_RELAXED);
		rsp.val = atom_desc.rsp.val;
		rte_bbdev_log_debug("Resp. desc %p: %x r %d c %d\n",
						desc, rsp.val, cb_idx, cbs_in_tb);

		op->status |= ((rsp.input_err) ? (1 << RTE_BBDEV_DATA_ERROR) : 0);
		op->status |= ((rsp.dma_err) ? (1 << RTE_BBDEV_DRV_ERROR) : 0);
		op->status |= ((rsp.fcw_err) ? (1 << RTE_BBDEV_DRV_ERROR) : 0);

		/* CRC invalid if error exists */
		if (!op->status)
			op->status |= rsp.crc_status << RTE_BBDEV_CRC_ERROR;
		if (q->op_type == RTE_BBDEV_OP_LDPC_DEC)
			op->ldpc_dec.iter_count = RTE_MAX((uint8_t) rsp.iter_cnt,
					op->ldpc_dec.iter_count);
		else
			op->turbo_dec.iter_count = RTE_MAX((uint8_t) rsp.iter_cnt,
					op->turbo_dec.iter_count);

		/* Check if this is the last desc in batch (Atomic Queue) */
		if (desc->req.last_desc_in_batch) {
			(*aq_dequeued)++;
			desc->req.last_desc_in_batch = 0;
		}
		desc->rsp.val = ACC_DMA_DESC_TYPE;
		desc->rsp.add_info_0 = 0;
		desc->rsp.add_info_1 = 0;
		dequeued_cbs++;
		cb_idx++;
	}

	*ref_op = op;

	return cb_idx;
}

/* Dequeue encode operations from ACC100 device. */
static uint16_t
acc100_dequeue_enc(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_enc_op **ops, uint16_t num)
{
	struct acc_queue *q = q_data->queue_private;
	uint32_t avail = acc_ring_avail_deq(q);
	uint32_t aq_dequeued = 0;
	uint16_t i, dequeued_ops = 0, dequeued_descs = 0;
	int ret, cbm;
	struct rte_bbdev_enc_op *op;

	if (avail == 0)
		return 0;
#ifdef RTE_LIBRTE_BBDEV_DEBUG
	if (unlikely(ops == NULL || q == NULL)) {
		rte_bbdev_log_debug("Unexpected undefined pointer");
		return 0;
	}
#endif
	op = acc_op_tail(q, 0);
	if (unlikely(ops == NULL || op == NULL))
		return 0;
	cbm = op->turbo_enc.code_block_mode;

	for (i = 0; i < num; i++) {
		if (cbm == RTE_BBDEV_TRANSPORT_BLOCK)
			ret = dequeue_enc_one_op_tb(q, &ops[dequeued_ops],
					&dequeued_ops, &aq_dequeued,
					&dequeued_descs);
		else
			ret = dequeue_enc_one_op_cb(q, &ops[dequeued_ops],
					&dequeued_ops, &aq_dequeued,
					&dequeued_descs);

		if (ret < 0)
			break;

		if (dequeued_ops >= num)
			break;
	}

	q->aq_dequeued += aq_dequeued;
	q->sw_ring_tail += dequeued_descs;

	/* Update enqueue stats */
	q_data->queue_stats.dequeued_count += dequeued_ops;

	return dequeued_ops;
}

/* Dequeue LDPC encode operations from ACC100 device. */
static uint16_t
acc100_dequeue_ldpc_enc(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_enc_op **ops, uint16_t num)
{
	struct acc_queue *q = q_data->queue_private;
	uint32_t avail = acc_ring_avail_deq(q);
	uint32_t aq_dequeued = 0;
	uint16_t i, dequeued_ops = 0, dequeued_descs = 0;
	int ret, cbm;
	struct rte_bbdev_enc_op *op;

	if (avail == 0)
		return 0;

	op = acc_op_tail(q, 0);
	if (unlikely(ops == NULL || op == NULL))
		return 0;
	cbm = op->ldpc_enc.code_block_mode;
	for (i = 0; i < avail; i++) {
		if (cbm == RTE_BBDEV_TRANSPORT_BLOCK)
			ret = dequeue_enc_one_op_tb(q, &ops[dequeued_ops],
					&dequeued_ops, &aq_dequeued,
					&dequeued_descs);
		else
			ret = dequeue_enc_one_op_cb(q, &ops[dequeued_ops],
					&dequeued_ops, &aq_dequeued,
					&dequeued_descs);
		if (ret < 0)
			break;
		if (dequeued_ops >= num)
			break;
	}

	q->aq_dequeued += aq_dequeued;
	q->sw_ring_tail += dequeued_descs;

	/* Update enqueue stats */
	q_data->queue_stats.dequeued_count += dequeued_ops;

	return dequeued_ops;
}

/* Dequeue decode operations from ACC100 device. */
static uint16_t
acc100_dequeue_dec(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_dec_op **ops, uint16_t num)
{
	struct acc_queue *q = q_data->queue_private;
	uint16_t dequeue_num;
	uint32_t avail = acc_ring_avail_deq(q);
	uint32_t aq_dequeued = 0;
	uint16_t i;
	uint16_t dequeued_cbs = 0;
	struct rte_bbdev_dec_op *op;
	int ret;

#ifdef RTE_LIBRTE_BBDEV_DEBUG
	if (unlikely(ops == 0 && q == NULL))
		return 0;
#endif

	dequeue_num = (avail < num) ? avail : num;

	for (i = 0; i < dequeue_num; ++i) {
		op = acc_op_tail(q, dequeued_cbs);
		if (unlikely(op == NULL))
			break;
		if (op->turbo_dec.code_block_mode == RTE_BBDEV_TRANSPORT_BLOCK)
			ret = dequeue_dec_one_op_tb(q, &ops[i], dequeued_cbs,
					&aq_dequeued);
		else
			ret = dequeue_dec_one_op_cb(q_data, q, &ops[i],
					dequeued_cbs, &aq_dequeued);

		if (ret < 0)
			break;
		dequeued_cbs += ret;
	}

	q->aq_dequeued += aq_dequeued;
	q->sw_ring_tail += dequeued_cbs;

	/* Update enqueue stats */
	q_data->queue_stats.dequeued_count += i;

	return i;
}

/* Dequeue decode operations from ACC100 device. */
static uint16_t
acc100_dequeue_ldpc_dec(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_dec_op **ops, uint16_t num)
{
	struct acc_queue *q = q_data->queue_private;
	uint16_t dequeue_num;
	uint32_t avail = acc_ring_avail_deq(q);
	uint32_t aq_dequeued = 0;
	uint16_t i;
	uint16_t dequeued_cbs = 0;
	struct rte_bbdev_dec_op *op;
	int ret;

#ifdef RTE_LIBRTE_BBDEV_DEBUG
	if (unlikely(ops == 0 && q == NULL))
		return 0;
#endif

	dequeue_num = RTE_MIN(avail, num);

	for (i = 0; i < dequeue_num; ++i) {
		op = acc_op_tail(q, dequeued_cbs);
		if (unlikely(op == NULL))
			break;
		if (op->ldpc_dec.code_block_mode == RTE_BBDEV_TRANSPORT_BLOCK)
			ret = dequeue_dec_one_op_tb(q, &ops[i], dequeued_cbs,
					&aq_dequeued);
		else
			ret = dequeue_ldpc_dec_one_op_cb(
					q_data, q, &ops[i], dequeued_cbs,
					&aq_dequeued);

		if (ret < 0)
			break;
		dequeued_cbs += ret;
	}

	q->aq_dequeued += aq_dequeued;
	q->sw_ring_tail += dequeued_cbs;

	/* Update enqueue stats */
	q_data->queue_stats.dequeued_count += i;

	return i;
}

/* Initialization Function */
static void
acc100_bbdev_init(struct rte_bbdev *dev, struct rte_pci_driver *drv)
{
	struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI(dev->device);

	dev->dev_ops = &acc100_bbdev_ops;
	dev->enqueue_enc_ops = acc100_enqueue_enc;
	dev->enqueue_dec_ops = acc100_enqueue_dec;
	dev->dequeue_enc_ops = acc100_dequeue_enc;
	dev->dequeue_dec_ops = acc100_dequeue_dec;
	dev->enqueue_ldpc_enc_ops = acc100_enqueue_ldpc_enc;
	dev->enqueue_ldpc_dec_ops = acc100_enqueue_ldpc_dec;
	dev->dequeue_ldpc_enc_ops = acc100_dequeue_ldpc_enc;
	dev->dequeue_ldpc_dec_ops = acc100_dequeue_ldpc_dec;

	/* Device variant specific handling */
	if ((pci_dev->id.device_id == ACC100_PF_DEVICE_ID) ||
			(pci_dev->id.device_id == ACC100_VF_DEVICE_ID)) {
		((struct acc_device *) dev->data->dev_private)->device_variant = ACC100_VARIANT;
		((struct acc_device *) dev->data->dev_private)->fcw_ld_fill = acc100_fcw_ld_fill;
	} else {
		((struct acc_device *) dev->data->dev_private)->device_variant = ACC101_VARIANT;
		((struct acc_device *) dev->data->dev_private)->fcw_ld_fill = acc101_fcw_ld_fill;
	}

	((struct acc_device *) dev->data->dev_private)->pf_device =
			!strcmp(drv->driver.name, RTE_STR(ACC100PF_DRIVER_NAME));

	((struct acc_device *) dev->data->dev_private)->mmio_base =
			pci_dev->mem_resource[0].addr;

	rte_bbdev_log_debug("Init device %s [%s] @ vaddr %p paddr %#"PRIx64"",
			drv->driver.name, dev->data->name,
			(void *)pci_dev->mem_resource[0].addr,
			pci_dev->mem_resource[0].phys_addr);
}

static int acc100_pci_probe(struct rte_pci_driver *pci_drv,
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
			sizeof(struct acc_device), RTE_CACHE_LINE_SIZE,
			pci_dev->device.numa_node);

	if (bbdev->data->dev_private == NULL) {
		rte_bbdev_log(CRIT,
				"Allocate of %zu bytes for device \"%s\" failed",
				sizeof(struct acc_device), dev_name);
				rte_bbdev_release(bbdev);
			return -ENOMEM;
	}

	/* Fill HW specific part of device structure */
	bbdev->device = &pci_dev->device;
	bbdev->intr_handle = pci_dev->intr_handle;
	bbdev->data->socket_id = pci_dev->device.numa_node;

	/* Invoke ACC100 device initialization function */
	acc100_bbdev_init(bbdev, pci_drv);

	rte_bbdev_log_debug("Initialised bbdev %s (id = %u)",
			dev_name, bbdev->data->dev_id);
	return 0;
}

static struct rte_pci_driver acc100_pci_pf_driver = {
		.probe = acc100_pci_probe,
		.remove = acc_pci_remove,
		.id_table = pci_id_acc100_pf_map,
		.drv_flags = RTE_PCI_DRV_NEED_MAPPING
};

static struct rte_pci_driver acc100_pci_vf_driver = {
		.probe = acc100_pci_probe,
		.remove = acc_pci_remove,
		.id_table = pci_id_acc100_vf_map,
		.drv_flags = RTE_PCI_DRV_NEED_MAPPING
};

RTE_PMD_REGISTER_PCI(ACC100PF_DRIVER_NAME, acc100_pci_pf_driver);
RTE_PMD_REGISTER_PCI_TABLE(ACC100PF_DRIVER_NAME, pci_id_acc100_pf_map);
RTE_PMD_REGISTER_PCI(ACC100VF_DRIVER_NAME, acc100_pci_vf_driver);
RTE_PMD_REGISTER_PCI_TABLE(ACC100VF_DRIVER_NAME, pci_id_acc100_vf_map);

/*
 * Workaround implementation to fix the power on status of some 5GUL engines
 * This requires DMA permission if ported outside DPDK
 * It consists in resolving the state of these engines by running a
 * dummy operation and resetting the engines to ensure state are reliably
 * defined.
 */
static void
poweron_cleanup(struct rte_bbdev *bbdev, struct acc_device *d,
		struct rte_acc_conf *conf)
{
	int i, template_idx, qg_idx;
	uint32_t address, status, value;
	rte_bbdev_log(WARNING, "Need to clear power-on 5GUL status in internal memory");
	/* Reset LDPC Cores */
	for (i = 0; i < ACC100_ENGINES_MAX; i++)
		acc_reg_write(d, HWPfFecUl5gCntrlReg +
				ACC_ENGINE_OFFSET * i, ACC100_RESET_HI);
	usleep(ACC_LONG_WAIT);
	for (i = 0; i < ACC100_ENGINES_MAX; i++)
		acc_reg_write(d, HWPfFecUl5gCntrlReg +
				ACC_ENGINE_OFFSET * i, ACC100_RESET_LO);
	usleep(ACC_LONG_WAIT);
	/* Prepare dummy workload */
	alloc_2x64mb_sw_rings_mem(bbdev, d, 0);
	/* Set base addresses */
	uint32_t phys_high = (uint32_t)(d->sw_rings_iova >> 32);
	uint32_t phys_low  = (uint32_t)(d->sw_rings_iova &
			~(ACC_SIZE_64MBYTE-1));
	acc_reg_write(d, HWPfDmaFec5GulDescBaseHiRegVf, phys_high);
	acc_reg_write(d, HWPfDmaFec5GulDescBaseLoRegVf, phys_low);

	/* Descriptor for a dummy 5GUL code block processing*/
	union acc_dma_desc *desc = NULL;
	desc = d->sw_rings;
	desc->req.data_ptrs[0].address = d->sw_rings_iova +
			ACC_DESC_FCW_OFFSET;
	desc->req.data_ptrs[0].blen = ACC_FCW_LD_BLEN;
	desc->req.data_ptrs[0].blkid = ACC_DMA_BLKID_FCW;
	desc->req.data_ptrs[0].last = 0;
	desc->req.data_ptrs[0].dma_ext = 0;
	desc->req.data_ptrs[1].address = d->sw_rings_iova + 512;
	desc->req.data_ptrs[1].blkid = ACC_DMA_BLKID_IN;
	desc->req.data_ptrs[1].last = 1;
	desc->req.data_ptrs[1].dma_ext = 0;
	desc->req.data_ptrs[1].blen = 44;
	desc->req.data_ptrs[2].address = d->sw_rings_iova + 1024;
	desc->req.data_ptrs[2].blkid = ACC_DMA_BLKID_OUT_ENC;
	desc->req.data_ptrs[2].last = 1;
	desc->req.data_ptrs[2].dma_ext = 0;
	desc->req.data_ptrs[2].blen = 5;
	/* Dummy FCW */
	desc->req.fcw_ld.FCWversion = ACC_FCW_VER;
	desc->req.fcw_ld.qm = 1;
	desc->req.fcw_ld.nfiller = 30;
	desc->req.fcw_ld.BG = 2 - 1;
	desc->req.fcw_ld.Zc = 7;
	desc->req.fcw_ld.ncb = 350;
	desc->req.fcw_ld.rm_e = 4;
	desc->req.fcw_ld.itmax = 10;
	desc->req.fcw_ld.gain_i = 1;
	desc->req.fcw_ld.gain_h = 1;

	int engines_to_restart[ACC100_SIG_UL_5G_LAST + 1] = {0};
	int num_failed_engine = 0;
	/* Detect engines in undefined state */
	for (template_idx = ACC100_SIG_UL_5G;
			template_idx <= ACC100_SIG_UL_5G_LAST;
			template_idx++) {
		/* Check engine power-on status */
		address = HwPfFecUl5gIbDebugReg +
				ACC_ENGINE_OFFSET * template_idx;
		status = (acc_reg_read(d, address) >> 4) & 0xF;
		if (status == 0) {
			engines_to_restart[num_failed_engine] = template_idx;
			num_failed_engine++;
		}
	}

	int numQqsAcc = conf->q_ul_5g.num_qgroups;
	int numQgs = conf->q_ul_5g.num_qgroups;
	value = 0;
	for (qg_idx = numQqsAcc; qg_idx < (numQgs + numQqsAcc); qg_idx++)
		value |= (1 << qg_idx);
	/* Force each engine which is in unspecified state */
	for (i = 0; i < num_failed_engine; i++) {
		int failed_engine = engines_to_restart[i];
		rte_bbdev_log(WARNING, "Force engine %d", failed_engine);
		for (template_idx = ACC100_SIG_UL_5G;
				template_idx <= ACC100_SIG_UL_5G_LAST;
				template_idx++) {
			address = HWPfQmgrGrpTmplateReg4Indx
					+ ACC_BYTES_IN_WORD * template_idx;
			if (template_idx == failed_engine)
				acc_reg_write(d, address, value);
			else
				acc_reg_write(d, address, 0);
		}
		/* Reset descriptor header */
		desc->req.word0 = ACC_DMA_DESC_TYPE;
		desc->req.word1 = 0;
		desc->req.word2 = 0;
		desc->req.word3 = 0;
		desc->req.numCBs = 1;
		desc->req.m2dlen = 2;
		desc->req.d2mlen = 1;
		/* Enqueue the code block for processing */
		union acc_enqueue_reg_fmt enq_req;
		enq_req.val = 0;
		enq_req.addr_offset = ACC_DESC_OFFSET;
		enq_req.num_elem = 1;
		enq_req.req_elem_addr = 0;
		rte_wmb();
		acc_reg_write(d, HWPfQmgrIngressAq + 0x100, enq_req.val);
		usleep(ACC_LONG_WAIT * 100);
		if (desc->req.word0 != 2)
			rte_bbdev_log(WARNING, "DMA Response %#"PRIx32"\n", desc->req.word0);
	}

	/* Reset LDPC Cores */
	for (i = 0; i < ACC100_ENGINES_MAX; i++)
		acc_reg_write(d, HWPfFecUl5gCntrlReg +
				ACC_ENGINE_OFFSET * i,
				ACC100_RESET_HI);
	usleep(ACC_LONG_WAIT);
	for (i = 0; i < ACC100_ENGINES_MAX; i++)
		acc_reg_write(d, HWPfFecUl5gCntrlReg +
				ACC_ENGINE_OFFSET * i,
				ACC100_RESET_LO);
	usleep(ACC_LONG_WAIT);
	acc_reg_write(d, HWPfHi5GHardResetReg, ACC100_RESET_HARD);
	usleep(ACC_LONG_WAIT);
	int numEngines = 0;
	/* Check engine power-on status again */
	for (template_idx = ACC100_SIG_UL_5G;
			template_idx <= ACC100_SIG_UL_5G_LAST;
			template_idx++) {
		address = HwPfFecUl5gIbDebugReg +
				ACC_ENGINE_OFFSET * template_idx;
		status = (acc_reg_read(d, address) >> 4) & 0xF;
		address = HWPfQmgrGrpTmplateReg4Indx
				+ ACC_BYTES_IN_WORD * template_idx;
		if (status == 1) {
			acc_reg_write(d, address, value);
			numEngines++;
		} else
			acc_reg_write(d, address, 0);
	}
	rte_bbdev_log(INFO, "Number of 5GUL engines %d", numEngines);

	rte_free(d->sw_rings_base);
	usleep(ACC_LONG_WAIT);
}

/* Initial configuration of a ACC100 device prior to running configure() */
static int
acc100_configure(const char *dev_name, struct rte_acc_conf *conf)
{
	rte_bbdev_log(INFO, "rte_acc100_configure");
	uint32_t value, address, status;
	int qg_idx, template_idx, vf_idx, acc, i, j;
	struct rte_bbdev *bbdev = rte_bbdev_get_named_dev(dev_name);

	/* Compile time checks */
	RTE_BUILD_BUG_ON(sizeof(struct acc_dma_req_desc) != 256);
	RTE_BUILD_BUG_ON(sizeof(union acc_dma_desc) != 256);
	RTE_BUILD_BUG_ON(sizeof(struct acc_fcw_td) != 24);
	RTE_BUILD_BUG_ON(sizeof(struct acc_fcw_te) != 32);

	if (bbdev == NULL) {
		rte_bbdev_log(ERR,
		"Invalid dev_name (%s), or device is not yet initialised",
		dev_name);
		return -ENODEV;
	}
	struct acc_device *d = bbdev->data->dev_private;

	/* Store configuration */
	rte_memcpy(&d->acc_conf, conf, sizeof(d->acc_conf));

	value = acc_reg_read(d, HwPfPcieGpexBridgeControl);
	bool firstCfg = (value != ACC100_CFG_PCI_BRIDGE);

	/* PCIe Bridge configuration */
	acc_reg_write(d, HwPfPcieGpexBridgeControl, ACC100_CFG_PCI_BRIDGE);
	for (i = 1; i < ACC100_GPEX_AXIMAP_NUM; i++)
		acc_reg_write(d,
				HwPfPcieGpexAxiAddrMappingWindowPexBaseHigh
				+ i * 16, 0);

	/* Prevent blocking AXI read on BRESP for AXI Write */
	address = HwPfPcieGpexAxiPioControl;
	value = ACC100_CFG_PCI_AXI;
	acc_reg_write(d, address, value);

	/* 5GDL PLL phase shift */
	acc_reg_write(d, HWPfChaDl5gPllPhshft0, 0x1);

	/* Explicitly releasing AXI as this may be stopped after PF FLR/BME */
	address = HWPfDmaAxiControl;
	value = 1;
	acc_reg_write(d, address, value);

	/* Enable granular dynamic clock gating */
	address = HWPfHiClkGateHystReg;
	value = ACC100_CLOCK_GATING_EN;
	acc_reg_write(d, address, value);

	/* Set default descriptor signature */
	address = HWPfDmaDescriptorSignatuture;
	value = 0;
	acc_reg_write(d, address, value);

	/* Enable the Error Detection in DMA */
	value = ACC100_CFG_DMA_ERROR;
	address = HWPfDmaErrorDetectionEn;
	acc_reg_write(d, address, value);

	/* AXI Cache configuration */
	value = ACC100_CFG_AXI_CACHE;
	address = HWPfDmaAxcacheReg;
	acc_reg_write(d, address, value);

	/* Adjust PCIe Lane adaptation */
	for (i = 0; i < ACC100_QUAD_NUMS; i++)
		for (j = 0; j < ACC100_LANES_PER_QUAD; j++)
			acc_reg_write(d, HwPfPcieLnAdaptctrl + i * ACC100_PCIE_QUAD_OFFSET
					+ j * ACC100_PCIE_LANE_OFFSET, ACC100_ADAPT);

	/* Enable PCIe live adaptation */
	for (i = 0; i < ACC100_QUAD_NUMS; i++)
		acc_reg_write(d, HwPfPciePcsEqControl +
				i * ACC100_PCIE_QUAD_OFFSET, ACC100_PCS_EQ);

	/* Default DMA Configuration (Qmgr Enabled) */
	address = HWPfDmaConfig0Reg;
	value = 0;
	acc_reg_write(d, address, value);
	address = HWPfDmaQmanen;
	value = 0;
	acc_reg_write(d, address, value);

	/* Default RLIM/ALEN configuration */
	address = HWPfDmaConfig1Reg;
	value = (1 << 31) + (23 << 8) + (1 << 6) + 7;
	acc_reg_write(d, address, value);

	/* Configure DMA Qmanager addresses */
	address = HWPfDmaQmgrAddrReg;
	value = HWPfQmgrEgressQueuesTemplate;
	acc_reg_write(d, address, value);

	/* Default Fabric Mode */
	address = HWPfFabricMode;
	value = ACC100_FABRIC_MODE;
	acc_reg_write(d, address, value);

	/* ===== Qmgr Configuration ===== */
	/* Configuration of the AQueue Depth QMGR_GRP_0_DEPTH_LOG2 for UL */
	int totalQgs = conf->q_ul_4g.num_qgroups +
			conf->q_ul_5g.num_qgroups +
			conf->q_dl_4g.num_qgroups +
			conf->q_dl_5g.num_qgroups;
	for (qg_idx = 0; qg_idx < totalQgs; qg_idx++) {
		address = HWPfQmgrDepthLog2Grp +
		ACC_BYTES_IN_WORD * qg_idx;
		value = aqDepth(qg_idx, conf);
		acc_reg_write(d, address, value);
		address = HWPfQmgrTholdGrp +
		ACC_BYTES_IN_WORD * qg_idx;
		value = (1 << 16) + (1 << (aqDepth(qg_idx, conf) - 1));
		acc_reg_write(d, address, value);
	}

	/* Template Priority in incremental order */
	for (template_idx = 0; template_idx < ACC_NUM_TMPL; template_idx++) {
		address = HWPfQmgrGrpTmplateReg0Indx + ACC_BYTES_IN_WORD * template_idx;
		value = ACC_TMPL_PRI_0;
		acc_reg_write(d, address, value);
		address = HWPfQmgrGrpTmplateReg1Indx + ACC_BYTES_IN_WORD * template_idx;
		value = ACC_TMPL_PRI_1;
		acc_reg_write(d, address, value);
		address = HWPfQmgrGrpTmplateReg2indx + ACC_BYTES_IN_WORD * template_idx;
		value = ACC_TMPL_PRI_2;
		acc_reg_write(d, address, value);
		address = HWPfQmgrGrpTmplateReg3Indx + ACC_BYTES_IN_WORD * template_idx;
		value = ACC_TMPL_PRI_3;
		acc_reg_write(d, address, value);
	}

	address = HWPfQmgrGrpPriority;
	value = ACC100_CFG_QMGR_HI_P;
	acc_reg_write(d, address, value);

	/* Template Configuration */
	for (template_idx = 0; template_idx < ACC_NUM_TMPL;
			template_idx++) {
		value = 0;
		address = HWPfQmgrGrpTmplateReg4Indx
				+ ACC_BYTES_IN_WORD * template_idx;
		acc_reg_write(d, address, value);
	}
	/* 4GUL */
	int numQgs = conf->q_ul_4g.num_qgroups;
	int numQqsAcc = 0;
	value = 0;
	for (qg_idx = numQqsAcc; qg_idx < (numQgs + numQqsAcc); qg_idx++)
		value |= (1 << qg_idx);
	for (template_idx = ACC100_SIG_UL_4G;
			template_idx <= ACC100_SIG_UL_4G_LAST;
			template_idx++) {
		address = HWPfQmgrGrpTmplateReg4Indx
				+ ACC_BYTES_IN_WORD * template_idx;
		acc_reg_write(d, address, value);
	}
	/* 5GUL */
	numQqsAcc += numQgs;
	numQgs	= conf->q_ul_5g.num_qgroups;
	value = 0;
	int numEngines = 0;
	for (qg_idx = numQqsAcc; qg_idx < (numQgs + numQqsAcc); qg_idx++)
		value |= (1 << qg_idx);
	for (template_idx = ACC100_SIG_UL_5G;
			template_idx <= ACC100_SIG_UL_5G_LAST;
			template_idx++) {
		/* Check engine power-on status */
		address = HwPfFecUl5gIbDebugReg +
				ACC_ENGINE_OFFSET * template_idx;
		status = (acc_reg_read(d, address) >> 4) & 0xF;
		address = HWPfQmgrGrpTmplateReg4Indx
				+ ACC_BYTES_IN_WORD * template_idx;
		if (status == 1) {
			acc_reg_write(d, address, value);
			numEngines++;
		} else
			acc_reg_write(d, address, 0);
	}
	rte_bbdev_log(INFO, "Number of 5GUL engines %d", numEngines);
	/* 4GDL */
	numQqsAcc += numQgs;
	numQgs	= conf->q_dl_4g.num_qgroups;
	value = 0;
	for (qg_idx = numQqsAcc; qg_idx < (numQgs + numQqsAcc); qg_idx++)
		value |= (1 << qg_idx);
	for (template_idx = ACC100_SIG_DL_4G;
			template_idx <= ACC100_SIG_DL_4G_LAST;
			template_idx++) {
		address = HWPfQmgrGrpTmplateReg4Indx
				+ ACC_BYTES_IN_WORD * template_idx;
		acc_reg_write(d, address, value);
	}
	/* 5GDL */
	numQqsAcc += numQgs;
	numQgs	= conf->q_dl_5g.num_qgroups;
	value = 0;
	for (qg_idx = numQqsAcc; qg_idx < (numQgs + numQqsAcc); qg_idx++)
		value |= (1 << qg_idx);
	for (template_idx = ACC100_SIG_DL_5G;
			template_idx <= ACC100_SIG_DL_5G_LAST;
			template_idx++) {
		address = HWPfQmgrGrpTmplateReg4Indx
				+ ACC_BYTES_IN_WORD * template_idx;
		acc_reg_write(d, address, value);
	}

	/* Queue Group Function mapping */
	int qman_func_id[8] = {0, 2, 1, 3, 4, 0, 0, 0};
	address = HWPfQmgrGrpFunction0;
	value = 0;
	for (qg_idx = 0; qg_idx < 8; qg_idx++) {
		acc = accFromQgid(qg_idx, conf);
		value |= qman_func_id[acc]<<(qg_idx * 4);
	}
	acc_reg_write(d, address, value);

	/* Configuration of the Arbitration QGroup depth to 1 */
	for (qg_idx = 0; qg_idx < totalQgs; qg_idx++) {
		address = HWPfQmgrArbQDepthGrp +
		ACC_BYTES_IN_WORD * qg_idx;
		value = 0;
		acc_reg_write(d, address, value);
	}

	/* Enabling AQueues through the Queue hierarchy*/
	for (vf_idx = 0; vf_idx < ACC100_NUM_VFS; vf_idx++) {
		for (qg_idx = 0; qg_idx < ACC100_NUM_QGRPS; qg_idx++) {
			value = 0;
			if (vf_idx < conf->num_vf_bundles &&
					qg_idx < totalQgs)
				value = (1 << aqNum(qg_idx, conf)) - 1;
			address = HWPfQmgrAqEnableVf
					+ vf_idx * ACC_BYTES_IN_WORD;
			value += (qg_idx << 16);
			acc_reg_write(d, address, value);
		}
	}

	/* This pointer to ARAM (128kB) is shifted by 2 (4B per register) */
	uint32_t aram_address = 0;
	for (qg_idx = 0; qg_idx < totalQgs; qg_idx++) {
		for (vf_idx = 0; vf_idx < conf->num_vf_bundles; vf_idx++) {
			address = HWPfQmgrVfBaseAddr + vf_idx
					* ACC_BYTES_IN_WORD + qg_idx
					* ACC_BYTES_IN_WORD * 64;
			value = aram_address;
			acc_reg_write(d, address, value);
			/* Offset ARAM Address for next memory bank
			 * - increment of 4B
			 */
			aram_address += aqNum(qg_idx, conf) *
					(1 << aqDepth(qg_idx, conf));
		}
	}

	if (aram_address > ACC100_WORDS_IN_ARAM_SIZE) {
		rte_bbdev_log(ERR, "ARAM Configuration not fitting %d %d\n",
				aram_address, ACC100_WORDS_IN_ARAM_SIZE);
		return -EINVAL;
	}

	/* ==== HI Configuration ==== */

	/* No Info Ring/MSI by default */
	acc_reg_write(d, HWPfHiInfoRingIntWrEnRegPf, 0);
	acc_reg_write(d, HWPfHiInfoRingVf2pfLoWrEnReg, 0);
	acc_reg_write(d, HWPfHiCfgMsiIntWrEnRegPf, 0xFFFFFFFF);
	acc_reg_write(d, HWPfHiCfgMsiVf2pfLoWrEnReg, 0xFFFFFFFF);
	/* Prevent Block on Transmit Error */
	address = HWPfHiBlockTransmitOnErrorEn;
	value = 0;
	acc_reg_write(d, address, value);
	/* Prevents to drop MSI */
	address = HWPfHiMsiDropEnableReg;
	value = 0;
	acc_reg_write(d, address, value);
	/* Set the PF Mode register */
	address = HWPfHiPfMode;
	value = (conf->pf_mode_en) ? ACC_PF_VAL : 0;
	acc_reg_write(d, address, value);

	/* QoS overflow init */
	value = 1;
	address = HWPfQosmonAEvalOverflow0;
	acc_reg_write(d, address, value);
	address = HWPfQosmonBEvalOverflow0;
	acc_reg_write(d, address, value);

	/* HARQ DDR Configuration */
	unsigned int ddrSizeInMb = ACC100_HARQ_DDR;
	for (vf_idx = 0; vf_idx < conf->num_vf_bundles; vf_idx++) {
		address = HWPfDmaVfDdrBaseRw + vf_idx
				* 0x10;
		value = ((vf_idx * (ddrSizeInMb / 64)) << 16) +
				(ddrSizeInMb - 1);
		acc_reg_write(d, address, value);
	}
	usleep(ACC_LONG_WAIT);

	/* Workaround in case some 5GUL engines are in an unexpected state */
	if (numEngines < (ACC100_SIG_UL_5G_LAST + 1))
		poweron_cleanup(bbdev, d, conf);

	uint32_t version = 0;
	for (i = 0; i < 4; i++)
		version += acc_reg_read(d,
				HWPfDdrPhyIdtmFwVersion + 4 * i) << (8 * i);
	if (version != ACC100_PRQ_DDR_VER) {
		rte_bbdev_log(ERR, "* Note: Not on DDR PRQ version %8x != %08x",
				version, ACC100_PRQ_DDR_VER);
	} else if (firstCfg) {
		/* ---- DDR configuration at boot up --- */
		/* Read Clear Ddr training status */
		acc_reg_read(d, HWPfChaDdrStDoneStatus);
		/* Reset PHY/IDTM/UMMC */
		acc_reg_write(d, HWPfChaDdrWbRstCfg, 3);
		acc_reg_write(d, HWPfChaDdrApbRstCfg, 2);
		acc_reg_write(d, HWPfChaDdrPhyRstCfg, 2);
		acc_reg_write(d, HWPfChaDdrCpuRstCfg, 3);
		acc_reg_write(d, HWPfChaDdrSifRstCfg, 2);
		usleep(ACC_MS_IN_US);
		/* Reset WB and APB resets */
		acc_reg_write(d, HWPfChaDdrWbRstCfg, 2);
		acc_reg_write(d, HWPfChaDdrApbRstCfg, 3);
		/* Configure PHY-IDTM */
		acc_reg_write(d, HWPfDdrPhyIdletimeout, 0x3e8);
		/* IDTM timing registers */
		acc_reg_write(d, HWPfDdrPhyRdLatency, 0x13);
		acc_reg_write(d, HWPfDdrPhyRdLatencyDbi, 0x15);
		acc_reg_write(d, HWPfDdrPhyWrLatency, 0x10011);
		/* Configure SDRAM MRS registers */
		acc_reg_write(d, HWPfDdrPhyMr01Dimm, 0x3030b70);
		acc_reg_write(d, HWPfDdrPhyMr01DimmDbi, 0x3030b50);
		acc_reg_write(d, HWPfDdrPhyMr23Dimm, 0x30);
		acc_reg_write(d, HWPfDdrPhyMr67Dimm, 0xc00);
		acc_reg_write(d, HWPfDdrPhyMr45Dimm, 0x4000000);
		/* Configure active lanes */
		acc_reg_write(d, HWPfDdrPhyDqsCountMax, 0x9);
		acc_reg_write(d, HWPfDdrPhyDqsCountNum, 0x9);
		/* Configure WR/RD leveling timing registers */
		acc_reg_write(d, HWPfDdrPhyWrlvlWwRdlvlRr, 0x101212);
		/* Configure what trainings to execute */
		acc_reg_write(d, HWPfDdrPhyTrngType, 0x2d3c);
		/* Releasing PHY reset */
		acc_reg_write(d, HWPfChaDdrPhyRstCfg, 3);
		/* Configure Memory Controller registers */
		acc_reg_write(d, HWPfDdrMemInitPhyTrng0, 0x3);
		acc_reg_write(d, HWPfDdrBcDram, 0x3c232003);
		acc_reg_write(d, HWPfDdrBcAddrMap, 0x31);
		/* Configure UMMC BC timing registers */
		acc_reg_write(d, HWPfDdrBcRef, 0xa22);
		acc_reg_write(d, HWPfDdrBcTim0, 0x4050501);
		acc_reg_write(d, HWPfDdrBcTim1, 0xf0b0476);
		acc_reg_write(d, HWPfDdrBcTim2, 0x103);
		acc_reg_write(d, HWPfDdrBcTim3, 0x144050a1);
		acc_reg_write(d, HWPfDdrBcTim4, 0x23300);
		acc_reg_write(d, HWPfDdrBcTim5, 0x4230276);
		acc_reg_write(d, HWPfDdrBcTim6, 0x857914);
		acc_reg_write(d, HWPfDdrBcTim7, 0x79100232);
		acc_reg_write(d, HWPfDdrBcTim8, 0x100007ce);
		acc_reg_write(d, HWPfDdrBcTim9, 0x50020);
		acc_reg_write(d, HWPfDdrBcTim10, 0x40ee);
		/* Configure UMMC DFI timing registers */
		acc_reg_write(d, HWPfDdrDfiInit, 0x5000);
		acc_reg_write(d, HWPfDdrDfiTim0, 0x15030006);
		acc_reg_write(d, HWPfDdrDfiTim1, 0x11305);
		acc_reg_write(d, HWPfDdrDfiPhyUpdEn, 0x1);
		acc_reg_write(d, HWPfDdrUmmcIntEn, 0x1f);
		/* Release IDTM CPU out of reset */
		acc_reg_write(d, HWPfChaDdrCpuRstCfg, 0x2);
		/* Wait PHY-IDTM to finish static training */
		for (i = 0; i < ACC100_DDR_TRAINING_MAX; i++) {
			usleep(ACC_MS_IN_US);
			value = acc_reg_read(d,
					HWPfChaDdrStDoneStatus);
			if (value & 1)
				break;
		}
		rte_bbdev_log(INFO, "DDR Training completed in %d ms", i);
		/* Enable Memory Controller */
		acc_reg_write(d, HWPfDdrUmmcCtrl, 0x401);
		/* Release AXI interface reset */
		acc_reg_write(d, HWPfChaDdrSifRstCfg, 3);
	}

	rte_bbdev_log_debug("PF Tip configuration complete for %s", dev_name);
	return 0;
}


/* Initial configuration of a ACC101 device prior to running configure() */
static int
acc101_configure(const char *dev_name, struct rte_acc_conf *conf)
{
	rte_bbdev_log(INFO, "rte_acc101_configure");
	uint32_t value, address, status;
	int qg_idx, template_idx, vf_idx, acc, i;
	struct rte_bbdev *bbdev = rte_bbdev_get_named_dev(dev_name);

	/* Compile time checks */
	RTE_BUILD_BUG_ON(sizeof(struct acc_dma_req_desc) != 256);
	RTE_BUILD_BUG_ON(sizeof(union acc_dma_desc) != 256);
	RTE_BUILD_BUG_ON(sizeof(struct acc_fcw_td) != 24);
	RTE_BUILD_BUG_ON(sizeof(struct acc_fcw_te) != 32);

	if (bbdev == NULL) {
		rte_bbdev_log(ERR,
		"Invalid dev_name (%s), or device is not yet initialised",
		dev_name);
		return -ENODEV;
	}
	struct acc_device *d = bbdev->data->dev_private;

	/* Store configuration */
	rte_memcpy(&d->acc_conf, conf, sizeof(d->acc_conf));

	/* PCIe Bridge configuration */
	acc_reg_write(d, HwPfPcieGpexBridgeControl, ACC101_CFG_PCI_BRIDGE);
	for (i = 1; i < ACC101_GPEX_AXIMAP_NUM; i++)
		acc_reg_write(d, HwPfPcieGpexAxiAddrMappingWindowPexBaseHigh + i * 16, 0);

	/* Prevent blocking AXI read on BRESP for AXI Write */
	address = HwPfPcieGpexAxiPioControl;
	value = ACC101_CFG_PCI_AXI;
	acc_reg_write(d, address, value);

	/* Explicitly releasing AXI including a 2ms delay on ACC101 */
	usleep(2000);
	acc_reg_write(d, HWPfDmaAxiControl, 1);

	/* Set the default 5GDL DMA configuration */
	acc_reg_write(d, HWPfDmaInboundDrainDataSize, ACC101_DMA_INBOUND);

	/* Enable granular dynamic clock gating */
	address = HWPfHiClkGateHystReg;
	value = ACC101_CLOCK_GATING_EN;
	acc_reg_write(d, address, value);

	/* Set default descriptor signature */
	address = HWPfDmaDescriptorSignatuture;
	value = 0;
	acc_reg_write(d, address, value);

	/* Enable the Error Detection in DMA */
	value = ACC101_CFG_DMA_ERROR;
	address = HWPfDmaErrorDetectionEn;
	acc_reg_write(d, address, value);

	/* AXI Cache configuration */
	value = ACC101_CFG_AXI_CACHE;
	address = HWPfDmaAxcacheReg;
	acc_reg_write(d, address, value);

	/* Default DMA Configuration (Qmgr Enabled) */
	address = HWPfDmaConfig0Reg;
	value = 0;
	acc_reg_write(d, address, value);
	address = HWPfDmaQmanen;
	value = 0;
	acc_reg_write(d, address, value);

	/* Default RLIM/ALEN configuration */
	address = HWPfDmaConfig1Reg;
	int alen_r = 0xF;
	int alen_w = 0x7;
	value = (1 << 31) + (alen_w << 20)  + (1 << 6) + alen_r;
	acc_reg_write(d, address, value);

	/* Configure DMA Qmanager addresses */
	address = HWPfDmaQmgrAddrReg;
	value = HWPfQmgrEgressQueuesTemplate;
	acc_reg_write(d, address, value);

	/* ===== Qmgr Configuration ===== */
	/* Configuration of the AQueue Depth QMGR_GRP_0_DEPTH_LOG2 for UL */
	int totalQgs = conf->q_ul_4g.num_qgroups +
			conf->q_ul_5g.num_qgroups +
			conf->q_dl_4g.num_qgroups +
			conf->q_dl_5g.num_qgroups;
	for (qg_idx = 0; qg_idx < totalQgs; qg_idx++) {
		address = HWPfQmgrDepthLog2Grp +
		ACC_BYTES_IN_WORD * qg_idx;
		value = aqDepth(qg_idx, conf);
		acc_reg_write(d, address, value);
		address = HWPfQmgrTholdGrp +
		ACC_BYTES_IN_WORD * qg_idx;
		value = (1 << 16) + (1 << (aqDepth(qg_idx, conf) - 1));
		acc_reg_write(d, address, value);
	}

	/* Template Priority in incremental order */
	for (template_idx = 0; template_idx < ACC_NUM_TMPL;
			template_idx++) {
		address = HWPfQmgrGrpTmplateReg0Indx + ACC_BYTES_IN_WORD * template_idx;
		value = ACC_TMPL_PRI_0;
		acc_reg_write(d, address, value);
		address = HWPfQmgrGrpTmplateReg1Indx + ACC_BYTES_IN_WORD * template_idx;
		value = ACC_TMPL_PRI_1;
		acc_reg_write(d, address, value);
		address = HWPfQmgrGrpTmplateReg2indx + ACC_BYTES_IN_WORD * template_idx;
		value = ACC_TMPL_PRI_2;
		acc_reg_write(d, address, value);
		address = HWPfQmgrGrpTmplateReg3Indx + ACC_BYTES_IN_WORD * template_idx;
		value = ACC_TMPL_PRI_3;
		acc_reg_write(d, address, value);
	}

	address = HWPfQmgrGrpPriority;
	value = ACC101_CFG_QMGR_HI_P;
	acc_reg_write(d, address, value);

	/* Template Configuration */
	for (template_idx = 0; template_idx < ACC_NUM_TMPL;
			template_idx++) {
		value = 0;
		address = HWPfQmgrGrpTmplateReg4Indx
				+ ACC_BYTES_IN_WORD * template_idx;
		acc_reg_write(d, address, value);
	}
	/* 4GUL */
	int numQgs = conf->q_ul_4g.num_qgroups;
	int numQqsAcc = 0;
	value = 0;
	for (qg_idx = numQqsAcc; qg_idx < (numQgs + numQqsAcc); qg_idx++)
		value |= (1 << qg_idx);
	for (template_idx = ACC101_SIG_UL_4G;
			template_idx <= ACC101_SIG_UL_4G_LAST;
			template_idx++) {
		address = HWPfQmgrGrpTmplateReg4Indx
				+ ACC_BYTES_IN_WORD * template_idx;
		acc_reg_write(d, address, value);
	}
	/* 5GUL */
	numQqsAcc += numQgs;
	numQgs	= conf->q_ul_5g.num_qgroups;
	value = 0;
	int numEngines = 0;
	for (qg_idx = numQqsAcc; qg_idx < (numQgs + numQqsAcc); qg_idx++)
		value |= (1 << qg_idx);
	for (template_idx = ACC101_SIG_UL_5G;
			template_idx <= ACC101_SIG_UL_5G_LAST;
			template_idx++) {
		/* Check engine power-on status */
		address = HwPfFecUl5gIbDebugReg +
				ACC_ENGINE_OFFSET * template_idx;
		status = (acc_reg_read(d, address) >> 4) & 0xF;
		address = HWPfQmgrGrpTmplateReg4Indx
				+ ACC_BYTES_IN_WORD * template_idx;
		if (status == 1) {
			acc_reg_write(d, address, value);
			numEngines++;
		} else
			acc_reg_write(d, address, 0);
	}
	rte_bbdev_log(INFO, "Number of 5GUL engines %d", numEngines);
	/* 4GDL */
	numQqsAcc += numQgs;
	numQgs	= conf->q_dl_4g.num_qgroups;
	value = 0;
	for (qg_idx = numQqsAcc; qg_idx < (numQgs + numQqsAcc); qg_idx++)
		value |= (1 << qg_idx);
	for (template_idx = ACC101_SIG_DL_4G;
			template_idx <= ACC101_SIG_DL_4G_LAST;
			template_idx++) {
		address = HWPfQmgrGrpTmplateReg4Indx
				+ ACC_BYTES_IN_WORD * template_idx;
		acc_reg_write(d, address, value);
	}
	/* 5GDL */
	numQqsAcc += numQgs;
	numQgs	= conf->q_dl_5g.num_qgroups;
	value = 0;
	for (qg_idx = numQqsAcc; qg_idx < (numQgs + numQqsAcc); qg_idx++)
		value |= (1 << qg_idx);
	for (template_idx = ACC101_SIG_DL_5G;
			template_idx <= ACC101_SIG_DL_5G_LAST;
			template_idx++) {
		address = HWPfQmgrGrpTmplateReg4Indx
				+ ACC_BYTES_IN_WORD * template_idx;
		acc_reg_write(d, address, value);
	}

	/* Queue Group Function mapping */
	int qman_func_id[8] = {0, 2, 1, 3, 4, 0, 0, 0};
	address = HWPfQmgrGrpFunction0;
	value = 0;
	for (qg_idx = 0; qg_idx < 8; qg_idx++) {
		acc = accFromQgid(qg_idx, conf);
		value |= qman_func_id[acc]<<(qg_idx * 4);
	}
	acc_reg_write(d, address, value);

	/* Configuration of the Arbitration QGroup depth to 1 */
	for (qg_idx = 0; qg_idx < totalQgs; qg_idx++) {
		address = HWPfQmgrArbQDepthGrp +
		ACC_BYTES_IN_WORD * qg_idx;
		value = 0;
		acc_reg_write(d, address, value);
	}

	/* Enabling AQueues through the Queue hierarchy*/
	for (vf_idx = 0; vf_idx < ACC101_NUM_VFS; vf_idx++) {
		for (qg_idx = 0; qg_idx < ACC101_NUM_QGRPS; qg_idx++) {
			value = 0;
			if (vf_idx < conf->num_vf_bundles &&
					qg_idx < totalQgs)
				value = (1 << aqNum(qg_idx, conf)) - 1;
			address = HWPfQmgrAqEnableVf
					+ vf_idx * ACC_BYTES_IN_WORD;
			value += (qg_idx << 16);
			acc_reg_write(d, address, value);
		}
	}

	/* This pointer to ARAM (128kB) is shifted by 2 (4B per register) */
	uint32_t aram_address = 0;
	for (qg_idx = 0; qg_idx < totalQgs; qg_idx++) {
		for (vf_idx = 0; vf_idx < conf->num_vf_bundles; vf_idx++) {
			address = HWPfQmgrVfBaseAddr + vf_idx
					* ACC_BYTES_IN_WORD + qg_idx
					* ACC_BYTES_IN_WORD * 64;
			value = aram_address;
			acc_reg_write(d, address, value);
			/* Offset ARAM Address for next memory bank
			 * - increment of 4B
			 */
			aram_address += aqNum(qg_idx, conf) *
					(1 << aqDepth(qg_idx, conf));
		}
	}

	if (aram_address > ACC101_WORDS_IN_ARAM_SIZE) {
		rte_bbdev_log(ERR, "ARAM Configuration not fitting %d %d\n",
				aram_address, ACC101_WORDS_IN_ARAM_SIZE);
		return -EINVAL;
	}

	/* ==== HI Configuration ==== */

	/* No Info Ring/MSI by default */
	acc_reg_write(d, HWPfHiInfoRingIntWrEnRegPf, 0);
	acc_reg_write(d, HWPfHiInfoRingVf2pfLoWrEnReg, 0);
	acc_reg_write(d, HWPfHiCfgMsiIntWrEnRegPf, 0xFFFFFFFF);
	acc_reg_write(d, HWPfHiCfgMsiVf2pfLoWrEnReg, 0xFFFFFFFF);
	/* Prevent Block on Transmit Error */
	address = HWPfHiBlockTransmitOnErrorEn;
	value = 0;
	acc_reg_write(d, address, value);
	/* Prevents to drop MSI */
	address = HWPfHiMsiDropEnableReg;
	value = 0;
	acc_reg_write(d, address, value);
	/* Set the PF Mode register */
	address = HWPfHiPfMode;
	value = (conf->pf_mode_en) ? ACC_PF_VAL : 0;
	acc_reg_write(d, address, value);
	/* Explicitly releasing AXI after PF Mode and 2 ms */
	usleep(2000);
	acc_reg_write(d, HWPfDmaAxiControl, 1);

	/* QoS overflow init */
	value = 1;
	address = HWPfQosmonAEvalOverflow0;
	acc_reg_write(d, address, value);
	address = HWPfQosmonBEvalOverflow0;
	acc_reg_write(d, address, value);

	/* HARQ DDR Configuration */
	unsigned int ddrSizeInMb = ACC101_HARQ_DDR;
	for (vf_idx = 0; vf_idx < conf->num_vf_bundles; vf_idx++) {
		address = HWPfDmaVfDdrBaseRw + vf_idx
				* 0x10;
		value = ((vf_idx * (ddrSizeInMb / 64)) << 16) +
				(ddrSizeInMb - 1);
		acc_reg_write(d, address, value);
	}
	usleep(ACC_LONG_WAIT);

	rte_bbdev_log_debug("PF TIP configuration complete for %s", dev_name);
	return 0;
}

int
rte_acc_configure(const char *dev_name, struct rte_acc_conf *conf)
{
	struct rte_bbdev *bbdev = rte_bbdev_get_named_dev(dev_name);
	if (bbdev == NULL) {
		rte_bbdev_log(ERR, "Invalid dev_name (%s), or device is not yet initialised",
				dev_name);
		return -ENODEV;
	}
	struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI(bbdev->device);
	rte_bbdev_log(INFO, "Configure dev id %x", pci_dev->id.device_id);
	if (pci_dev->id.device_id == ACC100_PF_DEVICE_ID)
		return acc100_configure(dev_name, conf);
	else if (pci_dev->id.device_id == ACC101_PF_DEVICE_ID)
		return acc101_configure(dev_name, conf);
	else
		return vrb1_configure(dev_name, conf);
}
