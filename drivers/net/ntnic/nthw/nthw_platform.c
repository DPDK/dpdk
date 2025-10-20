/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "nthw_platform_drv.h"

nthw_adapter_id_t nthw_platform_get_adapter_id(const uint16_t n_pci_device_id)
{
	switch (n_pci_device_id) {
	case NT_HW_PCI_DEVICE_ID_NT200A02:
		return NT_HW_ADAPTER_ID_NT200A02;

	case NT_HW_PCI_DEVICE_ID_NT400D13:
		return NT_HW_ADAPTER_ID_NT400D13;

	default:
		return NT_HW_ADAPTER_ID_UNKNOWN;
	}
}
