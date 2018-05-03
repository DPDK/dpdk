/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 NXP
 */

#include <string.h>

#include <rte_eal.h>
#include <rte_fslmc.h>
#include <rte_atomic.h>
#include <rte_lcore.h>
#include <rte_rawdev.h>
#include <rte_rawdev_pmd.h>
#include <rte_malloc.h>
#include <rte_ring.h>
#include <rte_mempool.h>

#include <mc/fsl_dpdmai.h>
#include <portal/dpaa2_hw_pvt.h>
#include <portal/dpaa2_hw_dpio.h>

#include "dpaa2_qdma.h"
#include "dpaa2_qdma_logs.h"
#include "rte_pmd_dpaa2_qdma.h"

/* Dynamic log type identifier */
int dpaa2_qdma_logtype;

/* QDMA device */
static struct qdma_device qdma_dev;

/* QDMA H/W queues list */
TAILQ_HEAD(qdma_hw_queue_list, qdma_hw_queue);
static struct qdma_hw_queue_list qdma_queue_list
	= TAILQ_HEAD_INITIALIZER(qdma_queue_list);

/* QDMA Virtual Queues */
struct qdma_virt_queue *qdma_vqs;

/* QDMA per core data */
struct qdma_per_core_info qdma_core_info[RTE_MAX_LCORE];

static struct qdma_hw_queue *
alloc_hw_queue(uint32_t lcore_id)
{
	struct qdma_hw_queue *queue = NULL;

	DPAA2_QDMA_FUNC_TRACE();

	/* Get a free queue from the list */
	TAILQ_FOREACH(queue, &qdma_queue_list, next) {
		if (queue->num_users == 0) {
			queue->lcore_id = lcore_id;
			queue->num_users++;
			break;
		}
	}

	return queue;
}

static void
free_hw_queue(struct qdma_hw_queue *queue)
{
	DPAA2_QDMA_FUNC_TRACE();

	queue->num_users--;
}


static struct qdma_hw_queue *
get_hw_queue(uint32_t lcore_id)
{
	struct qdma_per_core_info *core_info;
	struct qdma_hw_queue *queue, *temp;
	uint32_t least_num_users;
	int num_hw_queues, i;

	DPAA2_QDMA_FUNC_TRACE();

	core_info = &qdma_core_info[lcore_id];
	num_hw_queues = core_info->num_hw_queues;

	/*
	 * Allocate a HW queue if there are less queues
	 * than maximum per core queues configured
	 */
	if (num_hw_queues < qdma_dev.max_hw_queues_per_core) {
		queue = alloc_hw_queue(lcore_id);
		if (queue) {
			core_info->hw_queues[num_hw_queues] = queue;
			core_info->num_hw_queues++;
			return queue;
		}
	}

	queue = core_info->hw_queues[0];
	/* In case there is no queue associated with the core return NULL */
	if (!queue)
		return NULL;

	/* Fetch the least loaded H/W queue */
	least_num_users = core_info->hw_queues[0]->num_users;
	for (i = 0; i < num_hw_queues; i++) {
		temp = core_info->hw_queues[i];
		if (temp->num_users < least_num_users)
			queue = temp;
	}

	if (queue)
		queue->num_users++;

	return queue;
}

static void
put_hw_queue(struct qdma_hw_queue *queue)
{
	struct qdma_per_core_info *core_info;
	int lcore_id, num_hw_queues, i;

	DPAA2_QDMA_FUNC_TRACE();

	/*
	 * If this is the last user of the queue free it.
	 * Also remove it from QDMA core info.
	 */
	if (queue->num_users == 1) {
		free_hw_queue(queue);

		/* Remove the physical queue from core info */
		lcore_id = queue->lcore_id;
		core_info = &qdma_core_info[lcore_id];
		num_hw_queues = core_info->num_hw_queues;
		for (i = 0; i < num_hw_queues; i++) {
			if (queue == core_info->hw_queues[i])
				break;
		}
		for (; i < num_hw_queues - 1; i++)
			core_info->hw_queues[i] = core_info->hw_queues[i + 1];
		core_info->hw_queues[i] = NULL;
	} else {
		queue->num_users--;
	}
}

int __rte_experimental
rte_qdma_init(void)
{
	DPAA2_QDMA_FUNC_TRACE();

	rte_spinlock_init(&qdma_dev.lock);

	return 0;
}

void __rte_experimental
rte_qdma_attr_get(struct rte_qdma_attr *qdma_attr)
{
	DPAA2_QDMA_FUNC_TRACE();

	qdma_attr->num_hw_queues = qdma_dev.num_hw_queues;
}

int __rte_experimental
rte_qdma_reset(void)
{
	struct qdma_hw_queue *queue;
	int i;

	DPAA2_QDMA_FUNC_TRACE();

	/* In case QDMA device is not in stopped state, return -EBUSY */
	if (qdma_dev.state == 1) {
		DPAA2_QDMA_ERR(
			"Device is in running state. Stop before reset.");
		return -EBUSY;
	}

	/* In case there are pending jobs on any VQ, return -EBUSY */
	for (i = 0; i < qdma_dev.max_vqs; i++) {
		if (qdma_vqs[i].in_use && (qdma_vqs[i].num_enqueues !=
		    qdma_vqs[i].num_dequeues))
			DPAA2_QDMA_ERR("Jobs are still pending on VQ: %d", i);
			return -EBUSY;
	}

	/* Reset HW queues */
	TAILQ_FOREACH(queue, &qdma_queue_list, next)
		queue->num_users = 0;

	/* Reset and free virtual queues */
	for (i = 0; i < qdma_dev.max_vqs; i++) {
		if (qdma_vqs[i].status_ring)
			rte_ring_free(qdma_vqs[i].status_ring);
	}
	if (qdma_vqs)
		rte_free(qdma_vqs);
	qdma_vqs = NULL;

	/* Reset per core info */
	memset(&qdma_core_info, 0,
		sizeof(struct qdma_per_core_info) * RTE_MAX_LCORE);

	/* Free the FLE pool */
	if (qdma_dev.fle_pool)
		rte_mempool_free(qdma_dev.fle_pool);

	/* Reset QDMA device structure */
	qdma_dev.mode = RTE_QDMA_MODE_HW;
	qdma_dev.max_hw_queues_per_core = 0;
	qdma_dev.fle_pool = NULL;
	qdma_dev.fle_pool_count = 0;
	qdma_dev.max_vqs = 0;

	return 0;
}

int __rte_experimental
rte_qdma_configure(struct rte_qdma_config *qdma_config)
{
	int ret;

	DPAA2_QDMA_FUNC_TRACE();

	/* In case QDMA device is not in stopped state, return -EBUSY */
	if (qdma_dev.state == 1) {
		DPAA2_QDMA_ERR(
			"Device is in running state. Stop before config.");
		return -1;
	}

	/* Reset the QDMA device */
	ret = rte_qdma_reset();
	if (ret) {
		DPAA2_QDMA_ERR("Resetting QDMA failed");
		return ret;
	}

	/* Set mode */
	qdma_dev.mode = qdma_config->mode;

	/* Set max HW queue per core */
	if (qdma_config->max_hw_queues_per_core > MAX_HW_QUEUE_PER_CORE) {
		DPAA2_QDMA_ERR("H/W queues per core is more than: %d",
			       MAX_HW_QUEUE_PER_CORE);
		return -EINVAL;
	}
	qdma_dev.max_hw_queues_per_core =
		qdma_config->max_hw_queues_per_core;

	/* Allocate Virtual Queues */
	qdma_vqs = rte_malloc("qdma_virtual_queues",
			(sizeof(struct qdma_virt_queue) * qdma_config->max_vqs),
			RTE_CACHE_LINE_SIZE);
	if (!qdma_vqs) {
		DPAA2_QDMA_ERR("qdma_virtual_queues allocation failed");
		return -ENOMEM;
	}
	qdma_dev.max_vqs = qdma_config->max_vqs;

	/* Allocate FLE pool */
	qdma_dev.fle_pool = rte_mempool_create("qdma_fle_pool",
			qdma_config->fle_pool_count, QDMA_FLE_POOL_SIZE,
			QDMA_FLE_CACHE_SIZE(qdma_config->fle_pool_count), 0,
			NULL, NULL, NULL, NULL, SOCKET_ID_ANY, 0);
	if (!qdma_dev.fle_pool) {
		DPAA2_QDMA_ERR("qdma_fle_pool create failed");
		rte_free(qdma_vqs);
		qdma_vqs = NULL;
		return -ENOMEM;
	}
	qdma_dev.fle_pool_count = qdma_config->fle_pool_count;

	return 0;
}

int __rte_experimental
rte_qdma_start(void)
{
	DPAA2_QDMA_FUNC_TRACE();

	qdma_dev.state = 1;

	return 0;
}

int __rte_experimental
rte_qdma_vq_create(uint32_t lcore_id, uint32_t flags)
{
	char ring_name[32];
	int i;

	DPAA2_QDMA_FUNC_TRACE();

	rte_spinlock_lock(&qdma_dev.lock);

	/* Get a free Virtual Queue */
	for (i = 0; i < qdma_dev.max_vqs; i++) {
		if (qdma_vqs[i].in_use == 0)
			break;
	}

	/* Return in case no VQ is free */
	if (i == qdma_dev.max_vqs) {
		rte_spinlock_unlock(&qdma_dev.lock);
		return -ENODEV;
	}

	if (qdma_dev.mode == RTE_QDMA_MODE_HW ||
			(flags & RTE_QDMA_VQ_EXCLUSIVE_PQ)) {
		/* Allocate HW queue for a VQ */
		qdma_vqs[i].hw_queue = alloc_hw_queue(lcore_id);
		qdma_vqs[i].exclusive_hw_queue = 1;
	} else {
		/* Allocate a Ring for Virutal Queue in VQ mode */
		sprintf(ring_name, "status ring %d", i);
		qdma_vqs[i].status_ring = rte_ring_create(ring_name,
			qdma_dev.fle_pool_count, rte_socket_id(), 0);
		if (!qdma_vqs[i].status_ring) {
			DPAA2_QDMA_ERR("Status ring creation failed for vq");
			rte_spinlock_unlock(&qdma_dev.lock);
			return rte_errno;
		}

		/* Get a HW queue (shared) for a VQ */
		qdma_vqs[i].hw_queue = get_hw_queue(lcore_id);
		qdma_vqs[i].exclusive_hw_queue = 0;
	}

	if (qdma_vqs[i].hw_queue == NULL) {
		DPAA2_QDMA_ERR("No H/W queue available for VQ");
		if (qdma_vqs[i].status_ring)
			rte_ring_free(qdma_vqs[i].status_ring);
		qdma_vqs[i].status_ring = NULL;
		rte_spinlock_unlock(&qdma_dev.lock);
		return -ENODEV;
	}

	qdma_vqs[i].in_use = 1;
	qdma_vqs[i].lcore_id = lcore_id;

	rte_spinlock_unlock(&qdma_dev.lock);

	return i;
}

void __rte_experimental
rte_qdma_vq_stats(uint16_t vq_id,
		  struct rte_qdma_vq_stats *vq_status)
{
	struct qdma_virt_queue *qdma_vq = &qdma_vqs[vq_id];

	DPAA2_QDMA_FUNC_TRACE();

	if (qdma_vq->in_use) {
		vq_status->exclusive_hw_queue = qdma_vq->exclusive_hw_queue;
		vq_status->lcore_id = qdma_vq->lcore_id;
		vq_status->num_enqueues = qdma_vq->num_enqueues;
		vq_status->num_dequeues = qdma_vq->num_dequeues;
		vq_status->num_pending_jobs = vq_status->num_enqueues -
				vq_status->num_dequeues;
	}
}

int __rte_experimental
rte_qdma_vq_destroy(uint16_t vq_id)
{
	struct qdma_virt_queue *qdma_vq = &qdma_vqs[vq_id];

	DPAA2_QDMA_FUNC_TRACE();

	/* In case there are pending jobs on any VQ, return -EBUSY */
	if (qdma_vq->num_enqueues != qdma_vq->num_dequeues)
		return -EBUSY;

	rte_spinlock_lock(&qdma_dev.lock);

	if (qdma_vq->exclusive_hw_queue)
		free_hw_queue(qdma_vq->hw_queue);
	else {
		if (qdma_vqs->status_ring)
			rte_ring_free(qdma_vqs->status_ring);

		put_hw_queue(qdma_vq->hw_queue);
	}

	memset(qdma_vq, 0, sizeof(struct qdma_virt_queue));

	rte_spinlock_lock(&qdma_dev.lock);

	return 0;
}

void __rte_experimental
rte_qdma_stop(void)
{
	DPAA2_QDMA_FUNC_TRACE();

	qdma_dev.state = 0;
}

void __rte_experimental
rte_qdma_destroy(void)
{
	DPAA2_QDMA_FUNC_TRACE();

	rte_qdma_reset();
}

static const struct rte_rawdev_ops dpaa2_qdma_ops;

static int
add_hw_queues_to_list(struct dpaa2_dpdmai_dev *dpdmai_dev)
{
	struct qdma_hw_queue *queue;
	int i;

	DPAA2_QDMA_FUNC_TRACE();

	for (i = 0; i < dpdmai_dev->num_queues; i++) {
		queue = rte_zmalloc(NULL, sizeof(struct qdma_hw_queue), 0);
		if (!queue) {
			DPAA2_QDMA_ERR(
				"Memory allocation failed for QDMA queue");
			return -ENOMEM;
		}

		queue->dpdmai_dev = dpdmai_dev;
		queue->queue_id = i;

		TAILQ_INSERT_TAIL(&qdma_queue_list, queue, next);
		qdma_dev.num_hw_queues++;
	}

	return 0;
}

static void
remove_hw_queues_from_list(struct dpaa2_dpdmai_dev *dpdmai_dev)
{
	struct qdma_hw_queue *queue = NULL;
	struct qdma_hw_queue *tqueue = NULL;

	DPAA2_QDMA_FUNC_TRACE();

	TAILQ_FOREACH_SAFE(queue, &qdma_queue_list, next, tqueue) {
		if (queue->dpdmai_dev == dpdmai_dev) {
			TAILQ_REMOVE(&qdma_queue_list, queue, next);
			rte_free(queue);
			queue = NULL;
		}
	}
}

static int
dpaa2_dpdmai_dev_uninit(struct rte_rawdev *rawdev)
{
	struct dpaa2_dpdmai_dev *dpdmai_dev = rawdev->dev_private;
	int ret, i;

	DPAA2_QDMA_FUNC_TRACE();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	/* Remove HW queues from global list */
	remove_hw_queues_from_list(dpdmai_dev);

	ret = dpdmai_disable(&dpdmai_dev->dpdmai, CMD_PRI_LOW,
			     dpdmai_dev->token);
	if (ret)
		DPAA2_QDMA_ERR("dmdmai disable failed");

	/* Set up the DQRR storage for Rx */
	for (i = 0; i < DPDMAI_PRIO_NUM; i++) {
		struct dpaa2_queue *rxq = &(dpdmai_dev->rx_queue[i]);

		if (rxq->q_storage) {
			dpaa2_free_dq_storage(rxq->q_storage);
			rte_free(rxq->q_storage);
		}
	}

	/* Close the device at underlying layer*/
	ret = dpdmai_close(&dpdmai_dev->dpdmai, CMD_PRI_LOW, dpdmai_dev->token);
	if (ret)
		DPAA2_QDMA_ERR("Failure closing dpdmai device");

	return 0;
}

static int
dpaa2_dpdmai_dev_init(struct rte_rawdev *rawdev, int dpdmai_id)
{
	struct dpaa2_dpdmai_dev *dpdmai_dev = rawdev->dev_private;
	struct dpdmai_rx_queue_cfg rx_queue_cfg;
	struct dpdmai_attr attr;
	struct dpdmai_rx_queue_attr rx_attr;
	struct dpdmai_tx_queue_attr tx_attr;
	int ret, i;

	DPAA2_QDMA_FUNC_TRACE();

	/* For secondary processes, the primary has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	/* Open DPDMAI device */
	dpdmai_dev->dpdmai_id = dpdmai_id;
	dpdmai_dev->dpdmai.regs = rte_mcp_ptr_list[MC_PORTAL_INDEX];
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
	dpdmai_dev->num_queues = attr.num_of_priorities;

	/* Set up Rx Queues */
	for (i = 0; i < attr.num_of_priorities; i++) {
		struct dpaa2_queue *rxq;

		memset(&rx_queue_cfg, 0, sizeof(struct dpdmai_rx_queue_cfg));
		ret = dpdmai_set_rx_queue(&dpdmai_dev->dpdmai,
					  CMD_PRI_LOW,
					  dpdmai_dev->token,
					  i, &rx_queue_cfg);
		if (ret) {
			DPAA2_QDMA_ERR("Setting Rx queue failed with err: %d",
				       ret);
			goto init_err;
		}

		/* Allocate DQ storage for the DPDMAI Rx queues */
		rxq = &(dpdmai_dev->rx_queue[i]);
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
	}

	/* Get Rx and Tx queues FQID's */
	for (i = 0; i < DPDMAI_PRIO_NUM; i++) {
		ret = dpdmai_get_rx_queue(&dpdmai_dev->dpdmai, CMD_PRI_LOW,
					  dpdmai_dev->token, i, &rx_attr);
		if (ret) {
			DPAA2_QDMA_ERR("Reading device failed with err: %d",
				       ret);
			goto init_err;
		}
		dpdmai_dev->rx_queue[i].fqid = rx_attr.fqid;

		ret = dpdmai_get_tx_queue(&dpdmai_dev->dpdmai, CMD_PRI_LOW,
					  dpdmai_dev->token, i, &tx_attr);
		if (ret) {
			DPAA2_QDMA_ERR("Reading device failed with err: %d",
				       ret);
			goto init_err;
		}
		dpdmai_dev->tx_queue[i].fqid = tx_attr.fqid;
	}

	/* Enable the device */
	ret = dpdmai_enable(&dpdmai_dev->dpdmai, CMD_PRI_LOW,
			    dpdmai_dev->token);
	if (ret) {
		DPAA2_QDMA_ERR("Enabling device failed with err: %d", ret);
		goto init_err;
	}

	/* Add the HW queue to the global list */
	ret = add_hw_queues_to_list(dpdmai_dev);
	if (ret) {
		DPAA2_QDMA_ERR("Adding H/W queue to list failed");
		goto init_err;
	}
	DPAA2_QDMA_DEBUG("Initialized dpdmai object successfully");

	return 0;
init_err:
	dpaa2_dpdmai_dev_uninit(rawdev);
	return ret;
}

static int
rte_dpaa2_qdma_probe(struct rte_dpaa2_driver *dpaa2_drv,
		     struct rte_dpaa2_device *dpaa2_dev)
{
	struct rte_rawdev *rawdev;
	int ret;

	DPAA2_QDMA_FUNC_TRACE();

	rawdev = rte_rawdev_pmd_allocate(dpaa2_dev->device.name,
			sizeof(struct dpaa2_dpdmai_dev),
			rte_socket_id());
	if (!rawdev) {
		DPAA2_QDMA_ERR("Unable to allocate rawdevice");
		return -EINVAL;
	}

	dpaa2_dev->rawdev = rawdev;
	rawdev->dev_ops = &dpaa2_qdma_ops;
	rawdev->device = &dpaa2_dev->device;
	rawdev->driver_name = dpaa2_drv->driver.name;

	/* Invoke PMD device initialization function */
	ret = dpaa2_dpdmai_dev_init(rawdev, dpaa2_dev->object_id);
	if (ret) {
		rte_rawdev_pmd_release(rawdev);
		return ret;
	}

	return 0;
}

static int
rte_dpaa2_qdma_remove(struct rte_dpaa2_device *dpaa2_dev)
{
	struct rte_rawdev *rawdev = dpaa2_dev->rawdev;
	int ret;

	DPAA2_QDMA_FUNC_TRACE();

	dpaa2_dpdmai_dev_uninit(rawdev);

	ret = rte_rawdev_pmd_release(rawdev);
	if (ret)
		DPAA2_QDMA_ERR("Device cleanup failed");

	return 0;
}

static struct rte_dpaa2_driver rte_dpaa2_qdma_pmd = {
	.drv_type = DPAA2_QDMA,
	.probe = rte_dpaa2_qdma_probe,
	.remove = rte_dpaa2_qdma_remove,
};

RTE_PMD_REGISTER_DPAA2(dpaa2_qdma, rte_dpaa2_qdma_pmd);

RTE_INIT(dpaa2_qdma_init_log);
static void
dpaa2_qdma_init_log(void)
{
	dpaa2_qdma_logtype = rte_log_register("pmd.raw.dpaa2.qdma");
	if (dpaa2_qdma_logtype >= 0)
		rte_log_set_level(dpaa2_qdma_logtype, RTE_LOG_INFO);
}
