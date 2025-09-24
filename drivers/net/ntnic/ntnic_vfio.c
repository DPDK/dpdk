/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <sys/ioctl.h>

#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_vfio.h>
#include <rte_dev.h>
#include <rte_bus_pci.h>
#include <rte_spinlock.h>

#include <ntlog.h>
#include <nt_util.h>
#include "ntnic_vfio.h"

#define ONE_G_SIZE 0x40000000
#define ONE_G_MASK (ONE_G_SIZE - 1)
#define START_VF_IOVA 0x220000000000

static int
nt_vfio_vf_num(const struct rte_pci_device *pdev)
{
	return ((pdev->addr.devid & 0x1f) << 3) + ((pdev->addr.function) & 0x7);
}

/* Internal API */
struct vfio_dev {
	int container_fd;
	int group_fd;
	int dev_fd;
	uint64_t iova_addr;
};

static struct vfio_dev vfio_list[256];

static struct vfio_dev *
vfio_get(int vf_num)
{
	if (vf_num < 0 || vf_num > 255)
		return NULL;

	return &vfio_list[vf_num];
}

/* External API */
int
nt_vfio_setup(struct rte_pci_device *dev)
{
	int ret;
	char devname[RTE_DEV_NAME_MAX_LEN] = { 0 };
	int iommu_group_num;
	int vf_num;
	struct vfio_dev *vfio;

	NT_LOG(INF, NTNIC, "NT VFIO device setup %s", dev->name);

	vf_num = nt_vfio_vf_num(dev);

	vfio = vfio_get(vf_num);

	if (vfio == NULL) {
		NT_LOG(ERR, NTNIC, "VFIO device setup failed. Illegal device id");
		return -1;
	}

	vfio->dev_fd = -1;
	vfio->group_fd = -1;
	vfio->iova_addr = START_VF_IOVA;

	rte_pci_device_name(&dev->addr, devname, RTE_DEV_NAME_MAX_LEN);
	ret = rte_vfio_get_group_num(rte_pci_get_sysfs_path(), devname, &iommu_group_num);
	if (ret <= 0)
		return -1;

	if (vf_num == 0) {
		/* use default container for pf0 */
		vfio->container_fd = RTE_VFIO_DEFAULT_CONTAINER_FD;

	} else {
		vfio->container_fd = rte_vfio_container_create();

		if (vfio->container_fd < 0) {
			NT_LOG(ERR, NTNIC,
				"VFIO device setup failed. VFIO container creation failed.");
			return -1;
		}
	}

	vfio->group_fd = rte_vfio_container_group_bind(vfio->container_fd, iommu_group_num);

	if (vfio->group_fd < 0) {
		NT_LOG(ERR, NTNIC,
			"VFIO device setup failed. VFIO container group bind failed.");
		goto err;
	}

	if (vf_num > 0) {
		if (rte_pci_map_device(dev)) {
			NT_LOG(ERR, NTNIC,
				"Map VFIO device failed. is the vfio-pci driver loaded?");
			goto err;
		}
	}

	vfio->dev_fd = rte_intr_dev_fd_get(dev->intr_handle);

	NT_LOG(DBG, NTNIC,
		"%s: VFIO id=%d, dev_fd=%d, container_fd=%d, group_fd=%d, iommu_group_num=%d",
		dev->name, vf_num, vfio->dev_fd, vfio->container_fd, vfio->group_fd,
		iommu_group_num);

	return vf_num;

err:

	if (vfio->container_fd != RTE_VFIO_DEFAULT_CONTAINER_FD)
		rte_vfio_container_destroy(vfio->container_fd);

	return -1;
}

int
nt_vfio_remove(int vf_num)
{
	struct vfio_dev *vfio;

	NT_LOG(DBG, NTNIC, "NT VFIO device remove VF=%d", vf_num);

	vfio = vfio_get(vf_num);

	if (!vfio) {
		NT_LOG(ERR, NTNIC, "VFIO device remove failed. Illegal device id");
		return -1;
	}

	rte_vfio_container_destroy(vfio->container_fd);
	return 0;
}

int
nt_vfio_dma_map(int vf_num, void *virt_addr, uint64_t *iova_addr, uint64_t size)
{
	uint64_t gp_virt_base;
	uint64_t gp_offset;

	if (size == ONE_G_SIZE) {
		gp_virt_base = (uint64_t)virt_addr & ~ONE_G_MASK;
		gp_offset = (uint64_t)virt_addr & ONE_G_MASK;

	} else {
		gp_virt_base = (uint64_t)virt_addr;
		gp_offset = 0;
	}

	struct vfio_dev *vfio;

	vfio = vfio_get(vf_num);

	if (vfio == NULL) {
		NT_LOG(ERR, NTNIC, "VFIO MAP: VF number %d invalid", vf_num);
		return -1;
	}

	NT_LOG(DBG, NTNIC,
		"VFIO MMAP VF=%d VirtAddr=%p HPA=%" PRIX64 " VirtBase=%" PRIX64
		" IOVA Addr=%" PRIX64 " size=%" PRIX64,
		vf_num, virt_addr, rte_malloc_virt2iova(virt_addr), gp_virt_base, vfio->iova_addr,
		size);

	int res = rte_vfio_container_dma_map(vfio->container_fd, gp_virt_base, vfio->iova_addr,
			size);

	NT_LOG(DBG, NTNIC, "VFIO MMAP res %i, container_fd %i, vf_num %i", res,
		vfio->container_fd, vf_num);

	if (res) {
		NT_LOG(ERR, NTNIC, "rte_vfio_container_dma_map failed: res %d", res);
		return -1;
	}

	*iova_addr = vfio->iova_addr + gp_offset;

	vfio->iova_addr += ONE_G_SIZE;

	return 0;
}

int
nt_vfio_dma_unmap(int vf_num, void *virt_addr, uint64_t iova_addr, uint64_t size)
{
	uint64_t gp_virt_base;
	struct vfio_dev *vfio;

	if (size == ONE_G_SIZE) {
		uint64_t gp_offset;
		gp_virt_base = (uint64_t)virt_addr & ~ONE_G_MASK;
		gp_offset = (uint64_t)virt_addr & ONE_G_MASK;
		iova_addr -= gp_offset;

	} else {
		gp_virt_base = (uint64_t)virt_addr;
	}

	vfio = vfio_get(vf_num);

	if (vfio == NULL) {
		NT_LOG(ERR, NTNIC, "VFIO UNMAP: VF number %d invalid", vf_num);
		return -1;
	}

	int res = rte_vfio_container_dma_unmap(vfio->container_fd, gp_virt_base, iova_addr, size);

	if (res != 0) {
		NT_LOG(ERR, NTNIC,
			"VFIO UNMMAP FAILED! res %i, container_fd %i, vf_num %i, virt_base=%" PRIX64
			", IOVA=%" PRIX64 ", size=%" PRIX64,
			res, vfio->container_fd, vf_num, gp_virt_base, iova_addr, size);
		return -1;
	}

	return 0;
}

void
nt_vfio_init(void)
{
	struct nt_util_vfio_impl s = { .vfio_dma_map = nt_vfio_dma_map,
		       .vfio_dma_unmap = nt_vfio_dma_unmap
	};
	nt_util_vfio_init(&s);
}
