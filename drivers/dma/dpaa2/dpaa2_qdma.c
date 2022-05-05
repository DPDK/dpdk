/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2022 NXP
 */

#include <rte_eal.h>
#include <rte_fslmc.h>
#include <rte_dmadev.h>
#include <rte_dmadev_pmd.h>
#include <rte_kvargs.h>
#include <mc/fsl_dpdmai.h>
#include "dpaa2_qdma.h"
#include "dpaa2_qdma_logs.h"
/* Dynamic log type identifier */
int dpaa2_qdma_logtype;

uint32_t dpaa2_coherent_no_alloc_cache;
uint32_t dpaa2_coherent_alloc_cache;

static int
dpaa2_qdma_info_get(const struct rte_dma_dev *dev,
		    struct rte_dma_info *dev_info,
		    uint32_t info_sz)
{
	RTE_SET_USED(dev);
	RTE_SET_USED(info_sz);

	dev_info->dev_capa = RTE_DMA_CAPA_MEM_TO_MEM |
			     RTE_DMA_CAPA_MEM_TO_DEV |
			     RTE_DMA_CAPA_DEV_TO_DEV |
			     RTE_DMA_CAPA_DEV_TO_MEM |
			     RTE_DMA_CAPA_SILENT |
			     RTE_DMA_CAPA_OPS_COPY;
	dev_info->max_vchans = DPAA2_QDMA_MAX_VHANS;
	dev_info->max_desc = DPAA2_QDMA_MAX_DESC;
	dev_info->min_desc = DPAA2_QDMA_MIN_DESC;

	return 0;
}

static int
dpaa2_qdma_configure(struct rte_dma_dev *dev,
		     const struct rte_dma_conf *dev_conf,
		     uint32_t conf_sz)
{
	char name[32]; /* RTE_MEMZONE_NAMESIZE = 32 */
	struct dpaa2_dpdmai_dev *dpdmai_dev = dev->data->dev_private;
	struct qdma_device *qdma_dev = dpdmai_dev->qdma_dev;

	DPAA2_QDMA_FUNC_TRACE();

	RTE_SET_USED(conf_sz);

	/* In case QDMA device is not in stopped state, return -EBUSY */
	if (qdma_dev->state == 1) {
		DPAA2_QDMA_ERR(
			"Device is in running state. Stop before config.");
		return -1;
	}

	/* Allocate Virtual Queues */
	sprintf(name, "qdma_%d_vq", dev->data->dev_id);
	qdma_dev->vqs = rte_malloc(name,
			(sizeof(struct qdma_virt_queue) * dev_conf->nb_vchans),
			RTE_CACHE_LINE_SIZE);
	if (!qdma_dev->vqs) {
		DPAA2_QDMA_ERR("qdma_virtual_queues allocation failed");
		return -ENOMEM;
	}
	qdma_dev->num_vqs = dev_conf->nb_vchans;

	return 0;
}

static int
dpaa2_qdma_vchan_setup(struct rte_dma_dev *dev, uint16_t vchan,
		       const struct rte_dma_vchan_conf *conf,
		       uint32_t conf_sz)
{
	struct dpaa2_dpdmai_dev *dpdmai_dev = dev->data->dev_private;
	struct qdma_device *qdma_dev = dpdmai_dev->qdma_dev;
	uint32_t pool_size;
	char ring_name[32];
	char pool_name[64];
	int fd_long_format = 1;
	int sg_enable = 0;

	DPAA2_QDMA_FUNC_TRACE();

	RTE_SET_USED(conf_sz);

	if (qdma_dev->vqs[vchan].flags & DPAA2_QDMA_VQ_FD_SG_FORMAT)
		sg_enable = 1;

	if (qdma_dev->vqs[vchan].flags & DPAA2_QDMA_VQ_FD_SHORT_FORMAT)
		fd_long_format = 0;

	if (dev->data->dev_conf.enable_silent)
		qdma_dev->vqs[vchan].flags |= DPAA2_QDMA_VQ_NO_RESPONSE;

	if (sg_enable) {
		if (qdma_dev->num_vqs != 1) {
			DPAA2_QDMA_ERR(
				"qDMA SG format only supports physical queue!");
			return -ENODEV;
		}
		if (!fd_long_format) {
			DPAA2_QDMA_ERR(
				"qDMA SG format only supports long FD format!");
			return -ENODEV;
		}
		pool_size = QDMA_FLE_SG_POOL_SIZE;
	} else {
		pool_size = QDMA_FLE_SINGLE_POOL_SIZE;
	}

	if (qdma_dev->num_vqs == 1)
		qdma_dev->vqs[vchan].exclusive_hw_queue = 1;
	else {
		/* Allocate a Ring for Virtual Queue in VQ mode */
		snprintf(ring_name, sizeof(ring_name), "status ring %d %d",
			 dev->data->dev_id, vchan);
		qdma_dev->vqs[vchan].status_ring = rte_ring_create(ring_name,
			conf->nb_desc, rte_socket_id(), 0);
		if (!qdma_dev->vqs[vchan].status_ring) {
			DPAA2_QDMA_ERR("Status ring creation failed for vq");
			return rte_errno;
		}
	}

	snprintf(pool_name, sizeof(pool_name),
		"qdma_fle_pool_dev%d_qid%d", dpdmai_dev->dpdmai_id, vchan);
	qdma_dev->vqs[vchan].fle_pool = rte_mempool_create(pool_name,
			conf->nb_desc, pool_size,
			QDMA_FLE_CACHE_SIZE(conf->nb_desc), 0,
			NULL, NULL, NULL, NULL, SOCKET_ID_ANY, 0);
	if (!qdma_dev->vqs[vchan].fle_pool) {
		DPAA2_QDMA_ERR("qdma_fle_pool create failed");
		return -ENOMEM;
	}

	snprintf(pool_name, sizeof(pool_name),
		"qdma_job_pool_dev%d_qid%d", dpdmai_dev->dpdmai_id, vchan);
	qdma_dev->vqs[vchan].job_pool = rte_mempool_create(pool_name,
			conf->nb_desc, pool_size,
			QDMA_FLE_CACHE_SIZE(conf->nb_desc), 0,
			NULL, NULL, NULL, NULL, SOCKET_ID_ANY, 0);
	if (!qdma_dev->vqs[vchan].job_pool) {
		DPAA2_QDMA_ERR("qdma_job_pool create failed");
		return -ENOMEM;
	}

	qdma_dev->vqs[vchan].dpdmai_dev = dpdmai_dev;
	qdma_dev->vqs[vchan].nb_desc = conf->nb_desc;

	return 0;
}

static int
dpaa2_qdma_start(struct rte_dma_dev *dev)
{
	struct dpaa2_dpdmai_dev *dpdmai_dev = dev->data->dev_private;
	struct qdma_device *qdma_dev = dpdmai_dev->qdma_dev;

	DPAA2_QDMA_FUNC_TRACE();

	qdma_dev->state = 1;

	return 0;
}

static int
dpaa2_qdma_stop(struct rte_dma_dev *dev)
{
	struct dpaa2_dpdmai_dev *dpdmai_dev = dev->data->dev_private;
	struct qdma_device *qdma_dev = dpdmai_dev->qdma_dev;

	DPAA2_QDMA_FUNC_TRACE();

	qdma_dev->state = 0;

	return 0;
}

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

static int
dpaa2_qdma_close(__rte_unused struct rte_dma_dev *dev)
{
	DPAA2_QDMA_FUNC_TRACE();

	dpaa2_qdma_reset(dev);

	return 0;
}

static struct rte_dma_dev_ops dpaa2_qdma_ops = {
	.dev_info_get     = dpaa2_qdma_info_get,
	.dev_configure    = dpaa2_qdma_configure,
	.dev_start        = dpaa2_qdma_start,
	.dev_stop         = dpaa2_qdma_stop,
	.dev_close        = dpaa2_qdma_close,
	.vchan_setup      = dpaa2_qdma_vchan_setup,
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
