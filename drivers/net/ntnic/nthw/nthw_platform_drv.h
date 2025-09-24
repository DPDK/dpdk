/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __NTHW_PLATFORM_DRV_H__
#define __NTHW_PLATFORM_DRV_H__

#include <stdint.h>

#define NT_HW_PCI_VENDOR_ID (0x18f4)
#define NT_HW_PCI_DEVICE_ID_NT200A02 (0x1C5)
#define NT_HW_PCI_DEVICE_ID_NT400D13 (0x295)

enum nthw_adapter_id_e {
	NT_HW_ADAPTER_ID_UNKNOWN = 0,
	NT_HW_ADAPTER_ID_NT200A02,
	NT_HW_ADAPTER_ID_NT400D13,
};

typedef enum nthw_adapter_id_e nthw_adapter_id_t;

nthw_adapter_id_t nthw_platform_get_adapter_id(const uint16_t n_pci_device_id);

#endif	/* __NTHW_PLATFORM_DRV_H__ */
