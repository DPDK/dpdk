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

#ifndef EAL_PCI_INIT_H_
#define EAL_PCI_INIT_H_

#include "eal_vfio.h"

struct pci_map {
	void *addr;
	char *path;
	uint64_t offset;
	uint64_t size;
	uint64_t phaddr;
};

/*
 * For multi-process we need to reproduce all PCI mappings in secondary
 * processes, so save them in a tailq.
 */
struct mapped_pci_resource {
	TAILQ_ENTRY(mapped_pci_resource) next;

	struct rte_pci_addr pci_addr;
	char path[PATH_MAX];
	int nb_maps;
	struct pci_map maps[PCI_MAX_RESOURCE];
};

TAILQ_HEAD(mapped_pci_res_list, mapped_pci_resource);

/*
 * Helper function to map PCI resources right after hugepages in virtual memory
 */
extern void *pci_map_addr;
void *pci_find_max_end_va(void);

void *pci_map_resource(void *requested_addr, int fd, off_t offset,
	       size_t size, int additional_flags);

/* map IGB_UIO resource prototype */
int pci_uio_map_resource(struct rte_pci_device *dev);

void pci_unmap_resource(void *requested_addr, size_t size);

#ifdef RTE_LIBRTE_EAL_HOTPLUG
/* unmap IGB_UIO resource prototype */
void pci_uio_unmap_resource(struct rte_pci_device *dev);
#endif /* RTE_LIBRTE_EAL_HOTPLUG */

#ifdef VFIO_PRESENT

#define VFIO_MAX_GROUPS 64

int pci_vfio_enable(void);
int pci_vfio_is_enabled(void);
int pci_vfio_mp_sync_setup(void);

/* map VFIO resource prototype */
int pci_vfio_map_resource(struct rte_pci_device *dev);
int pci_vfio_get_group_fd(int iommu_group_fd);
int pci_vfio_get_container_fd(void);

/*
 * Function prototypes for VFIO multiprocess sync functions
 */
int vfio_mp_sync_send_request(int socket, int req);
int vfio_mp_sync_receive_request(int socket);
int vfio_mp_sync_send_fd(int socket, int fd);
int vfio_mp_sync_receive_fd(int socket);
int vfio_mp_sync_connect_to_primary(void);

/* socket comm protocol definitions */
#define SOCKET_REQ_CONTAINER 0x100
#define SOCKET_REQ_GROUP 0x200
#define SOCKET_OK 0x0
#define SOCKET_NO_FD 0x1
#define SOCKET_ERR 0xFF

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

#endif

#endif /* EAL_PCI_INIT_H_ */
