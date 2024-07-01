/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#ifndef _VIRTIO_HA_H_
#define _VIRTIO_HA_H_

#include <stdint.h>
#include <pthread.h>

#include <rte_pci.h>
#include <rte_uuid.h>
#include <rte_compat.h>

#define VIRTIO_HA_UDS_PATH "/tmp/virtio_ha_ipc"
#define VDPA_MAX_SOCK_LEN 108 /* Follow definition of struct sockaddr_un in sys/un.h */
#define VIRTIO_HA_MAX_FDS 3
#define VIRTIO_HA_MAX_MEM_REGIONS 8
#define VIRTIO_HA_VERSION_SIZE 64
#define VIRTIO_HA_TIME_SIZE 32

struct virtio_dev_name;

typedef void (*ctx_set_cb)(const struct virtio_dev_name *dev, const void *ctx, int vf_num_vm);
typedef void (*ctx_unset_cb)(const struct virtio_dev_name *dev);
typedef void (*fd_cb)(int fd, void *data);
typedef void (*ver_time_set)(char *version, char *buildtime);

enum virtio_ha_msg_type {
	VIRTIO_HA_APP_QUERY_VERSION = 0,
	VIRTIO_HA_APP_SET_PRIO_CHNL = 1,
	VIRTIO_HA_APP_REMOVE_PRIO_CHNL = 2,
	VIRTIO_HA_APP_QUERY_PF_LIST = 3,
	VIRTIO_HA_APP_QUERY_VF_LIST = 4,
	VIRTIO_HA_APP_QUERY_PF_CTX = 5,
	VIRTIO_HA_APP_QUERY_VF_CTX = 6,
	VIRTIO_HA_PF_STORE_CTX = 7,
	VIRTIO_HA_PF_REMOVE_CTX = 8,
	VIRTIO_HA_VF_STORE_DEVARG_VFIO_FDS = 9,
	VIRTIO_HA_VF_STORE_VHOST_FD = 10,
	VIRTIO_HA_VF_STORE_DMA_TBL = 11,
	VIRTIO_HA_VF_REMOVE_DEVARG_VFIO_FDS = 12,
	VIRTIO_HA_VF_REMOVE_VHOST_FD = 13,
	VIRTIO_HA_VF_REMOVE_DMA_TBL = 14,
	VIRTIO_HA_GLOBAL_STORE_CONTAINER = 15,
	VIRTIO_HA_GLOBAL_QUERY_CONTAINER = 16,
	VIRTIO_HA_GLOBAL_STORE_DMA_MAP = 17,
	VIRTIO_HA_GLOBAL_REMOVE_DMA_MAP = 18,
	VIRTIO_HA_GLOBAL_INIT_FINISH = 19,
	VIRTIO_HA_PRIO_CHNL_ADD_VF = 20,
	VIRTIO_HA_MESSAGE_MAX = 21,
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

struct virtio_ha_global_dma_map {
	uint64_t iova;
	uint64_t size;
};

struct virtio_ha_global_dma_entry {
	TAILQ_ENTRY(virtio_ha_global_dma_entry) next;
	struct virtio_ha_global_dma_map map;
};

TAILQ_HEAD(virtio_ha_global_dma_tbl, virtio_ha_global_dma_entry);

struct vdpa_vf_with_devargs {
    struct virtio_dev_name vf_name;
    char vhost_sock_addr[VDPA_MAX_SOCK_LEN];
    char vm_uuid[RTE_UUID_STRLEN];
	bool mem_tbl_set;
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

struct vdpa_vf_ctx_content {
	bool vhost_fd_saved;
    struct virtio_vdpa_dma_mem mem;
};

struct vdpa_vf_ctx {
    int vfio_container_fd;
    int vfio_group_fd;
    int vfio_device_fd;
	struct vdpa_vf_ctx_content ctt;
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
	struct virtio_ha_global_dma_tbl dma_tbl;
	uint32_t nr_pf;
	int global_cfd;
	int prio_chnl_fd;
	pthread_t prio_thread;
};

struct virtio_ha_dev_ctx_cb {
	ctx_set_cb set;
	ctx_unset_cb unset;
};

struct virtio_ha_vf_to_restore {
	TAILQ_ENTRY(virtio_ha_vf_to_restore) next;
	struct vdpa_vf_with_devargs vf_devargs;
	struct virtio_dev_name pf_name;
	int vf_cnt_vm; /* VF number in the same VM */
};

TAILQ_HEAD(virtio_ha_vf_restore_list, virtio_ha_vf_to_restore);

struct virtio_ha_vf_restore_queue {
	struct virtio_ha_vf_restore_list prio_q;
	struct virtio_ha_vf_restore_list non_prio_q;
	pthread_mutex_t lock;
};

struct virtio_ha_event_handler {
	void *data;
	int sock;
	fd_cb cb;
};

struct virtio_ha_version {
	char version[VIRTIO_HA_VERSION_SIZE];
	char time[VIRTIO_HA_TIME_SIZE];
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
int virtio_ha_ipc_client_init(ver_time_set set_ver);

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
int virtio_ha_pf_ctx_set(const struct virtio_dev_name *pf, const struct virtio_pf_ctx *ctx, int num);

/* App unset PF context from PF driver */
int virtio_ha_pf_ctx_unset(const struct virtio_dev_name *pf);

/* App set VF context to VF driver */
int virtio_ha_vf_ctx_set(const struct virtio_dev_name *vf, const struct vdpa_vf_ctx *ctx, int vf_cnt_vm);

/* App unset VF context from VF driver */
int virtio_ha_vf_ctx_unset(const struct virtio_dev_name *vf);

/* App set VF priority channel to HA service */
int virtio_ha_prio_chnl_init(struct virtio_ha_vf_restore_queue *rq);

/* App remove VF priority channel from HA service */
void virtio_ha_prio_chnl_destroy(void);


/* PF driver register PF context set/remove callbacks */
__rte_internal
void virtio_ha_pf_register_ctx_cb(const struct virtio_ha_dev_ctx_cb *ctx_cb);

/* PF driver store PF context to HA service */
__rte_internal
int virtio_ha_pf_ctx_store(const struct virtio_dev_name *pf, const struct virtio_pf_ctx *ctx);

/* PF driver remove PF context from HA service */
__rte_internal
int virtio_ha_pf_ctx_remove(const struct virtio_dev_name *pf);



/* VF driver register VF context set/remove callbacks */
__rte_internal
void virtio_ha_vf_register_ctx_cb(const struct virtio_ha_dev_ctx_cb *ctx_cb);

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

/* EAL layer store global VFIO container fd to HA service */
int virtio_ha_global_cfd_store(int vfio_container_fd);

/* EAL layer query global VFIO container fd from HA service */
int virtio_ha_global_cfd_query(int *vfio_container_fd);

/* EAL layer store global dma map of VFIO container fd to HA service */
int virtio_ha_global_dma_map_store(struct virtio_ha_global_dma_map *map);

/* EAL layer remove global dma map of VFIO container fd to HA service */
int virtio_ha_global_dma_map_remove(struct virtio_ha_global_dma_map *map);

/* App notify HA service that all devices are init */
int virtio_ha_global_init_finish(void);

#endif /* _VIRTIO_HA_H_ */
