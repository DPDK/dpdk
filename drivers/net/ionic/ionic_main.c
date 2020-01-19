/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright(c) 2018-2019 Pensando Systems, Inc. All rights reserved.
 */

#include <rte_memzone.h>

#include "ionic.h"

static int
ionic_dev_cmd_wait(struct ionic_dev *idev, unsigned long max_wait)
{
	unsigned long step_msec = 100;
	unsigned int max_wait_msec = max_wait * 1000;
	unsigned long elapsed_msec = 0;
	int done;

	/* Wait for dev cmd to complete.. but no more than max_wait sec */

	do {
		done = ionic_dev_cmd_done(idev);
		if (done) {
			IONIC_PRINT(DEBUG, "DEVCMD %d done took %ld msecs",
				idev->dev_cmd->cmd.cmd.opcode,
				elapsed_msec);
			return 0;
		}

		msec_delay(step_msec);

		elapsed_msec += step_msec;
	} while (elapsed_msec < max_wait_msec);

	IONIC_PRINT(DEBUG, "DEVCMD %d timeout after %ld msecs",
		idev->dev_cmd->cmd.cmd.opcode,
		elapsed_msec);

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
	if (err)
		return err;

	return ionic_dev_cmd_check_error(idev);
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
	int err = 0;
	uint32_t i;
	unsigned int nwords;
	uint32_t drv_size = sizeof(ident->drv.words) /
		sizeof(ident->drv.words[0]);
	uint32_t cmd_size = sizeof(idev->dev_cmd->data) /
		sizeof(idev->dev_cmd->data[0]);
	uint32_t dev_size = sizeof(ident->dev.words) /
		sizeof(ident->dev.words[0]);

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
	int err;

	ionic_dev_cmd_init(idev);
	err = ionic_dev_cmd_wait_check(idev, IONIC_DEVCMD_TIMEOUT);
	return err;
}

int
ionic_reset(struct ionic_adapter *adapter)
{
	struct ionic_dev *idev = &adapter->idev;
	int err;

	ionic_dev_cmd_reset(idev);
	err = ionic_dev_cmd_wait_check(idev, IONIC_DEVCMD_TIMEOUT);
	return err;
}

int
ionic_port_identify(struct ionic_adapter *adapter)
{
	struct ionic_dev *idev = &adapter->idev;
	struct ionic_identity *ident = &adapter->ident;
	unsigned int port_words = sizeof(ident->port.words) /
		sizeof(ident->port.words[0]);
	unsigned int cmd_words = sizeof(idev->dev_cmd->data) /
		sizeof(idev->dev_cmd->data[0]);
	unsigned int i;
	unsigned int nwords;
	int err;

	ionic_dev_cmd_port_identify(idev);
	err = ionic_dev_cmd_wait_check(idev, IONIC_DEVCMD_TIMEOUT);
	if (!err) {
		nwords = RTE_MIN(port_words, cmd_words);
		for (i = 0; i < nwords; i++)
			ident->port.words[i] =
				ioread32(&idev->dev_cmd->data[i]);
	}

	IONIC_PRINT(INFO, "speed %d ", ident->port.config.speed);
	IONIC_PRINT(INFO, "mtu %d ", ident->port.config.mtu);
	IONIC_PRINT(INFO, "state %d ", ident->port.config.state);
	IONIC_PRINT(INFO, "an_enable %d ", ident->port.config.an_enable);
	IONIC_PRINT(INFO, "fec_type %d ", ident->port.config.fec_type);
	IONIC_PRINT(INFO, "pause_type %d ", ident->port.config.pause_type);
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
	unsigned int config_words = sizeof(ident->port.config.words) /
		sizeof(ident->port.config.words[0]);
	unsigned int cmd_words = sizeof(idev->dev_cmd->data) /
		sizeof(idev->dev_cmd->data[0]);
	unsigned int nwords;
	unsigned int i;
	int err;

	if (idev->port_info)
		return 0;

	idev->port_info_sz = RTE_ALIGN(sizeof(*idev->port_info), PAGE_SIZE);

	snprintf(z_name, sizeof(z_name), "%s_port_%s_info",
		IONIC_DRV_NAME,
		adapter->pci_dev->device.name);

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

	ionic_dev_cmd_port_init(idev);
	err = ionic_dev_cmd_wait_check(idev, IONIC_DEVCMD_TIMEOUT);
	if (err) {
		IONIC_PRINT(ERR, "Failed to init port");
		return err;
	}

	ionic_dev_cmd_port_state(idev, IONIC_PORT_ADMIN_STATE_UP);
	err = ionic_dev_cmd_wait_check(idev, IONIC_DEVCMD_TIMEOUT);
	if (err) {
		IONIC_PRINT(WARNING, "Failed to bring port UP");
		return err;
	}

	return 0;
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
