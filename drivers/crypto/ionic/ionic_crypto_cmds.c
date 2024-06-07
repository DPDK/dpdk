/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021-2024 Advanced Micro Devices, Inc.
 */

#include <stdbool.h>

#include <rte_malloc.h>
#include <rte_memzone.h>

#include "ionic_crypto.h"

/* queuetype support level */
static const uint8_t iocpt_qtype_vers[IOCPT_QTYPE_MAX] = {
	[IOCPT_QTYPE_ADMINQ]  = 0,   /* 0 = Base version */
	[IOCPT_QTYPE_NOTIFYQ] = 0,   /* 0 = Base version */
	[IOCPT_QTYPE_CRYPTOQ] = 0,   /* 0 = Base version */
};

static const char *
iocpt_opcode_to_str(enum iocpt_cmd_opcode opcode)
{
	switch (opcode) {
	case IOCPT_CMD_NOP:
		return "IOCPT_CMD_NOP";
	case IOCPT_CMD_IDENTIFY:
		return "IOCPT_CMD_IDENTIFY";
	case IOCPT_CMD_RESET:
		return "IOCPT_CMD_RESET";
	case IOCPT_CMD_LIF_IDENTIFY:
		return "IOCPT_CMD_LIF_IDENTIFY";
	case IOCPT_CMD_LIF_INIT:
		return "IOCPT_CMD_LIF_INIT";
	case IOCPT_CMD_LIF_RESET:
		return "IOCPT_CMD_LIF_RESET";
	case IOCPT_CMD_LIF_GETATTR:
		return "IOCPT_CMD_LIF_GETATTR";
	case IOCPT_CMD_LIF_SETATTR:
		return "IOCPT_CMD_LIF_SETATTR";
	case IOCPT_CMD_Q_IDENTIFY:
		return "IOCPT_CMD_Q_IDENTIFY";
	case IOCPT_CMD_Q_INIT:
		return "IOCPT_CMD_Q_INIT";
	case IOCPT_CMD_Q_CONTROL:
		return "IOCPT_CMD_Q_CONTROL";
	case IOCPT_CMD_SESS_CONTROL:
		return "IOCPT_CMD_SESS_CONTROL";
	default:
		return "DEVCMD_UNKNOWN";
	}
}

/* Dev_cmd Interface */

static void
iocpt_dev_cmd_go(struct iocpt_dev *dev, union iocpt_dev_cmd *cmd)
{
	uint32_t cmd_size = RTE_DIM(cmd->words);
	uint32_t i;

	IOCPT_PRINT(DEBUG, "Sending %s (%d) via dev_cmd",
		iocpt_opcode_to_str(cmd->cmd.opcode), cmd->cmd.opcode);

	for (i = 0; i < cmd_size; i++)
		iowrite32(cmd->words[i], &dev->dev_cmd->cmd.words[i]);

	iowrite32(0, &dev->dev_cmd->done);
	iowrite32(1, &dev->dev_cmd->doorbell);
}

static int
iocpt_dev_cmd_wait(struct iocpt_dev *dev, unsigned long max_wait)
{
	unsigned long step_usec = IONIC_DEVCMD_CHECK_PERIOD_US;
	unsigned long max_wait_usec = max_wait * 1000000L;
	unsigned long elapsed_usec = 0;
	int done;

	/* Wait for dev cmd to complete.. but no more than max_wait sec */

	do {
		done = ioread32(&dev->dev_cmd->done) & IONIC_DEV_CMD_DONE;
		if (done != 0) {
			IOCPT_PRINT(DEBUG, "DEVCMD %d done took %lu usecs",
				ioread8(&dev->dev_cmd->cmd.cmd.opcode),
				elapsed_usec);
			return 0;
		}

		rte_delay_us_block(step_usec);

		elapsed_usec += step_usec;
	} while (elapsed_usec < max_wait_usec);

	IOCPT_PRINT(ERR, "DEVCMD %d timeout after %lu usecs",
		ioread8(&dev->dev_cmd->cmd.cmd.opcode), elapsed_usec);

	return -ETIMEDOUT;
}

static int
iocpt_dev_cmd_wait_check(struct iocpt_dev *dev, unsigned long max_wait)
{
	uint8_t status;
	int err;

	err = iocpt_dev_cmd_wait(dev, max_wait);
	if (err == 0) {
		status = ioread8(&dev->dev_cmd->comp.comp.status);
		if (status == IOCPT_RC_EAGAIN)
			err = -EAGAIN;
		else if (status != 0)
			err = -EIO;
	}

	IOCPT_PRINT(DEBUG, "dev_cmd returned %d", err);
	return err;
}

/* Dev_cmds */

static void
iocpt_dev_cmd_reset(struct iocpt_dev *dev)
{
	union iocpt_dev_cmd cmd = {
		.reset.opcode = IOCPT_CMD_RESET,
	};

	iocpt_dev_cmd_go(dev, &cmd);
}

static void
iocpt_dev_cmd_lif_identify(struct iocpt_dev *dev, uint8_t ver)
{
	union iocpt_dev_cmd cmd = {
		.lif_identify.opcode = IOCPT_CMD_LIF_IDENTIFY,
		.lif_identify.type = IOCPT_LIF_TYPE_DEFAULT,
		.lif_identify.ver = ver,
	};

	iocpt_dev_cmd_go(dev, &cmd);
}

static void
iocpt_dev_cmd_lif_init(struct iocpt_dev *dev, rte_iova_t info_pa)
{
	union iocpt_dev_cmd cmd = {
		.lif_init.opcode = IOCPT_CMD_LIF_INIT,
		.lif_init.type = IOCPT_LIF_TYPE_DEFAULT,
		.lif_init.info_pa = info_pa,
	};

	iocpt_dev_cmd_go(dev, &cmd);
}

static void
iocpt_dev_cmd_lif_reset(struct iocpt_dev *dev)
{
	union iocpt_dev_cmd cmd = {
		.lif_reset.opcode = IOCPT_CMD_LIF_RESET,
	};

	iocpt_dev_cmd_go(dev, &cmd);
}

static void
iocpt_dev_cmd_queue_identify(struct iocpt_dev *dev,
		uint8_t qtype, uint8_t qver)
{
	union iocpt_dev_cmd cmd = {
		.q_identify.opcode = IOCPT_CMD_Q_IDENTIFY,
		.q_identify.type = qtype,
		.q_identify.ver = qver,
	};

	iocpt_dev_cmd_go(dev, &cmd);
}

/* Dev_cmd consumers */

static void
iocpt_queue_identify(struct iocpt_dev *dev)
{
	union iocpt_q_identity *q_ident = &dev->ident.q;
	uint32_t q_words = RTE_DIM(q_ident->words);
	uint32_t cmd_words = RTE_DIM(dev->dev_cmd->data);
	uint32_t i, nwords, qtype;
	int err;

	for (qtype = 0; qtype < RTE_DIM(iocpt_qtype_vers); qtype++) {
		struct iocpt_qtype_info *qti = &dev->qtype_info[qtype];

		/* Filter out the types this driver knows about */
		switch (qtype) {
		case IOCPT_QTYPE_ADMINQ:
		case IOCPT_QTYPE_NOTIFYQ:
		case IOCPT_QTYPE_CRYPTOQ:
			break;
		default:
			continue;
		}

		memset(qti, 0, sizeof(*qti));

		if (iocpt_is_embedded()) {
			/* When embedded, FW will always match the driver */
			qti->version = iocpt_qtype_vers[qtype];
			continue;
		}

		/* On the host, query the FW for info */
		iocpt_dev_cmd_queue_identify(dev,
			qtype, iocpt_qtype_vers[qtype]);
		err = iocpt_dev_cmd_wait_check(dev, IONIC_DEVCMD_TIMEOUT);
		if (err == -EINVAL) {
			IOCPT_PRINT(ERR, "qtype %d not supported", qtype);
			continue;
		} else if (err == -EIO) {
			IOCPT_PRINT(ERR, "q_ident failed, older FW");
			return;
		} else if (err != 0) {
			IOCPT_PRINT(ERR, "q_ident failed, qtype %d: %d",
				qtype, err);
			return;
		}

		nwords = RTE_MIN(q_words, cmd_words);
		for (i = 0; i < nwords; i++)
			q_ident->words[i] = ioread32(&dev->dev_cmd->data[i]);

		qti->version   = q_ident->version;
		qti->supported = q_ident->supported;
		qti->features  = rte_le_to_cpu_64(q_ident->features);
		qti->desc_sz   = rte_le_to_cpu_16(q_ident->desc_sz);
		qti->comp_sz   = rte_le_to_cpu_16(q_ident->comp_sz);
		qti->sg_desc_sz = rte_le_to_cpu_16(q_ident->sg_desc_sz);
		qti->max_sg_elems = rte_le_to_cpu_16(q_ident->max_sg_elems);
		qti->sg_desc_stride =
			rte_le_to_cpu_16(q_ident->sg_desc_stride);

		IOCPT_PRINT(DEBUG, " qtype[%d].version = %d",
			qtype, qti->version);
		IOCPT_PRINT(DEBUG, " qtype[%d].supported = %#x",
			qtype, qti->supported);
		IOCPT_PRINT(DEBUG, " qtype[%d].features = %#jx",
			qtype, qti->features);
		IOCPT_PRINT(DEBUG, " qtype[%d].desc_sz = %d",
			qtype, qti->desc_sz);
		IOCPT_PRINT(DEBUG, " qtype[%d].comp_sz = %d",
			qtype, qti->comp_sz);
		IOCPT_PRINT(DEBUG, " qtype[%d].sg_desc_sz = %d",
			qtype, qti->sg_desc_sz);
		IOCPT_PRINT(DEBUG, " qtype[%d].max_sg_elems = %d",
			qtype, qti->max_sg_elems);
		IOCPT_PRINT(DEBUG, " qtype[%d].sg_desc_stride = %d",
			qtype, qti->sg_desc_stride);
	}
}

int
iocpt_dev_identify(struct iocpt_dev *dev)
{
	union iocpt_lif_identity *ident = &dev->ident.lif;
	union iocpt_lif_config *cfg = &ident->config;
	uint64_t features;
	uint32_t cmd_size = RTE_DIM(dev->dev_cmd->data);
	uint32_t dev_size = RTE_DIM(ident->words);
	uint32_t i, nwords;
	int err;

	memset(ident, 0, sizeof(*ident));

	iocpt_dev_cmd_lif_identify(dev, IOCPT_IDENTITY_VERSION_1);
	err = iocpt_dev_cmd_wait_check(dev, IONIC_DEVCMD_TIMEOUT);
	if (err != 0)
		return err;

	nwords = RTE_MIN(dev_size, cmd_size);
	for (i = 0; i < nwords; i++)
		ident->words[i] = ioread32(&dev->dev_cmd->data[i]);

	dev->max_qps =
		rte_le_to_cpu_32(cfg->queue_count[IOCPT_QTYPE_CRYPTOQ]);
	dev->max_sessions =
		rte_le_to_cpu_32(ident->max_nb_sessions);

	features = rte_le_to_cpu_64(ident->features);
	dev->features = RTE_CRYPTODEV_FF_HW_ACCELERATED;
	if (features & IOCPT_HW_SYM)
		dev->features |= RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO;
	if (features & IOCPT_HW_ASYM)
		dev->features |= RTE_CRYPTODEV_FF_ASYMMETRIC_CRYPTO;
	if (features & IOCPT_HW_CHAIN)
		dev->features |= RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING;
	if (features & IOCPT_HW_IP)
		dev->features |= RTE_CRYPTODEV_FF_IN_PLACE_SGL;
	if (features & IOCPT_HW_OOP) {
		dev->features |= RTE_CRYPTODEV_FF_OOP_SGL_IN_SGL_OUT;
		dev->features |= RTE_CRYPTODEV_FF_OOP_SGL_IN_LB_OUT;
		dev->features |= RTE_CRYPTODEV_FF_OOP_LB_IN_LB_OUT;
		dev->features |= RTE_CRYPTODEV_FF_OOP_LB_IN_SGL_OUT;
	}

	IOCPT_PRINT(INFO, "crypto.features %#jx",
		rte_le_to_cpu_64(ident->features));
	IOCPT_PRINT(INFO, "crypto.features_active %#jx",
		rte_le_to_cpu_64(cfg->features));
	IOCPT_PRINT(INFO, "crypto.queue_count[IOCPT_QTYPE_ADMINQ] %#x",
		rte_le_to_cpu_32(cfg->queue_count[IOCPT_QTYPE_ADMINQ]));
	IOCPT_PRINT(INFO, "crypto.queue_count[IOCPT_QTYPE_NOTIFYQ] %#x",
		rte_le_to_cpu_32(cfg->queue_count[IOCPT_QTYPE_NOTIFYQ]));
	IOCPT_PRINT(INFO, "crypto.queue_count[IOCPT_QTYPE_CRYPTOQ] %#x",
		rte_le_to_cpu_32(cfg->queue_count[IOCPT_QTYPE_CRYPTOQ]));
	IOCPT_PRINT(INFO, "crypto.max_sessions %u",
		rte_le_to_cpu_32(ident->max_nb_sessions));

	iocpt_queue_identify(dev);

	return 0;
}

int
iocpt_dev_init(struct iocpt_dev *dev, rte_iova_t info_pa)
{
	uint32_t retries = 5;
	int err;

retry_lif_init:
	iocpt_dev_cmd_lif_init(dev, info_pa);

	err = iocpt_dev_cmd_wait_check(dev, IONIC_DEVCMD_TIMEOUT);
	if (err == -EAGAIN && retries > 0) {
		retries--;
		rte_delay_us_block(IONIC_DEVCMD_RETRY_WAIT_US);
		goto retry_lif_init;
	}

	return err;
}

void
iocpt_dev_reset(struct iocpt_dev *dev)
{
	iocpt_dev_cmd_lif_reset(dev);
	(void)iocpt_dev_cmd_wait_check(dev, IONIC_DEVCMD_TIMEOUT);

	iocpt_dev_cmd_reset(dev);
	(void)iocpt_dev_cmd_wait_check(dev, IONIC_DEVCMD_TIMEOUT);
}
