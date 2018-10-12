/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Aquantia Corporation
 */

#include <rte_ethdev_pci.h>

#include "atl_ethdev.h"
#include "atl_common.h"
#include "atl_hw_regs.h"
#include "atl_logs.h"
#include "hw_atl/hw_atl_llh.h"
#include "hw_atl/hw_atl_b0.h"
#include "hw_atl/hw_atl_b0_internal.h"

static int eth_atl_dev_init(struct rte_eth_dev *eth_dev);
static int eth_atl_dev_uninit(struct rte_eth_dev *eth_dev);

static int  atl_dev_configure(struct rte_eth_dev *dev);
static int  atl_dev_start(struct rte_eth_dev *dev);
static void atl_dev_stop(struct rte_eth_dev *dev);
static void atl_dev_close(struct rte_eth_dev *dev);
static int  atl_dev_reset(struct rte_eth_dev *dev);

static int atl_fw_version_get(struct rte_eth_dev *dev, char *fw_version,
			      size_t fw_size);

static void atl_dev_info_get(struct rte_eth_dev *dev,
			       struct rte_eth_dev_info *dev_info);

static const uint32_t *atl_dev_supported_ptypes_get(struct rte_eth_dev *dev);

static int eth_atl_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	struct rte_pci_device *pci_dev);
static int eth_atl_pci_remove(struct rte_pci_device *pci_dev);

static void atl_dev_info_get(struct rte_eth_dev *dev,
				struct rte_eth_dev_info *dev_info);

int atl_logtype_init;
int atl_logtype_driver;

/*
 * The set of PCI devices this driver supports
 */
static const struct rte_pci_id pci_id_atl_map[] = {
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_AQUANTIA, AQ_DEVICE_ID_0001) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_AQUANTIA, AQ_DEVICE_ID_D100) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_AQUANTIA, AQ_DEVICE_ID_D107) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_AQUANTIA, AQ_DEVICE_ID_D108) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_AQUANTIA, AQ_DEVICE_ID_D109) },

	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_AQUANTIA, AQ_DEVICE_ID_AQC100) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_AQUANTIA, AQ_DEVICE_ID_AQC107) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_AQUANTIA, AQ_DEVICE_ID_AQC108) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_AQUANTIA, AQ_DEVICE_ID_AQC109) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_AQUANTIA, AQ_DEVICE_ID_AQC111) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_AQUANTIA, AQ_DEVICE_ID_AQC112) },

	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_AQUANTIA, AQ_DEVICE_ID_AQC100S) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_AQUANTIA, AQ_DEVICE_ID_AQC107S) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_AQUANTIA, AQ_DEVICE_ID_AQC108S) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_AQUANTIA, AQ_DEVICE_ID_AQC109S) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_AQUANTIA, AQ_DEVICE_ID_AQC111S) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_AQUANTIA, AQ_DEVICE_ID_AQC112S) },

	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_AQUANTIA, AQ_DEVICE_ID_AQC111E) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_AQUANTIA, AQ_DEVICE_ID_AQC112E) },
	{ .vendor_id = 0, /* sentinel */ },
};

static struct rte_pci_driver rte_atl_pmd = {
	.id_table = pci_id_atl_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING |
		     RTE_PCI_DRV_IOVA_AS_VA,
	.probe = eth_atl_pci_probe,
	.remove = eth_atl_pci_remove,
};

#define ATL_RX_OFFLOADS (DEV_RX_OFFLOAD_VLAN_STRIP \
			| DEV_RX_OFFLOAD_IPV4_CKSUM \
			| DEV_RX_OFFLOAD_UDP_CKSUM \
			| DEV_RX_OFFLOAD_TCP_CKSUM \
			| DEV_RX_OFFLOAD_JUMBO_FRAME)

#define ATL_TX_OFFLOADS (DEV_TX_OFFLOAD_VLAN_INSERT \
			| DEV_TX_OFFLOAD_IPV4_CKSUM \
			| DEV_TX_OFFLOAD_UDP_CKSUM \
			| DEV_TX_OFFLOAD_TCP_CKSUM \
			| DEV_TX_OFFLOAD_TCP_TSO \
			| DEV_TX_OFFLOAD_MULTI_SEGS)

static const struct rte_eth_desc_lim rx_desc_lim = {
	.nb_max = ATL_MAX_RING_DESC,
	.nb_min = ATL_MIN_RING_DESC,
	.nb_align = ATL_RXD_ALIGN,
};

static const struct rte_eth_desc_lim tx_desc_lim = {
	.nb_max = ATL_MAX_RING_DESC,
	.nb_min = ATL_MIN_RING_DESC,
	.nb_align = ATL_TXD_ALIGN,
	.nb_seg_max = ATL_TX_MAX_SEG,
	.nb_mtu_seg_max = ATL_TX_MAX_SEG,
};

static const struct eth_dev_ops atl_eth_dev_ops = {
	.dev_configure	      = atl_dev_configure,
	.dev_start	      = atl_dev_start,
	.dev_stop	      = atl_dev_stop,
	.dev_close	      = atl_dev_close,
	.dev_reset	      = atl_dev_reset,

	.fw_version_get       = atl_fw_version_get,
	.dev_infos_get	      = atl_dev_info_get,
	.dev_supported_ptypes_get = atl_dev_supported_ptypes_get,

	/* Queue Control */
	.rx_queue_start	      = atl_rx_queue_start,
	.rx_queue_stop	      = atl_rx_queue_stop,
	.rx_queue_setup       = atl_rx_queue_setup,
	.rx_queue_release     = atl_rx_queue_release,

	.tx_queue_start	      = atl_tx_queue_start,
	.tx_queue_stop	      = atl_tx_queue_stop,
	.tx_queue_setup       = atl_tx_queue_setup,
	.tx_queue_release     = atl_tx_queue_release,
};

static inline int32_t
atl_reset_hw(struct aq_hw_s *hw)
{
	return hw_atl_b0_hw_reset(hw);
}

static int
eth_atl_dev_init(struct rte_eth_dev *eth_dev)
{
	struct atl_adapter *adapter =
		(struct atl_adapter *)eth_dev->data->dev_private;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	struct aq_hw_s *hw = ATL_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	int err = 0;

	PMD_INIT_FUNC_TRACE();

	eth_dev->dev_ops = &atl_eth_dev_ops;
	eth_dev->rx_pkt_burst = &atl_recv_pkts;
	eth_dev->tx_pkt_burst = &atl_xmit_pkts;
	eth_dev->tx_pkt_prepare = &atl_prep_pkts;

	/* For secondary processes, the primary process has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	/* Vendor and Device ID need to be set before init of shared code */
	hw->device_id = pci_dev->id.device_id;
	hw->vendor_id = pci_dev->id.vendor_id;
	hw->mmio = (void *)pci_dev->mem_resource[0].addr;

	/* Hardware configuration - hardcode */
	adapter->hw_cfg.is_lro = false;
	adapter->hw_cfg.wol = false;

	hw->aq_nic_cfg = &adapter->hw_cfg;

	/* Allocate memory for storing MAC addresses */
	eth_dev->data->mac_addrs = rte_zmalloc("atlantic", ETHER_ADDR_LEN, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR, "MAC Malloc failed");
		return -ENOMEM;
	}

	err = hw_atl_utils_initfw(hw, &hw->aq_fw_ops);
	if (err)
		return err;

	/* Copy the permanent MAC address */
	if (hw->aq_fw_ops->get_mac_permanent(hw,
			eth_dev->data->mac_addrs->addr_bytes) != 0)
		return -EINVAL;

	return err;
}

static int
eth_atl_dev_uninit(struct rte_eth_dev *eth_dev)
{
	struct aq_hw_s *hw;

	PMD_INIT_FUNC_TRACE();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -EPERM;

	hw = ATL_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);

	if (hw->adapter_stopped == 0)
		atl_dev_close(eth_dev);

	eth_dev->dev_ops = NULL;
	eth_dev->rx_pkt_burst = NULL;
	eth_dev->tx_pkt_burst = NULL;

	rte_free(eth_dev->data->mac_addrs);
	eth_dev->data->mac_addrs = NULL;

	return 0;
}

static int
eth_atl_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
		sizeof(struct atl_adapter), eth_atl_dev_init);
}

static int
eth_atl_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, eth_atl_dev_uninit);
}

static int
atl_dev_configure(struct rte_eth_dev *dev __rte_unused)
{
	return 0;
}

/*
 * Configure device link speed and setup link.
 * It returns 0 on success.
 */
static int
atl_dev_start(struct rte_eth_dev *dev)
{
	struct aq_hw_s *hw = ATL_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int status;
	int err;

	PMD_INIT_FUNC_TRACE();

	/* set adapter started */
	hw->adapter_stopped = 0;

	/* reinitialize adapter
	 * this calls reset and start
	 */
	status = atl_reset_hw(hw);
	if (status != 0)
		return -EIO;

	err = hw_atl_b0_hw_init(hw, dev->data->mac_addrs->addr_bytes);

	hw_atl_b0_hw_start(hw);
	/* initialize transmission unit */
	atl_tx_init(dev);

	/* This can fail when allocating mbufs for descriptor rings */
	err = atl_rx_init(dev);
	if (err) {
		PMD_INIT_LOG(ERR, "Unable to initialize RX hardware");
		goto error;
	}

	PMD_INIT_LOG(DEBUG, "FW version: %u.%u.%u",
		hw->fw_ver_actual >> 24,
		(hw->fw_ver_actual >> 16) & 0xFF,
		hw->fw_ver_actual & 0xFFFF);
	PMD_INIT_LOG(DEBUG, "Driver version: %s", ATL_PMD_DRIVER_VERSION);

	err = atl_start_queues(dev);
	if (err < 0) {
		PMD_INIT_LOG(ERR, "Unable to start rxtx queues");
		goto error;
	}

	return 0;

error:
	atl_stop_queues(dev);
	return -EIO;
}

/*
 * Stop device: disable rx and tx functions to allow for reconfiguring.
 */
static void
atl_dev_stop(struct rte_eth_dev *dev)
{
	struct aq_hw_s *hw =
		ATL_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/* reset the NIC */
	atl_reset_hw(hw);
	hw->adapter_stopped = 1;

	atl_stop_queues(dev);

	/* Clear stored conf */
	dev->data->scattered_rx = 0;
	dev->data->lro = 0;
}

/*
 * Reset and stop device.
 */
static void
atl_dev_close(struct rte_eth_dev *dev)
{
	PMD_INIT_FUNC_TRACE();

	atl_dev_stop(dev);

	atl_free_queues(dev);
}

static int
atl_dev_reset(struct rte_eth_dev *dev)
{
	int ret;

	ret = eth_atl_dev_uninit(dev);
	if (ret)
		return ret;

	ret = eth_atl_dev_init(dev);

	return ret;
}

static int
atl_fw_version_get(struct rte_eth_dev *dev, char *fw_version, size_t fw_size)
{
	struct aq_hw_s *hw = ATL_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t fw_ver = 0;
	unsigned int ret = 0;

	ret = hw_atl_utils_get_fw_version(hw, &fw_ver);
	if (ret)
		return -EIO;

	ret = snprintf(fw_version, fw_size, "%u.%u.%u", fw_ver >> 24,
		       (fw_ver >> 16) & 0xFFU, fw_ver & 0xFFFFU);

	ret += 1; /* add string null-terminator */

	if (fw_size < ret)
		return ret;

	return 0;
}

static void
atl_dev_info_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);

	dev_info->max_rx_queues = AQ_HW_MAX_RX_QUEUES;
	dev_info->max_tx_queues = AQ_HW_MAX_TX_QUEUES;

	dev_info->min_rx_bufsize = 1024;
	dev_info->max_rx_pktlen = HW_ATL_B0_MTU_JUMBO;
	dev_info->max_mac_addrs = HW_ATL_B0_MAC_MAX;
	dev_info->max_vfs = pci_dev->max_vfs;

	dev_info->max_hash_mac_addrs = 0;
	dev_info->max_vmdq_pools = 0;
	dev_info->vmdq_queue_num = 0;

	dev_info->rx_offload_capa = ATL_RX_OFFLOADS;

	dev_info->tx_offload_capa = ATL_TX_OFFLOADS;


	dev_info->default_rxconf = (struct rte_eth_rxconf) {
		.rx_free_thresh = ATL_DEFAULT_RX_FREE_THRESH,
	};

	dev_info->default_txconf = (struct rte_eth_txconf) {
		.tx_free_thresh = ATL_DEFAULT_TX_FREE_THRESH,
	};

	dev_info->rx_desc_lim = rx_desc_lim;
	dev_info->tx_desc_lim = tx_desc_lim;
}

static const uint32_t *
atl_dev_supported_ptypes_get(struct rte_eth_dev *dev)
{
	static const uint32_t ptypes[] = {
		RTE_PTYPE_L2_ETHER,
		RTE_PTYPE_L2_ETHER_ARP,
		RTE_PTYPE_L2_ETHER_VLAN,
		RTE_PTYPE_L3_IPV4,
		RTE_PTYPE_L3_IPV6,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_L4_SCTP,
		RTE_PTYPE_L4_ICMP,
		RTE_PTYPE_UNKNOWN
	};

	if (dev->rx_pkt_burst == atl_recv_pkts)
		return ptypes;

	return NULL;
}

RTE_PMD_REGISTER_PCI(net_atlantic, rte_atl_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_atlantic, pci_id_atl_map);
RTE_PMD_REGISTER_KMOD_DEP(net_atlantic, "* igb_uio | uio_pci_generic");

RTE_INIT(atl_init_log)
{
	atl_logtype_init = rte_log_register("pmd.net.atlantic.init");
	if (atl_logtype_init >= 0)
		rte_log_set_level(atl_logtype_init, RTE_LOG_NOTICE);
	atl_logtype_driver = rte_log_register("pmd.net.atlantic.driver");
	if (atl_logtype_driver >= 0)
		rte_log_set_level(atl_logtype_driver, RTE_LOG_NOTICE);
}

