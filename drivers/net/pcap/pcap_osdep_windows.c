/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021 Dmitry Kozlyuk
 */

#include <winsock2.h>
#include <iphlpapi.h>
#include <strsafe.h>

#include "pcap_osdep.h"

/*
 * Given a device name like "\Device\NPF_{GUID}" extract the "{GUID}" part.
 * Return NULL if "{GUID}" part is not found.
 */
static const char *
iface_guid(const char *name)
{
	static const size_t GUID_LENGTH = 32 + 4; /* 16 hex bytes + 4 dashes */

	const char *ob, *cb;

	ob = strchr(name, '{');
	if (ob == NULL)
		return NULL;

	cb = strchr(ob, '}');
	if (cb == NULL || cb - ob != GUID_LENGTH + 1) /* + 1 opening '{' */
		return NULL;

	return ob;
}

/*
 * libpcap takes device names like "\Device\NPF_{GUID}",
 * GetAdapterIndex() takes interface names like "\DEVICE\TCPIP_{GUID}".
 * Try to convert, fall back to original device name.
 */
int
osdep_iface_index_get(const char *device_name)
{
	WCHAR adapter_name[MAX_ADAPTER_NAME_LENGTH];
	const char *guid;
	ULONG index;
	DWORD ret;

	guid = iface_guid(device_name);
	if (guid != NULL)
		StringCbPrintfW(adapter_name, sizeof(adapter_name),
			L"\\DEVICE\\TCPIP_%S", guid);
	else
		StringCbPrintfW(adapter_name, sizeof(adapter_name),
			L"%S", device_name);

	ret = GetAdapterIndex(adapter_name, &index);
	if (ret != NO_ERROR) {
		PMD_LOG(ERR, "GetAdapterIndex(%S) = %lu", adapter_name, ret);
		return -1;
	}

	return index;
}

/*
 * Helper function to get adapter information by name.
 * Returns adapter info on success, NULL on failure.
 * Caller must free the returned buffer.
 */
static IP_ADAPTER_ADDRESSES *
get_adapter_addresses(void)
{
	IP_ADAPTER_ADDRESSES *info = NULL;
	ULONG size;
	DWORD sys_ret;

	sys_ret = GetAdaptersAddresses(AF_UNSPEC, 0, NULL, NULL, &size);
	if (sys_ret != ERROR_BUFFER_OVERFLOW) {
		PMD_LOG(ERR, "GetAdapterAddresses() = %lu, expected %lu",
			sys_ret, ERROR_BUFFER_OVERFLOW);
		return NULL;
	}

	info = (IP_ADAPTER_ADDRESSES *)malloc(size);
	if (info == NULL) {
		PMD_LOG(ERR, "Cannot allocate adapter address info");
		return NULL;
	}

	sys_ret = GetAdaptersAddresses(AF_UNSPEC, 0, NULL, info, &size);
	if (sys_ret != ERROR_SUCCESS) {
		PMD_LOG(ERR, "GetAdapterAddresses() = %lu", sys_ret);
		free(info);
		return NULL;
	}

	return info;
}

/*
 * libpcap takes device names like "\Device\NPF_{GUID}",
 * GetAdaptersAddresses() returns names in "{GUID}" form.
 * Try to extract GUID from device name, fall back to original device name.
 */
int
osdep_iface_mac_get(const char *device_name, struct rte_ether_addr *mac)
{
	IP_ADAPTER_ADDRESSES *info = NULL, *cur = NULL;
	const char *adapter_name;
	int ret = -1;

	info = get_adapter_addresses();
	if (info == NULL)
		return -1;

	adapter_name = iface_guid(device_name);
	if (adapter_name == NULL)
		adapter_name = device_name;

	for (cur = info; cur != NULL; cur = cur->Next) {
		if (strcmp(cur->AdapterName, adapter_name) == 0) {
			if (cur->PhysicalAddressLength != RTE_ETHER_ADDR_LEN) {
				PMD_LOG(ERR, "Physical address length: want %u, got %lu",
					RTE_ETHER_ADDR_LEN,
					cur->PhysicalAddressLength);
				break;
			}

			memcpy(mac->addr_bytes, cur->PhysicalAddress,
				RTE_ETHER_ADDR_LEN);
			ret = 0;
			break;
		}
	}

	free(info);
	return ret;
}

int
osdep_iface_link_status(const char *device_name)
{
	IP_ADAPTER_ADDRESSES *info, *cur;
	const char *adapter_name;
	int ret = -1;

	info = get_adapter_addresses();
	if (info == NULL)
		return -1;

	adapter_name = iface_guid(device_name);
	if (adapter_name == NULL)
		adapter_name = device_name;

	for (cur = info; cur != NULL; cur = cur->Next) {
		if (strcmp(cur->AdapterName, adapter_name) == 0) {
			ret = (cur->OperStatus == IfOperStatusUp) ? 1 : 0;
			break;
		}
	}

	free(info);
	return ret;
}
