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

#include <stdio.h>
#include <stdint.h>

#include <rte_dev.h>
#include <rte_pci.h>
#include <rte_ethdev.h>
#include <rte_ethdev_pci.h>
#include <rte_string_fns.h>

#include "vnic_intr.h"
#include "vnic_cq.h"
#include "vnic_wq.h"
#include "vnic_rq.h"
#include "vnic_enet.h"
#include "enic.h"

#ifdef RTE_LIBRTE_ENIC_DEBUG
#define ENICPMD_FUNC_TRACE() \
	RTE_LOG(DEBUG, PMD, "ENICPMD trace: %s\n", __func__)
#else
#define ENICPMD_FUNC_TRACE() (void)0
#endif

/*
 * The set of PCI devices this driver supports
 */
#define CISCO_PCI_VENDOR_ID 0x1137
static const struct rte_pci_id pci_id_enic_map[] = {
	{ RTE_PCI_DEVICE(CISCO_PCI_VENDOR_ID, PCI_DEVICE_ID_CISCO_VIC_ENET) },
	{ RTE_PCI_DEVICE(CISCO_PCI_VENDOR_ID, PCI_DEVICE_ID_CISCO_VIC_ENET_VF) },
	{.vendor_id = 0, /* sentinel */},
};

static int
enicpmd_fdir_ctrl_func(struct rte_eth_dev *eth_dev,
			enum rte_filter_op filter_op, void *arg)
{
	struct enic *enic = pmd_priv(eth_dev);
	int ret = 0;

	ENICPMD_FUNC_TRACE();
	if (filter_op == RTE_ETH_FILTER_NOP)
		return 0;

	if (arg == NULL && filter_op != RTE_ETH_FILTER_FLUSH)
		return -EINVAL;

	switch (filter_op) {
	case RTE_ETH_FILTER_ADD:
	case RTE_ETH_FILTER_UPDATE:
		ret = enic_fdir_add_fltr(enic,
			(struct rte_eth_fdir_filter *)arg);
		break;

	case RTE_ETH_FILTER_DELETE:
		ret = enic_fdir_del_fltr(enic,
			(struct rte_eth_fdir_filter *)arg);
		break;

	case RTE_ETH_FILTER_STATS:
		enic_fdir_stats_get(enic, (struct rte_eth_fdir_stats *)arg);
		break;

	case RTE_ETH_FILTER_FLUSH:
		dev_warning(enic, "unsupported operation %u", filter_op);
		ret = -ENOTSUP;
		break;
	case RTE_ETH_FILTER_INFO:
		enic_fdir_info_get(enic, (struct rte_eth_fdir_info *)arg);
		break;
	default:
		dev_err(enic, "unknown operation %u", filter_op);
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int
enicpmd_dev_filter_ctrl(struct rte_eth_dev *dev,
		     enum rte_filter_type filter_type,
		     enum rte_filter_op filter_op,
		     void *arg)
{
	int ret = 0;

	ENICPMD_FUNC_TRACE();

	switch (filter_type) {
	case RTE_ETH_FILTER_GENERIC:
		if (filter_op != RTE_ETH_FILTER_GET)
			return -EINVAL;
		*(const void **)arg = &enic_flow_ops;
		break;
	case RTE_ETH_FILTER_FDIR:
		ret = enicpmd_fdir_ctrl_func(dev, filter_op, arg);
		break;
	default:
		dev_warning(enic, "Filter type (%d) not supported",
			filter_type);
		ret = -EINVAL;
		break;
	}

	return ret;
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
	for (index = 0; index < enic->wq_count; index++) {
		if (!enic->wq[index].ctrl)
			break;
	}
	if (enic->wq_count != index)
		return 0;
	/* check start of packet (SOP) RQs only in case scatter is disabled. */
	for (index = 0; index < enic->rq_count; index++) {
		if (!enic->rq[enic_rte_rq_idx_to_sop_idx(index)].ctrl)
			break;
	}
	if (enic->rq_count != index)
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
	if (queue_idx >= ENIC_WQ_MAX) {
		dev_err(enic,
			"Max number of TX queues exceeded.  Max is %d\n",
			ENIC_WQ_MAX);
		return -EINVAL;
	}

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

static uint32_t enicpmd_dev_rx_queue_count(struct rte_eth_dev *dev,
					   uint16_t rx_queue_id)
{
	struct enic *enic = pmd_priv(dev);
	uint32_t queue_count = 0;
	struct vnic_cq *cq;
	uint32_t cq_tail;
	uint16_t cq_idx;
	int rq_num;

	rq_num = enic_rte_rq_idx_to_sop_idx(rx_queue_id);
	cq = &enic->cq[enic_cq_rq(enic, rq_num)];
	cq_idx = cq->to_clean;

	cq_tail = ioread32(&cq->ctrl->cq_tail);

	if (cq_tail < cq_idx)
		cq_tail += cq->ring.desc_count;

	queue_count = cq_tail - cq_idx;

	return queue_count;
}

static int enicpmd_dev_rx_queue_setup(struct rte_eth_dev *eth_dev,
	uint16_t queue_idx,
	uint16_t nb_desc,
	unsigned int socket_id,
	const struct rte_eth_rxconf *rx_conf,
	struct rte_mempool *mp)
{
	int ret;
	struct enic *enic = pmd_priv(eth_dev);

	ENICPMD_FUNC_TRACE();
	/* With Rx scatter support, two RQs are now used on VIC per RQ used
	 * by the application.
	 */
	if (queue_idx * 2 >= ENIC_RQ_MAX) {
		dev_err(enic,
			"Max number of RX queues exceeded.  Max is %d. This PMD uses 2 RQs on VIC per RQ used by DPDK.\n",
			ENIC_RQ_MAX);
		return -EINVAL;
	}

	eth_dev->data->rx_queues[queue_idx] =
		(void *)&enic->rq[enic_rte_rq_idx_to_sop_idx(queue_idx)];

	ret = enic_alloc_rq(enic, queue_idx, socket_id, mp, nb_desc,
			    rx_conf->rx_free_thresh);
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
	int err;

	ENICPMD_FUNC_TRACE();
	if (on)
		err = enic_add_vlan(enic, vlan_id);
	else
		err = enic_del_vlan(enic, vlan_id);
	return err;
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

	enicpmd_vlan_offload_set(eth_dev, ETH_VLAN_STRIP_MASK);
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

	ENICPMD_FUNC_TRACE();
	return enic_link_update(enic);
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
	device_info->pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	/* Scattered Rx uses two receive queues per rx queue exposed to dpdk */
	device_info->max_rx_queues = enic->conf_rq_count / 2;
	device_info->max_tx_queues = enic->conf_wq_count;
	device_info->min_rx_bufsize = ENIC_MIN_MTU;
	device_info->max_rx_pktlen = enic->max_mtu + ETHER_HDR_LEN + 4;
	device_info->max_mac_addrs = ENIC_MAX_MAC_ADDR;
	device_info->rx_offload_capa =
		DEV_RX_OFFLOAD_VLAN_STRIP |
		DEV_RX_OFFLOAD_IPV4_CKSUM |
		DEV_RX_OFFLOAD_UDP_CKSUM  |
		DEV_RX_OFFLOAD_TCP_CKSUM;
	device_info->tx_offload_capa =
		DEV_TX_OFFLOAD_VLAN_INSERT |
		DEV_TX_OFFLOAD_IPV4_CKSUM  |
		DEV_TX_OFFLOAD_UDP_CKSUM   |
		DEV_TX_OFFLOAD_TCP_CKSUM   |
		DEV_TX_OFFLOAD_TCP_TSO;
	device_info->default_rxconf = (struct rte_eth_rxconf) {
		.rx_free_thresh = ENIC_DEFAULT_RX_FREE_THRESH
	};
}

static const uint32_t *enicpmd_dev_supported_ptypes_get(struct rte_eth_dev *dev)
{
	static const uint32_t ptypes[] = {
		RTE_PTYPE_L2_ETHER,
		RTE_PTYPE_L2_ETHER_VLAN,
		RTE_PTYPE_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_L4_FRAG,
		RTE_PTYPE_L4_NONFRAG,
		RTE_PTYPE_UNKNOWN
	};

	if (dev->rx_pkt_burst == enic_recv_pkts)
		return ptypes;
	return NULL;
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

static int enicpmd_add_mac_addr(struct rte_eth_dev *eth_dev,
	struct ether_addr *mac_addr,
	__rte_unused uint32_t index, __rte_unused uint32_t pool)
{
	struct enic *enic = pmd_priv(eth_dev);

	ENICPMD_FUNC_TRACE();
	return enic_set_mac_address(enic, mac_addr->addr_bytes);
}

static void enicpmd_remove_mac_addr(struct rte_eth_dev *eth_dev, uint32_t index)
{
	struct enic *enic = pmd_priv(eth_dev);

	ENICPMD_FUNC_TRACE();
	enic_del_mac_address(enic, index);
}

static int enicpmd_mtu_set(struct rte_eth_dev *eth_dev, uint16_t mtu)
{
	struct enic *enic = pmd_priv(eth_dev);

	ENICPMD_FUNC_TRACE();
	return enic_set_mtu(enic, mtu);
}

static const struct eth_dev_ops enicpmd_eth_dev_ops = {
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
	.dev_supported_ptypes_get = enicpmd_dev_supported_ptypes_get,
	.mtu_set              = enicpmd_mtu_set,
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
	.rx_queue_count       = enicpmd_dev_rx_queue_count,
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
	.filter_ctrl          = enicpmd_dev_filter_ctrl,
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

	enic->port_id = eth_dev->data->port_id;
	enic->rte_dev = eth_dev;
	eth_dev->dev_ops = &enicpmd_eth_dev_ops;
	eth_dev->rx_pkt_burst = &enic_recv_pkts;
	eth_dev->tx_pkt_burst = &enic_xmit_pkts;

	pdev = RTE_ETH_DEV_TO_PCI(eth_dev);
	rte_eth_copy_pci_info(eth_dev, pdev);
	enic->pdev = pdev;
	addr = &pdev->addr;

	snprintf(enic->bdf_name, ENICPMD_BDF_LENGTH, "%04x:%02x:%02x.%x",
		addr->domain, addr->bus, addr->devid, addr->function);

	return enic_probe(enic);
}

static int eth_enic_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev, sizeof(struct enic),
		eth_enicpmd_dev_init);
}

static int eth_enic_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, NULL);
}

static struct rte_pci_driver rte_enic_pmd = {
	.id_table = pci_id_enic_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
	.probe = eth_enic_pci_probe,
	.remove = eth_enic_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_enic, rte_enic_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_enic, pci_id_enic_map);
RTE_PMD_REGISTER_KMOD_DEP(net_enic, "* igb_uio | uio_pci_generic | vfio-pci");
