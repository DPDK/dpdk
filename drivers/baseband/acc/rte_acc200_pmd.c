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

/* Allocate 64MB memory used for all software rings. */
static int
acc200_setup_queues(struct rte_bbdev *dev, uint16_t num_queues, int socket_id)
{
	uint32_t phys_low, phys_high, value;
	struct acc_device *d = dev->data->dev_private;
	const struct acc200_registry_addr *reg_addr;
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

	/* Configure ACC200 with the base address for DMA descriptor rings.
	 * Same descriptor rings used for UL and DL DMA Engines.
	 * Note : Assuming only VF0 bundle is used for PF mode.
	 */
	phys_high = (uint32_t)(d->sw_rings_iova >> 32);
	phys_low  = (uint32_t)(d->sw_rings_iova & ~(ACC_SIZE_64MBYTE-1));

	/* Choose correct registry addresses for the device type. */
	if (d->pf_device)
		reg_addr = &pf_reg_addr;
	else
		reg_addr = &vf_reg_addr;

	/* Read the populated cfg from ACC200 registers. */
	fetch_acc200_config(dev);

	/* Start Pmon */
	for (value = 0; value <= 2; value++) {
		acc_reg_write(d, reg_addr->pmon_ctrl_a, value);
		acc_reg_write(d, reg_addr->pmon_ctrl_b, value);
		acc_reg_write(d, reg_addr->pmon_ctrl_c, value);
	}

	/* Release AXI from PF. */
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
	acc_reg_write(d, reg_addr->dma_ring_fft_hi, phys_high);
	acc_reg_write(d, reg_addr->dma_ring_fft_lo, phys_low);
	/*
	 * Configure Ring Size to the max queue ring size
	 * (used for wrapping purpose).
	 */
	value = log2_basic(d->sw_ring_size / ACC_RING_SIZE_GRANULARITY);
	acc_reg_write(d, reg_addr->ring_size, value);

	/* Configure tail pointer for use when SDONE enabled. */
	if (d->tail_ptrs == NULL)
		d->tail_ptrs = rte_zmalloc_socket(
				dev->device->driver->name,
				ACC200_NUM_QGRPS * ACC200_NUM_AQS * sizeof(uint32_t),
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
	acc_reg_write(d, reg_addr->tail_ptrs_fft_hi, phys_high);
	acc_reg_write(d, reg_addr->tail_ptrs_fft_lo, phys_low);

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
			"ACC200 (%s) configured  sw_rings = %p, sw_rings_iova = %#"
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

/* Free memory used for software rings. */
static int
acc200_dev_close(struct rte_bbdev *dev)
{
	struct acc_device *d = dev->data->dev_private;
	if (d->sw_rings_base != NULL) {
		rte_free(d->tail_ptrs);
		rte_free(d->sw_rings_base);
		rte_free(d->harq_layout);
		d->sw_rings_base = NULL;
		d->tail_ptrs = NULL;
		d->harq_layout = NULL;
	}
	/* Ensure all in flight HW transactions are completed. */
	usleep(ACC_LONG_WAIT);
	return 0;
}

/**
 * Report a ACC200 queue index which is free.
 * Return 0 to 16k for a valid queue_idx or -1 when no queue is available.
 * Note : Only supporting VF0 Bundle for PF mode.
 */
static int
acc200_find_free_queue_idx(struct rte_bbdev *dev,
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
	if (group_idx >= ACC200_NUM_QGRPS ||
			conf->priority >= qtop->num_qgroups) {
		rte_bbdev_log(INFO, "Invalid Priority on %s, priority %u",
				dev->data->name, conf->priority);
		return -1;
	}
	/* Find a free AQ_idx.  */
	for (aq_idx = 0; aq_idx < qtop->num_aqs_per_groups; aq_idx++) {
		if (((d->q_assigned_bit_map[group_idx] >> aq_idx) & 0x1) == 0) {
			/* Mark the Queue as assigned. */
			d->q_assigned_bit_map[group_idx] |= (1 << aq_idx);
			/* Report the AQ Index. */
			return (group_idx << ACC200_GRP_ID_SHIFT) + aq_idx;
		}
	}
	rte_bbdev_log(INFO, "Failed to find free queue on %s, priority %u",
			dev->data->name, conf->priority);
	return -1;
}

/* Setup ACC200 queue. */
static int
acc200_queue_setup(struct rte_bbdev *dev, uint16_t queue_id,
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

	q_idx = acc200_find_free_queue_idx(dev, conf);
	if (q_idx == -1) {
		ret = -EINVAL;
		goto free_companion_ring_addr;
	}

	q->qgrp_id = (q_idx >> ACC200_GRP_ID_SHIFT) & 0xF;
	q->vf_id = (q_idx >> ACC200_VF_ID_SHIFT)  & 0x3F;
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
			queue_offset(d->pf_device,
					q->vf_id, q->qgrp_id, q->aq_id));

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
acc200_print_op(struct rte_bbdev_dec_op *op, enum rte_bbdev_op_type op_type,
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

/* Stop ACC200 queue and clear counters. */
static int
acc200_queue_stop(struct rte_bbdev *dev, uint16_t queue_id)
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
		acc200_print_op(op, q->op_type, i);
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

/* Release ACC200 queue. */
static int
acc200_queue_release(struct rte_bbdev *dev, uint16_t q_id)
{
	struct acc_device *d = dev->data->dev_private;
	struct acc_queue *q = dev->data->queues[q_id].queue_private;

	if (q != NULL) {
		/* Mark the Queue as un-assigned. */
		d->q_assigned_bit_map[q->qgrp_id] &= (~0ULL - (uint64_t) (1 << q->aq_id));
		rte_free(q->companion_ring_addr);
		rte_free(q->lb_in);
		rte_free(q->lb_out);
		rte_free(q);
		dev->data->queues[q_id].queue_private = NULL;
	}

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
		{
			.type   = RTE_BBDEV_OP_LDPC_ENC,
			.cap.ldpc_enc = {
				.capability_flags =
					RTE_BBDEV_LDPC_RATE_MATCH |
					RTE_BBDEV_LDPC_CRC_24B_ATTACH |
					RTE_BBDEV_LDPC_INTERLEAVER_BYPASS,
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
				RTE_BBDEV_LDPC_LLR_COMPRESSION,
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

	/* Read and save the populated config from ACC200 registers. */
	fetch_acc200_config(dev);

	/* Exposed number of queues. */
	dev_info->num_queues[RTE_BBDEV_OP_NONE] = 0;
	dev_info->num_queues[RTE_BBDEV_OP_TURBO_DEC] = 0;
	dev_info->num_queues[RTE_BBDEV_OP_TURBO_ENC] = 0;
	dev_info->num_queues[RTE_BBDEV_OP_LDPC_DEC] = d->acc_conf.q_ul_5g.num_aqs_per_groups *
			d->acc_conf.q_ul_5g.num_qgroups;
	dev_info->num_queues[RTE_BBDEV_OP_LDPC_ENC] = d->acc_conf.q_dl_5g.num_aqs_per_groups *
			d->acc_conf.q_dl_5g.num_qgroups;
	dev_info->num_queues[RTE_BBDEV_OP_FFT] = 0;
	dev_info->queue_priority[RTE_BBDEV_OP_TURBO_DEC] = 0;
	dev_info->queue_priority[RTE_BBDEV_OP_TURBO_ENC] = 0;
	dev_info->queue_priority[RTE_BBDEV_OP_LDPC_DEC] = d->acc_conf.q_ul_5g.num_qgroups;
	dev_info->queue_priority[RTE_BBDEV_OP_LDPC_ENC] = d->acc_conf.q_dl_5g.num_qgroups;
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
	.setup_queues = acc200_setup_queues,
	.close = acc200_dev_close,
	.info_get = acc200_dev_info_get,
	.queue_setup = acc200_queue_setup,
	.queue_release = acc200_queue_release,
	.queue_stop = acc200_queue_stop,
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

/* Fill in a frame control word for LDPC decoding. */
static inline void
acc200_fcw_ld_fill(struct rte_bbdev_dec_op *op, struct acc_fcw_ld *fcw,
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
acc200_dma_desc_ld_fill(struct rte_bbdev_dec_op *op, struct acc_dma_req_desc *desc,
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
acc200_dma_desc_ld_update(struct rte_bbdev_dec_op *op,
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

/* Enqueue one encode operations for ACC200 device in CB mode
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

	uint16_t desc_idx = ((q->sw_ring_head + total_enqueued_descs)
			& q->sw_ring_wrap_mask);
	desc = q->ring_addr + desc_idx;
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
	struct acc_ptrs *context_ptrs = q->companion_ring_addr + desc_idx;
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

/* Enqueue one encode operations for ACC200 device for a partial TB
 * all codes blocks have same configuration multiplexed on the same descriptor.
 */
static inline void
enqueue_ldpc_enc_part_tb(struct acc_queue *q, struct rte_bbdev_enc_op *op,
		uint16_t total_enqueued_descs, int16_t num_cbs, uint32_t e,
		uint16_t in_len_B, uint32_t out_len_B, uint32_t *in_offset,
		uint32_t *out_offset)
{

	union acc_dma_desc *desc = NULL;
	struct rte_mbuf *output_head, *output;
	int i, next_triplet;
	struct rte_bbdev_op_ldpc_enc *enc = &op->ldpc_enc;


	uint16_t desc_idx = ((q->sw_ring_head + total_enqueued_descs) & q->sw_ring_wrap_mask);
	desc = q->ring_addr + desc_idx;
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

/* Enqueue one encode operations for ACC200 device in TB mode.
 * returns the number of descs used.
 */
static inline int
enqueue_ldpc_enc_one_op_tb(struct acc_queue *q, struct rte_bbdev_enc_op *op,
		uint16_t enq_descs, uint8_t cbs_in_tb)
{
	uint8_t num_a, num_b;
	uint16_t desc_idx, input_len_B, return_descs;
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
		enqueue_ldpc_enc_part_tb(q, op, enq_descs, enq, e, input_len_B,
				out_len_B, &in_offset, &out_offset);
		enq_descs++;
	}
	while (num_b > 0) {
		uint32_t e = op->ldpc_enc.tb_params.eb;
		uint32_t out_len_B = (e + 7) >> 3;
		uint8_t enq = RTE_MIN(num_b, ACC_MUX_5GDL_DESC);
		num_b -= enq;
		enqueue_ldpc_enc_part_tb(q, op, enq_descs, enq, e, input_len_B,
				out_len_B, &in_offset, &out_offset);
		enq_descs++;
	}

	return_descs = enq_descs - init_enq_descs;
	/* Keep total number of CBs in first TB. */
	desc_idx = ((q->sw_ring_head + init_enq_descs)
			& q->sw_ring_wrap_mask);
	desc = q->ring_addr + desc_idx;
	desc->req.cbs_in_tb = return_descs; /** Actual number of descriptors. */
	desc->req.op_addr = op;

	/* Set SDone on last CB descriptor for TB mode. */
	desc_idx = ((q->sw_ring_head + enq_descs - 1)
			& q->sw_ring_wrap_mask);
	desc = q->ring_addr + desc_idx;
	desc->req.sdone_enable = 1;
	desc->req.irq_enable = q->irq_enable;
	desc->req.op_addr = op;
	return return_descs;
}

/** Enqueue one decode operations for ACC200 device in CB mode. */
static inline int
enqueue_ldpc_dec_one_op_cb(struct acc_queue *q, struct rte_bbdev_dec_op *op,
		uint16_t total_enqueued_cbs, bool same_op)
{
	int ret, hq_len;
	union acc_dma_desc *desc;
	uint16_t desc_idx;
	struct rte_mbuf *input, *h_output_head, *h_output;
	uint32_t in_offset, h_out_offset, mbuf_total_left, h_out_length = 0;
	union acc_harq_layout_data *harq_layout;

	if (op->ldpc_dec.cb_params.e == 0)
		return -EINVAL;

	desc_idx = ((q->sw_ring_head + total_enqueued_cbs) & q->sw_ring_wrap_mask);
	desc = q->ring_addr + desc_idx;

	input = op->ldpc_dec.input.data;
	h_output_head = h_output = op->ldpc_dec.hard_output.data;
	in_offset = op->ldpc_dec.input.offset;
	h_out_offset = op->ldpc_dec.hard_output.offset;
	mbuf_total_left = op->ldpc_dec.input.length;
	harq_layout = q->d->harq_layout;

	if (same_op) {
		union acc_dma_desc *prev_desc;
		desc_idx = ((q->sw_ring_head + total_enqueued_cbs - 1) & q->sw_ring_wrap_mask);
		prev_desc = q->ring_addr + desc_idx;
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
		acc200_dma_desc_ld_update(op, &desc->req, input, h_output,
				&in_offset, &h_out_offset,
				&h_out_length, harq_layout);
	} else {
		struct acc_fcw_ld *fcw;
		uint32_t seg_total_left;
		fcw = &desc->req.fcw_ld;
		acc200_fcw_ld_fill(op, fcw, harq_layout);

		/* Special handling when using mbuf or not. */
		if (check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_DEC_SCATTER_GATHER))
			seg_total_left = rte_pktmbuf_data_len(input) - in_offset;
		else
			seg_total_left = fcw->rm_e;

		ret = acc200_dma_desc_ld_fill(op, &desc->req, &input, h_output,
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

#ifdef RTE_LIBRTE_BBDEV_DEBUG
	rte_memdump(stderr, "FCW", &desc->req.fcw_ld,
			sizeof(desc->req.fcw_ld) - 8);
	rte_memdump(stderr, "Req Desc.", desc, sizeof(*desc));
#endif

	/* One CB (one op) was successfully prepared to enqueue. */
	return 1;
}


/* Enqueue one decode operations for ACC200 device in TB mode. */
static inline int
enqueue_ldpc_dec_one_op_tb(struct acc_queue *q, struct rte_bbdev_dec_op *op,
		uint16_t total_enqueued_cbs, uint8_t cbs_in_tb)
{
	union acc_dma_desc *desc = NULL;
	union acc_dma_desc *desc_first = NULL;
	int ret;
	uint8_t r, c;
	uint32_t in_offset, h_out_offset, h_out_length, mbuf_total_left, seg_total_left;
	struct rte_mbuf *input, *h_output_head, *h_output;
	uint16_t current_enqueued_cbs = 0;
	uint16_t sys_cols, trail_len = 0;

	uint16_t desc_idx = ((q->sw_ring_head + total_enqueued_cbs) & q->sw_ring_wrap_mask);
	desc = q->ring_addr + desc_idx;
	desc_first = desc;
	uint64_t fcw_offset = (desc_idx << 8) + ACC_DESC_FCW_OFFSET;
	union acc_harq_layout_data *harq_layout = q->d->harq_layout;
	acc200_fcw_ld_fill(op, &desc->req.fcw_ld, harq_layout);

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
		if (check_bit(op->ldpc_dec.op_flags, RTE_BBDEV_LDPC_DEC_SCATTER_GATHER))
			seg_total_left = rte_pktmbuf_data_len(input) - in_offset;
		else
			seg_total_left = op->ldpc_dec.input.length;
		/* Set up DMA descriptor. */
		desc_idx = ((q->sw_ring_head + total_enqueued_cbs) & q->sw_ring_wrap_mask);
		desc = q->ring_addr + desc_idx;
		fcw_offset = (desc_idx << 8) + ACC_DESC_FCW_OFFSET;
		desc->req.data_ptrs[0].address = q->ring_addr_iova + fcw_offset;
		desc->req.data_ptrs[0].blen = ACC_FCW_LD_BLEN;
		rte_memcpy(&desc->req.fcw_ld, &desc_first->req.fcw_ld, ACC_FCW_LD_BLEN);
		desc->req.fcw_ld.tb_trailer_size = (c - r - 1) * trail_len;

		ret = acc200_dma_desc_ld_fill(op, &desc->req, &input,
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

#ifdef RTE_LIBRTE_BBDEV_DEBUG
	if (check_mbuf_total_left(mbuf_total_left) != 0)
		return -EINVAL;
#endif
	/* Set SDone on last CB descriptor for TB mode. */
	desc->req.sdone_enable = 1;
	desc->req.irq_enable = q->irq_enable;

	return current_enqueued_cbs;
}

/** Enqueue encode operations for ACC200 device in CB mode. */
static inline uint16_t
acc200_enqueue_ldpc_enc_cb(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_enc_op **ops, uint16_t num)
{
	struct acc_queue *q = q_data->queue_private;
	int32_t avail = acc_ring_avail_enq(q);
	uint16_t i = 0;
	union acc_dma_desc *desc;
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

	/* Set SDone in last CB in enqueued ops for CB mode. */
	desc = q->ring_addr + ((q->sw_ring_head + desc_idx - 1) & q->sw_ring_wrap_mask);
	desc->req.sdone_enable = 1;
	desc->req.irq_enable = q->irq_enable;

	acc_dma_enqueue(q, desc_idx, &q_data->queue_stats);

	/* Update stats. */
	q_data->queue_stats.enqueued_count += i;
	q_data->queue_stats.enqueue_err_count += num - i;

	return i;
}

/* Enqueue LDPC encode operations for ACC200 device in TB mode. */
static uint16_t
acc200_enqueue_ldpc_enc_tb(struct rte_bbdev_queue_data *q_data,
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

/* Enqueue encode operations for ACC200 device. */
static uint16_t
acc200_enqueue_ldpc_enc(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_enc_op **ops, uint16_t num)
{
	int32_t aq_avail = acc_aq_avail(q_data, num);
	if (unlikely((aq_avail <= 0) || (num == 0)))
		return 0;
	if (ops[0]->ldpc_enc.code_block_mode == RTE_BBDEV_TRANSPORT_BLOCK)
		return acc200_enqueue_ldpc_enc_tb(q_data, ops, num);
	else
		return acc200_enqueue_ldpc_enc_cb(q_data, ops, num);
}

/* Enqueue decode operations for ACC200 device in TB mode. */
static uint16_t
acc200_enqueue_ldpc_dec_tb(struct rte_bbdev_queue_data *q_data,
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

		ret = enqueue_ldpc_dec_one_op_tb(q, ops[i],
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

/* Enqueue decode operations for ACC200 device in CB mode. */
static uint16_t
acc200_enqueue_ldpc_dec_cb(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_dec_op **ops, uint16_t num)
{
	struct acc_queue *q = q_data->queue_private;
	int32_t avail = acc_ring_avail_enq(q);
	uint16_t i;
	union acc_dma_desc *desc;
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
		ret = enqueue_ldpc_dec_one_op_cb(q, ops[i], i, same_op);
		if (ret < 0) {
			acc_enqueue_invalid(q_data);
			break;
		}
	}

	if (unlikely(i == 0))
		return 0; /* Nothing to enqueue. */

	/* Set SDone in last CB in enqueued ops for CB mode. */
	desc = q->ring_addr + ((q->sw_ring_head + i - 1) & q->sw_ring_wrap_mask);
	desc->req.sdone_enable = 1;
	desc->req.irq_enable = q->irq_enable;

	acc_dma_enqueue(q, i, &q_data->queue_stats);

	/* Update stats. */
	q_data->queue_stats.enqueued_count += i;
	q_data->queue_stats.enqueue_err_count += num - i;
	return i;
}

/* Enqueue decode operations for ACC200 device. */
static uint16_t
acc200_enqueue_ldpc_dec(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_dec_op **ops, uint16_t num)
{
	int32_t aq_avail = acc_aq_avail(q_data, num);
	if (unlikely((aq_avail <= 0) || (num == 0)))
		return 0;
	if (ops[0]->ldpc_dec.code_block_mode == RTE_BBDEV_TRANSPORT_BLOCK)
		return acc200_enqueue_ldpc_dec_tb(q_data, ops, num);
	else
		return acc200_enqueue_ldpc_dec_cb(q_data, ops, num);
}


/* Dequeue one encode operations from ACC200 device in CB mode. */
static inline int
dequeue_enc_one_op_cb(struct acc_queue *q, struct rte_bbdev_enc_op **ref_op,
		uint16_t *dequeued_ops, uint32_t *aq_dequeued, uint16_t *dequeued_descs)
{
	union acc_dma_desc *desc, atom_desc;
	union acc_dma_rsp_desc rsp;
	struct rte_bbdev_enc_op *op;
	int i;
	struct acc_ptrs *context_ptrs;
	int desc_idx = ((q->sw_ring_tail + *dequeued_descs) & q->sw_ring_wrap_mask);

	desc = q->ring_addr + desc_idx;
	atom_desc.atom_hdr = __atomic_load_n((uint64_t *)desc, __ATOMIC_RELAXED);

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

/* Dequeue one LDPC encode operations from ACC200 device in TB mode.
 * That operation may cover multiple descriptors.
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

	desc = q->ring_addr + ((q->sw_ring_tail + *dequeued_descs) & q->sw_ring_wrap_mask);
	atom_desc.atom_hdr = __atomic_load_n((uint64_t *)desc, __ATOMIC_RELAXED);

	/* Check fdone bit. */
	if (!(atom_desc.rsp.val & ACC_FDONE))
		return -1;

	/* Get number of CBs in dequeued TB. */
	descs_in_tb = desc->req.cbs_in_tb;
	/* Get last CB */
	last_desc = q->ring_addr + ((q->sw_ring_tail + *dequeued_descs + descs_in_tb - 1)
			& q->sw_ring_wrap_mask);
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
		desc = q->ring_addr + ((q->sw_ring_tail + *dequeued_descs) & q->sw_ring_wrap_mask);
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

/* Dequeue one decode operations from ACC200 device in CB mode. */
static inline int
dequeue_ldpc_dec_one_op_cb(struct rte_bbdev_queue_data *q_data,
		struct acc_queue *q, struct rte_bbdev_dec_op **ref_op,
		uint16_t dequeued_cbs, uint32_t *aq_dequeued)
{
	union acc_dma_desc *desc, atom_desc;
	union acc_dma_rsp_desc rsp;
	struct rte_bbdev_dec_op *op;

	desc = q->ring_addr + ((q->sw_ring_tail + dequeued_cbs) & q->sw_ring_wrap_mask);
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

/* Dequeue one decode operations from ACC200 device in TB mode. */
static inline int
dequeue_dec_one_op_tb(struct acc_queue *q, struct rte_bbdev_dec_op **ref_op,
		uint16_t dequeued_cbs, uint32_t *aq_dequeued)
{
	union acc_dma_desc *desc, *last_desc, atom_desc;
	union acc_dma_rsp_desc rsp;
	struct rte_bbdev_dec_op *op;
	uint8_t cbs_in_tb = 1, cb_idx = 0;
	uint32_t tb_crc_check = 0;

	desc = q->ring_addr + ((q->sw_ring_tail + dequeued_cbs) & q->sw_ring_wrap_mask);
	atom_desc.atom_hdr = __atomic_load_n((uint64_t *)desc, __ATOMIC_RELAXED);

	/* Check fdone bit. */
	if (!(atom_desc.rsp.val & ACC_FDONE))
		return -1;

	/* Dequeue. */
	op = desc->req.op_addr;

	/* Get number of CBs in dequeued TB. */
	cbs_in_tb = desc->req.cbs_in_tb;
	/* Get last CB. */
	last_desc = q->ring_addr + ((q->sw_ring_tail + dequeued_cbs + cbs_in_tb - 1)
			& q->sw_ring_wrap_mask);
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
		desc = q->ring_addr + ((q->sw_ring_tail + dequeued_cbs) & q->sw_ring_wrap_mask);
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

/* Dequeue LDPC encode operations from ACC200 device. */
static uint16_t
acc200_dequeue_ldpc_enc(struct rte_bbdev_queue_data *q_data,
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
	op = (q->ring_addr + (q->sw_ring_tail & q->sw_ring_wrap_mask))->req.op_addr;
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

	/* Update enqueue stats. */
	q_data->queue_stats.dequeued_count += dequeued_ops;

	return dequeued_ops;
}

/* Dequeue decode operations from ACC200 device. */
static uint16_t
acc200_dequeue_ldpc_dec(struct rte_bbdev_queue_data *q_data,
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
		op = (q->ring_addr + ((q->sw_ring_tail + dequeued_cbs)
			& q->sw_ring_wrap_mask))->req.op_addr;
		if (op->ldpc_dec.code_block_mode == RTE_BBDEV_TRANSPORT_BLOCK)
			ret = dequeue_dec_one_op_tb(q, &ops[i], dequeued_cbs,
					&aq_dequeued);
		else
			ret = dequeue_ldpc_dec_one_op_cb(
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

/* Initialization Function */
static void
acc200_bbdev_init(struct rte_bbdev *dev, struct rte_pci_driver *drv)
{
	struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI(dev->device);

	dev->dev_ops = &acc200_bbdev_ops;
	dev->enqueue_ldpc_enc_ops = acc200_enqueue_ldpc_enc;
	dev->enqueue_ldpc_dec_ops = acc200_enqueue_ldpc_dec;
	dev->dequeue_ldpc_enc_ops = acc200_dequeue_ldpc_enc;
	dev->dequeue_ldpc_dec_ops = acc200_dequeue_ldpc_dec;

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
