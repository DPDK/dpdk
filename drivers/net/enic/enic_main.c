/*
 * Copyright 2008-2014 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 *
 * Copyright (c) 2014, Cisco Systems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in
 * the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */
#ident "$Id$"

#include <stdio.h>

#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <libgen.h>

#include <rte_pci.h>
#include <rte_memzone.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_string_fns.h>
#include <rte_ethdev.h>

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

static inline struct rte_mbuf *
enic_rxmbuf_alloc(struct rte_mempool *mp)
{
	struct rte_mbuf *m;

	m = __rte_mbuf_raw_alloc(mp);
	__rte_mbuf_sanity_check_raw(m, 0);
	return m;
}

void enic_set_hdr_split_size(struct enic *enic, u16 split_hdr_size)
{
	vnic_set_hdr_split_size(enic->vdev, split_hdr_size);
}

static void enic_free_wq_buf(__rte_unused struct vnic_wq *wq, struct vnic_wq_buf *buf)
{
	struct rte_mbuf *mbuf = (struct rte_mbuf *)buf->os_buf;

	rte_mempool_put(mbuf->pool, mbuf);
	buf->os_buf = NULL;
}

static void enic_wq_free_buf(struct vnic_wq *wq,
	__rte_unused struct cq_desc *cq_desc,
	struct vnic_wq_buf *buf,
	__rte_unused void *opaque)
{
	enic_free_wq_buf(wq, buf);
}

static int enic_wq_service(struct vnic_dev *vdev, struct cq_desc *cq_desc,
	__rte_unused u8 type, u16 q_number, u16 completed_index, void *opaque)
{
	struct enic *enic = vnic_dev_priv(vdev);

	vnic_wq_service(&enic->wq[q_number], cq_desc,
		completed_index, enic_wq_free_buf,
		opaque);

	return 0;
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

	for (i = 0; i < enic->rq_count; i++) {
		error_status = vnic_rq_error_status(&enic->rq[i]);
		if (error_status)
			dev_err(enic, "RQ[%d] error_status %d\n", i,
				error_status);
	}
}

unsigned int enic_cleanup_wq(struct enic *enic, struct vnic_wq *wq)
{
	unsigned int cq = enic_cq_wq(enic, wq->index);

	/* Return the work done */
	return vnic_cq_service(&enic->cq[cq],
		-1 /*wq_work_to_do*/, enic_wq_service, NULL);
}


int enic_send_pkt(struct enic *enic, struct vnic_wq *wq,
	struct rte_mbuf *tx_pkt, unsigned short len,
	uint8_t sop, uint8_t eop,
	uint16_t ol_flags, uint16_t vlan_tag)
{
	struct wq_enet_desc *desc = vnic_wq_next_desc(wq);
	uint16_t mss = 0;
	uint8_t cq_entry = eop;
	uint8_t vlan_tag_insert = 0;
	uint64_t bus_addr = (dma_addr_t)
	    (tx_pkt->buf_physaddr + RTE_PKTMBUF_HEADROOM);

	if (sop) {
		if (ol_flags & PKT_TX_VLAN_PKT)
			vlan_tag_insert = 1;

		if (enic->hw_ip_checksum) {
			if (ol_flags & PKT_TX_IP_CKSUM)
				mss |= ENIC_CALC_IP_CKSUM;

			if (ol_flags & PKT_TX_TCP_UDP_CKSUM)
				mss |= ENIC_CALC_TCP_UDP_CKSUM;
		}
	}

	wq_enet_desc_enc(desc,
		bus_addr,
		len,
		mss,
		0 /* header_length */,
		0 /* offload_mode WQ_ENET_OFFLOAD_MODE_CSUM */,
		eop,
		cq_entry,
		0 /* fcoe_encap */,
		vlan_tag_insert,
		vlan_tag,
		0 /* loopback */);

	vnic_wq_post(wq, (void *)tx_pkt, bus_addr, len,
		sop, eop,
		1 /*desc_skip_cnt*/,
		cq_entry,
		0 /*compressed send*/,
		0 /*wrid*/);

	return 0;
}

void enic_dev_stats_clear(struct enic *enic)
{
	if (vnic_dev_stats_clear(enic->vdev))
		dev_err(enic, "Error in clearing stats\n");
}

void enic_dev_stats_get(struct enic *enic, struct rte_eth_stats *r_stats)
{
	struct vnic_stats *stats;

	if (vnic_dev_stats_dump(enic->vdev, &stats)) {
		dev_err(enic, "Error in getting stats\n");
		return;
	}

	r_stats->ipackets = stats->rx.rx_frames_ok;
	r_stats->opackets = stats->tx.tx_frames_ok;

	r_stats->ibytes = stats->rx.rx_bytes_ok;
	r_stats->obytes = stats->tx.tx_bytes_ok;

	r_stats->ierrors = stats->rx.rx_errors;
	r_stats->oerrors = stats->tx.tx_errors;

	r_stats->imcasts = stats->rx.rx_multicast_frames_ok;
	r_stats->rx_nombuf = stats->rx.rx_no_bufs;
}

void enic_del_mac_address(struct enic *enic)
{
	if (vnic_dev_del_addr(enic->vdev, enic->mac_addr))
		dev_err(enic, "del mac addr failed\n");
}

void enic_set_mac_address(struct enic *enic, uint8_t *mac_addr)
{
	int err;

	if (!is_eth_addr_valid(mac_addr)) {
		dev_err(enic, "invalid mac address\n");
		return;
	}

	err = vnic_dev_del_addr(enic->vdev, mac_addr);
	if (err) {
		dev_err(enic, "del mac addr failed\n");
		return;
	}

	ether_addr_copy((struct ether_addr *)mac_addr,
		(struct ether_addr *)enic->mac_addr);

	err = vnic_dev_add_addr(enic->vdev, mac_addr);
	if (err) {
		dev_err(enic, "add mac addr failed\n");
		return;
	}
}

static void
enic_free_rq_buf(__rte_unused struct vnic_rq *rq, struct vnic_rq_buf *buf)
{
	if (!buf->os_buf)
		return;

	rte_pktmbuf_free((struct rte_mbuf *)buf->os_buf);
	buf->os_buf = NULL;
}

void enic_init_vnic_resources(struct enic *enic)
{
	unsigned int error_interrupt_enable = 1;
	unsigned int error_interrupt_offset = 0;
	unsigned int index = 0;

	for (index = 0; index < enic->rq_count; index++) {
		vnic_rq_init(&enic->rq[index],
			enic_cq_rq(enic, index),
			error_interrupt_enable,
			error_interrupt_offset);
	}

	for (index = 0; index < enic->wq_count; index++) {
		vnic_wq_init(&enic->wq[index],
			enic_cq_wq(enic, index),
			error_interrupt_enable,
			error_interrupt_offset);
	}

	vnic_dev_stats_clear(enic->vdev);

	for (index = 0; index < enic->cq_count; index++) {
		vnic_cq_init(&enic->cq[index],
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
	}

	vnic_intr_init(&enic->intr,
		enic->config.intr_timer_usec,
		enic->config.intr_timer_type,
		/*mask_on_assertion*/1);
}


static int enic_rq_alloc_buf(struct vnic_rq *rq)
{
	struct enic *enic = vnic_dev_priv(rq->vdev);
	dma_addr_t dma_addr;
	struct rq_enet_desc *desc = vnic_rq_next_desc(rq);
	uint8_t type = RQ_ENET_TYPE_ONLY_SOP;
	u16 split_hdr_size = vnic_get_hdr_split_size(enic->vdev);
	struct rte_mbuf *mbuf = enic_rxmbuf_alloc(rq->mp);
	struct rte_mbuf *hdr_mbuf = NULL;

	if (!mbuf) {
		dev_err(enic, "mbuf alloc in enic_rq_alloc_buf failed\n");
		return -1;
	}

	if (unlikely(split_hdr_size)) {
		if (vnic_rq_desc_avail(rq) < 2) {
			rte_mempool_put(mbuf->pool, mbuf);
			return -1;
		}
		hdr_mbuf = enic_rxmbuf_alloc(rq->mp);
		if (!hdr_mbuf) {
			rte_mempool_put(mbuf->pool, mbuf);
			dev_err(enic,
				"hdr_mbuf alloc in enic_rq_alloc_buf failed\n");
			return -1;
		}

		hdr_mbuf->data_off = RTE_PKTMBUF_HEADROOM;

		hdr_mbuf->nb_segs = 2;
		hdr_mbuf->port = enic->port_id;
		hdr_mbuf->next = mbuf;

		dma_addr = (dma_addr_t)
		    (hdr_mbuf->buf_physaddr + hdr_mbuf->data_off);

		rq_enet_desc_enc(desc, dma_addr, type, split_hdr_size);

		vnic_rq_post(rq, (void *)hdr_mbuf, 0 /*os_buf_index*/, dma_addr,
			(unsigned int)split_hdr_size, 0 /*wrid*/);

		desc = vnic_rq_next_desc(rq);
		type = RQ_ENET_TYPE_NOT_SOP;
	} else {
		mbuf->nb_segs = 1;
		mbuf->port = enic->port_id;
	}

	mbuf->data_off = RTE_PKTMBUF_HEADROOM;
	mbuf->next = NULL;

	dma_addr = (dma_addr_t)
	    (mbuf->buf_physaddr + mbuf->data_off);

	rq_enet_desc_enc(desc, dma_addr, type, mbuf->buf_len);

	vnic_rq_post(rq, (void *)mbuf, 0 /*os_buf_index*/, dma_addr,
		(unsigned int)mbuf->buf_len, 0 /*wrid*/);

	return 0;
}

static int enic_rq_indicate_buf(struct vnic_rq *rq,
	struct cq_desc *cq_desc, struct vnic_rq_buf *buf,
	int skipped, void *opaque)
{
	struct enic *enic = vnic_dev_priv(rq->vdev);
	struct rte_mbuf **rx_pkt_bucket = (struct rte_mbuf **)opaque;
	struct rte_mbuf *rx_pkt = NULL;
	struct rte_mbuf *hdr_rx_pkt = NULL;

	u8 type, color, eop, sop, ingress_port, vlan_stripped;
	u8 fcoe, fcoe_sof, fcoe_fc_crc_ok, fcoe_enc_error, fcoe_eof;
	u8 tcp_udp_csum_ok, udp, tcp, ipv4_csum_ok;
	u8 ipv6, ipv4, ipv4_fragment, fcs_ok, rss_type, csum_not_calc;
	u8 packet_error;
	u16 q_number, completed_index, bytes_written, vlan_tci, checksum;
	u32 rss_hash;

	cq_enet_rq_desc_dec((struct cq_enet_rq_desc *)cq_desc,
		&type, &color, &q_number, &completed_index,
		&ingress_port, &fcoe, &eop, &sop, &rss_type,
		&csum_not_calc, &rss_hash, &bytes_written,
		&packet_error, &vlan_stripped, &vlan_tci, &checksum,
		&fcoe_sof, &fcoe_fc_crc_ok, &fcoe_enc_error,
		&fcoe_eof, &tcp_udp_csum_ok, &udp, &tcp,
		&ipv4_csum_ok, &ipv6, &ipv4, &ipv4_fragment,
		&fcs_ok);

	rx_pkt = (struct rte_mbuf *)buf->os_buf;
	buf->os_buf = NULL;

	if (unlikely(packet_error)) {
		dev_err(enic, "packet error\n");
		rx_pkt->data_len = 0;
		return 0;
	}

	if (unlikely(skipped)) {
		rx_pkt->data_len = 0;
		return 0;
	}

	if (likely(!vnic_get_hdr_split_size(enic->vdev))) {
		/* No header split configured */
		*rx_pkt_bucket = rx_pkt;
		rx_pkt->pkt_len = bytes_written;

		if (ipv4) {
#ifdef RTE_NEXT_ABI
			rx_pkt->packet_type = RTE_PTYPE_L3_IPV4;
#else
			rx_pkt->ol_flags |= PKT_RX_IPV4_HDR;
#endif
			if (!csum_not_calc) {
				if (unlikely(!ipv4_csum_ok))
					rx_pkt->ol_flags |= PKT_RX_IP_CKSUM_BAD;

				if ((tcp || udp) && (!tcp_udp_csum_ok))
					rx_pkt->ol_flags |= PKT_RX_L4_CKSUM_BAD;
			}
		} else if (ipv6)
#ifdef RTE_NEXT_ABI
			rx_pkt->packet_type = RTE_PTYPE_L3_IPV6;
#else
			rx_pkt->ol_flags |= PKT_RX_IPV6_HDR;
#endif
	} else {
		/* Header split */
		if (sop && !eop) {
			/* This piece is header */
			*rx_pkt_bucket = rx_pkt;
			rx_pkt->pkt_len = bytes_written;
		} else {
			if (sop && eop) {
				/* The packet is smaller than split_hdr_size */
				*rx_pkt_bucket = rx_pkt;
				rx_pkt->pkt_len = bytes_written;
				if (ipv4) {
#ifdef RTE_NEXT_ABI
					rx_pkt->packet_type = RTE_PTYPE_L3_IPV4;
#else
					rx_pkt->ol_flags |= PKT_RX_IPV4_HDR;
#endif
					if (!csum_not_calc) {
						if (unlikely(!ipv4_csum_ok))
							rx_pkt->ol_flags |=
							    PKT_RX_IP_CKSUM_BAD;

						if ((tcp || udp) &&
						    (!tcp_udp_csum_ok))
							rx_pkt->ol_flags |=
							    PKT_RX_L4_CKSUM_BAD;
					}
				} else if (ipv6)
#ifdef RTE_NEXT_ABI
					rx_pkt->packet_type = RTE_PTYPE_L3_IPV6;
#else
					rx_pkt->ol_flags |= PKT_RX_IPV6_HDR;
#endif
			} else {
				/* Payload */
				hdr_rx_pkt = *rx_pkt_bucket;
				hdr_rx_pkt->pkt_len += bytes_written;
				if (ipv4) {
#ifdef RTE_NEXT_ABI
					hdr_rx_pkt->packet_type =
						RTE_PTYPE_L3_IPV4;
#else
					hdr_rx_pkt->ol_flags |= PKT_RX_IPV4_HDR;
#endif
					if (!csum_not_calc) {
						if (unlikely(!ipv4_csum_ok))
							hdr_rx_pkt->ol_flags |=
							    PKT_RX_IP_CKSUM_BAD;

						if ((tcp || udp) &&
						    (!tcp_udp_csum_ok))
							hdr_rx_pkt->ol_flags |=
							    PKT_RX_L4_CKSUM_BAD;
					}
				} else if (ipv6)
#ifdef RTE_NEXT_ABI
					hdr_rx_pkt->packet_type =
						RTE_PTYPE_L3_IPV6;
#else
					hdr_rx_pkt->ol_flags |= PKT_RX_IPV6_HDR;
#endif

			}
		}
	}

	rx_pkt->data_len = bytes_written;

	if (rss_hash) {
		rx_pkt->ol_flags |= PKT_RX_RSS_HASH;
		rx_pkt->hash.rss = rss_hash;
	}

	if (vlan_tci) {
		rx_pkt->ol_flags |= PKT_RX_VLAN_PKT;
		rx_pkt->vlan_tci = vlan_tci;
	}

	return eop;
}

static int enic_rq_service(struct vnic_dev *vdev, struct cq_desc *cq_desc,
	__rte_unused u8 type, u16 q_number, u16 completed_index, void *opaque)
{
	struct enic *enic = vnic_dev_priv(vdev);

	return vnic_rq_service(&enic->rq[q_number], cq_desc,
		completed_index, VNIC_RQ_RETURN_DESC,
		enic_rq_indicate_buf, opaque);

}

int enic_poll(struct vnic_rq *rq, struct rte_mbuf **rx_pkts,
	unsigned int budget, unsigned int *work_done)
{
	struct enic *enic = vnic_dev_priv(rq->vdev);
	unsigned int cq = enic_cq_rq(enic, rq->index);
	int err = 0;

	*work_done = vnic_cq_service(&enic->cq[cq],
		budget, enic_rq_service, (void *)rx_pkts);

	if (*work_done) {
		vnic_rq_fill(rq, enic_rq_alloc_buf);

		/* Need at least one buffer on ring to get going */
		if (vnic_rq_desc_used(rq) == 0) {
			dev_err(enic, "Unable to alloc receive buffers\n");
			err = -1;
		}
	}
	return err;
}

static void *
enic_alloc_consistent(__rte_unused void *priv, size_t size,
	dma_addr_t *dma_handle, u8 *name)
{
	void *vaddr;
	const struct rte_memzone *rz;
	*dma_handle = 0;

	rz = rte_memzone_reserve_aligned((const char *)name,
		size, 0, 0, ENIC_ALIGN);
	if (!rz) {
		pr_err("%s : Failed to allocate memory requested for %s",
			__func__, name);
		return NULL;
	}

	vaddr = rz->addr;
	*dma_handle = (dma_addr_t)rz->phys_addr;

	return vaddr;
}

static void
enic_free_consistent(__rte_unused struct rte_pci_device *hwdev,
	__rte_unused size_t size,
	__rte_unused void *vaddr,
	__rte_unused dma_addr_t dma_handle)
{
	/* Nothing to be done */
}

static void
enic_intr_handler(__rte_unused struct rte_intr_handle *handle,
	void *arg)
{
	struct enic *enic = pmd_priv((struct rte_eth_dev *)arg);

	vnic_intr_return_all_credits(&enic->intr);

	enic_log_q_error(enic);
}

int enic_enable(struct enic *enic)
{
	unsigned int index;
	struct rte_eth_dev *eth_dev = enic->rte_dev;

	eth_dev->data->dev_link.link_speed = vnic_dev_port_speed(enic->vdev);
	eth_dev->data->dev_link.link_duplex = ETH_LINK_FULL_DUPLEX;
	vnic_dev_notify_set(enic->vdev, -1); /* No Intr for notify */

	if (enic_clsf_init(enic))
		dev_warning(enic, "Init of hash table for clsf failed."\
			"Flow director feature will not work\n");

	/* Fill RQ bufs */
	for (index = 0; index < enic->rq_count; index++) {
		vnic_rq_fill(&enic->rq[index], enic_rq_alloc_buf);

		/* Need at least one buffer on ring to get going
		*/
		if (vnic_rq_desc_used(&enic->rq[index]) == 0) {
			dev_err(enic, "Unable to alloc receive buffers\n");
			return -1;
		}
	}

	for (index = 0; index < enic->wq_count; index++)
		vnic_wq_enable(&enic->wq[index]);
	for (index = 0; index < enic->rq_count; index++)
		vnic_rq_enable(&enic->rq[index]);

	vnic_dev_enable_wait(enic->vdev);

	/* Register and enable error interrupt */
	rte_intr_callback_register(&(enic->pdev->intr_handle),
		enic_intr_handler, (void *)enic->rte_dev);

	rte_intr_enable(&(enic->pdev->intr_handle));
	vnic_intr_unmask(&enic->intr);

	return 0;
}

int enic_alloc_intr_resources(struct enic *enic)
{
	int err;

	dev_info(enic, "vNIC resources used:  "\
		"wq %d rq %d cq %d intr %d\n",
		enic->wq_count, enic->rq_count,
		enic->cq_count, enic->intr_count);

	err = vnic_intr_alloc(enic->vdev, &enic->intr, 0);
	if (err)
		enic_free_vnic_resources(enic);

	return err;
}

void enic_free_rq(void *rxq)
{
	struct vnic_rq *rq = (struct vnic_rq *)rxq;
	struct enic *enic = vnic_dev_priv(rq->vdev);

	vnic_rq_free(rq);
	vnic_cq_free(&enic->cq[rq->index]);
}

void enic_start_wq(struct enic *enic, uint16_t queue_idx)
{
	vnic_wq_enable(&enic->wq[queue_idx]);
}

int enic_stop_wq(struct enic *enic, uint16_t queue_idx)
{
	return vnic_wq_disable(&enic->wq[queue_idx]);
}

void enic_start_rq(struct enic *enic, uint16_t queue_idx)
{
	vnic_rq_enable(&enic->rq[queue_idx]);
}

int enic_stop_rq(struct enic *enic, uint16_t queue_idx)
{
	return vnic_rq_disable(&enic->rq[queue_idx]);
}

int enic_alloc_rq(struct enic *enic, uint16_t queue_idx,
	unsigned int socket_id, struct rte_mempool *mp,
	uint16_t nb_desc)
{
	int err;
	struct vnic_rq *rq = &enic->rq[queue_idx];

	rq->socket_id = socket_id;
	rq->mp = mp;

	if (nb_desc) {
		if (nb_desc > enic->config.rq_desc_count) {
			dev_warning(enic,
				"RQ %d - number of rx desc in cmd line (%d)"\
				"is greater than that in the UCSM/CIMC adapter"\
				"policy.  Applying the value in the adapter "\
				"policy (%d).\n",
				queue_idx, nb_desc, enic->config.rq_desc_count);
		} else if (nb_desc != enic->config.rq_desc_count) {
			enic->config.rq_desc_count = nb_desc;
			dev_info(enic,
				"RX Queues - effective number of descs:%d\n",
				nb_desc);
		}
	}

	/* Allocate queue resources */
	err = vnic_rq_alloc(enic->vdev, &enic->rq[queue_idx], queue_idx,
		enic->config.rq_desc_count,
		sizeof(struct rq_enet_desc));
	if (err) {
		dev_err(enic, "error in allocation of rq\n");
		return err;
	}

	err = vnic_cq_alloc(enic->vdev, &enic->cq[queue_idx], queue_idx,
		socket_id, enic->config.rq_desc_count,
		sizeof(struct cq_enet_rq_desc));
	if (err) {
		vnic_rq_free(rq);
		dev_err(enic, "error in allocation of cq for rq\n");
	}

	return err;
}

void enic_free_wq(void *txq)
{
	struct vnic_wq *wq = (struct vnic_wq *)txq;
	struct enic *enic = vnic_dev_priv(wq->vdev);

	vnic_wq_free(wq);
	vnic_cq_free(&enic->cq[enic->rq_count + wq->index]);
}

int enic_alloc_wq(struct enic *enic, uint16_t queue_idx,
	unsigned int socket_id, uint16_t nb_desc)
{
	int err;
	struct vnic_wq *wq = &enic->wq[queue_idx];
	unsigned int cq_index = enic_cq_wq(enic, queue_idx);

	wq->socket_id = socket_id;
	if (nb_desc) {
		if (nb_desc > enic->config.wq_desc_count) {
			dev_warning(enic,
				"WQ %d - number of tx desc in cmd line (%d)"\
				"is greater than that in the UCSM/CIMC adapter"\
				"policy.  Applying the value in the adapter "\
				"policy (%d)\n",
				queue_idx, nb_desc, enic->config.wq_desc_count);
		} else if (nb_desc != enic->config.wq_desc_count) {
			enic->config.wq_desc_count = nb_desc;
			dev_info(enic,
				"TX Queues - effective number of descs:%d\n",
				nb_desc);
		}
	}

	/* Allocate queue resources */
	err = vnic_wq_alloc(enic->vdev, &enic->wq[queue_idx], queue_idx,
		enic->config.wq_desc_count,
		sizeof(struct wq_enet_desc));
	if (err) {
		dev_err(enic, "error in allocation of wq\n");
		return err;
	}

	err = vnic_cq_alloc(enic->vdev, &enic->cq[cq_index], cq_index,
		socket_id, enic->config.wq_desc_count,
		sizeof(struct cq_enet_wq_desc));
	if (err) {
		vnic_wq_free(wq);
		dev_err(enic, "error in allocation of cq for wq\n");
	}

	return err;
}

int enic_disable(struct enic *enic)
{
	unsigned int i;
	int err;

	vnic_intr_mask(&enic->intr);
	(void)vnic_intr_masked(&enic->intr); /* flush write */

	vnic_dev_disable(enic->vdev);

	enic_clsf_destroy(enic);

	if (!enic_is_sriov_vf(enic))
		vnic_dev_del_addr(enic->vdev, enic->mac_addr);

	for (i = 0; i < enic->wq_count; i++) {
		err = vnic_wq_disable(&enic->wq[i]);
		if (err)
			return err;
	}
	for (i = 0; i < enic->rq_count; i++) {
		err = vnic_rq_disable(&enic->rq[i]);
		if (err)
			return err;
	}

	vnic_dev_set_reset_flag(enic->vdev, 1);
	vnic_dev_notify_unset(enic->vdev);

	for (i = 0; i < enic->wq_count; i++)
		vnic_wq_clean(&enic->wq[i], enic_free_wq_buf);
	for (i = 0; i < enic->rq_count; i++)
		vnic_rq_clean(&enic->rq[i], enic_free_rq_buf);
	for (i = 0; i < enic->cq_count; i++)
		vnic_cq_clean(&enic->cq[i]);
	vnic_intr_clean(&enic->intr);

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

	err = enic_dev_wait(enic->vdev, vnic_dev_open,
		vnic_dev_open_done, 0);
	if (err)
		dev_err(enic_get_dev(enic),
			"vNIC device open failed, err %d\n", err);

	return err;
}

static int enic_set_rsskey(struct enic *enic)
{
	dma_addr_t rss_key_buf_pa;
	union vnic_rss_key *rss_key_buf_va = NULL;
	static union vnic_rss_key rss_key = {
		.key = {
			[0] = {.b = {85, 67, 83, 97, 119, 101, 115, 111, 109, 101}},
			[1] = {.b = {80, 65, 76, 79, 117, 110, 105, 113, 117, 101}},
			[2] = {.b = {76, 73, 78, 85, 88, 114, 111, 99, 107, 115}},
			[3] = {.b = {69, 78, 73, 67, 105, 115, 99, 111, 111, 108}},
		}
	};
	int err;
	u8 name[NAME_MAX];

	snprintf((char *)name, NAME_MAX, "rss_key-%s", enic->bdf_name);
	rss_key_buf_va = enic_alloc_consistent(enic, sizeof(union vnic_rss_key),
		&rss_key_buf_pa, name);
	if (!rss_key_buf_va)
		return -ENOMEM;

	rte_memcpy(rss_key_buf_va, &rss_key, sizeof(union vnic_rss_key));

	err = enic_set_rss_key(enic,
		rss_key_buf_pa,
		sizeof(union vnic_rss_key));

	enic_free_consistent(enic->pdev, sizeof(union vnic_rss_key),
		rss_key_buf_va, rss_key_buf_pa);

	return err;
}

static int enic_set_rsscpu(struct enic *enic, u8 rss_hash_bits)
{
	dma_addr_t rss_cpu_buf_pa;
	union vnic_rss_cpu *rss_cpu_buf_va = NULL;
	int i;
	int err;
	u8 name[NAME_MAX];

	snprintf((char *)name, NAME_MAX, "rss_cpu-%s", enic->bdf_name);
	rss_cpu_buf_va = enic_alloc_consistent(enic, sizeof(union vnic_rss_cpu),
		&rss_cpu_buf_pa, name);
	if (!rss_cpu_buf_va)
		return -ENOMEM;

	for (i = 0; i < (1 << rss_hash_bits); i++)
		(*rss_cpu_buf_va).cpu[i/4].b[i%4] = i % enic->rq_count;

	err = enic_set_rss_cpu(enic,
		rss_cpu_buf_pa,
		sizeof(union vnic_rss_cpu));

	enic_free_consistent(enic->pdev, sizeof(union vnic_rss_cpu),
		rss_cpu_buf_va, rss_cpu_buf_pa);

	return err;
}

static int enic_set_niccfg(struct enic *enic, u8 rss_default_cpu,
	u8 rss_hash_type, u8 rss_hash_bits, u8 rss_base_cpu, u8 rss_enable)
{
	const u8 tso_ipid_split_en = 0;
	int err;

	/* Enable VLAN tag stripping */

	err = enic_set_nic_cfg(enic,
		rss_default_cpu, rss_hash_type,
		rss_hash_bits, rss_base_cpu,
		rss_enable, tso_ipid_split_en,
		enic->ig_vlan_strip_en);

	return err;
}

int enic_set_rss_nic_cfg(struct enic *enic)
{
	const u8 rss_default_cpu = 0;
	const u8 rss_hash_type = NIC_CFG_RSS_HASH_TYPE_IPV4 |
	    NIC_CFG_RSS_HASH_TYPE_TCP_IPV4 |
	    NIC_CFG_RSS_HASH_TYPE_IPV6 |
	    NIC_CFG_RSS_HASH_TYPE_TCP_IPV6;
	const u8 rss_hash_bits = 7;
	const u8 rss_base_cpu = 0;
	u8 rss_enable = ENIC_SETTING(enic, RSS) && (enic->rq_count > 1);

	if (rss_enable) {
		if (!enic_set_rsskey(enic)) {
			if (enic_set_rsscpu(enic, rss_hash_bits)) {
				rss_enable = 0;
				dev_warning(enic, "RSS disabled, "\
					"Failed to set RSS cpu indirection table.");
			}
		} else {
			rss_enable = 0;
			dev_warning(enic,
				"RSS disabled, Failed to set RSS key.\n");
		}
	}

	return enic_set_niccfg(enic, rss_default_cpu, rss_hash_type,
		rss_hash_bits, rss_base_cpu, rss_enable);
}

int enic_setup_finish(struct enic *enic)
{
	int ret;

	ret = enic_set_rss_nic_cfg(enic);
	if (ret) {
		dev_err(enic, "Failed to config nic, aborting.\n");
		return -1;
	}

	vnic_dev_add_addr(enic->vdev, enic->mac_addr);

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
	struct rte_eth_dev *eth_dev = enic->rte_dev;

	rte_free(eth_dev->data->mac_addrs);
}


int enic_set_vnic_res(struct enic *enic)
{
	struct rte_eth_dev *eth_dev = enic->rte_dev;

	if ((enic->rq_count < eth_dev->data->nb_rx_queues) ||
		(enic->wq_count < eth_dev->data->nb_tx_queues)) {
		dev_err(dev, "Not enough resources configured, aborting\n");
		return -1;
	}

	enic->rq_count = eth_dev->data->nb_rx_queues;
	enic->wq_count = eth_dev->data->nb_tx_queues;
	if (enic->cq_count < (enic->rq_count + enic->wq_count)) {
		dev_err(dev, "Not enough resources configured, aborting\n");
		return -1;
	}

	enic->cq_count = enic->rq_count + enic->wq_count;
	return 0;
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

	eth_dev->data->mac_addrs = rte_zmalloc("enic_mac_addr", ETH_ALEN, 0);
	if (!eth_dev->data->mac_addrs) {
		dev_err(enic, "mac addr storage alloc failed, aborting.\n");
		return -1;
	}
	ether_addr_copy((struct ether_addr *) enic->mac_addr,
		&eth_dev->data->mac_addrs[0]);


	/* Get available resource counts
	*/
	enic_get_res_counts(enic);

	vnic_dev_set_reset_flag(enic->vdev, 0);

	return 0;

}

int enic_probe(struct enic *enic)
{
	struct rte_pci_device *pdev = enic->pdev;
	int err = -1;

	dev_debug(enic, " Initializing ENIC PMD version %s\n", DRV_VERSION);

	enic->bar0.vaddr = (void *)pdev->mem_resource[0].addr;
	enic->bar0.len = pdev->mem_resource[0].len;

	/* Register vNIC device */
	enic->vdev = vnic_dev_register(NULL, enic, enic->pdev, &enic->bar0, 1);
	if (!enic->vdev) {
		dev_err(enic, "vNIC registration failed, aborting\n");
		goto err_out;
	}

	vnic_register_cbacks(enic->vdev,
		enic_alloc_consistent,
		enic_free_consistent);

	/* Issue device open to get device in known state */
	err = enic_dev_open(enic);
	if (err) {
		dev_err(enic, "vNIC dev open failed, aborting\n");
		goto err_out_unregister;
	}

	/* Set ingress vlan rewrite mode before vnic initialization */
	err = vnic_dev_set_ig_vlan_rewrite_mode(enic->vdev,
		IG_VLAN_REWRITE_MODE_PRIORITY_TAG_DEFAULT_VLAN);
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
