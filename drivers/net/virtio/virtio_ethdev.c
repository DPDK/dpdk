/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2016 Intel Corporation. All rights reserved.
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

#include <rte_ethdev.h>
#include <rte_ethdev_pci.h>
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
#include "virtio_rxtx.h"

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
	int wait_to_complete);

static void virtio_set_hwaddr(struct virtio_hw *hw);
static void virtio_get_hwaddr(struct virtio_hw *hw);

static void virtio_dev_stats_get(struct rte_eth_dev *dev,
				 struct rte_eth_stats *stats);
static int virtio_dev_xstats_get(struct rte_eth_dev *dev,
				 struct rte_eth_xstat *xstats, unsigned n);
static int virtio_dev_xstats_get_names(struct rte_eth_dev *dev,
				       struct rte_eth_xstat_name *xstats_names,
				       unsigned limit);
static void virtio_dev_stats_reset(struct rte_eth_dev *dev);
static void virtio_dev_free_mbufs(struct rte_eth_dev *dev);
static int virtio_vlan_filter_set(struct rte_eth_dev *dev,
				uint16_t vlan_id, int on);
static int virtio_mac_addr_add(struct rte_eth_dev *dev,
				struct ether_addr *mac_addr,
				uint32_t index, uint32_t vmdq);
static void virtio_mac_addr_remove(struct rte_eth_dev *dev, uint32_t index);
static void virtio_mac_addr_set(struct rte_eth_dev *dev,
				struct ether_addr *mac_addr);

static int virtio_dev_queue_stats_mapping_set(
	struct rte_eth_dev *eth_dev,
	uint16_t queue_id,
	uint8_t stat_idx,
	uint8_t is_rx);

/*
 * The set of PCI devices this driver supports
 */
static const struct rte_pci_id pci_id_virtio_map[] = {
	{ RTE_PCI_DEVICE(VIRTIO_PCI_VENDORID, VIRTIO_PCI_LEGACY_DEVICEID_NET) },
	{ RTE_PCI_DEVICE(VIRTIO_PCI_VENDORID, VIRTIO_PCI_MODERN_DEVICEID_NET) },
	{ .vendor_id = 0, /* sentinel */ },
};

struct rte_virtio_xstats_name_off {
	char name[RTE_ETH_XSTATS_NAME_SIZE];
	unsigned offset;
};

/* [rt]x_qX_ is prepended to the name string here */
static const struct rte_virtio_xstats_name_off rte_virtio_rxq_stat_strings[] = {
	{"good_packets",           offsetof(struct virtnet_rx, stats.packets)},
	{"good_bytes",             offsetof(struct virtnet_rx, stats.bytes)},
	{"errors",                 offsetof(struct virtnet_rx, stats.errors)},
	{"multicast_packets",      offsetof(struct virtnet_rx, stats.multicast)},
	{"broadcast_packets",      offsetof(struct virtnet_rx, stats.broadcast)},
	{"undersize_packets",      offsetof(struct virtnet_rx, stats.size_bins[0])},
	{"size_64_packets",        offsetof(struct virtnet_rx, stats.size_bins[1])},
	{"size_65_127_packets",    offsetof(struct virtnet_rx, stats.size_bins[2])},
	{"size_128_255_packets",   offsetof(struct virtnet_rx, stats.size_bins[3])},
	{"size_256_511_packets",   offsetof(struct virtnet_rx, stats.size_bins[4])},
	{"size_512_1023_packets",  offsetof(struct virtnet_rx, stats.size_bins[5])},
	{"size_1024_1518_packets", offsetof(struct virtnet_rx, stats.size_bins[6])},
	{"size_1519_max_packets",  offsetof(struct virtnet_rx, stats.size_bins[7])},
};

/* [rt]x_qX_ is prepended to the name string here */
static const struct rte_virtio_xstats_name_off rte_virtio_txq_stat_strings[] = {
	{"good_packets",           offsetof(struct virtnet_tx, stats.packets)},
	{"good_bytes",             offsetof(struct virtnet_tx, stats.bytes)},
	{"errors",                 offsetof(struct virtnet_tx, stats.errors)},
	{"multicast_packets",      offsetof(struct virtnet_tx, stats.multicast)},
	{"broadcast_packets",      offsetof(struct virtnet_tx, stats.broadcast)},
	{"undersize_packets",      offsetof(struct virtnet_tx, stats.size_bins[0])},
	{"size_64_packets",        offsetof(struct virtnet_tx, stats.size_bins[1])},
	{"size_65_127_packets",    offsetof(struct virtnet_tx, stats.size_bins[2])},
	{"size_128_255_packets",   offsetof(struct virtnet_tx, stats.size_bins[3])},
	{"size_256_511_packets",   offsetof(struct virtnet_tx, stats.size_bins[4])},
	{"size_512_1023_packets",  offsetof(struct virtnet_tx, stats.size_bins[5])},
	{"size_1024_1518_packets", offsetof(struct virtnet_tx, stats.size_bins[6])},
	{"size_1519_max_packets",  offsetof(struct virtnet_tx, stats.size_bins[7])},
};

#define VIRTIO_NB_RXQ_XSTATS (sizeof(rte_virtio_rxq_stat_strings) / \
			    sizeof(rte_virtio_rxq_stat_strings[0]))
#define VIRTIO_NB_TXQ_XSTATS (sizeof(rte_virtio_txq_stat_strings) / \
			    sizeof(rte_virtio_txq_stat_strings[0]))

struct virtio_hw_internal virtio_hw_internal[RTE_MAX_ETHPORTS];

static int
virtio_send_command(struct virtnet_ctl *cvq, struct virtio_pmd_ctrl *ctrl,
		int *dlen, int pkt_num)
{
	uint32_t head, i;
	int k, sum = 0;
	virtio_net_ctrl_ack status = ~0;
	struct virtio_pmd_ctrl result;
	struct virtqueue *vq;

	ctrl->status = status;

	if (!cvq || !cvq->vq) {
		PMD_INIT_LOG(ERR, "Control queue is not supported.");
		return -1;
	}
	vq = cvq->vq;
	head = vq->vq_desc_head_idx;

	PMD_INIT_LOG(DEBUG, "vq->vq_desc_head_idx = %d, status = %d, "
		"vq->hw->cvq = %p vq = %p",
		vq->vq_desc_head_idx, status, vq->hw->cvq, vq);

	if ((vq->vq_free_cnt < ((uint32_t)pkt_num + 2)) || (pkt_num < 1))
		return -1;

	memcpy(cvq->virtio_net_hdr_mz->addr, ctrl,
		sizeof(struct virtio_pmd_ctrl));

	/*
	 * Format is enforced in qemu code:
	 * One TX packet for header;
	 * At least one TX packet per argument;
	 * One RX packet for ACK.
	 */
	vq->vq_ring.desc[head].flags = VRING_DESC_F_NEXT;
	vq->vq_ring.desc[head].addr = cvq->virtio_net_hdr_mem;
	vq->vq_ring.desc[head].len = sizeof(struct virtio_net_ctrl_hdr);
	vq->vq_free_cnt--;
	i = vq->vq_ring.desc[head].next;

	for (k = 0; k < pkt_num; k++) {
		vq->vq_ring.desc[i].flags = VRING_DESC_F_NEXT;
		vq->vq_ring.desc[i].addr = cvq->virtio_net_hdr_mem
			+ sizeof(struct virtio_net_ctrl_hdr)
			+ sizeof(ctrl->status) + sizeof(uint8_t)*sum;
		vq->vq_ring.desc[i].len = dlen[k];
		sum += dlen[k];
		vq->vq_free_cnt--;
		i = vq->vq_ring.desc[i].next;
	}

	vq->vq_ring.desc[i].flags = VRING_DESC_F_WRITE;
	vq->vq_ring.desc[i].addr = cvq->virtio_net_hdr_mem
			+ sizeof(struct virtio_net_ctrl_hdr);
	vq->vq_ring.desc[i].len = sizeof(ctrl->status);
	vq->vq_free_cnt--;

	vq->vq_desc_head_idx = vq->vq_ring.desc[i].next;

	vq_update_avail_ring(vq, head);
	vq_update_avail_idx(vq);

	PMD_INIT_LOG(DEBUG, "vq->vq_queue_index = %d", vq->vq_queue_index);

	virtqueue_notify(vq);

	rte_rmb();
	while (VIRTQUEUE_NUSED(vq) == 0) {
		rte_rmb();
		usleep(100);
	}

	while (VIRTQUEUE_NUSED(vq)) {
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

	memcpy(&result, cvq->virtio_net_hdr_mz->addr,
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

static void
virtio_dev_queue_release(void *queue __rte_unused)
{
	/* do nothing */
}

static int
virtio_get_queue_type(struct virtio_hw *hw, uint16_t vtpci_queue_idx)
{
	if (vtpci_queue_idx == hw->max_queue_pairs * 2)
		return VTNET_CQ;
	else if (vtpci_queue_idx % 2 == 0)
		return VTNET_RQ;
	else
		return VTNET_TQ;
}

static uint16_t
virtio_get_nr_vq(struct virtio_hw *hw)
{
	uint16_t nr_vq = hw->max_queue_pairs * 2;

	if (vtpci_with_feature(hw, VIRTIO_NET_F_CTRL_VQ))
		nr_vq += 1;

	return nr_vq;
}

static void
virtio_init_vring(struct virtqueue *vq)
{
	int size = vq->vq_nentries;
	struct vring *vr = &vq->vq_ring;
	uint8_t *ring_mem = vq->vq_ring_virt_mem;

	PMD_INIT_FUNC_TRACE();

	/*
	 * Reinitialise since virtio port might have been stopped and restarted
	 */
	memset(ring_mem, 0, vq->vq_ring_size);
	vring_init(vr, size, ring_mem, VIRTIO_PCI_VRING_ALIGN);
	vq->vq_used_cons_idx = 0;
	vq->vq_desc_head_idx = 0;
	vq->vq_avail_idx = 0;
	vq->vq_desc_tail_idx = (uint16_t)(vq->vq_nentries - 1);
	vq->vq_free_cnt = vq->vq_nentries;
	memset(vq->vq_descx, 0, sizeof(struct vq_desc_extra) * vq->vq_nentries);

	vring_desc_init(vr->desc, size);

	/*
	 * Disable device(host) interrupting guest
	 */
	virtqueue_disable_intr(vq);
}

static int
virtio_init_queue(struct rte_eth_dev *dev, uint16_t vtpci_queue_idx)
{
	char vq_name[VIRTQUEUE_MAX_NAME_SZ];
	char vq_hdr_name[VIRTQUEUE_MAX_NAME_SZ];
	const struct rte_memzone *mz = NULL, *hdr_mz = NULL;
	unsigned int vq_size, size;
	struct virtio_hw *hw = dev->data->dev_private;
	struct virtnet_rx *rxvq = NULL;
	struct virtnet_tx *txvq = NULL;
	struct virtnet_ctl *cvq = NULL;
	struct virtqueue *vq;
	size_t sz_hdr_mz = 0;
	void *sw_ring = NULL;
	int queue_type = virtio_get_queue_type(hw, vtpci_queue_idx);
	int ret;

	PMD_INIT_LOG(DEBUG, "setting up queue: %u", vtpci_queue_idx);

	/*
	 * Read the virtqueue size from the Queue Size field
	 * Always power of 2 and if 0 virtqueue does not exist
	 */
	vq_size = VTPCI_OPS(hw)->get_queue_num(hw, vtpci_queue_idx);
	PMD_INIT_LOG(DEBUG, "vq_size: %u", vq_size);
	if (vq_size == 0) {
		PMD_INIT_LOG(ERR, "virtqueue does not exist");
		return -EINVAL;
	}

	if (!rte_is_power_of_2(vq_size)) {
		PMD_INIT_LOG(ERR, "virtqueue size is not powerof 2");
		return -EINVAL;
	}

	snprintf(vq_name, sizeof(vq_name), "port%d_vq%d",
		 dev->data->port_id, vtpci_queue_idx);

	size = RTE_ALIGN_CEIL(sizeof(*vq) +
				vq_size * sizeof(struct vq_desc_extra),
				RTE_CACHE_LINE_SIZE);
	if (queue_type == VTNET_TQ) {
		/*
		 * For each xmit packet, allocate a virtio_net_hdr
		 * and indirect ring elements
		 */
		sz_hdr_mz = vq_size * sizeof(struct virtio_tx_region);
	} else if (queue_type == VTNET_CQ) {
		/* Allocate a page for control vq command, data and status */
		sz_hdr_mz = PAGE_SIZE;
	}

	vq = rte_zmalloc_socket(vq_name, size, RTE_CACHE_LINE_SIZE,
				SOCKET_ID_ANY);
	if (vq == NULL) {
		PMD_INIT_LOG(ERR, "can not allocate vq");
		return -ENOMEM;
	}
	hw->vqs[vtpci_queue_idx] = vq;

	vq->hw = hw;
	vq->vq_queue_index = vtpci_queue_idx;
	vq->vq_nentries = vq_size;

	/*
	 * Reserve a memzone for vring elements
	 */
	size = vring_size(vq_size, VIRTIO_PCI_VRING_ALIGN);
	vq->vq_ring_size = RTE_ALIGN_CEIL(size, VIRTIO_PCI_VRING_ALIGN);
	PMD_INIT_LOG(DEBUG, "vring_size: %d, rounded_vring_size: %d",
		     size, vq->vq_ring_size);

	mz = rte_memzone_reserve_aligned(vq_name, vq->vq_ring_size,
					 SOCKET_ID_ANY,
					 0, VIRTIO_PCI_VRING_ALIGN);
	if (mz == NULL) {
		if (rte_errno == EEXIST)
			mz = rte_memzone_lookup(vq_name);
		if (mz == NULL) {
			ret = -ENOMEM;
			goto fail_q_alloc;
		}
	}

	memset(mz->addr, 0, mz->len);

	vq->vq_ring_mem = mz->phys_addr;
	vq->vq_ring_virt_mem = mz->addr;
	PMD_INIT_LOG(DEBUG, "vq->vq_ring_mem:      0x%" PRIx64,
		     (uint64_t)mz->phys_addr);
	PMD_INIT_LOG(DEBUG, "vq->vq_ring_virt_mem: 0x%" PRIx64,
		     (uint64_t)(uintptr_t)mz->addr);

	virtio_init_vring(vq);

	if (sz_hdr_mz) {
		snprintf(vq_hdr_name, sizeof(vq_hdr_name), "port%d_vq%d_hdr",
			 dev->data->port_id, vtpci_queue_idx);
		hdr_mz = rte_memzone_reserve_aligned(vq_hdr_name, sz_hdr_mz,
						     SOCKET_ID_ANY, 0,
						     RTE_CACHE_LINE_SIZE);
		if (hdr_mz == NULL) {
			if (rte_errno == EEXIST)
				hdr_mz = rte_memzone_lookup(vq_hdr_name);
			if (hdr_mz == NULL) {
				ret = -ENOMEM;
				goto fail_q_alloc;
			}
		}
	}

	if (queue_type == VTNET_RQ) {
		size_t sz_sw = (RTE_PMD_VIRTIO_RX_MAX_BURST + vq_size) *
			       sizeof(vq->sw_ring[0]);

		sw_ring = rte_zmalloc_socket("sw_ring", sz_sw,
				RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
		if (!sw_ring) {
			PMD_INIT_LOG(ERR, "can not allocate RX soft ring");
			ret = -ENOMEM;
			goto fail_q_alloc;
		}

		vq->sw_ring = sw_ring;
		rxvq = &vq->rxq;
		rxvq->vq = vq;
		rxvq->port_id = dev->data->port_id;
		rxvq->mz = mz;
	} else if (queue_type == VTNET_TQ) {
		txvq = &vq->txq;
		txvq->vq = vq;
		txvq->port_id = dev->data->port_id;
		txvq->mz = mz;
		txvq->virtio_net_hdr_mz = hdr_mz;
		txvq->virtio_net_hdr_mem = hdr_mz->phys_addr;
	} else if (queue_type == VTNET_CQ) {
		cvq = &vq->cq;
		cvq->vq = vq;
		cvq->mz = mz;
		cvq->virtio_net_hdr_mz = hdr_mz;
		cvq->virtio_net_hdr_mem = hdr_mz->phys_addr;
		memset(cvq->virtio_net_hdr_mz->addr, 0, PAGE_SIZE);

		hw->cvq = cvq;
	}

	/* For virtio_user case (that is when hw->dev is NULL), we use
	 * virtual address. And we need properly set _offset_, please see
	 * VIRTIO_MBUF_DATA_DMA_ADDR in virtqueue.h for more information.
	 */
	if (!hw->virtio_user_dev)
		vq->offset = offsetof(struct rte_mbuf, buf_physaddr);
	else {
		vq->vq_ring_mem = (uintptr_t)mz->addr;
		vq->offset = offsetof(struct rte_mbuf, buf_addr);
		if (queue_type == VTNET_TQ)
			txvq->virtio_net_hdr_mem = (uintptr_t)hdr_mz->addr;
		else if (queue_type == VTNET_CQ)
			cvq->virtio_net_hdr_mem = (uintptr_t)hdr_mz->addr;
	}

	if (queue_type == VTNET_TQ) {
		struct virtio_tx_region *txr;
		unsigned int i;

		txr = hdr_mz->addr;
		memset(txr, 0, vq_size * sizeof(*txr));
		for (i = 0; i < vq_size; i++) {
			struct vring_desc *start_dp = txr[i].tx_indir;

			vring_desc_init(start_dp, RTE_DIM(txr[i].tx_indir));

			/* first indirect descriptor is always the tx header */
			start_dp->addr = txvq->virtio_net_hdr_mem
				+ i * sizeof(*txr)
				+ offsetof(struct virtio_tx_region, tx_hdr);

			start_dp->len = hw->vtnet_hdr_size;
			start_dp->flags = VRING_DESC_F_NEXT;
		}
	}

	if (VTPCI_OPS(hw)->setup_queue(hw, vq) < 0) {
		PMD_INIT_LOG(ERR, "setup_queue failed");
		return -EINVAL;
	}

	return 0;

fail_q_alloc:
	rte_free(sw_ring);
	rte_memzone_free(hdr_mz);
	rte_memzone_free(mz);
	rte_free(vq);

	return ret;
}

static void
virtio_free_queues(struct virtio_hw *hw)
{
	uint16_t nr_vq = virtio_get_nr_vq(hw);
	struct virtqueue *vq;
	int queue_type;
	uint16_t i;

	if (hw->vqs == NULL)
		return;

	for (i = 0; i < nr_vq; i++) {
		vq = hw->vqs[i];
		if (!vq)
			continue;

		queue_type = virtio_get_queue_type(hw, i);
		if (queue_type == VTNET_RQ) {
			rte_free(vq->sw_ring);
			rte_memzone_free(vq->rxq.mz);
		} else if (queue_type == VTNET_TQ) {
			rte_memzone_free(vq->txq.mz);
			rte_memzone_free(vq->txq.virtio_net_hdr_mz);
		} else {
			rte_memzone_free(vq->cq.mz);
			rte_memzone_free(vq->cq.virtio_net_hdr_mz);
		}

		rte_free(vq);
		hw->vqs[i] = NULL;
	}

	rte_free(hw->vqs);
	hw->vqs = NULL;
}

static int
virtio_alloc_queues(struct rte_eth_dev *dev)
{
	struct virtio_hw *hw = dev->data->dev_private;
	uint16_t nr_vq = virtio_get_nr_vq(hw);
	uint16_t i;
	int ret;

	hw->vqs = rte_zmalloc(NULL, sizeof(struct virtqueue *) * nr_vq, 0);
	if (!hw->vqs) {
		PMD_INIT_LOG(ERR, "failed to allocate vqs");
		return -ENOMEM;
	}

	for (i = 0; i < nr_vq; i++) {
		ret = virtio_init_queue(dev, i);
		if (ret < 0) {
			virtio_free_queues(hw);
			return ret;
		}
	}

	return 0;
}

static void virtio_queues_unbind_intr(struct rte_eth_dev *dev);

static void
virtio_dev_close(struct rte_eth_dev *dev)
{
	struct virtio_hw *hw = dev->data->dev_private;
	struct rte_intr_conf *intr_conf = &dev->data->dev_conf.intr_conf;

	PMD_INIT_LOG(DEBUG, "virtio_dev_close");

	/* reset the NIC */
	if (dev->data->dev_flags & RTE_ETH_DEV_INTR_LSC)
		VTPCI_OPS(hw)->set_config_irq(hw, VIRTIO_MSI_NO_VECTOR);
	if (intr_conf->rxq)
		virtio_queues_unbind_intr(dev);

	if (intr_conf->lsc || intr_conf->rxq) {
		rte_intr_disable(dev->intr_handle);
		rte_intr_efd_disable(dev->intr_handle);
		rte_free(dev->intr_handle->intr_vec);
		dev->intr_handle->intr_vec = NULL;
	}

	vtpci_reset(hw);
	virtio_dev_free_mbufs(dev);
	virtio_free_queues(hw);
}

static void
virtio_dev_promiscuous_enable(struct rte_eth_dev *dev)
{
	struct virtio_hw *hw = dev->data->dev_private;
	struct virtio_pmd_ctrl ctrl;
	int dlen[1];
	int ret;

	if (!vtpci_with_feature(hw, VIRTIO_NET_F_CTRL_RX)) {
		PMD_INIT_LOG(INFO, "host does not support rx control");
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
		PMD_INIT_LOG(INFO, "host does not support rx control");
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
		PMD_INIT_LOG(INFO, "host does not support rx control");
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
		PMD_INIT_LOG(INFO, "host does not support rx control");
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

#define VLAN_TAG_LEN           4    /* 802.3ac tag (not DMA'd) */
static int
virtio_mtu_set(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct virtio_hw *hw = dev->data->dev_private;
	uint32_t ether_hdr_len = ETHER_HDR_LEN + VLAN_TAG_LEN +
				 hw->vtnet_hdr_size;
	uint32_t frame_size = mtu + ether_hdr_len;
	uint32_t max_frame_size = hw->max_mtu + ether_hdr_len;

	max_frame_size = RTE_MIN(max_frame_size, VIRTIO_MAX_RX_PKTLEN);

	if (mtu < ETHER_MIN_MTU || frame_size > max_frame_size) {
		PMD_INIT_LOG(ERR, "MTU should be between %d and %d",
			ETHER_MIN_MTU, max_frame_size - ether_hdr_len);
		return -EINVAL;
	}
	return 0;
}

static int
virtio_dev_rx_queue_intr_enable(struct rte_eth_dev *dev, uint16_t queue_id)
{
	struct virtnet_rx *rxvq = dev->data->rx_queues[queue_id];
	struct virtqueue *vq = rxvq->vq;

	virtqueue_enable_intr(vq);
	return 0;
}

static int
virtio_dev_rx_queue_intr_disable(struct rte_eth_dev *dev, uint16_t queue_id)
{
	struct virtnet_rx *rxvq = dev->data->rx_queues[queue_id];
	struct virtqueue *vq = rxvq->vq;

	virtqueue_disable_intr(vq);
	return 0;
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
	.mtu_set                 = virtio_mtu_set,
	.dev_infos_get           = virtio_dev_info_get,
	.stats_get               = virtio_dev_stats_get,
	.xstats_get              = virtio_dev_xstats_get,
	.xstats_get_names        = virtio_dev_xstats_get_names,
	.stats_reset             = virtio_dev_stats_reset,
	.xstats_reset            = virtio_dev_stats_reset,
	.link_update             = virtio_dev_link_update,
	.rx_queue_setup          = virtio_dev_rx_queue_setup,
	.rx_queue_intr_enable    = virtio_dev_rx_queue_intr_enable,
	.rx_queue_intr_disable   = virtio_dev_rx_queue_intr_disable,
	.rx_queue_release        = virtio_dev_queue_release,
	.rx_descriptor_done      = virtio_dev_rx_queue_done,
	.tx_queue_setup          = virtio_dev_tx_queue_setup,
	.tx_queue_release        = virtio_dev_queue_release,
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
virtio_update_stats(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	unsigned i;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		const struct virtnet_tx *txvq = dev->data->tx_queues[i];
		if (txvq == NULL)
			continue;

		stats->opackets += txvq->stats.packets;
		stats->obytes += txvq->stats.bytes;
		stats->oerrors += txvq->stats.errors;

		if (i < RTE_ETHDEV_QUEUE_STAT_CNTRS) {
			stats->q_opackets[i] = txvq->stats.packets;
			stats->q_obytes[i] = txvq->stats.bytes;
		}
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		const struct virtnet_rx *rxvq = dev->data->rx_queues[i];
		if (rxvq == NULL)
			continue;

		stats->ipackets += rxvq->stats.packets;
		stats->ibytes += rxvq->stats.bytes;
		stats->ierrors += rxvq->stats.errors;

		if (i < RTE_ETHDEV_QUEUE_STAT_CNTRS) {
			stats->q_ipackets[i] = rxvq->stats.packets;
			stats->q_ibytes[i] = rxvq->stats.bytes;
		}
	}

	stats->rx_nombuf = dev->data->rx_mbuf_alloc_failed;
}

static int virtio_dev_xstats_get_names(struct rte_eth_dev *dev,
				       struct rte_eth_xstat_name *xstats_names,
				       __rte_unused unsigned limit)
{
	unsigned i;
	unsigned count = 0;
	unsigned t;

	unsigned nstats = dev->data->nb_tx_queues * VIRTIO_NB_TXQ_XSTATS +
		dev->data->nb_rx_queues * VIRTIO_NB_RXQ_XSTATS;

	if (xstats_names != NULL) {
		/* Note: limit checked in rte_eth_xstats_names() */

		for (i = 0; i < dev->data->nb_rx_queues; i++) {
			struct virtqueue *rxvq = dev->data->rx_queues[i];
			if (rxvq == NULL)
				continue;
			for (t = 0; t < VIRTIO_NB_RXQ_XSTATS; t++) {
				snprintf(xstats_names[count].name,
					sizeof(xstats_names[count].name),
					"rx_q%u_%s", i,
					rte_virtio_rxq_stat_strings[t].name);
				count++;
			}
		}

		for (i = 0; i < dev->data->nb_tx_queues; i++) {
			struct virtqueue *txvq = dev->data->tx_queues[i];
			if (txvq == NULL)
				continue;
			for (t = 0; t < VIRTIO_NB_TXQ_XSTATS; t++) {
				snprintf(xstats_names[count].name,
					sizeof(xstats_names[count].name),
					"tx_q%u_%s", i,
					rte_virtio_txq_stat_strings[t].name);
				count++;
			}
		}
		return count;
	}
	return nstats;
}

static int
virtio_dev_xstats_get(struct rte_eth_dev *dev, struct rte_eth_xstat *xstats,
		      unsigned n)
{
	unsigned i;
	unsigned count = 0;

	unsigned nstats = dev->data->nb_tx_queues * VIRTIO_NB_TXQ_XSTATS +
		dev->data->nb_rx_queues * VIRTIO_NB_RXQ_XSTATS;

	if (n < nstats)
		return nstats;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		struct virtnet_rx *rxvq = dev->data->rx_queues[i];

		if (rxvq == NULL)
			continue;

		unsigned t;

		for (t = 0; t < VIRTIO_NB_RXQ_XSTATS; t++) {
			xstats[count].value = *(uint64_t *)(((char *)rxvq) +
				rte_virtio_rxq_stat_strings[t].offset);
			xstats[count].id = count;
			count++;
		}
	}

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		struct virtnet_tx *txvq = dev->data->tx_queues[i];

		if (txvq == NULL)
			continue;

		unsigned t;

		for (t = 0; t < VIRTIO_NB_TXQ_XSTATS; t++) {
			xstats[count].value = *(uint64_t *)(((char *)txvq) +
				rte_virtio_txq_stat_strings[t].offset);
			xstats[count].id = count;
			count++;
		}
	}

	return count;
}

static void
virtio_dev_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	virtio_update_stats(dev, stats);
}

static void
virtio_dev_stats_reset(struct rte_eth_dev *dev)
{
	unsigned int i;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		struct virtnet_tx *txvq = dev->data->tx_queues[i];
		if (txvq == NULL)
			continue;

		txvq->stats.packets = 0;
		txvq->stats.bytes = 0;
		txvq->stats.errors = 0;
		txvq->stats.multicast = 0;
		txvq->stats.broadcast = 0;
		memset(txvq->stats.size_bins, 0,
		       sizeof(txvq->stats.size_bins[0]) * 8);
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		struct virtnet_rx *rxvq = dev->data->rx_queues[i];
		if (rxvq == NULL)
			continue;

		rxvq->stats.packets = 0;
		rxvq->stats.bytes = 0;
		rxvq->stats.errors = 0;
		rxvq->stats.multicast = 0;
		rxvq->stats.broadcast = 0;
		memset(rxvq->stats.size_bins, 0,
		       sizeof(rxvq->stats.size_bins[0]) * 8);
	}
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

static int
virtio_mac_table_set(struct virtio_hw *hw,
		     const struct virtio_net_ctrl_mac *uc,
		     const struct virtio_net_ctrl_mac *mc)
{
	struct virtio_pmd_ctrl ctrl;
	int err, len[2];

	if (!vtpci_with_feature(hw, VIRTIO_NET_F_CTRL_MAC_ADDR)) {
		PMD_DRV_LOG(INFO, "host does not support mac table");
		return -1;
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
	return err;
}

static int
virtio_mac_addr_add(struct rte_eth_dev *dev, struct ether_addr *mac_addr,
		    uint32_t index, uint32_t vmdq __rte_unused)
{
	struct virtio_hw *hw = dev->data->dev_private;
	const struct ether_addr *addrs = dev->data->mac_addrs;
	unsigned int i;
	struct virtio_net_ctrl_mac *uc, *mc;

	if (index >= VIRTIO_MAX_MAC_ADDRS) {
		PMD_DRV_LOG(ERR, "mac address index %u out of range", index);
		return -EINVAL;
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

	return virtio_mac_table_set(hw, uc, mc);
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

static int
virtio_negotiate_features(struct virtio_hw *hw, uint64_t req_features)
{
	uint64_t host_features;

	/* Prepare guest_features: feature that driver wants to support */
	PMD_INIT_LOG(DEBUG, "guest_features before negotiate = %" PRIx64,
		req_features);

	/* Read device(host) feature bits */
	host_features = VTPCI_OPS(hw)->get_features(hw);
	PMD_INIT_LOG(DEBUG, "host_features before negotiate = %" PRIx64,
		host_features);

	/* If supported, ensure MTU value is valid before acknowledging it. */
	if (host_features & req_features & (1ULL << VIRTIO_NET_F_MTU)) {
		struct virtio_net_config config;

		vtpci_read_dev_config(hw,
			offsetof(struct virtio_net_config, mtu),
			&config.mtu, sizeof(config.mtu));

		if (config.mtu < ETHER_MIN_MTU)
			req_features &= ~(1ULL << VIRTIO_NET_F_MTU);
	}

	/*
	 * Negotiate features: Subset of device feature bits are written back
	 * guest feature bits.
	 */
	hw->guest_features = req_features;
	hw->guest_features = vtpci_negotiate_features(hw, host_features);
	PMD_INIT_LOG(DEBUG, "features after negotiate = %" PRIx64,
		hw->guest_features);

	if (hw->modern) {
		if (!vtpci_with_feature(hw, VIRTIO_F_VERSION_1)) {
			PMD_INIT_LOG(ERR,
				"VIRTIO_F_VERSION_1 features is not enabled.");
			return -1;
		}
		vtpci_set_status(hw, VIRTIO_CONFIG_STATUS_FEATURES_OK);
		if (!(vtpci_get_status(hw) & VIRTIO_CONFIG_STATUS_FEATURES_OK)) {
			PMD_INIT_LOG(ERR,
				"failed to set FEATURES_OK status!");
			return -1;
		}
	}

	hw->req_guest_features = req_features;

	return 0;
}

/*
 * Process Virtio Config changed interrupt and call the callback
 * if link state changed.
 */
void
virtio_interrupt_handler(void *param)
{
	struct rte_eth_dev *dev = param;
	struct virtio_hw *hw = dev->data->dev_private;
	uint8_t isr;

	/* Read interrupt status which clears interrupt */
	isr = vtpci_isr(hw);
	PMD_DRV_LOG(INFO, "interrupt status = %#x", isr);

	if (rte_intr_enable(dev->intr_handle) < 0)
		PMD_DRV_LOG(ERR, "interrupt enable failed");

	if (isr & VIRTIO_PCI_ISR_CONFIG) {
		if (virtio_dev_link_update(dev, 0) == 0)
			_rte_eth_dev_callback_process(dev,
						      RTE_ETH_EVENT_INTR_LSC,
						      NULL, NULL);
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

/* Only support 1:1 queue/interrupt mapping so far.
 * TODO: support n:1 queue/interrupt mapping when there are limited number of
 * interrupt vectors (<N+1).
 */
static int
virtio_queues_bind_intr(struct rte_eth_dev *dev)
{
	uint32_t i;
	struct virtio_hw *hw = dev->data->dev_private;

	PMD_INIT_LOG(INFO, "queue/interrupt binding");
	for (i = 0; i < dev->data->nb_rx_queues; ++i) {
		dev->intr_handle->intr_vec[i] = i + 1;
		if (VTPCI_OPS(hw)->set_queue_irq(hw, hw->vqs[i * 2], i + 1) ==
						 VIRTIO_MSI_NO_VECTOR) {
			PMD_DRV_LOG(ERR, "failed to set queue vector");
			return -EBUSY;
		}
	}

	return 0;
}

static void
virtio_queues_unbind_intr(struct rte_eth_dev *dev)
{
	uint32_t i;
	struct virtio_hw *hw = dev->data->dev_private;

	PMD_INIT_LOG(INFO, "queue/interrupt unbinding");
	for (i = 0; i < dev->data->nb_rx_queues; ++i)
		VTPCI_OPS(hw)->set_queue_irq(hw,
					     hw->vqs[i * VTNET_CQ],
					     VIRTIO_MSI_NO_VECTOR);
}

static int
virtio_configure_intr(struct rte_eth_dev *dev)
{
	struct virtio_hw *hw = dev->data->dev_private;

	if (!rte_intr_cap_multiple(dev->intr_handle)) {
		PMD_INIT_LOG(ERR, "Multiple intr vector not supported");
		return -ENOTSUP;
	}

	if (rte_intr_efd_enable(dev->intr_handle, dev->data->nb_rx_queues)) {
		PMD_INIT_LOG(ERR, "Fail to create eventfd");
		return -1;
	}

	if (!dev->intr_handle->intr_vec) {
		dev->intr_handle->intr_vec =
			rte_zmalloc("intr_vec",
				    hw->max_queue_pairs * sizeof(int), 0);
		if (!dev->intr_handle->intr_vec) {
			PMD_INIT_LOG(ERR, "Failed to allocate %u rxq vectors",
				     hw->max_queue_pairs);
			return -ENOMEM;
		}
	}

	/* Re-register callback to update max_intr */
	rte_intr_callback_unregister(dev->intr_handle,
				     virtio_interrupt_handler,
				     dev);
	rte_intr_callback_register(dev->intr_handle,
				   virtio_interrupt_handler,
				   dev);

	/* DO NOT try to remove this! This function will enable msix, or QEMU
	 * will encounter SIGSEGV when DRIVER_OK is sent.
	 * And for legacy devices, this should be done before queue/vec binding
	 * to change the config size from 20 to 24, or VIRTIO_MSI_QUEUE_VECTOR
	 * (22) will be ignored.
	 */
	if (rte_intr_enable(dev->intr_handle) < 0) {
		PMD_DRV_LOG(ERR, "interrupt enable failed");
		return -1;
	}

	if (virtio_queues_bind_intr(dev) < 0) {
		PMD_INIT_LOG(ERR, "Failed to bind queue/interrupt");
		return -1;
	}

	return 0;
}

/* reset device and renegotiate features if needed */
static int
virtio_init_device(struct rte_eth_dev *eth_dev, uint64_t req_features)
{
	struct virtio_hw *hw = eth_dev->data->dev_private;
	struct virtio_net_config *config;
	struct virtio_net_config local_config;
	struct rte_pci_device *pci_dev = NULL;
	int ret;

	/* Reset the device although not necessary at startup */
	vtpci_reset(hw);

	/* Tell the host we've noticed this device. */
	vtpci_set_status(hw, VIRTIO_CONFIG_STATUS_ACK);

	/* Tell the host we've known how to drive the device. */
	vtpci_set_status(hw, VIRTIO_CONFIG_STATUS_DRIVER);
	if (virtio_negotiate_features(hw, req_features) < 0)
		return -1;

	if (!hw->virtio_user_dev) {
		pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
		rte_eth_copy_pci_info(eth_dev, pci_dev);
	}

	eth_dev->data->dev_flags = RTE_ETH_DEV_DETACHABLE;
	/* If host does not support both status and MSI-X then disable LSC */
	if (vtpci_with_feature(hw, VIRTIO_NET_F_STATUS) && hw->use_msix)
		eth_dev->data->dev_flags |= RTE_ETH_DEV_INTR_LSC;
	else
		eth_dev->data->dev_flags &= ~RTE_ETH_DEV_INTR_LSC;

	rx_func_get(eth_dev);

	/* Setting up rx_header size for the device */
	if (vtpci_with_feature(hw, VIRTIO_NET_F_MRG_RXBUF) ||
	    vtpci_with_feature(hw, VIRTIO_F_VERSION_1))
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

		vtpci_read_dev_config(hw,
			offsetof(struct virtio_net_config, mac),
			&config->mac, sizeof(config->mac));

		if (vtpci_with_feature(hw, VIRTIO_NET_F_STATUS)) {
			vtpci_read_dev_config(hw,
				offsetof(struct virtio_net_config, status),
				&config->status, sizeof(config->status));
		} else {
			PMD_INIT_LOG(DEBUG,
				     "VIRTIO_NET_F_STATUS is not supported");
			config->status = 0;
		}

		if (vtpci_with_feature(hw, VIRTIO_NET_F_MQ)) {
			vtpci_read_dev_config(hw,
				offsetof(struct virtio_net_config, max_virtqueue_pairs),
				&config->max_virtqueue_pairs,
				sizeof(config->max_virtqueue_pairs));
		} else {
			PMD_INIT_LOG(DEBUG,
				     "VIRTIO_NET_F_MQ is not supported");
			config->max_virtqueue_pairs = 1;
		}

		hw->max_queue_pairs = config->max_virtqueue_pairs;

		if (vtpci_with_feature(hw, VIRTIO_NET_F_MTU)) {
			vtpci_read_dev_config(hw,
				offsetof(struct virtio_net_config, mtu),
				&config->mtu,
				sizeof(config->mtu));

			/*
			 * MTU value has already been checked at negotiation
			 * time, but check again in case it has changed since
			 * then, which should not happen.
			 */
			if (config->mtu < ETHER_MIN_MTU) {
				PMD_INIT_LOG(ERR, "invalid max MTU value (%u)",
						config->mtu);
				return -1;
			}

			hw->max_mtu = config->mtu;
			/* Set initial MTU to maximum one supported by vhost */
			eth_dev->data->mtu = config->mtu;

		} else {
			hw->max_mtu = VIRTIO_MAX_RX_PKTLEN - ETHER_HDR_LEN -
				VLAN_TAG_LEN - hw->vtnet_hdr_size;
		}

		PMD_INIT_LOG(DEBUG, "config->max_virtqueue_pairs=%d",
				config->max_virtqueue_pairs);
		PMD_INIT_LOG(DEBUG, "config->status=%d", config->status);
		PMD_INIT_LOG(DEBUG,
				"PORT MAC: %02X:%02X:%02X:%02X:%02X:%02X",
				config->mac[0], config->mac[1],
				config->mac[2], config->mac[3],
				config->mac[4], config->mac[5]);
	} else {
		PMD_INIT_LOG(DEBUG, "config->max_virtqueue_pairs=1");
		hw->max_queue_pairs = 1;
	}

	ret = virtio_alloc_queues(eth_dev);
	if (ret < 0)
		return ret;

	if (eth_dev->data->dev_conf.intr_conf.rxq) {
		if (virtio_configure_intr(eth_dev) < 0) {
			PMD_INIT_LOG(ERR, "failed to configure interrupt");
			return -1;
		}
	}

	vtpci_reinit_complete(hw);

	if (pci_dev)
		PMD_INIT_LOG(DEBUG, "port %d vendorID=0x%x deviceID=0x%x",
			eth_dev->data->port_id, pci_dev->id.vendor_id,
			pci_dev->id.device_id);

	return 0;
}

/*
 * Remap the PCI device again (IO port map for legacy device and
 * memory map for modern device), so that the secondary process
 * could have the PCI initiated correctly.
 */
static int
virtio_remap_pci(struct rte_pci_device *pci_dev, struct virtio_hw *hw)
{
	if (hw->modern) {
		/*
		 * We don't have to re-parse the PCI config space, since
		 * rte_pci_map_device() makes sure the mapped address
		 * in secondary process would equal to the one mapped in
		 * the primary process: error will be returned if that
		 * requirement is not met.
		 *
		 * That said, we could simply reuse all cap pointers
		 * (such as dev_cfg, common_cfg, etc.) parsed from the
		 * primary process, which is stored in shared memory.
		 */
		if (rte_pci_map_device(pci_dev)) {
			PMD_INIT_LOG(DEBUG, "failed to map pci device!");
			return -1;
		}
	} else {
		if (rte_pci_ioport_map(pci_dev, 0, VTPCI_IO(hw)) < 0)
			return -1;
	}

	return 0;
}

static void
virtio_set_vtpci_ops(struct virtio_hw *hw)
{
#ifdef RTE_VIRTIO_USER
	if (hw->virtio_user_dev)
		VTPCI_OPS(hw) = &virtio_user_ops;
	else
#endif
	if (hw->modern)
		VTPCI_OPS(hw) = &modern_ops;
	else
		VTPCI_OPS(hw) = &legacy_ops;
}

/*
 * This function is based on probe() function in virtio_pci.c
 * It returns 0 on success.
 */
int
eth_virtio_dev_init(struct rte_eth_dev *eth_dev)
{
	struct virtio_hw *hw = eth_dev->data->dev_private;
	int ret;

	RTE_BUILD_BUG_ON(RTE_PKTMBUF_HEADROOM < sizeof(struct virtio_net_hdr_mrg_rxbuf));

	eth_dev->dev_ops = &virtio_eth_dev_ops;
	eth_dev->tx_pkt_burst = &virtio_xmit_pkts;

	if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		if (!hw->virtio_user_dev) {
			ret = virtio_remap_pci(RTE_ETH_DEV_TO_PCI(eth_dev), hw);
			if (ret)
				return ret;
		}

		virtio_set_vtpci_ops(hw);
		if (hw->use_simple_rxtx) {
			eth_dev->tx_pkt_burst = virtio_xmit_pkts_simple;
			eth_dev->rx_pkt_burst = virtio_recv_pkts_vec;
		} else {
			rx_func_get(eth_dev);
		}
		return 0;
	}

	/* Allocate memory for storing MAC addresses */
	eth_dev->data->mac_addrs = rte_zmalloc("virtio", VIRTIO_MAX_MAC_ADDRS * ETHER_ADDR_LEN, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR,
			"Failed to allocate %d bytes needed to store MAC addresses",
			VIRTIO_MAX_MAC_ADDRS * ETHER_ADDR_LEN);
		return -ENOMEM;
	}

	hw->port_id = eth_dev->data->port_id;
	/* For virtio_user case the hw->virtio_user_dev is populated by
	 * virtio_user_eth_dev_alloc() before eth_virtio_dev_init() is called.
	 */
	if (!hw->virtio_user_dev) {
		ret = vtpci_init(RTE_ETH_DEV_TO_PCI(eth_dev), hw);
		if (ret)
			return ret;
	}

	/* reset device and negotiate default features */
	ret = virtio_init_device(eth_dev, VIRTIO_PMD_DEFAULT_GUEST_FEATURES);
	if (ret < 0)
		return ret;

	/* Setup interrupt callback  */
	if (eth_dev->data->dev_flags & RTE_ETH_DEV_INTR_LSC)
		rte_intr_callback_register(eth_dev->intr_handle,
			virtio_interrupt_handler, eth_dev);

	return 0;
}

static int
eth_virtio_dev_uninit(struct rte_eth_dev *eth_dev)
{
	PMD_INIT_FUNC_TRACE();

	if (rte_eal_process_type() == RTE_PROC_SECONDARY)
		return -EPERM;

	virtio_dev_stop(eth_dev);
	virtio_dev_close(eth_dev);

	eth_dev->dev_ops = NULL;
	eth_dev->tx_pkt_burst = NULL;
	eth_dev->rx_pkt_burst = NULL;

	rte_free(eth_dev->data->mac_addrs);
	eth_dev->data->mac_addrs = NULL;

	/* reset interrupt callback  */
	if (eth_dev->data->dev_flags & RTE_ETH_DEV_INTR_LSC)
		rte_intr_callback_unregister(eth_dev->intr_handle,
						virtio_interrupt_handler,
						eth_dev);
	if (eth_dev->device)
		rte_pci_unmap_device(RTE_ETH_DEV_TO_PCI(eth_dev));

	PMD_INIT_LOG(DEBUG, "dev_uninit completed");

	return 0;
}

static int eth_virtio_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev, sizeof(struct virtio_hw),
		eth_virtio_dev_init);
}

static int eth_virtio_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, eth_virtio_dev_uninit);
}

static struct rte_pci_driver rte_virtio_pmd = {
	.driver = {
		.name = "net_virtio",
	},
	.id_table = pci_id_virtio_map,
	.drv_flags = 0,
	.probe = eth_virtio_pci_probe,
	.remove = eth_virtio_pci_remove,
};

RTE_INIT(rte_virtio_pmd_init);
static void
rte_virtio_pmd_init(void)
{
	if (rte_eal_iopl_init() != 0) {
		PMD_INIT_LOG(ERR, "IOPL call failed - cannot use virtio PMD");
		return;
	}

	rte_pci_register(&rte_virtio_pmd);
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
	int ret;

	PMD_INIT_LOG(DEBUG, "configure");

	if (dev->data->dev_conf.intr_conf.rxq) {
		ret = virtio_init_device(dev, hw->req_guest_features);
		if (ret < 0)
			return ret;
	}

	/* Virtio does L4 checksum but not L3! */
	if (rxmode->hw_ip_checksum) {
		PMD_DRV_LOG(NOTICE,
			    "virtio does not support IP checksum");
		return -ENOTSUP;
	}

	if (rxmode->enable_lro) {
		PMD_DRV_LOG(NOTICE,
			    "virtio does not support Large Receive Offload");
		return -ENOTSUP;
	}

	/* start control queue */
	if (vtpci_with_feature(hw, VIRTIO_NET_F_CTRL_VQ))
		virtio_dev_cq_start(dev);

	hw->vlan_strip = rxmode->hw_vlan_strip;

	if (rxmode->hw_vlan_filter
	    && !vtpci_with_feature(hw, VIRTIO_NET_F_CTRL_VLAN)) {
		PMD_DRV_LOG(NOTICE,
			    "vlan filtering not available on this host");
		return -ENOTSUP;
	}

	if (dev->data->dev_flags & RTE_ETH_DEV_INTR_LSC)
		/* Enable vector (0) for Link State Intrerrupt */
		if (VTPCI_OPS(hw)->set_config_irq(hw, 0) ==
				VIRTIO_MSI_NO_VECTOR) {
			PMD_DRV_LOG(ERR, "failed to set config vector");
			return -EBUSY;
		}

	return 0;
}


static int
virtio_dev_start(struct rte_eth_dev *dev)
{
	uint16_t nb_queues, i;
	struct virtnet_rx *rxvq;
	struct virtnet_tx *txvq __rte_unused;
	struct virtio_hw *hw = dev->data->dev_private;

	/* check if lsc interrupt feature is enabled */
	if (dev->data->dev_conf.intr_conf.lsc) {
		if (!(dev->data->dev_flags & RTE_ETH_DEV_INTR_LSC)) {
			PMD_DRV_LOG(ERR, "link status not supported by host");
			return -ENOTSUP;
		}
	}

	/* Enable uio/vfio intr/eventfd mapping: althrough we already did that
	 * in device configure, but it could be unmapped  when device is
	 * stopped.
	 */
	if (dev->data->dev_conf.intr_conf.lsc ||
	    dev->data->dev_conf.intr_conf.rxq) {
		rte_intr_disable(dev->intr_handle);

		if (rte_intr_enable(dev->intr_handle) < 0) {
			PMD_DRV_LOG(ERR, "interrupt enable failed");
			return -EIO;
		}
	}

	/*Notify the backend
	 *Otherwise the tap backend might already stop its queue due to fullness.
	 *vhost backend will have no chance to be waked up
	 */
	nb_queues = RTE_MAX(dev->data->nb_rx_queues, dev->data->nb_tx_queues);
	if (hw->max_queue_pairs > 1) {
		if (virtio_set_multiple_queues(dev, nb_queues) != 0)
			return -EINVAL;
	}

	PMD_INIT_LOG(DEBUG, "nb_queues=%d", nb_queues);

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxvq = dev->data->rx_queues[i];
		virtqueue_notify(rxvq->vq);
	}

	PMD_INIT_LOG(DEBUG, "Notified backend at initialization");

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxvq = dev->data->rx_queues[i];
		VIRTQUEUE_DUMP(rxvq->vq);
	}

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txvq = dev->data->tx_queues[i];
		VIRTQUEUE_DUMP(txvq->vq);
	}

	hw->started = 1;

	/* Initialize Link state */
	virtio_dev_link_update(dev, 0);

	return 0;
}

static void virtio_dev_free_mbufs(struct rte_eth_dev *dev)
{
	struct rte_mbuf *buf;
	int i, mbuf_num = 0;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		struct virtnet_rx *rxvq = dev->data->rx_queues[i];

		PMD_INIT_LOG(DEBUG,
			     "Before freeing rxq[%d] used and unused buf", i);
		VIRTQUEUE_DUMP(rxvq->vq);

		PMD_INIT_LOG(DEBUG, "rx_queues[%d]=%p", i, rxvq);
		while ((buf = virtqueue_detatch_unused(rxvq->vq)) != NULL) {
			rte_pktmbuf_free(buf);
			mbuf_num++;
		}

		PMD_INIT_LOG(DEBUG, "free %d mbufs", mbuf_num);
		PMD_INIT_LOG(DEBUG,
			     "After freeing rxq[%d] used and unused buf", i);
		VIRTQUEUE_DUMP(rxvq->vq);
	}

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		struct virtnet_tx *txvq = dev->data->tx_queues[i];

		PMD_INIT_LOG(DEBUG,
			     "Before freeing txq[%d] used and unused bufs",
			     i);
		VIRTQUEUE_DUMP(txvq->vq);

		mbuf_num = 0;
		while ((buf = virtqueue_detatch_unused(txvq->vq)) != NULL) {
			rte_pktmbuf_free(buf);
			mbuf_num++;
		}

		PMD_INIT_LOG(DEBUG, "free %d mbufs", mbuf_num);
		PMD_INIT_LOG(DEBUG,
			     "After freeing txq[%d] used and unused buf", i);
		VIRTQUEUE_DUMP(txvq->vq);
	}
}

/*
 * Stop device: disable interrupt and mark link down
 */
static void
virtio_dev_stop(struct rte_eth_dev *dev)
{
	struct virtio_hw *hw = dev->data->dev_private;
	struct rte_eth_link link;
	struct rte_intr_conf *intr_conf = &dev->data->dev_conf.intr_conf;

	PMD_INIT_LOG(DEBUG, "stop");

	if (intr_conf->lsc || intr_conf->rxq)
		rte_intr_disable(dev->intr_handle);

	hw->started = 0;
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
	link.link_duplex = ETH_LINK_FULL_DUPLEX;
	link.link_speed  = SPEED_10G;

	if (hw->started == 0) {
		link.link_status = ETH_LINK_DOWN;
	} else if (vtpci_with_feature(hw, VIRTIO_NET_F_STATUS)) {
		PMD_INIT_LOG(DEBUG, "Get link status from hw");
		vtpci_read_dev_config(hw,
				offsetof(struct virtio_net_config, status),
				&status, sizeof(status));
		if ((status & VIRTIO_NET_S_LINK_UP) == 0) {
			link.link_status = ETH_LINK_DOWN;
			PMD_INIT_LOG(DEBUG, "Port %d is down",
				     dev->data->port_id);
		} else {
			link.link_status = ETH_LINK_UP;
			PMD_INIT_LOG(DEBUG, "Port %d is up",
				     dev->data->port_id);
		}
	} else {
		link.link_status = ETH_LINK_UP;
	}
	virtio_dev_atomic_write_link_status(dev, &link);

	return (old.link_status == link.link_status) ? -1 : 0;
}

static void
virtio_dev_info_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	uint64_t tso_mask, host_features;
	struct virtio_hw *hw = dev->data->dev_private;

	dev_info->speed_capa = ETH_LINK_SPEED_10G; /* fake value */

	dev_info->pci_dev = dev->device ? RTE_ETH_DEV_TO_PCI(dev) : NULL;
	dev_info->max_rx_queues =
		RTE_MIN(hw->max_queue_pairs, VIRTIO_MAX_RX_QUEUES);
	dev_info->max_tx_queues =
		RTE_MIN(hw->max_queue_pairs, VIRTIO_MAX_TX_QUEUES);
	dev_info->min_rx_bufsize = VIRTIO_MIN_RX_BUFSIZE;
	dev_info->max_rx_pktlen = VIRTIO_MAX_RX_PKTLEN;
	dev_info->max_mac_addrs = VIRTIO_MAX_MAC_ADDRS;
	dev_info->default_txconf = (struct rte_eth_txconf) {
		.txq_flags = ETH_TXQ_FLAGS_NOOFFLOADS
	};

	host_features = VTPCI_OPS(hw)->get_features(hw);
	dev_info->rx_offload_capa = 0;
	if (host_features & (1ULL << VIRTIO_NET_F_GUEST_CSUM)) {
		dev_info->rx_offload_capa |=
			DEV_RX_OFFLOAD_TCP_CKSUM |
			DEV_RX_OFFLOAD_UDP_CKSUM;
	}
	tso_mask = (1ULL << VIRTIO_NET_F_GUEST_TSO4) |
		(1ULL << VIRTIO_NET_F_GUEST_TSO6);

	dev_info->tx_offload_capa = 0;
	if (hw->guest_features & (1ULL << VIRTIO_NET_F_CSUM)) {
		dev_info->tx_offload_capa |=
			DEV_TX_OFFLOAD_UDP_CKSUM |
			DEV_TX_OFFLOAD_TCP_CKSUM;
	}
	tso_mask = (1ULL << VIRTIO_NET_F_HOST_TSO4) |
		(1ULL << VIRTIO_NET_F_HOST_TSO6);
	if ((hw->guest_features & tso_mask) == tso_mask)
		dev_info->tx_offload_capa |= DEV_TX_OFFLOAD_TCP_TSO;
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

RTE_PMD_EXPORT_NAME(net_virtio, __COUNTER__);
RTE_PMD_REGISTER_PCI_TABLE(net_virtio, pci_id_virtio_map);
RTE_PMD_REGISTER_KMOD_DEP(net_virtio, "* igb_uio | uio_pci_generic | vfio-pci");
