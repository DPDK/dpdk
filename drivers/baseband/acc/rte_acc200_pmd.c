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

/* Stop ACC200 queue and clear counters. */
static int
acc200_queue_stop(struct rte_bbdev *dev, uint16_t queue_id)
{
	struct acc_queue *q;
	q = dev->data->queues[queue_id].queue_private;
	rte_bbdev_log(INFO, "Queue Stop %d H/T/D %d %d %x OpType %d",
			queue_id, q->sw_ring_head, q->sw_ring_tail,
			q->sw_ring_depth, q->op_type);
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
