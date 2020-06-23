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
#include "ice_rxtx.h"

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
ice_dcf_init_rxq(struct rte_eth_dev *dev, struct ice_rx_queue *rxq)
{
	struct ice_dcf_adapter *dcf_ad = dev->data->dev_private;
	struct rte_eth_dev_data *dev_data = dev->data;
	struct iavf_hw *hw = &dcf_ad->real_hw.avf;
	uint16_t buf_size, max_pkt_len, len;

	buf_size = rte_pktmbuf_data_room_size(rxq->mp) - RTE_PKTMBUF_HEADROOM;
	rxq->rx_hdr_len = 0;
	rxq->rx_buf_len = RTE_ALIGN(buf_size, (1 << ICE_RLAN_CTX_DBUF_S));
	len = ICE_SUPPORT_CHAIN_NUM * rxq->rx_buf_len;
	max_pkt_len = RTE_MIN(len, dev->data->dev_conf.rxmode.max_rx_pkt_len);

	/* Check if the jumbo frame and maximum packet length are set
	 * correctly.
	 */
	if (dev->data->dev_conf.rxmode.offloads & DEV_RX_OFFLOAD_JUMBO_FRAME) {
		if (max_pkt_len <= RTE_ETHER_MAX_LEN ||
		    max_pkt_len > ICE_FRAME_SIZE_MAX) {
			PMD_DRV_LOG(ERR, "maximum packet length must be "
				    "larger than %u and smaller than %u, "
				    "as jumbo frame is enabled",
				    (uint32_t)RTE_ETHER_MAX_LEN,
				    (uint32_t)ICE_FRAME_SIZE_MAX);
			return -EINVAL;
		}
	} else {
		if (max_pkt_len < RTE_ETHER_MIN_LEN ||
		    max_pkt_len > RTE_ETHER_MAX_LEN) {
			PMD_DRV_LOG(ERR, "maximum packet length must be "
				    "larger than %u and smaller than %u, "
				    "as jumbo frame is disabled",
				    (uint32_t)RTE_ETHER_MIN_LEN,
				    (uint32_t)RTE_ETHER_MAX_LEN);
			return -EINVAL;
		}
	}

	rxq->max_pkt_len = max_pkt_len;
	if ((dev_data->dev_conf.rxmode.offloads & DEV_RX_OFFLOAD_SCATTER) ||
	    (rxq->max_pkt_len + 2 * ICE_VLAN_TAG_SIZE) > buf_size) {
		dev_data->scattered_rx = 1;
	}
	rxq->qrx_tail = hw->hw_addr + IAVF_QRX_TAIL1(rxq->queue_id);
	IAVF_PCI_REG_WRITE(rxq->qrx_tail, rxq->nb_rx_desc - 1);
	IAVF_WRITE_FLUSH(hw);

	return 0;
}

static int
ice_dcf_init_rx_queues(struct rte_eth_dev *dev)
{
	struct ice_rx_queue **rxq =
		(struct ice_rx_queue **)dev->data->rx_queues;
	int i, ret;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		if (!rxq[i] || !rxq[i]->q_set)
			continue;
		ret = ice_dcf_init_rxq(dev, rxq[i]);
		if (ret)
			return ret;
	}

	ice_set_rx_function(dev);
	ice_set_tx_function(dev);

	return 0;
}

static int
ice_dcf_dev_start(struct rte_eth_dev *dev)
{
	struct ice_dcf_adapter *dcf_ad = dev->data->dev_private;
	struct ice_adapter *ad = &dcf_ad->parent;
	struct ice_dcf_hw *hw = &dcf_ad->real_hw;
	int ret;

	ad->pf.adapter_stopped = 0;

	hw->num_queue_pairs = RTE_MAX(dev->data->nb_rx_queues,
				      dev->data->nb_tx_queues);

	ret = ice_dcf_init_rx_queues(dev);
	if (ret) {
		PMD_DRV_LOG(ERR, "Fail to init queues");
		return ret;
	}

	if (hw->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_RSS_PF) {
		ret = ice_dcf_init_rss(hw);
		if (ret) {
			PMD_DRV_LOG(ERR, "Failed to configure RSS");
			return ret;
		}
	}

	dev->data->dev_link.link_status = ETH_LINK_UP;

	return 0;
}

static void
ice_dcf_dev_stop(struct rte_eth_dev *dev)
{
	struct ice_dcf_adapter *dcf_ad = dev->data->dev_private;
	struct ice_adapter *ad = &dcf_ad->parent;

	if (ad->pf.adapter_stopped == 1) {
		PMD_DRV_LOG(DEBUG, "Port is already stopped");
		return;
	}

	dev->data->dev_link.link_status = ETH_LINK_DOWN;
	ad->pf.adapter_stopped = 1;
}

static int
ice_dcf_dev_configure(struct rte_eth_dev *dev)
{
	struct ice_dcf_adapter *dcf_ad = dev->data->dev_private;
	struct ice_adapter *ad = &dcf_ad->parent;

	ad->rx_bulk_alloc_allowed = true;
	ad->tx_simple_allowed = true;

	if (dev->data->dev_conf.rxmode.mq_mode & ETH_MQ_RX_RSS_FLAG)
		dev->data->dev_conf.rxmode.offloads |= DEV_RX_OFFLOAD_RSS_HASH;

	return 0;
}

static int
ice_dcf_dev_info_get(struct rte_eth_dev *dev,
		     struct rte_eth_dev_info *dev_info)
{
	struct ice_dcf_adapter *adapter = dev->data->dev_private;
	struct ice_dcf_hw *hw = &adapter->real_hw;

	dev_info->max_mac_addrs = 1;
	dev_info->max_rx_queues = hw->vsi_res->num_queue_pairs;
	dev_info->max_tx_queues = hw->vsi_res->num_queue_pairs;
	dev_info->min_rx_bufsize = ICE_BUF_SIZE_MIN;
	dev_info->max_rx_pktlen = ICE_FRAME_SIZE_MAX;
	dev_info->hash_key_size = hw->vf_res->rss_key_size;
	dev_info->reta_size = hw->vf_res->rss_lut_size;
	dev_info->flow_type_rss_offloads = ICE_RSS_OFFLOAD_ALL;

	dev_info->rx_offload_capa =
		DEV_RX_OFFLOAD_VLAN_STRIP |
		DEV_RX_OFFLOAD_IPV4_CKSUM |
		DEV_RX_OFFLOAD_UDP_CKSUM |
		DEV_RX_OFFLOAD_TCP_CKSUM |
		DEV_RX_OFFLOAD_OUTER_IPV4_CKSUM |
		DEV_RX_OFFLOAD_SCATTER |
		DEV_RX_OFFLOAD_JUMBO_FRAME |
		DEV_RX_OFFLOAD_VLAN_FILTER |
		DEV_RX_OFFLOAD_RSS_HASH;
	dev_info->tx_offload_capa =
		DEV_TX_OFFLOAD_VLAN_INSERT |
		DEV_TX_OFFLOAD_IPV4_CKSUM |
		DEV_TX_OFFLOAD_UDP_CKSUM |
		DEV_TX_OFFLOAD_TCP_CKSUM |
		DEV_TX_OFFLOAD_SCTP_CKSUM |
		DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM |
		DEV_TX_OFFLOAD_TCP_TSO |
		DEV_TX_OFFLOAD_VXLAN_TNL_TSO |
		DEV_TX_OFFLOAD_GRE_TNL_TSO |
		DEV_TX_OFFLOAD_IPIP_TNL_TSO |
		DEV_TX_OFFLOAD_GENEVE_TNL_TSO |
		DEV_TX_OFFLOAD_MULTI_SEGS;

	dev_info->default_rxconf = (struct rte_eth_rxconf) {
		.rx_thresh = {
			.pthresh = ICE_DEFAULT_RX_PTHRESH,
			.hthresh = ICE_DEFAULT_RX_HTHRESH,
			.wthresh = ICE_DEFAULT_RX_WTHRESH,
		},
		.rx_free_thresh = ICE_DEFAULT_RX_FREE_THRESH,
		.rx_drop_en = 0,
		.offloads = 0,
	};

	dev_info->default_txconf = (struct rte_eth_txconf) {
		.tx_thresh = {
			.pthresh = ICE_DEFAULT_TX_PTHRESH,
			.hthresh = ICE_DEFAULT_TX_HTHRESH,
			.wthresh = ICE_DEFAULT_TX_WTHRESH,
		},
		.tx_free_thresh = ICE_DEFAULT_TX_FREE_THRESH,
		.tx_rs_thresh = ICE_DEFAULT_TX_RSBIT_THRESH,
		.offloads = 0,
	};

	dev_info->rx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = ICE_MAX_RING_DESC,
		.nb_min = ICE_MIN_RING_DESC,
		.nb_align = ICE_ALIGN_RING_DESC,
	};

	dev_info->tx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = ICE_MAX_RING_DESC,
		.nb_min = ICE_MIN_RING_DESC,
		.nb_align = ICE_ALIGN_RING_DESC,
	};

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
			enum rte_filter_op filter_op,
			void *arg)
{
	int ret = 0;

	if (!dev)
		return -EINVAL;

	switch (filter_type) {
	case RTE_ETH_FILTER_GENERIC:
		if (filter_op != RTE_ETH_FILTER_GET)
			return -EINVAL;
		*(const void **)arg = &ice_flow_ops;
		break;

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

static int
ice_dcf_link_update(__rte_unused struct rte_eth_dev *dev,
		    __rte_unused int wait_to_complete)
{
	return 0;
}

static const struct eth_dev_ops ice_dcf_eth_dev_ops = {
	.dev_start               = ice_dcf_dev_start,
	.dev_stop                = ice_dcf_dev_stop,
	.dev_close               = ice_dcf_dev_close,
	.dev_configure           = ice_dcf_dev_configure,
	.dev_infos_get           = ice_dcf_dev_info_get,
	.rx_queue_setup          = ice_rx_queue_setup,
	.tx_queue_setup          = ice_tx_queue_setup,
	.rx_queue_release        = ice_rx_queue_release,
	.tx_queue_release        = ice_tx_queue_release,
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
