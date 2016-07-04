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

#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <rte_log.h>
#include <rte_memory.h>
#include <rte_eal_memconfig.h>

#include "eal_filesystem.h"
#include "eal_vfio.h"

#ifdef VFIO_PRESENT

const struct vfio_iommu_type *
vfio_set_iommu_type(int vfio_container_fd) {
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
vfio_has_supported_extensions(int vfio_container_fd) {
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
vfio_get_container_fd(void)
{
	int ret, vfio_container_fd;

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
	} else {
		/*
		 * if we're in a secondary process, request container fd from the
		 * primary process via our socket
		 */
		int socket_fd;

		socket_fd = vfio_mp_sync_connect_to_primary();
		if (socket_fd < 0) {
			RTE_LOG(ERR, EAL, "  cannot connect to primary process!\n");
			return -1;
		}
		if (vfio_mp_sync_send_request(socket_fd, SOCKET_REQ_CONTAINER) < 0) {
			RTE_LOG(ERR, EAL, "  cannot request container fd!\n");
			close(socket_fd);
			return -1;
		}
		vfio_container_fd = vfio_mp_sync_receive_fd(socket_fd);
		if (vfio_container_fd < 0) {
			RTE_LOG(ERR, EAL, "  cannot get container fd!\n");
			close(socket_fd);
			return -1;
		}
		close(socket_fd);
		return vfio_container_fd;
	}

	return -1;
}

int
vfio_get_group_no(const char *sysfs_base,
		const char *dev_addr, int *iommu_group_no)
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
	*iommu_group_no = strtol(group_tok, &end, 10);
	if ((end != group_tok && *end != '\0') || errno != 0) {
		RTE_LOG(ERR, EAL, "  %s error parsing IOMMU number!\n", dev_addr);
		return -1;
	}

	return 1;
}

int
vfio_type1_dma_map(int vfio_container_fd)
{
	const struct rte_memseg *ms = rte_eal_get_physmem_layout();
	int i, ret;

	/* map all DPDK segments for DMA. use 1:1 PA to IOVA mapping */
	for (i = 0; i < RTE_MAX_MEMSEG; i++) {
		struct vfio_iommu_type1_dma_map dma_map;

		if (ms[i].addr == NULL)
			break;

		memset(&dma_map, 0, sizeof(dma_map));
		dma_map.argsz = sizeof(struct vfio_iommu_type1_dma_map);
		dma_map.vaddr = ms[i].addr_64;
		dma_map.size = ms[i].len;
		dma_map.iova = ms[i].phys_addr;
		dma_map.flags = VFIO_DMA_MAP_FLAG_READ | VFIO_DMA_MAP_FLAG_WRITE;

		ret = ioctl(vfio_container_fd, VFIO_IOMMU_MAP_DMA, &dma_map);

		if (ret) {
			RTE_LOG(ERR, EAL, "  cannot set up DMA remapping, "
					"error %i (%s)\n", errno, strerror(errno));
			return -1;
		}
	}

	return 0;
}

int
vfio_noiommu_dma_map(int __rte_unused vfio_container_fd)
{
	/* No-IOMMU mode does not need DMA mapping */
	return 0;
}

#endif
