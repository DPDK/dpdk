/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include <errno.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <rte_interrupts.h>
#include <rte_debug.h>
#include <rte_pci.h>
#include <rte_atomic.h>
#include <rte_eal.h>
#include <rte_ether.h>
#include <rte_ethdev_pci.h>
#include <rte_kvargs.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_dev.h>

#include <iavf_devids.h>

#include "ice_generic_flow.h"
#include "ice_dcf_ethdev.h"

static uint16_t
ice_dcf_recv_pkts(__rte_unused void *rx_queue,
		  __rte_unused struct rte_mbuf **bufs,
		  __rte_unused uint16_t nb_pkts)
{
	return 0;
}

static uint16_t
ice_dcf_xmit_pkts(__rte_unused void *tx_queue,
		  __rte_unused struct rte_mbuf **bufs,
		  __rte_unused uint16_t nb_pkts)
{
	return 0;
}

static int
ice_dcf_dev_start(struct rte_eth_dev *dev)
{
	dev->data->dev_link.link_status = ETH_LINK_UP;

	return 0;
}

static void
ice_dcf_dev_stop(struct rte_eth_dev *dev)
{
	dev->data->dev_link.link_status = ETH_LINK_DOWN;
}

static int
ice_dcf_dev_configure(__rte_unused struct rte_eth_dev *dev)
{
	return 0;
}

static int
ice_dcf_dev_info_get(struct rte_eth_dev *dev,
		     struct rte_eth_dev_info *dev_info)
{
	struct ice_dcf_adapter *adapter = dev->data->dev_private;

	dev_info->max_mac_addrs = 1;
	dev_info->max_rx_pktlen = (uint32_t)-1;
	dev_info->max_rx_queues = RTE_DIM(adapter->rxqs);
	dev_info->max_tx_queues = RTE_DIM(adapter->txqs);

	return 0;
}

static int
ice_dcf_stats_get(__rte_unused struct rte_eth_dev *dev,
		  __rte_unused struct rte_eth_stats *igb_stats)
{
	return 0;
}

static int
ice_dcf_stats_reset(__rte_unused struct rte_eth_dev *dev)
{
	return 0;
}

static int
ice_dcf_dev_promiscuous_enable(__rte_unused struct rte_eth_dev *dev)
{
	return 0;
}

static int
ice_dcf_dev_promiscuous_disable(__rte_unused struct rte_eth_dev *dev)
{
	return 0;
}

static int
ice_dcf_dev_allmulticast_enable(__rte_unused struct rte_eth_dev *dev)
{
	return 0;
}

static int
ice_dcf_dev_allmulticast_disable(__rte_unused struct rte_eth_dev *dev)
{
	return 0;
}

static int
ice_dcf_dev_filter_ctrl(struct rte_eth_dev *dev,
			enum rte_filter_type filter_type,
			__rte_unused enum rte_filter_op filter_op,
			__rte_unused void *arg)
{
	int ret = 0;

	if (!dev)
		return -EINVAL;

	switch (filter_type) {
	default:
		PMD_DRV_LOG(WARNING, "Filter type (%d) not supported",
			    filter_type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static void
ice_dcf_dev_close(struct rte_eth_dev *dev)
{
	struct ice_dcf_adapter *adapter = dev->data->dev_private;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return;

	dev->dev_ops = NULL;
	dev->rx_pkt_burst = NULL;
	dev->tx_pkt_burst = NULL;

	ice_dcf_uninit_parent_adapter(dev);
	ice_dcf_uninit_hw(dev, &adapter->real_hw);
}

static void
ice_dcf_queue_release(__rte_unused void *q)
{
}

static int
ice_dcf_link_update(__rte_unused struct rte_eth_dev *dev,
		    __rte_unused int wait_to_complete)
{
	return 0;
}

static int
ice_dcf_rx_queue_setup(struct rte_eth_dev *dev,
		       uint16_t rx_queue_id,
		       __rte_unused uint16_t nb_rx_desc,
		       __rte_unused unsigned int socket_id,
		       __rte_unused const struct rte_eth_rxconf *rx_conf,
		       __rte_unused struct rte_mempool *mb_pool)
{
	struct ice_dcf_adapter *adapter = dev->data->dev_private;

	dev->data->rx_queues[rx_queue_id] = &adapter->rxqs[rx_queue_id];

	return 0;
}

static int
ice_dcf_tx_queue_setup(struct rte_eth_dev *dev,
		       uint16_t tx_queue_id,
		       __rte_unused uint16_t nb_tx_desc,
		       __rte_unused unsigned int socket_id,
		       __rte_unused const struct rte_eth_txconf *tx_conf)
{
	struct ice_dcf_adapter *adapter = dev->data->dev_private;

	dev->data->tx_queues[tx_queue_id] = &adapter->txqs[tx_queue_id];

	return 0;
}

static const struct eth_dev_ops ice_dcf_eth_dev_ops = {
	.dev_start               = ice_dcf_dev_start,
	.dev_stop                = ice_dcf_dev_stop,
	.dev_close               = ice_dcf_dev_close,
	.dev_configure           = ice_dcf_dev_configure,
	.dev_infos_get           = ice_dcf_dev_info_get,
	.rx_queue_setup          = ice_dcf_rx_queue_setup,
	.tx_queue_setup          = ice_dcf_tx_queue_setup,
	.rx_queue_release        = ice_dcf_queue_release,
	.tx_queue_release        = ice_dcf_queue_release,
	.link_update             = ice_dcf_link_update,
	.stats_get               = ice_dcf_stats_get,
	.stats_reset             = ice_dcf_stats_reset,
	.promiscuous_enable      = ice_dcf_dev_promiscuous_enable,
	.promiscuous_disable     = ice_dcf_dev_promiscuous_disable,
	.allmulticast_enable     = ice_dcf_dev_allmulticast_enable,
	.allmulticast_disable    = ice_dcf_dev_allmulticast_disable,
	.filter_ctrl             = ice_dcf_dev_filter_ctrl,
};

static int
ice_dcf_dev_init(struct rte_eth_dev *eth_dev)
{
	struct ice_dcf_adapter *adapter = eth_dev->data->dev_private;

	eth_dev->dev_ops = &ice_dcf_eth_dev_ops;
	eth_dev->rx_pkt_burst = ice_dcf_recv_pkts;
	eth_dev->tx_pkt_burst = ice_dcf_xmit_pkts;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	eth_dev->data->dev_flags |= RTE_ETH_DEV_CLOSE_REMOVE;

	adapter->real_hw.vc_event_msg_cb = ice_dcf_handle_pf_event_msg;
	if (ice_dcf_init_hw(eth_dev, &adapter->real_hw) != 0) {
		PMD_INIT_LOG(ERR, "Failed to init DCF hardware");
		return -1;
	}

	if (ice_dcf_init_parent_adapter(eth_dev) != 0) {
		PMD_INIT_LOG(ERR, "Failed to init DCF parent adapter");
		ice_dcf_uninit_hw(eth_dev, &adapter->real_hw);
		return -1;
	}

	return 0;
}

static int
ice_dcf_dev_uninit(struct rte_eth_dev *eth_dev)
{
	ice_dcf_dev_close(eth_dev);

	return 0;
}

static int
ice_dcf_cap_check_handler(__rte_unused const char *key,
			  const char *value, __rte_unused void *opaque)
{
	if (strcmp(value, "dcf"))
		return -1;

	return 0;
}

static int
ice_dcf_cap_selected(struct rte_devargs *devargs)
{
	struct rte_kvargs *kvlist;
	const char *key = "cap";
	int ret = 0;

	if (devargs == NULL)
		return 0;

	kvlist = rte_kvargs_parse(devargs->args, NULL);
	if (kvlist == NULL)
		return 0;

	if (!rte_kvargs_count(kvlist, key))
		goto exit;

	/* dcf capability selected when there's a key-value pair: cap=dcf */
	if (rte_kvargs_process(kvlist, key,
			       ice_dcf_cap_check_handler, NULL) < 0)
		goto exit;

	ret = 1;

exit:
	rte_kvargs_free(kvlist);
	return ret;
}

static int eth_ice_dcf_pci_probe(__rte_unused struct rte_pci_driver *pci_drv,
			     struct rte_pci_device *pci_dev)
{
	if (!ice_dcf_cap_selected(pci_dev->device.devargs))
		return 1;

	return rte_eth_dev_pci_generic_probe(pci_dev,
					     sizeof(struct ice_dcf_adapter),
					     ice_dcf_dev_init);
}

static int eth_ice_dcf_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, ice_dcf_dev_uninit);
}

static const struct rte_pci_id pci_id_ice_dcf_map[] = {
	{ RTE_PCI_DEVICE(IAVF_INTEL_VENDOR_ID, IAVF_DEV_ID_ADAPTIVE_VF) },
	{ .vendor_id = 0, /* sentinel */ },
};

static struct rte_pci_driver rte_ice_dcf_pmd = {
	.id_table = pci_id_ice_dcf_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = eth_ice_dcf_pci_probe,
	.remove = eth_ice_dcf_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_ice_dcf, rte_ice_dcf_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_ice_dcf, pci_id_ice_dcf_map);
RTE_PMD_REGISTER_KMOD_DEP(net_ice_dcf, "* igb_uio | vfio-pci");
RTE_PMD_REGISTER_PARAM_STRING(net_ice_dcf, "cap=dcf");
