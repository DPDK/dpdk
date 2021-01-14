/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#ifndef _PCI_NETUIO_H_
#define _PCI_NETUIO_H_

#if !defined(NTDDI_WIN10_FE) || NTDDI_VERSION < NTDDI_WIN10_FE
/* GUID definition for device class netUIO */
DEFINE_GUID(GUID_DEVCLASS_NETUIO, 0x78912bc1, 0xcb8e, 0x4b28,
	0xa3, 0x29, 0xf3, 0x22, 0xeb, 0xad, 0xbe, 0x0f);

/* GUID definition for the netuio device interface */
DEFINE_GUID(GUID_DEVINTERFACE_NETUIO, 0x08336f60, 0x0679, 0x4c6c,
	0x85, 0xd2, 0xae, 0x7c, 0xed, 0x65, 0xff, 0xf7);
#endif

/* IOCTL code definitions */
#define IOCTL_NETUIO_MAP_HW_INTO_USERSPACE \
	CTL_CODE(FILE_DEVICE_NETWORK, 51, METHOD_BUFFERED, \
			 FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define  MAX_DEVICENAME_SZ 255

#pragma pack(push)
#pragma pack(8)
struct mem_region {
	UINT64 size;  /* memory region size */
	LARGE_INTEGER phys_addr;  /* physical address of the memory region */
	PVOID virt_addr;  /* virtual address of the memory region */
	PVOID user_mapped_virt_addr;  /* virtual address of the region mapped */
					/* into user process context */
};

#define PCI_MAX_BAR 6

struct device_info {
	struct mem_region hw[PCI_MAX_BAR];
};
#pragma pack(pop)

/**
 * Get device resource information by sending ioctl to netuio driver
 *
 * This function is private to EAL.
 *
 * @param dev_info
 *   HDEVINFO handle to device information set
 * @param dev_info_data
 *   SP_DEVINFO_DATA structure holding information about this enumerated device
 * @param dev
 *   PCI device context for this device
 * @return
 *   - 0 on success.
 *   - negative on error.
 */
int
get_netuio_device_info(HDEVINFO dev_info, PSP_DEVINFO_DATA dev_info_data,
	struct rte_pci_device *dev);

#endif /* _PCI_NETUIO_H_ */
