/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2025 HiSilicon Technologies Co., Ltd. All rights reserved.
 */

#include <errno.h>
#include <inttypes.h>
#include <string.h>
#include <sys/ioctl.h>

#include <rte_byteorder.h>
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
	dev->fp_obj->dev_private = dev->data->dev_private;

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
