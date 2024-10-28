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
#include <ctype.h>
#include <linux/vfio.h>

#include <eal_export.h>
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
#include <rte_vfio.h>
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
static const char *fslmc_group; /* dprc.x*/
static void *(*rte_mcp_ptr_list);

struct fslmc_dmaseg {
	uint64_t vaddr;
	uint64_t iova;
	uint64_t size;

	TAILQ_ENTRY(fslmc_dmaseg) next;
};

TAILQ_HEAD(fslmc_dmaseg_list, fslmc_dmaseg);

struct fslmc_dmaseg_list fslmc_memsegs =
		TAILQ_HEAD_INITIALIZER(fslmc_memsegs);
struct fslmc_dmaseg_list fslmc_iosegs =
		TAILQ_HEAD_INITIALIZER(fslmc_iosegs);

static uint64_t fslmc_mem_va2iova = RTE_BAD_IOVA;
static int fslmc_mem_map_num;

struct fslmc_mem_param {
	struct vfio_mp_param mp_param;
	struct fslmc_dmaseg_list memsegs;
	struct fslmc_dmaseg_list iosegs;
	uint64_t mem_va2iova;
	int mem_map_num;
};

enum {
	FSLMC_VFIO_SOCKET_REQ_CONTAINER = 0x100,
	FSLMC_VFIO_SOCKET_REQ_GROUP,
	FSLMC_VFIO_SOCKET_REQ_MEM
};

RTE_EXPORT_INTERNAL_SYMBOL(dpaa2_get_mcp_ptr)
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

static uint64_t
fslmc_io_virt2phy(const void *virtaddr)
{
	FILE *fp = fopen("/proc/self/maps", "r");
	char *line = NULL;
	size_t linesz;
	uint64_t start, end, phy;
	const uint64_t va = (const uint64_t)virtaddr;
	char tmp[1024];
	int ret;

	if (!fp)
		return RTE_BAD_IOVA;
	while (getdelim(&line, &linesz, '\n', fp) > 0) {
		char *ptr = line;
		int n;

		/** Parse virtual address range.*/
		n = 0;
		while (*ptr && !isspace(*ptr)) {
			tmp[n] = *ptr;
			ptr++;
			n++;
		}
		tmp[n] = 0;
		ret = sscanf(tmp, "%" SCNx64 "-%" SCNx64, &start, &end);
		if (ret != 2)
			continue;
		if (va < start || va >= end)
			continue;

		/** This virtual address is in this segment.*/
		while (*ptr == ' ' || *ptr == 'r' ||
			*ptr == 'w' || *ptr == 's' ||
			*ptr == 'p' || *ptr == 'x' ||
			*ptr == '-')
			ptr++;

		/** Extract phy address*/
		n = 0;
		while (*ptr && !isspace(*ptr)) {
			tmp[n] = *ptr;
			ptr++;
			n++;
		}
		tmp[n] = 0;
		phy = strtoul(tmp, 0, 16);
		if (!phy)
			continue;

		fclose(fp);
		return phy + va - start;
	}

	fclose(fp);
	return RTE_BAD_IOVA;
}

/*register a fslmc bus based dpaa2 driver */
RTE_EXPORT_INTERNAL_SYMBOL(rte_fslmc_object_register)
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
		group->iommu_type = VFIO_NOIOMMU_IOMMU;
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
		DPAA2_BUS_ERR("Find %s IOMMU group", group_name);
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
		snprintf(filename, sizeof(filename), RTE_VFIO_GROUP_FMT, iommu_group_num);
		vfio_group_fd = open(filename, O_RDWR);

		goto add_vfio_group;
	}
	/* if we're in a secondary process, request group fd from the primary
	 * process via mp channel.
	 */
	p->req = FSLMC_VFIO_SOCKET_REQ_GROUP;
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
		if (ret) {
			close(vfio_group_fd);
			return ret;
		}
	}

	return vfio_group_fd;
}

static int
fslmc_vfio_check_extensions(int vfio_container_fd)
{
	int ret;
	uint32_t idx, n_extensions = 0;
	static const int type_id[] = {VFIO_TYPE1_IOMMU, VFIO_SPAPR_TCE_v2_IOMMU,
		VFIO_NOIOMMU_IOMMU};
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
		vfio_container_fd = open(RTE_VFIO_CONTAINER_PATH, O_RDWR);
		if (vfio_container_fd < 0) {
			DPAA2_BUS_ERR("Open VFIO container(%s), err(%d)",
				RTE_VFIO_CONTAINER_PATH, vfio_container_fd);
			ret = vfio_container_fd;
			goto err_exit;
		}

		/* check VFIO API version */
		ret = ioctl(vfio_container_fd, VFIO_GET_API_VERSION);
		if (ret < 0) {
			DPAA2_BUS_ERR("Get VFIO API version(%d)",
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
			DPAA2_BUS_ERR("Unsupported IOMMU extensions found(%d)",
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
	p->req = FSLMC_VFIO_SOCKET_REQ_CONTAINER;
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
	free(mp_reply.msgs);
	DPAA2_BUS_ERR("Open container fd err(%d)", ret);
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
	struct fslmc_mem_param *map;

	if (msg->len_param != sizeof(*m)) {
		DPAA2_BUS_ERR("Invalid msg size(%d) for req(%d)",
			msg->len_param, m->req);
		return -EINVAL;
	}

	memset(&reply, 0, sizeof(reply));

	switch (m->req) {
	case FSLMC_VFIO_SOCKET_REQ_GROUP:
		r->req = FSLMC_VFIO_SOCKET_REQ_GROUP;
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
		reply.len_param = sizeof(*r);
		break;
	case FSLMC_VFIO_SOCKET_REQ_CONTAINER:
		r->req = FSLMC_VFIO_SOCKET_REQ_CONTAINER;
		fd = fslmc_vfio_container_fd();
		if (fd <= 0) {
			r->result = SOCKET_ERR;
		} else {
			r->result = SOCKET_OK;
			reply.num_fds = 1;
			reply.fds[0] = fd;
		}
		reply.len_param = sizeof(*r);
		break;
	case FSLMC_VFIO_SOCKET_REQ_MEM:
		map = (void *)reply.param;
		r = &map->mp_param;
		r->req = FSLMC_VFIO_SOCKET_REQ_MEM;
		r->result = SOCKET_OK;
		map->memsegs = fslmc_memsegs;
		map->iosegs = fslmc_iosegs;
		map->mem_va2iova = fslmc_mem_va2iova;
		map->mem_map_num = fslmc_mem_map_num;
		reply.len_param = sizeof(struct fslmc_mem_param);
		break;
	default:
		DPAA2_BUS_ERR("VFIO received invalid message(%08x)",
			m->req);
		return -ENOTSUP;
	}

	rte_strscpy(reply.name, FSLMC_VFIO_MP, sizeof(reply.name));
	ret = rte_mp_reply(&reply, peer);

	return ret;
}

static int
fslmc_vfio_mp_sync_mem_req(void)
{
	struct rte_mp_msg mp_req, *mp_rep;
	struct rte_mp_reply mp_reply = {0};
	struct timespec ts = {.tv_sec = 5, .tv_nsec = 0};
	int ret = 0;
	struct vfio_mp_param *mp_param;
	struct fslmc_mem_param *mem_rsp;

	mp_param = (void *)mp_req.param;
	memset(&mp_req, 0, sizeof(struct rte_mp_msg));
	mp_param->req = FSLMC_VFIO_SOCKET_REQ_MEM;
	rte_strscpy(mp_req.name, FSLMC_VFIO_MP, sizeof(mp_req.name));
	mp_req.len_param = sizeof(struct vfio_mp_param);
	if (rte_mp_request_sync(&mp_req, &mp_reply, &ts) == 0 &&
		mp_reply.nb_received == 1) {
		mp_rep = &mp_reply.msgs[0];
		mem_rsp = (struct fslmc_mem_param *)mp_rep->param;
		if (mem_rsp->mp_param.result == SOCKET_OK) {
			fslmc_memsegs = mem_rsp->memsegs;
			fslmc_mem_va2iova = mem_rsp->mem_va2iova;
			fslmc_mem_map_num = mem_rsp->mem_map_num;
		} else {
			DPAA2_BUS_ERR("Bad MEM SEG");
			ret = -EINVAL;
		}
	} else {
		ret = -EINVAL;
	}
	free(mp_reply.msgs);

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
	} else {
		ret = fslmc_vfio_mp_sync_mem_req();
		if (ret)
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
		DPAA2_BUS_ERR("Get iommu type(%d)", iommu_type);

		return iommu_type;
	}

	/* Check whether support for SMMU type IOMMU present or not */
	ret = ioctl(vfio_container_fd, VFIO_CHECK_EXTENSION, iommu_type);
	if (ret <= 0) {
		DPAA2_BUS_ERR("Unsupported IOMMU type(%d) ret(%d), err(%d)",
			iommu_type, ret, -errno);
		return -EINVAL;
	}

	ret = ioctl(vfio_group_fd, VFIO_GROUP_SET_CONTAINER,
			&vfio_container_fd);
	if (ret) {
		DPAA2_BUS_ERR("Set group container ret(%d), err(%d)",
			ret, -errno);

		return ret;
	}

	ret = ioctl(vfio_container_fd, VFIO_SET_IOMMU, iommu_type);
	if (ret) {
		DPAA2_BUS_ERR("Set iommu ret(%d), err(%d)",
			ret, -errno);

		return ret;
	}

	return fslmc_vfio_connect_container(vfio_group_fd);
}

static int
fslmc_map_dma(uint64_t vaddr, rte_iova_t iovaddr, size_t len)
{
	struct vfio_iommu_type1_dma_map dma_map = {
		.argsz = sizeof(struct vfio_iommu_type1_dma_map),
		.flags = VFIO_DMA_MAP_FLAG_READ | VFIO_DMA_MAP_FLAG_WRITE,
	};
	int ret, fd, is_io = 0;
	const char *group_name = fslmc_vfio_get_group_name();
	struct fslmc_dmaseg *dmaseg = NULL;
	uint64_t phy = 0;

	if (rte_eal_iova_mode() == RTE_IOVA_VA) {
		if (vaddr != iovaddr) {
			DPAA2_BUS_ERR("IOVA:VA(%" PRIx64 " : %" PRIx64 ") %s",
				iovaddr, vaddr,
				"should be 1:1 for VA mode");

			return -EINVAL;
		}
	}

	phy = rte_mem_virt2phy((const void *)(uintptr_t)vaddr);
	if (phy == RTE_BAD_IOVA) {
		phy = fslmc_io_virt2phy((const void *)(uintptr_t)vaddr);
		if (phy == RTE_BAD_IOVA)
			return -ENOMEM;
		is_io = 1;
	} else if (fslmc_mem_va2iova != RTE_BAD_IOVA &&
		fslmc_mem_va2iova != (iovaddr - vaddr)) {
		DPAA2_BUS_WARN("Multiple MEM PA<->VA conversions.");
	}
	DPAA2_BUS_DEBUG("%s(%zu): VA(%" PRIx64 "):IOVA(%" PRIx64 "):PHY(%" PRIx64 ")",
		is_io ? "DMA IO map size" : "DMA MEM map size",
		len, vaddr, iovaddr, phy);

	if (is_io)
		goto io_mapping_check;

	TAILQ_FOREACH(dmaseg, &fslmc_memsegs, next) {
		if (!((vaddr + len) <= dmaseg->vaddr ||
			(dmaseg->vaddr + dmaseg->size) <= vaddr)) {
			DPAA2_BUS_ERR("MEM: New VA Range(%" PRIx64 " ~ %" PRIx64 ")",
				vaddr, vaddr + len);
			DPAA2_BUS_ERR("MEM: Overlap with (%" PRIx64 " ~ %" PRIx64 ")",
				dmaseg->vaddr,
				dmaseg->vaddr + dmaseg->size);
			return -EEXIST;
		}
		if (!((iovaddr + len) <= dmaseg->iova ||
			(dmaseg->iova + dmaseg->size) <= iovaddr)) {
			DPAA2_BUS_ERR("MEM: New IOVA Range(%" PRIx64 " ~ %" PRIx64 ")",
				iovaddr, iovaddr + len);
			DPAA2_BUS_ERR("MEM: Overlap with (%" PRIx64 " ~ %" PRIx64 ")",
				dmaseg->iova,
				dmaseg->iova + dmaseg->size);
			return -EEXIST;
		}
	}
	goto start_mapping;

io_mapping_check:
	TAILQ_FOREACH(dmaseg, &fslmc_iosegs, next) {
		if (!((vaddr + len) <= dmaseg->vaddr ||
			(dmaseg->vaddr + dmaseg->size) <= vaddr)) {
			DPAA2_BUS_ERR("IO: New VA Range (%" PRIx64 " ~ %" PRIx64 ")",
				vaddr, vaddr + len);
			DPAA2_BUS_ERR("IO: Overlap with (%" PRIx64 " ~ %" PRIx64 ")",
				dmaseg->vaddr,
				dmaseg->vaddr + dmaseg->size);
			return -EEXIST;
		}
		if (!((iovaddr + len) <= dmaseg->iova ||
			(dmaseg->iova + dmaseg->size) <= iovaddr)) {
			DPAA2_BUS_ERR("IO: New IOVA Range(%" PRIx64 " ~ %" PRIx64 ")",
				iovaddr, iovaddr + len);
			DPAA2_BUS_ERR("IO: Overlap with (%" PRIx64 " ~ %" PRIx64 ")",
				dmaseg->iova,
				dmaseg->iova + dmaseg->size);
			return -EEXIST;
		}
	}

start_mapping:
	fd = fslmc_vfio_group_fd_by_name(group_name);
	if (fd <= 0) {
		DPAA2_BUS_ERR("%s: Get fd by name(%s) failed(%d)",
			__func__, group_name, fd);
		if (fd < 0)
			return fd;
		return -EIO;
	}
	if (fslmc_vfio_iommu_type(fd) == VFIO_NOIOMMU_IOMMU) {
		DPAA2_BUS_DEBUG("Running in NOIOMMU mode");
		if (phy != iovaddr) {
			DPAA2_BUS_ERR("IOVA should support with IOMMU");
			return -EIO;
		}
		goto end_mapping;
	}

	dma_map.size = len;
	dma_map.vaddr = vaddr;
	dma_map.iova = iovaddr;

	/* SET DMA MAP for IOMMU */
	if (!fslmc_vfio_container_connected(fd)) {
		DPAA2_BUS_ERR("Container is not connected");
		return -EIO;
	}

	ret = ioctl(fslmc_vfio_container_fd(), VFIO_IOMMU_MAP_DMA,
		&dma_map);
	if (ret) {
		DPAA2_BUS_ERR("%s(%d) VA(%" PRIx64 "):IOVA(%" PRIx64 "):PHY(%" PRIx64 ")",
			is_io ? "DMA IO map err" : "DMA MEM map err",
			errno, vaddr, iovaddr, phy);
		return ret;
	}

end_mapping:
	dmaseg = malloc(sizeof(struct fslmc_dmaseg));
	if (!dmaseg) {
		DPAA2_BUS_ERR("DMA segment malloc failed!");
		return -ENOMEM;
	}
	dmaseg->vaddr = vaddr;
	dmaseg->iova = iovaddr;
	dmaseg->size = len;
	if (is_io) {
		TAILQ_INSERT_TAIL(&fslmc_iosegs, dmaseg, next);
	} else {
		fslmc_mem_map_num++;
		if (fslmc_mem_map_num == 1)
			fslmc_mem_va2iova = iovaddr - vaddr;
		else
			fslmc_mem_va2iova = RTE_BAD_IOVA;
		TAILQ_INSERT_TAIL(&fslmc_memsegs, dmaseg, next);
	}
	DPAA2_BUS_LOG(NOTICE,
		"%s(%zx): VA(%" PRIx64 "):IOVA(%" PRIx64 "):PHY(%" PRIx64 ")",
		is_io ? "DMA I/O map size" : "DMA MEM map size",
		len, vaddr, iovaddr, phy);

	return 0;
}

static int
fslmc_unmap_dma(uint64_t vaddr, uint64_t iovaddr, size_t len)
{
	struct vfio_iommu_type1_dma_unmap dma_unmap = {
		.argsz = sizeof(struct vfio_iommu_type1_dma_unmap),
		.flags = 0,
	};
	int ret, fd, is_io = 0;
	const char *group_name = fslmc_vfio_get_group_name();
	struct fslmc_dmaseg *dmaseg = NULL;

	TAILQ_FOREACH(dmaseg, &fslmc_memsegs, next) {
		if (((vaddr && dmaseg->vaddr == vaddr) || !vaddr) &&
			dmaseg->iova == iovaddr &&
			dmaseg->size == len) {
			is_io = 0;
			break;
		}
	}

	if (!dmaseg) {
		TAILQ_FOREACH(dmaseg, &fslmc_iosegs, next) {
			if (((vaddr && dmaseg->vaddr == vaddr) || !vaddr) &&
				dmaseg->iova == iovaddr &&
				dmaseg->size == len) {
				is_io = 1;
				break;
			}
		}
	}

	if (!dmaseg) {
		DPAA2_BUS_ERR("IOVA(%" PRIx64 ") with length(%zx) not mapped",
			iovaddr, len);
		return 0;
	}

	fd = fslmc_vfio_group_fd_by_name(group_name);
	if (fd <= 0) {
		DPAA2_BUS_ERR("%s: Get fd by name(%s) failed(%d)",
			__func__, group_name, fd);
		if (fd < 0)
			return fd;
		return -EIO;
	}
	if (fslmc_vfio_iommu_type(fd) == VFIO_NOIOMMU_IOMMU) {
		DPAA2_BUS_DEBUG("Running in NOIOMMU mode");
		return 0;
	}

	dma_unmap.size = len;
	dma_unmap.iova = iovaddr;

	/* SET DMA MAP for IOMMU */
	if (!fslmc_vfio_container_connected(fd)) {
		DPAA2_BUS_ERR("Container is not connected ");
		return -EIO;
	}

	ret = ioctl(fslmc_vfio_container_fd(), VFIO_IOMMU_UNMAP_DMA,
		&dma_unmap);
	if (ret) {
		DPAA2_BUS_ERR("DMA un-map IOVA(%" PRIx64 " ~ %" PRIx64 ") err(%d)",
			iovaddr, iovaddr + len, errno);
		return ret;
	}

	if (is_io) {
		TAILQ_REMOVE(&fslmc_iosegs, dmaseg, next);
	} else {
		TAILQ_REMOVE(&fslmc_memsegs, dmaseg, next);
		fslmc_mem_map_num--;
		if (TAILQ_EMPTY(&fslmc_memsegs))
			fslmc_mem_va2iova = RTE_BAD_IOVA;
	}

	free(dmaseg);

	return 0;
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_fslmc_cold_mem_vaddr_to_iova)
uint64_t
rte_fslmc_cold_mem_vaddr_to_iova(void *vaddr,
	uint64_t size)
{
	struct fslmc_dmaseg *dmaseg;
	uint64_t va;

	va = (uint64_t)vaddr;
	TAILQ_FOREACH(dmaseg, &fslmc_memsegs, next) {
		if (va >= dmaseg->vaddr &&
			(va + size) < (dmaseg->vaddr + dmaseg->size)) {
			return dmaseg->iova + va - dmaseg->vaddr;
		}
	}

	return RTE_BAD_IOVA;
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_fslmc_cold_mem_iova_to_vaddr)
void *
rte_fslmc_cold_mem_iova_to_vaddr(uint64_t iova,
	uint64_t size)
{
	struct fslmc_dmaseg *dmaseg;

	TAILQ_FOREACH(dmaseg, &fslmc_memsegs, next) {
		if (iova >= dmaseg->iova &&
			(iova + size) < (dmaseg->iova + dmaseg->size))
			return (void *)((uintptr_t)dmaseg->vaddr
				+ (uintptr_t)(iova - dmaseg->iova));
	}

	return NULL;
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_fslmc_mem_vaddr_to_iova)
__rte_hot uint64_t
rte_fslmc_mem_vaddr_to_iova(void *vaddr)
{
	if (likely(fslmc_mem_va2iova != RTE_BAD_IOVA))
		return (uint64_t)vaddr + fslmc_mem_va2iova;

	return rte_fslmc_cold_mem_vaddr_to_iova(vaddr, 0);
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_fslmc_mem_iova_to_vaddr)
__rte_hot void *
rte_fslmc_mem_iova_to_vaddr(uint64_t iova)
{
	if (likely(fslmc_mem_va2iova != RTE_BAD_IOVA))
		return (void *)((uintptr_t)iova - (uintptr_t)fslmc_mem_va2iova);

	return rte_fslmc_cold_mem_iova_to_vaddr(iova, 0);
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_fslmc_io_vaddr_to_iova)
uint64_t
rte_fslmc_io_vaddr_to_iova(void *vaddr)
{
	struct fslmc_dmaseg *dmaseg = NULL;
	uint64_t va = (uint64_t)vaddr;

	TAILQ_FOREACH(dmaseg, &fslmc_iosegs, next) {
		if ((va >= dmaseg->vaddr) &&
			va < dmaseg->vaddr + dmaseg->size)
			return dmaseg->iova + va - dmaseg->vaddr;
	}

	return RTE_BAD_IOVA;
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_fslmc_io_iova_to_vaddr)
void *
rte_fslmc_io_iova_to_vaddr(uint64_t iova)
{
	struct fslmc_dmaseg *dmaseg = NULL;

	TAILQ_FOREACH(dmaseg, &fslmc_iosegs, next) {
		if ((iova >= dmaseg->iova) &&
			iova < dmaseg->iova + dmaseg->size)
			return (void *)((uintptr_t)dmaseg->vaddr
				+ (uintptr_t)(iova - dmaseg->iova));
	}

	return NULL;
}

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

		DPAA2_BUS_DEBUG("%s, va=%p, virt=%" PRIx64 ", iova=%" PRIx64 ", len=%zu",
			type == RTE_MEM_EVENT_ALLOC ? "alloc" : "dealloc",
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
			DPAA2_BUS_ERR("%s: Map=%d, addr=%p, len=%zu, err:(%d)",
				type == RTE_MEM_EVENT_ALLOC ?
				"DMA Mapping failed. " :
				"DMA Unmapping failed. ",
				type, va, map_len, ret);
			return;
		}

		cur_len += map_len;
	}

	DPAA2_BUS_DEBUG("Total %s: addr=%p, len=%zu",
		type == RTE_MEM_EVENT_ALLOC ? "Mapped" : "Unmapped",
		addr, len);
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

RTE_EXPORT_SYMBOL(rte_fslmc_vfio_mem_dmamap)
int
rte_fslmc_vfio_mem_dmamap(uint64_t vaddr, uint64_t iova, uint64_t size)
{
	return fslmc_map_dma(vaddr, iova, size);
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_fslmc_vfio_mem_dmaunmap)
int
rte_fslmc_vfio_mem_dmaunmap(uint64_t iova, uint64_t size)
{
	return fslmc_unmap_dma(0, iova, size);
}

int
fslmc_vfio_dmamap(void)
{
	int i = 0, ret;

	/* Lock before parsing and registering callback to memory subsystem */
	rte_mcfg_mem_read_lock();

	ret = rte_memseg_walk(fslmc_dmamap_seg, &i);
	if (ret) {
		rte_mcfg_mem_read_unlock();
		return ret;
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
	if (vfio_group_fd <= 0) {
		DPAA2_BUS_ERR("%s: Get fd by name(%s) failed(%d)",
			__func__, group_name, vfio_group_fd);
		if (vfio_group_fd < 0)
			return vfio_group_fd;
		return -EIO;
	}

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

RTE_EXPORT_INTERNAL_SYMBOL(rte_dpaa2_intr_enable)
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

RTE_EXPORT_INTERNAL_SYMBOL(rte_dpaa2_intr_disable)
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
		DPAA2_BUS_ERR("Error disabling dpaa2 interrupts for fd %d",
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
			return ret;
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
			return fd;
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
	return -EIO;
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
				object->create(dev_fd, &device_info, dev);
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
	if (vfio_group_fd <= 0) {
		DPAA2_BUS_INFO("%s: Get fd by name(%s) failed(%d)",
			__func__, group_name, vfio_group_fd);
		if (vfio_group_fd < 0)
			return vfio_group_fd;
		return -EIO;
	}

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
					return ret;
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
		return -EIO;
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
				return ret;
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
				return ret;
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
				return ret;
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
