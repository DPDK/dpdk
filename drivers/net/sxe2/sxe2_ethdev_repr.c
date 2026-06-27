/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#include "sxe2_ethdev_repr.h"
#include "sxe2_ethdev.h"
#include "sxe2_common.h"
#include "sxe2_common_log.h"
#include "sxe2_tx.h"
#include "sxe2_rx.h"
#include "sxe2_txrx.h"
#include "sxe2_switchdev.h"
#include "sxe2_mp.h"
#include "sxe2_dump.h"
#include "sxe2_stats.h"
#include "sxe2_flow.h"

static struct sxe2_pci_map_addr_info sxe2_net_map_addr_info_repr[SXE2_PCI_MAP_RES_MAX_COUNT] = {
	{0, 0, 0},
	{ SXE2_TXQ_LEGACY_DBLL(0), 0, 4},
	{ SXE2_RXQ_TAIL(0), 0, 4},
	{ SXE2_VF_DYN_CTL(0), 0, 4},
	{ SXE2_VF_INT_ITR(0, 0), 0, 4},
	{ SXE2_BAR4_MSIX_CTL(0), 4, 0x10},
};

static void sxe2_repr_dev_uinit(struct rte_eth_dev *dev);

static int32_t sxe2_repr_promisc_enable(struct rte_eth_dev *dev __rte_unused)
{
	return 0;
}
static int32_t sxe2_repr_promisc_disable(struct rte_eth_dev *dev __rte_unused)
{
	return 0;
}
static int32_t sxe2_repr_allmulti_enable(struct rte_eth_dev *dev __rte_unused)
{
	return 0;
}
static int32_t sxe2_repr_allmulti_disable(struct rte_eth_dev *dev __rte_unused)
{
	return 0;
}

static int32_t sxe2_repr_dev_configure(struct rte_eth_dev *dev)
{
	dev->data->mtu = SXE2_FRAME_SIZE_MAX - SXE2_ETH_OVERHEAD;
	return 0;
}

static int32_t sxe2_repr_dev_start(struct rte_eth_dev *dev)
{
	int32_t ret = 0;
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	PMD_INIT_FUNC_TRACE();

	ret = sxe2_queues_init(dev);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to init queues.");
		goto l_end;
	}

	sxe2_rx_mode_func_set(dev);
	sxe2_tx_mode_func_set(dev);

	ret = sxe2_link_update_init(dev);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to initialize link update, ret:%d", ret);
		goto l_end;
	}

	ret = sxe2_repr_rxq_intr_enable(dev);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to enable rx queue intr");
		goto l_end;
	}

	ret = sxe2_queues_start(dev);
	if (ret) {
		PMD_LOG_ERR(INIT, "enable queues failed");
		goto l_start_queues_err;
	}

	dev->data->dev_started = 1;
	adapter->started = 1;
	goto l_end;
l_start_queues_err:
	(void)sxe2_rxq_intr_disable(dev);
l_end:
	return ret;
}

static int32_t sxe2_repr_dev_stop(struct rte_eth_dev *dev)
{
	int32_t ret = 0;
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	PMD_INIT_FUNC_TRACE();

	if (adapter->started == 0)
		goto l_end;

	sxe2_repr_rxq_intr_disable(dev);

	sxe2_txqs_all_stop(dev);
	sxe2_rxqs_all_stop(dev);

	dev->data->dev_started = 0;
	adapter->started = 0;
l_end:
	return ret;
}

static int32_t sxe2_repr_dev_close(struct rte_eth_dev *dev)
{
	PMD_DEV_LOG_INFO(SXE2_DEV_PRIVATE_TO_ADAPTER(dev),
			 INIT, "repr close");
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		sxe2_mp_uninit(dev);
		goto l_end;
	}
	(void)sxe2_repr_dev_stop(dev);
	(void)sxe2_queues_release(dev);
	sxe2_mp_uninit(dev);
	sxe2_repr_dev_uinit(dev);
l_end:
	return 0;
}

static int32_t sxe2_repr_dev_infos_get(struct rte_eth_dev *dev,
			struct rte_eth_dev_info *dev_info)
{
	dev_info->max_rx_queues = 1;
	dev_info->max_tx_queues = 1;
	dev_info->min_rx_bufsize = SXE2_MIN_BUF_SIZE;
	dev_info->max_rx_pktlen = SXE2_FRAME_SIZE_MAX;
	dev_info->max_lro_pkt_size = SXE2_FRAME_SIZE_MAX * SXE2_RX_LRO_DESC_MAX_NUM;
	dev_info->max_mtu = dev_info->max_rx_pktlen - SXE2_ETH_OVERHEAD;
	dev_info->min_mtu = RTE_ETHER_MIN_MTU;
	dev_info->max_mac_addrs = SXE2_NUM_MACADDR_MAX;

	dev_info->rx_offload_capa =
		RTE_ETH_RX_OFFLOAD_KEEP_CRC |
		RTE_ETH_RX_OFFLOAD_IPV4_CKSUM |
		RTE_ETH_RX_OFFLOAD_UDP_CKSUM |
		RTE_ETH_RX_OFFLOAD_TCP_CKSUM |
		RTE_ETH_RX_OFFLOAD_SCTP_CKSUM |
		RTE_ETH_RX_OFFLOAD_OUTER_IPV4_CKSUM;
	dev_info->tx_offload_capa =
		RTE_ETH_TX_OFFLOAD_IPV4_CKSUM |
		RTE_ETH_TX_OFFLOAD_UDP_CKSUM |
		RTE_ETH_TX_OFFLOAD_TCP_CKSUM |
		RTE_ETH_TX_OFFLOAD_SCTP_CKSUM |
		RTE_ETH_TX_OFFLOAD_OUTER_IPV4_CKSUM |
		RTE_ETH_TX_OFFLOAD_OUTER_UDP_CKSUM;

	dev_info->tx_queue_offload_capa = RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;

	dev_info->default_rxconf = (struct rte_eth_rxconf) {
		.rx_thresh = {
			.pthresh = SXE2_DEFAULT_RX_PTHRESH,
			.hthresh = SXE2_DEFAULT_RX_HTHRESH,
			.wthresh = SXE2_DEFAULT_RX_WTHRESH,
		},
		.rx_free_thresh = SXE2_DEFAULT_RX_FREE_THRESH,
		.rx_drop_en = 0,
		.offloads = 0,
	};

	dev_info->default_txconf = (struct rte_eth_txconf) {
		.tx_thresh = {
			.pthresh = SXE2_DEFAULT_TX_PTHRESH,
			.hthresh = SXE2_DEFAULT_TX_HTHRESH,
			.wthresh = SXE2_DEFAULT_TX_WTHRESH,
		},
		.tx_free_thresh = SXE2_DEFAULT_TX_FREE_THRESH,
		.tx_rs_thresh = SXE2_DEFAULT_TX_RSBIT_THRESH,
		.offloads = 0,
	};

	dev_info->rx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = SXE2_MAX_RING_DESC,
		.nb_min = SXE2_MIN_RING_DESC,
		.nb_align = SXE2_ALIGN,
	};

	dev_info->tx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = SXE2_MAX_RING_DESC,
		.nb_min = SXE2_MIN_RING_DESC,
		.nb_align = SXE2_ALIGN,
		.nb_mtu_seg_max = SXE2_TX_MTU_SEG_MAX,
		.nb_seg_max = SXE2_MAX_RING_DESC,
	};

	dev_info->speed_capa = RTE_ETH_LINK_SPEED_10G | RTE_ETH_LINK_SPEED_25G |
			 RTE_ETH_LINK_SPEED_50G | RTE_ETH_LINK_SPEED_100G;

	dev_info->nb_rx_queues = dev->data->nb_rx_queues;
	dev_info->nb_tx_queues = dev->data->nb_tx_queues;

	dev_info->default_rxportconf.burst_size = SXE2_RX_MAX_BURST;
	dev_info->default_txportconf.burst_size = SXE2_TX_MAX_BURST;
	dev_info->default_rxportconf.nb_queues = 1;
	dev_info->default_txportconf.nb_queues = 1;
	dev_info->default_rxportconf.ring_size = SXE2_RING_SIZE_MIN;
	dev_info->default_txportconf.ring_size = SXE2_RING_SIZE_MIN;

	dev_info->rx_seg_capa.offset_allowed = false;

	dev_info->rx_seg_capa.offset_align_log2 = false;

	return 0;
}

static const struct eth_dev_ops sxe2_switchdev_repr_dev_ops = {
	.dev_configure              = sxe2_repr_dev_configure,

	.dev_start                  = sxe2_repr_dev_start,
	.dev_stop                   = sxe2_repr_dev_stop,

	.rx_queue_start             = sxe2_rx_queue_start,
	.rx_queue_stop              = sxe2_rx_queue_stop,
	.tx_queue_start             = sxe2_tx_queue_start,
	.tx_queue_stop              = sxe2_tx_queue_stop,
	.rx_queue_setup             = sxe2_rx_queue_setup,
	.rx_queue_release           = sxe2_rx_queue_release,
	.tx_queue_setup             = sxe2_tx_queue_setup,
	.tx_queue_release           = sxe2_tx_queue_release,

	.dev_close                  = sxe2_repr_dev_close,
	.dev_infos_get              = sxe2_repr_dev_infos_get,
	.dev_supported_ptypes_get   = sxe2_dev_supported_ptypes_get,
	.link_update                = sxe2_link_update,

	.promiscuous_enable         = sxe2_repr_promisc_enable,
	.promiscuous_disable        = sxe2_repr_promisc_disable,
	.allmulticast_enable        = sxe2_repr_allmulti_enable,
	.allmulticast_disable       = sxe2_repr_allmulti_disable,

	.eth_dev_priv_dump          = sxe2_eth_dev_priv_dump,

	.stats_get                  = sxe2_stats_info_get,
	.stats_reset                = sxe2_stats_info_reset,
	.xstats_get                 = sxe2_xstats_info_get,
	.xstats_get_names           = sxe2_xstats_names_get,
	.xstats_reset               = sxe2_stats_info_reset,
};

void sxe2_repr_all_close(struct rte_eth_dev *dev)
{
	uint16_t vf_id;
	struct rte_eth_dev *repr_eth_dev = NULL;
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);

	if (adapter->repr_ctxt.nb_repr_vf) {
		for (vf_id = 0; vf_id < adapter->repr_ctxt.nb_repr_vf; vf_id++) {
			repr_eth_dev = adapter->repr_ctxt.vf_rep_eth_dev[vf_id];
			if (!repr_eth_dev || repr_eth_dev->data->dev_started == 0)
				continue;

			(void)rte_eth_dev_stop(repr_eth_dev->data->port_id);
			(void)rte_eth_dev_close(repr_eth_dev->data->port_id);
		}
	}
}

static void sxe2_repr_adapter_init(struct rte_eth_dev *dev_repr,
				   struct sxe2_adapter *parent_adapter,
				   uint16_t repr_id)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev_repr);

	dev_repr->data->backer_port_id = parent_adapter->dev_port_id;
	dev_repr->data->representor_id = repr_id;
	dev_repr->data->dev_flags |= RTE_ETH_DEV_REPRESENTOR;

	adapter->is_dev_repr = true;
	adapter->dev_port_id = dev_repr->data->port_id;
	adapter->dev_type = parent_adapter->dev_type;
	adapter->switchdev_info.is_switchdev = parent_adapter->switchdev_info.is_switchdev;
	adapter->port_idx = parent_adapter->port_idx;
	adapter->pf_idx = parent_adapter->pf_idx;
	adapter->dev_info.pci = parent_adapter->dev_info.pci;
	adapter->dev_info.fw = parent_adapter->dev_info.fw;
}

static int32_t sxe2_repr_eth_init(struct rte_eth_dev *dev)
{
	int32_t ret = 0;

	ret = sxe2_filter_init(dev);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to initialize l2 filter, ret:%d", ret);
		goto l_end;
	}
	ret = sxe2_link_update_init(dev);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to initialize link update, ret:%d", ret);
		goto l_end;
	}

	ret = sxe2_mac_addr_init(dev);
	if (ret != 0) {
		PMD_LOG_ERR(INIT, "Failed to initialize mac address, ret:%d", ret);
		goto l_end;
	}

l_end:
	return ret;
}

static int32_t sxe2_repr_dev_pci_map_init(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *rep_adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_pci_map_context *map_ctxt = &rep_adapter->map_ctxt;
	struct sxe2_pci_map_bar_info *bar_info = NULL;
	struct sxe2_pci_map_segment_info *seg_info = NULL;
	uint16_t txq_cnt = rep_adapter->q_ctxt.qp_cnt_assign;
	uint16_t txq_base = rep_adapter->q_ctxt.base_idx_in_pf;
	uint16_t rxq_cnt = rep_adapter->q_ctxt.qp_cnt_assign;
	uint16_t rxq_base = rep_adapter->q_ctxt.base_idx_in_pf;
	uint16_t irq_cnt = rep_adapter->irq_ctxt.max_cnt_hw;
	uint16_t irq_base = rep_adapter->irq_ctxt.base_idx_in_func;
	int32_t ret = 0;

	PMD_INIT_FUNC_TRACE();

	rep_adapter->dev_info.dev_data = dev->data;

	map_ctxt->bar_cnt = 2;

	bar_info = rte_zmalloc("repr_bar_info",
			sizeof(struct sxe2_pci_map_bar_info) * map_ctxt->bar_cnt, 0);
	if (bar_info == NULL) {
		PMD_LOG_ERR(INIT, "Failed to alloc bar_info");
		ret = -ENOMEM;
		goto l_end;
	}
	bar_info[0].bar_idx = 0;
	bar_info[0].map_cnt = SXE2_PCI_MAP_RES_MAX_COUNT;
	seg_info = rte_zmalloc("repr_seg_info_bar0",
			sizeof(struct sxe2_pci_map_segment_info) * bar_info[0].map_cnt, 0);
	if (seg_info == NULL) {
		PMD_LOG_ERR(INIT, "Failed to alloc seg_info");
		ret = -ENOMEM;
		goto l_free_bar;
	}

	bar_info[0].seg_info = seg_info;

	bar_info[1].bar_idx = 4;
	bar_info[1].map_cnt = SXE2_PCI_MAP_RES_MAX_COUNT;
	seg_info = rte_zmalloc("repr_seg_info_bar4",
			sizeof(struct sxe2_pci_map_segment_info) * bar_info[1].map_cnt,
			0);
	if (!seg_info) {
		PMD_LOG_ERR(INIT, "Failed to alloc seg_info");
		ret = -ENOMEM;
		goto l_free_seg0;
	}

	bar_info[1].seg_info = seg_info;
	map_ctxt->bar_info = bar_info;

	map_ctxt->addr_info = sxe2_net_map_addr_info_repr;

	ret = sxe2_dev_pci_res_seg_map(rep_adapter, SXE2_PCI_MAP_RES_DOORBELL_TX,
			txq_cnt, txq_base);
	if (ret != 0) {
		PMD_LOG_ERR(INIT, "Failed to map txq doorbell addr, ret=%d", ret);
		goto l_free_seg1;
	}

	ret = sxe2_dev_pci_res_seg_map(rep_adapter, SXE2_PCI_MAP_RES_DOORBELL_RX_TAIL,
			rxq_cnt, rxq_base);
	if (ret != 0) {
		PMD_LOG_ERR(INIT, "Failed to map rxq tail doorbell addr, ret=%d", ret);
		goto l_free_txq;
	}

	ret = sxe2_dev_pci_res_seg_map(rep_adapter, SXE2_PCI_MAP_RES_IRQ_DYN,
			irq_cnt, irq_base);
	if (ret != 0) {
		PMD_LOG_ERR(INIT, "Failed to map irq dyn addr, ret=%d", ret);
		goto l_free_rxq_tail;
	}

	ret = sxe2_dev_pci_res_seg_map(rep_adapter, SXE2_PCI_MAP_RES_IRQ_ITR,
			irq_cnt, irq_base);
	if (ret != 0) {
		PMD_LOG_ERR(INIT, "Failed to map irq itr addr, ret=%d", ret);
		goto l_free_irq_dyn;
	}

	ret = sxe2_dev_pci_res_seg_map(rep_adapter, SXE2_PCI_MAP_RES_IRQ_MSIX,
			irq_cnt, irq_base);
	if (ret != 0) {
		PMD_LOG_ERR(INIT, "Failed to map irq msix addr, ret=%d", ret);
		goto l_free_irq_itr;
	}
	goto l_end;

l_free_irq_itr:
	(void)sxe2_dev_pci_seg_unmap(rep_adapter, SXE2_PCI_MAP_RES_IRQ_ITR);
l_free_irq_dyn:
	(void)sxe2_dev_pci_seg_unmap(rep_adapter, SXE2_PCI_MAP_RES_IRQ_DYN);
l_free_rxq_tail:
	(void)sxe2_dev_pci_seg_unmap(rep_adapter, SXE2_PCI_MAP_RES_DOORBELL_RX_TAIL);
l_free_txq:
	(void)sxe2_dev_pci_seg_unmap(rep_adapter, SXE2_PCI_MAP_RES_DOORBELL_TX);
l_free_seg1:
	if (bar_info[1].seg_info) {
		rte_free(bar_info[1].seg_info);
		bar_info[1].seg_info = NULL;
	}
l_free_seg0:
	if (bar_info[0].seg_info) {
		rte_free(bar_info[0].seg_info);
		bar_info[0].seg_info = NULL;
	}
l_free_bar:
	if (bar_info) {
		rte_free(bar_info);
		bar_info = NULL;
	}
l_end:
	return ret;
}

int32_t sxe2_repr_dev_init(struct rte_eth_dev *dev,
			   struct sxe2_adapter *parent_adapter,
			   uint16_t repr_id)
{
	int32_t ret = 0;

	PMD_INIT_FUNC_TRACE();

	sxe2_set_common_function(dev);

	sxe2_repr_adapter_init(dev, parent_adapter, repr_id);

	dev->dev_ops = &sxe2_switchdev_repr_dev_ops;

	ret = sxe2_vsi_repr_main_vsi_create(dev, parent_adapter, repr_id);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to create representor main vsi, ret=[%d]", ret);
		goto l_end;
	}

	ret = sxe2_switchdev_repr_private_data_init(dev, parent_adapter, repr_id);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to fill representor private data, ret=[%d]", ret);
		goto l_init_priv_data_err;
	}

	ret = sxe2_repr_dev_pci_map_init(dev);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to pci addr map, ret=[%d]", ret);
		goto l_init_pci_error;
	}

	ret = sxe2_switchdev_dev_info_init(dev, parent_adapter);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to get device info, ret=[%d]", ret);
		goto l_init_dev_info_err;
	}

	ret = sxe2_flow_init(dev);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to init flow, ret=%d", ret);
		goto l_init_flow_err;
	}

	ret = sxe2_repr_eth_init(dev);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to init device, status = %d", ret);
		goto l_init_eth_err;
	}

	ret = sxe2_sw_irq_ctxt_init(dev);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to initialize sw parameters, ret=[%d]", ret);
		goto l_init_sw_err;
	}

	goto l_end;

l_init_sw_err:
	sxe2_eth_uinit(dev);
l_init_eth_err:
	(void)sxe2_flow_uninit(dev);
l_init_flow_err:
l_init_dev_info_err:
	sxe2_dev_pci_map_uinit(dev);
l_init_pci_error:
	(void)sxe2_switchdev_uninit(dev);
l_init_priv_data_err:
	sxe2_vsi_repr_main_vsi_destroy(dev);
l_end:
	return ret;
}

static void sxe2_repr_dev_uinit(struct rte_eth_dev *dev)
{
	sxe2_eth_uinit(dev);
	(void)sxe2_flow_uninit(dev);
	sxe2_dev_pci_map_uinit(dev);
	(void)sxe2_switchdev_uninit(dev);
	sxe2_vsi_repr_main_vsi_destroy(dev);
}

int32_t sxe2_switchdev_repr_devs_init(struct sxe2_adapter *adapter,
				  struct rte_eth_devargs *req_eth_da)
{
	struct rte_eth_dev *eth_dev = NULL;
	int32_t ret;
	uint16_t repr_idx = 0, tmp_repr_idx = 0;
	char name[RTE_ETH_NAME_MAX_LEN];

	if (req_eth_da->nb_representor_ports == 0) {
		ret = 0;
		goto l_end;
	}

	if (req_eth_da->nb_representor_ports > adapter->repr_ctxt.nb_vf) {
		PMD_LOG_ERR(INIT, "Failed to create repr vsi, nb_representor_ports=%d, nb_vf=%d",
			    req_eth_da->nb_representor_ports, adapter->repr_ctxt.nb_vf);
		ret = -EINVAL;
		goto l_end;
	}

	ret = sxe2_other_vsi_create(adapter, req_eth_da->nb_representor_ports);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to create representor vsi, ret=%d", ret);
		goto l_release_port;
	}

	adapter->repr_ctxt.vf_rep_eth_dev = rte_zmalloc("sxe2_repr_ethdev",
			req_eth_da->nb_representor_ports * sizeof(struct rte_eth_dev *), 0);
	if (adapter->repr_ctxt.vf_rep_eth_dev == NULL) {
		PMD_LOG_ERR(INIT, "Failed to malloc representor eth dev.");
		ret = -ENOMEM;
		goto l_release_port;
	}

	for (repr_idx = 0; repr_idx < req_eth_da->nb_representor_ports; ++repr_idx) {
		snprintf(name, sizeof(name), "sxe2_representor_c%dpf%d%s%u",
			 adapter->pf_idx, adapter->pf_idx,
			 "vf",
			 req_eth_da->representor_ports[repr_idx]);

		eth_dev = rte_eth_dev_allocate(name);
		if (!eth_dev) {
			ret = -ENOMEM;
			goto l_release_port;
		}
		eth_dev->data->dev_private = rte_zmalloc_socket(name,
			sizeof(struct sxe2_adapter),
			RTE_CACHE_LINE_SIZE,
			rte_socket_id());

		if (!eth_dev->data->dev_private) {
			rte_eth_dev_release_port(eth_dev);
			ret = -ENOMEM;
			goto l_release_port;
		}

		eth_dev->device = rte_eth_devices[adapter->dev_info.dev_data->port_id].device;

		ret = sxe2_repr_dev_init(eth_dev, adapter,
					 req_eth_da->representor_ports[repr_idx]);
		if (ret) {
			PMD_LOG_ERR(INIT, "Sxe2 dev init failed, ret=%d", ret);
			rte_eth_dev_release_port(eth_dev);
			goto l_release_port;
		}

		eth_dev->intr_handle = rte_intr_instance_alloc(RTE_INTR_INSTANCE_F_SHARED);
		if (eth_dev->intr_handle == NULL) {
			PMD_LOG_ERR(INIT, "Sxe2 dev init representor intr_handle failed");
			ret = -ENOMEM;
			sxe2_repr_dev_uinit(eth_dev);
			rte_eth_dev_release_port(eth_dev);
			goto l_release_port;
		}
		adapter->repr_ctxt.vf_rep_eth_dev[repr_idx] = eth_dev;
		rte_eth_dev_probing_finish(eth_dev);
	}
	adapter->repr_ctxt.nb_repr_vf = req_eth_da->nb_representor_ports;
	goto l_end;

l_release_port:
	for (tmp_repr_idx = 0; tmp_repr_idx < repr_idx; ++tmp_repr_idx) {
		struct rte_eth_dev *rep_dev = adapter->repr_ctxt.vf_rep_eth_dev[tmp_repr_idx];
		if (rep_dev) {
			sxe2_repr_dev_uinit(rep_dev);
			if (rep_dev->intr_handle)
				rte_intr_instance_free(rep_dev->intr_handle);
			rte_eth_dev_release_port(rep_dev);
			adapter->repr_ctxt.vf_rep_eth_dev[tmp_repr_idx] = NULL;
		}
	}

	rte_free(adapter->repr_ctxt.vf_rep_eth_dev);
	adapter->repr_ctxt.vf_rep_eth_dev = NULL;

l_end:
	return ret;
}
