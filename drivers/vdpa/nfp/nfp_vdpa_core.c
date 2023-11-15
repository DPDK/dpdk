/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Corigine, Inc.
 * All rights reserved.
 */

#include "nfp_vdpa_core.h"

#include <nfp_common.h>
#include <rte_vhost.h>

#include "nfp_vdpa_log.h"

/* Available and used descs are in same order */
#ifndef VIRTIO_F_IN_ORDER
#define VIRTIO_F_IN_ORDER      35
#endif

#define NFP_QCP_NOTIFY_MAX_ADD    0x7f

enum nfp_qcp_notify_ptr {
	NFP_QCP_NOTIFY_WRITE_PTR = 0,
	NFP_QCP_NOTIFY_READ_PTR
};

/**
 * Add the value to the selected pointer of a queue
 *
 * @param queue
 *   Base address for queue structure
 * @param ptr
 *   Add to the Read or Write pointer
 * @param val
 *   Value to add to the queue pointer
 */
static inline void
nfp_qcp_notify_ptr_add(uint8_t *q,
		enum nfp_qcp_notify_ptr ptr,
		uint32_t val)
{
	uint32_t off;

	if (ptr == NFP_QCP_NOTIFY_WRITE_PTR)
		off = NFP_QCP_QUEUE_ADD_RPTR;
	else
		off = NFP_QCP_QUEUE_ADD_WPTR;

	for (; val > NFP_QCP_NOTIFY_MAX_ADD; val -= NFP_QCP_NOTIFY_MAX_ADD)
		nn_writel(rte_cpu_to_le_32(NFP_QCP_NOTIFY_MAX_ADD), q + off);

	nn_writel(rte_cpu_to_le_32(val), q + off);
}

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

static uint32_t
nfp_vdpa_check_offloads(void)
{
	return NFP_NET_CFG_CTRL_SCATTER |
			NFP_NET_CFG_CTRL_IN_ORDER;
}

int
nfp_vdpa_hw_start(struct nfp_vdpa_hw *vdpa_hw,
		int vid)
{
	int ret;
	uint32_t update;
	uint32_t new_ctrl;
	struct timespec wait_tst;
	struct nfp_hw *hw = &vdpa_hw->super;
	uint8_t mac_addr[RTE_ETHER_ADDR_LEN];

	nn_cfg_writeq(hw, NFP_NET_CFG_TXR_ADDR(0), vdpa_hw->vring[1].desc);
	nn_cfg_writeb(hw, NFP_NET_CFG_TXR_SZ(0), rte_log2_u32(vdpa_hw->vring[1].size));
	nn_cfg_writeq(hw, NFP_NET_CFG_TXR_ADDR(1), vdpa_hw->vring[1].avail);
	nn_cfg_writeq(hw, NFP_NET_CFG_TXR_ADDR(2), vdpa_hw->vring[1].used);

	nn_cfg_writeq(hw, NFP_NET_CFG_RXR_ADDR(0), vdpa_hw->vring[0].desc);
	nn_cfg_writeb(hw, NFP_NET_CFG_RXR_SZ(0), rte_log2_u32(vdpa_hw->vring[0].size));
	nn_cfg_writeq(hw, NFP_NET_CFG_RXR_ADDR(1), vdpa_hw->vring[0].avail);
	nn_cfg_writeq(hw, NFP_NET_CFG_RXR_ADDR(2), vdpa_hw->vring[0].used);

	rte_wmb();

	nfp_disable_queues(hw);
	nfp_enable_queues(hw, NFP_VDPA_MAX_QUEUES, NFP_VDPA_MAX_QUEUES);

	new_ctrl = nfp_vdpa_check_offloads();

	nn_cfg_writel(hw, NFP_NET_CFG_MTU, 9216);
	nn_cfg_writel(hw, NFP_NET_CFG_FLBUFSZ, 10240);

	/* TODO: Temporary set MAC to fixed value fe:1b:ac:05:a5:22 */
	mac_addr[0] = 0xfe;
	mac_addr[1] = 0x1b;
	mac_addr[2] = 0xac;
	mac_addr[3] = 0x05;
	mac_addr[4] = 0xa5;
	mac_addr[5] = (0x22 + vid);

	/* Writing new MAC to the specific port BAR address */
	nfp_write_mac(hw, (uint8_t *)mac_addr);

	/* Enable device */
	new_ctrl |= NFP_NET_CFG_CTRL_ENABLE;

	/* Signal the NIC about the change */
	update = NFP_NET_CFG_UPDATE_MACADDR |
			NFP_NET_CFG_UPDATE_GEN |
			NFP_NET_CFG_UPDATE_RING;

	ret = nfp_reconfig(hw, new_ctrl, update);
	if (ret < 0)
		return -EIO;

	hw->ctrl = new_ctrl;

	DRV_CORE_LOG(DEBUG, "Enabling the device, sleep 1 seconds...");
	wait_tst.tv_sec = 1;
	wait_tst.tv_nsec = 0;
	nanosleep(&wait_tst, 0);

	return 0;
}

void
nfp_vdpa_hw_stop(struct nfp_vdpa_hw *vdpa_hw)
{
	nfp_disable_queues(&vdpa_hw->super);
}

/*
 * This offset is used for mmaping the notify area. It implies it needs
 * to be a multiple of PAGE_SIZE.
 * For debugging, using notify region 0 with an offset of 4K. This should
 * point to the conf bar.
 */
uint64_t
nfp_vdpa_get_queue_notify_offset(struct nfp_vdpa_hw *vdpa_hw __rte_unused,
		int qid)
{
	return NFP_VDPA_NOTIFY_ADDR_BASE + ((uint64_t)qid * NFP_VDPA_NOTIFY_ADDR_INTERVAL);
}

/*
 * With just one queue the increment is 0, which does not
 * incremente the counter but will raise a queue event due
 * to queue configured for watermark events.
 */
void
nfp_vdpa_notify_queue(struct nfp_vdpa_hw *vdpa_hw,
		uint16_t qid)
{
	nfp_qcp_notify_ptr_add(vdpa_hw->notify_addr[qid],
			NFP_QCP_NOTIFY_WRITE_PTR, qid);
}
