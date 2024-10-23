/* SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright (c) 2015-2016 Freescale Semiconductor, Inc. All rights reserved.
 *   Copyright 2016-2024 NXP
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
#include <ethdev_driver.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_string_fns.h>
#include <rte_cycles.h>
#include <rte_kvargs.h>
#include <dev_driver.h>
#include <rte_eal_memconfig.h>
#include <eal_vfio.h>

#include "private.h"
#include "fslmc_vfio.h"
#include "fslmc_logs.h"
#include <mc/fsl_dpmng.h>

#include "portal/dpaa2_hw_pvt.h"
#include "portal/dpaa2_hw_dpio.h"

#define FSLMC_VFIO_MP "fslmc_vfio_mp_sync"

/* Container is composed by multiple groups, however,
 * now each process only supports single group with in container.
 */
static struct fslmc_vfio_container s_vfio_container;
/* Currently we only support single group/process. */
const char *fslmc_group; /* dprc.x*/
static uint32_t *msi_intr_vaddr;
void *(*rte_mcp_ptr_list);

void *
dpaa2_get_mcp_ptr(int portal_idx)
{
	if (rte_mcp_ptr_list)
		return rte_mcp_ptr_list[portal_idx];
	else
		return NULL;
}

static struct rte_dpaa2_object_list dpaa2_obj_list =
	TAILQ_HEAD_INITIALIZER(dpaa2_obj_list);

/*register a fslmc bus based dpaa2 driver */
void
rte_fslmc_object_register(struct rte_dpaa2_object *object)
{
	RTE_VERIFY(object);

	TAILQ_INSERT_TAIL(&dpaa2_obj_list, object, next);
}

static const char *
fslmc_vfio_get_group_name(void)
{
	return fslmc_group;
}

static void
fslmc_vfio_set_group_name(const char *group_name)
{
	fslmc_group = group_name;
}

static int
fslmc_vfio_add_group(int vfio_group_fd,
	int iommu_group_num, const char *group_name)
{
	struct fslmc_vfio_group *group;

	group = rte_zmalloc(NULL, sizeof(struct fslmc_vfio_group), 0);
	if (!group)
		return -ENOMEM;
	group->fd = vfio_group_fd;
	group->groupid = iommu_group_num;
	rte_strscpy(group->group_name, group_name, sizeof(group->group_name));
	if (rte_vfio_noiommu_is_enabled() > 0)
		group->iommu_type = RTE_VFIO_NOIOMMU;
	else
		group->iommu_type = VFIO_TYPE1_IOMMU;
	LIST_INSERT_HEAD(&s_vfio_container.groups, group, next);

	return 0;
}

static int
fslmc_vfio_clear_group(int vfio_group_fd)
{
	struct fslmc_vfio_group *group;
	struct fslmc_vfio_device *dev;
	int clear = 0;

	LIST_FOREACH(group, &s_vfio_container.groups, next) {
		if (group->fd == vfio_group_fd) {
			LIST_FOREACH(dev, &group->vfio_devices, next)
				LIST_REMOVE(dev, next);

			close(vfio_group_fd);
			LIST_REMOVE(group, next);
			rte_free(group);
			clear = 1;

			break;
		}
	}

	if (LIST_EMPTY(&s_vfio_container.groups)) {
		if (s_vfio_container.fd > 0)
			close(s_vfio_container.fd);

		s_vfio_container.fd = -1;
	}
	if (clear)
		return 0;

	return -ENODEV;
}

static int
fslmc_vfio_connect_container(int vfio_group_fd)
{
	struct fslmc_vfio_group *group;

	LIST_FOREACH(group, &s_vfio_container.groups, next) {
		if (group->fd == vfio_group_fd) {
			group->connected = 1;

			return 0;
		}
	}

	return -ENODEV;
}

static int
fslmc_vfio_container_connected(int vfio_group_fd)
{
	struct fslmc_vfio_group *group;

	LIST_FOREACH(group, &s_vfio_container.groups, next) {
		if (group->fd == vfio_group_fd) {
			if (group->connected)
				return 1;
		}
	}
	return 0;
}

static int
fslmc_vfio_iommu_type(int vfio_group_fd)
{
	struct fslmc_vfio_group *group;

	LIST_FOREACH(group, &s_vfio_container.groups, next) {
		if (group->fd == vfio_group_fd)
			return group->iommu_type;
	}
	return -ENODEV;
}

static int
fslmc_vfio_group_fd_by_name(const char *group_name)
{
	struct fslmc_vfio_group *group;

	LIST_FOREACH(group, &s_vfio_container.groups, next) {
		if (!strcmp(group->group_name, group_name))
			return group->fd;
	}
	return -ENODEV;
}

static int
fslmc_vfio_group_fd_by_id(int group_id)
{
	struct fslmc_vfio_group *group;

	LIST_FOREACH(group, &s_vfio_container.groups, next) {
		if (group->groupid == group_id)
			return group->fd;
	}
	return -ENODEV;
}

static int
fslmc_vfio_group_add_dev(int vfio_group_fd,
	int dev_fd, const char *name)
{
	struct fslmc_vfio_group *group;
	struct fslmc_vfio_device *dev;

	LIST_FOREACH(group, &s_vfio_container.groups, next) {
		if (group->fd == vfio_group_fd) {
			dev = rte_zmalloc(NULL,
				sizeof(struct fslmc_vfio_device), 0);
			dev->fd = dev_fd;
			rte_strscpy(dev->dev_name, name, sizeof(dev->dev_name));
			LIST_INSERT_HEAD(&group->vfio_devices, dev, next);
			return 0;
		}
	}
	return -ENODEV;
}

static int
fslmc_vfio_group_remove_dev(int vfio_group_fd,
	const char *name)
{
	struct fslmc_vfio_group *group = NULL;
	struct fslmc_vfio_device *dev;
	int removed = 0;

	LIST_FOREACH(group, &s_vfio_container.groups, next) {
		if (group->fd == vfio_group_fd)
			break;
	}

	if (group) {
		LIST_FOREACH(dev, &group->vfio_devices, next) {
			if (!strcmp(dev->dev_name, name)) {
				LIST_REMOVE(dev, next);
				removed = 1;
				break;
			}
		}
	}

	if (removed)
		return 0;

	return -ENODEV;
}

static int
fslmc_vfio_container_fd(void)
{
	return s_vfio_container.fd;
}

static int
fslmc_get_group_id(const char *group_name,
	int *groupid)
{
	int ret;

	/* get group number */
	ret = rte_vfio_get_group_num(SYSFS_FSL_MC_DEVICES,
			group_name, groupid);
	if (ret <= 0) {
		DPAA2_BUS_ERR("Unable to find %s IOMMU group", group_name);
		if (ret < 0)
			return ret;

		return -EIO;
	}

	DPAA2_BUS_DEBUG("GROUP(%s) has VFIO iommu group id = %d",
		group_name, *groupid);

	return 0;
}

static int
fslmc_vfio_open_group_fd(const char *group_name)
{
	int vfio_group_fd;
	char filename[PATH_MAX];
	struct rte_mp_msg mp_req, *mp_rep;
	struct rte_mp_reply mp_reply = {0};
	struct timespec ts = {.tv_sec = 5, .tv_nsec = 0};
	struct vfio_mp_param *p = (struct vfio_mp_param *)mp_req.param;
	int iommu_group_num, ret;

	vfio_group_fd = fslmc_vfio_group_fd_by_name(group_name);
	if (vfio_group_fd > 0)
		return vfio_group_fd;

	ret = fslmc_get_group_id(group_name, &iommu_group_num);
	if (ret)
		return ret;
	/* if primary, try to open the group */
	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		/* try regular group format */
		snprintf(filename, sizeof(filename),
			VFIO_GROUP_FMT, iommu_group_num);
		vfio_group_fd = open(filename, O_RDWR);

		goto add_vfio_group;
	}
	/* if we're in a secondary process, request group fd from the primary
	 * process via mp channel.
	 */
	p->req = SOCKET_REQ_GROUP;
	p->group_num = iommu_group_num;
	rte_strscpy(mp_req.name, FSLMC_VFIO_MP, sizeof(mp_req.name));
	mp_req.len_param = sizeof(*p);
	mp_req.num_fds = 0;

	vfio_group_fd = -1;
	if (rte_mp_request_sync(&mp_req, &mp_reply, &ts) == 0 &&
	    mp_reply.nb_received == 1) {
		mp_rep = &mp_reply.msgs[0];
		p = (struct vfio_mp_param *)mp_rep->param;
		if (p->result == SOCKET_OK && mp_rep->num_fds == 1)
			vfio_group_fd = mp_rep->fds[0];
		else if (p->result == SOCKET_NO_FD)
			DPAA2_BUS_ERR("Bad VFIO group fd");
	}

	free(mp_reply.msgs);

add_vfio_group:
	if (vfio_group_fd < 0) {
		if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
			DPAA2_BUS_ERR("Open VFIO group(%s) failed(%d)",
				filename, vfio_group_fd);
		} else {
			DPAA2_BUS_ERR("Cannot request group fd(%d)",
				vfio_group_fd);
		}
	} else {
		ret = fslmc_vfio_add_group(vfio_group_fd, iommu_group_num,
			group_name);
		if (ret)
			return ret;
	}

	return vfio_group_fd;
}

static int
fslmc_vfio_check_extensions(int vfio_container_fd)
{
	int ret;
	uint32_t idx, n_extensions = 0;
	static const int type_id[] = {RTE_VFIO_TYPE1, RTE_VFIO_SPAPR,
		RTE_VFIO_NOIOMMU};
	static const char * const type_id_nm[] = {"Type 1",
		"sPAPR", "No-IOMMU"};

	for (idx = 0; idx < RTE_DIM(type_id); idx++) {
		ret = ioctl(vfio_container_fd, VFIO_CHECK_EXTENSION,
			type_id[idx]);
		if (ret < 0) {
			DPAA2_BUS_ERR("Could not get IOMMU type, error %i (%s)",
				errno, strerror(errno));
			close(vfio_container_fd);
			return -errno;
		} else if (ret == 1) {
			/* we found a supported extension */
			n_extensions++;
		}
		DPAA2_BUS_DEBUG("IOMMU type %d (%s) is %s",
			type_id[idx], type_id_nm[idx],
			ret ? "supported" : "not supported");
	}

	/* if we didn't find any supported IOMMU types, fail */
	if (!n_extensions) {
		close(vfio_container_fd);
		return -EIO;
	}

	return 0;
}

static int
fslmc_vfio_open_container_fd(void)
{
	int ret, vfio_container_fd;
	struct rte_mp_msg mp_req, *mp_rep;
	struct rte_mp_reply mp_reply = {0};
	struct timespec ts = {.tv_sec = 5, .tv_nsec = 0};
	struct vfio_mp_param *p = (void *)mp_req.param;

	if (fslmc_vfio_container_fd() > 0)
		return fslmc_vfio_container_fd();

	/* if we're in a primary process, try to open the container */
	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		vfio_container_fd = open(VFIO_CONTAINER_PATH, O_RDWR);
		if (vfio_container_fd < 0) {
			DPAA2_BUS_ERR("Cannot open VFIO container(%s), err(%d)",
				VFIO_CONTAINER_PATH, vfio_container_fd);
			ret = vfio_container_fd;
			goto err_exit;
		}

		/* check VFIO API version */
		ret = ioctl(vfio_container_fd, VFIO_GET_API_VERSION);
		if (ret < 0) {
			DPAA2_BUS_ERR("Could not get VFIO API version(%d)",
				ret);
		} else if (ret != VFIO_API_VERSION) {
			DPAA2_BUS_ERR("Unsupported VFIO API version(%d)",
				ret);
			ret = -ENOTSUP;
		}
		if (ret < 0) {
			close(vfio_container_fd);
			goto err_exit;
		}

		ret = fslmc_vfio_check_extensions(vfio_container_fd);
		if (ret) {
			DPAA2_BUS_ERR("No supported IOMMU extensions found(%d)",
				ret);
			close(vfio_container_fd);
			goto err_exit;
		}

		goto success_exit;
	}
	/*
	 * if we're in a secondary process, request container fd from the
	 * primary process via mp channel
	 */
	p->req = SOCKET_REQ_CONTAINER;
	rte_strscpy(mp_req.name, FSLMC_VFIO_MP, sizeof(mp_req.name));
	mp_req.len_param = sizeof(*p);
	mp_req.num_fds = 0;

	vfio_container_fd = -1;
	ret = rte_mp_request_sync(&mp_req, &mp_reply, &ts);
	if (ret)
		goto err_exit;

	if (mp_reply.nb_received != 1) {
		ret = -EIO;
		goto err_exit;
	}

	mp_rep = &mp_reply.msgs[0];
	p = (void *)mp_rep->param;
	if (p->result == SOCKET_OK && mp_rep->num_fds == 1) {
		vfio_container_fd = mp_rep->fds[0];
		free(mp_reply.msgs);
	}

success_exit:
	s_vfio_container.fd = vfio_container_fd;

	return vfio_container_fd;

err_exit:
	if (mp_reply.msgs)
		free(mp_reply.msgs);
	DPAA2_BUS_ERR("Cannot request container fd err(%d)", ret);
	return ret;
}

int
fslmc_get_container_group(const char *group_name,
	int *groupid)
{
	int ret;

	if (!group_name) {
		DPAA2_BUS_ERR("No group name provided!");

		return -EINVAL;
	}
	ret = fslmc_get_group_id(group_name, groupid);
	if (ret)
		return ret;

	fslmc_vfio_set_group_name(group_name);

	return 0;
}

static int
fslmc_vfio_mp_primary(const struct rte_mp_msg *msg,
	const void *peer)
{
	int fd = -1;
	int ret;
	struct rte_mp_msg reply;
	struct vfio_mp_param *r = (void *)reply.param;
	const struct vfio_mp_param *m = (const void *)msg->param;

	if (msg->len_param != sizeof(*m)) {
		DPAA2_BUS_ERR("fslmc vfio received invalid message!");
		return -EINVAL;
	}

	memset(&reply, 0, sizeof(reply));

	switch (m->req) {
	case SOCKET_REQ_GROUP:
		r->req = SOCKET_REQ_GROUP;
		r->group_num = m->group_num;
		fd = fslmc_vfio_group_fd_by_id(m->group_num);
		if (fd < 0) {
			r->result = SOCKET_ERR;
		} else if (!fd) {
			/* if group exists but isn't bound to VFIO driver */
			r->result = SOCKET_NO_FD;
		} else {
			/* if group exists and is bound to VFIO driver */
			r->result = SOCKET_OK;
			reply.num_fds = 1;
			reply.fds[0] = fd;
		}
		break;
	case SOCKET_REQ_CONTAINER:
		r->req = SOCKET_REQ_CONTAINER;
		fd = fslmc_vfio_container_fd();
		if (fd <= 0) {
			r->result = SOCKET_ERR;
		} else {
			r->result = SOCKET_OK;
			reply.num_fds = 1;
			reply.fds[0] = fd;
		}
		break;
	default:
		DPAA2_BUS_ERR("fslmc vfio received invalid message(%08x)",
			m->req);
		return -ENOTSUP;
	}

	rte_strscpy(reply.name, FSLMC_VFIO_MP, sizeof(reply.name));
	reply.len_param = sizeof(*r);
	ret = rte_mp_reply(&reply, peer);

	return ret;
}

static int
fslmc_vfio_mp_sync_setup(void)
{
	int ret;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		ret = rte_mp_action_register(FSLMC_VFIO_MP,
			fslmc_vfio_mp_primary);
		if (ret && rte_errno != ENOTSUP)
			return ret;
	}

	return 0;
}

static int
vfio_connect_container(int vfio_container_fd,
	int vfio_group_fd)
{
	int ret;
	int iommu_type;

	if (fslmc_vfio_container_connected(vfio_group_fd)) {
		DPAA2_BUS_WARN("VFIO FD(%d) has connected to container",
			vfio_group_fd);
		return 0;
	}

	iommu_type = fslmc_vfio_iommu_type(vfio_group_fd);
	if (iommu_type < 0) {
		DPAA2_BUS_ERR("Failed to get iommu type(%d)",
			iommu_type);

		return iommu_type;
	}

	/* Check whether support for SMMU type IOMMU present or not */
	if (ioctl(vfio_container_fd, VFIO_CHECK_EXTENSION, iommu_type)) {
		/* Connect group to container */
		ret = ioctl(vfio_group_fd, VFIO_GROUP_SET_CONTAINER,
			&vfio_container_fd);
		if (ret) {
			DPAA2_BUS_ERR("Failed to setup group container");
			return -errno;
		}

		ret = ioctl(vfio_container_fd, VFIO_SET_IOMMU, iommu_type);
		if (ret) {
			DPAA2_BUS_ERR("Failed to setup VFIO iommu");
			return -errno;
		}
	} else {
		DPAA2_BUS_ERR("No supported IOMMU available");
		return -EINVAL;
	}

	return fslmc_vfio_connect_container(vfio_group_fd);
}

static int vfio_map_irq_region(void)
{
	int ret, fd;
	unsigned long *vaddr = NULL;
	struct vfio_iommu_type1_dma_map map = {
		.argsz = sizeof(map),
		.flags = VFIO_DMA_MAP_FLAG_READ | VFIO_DMA_MAP_FLAG_WRITE,
		.vaddr = 0x6030000,
		.iova = 0x6030000,
		.size = 0x1000,
	};
	const char *group_name = fslmc_vfio_get_group_name();

	fd = fslmc_vfio_group_fd_by_name(group_name);
	if (fd <= 0) {
		DPAA2_BUS_ERR("%s failed to open group fd(%d)",
			__func__, fd);
		if (fd < 0)
			return fd;
		return -rte_errno;
	}
	if (!fslmc_vfio_container_connected(fd)) {
		DPAA2_BUS_ERR("Container is not connected");
		return -EIO;
	}

	vaddr = (unsigned long *)mmap(NULL, 0x1000, PROT_WRITE |
		PROT_READ, MAP_SHARED, fd, 0x6030000);
	if (vaddr == MAP_FAILED) {
		DPAA2_BUS_INFO("Unable to map region (errno = %d)", errno);
		return -errno;
	}

	msi_intr_vaddr = (uint32_t *)((char *)(vaddr) + 64);
	map.vaddr = (unsigned long)vaddr;
	ret = ioctl(fslmc_vfio_container_fd(), VFIO_IOMMU_MAP_DMA, &map);
	if (!ret)
		return 0;

	DPAA2_BUS_ERR("Unable to map DMA address (errno = %d)", errno);
	return -errno;
}

static int fslmc_map_dma(uint64_t vaddr, rte_iova_t iovaddr, size_t len);
static int fslmc_unmap_dma(uint64_t vaddr, rte_iova_t iovaddr, size_t len);

static void
fslmc_memevent_cb(enum rte_mem_event type, const void *addr,
	size_t len, void *arg __rte_unused)
{
	struct rte_memseg_list *msl;
	struct rte_memseg *ms;
	size_t cur_len = 0, map_len = 0;
	uint64_t virt_addr;
	rte_iova_t iova_addr;
	int ret;

	msl = rte_mem_virt2memseg_list(addr);

	while (cur_len < len) {
		const void *va = RTE_PTR_ADD(addr, cur_len);

		ms = rte_mem_virt2memseg(va, msl);
		iova_addr = ms->iova;
		virt_addr = ms->addr_64;
		map_len = ms->len;

		DPAA2_BUS_DEBUG("Request for %s, va=%p, "
				"virt_addr=0x%" PRIx64 ", "
				"iova=0x%" PRIx64 ", map_len=%zu",
				type == RTE_MEM_EVENT_ALLOC ?
					"alloc" : "dealloc",
				va, virt_addr, iova_addr, map_len);

		/* iova_addr may be set to RTE_BAD_IOVA */
		if (iova_addr == RTE_BAD_IOVA) {
			DPAA2_BUS_DEBUG("Segment has invalid iova, skipping");
			cur_len += map_len;
			continue;
		}

		if (type == RTE_MEM_EVENT_ALLOC)
			ret = fslmc_map_dma(virt_addr, iova_addr, map_len);
		else
			ret = fslmc_unmap_dma(virt_addr, iova_addr, map_len);

		if (ret != 0) {
			DPAA2_BUS_ERR("DMA Mapping/Unmapping failed. "
					"Map=%d, addr=%p, len=%zu, err:(%d)",
					type, va, map_len, ret);
			return;
		}

		cur_len += map_len;
	}

	if (type == RTE_MEM_EVENT_ALLOC)
		DPAA2_BUS_DEBUG("Total Mapped: addr=%p, len=%zu",
				addr, len);
	else
		DPAA2_BUS_DEBUG("Total Unmapped: addr=%p, len=%zu",
				addr, len);
}

static int
fslmc_map_dma(uint64_t vaddr, rte_iova_t iovaddr,
	size_t len)
{
	struct vfio_iommu_type1_dma_map dma_map = {
		.argsz = sizeof(struct vfio_iommu_type1_dma_map),
		.flags = VFIO_DMA_MAP_FLAG_READ | VFIO_DMA_MAP_FLAG_WRITE,
	};
	int ret, fd;
	const char *group_name = fslmc_vfio_get_group_name();

	fd = fslmc_vfio_group_fd_by_name(group_name);
	if (fd <= 0) {
		DPAA2_BUS_ERR("%s failed to open group fd(%d)",
			__func__, fd);
		if (fd < 0)
			return fd;
		return -rte_errno;
	}
	if (fslmc_vfio_iommu_type(fd) == RTE_VFIO_NOIOMMU) {
		DPAA2_BUS_DEBUG("Running in NOIOMMU mode");
		return 0;
	}

	dma_map.size = len;
	dma_map.vaddr = vaddr;
	dma_map.iova = iovaddr;

#ifndef RTE_LIBRTE_DPAA2_USE_PHYS_IOVA
	if (vaddr != iovaddr) {
		DPAA2_BUS_WARN("vaddr(0x%"PRIx64") != iovaddr(0x%"PRIx64")",
			vaddr, iovaddr);
	}
#endif

	/* SET DMA MAP for IOMMU */
	if (!fslmc_vfio_container_connected(fd)) {
		DPAA2_BUS_ERR("Container is not connected ");
		return -EIO;
	}

	DPAA2_BUS_DEBUG("--> Map address: 0x%"PRIx64", size: %"PRIu64"",
			(uint64_t)dma_map.vaddr, (uint64_t)dma_map.size);
	ret = ioctl(fslmc_vfio_container_fd(), VFIO_IOMMU_MAP_DMA,
		&dma_map);
	if (ret) {
		DPAA2_BUS_ERR("VFIO_IOMMU_MAP_DMA API(errno = %d)",
				errno);
		return ret;
	}

	return 0;
}

static int
fslmc_unmap_dma(uint64_t vaddr, uint64_t iovaddr __rte_unused, size_t len)
{
	struct vfio_iommu_type1_dma_unmap dma_unmap = {
		.argsz = sizeof(struct vfio_iommu_type1_dma_unmap),
		.flags = 0,
	};
	int ret, fd;
	const char *group_name = fslmc_vfio_get_group_name();

	fd = fslmc_vfio_group_fd_by_name(group_name);
	if (fd <= 0) {
		DPAA2_BUS_ERR("%s failed to open group fd(%d)",
			__func__, fd);
		if (fd < 0)
			return fd;
		return -rte_errno;
	}
	if (fslmc_vfio_iommu_type(fd) == RTE_VFIO_NOIOMMU) {
		DPAA2_BUS_DEBUG("Running in NOIOMMU mode");
		return 0;
	}

	dma_unmap.size = len;
	dma_unmap.iova = vaddr;

	/* SET DMA MAP for IOMMU */
	if (!fslmc_vfio_container_connected(fd)) {
		DPAA2_BUS_ERR("Container is not connected ");
		return -EIO;
	}

	DPAA2_BUS_DEBUG("--> Unmap address: 0x%"PRIx64", size: %"PRIu64"",
			(uint64_t)dma_unmap.iova, (uint64_t)dma_unmap.size);
	ret = ioctl(fslmc_vfio_container_fd(), VFIO_IOMMU_UNMAP_DMA,
		&dma_unmap);
	if (ret) {
		DPAA2_BUS_ERR("VFIO_IOMMU_UNMAP_DMA API(errno = %d)",
				errno);
		return -1;
	}

	return 0;
}

static int
fslmc_dmamap_seg(const struct rte_memseg_list *msl __rte_unused,
		const struct rte_memseg *ms, void *arg)
{
	int *n_segs = arg;
	int ret;

	/* if IOVA address is invalid, skip */
	if (ms->iova == RTE_BAD_IOVA)
		return 0;

	ret = fslmc_map_dma(ms->addr_64, ms->iova, ms->len);
	if (ret)
		DPAA2_BUS_ERR("Unable to VFIO map (addr=%p, len=%zu)",
				ms->addr, ms->len);
	else
		(*n_segs)++;

	return ret;
}

int
rte_fslmc_vfio_mem_dmamap(uint64_t vaddr, uint64_t iova, uint64_t size)
{
	return fslmc_map_dma(vaddr, iova, size);
}

int
rte_fslmc_vfio_mem_dmaunmap(uint64_t iova, uint64_t size)
{
	return fslmc_unmap_dma(iova, 0, size);
}

int rte_fslmc_vfio_dmamap(void)
{
	int i = 0, ret;

	/* Lock before parsing and registering callback to memory subsystem */
	rte_mcfg_mem_read_lock();

	if (rte_memseg_walk(fslmc_dmamap_seg, &i) < 0) {
		rte_mcfg_mem_read_unlock();
		return -1;
	}

	ret = rte_mem_event_callback_register("fslmc_memevent_clb",
			fslmc_memevent_cb, NULL);
	if (ret && rte_errno == ENOTSUP)
		DPAA2_BUS_DEBUG("Memory event callbacks not supported");
	else if (ret)
		DPAA2_BUS_DEBUG("Unable to install memory handler");
	else
		DPAA2_BUS_DEBUG("Installed memory callback handler");

	DPAA2_BUS_DEBUG("Total %d segments found.", i);

	/* TODO - This is a W.A. as VFIO currently does not add the mapping of
	 * the interrupt region to SMMU. This should be removed once the
	 * support is added in the Kernel.
	 */
	vfio_map_irq_region();

	/* Existing segments have been mapped and memory callback for hotplug
	 * has been installed.
	 */
	rte_mcfg_mem_read_unlock();

	return 0;
}

static int
fslmc_vfio_setup_device(const char *dev_addr,
	int *vfio_dev_fd, struct vfio_device_info *device_info)
{
	struct vfio_group_status group_status = {
			.argsz = sizeof(group_status)
	};
	int vfio_group_fd, ret;
	const char *group_name = fslmc_vfio_get_group_name();

	vfio_group_fd = fslmc_vfio_group_fd_by_name(group_name);
	if (!fslmc_vfio_container_connected(vfio_group_fd)) {
		DPAA2_BUS_ERR("Container is not connected");
		return -EIO;
	}

	/* get a file descriptor for the device */
	*vfio_dev_fd = ioctl(vfio_group_fd, VFIO_GROUP_GET_DEVICE_FD, dev_addr);
	if (*vfio_dev_fd < 0) {
		/* if we cannot get a device fd, this implies a problem with
		 * the VFIO group or the container not having IOMMU configured.
		 */

		DPAA2_BUS_ERR("Getting a vfio_dev_fd for %s from %s failed",
			dev_addr, group_name);
		return -EIO;
	}

	/* test and setup the device */
	ret = ioctl(*vfio_dev_fd, VFIO_DEVICE_GET_INFO, device_info);
	if (ret) {
		DPAA2_BUS_ERR("%s cannot get device info err(%d)(%s)",
			dev_addr, errno, strerror(errno));
		return ret;
	}

	return fslmc_vfio_group_add_dev(vfio_group_fd, *vfio_dev_fd,
			dev_addr);
}

static intptr_t vfio_map_mcp_obj(const char *mcp_obj)
{
	intptr_t v_addr = (intptr_t)MAP_FAILED;
	int32_t ret, mc_fd;
	struct vfio_group_status status = { .argsz = sizeof(status) };

	struct vfio_device_info d_info = { .argsz = sizeof(d_info) };
	struct vfio_region_info reg_info = { .argsz = sizeof(reg_info) };

	fslmc_vfio_setup_device(mcp_obj, &mc_fd, &d_info);

	/* getting device region info*/
	ret = ioctl(mc_fd, VFIO_DEVICE_GET_REGION_INFO, &reg_info);
	if (ret < 0) {
		DPAA2_BUS_ERR("Error in VFIO getting REGION_INFO");
		goto MC_FAILURE;
	}

	v_addr = (size_t)mmap(NULL, reg_info.size,
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
	int *fd_ptr, vfio_dev_fd;

	len = sizeof(irq_set_buf);

	irq_set = (struct vfio_irq_set *)irq_set_buf;
	irq_set->argsz = len;
	irq_set->count = 1;
	irq_set->flags =
		VFIO_IRQ_SET_DATA_EVENTFD | VFIO_IRQ_SET_ACTION_TRIGGER;
	irq_set->index = index;
	irq_set->start = 0;
	fd_ptr = (int *)&irq_set->data;
	*fd_ptr = rte_intr_fd_get(intr_handle);

	vfio_dev_fd = rte_intr_dev_fd_get(intr_handle);
	ret = ioctl(vfio_dev_fd, VFIO_DEVICE_SET_IRQS, irq_set);
	if (ret) {
		DPAA2_BUS_ERR("Error:dpaa2 SET IRQs fd=%d, err = %d(%s)",
			      rte_intr_fd_get(intr_handle), errno,
			      strerror(errno));
		return ret;
	}

	return ret;
}

int rte_dpaa2_intr_disable(struct rte_intr_handle *intr_handle, int index)
{
	struct vfio_irq_set *irq_set;
	char irq_set_buf[IRQ_SET_BUF_LEN];
	int len, ret, vfio_dev_fd;

	len = sizeof(struct vfio_irq_set);

	irq_set = (struct vfio_irq_set *)irq_set_buf;
	irq_set->argsz = len;
	irq_set->flags = VFIO_IRQ_SET_DATA_NONE | VFIO_IRQ_SET_ACTION_TRIGGER;
	irq_set->index = index;
	irq_set->start = 0;
	irq_set->count = 0;

	vfio_dev_fd = rte_intr_dev_fd_get(intr_handle);
	ret = ioctl(vfio_dev_fd, VFIO_DEVICE_SET_IRQS, irq_set);
	if (ret)
		DPAA2_BUS_ERR(
			"Error disabling dpaa2 interrupts for fd %d",
			rte_intr_fd_get(intr_handle));

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
			DPAA2_BUS_ERR("Cannot get IRQ(%d) info, error %i (%s)",
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
			DPAA2_BUS_ERR("Cannot set up eventfd, error %i (%s)",
				      errno, strerror(errno));
			return -1;
		}

		if (rte_intr_fd_set(intr_handle, fd))
			return -rte_errno;

		if (rte_intr_type_set(intr_handle, RTE_INTR_HANDLE_VFIO_MSI))
			return -rte_errno;

		if (rte_intr_dev_fd_set(intr_handle, vfio_dev_fd))
			return -rte_errno;

		return 0;
	}

	/* if we're here, we haven't found a suitable interrupt vector */
	return -1;
}

static void
fslmc_close_iodevices(struct rte_dpaa2_device *dev,
	int vfio_fd)
{
	struct rte_dpaa2_object *object = NULL;
	struct rte_dpaa2_driver *drv;
	int ret, probe_all;

	switch (dev->dev_type) {
	case DPAA2_IO:
	case DPAA2_CON:
	case DPAA2_CI:
	case DPAA2_BPOOL:
	case DPAA2_MUX:
		TAILQ_FOREACH(object, &dpaa2_obj_list, next) {
			if (dev->dev_type == object->dev_type)
				object->close(dev->object_id);
			else
				continue;
		}
		break;
	case DPAA2_ETH:
	case DPAA2_CRYPTO:
	case DPAA2_QDMA:
		probe_all = rte_fslmc_bus.bus.conf.scan_mode !=
			    RTE_BUS_SCAN_ALLOWLIST;
		TAILQ_FOREACH(drv, &rte_fslmc_bus.driver_list, next) {
			if (drv->drv_type != dev->dev_type)
				continue;
			if (rte_dev_is_probed(&dev->device))
				continue;
			if (probe_all ||
			    (dev->device.devargs &&
			     dev->device.devargs->policy ==
			     RTE_DEV_ALLOWED)) {
				ret = drv->remove(dev);
				if (ret)
					DPAA2_BUS_ERR("Unable to remove");
			}
		}
		break;
	default:
		break;
	}

	ret = fslmc_vfio_group_remove_dev(vfio_fd, dev->device.name);
	if (ret) {
		DPAA2_BUS_ERR("Failed to remove %s from vfio",
			dev->device.name);
	}
	DPAA2_BUS_LOG(DEBUG, "Device (%s) Closed",
		      dev->device.name);
}

/*
 * fslmc_process_iodevices for processing only IO (ETH, CRYPTO, and possibly
 * EVENT) devices.
 */
static int
fslmc_process_iodevices(struct rte_dpaa2_device *dev)
{
	int dev_fd, ret;
	struct vfio_device_info device_info = { .argsz = sizeof(device_info) };
	struct rte_dpaa2_object *object = NULL;

	ret = fslmc_vfio_setup_device(dev->device.name, &dev_fd,
			&device_info);
	if (ret)
		return ret;

	switch (dev->dev_type) {
	case DPAA2_ETH:
		ret = rte_dpaa2_vfio_setup_intr(dev->intr_handle, dev_fd,
				device_info.num_irqs);
		if (ret)
			return ret;
		break;
	case DPAA2_CON:
	case DPAA2_IO:
	case DPAA2_CI:
	case DPAA2_BPOOL:
	case DPAA2_DPRTC:
	case DPAA2_MUX:
	case DPAA2_DPRC:
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

	DPAA2_BUS_LOG(DEBUG, "Device (%s) abstracted from VFIO",
		      dev->device.name);
	return 0;
}

static int
fslmc_process_mcp(struct rte_dpaa2_device *dev)
{
	int ret;
	intptr_t v_addr;
	struct fsl_mc_io dpmng  = {0};
	struct mc_version mc_ver_info = {0};

	rte_mcp_ptr_list = malloc(sizeof(void *) * (MC_PORTAL_INDEX + 1));
	if (!rte_mcp_ptr_list) {
		DPAA2_BUS_ERR("Unable to allocate MC portal memory");
		ret = -ENOMEM;
		goto cleanup;
	}

	v_addr = vfio_map_mcp_obj(dev->device.name);
	if (v_addr == (intptr_t)MAP_FAILED) {
		DPAA2_BUS_ERR("Error mapping region (errno = %d)", errno);
		ret = -1;
		goto cleanup;
	}

	/* check the MC version compatibility */
	dpmng.regs = (void *)v_addr;

	/* In case of secondary processes, MC version check is no longer
	 * required.
	 */
	if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		rte_mcp_ptr_list[MC_PORTAL_INDEX] = (void *)v_addr;
		return 0;
	}

	if (mc_get_version(&dpmng, CMD_PRI_LOW, &mc_ver_info)) {
		DPAA2_BUS_ERR("Unable to obtain MC version");
		ret = -1;
		goto cleanup;
	}

	if ((mc_ver_info.major != MC_VER_MAJOR) ||
	    (mc_ver_info.minor < MC_VER_MINOR)) {
		DPAA2_BUS_ERR("DPAA2 MC version not compatible!"
			      " Expected %d.%d.x, Detected %d.%d.%d",
			      MC_VER_MAJOR, MC_VER_MINOR,
			      mc_ver_info.major, mc_ver_info.minor,
			      mc_ver_info.revision);
		ret = -1;
		goto cleanup;
	}
	rte_mcp_ptr_list[MC_PORTAL_INDEX] = (void *)v_addr;

	return 0;

cleanup:
	if (rte_mcp_ptr_list) {
		free(rte_mcp_ptr_list);
		rte_mcp_ptr_list = NULL;
	}

	return ret;
}

int
fslmc_vfio_close_group(void)
{
	struct rte_dpaa2_device *dev, *dev_temp;
	int vfio_group_fd;
	const char *group_name = fslmc_vfio_get_group_name();

	vfio_group_fd = fslmc_vfio_group_fd_by_name(group_name);

	RTE_TAILQ_FOREACH_SAFE(dev, &rte_fslmc_bus.device_list, next, dev_temp) {
		if (dev->device.devargs &&
		    dev->device.devargs->policy == RTE_DEV_BLOCKED) {
			DPAA2_BUS_LOG(DEBUG, "%s Blacklisted, skipping",
				      dev->device.name);
			TAILQ_REMOVE(&rte_fslmc_bus.device_list, dev, next);
				continue;
		}
		switch (dev->dev_type) {
		case DPAA2_ETH:
		case DPAA2_CRYPTO:
		case DPAA2_QDMA:
		case DPAA2_IO:
			fslmc_close_iodevices(dev, vfio_group_fd);
			break;
		case DPAA2_CON:
		case DPAA2_CI:
		case DPAA2_BPOOL:
		case DPAA2_MUX:
			if (rte_eal_process_type() == RTE_PROC_SECONDARY)
				continue;

			fslmc_close_iodevices(dev, vfio_group_fd);
			break;
		case DPAA2_DPRTC:
		default:
			DPAA2_BUS_DEBUG("Device cannot be closed: Not supported (%s)",
					dev->device.name);
		}
	}

	fslmc_vfio_clear_group(vfio_group_fd);

	return 0;
}

int
fslmc_vfio_process_group(void)
{
	int ret;
	int found_mportal = 0;
	struct rte_dpaa2_device *dev, *dev_temp;
	bool is_dpmcp_in_blocklist = false, is_dpio_in_blocklist = false;
	int dpmcp_count = 0, dpio_count = 0, current_device;

	RTE_TAILQ_FOREACH_SAFE(dev, &rte_fslmc_bus.device_list, next,
		dev_temp) {
		if (dev->dev_type == DPAA2_MPORTAL) {
			dpmcp_count++;
			if (dev->device.devargs &&
			    dev->device.devargs->policy == RTE_DEV_BLOCKED)
				is_dpmcp_in_blocklist = true;
		}
		if (dev->dev_type == DPAA2_IO) {
			dpio_count++;
			if (dev->device.devargs &&
			    dev->device.devargs->policy == RTE_DEV_BLOCKED)
				is_dpio_in_blocklist = true;
		}
	}

	/* Search the MCP as that should be initialized first. */
	current_device = 0;
	RTE_TAILQ_FOREACH_SAFE(dev, &rte_fslmc_bus.device_list, next,
		dev_temp) {
		if (dev->dev_type == DPAA2_MPORTAL) {
			current_device++;
			if (dev->device.devargs &&
			    dev->device.devargs->policy == RTE_DEV_BLOCKED) {
				DPAA2_BUS_LOG(DEBUG, "%s Blocked, skipping",
					      dev->device.name);
				TAILQ_REMOVE(&rte_fslmc_bus.device_list,
						dev, next);
				continue;
			}

			if (rte_eal_process_type() == RTE_PROC_SECONDARY &&
			    !is_dpmcp_in_blocklist) {
				if (dpmcp_count == 1 ||
				    current_device != dpmcp_count) {
					TAILQ_REMOVE(&rte_fslmc_bus.device_list,
						     dev, next);
					continue;
				}
			}

			if (!found_mportal) {
				ret = fslmc_process_mcp(dev);
				if (ret) {
					DPAA2_BUS_ERR("Unable to map MC Portal");
					return -1;
				}
				found_mportal = 1;
			}

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
		DPAA2_BUS_ERR("No MC Portal device found. Not continuing");
		return -1;
	}

	/* Search for DPRC device next as it updates endpoint of
	 * other devices.
	 */
	current_device = 0;
	RTE_TAILQ_FOREACH_SAFE(dev, &rte_fslmc_bus.device_list, next, dev_temp) {
		if (dev->dev_type == DPAA2_DPRC) {
			ret = fslmc_process_iodevices(dev);
			if (ret) {
				DPAA2_BUS_ERR("Unable to process dprc");
				return -1;
			}
			TAILQ_REMOVE(&rte_fslmc_bus.device_list, dev, next);
		}
	}

	current_device = 0;
	RTE_TAILQ_FOREACH_SAFE(dev, &rte_fslmc_bus.device_list, next,
		dev_temp) {
		if (dev->dev_type == DPAA2_IO)
			current_device++;
		if (dev->device.devargs &&
		    dev->device.devargs->policy == RTE_DEV_BLOCKED) {
			DPAA2_BUS_LOG(DEBUG, "%s Blocked, skipping",
				      dev->device.name);
			TAILQ_REMOVE(&rte_fslmc_bus.device_list, dev, next);
			continue;
		}
		if (rte_eal_process_type() == RTE_PROC_SECONDARY &&
		    dev->dev_type != DPAA2_ETH &&
		    dev->dev_type != DPAA2_CRYPTO &&
		    dev->dev_type != DPAA2_QDMA &&
		    dev->dev_type != DPAA2_IO) {
			TAILQ_REMOVE(&rte_fslmc_bus.device_list, dev, next);
			continue;
		}
		switch (dev->dev_type) {
		case DPAA2_ETH:
		case DPAA2_CRYPTO:
		case DPAA2_QDMA:
			ret = fslmc_process_iodevices(dev);
			if (ret) {
				DPAA2_BUS_DEBUG("Dev (%s) init failed",
						dev->device.name);
				return ret;
			}
			break;
		case DPAA2_CON:
		case DPAA2_CI:
		case DPAA2_BPOOL:
		case DPAA2_DPRTC:
		case DPAA2_MUX:
			/* IN case of secondary processes, all control objects
			 * like dpbp, dpcon, dpci are not initialized/required
			 * - all of these are assumed to be initialized and made
			 *   available by primary.
			 */
			if (rte_eal_process_type() == RTE_PROC_SECONDARY)
				continue;

			/* Call the object creation routine and remove the
			 * device entry from device list
			 */
			ret = fslmc_process_iodevices(dev);
			if (ret) {
				DPAA2_BUS_DEBUG("Dev (%s) init failed",
						dev->device.name);
				return -1;
			}

			break;
		case DPAA2_IO:
			if (!is_dpio_in_blocklist && dpio_count > 1) {
				if (rte_eal_process_type() == RTE_PROC_SECONDARY
				    && current_device != dpio_count) {
					TAILQ_REMOVE(&rte_fslmc_bus.device_list,
						     dev, next);
					break;
				}
				if (rte_eal_process_type() == RTE_PROC_PRIMARY
				    && current_device == dpio_count) {
					TAILQ_REMOVE(&rte_fslmc_bus.device_list,
						     dev, next);
					break;
				}
			}

			ret = fslmc_process_iodevices(dev);
			if (ret) {
				DPAA2_BUS_DEBUG("Dev (%s) init failed",
						dev->device.name);
				return -1;
			}

			break;
		case DPAA2_UNKNOWN:
		default:
			/* Unknown - ignore */
			DPAA2_BUS_DEBUG("Found unknown device (%s)",
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
	int vfio_group_fd, vfio_container_fd, ret;
	struct vfio_group_status status = { .argsz = sizeof(status) };
	const char *group_name = fslmc_vfio_get_group_name();

	/* MC VFIO setup entry */
	vfio_container_fd = fslmc_vfio_container_fd();
	if (vfio_container_fd <= 0) {
		vfio_container_fd = fslmc_vfio_open_container_fd();
		if (vfio_container_fd < 0) {
			DPAA2_BUS_ERR("Failed to create MC VFIO container");
			return vfio_container_fd;
		}
	}

	if (!group_name) {
		DPAA2_BUS_DEBUG("DPAA2: DPRC not available");
		return -EINVAL;
	}

	vfio_group_fd = fslmc_vfio_group_fd_by_name(group_name);
	if (vfio_group_fd < 0) {
		vfio_group_fd = fslmc_vfio_open_group_fd(group_name);
		if (vfio_group_fd < 0) {
			DPAA2_BUS_ERR("open group name(%s) failed(%d)",
				group_name, vfio_group_fd);
			return -rte_errno;
		}
	}

	/* Check group viability */
	ret = ioctl(vfio_group_fd, VFIO_GROUP_GET_STATUS, &status);
	if (ret) {
		DPAA2_BUS_ERR("VFIO(%s:fd=%d) error getting group status(%d)",
			group_name, vfio_group_fd, ret);
		fslmc_vfio_clear_group(vfio_group_fd);
		return ret;
	}

	if (!(status.flags & VFIO_GROUP_FLAGS_VIABLE)) {
		DPAA2_BUS_ERR("VFIO group not viable");
		fslmc_vfio_clear_group(vfio_group_fd);
		return -EPERM;
	}

	/* check if group does not have a container yet */
	if (!(status.flags & VFIO_GROUP_FLAGS_CONTAINER_SET)) {
		/* Now connect this IOMMU group to given container */
		ret = vfio_connect_container(vfio_container_fd,
			vfio_group_fd);
	} else {
		/* Here is supposed in secondary process,
		 * group has been set to container in primary process.
		 */
		if (rte_eal_process_type() == RTE_PROC_PRIMARY)
			DPAA2_BUS_WARN("This group has been set container?");
		ret = fslmc_vfio_connect_container(vfio_group_fd);
	}
	if (ret) {
		DPAA2_BUS_ERR("vfio group connect failed(%d)", ret);
		fslmc_vfio_clear_group(vfio_group_fd);
		return ret;
	}

	/* Get Device information */
	ret = ioctl(vfio_group_fd, VFIO_GROUP_GET_DEVICE_FD, group_name);
	if (ret < 0) {
		DPAA2_BUS_ERR("Error getting device %s fd", group_name);
		fslmc_vfio_clear_group(vfio_group_fd);
		return ret;
	}

	ret = fslmc_vfio_mp_sync_setup();
	if (ret) {
		DPAA2_BUS_ERR("VFIO MP sync setup failed!");
		fslmc_vfio_clear_group(vfio_group_fd);
		return ret;
	}

	DPAA2_BUS_DEBUG("VFIO GROUP FD is %d", vfio_group_fd);

	return 0;
}
