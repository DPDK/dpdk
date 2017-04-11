/*-
 *   BSD LICENSE
 *
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
 *     * Neither the name of NXP nor the names of its
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

#include <string.h>
#include <dirent.h>

#include <rte_log.h>
#include <rte_bus.h>
#include <rte_eal_memconfig.h>
#include <rte_malloc.h>
#include <rte_devargs.h>
#include <rte_memcpy.h>
#include <rte_ethdev.h>

#include "rte_fslmc.h"
#include "fslmc_vfio.h"

#define FSLMC_BUS_LOG(level, fmt, args...) \
	RTE_LOG(level, EAL, "%s(): " fmt "\n", __func__, ##args)

struct rte_fslmc_bus rte_fslmc_bus;

static int
rte_fslmc_scan(void)
{
	int ret;

	ret = fslmc_vfio_setup_group();
	if (ret) {
		FSLMC_BUS_LOG(ERR, "fslmc: Unable to setup VFIO");
		return ret;
	}

	ret = fslmc_vfio_process_group();
	if (ret) {
		FSLMC_BUS_LOG(ERR, "fslmc: Unable to setup devices");
		return -1;
	}

	RTE_LOG(INFO, EAL, "fslmc: Bus scan completed\n");
	return 0;
}

static int
rte_fslmc_match(struct rte_dpaa2_driver *dpaa2_drv,
		struct rte_dpaa2_device *dpaa2_dev)
{
	if (dpaa2_drv->drv_type == dpaa2_dev->dev_type)
		return 0;

	return 1;
}

static int
rte_fslmc_probe(void)
{
	int ret = 0;
	struct rte_dpaa2_device *dev;
	struct rte_dpaa2_driver *drv;

	TAILQ_FOREACH(dev, &rte_fslmc_bus.device_list, next) {
		TAILQ_FOREACH(drv, &rte_fslmc_bus.driver_list, next) {
			ret = rte_fslmc_match(drv, dev);
			if (ret)
				continue;

			if (!drv->probe)
				continue;

			ret = drv->probe(drv, dev);
			if (ret)
				FSLMC_BUS_LOG(ERR, "Unable to probe.\n");
			break;
		}
	}
	return ret;
}

/*register a fslmc bus based dpaa2 driver */
void
rte_fslmc_driver_register(struct rte_dpaa2_driver *driver)
{
	RTE_VERIFY(driver);

	TAILQ_INSERT_TAIL(&rte_fslmc_bus.driver_list, driver, next);
	/* Update Bus references */
	driver->fslmc_bus = &rte_fslmc_bus;
}

/*un-register a fslmc bus based dpaa2 driver */
void
rte_fslmc_driver_unregister(struct rte_dpaa2_driver *driver)
{
	struct rte_fslmc_bus *fslmc_bus;

	fslmc_bus = driver->fslmc_bus;

	TAILQ_REMOVE(&fslmc_bus->driver_list, driver, next);
	/* Update Bus references */
	driver->fslmc_bus = NULL;
}

struct rte_fslmc_bus rte_fslmc_bus = {
	.bus = {
		.scan = rte_fslmc_scan,
		.probe = rte_fslmc_probe,
	},
	.device_list = TAILQ_HEAD_INITIALIZER(rte_fslmc_bus.device_list),
	.driver_list = TAILQ_HEAD_INITIALIZER(rte_fslmc_bus.driver_list),
};

RTE_REGISTER_BUS(FSLMC_BUS_NAME, rte_fslmc_bus.bus);
