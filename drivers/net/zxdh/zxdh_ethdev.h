/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#ifndef ZXDH_ETHDEV_H
#define ZXDH_ETHDEV_H

#include "ethdev_driver.h"

/* ZXDH PCI vendor/device ID. */
#define ZXDH_PCI_VENDOR_ID        0x1cf2

#define ZXDH_E310_PF_DEVICEID     0x8061
#define ZXDH_E310_VF_DEVICEID     0x8062
#define ZXDH_E312_PF_DEVICEID     0x8049
#define ZXDH_E312_VF_DEVICEID     0x8060

#define ZXDH_MAX_UC_MAC_ADDRS     32
#define ZXDH_MAX_MC_MAC_ADDRS     32
#define ZXDH_MAX_MAC_ADDRS        (ZXDH_MAX_UC_MAC_ADDRS + ZXDH_MAX_MC_MAC_ADDRS)

#define ZXDH_NUM_BARS             2

struct zxdh_hw {
	struct rte_eth_dev *eth_dev;
	uint64_t bar_addr[ZXDH_NUM_BARS];

	uint32_t speed;
	uint16_t device_id;
	uint16_t port_id;

	uint8_t duplex;
	uint8_t is_pf;
};

#endif /* ZXDH_ETHDEV_H */
