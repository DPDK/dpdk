/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Corigine, Inc.
 * All rights reserved.
 */

#include <pthread.h>

#include <nfp_common_pci.h>
#include <nfp_dev.h>
#include <rte_vfio.h>
#include <vdpa_driver.h>

#include "nfp_vdpa_core.h"
#include "nfp_vdpa_log.h"

#define NFP_VDPA_DRIVER_NAME nfp_vdpa

struct nfp_vdpa_dev {
	struct rte_pci_device *pci_dev;
	struct rte_vdpa_device *vdev;
	struct nfp_vdpa_hw hw;

	int vfio_container_fd;
	int vfio_group_fd;
	int vfio_dev_fd;
	int iommu_group;

	uint16_t max_queues;
};

struct nfp_vdpa_dev_node {
	TAILQ_ENTRY(nfp_vdpa_dev_node) next;
	struct nfp_vdpa_dev *device;
};

TAILQ_HEAD(vdpa_dev_list_head, nfp_vdpa_dev_node);

static struct vdpa_dev_list_head vdpa_dev_list =
	TAILQ_HEAD_INITIALIZER(vdpa_dev_list);

static pthread_mutex_t vdpa_list_lock = PTHREAD_MUTEX_INITIALIZER;

static struct nfp_vdpa_dev_node *
nfp_vdpa_find_node_by_pdev(struct rte_pci_device *pdev)
{
	bool found = false;
	struct nfp_vdpa_dev_node *node;

	pthread_mutex_lock(&vdpa_list_lock);

	TAILQ_FOREACH(node, &vdpa_dev_list, next) {
		if (pdev == node->device->pci_dev) {
			found = true;
			break;
		}
	}

	pthread_mutex_unlock(&vdpa_list_lock);

	if (found)
		return node;

	return NULL;
}

static int
nfp_vdpa_vfio_setup(struct nfp_vdpa_dev *device)
{
	int ret;
	char dev_name[RTE_DEV_NAME_MAX_LEN] = {0};
	struct rte_pci_device *pci_dev = device->pci_dev;

	rte_pci_unmap_device(pci_dev);

	rte_pci_device_name(&pci_dev->addr, dev_name, RTE_DEV_NAME_MAX_LEN);
	rte_vfio_get_group_num(rte_pci_get_sysfs_path(), dev_name,
			&device->iommu_group);

	device->vfio_container_fd = rte_vfio_container_create();
	if (device->vfio_container_fd < 0)
		return -1;

	device->vfio_group_fd = rte_vfio_container_group_bind(
			device->vfio_container_fd, device->iommu_group);
	if (device->vfio_group_fd < 0)
		goto container_destroy;

	DRV_VDPA_LOG(DEBUG, "container_fd=%d, group_fd=%d,\n",
			device->vfio_container_fd, device->vfio_group_fd);

	ret = rte_pci_map_device(pci_dev);
	if (ret != 0)
		goto group_unbind;

	device->vfio_dev_fd = rte_intr_dev_fd_get(pci_dev->intr_handle);

	return 0;

group_unbind:
	rte_vfio_container_group_unbind(device->vfio_container_fd, device->iommu_group);
container_destroy:
	rte_vfio_container_destroy(device->vfio_container_fd);

	return -1;
}

static void
nfp_vdpa_vfio_teardown(struct nfp_vdpa_dev *device)
{
	rte_pci_unmap_device(device->pci_dev);
	rte_vfio_container_group_unbind(device->vfio_container_fd, device->iommu_group);
	rte_vfio_container_destroy(device->vfio_container_fd);
}

struct rte_vdpa_dev_ops nfp_vdpa_ops = {
};

static int
nfp_vdpa_pci_probe(struct rte_pci_device *pci_dev)
{
	int ret;
	struct nfp_vdpa_dev *device;
	struct nfp_vdpa_dev_node *node;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	node = calloc(1, sizeof(*node));
	if (node == NULL)
		return -ENOMEM;

	device = calloc(1, sizeof(*device));
	if (device == NULL)
		goto free_node;

	device->pci_dev = pci_dev;

	ret = nfp_vdpa_vfio_setup(device);
	if (ret != 0)
		goto free_device;

	ret = nfp_vdpa_hw_init(&device->hw, pci_dev);
	if (ret != 0)
		goto vfio_teardown;

	device->max_queues = NFP_VDPA_MAX_QUEUES;

	device->vdev = rte_vdpa_register_device(&pci_dev->device, &nfp_vdpa_ops);
	if (device->vdev == NULL) {
		DRV_VDPA_LOG(ERR, "Failed to register device %s", pci_dev->name);
		goto vfio_teardown;
	}

	node->device = device;
	pthread_mutex_lock(&vdpa_list_lock);
	TAILQ_INSERT_TAIL(&vdpa_dev_list, node, next);
	pthread_mutex_unlock(&vdpa_list_lock);

	return 0;

vfio_teardown:
	nfp_vdpa_vfio_teardown(device);
free_device:
	free(device);
free_node:
	free(node);

	return -1;
}

static int
nfp_vdpa_pci_remove(struct rte_pci_device *pci_dev)
{
	struct nfp_vdpa_dev *device;
	struct nfp_vdpa_dev_node *node;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	node = nfp_vdpa_find_node_by_pdev(pci_dev);
	if (node == NULL) {
		DRV_VDPA_LOG(ERR, "Invalid device: %s", pci_dev->name);
		return -ENODEV;
	}

	device = node->device;

	pthread_mutex_lock(&vdpa_list_lock);
	TAILQ_REMOVE(&vdpa_dev_list, node, next);
	pthread_mutex_unlock(&vdpa_list_lock);

	rte_vdpa_unregister_device(device->vdev);
	nfp_vdpa_vfio_teardown(device);

	free(device);
	free(node);

	return 0;
}

static const struct rte_pci_id pci_id_nfp_vdpa_map[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_NETRONOME,
				PCI_DEVICE_ID_NFP6000_VF_NIC)
	},
	{
		.vendor_id = 0,
	},
};

static struct nfp_class_driver nfp_vdpa = {
	.drv_class = NFP_CLASS_VDPA,
	.name = RTE_STR(NFP_VDPA_DRIVER_NAME),
	.id_table = pci_id_nfp_vdpa_map,
	.drv_flags =  RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
	.probe = nfp_vdpa_pci_probe,
	.remove = nfp_vdpa_pci_remove,
};

RTE_INIT(nfp_vdpa_init)
{
	nfp_class_driver_register(&nfp_vdpa);
}

RTE_PMD_REGISTER_PCI_TABLE(NFP_VDPA_DRIVER_NAME, pci_id_nfp_vdpa_map);
RTE_PMD_REGISTER_KMOD_DEP(NFP_VDPA_DRIVER_NAME, "* vfio-pci");
