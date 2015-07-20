/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#ifdef RTE_EXEC_ENV_LINUXAPP
#include <dirent.h>
#include <fcntl.h>
#endif

#include <rte_ethdev.h>
#include <rte_memcpy.h>
#include <rte_string_fns.h>
#include <rte_memzone.h>
#include <rte_malloc.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_pci.h>
#include <rte_ether.h>
#include <rte_common.h>
#include <rte_errno.h>

#include <rte_memory.h>
#include <rte_eal.h>
#include <rte_dev.h>

#include "virtio_ethdev.h"
#include "virtio_pci.h"
#include "virtio_logs.h"
#include "virtqueue.h"


static int eth_virtio_dev_init(struct rte_eth_dev *eth_dev);
static int eth_virtio_dev_uninit(struct rte_eth_dev *eth_dev);
static int  virtio_dev_configure(struct rte_eth_dev *dev);
static int  virtio_dev_start(struct rte_eth_dev *dev);
static void virtio_dev_stop(struct rte_eth_dev *dev);
static void virtio_dev_promiscuous_enable(struct rte_eth_dev *dev);
static void virtio_dev_promiscuous_disable(struct rte_eth_dev *dev);
static void virtio_dev_allmulticast_enable(struct rte_eth_dev *dev);
static void virtio_dev_allmulticast_disable(struct rte_eth_dev *dev);
static void virtio_dev_info_get(struct rte_eth_dev *dev,
				struct rte_eth_dev_info *dev_info);
static int virtio_dev_link_update(struct rte_eth_dev *dev,
	__rte_unused int wait_to_complete);

static void virtio_set_hwaddr(struct virtio_hw *hw);
static void virtio_get_hwaddr(struct virtio_hw *hw);

static void virtio_dev_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats);
static void virtio_dev_stats_reset(struct rte_eth_dev *dev);
static void virtio_dev_free_mbufs(struct rte_eth_dev *dev);
static int virtio_vlan_filter_set(struct rte_eth_dev *dev,
				uint16_t vlan_id, int on);
static void virtio_mac_addr_add(struct rte_eth_dev *dev,
				struct ether_addr *mac_addr,
				uint32_t index, uint32_t vmdq __rte_unused);
static void virtio_mac_addr_remove(struct rte_eth_dev *dev, uint32_t index);
static void virtio_mac_addr_set(struct rte_eth_dev *dev,
				struct ether_addr *mac_addr);

static int virtio_dev_queue_stats_mapping_set(
	__rte_unused struct rte_eth_dev *eth_dev,
	__rte_unused uint16_t queue_id,
	__rte_unused uint8_t stat_idx,
	__rte_unused uint8_t is_rx);

/*
 * The set of PCI devices this driver supports
 */
static const struct rte_pci_id pci_id_virtio_map[] = {

#define RTE_PCI_DEV_ID_DECL_VIRTIO(vend, dev) {RTE_PCI_DEVICE(vend, dev)},
#include "rte_pci_dev_ids.h"

{ .vendor_id = 0, /* sentinel */ },
};

static int
virtio_send_command(struct virtqueue *vq, struct virtio_pmd_ctrl *ctrl,
		int *dlen, int pkt_num)
{
	uint32_t head, i;
	int k, sum = 0;
	virtio_net_ctrl_ack status = ~0;
	struct virtio_pmd_ctrl result;

	ctrl->status = status;

	if (!(vq && vq->hw->cvq)) {
		PMD_INIT_LOG(ERR,
			     "%s(): Control queue is not supported.",
			     __func__);
		return -1;
	}
	head = vq->vq_desc_head_idx;

	PMD_INIT_LOG(DEBUG, "vq->vq_desc_head_idx = %d, status = %d, "
		"vq->hw->cvq = %p vq = %p",
		vq->vq_desc_head_idx, status, vq->hw->cvq, vq);

	if ((vq->vq_free_cnt < ((uint32_t)pkt_num + 2)) || (pkt_num < 1))
		return -1;

	memcpy(vq->virtio_net_hdr_mz->addr, ctrl,
		sizeof(struct virtio_pmd_ctrl));

	/*
	 * Format is enforced in qemu code:
	 * One TX packet for header;
	 * At least one TX packet per argument;
	 * One RX packet for ACK.
	 */
	vq->vq_ring.desc[head].flags = VRING_DESC_F_NEXT;
	vq->vq_ring.desc[head].addr = vq->virtio_net_hdr_mz->phys_addr;
	vq->vq_ring.desc[head].len = sizeof(struct virtio_net_ctrl_hdr);
	vq->vq_free_cnt--;
	i = vq->vq_ring.desc[head].next;

	for (k = 0; k < pkt_num; k++) {
		vq->vq_ring.desc[i].flags = VRING_DESC_F_NEXT;
		vq->vq_ring.desc[i].addr = vq->virtio_net_hdr_mz->phys_addr
			+ sizeof(struct virtio_net_ctrl_hdr)
			+ sizeof(ctrl->status) + sizeof(uint8_t)*sum;
		vq->vq_ring.desc[i].len = dlen[k];
		sum += dlen[k];
		vq->vq_free_cnt--;
		i = vq->vq_ring.desc[i].next;
	}

	vq->vq_ring.desc[i].flags = VRING_DESC_F_WRITE;
	vq->vq_ring.desc[i].addr = vq->virtio_net_hdr_mz->phys_addr
			+ sizeof(struct virtio_net_ctrl_hdr);
	vq->vq_ring.desc[i].len = sizeof(ctrl->status);
	vq->vq_free_cnt--;

	vq->vq_desc_head_idx = vq->vq_ring.desc[i].next;

	vq_update_avail_ring(vq, head);
	vq_update_avail_idx(vq);

	PMD_INIT_LOG(DEBUG, "vq->vq_queue_index = %d", vq->vq_queue_index);

	virtqueue_notify(vq);

	rte_rmb();
	while (vq->vq_used_cons_idx == vq->vq_ring.used->idx) {
		rte_rmb();
		usleep(100);
	}

	while (vq->vq_used_cons_idx != vq->vq_ring.used->idx) {
		uint32_t idx, desc_idx, used_idx;
		struct vring_used_elem *uep;

		used_idx = (uint32_t)(vq->vq_used_cons_idx
				& (vq->vq_nentries - 1));
		uep = &vq->vq_ring.used->ring[used_idx];
		idx = (uint32_t) uep->id;
		desc_idx = idx;

		while (vq->vq_ring.desc[desc_idx].flags & VRING_DESC_F_NEXT) {
			desc_idx = vq->vq_ring.desc[desc_idx].next;
			vq->vq_free_cnt++;
		}

		vq->vq_ring.desc[desc_idx].next = vq->vq_desc_head_idx;
		vq->vq_desc_head_idx = idx;

		vq->vq_used_cons_idx++;
		vq->vq_free_cnt++;
	}

	PMD_INIT_LOG(DEBUG, "vq->vq_free_cnt=%d\nvq->vq_desc_head_idx=%d",
			vq->vq_free_cnt, vq->vq_desc_head_idx);

	memcpy(&result, vq->virtio_net_hdr_mz->addr,
			sizeof(struct virtio_pmd_ctrl));

	return result.status;
}

static int
virtio_set_multiple_queues(struct rte_eth_dev *dev, uint16_t nb_queues)
{
	struct virtio_hw *hw = dev->data->dev_private;
	struct virtio_pmd_ctrl ctrl;
	int dlen[1];
	int ret;

	ctrl.hdr.class = VIRTIO_NET_CTRL_MQ;
	ctrl.hdr.cmd = VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET;
	memcpy(ctrl.data, &nb_queues, sizeof(uint16_t));

	dlen[0] = sizeof(uint16_t);

	ret = virtio_send_command(hw->cvq, &ctrl, dlen, 1);
	if (ret) {
		PMD_INIT_LOG(ERR, "Multiqueue configured but send command "
			  "failed, this is too late now...");
		return -EINVAL;
	}

	return 0;
}

void
virtio_dev_queue_release(struct virtqueue *vq) {
	struct virtio_hw *hw = vq->hw;

	if (vq) {
		/* Select and deactivate the queue */
		VIRTIO_WRITE_REG_2(hw, VIRTIO_PCI_QUEUE_SEL, vq->queue_id);
		VIRTIO_WRITE_REG_4(hw, VIRTIO_PCI_QUEUE_PFN, 0);

		rte_free(vq);
		vq = NULL;
	}
}

int virtio_dev_queue_setup(struct rte_eth_dev *dev,
			int queue_type,
			uint16_t queue_idx,
			uint16_t vtpci_queue_idx,
			uint16_t nb_desc,
			unsigned int socket_id,
			struct virtqueue **pvq)
{
	char vq_name[VIRTQUEUE_MAX_NAME_SZ];
	const struct rte_memzone *mz;
	uint16_t vq_size;
	int size;
	struct virtio_hw *hw = dev->data->dev_private;
	struct virtqueue *vq = NULL;

	/* Write the virtqueue index to the Queue Select Field */
	VIRTIO_WRITE_REG_2(hw, VIRTIO_PCI_QUEUE_SEL, vtpci_queue_idx);
	PMD_INIT_LOG(DEBUG, "selecting queue: %d", vtpci_queue_idx);

	/*
	 * Read the virtqueue size from the Queue Size field
	 * Always power of 2 and if 0 virtqueue does not exist
	 */
	vq_size = VIRTIO_READ_REG_2(hw, VIRTIO_PCI_QUEUE_NUM);
	PMD_INIT_LOG(DEBUG, "vq_size: %d nb_desc:%d", vq_size, nb_desc);
	if (vq_size == 0) {
		PMD_INIT_LOG(ERR, "%s: virtqueue does not exist", __func__);
		return -EINVAL;
	}

	if (!rte_is_power_of_2(vq_size)) {
		PMD_INIT_LOG(ERR, "%s: virtqueue size is not powerof 2", __func__);
		return -EINVAL;
	}

	if (queue_type == VTNET_RQ) {
		snprintf(vq_name, sizeof(vq_name), "port%d_rvq%d",
			dev->data->port_id, queue_idx);
		vq = rte_zmalloc(vq_name, sizeof(struct virtqueue) +
			vq_size * sizeof(struct vq_desc_extra), RTE_CACHE_LINE_SIZE);
	} else if (queue_type == VTNET_TQ) {
		snprintf(vq_name, sizeof(vq_name), "port%d_tvq%d",
			dev->data->port_id, queue_idx);
		vq = rte_zmalloc(vq_name, sizeof(struct virtqueue) +
			vq_size * sizeof(struct vq_desc_extra), RTE_CACHE_LINE_SIZE);
	} else if (queue_type == VTNET_CQ) {
		snprintf(vq_name, sizeof(vq_name), "port%d_cvq",
			dev->data->port_id);
		vq = rte_zmalloc(vq_name, sizeof(struct virtqueue) +
			vq_size * sizeof(struct vq_desc_extra),
			RTE_CACHE_LINE_SIZE);
	}
	if (vq == NULL) {
		PMD_INIT_LOG(ERR, "%s: Can not allocate virtqueue", __func__);
		return (-ENOMEM);
	}

	vq->hw = hw;
	vq->port_id = dev->data->port_id;
	vq->queue_id = queue_idx;
	vq->vq_queue_index = vtpci_queue_idx;
	vq->vq_nentries = vq_size;

	if (nb_desc == 0 || nb_desc > vq_size)
		nb_desc = vq_size;
	vq->vq_free_cnt = nb_desc;

	/*
	 * Reserve a memzone for vring elements
	 */
	size = vring_size(vq_size, VIRTIO_PCI_VRING_ALIGN);
	vq->vq_ring_size = RTE_ALIGN_CEIL(size, VIRTIO_PCI_VRING_ALIGN);
	PMD_INIT_LOG(DEBUG, "vring_size: %d, rounded_vring_size: %d", size, vq->vq_ring_size);

	mz = rte_memzone_reserve_aligned(vq_name, vq->vq_ring_size,
		socket_id, 0, VIRTIO_PCI_VRING_ALIGN);
	if (mz == NULL) {
		if (rte_errno == EEXIST)
			mz = rte_memzone_lookup(vq_name);
		if (mz == NULL) {
			rte_free(vq);
			return -ENOMEM;
		}
	}

	/*
	 * Virtio PCI device VIRTIO_PCI_QUEUE_PF register is 32bit,
	 * and only accepts 32 bit page frame number.
	 * Check if the allocated physical memory exceeds 16TB.
	 */
	if ((mz->phys_addr + vq->vq_ring_size - 1) >> (VIRTIO_PCI_QUEUE_ADDR_SHIFT + 32)) {
		PMD_INIT_LOG(ERR, "vring address shouldn't be above 16TB!");
		rte_free(vq);
		return -ENOMEM;
	}

	memset(mz->addr, 0, sizeof(mz->len));
	vq->mz = mz;
	vq->vq_ring_mem = mz->phys_addr;
	vq->vq_ring_virt_mem = mz->addr;
	PMD_INIT_LOG(DEBUG, "vq->vq_ring_mem:      0x%"PRIx64, (uint64_t)mz->phys_addr);
	PMD_INIT_LOG(DEBUG, "vq->vq_ring_virt_mem: 0x%"PRIx64, (uint64_t)(uintptr_t)mz->addr);
	vq->virtio_net_hdr_mz  = NULL;
	vq->virtio_net_hdr_mem = 0;

	if (queue_type == VTNET_TQ) {
		/*
		 * For each xmit packet, allocate a virtio_net_hdr
		 */
		snprintf(vq_name, sizeof(vq_name), "port%d_tvq%d_hdrzone",
			dev->data->port_id, queue_idx);
		vq->virtio_net_hdr_mz = rte_memzone_reserve_aligned(vq_name,
			vq_size * hw->vtnet_hdr_size,
			socket_id, 0, RTE_CACHE_LINE_SIZE);
		if (vq->virtio_net_hdr_mz == NULL) {
			if (rte_errno == EEXIST)
				vq->virtio_net_hdr_mz =
					rte_memzone_lookup(vq_name);
			if (vq->virtio_net_hdr_mz == NULL) {
				rte_free(vq);
				return -ENOMEM;
			}
		}
		vq->virtio_net_hdr_mem =
			vq->virtio_net_hdr_mz->phys_addr;
		memset(vq->virtio_net_hdr_mz->addr, 0,
			vq_size * hw->vtnet_hdr_size);
	} else if (queue_type == VTNET_CQ) {
		/* Allocate a page for control vq command, data and status */
		snprintf(vq_name, sizeof(vq_name), "port%d_cvq_hdrzone",
			dev->data->port_id);
		vq->virtio_net_hdr_mz = rte_memzone_reserve_aligned(vq_name,
			PAGE_SIZE, socket_id, 0, RTE_CACHE_LINE_SIZE);
		if (vq->virtio_net_hdr_mz == NULL) {
			if (rte_errno == EEXIST)
				vq->virtio_net_hdr_mz =
					rte_memzone_lookup(vq_name);
			if (vq->virtio_net_hdr_mz == NULL) {
				rte_free(vq);
				return -ENOMEM;
			}
		}
		vq->virtio_net_hdr_mem =
			vq->virtio_net_hdr_mz->phys_addr;
		memset(vq->virtio_net_hdr_mz->addr, 0, PAGE_SIZE);
	}

	/*
	 * Set guest physical address of the virtqueue
	 * in VIRTIO_PCI_QUEUE_PFN config register of device
	 */
	VIRTIO_WRITE_REG_4(hw, VIRTIO_PCI_QUEUE_PFN,
			mz->phys_addr >> VIRTIO_PCI_QUEUE_ADDR_SHIFT);
	*pvq = vq;
	return 0;
}

static int
virtio_dev_cq_queue_setup(struct rte_eth_dev *dev, uint16_t vtpci_queue_idx,
		uint32_t socket_id)
{
	struct virtqueue *vq;
	int ret;
	struct virtio_hw *hw = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();
	ret = virtio_dev_queue_setup(dev, VTNET_CQ, VTNET_SQ_CQ_QUEUE_IDX,
			vtpci_queue_idx, 0, socket_id, &vq);
	if (ret < 0) {
		PMD_INIT_LOG(ERR, "control vq initialization failed");
		return ret;
	}

	hw->cvq = vq;
	return 0;
}

static void
virtio_free_queues(struct rte_eth_dev *dev)
{
	unsigned int i;

	for (i = 0; i < dev->data->nb_rx_queues; i++)
		virtio_dev_rx_queue_release(dev->data->rx_queues[i]);

	dev->data->nb_rx_queues = 0;

	for (i = 0; i < dev->data->nb_tx_queues; i++)
		virtio_dev_tx_queue_release(dev->data->tx_queues[i]);

	dev->data->nb_tx_queues = 0;
}

static void
virtio_dev_close(struct rte_eth_dev *dev)
{
	struct virtio_hw *hw = dev->data->dev_private;
	struct rte_pci_device *pci_dev = dev->pci_dev;

	PMD_INIT_LOG(DEBUG, "virtio_dev_close");

	/* reset the NIC */
	if (pci_dev->driver->drv_flags & RTE_PCI_DRV_INTR_LSC)
		vtpci_irq_config(hw, VIRTIO_MSI_NO_VECTOR);
	vtpci_reset(hw);
	hw->started = 0;
	virtio_dev_free_mbufs(dev);
	virtio_free_queues(dev);
}

static void
virtio_dev_promiscuous_enable(struct rte_eth_dev *dev)
{
	struct virtio_hw *hw = dev->data->dev_private;
	struct virtio_pmd_ctrl ctrl;
	int dlen[1];
	int ret;

	if (!vtpci_with_feature(hw, VIRTIO_NET_F_CTRL_RX)) {
		PMD_INIT_LOG(INFO, "host does not support rx control\n");
		return;
	}

	ctrl.hdr.class = VIRTIO_NET_CTRL_RX;
	ctrl.hdr.cmd = VIRTIO_NET_CTRL_RX_PROMISC;
	ctrl.data[0] = 1;
	dlen[0] = 1;

	ret = virtio_send_command(hw->cvq, &ctrl, dlen, 1);
	if (ret)
		PMD_INIT_LOG(ERR, "Failed to enable promisc");
}

static void
virtio_dev_promiscuous_disable(struct rte_eth_dev *dev)
{
	struct virtio_hw *hw = dev->data->dev_private;
	struct virtio_pmd_ctrl ctrl;
	int dlen[1];
	int ret;

	if (!vtpci_with_feature(hw, VIRTIO_NET_F_CTRL_RX)) {
		PMD_INIT_LOG(INFO, "host does not support rx control\n");
		return;
	}

	ctrl.hdr.class = VIRTIO_NET_CTRL_RX;
	ctrl.hdr.cmd = VIRTIO_NET_CTRL_RX_PROMISC;
	ctrl.data[0] = 0;
	dlen[0] = 1;

	ret = virtio_send_command(hw->cvq, &ctrl, dlen, 1);
	if (ret)
		PMD_INIT_LOG(ERR, "Failed to disable promisc");
}

static void
virtio_dev_allmulticast_enable(struct rte_eth_dev *dev)
{
	struct virtio_hw *hw = dev->data->dev_private;
	struct virtio_pmd_ctrl ctrl;
	int dlen[1];
	int ret;

	if (!vtpci_with_feature(hw, VIRTIO_NET_F_CTRL_RX)) {
		PMD_INIT_LOG(INFO, "host does not support rx control\n");
		return;
	}

	ctrl.hdr.class = VIRTIO_NET_CTRL_RX;
	ctrl.hdr.cmd = VIRTIO_NET_CTRL_RX_ALLMULTI;
	ctrl.data[0] = 1;
	dlen[0] = 1;

	ret = virtio_send_command(hw->cvq, &ctrl, dlen, 1);
	if (ret)
		PMD_INIT_LOG(ERR, "Failed to enable allmulticast");
}

static void
virtio_dev_allmulticast_disable(struct rte_eth_dev *dev)
{
	struct virtio_hw *hw = dev->data->dev_private;
	struct virtio_pmd_ctrl ctrl;
	int dlen[1];
	int ret;

	if (!vtpci_with_feature(hw, VIRTIO_NET_F_CTRL_RX)) {
		PMD_INIT_LOG(INFO, "host does not support rx control\n");
		return;
	}

	ctrl.hdr.class = VIRTIO_NET_CTRL_RX;
	ctrl.hdr.cmd = VIRTIO_NET_CTRL_RX_ALLMULTI;
	ctrl.data[0] = 0;
	dlen[0] = 1;

	ret = virtio_send_command(hw->cvq, &ctrl, dlen, 1);
	if (ret)
		PMD_INIT_LOG(ERR, "Failed to disable allmulticast");
}

/*
 * dev_ops for virtio, bare necessities for basic operation
 */
static const struct eth_dev_ops virtio_eth_dev_ops = {
	.dev_configure           = virtio_dev_configure,
	.dev_start               = virtio_dev_start,
	.dev_stop                = virtio_dev_stop,
	.dev_close               = virtio_dev_close,
	.promiscuous_enable      = virtio_dev_promiscuous_enable,
	.promiscuous_disable     = virtio_dev_promiscuous_disable,
	.allmulticast_enable     = virtio_dev_allmulticast_enable,
	.allmulticast_disable    = virtio_dev_allmulticast_disable,

	.dev_infos_get           = virtio_dev_info_get,
	.stats_get               = virtio_dev_stats_get,
	.stats_reset             = virtio_dev_stats_reset,
	.link_update             = virtio_dev_link_update,
	.rx_queue_setup          = virtio_dev_rx_queue_setup,
	.rx_queue_release        = virtio_dev_rx_queue_release,
	.tx_queue_setup          = virtio_dev_tx_queue_setup,
	.tx_queue_release        = virtio_dev_tx_queue_release,
	/* collect stats per queue */
	.queue_stats_mapping_set = virtio_dev_queue_stats_mapping_set,
	.vlan_filter_set         = virtio_vlan_filter_set,
	.mac_addr_add            = virtio_mac_addr_add,
	.mac_addr_remove         = virtio_mac_addr_remove,
	.mac_addr_set            = virtio_mac_addr_set,
};

static inline int
virtio_dev_atomic_read_link_status(struct rte_eth_dev *dev,
				struct rte_eth_link *link)
{
	struct rte_eth_link *dst = link;
	struct rte_eth_link *src = &(dev->data->dev_link);

	if (rte_atomic64_cmpset((uint64_t *)dst, *(uint64_t *)dst,
			*(uint64_t *)src) == 0)
		return -1;

	return 0;
}

/**
 * Atomically writes the link status information into global
 * structure rte_eth_dev.
 *
 * @param dev
 *   - Pointer to the structure rte_eth_dev to read from.
 *   - Pointer to the buffer to be saved with the link status.
 *
 * @return
 *   - On success, zero.
 *   - On failure, negative value.
 */
static inline int
virtio_dev_atomic_write_link_status(struct rte_eth_dev *dev,
		struct rte_eth_link *link)
{
	struct rte_eth_link *dst = &(dev->data->dev_link);
	struct rte_eth_link *src = link;

	if (rte_atomic64_cmpset((uint64_t *)dst, *(uint64_t *)dst,
					*(uint64_t *)src) == 0)
		return -1;

	return 0;
}

static void
virtio_dev_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	unsigned i;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		const struct virtqueue *txvq = dev->data->tx_queues[i];
		if (txvq == NULL)
			continue;

		stats->opackets += txvq->packets;
		stats->obytes += txvq->bytes;
		stats->oerrors += txvq->errors;

		if (i < RTE_ETHDEV_QUEUE_STAT_CNTRS) {
			stats->q_opackets[i] = txvq->packets;
			stats->q_obytes[i] = txvq->bytes;
		}
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		const struct virtqueue *rxvq = dev->data->rx_queues[i];
		if (rxvq == NULL)
			continue;

		stats->ipackets += rxvq->packets;
		stats->ibytes += rxvq->bytes;
		stats->ierrors += rxvq->errors;

		if (i < RTE_ETHDEV_QUEUE_STAT_CNTRS) {
			stats->q_ipackets[i] = rxvq->packets;
			stats->q_ibytes[i] = rxvq->bytes;
		}
	}

	stats->rx_nombuf = dev->data->rx_mbuf_alloc_failed;
}

static void
virtio_dev_stats_reset(struct rte_eth_dev *dev)
{
	unsigned int i;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		struct virtqueue *txvq = dev->data->tx_queues[i];
		if (txvq == NULL)
			continue;

		txvq->packets = 0;
		txvq->bytes = 0;
		txvq->errors = 0;
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		struct virtqueue *rxvq = dev->data->rx_queues[i];
		if (rxvq == NULL)
			continue;

		rxvq->packets = 0;
		rxvq->bytes = 0;
		rxvq->errors = 0;
	}

	dev->data->rx_mbuf_alloc_failed = 0;
}

static void
virtio_set_hwaddr(struct virtio_hw *hw)
{
	vtpci_write_dev_config(hw,
			offsetof(struct virtio_net_config, mac),
			&hw->mac_addr, ETHER_ADDR_LEN);
}

static void
virtio_get_hwaddr(struct virtio_hw *hw)
{
	if (vtpci_with_feature(hw, VIRTIO_NET_F_MAC)) {
		vtpci_read_dev_config(hw,
			offsetof(struct virtio_net_config, mac),
			&hw->mac_addr, ETHER_ADDR_LEN);
	} else {
		eth_random_addr(&hw->mac_addr[0]);
		virtio_set_hwaddr(hw);
	}
}

static void
virtio_mac_table_set(struct virtio_hw *hw,
		     const struct virtio_net_ctrl_mac *uc,
		     const struct virtio_net_ctrl_mac *mc)
{
	struct virtio_pmd_ctrl ctrl;
	int err, len[2];

	if (!vtpci_with_feature(hw, VIRTIO_NET_F_CTRL_MAC_ADDR)) {
		PMD_DRV_LOG(INFO, "host does not support mac table\n");
		return;
	}

	ctrl.hdr.class = VIRTIO_NET_CTRL_MAC;
	ctrl.hdr.cmd = VIRTIO_NET_CTRL_MAC_TABLE_SET;

	len[0] = uc->entries * ETHER_ADDR_LEN + sizeof(uc->entries);
	memcpy(ctrl.data, uc, len[0]);

	len[1] = mc->entries * ETHER_ADDR_LEN + sizeof(mc->entries);
	memcpy(ctrl.data + len[0], mc, len[1]);

	err = virtio_send_command(hw->cvq, &ctrl, len, 2);
	if (err != 0)
		PMD_DRV_LOG(NOTICE, "mac table set failed: %d", err);
}

static void
virtio_mac_addr_add(struct rte_eth_dev *dev, struct ether_addr *mac_addr,
		    uint32_t index, uint32_t vmdq __rte_unused)
{
	struct virtio_hw *hw = dev->data->dev_private;
	const struct ether_addr *addrs = dev->data->mac_addrs;
	unsigned int i;
	struct virtio_net_ctrl_mac *uc, *mc;

	if (index >= VIRTIO_MAX_MAC_ADDRS) {
		PMD_DRV_LOG(ERR, "mac address index %u out of range", index);
		return;
	}

	uc = alloca(VIRTIO_MAX_MAC_ADDRS * ETHER_ADDR_LEN + sizeof(uc->entries));
	uc->entries = 0;
	mc = alloca(VIRTIO_MAX_MAC_ADDRS * ETHER_ADDR_LEN + sizeof(mc->entries));
	mc->entries = 0;

	for (i = 0; i < VIRTIO_MAX_MAC_ADDRS; i++) {
		const struct ether_addr *addr
			= (i == index) ? mac_addr : addrs + i;
		struct virtio_net_ctrl_mac *tbl
			= is_multicast_ether_addr(addr) ? mc : uc;

		memcpy(&tbl->macs[tbl->entries++], addr, ETHER_ADDR_LEN);
	}

	virtio_mac_table_set(hw, uc, mc);
}

static void
virtio_mac_addr_remove(struct rte_eth_dev *dev, uint32_t index)
{
	struct virtio_hw *hw = dev->data->dev_private;
	struct ether_addr *addrs = dev->data->mac_addrs;
	struct virtio_net_ctrl_mac *uc, *mc;
	unsigned int i;

	if (index >= VIRTIO_MAX_MAC_ADDRS) {
		PMD_DRV_LOG(ERR, "mac address index %u out of range", index);
		return;
	}

	uc = alloca(VIRTIO_MAX_MAC_ADDRS * ETHER_ADDR_LEN + sizeof(uc->entries));
	uc->entries = 0;
	mc = alloca(VIRTIO_MAX_MAC_ADDRS * ETHER_ADDR_LEN + sizeof(mc->entries));
	mc->entries = 0;

	for (i = 0; i < VIRTIO_MAX_MAC_ADDRS; i++) {
		struct virtio_net_ctrl_mac *tbl;

		if (i == index || is_zero_ether_addr(addrs + i))
			continue;

		tbl = is_multicast_ether_addr(addrs + i) ? mc : uc;
		memcpy(&tbl->macs[tbl->entries++], addrs + i, ETHER_ADDR_LEN);
	}

	virtio_mac_table_set(hw, uc, mc);
}

static void
virtio_mac_addr_set(struct rte_eth_dev *dev, struct ether_addr *mac_addr)
{
	struct virtio_hw *hw = dev->data->dev_private;

	memcpy(hw->mac_addr, mac_addr, ETHER_ADDR_LEN);

	/* Use atomic update if available */
	if (vtpci_with_feature(hw, VIRTIO_NET_F_CTRL_MAC_ADDR)) {
		struct virtio_pmd_ctrl ctrl;
		int len = ETHER_ADDR_LEN;

		ctrl.hdr.class = VIRTIO_NET_CTRL_MAC;
		ctrl.hdr.cmd = VIRTIO_NET_CTRL_MAC_ADDR_SET;

		memcpy(ctrl.data, mac_addr, ETHER_ADDR_LEN);
		virtio_send_command(hw->cvq, &ctrl, &len, 1);
	} else if (vtpci_with_feature(hw, VIRTIO_NET_F_MAC))
		virtio_set_hwaddr(hw);
}

static int
virtio_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vlan_id, int on)
{
	struct virtio_hw *hw = dev->data->dev_private;
	struct virtio_pmd_ctrl ctrl;
	int len;

	if (!vtpci_with_feature(hw, VIRTIO_NET_F_CTRL_VLAN))
		return -ENOTSUP;

	ctrl.hdr.class = VIRTIO_NET_CTRL_VLAN;
	ctrl.hdr.cmd = on ? VIRTIO_NET_CTRL_VLAN_ADD : VIRTIO_NET_CTRL_VLAN_DEL;
	memcpy(ctrl.data, &vlan_id, sizeof(vlan_id));
	len = sizeof(vlan_id);

	return virtio_send_command(hw->cvq, &ctrl, &len, 1);
}

static void
virtio_negotiate_features(struct virtio_hw *hw)
{
	uint32_t host_features;

	/* Prepare guest_features: feature that driver wants to support */
	hw->guest_features = VIRTIO_PMD_GUEST_FEATURES;
	PMD_INIT_LOG(DEBUG, "guest_features before negotiate = %x",
		hw->guest_features);

	/* Read device(host) feature bits */
	host_features = VIRTIO_READ_REG_4(hw, VIRTIO_PCI_HOST_FEATURES);
	PMD_INIT_LOG(DEBUG, "host_features before negotiate = %x",
		host_features);

	/*
	 * Negotiate features: Subset of device feature bits are written back
	 * guest feature bits.
	 */
	hw->guest_features = vtpci_negotiate_features(hw, host_features);
	PMD_INIT_LOG(DEBUG, "features after negotiate = %x",
		hw->guest_features);
}

#ifdef RTE_EXEC_ENV_LINUXAPP
static int
parse_sysfs_value(const char *filename, unsigned long *val)
{
	FILE *f;
	char buf[BUFSIZ];
	char *end = NULL;

	f = fopen(filename, "r");
	if (f == NULL) {
		PMD_INIT_LOG(ERR, "%s(): cannot open sysfs value %s",
			     __func__, filename);
		return -1;
	}

	if (fgets(buf, sizeof(buf), f) == NULL) {
		PMD_INIT_LOG(ERR, "%s(): cannot read sysfs value %s",
			     __func__, filename);
		fclose(f);
		return -1;
	}
	*val = strtoul(buf, &end, 0);
	if ((buf[0] == '\0') || (end == NULL) || (*end != '\n')) {
		PMD_INIT_LOG(ERR, "%s(): cannot parse sysfs value %s",
			     __func__, filename);
		fclose(f);
		return -1;
	}
	fclose(f);
	return 0;
}

static int get_uio_dev(struct rte_pci_addr *loc, char *buf, unsigned int buflen,
			unsigned int *uio_num)
{
	struct dirent *e;
	DIR *dir;
	char dirname[PATH_MAX];

	/* depending on kernel version, uio can be located in uio/uioX
	 * or uio:uioX */
	snprintf(dirname, sizeof(dirname),
		     SYSFS_PCI_DEVICES "/" PCI_PRI_FMT "/uio",
		     loc->domain, loc->bus, loc->devid, loc->function);
	dir = opendir(dirname);
	if (dir == NULL) {
		/* retry with the parent directory */
		snprintf(dirname, sizeof(dirname),
			     SYSFS_PCI_DEVICES "/" PCI_PRI_FMT,
			     loc->domain, loc->bus, loc->devid, loc->function);
		dir = opendir(dirname);

		if (dir == NULL) {
			PMD_INIT_LOG(ERR, "Cannot opendir %s", dirname);
			return -1;
		}
	}

	/* take the first file starting with "uio" */
	while ((e = readdir(dir)) != NULL) {
		/* format could be uio%d ...*/
		int shortprefix_len = sizeof("uio") - 1;
		/* ... or uio:uio%d */
		int longprefix_len = sizeof("uio:uio") - 1;
		char *endptr;

		if (strncmp(e->d_name, "uio", 3) != 0)
			continue;

		/* first try uio%d */
		errno = 0;
		*uio_num = strtoull(e->d_name + shortprefix_len, &endptr, 10);
		if (errno == 0 && endptr != (e->d_name + shortprefix_len)) {
			snprintf(buf, buflen, "%s/uio%u", dirname, *uio_num);
			break;
		}

		/* then try uio:uio%d */
		errno = 0;
		*uio_num = strtoull(e->d_name + longprefix_len, &endptr, 10);
		if (errno == 0 && endptr != (e->d_name + longprefix_len)) {
			snprintf(buf, buflen, "%s/uio:uio%u", dirname,
				     *uio_num);
			break;
		}
	}
	closedir(dir);

	/* No uio resource found */
	if (e == NULL) {
		PMD_INIT_LOG(ERR, "Could not find uio resource");
		return -1;
	}

	return 0;
}

static int
virtio_has_msix(const struct rte_pci_addr *loc)
{
	DIR *d;
	char dirname[PATH_MAX];

	snprintf(dirname, sizeof(dirname),
		     SYSFS_PCI_DEVICES "/" PCI_PRI_FMT "/msi_irqs",
		     loc->domain, loc->bus, loc->devid, loc->function);

	d = opendir(dirname);
	if (d)
		closedir(d);

	return (d != NULL);
}

/* Extract I/O port numbers from sysfs */
static int virtio_resource_init_by_uio(struct rte_pci_device *pci_dev)
{
	char dirname[PATH_MAX];
	char filename[PATH_MAX];
	unsigned long start, size;
	unsigned int uio_num;

	if (get_uio_dev(&pci_dev->addr, dirname, sizeof(dirname), &uio_num) < 0)
		return -1;

	/* get portio size */
	snprintf(filename, sizeof(filename),
		     "%s/portio/port0/size", dirname);
	if (parse_sysfs_value(filename, &size) < 0) {
		PMD_INIT_LOG(ERR, "%s(): cannot parse size",
			     __func__);
		return -1;
	}

	/* get portio start */
	snprintf(filename, sizeof(filename),
		 "%s/portio/port0/start", dirname);
	if (parse_sysfs_value(filename, &start) < 0) {
		PMD_INIT_LOG(ERR, "%s(): cannot parse portio start",
			     __func__);
		return -1;
	}
	pci_dev->mem_resource[0].addr = (void *)(uintptr_t)start;
	pci_dev->mem_resource[0].len =  (uint64_t)size;
	PMD_INIT_LOG(DEBUG,
		     "PCI Port IO found start=0x%lx with size=0x%lx",
		     start, size);

	/* save fd */
	memset(dirname, 0, sizeof(dirname));
	snprintf(dirname, sizeof(dirname), "/dev/uio%u", uio_num);
	pci_dev->intr_handle.fd = open(dirname, O_RDWR);
	if (pci_dev->intr_handle.fd < 0) {
		PMD_INIT_LOG(ERR, "Cannot open %s: %s\n",
			dirname, strerror(errno));
		return -1;
	}

	pci_dev->intr_handle.type = RTE_INTR_HANDLE_UIO;
	pci_dev->driver->drv_flags |= RTE_PCI_DRV_INTR_LSC;

	return 0;
}

/* Extract port I/O numbers from proc/ioports */
static int virtio_resource_init_by_ioports(struct rte_pci_device *pci_dev)
{
	uint16_t start, end;
	int size;
	FILE *fp;
	char *line = NULL;
	char pci_id[16];
	int found = 0;
	size_t linesz;

	snprintf(pci_id, sizeof(pci_id), PCI_PRI_FMT,
		 pci_dev->addr.domain,
		 pci_dev->addr.bus,
		 pci_dev->addr.devid,
		 pci_dev->addr.function);

	fp = fopen("/proc/ioports", "r");
	if (fp == NULL) {
		PMD_INIT_LOG(ERR, "%s(): can't open ioports", __func__);
		return -1;
	}

	while (getdelim(&line, &linesz, '\n', fp) > 0) {
		char *ptr = line;
		char *left;
		int n;

		n = strcspn(ptr, ":");
		ptr[n] = 0;
		left = &ptr[n+1];

		while (*left && isspace(*left))
			left++;

		if (!strncmp(left, pci_id, strlen(pci_id))) {
			found = 1;

			while (*ptr && isspace(*ptr))
				ptr++;

			sscanf(ptr, "%04hx-%04hx", &start, &end);
			size = end - start + 1;

			break;
		}
	}

	free(line);
	fclose(fp);

	if (!found)
		return -1;

	pci_dev->mem_resource[0].addr = (void *)(uintptr_t)(uint32_t)start;
	pci_dev->mem_resource[0].len =  (uint64_t)size;
	PMD_INIT_LOG(DEBUG,
		"PCI Port IO found start=0x%x with size=0x%x",
		start, size);

	/* can't support lsc interrupt without uio */
	pci_dev->driver->drv_flags &= ~RTE_PCI_DRV_INTR_LSC;

	return 0;
}

/* Extract I/O port numbers from sysfs */
static int virtio_resource_init(struct rte_pci_device *pci_dev)
{
	if (virtio_resource_init_by_uio(pci_dev) == 0)
		return 0;
	else
		return virtio_resource_init_by_ioports(pci_dev);
}

#else
static int
virtio_has_msix(const struct rte_pci_addr *loc __rte_unused)
{
	/* nic_uio does not enable interrupts, return 0 (false). */
	return 0;
}

static int virtio_resource_init(struct rte_pci_device *pci_dev __rte_unused)
{
	/* no setup required */
	return 0;
}
#endif

/*
 * Process Virtio Config changed interrupt and call the callback
 * if link state changed.
 */
static void
virtio_interrupt_handler(__rte_unused struct rte_intr_handle *handle,
			 void *param)
{
	struct rte_eth_dev *dev = param;
	struct virtio_hw *hw = dev->data->dev_private;
	uint8_t isr;

	/* Read interrupt status which clears interrupt */
	isr = vtpci_isr(hw);
	PMD_DRV_LOG(INFO, "interrupt status = %#x", isr);

	if (rte_intr_enable(&dev->pci_dev->intr_handle) < 0)
		PMD_DRV_LOG(ERR, "interrupt enable failed");

	if (isr & VIRTIO_PCI_ISR_CONFIG) {
		if (virtio_dev_link_update(dev, 0) == 0)
			_rte_eth_dev_callback_process(dev,
						      RTE_ETH_EVENT_INTR_LSC);
	}

}

static void
rx_func_get(struct rte_eth_dev *eth_dev)
{
	struct virtio_hw *hw = eth_dev->data->dev_private;
	if (vtpci_with_feature(hw, VIRTIO_NET_F_MRG_RXBUF))
		eth_dev->rx_pkt_burst = &virtio_recv_mergeable_pkts;
	else
		eth_dev->rx_pkt_burst = &virtio_recv_pkts;
}

/*
 * This function is based on probe() function in virtio_pci.c
 * It returns 0 on success.
 */
static int
eth_virtio_dev_init(struct rte_eth_dev *eth_dev)
{
	struct virtio_hw *hw = eth_dev->data->dev_private;
	struct virtio_net_config *config;
	struct virtio_net_config local_config;
	uint32_t offset_conf = sizeof(config->mac);
	struct rte_pci_device *pci_dev;

	RTE_BUILD_BUG_ON(RTE_PKTMBUF_HEADROOM < sizeof(struct virtio_net_hdr));

	eth_dev->dev_ops = &virtio_eth_dev_ops;
	eth_dev->tx_pkt_burst = &virtio_xmit_pkts;

	if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		rx_func_get(eth_dev);
		return 0;
	}

	/* Allocate memory for storing MAC addresses */
	eth_dev->data->mac_addrs = rte_zmalloc("virtio", ETHER_ADDR_LEN, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR,
			"Failed to allocate %d bytes needed to store MAC addresses",
			ETHER_ADDR_LEN);
		return -ENOMEM;
	}

	pci_dev = eth_dev->pci_dev;
	if (virtio_resource_init(pci_dev) < 0)
		return -1;

	hw->use_msix = virtio_has_msix(&pci_dev->addr);
	hw->io_base = (uint32_t)(uintptr_t)pci_dev->mem_resource[0].addr;

	/* Reset the device although not necessary at startup */
	vtpci_reset(hw);

	/* Tell the host we've noticed this device. */
	vtpci_set_status(hw, VIRTIO_CONFIG_STATUS_ACK);

	/* Tell the host we've known how to drive the device. */
	vtpci_set_status(hw, VIRTIO_CONFIG_STATUS_DRIVER);
	virtio_negotiate_features(hw);

	rx_func_get(eth_dev);

	/* Setting up rx_header size for the device */
	if (vtpci_with_feature(hw, VIRTIO_NET_F_MRG_RXBUF))
		hw->vtnet_hdr_size = sizeof(struct virtio_net_hdr_mrg_rxbuf);
	else
		hw->vtnet_hdr_size = sizeof(struct virtio_net_hdr);

	/* Copy the permanent MAC address to: virtio_hw */
	virtio_get_hwaddr(hw);
	ether_addr_copy((struct ether_addr *) hw->mac_addr,
			&eth_dev->data->mac_addrs[0]);
	PMD_INIT_LOG(DEBUG,
		     "PORT MAC: %02X:%02X:%02X:%02X:%02X:%02X",
		     hw->mac_addr[0], hw->mac_addr[1], hw->mac_addr[2],
		     hw->mac_addr[3], hw->mac_addr[4], hw->mac_addr[5]);

	if (vtpci_with_feature(hw, VIRTIO_NET_F_CTRL_VQ)) {
		config = &local_config;

		if (vtpci_with_feature(hw, VIRTIO_NET_F_STATUS)) {
			offset_conf += sizeof(config->status);
		} else {
			PMD_INIT_LOG(DEBUG,
				     "VIRTIO_NET_F_STATUS is not supported");
			config->status = 0;
		}

		if (vtpci_with_feature(hw, VIRTIO_NET_F_MQ)) {
			offset_conf += sizeof(config->max_virtqueue_pairs);
		} else {
			PMD_INIT_LOG(DEBUG,
				     "VIRTIO_NET_F_MQ is not supported");
			config->max_virtqueue_pairs = 1;
		}

		vtpci_read_dev_config(hw, 0, (uint8_t *)config, offset_conf);

		hw->max_rx_queues =
			(VIRTIO_MAX_RX_QUEUES < config->max_virtqueue_pairs) ?
			VIRTIO_MAX_RX_QUEUES : config->max_virtqueue_pairs;
		hw->max_tx_queues =
			(VIRTIO_MAX_TX_QUEUES < config->max_virtqueue_pairs) ?
			VIRTIO_MAX_TX_QUEUES : config->max_virtqueue_pairs;

		virtio_dev_cq_queue_setup(eth_dev,
					config->max_virtqueue_pairs * 2,
					SOCKET_ID_ANY);

		PMD_INIT_LOG(DEBUG, "config->max_virtqueue_pairs=%d",
				config->max_virtqueue_pairs);
		PMD_INIT_LOG(DEBUG, "config->status=%d", config->status);
		PMD_INIT_LOG(DEBUG,
				"PORT MAC: %02X:%02X:%02X:%02X:%02X:%02X",
				config->mac[0], config->mac[1],
				config->mac[2], config->mac[3],
				config->mac[4], config->mac[5]);
	} else {
		hw->max_rx_queues = 1;
		hw->max_tx_queues = 1;
	}

	eth_dev->data->nb_rx_queues = hw->max_rx_queues;
	eth_dev->data->nb_tx_queues = hw->max_tx_queues;

	PMD_INIT_LOG(DEBUG, "hw->max_rx_queues=%d   hw->max_tx_queues=%d",
			hw->max_rx_queues, hw->max_tx_queues);
	PMD_INIT_LOG(DEBUG, "port %d vendorID=0x%x deviceID=0x%x",
			eth_dev->data->port_id, pci_dev->id.vendor_id,
			pci_dev->id.device_id);

	/* Setup interrupt callback  */
	if (pci_dev->driver->drv_flags & RTE_PCI_DRV_INTR_LSC)
		rte_intr_callback_register(&pci_dev->intr_handle,
				   virtio_interrupt_handler, eth_dev);

	virtio_dev_cq_start(eth_dev);

	return 0;
}

static int
eth_virtio_dev_uninit(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev;
	struct virtio_hw *hw = eth_dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();

	if (rte_eal_process_type() == RTE_PROC_SECONDARY)
		return -EPERM;

	if (hw->started == 1) {
		virtio_dev_stop(eth_dev);
		virtio_dev_close(eth_dev);
	}
	pci_dev = eth_dev->pci_dev;

	eth_dev->dev_ops = NULL;
	eth_dev->tx_pkt_burst = NULL;
	eth_dev->rx_pkt_burst = NULL;

	virtio_dev_queue_release(hw->cvq);

	rte_free(eth_dev->data->mac_addrs);
	eth_dev->data->mac_addrs = NULL;

	/* reset interrupt callback  */
	if (pci_dev->driver->drv_flags & RTE_PCI_DRV_INTR_LSC)
		rte_intr_callback_unregister(&pci_dev->intr_handle,
						virtio_interrupt_handler,
						eth_dev);

	PMD_INIT_LOG(DEBUG, "dev_uninit completed");

	return 0;
}

static struct eth_driver rte_virtio_pmd = {
	.pci_drv = {
		.name = "rte_virtio_pmd",
		.id_table = pci_id_virtio_map,
		.drv_flags = RTE_PCI_DRV_DETACHABLE,
	},
	.eth_dev_init = eth_virtio_dev_init,
	.eth_dev_uninit = eth_virtio_dev_uninit,
	.dev_private_size = sizeof(struct virtio_hw),
};

/*
 * Driver initialization routine.
 * Invoked once at EAL init time.
 * Register itself as the [Poll Mode] Driver of PCI virtio devices.
 * Returns 0 on success.
 */
static int
rte_virtio_pmd_init(const char *name __rte_unused,
		    const char *param __rte_unused)
{
	if (rte_eal_iopl_init() != 0) {
		PMD_INIT_LOG(ERR, "IOPL call failed - cannot use virtio PMD");
		return -1;
	}

	rte_eth_driver_register(&rte_virtio_pmd);
	return 0;
}

/*
 * Configure virtio device
 * It returns 0 on success.
 */
static int
virtio_dev_configure(struct rte_eth_dev *dev)
{
	const struct rte_eth_rxmode *rxmode = &dev->data->dev_conf.rxmode;
	struct virtio_hw *hw = dev->data->dev_private;
	struct rte_pci_device *pci_dev = dev->pci_dev;

	PMD_INIT_LOG(DEBUG, "configure");

	if (rxmode->hw_ip_checksum) {
		PMD_DRV_LOG(ERR, "HW IP checksum not supported");
		return (-EINVAL);
	}

	hw->vlan_strip = rxmode->hw_vlan_strip;

	if (rxmode->hw_vlan_filter
	    && !vtpci_with_feature(hw, VIRTIO_NET_F_CTRL_VLAN)) {
		PMD_DRV_LOG(NOTICE,
			    "vlan filtering not available on this host");
		return -ENOTSUP;
	}

	if (pci_dev->driver->drv_flags & RTE_PCI_DRV_INTR_LSC)
		if (vtpci_irq_config(hw, 0) == VIRTIO_MSI_NO_VECTOR) {
			PMD_DRV_LOG(ERR, "failed to set config vector");
			return -EBUSY;
		}

	return 0;
}


static int
virtio_dev_start(struct rte_eth_dev *dev)
{
	uint16_t nb_queues, i;
	struct virtio_hw *hw = dev->data->dev_private;
	struct rte_pci_device *pci_dev = dev->pci_dev;

	/* check if lsc interrupt feature is enabled */
	if ((dev->data->dev_conf.intr_conf.lsc) &&
		(pci_dev->driver->drv_flags & RTE_PCI_DRV_INTR_LSC)) {
		if (!vtpci_with_feature(hw, VIRTIO_NET_F_STATUS)) {
			PMD_DRV_LOG(ERR, "link status not supported by host");
			return -ENOTSUP;
		}

		if (rte_intr_enable(&dev->pci_dev->intr_handle) < 0) {
			PMD_DRV_LOG(ERR, "interrupt enable failed");
			return -EIO;
		}
	}

	/* Initialize Link state */
	virtio_dev_link_update(dev, 0);

	/* On restart after stop do not touch queues */
	if (hw->started)
		return 0;

	/* Do final configuration before rx/tx engine starts */
	virtio_dev_rxtx_start(dev);
	vtpci_reinit_complete(hw);

	hw->started = 1;

	/*Notify the backend
	 *Otherwise the tap backend might already stop its queue due to fullness.
	 *vhost backend will have no chance to be waked up
	 */
	nb_queues = dev->data->nb_rx_queues;
	if (nb_queues > 1) {
		if (virtio_set_multiple_queues(dev, nb_queues) != 0)
			return -EINVAL;
	}

	PMD_INIT_LOG(DEBUG, "nb_queues=%d", nb_queues);

	for (i = 0; i < nb_queues; i++)
		virtqueue_notify(dev->data->rx_queues[i]);

	PMD_INIT_LOG(DEBUG, "Notified backend at initialization");

	for (i = 0; i < dev->data->nb_rx_queues; i++)
		VIRTQUEUE_DUMP((struct virtqueue *)dev->data->rx_queues[i]);

	for (i = 0; i < dev->data->nb_tx_queues; i++)
		VIRTQUEUE_DUMP((struct virtqueue *)dev->data->tx_queues[i]);

	return 0;
}

static void virtio_dev_free_mbufs(struct rte_eth_dev *dev)
{
	struct rte_mbuf *buf;
	int i, mbuf_num = 0;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		PMD_INIT_LOG(DEBUG,
			     "Before freeing rxq[%d] used and unused buf", i);
		VIRTQUEUE_DUMP((struct virtqueue *)dev->data->rx_queues[i]);

		PMD_INIT_LOG(DEBUG, "rx_queues[%d]=%p",
				i, dev->data->rx_queues[i]);
		while ((buf = (struct rte_mbuf *)virtqueue_detatch_unused(
					dev->data->rx_queues[i])) != NULL) {
			rte_pktmbuf_free(buf);
			mbuf_num++;
		}

		PMD_INIT_LOG(DEBUG, "free %d mbufs", mbuf_num);
		PMD_INIT_LOG(DEBUG,
			     "After freeing rxq[%d] used and unused buf", i);
		VIRTQUEUE_DUMP((struct virtqueue *)dev->data->rx_queues[i]);
	}

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		PMD_INIT_LOG(DEBUG,
			     "Before freeing txq[%d] used and unused bufs",
			     i);
		VIRTQUEUE_DUMP((struct virtqueue *)dev->data->tx_queues[i]);

		mbuf_num = 0;
		while ((buf = (struct rte_mbuf *)virtqueue_detatch_unused(
					dev->data->tx_queues[i])) != NULL) {
			rte_pktmbuf_free(buf);

			mbuf_num++;
		}

		PMD_INIT_LOG(DEBUG, "free %d mbufs", mbuf_num);
		PMD_INIT_LOG(DEBUG,
			     "After freeing txq[%d] used and unused buf", i);
		VIRTQUEUE_DUMP((struct virtqueue *)dev->data->tx_queues[i]);
	}
}

/*
 * Stop device: disable interrupt and mark link down
 */
static void
virtio_dev_stop(struct rte_eth_dev *dev)
{
	struct rte_eth_link link;

	PMD_INIT_LOG(DEBUG, "stop");

	if (dev->data->dev_conf.intr_conf.lsc)
		rte_intr_disable(&dev->pci_dev->intr_handle);

	memset(&link, 0, sizeof(link));
	virtio_dev_atomic_write_link_status(dev, &link);
}

static int
virtio_dev_link_update(struct rte_eth_dev *dev, __rte_unused int wait_to_complete)
{
	struct rte_eth_link link, old;
	uint16_t status;
	struct virtio_hw *hw = dev->data->dev_private;
	memset(&link, 0, sizeof(link));
	virtio_dev_atomic_read_link_status(dev, &link);
	old = link;
	link.link_duplex = FULL_DUPLEX;
	link.link_speed  = SPEED_10G;

	if (vtpci_with_feature(hw, VIRTIO_NET_F_STATUS)) {
		PMD_INIT_LOG(DEBUG, "Get link status from hw");
		vtpci_read_dev_config(hw,
				offsetof(struct virtio_net_config, status),
				&status, sizeof(status));
		if ((status & VIRTIO_NET_S_LINK_UP) == 0) {
			link.link_status = 0;
			PMD_INIT_LOG(DEBUG, "Port %d is down",
				     dev->data->port_id);
		} else {
			link.link_status = 1;
			PMD_INIT_LOG(DEBUG, "Port %d is up",
				     dev->data->port_id);
		}
	} else {
		link.link_status = 1;   /* Link up */
	}
	virtio_dev_atomic_write_link_status(dev, &link);

	return (old.link_status == link.link_status) ? -1 : 0;
}

static void
virtio_dev_info_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	struct virtio_hw *hw = dev->data->dev_private;

	dev_info->driver_name = dev->driver->pci_drv.name;
	dev_info->max_rx_queues = (uint16_t)hw->max_rx_queues;
	dev_info->max_tx_queues = (uint16_t)hw->max_tx_queues;
	dev_info->min_rx_bufsize = VIRTIO_MIN_RX_BUFSIZE;
	dev_info->max_rx_pktlen = VIRTIO_MAX_RX_PKTLEN;
	dev_info->max_mac_addrs = VIRTIO_MAX_MAC_ADDRS;
	dev_info->default_txconf = (struct rte_eth_txconf) {
		.txq_flags = ETH_TXQ_FLAGS_NOOFFLOADS
	};
}

/*
 * It enables testpmd to collect per queue stats.
 */
static int
virtio_dev_queue_stats_mapping_set(__rte_unused struct rte_eth_dev *eth_dev,
__rte_unused uint16_t queue_id, __rte_unused uint8_t stat_idx,
__rte_unused uint8_t is_rx)
{
	return 0;
}

static struct rte_driver rte_virtio_driver = {
	.type = PMD_PDEV,
	.init = rte_virtio_pmd_init,
};

PMD_REGISTER_DRIVER(rte_virtio_driver);
