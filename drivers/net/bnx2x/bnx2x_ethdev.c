/*
 * Copyright (c) 2013-2015 Brocade Communications Systems, Inc.
 *
 * All rights reserved.
 */

#include "bnx2x.h"
#include "bnx2x_rxtx.h"

#include <rte_dev.h>

/*
 * The set of PCI devices this driver supports
 */
static struct rte_pci_id pci_id_bnx2x_map[] = {
#define RTE_PCI_DEV_ID_DECL_BNX2X(vend, dev) {RTE_PCI_DEVICE(vend, dev)},
#include "rte_pci_dev_ids.h"
	{ .vendor_id = 0, }
};

static struct rte_pci_id pci_id_bnx2xvf_map[] = {
#define RTE_PCI_DEV_ID_DECL_BNX2XVF(vend, dev) {RTE_PCI_DEVICE(vend, dev)},
#include "rte_pci_dev_ids.h"
	{ .vendor_id = 0, }
};

static void
bnx2x_link_update(struct rte_eth_dev *dev)
{
	struct bnx2x_softc *sc = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();
	bnx2x_link_status_update(sc);
	mb();
	dev->data->dev_link.link_speed = sc->link_vars.line_speed;
	switch (sc->link_vars.duplex) {
		case DUPLEX_FULL:
			dev->data->dev_link.link_duplex = ETH_LINK_FULL_DUPLEX;
			break;
		case DUPLEX_HALF:
			dev->data->dev_link.link_duplex = ETH_LINK_HALF_DUPLEX;
			break;
		default:
			dev->data->dev_link.link_duplex = ETH_LINK_AUTONEG_DUPLEX;
	}
	dev->data->dev_link.link_status = sc->link_vars.link_up;
}

static void
bnx2x_interrupt_action(struct rte_eth_dev *dev)
{
	struct bnx2x_softc *sc = dev->data->dev_private;
	uint32_t link_status;

	PMD_DRV_LOG(INFO, "Interrupt handled");

	if (bnx2x_intr_legacy(sc, 0))
		DELAY_MS(250);
	if (sc->periodic_flags & PERIODIC_GO)
		bnx2x_periodic_callout(sc);
	link_status = REG_RD(sc, sc->link_params.shmem_base +
			offsetof(struct shmem_region,
				port_mb[sc->link_params.port].link_status));
	if ((link_status & LINK_STATUS_LINK_UP) != dev->data->dev_link.link_status)
		bnx2x_link_update(dev);
}

static __rte_unused void
bnx2x_interrupt_handler(__rte_unused struct rte_intr_handle *handle, void *param)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;

	bnx2x_interrupt_action(dev);
	rte_intr_enable(&(dev->pci_dev->intr_handle));
}

/*
 * Devops - helper functions can be called from user application
 */

static int
bnx2x_dev_configure(struct rte_eth_dev *dev)
{
	struct bnx2x_softc *sc = dev->data->dev_private;
	int mp_ncpus = sysconf(_SC_NPROCESSORS_CONF);
	int ret;

	PMD_INIT_FUNC_TRACE();

	if (dev->data->dev_conf.rxmode.jumbo_frame)
		sc->mtu = dev->data->dev_conf.rxmode.max_rx_pkt_len;

	if (dev->data->nb_tx_queues > dev->data->nb_rx_queues) {
		PMD_DRV_LOG(ERR, "The number of TX queues is greater than number of RX queues");
		return -EINVAL;
	}

	sc->num_queues = MAX(dev->data->nb_rx_queues, dev->data->nb_tx_queues);
	if (sc->num_queues > mp_ncpus) {
		PMD_DRV_LOG(ERR, "The number of queues is more than number of CPUs");
		return -EINVAL;
	}

	PMD_DRV_LOG(DEBUG, "num_queues=%d, mtu=%d",
		       sc->num_queues, sc->mtu);

	/* allocate ilt */
	if (bnx2x_alloc_ilt_mem(sc) != 0) {
		PMD_DRV_LOG(ERR, "bnx2x_alloc_ilt_mem was failed");
		return -ENXIO;
	}

	/* allocate the host hardware/software hsi structures */
	if (bnx2x_alloc_hsi_mem(sc) != 0) {
		PMD_DRV_LOG(ERR, "bnx2x_alloc_hsi_mem was failed");
		bnx2x_free_ilt_mem(sc);
		return -ENXIO;
	}

	if (IS_VF(sc)) {
		if (bnx2x_dma_alloc(sc, sizeof(struct bnx2x_vf_mbx_msg),
				  &sc->vf2pf_mbox_mapping, "vf2pf_mbox",
				  RTE_CACHE_LINE_SIZE) != 0)
			return -ENOMEM;

		sc->vf2pf_mbox = (struct bnx2x_vf_mbx_msg *)sc->vf2pf_mbox_mapping.vaddr;
		if (bnx2x_dma_alloc(sc, sizeof(struct bnx2x_vf_bulletin),
				  &sc->pf2vf_bulletin_mapping, "vf2pf_bull",
				  RTE_CACHE_LINE_SIZE) != 0)
			return -ENOMEM;

		sc->pf2vf_bulletin = (struct bnx2x_vf_bulletin *)sc->pf2vf_bulletin_mapping.vaddr;

		ret = bnx2x_vf_get_resources(sc, sc->num_queues, sc->num_queues);
		if (ret)
			return ret;
	}

	return 0;
}

static int
bnx2x_dev_start(struct rte_eth_dev *dev)
{
	struct bnx2x_softc *sc = dev->data->dev_private;
	int ret = 0;

	PMD_INIT_FUNC_TRACE();

	ret = bnx2x_init(sc);
	if (ret) {
		PMD_DRV_LOG(DEBUG, "bnx2x_init failed (%d)", ret);
		return -1;
	}

	if (IS_PF(sc)) {
		rte_intr_callback_register(&(dev->pci_dev->intr_handle),
				bnx2x_interrupt_handler, (void *)dev);

		if(rte_intr_enable(&(dev->pci_dev->intr_handle)))
			PMD_DRV_LOG(ERR, "rte_intr_enable failed");
	}

	ret = bnx2x_dev_rx_init(dev);
	if (ret != 0) {
		PMD_DRV_LOG(DEBUG, "bnx2x_dev_rx_init returned error code");
		return -3;
	}

	/* Print important adapter info for the user. */
	bnx2x_print_adapter_info(sc);

	DELAY_MS(2500);

	return ret;
}

static void
bnx2x_dev_stop(struct rte_eth_dev *dev)
{
	struct bnx2x_softc *sc = dev->data->dev_private;
	int ret = 0;

	PMD_INIT_FUNC_TRACE();

	if (IS_PF(sc)) {
		rte_intr_disable(&(dev->pci_dev->intr_handle));
		rte_intr_callback_unregister(&(dev->pci_dev->intr_handle),
				bnx2x_interrupt_handler, (void *)dev);
	}

	ret = bnx2x_nic_unload(sc, UNLOAD_NORMAL, FALSE);
	if (ret) {
		PMD_DRV_LOG(DEBUG, "bnx2x_nic_unload failed (%d)", ret);
		return;
	}

	return;
}

static void
bnx2x_dev_close(struct rte_eth_dev *dev)
{
	struct bnx2x_softc *sc = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();

	if (IS_VF(sc))
		bnx2x_vf_close(sc);

	bnx2x_dev_clear_queues(dev);
	memset(&(dev->data->dev_link), 0 , sizeof(struct rte_eth_link));

	/* free the host hardware/software hsi structures */
	bnx2x_free_hsi_mem(sc);

	/* free ilt */
	bnx2x_free_ilt_mem(sc);
}

static void
bnx2x_promisc_enable(struct rte_eth_dev *dev)
{
	struct bnx2x_softc *sc = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();
	sc->rx_mode = BNX2X_RX_MODE_PROMISC;
	bnx2x_set_rx_mode(sc);
}

static void
bnx2x_promisc_disable(struct rte_eth_dev *dev)
{
	struct bnx2x_softc *sc = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();
	sc->rx_mode = BNX2X_RX_MODE_NORMAL;
	bnx2x_set_rx_mode(sc);
}

static void
bnx2x_dev_allmulticast_enable(struct rte_eth_dev *dev)
{
	struct bnx2x_softc *sc = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();
	sc->rx_mode = BNX2X_RX_MODE_ALLMULTI;
	bnx2x_set_rx_mode(sc);
}

static void
bnx2x_dev_allmulticast_disable(struct rte_eth_dev *dev)
{
	struct bnx2x_softc *sc = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();
	sc->rx_mode = BNX2X_RX_MODE_NORMAL;
	bnx2x_set_rx_mode(sc);
}

static int
bnx2x_dev_link_update(struct rte_eth_dev *dev, __rte_unused int wait_to_complete)
{
	PMD_INIT_FUNC_TRACE();

	int old_link_status = dev->data->dev_link.link_status;

	bnx2x_link_update(dev);

	return old_link_status == dev->data->dev_link.link_status ? -1 : 0;
}

static int
bnx2xvf_dev_link_update(struct rte_eth_dev *dev, __rte_unused int wait_to_complete)
{
	int old_link_status = dev->data->dev_link.link_status;
	struct bnx2x_softc *sc = dev->data->dev_private;

	bnx2x_link_update(dev);

	bnx2x_check_bull(sc);
	if (sc->old_bulletin.valid_bitmap & (1 << CHANNEL_DOWN)) {
		PMD_DRV_LOG(ERR, "PF indicated channel is down."
				"VF device is no longer operational");
		dev->data->dev_link.link_status = 0;
	}

	return old_link_status == dev->data->dev_link.link_status ? -1 : 0;
}

static void
bnx2x_dev_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	struct bnx2x_softc *sc = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();

	bnx2x_stats_handle(sc, STATS_EVENT_UPDATE);

	memset(stats, 0, sizeof (struct rte_eth_stats));

	stats->ipackets =
		HILO_U64(sc->eth_stats.total_unicast_packets_received_hi,
				sc->eth_stats.total_unicast_packets_received_lo) +
		HILO_U64(sc->eth_stats.total_multicast_packets_received_hi,
				sc->eth_stats.total_multicast_packets_received_lo) +
		HILO_U64(sc->eth_stats.total_broadcast_packets_received_hi,
				sc->eth_stats.total_broadcast_packets_received_lo);

	stats->opackets =
		HILO_U64(sc->eth_stats.total_unicast_packets_transmitted_hi,
				sc->eth_stats.total_unicast_packets_transmitted_lo) +
		HILO_U64(sc->eth_stats.total_multicast_packets_transmitted_hi,
				sc->eth_stats.total_multicast_packets_transmitted_lo) +
		HILO_U64(sc->eth_stats.total_broadcast_packets_transmitted_hi,
				sc->eth_stats.total_broadcast_packets_transmitted_lo);

	stats->ibytes =
		HILO_U64(sc->eth_stats.total_bytes_received_hi,
				sc->eth_stats.total_bytes_received_lo);

	stats->obytes =
		HILO_U64(sc->eth_stats.total_bytes_transmitted_hi,
				sc->eth_stats.total_bytes_transmitted_lo);

	stats->ierrors =
		HILO_U64(sc->eth_stats.error_bytes_received_hi,
				sc->eth_stats.error_bytes_received_lo);

	stats->oerrors = 0;

	stats->rx_nombuf =
		HILO_U64(sc->eth_stats.no_buff_discard_hi,
				sc->eth_stats.no_buff_discard_lo);
}

static void
bnx2x_dev_infos_get(struct rte_eth_dev *dev, __rte_unused struct rte_eth_dev_info *dev_info)
{
	struct bnx2x_softc *sc = dev->data->dev_private;
	dev_info->max_rx_queues  = sc->max_rx_queues;
	dev_info->max_tx_queues  = sc->max_tx_queues;
	dev_info->min_rx_bufsize = BNX2X_MIN_RX_BUF_SIZE;
	dev_info->max_rx_pktlen  = BNX2X_MAX_RX_PKT_LEN;
	dev_info->max_mac_addrs  = BNX2X_MAX_MAC_ADDRS;
}

static void
bnx2x_mac_addr_add(struct rte_eth_dev *dev, struct ether_addr *mac_addr,
		uint32_t index, uint32_t pool)
{
	struct bnx2x_softc *sc = dev->data->dev_private;

	if (sc->mac_ops.mac_addr_add)
		sc->mac_ops.mac_addr_add(dev, mac_addr, index, pool);
}

static void
bnx2x_mac_addr_remove(struct rte_eth_dev *dev, uint32_t index)
{
	struct bnx2x_softc *sc = dev->data->dev_private;

	if (sc->mac_ops.mac_addr_remove)
		sc->mac_ops.mac_addr_remove(dev, index);
}

static struct eth_dev_ops bnx2x_eth_dev_ops = {
	.dev_configure                = bnx2x_dev_configure,
	.dev_start                    = bnx2x_dev_start,
	.dev_stop                     = bnx2x_dev_stop,
	.dev_close                    = bnx2x_dev_close,
	.promiscuous_enable           = bnx2x_promisc_enable,
	.promiscuous_disable          = bnx2x_promisc_disable,
	.allmulticast_enable          = bnx2x_dev_allmulticast_enable,
	.allmulticast_disable         = bnx2x_dev_allmulticast_disable,
	.link_update                  = bnx2x_dev_link_update,
	.stats_get                    = bnx2x_dev_stats_get,
	.dev_infos_get                = bnx2x_dev_infos_get,
	.rx_queue_setup               = bnx2x_dev_rx_queue_setup,
	.rx_queue_release             = bnx2x_dev_rx_queue_release,
	.tx_queue_setup               = bnx2x_dev_tx_queue_setup,
	.tx_queue_release             = bnx2x_dev_tx_queue_release,
	.mac_addr_add                 = bnx2x_mac_addr_add,
	.mac_addr_remove              = bnx2x_mac_addr_remove,
};

/*
 * dev_ops for virtual function
 */
static struct eth_dev_ops bnx2xvf_eth_dev_ops = {
	.dev_configure                = bnx2x_dev_configure,
	.dev_start                    = bnx2x_dev_start,
	.dev_stop                     = bnx2x_dev_stop,
	.dev_close                    = bnx2x_dev_close,
	.promiscuous_enable           = bnx2x_promisc_enable,
	.promiscuous_disable          = bnx2x_promisc_disable,
	.allmulticast_enable          = bnx2x_dev_allmulticast_enable,
	.allmulticast_disable         = bnx2x_dev_allmulticast_disable,
	.link_update                  = bnx2xvf_dev_link_update,
	.stats_get                    = bnx2x_dev_stats_get,
	.dev_infos_get                = bnx2x_dev_infos_get,
	.rx_queue_setup               = bnx2x_dev_rx_queue_setup,
	.rx_queue_release             = bnx2x_dev_rx_queue_release,
	.tx_queue_setup               = bnx2x_dev_tx_queue_setup,
	.tx_queue_release             = bnx2x_dev_tx_queue_release,
	.mac_addr_add                 = bnx2x_mac_addr_add,
	.mac_addr_remove              = bnx2x_mac_addr_remove,
};


static int
bnx2x_common_dev_init(struct rte_eth_dev *eth_dev, int is_vf)
{
	int ret = 0;
	struct rte_pci_device *pci_dev;
	struct bnx2x_softc *sc;

	PMD_INIT_FUNC_TRACE();

	eth_dev->dev_ops = is_vf ? &bnx2xvf_eth_dev_ops : &bnx2x_eth_dev_ops;
	pci_dev = eth_dev->pci_dev;
	sc = eth_dev->data->dev_private;
	sc->pcie_bus    = pci_dev->addr.bus;
	sc->pcie_device = pci_dev->addr.devid;

	if (is_vf)
		sc->flags = BNX2X_IS_VF_FLAG;

	sc->devinfo.vendor_id    = pci_dev->id.vendor_id;
	sc->devinfo.device_id    = pci_dev->id.device_id;
	sc->devinfo.subvendor_id = pci_dev->id.subsystem_vendor_id;
	sc->devinfo.subdevice_id = pci_dev->id.subsystem_device_id;

	sc->pcie_func = pci_dev->addr.function;
	sc->bar[BAR0].base_addr = (void *)pci_dev->mem_resource[0].addr;
	if (is_vf)
		sc->bar[BAR1].base_addr = (void *)
			((uint64_t)pci_dev->mem_resource[0].addr + PXP_VF_ADDR_DB_START);
	else
		sc->bar[BAR1].base_addr = pci_dev->mem_resource[2].addr;

	assert(sc->bar[BAR0].base_addr);
	assert(sc->bar[BAR1].base_addr);

	bnx2x_load_firmware(sc);
	assert(sc->firmware);

	if (eth_dev->data->dev_conf.rx_adv_conf.rss_conf.rss_hf & ETH_RSS_NONFRAG_IPV4_UDP)
		sc->udp_rss = 1;

	sc->rx_budget = BNX2X_RX_BUDGET;
	sc->hc_rx_ticks = BNX2X_RX_TICKS;
	sc->hc_tx_ticks = BNX2X_TX_TICKS;

	sc->interrupt_mode = INTR_MODE_SINGLE_MSIX;
	sc->rx_mode = BNX2X_RX_MODE_NORMAL;

	sc->pci_dev = pci_dev;
	ret = bnx2x_attach(sc);
	if (ret) {
		PMD_DRV_LOG(ERR, "bnx2x_attach failed (%d)", ret);
	}

	eth_dev->data->mac_addrs = (struct ether_addr *)sc->link_params.mac_addr;

	PMD_DRV_LOG(INFO, "pcie_bus=%d, pcie_device=%d",
			sc->pcie_bus, sc->pcie_device);
	PMD_DRV_LOG(INFO, "bar0.addr=%p, bar1.addr=%p",
			sc->bar[BAR0].base_addr, sc->bar[BAR1].base_addr);
	PMD_DRV_LOG(INFO, "port=%d, path=%d, vnic=%d, func=%d",
			PORT_ID(sc), PATH_ID(sc), VNIC_ID(sc), FUNC_ID(sc));
	PMD_DRV_LOG(INFO, "portID=%d vendorID=0x%x deviceID=0x%x",
			eth_dev->data->port_id, pci_dev->id.vendor_id, pci_dev->id.device_id);

	return ret;
}

static int
eth_bnx2x_dev_init(struct rte_eth_dev *eth_dev)
{
	PMD_INIT_FUNC_TRACE();
	return bnx2x_common_dev_init(eth_dev, 0);
}

static int
eth_bnx2xvf_dev_init(struct rte_eth_dev *eth_dev)
{
	PMD_INIT_FUNC_TRACE();
	return bnx2x_common_dev_init(eth_dev, 1);
}

static struct eth_driver rte_bnx2x_pmd = {
	.pci_drv = {
		.name = "rte_bnx2x_pmd",
		.id_table = pci_id_bnx2x_map,
		.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
	},
	.eth_dev_init = eth_bnx2x_dev_init,
	.dev_private_size = sizeof(struct bnx2x_softc),
};

/*
 * virtual function driver struct
 */
static struct eth_driver rte_bnx2xvf_pmd = {
	.pci_drv = {
		.name = "rte_bnx2xvf_pmd",
		.id_table = pci_id_bnx2xvf_map,
		.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	},
	.eth_dev_init = eth_bnx2xvf_dev_init,
	.dev_private_size = sizeof(struct bnx2x_softc),
};

static int rte_bnx2x_pmd_init(const char *name __rte_unused, const char *params __rte_unused)
{
	PMD_INIT_FUNC_TRACE();
	rte_eth_driver_register(&rte_bnx2x_pmd);

	return 0;
}

static int rte_bnx2xvf_pmd_init(const char *name __rte_unused, const char *params __rte_unused)
{
	PMD_INIT_FUNC_TRACE();
	rte_eth_driver_register(&rte_bnx2xvf_pmd);

	return 0;
}

static struct rte_driver rte_bnx2x_driver = {
	.type = PMD_PDEV,
	.init = rte_bnx2x_pmd_init,
};

static struct rte_driver rte_bnx2xvf_driver = {
	.type = PMD_PDEV,
	.init = rte_bnx2xvf_pmd_init,
};

PMD_REGISTER_DRIVER(rte_bnx2x_driver);
PMD_REGISTER_DRIVER(rte_bnx2xvf_driver);
