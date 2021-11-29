/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <rte_string_fns.h>
#include <rte_version.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_bus_pci.h>
#ifdef RTE_NET_IXGBE
#include <rte_pmd_ixgbe.h>
#endif
#include "rte_ethtool.h"

#define PKTPOOL_SIZE 512
#define PKTPOOL_CACHE 32


int
rte_ethtool_get_drvinfo(uint16_t port_id, struct ethtool_drvinfo *drvinfo)
{
	struct rte_eth_dev_info dev_info;
	struct rte_dev_reg_info reg_info;
	const struct rte_pci_device *pci_dev;
	const struct rte_bus *bus = NULL;
	int n;
	int ret;

	if (drvinfo == NULL)
		return -EINVAL;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	ret = rte_eth_dev_fw_version_get(port_id, drvinfo->fw_version,
			      sizeof(drvinfo->fw_version));
	if (ret < 0)
		printf("firmware version get error: (%s)\n", strerror(-ret));
	else if (ret > 0)
		printf("Insufficient fw version buffer size, "
		       "the minimum size should be %d\n", ret);

	ret = rte_eth_dev_info_get(port_id, &dev_info);
	if (ret != 0) {
		printf("Error during getting device (port %u) info: %s\n",
		       port_id, strerror(-ret));

		return ret;
	}

	strlcpy(drvinfo->driver, dev_info.driver_name,
		sizeof(drvinfo->driver));
	strlcpy(drvinfo->version, rte_version(), sizeof(drvinfo->version));
	/* TODO: replace bus_info by rte_devargs.name */
	if (dev_info.device)
		bus = rte_bus_find_by_device(dev_info.device);
	if (bus && !strcmp(bus->name, "pci")) {
		pci_dev = RTE_DEV_TO_PCI(dev_info.device);
		snprintf(drvinfo->bus_info, sizeof(drvinfo->bus_info),
			"%04x:%02x:%02x.%x",
			pci_dev->addr.domain, pci_dev->addr.bus,
			pci_dev->addr.devid, pci_dev->addr.function);
	} else {
		snprintf(drvinfo->bus_info, sizeof(drvinfo->bus_info), "N/A");
	}

	memset(&reg_info, 0, sizeof(reg_info));
	rte_eth_dev_get_reg_info(port_id, &reg_info);
	n = reg_info.length;
	if (n > 0)
		drvinfo->regdump_len = n;
	else
		drvinfo->regdump_len = 0;

	n = rte_eth_dev_get_eeprom_length(port_id);
	if (n > 0)
		drvinfo->eedump_len = n;
	else
		drvinfo->eedump_len = 0;

	drvinfo->n_stats = sizeof(struct rte_eth_stats) / sizeof(uint64_t);
	drvinfo->testinfo_len = 0;

	return 0;
}

int
rte_ethtool_get_regs_len(uint16_t port_id)
{
	struct rte_dev_reg_info reg_info;
	int ret;

	memset(&reg_info, 0, sizeof(reg_info));

	ret = rte_eth_dev_get_reg_info(port_id, &reg_info);
	if (ret)
		return ret;

	return reg_info.length * reg_info.width;
}

int
rte_ethtool_get_regs(uint16_t port_id, struct ethtool_regs *regs, void *data)
{
	struct rte_dev_reg_info reg_info;
	int status;

	if (regs == NULL || data == NULL)
		return -EINVAL;

	reg_info.data = data;
	reg_info.length = 0;

	status = rte_eth_dev_get_reg_info(port_id, &reg_info);
	if (status)
		return status;
	regs->version = reg_info.version;

	return 0;
}

int
rte_ethtool_get_link(uint16_t port_id)
{
	struct rte_eth_link link;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	ret = rte_eth_link_get(port_id, &link);
	if (ret < 0)
		return ret;

	return link.link_status;
}

int
rte_ethtool_get_eeprom_len(uint16_t port_id)
{
	return rte_eth_dev_get_eeprom_length(port_id);
}

int
rte_ethtool_get_eeprom(uint16_t port_id, struct ethtool_eeprom *eeprom,
	void *words)
{
	struct rte_dev_eeprom_info eeprom_info;
	int status;

	if (eeprom == NULL || words == NULL)
		return -EINVAL;

	eeprom_info.offset = eeprom->offset;
	eeprom_info.length = eeprom->len;
	eeprom_info.data = words;

	status = rte_eth_dev_get_eeprom(port_id, &eeprom_info);
	if (status)
		return status;

	eeprom->magic = eeprom_info.magic;

	return 0;
}

int
rte_ethtool_set_eeprom(uint16_t port_id, struct ethtool_eeprom *eeprom,
	void *words)
{
	struct rte_dev_eeprom_info eeprom_info;
	int status;

	if (eeprom == NULL || words == NULL || eeprom->offset >= eeprom->len)
		return -EINVAL;

	eeprom_info.offset = eeprom->offset;
	eeprom_info.length = eeprom->len;
	eeprom_info.data = words;

	status = rte_eth_dev_set_eeprom(port_id, &eeprom_info);
	if (status)
		return status;

	eeprom->magic = eeprom_info.magic;

	return 0;
}

int
rte_ethtool_get_module_info(uint16_t port_id, uint32_t *modinfo)
{
	struct rte_eth_dev_module_info *info;

	info = (struct rte_eth_dev_module_info *)modinfo;
	return rte_eth_dev_get_module_info(port_id, info);
}

int
rte_ethtool_get_module_eeprom(uint16_t port_id, struct ethtool_eeprom *eeprom,
	void *words)
{
	struct rte_dev_eeprom_info eeprom_info;
	int status;

	if (eeprom == NULL || words == NULL)
		return -EINVAL;

	eeprom_info.offset = eeprom->offset;
	eeprom_info.length = eeprom->len;
	eeprom_info.data = words;

	status = rte_eth_dev_get_module_eeprom(port_id, &eeprom_info);
	if (status)
		return status;

	return 0;
}

int
rte_ethtool_get_pauseparam(uint16_t port_id,
	struct ethtool_pauseparam *pause_param)
{
	struct rte_eth_fc_conf fc_conf;
	int status;

	if (pause_param == NULL)
		return -EINVAL;

	status = rte_eth_dev_flow_ctrl_get(port_id, &fc_conf);
	if (status)
		return status;

	pause_param->tx_pause = 0;
	pause_param->rx_pause = 0;
	switch (fc_conf.mode) {
	case RTE_ETH_FC_RX_PAUSE:
		pause_param->rx_pause = 1;
		break;
	case RTE_ETH_FC_TX_PAUSE:
		pause_param->tx_pause = 1;
		break;
	case RTE_ETH_FC_FULL:
		pause_param->rx_pause = 1;
		pause_param->tx_pause = 1;
	default:
		/* dummy block to avoid compiler warning */
		break;
	}
	pause_param->autoneg = (uint32_t)fc_conf.autoneg;

	return 0;
}

int
rte_ethtool_set_pauseparam(uint16_t port_id,
	struct ethtool_pauseparam *pause_param)
{
	struct rte_eth_fc_conf fc_conf;
	int status;

	if (pause_param == NULL)
		return -EINVAL;

	/*
	 * Read device flow control parameter first since
	 * ethtool set_pauseparam op doesn't have all the information.
	 * as defined in struct rte_eth_fc_conf.
	 * This API requires the device to support both
	 * rte_eth_dev_flow_ctrl_get and rte_eth_dev_flow_ctrl_set, otherwise
	 * return -ENOTSUP
	 */
	status = rte_eth_dev_flow_ctrl_get(port_id, &fc_conf);
	if (status)
		return status;

	fc_conf.autoneg = (uint8_t)pause_param->autoneg;

	if (pause_param->tx_pause) {
		if (pause_param->rx_pause)
			fc_conf.mode = RTE_ETH_FC_FULL;
		else
			fc_conf.mode = RTE_ETH_FC_TX_PAUSE;
	} else {
		if (pause_param->rx_pause)
			fc_conf.mode = RTE_ETH_FC_RX_PAUSE;
		else
			fc_conf.mode = RTE_ETH_FC_NONE;
	}

	status = rte_eth_dev_flow_ctrl_set(port_id, &fc_conf);
	if (status)
		return status;

	return 0;
}

int
rte_ethtool_net_open(uint16_t port_id)
{
	int ret;

	ret = rte_eth_dev_stop(port_id);
	if (ret != 0)
		return ret;

	return rte_eth_dev_start(port_id);
}

int
rte_ethtool_net_stop(uint16_t port_id)
{
	return rte_eth_dev_stop(port_id);
}

int
rte_ethtool_net_get_mac_addr(uint16_t port_id, struct rte_ether_addr *addr)
{
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	if (addr == NULL)
		return -EINVAL;

	ret = rte_eth_macaddr_get(port_id, addr);
	if (ret != 0)
		return ret;

	return 0;
}

int
rte_ethtool_net_set_mac_addr(uint16_t port_id, struct rte_ether_addr *addr)
{
	if (addr == NULL)
		return -EINVAL;
	return rte_eth_dev_default_mac_addr_set(port_id, addr);
}

int
rte_ethtool_net_validate_addr(uint16_t port_id __rte_unused,
	struct rte_ether_addr *addr)
{
	if (addr == NULL)
		return -EINVAL;
	return rte_is_valid_assigned_ether_addr(addr);
}

int
rte_ethtool_net_change_mtu(uint16_t port_id, int mtu)
{
	if (mtu < 0 || mtu > UINT16_MAX)
		return -EINVAL;
	return rte_eth_dev_set_mtu(port_id, (uint16_t)mtu);
}

int
rte_ethtool_net_get_stats64(uint16_t port_id, struct rte_eth_stats *stats)
{
	if (stats == NULL)
		return -EINVAL;
	return rte_eth_stats_get(port_id, stats);
}

int
rte_ethtool_net_vlan_rx_add_vid(uint16_t port_id, uint16_t vid)
{
	return rte_eth_dev_vlan_filter(port_id, vid, 1);
}

int
rte_ethtool_net_vlan_rx_kill_vid(uint16_t port_id, uint16_t vid)
{
	return rte_eth_dev_vlan_filter(port_id, vid, 0);
}

/*
 * The set_rx_mode provides driver-specific rx mode setting.
 * This implementation implements rx mode setting based upon
 * ixgbe/igb drivers. Further improvement is to provide a
 * callback op field over struct rte_eth_dev::dev_ops so each
 * driver can register device-specific implementation
 */
int
rte_ethtool_net_set_rx_mode(uint16_t port_id)
{
	uint16_t num_vfs;
	struct rte_eth_dev_info dev_info;
	uint16_t vf;
	int ret;

	ret = rte_eth_dev_info_get(port_id, &dev_info);
	if (ret != 0)
		return ret;

	num_vfs = dev_info.max_vfs;

	/* Set VF vf_rx_mode, VF unsupport status is discard */
	for (vf = 0; vf < num_vfs; vf++) {
#ifdef RTE_NET_IXGBE
		rte_pmd_ixgbe_set_vf_rxmode(port_id, vf,
			RTE_ETH_VMDQ_ACCEPT_UNTAG, 0);
#endif
	}

	/* Enable Rx vlan filter, VF unsupported status is discard */
	ret = rte_eth_dev_set_vlan_offload(port_id, RTE_ETH_VLAN_FILTER_MASK);
	if (ret != 0)
		return ret;

	return 0;
}


int
rte_ethtool_get_ringparam(uint16_t port_id,
	struct ethtool_ringparam *ring_param)
{
	struct rte_eth_dev_info dev_info;
	struct rte_eth_rxq_info rx_qinfo;
	struct rte_eth_txq_info tx_qinfo;
	int stat;
	int ret;

	if (ring_param == NULL)
		return -EINVAL;

	ret = rte_eth_dev_info_get(port_id, &dev_info);
	if (ret != 0)
		return ret;

	stat = rte_eth_rx_queue_info_get(port_id, 0, &rx_qinfo);
	if (stat != 0)
		return stat;

	stat = rte_eth_tx_queue_info_get(port_id, 0, &tx_qinfo);
	if (stat != 0)
		return stat;

	memset(ring_param, 0, sizeof(*ring_param));
	ring_param->rx_pending = rx_qinfo.nb_desc;
	ring_param->rx_max_pending = dev_info.rx_desc_lim.nb_max;
	ring_param->tx_pending = tx_qinfo.nb_desc;
	ring_param->tx_max_pending = dev_info.tx_desc_lim.nb_max;

	return 0;
}


int
rte_ethtool_set_ringparam(uint16_t port_id,
	struct ethtool_ringparam *ring_param)
{
	struct rte_eth_rxq_info rx_qinfo;
	int stat;

	if (ring_param == NULL)
		return -EINVAL;

	stat = rte_eth_rx_queue_info_get(port_id, 0, &rx_qinfo);
	if (stat != 0)
		return stat;

	stat = rte_eth_dev_stop(port_id);
	if (stat != 0)
		return stat;

	stat = rte_eth_tx_queue_setup(port_id, 0, ring_param->tx_pending,
		rte_eth_dev_socket_id(port_id), NULL);
	if (stat != 0)
		return stat;

	stat = rte_eth_rx_queue_setup(port_id, 0, ring_param->rx_pending,
		rte_eth_dev_socket_id(port_id), NULL, rx_qinfo.mp);
	if (stat != 0)
		return stat;

	return rte_eth_dev_start(port_id);
}
