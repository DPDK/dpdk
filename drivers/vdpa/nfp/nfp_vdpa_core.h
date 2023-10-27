/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Corigine, Inc.
 * All rights reserved.
 */

#ifndef __NFP_VDPA_CORE_H__
#define __NFP_VDPA_CORE_H__

#include <bus_pci_driver.h>
#include <nfp_common.h>
#include <rte_ether.h>

#define NFP_VDPA_MAX_QUEUES         1

#define NFP_VDPA_NOTIFY_ADDR_BASE        0x4000
#define NFP_VDPA_NOTIFY_ADDR_INTERVAL    0x1000

struct nfp_vdpa_hw {
	struct nfp_hw super;

	uint64_t features;
	uint64_t req_features;

	uint8_t *notify_addr[NFP_VDPA_MAX_QUEUES * 2];

	uint8_t mac_addr[RTE_ETHER_ADDR_LEN];
	uint8_t notify_region;
};

int nfp_vdpa_hw_init(struct nfp_vdpa_hw *vdpa_hw, struct rte_pci_device *dev);

#endif /* __NFP_VDPA_CORE_H__ */
