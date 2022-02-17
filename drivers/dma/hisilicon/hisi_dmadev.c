/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 HiSilicon Limited
 */

#include <inttypes.h>
#include <string.h>

#include <rte_bus_pci.h>
#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_io.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_pci.h>
#include <rte_dmadev_pmd.h>

#include "hisi_dmadev.h"

RTE_LOG_REGISTER_DEFAULT(hisi_dma_logtype, INFO);
#define HISI_DMA_LOG(level, fmt, args...) \
		rte_log(RTE_LOG_ ## level, hisi_dma_logtype, \
		"%s(): " fmt "\n", __func__, ##args)
#define HISI_DMA_LOG_RAW(hw, level, fmt, args...) \
		rte_log(RTE_LOG_ ## level, hisi_dma_logtype, \
		"%s %s(): " fmt "\n", (hw)->data->dev_name, \
		__func__, ##args)
#define HISI_DMA_DEBUG(hw, fmt, args...) \
		HISI_DMA_LOG_RAW(hw, DEBUG, fmt, ## args)
#define HISI_DMA_INFO(hw, fmt, args...) \
		HISI_DMA_LOG_RAW(hw, INFO, fmt, ## args)
#define HISI_DMA_WARN(hw, fmt, args...) \
		HISI_DMA_LOG_RAW(hw, WARNING, fmt, ## args)
#define HISI_DMA_ERR(hw, fmt, args...) \
		HISI_DMA_LOG_RAW(hw, ERR, fmt, ## args)

static uint32_t
hisi_dma_queue_base(struct hisi_dma_dev *hw)
{
	if (hw->reg_layout == HISI_DMA_REG_LAYOUT_HIP08)
		return HISI_DMA_HIP08_QUEUE_BASE;
	else if (hw->reg_layout == HISI_DMA_REG_LAYOUT_HIP09)
		return HISI_DMA_HIP09_QUEUE_BASE;
	else
		return 0;
}

static volatile void *
hisi_dma_queue_regaddr(struct hisi_dma_dev *hw, uint32_t qoff)
{
	uint32_t off = hisi_dma_queue_base(hw) +
			hw->queue_id * HISI_DMA_QUEUE_REGION_SIZE + qoff;
	return (volatile void *)((char *)hw->io_base + off);
}

static void
hisi_dma_write_reg(void *base, uint32_t off, uint32_t val)
{
	rte_write32(rte_cpu_to_le_32(val),
		    (volatile void *)((char *)base + off));
}

static void
hisi_dma_write_dev(struct hisi_dma_dev *hw, uint32_t off, uint32_t val)
{
	hisi_dma_write_reg(hw->io_base, off, val);
}

static void
hisi_dma_write_queue(struct hisi_dma_dev *hw, uint32_t qoff, uint32_t val)
{
	uint32_t off = hisi_dma_queue_base(hw) +
			hw->queue_id * HISI_DMA_QUEUE_REGION_SIZE + qoff;
	hisi_dma_write_dev(hw, off, val);
}

static uint32_t
hisi_dma_read_reg(void *base, uint32_t off)
{
	uint32_t val = rte_read32((volatile void *)((char *)base + off));
	return rte_le_to_cpu_32(val);
}

static uint32_t
hisi_dma_read_dev(struct hisi_dma_dev *hw, uint32_t off)
{
	return hisi_dma_read_reg(hw->io_base, off);
}

static uint32_t
hisi_dma_read_queue(struct hisi_dma_dev *hw, uint32_t qoff)
{
	uint32_t off = hisi_dma_queue_base(hw) +
			hw->queue_id * HISI_DMA_QUEUE_REGION_SIZE + qoff;
	return hisi_dma_read_dev(hw, off);
}

static void
hisi_dma_update_bit(struct hisi_dma_dev *hw, uint32_t off, uint32_t pos,
		    bool set)
{
	uint32_t tmp = hisi_dma_read_dev(hw, off);
	uint32_t mask = 1u << pos;
	tmp = set ? tmp | mask : tmp & ~mask;
	hisi_dma_write_dev(hw, off, tmp);
}

static void
hisi_dma_update_queue_bit(struct hisi_dma_dev *hw, uint32_t qoff, uint32_t pos,
			  bool set)
{
	uint32_t tmp = hisi_dma_read_queue(hw, qoff);
	uint32_t mask = 1u << pos;
	tmp = set ? tmp | mask : tmp & ~mask;
	hisi_dma_write_queue(hw, qoff, tmp);
}

static void
hisi_dma_update_queue_mbit(struct hisi_dma_dev *hw, uint32_t qoff,
			   uint32_t mask, bool set)
{
	uint32_t tmp = hisi_dma_read_queue(hw, qoff);
	tmp = set ? tmp | mask : tmp & ~mask;
	hisi_dma_write_queue(hw, qoff, tmp);
}

#define hisi_dma_poll_hw_state(hw, val, cond, sleep_us, timeout_us) ({ \
	uint32_t timeout = 0; \
	while (timeout++ <= (timeout_us)) { \
		(val) = hisi_dma_read_queue(hw, HISI_DMA_QUEUE_FSM_REG); \
		if (cond) \
			break; \
		rte_delay_us(sleep_us); \
	} \
	(cond) ? 0 : -ETIME; \
})

static int
hisi_dma_reset_hw(struct hisi_dma_dev *hw)
{
#define POLL_SLEEP_US	100
#define POLL_TIMEOUT_US	10000

	uint32_t tmp;
	int ret;

	hisi_dma_update_queue_bit(hw, HISI_DMA_QUEUE_CTRL0_REG,
				  HISI_DMA_QUEUE_CTRL0_PAUSE_B, true);
	hisi_dma_update_queue_bit(hw, HISI_DMA_QUEUE_CTRL0_REG,
				  HISI_DMA_QUEUE_CTRL0_EN_B, false);

	ret = hisi_dma_poll_hw_state(hw, tmp,
		FIELD_GET(HISI_DMA_QUEUE_FSM_STS_M, tmp) != HISI_DMA_STATE_RUN,
		POLL_SLEEP_US, POLL_TIMEOUT_US);
	if (ret) {
		HISI_DMA_ERR(hw, "disable dma timeout!");
		return ret;
	}

	hisi_dma_update_queue_bit(hw, HISI_DMA_QUEUE_CTRL1_REG,
				  HISI_DMA_QUEUE_CTRL1_RESET_B, true);
	hisi_dma_write_queue(hw, HISI_DMA_QUEUE_SQ_TAIL_REG, 0);
	hisi_dma_write_queue(hw, HISI_DMA_QUEUE_CQ_HEAD_REG, 0);
	hisi_dma_update_queue_bit(hw, HISI_DMA_QUEUE_CTRL0_REG,
				  HISI_DMA_QUEUE_CTRL0_PAUSE_B, false);

	ret = hisi_dma_poll_hw_state(hw, tmp,
		FIELD_GET(HISI_DMA_QUEUE_FSM_STS_M, tmp) == HISI_DMA_STATE_IDLE,
		POLL_SLEEP_US, POLL_TIMEOUT_US);
	if (ret) {
		HISI_DMA_ERR(hw, "reset dma timeout!");
		return ret;
	}

	return 0;
}

static void
hisi_dma_init_common(struct hisi_dma_dev *hw)
{
	hisi_dma_write_queue(hw, HISI_DMA_QUEUE_SQ_BASE_L_REG,
			     lower_32_bits(hw->sqe_iova));
	hisi_dma_write_queue(hw, HISI_DMA_QUEUE_SQ_BASE_H_REG,
			     upper_32_bits(hw->sqe_iova));
	hisi_dma_write_queue(hw, HISI_DMA_QUEUE_CQ_BASE_L_REG,
			     lower_32_bits(hw->cqe_iova));
	hisi_dma_write_queue(hw, HISI_DMA_QUEUE_CQ_BASE_H_REG,
			     upper_32_bits(hw->cqe_iova));
	hisi_dma_write_queue(hw, HISI_DMA_QUEUE_SQ_DEPTH_REG,
			     hw->sq_depth_mask);
	hisi_dma_write_queue(hw, HISI_DMA_QUEUE_CQ_DEPTH_REG, hw->cq_depth - 1);
	hisi_dma_write_queue(hw, HISI_DMA_QUEUE_SQ_TAIL_REG, 0);
	hisi_dma_write_queue(hw, HISI_DMA_QUEUE_CQ_HEAD_REG, 0);
	hisi_dma_write_queue(hw, HISI_DMA_QUEUE_ERR_INT_NUM0_REG, 0);
	hisi_dma_write_queue(hw, HISI_DMA_QUEUE_ERR_INT_NUM1_REG, 0);
	hisi_dma_write_queue(hw, HISI_DMA_QUEUE_ERR_INT_NUM2_REG, 0);
}

static void
hisi_dma_init_hw(struct hisi_dma_dev *hw)
{
	hisi_dma_init_common(hw);

	if (hw->reg_layout == HISI_DMA_REG_LAYOUT_HIP08) {
		hisi_dma_write_queue(hw, HISI_DMA_HIP08_QUEUE_ERR_INT_NUM3_REG,
				     0);
		hisi_dma_write_queue(hw, HISI_DMA_HIP08_QUEUE_ERR_INT_NUM4_REG,
				     0);
		hisi_dma_write_queue(hw, HISI_DMA_HIP08_QUEUE_ERR_INT_NUM5_REG,
				     0);
		hisi_dma_write_queue(hw, HISI_DMA_HIP08_QUEUE_ERR_INT_NUM6_REG,
				     0);
		hisi_dma_update_queue_bit(hw, HISI_DMA_QUEUE_CTRL0_REG,
				HISI_DMA_HIP08_QUEUE_CTRL0_ERR_ABORT_B, false);
		hisi_dma_update_queue_mbit(hw, HISI_DMA_QUEUE_INT_STATUS_REG,
				HISI_DMA_HIP08_QUEUE_INT_MASK_M, true);
		hisi_dma_update_queue_mbit(hw, HISI_DMA_QUEUE_INT_MASK_REG,
				HISI_DMA_HIP08_QUEUE_INT_MASK_M, true);
	} else if (hw->reg_layout == HISI_DMA_REG_LAYOUT_HIP09) {
		hisi_dma_update_queue_mbit(hw, HISI_DMA_QUEUE_CTRL0_REG,
				HISI_DMA_HIP09_QUEUE_CTRL0_ERR_ABORT_M, false);
		hisi_dma_update_queue_mbit(hw, HISI_DMA_QUEUE_INT_STATUS_REG,
				HISI_DMA_HIP09_QUEUE_INT_MASK_M, true);
		hisi_dma_update_queue_mbit(hw, HISI_DMA_QUEUE_INT_MASK_REG,
				HISI_DMA_HIP09_QUEUE_INT_MASK_M, true);
		hisi_dma_update_queue_mbit(hw,
				HISI_DMA_HIP09_QUEUE_ERR_INT_STATUS_REG,
				HISI_DMA_HIP09_QUEUE_ERR_INT_MASK_M, true);
		hisi_dma_update_queue_mbit(hw,
				HISI_DMA_HIP09_QUEUE_ERR_INT_MASK_REG,
				HISI_DMA_HIP09_QUEUE_ERR_INT_MASK_M, true);
		hisi_dma_update_queue_bit(hw, HISI_DMA_QUEUE_CTRL1_REG,
				HISI_DMA_HIP09_QUEUE_CTRL1_VA_ENABLE_B, true);
		hisi_dma_update_bit(hw,
				HISI_DMA_HIP09_QUEUE_CFG_REG(hw->queue_id),
				HISI_DMA_HIP09_QUEUE_CFG_LINK_DOWN_MASK_B,
				true);
	}
}

static void
hisi_dma_init_gbl(void *pci_bar, uint8_t revision)
{
	struct hisi_dma_dev hw;

	memset(&hw, 0, sizeof(hw));
	hw.io_base = pci_bar;

	if (revision == HISI_DMA_REVISION_HIP08B)
		hisi_dma_update_bit(&hw, HISI_DMA_HIP08_MODE_REG,
				    HISI_DMA_HIP08_MODE_SEL_B, true);
}

static uint8_t
hisi_dma_reg_layout(uint8_t revision)
{
	if (revision == HISI_DMA_REVISION_HIP08B)
		return HISI_DMA_REG_LAYOUT_HIP08;
	else if (revision >= HISI_DMA_REVISION_HIP09A)
		return HISI_DMA_REG_LAYOUT_HIP09;
	else
		return HISI_DMA_REG_LAYOUT_INVALID;
}

static void
hisi_dma_zero_iomem(struct hisi_dma_dev *hw)
{
	memset(hw->iomz->addr, 0, hw->iomz_sz);
}

static int
hisi_dma_alloc_iomem(struct hisi_dma_dev *hw, uint16_t ring_size,
		     const char *dev_name)
{
	uint32_t sq_size = sizeof(struct hisi_dma_sqe) * ring_size;
	uint32_t cq_size = sizeof(struct hisi_dma_cqe) *
			   (ring_size + HISI_DMA_CQ_RESERVED);
	uint32_t status_size = sizeof(uint16_t) * ring_size;
	char mz_name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *iomz;
	uint32_t total_size;

	sq_size = RTE_CACHE_LINE_ROUNDUP(sq_size);
	cq_size = RTE_CACHE_LINE_ROUNDUP(cq_size);
	status_size = RTE_CACHE_LINE_ROUNDUP(status_size);
	total_size = sq_size + cq_size + status_size;

	(void)snprintf(mz_name, sizeof(mz_name), "hisi_dma:%s", dev_name);
	iomz = rte_memzone_reserve(mz_name, total_size, hw->data->numa_node,
				   RTE_MEMZONE_IOVA_CONTIG);
	if (iomz == NULL) {
		HISI_DMA_ERR(hw, "malloc %s iomem fail!", mz_name);
		return -ENOMEM;
	}

	hw->iomz = iomz;
	hw->iomz_sz = total_size;
	hw->sqe = iomz->addr;
	hw->cqe = (void *)((char *)iomz->addr + sq_size);
	hw->status = (void *)((char *)iomz->addr + sq_size + cq_size);
	hw->sqe_iova = iomz->iova;
	hw->cqe_iova = iomz->iova + sq_size;
	hw->sq_depth_mask = ring_size - 1;
	hw->cq_depth = ring_size + HISI_DMA_CQ_RESERVED;
	hisi_dma_zero_iomem(hw);

	return 0;
}

static void
hisi_dma_free_iomem(struct hisi_dma_dev *hw)
{
	if (hw->iomz != NULL)
		rte_memzone_free(hw->iomz);

	hw->iomz = NULL;
	hw->sqe = NULL;
	hw->cqe = NULL;
	hw->status = NULL;
	hw->sqe_iova = 0;
	hw->cqe_iova = 0;
	hw->sq_depth_mask = 0;
	hw->cq_depth = 0;
}

static int
hisi_dma_info_get(const struct rte_dma_dev *dev,
		  struct rte_dma_info *dev_info,
		  uint32_t info_sz)
{
	struct hisi_dma_dev *hw = dev->data->dev_private;
	RTE_SET_USED(info_sz);

	dev_info->dev_capa = RTE_DMA_CAPA_MEM_TO_MEM |
			     RTE_DMA_CAPA_OPS_COPY;
	if (hw->reg_layout == HISI_DMA_REG_LAYOUT_HIP09)
		dev_info->dev_capa |= RTE_DMA_CAPA_HANDLES_ERRORS;

	dev_info->max_vchans = 1;
	dev_info->max_desc = HISI_DMA_MAX_DESC_NUM;
	dev_info->min_desc = HISI_DMA_MIN_DESC_NUM;

	return 0;
}

static int
hisi_dma_configure(struct rte_dma_dev *dev,
		   const struct rte_dma_conf *conf,
		   uint32_t conf_sz)
{
	RTE_SET_USED(dev);
	RTE_SET_USED(conf);
	RTE_SET_USED(conf_sz);
	return 0;
}

static int
hisi_dma_vchan_setup(struct rte_dma_dev *dev, uint16_t vchan,
		     const struct rte_dma_vchan_conf *conf,
		     uint32_t conf_sz)
{
	struct hisi_dma_dev *hw = dev->data->dev_private;
	int ret;

	RTE_SET_USED(vchan);
	RTE_SET_USED(conf_sz);

	if (!rte_is_power_of_2(conf->nb_desc)) {
		HISI_DMA_ERR(hw, "Number of desc must be power of 2!");
		return -EINVAL;
	}

	hisi_dma_free_iomem(hw);
	ret = hisi_dma_alloc_iomem(hw, conf->nb_desc, dev->data->dev_name);
	if (ret)
		return ret;

	return 0;
}

static int
hisi_dma_start(struct rte_dma_dev *dev)
{
	struct hisi_dma_dev *hw = dev->data->dev_private;

	if (hw->iomz == NULL) {
		HISI_DMA_ERR(hw, "Vchan was not setup, start fail!\n");
		return -EINVAL;
	}

	/* Reset the dmadev to a known state, include:
	 *   1) zero iomem, also include status fields.
	 *   2) init hardware register.
	 *   3) init index values to zero.
	 *   4) init running statistics.
	 */
	hisi_dma_zero_iomem(hw);
	hisi_dma_init_hw(hw);
	hw->ridx = 0;
	hw->cridx = 0;
	hw->sq_head = 0;
	hw->sq_tail = 0;
	hw->cq_sq_head = 0;
	hw->cq_head = 0;
	hw->cqs_completed = 0;
	hw->cqe_vld = 1;
	hw->submitted = 0;
	hw->completed = 0;
	hw->errors = 0;
	hw->qfulls = 0;

	hisi_dma_update_queue_bit(hw, HISI_DMA_QUEUE_CTRL0_REG,
				  HISI_DMA_QUEUE_CTRL0_EN_B, true);

	return 0;
}

static int
hisi_dma_stop(struct rte_dma_dev *dev)
{
	return hisi_dma_reset_hw(dev->data->dev_private);
}

static int
hisi_dma_close(struct rte_dma_dev *dev)
{
	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		/* The dmadev already stopped */
		hisi_dma_free_iomem(dev->data->dev_private);
	}
	return 0;
}

static int
hisi_dma_stats_get(const struct rte_dma_dev *dev, uint16_t vchan,
		   struct rte_dma_stats *stats,
		   uint32_t stats_sz)
{
	struct hisi_dma_dev *hw = dev->data->dev_private;

	RTE_SET_USED(vchan);
	RTE_SET_USED(stats_sz);
	stats->submitted = hw->submitted;
	stats->completed = hw->completed;
	stats->errors = hw->errors;

	return 0;
}

static int
hisi_dma_stats_reset(struct rte_dma_dev *dev, uint16_t vchan)
{
	struct hisi_dma_dev *hw = dev->data->dev_private;

	RTE_SET_USED(vchan);
	hw->submitted = 0;
	hw->completed = 0;
	hw->errors = 0;
	hw->qfulls = 0;

	return 0;
}

static void
hisi_dma_dump_range(struct hisi_dma_dev *hw, FILE *f, uint32_t start,
		    uint32_t end)
{
#define DUMP_REGNUM_PER_LINE	4

	uint32_t cnt, i;

	cnt = 0;
	for (i = start; i <= end; i += sizeof(uint32_t)) {
		if (cnt % DUMP_REGNUM_PER_LINE == 0)
			(void)fprintf(f, "      [%4x]:", i);
		(void)fprintf(f, " 0x%08x", hisi_dma_read_dev(hw, i));
		cnt++;
		if (cnt % DUMP_REGNUM_PER_LINE == 0)
			(void)fprintf(f, "\n");
	}
	if (cnt % DUMP_REGNUM_PER_LINE)
		(void)fprintf(f, "\n");
}

static void
hisi_dma_dump_common(struct hisi_dma_dev *hw, FILE *f)
{
	struct {
		uint8_t reg_layout;
		uint32_t start;
		uint32_t end;
	} reg_info[] = {
		{ HISI_DMA_REG_LAYOUT_HIP08,
		  HISI_DMA_HIP08_DUMP_START_REG,
		  HISI_DMA_HIP08_DUMP_END_REG },
		{ HISI_DMA_REG_LAYOUT_HIP09,
		  HISI_DMA_HIP09_DUMP_REGION_A_START_REG,
		  HISI_DMA_HIP09_DUMP_REGION_A_END_REG },
		{ HISI_DMA_REG_LAYOUT_HIP09,
		  HISI_DMA_HIP09_DUMP_REGION_B_START_REG,
		  HISI_DMA_HIP09_DUMP_REGION_B_END_REG },
		{ HISI_DMA_REG_LAYOUT_HIP09,
		  HISI_DMA_HIP09_DUMP_REGION_C_START_REG,
		  HISI_DMA_HIP09_DUMP_REGION_C_END_REG },
		{ HISI_DMA_REG_LAYOUT_HIP09,
		  HISI_DMA_HIP09_DUMP_REGION_D_START_REG,
		  HISI_DMA_HIP09_DUMP_REGION_D_END_REG },
	};
	uint32_t i;

	(void)fprintf(f, "    common-register:\n");
	for (i = 0; i < RTE_DIM(reg_info); i++) {
		if (hw->reg_layout != reg_info[i].reg_layout)
			continue;
		hisi_dma_dump_range(hw, f, reg_info[i].start, reg_info[i].end);
	}
}

static void
hisi_dma_dump_read_queue(struct hisi_dma_dev *hw, uint32_t qoff,
			 char *buffer, int max_sz)
{
	memset(buffer, 0, max_sz);

	/* Address-related registers are not printed for security reasons. */
	if (qoff == HISI_DMA_QUEUE_SQ_BASE_L_REG ||
	    qoff == HISI_DMA_QUEUE_SQ_BASE_H_REG ||
	    qoff == HISI_DMA_QUEUE_CQ_BASE_L_REG ||
	    qoff == HISI_DMA_QUEUE_CQ_BASE_H_REG) {
		(void)snprintf(buffer, max_sz, "**********");
		return;
	}

	(void)snprintf(buffer, max_sz, "0x%08x", hisi_dma_read_queue(hw, qoff));
}

static void
hisi_dma_dump_queue(struct hisi_dma_dev *hw, FILE *f)
{
#define REG_FMT_LEN	32
	char buf[REG_FMT_LEN] = { 0 };
	uint32_t i;

	(void)fprintf(f, "    queue-register:\n");
	for (i = 0; i < HISI_DMA_QUEUE_REGION_SIZE; ) {
		hisi_dma_dump_read_queue(hw, i, buf, sizeof(buf));
		(void)fprintf(f, "      [%2x]: %s", i, buf);
		i += sizeof(uint32_t);
		hisi_dma_dump_read_queue(hw, i, buf, sizeof(buf));
		(void)fprintf(f, " %s", buf);
		i += sizeof(uint32_t);
		hisi_dma_dump_read_queue(hw, i, buf, sizeof(buf));
		(void)fprintf(f, " %s", buf);
		i += sizeof(uint32_t);
		hisi_dma_dump_read_queue(hw, i, buf, sizeof(buf));
		(void)fprintf(f, " %s\n", buf);
		i += sizeof(uint32_t);
	}
}

static int
hisi_dma_dump(const struct rte_dma_dev *dev, FILE *f)
{
	struct hisi_dma_dev *hw = dev->data->dev_private;

	(void)fprintf(f,
		"    revision: 0x%x queue_id: %u ring_size: %u\n"
		"    ridx: %u cridx: %u\n"
		"    sq_head: %u sq_tail: %u cq_sq_head: %u\n"
		"    cq_head: %u cqs_completed: %u cqe_vld: %u\n"
		"    submitted: %" PRIu64 " completed: %" PRIu64 " errors: %"
		PRIu64 " qfulls: %" PRIu64 "\n",
		hw->revision, hw->queue_id,
		hw->sq_depth_mask > 0 ? hw->sq_depth_mask + 1 : 0,
		hw->ridx, hw->cridx,
		hw->sq_head, hw->sq_tail, hw->cq_sq_head,
		hw->cq_head, hw->cqs_completed, hw->cqe_vld,
		hw->submitted, hw->completed, hw->errors, hw->qfulls);
	hisi_dma_dump_queue(hw, f);
	hisi_dma_dump_common(hw, f);

	return 0;
}

static int
hisi_dma_copy(void *dev_private, uint16_t vchan,
		 rte_iova_t src, rte_iova_t dst,
		 uint32_t length, uint64_t flags)
{
	struct hisi_dma_dev *hw = dev_private;
	struct hisi_dma_sqe *sqe = &hw->sqe[hw->sq_tail];

	RTE_SET_USED(vchan);

	if (((hw->sq_tail + 1) & hw->sq_depth_mask) == hw->sq_head) {
		hw->qfulls++;
		return -ENOSPC;
	}

	sqe->dw0 = rte_cpu_to_le_32(SQE_OPCODE_M2M);
	sqe->dw1 = 0;
	sqe->dw2 = 0;
	sqe->length = rte_cpu_to_le_32(length);
	sqe->src_addr = rte_cpu_to_le_64(src);
	sqe->dst_addr = rte_cpu_to_le_64(dst);
	hw->sq_tail = (hw->sq_tail + 1) & hw->sq_depth_mask;
	hw->submitted++;

	if (flags & RTE_DMA_OP_FLAG_FENCE)
		sqe->dw0 |= rte_cpu_to_le_32(SQE_FENCE_FLAG);
	if (flags & RTE_DMA_OP_FLAG_SUBMIT)
		rte_write32(rte_cpu_to_le_32(hw->sq_tail), hw->sq_tail_reg);

	return hw->ridx++;
}

static int
hisi_dma_submit(void *dev_private, uint16_t vchan)
{
	struct hisi_dma_dev *hw = dev_private;

	RTE_SET_USED(vchan);
	rte_write32(rte_cpu_to_le_32(hw->sq_tail), hw->sq_tail_reg);

	return 0;
}

static inline void
hisi_dma_scan_cq(struct hisi_dma_dev *hw)
{
	volatile struct hisi_dma_cqe *cqe;
	uint16_t csq_head = hw->cq_sq_head;
	uint16_t cq_head = hw->cq_head;
	uint16_t count = 0;
	uint64_t misc;

	while (true) {
		cqe = &hw->cqe[cq_head];
		misc = cqe->misc;
		misc = rte_le_to_cpu_64(misc);
		if (FIELD_GET(CQE_VALID_B, misc) != hw->cqe_vld)
			break;

		csq_head = FIELD_GET(CQE_SQ_HEAD_MASK, misc);
		if (unlikely(misc & CQE_STATUS_MASK))
			hw->status[csq_head] = FIELD_GET(CQE_STATUS_MASK,
							 misc);

		count++;
		cq_head++;
		if (cq_head == hw->cq_depth) {
			hw->cqe_vld = !hw->cqe_vld;
			cq_head = 0;
		}
	}

	if (count == 0)
		return;

	hw->cq_head = cq_head;
	hw->cq_sq_head = (csq_head + 1) & hw->sq_depth_mask;
	hw->cqs_completed += count;
	if (hw->cqs_completed >= HISI_DMA_CQ_RESERVED) {
		rte_write32(rte_cpu_to_le_32(cq_head), hw->cq_head_reg);
		hw->cqs_completed = 0;
	}
}

static inline uint16_t
hisi_dma_calc_cpls(struct hisi_dma_dev *hw, const uint16_t nb_cpls)
{
	uint16_t cpl_num;

	if (hw->cq_sq_head >= hw->sq_head)
		cpl_num = hw->cq_sq_head - hw->sq_head;
	else
		cpl_num = hw->sq_depth_mask + 1 - hw->sq_head + hw->cq_sq_head;

	if (cpl_num > nb_cpls)
		cpl_num = nb_cpls;

	return cpl_num;
}

static uint16_t
hisi_dma_completed(void *dev_private,
		   uint16_t vchan, const uint16_t nb_cpls,
		   uint16_t *last_idx, bool *has_error)
{
	struct hisi_dma_dev *hw = dev_private;
	uint16_t sq_head = hw->sq_head;
	uint16_t cpl_num, i;

	RTE_SET_USED(vchan);
	hisi_dma_scan_cq(hw);

	cpl_num = hisi_dma_calc_cpls(hw, nb_cpls);
	for (i = 0; i < cpl_num; i++) {
		if (hw->status[sq_head]) {
			*has_error = true;
			break;
		}
		sq_head = (sq_head + 1) & hw->sq_depth_mask;
	}
	if (i > 0) {
		hw->cridx += i;
		*last_idx = hw->cridx - 1;
		hw->sq_head = sq_head;
	}
	hw->completed += i;

	return i;
}

static enum rte_dma_status_code
hisi_dma_convert_status(uint16_t status)
{
	switch (status) {
	case HISI_DMA_STATUS_SUCCESS:
		return RTE_DMA_STATUS_SUCCESSFUL;
	case HISI_DMA_STATUS_INVALID_OPCODE:
		return RTE_DMA_STATUS_INVALID_OPCODE;
	case HISI_DMA_STATUS_INVALID_LENGTH:
		return RTE_DMA_STATUS_INVALID_LENGTH;
	case HISI_DMA_STATUS_USER_ABORT:
		return RTE_DMA_STATUS_USER_ABORT;
	case HISI_DMA_STATUS_REMOTE_READ_ERROR:
	case HISI_DMA_STATUS_AXI_READ_ERROR:
		return RTE_DMA_STATUS_BUS_READ_ERROR;
	case HISI_DMA_STATUS_AXI_WRITE_ERROR:
		return RTE_DMA_STATUS_BUS_WRITE_ERROR;
	case HISI_DMA_STATUS_DATA_POISON:
	case HISI_DMA_STATUS_REMOTE_DATA_POISION:
		return RTE_DMA_STATUS_DATA_POISION;
	case HISI_DMA_STATUS_SQE_READ_ERROR:
	case HISI_DMA_STATUS_SQE_READ_POISION:
		return RTE_DMA_STATUS_DESCRIPTOR_READ_ERROR;
	case HISI_DMA_STATUS_LINK_DOWN_ERROR:
		return RTE_DMA_STATUS_DEV_LINK_ERROR;
	default:
		return RTE_DMA_STATUS_ERROR_UNKNOWN;
	}
}

static uint16_t
hisi_dma_completed_status(void *dev_private,
			  uint16_t vchan, const uint16_t nb_cpls,
			  uint16_t *last_idx, enum rte_dma_status_code *status)
{
	struct hisi_dma_dev *hw = dev_private;
	uint16_t sq_head = hw->sq_head;
	uint16_t cpl_num, i;

	RTE_SET_USED(vchan);
	hisi_dma_scan_cq(hw);

	cpl_num = hisi_dma_calc_cpls(hw, nb_cpls);
	for (i = 0; i < cpl_num; i++) {
		status[i] = hisi_dma_convert_status(hw->status[sq_head]);
		hw->errors += !!status[i];
		hw->status[sq_head] = HISI_DMA_STATUS_SUCCESS;
		sq_head = (sq_head + 1) & hw->sq_depth_mask;
	}
	if (likely(cpl_num > 0)) {
		hw->cridx += cpl_num;
		*last_idx = hw->cridx - 1;
		hw->sq_head = sq_head;
	}
	hw->completed += cpl_num;

	return cpl_num;
}

static uint16_t
hisi_dma_burst_capacity(const void *dev_private, uint16_t vchan)
{
	const struct hisi_dma_dev *hw = dev_private;
	uint16_t sq_head = hw->sq_head;
	uint16_t sq_tail = hw->sq_tail;

	RTE_SET_USED(vchan);

	return (sq_tail >= sq_head) ? hw->sq_depth_mask - sq_tail + sq_head :
				      sq_head - 1 - sq_tail;
}

static void
hisi_dma_gen_dev_name(const struct rte_pci_device *pci_dev,
		      uint8_t queue_id, char *dev_name, size_t size)
{
	char name[RTE_DEV_NAME_MAX_LEN] = { 0 };

	memset(dev_name, 0, size);
	rte_pci_device_name(&pci_dev->addr, name, sizeof(name));
	(void)snprintf(dev_name, size, "%s-ch%u", name, queue_id);
}

/**
 * Hardware queue state machine:
 *
 *   -----------  dmadev_create	  ------------------
 *   | Unknown | ---------------> |      IDLE      |
 *   -----------                  ------------------
 *                                   ^          |
 *                                   |          |dev_start
 *                           dev_stop|          |
 *                                   |          v
 *                                ------------------
 *                                |      RUN       |
 *                                ------------------
 *
 */
static const struct rte_dma_dev_ops hisi_dmadev_ops = {
	.dev_info_get     = hisi_dma_info_get,
	.dev_configure    = hisi_dma_configure,
	.dev_start        = hisi_dma_start,
	.dev_stop         = hisi_dma_stop,
	.dev_close        = hisi_dma_close,
	.vchan_setup      = hisi_dma_vchan_setup,
	.stats_get        = hisi_dma_stats_get,
	.stats_reset      = hisi_dma_stats_reset,
	.dev_dump         = hisi_dma_dump,
};

static int
hisi_dma_create(struct rte_pci_device *pci_dev, uint8_t queue_id,
		uint8_t revision)
{
#define REG_PCI_BAR_INDEX	2

	char name[RTE_DEV_NAME_MAX_LEN];
	struct rte_dma_dev *dev;
	struct hisi_dma_dev *hw;
	int ret;

	hisi_dma_gen_dev_name(pci_dev, queue_id, name, sizeof(name));
	dev = rte_dma_pmd_allocate(name, pci_dev->device.numa_node,
				   sizeof(*hw));
	if (dev == NULL) {
		HISI_DMA_LOG(ERR, "%s allocate dmadev fail!", name);
		return -EINVAL;
	}

	dev->device = &pci_dev->device;
	dev->dev_ops = &hisi_dmadev_ops;
	dev->fp_obj->dev_private = dev->data->dev_private;
	dev->fp_obj->copy = hisi_dma_copy;
	dev->fp_obj->submit = hisi_dma_submit;
	dev->fp_obj->completed = hisi_dma_completed;
	dev->fp_obj->completed_status = hisi_dma_completed_status;
	dev->fp_obj->burst_capacity = hisi_dma_burst_capacity;

	hw = dev->data->dev_private;
	hw->data = dev->data;
	hw->revision = revision;
	hw->reg_layout = hisi_dma_reg_layout(revision);
	hw->io_base = pci_dev->mem_resource[REG_PCI_BAR_INDEX].addr;
	hw->queue_id = queue_id;
	hw->sq_tail_reg = hisi_dma_queue_regaddr(hw,
						 HISI_DMA_QUEUE_SQ_TAIL_REG);
	hw->cq_head_reg = hisi_dma_queue_regaddr(hw,
						 HISI_DMA_QUEUE_CQ_HEAD_REG);

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		ret = hisi_dma_reset_hw(hw);
		if (ret) {
			HISI_DMA_LOG(ERR, "%s init device fail!", name);
			(void)rte_dma_pmd_release(name);
			return -EIO;
		}
	}

	dev->state = RTE_DMA_DEV_READY;
	HISI_DMA_LOG(DEBUG, "%s create dmadev success!", name);

	return 0;
}

static int
hisi_dma_check_revision(struct rte_pci_device *pci_dev, const char *name,
			uint8_t *out_revision)
{
	uint8_t revision;
	int ret;

	ret = rte_pci_read_config(pci_dev, &revision, 1,
				  HISI_DMA_PCI_REVISION_ID_REG);
	if (ret != 1) {
		HISI_DMA_LOG(ERR, "%s read PCI revision failed!", name);
		return -EINVAL;
	}
	if (hisi_dma_reg_layout(revision) == HISI_DMA_REG_LAYOUT_INVALID) {
		HISI_DMA_LOG(ERR, "%s revision: 0x%x not supported!",
			     name, revision);
		return -EINVAL;
	}

	*out_revision = revision;
	return 0;
}

static int
hisi_dma_probe(struct rte_pci_driver *pci_drv __rte_unused,
	       struct rte_pci_device *pci_dev)
{
	char name[RTE_DEV_NAME_MAX_LEN] = { 0 };
	uint8_t revision;
	uint8_t i;
	int ret;

	rte_pci_device_name(&pci_dev->addr, name, sizeof(name));

	if (pci_dev->mem_resource[2].addr == NULL) {
		HISI_DMA_LOG(ERR, "%s BAR2 is NULL!\n", name);
		return -ENODEV;
	}

	ret = hisi_dma_check_revision(pci_dev, name, &revision);
	if (ret)
		return ret;
	HISI_DMA_LOG(DEBUG, "%s read PCI revision: 0x%x", name, revision);

	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		hisi_dma_init_gbl(pci_dev->mem_resource[2].addr, revision);

	for (i = 0; i < HISI_DMA_MAX_HW_QUEUES; i++) {
		ret = hisi_dma_create(pci_dev, i, revision);
		if (ret) {
			HISI_DMA_LOG(ERR, "%s create dmadev %u failed!",
				     name, i);
			break;
		}
	}

	return ret;
}

static int
hisi_dma_remove(struct rte_pci_device *pci_dev)
{
	char name[RTE_DEV_NAME_MAX_LEN];
	uint8_t i;
	int ret;

	for (i = 0; i < HISI_DMA_MAX_HW_QUEUES; i++) {
		hisi_dma_gen_dev_name(pci_dev, i, name, sizeof(name));
		ret = rte_dma_pmd_release(name);
		if (ret)
			return ret;
	}

	return 0;
}

static const struct rte_pci_id pci_id_hisi_dma_map[] = {
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_HUAWEI, HISI_DMA_DEVICE_ID) },
	{ .vendor_id = 0, }, /* sentinel */
};

static struct rte_pci_driver hisi_dma_pmd_drv = {
	.id_table  = pci_id_hisi_dma_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe     = hisi_dma_probe,
	.remove    = hisi_dma_remove,
};

RTE_PMD_REGISTER_PCI(dma_hisilicon, hisi_dma_pmd_drv);
RTE_PMD_REGISTER_PCI_TABLE(dma_hisilicon, pci_id_hisi_dma_map);
RTE_PMD_REGISTER_KMOD_DEP(dma_hisilicon, "vfio-pci");
