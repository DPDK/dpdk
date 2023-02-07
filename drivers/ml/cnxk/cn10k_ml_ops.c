/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#include <rte_mldev.h>
#include <rte_mldev_pmd.h>

#include "cn10k_ml_dev.h"
#include "cn10k_ml_ops.h"

static void
qp_memzone_name_get(char *name, int size, int dev_id, int qp_id)
{
	snprintf(name, size, "cn10k_ml_qp_mem_%u:%u", dev_id, qp_id);
}

static int
cn10k_ml_qp_destroy(const struct rte_ml_dev *dev, struct cn10k_ml_qp *qp)
{
	const struct rte_memzone *qp_mem;
	char name[RTE_MEMZONE_NAMESIZE];
	int ret;

	qp_memzone_name_get(name, RTE_MEMZONE_NAMESIZE, dev->data->dev_id, qp->id);
	qp_mem = rte_memzone_lookup(name);
	ret = rte_memzone_free(qp_mem);
	if (ret)
		return ret;

	rte_free(qp);

	return 0;
}

static int
cn10k_ml_dev_queue_pair_release(struct rte_ml_dev *dev, uint16_t queue_pair_id)
{
	struct cn10k_ml_qp *qp;
	int ret;

	qp = dev->data->queue_pairs[queue_pair_id];
	if (qp == NULL)
		return -EINVAL;

	ret = cn10k_ml_qp_destroy(dev, qp);
	if (ret) {
		plt_err("Could not destroy queue pair %u", queue_pair_id);
		return ret;
	}

	dev->data->queue_pairs[queue_pair_id] = NULL;

	return 0;
}

static struct cn10k_ml_qp *
cn10k_ml_qp_create(const struct rte_ml_dev *dev, uint16_t qp_id, uint32_t nb_desc, int socket_id)
{
	const struct rte_memzone *qp_mem;
	char name[RTE_MEMZONE_NAMESIZE];
	struct cn10k_ml_qp *qp;
	uint32_t len;
	uint8_t *va;

	/* Allocate queue pair */
	qp = rte_zmalloc_socket("cn10k_ml_pmd_queue_pair", sizeof(struct cn10k_ml_qp), ROC_ALIGN,
				socket_id);
	if (qp == NULL) {
		plt_err("Could not allocate queue pair");
		return NULL;
	}

	/* For request queue */
	len = nb_desc * sizeof(struct cn10k_ml_req);
	qp_memzone_name_get(name, RTE_MEMZONE_NAMESIZE, dev->data->dev_id, qp_id);
	qp_mem = rte_memzone_reserve_aligned(
		name, len, socket_id, RTE_MEMZONE_SIZE_HINT_ONLY | RTE_MEMZONE_256MB, ROC_ALIGN);
	if (qp_mem == NULL) {
		plt_err("Could not reserve memzone: %s", name);
		goto qp_free;
	}

	va = qp_mem->addr;
	memset(va, 0, len);

	/* Initialize Request queue */
	qp->id = qp_id;
	qp->queue.reqs = (struct cn10k_ml_req *)va;
	qp->queue.head = 0;
	qp->queue.tail = 0;
	qp->queue.wait_cycles = ML_CN10K_CMD_TIMEOUT * plt_tsc_hz();
	qp->nb_desc = nb_desc;

	return qp;

qp_free:
	rte_free(qp);

	return NULL;
}

static int
cn10k_ml_dev_info_get(struct rte_ml_dev *dev, struct rte_ml_dev_info *dev_info)
{
	if (dev_info == NULL)
		return -EINVAL;

	memset(dev_info, 0, sizeof(struct rte_ml_dev_info));
	dev_info->driver_name = dev->device->driver->name;
	dev_info->max_models = ML_CN10K_MAX_MODELS;
	dev_info->max_queue_pairs = ML_CN10K_MAX_QP_PER_DEVICE;
	dev_info->max_desc = ML_CN10K_MAX_DESC_PER_QP;
	dev_info->max_segments = ML_CN10K_MAX_SEGMENTS;
	dev_info->min_align_size = ML_CN10K_ALIGN_SIZE;

	return 0;
}

static int
cn10k_ml_dev_configure(struct rte_ml_dev *dev, const struct rte_ml_dev_config *conf)
{
	struct rte_ml_dev_info dev_info;
	struct cn10k_ml_dev *mldev;
	struct cn10k_ml_qp *qp;
	uint32_t mz_size;
	uint16_t qp_id;
	int ret;

	if (dev == NULL || conf == NULL)
		return -EINVAL;

	/* Get CN10K device handle */
	mldev = dev->data->dev_private;

	cn10k_ml_dev_info_get(dev, &dev_info);
	if (conf->nb_models > dev_info.max_models) {
		plt_err("Invalid device config, nb_models > %u\n", dev_info.max_models);
		return -EINVAL;
	}

	if (conf->nb_queue_pairs > dev_info.max_queue_pairs) {
		plt_err("Invalid device config, nb_queue_pairs > %u\n", dev_info.max_queue_pairs);
		return -EINVAL;
	}

	if (mldev->state == ML_CN10K_DEV_STATE_PROBED) {
		plt_ml_dbg("Configuring ML device, nb_queue_pairs = %u, nb_models = %u",
			   conf->nb_queue_pairs, conf->nb_models);

		/* Load firmware */
		ret = cn10k_ml_fw_load(mldev);
		if (ret != 0)
			return ret;
	} else if (mldev->state == ML_CN10K_DEV_STATE_CONFIGURED) {
		plt_ml_dbg("Re-configuring ML device, nb_queue_pairs = %u, nb_models = %u",
			   conf->nb_queue_pairs, conf->nb_models);
	} else if (mldev->state == ML_CN10K_DEV_STATE_STARTED) {
		plt_err("Device can't be reconfigured in started state\n");
		return -ENOTSUP;
	} else if (mldev->state == ML_CN10K_DEV_STATE_CLOSED) {
		plt_err("Device can't be reconfigured after close\n");
		return -ENOTSUP;
	}

	/* Configure queue-pairs */
	if (dev->data->queue_pairs == NULL) {
		mz_size = sizeof(dev->data->queue_pairs[0]) * conf->nb_queue_pairs;
		dev->data->queue_pairs =
			rte_zmalloc("cn10k_mldev_queue_pairs", mz_size, RTE_CACHE_LINE_SIZE);
		if (dev->data->queue_pairs == NULL) {
			dev->data->nb_queue_pairs = 0;
			plt_err("Failed to get memory for queue_pairs, nb_queue_pairs %u",
				conf->nb_queue_pairs);
			return -ENOMEM;
		}
	} else { /* Re-configure */
		void **queue_pairs;

		/* Release all queue pairs as ML spec doesn't support queue_pair_destroy. */
		for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
			qp = dev->data->queue_pairs[qp_id];
			if (qp != NULL) {
				ret = cn10k_ml_dev_queue_pair_release(dev, qp_id);
				if (ret < 0)
					return ret;
			}
		}

		queue_pairs = dev->data->queue_pairs;
		queue_pairs =
			rte_realloc(queue_pairs, sizeof(queue_pairs[0]) * conf->nb_queue_pairs,
				    RTE_CACHE_LINE_SIZE);
		if (queue_pairs == NULL) {
			dev->data->nb_queue_pairs = 0;
			plt_err("Failed to realloc queue_pairs, nb_queue_pairs = %u",
				conf->nb_queue_pairs);
			ret = -ENOMEM;
			goto error;
		}

		memset(queue_pairs, 0, sizeof(queue_pairs[0]) * conf->nb_queue_pairs);
		dev->data->queue_pairs = queue_pairs;
	}
	dev->data->nb_queue_pairs = conf->nb_queue_pairs;

	mldev->state = ML_CN10K_DEV_STATE_CONFIGURED;

	return 0;

error:
	if (dev->data->queue_pairs != NULL)
		rte_free(dev->data->queue_pairs);

	return ret;
}

static int
cn10k_ml_dev_close(struct rte_ml_dev *dev)
{
	struct cn10k_ml_dev *mldev;
	struct cn10k_ml_qp *qp;
	uint16_t qp_id;

	if (dev == NULL)
		return -EINVAL;

	mldev = dev->data->dev_private;

	/* Destroy all queue pairs */
	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		qp = dev->data->queue_pairs[qp_id];
		if (qp != NULL) {
			if (cn10k_ml_qp_destroy(dev, qp) != 0)
				plt_err("Could not destroy queue pair %u", qp_id);
			dev->data->queue_pairs[qp_id] = NULL;
		}
	}

	if (dev->data->queue_pairs)
		rte_free(dev->data->queue_pairs);

	/* Unload firmware */
	cn10k_ml_fw_unload(mldev);

	/* Clear scratch registers */
	roc_ml_reg_write64(&mldev->roc, 0, ML_SCRATCH_WORK_PTR);
	roc_ml_reg_write64(&mldev->roc, 0, ML_SCRATCH_FW_CTRL);
	roc_ml_reg_write64(&mldev->roc, 0, ML_SCRATCH_DBG_BUFFER_HEAD_C0);
	roc_ml_reg_write64(&mldev->roc, 0, ML_SCRATCH_DBG_BUFFER_TAIL_C0);
	roc_ml_reg_write64(&mldev->roc, 0, ML_SCRATCH_DBG_BUFFER_HEAD_C1);
	roc_ml_reg_write64(&mldev->roc, 0, ML_SCRATCH_DBG_BUFFER_TAIL_C1);

	/* Reset ML_MLR_BASE */
	roc_ml_reg_write64(&mldev->roc, 0, ML_MLR_BASE);
	plt_ml_dbg("ML_MLR_BASE = 0x%016lx", roc_ml_reg_read64(&mldev->roc, ML_MLR_BASE));

	mldev->state = ML_CN10K_DEV_STATE_CLOSED;

	/* Remove PCI device */
	return rte_dev_remove(dev->device);
}

static int
cn10k_ml_dev_start(struct rte_ml_dev *dev)
{
	struct cn10k_ml_dev *mldev;
	uint64_t reg_val64;

	mldev = dev->data->dev_private;

	reg_val64 = roc_ml_reg_read64(&mldev->roc, ML_CFG);
	reg_val64 |= ROC_ML_CFG_ENA;
	roc_ml_reg_write64(&mldev->roc, reg_val64, ML_CFG);
	plt_ml_dbg("ML_CFG => 0x%016lx", roc_ml_reg_read64(&mldev->roc, ML_CFG));

	mldev->state = ML_CN10K_DEV_STATE_STARTED;

	return 0;
}

static int
cn10k_ml_dev_stop(struct rte_ml_dev *dev)
{
	struct cn10k_ml_dev *mldev;
	uint64_t reg_val64;

	mldev = dev->data->dev_private;

	reg_val64 = roc_ml_reg_read64(&mldev->roc, ML_CFG);
	reg_val64 &= ~ROC_ML_CFG_ENA;
	roc_ml_reg_write64(&mldev->roc, reg_val64, ML_CFG);
	plt_ml_dbg("ML_CFG => 0x%016lx", roc_ml_reg_read64(&mldev->roc, ML_CFG));

	mldev->state = ML_CN10K_DEV_STATE_CONFIGURED;

	return 0;
}

static int
cn10k_ml_dev_queue_pair_setup(struct rte_ml_dev *dev, uint16_t queue_pair_id,
			      const struct rte_ml_dev_qp_conf *qp_conf, int socket_id)
{
	struct rte_ml_dev_info dev_info;
	struct cn10k_ml_qp *qp;
	uint32_t nb_desc;

	if (queue_pair_id >= dev->data->nb_queue_pairs) {
		plt_err("Queue-pair id = %u (>= max queue pairs supported, %u)\n", queue_pair_id,
			dev->data->nb_queue_pairs);
		return -EINVAL;
	}

	if (dev->data->queue_pairs[queue_pair_id] != NULL)
		cn10k_ml_dev_queue_pair_release(dev, queue_pair_id);

	cn10k_ml_dev_info_get(dev, &dev_info);
	if ((qp_conf->nb_desc > dev_info.max_desc) || (qp_conf->nb_desc == 0)) {
		plt_err("Could not setup queue pair for %u descriptors", qp_conf->nb_desc);
		return -EINVAL;
	}
	plt_ml_dbg("Creating queue-pair, queue_pair_id = %u, nb_desc = %u", queue_pair_id,
		   qp_conf->nb_desc);

	/* As the number of usable descriptors is 1 less than the queue size being created, we
	 * increment the size of queue by 1 than the requested size, except when the requested size
	 * is equal to the maximum possible size.
	 */
	nb_desc =
		(qp_conf->nb_desc == dev_info.max_desc) ? dev_info.max_desc : qp_conf->nb_desc + 1;
	qp = cn10k_ml_qp_create(dev, queue_pair_id, nb_desc, socket_id);
	if (qp == NULL) {
		plt_err("Could not create queue pair %u", queue_pair_id);
		return -ENOMEM;
	}
	dev->data->queue_pairs[queue_pair_id] = qp;

	return 0;
}

struct rte_ml_dev_ops cn10k_ml_ops = {
	/* Device control ops */
	.dev_info_get = cn10k_ml_dev_info_get,
	.dev_configure = cn10k_ml_dev_configure,
	.dev_close = cn10k_ml_dev_close,
	.dev_start = cn10k_ml_dev_start,
	.dev_stop = cn10k_ml_dev_stop,

	/* Queue-pair handling ops */
	.dev_queue_pair_setup = cn10k_ml_dev_queue_pair_setup,
	.dev_queue_pair_release = cn10k_ml_dev_queue_pair_release,
};
