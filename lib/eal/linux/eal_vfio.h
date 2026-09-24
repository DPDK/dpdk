/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef EAL_VFIO_H_
#define EAL_VFIO_H_

#include <rte_common.h>
#include <rte_spinlock.h>

#include <stdint.h>

#include <dev_vfio.h>

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
	bool active;
	int group_num;
	int fd;
	int n_devices;
};

/* device tracking (common for group and cdev modes) */
struct vfio_device {
	bool active;
	enum dev_vfio_mode mode;
	int fd;
	union {
		struct {
			int dev_num;   /**< device number, e.g., X in /dev/vfio/devices/vfioX */
		};
		struct {
			int group; /**< back-reference to group list */
			char *sysfs_base; /**< sysfs path prefix */
			char *dev_addr;   /**< device address */
		};
	};
};

/* group mode specific configuration */
struct vfio_group_config {
	bool iommu_type_set;
	bool mem_event_clb_set;
	size_t n_groups;
	struct vfio_group groups[RTE_MAX_VFIO_GROUPS];
};

/* cdev mode specific configuration */
struct vfio_cdev_config {
	uint32_t ioas_id;
};

/* per-container configuration */
struct vfio_container {
	bool active;
	bool dma_setup_done;
	int container_fd;
	struct vfio_user_mem_maps mem_maps;
	union {
		struct vfio_group_config group_cfg;
		struct vfio_cdev_config cdev_cfg;
	};
	int n_devices;
	struct vfio_device devices[RTE_MAX_VFIO_DEVICES];
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

/* mode-independent ops */
struct vfio_iommu_ops {
	int type_id;
	const char *name;
	bool partial_unmap;
	vfio_dma_user_func_t dma_user_map_func;
	vfio_dma_func_t dma_map_func;
};

/* global configuration */
struct vfio_config {
	struct vfio_container *default_cfg;
	enum dev_vfio_mode mode;
	enum dev_vfio_iova_mode iova_mode;
	const struct vfio_iommu_ops *ops;
};

/* current configuration */
extern struct vfio_config vfio_global_cfg;

#define VFIO_GROUP_FOREACH(cfg, grp) \
	for ((grp) = &((cfg)->group_cfg.groups[0]); \
		(grp) < &((cfg)->group_cfg.groups[RTE_DIM((cfg)->group_cfg.groups)]); \
		(grp)++)

#define VFIO_GROUP_FOREACH_ACTIVE(cfg, grp) \
	VFIO_GROUP_FOREACH((cfg), (grp)) \
		if ((grp)->active)

#define VFIO_DEVICE_FOREACH(cfg, dev) \
	for ((dev) = &((cfg)->devices[0]); \
		(dev) < &((cfg)->devices[RTE_DIM((cfg)->devices)]); \
		(dev)++)

#define VFIO_DEVICE_FOREACH_ACTIVE(cfg, dev) \
	VFIO_DEVICE_FOREACH((cfg), (dev)) \
		if ((dev)->active)

int vfio_get_iommu_type(void);
int vfio_mp_sync_setup(void);
void vfio_mp_sync_cleanup(void);
bool vfio_container_is_default(struct vfio_container *cfg);

/* group mode functions */
int vfio_group_enable(struct vfio_container *cfg);
int vfio_group_open_container_fd(void);
int vfio_group_get_num(const char *sysfs_base, const char *dev_addr,
		int *iommu_group_num);
struct vfio_group *vfio_group_get_by_num(struct vfio_container *cfg, int iommu_group);
struct vfio_group *vfio_group_create(struct vfio_container *cfg, int iommu_group);
void vfio_group_erase(struct vfio_container *cfg, struct vfio_group *grp);
int vfio_group_open_fd(struct vfio_container *cfg, struct vfio_group *grp);
int vfio_group_prepare(struct vfio_container *cfg, struct vfio_group *grp);
int vfio_group_setup_iommu(struct vfio_container *cfg);
int vfio_group_setup_device_fd(const char *dev_addr, struct vfio_group *grp,
		struct vfio_device *dev);

/* cdev mode functions */
int vfio_cdev_enable(struct vfio_container *cfg);
void vfio_cdev_setup_ops(void);
int vfio_cdev_setup_ioas(struct vfio_container *cfg);
int vfio_cdev_sync_ioas(struct vfio_container *cfg);
int vfio_cdev_get_iommufd(void);
int vfio_cdev_get_device_num(const char *sysfs_base, const char *dev_addr,
		int *cdev_dev_num);
struct vfio_device *vfio_cdev_get_dev_by_num(struct vfio_container *cfg, int cdev_dev_num);
int vfio_cdev_setup_device(struct vfio_container *cfg, struct vfio_device *dev);

#define EAL_VFIO_MP "eal_vfio_mp_sync"

#define VFIO_SOCKET_REQ_CONTAINER 0x100
#define VFIO_SOCKET_REQ_GROUP 0x200
#define VFIO_SOCKET_REQ_IOMMU_TYPE 0x400
#define VFIO_SOCKET_REQ_IOVA_MODE 0x800
#define VFIO_SOCKET_REQ_CDEV 0x1000
#define VFIO_SOCKET_REQ_IOAS_ID 0x2000
#define VFIO_SOCKET_OK 0x0
#define VFIO_SOCKET_NO_FD 0x1
#define VFIO_SOCKET_ERR 0xFF

struct vfio_mp_param {
	int req;
	int result;
	union {
		int group_num;
		int iommu_type_id;
		int cdev_dev_num;
		int ioas_id;
		enum dev_vfio_mode mode;
		enum dev_vfio_iova_mode iova_mode;
	};
};

#endif /* EAL_VFIO_H_ */
