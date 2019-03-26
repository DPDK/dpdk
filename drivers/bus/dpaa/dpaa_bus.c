/* SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright 2017 NXP
 *
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
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_memory.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_alarm.h>
#include <rte_ether.h>
#include <rte_ethdev_driver.h>
#include <rte_malloc.h>
#include <rte_ring.h>
#include <rte_bus.h>
#include <rte_mbuf_pool_ops.h>

#include <rte_dpaa_bus.h>
#include <rte_dpaa_logs.h>
#include <dpaax_iova_table.h>

#include <fsl_usd.h>
#include <fsl_qman.h>
#include <fsl_bman.h>
#include <of.h>
#include <netcfg.h>

int dpaa_logtype_bus;
int dpaa_logtype_mempool;
int dpaa_logtype_pmd;
int dpaa_logtype_eventdev;

static struct rte_dpaa_bus rte_dpaa_bus;
struct netcfg_info *dpaa_netcfg;

/* define a variable to hold the portal_key, once created.*/
static pthread_key_t dpaa_portal_key;

unsigned int dpaa_svr_family;

#define FSL_DPAA_BUS_NAME	dpaa_bus

RTE_DEFINE_PER_LCORE(bool, dpaa_io);
RTE_DEFINE_PER_LCORE(struct dpaa_portal_dqrr, held_bufs);

static int
compare_dpaa_devices(struct rte_dpaa_device *dev1,
		     struct rte_dpaa_device *dev2)
{
	int comp = 0;

	/* Segragating ETH from SEC devices */
	if (dev1->device_type > dev2->device_type)
		comp = 1;
	else if (dev1->device_type < dev2->device_type)
		comp = -1;
	else
		comp = 0;

	if ((comp != 0) || (dev1->device_type != FSL_DPAA_ETH))
		return comp;

	if (dev1->id.fman_id > dev2->id.fman_id) {
		comp = 1;
	} else if (dev1->id.fman_id < dev2->id.fman_id) {
		comp = -1;
	} else {
		/* FMAN ids match, check for mac_id */
		if (dev1->id.mac_id > dev2->id.mac_id)
			comp = 1;
		else if (dev1->id.mac_id < dev2->id.mac_id)
			comp = -1;
		else
			comp = 0;
	}

	return comp;
}

static inline void
dpaa_add_to_device_list(struct rte_dpaa_device *newdev)
{
	int comp, inserted = 0;
	struct rte_dpaa_device *dev = NULL;
	struct rte_dpaa_device *tdev = NULL;

	TAILQ_FOREACH_SAFE(dev, &rte_dpaa_bus.device_list, next, tdev) {
		comp = compare_dpaa_devices(newdev, dev);
		if (comp < 0) {
			TAILQ_INSERT_BEFORE(dev, newdev, next);
			inserted = 1;
			break;
		}
	}

	if (!inserted)
		TAILQ_INSERT_TAIL(&rte_dpaa_bus.device_list, newdev, next);
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

static struct rte_devargs *
dpaa_devargs_lookup(struct rte_dpaa_device *dev)
{
	struct rte_devargs *devargs;
	char dev_name[32];

	RTE_EAL_DEVARGS_FOREACH("dpaa_bus", devargs) {
		devargs->bus->parse(devargs->name, &dev_name);
		if (strcmp(dev_name, dev->device.name) == 0) {
			DPAA_BUS_INFO("**Devargs matched %s", dev_name);
			return devargs;
		}
	}
	return NULL;
}

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

		dev->device.bus = &rte_dpaa_bus.bus;

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
		DPAA_BUS_LOG(INFO, "%s netdev added", dev->name);
		dev->device.name = dev->name;
		dev->device.devargs = dpaa_devargs_lookup(dev);

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
		DPAA_BUS_LOG(INFO, "%s cryptodev added", dev->name);
		dev->device.name = dev->name;
		dev->device.devargs = dpaa_devargs_lookup(dev);

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

int rte_dpaa_portal_init(void *arg)
{
	pthread_t id;
	unsigned int cpu, lcore = rte_lcore_id();
	int ret;
	struct dpaa_portal *dpaa_io_portal;

	BUS_INIT_FUNC_TRACE();

	if ((size_t)arg == 1 || lcore == LCORE_ID_ANY)
		lcore = rte_get_master_lcore();
	else
		if (lcore >= RTE_MAX_LCORE)
			return -1;

	cpu = lcore_config[lcore].core_id;

	/* Set CPU affinity for this thread.*/
	id = pthread_self();
	ret = pthread_setaffinity_np(id, sizeof(cpu_set_t),
			&lcore_config[lcore].cpuset);
	if (ret) {
		DPAA_BUS_LOG(ERR, "pthread_setaffinity_np failed on core :%u"
			     " (lcore=%u) with ret: %d", cpu, lcore, ret);
		return ret;
	}

	/* Initialise bman thread portals */
	ret = bman_thread_init();
	if (ret) {
		DPAA_BUS_LOG(ERR, "bman_thread_init failed on core %u"
			     " (lcore=%u) with ret: %d", cpu, lcore, ret);
		return ret;
	}

	DPAA_BUS_LOG(DEBUG, "BMAN thread initialized - CPU=%d lcore=%d",
		     cpu, lcore);

	/* Initialise qman thread portals */
	ret = qman_thread_init();
	if (ret) {
		DPAA_BUS_LOG(ERR, "qman_thread_init failed on core %u"
			    " (lcore=%u) with ret: %d", cpu, lcore, ret);
		bman_thread_finish();
		return ret;
	}

	DPAA_BUS_LOG(DEBUG, "QMAN thread initialized - CPU=%d lcore=%d",
		     cpu, lcore);

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
		DPAA_BUS_LOG(ERR, "pthread_setspecific failed on core %u"
			     " (lcore=%u) with ret: %d", cpu, lcore, ret);
		dpaa_portal_finish(NULL);

		return ret;
	}

	RTE_PER_LCORE(dpaa_io) = true;

	DPAA_BUS_LOG(DEBUG, "QMAN thread initialized");

	return 0;
}

int
rte_dpaa_portal_fq_init(void *arg, struct qman_fq *fq)
{
	/* Affine above created portal with channel*/
	u32 sdqcr;
	struct qman_portal *qp;
	int ret;

	if (unlikely(!RTE_PER_LCORE(dpaa_io))) {
		ret = rte_dpaa_portal_init(arg);
		if (ret < 0) {
			DPAA_BUS_LOG(ERR, "portal initialization failure");
			return ret;
		}
	}

	/* Initialise qman specific portals */
	qp = fsl_qman_portal_create();
	if (!qp) {
		DPAA_BUS_LOG(ERR, "Unable to alloc fq portal");
		return -1;
	}
	fq->qp = qp;
	sdqcr = QM_SDQCR_CHANNELS_POOL_CONV(fq->ch_id);
	qman_static_dequeue_add(sdqcr, qp);

	return 0;
}

int rte_dpaa_portal_fq_close(struct qman_fq *fq)
{
	return fsl_qman_portal_destroy(fq->qp);
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

	RTE_PER_LCORE(dpaa_io) = false;
}

static int
rte_dpaa_bus_parse(const char *name, void *out_name)
{
	int i, j;
	int max_fman = 2, max_macs = 16;
	char *sep = strchr(name, ':');

	if (strncmp(name, RTE_STR(FSL_DPAA_BUS_NAME),
		strlen(RTE_STR(FSL_DPAA_BUS_NAME)))) {
		return -EINVAL;
	}

	if (!sep) {
		DPAA_BUS_ERR("Incorrect device name observed");
		return -EINVAL;
	}

	sep = (char *) (sep + 1);

	for (i = 0; i < max_fman; i++) {
		for (j = 0; j < max_macs; j++) {
			char fm_name[16];
			snprintf(fm_name, 16, "fm%d-mac%d", i, j);
			if (strcmp(fm_name, sep) == 0) {
				if (out_name)
					strcpy(out_name, sep);
				return 0;
			}
		}
	}

	for (i = 0; i < RTE_LIBRTE_DPAA_MAX_CRYPTODEV; i++) {
		char sec_name[16];

		snprintf(sec_name, 16, "dpaa-sec%d", i);
		if (strcmp(sec_name, sep) == 0) {
			if (out_name)
				strcpy(out_name, sep);
			return 0;
		}
	}

	return -EINVAL;
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
	/* detected DPAA devices */
	rte_dpaa_bus.detected = 1;

	/* create the key, supplying a function that'll be invoked
	 * when a portal affined thread will be deleted.
	 */
	ret = pthread_key_create(&dpaa_portal_key, dpaa_portal_finish);
	if (ret) {
		DPAA_BUS_LOG(DEBUG, "Unable to create pthread key. (%d)", ret);
		dpaa_clean_device_list();
		return ret;
	}

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
	if (!drv || !dev) {
		DPAA_BUS_DEBUG("Invalid drv or dev received.");
		return -1;
	}

	if (drv->drv_type == dev->device_type)
		return 0;

	return -1;
}

static int
rte_dpaa_bus_dev_build(void)
{
	int ret;

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

#ifdef RTE_LIBRTE_DPAA_DEBUG_DRIVER
	dump_netcfg(dpaa_netcfg);
#endif

	DPAA_BUS_LOG(DEBUG, "Number of ethernet devices = %d",
		     dpaa_netcfg->num_ethports);
	ret = dpaa_create_device_list();
	if (ret) {
		DPAA_BUS_LOG(ERR, "Unable to create device list. (%d)", ret);
		return ret;
	}
	return 0;
}

static int
rte_dpaa_bus_probe(void)
{
	int ret = -1;
	struct rte_dpaa_device *dev;
	struct rte_dpaa_driver *drv;
	FILE *svr_file = NULL;
	unsigned int svr_ver;
	int probe_all = rte_dpaa_bus.bus.conf.scan_mode != RTE_BUS_SCAN_WHITELIST;

	/* If DPAA bus is not present nothing needs to be done */
	if (!rte_dpaa_bus.detected)
		return 0;

	rte_dpaa_bus_dev_build();

	/* If no device present on DPAA bus nothing needs to be done */
	if (TAILQ_EMPTY(&rte_dpaa_bus.device_list))
		return 0;

	svr_file = fopen(DPAA_SOC_ID_FILE, "r");
	if (svr_file) {
		if (fscanf(svr_file, "svr:%x", &svr_ver) > 0)
			dpaa_svr_family = svr_ver & SVR_MASK;
		fclose(svr_file);
	}

	/* And initialize the PA->VA translation table */
	dpaax_iova_table_populate();

	/* For each registered driver, and device, call the driver->probe */
	TAILQ_FOREACH(dev, &rte_dpaa_bus.device_list, next) {
		TAILQ_FOREACH(drv, &rte_dpaa_bus.driver_list, next) {
			ret = rte_dpaa_device_match(drv, dev);
			if (ret)
				continue;

			if (rte_dev_is_probed(&dev->device))
				continue;

			if (!drv->probe ||
			    (dev->device.devargs &&
			    dev->device.devargs->policy == RTE_DEV_BLACKLISTED))
				continue;

			if (probe_all ||
			    (dev->device.devargs &&
			    dev->device.devargs->policy ==
			    RTE_DEV_WHITELISTED)) {
				ret = drv->probe(drv, dev);
				if (ret) {
					DPAA_BUS_ERR("Unable to probe.\n");
				} else {
					dev->driver = drv;
					dev->device.driver = &drv->driver;
				}
			}
			break;
		}
	}

	/* Register DPAA mempool ops only if any DPAA device has
	 * been detected.
	 */
	rte_mbuf_set_platform_mempool_ops(DPAA_MEMPOOL_OPS_NAME);

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
	if ((access(DPAA_DEV_PATH1, F_OK) != 0) &&
	    (access(DPAA_DEV_PATH2, F_OK) != 0)) {
		return RTE_IOVA_DC;
	}
	return RTE_IOVA_PA;
}

static struct rte_dpaa_bus rte_dpaa_bus = {
	.bus = {
		.scan = rte_dpaa_bus_scan,
		.probe = rte_dpaa_bus_probe,
		.parse = rte_dpaa_bus_parse,
		.find_device = rte_dpaa_find_device,
		.get_iommu_class = rte_dpaa_get_iommu_class,
	},
	.device_list = TAILQ_HEAD_INITIALIZER(rte_dpaa_bus.device_list),
	.driver_list = TAILQ_HEAD_INITIALIZER(rte_dpaa_bus.driver_list),
	.device_count = 0,
};

RTE_REGISTER_BUS(FSL_DPAA_BUS_NAME, rte_dpaa_bus.bus);

RTE_INIT(dpaa_init_log)
{
	dpaa_logtype_bus = rte_log_register("bus.dpaa");
	if (dpaa_logtype_bus >= 0)
		rte_log_set_level(dpaa_logtype_bus, RTE_LOG_NOTICE);

	dpaa_logtype_mempool = rte_log_register("mempool.dpaa");
	if (dpaa_logtype_mempool >= 0)
		rte_log_set_level(dpaa_logtype_mempool, RTE_LOG_NOTICE);

	dpaa_logtype_pmd = rte_log_register("pmd.net.dpaa");
	if (dpaa_logtype_pmd >= 0)
		rte_log_set_level(dpaa_logtype_pmd, RTE_LOG_NOTICE);

	dpaa_logtype_eventdev = rte_log_register("pmd.event.dpaa");
	if (dpaa_logtype_eventdev >= 0)
		rte_log_set_level(dpaa_logtype_eventdev, RTE_LOG_NOTICE);
}
