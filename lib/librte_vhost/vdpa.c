/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

/**
 * @file
 *
 * Device specific vhost lib
 */

#include <stdbool.h>

#include <rte_malloc.h>
#include "rte_vdpa.h"
#include "vhost.h"

static struct rte_vdpa_device *vdpa_devices[MAX_VHOST_DEVICE];
static uint32_t vdpa_device_num;

static bool
is_same_vdpa_device(struct rte_vdpa_dev_addr *a,
		struct rte_vdpa_dev_addr *b)
{
	bool ret = true;

	if (a->type != b->type)
		return false;

	switch (a->type) {
	case PCI_ADDR:
		if (a->pci_addr.domain != b->pci_addr.domain ||
				a->pci_addr.bus != b->pci_addr.bus ||
				a->pci_addr.devid != b->pci_addr.devid ||
				a->pci_addr.function != b->pci_addr.function)
			ret = false;
		break;
	default:
		break;
	}

	return ret;
}

int
rte_vdpa_register_device(struct rte_vdpa_dev_addr *addr,
		struct rte_vdpa_dev_ops *ops)
{
	struct rte_vdpa_device *dev;
	char device_name[MAX_VDPA_NAME_LEN];
	int i;

	if (vdpa_device_num >= MAX_VHOST_DEVICE)
		return -1;

	for (i = 0; i < MAX_VHOST_DEVICE; i++) {
		dev = vdpa_devices[i];
		if (dev && is_same_vdpa_device(&dev->addr, addr))
			return -1;
	}

	for (i = 0; i < MAX_VHOST_DEVICE; i++) {
		if (vdpa_devices[i] == NULL)
			break;
	}

	sprintf(device_name, "vdpa-dev-%d", i);
	dev = rte_zmalloc(device_name, sizeof(struct rte_vdpa_device),
			RTE_CACHE_LINE_SIZE);
	if (!dev)
		return -1;

	memcpy(&dev->addr, addr, sizeof(struct rte_vdpa_dev_addr));
	dev->ops = ops;
	vdpa_devices[i] = dev;
	vdpa_device_num++;

	return i;
}

int
rte_vdpa_unregister_device(int did)
{
	if (did < 0 || did >= MAX_VHOST_DEVICE || vdpa_devices[did] == NULL)
		return -1;

	rte_free(vdpa_devices[did]);
	vdpa_devices[did] = NULL;
	vdpa_device_num--;

	return did;
}

int
rte_vdpa_find_device_id(struct rte_vdpa_dev_addr *addr)
{
	struct rte_vdpa_device *dev;
	int i;

	for (i = 0; i < MAX_VHOST_DEVICE; ++i) {
		dev = vdpa_devices[i];
		if (dev && is_same_vdpa_device(&dev->addr, addr))
			return i;
	}

	return -1;
}

struct rte_vdpa_device *
rte_vdpa_get_device(int did)
{
	if (did < 0 || did >= MAX_VHOST_DEVICE)
		return NULL;

	return vdpa_devices[did];
}
