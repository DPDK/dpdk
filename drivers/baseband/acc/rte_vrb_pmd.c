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
#include <rte_cycles.h>

#include <rte_bbdev.h>
#include <rte_bbdev_pmd.h>
#include "vrb_pmd.h"

#ifdef RTE_LIBRTE_BBDEV_DEBUG
RTE_LOG_REGISTER_DEFAULT(vrb_logtype, DEBUG);
#else
RTE_LOG_REGISTER_DEFAULT(vrb_logtype, NOTICE);
#endif

/* Calculate the offset of the enqueue register. */
static inline uint32_t
vrb1_queue_offset(bool pf_device, uint8_t vf_id, uint8_t qgrp_id, uint16_t aq_id)
{
	if (pf_device)
		return ((vf_id << 12) + (qgrp_id << 7) + (aq_id << 3) + VRB1_PfQmgrIngressAq);
	else
		return ((qgrp_id << 7) + (aq_id << 3) + VRB1_VfQmgrIngressAq);
}

enum {UL_4G = 0, UL_5G, DL_4G, DL_5G, FFT, NUM_ACC};

/* Return the accelerator enum for a Queue Group Index. */
static inline int
accFromQgid(int qg_idx, const struct rte_acc_conf *acc_conf)
{
	int accQg[VRB_MAX_QGRPS];
	int NumQGroupsPerFn[NUM_ACC];
	int acc, qgIdx, qgIndex = 0;
	for (qgIdx = 0; qgIdx < VRB_MAX_QGRPS; qgIdx++)
		accQg[qgIdx] = 0;
	NumQGroupsPerFn[UL_4G] = acc_conf->q_ul_4g.num_qgroups;
	NumQGroupsPerFn[UL_5G] = acc_conf->q_ul_5g.num_qgroups;
	NumQGroupsPerFn[DL_4G] = acc_conf->q_dl_4g.num_qgroups;
	NumQGroupsPerFn[DL_5G] = acc_conf->q_dl_5g.num_qgroups;
	NumQGroupsPerFn[FFT] = acc_conf->q_fft.num_qgroups;
	for (acc = UL_4G;  acc < NUM_ACC; acc++)
		for (qgIdx = 0; qgIdx < NumQGroupsPerFn[acc]; qgIdx++)
			accQg[qgIndex++] = acc;
	acc = accQg[qg_idx];
	return acc;
}

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

/* Return the AQ depth for a Queue Group Index. */
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

/* Return the AQ depth for a Queue Group Index. */
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
		reg = acc_reg_read(d, d->queue_offset(d->pf_device, 0, qg, d->num_aqs - 1));
		if (reg & ACC_QUEUE_ENABLE) {
			q_top->num_aqs_per_groups = d->num_aqs;
			return;
		}
		q_top->num_aqs_per_groups = 0;
		for (aq = 0; aq < d->num_aqs; aq++) {
			reg = acc_reg_read(d, d->queue_offset(d->pf_device, 0, qg, aq));
			if (reg & ACC_QUEUE_ENABLE)
				q_top->num_aqs_per_groups++;
		}
	}
}

/* Check device Qmgr is enabled for protection */
static inline bool
vrb_check_device_enable(struct rte_bbdev *dev)
{
	uint32_t reg_aq, qg;
	struct acc_device *d = dev->data->dev_private;

	for (qg = 0; qg < d->num_qgroups; qg++) {
		reg_aq = acc_reg_read(d, d->queue_offset(d->pf_device, 0, qg, 0));
		if (reg_aq & ACC_QUEUE_ENABLE)
			return true;
	}
	return false;
}

/* Fetch configuration enabled for the PF/VF using MMIO Read (slow). */
static inline void
fetch_acc_config(struct rte_bbdev *dev)
{
	struct acc_device *d = dev->data->dev_private;
	struct rte_acc_conf *acc_conf = &d->acc_conf;
	uint8_t acc, qg;
	uint32_t reg_aq, reg_len0, reg_len1, reg0, reg1;
	uint32_t reg_mode, idx;
	struct rte_acc_queue_topology *q_top = NULL;
	int qman_func_id[VRB_NUM_ACCS] = {ACC_ACCMAP_0, ACC_ACCMAP_1,
			ACC_ACCMAP_2, ACC_ACCMAP_3, ACC_ACCMAP_4, ACC_ACCMAP_5};

	/* No need to retrieve the configuration is already done. */
	if (d->configured)
		return;

	if (!vrb_check_device_enable(dev)) {
		rte_bbdev_log(NOTICE, "%s has no queue enabled and can't be used.",
				dev->data->name);
		return;
	}

	d->ddr_size = 0;

	/* Single VF Bundle by VF. */
	acc_conf->num_vf_bundles = 1;
	initQTop(acc_conf);

	reg0 = acc_reg_read(d, d->reg_addr->qman_group_func);
	reg1 = acc_reg_read(d, d->reg_addr->qman_group_func + 4);
	for (qg = 0; qg < d->num_qgroups; qg++) {
		reg_aq = acc_reg_read(d, d->queue_offset(d->pf_device, 0, qg, 0));
		if (reg_aq & ACC_QUEUE_ENABLE) {
			if (qg < ACC_NUM_QGRPS_PER_WORD)
				idx = (reg0 >> (qg * 4)) & 0x7;
			else
				idx = (reg1 >> ((qg - ACC_NUM_QGRPS_PER_WORD) * 4)) & 0x7;
			if (idx < VRB1_NUM_ACCS) {
				acc = qman_func_id[idx];
				updateQtop(acc, qg, acc_conf, d);
			}
		}
	}

	/* Check the depth of the AQs. */
	reg_len0 = acc_reg_read(d, d->reg_addr->depth_log0_offset);
	reg_len1 = acc_reg_read(d, d->reg_addr->depth_log1_offset);
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
		reg_mode = acc_reg_read(d, d->reg_addr->pf_mode);
		acc_conf->pf_mode_en = (reg_mode == ACC_PF_VAL) ? 1 : 0;
	} else {
		reg_mode = acc_reg_read(d, d->reg_addr->hi_mode);
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

static inline void
vrb_vf2pf(struct acc_device *d, unsigned int payload)
{
	acc_reg_write(d, d->reg_addr->vf2pf_doorbell, payload);
}

/* Request device status information. */
static inline uint32_t
vrb_device_status(struct rte_bbdev *dev)
{
	struct acc_device *d = dev->data->dev_private;
	uint32_t reg, time_out = 0;

	if (d->pf_device)
		return RTE_BBDEV_DEV_NOT_SUPPORTED;

	vrb_vf2pf(d, ACC_VF2PF_STATUS_REQUEST);
	reg = acc_reg_read(d, d->reg_addr->pf2vf_doorbell);
	while ((time_out < ACC_STATUS_TO) && (reg == RTE_BBDEV_DEV_NOSTATUS)) {
		usleep(ACC_STATUS_WAIT); /*< Wait or VF->PF->VF Comms */
		reg = acc_reg_read(d, d->reg_addr->pf2vf_doorbell);
		time_out++;
	}

	return reg;
}

/* Checks PF Info Ring to find the interrupt cause and handles it accordingly. */
static inline void
vrb_check_ir(struct acc_device *acc_dev)
{
	volatile union acc_info_ring_data *ring_data;
	uint16_t info_ring_head = acc_dev->info_ring_head;
	if (unlikely(acc_dev->info_ring == NULL))
		return;

	ring_data = acc_dev->info_ring + (acc_dev->info_ring_head & ACC_INFO_RING_MASK);

	while (ring_data->valid) {
		if ((ring_data->int_nb < ACC_PF_INT_DMA_DL_DESC_IRQ) || (
				ring_data->int_nb > ACC_PF_INT_DMA_DL5G_DESC_IRQ)) {
			rte_bbdev_log(WARNING, "InfoRing: ITR:%d Info:0x%x",
					ring_data->int_nb, ring_data->detailed_info);
			/* Initialize Info Ring entry and move forward. */
			ring_data->val = 0;
		}
		info_ring_head++;
		ring_data = acc_dev->info_ring + (info_ring_head & ACC_INFO_RING_MASK);
	}
}

/* Interrupt handler triggered by dev for handling specific interrupt. */
static void
vrb_dev_interrupt_handler(void *cb_arg)
{
	struct rte_bbdev *dev = cb_arg;
	struct acc_device *acc_dev = dev->data->dev_private;
	volatile union acc_info_ring_data *ring_data;
	struct acc_deq_intr_details deq_intr_det;

	ring_data = acc_dev->info_ring + (acc_dev->info_ring_head & ACC_INFO_RING_MASK);

	while (ring_data->valid) {
		if (acc_dev->pf_device) {
			rte_bbdev_log_debug(
					"VRB1 PF Interrupt received, Info Ring data: 0x%x -> %d",
					ring_data->val, ring_data->int_nb);

			switch (ring_data->int_nb) {
			case ACC_PF_INT_DMA_DL_DESC_IRQ:
			case ACC_PF_INT_DMA_UL_DESC_IRQ:
			case ACC_PF_INT_DMA_FFT_DESC_IRQ:
			case ACC_PF_INT_DMA_UL5G_DESC_IRQ:
			case ACC_PF_INT_DMA_DL5G_DESC_IRQ:
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
				rte_bbdev_pmd_callback_process(dev, RTE_BBDEV_EVENT_ERROR, NULL);
				break;
			}
		} else {
			rte_bbdev_log_debug(
					"VRB1 VF Interrupt received, Info Ring data: 0x%x\n",
					ring_data->val);
			switch (ring_data->int_nb) {
			case ACC_VF_INT_DMA_DL_DESC_IRQ:
			case ACC_VF_INT_DMA_UL_DESC_IRQ:
			case ACC_VF_INT_DMA_FFT_DESC_IRQ:
			case ACC_VF_INT_DMA_UL5G_DESC_IRQ:
			case ACC_VF_INT_DMA_DL5G_DESC_IRQ:
				/* VFs are not aware of their vf_id - it's set to 0.  */
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
				rte_bbdev_pmd_callback_process(dev, RTE_BBDEV_EVENT_ERROR, NULL);
				break;
			}
		}

		/* Initialize Info Ring entry and move forward. */
		ring_data->val = 0;
		++acc_dev->info_ring_head;
		ring_data = acc_dev->info_ring +
				(acc_dev->info_ring_head & ACC_INFO_RING_MASK);
	}
}

/* Allocate and setup inforing. */
static int
allocate_info_ring(struct rte_bbdev *dev)
{
	struct acc_device *d = dev->data->dev_private;
	rte_iova_t info_ring_iova;
	uint32_t phys_low, phys_high;

	if (d->info_ring != NULL)
		return 0; /* Already configured. */

	/* Allocate InfoRing */
	d->info_ring = rte_zmalloc_socket("Info Ring", ACC_INFO_RING_NUM_ENTRIES *
			sizeof(*d->info_ring), RTE_CACHE_LINE_SIZE, dev->data->socket_id);
	if (d->info_ring == NULL) {
		rte_bbdev_log(ERR,
				"Failed to allocate Info Ring for %s:%u",
				dev->device->driver->name,
				dev->data->dev_id);
		return -ENOMEM;
	}
	info_ring_iova = rte_malloc_virt2iova(d->info_ring);

	/* Setup Info Ring. */
	phys_high = (uint32_t)(info_ring_iova >> 32);
	phys_low  = (uint32_t)(info_ring_iova);
	acc_reg_write(d, d->reg_addr->info_ring_hi, phys_high);
	acc_reg_write(d, d->reg_addr->info_ring_lo, phys_low);
	acc_reg_write(d, d->reg_addr->info_ring_en, VRB1_REG_IRQ_EN_ALL);
	d->info_ring_head = (acc_reg_read(d, d->reg_addr->info_ring_ptr) &
			0xFFF) / sizeof(union acc_info_ring_data);
	return 0;
}


/* Allocate 64MB memory used for all software rings. */
static int
vrb_setup_queues(struct rte_bbdev *dev, uint16_t num_queues, int socket_id)
{
	uint32_t phys_low, phys_high, value;
	struct acc_device *d = dev->data->dev_private;
	int ret;

	if (d->pf_device && !d->acc_conf.pf_mode_en) {
		rte_bbdev_log(NOTICE,
				"%s has PF mode disabled. This PF can't be used.",
				dev->data->name);
		return -ENODEV;
	}
	if (!d->pf_device && d->acc_conf.pf_mode_en) {
		rte_bbdev_log(NOTICE,
				"%s has PF mode enabled. This VF can't be used.",
				dev->data->name);
		return -ENODEV;
	}

	if (!vrb_check_device_enable(dev)) {
		rte_bbdev_log(NOTICE, "%s has no queue enabled and can't be used.",
				dev->data->name);
		return -ENODEV;
	}

	alloc_sw_rings_min_mem(dev, d, num_queues, socket_id);

	/* If minimal memory space approach failed, then allocate
	 * the 2 * 64MB block for the sw rings.
	 */
	if (d->sw_rings == NULL)
		alloc_2x64mb_sw_rings_mem(dev, d, socket_id);

	if (d->sw_rings == NULL) {
		rte_bbdev_log(NOTICE,
				"Failure allocating sw_rings memory");
		return -ENOMEM;
	}

	/* Configure device with the base address for DMA descriptor rings.
	 * Same descriptor rings used for UL and DL DMA Engines.
	 * Note : Assuming only VF0 bundle is used for PF mode.
	 */
	phys_high = (uint32_t)(d->sw_rings_iova >> 32);
	phys_low  = (uint32_t)(d->sw_rings_iova & ~(ACC_SIZE_64MBYTE-1));

	/* Read the populated cfg from device registers. */
	fetch_acc_config(dev);

	/* Start Pmon */
	for (value = 0; value <= 2; value++) {
		acc_reg_write(d, d->reg_addr->pmon_ctrl_a, value);
		acc_reg_write(d, d->reg_addr->pmon_ctrl_b, value);
		acc_reg_write(d, d->reg_addr->pmon_ctrl_c, value);
	}

	/* Release AXI from PF. */
	if (d->pf_device)
		acc_reg_write(d, VRB1_PfDmaAxiControl, 1);

	acc_reg_write(d, d->reg_addr->dma_ring_ul5g_hi, phys_high);
	acc_reg_write(d, d->reg_addr->dma_ring_ul5g_lo, phys_low);
	acc_reg_write(d, d->reg_addr->dma_ring_dl5g_hi, phys_high);
	acc_reg_write(d, d->reg_addr->dma_ring_dl5g_lo, phys_low);
	acc_reg_write(d, d->reg_addr->dma_ring_ul4g_hi, phys_high);
	acc_reg_write(d, d->reg_addr->dma_ring_ul4g_lo, phys_low);
	acc_reg_write(d, d->reg_addr->dma_ring_dl4g_hi, phys_high);
	acc_reg_write(d, d->reg_addr->dma_ring_dl4g_lo, phys_low);
	acc_reg_write(d, d->reg_addr->dma_ring_fft_hi, phys_high);
	acc_reg_write(d, d->reg_addr->dma_ring_fft_lo, phys_low);
	/*
	 * Configure Ring Size to the max queue ring size
	 * (used for wrapping purpose).
	 */
	value = log2_basic(d->sw_ring_size / ACC_RING_SIZE_GRANULARITY);
	acc_reg_write(d, d->reg_addr->ring_size, value);

	/* Configure tail pointer for use when SDONE enabled. */
	if (d->tail_ptrs == NULL)
		d->tail_ptrs = rte_zmalloc_socket(
				dev->device->driver->name,
				VRB_MAX_QGRPS * VRB_MAX_AQS * sizeof(uint32_t),
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
	acc_reg_write(d, d->reg_addr->tail_ptrs_ul5g_hi, phys_high);
	acc_reg_write(d, d->reg_addr->tail_ptrs_ul5g_lo, phys_low);
	acc_reg_write(d, d->reg_addr->tail_ptrs_dl5g_hi, phys_high);
	acc_reg_write(d, d->reg_addr->tail_ptrs_dl5g_lo, phys_low);
	acc_reg_write(d, d->reg_addr->tail_ptrs_ul4g_hi, phys_high);
	acc_reg_write(d, d->reg_addr->tail_ptrs_ul4g_lo, phys_low);
	acc_reg_write(d, d->reg_addr->tail_ptrs_dl4g_hi, phys_high);
	acc_reg_write(d, d->reg_addr->tail_ptrs_dl4g_lo, phys_low);
	acc_reg_write(d, d->reg_addr->tail_ptrs_fft_hi, phys_high);
	acc_reg_write(d, d->reg_addr->tail_ptrs_fft_lo, phys_low);

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
	vrb_vf2pf(d, ACC_VF2PF_USING_VF);

	rte_bbdev_log_debug(
			"Device (%s) configured  sw_rings = %p, sw_rings_iova = %#"
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
vrb_intr_enable(struct rte_bbdev *dev)
{
	int ret;
	struct acc_device *d = dev->data->dev_private;

	if (d->device_variant == VRB1_VARIANT) {
		/* On VRB1: cannot enable MSI/IR to avoid potential back-pressure corner case. */
		rte_bbdev_log(ERR, "VRB1 (%s) doesn't support any MSI/MSI-X interrupt\n",
				dev->data->name);
		return -ENOTSUP;
	}

	/*
	 * MSI/MSI-X are supported.
	 * Option controlled by vfio-intr through EAL parameter.
	 */
	if (rte_intr_type_get(dev->intr_handle) == RTE_INTR_HANDLE_VFIO_MSI) {

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
				vrb_dev_interrupt_handler, dev);
		if (ret < 0) {
			rte_bbdev_log(ERR,
					"Couldn't register interrupt callback for device: %s",
					dev->data->name);
			rte_free(d->info_ring);
			return ret;
		}

		return 0;
	} else if (rte_intr_type_get(dev->intr_handle) == RTE_INTR_HANDLE_VFIO_MSIX) {
		int i, max_queues;
		struct acc_device *acc_dev = dev->data->dev_private;

		ret = allocate_info_ring(dev);
		if (ret < 0) {
			rte_bbdev_log(ERR,
					"Couldn't allocate info ring for device: %s",
					dev->data->name);
			return ret;
		}

		if (acc_dev->pf_device)
			max_queues = VRB1_MAX_PF_MSIX;
		else
			max_queues = VRB1_MAX_VF_MSIX;

		if (rte_intr_efd_enable(dev->intr_handle, max_queues)) {
			rte_bbdev_log(ERR, "Failed to create fds for %u queues",
					dev->data->num_queues);
			return -1;
		}

		for (i = 0; i < max_queues; ++i) {
			if (rte_intr_efds_index_set(dev->intr_handle, i,
					rte_intr_fd_get(dev->intr_handle)))
				return -rte_errno;
		}

		if (rte_intr_vec_list_alloc(dev->intr_handle, "intr_vec",
				dev->data->num_queues)) {
			rte_bbdev_log(ERR, "Failed to allocate %u vectors",
					dev->data->num_queues);
			return -ENOMEM;
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
				vrb_dev_interrupt_handler, dev);
		if (ret < 0) {
			rte_bbdev_log(ERR,
					"Couldn't register interrupt callback for device: %s",
					dev->data->name);
			rte_free(d->info_ring);
			return ret;
		}

		return 0;
	}

	rte_bbdev_log(ERR, "Device (%s) supports only VFIO MSI/MSI-X interrupts\n",
			dev->data->name);
	return -ENOTSUP;
}

/* Free memory used for software rings. */
static int
vrb_dev_close(struct rte_bbdev *dev)
{
	struct acc_device *d = dev->data->dev_private;
	vrb_check_ir(d);
	if (d->sw_rings_base != NULL) {
		rte_free(d->tail_ptrs);
		rte_free(d->info_ring);
		rte_free(d->sw_rings_base);
		rte_free(d->harq_layout);
		d->tail_ptrs = NULL;
		d->info_ring = NULL;
		d->sw_rings_base = NULL;
		d->harq_layout = NULL;
	}
	/* Ensure all in flight HW transactions are completed. */
	usleep(ACC_LONG_WAIT);
	return 0;
}

/**
 * Report a queue index which is free.
 * Return 0 to 16k for a valid queue_idx or -1 when no queue is available.
 * Note : Only supporting VF0 Bundle for PF mode.
 */
static int
vrb_find_free_queue_idx(struct rte_bbdev *dev,
		const struct rte_bbdev_queue_conf *conf)
{
	struct acc_device *d = dev->data->dev_private;
	int op_2_acc[6] = {0, UL_4G, DL_4G, UL_5G, DL_5G, FFT};
	int acc = op_2_acc[conf->op_type];
	struct rte_acc_queue_topology *qtop = NULL;
	uint16_t group_idx;
	uint64_t aq_idx;

	qtopFromAcc(&qtop, acc, &(d->acc_conf));
	if (qtop == NULL)
		return -1;
	/* Identify matching QGroup Index which are sorted in priority order. */
	group_idx = qtop->first_qgroup_index + conf->priority;
	if (group_idx >= d->num_qgroups ||
			conf->priority >= qtop->num_qgroups) {
		rte_bbdev_log(INFO, "Invalid Priority on %s, priority %u",
				dev->data->name, conf->priority);
		return -1;
	}
	/* Find a free AQ_idx.  */
	for (aq_idx = 0; aq_idx < qtop->num_aqs_per_groups; aq_idx++) {
		if (((d->q_assigned_bit_map[group_idx] >> aq_idx) & 0x1) == 0) {
			/* Mark the Queue as assigned. */
			d->q_assigned_bit_map[group_idx] |= (1ULL << aq_idx);
			/* Report the AQ Index. */
			return (group_idx << VRB1_GRP_ID_SHIFT) + aq_idx;
		}
	}
	rte_bbdev_log(INFO, "Failed to find free queue on %s, priority %u",
			dev->data->name, conf->priority);
	return -1;
}

/* Setup device queue. */
static int
vrb_queue_setup(struct rte_bbdev *dev, uint16_t queue_id,
		const struct rte_bbdev_queue_conf *conf)
{
	struct acc_device *d = dev->data->dev_private;
	struct acc_queue *q;
	int32_t q_idx;
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

	/* Prepare the Ring with default descriptor format. */
	union acc_dma_desc *desc = NULL;
	unsigned int desc_idx, b_idx;
	int fcw_len = (conf->op_type == RTE_BBDEV_OP_LDPC_ENC ?
			ACC_FCW_LE_BLEN : (conf->op_type == RTE_BBDEV_OP_TURBO_DEC ?
			ACC_FCW_TD_BLEN : (conf->op_type == RTE_BBDEV_OP_LDPC_DEC ?
			ACC_FCW_LD_BLEN : ACC_FCW_FFT_BLEN)));

	for (desc_idx = 0; desc_idx < d->sw_ring_max_depth; desc_idx++) {
		desc = q->ring_addr + desc_idx;
		desc->req.word0 = ACC_DMA_DESC_TYPE;
		desc->req.word1 = 0; /**< Timestamp. */
		desc->req.word2 = 0;
		desc->req.word3 = 0;
		uint64_t fcw_offset = (desc_idx << 8) + ACC_DESC_FCW_OFFSET;
		desc->req.data_ptrs[0].address = q->ring_addr_iova + fcw_offset;
		desc->req.data_ptrs[0].blen = fcw_len;
		desc->req.data_ptrs[0].blkid = ACC_DMA_BLKID_FCW;
		desc->req.data_ptrs[0].last = 0;
		desc->req.data_ptrs[0].dma_ext = 0;
		for (b_idx = 1; b_idx < ACC_DMA_MAX_NUM_POINTERS - 1; b_idx++) {
			desc->req.data_ptrs[b_idx].blkid = ACC_DMA_BLKID_IN;
			desc->req.data_ptrs[b_idx].last = 1;
			desc->req.data_ptrs[b_idx].dma_ext = 0;
			b_idx++;
			desc->req.data_ptrs[b_idx].blkid =
					ACC_DMA_BLKID_OUT_ENC;
			desc->req.data_ptrs[b_idx].last = 1;
			desc->req.data_ptrs[b_idx].dma_ext = 0;
		}
		/* Preset some fields of LDPC FCW. */
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

	q_idx = vrb_find_free_queue_idx(dev, conf);
	if (q_idx == -1) {
		ret = -EINVAL;
		goto free_companion_ring_addr;
	}

	q->qgrp_id = (q_idx >> VRB1_GRP_ID_SHIFT) & 0xF;
	q->vf_id = (q_idx >> VRB1_VF_ID_SHIFT)  & 0x3F;
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
	else if (conf->op_type ==  RTE_BBDEV_OP_FFT)
		q->aq_depth = (1 << d->acc_conf.q_fft.aq_depth_log2);

	q->mmio_reg_enqueue = RTE_PTR_ADD(d->mmio_base,
			d->queue_offset(d->pf_device, q->vf_id, q->qgrp_id, q->aq_id));

	rte_bbdev_log_debug(
			"Setup dev%u q%u: qgrp_id=%u, vf_id=%u, aq_id=%u, aq_depth=%u, mmio_reg_enqueue=%p base %p\n",
			dev->data->dev_id, queue_id, q->qgrp_id, q->vf_id,
			q->aq_id, q->aq_depth, q->mmio_reg_enqueue,
			d->mmio_base);

	dev->data->queues[queue_id].queue_private = q;
	return 0;

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
vrb_print_op(struct rte_bbdev_dec_op *op, enum rte_bbdev_op_type op_type,
		uint16_t index)
{
	if (op == NULL)
		return;
	if (op_type == RTE_BBDEV_OP_LDPC_DEC)
		rte_bbdev_log(INFO,
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
		rte_bbdev_log(INFO,
			"  Op 5GDL %d %d %d %d %d %d %d %d %d",
			index,
			op_dl->ldpc_enc.basegraph, op_dl->ldpc_enc.z_c,
			op_dl->ldpc_enc.n_cb, op_dl->ldpc_enc.q_m,
			op_dl->ldpc_enc.n_filler, op_dl->ldpc_enc.cb_params.e,
			op_dl->ldpc_enc.op_flags, op_dl->ldpc_enc.rv_index
			);
	}
}

/* Stop queue and clear counters. */
static int
vrb_queue_stop(struct rte_bbdev *dev, uint16_t queue_id)
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
		vrb_print_op(op, q->op_type, i);
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

/* Release queue. */
static int
vrb_queue_release(struct rte_bbdev *dev, uint16_t q_id)
{
	struct acc_device *d = dev->data->dev_private;
	struct acc_queue *q = dev->data->queues[q_id].queue_private;

	if (q != NULL) {
		/* Mark the Queue as un-assigned. */
		d->q_assigned_bit_map[q->qgrp_id] &= (~0ULL - (1 << (uint64_t) q->aq_id));
		rte_free(q->companion_ring_addr);
		rte_free(q->lb_in);
		rte_free(q->lb_out);
		rte_free(q);
		dev->data->queues[q_id].queue_private = NULL;
	}

	return 0;
}

/* Get device info. */
static void
vrb_dev_info_get(struct rte_bbdev *dev, struct rte_bbdev_driver_info *dev_info)
{
	struct acc_device *d = dev->data->dev_private;
	int i;
	static const struct rte_bbdev_op_cap vrb1_bbdev_capabilities[] = {
		{
			.type = RTE_BBDEV_OP_TURBO_DEC,
			.cap.turbo_dec = {
				.capability_flags =
					RTE_BBDEV_TURBO_SUBBLOCK_DEINTERLEAVE |
					RTE_BBDEV_TURBO_CRC_TYPE_24B |
					RTE_BBDEV_TURBO_DEC_CRC_24B_DROP |
					RTE_BBDEV_TURBO_EQUALIZER |
					RTE_BBDEV_TURBO_SOFT_OUT_SATURATE |
					RTE_BBDEV_TURBO_HALF_ITERATION_EVEN |
					RTE_BBDEV_TURBO_CONTINUE_CRC_MATCH |
					RTE_BBDEV_TURBO_SOFT_OUTPUT |
					RTE_BBDEV_TURBO_EARLY_TERMINATION |
					RTE_BBDEV_TURBO_DEC_INTERRUPTS |
					RTE_BBDEV_TURBO_NEG_LLR_1_BIT_IN |
					RTE_BBDEV_TURBO_NEG_LLR_1_BIT_SOFT_OUT |
					RTE_BBDEV_TURBO_MAP_DEC |
					RTE_BBDEV_TURBO_DEC_TB_CRC_24B_KEEP |
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
				RTE_BBDEV_LDPC_CRC_TYPE_24A_CHECK |
				RTE_BBDEV_LDPC_CRC_TYPE_16_CHECK |
				RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE |
				RTE_BBDEV_LDPC_HQ_COMBINE_OUT_ENABLE |
				RTE_BBDEV_LDPC_ITERATION_STOP_ENABLE |
				RTE_BBDEV_LDPC_DEINTERLEAVER_BYPASS |
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
		{
			.type	= RTE_BBDEV_OP_FFT,
			.cap.fft = {
				.capability_flags =
						RTE_BBDEV_FFT_WINDOWING |
						RTE_BBDEV_FFT_CS_ADJUSTMENT |
						RTE_BBDEV_FFT_DFT_BYPASS |
						RTE_BBDEV_FFT_IDFT_BYPASS |
						RTE_BBDEV_FFT_WINDOWING_BYPASS,
				.num_buffers_src =
						RTE_BBDEV_LDPC_MAX_CODE_BLOCKS,
				.num_buffers_dst =
						RTE_BBDEV_LDPC_MAX_CODE_BLOCKS,
			}
		},
		RTE_BBDEV_END_OF_CAPABILITIES_LIST()
	};

	static struct rte_bbdev_queue_conf default_queue_conf;
	default_queue_conf.socket = dev->data->socket_id;
	default_queue_conf.queue_size = ACC_MAX_QUEUE_DEPTH;

	dev_info->driver_name = dev->device->driver->name;

	/* Read and save the populated config from registers. */
	fetch_acc_config(dev);
	/* Check the status of device. */
	dev_info->device_status = vrb_device_status(dev);

	/* Exposed number of queues. */
	dev_info->num_queues[RTE_BBDEV_OP_NONE] = 0;
	dev_info->num_queues[RTE_BBDEV_OP_TURBO_DEC] = d->acc_conf.q_ul_4g.num_aqs_per_groups *
			d->acc_conf.q_ul_4g.num_qgroups;
	dev_info->num_queues[RTE_BBDEV_OP_TURBO_ENC] = d->acc_conf.q_dl_4g.num_aqs_per_groups *
			d->acc_conf.q_dl_4g.num_qgroups;
	dev_info->num_queues[RTE_BBDEV_OP_LDPC_DEC] = d->acc_conf.q_ul_5g.num_aqs_per_groups *
			d->acc_conf.q_ul_5g.num_qgroups;
	dev_info->num_queues[RTE_BBDEV_OP_LDPC_ENC] = d->acc_conf.q_dl_5g.num_aqs_per_groups *
			d->acc_conf.q_dl_5g.num_qgroups;
	dev_info->num_queues[RTE_BBDEV_OP_FFT] = d->acc_conf.q_fft.num_aqs_per_groups *
			d->acc_conf.q_fft.num_qgroups;
	dev_info->queue_priority[RTE_BBDEV_OP_TURBO_DEC] = d->acc_conf.q_ul_4g.num_qgroups;
	dev_info->queue_priority[RTE_BBDEV_OP_TURBO_ENC] = d->acc_conf.q_dl_4g.num_qgroups;
	dev_info->queue_priority[RTE_BBDEV_OP_LDPC_DEC] = d->acc_conf.q_ul_5g.num_qgroups;
	dev_info->queue_priority[RTE_BBDEV_OP_LDPC_ENC] = d->acc_conf.q_dl_5g.num_qgroups;
	dev_info->queue_priority[RTE_BBDEV_OP_FFT] = d->acc_conf.q_fft.num_qgroups;
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
	dev_info->capabilities = vrb1_bbdev_capabilities;
	dev_info->harq_buffer_size = 0;

	vrb_check_ir(d);
}

static int
vrb_queue_intr_enable(struct rte_bbdev *dev, uint16_t queue_id)
{
	struct acc_queue *q = dev->data->queues[queue_id].queue_private;

	if (rte_intr_type_get(dev->intr_handle) != RTE_INTR_HANDLE_VFIO_MSI &&
			rte_intr_type_get(dev->intr_handle) != RTE_INTR_HANDLE_VFIO_MSIX)
		return -ENOTSUP;

	q->irq_enable = 1;
	return 0;
}

static int
vrb_queue_intr_disable(struct rte_bbdev *dev, uint16_t queue_id)
{
	struct acc_queue *q = dev->data->queues[queue_id].queue_private;

	if (rte_intr_type_get(dev->intr_handle) != RTE_INTR_HANDLE_VFIO_MSI &&
			rte_intr_type_get(dev->intr_handle) != RTE_INTR_HANDLE_VFIO_MSIX)
		return -ENOTSUP;

	q->irq_enable = 0;
	return 0;
}

static const struct rte_bbdev_ops vrb_bbdev_ops = {
	.setup_queues = vrb_setup_queues,
	.intr_enable = vrb_intr_enable,
	.close = vrb_dev_close,
	.info_get = vrb_dev_info_get,
	.queue_setup = vrb_queue_setup,
	.queue_release = vrb_queue_release,
	.queue_stop = vrb_queue_stop,
	.queue_intr_enable = vrb_queue_intr_enable,
	.queue_intr_disable = vrb_queue_intr_disable
};

/* PCI PF address map. */
static struct rte_pci_id pci_id_vrb_pf_map[] = {
	{
		RTE_PCI_DEVICE(RTE_VRB1_VENDOR_ID, RTE_VRB1_PF_DEVICE_ID)
	},
	{.device_id = 0},
};

/* PCI VF address map. */
static struct rte_pci_id pci_id_vrb_vf_map[] = {
	{
		RTE_PCI_DEVICE(RTE_VRB1_VENDOR_ID, RTE_VRB1_VF_DEVICE_ID)
	},
	{.device_id = 0},
};

/* Fill in a frame control word for turbo decoding. */
static inline void
vrb_fcw_td_fill(const struct rte_bbdev_dec_op *op, struct acc_fcw_td *fcw)
{
	fcw->fcw_ver = 1;
	fcw->num_maps = ACC_FCW_TD_AUTOMAP;
	fcw->bypass_sb_deint = !check_bit(op->turbo_dec.op_flags,
			RTE_BBDEV_TURBO_SUBBLOCK_DEINTERLEAVE);
	if (op->turbo_dec.code_block_mode == RTE_BBDEV_TRANSPORT_BLOCK) {
		fcw->c = op->turbo_dec.tb_params.c;
		fcw->k_pos = op->turbo_dec.tb_params.k_pos;
	} else {
		fcw->c = 1;
		fcw->k_pos = op->turbo_dec.cb_params.k;
	}
	if (check_bit(op->turbo_dec.op_flags, RTE_BBDEV_TURBO_SOFT_OUTPUT)) {
		fcw->soft_output_en = 1;
		fcw->sw_soft_out_dis = 0;
		fcw->sw_et_cont = check_bit(op->turbo_dec.op_flags,
				RTE_BBDEV_TURBO_CONTINUE_CRC_MATCH);
		fcw->sw_soft_out_saturation = check_bit(op->turbo_dec.op_flags,
				RTE_BBDEV_TURBO_SOFT_OUT_SATURATE);
		if (check_bit(op->turbo_dec.op_flags,
				RTE_BBDEV_TURBO_EQUALIZER)) {
			fcw->bypass_teq = 0;
			if (op->turbo_dec.code_block_mode == RTE_BBDEV_TRANSPORT_BLOCK) {
				fcw->cab = op->turbo_dec.tb_params.cab;
				fcw->ea = op->turbo_dec.tb_params.ea;
				fcw->eb = op->turbo_dec.tb_params.eb;
			} else {
				fcw->ea = op->turbo_dec.cb_params.e;
				fcw->eb = op->turbo_dec.cb_params.e;
			}
			if (op->turbo_dec.rv_index == 0)
				fcw->k0_start_col = ACC_FCW_TD_RVIDX_0;
			else if (op->turbo_dec.rv_index == 1)
				fcw->k0_start_col = ACC_FCW_TD_RVIDX_1;
			else if (op->turbo_dec.rv_index == 2)
				fcw->k0_start_col = ACC_FCW_TD_RVIDX_2;
			else
				fcw->k0_start_col = ACC_FCW_TD_RVIDX_3;
		} else {
			fcw->bypass_teq = 1;
			fcw->eb = 64; /* avoid undefined value */
		}
	} else {
		fcw->soft_output_en = 0;
		fcw->sw_soft_out_dis = 1;
		fcw->bypass_teq = 0;
	}

	fcw->code_block_mode = 1; /* FIXME */
	fcw->turbo_crc_type = check_bit(op->turbo_dec.op_flags,
			RTE_BBDEV_TURBO_CRC_TYPE_24B);

	fcw->ext_td_cold_reg_en = 1;
	fcw->raw_decoder_input_on = 0;
	fcw->max_iter = RTE_MAX((uint8_t) op->turbo_dec.iter_max, 2);
	fcw->min_iter = 2;
	fcw->half_iter_on = check_bit(op->turbo_dec.op_flags, RTE_BBDEV_TURBO_HALF_ITERATION_EVEN);

	fcw->early_stop_en = check_bit(op->turbo_dec.op_flags,
			RTE_BBDEV_TURBO_EARLY_TERMINATION) & !fcw->soft_output_en;
	fcw->ext_scale = 0xF;
}

/* Fill in a frame control word for LDPC decoding. */
static inline void
vrb1_fcw_ld_fill(struct rte_bbdev_dec_op *op, struct acc_fcw_ld *fcw,
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
		/* Disable HARQ input in that case to carry forward. */
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
	fcw->bypass_dec = 0;
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
		harq_in_length = RTE_ALIGN_CEIL(harq_in_length, 64);
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
	fcw->cnu_algo = ACC_ALGO_MSA;
	fcw->synd_precoder = fcw->itstop;
	/*
	 * These are all implicitly set:
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
		l = k0_p + fcw->rm_e;
		harq_out_length = (uint16_t) fcw->hcin_size0;
		harq_out_length = RTE_MIN(RTE_MAX(harq_out_length, l), ncb_p);
		harq_out_length = RTE_ALIGN_CEIL(harq_out_length, 64);
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

	fcw->tb_crc_select = 0;
	if (check_bit(op->ldpc_dec.op_flags, RTE_BBDEV_LDPC_CRC_TYPE_24A_CHECK))
		fcw->tb_crc_select = 2;
	if (check_bit(op->ldpc_dec.op_flags, RTE_BBDEV_LDPC_CRC_TYPE_16_CHECK))
		fcw->tb_crc_select = 1;
}

static inline int
vrb_dma_desc_td_fill(struct rte_bbdev_dec_op *op,
		struct acc_dma_req_desc *desc, struct rte_mbuf **input,
		struct rte_mbuf *h_output, struct rte_mbuf *s_output,
		uint32_t *in_offset, uint32_t *h_out_offset,
		uint32_t *s_out_offset, uint32_t *h_out_length,
		uint32_t *s_out_length, uint32_t *mbuf_total_left,
		uint32_t *seg_total_left, uint8_t r)
{
	int next_triplet = 1; /* FCW already done. */
	uint16_t k;
	uint16_t crc24_overlap = 0;
	uint32_t e, kw;

	desc->word0 = ACC_DMA_DESC_TYPE;
	desc->word1 = 0; /**< Timestamp could be disabled. */
	desc->word2 = 0;
	desc->word3 = 0;
	desc->numCBs = 1;

	if (op->turbo_dec.code_block_mode == RTE_BBDEV_TRANSPORT_BLOCK) {
		k = op->turbo_dec.tb_params.k_pos;
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
	*h_out_length = ((k - crc24_overlap) >> 3);
	next_triplet = acc_dma_fill_blk_type(
			desc, h_output, *h_out_offset,
			*h_out_length, next_triplet, ACC_DMA_BLKID_OUT_HARD);
	if (unlikely(next_triplet < 0)) {
		rte_bbdev_log(ERR,
				"Mismatch between data to process and mbuf data length in bbdev_op: %p",
				op);
		return -1;
	}

	op->turbo_dec.hard_output.length += *h_out_length;
	*h_out_offset += *h_out_length;

	/* Soft output. */
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
vrb1_dma_desc_ld_fill(struct rte_bbdev_dec_op *op,
		struct acc_dma_req_desc *desc,
		struct rte_mbuf **input, struct rte_mbuf *h_output,
		uint32_t *in_offset, uint32_t *h_out_offset,
		uint32_t *h_out_length, uint32_t *mbuf_total_left,
		uint32_t *seg_total_left, struct acc_fcw_ld *fcw)
{
	struct rte_bbdev_op_ldpc_dec *dec = &op->ldpc_dec;
	int next_triplet = 1; /* FCW already done. */
	uint32_t input_length;
	uint16_t output_length, crc24_overlap = 0;
	uint16_t sys_cols, K, h_p_size, h_np_size;
	bool h_comp = check_bit(dec->op_flags, RTE_BBDEV_LDPC_HARQ_6BIT_COMPRESSION);

	acc_header_init(desc);

	if (check_bit(op->ldpc_dec.op_flags, RTE_BBDEV_LDPC_CRC_TYPE_24B_DROP))
		crc24_overlap = 24;

	/* Compute some LDPC BG lengths. */
	input_length = fcw->rm_e;
	if (check_bit(op->ldpc_dec.op_flags, RTE_BBDEV_LDPC_LLR_COMPRESSION))
		input_length = (input_length * 3 + 3) / 4;
	sys_cols = (dec->basegraph == 1) ? 22 : 10;
	K = sys_cols * dec->z_c;
	output_length = K - dec->n_filler - crc24_overlap;

	if (unlikely((*mbuf_total_left == 0) || (*mbuf_total_left < input_length))) {
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

	if (check_bit(op->ldpc_dec.op_flags, RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE)) {
		if (op->ldpc_dec.harq_combined_input.data == 0) {
			rte_bbdev_log(ERR, "HARQ input is not defined");
			return -1;
		}
		h_p_size = fcw->hcin_size0 + fcw->hcin_size1;
		if (h_comp)
			h_p_size = (h_p_size * 3 + 3) / 4;
		if (op->ldpc_dec.harq_combined_input.data == 0) {
			rte_bbdev_log(ERR, "HARQ input is not defined");
			return -1;
		}
		acc_dma_fill_blk_type(
				desc,
				op->ldpc_dec.harq_combined_input.data,
				op->ldpc_dec.harq_combined_input.offset,
				h_p_size,
				next_triplet,
				ACC_DMA_BLKID_IN_HARQ);
		next_triplet++;
	}

	desc->data_ptrs[next_triplet - 1].last = 1;
	desc->m2dlen = next_triplet;
	*mbuf_total_left -= input_length;

	next_triplet = acc_dma_fill_blk_type(desc, h_output,
			*h_out_offset, output_length >> 3, next_triplet,
			ACC_DMA_BLKID_OUT_HARD);

	if (check_bit(op->ldpc_dec.op_flags, RTE_BBDEV_LDPC_HQ_COMBINE_OUT_ENABLE)) {
		if (op->ldpc_dec.harq_combined_output.data == 0) {
			rte_bbdev_log(ERR, "HARQ output is not defined");
			return -1;
		}

		/* Pruned size of the HARQ. */
		h_p_size = fcw->hcout_size0 + fcw->hcout_size1;
		/* Non-Pruned size of the HARQ. */
		h_np_size = fcw->hcout_offset > 0 ?
				fcw->hcout_offset + fcw->hcout_size1 :
				h_p_size;
		if (h_comp) {
			h_np_size = (h_np_size * 3 + 3) / 4;
			h_p_size = (h_p_size * 3 + 3) / 4;
		}
		dec->harq_combined_output.length = h_np_size;
		acc_dma_fill_blk_type(
				desc,
				dec->harq_combined_output.data,
				dec->harq_combined_output.offset,
				h_p_size,
				next_triplet,
				ACC_DMA_BLKID_OUT_HARQ);

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
vrb_dma_desc_ld_update(struct rte_bbdev_dec_op *op,
		struct acc_dma_req_desc *desc,
		struct rte_mbuf *input, struct rte_mbuf *h_output,
		uint32_t *in_offset, uint32_t *h_out_offset,
		uint32_t *h_out_length,
		union acc_harq_layout_data *harq_layout)
{
	int next_triplet = 1; /* FCW already done. */
	desc->data_ptrs[next_triplet].address = rte_pktmbuf_iova_offset(input, *in_offset);
	next_triplet++;

	if (check_bit(op->ldpc_dec.op_flags, RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE)) {
		struct rte_bbdev_op_data hi = op->ldpc_dec.harq_combined_input;
		desc->data_ptrs[next_triplet].address =
				rte_pktmbuf_iova_offset(hi.data, hi.offset);
		next_triplet++;
	}

	desc->data_ptrs[next_triplet].address =
			rte_pktmbuf_iova_offset(h_output, *h_out_offset);
	*h_out_length = desc->data_ptrs[next_triplet].blen;
	next_triplet++;

	if (check_bit(op->ldpc_dec.op_flags,
				RTE_BBDEV_LDPC_HQ_COMBINE_OUT_ENABLE)) {
		/* Adjust based on previous operation. */
		struct rte_bbdev_dec_op *prev_op = desc->op_addr;
		op->ldpc_dec.harq_combined_output.length =
				prev_op->ldpc_dec.harq_combined_output.length;
		uint32_t harq_idx = hq_index(op->ldpc_dec.harq_combined_output.offset);
		uint32_t prev_harq_idx = hq_index(prev_op->ldpc_dec.harq_combined_output.offset);
		harq_layout[harq_idx].val = harq_layout[prev_harq_idx].val;
		struct rte_bbdev_op_data ho = op->ldpc_dec.harq_combined_output;
		desc->data_ptrs[next_triplet].address =
				rte_pktmbuf_iova_offset(ho.data, ho.offset);
		next_triplet++;
	}

	op->ldpc_dec.hard_output.length += *h_out_length;
	desc->op_addr = op;
}

/* Enqueue one encode operations for device in CB mode */
static inline int
enqueue_enc_one_op_cb(struct acc_queue *q, struct rte_bbdev_enc_op *op,
		uint16_t total_enqueued_cbs)
{
	union acc_dma_desc *desc = NULL;
	int ret;
	uint32_t in_offset, out_offset, out_length, mbuf_total_left, seg_total_left;
	struct rte_mbuf *input, *output_head, *output;

	desc = acc_desc(q, total_enqueued_cbs);
	acc_fcw_te_fill(op, &desc->req.fcw_te);

	input = op->turbo_enc.input.data;
	output_head = output = op->turbo_enc.output.data;
	in_offset = op->turbo_enc.input.offset;
	out_offset = op->turbo_enc.output.offset;
	out_length = 0;
	mbuf_total_left = op->turbo_enc.input.length;
	seg_total_left = rte_pktmbuf_data_len(op->turbo_enc.input.data) - in_offset;

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
#endif
	/* One CB (one op) was successfully prepared to enqueue */
	return 1;
}

/* Enqueue one encode operations for device in CB mode
 * multiplexed on the same descriptor.
 */
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
	struct acc_ptrs *context_ptrs;

	desc = acc_desc(q, total_enqueued_descs);
	acc_fcw_le_fill(ops[0], &desc->req.fcw_le, num, 0);

	/** This could be done at polling. */
	acc_header_init(&desc->req);
	desc->req.numCBs = num;

	in_length_in_bytes = ops[0]->ldpc_enc.input.data->data_len;
	out_length = (enc->cb_params.e + 7) >> 3;
	desc->req.m2dlen = 1 + num;
	desc->req.d2mlen = num;
	next_triplet = 1;

	for (i = 0; i < num; i++) {
		desc->req.data_ptrs[next_triplet].address =
			rte_pktmbuf_iova_offset(ops[i]->ldpc_enc.input.data, 0);
		desc->req.data_ptrs[next_triplet].blen = in_length_in_bytes;
		next_triplet++;
		desc->req.data_ptrs[next_triplet].address = rte_pktmbuf_iova_offset(
				ops[i]->ldpc_enc.output.data, 0);
		desc->req.data_ptrs[next_triplet].blen = out_length;
		next_triplet++;
		ops[i]->ldpc_enc.output.length = out_length;
		output_head = output = ops[i]->ldpc_enc.output.data;
		mbuf_append(output_head, output, out_length);
		output->data_len = out_length;
	}

	desc->req.op_addr = ops[0];
	/* Keep track of pointers even when multiplexed in single descriptor. */
	context_ptrs = q->companion_ring_addr + acc_desc_idx(q, total_enqueued_descs);
	for (i = 0; i < num; i++)
		context_ptrs->ptr[i].op_addr = ops[i];

#ifdef RTE_LIBRTE_BBDEV_DEBUG
	rte_memdump(stderr, "FCW", &desc->req.fcw_le,
			sizeof(desc->req.fcw_le) - 8);
	rte_memdump(stderr, "Req Desc.", desc, sizeof(*desc));
#endif

	/* Number of compatible CBs/ops successfully prepared to enqueue. */
	return num;
}

/* Enqueue one encode operations for device for a partial TB
 * all codes blocks have same configuration multiplexed on the same descriptor.
 */
static inline void
vrb1_enqueue_ldpc_enc_part_tb(struct acc_queue *q, struct rte_bbdev_enc_op *op,
		uint16_t total_enqueued_descs, int16_t num_cbs, uint32_t e,
		uint16_t in_len_B, uint32_t out_len_B, uint32_t *in_offset,
		uint32_t *out_offset)
{

	union acc_dma_desc *desc = NULL;
	struct rte_mbuf *output_head, *output;
	int i, next_triplet;
	struct rte_bbdev_op_ldpc_enc *enc = &op->ldpc_enc;

	desc = acc_desc(q, total_enqueued_descs);
	acc_fcw_le_fill(op, &desc->req.fcw_le, num_cbs, e);

	/** This could be done at polling. */
	acc_header_init(&desc->req);
	desc->req.numCBs = num_cbs;

	desc->req.m2dlen = 1 + num_cbs;
	desc->req.d2mlen = num_cbs;
	next_triplet = 1;

	for (i = 0; i < num_cbs; i++) {
		desc->req.data_ptrs[next_triplet].address = rte_pktmbuf_iova_offset(
				enc->input.data, *in_offset);
		*in_offset += in_len_B;
		desc->req.data_ptrs[next_triplet].blen = in_len_B;
		next_triplet++;
		desc->req.data_ptrs[next_triplet].address = rte_pktmbuf_iova_offset(
				enc->output.data, *out_offset);
		*out_offset += out_len_B;
		desc->req.data_ptrs[next_triplet].blen = out_len_B;
		next_triplet++;
		enc->output.length += out_len_B;
		output_head = output = enc->output.data;
		mbuf_append(output_head, output, out_len_B);
	}

#ifdef RTE_LIBRTE_BBDEV_DEBUG
	rte_memdump(stderr, "FCW", &desc->req.fcw_le,
			sizeof(desc->req.fcw_le) - 8);
	rte_memdump(stderr, "Req Desc.", desc, sizeof(*desc));
#endif

}

/* Enqueue one encode operations for device in TB mode. */
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
		if (unlikely((input == NULL) || (output == NULL)))
			return -1;

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

	/* In case the number of CB doesn't match, the configuration was invalid. */
	if (unlikely(current_enqueued_cbs != cbs_in_tb))
		return -1;

	/* Set SDone on last CB descriptor for TB mode. */
	desc->req.sdone_enable = 1;

	return current_enqueued_cbs;
}

/* Enqueue one encode operations for device in TB mode.
 * returns the number of descs used.
 */
static inline int
vrb1_enqueue_ldpc_enc_one_op_tb(struct acc_queue *q, struct rte_bbdev_enc_op *op,
		uint16_t enq_descs, uint8_t cbs_in_tb)
{
	uint8_t num_a, num_b;
	uint16_t input_len_B, return_descs;
	uint8_t r = op->ldpc_enc.tb_params.r;
	uint8_t cab =  op->ldpc_enc.tb_params.cab;
	union acc_dma_desc *desc;
	uint16_t init_enq_descs = enq_descs;
	uint32_t in_offset = 0, out_offset = 0;

	input_len_B = ((op->ldpc_enc.basegraph == 1 ? 22 : 10) * op->ldpc_enc.z_c) >> 3;

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
		uint32_t out_len_B = (e + 7) >> 3;
		uint8_t enq = RTE_MIN(num_a, ACC_MUX_5GDL_DESC);
		num_a -= enq;
		vrb1_enqueue_ldpc_enc_part_tb(q, op, enq_descs, enq, e, input_len_B,
				out_len_B, &in_offset, &out_offset);
		enq_descs++;
	}
	while (num_b > 0) {
		uint32_t e = op->ldpc_enc.tb_params.eb;
		uint32_t out_len_B = (e + 7) >> 3;
		uint8_t enq = RTE_MIN(num_b, ACC_MUX_5GDL_DESC);
		num_b -= enq;
		vrb1_enqueue_ldpc_enc_part_tb(q, op, enq_descs, enq, e, input_len_B,
				out_len_B, &in_offset, &out_offset);
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

/** Enqueue one decode operations for device in CB mode. */
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

	desc = acc_desc(q, total_enqueued_cbs);
	vrb_fcw_td_fill(op, &desc->req.fcw_td);

	input = op->turbo_dec.input.data;
	h_output_head = h_output = op->turbo_dec.hard_output.data;
	s_output_head = s_output = op->turbo_dec.soft_output.data;
	in_offset = op->turbo_dec.input.offset;
	h_out_offset = op->turbo_dec.hard_output.offset;
	s_out_offset = op->turbo_dec.soft_output.offset;
	h_out_length = s_out_length = 0;
	mbuf_total_left = op->turbo_dec.input.length;
	seg_total_left = rte_pktmbuf_data_len(input) - in_offset;

	/* Set up DMA descriptor */
	desc = acc_desc(q, total_enqueued_cbs);

	ret = vrb_dma_desc_td_fill(op, &desc->req, &input, h_output,
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
			sizeof(desc->req.fcw_td));
	rte_memdump(stderr, "Req Desc.", desc, sizeof(*desc));
#endif

	/* One CB (one op) was successfully prepared to enqueue */
	return 1;
}

/** Enqueue one decode operations for device in CB mode */
static inline int
vrb_enqueue_ldpc_dec_one_op_cb(struct acc_queue *q, struct rte_bbdev_dec_op *op,
		uint16_t total_enqueued_cbs, bool same_op)
{
	int ret, hq_len;
	union acc_dma_desc *desc;
	struct rte_mbuf *input, *h_output_head, *h_output;
	uint32_t in_offset, h_out_offset, mbuf_total_left, h_out_length = 0;
	union acc_harq_layout_data *harq_layout;

	if (op->ldpc_dec.cb_params.e == 0)
		return -EINVAL;

	desc = acc_desc(q, total_enqueued_cbs);

	input = op->ldpc_dec.input.data;
	h_output_head = h_output = op->ldpc_dec.hard_output.data;
	in_offset = op->ldpc_dec.input.offset;
	h_out_offset = op->ldpc_dec.hard_output.offset;
	mbuf_total_left = op->ldpc_dec.input.length;
	harq_layout = q->d->harq_layout;

	if (same_op) {
		union acc_dma_desc *prev_desc;
		prev_desc = acc_desc(q, total_enqueued_cbs - 1);
		uint8_t *prev_ptr = (uint8_t *) prev_desc;
		uint8_t *new_ptr = (uint8_t *) desc;
		/* Copy first 4 words and BDESCs. */
		rte_memcpy(new_ptr, prev_ptr, ACC_5GUL_SIZE_0);
		rte_memcpy(new_ptr + ACC_5GUL_OFFSET_0,
				prev_ptr + ACC_5GUL_OFFSET_0,
				ACC_5GUL_SIZE_1);
		desc->req.op_addr = prev_desc->req.op_addr;
		/* Copy FCW. */
		rte_memcpy(new_ptr + ACC_DESC_FCW_OFFSET,
				prev_ptr + ACC_DESC_FCW_OFFSET,
				ACC_FCW_LD_BLEN);
		vrb_dma_desc_ld_update(op, &desc->req, input, h_output,
				&in_offset, &h_out_offset,
				&h_out_length, harq_layout);
	} else {
		struct acc_fcw_ld *fcw;
		uint32_t seg_total_left;
		fcw = &desc->req.fcw_ld;
		q->d->fcw_ld_fill(op, fcw, harq_layout);

		/* Special handling when using mbuf or not. */
		if (check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_DEC_SCATTER_GATHER))
			seg_total_left = rte_pktmbuf_data_len(input) - in_offset;
		else
			seg_total_left = fcw->rm_e;

		ret = vrb1_dma_desc_ld_fill(op, &desc->req, &input, h_output,
				&in_offset, &h_out_offset,
				&h_out_length, &mbuf_total_left,
				&seg_total_left, fcw);
		if (unlikely(ret < 0))
			return ret;
	}

	/* Hard output. */
	mbuf_append(h_output_head, h_output, h_out_length);
	if (op->ldpc_dec.harq_combined_output.length > 0) {
		/* Push the HARQ output into host memory. */
		struct rte_mbuf *hq_output_head, *hq_output;
		hq_output_head = op->ldpc_dec.harq_combined_output.data;
		hq_output = op->ldpc_dec.harq_combined_output.data;
		hq_len = op->ldpc_dec.harq_combined_output.length;
		if (unlikely(!mbuf_append(hq_output_head, hq_output, hq_len))) {
			rte_bbdev_log(ERR, "HARQ output mbuf issue %d %d\n",
					hq_output->buf_len,
					hq_len);
			return -1;
		}
	}

	if (op->ldpc_dec.soft_output.length > 0)
		mbuf_append(op->ldpc_dec.soft_output.data, op->ldpc_dec.soft_output.data,
				op->ldpc_dec.soft_output.length);

#ifdef RTE_LIBRTE_BBDEV_DEBUG
	rte_memdump(stderr, "FCW", &desc->req.fcw_ld,
			sizeof(desc->req.fcw_ld) - 8);
	rte_memdump(stderr, "Req Desc.", desc, sizeof(*desc));
#endif

	/* One CB (one op) was successfully prepared to enqueue. */
	return 1;
}


/* Enqueue one decode operations for device in TB mode. */
static inline int
vrb_enqueue_ldpc_dec_one_op_tb(struct acc_queue *q, struct rte_bbdev_dec_op *op,
		uint16_t total_enqueued_cbs, uint8_t cbs_in_tb)
{
	union acc_dma_desc *desc = NULL;
	union acc_dma_desc *desc_first = NULL;
	int ret;
	uint8_t r, c;
	uint32_t in_offset, h_out_offset, h_out_length, mbuf_total_left, seg_total_left;
	struct rte_mbuf *input, *h_output_head, *h_output;
	uint16_t current_enqueued_cbs = 0;
	uint16_t desc_idx, sys_cols, trail_len = 0;
	uint64_t fcw_offset;
	union acc_harq_layout_data *harq_layout;

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
	if (check_bit(op->ldpc_dec.op_flags, RTE_BBDEV_LDPC_CRC_TYPE_24A_CHECK)) {
		sys_cols = (op->ldpc_dec.basegraph == 1) ? 22 : 10;
		trail_len = sys_cols * op->ldpc_dec.z_c -
				op->ldpc_dec.n_filler - 24;
	}

	while (mbuf_total_left > 0 && r < c) {
		if (unlikely((input == NULL) || (h_output == NULL)))
			return -1;

		if (check_bit(op->ldpc_dec.op_flags, RTE_BBDEV_LDPC_DEC_SCATTER_GATHER))
			seg_total_left = rte_pktmbuf_data_len(input) - in_offset;
		else
			seg_total_left = op->ldpc_dec.input.length;
		/* Set up DMA descriptor. */
		desc_idx = acc_desc_idx(q, total_enqueued_cbs);
		desc = q->ring_addr + desc_idx;
		fcw_offset = (desc_idx << 8) + ACC_DESC_FCW_OFFSET;
		desc->req.data_ptrs[0].address = q->ring_addr_iova + fcw_offset;
		desc->req.data_ptrs[0].blen = ACC_FCW_LD_BLEN;
		rte_memcpy(&desc->req.fcw_ld, &desc_first->req.fcw_ld, ACC_FCW_LD_BLEN);
		desc->req.fcw_ld.tb_trailer_size = (c - r - 1) * trail_len;

		ret = vrb1_dma_desc_ld_fill(op, &desc->req, &input,
				h_output, &in_offset, &h_out_offset,
				&h_out_length,
				&mbuf_total_left, &seg_total_left,
				&desc->req.fcw_ld);

		if (unlikely(ret < 0))
			return ret;

		/* Hard output. */
		mbuf_append(h_output_head, h_output, h_out_length);

		/* Set total number of CBs in TB. */
		desc->req.cbs_in_tb = cbs_in_tb;
#ifdef RTE_LIBRTE_BBDEV_DEBUG
		rte_memdump(stderr, "FCW", &desc->req.fcw_td,
				sizeof(desc->req.fcw_td) - 8);
		rte_memdump(stderr, "Req Desc.", desc, sizeof(*desc));
#endif
		if (check_bit(op->ldpc_dec.op_flags, RTE_BBDEV_LDPC_DEC_SCATTER_GATHER)
				&& (seg_total_left == 0)) {
			/* Go to the next mbuf. */
			input = input->next;
			in_offset = 0;
			h_output = h_output->next;
			h_out_offset = 0;
		}
		total_enqueued_cbs++;
		current_enqueued_cbs++;
		r++;
	}

	/* In case the number of CB doesn't match, the configuration was invalid. */
	if (unlikely(current_enqueued_cbs != cbs_in_tb))
		return -1;

#ifdef RTE_LIBRTE_BBDEV_DEBUG
	if (check_mbuf_total_left(mbuf_total_left) != 0)
		return -EINVAL;
#endif
	/* Set SDone on last CB descriptor for TB mode. */
	desc->req.sdone_enable = 1;

	return current_enqueued_cbs;
}

/* Enqueue one decode operations for device in TB mode */
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
	uint16_t desc_idx, current_enqueued_cbs = 0;
	uint64_t fcw_offset;

	desc_idx = acc_desc_idx(q, total_enqueued_cbs);
	desc = q->ring_addr + desc_idx;
	fcw_offset = (desc_idx << 8) + ACC_DESC_FCW_OFFSET;
	vrb_fcw_td_fill(op, &desc->req.fcw_td);

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
		if (unlikely((input == NULL) || (h_output == NULL)))
			return -1;

		seg_total_left = rte_pktmbuf_data_len(input) - in_offset;

		/* Set up DMA descriptor */
		desc = acc_desc(q, total_enqueued_cbs);
		desc->req.data_ptrs[0].address = q->ring_addr_iova + fcw_offset;
		desc->req.data_ptrs[0].blen = ACC_FCW_TD_BLEN;
		ret = vrb_dma_desc_td_fill(op, &desc->req, &input,
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

	/* In case the number of CB doesn't match, the configuration was invalid. */
	if (unlikely(current_enqueued_cbs != cbs_in_tb))
		return -1;

	/* Set SDone on last CB descriptor for TB mode */
	desc->req.sdone_enable = 1;

	return current_enqueued_cbs;
}

/* Enqueue encode operations for device in CB mode. */
static uint16_t
vrb_enqueue_enc_cb(struct rte_bbdev_queue_data *q_data,
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

/** Enqueue encode operations for device in CB mode. */
static inline uint16_t
vrb_enqueue_ldpc_enc_cb(struct rte_bbdev_queue_data *q_data,
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
		ret = enqueue_ldpc_enc_n_op_cb(q, &ops[i], desc_idx, enq);
		if (ret < 0) {
			acc_enqueue_invalid(q_data);
			break;
		}
		i += enq;
		desc_idx++;
		left = num - i;
	}

	if (unlikely(i == 0))
		return 0; /* Nothing to enqueue. */

	acc_dma_enqueue(q, desc_idx, &q_data->queue_stats);

	/* Update stats. */
	q_data->queue_stats.enqueued_count += i;
	q_data->queue_stats.enqueue_err_count += num - i;

	return i;
}

/* Enqueue encode operations for device in TB mode. */
static uint16_t
vrb_enqueue_enc_tb(struct rte_bbdev_queue_data *q_data,
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
		if (unlikely((avail - cbs_in_tb < 0) || (cbs_in_tb == 0))) {
			acc_enqueue_ring_full(q_data);
			break;
		}
		avail -= cbs_in_tb;

		ret = enqueue_enc_one_op_tb(q, ops[i], enqueued_cbs, cbs_in_tb);
		if (ret <= 0) {
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

/* Enqueue LDPC encode operations for device in TB mode. */
static uint16_t
vrb_enqueue_ldpc_enc_tb(struct rte_bbdev_queue_data *q_data,
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
		if (unlikely((avail - cbs_in_tb < 0) || (cbs_in_tb == 0))) {
			acc_enqueue_ring_full(q_data);
			break;
		}

		descs_used = vrb1_enqueue_ldpc_enc_one_op_tb(q, ops[i], enqueued_descs, cbs_in_tb);
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

/* Enqueue encode operations for device. */
static uint16_t
vrb_enqueue_enc(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_enc_op **ops, uint16_t num)
{
	int32_t aq_avail = acc_aq_avail(q_data, num);
	if (unlikely((aq_avail <= 0) || (num == 0)))
		return 0;
	if (ops[0]->turbo_enc.code_block_mode == RTE_BBDEV_TRANSPORT_BLOCK)
		return vrb_enqueue_enc_tb(q_data, ops, num);
	else
		return vrb_enqueue_enc_cb(q_data, ops, num);
}

/* Enqueue encode operations for device. */
static uint16_t
vrb_enqueue_ldpc_enc(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_enc_op **ops, uint16_t num)
{
	int32_t aq_avail = acc_aq_avail(q_data, num);
	if (unlikely((aq_avail <= 0) || (num == 0)))
		return 0;
	if (ops[0]->ldpc_enc.code_block_mode == RTE_BBDEV_TRANSPORT_BLOCK)
		return vrb_enqueue_ldpc_enc_tb(q_data, ops, num);
	else
		return vrb_enqueue_ldpc_enc_cb(q_data, ops, num);
}


/* Enqueue decode operations for device in CB mode. */
static uint16_t
vrb_enqueue_dec_cb(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_dec_op **ops, uint16_t num)
{
	struct acc_queue *q = q_data->queue_private;
	int32_t avail = acc_ring_avail_enq(q);
	uint16_t i;
	int ret;

	for (i = 0; i < num; ++i) {
		/* Check if there are available space for further processing. */
		if (unlikely(avail - 1 < 0))
			break;
		avail -= 1;

		ret = enqueue_dec_one_op_cb(q, ops[i], i);
		if (ret < 0)
			break;
	}

	if (unlikely(i == 0))
		return 0; /* Nothing to enqueue. */

	acc_dma_enqueue(q, i, &q_data->queue_stats);

	/* Update stats. */
	q_data->queue_stats.enqueued_count += i;
	q_data->queue_stats.enqueue_err_count += num - i;

	return i;
}

/* Enqueue decode operations for device in TB mode. */
static uint16_t
vrb_enqueue_ldpc_dec_tb(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_dec_op **ops, uint16_t num)
{
	struct acc_queue *q = q_data->queue_private;
	int32_t avail = acc_ring_avail_enq(q);
	uint16_t i, enqueued_cbs = 0;
	uint8_t cbs_in_tb;
	int ret;

	for (i = 0; i < num; ++i) {
		cbs_in_tb = get_num_cbs_in_tb_ldpc_dec(&ops[i]->ldpc_dec);
		/* Check if there are available space for further processing. */
		if (unlikely((avail - cbs_in_tb < 0) ||
				(cbs_in_tb == 0)))
			break;
		avail -= cbs_in_tb;

		ret = vrb_enqueue_ldpc_dec_one_op_tb(q, ops[i],
				enqueued_cbs, cbs_in_tb);
		if (ret <= 0)
			break;
		enqueued_cbs += ret;
	}

	acc_dma_enqueue(q, enqueued_cbs, &q_data->queue_stats);

	/* Update stats. */
	q_data->queue_stats.enqueued_count += i;
	q_data->queue_stats.enqueue_err_count += num - i;
	return i;
}

/* Enqueue decode operations for device in CB mode. */
static uint16_t
vrb_enqueue_ldpc_dec_cb(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_dec_op **ops, uint16_t num)
{
	struct acc_queue *q = q_data->queue_private;
	int32_t avail = acc_ring_avail_enq(q);
	uint16_t i;
	int ret;
	bool same_op = false;

	for (i = 0; i < num; ++i) {
		/* Check if there are available space for further processing. */
		if (unlikely(avail < 1)) {
			acc_enqueue_ring_full(q_data);
			break;
		}
		avail -= 1;

		rte_bbdev_log(INFO, "Op %d %d %d %d %d %d %d %d %d %d %d %d\n",
			i, ops[i]->ldpc_dec.op_flags, ops[i]->ldpc_dec.rv_index,
			ops[i]->ldpc_dec.iter_max, ops[i]->ldpc_dec.iter_count,
			ops[i]->ldpc_dec.basegraph, ops[i]->ldpc_dec.z_c,
			ops[i]->ldpc_dec.n_cb, ops[i]->ldpc_dec.q_m,
			ops[i]->ldpc_dec.n_filler, ops[i]->ldpc_dec.cb_params.e,
			same_op);
		ret = vrb_enqueue_ldpc_dec_one_op_cb(q, ops[i], i, same_op);
		if (ret < 0) {
			acc_enqueue_invalid(q_data);
			break;
		}
	}

	if (unlikely(i == 0))
		return 0; /* Nothing to enqueue. */

	acc_dma_enqueue(q, i, &q_data->queue_stats);

	/* Update stats. */
	q_data->queue_stats.enqueued_count += i;
	q_data->queue_stats.enqueue_err_count += num - i;
	return i;
}


/* Enqueue decode operations for device in TB mode. */
static uint16_t
vrb_enqueue_dec_tb(struct rte_bbdev_queue_data *q_data,
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
		if (unlikely((avail - cbs_in_tb < 0) || (cbs_in_tb == 0))) {
			acc_enqueue_ring_full(q_data);
			break;
		}
		avail -= cbs_in_tb;

		ret = enqueue_dec_one_op_tb(q, ops[i], enqueued_cbs, cbs_in_tb);
		if (ret <= 0) {
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

/* Enqueue decode operations for device. */
static uint16_t
vrb_enqueue_dec(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_dec_op **ops, uint16_t num)
{
	int32_t aq_avail = acc_aq_avail(q_data, num);
	if (unlikely((aq_avail <= 0) || (num == 0)))
		return 0;
	if (ops[0]->turbo_dec.code_block_mode == RTE_BBDEV_TRANSPORT_BLOCK)
		return vrb_enqueue_dec_tb(q_data, ops, num);
	else
		return vrb_enqueue_dec_cb(q_data, ops, num);
}

/* Enqueue decode operations for device. */
static uint16_t
vrb_enqueue_ldpc_dec(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_dec_op **ops, uint16_t num)
{
	int32_t aq_avail = acc_aq_avail(q_data, num);
	if (unlikely((aq_avail <= 0) || (num == 0)))
		return 0;
	if (ops[0]->ldpc_dec.code_block_mode == RTE_BBDEV_TRANSPORT_BLOCK)
		return vrb_enqueue_ldpc_dec_tb(q_data, ops, num);
	else
		return vrb_enqueue_ldpc_dec_cb(q_data, ops, num);
}


/* Dequeue one encode operations from device in CB mode. */
static inline int
vrb_dequeue_enc_one_op_cb(struct acc_queue *q, struct rte_bbdev_enc_op **ref_op,
		uint16_t *dequeued_ops, uint32_t *aq_dequeued, uint16_t *dequeued_descs,
		uint16_t max_requested_ops)
{
	union acc_dma_desc *desc, atom_desc;
	union acc_dma_rsp_desc rsp;
	struct rte_bbdev_enc_op *op;
	int i;
	struct acc_ptrs *context_ptrs;
	uint16_t desc_idx;

	desc_idx = acc_desc_idx_tail(q, *dequeued_descs);
	desc = q->ring_addr + desc_idx;
	atom_desc.atom_hdr = __atomic_load_n((uint64_t *)desc, __ATOMIC_RELAXED);

	if (*dequeued_ops + desc->req.numCBs > max_requested_ops)
		return -1;

	/* Check fdone bit. */
	if (!(atom_desc.rsp.val & ACC_FDONE))
		return -1;

	rsp.val = atom_desc.rsp.val;
	rte_bbdev_log_debug("Resp. desc %p: %x", desc, rsp.val);

	/* Dequeue. */
	op = desc->req.op_addr;

	/* Clearing status, it will be set based on response. */
	op->status = 0;
	op->status |= ((rsp.input_err) ? (1 << RTE_BBDEV_DATA_ERROR) : 0);
	op->status |= ((rsp.dma_err) ? (1 << RTE_BBDEV_DRV_ERROR) : 0);
	op->status |= ((rsp.fcw_err) ? (1 << RTE_BBDEV_DRV_ERROR) : 0);

	if (desc->req.last_desc_in_batch) {
		(*aq_dequeued)++;
		desc->req.last_desc_in_batch = 0;
	}
	desc->rsp.val = ACC_DMA_DESC_TYPE;
	desc->rsp.add_info_0 = 0; /* Reserved bits. */
	desc->rsp.add_info_1 = 0; /* Reserved bits. */

	ref_op[0] = op;
	context_ptrs = q->companion_ring_addr + desc_idx;
	for (i = 1 ; i < desc->req.numCBs; i++)
		ref_op[i] = context_ptrs->ptr[i].op_addr;

	/* One op was successfully dequeued. */
	(*dequeued_descs)++;
	*dequeued_ops += desc->req.numCBs;
	return desc->req.numCBs;
}

/* Dequeue one LDPC encode operations from device in TB mode.
 * That operation may cover multiple descriptors.
 */
static inline int
vrb_dequeue_enc_one_op_tb(struct acc_queue *q, struct rte_bbdev_enc_op **ref_op,
		uint16_t *dequeued_ops, uint32_t *aq_dequeued,
		uint16_t *dequeued_descs, uint16_t max_requested_ops)
{
	union acc_dma_desc *desc, *last_desc, atom_desc;
	union acc_dma_rsp_desc rsp;
	struct rte_bbdev_enc_op *op;
	uint8_t i = 0;
	uint16_t current_dequeued_descs = 0, descs_in_tb;

	desc = acc_desc_tail(q, *dequeued_descs);
	atom_desc.atom_hdr = __atomic_load_n((uint64_t *)desc, __ATOMIC_RELAXED);

	if (*dequeued_ops + 1 > max_requested_ops)
		return -1;

	/* Check fdone bit. */
	if (!(atom_desc.rsp.val & ACC_FDONE))
		return -1;

	/* Get number of CBs in dequeued TB. */
	descs_in_tb = desc->req.cbs_in_tb;
	/* Get last CB */
	last_desc = acc_desc_tail(q, *dequeued_descs + descs_in_tb - 1);
	/* Check if last CB in TB is ready to dequeue (and thus
	 * the whole TB) - checking sdone bit. If not return.
	 */
	atom_desc.atom_hdr = __atomic_load_n((uint64_t *)last_desc, __ATOMIC_RELAXED);
	if (!(atom_desc.rsp.val & ACC_SDONE))
		return -1;

	/* Dequeue. */
	op = desc->req.op_addr;

	/* Clearing status, it will be set based on response. */
	op->status = 0;

	while (i < descs_in_tb) {
		desc = acc_desc_tail(q, *dequeued_descs);
		atom_desc.atom_hdr = __atomic_load_n((uint64_t *)desc, __ATOMIC_RELAXED);
		rsp.val = atom_desc.rsp.val;
		rte_bbdev_log_debug("Resp. desc %p: %x", desc, rsp.val);

		op->status |= ((rsp.input_err) ? (1 << RTE_BBDEV_DATA_ERROR) : 0);
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

/* Dequeue one decode operation from device in CB mode. */
static inline int
vrb_dequeue_dec_one_op_cb(struct rte_bbdev_queue_data *q_data,
		struct acc_queue *q, struct rte_bbdev_dec_op **ref_op,
		uint16_t dequeued_cbs, uint32_t *aq_dequeued)
{
	union acc_dma_desc *desc, atom_desc;
	union acc_dma_rsp_desc rsp;
	struct rte_bbdev_dec_op *op;

	desc = acc_desc_tail(q, dequeued_cbs);
	atom_desc.atom_hdr = __atomic_load_n((uint64_t *)desc, __ATOMIC_RELAXED);

	/* Check fdone bit. */
	if (!(atom_desc.rsp.val & ACC_FDONE))
		return -1;

	rsp.val = atom_desc.rsp.val;
	rte_bbdev_log_debug("Resp. desc %p: %x\n", desc, rsp.val);

	/* Dequeue. */
	op = desc->req.op_addr;

	/* Clearing status, it will be set based on response. */
	op->status = 0;
	op->status |= ((rsp.input_err) ? (1 << RTE_BBDEV_DATA_ERROR) : 0);
	op->status |= ((rsp.dma_err) ? (1 << RTE_BBDEV_DRV_ERROR) : 0);
	op->status |= ((rsp.fcw_err) ? (1 << RTE_BBDEV_DRV_ERROR) : 0);
	if (op->status != 0) {
		/* These errors are not expected. */
		q_data->queue_stats.dequeue_err_count++;
		vrb_check_ir(q->d);
	}

	/* CRC invalid if error exists. */
	if (!op->status)
		op->status |= rsp.crc_status << RTE_BBDEV_CRC_ERROR;
	op->turbo_dec.iter_count = (uint8_t) rsp.iter_cnt;
	/* Check if this is the last desc in batch (Atomic Queue). */
	if (desc->req.last_desc_in_batch) {
		(*aq_dequeued)++;
		desc->req.last_desc_in_batch = 0;
	}
	desc->rsp.val = ACC_DMA_DESC_TYPE;
	desc->rsp.add_info_0 = 0;
	desc->rsp.add_info_1 = 0;
	*ref_op = op;

	/* One CB (op) was successfully dequeued. */
	return 1;
}

/* Dequeue one decode operations from device in CB mode. */
static inline int
vrb_dequeue_ldpc_dec_one_op_cb(struct rte_bbdev_queue_data *q_data,
		struct acc_queue *q, struct rte_bbdev_dec_op **ref_op,
		uint16_t dequeued_cbs, uint32_t *aq_dequeued)
{
	union acc_dma_desc *desc, atom_desc;
	union acc_dma_rsp_desc rsp;
	struct rte_bbdev_dec_op *op;

	desc = acc_desc_tail(q, dequeued_cbs);
	atom_desc.atom_hdr = __atomic_load_n((uint64_t *)desc, __ATOMIC_RELAXED);

	/* Check fdone bit. */
	if (!(atom_desc.rsp.val & ACC_FDONE))
		return -1;

	rsp.val = atom_desc.rsp.val;
	rte_bbdev_log_debug("Resp. desc %p: %x %x %x\n", desc, rsp.val, desc->rsp.add_info_0,
			desc->rsp.add_info_1);

	/* Dequeue. */
	op = desc->req.op_addr;

	/* Clearing status, it will be set based on response. */
	op->status = 0;
	op->status |= rsp.input_err << RTE_BBDEV_DATA_ERROR;
	op->status |= rsp.dma_err << RTE_BBDEV_DRV_ERROR;
	op->status |= rsp.fcw_err << RTE_BBDEV_DRV_ERROR;
	if (op->status != 0)
		q_data->queue_stats.dequeue_err_count++;

	op->status |= rsp.crc_status << RTE_BBDEV_CRC_ERROR;
	if (op->ldpc_dec.hard_output.length > 0 && !rsp.synd_ok)
		op->status |= 1 << RTE_BBDEV_SYNDROME_ERROR;

	if (check_bit(op->ldpc_dec.op_flags, RTE_BBDEV_LDPC_CRC_TYPE_24A_CHECK)  ||
			check_bit(op->ldpc_dec.op_flags, RTE_BBDEV_LDPC_CRC_TYPE_16_CHECK)) {
		if (desc->rsp.add_info_1 != 0)
			op->status |= 1 << RTE_BBDEV_CRC_ERROR;
	}

	op->ldpc_dec.iter_count = (uint8_t) rsp.iter_cnt;

	if (op->status & (1 << RTE_BBDEV_DRV_ERROR))
		vrb_check_ir(q->d);

	/* Check if this is the last desc in batch (Atomic Queue). */
	if (desc->req.last_desc_in_batch) {
		(*aq_dequeued)++;
		desc->req.last_desc_in_batch = 0;
	}

	desc->rsp.val = ACC_DMA_DESC_TYPE;
	desc->rsp.add_info_0 = 0;
	desc->rsp.add_info_1 = 0;

	*ref_op = op;

	/* One CB (op) was successfully dequeued. */
	return 1;
}

/* Dequeue one decode operations from device in TB mode for 4G or 5G. */
static inline int
vrb_dequeue_dec_one_op_tb(struct acc_queue *q, struct rte_bbdev_dec_op **ref_op,
		uint16_t dequeued_cbs, uint32_t *aq_dequeued)
{
	union acc_dma_desc *desc, *last_desc, atom_desc;
	union acc_dma_rsp_desc rsp;
	struct rte_bbdev_dec_op *op;
	uint8_t cbs_in_tb = 1, cb_idx = 0;
	uint32_t tb_crc_check = 0;

	desc = acc_desc_tail(q, dequeued_cbs);
	atom_desc.atom_hdr = __atomic_load_n((uint64_t *)desc, __ATOMIC_RELAXED);

	/* Check fdone bit. */
	if (!(atom_desc.rsp.val & ACC_FDONE))
		return -1;

	/* Dequeue. */
	op = desc->req.op_addr;

	/* Get number of CBs in dequeued TB. */
	cbs_in_tb = desc->req.cbs_in_tb;
	/* Get last CB. */
	last_desc = acc_desc_tail(q, dequeued_cbs + cbs_in_tb - 1);
	/* Check if last CB in TB is ready to dequeue (and thus the whole TB) - checking sdone bit.
	 * If not return.
	 */
	atom_desc.atom_hdr = __atomic_load_n((uint64_t *)last_desc, __ATOMIC_RELAXED);
	if (!(atom_desc.rsp.val & ACC_SDONE))
		return -1;

	/* Clearing status, it will be set based on response. */
	op->status = 0;

	/* Read remaining CBs if exists. */
	while (cb_idx < cbs_in_tb) {
		desc = acc_desc_tail(q, dequeued_cbs);
		atom_desc.atom_hdr = __atomic_load_n((uint64_t *)desc, __ATOMIC_RELAXED);
		rsp.val = atom_desc.rsp.val;
		rte_bbdev_log_debug("Resp. desc %p: %x %x %x", desc,
				rsp.val, desc->rsp.add_info_0,
				desc->rsp.add_info_1);

		op->status |= ((rsp.input_err) ? (1 << RTE_BBDEV_DATA_ERROR) : 0);
		op->status |= ((rsp.dma_err) ? (1 << RTE_BBDEV_DRV_ERROR) : 0);
		op->status |= ((rsp.fcw_err) ? (1 << RTE_BBDEV_DRV_ERROR) : 0);

		if (check_bit(op->ldpc_dec.op_flags, RTE_BBDEV_LDPC_CRC_TYPE_24A_CHECK))
			tb_crc_check ^= desc->rsp.add_info_1;

		/* CRC invalid if error exists. */
		if (!op->status)
			op->status |= rsp.crc_status << RTE_BBDEV_CRC_ERROR;
		if (q->op_type == RTE_BBDEV_OP_LDPC_DEC)
			op->ldpc_dec.iter_count = RTE_MAX((uint8_t) rsp.iter_cnt,
					op->ldpc_dec.iter_count);
		else
			op->turbo_dec.iter_count = RTE_MAX((uint8_t) rsp.iter_cnt,
					op->turbo_dec.iter_count);

		/* Check if this is the last desc in batch (Atomic Queue). */
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

	if (check_bit(op->ldpc_dec.op_flags, RTE_BBDEV_LDPC_CRC_TYPE_24A_CHECK)) {
		rte_bbdev_log_debug("TB-CRC Check %x\n", tb_crc_check);
		if (tb_crc_check > 0)
			op->status |= 1 << RTE_BBDEV_CRC_ERROR;
	}

	*ref_op = op;

	return cb_idx;
}

/* Dequeue encode operations from device. */
static uint16_t
vrb_dequeue_enc(struct rte_bbdev_queue_data *q_data,
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

	cbm = op->turbo_enc.code_block_mode;

	for (i = 0; i < avail; i++) {
		if (cbm == RTE_BBDEV_TRANSPORT_BLOCK)
			ret = vrb_dequeue_enc_one_op_tb(q, &ops[dequeued_ops],
					&dequeued_ops, &aq_dequeued,
					&dequeued_descs, num);
		else
			ret = vrb_dequeue_enc_one_op_cb(q, &ops[dequeued_ops],
					&dequeued_ops, &aq_dequeued,
					&dequeued_descs, num);
		if (ret < 0)
			break;
	}

	q->aq_dequeued += aq_dequeued;
	q->sw_ring_tail += dequeued_descs;

	/* Update enqueue stats. */
	q_data->queue_stats.dequeued_count += dequeued_ops;

	return dequeued_ops;
}

/* Dequeue LDPC encode operations from device. */
static uint16_t
vrb_dequeue_ldpc_enc(struct rte_bbdev_queue_data *q_data,
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
	cbm = op->ldpc_enc.code_block_mode;

	for (i = 0; i < avail; i++) {
		if (cbm == RTE_BBDEV_TRANSPORT_BLOCK)
			ret = vrb_dequeue_enc_one_op_tb(q, &ops[dequeued_ops],
					&dequeued_ops, &aq_dequeued,
					&dequeued_descs, num);
		else
			ret = vrb_dequeue_enc_one_op_cb(q, &ops[dequeued_ops],
					&dequeued_ops, &aq_dequeued,
					&dequeued_descs, num);
		if (ret < 0)
			break;
	}

	q->aq_dequeued += aq_dequeued;
	q->sw_ring_tail += dequeued_descs;

	/* Update enqueue stats. */
	q_data->queue_stats.dequeued_count += dequeued_ops;

	return dequeued_ops;
}

/* Dequeue decode operations from device. */
static uint16_t
vrb_dequeue_dec(struct rte_bbdev_queue_data *q_data,
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

	dequeue_num = (avail < num) ? avail : num;

	for (i = 0; i < dequeue_num; ++i) {
		op = acc_op_tail(q, dequeued_cbs);
		if (op->turbo_dec.code_block_mode == RTE_BBDEV_TRANSPORT_BLOCK)
			ret = vrb_dequeue_dec_one_op_tb(q, &ops[i], dequeued_cbs,
					&aq_dequeued);
		else
			ret = vrb_dequeue_dec_one_op_cb(q_data, q, &ops[i],
					dequeued_cbs, &aq_dequeued);

		if (ret <= 0)
			break;
		dequeued_cbs += ret;
	}

	q->aq_dequeued += aq_dequeued;
	q->sw_ring_tail += dequeued_cbs;

	/* Update enqueue stats */
	q_data->queue_stats.dequeued_count += i;

	return i;
}

/* Dequeue decode operations from device. */
static uint16_t
vrb_dequeue_ldpc_dec(struct rte_bbdev_queue_data *q_data,
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

	dequeue_num = RTE_MIN(avail, num);

	for (i = 0; i < dequeue_num; ++i) {
		op = acc_op_tail(q, dequeued_cbs);
		if (op->ldpc_dec.code_block_mode == RTE_BBDEV_TRANSPORT_BLOCK)
			ret = vrb_dequeue_dec_one_op_tb(q, &ops[i], dequeued_cbs,
					&aq_dequeued);
		else
			ret = vrb_dequeue_ldpc_dec_one_op_cb(
					q_data, q, &ops[i], dequeued_cbs,
					&aq_dequeued);

		if (ret <= 0)
			break;
		dequeued_cbs += ret;
	}

	q->aq_dequeued += aq_dequeued;
	q->sw_ring_tail += dequeued_cbs;

	/* Update enqueue stats. */
	q_data->queue_stats.dequeued_count += i;

	return i;
}

/* Fill in a frame control word for FFT processing. */
static inline void
vrb1_fcw_fft_fill(struct rte_bbdev_fft_op *op, struct acc_fcw_fft *fcw)
{
	fcw->in_frame_size = op->fft.input_sequence_size;
	fcw->leading_pad_size = op->fft.input_leading_padding;
	fcw->out_frame_size = op->fft.output_sequence_size;
	fcw->leading_depad_size = op->fft.output_leading_depadding;
	fcw->cs_window_sel = op->fft.window_index[0] +
			(op->fft.window_index[1] << 8) +
			(op->fft.window_index[2] << 16) +
			(op->fft.window_index[3] << 24);
	fcw->cs_window_sel2 = op->fft.window_index[4] +
			(op->fft.window_index[5] << 8);
	fcw->cs_enable_bmap = op->fft.cs_bitmap;
	fcw->num_antennas = op->fft.num_antennas_log2;
	fcw->idft_size = op->fft.idft_log2;
	fcw->dft_size = op->fft.dft_log2;
	fcw->cs_offset = op->fft.cs_time_adjustment;
	fcw->idft_shift = op->fft.idft_shift;
	fcw->dft_shift = op->fft.dft_shift;
	fcw->cs_multiplier = op->fft.ncs_reciprocal;
	if (check_bit(op->fft.op_flags, RTE_BBDEV_FFT_IDFT_BYPASS)) {
		if (check_bit(op->fft.op_flags, RTE_BBDEV_FFT_WINDOWING_BYPASS))
			fcw->bypass = 2;
		else
			fcw->bypass = 1;
	} else if (check_bit(op->fft.op_flags, RTE_BBDEV_FFT_DFT_BYPASS))
		fcw->bypass = 3;
	else
		fcw->bypass = 0;
}

static inline int
vrb1_dma_desc_fft_fill(struct rte_bbdev_fft_op *op,
		struct acc_dma_req_desc *desc,
		struct rte_mbuf *input, struct rte_mbuf *output,
		uint32_t *in_offset, uint32_t *out_offset)
{
	/* FCW already done. */
	acc_header_init(desc);
	desc->data_ptrs[1].address = rte_pktmbuf_iova_offset(input, *in_offset);
	desc->data_ptrs[1].blen = op->fft.input_sequence_size * 4;
	desc->data_ptrs[1].blkid = ACC_DMA_BLKID_IN;
	desc->data_ptrs[1].last = 1;
	desc->data_ptrs[1].dma_ext = 0;
	desc->data_ptrs[2].address = rte_pktmbuf_iova_offset(output, *out_offset);
	desc->data_ptrs[2].blen = op->fft.output_sequence_size * 4;
	desc->data_ptrs[2].blkid = ACC_DMA_BLKID_OUT_HARD;
	desc->data_ptrs[2].last = 1;
	desc->data_ptrs[2].dma_ext = 0;
	desc->m2dlen = 2;
	desc->d2mlen = 1;
	desc->ib_ant_offset = op->fft.input_sequence_size;
	desc->num_ant = op->fft.num_antennas_log2 - 3;
	int num_cs = 0, i;
	for (i = 0; i < 12; i++)
		if (check_bit(op->fft.cs_bitmap, 1 << i))
			num_cs++;
	desc->num_cs = num_cs;
	desc->ob_cyc_offset = op->fft.output_sequence_size;
	desc->ob_ant_offset = op->fft.output_sequence_size * num_cs;
	desc->op_addr = op;
	return 0;
}


/** Enqueue one FFT operation for device. */
static inline int
vrb_enqueue_fft_one_op(struct acc_queue *q, struct rte_bbdev_fft_op *op,
		uint16_t total_enqueued_cbs)
{
	union acc_dma_desc *desc;
	struct rte_mbuf *input, *output;
	uint32_t in_offset, out_offset;
	struct acc_fcw_fft *fcw;

	desc = acc_desc(q, total_enqueued_cbs);
	input = op->fft.base_input.data;
	output = op->fft.base_output.data;
	in_offset = op->fft.base_input.offset;
	out_offset = op->fft.base_output.offset;
	fcw = &desc->req.fcw_fft;

	vrb1_fcw_fft_fill(op, fcw);
	vrb1_dma_desc_fft_fill(op, &desc->req, input, output, &in_offset, &out_offset);
#ifdef RTE_LIBRTE_BBDEV_DEBUG
	rte_memdump(stderr, "FCW", &desc->req.fcw_fft,
			sizeof(desc->req.fcw_fft));
	rte_memdump(stderr, "Req Desc.", desc, sizeof(*desc));
#endif
	return 1;
}

/* Enqueue decode operations for device. */
static uint16_t
vrb_enqueue_fft(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_fft_op **ops, uint16_t num)
{
	struct acc_queue *q;
	int32_t aq_avail, avail;
	uint16_t i;
	int ret;

	aq_avail = acc_aq_avail(q_data, num);
	if (unlikely((aq_avail <= 0) || (num == 0)))
		return 0;
	q = q_data->queue_private;
	avail = acc_ring_avail_enq(q);

	for (i = 0; i < num; ++i) {
		/* Check if there are available space for further processing. */
		if (unlikely(avail < 1))
			break;
		avail -= 1;
		ret = vrb_enqueue_fft_one_op(q, ops[i], i);
		if (ret < 0)
			break;
	}

	if (unlikely(i == 0))
		return 0; /* Nothing to enqueue. */

	acc_dma_enqueue(q, i, &q_data->queue_stats);

	/* Update stats */
	q_data->queue_stats.enqueued_count += i;
	q_data->queue_stats.enqueue_err_count += num - i;
	return i;
}


/* Dequeue one FFT operations from device. */
static inline int
vrb_dequeue_fft_one_op(struct rte_bbdev_queue_data *q_data,
		struct acc_queue *q, struct rte_bbdev_fft_op **ref_op,
		uint16_t dequeued_cbs, uint32_t *aq_dequeued)
{
	union acc_dma_desc *desc, atom_desc;
	union acc_dma_rsp_desc rsp;
	struct rte_bbdev_fft_op *op;

	desc = acc_desc_tail(q, dequeued_cbs);
	atom_desc.atom_hdr = __atomic_load_n((uint64_t *)desc, __ATOMIC_RELAXED);

	/* Check fdone bit */
	if (!(atom_desc.rsp.val & ACC_FDONE))
		return -1;

	rsp.val = atom_desc.rsp.val;
#ifdef RTE_LIBRTE_BBDEV_DEBUG
	rte_memdump(stderr, "Resp", &desc->rsp.val,
			sizeof(desc->rsp.val));
#endif
	/* Dequeue. */
	op = desc->req.op_addr;

	/* Clearing status, it will be set based on response. */
	op->status = 0;
	op->status |= rsp.input_err << RTE_BBDEV_DATA_ERROR;
	op->status |= rsp.dma_err << RTE_BBDEV_DRV_ERROR;
	op->status |= rsp.fcw_err << RTE_BBDEV_DRV_ERROR;
	if (op->status != 0)
		q_data->queue_stats.dequeue_err_count++;

	if (op->status & (1 << RTE_BBDEV_DRV_ERROR))
		vrb_check_ir(q->d);

	/* Check if this is the last desc in batch (Atomic Queue). */
	if (desc->req.last_desc_in_batch) {
		(*aq_dequeued)++;
		desc->req.last_desc_in_batch = 0;
	}
	desc->rsp.val = ACC_DMA_DESC_TYPE;
	desc->rsp.add_info_0 = 0;
	*ref_op = op;
	/* One CB (op) was successfully dequeued. */
	return 1;
}


/* Dequeue FFT operations from device. */
static uint16_t
vrb_dequeue_fft(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_fft_op **ops, uint16_t num)
{
	struct acc_queue *q = q_data->queue_private;
	uint16_t dequeue_num, i, dequeued_cbs = 0;
	uint32_t avail = acc_ring_avail_deq(q);
	uint32_t aq_dequeued = 0;
	int ret;

	dequeue_num = RTE_MIN(avail, num);

	for (i = 0; i < dequeue_num; ++i) {
		ret = vrb_dequeue_fft_one_op(q_data, q, &ops[i], dequeued_cbs, &aq_dequeued);
		if (ret <= 0)
			break;
		dequeued_cbs += ret;
	}

	q->aq_dequeued += aq_dequeued;
	q->sw_ring_tail += dequeued_cbs;
	/* Update enqueue stats. */
	q_data->queue_stats.dequeued_count += i;
	return i;
}

/* Initialization Function */
static void
vrb_bbdev_init(struct rte_bbdev *dev, struct rte_pci_driver *drv)
{
	struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI(dev->device);
	struct acc_device *d = dev->data->dev_private;

	dev->dev_ops = &vrb_bbdev_ops;
	dev->enqueue_enc_ops = vrb_enqueue_enc;
	dev->enqueue_dec_ops = vrb_enqueue_dec;
	dev->dequeue_enc_ops = vrb_dequeue_enc;
	dev->dequeue_dec_ops = vrb_dequeue_dec;
	dev->enqueue_ldpc_enc_ops = vrb_enqueue_ldpc_enc;
	dev->enqueue_ldpc_dec_ops = vrb_enqueue_ldpc_dec;
	dev->dequeue_ldpc_enc_ops = vrb_dequeue_ldpc_enc;
	dev->dequeue_ldpc_dec_ops = vrb_dequeue_ldpc_dec;
	dev->enqueue_fft_ops = vrb_enqueue_fft;
	dev->dequeue_fft_ops = vrb_dequeue_fft;

	d->pf_device = !strcmp(drv->driver.name, RTE_STR(VRB_PF_DRIVER_NAME));
	d->mmio_base = pci_dev->mem_resource[0].addr;

	/* Device variant specific handling. */
	if ((pci_dev->id.device_id == RTE_VRB1_PF_DEVICE_ID) ||
			(pci_dev->id.device_id == RTE_VRB1_VF_DEVICE_ID)) {
		d->device_variant = VRB1_VARIANT;
		d->queue_offset = vrb1_queue_offset;
		d->fcw_ld_fill = vrb1_fcw_ld_fill;
		d->num_qgroups = VRB1_NUM_QGRPS;
		d->num_aqs = VRB1_NUM_AQS;
		if (d->pf_device)
			d->reg_addr = &vrb1_pf_reg_addr;
		else
			d->reg_addr = &vrb1_vf_reg_addr;
	}

	rte_bbdev_log_debug("Init device %s [%s] @ vaddr %p paddr %#"PRIx64"",
			drv->driver.name, dev->data->name,
			(void *)pci_dev->mem_resource[0].addr,
			pci_dev->mem_resource[0].phys_addr);
}

static int vrb_pci_probe(struct rte_pci_driver *pci_drv,
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

	/* Invoke device initialization function. */
	vrb_bbdev_init(bbdev, pci_drv);

	rte_bbdev_log_debug("Initialised bbdev %s (id = %u)",
			dev_name, bbdev->data->dev_id);
	return 0;
}

static struct rte_pci_driver vrb_pci_pf_driver = {
		.probe = vrb_pci_probe,
		.remove = acc_pci_remove,
		.id_table = pci_id_vrb_pf_map,
		.drv_flags = RTE_PCI_DRV_NEED_MAPPING
};

static struct rte_pci_driver vrb_pci_vf_driver = {
		.probe = vrb_pci_probe,
		.remove = acc_pci_remove,
		.id_table = pci_id_vrb_vf_map,
		.drv_flags = RTE_PCI_DRV_NEED_MAPPING
};

RTE_PMD_REGISTER_PCI(VRB_PF_DRIVER_NAME, vrb_pci_pf_driver);
RTE_PMD_REGISTER_PCI_TABLE(VRB_PF_DRIVER_NAME, pci_id_vrb_pf_map);
RTE_PMD_REGISTER_PCI(VRB_VF_DRIVER_NAME, vrb_pci_vf_driver);
RTE_PMD_REGISTER_PCI_TABLE(VRB_VF_DRIVER_NAME, pci_id_vrb_vf_map);

/* Initial configuration of a VRB1 device prior to running configure(). */
int
vrb1_configure(const char *dev_name, struct rte_acc_conf *conf)
{
	rte_bbdev_log(INFO, "vrb1_configure");
	uint32_t value, address, status;
	int qg_idx, template_idx, vf_idx, acc, i, rlim, alen, timestamp, totalQgs, numEngines;
	int numQgs, numQqsAcc;
	struct rte_bbdev *bbdev = rte_bbdev_get_named_dev(dev_name);

	/* Compile time checks. */
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

	/* Store configuration. */
	rte_memcpy(&d->acc_conf, conf, sizeof(d->acc_conf));

	/* Check we are already out of PG. */
	status = acc_reg_read(d, VRB1_PfHiSectionPowerGatingAck);
	if (status > 0) {
		if (status != VRB1_PG_MASK_0) {
			rte_bbdev_log(ERR, "Unexpected status %x %x",
					status, VRB1_PG_MASK_0);
			return -ENODEV;
		}
		/* Clock gate sections that will be un-PG. */
		acc_reg_write(d, VRB1_PfHiClkGateHystReg, VRB1_CLK_DIS);
		/* Un-PG required sections. */
		acc_reg_write(d, VRB1_PfHiSectionPowerGatingReq,
				VRB1_PG_MASK_1);
		status = acc_reg_read(d, VRB1_PfHiSectionPowerGatingAck);
		if (status != VRB1_PG_MASK_1) {
			rte_bbdev_log(ERR, "Unexpected status %x %x",
					status, VRB1_PG_MASK_1);
			return -ENODEV;
		}
		acc_reg_write(d, VRB1_PfHiSectionPowerGatingReq,
				VRB1_PG_MASK_2);
		status = acc_reg_read(d, VRB1_PfHiSectionPowerGatingAck);
		if (status != VRB1_PG_MASK_2) {
			rte_bbdev_log(ERR, "Unexpected status %x %x",
					status, VRB1_PG_MASK_2);
			return -ENODEV;
		}
		acc_reg_write(d, VRB1_PfHiSectionPowerGatingReq,
				VRB1_PG_MASK_3);
		status = acc_reg_read(d, VRB1_PfHiSectionPowerGatingAck);
		if (status != VRB1_PG_MASK_3) {
			rte_bbdev_log(ERR, "Unexpected status %x %x",
					status, VRB1_PG_MASK_3);
			return -ENODEV;
		}
		/* Enable clocks for all sections. */
		acc_reg_write(d, VRB1_PfHiClkGateHystReg, VRB1_CLK_EN);
	}

	/* Explicitly releasing AXI as this may be stopped after PF FLR/BME. */
	address = VRB1_PfDmaAxiControl;
	value = 1;
	acc_reg_write(d, address, value);

	/* Set the fabric mode. */
	address = VRB1_PfFabricM2iBufferReg;
	value = VRB1_FABRIC_MODE;
	acc_reg_write(d, address, value);

	/* Set default descriptor signature. */
	address = VRB1_PfDmaDescriptorSignatuture;
	value = 0;
	acc_reg_write(d, address, value);

	/* Enable the Error Detection in DMA. */
	value = VRB1_CFG_DMA_ERROR;
	address = VRB1_PfDmaErrorDetectionEn;
	acc_reg_write(d, address, value);

	/* AXI Cache configuration. */
	value = VRB1_CFG_AXI_CACHE;
	address = VRB1_PfDmaAxcacheReg;
	acc_reg_write(d, address, value);

	/* AXI Response configuration. */
	acc_reg_write(d, VRB1_PfDmaCfgRrespBresp, 0x0);

	/* Default DMA Configuration (Qmgr Enabled). */
	address = VRB1_PfDmaConfig0Reg;
	value = 0;
	acc_reg_write(d, address, value);
	address = VRB1_PfDmaQmanen;
	value = 0;
	acc_reg_write(d, address, value);

	/* Default RLIM/ALEN configuration. */
	rlim = 0;
	alen = 1;
	timestamp = 0;
	address = VRB1_PfDmaConfig1Reg;
	value = (1 << 31) + (rlim << 8) + (timestamp << 6) + alen;
	acc_reg_write(d, address, value);

	/* Default FFT configuration. */
	address = VRB1_PfFftConfig0;
	value = VRB1_FFT_CFG_0;
	acc_reg_write(d, address, value);

	/* Configure DMA Qmanager addresses. */
	address = VRB1_PfDmaQmgrAddrReg;
	value = VRB1_PfQmgrEgressQueuesTemplate;
	acc_reg_write(d, address, value);

	/* ===== Qmgr Configuration ===== */
	/* Configuration of the AQueue Depth QMGR_GRP_0_DEPTH_LOG2 for UL. */
	totalQgs = conf->q_ul_4g.num_qgroups +
			conf->q_ul_5g.num_qgroups +
			conf->q_dl_4g.num_qgroups +
			conf->q_dl_5g.num_qgroups +
			conf->q_fft.num_qgroups;
	for (qg_idx = 0; qg_idx < VRB1_NUM_QGRPS; qg_idx++) {
		address = VRB1_PfQmgrDepthLog2Grp + ACC_BYTES_IN_WORD * qg_idx;
		value = aqDepth(qg_idx, conf);
		acc_reg_write(d, address, value);
		address = VRB1_PfQmgrTholdGrp + ACC_BYTES_IN_WORD * qg_idx;
		value = (1 << 16) + (1 << (aqDepth(qg_idx, conf) - 1));
		acc_reg_write(d, address, value);
	}

	/* Template Priority in incremental order. */
	for (template_idx = 0; template_idx < ACC_NUM_TMPL;
			template_idx++) {
		address = VRB1_PfQmgrGrpTmplateReg0Indx + ACC_BYTES_IN_WORD * template_idx;
		value = ACC_TMPL_PRI_0;
		acc_reg_write(d, address, value);
		address = VRB1_PfQmgrGrpTmplateReg1Indx + ACC_BYTES_IN_WORD * template_idx;
		value = ACC_TMPL_PRI_1;
		acc_reg_write(d, address, value);
		address = VRB1_PfQmgrGrpTmplateReg2indx + ACC_BYTES_IN_WORD * template_idx;
		value = ACC_TMPL_PRI_2;
		acc_reg_write(d, address, value);
		address = VRB1_PfQmgrGrpTmplateReg3Indx + ACC_BYTES_IN_WORD * template_idx;
		value = ACC_TMPL_PRI_3;
		acc_reg_write(d, address, value);
	}

	address = VRB1_PfQmgrGrpPriority;
	value = VRB1_CFG_QMGR_HI_P;
	acc_reg_write(d, address, value);

	/* Template Configuration. */
	for (template_idx = 0; template_idx < ACC_NUM_TMPL;
			template_idx++) {
		value = 0;
		address = VRB1_PfQmgrGrpTmplateReg4Indx
				+ ACC_BYTES_IN_WORD * template_idx;
		acc_reg_write(d, address, value);
	}
	/* 4GUL */
	numQgs = conf->q_ul_4g.num_qgroups;
	numQqsAcc = 0;
	value = 0;
	for (qg_idx = numQqsAcc; qg_idx < (numQgs + numQqsAcc); qg_idx++)
		value |= (1 << qg_idx);
	for (template_idx = VRB1_SIG_UL_4G;
			template_idx <= VRB1_SIG_UL_4G_LAST;
			template_idx++) {
		address = VRB1_PfQmgrGrpTmplateReg4Indx
				+ ACC_BYTES_IN_WORD * template_idx;
		acc_reg_write(d, address, value);
	}
	/* 5GUL */
	numQqsAcc += numQgs;
	numQgs = conf->q_ul_5g.num_qgroups;
	value = 0;
	numEngines = 0;
	for (qg_idx = numQqsAcc; qg_idx < (numQgs + numQqsAcc); qg_idx++)
		value |= (1 << qg_idx);
	for (template_idx = VRB1_SIG_UL_5G;
			template_idx <= VRB1_SIG_UL_5G_LAST;
			template_idx++) {
		/* Check engine power-on status */
		address = VRB1_PfFecUl5gIbDebugReg + ACC_ENGINE_OFFSET * template_idx;
		status = (acc_reg_read(d, address) >> 4) & 0x7;
		address = VRB1_PfQmgrGrpTmplateReg4Indx
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
	for (template_idx = VRB1_SIG_DL_4G;
			template_idx <= VRB1_SIG_DL_4G_LAST;
			template_idx++) {
		address = VRB1_PfQmgrGrpTmplateReg4Indx
				+ ACC_BYTES_IN_WORD * template_idx;
		acc_reg_write(d, address, value);
	}
	/* 5GDL */
	numQqsAcc += numQgs;
	numQgs	= conf->q_dl_5g.num_qgroups;
	value = 0;
	for (qg_idx = numQqsAcc; qg_idx < (numQgs + numQqsAcc); qg_idx++)
		value |= (1 << qg_idx);
	for (template_idx = VRB1_SIG_DL_5G;
			template_idx <= VRB1_SIG_DL_5G_LAST;
			template_idx++) {
		address = VRB1_PfQmgrGrpTmplateReg4Indx
				+ ACC_BYTES_IN_WORD * template_idx;
		acc_reg_write(d, address, value);
	}
	/* FFT */
	numQqsAcc += numQgs;
	numQgs	= conf->q_fft.num_qgroups;
	value = 0;
	for (qg_idx = numQqsAcc; qg_idx < (numQgs + numQqsAcc); qg_idx++)
		value |= (1 << qg_idx);
	for (template_idx = VRB1_SIG_FFT;
			template_idx <= VRB1_SIG_FFT_LAST;
			template_idx++) {
		address = VRB1_PfQmgrGrpTmplateReg4Indx
				+ ACC_BYTES_IN_WORD * template_idx;
		acc_reg_write(d, address, value);
	}

	/* Queue Group Function mapping. */
	int qman_func_id[8] = {0, 2, 1, 3, 4, 0, 0, 0};
	value = 0;
	for (qg_idx = 0; qg_idx < ACC_NUM_QGRPS_PER_WORD; qg_idx++) {
		acc = accFromQgid(qg_idx, conf);
		value |= qman_func_id[acc] << (qg_idx * 4);
	}
	acc_reg_write(d, VRB1_PfQmgrGrpFunction0, value);
	value = 0;
	for (qg_idx = 0; qg_idx < ACC_NUM_QGRPS_PER_WORD; qg_idx++) {
		acc = accFromQgid(qg_idx + ACC_NUM_QGRPS_PER_WORD, conf);
		value |= qman_func_id[acc] << (qg_idx * 4);
	}
	acc_reg_write(d, VRB1_PfQmgrGrpFunction1, value);

	/* Configuration of the Arbitration QGroup depth to 1. */
	for (qg_idx = 0; qg_idx < VRB1_NUM_QGRPS; qg_idx++) {
		address = VRB1_PfQmgrArbQDepthGrp +
				ACC_BYTES_IN_WORD * qg_idx;
		value = 0;
		acc_reg_write(d, address, value);
	}

	/* This pointer to ARAM (256kB) is shifted by 2 (4B per register). */
	uint32_t aram_address = 0;
	for (qg_idx = 0; qg_idx < totalQgs; qg_idx++) {
		for (vf_idx = 0; vf_idx < conf->num_vf_bundles; vf_idx++) {
			address = VRB1_PfQmgrVfBaseAddr + vf_idx
					* ACC_BYTES_IN_WORD + qg_idx
					* ACC_BYTES_IN_WORD * 64;
			value = aram_address;
			acc_reg_write(d, address, value);
			/* Offset ARAM Address for next memory bank - increment of 4B. */
			aram_address += aqNum(qg_idx, conf) *
					(1 << aqDepth(qg_idx, conf));
		}
	}

	if (aram_address > VRB1_WORDS_IN_ARAM_SIZE) {
		rte_bbdev_log(ERR, "ARAM Configuration not fitting %d %d\n",
				aram_address, VRB1_WORDS_IN_ARAM_SIZE);
		return -EINVAL;
	}

	/* Performance tuning. */
	acc_reg_write(d, VRB1_PfFabricI2Mdma_weight, 0x0FFF);
	acc_reg_write(d, VRB1_PfDma4gdlIbThld, 0x1f10);

	/* ==== HI Configuration ==== */

	/* No Info Ring/MSI by default. */
	address = VRB1_PfHiInfoRingIntWrEnRegPf;
	value = 0;
	acc_reg_write(d, address, value);
	address = VRB1_PfHiCfgMsiIntWrEnRegPf;
	value = 0xFFFFFFFF;
	acc_reg_write(d, address, value);
	/* Prevent Block on Transmit Error. */
	address = VRB1_PfHiBlockTransmitOnErrorEn;
	value = 0;
	acc_reg_write(d, address, value);
	/* Prevents to drop MSI. */
	address = VRB1_PfHiMsiDropEnableReg;
	value = 0;
	acc_reg_write(d, address, value);
	/* Set the PF Mode register. */
	address = VRB1_PfHiPfMode;
	value = (conf->pf_mode_en) ? ACC_PF_VAL : 0;
	acc_reg_write(d, address, value);

	/* QoS overflow init. */
	value = 1;
	address = VRB1_PfQosmonAEvalOverflow0;
	acc_reg_write(d, address, value);
	address = VRB1_PfQosmonBEvalOverflow0;
	acc_reg_write(d, address, value);

	/* Configure the FFT RAM LUT. */
	uint32_t fft_lut[VRB1_FFT_RAM_SIZE] = {
	0x1FFFF, 0x1FFFF, 0x1FFFE, 0x1FFFA, 0x1FFF6, 0x1FFF1, 0x1FFEA, 0x1FFE2,
	0x1FFD9, 0x1FFCE, 0x1FFC2, 0x1FFB5, 0x1FFA7, 0x1FF98, 0x1FF87, 0x1FF75,
	0x1FF62, 0x1FF4E, 0x1FF38, 0x1FF21, 0x1FF09, 0x1FEF0, 0x1FED6, 0x1FEBA,
	0x1FE9D, 0x1FE7F, 0x1FE5F, 0x1FE3F, 0x1FE1D, 0x1FDFA, 0x1FDD5, 0x1FDB0,
	0x1FD89, 0x1FD61, 0x1FD38, 0x1FD0D, 0x1FCE1, 0x1FCB4, 0x1FC86, 0x1FC57,
	0x1FC26, 0x1FBF4, 0x1FBC1, 0x1FB8D, 0x1FB58, 0x1FB21, 0x1FAE9, 0x1FAB0,
	0x1FA75, 0x1FA3A, 0x1F9FD, 0x1F9BF, 0x1F980, 0x1F93F, 0x1F8FD, 0x1F8BA,
	0x1F876, 0x1F831, 0x1F7EA, 0x1F7A3, 0x1F75A, 0x1F70F, 0x1F6C4, 0x1F677,
	0x1F629, 0x1F5DA, 0x1F58A, 0x1F539, 0x1F4E6, 0x1F492, 0x1F43D, 0x1F3E7,
	0x1F38F, 0x1F337, 0x1F2DD, 0x1F281, 0x1F225, 0x1F1C8, 0x1F169, 0x1F109,
	0x1F0A8, 0x1F046, 0x1EFE2, 0x1EF7D, 0x1EF18, 0x1EEB0, 0x1EE48, 0x1EDDF,
	0x1ED74, 0x1ED08, 0x1EC9B, 0x1EC2D, 0x1EBBE, 0x1EB4D, 0x1EADB, 0x1EA68,
	0x1E9F4, 0x1E97F, 0x1E908, 0x1E891, 0x1E818, 0x1E79E, 0x1E722, 0x1E6A6,
	0x1E629, 0x1E5AA, 0x1E52A, 0x1E4A9, 0x1E427, 0x1E3A3, 0x1E31F, 0x1E299,
	0x1E212, 0x1E18A, 0x1E101, 0x1E076, 0x1DFEB, 0x1DF5E, 0x1DED0, 0x1DE41,
	0x1DDB1, 0x1DD20, 0x1DC8D, 0x1DBFA, 0x1DB65, 0x1DACF, 0x1DA38, 0x1D9A0,
	0x1D907, 0x1D86C, 0x1D7D1, 0x1D734, 0x1D696, 0x1D5F7, 0x1D557, 0x1D4B6,
	0x1D413, 0x1D370, 0x1D2CB, 0x1D225, 0x1D17E, 0x1D0D6, 0x1D02D, 0x1CF83,
	0x1CED8, 0x1CE2B, 0x1CD7E, 0x1CCCF, 0x1CC1F, 0x1CB6E, 0x1CABC, 0x1CA09,
	0x1C955, 0x1C89F, 0x1C7E9, 0x1C731, 0x1C679, 0x1C5BF, 0x1C504, 0x1C448,
	0x1C38B, 0x1C2CD, 0x1C20E, 0x1C14E, 0x1C08C, 0x1BFCA, 0x1BF06, 0x1BE42,
	0x1BD7C, 0x1BCB5, 0x1BBED, 0x1BB25, 0x1BA5B, 0x1B990, 0x1B8C4, 0x1B7F6,
	0x1B728, 0x1B659, 0x1B589, 0x1B4B7, 0x1B3E5, 0x1B311, 0x1B23D, 0x1B167,
	0x1B091, 0x1AFB9, 0x1AEE0, 0x1AE07, 0x1AD2C, 0x1AC50, 0x1AB73, 0x1AA95,
	0x1A9B6, 0x1A8D6, 0x1A7F6, 0x1A714, 0x1A631, 0x1A54D, 0x1A468, 0x1A382,
	0x1A29A, 0x1A1B2, 0x1A0C9, 0x19FDF, 0x19EF4, 0x19E08, 0x19D1B, 0x19C2D,
	0x19B3E, 0x19A4E, 0x1995D, 0x1986B, 0x19778, 0x19684, 0x1958F, 0x19499,
	0x193A2, 0x192AA, 0x191B1, 0x190B8, 0x18FBD, 0x18EC1, 0x18DC4, 0x18CC7,
	0x18BC8, 0x18AC8, 0x189C8, 0x188C6, 0x187C4, 0x186C1, 0x185BC, 0x184B7,
	0x183B1, 0x182AA, 0x181A2, 0x18099, 0x17F8F, 0x17E84, 0x17D78, 0x17C6C,
	0x17B5E, 0x17A4F, 0x17940, 0x17830, 0x1771E, 0x1760C, 0x174F9, 0x173E5,
	0x172D1, 0x171BB, 0x170A4, 0x16F8D, 0x16E74, 0x16D5B, 0x16C41, 0x16B26,
	0x16A0A, 0x168ED, 0x167CF, 0x166B1, 0x16592, 0x16471, 0x16350, 0x1622E,
	0x1610B, 0x15FE8, 0x15EC3, 0x15D9E, 0x15C78, 0x15B51, 0x15A29, 0x15900,
	0x157D7, 0x156AC, 0x15581, 0x15455, 0x15328, 0x151FB, 0x150CC, 0x14F9D,
	0x14E6D, 0x14D3C, 0x14C0A, 0x14AD8, 0x149A4, 0x14870, 0x1473B, 0x14606,
	0x144CF, 0x14398, 0x14260, 0x14127, 0x13FEE, 0x13EB3, 0x13D78, 0x13C3C,
	0x13B00, 0x139C2, 0x13884, 0x13745, 0x13606, 0x134C5, 0x13384, 0x13242,
	0x130FF, 0x12FBC, 0x12E78, 0x12D33, 0x12BEE, 0x12AA7, 0x12960, 0x12819,
	0x126D0, 0x12587, 0x1243D, 0x122F3, 0x121A8, 0x1205C, 0x11F0F, 0x11DC2,
	0x11C74, 0x11B25, 0x119D6, 0x11886, 0x11735, 0x115E3, 0x11491, 0x1133F,
	0x111EB, 0x11097, 0x10F42, 0x10DED, 0x10C97, 0x10B40, 0x109E9, 0x10891,
	0x10738, 0x105DF, 0x10485, 0x1032B, 0x101D0, 0x10074, 0x0FF18, 0x0FDBB,
	0x0FC5D, 0x0FAFF, 0x0F9A0, 0x0F841, 0x0F6E1, 0x0F580, 0x0F41F, 0x0F2BD,
	0x0F15B, 0x0EFF8, 0x0EE94, 0x0ED30, 0x0EBCC, 0x0EA67, 0x0E901, 0x0E79A,
	0x0E633, 0x0E4CC, 0x0E364, 0x0E1FB, 0x0E092, 0x0DF29, 0x0DDBE, 0x0DC54,
	0x0DAE9, 0x0D97D, 0x0D810, 0x0D6A4, 0x0D536, 0x0D3C8, 0x0D25A, 0x0D0EB,
	0x0CF7C, 0x0CE0C, 0x0CC9C, 0x0CB2B, 0x0C9B9, 0x0C847, 0x0C6D5, 0x0C562,
	0x0C3EF, 0x0C27B, 0x0C107, 0x0BF92, 0x0BE1D, 0x0BCA8, 0x0BB32, 0x0B9BB,
	0x0B844, 0x0B6CD, 0x0B555, 0x0B3DD, 0x0B264, 0x0B0EB, 0x0AF71, 0x0ADF7,
	0x0AC7D, 0x0AB02, 0x0A987, 0x0A80B, 0x0A68F, 0x0A513, 0x0A396, 0x0A219,
	0x0A09B, 0x09F1D, 0x09D9E, 0x09C20, 0x09AA1, 0x09921, 0x097A1, 0x09621,
	0x094A0, 0x0931F, 0x0919E, 0x0901C, 0x08E9A, 0x08D18, 0x08B95, 0x08A12,
	0x0888F, 0x0870B, 0x08587, 0x08402, 0x0827E, 0x080F9, 0x07F73, 0x07DEE,
	0x07C68, 0x07AE2, 0x0795B, 0x077D4, 0x0764D, 0x074C6, 0x0733E, 0x071B6,
	0x0702E, 0x06EA6, 0x06D1D, 0x06B94, 0x06A0B, 0x06881, 0x066F7, 0x0656D,
	0x063E3, 0x06258, 0x060CE, 0x05F43, 0x05DB7, 0x05C2C, 0x05AA0, 0x05914,
	0x05788, 0x055FC, 0x0546F, 0x052E3, 0x05156, 0x04FC9, 0x04E3B, 0x04CAE,
	0x04B20, 0x04992, 0x04804, 0x04676, 0x044E8, 0x04359, 0x041CB, 0x0403C,
	0x03EAD, 0x03D1D, 0x03B8E, 0x039FF, 0x0386F, 0x036DF, 0x0354F, 0x033BF,
	0x0322F, 0x0309F, 0x02F0F, 0x02D7E, 0x02BEE, 0x02A5D, 0x028CC, 0x0273B,
	0x025AA, 0x02419, 0x02288, 0x020F7, 0x01F65, 0x01DD4, 0x01C43, 0x01AB1,
	0x0191F, 0x0178E, 0x015FC, 0x0146A, 0x012D8, 0x01147, 0x00FB5, 0x00E23,
	0x00C91, 0x00AFF, 0x0096D, 0x007DB, 0x00648, 0x004B6, 0x00324, 0x00192};

	acc_reg_write(d, VRB1_PfFftRamPageAccess, VRB1_FFT_RAM_EN + 64);
	for (i = 0; i < VRB1_FFT_RAM_SIZE; i++)
		acc_reg_write(d, VRB1_PfFftRamOff + i * 4, fft_lut[i]);
	acc_reg_write(d, VRB1_PfFftRamPageAccess, VRB1_FFT_RAM_DIS);

	/* Enabling AQueues through the Queue hierarchy. */
	for (vf_idx = 0; vf_idx < VRB1_NUM_VFS; vf_idx++) {
		for (qg_idx = 0; qg_idx < VRB1_NUM_QGRPS; qg_idx++) {
			value = 0;
			if (vf_idx < conf->num_vf_bundles && qg_idx < totalQgs)
				value = (1 << aqNum(qg_idx, conf)) - 1;
			address = VRB1_PfQmgrAqEnableVf + vf_idx * ACC_BYTES_IN_WORD;
			value += (qg_idx << 16);
			acc_reg_write(d, address, value);
		}
	}

	rte_bbdev_log_debug("PF Tip configuration complete for %s", dev_name);
	return 0;
}
