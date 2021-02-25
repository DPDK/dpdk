/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright(c) 2018-2019 Pensando Systems, Inc. All rights reserved.
 */

#include <stdbool.h>

#include <rte_memzone.h>

#include "ionic.h"
#include "ionic_ethdev.h"
#include "ionic_lif.h"

static const char *
ionic_error_to_str(enum ionic_status_code code)
{
	switch (code) {
	case IONIC_RC_SUCCESS:
		return "IONIC_RC_SUCCESS";
	case IONIC_RC_EVERSION:
		return "IONIC_RC_EVERSION";
	case IONIC_RC_EOPCODE:
		return "IONIC_RC_EOPCODE";
	case IONIC_RC_EIO:
		return "IONIC_RC_EIO";
	case IONIC_RC_EPERM:
		return "IONIC_RC_EPERM";
	case IONIC_RC_EQID:
		return "IONIC_RC_EQID";
	case IONIC_RC_EQTYPE:
		return "IONIC_RC_EQTYPE";
	case IONIC_RC_ENOENT:
		return "IONIC_RC_ENOENT";
	case IONIC_RC_EINTR:
		return "IONIC_RC_EINTR";
	case IONIC_RC_EAGAIN:
		return "IONIC_RC_EAGAIN";
	case IONIC_RC_ENOMEM:
		return "IONIC_RC_ENOMEM";
	case IONIC_RC_EFAULT:
		return "IONIC_RC_EFAULT";
	case IONIC_RC_EBUSY:
		return "IONIC_RC_EBUSY";
	case IONIC_RC_EEXIST:
		return "IONIC_RC_EEXIST";
	case IONIC_RC_EINVAL:
		return "IONIC_RC_EINVAL";
	case IONIC_RC_ENOSPC:
		return "IONIC_RC_ENOSPC";
	case IONIC_RC_ERANGE:
		return "IONIC_RC_ERANGE";
	case IONIC_RC_BAD_ADDR:
		return "IONIC_RC_BAD_ADDR";
	case IONIC_RC_DEV_CMD:
		return "IONIC_RC_DEV_CMD";
	case IONIC_RC_ERROR:
		return "IONIC_RC_ERROR";
	case IONIC_RC_ERDMA:
		return "IONIC_RC_ERDMA";
	default:
		return "IONIC_RC_UNKNOWN";
	}
}

const char *
ionic_opcode_to_str(enum ionic_cmd_opcode opcode)
{
	switch (opcode) {
	case IONIC_CMD_NOP:
		return "IONIC_CMD_NOP";
	case IONIC_CMD_INIT:
		return "IONIC_CMD_INIT";
	case IONIC_CMD_RESET:
		return "IONIC_CMD_RESET";
	case IONIC_CMD_IDENTIFY:
		return "IONIC_CMD_IDENTIFY";
	case IONIC_CMD_GETATTR:
		return "IONIC_CMD_GETATTR";
	case IONIC_CMD_SETATTR:
		return "IONIC_CMD_SETATTR";
	case IONIC_CMD_PORT_IDENTIFY:
		return "IONIC_CMD_PORT_IDENTIFY";
	case IONIC_CMD_PORT_INIT:
		return "IONIC_CMD_PORT_INIT";
	case IONIC_CMD_PORT_RESET:
		return "IONIC_CMD_PORT_RESET";
	case IONIC_CMD_PORT_GETATTR:
		return "IONIC_CMD_PORT_GETATTR";
	case IONIC_CMD_PORT_SETATTR:
		return "IONIC_CMD_PORT_SETATTR";
	case IONIC_CMD_LIF_INIT:
		return "IONIC_CMD_LIF_INIT";
	case IONIC_CMD_LIF_RESET:
		return "IONIC_CMD_LIF_RESET";
	case IONIC_CMD_LIF_IDENTIFY:
		return "IONIC_CMD_LIF_IDENTIFY";
	case IONIC_CMD_LIF_SETATTR:
		return "IONIC_CMD_LIF_SETATTR";
	case IONIC_CMD_LIF_GETATTR:
		return "IONIC_CMD_LIF_GETATTR";
	case IONIC_CMD_RX_MODE_SET:
		return "IONIC_CMD_RX_MODE_SET";
	case IONIC_CMD_RX_FILTER_ADD:
		return "IONIC_CMD_RX_FILTER_ADD";
	case IONIC_CMD_RX_FILTER_DEL:
		return "IONIC_CMD_RX_FILTER_DEL";
	case IONIC_CMD_Q_INIT:
		return "IONIC_CMD_Q_INIT";
	case IONIC_CMD_Q_CONTROL:
		return "IONIC_CMD_Q_CONTROL";
	case IONIC_CMD_Q_IDENTIFY:
		return "IONIC_CMD_Q_IDENTIFY";
	case IONIC_CMD_RDMA_RESET_LIF:
		return "IONIC_CMD_RDMA_RESET_LIF";
	case IONIC_CMD_RDMA_CREATE_EQ:
		return "IONIC_CMD_RDMA_CREATE_EQ";
	case IONIC_CMD_RDMA_CREATE_CQ:
		return "IONIC_CMD_RDMA_CREATE_CQ";
	case IONIC_CMD_RDMA_CREATE_ADMINQ:
		return "IONIC_CMD_RDMA_CREATE_ADMINQ";
	default:
		return "DEVCMD_UNKNOWN";
	}
}

static int
ionic_adminq_check_err(struct ionic_admin_ctx *ctx, bool timeout)
{
	const char *name;
	const char *status;

	name = ionic_opcode_to_str(ctx->cmd.cmd.opcode);

	if (ctx->comp.comp.status || timeout) {
		status = ionic_error_to_str(ctx->comp.comp.status);
		IONIC_PRINT(ERR, "%s (%d) failed: %s (%d)",
			name,
			ctx->cmd.cmd.opcode,
			timeout ? "TIMEOUT" : status,
			timeout ? -1 : ctx->comp.comp.status);
		return -EIO;
	}

	IONIC_PRINT(DEBUG, "%s (%d) succeeded", name, ctx->cmd.cmd.opcode);

	return 0;
}

static bool
ionic_adminq_service(struct ionic_cq *cq, uint16_t cq_desc_index,
		void *cb_arg __rte_unused)
{
	struct ionic_admin_comp *cq_desc_base = cq->base;
	struct ionic_admin_comp *cq_desc = &cq_desc_base[cq_desc_index];
	struct ionic_qcq *qcq = IONIC_CQ_TO_QCQ(cq);
	struct ionic_queue *q = &qcq->q;
	struct ionic_admin_ctx *ctx;
	uint16_t curr_q_tail_idx;
	uint16_t stop_index;
	void **info;

	if (!color_match(cq_desc->color, cq->done_color))
		return false;

	stop_index = rte_le_to_cpu_16(cq_desc->comp_index);

	do {
		info = IONIC_INFO_PTR(q, q->tail_idx);

		ctx = info[0];
		if (ctx) {
			memcpy(&ctx->comp, cq_desc, sizeof(*cq_desc));

			ctx->pending_work = false; /* done */
		}

		curr_q_tail_idx = q->tail_idx;
		q->tail_idx = Q_NEXT_TO_SRVC(q, 1);
	} while (curr_q_tail_idx != stop_index);

	return true;
}

/** ionic_adminq_post - Post an admin command.
 * @lif:                Handle to lif.
 * @cmd_ctx:            Api admin command context.
 *
 * Post the command to an admin queue in the ethernet driver.  If this command
 * succeeds, then the command has been posted, but that does not indicate a
 * completion.  If this command returns success, then the completion callback
 * will eventually be called.
 *
 * Return: zero or negative error status.
 */
static int
ionic_adminq_post(struct ionic_lif *lif, struct ionic_admin_ctx *ctx)
{
	struct ionic_queue *q = &lif->adminqcq->qcq.q;
	struct ionic_admin_cmd *q_desc_base = q->base;
	struct ionic_admin_cmd *q_desc;
	void **info;
	int err = 0;

	rte_spinlock_lock(&lif->adminq_lock);

	if (ionic_q_space_avail(q) < 1) {
		err = -ENOSPC;
		goto err_out;
	}

	q_desc = &q_desc_base[q->head_idx];

	memcpy(q_desc, &ctx->cmd, sizeof(ctx->cmd));

	info = IONIC_INFO_PTR(q, q->head_idx);
	info[0] = ctx;

	q->head_idx = Q_NEXT_TO_POST(q, 1);

	/* Ring doorbell */
	rte_wmb();
	ionic_q_flush(q);

err_out:
	rte_spinlock_unlock(&lif->adminq_lock);

	return err;
}

static int
ionic_adminq_wait_for_completion(struct ionic_lif *lif,
		struct ionic_admin_ctx *ctx, unsigned long max_wait)
{
	unsigned long step_usec = IONIC_DEVCMD_CHECK_PERIOD_US;
	unsigned long max_wait_usec = max_wait * 1000000L;
	unsigned long elapsed_usec = 0;
	int budget = 8;

	while (ctx->pending_work && elapsed_usec < max_wait_usec) {
		/*
		 * Locking here as adminq is served inline and could be
		 * called from multiple places
		 */
		rte_spinlock_lock(&lif->adminq_service_lock);

		ionic_qcq_service(&lif->adminqcq->qcq, budget,
				ionic_adminq_service, NULL);

		rte_spinlock_unlock(&lif->adminq_service_lock);

		rte_delay_us_block(step_usec);
		elapsed_usec += step_usec;
	}

	return (!ctx->pending_work);
}

int
ionic_adminq_post_wait(struct ionic_lif *lif, struct ionic_admin_ctx *ctx)
{
	bool done;
	int err;

	IONIC_PRINT(DEBUG, "Sending %s (%d) via the admin queue",
		ionic_opcode_to_str(ctx->cmd.cmd.opcode), ctx->cmd.cmd.opcode);

	err = ionic_adminq_post(lif, ctx);
	if (err) {
		IONIC_PRINT(ERR, "Failure posting %d to the admin queue (%d)",
			ctx->cmd.cmd.opcode, err);
		return err;
	}

	done = ionic_adminq_wait_for_completion(lif, ctx,
		IONIC_DEVCMD_TIMEOUT);

	return ionic_adminq_check_err(ctx, !done /* timed out */);
}

static int
ionic_dev_cmd_wait(struct ionic_dev *idev, unsigned long max_wait)
{
	unsigned long step_usec = IONIC_DEVCMD_CHECK_PERIOD_US;
	unsigned long max_wait_usec = max_wait * 1000000L;
	unsigned long elapsed_usec = 0;
	int done;

	/* Wait for dev cmd to complete.. but no more than max_wait sec */

	do {
		done = ionic_dev_cmd_done(idev);
		if (done) {
			IONIC_PRINT(DEBUG, "DEVCMD %d done took %ld usecs",
				ioread8(&idev->dev_cmd->cmd.cmd.opcode),
				elapsed_usec);
			return 0;
		}

		rte_delay_us_block(step_usec);

		elapsed_usec += step_usec;
	} while (elapsed_usec < max_wait_usec);

	IONIC_PRINT(ERR, "DEVCMD %d timeout after %ld usecs",
		ioread8(&idev->dev_cmd->cmd.cmd.opcode),
		elapsed_usec);

	return -ETIMEDOUT;
}

static int
ionic_dev_cmd_check_error(struct ionic_dev *idev)
{
	uint8_t status;

	status = ionic_dev_cmd_status(idev);
	if (status == 0)
		return 0;

	return -EIO;
}

int
ionic_dev_cmd_wait_check(struct ionic_dev *idev, unsigned long max_wait)
{
	int err;

	err = ionic_dev_cmd_wait(idev, max_wait);

	if (!err)
		err = ionic_dev_cmd_check_error(idev);

	IONIC_PRINT(DEBUG, "dev_cmd returned %d", err);
	return err;
}

int
ionic_setup(struct ionic_adapter *adapter)
{
	return ionic_dev_setup(adapter);
}

int
ionic_identify(struct ionic_adapter *adapter)
{
	struct ionic_dev *idev = &adapter->idev;
	struct ionic_identity *ident = &adapter->ident;
	uint32_t drv_size = RTE_DIM(ident->drv.words);
	uint32_t cmd_size = RTE_DIM(idev->dev_cmd->data);
	uint32_t dev_size = RTE_DIM(ident->dev.words);
	uint32_t i, nwords;
	int err;

	memset(ident, 0, sizeof(*ident));

	ident->drv.os_type = IONIC_OS_TYPE_LINUX;
	ident->drv.os_dist = 0;
	snprintf(ident->drv.os_dist_str,
		sizeof(ident->drv.os_dist_str), "Unknown");
	ident->drv.kernel_ver = 0;
	snprintf(ident->drv.kernel_ver_str,
		sizeof(ident->drv.kernel_ver_str), "DPDK");
	strncpy(ident->drv.driver_ver_str, IONIC_DRV_VERSION,
		sizeof(ident->drv.driver_ver_str) - 1);

	nwords = RTE_MIN(drv_size, cmd_size);
	for (i = 0; i < nwords; i++)
		iowrite32(ident->drv.words[i], &idev->dev_cmd->data[i]);

	ionic_dev_cmd_identify(idev, IONIC_IDENTITY_VERSION_1);
	err = ionic_dev_cmd_wait_check(idev, IONIC_DEVCMD_TIMEOUT);
	if (!err) {
		nwords = RTE_MIN(dev_size, cmd_size);
		for (i = 0; i < nwords; i++)
			ident->dev.words[i] = ioread32(&idev->dev_cmd->data[i]);
	}

	return err;
}

int
ionic_init(struct ionic_adapter *adapter)
{
	struct ionic_dev *idev = &adapter->idev;

	ionic_dev_cmd_init(idev);
	return ionic_dev_cmd_wait_check(idev, IONIC_DEVCMD_TIMEOUT);
}

int
ionic_reset(struct ionic_adapter *adapter)
{
	struct ionic_dev *idev = &adapter->idev;

	ionic_dev_cmd_reset(idev);
	return ionic_dev_cmd_wait_check(idev, IONIC_DEVCMD_TIMEOUT);
}

int
ionic_port_identify(struct ionic_adapter *adapter)
{
	struct ionic_dev *idev = &adapter->idev;
	struct ionic_identity *ident = &adapter->ident;
	uint32_t port_words = RTE_DIM(ident->port.words);
	uint32_t cmd_words = RTE_DIM(idev->dev_cmd->data);
	uint32_t i, nwords;
	int err;

	ionic_dev_cmd_port_identify(idev);
	err = ionic_dev_cmd_wait_check(idev, IONIC_DEVCMD_TIMEOUT);
	if (!err) {
		nwords = RTE_MIN(port_words, cmd_words);
		for (i = 0; i < nwords; i++)
			ident->port.words[i] =
				ioread32(&idev->dev_cmd->data[i]);
	}

	IONIC_PRINT(INFO, "speed %d",
		rte_le_to_cpu_32(ident->port.config.speed));
	IONIC_PRINT(INFO, "mtu %d",
		rte_le_to_cpu_32(ident->port.config.mtu));
	IONIC_PRINT(INFO, "state %d", ident->port.config.state);
	IONIC_PRINT(INFO, "an_enable %d", ident->port.config.an_enable);
	IONIC_PRINT(INFO, "fec_type %d", ident->port.config.fec_type);
	IONIC_PRINT(INFO, "pause_type %d", ident->port.config.pause_type);
	IONIC_PRINT(INFO, "loopback_mode %d",
		ident->port.config.loopback_mode);

	return err;
}

static const struct rte_memzone *
ionic_memzone_reserve(const char *name, uint32_t len, int socket_id)
{
	const struct rte_memzone *mz;

	mz = rte_memzone_lookup(name);
	if (mz)
		return mz;

	mz = rte_memzone_reserve_aligned(name, len, socket_id,
		RTE_MEMZONE_IOVA_CONTIG, IONIC_ALIGN);
	return mz;
}

int
ionic_port_init(struct ionic_adapter *adapter)
{
	struct ionic_dev *idev = &adapter->idev;
	struct ionic_identity *ident = &adapter->ident;
	char z_name[RTE_MEMZONE_NAMESIZE];
	uint32_t config_words = RTE_DIM(ident->port.config.words);
	uint32_t cmd_words = RTE_DIM(idev->dev_cmd->data);
	uint32_t i, nwords;
	int err;

	if (idev->port_info)
		return 0;

	idev->port_info_sz = RTE_ALIGN(sizeof(*idev->port_info),
			rte_mem_page_size());

	snprintf(z_name, sizeof(z_name), "%s_port_%s_info",
		IONIC_DRV_NAME, adapter->name);

	idev->port_info_z = ionic_memzone_reserve(z_name, idev->port_info_sz,
		SOCKET_ID_ANY);
	if (!idev->port_info_z) {
		IONIC_PRINT(ERR, "Cannot reserve port info DMA memory");
		return -ENOMEM;
	}

	idev->port_info = idev->port_info_z->addr;
	idev->port_info_pa = idev->port_info_z->iova;

	nwords = RTE_MIN(config_words, cmd_words);

	for (i = 0; i < nwords; i++)
		iowrite32(ident->port.config.words[i], &idev->dev_cmd->data[i]);

	idev->port_info->config.state = IONIC_PORT_ADMIN_STATE_UP;
	ionic_dev_cmd_port_init(idev);
	err = ionic_dev_cmd_wait_check(idev, IONIC_DEVCMD_TIMEOUT);
	if (err)
		IONIC_PRINT(ERR, "Failed to init port");

	return err;
}

int
ionic_port_reset(struct ionic_adapter *adapter)
{
	struct ionic_dev *idev = &adapter->idev;
	int err;

	if (!idev->port_info)
		return 0;

	ionic_dev_cmd_port_reset(idev);
	err = ionic_dev_cmd_wait_check(idev, IONIC_DEVCMD_TIMEOUT);
	if (err) {
		IONIC_PRINT(ERR, "Failed to reset port");
		return err;
	}

	idev->port_info = NULL;
	idev->port_info_pa = 0;

	return 0;
}
