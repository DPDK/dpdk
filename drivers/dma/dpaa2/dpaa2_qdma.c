/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2022 NXP
 */

#include <rte_eal.h>
#include <rte_fslmc.h>
#include <rte_dmadev.h>
#include <rte_dmadev_pmd.h>
#include <mc/fsl_dpdmai.h>
#include "dpaa2_qdma.h"
#include "dpaa2_qdma_logs.h"
/* Dynamic log type identifier */
int dpaa2_qdma_logtype;

uint32_t dpaa2_coherent_no_alloc_cache;
uint32_t dpaa2_coherent_alloc_cache;

static int
dpaa2_qdma_reset(struct rte_dma_dev *dev)
{
	struct dpaa2_dpdmai_dev *dpdmai_dev = dev->data->dev_private;
	struct qdma_device *qdma_dev = dpdmai_dev->qdma_dev;
	int i;

	DPAA2_QDMA_FUNC_TRACE();

	/* In case QDMA device is not in stopped state, return -EBUSY */
	if (qdma_dev->state == 1) {
		DPAA2_QDMA_ERR(
			"Device is in running state. Stop before reset.");
		return -EBUSY;
	}

	/* In case there are pending jobs on any VQ, return -EBUSY */
	for (i = 0; i < qdma_dev->num_vqs; i++) {
		if (qdma_dev->vqs[i].in_use && (qdma_dev->vqs[i].num_enqueues !=
		    qdma_dev->vqs[i].num_dequeues)) {
			DPAA2_QDMA_ERR("Jobs are still pending on VQ: %d", i);
			return -EBUSY;
		}
	}

	/* Reset and free virtual queues */
	for (i = 0; i < qdma_dev->num_vqs; i++) {
		if (qdma_dev->vqs[i].status_ring)
			rte_ring_free(qdma_dev->vqs[i].status_ring);
	}
	if (qdma_dev->vqs)
		rte_free(qdma_dev->vqs);
	qdma_dev->vqs = NULL;

	/* Reset QDMA device structure */
	qdma_dev->num_vqs = 0;

	return 0;
}

static struct rte_dma_dev_ops dpaa2_qdma_ops = {
};

static int
dpaa2_dpdmai_dev_uninit(struct rte_dma_dev *dev)
{
	struct dpaa2_dpdmai_dev *dpdmai_dev = dev->data->dev_private;
	int ret;

	DPAA2_QDMA_FUNC_TRACE();

	ret = dpdmai_disable(&dpdmai_dev->dpdmai, CMD_PRI_LOW,
			     dpdmai_dev->token);
	if (ret)
		DPAA2_QDMA_ERR("dmdmai disable failed");

	/* Set up the DQRR storage for Rx */
	struct dpaa2_queue *rxq = &(dpdmai_dev->rx_queue[0]);

	if (rxq->q_storage) {
		dpaa2_free_dq_storage(rxq->q_storage);
		rte_free(rxq->q_storage);
	}

	/* Close the device at underlying layer*/
	ret = dpdmai_close(&dpdmai_dev->dpdmai, CMD_PRI_LOW, dpdmai_dev->token);
	if (ret)
		DPAA2_QDMA_ERR("Failure closing dpdmai device");

	return 0;
}

static int
dpaa2_dpdmai_dev_init(struct rte_dma_dev *dev, int dpdmai_id)
{
	struct dpaa2_dpdmai_dev *dpdmai_dev = dev->data->dev_private;
	struct dpdmai_rx_queue_cfg rx_queue_cfg;
	struct dpdmai_attr attr;
	struct dpdmai_rx_queue_attr rx_attr;
	struct dpdmai_tx_queue_attr tx_attr;
	struct dpaa2_queue *rxq;
	int ret;

	DPAA2_QDMA_FUNC_TRACE();

	/* Open DPDMAI device */
	dpdmai_dev->dpdmai_id = dpdmai_id;
	dpdmai_dev->dpdmai.regs = dpaa2_get_mcp_ptr(MC_PORTAL_INDEX);
	dpdmai_dev->qdma_dev = rte_malloc(NULL, sizeof(struct qdma_device),
					  RTE_CACHE_LINE_SIZE);
	ret = dpdmai_open(&dpdmai_dev->dpdmai, CMD_PRI_LOW,
			  dpdmai_dev->dpdmai_id, &dpdmai_dev->token);
	if (ret) {
		DPAA2_QDMA_ERR("dpdmai_open() failed with err: %d", ret);
		return ret;
	}

	/* Get DPDMAI attributes */
	ret = dpdmai_get_attributes(&dpdmai_dev->dpdmai, CMD_PRI_LOW,
				    dpdmai_dev->token, &attr);
	if (ret) {
		DPAA2_QDMA_ERR("dpdmai get attributes failed with err: %d",
			       ret);
		goto init_err;
	}
	dpdmai_dev->num_queues = attr.num_of_queues;

	/* Set up Rx Queue */
	memset(&rx_queue_cfg, 0, sizeof(struct dpdmai_rx_queue_cfg));
	ret = dpdmai_set_rx_queue(&dpdmai_dev->dpdmai,
				  CMD_PRI_LOW,
				  dpdmai_dev->token,
				  0, 0, &rx_queue_cfg);
	if (ret) {
		DPAA2_QDMA_ERR("Setting Rx queue failed with err: %d",
			       ret);
		goto init_err;
	}

	/* Allocate DQ storage for the DPDMAI Rx queues */
	rxq = &(dpdmai_dev->rx_queue[0]);
	rxq->q_storage = rte_malloc("dq_storage",
				    sizeof(struct queue_storage_info_t),
				    RTE_CACHE_LINE_SIZE);
	if (!rxq->q_storage) {
		DPAA2_QDMA_ERR("q_storage allocation failed");
		ret = -ENOMEM;
		goto init_err;
	}

	memset(rxq->q_storage, 0, sizeof(struct queue_storage_info_t));
	ret = dpaa2_alloc_dq_storage(rxq->q_storage);
	if (ret) {
		DPAA2_QDMA_ERR("dpaa2_alloc_dq_storage failed");
		goto init_err;
	}

	/* Get Rx and Tx queues FQID */
	ret = dpdmai_get_rx_queue(&dpdmai_dev->dpdmai, CMD_PRI_LOW,
				  dpdmai_dev->token, 0, 0, &rx_attr);
	if (ret) {
		DPAA2_QDMA_ERR("Reading device failed with err: %d",
			       ret);
		goto init_err;
	}
	dpdmai_dev->rx_queue[0].fqid = rx_attr.fqid;

	ret = dpdmai_get_tx_queue(&dpdmai_dev->dpdmai, CMD_PRI_LOW,
				  dpdmai_dev->token, 0, 0, &tx_attr);
	if (ret) {
		DPAA2_QDMA_ERR("Reading device failed with err: %d",
			       ret);
		goto init_err;
	}
	dpdmai_dev->tx_queue[0].fqid = tx_attr.fqid;

	/* Enable the device */
	ret = dpdmai_enable(&dpdmai_dev->dpdmai, CMD_PRI_LOW,
			    dpdmai_dev->token);
	if (ret) {
		DPAA2_QDMA_ERR("Enabling device failed with err: %d", ret);
		goto init_err;
	}

	if (!dpaa2_coherent_no_alloc_cache) {
		if (dpaa2_svr_family == SVR_LX2160A) {
			dpaa2_coherent_no_alloc_cache =
				DPAA2_LX2_COHERENT_NO_ALLOCATE_CACHE;
			dpaa2_coherent_alloc_cache =
				DPAA2_LX2_COHERENT_ALLOCATE_CACHE;
		} else {
			dpaa2_coherent_no_alloc_cache =
				DPAA2_COHERENT_NO_ALLOCATE_CACHE;
			dpaa2_coherent_alloc_cache =
				DPAA2_COHERENT_ALLOCATE_CACHE;
		}
	}

	DPAA2_QDMA_DEBUG("Initialized dpdmai object successfully");

	/* Reset the QDMA device */
	ret = dpaa2_qdma_reset(dev);
	if (ret) {
		DPAA2_QDMA_ERR("Resetting QDMA failed");
		goto init_err;
	}

	return 0;
init_err:
	dpaa2_dpdmai_dev_uninit(dev);
	return ret;
}

static int
dpaa2_qdma_probe(struct rte_dpaa2_driver *dpaa2_drv,
		 struct rte_dpaa2_device *dpaa2_dev)
{
	struct rte_dma_dev *dmadev;
	int ret;

	DPAA2_QDMA_FUNC_TRACE();

	RTE_SET_USED(dpaa2_drv);

	dmadev = rte_dma_pmd_allocate(dpaa2_dev->device.name,
				      rte_socket_id(),
				      sizeof(struct dpaa2_dpdmai_dev));
	if (!dmadev) {
		DPAA2_QDMA_ERR("Unable to allocate dmadevice");
		return -EINVAL;
	}

	dpaa2_dev->dmadev = dmadev;
	dmadev->dev_ops = &dpaa2_qdma_ops;
	dmadev->device = &dpaa2_dev->device;
	dmadev->fp_obj->dev_private = dmadev->data->dev_private;

	/* Invoke PMD device initialization function */
	ret = dpaa2_dpdmai_dev_init(dmadev, dpaa2_dev->object_id);
	if (ret) {
		rte_dma_pmd_release(dpaa2_dev->device.name);
		return ret;
	}

	dmadev->state = RTE_DMA_DEV_READY;
	return 0;
}

static int
dpaa2_qdma_remove(struct rte_dpaa2_device *dpaa2_dev)
{
	struct rte_dma_dev *dmadev = dpaa2_dev->dmadev;
	int ret;

	DPAA2_QDMA_FUNC_TRACE();

	dpaa2_dpdmai_dev_uninit(dmadev);

	ret = rte_dma_pmd_release(dpaa2_dev->device.name);
	if (ret)
		DPAA2_QDMA_ERR("Device cleanup failed");

	return 0;
}

static struct rte_dpaa2_driver rte_dpaa2_qdma_pmd;

static struct rte_dpaa2_driver rte_dpaa2_qdma_pmd = {
	.drv_flags = RTE_DPAA2_DRV_IOVA_AS_VA,
	.drv_type = DPAA2_QDMA,
	.probe = dpaa2_qdma_probe,
	.remove = dpaa2_qdma_remove,
};

RTE_PMD_REGISTER_DPAA2(dpaa2_qdma, rte_dpaa2_qdma_pmd);
RTE_PMD_REGISTER_PARAM_STRING(dpaa2_qdma,
	"no_prefetch=<int> ");
RTE_LOG_REGISTER_DEFAULT(dpaa_qdma2_logtype, INFO);
