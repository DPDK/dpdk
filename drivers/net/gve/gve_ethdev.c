/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Intel Corporation
 */

#include "gve_ethdev.h"
#include "base/gve_adminq.h"
#include "base/gve_register.h"

const char gve_version_str[] = GVE_VERSION;
static const char gve_version_prefix[] = GVE_VERSION_PREFIX;

static void
gve_write_version(uint8_t *driver_version_register)
{
	const char *c = gve_version_prefix;

	while (*c) {
		writeb(*c, driver_version_register);
		c++;
	}

	c = gve_version_str;
	while (*c) {
		writeb(*c, driver_version_register);
		c++;
	}
	writeb('\n', driver_version_register);
}

static int
gve_dev_configure(__rte_unused struct rte_eth_dev *dev)
{
	return 0;
}

static int
gve_link_update(struct rte_eth_dev *dev, __rte_unused int wait_to_complete)
{
	struct gve_priv *priv = dev->data->dev_private;
	struct rte_eth_link link;
	int err;

	memset(&link, 0, sizeof(link));
	link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
	link.link_autoneg = RTE_ETH_LINK_AUTONEG;

	if (!dev->data->dev_started) {
		link.link_status = RTE_ETH_LINK_DOWN;
		link.link_speed = RTE_ETH_SPEED_NUM_NONE;
	} else {
		link.link_status = RTE_ETH_LINK_UP;
		PMD_DRV_LOG(DEBUG, "Get link status from hw");
		err = gve_adminq_report_link_speed(priv);
		if (err) {
			PMD_DRV_LOG(ERR, "Failed to get link speed.");
			priv->link_speed = RTE_ETH_SPEED_NUM_UNKNOWN;
		}
		link.link_speed = priv->link_speed;
	}

	return rte_eth_linkstatus_set(dev, &link);
}

static int
gve_dev_start(struct rte_eth_dev *dev)
{
	dev->data->dev_started = 1;
	gve_link_update(dev, 0);

	return 0;
}

static int
gve_dev_stop(struct rte_eth_dev *dev)
{
	dev->data->dev_link.link_status = RTE_ETH_LINK_DOWN;
	dev->data->dev_started = 0;

	return 0;
}

static int
gve_dev_close(struct rte_eth_dev *dev)
{
	int err = 0;

	if (dev->data->dev_started) {
		err = gve_dev_stop(dev);
		if (err != 0)
			PMD_DRV_LOG(ERR, "Failed to stop dev.");
	}

	dev->data->mac_addrs = NULL;

	return err;
}

static const struct eth_dev_ops gve_eth_dev_ops = {
	.dev_configure        = gve_dev_configure,
	.dev_start            = gve_dev_start,
	.dev_stop             = gve_dev_stop,
	.dev_close            = gve_dev_close,
	.link_update          = gve_link_update,
};

static void
gve_free_counter_array(struct gve_priv *priv)
{
	rte_memzone_free(priv->cnt_array_mz);
	priv->cnt_array = NULL;
}

static void
gve_free_irq_db(struct gve_priv *priv)
{
	rte_memzone_free(priv->irq_dbs_mz);
	priv->irq_dbs = NULL;
}

static void
gve_teardown_device_resources(struct gve_priv *priv)
{
	int err;

	/* Tell device its resources are being freed */
	if (gve_get_device_resources_ok(priv)) {
		err = gve_adminq_deconfigure_device_resources(priv);
		if (err)
			PMD_DRV_LOG(ERR, "Could not deconfigure device resources: err=%d", err);
	}
	gve_free_counter_array(priv);
	gve_free_irq_db(priv);
	gve_clear_device_resources_ok(priv);
}

static uint8_t
pci_dev_find_capability(struct rte_pci_device *pdev, int cap)
{
	uint8_t pos, id;
	uint16_t ent;
	int loops;
	int ret;

	ret = rte_pci_read_config(pdev, &pos, sizeof(pos), PCI_CAPABILITY_LIST);
	if (ret != sizeof(pos))
		return 0;

	loops = (PCI_CFG_SPACE_SIZE - PCI_STD_HEADER_SIZEOF) / PCI_CAP_SIZEOF;

	while (pos && loops--) {
		ret = rte_pci_read_config(pdev, &ent, sizeof(ent), pos);
		if (ret != sizeof(ent))
			return 0;

		id = ent & 0xff;
		if (id == 0xff)
			break;

		if (id == cap)
			return pos;

		pos = (ent >> 8);
	}

	return 0;
}

static int
pci_dev_msix_vec_count(struct rte_pci_device *pdev)
{
	uint8_t msix_cap = pci_dev_find_capability(pdev, PCI_CAP_ID_MSIX);
	uint16_t control;
	int ret;

	if (!msix_cap)
		return 0;

	ret = rte_pci_read_config(pdev, &control, sizeof(control), msix_cap + PCI_MSIX_FLAGS);
	if (ret != sizeof(control))
		return 0;

	return (control & PCI_MSIX_FLAGS_QSIZE) + 1;
}

static int
gve_setup_device_resources(struct gve_priv *priv)
{
	char z_name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *mz;
	int err = 0;

	snprintf(z_name, sizeof(z_name), "gve_%s_cnt_arr", priv->pci_dev->device.name);
	mz = rte_memzone_reserve_aligned(z_name,
					 priv->num_event_counters * sizeof(*priv->cnt_array),
					 rte_socket_id(), RTE_MEMZONE_IOVA_CONTIG,
					 PAGE_SIZE);
	if (mz == NULL) {
		PMD_DRV_LOG(ERR, "Could not alloc memzone for count array");
		return -ENOMEM;
	}
	priv->cnt_array = (rte_be32_t *)mz->addr;
	priv->cnt_array_mz = mz;

	snprintf(z_name, sizeof(z_name), "gve_%s_irqmz", priv->pci_dev->device.name);
	mz = rte_memzone_reserve_aligned(z_name,
					 sizeof(*priv->irq_dbs) * (priv->num_ntfy_blks),
					 rte_socket_id(), RTE_MEMZONE_IOVA_CONTIG,
					 PAGE_SIZE);
	if (mz == NULL) {
		PMD_DRV_LOG(ERR, "Could not alloc memzone for irq_dbs");
		err = -ENOMEM;
		goto free_cnt_array;
	}
	priv->irq_dbs = (struct gve_irq_db *)mz->addr;
	priv->irq_dbs_mz = mz;

	err = gve_adminq_configure_device_resources(priv,
						    priv->cnt_array_mz->iova,
						    priv->num_event_counters,
						    priv->irq_dbs_mz->iova,
						    priv->num_ntfy_blks);
	if (unlikely(err)) {
		PMD_DRV_LOG(ERR, "Could not config device resources: err=%d", err);
		goto free_irq_dbs;
	}
	return 0;

free_irq_dbs:
	gve_free_irq_db(priv);
free_cnt_array:
	gve_free_counter_array(priv);

	return err;
}

static int
gve_init_priv(struct gve_priv *priv, bool skip_describe_device)
{
	int num_ntfy;
	int err;

	/* Set up the adminq */
	err = gve_adminq_alloc(priv);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to alloc admin queue: err=%d", err);
		return err;
	}

	if (skip_describe_device)
		goto setup_device;

	/* Get the initial information we need from the device */
	err = gve_adminq_describe_device(priv);
	if (err) {
		PMD_DRV_LOG(ERR, "Could not get device information: err=%d", err);
		goto free_adminq;
	}

	num_ntfy = pci_dev_msix_vec_count(priv->pci_dev);
	if (num_ntfy <= 0) {
		PMD_DRV_LOG(ERR, "Could not count MSI-x vectors");
		err = -EIO;
		goto free_adminq;
	} else if (num_ntfy < GVE_MIN_MSIX) {
		PMD_DRV_LOG(ERR, "GVE needs at least %d MSI-x vectors, but only has %d",
			    GVE_MIN_MSIX, num_ntfy);
		err = -EINVAL;
		goto free_adminq;
	}

	priv->num_registered_pages = 0;

	/* gvnic has one Notification Block per MSI-x vector, except for the
	 * management vector
	 */
	priv->num_ntfy_blks = (num_ntfy - 1) & ~0x1;
	priv->mgmt_msix_idx = priv->num_ntfy_blks;

	priv->max_nb_txq = RTE_MIN(priv->max_nb_txq, priv->num_ntfy_blks / 2);
	priv->max_nb_rxq = RTE_MIN(priv->max_nb_rxq, priv->num_ntfy_blks / 2);

	if (priv->default_num_queues > 0) {
		priv->max_nb_txq = RTE_MIN(priv->default_num_queues, priv->max_nb_txq);
		priv->max_nb_rxq = RTE_MIN(priv->default_num_queues, priv->max_nb_rxq);
	}

	PMD_DRV_LOG(INFO, "Max TX queues %d, Max RX queues %d",
		    priv->max_nb_txq, priv->max_nb_rxq);

setup_device:
	err = gve_setup_device_resources(priv);
	if (!err)
		return 0;
free_adminq:
	gve_adminq_free(priv);
	return err;
}

static void
gve_teardown_priv_resources(struct gve_priv *priv)
{
	gve_teardown_device_resources(priv);
	gve_adminq_free(priv);
}

static int
gve_dev_init(struct rte_eth_dev *eth_dev)
{
	struct gve_priv *priv = eth_dev->data->dev_private;
	int max_tx_queues, max_rx_queues;
	struct rte_pci_device *pci_dev;
	struct gve_registers *reg_bar;
	rte_be32_t *db_bar;
	int err;

	eth_dev->dev_ops = &gve_eth_dev_ops;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	pci_dev = RTE_DEV_TO_PCI(eth_dev->device);

	reg_bar = pci_dev->mem_resource[GVE_REG_BAR].addr;
	if (!reg_bar) {
		PMD_DRV_LOG(ERR, "Failed to map pci bar!");
		return -ENOMEM;
	}

	db_bar = pci_dev->mem_resource[GVE_DB_BAR].addr;
	if (!db_bar) {
		PMD_DRV_LOG(ERR, "Failed to map doorbell bar!");
		return -ENOMEM;
	}

	gve_write_version(&reg_bar->driver_version);
	/* Get max queues to alloc etherdev */
	max_tx_queues = ioread32be(&reg_bar->max_tx_queues);
	max_rx_queues = ioread32be(&reg_bar->max_rx_queues);

	priv->reg_bar0 = reg_bar;
	priv->db_bar2 = db_bar;
	priv->pci_dev = pci_dev;
	priv->state_flags = 0x0;

	priv->max_nb_txq = max_tx_queues;
	priv->max_nb_rxq = max_rx_queues;

	err = gve_init_priv(priv, false);
	if (err)
		return err;

	eth_dev->data->mac_addrs = &priv->dev_addr;

	return 0;
}

static int
gve_dev_uninit(struct rte_eth_dev *eth_dev)
{
	struct gve_priv *priv = eth_dev->data->dev_private;

	gve_teardown_priv_resources(priv);

	eth_dev->data->mac_addrs = NULL;

	return 0;
}

static int
gve_pci_probe(__rte_unused struct rte_pci_driver *pci_drv,
	      struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev, sizeof(struct gve_priv), gve_dev_init);
}

static int
gve_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, gve_dev_uninit);
}

static const struct rte_pci_id pci_id_gve_map[] = {
	{ RTE_PCI_DEVICE(GOOGLE_VENDOR_ID, GVE_DEV_ID) },
	{ .device_id = 0 },
};

static struct rte_pci_driver rte_gve_pmd = {
	.id_table = pci_id_gve_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = gve_pci_probe,
	.remove = gve_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_gve, rte_gve_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_gve, pci_id_gve_map);
RTE_PMD_REGISTER_KMOD_DEP(net_gve, "* igb_uio | vfio-pci");
RTE_LOG_REGISTER_SUFFIX(gve_logtype_driver, driver, NOTICE);
