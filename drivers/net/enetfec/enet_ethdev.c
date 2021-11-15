/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020-2021 NXP
 */

#include <ethdev_vdev.h>
#include <ethdev_driver.h>
#include "enet_pmd_logs.h"
#include "enet_ethdev.h"

#define ENETFEC_NAME_PMD                net_enetfec

static int
enetfec_eth_init(struct rte_eth_dev *dev)
{
	rte_eth_dev_probing_finish(dev);
	return 0;
}

static int
pmd_enetfec_probe(struct rte_vdev_device *vdev)
{
	struct rte_eth_dev *dev = NULL;
	struct enetfec_private *fep;
	const char *name;
	int rc;

	name = rte_vdev_device_name(vdev);
	ENETFEC_PMD_LOG(INFO, "Initializing pmd_fec for %s", name);

	dev = rte_eth_vdev_allocate(vdev, sizeof(*fep));
	if (dev == NULL)
		return -ENOMEM;

	/* setup board info structure */
	fep = dev->data->dev_private;
	fep->dev = dev;
	rc = enetfec_eth_init(dev);
	if (rc)
		goto failed_init;

	return 0;

failed_init:
	ENETFEC_PMD_ERR("Failed to init");
	return rc;
}

static int
pmd_enetfec_remove(struct rte_vdev_device *vdev)
{
	struct rte_eth_dev *eth_dev = NULL;
	int ret;

	/* find the ethdev entry */
	eth_dev = rte_eth_dev_allocated(rte_vdev_device_name(vdev));
	if (eth_dev == NULL)
		return -ENODEV;

	ret = rte_eth_dev_release_port(eth_dev);
	if (ret != 0)
		return -EINVAL;

	ENETFEC_PMD_INFO("Release enetfec sw device");
	return 0;
}

static struct rte_vdev_driver pmd_enetfec_drv = {
	.probe = pmd_enetfec_probe,
	.remove = pmd_enetfec_remove,
};

RTE_PMD_REGISTER_VDEV(ENETFEC_NAME_PMD, pmd_enetfec_drv);
RTE_LOG_REGISTER_DEFAULT(enetfec_logtype_pmd, NOTICE);
