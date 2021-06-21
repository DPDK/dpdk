/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#include <rte_bus_pci.h>
#include <rte_pci.h>
#include <rte_rawdev.h>
#include <rte_rawdev_pmd.h>

#include <roc_api.h>
#include <roc_bphy_irq.h>

#include "cnxk_bphy_irq.h"

static struct bphy_device *
cnxk_bphy_get_bphy_dev_by_dev_id(uint16_t dev_id)
{
	struct rte_rawdev *rawdev;

	if (!rte_rawdev_pmd_is_valid_dev(dev_id))
		return NULL;

	rawdev = &rte_rawdevs[dev_id];

	return (struct bphy_device *)rawdev->dev_private;
}

int
cnxk_bphy_intr_init(uint16_t dev_id)
{
	struct bphy_device *bphy_dev = cnxk_bphy_get_bphy_dev_by_dev_id(dev_id);

	bphy_dev->irq_chip = roc_bphy_intr_init();
	if (bphy_dev->irq_chip == NULL)
		return -ENOMEM;

	return 0;
}

void
cnxk_bphy_intr_fini(uint16_t dev_id)
{
	struct bphy_device *bphy_dev = cnxk_bphy_get_bphy_dev_by_dev_id(dev_id);
	struct roc_bphy_irq_chip *irq_chip = bphy_dev->irq_chip;

	roc_bphy_intr_fini(irq_chip);
	bphy_dev->irq_chip = NULL;
}
