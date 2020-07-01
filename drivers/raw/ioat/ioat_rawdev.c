/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include <rte_cycles.h>
#include <rte_bus_pci.h>
#include <rte_string_fns.h>
#include <rte_rawdev_pmd.h>

#include "rte_ioat_rawdev.h"

static struct rte_pci_driver ioat_pmd_drv;

#define IOAT_VENDOR_ID		0x8086
#define IOAT_DEVICE_ID_SKX	0x2021
#define IOAT_DEVICE_ID_BDX0	0x6f20
#define IOAT_DEVICE_ID_BDX1	0x6f21
#define IOAT_DEVICE_ID_BDX2	0x6f22
#define IOAT_DEVICE_ID_BDX3	0x6f23
#define IOAT_DEVICE_ID_BDX4	0x6f24
#define IOAT_DEVICE_ID_BDX5	0x6f25
#define IOAT_DEVICE_ID_BDX6	0x6f26
#define IOAT_DEVICE_ID_BDX7	0x6f27
#define IOAT_DEVICE_ID_BDXE	0x6f2E
#define IOAT_DEVICE_ID_BDXF	0x6f2F
#define IOAT_DEVICE_ID_ICX	0x0b00

RTE_LOG_REGISTER(ioat_pmd_logtype, rawdev.ioat, INFO);

#define IOAT_PMD_LOG(level, fmt, args...) rte_log(RTE_LOG_ ## level, \
	ioat_pmd_logtype, "%s(): " fmt "\n", __func__, ##args)

#define IOAT_PMD_DEBUG(fmt, args...)  IOAT_PMD_LOG(DEBUG, fmt, ## args)
#define IOAT_PMD_INFO(fmt, args...)   IOAT_PMD_LOG(INFO, fmt, ## args)
#define IOAT_PMD_ERR(fmt, args...)    IOAT_PMD_LOG(ERR, fmt, ## args)
#define IOAT_PMD_WARN(fmt, args...)   IOAT_PMD_LOG(WARNING, fmt, ## args)

#define DESC_SZ sizeof(struct rte_ioat_generic_hw_desc)
#define COMPLETION_SZ sizeof(__m128i)

static int
ioat_dev_configure(const struct rte_rawdev *dev, rte_rawdev_obj_t config)
{
	struct rte_ioat_rawdev_config *params = config;
	struct rte_ioat_rawdev *ioat = dev->dev_private;
	char mz_name[RTE_MEMZONE_NAMESIZE];
	unsigned short i;

	if (dev->started)
		return -EBUSY;

	if (params == NULL)
		return -EINVAL;

	if (params->ring_size > 4096 || params->ring_size < 64 ||
			!rte_is_power_of_2(params->ring_size))
		return -EINVAL;

	ioat->ring_size = params->ring_size;
	if (ioat->desc_ring != NULL) {
		rte_memzone_free(ioat->desc_mz);
		ioat->desc_ring = NULL;
		ioat->desc_mz = NULL;
	}

	/* allocate one block of memory for both descriptors
	 * and completion handles.
	 */
	snprintf(mz_name, sizeof(mz_name), "rawdev%u_desc_ring", dev->dev_id);
	ioat->desc_mz = rte_memzone_reserve(mz_name,
			(DESC_SZ + COMPLETION_SZ) * ioat->ring_size,
			dev->device->numa_node, RTE_MEMZONE_IOVA_CONTIG);
	if (ioat->desc_mz == NULL)
		return -ENOMEM;
	ioat->desc_ring = ioat->desc_mz->addr;
	ioat->hdls = (void *)&ioat->desc_ring[ioat->ring_size];

	ioat->ring_addr = ioat->desc_mz->iova;

	/* configure descriptor ring - each one points to next */
	for (i = 0; i < ioat->ring_size; i++) {
		ioat->desc_ring[i].next = ioat->ring_addr +
				(((i + 1) % ioat->ring_size) * DESC_SZ);
	}

	return 0;
}

static int
ioat_dev_start(struct rte_rawdev *dev)
{
	struct rte_ioat_rawdev *ioat = dev->dev_private;

	if (ioat->ring_size == 0 || ioat->desc_ring == NULL)
		return -EBUSY;

	/* inform hardware of where the descriptor ring is */
	ioat->regs->chainaddr = ioat->ring_addr;
	/* inform hardware of where to write the status/completions */
	ioat->regs->chancmp = ioat->status_addr;

	/* prime the status register to be set to the last element */
	ioat->status =  ioat->ring_addr + ((ioat->ring_size - 1) * DESC_SZ);
	return 0;
}

static void
ioat_dev_stop(struct rte_rawdev *dev)
{
	RTE_SET_USED(dev);
}

static void
ioat_dev_info_get(struct rte_rawdev *dev, rte_rawdev_obj_t dev_info)
{
	struct rte_ioat_rawdev_config *cfg = dev_info;
	struct rte_ioat_rawdev *ioat = dev->dev_private;

	if (cfg != NULL)
		cfg->ring_size = ioat->ring_size;
}

static const char * const xstat_names[] = {
		"failed_enqueues", "successful_enqueues",
		"copies_started", "copies_completed"
};

static int
ioat_xstats_get(const struct rte_rawdev *dev, const unsigned int ids[],
		uint64_t values[], unsigned int n)
{
	const struct rte_ioat_rawdev *ioat = dev->dev_private;
	unsigned int i;

	for (i = 0; i < n; i++) {
		switch (ids[i]) {
		case 0: values[i] = ioat->enqueue_failed; break;
		case 1: values[i] = ioat->enqueued; break;
		case 2: values[i] = ioat->started; break;
		case 3: values[i] = ioat->completed; break;
		default: values[i] = 0; break;
		}
	}
	return n;
}

static int
ioat_xstats_get_names(const struct rte_rawdev *dev,
		struct rte_rawdev_xstats_name *names,
		unsigned int size)
{
	unsigned int i;

	RTE_SET_USED(dev);
	if (size < RTE_DIM(xstat_names))
		return RTE_DIM(xstat_names);

	for (i = 0; i < RTE_DIM(xstat_names); i++)
		strlcpy(names[i].name, xstat_names[i], sizeof(names[i]));

	return RTE_DIM(xstat_names);
}

static int
ioat_xstats_reset(struct rte_rawdev *dev, const uint32_t *ids, uint32_t nb_ids)
{
	struct rte_ioat_rawdev *ioat = dev->dev_private;
	unsigned int i;

	if (!ids) {
		ioat->enqueue_failed = 0;
		ioat->enqueued = 0;
		ioat->started = 0;
		ioat->completed = 0;
		return 0;
	}

	for (i = 0; i < nb_ids; i++) {
		switch (ids[i]) {
		case 0:
			ioat->enqueue_failed = 0;
			break;
		case 1:
			ioat->enqueued = 0;
			break;
		case 2:
			ioat->started = 0;
			break;
		case 3:
			ioat->completed = 0;
			break;
		default:
			IOAT_PMD_WARN("Invalid xstat id - cannot reset value");
			break;
		}
	}

	return 0;
}

extern int ioat_rawdev_test(uint16_t dev_id);

static int
ioat_rawdev_create(const char *name, struct rte_pci_device *dev)
{
	static const struct rte_rawdev_ops ioat_rawdev_ops = {
			.dev_configure = ioat_dev_configure,
			.dev_start = ioat_dev_start,
			.dev_stop = ioat_dev_stop,
			.dev_info_get = ioat_dev_info_get,
			.xstats_get = ioat_xstats_get,
			.xstats_get_names = ioat_xstats_get_names,
			.xstats_reset = ioat_xstats_reset,
			.dev_selftest = ioat_rawdev_test,
	};

	struct rte_rawdev *rawdev = NULL;
	struct rte_ioat_rawdev *ioat = NULL;
	const struct rte_memzone *mz = NULL;
	char mz_name[RTE_MEMZONE_NAMESIZE];
	int ret = 0;
	int retry = 0;

	if (!name) {
		IOAT_PMD_ERR("Invalid name of the device!");
		ret = -EINVAL;
		goto cleanup;
	}

	/* Allocate device structure */
	rawdev = rte_rawdev_pmd_allocate(name, sizeof(struct rte_ioat_rawdev),
					 dev->device.numa_node);
	if (rawdev == NULL) {
		IOAT_PMD_ERR("Unable to allocate raw device");
		ret = -ENOMEM;
		goto cleanup;
	}

	snprintf(mz_name, sizeof(mz_name), "rawdev%u_private", rawdev->dev_id);
	mz = rte_memzone_reserve(mz_name, sizeof(struct rte_ioat_rawdev),
			dev->device.numa_node, RTE_MEMZONE_IOVA_CONTIG);
	if (mz == NULL) {
		IOAT_PMD_ERR("Unable to reserve memzone for private data\n");
		ret = -ENOMEM;
		goto cleanup;
	}

	rawdev->dev_private = mz->addr;
	rawdev->dev_ops = &ioat_rawdev_ops;
	rawdev->device = &dev->device;
	rawdev->driver_name = dev->device.driver->name;

	ioat = rawdev->dev_private;
	ioat->rawdev = rawdev;
	ioat->mz = mz;
	ioat->regs = dev->mem_resource[0].addr;
	ioat->ring_size = 0;
	ioat->desc_ring = NULL;
	ioat->status_addr = ioat->mz->iova +
			offsetof(struct rte_ioat_rawdev, status);

	/* do device initialization - reset and set error behaviour */
	if (ioat->regs->chancnt != 1)
		IOAT_PMD_ERR("%s: Channel count == %d\n", __func__,
				ioat->regs->chancnt);

	if (ioat->regs->chanctrl & 0x100) { /* locked by someone else */
		IOAT_PMD_WARN("%s: Channel appears locked\n", __func__);
		ioat->regs->chanctrl = 0;
	}

	ioat->regs->chancmd = RTE_IOAT_CHANCMD_SUSPEND;
	rte_delay_ms(1);
	ioat->regs->chancmd = RTE_IOAT_CHANCMD_RESET;
	rte_delay_ms(1);
	while (ioat->regs->chancmd & RTE_IOAT_CHANCMD_RESET) {
		ioat->regs->chainaddr = 0;
		rte_delay_ms(1);
		if (++retry >= 200) {
			IOAT_PMD_ERR("%s: cannot reset device. CHANCMD=0x%"PRIx8", CHANSTS=0x%"PRIx64", CHANERR=0x%"PRIx32"\n",
					__func__,
					ioat->regs->chancmd,
					ioat->regs->chansts,
					ioat->regs->chanerr);
			ret = -EIO;
		}
	}
	ioat->regs->chanctrl = RTE_IOAT_CHANCTRL_ANY_ERR_ABORT_EN |
			RTE_IOAT_CHANCTRL_ERR_COMPLETION_EN;

	return 0;

cleanup:
	if (rawdev)
		rte_rawdev_pmd_release(rawdev);

	return ret;
}

static int
ioat_rawdev_destroy(const char *name)
{
	int ret;
	struct rte_rawdev *rdev;

	if (!name) {
		IOAT_PMD_ERR("Invalid device name");
		return -EINVAL;
	}

	rdev = rte_rawdev_pmd_get_named_dev(name);
	if (!rdev) {
		IOAT_PMD_ERR("Invalid device name (%s)", name);
		return -EINVAL;
	}

	if (rdev->dev_private != NULL) {
		struct rte_ioat_rawdev *ioat = rdev->dev_private;
		rdev->dev_private = NULL;
		rte_memzone_free(ioat->desc_mz);
		rte_memzone_free(ioat->mz);
	}

	/* rte_rawdev_close is called by pmd_release */
	ret = rte_rawdev_pmd_release(rdev);
	if (ret)
		IOAT_PMD_DEBUG("Device cleanup failed");

	return 0;
}

static int
ioat_rawdev_probe(struct rte_pci_driver *drv, struct rte_pci_device *dev)
{
	char name[32];
	int ret = 0;


	rte_pci_device_name(&dev->addr, name, sizeof(name));
	IOAT_PMD_INFO("Init %s on NUMA node %d", name, dev->device.numa_node);

	dev->device.driver = &drv->driver;
	ret = ioat_rawdev_create(name, dev);
	return ret;
}

static int
ioat_rawdev_remove(struct rte_pci_device *dev)
{
	char name[32];
	int ret;

	rte_pci_device_name(&dev->addr, name, sizeof(name));

	IOAT_PMD_INFO("Closing %s on NUMA node %d",
			name, dev->device.numa_node);

	ret = ioat_rawdev_destroy(name);
	return ret;
}

static const struct rte_pci_id pci_id_ioat_map[] = {
	{ RTE_PCI_DEVICE(IOAT_VENDOR_ID, IOAT_DEVICE_ID_SKX) },
	{ RTE_PCI_DEVICE(IOAT_VENDOR_ID, IOAT_DEVICE_ID_BDX0) },
	{ RTE_PCI_DEVICE(IOAT_VENDOR_ID, IOAT_DEVICE_ID_BDX1) },
	{ RTE_PCI_DEVICE(IOAT_VENDOR_ID, IOAT_DEVICE_ID_BDX2) },
	{ RTE_PCI_DEVICE(IOAT_VENDOR_ID, IOAT_DEVICE_ID_BDX3) },
	{ RTE_PCI_DEVICE(IOAT_VENDOR_ID, IOAT_DEVICE_ID_BDX4) },
	{ RTE_PCI_DEVICE(IOAT_VENDOR_ID, IOAT_DEVICE_ID_BDX5) },
	{ RTE_PCI_DEVICE(IOAT_VENDOR_ID, IOAT_DEVICE_ID_BDX6) },
	{ RTE_PCI_DEVICE(IOAT_VENDOR_ID, IOAT_DEVICE_ID_BDX7) },
	{ RTE_PCI_DEVICE(IOAT_VENDOR_ID, IOAT_DEVICE_ID_BDXE) },
	{ RTE_PCI_DEVICE(IOAT_VENDOR_ID, IOAT_DEVICE_ID_BDXF) },
	{ RTE_PCI_DEVICE(IOAT_VENDOR_ID, IOAT_DEVICE_ID_ICX) },
	{ .vendor_id = 0, /* sentinel */ },
};

static struct rte_pci_driver ioat_pmd_drv = {
	.id_table = pci_id_ioat_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
	.probe = ioat_rawdev_probe,
	.remove = ioat_rawdev_remove,
};

RTE_PMD_REGISTER_PCI(IOAT_PMD_RAWDEV_NAME, ioat_pmd_drv);
RTE_PMD_REGISTER_PCI_TABLE(IOAT_PMD_RAWDEV_NAME, pci_id_ioat_map);
RTE_PMD_REGISTER_KMOD_DEP(IOAT_PMD_RAWDEV_NAME, "* igb_uio | uio_pci_generic");
