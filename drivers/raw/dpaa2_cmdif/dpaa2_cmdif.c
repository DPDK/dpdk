/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2019 NXP
 */

#include <stdio.h>
#include <errno.h>
#include <stdint.h>

#include <rte_bus_vdev.h>
#include <rte_atomic.h>
#include <rte_interrupts.h>
#include <rte_branch_prediction.h>
#include <rte_lcore.h>

#include <rte_rawdev.h>
#include <rte_rawdev_pmd.h>

#include <portal/dpaa2_hw_pvt.h>
#include <portal/dpaa2_hw_dpio.h>
#include "dpaa2_cmdif_logs.h"
#include "rte_pmd_dpaa2_cmdif.h"

/* CMDIF driver name */
#define DPAA2_CMDIF_PMD_NAME dpaa2_dpci

/*
 * This API provides the DPCI device ID in 'attr_value'.
 * The device ID shall be passed by GPP to the AIOP using CMDIF commands.
 */
static int
dpaa2_cmdif_get_attr(struct rte_rawdev *dev,
		     const char *attr_name,
		     uint64_t *attr_value)
{
	struct dpaa2_dpci_dev *cidev = dev->dev_private;

	DPAA2_CMDIF_FUNC_TRACE();

	RTE_SET_USED(attr_name);

	if (!attr_value) {
		DPAA2_CMDIF_ERR("Invalid arguments for getting attributes");
		return -EINVAL;
	}
	*attr_value = cidev->dpci_id;

	return 0;
}

static int
dpaa2_cmdif_enqueue_bufs(struct rte_rawdev *dev,
			 struct rte_rawdev_buf **buffers,
			 unsigned int count,
			 rte_rawdev_obj_t context)
{
	struct dpaa2_dpci_dev *cidev = dev->dev_private;
	struct rte_dpaa2_cmdif_context *cmdif_send_cnxt;
	struct dpaa2_queue *txq;
	struct qbman_fd fd;
	struct qbman_eq_desc eqdesc;
	struct qbman_swp *swp;
	uint32_t retry_count = 0;
	int ret;

	RTE_SET_USED(count);

	if (unlikely(!DPAA2_PER_LCORE_DPIO)) {
		ret = dpaa2_affine_qbman_swp();
		if (ret) {
			DPAA2_CMDIF_ERR(
				"Failed to allocate IO portal, tid: %d\n",
				rte_gettid());
			return 0;
		}
	}
	swp = DPAA2_PER_LCORE_PORTAL;

	cmdif_send_cnxt = (struct rte_dpaa2_cmdif_context *)(context);
	txq = &(cidev->tx_queue[cmdif_send_cnxt->priority]);

	/* Prepare enqueue descriptor */
	qbman_eq_desc_clear(&eqdesc);
	qbman_eq_desc_set_fq(&eqdesc, txq->fqid);
	qbman_eq_desc_set_no_orp(&eqdesc, 0);
	qbman_eq_desc_set_response(&eqdesc, 0, 0);

	/* Set some of the FD parameters to i.
	 * For performance reasons do not memset
	 */
	fd.simple.bpid_offset = 0;
	fd.simple.ctrl = 0;

	DPAA2_SET_FD_ADDR(&fd, DPAA2_VADDR_TO_IOVA(buffers[0]->buf_addr));
	DPAA2_SET_FD_LEN(&fd, cmdif_send_cnxt->size);
	DPAA2_SET_FD_FRC(&fd, cmdif_send_cnxt->frc);
	DPAA2_SET_FD_FLC(&fd, cmdif_send_cnxt->flc);

	/* Enqueue a packet to the QBMAN */
	do {
		ret = qbman_swp_enqueue_multiple(swp, &eqdesc, &fd, NULL, 1);
		if (ret < 0 && ret != -EBUSY)
			DPAA2_CMDIF_ERR("Transmit failure with err: %d\n", ret);
		retry_count++;
	} while ((ret == -EBUSY) && (retry_count < DPAA2_MAX_TX_RETRY_COUNT));

	if (ret < 0)
		return ret;

	DPAA2_CMDIF_DP_DEBUG("Successfully transmitted a packet\n");

	return 1;
}

static int
dpaa2_cmdif_dequeue_bufs(struct rte_rawdev *dev,
			 struct rte_rawdev_buf **buffers,
			 unsigned int count,
			 rte_rawdev_obj_t context)
{
	struct dpaa2_dpci_dev *cidev = dev->dev_private;
	struct rte_dpaa2_cmdif_context *cmdif_rcv_cnxt;
	struct dpaa2_queue *rxq;
	struct qbman_swp *swp;
	struct qbman_result *dq_storage;
	const struct qbman_fd *fd;
	struct qbman_pull_desc pulldesc;
	uint8_t status;
	int ret;

	RTE_SET_USED(count);

	if (unlikely(!DPAA2_PER_LCORE_DPIO)) {
		ret = dpaa2_affine_qbman_swp();
		if (ret) {
			DPAA2_CMDIF_ERR(
				"Failed to allocate IO portal, tid: %d\n",
				rte_gettid());
			return 0;
		}
	}
	swp = DPAA2_PER_LCORE_PORTAL;

	cmdif_rcv_cnxt = (struct rte_dpaa2_cmdif_context *)(context);
	rxq = &(cidev->rx_queue[cmdif_rcv_cnxt->priority]);
	dq_storage = rxq->q_storage->dq_storage[0];

	qbman_pull_desc_clear(&pulldesc);
	qbman_pull_desc_set_fq(&pulldesc, rxq->fqid);
	qbman_pull_desc_set_numframes(&pulldesc, 1);
	qbman_pull_desc_set_storage(&pulldesc, dq_storage,
		(uint64_t)(DPAA2_VADDR_TO_IOVA(dq_storage)), 1);

	while (1) {
		if (qbman_swp_pull(swp, &pulldesc)) {
			DPAA2_CMDIF_DP_WARN("VDQ cmd not issued. QBMAN is busy\n");
			/* Portal was busy, try again */
			continue;
		}
		break;
	}

	/* Check if previous issued command is completed. */
	while (!qbman_check_command_complete(dq_storage))
		;
	/* Loop until the dq_storage is updated with new token by QBMAN */
	while (!qbman_result_has_new_result(swp, dq_storage))
		;

	/* Check for valid frame. */
	status = (uint8_t)qbman_result_DQ_flags(dq_storage);
	if (unlikely((status & QBMAN_DQ_STAT_VALIDFRAME) == 0)) {
		DPAA2_CMDIF_DP_DEBUG("No frame is delivered\n");
		return 0;
	}

	fd = qbman_result_DQ_fd(dq_storage);

	buffers[0]->buf_addr = (void *)DPAA2_IOVA_TO_VADDR(
			DPAA2_GET_FD_ADDR(fd) +	DPAA2_GET_FD_OFFSET(fd));
	cmdif_rcv_cnxt->size = DPAA2_GET_FD_LEN(fd);
	cmdif_rcv_cnxt->flc = DPAA2_GET_FD_FLC(fd);
	cmdif_rcv_cnxt->frc = DPAA2_GET_FD_FRC(fd);

	DPAA2_CMDIF_DP_DEBUG("packet received\n");

	return 1;
}

static const struct rte_rawdev_ops dpaa2_cmdif_ops = {
	.attr_get = dpaa2_cmdif_get_attr,
	.enqueue_bufs = dpaa2_cmdif_enqueue_bufs,
	.dequeue_bufs = dpaa2_cmdif_dequeue_bufs,
};

static int
dpaa2_cmdif_create(const char *name,
		   struct rte_vdev_device *vdev,
		   int socket_id)
{
	struct rte_rawdev *rawdev;
	struct dpaa2_dpci_dev *cidev;

	/* Allocate device structure */
	rawdev = rte_rawdev_pmd_allocate(name, sizeof(struct dpaa2_dpci_dev),
					 socket_id);
	if (!rawdev) {
		DPAA2_CMDIF_ERR("Unable to allocate rawdevice");
		return -EINVAL;
	}

	rawdev->dev_ops = &dpaa2_cmdif_ops;
	rawdev->device = &vdev->device;

	/* For secondary processes, the primary has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	cidev = rte_dpaa2_alloc_dpci_dev();
	if (!cidev) {
		DPAA2_CMDIF_ERR("Unable to allocate CI device");
		rte_rawdev_pmd_release(rawdev);
		return -ENODEV;
	}

	rawdev->dev_private = cidev;

	return 0;
}

static int
dpaa2_cmdif_destroy(const char *name)
{
	int ret;
	struct rte_rawdev *rdev;

	rdev = rte_rawdev_pmd_get_named_dev(name);
	if (!rdev) {
		DPAA2_CMDIF_ERR("Invalid device name (%s)", name);
		return -EINVAL;
	}

	/* The primary process will only free the DPCI device */
	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		rte_dpaa2_free_dpci_dev(rdev->dev_private);

	ret = rte_rawdev_pmd_release(rdev);
	if (ret)
		DPAA2_CMDIF_DEBUG("Device cleanup failed");

	return 0;
}

static int
dpaa2_cmdif_probe(struct rte_vdev_device *vdev)
{
	const char *name;
	int ret = 0;

	name = rte_vdev_device_name(vdev);

	DPAA2_CMDIF_INFO("Init %s on NUMA node %d", name, rte_socket_id());

	ret = dpaa2_cmdif_create(name, vdev, rte_socket_id());

	return ret;
}

static int
dpaa2_cmdif_remove(struct rte_vdev_device *vdev)
{
	const char *name;
	int ret;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -1;

	DPAA2_CMDIF_INFO("Closing %s on NUMA node %d", name, rte_socket_id());

	ret = dpaa2_cmdif_destroy(name);

	return ret;
}

static struct rte_vdev_driver dpaa2_cmdif_drv = {
	.probe = dpaa2_cmdif_probe,
	.remove = dpaa2_cmdif_remove
};

RTE_PMD_REGISTER_VDEV(DPAA2_CMDIF_PMD_NAME, dpaa2_cmdif_drv);
RTE_LOG_REGISTER(dpaa2_cmdif_logtype, pmd.raw.dpaa2.cmdif, INFO);
