/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright(c) 2018-2019 Pensando Systems, Inc. All rights reserved.
 */

#include <rte_malloc.h>

#include "ionic_dev.h"
#include "ionic.h"

int
ionic_dev_setup(struct ionic_adapter *adapter)
{
	struct ionic_dev_bar *bar = adapter->bars;
	unsigned int num_bars = adapter->num_bars;
	struct ionic_dev *idev = &adapter->idev;
	uint32_t sig;
	u_char *bar0_base;

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

	/* BAR1: doorbells */
	bar++;
	if (num_bars < 2) {
		IONIC_PRINT(ERR, "Doorbell bar missing, aborting");
		return -EFAULT;
	}

	idev->db_pages = bar->vaddr;
	idev->phy_db_pages = bar->bus_addr;

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
	unsigned int i;
	uint32_t comp_size = sizeof(comp->words) /
		sizeof(comp->words[0]);

	for (i = 0; i < comp_size; i++)
		comp->words[i] = ioread32(&idev->dev_cmd->comp.words[i]);
}

void
ionic_dev_cmd_go(struct ionic_dev *idev, union ionic_dev_cmd *cmd)
{
	unsigned int i;
	uint32_t cmd_size = sizeof(cmd->words) /
		sizeof(cmd->words[0]);

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
		.port_init.info_pa = idev->port_info_pa,
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
		.port_setattr.speed = speed,
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
		.port_setattr.mtu = mtu,
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
ionic_dev_cmd_lif_init(struct ionic_dev *idev, uint16_t lif_index,
		       rte_iova_t info_pa)
{
	union ionic_dev_cmd cmd = {
		.lif_init.opcode = IONIC_CMD_LIF_INIT,
		.lif_init.index = lif_index,
		.lif_init.info_pa = info_pa,
	};

	ionic_dev_cmd_go(idev, &cmd);
}

void
ionic_dev_cmd_lif_reset(struct ionic_dev *idev, uint16_t lif_index)
{
	union ionic_dev_cmd cmd = {
		.lif_init.opcode = IONIC_CMD_LIF_RESET,
		.lif_init.index = lif_index,
	};

	ionic_dev_cmd_go(idev, &cmd);
}
