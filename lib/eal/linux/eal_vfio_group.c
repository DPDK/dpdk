/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include <uapi/linux/vfio.h>

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_errno.h>
#include <rte_log.h>
#include <rte_memory.h>

#include "eal_vfio.h"
#include "eal_private.h"

static int vfio_type1_dma_map(struct vfio_container *);
static int vfio_type1_dma_mem_map(struct vfio_container *, uint64_t, uint64_t, uint64_t, int);
static int vfio_spapr_dma_map(struct vfio_container *);
static int vfio_spapr_dma_mem_map(struct vfio_container *, uint64_t, uint64_t, uint64_t, int);
static int vfio_noiommu_dma_map(struct vfio_container *);
static int vfio_noiommu_dma_mem_map(struct vfio_container *, uint64_t, uint64_t, uint64_t, int);

/* IOMMU types we support */
const struct vfio_iommu_ops iommu_types[] = {
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

const struct vfio_iommu_ops *
vfio_set_iommu_type(int vfio_container_fd)
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
			rte_errno = EIO;
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

int
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
