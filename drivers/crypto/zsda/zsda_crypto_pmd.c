/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 ZTE Corporation
 */

#include <rte_cryptodev.h>

#include "zsda_crypto_pmd.h"

uint8_t zsda_crypto_driver_id;

static struct rte_cryptodev_ops crypto_zsda_ops = {
	.dev_configure = NULL,
	.dev_start = NULL,
	.dev_stop = NULL,
	.dev_close = NULL,
	.dev_infos_get = NULL,

	.stats_get = NULL,
	.stats_reset = NULL,
	.queue_pair_setup = NULL,
	.queue_pair_release = NULL,

	.sym_session_get_size = NULL,
	.sym_session_configure = NULL,
	.sym_session_clear = NULL,
};

static const char zsda_crypto_drv_name[] = RTE_STR(CRYPTODEV_NAME_ZSDA_PMD);
static const struct rte_driver cryptodev_zsda_crypto_driver = {
	.name = zsda_crypto_drv_name,
	.alias = zsda_crypto_drv_name
};

int
zsda_crypto_dev_create(struct zsda_pci_device *zsda_pci_dev)
{
	struct zsda_device_info *dev_info =
		&zsda_devs[zsda_pci_dev->zsda_dev_id];

	struct rte_cryptodev_pmd_init_params init_params = {
		.name = "",
		.socket_id = (int)rte_socket_id(),
		.private_data_size = sizeof(struct zsda_crypto_dev_private)
	};

	char name[RTE_CRYPTODEV_NAME_MAX_LEN];
	struct rte_cryptodev *cryptodev;
	struct zsda_crypto_dev_private *crypto_dev_priv;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return ZSDA_SUCCESS;

	snprintf(name, RTE_CRYPTODEV_NAME_MAX_LEN, "%s_%s", zsda_pci_dev->name,
		 "crypto");
	ZSDA_LOG(DEBUG, "Creating ZSDA crypto device %s", name);

	dev_info->crypto_rte_dev.driver = &cryptodev_zsda_crypto_driver;
	dev_info->crypto_rte_dev.numa_node = dev_info->pci_dev->device.numa_node;

	cryptodev = rte_cryptodev_pmd_create(name, &(dev_info->crypto_rte_dev),
					     &init_params);

	if (cryptodev == NULL) {
		ZSDA_LOG(ERR, "Failed! rte_cryptodev_pmd_create");
		goto error;
	}

	dev_info->crypto_rte_dev.name = cryptodev->data->name;
	cryptodev->driver_id = zsda_crypto_driver_id;

	cryptodev->dev_ops = &crypto_zsda_ops;

	cryptodev->enqueue_burst = NULL;
	cryptodev->dequeue_burst = NULL;
	cryptodev->feature_flags = 0;

	crypto_dev_priv = cryptodev->data->dev_private;
	crypto_dev_priv->zsda_pci_dev = zsda_pci_dev;
	crypto_dev_priv->cryptodev = cryptodev;

	zsda_pci_dev->crypto_dev_priv = crypto_dev_priv;

	return ZSDA_SUCCESS;

error:

	rte_cryptodev_pmd_destroy(cryptodev);
	memset(&dev_info->crypto_rte_dev, 0, sizeof(dev_info->crypto_rte_dev));

	return -EFAULT;
}

void
zsda_crypto_dev_destroy(struct zsda_pci_device *zsda_pci_dev)
{
	struct zsda_crypto_dev_private *crypto_dev_priv;

	if (zsda_pci_dev == NULL)
		return;

	crypto_dev_priv = zsda_pci_dev->crypto_dev_priv;
	if (crypto_dev_priv == NULL)
		return;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		rte_memzone_free(crypto_dev_priv->capa_mz);

	rte_cryptodev_pmd_destroy(crypto_dev_priv->cryptodev);
	zsda_devs[zsda_pci_dev->zsda_dev_id].crypto_rte_dev.name = NULL;
	zsda_pci_dev->crypto_dev_priv = NULL;
}

static struct cryptodev_driver zsda_crypto_drv;
RTE_PMD_REGISTER_CRYPTO_DRIVER(zsda_crypto_drv, cryptodev_zsda_crypto_driver,
			       zsda_crypto_driver_id);
