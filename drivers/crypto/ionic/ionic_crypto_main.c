/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021-2024 Advanced Micro Devices, Inc.
 */

#include <inttypes.h>

#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_bitops.h>

#include "ionic_crypto.h"

static int
iocpt_init(struct iocpt_dev *dev)
{
	dev->state |= IOCPT_DEV_F_INITED;

	return 0;
}

void
iocpt_configure(struct iocpt_dev *dev)
{
	RTE_SET_USED(dev);
}

void
iocpt_deinit(struct iocpt_dev *dev)
{
	IOCPT_PRINT_CALL();

	if (!(dev->state & IOCPT_DEV_F_INITED))
		return;

	dev->state &= ~IOCPT_DEV_F_INITED;
}

static int
iocpt_devargs(struct rte_devargs *devargs, struct iocpt_dev *dev)
{
	RTE_SET_USED(devargs);
	RTE_SET_USED(dev);

	return 0;
}

int
iocpt_probe(void *bus_dev, struct rte_device *rte_dev,
	struct iocpt_dev_bars *bars, const struct iocpt_dev_intf *intf,
	uint8_t driver_id, uint8_t socket_id)
{
	struct rte_cryptodev_pmd_init_params init_params = {
		"iocpt",
		sizeof(struct iocpt_dev),
		socket_id,
		RTE_CRYPTODEV_PMD_DEFAULT_MAX_NB_QUEUE_PAIRS
	};
	struct rte_cryptodev *cdev;
	struct iocpt_dev *dev;
	uint32_t i;
	int err;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		IOCPT_PRINT(ERR, "Multi-process not supported");
		err = -EPERM;
		goto err;
	}

	cdev = rte_cryptodev_pmd_create(rte_dev->name, rte_dev, &init_params);
	if (cdev == NULL) {
		IOCPT_PRINT(ERR, "Out of memory");
		err = -ENOMEM;
		goto err;
	}

	dev = cdev->data->dev_private;
	dev->crypto_dev = cdev;
	dev->bus_dev = bus_dev;
	dev->intf = intf;
	dev->driver_id = driver_id;
	dev->socket_id = socket_id;

	for (i = 0; i < bars->num_bars; i++) {
		struct ionic_dev_bar *bar = &bars->bar[i];

		IOCPT_PRINT(DEBUG,
			"bar[%u] = { .va = %p, .pa = %#jx, .len = %lu }",
			i, bar->vaddr, bar->bus_addr, bar->len);
		if (bar->vaddr == NULL) {
			IOCPT_PRINT(ERR, "Null bar found, aborting");
			err = -EFAULT;
			goto err_destroy_crypto_dev;
		}

		dev->bars.bar[i].vaddr = bar->vaddr;
		dev->bars.bar[i].bus_addr = bar->bus_addr;
		dev->bars.bar[i].len = bar->len;
	}
	dev->bars.num_bars = bars->num_bars;

	err = iocpt_devargs(rte_dev->devargs, dev);
	if (err != 0) {
		IOCPT_PRINT(ERR, "Cannot parse device arguments");
		goto err_destroy_crypto_dev;
	}

	err = iocpt_setup_bars(dev);
	if (err != 0) {
		IOCPT_PRINT(ERR, "Cannot setup BARs: %d, aborting", err);
		goto err_destroy_crypto_dev;
	}

	err = iocpt_init(dev);
	if (err != 0) {
		IOCPT_PRINT(ERR, "Cannot init device: %d, aborting", err);
		goto err_destroy_crypto_dev;
	}

	return 0;

err_destroy_crypto_dev:
	rte_cryptodev_pmd_destroy(cdev);
err:
	return err;
}

int
iocpt_remove(struct rte_device *rte_dev)
{
	struct rte_cryptodev *cdev;
	struct iocpt_dev *dev;

	cdev = rte_cryptodev_pmd_get_named_dev(rte_dev->name);
	if (cdev == NULL) {
		IOCPT_PRINT(DEBUG, "Cannot find device %s", rte_dev->name);
		return -ENODEV;
	}

	dev = cdev->data->dev_private;

	iocpt_deinit(dev);

	rte_cryptodev_pmd_destroy(cdev);

	return 0;
}

RTE_LOG_REGISTER_DEFAULT(iocpt_logtype, NOTICE);
