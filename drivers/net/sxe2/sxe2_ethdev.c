/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#include <rte_string_fns.h>
#include <ethdev_pci.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <rte_tailq.h>
#include <rte_version.h>
#include <bus_pci_driver.h>
#include <dev_driver.h>
#include <ethdev_driver.h>
#include <rte_ethdev.h>
#include <rte_alarm.h>
#include <rte_dev_info.h>
#include <rte_pci.h>
#include <rte_mbuf_dyn.h>
#include <rte_cycles.h>
#include <rte_eal_paging.h>

#include "sxe2_ethdev.h"
#include "sxe2_irq.h"
#include "sxe2_drv_cmd.h"
#include "sxe2_cmd_chnl.h"
#include "sxe2_tx.h"
#include "sxe2_rx.h"
#include "sxe2_txrx.h"
#include "sxe2_mac.h"
#include "sxe2_common.h"
#include "sxe2_common_log.h"
#include "sxe2_mp.h"
#include "sxe2_flow.h"
#include "sxe2_stats.h"
#include "sxe2_host_regs.h"
#include "sxe2_switchdev.h"
#include "sxe2_ioctl_chnl_func.h"
#include "sxe2_ethdev_repr.h"
#include "sxe2vf_regs.h"

#define SXE2_PCI_VENDOR_ID_1    0x1ff2
#define SXE2_PCI_DEVICE_ID_PF_1 0x10b1
#define SXE2_PCI_DEVICE_ID_VF_1 0x10b

#define SXE2_PCI_VENDOR_ID_2    0x1d94
#define SXE2_PCI_DEVICE_ID_PF_2 0x1260
#define SXE2_PCI_DEVICE_ID_VF_2 0x126f

#define SXE2_PCI_DEVICE_ID_PF_3 0x10b3
#define SXE2_PCI_DEVICE_ID_VF_3 0x10b4

#define SXE2_PCI_VENDOR_ID_206F 0x206f

static const struct rte_pci_id pci_id_sxe2_tbl[] = {
	{ RTE_PCI_DEVICE(SXE2_PCI_VENDOR_ID_1, SXE2_PCI_DEVICE_ID_PF_1)},
	{ RTE_PCI_DEVICE(SXE2_PCI_VENDOR_ID_1, SXE2_PCI_DEVICE_ID_VF_1)},
	{ RTE_PCI_DEVICE(SXE2_PCI_VENDOR_ID_2, SXE2_PCI_DEVICE_ID_PF_2)},
	{ RTE_PCI_DEVICE(SXE2_PCI_VENDOR_ID_2, SXE2_PCI_DEVICE_ID_VF_2)},
	{ RTE_PCI_DEVICE(SXE2_PCI_VENDOR_ID_1, SXE2_PCI_DEVICE_ID_PF_3)},
	{ RTE_PCI_DEVICE(SXE2_PCI_VENDOR_ID_1, SXE2_PCI_DEVICE_ID_VF_3)},
	{ RTE_PCI_DEVICE(SXE2_PCI_VENDOR_ID_206F, SXE2_PCI_DEVICE_ID_PF_1)},
	{ RTE_PCI_DEVICE(SXE2_PCI_VENDOR_ID_206F, SXE2_PCI_DEVICE_ID_VF_1)},
	{ .vendor_id = 0, },
};

static struct sxe2_pci_map_addr_info sxe2_net_map_addr_info_pf[SXE2_PCI_MAP_RES_MAX_COUNT] = {
	[SXE2_PCI_MAP_RES_INVALID] = {.addr_base = 0,
				      .bar_idx = 0,
				      .reg_width = 0},
	[SXE2_PCI_MAP_RES_DOORBELL_TX] = {.addr_base = SXE2_TXQ_LEGACY_DBLL(0),
				      .bar_idx = 0,
				      .reg_width = 4},
	[SXE2_PCI_MAP_RES_DOORBELL_RX_TAIL] = {.addr_base = SXE2_RXQ_TAIL(0),
				      .bar_idx = 0,
				      .reg_width = 4},
	[SXE2_PCI_MAP_RES_IRQ_DYN] = {.addr_base = SXE2_VF_DYN_CTL(0),
				      .bar_idx = 0,
				      .reg_width = 4},
	[SXE2_PCI_MAP_RES_IRQ_ITR] = {.addr_base = SXE2_VF_INT_ITR(0, 0),
				      .bar_idx = 0,
				      .reg_width = 4},
	[SXE2_PCI_MAP_RES_IRQ_MSIX] = {.addr_base = SXE2_BAR4_MSIX_CTL(0),
				      .bar_idx = 4,
				      .reg_width = 10},
};

static struct sxe2_pci_map_addr_info sxe2_net_map_addr_info_vf[SXE2_PCI_MAP_RES_MAX_COUNT] = {
	[SXE2_PCI_MAP_RES_INVALID]  = {.addr_base = 0,
				      .bar_idx = 0,
				      .reg_width = 0},
	[SXE2_PCI_MAP_RES_DOORBELL_TX] = {.addr_base = SXE2VF_TXQ_TAIL(0),
				      .bar_idx = 0,
				      .reg_width = 4},
	[SXE2_PCI_MAP_RES_DOORBELL_RX_TAIL] = {.addr_base = SXE2VF_RXQ_TAIL(0),
				      .bar_idx = 0,
				      .reg_width = 4},
	[SXE2_PCI_MAP_RES_IRQ_DYN] = {.addr_base = SXE2VF_VF_DYN_CTL(0),
				      .bar_idx = 0,
				      .reg_width = 4},
	[SXE2_PCI_MAP_RES_IRQ_ITR] = {.addr_base = SXE2VF_VF_INT_ITR(0, 0),
				      .bar_idx = 0,
				      .reg_width = 4},
	[SXE2_PCI_MAP_RES_IRQ_MSIX] = {.addr_base = SXE2VF_BAR4_MSIX_CTL(0),
				      .bar_idx = 4,
				      .reg_width = 0x10},
};

static int32_t sxe2_dev_configure(struct rte_eth_dev *dev);
static int32_t sxe2_dev_start(struct rte_eth_dev *dev);
static int32_t sxe2_dev_stop(struct rte_eth_dev *dev);
static int32_t sxe2_dev_close(struct rte_eth_dev *dev);
static int32_t sxe2_dev_infos_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info);
static const uint32_t *sxe2_buffer_split_supported_hdr_ptypes_get(struct rte_eth_dev *dev
				__rte_unused, size_t *no_of_elements __rte_unused);
static int32_t sxe2_udp_tunnel_port_add(struct rte_eth_dev *dev,
					struct rte_eth_udp_tunnel *tunnel_udp);
static int32_t sxe2_udp_tunnel_port_del(struct rte_eth_dev *dev,
					struct rte_eth_udp_tunnel *tunnel_udp);
static int32_t sxe2_fw_version_string_get(struct rte_eth_dev *dev,
				      char *fw_version, size_t fw_size);

static const struct eth_dev_ops sxe2_eth_dev_ops = {
	.dev_configure              = sxe2_dev_configure,
	.dev_start                  = sxe2_dev_start,
	.dev_stop                   = sxe2_dev_stop,
	.dev_close                  = sxe2_dev_close,
	.dev_infos_get              = sxe2_dev_infos_get,
	.dev_supported_ptypes_get   = sxe2_dev_supported_ptypes_get,
	.link_update                = sxe2_link_update,

	.rx_queue_start             = sxe2_rx_queue_start,
	.rx_queue_stop              = sxe2_rx_queue_stop,
	.tx_queue_start             = sxe2_tx_queue_start,
	.tx_queue_stop              = sxe2_tx_queue_stop,
	.rx_queue_setup             = sxe2_rx_queue_setup,
	.rx_queue_release           = sxe2_rx_queue_release,
	.tx_queue_setup             = sxe2_tx_queue_setup,
	.tx_queue_release           = sxe2_tx_queue_release,
	.rx_queue_intr_enable       = sxe2_rx_queue_intr_enable,
	.rx_queue_intr_disable      = sxe2_rx_queue_intr_disable,

	.rxq_info_get               = sxe2_rx_queue_info_get,
	.txq_info_get               = sxe2_tx_queue_info_get,
	.rx_burst_mode_get          = sxe2_rx_burst_mode_get,
	.tx_burst_mode_get          = sxe2_tx_burst_mode_get,
	.tx_done_cleanup            = sxe2_tx_done_cleanup,

	.promiscuous_enable         = sxe2_promisc_enable,
	.promiscuous_disable        = sxe2_promisc_disable,
	.allmulticast_enable        = sxe2_allmulti_enable,
	.allmulticast_disable       = sxe2_allmulti_disable,

	.mac_addr_add               = sxe2_mac_addr_add,
	.mac_addr_remove            = sxe2_mac_addr_del,
	.mac_addr_set               = sxe2_mac_addr_set,
	.set_mc_addr_list           = sxe2_set_mc_addr_list,
	.mtu_set                    = sxe2_mtu_set,
	.buffer_split_supported_hdr_ptypes_get = sxe2_buffer_split_supported_hdr_ptypes_get,

	.vlan_filter_set            = sxe2_dev_vlan_filter_set,
	.vlan_offload_set           = sxe2_dev_vlan_offload_set,

	.reta_update                = sxe2_dev_rss_reta_update,
	.reta_query                 = sxe2_dev_rss_reta_query,
	.rss_hash_update            = sxe2_dev_rss_hash_update,
	.rss_hash_conf_get          = sxe2_dev_rss_hash_conf_get,

	.udp_tunnel_port_add        = sxe2_udp_tunnel_port_add,
	.udp_tunnel_port_del        = sxe2_udp_tunnel_port_del,

	.flow_ops_get               = sxe2_flow_ops_get,
	.tm_ops_get                 = sxe2_tm_ops_get,

	.stats_get                  = sxe2_stats_info_get,
	.stats_reset                = sxe2_stats_info_reset,
	.xstats_get                 = sxe2_xstats_info_get,
	.xstats_get_names           = sxe2_xstats_names_get,
	.xstats_reset               = sxe2_stats_info_reset,

	.queue_stats_mapping_set    = sxe2_queue_stats_mapping_set,

	.fw_version_get             = sxe2_fw_version_string_get,

	.get_monitor_addr           = sxe2_get_monitor_addr,
};

static int32_t sxe2_dev_configure(struct rte_eth_dev *dev)
{
	int32_t ret = 0;
	PMD_INIT_FUNC_TRACE();

	if (dev->data->dev_conf.rxmode.mq_mode  & RTE_ETH_MQ_RX_RSS_FLAG)
		dev->data->dev_conf.rxmode.offloads |= RTE_ETH_RX_OFFLOAD_RSS_HASH;

	ret = sxe2_vlan_default_cfg(dev);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to init vlan, ret=%d", ret);
		goto end;
	}

	ret = sxe2_rss_init(dev);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to init rss, ret=%d", ret);
		goto end;
	}

end:
	return ret;
}

static int32_t sxe2_dev_stop(struct rte_eth_dev *dev)
{
	int32_t ret = 0;
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	PMD_INIT_FUNC_TRACE();

	if (adapter->started == 0)
		goto l_end;

	sxe2_rxq_intr_disable(dev);

	sxe2_txqs_all_stop(dev);
	sxe2_rxqs_all_stop(dev);

	(void)sxe2_filter_rule_stop(dev);

	dev->data->dev_started = 0;
	adapter->started = 0;
l_end:
	return ret;
}

static int32_t sxe2_dev_start(struct rte_eth_dev *dev)
{
	int32_t ret = 0;
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	PMD_INIT_FUNC_TRACE();

	ret = sxe2_flow_init_udp_tunnel_port(dev);
	if (ret) {
		PMD_LOG_ERR(DRV, "Failed to init udp tunnel port, ret: %d.", ret);
		goto l_end;
	}

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

	ret = sxe2_filter_rule_start(dev);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to add all mc addr to fw.");
		goto l_end;
	}

	ret = sxe2_rxq_intr_enable(dev);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to enable rx queue intr");
		goto l_rxq_intr_err;
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
l_rxq_intr_err:
	(void)sxe2_filter_rule_stop(dev);
l_end:
	return ret;
}

static enum sxe2_udp_tunnel_protocol
sxe2_udp_tunnel_type_rte_to_sxe2(enum rte_eth_tunnel_type rte_type)
{
	static enum sxe2_udp_tunnel_protocol sxe2_udp_proto_map[RTE_ETH_TUNNEL_TYPE_MAX] = {
		[RTE_ETH_TUNNEL_TYPE_NONE] = SXE2_UDP_TUNNEL_MAX,
		[RTE_ETH_TUNNEL_TYPE_VXLAN] = SXE2_UDP_TUNNEL_PROTOCOL_VXLAN,
		[RTE_ETH_TUNNEL_TYPE_GENEVE] = SXE2_UDP_TUNNEL_PROTOCOL_GENEVE,
		[RTE_ETH_TUNNEL_TYPE_TEREDO] = SXE2_UDP_TUNNEL_PROTOCOL_TEREDO,
		[RTE_ETH_TUNNEL_TYPE_NVGRE] = SXE2_UDP_TUNNEL_PROTOCOL_NVGRE,
		[RTE_ETH_TUNNEL_TYPE_IP_IN_GRE] = SXE2_UDP_TUNNEL_MAX,
		[RTE_ETH_L2_TUNNEL_TYPE_E_TAG] = SXE2_UDP_TUNNEL_MAX,
		[RTE_ETH_TUNNEL_TYPE_VXLAN_GPE] = SXE2_UDP_TUNNEL_PROTOCOL_VXLAN_GPE,
		[RTE_ETH_TUNNEL_TYPE_ECPRI]  = SXE2_UDP_TUNNEL_PROTOCOL_ECPRI
	};

	if (rte_type >= RTE_ETH_TUNNEL_TYPE_MAX) {
		PMD_LOG_ERR(DRV, "Invalid rte_eth_tunnel_type %d!", rte_type);
		rte_type = RTE_ETH_TUNNEL_TYPE_NONE;
	}

	return sxe2_udp_proto_map[rte_type];
}

int32_t sxe2_udp_tunnel_port_add_common(struct sxe2_adapter *ad,
				    enum sxe2_udp_tunnel_protocol tunnel_proto,
				    uint16_t udp_port)
{
	struct sxe2_udp_tunnel_cfg *tunnel_config;
	int32_t ret = -1;

	rte_spinlock_lock(&ad->udp_tunnel_ctx.lock);

	tunnel_config = &ad->udp_tunnel_ctx.tunnel_conf[tunnel_proto];

	if (tunnel_config->dev_status == SXE2_UDP_TUNNEL_ENABLE) {
		if (udp_port == tunnel_config->dev_port &&
			tunnel_config->dev_ref_cnt < 0xFFFFU) {
			tunnel_config->dev_ref_cnt++;
			ret = 0;
			goto l_unlock_end;
		} else {
			PMD_LOG_ERR(DRV, "Adding multiple ports to the same protocol "
				    "is not supported!");
			ret = -EINVAL;
			goto l_unlock_end;
		}
	} else {
		ret = sxe2_drv_udp_tunnel_add(ad, tunnel_proto, udp_port);
		if (ret != 0)
			goto l_unlock_end;

		tunnel_config->protocol = tunnel_proto;
		tunnel_config->dev_port = udp_port;
		tunnel_config->dev_status  = SXE2_UDP_TUNNEL_ENABLE;
		tunnel_config->dev_ref_cnt++;
	}

l_unlock_end:
	rte_spinlock_unlock(&ad->udp_tunnel_ctx.lock);
	return ret;
}

int32_t sxe2_udp_tunnel_port_del_common(struct sxe2_adapter *ad,
				    enum sxe2_udp_tunnel_protocol tunnel_proto,
				    uint16_t udp_port)
{
	struct sxe2_udp_tunnel_cfg *tunnel_config;
	int32_t ret = -1;

	rte_spinlock_lock(&ad->udp_tunnel_ctx.lock);
	tunnel_config = &ad->udp_tunnel_ctx.tunnel_conf[tunnel_proto];

	if (tunnel_config->dev_status == SXE2_UDP_TUNNEL_ENABLE &&
		udp_port == tunnel_config->dev_port) {
		if (tunnel_config->dev_ref_cnt > 1) {
			tunnel_config->dev_ref_cnt--;
			ret = 0;
			goto l_unlock_end;
		} else {
			ret = sxe2_drv_udp_tunnel_del(ad, tunnel_proto, udp_port);
			if (ret != 0)
				goto l_unlock_end;

			tunnel_config->dev_status  = SXE2_UDP_TUNNEL_DISABLE;
			tunnel_config->dev_ref_cnt = 0;
		}
		goto l_unlock_end;
	}

	ret = -EINVAL;

l_unlock_end:
	rte_spinlock_unlock(&ad->udp_tunnel_ctx.lock);
	return ret;
}

static int32_t sxe2_udp_tunnel_port_clear(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *ad = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_udp_tunnel_cfg *tunnel_config;
	int32_t ret = 0;
	uint16_t tunnel_proto = 0;

	rte_spinlock_lock(&ad->udp_tunnel_ctx.lock);

	for (tunnel_proto = 0; tunnel_proto < SXE2_UDP_TUNNEL_MAX; tunnel_proto++) {
		tunnel_config = &ad->udp_tunnel_ctx.tunnel_conf[tunnel_proto];
		if (tunnel_config->dev_status == SXE2_UDP_TUNNEL_ENABLE) {
			ret = sxe2_drv_udp_tunnel_del(ad, tunnel_config->protocol,
					tunnel_config->dev_port);
			if (ret) {
				PMD_LOG_ERR(DRV, "Failed to delete udp tunnel port %d, proto %d",
					    tunnel_config->dev_port, tunnel_config->protocol);
				goto l_unlock_end;
			}

			tunnel_config->dev_status  = SXE2_UDP_TUNNEL_DISABLE;
			tunnel_config->dev_ref_cnt = 0;
		}
	}
l_unlock_end:
	rte_spinlock_unlock(&ad->udp_tunnel_ctx.lock);
	return ret;
}

static int32_t sxe2_udp_tunnel_port_add(struct rte_eth_dev *dev,
			struct rte_eth_udp_tunnel *tunnel_udp)
{
	int32_t ret = 0;
	enum sxe2_udp_tunnel_protocol tunnel_proto;
	struct sxe2_adapter *ad = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);

	if (tunnel_udp->udp_port == 0) {
		ret = -EINVAL;
		goto l_end;
	}

	tunnel_proto = sxe2_udp_tunnel_type_rte_to_sxe2(tunnel_udp->prot_type);
	if (tunnel_proto >= SXE2_UDP_TUNNEL_MAX) {
		ret = -EINVAL;
		goto l_end;
	}

	ret = sxe2_udp_tunnel_port_add_common(ad, tunnel_proto, tunnel_udp->udp_port);
	if (ret) {
		PMD_LOG_ERR(DRV, "Add tunnel port failed, ret = %d", ret);
		goto l_end;
	}

l_end:
	return ret;
}

static int32_t sxe2_udp_tunnel_port_del(struct rte_eth_dev *dev,
			struct rte_eth_udp_tunnel *tunnel_udp)
{
	int32_t ret = 0;
	enum sxe2_udp_tunnel_protocol tunnel_proto;
	struct sxe2_adapter *ad = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);

	tunnel_proto = sxe2_udp_tunnel_type_rte_to_sxe2(tunnel_udp->prot_type);
	if (tunnel_proto >= SXE2_UDP_TUNNEL_MAX) {
		ret = -EINVAL;
		goto l_end;
	}

	ret = sxe2_udp_tunnel_port_del_common(ad, tunnel_proto, tunnel_udp->udp_port);
	if (ret) {
		PMD_LOG_ERR(DRV, "Delete tunnel port failed, ret = %d", ret);
		goto l_end;
	}

l_end:
	return ret;
}

static int32_t sxe2_dev_infos_get(struct rte_eth_dev *dev,
			struct rte_eth_dev_info *dev_info)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_vsi *vsi = adapter->vsi_ctxt.main_vsi;

	dev_info->max_rx_queues = vsi->rxqs.q_cnt;
	dev_info->max_tx_queues = vsi->txqs.q_cnt;
	dev_info->min_rx_bufsize = SXE2_MIN_BUF_SIZE;
	dev_info->max_rx_pktlen = SXE2_FRAME_SIZE_MAX;
	dev_info->max_lro_pkt_size = SXE2_FRAME_SIZE_MAX * SXE2_RX_LRO_DESC_MAX_NUM;
	dev_info->max_mtu = dev_info->max_rx_pktlen - SXE2_ETH_OVERHEAD;
	dev_info->min_mtu = RTE_ETHER_MIN_MTU;

	dev_info->rx_offload_capa =
		RTE_ETH_RX_OFFLOAD_VLAN_STRIP |
		RTE_ETH_RX_OFFLOAD_KEEP_CRC |
		RTE_ETH_RX_OFFLOAD_SCATTER |
		RTE_ETH_RX_OFFLOAD_IPV4_CKSUM |
		RTE_ETH_RX_OFFLOAD_UDP_CKSUM |
		RTE_ETH_RX_OFFLOAD_TCP_CKSUM |
		RTE_ETH_RX_OFFLOAD_SCTP_CKSUM |
		RTE_ETH_RX_OFFLOAD_OUTER_IPV4_CKSUM |
		RTE_ETH_RX_OFFLOAD_BUFFER_SPLIT |
#ifndef RTE_LIBRTE_SXE2_16BYTE_RX_DESC
		RTE_ETH_RX_OFFLOAD_QINQ_STRIP |
#endif
		RTE_ETH_RX_OFFLOAD_VLAN_EXTEND |
		RTE_ETH_RX_OFFLOAD_TCP_LRO;

	dev_info->tx_offload_capa =
		RTE_ETH_TX_OFFLOAD_VLAN_INSERT |
		RTE_ETH_TX_OFFLOAD_QINQ_INSERT |
		RTE_ETH_TX_OFFLOAD_MULTI_SEGS |
		RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE |
		RTE_ETH_TX_OFFLOAD_IPV4_CKSUM |
		RTE_ETH_TX_OFFLOAD_UDP_CKSUM |
		RTE_ETH_TX_OFFLOAD_TCP_CKSUM |
		RTE_ETH_TX_OFFLOAD_SCTP_CKSUM |
		RTE_ETH_TX_OFFLOAD_OUTER_IPV4_CKSUM |
		RTE_ETH_TX_OFFLOAD_OUTER_UDP_CKSUM |
		RTE_ETH_TX_OFFLOAD_TCP_TSO |
		RTE_ETH_TX_OFFLOAD_UDP_TSO |
		RTE_ETH_TX_OFFLOAD_VXLAN_TNL_TSO |
		RTE_ETH_TX_OFFLOAD_GRE_TNL_TSO |
		RTE_ETH_TX_OFFLOAD_IPIP_TNL_TSO |
		RTE_ETH_TX_OFFLOAD_GENEVE_TNL_TSO;

	dev_info->rx_queue_offload_capa =
		RTE_ETH_RX_OFFLOAD_BUFFER_SPLIT |
		RTE_ETH_RX_OFFLOAD_KEEP_CRC |
		RTE_ETH_RX_OFFLOAD_SCATTER |
		RTE_ETH_RX_OFFLOAD_IPV4_CKSUM |
		RTE_ETH_RX_OFFLOAD_UDP_CKSUM |
		RTE_ETH_RX_OFFLOAD_TCP_CKSUM |
		RTE_ETH_RX_OFFLOAD_SCTP_CKSUM |
		RTE_ETH_RX_OFFLOAD_OUTER_IPV4_CKSUM |
		RTE_ETH_RX_OFFLOAD_TCP_LRO;
	dev_info->tx_queue_offload_capa =
		RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE |
		RTE_ETH_TX_OFFLOAD_TCP_TSO |
		RTE_ETH_TX_OFFLOAD_UDP_TSO |
		RTE_ETH_TX_OFFLOAD_MULTI_SEGS |
		RTE_ETH_TX_OFFLOAD_IPV4_CKSUM |
		RTE_ETH_TX_OFFLOAD_UDP_CKSUM |
		RTE_ETH_TX_OFFLOAD_TCP_CKSUM |
		RTE_ETH_TX_OFFLOAD_SCTP_CKSUM |
		RTE_ETH_TX_OFFLOAD_OUTER_IPV4_CKSUM |
		RTE_ETH_TX_OFFLOAD_OUTER_UDP_CKSUM |
		RTE_ETH_TX_OFFLOAD_VXLAN_TNL_TSO |
		RTE_ETH_TX_OFFLOAD_GRE_TNL_TSO |
		RTE_ETH_TX_OFFLOAD_IPIP_TNL_TSO |
		RTE_ETH_TX_OFFLOAD_GENEVE_TNL_TSO;


	if (adapter->cap_flags & SXE2_DEV_CAPS_OFFLOAD_PTP)
		dev_info->rx_offload_capa |= RTE_ETH_RX_OFFLOAD_TIMESTAMP;

	if (sxe2_ipsec_supported(adapter)) {
		dev_info->rx_offload_capa |= RTE_ETH_RX_OFFLOAD_SECURITY;
		dev_info->tx_offload_capa |= RTE_ETH_TX_OFFLOAD_SECURITY;
	}

	if (adapter->cap_flags & SXE2_DEV_CAPS_OFFLOAD_RSS) {
		dev_info->rx_offload_capa |= RTE_ETH_RX_OFFLOAD_RSS_HASH;
		dev_info->flow_type_rss_offloads  |= SXE2_RSS_HF_SUPPORT_ALL;
		dev_info->reta_size = adapter->rss_ctxt.rss_lut_size;
		dev_info->hash_key_size = adapter->rss_ctxt.rss_key_size;
		dev_info->rss_algo_capa =
			RTE_ETH_HASH_ALGO_TO_CAPA(RTE_ETH_HASH_FUNCTION_DEFAULT) |
			RTE_ETH_HASH_ALGO_TO_CAPA(RTE_ETH_HASH_FUNCTION_TOEPLITZ) |
			RTE_ETH_HASH_ALGO_TO_CAPA(RTE_ETH_HASH_FUNCTION_SYMMETRIC_TOEPLITZ) |
			RTE_ETH_HASH_ALGO_TO_CAPA(RTE_ETH_HASH_FUNCTION_SIMPLE_XOR);
	}

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

	dev_info->default_rxportconf.burst_size = SXE2_RX_MAX_BURST;
	dev_info->default_txportconf.burst_size = SXE2_TX_MAX_BURST;
	dev_info->default_rxportconf.nb_queues = 1;
	dev_info->default_txportconf.nb_queues = 1;
	dev_info->default_rxportconf.ring_size = SXE2_RING_SIZE_MIN;
	dev_info->default_txportconf.ring_size = SXE2_RING_SIZE_MIN;

	dev_info->rx_seg_capa.max_nseg = SXE2_RX_MAX_NSEG;

	dev_info->rx_seg_capa.multi_pools = true;

	dev_info->rx_seg_capa.offset_allowed = false;

	dev_info->rx_seg_capa.offset_align_log2 = false;

	return 0;
}

static const uint32_t *
sxe2_buffer_split_supported_hdr_ptypes_get(struct rte_eth_dev *dev __rte_unused,
					   size_t *no_of_elements __rte_unused)
{
	static const uint32_t ptypes[] = {
		RTE_PTYPE_L2_ETHER,
		RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN | RTE_PTYPE_L4_UDP,
		RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN | RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN | RTE_PTYPE_L4_SCTP,
		RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN | RTE_PTYPE_L4_UDP,
		RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN | RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN | RTE_PTYPE_L4_SCTP,

		RTE_PTYPE_TUNNEL_GRENAT,

		RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER,

		RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN,

		RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN | RTE_PTYPE_INNER_L4_UDP,
		RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN | RTE_PTYPE_INNER_L4_TCP,
		RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN | RTE_PTYPE_INNER_L4_SCTP,

		RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN | RTE_PTYPE_INNER_L4_UDP,
		RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN | RTE_PTYPE_INNER_L4_TCP,
		RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN | RTE_PTYPE_INNER_L4_SCTP,

		RTE_PTYPE_UNKNOWN
	};

	return ptypes;
}

struct sxe2_pci_map_bar_info *sxe2_dev_get_bar_info(struct sxe2_adapter *adapter,
						    enum sxe2_pci_map_resource res_type)
{
	struct sxe2_pci_map_context *map_ctxt = &adapter->map_ctxt;
	struct sxe2_pci_map_bar_info *bar_info = NULL;
	uint8_t bar_idx = SXE2_PCI_MAP_BAR_INVALID;
	uint8_t i;

	bar_idx = map_ctxt->addr_info[res_type].bar_idx;
	if (bar_idx == SXE2_PCI_MAP_BAR_INVALID) {
		PMD_DEV_LOG_ERR(adapter, INIT, "Invalid bar index with resource type %d", res_type);
		goto l_end;
	}

	for (i = 0; i < map_ctxt->bar_cnt; i++) {
		if (bar_idx == map_ctxt->bar_info[i].bar_idx) {
			bar_info = &map_ctxt->bar_info[i];
			break;
		}
	}

l_end:
	return bar_info;
}

void *sxe2_pci_map_addr_get(struct sxe2_adapter *adapter,
			     enum sxe2_pci_map_resource res_type,
			     uint16_t idx_in_func)
{
	struct sxe2_pci_map_context *map_ctxt = &adapter->map_ctxt;
	struct sxe2_pci_map_segment_info *seg_info = NULL;
	struct sxe2_pci_map_bar_info *bar_info = NULL;
	void *addr = NULL;
	uintptr_t calc_addr = 0;
	uint8_t reg_width = 0;
	uint8_t i = 0;

	bar_info = sxe2_dev_get_bar_info(adapter, res_type);
	if (bar_info == NULL) {
		PMD_DEV_LOG_WARN(adapter, INIT, "Failed to get bar info, res_type=[%d]",
				res_type);
		goto l_end;
	}
	seg_info = bar_info->seg_info;

	reg_width = map_ctxt->addr_info[res_type].reg_width;
	if (reg_width == 0) {
		PMD_DEV_LOG_WARN(adapter, INIT, "Invalid reg width with resource type %d",
				 res_type);
		goto l_end;
	}

	for (i = 0; i < bar_info->map_cnt; i++) {
		seg_info = &bar_info->seg_info[i];
		if (res_type == seg_info->type) {
			calc_addr = (uintptr_t)seg_info->addr;
			calc_addr += (uintptr_t)seg_info->page_inner_offset;
			calc_addr += (uintptr_t)reg_width * (uintptr_t)idx_in_func;
			addr = (void *)calc_addr;
			goto l_end;
		}
	}

l_end:
	return addr;
}

static int32_t sxe2_eth_init(struct rte_eth_dev *dev)
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

	ret = sxe2_mtu_set(dev, dev->data->mtu);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to set mtu, ret=%d", ret);
		goto l_end;
	}

	ret = sxe2_mac_addr_init(dev);
	if (ret != 0) {
		PMD_LOG_ERR(INIT, "Failed to initialize mac address, ret:%d", ret);
		goto l_end;
	}

	ret = sxe2_mac_default_cfg(dev);
	if (ret != 0) {
		PMD_LOG_ERR(INIT, "Failed to configure default mac address, ret:%d", ret);
		goto l_err;
	}

	ret = sxe2_vlan_cfg_init(dev);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to initialize vlan config, ret:%d", ret);
		goto l_err;
	}
	goto l_end;

l_err:
	sxe2_mac_addr_uinit(dev);
	(void)sxe2_filter_uinit(dev);
l_end:
	return ret;
}

void sxe2_eth_uinit(struct rte_eth_dev *dev __rte_unused)
{
	sxe2_mac_addr_uinit(dev);
	(void)sxe2_filter_uinit(dev);
}

static void sxe2_drv_dev_caps_set(struct sxe2_adapter *adapter,
			struct sxe2_drv_dev_caps_resp *dev_caps)
{
	adapter->port_idx = dev_caps->port_idx;

	adapter->cap_flags = 0;

	if (dev_caps->cap_flags & SXE2_DEV_CAPS_OFFLOAD_L2)
		adapter->cap_flags |= SXE2_DEV_CAPS_OFFLOAD_L2;

	if (dev_caps->cap_flags & SXE2_DEV_CAPS_OFFLOAD_VLAN)
		adapter->cap_flags |= SXE2_DEV_CAPS_OFFLOAD_VLAN;

	if (dev_caps->cap_flags & SXE2_DEV_CAPS_OFFLOAD_RSS)
		adapter->cap_flags |= SXE2_DEV_CAPS_OFFLOAD_RSS;

	if (dev_caps->cap_flags & SXE2_DEV_CAPS_OFFLOAD_IPSEC)
		adapter->cap_flags |= SXE2_DEV_CAPS_OFFLOAD_IPSEC;

	if (dev_caps->cap_flags & SXE2_DEV_CAPS_OFFLOAD_FNAV)
		adapter->cap_flags |= SXE2_DEV_CAPS_OFFLOAD_FNAV;

	if (dev_caps->cap_flags & SXE2_DEV_CAPS_OFFLOAD_TM)
		adapter->cap_flags |= SXE2_DEV_CAPS_OFFLOAD_TM;

	if (dev_caps->cap_flags & SXE2_DEV_CAPS_OFFLOAD_PTP)
		adapter->cap_flags |= SXE2_DEV_CAPS_OFFLOAD_PTP;

	if (dev_caps->cap_flags & SXE2_DEV_CAPS_OFFLOAD_Q_MAP)
		adapter->cap_flags |= SXE2_DEV_CAPS_OFFLOAD_Q_MAP;

	if (dev_caps->cap_flags & SXE2_DEV_CAPS_OFFLOAD_FC_STATE)
		adapter->cap_flags |= SXE2_DEV_CAPS_OFFLOAD_FC_STATE;
}

static void sxe2_sw_representor_ctx_hw_cap_set(struct sxe2_adapter *adapter,
			struct sxe2_drv_representor_caps *repr_caps)
{
	adapter->repr_ctxt.nb_vf = repr_caps->cnt_repr_vf;
	if (adapter->repr_ctxt.nb_vf > 0) {
		memcpy(adapter->repr_ctxt.repr_vf_id, repr_caps->repr_vf_id,
			adapter->repr_ctxt.nb_vf * sizeof(struct sxe2_drv_vsi_caps));
	}
}

static void sxe2_sw_sched_hw_cap_set(struct sxe2_adapter *adapter,
				     struct sxe2_txsch_caps *txsch_caps)
{
	adapter->sched_ctxt.tm_layers = txsch_caps->layer_cap;
	adapter->sched_ctxt.root_max_children = txsch_caps->tm_mid_node_num;
	adapter->sched_ctxt.prio_max = txsch_caps->prio_num;
}

static int32_t sxe2_func_caps_get(struct sxe2_adapter *adapter)
{
	int32_t ret = -1;
	struct sxe2_drv_dev_caps_resp dev_caps = {0};

	ret = sxe2_drv_dev_caps_get(adapter, &dev_caps);
	if (ret)
		goto l_end;

	adapter->dev_type = dev_caps.dev_type;

	sxe2_drv_dev_caps_set(adapter,  &dev_caps);

	sxe2_sw_queue_ctx_hw_cap_set(adapter, &dev_caps.queue_caps);

	sxe2_sw_irq_ctx_hw_cap_set(adapter, &dev_caps.msix_caps);

	sxe2_sw_rss_ctx_hw_cap_set(adapter, &dev_caps.rss_hash_caps);

	sxe2_sw_vsi_ctx_hw_cap_set(adapter, &dev_caps.vsi_caps);

	sxe2_sw_representor_ctx_hw_cap_set(adapter, &dev_caps.repr_caps);

	sxe2_sw_sched_hw_cap_set(adapter, &dev_caps.txsch_caps);

l_end:
	return ret;
}

static int32_t sxe2_switchdev_info_get(struct sxe2_adapter *adapter)
{
	int32_t ret = 0;
	struct sxe2_switchdev_info switchdev_info = {0};

	ret = sxe2_drv_switchdev_info_get(adapter, &switchdev_info);
	if (ret)
		goto l_end;
	if (switchdev_info.primary && switchdev_info.representor) {
		PMD_LOG_ERR(INIT, "device could not be both primary and representor");
		ret = -ENODEV;
		goto l_end;
	}
	adapter->switchdev_info = switchdev_info;
l_end:
	return ret;
}

static int32_t sxe2_dev_caps_get(struct sxe2_adapter *adapter)
{
	int32_t ret = -1;

	ret = sxe2_func_caps_get(adapter);
	if (ret) {
		PMD_LOG_ERR(INIT, "get function caps failed, ret=%d", ret);
		goto l_end;
	}

	ret = sxe2_switchdev_info_get(adapter);
	if (ret)
		PMD_LOG_ERR(INIT, "get switchdev info failed, ret=%d", ret);

l_end:
	return ret;
}

uint32_t sxe2_pci_map_read_reg(struct sxe2_adapter *adapter,
		enum sxe2_pci_map_resource res_type, uint16_t idx_in_func)
{
	void *reg_addr;
	uint32_t value;

	reg_addr = sxe2_pci_map_addr_get(adapter, res_type, idx_in_func);
	if (unlikely(reg_addr == NULL)) {
		PMD_DEV_LOG_ERR(adapter, DRV, "reg addr:0x%p is error.", reg_addr);
		value = SXE2_PCI_MAP_INVALID_VAL;
		goto l_ret;
	}

	value = SXE2_PCI_REG_READ(reg_addr);

l_ret:
	return value;
}

void sxe2_pci_map_write_reg(struct sxe2_adapter *adapter,
		enum sxe2_pci_map_resource res_type, uint16_t idx_in_func, uint32_t value)
{
	void *reg_addr;

	reg_addr = sxe2_pci_map_addr_get(adapter, res_type, idx_in_func);
	if (unlikely(reg_addr == NULL)) {
		PMD_DEV_LOG_ERR(adapter, DRV, "reg addr:0x%p is error.", reg_addr);
		goto l_ret;
	}

	SXE2_PCI_REG_WRITE_WC(reg_addr, value);
l_ret:
	return;
}

int32_t sxe2_dev_pci_seg_map(struct sxe2_adapter *adapter,
			     enum sxe2_pci_map_resource res_type,
			     uint64_t org_len,
			     uint64_t org_offset)
{
	struct sxe2_pci_map_bar_info *bar_info = NULL;
	struct sxe2_pci_map_segment_info *seg_info = NULL;
	void *map_addr = NULL;
	int32_t ret = 0;
	size_t page_size = 0;
	size_t aligned_len = 0;
	size_t page_inner_offset = 0;
	off_t aligned_offset = 0;
	uint8_t i = 0;

	if (org_len == 0) {
		PMD_DEV_LOG_ERR(adapter, INIT, "Invalid length, ori_len = 0");
		ret = -EFAULT;
		goto l_end;
	}

	bar_info = sxe2_dev_get_bar_info(adapter, res_type);
	if (!bar_info) {
		PMD_LOG_ERR(INIT, "Failed to get bar info, res_type=[%d]", res_type);
		ret = -EFAULT;
		goto l_end;
	}
	seg_info = bar_info->seg_info;

	page_size = rte_mem_page_size();

	aligned_offset = RTE_ALIGN_FLOOR(org_offset, page_size);
	page_inner_offset = org_offset - aligned_offset;
	aligned_len = RTE_ALIGN(page_inner_offset + org_len, page_size);

	map_addr = sxe2_drv_dev_mmap(adapter->cdev, bar_info->bar_idx,
				     aligned_len, aligned_offset);
	if (!map_addr) {
		PMD_LOG_ERR(INIT, "Failed to mmap BAR space, type=%d, len=%" PRIu64
			    ", offset=%" PRIu64 ", page_size=%zu",
			    res_type, org_len, org_offset, page_size);
		ret = -EFAULT;
		goto l_end;
	}

	for (i = 0; i < bar_info->map_cnt; i++) {
		if (seg_info[i].type != SXE2_PCI_MAP_RES_INVALID)
			continue;
		seg_info[i].type = res_type;
		seg_info[i].addr = map_addr;
		seg_info[i].page_inner_offset = page_inner_offset;
		seg_info[i].len = aligned_len;
		break;
	}
	if (i == bar_info->map_cnt) {
		PMD_LOG_ERR(INIT, "No memory to save resource, res_type=%d", res_type);
		ret = -ENOMEM;
		sxe2_drv_dev_munmap(adapter->cdev, map_addr, aligned_len);
		goto l_end;
	}

l_end:
	return ret;
}

static int32_t sxe2_hw_init(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	int32_t ret = -1;

	PMD_INIT_FUNC_TRACE();

	ret = sxe2_dev_caps_get(adapter);
	if (ret)
		PMD_LOG_ERR(INIT, "Failed to get device caps, ret=[%d]", ret);

	return ret;
}

static int32_t sxe2_sw_init(struct rte_eth_dev *dev)
{
	int32_t ret = -1;

	PMD_INIT_FUNC_TRACE();

	ret = sxe2_sw_irq_ctxt_init(dev);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to sw irq ctxt init, ret=[%d]", ret);
		goto l_end;
	}

l_end:
	return ret;
}

static void sxe2_sw_uninit(struct rte_eth_dev *dev)
{
	sxe2_sw_irq_ctxt_uninit(dev);
}

int32_t sxe2_dev_pci_res_seg_map(struct sxe2_adapter *adapter,
				 uint32_t res_type,
				 uint32_t item_cnt,
				 uint32_t item_base)
{
	struct sxe2_pci_map_addr_info *addr_info = NULL;
	int32_t ret = 0;

	addr_info = &adapter->map_ctxt.addr_info[res_type];
	if (!addr_info || addr_info->bar_idx == SXE2_PCI_MAP_BAR_INVALID) {
		PMD_DEV_LOG_ERR(adapter, INIT, "Invalid bar index with resource type %d", res_type);
		ret = -EFAULT;
		goto l_end;
	}

	ret = sxe2_dev_pci_seg_map(adapter, res_type, item_cnt * addr_info->reg_width,
			addr_info->addr_base + item_base * addr_info->reg_width);
	if (ret != 0) {
		PMD_DEV_LOG_ERR(adapter, INIT, "Failed to map resource, res_type=%d", res_type);
		goto l_end;
	}
l_end:
	return ret;
}

void sxe2_dev_pci_seg_unmap(struct sxe2_adapter *adapter, uint32_t res_type)
{
	struct sxe2_pci_map_bar_info *bar_info = NULL;
	struct sxe2_pci_map_segment_info *seg_info = NULL;
	uint32_t i = 0;

	bar_info = sxe2_dev_get_bar_info(adapter, res_type);
	if (bar_info == NULL) {
		PMD_DEV_LOG_WARN(adapter, INIT, "Failed to get bar info, res_type=[%d]", res_type);
		goto l_end;
	}
	seg_info = bar_info->seg_info;

	for (i = 0; i < bar_info->map_cnt; i++) {
		if (res_type == seg_info[i].type) {
			(void)sxe2_drv_dev_munmap(adapter->cdev, seg_info[i].addr,
						  seg_info[i].len);
			memset(&seg_info[i], 0, sizeof(struct sxe2_pci_map_segment_info));
			break;
		}
	}

l_end:
	return;
}

static int32_t sxe2_dev_info_init(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter =
		SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct rte_pci_device *pci_dev = RTE_CLASS_TO_BUS_DEVICE(dev, *pci_dev);
	struct sxe2_dev_info *dev_info = &adapter->dev_info;
	struct sxe2_drv_dev_info_resp dev_info_resp = {0};
	struct sxe2_drv_dev_fw_info_resp dev_fw_info_resp = {0};
	int32_t ret = 0;

	dev_info->pci.bus_devid = pci_dev->addr.devid;
	dev_info->pci.bus_function = pci_dev->addr.function;

	ret = sxe2_drv_dev_info_get(adapter, &dev_info_resp);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to get device info, ret=[%d]", ret);
		goto l_end;
	}
	dev_info->pci.serial_number = dev_info_resp.dsn;

	ret = sxe2_drv_dev_fw_info_get(adapter, &dev_fw_info_resp);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to get device fw info, ret=[%d]", ret);
		goto l_end;
	}
	dev_info->fw.build_id = dev_fw_info_resp.build_id;
	dev_info->fw.fix_version_id = dev_fw_info_resp.fix_version_id;
	dev_info->fw.sub_version_id = dev_fw_info_resp.sub_version_id;
	dev_info->fw.main_version_id = dev_fw_info_resp.main_version_id;

	if (rte_is_valid_assigned_ether_addr((struct rte_ether_addr *)dev_info_resp.mac_addr))
		rte_ether_addr_copy((struct rte_ether_addr *)dev_info_resp.mac_addr,
						(struct rte_ether_addr *)dev_info->mac.perm_addr);
	else
		rte_eth_random_addr(dev_info->mac.perm_addr);

l_end:
	return ret;
}

int32_t sxe2_dev_pci_map_init(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter  = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct rte_pci_device *pci_dev = RTE_CLASS_TO_BUS_DEVICE(dev, *pci_dev);
	struct sxe2_pci_map_context *map_ctxt = &adapter->map_ctxt;
	struct sxe2_pci_map_bar_info *bar_info = NULL;
	struct sxe2_pci_map_segment_info *seg_info = NULL;
	uint16_t txq_cnt = adapter->q_ctxt.qp_cnt_assign;
	uint16_t txq_base = adapter->q_ctxt.base_idx_in_pf;
	uint16_t rxq_cnt = adapter->q_ctxt.qp_cnt_assign;
	uint16_t irq_cnt = adapter->irq_ctxt.max_cnt_hw;
	uint16_t irq_base = adapter->irq_ctxt.base_idx_in_func;
	uint16_t rxq_base = adapter->q_ctxt.base_idx_in_pf;
	int32_t ret = 0;

	PMD_INIT_FUNC_TRACE();

	adapter->dev_info.dev_data = dev->data;

	if (!pci_dev->mem_resource[0].phys_addr) {
		PMD_LOG_ERR(INIT, "Physical address not scanned");
		ret = -ENXIO;
		goto l_end;
	}

	map_ctxt->bar_cnt = 2;

	bar_info = rte_zmalloc(NULL, sizeof(*bar_info) * map_ctxt->bar_cnt, 0);
	if (!bar_info) {
		PMD_LOG_ERR(INIT, "Failed to alloc bar_info");
		ret = -ENOMEM;
		goto l_end;
	}
	bar_info[0].bar_idx = 0;
	bar_info[0].map_cnt = SXE2_PCI_MAP_RES_MAX_COUNT;
	seg_info = rte_zmalloc(NULL, sizeof(*seg_info) * bar_info[0].map_cnt, 0);
	if (!seg_info) {
		PMD_LOG_ERR(INIT, "Failed to alloc seg_info");
		ret = -ENOMEM;
		goto l_free_bar;
	}

	bar_info[0].seg_info = seg_info;

	bar_info[1].bar_idx = 4;
	bar_info[1].map_cnt = SXE2_PCI_MAP_RES_MAX_COUNT;
	seg_info = rte_zmalloc(NULL, sizeof(*seg_info) * bar_info[1].map_cnt, 0);
	if (!seg_info) {
		PMD_LOG_ERR(INIT, "Failed to alloc seg_info");
		ret = -ENOMEM;
		goto l_free_seg0;
	}

	bar_info[1].seg_info = seg_info;
	map_ctxt->bar_info = bar_info;

	if (adapter->dev_type == SXE2_DEV_T_VF)
		map_ctxt->addr_info = sxe2_net_map_addr_info_vf;
	else
		map_ctxt->addr_info = sxe2_net_map_addr_info_pf;

	ret = sxe2_dev_pci_res_seg_map(adapter, SXE2_PCI_MAP_RES_DOORBELL_TX,
				       txq_cnt, txq_base);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to map txq doorbell addr, ret=%d", ret);
		goto l_free_seg1;
	}

	ret = sxe2_dev_pci_res_seg_map(adapter, SXE2_PCI_MAP_RES_DOORBELL_RX_TAIL,
				       rxq_cnt, rxq_base);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to map rxq tail doorbell addr, ret=%d", ret);
		goto l_free_txq;
	}

	ret = sxe2_dev_pci_res_seg_map(adapter, SXE2_PCI_MAP_RES_IRQ_DYN,
				       irq_cnt, irq_base);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to map irq dyn addr, ret=%d", ret);
		goto l_free_rxq_tail;
	}

	ret = sxe2_dev_pci_res_seg_map(adapter, SXE2_PCI_MAP_RES_IRQ_ITR,
				       irq_cnt, irq_base);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to map irq itr addr, ret=%d", ret);
		goto l_free_irq_dyn;
	}

	ret = sxe2_dev_pci_res_seg_map(adapter, SXE2_PCI_MAP_RES_IRQ_MSIX,
				       irq_cnt, irq_base);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to map irq msix addr, ret=%d", ret);
		goto l_free_irq_itr;
	}
	goto l_end;

l_free_irq_itr:
	(void)sxe2_dev_pci_seg_unmap(adapter, SXE2_PCI_MAP_RES_IRQ_ITR);
l_free_irq_dyn:
	(void)sxe2_dev_pci_seg_unmap(adapter, SXE2_PCI_MAP_RES_IRQ_DYN);
l_free_rxq_tail:
	(void)sxe2_dev_pci_seg_unmap(adapter, SXE2_PCI_MAP_RES_DOORBELL_RX_TAIL);
l_free_txq:
	(void)sxe2_dev_pci_seg_unmap(adapter, SXE2_PCI_MAP_RES_DOORBELL_TX);
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

void sxe2_dev_pci_map_uinit(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter  = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_pci_map_context *map_ctxt = &adapter->map_ctxt;
	struct sxe2_pci_map_bar_info *bar_info = NULL;
	uint8_t i = 0;

	PMD_INIT_FUNC_TRACE();

	(void)sxe2_dev_pci_seg_unmap(adapter, SXE2_PCI_MAP_RES_DOORBELL_RX_TAIL);
	(void)sxe2_dev_pci_seg_unmap(adapter, SXE2_PCI_MAP_RES_DOORBELL_TX);
	(void)sxe2_dev_pci_seg_unmap(adapter, SXE2_PCI_MAP_RES_IRQ_DYN);
	(void)sxe2_dev_pci_seg_unmap(adapter, SXE2_PCI_MAP_RES_IRQ_ITR);
	(void)sxe2_dev_pci_seg_unmap(adapter, SXE2_PCI_MAP_RES_IRQ_MSIX);

	if (map_ctxt != NULL && map_ctxt->bar_info != NULL) {
		for (i = 0; i < map_ctxt->bar_cnt; i++) {
			bar_info = &map_ctxt->bar_info[i];
			if (bar_info != NULL && bar_info->seg_info != NULL) {
				rte_free(bar_info->seg_info);
				bar_info->seg_info = NULL;
			}
		}
		rte_free(map_ctxt->bar_info);
		map_ctxt->bar_info = NULL;
	}

	adapter->dev_info.dev_data = NULL;
}

uint32_t sxe2_sched_mode_get(struct sxe2_adapter *adapter)
{
	uint32_t ret_mode = SXE2_SCHED_MODE_INVALID;
	struct sxe2_tm_context *tm_ctxt = &adapter->tm_ctxt;

	if (adapter->devargs.no_sched_mode)
		ret_mode = SXE2_SCHED_MODE_NO_SCHED;
	else if (tm_ctxt->committed)
		ret_mode = SXE2_SCHED_MODE_TM;
	else
		ret_mode = SXE2_SCHED_MODE_DEFAULT;

	return ret_mode;
}

static int32_t sxe2_sched_init(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	int32_t ret = 0;

	adapter->sched_ctxt.adj_lvl = adapter->devargs.sched_layer_mode;

	if (adapter->devargs.no_sched_mode) {
		PMD_LOG_DEBUG(DRV, "TM feature will be disabled in high-performance mode.");
		adapter->cap_flags &= ~(SXE2_DEV_CAPS_OFFLOAD_TM);
	} else {
		ret = sxe2_tm_init(dev);
		if (ret)
			goto l_end;

		ret = sxe2_drv_root_tree_alloc(dev);
		if (ret)
			goto l_end;
	}

l_end:
	return ret;
}

static int32_t sxe2_sched_uinit(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	int32_t ret = 0;

	if (!adapter->devargs.no_sched_mode) {
		ret = sxe2_tm_uninit(dev);
		if (ret) {
			PMD_LOG_ERR(INIT, "Failed to uninit tm, ret=%d", ret);
			goto l_end;
		}

		ret = sxe2_drv_root_tree_release(dev);
		if (ret) {
			PMD_LOG_ERR(INIT, "Failed to release root tree, ret=%d", ret);
			goto l_end;
		}
	}

l_end:
	return ret;
}

static int32_t sxe2_dev_init(struct rte_eth_dev *dev,
			     __rte_unused struct sxe2_dev_kvargs_info *kvargs)
{
	int32_t ret = 0;

	PMD_INIT_FUNC_TRACE();

	sxe2_set_common_function(dev);

	dev->dev_ops = &sxe2_eth_dev_ops;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		sxe2_rx_mode_func_set(dev);
		sxe2_tx_mode_func_set(dev);
		ret = sxe2_mp_init(dev);
		if (ret != 0)
			PMD_LOG_ERR(INIT, "Failed to mp init (secondary), ret=%d", ret);
		goto l_end;
	}

	ret = sxe2_hw_init(dev);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to initialize hw, ret=[%d]", ret);
		goto l_end;
	}

	ret = sxe2_dev_pci_map_init(dev);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to pci addr map, ret=[%d]", ret);
		goto l_end;
	}

	ret = sxe2_vsi_init(dev);
	if (ret) {
		PMD_LOG_ERR(INIT, "create main vsi failed, ret=%d", ret);
		goto init_vsi_err;
	}

	ret = sxe2_dev_info_init(dev);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to get device info, ret=[%d]", ret);
		goto init_dev_info_err;
	}

	ret = sxe2_switchdev_init(dev);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to initialize switchdev mode, ret=[%d]", ret);
		goto init_switchdev_err;
	}

	ret = sxe2_sw_init(dev);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to initialize sw parameters, ret=[%d]", ret);
		goto init_sw_err;
	}

	ret = sxe2_intr_init(dev);
	if (ret != 0) {
		PMD_LOG_ERR(INIT, "Failed to initialize interrupt, ret:%d", ret);
		goto init_irq_err;
	}

	ret = sxe2_eth_init(dev);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to initialize eth parameters, ret=%d", ret);
		goto init_eth_err;
	}

	ret = sxe2_security_init(dev);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to initialize security, ret=%d", ret);
		goto init_security_err;
	}

	ret = sxe2_rss_disable(dev);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to disable rss, ret=%d", ret);
		goto init_rss_err;
	}

	ret = sxe2_flow_init(dev);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to init flow, ret=%d", ret);
		goto init_flow_err;
	}

	ret = sxe2_sched_init(dev);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to init sched, ret=%d", ret);
		goto init_sched_err;
	}

	ret = sxe2_stats_init(dev);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to stats init, ret=%d", ret);
		goto init_xstats_err;
	}

	ret = sxe2_mp_init(dev);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to mp init, ret=%d", ret);
		goto init_xstats_err;
	}

	goto l_end;

init_xstats_err:
	(void)sxe2_sched_uinit(dev);
init_sched_err:
	(void)sxe2_flow_uninit(dev);
init_flow_err:
init_rss_err:
	sxe2_security_uinit(dev);
init_security_err:
	sxe2_eth_uinit(dev);
init_eth_err:
	sxe2_intr_uninit(dev);
init_irq_err:
	sxe2_sw_uninit(dev);
init_sw_err:
	(void)sxe2_switchdev_uninit(dev);
init_switchdev_err:
init_dev_info_err:
	sxe2_vsi_uninit(dev);
init_vsi_err:
	sxe2_dev_pci_map_uinit(dev);
l_end:
	return ret;
}

static int32_t sxe2_dev_close(struct rte_eth_dev *dev)
{
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		sxe2_mp_uninit(dev);
		goto l_end;
	}
	sxe2_repr_all_close(dev);
	(void)sxe2_dev_stop(dev);
	(void)sxe2_queues_release(dev);
	sxe2_mp_uninit(dev);
	(void)sxe2_sched_uinit(dev);
	(void)sxe2_rss_disable(dev);
	(void)sxe2_flow_uninit(dev);
	(void)sxe2_udp_tunnel_port_clear(dev);
	sxe2_vsi_uninit(dev);
	sxe2_security_uinit(dev);
	sxe2_intr_uninit(dev);
	(void)sxe2_switchdev_uninit(dev);
	sxe2_sw_uninit(dev);
	(void)sxe2_switchdev_uninit(dev);
	sxe2_dev_pci_map_uinit(dev);
	sxe2_eth_uinit(dev);
	sxe2_dev_pci_map_uinit(dev);
	sxe2_free_repr_info(dev);

l_end:
	return 0;
}

static int32_t sxe2_dev_uninit(struct rte_eth_dev *dev)
{
	int32_t ret = 0;
	int32_t i = 0;
	struct sxe2_adapter *adapter = NULL;
	struct rte_eth_dev *rep_dev = NULL;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		goto l_end;

	adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	for (i = 0; i < adapter->repr_ctxt.nb_repr_vf; i++) {
		rep_dev = adapter->repr_ctxt.vf_rep_eth_dev[i];
		if (rep_dev) {
			ret = rep_dev->dev_ops->dev_close(rep_dev);
			if (ret)
				goto l_end;
			if (rep_dev->intr_handle)
				rte_intr_instance_free(rep_dev->intr_handle);
			ret = rte_eth_dev_release_port(rep_dev);
			if (ret)
				goto l_end;
			adapter->repr_ctxt.vf_rep_eth_dev[i] = NULL;
		}
	}

	ret = sxe2_dev_close(dev);
	if (ret) {
		PMD_LOG_ERR(INIT, "Sxe2 dev close failed, ret=%d", ret);
		goto l_end;
	}

l_end:
	return ret;
}

static int32_t sxe2_eth_pmd_remove(struct sxe2_common_device *cdev)
{
	struct rte_eth_dev *eth_dev;
	struct rte_pci_device *pci_dev = RTE_BUS_DEVICE(cdev->dev, *pci_dev);
	int32_t ret = 0;

	eth_dev = rte_eth_dev_allocated(pci_dev->device.name);
	if (!eth_dev) {
		PMD_LOG_INFO(INIT, "Sxe2 dev allocated failed");
		goto l_end;
	}

	ret = sxe2_dev_uninit(eth_dev);
	if (ret) {
		PMD_LOG_ERR(INIT, "Sxe2 dev uninit failed, ret=%d", ret);
		goto l_end;
	}
	(void)rte_eth_dev_release_port(eth_dev);

l_end:
	return ret;
}

static int32_t sxe2_fw_version_string_get(struct rte_eth_dev *dev, char *fw_version, size_t fw_size)
{
	struct sxe2_adapter *adapter =
		SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_fw_info *fw_info = &adapter->dev_info.fw;
	int32_t ret_len;
	int32_t ret;

	ret_len = snprintf(fw_version, fw_size,
			   "%u.%u.%u.%u",
			   fw_info->main_version_id,
			   fw_info->sub_version_id,
			   fw_info->fix_version_id,
			   fw_info->build_id);

	if (ret_len < 0) {
		ret = -EINVAL;
		goto out;
	}

	ret_len += 1;
	if (fw_size < (size_t)ret_len)
		ret = -EINVAL;
	else
		ret = 0;

out:
	return ret;
}

static uint16_t sxe2_switchdev_repr_id_encode_get(struct sxe2_switchdev_info *switchdev_info)
{
	enum rte_eth_representor_type type;
	uint16_t repr = switchdev_info->vf_num;
	uint32_t pf = switchdev_info->pf_num;

	switch (switchdev_info->port_name_type) {
	case SXE2_PHYS_PORT_NAME_TYPE_UPLINK:
		if (!switchdev_info->representor)
			return UINT16_MAX;
		type = RTE_ETH_REPRESENTOR_PF;
		pf = switchdev_info->mpesw_owner;
		break;
	case SXE2_PHYS_PORT_NAME_TYPE_PFVF:
	default:
		type = RTE_ETH_REPRESENTOR_VF;
		break;
	}

	return SXE2_REPRESENTOR_ID(pf, type, repr);
}

static bool sxe2_switchdev_repr_match(struct sxe2_adapter *adapter,
				   struct rte_eth_devargs *req_eth_da)
{
	uint32_t port_idx = 0;
	uint32_t repr_idx;
	uint16_t kernel_repr_id = sxe2_switchdev_repr_id_encode_get(&adapter->switchdev_info);
	uint16_t repr_id;

	switch (req_eth_da->type) {
	case RTE_ETH_REPRESENTOR_PF:
		break;
	case RTE_ETH_REPRESENTOR_VF:
		if (adapter->switchdev_info.port_name_type !=
		SXE2_PHYS_PORT_NAME_TYPE_PFVF) {
			rte_errno = EBUSY;
			return false;
		}
		break;
	case RTE_ETH_REPRESENTOR_NONE:
		rte_errno = EBUSY;
		return false;
	default:
		rte_errno = ENOTSUP;
		return false;
	}

	for (repr_idx = 0; repr_idx < req_eth_da->nb_representor_ports; ++repr_idx) {
		repr_id = SXE2_REPRESENTOR_ID(req_eth_da->ports[port_idx],
					      req_eth_da->type,
					      req_eth_da->representor_ports[repr_idx]);
		if (repr_id == kernel_repr_id)
			return true;
	}
	rte_errno = EBUSY;
	return false;
}

static int32_t sxe2_eth_pmd_probe_pf(struct sxe2_common_device *cdev,
		struct rte_eth_devargs *req_eth_da __rte_unused,
		uint16_t owner_id __rte_unused,
		struct sxe2_dev_kvargs_info *kvargs)
{
	struct rte_pci_device *pci_dev = RTE_BUS_DEVICE(cdev->dev, *pci_dev);
	struct rte_eth_dev *eth_dev = NULL;
	struct sxe2_adapter *adapter = NULL;
	int32_t ret = 0;

	if (!cdev) {
		ret = -EINVAL;
		goto l_end;
	}

	eth_dev = rte_eth_dev_pci_allocate(pci_dev, sizeof(struct sxe2_adapter));
	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		if (eth_dev == NULL) {
			PMD_LOG_ERR(INIT, "Can not allocate ethdev");
			ret = -ENOMEM;
			goto l_end;
		}
	} else {
		if (!eth_dev) {
			PMD_LOG_DEBUG(INIT, "Can not attach secondary ethdev");
			ret = -EINVAL;
			goto l_end;
		}
	}

	adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(eth_dev);
	adapter->dev_port_id = eth_dev->data->port_id;
	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		adapter->cdev = cdev;

	ret = sxe2_dev_init(eth_dev, kvargs);
	if (ret != 0) {
		PMD_DEV_LOG_ERR(adapter, INIT, "Sxe2 dev init failed, ret=%d", ret);
		goto l_release_port;
	}

	if (req_eth_da->nb_representor_ports > 0) {
		if (!adapter->switchdev_info.is_switchdev) {
			PMD_DEV_LOG_ERR(adapter, INIT, "Representor requested but Switchdev not enabled");
			ret = -ENOTSUP;
			goto l_dev_uinit;
		}

		if (!sxe2_switchdev_repr_match(adapter, req_eth_da)) {
			PMD_DEV_LOG_ERR(adapter, INIT, "Representor parameters mismatch");
			ret = -ENOTSUP;
			goto l_dev_uinit;
		}

		ret = sxe2_switchdev_repr_devs_init(adapter, req_eth_da);
		if (ret) {
			PMD_DEV_LOG_ERR(adapter, INIT, "Failed to init representor, ret=%d", ret);
			goto l_dev_uinit;
		}
	} else {
		PMD_DEV_LOG_DEBUG(adapter, INIT, "No representors requested, skipping.");
	}

	rte_eth_dev_probing_finish(eth_dev);
	PMD_DEV_LOG_DEBUG(adapter, INIT, "Sxe2 eth pmd probe successful!");
	goto l_end;

l_dev_uinit:
	(void)sxe2_dev_uninit(eth_dev);
l_release_port:
	(void)rte_eth_dev_release_port(eth_dev);
l_end:
	return ret;
}

static int32_t sxe2_parse_eth_devargs(struct rte_device *dev,
			  struct rte_eth_devargs *eth_da)
{
	int32_t ret = 0;

	if (dev->devargs == NULL)
		return 0;

	memset(eth_da, 0, sizeof(*eth_da));

	if (dev->devargs->cls_str) {
		ret = rte_eth_devargs_parse(dev->devargs->cls_str,
					    eth_da,
					    1);
		if (ret) {
			PMD_LOG_ERR(INIT, "Failed to parse device arguments: %s",
				dev->devargs->cls_str);
			return -rte_errno;
		}
	}

	if (eth_da->type == RTE_ETH_REPRESENTOR_NONE && dev->devargs->args) {
		ret = rte_eth_devargs_parse(dev->devargs->args,
					    eth_da,
					    1);
		if (ret) {
			PMD_LOG_ERR(INIT, "Failed to parse device arguments: %s",
				dev->devargs->args);
			return -rte_errno;
		}
	}

	return 0;
}

static int32_t sxe2_eth_pmd_probe(struct sxe2_common_device *cdev,
				  struct sxe2_dev_kvargs_info *kvargs)
{
	struct rte_eth_devargs eth_da = { .nb_ports = 0 };
	int32_t ret = 0;

	ret = sxe2_parse_eth_devargs(cdev->dev, &eth_da);
	if (ret != 0) {
		ret = -EINVAL;
		goto l_end;
	}

	ret = sxe2_eth_pmd_probe_pf(cdev, &eth_da, 0, kvargs);

l_end:
	return ret;
}

static struct sxe2_class_driver sxe2_eth_pmd = {
	.drv_class = SXE2_CLASS_TYPE_ETH,
	.name = "SXE2_ETH_PMD_DRIVER_NAME",
	.probe = sxe2_eth_pmd_probe,
	.remove = sxe2_eth_pmd_remove,
	.id_table = pci_id_sxe2_tbl,
	.intr_lsc = 1,
	.intr_rmv = 1,
};

bool sxe2_ethdev_check(struct rte_eth_dev *dev)
{
	return !strcmp(dev->device->driver->name, "sxe2_pci");
}

RTE_INIT(rte_sxe2_pmd_init)
{
	sxe2_common_init();
	sxe2_class_driver_register(&sxe2_eth_pmd);
}

RTE_PMD_EXPORT_NAME(net_sxe2);
RTE_PMD_REGISTER_PCI_TABLE(net_sxe2, pci_id_sxe2_tbl);
RTE_PMD_REGISTER_KMOD_DEP(net_sxe2, "* sxe2");

RTE_LOG_REGISTER_SUFFIX(sxe2_log_init, init, NOTICE);
RTE_LOG_REGISTER_SUFFIX(sxe2_log_driver, driver, NOTICE);
RTE_LOG_REGISTER_SUFFIX(sxe2_log_rx, rx, NOTICE);
RTE_LOG_REGISTER_SUFFIX(sxe2_log_tx, tx, NOTICE);
RTE_LOG_REGISTER_SUFFIX(sxe2_log_hw, hw, NOTICE);
