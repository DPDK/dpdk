/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "cnxk_ethdev.h"
#include <rte_mtr_driver.h>

const struct rte_mtr_ops nix_mtr_ops = {
};

int
cnxk_nix_mtr_ops_get(struct rte_eth_dev *dev, void *ops)
{
	RTE_SET_USED(dev);

	*(const void **)ops = &nix_mtr_ops;
	return 0;
}
