/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Yunsilicon Technology Co., Ltd.
 */

#include <ethdev_pci.h>

#include "xsc_log.h"
#include "xsc_defs.h"
#include "xsc_ethdev.h"

static int
xsc_ethdev_rss_hash_conf_get(struct rte_eth_dev *dev,
			     struct rte_eth_rss_conf *rss_conf)
{
	struct xsc_ethdev_priv *priv = TO_XSC_ETHDEV_PRIV(dev);

	if (rss_conf->rss_key != NULL && rss_conf->rss_key_len >= priv->rss_conf.rss_key_len)
		memcpy(rss_conf->rss_key, priv->rss_conf.rss_key, priv->rss_conf.rss_key_len);

	rss_conf->rss_key_len = priv->rss_conf.rss_key_len;
	rss_conf->rss_hf = priv->rss_conf.rss_hf;
	return 0;
}

static int
xsc_ethdev_rss_hash_update(struct rte_eth_dev *dev,
			   struct rte_eth_rss_conf *rss_conf)
{
	struct xsc_ethdev_priv *priv = TO_XSC_ETHDEV_PRIV(dev);
	int ret = 0;

	ret = xsc_dev_rss_key_modify(priv->xdev, rss_conf->rss_key, rss_conf->rss_key_len);
	if (ret == 0) {
		memcpy(priv->rss_conf.rss_key, rss_conf->rss_key,
		       priv->rss_conf.rss_key_len);
		priv->rss_conf.rss_key_len = rss_conf->rss_key_len;
		priv->rss_conf.rss_hf = rss_conf->rss_hf;
	}

	return ret;
}

static int
xsc_ethdev_configure(struct rte_eth_dev *dev)
{
	struct xsc_ethdev_priv *priv = TO_XSC_ETHDEV_PRIV(dev);
	int ret;
	struct rte_eth_rss_conf *rss_conf;

	priv->num_sq = dev->data->nb_tx_queues;
	priv->num_rq = dev->data->nb_rx_queues;

	if (dev->data->dev_conf.rxmode.mq_mode & RTE_ETH_MQ_RX_RSS_FLAG)
		dev->data->dev_conf.rxmode.offloads |= RTE_ETH_RX_OFFLOAD_RSS_HASH;

	if (priv->rss_conf.rss_key == NULL) {
		priv->rss_conf.rss_key = rte_zmalloc(NULL, XSC_RSS_HASH_KEY_LEN,
						     RTE_CACHE_LINE_SIZE);
		if (priv->rss_conf.rss_key == NULL) {
			PMD_DRV_LOG(ERR, "Failed to alloc rss key");
			rte_errno = ENOMEM;
			ret = -rte_errno;
			goto error;
		}
		priv->rss_conf.rss_key_len = XSC_RSS_HASH_KEY_LEN;
	}

	if (dev->data->dev_conf.rx_adv_conf.rss_conf.rss_key != NULL) {
		rss_conf = &dev->data->dev_conf.rx_adv_conf.rss_conf;
		ret = xsc_ethdev_rss_hash_update(dev, rss_conf);
		if (ret != 0) {
			PMD_DRV_LOG(ERR, "Xsc pmd set rss key error!");
			rte_errno = -ENOEXEC;
			goto error;
		}
	}

	priv->txqs = (void *)dev->data->tx_queues;
	priv->rxqs = (void *)dev->data->rx_queues;
	return 0;

error:
	return -rte_errno;
}

static int
xsc_ethdev_mac_addr_add(struct rte_eth_dev *dev, struct rte_ether_addr *mac, uint32_t index)
{
	int i;

	rte_errno = EINVAL;
	if (index > XSC_MAX_MAC_ADDRESSES)
		return -rte_errno;

	if (rte_is_zero_ether_addr(mac))
		return -rte_errno;

	for (i = 0; i != XSC_MAX_MAC_ADDRESSES; ++i) {
		if (i == (int)index)
			continue;
		if (memcmp(&dev->data->mac_addrs[i], mac, sizeof(*mac)))
			continue;
		/* Address already configured elsewhere, return with error */
		rte_errno = EADDRINUSE;
		return -rte_errno;
	}

	dev->data->mac_addrs[index] = *mac;
	return 0;
}

const struct eth_dev_ops xsc_eth_dev_ops = {
	.dev_configure = xsc_ethdev_configure,
	.rss_hash_update = xsc_ethdev_rss_hash_update,
	.rss_hash_conf_get = xsc_ethdev_rss_hash_conf_get,
};

static int
xsc_ethdev_init_one_representor(struct rte_eth_dev *eth_dev, void *init_params)
{
	int ret;
	struct xsc_repr_port *repr_port = (struct xsc_repr_port *)init_params;
	struct xsc_ethdev_priv *priv = TO_XSC_ETHDEV_PRIV(eth_dev);
	struct xsc_dev_config *config = &priv->config;
	struct rte_ether_addr mac;

	priv->repr_port = repr_port;
	repr_port->drv_data = eth_dev;
	priv->xdev = repr_port->xdev;
	priv->mtu = RTE_ETHER_MTU;
	priv->funcid_type = (repr_port->info.funcid & XSC_FUNCID_TYPE_MASK) >> 14;
	priv->funcid = repr_port->info.funcid & XSC_FUNCID_MASK;
	if (repr_port->info.port_type == XSC_PORT_TYPE_UPLINK ||
	    repr_port->info.port_type == XSC_PORT_TYPE_UPLINK_BOND)
		priv->eth_type = RTE_ETH_REPRESENTOR_PF;
	else
		priv->eth_type = RTE_ETH_REPRESENTOR_VF;
	priv->representor_id = repr_port->info.repr_id;
	priv->dev_data = eth_dev->data;
	priv->ifindex = repr_port->info.ifindex;

	eth_dev->data->dev_flags |= RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS;
	eth_dev->data->mac_addrs = priv->mac;
	if (rte_is_zero_ether_addr(eth_dev->data->mac_addrs)) {
		ret = xsc_dev_get_mac(priv->xdev, mac.addr_bytes);
		if (ret != 0) {
			PMD_DRV_LOG(ERR, "Port %u cannot get MAC address",
				    eth_dev->data->port_id);
			return -ENODEV;
		}
	}

	xsc_ethdev_mac_addr_add(eth_dev, &mac, 0);

	config->hw_csum = 1;
	config->pph_flag =  priv->xdev->devargs.pph_mode;
	if ((config->pph_flag & XSC_TX_PPH) != 0) {
		config->tso = 0;
	} else {
		config->tso = 1;
		if (config->tso)
			config->tso_max_payload_sz = 1500;
	}

	priv->is_representor = (priv->eth_type == RTE_ETH_REPRESENTOR_NONE) ? 0 : 1;
	if (priv->is_representor) {
		eth_dev->data->dev_flags |= RTE_ETH_DEV_REPRESENTOR;
		eth_dev->data->representor_id = priv->representor_id;
		eth_dev->data->backer_port_id = eth_dev->data->port_id;
	}

	eth_dev->dev_ops = &xsc_eth_dev_ops;
	eth_dev->rx_pkt_burst = rte_eth_pkt_burst_dummy;
	eth_dev->tx_pkt_burst = rte_eth_pkt_burst_dummy;

	rte_eth_dev_probing_finish(eth_dev);

	return 0;
}

static int
xsc_ethdev_init_representors(struct rte_eth_dev *eth_dev)
{
	struct xsc_ethdev_priv *priv = TO_XSC_ETHDEV_PRIV(eth_dev);
	struct rte_eth_devargs eth_da = { .nb_representor_ports = 0 };
	struct rte_device *dev;
	struct xsc_dev *xdev;
	struct xsc_repr_port *repr_port;
	char name[RTE_ETH_NAME_MAX_LEN];
	int i;
	int ret;

	PMD_INIT_FUNC_TRACE();

	dev = &priv->pci_dev->device;
	if (dev->devargs != NULL) {
		ret = rte_eth_devargs_parse(dev->devargs->args, &eth_da, 1);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "Failed to parse device arguments: %s",
				    dev->devargs->args);
			return -EINVAL;
		}
	}

	xdev = priv->xdev;
	ret = xsc_dev_repr_ports_probe(xdev, eth_da.nb_representor_ports, RTE_MAX_ETHPORTS);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Failed to probe %d xsc device representors",
			    eth_da.nb_representor_ports);
		return ret;
	}

	/* PF rep init */
	repr_port = &xdev->repr_ports[xdev->num_repr_ports - 1];
	ret = xsc_ethdev_init_one_representor(eth_dev, repr_port);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Failed to init backing representor");
		return ret;
	}

	/* VF rep init */
	for (i = 0; i < eth_da.nb_representor_ports; i++) {
		repr_port = &xdev->repr_ports[i];
		snprintf(name, sizeof(name), "%s_rep_%d",
			 xdev->name, repr_port->info.repr_id);
		ret = rte_eth_dev_create(dev,
					 name,
					 sizeof(struct xsc_ethdev_priv),
					 NULL, NULL,
					 xsc_ethdev_init_one_representor,
					 repr_port);
		if (ret != 0) {
			PMD_DRV_LOG(ERR, "Failed to create representor: %d", i);
			goto destroy_reprs;
		}
	}

	return 0;

destroy_reprs:
	/* Destroy vf reprs */
	while ((i--) > 1) {
		repr_port = &xdev->repr_ports[i];
		rte_eth_dev_destroy((struct rte_eth_dev *)repr_port->drv_data, NULL);
	}

	/* Destroy pf repr */
	repr_port = &xdev->repr_ports[xdev->num_repr_ports - 1];
	rte_eth_dev_destroy((struct rte_eth_dev *)repr_port->drv_data, NULL);
	return ret;
}

static int
xsc_ethdev_init(struct rte_eth_dev *eth_dev)
{
	struct xsc_ethdev_priv *priv = TO_XSC_ETHDEV_PRIV(eth_dev);
	int ret;

	PMD_INIT_FUNC_TRACE();

	priv->eth_dev = eth_dev;
	priv->pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);

	ret = xsc_dev_init(priv->pci_dev, &priv->xdev);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to initialize xsc device");
		return ret;
	}
	priv->xdev->port_id = eth_dev->data->port_id;

	ret = xsc_ethdev_init_representors(eth_dev);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Failed to initialize representors");
		goto uninit_xsc_dev;
	}

	return 0;

uninit_xsc_dev:
	xsc_dev_uninit(priv->xdev);
	return ret;
}

static int
xsc_ethdev_uninit(struct rte_eth_dev *eth_dev)
{
	struct xsc_ethdev_priv *priv = TO_XSC_ETHDEV_PRIV(eth_dev);

	PMD_INIT_FUNC_TRACE();

	xsc_dev_uninit(priv->xdev);

	return 0;
}

static int
xsc_ethdev_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
		     struct rte_pci_device *pci_dev)
{
	int ret;

	PMD_INIT_FUNC_TRACE();

	ret = rte_eth_dev_pci_generic_probe(pci_dev,
					    sizeof(struct xsc_ethdev_priv),
					    xsc_ethdev_init);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to probe ethdev: %s", pci_dev->name);
		return ret;
	}

	return 0;
}

static int
xsc_ethdev_pci_remove(struct rte_pci_device *pci_dev)
{
	int ret;

	PMD_INIT_FUNC_TRACE();

	ret = rte_eth_dev_pci_generic_remove(pci_dev, xsc_ethdev_uninit);
	if (ret) {
		PMD_DRV_LOG(ERR, "Could not remove ethdev: %s", pci_dev->name);
		return ret;
	}

	return 0;
}

static const struct rte_pci_id xsc_ethdev_pci_id_map[] = {
	{ RTE_PCI_DEVICE(XSC_PCI_VENDOR_ID, XSC_PCI_DEV_ID_MS) },
	{ RTE_PCI_DEVICE(XSC_PCI_VENDOR_ID, XSC_PCI_DEV_ID_MSVF) },
	{ RTE_PCI_DEVICE(XSC_PCI_VENDOR_ID, XSC_PCI_DEV_ID_MVH) },
	{ RTE_PCI_DEVICE(XSC_PCI_VENDOR_ID, XSC_PCI_DEV_ID_MVHVF) },
	{ RTE_PCI_DEVICE(XSC_PCI_VENDOR_ID, XSC_PCI_DEV_ID_MVS) },
	{ RTE_PCI_DEVICE(0, 0) },
};

static struct rte_pci_driver xsc_ethdev_pci_driver = {
	.id_table  = xsc_ethdev_pci_id_map,
	.probe = xsc_ethdev_pci_probe,
	.remove = xsc_ethdev_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_xsc, xsc_ethdev_pci_driver);
RTE_PMD_REGISTER_PCI_TABLE(net_xsc, xsc_ethdev_pci_id_map);
RTE_PMD_REGISTER_PARAM_STRING(net_xsc,
			      XSC_PPH_MODE_ARG "=<x>"
			      XSC_NIC_MODE_ARG "=<x>"
			      XSC_FLOW_MODE_ARG "=<x>");

RTE_LOG_REGISTER_SUFFIX(xsc_logtype_init, init, NOTICE);
RTE_LOG_REGISTER_SUFFIX(xsc_logtype_driver, driver, NOTICE);
