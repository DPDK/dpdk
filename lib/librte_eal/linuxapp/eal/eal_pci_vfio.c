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
#include <linux/pci_regs.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <rte_log.h>
#include <rte_pci.h>
#include <rte_eal_memconfig.h>
#include <rte_malloc.h>
#include <eal_private.h>

#include "eal_filesystem.h"
#include "eal_pci_init.h"
#include "eal_vfio.h"

/**
 * @file
 * PCI probing under linux (VFIO version)
 *
 * This code tries to determine if the PCI device is bound to VFIO driver,
 * and initialize it (map BARs, set up interrupts) if that's the case.
 *
 * This file is only compiled if CONFIG_RTE_EAL_VFIO is set to "y".
 */

#ifdef VFIO_PRESENT

#define PAGE_SIZE   (sysconf(_SC_PAGESIZE))
#define PAGE_MASK   (~(PAGE_SIZE - 1))

static struct rte_tailq_elem rte_vfio_tailq = {
	.name = "VFIO_RESOURCE_LIST",
};
EAL_REGISTER_TAILQ(rte_vfio_tailq)

#define VFIO_DIR "/dev/vfio"
#define VFIO_CONTAINER_PATH "/dev/vfio/vfio"
#define VFIO_GROUP_FMT "/dev/vfio/%u"
#define VFIO_GET_REGION_ADDR(x) ((uint64_t) x << 40ULL)

/* per-process VFIO config */
static struct vfio_config vfio_cfg;

int
pci_vfio_read_config(const struct rte_intr_handle *intr_handle,
		    void *buf, size_t len, off_t offs)
{
	return pread64(intr_handle->vfio_dev_fd, buf, len,
	       VFIO_GET_REGION_ADDR(VFIO_PCI_CONFIG_REGION_INDEX) + offs);
}

int
pci_vfio_write_config(const struct rte_intr_handle *intr_handle,
		    const void *buf, size_t len, off_t offs)
{
	return pwrite64(intr_handle->vfio_dev_fd, buf, len,
	       VFIO_GET_REGION_ADDR(VFIO_PCI_CONFIG_REGION_INDEX) + offs);
}

/* get PCI BAR number where MSI-X interrupts are */
static int
pci_vfio_get_msix_bar(int fd, int *msix_bar, uint32_t *msix_table_offset,
		      uint32_t *msix_table_size)
{
	int ret;
	uint32_t reg;
	uint16_t flags;
	uint8_t cap_id, cap_offset;

	/* read PCI capability pointer from config space */
	ret = pread64(fd, &reg, sizeof(reg),
			VFIO_GET_REGION_ADDR(VFIO_PCI_CONFIG_REGION_INDEX) +
			PCI_CAPABILITY_LIST);
	if (ret != sizeof(reg)) {
		RTE_LOG(ERR, EAL, "Cannot read capability pointer from PCI "
				"config space!\n");
		return -1;
	}

	/* we need first byte */
	cap_offset = reg & 0xFF;

	while (cap_offset) {

		/* read PCI capability ID */
		ret = pread64(fd, &reg, sizeof(reg),
				VFIO_GET_REGION_ADDR(VFIO_PCI_CONFIG_REGION_INDEX) +
				cap_offset);
		if (ret != sizeof(reg)) {
			RTE_LOG(ERR, EAL, "Cannot read capability ID from PCI "
					"config space!\n");
			return -1;
		}

		/* we need first byte */
		cap_id = reg & 0xFF;

		/* if we haven't reached MSI-X, check next capability */
		if (cap_id != PCI_CAP_ID_MSIX) {
			ret = pread64(fd, &reg, sizeof(reg),
					VFIO_GET_REGION_ADDR(VFIO_PCI_CONFIG_REGION_INDEX) +
					cap_offset);
			if (ret != sizeof(reg)) {
				RTE_LOG(ERR, EAL, "Cannot read capability pointer from PCI "
						"config space!\n");
				return -1;
			}

			/* we need second byte */
			cap_offset = (reg & 0xFF00) >> 8;

			continue;
		}
		/* else, read table offset */
		else {
			/* table offset resides in the next 4 bytes */
			ret = pread64(fd, &reg, sizeof(reg),
					VFIO_GET_REGION_ADDR(VFIO_PCI_CONFIG_REGION_INDEX) +
					cap_offset + 4);
			if (ret != sizeof(reg)) {
				RTE_LOG(ERR, EAL, "Cannot read table offset from PCI config "
						"space!\n");
				return -1;
			}

			ret = pread64(fd, &flags, sizeof(flags),
					VFIO_GET_REGION_ADDR(VFIO_PCI_CONFIG_REGION_INDEX) +
					cap_offset + 2);
			if (ret != sizeof(flags)) {
				RTE_LOG(ERR, EAL, "Cannot read table flags from PCI config "
						"space!\n");
				return -1;
			}

			*msix_bar = reg & RTE_PCI_MSIX_TABLE_BIR;
			*msix_table_offset = reg & RTE_PCI_MSIX_TABLE_OFFSET;
			*msix_table_size = 16 * (1 + (flags & RTE_PCI_MSIX_FLAGS_QSIZE));

			return 0;
		}
	}
	return 0;
}

/* set PCI bus mastering */
static int
pci_vfio_set_bus_master(int dev_fd)
{
	uint16_t reg;
	int ret;

	ret = pread64(dev_fd, &reg, sizeof(reg),
			VFIO_GET_REGION_ADDR(VFIO_PCI_CONFIG_REGION_INDEX) +
			PCI_COMMAND);
	if (ret != sizeof(reg)) {
		RTE_LOG(ERR, EAL, "Cannot read command from PCI config space!\n");
		return -1;
	}

	/* set the master bit */
	reg |= PCI_COMMAND_MASTER;

	ret = pwrite64(dev_fd, &reg, sizeof(reg),
			VFIO_GET_REGION_ADDR(VFIO_PCI_CONFIG_REGION_INDEX) +
			PCI_COMMAND);

	if (ret != sizeof(reg)) {
		RTE_LOG(ERR, EAL, "Cannot write command to PCI config space!\n");
		return -1;
	}

	return 0;
}

/* set up DMA mappings */
static int
pci_vfio_setup_dma_maps(int vfio_container_fd)
{
	const struct rte_memseg *ms = rte_eal_get_physmem_layout();
	int i, ret;

	ret = ioctl(vfio_container_fd, VFIO_SET_IOMMU,
			VFIO_TYPE1_IOMMU);
	if (ret) {
		RTE_LOG(ERR, EAL, "  cannot set IOMMU type, "
				"error %i (%s)\n", errno, strerror(errno));
		return -1;
	}

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

/* set up interrupt support (but not enable interrupts) */
static int
pci_vfio_setup_interrupts(struct rte_pci_device *dev, int vfio_dev_fd)
{
	int i, ret, intr_idx;

	/* default to invalid index */
	intr_idx = VFIO_PCI_NUM_IRQS;

	/* get interrupt type from internal config (MSI-X by default, can be
	 * overriden from the command line
	 */
	switch (internal_config.vfio_intr_mode) {
	case RTE_INTR_MODE_MSIX:
		intr_idx = VFIO_PCI_MSIX_IRQ_INDEX;
		break;
	case RTE_INTR_MODE_MSI:
		intr_idx = VFIO_PCI_MSI_IRQ_INDEX;
		break;
	case RTE_INTR_MODE_LEGACY:
		intr_idx = VFIO_PCI_INTX_IRQ_INDEX;
		break;
	/* don't do anything if we want to automatically determine interrupt type */
	case RTE_INTR_MODE_NONE:
		break;
	default:
		RTE_LOG(ERR, EAL, "  unknown default interrupt type!\n");
		return -1;
	}

	/* start from MSI-X interrupt type */
	for (i = VFIO_PCI_MSIX_IRQ_INDEX; i >= 0; i--) {
		struct vfio_irq_info irq = { .argsz = sizeof(irq) };
		int fd = -1;

		/* skip interrupt modes we don't want */
		if (internal_config.vfio_intr_mode != RTE_INTR_MODE_NONE &&
				i != intr_idx)
			continue;

		irq.index = i;

		ret = ioctl(vfio_dev_fd, VFIO_DEVICE_GET_IRQ_INFO, &irq);
		if (ret < 0) {
			RTE_LOG(ERR, EAL, "  cannot get IRQ info, "
					"error %i (%s)\n", errno, strerror(errno));
			return -1;
		}

		/* if this vector cannot be used with eventfd, fail if we explicitly
		 * specified interrupt type, otherwise continue */
		if ((irq.flags & VFIO_IRQ_INFO_EVENTFD) == 0) {
			if (internal_config.vfio_intr_mode != RTE_INTR_MODE_NONE) {
				RTE_LOG(ERR, EAL,
						"  interrupt vector does not support eventfd!\n");
				return -1;
			} else
				continue;
		}

		/* set up an eventfd for interrupts */
		fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
		if (fd < 0) {
			RTE_LOG(ERR, EAL, "  cannot set up eventfd, "
					"error %i (%s)\n", errno, strerror(errno));
			return -1;
		}

		dev->intr_handle.fd = fd;
		dev->intr_handle.vfio_dev_fd = vfio_dev_fd;

		switch (i) {
		case VFIO_PCI_MSIX_IRQ_INDEX:
			internal_config.vfio_intr_mode = RTE_INTR_MODE_MSIX;
			dev->intr_handle.type = RTE_INTR_HANDLE_VFIO_MSIX;
			break;
		case VFIO_PCI_MSI_IRQ_INDEX:
			internal_config.vfio_intr_mode = RTE_INTR_MODE_MSI;
			dev->intr_handle.type = RTE_INTR_HANDLE_VFIO_MSI;
			break;
		case VFIO_PCI_INTX_IRQ_INDEX:
			internal_config.vfio_intr_mode = RTE_INTR_MODE_LEGACY;
			dev->intr_handle.type = RTE_INTR_HANDLE_VFIO_LEGACY;
			break;
		default:
			RTE_LOG(ERR, EAL, "  unknown interrupt type!\n");
			return -1;
		}

		return 0;
	}

	/* if we're here, we haven't found a suitable interrupt vector */
	return -1;
}

/* open container fd or get an existing one */
int
pci_vfio_get_container_fd(void)
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

		/* check if we support IOMMU type 1 */
		ret = ioctl(vfio_container_fd, VFIO_CHECK_EXTENSION, VFIO_TYPE1_IOMMU);
		if (ret != 1) {
			if (ret < 0)
				RTE_LOG(ERR, EAL, "  could not get IOMMU type, "
					"error %i (%s)\n", errno,
					strerror(errno));
			else
				RTE_LOG(ERR, EAL, "  unsupported IOMMU type "
					"detected in VFIO\n");
			close(vfio_container_fd);
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

/* open group fd or get an existing one */
int
pci_vfio_get_group_fd(int iommu_group_no)
{
	int i;
	int vfio_group_fd;
	char filename[PATH_MAX];

	/* check if we already have the group descriptor open */
	for (i = 0; i < vfio_cfg.vfio_group_idx; i++)
		if (vfio_cfg.vfio_groups[i].group_no == iommu_group_no)
			return vfio_cfg.vfio_groups[i].fd;

	/* if primary, try to open the group */
	if (internal_config.process_type == RTE_PROC_PRIMARY) {
		snprintf(filename, sizeof(filename),
				 VFIO_GROUP_FMT, iommu_group_no);
		vfio_group_fd = open(filename, O_RDWR);
		if (vfio_group_fd < 0) {
			/* if file not found, it's not an error */
			if (errno != ENOENT) {
				RTE_LOG(ERR, EAL, "Cannot open %s: %s\n", filename,
						strerror(errno));
				return -1;
			}
			return 0;
		}

		/* if the fd is valid, create a new group for it */
		if (vfio_cfg.vfio_group_idx == VFIO_MAX_GROUPS) {
			RTE_LOG(ERR, EAL, "Maximum number of VFIO groups reached!\n");
			return -1;
		}
		vfio_cfg.vfio_groups[vfio_cfg.vfio_group_idx].group_no = iommu_group_no;
		vfio_cfg.vfio_groups[vfio_cfg.vfio_group_idx].fd = vfio_group_fd;
		return vfio_group_fd;
	}
	/* if we're in a secondary process, request group fd from the primary
	 * process via our socket
	 */
	else {
		int socket_fd, ret;

		socket_fd = vfio_mp_sync_connect_to_primary();

		if (socket_fd < 0) {
			RTE_LOG(ERR, EAL, "  cannot connect to primary process!\n");
			return -1;
		}
		if (vfio_mp_sync_send_request(socket_fd, SOCKET_REQ_GROUP) < 0) {
			RTE_LOG(ERR, EAL, "  cannot request container fd!\n");
			close(socket_fd);
			return -1;
		}
		if (vfio_mp_sync_send_request(socket_fd, iommu_group_no) < 0) {
			RTE_LOG(ERR, EAL, "  cannot send group number!\n");
			close(socket_fd);
			return -1;
		}
		ret = vfio_mp_sync_receive_request(socket_fd);
		switch (ret) {
		case SOCKET_NO_FD:
			close(socket_fd);
			return 0;
		case SOCKET_OK:
			vfio_group_fd = vfio_mp_sync_receive_fd(socket_fd);
			/* if we got the fd, return it */
			if (vfio_group_fd > 0) {
				close(socket_fd);
				return vfio_group_fd;
			}
			/* fall-through on error */
		default:
			RTE_LOG(ERR, EAL, "  cannot get container fd!\n");
			close(socket_fd);
			return -1;
		}
	}
	return -1;
}

/* parse IOMMU group number for a PCI device
 * returns -1 for errors, 0 for non-existent group */
static int
pci_vfio_get_group_no(const char *pci_addr)
{
	char linkname[PATH_MAX];
	char filename[PATH_MAX];
	char *tok[16], *group_tok, *end;
	int ret, iommu_group_no;

	memset(linkname, 0, sizeof(linkname));
	memset(filename, 0, sizeof(filename));

	/* try to find out IOMMU group for this device */
	snprintf(linkname, sizeof(linkname),
			 SYSFS_PCI_DEVICES "/%s/iommu_group", pci_addr);

	ret = readlink(linkname, filename, sizeof(filename));

	/* if the link doesn't exist, no VFIO for us */
	if (ret < 0)
		return 0;

	ret = rte_strsplit(filename, sizeof(filename),
			tok, RTE_DIM(tok), '/');

	if (ret <= 0) {
		RTE_LOG(ERR, EAL, "  %s cannot get IOMMU group\n", pci_addr);
		return -1;
	}

	/* IOMMU group is always the last token */
	errno = 0;
	group_tok = tok[ret - 1];
	end = group_tok;
	iommu_group_no = strtol(group_tok, &end, 10);
	if ((end != group_tok && *end != '\0') || errno != 0) {
		RTE_LOG(ERR, EAL, "  %s error parsing IOMMU number!\n", pci_addr);
		return -1;
	}

	return iommu_group_no;
}

static void
clear_current_group(void)
{
	vfio_cfg.vfio_groups[vfio_cfg.vfio_group_idx].group_no = 0;
	vfio_cfg.vfio_groups[vfio_cfg.vfio_group_idx].fd = -1;
}


/*
 * map the PCI resources of a PCI device in virtual memory (VFIO version).
 * primary and secondary processes follow almost exactly the same path
 */
int
pci_vfio_map_resource(struct rte_pci_device *dev)
{
	struct vfio_group_status group_status = {
			.argsz = sizeof(group_status)
	};
	struct vfio_device_info device_info = { .argsz = sizeof(device_info) };
	int vfio_group_fd, vfio_dev_fd;
	int iommu_group_no;
	char pci_addr[PATH_MAX] = {0};
	struct rte_pci_addr *loc = &dev->addr;
	int i, ret, msix_bar;
	struct mapped_pci_resource *vfio_res = NULL;
	struct mapped_pci_res_list *vfio_res_list = RTE_TAILQ_CAST(rte_vfio_tailq.head, mapped_pci_res_list);

	struct pci_map *maps;
	uint32_t msix_table_offset = 0;
	uint32_t msix_table_size = 0;

	dev->intr_handle.fd = -1;
	dev->intr_handle.type = RTE_INTR_HANDLE_UNKNOWN;

	/* store PCI address string */
	snprintf(pci_addr, sizeof(pci_addr), PCI_PRI_FMT,
			loc->domain, loc->bus, loc->devid, loc->function);

	/* get group number */
	iommu_group_no = pci_vfio_get_group_no(pci_addr);

	/* if 0, group doesn't exist */
	if (iommu_group_no == 0) {
		RTE_LOG(WARNING, EAL, "  %s not managed by VFIO driver, skipping\n",
				pci_addr);
		return 1;
	}
	/* if negative, something failed */
	else if (iommu_group_no < 0)
		return -1;

	/* get the actual group fd */
	vfio_group_fd = pci_vfio_get_group_fd(iommu_group_no);
	if (vfio_group_fd < 0)
		return -1;

	/* store group fd */
	vfio_cfg.vfio_groups[vfio_cfg.vfio_group_idx].group_no = iommu_group_no;
	vfio_cfg.vfio_groups[vfio_cfg.vfio_group_idx].fd = vfio_group_fd;

	/* if group_fd == 0, that means the device isn't managed by VFIO */
	if (vfio_group_fd == 0) {
		RTE_LOG(WARNING, EAL, "  %s not managed by VFIO driver, skipping\n",
				pci_addr);
		/* we store 0 as group fd to distinguish between existing but
		 * unbound VFIO groups, and groups that don't exist at all.
		 */
		vfio_cfg.vfio_group_idx++;
		return 1;
	}

	/*
	 * at this point, we know at least one port on this device is bound to VFIO,
	 * so we can proceed to try and set this particular port up
	 */

	/* check if the group is viable */
	ret = ioctl(vfio_group_fd, VFIO_GROUP_GET_STATUS, &group_status);
	if (ret) {
		RTE_LOG(ERR, EAL, "  %s cannot get group status, "
				"error %i (%s)\n", pci_addr, errno, strerror(errno));
		close(vfio_group_fd);
		clear_current_group();
		return -1;
	} else if (!(group_status.flags & VFIO_GROUP_FLAGS_VIABLE)) {
		RTE_LOG(ERR, EAL, "  %s VFIO group is not viable!\n", pci_addr);
		close(vfio_group_fd);
		clear_current_group();
		return -1;
	}

	/*
	 * at this point, we know that this group is viable (meaning, all devices
	 * are either bound to VFIO or not bound to anything)
	 */

	/* check if group does not have a container yet */
	if (!(group_status.flags & VFIO_GROUP_FLAGS_CONTAINER_SET)) {

		/* add group to a container */
		ret = ioctl(vfio_group_fd, VFIO_GROUP_SET_CONTAINER,
				&vfio_cfg.vfio_container_fd);
		if (ret) {
			RTE_LOG(ERR, EAL, "  %s cannot add VFIO group to container, "
					"error %i (%s)\n", pci_addr, errno, strerror(errno));
			close(vfio_group_fd);
			clear_current_group();
			return -1;
		}
		/*
		 * at this point we know that this group has been successfully
		 * initialized, so we increment vfio_group_idx to indicate that we can
		 * add new groups.
		 */
		vfio_cfg.vfio_group_idx++;
	}

	/*
	 * set up DMA mappings for container
	 *
	 * needs to be done only once, only when at least one group is assigned to
	 * a container and only in primary process
	 */
	if (internal_config.process_type == RTE_PROC_PRIMARY &&
			vfio_cfg.vfio_container_has_dma == 0) {
		ret = pci_vfio_setup_dma_maps(vfio_cfg.vfio_container_fd);
		if (ret) {
			RTE_LOG(ERR, EAL, "  %s DMA remapping failed, "
					"error %i (%s)\n", pci_addr, errno, strerror(errno));
			return -1;
		}
		vfio_cfg.vfio_container_has_dma = 1;
	}

	/* get a file descriptor for the device */
	vfio_dev_fd = ioctl(vfio_group_fd, VFIO_GROUP_GET_DEVICE_FD, pci_addr);
	if (vfio_dev_fd < 0) {
		/* if we cannot get a device fd, this simply means that this
		 * particular port is not bound to VFIO
		 */
		RTE_LOG(WARNING, EAL, "  %s not managed by VFIO driver, skipping\n",
				pci_addr);
		return 1;
	}

	/* test and setup the device */
	ret = ioctl(vfio_dev_fd, VFIO_DEVICE_GET_INFO, &device_info);
	if (ret) {
		RTE_LOG(ERR, EAL, "  %s cannot get device info, "
				"error %i (%s)\n", pci_addr, errno, strerror(errno));
		close(vfio_dev_fd);
		return -1;
	}

	/* get MSI-X BAR, if any (we have to know where it is because we can't
	 * easily mmap it when using VFIO) */
	msix_bar = -1;
	ret = pci_vfio_get_msix_bar(vfio_dev_fd, &msix_bar,
				    &msix_table_offset, &msix_table_size);
	if (ret < 0) {
		RTE_LOG(ERR, EAL, "  %s cannot get MSI-X BAR number!\n", pci_addr);
		close(vfio_dev_fd);
		return -1;
	}

	/* if we're in a primary process, allocate vfio_res and get region info */
	if (internal_config.process_type == RTE_PROC_PRIMARY) {
		vfio_res = rte_zmalloc("VFIO_RES", sizeof(*vfio_res), 0);
		if (vfio_res == NULL) {
			RTE_LOG(ERR, EAL,
				"%s(): cannot store uio mmap details\n", __func__);
			close(vfio_dev_fd);
			return -1;
		}
		memcpy(&vfio_res->pci_addr, &dev->addr, sizeof(vfio_res->pci_addr));

		/* get number of registers (up to BAR5) */
		vfio_res->nb_maps = RTE_MIN((int) device_info.num_regions,
				VFIO_PCI_BAR5_REGION_INDEX + 1);
	} else {
		/* if we're in a secondary process, just find our tailq entry */
		TAILQ_FOREACH(vfio_res, vfio_res_list, next) {
			if (memcmp(&vfio_res->pci_addr, &dev->addr, sizeof(dev->addr)))
				continue;
			break;
		}
		/* if we haven't found our tailq entry, something's wrong */
		if (vfio_res == NULL) {
			RTE_LOG(ERR, EAL, "  %s cannot find TAILQ entry for PCI device!\n",
					pci_addr);
			close(vfio_dev_fd);
			return -1;
		}
	}

	/* map BARs */
	maps = vfio_res->maps;

	for (i = 0; i < (int) vfio_res->nb_maps; i++) {
		struct vfio_region_info reg = { .argsz = sizeof(reg) };
		void *bar_addr;
		struct memreg {
			unsigned long offset, size;
		} memreg[2] = {};

		reg.index = i;

		ret = ioctl(vfio_dev_fd, VFIO_DEVICE_GET_REGION_INFO, &reg);

		if (ret) {
			RTE_LOG(ERR, EAL, "  %s cannot get device region info "
					"error %i (%s)\n", pci_addr, errno, strerror(errno));
			close(vfio_dev_fd);
			if (internal_config.process_type == RTE_PROC_PRIMARY)
				rte_free(vfio_res);
			return -1;
		}

		/* skip non-mmapable BARs */
		if ((reg.flags & VFIO_REGION_INFO_FLAG_MMAP) == 0)
			continue;

		if (i == msix_bar) {
			/*
			 * VFIO will not let us map the MSI-X table,
			 * but we can map around it.
			 */
			uint32_t table_start = msix_table_offset;
			uint32_t table_end = table_start + msix_table_size;
			table_end = (table_end + ~PAGE_MASK) & PAGE_MASK;
			table_start &= PAGE_MASK;

			if (table_start == 0 && table_end >= reg.size) {
				/* Cannot map this BAR */
				RTE_LOG(DEBUG, EAL, "Skipping BAR %d\n", i);
				continue;
			} else {
				memreg[0].offset = reg.offset;
				memreg[0].size = table_start;
				memreg[1].offset = table_end;
				memreg[1].size = reg.size - table_end;

				RTE_LOG(DEBUG, EAL,
					"Trying to map BAR %d that contains the MSI-X "
					"table. Trying offsets: "
					"0x%04lx:0x%04lx, 0x%04lx:0x%04lx\n", i,
					memreg[0].offset, memreg[0].size,
					memreg[1].offset, memreg[1].size);
			}
		} else {
			memreg[0].offset = reg.offset;
			memreg[0].size = reg.size;
		}

		/* try to figure out an address */
		if (internal_config.process_type == RTE_PROC_PRIMARY) {
			/* try mapping somewhere close to the end of hugepages */
			if (pci_map_addr == NULL)
				pci_map_addr = pci_find_max_end_va();

			bar_addr = pci_map_addr;
			pci_map_addr = RTE_PTR_ADD(bar_addr, (size_t) reg.size);
		} else {
			bar_addr = maps[i].addr;
		}

		/* reserve the address using an inaccessible mapping */
		bar_addr = mmap(bar_addr, reg.size, 0, MAP_PRIVATE |
				MAP_ANONYMOUS, -1, 0);
		if (bar_addr != MAP_FAILED) {
			void *map_addr = NULL;
			if (memreg[0].size) {
				/* actual map of first part */
				map_addr = pci_map_resource(bar_addr, vfio_dev_fd,
							    memreg[0].offset,
							    memreg[0].size,
							    MAP_FIXED);
			}

			/* if there's a second part, try to map it */
			if (map_addr != MAP_FAILED
			    && memreg[1].offset && memreg[1].size) {
				void *second_addr = RTE_PTR_ADD(bar_addr, memreg[1].offset);
				map_addr = pci_map_resource(second_addr,
							    vfio_dev_fd, memreg[1].offset,
							    memreg[1].size,
							    MAP_FIXED);
			}

			if (map_addr == MAP_FAILED || !map_addr) {
				munmap(bar_addr, reg.size);
				bar_addr = MAP_FAILED;
			}
		}

		if (bar_addr == MAP_FAILED ||
				(internal_config.process_type == RTE_PROC_SECONDARY &&
						bar_addr != maps[i].addr)) {
			RTE_LOG(ERR, EAL, "  %s mapping BAR%i failed: %s\n", pci_addr, i,
					strerror(errno));
			close(vfio_dev_fd);
			if (internal_config.process_type == RTE_PROC_PRIMARY)
				rte_free(vfio_res);
			return -1;
		}

		maps[i].addr = bar_addr;
		maps[i].offset = reg.offset;
		maps[i].size = reg.size;
		maps[i].path = NULL; /* vfio doesn't have per-resource paths */
		dev->mem_resource[i].addr = bar_addr;
	}

	/* if secondary process, do not set up interrupts */
	if (internal_config.process_type == RTE_PROC_PRIMARY) {
		if (pci_vfio_setup_interrupts(dev, vfio_dev_fd) != 0) {
			RTE_LOG(ERR, EAL, "  %s error setting up interrupts!\n", pci_addr);
			close(vfio_dev_fd);
			rte_free(vfio_res);
			return -1;
		}

		/* set bus mastering for the device */
		if (pci_vfio_set_bus_master(vfio_dev_fd)) {
			RTE_LOG(ERR, EAL, "  %s cannot set up bus mastering!\n", pci_addr);
			close(vfio_dev_fd);
			rte_free(vfio_res);
			return -1;
		}

		/* Reset the device */
		ioctl(vfio_dev_fd, VFIO_DEVICE_RESET);
	}

	if (internal_config.process_type == RTE_PROC_PRIMARY)
		TAILQ_INSERT_TAIL(vfio_res_list, vfio_res, next);

	return 0;
}

int
pci_vfio_enable(void)
{
	/* initialize group list */
	int i;
	int module_vfio_type1;

	for (i = 0; i < VFIO_MAX_GROUPS; i++) {
		vfio_cfg.vfio_groups[i].fd = -1;
		vfio_cfg.vfio_groups[i].group_no = -1;
	}

	module_vfio_type1 = rte_eal_check_module("vfio_iommu_type1");

	/* return error directly */
	if (module_vfio_type1 == -1) {
		RTE_LOG(INFO, EAL, "Could not get loaded module details!\n");
		return -1;
	}

	/* return 0 if VFIO modules not loaded */
	if (module_vfio_type1 == 0) {
		RTE_LOG(INFO, EAL, "VFIO modules not all loaded, "
			"skip VFIO support...\n");
		return 0;
	}

	vfio_cfg.vfio_container_fd = pci_vfio_get_container_fd();

	/* check if we have VFIO driver enabled */
	if (vfio_cfg.vfio_container_fd != -1)
		vfio_cfg.vfio_enabled = 1;
	else
		RTE_LOG(NOTICE, EAL, "VFIO support could not be initialized\n");

	return 0;
}

int
pci_vfio_is_enabled(void)
{
	return vfio_cfg.vfio_enabled;
}
#endif
