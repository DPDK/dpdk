/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium, Inc
 */

#include <rte_cryptodev.h>

#include "otx_cryptodev.h"
#include "otx_cryptodev_ops.h"

int
otx_cpt_dev_create(struct rte_cryptodev *c_dev)
{
	RTE_SET_USED(c_dev);
	return 0;
}
