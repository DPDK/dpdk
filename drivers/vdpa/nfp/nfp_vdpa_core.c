/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Corigine, Inc.
 * All rights reserved.
 */

#include "nfp_vdpa_core.h"

#include <rte_vhost.h>

#include "nfp_vdpa_log.h"

/* Available and used descs are in same order */
#ifndef VIRTIO_F_IN_ORDER
#define VIRTIO_F_IN_ORDER      35
#endif

int
nfp_vdpa_hw_init(struct nfp_vdpa_hw *vdpa_hw,
		struct rte_pci_device *pci_dev)
{
	uint32_t queue;
	struct nfp_hw *hw;
	uint8_t *notify_base;

	hw = &vdpa_hw->super;
	hw->ctrl_bar = pci_dev->mem_resource[0].addr;
	if (hw->ctrl_bar == NULL) {
		DRV_CORE_LOG(ERR, "hw->ctrl_bar is NULL. BAR0 not configured.");
		return -ENODEV;
	}

	notify_base = hw->ctrl_bar + NFP_VDPA_NOTIFY_ADDR_BASE;
	for (queue = 0; queue < NFP_VDPA_MAX_QUEUES; queue++) {
		uint32_t idx = queue * 2;

		/* RX */
		vdpa_hw->notify_addr[idx] = notify_base;
		notify_base += NFP_VDPA_NOTIFY_ADDR_INTERVAL;
		/* TX */
		vdpa_hw->notify_addr[idx + 1] = notify_base;
		notify_base += NFP_VDPA_NOTIFY_ADDR_INTERVAL;

		vdpa_hw->notify_region = queue;
		DRV_CORE_LOG(DEBUG, "notify_addr[%d] at %p, notify_addr[%d] at %p",
				idx, vdpa_hw->notify_addr[idx],
				idx + 1, vdpa_hw->notify_addr[idx + 1]);
	}

	vdpa_hw->features = (1ULL << VIRTIO_F_VERSION_1) |
			(1ULL << VIRTIO_F_IN_ORDER) |
			(1ULL << VHOST_USER_F_PROTOCOL_FEATURES);

	return 0;
}
