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

struct nfp_vdpa_vring {
	uint64_t desc;
	uint64_t avail;
	uint64_t used;
	uint16_t size;
	uint16_t last_avail_idx;
	uint16_t last_used_idx;
};

struct nfp_vdpa_hw {
	struct nfp_hw super;

	uint64_t features;
	uint64_t req_features;

	uint8_t *notify_addr[NFP_VDPA_MAX_QUEUES * 2];
	struct nfp_vdpa_vring vring[NFP_VDPA_MAX_QUEUES * 2];

	uint8_t mac_addr[RTE_ETHER_ADDR_LEN];
	uint8_t notify_region;
	uint8_t nr_vring;
};

int nfp_vdpa_hw_init(struct nfp_vdpa_hw *vdpa_hw, struct rte_pci_device *dev);

int nfp_vdpa_hw_start(struct nfp_vdpa_hw *vdpa_hw, int vid);

void nfp_vdpa_hw_stop(struct nfp_vdpa_hw *vdpa_hw);

void nfp_vdpa_notify_queue(struct nfp_vdpa_hw *vdpa_hw, uint16_t qid);

uint64_t nfp_vdpa_get_queue_notify_offset(struct nfp_vdpa_hw *vdpa_hw, int qid);

#endif /* __NFP_VDPA_CORE_H__ */
