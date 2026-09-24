/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef EAL_VFIO_H_
#define EAL_VFIO_H_

#include <rte_common.h>

#include <stdint.h>

/* hot plug/unplug of VFIO groups may cause all DMA maps to be dropped. we can
 * recreate the mappings for DPDK segments, but we cannot do so for memory that
 * was registered by the user themselves, so we need to store the user mappings
 * somewhere, to recreate them later.
 */
#define EAL_VFIO_MAX_USER_MEM_MAPS 256

/* user memory map entry */
struct vfio_user_mem_map {
	uint64_t addr;  /**< start VA */
	uint64_t iova;  /**< start IOVA */
	uint64_t len;   /**< total length of the mapping */
	uint64_t chunk; /**< this mapping can be split in chunks of this size */
};

/* user memory maps container (common for all API modes) */
struct vfio_user_mem_maps {
	rte_spinlock_recursive_t lock;
	int n_maps;
	struct vfio_user_mem_map maps[EAL_VFIO_MAX_USER_MEM_MAPS];
};

/*
 * we don't need to store device fd's anywhere since they can be obtained from
 * the group fd via an ioctl() call.
 */
struct vfio_group {
	int group_num;
	int fd;
	int devices;
};

struct vfio_container {
	int container_fd;
	int vfio_active_groups;
	struct vfio_group vfio_groups[RTE_MAX_VFIO_GROUPS];
	struct vfio_user_mem_maps mem_maps;
};

/* DMA mapping function prototype.
 * Takes VFIO container config as a parameter.
 * Returns 0 on success, -1 on error.
 */
typedef int (*vfio_dma_func_t)(struct vfio_container *cfg);

/* Custom memory region DMA mapping function prototype.
 * Takes VFIO container config, virtual address, physical address, length and
 * operation type (0 to unmap 1 for map) as a parameters.
 * Returns 0 on success, -1 on error.
 */
typedef int (*vfio_dma_user_func_t)(struct vfio_container *cfg, uint64_t vaddr, uint64_t iova,
		uint64_t len, int do_map);

struct vfio_iommu_ops {
	int type_id;
	const char *name;
	bool partial_unmap;
	vfio_dma_user_func_t dma_user_map_func;
	vfio_dma_func_t dma_map_func;
};

/* get the vfio container that devices are bound to by default */
int vfio_open_container_fd(bool mp_request);

/* global configuration */
struct vfio_config {
	struct vfio_container *default_cfg;
	const struct vfio_iommu_ops *ops;
};

/* current configuration */
extern struct vfio_config vfio_global_cfg;

/* pick IOMMU type. returns a pointer to vfio_iommu_ops or NULL for error */
const struct vfio_iommu_ops *
vfio_set_iommu_type(int vfio_container_fd);

int
vfio_get_iommu_type(void);

int vfio_get_group_fd_by_num(int iommu_group_num);

/* check if we have any supported extensions */
int
vfio_has_supported_extensions(int vfio_container_fd);

int vfio_mp_sync_setup(void);
void vfio_mp_sync_cleanup(void);

#define EAL_VFIO_MP "eal_vfio_mp_sync"

#define VFIO_SOCKET_REQ_CONTAINER 0x100
#define VFIO_SOCKET_REQ_GROUP 0x200
#define VFIO_SOCKET_REQ_IOMMU_TYPE 0x400
#define VFIO_SOCKET_OK 0x0
#define VFIO_SOCKET_NO_FD 0x1
#define VFIO_SOCKET_ERR 0xFF

struct vfio_mp_param {
	int req;
	int result;
	union {
		int group_num;
		int iommu_type_id;
	};
};

#endif /* EAL_VFIO_H_ */
