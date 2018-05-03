/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include <inttypes.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <rte_errno.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_eal_memconfig.h>
#include <rte_vfio.h>

#include "eal_filesystem.h"
#include "eal_vfio.h"
#include "eal_private.h"

#ifdef VFIO_PRESENT

#define VFIO_MEM_EVENT_CLB_NAME "vfio_mem_event_clb"

/* hot plug/unplug of VFIO groups may cause all DMA maps to be dropped. we can
 * recreate the mappings for DPDK segments, but we cannot do so for memory that
 * was registered by the user themselves, so we need to store the user mappings
 * somewhere, to recreate them later.
 */
#define VFIO_MAX_USER_MEM_MAPS 256
struct user_mem_map {
	uint64_t addr;
	uint64_t iova;
	uint64_t len;
};

struct user_mem_maps {
	rte_spinlock_recursive_t lock;
	int n_maps;
	struct user_mem_map maps[VFIO_MAX_USER_MEM_MAPS];
};

struct vfio_config {
	int vfio_enabled;
	int vfio_container_fd;
	int vfio_active_groups;
	const struct vfio_iommu_type *vfio_iommu_type;
	struct vfio_group vfio_groups[VFIO_MAX_GROUPS];
	struct user_mem_maps mem_maps;
};

/* per-process VFIO config */
static struct vfio_config vfio_cfgs[VFIO_MAX_CONTAINERS];
static struct vfio_config *default_vfio_cfg = &vfio_cfgs[0];

static int vfio_type1_dma_map(int);
static int vfio_type1_dma_mem_map(int, uint64_t, uint64_t, uint64_t, int);
static int vfio_spapr_dma_map(int);
static int vfio_spapr_dma_mem_map(int, uint64_t, uint64_t, uint64_t, int);
static int vfio_noiommu_dma_map(int);
static int vfio_noiommu_dma_mem_map(int, uint64_t, uint64_t, uint64_t, int);
static int vfio_dma_mem_map(struct vfio_config *vfio_cfg, uint64_t vaddr,
		uint64_t iova, uint64_t len, int do_map);

/* IOMMU types we support */
static const struct vfio_iommu_type iommu_types[] = {
	/* x86 IOMMU, otherwise known as type 1 */
	{
		.type_id = RTE_VFIO_TYPE1,
		.name = "Type 1",
		.dma_map_func = &vfio_type1_dma_map,
		.dma_user_map_func = &vfio_type1_dma_mem_map
	},
	/* ppc64 IOMMU, otherwise known as spapr */
	{
		.type_id = RTE_VFIO_SPAPR,
		.name = "sPAPR",
		.dma_map_func = &vfio_spapr_dma_map,
		.dma_user_map_func = &vfio_spapr_dma_mem_map
	},
	/* IOMMU-less mode */
	{
		.type_id = RTE_VFIO_NOIOMMU,
		.name = "No-IOMMU",
		.dma_map_func = &vfio_noiommu_dma_map,
		.dma_user_map_func = &vfio_noiommu_dma_mem_map
	},
};

/* for sPAPR IOMMU, we will need to walk memseg list, but we cannot use
 * rte_memseg_walk() because by the time we enter callback we will be holding a
 * write lock, so regular rte-memseg_walk will deadlock. copying the same
 * iteration code everywhere is not ideal as well. so, use a lockless copy of
 * memseg walk here.
 */
static int
memseg_walk_thread_unsafe(rte_memseg_walk_t func, void *arg)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	int i, ms_idx, ret = 0;

	for (i = 0; i < RTE_MAX_MEMSEG_LISTS; i++) {
		struct rte_memseg_list *msl = &mcfg->memsegs[i];
		const struct rte_memseg *ms;
		struct rte_fbarray *arr;

		if (msl->memseg_arr.count == 0)
			continue;

		arr = &msl->memseg_arr;

		ms_idx = rte_fbarray_find_next_used(arr, 0);
		while (ms_idx >= 0) {
			ms = rte_fbarray_get(arr, ms_idx);
			ret = func(msl, ms, arg);
			if (ret < 0)
				return -1;
			if (ret > 0)
				return 1;
			ms_idx = rte_fbarray_find_next_used(arr, ms_idx + 1);
		}
	}
	return 0;
}

static int
is_null_map(const struct user_mem_map *map)
{
	return map->addr == 0 && map->iova == 0 && map->len == 0;
}

/* we may need to merge user mem maps together in case of user mapping/unmapping
 * chunks of memory, so we'll need a comparator function to sort segments.
 */
static int
user_mem_map_cmp(const void *a, const void *b)
{
	const struct user_mem_map *umm_a = a;
	const struct user_mem_map *umm_b = b;

	/* move null entries to end */
	if (is_null_map(umm_a))
		return 1;
	if (is_null_map(umm_b))
		return -1;

	/* sort by iova first */
	if (umm_a->iova < umm_b->iova)
		return -1;
	if (umm_a->iova > umm_b->iova)
		return 1;

	if (umm_a->addr < umm_b->addr)
		return -1;
	if (umm_a->addr > umm_b->addr)
		return 1;

	if (umm_a->len < umm_b->len)
		return -1;
	if (umm_a->len > umm_b->len)
		return 1;

	return 0;
}

/* adjust user map entry. this may result in shortening of existing map, or in
 * splitting existing map in two pieces.
 */
static void
adjust_map(struct user_mem_map *src, struct user_mem_map *end,
		uint64_t remove_va_start, uint64_t remove_len)
{
	/* if va start is same as start address, we're simply moving start */
	if (remove_va_start == src->addr) {
		src->addr += remove_len;
		src->iova += remove_len;
		src->len -= remove_len;
	} else if (remove_va_start + remove_len == src->addr + src->len) {
		/* we're shrinking mapping from the end */
		src->len -= remove_len;
	} else {
		/* we're blowing a hole in the middle */
		struct user_mem_map tmp;
		uint64_t total_len = src->len;

		/* adjust source segment length */
		src->len = remove_va_start - src->addr;

		/* create temporary segment in the middle */
		tmp.addr = src->addr + src->len;
		tmp.iova = src->iova + src->len;
		tmp.len = remove_len;

		/* populate end segment - this one we will be keeping */
		end->addr = tmp.addr + tmp.len;
		end->iova = tmp.iova + tmp.len;
		end->len = total_len - src->len - tmp.len;
	}
}

/* try merging two maps into one, return 1 if succeeded */
static int
merge_map(struct user_mem_map *left, struct user_mem_map *right)
{
	if (left->addr + left->len != right->addr)
		return 0;
	if (left->iova + left->len != right->iova)
		return 0;

	left->len += right->len;

	memset(right, 0, sizeof(*right));

	return 1;
}

static struct user_mem_map *
find_user_mem_map(struct user_mem_maps *user_mem_maps, uint64_t addr,
		uint64_t iova, uint64_t len)
{
	uint64_t va_end = addr + len;
	uint64_t iova_end = iova + len;
	int i;

	for (i = 0; i < user_mem_maps->n_maps; i++) {
		struct user_mem_map *map = &user_mem_maps->maps[i];
		uint64_t map_va_end = map->addr + map->len;
		uint64_t map_iova_end = map->iova + map->len;

		/* check start VA */
		if (addr < map->addr || addr >= map_va_end)
			continue;
		/* check if VA end is within boundaries */
		if (va_end <= map->addr || va_end > map_va_end)
			continue;

		/* check start IOVA */
		if (iova < map->iova || iova >= map_iova_end)
			continue;
		/* check if IOVA end is within boundaries */
		if (iova_end <= map->iova || iova_end > map_iova_end)
			continue;

		/* we've found our map */
		return map;
	}
	return NULL;
}

/* this will sort all user maps, and merge/compact any adjacent maps */
static void
compact_user_maps(struct user_mem_maps *user_mem_maps)
{
	int i, n_merged, cur_idx;

	qsort(user_mem_maps->maps, user_mem_maps->n_maps,
			sizeof(user_mem_maps->maps[0]), user_mem_map_cmp);

	/* we'll go over the list backwards when merging */
	n_merged = 0;
	for (i = user_mem_maps->n_maps - 2; i >= 0; i--) {
		struct user_mem_map *l, *r;

		l = &user_mem_maps->maps[i];
		r = &user_mem_maps->maps[i + 1];

		if (is_null_map(l) || is_null_map(r))
			continue;

		if (merge_map(l, r))
			n_merged++;
	}

	/* the entries are still sorted, but now they have holes in them, so
	 * walk through the list and remove the holes
	 */
	if (n_merged > 0) {
		cur_idx = 0;
		for (i = 0; i < user_mem_maps->n_maps; i++) {
			if (!is_null_map(&user_mem_maps->maps[i])) {
				struct user_mem_map *src, *dst;

				src = &user_mem_maps->maps[i];
				dst = &user_mem_maps->maps[cur_idx++];

				if (src != dst) {
					memcpy(dst, src, sizeof(*src));
					memset(src, 0, sizeof(*src));
				}
			}
		}
		user_mem_maps->n_maps = cur_idx;
	}
}

static int
vfio_open_group_fd(int iommu_group_num)
{
	int vfio_group_fd;
	char filename[PATH_MAX];
	struct rte_mp_msg mp_req, *mp_rep;
	struct rte_mp_reply mp_reply;
	struct timespec ts = {.tv_sec = 5, .tv_nsec = 0};
	struct vfio_mp_param *p = (struct vfio_mp_param *)mp_req.param;

	/* if primary, try to open the group */
	if (internal_config.process_type == RTE_PROC_PRIMARY) {
		/* try regular group format */
		snprintf(filename, sizeof(filename),
				 VFIO_GROUP_FMT, iommu_group_num);
		vfio_group_fd = open(filename, O_RDWR);
		if (vfio_group_fd < 0) {
			/* if file not found, it's not an error */
			if (errno != ENOENT) {
				RTE_LOG(ERR, EAL, "Cannot open %s: %s\n", filename,
						strerror(errno));
				return -1;
			}

			/* special case: try no-IOMMU path as well */
			snprintf(filename, sizeof(filename),
					VFIO_NOIOMMU_GROUP_FMT,
					iommu_group_num);
			vfio_group_fd = open(filename, O_RDWR);
			if (vfio_group_fd < 0) {
				if (errno != ENOENT) {
					RTE_LOG(ERR, EAL, "Cannot open %s: %s\n", filename,
							strerror(errno));
					return -1;
				}
				return 0;
			}
			/* noiommu group found */
		}

		return vfio_group_fd;
	}
	/* if we're in a secondary process, request group fd from the primary
	 * process via mp channel.
	 */
	p->req = SOCKET_REQ_GROUP;
	p->group_num = iommu_group_num;
	strcpy(mp_req.name, EAL_VFIO_MP);
	mp_req.len_param = sizeof(*p);
	mp_req.num_fds = 0;

	vfio_group_fd = -1;
	if (rte_mp_request_sync(&mp_req, &mp_reply, &ts) == 0 &&
	    mp_reply.nb_received == 1) {
		mp_rep = &mp_reply.msgs[0];
		p = (struct vfio_mp_param *)mp_rep->param;
		if (p->result == SOCKET_OK && mp_rep->num_fds == 1) {
			vfio_group_fd = mp_rep->fds[0];
		} else if (p->result == SOCKET_NO_FD) {
			RTE_LOG(ERR, EAL, "  bad VFIO group fd\n");
			vfio_group_fd = 0;
		}
		free(mp_reply.msgs);
	}

	if (vfio_group_fd < 0)
		RTE_LOG(ERR, EAL, "  cannot request group fd\n");
	return vfio_group_fd;
}

static struct vfio_config *
get_vfio_cfg_by_group_num(int iommu_group_num)
{
	struct vfio_config *vfio_cfg;
	int i, j;

	for (i = 0; i < VFIO_MAX_CONTAINERS; i++) {
		vfio_cfg = &vfio_cfgs[i];
		for (j = 0; j < VFIO_MAX_GROUPS; j++) {
			if (vfio_cfg->vfio_groups[j].group_num ==
					iommu_group_num)
				return vfio_cfg;
		}
	}

	return NULL;
}

static struct vfio_config *
get_vfio_cfg_by_group_fd(int vfio_group_fd)
{
	struct vfio_config *vfio_cfg;
	int i, j;

	for (i = 0; i < VFIO_MAX_CONTAINERS; i++) {
		vfio_cfg = &vfio_cfgs[i];
		for (j = 0; j < VFIO_MAX_GROUPS; j++)
			if (vfio_cfg->vfio_groups[j].fd == vfio_group_fd)
				return vfio_cfg;
	}

	return NULL;
}

static struct vfio_config *
get_vfio_cfg_by_container_fd(int container_fd)
{
	int i;

	for (i = 0; i < VFIO_MAX_CONTAINERS; i++) {
		if (vfio_cfgs[i].vfio_container_fd == container_fd)
			return &vfio_cfgs[i];
	}

	return NULL;
}

int
rte_vfio_get_group_fd(int iommu_group_num)
{
	int i;
	int vfio_group_fd;
	struct vfio_group *cur_grp;
	struct vfio_config *vfio_cfg;

	/* get the vfio_config it belongs to */
	vfio_cfg = get_vfio_cfg_by_group_num(iommu_group_num);
	vfio_cfg = vfio_cfg ? vfio_cfg : default_vfio_cfg;

	/* check if we already have the group descriptor open */
	for (i = 0; i < VFIO_MAX_GROUPS; i++)
		if (vfio_cfg->vfio_groups[i].group_num == iommu_group_num)
			return vfio_cfg->vfio_groups[i].fd;

	/* Lets see first if there is room for a new group */
	if (vfio_cfg->vfio_active_groups == VFIO_MAX_GROUPS) {
		RTE_LOG(ERR, EAL, "Maximum number of VFIO groups reached!\n");
		return -1;
	}

	/* Now lets get an index for the new group */
	for (i = 0; i < VFIO_MAX_GROUPS; i++)
		if (vfio_cfg->vfio_groups[i].group_num == -1) {
			cur_grp = &vfio_cfg->vfio_groups[i];
			break;
		}

	/* This should not happen */
	if (i == VFIO_MAX_GROUPS) {
		RTE_LOG(ERR, EAL, "No VFIO group free slot found\n");
		return -1;
	}

	vfio_group_fd = vfio_open_group_fd(iommu_group_num);
	if (vfio_group_fd < 0) {
		RTE_LOG(ERR, EAL, "Failed to open group %d\n", iommu_group_num);
		return -1;
	}

	cur_grp->group_num = iommu_group_num;
	cur_grp->fd = vfio_group_fd;
	vfio_cfg->vfio_active_groups++;

	return vfio_group_fd;
}

static int
get_vfio_group_idx(int vfio_group_fd)
{
	struct vfio_config *vfio_cfg;
	int i, j;

	for (i = 0; i < VFIO_MAX_CONTAINERS; i++) {
		vfio_cfg = &vfio_cfgs[i];
		for (j = 0; j < VFIO_MAX_GROUPS; j++)
			if (vfio_cfg->vfio_groups[j].fd == vfio_group_fd)
				return j;
	}

	return -1;
}

static void
vfio_group_device_get(int vfio_group_fd)
{
	struct vfio_config *vfio_cfg;
	int i;

	vfio_cfg = get_vfio_cfg_by_group_fd(vfio_group_fd);
	if (vfio_cfg == NULL) {
		RTE_LOG(ERR, EAL, "  invalid group fd!\n");
		return;
	}

	i = get_vfio_group_idx(vfio_group_fd);
	if (i < 0 || i > (VFIO_MAX_GROUPS - 1))
		RTE_LOG(ERR, EAL, "  wrong vfio_group index (%d)\n", i);
	else
		vfio_cfg->vfio_groups[i].devices++;
}

static void
vfio_group_device_put(int vfio_group_fd)
{
	struct vfio_config *vfio_cfg;
	int i;

	vfio_cfg = get_vfio_cfg_by_group_fd(vfio_group_fd);
	if (vfio_cfg == NULL) {
		RTE_LOG(ERR, EAL, "  invalid group fd!\n");
		return;
	}

	i = get_vfio_group_idx(vfio_group_fd);
	if (i < 0 || i > (VFIO_MAX_GROUPS - 1))
		RTE_LOG(ERR, EAL, "  wrong vfio_group index (%d)\n", i);
	else
		vfio_cfg->vfio_groups[i].devices--;
}

static int
vfio_group_device_count(int vfio_group_fd)
{
	struct vfio_config *vfio_cfg;
	int i;

	vfio_cfg = get_vfio_cfg_by_group_fd(vfio_group_fd);
	if (vfio_cfg == NULL) {
		RTE_LOG(ERR, EAL, "  invalid group fd!\n");
		return -1;
	}

	i = get_vfio_group_idx(vfio_group_fd);
	if (i < 0 || i > (VFIO_MAX_GROUPS - 1)) {
		RTE_LOG(ERR, EAL, "  wrong vfio_group index (%d)\n", i);
		return -1;
	}

	return vfio_cfg->vfio_groups[i].devices;
}

static void
vfio_mem_event_callback(enum rte_mem_event type, const void *addr, size_t len,
		void *arg __rte_unused)
{
	struct rte_memseg_list *msl;
	struct rte_memseg *ms;
	size_t cur_len = 0;

	msl = rte_mem_virt2memseg_list(addr);

	/* for IOVA as VA mode, no need to care for IOVA addresses */
	if (rte_eal_iova_mode() == RTE_IOVA_VA) {
		uint64_t vfio_va = (uint64_t)(uintptr_t)addr;
		if (type == RTE_MEM_EVENT_ALLOC)
			vfio_dma_mem_map(default_vfio_cfg, vfio_va, vfio_va,
					len, 1);
		else
			vfio_dma_mem_map(default_vfio_cfg, vfio_va, vfio_va,
					len, 0);
		return;
	}

	/* memsegs are contiguous in memory */
	ms = rte_mem_virt2memseg(addr, msl);
	while (cur_len < len) {
		if (type == RTE_MEM_EVENT_ALLOC)
			vfio_dma_mem_map(default_vfio_cfg, ms->addr_64,
					ms->iova, ms->len, 1);
		else
			vfio_dma_mem_map(default_vfio_cfg, ms->addr_64,
					ms->iova, ms->len, 0);

		cur_len += ms->len;
		++ms;
	}
}

int
rte_vfio_clear_group(int vfio_group_fd)
{
	int i;
	struct rte_mp_msg mp_req, *mp_rep;
	struct rte_mp_reply mp_reply;
	struct timespec ts = {.tv_sec = 5, .tv_nsec = 0};
	struct vfio_mp_param *p = (struct vfio_mp_param *)mp_req.param;
	struct vfio_config *vfio_cfg;

	vfio_cfg = get_vfio_cfg_by_group_fd(vfio_group_fd);
	if (vfio_cfg == NULL) {
		RTE_LOG(ERR, EAL, "  invalid group fd!\n");
		return -1;
	}

	if (internal_config.process_type == RTE_PROC_PRIMARY) {

		i = get_vfio_group_idx(vfio_group_fd);
		if (i < 0)
			return -1;
		vfio_cfg->vfio_groups[i].group_num = -1;
		vfio_cfg->vfio_groups[i].fd = -1;
		vfio_cfg->vfio_groups[i].devices = 0;
		vfio_cfg->vfio_active_groups--;
		return 0;
	}

	p->req = SOCKET_CLR_GROUP;
	p->group_num = vfio_group_fd;
	strcpy(mp_req.name, EAL_VFIO_MP);
	mp_req.len_param = sizeof(*p);
	mp_req.num_fds = 0;

	if (rte_mp_request_sync(&mp_req, &mp_reply, &ts) == 0 &&
	    mp_reply.nb_received == 1) {
		mp_rep = &mp_reply.msgs[0];
		p = (struct vfio_mp_param *)mp_rep->param;
		if (p->result == SOCKET_OK) {
			free(mp_reply.msgs);
			return 0;
		} else if (p->result == SOCKET_NO_FD)
			RTE_LOG(ERR, EAL, "  BAD VFIO group fd!\n");
		else
			RTE_LOG(ERR, EAL, "  no such VFIO group fd!\n");

		free(mp_reply.msgs);
	}

	return -1;
}

int
rte_vfio_setup_device(const char *sysfs_base, const char *dev_addr,
		int *vfio_dev_fd, struct vfio_device_info *device_info)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	rte_rwlock_t *mem_lock = &mcfg->memory_hotplug_lock;
	struct vfio_group_status group_status = {
			.argsz = sizeof(group_status)
	};
	struct vfio_config *vfio_cfg;
	struct user_mem_maps *user_mem_maps;
	int vfio_container_fd;
	int vfio_group_fd;
	int iommu_group_num;
	int i, ret;

	/* get group number */
	ret = rte_vfio_get_group_num(sysfs_base, dev_addr, &iommu_group_num);
	if (ret == 0) {
		RTE_LOG(WARNING, EAL, "  %s not managed by VFIO driver, skipping\n",
			dev_addr);
		return 1;
	}

	/* if negative, something failed */
	if (ret < 0)
		return -1;

	/* get the actual group fd */
	vfio_group_fd = rte_vfio_get_group_fd(iommu_group_num);
	if (vfio_group_fd < 0)
		return -1;

	/* if group_fd == 0, that means the device isn't managed by VFIO */
	if (vfio_group_fd == 0) {
		RTE_LOG(WARNING, EAL, " %s not managed by VFIO driver, skipping\n",
				dev_addr);
		return 1;
	}

	/*
	 * at this point, we know that this group is viable (meaning, all devices
	 * are either bound to VFIO or not bound to anything)
	 */

	/* check if the group is viable */
	ret = ioctl(vfio_group_fd, VFIO_GROUP_GET_STATUS, &group_status);
	if (ret) {
		RTE_LOG(ERR, EAL, "  %s cannot get group status, "
				"error %i (%s)\n", dev_addr, errno, strerror(errno));
		close(vfio_group_fd);
		rte_vfio_clear_group(vfio_group_fd);
		return -1;
	} else if (!(group_status.flags & VFIO_GROUP_FLAGS_VIABLE)) {
		RTE_LOG(ERR, EAL, "  %s VFIO group is not viable!\n", dev_addr);
		close(vfio_group_fd);
		rte_vfio_clear_group(vfio_group_fd);
		return -1;
	}

	/* get the vfio_config it belongs to */
	vfio_cfg = get_vfio_cfg_by_group_num(iommu_group_num);
	vfio_cfg = vfio_cfg ? vfio_cfg : default_vfio_cfg;
	vfio_container_fd = vfio_cfg->vfio_container_fd;
	user_mem_maps = &vfio_cfg->mem_maps;

	/* check if group does not have a container yet */
	if (!(group_status.flags & VFIO_GROUP_FLAGS_CONTAINER_SET)) {

		/* add group to a container */
		ret = ioctl(vfio_group_fd, VFIO_GROUP_SET_CONTAINER,
				&vfio_container_fd);
		if (ret) {
			RTE_LOG(ERR, EAL, "  %s cannot add VFIO group to container, "
					"error %i (%s)\n", dev_addr, errno, strerror(errno));
			close(vfio_group_fd);
			rte_vfio_clear_group(vfio_group_fd);
			return -1;
		}

		/*
		 * pick an IOMMU type and set up DMA mappings for container
		 *
		 * needs to be done only once, only when first group is
		 * assigned to a container and only in primary process.
		 * Note this can happen several times with the hotplug
		 * functionality.
		 */
		if (internal_config.process_type == RTE_PROC_PRIMARY &&
				vfio_cfg->vfio_active_groups == 1 &&
				vfio_group_device_count(vfio_group_fd) == 0) {
			const struct vfio_iommu_type *t;

			/* select an IOMMU type which we will be using */
			t = vfio_set_iommu_type(vfio_container_fd);
			if (!t) {
				RTE_LOG(ERR, EAL,
					"  %s failed to select IOMMU type\n",
					dev_addr);
				close(vfio_group_fd);
				rte_vfio_clear_group(vfio_group_fd);
				return -1;
			}
			/* lock memory hotplug before mapping and release it
			 * after registering callback, to prevent races
			 */
			rte_rwlock_read_lock(mem_lock);
			if (vfio_cfg == default_vfio_cfg)
				ret = t->dma_map_func(vfio_container_fd);
			else
				ret = 0;
			if (ret) {
				RTE_LOG(ERR, EAL,
					"  %s DMA remapping failed, error %i (%s)\n",
					dev_addr, errno, strerror(errno));
				close(vfio_group_fd);
				rte_vfio_clear_group(vfio_group_fd);
				rte_rwlock_read_unlock(mem_lock);
				return -1;
			}

			vfio_cfg->vfio_iommu_type = t;

			/* re-map all user-mapped segments */
			rte_spinlock_recursive_lock(&user_mem_maps->lock);

			/* this IOMMU type may not support DMA mapping, but
			 * if we have mappings in the list - that means we have
			 * previously mapped something successfully, so we can
			 * be sure that DMA mapping is supported.
			 */
			for (i = 0; i < user_mem_maps->n_maps; i++) {
				struct user_mem_map *map;
				map = &user_mem_maps->maps[i];

				ret = t->dma_user_map_func(
						vfio_container_fd,
						map->addr, map->iova, map->len,
						1);
				if (ret) {
					RTE_LOG(ERR, EAL, "Couldn't map user memory for DMA: "
							"va: 0x%" PRIx64 " "
							"iova: 0x%" PRIx64 " "
							"len: 0x%" PRIu64 "\n",
							map->addr, map->iova,
							map->len);
					rte_spinlock_recursive_unlock(
							&user_mem_maps->lock);
					rte_rwlock_read_unlock(mem_lock);
					return -1;
				}
			}
			rte_spinlock_recursive_unlock(&user_mem_maps->lock);

			/* register callback for mem events */
			if (vfio_cfg == default_vfio_cfg)
				ret = rte_mem_event_callback_register(
					VFIO_MEM_EVENT_CLB_NAME,
					vfio_mem_event_callback, NULL);
			else
				ret = 0;
			/* unlock memory hotplug */
			rte_rwlock_read_unlock(mem_lock);

			if (ret && rte_errno != ENOTSUP) {
				RTE_LOG(ERR, EAL, "Could not install memory event callback for VFIO\n");
				return -1;
			}
			if (ret)
				RTE_LOG(DEBUG, EAL, "Memory event callbacks not supported\n");
			else
				RTE_LOG(DEBUG, EAL, "Installed memory event callback for VFIO\n");
		}
	}

	/* get a file descriptor for the device */
	*vfio_dev_fd = ioctl(vfio_group_fd, VFIO_GROUP_GET_DEVICE_FD, dev_addr);
	if (*vfio_dev_fd < 0) {
		/* if we cannot get a device fd, this implies a problem with
		 * the VFIO group or the container not having IOMMU configured.
		 */

		RTE_LOG(WARNING, EAL, "Getting a vfio_dev_fd for %s failed\n",
				dev_addr);
		close(vfio_group_fd);
		rte_vfio_clear_group(vfio_group_fd);
		return -1;
	}

	/* test and setup the device */
	ret = ioctl(*vfio_dev_fd, VFIO_DEVICE_GET_INFO, device_info);
	if (ret) {
		RTE_LOG(ERR, EAL, "  %s cannot get device info, "
				"error %i (%s)\n", dev_addr, errno,
				strerror(errno));
		close(*vfio_dev_fd);
		close(vfio_group_fd);
		rte_vfio_clear_group(vfio_group_fd);
		return -1;
	}
	vfio_group_device_get(vfio_group_fd);

	return 0;
}

int
rte_vfio_release_device(const char *sysfs_base, const char *dev_addr,
		    int vfio_dev_fd)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	rte_rwlock_t *mem_lock = &mcfg->memory_hotplug_lock;
	struct vfio_group_status group_status = {
			.argsz = sizeof(group_status)
	};
	struct vfio_config *vfio_cfg;
	int vfio_group_fd;
	int iommu_group_num;
	int ret;

	/* we don't want any DMA mapping messages to come while we're detaching
	 * VFIO device, because this might be the last device and we might need
	 * to unregister the callback.
	 */
	rte_rwlock_read_lock(mem_lock);

	/* get group number */
	ret = rte_vfio_get_group_num(sysfs_base, dev_addr, &iommu_group_num);
	if (ret <= 0) {
		RTE_LOG(WARNING, EAL, "  %s not managed by VFIO driver\n",
			dev_addr);
		/* This is an error at this point. */
		ret = -1;
		goto out;
	}

	/* get the actual group fd */
	vfio_group_fd = rte_vfio_get_group_fd(iommu_group_num);
	if (vfio_group_fd <= 0) {
		RTE_LOG(INFO, EAL, "rte_vfio_get_group_fd failed for %s\n",
				   dev_addr);
		ret = -1;
		goto out;
	}

	/* get the vfio_config it belongs to */
	vfio_cfg = get_vfio_cfg_by_group_num(iommu_group_num);
	vfio_cfg = vfio_cfg ? vfio_cfg : default_vfio_cfg;

	/* At this point we got an active group. Closing it will make the
	 * container detachment. If this is the last active group, VFIO kernel
	 * code will unset the container and the IOMMU mappings.
	 */

	/* Closing a device */
	if (close(vfio_dev_fd) < 0) {
		RTE_LOG(INFO, EAL, "Error when closing vfio_dev_fd for %s\n",
				   dev_addr);
		ret = -1;
		goto out;
	}

	/* An VFIO group can have several devices attached. Just when there is
	 * no devices remaining should the group be closed.
	 */
	vfio_group_device_put(vfio_group_fd);
	if (!vfio_group_device_count(vfio_group_fd)) {

		if (close(vfio_group_fd) < 0) {
			RTE_LOG(INFO, EAL, "Error when closing vfio_group_fd for %s\n",
				dev_addr);
			ret = -1;
			goto out;
		}

		if (rte_vfio_clear_group(vfio_group_fd) < 0) {
			RTE_LOG(INFO, EAL, "Error when clearing group for %s\n",
					   dev_addr);
			ret = -1;
			goto out;
		}
	}

	/* if there are no active device groups, unregister the callback to
	 * avoid spurious attempts to map/unmap memory from VFIO.
	 */
	if (vfio_cfg == default_vfio_cfg && vfio_cfg->vfio_active_groups == 0)
		rte_mem_event_callback_unregister(VFIO_MEM_EVENT_CLB_NAME,
				NULL);

	/* success */
	ret = 0;

out:
	rte_rwlock_read_unlock(mem_lock);
	return ret;
}

int
rte_vfio_enable(const char *modname)
{
	/* initialize group list */
	int i, j;
	int vfio_available;

	rte_spinlock_recursive_t lock = RTE_SPINLOCK_RECURSIVE_INITIALIZER;

	for (i = 0; i < VFIO_MAX_CONTAINERS; i++) {
		vfio_cfgs[i].vfio_container_fd = -1;
		vfio_cfgs[i].vfio_active_groups = 0;
		vfio_cfgs[i].vfio_iommu_type = NULL;
		vfio_cfgs[i].mem_maps.lock = lock;

		for (j = 0; j < VFIO_MAX_GROUPS; j++) {
			vfio_cfgs[i].vfio_groups[j].fd = -1;
			vfio_cfgs[i].vfio_groups[j].group_num = -1;
			vfio_cfgs[i].vfio_groups[j].devices = 0;
		}
	}

	/* inform the user that we are probing for VFIO */
	RTE_LOG(INFO, EAL, "Probing VFIO support...\n");

	/* check if vfio module is loaded */
	vfio_available = rte_eal_check_module(modname);

	/* return error directly */
	if (vfio_available == -1) {
		RTE_LOG(INFO, EAL, "Could not get loaded module details!\n");
		return -1;
	}

	/* return 0 if VFIO modules not loaded */
	if (vfio_available == 0) {
		RTE_LOG(DEBUG, EAL, "VFIO modules not loaded, "
			"skipping VFIO support...\n");
		return 0;
	}

	default_vfio_cfg->vfio_container_fd = rte_vfio_get_container_fd();

	/* check if we have VFIO driver enabled */
	if (default_vfio_cfg->vfio_container_fd != -1) {
		RTE_LOG(NOTICE, EAL, "VFIO support initialized\n");
		default_vfio_cfg->vfio_enabled = 1;
	} else {
		RTE_LOG(NOTICE, EAL, "VFIO support could not be initialized\n");
	}

	return 0;
}

int
rte_vfio_is_enabled(const char *modname)
{
	const int mod_available = rte_eal_check_module(modname) > 0;
	return default_vfio_cfg->vfio_enabled && mod_available;
}

const struct vfio_iommu_type *
vfio_set_iommu_type(int vfio_container_fd)
{
	unsigned idx;
	for (idx = 0; idx < RTE_DIM(iommu_types); idx++) {
		const struct vfio_iommu_type *t = &iommu_types[idx];

		int ret = ioctl(vfio_container_fd, VFIO_SET_IOMMU,
				t->type_id);
		if (!ret) {
			RTE_LOG(NOTICE, EAL, "  using IOMMU type %d (%s)\n",
					t->type_id, t->name);
			return t;
		}
		/* not an error, there may be more supported IOMMU types */
		RTE_LOG(DEBUG, EAL, "  set IOMMU type %d (%s) failed, "
				"error %i (%s)\n", t->type_id, t->name, errno,
				strerror(errno));
	}
	/* if we didn't find a suitable IOMMU type, fail */
	return NULL;
}

int
vfio_has_supported_extensions(int vfio_container_fd)
{
	int ret;
	unsigned idx, n_extensions = 0;
	for (idx = 0; idx < RTE_DIM(iommu_types); idx++) {
		const struct vfio_iommu_type *t = &iommu_types[idx];

		ret = ioctl(vfio_container_fd, VFIO_CHECK_EXTENSION,
				t->type_id);
		if (ret < 0) {
			RTE_LOG(ERR, EAL, "  could not get IOMMU type, "
				"error %i (%s)\n", errno,
				strerror(errno));
			close(vfio_container_fd);
			return -1;
		} else if (ret == 1) {
			/* we found a supported extension */
			n_extensions++;
		}
		RTE_LOG(DEBUG, EAL, "  IOMMU type %d (%s) is %s\n",
				t->type_id, t->name,
				ret ? "supported" : "not supported");
	}

	/* if we didn't find any supported IOMMU types, fail */
	if (!n_extensions) {
		close(vfio_container_fd);
		return -1;
	}

	return 0;
}

int
rte_vfio_get_container_fd(void)
{
	int ret, vfio_container_fd;
	struct rte_mp_msg mp_req, *mp_rep;
	struct rte_mp_reply mp_reply;
	struct timespec ts = {.tv_sec = 5, .tv_nsec = 0};
	struct vfio_mp_param *p = (struct vfio_mp_param *)mp_req.param;


	/* if we're in a primary process, try to open the container */
	if (internal_config.process_type == RTE_PROC_PRIMARY) {
		vfio_container_fd = open(VFIO_CONTAINER_PATH, O_RDWR);
		if (vfio_container_fd < 0) {
			RTE_LOG(ERR, EAL, "  cannot open VFIO container, "
					"error %i (%s)\n", errno, strerror(errno));
			return -1;
		}

		/* check VFIO API version */
		ret = ioctl(vfio_container_fd, VFIO_GET_API_VERSION);
		if (ret != VFIO_API_VERSION) {
			if (ret < 0)
				RTE_LOG(ERR, EAL, "  could not get VFIO API version, "
						"error %i (%s)\n", errno, strerror(errno));
			else
				RTE_LOG(ERR, EAL, "  unsupported VFIO API version!\n");
			close(vfio_container_fd);
			return -1;
		}

		ret = vfio_has_supported_extensions(vfio_container_fd);
		if (ret) {
			RTE_LOG(ERR, EAL, "  no supported IOMMU "
					"extensions found!\n");
			return -1;
		}

		return vfio_container_fd;
	}
	/*
	 * if we're in a secondary process, request container fd from the
	 * primary process via mp channel
	 */
	p->req = SOCKET_REQ_CONTAINER;
	strcpy(mp_req.name, EAL_VFIO_MP);
	mp_req.len_param = sizeof(*p);
	mp_req.num_fds = 0;

	vfio_container_fd = -1;
	if (rte_mp_request_sync(&mp_req, &mp_reply, &ts) == 0 &&
	    mp_reply.nb_received == 1) {
		mp_rep = &mp_reply.msgs[0];
		p = (struct vfio_mp_param *)mp_rep->param;
		if (p->result == SOCKET_OK && mp_rep->num_fds == 1) {
			free(mp_reply.msgs);
			return mp_rep->fds[0];
		}
		free(mp_reply.msgs);
	}

	RTE_LOG(ERR, EAL, "  cannot request container fd\n");
	return -1;
}

int
rte_vfio_get_group_num(const char *sysfs_base,
		const char *dev_addr, int *iommu_group_num)
{
	char linkname[PATH_MAX];
	char filename[PATH_MAX];
	char *tok[16], *group_tok, *end;
	int ret;

	memset(linkname, 0, sizeof(linkname));
	memset(filename, 0, sizeof(filename));

	/* try to find out IOMMU group for this device */
	snprintf(linkname, sizeof(linkname),
			 "%s/%s/iommu_group", sysfs_base, dev_addr);

	ret = readlink(linkname, filename, sizeof(filename));

	/* if the link doesn't exist, no VFIO for us */
	if (ret < 0)
		return 0;

	ret = rte_strsplit(filename, sizeof(filename),
			tok, RTE_DIM(tok), '/');

	if (ret <= 0) {
		RTE_LOG(ERR, EAL, "  %s cannot get IOMMU group\n", dev_addr);
		return -1;
	}

	/* IOMMU group is always the last token */
	errno = 0;
	group_tok = tok[ret - 1];
	end = group_tok;
	*iommu_group_num = strtol(group_tok, &end, 10);
	if ((end != group_tok && *end != '\0') || errno != 0) {
		RTE_LOG(ERR, EAL, "  %s error parsing IOMMU number!\n", dev_addr);
		return -1;
	}

	return 1;
}

static int
type1_map(const struct rte_memseg_list *msl __rte_unused,
		const struct rte_memseg *ms, void *arg)
{
	int *vfio_container_fd = arg;

	return vfio_type1_dma_mem_map(*vfio_container_fd, ms->addr_64, ms->iova,
			ms->len, 1);
}

static int
vfio_type1_dma_mem_map(int vfio_container_fd, uint64_t vaddr, uint64_t iova,
		uint64_t len, int do_map)
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
		dma_map.flags = VFIO_DMA_MAP_FLAG_READ |
				VFIO_DMA_MAP_FLAG_WRITE;

		ret = ioctl(vfio_container_fd, VFIO_IOMMU_MAP_DMA, &dma_map);
		if (ret) {
			RTE_LOG(ERR, EAL, "  cannot set up DMA remapping, error %i (%s)\n",
				errno, strerror(errno));
				return -1;
		}
	} else {
		memset(&dma_unmap, 0, sizeof(dma_unmap));
		dma_unmap.argsz = sizeof(struct vfio_iommu_type1_dma_unmap);
		dma_unmap.size = len;
		dma_unmap.iova = iova;

		ret = ioctl(vfio_container_fd, VFIO_IOMMU_UNMAP_DMA,
				&dma_unmap);
		if (ret) {
			RTE_LOG(ERR, EAL, "  cannot clear DMA remapping, error %i (%s)\n",
					errno, strerror(errno));
			return -1;
		}
	}

	return 0;
}

static int
vfio_type1_dma_map(int vfio_container_fd)
{
	return rte_memseg_walk(type1_map, &vfio_container_fd);
}

static int
vfio_spapr_dma_do_map(int vfio_container_fd, uint64_t vaddr, uint64_t iova,
		uint64_t len, int do_map)
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
		dma_map.flags = VFIO_DMA_MAP_FLAG_READ |
				VFIO_DMA_MAP_FLAG_WRITE;

		ret = ioctl(vfio_container_fd, VFIO_IOMMU_MAP_DMA, &dma_map);
		if (ret) {
			RTE_LOG(ERR, EAL, "  cannot set up DMA remapping, error %i (%s)\n",
				errno, strerror(errno));
				return -1;
		}

	} else {
		struct vfio_iommu_spapr_register_memory reg = {
			.argsz = sizeof(reg),
			.flags = 0
		};
		reg.vaddr = (uintptr_t) vaddr;
		reg.size = len;

		ret = ioctl(vfio_container_fd,
				VFIO_IOMMU_SPAPR_UNREGISTER_MEMORY, &reg);
		if (ret) {
			RTE_LOG(ERR, EAL, "  cannot unregister vaddr for IOMMU, error %i (%s)\n",
					errno, strerror(errno));
			return -1;
		}

		memset(&dma_unmap, 0, sizeof(dma_unmap));
		dma_unmap.argsz = sizeof(struct vfio_iommu_type1_dma_unmap);
		dma_unmap.size = len;
		dma_unmap.iova = iova;

		ret = ioctl(vfio_container_fd, VFIO_IOMMU_UNMAP_DMA,
				&dma_unmap);
		if (ret) {
			RTE_LOG(ERR, EAL, "  cannot clear DMA remapping, error %i (%s)\n",
					errno, strerror(errno));
			return -1;
		}
	}

	return 0;
}

static int
vfio_spapr_map_walk(const struct rte_memseg_list *msl __rte_unused,
		const struct rte_memseg *ms, void *arg)
{
	int *vfio_container_fd = arg;

	return vfio_spapr_dma_mem_map(*vfio_container_fd, ms->addr_64, ms->iova,
			ms->len, 1);
}

struct spapr_walk_param {
	uint64_t window_size;
	uint64_t hugepage_sz;
};
static int
vfio_spapr_window_size_walk(const struct rte_memseg_list *msl __rte_unused,
		const struct rte_memseg *ms, void *arg)
{
	struct spapr_walk_param *param = arg;
	uint64_t max = ms->iova + ms->len;

	if (max > param->window_size) {
		param->hugepage_sz = ms->hugepage_sz;
		param->window_size = max;
	}

	return 0;
}

static int
vfio_spapr_create_new_dma_window(int vfio_container_fd,
		struct vfio_iommu_spapr_tce_create *create) {
	struct vfio_iommu_spapr_tce_remove remove = {
		.argsz = sizeof(remove),
	};
	struct vfio_iommu_spapr_tce_info info = {
		.argsz = sizeof(info),
	};
	int ret;

	/* query spapr iommu info */
	ret = ioctl(vfio_container_fd, VFIO_IOMMU_SPAPR_TCE_GET_INFO, &info);
	if (ret) {
		RTE_LOG(ERR, EAL, "  cannot get iommu info, "
				"error %i (%s)\n", errno, strerror(errno));
		return -1;
	}

	/* remove default DMA of 32 bit window */
	remove.start_addr = info.dma32_window_start;
	ret = ioctl(vfio_container_fd, VFIO_IOMMU_SPAPR_TCE_REMOVE, &remove);
	if (ret) {
		RTE_LOG(ERR, EAL, "  cannot remove default DMA window, "
				"error %i (%s)\n", errno, strerror(errno));
		return -1;
	}

	/* create new DMA window */
	ret = ioctl(vfio_container_fd, VFIO_IOMMU_SPAPR_TCE_CREATE, create);
	if (ret) {
		RTE_LOG(ERR, EAL, "  cannot create new DMA window, "
				"error %i (%s)\n", errno, strerror(errno));
		return -1;
	}

	if (create->start_addr != 0) {
		RTE_LOG(ERR, EAL, "  DMA window start address != 0\n");
		return -1;
	}

	return 0;
}

static int
vfio_spapr_dma_mem_map(int vfio_container_fd, uint64_t vaddr, uint64_t iova,
		uint64_t len, int do_map)
{
	struct spapr_walk_param param;
	struct vfio_iommu_spapr_tce_create create = {
		.argsz = sizeof(create),
	};
	struct vfio_config *vfio_cfg;
	struct user_mem_maps *user_mem_maps;
	int i, ret = 0;

	vfio_cfg = get_vfio_cfg_by_container_fd(vfio_container_fd);
	if (vfio_cfg == NULL) {
		RTE_LOG(ERR, EAL, "  invalid container fd!\n");
		return -1;
	}

	user_mem_maps = &vfio_cfg->mem_maps;
	rte_spinlock_recursive_lock(&user_mem_maps->lock);

	/* check if window size needs to be adjusted */
	memset(&param, 0, sizeof(param));

	if (memseg_walk_thread_unsafe(vfio_spapr_window_size_walk,
				&param) < 0) {
		RTE_LOG(ERR, EAL, "Could not get window size\n");
		ret = -1;
		goto out;
	}

	/* also check user maps */
	for (i = 0; i < user_mem_maps->n_maps; i++) {
		uint64_t max = user_mem_maps->maps[i].iova +
				user_mem_maps->maps[i].len;
		create.window_size = RTE_MAX(create.window_size, max);
	}

	/* sPAPR requires window size to be a power of 2 */
	create.window_size = rte_align64pow2(param.window_size);
	create.page_shift = __builtin_ctzll(param.hugepage_sz);
	create.levels = 1;

	if (do_map) {
		void *addr;
		/* re-create window and remap the entire memory */
		if (iova > create.window_size) {
			if (vfio_spapr_create_new_dma_window(vfio_container_fd,
					&create) < 0) {
				RTE_LOG(ERR, EAL, "Could not create new DMA window\n");
				ret = -1;
				goto out;
			}
			if (memseg_walk_thread_unsafe(vfio_spapr_map_walk,
					&vfio_container_fd) < 0) {
				RTE_LOG(ERR, EAL, "Could not recreate DMA maps\n");
				ret = -1;
				goto out;
			}
			/* remap all user maps */
			for (i = 0; i < user_mem_maps->n_maps; i++) {
				struct user_mem_map *map =
						&user_mem_maps->maps[i];
				if (vfio_spapr_dma_do_map(vfio_container_fd,
						map->addr, map->iova, map->len,
						1)) {
					RTE_LOG(ERR, EAL, "Could not recreate user DMA maps\n");
					ret = -1;
					goto out;
				}
			}
		}

		/* now that we've remapped all of the memory that was present
		 * before, map the segment that we were requested to map.
		 *
		 * however, if we were called by the callback, the memory we
		 * were called with was already in the memseg list, so previous
		 * mapping should've mapped that segment already.
		 *
		 * virt2memseg_list is a relatively cheap check, so use that. if
		 * memory is within any memseg list, it's a memseg, so it's
		 * already mapped.
		 */
		addr = (void *)(uintptr_t)vaddr;
		if (rte_mem_virt2memseg_list(addr) == NULL &&
				vfio_spapr_dma_do_map(vfio_container_fd,
					vaddr, iova, len, 1) < 0) {
			RTE_LOG(ERR, EAL, "Could not map segment\n");
			ret = -1;
			goto out;
		}
	} else {
		/* for unmap, check if iova within DMA window */
		if (iova > create.window_size) {
			RTE_LOG(ERR, EAL, "iova beyond DMA window for unmap");
			ret = -1;
			goto out;
		}

		vfio_spapr_dma_do_map(vfio_container_fd, vaddr, iova, len, 0);
	}
out:
	rte_spinlock_recursive_unlock(&user_mem_maps->lock);
	return ret;
}

static int
vfio_spapr_dma_map(int vfio_container_fd)
{
	struct vfio_iommu_spapr_tce_create create = {
		.argsz = sizeof(create),
	};
	struct spapr_walk_param param;

	memset(&param, 0, sizeof(param));

	/* create DMA window from 0 to max(phys_addr + len) */
	rte_memseg_walk(vfio_spapr_window_size_walk, &param);

	/* sPAPR requires window size to be a power of 2 */
	create.window_size = rte_align64pow2(param.window_size);
	create.page_shift = __builtin_ctzll(param.hugepage_sz);
	create.levels = 1;

	if (vfio_spapr_create_new_dma_window(vfio_container_fd, &create) < 0) {
		RTE_LOG(ERR, EAL, "Could not create new DMA window\n");
		return -1;
	}

	/* map all DPDK segments for DMA. use 1:1 PA to IOVA mapping */
	if (rte_memseg_walk(vfio_spapr_map_walk, &vfio_container_fd) < 0)
		return -1;

	return 0;
}

static int
vfio_noiommu_dma_map(int __rte_unused vfio_container_fd)
{
	/* No-IOMMU mode does not need DMA mapping */
	return 0;
}

static int
vfio_noiommu_dma_mem_map(int __rte_unused vfio_container_fd,
			 uint64_t __rte_unused vaddr,
			 uint64_t __rte_unused iova, uint64_t __rte_unused len,
			 int __rte_unused do_map)
{
	/* No-IOMMU mode does not need DMA mapping */
	return 0;
}

static int
vfio_dma_mem_map(struct vfio_config *vfio_cfg, uint64_t vaddr, uint64_t iova,
		uint64_t len, int do_map)
{
	const struct vfio_iommu_type *t = vfio_cfg->vfio_iommu_type;

	if (!t) {
		RTE_LOG(ERR, EAL, "  VFIO support not initialized\n");
		rte_errno = ENODEV;
		return -1;
	}

	if (!t->dma_user_map_func) {
		RTE_LOG(ERR, EAL,
			"  VFIO custom DMA region maping not supported by IOMMU %s\n",
			t->name);
		rte_errno = ENOTSUP;
		return -1;
	}

	return t->dma_user_map_func(vfio_cfg->vfio_container_fd, vaddr, iova,
			len, do_map);
}

static int
container_dma_map(struct vfio_config *vfio_cfg, uint64_t vaddr, uint64_t iova,
		uint64_t len)
{
	struct user_mem_map *new_map;
	struct user_mem_maps *user_mem_maps;
	int ret = 0;

	user_mem_maps = &vfio_cfg->mem_maps;
	rte_spinlock_recursive_lock(&user_mem_maps->lock);
	if (user_mem_maps->n_maps == VFIO_MAX_USER_MEM_MAPS) {
		RTE_LOG(ERR, EAL, "No more space for user mem maps\n");
		rte_errno = ENOMEM;
		ret = -1;
		goto out;
	}
	/* map the entry */
	if (vfio_dma_mem_map(vfio_cfg, vaddr, iova, len, 1)) {
		/* technically, this will fail if there are currently no devices
		 * plugged in, even if a device were added later, this mapping
		 * might have succeeded. however, since we cannot verify if this
		 * is a valid mapping without having a device attached, consider
		 * this to be unsupported, because we can't just store any old
		 * mapping and pollute list of active mappings willy-nilly.
		 */
		RTE_LOG(ERR, EAL, "Couldn't map new region for DMA\n");
		ret = -1;
		goto out;
	}
	/* create new user mem map entry */
	new_map = &user_mem_maps->maps[user_mem_maps->n_maps++];
	new_map->addr = vaddr;
	new_map->iova = iova;
	new_map->len = len;

	compact_user_maps(user_mem_maps);
out:
	rte_spinlock_recursive_unlock(&user_mem_maps->lock);
	return ret;
}

static int
container_dma_unmap(struct vfio_config *vfio_cfg, uint64_t vaddr, uint64_t iova,
		uint64_t len)
{
	struct user_mem_map *map, *new_map = NULL;
	struct user_mem_maps *user_mem_maps;
	int ret = 0;

	user_mem_maps = &vfio_cfg->mem_maps;
	rte_spinlock_recursive_lock(&user_mem_maps->lock);

	/* find our mapping */
	map = find_user_mem_map(user_mem_maps, vaddr, iova, len);
	if (!map) {
		RTE_LOG(ERR, EAL, "Couldn't find previously mapped region\n");
		rte_errno = EINVAL;
		ret = -1;
		goto out;
	}
	if (map->addr != vaddr || map->iova != iova || map->len != len) {
		/* we're partially unmapping a previously mapped region, so we
		 * need to split entry into two.
		 */
		if (user_mem_maps->n_maps == VFIO_MAX_USER_MEM_MAPS) {
			RTE_LOG(ERR, EAL, "Not enough space to store partial mapping\n");
			rte_errno = ENOMEM;
			ret = -1;
			goto out;
		}
		new_map = &user_mem_maps->maps[user_mem_maps->n_maps++];
	}

	/* unmap the entry */
	if (vfio_dma_mem_map(vfio_cfg, vaddr, iova, len, 0)) {
		/* there may not be any devices plugged in, so unmapping will
		 * fail with ENODEV/ENOTSUP rte_errno values, but that doesn't
		 * stop us from removing the mapping, as the assumption is we
		 * won't be needing this memory any more and thus will want to
		 * prevent it from being remapped again on hotplug. so, only
		 * fail if we indeed failed to unmap (e.g. if the mapping was
		 * within our mapped range but had invalid alignment).
		 */
		if (rte_errno != ENODEV && rte_errno != ENOTSUP) {
			RTE_LOG(ERR, EAL, "Couldn't unmap region for DMA\n");
			ret = -1;
			goto out;
		} else {
			RTE_LOG(DEBUG, EAL, "DMA unmapping failed, but removing mappings anyway\n");
		}
	}
	/* remove map from the list of active mappings */
	if (new_map != NULL) {
		adjust_map(map, new_map, vaddr, len);

		/* if we've created a new map by splitting, sort everything */
		if (!is_null_map(new_map)) {
			compact_user_maps(user_mem_maps);
		} else {
			/* we've created a new mapping, but it was unused */
			user_mem_maps->n_maps--;
		}
	} else {
		memset(map, 0, sizeof(*map));
		compact_user_maps(user_mem_maps);
		user_mem_maps->n_maps--;
	}

out:
	rte_spinlock_recursive_unlock(&user_mem_maps->lock);
	return ret;
}

int __rte_experimental
rte_vfio_dma_map(uint64_t vaddr, uint64_t iova, uint64_t len)
{
	if (len == 0) {
		rte_errno = EINVAL;
		return -1;
	}

	return container_dma_map(default_vfio_cfg, vaddr, iova, len);
}

int __rte_experimental
rte_vfio_dma_unmap(uint64_t vaddr, uint64_t iova, uint64_t len)
{
	if (len == 0) {
		rte_errno = EINVAL;
		return -1;
	}

	return container_dma_unmap(default_vfio_cfg, vaddr, iova, len);
}

int
rte_vfio_noiommu_is_enabled(void)
{
	int fd;
	ssize_t cnt;
	char c;

	fd = open(VFIO_NOIOMMU_MODE, O_RDONLY);
	if (fd < 0) {
		if (errno != ENOENT) {
			RTE_LOG(ERR, EAL, "  cannot open vfio noiommu file %i (%s)\n",
					errno, strerror(errno));
			return -1;
		}
		/*
		 * else the file does not exists
		 * i.e. noiommu is not enabled
		 */
		return 0;
	}

	cnt = read(fd, &c, 1);
	close(fd);
	if (cnt != 1) {
		RTE_LOG(ERR, EAL, "  unable to read from vfio noiommu "
				"file %i (%s)\n", errno, strerror(errno));
		return -1;
	}

	return c == 'Y';
}

int __rte_experimental
rte_vfio_container_create(void)
{
	int i;

	/* Find an empty slot to store new vfio config */
	for (i = 1; i < VFIO_MAX_CONTAINERS; i++) {
		if (vfio_cfgs[i].vfio_container_fd == -1)
			break;
	}

	if (i == VFIO_MAX_CONTAINERS) {
		RTE_LOG(ERR, EAL, "exceed max vfio container limit\n");
		return -1;
	}

	vfio_cfgs[i].vfio_container_fd = rte_vfio_get_container_fd();
	if (vfio_cfgs[i].vfio_container_fd < 0) {
		RTE_LOG(NOTICE, EAL, "fail to create a new container\n");
		return -1;
	}

	return vfio_cfgs[i].vfio_container_fd;
}

int __rte_experimental
rte_vfio_container_destroy(int container_fd)
{
	struct vfio_config *vfio_cfg;
	int i;

	vfio_cfg = get_vfio_cfg_by_container_fd(container_fd);
	if (vfio_cfg == NULL) {
		RTE_LOG(ERR, EAL, "Invalid container fd\n");
		return -1;
	}

	for (i = 0; i < VFIO_MAX_GROUPS; i++)
		if (vfio_cfg->vfio_groups[i].group_num != -1)
			rte_vfio_container_group_unbind(container_fd,
				vfio_cfg->vfio_groups[i].group_num);

	close(container_fd);
	vfio_cfg->vfio_container_fd = -1;
	vfio_cfg->vfio_active_groups = 0;
	vfio_cfg->vfio_iommu_type = NULL;

	return 0;
}

int __rte_experimental
rte_vfio_container_group_bind(int container_fd, int iommu_group_num)
{
	struct vfio_config *vfio_cfg;
	struct vfio_group *cur_grp;
	int vfio_group_fd;
	int i;

	vfio_cfg = get_vfio_cfg_by_container_fd(container_fd);
	if (vfio_cfg == NULL) {
		RTE_LOG(ERR, EAL, "Invalid container fd\n");
		return -1;
	}

	/* Check room for new group */
	if (vfio_cfg->vfio_active_groups == VFIO_MAX_GROUPS) {
		RTE_LOG(ERR, EAL, "Maximum number of VFIO groups reached!\n");
		return -1;
	}

	/* Get an index for the new group */
	for (i = 0; i < VFIO_MAX_GROUPS; i++)
		if (vfio_cfg->vfio_groups[i].group_num == -1) {
			cur_grp = &vfio_cfg->vfio_groups[i];
			break;
		}

	/* This should not happen */
	if (i == VFIO_MAX_GROUPS) {
		RTE_LOG(ERR, EAL, "No VFIO group free slot found\n");
		return -1;
	}

	vfio_group_fd = vfio_open_group_fd(iommu_group_num);
	if (vfio_group_fd < 0) {
		RTE_LOG(ERR, EAL, "Failed to open group %d\n", iommu_group_num);
		return -1;
	}
	cur_grp->group_num = iommu_group_num;
	cur_grp->fd = vfio_group_fd;
	cur_grp->devices = 0;
	vfio_cfg->vfio_active_groups++;

	return vfio_group_fd;
}

int __rte_experimental
rte_vfio_container_group_unbind(int container_fd, int iommu_group_num)
{
	struct vfio_config *vfio_cfg;
	struct vfio_group *cur_grp;
	int i;

	vfio_cfg = get_vfio_cfg_by_container_fd(container_fd);
	if (vfio_cfg == NULL) {
		RTE_LOG(ERR, EAL, "Invalid container fd\n");
		return -1;
	}

	for (i = 0; i < VFIO_MAX_GROUPS; i++) {
		if (vfio_cfg->vfio_groups[i].group_num == iommu_group_num) {
			cur_grp = &vfio_cfg->vfio_groups[i];
			break;
		}
	}

	/* This should not happen */
	if (i == VFIO_MAX_GROUPS) {
		RTE_LOG(ERR, EAL, "Specified group number not found\n");
		return -1;
	}

	if (cur_grp->fd >= 0 && close(cur_grp->fd) < 0) {
		RTE_LOG(ERR, EAL, "Error when closing vfio_group_fd for"
			" iommu_group_num %d\n", iommu_group_num);
		return -1;
	}
	cur_grp->group_num = -1;
	cur_grp->fd = -1;
	cur_grp->devices = 0;
	vfio_cfg->vfio_active_groups--;

	return 0;
}

int __rte_experimental
rte_vfio_container_dma_map(int container_fd, uint64_t vaddr, uint64_t iova,
		uint64_t len)
{
	struct vfio_config *vfio_cfg;

	if (len == 0) {
		rte_errno = EINVAL;
		return -1;
	}

	vfio_cfg = get_vfio_cfg_by_container_fd(container_fd);
	if (vfio_cfg == NULL) {
		RTE_LOG(ERR, EAL, "Invalid container fd\n");
		return -1;
	}

	return container_dma_map(vfio_cfg, vaddr, iova, len);
}

int __rte_experimental
rte_vfio_container_dma_unmap(int container_fd, uint64_t vaddr, uint64_t iova,
		uint64_t len)
{
	struct vfio_config *vfio_cfg;

	if (len == 0) {
		rte_errno = EINVAL;
		return -1;
	}

	vfio_cfg = get_vfio_cfg_by_container_fd(container_fd);
	if (vfio_cfg == NULL) {
		RTE_LOG(ERR, EAL, "Invalid container fd\n");
		return -1;
	}

	return container_dma_unmap(vfio_cfg, vaddr, iova, len);
}

#else

int __rte_experimental
rte_vfio_dma_map(uint64_t __rte_unused vaddr, __rte_unused uint64_t iova,
		  __rte_unused uint64_t len)
{
	return -1;
}

int __rte_experimental
rte_vfio_dma_unmap(uint64_t __rte_unused vaddr, uint64_t __rte_unused iova,
		    __rte_unused uint64_t len)
{
	return -1;
}

int
rte_vfio_setup_device(__rte_unused const char *sysfs_base,
		__rte_unused const char *dev_addr,
		__rte_unused int *vfio_dev_fd,
		__rte_unused struct vfio_device_info *device_info)
{
	return -1;
}

int
rte_vfio_release_device(__rte_unused const char *sysfs_base,
		__rte_unused const char *dev_addr, __rte_unused int fd)
{
	return -1;
}

int
rte_vfio_enable(__rte_unused const char *modname)
{
	return -1;
}

int
rte_vfio_is_enabled(__rte_unused const char *modname)
{
	return -1;
}

int
rte_vfio_noiommu_is_enabled(void)
{
	return -1;
}

int
rte_vfio_clear_group(__rte_unused int vfio_group_fd)
{
	return -1;
}

int __rte_experimental
rte_vfio_get_group_num(__rte_unused const char *sysfs_base,
		__rte_unused const char *dev_addr,
		__rte_unused int *iommu_group_num)
{
	return -1;
}

int __rte_experimental
rte_vfio_get_container_fd(void)
{
	return -1;
}

int __rte_experimental
rte_vfio_get_group_fd(__rte_unused int iommu_group_num)
{
	return -1;
}

int __rte_experimental
rte_vfio_container_create(void)
{
	return -1;
}

int __rte_experimental
rte_vfio_container_destroy(__rte_unused int container_fd)
{
	return -1;
}

int __rte_experimental
rte_vfio_container_group_bind(__rte_unused int container_fd,
		__rte_unused int iommu_group_num)
{
	return -1;
}

int __rte_experimental
rte_vfio_container_group_unbind(__rte_unused int container_fd,
		__rte_unused int iommu_group_num)
{
	return -1;
}

int __rte_experimental
rte_vfio_container_dma_map(__rte_unused int container_fd,
		__rte_unused uint64_t vaddr,
		__rte_unused uint64_t iova,
		__rte_unused uint64_t len)
{
	return -1;
}

int __rte_experimental
rte_vfio_container_dma_unmap(__rte_unused int container_fd,
		__rte_unused uint64_t vaddr,
		__rte_unused uint64_t iova,
		__rte_unused uint64_t len)
{
	return -1;
}

#endif /* VFIO_PRESENT */
