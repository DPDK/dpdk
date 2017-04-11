/*-
 *   BSD LICENSE
 *
 *   Copyright (c) 2016 Freescale Semiconductor, Inc. All rights reserved.
 *   Copyright (c) 2016 NXP. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Freescale Semiconductor, Inc nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <time.h>
#include <net/if.h>

#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_string_fns.h>
#include <rte_cycles.h>
#include <rte_kvargs.h>
#include <rte_dev.h>
#include <rte_ethdev.h>
#include <rte_fslmc.h>

#include <fslmc_vfio.h>
#include "dpaa2_ethdev.h"

static struct rte_dpaa2_driver rte_dpaa2_pmd;

static int
dpaa2_dev_init(struct rte_eth_dev *eth_dev)
{
	/* For secondary processes, the primary has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	eth_dev->data->drv_name = rte_dpaa2_pmd.driver.name;

	return 0;
}

static int
dpaa2_dev_uninit(struct rte_eth_dev *eth_dev __rte_unused)
{
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -EPERM;

	return 0;
}

static int
rte_dpaa2_probe(struct rte_dpaa2_driver *dpaa2_drv __rte_unused,
		struct rte_dpaa2_device *dpaa2_dev)
{
	struct rte_eth_dev *eth_dev;
	char ethdev_name[RTE_ETH_NAME_MAX_LEN];

	int diag;

	sprintf(ethdev_name, "dpni-%d", dpaa2_dev->object_id);

	eth_dev = rte_eth_dev_allocate(ethdev_name);
	if (eth_dev == NULL)
		return -ENOMEM;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		eth_dev->data->dev_private = rte_zmalloc(
						"ethdev private structure",
						sizeof(struct dpaa2_dev_priv),
						RTE_CACHE_LINE_SIZE);
		if (eth_dev->data->dev_private == NULL) {
			RTE_LOG(CRIT, PMD, "Cannot allocate memzone for"
				" private port data\n");
			rte_eth_dev_release_port(eth_dev);
			return -ENOMEM;
		}
	}
	eth_dev->device = &dpaa2_dev->device;
	dpaa2_dev->eth_dev = eth_dev;
	eth_dev->data->rx_mbuf_alloc_failed = 0;

	/* Invoke PMD device initialization function */
	diag = dpaa2_dev_init(eth_dev);
	if (diag == 0)
		return 0;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		rte_free(eth_dev->data->dev_private);
	rte_eth_dev_release_port(eth_dev);
	return diag;
}

static int
rte_dpaa2_remove(struct rte_dpaa2_device *dpaa2_dev)
{
	struct rte_eth_dev *eth_dev;

	eth_dev = dpaa2_dev->eth_dev;
	dpaa2_dev_uninit(eth_dev);

	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		rte_free(eth_dev->data->dev_private);
	rte_eth_dev_release_port(eth_dev);

	return 0;
}

static struct rte_dpaa2_driver rte_dpaa2_pmd = {
	.drv_type = DPAA2_MC_DPNI_DEVID,
	.probe = rte_dpaa2_probe,
	.remove = rte_dpaa2_remove,
};

RTE_PMD_REGISTER_DPAA2(net_dpaa2, rte_dpaa2_pmd);
