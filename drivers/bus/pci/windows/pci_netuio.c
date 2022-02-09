/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation.
 */

#include <sys/queue.h>

#include <rte_windows.h>
#include <rte_errno.h>
#include <rte_log.h>
#include <rte_eal.h>

#ifdef __MINGW32__
#include <ddk/ndisguid.h>
#else
#include <ndisguid.h>
#endif

#include "private.h"
#include "pci_netuio.h"

static int
send_ioctl(HANDLE f, DWORD ioctl,
	void *in_buf, DWORD in_buf_size, void *out_buf, DWORD out_buf_size)
{
	BOOL res;
	DWORD bytes_ret = 0;

	res = DeviceIoControl(f, ioctl, in_buf, in_buf_size,
		out_buf, out_buf_size, &bytes_ret, NULL);
	if (!res) {
		RTE_LOG_WIN32_ERR("DeviceIoControl:IOCTL query failed");
		return -1;
	}

	return ERROR_SUCCESS;
}

static HDEVINFO
get_netuio_device_information_set(HDEVINFO dev_info,
	PSP_DEVINFO_DATA dev_info_data)
{
	BOOL res;
	DWORD required_size = 0;
	TCHAR dev_instance_id[MAX_DEVICENAME_SZ];
	HDEVINFO di_set = INVALID_HANDLE_VALUE;

	/* obtain the driver interface for this device */
	res = SetupDiGetDeviceInstanceId(dev_info, dev_info_data,
		dev_instance_id, sizeof(dev_instance_id), &required_size);
	if (!res) {
		RTE_LOG_WIN32_ERR("SetupDiGetDeviceInstanceId");
		goto end;
	}

	/* return the device information set for this device */
	di_set = SetupDiGetClassDevs(&GUID_DEVINTERFACE_NETUIO,
		dev_instance_id, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	if (di_set == INVALID_HANDLE_VALUE) {
		RTE_LOG_WIN32_ERR("SetupDiGetClassDevs(device information set)");
		goto end;
	}
end:
	return di_set;
}

static PSP_DEVICE_INTERFACE_DETAIL_DATA
get_netuio_device_interface_detail(HDEVINFO di_set)
{
	BOOL res;
	DWORD required_size = 0;
	SP_DEVICE_INTERFACE_DATA  dev_ifx_data = { 0 };
	PSP_DEVICE_INTERFACE_DETAIL_DATA dev_ifx_detail = NULL;

	dev_ifx_data.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

	/* enumerate the netUIO interfaces for this device information set */
	res = SetupDiEnumDeviceInterfaces(di_set, 0, &GUID_DEVINTERFACE_NETUIO,
		0, &dev_ifx_data);
	if (!res) {
		RTE_LOG_WIN32_ERR("SetupDiEnumDeviceInterfaces: no device interface");
		goto end;
	}

	/* request and allocate required size for the device interface detail */
	required_size = 0;
	res = SetupDiGetDeviceInterfaceDetail(di_set, &dev_ifx_data, NULL, 0,
		&required_size, NULL);
	if (!res) {
		/* ERROR_INSUFFICIENT_BUFFER is expected */
		if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
			RTE_LOG_WIN32_ERR("SetupDiGetDeviceInterfaceDetail");
			goto end;
		}
	}

	dev_ifx_detail = malloc(required_size);
	if (!dev_ifx_detail) {
		RTE_LOG(ERR, EAL, "Could not allocate memory for dev interface.\n");
		goto end;
	}
	dev_ifx_detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

	res = SetupDiGetDeviceInterfaceDetail(di_set, &dev_ifx_data,
		dev_ifx_detail, required_size, NULL, NULL);
	if (!res) {
		RTE_LOG_WIN32_ERR("SetupDiGetDeviceInterfaceDetail");
		free(dev_ifx_detail);
		dev_ifx_detail = NULL;
		goto end;
	}

end:
	return dev_ifx_detail;
}

/*
 * get device resource information by sending ioctl to netuio driver
 */
int
get_netuio_device_info(HDEVINFO dev_info, PSP_DEVINFO_DATA dev_info_data,
	struct rte_pci_device *dev)
{
	int ret = -1;
	HDEVINFO di_set = INVALID_HANDLE_VALUE;
	PSP_DEVICE_INTERFACE_DETAIL_DATA dev_ifx_detail = NULL;
	HANDLE netuio = INVALID_HANDLE_VALUE;
	struct device_info hw_info = { 0 };
	unsigned int idx;

	/* obtain the device information set for this device */
	di_set = get_netuio_device_information_set(dev_info, dev_info_data);
	if (di_set == INVALID_HANDLE_VALUE)
		goto end;

	/* obtain the device interface detail for this device */
	dev_ifx_detail = get_netuio_device_interface_detail(di_set);
	if (!dev_ifx_detail)
		goto end;

	/* open the kernel driver */
	netuio = CreateFile(dev_ifx_detail->DevicePath,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	if (netuio == INVALID_HANDLE_VALUE) {
		RTE_LOG_WIN32_ERR("CreateFile");
		RTE_LOG(ERR, EAL, "Unable to open driver file \"%s\".\n",
			dev_ifx_detail->DevicePath);
		goto end;
	}

	/* send ioctl to retrieve device information */
	if (send_ioctl(netuio, IOCTL_NETUIO_MAP_HW_INTO_USERSPACE, NULL, 0,
		&hw_info, sizeof(hw_info)) != ERROR_SUCCESS) {
		RTE_LOG(ERR, EAL, "Unable to send ioctl to driver.\n");
		goto end;
	}

	/* set relevant values into the dev structure */
	for (idx = 0; idx < PCI_MAX_RESOURCE; idx++) {
		dev->mem_resource[idx].phys_addr =
		    hw_info.hw[idx].phys_addr.QuadPart;
		dev->mem_resource[idx].addr =
		    hw_info.hw[idx].user_mapped_virt_addr;
		dev->mem_resource[idx].len = hw_info.hw[idx].size;
	}

	ret = ERROR_SUCCESS;
end:
	if (ret != ERROR_SUCCESS) {
		/* Only close the handle to the driver in case of an error.
		 * Otherwise, we want to keep the handle open. Closing it
		 * here will cause the driver to unmap all the process-mapped
		 * values resulting in invalid addresses.
		 */
		if (netuio != INVALID_HANDLE_VALUE)
			CloseHandle(netuio);
	}

	free(dev_ifx_detail);

	if (di_set != INVALID_HANDLE_VALUE)
		SetupDiDestroyDeviceInfoList(di_set);

	return ret;
}
