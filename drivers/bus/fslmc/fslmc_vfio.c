/* SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright (c) 2015-2016 Freescale Semiconductor, Inc. All rights reserved.
 *   Copyright 2016 NXP
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/vfs.h>
#include <libgen.h>
#include <dirent.h>
#include <sys/eventfd.h>

#include <eal_filesystem.h>
#include <rte_mbuf.h>
#include <rte_ethdev_driver.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_string_fns.h>
#include <rte_cycles.h>
#include <rte_kvargs.h>
#include <rte_dev.h>
#include <rte_bus.h>

#include "rte_fslmc.h"
#include "fslmc_vfio.h"
#include <mc/fsl_dpmng.h>

#include "portal/dpaa2_hw_pvt.h"
#include "portal/dpaa2_hw_dpio.h"

#define FSLMC_VFIO_LOG(level, fmt, args...) \
	RTE_LOG(level, EAL, fmt "\n", ##args)

/** Pathname of FSL-MC devices directory. */
#define SYSFS_FSL_MC_DEVICES "/sys/bus/fsl-mc/devices"

#define FSLMC_CONTAINER_MAX_LEN 8 /**< Of the format dprc.XX */

/* Number of VFIO containers & groups with in */
static struct fslmc_vfio_group vfio_group;
static struct fslmc_vfio_container vfio_container;
static int container_device_fd;
static char *g_container;
static uint32_t *msi_intr_vaddr;
void *(*rte_mcp_ptr_list);
static int is_dma_done;

static struct rte_dpaa2_object_list dpaa2_obj_list =
	TAILQ_HEAD_INITIALIZER(dpaa2_obj_list);

/*register a fslmc bus based dpaa2 driver */
void
rte_fslmc_object_register(struct rte_dpaa2_object *object)
{
	RTE_VERIFY(object);

	TAILQ_INSERT_TAIL(&dpaa2_obj_list, object, next);
}

int
fslmc_get_container_group(int *groupid)
{
	int ret;
	char *container;

	if (!g_container) {
		container = getenv("DPRC");
		if (container == NULL) {
			RTE_LOG(WARNING, EAL, "DPAA2: DPRC not available\n");
			return -EINVAL;
		}

		if (strlen(container) >= FSLMC_CONTAINER_MAX_LEN) {
			FSLMC_VFIO_LOG(ERR, "Invalid container name: %s\n",
				       container);
			return -1;
		}

		g_container = strdup(container);
		if (!g_container) {
			FSLMC_VFIO_LOG(ERR, "Out of memory.");
			return -ENOMEM;
		}
	}

	/* get group number */
	ret = vfio_get_group_no(SYSFS_FSL_MC_DEVICES, g_container, groupid);
	if (ret <= 0) {
		FSLMC_VFIO_LOG(ERR, "Unable to find %s IOMMU group",
			       g_container);
		return -1;
	}

	FSLMC_VFIO_LOG(DEBUG, "Container: %s has VFIO iommu group id = %d",
		       g_container, *groupid);

	return 0;
}

static int
vfio_connect_container(void)
{
	int fd, ret;

	if (vfio_container.used) {
		FSLMC_VFIO_LOG(DEBUG, "No container available.");
		return -1;
	}

	/* Try connecting to vfio container if already created */
	if (!ioctl(vfio_group.fd, VFIO_GROUP_SET_CONTAINER,
		&vfio_container.fd)) {
		FSLMC_VFIO_LOG(INFO,
		    "Container pre-exists with FD[0x%x] for this group",
		    vfio_container.fd);
		vfio_group.container = &vfio_container;
		return 0;
	}

	/* Opens main vfio file descriptor which represents the "container" */
	fd = vfio_get_container_fd();
	if (fd < 0) {
		FSLMC_VFIO_LOG(ERR, "Failed to open VFIO container");
		return -errno;
	}

	/* Check whether support for SMMU type IOMMU present or not */
	if (ioctl(fd, VFIO_CHECK_EXTENSION, VFIO_TYPE1_IOMMU)) {
		/* Connect group to container */
		ret = ioctl(vfio_group.fd, VFIO_GROUP_SET_CONTAINER, &fd);
		if (ret) {
			FSLMC_VFIO_LOG(ERR, "Failed to setup group container");
			close(fd);
			return -errno;
		}

		ret = ioctl(fd, VFIO_SET_IOMMU, VFIO_TYPE1_IOMMU);
		if (ret) {
			FSLMC_VFIO_LOG(ERR, "Failed to setup VFIO iommu");
			close(fd);
			return -errno;
		}
	} else {
		FSLMC_VFIO_LOG(ERR, "No supported IOMMU available");
		close(fd);
		return -EINVAL;
	}

	vfio_container.used = 1;
	vfio_container.fd = fd;
	vfio_container.group = &vfio_group;
	vfio_group.container = &vfio_container;

	return 0;
}

static int vfio_map_irq_region(struct fslmc_vfio_group *group)
{
	int ret;
	unsigned long *vaddr = NULL;
	struct vfio_iommu_type1_dma_map map = {
		.argsz = sizeof(map),
		.flags = VFIO_DMA_MAP_FLAG_READ | VFIO_DMA_MAP_FLAG_WRITE,
		.vaddr = 0x6030000,
		.iova = 0x6030000,
		.size = 0x1000,
	};

	vaddr = (unsigned long *)mmap(NULL, 0x1000, PROT_WRITE |
		PROT_READ, MAP_SHARED, container_device_fd, 0x6030000);
	if (vaddr == MAP_FAILED) {
		FSLMC_VFIO_LOG(ERR, "Unable to map region (errno = %d)", errno);
		return -errno;
	}

	msi_intr_vaddr = (uint32_t *)((char *)(vaddr) + 64);
	map.vaddr = (unsigned long)vaddr;
	ret = ioctl(group->container->fd, VFIO_IOMMU_MAP_DMA, &map);
	if (ret == 0)
		return 0;

	FSLMC_VFIO_LOG(ERR, "VFIO_IOMMU_MAP_DMA fails (errno = %d)", errno);
	return -errno;
}

int rte_fslmc_vfio_dmamap(void)
{
	int ret;
	struct fslmc_vfio_group *group;
	struct vfio_iommu_type1_dma_map dma_map = {
		.argsz = sizeof(struct vfio_iommu_type1_dma_map),
		.flags = VFIO_DMA_MAP_FLAG_READ | VFIO_DMA_MAP_FLAG_WRITE,
	};

	int i;
	const struct rte_memseg *memseg;

	if (is_dma_done)
		return 0;

	memseg = rte_eal_get_physmem_layout();
	if (memseg == NULL) {
		FSLMC_VFIO_LOG(ERR, "Cannot get physical layout.");
		return -ENODEV;
	}

	for (i = 0; i < RTE_MAX_MEMSEG; i++) {
		if (memseg[i].addr == NULL && memseg[i].len == 0) {
			FSLMC_VFIO_LOG(DEBUG, "Total %d segments found.", i);
			break;
		}

		dma_map.size = memseg[i].len;
		dma_map.vaddr = memseg[i].addr_64;
#ifdef RTE_LIBRTE_DPAA2_USE_PHYS_IOVA
		if (rte_eal_iova_mode() == RTE_IOVA_VA)
			dma_map.iova = dma_map.vaddr;
		else
			dma_map.iova = memseg[i].iova;
#else
		dma_map.iova = dma_map.vaddr;
#endif

		/* SET DMA MAP for IOMMU */
		group = &vfio_group;

		if (!group->container) {
			FSLMC_VFIO_LOG(ERR, "Container is not connected ");
			return -1;
		}

		FSLMC_VFIO_LOG(DEBUG, "-->Initial SHM Virtual ADDR %llX",
			     dma_map.vaddr);
		FSLMC_VFIO_LOG(DEBUG, "-----> DMA size 0x%llX", dma_map.size);
		ret = ioctl(group->container->fd, VFIO_IOMMU_MAP_DMA,
			    &dma_map);
		if (ret) {
			FSLMC_VFIO_LOG(ERR, "VFIO_IOMMU_MAP_DMA API(errno = %d)",
				       errno);
			return ret;
		}
	}

	/* Verifying that at least single segment is available */
	if (i <= 0) {
		FSLMC_VFIO_LOG(ERR, "No Segments found for VFIO Mapping");
		return -1;
	}

	/* TODO - This is a W.A. as VFIO currently does not add the mapping of
	 * the interrupt region to SMMU. This should be removed once the
	 * support is added in the Kernel.
	 */
	vfio_map_irq_region(group);

	is_dma_done = 1;

	return 0;
}

static int64_t vfio_map_mcp_obj(struct fslmc_vfio_group *group, char *mcp_obj)
{
	int64_t v_addr = (int64_t)MAP_FAILED;
	int32_t ret, mc_fd;

	struct vfio_device_info d_info = { .argsz = sizeof(d_info) };
	struct vfio_region_info reg_info = { .argsz = sizeof(reg_info) };

	/* getting the mcp object's fd*/
	mc_fd = ioctl(group->fd, VFIO_GROUP_GET_DEVICE_FD, mcp_obj);
	if (mc_fd < 0) {
		FSLMC_VFIO_LOG(ERR, "error in VFIO get dev %s fd from group %d",
			       mcp_obj, group->fd);
		return v_addr;
	}

	/* getting device info*/
	ret = ioctl(mc_fd, VFIO_DEVICE_GET_INFO, &d_info);
	if (ret < 0) {
		FSLMC_VFIO_LOG(ERR, "error in VFIO getting DEVICE_INFO");
		goto MC_FAILURE;
	}

	/* getting device region info*/
	ret = ioctl(mc_fd, VFIO_DEVICE_GET_REGION_INFO, &reg_info);
	if (ret < 0) {
		FSLMC_VFIO_LOG(ERR, "error in VFIO getting REGION_INFO");
		goto MC_FAILURE;
	}

	FSLMC_VFIO_LOG(DEBUG, "region offset = %llx  , region size = %llx",
		       reg_info.offset, reg_info.size);

	v_addr = (uint64_t)mmap(NULL, reg_info.size,
		PROT_WRITE | PROT_READ, MAP_SHARED,
		mc_fd, reg_info.offset);

MC_FAILURE:
	close(mc_fd);

	return v_addr;
}

#define IRQ_SET_BUF_LEN  (sizeof(struct vfio_irq_set) + sizeof(int))

int rte_dpaa2_intr_enable(struct rte_intr_handle *intr_handle, int index)
{
	int len, ret;
	char irq_set_buf[IRQ_SET_BUF_LEN];
	struct vfio_irq_set *irq_set;
	int *fd_ptr;

	len = sizeof(irq_set_buf);

	irq_set = (struct vfio_irq_set *)irq_set_buf;
	irq_set->argsz = len;
	irq_set->count = 1;
	irq_set->flags =
		VFIO_IRQ_SET_DATA_EVENTFD | VFIO_IRQ_SET_ACTION_TRIGGER;
	irq_set->index = index;
	irq_set->start = 0;
	fd_ptr = (int *)&irq_set->data;
	*fd_ptr = intr_handle->fd;

	ret = ioctl(intr_handle->vfio_dev_fd, VFIO_DEVICE_SET_IRQS, irq_set);
	if (ret) {
		RTE_LOG(ERR, EAL, "Error:dpaa2 SET IRQs fd=%d, err = %d(%s)\n",
			intr_handle->fd, errno, strerror(errno));
		return ret;
	}

	return ret;
}

int rte_dpaa2_intr_disable(struct rte_intr_handle *intr_handle, int index)
{
	struct vfio_irq_set *irq_set;
	char irq_set_buf[IRQ_SET_BUF_LEN];
	int len, ret;

	len = sizeof(struct vfio_irq_set);

	irq_set = (struct vfio_irq_set *)irq_set_buf;
	irq_set->argsz = len;
	irq_set->flags = VFIO_IRQ_SET_DATA_NONE | VFIO_IRQ_SET_ACTION_TRIGGER;
	irq_set->index = index;
	irq_set->start = 0;
	irq_set->count = 0;

	ret = ioctl(intr_handle->vfio_dev_fd, VFIO_DEVICE_SET_IRQS, irq_set);
	if (ret)
		RTE_LOG(ERR, EAL,
			"Error disabling dpaa2 interrupts for fd %d\n",
			intr_handle->fd);

	return ret;
}

/* set up interrupt support (but not enable interrupts) */
int
rte_dpaa2_vfio_setup_intr(struct rte_intr_handle *intr_handle,
			  int vfio_dev_fd,
			  int num_irqs)
{
	int i, ret;

	/* start from MSI-X interrupt type */
	for (i = 0; i < num_irqs; i++) {
		struct vfio_irq_info irq_info = { .argsz = sizeof(irq_info) };
		int fd = -1;

		irq_info.index = i;

		ret = ioctl(vfio_dev_fd, VFIO_DEVICE_GET_IRQ_INFO, &irq_info);
		if (ret < 0) {
			FSLMC_VFIO_LOG(ERR,
				       "cannot get IRQ(%d) info, error %i (%s)",
				       i, errno, strerror(errno));
			return -1;
		}

		/* if this vector cannot be used with eventfd,
		 * fail if we explicitly
		 * specified interrupt type, otherwise continue
		 */
		if ((irq_info.flags & VFIO_IRQ_INFO_EVENTFD) == 0)
			continue;

		/* set up an eventfd for interrupts */
		fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
		if (fd < 0) {
			FSLMC_VFIO_LOG(ERR,
				       "cannot set up eventfd, error %i (%s)\n",
				       errno, strerror(errno));
			return -1;
		}

		intr_handle->fd = fd;
		intr_handle->type = RTE_INTR_HANDLE_VFIO_MSI;
		intr_handle->vfio_dev_fd = vfio_dev_fd;

		return 0;
	}

	/* if we're here, we haven't found a suitable interrupt vector */
	return -1;
}

/*
 * fslmc_process_iodevices for processing only IO (ETH, CRYPTO, and possibly
 * EVENT) devices.
 */
static int
fslmc_process_iodevices(struct rte_dpaa2_device *dev)
{
	int dev_fd;
	struct vfio_device_info device_info = { .argsz = sizeof(device_info) };
	struct rte_dpaa2_object *object = NULL;

	dev_fd = ioctl(vfio_group.fd, VFIO_GROUP_GET_DEVICE_FD,
		       dev->device.name);
	if (dev_fd <= 0) {
		FSLMC_VFIO_LOG(ERR, "Unable to obtain device FD for device:%s",
			       dev->device.name);
		return -1;
	}

	if (ioctl(dev_fd, VFIO_DEVICE_GET_INFO, &device_info)) {
		FSLMC_VFIO_LOG(ERR, "DPAA2 VFIO_DEVICE_GET_INFO fail");
		return -1;
	}

	switch (dev->dev_type) {
	case DPAA2_ETH:
		rte_dpaa2_vfio_setup_intr(&dev->intr_handle, dev_fd,
					  device_info.num_irqs);
		break;
	case DPAA2_CON:
	case DPAA2_IO:
	case DPAA2_CI:
	case DPAA2_BPOOL:
		TAILQ_FOREACH(object, &dpaa2_obj_list, next) {
			if (dev->dev_type == object->dev_type)
				object->create(dev_fd, &device_info,
					       dev->object_id);
			else
				continue;
		}
		break;
	default:
		break;
	}

	FSLMC_VFIO_LOG(DEBUG, "Device (%s) abstracted from VFIO",
		       dev->device.name);
	return 0;
}

static int
fslmc_process_mcp(struct rte_dpaa2_device *dev)
{
	int64_t v_addr;
	char *dev_name;
	struct fsl_mc_io dpmng  = {0};
	struct mc_version mc_ver_info = {0};

	rte_mcp_ptr_list = malloc(sizeof(void *) * 1);
	if (!rte_mcp_ptr_list) {
		FSLMC_VFIO_LOG(ERR, "Out of memory");
		return -ENOMEM;
	}

	dev_name = strdup(dev->device.name);
	if (!dev_name) {
		FSLMC_VFIO_LOG(ERR, "Out of memory.");
		free(rte_mcp_ptr_list);
		rte_mcp_ptr_list = NULL;
		return -ENOMEM;
	}

	v_addr = vfio_map_mcp_obj(&vfio_group, dev_name);
	if (v_addr == (int64_t)MAP_FAILED) {
		FSLMC_VFIO_LOG(ERR, "Error mapping region  (errno = %d)",
			       errno);
		free(rte_mcp_ptr_list);
		rte_mcp_ptr_list = NULL;
		return -1;
	}

	/* check the MC version compatibility */
	dpmng.regs = (void *)v_addr;
	if (mc_get_version(&dpmng, CMD_PRI_LOW, &mc_ver_info))
		RTE_LOG(WARNING, PMD, "\tmc_get_version failed\n");

	if ((mc_ver_info.major != MC_VER_MAJOR) ||
	    (mc_ver_info.minor < MC_VER_MINOR)) {
		RTE_LOG(ERR, PMD, "DPAA2 MC version not compatible!"
			" Expected %d.%d.x, Detected %d.%d.%d\n",
			MC_VER_MAJOR, MC_VER_MINOR,
			mc_ver_info.major, mc_ver_info.minor,
			mc_ver_info.revision);
		free(rte_mcp_ptr_list);
		rte_mcp_ptr_list = NULL;
		return -1;
	}
	rte_mcp_ptr_list[0] = (void *)v_addr;

	return 0;
}

int
fslmc_vfio_process_group(void)
{
	int ret;
	int found_mportal = 0;
	struct rte_dpaa2_device *dev, *dev_temp;

	/* Search the MCP as that should be initialized first. */
	TAILQ_FOREACH_SAFE(dev, &rte_fslmc_bus.device_list, next, dev_temp) {
		if (dev->dev_type == DPAA2_MPORTAL) {
			ret = fslmc_process_mcp(dev);
			if (ret) {
				FSLMC_VFIO_LOG(DEBUG, "Unable to map Portal.");
				return -1;
			}
			if (!found_mportal)
				found_mportal = 1;

			TAILQ_REMOVE(&rte_fslmc_bus.device_list, dev, next);
			free(dev);
			dev = NULL;
			/* Ideally there is only a single dpmcp, but in case
			 * multiple exists, looping on remaining devices.
			 */
		}
	}

	/* Cannot continue if there is not even a single mportal */
	if (!found_mportal) {
		FSLMC_VFIO_LOG(DEBUG,
			       "No MC Portal device found. Not continuing.");
		return -1;
	}

	TAILQ_FOREACH_SAFE(dev, &rte_fslmc_bus.device_list, next, dev_temp) {
		if (!dev)
			break;

		switch (dev->dev_type) {
		case DPAA2_ETH:
		case DPAA2_CRYPTO:
			ret = fslmc_process_iodevices(dev);
			if (ret) {
				FSLMC_VFIO_LOG(DEBUG,
					       "Dev (%s) init failed.",
					       dev->device.name);
				return ret;
			}
			break;
		case DPAA2_CON:
		case DPAA2_IO:
		case DPAA2_CI:
		case DPAA2_BPOOL:
			/* Call the object creation routine and remove the
			 * device entry from device list
			 */
			ret = fslmc_process_iodevices(dev);
			if (ret) {
				FSLMC_VFIO_LOG(DEBUG,
					       "Dev (%s) init failed.",
					       dev->device.name);
				return -1;
			}

			/* This device is not required to be in the DPDK
			 * exposed device list.
			 */
			TAILQ_REMOVE(&rte_fslmc_bus.device_list, dev, next);
			free(dev);
			dev = NULL;
			break;
		case DPAA2_UNKNOWN:
		default:
			/* Unknown - ignore */
			FSLMC_VFIO_LOG(DEBUG, "Found unknown device (%s).",
				       dev->device.name);
			TAILQ_REMOVE(&rte_fslmc_bus.device_list, dev, next);
			free(dev);
			dev = NULL;
		}
	}

	return 0;
}

int
fslmc_vfio_setup_group(void)
{
	int groupid;
	int ret;
	struct vfio_group_status status = { .argsz = sizeof(status) };

	/* if already done once */
	if (container_device_fd)
		return 0;

	ret = fslmc_get_container_group(&groupid);
	if (ret)
		return ret;

	/* In case this group was already opened, continue without any
	 * processing.
	 */
	if (vfio_group.groupid == groupid) {
		FSLMC_VFIO_LOG(ERR, "groupid already exists %d", groupid);
		return 0;
	}

	/* Get the actual group fd */
	ret = vfio_get_group_fd(groupid);
	if (ret < 0)
		return ret;
	vfio_group.fd = ret;

	/* Check group viability */
	ret = ioctl(vfio_group.fd, VFIO_GROUP_GET_STATUS, &status);
	if (ret) {
		FSLMC_VFIO_LOG(ERR, "VFIO error getting group status");
		close(vfio_group.fd);
		rte_vfio_clear_group(vfio_group.fd);
		return ret;
	}

	if (!(status.flags & VFIO_GROUP_FLAGS_VIABLE)) {
		FSLMC_VFIO_LOG(ERR, "VFIO group not viable");
		close(vfio_group.fd);
		rte_vfio_clear_group(vfio_group.fd);
		return -EPERM;
	}
	/* Since Group is VIABLE, Store the groupid */
	vfio_group.groupid = groupid;

	/* check if group does not have a container yet */
	if (!(status.flags & VFIO_GROUP_FLAGS_CONTAINER_SET)) {
		/* Now connect this IOMMU group to given container */
		ret = vfio_connect_container();
		if (ret) {
			FSLMC_VFIO_LOG(ERR,
				"Error connecting container with groupid %d",
				groupid);
			close(vfio_group.fd);
			rte_vfio_clear_group(vfio_group.fd);
			return ret;
		}
	}

	/* Get Device information */
	ret = ioctl(vfio_group.fd, VFIO_GROUP_GET_DEVICE_FD, g_container);
	if (ret < 0) {
		FSLMC_VFIO_LOG(ERR, "Error getting device %s fd from group %d",
			       g_container, vfio_group.groupid);
		close(vfio_group.fd);
		rte_vfio_clear_group(vfio_group.fd);
		return ret;
	}
	container_device_fd = ret;
	FSLMC_VFIO_LOG(DEBUG, "VFIO Container FD is [0x%X]",
		       container_device_fd);

	return 0;
}
