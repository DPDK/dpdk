/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020
 */

#ifndef _TXGBE_TYPE_H_
#define _TXGBE_TYPE_H_

#define TXGBE_ALIGN				128 /* as intel did */

#include "txgbe_osdep.h"
#include "txgbe_devids.h"

struct txgbe_mac_info {
	u8 perm_addr[ETH_ADDR_LEN];
	u32 num_rar_entries;
};

struct txgbe_hw {
	void IOMEM *hw_addr;
	void *back;
	struct txgbe_mac_info mac;

	u16 device_id;
	u16 vendor_id;
	u16 subsystem_device_id;
	u16 subsystem_vendor_id;

	bool allow_unsupported_sfp;

	uint64_t isb_dma;
	void IOMEM *isb_mem;
};

#endif /* _TXGBE_TYPE_H_ */
