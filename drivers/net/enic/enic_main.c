/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2017 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 */

#include <stdio.h>

#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <rte_memzone.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_string_fns.h>
#include <rte_ethdev_driver.h>

#include "enic_compat.h"
#include "enic.h"
#include "wq_enet_desc.h"
#include "rq_enet_desc.h"
#include "cq_enet_desc.h"
#include "vnic_enet.h"
#include "vnic_dev.h"
#include "vnic_wq.h"
#include "vnic_rq.h"
#include "vnic_cq.h"
#include "vnic_intr.h"
#include "vnic_nic.h"

static inline int enic_is_sriov_vf(struct enic *enic)
{
	return enic->pdev->id.device_id == PCI_DEVICE_ID_CISCO_VIC_ENET_VF;
}

static int is_zero_addr(uint8_t *addr)
{
	return !(addr[0] |  addr[1] | addr[2] | addr[3] | addr[4] | addr[5]);
}

static int is_mcast_addr(uint8_t *addr)
{
	return addr[0] & 1;
}

static int is_eth_addr_valid(uint8_t *addr)
{
	return !is_mcast_addr(addr) && !is_zero_addr(addr);
}

static void
enic_rxmbuf_queue_release(__rte_unused struct enic *enic, struct vnic_rq *rq)
{
	uint16_t i;

	if (!rq || !rq->mbuf_ring) {
		dev_debug(enic, "Pointer to rq or mbuf_ring is NULL");
		return;
	}

	for (i = 0; i < rq->ring.desc_count; i++) {
		if (rq->mbuf_ring[i]) {
			rte_pktmbuf_free_seg(rq->mbuf_ring[i]);
			rq->mbuf_ring[i] = NULL;
		}
	}
}

static void enic_free_wq_buf(struct rte_mbuf **buf)
{
	struct rte_mbuf *mbuf = *buf;

	rte_pktmbuf_free_seg(mbuf);
	*buf = NULL;
}

static void enic_log_q_error(struct enic *enic)
{
	unsigned int i;
	u32 error_status;

	for (i = 0; i < enic->wq_count; i++) {
		error_status = vnic_wq_error_status(&enic->wq[i]);
		if (error_status)
			dev_err(enic, "WQ[%d] error_status %d\n", i,
				error_status);
	}

	for (i = 0; i < enic_vnic_rq_count(enic); i++) {
		if (!enic->rq[i].in_use)
			continue;
		error_status = vnic_rq_error_status(&enic->rq[i]);
		if (error_status)
			dev_err(enic, "RQ[%d] error_status %d\n", i,
				error_status);
	}
}

static void enic_clear_soft_stats(struct enic *enic)
{
	struct enic_soft_stats *soft_stats = &enic->soft_stats;
	rte_atomic64_clear(&soft_stats->rx_nombuf);
	rte_atomic64_clear(&soft_stats->rx_packet_errors);
	rte_atomic64_clear(&soft_stats->tx_oversized);
}

static void enic_init_soft_stats(struct enic *enic)
{
	struct enic_soft_stats *soft_stats = &enic->soft_stats;
	rte_atomic64_init(&soft_stats->rx_nombuf);
	rte_atomic64_init(&soft_stats->rx_packet_errors);
	rte_atomic64_init(&soft_stats->tx_oversized);
	enic_clear_soft_stats(enic);
}

void enic_dev_stats_clear(struct enic *enic)
{
	if (vnic_dev_stats_clear(enic->vdev))
		dev_err(enic, "Error in clearing stats\n");
	enic_clear_soft_stats(enic);
}

int enic_dev_stats_get(struct enic *enic, struct rte_eth_stats *r_stats)
{
	struct vnic_stats *stats;
	struct enic_soft_stats *soft_stats = &enic->soft_stats;
	int64_t rx_truncated;
	uint64_t rx_packet_errors;
	int ret = vnic_dev_stats_dump(enic->vdev, &stats);

	if (ret) {
		dev_err(enic, "Error in getting stats\n");
		return ret;
	}

	/* The number of truncated packets can only be calculated by
	 * subtracting a hardware counter from error packets received by
	 * the driver. Note: this causes transient inaccuracies in the
	 * ipackets count. Also, the length of truncated packets are
	 * counted in ibytes even though truncated packets are dropped
	 * which can make ibytes be slightly higher than it should be.
	 */
	rx_packet_errors = rte_atomic64_read(&soft_stats->rx_packet_errors);
	rx_truncated = rx_packet_errors - stats->rx.rx_errors;

	r_stats->ipackets = stats->rx.rx_frames_ok - rx_truncated;
	r_stats->opackets = stats->tx.tx_frames_ok;

	r_stats->ibytes = stats->rx.rx_bytes_ok;
	r_stats->obytes = stats->tx.tx_bytes_ok;

	r_stats->ierrors = stats->rx.rx_errors + stats->rx.rx_drop;
	r_stats->oerrors = stats->tx.tx_errors
			   + rte_atomic64_read(&soft_stats->tx_oversized);

	r_stats->imissed = stats->rx.rx_no_bufs + rx_truncated;

	r_stats->rx_nombuf = rte_atomic64_read(&soft_stats->rx_nombuf);
	return 0;
}

int enic_del_mac_address(struct enic *enic, int mac_index)
{
	struct rte_eth_dev *eth_dev = enic->rte_dev;
	uint8_t *mac_addr = eth_dev->data->mac_addrs[mac_index].addr_bytes;

	return vnic_dev_del_addr(enic->vdev, mac_addr);
}

int enic_set_mac_address(struct enic *enic, uint8_t *mac_addr)
{
	int err;

	if (!is_eth_addr_valid(mac_addr)) {
		dev_err(enic, "invalid mac address\n");
		return -EINVAL;
	}

	err = vnic_dev_add_addr(enic->vdev, mac_addr);
	if (err)
		dev_err(enic, "add mac addr failed\n");
	return err;
}

static void
enic_free_rq_buf(struct rte_mbuf **mbuf)
{
	if (*mbuf == NULL)
		return;

	rte_pktmbuf_free(*mbuf);
	*mbuf = NULL;
}

void enic_init_vnic_resources(struct enic *enic)
{
	unsigned int error_interrupt_enable = 1;
	unsigned int error_interrupt_offset = 0;
	unsigned int rxq_interrupt_enable = 0;
	unsigned int rxq_interrupt_offset = ENICPMD_RXQ_INTR_OFFSET;
	unsigned int index = 0;
	unsigned int cq_idx;
	struct vnic_rq *data_rq;

	if (enic->rte_dev->data->dev_conf.intr_conf.rxq)
		rxq_interrupt_enable = 1;

	for (index = 0; index < enic->rq_count; index++) {
		cq_idx = enic_cq_rq(enic, enic_rte_rq_idx_to_sop_idx(index));

		vnic_rq_init(&enic->rq[enic_rte_rq_idx_to_sop_idx(index)],
			cq_idx,
			error_interrupt_enable,
			error_interrupt_offset);

		data_rq = &enic->rq[enic_rte_rq_idx_to_data_idx(index)];
		if (data_rq->in_use)
			vnic_rq_init(data_rq,
				     cq_idx,
				     error_interrupt_enable,
				     error_interrupt_offset);

		vnic_cq_init(&enic->cq[cq_idx],
			0 /* flow_control_enable */,
			1 /* color_enable */,
			0 /* cq_head */,
			0 /* cq_tail */,
			1 /* cq_tail_color */,
			rxq_interrupt_enable,
			1 /* cq_entry_enable */,
			0 /* cq_message_enable */,
			rxq_interrupt_offset,
			0 /* cq_message_addr */);
		if (rxq_interrupt_enable)
			rxq_interrupt_offset++;
	}

	for (index = 0; index < enic->wq_count; index++) {
		vnic_wq_init(&enic->wq[index],
			enic_cq_wq(enic, index),
			error_interrupt_enable,
			error_interrupt_offset);
		/* Compute unsupported ol flags for enic_prep_pkts() */
		enic->wq[index].tx_offload_notsup_mask =
			PKT_TX_OFFLOAD_MASK ^ enic->tx_offload_mask;

		cq_idx = enic_cq_wq(enic, index);
		vnic_cq_init(&enic->cq[cq_idx],
			0 /* flow_control_enable */,
			1 /* color_enable */,
			0 /* cq_head */,
			0 /* cq_tail */,
			1 /* cq_tail_color */,
			0 /* interrupt_enable */,
			0 /* cq_entry_enable */,
			1 /* cq_message_enable */,
			0 /* interrupt offset */,
			(u64)enic->wq[index].cqmsg_rz->iova);
	}

	for (index = 0; index < enic->intr_count; index++) {
		vnic_intr_init(&enic->intr[index],
			       enic->config.intr_timer_usec,
			       enic->config.intr_timer_type,
			       /*mask_on_assertion*/1);
	}
}


static int
enic_alloc_rx_queue_mbufs(struct enic *enic, struct vnic_rq *rq)
{
	struct rte_mbuf *mb;
	struct rq_enet_desc *rqd = rq->ring.descs;
	unsigned i;
	dma_addr_t dma_addr;
	uint32_t max_rx_pkt_len;
	uint16_t rq_buf_len;

	if (!rq->in_use)
		return 0;

	dev_debug(enic, "queue %u, allocating %u rx queue mbufs\n", rq->index,
		  rq->ring.desc_count);

	/*
	 * If *not* using scatter and the mbuf size is greater than the
	 * requested max packet size (max_rx_pkt_len), then reduce the
	 * posted buffer size to max_rx_pkt_len. HW still receives packets
	 * larger than max_rx_pkt_len, but they will be truncated, which we
	 * drop in the rx handler. Not ideal, but better than returning
	 * large packets when the user is not expecting them.
	 */
	max_rx_pkt_len = enic->rte_dev->data->dev_conf.rxmode.max_rx_pkt_len;
	rq_buf_len = rte_pktmbuf_data_room_size(rq->mp) - RTE_PKTMBUF_HEADROOM;
	if (max_rx_pkt_len < rq_buf_len && !rq->data_queue_enable)
		rq_buf_len = max_rx_pkt_len;
	for (i = 0; i < rq->ring.desc_count; i++, rqd++) {
		mb = rte_mbuf_raw_alloc(rq->mp);
		if (mb == NULL) {
			dev_err(enic, "RX mbuf alloc failed queue_id=%u\n",
			(unsigned)rq->index);
			return -ENOMEM;
		}

		mb->data_off = RTE_PKTMBUF_HEADROOM;
		dma_addr = (dma_addr_t)(mb->buf_iova
			   + RTE_PKTMBUF_HEADROOM);
		rq_enet_desc_enc(rqd, dma_addr,
				(rq->is_sop ? RQ_ENET_TYPE_ONLY_SOP
				: RQ_ENET_TYPE_NOT_SOP),
				rq_buf_len);
		rq->mbuf_ring[i] = mb;
	}
	/*
	 * Do not post the buffers to the NIC until we enable the RQ via
	 * enic_start_rq().
	 */
	rq->need_initial_post = true;
	/* Initialize fetch index while RQ is disabled */
	iowrite32(0, &rq->ctrl->fetch_index);
	return 0;
}

/*
 * Post the Rx buffers for the first time. enic_alloc_rx_queue_mbufs() has
 * allocated the buffers and filled the RQ descriptor ring. Just need to push
 * the post index to the NIC.
 */
static void
enic_initial_post_rx(struct enic *enic, struct vnic_rq *rq)
{
	if (!rq->in_use || !rq->need_initial_post)
		return;

	/* make sure all prior writes are complete before doing the PIO write */
	rte_rmb();

	/* Post all but the last buffer to VIC. */
	rq->posted_index = rq->ring.desc_count - 1;

	rq->rx_nb_hold = 0;

	dev_debug(enic, "port=%u, qidx=%u, Write %u posted idx, %u sw held\n",
		enic->port_id, rq->index, rq->posted_index, rq->rx_nb_hold);
	iowrite32(rq->posted_index, &rq->ctrl->posted_index);
	rte_rmb();
	rq->need_initial_post = false;
}

static void *
enic_alloc_consistent(void *priv, size_t size,
	dma_addr_t *dma_handle, u8 *name)
{
	void *vaddr;
	const struct rte_memzone *rz;
	*dma_handle = 0;
	struct enic *enic = (struct enic *)priv;
	struct enic_memzone_entry *mze;

	rz = rte_memzone_reserve_aligned((const char *)name, size,
			SOCKET_ID_ANY, RTE_MEMZONE_IOVA_CONTIG, ENIC_ALIGN);
	if (!rz) {
		pr_err("%s : Failed to allocate memory requested for %s\n",
			__func__, name);
		return NULL;
	}

	vaddr = rz->addr;
	*dma_handle = (dma_addr_t)rz->iova;

	mze = rte_malloc("enic memzone entry",
			 sizeof(struct enic_memzone_entry), 0);

	if (!mze) {
		pr_err("%s : Failed to allocate memory for memzone list\n",
		       __func__);
		rte_memzone_free(rz);
		return NULL;
	}

	mze->rz = rz;

	rte_spinlock_lock(&enic->memzone_list_lock);
	LIST_INSERT_HEAD(&enic->memzone_list, mze, entries);
	rte_spinlock_unlock(&enic->memzone_list_lock);

	return vaddr;
}

static void
enic_free_consistent(void *priv,
		     __rte_unused size_t size,
		     void *vaddr,
		     dma_addr_t dma_handle)
{
	struct enic_memzone_entry *mze;
	struct enic *enic = (struct enic *)priv;

	rte_spinlock_lock(&enic->memzone_list_lock);
	LIST_FOREACH(mze, &enic->memzone_list, entries) {
		if (mze->rz->addr == vaddr &&
		    mze->rz->iova == dma_handle)
			break;
	}
	if (mze == NULL) {
		rte_spinlock_unlock(&enic->memzone_list_lock);
		dev_warning(enic,
			    "Tried to free memory, but couldn't find it in the memzone list\n");
		return;
	}
	LIST_REMOVE(mze, entries);
	rte_spinlock_unlock(&enic->memzone_list_lock);
	rte_memzone_free(mze->rz);
	rte_free(mze);
}

int enic_link_update(struct enic *enic)
{
	struct rte_eth_dev *eth_dev = enic->rte_dev;
	struct rte_eth_link link;

	memset(&link, 0, sizeof(link));
	link.link_status = enic_get_link_status(enic);
	link.link_duplex = ETH_LINK_FULL_DUPLEX;
	link.link_speed = vnic_dev_port_speed(enic->vdev);

	return rte_eth_linkstatus_set(eth_dev, &link);
}

static void
enic_intr_handler(void *arg)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)arg;
	struct enic *enic = pmd_priv(dev);

	vnic_intr_return_all_credits(&enic->intr[ENICPMD_LSC_INTR_OFFSET]);

	enic_link_update(enic);
	_rte_eth_dev_callback_process(dev, RTE_ETH_EVENT_INTR_LSC, NULL);
	enic_log_q_error(enic);
}

static int enic_rxq_intr_init(struct enic *enic)
{
	struct rte_intr_handle *intr_handle;
	uint32_t rxq_intr_count, i;
	int err;

	intr_handle = enic->rte_dev->intr_handle;
	if (!enic->rte_dev->data->dev_conf.intr_conf.rxq)
		return 0;
	/*
	 * Rx queue interrupts only work when we have MSI-X interrupts,
	 * one per queue. Sharing one interrupt is technically
	 * possible with VIC, but it is not worth the complications it brings.
	 */
	if (!rte_intr_cap_multiple(intr_handle)) {
		dev_err(enic, "Rx queue interrupts require MSI-X interrupts"
			" (vfio-pci driver)\n");
		return -ENOTSUP;
	}
	rxq_intr_count = enic->intr_count - ENICPMD_RXQ_INTR_OFFSET;
	err = rte_intr_efd_enable(intr_handle, rxq_intr_count);
	if (err) {
		dev_err(enic, "Failed to enable event fds for Rx queue"
			" interrupts\n");
		return err;
	}
	intr_handle->intr_vec = rte_zmalloc("enic_intr_vec",
					    rxq_intr_count * sizeof(int), 0);
	if (intr_handle->intr_vec == NULL) {
		dev_err(enic, "Failed to allocate intr_vec\n");
		return -ENOMEM;
	}
	for (i = 0; i < rxq_intr_count; i++)
		intr_handle->intr_vec[i] = i + ENICPMD_RXQ_INTR_OFFSET;
	return 0;
}

static void enic_rxq_intr_deinit(struct enic *enic)
{
	struct rte_intr_handle *intr_handle;

	intr_handle = enic->rte_dev->intr_handle;
	rte_intr_efd_disable(intr_handle);
	if (intr_handle->intr_vec != NULL) {
		rte_free(intr_handle->intr_vec);
		intr_handle->intr_vec = NULL;
	}
}

static void enic_prep_wq_for_simple_tx(struct enic *enic, uint16_t queue_idx)
{
	struct wq_enet_desc *desc;
	struct vnic_wq *wq;
	unsigned int i;

	/*
	 * Fill WQ descriptor fields that never change. Every descriptor is
	 * one packet, so set EOP. Also set CQ_ENTRY every ENIC_WQ_CQ_THRESH
	 * descriptors (i.e. request one completion update every 32 packets).
	 */
	wq = &enic->wq[queue_idx];
	desc = (struct wq_enet_desc *)wq->ring.descs;
	for (i = 0; i < wq->ring.desc_count; i++, desc++) {
		desc->header_length_flags = 1 << WQ_ENET_FLAGS_EOP_SHIFT;
		if (i % ENIC_WQ_CQ_THRESH == ENIC_WQ_CQ_THRESH - 1)
			desc->header_length_flags |=
				(1 << WQ_ENET_FLAGS_CQ_ENTRY_SHIFT);
	}
}

/*
 * The 'strong' version is in enic_rxtx_vec_avx2.c. This weak version is used
 * used when that file is not compiled.
 */
__rte_weak bool
enic_use_vector_rx_handler(__rte_unused struct enic *enic)
{
	return false;
}

static void pick_rx_handler(struct enic *enic)
{
	struct rte_eth_dev *eth_dev;

	/*
	 * Preference order:
	 * 1. The vectorized handler if possible and requested.
	 * 2. The non-scatter, simplified handler if scatter Rx is not used.
	 * 3. The default handler as a fallback.
	 */
	eth_dev = enic->rte_dev;
	if (enic_use_vector_rx_handler(enic))
		return;
	if (enic->rq_count > 0 && enic->rq[0].data_queue_enable == 0) {
		PMD_INIT_LOG(DEBUG, " use the non-scatter Rx handler");
		eth_dev->rx_pkt_burst = &enic_noscatter_recv_pkts;
	} else {
		PMD_INIT_LOG(DEBUG, " use the normal Rx handler");
		eth_dev->rx_pkt_burst = &enic_recv_pkts;
	}
}

int enic_enable(struct enic *enic)
{
	unsigned int index;
	int err;
	struct rte_eth_dev *eth_dev = enic->rte_dev;
	uint64_t simple_tx_offloads;
	uintptr_t p;

	if (enic->enable_avx2_rx) {
		struct rte_mbuf mb_def = { .buf_addr = 0 };

		/*
		 * mbuf_initializer contains const-after-init fields of
		 * receive mbufs (i.e. 64 bits of fields from rearm_data).
		 * It is currently used by the vectorized handler.
		 */
		mb_def.nb_segs = 1;
		mb_def.data_off = RTE_PKTMBUF_HEADROOM;
		mb_def.port = enic->port_id;
		rte_mbuf_refcnt_set(&mb_def, 1);
		rte_compiler_barrier();
		p = (uintptr_t)&mb_def.rearm_data;
		enic->mbuf_initializer = *(uint64_t *)p;
	}

	eth_dev->data->dev_link.link_speed = vnic_dev_port_speed(enic->vdev);
	eth_dev->data->dev_link.link_duplex = ETH_LINK_FULL_DUPLEX;

	/* vnic notification of link status has already been turned on in
	 * enic_dev_init() which is called during probe time.  Here we are
	 * just turning on interrupt vector 0 if needed.
	 */
	if (eth_dev->data->dev_conf.intr_conf.lsc)
		vnic_dev_notify_set(enic->vdev, 0);

	err = enic_rxq_intr_init(enic);
	if (err)
		return err;
	if (enic_clsf_init(enic))
		dev_warning(enic, "Init of hash table for clsf failed."\
			"Flow director feature will not work\n");

	for (index = 0; index < enic->rq_count; index++) {
		err = enic_alloc_rx_queue_mbufs(enic,
			&enic->rq[enic_rte_rq_idx_to_sop_idx(index)]);
		if (err) {
			dev_err(enic, "Failed to alloc sop RX queue mbufs\n");
			return err;
		}
		err = enic_alloc_rx_queue_mbufs(enic,
			&enic->rq[enic_rte_rq_idx_to_data_idx(index)]);
		if (err) {
			/* release the allocated mbufs for the sop rq*/
			enic_rxmbuf_queue_release(enic,
				&enic->rq[enic_rte_rq_idx_to_sop_idx(index)]);

			dev_err(enic, "Failed to alloc data RX queue mbufs\n");
			return err;
		}
	}

	/*
	 * Use the simple TX handler if possible. Only checksum offloads
	 * and vlan insertion are supported.
	 */
	simple_tx_offloads = enic->tx_offload_capa &
		(DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM |
		 DEV_TX_OFFLOAD_VLAN_INSERT |
		 DEV_TX_OFFLOAD_IPV4_CKSUM |
		 DEV_TX_OFFLOAD_UDP_CKSUM |
		 DEV_TX_OFFLOAD_TCP_CKSUM);
	if ((eth_dev->data->dev_conf.txmode.offloads &
	     ~simple_tx_offloads) == 0) {
		PMD_INIT_LOG(DEBUG, " use the simple tx handler");
		eth_dev->tx_pkt_burst = &enic_simple_xmit_pkts;
		for (index = 0; index < enic->wq_count; index++)
			enic_prep_wq_for_simple_tx(enic, index);
	} else {
		PMD_INIT_LOG(DEBUG, " use the default tx handler");
		eth_dev->tx_pkt_burst = &enic_xmit_pkts;
	}

	pick_rx_handler(enic);

	for (index = 0; index < enic->wq_count; index++)
		enic_start_wq(enic, index);
	for (index = 0; index < enic->rq_count; index++)
		enic_start_rq(enic, index);

	vnic_dev_add_addr(enic->vdev, enic->mac_addr);

	vnic_dev_enable_wait(enic->vdev);

	/* Register and enable error interrupt */
	rte_intr_callback_register(&(enic->pdev->intr_handle),
		enic_intr_handler, (void *)enic->rte_dev);

	rte_intr_enable(&(enic->pdev->intr_handle));
	/* Unmask LSC interrupt */
	vnic_intr_unmask(&enic->intr[ENICPMD_LSC_INTR_OFFSET]);

	return 0;
}

int enic_alloc_intr_resources(struct enic *enic)
{
	int err;
	unsigned int i;

	dev_info(enic, "vNIC resources used:  "\
		"wq %d rq %d cq %d intr %d\n",
		enic->wq_count, enic_vnic_rq_count(enic),
		enic->cq_count, enic->intr_count);

	for (i = 0; i < enic->intr_count; i++) {
		err = vnic_intr_alloc(enic->vdev, &enic->intr[i], i);
		if (err) {
			enic_free_vnic_resources(enic);
			return err;
		}
	}
	return 0;
}

void enic_free_rq(void *rxq)
{
	struct vnic_rq *rq_sop, *rq_data;
	struct enic *enic;

	if (rxq == NULL)
		return;

	rq_sop = (struct vnic_rq *)rxq;
	enic = vnic_dev_priv(rq_sop->vdev);
	rq_data = &enic->rq[rq_sop->data_queue_idx];

	if (rq_sop->free_mbufs) {
		struct rte_mbuf **mb;
		int i;

		mb = rq_sop->free_mbufs;
		for (i = ENIC_RX_BURST_MAX - rq_sop->num_free_mbufs;
		     i < ENIC_RX_BURST_MAX; i++)
			rte_pktmbuf_free(mb[i]);
		rte_free(rq_sop->free_mbufs);
		rq_sop->free_mbufs = NULL;
		rq_sop->num_free_mbufs = 0;
	}

	enic_rxmbuf_queue_release(enic, rq_sop);
	if (rq_data->in_use)
		enic_rxmbuf_queue_release(enic, rq_data);

	rte_free(rq_sop->mbuf_ring);
	if (rq_data->in_use)
		rte_free(rq_data->mbuf_ring);

	rq_sop->mbuf_ring = NULL;
	rq_data->mbuf_ring = NULL;

	vnic_rq_free(rq_sop);
	if (rq_data->in_use)
		vnic_rq_free(rq_data);

	vnic_cq_free(&enic->cq[enic_sop_rq_idx_to_cq_idx(rq_sop->index)]);

	rq_sop->in_use = 0;
	rq_data->in_use = 0;
}

void enic_start_wq(struct enic *enic, uint16_t queue_idx)
{
	struct rte_eth_dev *eth_dev = enic->rte_dev;
	vnic_wq_enable(&enic->wq[queue_idx]);
	eth_dev->data->tx_queue_state[queue_idx] = RTE_ETH_QUEUE_STATE_STARTED;
}

int enic_stop_wq(struct enic *enic, uint16_t queue_idx)
{
	struct rte_eth_dev *eth_dev = enic->rte_dev;
	int ret;

	ret = vnic_wq_disable(&enic->wq[queue_idx]);
	if (ret)
		return ret;

	eth_dev->data->tx_queue_state[queue_idx] = RTE_ETH_QUEUE_STATE_STOPPED;
	return 0;
}

void enic_start_rq(struct enic *enic, uint16_t queue_idx)
{
	struct vnic_rq *rq_sop;
	struct vnic_rq *rq_data;
	rq_sop = &enic->rq[enic_rte_rq_idx_to_sop_idx(queue_idx)];
	rq_data = &enic->rq[rq_sop->data_queue_idx];
	struct rte_eth_dev *eth_dev = enic->rte_dev;

	if (rq_data->in_use) {
		vnic_rq_enable(rq_data);
		enic_initial_post_rx(enic, rq_data);
	}
	rte_mb();
	vnic_rq_enable(rq_sop);
	enic_initial_post_rx(enic, rq_sop);
	eth_dev->data->rx_queue_state[queue_idx] = RTE_ETH_QUEUE_STATE_STARTED;
}

int enic_stop_rq(struct enic *enic, uint16_t queue_idx)
{
	int ret1 = 0, ret2 = 0;
	struct rte_eth_dev *eth_dev = enic->rte_dev;
	struct vnic_rq *rq_sop;
	struct vnic_rq *rq_data;
	rq_sop = &enic->rq[enic_rte_rq_idx_to_sop_idx(queue_idx)];
	rq_data = &enic->rq[rq_sop->data_queue_idx];

	ret2 = vnic_rq_disable(rq_sop);
	rte_mb();
	if (rq_data->in_use)
		ret1 = vnic_rq_disable(rq_data);

	if (ret2)
		return ret2;
	else if (ret1)
		return ret1;

	eth_dev->data->rx_queue_state[queue_idx] = RTE_ETH_QUEUE_STATE_STOPPED;
	return 0;
}

int enic_alloc_rq(struct enic *enic, uint16_t queue_idx,
	unsigned int socket_id, struct rte_mempool *mp,
	uint16_t nb_desc, uint16_t free_thresh)
{
	int rc;
	uint16_t sop_queue_idx = enic_rte_rq_idx_to_sop_idx(queue_idx);
	uint16_t data_queue_idx = enic_rte_rq_idx_to_data_idx(queue_idx);
	struct vnic_rq *rq_sop = &enic->rq[sop_queue_idx];
	struct vnic_rq *rq_data = &enic->rq[data_queue_idx];
	unsigned int mbuf_size, mbufs_per_pkt;
	unsigned int nb_sop_desc, nb_data_desc;
	uint16_t min_sop, max_sop, min_data, max_data;
	uint32_t max_rx_pkt_len;

	rq_sop->is_sop = 1;
	rq_sop->data_queue_idx = data_queue_idx;
	rq_data->is_sop = 0;
	rq_data->data_queue_idx = 0;
	rq_sop->socket_id = socket_id;
	rq_sop->mp = mp;
	rq_data->socket_id = socket_id;
	rq_data->mp = mp;
	rq_sop->in_use = 1;
	rq_sop->rx_free_thresh = free_thresh;
	rq_data->rx_free_thresh = free_thresh;
	dev_debug(enic, "Set queue_id:%u free thresh:%u\n", queue_idx,
		  free_thresh);

	mbuf_size = (uint16_t)(rte_pktmbuf_data_room_size(mp) -
			       RTE_PKTMBUF_HEADROOM);
	/* max_rx_pkt_len includes the ethernet header and CRC. */
	max_rx_pkt_len = enic->rte_dev->data->dev_conf.rxmode.max_rx_pkt_len;

	if (enic->rte_dev->data->dev_conf.rxmode.offloads &
	    DEV_RX_OFFLOAD_SCATTER) {
		dev_info(enic, "Rq %u Scatter rx mode enabled\n", queue_idx);
		/* ceil((max pkt len)/mbuf_size) */
		mbufs_per_pkt = (max_rx_pkt_len + mbuf_size - 1) / mbuf_size;
	} else {
		dev_info(enic, "Scatter rx mode disabled\n");
		mbufs_per_pkt = 1;
		if (max_rx_pkt_len > mbuf_size) {
			dev_warning(enic, "The maximum Rx packet size (%u) is"
				    " larger than the mbuf size (%u), and"
				    " scatter is disabled. Larger packets will"
				    " be truncated.\n",
				    max_rx_pkt_len, mbuf_size);
		}
	}

	if (mbufs_per_pkt > 1) {
		dev_info(enic, "Rq %u Scatter rx mode in use\n", queue_idx);
		rq_sop->data_queue_enable = 1;
		rq_data->in_use = 1;
		/*
		 * HW does not directly support rxmode.max_rx_pkt_len. HW always
		 * receives packet sizes up to the "max" MTU.
		 * If not using scatter, we can achieve the effect of dropping
		 * larger packets by reducing the size of posted buffers.
		 * See enic_alloc_rx_queue_mbufs().
		 */
		if (max_rx_pkt_len <
		    enic_mtu_to_max_rx_pktlen(enic->max_mtu)) {
			dev_warning(enic, "rxmode.max_rx_pkt_len is ignored"
				    " when scatter rx mode is in use.\n");
		}
	} else {
		dev_info(enic, "Rq %u Scatter rx mode not being used\n",
			 queue_idx);
		rq_sop->data_queue_enable = 0;
		rq_data->in_use = 0;
	}

	/* number of descriptors have to be a multiple of 32 */
	nb_sop_desc = (nb_desc / mbufs_per_pkt) & ENIC_ALIGN_DESCS_MASK;
	nb_data_desc = (nb_desc - nb_sop_desc) & ENIC_ALIGN_DESCS_MASK;

	rq_sop->max_mbufs_per_pkt = mbufs_per_pkt;
	rq_data->max_mbufs_per_pkt = mbufs_per_pkt;

	if (mbufs_per_pkt > 1) {
		min_sop = ENIC_RX_BURST_MAX;
		max_sop = ((enic->config.rq_desc_count /
			    (mbufs_per_pkt - 1)) & ENIC_ALIGN_DESCS_MASK);
		min_data = min_sop * (mbufs_per_pkt - 1);
		max_data = enic->config.rq_desc_count;
	} else {
		min_sop = ENIC_RX_BURST_MAX;
		max_sop = enic->config.rq_desc_count;
		min_data = 0;
		max_data = 0;
	}

	if (nb_desc < (min_sop + min_data)) {
		dev_warning(enic,
			    "Number of rx descs too low, adjusting to minimum\n");
		nb_sop_desc = min_sop;
		nb_data_desc = min_data;
	} else if (nb_desc > (max_sop + max_data)) {
		dev_warning(enic,
			    "Number of rx_descs too high, adjusting to maximum\n");
		nb_sop_desc = max_sop;
		nb_data_desc = max_data;
	}
	if (mbufs_per_pkt > 1) {
		dev_info(enic, "For max packet size %u and mbuf size %u valid"
			 " rx descriptor range is %u to %u\n",
			 max_rx_pkt_len, mbuf_size, min_sop + min_data,
			 max_sop + max_data);
	}
	dev_info(enic, "Using %d rx descriptors (sop %d, data %d)\n",
		 nb_sop_desc + nb_data_desc, nb_sop_desc, nb_data_desc);

	/* Allocate sop queue resources */
	rc = vnic_rq_alloc(enic->vdev, rq_sop, sop_queue_idx,
		nb_sop_desc, sizeof(struct rq_enet_desc));
	if (rc) {
		dev_err(enic, "error in allocation of sop rq\n");
		goto err_exit;
	}
	nb_sop_desc = rq_sop->ring.desc_count;

	if (rq_data->in_use) {
		/* Allocate data queue resources */
		rc = vnic_rq_alloc(enic->vdev, rq_data, data_queue_idx,
				   nb_data_desc,
				   sizeof(struct rq_enet_desc));
		if (rc) {
			dev_err(enic, "error in allocation of data rq\n");
			goto err_free_rq_sop;
		}
		nb_data_desc = rq_data->ring.desc_count;
	}
	rc = vnic_cq_alloc(enic->vdev, &enic->cq[queue_idx], queue_idx,
			   socket_id, nb_sop_desc + nb_data_desc,
			   sizeof(struct cq_enet_rq_desc));
	if (rc) {
		dev_err(enic, "error in allocation of cq for rq\n");
		goto err_free_rq_data;
	}

	/* Allocate the mbuf rings */
	rq_sop->mbuf_ring = (struct rte_mbuf **)
		rte_zmalloc_socket("rq->mbuf_ring",
				   sizeof(struct rte_mbuf *) * nb_sop_desc,
				   RTE_CACHE_LINE_SIZE, rq_sop->socket_id);
	if (rq_sop->mbuf_ring == NULL)
		goto err_free_cq;

	if (rq_data->in_use) {
		rq_data->mbuf_ring = (struct rte_mbuf **)
			rte_zmalloc_socket("rq->mbuf_ring",
				sizeof(struct rte_mbuf *) * nb_data_desc,
				RTE_CACHE_LINE_SIZE, rq_sop->socket_id);
		if (rq_data->mbuf_ring == NULL)
			goto err_free_sop_mbuf;
	}

	rq_sop->free_mbufs = (struct rte_mbuf **)
		rte_zmalloc_socket("rq->free_mbufs",
				   sizeof(struct rte_mbuf *) *
				   ENIC_RX_BURST_MAX,
				   RTE_CACHE_LINE_SIZE, rq_sop->socket_id);
	if (rq_sop->free_mbufs == NULL)
		goto err_free_data_mbuf;
	rq_sop->num_free_mbufs = 0;

	rq_sop->tot_nb_desc = nb_desc; /* squirl away for MTU update function */

	return 0;

err_free_data_mbuf:
	rte_free(rq_data->mbuf_ring);
err_free_sop_mbuf:
	rte_free(rq_sop->mbuf_ring);
err_free_cq:
	/* cleanup on error */
	vnic_cq_free(&enic->cq[queue_idx]);
err_free_rq_data:
	if (rq_data->in_use)
		vnic_rq_free(rq_data);
err_free_rq_sop:
	vnic_rq_free(rq_sop);
err_exit:
	return -ENOMEM;
}

void enic_free_wq(void *txq)
{
	struct vnic_wq *wq;
	struct enic *enic;

	if (txq == NULL)
		return;

	wq = (struct vnic_wq *)txq;
	enic = vnic_dev_priv(wq->vdev);
	rte_memzone_free(wq->cqmsg_rz);
	vnic_wq_free(wq);
	vnic_cq_free(&enic->cq[enic->rq_count + wq->index]);
}

int enic_alloc_wq(struct enic *enic, uint16_t queue_idx,
	unsigned int socket_id, uint16_t nb_desc)
{
	int err;
	struct vnic_wq *wq = &enic->wq[queue_idx];
	unsigned int cq_index = enic_cq_wq(enic, queue_idx);
	char name[NAME_MAX];
	static int instance;

	wq->socket_id = socket_id;
	/*
	 * rte_eth_tx_queue_setup() checks min, max, and alignment. So just
	 * print an info message for diagnostics.
	 */
	dev_info(enic, "TX Queues - effective number of descs:%d\n", nb_desc);

	/* Allocate queue resources */
	err = vnic_wq_alloc(enic->vdev, &enic->wq[queue_idx], queue_idx,
		nb_desc,
		sizeof(struct wq_enet_desc));
	if (err) {
		dev_err(enic, "error in allocation of wq\n");
		return err;
	}

	err = vnic_cq_alloc(enic->vdev, &enic->cq[cq_index], cq_index,
		socket_id, nb_desc,
		sizeof(struct cq_enet_wq_desc));
	if (err) {
		vnic_wq_free(wq);
		dev_err(enic, "error in allocation of cq for wq\n");
	}

	/* setup up CQ message */
	snprintf((char *)name, sizeof(name),
		 "vnic_cqmsg-%s-%d-%d", enic->bdf_name, queue_idx,
		instance++);

	wq->cqmsg_rz = rte_memzone_reserve_aligned((const char *)name,
			sizeof(uint32_t), SOCKET_ID_ANY,
			RTE_MEMZONE_IOVA_CONTIG, ENIC_ALIGN);
	if (!wq->cqmsg_rz)
		return -ENOMEM;

	return err;
}

int enic_disable(struct enic *enic)
{
	unsigned int i;
	int err;

	for (i = 0; i < enic->intr_count; i++) {
		vnic_intr_mask(&enic->intr[i]);
		(void)vnic_intr_masked(&enic->intr[i]); /* flush write */
	}
	enic_rxq_intr_deinit(enic);
	rte_intr_disable(&enic->pdev->intr_handle);
	rte_intr_callback_unregister(&enic->pdev->intr_handle,
				     enic_intr_handler,
				     (void *)enic->rte_dev);

	vnic_dev_disable(enic->vdev);

	enic_clsf_destroy(enic);

	if (!enic_is_sriov_vf(enic))
		vnic_dev_del_addr(enic->vdev, enic->mac_addr);

	for (i = 0; i < enic->wq_count; i++) {
		err = vnic_wq_disable(&enic->wq[i]);
		if (err)
			return err;
	}
	for (i = 0; i < enic_vnic_rq_count(enic); i++) {
		if (enic->rq[i].in_use) {
			err = vnic_rq_disable(&enic->rq[i]);
			if (err)
				return err;
		}
	}

	/* If we were using interrupts, set the interrupt vector to -1
	 * to disable interrupts.  We are not disabling link notifcations,
	 * though, as we want the polling of link status to continue working.
	 */
	if (enic->rte_dev->data->dev_conf.intr_conf.lsc)
		vnic_dev_notify_set(enic->vdev, -1);

	vnic_dev_set_reset_flag(enic->vdev, 1);

	for (i = 0; i < enic->wq_count; i++)
		vnic_wq_clean(&enic->wq[i], enic_free_wq_buf);

	for (i = 0; i < enic_vnic_rq_count(enic); i++)
		if (enic->rq[i].in_use)
			vnic_rq_clean(&enic->rq[i], enic_free_rq_buf);
	for (i = 0; i < enic->cq_count; i++)
		vnic_cq_clean(&enic->cq[i]);
	for (i = 0; i < enic->intr_count; i++)
		vnic_intr_clean(&enic->intr[i]);

	return 0;
}

static int enic_dev_wait(struct vnic_dev *vdev,
	int (*start)(struct vnic_dev *, int),
	int (*finished)(struct vnic_dev *, int *),
	int arg)
{
	int done;
	int err;
	int i;

	err = start(vdev, arg);
	if (err)
		return err;

	/* Wait for func to complete...2 seconds max */
	for (i = 0; i < 2000; i++) {
		err = finished(vdev, &done);
		if (err)
			return err;
		if (done)
			return 0;
		usleep(1000);
	}
	return -ETIMEDOUT;
}

static int enic_dev_open(struct enic *enic)
{
	int err;
	int flags = CMD_OPENF_IG_DESCCACHE;

	err = enic_dev_wait(enic->vdev, vnic_dev_open,
		vnic_dev_open_done, flags);
	if (err)
		dev_err(enic_get_dev(enic),
			"vNIC device open failed, err %d\n", err);

	return err;
}

static int enic_set_rsskey(struct enic *enic, uint8_t *user_key)
{
	dma_addr_t rss_key_buf_pa;
	union vnic_rss_key *rss_key_buf_va = NULL;
	int err, i;
	u8 name[NAME_MAX];

	RTE_ASSERT(user_key != NULL);
	snprintf((char *)name, NAME_MAX, "rss_key-%s", enic->bdf_name);
	rss_key_buf_va = enic_alloc_consistent(enic, sizeof(union vnic_rss_key),
		&rss_key_buf_pa, name);
	if (!rss_key_buf_va)
		return -ENOMEM;

	for (i = 0; i < ENIC_RSS_HASH_KEY_SIZE; i++)
		rss_key_buf_va->key[i / 10].b[i % 10] = user_key[i];

	err = enic_set_rss_key(enic,
		rss_key_buf_pa,
		sizeof(union vnic_rss_key));

	/* Save for later queries */
	if (!err) {
		rte_memcpy(&enic->rss_key, rss_key_buf_va,
			   sizeof(union vnic_rss_key));
	}
	enic_free_consistent(enic, sizeof(union vnic_rss_key),
		rss_key_buf_va, rss_key_buf_pa);

	return err;
}

int enic_set_rss_reta(struct enic *enic, union vnic_rss_cpu *rss_cpu)
{
	dma_addr_t rss_cpu_buf_pa;
	union vnic_rss_cpu *rss_cpu_buf_va = NULL;
	int err;
	u8 name[NAME_MAX];

	snprintf((char *)name, NAME_MAX, "rss_cpu-%s", enic->bdf_name);
	rss_cpu_buf_va = enic_alloc_consistent(enic, sizeof(union vnic_rss_cpu),
		&rss_cpu_buf_pa, name);
	if (!rss_cpu_buf_va)
		return -ENOMEM;

	rte_memcpy(rss_cpu_buf_va, rss_cpu, sizeof(union vnic_rss_cpu));

	err = enic_set_rss_cpu(enic,
		rss_cpu_buf_pa,
		sizeof(union vnic_rss_cpu));

	enic_free_consistent(enic, sizeof(union vnic_rss_cpu),
		rss_cpu_buf_va, rss_cpu_buf_pa);

	/* Save for later queries */
	if (!err)
		rte_memcpy(&enic->rss_cpu, rss_cpu, sizeof(union vnic_rss_cpu));
	return err;
}

static int enic_set_niccfg(struct enic *enic, u8 rss_default_cpu,
	u8 rss_hash_type, u8 rss_hash_bits, u8 rss_base_cpu, u8 rss_enable)
{
	const u8 tso_ipid_split_en = 0;
	int err;

	err = enic_set_nic_cfg(enic,
		rss_default_cpu, rss_hash_type,
		rss_hash_bits, rss_base_cpu,
		rss_enable, tso_ipid_split_en,
		enic->ig_vlan_strip_en);

	return err;
}

/* Initialize RSS with defaults, called from dev_configure */
int enic_init_rss_nic_cfg(struct enic *enic)
{
	static uint8_t default_rss_key[] = {
		85, 67, 83, 97, 119, 101, 115, 111, 109, 101,
		80, 65, 76, 79, 117, 110, 105, 113, 117, 101,
		76, 73, 78, 85, 88, 114, 111, 99, 107, 115,
		69, 78, 73, 67, 105, 115, 99, 111, 111, 108,
	};
	struct rte_eth_rss_conf rss_conf;
	union vnic_rss_cpu rss_cpu;
	int ret, i;

	rss_conf = enic->rte_dev->data->dev_conf.rx_adv_conf.rss_conf;
	/*
	 * If setting key for the first time, and the user gives us none, then
	 * push the default key to NIC.
	 */
	if (rss_conf.rss_key == NULL) {
		rss_conf.rss_key = default_rss_key;
		rss_conf.rss_key_len = ENIC_RSS_HASH_KEY_SIZE;
	}
	ret = enic_set_rss_conf(enic, &rss_conf);
	if (ret) {
		dev_err(enic, "Failed to configure RSS\n");
		return ret;
	}
	if (enic->rss_enable) {
		/* If enabling RSS, use the default reta */
		for (i = 0; i < ENIC_RSS_RETA_SIZE; i++) {
			rss_cpu.cpu[i / 4].b[i % 4] =
				enic_rte_rq_idx_to_sop_idx(i % enic->rq_count);
		}
		ret = enic_set_rss_reta(enic, &rss_cpu);
		if (ret)
			dev_err(enic, "Failed to set RSS indirection table\n");
	}
	return ret;
}

int enic_setup_finish(struct enic *enic)
{
	enic_init_soft_stats(enic);

	/* Default conf */
	vnic_dev_packet_filter(enic->vdev,
		1 /* directed  */,
		1 /* multicast */,
		1 /* broadcast */,
		0 /* promisc   */,
		1 /* allmulti  */);

	enic->promisc = 0;
	enic->allmulti = 1;

	return 0;
}

static int enic_rss_conf_valid(struct enic *enic,
			       struct rte_eth_rss_conf *rss_conf)
{
	/* RSS is disabled per VIC settings. Ignore rss_conf. */
	if (enic->flow_type_rss_offloads == 0)
		return 0;
	if (rss_conf->rss_key != NULL &&
	    rss_conf->rss_key_len != ENIC_RSS_HASH_KEY_SIZE) {
		dev_err(enic, "Given rss_key is %d bytes, it must be %d\n",
			rss_conf->rss_key_len, ENIC_RSS_HASH_KEY_SIZE);
		return -EINVAL;
	}
	if (rss_conf->rss_hf != 0 &&
	    (rss_conf->rss_hf & enic->flow_type_rss_offloads) == 0) {
		dev_err(enic, "Given rss_hf contains none of the supported"
			" types\n");
		return -EINVAL;
	}
	return 0;
}

/* Set hash type and key according to rss_conf */
int enic_set_rss_conf(struct enic *enic, struct rte_eth_rss_conf *rss_conf)
{
	struct rte_eth_dev *eth_dev;
	uint64_t rss_hf;
	u8 rss_hash_type;
	u8 rss_enable;
	int ret;

	RTE_ASSERT(rss_conf != NULL);
	ret = enic_rss_conf_valid(enic, rss_conf);
	if (ret) {
		dev_err(enic, "RSS configuration (rss_conf) is invalid\n");
		return ret;
	}

	eth_dev = enic->rte_dev;
	rss_hash_type = 0;
	rss_hf = rss_conf->rss_hf & enic->flow_type_rss_offloads;
	if (enic->rq_count > 1 &&
	    (eth_dev->data->dev_conf.rxmode.mq_mode & ETH_MQ_RX_RSS_FLAG) &&
	    rss_hf != 0) {
		rss_enable = 1;
		if (rss_hf & (ETH_RSS_IPV4 | ETH_RSS_FRAG_IPV4 |
			      ETH_RSS_NONFRAG_IPV4_OTHER))
			rss_hash_type |= NIC_CFG_RSS_HASH_TYPE_IPV4;
		if (rss_hf & ETH_RSS_NONFRAG_IPV4_TCP)
			rss_hash_type |= NIC_CFG_RSS_HASH_TYPE_TCP_IPV4;
		if (rss_hf & ETH_RSS_NONFRAG_IPV4_UDP) {
			rss_hash_type |= NIC_CFG_RSS_HASH_TYPE_UDP_IPV4;
			if (enic->udp_rss_weak) {
				/*
				 * 'TCP' is not a typo. The "weak" version of
				 * UDP RSS requires both the TCP and UDP bits
				 * be set. It does enable TCP RSS as well.
				 */
				rss_hash_type |= NIC_CFG_RSS_HASH_TYPE_TCP_IPV4;
			}
		}
		if (rss_hf & (ETH_RSS_IPV6 | ETH_RSS_IPV6_EX |
			      ETH_RSS_FRAG_IPV6 | ETH_RSS_NONFRAG_IPV6_OTHER))
			rss_hash_type |= NIC_CFG_RSS_HASH_TYPE_IPV6;
		if (rss_hf & (ETH_RSS_NONFRAG_IPV6_TCP | ETH_RSS_IPV6_TCP_EX))
			rss_hash_type |= NIC_CFG_RSS_HASH_TYPE_TCP_IPV6;
		if (rss_hf & (ETH_RSS_NONFRAG_IPV6_UDP | ETH_RSS_IPV6_UDP_EX)) {
			rss_hash_type |= NIC_CFG_RSS_HASH_TYPE_UDP_IPV6;
			if (enic->udp_rss_weak)
				rss_hash_type |= NIC_CFG_RSS_HASH_TYPE_TCP_IPV6;
		}
	} else {
		rss_enable = 0;
		rss_hf = 0;
	}

	/* Set the hash key if provided */
	if (rss_enable && rss_conf->rss_key) {
		ret = enic_set_rsskey(enic, rss_conf->rss_key);
		if (ret) {
			dev_err(enic, "Failed to set RSS key\n");
			return ret;
		}
	}

	ret = enic_set_niccfg(enic, ENIC_RSS_DEFAULT_CPU, rss_hash_type,
			      ENIC_RSS_HASH_BITS, ENIC_RSS_BASE_CPU,
			      rss_enable);
	if (!ret) {
		enic->rss_hf = rss_hf;
		enic->rss_hash_type = rss_hash_type;
		enic->rss_enable = rss_enable;
	} else {
		dev_err(enic, "Failed to update RSS configurations."
			" hash=0x%x\n", rss_hash_type);
	}
	return ret;
}

int enic_set_vlan_strip(struct enic *enic)
{
	/*
	 * Unfortunately, VLAN strip on/off and RSS on/off are configured
	 * together. So, re-do niccfg, preserving the current RSS settings.
	 */
	return enic_set_niccfg(enic, ENIC_RSS_DEFAULT_CPU, enic->rss_hash_type,
			       ENIC_RSS_HASH_BITS, ENIC_RSS_BASE_CPU,
			       enic->rss_enable);
}

void enic_add_packet_filter(struct enic *enic)
{
	/* Args -> directed, multicast, broadcast, promisc, allmulti */
	vnic_dev_packet_filter(enic->vdev, 1, 1, 1,
		enic->promisc, enic->allmulti);
}

int enic_get_link_status(struct enic *enic)
{
	return vnic_dev_link_status(enic->vdev);
}

static void enic_dev_deinit(struct enic *enic)
{
	/* stop link status checking */
	vnic_dev_notify_unset(enic->vdev);

	/* mac_addrs is freed by rte_eth_dev_release_port() */
	rte_free(enic->cq);
	rte_free(enic->intr);
	rte_free(enic->rq);
	rte_free(enic->wq);
}


int enic_set_vnic_res(struct enic *enic)
{
	struct rte_eth_dev *eth_dev = enic->rte_dev;
	int rc = 0;
	unsigned int required_rq, required_wq, required_cq, required_intr;

	/* Always use two vNIC RQs per eth_dev RQ, regardless of Rx scatter. */
	required_rq = eth_dev->data->nb_rx_queues * 2;
	required_wq = eth_dev->data->nb_tx_queues;
	required_cq = eth_dev->data->nb_rx_queues + eth_dev->data->nb_tx_queues;
	required_intr = 1; /* 1 for LSC even if intr_conf.lsc is 0 */
	if (eth_dev->data->dev_conf.intr_conf.rxq) {
		required_intr += eth_dev->data->nb_rx_queues;
	}

	if (enic->conf_rq_count < required_rq) {
		dev_err(dev, "Not enough Receive queues. Requested:%u which uses %d RQs on VIC, Configured:%u\n",
			eth_dev->data->nb_rx_queues,
			required_rq, enic->conf_rq_count);
		rc = -EINVAL;
	}
	if (enic->conf_wq_count < required_wq) {
		dev_err(dev, "Not enough Transmit queues. Requested:%u, Configured:%u\n",
			eth_dev->data->nb_tx_queues, enic->conf_wq_count);
		rc = -EINVAL;
	}

	if (enic->conf_cq_count < required_cq) {
		dev_err(dev, "Not enough Completion queues. Required:%u, Configured:%u\n",
			required_cq, enic->conf_cq_count);
		rc = -EINVAL;
	}
	if (enic->conf_intr_count < required_intr) {
		dev_err(dev, "Not enough Interrupts to support Rx queue"
			" interrupts. Required:%u, Configured:%u\n",
			required_intr, enic->conf_intr_count);
		rc = -EINVAL;
	}

	if (rc == 0) {
		enic->rq_count = eth_dev->data->nb_rx_queues;
		enic->wq_count = eth_dev->data->nb_tx_queues;
		enic->cq_count = enic->rq_count + enic->wq_count;
		enic->intr_count = required_intr;
	}

	return rc;
}

/* Initialize the completion queue for an RQ */
static int
enic_reinit_rq(struct enic *enic, unsigned int rq_idx)
{
	struct vnic_rq *sop_rq, *data_rq;
	unsigned int cq_idx;
	int rc = 0;

	sop_rq = &enic->rq[enic_rte_rq_idx_to_sop_idx(rq_idx)];
	data_rq = &enic->rq[enic_rte_rq_idx_to_data_idx(rq_idx)];
	cq_idx = rq_idx;

	vnic_cq_clean(&enic->cq[cq_idx]);
	vnic_cq_init(&enic->cq[cq_idx],
		     0 /* flow_control_enable */,
		     1 /* color_enable */,
		     0 /* cq_head */,
		     0 /* cq_tail */,
		     1 /* cq_tail_color */,
		     0 /* interrupt_enable */,
		     1 /* cq_entry_enable */,
		     0 /* cq_message_enable */,
		     0 /* interrupt offset */,
		     0 /* cq_message_addr */);


	vnic_rq_init_start(sop_rq, enic_cq_rq(enic,
			   enic_rte_rq_idx_to_sop_idx(rq_idx)), 0,
			   sop_rq->ring.desc_count - 1, 1, 0);
	if (data_rq->in_use) {
		vnic_rq_init_start(data_rq,
				   enic_cq_rq(enic,
				   enic_rte_rq_idx_to_data_idx(rq_idx)), 0,
				   data_rq->ring.desc_count - 1, 1, 0);
	}

	rc = enic_alloc_rx_queue_mbufs(enic, sop_rq);
	if (rc)
		return rc;

	if (data_rq->in_use) {
		rc = enic_alloc_rx_queue_mbufs(enic, data_rq);
		if (rc) {
			enic_rxmbuf_queue_release(enic, sop_rq);
			return rc;
		}
	}

	return 0;
}

/* The Cisco NIC can send and receive packets up to a max packet size
 * determined by the NIC type and firmware. There is also an MTU
 * configured into the NIC via the CIMC/UCSM management interface
 * which can be overridden by this function (up to the max packet size).
 * Depending on the network setup, doing so may cause packet drops
 * and unexpected behavior.
 */
int enic_set_mtu(struct enic *enic, uint16_t new_mtu)
{
	unsigned int rq_idx;
	struct vnic_rq *rq;
	int rc = 0;
	uint16_t old_mtu;	/* previous setting */
	uint16_t config_mtu;	/* Value configured into NIC via CIMC/UCSM */
	struct rte_eth_dev *eth_dev = enic->rte_dev;

	old_mtu = eth_dev->data->mtu;
	config_mtu = enic->config.mtu;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -E_RTE_SECONDARY;

	if (new_mtu > enic->max_mtu) {
		dev_err(enic,
			"MTU not updated: requested (%u) greater than max (%u)\n",
			new_mtu, enic->max_mtu);
		return -EINVAL;
	}
	if (new_mtu < ENIC_MIN_MTU) {
		dev_info(enic,
			"MTU not updated: requested (%u) less than min (%u)\n",
			new_mtu, ENIC_MIN_MTU);
		return -EINVAL;
	}
	if (new_mtu > config_mtu)
		dev_warning(enic,
			"MTU (%u) is greater than value configured in NIC (%u)\n",
			new_mtu, config_mtu);

	/* Update the MTU and maximum packet length */
	eth_dev->data->mtu = new_mtu;
	eth_dev->data->dev_conf.rxmode.max_rx_pkt_len =
		enic_mtu_to_max_rx_pktlen(new_mtu);

	/*
	 * If the device has not started (enic_enable), nothing to do.
	 * Later, enic_enable() will set up RQs reflecting the new maximum
	 * packet length.
	 */
	if (!eth_dev->data->dev_started)
		goto set_mtu_done;

	/*
	 * The device has started, re-do RQs on the fly. In the process, we
	 * pick up the new maximum packet length.
	 *
	 * Some applications rely on the ability to change MTU without stopping
	 * the device. So keep this behavior for now.
	 */
	rte_spinlock_lock(&enic->mtu_lock);

	/* Stop traffic on all RQs */
	for (rq_idx = 0; rq_idx < enic->rq_count * 2; rq_idx++) {
		rq = &enic->rq[rq_idx];
		if (rq->is_sop && rq->in_use) {
			rc = enic_stop_rq(enic,
					  enic_sop_rq_idx_to_rte_idx(rq_idx));
			if (rc) {
				dev_err(enic, "Failed to stop Rq %u\n", rq_idx);
				goto set_mtu_done;
			}
		}
	}

	/* replace Rx function with a no-op to avoid getting stale pkts */
	eth_dev->rx_pkt_burst = enic_dummy_recv_pkts;
	rte_mb();

	/* Allow time for threads to exit the real Rx function. */
	usleep(100000);

	/* now it is safe to reconfigure the RQs */


	/* free and reallocate RQs with the new MTU */
	for (rq_idx = 0; rq_idx < enic->rq_count; rq_idx++) {
		rq = &enic->rq[enic_rte_rq_idx_to_sop_idx(rq_idx)];
		if (!rq->in_use)
			continue;

		enic_free_rq(rq);
		rc = enic_alloc_rq(enic, rq_idx, rq->socket_id, rq->mp,
				   rq->tot_nb_desc, rq->rx_free_thresh);
		if (rc) {
			dev_err(enic,
				"Fatal MTU alloc error- No traffic will pass\n");
			goto set_mtu_done;
		}

		rc = enic_reinit_rq(enic, rq_idx);
		if (rc) {
			dev_err(enic,
				"Fatal MTU RQ reinit- No traffic will pass\n");
			goto set_mtu_done;
		}
	}

	/* put back the real receive function */
	rte_mb();
	pick_rx_handler(enic);
	rte_mb();

	/* restart Rx traffic */
	for (rq_idx = 0; rq_idx < enic->rq_count; rq_idx++) {
		rq = &enic->rq[enic_rte_rq_idx_to_sop_idx(rq_idx)];
		if (rq->is_sop && rq->in_use)
			enic_start_rq(enic, rq_idx);
	}

set_mtu_done:
	dev_info(enic, "MTU changed from %u to %u\n",  old_mtu, new_mtu);
	rte_spinlock_unlock(&enic->mtu_lock);
	return rc;
}

static int enic_dev_init(struct enic *enic)
{
	int err;
	struct rte_eth_dev *eth_dev = enic->rte_dev;

	vnic_dev_intr_coal_timer_info_default(enic->vdev);

	/* Get vNIC configuration
	*/
	err = enic_get_vnic_config(enic);
	if (err) {
		dev_err(dev, "Get vNIC configuration failed, aborting\n");
		return err;
	}

	/* Get available resource counts */
	enic_get_res_counts(enic);
	if (enic->conf_rq_count == 1) {
		dev_err(enic, "Running with only 1 RQ configured in the vNIC is not supported.\n");
		dev_err(enic, "Please configure 2 RQs in the vNIC for each Rx queue used by DPDK.\n");
		dev_err(enic, "See the ENIC PMD guide for more information.\n");
		return -EINVAL;
	}
	/* Queue counts may be zeros. rte_zmalloc returns NULL in that case. */
	enic->cq = rte_zmalloc("enic_vnic_cq", sizeof(struct vnic_cq) *
			       enic->conf_cq_count, 8);
	enic->intr = rte_zmalloc("enic_vnic_intr", sizeof(struct vnic_intr) *
				 enic->conf_intr_count, 8);
	enic->rq = rte_zmalloc("enic_vnic_rq", sizeof(struct vnic_rq) *
			       enic->conf_rq_count, 8);
	enic->wq = rte_zmalloc("enic_vnic_wq", sizeof(struct vnic_wq) *
			       enic->conf_wq_count, 8);
	if (enic->conf_cq_count > 0 && enic->cq == NULL) {
		dev_err(enic, "failed to allocate vnic_cq, aborting.\n");
		return -1;
	}
	if (enic->conf_intr_count > 0 && enic->intr == NULL) {
		dev_err(enic, "failed to allocate vnic_intr, aborting.\n");
		return -1;
	}
	if (enic->conf_rq_count > 0 && enic->rq == NULL) {
		dev_err(enic, "failed to allocate vnic_rq, aborting.\n");
		return -1;
	}
	if (enic->conf_wq_count > 0 && enic->wq == NULL) {
		dev_err(enic, "failed to allocate vnic_wq, aborting.\n");
		return -1;
	}

	/* Get the supported filters */
	enic_fdir_info(enic);

	eth_dev->data->mac_addrs = rte_zmalloc("enic_mac_addr",
					sizeof(struct ether_addr) *
					ENIC_UNICAST_PERFECT_FILTERS, 0);
	if (!eth_dev->data->mac_addrs) {
		dev_err(enic, "mac addr storage alloc failed, aborting.\n");
		return -1;
	}
	ether_addr_copy((struct ether_addr *) enic->mac_addr,
			eth_dev->data->mac_addrs);

	vnic_dev_set_reset_flag(enic->vdev, 0);

	LIST_INIT(&enic->flows);
	rte_spinlock_init(&enic->flows_lock);
	enic->max_flow_counter = -1;

	/* set up link status checking */
	vnic_dev_notify_set(enic->vdev, -1); /* No Intr for notify */

	enic->overlay_offload = false;
	if (enic->disable_overlay && enic->vxlan) {
		/*
		 * Explicitly disable overlay offload as the setting is
		 * sticky, and resetting vNIC does not disable it.
		 */
		if (vnic_dev_overlay_offload_ctrl(enic->vdev,
						  OVERLAY_FEATURE_VXLAN,
						  OVERLAY_OFFLOAD_DISABLE)) {
			dev_err(enic, "failed to disable overlay offload\n");
		} else {
			dev_info(enic, "Overlay offload is disabled\n");
		}
	}
	if (!enic->disable_overlay && enic->vxlan &&
	    /* 'VXLAN feature' enables VXLAN, NVGRE, and GENEVE. */
	    vnic_dev_overlay_offload_ctrl(enic->vdev,
					  OVERLAY_FEATURE_VXLAN,
					  OVERLAY_OFFLOAD_ENABLE) == 0) {
		enic->tx_offload_capa |=
			DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM |
			DEV_TX_OFFLOAD_GENEVE_TNL_TSO |
			DEV_TX_OFFLOAD_VXLAN_TNL_TSO;
		enic->tx_offload_mask |=
			PKT_TX_OUTER_IPV6 |
			PKT_TX_OUTER_IPV4 |
			PKT_TX_OUTER_IP_CKSUM |
			PKT_TX_TUNNEL_MASK;
		enic->overlay_offload = true;
		dev_info(enic, "Overlay offload is enabled\n");
	}
	/*
	 * Reset the vxlan port if HW vxlan parsing is available. It
	 * is always enabled regardless of overlay offload
	 * enable/disable.
	 */
	if (enic->vxlan) {
		enic->vxlan_port = ENIC_DEFAULT_VXLAN_PORT;
		/*
		 * Reset the vxlan port to the default, as the NIC firmware
		 * does not reset it automatically and keeps the old setting.
		 */
		if (vnic_dev_overlay_offload_cfg(enic->vdev,
						 OVERLAY_CFG_VXLAN_PORT_UPDATE,
						 ENIC_DEFAULT_VXLAN_PORT)) {
			dev_err(enic, "failed to update vxlan port\n");
			return -EINVAL;
		}
	}

	return 0;

}

int enic_probe(struct enic *enic)
{
	struct rte_pci_device *pdev = enic->pdev;
	int err = -1;

	dev_debug(enic, " Initializing ENIC PMD\n");

	/* if this is a secondary process the hardware is already initialized */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	enic->bar0.vaddr = (void *)pdev->mem_resource[0].addr;
	enic->bar0.len = pdev->mem_resource[0].len;

	/* Register vNIC device */
	enic->vdev = vnic_dev_register(NULL, enic, enic->pdev, &enic->bar0, 1);
	if (!enic->vdev) {
		dev_err(enic, "vNIC registration failed, aborting\n");
		goto err_out;
	}

	LIST_INIT(&enic->memzone_list);
	rte_spinlock_init(&enic->memzone_list_lock);

	vnic_register_cbacks(enic->vdev,
		enic_alloc_consistent,
		enic_free_consistent);

	/*
	 * Allocate the consistent memory for stats and counters upfront so
	 * both primary and secondary processes can access them.
	 */
	err = vnic_dev_alloc_stats_mem(enic->vdev);
	if (err) {
		dev_err(enic, "Failed to allocate cmd memory, aborting\n");
		goto err_out_unregister;
	}
	err = vnic_dev_alloc_counter_mem(enic->vdev);
	if (err) {
		dev_err(enic, "Failed to allocate counter memory, aborting\n");
		goto err_out_unregister;
	}

	/* Issue device open to get device in known state */
	err = enic_dev_open(enic);
	if (err) {
		dev_err(enic, "vNIC dev open failed, aborting\n");
		goto err_out_unregister;
	}

	/* Set ingress vlan rewrite mode before vnic initialization */
	dev_debug(enic, "Set ig_vlan_rewrite_mode=%u\n",
		  enic->ig_vlan_rewrite_mode);
	err = vnic_dev_set_ig_vlan_rewrite_mode(enic->vdev,
		enic->ig_vlan_rewrite_mode);
	if (err) {
		dev_err(enic,
			"Failed to set ingress vlan rewrite mode, aborting.\n");
		goto err_out_dev_close;
	}

	/* Issue device init to initialize the vnic-to-switch link.
	 * We'll start with carrier off and wait for link UP
	 * notification later to turn on carrier.  We don't need
	 * to wait here for the vnic-to-switch link initialization
	 * to complete; link UP notification is the indication that
	 * the process is complete.
	 */

	err = vnic_dev_init(enic->vdev, 0);
	if (err) {
		dev_err(enic, "vNIC dev init failed, aborting\n");
		goto err_out_dev_close;
	}

	err = enic_dev_init(enic);
	if (err) {
		dev_err(enic, "Device initialization failed, aborting\n");
		goto err_out_dev_close;
	}

	return 0;

err_out_dev_close:
	vnic_dev_close(enic->vdev);
err_out_unregister:
	vnic_dev_unregister(enic->vdev);
err_out:
	return err;
}

void enic_remove(struct enic *enic)
{
	enic_dev_deinit(enic);
	vnic_dev_close(enic->vdev);
	vnic_dev_unregister(enic->vdev);
}
