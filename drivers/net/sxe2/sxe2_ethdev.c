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
#include "sxe2_drv_cmd.h"
#include "sxe2_cmd_chnl.h"
#include "sxe2_common.h"
#include "sxe2_common_log.h"
#include "sxe2_host_regs.h"
#include "sxe2_ioctl_chnl_func.h"

#define SXE2_PCI_VENDOR_ID_1    0x1ff2
#define SXE2_PCI_DEVICE_ID_PF_1 0x10b1
#define SXE2_PCI_DEVICE_ID_VF_1 0x10b2

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

static int32_t sxe2_dev_configure(struct rte_eth_dev *dev)
{
	int32_t ret = 0;
	PMD_INIT_FUNC_TRACE();

	if (dev->data->dev_conf.rxmode.mq_mode  & RTE_ETH_MQ_RX_RSS_FLAG)
		dev->data->dev_conf.rxmode.offloads |= RTE_ETH_RX_OFFLOAD_RSS_HASH;

	return ret;
}

static void __rte_cold sxe2_txqs_all_stop(struct rte_eth_dev *dev __rte_unused)
{
}

static void __rte_cold sxe2_rxqs_all_stop(struct rte_eth_dev *dev __rte_unused)
{
}

static int32_t sxe2_dev_stop(struct rte_eth_dev *dev)
{
	int32_t ret = 0;
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	PMD_INIT_FUNC_TRACE();

	if (adapter->started == 0)
		goto l_end;

	sxe2_txqs_all_stop(dev);
	sxe2_rxqs_all_stop(dev);

	dev->data->dev_started = 0;
	adapter->started = 0;
l_end:
	return ret;
}

static int32_t __rte_cold sxe2_txqs_all_start(struct rte_eth_dev *dev __rte_unused)
{
	return 0;
}

static int32_t __rte_cold sxe2_rxqs_all_start(struct rte_eth_dev *dev __rte_unused)
{
	return 0;
}

static int32_t sxe2_queues_start(struct rte_eth_dev *dev)
{
	int32_t ret = 0;
	ret = sxe2_txqs_all_start(dev);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to start tx queue.");
		goto l_end;
	}

	ret = sxe2_rxqs_all_start(dev);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to start rx queue.");
		sxe2_txqs_all_stop(dev);
	}

l_end:
	return ret;
}

static int32_t sxe2_dev_start(struct rte_eth_dev *dev)
{
	int32_t ret = 0;
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	PMD_INIT_FUNC_TRACE();

	ret = sxe2_queues_init(dev);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to init queues.");
		goto l_end;
	}

	ret = sxe2_queues_start(dev);
	if (ret) {
		PMD_LOG_ERR(INIT, "enable queues failed");
		goto l_end;
	}

	dev->data->dev_started = 1;
	adapter->started = 1;
	goto l_end;

l_end:
	return ret;
}

static int32_t sxe2_dev_close(struct rte_eth_dev *dev)
{
	(void)sxe2_dev_stop(dev);

	sxe2_vsi_uninit(dev);

	return 0;
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
		RTE_ETH_RX_OFFLOAD_KEEP_CRC |
		RTE_ETH_RX_OFFLOAD_SCATTER |
		RTE_ETH_RX_OFFLOAD_IPV4_CKSUM |
		RTE_ETH_RX_OFFLOAD_UDP_CKSUM |
		RTE_ETH_RX_OFFLOAD_TCP_CKSUM |
		RTE_ETH_RX_OFFLOAD_SCTP_CKSUM |
		RTE_ETH_RX_OFFLOAD_OUTER_IPV4_CKSUM |
		RTE_ETH_RX_OFFLOAD_BUFFER_SPLIT |
		RTE_ETH_RX_OFFLOAD_TCP_LRO;

	dev_info->tx_offload_capa =
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

static const struct eth_dev_ops sxe2_eth_dev_ops = {
	.dev_configure              = sxe2_dev_configure,
	.dev_start                  = sxe2_dev_start,
	.dev_stop                   = sxe2_dev_stop,
	.dev_close                  = sxe2_dev_close,
	.dev_infos_get              = sxe2_dev_infos_get,
};

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

	sxe2_sw_vsi_ctx_hw_cap_set(adapter, &dev_caps.vsi_caps);

l_end:
	return ret;
}

static int32_t sxe2_dev_caps_get(struct sxe2_adapter *adapter)
{
	int32_t ret = -1;

	ret = sxe2_func_caps_get(adapter);
	if (ret)
		PMD_LOG_ERR(INIT, "get function caps failed, ret=%d", ret);

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

static int32_t sxe2_dev_info_init(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter =
		SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI(dev->device);
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

static int32_t sxe2_dev_init(struct rte_eth_dev *dev,
			     struct sxe2_dev_kvargs_info *kvargs __rte_unused)
{
	int32_t ret = 0;

	PMD_INIT_FUNC_TRACE();

	dev->dev_ops = &sxe2_eth_dev_ops;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		goto l_end;

	ret = sxe2_hw_init(dev);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to initialize hw, ret=[%d]", ret);
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

	goto l_end;

init_dev_info_err:
	sxe2_vsi_uninit(dev);
init_vsi_err:
l_end:
	return ret;
}

static int32_t sxe2_dev_uninit(struct rte_eth_dev *dev)
{
	int32_t ret = 0;
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		goto l_end;

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
	struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI(cdev->dev);
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

static int32_t sxe2_eth_pmd_probe_pf(struct sxe2_common_device *cdev,
		struct rte_eth_devargs *req_eth_da __rte_unused,
		uint16_t owner_id __rte_unused,
		struct sxe2_dev_kvargs_info *kvargs)
{
	struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI(cdev->dev);
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

	rte_eth_dev_probing_finish(eth_dev);
	PMD_DEV_LOG_DEBUG(adapter, INIT, "Sxe2 eth pmd probe successful!");
	goto l_end;

l_release_port:
	(void)rte_eth_dev_release_port(eth_dev);
l_end:
	return ret;
}

static int32_t sxe2_parse_eth_devargs(struct rte_device *dev,
			  struct rte_eth_devargs *eth_da)
{
	int ret = 0;

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
