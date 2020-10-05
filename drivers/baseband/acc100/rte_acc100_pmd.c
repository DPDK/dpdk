/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
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

#include <rte_bbdev.h>
#include <rte_bbdev_pmd.h>
#include "rte_acc100_pmd.h"

#ifdef RTE_LIBRTE_BBDEV_DEBUG
RTE_LOG_REGISTER(acc100_logtype, pmd.bb.acc100, DEBUG);
#else
RTE_LOG_REGISTER(acc100_logtype, pmd.bb.acc100, NOTICE);
#endif

/* Write to MMIO register address */
static inline void
mmio_write(void *addr, uint32_t value)
{
	*((volatile uint32_t *)(addr)) = rte_cpu_to_le_32(value);
}

/* Write a register of a ACC100 device */
static inline void
acc100_reg_write(struct acc100_device *d, uint32_t offset, uint32_t payload)
{
	void *reg_addr = RTE_PTR_ADD(d->mmio_base, offset);
	mmio_write(reg_addr, payload);
	usleep(ACC100_LONG_WAIT);
}

/* Read a register of a ACC100 device */
static inline uint32_t
acc100_reg_read(struct acc100_device *d, uint32_t offset)
{

	void *reg_addr = RTE_PTR_ADD(d->mmio_base, offset);
	uint32_t ret = *((volatile uint32_t *)(reg_addr));
	return rte_le_to_cpu_32(ret);
}

/* Basic Implementation of Log2 for exact 2^N */
static inline uint32_t
log2_basic(uint32_t value)
{
	return (value == 0) ? 0 : rte_bsf32(value);
}

/* Calculate memory alignment offset assuming alignment is 2^N */
static inline uint32_t
calc_mem_alignment_offset(void *unaligned_virt_mem, uint32_t alignment)
{
	rte_iova_t unaligned_phy_mem = rte_malloc_virt2iova(unaligned_virt_mem);
	return (uint32_t)(alignment -
			(unaligned_phy_mem & (alignment-1)));
}

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

/* Return the queue topology for a Queue Group Index */
static inline void
qtopFromAcc(struct rte_acc100_queue_topology **qtop, int acc_enum,
		struct rte_acc100_conf *acc100_conf)
{
	struct rte_acc100_queue_topology *p_qtop;
	p_qtop = NULL;
	switch (acc_enum) {
	case UL_4G:
		p_qtop = &(acc100_conf->q_ul_4g);
		break;
	case UL_5G:
		p_qtop = &(acc100_conf->q_ul_5g);
		break;
	case DL_4G:
		p_qtop = &(acc100_conf->q_dl_4g);
		break;
	case DL_5G:
		p_qtop = &(acc100_conf->q_dl_5g);
		break;
	default:
		/* NOTREACHED */
		rte_bbdev_log(ERR, "Unexpected error evaluating qtopFromAcc");
		break;
	}
	*qtop = p_qtop;
}

static void
initQTop(struct rte_acc100_conf *acc100_conf)
{
	acc100_conf->q_ul_4g.num_aqs_per_groups = 0;
	acc100_conf->q_ul_4g.num_qgroups = 0;
	acc100_conf->q_ul_4g.first_qgroup_index = -1;
	acc100_conf->q_ul_5g.num_aqs_per_groups = 0;
	acc100_conf->q_ul_5g.num_qgroups = 0;
	acc100_conf->q_ul_5g.first_qgroup_index = -1;
	acc100_conf->q_dl_4g.num_aqs_per_groups = 0;
	acc100_conf->q_dl_4g.num_qgroups = 0;
	acc100_conf->q_dl_4g.first_qgroup_index = -1;
	acc100_conf->q_dl_5g.num_aqs_per_groups = 0;
	acc100_conf->q_dl_5g.num_qgroups = 0;
	acc100_conf->q_dl_5g.first_qgroup_index = -1;
}

static inline void
updateQtop(uint8_t acc, uint8_t qg, struct rte_acc100_conf *acc100_conf,
		struct acc100_device *d) {
	uint32_t reg;
	struct rte_acc100_queue_topology *q_top = NULL;
	qtopFromAcc(&q_top, acc, acc100_conf);
	if (unlikely(q_top == NULL))
		return;
	uint16_t aq;
	q_top->num_qgroups++;
	if (q_top->first_qgroup_index == -1) {
		q_top->first_qgroup_index = qg;
		/* Can be optimized to assume all are enabled by default */
		reg = acc100_reg_read(d, queue_offset(d->pf_device,
				0, qg, ACC100_NUM_AQS - 1));
		if (reg & ACC100_QUEUE_ENABLE) {
			q_top->num_aqs_per_groups = ACC100_NUM_AQS;
			return;
		}
		q_top->num_aqs_per_groups = 0;
		for (aq = 0; aq < ACC100_NUM_AQS; aq++) {
			reg = acc100_reg_read(d, queue_offset(d->pf_device,
					0, qg, aq));
			if (reg & ACC100_QUEUE_ENABLE)
				q_top->num_aqs_per_groups++;
		}
	}
}

/* Fetch configuration enabled for the PF/VF using MMIO Read (slow) */
static inline void
fetch_acc100_config(struct rte_bbdev *dev)
{
	struct acc100_device *d = dev->data->dev_private;
	struct rte_acc100_conf *acc100_conf = &d->acc100_conf;
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

	d->ddr_size = (1 + acc100_reg_read(d, reg_addr->ddr_range)) << 10;

	/* Single VF Bundle by VF */
	acc100_conf->num_vf_bundles = 1;
	initQTop(acc100_conf);

	struct rte_acc100_queue_topology *q_top = NULL;
	int qman_func_id[ACC100_NUM_ACCS] = {ACC100_ACCMAP_0, ACC100_ACCMAP_1,
			ACC100_ACCMAP_2, ACC100_ACCMAP_3, ACC100_ACCMAP_4};
	reg = acc100_reg_read(d, reg_addr->qman_group_func);
	for (qg = 0; qg < ACC100_NUM_QGRPS_PER_WORD; qg++) {
		reg_aq = acc100_reg_read(d,
				queue_offset(d->pf_device, 0, qg, 0));
		if (reg_aq & ACC100_QUEUE_ENABLE) {
			uint32_t idx = (reg >> (qg * 4)) & 0x7;
			if (idx < ACC100_NUM_ACCS) {
				acc = qman_func_id[idx];
				updateQtop(acc, qg, acc100_conf, d);
			}
		}
	}

	/* Check the depth of the AQs*/
	reg_len0 = acc100_reg_read(d, reg_addr->depth_log0_offset);
	reg_len1 = acc100_reg_read(d, reg_addr->depth_log1_offset);
	for (acc = 0; acc < NUM_ACC; acc++) {
		qtopFromAcc(&q_top, acc, acc100_conf);
		if (q_top->first_qgroup_index < ACC100_NUM_QGRPS_PER_WORD)
			q_top->aq_depth_log2 = (reg_len0 >>
					(q_top->first_qgroup_index * 4))
					& 0xF;
		else
			q_top->aq_depth_log2 = (reg_len1 >>
					((q_top->first_qgroup_index -
					ACC100_NUM_QGRPS_PER_WORD) * 4))
					& 0xF;
	}

	/* Read PF mode */
	if (d->pf_device) {
		reg_mode = acc100_reg_read(d, HWPfHiPfMode);
		acc100_conf->pf_mode_en = (reg_mode == ACC100_PF_VAL) ? 1 : 0;
	}

	rte_bbdev_log_debug(
			"%s Config LLR SIGN IN/OUT %s %s QG %u %u %u %u AQ %u %u %u %u Len %u %u %u %u\n",
			(d->pf_device) ? "PF" : "VF",
			(acc100_conf->input_pos_llr_1_bit) ? "POS" : "NEG",
			(acc100_conf->output_pos_llr_1_bit) ? "POS" : "NEG",
			acc100_conf->q_ul_4g.num_qgroups,
			acc100_conf->q_dl_4g.num_qgroups,
			acc100_conf->q_ul_5g.num_qgroups,
			acc100_conf->q_dl_5g.num_qgroups,
			acc100_conf->q_ul_4g.num_aqs_per_groups,
			acc100_conf->q_dl_4g.num_aqs_per_groups,
			acc100_conf->q_ul_5g.num_aqs_per_groups,
			acc100_conf->q_dl_5g.num_aqs_per_groups,
			acc100_conf->q_ul_4g.aq_depth_log2,
			acc100_conf->q_dl_4g.aq_depth_log2,
			acc100_conf->q_ul_5g.aq_depth_log2,
			acc100_conf->q_dl_5g.aq_depth_log2);
}

static void
free_base_addresses(void **base_addrs, int size)
{
	int i;
	for (i = 0; i < size; i++)
		rte_free(base_addrs[i]);
}

static inline uint32_t
get_desc_len(void)
{
	return sizeof(union acc100_dma_desc);
}

/* Allocate the 2 * 64MB block for the sw rings */
static int
alloc_2x64mb_sw_rings_mem(struct rte_bbdev *dev, struct acc100_device *d,
		int socket)
{
	uint32_t sw_ring_size = ACC100_SIZE_64MBYTE;
	d->sw_rings_base = rte_zmalloc_socket(dev->device->driver->name,
			2 * sw_ring_size, RTE_CACHE_LINE_SIZE, socket);
	if (d->sw_rings_base == NULL) {
		rte_bbdev_log(ERR, "Failed to allocate memory for %s:%u",
				dev->device->driver->name,
				dev->data->dev_id);
		return -ENOMEM;
	}
	uint32_t next_64mb_align_offset = calc_mem_alignment_offset(
			d->sw_rings_base, ACC100_SIZE_64MBYTE);
	d->sw_rings = RTE_PTR_ADD(d->sw_rings_base, next_64mb_align_offset);
	d->sw_rings_iova = rte_malloc_virt2iova(d->sw_rings_base) +
			next_64mb_align_offset;
	d->sw_ring_size = ACC100_MAX_QUEUE_DEPTH * get_desc_len();
	d->sw_ring_max_depth = ACC100_MAX_QUEUE_DEPTH;

	return 0;
}

/* Attempt to allocate minimised memory space for sw rings */
static void
alloc_sw_rings_min_mem(struct rte_bbdev *dev, struct acc100_device *d,
		uint16_t num_queues, int socket)
{
	rte_iova_t sw_rings_base_iova, next_64mb_align_addr_iova;
	uint32_t next_64mb_align_offset;
	rte_iova_t sw_ring_iova_end_addr;
	void *base_addrs[ACC100_SW_RING_MEM_ALLOC_ATTEMPTS];
	void *sw_rings_base;
	int i = 0;
	uint32_t q_sw_ring_size = ACC100_MAX_QUEUE_DEPTH * get_desc_len();
	uint32_t dev_sw_ring_size = q_sw_ring_size * num_queues;

	/* Find an aligned block of memory to store sw rings */
	while (i < ACC100_SW_RING_MEM_ALLOC_ATTEMPTS) {
		/*
		 * sw_ring allocated memory is guaranteed to be aligned to
		 * q_sw_ring_size at the condition that the requested size is
		 * less than the page size
		 */
		sw_rings_base = rte_zmalloc_socket(
				dev->device->driver->name,
				dev_sw_ring_size, q_sw_ring_size, socket);

		if (sw_rings_base == NULL) {
			rte_bbdev_log(ERR,
					"Failed to allocate memory for %s:%u",
					dev->device->driver->name,
					dev->data->dev_id);
			break;
		}

		sw_rings_base_iova = rte_malloc_virt2iova(sw_rings_base);
		next_64mb_align_offset = calc_mem_alignment_offset(
				sw_rings_base, ACC100_SIZE_64MBYTE);
		next_64mb_align_addr_iova = sw_rings_base_iova +
				next_64mb_align_offset;
		sw_ring_iova_end_addr = sw_rings_base_iova + dev_sw_ring_size;

		/* Check if the end of the sw ring memory block is before the
		 * start of next 64MB aligned mem address
		 */
		if (sw_ring_iova_end_addr < next_64mb_align_addr_iova) {
			d->sw_rings_iova = sw_rings_base_iova;
			d->sw_rings = sw_rings_base;
			d->sw_rings_base = sw_rings_base;
			d->sw_ring_size = q_sw_ring_size;
			d->sw_ring_max_depth = ACC100_MAX_QUEUE_DEPTH;
			break;
		}
		/* Store the address of the unaligned mem block */
		base_addrs[i] = sw_rings_base;
		i++;
	}

	/* Free all unaligned blocks of mem allocated in the loop */
	free_base_addresses(base_addrs, i);
}


/* Allocate 64MB memory used for all software rings */
static int
acc100_setup_queues(struct rte_bbdev *dev, uint16_t num_queues, int socket_id)
{
	uint32_t phys_low, phys_high, payload;
	struct acc100_device *d = dev->data->dev_private;
	const struct acc100_registry_addr *reg_addr;

	if (d->pf_device && !d->acc100_conf.pf_mode_en) {
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
	phys_low  = (uint32_t)(d->sw_rings_iova & ~(ACC100_SIZE_64MBYTE-1));

	/* Choose correct registry addresses for the device type */
	if (d->pf_device)
		reg_addr = &pf_reg_addr;
	else
		reg_addr = &vf_reg_addr;

	/* Read the populated cfg from ACC100 registers */
	fetch_acc100_config(dev);

	/* Release AXI from PF */
	if (d->pf_device)
		acc100_reg_write(d, HWPfDmaAxiControl, 1);

	acc100_reg_write(d, reg_addr->dma_ring_ul5g_hi, phys_high);
	acc100_reg_write(d, reg_addr->dma_ring_ul5g_lo, phys_low);
	acc100_reg_write(d, reg_addr->dma_ring_dl5g_hi, phys_high);
	acc100_reg_write(d, reg_addr->dma_ring_dl5g_lo, phys_low);
	acc100_reg_write(d, reg_addr->dma_ring_ul4g_hi, phys_high);
	acc100_reg_write(d, reg_addr->dma_ring_ul4g_lo, phys_low);
	acc100_reg_write(d, reg_addr->dma_ring_dl4g_hi, phys_high);
	acc100_reg_write(d, reg_addr->dma_ring_dl4g_lo, phys_low);

	/*
	 * Configure Ring Size to the max queue ring size
	 * (used for wrapping purpose)
	 */
	payload = log2_basic(d->sw_ring_size / 64);
	acc100_reg_write(d, reg_addr->ring_size, payload);

	/* Configure tail pointer for use when SDONE enabled */
	d->tail_ptrs = rte_zmalloc_socket(
			dev->device->driver->name,
			ACC100_NUM_QGRPS * ACC100_NUM_AQS * sizeof(uint32_t),
			RTE_CACHE_LINE_SIZE, socket_id);
	if (d->tail_ptrs == NULL) {
		rte_bbdev_log(ERR, "Failed to allocate tail ptr for %s:%u",
				dev->device->driver->name,
				dev->data->dev_id);
		rte_free(d->sw_rings);
		return -ENOMEM;
	}
	d->tail_ptr_iova = rte_malloc_virt2iova(d->tail_ptrs);

	phys_high = (uint32_t)(d->tail_ptr_iova >> 32);
	phys_low  = (uint32_t)(d->tail_ptr_iova);
	acc100_reg_write(d, reg_addr->tail_ptrs_ul5g_hi, phys_high);
	acc100_reg_write(d, reg_addr->tail_ptrs_ul5g_lo, phys_low);
	acc100_reg_write(d, reg_addr->tail_ptrs_dl5g_hi, phys_high);
	acc100_reg_write(d, reg_addr->tail_ptrs_dl5g_lo, phys_low);
	acc100_reg_write(d, reg_addr->tail_ptrs_ul4g_hi, phys_high);
	acc100_reg_write(d, reg_addr->tail_ptrs_ul4g_lo, phys_low);
	acc100_reg_write(d, reg_addr->tail_ptrs_dl4g_hi, phys_high);
	acc100_reg_write(d, reg_addr->tail_ptrs_dl4g_lo, phys_low);

	d->harq_layout = rte_zmalloc_socket("HARQ Layout",
			ACC100_HARQ_LAYOUT * sizeof(*d->harq_layout),
			RTE_CACHE_LINE_SIZE, dev->data->socket_id);
	if (d->harq_layout == NULL) {
		rte_bbdev_log(ERR, "Failed to allocate harq_layout for %s:%u",
				dev->device->driver->name,
				dev->data->dev_id);
		rte_free(d->sw_rings);
		return -ENOMEM;
	}

	/* Mark as configured properly */
	d->configured = true;

	rte_bbdev_log_debug(
			"ACC100 (%s) configured  sw_rings = %p, sw_rings_iova = %#"
			PRIx64, dev->data->name, d->sw_rings, d->sw_rings_iova);

	return 0;
}

/* Free memory used for software rings */
static int
acc100_dev_close(struct rte_bbdev *dev)
{
	struct acc100_device *d = dev->data->dev_private;
	if (d->sw_rings_base != NULL) {
		rte_free(d->tail_ptrs);
		rte_free(d->sw_rings_base);
		d->sw_rings_base = NULL;
	}
	/* Ensure all in flight HW transactions are completed */
	usleep(ACC100_LONG_WAIT);
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
	struct acc100_device *d = dev->data->dev_private;
	int op_2_acc[5] = {0, UL_4G, DL_4G, UL_5G, DL_5G};
	int acc = op_2_acc[conf->op_type];
	struct rte_acc100_queue_topology *qtop = NULL;

	qtopFromAcc(&qtop, acc, &(d->acc100_conf));
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
	uint16_t aq_idx;
	for (aq_idx = 0; aq_idx < qtop->num_aqs_per_groups; aq_idx++) {
		if (((d->q_assigned_bit_map[group_idx] >> aq_idx) & 0x1) == 0) {
			/* Mark the Queue as assigned */
			d->q_assigned_bit_map[group_idx] |= (1 << aq_idx);
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
	struct acc100_device *d = dev->data->dev_private;
	struct acc100_queue *q;
	int16_t q_idx;

	/* Allocate the queue data structure. */
	q = rte_zmalloc_socket(dev->device->driver->name, sizeof(*q),
			RTE_CACHE_LINE_SIZE, conf->socket);
	if (q == NULL) {
		rte_bbdev_log(ERR, "Failed to allocate queue memory");
		return -ENOMEM;
	}
	if (d == NULL) {
		rte_bbdev_log(ERR, "Undefined device");
		return -ENODEV;
	}

	q->d = d;
	q->ring_addr = RTE_PTR_ADD(d->sw_rings, (d->sw_ring_size * queue_id));
	q->ring_addr_iova = d->sw_rings_iova + (d->sw_ring_size * queue_id);

	/* Prepare the Ring with default descriptor format */
	union acc100_dma_desc *desc = NULL;
	unsigned int desc_idx, b_idx;
	int fcw_len = (conf->op_type == RTE_BBDEV_OP_LDPC_ENC ?
		ACC100_FCW_LE_BLEN : (conf->op_type == RTE_BBDEV_OP_TURBO_DEC ?
		ACC100_FCW_TD_BLEN : ACC100_FCW_LD_BLEN));

	for (desc_idx = 0; desc_idx < d->sw_ring_max_depth; desc_idx++) {
		desc = q->ring_addr + desc_idx;
		desc->req.word0 = ACC100_DMA_DESC_TYPE;
		desc->req.word1 = 0; /**< Timestamp */
		desc->req.word2 = 0;
		desc->req.word3 = 0;
		uint64_t fcw_offset = (desc_idx << 8) + ACC100_DESC_FCW_OFFSET;
		desc->req.data_ptrs[0].address = q->ring_addr_iova + fcw_offset;
		desc->req.data_ptrs[0].blen = fcw_len;
		desc->req.data_ptrs[0].blkid = ACC100_DMA_BLKID_FCW;
		desc->req.data_ptrs[0].last = 0;
		desc->req.data_ptrs[0].dma_ext = 0;
		for (b_idx = 1; b_idx < ACC100_DMA_MAX_NUM_POINTERS - 1;
				b_idx++) {
			desc->req.data_ptrs[b_idx].blkid = ACC100_DMA_BLKID_IN;
			desc->req.data_ptrs[b_idx].last = 1;
			desc->req.data_ptrs[b_idx].dma_ext = 0;
			b_idx++;
			desc->req.data_ptrs[b_idx].blkid =
					ACC100_DMA_BLKID_OUT_ENC;
			desc->req.data_ptrs[b_idx].last = 1;
			desc->req.data_ptrs[b_idx].dma_ext = 0;
		}
		/* Preset some fields of LDPC FCW */
		desc->req.fcw_ld.FCWversion = ACC100_FCW_VER;
		desc->req.fcw_ld.gain_i = 1;
		desc->req.fcw_ld.gain_h = 1;
	}

	q->lb_in = rte_zmalloc_socket(dev->device->driver->name,
			RTE_CACHE_LINE_SIZE,
			RTE_CACHE_LINE_SIZE, conf->socket);
	if (q->lb_in == NULL) {
		rte_bbdev_log(ERR, "Failed to allocate lb_in memory");
		rte_free(q);
		return -ENOMEM;
	}
	q->lb_in_addr_iova = rte_malloc_virt2iova(q->lb_in);
	q->lb_out = rte_zmalloc_socket(dev->device->driver->name,
			RTE_CACHE_LINE_SIZE,
			RTE_CACHE_LINE_SIZE, conf->socket);
	if (q->lb_out == NULL) {
		rte_bbdev_log(ERR, "Failed to allocate lb_out memory");
		rte_free(q->lb_in);
		rte_free(q);
		return -ENOMEM;
	}
	q->lb_out_addr_iova = rte_malloc_virt2iova(q->lb_out);

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
		rte_free(q->lb_in);
		rte_free(q->lb_out);
		rte_free(q);
		return -1;
	}

	q->qgrp_id = (q_idx >> ACC100_GRP_ID_SHIFT) & 0xF;
	q->vf_id = (q_idx >> ACC100_VF_ID_SHIFT)  & 0x3F;
	q->aq_id = q_idx & 0xF;
	q->aq_depth = (conf->op_type ==  RTE_BBDEV_OP_TURBO_DEC) ?
			(1 << d->acc100_conf.q_ul_4g.aq_depth_log2) :
			(1 << d->acc100_conf.q_dl_4g.aq_depth_log2);

	q->mmio_reg_enqueue = RTE_PTR_ADD(d->mmio_base,
			queue_offset(d->pf_device,
					q->vf_id, q->qgrp_id, q->aq_id));

	rte_bbdev_log_debug(
			"Setup dev%u q%u: qgrp_id=%u, vf_id=%u, aq_id=%u, aq_depth=%u, mmio_reg_enqueue=%p",
			dev->data->dev_id, queue_id, q->qgrp_id, q->vf_id,
			q->aq_id, q->aq_depth, q->mmio_reg_enqueue);

	dev->data->queues[queue_id].queue_private = q;
	return 0;
}

/* Release ACC100 queue */
static int
acc100_queue_release(struct rte_bbdev *dev, uint16_t q_id)
{
	struct acc100_device *d = dev->data->dev_private;
	struct acc100_queue *q = dev->data->queues[q_id].queue_private;

	if (q != NULL) {
		/* Mark the Queue as un-assigned */
		d->q_assigned_bit_map[q->qgrp_id] &= (0xFFFFFFFF -
				(1 << q->aq_id));
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
	struct acc100_device *d = dev->data->dev_private;

	static const struct rte_bbdev_op_cap bbdev_capabilities[] = {
		RTE_BBDEV_END_OF_CAPABILITIES_LIST()
	};

	static struct rte_bbdev_queue_conf default_queue_conf;
	default_queue_conf.socket = dev->data->socket_id;
	default_queue_conf.queue_size = ACC100_MAX_QUEUE_DEPTH;

	dev_info->driver_name = dev->device->driver->name;

	/* Read and save the populated config from ACC100 registers */
	fetch_acc100_config(dev);

	/* This isn't ideal because it reports the maximum number of queues but
	 * does not provide info on how many can be uplink/downlink or different
	 * priorities
	 */
	dev_info->max_num_queues =
			d->acc100_conf.q_dl_5g.num_aqs_per_groups *
			d->acc100_conf.q_dl_5g.num_qgroups +
			d->acc100_conf.q_ul_5g.num_aqs_per_groups *
			d->acc100_conf.q_ul_5g.num_qgroups +
			d->acc100_conf.q_dl_4g.num_aqs_per_groups *
			d->acc100_conf.q_dl_4g.num_qgroups +
			d->acc100_conf.q_ul_4g.num_aqs_per_groups *
			d->acc100_conf.q_ul_4g.num_qgroups;
	dev_info->queue_size_lim = ACC100_MAX_QUEUE_DEPTH;
	dev_info->hardware_accelerated = true;
	dev_info->max_dl_queue_priority =
			d->acc100_conf.q_dl_4g.num_qgroups - 1;
	dev_info->max_ul_queue_priority =
			d->acc100_conf.q_ul_4g.num_qgroups - 1;
	dev_info->default_queue_conf = default_queue_conf;
	dev_info->cpu_flag_reqs = NULL;
	dev_info->min_alignment = 64;
	dev_info->capabilities = bbdev_capabilities;
	dev_info->harq_buffer_size = d->ddr_size;
}

static const struct rte_bbdev_ops acc100_bbdev_ops = {
	.setup_queues = acc100_setup_queues,
	.close = acc100_dev_close,
	.info_get = acc100_dev_info_get,
	.queue_setup = acc100_queue_setup,
	.queue_release = acc100_queue_release,
};

/* ACC100 PCI PF address map */
static struct rte_pci_id pci_id_acc100_pf_map[] = {
	{
		RTE_PCI_DEVICE(RTE_ACC100_VENDOR_ID, RTE_ACC100_PF_DEVICE_ID)
	},
	{.device_id = 0},
};

/* ACC100 PCI VF address map */
static struct rte_pci_id pci_id_acc100_vf_map[] = {
	{
		RTE_PCI_DEVICE(RTE_ACC100_VENDOR_ID, RTE_ACC100_VF_DEVICE_ID)
	},
	{.device_id = 0},
};

/* Initialization Function */
static void
acc100_bbdev_init(struct rte_bbdev *dev, struct rte_pci_driver *drv)
{
	struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI(dev->device);

	dev->dev_ops = &acc100_bbdev_ops;

	((struct acc100_device *) dev->data->dev_private)->pf_device =
			!strcmp(drv->driver.name,
					RTE_STR(ACC100PF_DRIVER_NAME));
	((struct acc100_device *) dev->data->dev_private)->mmio_base =
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
			sizeof(struct acc100_device), RTE_CACHE_LINE_SIZE,
			pci_dev->device.numa_node);

	if (bbdev->data->dev_private == NULL) {
		rte_bbdev_log(CRIT,
				"Allocate of %zu bytes for device \"%s\" failed",
				sizeof(struct acc100_device), dev_name);
				rte_bbdev_release(bbdev);
			return -ENOMEM;
	}

	/* Fill HW specific part of device structure */
	bbdev->device = &pci_dev->device;
	bbdev->intr_handle = &pci_dev->intr_handle;
	bbdev->data->socket_id = pci_dev->device.numa_node;

	/* Invoke ACC100 device initialization function */
	acc100_bbdev_init(bbdev, pci_drv);

	rte_bbdev_log_debug("Initialised bbdev %s (id = %u)",
			dev_name, bbdev->data->dev_id);
	return 0;
}

static int acc100_pci_remove(struct rte_pci_device *pci_dev)
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
	rte_bbdev_release(bbdev);

	rte_bbdev_log_debug("Destroyed bbdev = %u", dev_id);

	return 0;
}

static struct rte_pci_driver acc100_pci_pf_driver = {
		.probe = acc100_pci_probe,
		.remove = acc100_pci_remove,
		.id_table = pci_id_acc100_pf_map,
		.drv_flags = RTE_PCI_DRV_NEED_MAPPING
};

static struct rte_pci_driver acc100_pci_vf_driver = {
		.probe = acc100_pci_probe,
		.remove = acc100_pci_remove,
		.id_table = pci_id_acc100_vf_map,
		.drv_flags = RTE_PCI_DRV_NEED_MAPPING
};

RTE_PMD_REGISTER_PCI(ACC100PF_DRIVER_NAME, acc100_pci_pf_driver);
RTE_PMD_REGISTER_PCI_TABLE(ACC100PF_DRIVER_NAME, pci_id_acc100_pf_map);
RTE_PMD_REGISTER_PCI(ACC100VF_DRIVER_NAME, acc100_pci_vf_driver);
RTE_PMD_REGISTER_PCI_TABLE(ACC100VF_DRIVER_NAME, pci_id_acc100_vf_map);
