/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Corigine, Inc.
 * All rights reserved.
 */

#include <rte_common.h>
#include <rte_service_component.h>
#include <rte_malloc.h>
#include <ethdev_pci.h>
#include <ethdev_driver.h>

#include "../nfp_common.h"
#include "../nfp_logs.h"
#include "../nfp_ctrl.h"
#include "../nfp_cpp_bridge.h"
#include "nfp_flower.h"

int
nfp_init_app_fw_flower(struct nfp_pf_dev *pf_dev)
{
	unsigned int numa_node;
	struct nfp_app_fw_flower *app_fw_flower;

	numa_node = rte_socket_id();

	/* Allocate memory for the Flower app */
	app_fw_flower = rte_zmalloc_socket("nfp_app_fw_flower", sizeof(*app_fw_flower),
			RTE_CACHE_LINE_SIZE, numa_node);
	if (app_fw_flower == NULL) {
		PMD_INIT_LOG(ERR, "Could not malloc app fw flower");
		return -ENOMEM;
	}

	pf_dev->app_fw_priv = app_fw_flower;

	return 0;
}

int
nfp_secondary_init_app_fw_flower(__rte_unused struct nfp_cpp *cpp)
{
	PMD_INIT_LOG(ERR, "Flower firmware not supported");
	return -ENOTSUP;
}
