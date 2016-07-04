/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef EAL_VFIO_H_
#define EAL_VFIO_H_

/*
 * determine if VFIO is present on the system
 */
#ifdef RTE_EAL_VFIO
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0)
#include <linux/vfio.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
#define RTE_PCI_MSIX_TABLE_BIR    0x7
#define RTE_PCI_MSIX_TABLE_OFFSET 0xfffffff8
#define RTE_PCI_MSIX_FLAGS_QSIZE  0x07ff
#else
#define RTE_PCI_MSIX_TABLE_BIR    PCI_MSIX_TABLE_BIR
#define RTE_PCI_MSIX_TABLE_OFFSET PCI_MSIX_TABLE_OFFSET
#define RTE_PCI_MSIX_FLAGS_QSIZE  PCI_MSIX_FLAGS_QSIZE
#endif

#define RTE_VFIO_TYPE1 VFIO_TYPE1_IOMMU

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
#define RTE_VFIO_NOIOMMU 8
#else
#define RTE_VFIO_NOIOMMU VFIO_NOIOMMU_IOMMU
#endif

#define VFIO_MAX_GROUPS 64

/*
 * Function prototypes for VFIO multiprocess sync functions
 */
int vfio_mp_sync_send_request(int socket, int req);
int vfio_mp_sync_receive_request(int socket);
int vfio_mp_sync_send_fd(int socket, int fd);
int vfio_mp_sync_receive_fd(int socket);
int vfio_mp_sync_connect_to_primary(void);

/*
 * we don't need to store device fd's anywhere since they can be obtained from
 * the group fd via an ioctl() call.
 */
struct vfio_group {
	int group_no;
	int fd;
};

struct vfio_config {
	int vfio_enabled;
	int vfio_container_fd;
	int vfio_container_has_dma;
	int vfio_group_idx;
	struct vfio_group vfio_groups[VFIO_MAX_GROUPS];
};

#define VFIO_DIR "/dev/vfio"
#define VFIO_CONTAINER_PATH "/dev/vfio/vfio"
#define VFIO_GROUP_FMT "/dev/vfio/%u"
#define VFIO_NOIOMMU_GROUP_FMT "/dev/vfio/noiommu-%u"
#define VFIO_GET_REGION_ADDR(x) ((uint64_t) x << 40ULL)
#define VFIO_GET_REGION_IDX(x) (x >> 40)

/* DMA mapping function prototype.
 * Takes VFIO container fd as a parameter.
 * Returns 0 on success, -1 on error.
 * */
typedef int (*vfio_dma_func_t)(int);

struct vfio_iommu_type {
	int type_id;
	const char *name;
	vfio_dma_func_t dma_map_func;
};

/* pick IOMMU type. returns a pointer to vfio_iommu_type or NULL for error */
const struct vfio_iommu_type *
vfio_set_iommu_type(int vfio_container_fd);

/* check if we have any supported extensions */
int
vfio_has_supported_extensions(int vfio_container_fd);

/* open container fd or get an existing one */
int
vfio_get_container_fd(void);

/* parse IOMMU group number for a device
 * returns 1 on success, -1 for errors, 0 for non-existent group
 */
int
vfio_get_group_no(const char *sysfs_base,
		const char *dev_addr, int *iommu_group_no);

/* open group fd or get an existing one */
int
vfio_get_group_fd(int iommu_group_no);

/**
 * Setup vfio_cfg for the device identified by its address. It discovers
 * the configured I/O MMU groups or sets a new one for the device. If a new
 * groups is assigned, the DMA mapping is performed.
 * Returns 0 on success, a negative value on failure and a positive value in
 * case the given device cannot be managed this way.
 */
int vfio_setup_device(const char *sysfs_base, const char *dev_addr,
		int *vfio_dev_fd, struct vfio_device_info *device_info);

int vfio_enable(const char *modname);
int vfio_is_enabled(const char *modname);

int pci_vfio_enable(void);
int pci_vfio_is_enabled(void);

int vfio_mp_sync_setup(void);

#define SOCKET_REQ_CONTAINER 0x100
#define SOCKET_REQ_GROUP 0x200
#define SOCKET_OK 0x0
#define SOCKET_NO_FD 0x1
#define SOCKET_ERR 0xFF

#define VFIO_PRESENT
#endif /* kernel version */
#endif /* RTE_EAL_VFIO */

#endif /* EAL_VFIO_H_ */
