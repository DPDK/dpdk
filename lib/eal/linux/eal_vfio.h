/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef EAL_VFIO_H_
#define EAL_VFIO_H_

#include <rte_common.h>

#include <stdint.h>

/*
 * we don't need to store device fd's anywhere since they can be obtained from
 * the group fd via an ioctl() call.
 */
struct vfio_group {
	int group_num;
	int fd;
	int devices;
};

/* DMA mapping function prototype.
 * Takes VFIO container fd as a parameter.
 * Returns 0 on success, -1 on error.
 */
typedef int (*vfio_dma_func_t)(int);

/* Custom memory region DMA mapping function prototype.
 * Takes VFIO container fd, virtual address, physical address, length and
 * operation type (0 to unmap 1 for map) as a parameters.
 * Returns 0 on success, -1 on error.
 */
typedef int (*vfio_dma_user_func_t)(int fd, uint64_t vaddr, uint64_t iova,
		uint64_t len, int do_map);

struct vfio_iommu_type {
	int type_id;
	const char *name;
	bool partial_unmap;
	vfio_dma_user_func_t dma_user_map_func;
	vfio_dma_func_t dma_map_func;
};

/* get the vfio container that devices are bound to by default */
int vfio_open_container_fd(bool mp_request);

/* pick IOMMU type. returns a pointer to vfio_iommu_type or NULL for error */
const struct vfio_iommu_type *
vfio_set_iommu_type(int vfio_container_fd);

int
vfio_get_iommu_type(void);

/* check if we have any supported extensions */
int
vfio_has_supported_extensions(int vfio_container_fd);

int vfio_mp_sync_setup(void);
void vfio_mp_sync_cleanup(void);

#define EAL_VFIO_MP "eal_vfio_mp_sync"

#define SOCKET_REQ_CONTAINER 0x100
#define SOCKET_REQ_GROUP 0x200
#define SOCKET_REQ_IOMMU_TYPE 0x400
#define SOCKET_OK 0x0
#define SOCKET_NO_FD 0x1
#define SOCKET_ERR 0xFF

struct vfio_mp_param {
	int req;
	int result;
	union {
		int group_num;
		int iommu_type_id;
	};
};

#endif /* EAL_VFIO_H_ */
