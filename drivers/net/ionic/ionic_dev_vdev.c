/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019-2024 Advanced Micro Devices, Inc.
 */

#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

#include <rte_errno.h>
#include <rte_common.h>
#include <rte_log.h>
#include <rte_eal.h>
#include <bus_vdev_driver.h>
#include <rte_dev.h>
#include <rte_string_fns.h>
#include <rte_kvargs.h>

#include "ionic.h"
#include "ionic_common.h"
#include "ionic_logs.h"
#include "ionic_ethdev.h"

#define IONIC_VDEV_DEV_BAR          0
#define IONIC_VDEV_INTR_CTL_BAR     1
#define IONIC_VDEV_INTR_CFG_BAR     2
#define IONIC_VDEV_DB_BAR           3
#define IONIC_VDEV_BARS_MAX         4

#define IONIC_VDEV_DEV_INFO_REGS_OFFSET      0x0000
#define IONIC_VDEV_DEV_CMD_REGS_OFFSET       0x0800

#define IONIC_VDEV_FW_WAIT_US       1000     /* 1ms */
#define IONIC_VDEV_FW_WAIT_MAX      5000     /* 5s */

static int
ionic_vdev_setup(struct ionic_adapter *adapter)
{
	struct ionic_bars *bars = &adapter->bars;
	struct ionic_dev *idev = &adapter->idev;
	uint8_t *bar0_base;
	uint32_t sig;
	uint32_t fw_waits = 0;
	uint8_t fw;

	IONIC_PRINT_CALL();

	/* BAR0: dev_cmd and interrupts */
	if (bars->num_bars < 1) {
		IONIC_PRINT(ERR, "No bars found, aborting");
		return -EFAULT;
	}

	bar0_base = bars->bar[IONIC_VDEV_DEV_BAR].vaddr;
	idev->dev_info = (union ionic_dev_info_regs *)
		&bar0_base[IONIC_VDEV_DEV_INFO_REGS_OFFSET];
	idev->dev_cmd = (union ionic_dev_cmd_regs *)
		&bar0_base[IONIC_VDEV_DEV_CMD_REGS_OFFSET];
	idev->intr_ctrl = (void *)bars->bar[IONIC_VDEV_INTR_CTL_BAR].vaddr;
	idev->db_pages = (void *)bars->bar[IONIC_VDEV_DB_BAR].vaddr;

	sig = ioread32(&idev->dev_info->signature);
	if (sig != IONIC_DEV_INFO_SIGNATURE) {
		IONIC_PRINT(ERR, "Incompatible firmware signature %x", sig);
		return -EFAULT;
	}

	/* Wait for the FW to indicate readiness */
	while (1) {
		fw = ioread8(&idev->dev_info->fw_status);
		if ((fw & IONIC_FW_STS_F_RUNNING) != 0)
			break;

		if (fw_waits > IONIC_VDEV_FW_WAIT_MAX) {
			IONIC_PRINT(ERR, "Firmware readiness bit not set");
			return -ETIMEDOUT;
		}

		fw_waits++;
		rte_delay_us_block(IONIC_VDEV_FW_WAIT_US);
	}
	IONIC_PRINT(DEBUG, "Firmware ready (%u waits)", fw_waits);

	adapter->name = rte_vdev_device_name(adapter->bus_dev);

	return 0;
}

static void
ionic_vdev_poll(struct ionic_adapter *adapter)
{
	ionic_dev_interrupt_handler(adapter);
}

static void
ionic_vdev_unmap_bars(struct ionic_adapter *adapter)
{
	struct ionic_bars *bars = &adapter->bars;
	uint32_t i;

	for (i = 0; i < IONIC_VDEV_BARS_MAX; i++)
		ionic_uio_rel_rsrc(adapter->name, i, &bars->bar[i]);
}

static const struct ionic_dev_intf ionic_vdev_intf = {
	.setup = ionic_vdev_setup,
	.poll = ionic_vdev_poll,
	.unmap_bars = ionic_vdev_unmap_bars,
};

static int
eth_ionic_vdev_probe(struct rte_vdev_device *vdev)
{
	struct ionic_bars bars = {};
	const char *name = rte_vdev_device_name(vdev);
	unsigned int i;

	IONIC_PRINT(NOTICE, "Initializing device %s",
		rte_eal_process_type() == RTE_PROC_SECONDARY ?
			"[SECONDARY]" : "");

	ionic_uio_scan_mnet_devices();

	for (i = 0; i < IONIC_VDEV_BARS_MAX; i++)
		ionic_uio_get_rsrc(name, i, &bars.bar[i]);

	bars.num_bars = IONIC_VDEV_BARS_MAX;

	return eth_ionic_dev_probe((void *)vdev,
			&vdev->device,
			&bars,
			&ionic_vdev_intf,
			IONIC_DEV_ID_ETH_VF,
			IONIC_PENSANDO_VENDOR_ID);
}

static int
eth_ionic_vdev_remove(struct rte_vdev_device *vdev)
{
	return eth_ionic_dev_remove(&vdev->device);
}

static struct rte_vdev_driver rte_vdev_ionic_pmd = {
	.probe = eth_ionic_vdev_probe,
	.remove = eth_ionic_vdev_remove,
};

RTE_PMD_REGISTER_VDEV(net_ionic, rte_vdev_ionic_pmd);
