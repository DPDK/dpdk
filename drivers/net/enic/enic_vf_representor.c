/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2019 Cisco Systems, Inc.  All rights reserved.
 */

#include <stdint.h>
#include <stdio.h>

#include <rte_bus_pci.h>
#include <rte_common.h>
#include <rte_dev.h>
#include <rte_ethdev_driver.h>
#include <rte_ethdev_pci.h>
#include <rte_flow_driver.h>
#include <rte_kvargs.h>
#include <rte_pci.h>
#include <rte_string_fns.h>

#include "enic_compat.h"
#include "enic.h"
#include "vnic_dev.h"
#include "vnic_enet.h"
#include "vnic_intr.h"
#include "vnic_cq.h"
#include "vnic_wq.h"
#include "vnic_rq.h"

static uint16_t enic_vf_recv_pkts(void *rx_queue __rte_unused,
				  struct rte_mbuf **rx_pkts __rte_unused,
				  uint16_t nb_pkts __rte_unused)
{
	return 0;
}

static uint16_t enic_vf_xmit_pkts(void *tx_queue __rte_unused,
				  struct rte_mbuf **tx_pkts __rte_unused,
				  uint16_t nb_pkts __rte_unused)
{
	return 0;
}

static int enic_vf_dev_tx_queue_setup(struct rte_eth_dev *eth_dev __rte_unused,
	uint16_t queue_idx __rte_unused,
	uint16_t nb_desc __rte_unused,
	unsigned int socket_id __rte_unused,
	const struct rte_eth_txconf *tx_conf __rte_unused)
{
	ENICPMD_FUNC_TRACE();
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -E_RTE_SECONDARY;
	return 0;
}

static void enic_vf_dev_tx_queue_release(void *txq __rte_unused)
{
	ENICPMD_FUNC_TRACE();
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return;
}

static int enic_vf_dev_rx_queue_setup(struct rte_eth_dev *eth_dev __rte_unused,
	uint16_t queue_idx __rte_unused,
	uint16_t nb_desc __rte_unused,
	unsigned int socket_id __rte_unused,
	const struct rte_eth_rxconf *rx_conf __rte_unused,
	struct rte_mempool *mp __rte_unused)
{
	ENICPMD_FUNC_TRACE();
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -E_RTE_SECONDARY;
	return 0;
}

static void enic_vf_dev_rx_queue_release(void *rxq __rte_unused)
{
	ENICPMD_FUNC_TRACE();
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return;
}

static int enic_vf_dev_configure(struct rte_eth_dev *eth_dev __rte_unused)
{
	ENICPMD_FUNC_TRACE();
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -E_RTE_SECONDARY;
	return 0;
}

static int enic_vf_dev_start(struct rte_eth_dev *eth_dev)
{
	struct enic_vf_representor *vf;
	int ret;

	ENICPMD_FUNC_TRACE();
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -E_RTE_SECONDARY;

	vf = eth_dev->data->dev_private;
	/* Remove all packet filters so no ingress packets go to VF.
	 * When PF enables switchdev, it will ensure packet filters
	 * are removed.  So, this is not technically needed.
	 */
	ENICPMD_LOG(DEBUG, "Clear packet filters");
	ret = vnic_dev_packet_filter(vf->enic.vdev, 0, 0, 0, 0, 0);
	if (ret) {
		ENICPMD_LOG(ERR, "Cannot clear packet filters");
		return ret;
	}
	return 0;
}

static void enic_vf_dev_stop(struct rte_eth_dev *eth_dev __rte_unused)
{
	ENICPMD_FUNC_TRACE();
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return;
}

/*
 * "close" is no-op for now and solely exists so that rte_eth_dev_close()
 * can finish its own cleanup without errors.
 */
static void enic_vf_dev_close(struct rte_eth_dev *eth_dev __rte_unused)
{
	ENICPMD_FUNC_TRACE();
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return;
}

static int enic_vf_link_update(struct rte_eth_dev *eth_dev,
	int wait_to_complete __rte_unused)
{
	struct enic_vf_representor *vf;
	struct rte_eth_link link;
	struct enic *pf;

	ENICPMD_FUNC_TRACE();
	vf = eth_dev->data->dev_private;
	pf = vf->pf;
	/*
	 * Link status and speed are same as PF. Update PF status and then
	 * copy it to VF.
	 */
	enic_link_update(pf->rte_dev);
	rte_eth_linkstatus_get(pf->rte_dev, &link);
	rte_eth_linkstatus_set(eth_dev, &link);
	return 0;
}

static int enic_vf_stats_get(struct rte_eth_dev *eth_dev,
	struct rte_eth_stats *stats)
{
	struct enic_vf_representor *vf;
	struct vnic_stats *vs;
	int err;

	ENICPMD_FUNC_TRACE();
	vf = eth_dev->data->dev_private;
	/* Get VF stats via PF */
	err = vnic_dev_stats_dump(vf->enic.vdev, &vs);
	if (err) {
		ENICPMD_LOG(ERR, "error in getting stats\n");
		return err;
	}
	stats->ipackets = vs->rx.rx_frames_ok;
	stats->opackets = vs->tx.tx_frames_ok;
	stats->ibytes = vs->rx.rx_bytes_ok;
	stats->obytes = vs->tx.tx_bytes_ok;
	stats->ierrors = vs->rx.rx_errors + vs->rx.rx_drop;
	stats->oerrors = vs->tx.tx_errors;
	stats->imissed = vs->rx.rx_no_bufs;
	return 0;
}

static int enic_vf_stats_reset(struct rte_eth_dev *eth_dev)
{
	struct enic_vf_representor *vf;
	int err;

	ENICPMD_FUNC_TRACE();
	vf = eth_dev->data->dev_private;
	/* Ask PF to clear VF stats */
	err = vnic_dev_stats_clear(vf->enic.vdev);
	if (err)
		ENICPMD_LOG(ERR, "error in clearing stats\n");
	return err;
}

static int enic_vf_dev_infos_get(struct rte_eth_dev *eth_dev,
	struct rte_eth_dev_info *device_info)
{
	struct enic_vf_representor *vf;
	struct enic *pf;

	ENICPMD_FUNC_TRACE();
	vf = eth_dev->data->dev_private;
	pf = vf->pf;
	device_info->max_rx_queues = eth_dev->data->nb_rx_queues;
	device_info->max_tx_queues = eth_dev->data->nb_tx_queues;
	device_info->min_rx_bufsize = ENIC_MIN_MTU;
	/* Max packet size is same as PF */
	device_info->max_rx_pktlen = enic_mtu_to_max_rx_pktlen(pf->max_mtu);
	device_info->max_mac_addrs = ENIC_UNICAST_PERFECT_FILTERS;
	/* No offload capa, RSS, etc. until Tx/Rx handlers are added */
	device_info->rx_offload_capa = 0;
	device_info->tx_offload_capa = 0;
	device_info->switch_info.name =	pf->rte_dev->device->name;
	device_info->switch_info.domain_id = vf->switch_domain_id;
	device_info->switch_info.port_id = vf->vf_id;
	return 0;
}

static void set_vf_packet_filter(struct enic_vf_representor *vf)
{
	/* switchdev: packet filters are ignored */
	if (vf->enic.switchdev_mode)
		return;
	/* Ask PF to apply filters on VF */
	vnic_dev_packet_filter(vf->enic.vdev, 1 /* unicast */, 1 /* mcast */,
		1 /* bcast */, vf->promisc, vf->allmulti);
}

static int enic_vf_promiscuous_enable(struct rte_eth_dev *eth_dev)
{
	struct enic_vf_representor *vf;

	ENICPMD_FUNC_TRACE();
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -E_RTE_SECONDARY;
	vf = eth_dev->data->dev_private;
	vf->promisc = 1;
	set_vf_packet_filter(vf);
	return 0;
}

static int enic_vf_promiscuous_disable(struct rte_eth_dev *eth_dev)
{
	struct enic_vf_representor *vf;

	ENICPMD_FUNC_TRACE();
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -E_RTE_SECONDARY;
	vf = eth_dev->data->dev_private;
	vf->promisc = 0;
	set_vf_packet_filter(vf);
	return 0;
}

static int enic_vf_allmulticast_enable(struct rte_eth_dev *eth_dev)
{
	struct enic_vf_representor *vf;

	ENICPMD_FUNC_TRACE();
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -E_RTE_SECONDARY;
	vf = eth_dev->data->dev_private;
	vf->allmulti = 1;
	set_vf_packet_filter(vf);
	return 0;
}

static int enic_vf_allmulticast_disable(struct rte_eth_dev *eth_dev)
{
	struct enic_vf_representor *vf;

	ENICPMD_FUNC_TRACE();
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -E_RTE_SECONDARY;
	vf = eth_dev->data->dev_private;
	vf->allmulti = 0;
	set_vf_packet_filter(vf);
	return 0;
}

/*
 * A minimal set of handlers.
 * The representor can get/set a small set of VF settings via "proxy" devcmd.
 * With proxy devcmd, the PF driver basically tells the VIC firmware to
 * "perform this devcmd on that VF".
 */
static const struct eth_dev_ops enic_vf_representor_dev_ops = {
	.allmulticast_enable  = enic_vf_allmulticast_enable,
	.allmulticast_disable = enic_vf_allmulticast_disable,
	.dev_configure        = enic_vf_dev_configure,
	.dev_infos_get        = enic_vf_dev_infos_get,
	.dev_start            = enic_vf_dev_start,
	.dev_stop             = enic_vf_dev_stop,
	.dev_close            = enic_vf_dev_close,
	.link_update          = enic_vf_link_update,
	.promiscuous_enable   = enic_vf_promiscuous_enable,
	.promiscuous_disable  = enic_vf_promiscuous_disable,
	.stats_get            = enic_vf_stats_get,
	.stats_reset          = enic_vf_stats_reset,
	.rx_queue_setup	      = enic_vf_dev_rx_queue_setup,
	.rx_queue_release     = enic_vf_dev_rx_queue_release,
	.tx_queue_setup	      = enic_vf_dev_tx_queue_setup,
	.tx_queue_release     = enic_vf_dev_tx_queue_release,
};

static int get_vf_config(struct enic_vf_representor *vf)
{
	struct vnic_enet_config *c;
	struct enic *pf;
	int switch_mtu;
	int err;

	c = &vf->config;
	pf = vf->pf;
	/* VF MAC */
	err = vnic_dev_get_mac_addr(vf->enic.vdev, vf->mac_addr.addr_bytes);
	if (err) {
		ENICPMD_LOG(ERR, "error in getting MAC address\n");
		return err;
	}
	rte_ether_addr_copy(&vf->mac_addr, vf->eth_dev->data->mac_addrs);

	/* VF MTU per its vNIC setting */
	err = vnic_dev_spec(vf->enic.vdev,
			    offsetof(struct vnic_enet_config, mtu),
			    sizeof(c->mtu), &c->mtu);
	if (err) {
		ENICPMD_LOG(ERR, "error in getting MTU\n");
		return err;
	}
	/*
	 * Blade switch (fabric interconnect) port's MTU. Assume the kernel
	 * enic driver runs on VF. That driver automatically adjusts its MTU
	 * according to the switch MTU.
	 */
	switch_mtu = vnic_dev_mtu(pf->vdev);
	vf->eth_dev->data->mtu = c->mtu;
	if (switch_mtu > c->mtu)
		vf->eth_dev->data->mtu = RTE_MIN(ENIC_MAX_MTU, switch_mtu);
	return 0;
}

int enic_vf_representor_init(struct rte_eth_dev *eth_dev, void *init_params)
{
	struct enic_vf_representor *vf, *params;
	struct rte_pci_device *pdev;
	struct enic *pf, *vf_enic;
	struct rte_pci_addr *addr;
	int ret;

	ENICPMD_FUNC_TRACE();
	params = init_params;
	vf = eth_dev->data->dev_private;
	vf->switch_domain_id = params->switch_domain_id;
	vf->vf_id = params->vf_id;
	vf->eth_dev = eth_dev;
	vf->pf = params->pf;
	vf->allmulti = 1;
	vf->promisc = 0;
	pf = vf->pf;
	vf->enic.switchdev_mode = pf->switchdev_mode;
	/* Only switchdev is supported now */
	RTE_ASSERT(vf->enic.switchdev_mode);

	/* Check for non-existent VFs */
	pdev = RTE_ETH_DEV_TO_PCI(pf->rte_dev);
	if (vf->vf_id >= pdev->max_vfs) {
		ENICPMD_LOG(ERR, "VF ID is invalid. vf_id %u max_vfs %u",
			    vf->vf_id, pdev->max_vfs);
		return -ENODEV;
	}

	eth_dev->device->driver = pf->rte_dev->device->driver;
	eth_dev->dev_ops = &enic_vf_representor_dev_ops;
	eth_dev->data->dev_flags |= RTE_ETH_DEV_REPRESENTOR
		| RTE_ETH_DEV_CLOSE_REMOVE;
	eth_dev->data->representor_id = vf->vf_id;
	eth_dev->data->mac_addrs = rte_zmalloc("enic_mac_addr_vf",
		sizeof(struct rte_ether_addr) *
		ENIC_UNICAST_PERFECT_FILTERS, 0);
	if (eth_dev->data->mac_addrs == NULL)
		return -ENOMEM;
	/* Use 1 RX queue and 1 TX queue for representor path */
	eth_dev->data->nb_rx_queues = 1;
	eth_dev->data->nb_tx_queues = 1;
	eth_dev->rx_pkt_burst = &enic_vf_recv_pkts;
	eth_dev->tx_pkt_burst = &enic_vf_xmit_pkts;
	/* Initial link state copied from PF */
	eth_dev->data->dev_link = pf->rte_dev->data->dev_link;
	/* Representor vdev to perform devcmd */
	vf->enic.vdev = vnic_vf_rep_register(&vf->enic, pf->vdev, vf->vf_id);
	if (vf->enic.vdev == NULL)
		return -ENOMEM;
	ret = vnic_dev_alloc_stats_mem(vf->enic.vdev);
	if (ret)
		return ret;
	/* Get/copy VF vNIC MAC, MTU, etc. into eth_dev */
	ret = get_vf_config(vf);
	if (ret)
		return ret;

	/*
	 * Calculate VF BDF. The firmware ensures that PF BDF is always
	 * bus:dev.0, and VF BDFs are dev.1, dev.2, and so on.
	 */
	vf->bdf = pdev->addr;
	vf->bdf.function += vf->vf_id + 1;

	/* Copy a few fields used by enic_fm_flow */
	vf_enic = &vf->enic;
	vf_enic->switch_domain_id = vf->switch_domain_id;
	vf_enic->flow_filter_mode = pf->flow_filter_mode;
	vf_enic->rte_dev = eth_dev;
	vf_enic->dev_data = eth_dev->data;
	LIST_INIT(&vf_enic->flows);
	LIST_INIT(&vf_enic->memzone_list);
	rte_spinlock_init(&vf_enic->memzone_list_lock);
	addr = &vf->bdf;
	snprintf(vf_enic->bdf_name, ENICPMD_BDF_LENGTH, "%04x:%02x:%02x.%x",
		 addr->domain, addr->bus, addr->devid, addr->function);
	return 0;
}

int enic_vf_representor_uninit(struct rte_eth_dev *eth_dev)
{
	struct enic_vf_representor *vf;

	ENICPMD_FUNC_TRACE();
	vf = eth_dev->data->dev_private;
	vnic_dev_unregister(vf->enic.vdev);
	return 0;
}
