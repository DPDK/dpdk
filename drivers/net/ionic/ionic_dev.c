/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright(c) 2018-2019 Pensando Systems, Inc. All rights reserved.
 */

#include <stdbool.h>

#include <rte_malloc.h>

#include "ionic_dev.h"
#include "ionic_lif.h"
#include "ionic.h"

int
ionic_dev_setup(struct ionic_adapter *adapter)
{
	struct ionic_dev_bar *bar = adapter->bars;
	unsigned int num_bars = adapter->num_bars;
	struct ionic_dev *idev = &adapter->idev;
	uint32_t sig;
	u_char *bar0_base;
	unsigned int i;

	/* BAR0: dev_cmd and interrupts */
	if (num_bars < 1) {
		IONIC_PRINT(ERR, "No bars found, aborting");
		return -EFAULT;
	}

	if (bar->len < IONIC_BAR0_SIZE) {
		IONIC_PRINT(ERR,
			"Resource bar size %lu too small, aborting",
			bar->len);
		return -EFAULT;
	}

	bar0_base = bar->vaddr;
	idev->dev_info = (union ionic_dev_info_regs *)
		&bar0_base[IONIC_BAR0_DEV_INFO_REGS_OFFSET];
	idev->dev_cmd = (union ionic_dev_cmd_regs *)
		&bar0_base[IONIC_BAR0_DEV_CMD_REGS_OFFSET];
	idev->intr_status = (struct ionic_intr_status *)
		&bar0_base[IONIC_BAR0_INTR_STATUS_OFFSET];
	idev->intr_ctrl = (struct ionic_intr *)
		&bar0_base[IONIC_BAR0_INTR_CTRL_OFFSET];

	sig = ioread32(&idev->dev_info->signature);
	if (sig != IONIC_DEV_INFO_SIGNATURE) {
		IONIC_PRINT(ERR, "Incompatible firmware signature %" PRIx32 "",
			sig);
		return -EFAULT;
	}

	for (i = 0; i < IONIC_DEVINFO_FWVERS_BUFLEN; i++)
		adapter->fw_version[i] =
			ioread8(&idev->dev_info->fw_version[i]);
	adapter->fw_version[IONIC_DEVINFO_FWVERS_BUFLEN - 1] = '\0';

	IONIC_PRINT(DEBUG, "Firmware version: %s", adapter->fw_version);

	/* BAR1: doorbells */
	bar++;
	if (num_bars < 2) {
		IONIC_PRINT(ERR, "Doorbell bar missing, aborting");
		return -EFAULT;
	}

	idev->db_pages = bar->vaddr;

	return 0;
}

/* Devcmd Interface */

uint8_t
ionic_dev_cmd_status(struct ionic_dev *idev)
{
	return ioread8(&idev->dev_cmd->comp.comp.status);
}

bool
ionic_dev_cmd_done(struct ionic_dev *idev)
{
	return ioread32(&idev->dev_cmd->done) & IONIC_DEV_CMD_DONE;
}

void
ionic_dev_cmd_comp(struct ionic_dev *idev, void *mem)
{
	union ionic_dev_cmd_comp *comp = mem;
	uint32_t comp_size = RTE_DIM(comp->words);
	uint32_t i;

	for (i = 0; i < comp_size; i++)
		comp->words[i] = ioread32(&idev->dev_cmd->comp.words[i]);
}

void
ionic_dev_cmd_go(struct ionic_dev *idev, union ionic_dev_cmd *cmd)
{
	uint32_t cmd_size = RTE_DIM(cmd->words);
	uint32_t i;

	IONIC_PRINT(DEBUG, "Sending %s (%d) via dev_cmd",
		    ionic_opcode_to_str(cmd->cmd.opcode), cmd->cmd.opcode);

	for (i = 0; i < cmd_size; i++)
		iowrite32(cmd->words[i], &idev->dev_cmd->cmd.words[i]);

	iowrite32(0, &idev->dev_cmd->done);
	iowrite32(1, &idev->dev_cmd->doorbell);
}

/* Device commands */

void
ionic_dev_cmd_identify(struct ionic_dev *idev, uint8_t ver)
{
	union ionic_dev_cmd cmd = {
		.identify.opcode = IONIC_CMD_IDENTIFY,
		.identify.ver = ver,
	};

	ionic_dev_cmd_go(idev, &cmd);
}

void
ionic_dev_cmd_init(struct ionic_dev *idev)
{
	union ionic_dev_cmd cmd = {
		.init.opcode = IONIC_CMD_INIT,
		.init.type = 0,
	};

	ionic_dev_cmd_go(idev, &cmd);
}

void
ionic_dev_cmd_reset(struct ionic_dev *idev)
{
	union ionic_dev_cmd cmd = {
		.reset.opcode = IONIC_CMD_RESET,
	};

	ionic_dev_cmd_go(idev, &cmd);
}

/* Port commands */

void
ionic_dev_cmd_port_identify(struct ionic_dev *idev)
{
	union ionic_dev_cmd cmd = {
		.port_init.opcode = IONIC_CMD_PORT_IDENTIFY,
		.port_init.index = 0,
	};

	ionic_dev_cmd_go(idev, &cmd);
}

void
ionic_dev_cmd_port_init(struct ionic_dev *idev)
{
	union ionic_dev_cmd cmd = {
		.port_init.opcode = IONIC_CMD_PORT_INIT,
		.port_init.index = 0,
		.port_init.info_pa = rte_cpu_to_le_64(idev->port_info_pa),
	};

	ionic_dev_cmd_go(idev, &cmd);
}

void
ionic_dev_cmd_port_reset(struct ionic_dev *idev)
{
	union ionic_dev_cmd cmd = {
		.port_reset.opcode = IONIC_CMD_PORT_RESET,
		.port_reset.index = 0,
	};

	ionic_dev_cmd_go(idev, &cmd);
}

void
ionic_dev_cmd_port_state(struct ionic_dev *idev, uint8_t state)
{
	union ionic_dev_cmd cmd = {
		.port_setattr.opcode = IONIC_CMD_PORT_SETATTR,
		.port_setattr.index = 0,
		.port_setattr.attr = IONIC_PORT_ATTR_STATE,
		.port_setattr.state = state,
	};

	ionic_dev_cmd_go(idev, &cmd);
}

void
ionic_dev_cmd_port_speed(struct ionic_dev *idev, uint32_t speed)
{
	union ionic_dev_cmd cmd = {
		.port_setattr.opcode = IONIC_CMD_PORT_SETATTR,
		.port_setattr.index = 0,
		.port_setattr.attr = IONIC_PORT_ATTR_SPEED,
		.port_setattr.speed = rte_cpu_to_le_32(speed),
	};

	ionic_dev_cmd_go(idev, &cmd);
}

void
ionic_dev_cmd_port_mtu(struct ionic_dev *idev, uint32_t mtu)
{
	union ionic_dev_cmd cmd = {
		.port_setattr.opcode = IONIC_CMD_PORT_SETATTR,
		.port_setattr.index = 0,
		.port_setattr.attr = IONIC_PORT_ATTR_MTU,
		.port_setattr.mtu = rte_cpu_to_le_32(mtu),
	};

	ionic_dev_cmd_go(idev, &cmd);
}

void
ionic_dev_cmd_port_autoneg(struct ionic_dev *idev, uint8_t an_enable)
{
	union ionic_dev_cmd cmd = {
		.port_setattr.opcode = IONIC_CMD_PORT_SETATTR,
		.port_setattr.index = 0,
		.port_setattr.attr = IONIC_PORT_ATTR_AUTONEG,
		.port_setattr.an_enable = an_enable,
	};

	ionic_dev_cmd_go(idev, &cmd);
}

void
ionic_dev_cmd_port_fec(struct ionic_dev *idev, uint8_t fec_type)
{
	union ionic_dev_cmd cmd = {
		.port_setattr.opcode = IONIC_CMD_PORT_SETATTR,
		.port_setattr.index = 0,
		.port_setattr.attr = IONIC_PORT_ATTR_FEC,
		.port_setattr.fec_type = fec_type,
	};

	ionic_dev_cmd_go(idev, &cmd);
}

void
ionic_dev_cmd_port_pause(struct ionic_dev *idev, uint8_t pause_type)
{
	union ionic_dev_cmd cmd = {
		.port_setattr.opcode = IONIC_CMD_PORT_SETATTR,
		.port_setattr.index = 0,
		.port_setattr.attr = IONIC_PORT_ATTR_PAUSE,
		.port_setattr.pause_type = pause_type,
	};

	ionic_dev_cmd_go(idev, &cmd);
}

void
ionic_dev_cmd_port_loopback(struct ionic_dev *idev, uint8_t loopback_mode)
{
	union ionic_dev_cmd cmd = {
		.port_setattr.opcode = IONIC_CMD_PORT_SETATTR,
		.port_setattr.index = 0,
		.port_setattr.attr = IONIC_PORT_ATTR_LOOPBACK,
		.port_setattr.loopback_mode = loopback_mode,
	};

	ionic_dev_cmd_go(idev, &cmd);
}

/* LIF commands */

void
ionic_dev_cmd_queue_identify(struct ionic_dev *idev,
		uint16_t lif_type, uint8_t qtype, uint8_t qver)
{
	union ionic_dev_cmd cmd = {
		.q_identify.opcode = IONIC_CMD_Q_IDENTIFY,
		.q_identify.lif_type = rte_cpu_to_le_16(lif_type),
		.q_identify.type = qtype,
		.q_identify.ver = qver,
	};

	ionic_dev_cmd_go(idev, &cmd);
}

void
ionic_dev_cmd_lif_identify(struct ionic_dev *idev, uint8_t type, uint8_t ver)
{
	union ionic_dev_cmd cmd = {
		.lif_identify.opcode = IONIC_CMD_LIF_IDENTIFY,
		.lif_identify.type = type,
		.lif_identify.ver = ver,
	};

	ionic_dev_cmd_go(idev, &cmd);
}

void
ionic_dev_cmd_lif_init(struct ionic_dev *idev, rte_iova_t info_pa)
{
	union ionic_dev_cmd cmd = {
		.lif_init.opcode = IONIC_CMD_LIF_INIT,
		.lif_init.info_pa = rte_cpu_to_le_64(info_pa),
	};

	ionic_dev_cmd_go(idev, &cmd);
}

void
ionic_dev_cmd_lif_reset(struct ionic_dev *idev)
{
	union ionic_dev_cmd cmd = {
		.lif_init.opcode = IONIC_CMD_LIF_RESET,
	};

	ionic_dev_cmd_go(idev, &cmd);
}

struct ionic_doorbell *
ionic_db_map(struct ionic_lif *lif, struct ionic_queue *q)
{
	return lif->kern_dbpage + q->hw_type;
}

void
ionic_intr_init(struct ionic_dev *idev, struct ionic_intr_info *intr,
		unsigned long index)
{
	ionic_intr_clean(idev->intr_ctrl, index);
	intr->index = index;
}

void
ionic_dev_cmd_adminq_init(struct ionic_dev *idev, struct ionic_qcq *qcq)
{
	struct ionic_queue *q = &qcq->q;
	struct ionic_cq *cq = &qcq->cq;

	union ionic_dev_cmd cmd = {
		.q_init.opcode = IONIC_CMD_Q_INIT,
		.q_init.type = q->type,
		.q_init.ver = qcq->lif->qtype_info[q->type].version,
		.q_init.index = rte_cpu_to_le_32(q->index),
		.q_init.flags = rte_cpu_to_le_16(IONIC_QINIT_F_ENA),
		.q_init.intr_index = rte_cpu_to_le_16(IONIC_INTR_NONE),
		.q_init.ring_size = rte_log2_u32(q->num_descs),
		.q_init.ring_base = rte_cpu_to_le_64(q->base_pa),
		.q_init.cq_ring_base = rte_cpu_to_le_64(cq->base_pa),
	};

	IONIC_PRINT(DEBUG, "adminq.q_init.ver %u", cmd.q_init.ver);

	ionic_dev_cmd_go(idev, &cmd);
}

int
ionic_cq_init(struct ionic_cq *cq, uint16_t num_descs)
{
	if (!rte_is_power_of_2(num_descs) ||
	    num_descs < IONIC_MIN_RING_DESC ||
	    num_descs > IONIC_MAX_RING_DESC) {
		IONIC_PRINT(ERR, "%u descriptors (min: %u max: %u)",
			num_descs, IONIC_MIN_RING_DESC, IONIC_MAX_RING_DESC);
		return -EINVAL;
	}

	cq->num_descs = num_descs;
	cq->size_mask = num_descs - 1;
	cq->tail_idx = 0;
	cq->done_color = 1;

	return 0;
}

void
ionic_cq_map(struct ionic_cq *cq, void *base, rte_iova_t base_pa)
{
	cq->base = base;
	cq->base_pa = base_pa;
}

uint32_t
ionic_cq_service(struct ionic_cq *cq, uint32_t work_to_do,
		 ionic_cq_cb cb, void *cb_arg)
{
	uint32_t work_done = 0;

	if (work_to_do == 0)
		return 0;

	while (cb(cq, cq->tail_idx, cb_arg)) {
		cq->tail_idx = Q_NEXT_TO_SRVC(cq, 1);
		if (cq->tail_idx == 0)
			cq->done_color = !cq->done_color;

		if (++work_done == work_to_do)
			break;
	}

	return work_done;
}

int
ionic_q_init(struct ionic_queue *q, uint32_t index, uint16_t num_descs)
{
	uint32_t ring_size;

	if (!rte_is_power_of_2(num_descs))
		return -EINVAL;

	ring_size = rte_log2_u32(num_descs);
	if (ring_size < 2 || ring_size > 16)
		return -EINVAL;

	q->index = index;
	q->num_descs = num_descs;
	q->size_mask = num_descs - 1;
	q->head_idx = 0;
	q->tail_idx = 0;

	return 0;
}

void
ionic_q_map(struct ionic_queue *q, void *base, rte_iova_t base_pa)
{
	q->base = base;
	q->base_pa = base_pa;
}

void
ionic_q_sg_map(struct ionic_queue *q, void *base, rte_iova_t base_pa)
{
	q->sg_base = base;
	q->sg_base_pa = base_pa;
}
