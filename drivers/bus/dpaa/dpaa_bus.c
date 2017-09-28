/*-
 *   BSD LICENSE
 *
 *   Copyright 2017 NXP.
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
/* System headers */
#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>
#include <limits.h>
#include <sched.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/syscall.h>

#include <rte_config.h>
#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_interrupts.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_pci.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_alarm.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_ring.h>
#include <rte_bus.h>

#include <rte_dpaa_bus.h>
#include <rte_dpaa_logs.h>

int dpaa_logtype_bus;

struct rte_dpaa_bus rte_dpaa_bus;

static inline void
dpaa_add_to_device_list(struct rte_dpaa_device *dev)
{
	TAILQ_INSERT_TAIL(&rte_dpaa_bus.device_list, dev, next);
}

static inline void
dpaa_remove_from_device_list(struct rte_dpaa_device *dev)
{
	TAILQ_INSERT_TAIL(&rte_dpaa_bus.device_list, dev, next);
}

static int
rte_dpaa_bus_scan(void)
{
	BUS_INIT_FUNC_TRACE();

	return 0;
}

/* register a dpaa bus based dpaa driver */
void
rte_dpaa_driver_register(struct rte_dpaa_driver *driver)
{
	RTE_VERIFY(driver);

	BUS_INIT_FUNC_TRACE();

	TAILQ_INSERT_TAIL(&rte_dpaa_bus.driver_list, driver, next);
	/* Update Bus references */
	driver->dpaa_bus = &rte_dpaa_bus;
}

/* un-register a dpaa bus based dpaa driver */
void
rte_dpaa_driver_unregister(struct rte_dpaa_driver *driver)
{
	struct rte_dpaa_bus *dpaa_bus;

	BUS_INIT_FUNC_TRACE();

	dpaa_bus = driver->dpaa_bus;

	TAILQ_REMOVE(&dpaa_bus->driver_list, driver, next);
	/* Update Bus references */
	driver->dpaa_bus = NULL;
}

static int
rte_dpaa_device_match(struct rte_dpaa_driver *drv,
		      struct rte_dpaa_device *dev)
{
	int ret = -1;

	BUS_INIT_FUNC_TRACE();

	if (!drv || !dev) {
		DPAA_BUS_DEBUG("Invalid drv or dev received.");
		return ret;
	}

	if (drv->drv_type == dev->device_type) {
		DPAA_BUS_INFO("Device: %s matches for driver: %s",
			      dev->name, drv->driver.name);
		ret = 0; /* Found a match */
	}

	return ret;
}

static int
rte_dpaa_bus_probe(void)
{
	int ret = -1;
	struct rte_dpaa_device *dev;
	struct rte_dpaa_driver *drv;

	BUS_INIT_FUNC_TRACE();

	/* For each registered driver, and device, call the driver->probe */
	TAILQ_FOREACH(dev, &rte_dpaa_bus.device_list, next) {
		TAILQ_FOREACH(drv, &rte_dpaa_bus.driver_list, next) {
			ret = rte_dpaa_device_match(drv, dev);
			if (ret)
				continue;

			if (!drv->probe)
				continue;

			ret = drv->probe(drv, dev);
			if (ret)
				DPAA_BUS_ERR("Unable to probe.\n");
			break;
		}
	}
	return 0;
}

static struct rte_device *
rte_dpaa_find_device(const struct rte_device *start, rte_dev_cmp_t cmp,
		     const void *data)
{
	struct rte_dpaa_device *dev;

	TAILQ_FOREACH(dev, &rte_dpaa_bus.device_list, next) {
		if (start && &dev->device == start) {
			start = NULL;  /* starting point found */
			continue;
		}

		if (cmp(&dev->device, data) == 0)
			return &dev->device;
	}

	return NULL;
}

struct rte_dpaa_bus rte_dpaa_bus = {
	.bus = {
		.scan = rte_dpaa_bus_scan,
		.probe = rte_dpaa_bus_probe,
		.find_device = rte_dpaa_find_device,
	},
	.device_list = TAILQ_HEAD_INITIALIZER(rte_dpaa_bus.device_list),
	.driver_list = TAILQ_HEAD_INITIALIZER(rte_dpaa_bus.driver_list),
	.device_count = 0,
};

RTE_REGISTER_BUS(FSL_DPAA_BUS_NAME, rte_dpaa_bus.bus);

RTE_INIT(dpaa_init_log);
static void
dpaa_init_log(void)
{
	dpaa_logtype_bus = rte_log_register("bus.dpaa");
	if (dpaa_logtype_bus >= 0)
		rte_log_set_level(dpaa_logtype_bus, RTE_LOG_NOTICE);
}
