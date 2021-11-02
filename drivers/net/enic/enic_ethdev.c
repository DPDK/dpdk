/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2017 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 */

#include <stdio.h>
#include <stdint.h>

#include <rte_dev.h>
#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <ethdev_driver.h>
#include <ethdev_pci.h>
#include <rte_geneve.h>
#include <rte_kvargs.h>
#include <rte_string_fns.h>

#include "vnic_intr.h"
#include "vnic_cq.h"
#include "vnic_wq.h"
#include "vnic_rq.h"
#include "vnic_enet.h"
#include "enic.h"

/*
 * The set of PCI devices this driver supports
 */
#define CISCO_PCI_VENDOR_ID 0x1137
static const struct rte_pci_id pci_id_enic_map[] = {
	{RTE_PCI_DEVICE(CISCO_PCI_VENDOR_ID, PCI_DEVICE_ID_CISCO_VIC_ENET)},
	{RTE_PCI_DEVICE(CISCO_PCI_VENDOR_ID, PCI_DEVICE_ID_CISCO_VIC_ENET_VF)},
	{RTE_PCI_DEVICE(CISCO_PCI_VENDOR_ID, PCI_DEVICE_ID_CISCO_VIC_ENET_SN)},
	{.vendor_id = 0, /* sentinel */},
};

/* Supported link speeds of production VIC models */
static const struct vic_speed_capa {
	uint16_t sub_devid;
	uint32_t capa;
} vic_speed_capa_map[] = {
	{ 0x0043, RTE_ETH_LINK_SPEED_10G }, /* VIC */
	{ 0x0047, RTE_ETH_LINK_SPEED_10G }, /* P81E PCIe */
	{ 0x0048, RTE_ETH_LINK_SPEED_10G }, /* M81KR Mezz */
	{ 0x004f, RTE_ETH_LINK_SPEED_10G }, /* 1280 Mezz */
	{ 0x0084, RTE_ETH_LINK_SPEED_10G }, /* 1240 MLOM */
	{ 0x0085, RTE_ETH_LINK_SPEED_10G }, /* 1225 PCIe */
	{ 0x00cd, RTE_ETH_LINK_SPEED_10G | RTE_ETH_LINK_SPEED_40G }, /* 1285 PCIe */
	{ 0x00ce, RTE_ETH_LINK_SPEED_10G }, /* 1225T PCIe */
	{ 0x012a, RTE_ETH_LINK_SPEED_40G }, /* M4308 */
	{ 0x012c, RTE_ETH_LINK_SPEED_10G | RTE_ETH_LINK_SPEED_40G }, /* 1340 MLOM */
	{ 0x012e, RTE_ETH_LINK_SPEED_10G }, /* 1227 PCIe */
	{ 0x0137, RTE_ETH_LINK_SPEED_10G | RTE_ETH_LINK_SPEED_40G }, /* 1380 Mezz */
	{ 0x014d, RTE_ETH_LINK_SPEED_10G | RTE_ETH_LINK_SPEED_40G }, /* 1385 PCIe */
	{ 0x015d, RTE_ETH_LINK_SPEED_10G | RTE_ETH_LINK_SPEED_40G }, /* 1387 MLOM */
	{ 0x0215, RTE_ETH_LINK_SPEED_10G | RTE_ETH_LINK_SPEED_25G |
		  RTE_ETH_LINK_SPEED_40G }, /* 1440 Mezz */
	{ 0x0216, RTE_ETH_LINK_SPEED_10G | RTE_ETH_LINK_SPEED_25G |
		  RTE_ETH_LINK_SPEED_40G }, /* 1480 MLOM */
	{ 0x0217, RTE_ETH_LINK_SPEED_10G | RTE_ETH_LINK_SPEED_25G }, /* 1455 PCIe */
	{ 0x0218, RTE_ETH_LINK_SPEED_10G | RTE_ETH_LINK_SPEED_25G }, /* 1457 MLOM */
	{ 0x0219, RTE_ETH_LINK_SPEED_40G }, /* 1485 PCIe */
	{ 0x021a, RTE_ETH_LINK_SPEED_40G }, /* 1487 MLOM */
	{ 0x024a, RTE_ETH_LINK_SPEED_40G | RTE_ETH_LINK_SPEED_100G }, /* 1495 PCIe */
	{ 0x024b, RTE_ETH_LINK_SPEED_40G | RTE_ETH_LINK_SPEED_100G }, /* 1497 MLOM */
	{ 0, 0 }, /* End marker */
};

#define ENIC_DEVARG_CQ64 "cq64"
#define ENIC_DEVARG_DISABLE_OVERLAY "disable-overlay"
#define ENIC_DEVARG_ENABLE_AVX2_RX "enable-avx2-rx"
#define ENIC_DEVARG_IG_VLAN_REWRITE "ig-vlan-rewrite"
#define ENIC_DEVARG_REPRESENTOR "representor"

RTE_LOG_REGISTER_DEFAULT(enic_pmd_logtype, INFO);

static int
enicpmd_dev_flow_ops_get(struct rte_eth_dev *dev,
			 const struct rte_flow_ops **ops)
{
	struct enic *enic = pmd_priv(dev);

	ENICPMD_FUNC_TRACE();

	if (enic->flow_filter_mode == FILTER_FLOWMAN)
		*ops = &enic_fm_flow_ops;
	else
		*ops = &enic_flow_ops;
	return 0;
}

static void enicpmd_dev_tx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	void *txq = dev->data->tx_queues[qid];

	ENICPMD_FUNC_TRACE();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return;

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
	const struct rte_eth_txconf *tx_conf)
{
	int ret;
	struct enic *enic = pmd_priv(eth_dev);
	struct vnic_wq *wq;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -E_RTE_SECONDARY;

	ENICPMD_FUNC_TRACE();
	RTE_ASSERT(queue_idx < enic->conf_wq_count);
	wq = &enic->wq[queue_idx];
	wq->offloads = tx_conf->offloads |
		eth_dev->data->dev_conf.txmode.offloads;
	eth_dev->data->tx_queues[queue_idx] = (void *)wq;

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

static void enicpmd_dev_rx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	void *rxq = dev->data->rx_queues[qid];

	ENICPMD_FUNC_TRACE();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return;

	enic_free_rq(rxq);
}

static uint32_t enicpmd_dev_rx_queue_count(void *rx_queue)
{
	struct enic *enic;
	struct vnic_rq *sop_rq;
	uint32_t queue_count = 0;
	struct vnic_cq *cq;
	uint32_t cq_tail;
	uint16_t cq_idx;

	sop_rq = rx_queue;
	enic = vnic_dev_priv(sop_rq->vdev);
	cq = &enic->cq[enic_cq_rq(enic, sop_rq->index)];
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

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -E_RTE_SECONDARY;
	RTE_ASSERT(enic_rte_rq_idx_to_sop_idx(queue_idx) < enic->conf_rq_count);
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

static int enicpmd_vlan_offload_set(struct rte_eth_dev *eth_dev, int mask)
{
	struct enic *enic = pmd_priv(eth_dev);
	uint64_t offloads;

	ENICPMD_FUNC_TRACE();

	offloads = eth_dev->data->dev_conf.rxmode.offloads;
	if (mask & RTE_ETH_VLAN_STRIP_MASK) {
		if (offloads & RTE_ETH_RX_OFFLOAD_VLAN_STRIP)
			enic->ig_vlan_strip_en = 1;
		else
			enic->ig_vlan_strip_en = 0;
	}

	return enic_set_vlan_strip(enic);
}

static int enicpmd_dev_configure(struct rte_eth_dev *eth_dev)
{
	int ret;
	int mask;
	struct enic *enic = pmd_priv(eth_dev);

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -E_RTE_SECONDARY;

	ENICPMD_FUNC_TRACE();
	ret = enic_set_vnic_res(enic);
	if (ret) {
		dev_err(enic, "Set vNIC resource num  failed, aborting\n");
		return ret;
	}

	if (eth_dev->data->dev_conf.rxmode.mq_mode & RTE_ETH_MQ_RX_RSS_FLAG)
		eth_dev->data->dev_conf.rxmode.offloads |=
			RTE_ETH_RX_OFFLOAD_RSS_HASH;

	enic->mc_count = 0;
	enic->hw_ip_checksum = !!(eth_dev->data->dev_conf.rxmode.offloads &
				  RTE_ETH_RX_OFFLOAD_CHECKSUM);
	/* All vlan offload masks to apply the current settings */
	mask = RTE_ETH_VLAN_STRIP_MASK |
		RTE_ETH_VLAN_FILTER_MASK |
		RTE_ETH_VLAN_EXTEND_MASK;
	ret = enicpmd_vlan_offload_set(eth_dev, mask);
	if (ret) {
		dev_err(enic, "Failed to configure VLAN offloads\n");
		return ret;
	}
	/*
	 * Initialize RSS with the default reta and key. If the user key is
	 * given (rx_adv_conf.rss_conf.rss_key), will use that instead of the
	 * default key.
	 */
	return enic_init_rss_nic_cfg(enic);
}

/* Start the device.
 * It returns 0 on success.
 */
static int enicpmd_dev_start(struct rte_eth_dev *eth_dev)
{
	struct enic *enic = pmd_priv(eth_dev);

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -E_RTE_SECONDARY;

	ENICPMD_FUNC_TRACE();
	return enic_enable(enic);
}

/*
 * Stop device: disable rx and tx functions to allow for reconfiguring.
 */
static int enicpmd_dev_stop(struct rte_eth_dev *eth_dev)
{
	struct rte_eth_link link;
	struct enic *enic = pmd_priv(eth_dev);

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	ENICPMD_FUNC_TRACE();
	enic_disable(enic);

	memset(&link, 0, sizeof(link));
	rte_eth_linkstatus_set(eth_dev, &link);

	return 0;
}

/*
 * Stop device.
 */
static int enicpmd_dev_close(struct rte_eth_dev *eth_dev)
{
	struct enic *enic = pmd_priv(eth_dev);

	ENICPMD_FUNC_TRACE();
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	enic_remove(enic);

	return 0;
}

static int enicpmd_dev_link_update(struct rte_eth_dev *eth_dev,
	__rte_unused int wait_to_complete)
{
	ENICPMD_FUNC_TRACE();
	return enic_link_update(eth_dev);
}

static int enicpmd_dev_stats_get(struct rte_eth_dev *eth_dev,
	struct rte_eth_stats *stats)
{
	struct enic *enic = pmd_priv(eth_dev);

	ENICPMD_FUNC_TRACE();
	return enic_dev_stats_get(enic, stats);
}

static int enicpmd_dev_stats_reset(struct rte_eth_dev *eth_dev)
{
	struct enic *enic = pmd_priv(eth_dev);

	ENICPMD_FUNC_TRACE();
	return enic_dev_stats_clear(enic);
}

static uint32_t speed_capa_from_pci_id(struct rte_eth_dev *eth_dev)
{
	const struct vic_speed_capa *m;
	struct rte_pci_device *pdev;
	uint16_t id;

	pdev = RTE_ETH_DEV_TO_PCI(eth_dev);
	id = pdev->id.subsystem_device_id;
	for (m = vic_speed_capa_map; m->sub_devid != 0; m++) {
		if (m->sub_devid == id)
			return m->capa;
	}
	/* 1300 and later models are at least 40G */
	if (id >= 0x0100)
		return RTE_ETH_LINK_SPEED_40G;
	/* VFs have subsystem id 0, check device id */
	if (id == 0) {
		/* Newer VF implies at least 40G model */
		if (pdev->id.device_id == PCI_DEVICE_ID_CISCO_VIC_ENET_SN)
			return RTE_ETH_LINK_SPEED_40G;
	}
	return RTE_ETH_LINK_SPEED_10G;
}

static int enicpmd_dev_info_get(struct rte_eth_dev *eth_dev,
	struct rte_eth_dev_info *device_info)
{
	struct enic *enic = pmd_priv(eth_dev);

	ENICPMD_FUNC_TRACE();
	/* Scattered Rx uses two receive queues per rx queue exposed to dpdk */
	device_info->max_rx_queues = enic->conf_rq_count / 2;
	device_info->max_tx_queues = enic->conf_wq_count;
	device_info->min_rx_bufsize = ENIC_MIN_MTU;
	/* "Max" mtu is not a typo. HW receives packet sizes up to the
	 * max mtu regardless of the current mtu (vNIC's mtu). vNIC mtu is
	 * a hint to the driver to size receive buffers accordingly so that
	 * larger-than-vnic-mtu packets get truncated.. For DPDK, we let
	 * the user decide the buffer size via rxmode.mtu, basically
	 * ignoring vNIC mtu.
	 */
	device_info->max_rx_pktlen = enic_mtu_to_max_rx_pktlen(enic->max_mtu);
	device_info->max_mac_addrs = ENIC_UNICAST_PERFECT_FILTERS;
	device_info->min_mtu = ENIC_MIN_MTU;
	device_info->max_mtu = enic->max_mtu;
	device_info->rx_offload_capa = enic->rx_offload_capa;
	device_info->tx_offload_capa = enic->tx_offload_capa;
	device_info->tx_queue_offload_capa = enic->tx_queue_offload_capa;
	device_info->dev_capa &= ~RTE_ETH_DEV_CAPA_FLOW_RULE_KEEP;
	device_info->default_rxconf = (struct rte_eth_rxconf) {
		.rx_free_thresh = ENIC_DEFAULT_RX_FREE_THRESH
	};
	device_info->reta_size = enic->reta_size;
	device_info->hash_key_size = enic->hash_key_size;
	device_info->flow_type_rss_offloads = enic->flow_type_rss_offloads;
	device_info->rx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = enic->config.rq_desc_count,
		.nb_min = ENIC_MIN_RQ_DESCS,
		.nb_align = ENIC_ALIGN_DESCS,
	};
	device_info->tx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = enic->config.wq_desc_count,
		.nb_min = ENIC_MIN_WQ_DESCS,
		.nb_align = ENIC_ALIGN_DESCS,
		.nb_seg_max = ENIC_TX_XMIT_MAX,
		.nb_mtu_seg_max = ENIC_NON_TSO_MAX_DESC,
	};
	device_info->default_rxportconf = (struct rte_eth_dev_portconf) {
		.burst_size = ENIC_DEFAULT_RX_BURST,
		.ring_size = RTE_MIN(device_info->rx_desc_lim.nb_max,
			ENIC_DEFAULT_RX_RING_SIZE),
		.nb_queues = ENIC_DEFAULT_RX_RINGS,
	};
	device_info->default_txportconf = (struct rte_eth_dev_portconf) {
		.burst_size = ENIC_DEFAULT_TX_BURST,
		.ring_size = RTE_MIN(device_info->tx_desc_lim.nb_max,
			ENIC_DEFAULT_TX_RING_SIZE),
		.nb_queues = ENIC_DEFAULT_TX_RINGS,
	};
	device_info->speed_capa = speed_capa_from_pci_id(eth_dev);

	return 0;
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
	static const uint32_t ptypes_overlay[] = {
		RTE_PTYPE_L2_ETHER,
		RTE_PTYPE_L2_ETHER_VLAN,
		RTE_PTYPE_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_L4_FRAG,
		RTE_PTYPE_L4_NONFRAG,
		RTE_PTYPE_TUNNEL_GRENAT,
		RTE_PTYPE_INNER_L2_ETHER,
		RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_INNER_L4_TCP,
		RTE_PTYPE_INNER_L4_UDP,
		RTE_PTYPE_INNER_L4_FRAG,
		RTE_PTYPE_INNER_L4_NONFRAG,
		RTE_PTYPE_UNKNOWN
	};

	if (dev->rx_pkt_burst != enic_dummy_recv_pkts &&
	    dev->rx_pkt_burst != NULL) {
		struct enic *enic = pmd_priv(dev);
		if (enic->overlay_offload)
			return ptypes_overlay;
		else
			return ptypes;
	}
	return NULL;
}

static int enicpmd_dev_promiscuous_enable(struct rte_eth_dev *eth_dev)
{
	struct enic *enic = pmd_priv(eth_dev);
	int ret;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -E_RTE_SECONDARY;

	ENICPMD_FUNC_TRACE();

	enic->promisc = 1;
	ret = enic_add_packet_filter(enic);
	if (ret != 0)
		enic->promisc = 0;

	return ret;
}

static int enicpmd_dev_promiscuous_disable(struct rte_eth_dev *eth_dev)
{
	struct enic *enic = pmd_priv(eth_dev);
	int ret;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -E_RTE_SECONDARY;

	ENICPMD_FUNC_TRACE();
	enic->promisc = 0;
	ret = enic_add_packet_filter(enic);
	if (ret != 0)
		enic->promisc = 1;

	return ret;
}

static int enicpmd_dev_allmulticast_enable(struct rte_eth_dev *eth_dev)
{
	struct enic *enic = pmd_priv(eth_dev);
	int ret;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -E_RTE_SECONDARY;

	ENICPMD_FUNC_TRACE();
	enic->allmulti = 1;
	ret = enic_add_packet_filter(enic);
	if (ret != 0)
		enic->allmulti = 0;

	return ret;
}

static int enicpmd_dev_allmulticast_disable(struct rte_eth_dev *eth_dev)
{
	struct enic *enic = pmd_priv(eth_dev);
	int ret;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -E_RTE_SECONDARY;

	ENICPMD_FUNC_TRACE();
	enic->allmulti = 0;
	ret = enic_add_packet_filter(enic);
	if (ret != 0)
		enic->allmulti = 1;

	return ret;
}

static int enicpmd_add_mac_addr(struct rte_eth_dev *eth_dev,
	struct rte_ether_addr *mac_addr,
	__rte_unused uint32_t index, __rte_unused uint32_t pool)
{
	struct enic *enic = pmd_priv(eth_dev);

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -E_RTE_SECONDARY;

	ENICPMD_FUNC_TRACE();
	return enic_set_mac_address(enic, mac_addr->addr_bytes);
}

static void enicpmd_remove_mac_addr(struct rte_eth_dev *eth_dev, uint32_t index)
{
	struct enic *enic = pmd_priv(eth_dev);

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return;

	ENICPMD_FUNC_TRACE();
	if (enic_del_mac_address(enic, index))
		dev_err(enic, "del mac addr failed\n");
}

static int enicpmd_set_mac_addr(struct rte_eth_dev *eth_dev,
				struct rte_ether_addr *addr)
{
	struct enic *enic = pmd_priv(eth_dev);
	int ret;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -E_RTE_SECONDARY;

	ENICPMD_FUNC_TRACE();
	ret = enic_del_mac_address(enic, 0);
	if (ret)
		return ret;
	return enic_set_mac_address(enic, addr->addr_bytes);
}

static void debug_log_add_del_addr(struct rte_ether_addr *addr, bool add)
{
	char mac_str[RTE_ETHER_ADDR_FMT_SIZE];

	rte_ether_format_addr(mac_str, RTE_ETHER_ADDR_FMT_SIZE, addr);
	ENICPMD_LOG(DEBUG, " %s address %s\n",
		     add ? "add" : "remove", mac_str);
}

static int enicpmd_set_mc_addr_list(struct rte_eth_dev *eth_dev,
				    struct rte_ether_addr *mc_addr_set,
				    uint32_t nb_mc_addr)
{
	struct enic *enic = pmd_priv(eth_dev);
	char mac_str[RTE_ETHER_ADDR_FMT_SIZE];
	struct rte_ether_addr *addr;
	uint32_t i, j;
	int ret;

	ENICPMD_FUNC_TRACE();

	/* Validate the given addresses first */
	for (i = 0; i < nb_mc_addr && mc_addr_set != NULL; i++) {
		addr = &mc_addr_set[i];
		if (!rte_is_multicast_ether_addr(addr) ||
		    rte_is_broadcast_ether_addr(addr)) {
			rte_ether_format_addr(mac_str,
					RTE_ETHER_ADDR_FMT_SIZE, addr);
			ENICPMD_LOG(ERR, " invalid multicast address %s\n",
				     mac_str);
			return -EINVAL;
		}
	}

	/* Flush all if requested */
	if (nb_mc_addr == 0 || mc_addr_set == NULL) {
		ENICPMD_LOG(DEBUG, " flush multicast addresses\n");
		for (i = 0; i < enic->mc_count; i++) {
			addr = &enic->mc_addrs[i];
			debug_log_add_del_addr(addr, false);
			ret = vnic_dev_del_addr(enic->vdev, addr->addr_bytes);
			if (ret)
				return ret;
		}
		enic->mc_count = 0;
		return 0;
	}

	if (nb_mc_addr > ENIC_MULTICAST_PERFECT_FILTERS) {
		ENICPMD_LOG(ERR, " too many multicast addresses: max=%d\n",
			     ENIC_MULTICAST_PERFECT_FILTERS);
		return -ENOSPC;
	}
	/*
	 * devcmd is slow, so apply the difference instead of flushing and
	 * adding everything.
	 * 1. Delete addresses on the NIC but not on the host
	 */
	for (i = 0; i < enic->mc_count; i++) {
		addr = &enic->mc_addrs[i];
		for (j = 0; j < nb_mc_addr; j++) {
			if (rte_is_same_ether_addr(addr, &mc_addr_set[j]))
				break;
		}
		if (j < nb_mc_addr)
			continue;
		debug_log_add_del_addr(addr, false);
		ret = vnic_dev_del_addr(enic->vdev, addr->addr_bytes);
		if (ret)
			return ret;
	}
	/* 2. Add addresses on the host but not on the NIC */
	for (i = 0; i < nb_mc_addr; i++) {
		addr = &mc_addr_set[i];
		for (j = 0; j < enic->mc_count; j++) {
			if (rte_is_same_ether_addr(addr, &enic->mc_addrs[j]))
				break;
		}
		if (j < enic->mc_count)
			continue;
		debug_log_add_del_addr(addr, true);
		ret = vnic_dev_add_addr(enic->vdev, addr->addr_bytes);
		if (ret)
			return ret;
	}
	/* Keep a copy so we can flush/apply later on.. */
	memcpy(enic->mc_addrs, mc_addr_set,
	       nb_mc_addr * sizeof(struct rte_ether_addr));
	enic->mc_count = nb_mc_addr;
	return 0;
}

static int enicpmd_mtu_set(struct rte_eth_dev *eth_dev, uint16_t mtu)
{
	struct enic *enic = pmd_priv(eth_dev);

	ENICPMD_FUNC_TRACE();
	return enic_set_mtu(enic, mtu);
}

static int enicpmd_dev_rss_reta_query(struct rte_eth_dev *dev,
				      struct rte_eth_rss_reta_entry64
				      *reta_conf,
				      uint16_t reta_size)
{
	struct enic *enic = pmd_priv(dev);
	uint16_t i, idx, shift;

	ENICPMD_FUNC_TRACE();
	if (reta_size != ENIC_RSS_RETA_SIZE) {
		dev_err(enic, "reta_query: wrong reta_size. given=%u expected=%u\n",
			reta_size, ENIC_RSS_RETA_SIZE);
		return -EINVAL;
	}

	for (i = 0; i < reta_size; i++) {
		idx = i / RTE_ETH_RETA_GROUP_SIZE;
		shift = i % RTE_ETH_RETA_GROUP_SIZE;
		if (reta_conf[idx].mask & (1ULL << shift))
			reta_conf[idx].reta[shift] = enic_sop_rq_idx_to_rte_idx(
				enic->rss_cpu.cpu[i / 4].b[i % 4]);
	}

	return 0;
}

static int enicpmd_dev_rss_reta_update(struct rte_eth_dev *dev,
				       struct rte_eth_rss_reta_entry64
				       *reta_conf,
				       uint16_t reta_size)
{
	struct enic *enic = pmd_priv(dev);
	union vnic_rss_cpu rss_cpu;
	uint16_t i, idx, shift;

	ENICPMD_FUNC_TRACE();
	if (reta_size != ENIC_RSS_RETA_SIZE) {
		dev_err(enic, "reta_update: wrong reta_size. given=%u"
			" expected=%u\n",
			reta_size, ENIC_RSS_RETA_SIZE);
		return -EINVAL;
	}
	/*
	 * Start with the current reta and modify it per reta_conf, as we
	 * need to push the entire reta even if we only modify one entry.
	 */
	rss_cpu = enic->rss_cpu;
	for (i = 0; i < reta_size; i++) {
		idx = i / RTE_ETH_RETA_GROUP_SIZE;
		shift = i % RTE_ETH_RETA_GROUP_SIZE;
		if (reta_conf[idx].mask & (1ULL << shift))
			rss_cpu.cpu[i / 4].b[i % 4] =
				enic_rte_rq_idx_to_sop_idx(
					reta_conf[idx].reta[shift]);
	}
	return enic_set_rss_reta(enic, &rss_cpu);
}

static int enicpmd_dev_rss_hash_update(struct rte_eth_dev *dev,
				       struct rte_eth_rss_conf *rss_conf)
{
	struct enic *enic = pmd_priv(dev);

	ENICPMD_FUNC_TRACE();
	return enic_set_rss_conf(enic, rss_conf);
}

static int enicpmd_dev_rss_hash_conf_get(struct rte_eth_dev *dev,
					 struct rte_eth_rss_conf *rss_conf)
{
	struct enic *enic = pmd_priv(dev);

	ENICPMD_FUNC_TRACE();
	if (rss_conf == NULL)
		return -EINVAL;
	if (rss_conf->rss_key != NULL &&
	    rss_conf->rss_key_len < ENIC_RSS_HASH_KEY_SIZE) {
		dev_err(enic, "rss_hash_conf_get: wrong rss_key_len. given=%u"
			" expected=%u+\n",
			rss_conf->rss_key_len, ENIC_RSS_HASH_KEY_SIZE);
		return -EINVAL;
	}
	rss_conf->rss_hf = enic->rss_hf;
	if (rss_conf->rss_key != NULL) {
		int i;
		for (i = 0; i < ENIC_RSS_HASH_KEY_SIZE; i++) {
			rss_conf->rss_key[i] =
				enic->rss_key.key[i / 10].b[i % 10];
		}
		rss_conf->rss_key_len = ENIC_RSS_HASH_KEY_SIZE;
	}
	return 0;
}

static void enicpmd_dev_rxq_info_get(struct rte_eth_dev *dev,
				     uint16_t rx_queue_id,
				     struct rte_eth_rxq_info *qinfo)
{
	struct enic *enic = pmd_priv(dev);
	struct vnic_rq *rq_sop;
	struct vnic_rq *rq_data;
	struct rte_eth_rxconf *conf;
	uint16_t sop_queue_idx;
	uint16_t data_queue_idx;

	ENICPMD_FUNC_TRACE();
	sop_queue_idx = enic_rte_rq_idx_to_sop_idx(rx_queue_id);
	data_queue_idx = enic_rte_rq_idx_to_data_idx(rx_queue_id, enic);
	rq_sop = &enic->rq[sop_queue_idx];
	rq_data = &enic->rq[data_queue_idx]; /* valid if data_queue_enable */
	qinfo->mp = rq_sop->mp;
	qinfo->scattered_rx = rq_sop->data_queue_enable;
	qinfo->nb_desc = rq_sop->ring.desc_count;
	if (qinfo->scattered_rx)
		qinfo->nb_desc += rq_data->ring.desc_count;
	conf = &qinfo->conf;
	memset(conf, 0, sizeof(*conf));
	conf->rx_free_thresh = rq_sop->rx_free_thresh;
	conf->rx_drop_en = 1;
	/*
	 * Except VLAN stripping (port setting), all the checksum offloads
	 * are always enabled.
	 */
	conf->offloads = enic->rx_offload_capa;
	if (!enic->ig_vlan_strip_en)
		conf->offloads &= ~RTE_ETH_RX_OFFLOAD_VLAN_STRIP;
	/* rx_thresh and other fields are not applicable for enic */
}

static void enicpmd_dev_txq_info_get(struct rte_eth_dev *dev,
				     uint16_t tx_queue_id,
				     struct rte_eth_txq_info *qinfo)
{
	struct enic *enic = pmd_priv(dev);
	struct vnic_wq *wq = &enic->wq[tx_queue_id];

	ENICPMD_FUNC_TRACE();
	qinfo->nb_desc = wq->ring.desc_count;
	memset(&qinfo->conf, 0, sizeof(qinfo->conf));
	qinfo->conf.offloads = wq->offloads;
	/* tx_thresh, and all the other fields are not applicable for enic */
}

static int enicpmd_dev_rx_burst_mode_get(struct rte_eth_dev *dev,
					 __rte_unused uint16_t queue_id,
					 struct rte_eth_burst_mode *mode)
{
	eth_rx_burst_t pkt_burst = dev->rx_pkt_burst;
	struct enic *enic = pmd_priv(dev);
	const char *info_str = NULL;
	int ret = -EINVAL;

	ENICPMD_FUNC_TRACE();
	if (enic->use_noscatter_vec_rx_handler)
		info_str = "Vector AVX2 No Scatter";
	else if (pkt_burst == enic_noscatter_recv_pkts)
		info_str = "Scalar No Scatter";
	else if (pkt_burst == enic_recv_pkts)
		info_str = "Scalar";
	else if (pkt_burst == enic_recv_pkts_64)
		info_str = "Scalar 64B Completion";
	if (info_str) {
		strlcpy(mode->info, info_str, sizeof(mode->info));
		ret = 0;
	}
	return ret;
}

static int enicpmd_dev_tx_burst_mode_get(struct rte_eth_dev *dev,
					 __rte_unused uint16_t queue_id,
					 struct rte_eth_burst_mode *mode)
{
	eth_tx_burst_t pkt_burst = dev->tx_pkt_burst;
	const char *info_str = NULL;
	int ret = -EINVAL;

	ENICPMD_FUNC_TRACE();
	if (pkt_burst == enic_simple_xmit_pkts)
		info_str = "Scalar Simplified";
	else if (pkt_burst == enic_xmit_pkts)
		info_str = "Scalar";
	if (info_str) {
		strlcpy(mode->info, info_str, sizeof(mode->info));
		ret = 0;
	}
	return ret;
}

static int enicpmd_dev_rx_queue_intr_enable(struct rte_eth_dev *eth_dev,
					    uint16_t rx_queue_id)
{
	struct enic *enic = pmd_priv(eth_dev);

	ENICPMD_FUNC_TRACE();
	vnic_intr_unmask(&enic->intr[rx_queue_id + ENICPMD_RXQ_INTR_OFFSET]);
	return 0;
}

static int enicpmd_dev_rx_queue_intr_disable(struct rte_eth_dev *eth_dev,
					     uint16_t rx_queue_id)
{
	struct enic *enic = pmd_priv(eth_dev);

	ENICPMD_FUNC_TRACE();
	vnic_intr_mask(&enic->intr[rx_queue_id + ENICPMD_RXQ_INTR_OFFSET]);
	return 0;
}

static int udp_tunnel_common_check(struct enic *enic,
				   struct rte_eth_udp_tunnel *tnl)
{
	if (tnl->prot_type != RTE_ETH_TUNNEL_TYPE_VXLAN &&
	    tnl->prot_type != RTE_ETH_TUNNEL_TYPE_GENEVE)
		return -ENOTSUP;
	if (!enic->overlay_offload) {
		ENICPMD_LOG(DEBUG, " overlay offload is not supported\n");
		return -ENOTSUP;
	}
	return 0;
}

static int update_tunnel_port(struct enic *enic, uint16_t port, bool vxlan)
{
	uint8_t cfg;

	cfg = vxlan ? OVERLAY_CFG_VXLAN_PORT_UPDATE :
		OVERLAY_CFG_GENEVE_PORT_UPDATE;
	if (vnic_dev_overlay_offload_cfg(enic->vdev, cfg, port)) {
		ENICPMD_LOG(DEBUG, " failed to update tunnel port\n");
		return -EINVAL;
	}
	ENICPMD_LOG(DEBUG, " updated %s port to %u\n",
		    vxlan ? "vxlan" : "geneve", port);
	if (vxlan)
		enic->vxlan_port = port;
	else
		enic->geneve_port = port;
	return 0;
}

static int enicpmd_dev_udp_tunnel_port_add(struct rte_eth_dev *eth_dev,
					   struct rte_eth_udp_tunnel *tnl)
{
	struct enic *enic = pmd_priv(eth_dev);
	uint16_t port;
	bool vxlan;
	int ret;

	ENICPMD_FUNC_TRACE();
	ret = udp_tunnel_common_check(enic, tnl);
	if (ret)
		return ret;
	vxlan = (tnl->prot_type == RTE_ETH_TUNNEL_TYPE_VXLAN);
	if (vxlan)
		port = enic->vxlan_port;
	else
		port = enic->geneve_port;
	/*
	 * The NIC has 1 configurable port number per tunnel type.
	 * "Adding" a new port number replaces it.
	 */
	if (tnl->udp_port == port || tnl->udp_port == 0) {
		ENICPMD_LOG(DEBUG, " %u is already configured or invalid\n",
			     tnl->udp_port);
		return -EINVAL;
	}
	return update_tunnel_port(enic, tnl->udp_port, vxlan);
}

static int enicpmd_dev_udp_tunnel_port_del(struct rte_eth_dev *eth_dev,
					   struct rte_eth_udp_tunnel *tnl)
{
	struct enic *enic = pmd_priv(eth_dev);
	uint16_t port;
	bool vxlan;
	int ret;

	ENICPMD_FUNC_TRACE();
	ret = udp_tunnel_common_check(enic, tnl);
	if (ret)
		return ret;
	vxlan = (tnl->prot_type == RTE_ETH_TUNNEL_TYPE_VXLAN);
	if (vxlan)
		port = enic->vxlan_port;
	else
		port = enic->geneve_port;
	/*
	 * Clear the previously set port number and restore the
	 * hardware default port number. Some drivers disable VXLAN
	 * offloads when there are no configured port numbers. But
	 * enic does not do that as VXLAN is part of overlay offload,
	 * which is tied to inner RSS and TSO.
	 */
	if (tnl->udp_port != port) {
		ENICPMD_LOG(DEBUG, " %u is not a configured tunnel port\n",
			     tnl->udp_port);
		return -EINVAL;
	}
	port = vxlan ? RTE_VXLAN_DEFAULT_PORT : RTE_GENEVE_DEFAULT_PORT;
	return update_tunnel_port(enic, port, vxlan);
}

static int enicpmd_dev_fw_version_get(struct rte_eth_dev *eth_dev,
				      char *fw_version, size_t fw_size)
{
	struct vnic_devcmd_fw_info *info;
	struct enic *enic;
	int ret;

	ENICPMD_FUNC_TRACE();

	enic = pmd_priv(eth_dev);
	ret = vnic_dev_fw_info(enic->vdev, &info);
	if (ret)
		return ret;
	ret = snprintf(fw_version, fw_size, "%s %s",
		 info->fw_version, info->fw_build);
	if (ret < 0)
		return -EINVAL;

	ret += 1; /* add the size of '\0' */
	if (fw_size < (size_t)ret)
		return ret;
	else
		return 0;
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
	.vlan_filter_set      = NULL,
	.vlan_tpid_set        = NULL,
	.vlan_offload_set     = enicpmd_vlan_offload_set,
	.vlan_strip_queue_set = NULL,
	.rx_queue_start       = enicpmd_dev_rx_queue_start,
	.rx_queue_stop        = enicpmd_dev_rx_queue_stop,
	.tx_queue_start       = enicpmd_dev_tx_queue_start,
	.tx_queue_stop        = enicpmd_dev_tx_queue_stop,
	.rx_queue_setup       = enicpmd_dev_rx_queue_setup,
	.rx_queue_release     = enicpmd_dev_rx_queue_release,
	.tx_queue_setup       = enicpmd_dev_tx_queue_setup,
	.tx_queue_release     = enicpmd_dev_tx_queue_release,
	.rx_queue_intr_enable = enicpmd_dev_rx_queue_intr_enable,
	.rx_queue_intr_disable = enicpmd_dev_rx_queue_intr_disable,
	.rxq_info_get         = enicpmd_dev_rxq_info_get,
	.txq_info_get         = enicpmd_dev_txq_info_get,
	.rx_burst_mode_get    = enicpmd_dev_rx_burst_mode_get,
	.tx_burst_mode_get    = enicpmd_dev_tx_burst_mode_get,
	.dev_led_on           = NULL,
	.dev_led_off          = NULL,
	.flow_ctrl_get        = NULL,
	.flow_ctrl_set        = NULL,
	.priority_flow_ctrl_set = NULL,
	.mac_addr_add         = enicpmd_add_mac_addr,
	.mac_addr_remove      = enicpmd_remove_mac_addr,
	.mac_addr_set         = enicpmd_set_mac_addr,
	.set_mc_addr_list     = enicpmd_set_mc_addr_list,
	.flow_ops_get         = enicpmd_dev_flow_ops_get,
	.reta_query           = enicpmd_dev_rss_reta_query,
	.reta_update          = enicpmd_dev_rss_reta_update,
	.rss_hash_conf_get    = enicpmd_dev_rss_hash_conf_get,
	.rss_hash_update      = enicpmd_dev_rss_hash_update,
	.udp_tunnel_port_add  = enicpmd_dev_udp_tunnel_port_add,
	.udp_tunnel_port_del  = enicpmd_dev_udp_tunnel_port_del,
	.fw_version_get       = enicpmd_dev_fw_version_get,
};

static int enic_parse_zero_one(const char *key,
			       const char *value,
			       void *opaque)
{
	struct enic *enic;
	bool b;

	enic = (struct enic *)opaque;
	if (strcmp(value, "0") == 0) {
		b = false;
	} else if (strcmp(value, "1") == 0) {
		b = true;
	} else {
		dev_err(enic, "Invalid value for %s"
			": expected=0|1 given=%s\n", key, value);
		return -EINVAL;
	}
	if (strcmp(key, ENIC_DEVARG_CQ64) == 0)
		enic->cq64_request = b;
	if (strcmp(key, ENIC_DEVARG_DISABLE_OVERLAY) == 0)
		enic->disable_overlay = b;
	if (strcmp(key, ENIC_DEVARG_ENABLE_AVX2_RX) == 0)
		enic->enable_avx2_rx = b;
	return 0;
}

static int enic_parse_ig_vlan_rewrite(__rte_unused const char *key,
				      const char *value,
				      void *opaque)
{
	struct enic *enic;

	enic = (struct enic *)opaque;
	if (strcmp(value, "trunk") == 0) {
		/* Trunk mode: always tag */
		enic->ig_vlan_rewrite_mode = IG_VLAN_REWRITE_MODE_DEFAULT_TRUNK;
	} else if (strcmp(value, "untag") == 0) {
		/* Untag default VLAN mode: untag if VLAN = default VLAN */
		enic->ig_vlan_rewrite_mode =
			IG_VLAN_REWRITE_MODE_UNTAG_DEFAULT_VLAN;
	} else if (strcmp(value, "priority") == 0) {
		/*
		 * Priority-tag default VLAN mode: priority tag (VLAN header
		 * with ID=0) if VLAN = default
		 */
		enic->ig_vlan_rewrite_mode =
			IG_VLAN_REWRITE_MODE_PRIORITY_TAG_DEFAULT_VLAN;
	} else if (strcmp(value, "pass") == 0) {
		/* Pass through mode: do not touch tags */
		enic->ig_vlan_rewrite_mode = IG_VLAN_REWRITE_MODE_PASS_THRU;
	} else {
		dev_err(enic, "Invalid value for " ENIC_DEVARG_IG_VLAN_REWRITE
			": expected=trunk|untag|priority|pass given=%s\n",
			value);
		return -EINVAL;
	}
	return 0;
}

static int enic_check_devargs(struct rte_eth_dev *dev)
{
	static const char *const valid_keys[] = {
		ENIC_DEVARG_CQ64,
		ENIC_DEVARG_DISABLE_OVERLAY,
		ENIC_DEVARG_ENABLE_AVX2_RX,
		ENIC_DEVARG_IG_VLAN_REWRITE,
		ENIC_DEVARG_REPRESENTOR,
		NULL};
	struct enic *enic = pmd_priv(dev);
	struct rte_kvargs *kvlist;

	ENICPMD_FUNC_TRACE();

	enic->cq64_request = true; /* Use 64B entry if available */
	enic->disable_overlay = false;
	enic->enable_avx2_rx = false;
	enic->ig_vlan_rewrite_mode = IG_VLAN_REWRITE_MODE_PASS_THRU;
	if (!dev->device->devargs)
		return 0;
	kvlist = rte_kvargs_parse(dev->device->devargs->args, valid_keys);
	if (!kvlist)
		return -EINVAL;
	if (rte_kvargs_process(kvlist, ENIC_DEVARG_CQ64,
			       enic_parse_zero_one, enic) < 0 ||
	    rte_kvargs_process(kvlist, ENIC_DEVARG_DISABLE_OVERLAY,
			       enic_parse_zero_one, enic) < 0 ||
	    rte_kvargs_process(kvlist, ENIC_DEVARG_ENABLE_AVX2_RX,
			       enic_parse_zero_one, enic) < 0 ||
	    rte_kvargs_process(kvlist, ENIC_DEVARG_IG_VLAN_REWRITE,
			       enic_parse_ig_vlan_rewrite, enic) < 0) {
		rte_kvargs_free(kvlist);
		return -EINVAL;
	}
	rte_kvargs_free(kvlist);
	return 0;
}

/* Initialize the driver for PF */
static int eth_enic_dev_init(struct rte_eth_dev *eth_dev,
			     void *init_params __rte_unused)
{
	struct rte_pci_device *pdev;
	struct rte_pci_addr *addr;
	struct enic *enic = pmd_priv(eth_dev);
	int err;

	ENICPMD_FUNC_TRACE();
	eth_dev->dev_ops = &enicpmd_eth_dev_ops;
	eth_dev->rx_queue_count = enicpmd_dev_rx_queue_count;
	eth_dev->rx_pkt_burst = &enic_recv_pkts;
	eth_dev->tx_pkt_burst = &enic_xmit_pkts;
	eth_dev->tx_pkt_prepare = &enic_prep_pkts;
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		enic_pick_tx_handler(eth_dev);
		enic_pick_rx_handler(eth_dev);
		return 0;
	}
	/* Only the primary sets up adapter and other data in shared memory */
	enic->port_id = eth_dev->data->port_id;
	enic->rte_dev = eth_dev;
	enic->dev_data = eth_dev->data;

	pdev = RTE_ETH_DEV_TO_PCI(eth_dev);
	rte_eth_copy_pci_info(eth_dev, pdev);
	enic->pdev = pdev;
	addr = &pdev->addr;

	snprintf(enic->bdf_name, ENICPMD_BDF_LENGTH, "%04x:%02x:%02x.%x",
		addr->domain, addr->bus, addr->devid, addr->function);

	err = enic_check_devargs(eth_dev);
	if (err)
		return err;
	err = enic_probe(enic);
	if (!err && enic->fm) {
		err = enic_fm_allocate_switch_domain(enic);
		if (err)
			ENICPMD_LOG(ERR, "failed to allocate switch domain id");
	}
	return err;
}

static int eth_enic_dev_uninit(struct rte_eth_dev *eth_dev)
{
	struct enic *enic = pmd_priv(eth_dev);
	int err;

	ENICPMD_FUNC_TRACE();
	eth_dev->device = NULL;
	eth_dev->intr_handle = NULL;
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;
	err = rte_eth_switch_domain_free(enic->switch_domain_id);
	if (err)
		ENICPMD_LOG(WARNING, "failed to free switch domain: %d", err);
	return 0;
}

static int eth_enic_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	struct rte_pci_device *pci_dev)
{
	char name[RTE_ETH_NAME_MAX_LEN];
	struct rte_eth_devargs eth_da = { .nb_representor_ports = 0 };
	struct rte_eth_dev *pf_ethdev;
	struct enic *pf_enic;
	int i, retval;

	ENICPMD_FUNC_TRACE();
	if (pci_dev->device.devargs) {
		retval = rte_eth_devargs_parse(pci_dev->device.devargs->args,
				&eth_da);
		if (retval)
			return retval;
	}
	if (eth_da.nb_representor_ports > 0 &&
	    eth_da.type != RTE_ETH_REPRESENTOR_VF) {
		ENICPMD_LOG(ERR, "unsupported representor type: %s\n",
			    pci_dev->device.devargs->args);
		return -ENOTSUP;
	}
	retval = rte_eth_dev_create(&pci_dev->device, pci_dev->device.name,
		sizeof(struct enic),
		eth_dev_pci_specific_init, pci_dev,
		eth_enic_dev_init, NULL);
	if (retval || eth_da.nb_representor_ports < 1)
		return retval;

	/* Probe VF representor */
	pf_ethdev = rte_eth_dev_allocated(pci_dev->device.name);
	if (pf_ethdev == NULL)
		return -ENODEV;
	/* Representors require flowman */
	pf_enic = pmd_priv(pf_ethdev);
	if (pf_enic->fm == NULL) {
		ENICPMD_LOG(ERR, "VF representors require flowman");
		return -ENOTSUP;
	}
	/*
	 * For now representors imply switchdev, as firmware does not support
	 * legacy mode SR-IOV
	 */
	pf_enic->switchdev_mode = 1;
	/* Calculate max VF ID before initializing representor*/
	pf_enic->max_vf_id = 0;
	for (i = 0; i < eth_da.nb_representor_ports; i++) {
		pf_enic->max_vf_id = RTE_MAX(pf_enic->max_vf_id,
					     eth_da.representor_ports[i]);
	}
	for (i = 0; i < eth_da.nb_representor_ports; i++) {
		struct enic_vf_representor representor;

		representor.vf_id = eth_da.representor_ports[i];
				representor.switch_domain_id =
			pmd_priv(pf_ethdev)->switch_domain_id;
		representor.pf = pmd_priv(pf_ethdev);
		snprintf(name, sizeof(name), "net_%s_representor_%d",
			pci_dev->device.name, eth_da.representor_ports[i]);
		retval = rte_eth_dev_create(&pci_dev->device, name,
			sizeof(struct enic_vf_representor), NULL, NULL,
			enic_vf_representor_init, &representor);
		if (retval) {
			ENICPMD_LOG(ERR, "failed to create enic vf representor %s",
				    name);
			return retval;
		}
	}
	return 0;
}

static int eth_enic_pci_remove(struct rte_pci_device *pci_dev)
{
	struct rte_eth_dev *ethdev;

	ENICPMD_FUNC_TRACE();
	ethdev = rte_eth_dev_allocated(pci_dev->device.name);
	if (!ethdev)
		return -ENODEV;
	if (ethdev->data->dev_flags & RTE_ETH_DEV_REPRESENTOR)
		return rte_eth_dev_destroy(ethdev, enic_vf_representor_uninit);
	else
		return rte_eth_dev_destroy(ethdev, eth_enic_dev_uninit);
}

static struct rte_pci_driver rte_enic_pmd = {
	.id_table = pci_id_enic_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
	.probe = eth_enic_pci_probe,
	.remove = eth_enic_pci_remove,
};

int dev_is_enic(struct rte_eth_dev *dev)
{
	return dev->device->driver == &rte_enic_pmd.driver;
}

RTE_PMD_REGISTER_PCI(net_enic, rte_enic_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_enic, pci_id_enic_map);
RTE_PMD_REGISTER_KMOD_DEP(net_enic, "* igb_uio | uio_pci_generic | vfio-pci");
RTE_PMD_REGISTER_PARAM_STRING(net_enic,
	ENIC_DEVARG_CQ64 "=0|1"
	ENIC_DEVARG_DISABLE_OVERLAY "=0|1 "
	ENIC_DEVARG_ENABLE_AVX2_RX "=0|1 "
	ENIC_DEVARG_IG_VLAN_REWRITE "=trunk|untag|priority|pass");
