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
static int  atl_dev_set_link_up(struct rte_eth_dev *dev);
static int  atl_dev_set_link_down(struct rte_eth_dev *dev);
static void atl_dev_close(struct rte_eth_dev *dev);
static int  atl_dev_reset(struct rte_eth_dev *dev);
static int  atl_dev_link_update(struct rte_eth_dev *dev, int wait);

static int atl_fw_version_get(struct rte_eth_dev *dev, char *fw_version,
			      size_t fw_size);

static void atl_dev_info_get(struct rte_eth_dev *dev,
			       struct rte_eth_dev_info *dev_info);

static const uint32_t *atl_dev_supported_ptypes_get(struct rte_eth_dev *dev);

static void atl_dev_link_status_print(struct rte_eth_dev *dev);

/* Interrupts */
static int atl_dev_rxq_interrupt_setup(struct rte_eth_dev *dev);
static int atl_dev_lsc_interrupt_setup(struct rte_eth_dev *dev, uint8_t on);
static int atl_dev_interrupt_get_status(struct rte_eth_dev *dev);
static int atl_dev_interrupt_action(struct rte_eth_dev *dev,
				    struct rte_intr_handle *handle);
static void atl_dev_interrupt_handler(void *param);

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
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC |
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
	.dev_set_link_up      = atl_dev_set_link_up,
	.dev_set_link_down    = atl_dev_set_link_down,
	.dev_close	      = atl_dev_close,
	.dev_reset	      = atl_dev_reset,

	/* Link */
	.link_update	      = atl_dev_link_update,

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

	.rx_queue_intr_enable = atl_dev_rx_queue_intr_enable,
	.rx_queue_intr_disable = atl_dev_rx_queue_intr_disable,
};

static inline int32_t
atl_reset_hw(struct aq_hw_s *hw)
{
	return hw_atl_b0_hw_reset(hw);
}

static inline void
atl_enable_intr(struct rte_eth_dev *dev)
{
	struct aq_hw_s *hw = ATL_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	hw_atl_itr_irq_msk_setlsw_set(hw, 0xffffffff);
}

static void
atl_disable_intr(struct aq_hw_s *hw)
{
	PMD_INIT_FUNC_TRACE();
	hw_atl_itr_irq_msk_clearlsw_set(hw, 0xffffffff);
}

static int
eth_atl_dev_init(struct rte_eth_dev *eth_dev)
{
	struct atl_adapter *adapter =
		(struct atl_adapter *)eth_dev->data->dev_private;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
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
	adapter->hw_cfg.link_speed_msk = AQ_NIC_RATE_10G |
			  AQ_NIC_RATE_5G |
			  AQ_NIC_RATE_2G5 |
			  AQ_NIC_RATE_1G |
			  AQ_NIC_RATE_100M;

	hw->aq_nic_cfg = &adapter->hw_cfg;

	/* disable interrupt */
	atl_disable_intr(hw);

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

	rte_intr_callback_register(intr_handle,
				   atl_dev_interrupt_handler, eth_dev);

	/* enable uio/vfio intr/eventfd mapping */
	rte_intr_enable(intr_handle);

	/* enable support intr */
	atl_enable_intr(eth_dev);

	return err;
}

static int
eth_atl_dev_uninit(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
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

	/* disable uio intr before callback unregister */
	rte_intr_disable(intr_handle);
	rte_intr_callback_unregister(intr_handle,
				     atl_dev_interrupt_handler, eth_dev);

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
atl_dev_configure(struct rte_eth_dev *dev)
{
	struct atl_interrupt *intr =
		ATL_DEV_PRIVATE_TO_INTR(dev->data->dev_private);

	PMD_INIT_FUNC_TRACE();

	/* set flag to update link status after init */
	intr->flags |= ATL_FLAG_NEED_LINK_UPDATE;

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
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	uint32_t intr_vector = 0;
	uint32_t *link_speeds;
	uint32_t speed = 0;
	int status;
	int err;

	PMD_INIT_FUNC_TRACE();

	/* set adapter started */
	hw->adapter_stopped = 0;

	if (dev->data->dev_conf.link_speeds & ETH_LINK_SPEED_FIXED) {
		PMD_INIT_LOG(ERR,
		"Invalid link_speeds for port %u, fix speed not supported",
				dev->data->port_id);
		return -EINVAL;
	}

	/* disable uio/vfio intr/eventfd mapping */
	rte_intr_disable(intr_handle);

	/* reinitialize adapter
	 * this calls reset and start
	 */
	status = atl_reset_hw(hw);
	if (status != 0)
		return -EIO;

	err = hw_atl_b0_hw_init(hw, dev->data->mac_addrs->addr_bytes);

	hw_atl_b0_hw_start(hw);
	/* check and configure queue intr-vector mapping */
	if ((rte_intr_cap_multiple(intr_handle) ||
	    !RTE_ETH_DEV_SRIOV(dev).active) &&
	    dev->data->dev_conf.intr_conf.rxq != 0) {
		intr_vector = dev->data->nb_rx_queues;
		if (intr_vector > ATL_MAX_INTR_QUEUE_NUM) {
			PMD_INIT_LOG(ERR, "At most %d intr queues supported",
					ATL_MAX_INTR_QUEUE_NUM);
			return -ENOTSUP;
		}
		if (rte_intr_efd_enable(intr_handle, intr_vector)) {
			PMD_INIT_LOG(ERR, "rte_intr_efd_enable failed");
			return -1;
		}
	}

	if (rte_intr_dp_is_en(intr_handle) && !intr_handle->intr_vec) {
		intr_handle->intr_vec = rte_zmalloc("intr_vec",
				    dev->data->nb_rx_queues * sizeof(int), 0);
		if (intr_handle->intr_vec == NULL) {
			PMD_INIT_LOG(ERR, "Failed to allocate %d rx_queues"
				     " intr_vec", dev->data->nb_rx_queues);
			return -ENOMEM;
		}
	}

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

	err = hw->aq_fw_ops->update_link_status(hw);

	if (err)
		goto error;

	dev->data->dev_link.link_status = hw->aq_link_status.mbps != 0;

	link_speeds = &dev->data->dev_conf.link_speeds;

	speed = 0x0;

	if (*link_speeds == ETH_LINK_SPEED_AUTONEG) {
		speed = hw->aq_nic_cfg->link_speed_msk;
	} else {
		if (*link_speeds & ETH_LINK_SPEED_10G)
			speed |= AQ_NIC_RATE_10G;
		if (*link_speeds & ETH_LINK_SPEED_5G)
			speed |= AQ_NIC_RATE_5G;
		if (*link_speeds & ETH_LINK_SPEED_1G)
			speed |= AQ_NIC_RATE_1G;
		if (*link_speeds & ETH_LINK_SPEED_2_5G)
			speed |=  AQ_NIC_RATE_2G5;
		if (*link_speeds & ETH_LINK_SPEED_100M)
			speed |= AQ_NIC_RATE_100M;
	}

	err = hw->aq_fw_ops->set_link_speed(hw, speed);
	if (err)
		goto error;

	if (rte_intr_allow_others(intr_handle)) {
		/* check if lsc interrupt is enabled */
		if (dev->data->dev_conf.intr_conf.lsc != 0)
			atl_dev_lsc_interrupt_setup(dev, true);
		else
			atl_dev_lsc_interrupt_setup(dev, false);
	} else {
		rte_intr_callback_unregister(intr_handle,
					     atl_dev_interrupt_handler, dev);
		if (dev->data->dev_conf.intr_conf.lsc != 0)
			PMD_INIT_LOG(INFO, "lsc won't enable because of"
				     " no intr multiplex");
	}

	/* check if rxq interrupt is enabled */
	if (dev->data->dev_conf.intr_conf.rxq != 0 &&
	    rte_intr_dp_is_en(intr_handle))
		atl_dev_rxq_interrupt_setup(dev);

	/* enable uio/vfio intr/eventfd mapping */
	rte_intr_enable(intr_handle);

	/* resume enabled intr since hw reset */
	atl_enable_intr(dev);

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
	struct rte_eth_link link;
	struct aq_hw_s *hw =
		ATL_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;

	PMD_INIT_FUNC_TRACE();

	/* disable interrupts */
	atl_disable_intr(hw);

	/* reset the NIC */
	atl_reset_hw(hw);
	hw->adapter_stopped = 1;

	atl_stop_queues(dev);

	/* Clear stored conf */
	dev->data->scattered_rx = 0;
	dev->data->lro = 0;

	/* Clear recorded link status */
	memset(&link, 0, sizeof(link));
	rte_eth_linkstatus_set(dev, &link);

	if (!rte_intr_allow_others(intr_handle))
		/* resume to the default handler */
		rte_intr_callback_register(intr_handle,
					   atl_dev_interrupt_handler,
					   (void *)dev);

	/* Clean datapath event and queue/vec mapping */
	rte_intr_efd_disable(intr_handle);
	if (intr_handle->intr_vec != NULL) {
		rte_free(intr_handle->intr_vec);
		intr_handle->intr_vec = NULL;
	}
}

/*
 * Set device link up: enable tx.
 */
static int
atl_dev_set_link_up(struct rte_eth_dev *dev)
{
	struct aq_hw_s *hw = ATL_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	return hw->aq_fw_ops->set_link_speed(hw,
			hw->aq_nic_cfg->link_speed_msk);
}

/*
 * Set device link down: disable tx.
 */
static int
atl_dev_set_link_down(struct rte_eth_dev *dev)
{
	struct aq_hw_s *hw = ATL_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	return hw->aq_fw_ops->set_link_speed(hw, 0);
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

	dev_info->speed_capa = ETH_LINK_SPEED_1G | ETH_LINK_SPEED_10G;
	dev_info->speed_capa |= ETH_LINK_SPEED_100M;
	dev_info->speed_capa |= ETH_LINK_SPEED_2_5G;
	dev_info->speed_capa |= ETH_LINK_SPEED_5G;
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

/* return 0 means link status changed, -1 means not changed */
static int
atl_dev_link_update(struct rte_eth_dev *dev, int wait __rte_unused)
{
	struct aq_hw_s *hw = ATL_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct atl_interrupt *intr =
		ATL_DEV_PRIVATE_TO_INTR(dev->data->dev_private);
	struct rte_eth_link link, old;
	int err = 0;

	link.link_status = ETH_LINK_DOWN;
	link.link_speed = 0;
	link.link_duplex = ETH_LINK_FULL_DUPLEX;
	link.link_autoneg = hw->is_autoneg ? ETH_LINK_AUTONEG : ETH_LINK_FIXED;
	memset(&old, 0, sizeof(old));

	/* load old link status */
	rte_eth_linkstatus_get(dev, &old);

	/* read current link status */
	err = hw->aq_fw_ops->update_link_status(hw);

	if (err)
		return 0;

	if (hw->aq_link_status.mbps == 0) {
		/* write default (down) link status */
		rte_eth_linkstatus_set(dev, &link);
		if (link.link_status == old.link_status)
			return -1;
		return 0;
	}

	intr->flags &= ~ATL_FLAG_NEED_LINK_CONFIG;

	link.link_status = ETH_LINK_UP;
	link.link_duplex = ETH_LINK_FULL_DUPLEX;
	link.link_speed = hw->aq_link_status.mbps;

	rte_eth_linkstatus_set(dev, &link);

	if (link.link_status == old.link_status)
		return -1;

	return 0;
}


/**
 * It clears the interrupt causes and enables the interrupt.
 * It will be called once only during nic initialized.
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 * @param on
 *  Enable or Disable.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */

static int
atl_dev_lsc_interrupt_setup(struct rte_eth_dev *dev, uint8_t on __rte_unused)
{
	atl_dev_link_status_print(dev);
	return 0;
}

static int
atl_dev_rxq_interrupt_setup(struct rte_eth_dev *dev __rte_unused)
{
	return 0;
}


static int
atl_dev_interrupt_get_status(struct rte_eth_dev *dev)
{
	struct atl_interrupt *intr =
		ATL_DEV_PRIVATE_TO_INTR(dev->data->dev_private);
	struct aq_hw_s *hw = ATL_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	u64 cause = 0;

	hw_atl_b0_hw_irq_read(hw, &cause);

	atl_disable_intr(hw);
	intr->flags = cause & BIT(ATL_IRQ_CAUSE_LINK) ?
			ATL_FLAG_NEED_LINK_UPDATE : 0;

	return 0;
}

/**
 * It gets and then prints the link status.
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
static void
atl_dev_link_status_print(struct rte_eth_dev *dev)
{
	struct rte_eth_link link;

	memset(&link, 0, sizeof(link));
	rte_eth_linkstatus_get(dev, &link);
	if (link.link_status) {
		PMD_DRV_LOG(INFO, "Port %d: Link Up - speed %u Mbps - %s",
					(int)(dev->data->port_id),
					(unsigned int)link.link_speed,
			link.link_duplex == ETH_LINK_FULL_DUPLEX ?
					"full-duplex" : "half-duplex");
	} else {
		PMD_DRV_LOG(INFO, " Port %d: Link Down",
				(int)(dev->data->port_id));
	}


#ifdef DEBUG
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);

	PMD_DRV_LOG(DEBUG, "PCI Address: " PCI_PRI_FMT,
				pci_dev->addr.domain,
				pci_dev->addr.bus,
				pci_dev->addr.devid,
				pci_dev->addr.function);
}
#endif

	PMD_DRV_LOG(INFO, "Link speed:%d", link.link_speed);
}

/*
 * It executes link_update after knowing an interrupt occurred.
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
static int
atl_dev_interrupt_action(struct rte_eth_dev *dev,
			   struct rte_intr_handle *intr_handle)
{
	struct atl_interrupt *intr =
		ATL_DEV_PRIVATE_TO_INTR(dev->data->dev_private);

	if (intr->flags & ATL_FLAG_NEED_LINK_UPDATE) {
		atl_dev_link_update(dev, 0);
		intr->flags &= ~ATL_FLAG_NEED_LINK_UPDATE;
		atl_dev_link_status_print(dev);
		_rte_eth_dev_callback_process(dev,
			RTE_ETH_EVENT_INTR_LSC, NULL);
	}

	atl_enable_intr(dev);
	rte_intr_enable(intr_handle);

	return 0;
}

/**
 * Interrupt handler triggered by NIC  for handling
 * specific interrupt.
 *
 * @param handle
 *  Pointer to interrupt handle.
 * @param param
 *  The address of parameter (struct rte_eth_dev *) regsitered before.
 *
 * @return
 *  void
 */
static void
atl_dev_interrupt_handler(void *param)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;

	atl_dev_interrupt_get_status(dev);
	atl_dev_interrupt_action(dev, dev->intr_handle);
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

