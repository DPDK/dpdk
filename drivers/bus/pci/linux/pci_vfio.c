/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdbool.h>

#include <rte_log.h>
#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <rte_eal_paging.h>
#include <rte_malloc.h>
#include <rte_vfio.h>
#include <rte_eal.h>
#include <bus_driver.h>
#include <rte_spinlock.h>
#include <rte_tailq.h>

#include "eal_filesystem.h"

#include "pci_init.h"
#include "private.h"

/**
 * @file
 * PCI probing using Linux VFIO.
 *
 * This code tries to determine if the PCI device is bound to VFIO driver,
 * and initialize it (map BARs, set up interrupts) if that's the case.
 *
 */

#ifdef VFIO_PRESENT

static struct rte_tailq_elem rte_vfio_tailq = {
	.name = "VFIO_RESOURCE_LIST",
};
EAL_REGISTER_TAILQ(rte_vfio_tailq)

static int
pci_vfio_get_region(const struct rte_pci_device *dev, int index,
		    uint64_t *size, uint64_t *offset)
{
	const struct rte_pci_device_internal *pdev =
		RTE_PCI_DEVICE_INTERNAL_CONST(dev);

	if (index >= VFIO_PCI_NUM_REGIONS || index >= RTE_MAX_PCI_REGIONS)
		return -1;

	if (pdev->region[index].size == 0 && pdev->region[index].offset == 0)
		return -1;

	*size   = pdev->region[index].size;
	*offset = pdev->region[index].offset;

	return 0;
}

int
pci_vfio_read_config(const struct rte_pci_device *dev,
		    void *buf, size_t len, off_t offs)
{
	uint64_t size, offset;
	int fd;

	fd = rte_intr_dev_fd_get(dev->intr_handle);
	if (fd < 0)
		return -1;

	if (pci_vfio_get_region(dev, VFIO_PCI_CONFIG_REGION_INDEX,
				&size, &offset) != 0)
		return -1;

	if ((uint64_t)len + offs > size)
		return -1;

	return pread64(fd, buf, len, offset + offs);
}

int
pci_vfio_write_config(const struct rte_pci_device *dev,
		    const void *buf, size_t len, off_t offs)
{
	uint64_t size, offset;
	int fd;

	fd = rte_intr_dev_fd_get(dev->intr_handle);
	if (fd < 0)
		return -1;

	if (pci_vfio_get_region(dev, VFIO_PCI_CONFIG_REGION_INDEX,
				&size, &offset) != 0)
		return -1;

	if ((uint64_t)len + offs > size)
		return -1;

	return pwrite64(fd, buf, len, offset + offs);
}

/* get PCI BAR number where MSI-X interrupts are */
static int
pci_vfio_get_msix_bar(const struct rte_pci_device *dev,
	struct pci_msix_table *msix_table)
{
	off_t cap_offset;

	cap_offset = rte_pci_find_capability(dev, RTE_PCI_CAP_ID_MSIX);
	if (cap_offset < 0)
		return -1;

	if (cap_offset != 0) {
		uint16_t flags;
		uint32_t reg;

		if (rte_pci_read_config(dev, &reg, sizeof(reg), cap_offset +
				RTE_PCI_MSIX_TABLE) < 0) {
			RTE_LOG(ERR, EAL,
				"Cannot read MSIX table from PCI config space!\n");
			return -1;
		}

		if (rte_pci_read_config(dev, &flags, sizeof(flags), cap_offset +
				RTE_PCI_MSIX_FLAGS) < 0) {
			RTE_LOG(ERR, EAL,
				"Cannot read MSIX flags from PCI config space!\n");
			return -1;
		}

		msix_table->bar_index = reg & RTE_PCI_MSIX_TABLE_BIR;
		msix_table->offset = reg & RTE_PCI_MSIX_TABLE_OFFSET;
		msix_table->size = 16 * (1 + (flags & RTE_PCI_MSIX_FLAGS_QSIZE));
	}

	return 0;
}

/* enable PCI bus memory space */
static int
pci_vfio_enable_bus_memory(struct rte_pci_device *dev, int dev_fd)
{
	uint64_t size, offset;
	uint16_t cmd;
	int ret;

	if (pci_vfio_get_region(dev, VFIO_PCI_CONFIG_REGION_INDEX,
		&size, &offset) != 0) {
		RTE_LOG(ERR, EAL, "Cannot get offset of CONFIG region.\n");
		return -1;
	}

	ret = pread64(dev_fd, &cmd, sizeof(cmd), offset + RTE_PCI_COMMAND);

	if (ret != sizeof(cmd)) {
		RTE_LOG(ERR, EAL, "Cannot read command from PCI config space!\n");
		return -1;
	}

	if (cmd & RTE_PCI_COMMAND_MEMORY)
		return 0;

	cmd |= RTE_PCI_COMMAND_MEMORY;
	ret = pwrite64(dev_fd, &cmd, sizeof(cmd), offset + RTE_PCI_COMMAND);

	if (ret != sizeof(cmd)) {
		RTE_LOG(ERR, EAL, "Cannot write command to PCI config space!\n");
		return -1;
	}

	return 0;
}

/* set up interrupt support (but not enable interrupts) */
static int
pci_vfio_setup_interrupts(struct rte_pci_device *dev, int vfio_dev_fd)
{
	int i, ret, intr_idx;
	enum rte_intr_mode intr_mode;

	/* default to invalid index */
	intr_idx = VFIO_PCI_NUM_IRQS;

	/* Get default / configured intr_mode */
	intr_mode = rte_eal_vfio_intr_mode();

	/* get interrupt type from internal config (MSI-X by default, can be
	 * overridden from the command line
	 */
	switch (intr_mode) {
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
		RTE_LOG(ERR, EAL, "Unknown default interrupt type!\n");
		return -1;
	}

	/* start from MSI-X interrupt type */
	for (i = VFIO_PCI_MSIX_IRQ_INDEX; i >= 0; i--) {
		struct vfio_irq_info irq = { .argsz = sizeof(irq) };
		int fd = -1;

		/* skip interrupt modes we don't want */
		if (intr_mode != RTE_INTR_MODE_NONE &&
				i != intr_idx)
			continue;

		irq.index = i;

		ret = ioctl(vfio_dev_fd, VFIO_DEVICE_GET_IRQ_INFO, &irq);
		if (ret < 0) {
			RTE_LOG(ERR, EAL, "Cannot get VFIO IRQ info, error "
					"%i (%s)\n", errno, strerror(errno));
			return -1;
		}

		/* if this vector cannot be used with eventfd, fail if we explicitly
		 * specified interrupt type, otherwise continue */
		if ((irq.flags & VFIO_IRQ_INFO_EVENTFD) == 0) {
			if (intr_mode != RTE_INTR_MODE_NONE) {
				RTE_LOG(ERR, EAL,
					"Interrupt vector does not support eventfd!\n");
				return -1;
			} else
				continue;
		}

		/* Reallocate the efds and elist fields of intr_handle based
		 * on PCI device MSIX size.
		 */
		if (i == VFIO_PCI_MSIX_IRQ_INDEX &&
				(uint32_t)rte_intr_nb_intr_get(dev->intr_handle) < irq.count &&
				rte_intr_event_list_update(dev->intr_handle, irq.count))
			return -1;

		/* set up an eventfd for interrupts */
		fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
		if (fd < 0) {
			RTE_LOG(ERR, EAL, "Cannot set up eventfd, error "
					"%i (%s)\n", errno, strerror(errno));
			return -1;
		}

		if (rte_intr_fd_set(dev->intr_handle, fd))
			return -1;

		switch (i) {
		case VFIO_PCI_MSIX_IRQ_INDEX:
			intr_mode = RTE_INTR_MODE_MSIX;
			rte_intr_type_set(dev->intr_handle,
						 RTE_INTR_HANDLE_VFIO_MSIX);
			break;
		case VFIO_PCI_MSI_IRQ_INDEX:
			intr_mode = RTE_INTR_MODE_MSI;
			rte_intr_type_set(dev->intr_handle,
						 RTE_INTR_HANDLE_VFIO_MSI);
			break;
		case VFIO_PCI_INTX_IRQ_INDEX:
			intr_mode = RTE_INTR_MODE_LEGACY;
			rte_intr_type_set(dev->intr_handle,
						 RTE_INTR_HANDLE_VFIO_LEGACY);
			break;
		default:
			RTE_LOG(ERR, EAL, "Unknown interrupt type!\n");
			return -1;
		}

		return 0;
	}

	/* if we're here, we haven't found a suitable interrupt vector */
	return -1;
}

#ifdef HAVE_VFIO_DEV_REQ_INTERFACE
/*
 * Spinlock for device hot-unplug failure handling.
 * If it tries to access bus or device, such as handle sigbus on bus
 * or handle memory failure for device, just need to use this lock.
 * It could protect the bus and the device to avoid race condition.
 */
static rte_spinlock_t failure_handle_lock = RTE_SPINLOCK_INITIALIZER;

static void
pci_vfio_req_handler(void *param)
{
	struct rte_bus *bus;
	int ret;
	struct rte_device *device = (struct rte_device *)param;

	rte_spinlock_lock(&failure_handle_lock);
	bus = rte_bus_find_by_device(device);
	if (bus == NULL) {
		RTE_LOG(ERR, EAL, "Cannot find bus for device (%s)\n",
			device->name);
		goto handle_end;
	}

	/*
	 * vfio kernel module request user space to release allocated
	 * resources before device be deleted in kernel, so it can directly
	 * call the vfio bus hot-unplug handler to process it.
	 */
	ret = bus->hot_unplug_handler(device);
	if (ret)
		RTE_LOG(ERR, EAL,
			"Can not handle hot-unplug for device (%s)\n",
			device->name);
handle_end:
	rte_spinlock_unlock(&failure_handle_lock);
}

/* enable notifier (only enable req now) */
static int
pci_vfio_enable_notifier(struct rte_pci_device *dev, int vfio_dev_fd)
{
	int ret;
	int fd = -1;

	/* set up an eventfd for req notifier */
	fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	if (fd < 0) {
		RTE_LOG(ERR, EAL, "Cannot set up eventfd, error %i (%s)\n",
			errno, strerror(errno));
		return -1;
	}

	if (rte_intr_fd_set(dev->vfio_req_intr_handle, fd))
		return -1;

	if (rte_intr_type_set(dev->vfio_req_intr_handle, RTE_INTR_HANDLE_VFIO_REQ))
		return -1;

	if (rte_intr_dev_fd_set(dev->vfio_req_intr_handle, vfio_dev_fd))
		return -1;

	ret = rte_intr_callback_register(dev->vfio_req_intr_handle,
					 pci_vfio_req_handler,
					 (void *)&dev->device);
	if (ret) {
		RTE_LOG(ERR, EAL, "Fail to register req notifier handler.\n");
		goto error;
	}

	ret = rte_intr_enable(dev->vfio_req_intr_handle);
	if (ret) {
		RTE_LOG(ERR, EAL, "Fail to enable req notifier.\n");
		ret = rte_intr_callback_unregister(dev->vfio_req_intr_handle,
						 pci_vfio_req_handler,
						 (void *)&dev->device);
		if (ret < 0)
			RTE_LOG(ERR, EAL,
				"Fail to unregister req notifier handler.\n");
		goto error;
	}

	return 0;
error:
	close(fd);

	rte_intr_fd_set(dev->vfio_req_intr_handle, -1);
	rte_intr_type_set(dev->vfio_req_intr_handle, RTE_INTR_HANDLE_UNKNOWN);
	rte_intr_dev_fd_set(dev->vfio_req_intr_handle, -1);

	return -1;
}

/* disable notifier (only disable req now) */
static int
pci_vfio_disable_notifier(struct rte_pci_device *dev)
{
	int ret;

	ret = rte_intr_disable(dev->vfio_req_intr_handle);
	if (ret) {
		RTE_LOG(ERR, EAL, "fail to disable req notifier.\n");
		return -1;
	}

	ret = rte_intr_callback_unregister_sync(dev->vfio_req_intr_handle,
					   pci_vfio_req_handler,
					   (void *)&dev->device);
	if (ret < 0) {
		RTE_LOG(ERR, EAL,
			 "fail to unregister req notifier handler.\n");
		return -1;
	}

	close(rte_intr_fd_get(dev->vfio_req_intr_handle));

	rte_intr_fd_set(dev->vfio_req_intr_handle, -1);
	rte_intr_type_set(dev->vfio_req_intr_handle, RTE_INTR_HANDLE_UNKNOWN);
	rte_intr_dev_fd_set(dev->vfio_req_intr_handle, -1);

	return 0;
}
#endif

static int
pci_vfio_is_ioport_bar(const struct rte_pci_device *dev, int vfio_dev_fd,
	int bar_index)
{
	uint64_t size, offset;
	uint32_t ioport_bar;
	int ret;

	if (pci_vfio_get_region(dev, VFIO_PCI_CONFIG_REGION_INDEX,
		&size, &offset) != 0) {
		RTE_LOG(ERR, EAL, "Cannot get offset of CONFIG region.\n");
		return -1;
	}

	ret = pread64(vfio_dev_fd, &ioport_bar, sizeof(ioport_bar),
			  offset + RTE_PCI_BASE_ADDRESS_0 + bar_index * 4);
	if (ret != sizeof(ioport_bar)) {
		RTE_LOG(ERR, EAL, "Cannot read command (%x) from config space!\n",
			RTE_PCI_BASE_ADDRESS_0 + bar_index*4);
		return -1;
	}

	return (ioport_bar & RTE_PCI_BASE_ADDRESS_SPACE_IO) != 0;
}

static int
pci_rte_vfio_setup_device(struct rte_pci_device *dev, int vfio_dev_fd)
{
	if (pci_vfio_setup_interrupts(dev, vfio_dev_fd) != 0) {
		RTE_LOG(ERR, EAL, "Error setting up interrupts!\n");
		return -1;
	}

	if (pci_vfio_enable_bus_memory(dev, vfio_dev_fd)) {
		RTE_LOG(ERR, EAL, "Cannot enable bus memory!\n");
		return -1;
	}

	if (rte_pci_set_bus_master(dev, true)) {
		RTE_LOG(ERR, EAL, "Cannot set up bus mastering!\n");
		return -1;
	}

	/*
	 * Reset the device. If the device is not capable of resetting,
	 * then it updates errno as EINVAL.
	 */
	if (ioctl(vfio_dev_fd, VFIO_DEVICE_RESET) && errno != EINVAL) {
		RTE_LOG(ERR, EAL, "Unable to reset device! Error: %d (%s)\n",
				errno, strerror(errno));
		return -1;
	}

	return 0;
}

static int
pci_vfio_mmap_bar(int vfio_dev_fd, struct mapped_pci_resource *vfio_res,
		int bar_index, int additional_flags)
{
	struct memreg {
		uint64_t offset;
		size_t   size;
	} memreg[2] = {};
	void *bar_addr;
	struct pci_msix_table *msix_table = &vfio_res->msix_table;
	struct pci_map *bar = &vfio_res->maps[bar_index];

	if (bar->size == 0) {
		RTE_LOG(DEBUG, EAL, "Bar size is 0, skip BAR%d\n", bar_index);
		return 0;
	}

	if (msix_table->bar_index == bar_index) {
		/*
		 * VFIO will not let us map the MSI-X table,
		 * but we can map around it.
		 */
		uint32_t table_start = msix_table->offset;
		uint32_t table_end = table_start + msix_table->size;
		table_end = RTE_ALIGN(table_end, rte_mem_page_size());
		table_start = RTE_ALIGN_FLOOR(table_start, rte_mem_page_size());

		/* If page-aligned start of MSI-X table is less than the
		 * actual MSI-X table start address, reassign to the actual
		 * start address.
		 */
		if (table_start < msix_table->offset)
			table_start = msix_table->offset;

		if (table_start == 0 && table_end >= bar->size) {
			/* Cannot map this BAR */
			RTE_LOG(DEBUG, EAL, "Skipping BAR%d\n", bar_index);
			bar->size = 0;
			bar->addr = 0;
			return 0;
		}

		memreg[0].offset = bar->offset;
		memreg[0].size = table_start;
		if (bar->size < table_end) {
			/*
			 * If MSI-X table end is beyond BAR end, don't attempt
			 * to perform second mapping.
			 */
			memreg[1].offset = 0;
			memreg[1].size = 0;
		} else {
			memreg[1].offset = bar->offset + table_end;
			memreg[1].size = bar->size - table_end;
		}

		RTE_LOG(DEBUG, EAL,
			"Trying to map BAR%d that contains the MSI-X "
			"table. Trying offsets: "
			"0x%04" PRIx64 ":0x%04zx, 0x%04" PRIx64 ":0x%04zx\n",
			bar_index,
			memreg[0].offset, memreg[0].size,
			memreg[1].offset, memreg[1].size);
	} else {
		memreg[0].offset = bar->offset;
		memreg[0].size = bar->size;
	}

	/* reserve the address using an inaccessible mapping */
	bar_addr = mmap(bar->addr, bar->size, 0, MAP_PRIVATE |
			MAP_ANONYMOUS | additional_flags, -1, 0);
	if (bar_addr != MAP_FAILED) {
		void *map_addr = NULL;
		if (memreg[0].size) {
			/* actual map of first part */
			map_addr = pci_map_resource(bar_addr, vfio_dev_fd,
							memreg[0].offset,
							memreg[0].size,
							RTE_MAP_FORCE_ADDRESS);
		}

		/*
		 * Regarding "memreg[0].size == 0":
		 * If this BAR has MSI-X table, memreg[0].size (the
		 * first part or the part before the table) can
		 * legitimately be 0 for hardware using vector table
		 * offset 0 (i.e. first part does not exist).
		 *
		 * When memreg[0].size is 0, "mapping the first part"
		 * never happens, and map_addr is NULL at this
		 * point. So check that mapping has been actually
		 * attempted.
		 */
		/* if there's a second part, try to map it */
		if ((map_addr != NULL || memreg[0].size == 0)
			&& memreg[1].offset && memreg[1].size) {
			void *second_addr = RTE_PTR_ADD(bar_addr,
						(uintptr_t)(memreg[1].offset -
						bar->offset));
			map_addr = pci_map_resource(second_addr,
							vfio_dev_fd,
							memreg[1].offset,
							memreg[1].size,
							RTE_MAP_FORCE_ADDRESS);
		}

		if (map_addr == NULL) {
			munmap(bar_addr, bar->size);
			bar_addr = MAP_FAILED;
			RTE_LOG(ERR, EAL, "Failed to map pci BAR%d\n",
					bar_index);
			return -1;
		}
	} else {
		RTE_LOG(ERR, EAL,
				"Failed to create inaccessible mapping for BAR%d\n",
				bar_index);
		return -1;
	}

	bar->addr = bar_addr;
	return 0;
}

static int
pci_vfio_sparse_mmap_bar(int vfio_dev_fd, struct mapped_pci_resource *vfio_res,
		int bar_index, int additional_flags)
{
	struct pci_map *bar = &vfio_res->maps[bar_index];
	struct vfio_region_sparse_mmap_area *sparse;
	void *bar_addr;
	uint32_t i;

	if (bar->size == 0) {
		RTE_LOG(DEBUG, EAL, "Bar size is 0, skip BAR%d\n", bar_index);
		return 0;
	}

	/* reserve the address using an inaccessible mapping */
	bar_addr = mmap(bar->addr, bar->size, 0, MAP_PRIVATE |
			MAP_ANONYMOUS | additional_flags, -1, 0);
	if (bar_addr != MAP_FAILED) {
		void *map_addr = NULL;
		for (i = 0; i < bar->nr_areas; i++) {
			sparse = &bar->areas[i];
			if (sparse->size) {
				void *addr = RTE_PTR_ADD(bar_addr, (uintptr_t)sparse->offset);
				map_addr = pci_map_resource(addr, vfio_dev_fd,
					bar->offset + sparse->offset, sparse->size,
					RTE_MAP_FORCE_ADDRESS);
				if (map_addr == NULL) {
					munmap(bar_addr, bar->size);
					RTE_LOG(ERR, EAL, "Failed to map pci BAR%d\n",
						bar_index);
					goto err_map;
				}
			}
		}
	} else {
		RTE_LOG(ERR, EAL, "Failed to create inaccessible mapping for BAR%d\n",
			bar_index);
		goto err_map;
	}

	bar->addr = bar_addr;
	return 0;

err_map:
	bar->nr_areas = 0;
	return -1;
}

/*
 * region info may contain capability headers, so we need to keep reallocating
 * the memory until we match allocated memory size with argsz.
 */
static int
pci_vfio_get_region_info(int vfio_dev_fd, struct vfio_region_info **info,
		int region)
{
	struct vfio_region_info *ri;
	size_t argsz = sizeof(*ri);
	int ret;

	ri = malloc(sizeof(*ri));
	if (ri == NULL) {
		RTE_LOG(ERR, EAL,
			"Cannot allocate memory for VFIO region info\n");
		return -1;
	}
again:
	memset(ri, 0, argsz);
	ri->argsz = argsz;
	ri->index = region;

	ret = ioctl(vfio_dev_fd, VFIO_DEVICE_GET_REGION_INFO, ri);
	if (ret < 0) {
		free(ri);
		return ret;
	}
	if (ri->argsz != argsz) {
		struct vfio_region_info *tmp;

		argsz = ri->argsz;
		tmp = realloc(ri, argsz);

		if (tmp == NULL) {
			/* realloc failed but the ri is still there */
			free(ri);
			RTE_LOG(ERR, EAL,
				"Cannot reallocate memory for VFIO region info\n");
			return -1;
		}
		ri = tmp;
		goto again;
	}
	*info = ri;

	return 0;
}

static struct vfio_info_cap_header *
pci_vfio_info_cap(struct vfio_region_info *info, int cap)
{
	struct vfio_info_cap_header *h;
	size_t offset;

	if ((info->flags & RTE_VFIO_INFO_FLAG_CAPS) == 0) {
		/* VFIO info does not advertise capabilities */
		return NULL;
	}

	offset = VFIO_CAP_OFFSET(info);
	while (offset != 0) {
		h = RTE_PTR_ADD(info, offset);
		if (h->id == cap)
			return h;
		offset = h->next;
	}
	return NULL;
}

static int
pci_vfio_msix_is_mappable(int vfio_dev_fd, int msix_region)
{
	struct vfio_region_info *info = NULL;
	int ret;

	ret = pci_vfio_get_region_info(vfio_dev_fd, &info, msix_region);
	if (ret < 0)
		return -1;

	ret = pci_vfio_info_cap(info, RTE_VFIO_CAP_MSIX_MAPPABLE) != NULL;

	/* cleanup */
	free(info);

	return ret;
}

static int
pci_vfio_fill_regions(struct rte_pci_device *dev, int vfio_dev_fd,
		      struct vfio_device_info *device_info)
{
	struct rte_pci_device_internal *pdev = RTE_PCI_DEVICE_INTERNAL(dev);
	struct vfio_region_info *reg = NULL;
	int nb_maps, i, ret;

	nb_maps = RTE_MIN((int)device_info->num_regions,
			VFIO_PCI_CONFIG_REGION_INDEX + 1);

	for (i = 0; i < nb_maps; i++) {
		ret = pci_vfio_get_region_info(vfio_dev_fd, &reg, i);
		if (ret < 0) {
			RTE_LOG(DEBUG, EAL, "%s cannot get device region info error %i (%s)\n",
				dev->name, errno, strerror(errno));
			return -1;
		}

		pdev->region[i].size = reg->size;
		pdev->region[i].offset = reg->offset;

		free(reg);
	}

	return 0;
}

static int
pci_vfio_map_resource_primary(struct rte_pci_device *dev)
{
	struct rte_pci_device_internal *pdev = RTE_PCI_DEVICE_INTERNAL(dev);
	struct vfio_device_info device_info = { .argsz = sizeof(device_info) };
	struct vfio_region_info *reg = NULL;
	char pci_addr[PATH_MAX] = {0};
	int vfio_dev_fd;
	struct rte_pci_addr *loc = &dev->addr;
	int i, j, ret;
	struct mapped_pci_resource *vfio_res = NULL;
	struct mapped_pci_res_list *vfio_res_list =
		RTE_TAILQ_CAST(rte_vfio_tailq.head, mapped_pci_res_list);

	struct pci_map *maps;

	if (rte_intr_fd_set(dev->intr_handle, -1))
		return -1;

#ifdef HAVE_VFIO_DEV_REQ_INTERFACE
	if (rte_intr_fd_set(dev->vfio_req_intr_handle, -1))
		return -1;
#endif

	/* store PCI address string */
	snprintf(pci_addr, sizeof(pci_addr), PCI_PRI_FMT,
			loc->domain, loc->bus, loc->devid, loc->function);

	ret = rte_vfio_setup_device(rte_pci_get_sysfs_path(), pci_addr,
					&vfio_dev_fd, &device_info);
	if (ret)
		return ret;

	if (rte_intr_dev_fd_set(dev->intr_handle, vfio_dev_fd))
		goto err_vfio_dev_fd;

	/* allocate vfio_res and get region info */
	vfio_res = rte_zmalloc("VFIO_RES", sizeof(*vfio_res), 0);
	if (vfio_res == NULL) {
		RTE_LOG(ERR, EAL,
			"Cannot store VFIO mmap details\n");
		goto err_vfio_dev_fd;
	}
	memcpy(&vfio_res->pci_addr, &dev->addr, sizeof(vfio_res->pci_addr));

	/* get number of registers (up to BAR5) */
	vfio_res->nb_maps = RTE_MIN((int) device_info.num_regions,
			VFIO_PCI_BAR5_REGION_INDEX + 1);

	/* map BARs */
	maps = vfio_res->maps;

	ret = pci_vfio_get_region_info(vfio_dev_fd, &reg,
		VFIO_PCI_CONFIG_REGION_INDEX);
	if (ret < 0) {
		RTE_LOG(ERR, EAL, "%s cannot get device region info error %i (%s)\n",
			dev->name, errno, strerror(errno));
		goto err_vfio_res;
	}
	pdev->region[VFIO_PCI_CONFIG_REGION_INDEX].size = reg->size;
	pdev->region[VFIO_PCI_CONFIG_REGION_INDEX].offset = reg->offset;
	free(reg);

	vfio_res->msix_table.bar_index = -1;
	/* get MSI-X BAR, if any (we have to know where it is because we can't
	 * easily mmap it when using VFIO)
	 */
	ret = pci_vfio_get_msix_bar(dev, &vfio_res->msix_table);
	if (ret < 0) {
		RTE_LOG(ERR, EAL, "%s cannot get MSI-X BAR number!\n",
				pci_addr);
		goto err_vfio_res;
	}
	/* if we found our MSI-X BAR region, check if we can mmap it */
	if (vfio_res->msix_table.bar_index != -1) {
		int ret = pci_vfio_msix_is_mappable(vfio_dev_fd,
				vfio_res->msix_table.bar_index);
		if (ret < 0) {
			RTE_LOG(ERR, EAL, "Couldn't check if MSI-X BAR is mappable\n");
			goto err_vfio_res;
		} else if (ret != 0) {
			/* we can map it, so we don't care where it is */
			RTE_LOG(DEBUG, EAL, "VFIO reports MSI-X BAR as mappable\n");
			vfio_res->msix_table.bar_index = -1;
		}
	}

	for (i = 0; i < vfio_res->nb_maps; i++) {
		void *bar_addr;
		struct vfio_info_cap_header *hdr;
		struct vfio_region_info_cap_sparse_mmap *sparse;

		ret = pci_vfio_get_region_info(vfio_dev_fd, &reg, i);
		if (ret < 0) {
			RTE_LOG(ERR, EAL,
				"%s cannot get device region info error "
				"%i (%s)\n", pci_addr, errno, strerror(errno));
			goto err_map;
		}

		pdev->region[i].size = reg->size;
		pdev->region[i].offset = reg->offset;

		/* chk for io port region */
		ret = pci_vfio_is_ioport_bar(dev, vfio_dev_fd, i);
		if (ret < 0) {
			free(reg);
			goto err_map;
		} else if (ret) {
			RTE_LOG(INFO, EAL, "Ignore mapping IO port bar(%d)\n",
					i);
			free(reg);
			continue;
		}

		/* skip non-mmappable BARs */
		if ((reg->flags & VFIO_REGION_INFO_FLAG_MMAP) == 0) {
			free(reg);
			continue;
		}

		/* try mapping somewhere close to the end of hugepages */
		if (pci_map_addr == NULL)
			pci_map_addr = pci_find_max_end_va();

		bar_addr = pci_map_addr;
		pci_map_addr = RTE_PTR_ADD(bar_addr, (size_t) reg->size);

		pci_map_addr = RTE_PTR_ALIGN(pci_map_addr,
					sysconf(_SC_PAGE_SIZE));

		maps[i].addr = bar_addr;
		maps[i].offset = reg->offset;
		maps[i].size = reg->size;
		maps[i].path = NULL; /* vfio doesn't have per-resource paths */

		hdr = pci_vfio_info_cap(reg, VFIO_REGION_INFO_CAP_SPARSE_MMAP);

		if (hdr != NULL) {
			sparse = container_of(hdr,
				struct vfio_region_info_cap_sparse_mmap, header);
			if (sparse->nr_areas > 0) {
				maps[i].nr_areas = sparse->nr_areas;
				maps[i].areas = rte_zmalloc(NULL,
					sizeof(*maps[i].areas) * maps[i].nr_areas, 0);
				if (maps[i].areas == NULL) {
					RTE_LOG(ERR, EAL,
						"Cannot alloc memory for sparse map areas\n");
					goto err_map;
				}
				memcpy(maps[i].areas, sparse->areas,
					sizeof(*maps[i].areas) * maps[i].nr_areas);
			}
		}

		if (maps[i].nr_areas > 0) {
			ret = pci_vfio_sparse_mmap_bar(vfio_dev_fd, vfio_res, i, 0);
			if (ret < 0) {
				RTE_LOG(ERR, EAL, "%s sparse mapping BAR%i failed: %s\n",
						pci_addr, i, strerror(errno));
				free(reg);
				goto err_map;
			}
		} else {
			ret = pci_vfio_mmap_bar(vfio_dev_fd, vfio_res, i, 0);
			if (ret < 0) {
				RTE_LOG(ERR, EAL, "%s mapping BAR%i failed: %s\n",
						pci_addr, i, strerror(errno));
				free(reg);
				goto err_map;
			}
		}

		dev->mem_resource[i].addr = maps[i].addr;

		free(reg);
	}

	if (pci_rte_vfio_setup_device(dev, vfio_dev_fd) < 0) {
		RTE_LOG(ERR, EAL, "%s setup device failed\n", pci_addr);
		goto err_map;
	}

#ifdef HAVE_VFIO_DEV_REQ_INTERFACE
	if (pci_vfio_enable_notifier(dev, vfio_dev_fd) != 0) {
		RTE_LOG(ERR, EAL, "Error setting up notifier!\n");
		goto err_map;
	}

#endif
	TAILQ_INSERT_TAIL(vfio_res_list, vfio_res, next);

	return 0;
err_map:
	for (j = 0; j < i; j++) {
		if (maps[j].addr)
			pci_unmap_resource(maps[j].addr, maps[j].size);
		if (maps[j].nr_areas > 0)
			rte_free(maps[j].areas);
	}
err_vfio_res:
	rte_free(vfio_res);
err_vfio_dev_fd:
	rte_vfio_release_device(rte_pci_get_sysfs_path(),
			pci_addr, vfio_dev_fd);
	return -1;
}

static int
pci_vfio_map_resource_secondary(struct rte_pci_device *dev)
{
	struct vfio_device_info device_info = { .argsz = sizeof(device_info) };
	char pci_addr[PATH_MAX] = {0};
	int vfio_dev_fd;
	struct rte_pci_addr *loc = &dev->addr;
	int i, j, ret;
	struct mapped_pci_resource *vfio_res = NULL;
	struct mapped_pci_res_list *vfio_res_list =
		RTE_TAILQ_CAST(rte_vfio_tailq.head, mapped_pci_res_list);

	struct pci_map *maps;

	if (rte_intr_fd_set(dev->intr_handle, -1))
		return -1;
#ifdef HAVE_VFIO_DEV_REQ_INTERFACE
	if (rte_intr_fd_set(dev->vfio_req_intr_handle, -1))
		return -1;
#endif

	/* store PCI address string */
	snprintf(pci_addr, sizeof(pci_addr), PCI_PRI_FMT,
			loc->domain, loc->bus, loc->devid, loc->function);

	/* if we're in a secondary process, just find our tailq entry */
	TAILQ_FOREACH(vfio_res, vfio_res_list, next) {
		if (rte_pci_addr_cmp(&vfio_res->pci_addr,
						 &dev->addr))
			continue;
		break;
	}
	/* if we haven't found our tailq entry, something's wrong */
	if (vfio_res == NULL) {
		RTE_LOG(ERR, EAL, "%s cannot find TAILQ entry for PCI device!\n",
				pci_addr);
		return -1;
	}

	ret = rte_vfio_setup_device(rte_pci_get_sysfs_path(), pci_addr,
					&vfio_dev_fd, &device_info);
	if (ret)
		return ret;

	ret = pci_vfio_fill_regions(dev, vfio_dev_fd, &device_info);
	if (ret)
		return ret;

	/* map BARs */
	maps = vfio_res->maps;

	for (i = 0; i < vfio_res->nb_maps; i++) {
		if (maps[i].nr_areas > 0) {
			ret = pci_vfio_sparse_mmap_bar(vfio_dev_fd, vfio_res, i, MAP_FIXED);
			if (ret < 0) {
				RTE_LOG(ERR, EAL, "%s sparse mapping BAR%i failed: %s\n",
						pci_addr, i, strerror(errno));
				goto err_vfio_dev_fd;
			}
		} else {
			ret = pci_vfio_mmap_bar(vfio_dev_fd, vfio_res, i, MAP_FIXED);
			if (ret < 0) {
				RTE_LOG(ERR, EAL, "%s mapping BAR%i failed: %s\n",
						pci_addr, i, strerror(errno));
				goto err_vfio_dev_fd;
			}
		}

		dev->mem_resource[i].addr = maps[i].addr;
	}

	/* we need save vfio_dev_fd, so it can be used during release */
	if (rte_intr_dev_fd_set(dev->intr_handle, vfio_dev_fd))
		goto err_vfio_dev_fd;
#ifdef HAVE_VFIO_DEV_REQ_INTERFACE
	if (rte_intr_dev_fd_set(dev->vfio_req_intr_handle, vfio_dev_fd))
		goto err_vfio_dev_fd;
#endif

	return 0;
err_vfio_dev_fd:
	for (j = 0; j < i; j++) {
		if (maps[j].addr)
			pci_unmap_resource(maps[j].addr, maps[j].size);
	}
	rte_vfio_release_device(rte_pci_get_sysfs_path(),
			pci_addr, vfio_dev_fd);
	return -1;
}

/*
 * map the PCI resources of a PCI device in virtual memory (VFIO version).
 * primary and secondary processes follow almost exactly the same path
 */
int
pci_vfio_map_resource(struct rte_pci_device *dev)
{
	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		return pci_vfio_map_resource_primary(dev);
	else
		return pci_vfio_map_resource_secondary(dev);
}

static struct mapped_pci_resource *
find_and_unmap_vfio_resource(struct mapped_pci_res_list *vfio_res_list,
			struct rte_pci_device *dev,
			const char *pci_addr)
{
	struct mapped_pci_resource *vfio_res = NULL;
	struct pci_map *maps;
	int i;

	/* Get vfio_res */
	TAILQ_FOREACH(vfio_res, vfio_res_list, next) {
		if (rte_pci_addr_cmp(&vfio_res->pci_addr, &dev->addr))
			continue;
		break;
	}

	if (vfio_res == NULL)
		return vfio_res;

	RTE_LOG(INFO, EAL, "Releasing PCI mapped resource for %s\n",
		pci_addr);

	maps = vfio_res->maps;
	for (i = 0; i < vfio_res->nb_maps; i++) {

		/*
		 * We do not need to be aware of MSI-X table BAR mappings as
		 * when mapping. Just using current maps array is enough
		 */
		if (maps[i].addr) {
			RTE_LOG(INFO, EAL, "Calling pci_unmap_resource for %s at %p\n",
				pci_addr, maps[i].addr);
			pci_unmap_resource(maps[i].addr, maps[i].size);
		}

		if (maps[i].nr_areas > 0)
			rte_free(maps[i].areas);
	}

	return vfio_res;
}

static int
pci_vfio_unmap_resource_primary(struct rte_pci_device *dev)
{
	char pci_addr[PATH_MAX] = {0};
	struct rte_pci_addr *loc = &dev->addr;
	struct mapped_pci_resource *vfio_res = NULL;
	struct mapped_pci_res_list *vfio_res_list;
	int ret, vfio_dev_fd;

	/* store PCI address string */
	snprintf(pci_addr, sizeof(pci_addr), PCI_PRI_FMT,
			loc->domain, loc->bus, loc->devid, loc->function);

#ifdef HAVE_VFIO_DEV_REQ_INTERFACE
	ret = pci_vfio_disable_notifier(dev);
	if (ret) {
		RTE_LOG(ERR, EAL, "fail to disable req notifier.\n");
		return -1;
	}

#endif
	if (rte_intr_fd_get(dev->intr_handle) < 0)
		return -1;

	if (close(rte_intr_fd_get(dev->intr_handle)) < 0) {
		RTE_LOG(INFO, EAL, "Error when closing eventfd file descriptor for %s\n",
			pci_addr);
		return -1;
	}

	vfio_dev_fd = rte_intr_dev_fd_get(dev->intr_handle);
	if (vfio_dev_fd < 0)
		return -1;

	if (rte_pci_set_bus_master(dev, false)) {
		RTE_LOG(ERR, EAL, "%s cannot unset bus mastering for PCI device!\n",
				pci_addr);
		return -1;
	}

	ret = rte_vfio_release_device(rte_pci_get_sysfs_path(), pci_addr,
				      vfio_dev_fd);
	if (ret < 0) {
		RTE_LOG(ERR, EAL, "Cannot release VFIO device\n");
		return ret;
	}

	vfio_res_list =
		RTE_TAILQ_CAST(rte_vfio_tailq.head, mapped_pci_res_list);
	vfio_res = find_and_unmap_vfio_resource(vfio_res_list, dev, pci_addr);

	/* if we haven't found our tailq entry, something's wrong */
	if (vfio_res == NULL) {
		RTE_LOG(ERR, EAL, "%s cannot find TAILQ entry for PCI device!\n",
				pci_addr);
		return -1;
	}

	TAILQ_REMOVE(vfio_res_list, vfio_res, next);
	rte_free(vfio_res);
	return 0;
}

static int
pci_vfio_unmap_resource_secondary(struct rte_pci_device *dev)
{
	char pci_addr[PATH_MAX] = {0};
	struct rte_pci_addr *loc = &dev->addr;
	struct mapped_pci_resource *vfio_res = NULL;
	struct mapped_pci_res_list *vfio_res_list;
	int ret, vfio_dev_fd;

	/* store PCI address string */
	snprintf(pci_addr, sizeof(pci_addr), PCI_PRI_FMT,
			loc->domain, loc->bus, loc->devid, loc->function);

	vfio_dev_fd = rte_intr_dev_fd_get(dev->intr_handle);
	if (vfio_dev_fd < 0)
		return -1;

	ret = rte_vfio_release_device(rte_pci_get_sysfs_path(), pci_addr,
				      vfio_dev_fd);
	if (ret < 0) {
		RTE_LOG(ERR, EAL, "Cannot release VFIO device\n");
		return ret;
	}

	vfio_res_list =
		RTE_TAILQ_CAST(rte_vfio_tailq.head, mapped_pci_res_list);
	vfio_res = find_and_unmap_vfio_resource(vfio_res_list, dev, pci_addr);

	/* if we haven't found our tailq entry, something's wrong */
	if (vfio_res == NULL) {
		RTE_LOG(ERR, EAL, "%s cannot find TAILQ entry for PCI device!\n",
				pci_addr);
		return -1;
	}

	return 0;
}

int
pci_vfio_unmap_resource(struct rte_pci_device *dev)
{
	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		return pci_vfio_unmap_resource_primary(dev);
	else
		return pci_vfio_unmap_resource_secondary(dev);
}

int
pci_vfio_ioport_map(struct rte_pci_device *dev, int bar,
		    struct rte_pci_ioport *p)
{
	uint64_t size, offset;

	if (bar < VFIO_PCI_BAR0_REGION_INDEX ||
	    bar > VFIO_PCI_BAR5_REGION_INDEX) {
		RTE_LOG(ERR, EAL, "invalid bar (%d)!\n", bar);
		return -1;
	}

	if (pci_vfio_get_region(dev, bar, &size, &offset) != 0) {
		RTE_LOG(ERR, EAL, "Cannot get offset of region %d.\n", bar);
		return -1;
	}

	p->dev = dev;
	p->base = offset;
	return 0;
}

void
pci_vfio_ioport_read(struct rte_pci_ioport *p,
		     void *data, size_t len, off_t offset)
{
	const struct rte_intr_handle *intr_handle = p->dev->intr_handle;
	int vfio_dev_fd = rte_intr_dev_fd_get(intr_handle);

	if (vfio_dev_fd < 0)
		return;

	if (pread64(vfio_dev_fd, data,
		    len, p->base + offset) <= 0)
		RTE_LOG(ERR, EAL,
			"Can't read from PCI bar (%" PRIu64 ") : offset (%x)\n",
			VFIO_GET_REGION_IDX(p->base), (int)offset);
}

void
pci_vfio_ioport_write(struct rte_pci_ioport *p,
		      const void *data, size_t len, off_t offset)
{
	const struct rte_intr_handle *intr_handle = p->dev->intr_handle;
	int vfio_dev_fd = rte_intr_dev_fd_get(intr_handle);

	if (vfio_dev_fd < 0)
		return;

	if (pwrite64(vfio_dev_fd, data,
		     len, p->base + offset) <= 0)
		RTE_LOG(ERR, EAL,
			"Can't write to PCI bar (%" PRIu64 ") : offset (%x)\n",
			VFIO_GET_REGION_IDX(p->base), (int)offset);
}

int
pci_vfio_ioport_unmap(struct rte_pci_ioport *p)
{
	RTE_SET_USED(p);
	return -1;
}

int
pci_vfio_mmio_read(const struct rte_pci_device *dev, int bar,
		   void *buf, size_t len, off_t offs)
{
	uint64_t size, offset;
	int fd;

	fd = rte_intr_dev_fd_get(dev->intr_handle);
	if (fd < 0)
		return -1;

	if (pci_vfio_get_region(dev, bar, &size, &offset) != 0)
		return -1;

	if ((uint64_t)len + offs > size)
		return -1;

	return pread64(fd, buf, len, offset + offs);
}

int
pci_vfio_mmio_write(const struct rte_pci_device *dev, int bar,
		    const void *buf, size_t len, off_t offs)
{
	uint64_t size, offset;
	int fd;

	fd = rte_intr_dev_fd_get(dev->intr_handle);
	if (fd < 0)
		return -1;

	if (pci_vfio_get_region(dev, bar, &size, &offset) != 0)
		return -1;

	if ((uint64_t)len + offs > size)
		return -1;

	return pwrite64(fd, buf, len, offset + offs);
}

int
pci_vfio_is_enabled(void)
{
	return rte_vfio_is_enabled("vfio_pci");
}
#endif
