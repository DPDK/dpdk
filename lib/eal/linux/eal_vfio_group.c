/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2025 Intel Corporation
 */

#include <uapi/linux/vfio.h>

#include <dirent.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <rte_log.h>
#include <rte_errno.h>
#include <rte_eal_memconfig.h>
#include <rte_memory.h>
#include <rte_string_fns.h>
#include <dev_vfio.h>

#include "eal_vfio.h"
#include "eal_private.h"
#include "eal_internal_cfg.h"

#define DEV_VFIO_DIR "/dev/vfio"
#define DEV_VFIO_NOIOMMU_GROUP_FMT "/dev/vfio/noiommu-%u"

static int vfio_type1_dma_map(struct vfio_container *);
static int vfio_type1_dma_mem_map(struct vfio_container *, uint64_t, uint64_t, uint64_t, int);
static int vfio_spapr_dma_map(struct vfio_container *);
static int vfio_spapr_dma_mem_map(struct vfio_container *, uint64_t, uint64_t, uint64_t, int);
static int vfio_noiommu_dma_map(struct vfio_container *);
static int vfio_noiommu_dma_mem_map(struct vfio_container *, uint64_t, uint64_t, uint64_t, int);

/* IOMMU types we support */
static const struct vfio_iommu_ops iommu_types[] = {
	/* x86 IOMMU, otherwise known as type 1 */
	{
		.type_id = VFIO_TYPE1_IOMMU,
		.name = "Type 1",
		.partial_unmap = false,
		.dma_map_func = &vfio_type1_dma_map,
		.dma_user_map_func = &vfio_type1_dma_mem_map
	},
	/* ppc64 IOMMU, otherwise known as spapr */
	{
		.type_id = VFIO_SPAPR_TCE_v2_IOMMU,
		.name = "sPAPR",
		.partial_unmap = true,
		.dma_map_func = &vfio_spapr_dma_map,
		.dma_user_map_func = &vfio_spapr_dma_mem_map
	},
	/* IOMMU-less mode */
	{
		.type_id = VFIO_NOIOMMU_IOMMU,
		.name = "No-IOMMU",
		.partial_unmap = true,
		.dma_map_func = &vfio_noiommu_dma_map,
		.dma_user_map_func = &vfio_noiommu_dma_mem_map
	},
};

static const struct vfio_iommu_ops *
vfio_group_set_iommu_type(int vfio_container_fd)
{
	for (unsigned int idx = 0; idx < RTE_DIM(iommu_types); idx++) {
		const struct vfio_iommu_ops *t = &iommu_types[idx];

		int ret = ioctl(vfio_container_fd, VFIO_SET_IOMMU, t->type_id);
		if (ret == 0)
			return t;
		/* not an error, there may be more supported IOMMU types */
		EAL_LOG(DEBUG, "Set IOMMU type %d (%s) failed, error %i (%s)", t->type_id, t->name,
			errno, strerror(errno));
	}
	/* if we didn't find a suitable IOMMU type, fail */
	return NULL;
}

static int
type1_map(const struct rte_memseg_list *msl, const struct rte_memseg *ms, void *arg)
{
	struct vfio_container *cfg = arg;

	/* skip external memory that isn't a heap */
	if (msl->external && !msl->heap)
		return 0;

	/* skip any segments with invalid IOVA addresses */
	if (ms->iova == RTE_BAD_IOVA)
		return 0;

	return vfio_type1_dma_mem_map(cfg, ms->addr_64, ms->iova, ms->len, 1);
}

static int
vfio_type1_dma_mem_map(struct vfio_container *cfg, uint64_t vaddr, uint64_t iova, uint64_t len,
		int do_map)
{
	struct vfio_iommu_type1_dma_map dma_map;
	struct vfio_iommu_type1_dma_unmap dma_unmap;
	int ret;

	if (do_map != 0) {
		memset(&dma_map, 0, sizeof(dma_map));
		dma_map.argsz = sizeof(struct vfio_iommu_type1_dma_map);
		dma_map.vaddr = vaddr;
		dma_map.size = len;
		dma_map.iova = iova;
		dma_map.flags = VFIO_DMA_MAP_FLAG_READ | VFIO_DMA_MAP_FLAG_WRITE;

		ret = ioctl(cfg->container_fd, VFIO_IOMMU_MAP_DMA, &dma_map);
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
		memset(&dma_unmap, 0, sizeof(dma_unmap));
		dma_unmap.argsz = sizeof(struct vfio_iommu_type1_dma_unmap);
		dma_unmap.size = len;
		dma_unmap.iova = iova;

		ret = ioctl(cfg->container_fd, VFIO_IOMMU_UNMAP_DMA, &dma_unmap);
		if (ret) {
			EAL_LOG(ERR, "Cannot clear DMA remapping, error %i (%s)", errno,
				strerror(errno));
			return -1;
		} else if (dma_unmap.size != len) {
			EAL_LOG(ERR, "Unexpected size %"PRIu64
				" of DMA remapping cleared instead of %"PRIu64,
				(uint64_t)dma_unmap.size, len);
			return -1;
		}
	}

	return 0;
}

static int
vfio_type1_dma_map(struct vfio_container *cfg)
{
	return rte_memseg_walk(type1_map, cfg);
}

/* Track the size of the statically allocated DMA window for SPAPR */
static uint64_t spapr_dma_win_len;
static uint64_t spapr_dma_win_page_sz;

static int
vfio_spapr_dma_do_map(struct vfio_container *cfg, uint64_t vaddr, uint64_t iova, uint64_t len,
		int do_map)
{
	struct vfio_iommu_spapr_register_memory reg = {
		.argsz = sizeof(reg),
		.vaddr = (uintptr_t) vaddr,
		.size = len,
		.flags = 0
	};
	int ret;

	if (do_map != 0) {
		struct vfio_iommu_type1_dma_map dma_map;

		if (iova + len > spapr_dma_win_len) {
			EAL_LOG(ERR, "DMA map attempt outside DMA window");
			return -1;
		}

		ret = ioctl(cfg->container_fd, VFIO_IOMMU_SPAPR_REGISTER_MEMORY, &reg);
		if (ret) {
			EAL_LOG(ERR, "Cannot register vaddr for IOMMU, error %i (%s)", errno,
				strerror(errno));
			return -1;
		}

		memset(&dma_map, 0, sizeof(dma_map));
		dma_map.argsz = sizeof(struct vfio_iommu_type1_dma_map);
		dma_map.vaddr = vaddr;
		dma_map.size = len;
		dma_map.iova = iova;
		dma_map.flags = VFIO_DMA_MAP_FLAG_READ | VFIO_DMA_MAP_FLAG_WRITE;

		ret = ioctl(cfg->container_fd, VFIO_IOMMU_MAP_DMA, &dma_map);
		if (ret) {
			EAL_LOG(ERR, "Cannot map vaddr for IOMMU, error %i (%s)", errno,
				strerror(errno));
			return -1;
		}

	} else {
		struct vfio_iommu_type1_dma_unmap dma_unmap;

		memset(&dma_unmap, 0, sizeof(dma_unmap));
		dma_unmap.argsz = sizeof(struct vfio_iommu_type1_dma_unmap);
		dma_unmap.size = len;
		dma_unmap.iova = iova;

		ret = ioctl(cfg->container_fd, VFIO_IOMMU_UNMAP_DMA, &dma_unmap);
		if (ret) {
			EAL_LOG(ERR, "Cannot unmap vaddr for IOMMU, error %i (%s)", errno,
				strerror(errno));
			return -1;
		}

		ret = ioctl(cfg->container_fd, VFIO_IOMMU_SPAPR_UNREGISTER_MEMORY, &reg);
		if (ret) {
			EAL_LOG(ERR, "Cannot unregister vaddr for IOMMU, error %i (%s)", errno,
				strerror(errno));
			return -1;
		}
	}

	return ret;
}

static int
vfio_spapr_map_walk(const struct rte_memseg_list *msl, const struct rte_memseg *ms, void *arg)
{
	struct vfio_container *cfg = arg;

	/* skip external memory that isn't a heap */
	if (msl->external && !msl->heap)
		return 0;

	/* skip any segments with invalid IOVA addresses */
	if (ms->iova == RTE_BAD_IOVA)
		return 0;

	return vfio_spapr_dma_do_map(cfg, ms->addr_64, ms->iova, ms->len, 1);
}

struct spapr_size_walk_param {
	uint64_t max_va;
	uint64_t page_sz;
	bool is_user_managed;
};

/*
 * In order to set the DMA window size required for the SPAPR IOMMU
 * we need to walk the existing virtual memory allocations as well as
 * find the hugepage size used.
 */
static int
vfio_spapr_size_walk(const struct rte_memseg_list *msl, void *arg)
{
	struct spapr_size_walk_param *param = arg;
	uint64_t max = (uint64_t) msl->base_va + (uint64_t) msl->len;

	if (msl->external && !msl->heap) {
		/* ignore user managed external memory */
		param->is_user_managed = true;
		return 0;
	}

	if (max > param->max_va) {
		param->page_sz = msl->page_sz;
		param->max_va = max;
	}

	return 0;
}

/*
 * Find the highest memory address used in physical or virtual address
 * space and use that as the top of the DMA window.
 */
static int
find_highest_mem_addr(struct spapr_size_walk_param *param)
{
	/* find the maximum IOVA address for setting the DMA window size */
	if (rte_eal_iova_mode() == RTE_IOVA_PA) {
		static const char proc_iomem[] = "/proc/iomem";
		static const char str_sysram[] = "System RAM";
		uint64_t start, end, max = 0;
		char *line = NULL;
		char *dash, *space;
		size_t line_len;

		/*
		 * Example "System RAM" in /proc/iomem:
		 * 00000000-1fffffffff : System RAM
		 * 200000000000-201fffffffff : System RAM
		 */
		FILE *fd = fopen(proc_iomem, "r");
		if (fd == NULL) {
			EAL_LOG(ERR, "Cannot open %s", proc_iomem);
			return -1;
		}
		/* Scan /proc/iomem for the highest PA in the system */
		while (getline(&line, &line_len, fd) != -1) {
			if (strstr(line, str_sysram) == NULL)
				continue;

			space = strstr(line, " ");
			dash = strstr(line, "-");

			/* Validate the format of the memory string */
			if (space == NULL || dash == NULL || space < dash) {
				EAL_LOG(ERR, "Can't parse line \"%s\" in file %s", line,
					proc_iomem);
				continue;
			}

			start = strtoull(line, NULL, 16);
			end   = strtoull(dash + 1, NULL, 16);
			EAL_LOG(DEBUG, "Found system RAM from 0x%" PRIx64 " to 0x%" PRIx64,
				start, end);
			if (end > max)
				max = end;
		}
		free(line);
		fclose(fd);

		if (max == 0) {
			EAL_LOG(ERR, "Failed to find valid \"System RAM\" entry in file %s",
				proc_iomem);
			return -1;
		}

		spapr_dma_win_len = rte_align64pow2(max + 1);
		return 0;
	} else if (rte_eal_iova_mode() == RTE_IOVA_VA) {
		EAL_LOG(DEBUG, "Highest VA address in memseg list is 0x%" PRIx64, param->max_va);
		spapr_dma_win_len = rte_align64pow2(param->max_va);
		return 0;
	}

	spapr_dma_win_len = 0;
	EAL_LOG(ERR, "Unsupported IOVA mode");
	return -1;
}


/*
 * The SPAPRv2 IOMMU supports 2 DMA windows with starting
 * address at 0 or 1<<59.  By default, a DMA window is set
 * at address 0, 2GB long, with a 4KB page.  For DPDK we
 * must remove the default window and setup a new DMA window
 * based on the hugepage size and memory requirements of
 * the application before we can map memory for DMA.
 */
static int
spapr_dma_win_size(void)
{
	struct spapr_size_walk_param param;

	/* only create DMA window once */
	if (spapr_dma_win_len > 0)
		return 0;

	/* walk the memseg list to find the page size/max VA address */
	memset(&param, 0, sizeof(param));
	if (rte_memseg_list_walk(vfio_spapr_size_walk, &param) < 0) {
		EAL_LOG(ERR, "Failed to walk memseg list for DMA window size");
		return -1;
	}

	/* we can't be sure if DMA window covers external memory */
	if (param.is_user_managed)
		EAL_LOG(WARNING, "Detected user managed external memory which may not be managed by the IOMMU");

	/* check physical/virtual memory size */
	if (find_highest_mem_addr(&param) < 0)
		return -1;
	EAL_LOG(DEBUG, "Setting DMA window size to 0x%" PRIx64, spapr_dma_win_len);
	spapr_dma_win_page_sz = param.page_sz;
	rte_mem_set_dma_mask(rte_ctz64(spapr_dma_win_len));
	return 0;
}

static int
vfio_spapr_create_dma_window(struct vfio_container *cfg)
{
	struct vfio_iommu_spapr_tce_create create = { .argsz = sizeof(create), };
	struct vfio_iommu_spapr_tce_remove remove = { .argsz = sizeof(remove), };
	struct vfio_iommu_spapr_tce_info info = { .argsz = sizeof(info), };
	int ret;

	ret = spapr_dma_win_size();
	if (ret < 0)
		return ret;

	ret = ioctl(cfg->container_fd, VFIO_IOMMU_SPAPR_TCE_GET_INFO, &info);
	if (ret) {
		EAL_LOG(ERR, "Cannot get IOMMU info, error %i (%s)", errno, strerror(errno));
		return -1;
	}

	/*
	 * sPAPR v1/v2 IOMMU always has a default 1G DMA window set.  The window
	 * can't be changed for v1 but it can be changed for v2. Since DPDK only
	 * supports v2, remove the default DMA window so it can be resized.
	 */
	remove.start_addr = info.dma32_window_start;
	ret = ioctl(cfg->container_fd, VFIO_IOMMU_SPAPR_TCE_REMOVE, &remove);
	if (ret)
		return -1;

	/* create a new DMA window (start address is not selectable) */
	create.window_size = spapr_dma_win_len;
	create.page_shift  = rte_ctz64(spapr_dma_win_page_sz);
	create.levels = 1;
	ret = ioctl(cfg->container_fd, VFIO_IOMMU_SPAPR_TCE_CREATE, &create);
	/*
	 * The vfio_iommu_spapr_tce_info structure was modified in
	 * Linux kernel 4.2.0 to add support for the
	 * vfio_iommu_spapr_tce_ddw_info structure needed to try
	 * multiple table levels.  Skip the attempt if running with
	 * an older kernel.
	 */
	if (ret) {
		/* if at first we don't succeed, try more levels */
		uint32_t levels;

		for (levels = create.levels + 1;
			ret && levels <= info.ddw.levels; levels++) {
			create.levels = levels;
			ret = ioctl(cfg->container_fd, VFIO_IOMMU_SPAPR_TCE_CREATE, &create);
		}
	}
	if (ret) {
		EAL_LOG(ERR, "Cannot create new DMA window, error %i (%s)", errno,
			strerror(errno));
		EAL_LOG(ERR, "Consider using a larger hugepage size if supported by the system");
		return -1;
	}

	/* verify the start address  */
	if (create.start_addr != 0) {
		EAL_LOG(ERR, "Received unsupported start address 0x%" PRIx64,
			(uint64_t)create.start_addr);
		return -1;
	}
	return ret;
}

static int
vfio_spapr_dma_mem_map(struct vfio_container *cfg, uint64_t vaddr, uint64_t iova, uint64_t len,
		int do_map)
{
	int ret = 0;

	if (do_map) {
		if (vfio_spapr_dma_do_map(cfg, vaddr, iova, len, 1)) {
			EAL_LOG(ERR, "Failed to map DMA");
			ret = -1;
		}
	} else {
		if (vfio_spapr_dma_do_map(cfg, vaddr, iova, len, 0)) {
			EAL_LOG(ERR, "Failed to unmap DMA");
			ret = -1;
		}
	}

	return ret;
}

static int
vfio_spapr_dma_map(struct vfio_container *cfg)
{
	if (vfio_spapr_create_dma_window(cfg) < 0) {
		EAL_LOG(ERR, "Could not create new DMA window!");
		return -1;
	}

	/* map all existing DPDK segments for DMA */
	if (rte_memseg_walk(vfio_spapr_map_walk, cfg) < 0)
		return -1;

	return 0;
}

static int
vfio_noiommu_dma_map(struct vfio_container *cfg __rte_unused)
{
	/* No-IOMMU mode does not need DMA mapping */
	return 0;
}

static int
vfio_noiommu_dma_mem_map(struct vfio_container *cfg __rte_unused, uint64_t vaddr __rte_unused,
		uint64_t iova __rte_unused, uint64_t len __rte_unused, int do_map __rte_unused)
{
	/* No-IOMMU mode does not need DMA mapping */
	return 0;
}

struct vfio_group *
vfio_group_create(struct vfio_container *cfg, int iommu_group)
{
	struct vfio_group *grp;

	if (cfg->group_cfg.n_groups >= RTE_DIM(cfg->group_cfg.groups)) {
		EAL_LOG(ERR, "Cannot add more VFIO groups to container");
		return NULL;
	}
	VFIO_GROUP_FOREACH(cfg, grp) {
		if (grp->active)
			continue;
		cfg->group_cfg.n_groups++;
		grp->active = true;
		grp->group_num = iommu_group;
		grp->fd = -1;
		return grp;
	}
	/* should not happen */
	return NULL;
}

void
vfio_group_erase(struct vfio_container *cfg, struct vfio_group *grp)
{
	struct vfio_group_config *group_cfg = &cfg->group_cfg;

	if (grp->fd >= 0 && close(grp->fd) < 0)
		EAL_LOG(ERR, "Error when closing group fd %d", grp->fd);

	*grp = (struct vfio_group){0};
	group_cfg->n_groups--;

	/* if this was the last group in config, erase IOMMU setup and unregister callback */
	if (group_cfg->n_groups == 0) {
		cfg->dma_setup_done = false;
		group_cfg->iommu_type_set = false;
	}
}

struct vfio_group *
vfio_group_get_by_num(struct vfio_container *cfg, int iommu_group)
{
	struct vfio_group *grp;

	VFIO_GROUP_FOREACH_ACTIVE(cfg, grp) {
		if (grp->group_num == iommu_group)
			return grp;
	}
	return NULL;
}

static int
vfio_open_group_sysfs(int iommu_group_num)
{
	char filename[PATH_MAX];
	int fd;

	if (vfio_global_cfg.iova_mode == DEV_VFIO_IOVA_MODE_VA)
		snprintf(filename, sizeof(filename), DEV_VFIO_GROUP_FMT, iommu_group_num);
	else if (vfio_global_cfg.iova_mode == DEV_VFIO_IOVA_MODE_PA)
		snprintf(filename, sizeof(filename), DEV_VFIO_NOIOMMU_GROUP_FMT, iommu_group_num);

	/* reset errno before open to differentiate errors */
	errno = 0;
	fd = open(filename, O_RDWR);

	/* we have to differentiate between failed open and non-existence */
	if (errno == ENOENT)
		return -ENOENT;
	return fd;
}

static int
vfio_group_request_fd(int iommu_group_num)
{
	struct rte_mp_msg mp_req, *mp_rep;
	struct rte_mp_reply mp_reply = {0};
	struct timespec ts = {.tv_sec = 5, .tv_nsec = 0};
	struct vfio_mp_param *p = (struct vfio_mp_param *)mp_req.param;
	int vfio_group_fd = -1;

	p->req = VFIO_SOCKET_REQ_GROUP;
	p->group_num = iommu_group_num;
	rte_strscpy(mp_req.name, EAL_VFIO_MP, sizeof(mp_req.name));
	mp_req.len_param = sizeof(*p);
	mp_req.num_fds = 0;

	if (rte_mp_request_sync(&mp_req, &mp_reply, &ts) == 0 && mp_reply.nb_received == 1) {
		mp_rep = &mp_reply.msgs[0];
		p = (struct vfio_mp_param *)mp_rep->param;
		if (p->result == VFIO_SOCKET_OK && mp_rep->num_fds == 1) {
			vfio_group_fd = mp_rep->fds[0];
		} else if (p->result == VFIO_SOCKET_NO_FD) {
			EAL_LOG(ERR, "Bad VFIO group fd");
			vfio_group_fd = -ENOENT;
		}
	}

	free(mp_reply.msgs);
	return vfio_group_fd;
}

int
vfio_group_open_fd(struct vfio_container *cfg, struct vfio_group *grp)
{
	int vfio_group_fd;

	/* we make multiprocess request only in secondary processes for default config */
	if ((rte_eal_process_type() != RTE_PROC_PRIMARY) && (vfio_container_is_default(cfg)))
		vfio_group_fd = vfio_group_request_fd(grp->group_num);
	else
		vfio_group_fd = vfio_open_group_sysfs(grp->group_num);

	/* pass the non-existence up the chain */
	if (vfio_group_fd == -ENOENT)
		return vfio_group_fd;
	else if (vfio_group_fd < 0) {
		EAL_LOG(ERR, "Failed to open VFIO group %d", grp->group_num);
		return vfio_group_fd;
	}
	grp->fd = vfio_group_fd;
	return 0;
}

static const struct vfio_iommu_ops *
vfio_group_sync_iommu_ops(void)
{
	struct rte_mp_msg mp_req, *mp_rep;
	struct rte_mp_reply mp_reply = {0};
	struct timespec ts = {.tv_sec = 5, .tv_nsec = 0};
	struct vfio_mp_param *p = (struct vfio_mp_param *)mp_req.param;
	int iommu_type_id;
	unsigned int i;

	/* find default container's IOMMU type */
	p->req = VFIO_SOCKET_REQ_IOMMU_TYPE;
	rte_strscpy(mp_req.name, EAL_VFIO_MP, sizeof(mp_req.name));
	mp_req.len_param = sizeof(*p);
	mp_req.num_fds = 0;

	iommu_type_id = -1;
	if (rte_mp_request_sync(&mp_req, &mp_reply, &ts) == 0 && mp_reply.nb_received == 1) {
		mp_rep = &mp_reply.msgs[0];
		p = (struct vfio_mp_param *)mp_rep->param;
		if (p->result == VFIO_SOCKET_OK)
			iommu_type_id = p->iommu_type_id;
	}
	free(mp_reply.msgs);
	if (iommu_type_id < 0) {
		EAL_LOG(ERR, "Could not get IOMMU type from primary process");
		return NULL;
	}

	/* we now have an fd for default container, as well as its IOMMU type.
	 * now, set up default VFIO container config to match.
	 */
	for (i = 0; i < RTE_DIM(iommu_types); i++) {
		const struct vfio_iommu_ops *t = &iommu_types[i];
		if (t->type_id != iommu_type_id)
			continue;

		return t;
	}
	EAL_LOG(ERR, "Could not find IOMMU type id (%i)", iommu_type_id);
	return NULL;
}

static int
vfio_has_supported_extensions(int vfio_container_fd)
{
	unsigned int n_extensions = 0;
	int ret;

	for (unsigned int idx = 0; idx < RTE_DIM(iommu_types); idx++) {
		const struct vfio_iommu_ops *t = &iommu_types[idx];

		ret = ioctl(vfio_container_fd, VFIO_CHECK_EXTENSION, t->type_id);
		if (ret < 0) {
			EAL_LOG(ERR, "Could not get IOMMU type, error %i (%s)", errno,
				strerror(errno));
			return -1;
		} else if (ret == 1) {
			/* we found a supported extension */
			n_extensions++;
		}
		EAL_LOG(DEBUG, "IOMMU type %d (%s) is %s", t->type_id, t->name,
			ret ? "supported" : "not supported");
	}

	/* if we didn't find any supported IOMMU types, fail */
	if (n_extensions == 0)
		return -1;

	return 0;
}

int
vfio_group_open_container_fd(void)
{
	int ret, vfio_container_fd;

	vfio_container_fd = open(DEV_VFIO_CONTAINER_PATH, O_RDWR);
	if (vfio_container_fd < 0) {
		EAL_LOG(DEBUG, "Cannot open VFIO container %s, error %i (%s)",
			DEV_VFIO_CONTAINER_PATH, errno, strerror(errno));
		return -1;
	}

	/* check VFIO API version */
	ret = ioctl(vfio_container_fd, VFIO_GET_API_VERSION);
	if (ret != VFIO_API_VERSION) {
		if (ret < 0)
			EAL_LOG(DEBUG, "Could not get VFIO API version, error %i (%s)", errno,
				strerror(errno));
		else
			EAL_LOG(DEBUG, "Unsupported VFIO API version!");
		close(vfio_container_fd);
		return -1;
	}

	ret = vfio_has_supported_extensions(vfio_container_fd);
	if (ret) {
		EAL_LOG(DEBUG, "No supported IOMMU extensions found!");
		close(vfio_container_fd);
		return -1;
	}

	return vfio_container_fd;
}

int
vfio_group_enable(struct vfio_container *cfg)
{
	int container_fd;
	DIR *dir;

	/* VFIO directory might not exist (e.g., unprivileged containers) */
	dir = opendir(DEV_VFIO_DIR);
	if (dir == NULL) {
		EAL_LOG(DEBUG, "VFIO directory does not exist, skipping VFIO group support...");
		return 1;
	}
	closedir(dir);

	/* open a default container */
	container_fd = vfio_group_open_container_fd();
	if (container_fd < 0)
		return -1;

	cfg->container_fd = container_fd;
	return 0;
}

int
vfio_group_prepare(struct vfio_container *cfg, struct vfio_group *grp)
{
	struct vfio_group_status group_status = {
		.argsz = sizeof(group_status)};
	int ret;

	/*
	 * We need to assign group to a container and check if it is viable, but there are cases
	 * where we don't need to do that.
	 *
	 * For default container, we need to set up the group only in primary process, as secondary
	 * process would have requested group fd over IPC, which implies it would have already been
	 * set up by the primary.
	 *
	 * For custom containers, every process sets up its own groups.
	 */
	if (vfio_container_is_default(cfg) && rte_eal_process_type() != RTE_PROC_PRIMARY) {
		EAL_LOG(DEBUG, "Skipping setup for VFIO group %d", grp->group_num);
		return 0;
	}

	/* check if the group is viable */
	ret = ioctl(grp->fd, VFIO_GROUP_GET_STATUS, &group_status);
	if (ret) {
		EAL_LOG(ERR, "Cannot get VFIO group status for group %d, error %i (%s)",
			grp->group_num, errno, strerror(errno));
		return -1;
	}

	if ((group_status.flags & VFIO_GROUP_FLAGS_VIABLE) == 0) {
		EAL_LOG(ERR, "VFIO group %d is not viable! "
			"Not all devices in IOMMU group bound to VFIO or unbound",
			grp->group_num);
		return -1;
	}

	/* set container for group if necessary */
	if ((group_status.flags & VFIO_GROUP_FLAGS_CONTAINER_SET) == 0) {
		/* add group to a container */
		ret = ioctl(grp->fd, VFIO_GROUP_SET_CONTAINER, &cfg->container_fd);
		if (ret) {
			EAL_LOG(ERR, "Cannot add VFIO group %d to container, error %i (%s)",
				grp->group_num, errno, strerror(errno));
			return -1;
		}
	} else {
		/* group is already added to a container - this should not happen */
		EAL_LOG(ERR, "VFIO group %d is already assigned to a container", grp->group_num);
		return -1;
	}
	return 0;
}

int
vfio_group_setup_iommu(struct vfio_container *cfg)
{
	const struct vfio_iommu_ops *ops;

	/*
	 * Setting IOMMU type is a per-container operation (via ioctl on container fd), but the ops
	 * structure is global and shared across all containers.
	 *
	 * For secondary processes with default container, we sync ops from primary. For all other
	 * cases (primary, or secondary with custom containers), we set IOMMU type on the container
	 * which also discovers the ops.
	 */
	if (vfio_container_is_default(cfg) && rte_eal_process_type() != RTE_PROC_PRIMARY) {
		/* Secondary process: sync ops from primary for default container */
		ops = vfio_group_sync_iommu_ops();
		if (ops == NULL)
			return -1;
	} else {
		/* Primary process OR custom container: set IOMMU type on container */
		ops = vfio_group_set_iommu_type(cfg->container_fd);
		if (ops == NULL)
			return -1;
	}

	/* Set or verify global ops */
	if (vfio_global_cfg.ops == NULL) {
		vfio_global_cfg.ops = ops;
		EAL_LOG(INFO, "IOMMU type set to %d (%s)", ops->type_id, ops->name);
	} else if (vfio_global_cfg.ops != ops) {
		/* This shouldn't happen on the same machine, but log it */
		EAL_LOG(WARNING, "Container has different IOMMU type (%d - %s) "
			"than previously set (%d - %s)",
			ops->type_id, ops->name, vfio_global_cfg.ops->type_id,
			vfio_global_cfg.ops->name);
	}

	return 0;
}

int
vfio_group_setup_device_fd(const char *dev_addr, struct vfio_group *grp, struct vfio_device *dev)
{
	rte_uuid_t vf_token;
	int fd;

	rte_eal_vfio_get_vf_token(vf_token);

	if (!rte_uuid_is_null(vf_token)) {
		char vf_token_str[RTE_UUID_STRLEN];
		char devaddr[PATH_MAX];

		rte_uuid_unparse(vf_token, vf_token_str, sizeof(vf_token_str));
		snprintf(devaddr, sizeof(devaddr), "%s vf_token=%s", dev_addr, vf_token_str);

		fd = ioctl(grp->fd, VFIO_GROUP_GET_DEVICE_FD, devaddr);
		if (fd >= 0)
			goto out;
	}
	/* get a file descriptor for the device */
	fd = ioctl(grp->fd, VFIO_GROUP_GET_DEVICE_FD, dev_addr);
	if (fd < 0) {
		/*
		 * if we cannot get a device fd, this implies a problem with the VFIO group or the
		 * container not having IOMMU configured.
		 */
		EAL_LOG(WARNING, "Getting a vfio_dev_fd for %s failed", dev_addr);
		return -1;
	}
out:
	dev->fd = fd;
	/* store backreference to group */
	dev->group = grp->group_num;
	/* increment number of devices in group */
	grp->n_devices++;
	return 0;
}

int
vfio_group_get_num(const char *sysfs_base, const char *dev_addr, int *iommu_group_num)
{
	char linkname[PATH_MAX];
	char filename[PATH_MAX];
	char *tok[16], *group_tok, *end;
	int ret, group_num;

	memset(linkname, 0, sizeof(linkname));
	memset(filename, 0, sizeof(filename));

	/* try to find out IOMMU group for this device */
	snprintf(linkname, sizeof(linkname), "%s/%s/iommu_group", sysfs_base, dev_addr);

	ret = readlink(linkname, filename, sizeof(filename));

	/* if the link doesn't exist, no VFIO for us */
	if (ret < 0)
		return 0;

	ret = rte_strsplit(filename, sizeof(filename), tok, RTE_DIM(tok), '/');
	if (ret <= 0) {
		EAL_LOG(ERR, "%s cannot get IOMMU group", dev_addr);
		return -1;
	}

	/* IOMMU group is always the last token */
	errno = 0;
	group_tok = tok[ret - 1];
	end = group_tok;
	group_num = strtol(group_tok, &end, 10);
	if (end == group_tok || *end != '\0' || errno != 0) {
		EAL_LOG(ERR, "%s error parsing IOMMU number!", dev_addr);
		return -1;
	}
	*iommu_group_num = group_num;

	return 1;
}
