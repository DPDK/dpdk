/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#ifndef _VIRTIO_HA_H_
#define _VIRTIO_HA_H_

#include <stdint.h>

#include <rte_pci.h>
#include <rte_uuid.h>
#include <rte_compat.h>

#define VDPA_MAX_SOCK_LEN 108 /* Follow definition of struct sockaddr_un in sys/un.h */

struct virtio_dev_name {
    char dev_bdf[PCI_PRI_STR_SIZE];
};

struct virtio_pf_ctx {
    int vfio_group_fd;
    int vfio_device_fd;
};

struct vdpa_vf_with_devargs {
    struct virtio_dev_name vf_name;
    char vhost_sock_addr[VDPA_MAX_SOCK_LEN];
    char vm_uuid[RTE_UUID_STRLEN];
};

struct virtio_vdpa_mem_region {
	uint64_t host_phys_addr;
	uint64_t guest_phys_addr;
	uint64_t size;
};

struct virtio_vdpa_dma_mem {
	uint32_t nregions;
	struct virtio_vdpa_mem_region regions[];
};

struct vdpa_vf_ctx {
    int vfio_container_fd;
    int vfio_group_fd;
    int vfio_device_fd;
    struct virtio_vdpa_dma_mem mem;
};

/* App query PF list from HA service, return number of PF */
int virtio_ha_pf_list_query(struct virtio_dev_name **list);

/* App query VF list of specific PF from HA service, return number of VF */
int virtio_ha_vf_list_query(const struct virtio_dev_name *pf,
    struct vdpa_vf_with_devargs **vf_list);

/* App query PF context from HA service */
int virtio_ha_pf_ctx_query(const struct virtio_dev_name *pf, struct virtio_pf_ctx *ctx);

/* App query VF context from HA service */
int virtio_ha_vf_ctx_query(struct virtio_dev_name *vf,
	const struct virtio_dev_name *pf, struct vdpa_vf_ctx **ctx);

/* App set PF context to PF driver */
int virtio_ha_pf_ctx_set(const struct virtio_dev_name *pf, const struct virtio_pf_ctx *ctx);

/* App unset PF context from PF driver */
int virtio_ha_pf_ctx_unset(const struct virtio_dev_name *pf);

/* App set VF context to VF driver */
int virtio_ha_vf_ctx_set(const struct virtio_dev_name *vf, const struct vdpa_vf_ctx *ctx);

/* App unset VF context from VF driver */
int virtio_ha_vf_ctx_unset(const struct virtio_dev_name *vf);


/* PF driver store PF context to HA service */
__rte_internal
int virtio_ha_pf_ctx_store(const struct virtio_dev_name *pf, const struct virtio_pf_ctx *ctx);

/* PF driver remove PF context from HA service */
__rte_internal
int virtio_ha_pf_ctx_remove(const struct virtio_dev_name *pf);



/* VF driver store VF devargs and VFIO fds to HA service */
__rte_internal
int virtio_ha_vf_devargs_fds_store(struct vdpa_vf_with_devargs *vf_dev,
    const struct virtio_dev_name *pf, int vfio_container_fd,
	int vfio_group_fd, int vfio_device_fd);

/* VF driver remove VF devargs and VFIO fds from HA service */
__rte_internal
int virtio_ha_vf_devargs_fds_remove(struct virtio_dev_name *vf,
	const struct virtio_dev_name *pf);

/* VF driver store VF vhost-user socket fd to HA service */
__rte_internal
int virtio_ha_vf_vhost_fd_store(struct virtio_dev_name *vf,
	const struct virtio_dev_name *pf, int fd);

/* VF driver remove VF vhost-user socket fd from HA service */
__rte_internal
int virtio_ha_vf_vhost_fd_remove(struct virtio_dev_name *vf, 
	const struct virtio_dev_name *pf);

/* VF driver store VF DMA memory table to HA service */
__rte_internal
int virtio_ha_vf_mem_tbl_store(const struct virtio_dev_name *vf,
    const struct virtio_dev_name *pf, const struct virtio_vdpa_dma_mem *mem);

/* VF driver remove VF DMA memory table from HA service */
__rte_internal
int virtio_ha_vf_mem_tbl_remove(struct virtio_dev_name *vf,
	const struct virtio_dev_name *pf);

#endif /* _VIRTIO_HA_H_ */
