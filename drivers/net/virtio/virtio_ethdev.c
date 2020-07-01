/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include <rte_ethdev_driver.h>
#include <rte_ethdev_pci.h>
#include <rte_memcpy.h>
#include <rte_string_fns.h>
#include <rte_memzone.h>
#include <rte_malloc.h>
#include <rte_branch_prediction.h>
#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_arp.h>
#include <rte_common.h>
#include <rte_errno.h>
#include <rte_cpuflags.h>

#include <rte_memory.h>
#include <rte_eal.h>
#include <rte_dev.h>
#include <rte_cycles.h>
#include <rte_kvargs.h>

#include "virtio_ethdev.h"
#include "virtio_pci.h"
#include "virtio_logs.h"
#include "virtqueue.h"
#include "virtio_rxtx.h"
#include "virtio_user/virtio_user_dev.h"

static int eth_virtio_dev_uninit(struct rte_eth_dev *eth_dev);
static int  virtio_dev_configure(struct rte_eth_dev *dev);
static int  virtio_dev_start(struct rte_eth_dev *dev);
static void virtio_dev_stop(struct rte_eth_dev *dev);
static int virtio_dev_promiscuous_enable(struct rte_eth_dev *dev);
static int virtio_dev_promiscuous_disable(struct rte_eth_dev *dev);
static int virtio_dev_allmulticast_enable(struct rte_eth_dev *dev);
static int virtio_dev_allmulticast_disable(struct rte_eth_dev *dev);
static uint32_t virtio_dev_speed_capa_get(uint32_t speed);
static int virtio_dev_devargs_parse(struct rte_devargs *devargs,
	int *vdpa,
	uint32_t *speed,
	int *vectorized);
static int virtio_dev_info_get(struct rte_eth_dev *dev,
				struct rte_eth_dev_info *dev_info);
static int virtio_dev_link_update(struct rte_eth_dev *dev,
	int wait_to_complete);
static int virtio_dev_vlan_offload_set(struct rte_eth_dev *dev, int mask);

static void virtio_set_hwaddr(struct virtio_hw *hw);
static void virtio_get_hwaddr(struct virtio_hw *hw);

static int virtio_dev_stats_get(struct rte_eth_dev *dev,
				 struct rte_eth_stats *stats);
static int virtio_dev_xstats_get(struct rte_eth_dev *dev,
				 struct rte_eth_xstat *xstats, unsigned n);
static int virtio_dev_xstats_get_names(struct rte_eth_dev *dev,
				       struct rte_eth_xstat_name *xstats_names,
				       unsigned limit);
static int virtio_dev_stats_reset(struct rte_eth_dev *dev);
static void virtio_dev_free_mbufs(struct rte_eth_dev *dev);
static int virtio_vlan_filter_set(struct rte_eth_dev *dev,
				uint16_t vlan_id, int on);
static int virtio_mac_addr_add(struct rte_eth_dev *dev,
				struct rte_ether_addr *mac_addr,
				uint32_t index, uint32_t vmdq);
static void virtio_mac_addr_remove(struct rte_eth_dev *dev, uint32_t index);
static int virtio_mac_addr_set(struct rte_eth_dev *dev,
				struct rte_ether_addr *mac_addr);

static int virtio_intr_disable(struct rte_eth_dev *dev);

static int virtio_dev_queue_stats_mapping_set(
	struct rte_eth_dev *eth_dev,
	uint16_t queue_id,
	uint8_t stat_idx,
	uint8_t is_rx);

static void virtio_notify_peers(struct rte_eth_dev *dev);
static void virtio_ack_link_announce(struct rte_eth_dev *dev);

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

static struct virtio_pmd_ctrl *
virtio_send_command_packed(struct virtnet_ctl *cvq,
			   struct virtio_pmd_ctrl *ctrl,
			   int *dlen, int pkt_num)
{
	struct virtqueue *vq = cvq->vq;
	int head;
	struct vring_packed_desc *desc = vq->vq_packed.ring.desc;
	struct virtio_pmd_ctrl *result;
	uint16_t flags;
	int sum = 0;
	int nb_descs = 0;
	int k;

	/*
	 * Format is enforced in qemu code:
	 * One TX packet for header;
	 * At least one TX packet per argument;
	 * One RX packet for ACK.
	 */
	head = vq->vq_avail_idx;
	flags = vq->vq_packed.cached_flags;
	desc[head].addr = cvq->virtio_net_hdr_mem;
	desc[head].len = sizeof(struct virtio_net_ctrl_hdr);
	vq->vq_free_cnt--;
	nb_descs++;
	if (++vq->vq_avail_idx >= vq->vq_nentries) {
		vq->vq_avail_idx -= vq->vq_nentries;
		vq->vq_packed.cached_flags ^= VRING_PACKED_DESC_F_AVAIL_USED;
	}

	for (k = 0; k < pkt_num; k++) {
		desc[vq->vq_avail_idx].addr = cvq->virtio_net_hdr_mem
			+ sizeof(struct virtio_net_ctrl_hdr)
			+ sizeof(ctrl->status) + sizeof(uint8_t) * sum;
		desc[vq->vq_avail_idx].len = dlen[k];
		desc[vq->vq_avail_idx].flags = VRING_DESC_F_NEXT |
			vq->vq_packed.cached_flags;
		sum += dlen[k];
		vq->vq_free_cnt--;
		nb_descs++;
		if (++vq->vq_avail_idx >= vq->vq_nentries) {
			vq->vq_avail_idx -= vq->vq_nentries;
			vq->vq_packed.cached_flags ^=
				VRING_PACKED_DESC_F_AVAIL_USED;
		}
	}

	desc[vq->vq_avail_idx].addr = cvq->virtio_net_hdr_mem
		+ sizeof(struct virtio_net_ctrl_hdr);
	desc[vq->vq_avail_idx].len = sizeof(ctrl->status);
	desc[vq->vq_avail_idx].flags = VRING_DESC_F_WRITE |
		vq->vq_packed.cached_flags;
	vq->vq_free_cnt--;
	nb_descs++;
	if (++vq->vq_avail_idx >= vq->vq_nentries) {
		vq->vq_avail_idx -= vq->vq_nentries;
		vq->vq_packed.cached_flags ^= VRING_PACKED_DESC_F_AVAIL_USED;
	}

	virtio_wmb(vq->hw->weak_barriers);
	desc[head].flags = VRING_DESC_F_NEXT | flags;

	virtio_wmb(vq->hw->weak_barriers);
	virtqueue_notify(vq);

	/* wait for used descriptors in virtqueue */
	while (!desc_is_used(&desc[head], vq))
		usleep(100);

	virtio_rmb(vq->hw->weak_barriers);

	/* now get used descriptors */
	vq->vq_free_cnt += nb_descs;
	vq->vq_used_cons_idx += nb_descs;
	if (vq->vq_used_cons_idx >= vq->vq_nentries) {
		vq->vq_used_cons_idx -= vq->vq_nentries;
		vq->vq_packed.used_wrap_counter ^= 1;
	}

	PMD_INIT_LOG(DEBUG, "vq->vq_free_cnt=%d\n"
			"vq->vq_avail_idx=%d\n"
			"vq->vq_used_cons_idx=%d\n"
			"vq->vq_packed.cached_flags=0x%x\n"
			"vq->vq_packed.used_wrap_counter=%d\n",
			vq->vq_free_cnt,
			vq->vq_avail_idx,
			vq->vq_used_cons_idx,
			vq->vq_packed.cached_flags,
			vq->vq_packed.used_wrap_counter);

	result = cvq->virtio_net_hdr_mz->addr;
	return result;
}

static struct virtio_pmd_ctrl *
virtio_send_command_split(struct virtnet_ctl *cvq,
			  struct virtio_pmd_ctrl *ctrl,
			  int *dlen, int pkt_num)
{
	struct virtio_pmd_ctrl *result;
	struct virtqueue *vq = cvq->vq;
	uint32_t head, i;
	int k, sum = 0;

	head = vq->vq_desc_head_idx;

	/*
	 * Format is enforced in qemu code:
	 * One TX packet for header;
	 * At least one TX packet per argument;
	 * One RX packet for ACK.
	 */
	vq->vq_split.ring.desc[head].flags = VRING_DESC_F_NEXT;
	vq->vq_split.ring.desc[head].addr = cvq->virtio_net_hdr_mem;
	vq->vq_split.ring.desc[head].len = sizeof(struct virtio_net_ctrl_hdr);
	vq->vq_free_cnt--;
	i = vq->vq_split.ring.desc[head].next;

	for (k = 0; k < pkt_num; k++) {
		vq->vq_split.ring.desc[i].flags = VRING_DESC_F_NEXT;
		vq->vq_split.ring.desc[i].addr = cvq->virtio_net_hdr_mem
			+ sizeof(struct virtio_net_ctrl_hdr)
			+ sizeof(ctrl->status) + sizeof(uint8_t)*sum;
		vq->vq_split.ring.desc[i].len = dlen[k];
		sum += dlen[k];
		vq->vq_free_cnt--;
		i = vq->vq_split.ring.desc[i].next;
	}

	vq->vq_split.ring.desc[i].flags = VRING_DESC_F_WRITE;
	vq->vq_split.ring.desc[i].addr = cvq->virtio_net_hdr_mem
			+ sizeof(struct virtio_net_ctrl_hdr);
	vq->vq_split.ring.desc[i].len = sizeof(ctrl->status);
	vq->vq_free_cnt--;

	vq->vq_desc_head_idx = vq->vq_split.ring.desc[i].next;

	vq_update_avail_ring(vq, head);
	vq_update_avail_idx(vq);

	PMD_INIT_LOG(DEBUG, "vq->vq_queue_index = %d", vq->vq_queue_index);

	virtqueue_notify(vq);

	while (virtqueue_nused(vq) == 0)
		usleep(100);

	while (virtqueue_nused(vq)) {
		uint32_t idx, desc_idx, used_idx;
		struct vring_used_elem *uep;

		used_idx = (uint32_t)(vq->vq_used_cons_idx
				& (vq->vq_nentries - 1));
		uep = &vq->vq_split.ring.used->ring[used_idx];
		idx = (uint32_t) uep->id;
		desc_idx = idx;

		while (vq->vq_split.ring.desc[desc_idx].flags &
				VRING_DESC_F_NEXT) {
			desc_idx = vq->vq_split.ring.desc[desc_idx].next;
			vq->vq_free_cnt++;
		}

		vq->vq_split.ring.desc[desc_idx].next = vq->vq_desc_head_idx;
		vq->vq_desc_head_idx = idx;

		vq->vq_used_cons_idx++;
		vq->vq_free_cnt++;
	}

	PMD_INIT_LOG(DEBUG, "vq->vq_free_cnt=%d\nvq->vq_desc_head_idx=%d",
			vq->vq_free_cnt, vq->vq_desc_head_idx);

	result = cvq->virtio_net_hdr_mz->addr;
	return result;
}

static int
virtio_send_command(struct virtnet_ctl *cvq, struct virtio_pmd_ctrl *ctrl,
		    int *dlen, int pkt_num)
{
	virtio_net_ctrl_ack status = ~0;
	struct virtio_pmd_ctrl *result;
	struct virtqueue *vq;

	ctrl->status = status;

	if (!cvq || !cvq->vq) {
		PMD_INIT_LOG(ERR, "Control queue is not supported.");
		return -1;
	}

	rte_spinlock_lock(&cvq->lock);
	vq = cvq->vq;

	PMD_INIT_LOG(DEBUG, "vq->vq_desc_head_idx = %d, status = %d, "
		"vq->hw->cvq = %p vq = %p",
		vq->vq_desc_head_idx, status, vq->hw->cvq, vq);

	if (vq->vq_free_cnt < pkt_num + 2 || pkt_num < 1) {
		rte_spinlock_unlock(&cvq->lock);
		return -1;
	}

	memcpy(cvq->virtio_net_hdr_mz->addr, ctrl,
		sizeof(struct virtio_pmd_ctrl));

	if (vtpci_packed_queue(vq->hw))
		result = virtio_send_command_packed(cvq, ctrl, dlen, pkt_num);
	else
		result = virtio_send_command_split(cvq, ctrl, dlen, pkt_num);

	rte_spinlock_unlock(&cvq->lock);
	return result->status;
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
	uint8_t *ring_mem = vq->vq_ring_virt_mem;

	PMD_INIT_FUNC_TRACE();

	memset(ring_mem, 0, vq->vq_ring_size);

	vq->vq_used_cons_idx = 0;
	vq->vq_desc_head_idx = 0;
	vq->vq_avail_idx = 0;
	vq->vq_desc_tail_idx = (uint16_t)(vq->vq_nentries - 1);
	vq->vq_free_cnt = vq->vq_nentries;
	memset(vq->vq_descx, 0, sizeof(struct vq_desc_extra) * vq->vq_nentries);
	if (vtpci_packed_queue(vq->hw)) {
		vring_init_packed(&vq->vq_packed.ring, ring_mem,
				  VIRTIO_PCI_VRING_ALIGN, size);
		vring_desc_init_packed(vq, size);
	} else {
		struct vring *vr = &vq->vq_split.ring;

		vring_init_split(vr, ring_mem, VIRTIO_PCI_VRING_ALIGN, size);
		vring_desc_init_split(vr->desc, size);
	}
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
	int numa_node = dev->device->numa_node;

	PMD_INIT_LOG(INFO, "setting up queue: %u on NUMA node %d",
			vtpci_queue_idx, numa_node);

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

	if (!vtpci_packed_queue(hw) && !rte_is_power_of_2(vq_size)) {
		PMD_INIT_LOG(ERR, "split virtqueue size is not power of 2");
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
				numa_node);
	if (vq == NULL) {
		PMD_INIT_LOG(ERR, "can not allocate vq");
		return -ENOMEM;
	}
	hw->vqs[vtpci_queue_idx] = vq;

	vq->hw = hw;
	vq->vq_queue_index = vtpci_queue_idx;
	vq->vq_nentries = vq_size;
	if (vtpci_packed_queue(hw)) {
		vq->vq_packed.used_wrap_counter = 1;
		vq->vq_packed.cached_flags = VRING_PACKED_DESC_F_AVAIL;
		vq->vq_packed.event_flags_shadow = 0;
		if (queue_type == VTNET_RQ)
			vq->vq_packed.cached_flags |= VRING_DESC_F_WRITE;
	}

	/*
	 * Reserve a memzone for vring elements
	 */
	size = vring_size(hw, vq_size, VIRTIO_PCI_VRING_ALIGN);
	vq->vq_ring_size = RTE_ALIGN_CEIL(size, VIRTIO_PCI_VRING_ALIGN);
	PMD_INIT_LOG(DEBUG, "vring_size: %d, rounded_vring_size: %d",
		     size, vq->vq_ring_size);

	mz = rte_memzone_reserve_aligned(vq_name, vq->vq_ring_size,
			numa_node, RTE_MEMZONE_IOVA_CONTIG,
			VIRTIO_PCI_VRING_ALIGN);
	if (mz == NULL) {
		if (rte_errno == EEXIST)
			mz = rte_memzone_lookup(vq_name);
		if (mz == NULL) {
			ret = -ENOMEM;
			goto fail_q_alloc;
		}
	}

	memset(mz->addr, 0, mz->len);

	vq->vq_ring_mem = mz->iova;
	vq->vq_ring_virt_mem = mz->addr;
	PMD_INIT_LOG(DEBUG, "vq->vq_ring_mem:      0x%" PRIx64,
		     (uint64_t)mz->iova);
	PMD_INIT_LOG(DEBUG, "vq->vq_ring_virt_mem: 0x%" PRIx64,
		     (uint64_t)(uintptr_t)mz->addr);

	virtio_init_vring(vq);

	if (sz_hdr_mz) {
		snprintf(vq_hdr_name, sizeof(vq_hdr_name), "port%d_vq%d_hdr",
			 dev->data->port_id, vtpci_queue_idx);
		hdr_mz = rte_memzone_reserve_aligned(vq_hdr_name, sz_hdr_mz,
				numa_node, RTE_MEMZONE_IOVA_CONTIG,
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
				RTE_CACHE_LINE_SIZE, numa_node);
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
		txvq->virtio_net_hdr_mem = hdr_mz->iova;
	} else if (queue_type == VTNET_CQ) {
		cvq = &vq->cq;
		cvq->vq = vq;
		cvq->mz = mz;
		cvq->virtio_net_hdr_mz = hdr_mz;
		cvq->virtio_net_hdr_mem = hdr_mz->iova;
		memset(cvq->virtio_net_hdr_mz->addr, 0, PAGE_SIZE);

		hw->cvq = cvq;
	}

	/* For virtio_user case (that is when hw->virtio_user_dev is not NULL),
	 * we use virtual address. And we need properly set _offset_, please see
	 * VIRTIO_MBUF_DATA_DMA_ADDR in virtqueue.h for more information.
	 */
	if (!hw->virtio_user_dev)
		vq->offset = offsetof(struct rte_mbuf, buf_iova);
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

			/* first indirect descriptor is always the tx header */
			if (!vtpci_packed_queue(hw)) {
				vring_desc_init_split(start_dp,
						      RTE_DIM(txr[i].tx_indir));
				start_dp->addr = txvq->virtio_net_hdr_mem
					+ i * sizeof(*txr)
					+ offsetof(struct virtio_tx_region,
						   tx_hdr);
				start_dp->len = hw->vtnet_hdr_size;
				start_dp->flags = VRING_DESC_F_NEXT;
			}
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

	if (!hw->opened)
		return;
	hw->opened = false;

	/* reset the NIC */
	if (dev->data->dev_flags & RTE_ETH_DEV_INTR_LSC)
		VTPCI_OPS(hw)->set_config_irq(hw, VIRTIO_MSI_NO_VECTOR);
	if (intr_conf->rxq)
		virtio_queues_unbind_intr(dev);

	if (intr_conf->lsc || intr_conf->rxq) {
		virtio_intr_disable(dev);
		rte_intr_efd_disable(dev->intr_handle);
		rte_free(dev->intr_handle->intr_vec);
		dev->intr_handle->intr_vec = NULL;
	}

	vtpci_reset(hw);
	virtio_dev_free_mbufs(dev);
	virtio_free_queues(hw);

#ifdef RTE_VIRTIO_USER
	if (hw->virtio_user_dev)
		virtio_user_dev_uninit(hw->virtio_user_dev);
	else
#endif
	if (dev->device) {
		rte_pci_unmap_device(RTE_ETH_DEV_TO_PCI(dev));
		if (!hw->modern)
			rte_pci_ioport_unmap(VTPCI_IO(hw));
	}
}

static int
virtio_dev_promiscuous_enable(struct rte_eth_dev *dev)
{
	struct virtio_hw *hw = dev->data->dev_private;
	struct virtio_pmd_ctrl ctrl;
	int dlen[1];
	int ret;

	if (!vtpci_with_feature(hw, VIRTIO_NET_F_CTRL_RX)) {
		PMD_INIT_LOG(INFO, "host does not support rx control");
		return -ENOTSUP;
	}

	ctrl.hdr.class = VIRTIO_NET_CTRL_RX;
	ctrl.hdr.cmd = VIRTIO_NET_CTRL_RX_PROMISC;
	ctrl.data[0] = 1;
	dlen[0] = 1;

	ret = virtio_send_command(hw->cvq, &ctrl, dlen, 1);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to enable promisc");
		return -EAGAIN;
	}

	return 0;
}

static int
virtio_dev_promiscuous_disable(struct rte_eth_dev *dev)
{
	struct virtio_hw *hw = dev->data->dev_private;
	struct virtio_pmd_ctrl ctrl;
	int dlen[1];
	int ret;

	if (!vtpci_with_feature(hw, VIRTIO_NET_F_CTRL_RX)) {
		PMD_INIT_LOG(INFO, "host does not support rx control");
		return -ENOTSUP;
	}

	ctrl.hdr.class = VIRTIO_NET_CTRL_RX;
	ctrl.hdr.cmd = VIRTIO_NET_CTRL_RX_PROMISC;
	ctrl.data[0] = 0;
	dlen[0] = 1;

	ret = virtio_send_command(hw->cvq, &ctrl, dlen, 1);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to disable promisc");
		return -EAGAIN;
	}

	return 0;
}

static int
virtio_dev_allmulticast_enable(struct rte_eth_dev *dev)
{
	struct virtio_hw *hw = dev->data->dev_private;
	struct virtio_pmd_ctrl ctrl;
	int dlen[1];
	int ret;

	if (!vtpci_with_feature(hw, VIRTIO_NET_F_CTRL_RX)) {
		PMD_INIT_LOG(INFO, "host does not support rx control");
		return -ENOTSUP;
	}

	ctrl.hdr.class = VIRTIO_NET_CTRL_RX;
	ctrl.hdr.cmd = VIRTIO_NET_CTRL_RX_ALLMULTI;
	ctrl.data[0] = 1;
	dlen[0] = 1;

	ret = virtio_send_command(hw->cvq, &ctrl, dlen, 1);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to enable allmulticast");
		return -EAGAIN;
	}

	return 0;
}

static int
virtio_dev_allmulticast_disable(struct rte_eth_dev *dev)
{
	struct virtio_hw *hw = dev->data->dev_private;
	struct virtio_pmd_ctrl ctrl;
	int dlen[1];
	int ret;

	if (!vtpci_with_feature(hw, VIRTIO_NET_F_CTRL_RX)) {
		PMD_INIT_LOG(INFO, "host does not support rx control");
		return -ENOTSUP;
	}

	ctrl.hdr.class = VIRTIO_NET_CTRL_RX;
	ctrl.hdr.cmd = VIRTIO_NET_CTRL_RX_ALLMULTI;
	ctrl.data[0] = 0;
	dlen[0] = 1;

	ret = virtio_send_command(hw->cvq, &ctrl, dlen, 1);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to disable allmulticast");
		return -EAGAIN;
	}

	return 0;
}

#define VLAN_TAG_LEN           4    /* 802.3ac tag (not DMA'd) */
static int
virtio_mtu_set(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct virtio_hw *hw = dev->data->dev_private;
	uint32_t ether_hdr_len = RTE_ETHER_HDR_LEN + VLAN_TAG_LEN +
				 hw->vtnet_hdr_size;
	uint32_t frame_size = mtu + ether_hdr_len;
	uint32_t max_frame_size = hw->max_mtu + ether_hdr_len;

	max_frame_size = RTE_MIN(max_frame_size, VIRTIO_MAX_RX_PKTLEN);

	if (mtu < RTE_ETHER_MIN_MTU || frame_size > max_frame_size) {
		PMD_INIT_LOG(ERR, "MTU should be between %d and %d",
			RTE_ETHER_MIN_MTU, max_frame_size - ether_hdr_len);
		return -EINVAL;
	}
	return 0;
}

static int
virtio_dev_rx_queue_intr_enable(struct rte_eth_dev *dev, uint16_t queue_id)
{
	struct virtio_hw *hw = dev->data->dev_private;
	struct virtnet_rx *rxvq = dev->data->rx_queues[queue_id];
	struct virtqueue *vq = rxvq->vq;

	virtqueue_enable_intr(vq);
	virtio_mb(hw->weak_barriers);
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
	.vlan_offload_set        = virtio_dev_vlan_offload_set,
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

/*
 * dev_ops for virtio-user in secondary processes, as we just have
 * some limited supports currently.
 */
const struct eth_dev_ops virtio_user_secondary_eth_dev_ops = {
	.dev_infos_get           = virtio_dev_info_get,
	.stats_get               = virtio_dev_stats_get,
	.xstats_get              = virtio_dev_xstats_get,
	.xstats_get_names        = virtio_dev_xstats_get_names,
	.stats_reset             = virtio_dev_stats_reset,
	.xstats_reset            = virtio_dev_stats_reset,
	/* collect stats per queue */
	.queue_stats_mapping_set = virtio_dev_queue_stats_mapping_set,
};

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
			struct virtnet_rx *rxvq = dev->data->rx_queues[i];
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
			struct virtnet_tx *txvq = dev->data->tx_queues[i];
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

static int
virtio_dev_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	virtio_update_stats(dev, stats);

	return 0;
}

static int
virtio_dev_stats_reset(struct rte_eth_dev *dev)
{
	unsigned int i;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		struct virtnet_tx *txvq = dev->data->tx_queues[i];
		if (txvq == NULL)
			continue;

		txvq->stats.packets = 0;
		txvq->stats.bytes = 0;
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

	return 0;
}

static void
virtio_set_hwaddr(struct virtio_hw *hw)
{
	vtpci_write_dev_config(hw,
			offsetof(struct virtio_net_config, mac),
			&hw->mac_addr, RTE_ETHER_ADDR_LEN);
}

static void
virtio_get_hwaddr(struct virtio_hw *hw)
{
	if (vtpci_with_feature(hw, VIRTIO_NET_F_MAC)) {
		vtpci_read_dev_config(hw,
			offsetof(struct virtio_net_config, mac),
			&hw->mac_addr, RTE_ETHER_ADDR_LEN);
	} else {
		rte_eth_random_addr(&hw->mac_addr[0]);
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

	len[0] = uc->entries * RTE_ETHER_ADDR_LEN + sizeof(uc->entries);
	memcpy(ctrl.data, uc, len[0]);

	len[1] = mc->entries * RTE_ETHER_ADDR_LEN + sizeof(mc->entries);
	memcpy(ctrl.data + len[0], mc, len[1]);

	err = virtio_send_command(hw->cvq, &ctrl, len, 2);
	if (err != 0)
		PMD_DRV_LOG(NOTICE, "mac table set failed: %d", err);
	return err;
}

static int
virtio_mac_addr_add(struct rte_eth_dev *dev, struct rte_ether_addr *mac_addr,
		    uint32_t index, uint32_t vmdq __rte_unused)
{
	struct virtio_hw *hw = dev->data->dev_private;
	const struct rte_ether_addr *addrs = dev->data->mac_addrs;
	unsigned int i;
	struct virtio_net_ctrl_mac *uc, *mc;

	if (index >= VIRTIO_MAX_MAC_ADDRS) {
		PMD_DRV_LOG(ERR, "mac address index %u out of range", index);
		return -EINVAL;
	}

	uc = alloca(VIRTIO_MAX_MAC_ADDRS * RTE_ETHER_ADDR_LEN +
		sizeof(uc->entries));
	uc->entries = 0;
	mc = alloca(VIRTIO_MAX_MAC_ADDRS * RTE_ETHER_ADDR_LEN +
		sizeof(mc->entries));
	mc->entries = 0;

	for (i = 0; i < VIRTIO_MAX_MAC_ADDRS; i++) {
		const struct rte_ether_addr *addr
			= (i == index) ? mac_addr : addrs + i;
		struct virtio_net_ctrl_mac *tbl
			= rte_is_multicast_ether_addr(addr) ? mc : uc;

		memcpy(&tbl->macs[tbl->entries++], addr, RTE_ETHER_ADDR_LEN);
	}

	return virtio_mac_table_set(hw, uc, mc);
}

static void
virtio_mac_addr_remove(struct rte_eth_dev *dev, uint32_t index)
{
	struct virtio_hw *hw = dev->data->dev_private;
	struct rte_ether_addr *addrs = dev->data->mac_addrs;
	struct virtio_net_ctrl_mac *uc, *mc;
	unsigned int i;

	if (index >= VIRTIO_MAX_MAC_ADDRS) {
		PMD_DRV_LOG(ERR, "mac address index %u out of range", index);
		return;
	}

	uc = alloca(VIRTIO_MAX_MAC_ADDRS * RTE_ETHER_ADDR_LEN +
		sizeof(uc->entries));
	uc->entries = 0;
	mc = alloca(VIRTIO_MAX_MAC_ADDRS * RTE_ETHER_ADDR_LEN +
		sizeof(mc->entries));
	mc->entries = 0;

	for (i = 0; i < VIRTIO_MAX_MAC_ADDRS; i++) {
		struct virtio_net_ctrl_mac *tbl;

		if (i == index || rte_is_zero_ether_addr(addrs + i))
			continue;

		tbl = rte_is_multicast_ether_addr(addrs + i) ? mc : uc;
		memcpy(&tbl->macs[tbl->entries++], addrs + i,
			RTE_ETHER_ADDR_LEN);
	}

	virtio_mac_table_set(hw, uc, mc);
}

static int
virtio_mac_addr_set(struct rte_eth_dev *dev, struct rte_ether_addr *mac_addr)
{
	struct virtio_hw *hw = dev->data->dev_private;

	memcpy(hw->mac_addr, mac_addr, RTE_ETHER_ADDR_LEN);

	/* Use atomic update if available */
	if (vtpci_with_feature(hw, VIRTIO_NET_F_CTRL_MAC_ADDR)) {
		struct virtio_pmd_ctrl ctrl;
		int len = RTE_ETHER_ADDR_LEN;

		ctrl.hdr.class = VIRTIO_NET_CTRL_MAC;
		ctrl.hdr.cmd = VIRTIO_NET_CTRL_MAC_ADDR_SET;

		memcpy(ctrl.data, mac_addr, RTE_ETHER_ADDR_LEN);
		return virtio_send_command(hw->cvq, &ctrl, &len, 1);
	}

	if (!vtpci_with_feature(hw, VIRTIO_NET_F_MAC))
		return -ENOTSUP;

	virtio_set_hwaddr(hw);
	return 0;
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
virtio_intr_unmask(struct rte_eth_dev *dev)
{
	struct virtio_hw *hw = dev->data->dev_private;

	if (rte_intr_ack(dev->intr_handle) < 0)
		return -1;

	if (!hw->virtio_user_dev)
		hw->use_msix = vtpci_msix_detect(RTE_ETH_DEV_TO_PCI(dev));

	return 0;
}

static int
virtio_intr_enable(struct rte_eth_dev *dev)
{
	struct virtio_hw *hw = dev->data->dev_private;

	if (rte_intr_enable(dev->intr_handle) < 0)
		return -1;

	if (!hw->virtio_user_dev)
		hw->use_msix = vtpci_msix_detect(RTE_ETH_DEV_TO_PCI(dev));

	return 0;
}

static int
virtio_intr_disable(struct rte_eth_dev *dev)
{
	struct virtio_hw *hw = dev->data->dev_private;

	if (rte_intr_disable(dev->intr_handle) < 0)
		return -1;

	if (!hw->virtio_user_dev)
		hw->use_msix = vtpci_msix_detect(RTE_ETH_DEV_TO_PCI(dev));

	return 0;
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

		if (config.mtu < RTE_ETHER_MIN_MTU)
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

int
virtio_dev_pause(struct rte_eth_dev *dev)
{
	struct virtio_hw *hw = dev->data->dev_private;

	rte_spinlock_lock(&hw->state_lock);

	if (hw->started == 0) {
		/* Device is just stopped. */
		rte_spinlock_unlock(&hw->state_lock);
		return -1;
	}
	hw->started = 0;
	/*
	 * Prevent the worker threads from touching queues to avoid contention,
	 * 1 ms should be enough for the ongoing Tx function to finish.
	 */
	rte_delay_ms(1);
	return 0;
}

/*
 * Recover hw state to let the worker threads continue.
 */
void
virtio_dev_resume(struct rte_eth_dev *dev)
{
	struct virtio_hw *hw = dev->data->dev_private;

	hw->started = 1;
	rte_spinlock_unlock(&hw->state_lock);
}

/*
 * Should be called only after device is paused.
 */
int
virtio_inject_pkts(struct rte_eth_dev *dev, struct rte_mbuf **tx_pkts,
		int nb_pkts)
{
	struct virtio_hw *hw = dev->data->dev_private;
	struct virtnet_tx *txvq = dev->data->tx_queues[0];
	int ret;

	hw->inject_pkts = tx_pkts;
	ret = dev->tx_pkt_burst(txvq, tx_pkts, nb_pkts);
	hw->inject_pkts = NULL;

	return ret;
}

static void
virtio_notify_peers(struct rte_eth_dev *dev)
{
	struct virtio_hw *hw = dev->data->dev_private;
	struct virtnet_rx *rxvq;
	struct rte_mbuf *rarp_mbuf;

	if (!dev->data->rx_queues)
		return;

	rxvq = dev->data->rx_queues[0];
	if (!rxvq)
		return;

	rarp_mbuf = rte_net_make_rarp_packet(rxvq->mpool,
			(struct rte_ether_addr *)hw->mac_addr);
	if (rarp_mbuf == NULL) {
		PMD_DRV_LOG(ERR, "failed to make RARP packet.");
		return;
	}

	/* If virtio port just stopped, no need to send RARP */
	if (virtio_dev_pause(dev) < 0) {
		rte_pktmbuf_free(rarp_mbuf);
		return;
	}

	virtio_inject_pkts(dev, &rarp_mbuf, 1);
	virtio_dev_resume(dev);
}

static void
virtio_ack_link_announce(struct rte_eth_dev *dev)
{
	struct virtio_hw *hw = dev->data->dev_private;
	struct virtio_pmd_ctrl ctrl;

	ctrl.hdr.class = VIRTIO_NET_CTRL_ANNOUNCE;
	ctrl.hdr.cmd = VIRTIO_NET_CTRL_ANNOUNCE_ACK;

	virtio_send_command(hw->cvq, &ctrl, NULL, 0);
}

/*
 * Process virtio config changed interrupt. Call the callback
 * if link state changed, generate gratuitous RARP packet if
 * the status indicates an ANNOUNCE.
 */
void
virtio_interrupt_handler(void *param)
{
	struct rte_eth_dev *dev = param;
	struct virtio_hw *hw = dev->data->dev_private;
	uint8_t isr;
	uint16_t status;

	/* Read interrupt status which clears interrupt */
	isr = vtpci_isr(hw);
	PMD_DRV_LOG(INFO, "interrupt status = %#x", isr);

	if (virtio_intr_unmask(dev) < 0)
		PMD_DRV_LOG(ERR, "interrupt enable failed");

	if (isr & VIRTIO_PCI_ISR_CONFIG) {
		if (virtio_dev_link_update(dev, 0) == 0)
			_rte_eth_dev_callback_process(dev,
						      RTE_ETH_EVENT_INTR_LSC,
						      NULL);

		if (vtpci_with_feature(hw, VIRTIO_NET_F_STATUS)) {
			vtpci_read_dev_config(hw,
				offsetof(struct virtio_net_config, status),
				&status, sizeof(status));
			if (status & VIRTIO_NET_S_ANNOUNCE) {
				virtio_notify_peers(dev);
				if (hw->cvq)
					virtio_ack_link_announce(dev);
			}
		}
	}
}

/* set rx and tx handlers according to what is supported */
static void
set_rxtx_funcs(struct rte_eth_dev *eth_dev)
{
	struct virtio_hw *hw = eth_dev->data->dev_private;

	eth_dev->tx_pkt_prepare = virtio_xmit_pkts_prepare;
	if (vtpci_packed_queue(hw)) {
		PMD_INIT_LOG(INFO,
			"virtio: using packed ring %s Tx path on port %u",
			hw->use_vec_tx ? "vectorized" : "standard",
			eth_dev->data->port_id);
		if (hw->use_vec_tx)
			eth_dev->tx_pkt_burst = virtio_xmit_pkts_packed_vec;
		else
			eth_dev->tx_pkt_burst = virtio_xmit_pkts_packed;
	} else {
		if (hw->use_inorder_tx) {
			PMD_INIT_LOG(INFO, "virtio: using inorder Tx path on port %u",
				eth_dev->data->port_id);
			eth_dev->tx_pkt_burst = virtio_xmit_pkts_inorder;
		} else {
			PMD_INIT_LOG(INFO, "virtio: using standard Tx path on port %u",
				eth_dev->data->port_id);
			eth_dev->tx_pkt_burst = virtio_xmit_pkts;
		}
	}

	if (vtpci_packed_queue(hw)) {
		if (hw->use_vec_rx) {
			PMD_INIT_LOG(INFO,
				"virtio: using packed ring vectorized Rx path on port %u",
				eth_dev->data->port_id);
			eth_dev->rx_pkt_burst =
				&virtio_recv_pkts_packed_vec;
		} else if (vtpci_with_feature(hw, VIRTIO_NET_F_MRG_RXBUF)) {
			PMD_INIT_LOG(INFO,
				"virtio: using packed ring mergeable buffer Rx path on port %u",
				eth_dev->data->port_id);
			eth_dev->rx_pkt_burst =
				&virtio_recv_mergeable_pkts_packed;
		} else {
			PMD_INIT_LOG(INFO,
				"virtio: using packed ring standard Rx path on port %u",
				eth_dev->data->port_id);
			eth_dev->rx_pkt_burst = &virtio_recv_pkts_packed;
		}
	} else {
		if (hw->use_vec_rx) {
			PMD_INIT_LOG(INFO, "virtio: using vectorized Rx path on port %u",
				eth_dev->data->port_id);
			eth_dev->rx_pkt_burst = virtio_recv_pkts_vec;
		} else if (hw->use_inorder_rx) {
			PMD_INIT_LOG(INFO,
				"virtio: using inorder Rx path on port %u",
				eth_dev->data->port_id);
			eth_dev->rx_pkt_burst =	&virtio_recv_pkts_inorder;
		} else if (vtpci_with_feature(hw, VIRTIO_NET_F_MRG_RXBUF)) {
			PMD_INIT_LOG(INFO,
				"virtio: using mergeable buffer Rx path on port %u",
				eth_dev->data->port_id);
			eth_dev->rx_pkt_burst = &virtio_recv_mergeable_pkts;
		} else {
			PMD_INIT_LOG(INFO, "virtio: using standard Rx path on port %u",
				eth_dev->data->port_id);
			eth_dev->rx_pkt_burst = &virtio_recv_pkts;
		}
	}

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
	if (virtio_intr_enable(dev) < 0) {
		PMD_DRV_LOG(ERR, "interrupt enable failed");
		return -1;
	}

	if (virtio_queues_bind_intr(dev) < 0) {
		PMD_INIT_LOG(ERR, "Failed to bind queue/interrupt");
		return -1;
	}

	return 0;
}
#define SPEED_UNKNOWN    0xffffffff
#define DUPLEX_UNKNOWN   0xff
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

	if (hw->vqs) {
		virtio_dev_free_mbufs(eth_dev);
		virtio_free_queues(hw);
	}

	/* Tell the host we've noticed this device. */
	vtpci_set_status(hw, VIRTIO_CONFIG_STATUS_ACK);

	/* Tell the host we've known how to drive the device. */
	vtpci_set_status(hw, VIRTIO_CONFIG_STATUS_DRIVER);
	if (virtio_negotiate_features(hw, req_features) < 0)
		return -1;

	hw->weak_barriers = !vtpci_with_feature(hw, VIRTIO_F_ORDER_PLATFORM);

	if (!hw->virtio_user_dev)
		pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);

	/* If host does not support both status and MSI-X then disable LSC */
	if (vtpci_with_feature(hw, VIRTIO_NET_F_STATUS) &&
	    hw->use_msix != VIRTIO_MSIX_NONE)
		eth_dev->data->dev_flags |= RTE_ETH_DEV_INTR_LSC;
	else
		eth_dev->data->dev_flags &= ~RTE_ETH_DEV_INTR_LSC;

	/* Setting up rx_header size for the device */
	if (vtpci_with_feature(hw, VIRTIO_NET_F_MRG_RXBUF) ||
	    vtpci_with_feature(hw, VIRTIO_F_VERSION_1) ||
	    vtpci_with_feature(hw, VIRTIO_F_RING_PACKED))
		hw->vtnet_hdr_size = sizeof(struct virtio_net_hdr_mrg_rxbuf);
	else
		hw->vtnet_hdr_size = sizeof(struct virtio_net_hdr);

	/* Copy the permanent MAC address to: virtio_hw */
	virtio_get_hwaddr(hw);
	rte_ether_addr_copy((struct rte_ether_addr *)hw->mac_addr,
			&eth_dev->data->mac_addrs[0]);
	PMD_INIT_LOG(DEBUG,
		     "PORT MAC: %02X:%02X:%02X:%02X:%02X:%02X",
		     hw->mac_addr[0], hw->mac_addr[1], hw->mac_addr[2],
		     hw->mac_addr[3], hw->mac_addr[4], hw->mac_addr[5]);

	if (hw->speed == SPEED_UNKNOWN) {
		if (vtpci_with_feature(hw, VIRTIO_NET_F_SPEED_DUPLEX)) {
			config = &local_config;
			vtpci_read_dev_config(hw,
				offsetof(struct virtio_net_config, speed),
				&config->speed, sizeof(config->speed));
			vtpci_read_dev_config(hw,
				offsetof(struct virtio_net_config, duplex),
				&config->duplex, sizeof(config->duplex));
			hw->speed = config->speed;
			hw->duplex = config->duplex;
		}
	}
	if (hw->speed == SPEED_UNKNOWN)
		hw->speed = ETH_SPEED_NUM_10G;
	if (hw->duplex == DUPLEX_UNKNOWN)
		hw->duplex = ETH_LINK_FULL_DUPLEX;
	PMD_INIT_LOG(DEBUG, "link speed = %d, duplex = %d",
		hw->speed, hw->duplex);
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
			if (config->mtu < RTE_ETHER_MIN_MTU) {
				PMD_INIT_LOG(ERR, "invalid max MTU value (%u)",
						config->mtu);
				return -1;
			}

			hw->max_mtu = config->mtu;
			/* Set initial MTU to maximum one supported by vhost */
			eth_dev->data->mtu = config->mtu;

		} else {
			hw->max_mtu = VIRTIO_MAX_RX_PKTLEN - RTE_ETHER_HDR_LEN -
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
		hw->max_mtu = VIRTIO_MAX_RX_PKTLEN - RTE_ETHER_HDR_LEN -
			VLAN_TAG_LEN - hw->vtnet_hdr_size;
	}

	ret = virtio_alloc_queues(eth_dev);
	if (ret < 0)
		return ret;

	if (eth_dev->data->dev_conf.intr_conf.rxq) {
		if (virtio_configure_intr(eth_dev) < 0) {
			PMD_INIT_LOG(ERR, "failed to configure interrupt");
			virtio_free_queues(hw);
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
	uint32_t speed = SPEED_UNKNOWN;
	int vectorized = 0;
	int ret;

	if (sizeof(struct virtio_net_hdr_mrg_rxbuf) > RTE_PKTMBUF_HEADROOM) {
		PMD_INIT_LOG(ERR,
			"Not sufficient headroom required = %d, avail = %d",
			(int)sizeof(struct virtio_net_hdr_mrg_rxbuf),
			RTE_PKTMBUF_HEADROOM);

		return -1;
	}

	eth_dev->dev_ops = &virtio_eth_dev_ops;

	if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		if (!hw->virtio_user_dev) {
			ret = virtio_remap_pci(RTE_ETH_DEV_TO_PCI(eth_dev), hw);
			if (ret)
				return ret;
		}

		virtio_set_vtpci_ops(hw);
		set_rxtx_funcs(eth_dev);

		return 0;
	}
	ret = virtio_dev_devargs_parse(eth_dev->device->devargs,
		 NULL, &speed, &vectorized);
	if (ret < 0)
		return ret;
	hw->speed = speed;
	/*
	 * Pass the information to the rte_eth_dev_close() that it should also
	 * release the private port resources.
	 */
	eth_dev->data->dev_flags |= RTE_ETH_DEV_CLOSE_REMOVE;

	/* Allocate memory for storing MAC addresses */
	eth_dev->data->mac_addrs = rte_zmalloc("virtio",
				VIRTIO_MAX_MAC_ADDRS * RTE_ETHER_ADDR_LEN, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR,
			"Failed to allocate %d bytes needed to store MAC addresses",
			VIRTIO_MAX_MAC_ADDRS * RTE_ETHER_ADDR_LEN);
		return -ENOMEM;
	}

	hw->port_id = eth_dev->data->port_id;
	/* For virtio_user case the hw->virtio_user_dev is populated by
	 * virtio_user_eth_dev_alloc() before eth_virtio_dev_init() is called.
	 */
	if (!hw->virtio_user_dev) {
		ret = vtpci_init(RTE_ETH_DEV_TO_PCI(eth_dev), hw);
		if (ret)
			goto err_vtpci_init;
	}

	rte_spinlock_init(&hw->state_lock);

	/* reset device and negotiate default features */
	ret = virtio_init_device(eth_dev, VIRTIO_PMD_DEFAULT_GUEST_FEATURES);
	if (ret < 0)
		goto err_virtio_init;

	if (vectorized) {
		if (!vtpci_packed_queue(hw)) {
			hw->use_vec_rx = 1;
		} else {
#if !defined(CC_AVX512_SUPPORT)
			PMD_DRV_LOG(INFO,
				"building environment do not support packed ring vectorized");
#else
			hw->use_vec_rx = 1;
			hw->use_vec_tx = 1;
#endif
		}
	}

	hw->opened = true;

	return 0;

err_virtio_init:
	if (!hw->virtio_user_dev) {
		rte_pci_unmap_device(RTE_ETH_DEV_TO_PCI(eth_dev));
		if (!hw->modern)
			rte_pci_ioport_unmap(VTPCI_IO(hw));
	}
err_vtpci_init:
	rte_free(eth_dev->data->mac_addrs);
	eth_dev->data->mac_addrs = NULL;
	return ret;
}

static int
eth_virtio_dev_uninit(struct rte_eth_dev *eth_dev)
{
	PMD_INIT_FUNC_TRACE();

	if (rte_eal_process_type() == RTE_PROC_SECONDARY)
		return 0;

	virtio_dev_stop(eth_dev);
	virtio_dev_close(eth_dev);

	eth_dev->dev_ops = NULL;
	eth_dev->tx_pkt_burst = NULL;
	eth_dev->rx_pkt_burst = NULL;

	PMD_INIT_LOG(DEBUG, "dev_uninit completed");

	return 0;
}


static int vdpa_check_handler(__rte_unused const char *key,
		const char *value, void *ret_val)
{
	if (strcmp(value, "1") == 0)
		*(int *)ret_val = 1;
	else
		*(int *)ret_val = 0;

	return 0;
}


static uint32_t
virtio_dev_speed_capa_get(uint32_t speed)
{
	switch (speed) {
	case ETH_SPEED_NUM_10G:
		return ETH_LINK_SPEED_10G;
	case ETH_SPEED_NUM_20G:
		return ETH_LINK_SPEED_20G;
	case ETH_SPEED_NUM_25G:
		return ETH_LINK_SPEED_25G;
	case ETH_SPEED_NUM_40G:
		return ETH_LINK_SPEED_40G;
	case ETH_SPEED_NUM_50G:
		return ETH_LINK_SPEED_50G;
	case ETH_SPEED_NUM_56G:
		return ETH_LINK_SPEED_56G;
	case ETH_SPEED_NUM_100G:
		return ETH_LINK_SPEED_100G;
	default:
		return 0;
	}
}

static int vectorized_check_handler(__rte_unused const char *key,
		const char *value, void *ret_val)
{
	if (strcmp(value, "1") == 0)
		*(int *)ret_val = 1;
	else
		*(int *)ret_val = 0;

	return 0;
}

#define VIRTIO_ARG_SPEED      "speed"
#define VIRTIO_ARG_VDPA       "vdpa"
#define VIRTIO_ARG_VECTORIZED "vectorized"


static int
link_speed_handler(const char *key __rte_unused,
		const char *value, void *ret_val)
{
	uint32_t val;
	if (!value || !ret_val)
		return -EINVAL;
	val = strtoul(value, NULL, 0);
	/* validate input */
	if (virtio_dev_speed_capa_get(val) == 0)
		return -EINVAL;
	*(uint32_t *)ret_val = val;

	return 0;
}


static int
virtio_dev_devargs_parse(struct rte_devargs *devargs, int *vdpa,
	uint32_t *speed, int *vectorized)
{
	struct rte_kvargs *kvlist;
	int ret = 0;

	if (devargs == NULL)
		return 0;

	kvlist = rte_kvargs_parse(devargs->args, NULL);
	if (kvlist == NULL) {
		PMD_INIT_LOG(ERR, "error when parsing param");
		return 0;
	}
	if (vdpa && rte_kvargs_count(kvlist, VIRTIO_ARG_VDPA) == 1) {
		/* vdpa mode selected when there's a key-value pair:
		 * vdpa=1
		 */
		ret = rte_kvargs_process(kvlist, VIRTIO_ARG_VDPA,
				vdpa_check_handler, vdpa);
		if (ret < 0) {
			PMD_INIT_LOG(ERR, "Failed to parse %s",
				VIRTIO_ARG_VDPA);
			goto exit;
		}
	}
	if (speed && rte_kvargs_count(kvlist, VIRTIO_ARG_SPEED) == 1) {
		ret = rte_kvargs_process(kvlist,
					VIRTIO_ARG_SPEED,
					link_speed_handler, speed);
		if (ret < 0) {
			PMD_INIT_LOG(ERR, "Failed to parse %s",
					VIRTIO_ARG_SPEED);
			goto exit;
		}
	}

	if (vectorized &&
		rte_kvargs_count(kvlist, VIRTIO_ARG_VECTORIZED) == 1) {
		ret = rte_kvargs_process(kvlist,
				VIRTIO_ARG_VECTORIZED,
				vectorized_check_handler, vectorized);
		if (ret < 0) {
			PMD_INIT_LOG(ERR, "Failed to parse %s",
					VIRTIO_ARG_VECTORIZED);
			goto exit;
		}
	}

exit:
	rte_kvargs_free(kvlist);
	return ret;
}

static int eth_virtio_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	struct rte_pci_device *pci_dev)
{
	int vdpa = 0;
	int ret = 0;

	ret = virtio_dev_devargs_parse(pci_dev->device.devargs, &vdpa, NULL,
		NULL);
	if (ret < 0) {
		PMD_INIT_LOG(ERR, "devargs parsing is failed");
		return ret;
	}
	/* virtio pmd skips probe if device needs to work in vdpa mode */
	if (vdpa == 1)
		return 1;

	return rte_eth_dev_pci_generic_probe(pci_dev, sizeof(struct virtio_hw),
		eth_virtio_dev_init);
}

static int eth_virtio_pci_remove(struct rte_pci_device *pci_dev)
{
	int ret;

	ret = rte_eth_dev_pci_generic_remove(pci_dev, eth_virtio_dev_uninit);
	/* Port has already been released by close. */
	if (ret == -ENODEV)
		ret = 0;
	return ret;
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

RTE_INIT(rte_virtio_pmd_init)
{
	rte_eal_iopl_init();
	rte_pci_register(&rte_virtio_pmd);
}

static bool
rx_offload_enabled(struct virtio_hw *hw)
{
	return vtpci_with_feature(hw, VIRTIO_NET_F_GUEST_CSUM) ||
		vtpci_with_feature(hw, VIRTIO_NET_F_GUEST_TSO4) ||
		vtpci_with_feature(hw, VIRTIO_NET_F_GUEST_TSO6);
}

static bool
tx_offload_enabled(struct virtio_hw *hw)
{
	return vtpci_with_feature(hw, VIRTIO_NET_F_CSUM) ||
		vtpci_with_feature(hw, VIRTIO_NET_F_HOST_TSO4) ||
		vtpci_with_feature(hw, VIRTIO_NET_F_HOST_TSO6);
}

/*
 * Configure virtio device
 * It returns 0 on success.
 */
static int
virtio_dev_configure(struct rte_eth_dev *dev)
{
	const struct rte_eth_rxmode *rxmode = &dev->data->dev_conf.rxmode;
	const struct rte_eth_txmode *txmode = &dev->data->dev_conf.txmode;
	struct virtio_hw *hw = dev->data->dev_private;
	uint32_t ether_hdr_len = RTE_ETHER_HDR_LEN + VLAN_TAG_LEN +
		hw->vtnet_hdr_size;
	uint64_t rx_offloads = rxmode->offloads;
	uint64_t tx_offloads = txmode->offloads;
	uint64_t req_features;
	int ret;

	PMD_INIT_LOG(DEBUG, "configure");
	req_features = VIRTIO_PMD_DEFAULT_GUEST_FEATURES;

	if (rxmode->mq_mode != ETH_MQ_RX_NONE) {
		PMD_DRV_LOG(ERR,
			"Unsupported Rx multi queue mode %d",
			rxmode->mq_mode);
		return -EINVAL;
	}

	if (txmode->mq_mode != ETH_MQ_TX_NONE) {
		PMD_DRV_LOG(ERR,
			"Unsupported Tx multi queue mode %d",
			txmode->mq_mode);
		return -EINVAL;
	}

	if (dev->data->dev_conf.intr_conf.rxq) {
		ret = virtio_init_device(dev, hw->req_guest_features);
		if (ret < 0)
			return ret;
	}

	if (rxmode->max_rx_pkt_len > hw->max_mtu + ether_hdr_len)
		req_features &= ~(1ULL << VIRTIO_NET_F_MTU);

	if (rx_offloads & (DEV_RX_OFFLOAD_UDP_CKSUM |
			   DEV_RX_OFFLOAD_TCP_CKSUM))
		req_features |= (1ULL << VIRTIO_NET_F_GUEST_CSUM);

	if (rx_offloads & DEV_RX_OFFLOAD_TCP_LRO)
		req_features |=
			(1ULL << VIRTIO_NET_F_GUEST_TSO4) |
			(1ULL << VIRTIO_NET_F_GUEST_TSO6);

	if (tx_offloads & (DEV_TX_OFFLOAD_UDP_CKSUM |
			   DEV_TX_OFFLOAD_TCP_CKSUM))
		req_features |= (1ULL << VIRTIO_NET_F_CSUM);

	if (tx_offloads & DEV_TX_OFFLOAD_TCP_TSO)
		req_features |=
			(1ULL << VIRTIO_NET_F_HOST_TSO4) |
			(1ULL << VIRTIO_NET_F_HOST_TSO6);

	/* if request features changed, reinit the device */
	if (req_features != hw->req_guest_features) {
		ret = virtio_init_device(dev, req_features);
		if (ret < 0)
			return ret;
	}

	if ((rx_offloads & (DEV_RX_OFFLOAD_UDP_CKSUM |
			    DEV_RX_OFFLOAD_TCP_CKSUM)) &&
		!vtpci_with_feature(hw, VIRTIO_NET_F_GUEST_CSUM)) {
		PMD_DRV_LOG(ERR,
			"rx checksum not available on this host");
		return -ENOTSUP;
	}

	if ((rx_offloads & DEV_RX_OFFLOAD_TCP_LRO) &&
		(!vtpci_with_feature(hw, VIRTIO_NET_F_GUEST_TSO4) ||
		 !vtpci_with_feature(hw, VIRTIO_NET_F_GUEST_TSO6))) {
		PMD_DRV_LOG(ERR,
			"Large Receive Offload not available on this host");
		return -ENOTSUP;
	}

	/* start control queue */
	if (vtpci_with_feature(hw, VIRTIO_NET_F_CTRL_VQ))
		virtio_dev_cq_start(dev);

	if (rx_offloads & DEV_RX_OFFLOAD_VLAN_STRIP)
		hw->vlan_strip = 1;

	if ((rx_offloads & DEV_RX_OFFLOAD_VLAN_FILTER)
	    && !vtpci_with_feature(hw, VIRTIO_NET_F_CTRL_VLAN)) {
		PMD_DRV_LOG(ERR,
			    "vlan filtering not available on this host");
		return -ENOTSUP;
	}

	hw->has_tx_offload = tx_offload_enabled(hw);
	hw->has_rx_offload = rx_offload_enabled(hw);

	if (dev->data->dev_flags & RTE_ETH_DEV_INTR_LSC)
		/* Enable vector (0) for Link State Intrerrupt */
		if (VTPCI_OPS(hw)->set_config_irq(hw, 0) ==
				VIRTIO_MSI_NO_VECTOR) {
			PMD_DRV_LOG(ERR, "failed to set config vector");
			return -EBUSY;
		}

	if (vtpci_packed_queue(hw)) {
#if defined(RTE_ARCH_X86_64) && defined(CC_AVX512_SUPPORT)
		if ((hw->use_vec_rx || hw->use_vec_tx) &&
		    (!rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX512F) ||
		     !vtpci_with_feature(hw, VIRTIO_F_IN_ORDER) ||
		     !vtpci_with_feature(hw, VIRTIO_F_VERSION_1))) {
			PMD_DRV_LOG(INFO,
				"disabled packed ring vectorized path for requirements not met");
			hw->use_vec_rx = 0;
			hw->use_vec_tx = 0;
		}
#else
		hw->use_vec_rx = 0;
		hw->use_vec_tx = 0;
#endif

		if (hw->use_vec_rx) {
			if (vtpci_with_feature(hw, VIRTIO_NET_F_MRG_RXBUF)) {
				PMD_DRV_LOG(INFO,
					"disabled packed ring vectorized rx for mrg_rxbuf enabled");
				hw->use_vec_rx = 0;
			}

			if (rx_offloads & DEV_RX_OFFLOAD_TCP_LRO) {
				PMD_DRV_LOG(INFO,
					"disabled packed ring vectorized rx for TCP_LRO enabled");
				hw->use_vec_rx = 0;
			}
		}
	} else {
		if (vtpci_with_feature(hw, VIRTIO_F_IN_ORDER)) {
			hw->use_inorder_tx = 1;
			hw->use_inorder_rx = 1;
			hw->use_vec_rx = 0;
		}

		if (hw->use_vec_rx) {
#if defined RTE_ARCH_ARM64 || defined RTE_ARCH_ARM
			if (!rte_cpu_get_flag_enabled(RTE_CPUFLAG_NEON)) {
				PMD_DRV_LOG(INFO,
					"disabled split ring vectorized path for requirement not met");
				hw->use_vec_rx = 0;
			}
#endif
			if (vtpci_with_feature(hw, VIRTIO_NET_F_MRG_RXBUF)) {
				PMD_DRV_LOG(INFO,
					"disabled split ring vectorized rx for mrg_rxbuf enabled");
				hw->use_vec_rx = 0;
			}

			if (rx_offloads & (DEV_RX_OFFLOAD_UDP_CKSUM |
					   DEV_RX_OFFLOAD_TCP_CKSUM |
					   DEV_RX_OFFLOAD_TCP_LRO |
					   DEV_RX_OFFLOAD_VLAN_STRIP)) {
				PMD_DRV_LOG(INFO,
					"disabled split ring vectorized rx for offloading enabled");
				hw->use_vec_rx = 0;
			}
		}
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
	int ret;

	/* Finish the initialization of the queues */
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		ret = virtio_dev_rx_queue_setup_finish(dev, i);
		if (ret < 0)
			return ret;
	}
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		ret = virtio_dev_tx_queue_setup_finish(dev, i);
		if (ret < 0)
			return ret;
	}

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
		virtio_intr_disable(dev);

		/* Setup interrupt callback  */
		if (dev->data->dev_flags & RTE_ETH_DEV_INTR_LSC)
			rte_intr_callback_register(dev->intr_handle,
						   virtio_interrupt_handler,
						   dev);

		if (virtio_intr_enable(dev) < 0) {
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
		/* Flush the old packets */
		virtqueue_rxvq_flush(rxvq->vq);
		virtqueue_notify(rxvq->vq);
	}

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txvq = dev->data->tx_queues[i];
		virtqueue_notify(txvq->vq);
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

	set_rxtx_funcs(dev);
	hw->started = true;

	/* Initialize Link state */
	virtio_dev_link_update(dev, 0);

	return 0;
}

static void virtio_dev_free_mbufs(struct rte_eth_dev *dev)
{
	struct virtio_hw *hw = dev->data->dev_private;
	uint16_t nr_vq = virtio_get_nr_vq(hw);
	const char *type __rte_unused;
	unsigned int i, mbuf_num = 0;
	struct virtqueue *vq;
	struct rte_mbuf *buf;
	int queue_type;

	if (hw->vqs == NULL)
		return;

	for (i = 0; i < nr_vq; i++) {
		vq = hw->vqs[i];
		if (!vq)
			continue;

		queue_type = virtio_get_queue_type(hw, i);
		if (queue_type == VTNET_RQ)
			type = "rxq";
		else if (queue_type == VTNET_TQ)
			type = "txq";
		else
			continue;

		PMD_INIT_LOG(DEBUG,
			"Before freeing %s[%d] used and unused buf",
			type, i);
		VIRTQUEUE_DUMP(vq);

		while ((buf = virtqueue_detach_unused(vq)) != NULL) {
			rte_pktmbuf_free(buf);
			mbuf_num++;
		}

		PMD_INIT_LOG(DEBUG,
			"After freeing %s[%d] used and unused buf",
			type, i);
		VIRTQUEUE_DUMP(vq);
	}

	PMD_INIT_LOG(DEBUG, "%d mbufs freed", mbuf_num);
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

	rte_spinlock_lock(&hw->state_lock);
	if (!hw->started)
		goto out_unlock;
	hw->started = false;

	if (intr_conf->lsc || intr_conf->rxq) {
		virtio_intr_disable(dev);

		/* Reset interrupt callback  */
		if (dev->data->dev_flags & RTE_ETH_DEV_INTR_LSC) {
			rte_intr_callback_unregister(dev->intr_handle,
						     virtio_interrupt_handler,
						     dev);
		}
	}

	memset(&link, 0, sizeof(link));
	rte_eth_linkstatus_set(dev, &link);
out_unlock:
	rte_spinlock_unlock(&hw->state_lock);
}

static int
virtio_dev_link_update(struct rte_eth_dev *dev, __rte_unused int wait_to_complete)
{
	struct rte_eth_link link;
	uint16_t status;
	struct virtio_hw *hw = dev->data->dev_private;

	memset(&link, 0, sizeof(link));
	link.link_duplex = hw->duplex;
	link.link_speed  = hw->speed;
	link.link_autoneg = ETH_LINK_AUTONEG;

	if (!hw->started) {
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

	return rte_eth_linkstatus_set(dev, &link);
}

static int
virtio_dev_vlan_offload_set(struct rte_eth_dev *dev, int mask)
{
	const struct rte_eth_rxmode *rxmode = &dev->data->dev_conf.rxmode;
	struct virtio_hw *hw = dev->data->dev_private;
	uint64_t offloads = rxmode->offloads;

	if (mask & ETH_VLAN_FILTER_MASK) {
		if ((offloads & DEV_RX_OFFLOAD_VLAN_FILTER) &&
				!vtpci_with_feature(hw, VIRTIO_NET_F_CTRL_VLAN)) {

			PMD_DRV_LOG(NOTICE,
				"vlan filtering not available on this host");

			return -ENOTSUP;
		}
	}

	if (mask & ETH_VLAN_STRIP_MASK)
		hw->vlan_strip = !!(offloads & DEV_RX_OFFLOAD_VLAN_STRIP);

	return 0;
}

static int
virtio_dev_info_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	uint64_t tso_mask, host_features;
	struct virtio_hw *hw = dev->data->dev_private;
	dev_info->speed_capa = virtio_dev_speed_capa_get(hw->speed);

	dev_info->max_rx_queues =
		RTE_MIN(hw->max_queue_pairs, VIRTIO_MAX_RX_QUEUES);
	dev_info->max_tx_queues =
		RTE_MIN(hw->max_queue_pairs, VIRTIO_MAX_TX_QUEUES);
	dev_info->min_rx_bufsize = VIRTIO_MIN_RX_BUFSIZE;
	dev_info->max_rx_pktlen = VIRTIO_MAX_RX_PKTLEN;
	dev_info->max_mac_addrs = VIRTIO_MAX_MAC_ADDRS;

	host_features = VTPCI_OPS(hw)->get_features(hw);
	dev_info->rx_offload_capa = DEV_RX_OFFLOAD_VLAN_STRIP;
	dev_info->rx_offload_capa |= DEV_RX_OFFLOAD_JUMBO_FRAME;
	if (host_features & (1ULL << VIRTIO_NET_F_GUEST_CSUM)) {
		dev_info->rx_offload_capa |=
			DEV_RX_OFFLOAD_TCP_CKSUM |
			DEV_RX_OFFLOAD_UDP_CKSUM;
	}
	if (host_features & (1ULL << VIRTIO_NET_F_CTRL_VLAN))
		dev_info->rx_offload_capa |= DEV_RX_OFFLOAD_VLAN_FILTER;
	tso_mask = (1ULL << VIRTIO_NET_F_GUEST_TSO4) |
		(1ULL << VIRTIO_NET_F_GUEST_TSO6);
	if ((host_features & tso_mask) == tso_mask)
		dev_info->rx_offload_capa |= DEV_RX_OFFLOAD_TCP_LRO;

	dev_info->tx_offload_capa = DEV_TX_OFFLOAD_MULTI_SEGS |
				    DEV_TX_OFFLOAD_VLAN_INSERT;
	if (host_features & (1ULL << VIRTIO_NET_F_CSUM)) {
		dev_info->tx_offload_capa |=
			DEV_TX_OFFLOAD_UDP_CKSUM |
			DEV_TX_OFFLOAD_TCP_CKSUM;
	}
	tso_mask = (1ULL << VIRTIO_NET_F_HOST_TSO4) |
		(1ULL << VIRTIO_NET_F_HOST_TSO6);
	if ((host_features & tso_mask) == tso_mask)
		dev_info->tx_offload_capa |= DEV_TX_OFFLOAD_TCP_TSO;

	return 0;
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
RTE_LOG_REGISTER(virtio_logtype_init, pmd.net.virtio.init, NOTICE);
RTE_LOG_REGISTER(virtio_logtype_driver, pmd.net.virtio.driver, NOTICE);
