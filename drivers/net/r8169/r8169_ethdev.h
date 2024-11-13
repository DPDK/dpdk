/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Realtek Corporation. All rights reserved
 */

#ifndef R8169_ETHDEV_H
#define R8169_ETHDEV_H

#include <stdint.h>

#include <rte_ethdev.h>
#include <rte_ethdev_core.h>

#include "r8169_compat.h"

struct rtl_hw {
	u8 *mmio_addr;
};

struct rtl_sw_stats {
	u64 tx_packets;
	u64 tx_bytes;
	u64 tx_errors;
	u64 rx_packets;
	u64 rx_bytes;
	u64 rx_errors;
};

struct rtl_adapter {
	struct rtl_hw       hw;
	struct rtl_sw_stats sw_stats;
};

#endif /* R8169_ETHDEV_H */
