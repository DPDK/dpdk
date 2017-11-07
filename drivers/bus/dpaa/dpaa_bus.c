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

#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_interrupts.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_pci.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_memory.h>
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

#include <fsl_usd.h>
#include <fsl_qman.h>
#include <fsl_bman.h>
#include <of.h>
#include <netcfg.h>

int dpaa_logtype_bus;
int dpaa_logtype_mempool;
int dpaa_logtype_pmd;

struct rte_dpaa_bus rte_dpaa_bus;
struct netcfg_info *dpaa_netcfg;

/* define a variable to hold the portal_key, once created.*/
pthread_key_t dpaa_portal_key;

RTE_DEFINE_PER_LCORE(bool, _dpaa_io);

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

/*
 * Reads the SEC device from DTS
 * Returns -1 if SEC devices not available, 0 otherwise
 */
static inline int
dpaa_sec_available(void)
{
	const struct device_node *caam_node;

	for_each_compatible_node(caam_node, NULL, "fsl,sec-v4.0") {
		return 0;
	}

	return -1;
}

static void dpaa_clean_device_list(void);

static int
dpaa_create_device_list(void)
{
	int i;
	int ret;
	struct rte_dpaa_device *dev;
	struct fm_eth_port_cfg *cfg;
	struct fman_if *fman_intf;

	/* Creating Ethernet Devices */
	for (i = 0; i < dpaa_netcfg->num_ethports; i++) {
		dev = calloc(1, sizeof(struct rte_dpaa_device));
		if (!dev) {
			DPAA_BUS_LOG(ERR, "Failed to allocate ETH devices");
			ret = -ENOMEM;
			goto cleanup;
		}

		cfg = &dpaa_netcfg->port_cfg[i];
		fman_intf = cfg->fman_if;

		/* Device identifiers */
		dev->id.fman_id = fman_intf->fman_idx + 1;
		dev->id.mac_id = fman_intf->mac_idx;
		dev->device_type = FSL_DPAA_ETH;
		dev->id.dev_id = i;

		/* Create device name */
		memset(dev->name, 0, RTE_ETH_NAME_MAX_LEN);
		sprintf(dev->name, "fm%d-mac%d", (fman_intf->fman_idx + 1),
			fman_intf->mac_idx);
		DPAA_BUS_LOG(DEBUG, "Device added: %s", dev->name);
		dev->device.name = dev->name;

		dpaa_add_to_device_list(dev);
	}

	rte_dpaa_bus.device_count = i;

	/* Unlike case of ETH, RTE_LIBRTE_DPAA_MAX_CRYPTODEV SEC devices are
	 * constantly created only if "sec" property is found in the device
	 * tree. Logically there is no limit for number of devices (QI
	 * interfaces) that can be created.
	 */

	if (dpaa_sec_available()) {
		DPAA_BUS_LOG(INFO, "DPAA SEC devices are not available");
		return 0;
	}

	/* Creating SEC Devices */
	for (i = 0; i < RTE_LIBRTE_DPAA_MAX_CRYPTODEV; i++) {
		dev = calloc(1, sizeof(struct rte_dpaa_device));
		if (!dev) {
			DPAA_BUS_LOG(ERR, "Failed to allocate SEC devices");
			ret = -1;
			goto cleanup;
		}

		dev->device_type = FSL_DPAA_CRYPTO;
		dev->id.dev_id = rte_dpaa_bus.device_count + i;

		/* Even though RTE_CRYPTODEV_NAME_MAX_LEN is valid length of
		 * crypto PMD, using RTE_ETH_NAME_MAX_LEN as that is the size
		 * allocated for dev->name/
		 */
		memset(dev->name, 0, RTE_ETH_NAME_MAX_LEN);
		sprintf(dev->name, "dpaa-sec%d", i);
		DPAA_BUS_LOG(DEBUG, "Device added: %s", dev->name);

		dpaa_add_to_device_list(dev);
	}

	rte_dpaa_bus.device_count += i;

	return 0;

cleanup:
	dpaa_clean_device_list();
	return ret;
}

static void
dpaa_clean_device_list(void)
{
	struct rte_dpaa_device *dev = NULL;
	struct rte_dpaa_device *tdev = NULL;

	TAILQ_FOREACH_SAFE(dev, &rte_dpaa_bus.device_list, next, tdev) {
		TAILQ_REMOVE(&rte_dpaa_bus.device_list, dev, next);
		free(dev);
		dev = NULL;
	}
}

/** XXX move this function into a separate file */
static int
_dpaa_portal_init(void *arg)
{
	cpu_set_t cpuset;
	pthread_t id;
	uint32_t cpu = rte_lcore_id();
	int ret;
	struct dpaa_portal *dpaa_io_portal;

	BUS_INIT_FUNC_TRACE();

	if ((uint64_t)arg == 1 || cpu == LCORE_ID_ANY)
		cpu = rte_get_master_lcore();
	/* if the core id is not supported */
	else
		if (cpu >= RTE_MAX_LCORE)
			return -1;

	/* Set CPU affinity for this thread */
	CPU_ZERO(&cpuset);
	CPU_SET(cpu, &cpuset);
	id = pthread_self();
	ret = pthread_setaffinity_np(id, sizeof(cpu_set_t), &cpuset);
	if (ret) {
		DPAA_BUS_LOG(ERR, "pthread_setaffinity_np failed on "
			"core :%d with ret: %d", cpu, ret);
		return ret;
	}

	/* Initialise bman thread portals */
	ret = bman_thread_init();
	if (ret) {
		DPAA_BUS_LOG(ERR, "bman_thread_init failed on "
			"core %d with ret: %d", cpu, ret);
		return ret;
	}

	DPAA_BUS_LOG(DEBUG, "BMAN thread initialized");

	/* Initialise qman thread portals */
	ret = qman_thread_init();
	if (ret) {
		DPAA_BUS_LOG(ERR, "bman_thread_init failed on "
			"core %d with ret: %d", cpu, ret);
		bman_thread_finish();
		return ret;
	}

	DPAA_BUS_LOG(DEBUG, "QMAN thread initialized");

	dpaa_io_portal = rte_malloc(NULL, sizeof(struct dpaa_portal),
				    RTE_CACHE_LINE_SIZE);
	if (!dpaa_io_portal) {
		DPAA_BUS_LOG(ERR, "Unable to allocate memory");
		bman_thread_finish();
		qman_thread_finish();
		return -ENOMEM;
	}

	dpaa_io_portal->qman_idx = qman_get_portal_index();
	dpaa_io_portal->bman_idx = bman_get_portal_index();
	dpaa_io_portal->tid = syscall(SYS_gettid);

	ret = pthread_setspecific(dpaa_portal_key, (void *)dpaa_io_portal);
	if (ret) {
		DPAA_BUS_LOG(ERR, "pthread_setspecific failed on "
			    "core %d with ret: %d", cpu, ret);
		dpaa_portal_finish(NULL);

		return ret;
	}

	RTE_PER_LCORE(_dpaa_io) = true;

	DPAA_BUS_LOG(DEBUG, "QMAN thread initialized");

	return 0;
}

/*
 * rte_dpaa_portal_init - Wrapper over _dpaa_portal_init with thread level check
 * XXX Complete this
 */
int
rte_dpaa_portal_init(void *arg)
{
	if (unlikely(!RTE_PER_LCORE(_dpaa_io)))
		return _dpaa_portal_init(arg);

	return 0;
}

void
dpaa_portal_finish(void *arg)
{
	struct dpaa_portal *dpaa_io_portal = (struct dpaa_portal *)arg;

	if (!dpaa_io_portal) {
		DPAA_BUS_LOG(DEBUG, "Portal already cleaned");
		return;
	}

	bman_thread_finish();
	qman_thread_finish();

	pthread_setspecific(dpaa_portal_key, NULL);

	rte_free(dpaa_io_portal);
	dpaa_io_portal = NULL;

	RTE_PER_LCORE(_dpaa_io) = false;
}

#define DPAA_DEV_PATH1 "/sys/devices/platform/soc/soc:fsl,dpaa"
#define DPAA_DEV_PATH2 "/sys/devices/platform/fsl,dpaa"

static int
rte_dpaa_bus_scan(void)
{
	int ret;

	BUS_INIT_FUNC_TRACE();

	if ((access(DPAA_DEV_PATH1, F_OK) != 0) &&
	    (access(DPAA_DEV_PATH2, F_OK) != 0)) {
		RTE_LOG(DEBUG, EAL, "DPAA Bus not present. Skipping.\n");
		return 0;
	}

	/* Load the device-tree driver */
	ret = of_init();
	if (ret) {
		DPAA_BUS_LOG(ERR, "of_init failed with ret: %d", ret);
		return -1;
	}

	/* Get the interface configurations from device-tree */
	dpaa_netcfg = netcfg_acquire();
	if (!dpaa_netcfg) {
		DPAA_BUS_LOG(ERR, "netcfg_acquire failed");
		return -EINVAL;
	}

	RTE_LOG(NOTICE, EAL, "DPAA Bus Detected\n");

	if (!dpaa_netcfg->num_ethports) {
		DPAA_BUS_LOG(INFO, "no network interfaces available");
		/* This is not an error */
		return 0;
	}

	DPAA_BUS_LOG(DEBUG, "Bus: Address of netcfg=%p, Ethports=%d",
		     dpaa_netcfg, dpaa_netcfg->num_ethports);

#ifdef RTE_LIBRTE_DPAA_DEBUG_DRIVER
	dump_netcfg(dpaa_netcfg);
#endif

	DPAA_BUS_LOG(DEBUG, "Number of devices = %d\n",
		     dpaa_netcfg->num_ethports);
	ret = dpaa_create_device_list();
	if (ret) {
		DPAA_BUS_LOG(ERR, "Unable to create device list. (%d)", ret);
		return ret;
	}

	/* create the key, supplying a function that'll be invoked
	 * when a portal affined thread will be deleted.
	 */
	ret = pthread_key_create(&dpaa_portal_key, dpaa_portal_finish);
	if (ret) {
		DPAA_BUS_LOG(DEBUG, "Unable to create pthread key. (%d)", ret);
		dpaa_clean_device_list();
		return ret;
	}

	DPAA_BUS_LOG(DEBUG, "dpaa_portal_key=%u, ret=%d\n",
		    (unsigned int)dpaa_portal_key, ret);

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

/*
 * Get iommu class of DPAA2 devices on the bus.
 */
static enum rte_iova_mode
rte_dpaa_get_iommu_class(void)
{
	return RTE_IOVA_PA;
}

struct rte_dpaa_bus rte_dpaa_bus = {
	.bus = {
		.scan = rte_dpaa_bus_scan,
		.probe = rte_dpaa_bus_probe,
		.find_device = rte_dpaa_find_device,
		.get_iommu_class = rte_dpaa_get_iommu_class,
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

	dpaa_logtype_mempool = rte_log_register("mempool.dpaa");
	if (dpaa_logtype_mempool >= 0)
		rte_log_set_level(dpaa_logtype_mempool, RTE_LOG_NOTICE);

	dpaa_logtype_pmd = rte_log_register("pmd.dpaa");
	if (dpaa_logtype_pmd >= 0)
		rte_log_set_level(dpaa_logtype_pmd, RTE_LOG_NOTICE);
}
