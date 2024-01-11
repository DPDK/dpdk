/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#ifndef _VIRTIO_HA_H_
#define _VIRTIO_HA_H_

#include <stdint.h>

#include <rte_pci.h>
#include <rte_uuid.h>
#include <rte_compat.h>

#define VIRTIO_HA_UDS_PATH "/tmp/virtio_ha_ipc"
#define VDPA_MAX_SOCK_LEN 108 /* Follow definition of struct sockaddr_un in sys/un.h */
#define VIRTIO_HA_MAX_FDS 3

enum virtio_ha_msg_type {
	VIRTIO_HA_APP_QUERY_PF_LIST = 0,
	VIRTIO_HA_APP_QUERY_VF_LIST = 1,
	VIRTIO_HA_APP_QUERY_PF_CTX = 2,
	VIRTIO_HA_APP_QUERY_VF_CTX = 3,
	VIRTIO_HA_PF_STORE_CTX = 4,
	VIRTIO_HA_PF_REMOVE_CTX = 5,
	VIRTIO_HA_VF_STORE_DEVARG_VFIO_FDS = 6,
	VIRTIO_HA_VF_STORE_VHOST_FD = 7,
	VIRTIO_HA_VF_STORE_DMA_TBL = 8,
	VIRTIO_HA_VF_REMOVE_DEVARG_VFIO_FDS = 9,
	VIRTIO_HA_VF_REMOVE_VHOST_FD = 10,
	VIRTIO_HA_VF_REMOVE_DMA_TBL = 11,
	VIRTIO_HA_MESSAGE_MAX = 12,
};

struct virtio_ha_msg_hdr {
	uint32_t type; /* Message type defined in virtio_ha_msg_type */
	char bdf[PCI_PRI_STR_SIZE]; /* PCI BDF string of PF */
	uint32_t size; /* Payload size */
};

struct virtio_ha_msg {
	struct virtio_ha_msg_hdr hdr;
	struct iovec iov;
	int nr_fds;
	int fds[VIRTIO_HA_MAX_FDS];
};
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

struct virtio_ha_vf_dev {
	TAILQ_ENTRY(virtio_ha_vf_dev) next;
	struct vdpa_vf_with_devargs vf_devargs;
	int vhost_fd;
	struct vdpa_vf_ctx vf_ctx;
};

TAILQ_HEAD(virtio_ha_vf_dev_list, virtio_ha_vf_dev);

struct virtio_ha_pf_dev {
	TAILQ_ENTRY(virtio_ha_pf_dev) next;
	struct virtio_ha_vf_dev_list vf_list;
	uint32_t nr_vf;
	struct virtio_dev_name pf_name;
	struct virtio_pf_ctx pf_ctx;
};

TAILQ_HEAD(virtio_ha_pf_dev_list, virtio_ha_pf_dev);

struct virtio_ha_device_list {
	struct virtio_ha_pf_dev_list pf_list;
	uint32_t nr_pf;
};

/* IPC client/server allocate an HA message */
struct virtio_ha_msg *virtio_ha_alloc_msg(void);

/* IPC client/server free an HA message */
void virtio_ha_free_msg(struct virtio_ha_msg *msg);

/* IPC client/server reset this message so it can be re-used. This could avoid free and re-alloc */
void virtio_ha_reset_msg(struct virtio_ha_msg *msg);

/* IPC client/server send an HA message
 * return bytes# of send on success or negative val on failure
 */
int virtio_ha_send_msg(int sockfd, struct virtio_ha_msg *msg);

/* IPC client/server receive an HA message
 * return bytes# of recv on success or negative val on failure
 */
int virtio_ha_recv_msg(int sockfd, struct virtio_ha_msg *msg);

/* IPC client init
 * return 0 on success or negative val on failure
 */
int virtio_ha_ipc_client_init(void);

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
