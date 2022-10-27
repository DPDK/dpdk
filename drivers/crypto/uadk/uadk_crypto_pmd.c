/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2022-2023 Huawei Technologies Co.,Ltd. All rights reserved.
 * Copyright 2022-2023 Linaro ltd.
 */

#include <stdlib.h>

#include <bus_vdev_driver.h>
#include <cryptodev_pmd.h>
#include <rte_bus_vdev.h>

#include <uadk/wd_cipher.h>
#include <uadk/wd_digest.h>
#include <uadk/wd_sched.h>

#include "uadk_crypto_pmd_private.h"

static uint8_t uadk_cryptodev_driver_id;

static struct rte_cryptodev_ops uadk_crypto_pmd_ops = {
		.dev_configure		= NULL,
		.dev_start		= NULL,
		.dev_stop		= NULL,
		.dev_close		= NULL,
		.stats_get		= NULL,
		.stats_reset		= NULL,
		.dev_infos_get		= NULL,
		.queue_pair_setup	= NULL,
		.queue_pair_release	= NULL,
		.sym_session_get_size	= NULL,
		.sym_session_configure	= NULL,
		.sym_session_clear	= NULL,
};

static int
uadk_cryptodev_probe(struct rte_vdev_device *vdev)
{
	struct rte_cryptodev_pmd_init_params init_params = {
		.name = "",
		.private_data_size = sizeof(struct uadk_crypto_priv),
		.max_nb_queue_pairs =
				RTE_CRYPTODEV_PMD_DEFAULT_MAX_NB_QUEUE_PAIRS,
	};
	enum uadk_crypto_version version = UADK_CRYPTO_V2;
	struct uadk_crypto_priv *priv;
	struct rte_cryptodev *dev;
	struct uacce_dev *udev;
	const char *name;

	udev = wd_get_accel_dev("cipher");
	if (!udev)
		return -ENODEV;

	if (!strcmp(udev->api, "hisi_qm_v2"))
		version = UADK_CRYPTO_V2;

	free(udev);

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	dev = rte_cryptodev_pmd_create(name, &vdev->device, &init_params);
	if (dev == NULL) {
		UADK_LOG(ERR, "driver %s: create failed", init_params.name);
		return -ENODEV;
	}

	dev->dev_ops = &uadk_crypto_pmd_ops;
	dev->driver_id = uadk_cryptodev_driver_id;
	dev->dequeue_burst = NULL;
	dev->enqueue_burst = NULL;
	dev->feature_flags = RTE_CRYPTODEV_FF_HW_ACCELERATED;
	priv = dev->data->dev_private;
	priv->version = version;

	rte_cryptodev_pmd_probing_finish(dev);

	return 0;
}

static int
uadk_cryptodev_remove(struct rte_vdev_device *vdev)
{
	struct rte_cryptodev *cryptodev;
	const char *name;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	cryptodev = rte_cryptodev_pmd_get_named_dev(name);
	if (cryptodev == NULL)
		return -ENODEV;

	return rte_cryptodev_pmd_destroy(cryptodev);
}

static struct rte_vdev_driver uadk_crypto_pmd = {
	.probe       = uadk_cryptodev_probe,
	.remove      = uadk_cryptodev_remove,
};

static struct cryptodev_driver uadk_crypto_drv;

#define UADK_CRYPTO_DRIVER_NAME crypto_uadk
RTE_PMD_REGISTER_VDEV(UADK_CRYPTO_DRIVER_NAME, uadk_crypto_pmd);
RTE_PMD_REGISTER_CRYPTO_DRIVER(uadk_crypto_drv, uadk_crypto_pmd.driver,
			       uadk_cryptodev_driver_id);
RTE_LOG_REGISTER_DEFAULT(uadk_crypto_logtype, INFO);
