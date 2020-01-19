/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright(c) 2018-2019 Pensando Systems, Inc. All rights reserved.
 */

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
