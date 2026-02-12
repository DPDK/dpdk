/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2025 HiSilicon Technologies Co., Ltd. All rights reserved.
 */

#include <errno.h>
#include <inttypes.h>
#include <string.h>
#include <sys/ioctl.h>

#include <rte_byteorder.h>
#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_io.h>
#include <rte_kvargs.h>
#include <rte_log.h>
#include <rte_malloc.h>

#include <rte_dmadev_pmd.h>

#include "hisi_acc_dmadev.h"

RTE_LOG_REGISTER_DEFAULT(hacc_dma_logtype, INFO);
#define RTE_LOGTYPE_HACC_DMA hacc_dma_logtype
#define HACC_DMA_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, HACC_DMA, "%s(): ", __func__, __VA_ARGS__)
#define HACC_DMA_DEV_LOG(hw, level, ...) \
	RTE_LOG_LINE_PREFIX(level, HACC_DMA, "%s %s(): ", \
		(hw)->data->dev_name RTE_LOG_COMMA __func__, __VA_ARGS__)
#define HACC_DMA_DEBUG(hw, ...) \
	HACC_DMA_DEV_LOG(hw, DEBUG, __VA_ARGS__)
#define HACC_DMA_INFO(hw, ...) \
	HACC_DMA_DEV_LOG(hw, INFO, __VA_ARGS__)
#define HACC_DMA_WARN(hw, ...) \
	HACC_DMA_DEV_LOG(hw, WARNING, __VA_ARGS__)
#define HACC_DMA_ERR(hw, ...) \
	HACC_DMA_DEV_LOG(hw, ERR, __VA_ARGS__)

static int
hacc_dma_info_get(const struct rte_dma_dev *dev,
		  struct rte_dma_info *dev_info,
		  uint32_t info_sz)
{
	struct hacc_dma_dev *hw = dev->data->dev_private;

	RTE_SET_USED(info_sz);

	dev_info->dev_capa = RTE_DMA_CAPA_MEM_TO_MEM |
			     RTE_DMA_CAPA_SVA |
			     RTE_DMA_CAPA_OPS_COPY |
			     RTE_DMA_CAPA_OPS_FILL;
	dev_info->max_vchans = 1;
	dev_info->max_desc = hw->sq_depth;
	dev_info->min_desc = hw->sq_depth;

	return 0;
}

static int
hacc_dma_configure(struct rte_dma_dev *dev,
		   const struct rte_dma_conf *conf,
		   uint32_t conf_sz)
{
	RTE_SET_USED(dev);
	RTE_SET_USED(conf);
	RTE_SET_USED(conf_sz);
	return 0;
}

static int
hacc_dma_start(struct rte_dma_dev *dev)
{
	struct hacc_dma_dev *hw = dev->data->dev_private;
	int ret;

	if ((*hw->sq_status != 0) || (*hw->cq_status != 0)) {
		HACC_DMA_ERR(hw, "detect dev is abnormal!");
		return -EIO;
	}

	if (hw->started) {
		hw->ridx = 0;
		hw->cridx = 0;
		hw->stop_proc = 0;
		return 0;
	}

	memset(hw->sqe, 0, hw->sqe_size * hw->sq_depth);
	memset(hw->cqe, 0, sizeof(struct hacc_dma_cqe) * hw->cq_depth);
	memset(hw->status, 0, sizeof(uint16_t) * hw->sq_depth);
	hw->ridx = 0;
	hw->cridx = 0;
	hw->sq_head = 0;
	hw->sq_tail = 0;
	hw->cq_sq_head = 0;
	hw->avail_sqes = hw->sq_depth - HACC_DMA_SQ_GAP_NUM - 1;
	hw->cq_head = 0;
	hw->cqs_completed = 0;
	hw->cqe_vld = 1;
	hw->stop_proc = 0;
	hw->submitted = 0;
	hw->completed = 0;
	hw->errors = 0;
	hw->invalid_lens = 0;
	hw->qfulls = 0;

	ret = rte_uacce_queue_start(&hw->qctx);
	if (ret == 0)
		hw->started = true;

	return ret;
}

static int
hacc_dma_stop(struct rte_dma_dev *dev)
{
#define MAX_WAIT_MSEC	1000
#define MAX_CPL_NUM	64
	struct hacc_dma_dev *hw = dev->data->dev_private;
	uint32_t wait_msec = 0;

	/* Flag stop processing new requests. */
	hw->stop_proc = 1;
	/* Currently, there is no method to notify the hardware to stop.
	 * Therefore, the timeout mechanism is used to wait for the dataplane
	 * to stop.
	 */
	while (hw->sq_head != hw->sq_tail && wait_msec++ < MAX_WAIT_MSEC) {
		if ((*hw->sq_status != 0) || (*hw->cq_status != 0)) {
			/* This indicates that the dev is abnormal. The correct error handling
			 * is to close the dev (so that kernel module will perform error handling)
			 * and apply for a new dev.
			 * If an error code is returned here, the dev cannot be closed. Therefore,
			 * zero is returned and an error trace is added.
			 */
			HACC_DMA_ERR(hw, "detect dev is abnormal!");
			return 0;
		}
		rte_delay_ms(1);
	}
	if (hw->sq_head != hw->sq_tail) {
		HACC_DMA_ERR(hw, "dev is still active!");
		return -EBUSY;
	}

	return 0;
}

static int
hacc_dma_close(struct rte_dma_dev *dev)
{
	struct hacc_dma_dev *hw = dev->data->dev_private;
	/* The dmadev already stopped */
	rte_free(hw->status);
	rte_uacce_queue_unmap(&hw->qctx, RTE_UACCE_QFRT_DUS);
	rte_uacce_queue_unmap(&hw->qctx, RTE_UACCE_QFRT_MMIO);
	rte_uacce_queue_free(&hw->qctx);
	return 0;
}

static int
hacc_dma_vchan_setup(struct rte_dma_dev *dev, uint16_t vchan,
		     const struct rte_dma_vchan_conf *conf,
		     uint32_t conf_sz)
{
	RTE_SET_USED(dev);
	RTE_SET_USED(vchan);
	RTE_SET_USED(conf);
	RTE_SET_USED(conf_sz);
	return 0;
}

static int
hacc_dma_stats_get(const struct rte_dma_dev *dev, uint16_t vchan,
		   struct rte_dma_stats *stats,
		   uint32_t stats_sz)
{
	struct hacc_dma_dev *hw = dev->data->dev_private;

	RTE_SET_USED(vchan);
	RTE_SET_USED(stats_sz);
	stats->submitted = hw->submitted;
	stats->completed = hw->completed;
	stats->errors    = hw->errors;

	return 0;
}

static int
hacc_dma_stats_reset(struct rte_dma_dev *dev, uint16_t vchan)
{
	struct hacc_dma_dev *hw = dev->data->dev_private;

	RTE_SET_USED(vchan);
	hw->submitted    = 0;
	hw->completed    = 0;
	hw->errors       = 0;
	hw->invalid_lens = 0;
	hw->io_errors    = 0;
	hw->qfulls       = 0;

	return 0;
}

static int
hacc_dma_dump(const struct rte_dma_dev *dev, FILE *f)
{
	struct hacc_dma_dev *hw = dev->data->dev_private;

	fprintf(f, "  sqn: %u sq_status: %s cq_status: %s stop_proc: %u\n"
		"  sqe_size: %u sq_depth: %u sq_depth_mask: %u cq_depth: %u\n",
		hw->sqn, (*hw->sq_status != 0) ? "ERR" : "OK",
		(*hw->cq_status != 0) ? "ERR" : "OK",
		hw->stop_proc,
		hw->sqe_size, hw->sq_depth, hw->sq_depth_mask, hw->cq_depth);
	fprintf(f, "  ridx: %u cridx: %u\n"
		"  sq_head: %u sq_tail: %u cq_sq_head: %u avail_sqes: %u\n"
		"  cq_head: %u cqs_completed: %u cqe_vld: %u\n",
		hw->ridx, hw->cridx,
		hw->sq_head, hw->sq_tail, hw->cq_sq_head, hw->avail_sqes,
		hw->cq_head, hw->cqs_completed, hw->cqe_vld);
	fprintf(f, "  submitted: %" PRIu64 " completed: %" PRIu64 " errors: %" PRIu64
		" invalid_lens: %" PRIu64 " io_errors: %" PRIu64 " qfulls: %" PRIu64 "\n",
		hw->submitted, hw->completed, hw->errors, hw->invalid_lens,
		hw->io_errors, hw->qfulls);

	return 0;
}

static inline void
hacc_dma_sq_doorbell(struct hacc_dma_dev *hw)
{
	uint64_t doorbell = (uint64_t)(hw->sqn & HACC_DMA_DOORBELL_SQN_MASK) |
			    (HACC_DMA_DOORBELL_SQ_CMD << HACC_DMA_DOORBELL_CMD_SHIFT) |
			    (((uint64_t)hw->sq_tail) << HACC_DMA_DOORBELL_IDX_SHIFT);
	rte_io_wmb();
	*(volatile uint64_t *)hw->doorbell_reg = doorbell;
}

static int
hacc_dma_copy(void *dev_private, uint16_t vchan, rte_iova_t src, rte_iova_t dst,
	      uint32_t length, uint64_t flags)
{
	struct hacc_dma_dev *hw = dev_private;
	struct hacc_dma_sqe *sqe = &hw->sqe[hw->sq_tail];

	RTE_SET_USED(vchan);

	if (unlikely(hw->stop_proc > 0))
		return -EPERM;

	if (unlikely(length > HACC_DMA_MAX_OP_SIZE)) {
		hw->invalid_lens++;
		return -EINVAL;
	}

	if (unlikely(*hw->sq_status != 0)) {
		hw->io_errors++;
		return -EIO;
	}

	if (hw->avail_sqes == 0) {
		hw->qfulls++;
		return -ENOSPC;
	}

	sqe->bd_type       = HACC_DMA_SQE_TYPE;
	sqe->task_type     = HACC_DMA_TASK_TYPE;
	sqe->task_type_ext = HACC_DMA_DATA_MEMCPY;
	sqe->init_val      = 0;
	sqe->addr_array    = src;
	sqe->dst_addr      = dst;
	sqe->data_size     = length;
	sqe->dw0           = HACC_DMA_SVA_PREFETCH_EN;
	sqe->wb_field      = 0;

	hw->sq_tail = (hw->sq_tail + 1) & hw->sq_depth_mask;
	hw->avail_sqes--;
	hw->submitted++;

	if (flags & RTE_DMA_OP_FLAG_SUBMIT)
		hacc_dma_sq_doorbell(hw);

	return hw->ridx++;
}

static int
hacc_dma_fill(void *dev_private, uint16_t vchan, uint64_t pattern,
	      rte_iova_t dst, uint32_t length, uint64_t flags)
{
	struct hacc_dma_dev *hw = dev_private;
	struct hacc_dma_sqe *sqe = &hw->sqe[hw->sq_tail];

	RTE_SET_USED(vchan);

	if (unlikely(hw->stop_proc > 0))
		return -EPERM;

	if (unlikely(length > HACC_DMA_MAX_OP_SIZE)) {
		hw->invalid_lens++;
		return -EINVAL;
	}

	if (unlikely(*hw->sq_status != 0)) {
		hw->io_errors++;
		return -EIO;
	}

	if (hw->avail_sqes == 0) {
		hw->qfulls++;
		return -ENOSPC;
	}

	sqe->bd_type       = HACC_DMA_SQE_TYPE;
	sqe->task_type     = HACC_DMA_TASK_TYPE;
	sqe->task_type_ext = HACC_DMA_DATA_MEMSET;
	sqe->init_val      = pattern;
	sqe->addr_array    = 0;
	sqe->dst_addr      = dst;
	sqe->data_size     = length;
	sqe->dw0           = HACC_DMA_SVA_PREFETCH_EN;
	sqe->wb_field      = 0;

	hw->sq_tail = (hw->sq_tail + 1) & hw->sq_depth_mask;
	hw->avail_sqes--;
	hw->submitted++;

	if (flags & RTE_DMA_OP_FLAG_SUBMIT)
		hacc_dma_sq_doorbell(hw);

	return hw->ridx++;
}

static int
hacc_dma_submit(void *dev_private, uint16_t vchan)
{
	struct hacc_dma_dev *hw = dev_private;

	RTE_SET_USED(vchan);

	if (unlikely(*hw->sq_status != 0)) {
		hw->io_errors++;
		return -EIO;
	}

	hacc_dma_sq_doorbell(hw);

	return 0;
}

static inline void
hacc_dma_cq_doorbell(struct hacc_dma_dev *hw)
{
	uint64_t doorbell = (uint64_t)(hw->sqn & HACC_DMA_DOORBELL_SQN_MASK) |
			    (HACC_DMA_DOORBELL_CQ_CMD << HACC_DMA_DOORBELL_CMD_SHIFT) |
			    (((uint64_t)hw->cq_head) << HACC_DMA_DOORBELL_IDX_SHIFT);
	rte_io_wmb();
	*(volatile uint64_t *)hw->doorbell_reg = doorbell;
}

static inline void
hacc_dma_scan_cq(struct hacc_dma_dev *hw)
{
	volatile struct hacc_dma_cqe *cqe;
	struct hacc_dma_sqe *sqe;
	uint16_t csq_head = hw->cq_sq_head;
	uint16_t cq_head = hw->cq_head;
	uint16_t count = 0;
	uint64_t misc;

	if (unlikely(*hw->cq_status != 0)) {
		hw->io_errors++;
		return;
	}

	while (count < hw->cq_depth) {
		cqe = &hw->cqe[cq_head];
		misc = cqe->misc;
		misc = rte_le_to_cpu_64(misc);
		if (RTE_FIELD_GET64(HACC_DMA_CQE_VALID_B, misc) != hw->cqe_vld)
			break;

		csq_head = RTE_FIELD_GET64(HACC_DMA_SQ_HEAD_MASK, misc);
		if (unlikely(csq_head > hw->sq_depth_mask)) {
			/**
			 * Defensive programming to prevent overflow of the
			 * status array indexed by csq_head. Only error logs
			 * are used for prompting.
			 */
			HACC_DMA_ERR(hw, "invalid csq_head: %u!", csq_head);
			count = 0;
			break;
		}
		sqe = &hw->sqe[csq_head];
		if (sqe->done_flag != HACC_DMA_TASK_DONE ||
			sqe->err_type || sqe->ext_err_type || sqe->wtype) {
			hw->status[csq_head] = RTE_DMA_STATUS_ERROR_UNKNOWN;
		}

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
	hw->avail_sqes += count;
	hw->cqs_completed += count;
	if (hw->cqs_completed >= HACC_DMA_CQ_DOORBELL_PACE) {
		hacc_dma_cq_doorbell(hw);
		hw->cqs_completed = 0;
	}
}

static inline uint16_t
hacc_dma_calc_cpls(struct hacc_dma_dev *hw, const uint16_t nb_cpls)
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
hacc_dma_completed(void *dev_private,
		   uint16_t vchan, const uint16_t nb_cpls,
		   uint16_t *last_idx, bool *has_error)
{
	struct hacc_dma_dev *hw = dev_private;
	uint16_t sq_head = hw->sq_head;
	uint16_t cpl_num, i;

	RTE_SET_USED(vchan);
	hacc_dma_scan_cq(hw);

	cpl_num = hacc_dma_calc_cpls(hw, nb_cpls);
	for (i = 0; i < cpl_num; i++) {
		if (hw->status[sq_head]) {
			*has_error = true;
			break;
		}
		sq_head = (sq_head + 1) & hw->sq_depth_mask;
	}
	*last_idx = hw->cridx + i - 1;
	if (i > 0) {
		hw->cridx += i;
		hw->sq_head = sq_head;
		hw->completed += i;
	}

	return i;
}

static uint16_t
hacc_dma_completed_status(void *dev_private,
			  uint16_t vchan, const uint16_t nb_cpls,
			  uint16_t *last_idx, enum rte_dma_status_code *status)
{
	struct hacc_dma_dev *hw = dev_private;
	uint16_t sq_head = hw->sq_head;
	uint16_t cpl_num, i;

	RTE_SET_USED(vchan);
	hacc_dma_scan_cq(hw);

	cpl_num = hacc_dma_calc_cpls(hw, nb_cpls);
	for (i = 0; i < cpl_num; i++) {
		status[i] = hw->status[sq_head];
		hw->errors += !!status[i];
		hw->status[sq_head] = 0;
		sq_head = (sq_head + 1) & hw->sq_depth_mask;
	}
	*last_idx = hw->cridx + cpl_num - 1;
	if (likely(cpl_num > 0)) {
		hw->cridx += cpl_num;
		hw->sq_head = sq_head;
		hw->completed += cpl_num;
	}

	return cpl_num;
}

static uint16_t
hacc_dma_burst_capacity(const void *dev_private, uint16_t vchan)
{
	const struct hacc_dma_dev *hw = dev_private;
	RTE_SET_USED(vchan);
	return hw->avail_sqes;
}

static const struct rte_dma_dev_ops hacc_dmadev_ops = {
	.dev_info_get     = hacc_dma_info_get,
	.dev_configure    = hacc_dma_configure,
	.dev_start        = hacc_dma_start,
	.dev_stop         = hacc_dma_stop,
	.dev_close        = hacc_dma_close,
	.vchan_setup      = hacc_dma_vchan_setup,
	.stats_get        = hacc_dma_stats_get,
	.stats_reset      = hacc_dma_stats_reset,
	.dev_dump         = hacc_dma_dump,
};

static void
hacc_dma_gen_dev_name(const struct rte_uacce_device *uacce_dev,
		      uint16_t queue_id, char *dev_name, size_t size)
{
	memset(dev_name, 0, size);
	(void)snprintf(dev_name, size, "%s-dma%u", uacce_dev->device.name, queue_id);
}

static void
hacc_dma_gen_dev_prefix(const struct rte_uacce_device *uacce_dev, char *dev_name, size_t size)
{
	memset(dev_name, 0, size);
	(void)snprintf(dev_name, size, "%s-dma", uacce_dev->device.name);
}

static int
hacc_dma_get_qp_info(struct hacc_dma_dev *hw)
{
#define CMD_QM_GET_QP_CTX	_IOWR('H', 10, struct hacc_dma_qp_contex)
#define CMD_QM_GET_QP_INFO	_IOWR('H', 11, struct hacc_dma_qp_info)
#define QP_ALG_TYPE		2
	struct hacc_dma_qp_contex {
		uint16_t id;
		uint16_t qc_type;
	} qp_ctx;
	struct hacc_dma_qp_info {
		uint32_t sqe_size;
		uint16_t sq_depth;
		uint16_t cq_depth;
		uint64_t reserved;
	} qp_info;
	int ret;

	memset(&qp_ctx, 0, sizeof(qp_ctx));
	qp_ctx.qc_type = QP_ALG_TYPE;
	ret = rte_uacce_queue_ioctl(&hw->qctx, CMD_QM_GET_QP_CTX, &qp_ctx);
	if (ret != 0) {
		HACC_DMA_ERR(hw, "get qm qp context fail!");
		return -EINVAL;
	}
	hw->sqn = qp_ctx.id;

	memset(&qp_info, 0, sizeof(qp_info));
	ret = rte_uacce_queue_ioctl(&hw->qctx, CMD_QM_GET_QP_INFO, &qp_info);
	if (ret != 0) {
		HACC_DMA_ERR(hw, "get qm qp info fail!");
		return -EINVAL;
	}
	if ((qp_info.sq_depth & (qp_info.sq_depth - 1)) != 0) {
		HACC_DMA_ERR(hw, "sq depth is not 2's power!");
		return -EINVAL;
	}
	hw->sqe_size = qp_info.sqe_size;
	hw->sq_depth = qp_info.sq_depth;
	hw->cq_depth = qp_info.cq_depth;
	hw->sq_depth_mask = hw->sq_depth - 1;

	return 0;
}

static int
hacc_dma_create(struct rte_uacce_device *uacce_dev, uint16_t queue_id)
{
	char name[RTE_DEV_NAME_MAX_LEN];
	struct rte_dma_dev *dev;
	struct hacc_dma_dev *hw;
	int ret;

	hacc_dma_gen_dev_name(uacce_dev, queue_id, name, sizeof(name));
	dev = rte_dma_pmd_allocate(name, uacce_dev->device.numa_node,
				   sizeof(struct hacc_dma_dev));
	if (dev == NULL) {
		HACC_DMA_LOG(ERR, "%s allocate dmadev fail!", name);
		return -ENOMEM;
	}

	dev->device = &uacce_dev->device;
	dev->dev_ops = &hacc_dmadev_ops;
	dev->fp_obj->dev_private = dev->data->dev_private;
	dev->fp_obj->copy = hacc_dma_copy;
	dev->fp_obj->fill = hacc_dma_fill;
	dev->fp_obj->submit = hacc_dma_submit;
	dev->fp_obj->completed = hacc_dma_completed;
	dev->fp_obj->completed_status = hacc_dma_completed_status;
	dev->fp_obj->burst_capacity = hacc_dma_burst_capacity;

	hw = dev->data->dev_private;
	hw->data = dev->data; /* make sure ACC_DMA_DEBUG/INFO/WARN/ERR was available. */

	ret = rte_uacce_queue_alloc(uacce_dev, &hw->qctx);
	if (ret != 0) {
		HACC_DMA_ERR(hw, "alloc queue fail!");
		goto release_dma_pmd;
	}

	ret = hacc_dma_get_qp_info(hw);
	if (ret != 0)
		goto free_uacce_queue;

	hw->io_base = rte_uacce_queue_mmap(&hw->qctx, RTE_UACCE_QFRT_MMIO);
	if (hw->io_base == NULL) {
		HACC_DMA_ERR(hw, "mmap MMIO region fail!");
		ret = -EINVAL;
		goto free_uacce_queue;
	}
	hw->doorbell_reg = (void *)((uintptr_t)hw->io_base + HACC_DMA_DOORBELL_OFFSET);

	hw->dus_base = rte_uacce_queue_mmap(&hw->qctx, RTE_UACCE_QFRT_DUS);
	if (hw->dus_base == NULL) {
		HACC_DMA_ERR(hw, "mmap DUS region fail!");
		ret = -EINVAL;
		goto unmap_mmio;
	}
	hw->sqe = hw->dus_base;
	hw->cqe = (void *)((uintptr_t)hw->dus_base + hw->sqe_size * hw->sq_depth);
	hw->sq_status = (uint32_t *)((uintptr_t)hw->dus_base +
			uacce_dev->qfrt_sz[RTE_UACCE_QFRT_DUS] - sizeof(uint32_t));
	hw->cq_status = hw->sq_status - 1;

	hw->status = rte_zmalloc_socket(NULL, sizeof(uint16_t) * hw->sq_depth,
					RTE_CACHE_LINE_SIZE, uacce_dev->numa_node);
	if (hw->status == NULL) {
		HACC_DMA_ERR(hw, "malloc status region fail!");
		ret = -ENOMEM;
		goto unmap_dus;
	}

	dev->state = RTE_DMA_DEV_READY;
	HACC_DMA_DEBUG(hw, "create dmadev %s success!", name);

	return 0;

unmap_dus:
	rte_uacce_queue_unmap(&hw->qctx, RTE_UACCE_QFRT_DUS);
unmap_mmio:
	rte_uacce_queue_unmap(&hw->qctx, RTE_UACCE_QFRT_MMIO);
free_uacce_queue:
	rte_uacce_queue_free(&hw->qctx);
release_dma_pmd:
	rte_dma_pmd_release(name);
	return ret;
}

static int
hacc_dma_parse_queues(const char *key, const char *value, void *extra_args)
{
	struct hacc_dma_config *config = extra_args;
	uint64_t val;
	char *end;

	RTE_SET_USED(key);

	errno = 0;
	val = strtoull(value, &end, 0);
	if (errno == ERANGE || value == end || *end != '\0' || val == 0) {
		HACC_DMA_LOG(ERR, "%s invalid queues! set to default one queue!",
			    config->dev->name);
		config->queues = HACC_DMA_DEFAULT_QUEUES;
	} else if (val > config->avail_queues) {
		HACC_DMA_LOG(WARNING, "%s exceed available queues! set to available queues %u",
			     config->dev->name, config->avail_queues);
		config->queues = config->avail_queues;
	} else {
		config->queues = val;
	}

	return 0;
}

static int
hacc_dma_parse_devargs(struct rte_uacce_device *uacce_dev, struct hacc_dma_config *config)
{
	struct rte_kvargs *kvlist;
	int avail_queues;

	avail_queues = rte_uacce_avail_queues(uacce_dev);
	if (avail_queues <= 0) {
		HACC_DMA_LOG(ERR, "%s don't have available queues!", uacce_dev->name);
		return -EINVAL;
	}
	config->dev = uacce_dev;
	config->avail_queues = avail_queues <= UINT16_MAX ? avail_queues : UINT16_MAX;

	if (uacce_dev->device.devargs == NULL)
		return 0;

	kvlist = rte_kvargs_parse(uacce_dev->device.devargs->args, NULL);
	if (kvlist == NULL)
		return 0;

	(void)rte_kvargs_process(kvlist, HACC_DMA_DEVARG_QUEUES, &hacc_dma_parse_queues, config);

	rte_kvargs_free(kvlist);

	return 0;
}

static int
hacc_dma_probe(struct rte_uacce_driver *dr, struct rte_uacce_device *uacce_dev)
{
	struct hacc_dma_config config = { .queues = HACC_DMA_DEFAULT_QUEUES };
	int ret = 0;
	uint32_t i;

	RTE_SET_USED(dr);

	ret = hacc_dma_parse_devargs(uacce_dev, &config);
	if (ret != 0)
		return ret;

	for (i = 0; i < config.queues; i++) {
		ret = hacc_dma_create(uacce_dev, i);
		if (ret != 0) {
			HACC_DMA_LOG(ERR, "%s create dmadev No.%u failed!", uacce_dev->name, i);
			break;
		}
	}

	if (ret != 0 && i > 0) {
		HACC_DMA_LOG(WARNING, "%s probed %u dmadev, can't probe more!", uacce_dev->name, i);
		ret = 0;
	}

	return ret;
}

static int
hacc_dma_remove(struct rte_uacce_device *uacce_dev)
{
	char name[RTE_DEV_NAME_MAX_LEN];
	struct rte_dma_info info;
	int i = 0;
	int ret;

	hacc_dma_gen_dev_prefix(uacce_dev, name, sizeof(name));
	RTE_DMA_FOREACH_DEV(i) {
		ret = rte_dma_info_get(i, &info);
		if (ret != 0)
			continue;
		if (strncmp(info.dev_name, name, strlen(name)) == 0)
			rte_dma_pmd_release(info.dev_name);
	}

	return 0;
}

static const struct rte_uacce_id hacc_dma_id_table[] = {
	{ "hisi_qm_v5", "udma" },
	{ .dev_api = NULL, },
};

static struct rte_uacce_driver hacc_dma_pmd_drv = {
	.id_table = hacc_dma_id_table,
	.probe    = hacc_dma_probe,
	.remove   = hacc_dma_remove,
};

RTE_PMD_REGISTER_UACCE(dma_hisi_acc, hacc_dma_pmd_drv);
RTE_PMD_REGISTER_PARAM_STRING(dma_hisi_acc,
			HACC_DMA_DEVARG_QUEUES "=<uint16> ");
