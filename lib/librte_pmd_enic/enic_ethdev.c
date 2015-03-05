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
#include <stdint.h>

#include <rte_dev.h>
#include <rte_pci.h>
#include <rte_ethdev.h>
#include <rte_string_fns.h>

#include "vnic_intr.h"
#include "vnic_cq.h"
#include "vnic_wq.h"
#include "vnic_rq.h"
#include "vnic_enet.h"
#include "enic.h"

#define ENICPMD_FUNC_TRACE() \
	RTE_LOG(DEBUG, PMD, "ENICPMD trace: %s\n", __func__)

/*
 * The set of PCI devices this driver supports
 */
static struct rte_pci_id pci_id_enic_map[] = {
#define RTE_PCI_DEV_ID_DECL_ENIC(vend, dev) {RTE_PCI_DEVICE(vend, dev)},
#ifndef PCI_VENDOR_ID_CISCO
#define PCI_VENDOR_ID_CISCO	0x1137
#endif
#include "rte_pci_dev_ids.h"
RTE_PCI_DEV_ID_DECL_ENIC(PCI_VENDOR_ID_CISCO, PCI_DEVICE_ID_CISCO_VIC_ENET)
RTE_PCI_DEV_ID_DECL_ENIC(PCI_VENDOR_ID_CISCO, PCI_DEVICE_ID_CISCO_VIC_ENET_VF)
{.vendor_id = 0, /* Sentinal */},
};

static int enicpmd_fdir_remove_perfect_filter(struct rte_eth_dev *eth_dev,
		struct rte_fdir_filter *fdir_filter,
		__rte_unused uint16_t soft_id)
{
	struct enic *enic = pmd_priv(eth_dev);

	ENICPMD_FUNC_TRACE();
	return enic_fdir_del_fltr(enic, fdir_filter);
}

static int enicpmd_fdir_add_perfect_filter(struct rte_eth_dev *eth_dev,
	struct rte_fdir_filter *fdir_filter, __rte_unused uint16_t soft_id,
	uint8_t queue, uint8_t drop)
{
	struct enic *enic = pmd_priv(eth_dev);

	ENICPMD_FUNC_TRACE();
	return enic_fdir_add_fltr(enic, fdir_filter, (uint16_t)queue, drop);
}

static void enicpmd_fdir_info_get(struct rte_eth_dev *eth_dev,
	struct rte_eth_fdir *fdir)
{
	struct enic *enic = pmd_priv(eth_dev);

	ENICPMD_FUNC_TRACE();
	*fdir = enic->fdir.stats;
}

static void enicpmd_dev_tx_queue_release(void *txq)
{
	ENICPMD_FUNC_TRACE();
	enic_free_wq(txq);
}

static int enicpmd_dev_setup_intr(struct enic *enic)
{
	int ret;
	unsigned int index;

	ENICPMD_FUNC_TRACE();

	/* Are we done with the init of all the queues? */
	for (index = 0; index < enic->cq_count; index++) {
		if (!enic->cq[index].ctrl)
			break;
	}

	if (enic->cq_count != index)
		return 0;

	ret = enic_alloc_intr_resources(enic);
	if (ret) {
		dev_err(enic, "alloc intr failed\n");
		return ret;
	}
	enic_init_vnic_resources(enic);

	ret = enic_setup_finish(enic);
	if (ret)
		dev_err(enic, "setup could not be finished\n");

	return ret;
}

static int enicpmd_dev_tx_queue_setup(struct rte_eth_dev *eth_dev,
	uint16_t queue_idx,
	uint16_t nb_desc,
	unsigned int socket_id,
	__rte_unused const struct rte_eth_txconf *tx_conf)
{
	int ret;
	struct enic *enic = pmd_priv(eth_dev);

	ENICPMD_FUNC_TRACE();
	eth_dev->data->tx_queues[queue_idx] = (void *)&enic->wq[queue_idx];

	ret = enic_alloc_wq(enic, queue_idx, socket_id, nb_desc);
	if (ret) {
		dev_err(enic, "error in allocating wq\n");
		return ret;
	}

	return enicpmd_dev_setup_intr(enic);
}

static int enicpmd_dev_tx_queue_start(struct rte_eth_dev *eth_dev,
	uint16_t queue_idx)
{
	struct enic *enic = pmd_priv(eth_dev);

	ENICPMD_FUNC_TRACE();

	enic_start_wq(enic, queue_idx);

	return 0;
}

static int enicpmd_dev_tx_queue_stop(struct rte_eth_dev *eth_dev,
	uint16_t queue_idx)
{
	int ret;
	struct enic *enic = pmd_priv(eth_dev);

	ENICPMD_FUNC_TRACE();

	ret = enic_stop_wq(enic, queue_idx);
	if (ret)
		dev_err(enic, "error in stopping wq %d\n", queue_idx);

	return ret;
}

static int enicpmd_dev_rx_queue_start(struct rte_eth_dev *eth_dev,
	uint16_t queue_idx)
{
	struct enic *enic = pmd_priv(eth_dev);

	ENICPMD_FUNC_TRACE();

	enic_start_rq(enic, queue_idx);

	return 0;
}

static int enicpmd_dev_rx_queue_stop(struct rte_eth_dev *eth_dev,
	uint16_t queue_idx)
{
	int ret;
	struct enic *enic = pmd_priv(eth_dev);

	ENICPMD_FUNC_TRACE();

	ret = enic_stop_rq(enic, queue_idx);
	if (ret)
		dev_err(enic, "error in stopping rq %d\n", queue_idx);

	return ret;
}

static void enicpmd_dev_rx_queue_release(void *rxq)
{
	ENICPMD_FUNC_TRACE();
	enic_free_rq(rxq);
}

static int enicpmd_dev_rx_queue_setup(struct rte_eth_dev *eth_dev,
	uint16_t queue_idx,
	uint16_t nb_desc,
	unsigned int socket_id,
	__rte_unused const struct rte_eth_rxconf *rx_conf,
	struct rte_mempool *mp)
{
	int ret;
	struct enic *enic = pmd_priv(eth_dev);

	ENICPMD_FUNC_TRACE();
	eth_dev->data->rx_queues[queue_idx] = (void *)&enic->rq[queue_idx];

	ret = enic_alloc_rq(enic, queue_idx, socket_id, mp, nb_desc);
	if (ret) {
		dev_err(enic, "error in allocating rq\n");
		return ret;
	}

	return enicpmd_dev_setup_intr(enic);
}

static int enicpmd_vlan_filter_set(struct rte_eth_dev *eth_dev,
	uint16_t vlan_id, int on)
{
	struct enic *enic = pmd_priv(eth_dev);

	ENICPMD_FUNC_TRACE();
	if (on)
		enic_add_vlan(enic, vlan_id);
	else
		enic_del_vlan(enic, vlan_id);
	return 0;
}

static void enicpmd_vlan_offload_set(struct rte_eth_dev *eth_dev, int mask)
{
	struct enic *enic = pmd_priv(eth_dev);

	ENICPMD_FUNC_TRACE();

	if (mask & ETH_VLAN_STRIP_MASK) {
		if (eth_dev->data->dev_conf.rxmode.hw_vlan_strip)
			enic->ig_vlan_strip_en = 1;
		else
			enic->ig_vlan_strip_en = 0;
	}
	enic_set_rss_nic_cfg(enic);


	if (mask & ETH_VLAN_FILTER_MASK) {
		dev_warning(enic,
			"Configuration of VLAN filter is not supported\n");
	}

	if (mask & ETH_VLAN_EXTEND_MASK) {
		dev_warning(enic,
			"Configuration of extended VLAN is not supported\n");
	}
}

static int enicpmd_dev_configure(struct rte_eth_dev *eth_dev)
{
	int ret;
	struct enic *enic = pmd_priv(eth_dev);

	ENICPMD_FUNC_TRACE();
	ret = enic_set_vnic_res(enic);
	if (ret) {
		dev_err(enic, "Set vNIC resource num  failed, aborting\n");
		return ret;
	}

	if (eth_dev->data->dev_conf.rxmode.split_hdr_size &&
		eth_dev->data->dev_conf.rxmode.header_split) {
		/* Enable header-data-split */
		enic_set_hdr_split_size(enic,
			eth_dev->data->dev_conf.rxmode.split_hdr_size);
	}

	enic->hw_ip_checksum = eth_dev->data->dev_conf.rxmode.hw_ip_checksum;
	return 0;
}

/* Start the device.
 * It returns 0 on success.
 */
static int enicpmd_dev_start(struct rte_eth_dev *eth_dev)
{
	struct enic *enic = pmd_priv(eth_dev);

	ENICPMD_FUNC_TRACE();
	return enic_enable(enic);
}

/*
 * Stop device: disable rx and tx functions to allow for reconfiguring.
 */
static void enicpmd_dev_stop(struct rte_eth_dev *eth_dev)
{
	struct rte_eth_link link;
	struct enic *enic = pmd_priv(eth_dev);

	ENICPMD_FUNC_TRACE();
	enic_disable(enic);
	memset(&link, 0, sizeof(link));
	rte_atomic64_cmpset((uint64_t *)&eth_dev->data->dev_link,
		*(uint64_t *)&eth_dev->data->dev_link,
		*(uint64_t *)&link);
}

/*
 * Stop device.
 */
static void enicpmd_dev_close(struct rte_eth_dev *eth_dev)
{
	struct enic *enic = pmd_priv(eth_dev);

	ENICPMD_FUNC_TRACE();
	enic_remove(enic);
}

static int enicpmd_dev_link_update(struct rte_eth_dev *eth_dev,
	__rte_unused int wait_to_complete)
{
	struct enic *enic = pmd_priv(eth_dev);
	int ret;
	int link_status = 0;

	ENICPMD_FUNC_TRACE();
	link_status = enic_get_link_status(enic);
	ret = (link_status == enic->link_status);
	enic->link_status = link_status;
	eth_dev->data->dev_link.link_status = link_status;
	eth_dev->data->dev_link.link_duplex = ETH_LINK_FULL_DUPLEX;
	eth_dev->data->dev_link.link_speed = vnic_dev_port_speed(enic->vdev);
	return ret;
}

static void enicpmd_dev_stats_get(struct rte_eth_dev *eth_dev,
	struct rte_eth_stats *stats)
{
	struct enic *enic = pmd_priv(eth_dev);

	ENICPMD_FUNC_TRACE();
	enic_dev_stats_get(enic, stats);
}

static void enicpmd_dev_stats_reset(struct rte_eth_dev *eth_dev)
{
	struct enic *enic = pmd_priv(eth_dev);

	ENICPMD_FUNC_TRACE();
	enic_dev_stats_clear(enic);
}

static void enicpmd_dev_info_get(struct rte_eth_dev *eth_dev,
	struct rte_eth_dev_info *device_info)
{
	struct enic *enic = pmd_priv(eth_dev);

	ENICPMD_FUNC_TRACE();
	device_info->max_rx_queues = enic->rq_count;
	device_info->max_tx_queues = enic->wq_count;
	device_info->min_rx_bufsize = ENIC_MIN_MTU;
	device_info->max_rx_pktlen = enic->config.mtu;
	device_info->max_mac_addrs = 1;
	device_info->rx_offload_capa =
		DEV_RX_OFFLOAD_VLAN_STRIP |
		DEV_RX_OFFLOAD_IPV4_CKSUM |
		DEV_RX_OFFLOAD_UDP_CKSUM  |
		DEV_RX_OFFLOAD_TCP_CKSUM;
	device_info->tx_offload_capa =
		DEV_TX_OFFLOAD_VLAN_INSERT |
		DEV_TX_OFFLOAD_IPV4_CKSUM  |
		DEV_TX_OFFLOAD_UDP_CKSUM   |
		DEV_TX_OFFLOAD_TCP_CKSUM;
}

static void enicpmd_dev_promiscuous_enable(struct rte_eth_dev *eth_dev)
{
	struct enic *enic = pmd_priv(eth_dev);

	ENICPMD_FUNC_TRACE();
	enic->promisc = 1;
	enic_add_packet_filter(enic);
}

static void enicpmd_dev_promiscuous_disable(struct rte_eth_dev *eth_dev)
{
	struct enic *enic = pmd_priv(eth_dev);

	ENICPMD_FUNC_TRACE();
	enic->promisc = 0;
	enic_add_packet_filter(enic);
}

static void enicpmd_dev_allmulticast_enable(struct rte_eth_dev *eth_dev)
{
	struct enic *enic = pmd_priv(eth_dev);

	ENICPMD_FUNC_TRACE();
	enic->allmulti = 1;
	enic_add_packet_filter(enic);
}

static void enicpmd_dev_allmulticast_disable(struct rte_eth_dev *eth_dev)
{
	struct enic *enic = pmd_priv(eth_dev);

	ENICPMD_FUNC_TRACE();
	enic->allmulti = 0;
	enic_add_packet_filter(enic);
}

static void enicpmd_add_mac_addr(struct rte_eth_dev *eth_dev,
	struct ether_addr *mac_addr,
	__rte_unused uint32_t index, __rte_unused uint32_t pool)
{
	struct enic *enic = pmd_priv(eth_dev);

	ENICPMD_FUNC_TRACE();
	enic_set_mac_address(enic, mac_addr->addr_bytes);
}

static void enicpmd_remove_mac_addr(struct rte_eth_dev *eth_dev, __rte_unused uint32_t index)
{
	struct enic *enic = pmd_priv(eth_dev);

	ENICPMD_FUNC_TRACE();
	enic_del_mac_address(enic);
}


static uint16_t enicpmd_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
	uint16_t nb_pkts)
{
	unsigned int index;
	unsigned int frags;
	unsigned int pkt_len;
	unsigned int seg_len;
	unsigned int inc_len;
	unsigned int nb_segs;
	struct rte_mbuf *tx_pkt;
	struct vnic_wq *wq = (struct vnic_wq *)tx_queue;
	struct enic *enic = vnic_dev_priv(wq->vdev);
	unsigned short vlan_id;
	unsigned short ol_flags;

	for (index = 0; index < nb_pkts; index++) {
		tx_pkt = *tx_pkts++;
		inc_len = 0;
		nb_segs = tx_pkt->nb_segs;
		if (nb_segs > vnic_wq_desc_avail(wq)) {
			/* wq cleanup and try again */
			if (!enic_cleanup_wq(enic, wq) ||
				(nb_segs > vnic_wq_desc_avail(wq)))
				return index;
		}
		pkt_len = tx_pkt->pkt_len;
		vlan_id = tx_pkt->vlan_tci;
		ol_flags = tx_pkt->ol_flags;
		for (frags = 0; inc_len < pkt_len; frags++) {
			if (!tx_pkt)
				break;
			seg_len = tx_pkt->data_len;
			inc_len += seg_len;
			if (enic_send_pkt(enic, wq, tx_pkt,
				    (unsigned short)seg_len, !frags,
				    (pkt_len == inc_len), ol_flags, vlan_id)) {
				break;
			}
			tx_pkt = tx_pkt->next;
		}
	}

	enic_cleanup_wq(enic, wq);
	return index;
}

static uint16_t enicpmd_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
	uint16_t nb_pkts)
{
	struct vnic_rq *rq = (struct vnic_rq *)rx_queue;
	unsigned int work_done;

	if (enic_poll(rq, rx_pkts, (unsigned int)nb_pkts, &work_done))
		dev_err(enic, "error in enicpmd poll\n");

	return work_done;
}

static struct eth_dev_ops enicpmd_eth_dev_ops = {
	.dev_configure        = enicpmd_dev_configure,
	.dev_start            = enicpmd_dev_start,
	.dev_stop             = enicpmd_dev_stop,
	.dev_set_link_up      = NULL,
	.dev_set_link_down    = NULL,
	.dev_close            = enicpmd_dev_close,
	.promiscuous_enable   = enicpmd_dev_promiscuous_enable,
	.promiscuous_disable  = enicpmd_dev_promiscuous_disable,
	.allmulticast_enable  = enicpmd_dev_allmulticast_enable,
	.allmulticast_disable = enicpmd_dev_allmulticast_disable,
	.link_update          = enicpmd_dev_link_update,
	.stats_get            = enicpmd_dev_stats_get,
	.stats_reset          = enicpmd_dev_stats_reset,
	.queue_stats_mapping_set = NULL,
	.dev_infos_get        = enicpmd_dev_info_get,
	.mtu_set              = NULL,
	.vlan_filter_set      = enicpmd_vlan_filter_set,
	.vlan_tpid_set        = NULL,
	.vlan_offload_set     = enicpmd_vlan_offload_set,
	.vlan_strip_queue_set = NULL,
	.rx_queue_start       = enicpmd_dev_rx_queue_start,
	.rx_queue_stop        = enicpmd_dev_rx_queue_stop,
	.tx_queue_start       = enicpmd_dev_tx_queue_start,
	.tx_queue_stop        = enicpmd_dev_tx_queue_stop,
	.rx_queue_setup       = enicpmd_dev_rx_queue_setup,
	.rx_queue_release     = enicpmd_dev_rx_queue_release,
	.rx_queue_count       = NULL,
	.rx_descriptor_done   = NULL,
	.tx_queue_setup       = enicpmd_dev_tx_queue_setup,
	.tx_queue_release     = enicpmd_dev_tx_queue_release,
	.dev_led_on           = NULL,
	.dev_led_off          = NULL,
	.flow_ctrl_get        = NULL,
	.flow_ctrl_set        = NULL,
	.priority_flow_ctrl_set = NULL,
	.mac_addr_add         = enicpmd_add_mac_addr,
	.mac_addr_remove      = enicpmd_remove_mac_addr,
	.fdir_add_signature_filter    = NULL,
	.fdir_update_signature_filter = NULL,
	.fdir_remove_signature_filter = NULL,
	.fdir_infos_get               = enicpmd_fdir_info_get,
	.fdir_add_perfect_filter      = enicpmd_fdir_add_perfect_filter,
	.fdir_update_perfect_filter   = enicpmd_fdir_add_perfect_filter,
	.fdir_remove_perfect_filter   = enicpmd_fdir_remove_perfect_filter,
	.fdir_set_masks               = NULL,
};

struct enic *enicpmd_list_head = NULL;
/* Initialize the driver
 * It returns 0 on success.
 */
static int eth_enicpmd_dev_init(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pdev;
	struct rte_pci_addr *addr;
	struct enic *enic = pmd_priv(eth_dev);

	ENICPMD_FUNC_TRACE();

	enic->rte_dev = eth_dev;
	eth_dev->dev_ops = &enicpmd_eth_dev_ops;
	eth_dev->rx_pkt_burst = &enicpmd_recv_pkts;
	eth_dev->tx_pkt_burst = &enicpmd_xmit_pkts;

	pdev = eth_dev->pci_dev;
	enic->pdev = pdev;
	addr = &pdev->addr;

	snprintf(enic->bdf_name, ENICPMD_BDF_LENGTH, "%04x:%02x:%02x.%x",
		addr->domain, addr->bus, addr->devid, addr->function);

	return enic_probe(enic);
}

static struct eth_driver rte_enic_pmd = {
	{
		.name = "rte_enic_pmd",
		.id_table = pci_id_enic_map,
		.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	},
	.eth_dev_init = eth_enicpmd_dev_init,
	.dev_private_size = sizeof(struct enic),
};

/* Driver initialization routine.
 * Invoked once at EAL init time.
 * Register as the [Poll Mode] Driver of Cisco ENIC device.
 */
static int
rte_enic_pmd_init(const char *name __rte_unused,
	const char *params __rte_unused)
{
	ENICPMD_FUNC_TRACE();

	rte_eth_driver_register(&rte_enic_pmd);
	return 0;
}

static struct rte_driver rte_enic_driver = {
	.type = PMD_PDEV,
	.init = rte_enic_pmd_init,
};

PMD_REGISTER_DRIVER(rte_enic_driver);
