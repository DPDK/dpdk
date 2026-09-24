/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2025 Intel Corporation
 */

#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <uapi/linux/iommufd.h>
#include <uapi/linux/vfio.h>

#include <rte_log.h>
#include <rte_errno.h>
#include <rte_memory.h>
#include <rte_string_fns.h>

#include "eal_vfio.h"
#include "eal_private.h"
#include "eal_internal_cfg.h"

#define DEV_VFIO_IOMMUFD_PATH "/dev/iommu"
#define DEV_VFIO_CDEV_FMT "/dev/vfio/devices/vfio%d"
#define DEV_VFIO_NOIOMMU_CDEV_FMT "/dev/vfio/devices/noiommu-vfio%d"

static int vfio_cdev_dma_map(struct vfio_container *cfg);
static int vfio_cdev_dma_mem_map(struct vfio_container *cfg, uint64_t vaddr,
		uint64_t iova, uint64_t len, int do_map);

/* IOMMUFD cdev mode IOMMU operations */
static const struct vfio_iommu_ops iommufd_ops = {
	.type_id = 0, /* cdev mode doesn't use type_id */
	.name = "IOMMUFD",
	.partial_unmap = false,
	.dma_map_func = &vfio_cdev_dma_map,
	.dma_user_map_func = &vfio_cdev_dma_mem_map
};

void
vfio_cdev_setup_ops(void)
{
	vfio_global_cfg.ops = &iommufd_ops;
}

static int
vfio_cdev_dma_mem_map(struct vfio_container *cfg, uint64_t vaddr, uint64_t iova,
		uint64_t len, int do_map)
{
	struct iommu_ioas_map ioas_map;
	struct iommu_ioas_unmap ioas_unmap;
	int ret;

	if (do_map != 0) {
		memset(&ioas_map, 0, sizeof(ioas_map));
		ioas_map.size = sizeof(struct iommu_ioas_map);
		ioas_map.flags = IOMMU_IOAS_MAP_FIXED_IOVA |
			IOMMU_IOAS_MAP_READABLE |
			IOMMU_IOAS_MAP_WRITEABLE;
		ioas_map.ioas_id = cfg->cdev_cfg.ioas_id;
		ioas_map.user_va = vaddr;
		ioas_map.length = len;
		ioas_map.iova = iova;

		ret = ioctl(cfg->container_fd, IOMMU_IOAS_MAP, &ioas_map);
		if (ret) {
			/**
			 * In case the mapping was already done EEXIST will be
			 * returned from kernel.
			 */
			if (errno == EEXIST) {
				EAL_LOG(DEBUG, "Memory segment is already mapped, skipping");
			} else {
				EAL_LOG(ERR, "Cannot set up DMA remapping, error %i (%s)", errno,
					strerror(errno));
				return -1;
			}
		}
	} else {
		memset(&ioas_unmap, 0, sizeof(ioas_unmap));
		ioas_unmap.size = sizeof(struct iommu_ioas_unmap);
		ioas_unmap.ioas_id = cfg->cdev_cfg.ioas_id;
		ioas_unmap.length = len;
		ioas_unmap.iova = iova;

		ret = ioctl(cfg->container_fd, IOMMU_IOAS_UNMAP, &ioas_unmap);
		if (ret) {
			EAL_LOG(ERR, "Cannot clear DMA remapping, error %i (%s)", errno,
				strerror(errno));
			return -1;
		}
	}

	return 0;
}

static int
cdev_map(const struct rte_memseg_list *msl, const struct rte_memseg *ms,
		void *arg)
{
	struct vfio_container *cfg = arg;

	/* skip external memory that isn't a heap */
	if (msl->external && !msl->heap)
		return 0;

	/* skip any segments with invalid IOVA addresses */
	if (ms->iova == RTE_BAD_IOVA)
		return 0;

	return vfio_cdev_dma_mem_map(cfg, ms->addr_64, ms->iova, ms->len, 1);
}

static int
vfio_cdev_dma_map(struct vfio_container *cfg)
{
	return rte_memseg_walk(cdev_map, cfg);
}

int
vfio_cdev_sync_ioas(struct vfio_container *cfg)
{
	struct rte_mp_msg mp_req, *mp_rep;
	struct rte_mp_reply mp_reply = {0};
	struct timespec ts = {.tv_sec = 5, .tv_nsec = 0};
	struct vfio_mp_param *p = (struct vfio_mp_param *)mp_req.param;

	p->req = VFIO_SOCKET_REQ_IOAS_ID;
	rte_strscpy(mp_req.name, EAL_VFIO_MP, sizeof(mp_req.name));
	mp_req.len_param = sizeof(*p);
	mp_req.num_fds = 0;

	if (rte_mp_request_sync(&mp_req, &mp_reply, &ts) == 0 && mp_reply.nb_received == 1) {
		mp_rep = &mp_reply.msgs[0];
		p = (struct vfio_mp_param *)mp_rep->param;
		if (p->result == VFIO_SOCKET_OK && mp_rep->num_fds == 0) {
			cfg->cdev_cfg.ioas_id = p->ioas_id;
			free(mp_reply.msgs);
			return 0;
		}
	}

	free(mp_reply.msgs);
	EAL_LOG(ERR, "Cannot request ioas_id");
	return -1;
}

int
vfio_cdev_setup_ioas(struct vfio_container *cfg)
{
	struct iommu_ioas_alloc ioas_alloc;
	int ret;

	/* Allocate an IOAS */
	memset(&ioas_alloc, 0, sizeof(ioas_alloc));
	ioas_alloc.size = sizeof(struct iommu_ioas_alloc);
	ioas_alloc.flags = 0;

	ret = ioctl(cfg->container_fd, IOMMU_IOAS_ALLOC, &ioas_alloc);
	if (ret) {
		EAL_LOG(ERR, "Cannot allocate IOAS, error %i (%s)", errno, strerror(errno));
		return -1;
	}
	cfg->cdev_cfg.ioas_id = ioas_alloc.out_ioas_id;

	EAL_LOG(DEBUG, "Allocated IOAS with ID %u", cfg->cdev_cfg.ioas_id);
	return 0;
}

int
vfio_cdev_get_iommufd(void)
{
	int iommufd;

	/* if not requesting via mp, open iommufd locally */
	iommufd = open(DEV_VFIO_IOMMUFD_PATH, O_RDWR);
	if (iommufd < 0) {
		EAL_LOG(ERR, "Cannot open %s: %s", DEV_VFIO_IOMMUFD_PATH, strerror(errno));
		return -1;
	}

	return iommufd;
}

int
vfio_cdev_enable(struct vfio_container *cfg)
{
	int iommufd;

	/* Check if iommufd device exists */
	if (access(DEV_VFIO_IOMMUFD_PATH, F_OK) != 0) {
		EAL_LOG(DEBUG, "IOMMUFD device does not exist, skipping VFIO cdev support...");
		return 1;
	}

	/* open iommufd */
	iommufd = vfio_cdev_get_iommufd();
	if (iommufd < 0)
		return -1;

	cfg->container_fd = iommufd;
	return 0;
}

int
vfio_cdev_get_device_num(const char *sysfs_base, const char *dev_addr, int *cdev_dev_num)
{
	char linkname[PATH_MAX] = {0};
	const char *prefix;
	size_t prefix_len;
	char *dev_tok, *end;
	int dev_num;
	DIR *dir;
	struct dirent *entry;

	/* different prefixes for different IOMMU modes */
	switch (vfio_global_cfg.iova_mode) {
	case DEV_VFIO_IOVA_MODE_VA:
		prefix = "vfio";
		prefix_len = strlen(prefix);
		break;
	case DEV_VFIO_IOVA_MODE_PA:
		prefix = "noiommu-vfio";
		prefix_len = strlen(prefix);
		break;
	default:
		EAL_LOG(ERR, "VFIO IOMMU mode is not initialized");
		return -1;
	}

	/* check if vfio-dev directory exists for this device */
	snprintf(linkname, sizeof(linkname), "%s/%s/vfio-dev", sysfs_base, dev_addr);

	dir = opendir(linkname);
	if (dir == NULL) {
		/* device doesn't have vfio-dev, not bound to vfio-pci cdev */
		return 0;
	}

	/* find the matching VFIO cdev entry */
	while ((entry = readdir(dir)) != NULL) {
		if (strncmp(entry->d_name, prefix, prefix_len) != 0)
			continue;

		/* parse device number after the cdev entry prefix */
		errno = 0;
		dev_tok = entry->d_name + prefix_len;
		end = dev_tok;
		dev_num = strtol(dev_tok, &end, 10);
		if (end == dev_tok || *end != '\0' || errno != 0) {
			EAL_LOG(ERR, "%s error parsing VFIO cdev device number!", dev_addr);
			closedir(dir);
			return -1;
		}
		*cdev_dev_num = dev_num;
		closedir(dir);
		return 1;
	}

	closedir(dir);
	/* no vfio device found */
	return 0;
}

struct vfio_device *
vfio_cdev_get_dev_by_num(struct vfio_container *cfg, int cdev_dev_num)
{
	struct vfio_device *dev;
	/* find device handle */
	VFIO_DEVICE_FOREACH_ACTIVE(cfg, dev) {
		if (dev->dev_num != cdev_dev_num)
			continue;
		return dev;
	}
	return NULL;
}

static int
cdev_open_device_fd(int cdev_dev_num)
{
	char devname[PATH_MAX] = {0};
	int dev_fd;

	switch (vfio_global_cfg.iova_mode) {
	case DEV_VFIO_IOVA_MODE_VA:
		snprintf(devname, sizeof(devname), DEV_VFIO_CDEV_FMT, cdev_dev_num);
		break;
	case DEV_VFIO_IOVA_MODE_PA:
		snprintf(devname, sizeof(devname), DEV_VFIO_NOIOMMU_CDEV_FMT, cdev_dev_num);
		break;
	default:
		EAL_LOG(ERR, "VFIO IOMMU mode is not initialized");
		return -1;
	}

	dev_fd = open(devname, O_RDWR);
	if (dev_fd < 0) {
		EAL_LOG(ERR, "Cannot open %s: %s", devname, strerror(errno));
		return -1;
	}

	return dev_fd;
}

static int
cdev_attach_device_to_iommufd(struct vfio_container *cfg, struct vfio_device *dev)
{
	struct vfio_device_bind_iommufd bind = {0};
	struct vfio_device_attach_iommufd_pt attach = {0};
	rte_uuid_t vf_token;

	rte_eal_vfio_get_vf_token(vf_token);

	/* try with token first */
	if (!rte_uuid_is_null(vf_token)) {
		bind.flags = VFIO_DEVICE_BIND_FLAG_TOKEN;
		bind.token_uuid_ptr = (uintptr_t)&vf_token;
		bind.argsz = sizeof(bind);
		bind.iommufd = cfg->container_fd;

		/* this may fail because the kernel is too old */
		if (ioctl(dev->fd, VFIO_DEVICE_BIND_IOMMUFD, &bind) < 0) {
			EAL_LOG(DEBUG, "Failed to bind device %d with VF token", dev->dev_num);
			EAL_LOG(NOTICE, "Unable to use VF tokens with current kernel version.");
			EAL_LOG(NOTICE, "Please use kernel >=6.17 or use group mode.");
			/* erase the bind structure */
			bind = (struct vfio_device_bind_iommufd){0};
		} else {
			goto attach;
		}
	}
	bind.flags = 0;
	bind.argsz = sizeof(bind);
	bind.iommufd = cfg->container_fd;

	if (ioctl(dev->fd, VFIO_DEVICE_BIND_IOMMUFD, &bind) < 0) {
		EAL_LOG(ERR, "Cannot bind device to IOMMUFD, error %i (%s)", errno,
			strerror(errno));
		return -1;
	}

attach:
	/* attach device to IOAS */
	attach.argsz = sizeof(attach);
	attach.flags = 0;
	attach.pt_id = cfg->cdev_cfg.ioas_id;

	if (ioctl(dev->fd, VFIO_DEVICE_ATTACH_IOMMUFD_PT, &attach) < 0) {
		EAL_LOG(ERR, "Cannot attach device to IOAS, error %i (%s)", errno,
			strerror(errno));
		return -1;
	}

	return 0;
}

static int
vfio_cdev_request_dev_fd(struct vfio_device *dev)
{
	struct rte_mp_msg mp_req, *mp_rep;
	struct rte_mp_reply mp_reply = {0};
	struct timespec ts = {.tv_sec = 5, .tv_nsec = 0};
	struct vfio_mp_param *p = (struct vfio_mp_param *)mp_req.param;
	int device_fd = -1;

	/* secondary process requests device fd from primary */
	p->req = VFIO_SOCKET_REQ_CDEV;
	p->cdev_dev_num = dev->dev_num;
	rte_strscpy(mp_req.name, EAL_VFIO_MP, sizeof(mp_req.name));
	mp_req.len_param = sizeof(*p);
	mp_req.num_fds = 0;

	if (rte_mp_request_sync(&mp_req, &mp_reply, &ts) == 0 && mp_reply.nb_received == 1) {
		mp_rep = &mp_reply.msgs[0];
		p = (struct vfio_mp_param *)mp_rep->param;
		if (p->result == VFIO_SOCKET_OK && mp_rep->num_fds == 1)
			device_fd = mp_rep->fds[0];
	}

	free(mp_reply.msgs);

	if (device_fd < 0) {
		EAL_LOG(ERR, "Cannot request device fd for vfio%d", dev->dev_num);
		return -1;
	}
	dev->fd = device_fd;

	return 0;
}

int
vfio_cdev_setup_device(struct vfio_container *cfg, struct vfio_device *dev)
{
	int device_fd;

	/* get device fd - primary or custom container opens it, secondary requests from primary */
	if (rte_eal_process_type() == RTE_PROC_PRIMARY || !vfio_container_is_default(cfg)) {
		device_fd = cdev_open_device_fd(dev->dev_num);
		if (device_fd < 0)
			return -1;
		dev->fd = device_fd;

		/* attach device to iommufd */
		if (cdev_attach_device_to_iommufd(cfg, dev) < 0) {
			close(device_fd);
			dev->fd = -1;
			return -1;
		}
	} else if (vfio_cdev_request_dev_fd(dev) < 0) {
		return -1;
	}
	return 0;
}
